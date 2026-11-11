/* WARP64U: the Teletype Games store on the C64.
 *
 * The list is the shelf on the server (INDEX.TXT) beside what is installed
 * here (WARP64U.IDX); RETURN installs or updates, R runs, DEL removes, U
 * updates every title, SPACE shows the details of one, P its preview, C
 * narrows the list to one category, F1 reads the shelf again. At start a new
 * WARP64U on the shelf is offered. Running a title leaves WARP64U the old
 * way: LOAD and RUN on the screen, two RETURNs in the keyboard buffer. */
#include <cbm.h>
#include <conio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "disk.h"
#include "net.h"
#include "preview.h"
#include "store.h"
#include "text.h"
#include "version.h"

extern const NetDriver net_uci;
extern const NetDriver net_ip65;
extern const NetDriver net_modem;

#define LIST_TOP 2
#define LIST_ROWS 14
#define DETAIL_TOP 17
#define MESSAGE_ROW 23
#define HELP_ROW 24

#define ROW_REMOTE 0
#define ROW_LOCAL 1

#define KEY_STOP 0x03
#define KEY_RETURN 0x0D
#define KEY_DOWN 0x11
#define KEY_DEL 0x14
#define KEY_UP 0x91
#define KEY_F1 0x85
#define KEY_SPACE 0x20

#define CATEGORY_SLOTS 6

/* Where LOAD and RUN go so that the two RETURNs find them. Leaving to BASIC
 * scrolls the screen up a line and prints READY. at the top, so LOAD, written
 * on row 2, is under the cursor on row 1; LOAD then prints SEARCHING FOR,
 * LOADING and READY. on rows 3 to 5, and the cursor stops on row 6, where RUN,
 * written on row 7, has scrolled to. Measured in VICE with the stock KERNAL. */
#define LAUNCH_LOAD_ROW 2
#define LAUNCH_RUN_ROW 7
#define LAUNCH_EXIT_ROW 0

typedef struct {
    unsigned char kind;
    unsigned char index;
} Row;

static Store store;
static Row rows[SHELF_MAX + LOCAL_MAX];
static unsigned char row_count;
static unsigned char selected;
static unsigned char first;
static unsigned char online;
static unsigned char device;
static char categories[CATEGORY_SLOTS][SHELF_CATEGORY_MAX + 1];
static unsigned char category_count;
static unsigned char filter;

static void put_ascii(const char *text, unsigned char width)
{
    while (width && *text) {
        cputc(display_from_ascii(*text++));
        --width;
    }
    while (width--) cputc(' ');
}

static void put_text(const char *text, unsigned char width)
{
    while (width && *text) {
        cputc(*text++);
        --width;
    }
    while (width--) cputc(' ');
}

static void clear_message(void)
{
    gotoxy(0, MESSAGE_ROW);
    cclear(39);
    gotoxy(0, MESSAGE_ROW);
}

static void message_ascii(const char *text)
{
    unsigned char n = 39;
    clear_message();
    textcolor(COLOR_YELLOW);
    while (n-- && *text) cputc(display_from_ascii(*text++));
    textcolor(COLOR_GRAY3);
}

static void message(const char *text)
{
    unsigned char n = 39;
    clear_message();
    textcolor(COLOR_YELLOW);
    while (n-- && *text) cputc(*text++);
    textcolor(COLOR_GRAY3);
}

static const char *row_file(const Row *row)
{
    return row->kind == ROW_REMOTE ? store.remote.entries[row->index].file : store.local[row->index].file;
}

static void collect_categories(void)
{
    unsigned char i, k;
    const char *category;
    category_count = 0;
    for (i = 0; i < store.remote.count; ++i) {
        category = store.remote.entries[i].category;
        for (k = 0; k < category_count; ++k)
            if (strcmp(categories[k], category) == 0) break;
        if (k == category_count && category_count < CATEGORY_SLOTS) strcpy(categories[category_count++], category);
    }
    if (filter > category_count) filter = 0;
}

/* A title without a category is a game. */
static void put_category(const char *category, unsigned char width)
{
    if (*category) put_ascii(category, width);
    else put_text("game", width);
}

