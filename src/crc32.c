#include "crc32.h"

static uint32_t table[256];

void crc32_init(void)
{
    unsigned int n;
    unsigned char k;
    uint32_t c;
    for (n = 0; n < 256; ++n) {
        c = n;
        for (k = 0; k < 8; ++k) c = (c & 1) ? (0xEDB88320UL ^ (c >> 1)) : (c >> 1);
        table[n] = c;
    }
}

uint32_t crc32_update(uint32_t crc, const unsigned char *data, unsigned int length)
{
    while (length--) crc = table[(unsigned char)(crc ^ *data++)] ^ (crc >> 8);
    return crc;
}
