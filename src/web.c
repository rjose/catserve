#define _GNU_SOURCE

#include <err.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "lauxlib.h"

#include "context.h"
#include "errors.h"
#include "tcp.h"
#include "web.h"
#include "websockets/ws.h"

/*==============================================================================
 * Defines
 */

#define LISTENQ 1024
#define MAXLINE 1024

#define CONTENT_LENGTH_KEY_LEN 16
#define REGISTRATION_FUNCNAME "register_connection"
#define DEREGISTRATION_FUNCNAME "deregister_connection"


/*==============================================================================
 * Static declarations
 */

/* Web handler context */
typedef struct WHContext_ {
        Context *context;
        int connfd;
} WHContext;

static const char content_length_key[] = "content-length: ";

static int deregister_ws_connection(int, Context *);
static int handle_http_request(WHContext *, const char *, size_t,
                                               const char *, size_t);
static void *handle_request_routine(void *);
static int handle_websocket_request(WHContext *, const char*);
static int lua_registration_helper(const char *, int, lua_State *);
static int push_message(lua_State *);
static int read_request_body(int, const char *, char **, size_t *);
static int read_request_string(int, char **, size_t *);
static int register_ws_connection(int, Context *);
static int registration_helper(const char *, int, Context *);


/*==============================================================================
 * Public API
 */


/*------------------------------------------------------------------------------
 * Registers "push_message" as a lua function.
 *
 * This enables us to move some of the server's broadcast logic into lua
 * modules.
 */
int
web_register_lua_funcs(lua_State *L)
{
        lua_pushcfunction(L, push_message);
        lua_setglobal(L, "push_message");

        return 0;
}

/*------------------------------------------------------------------------------
 * Listens for web requests.
 *
 * This runs in its own thread accepting connections on the specified port. It
 * spawns a new thread to handle each request via handle_request_routine.
 *
 * TODO: Allow port to be configured (add this to context)
 */
void *
web_routine(void *context)
{
        int option;
        pthread_t tid;
        WHContext *handler_context;
        int listenfd;
        int connfd;
        struct sockaddr_in cliaddr;
        struct sockaddr_in servaddr;
        socklen_t clilen = sizeof(cliaddr);
        uint16_t port = ((Context *)context)->port;

        syslog(LOG_INFO, "Listening on port: %d", port);

        /*
         * Start listening for web requests on a socket.
         *
         * We'll re-use the port so we don't have to wait before the program can
         * be restarted because of the TIME_WAIT state.
         */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);

        option = 1;
        if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &option,
                                              sizeof(option)) != 0)
                socket_failure(__FILE__, __LINE__);

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(port);

	if (bind(listenfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0)
                socket_failure(__FILE__, __LINE__);

	if (listen(listenfd, LISTENQ) < 0)
                socket_failure(__FILE__, __LINE__);

        /*
         * Accept connections, spinning up new request handler threads for each
         * one.
         */
        while(1) {
		connfd = accept(listenfd, (struct sockaddr*) &cliaddr, &clilen);

                /* Set up context for handler */
                if (NULL == (handler_context =
                                        (WHContext *)malloc(sizeof(WHContext))))
                        mem_alloc_failure(__FILE__, __LINE__);
                handler_context->context = (Context *)context;
                handler_context->connfd = connfd;

                /* Create handler thread and detach
                 * NOTE: handle_request_routine must free handler_context
                 */
                if (0 != pthread_create(&tid, NULL, handle_request_routine,
                                                    (void *)handler_context))
                        pthread_failure(__FILE__, __LINE__);
                if (pthread_detach(tid) != 0)
                        pthread_failure(__FILE__, __LINE__);
        }

        return NULL;
}


/*==============================================================================
 * Static functions
 */

/*------------------------------------------------------------------------------
 * Deregisters a websocket client.
 *
 * This happens in the lua space of the server.  Once a client is deregsitered,
 * no brodcasts will go to it. The connection file descriptor is used as the
 * key.
 */
static int
deregister_ws_connection(int connfd, Context *context)
{
        return registration_helper(DEREGISTRATION_FUNCNAME, connfd, context);
}


/*------------------------------------------------------------------------------
 * Handles HTTP request and writes out the HTTP response.
 *
 * This function helps the primary handle_request_routine by dealing with all
 * HTTP requests. The application logic for the route handling is in the lua
 * space (in Router.handle_request).
 *
 */