static void build_rows(void)
{
    unsigned char i;
    row_count = 0;
    collect_categories();
    for (i = 0; i < store.remote.count; ++i) {
        if (filter && strcmp(store.remote.entries[i].category, categories[filter - 1]) != 0) continue;
        rows[row_count].kind = ROW_REMOTE;
        rows[row_count++].index = i;
    }
    for (i = 0; i < store.local_count; ++i) {
        if (filter) break;
        if (shelf_find(&store.remote, store.local[i].file) >= 0) continue;
        rows[row_count].kind = ROW_LOCAL;
        rows[row_count++].index = i;
    }
    if (selected >= row_count) selected = row_count ? row_count - 1 : 0;
}

static unsigned char row_state(const Row *row)
{
    if (row->kind == ROW_LOCAL) return STATE_LOCAL_ONLY;
    return store_state(&store, store.remote.entries[row->index].file, store.remote.entries[row->index].version);
}

static void draw_header(void)
{
    gotoxy(0, 0);
    revers(1);
    textcolor(COLOR_LIGHTGREEN);
    cputs(" Warp64u " WARP64U_VERSION);
    cputc(' ');
    if (filter) put_category(categories[filter - 1], 40 - 10 - (unsigned char)strlen(WARP64U_VERSION) - 10);
    else put_text("all", 40 - 10 - (unsigned char)strlen(WARP64U_VERSION) - 10);
    put_text(online && store.net ? store.net->name : (const char *)"offline", 10);
    revers(0);
    textcolor(COLOR_GRAY3);
}

static void draw_row(unsigned char n)
{
    unsigned char line = LIST_TOP + n;
    unsigned char at = first + n;
    const Row *row;
    unsigned char state;
    gotoxy(0, line);
    if (at >= row_count) {
        cclear(40);
        return;
    }
    row = &rows[at];
    state = row_state(row);
    revers(at == selected);
    if (state == STATE_UPDATE) textcolor(COLOR_YELLOW);
    else if (state == STATE_INSTALLED) textcolor(COLOR_LIGHTGREEN);
    else if (state == STATE_LOCAL_ONLY) textcolor(COLOR_GRAY2);
    else textcolor(COLOR_WHITE);
    cputc(' ');
    if (row->kind == ROW_REMOTE) put_ascii(store.remote.entries[row->index].title, 26);
    else put_ascii(store.local[row->index].file, 26);
    switch (state) {
    case STATE_INSTALLED: put_text("installed", 13); break;
    case STATE_UPDATE: put_text("update", 13); break;
    case STATE_LOCAL_ONLY: put_text("local only", 13); break;
    default: put_ascii(store.remote.entries[row->index].version, 13); break;
    }
    revers(0);
    textcolor(COLOR_GRAY3);
}

static void draw_details(void)
{
    const Row *row;
    const char *text;
    unsigned char line, n;
    int local;
    for (line = DETAIL_TOP; line < MESSAGE_ROW; ++line) {
        gotoxy(0, line);
        cclear(40);
    }
    if (!row_count) return;
    row = &rows[selected];
    gotoxy(0, DETAIL_TOP);
    textcolor(COLOR_LIGHTBLUE);
    local = store_local_find(&store, row_file(row));
    if (row->kind == ROW_REMOTE) {
        put_ascii(store.remote.entries[row->index].file, 17);
        put_ascii(store.remote.entries[row->index].version, 10);
    } else {
        put_ascii(store.local[row->index].file, 20);
    }
    if (local >= 0) {
        gotoxy(28, DETAIL_TOP);
        put_text("here ", 5);
        put_ascii(store.local[local].version, 7);
    }
    textcolor(COLOR_GRAY3);
    if (row->kind != ROW_REMOTE) return;
    text = store.remote.entries[row->index].description;
    for (line = DETAIL_TOP + 1; line < MESSAGE_ROW && *text; ++line) {
        gotoxy(0, line);
        n = 40;
        while (n && *text) {
            cputc(display_from_ascii(*text++));
            --n;
        }
    }
}

static void draw_help(void)
{
    gotoxy(0, HELP_ROW);
    revers(1);
    textcolor(COLOR_GRAY2);
    put_text(online ? " ret get r run del u all spc info c type" : " r run  del remove  f1 retry  q quit", 39);
    revers(0);
    textcolor(COLOR_GRAY3);
}

static void draw_list(void)
{
    unsigned char n;
    if (selected < first) first = selected;
    if (selected >= first + LIST_ROWS) first = selected - LIST_ROWS + 1;
    for (n = 0; n < LIST_ROWS; ++n) draw_row(n);
    draw_details();
}

