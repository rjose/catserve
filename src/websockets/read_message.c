#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>

#include "util.h"
#include "ws.h"
#include "constants.h"
#include "errors.h"

/*==============================================================================
 * Defines
 */

#define MAXLINE 1000


/* ============================================================================ 
 * Static declarations
 */
enum WebsocketReadState {
        WSF_START,
        WSF_READ_MED_LEN,
        WSF_READ_LONG_LEN,
        WSF_READ
};

typedef struct WebsocketFrame_ {
        uint8_t *buf;
        size_t buf_len;
        size_t num_read;
        size_t num_to_read;
        enum WebsocketReadState read_state;
} WebsocketFrame;


static char *append_message(char *, char *);
static int ws_append_bytes(WebsocketFrame *, uint8_t *, size_t);
static int ws_extend_frame_buf(WebsocketFrame *, size_t);
static const uint8_t *ws_extract_message(const uint8_t *);
static int ws_init_frame(WebsocketFrame *);
static int ws_is_close_frame(const uint8_t*);
static int ws_is_final(const uint8_t*);
static int ws_is_ping_frame(const uint8_t*);
static int ws_is_pong_frame(const uint8_t*);
static int ws_is_text_frame(const uint8_t*);
static int ws_update_read_state(WebsocketFrame *);


/*==============================================================================
 * Public API
 */

/*------------------------------------------------------------------------------
 * Reads next message from a websocket channel.
 *
 * When reading frames from a socket connection, we have to be careful not to
 * read only the number of bytes that are part of the frame.  To do this, we set
 * up a little state machine. We update the number of bytes to read using
 * "ws_update_read_state" and "ws_append_bytes". When "ws_update_read_state"
 * returns 0, we have a valid websocket frame.
 *
 * If the websocket frame has a message in it, we can use "ws_extract_message"
 * to retrieve it.
 *
 * NOTE: This also handles messages split into multiple frame fragments.
 *
 */
enum WebsocketFrameType
ws_read_next_message(int connfd, ws_read_bytes_fp read_bytes, char **message)
{
        WebsocketFrame frame;
        enum WebsocketFrameType result;
        char *frame_message = NULL;
	char buf[MAXLINE+1];
        int num_to_read;
        int num_read;
        char *tmp = NULL;

        /*
         * This reads frames in and combines any fragments together
         */
        frame.buf = NULL;
        while (1) {
                /*
                 * Read a frame in
                 */
                ws_init_frame(&frame);
                while (ws_update_read_state(&frame) == 1) {
                        num_to_read = frame.num_to_read;
                        if (num_to_read > MAXLINE)
                                num_to_read = MAXLINE;

                        if (num_to_read == 0)
                                continue;

                        if ((num_read = read_bytes(connfd, buf, num_to_read)) <= 0) {
                                result = WS_FT_ERROR;
                                goto error;
                        }

                        ws_append_bytes(&frame, (uint8_t *)buf, num_read);
                }

                /*
                 * Handle frame
                 */
                if (ws_is_text_frame(frame.buf)) {
                        result = WS_FT_TEXT;
                        tmp = (char *) ws_extract_message(frame.buf);

                        /* NOTE: append_message will free tmp if needed */
                        frame_message = append_message(frame_message, tmp);
                }
                else if (ws_is_ping_frame(frame.buf))
                        result = WS_FT_PING;
                else if (ws_is_pong_frame(frame.buf))
                        result = WS_FT_PONG;
                else if (ws_is_close_frame(frame.buf))
                        result = WS_FT_CLOSE;
                else {
                        result = WS_FT_ERROR;
                        syslog(LOG_ERR, "Unknown websocket frame type");
                }

                /*
                 * If this is the final fragment, we're done; otherwise,
                 * continue collecting frames.
                 */
                if (ws_is_final(frame.buf)) {
                        *message = frame_message;
                        break;
                }
        }

error:
        free(frame.buf);
        return result;
}

/*==============================================================================
 * Static functions
 */


/*------------------------------------------------------------------------------
 * Concatenates src onto dst.
 *
 * NOTE: This could go into util.c if anyone else needed it.
 */
