/* HTTP/1.0 GET over one connection of a NetDriver.
 *
 * The request carries a Host header (the server picks the site by it). The
 * status line and Content-Length are read; the other headers are skipped; the
 * body goes to the sink as it arrives, so a file never has to fit in memory. */
#ifndef WE64_HTTP_H
#define WE64_HTTP_H

#include <stdint.h>
#include "net.h"

#define HTTP_OK 0
#define HTTP_ERR_CONNECT 1
#define HTTP_ERR_SEND 2
#define HTTP_ERR_RECEIVE 3
#define HTTP_ERR_STATUS 4
#define HTTP_ERR_SHORT 5
#define HTTP_ERR_SINK 6
#define HTTP_ERR_ABORTED 7

typedef int (*HttpSink)(const unsigned char *data, unsigned int length, void *context);
typedef int (*HttpTick)(uint32_t received, uint32_t total);

typedef struct {
    unsigned int status;
    uint32_t length;
    uint32_t received;
} HttpResult;

int http_get(const NetDriver *net, const char *host, unsigned int port, const char *path,
             HttpSink sink, void *context, HttpTick tick, HttpResult *result);

#endif