static void draw_all(void)
{
    clrscr();
    draw_header();
    draw_list();
    if (!row_count) {
        gotoxy(1, LIST_TOP + 1);
        cputs(online ? "The shelf is empty." : "Nothing installed, and no network.");
    }
    draw_help();
}

static uint32_t last_shown;

static void show_count(uint32_t value)
{
    char digits[11];
    unsigned char n = 0;
    do {
        digits[n++] = (char)('0' + value % 10);
        value /= 10;
    } while (value);
    while (n) cputc(digits[--n]);
}

static int tick(uint32_t received, uint32_t total)
{
    if (kbhit() && cgetc() == KEY_STOP) return 1;
    if (received != last_shown) {
        last_shown = received;
        gotoxy(20, MESSAGE_ROW);
        textcolor(COLOR_YELLOW);
        show_count(received);
        if (total) {
            cputc('/');
            show_count(total);
        }
        cclear(39 - wherex());
        textcolor(COLOR_GRAY3);
    }
    return 0;
}

static void read_config(void)
{
    static unsigned char buffer[64];
    char name[12];
    int n;
    ascii_copy(name, "WARP64U.CFG");
    disk_use(device);
    if (disk_open_read(name, DISK_SEQ) != 0) return;
    while ((n = disk_read(buffer, sizeof buffer)) > 0) config_feed(&store.config, buffer, (unsigned int)n);
    buffer[0] = 0x0D;
    config_feed(&store.config, buffer, 1);
    disk_close_read();
}

static int try_driver(const NetDriver *driver)
{
    message("Starting the network: ");
    cputs(driver->name);
    if (driver->start() != 0) {
        message_ascii(driver->error());
        return 0;
    }
    store.net = driver;
    return 1;
}

static int wanted(const char *source_literal)
{
    char name[8];
    char automatic[8];
    ascii_copy(name, source_literal);
    ascii_copy(automatic, "auto");
    return strcmp(store.config.net, automatic) == 0 || strcmp(store.config.net, name) == 0;
}

/* The modem goes before ip65: a SwiftLink sits at $DE00 like an ETH64, and
 * ip65's probe takes it for one; only a modem that answers AT with OK is a
 * modem. */
static void connect_network(void)
{
    online = 0;
    if (wanted("uci") && try_driver(&net_uci)) online = 1;
    else if (wanted("modem") && try_driver(&net_modem)) online = 1;
    else if (wanted("ip65") && try_driver(&net_ip65)) online = 1;
}

static void reload(void)
{
    store_load_local(&store);
    store.remote.count = 0;
    if (online) {
        message("Reading the shelf...");
        last_shown = 0;
        if (store_fetch(&store, tick) != STORE_OK) {
            build_rows();
            draw_all();
            message_ascii(store.message);
            return;
        }
    }
    build_rows();
    draw_all();
    if (online) {
        gotoxy(0, MESSAGE_ROW);
        textcolor(COLOR_GRAY2);
        show_count(store.remote.count);
        cputs(" titles on the shelf");
        textcolor(COLOR_GRAY3);
    }
}

static int confirm(const char *question)
{
    char c;
    message(question);
    cputs(" (y/n)");
    c = cgetc();
    return c == 'y';
}

static void put_screen_line(unsigned char row, const char *ascii_text)
{
    unsigned char *screen = (unsigned char *)(0x0400 + row * 40);
    unsigned char c;
    while ((c = (unsigned char)*ascii_text++) != 0) {
        if (c >= 0x41 && c <= 0x5A) c = (unsigned char)(c - 0x40);
        *screen++ = c;
    }
}

static void launch(const char *file)
{
    char name[SHELF_FILE_MAX + 1];
    char line[40];
    char number[4];
    shelf_disk_name(file, name);
    line[0] = 0x4C;
    line[1] = 0x4F;
    line[2] = 0x41;
    line[3] = 0x44;
    line[4] = 0x22;
    line[5] = 0;
    strcat(line, name);
    number[0] = 0x22;
    number[1] = 0x2C;
    number[2] = 0;
    strcat(line, number);
    if (device >= 10) {
        number[0] = (char)(0x30 + device / 10);
        number[1] = (char)(0x30 + device % 10);
        number[2] = 0;
    } else {
        number[0] = (char)(0x30 + device);
        number[1] = 0;
    }
    strcat(line, number);
    number[0] = 0x2C;
    number[1] = 0x31;
    number[2] = 0;
    strcat(line, number);

    bordercolor(COLOR_LIGHTBLUE);
    bgcolor(COLOR_BLUE);
    textcolor(COLOR_LIGHTBLUE);
    clrscr();
    *(unsigned char *)0xD018 = 0x15;
    put_screen_line(LAUNCH_LOAD_ROW, line);
    line[0] = 0x52;
    line[1] = 0x55;
    line[2] = 0x4E;
    line[3] = 0;
    put_screen_line(LAUNCH_RUN_ROW, line);
    gotoxy(0, LAUNCH_EXIT_ROW);
    disk_finish();
    *(unsigned char *)0x0277 = 0x0D;
    *(unsigned char *)0x0278 = 0x0D;
    *(unsigned char *)0x00C6 = 2;
    exit(0);
}

