/* The store: the shelf on the server, what is installed here, and the three
 * things to do with a title.
 *
 * Installing writes the download to NAME.TMP, checks its size and CRC-32
 * against the index, and only then scratches NAME and renames NAME.TMP to it:
 * a broken or interrupted download leaves the installed version as it was.
 * WARP64U.IDX, the list of what is installed here, is written last. */
#ifndef WARP64U_STORE_H
#define WARP64U_STORE_H

#include "config.h"
#include "http.h"
#include "net.h"
#include "shelf.h"

#define LOCAL_MAX 24

typedef struct {
    char file[SHELF_FILE_MAX + 1];
    char version[SHELF_VERSION_MAX + 1];
    uint32_t size;
    uint32_t crc32;
} LocalEntry;

typedef struct {
    Config config;
    const NetDriver *net;
    Shelf remote;
    LocalEntry local[LOCAL_MAX];
    unsigned char local_count;
    char message[48];
} Store;

#define STATE_AVAILABLE 0
#define STATE_INSTALLED 1
#define STATE_UPDATE 2
#define STATE_LOCAL_ONLY 3

#define STORE_OK 0
#define STORE_ERR_NETWORK 1
#define STORE_ERR_DISK 2
#define STORE_ERR_CHECK 3
#define STORE_ERR_FULL 4
#define STORE_ERR_ABORTED 5

void store_init(Store *store);
int store_load_local(Store *store);
int store_fetch(Store *store, HttpTick tick);
unsigned char store_state(const Store *store, const char *file, const char *remote_version);
int store_local_find(const Store *store, const char *file);
int store_install(Store *store, unsigned char index, HttpTick tick);
int store_remove(Store *store, const char *file);
int store_preview(Store *store, unsigned char index, HttpSink sink, void *context, HttpTick tick);

#endif
