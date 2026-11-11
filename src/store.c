#include <string.h>
#include "crc32.h"
#include "disk.h"
#include "store.h"
#include "text.h"

#define CHUNK 128

static unsigned char chunk[CHUNK];

/* One line, one entry, one URL and one HTTP result serve every function
 * here: none of them runs inside another. */
static char line[SHELF_LINE_MAX + 1];
static ShelfEntry entry;
static char url[64];
static HttpResult result;

static void say(Store *store, const char *source_literal, const char *detail)
{
    ascii_copy(store->message, source_literal);
    if (detail) strncat(store->message, detail, sizeof store->message - strlen(store->message) - 1);
}

static void index_name(char *name, const char *source_literal)
{
    ascii_copy(name, source_literal);
}

void store_init(Store *store)
{
    memset(store, 0, sizeof *store);
    config_defaults(&store->config);
    crc32_init();
}

int store_local_find(const Store *store, const char *file)
{
    unsigned char i;
    for (i = 0; i < store->local_count; ++i)
        if (strcmp(store->local[i].file, file) == 0) return i;
    return -1;
}

static void local_put(Store *store, const ShelfEntry *entry)
{
    int i = store_local_find(store, entry->file);
    if (i < 0) {
        if (store->local_count >= LOCAL_MAX) return;
        i = store->local_count++;
    }
    strcpy(store->local[i].file, entry->file);
    strcpy(store->local[i].version, entry->version);
    store->local[i].size = entry->size;
    store->local[i].crc32 = entry->crc32;
}

static void local_remove(Store *store, const char *file)
{
    int i = store_local_find(store, file);
    if (i < 0) return;
    --store->local_count;
    for (; i < store->local_count; ++i) store->local[i] = store->local[i + 1];
}

int store_load_local(Store *store)
{
    char name[17];
    unsigned int length = 0;
    int n, i;
    unsigned char c;

    unsigned char tries = 3;

    index_name(name, "WARP64U.IDX");
    while (disk_open_read(name, DISK_SEQ) != 0) {
        if (disk_status()[0] == 0x36 && disk_status()[1] == 0x32) {
            store->local_count = 0;
            return STORE_OK;
        }
        if (!--tries) {
            say(store, "Index: ", disk_status());
            return STORE_ERR_DISK;
        }
    }
    store->local_count = 0;
    while ((n = disk_read(chunk, CHUNK)) > 0) {
        for (i = 0; i < n; ++i) {
            c = chunk[i];
            if (c == ASCII_LF || c == ASCII_CR) {
                line[length] = 0;
                if (length && shelf_parse_line(line, &entry)) local_put(store, &entry);
                length = 0;
            } else if (length < SHELF_LINE_MAX) {
                line[length++] = (char)c;
            }
        }
    }
    disk_close_read();
    line[length] = 0;
    if (length && shelf_parse_line(line, &entry)) local_put(store, &entry);
    return STORE_OK;
}

static int save_local(Store *store)
{
    char next[17], name[17];
    unsigned char i;

    index_name(next, "WARP64U.IDN");
    index_name(name, "WARP64U.IDX");
    disk_scratch(next);
    if (disk_open_write(next, DISK_SEQ) != 0) return STORE_ERR_DISK;
    memset(&entry, 0, sizeof entry);
    for (i = 0; i < store->local_count; ++i) {
        strcpy(entry.file, store->local[i].file);
        strcpy(entry.version, store->local[i].version);
        entry.size = store->local[i].size;
        entry.crc32 = store->local[i].crc32;
        shelf_format_line(&entry, line);
        if (disk_write((const unsigned char *)line, (unsigned int)strlen(line)) != 0) {
            disk_close_write();
            return STORE_ERR_DISK;
        }
    }
    if (disk_close_write() != 0) return STORE_ERR_DISK;
    disk_scratch(name);
    return disk_rename(next, name) == 0 ? STORE_OK : STORE_ERR_DISK;
}

static int shelf_sink(const unsigned char *data, unsigned int length, void *context)
{
    shelf_feed((Shelf *)context, data, length);
    return 0;
}

static void url_of(const Store *store, const char *file, char *url)
{
    strcpy(url, store->config.path);
    url[strlen(url) + 1] = 0;
    url[strlen(url)] = 0x2F;
    strcat(url, file);
}

