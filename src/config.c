#include <string.h>
#include "config.h"
#include "text.h"

static char pending[CONFIG_TEXT_MAX * 2];
static unsigned char pending_length;

void config_defaults(Config *config)
{
    ascii_copy(config->host, "archive.teletypegames.org");
    config->port = 80;
    ascii_copy(config->path, "/C64");
    ascii_copy(config->net, "auto");
    config->drive = 8;
    config->dir[0] = 0;
    pending_length = 0;
}

static char normalized(char c)
{
#ifdef __CC65__
    unsigned char u = (unsigned char)c;
    if (u >= 0xC1 && u <= 0xDA) return (char)(u - 0x80);
#endif
    return c;
}

static unsigned int to_number(const char *text)
{
    unsigned int value = 0;
    while (ASCII_IS_DIGIT(*text)) value = value * 10 + (unsigned int)(*text++ - 0x30);
    return value;
}

static int key_is(const char *key, const char *source_literal)
{
    char wanted[8];
    ascii_copy(wanted, source_literal);
    return ascii_equal_nocase(key, wanted);
}

static void lower_into(char *to, const char *value, unsigned char max)
{
    unsigned char n = 0;
    while (*value && n < max) {
        char c = *value++;
        to[n++] = (c >= 0x41 && c <= 0x5A) ? (char)(c + 0x20) : c;
    }
    to[n] = 0;
}

void config_line(Config *config, const char *line)
{
    char key[8];
    char value[CONFIG_TEXT_MAX + 1];
    unsigned char n = 0;
    while (*line == ASCII_SPACE) ++line;
    while (*line && *line != ASCII_EQUALS && n < sizeof key - 1) key[n++] = ascii_upper(normalized(*line++));
    key[n] = 0;
    if (*line != ASCII_EQUALS) return;
    ++line;
    n = 0;
    while (*line && *line != ASCII_CR && *line != ASCII_LF && n < CONFIG_TEXT_MAX) value[n++] = normalized(*line++);
    while (n && value[n - 1] == ASCII_SPACE) --n;
    value[n] = 0;

    if (key_is(key, "HOST")) lower_into(config->host, value, CONFIG_TEXT_MAX);
    else if (key_is(key, "PORT")) config->port = to_number(value);
    else if (key_is(key, "PATH")) strcpy(config->path, value);
    else if (key_is(key, "NET")) lower_into(config->net, value, sizeof config->net - 1);
    else if (key_is(key, "DRIVE")) config->drive = (unsigned char)to_number(value);
    else if (key_is(key, "DIR")) strcpy(config->dir, value);
}

void config_feed(Config *config, const unsigned char *data, unsigned int length)
{
    unsigned char c;
    while (length--) {
        c = *data++;
        if (c == ASCII_CR || c == ASCII_LF) {
            pending[pending_length] = 0;
            if (pending_length) config_line(config, pending);
            pending_length = 0;
        } else if (pending_length < sizeof pending - 1) {
            pending[pending_length++] = (char)c;
        }
    }
}
