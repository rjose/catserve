#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include "constants.h"
#include "errors.h"
#include "util.h"

/*==============================================================================
 * Public API
 */


/*------------------------------------------------------------------------------
 * Makes a text frame based for the specified message.
 *
 * NOTE: This function will always set the FIN bit to 1. If you want to send
 * fragments, set this to 0 once you get the frame back.
 */
size_t
ws_make_text_frame(const char *message, const uint8_t mask[4], uint8_t **frame_p)
{
        uint64_t i;
        uint64_t message_len;
        size_t mask_len;
        size_t num_len_bytes; /* Number of extended payload len bytes */
        uint8_t byte0, byte1;     /* First two bytes of the frame */
        uint64_t tmp;
        size_t frame_len;
        uint8_t *result = NULL;

        /* We know this is a text frame */
        byte0 = WS_FRAME_OP_TEXT;
        byte0 |= WS_FRAME_FIN;

        /* If a mask is specified, set the mask bit */
        byte1 = mask ? WS_FRAME_MASK : 0;

        /*
         * Figure out the length of the frame and then allocate memory. This
         * involves figuring out if we need a mask, if we need extra length
         * bytes, and how big the message is.
         */
        mask_len = mask ? 4 : 0;
        message_len = strlen(message);
        if (message_len <= SHORT_MESSAGE_LEN) {
                num_len_bytes = 0;
                byte1 |= message_len;
        }
        else if (message_len > SHORT_MESSAGE_LEN &&
                                               message_len <= MED_MESSAGE_LEN) {
                num_len_bytes = 2;
                byte1 |= MED_MESSAGE_KEY;
        }
        else {
                num_len_bytes = 8;
                byte1 |= LONG_MESSAGE_KEY;
        }
        frame_len = 2 + num_len_bytes + mask_len + message_len;
        if ((result = (uint8_t *)malloc(frame_len)) == NULL)
                mem_alloc_failure(__FILE__, __LINE__);

        /*
         * Write data into the frame. First, we'll write the first 2 bytes
         * that we've constructed. After this comes the extended payload
         * length (if needed). After that is the mask (if needed). Finally, we
         * write our message.
         */
        result[0] = byte0;
        result[1] = byte1;

        /* Write extended length */
        tmp = message_len;
        for (i = num_len_bytes; i > 0; i--) {
                result[2 + i - 1] = tmp & 0xFF;
                tmp >>= 8;
        }
        
        /* Write mask */
        if (mask)
                for (i = 0; i < mask_len; i++)
                        result[2 + num_len_bytes + i] = mask[i];

        /* Write message */
        for (i = 0; i < message_len; i++) {
                result[2 + num_len_bytes + mask_len + i] =
                                       toggle_mask(message[i], i, mask);
        }

        /*
         * Return results
         */
        *frame_p = result;

        return frame_len;
}


/*------------------------------------------------------------------------------
 * Makes a close frame.
 */
size_t
ws_make_close_frame(uint8_t **frame_p)
{
        uint8_t byte0, byte1;     /* First two bytes of the frame */
        uint8_t *result = NULL;

        byte0 = WS_FRAME_OP_CLOSE;
        byte0 |= WS_FRAME_FIN;

        byte1 = 0;

        if ((result = (uint8_t *)malloc(2)) == NULL)
                mem_alloc_failure(__FILE__, __LINE__);

        result[0] = byte0;
        result[1] = byte1;

        /*
         * Return result
         */
        *frame_p = result;

        return 2;
}


/*------------------------------------------------------------------------------
 * Makes a ping frame.
 */
size_t
ws_make_ping_frame(uint8_t **frame_p)
{
        uint8_t byte0, byte1;     /* First two bytes of the frame */
        uint8_t *result = NULL;

        byte0 = WS_FRAME_OP_PING;
        byte0 |= WS_FRAME_FIN;

        byte1 = 0;

        if ((result = (uint8_t *)malloc(2)) == NULL)
                mem_alloc_failure(__FILE__, __LINE__);

        result[0] = byte0;
        result[1] = byte1;

        /*
         * Return result
         */
        *frame_p = result;
        return 2;
}


/*------------------------------------------------------------------------------
 * Makes a pong frame.
 */
size_t
ws_make_pong_frame(uint8_t **frame_p)
{
        uint8_t byte0, byte1;     /* First two bytes of the frame */
        uint8_t *result = NULL;

        byte0 = WS_FRAME_OP_PONG;
        byte0 |= WS_FRAME_FIN;

        byte1 = 0;

        if ((result = (uint8_t *)malloc(2)) == NULL)
                mem_alloc_failure(__FILE__, __LINE__);

        result[0] = byte0;
        result[1] = byte1;

        /*
         * Return result
         */
        *frame_p = result;
        return 2;
}
