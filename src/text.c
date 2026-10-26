#include "text.h"

#ifdef __CC65__
char ascii_from_source(char c)
{
    unsigned char u = (unsigned char)c;
    if (u >= 0x41 && u <= 0x5A) return (char)(u + 0x20);
    if (u >= 0xC1 && u <= 0xDA) return (char)(u - 0x80);
    if (u >= 0x61 && u <= 0x7A) return (char)(u - 0x20);
    return c;
}

char display_from_ascii(char c)
{
    unsigned char u = (unsigned char)c;
    if (u >= 0x41 && u <= 0x5A) return (char)(u + 0x80);
    if (u >= 0x61 && u <= 0x7A) return (char)(u - 0x20);
    if (u < 0x20 || u > 0x7E) return '?';
    return c;
}
#else
char ascii_from_source(char c) { return c; }
char display_from_ascii(char c) { return (c < 0x20 || c > 0x7E) ? '?' : c; }
#endif

void ascii_copy(char *to, const char *source_literal)
{
    while (*source_literal) *to++ = ascii_from_source(*source_literal++);
    *to = 0;
}

char ascii_upper(char c)
{
    return (c >= 0x61 && c <= 0x7A) ? (char)(c - 0x20) : c;
}

int ascii_equal_nocase(const char *a, const char *b)
{
    while (*a && *b) {
        if (ascii_upper(*a) != ascii_upper(*b)) return 0;
        ++a;
        ++b;
    }
    return *a == *b;
}
