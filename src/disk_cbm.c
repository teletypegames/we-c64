/* The disk on a CBM drive through the KERNAL: a real 1541, an SD2IEC, the
 * drives of an Ultimate, VICE. Commands go to channel 15 (s0: scratches,
 * r0: renames, cd: changes the folder of an SD2IEC), and its answer is read
 * back after every step.
 *
 * Channel 15 stays open from the first command to disk_finish: closing it
 * closes every file of the drive, so reading the status of an open file by
 * opening and closing 15 would close the file itself. The DOS commands are written in lower case on
 * purpose: cc65 stores them as unshifted PETSCII, which is what the drive
 * expects. File names are the directory's ASCII capitals, the same bytes. */
#include <cbm.h>
#include <string.h>
#include "disk.h"

#define LFN_DATA 2
#define LFN_COMMAND 15

static unsigned char device = 8;
static char status[41];
static char command[48];

static unsigned char command_open;

void disk_finish(void)
{
    if (command_open) cbm_close(LFN_COMMAND);
    command_open = 0;
}

void disk_use(unsigned char number)
{
    if (number != device) disk_finish();
    device = number;
}

static int open_command(void)
{
    if (command_open) return 0;
    if (cbm_open(LFN_COMMAND, device, 15, "") != 0) {
        strcpy(status, "74,drive not ready,00,00");
        return -1;
    }
    command_open = 1;
    return 0;
}

static int read_status(void)
{
    int n;
    status[0] = 0;
    if (open_command() != 0) return 74;
    n = cbm_read(LFN_COMMAND, status, sizeof status - 1);
    if (n < 0) n = 0;
    while (n && (status[n - 1] == 0x0D || status[n - 1] == 0x0A)) --n;
    status[n] = 0;
    if (n < 2) return 74;
    return (status[0] - 0x30) * 10 + (status[1] - 0x30);
}

static int run_command(const char *text)
{
    unsigned int length = (unsigned int)strlen(text);
    if (open_command() != 0) return 74;
    if (cbm_write(LFN_COMMAND, text, length) != (int)length) {
        strcpy(status, "74,drive not ready,00,00");
        return 74;
    }
    return read_status();
}

int disk_cd(const char *dir)
{
    strcpy(command, "cd:");
    strcat(command, dir);
    return run_command(command) < 20 ? 0 : -1;
}

int disk_open_write(const char *name, unsigned char type)
{
    strcpy(command, name);
    strcat(command, type == DISK_PRG ? ",p,w" : ",s,w");
    if (cbm_open(LFN_DATA, device, LFN_DATA, command) != 0) {
        read_status();
        return -1;
    }
    if (read_status() >= 20) {
        cbm_close(LFN_DATA);
        return -1;
    }
    return 0;
}

int disk_write(const unsigned char *data, unsigned int length)
{
    if (cbm_write(LFN_DATA, data, length) != (int)length) return -1;
    return 0;
}

int disk_close_write(void)
{
    cbm_close(LFN_DATA);
    return read_status() < 20 ? 0 : -1;
}

int disk_open_read(const char *name, unsigned char type)
{
    strcpy(command, name);
    strcat(command, type == DISK_PRG ? ",p,r" : ",s,r");
    if (cbm_open(LFN_DATA, device, LFN_DATA, command) != 0) {
        read_status();
        return -1;
    }
    if (read_status() >= 20) {
        cbm_close(LFN_DATA);
        return -1;
    }
    return 0;
}

int disk_read(unsigned char *buffer, unsigned int max)
{
    int n = cbm_read(LFN_DATA, buffer, max);
    return n < 0 ? 0 : n;
}

void disk_close_read(void)
{
    cbm_close(LFN_DATA);
}

int disk_scratch(const char *name)
{
    strcpy(command, "s0:");
    strcat(command, name);
    return run_command(command) < 20 ? 0 : -1;
}

int disk_rename(const char *from, const char *to)
{
    strcpy(command, "r0:");
    strcat(command, to);
    strcat(command, "=");
    strcat(command, from);
    return run_command(command) < 20 ? 0 : -1;
}

const char *disk_status(void)
{
    return status;
}
