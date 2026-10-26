/* One shelf: the titles of INDEX.TXT, or of the local WARP64U.IDX.
 *
 *   #SHELF 1 file|version|size|crc32|title|author|description
 *   ARIADNE.PRG|1.1.0|10561|e6d8fe00|Thread of Ariadne|Teletype Games|...
 *
 * A line is read a byte at a time, so the index streams straight from the
 * network into the table; a line that does not parse is left out. */
#ifndef WARP64U_SHELF_H
#define WARP64U_SHELF_H

#include <stdint.h>

#define SHELF_MAX 40
#define SHELF_FILE_MAX 16
#define SHELF_VERSION_MAX 15
#define SHELF_TITLE_MAX 36
#define SHELF_DESC_MAX 100
#define SHELF_LINE_MAX 256

typedef struct {
    char file[SHELF_FILE_MAX + 1];
    char version[SHELF_VERSION_MAX + 1];
    uint32_t size;
    uint32_t crc32;
    char title[SHELF_TITLE_MAX + 1];
    char description[SHELF_DESC_MAX + 1];
} ShelfEntry;

typedef struct {
    ShelfEntry entries[SHELF_MAX];
    unsigned char count;
    unsigned char skipped;
    char line[SHELF_LINE_MAX + 1];
    unsigned int line_length;
} Shelf;

void shelf_clear(Shelf *shelf);
void shelf_feed(Shelf *shelf, const unsigned char *data, unsigned int length);
void shelf_finish(Shelf *shelf);
int shelf_parse_line(const char *line, ShelfEntry *entry);
void shelf_format_line(const ShelfEntry *entry, char *line);
int shelf_find(const Shelf *shelf, const char *file);
int shelf_put(Shelf *shelf, const ShelfEntry *entry);
void shelf_remove(Shelf *shelf, const char *file);
void shelf_disk_name(const char *file, char *name);

#endif