static int
handle_http_request(
              WHContext *wh_context, const char *request_string, size_t req_len,
                                              const char *body, size_t body_len)
{
        int connfd = wh_context->connfd;
        Context *context = wh_context->context;
        lua_State *L_main = context->main_lua_state;
        char *res_str = NULL;
        size_t res_len = 0;
        const char *tmp = NULL;
        int result = 0;

        /* Lock main lua mutex */
        lock_main(context);

        /*
         * Call "Router.handle_request"
         */
        lua_getglobal(L_main, "Router"); // This is set when requiring qplan.lua
        lua_pushstring(L_main, "handle_request");
        lua_gettable(L_main, -2);
        lua_pushlstring(L_main, request_string, req_len);
        lua_pushlstring(L_main, body, body_len);
        if (lua_pcall(L_main, 2, 1, 0) != 0) {
                syslog(LOG_ERR, lua_tostring(L_main, -1));
                lua_pop(L_main, 1);
                result = -1;
                goto error;
        }

        /*
         * Get response and write it out
         */
        tmp = lua_tolstring(L_main, -1, &res_len);
        if ((res_str = (char *)malloc(sizeof(char)*res_len)) == NULL)
                mem_alloc_failure(__FILE__, __LINE__);
        strncpy(res_str, tmp, res_len);
        lua_pop(L_main, 1);
        my_writen(connfd, res_str, res_len);

error:
        /*
         * Unlock main mutex and clean up
         */
        unlock_main(context);
        free(res_str);

        return result;
}

/*------------------------------------------------------------------------------
 * Handles web messages (both HTTP and establishing a WebSockets connection).
 *
 * Whenever web_routine gets a request, it spins up a new thread that runs this
 * function in it. If the request is normal HTTP, this hands off execution to
 * handle_http_request. If the request is the start of a WebSocket handshake, it
 * hands off execution to handle_websocket_request.
 *
 * NOTE: The req_context passed in as "arg" needs to be freed by this function.
 */
static void *
handle_request_routine(void *arg)
{
        WHContext *req_context = (WHContext *)arg;
        int connfd = req_context->connfd;
        char *request_string;
        size_t req_len;
        char *body = NULL;
        size_t body_len = 0;

        /*
         * Read in requst string and body (if needed)
         */
        if (read_request_string(connfd, &request_string, &req_len) < 0)
                goto error;

        if (read_request_body(connfd, request_string, &body, &body_len) < 0)
                goto error;

        /*
         * Hand off handling of request
         */
        if (ws_is_handshake(request_string))
                handle_websocket_request(req_context, request_string);
        else
                handle_http_request(req_context, request_string, req_len,
                                                            body, body_len);

error:
        close(connfd);
        free(request_string);
        free(body);
        free(req_context);
        return NULL;
}


/*------------------------------------------------------------------------------
 * Handles websocket requests.
 *
 * This completes the websocket handshake and then goes into a loop to read
 * websocket messages from a client.
 *
 */
static int
handle_websocket_request(WHContext *wh_context, const char* request_string)
{
        int connfd = wh_context->connfd;
        Context* context = wh_context->context;
        const char *res_str;
        uint8_t *response_frame = NULL;
        enum WebsocketFrameType frame_type;
        char *message = NULL;
        size_t frame_len;
        int result = 0;

        /*
         * Complete handshake to establish connection
         */
        res_str = ws_complete_handshake(request_string);
        my_writen(connfd, res_str, strlen(res_str));
        free((char *)res_str);
        register_ws_connection(connfd, context);


        /*
         * Read and handle messages
         *
         * NOTE: Currently, text frames are just echoed back. When we need to
         * customize this behavior, we should tie this up to lua.
         */
        while(1) {
                frame_type = ws_read_next_message(connfd, my_buffered_read, &message);
                if (frame_type == WS_FT_TEXT) {
                        frame_len = ws_make_text_frame(message, NULL, &response_frame);
                        my_writen(connfd, response_frame, frame_len);
                        free(response_frame);
                }
                else if (frame_type == WS_FT_PING) {
                        frame_len = ws_make_pong_frame(&response_frame);
                        my_writen(connfd, response_frame, frame_len);
                        free(response_frame);
                }
                else if (frame_type == WS_FT_CLOSE) {
                        deregister_ws_connection(connfd, context);
                        break;
                }
                else {
                        deregister_ws_connection(connfd, context);
                        result = -1;
                        break;
                }
        }

        return result;
}


/*------------------------------------------------------------------------------
 * Heart of register/deregister websocket clients.
 *
 * This was pulled out of registration_helper so we could call it outside of the
 * registration_helper function (which locks/unlocks mutex).
 *
 */
static int
lua_registration_helper(const char *funcname, int connfd, lua_State *L)
{
        int error = 0;

        lua_getglobal(L, "WebSocket"); // This is set when requiring qplan.lua
        lua_pushstring(L, funcname);
        lua_gettable(L, -2);
        lua_pushinteger(L, connfd);
        error = lua_pcall(L, 1, 1, 0);

        return error;
}


/*------------------------------------------------------------------------------
 * Writes message to websocket channel.
 *
 * This function will be called from lua. The websocket connection must already
 * have been established and the connection fd be registered. This should happen
 * as part of handle_websocket_request.
 */
