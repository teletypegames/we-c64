/* The preview of a title: NAME.PVW from the shelf, a C64 bitmap shown on the
 * whole screen until a key is pressed.
 *
 *   byte 0       mode: 0 hires, 1 multicolor
 *   byte 1       background color ($D021, multicolor)
 *   byte 2       border color
 *   1000 bytes   screen matrix: the two colors of a hires cell, or the 01
 *                and 10 colors of a multicolor one (high and low nibble)
 *   1000 bytes   color RAM, the 11 color (multicolor only)
 *   8000 bytes   bitmap
 *
 * The VIC shows it from bank 3: the bitmap at $E000, under the KERNAL (a
 * write reaches the RAM there), the matrix at $D000, under the I/O area. */
#ifndef WARP64U_PREVIEW_H
#define WARP64U_PREVIEW_H

#include "store.h"

/* Downloads and shows the preview of shelf entry +index+; returns a STORE_
 * code. The screen is left in bitmap mode only while it is shown; the
 * caller redraws the text screen afterwards. */
int preview_show(Store *store, unsigned char index);

#endif