static char *
append_message(char *dst, char *src)
{
        size_t src_len;
        size_t dst_len;

        if (src == NULL)
                return dst;

        if (dst == NULL)
                return src;

        src_len = strlen(src);
        dst_len = strlen(dst);
        if ((dst=(char *)realloc(dst, dst_len + src_len + 1)) == NULL)
                mem_alloc_failure(__FILE__, __LINE__);

        strncpy(dst+dst_len, src, src_len);
        free(src);

        return dst;
}

/*------------------------------------------------------------------------------
 * Appends bytes to a frame's buf.
 *
 * NOTE: Assuming the frame->buf has enough space
 */
static int
ws_append_bytes(WebsocketFrame *frame, uint8_t *src, size_t n)
{
        size_t i;
        if (frame->num_read + n > frame->buf_len)
                return -1;

        for (i = 0; i < n; i++) {
                frame->buf[frame->num_read++] = src[i];
                frame->num_to_read--;
        }

        if (frame->num_to_read < 0)
                return -1;

        return 0;
}


/*------------------------------------------------------------------------------
 * Allocates more space to frame->buf.
 */
static int
ws_extend_frame_buf(WebsocketFrame *frame, size_t more_len)
{
        if ((frame->buf =
             (uint8_t *)realloc(frame->buf, frame->buf_len + more_len)) == NULL)
                mem_alloc_failure(__FILE__, __LINE__);

        frame->buf_len += more_len;
        return 0;
}


/*------------------------------------------------------------------------------
 * Extracts message from a frame.
 *
 * The message may be either text or binary. If text, then the string is NUL
 * terminated.
 *
 * NOTE: Caller of this function is responsible for freeing the returned data.
 */
static const uint8_t *
ws_extract_message(const uint8_t *frame)
{
        uint64_t i;
        uint8_t byte0;
        uint8_t byte1;
        uint64_t message_len;
        uint8_t message_start;
        uint8_t *mask;
        uint8_t *result;
        uint8_t num_len_bytes;

        /* Only handling TEXT or BIN frames */
        byte0 = frame[0];
        if (!(byte0 | WS_FRAME_OP_TEXT || byte0 | WS_FRAME_OP_BIN))
                return NULL;

        byte1 = frame[1];

        /*
         * Compute message length
         */
        message_len = byte1 & ~WS_FRAME_MASK;
        if (message_len <= SHORT_MESSAGE_LEN) {
                num_len_bytes = 0;
        }
        else if (message_len == MED_MESSAGE_KEY) {
                num_len_bytes = 2;
                message_len = 0;
        }
        else if (message_len == LONG_MESSAGE_KEY) {
                num_len_bytes = 8;
                message_len = 0;
        }
        else {
                /* Should never get here */
                return NULL;
        }

        /* This only does anything for medium and long messages */
        for (i = 0; i < num_len_bytes; i++) {
                message_len <<= 8;
                message_len += frame[2 + i];
        }

        /*
         * Figure out where message and mask start.
         */
        mask = NULL;
        message_start = 2 + num_len_bytes;
        if (byte1 & WS_FRAME_MASK) {
                mask = (uint8_t *)frame + 2 + num_len_bytes;
                message_start = 2 + num_len_bytes + MASK_LEN;
        }
        
        /*
         * Allocate memory and fill in the message.
         */
        if ((result = (uint8_t *)malloc(message_len + 1)) == NULL)
                mem_alloc_failure(__FILE__, __LINE__);

        for (i = 0; i < message_len; i++)
                result[i] = toggle_mask(frame[message_start+i], i, mask);

        /* Add NUL if text frame */
        if (byte0 | WS_FRAME_OP_TEXT)
                result[message_len] = '\0';

        return result;
}

/*------------------------------------------------------------------------------
 * Initializes a frame so it's ready for reading.
 *
 * NOTE: The caller must free frame->buf when done.
 */
static int
ws_init_frame(WebsocketFrame *frame)
{
        free(frame->buf);
        frame->buf = NULL;
        frame->buf_len = 0;
        frame->num_to_read = 2;
        frame->num_read = 0;
        frame->read_state = WSF_START;

        ws_extend_frame_buf(frame, frame->num_to_read);
        return 0;
}