/* Installs shelf entry +index+, showing the progress on the message row;
 * returns the STORE_ code. */
static int install(unsigned char index)
{
    message("Loading ");
    textcolor(COLOR_YELLOW);
    put_ascii(store.remote.entries[index].file, 12);
    textcolor(COLOR_GRAY3);
    last_shown = 0;
    return store_install(&store, index, tick);
}

static void put_label(unsigned char line, const char *label)
{
    gotoxy(1, line);
    textcolor(COLOR_GRAY2);
    put_text(label, 9);
    textcolor(COLOR_GRAY3);
}

/* The description, broken at the spaces into lines of 38 from +line+ on. */
static void put_wrapped(const char *text, unsigned char line)
{
    unsigned char width, cut;
    while (*text && line < MESSAGE_ROW) {
        while (*text == ASCII_SPACE) ++text;
        width = 0;
        cut = 0;
        while (text[width] && width < 38) {
            if (text[width] == ASCII_SPACE) cut = width;
            ++width;
        }
        if (text[width] && cut) width = cut;
        gotoxy(1, line++);
        for (cut = 0; cut < width; ++cut) cputc(display_from_ascii(text[cut]));
        text += width;
    }
}

static void draw_info(unsigned char index)
{
    const ShelfEntry *entry = &store.remote.entries[index];
    int local = store_local_find(&store, entry->file);
    clrscr();
    draw_header();
    gotoxy(1, LIST_TOP);
    textcolor(COLOR_WHITE);
    put_ascii(entry->title, 38);
    put_label(LIST_TOP + 2, "file");
    put_ascii(entry->file, 28);
    put_label(LIST_TOP + 3, "version");
    put_ascii(entry->version, 10);
    if (local >= 0) {
        put_text("  here ", 7);
        put_ascii(store.local[local].version, 10);
    } else {
        put_text("  not here", 10);
    }
    put_label(LIST_TOP + 4, "size");
    show_count(entry->size);
    cputs(" bytes");
    put_label(LIST_TOP + 5, "author");
    put_ascii(entry->author, 28);
    put_label(LIST_TOP + 6, "type");
    put_category(entry->category, 28);
    textcolor(COLOR_LIGHTBLUE);
    put_wrapped(entry->description, LIST_TOP + 8);
    textcolor(COLOR_GRAY3);
    gotoxy(0, HELP_ROW);
    revers(1);
    textcolor(COLOR_GRAY2);
    put_text(entry->preview ? " ret get  r run  p preview  other: back" : " ret get  r run  other key: back", 39);
    revers(0);
    textcolor(COLOR_GRAY3);
}

/* Returns STORE_OK, or the STORE_ code of a failed preview (the reason is
 * in store.message); the caller redraws its screen. */
static int show_preview(unsigned char index)
{
    if (!online) return STORE_ERR_NETWORK;
    if (!store.remote.entries[index].preview) {
        ascii_copy(store.message, "No preview on the shelf");
        return STORE_ERR_CHECK;
    }
    message("Loading the preview...");
    return preview_show(&store, index);
}

/* The details of shelf entry +index+, until a key other than its own. */
static void info(unsigned char index)
{
    char c;
    unsigned char said = 0;
    for (;;) {
        draw_info(index);
        if (said) message_ascii(store.message);
        said = 0;
        c = cgetc();
        if (c == 'p') {
            said = show_preview(index) != STORE_OK;
        } else if (c == KEY_RETURN && online) {
            install(index);
            said = 1;
        } else if (c == 'r' && store_local_find(&store, store.remote.entries[index].file) >= 0) {
            launch(store.remote.entries[index].file);
        } else {
            return;
        }
    }
}

