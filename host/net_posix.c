#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "../src/net.h"

static int sock = -1;
static char error_text[64];

static int start(void) { return 0; }

static int connect_to(const char *host, unsigned int port)
{
    struct addrinfo hints, *found = 0;
    char service[8];
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(service, sizeof service, "%u", port);
    if (getaddrinfo(host, service, &hints, &found) != 0) {
        snprintf(error_text, sizeof error_text, "no such host");
        return -1;
    }
    sock = socket(found->ai_family, found->ai_socktype, found->ai_protocol);
    if (sock < 0 || connect(sock, found->ai_addr, found->ai_addrlen) != 0) {
        snprintf(error_text, sizeof error_text, "refused");
        freeaddrinfo(found);
        if (sock >= 0) close(sock);
        sock = -1;
        return -1;
    }
    freeaddrinfo(found);
    return 0;
}

static int send_data(const unsigned char *data, unsigned int length)
{
    while (length) {
        ssize_t n = send(sock, data, length, 0);
        if (n <= 0) return -1;
        data += n;
        length -= (unsigned int)n;
    }
    return 0;
}

static int receive(unsigned char *buffer, unsigned int max)
{
    ssize_t n = recv(sock, buffer, max > 300 ? 300 : max, 0);
    if (n < 0) return NET_ERROR;
    return (int)n;
}

static void close_connection(void)
{
    if (sock >= 0) close(sock);
    sock = -1;
}

static const char *error(void) { return error_text; }

const NetDriver net_posix = { "posix", start, connect_to, send_data, receive, close_connection, error };