/*------------------------------------------------------------------------------
 * Checks if frame_str is a CLOSE frame.
 */
static int
ws_is_close_frame(const uint8_t* frame_str)
{
        return (frame_str[0] & 0x0f) == WS_FRAME_OP_CLOSE;
}

/*------------------------------------------------------------------------------
 * Checks if frame_str is a PING frame.
 */
static int
ws_is_ping_frame(const uint8_t* frame_str)
{
        return (frame_str[0] & 0x0f) == WS_FRAME_OP_PING;
}

/*------------------------------------------------------------------------------
 * Checks if frame_str is a PONG frame.
 */
static int
ws_is_pong_frame(const uint8_t* frame_str)
{
        return (frame_str[0] & 0x0f) == WS_FRAME_OP_PONG;
}

/*------------------------------------------------------------------------------
 * Checks if frame_str is a TEXT frame.
 */
static int
ws_is_text_frame(const uint8_t* frame_str)
{
        return (frame_str[0] & 0x0f) == WS_FRAME_OP_TEXT;
}

/*------------------------------------------------------------------------------
 * Checks if frame_str is the final fragment in a message.
 */
static int
ws_is_final(const uint8_t* frame_str)
{
        return (frame_str[0] & 0xf0) == WS_FRAME_FIN;
}

/*------------------------------------------------------------------------------
 * Implements state machine for reading websocket frames.
 *
 * Returns 1 if there's still more to read; 0 if no more to read; -1 if
 * something went wrong (which probably means we should close the websocket
 * connection).
 *
 * This function is idempotent.
 */
static int
ws_update_read_state(WebsocketFrame *frame)
{
        size_t i;
        uint8_t byte1;
        size_t message_len;
        int mask_len;
        int num_len_bytes;

        /* If there's more to read, then come back when you're done */
        if (frame->num_to_read > 0)
                return 1;

        /*
         * If we were reading in the message payload (the last part of the
         * frame), and there's no more to read (see condition directly above),
         * then there's nothing left to do.
         */
        if (frame->read_state == WSF_READ)
                return 0;

        /*
         * If we're just starting and have finished reading the first 2 bytes of the
         * frame, we need to check if we there's a mask we need to read in and
         * how long the message is.
         *
         * For short messages (<= 125) we have all the information we need in
         * the first 2 bytes of the frame. For medium and longer messages, we'll
         * need to read in more bytes to compute the length before we can start
         * reading in the payload.
         */
        if (frame->read_state == WSF_START) {
                byte1 = frame->buf[1];
                mask_len = (byte1 & WS_FRAME_MASK) ? 4 : 0;
                message_len = byte1 & ~WS_FRAME_MASK;

                if (message_len <= SHORT_MESSAGE_LEN) {
                       frame->num_to_read = message_len + mask_len;
                       frame->read_state = WSF_READ;
                }
                else if (message_len == MED_MESSAGE_KEY) {
                        frame->num_to_read = NUM_MED_LEN_BYTES + mask_len;
                        frame->read_state = WSF_READ_MED_LEN;
                }
                else if (message_len == LONG_MESSAGE_KEY) {
                        frame->num_to_read = NUM_LONG_LEN_BYTES + mask_len;
                        frame->read_state = WSF_READ_LONG_LEN;
                }
                else {
                        return -1;
                }
                ws_extend_frame_buf(frame, frame->num_to_read);
                return 1;
        }


        /*
         * At this point, we have enough info to compute the payload length.
         */
        if (frame->read_state == WSF_READ_MED_LEN)
                num_len_bytes = NUM_MED_LEN_BYTES;
        else if (frame->read_state == WSF_READ_LONG_LEN)
                num_len_bytes = NUM_LONG_LEN_BYTES;
        else {
                /* If we got here, then something got messed up */
                return -1;
        }

        message_len = 0;
        for (i = 0; i < num_len_bytes; i++) {
                message_len <<= 8;
                message_len += frame->buf[2 + i];
        }

        frame->num_to_read = message_len;
        frame->read_state = WSF_READ;
        ws_extend_frame_buf(frame, frame->num_to_read);
        return 1;
}
