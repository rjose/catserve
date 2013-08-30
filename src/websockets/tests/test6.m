#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include "../ws.h"
#include "../read_message.c"
#include "test_util.h"
#import "Testing.h"


/* ============================================================================
 * Test data
 */

static const char med126txt[] = "./data/med-126.txt";
static char med126[126 + 1];


static const char long66000txt[] = "./data/long-66000.txt";
static char long66000[66000 + 1];

/* ============================================================================
 * Main
 */

int main()
{
        const char *message_body = NULL;
        const uint8_t *frame = NULL;

        START_SET("Extract medium message");
        load_data(med126, 126, med126txt);
        ws_make_text_frame(med126, NULL, &frame);
        
        message_body = ws_extract_message(frame);
        pass(0 == strcmp(med126, message_body), "Extract medium");

        free(frame);
        free(message_body);
        END_SET("Extract medium message");



        START_SET("Extract long message");
        load_data(long66000, 66000, long66000txt);
        ws_make_text_frame(long66000, NULL, &frame);
        
        message_body = ws_extract_message(frame);
        pass(0 == strcmp(long66000, message_body), "Extract long");

        free(frame);
        free(message_body);

        END_SET("Extract long message");
        return 0;
}
