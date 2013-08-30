#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../ws.h"
#include "test_util.h"
#import "Testing.h"



/* ============================================================================
 * Test data
 */
static char empty_message[] = "";

static char hello_message[] = "Hello";

/* 125 chars is the biggest short message we can handle */
static char big_short_message[] =
        "Now is the time for all good men to come to to the aid of their "
        "party. How many more characters will it take to reach 125 !!!"
;


/* ============================================================================
 * Expected results
 */


/*
 * Byte 0: 10000001
 *      Bit 0    (FIN):         1     (final fragment)
 *      Bits 4-7 (OPCODE):      00001 (text frame)
 *
 * Byte 1: 00000101 
 *      Bit 0    (Mask):        0     (unmasked)
 *      Bits 1-7 (Payload len): 0x05
 *
 * Bytes 2-6: Payload           'H', 'e', 'l', 'l', 'o'
 */
uint8_t hello_message_frame[] = {0x81, 0x05,
                                 0x48, 0x65, 0x6c, 0x6c, 0x6f};

uint8_t empty_message_frame[] = {0x81, 0x00};

uint8_t big_short_frame_start[] = {0x81, 0x7d}; 

/*
 * Byte 0: 10000001
 *      Bit 0    (FIN):         1     (final fragment)
 *      Bits 4-7 (OPCODE):      00001 (text frame)
 *
 * Byte 1: 10000101 
 *      Bit 0    (Mask):        1     (masked)
 *      Bits 1-7 (Payload len): 0x05
 *
 * Bytes 2-5: Mask bytes        0x37, 0xfa, 0x21, 0x3d
 * Bytes 6-10: Payload          (masked "Hello")
 */
uint8_t masked_hello_frame[] = {0x81, 0x85,
                                0x37, 0xfa, 0x21, 0x3d,
                                0x7f, 0x9f, 0x4d, 0x51, 0x58};



/* ============================================================================
 * Main
 */

int main()
{
        const uint8_t *frame = NULL;
        ssize_t frame_len = 0;

        /*
         * Build frame for small message
         */
        START_SET("Build small message");

        frame_len = ws_make_text_frame(hello_message, NULL, &frame);
        pass(7 == frame_len, "Check frame length");
        pass(1 == check_frame(hello_message_frame, 7, frame), "Hello message");
        free(frame);

        frame_len = ws_make_text_frame(empty_message, NULL, &frame);
        pass(2 == frame_len, "Check frame length");
        pass(1 == check_frame(empty_message_frame, 2, frame), "'' message");
        free(frame);

        frame_len = ws_make_text_frame(big_short_message, NULL, &frame);
        pass(127 == frame_len, "Check frame length");
        pass(1 == check_frame(big_short_frame_start, 2, frame), "big short message");
        // Check first and last chars of message body
        pass(0x4e == frame[2], "First letter should be 'N'");
        pass(0x21 == frame[126], "Last letter should be '!'");
        free(frame);

        END_SET("Build small message");

        /*
         * Build frame for small masked message
         */
        START_SET("Build small masked message");

        uint8_t mask[] = {0x37, 0xfa, 0x21, 0x3d};
        frame_len = ws_make_text_frame(hello_message, mask, &frame);
        pass(11 == frame_len, "Check frame length");
        pass(1 == check_frame(masked_hello_frame, 11, frame), "Masked hello");

        END_SET("Build small masked message");

        return 0;
}
