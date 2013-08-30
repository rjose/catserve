#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "../ws.h"
#include "../read_message.c"

#import "Testing.h"

/* ============================================================================
 * Test data
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
 * Expected results
 */
static char empty_message[] = "";

static char hello_message[] = "Hello";

/* ============================================================================
 * Main
 */

int main()
{
        const char *message_body = NULL;

        /*
         * Read small message
         */
        START_SET("Extract small message");
        message_body = ws_extract_message(hello_message_frame);
        pass(0 == strcmp(message_body, hello_message), "Hello message");
        free(message_body);

        message_body = ws_extract_message(empty_message_frame);
        pass(0 == strcmp(message_body, empty_message), "Empty message");
        free(message_body);
        END_SET("Extract small message");

        /*
         * Read small, masked message
         */
        START_SET("Extract small, masked message");

        message_body = ws_extract_message(masked_hello_frame);
        pass(0 == strcmp(message_body, hello_message), "Masked message");
        free(message_body);

        END_SET("Extract small, masked message");

        return 0;
}
