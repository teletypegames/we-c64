#include <string.h>
#include "shelf.h"
#include "text.h"

void shelf_clear(Shelf *shelf)
{
    shelf->count = 0;
    shelf->skipped = 0;
    shelf->line_length = 0;
}

static void take_line(Shelf *shelf)
{
    shelf->line[shelf->line_length] = 0;
    if (shelf->line_length == 0 || shelf->line[0] == ASCII_HASH) return;
    if (shelf->count < SHELF_MAX && shelf_parse_line(shelf->line, &shelf->entries[shelf->count]))
        ++shelf->count;
    else
        ++shelf->skipped;
}

void shelf_feed(Shelf *shelf, const unsigned char *data, unsigned int length)
{
    unsigned char c;
    while (length--) {
        c = *data++;
        if (c == ASCII_LF) {
            take_line(shelf);
            shelf->line_length = 0;
        } else if (c != ASCII_CR && shelf->line_length < SHELF_LINE_MAX) {
            shelf->line[shelf->line_length++] = (char)c;
        }
    }
}

void shelf_finish(Shelf *shelf)
{
    if (shelf->line_length > 0) take_line(shelf);
    shelf->line_length = 0;
}

static const char *field(const char *p, char *out, unsigned int max)
{
    unsigned int n = 0;
    while (*p && *p != ASCII_BAR) {
        if (n < max) out[n++] = *p;
        ++p;
    }
    out[n] = 0;
    return *p == ASCII_BAR ? p + 1 : p;
}

static int parse_number(const char *text, uint32_t *value, unsigned char base)
{
    uint32_t v = 0;
    unsigned char digit;
    char c;
    if (!*text) return 0;
    while ((c = *text++) != 0) {
        if (ASCII_IS_DIGIT(c)) digit = (unsigned char)(c - 0x30);
        else if (base == 16 && c >= 0x61 && c <= 0x66) digit = (unsigned char)(c - 0x61 + 10);
        else if (base == 16 && c >= 0x41 && c <= 0x46) digit = (unsigned char)(c - 0x41 + 10);
        else return 0;
        v = v * base + digit;
    }
    *value = v;
    return 1;
}

static int valid_file(const char *file)
{
    char c;
    if (!*file) return 0;
    while ((c = *file++) != 0) {
        if (!(ASCII_IS_DIGIT(c) || (c >= 0x41 && c <= 0x5A) || c == 0x2D || c == ASCII_DOT)) return 0;
    }
    return 1;
}

static int parse(const char *line, ShelfEntry *entry, unsigned char strict)
{
    char number[12];
    const char *p = line;
    char preview[2];
    p = field(p, entry->file, SHELF_FILE_MAX);
    p = field(p, entry->version, SHELF_VERSION_MAX);
    p = field(p, number, sizeof number - 1);
    if (!parse_number(number, &entry->size, 10)) return 0;
    p = field(p, number, sizeof number - 1);
    if (!parse_number(number, &entry->crc32, 16)) {
        if (strict) return 0;
        entry->crc32 = 0;
    }
    p = field(p, entry->title, SHELF_TITLE_MAX);
    p = field(p, entry->author, SHELF_AUTHOR_MAX);
    p = field(p, entry->description, SHELF_DESC_MAX);
    p = field(p, entry->category, SHELF_CATEGORY_MAX);
    field(p, preview, 1);
    entry->preview = preview[0] != 0;
    if (!valid_file(entry->file) || !entry->version[0]) return 0;
    if (!entry->title[0]) strcpy(entry->title, entry->file);
    return 1;
}

int shelf_parse_line(const char *line, ShelfEntry *entry)
{
    return parse(line, entry, 1);
}

int shelf_parse_local_line(const char *line, ShelfEntry *entry)
{
    return parse(line, entry, 0);
}

static const unsigned char hex_digits[16] = {
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46
};

static char *put_text(char *p, const char *text)
{
    while (*text) *p++ = *text++;
    *p++ = ASCII_BAR;
    return p;
}

static char *put_number(char *p, uint32_t value, unsigned char base)
{
    char digits[11];
    unsigned char n = 0, d;
    unsigned char width = base == 16 ? 8 : 1;
    do {
        d = (unsigned char)(value % base);
        digits[n++] = (char)hex_digits[d];
        value /= base;
    } while (value || n < width);
    while (n) *p++ = digits[--n];
    *p++ = ASCII_BAR;
    return p;
}

void shelf_format_line(const ShelfEntry *entry, char *line)
{
    char *p = line;
    p = put_text(p, entry->file);
    p = put_text(p, entry->version);
    p = put_number(p, entry->size, 10);
    p = put_number(p, entry->crc32, 16);
    p = put_text(p, entry->title);
    p = put_text(p, entry->author);
    p = put_text(p, entry->description);
    p[-1] = ASCII_LF;
    *p = 0;
}

int shelf_find(const Shelf *shelf, const char *file)
{
    unsigned char i;
    for (i = 0; i < shelf->count; ++i)
        if (strcmp(shelf->entries[i].file, file) == 0) return i;
    return -1;
}

int shelf_put(Shelf *shelf, const ShelfEntry *entry)
{
    int i = shelf_find(shelf, entry->file);
    if (i < 0) {
        if (shelf->count >= SHELF_MAX) return 0;
        i = shelf->count++;
    }
    shelf->entries[i] = *entry;
    return 1;
}

void shelf_remove(Shelf *shelf, const char *file)
{
    int i = shelf_find(shelf, file);
    if (i < 0) return;
    --shelf->count;
    for (; i < shelf->count; ++i) shelf->entries[i] = shelf->entries[i + 1];
}

void shelf_disk_name(const char *file, char *name)
{
    unsigned char n = 0;
    while (*file && *file != ASCII_DOT && n < SHELF_FILE_MAX) name[n++] = *file++;
    name[n] = 0;
}
