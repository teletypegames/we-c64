/* One TCP connection, whatever the hardware.
 *
 * A driver opens a connection to a host name and a port, sends, and receives
 * what has arrived. net_receive returns the number of bytes read, 0 when the
 * peer closed the connection, and NET_WAIT when nothing has come yet. */
#ifndef WARP64U_NET_H
#define WARP64U_NET_H

#define NET_WAIT (-1)
#define NET_ERROR (-2)

typedef struct {
    const char *name;
    int (*start)(void);
    int (*connect)(const char *host, unsigned int port);
    int (*send)(const unsigned char *data, unsigned int length);
    int (*receive)(unsigned char *buffer, unsigned int max);
    void (*close)(void);
    const char *(*error)(void);
} NetDriver;

#endif
