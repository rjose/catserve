#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../ws.h"
#include "test_util.h"
#import "Testing.h"


#define BUF_LEN 100


/* ============================================================================
 * Main
 */


static uint8_t input_ping_frame[] = {0x89, 0x00};
static uint8_t input_pong_frame[] = {0x8A, 0x00};
static uint8_t input_close_frame[] = {0x88, 0x00};
static uint8_t input_hello_frame[] = {0x81, 0x05, 0x48, 0x65, 0x6c, 0x6c, 0x6f};
static uint8_t input_hello_frag_frame[] = {0x01, 0x03, 0x48, 0x65, 0x6c,
                                           0x81, 0x02, 0x6c, 0x6f};

static uint8_t *source_bytes;

// TODO: Configure to just read 2 bytes at a time
/*
 * NOTE: source_bytes needs to be set first
 */
static size_t read_bytes(int fd, char *ptr, size_t maxlen)
{
        int i;
        for (i = 0; i < maxlen; i++)
                *ptr++ = *source_bytes++;
        return i;
}

int main()
{
        enum WebsocketFrameType frame_type;
        char *message = NULL;
        int fd = 1;

        START_SET("Read ping, pong, close frame");

        source_bytes = input_ping_frame;
        frame_type = ws_read_next_message(fd, read_bytes, &message);
        pass(frame_type == WS_FT_PING, "Read ping frame");
        free(message);

        source_bytes = input_pong_frame;
        frame_type = ws_read_next_message(fd, read_bytes, &message);
        pass(frame_type == WS_FT_PONG, "Read pong frame");
        free(message);

        source_bytes = input_close_frame;
        frame_type = ws_read_next_message(fd, read_bytes, &message);
        pass(frame_type == WS_FT_CLOSE, "Read close frame");
        free(message);

        END_SET("Read ping, pong, close frame");

        START_SET("Read text frames");

        source_bytes = input_hello_frame;
        frame_type = ws_read_next_message(fd, read_bytes, &message);
        pass(frame_type == WS_FT_TEXT, "Read text frame");
        pass(strcmp(message, "Hello") == 0, "Got hello");
        free(message);

        source_bytes = input_hello_frag_frame;
        frame_type = ws_read_next_message(fd, read_bytes, &message);
        pass(frame_type == WS_FT_TEXT, "Read fragmented text frame");
        pass(strcmp(message, "Hello") == 0, "Got hello");
        free(message);

        END_SET("Read text frames");
        return 0;
}

static void demo_read_bytes()
{
        char buf[BUF_LEN];
        size_t num_read;

        source_bytes = input_ping_frame;
        num_read = read_bytes(1, buf, 2);
        printf("num_read: %d, byte0: 0x%x\n", num_read, buf[0]);
}
