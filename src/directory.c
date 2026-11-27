#include <string.h>
#include "directory.h"
#include "text.h"

void directory_clear(Directory *dir)
{
    dir->count = 0;
    dir->skipped = 0;
    dir->line_length = 0;
}

static void take_line(Directory *dir)
{
    dir->line[dir->line_length] = 0;
    if (dir->line_length == 0 || dir->line[0] == ASCII_HASH) return;
    if (dir->count < DIRECTORY_MAX && directory_parse_line(dir->line, &dir->entries[dir->count]))
        ++dir->count;
    else
        ++dir->skipped;
}

void directory_feed(Directory *dir, const unsigned char *data, unsigned int length)
{
    unsigned char c;
    while (length--) {
        c = *data++;
        if (c == ASCII_LF) {
            take_line(dir);
            dir->line_length = 0;
        } else if (c != ASCII_CR && dir->line_length < DIRECTORY_LINE_MAX) {
            dir->line[dir->line_length++] = (char)c;
        }
    }
}

void directory_finish(Directory *dir)
{
    if (dir->line_length > 0) take_line(dir);
    dir->line_length = 0;
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

static int parse(const char *line, DirectoryEntry *entry, unsigned char strict)
{
    char number[12];
    const char *p = line;
    char preview[2];
    p = field(p, entry->file, DIRECTORY_FILE_MAX);
    p = field(p, entry->version, DIRECTORY_VERSION_MAX);
    p = field(p, number, sizeof number - 1);
    if (!parse_number(number, &entry->size, 10)) return 0;
    p = field(p, number, sizeof number - 1);
    if (!parse_number(number, &entry->crc32, 16)) {
        if (strict) return 0;
        entry->crc32 = 0;
    }
    p = field(p, entry->title, DIRECTORY_TITLE_MAX);
    p = field(p, entry->author, DIRECTORY_AUTHOR_MAX);
    p = field(p, entry->description, DIRECTORY_DESC_MAX);
    p = field(p, entry->category, DIRECTORY_CATEGORY_MAX);
    field(p, preview, 1);
    entry->preview = preview[0] != 0;
    if (!valid_file(entry->file) || !entry->version[0]) return 0;
    if (!entry->title[0]) strcpy(entry->title, entry->file);
    return 1;
}

int directory_parse_line(const char *line, DirectoryEntry *entry)
{
    return parse(line, entry, 1);
}

int directory_parse_local_line(const char *line, DirectoryEntry *entry)
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

void directory_format_line(const DirectoryEntry *entry, char *line)
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

int directory_find(const Directory *dir, const char *file)
{
    unsigned char i;
    for (i = 0; i < dir->count; ++i)
        if (strcmp(dir->entries[i].file, file) == 0) return i;
    return -1;
}

int directory_put(Directory *dir, const DirectoryEntry *entry)
{
    int i = directory_find(dir, entry->file);
    if (i < 0) {
        if (dir->count >= DIRECTORY_MAX) return 0;
        i = dir->count++;
    }
    dir->entries[i] = *entry;
    return 1;
}

void directory_remove(Directory *dir, const char *file)
{
    int i = directory_find(dir, file);
    if (i < 0) return;
    --dir->count;
    for (; i < dir->count; ++i) dir->entries[i] = dir->entries[i + 1];
}

void directory_disk_name(const char *file, char *name)
{
    unsigned char n = 0;
    while (*file && *file != ASCII_DOT && n < DIRECTORY_FILE_MAX) name[n++] = *file++;
    name[n] = 0;
}
