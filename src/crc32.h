/* zlib's CRC-32, the one INDEX.TXT carries. */
#ifndef WARP64U_CRC32_H
#define WARP64U_CRC32_H

#include <stdint.h>

void crc32_init(void);
uint32_t crc32_update(uint32_t crc, const unsigned char *data, unsigned int length);

#define CRC32_START 0xFFFFFFFFUL
#define CRC32_FINISH(crc) ((uint32_t)((crc) ^ 0xFFFFFFFFUL))

#endif
