#include <stdint.h>
#include <string.h>

#include "base64.h"

/*==============================================================================
 * Defines
 */

#define PADDING '='


/*==============================================================================
 * Static declarations
 */
const static char digits64[] =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int char_to_data(char c, uint8_t *data);


/*==============================================================================
 * Public API
 */


/*------------------------------------------------------------------------------
 * Converts binary src bytes to base64-encoded text in dst.
 */
int base64_encode(char **dst, const uint8_t *src, size_t len)
{
        size_t i;
        size_t res_index;
        size_t pad_length;
        size_t result_len;
        uint8_t cur, leftover;
        uint8_t *result;

        /*
         * The pad length will be 0, 1, or 2 depending on how the final bits in
         * the source data lay over the 6-bit encoding characters.
         */
        pad_length = (8 * len) % 6;
        pad_length = pad_length == 0 ? 0 : (6 - pad_length)/2;

        /*
         * Next, we allocate memory for the result string.  To find the number
         * of 6-bit bytes we need, we multiply the number of source bytes by 8
         * and (integer) divide by 6. If we need to do any padding, that means
         * we need another 6-bit byte for the leftover bits. We'll also need to
         * allocate for the padding characters.
         */
        result_len = len * 8 / 6;
        result_len += (pad_length ? 1 : 0) + pad_length;

        if ((result = (uint8_t *)malloc(result_len + 1)) == NULL)
                exit(-1);

        /*
         * We can view the encoding of the src data as 3 cases which cycle over
         * the bytes:
         *
         *   Case 0: Start
         *   -------------
         *   Here, the top 6-bits of the first byte are downshifted 2 to form
         *   then ext encoded char. The leftover 2 bits are upshifted by 4 to be
         *   used as part of the next encoded byte. 
         *
         *   Case 1: Middle
         *   --------------
         *   Here, the 2 bits from Case 0 are added to the first 4 bits of the
         *   current byte (which are downshifted 4) to identify the next
         *   encoding char. The remaining 4 bits of the current byte are
         *   upshifted 2 to be used as part of the next encoded byte.
         *
         *   Case 2: End
         *   -----------
         *   Here, the 4 bits from Case 1 are added to the first 2 bits of the
         *   current byte (downshifted 6) to identify the next encoding char.
         *   This leaves exactly 6 bits from the current byte which are used to
         *   identify one more encoding char.
         */
        leftover = 0;
        res_index = 0;
        for (i = 0; i < len; i++) {
                cur = src[i];
                switch (i % 3) {
                        case 0:
                                result[res_index++] = digits64[cur >> 2];
                                leftover = (0x3 & cur) << 4;
                                break;

                        case 1:
                                result[res_index++] =
                                                digits64[leftover + (cur >> 4)];
                                leftover = (0xF & cur) << 2;
                                break;

                        case 2:
                                result[res_index++] =
                                                digits64[leftover + (cur >> 6)];
                                result[res_index++] = digits64[0x3F & cur];
                                leftover = 0;
                                break;
                }
        }


        /*
         * If there are any leftover bits, they will already have been shifted
         * appropriately, so we can add the next encoding char directly.
         */
        if (leftover)
                result[res_index++] = digits64[leftover];

        /*
         * The last step is to add the padding characters (if needed)
         */
        for (i = 0; i < pad_length; i++) {
                result[res_index++] = PADDING;
        }

        /*
         * Don't forget to terminate the string!
         */
        result[res_index++] = '\0';

        *dst = (char *)result;
        return 0;
}


/*------------------------------------------------------------------------------
 * Converts a base64-encoded string into binary bytes.
 */
int base64_decode(uint8_t **dst, const char *src, size_t *data_len)
{
        size_t i;
        size_t res_index;
        size_t res_len;
        size_t src_len;
        uint8_t cur;
        uint8_t leftover = 0;
        uint8_t *result = NULL;

        /*
         * First, we have to figure out how much memory we need and then
         * allocate enough for the result.
         *
         * We start by looking at the src length. Because each byte in src has 6
         * bits of data, to figure out the number of 8-bit bytes we need, we
         * multiply the src length by 6 and then divide by 8. After that, we
         * subtract off a byte for each padding byte in src.
         */
        src_len = strlen(src);
        if (src_len == 0)
                goto error;

        res_len = src_len * 6 / 8;
        i = src_len;
        while (i > 0 && src[i - 1] == PADDING) {
                res_len--;
                i--;
        }

        if ((result = (uint8_t *)malloc(res_len)) == NULL)
                exit(-1);
        
        /*
         * Next, we decode the data.
         *
         * We'll decode byte-by-byte until we reach the end of src or hit a
         * PADDING char. If anything goes wrong, we bail out. Note that the
         * first 2 bits of each src byte are 0 since only 6 bits are used per
         * src byte.
         *
         * There are 4 cases that cycle:
         *
         *   Case 0:
         *   -------
         *   This is the beginning of the cycle. All we can do is upshift the
         *   6-bits of data in anticipation of future steps.
         *
         *   Case 1:
         *   -------
         *   We combine the bits from Case 0 with the first 2 data bits of
         *   the current byte. The remaining 4 bits are upshifted for later.
         *
         *   Case 2:
         *   -------
         *   We take the 4 bits from Case 1 and combine with the first 4 data
         *   bits of the current byte. The remaining 2 bits are upshifted for
         *   later.
         *
         *   Case 3:
         *   -------
         *   Here, we combine the 2 bits from Case 2 with the 6 bits of the
         *   current byte. There are no leftover bits at this point.
         *
         */
        res_index = 0;
        for (i = 0; i < src_len; i++) {
                if (src[i] == PADDING)
                        break;

                if (char_to_data(src[i], &cur) != 0)
                        goto error;

                switch(i % 4) {
                        case 0:
                                leftover = cur << 2;
                                break;

                        case 1:
                                result[res_index++] = leftover + (cur >> 4);
                                leftover = (0xF & cur) << 4;
                                break;

                        case 2:
                                result[res_index++] = leftover + (cur >> 2);
                                leftover = (0x3 & cur) << 6;
                                break;

                        case 3:
                                result[res_index++] = leftover + cur;
                                leftover = 0;
                                break;
                }
        }

        /*
         * Any leftover bits are ready to be stored.
         */
        if (leftover)
                result[res_index++] = leftover;

        /*
         * Since we're constructing binary data, we have to let the caller know
         * how many bytes are in the result.
         */
        if (data_len)
                *data_len = res_len;

        /*
         * Finally, we can return the result.
         */
        *dst = result;
        return 0;

error:
        if (result)
                free(result);

        return -1;
}


/*==============================================================================
 * Static functions
 */

/*------------------------------------------------------------------------------
 * Maps a character into its agreed-upon base64 value.
 */
static int char_to_data(char c, uint8_t *data)
{
        if (c >= 'A' && c <= 'Z')
                *data = c - 'A';
        else if (c >= 'a' && c <= 'z')
                *data = c - 'a' + 26;
        else if (c >= '0' && c <= '9')
                *data = c - '0' + 52;
        else if (c == '+')
                *data = 62;
        else if (c == '/')
                *data = 63;
        else
                return -1;

        return 0;
}