static int
push_message(lua_State *L)
{
        uint8_t connfd;
        const char *message = NULL;
        uint8_t *response_frame = NULL;
        size_t frame_len;

        /*
         * Get connfd and message args from lua
         */
        connfd = luaL_checkunsigned(L, 1);
        message = luaL_checkstring(L, 2);

        /*
         * Write data to connfd. If something goes wrong, deregister this
         * channel.
         */
        frame_len = ws_make_text_frame(message, NULL, &response_frame);
        if (my_writen(connfd, response_frame, frame_len) != 0) {
                if (lua_registration_helper(DEREGISTRATION_FUNCNAME,connfd,L)) {
                        syslog(LOG_ERR, lua_tostring(L, -1));
                        lua_pop(L, 1);
                }
        }
        free(response_frame);

        return 0; /* No results */
}

/*------------------------------------------------------------------------------
 * Reads body from HTTP request.
 *
 * This reads in the request body based on the "Content-length" header in the
 * "request_string". On error, this frees body and returns -1.
 *
 * NOTE: Callers of this function are responsible for freeing "body".
 */
static int
read_request_body(int connfd, const char *request_string,
                                           char **body_p, size_t *body_len_p)
{
        char *s;
        char *body = NULL;
        size_t body_len = 0;

        /*
         * Read in body (if any)
         */
        if ((s = strcasestr(request_string, "Content-Length: "))) {
                body_len = strtol(s + CONTENT_LENGTH_KEY_LEN, NULL, 0);

                if ((body = malloc(body_len + 1)) == NULL)
                        mem_alloc_failure(__FILE__, __LINE__);

                if (my_buffered_read(connfd, body, body_len) < 0)
                        goto error;
        }

        /*
         * Store results
         */
        *body_p = body;
        *body_len_p = body_len;

        return 0;

error:
        free(body);
        return -1;
}



/*------------------------------------------------------------------------------
 * Reads HTTP request string up to, but not including, the message body.
 *
 * This may be a regular HTTP request or the beginning of a websocket handshake.
 * The request string includes all of the headers.
 *
 * NOTE: Callers of this function are responsible for freeing "request_string"
 */
static int
read_request_string(int connfd,
                               char **request_string_p, size_t *req_len_p)
{
	char buf[MAXLINE];
        int str_capacity;
        int cur_len;
        char *request_string;
        size_t req_len = 0;
        int i;

        /*
         * Allocate enough space to store MAXLINE chars to start.
         */
        if ((request_string = malloc(sizeof(char) * MAXLINE)) == NULL)
                mem_alloc_failure(__FILE__, __LINE__);
        str_capacity = MAXLINE;

        /*
         * Read in HTTP request string
         */
	while ((cur_len = my_readline(connfd, buf, MAXLINE)) > 0) {
		if (strcmp(buf, "\r\n") == 0)
			break;

                /*
                 * Build request string, increasing size if needed
                 */
                if ((req_len + cur_len) >= str_capacity) {
                        if ((request_string = realloc(request_string,
                            sizeof(char) * (str_capacity + MAXLINE))) == NULL)
                                mem_alloc_failure(__FILE__, __LINE__);

                        str_capacity += MAXLINE;
                }
                for (i = 0; i < cur_len; i++)
                        request_string[req_len++] = buf[i];
	}
        request_string[req_len] = '\0';

        /*
         * Return results
         */
        *request_string_p = request_string;
        *req_len_p = req_len;

        return 0;
}


/*------------------------------------------------------------------------------
 * Registers a websocket client.
 *
 * This happens in the lua space of the server.  Once a client is regsitered,
 * all brodcasts will go to it. The connection file descriptor is used as the
 * key.
 */
static int
register_ws_connection(int connfd, Context *context)
{
        return registration_helper(REGISTRATION_FUNCNAME, connfd, context);
}



/*------------------------------------------------------------------------------
 * Pulls out common code in register/deregister websocket clients.
 *
 * The funcname must be one of REGISTRATION_FUNCNAME or DEREGISTRATION_FUNCNAME.
 * These correspond to functions in the lua space. This is a wrapper around
 * lua_registration_helper.
 */
static int
registration_helper(const char *funcname, int connfd, Context *context)
{
        lua_State *L_main = context->main_lua_state;
        int result = 0;

        lock_main(context);

        /*
         * Call registration/deregistration function
         */
        if (0 != lua_registration_helper(funcname, connfd, L_main)) {
                syslog(LOG_ERR, lua_tostring(L_main, -1));
                lua_pop(L_main, 1);
                result = -1;
                goto error;
        }
        result = lua_tointeger(L_main, -1);
        lua_pop(L_main, 1);

error:
        unlock_main(context);
        return result;
}
