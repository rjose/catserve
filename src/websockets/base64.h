#ifndef BASE64_H
#define BASE64_H

#include <stdlib.h>
#include <stdint.h>

int base64_decode(uint8_t **dst, const char *src, size_t *data_len);
int base64_encode(char **dst, const uint8_t *src, size_t len);

#endif
