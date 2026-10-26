/* The ip65 driver: RR-Net, ETH64 and their clones (CS8900a, LAN91C96), with
 * ip65's own TCP/IP stack, DHCP and DNS. VICE emulates the RR-Net, so this is
 * the driver the emulator can test. ip65's error texts are ASCII already.
 *
 * ip65 hands received data to a callback while ip65_process runs. The driver
 * runs it only when its buffer is empty, so one segment always fits; a slow
 * disk simply leaves the next ones to TCP's retransmission. */
#include <ip65.h>
#include <string.h>
#include "net.h"
#include "text.h"

#define SEGMENT 1460
#define IDLE_MS 20000U

static unsigned char received[SEGMENT];
static unsigned int received_length;
static unsigned int received_at;
static unsigned char closed;
static unsigned int last_activity;
static char error_text[40];

static void remember_error(const char *stage_literal)
{
    const char *text = ip65_strerror(ip65_error);
    unsigned char n;
    ascii_copy(error_text, stage_literal);
    n = (unsigned char)strlen(error_text);
    while (*text && n < sizeof error_text - 1) error_text[n++] = *text++;
    error_text[n] = 0;
}

static void __fastcall__ on_data(const uint8_t *data, int16_t length)
{
    if (length < 0) {
        closed = 1;
        return;
    }
    if ((unsigned int)length > SEGMENT) length = SEGMENT;
    memcpy(received, data, (unsigned int)length);
    received_length = (unsigned int)length;
    received_at = 0;
}

static int start(void)
{
    if (ip65_init(ETH_INIT_DEFAULT)) {
        remember_error("card: ");
        return -1;
    }
    if (dhcp_init()) {
        remember_error("DHCP: ");
        return -1;
    }
    return 0;
}

static int connect_to(const char *host, unsigned int port)
{
    uint32_t address = dns_resolve(host);
    if (!address) {
        remember_error("DNS: ");
        return -1;
    }
    received_length = 0;
    received_at = 0;
    closed = 0;
    if (tcp_connect(address, port, on_data)) {
        remember_error("TCP: ");
        return -1;
    }
    last_activity = timer_read();
    return 0;
}

static int send_data(const unsigned char *data, unsigned int length)
{
    unsigned int part;
    while (length) {
        part = length > SEGMENT ? SEGMENT : length;
        if (tcp_send(data, part)) {
            remember_error("send: ");
            return -1;
        }
        data += part;
        length -= part;
    }
    return 0;
}

static int receive(unsigned char *buffer, unsigned int max)
{
    unsigned int n;
    if (received_at >= received_length) {
        if (closed) return 0;
        ip65_process();
        if (received_at >= received_length) {
            if (closed) return 0;
            if ((unsigned int)(timer_read() - last_activity) > IDLE_MS) {
                ascii_copy(error_text, "timed out");
                return NET_ERROR;
            }
            return NET_WAIT;
        }
        last_activity = timer_read();
    }
    n = received_length - received_at;
    if (n > max) n = max;
    memcpy(buffer, received + received_at, n);
    received_at += n;
    return (int)n;
}

static void close_connection(void)
{
    if (!closed) tcp_close();
    closed = 1;
}

static const char *error(void)
{
    return error_text;
}

const NetDriver net_ip65 = { "ip65", start, connect_to, send_data, receive, close_connection, error };
