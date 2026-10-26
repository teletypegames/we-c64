/* The few file operations the store needs, on a CBM drive (IEC) or, in the
 * host tests, a folder. Names are the bare disk names (ARIADNE, not
 * ARIADNE.PRG); a program is written as a PRG file, the index files as SEQ. */
#ifndef WARP64U_DISK_H
#define WARP64U_DISK_H

#define DISK_PRG 0
#define DISK_SEQ 1

void disk_use(unsigned char device);
void disk_finish(void);
int disk_cd(const char *dir);
int disk_open_write(const char *name, unsigned char type);
int disk_write(const unsigned char *data, unsigned int length);
int disk_close_write(void);
int disk_open_read(const char *name, unsigned char type);
int disk_read(unsigned char *buffer, unsigned int max);
void disk_close_read(void);
int disk_scratch(const char *name);
int disk_rename(const char *from, const char *to);
const char *disk_status(void);

#endif
