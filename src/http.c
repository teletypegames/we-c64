#include <string.h>
#include "http.h"
#include "text.h"

#define BUFFER_SIZE 892

static unsigned char buffer[BUFFER_SIZE];

static int send_all(const NetDriver *net, const unsigned char *data, unsigned int length)
{
    return net->send(data, length) == 0;
}

static char *put_literal(char *p, const char *source_literal)
{
    ascii_copy(p, source_literal);
    return p + strlen(p);
}

static char *put_crlf(char *p)
{
    *p++ = ASCII_CR;
    *p++ = ASCII_LF;
    return p;
}

static unsigned int build_request(char *request, const char *host, const char *path)
{
    char *p = request;
    p = put_literal(p, "GET ");
    strcpy(p, path);
    p += strlen(p);
    p = put_literal(p, " HTTP/1.0");
    p = put_crlf(p);
    p = put_literal(p, "Host: ");
    strcpy(p, host);
    p += strlen(p);
    p = put_crlf(p);
    p = put_literal(p, "User-Agent: WE64");
    p = put_crlf(p);
    p = put_crlf(p);
    *p = 0;
    return (unsigned int)(p - request);
}

typedef struct {
    unsigned char state;
    unsigned int status;
    char header[40];
    unsigned char header_length;
    uint32_t length;
    unsigned char has_length;
} Head;

#define HEAD_STATUS 0
#define HEAD_FIELDS 1
#define HEAD_DONE 2

static void head_line(Head *head)
{
    char name[16];
    char wanted[16];
    unsigned char i = 0;
    const char *p = head->header;
    head->header[head->header_length] = 0;
    if (head->state == HEAD_STATUS && head->header_length == 0) return;
    if (head->state == HEAD_STATUS) {
        while (*p && *p != ASCII_SPACE) ++p;
        while (*p == ASCII_SPACE) ++p;
        while (ASCII_IS_DIGIT(*p)) head->status = head->status * 10 + (unsigned int)(*p++ - 0x30);
        head->state = HEAD_FIELDS;
        return;
    }
    if (head->header_length == 0) {
        head->state = HEAD_DONE;
        return;
    }
    while (*p && *p != ASCII_COLON && i < sizeof name - 1) name[i++] = *p++;
    name[i] = 0;
    ascii_copy(wanted, "Content-Length");
    if (*p == ASCII_COLON && ascii_equal_nocase(name, wanted)) {
        ++p;
        while (*p == ASCII_SPACE) ++p;
        head->length = 0;
        while (ASCII_IS_DIGIT(*p)) head->length = head->length * 10 + (uint32_t)(*p++ - 0x30);
        head->has_length = 1;
    }
}

int http_get(const NetDriver *net, const char *host, unsigned int port, const char *path,
             HttpSink sink, void *context, HttpTick tick, HttpResult *result)
{
    Head head;
    int n;
    unsigned int i;
    unsigned char c;
    unsigned int length;
    int error = HTTP_OK;

    memset(&head, 0, sizeof head);
    result->status = 0;
    result->length = 0;
    result->received = 0;

    if (net->connect(host, port) != 0) return HTTP_ERR_CONNECT;
    length = build_request((char *)buffer, host, path);
    if (!send_all(net, buffer, length)) {
        net->close();
        return HTTP_ERR_SEND;
    }

    for (;;) {
        n = net->receive(buffer, BUFFER_SIZE);
        if (n == NET_WAIT) {
            if (tick && tick(result->received, head.length)) {
                error = HTTP_ERR_ABORTED;
                break;
            }
            continue;
        }
        if (n == NET_ERROR) {
            error = HTTP_ERR_RECEIVE;
            break;
        }
        if (n == 0) break;
        i = 0;
        while (head.state != HEAD_DONE && i < (unsigned int)n) {
            c = buffer[i++];
            if (c == ASCII_LF) {
                head_line(&head);
                head.header_length = 0;
            } else if (c != ASCII_CR && head.header_length < sizeof head.header - 1) {
                head.header[head.header_length++] = (char)c;
            }
        }
        if (head.state == HEAD_DONE && i < (unsigned int)n) {
            if (head.status != 200) {
                error = HTTP_ERR_STATUS;
                break;
            }
            length = (unsigned int)n - i;
            if (head.has_length && result->received + length > head.length)
                length = (unsigned int)(head.length - result->received);
            if (sink(buffer + i, length, context) != 0) {
                error = HTTP_ERR_SINK;
                break;
            }
            result->received += length;
            if (tick && tick(result->received, head.length)) {
                error = HTTP_ERR_ABORTED;
                break;
            }
        }
        if (head.state == HEAD_DONE && head.has_length && result->received >= head.length) break;
    }
    net->close();

    result->status = head.status;
    result->length = head.has_length ? head.length : result->received;
    if (error != HTTP_OK) return error;
    if (head.state != HEAD_DONE) return HTTP_ERR_RECEIVE;
    if (head.status != 200) return HTTP_ERR_STATUS;
    if (head.has_length && result->received != head.length) return HTTP_ERR_SHORT;
    return HTTP_OK;
}
