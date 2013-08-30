#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../ws.h"
#include "test_util.h"
#import "Testing.h"


/* ============================================================================
 * Expected results
 */


/*
 * Byte 0: 10000001
 *      Bit 0    (FIN):         1     (final fragment)
 *      Bits 4-7 (OPCODE):      8, 9, or A (close, ping, or pong)
 *
 * Byte 1: 00000101 
 *      Bit 0    (Mask):        0     (unmasked)
 *      Bits 1-7 (Payload len): 0
 *
 */
uint8_t close_frame[] = {0x88, 0x00};
uint8_t ping_frame[] = {0x89, 0x00};
uint8_t pong_frame[] = {0x8a, 0x00};



/* ============================================================================
 * Main
 */

int main()
{
        const uint8_t *frame = NULL;

        START_SET("Make frames");

        ws_make_close_frame(&frame);
        pass(1 == check_frame(close_frame, 2, frame), "Make close frame");
        free(frame);

        ws_make_ping_frame(&frame);
        pass(1 == check_frame(ping_frame, 2, frame), "Make ping frame");
        free(frame);

        ws_make_pong_frame(&frame);
        pass(1 == check_frame(pong_frame, 2, frame), "Make pong frame");
        free(frame);


        END_SET("Make frames");

        return 0;
}
