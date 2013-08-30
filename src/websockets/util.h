#ifndef UTIL_H
#define UTIL_H

#include <stdint.h>

#include <sys/types.h>

uint8_t toggle_mask(uint8_t c, size_t index, const uint8_t mask[4]);

#endif
