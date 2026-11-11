/* The store against a real HTTP server and a folder for a disk.
 *
 *   SERVER_ROOT=<served folder> DISK_DIR=<disk folder> PORT=<port> ./test_store
 *
 * Every scenario writes the shelf it needs into SERVER_ROOT/C64 first. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "../src/config.h"
#include "../src/crc32.h"
#include "../src/disk.h"
#include "../src/http.h"
#include "../src/shelf.h"
#include "../src/store.h"

extern const NetDriver net_posix;

static int failures;
static Store store;

#define CHECK(cond, what) do { if (cond) printf("  ok    %s\n", what); else { printf("  FAIL  %s\n", what); ++failures; } } while (0)

static void path_in(const char *root_env, const char *name, char *path)
{
    snprintf(path, 512, "%s/%s", getenv(root_env), name);
}

static void write_file(const char *root_env, const char *name, const char *data, size_t length)
{
    char path[512];
    FILE *f;
    path_in(root_env, name, path);
    f = fopen(path, "wb");
    fwrite(data, 1, length, f);
    fclose(f);
}

static long file_size(const char *root_env, const char *name)
{
    char path[512];
    struct stat st;
    path_in(root_env, name, path);
    return stat(path, &st) == 0 ? (long)st.st_size : -1;
}

static int file_equals(const char *root_env, const char *name, const char *data, size_t length)
{
    char path[512], buffer[4096];
    FILE *f;
    size_t n;
    path_in(root_env, name, path);
    f = fopen(path, "rb");
    if (!f) return 0;
    n = fread(buffer, 1, sizeof buffer, f);
    fclose(f);
    return n == length && memcmp(buffer, data, length) == 0;
}

static unsigned long crc_of(const char *data, size_t length)
{
    return (unsigned long)CRC32_FINISH(crc32_update(CRC32_START, (const unsigned char *)data, (unsigned int)length));
}

static char index_text[4096];

static void shelf_begin(void)
{
    strcpy(index_text, "#SHELF 1 file|version|size|crc32|title|author|description\n");
}

static void shelf_add(const char *file, const char *version, const char *data, size_t length, const char *title)
{
    char line[512], name[64];
    snprintf(line, sizeof line, "%s|%s|%zu|%08lx|%s|Teletype Games|A test title called %s.\n",
             file, version, length, crc_of(data, length), title, title);
    strcat(index_text, line);
    snprintf(name, sizeof name, "C64/%s", file);
    write_file("SERVER_ROOT", name, data, length);
}

static void shelf_publish(void)
{
    write_file("SERVER_ROOT", "C64/INDEX.TXT", index_text, strlen(index_text));
}

static int remote_index(const char *file)
{
    return shelf_find(&store.remote, file);
}

static const char game1[] = "\x01\x08game one, version one";
static const char game1b[] = "\x01\x08game one, version two, longer";
static const char game2[] = "\x01\x08game two";

static void test_config(void)
{
    Config config;
    printf("config\n");
    config_defaults(&config);
    CHECK(strcmp(config.host, "archive.teletypegames.org") == 0 && config.port == 80 && strcmp(config.path, "/C64") == 0,
          "defaults: archive.teletypegames.org:80/C64");
    {
        const char *text = "host=Example.ORG\r\nPORT=8080\rdrive=9\nDIR=GAMES\nNET=IP65\nbogus\n";
        config_feed(&config, (const unsigned char *)text, (unsigned int)strlen(text));
    }
    CHECK(strcmp(config.host, "example.org") == 0 && config.port == 8080 && config.drive == 9, "keys in any case, CR or LF");
    CHECK(strcmp(config.dir, "GAMES") == 0 && strcmp(config.net, "ip65") == 0, "DIR as typed, NET lower case");
}

static void test_shelf_lines(void)
{
    ShelfEntry entry;
    char line[300];
    printf("shelf lines\n");
    CHECK(shelf_parse_line("ARIADNE.PRG|1.1.0|10561|e6d8fe00|Thread of Ariadne|Teletype Games|Co-op.", &entry) &&
          entry.size == 10561 && entry.crc32 == 0xe6d8fe00UL && strcmp(entry.title, "Thread of Ariadne") == 0,
          "a line of INDEX.TXT");
    CHECK(entry.category[0] == 0 && !entry.preview && strcmp(entry.author, "Teletype Games") == 0,
          "a version 1 line: the author, no category, no preview");
    CHECK(shelf_parse_line("TRICKSTER-TI.PRG|1.0.0|6593|c4e6bb9f|Trickster Tiles C64|Teletype Games|Hop.|tool|TRICKSTER-TI.PVW",
                           &entry) && strcmp(entry.description, "Hop.") == 0 && strcmp(entry.category, "tool") == 0 && entry.preview,
          "a version 2 line: the category and the preview");
    CHECK(shelf_parse_line("A.PRG|1|1|00000000|x|y|z||", &entry) && entry.category[0] == 0 && !entry.preview,
          "a version 2 line without category and preview");
    CHECK(shelf_parse_line("A.PRG|1|1|00000000|A title of more than thirty-two characters|y|z|game|", &entry) &&
          strlen(entry.title) == SHELF_TITLE_MAX, "a long title is cut");
    CHECK(!shelf_parse_line("ariadne.prg|1|1|00000000|x|y|z", &entry), "a lower-case file name is refused");
    CHECK(!shelf_parse_line("A.PRG|1|ten|00000000|x|y|z", &entry), "a size that is not a number is refused");
    strcpy(entry.file, "C64-DEMO.PRG");
    strcpy(entry.version, "1.0.0");
    entry.size = 4641;
    entry.crc32 = 0xaf681ffcUL;
    strcpy(entry.title, "Demo");
    strcpy(entry.author, "");
    strcpy(entry.description, "");
    shelf_format_line(&entry, line);
    CHECK(strcmp(line, "C64-DEMO.PRG|1.0.0|4641|AF681FFC|Demo||\n") == 0,
          "a local line round-trips");
    entry.crc32 = 0xb759e215UL;
    shelf_format_line(&entry, line);
    CHECK(strstr(line, "|B759E215|") != NULL, "a CRC-32 with letters is written in hex capitals");
    CHECK(!shelf_parse_line("B.PRG|2.2.0|1|l759l215|||", &entry), "a broken CRC-32 is refused on the shelf");
    CHECK(shelf_parse_local_line("B.PRG|2.2.0|1|l759l215|||", &entry) && strcmp(entry.version, "2.2.0") == 0,
          "and read in the local index, the name and version kept");
    shelf_disk_name("C64-DEMO.PRG", line);
    CHECK(strcmp(line, "C64-DEMO") == 0, "the disk name drops .PRG");
}

static void test_store(void)
{
    int i;
    printf("store\n");
    store_init(&store);
    store.net = &net_posix;
    strcpy(store.config.host, "127.0.0.1");
    store.config.port = (unsigned int)atoi(getenv("PORT"));

    shelf_begin();
    shelf_add("GAMEONE.PRG", "1.0.0", game1, sizeof game1 - 1, "Game One");
    shelf_add("GAMETWO.PRG", "2.0.0", game2, sizeof game2 - 1, "Game Two");
    strcat(index_text, "BROKEN LINE\n");
    shelf_publish();

    CHECK(store_fetch(&store, 0) == STORE_OK, "the index is fetched");
    CHECK(store.remote.count == 2 && store.remote.skipped == 1, "two titles, the broken line skipped");
    CHECK(store_load_local(&store) == STORE_OK && store.local_count == 0, "nothing installed yet");

    i = remote_index("GAMEONE.PRG");
    CHECK(store_install(&store, (unsigned char)i, 0) == STORE_OK, "install GAMEONE");
    CHECK(file_equals("DISK_DIR", "GAMEONE", game1, sizeof game1 - 1), "GAMEONE on the disk, byte for byte");
    CHECK(file_size("DISK_DIR", "GAMEONE.TMP") < 0, "no GAMEONE.TMP left");
    CHECK(store_state(&store, "GAMEONE.PRG", "1.0.0") == STATE_INSTALLED, "installed");

    store_init(&store);
    store.net = &net_posix;
    strcpy(store.config.host, "127.0.0.1");
    store.config.port = (unsigned int)atoi(getenv("PORT"));
    CHECK(store_load_local(&store) == STORE_OK && store.local_count == 1 && strcmp(store.local[0].version, "1.0.0") == 0,
          "WARP64U.IDX remembers it");

    shelf_begin();
    shelf_add("GAMEONE.PRG", "1.1.0", game1b, sizeof game1b - 1, "Game One");
    shelf_add("GAMETWO.PRG", "2.0.0", game2, sizeof game2 - 1, "Game Two");
    shelf_publish();
    store_fetch(&store, 0);
    CHECK(store_state(&store, "GAMEONE.PRG", "1.1.0") == STATE_UPDATE, "a new version is an update");
    CHECK(store_install(&store, (unsigned char)remote_index("GAMEONE.PRG"), 0) == STORE_OK &&
          file_equals("DISK_DIR", "GAMEONE", game1b, sizeof game1b - 1), "the update replaces it");

    shelf_begin();
    shelf_add("GAMEONE.PRG", "1.2.0", game1b, sizeof game1b - 1, "Game One");
    shelf_publish();
    write_file("SERVER_ROOT", "C64/GAMEONE.PRG", game1, sizeof game1 - 1);
    store_fetch(&store, 0);
    CHECK(store_install(&store, (unsigned char)remote_index("GAMEONE.PRG"), 0) == STORE_ERR_CHECK,
          "a file that does not match its line is refused");
    CHECK(file_equals("DISK_DIR", "GAMEONE", game1b, sizeof game1b - 1) && file_size("DISK_DIR", "GAMEONE.TMP") < 0,
          "and the installed version stays, no TMP left");
    CHECK(store_state(&store, "GAMEONE.PRG", "1.2.0") == STATE_UPDATE, "still offered as an update");

    shelf_begin();
    shelf_add("GAMEONE.PRG", "1.1.0", game1b, sizeof game1b - 1, "Game One");
    strcat(index_text, "MISSING.PRG|1.0.0|10|00000000|Missing|x|y\n");
    shelf_publish();
    store_fetch(&store, 0);
    CHECK(store_install(&store, (unsigned char)remote_index("MISSING.PRG"), 0) == STORE_ERR_NETWORK &&
          file_size("DISK_DIR", "MISSING") < 0 && file_size("DISK_DIR", "MISSING.TMP") < 0,
          "a 404 installs nothing");

    setenv("DISK_FULL_AFTER", "4", 1);
    shelf_begin();
    shelf_add("GAMETWO.PRG", "2.0.0", game2, sizeof game2 - 1, "Game Two");
    shelf_publish();
    store_fetch(&store, 0);
    CHECK(store_install(&store, (unsigned char)remote_index("GAMETWO.PRG"), 0) == STORE_ERR_DISK &&
          file_size("DISK_DIR", "GAMETWO") < 0 && strstr(store.message, "DISK FULL") != 0,
          "a full disk installs nothing and says so");
    unsetenv("DISK_FULL_AFTER");

    CHECK(store_state(&store, "GAMEONE.PRG", 0) == STATE_LOCAL_ONLY, "a title gone from the server is local only");
    CHECK(store_remove(&store, "GAMEONE.PRG") == STORE_OK && file_size("DISK_DIR", "GAMEONE") < 0 &&
          store_local_find(&store, "GAMEONE.PRG") < 0, "remove takes the file and the line");
    store_load_local(&store);
    CHECK(store.local_count == 0, "and WARP64U.IDX agrees");

    store.config.port = 1;
    CHECK(store_fetch(&store, 0) == STORE_ERR_NETWORK && strstr(store.message, "refused") != 0,
          "no server: the index says why");
}

static int nolength_sink(const unsigned char *data, unsigned int length, void *context)
{
    *(unsigned int *)context += length;
    (void)data;
    return 0;
}

typedef struct {
    char data[64];
    unsigned int length;
} Collected;

static int collect_sink(const unsigned char *data, unsigned int length, void *context)
{
    Collected *collected = (Collected *)context;
    if (collected->length + length > sizeof collected->data) return 1;
    memcpy(collected->data + collected->length, data, length);
    collected->length += length;
    return 0;
}

static void test_preview(void)
{
    static const char picture[] = "\x01\x00\x06pixels";
    Collected collected;
    printf("preview\n");
    store_init(&store);
    store.net = &net_posix;
    strcpy(store.config.host, "127.0.0.1");
    store.config.port = (unsigned int)atoi(getenv("PORT"));
    shelf_begin();
    shelf_add("GAMEONE.PRG", "1.0.0", game1, sizeof game1 - 1, "Game One");
    shelf_add("GAMETWO.PRG", "2.0.0", game2, sizeof game2 - 1, "Game Two");
    shelf_publish();
    write_file("SERVER_ROOT", "C64/GAMEONE.PVW", picture, sizeof picture - 1);
    store_fetch(&store, 0);
    memset(&collected, 0, sizeof collected);
    CHECK(store_preview(&store, (unsigned char)remote_index("GAMEONE.PRG"), collect_sink, &collected, 0) == STORE_OK &&
          collected.length == sizeof picture - 1 && memcmp(collected.data, picture, collected.length) == 0,
          "NAME.PVW beside NAME.PRG is fetched");
    memset(&collected, 0, sizeof collected);
    CHECK(store_preview(&store, (unsigned char)remote_index("GAMETWO.PRG"), collect_sink, &collected, 0) == STORE_ERR_NETWORK,
          "a missing preview is an error");
}

static void test_http(void)
{
    HttpResult result;
    unsigned int got = 0;
    printf("http\n");
    CHECK(http_get(&net_posix, "127.0.0.1", (unsigned int)atoi(getenv("PORT")), "/C64/NOLENGTH.PRG",
                   nolength_sink, &got, 0, &result) == HTTP_OK && got == 10 && result.length == 10,
          "a body without Content-Length ends with the connection");
    got = 0;
    CHECK(http_get(&net_posix, "127.0.0.1", (unsigned int)atoi(getenv("PORT")), "/C64/LEADING.PRG",
                   nolength_sink, &got, 0, &result) == HTTP_OK && got == 4 && result.status == 200,
          "an empty line before the status line is skipped");
    got = 0;
    CHECK(http_get(&net_posix, "127.0.0.1", (unsigned int)atoi(getenv("PORT")), "/C64/TRAILING.PRG",
                   nolength_sink, &got, 0, &result) == HTTP_OK && got == 4,
          "bytes past Content-Length are dropped");
    CHECK(http_get(&net_posix, "127.0.0.1", (unsigned int)atoi(getenv("PORT")), "/C64/NOPE.PRG",
                   nolength_sink, &got, 0, &result) == HTTP_ERR_STATUS && result.status == 404,
          "a 404 is a status error");
}

int main(void)
{
    crc32_init();
    test_config();
    test_shelf_lines();
    test_store();
    test_preview();
    test_http();
    printf(failures ? "%d FAILED\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
