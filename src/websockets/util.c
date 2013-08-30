#include "util.h"

uint8_t toggle_mask(uint8_t c, size_t index, const uint8_t mask[4])
{
        uint8_t result = c;
        if (mask)
                result = c ^ mask[index % 4];

        return result;
}