int store_fetch(Store *store, HttpTick tick)
{
    char index[10];
    int error;

    index_name(index, "INDEX.TXT");
    url_of(store, index, url);
    shelf_clear(&store->remote);
    error = http_get(store->net, store->config.host, store->config.port, url, shelf_sink, &store->remote, tick, &result);
    shelf_finish(&store->remote);
    if (error == HTTP_ERR_ABORTED) return STORE_ERR_ABORTED;
    if (error != HTTP_OK) {
        say(store, "Index: ", 0);
        if (error == HTTP_ERR_CONNECT) strcat(store->message, store->net->error());
        else if (error == HTTP_ERR_STATUS) {
            ascii_copy(index, "HTTP ");
            strcat(store->message, index);
            index[0] = (char)(0x30 + result.status / 100);
            index[1] = (char)(0x30 + result.status / 10 % 10);
            index[2] = (char)(0x30 + result.status % 10);
            index[3] = 0;
            strcat(store->message, index);
        } else {
            ascii_copy(index, "broken");
            strcat(store->message, index);
        }
        return STORE_ERR_NETWORK;
    }
    return STORE_OK;
}

unsigned char store_state(const Store *store, const char *file, const char *remote_version)
{
    int i = store_local_find(store, file);
    if (i < 0) return STATE_AVAILABLE;
    if (!remote_version) return STATE_LOCAL_ONLY;
    return strcmp(store->local[i].version, remote_version) == 0 ? STATE_INSTALLED : STATE_UPDATE;
}

typedef struct {
    uint32_t crc;
    uint32_t size;
    unsigned char failed;
} Download;

static int disk_sink(const unsigned char *data, unsigned int length, void *context)
{
    Download *download = (Download *)context;
    download->crc = crc32_update(download->crc, data, length);
    download->size += length;
    if (disk_write(data, length) != 0) {
        download->failed = 1;
        return 1;
    }
    return 0;
}

int store_install(Store *store, unsigned char index, HttpTick tick)
{
    const ShelfEntry *wanted = &store->remote.entries[index];
    static char name[SHELF_FILE_MAX + 1], temporary[SHELF_FILE_MAX + 5], suffix[5];
    static Download download;
    int error;

    shelf_disk_name(wanted->file, name);
    strcpy(temporary, name);
    ascii_copy(suffix, ".TMP");
    strcat(temporary, suffix);
    url_of(store, wanted->file, url);

    disk_scratch(temporary);
    if (disk_open_write(temporary, DISK_PRG) != 0) {
        say(store, "Disk: ", disk_status());
        return STORE_ERR_DISK;
    }
    download.crc = CRC32_START;
    download.size = 0;
    download.failed = 0;
    error = http_get(store->net, store->config.host, store->config.port, url, disk_sink, &download, tick, &result);
    if (disk_close_write() != 0) download.failed = 1;

    if (error != HTTP_OK || download.failed || download.size != wanted->size || CRC32_FINISH(download.crc) != wanted->crc32) {
        disk_scratch(temporary);
        if (error == HTTP_ERR_ABORTED) {
            say(store, "Stopped", 0);
            return STORE_ERR_ABORTED;
        }
        if (download.failed) {
            say(store, "Disk: ", disk_status());
            return STORE_ERR_DISK;
        }
        if (error != HTTP_OK) {
            say(store, "Download failed", 0);
            return STORE_ERR_NETWORK;
        }
        say(store, "Check failed: ", wanted->file);
        return STORE_ERR_CHECK;
    }

    disk_scratch(name);
    if (disk_rename(temporary, name) != 0) {
        say(store, "Disk: ", disk_status());
        return STORE_ERR_DISK;
    }
    local_put(store, wanted);
    if (save_local(store) != STORE_OK) {
        say(store, "Disk: ", disk_status());
        return STORE_ERR_DISK;
    }
    say(store, "Installed ", wanted->file);
    return STORE_OK;
}

int store_preview(Store *store, unsigned char index, HttpSink sink, void *context, HttpTick tick)
{
    static char name[SHELF_FILE_MAX + 5], suffix[5];
    int error;

    shelf_disk_name(store->remote.entries[index].file, name);
    ascii_copy(suffix, ".PVW");
    strcat(name, suffix);
    url_of(store, name, url);
    error = http_get(store->net, store->config.host, store->config.port, url, sink, context, tick, &result);
    if (error == HTTP_OK) return STORE_OK;
    if (error == HTTP_ERR_ABORTED) {
        say(store, "Stopped", 0);
        return STORE_ERR_ABORTED;
    }
    say(store, "Preview failed: ", name);
    return STORE_ERR_NETWORK;
}

int store_remove(Store *store, const char *file_in_list)
{
    char name[SHELF_FILE_MAX + 1], file[SHELF_FILE_MAX + 1];
    strcpy(file, file_in_list);
    shelf_disk_name(file, name);
    disk_scratch(name);
    local_remove(store, file);
    if (save_local(store) != STORE_OK) {
        say(store, "Disk: ", disk_status());
        return STORE_ERR_DISK;
    }
    say(store, "Removed ", file);
    return STORE_OK;
}
