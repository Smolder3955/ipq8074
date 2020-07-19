/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * $ATH_LICENSE_HOSTSDK0_C$
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <linux/version.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include <a_osapi.h>
#include <athdefs.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define PCI_BAR_OFFSET  0x20000

#define here()  printf("SPECTRAL : %s : %d\n", __func__, __LINE__)
/*
 * This is a user-level agent which provides diagnostic read/write
 * access to Target space.  This may be used
 *   to collect information for analysis
 *   to read/write Target registers
 *   etc.
 */

#define DIAG_READ_TARGET      1
#define DIAG_WRITE_TARGET     2
#define DIAG_READ_WORD        3
#define DIAG_WRITE_WORD       4

#define ADDRESS_FLAG                    0x001
#define LENGTH_FLAG                     0x002
#define PARAM_FLAG                      0x004
#define FILE_FLAG                       0x008
#define UNUSED0x010                     0x010
#define AND_OP_FLAG                     0x020
#define BITWISE_OP_FLAG                 0x040
#define QUIET_FLAG                      0x080
#define UNUSED0x100                     0x100
#define UNUSED0x200                     0x200
#define UNUSED0x400                     0x400
#define DEVICE_FLAG                     0x800

/* Limit malloc size when reading/writing file */
#define MAX_BUF                         (8*1024)

unsigned int flag;
const char *progname;
const char commands[] =
"commands and options:\n\
--get --address=<target word address>\n\
--set --address=<target word address> --[value|param]=<value>\n\
                                      --or=<OR-ing value>\n\
                                      --and=<AND-ing value>\n\
--read --address=<target address> --length=<bytes> --file=<filename>\n\
--write --address=<target address> --file=<filename>\n\
                                   --[value|param]=<value>\n\
--quiet\n\
--device=<device name> (if not default)\n\
The options can also be given in the abbreviated form --option=x or -o x.\n\
The options can be given in any order.";

#define A_ROUND_UP(x, y)             ((((x) + ((y) - 1)) / (y)) * (y))

#define quiet() (flag & QUIET_FLAG)
#define nqprintf(args...) if (!quiet()) {printf(args);}
#define min(x,y) ((x) < (y) ? (x) : (y))

INLINE void *
MALLOC(int nbytes)
{
    void *p= malloc(nbytes);

    if (!p)
    {
        fprintf(stderr, "err -Cannot allocate memory\n");
    }

    return p;
}

void
usage(void)
{
    fprintf(stderr, "usage:\n%s ", progname);
    fprintf(stderr, "%s\n", commands);
    exit(-1);
}

void
DiagReadTarget(int dev, u_int32_t address, u_int8_t *buffer, u_int32_t length)
{
    int nbyte;
    unsigned int remaining;

    (void)lseek(dev, address, SEEK_SET);

    remaining = length;
    while (remaining) {
        nbyte = read(dev, buffer, (size_t)remaining);
        if (nbyte <= 0) {
            fprintf(stderr, "err %s failed (nbyte=%d, address=0x%x remaining=%d).\n",
                __FUNCTION__, nbyte, address, remaining);
            exit(1);
        }

        remaining -= nbyte;
        buffer += nbyte;
        address += nbyte;
    }
}

void
DiagReadWord(int dev, u_int32_t address, u_int32_t *buffer)
{
    DiagReadTarget(dev, address, (u_int8_t *)buffer, sizeof(*buffer));
}

void
DiagWriteTarget(int dev, u_int32_t address, u_int8_t *buffer, u_int32_t length)
{
    int nbyte;
    unsigned int remaining;

    (void)lseek(dev, address, SEEK_SET);

    remaining = length;
    while (remaining) {
        nbyte = write(dev, buffer, (size_t)remaining);
        if (nbyte <= 0) {
            fprintf(stderr, "err %s failed (nbyte=%d, address=0x%x remaining=%d).\n",
                __FUNCTION__, nbyte, address, remaining);
        }

        remaining -= nbyte;
        buffer += nbyte;
        address += nbyte;
    }
}

void
DiagWriteWord(int dev, u_int32_t address, u_int32_t value)
{
    u_int32_t param = value;

    DiagWriteTarget(dev, address, (u_int8_t *)&param, sizeof(param));
}

unsigned int
parse_address(char *optarg)
{
    unsigned int address;

    /* may want to add support for symbolic addresses here */

    address = strtoul(optarg, NULL, 0);

    return address;
}

#define DEVNAME "/sys/devices/pci0000:00/0000:00:1c.3/0000:05:00.0/athdiag"

int
dev_init (char* dev_name) {

    int c, fd, dev;
    unsigned int address, length;
    u_int32_t param;
    char filename[PATH_MAX];
    char devicename[PATH_MAX];
    unsigned int cmd;
    u_int8_t *buffer;
    unsigned int bitwise_mask;

    memset(filename, '\0', sizeof(filename));
    memset(devicename, '\0', sizeof(devicename));

    if (!dev_name) {
        strncpy(devicename, DEVNAME, sizeof(devicename));
    } else {
        strncpy(devicename, dev_name, sizeof(devicename));
    }
    flag |= DEVICE_FLAG;

    for (;;) {
        /* DIAG uses a sysfs special file which may be auto-detected */
        if (!(flag & DEVICE_FLAG)) {
            FILE *find_dev;
            size_t nbytes;

            /*
             * Convenience: if no device was specified on the command
             * line, try to figure it out.  Typically there's only a
             * single device anyway.
             */
            find_dev = popen("find /sys/devices -name athdiag | head -1", "r");
            if (find_dev) {
                nbytes=fread(devicename, 1, sizeof(devicename), find_dev);
                pclose(find_dev);

                if (nbytes > 15) {
                    /* auto-detect possibly successful */
                    devicename[nbytes-1]='\0'; /* replace \n with 0 */
                } else {
                    strcpy(devicename, "unknown_DIAG_device");
                }
            }
        }

        dev = open(devicename, O_RDWR);
        if (dev >= 0) {
            fprintf(stderr, "opened device\n");
            break; /* successfully opened diag special file */
        } else {
            fprintf(stderr, "err %s failed (%d) to open DIAG file (%s)\n",
                __FUNCTION__, errno, devicename);
            exit(1);
        }
    }

    return dev;
}

int write_reg(int dev,  u_int32_t address, u_int32_t param)
{
    address = PCI_BAR_OFFSET | address;
    DiagWriteWord(dev, address, param);
    //printf("W : address = 0x%x value = 0x%x\n", address, param);
    return 0;
}

int read_reg(int dev, u_int32_t address, u_int32_t* param)
{
    *param = 0;

    address = PCI_BAR_OFFSET | address;
    DiagReadWord(dev, address, param);
    //printf("R : address = 0x%x value = 0x%x\n", address, *param);
    return *param;
}
