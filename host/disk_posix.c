#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/disk.h"

static FILE *writing;
static FILE *reading;
static char status[48] = "00, OK,00,00";

static void path_of(const char *name, char *path)
{
    const char *dir = getenv("DISK_DIR");
    snprintf(path, 512, "%s/%s", dir ? dir : ".", name);
}

int disk_open_write(const char *name, unsigned char type)
{
    char path[512];
    (void)type;
    path_of(name, path);
    writing = fopen(path, "wb");
    if (!writing) snprintf(status, sizeof status, "26,WRITE PROTECT ON,00,00");
    return writing ? 0 : -1;
}

int disk_write(const unsigned char *data, unsigned int length)
{
    const char *full = getenv("DISK_FULL_AFTER");
    if (full && ftell(writing) + (long)length > atol(full)) {
        snprintf(status, sizeof status, "72,DISK FULL,00,00");
        return -1;
    }
    return fwrite(data, 1, length, writing) == length ? 0 : -1;
}

int disk_close_write(void)
{
    int result = fclose(writing);
    writing = 0;
    return result == 0 ? 0 : -1;
}

int disk_open_read(const char *name, unsigned char type)
{
    char path[512];
    (void)type;
    path_of(name, path);
    reading = fopen(path, "rb");
    return reading ? 0 : -1;
}

int disk_read(unsigned char *buffer, unsigned int max)
{
    return (int)fread(buffer, 1, max, reading);
}

void disk_close_read(void)
{
    fclose(reading);
    reading = 0;
}

int disk_scratch(const char *name)
{
    char path[512];
    path_of(name, path);
    remove(path);
    return 0;
}

int disk_rename(const char *from, const char *to)
{
    char a[512], b[512];
    path_of(from, a);
    path_of(to, b);
    return rename(a, b) == 0 ? 0 : -1;
}

const char *disk_status(void) { return status; }

void disk_use(unsigned char device) { (void)device; }

void disk_finish(void) {}

int disk_cd(const char *dir) { (void)dir; return 0; }
