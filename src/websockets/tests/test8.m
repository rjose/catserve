#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* NOTE: Including the C file to test static functions */
#include "../read_message.c"

#include "test_util.h"
#import "Testing.h"



/* ============================================================================
 * Main
 */
int main()
{
        uint8_t set1[50];
        size_t num;

        START_SET("How much to read");

        /* Create frame */
        WebsocketFrame frame1;
        frame1.buf = NULL;
        ws_init_frame(&frame1);

        /* Figure out num bytes to read at the beginning */
        pass(2 == frame1.num_to_read, "Should start by reading 2 bytes");
        pass(WSF_START == frame1.read_state, "State should be WSF_START");

        set1[0] = 0x81;
        set1[1] = 0x05;
        pass(0 == ws_append_bytes(&frame1, set1, 2), "Append 2 bytes");
        pass(2 == frame1.num_read, "num_read should be 2");
        pass(0 == frame1.num_to_read, "num_to_read should be 0");
        pass(0x81 == frame1.buf[0], "Check first byte");
        pass(0x05 == frame1.buf[1], "Check second byte");

        pass(1 == ws_update_read_state(&frame1), "Should have more to read");
        pass(5 == frame1.num_to_read, "num_to_read should be 5");
        pass(7 == frame1.buf_len, "buf_len should be 7");
        pass(WSF_READ == frame1.read_state, "should be WSF_READ");

        set1[0] = 'H';
        set1[1] = 'e';
        set1[2] = 'l';
        set1[3] = 'l';
        set1[4] = 'o';
        ws_append_bytes(&frame1, set1, 5);
        pass(0 == frame1.num_to_read, "num_to_read should be 0");
        pass(0 == ws_update_read_state(&frame1), "Shouldn't have more to read");
        

        END_SET("How much to read");

        // TODO: Test each of these functions individually
        // TODO: Test num to read for short mask, for medium, for long
        return 0;
}
