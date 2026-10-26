/* The modem driver: a SwiftLink (or Turbo232) cartridge at $DE00 with a WiFi
 * modem behind it that speaks Hayes AT commands and dials a host name and a
 * port (Zimodem, WiModem232, tcpser and their like).
 *
 *   ATE0               no echo
 *   ATDT host:port     CONNECT ... then the line is the TCP connection
 *   +++ (with a second of silence around it), ATH   hang up
 *
 * The serial line is cc65's SwiftLink driver: interrupt driven, with RTS/CTS,
 * so the modem waits while the disk is written. The end of a download is the
 * HTTP Content-Length; a NO CARRIER in the data is not looked for. */
#include <serial.h>
#include <string.h>
#include <time.h>
#include "net.h"
#include "text.h"

#define LINE_MAX 40
#define IDLE_TICKS (20 * CLOCKS_PER_SEC)

extern char c64_swlink_ser[];

static const struct ser_params params = {
    SER_BAUD_38400, SER_BITS_8, SER_STOP_1, SER_PAR_NONE, SER_HS_HW
};

static unsigned char installed;
static char line[LINE_MAX + 1];
static char error_text[40];
static clock_t last_activity;

static void put_bytes(const char *data, unsigned int length)
{
    while (length) {
        if (ser_put(*data) == SER_ERR_OK) {
            ++data;
            --length;
        }
    }
}

static void put_literal(const char *source_literal)
{
    static char text[LINE_MAX + 1];
    ascii_copy(text, source_literal);
    put_bytes(text, (unsigned int)strlen(text));
}

static void put_cr(void)
{
    char cr = ASCII_CR;
    put_bytes(&cr, 1);
}

static void wait_ticks(clock_t ticks)
{
    clock_t start = clock();
    while (clock() - start < ticks) {
    }
}

static void drain(void)
{
    char c;
    while (ser_get(&c) == SER_ERR_OK) {
    }
}

/* One answer line of the modem, empty lines skipped; 0 on a timeout. */
static int read_line(clock_t ticks)
{
    clock_t start = clock();
    unsigned char n = 0;
    char c;
    for (;;) {
        if (ser_get(&c) == SER_ERR_OK) {
            if (c == ASCII_CR || c == ASCII_LF) {
                if (n) {
                    line[n] = 0;
                    return 1;
                }
            } else if (n < LINE_MAX) {
                line[n++] = c;
            }
        } else if (clock() - start > ticks) {
            return 0;
        }
    }
}

static int line_starts(const char *source_literal)
{
    static char wanted[16];
    ascii_copy(wanted, source_literal);
    return strncmp(line, wanted, strlen(wanted)) == 0;
}

static int command_ok(const char *source_literal)
{
    drain();
    put_literal(source_literal);
    put_cr();
    while (read_line(2 * CLOCKS_PER_SEC)) {
        if (line_starts("OK")) return 1;
        if (line_starts("ERROR")) return 0;
    }
    return 0;
}

static int start(void)
{
    if (!installed) {
        if (ser_install(c64_swlink_ser) != SER_ERR_OK) {
            ascii_copy(error_text, "no SwiftLink driver");
            return -1;
        }
        installed = 1;
    }
    if (ser_open(&params) != SER_ERR_OK) {
        ascii_copy(error_text, "no SwiftLink");
        return -1;
    }
    command_ok("ATE0");
    if (!command_ok("AT")) {
        ser_close();
        ascii_copy(error_text, "no modem answers AT");
        return -1;
    }
    return 0;
}

static int connect_to(const char *host, unsigned int port)
{
    char digits[6];
    unsigned char n = 0;
    drain();
    put_literal("ATDT");
    put_bytes(host, (unsigned int)strlen(host));
    {
        char colon = ASCII_COLON;
        put_bytes(&colon, 1);
    }
    do {
        digits[n++] = (char)(0x30 + port % 10);
        port /= 10;
    } while (port);
    while (n) put_bytes(&digits[--n], 1);
    put_cr();
    while (read_line(30 * CLOCKS_PER_SEC)) {
        if (line_starts("CONNECT")) {
            last_activity = clock();
            return 0;
        }
        if (line_starts("NO CARRIER") || line_starts("ERROR") || line_starts("BUSY") || line_starts("NO ANSWER")) {
            strncpy(error_text, line, sizeof error_text - 1);
            error_text[sizeof error_text - 1] = 0;
            return -1;
        }
    }
    ascii_copy(error_text, "the modem did not connect");
    return -1;
}

static int send_data(const unsigned char *data, unsigned int length)
{
    put_bytes((const char *)data, length);
    return 0;
}

static int receive(unsigned char *buffer, unsigned int max)
{
    unsigned int n = 0;
    char c;
    while (n < max && ser_get(&c) == SER_ERR_OK) buffer[n++] = (unsigned char)c;
    if (n) {
        last_activity = clock();
        return (int)n;
    }
    if (clock() - last_activity > IDLE_TICKS) {
        ascii_copy(error_text, "timed out");
        return NET_ERROR;
    }
    return NET_WAIT;
}

static void close_connection(void)
{
    wait_ticks(CLOCKS_PER_SEC + CLOCKS_PER_SEC / 5);
    put_literal("+++");
    wait_ticks(CLOCKS_PER_SEC + CLOCKS_PER_SEC / 5);
    drain();
    command_ok("ATH");
}

static const char *error(void)
{
    return error_text;
}

const NetDriver net_modem = { "modem", start, connect_to, send_data, receive, close_connection, error };
