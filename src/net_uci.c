/* The Ultimate driver: C64 Ultimate, Ultimate 64, 1541 Ultimate-II+. The
 * Ultimate's own firmware does TCP and DNS; the C64 talks to it through the
 * Ultimate Command Interface, four registers at $DF1C-$DF1F, with the network
 * target ($03).
 *
 *   OPEN_TCP  03 07 port.lo port.hi host... 00  -> data: socket
 *   READ      03 10 socket len.lo len.hi       -> data: count.lo count.hi bytes
 *   WRITE     03 11 socket bytes...
 *   CLOSE     03 09 socket
 *
 * A read answers 0 when the peer closed and $FFFF when nothing has come yet.
 * The Command Interface is off by default; with it off, $DF1D does not read
 * $C9 and the driver says where to turn it on.
 *
 * Not tried on hardware yet: written from the Ultimate documentation
 * (1541u-documentation.readthedocs.io, uci/network_target). */
#include <string.h>
#include "net.h"
#include "text.h"

#define CONTROL (*(volatile unsigned char *)0xDF1C)
#define STATUS (*(volatile unsigned char *)0xDF1C)
#define COMMAND (*(volatile unsigned char *)0xDF1D)
#define IDENTITY (*(volatile unsigned char *)0xDF1D)
#define RESPONSE (*(volatile unsigned char *)0xDF1E)
#define STATUS_DATA (*(volatile unsigned char *)0xDF1F)

#define PUSH_COMMAND 0x01
#define DATA_ACCEPTED 0x02
#define CLEAR_ERROR 0x08

#define STATUS_DATA_AVAILABLE 0x80
#define STATUS_STATUS_AVAILABLE 0x40
#define STATUS_STATE 0x30
#define STATUS_STATE_BUSY 0x10
#define STATUS_ERROR 0x04
#define STATUS_ACCEPTING 0x02

#define TARGET_NETWORK 0x03
#define NET_IDENTIFY 0x01
#define NET_OPEN_TCP 0x07
#define NET_CLOSE 0x09
#define NET_READ 0x10
#define NET_WRITE 0x11

#define READ_MAX 892
#define WRITE_MAX 512

static unsigned char socket_id;
static unsigned char connected;
static unsigned char data[READ_MAX + 2];
static unsigned int data_length;
static char status_text[40];
static char error_text[48];

#define PATIENCE 60000U

/* Every wait on the Ultimate is bounded: without one (or with the Command
 * Interface off) the registers read open bus and a loop could spin forever. */
static int wait_state(unsigned char mask, unsigned char value)
{
    unsigned int n = PATIENCE;
    while ((STATUS & mask) != value) {
        if (!--n) return 0;
    }
    return 1;
}

static int command(const unsigned char *bytes, unsigned int length)
{
    unsigned int i;
    unsigned char tries = 3;
    while (tries--) {
        if (!wait_state(STATUS_STATE, 0)) return 0;
        for (i = 0; i < length; ++i) COMMAND = bytes[i];
        CONTROL = PUSH_COMMAND;
        if (STATUS & STATUS_ERROR) {
            CONTROL = CLEAR_ERROR;
            continue;
        }
        {
            unsigned int n = PATIENCE;
            while ((STATUS & STATUS_STATE) == STATUS_STATE_BUSY) {
                if (!--n) return 0;
            }
        }
        return 1;
    }
    return 0;
}

static void answer(void)
{
    unsigned char n = 0;
    unsigned int guard = 2048;
    data_length = 0;
    while ((STATUS & STATUS_DATA_AVAILABLE) && --guard) {
        unsigned char b = RESPONSE;
        if (data_length < sizeof data) data[data_length++] = b;
    }
    guard = 256;
    while ((STATUS & STATUS_STATUS_AVAILABLE) && --guard) {
        char c = (char)STATUS_DATA;
        if (n < sizeof status_text - 1) status_text[n++] = c;
    }
    status_text[n] = 0;
    CONTROL = DATA_ACCEPTED;
    wait_state(STATUS_ACCEPTING, 0);
}

static int status_ok(void)
{
    return status_text[0] == 0x30 && status_text[1] == 0x30;
}

static void remember_status(void)
{
    strncpy(error_text, status_text, sizeof error_text - 1);
    error_text[sizeof error_text - 1] = 0;
}

/* $C9 at $DF1D is the Command Interface's mark, but open bus can read the
 * same, so the network target is asked to identify itself too. */
static int start(void)
{
    unsigned char request[2];
    if (IDENTITY != 0xC9) {
        ascii_copy(error_text, "turn on the Command Interface");
        return -1;
    }
    request[0] = TARGET_NETWORK;
    request[1] = NET_IDENTIFY;
    if (!command(request, 2)) {
        ascii_copy(error_text, "no Ultimate network target");
        return -1;
    }
    answer();
    if (!status_ok() || data_length == 0) {
        ascii_copy(error_text, "no Ultimate network target");
        return -1;
    }
    connected = 0;
    return 0;
}

static int connect_to(const char *host, unsigned int port)
{
    static unsigned char request[4 + 64];
    unsigned int n = (unsigned int)strlen(host);
    if (n > 63) n = 63;
    request[0] = TARGET_NETWORK;
    request[1] = NET_OPEN_TCP;
    request[2] = (unsigned char)(port & 0xFF);
    request[3] = (unsigned char)(port >> 8);
    memcpy(request + 4, host, n);
    request[4 + n] = 0;
    if (!command(request, 5 + n)) {
        ascii_copy(error_text, "the Ultimate does not answer");
        return -1;
    }
    answer();
    if (!status_ok() || data_length < 1) {
        remember_status();
        return -1;
    }
    socket_id = data[0];
    connected = 1;
    return 0;
}

static int send_data(const unsigned char *bytes, unsigned int length)
{
    static unsigned char request[3 + WRITE_MAX];
    unsigned int part;
    while (length) {
        part = length > WRITE_MAX ? WRITE_MAX : length;
        request[0] = TARGET_NETWORK;
        request[1] = NET_WRITE;
        request[2] = socket_id;
        memcpy(request + 3, bytes, part);
        if (!command(request, 3 + part)) return -1;
        answer();
        if (!status_ok()) {
            remember_status();
            return -1;
        }
        bytes += part;
        length -= part;
    }
    return 0;
}

static int receive(unsigned char *buffer, unsigned int max)
{
    unsigned char request[5];
    unsigned int count;
    if (!connected) return 0;
    if (max > READ_MAX) max = READ_MAX;
    request[0] = TARGET_NETWORK;
    request[1] = NET_READ;
    request[2] = socket_id;
    request[3] = (unsigned char)(max & 0xFF);
    request[4] = (unsigned char)(max >> 8);
    if (!command(request, 5)) return NET_ERROR;
    answer();
    if (data_length < 2) {
        remember_status();
        return NET_ERROR;
    }
    count = (unsigned int)data[0] | ((unsigned int)data[1] << 8);
    if (count == 0xFFFF) return NET_WAIT;
    if (count == 0) {
        connected = 0;
        return 0;
    }
    if (count > data_length - 2) count = data_length - 2;
    if (count > max) count = max;
    memcpy(buffer, data + 2, count);
    return (int)count;
}

static void close_connection(void)
{
    unsigned char request[3];
    request[0] = TARGET_NETWORK;
    request[1] = NET_CLOSE;
    request[2] = socket_id;
    if (command(request, 3)) answer();
    connected = 0;
}

static const char *error(void)
{
    return error_text;
}

const NetDriver net_uci = { "Ultimate", start, connect_to, send_data, receive, close_connection, error };
