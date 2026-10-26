/* WARP64U.CFG: KEY=value lines, ASCII or PETSCII as typed on the C64.
 * A missing file or key leaves the default. */
#ifndef WARP64U_CONFIG_H
#define WARP64U_CONFIG_H

#define CONFIG_TEXT_MAX 40

typedef struct {
    char host[CONFIG_TEXT_MAX + 1];
    unsigned int port;
    char path[CONFIG_TEXT_MAX + 1];
    char net[8];
    unsigned char drive;
    char dir[CONFIG_TEXT_MAX + 1];
} Config;

void config_defaults(Config *config);
void config_line(Config *config, const char *line);
void config_feed(Config *config, const unsigned char *data, unsigned int length);

#endif