static int is_self(const char *file)
{
    char name[12];
    ascii_copy(name, "WARP64U.PRG");
    return strcmp(file, name) == 0;
}

/* Every title with a newer version on the shelf, WARP64U itself left out:
 * that one is offered at start. RUN/STOP stops the round. */
static void update_all(void)
{
    unsigned char i, done = 0, failed = 0;
    int result;
    for (i = 0; i < store.remote.count; ++i) {
        if (is_self(store.remote.entries[i].file)) continue;
        if (store_state(&store, store.remote.entries[i].file, store.remote.entries[i].version) != STATE_UPDATE) continue;
        result = install(i);
        if (result == STORE_ERR_ABORTED) break;
        if (result == STORE_OK) ++done;
        else ++failed;
    }
    draw_all();
    if (failed) {
        message_ascii(store.message);
        return;
    }
    message("");
    show_count(done);
    cputs(done == 1 ? " title updated" : " titles updated");
}

/* A WARP64U on the shelf other than this one is offered; once installed,
 * it is started in place of this one. */
static void offer_self_update(void)
{
    char version[sizeof WARP64U_VERSION];
    unsigned char i;
    if (!online) return;
    ascii_copy(version, WARP64U_VERSION);
    for (i = 0; i < store.remote.count; ++i) {
        if (!is_self(store.remote.entries[i].file)) continue;
        if (strcmp(store.remote.entries[i].version, version) == 0) return;
        message("Warp64u ");
        put_ascii(store.remote.entries[i].version, 10);
        cputs(" is out. Update? (y/n)");
        if (cgetc() != 'y') {
            message("");
            return;
        }
        if (install(i) == STORE_OK) launch(store.remote.entries[i].file);
        draw_all();
        message_ascii(store.message);
        return;
    }
}

int main(void)
{
    unsigned char c;
    const Row *row;

    device = *(unsigned char *)0x00BA;
    if (device < 8) device = 8;

    bordercolor(COLOR_BLACK);
    bgcolor(COLOR_BLACK);
    *(unsigned char *)0xD018 = 0x17;
    textcolor(COLOR_GRAY3);
    clrscr();

    store_init(&store);
    store.config.drive = device;
    read_config();
    device = store.config.drive;
    disk_use(device);
    if (store.config.dir[0]) disk_cd(store.config.dir);
    draw_header();
    connect_network();
    reload();
    offer_self_update();

    for (;;) {
        c = cgetc();
        row = row_count ? &rows[selected] : 0;
        switch (c) {
        case KEY_DOWN:
            if (selected + 1 < row_count) {
                ++selected;
                draw_list();
            }
            break;
        case KEY_UP:
            if (selected) {
                --selected;
                draw_list();
            }
            break;
        case KEY_F1:
            if (!online) connect_network();
            reload();
            break;
        case KEY_RETURN:
            if (!row || !online || row->kind != ROW_REMOTE) break;
            install(row->index);
            draw_list();
            draw_header();
            draw_help();
            message_ascii(store.message);
            break;
        case KEY_SPACE:
            if (!row || row->kind != ROW_REMOTE) break;
            info(row->index);
            draw_all();
            break;
        case 'p':
            if (!row || row->kind != ROW_REMOTE) break;
            c = (unsigned char)(show_preview(row->index) != STORE_OK);
            draw_all();
            if (c) message_ascii(store.message);
            break;
        case 'u':
            if (online) update_all();
            break;
        case 'c':
            if (!category_count) break;
            filter = (unsigned char)((filter + 1) % (category_count + 1));
            selected = 0;
            first = 0;
            build_rows();
            draw_all();
            break;
        case 'r':
            if (!row || store_local_find(&store, row_file(row)) < 0) break;
            launch(row_file(row));
            break;
        case KEY_DEL:
            if (!row || store_local_find(&store, row_file(row)) < 0) break;
            if (!confirm("Remove it from the disk?")) {
                message("");
                break;
            }
            store_remove(&store, row_file(row));
            build_rows();
            draw_all();
            message_ascii(store.message);
            break;
        case 'q':
        case KEY_STOP:
            disk_finish();
            *(unsigned char *)0xD018 = 0x15;
            bordercolor(COLOR_LIGHTBLUE);
            bgcolor(COLOR_BLUE);
            textcolor(COLOR_LIGHTBLUE);
            clrscr();
            return 0;
        }
    }
}
