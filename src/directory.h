/* One directory: the titles of INDEX.TXT, or of the local WE64.IDX.
 *
 *   #SHELF 2 file|version|size|crc32|title|author|description|category|preview
 *   ARIADNE.PRG|1.1.0|10561|e6d8fe00|Thread of Ariadne|Teletype Games|...|game|ARIADNE.PVW
 *
 * Version 1 lines end after the description; the category and the preview
 * name are then empty. A non-empty preview field means NAME.PVW is in the
 * directory beside NAME.PRG. (`#SHELF` is the header the engine writes; a
 * line starting with `#` is skipped, so its name does not matter here.)
 *
 * A line is read a byte at a time, so the index streams straight from the
 * network into the table; a line that does not parse is left out. */
#ifndef WE64_DIRECTORY_H
#define WE64_DIRECTORY_H

#include <stdint.h>

#define DIRECTORY_MAX 24
#define DIRECTORY_FILE_MAX 16
#define DIRECTORY_VERSION_MAX 15
#define DIRECTORY_TITLE_MAX 32
#define DIRECTORY_AUTHOR_MAX 16
#define DIRECTORY_DESC_MAX 100
#define DIRECTORY_CATEGORY_MAX 8
#define DIRECTORY_LINE_MAX 256

typedef struct {
    char file[DIRECTORY_FILE_MAX + 1];
    char version[DIRECTORY_VERSION_MAX + 1];
    uint32_t size;
    uint32_t crc32;
    char title[DIRECTORY_TITLE_MAX + 1];
    char author[DIRECTORY_AUTHOR_MAX + 1];
    char description[DIRECTORY_DESC_MAX + 1];
    char category[DIRECTORY_CATEGORY_MAX + 1];
    unsigned char preview;
} DirectoryEntry;

typedef struct {
    DirectoryEntry entries[DIRECTORY_MAX];
    unsigned char count;
    unsigned char skipped;
    char line[DIRECTORY_LINE_MAX + 1];
    unsigned int line_length;
} Directory;

void directory_clear(Directory *dir);
void directory_feed(Directory *dir, const unsigned char *data, unsigned int length);
void directory_finish(Directory *dir);
int directory_parse_line(const char *line, DirectoryEntry *entry);
/* The same for a line of WE64.IDX, where a CRC-32 that does not parse
 * is read as 0 (1.0.1 and earlier wrote the hex letters of a CRC-32 wrong on
 * the C64); the local list needs the name and the version only. */
int directory_parse_local_line(const char *line, DirectoryEntry *entry);
void directory_format_line(const DirectoryEntry *entry, char *line);
int directory_find(const Directory *dir, const char *file);
int directory_put(Directory *dir, const DirectoryEntry *entry);
void directory_remove(Directory *dir, const char *file);
void directory_disk_name(const char *file, char *name);

#endif
