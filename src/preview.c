#include <conio.h>
#include <string.h>
#include "preview.h"
#include "text.h"

#define HEAD 3
#define CELLS 1000
#define BITMAP 8000

#define TEXT_SCREEN ((unsigned char *)0x0400)
#define CELL_COLORS ((unsigned char *)0xD800)
#define BITMAP_RAM ((unsigned char *)0xE000)
#define MATRIX_RAM ((unsigned char *)0xD000)
#define SAFE_NMI 0xFF40

#define VIC_CONTROL1 (*(unsigned char *)0xD011)
#define VIC_CONTROL2 (*(unsigned char *)0xD016)
#define VIC_MEMORY (*(unsigned char *)0xD018)
#define VIC_BORDER (*(unsigned char *)0xD020)
#define VIC_BACKGROUND (*(unsigned char *)0xD021)
#define CIA2_BANK (*(unsigned char *)0xDD00)
#define CPU_PORT (*(unsigned char *)0x0001)

static unsigned char head[HEAD];
static unsigned int at;

/* The matrix waits on the blanked text screen, the color RAM and the bitmap
 * go to their places at once. */
static int sink(const unsigned char *data, unsigned int length, void *context)
{
    unsigned int n;
    unsigned char c;
    (void)context;
    while (length--) {
        c = *data++;
        n = at++;
        if (n < HEAD) {
            head[n] = c;
            continue;
        }
        n -= HEAD;
        if (n < CELLS) {
            TEXT_SCREEN[n] = c;
            continue;
        }
        n -= CELLS;
        if (head[0]) {
            if (n < CELLS) {
                CELL_COLORS[n] = c;
                continue;
            }
            n -= CELLS;
        }
        if (n >= BITMAP) return 1;
        BITMAP_RAM[n] = c;
    }
    return 0;
}

static int tick(uint32_t received, uint32_t total)
{
    (void)total;
    VIC_BORDER = (unsigned char)(received >> 9);
    return kbhit() && cgetc() == 0x03;
}

/* Moves the matrix under the I/O area, with the I/O and the ROMs switched
 * out for the copy; an NMI meanwhile finds an RTI through the RAM vector. */
static void place_matrix(void)
{
    unsigned char port;
    *(unsigned char *)SAFE_NMI = 0x40;
    *(unsigned char *)0xFFFA = (unsigned char)SAFE_NMI;
    *(unsigned char *)0xFFFB = (unsigned char)(SAFE_NMI >> 8);
    asm("sei");
    port = CPU_PORT;
    CPU_PORT = (unsigned char)((port & 0xF8) | 0x04);
    memcpy(MATRIX_RAM, TEXT_SCREEN, CELLS);
    CPU_PORT = port;
    asm("cli");
}

static void text_mode(void)
{
    CIA2_BANK |= 0x03;
    VIC_MEMORY = 0x17;
    VIC_CONTROL2 = 0xC8;
    VIC_CONTROL1 = 0x1B;
    VIC_BORDER = COLOR_BLACK;
    VIC_BACKGROUND = COLOR_BLACK;
}

int preview_show(Store *store, unsigned char index)
{
    int result;
    unsigned int expected;

    at = 0;
    head[0] = 0;
    VIC_CONTROL1 &= 0xEF;
    result = store_preview(store, index, sink, 0, tick);
    expected = HEAD + CELLS + BITMAP + (head[0] ? CELLS : 0);
    if (result == STORE_OK && at != expected) {
        ascii_copy(store->message, "Preview broken");
        result = STORE_ERR_CHECK;
    }
    if (result != STORE_OK) {
        text_mode();
        return result;
    }

    place_matrix();
    VIC_BACKGROUND = head[1];
    VIC_BORDER = head[2];
    CIA2_BANK &= 0xFC;
    VIC_MEMORY = 0x48;
    VIC_CONTROL2 = head[0] ? 0xD8 : 0xC8;
    VIC_CONTROL1 = 0x3B;
    while (kbhit()) cgetc();
    cgetc();
    text_mode();
    return STORE_OK;
}
