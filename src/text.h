/* Text between the wire and the machine.
 *
 * Everything on the network and in INDEX.TXT is ASCII. cc65 stores the string
 * literals of a C64 program as PETSCII, so a literal that goes on the wire is
 * turned into ASCII first, and ASCII that goes on the screen into PETSCII.
 * On the host both are the identity.
 *
 * The same reason keeps character literals out of the code that reads the
 * wire: cc65 would turn '|' or '\n' into PETSCII too, so ASCII bytes are
 * written as numbers (ASCII_BAR and the rest). */
#ifndef WE64_TEXT_H
#define WE64_TEXT_H

#define ASCII_LF 0x0A
#define ASCII_CR 0x0D
#define ASCII_SPACE 0x20
#define ASCII_HASH 0x23
#define ASCII_DOT 0x2E
#define ASCII_COLON 0x3A
#define ASCII_EQUALS 0x3D
#define ASCII_BAR 0x7C
#define ASCII_IS_DIGIT(c) ((c) >= 0x30 && (c) <= 0x39)

char ascii_from_source(char c);
char display_from_ascii(char c);
void ascii_copy(char *to, const char *source_literal);
char ascii_upper(char c);
int ascii_equal_nocase(const char *a, const char *b);

#endif
