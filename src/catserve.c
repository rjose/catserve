#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <syslog.h>
#include <unistd.h>

#include <netinet/in.h>

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "context.h"
#include "errors.h"
#include "web.h"

/*============================================================================= 
 * Defines
 */
#define DEFAULT_PATH "."
#define DEFAULT_PORT 8888

/*=============================================================================
 * Static declarations
 */
static void clean_up();
static lua_State *init_lua_state(const char *config_file);
static void sigint_handler(int signal);

static pthread_mutex_t main_mutex = PTHREAD_MUTEX_INITIALIZER;
static lua_State *L_main;
static char *get_arg_value(const char *);
static int configure_app(int , char **, char **, uint16_t *, char **);




/*============================================================================= 
 * Main
 */
int
main(int argc, char *argv[])
{
	void *thread_result;
	long status;
        pthread_t web_thread_id;
        char *app_root = NULL;
        char *config_file = NULL;
        uint16_t port = DEFAULT_PORT;

        /* Enable syslog */
        openlog(NULL, LOG_CONS | LOG_PERROR | LOG_PID, LOG_USER);

        /* Handle program signals */
        signal(SIGPIPE, SIG_IGN);
        signal(SIGINT, &sigint_handler);

        /* Get app root and port from commandline args */
        if (configure_app(argc, argv, &app_root, &port, &config_file) != 0) {
                fprintf(stderr, "Usage: catserve -r=<app root path> -c=<config file> [-p=<port>]\n");
                exit(1);
        }
        if (0 != chdir(app_root ? app_root : DEFAULT_PATH))
                misc_failure(__FILE__, __LINE__);

        /* Set up lua */
        L_main = init_lua_state(config_file);
        if (web_register_lua_funcs(L_main) != 0)
                lua_failure(__FILE__, __LINE__);

        /* Spin up web server thread */
        Context context;
        context.main_lua_state = L_main;
        context.main_mutex = &main_mutex;
        context.port = port;
	status = pthread_create(&web_thread_id, NULL, web_routine, (void *)&context);
	if (status != 0)
                pthread_failure(__FILE__, __LINE__);


	/* Join web server thread when done. */
        /* NOTE: Most of the time, we'll exit through a SIGINT */
	status = pthread_join(web_thread_id, &thread_result);
	if (status != 0)
                pthread_failure(__FILE__, __LINE__);

        /* Clean up */
        clean_up();
        free(app_root);
	return 0;
}


/*============================================================================= 
 * Static functions
 */


/*------------------------------------------------------------------------------
 * Cleans up before exiting program
 */
static void
clean_up()
{
        lua_close(L_main);
        closelog();
	printf("\nWe are most successfully done!\n");
}


/*------------------------------------------------------------------------------
 * Returns app_root and port for catserve instance.
 */
static int
configure_app(int argc, char *argv[], char **app_root, uint16_t *port, char **config_file)
{
        int i;
        char *arg_value;
        int result = 0;
        int root_specified = 0;
        int config_specified = 0;

        for (i=1; i < argc; i++) {
                arg_value = get_arg_value(argv[i]);
                if (arg_value) {
                        switch(argv[i][1]) {
                                case 'r':
                                        *app_root = arg_value;
                                        root_specified = 1;
                                        break;

                                case 'p':
                                        *port = strtol(arg_value, NULL, 0);
                                        free(arg_value);
                                        break;

                                case 'c':
                                        *config_file = arg_value;
                                        config_specified = 1;
                                        break;

                                default:
                                        result = -1;
                                        break;
                        }
                }
        }
        if (!root_specified || !config_specified)
                result = -1;
        return result;
}


/*------------------------------------------------------------------------------
 * Returns value of commandline arg.
 *
 *      The arg input should look like "-p=8000". We're only handling single
 *      character flags. There must not be any whitespace in the arg.
 */
static char *
get_arg_value(const char *arg)
{
        char *result = NULL;
        int length = strlen(arg);
        int val_length = length - 3;

        if (length < 2 || arg[0] != '-' || arg[2] != '=')
                return NULL;

        if (NULL == (result = (char *)malloc(val_length + 1)))
                mem_alloc_failure(__FILE__, __LINE__);

        strncpy(result, arg+3, val_length + 1);
        return result;
}


/*------------------------------------------------------------------------------
 * Requires lua modules needed by the app
 *
 * NOTE: We may want to specify the file to require on the commandline. This
 * will make this app more generic.
 */
static lua_State *
init_lua_state(const char *config_file)
{
        lua_State *result = luaL_newstate();
        luaL_openlibs(result);

        /* Load functionality */
        lua_getglobal(result, "require");
        lua_pushstring(result, "init");
        if (lua_pcall(result, 1, 1, 0) != LUA_OK)
                luaL_error(result, "Problem requiring init.lua: %s",
                                lua_tostring(result, -1));

        /* Run initialize function */
        lua_getglobal(result, "initialize");
        lua_pushstring(result, config_file);
        if (lua_pcall(result, 1, 1, 0) != LUA_OK)
                luaL_error(result, "Problem calling 'initialize': %s",
                                lua_tostring(result, -1));
        return result;
}


/*------------------------------------------------------------------------------
 * Handles SIGINT by just cleaning up and exiting.
 */
static void
sigint_handler(int signal)
{
        clean_up();
        exit(0);
}
