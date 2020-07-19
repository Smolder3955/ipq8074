/*
 * Copyright (c) 2004-2012 Qualcomm Atheros, Inc.
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
#include <stdbool.h>

#include <a_osapi.h>
#include <athdefs.h>
#include <a_types.h>
#include <if_athioctl.h>
#include <wmi.h>
#include "bmi_msg.h"
#include "bmi_ua.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

/*
 * This is a user-level agent which provides convenient access to
 * Bootloader Messaging Interface commands.  This may be used during
 * startup to customize behavior of a WLAN Target SoC.
 */


#define ADDRESS_FLAG                    0x001
#define LENGTH_FLAG                     0x002
#define PARAM_FLAG                      0x004
#define FILE_FLAG                       0x008
#define COUNT_FLAG                      0x010
#define AND_OP_FLAG                     0x020
#define BITWISE_OP_FLAG                 0x040
#define QUIET_FLAG                      0x080
#define UNCOMPRESS_FLAG                 0x100
#define TIMEOUT_FLAG                    0x200
#define UNUSED                          0x400
#define DEVICE_FLAG                     0x800

/* Limit malloc size when reading/writing file */
#define MAX_BUF                         (8*1024)

unsigned int flag;
const char *progname;
const char commands[] = 
"commands and options:\n\
--get --address=<register address>\n\
--set --address=<register address> --param=<register value>\n\
--set --address=<register address> --or=<Or-ing mask value>\n\
--set --address=<register address> --and=<And-ing mask value>\n\
--read --address=<target address> --length=<bytes> --file=<filename>\n\
--write --address=<target address> [--file=<filename> | --param=<value>] [--uncompress]\n\
--execute --address=<function start address> --param=<input param>\n\
--begin --address=<function start address>\n\
--nvram <segmentname>\n\
--info\n\
--quiet\n\
--done\n\
--timeout=<time to wait for command completion in seconds>\n\
--interface=<ethx for socket interface, sysfs for sysfs interface>\n\
--device=<device name>\n\
--wait\n\
The options can also be given in the abbreviated form --option=x or -o x. The options can be given in any order";

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

static void
usage(void)
{
    fprintf(stderr, "usage:\n%s [-i device] commands\n", progname);
    fprintf(stderr, "%s\n", commands);
    exit(-1);
}

/* Get Target Type and Version information. */
static void
BMIGetTargetInfo(int dev, struct bmi_target_info *targ_info)
{
    int nbyte;

    struct bmi_get_target_info_cmd bmicmd;

    bmicmd.u.command = BMI_GET_TARGET_INFO;
    nbyte = write(dev, &bmicmd, sizeof(bmicmd.u.command));
    if (nbyte != sizeof(bmicmd.u.command))
    {
        fprintf(stderr, "err -BMIGetTargetInfo cannot send cmd (%d)\n", nbyte);
    }

    nbyte = read(dev, &bmicmd.u.response, sizeof(bmicmd.u.response));
    if (nbyte != sizeof(bmicmd.u.response))
    {
        fprintf(stderr, "err -BMIGetTargetInfo cannot read response (%d)\n", nbyte);
    }

    /* Validate byte count in target's response */
    if (bmicmd.u.response.info.target_info_byte_count != sizeof(*targ_info))
    {
        fprintf(stderr, "err -BMIGetTargetInfo unexpected targ_info byte count (%d)\n",
			bmicmd.u.response.info.target_info_byte_count);
    }

    *targ_info = bmicmd.u.response.info; /* struct copy */
}

static void
BMIDone(int dev)
{
    int nbyte;

    struct bmi_done_cmd bmicmd;

    bmicmd.command = BMI_DONE;
    nbyte = write(dev, &bmicmd, sizeof(bmicmd));
    if (nbyte != sizeof(bmicmd))
        fprintf(stderr, "err -BMIDone cannot send cmd (%d)\n", nbyte);
}

static void
BMIReadMemory(int dev, A_UINT32 address, char *buffer, A_UINT32 length)
{
    int nbyte;
    unsigned int remaining, rxlen;
    struct bmi_read_memory_request bmicmd;

    for (remaining = length; remaining; remaining -= rxlen) {
        rxlen = min(remaining, BMI_DATASZ_MAX);
        bmicmd.command = BMI_READ_MEMORY;
        bmicmd.address = address;
        bmicmd.length = rxlen;

        nbyte = write(dev, &bmicmd, sizeof(bmicmd));
        if (nbyte != sizeof(bmicmd)) {
            fprintf(stderr, "err -BMIReadMemory cannot send cmd (%d)\n", nbyte);
        }

        nbyte = read(dev, buffer, rxlen);
        if (nbyte != rxlen) {
            fprintf(stderr, "err -BMIReadMemory cannot read response (%d)\n", nbyte);
        }
        buffer += rxlen;
        address += rxlen;
    }
}

static void
BMIWriteMemory(int dev, A_UINT32 address, char *buffer, A_UINT32 length)
{
    int nbyte;
    unsigned int remaining, txlen;
    struct bmi_write_memory_cmd bmicmd;

    for (remaining = length; remaining; remaining -= txlen) {
        int bmi_write_len;

        txlen = min(remaining, sizeof(bmicmd.data));
        bmicmd.command = BMI_WRITE_MEMORY;
        bmicmd.address = address;
        bmicmd.length = txlen;
        memcpy(bmicmd.data, &buffer[length-remaining], txlen);

        bmi_write_len = txlen + (int)&(((struct bmi_write_memory_cmd *)0)->data);
        nbyte = write(dev, &bmicmd, bmi_write_len);
        if (nbyte != bmi_write_len)
            fprintf(stderr, "err -BMIWriteMemory cannot send cmd (%d)\n", nbyte);

        address += txlen;
    }
}

static void
BMILZStreamStart(int dev, A_UINT32 address)
{
    int nbyte;
    struct bmi_lz_stream_start_cmd bmicmd;

    bmicmd.command = BMI_LZ_STREAM_START;
    bmicmd.address = address;

    nbyte = write(dev, &bmicmd, sizeof(bmicmd));
    if (nbyte != sizeof(bmicmd))
        fprintf(stderr, "err -BMILZStreamStart cannot send cmd (%d)\n", nbyte);
}

static void
BMILZData(int dev, char *buffer, A_UINT32 length)
{
    int nbyte;
    unsigned int remaining, txlen;
    struct bmi_lz_data_cmd bmicmd;
    int szhdr = sizeof(bmicmd)-sizeof(bmicmd.data);

    for (remaining = length; remaining; remaining -= txlen) {
        int bmi_write_len;

        txlen = min(remaining, sizeof(bmicmd.data));
        bmicmd.command = BMI_LZ_DATA;
        bmicmd.length = txlen;
        memcpy(bmicmd.data, &buffer[length-remaining], txlen);

        bmi_write_len = txlen + szhdr;
        nbyte = write(dev, &bmicmd, bmi_write_len);
        if (nbyte != bmi_write_len)
            fprintf(stderr, "err -BMILZData cannot send cmd (%d)\n", nbyte);
    }
}

static void
BMIReadSOCWord(int dev, A_UINT32 address, A_UINT32 *value)
{
    int nbyte;
    struct bmi_read_soc_word_request bmicmd;

    bmicmd.command = BMI_READ_SOC_WORD;
    bmicmd.address = address;
    nbyte = write(dev, &bmicmd, sizeof(bmicmd));
    if (nbyte != sizeof(bmicmd))
        fprintf(stderr, "err -BMIReadSOCWord cannot send cmd(%d)\n", nbyte);

    nbyte = read(dev, value, sizeof(*value));
    if (nbyte != sizeof(*value)) {
        fprintf(stderr, "err -BMIReadSOCWord cannot read response (%d)\n", nbyte);
    }
}

static void
BMIWriteSOCWord(int dev, A_UINT32 address, A_UINT32 param)
{
    int nbyte;
    struct bmi_write_soc_register_cmd bmicmd;

    bmicmd.command = BMI_WRITE_SOC_WORD;
    bmicmd.address = address;
    bmicmd.new_value = param;
    nbyte = write(dev, &bmicmd, sizeof(bmicmd));
    if (nbyte != sizeof(bmicmd)) {
        fprintf(stderr, "err -BMIWriteSOCWord cannot send cmd(%d)\n", nbyte);
    }
}

static void
BMISetAppStart(int dev, A_UINT32 address)
{
    int nbyte;
    struct bmi_set_app_start_cmd bmicmd;

    bmicmd.command = BMI_SET_APP_START;
    bmicmd.address = address;
    nbyte = write(dev, &bmicmd, sizeof(bmicmd));
    if (nbyte != sizeof(bmicmd))
        fprintf(stderr, "err -BMISetAppStart cannot send cmd(%d)\n", nbyte);
}

static A_UINT32
BMIExecute(int dev, A_UINT32 address, A_UINT32 param)
{
    int nbyte;
    A_UINT32 rv;
    struct bmi_execute_cmd bmicmd;

    bmicmd.command = BMI_EXECUTE;
    bmicmd.TargetPC = address;
    bmicmd.parameter = param;
    nbyte = write(dev, &bmicmd, sizeof(bmicmd));
    if (nbyte != sizeof(bmicmd)) {
        fprintf(stderr, "err -BMIExecute cannot send cmd(%d)\n", nbyte);
    }

    do {
        nbyte = read(dev, &rv, sizeof(rv));
    } while ((nbyte < 0) && (errno == ETIMEDOUT));

    if (nbyte != sizeof(rv)) {
        fprintf(stderr, "err -BMIExecute cannot read response (%d)\n", nbyte);
    }

    return rv;
}

static A_UINT32
BMINvram(int dev, char *nvram_name)
{
    int nbyte, slen, bmi_write_length;
    struct bmi_nvram_process_cmd bmicmd;
    A_UINT32 rv;

    bmicmd.command = BMI_NVRAM_PROCESS;
    slen = strlen(nvram_name)+1;
    memcpy(bmicmd.name, nvram_name, slen);
    bmi_write_length = slen + sizeof(bmicmd.command);

    nbyte = write(dev, &bmicmd, bmi_write_length);
    if (nbyte != bmi_write_length) {
        fprintf(stderr, "err -BMINvram cannot send cmd(%d)\n", nbyte);
    }

    do {
        nbyte = read(dev, &rv, sizeof(rv));
    } while ((nbyte < 0) && (errno == ETIMEDOUT));

    if (nbyte != sizeof(rv)) {
        fprintf(stderr, "err -BMINvram cannot read response (%d)\n", nbyte);
    }

    return rv;
}

int
main (int argc, char **argv) {
    int c, fd, dev;
    unsigned int address, length;
    unsigned int count, param;
    char filename[PATH_MAX];
    char nvramname[BMI_NVRAM_SEG_NAME_SZ];
    char devicename[PATH_MAX];
    unsigned int cmd;
    char *buffer;
    struct stat filestat;
    int target_version = -1;
    int target_type = -1;
    unsigned int bitwise_mask;
    unsigned int timeout;
    struct timeval tod;
    long start_secs;
    int waiting_msg_printed = 0;
    
    progname = argv[0];
 
    if (argc == 1) usage();

    flag = 0;
    memset(filename, '\0', sizeof(filename));
    memset(devicename, '\0', sizeof(devicename));

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"get", 0, NULL, 'g'},
            {"set", 0, NULL, 's'},
            {"read", 0, NULL, 'r'},
            {"timeout", 1, NULL, 'T'},
            {"file", 1, NULL, 'f'},
            {"done", 0, NULL, 'd'},
            {"write", 0, NULL, 'w'},
            {"begin", 0, NULL, 'b'},
            {"count", 1, NULL, 'c'},
            {"param", 1, NULL, 'p'},
            {"length", 1, NULL, 'l'},
            {"execute", 0, NULL, 'e'},
            {"address", 1, NULL, 'a'},
            {"info", 0, NULL, 'I'},
            {"and", 1, NULL, 'n'},
            {"or", 1, NULL, 'o'},
            {"nvram", 1, NULL, 'N'},
            {"quiet", 0, NULL, 'q'},
            {"uncompress", 0, NULL, 'u'},
            {"device", 1, NULL, 'D'},
            {"wait", 0, NULL, 'W'},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "rwWtebdgsIqf:l:a:p:c:n:o:m:D:",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'r':
            cmd = BMI_READ_MEMORY;
            break;

        case 'w':
            cmd = BMI_WRITE_MEMORY;
            break;

        case 'e':
            cmd = BMI_EXECUTE;
            break;

        case 'b':
            cmd = BMI_SET_APP_START;
            break;

        case 'd':
            cmd = BMI_DONE;
            break;

        case 'g':
            cmd = BMI_READ_SOC_WORD;
            break;

        case 's':
            cmd = BMI_WRITE_SOC_WORD;
            break;

        case 'f':
            memset(filename, '\0', sizeof(filename));
            strncpy(filename, optarg, sizeof(filename));
            flag |= FILE_FLAG;
            break;

        case 'l':
            length = atoi(optarg);
            flag |= LENGTH_FLAG;
            break;

        case 'a':
            address = strtoul(optarg, NULL, 0);
            flag |= ADDRESS_FLAG;
            break;

        case 'p':
            param = strtoul(optarg, NULL, 0);
            flag |= PARAM_FLAG;
            break;

        case 'c':
            count = atoi(optarg);
            flag |= COUNT_FLAG;
            break;

        case 'I':
            cmd = BMI_GET_TARGET_INFO;
	    break;
            
        case 'n':
            flag |= PARAM_FLAG | AND_OP_FLAG | BITWISE_OP_FLAG;
            bitwise_mask = strtoul(optarg, NULL, 0);
            break;
            
        case 'o':                
            flag |= PARAM_FLAG | BITWISE_OP_FLAG;
            bitwise_mask = strtoul(optarg, NULL, 0);
            break;

        case 'q':
            flag |= QUIET_FLAG;
            break;
            
        case 'u':
            flag |= UNCOMPRESS_FLAG;
            break;

        case 'T':
            timeout = strtoul(optarg, NULL, 0);
            flag |= TIMEOUT_FLAG;
            break;
            
        case 'D':
            strncpy(devicename, optarg, sizeof(devicename)-5);
            strcat(devicename, "/bmi");
            flag |= DEVICE_FLAG;
            break;

        case 'N':
            cmd = BMI_NVRAM_PROCESS;
            memset(nvramname, '\0', sizeof(nvramname));
            strncpy(nvramname, optarg, sizeof(nvramname));
            break;

        case 'W':
            cmd = BMI_NO_COMMAND;
            break;

        default:
            usage();
        }
    }

    if (flag & TIMEOUT_FLAG) {
        gettimeofday(&tod, NULL);
        start_secs = tod.tv_sec;
    }

    for (;;) {

        /* BMI uses a sysfs special file which may be auto-detected */
        if (!(flag & DEVICE_FLAG)) {
    	    FILE *findbmi;
    	    size_t nbytes;
    
            /*
             * Convenience: if no device was specified on the command
             * line, try to figure it out.  Typically there's only a
             * single device anyway.
             */
    	    findbmi = popen("find /sys/devices -name bmi | head -1", "r");
            if (findbmi) {
    	        nbytes=fread(devicename, 1, sizeof(devicename), findbmi);
    	        pclose(findbmi);
    
    	        if (nbytes > 15) {
                    /* auto-detect possibly successful */
    	            devicename[nbytes-1]='\0'; /* replace \n with 0 */
    	        } else {
                    strcpy(devicename, "unknown_BMI_device");
                }
            }
        }
    
        dev = open(devicename, O_RDWR);
        if (dev >= 0) {
            break; /* successfully opened bmi special file */
        } else {
            if (flag & TIMEOUT_FLAG) {
                gettimeofday(&tod, NULL);
                if (tod.tv_sec - start_secs > timeout) {
                    /* timeout expired */
                    fprintf(stderr, "err -Failed (%d) to open bmi file (%s)\n", errno, devicename);
                    exit(1);
                }
            }
        }

        if (!waiting_msg_printed) {
            nqprintf("bmiloader is waiting for Target....\n");
            waiting_msg_printed = 1;
        }
    
        /* Give the Target device a chance to start. */
        usleep(100000); /* Wait for 100ms */
    }

    switch(cmd)
    {
    case BMI_NO_COMMAND:
        /* Used to check args and bmi file */
        break;

    case BMI_DONE:
        nqprintf("BMI Done\n");
        BMIDone(dev);
        break;

    case BMI_READ_MEMORY:
        if ((flag & (ADDRESS_FLAG | LENGTH_FLAG | FILE_FLAG)) == 
            (ADDRESS_FLAG | LENGTH_FLAG | FILE_FLAG))
        {
            nqprintf(
                 "BMI Read Memory (address: 0x%x, length: %d, filename: %s)\n",
                  address, length, filename);

            if ((fd = open(filename, O_CREAT|O_WRONLY|O_TRUNC, 00644)) < 0)
            {
                perror("Could not create a file");
                exit(1);
            }
            buffer = (char *)MALLOC(MAX_BUF + 12);

            {
                unsigned int remaining = length;

                while (remaining)
                {
                    length = (remaining > MAX_BUF) ? MAX_BUF : remaining;
                    BMIReadMemory(dev, address, buffer, length);
                    write(fd, buffer, length);
                    remaining -= length;
                    address += length;
                }
            }

            close(fd);
            free(buffer);
        }
        else usage();
        break;

    case BMI_WRITE_MEMORY:
        if (!(flag & ADDRESS_FLAG))
        {
            if (flag & FILE_FLAG) {
                /*
                 * When we fail to specify an address for a file write,
                 * assume it's a segmented file which includes metadata
                 * for addresses and lengths for each segment.
                 */
                address = BMI_SEGMENTED_WRITE_ADDR;
                flag |= ADDRESS_FLAG;
            } else {
                usage(); /* no address specified */
            }
        }
        if (!(flag & (FILE_FLAG | PARAM_FLAG)))
        {
            usage(); /* no data specified */
        }
        if ((flag & FILE_FLAG) && (flag & PARAM_FLAG))
        {
            usage(); /* too much data specified */
        }
        if ((flag & UNCOMPRESS_FLAG) && !(flag & FILE_FLAG))
        {
            usage(); /* uncompress only works with a file */
        }

        if (flag & FILE_FLAG)
        {
            nqprintf(
                 "BMI Write %sMemory (address: 0x%x, filename: %s)\n",
                  ((flag & UNCOMPRESS_FLAG) ? "compressed " : ""),
                  address, filename);
            if ((fd = open(filename, O_RDONLY)) < 0)
            {
                perror("Could not open file");
                exit(1);
            }
            memset(&filestat, '\0', sizeof(struct stat));
            buffer = (char *)MALLOC(MAX_BUF + 12);
            fstat(fd, &filestat);
            length = filestat.st_size;

            if (flag & UNCOMPRESS_FLAG) {
                /* Initiate compressed stream */
                BMILZStreamStart(dev, address);
            }
        }
        else
        { /* PARAM_FLAG */
            nqprintf(
                 "BMI Write Memory (address: 0x%x, value: 0x%x)\n",
                  address, param);
            length = sizeof(param);
            buffer = (char *)&param;
            fd = -1;
        }

        /*
         * Write length bytes of data to memory.
         * Data is either present in buffer OR
         * needs to be read from fd in MAX_BUF chunks.
         *
         * Within the kernel, the implementation of
         * BMI_WRITE_MEMORY further limits the size
         * of each transfer over the interconnect
         * according to BMI protocol limitations.
         */ 
        {
            unsigned int remaining = length;

            while (remaining)
            {
                int nbyte;

                length = (remaining > MAX_BUF) ? MAX_BUF : remaining;
                if (flag & UNCOMPRESS_FLAG) {
                    /* 0 pad last word of data to avoid messy uncompression */
                    ((A_UINT32 *)buffer)[2+((length-1)/4)] = 0;

                    nbyte = read(fd, buffer, length);
                    if (nbyte != length) {
                        fprintf(stderr, "err -Read from compressed file failed (%d)\n", nbyte);
                    }
                    nqprintf("BMI LZ Data (length: 0x%x)\n", length);
                    BMILZData(dev, buffer, length);
                } else {
                    if (fd > 0)
                    {
                        nbyte = read(fd, buffer, length);
                        if (nbyte != length) {
                            fprintf(stderr, "err -Read from file failed (%d)\n", nbyte);
                        }
                    }

                    BMIWriteMemory(dev, address, buffer, length);
                }

                remaining -= length;
                address += length;
            }
        }

        if (flag & FILE_FLAG) {
            free(buffer);
            close(fd);
        }

        if (flag & UNCOMPRESS_FLAG) {
            BMILZStreamStart(dev, 0);
        }

        break;

    case BMI_READ_SOC_WORD:
        if ((flag & (ADDRESS_FLAG)) == (ADDRESS_FLAG))
        {
            nqprintf("BMI Read Word (address: 0x%x)\n", address);
            BMIReadSOCWord(dev, address, &param);

            if (quiet()) {
                printf("0x%x\n", param);
            } else {
                printf("Return Value from target: 0x%x\n", param);
            }
        }
        else usage();
        break;

    case BMI_WRITE_SOC_WORD:
        if ((flag & (ADDRESS_FLAG | PARAM_FLAG)) == (ADDRESS_FLAG | PARAM_FLAG))
        {
            A_UINT32 origvalue = 0;
            
            if (flag & BITWISE_OP_FLAG) {
                /* first read */    
                BMIReadSOCWord(dev, address, &origvalue);
                param = origvalue;
                
                /* now modify */
                if (flag & AND_OP_FLAG) {
                    param &= bitwise_mask;        
                } else {
                    param |= bitwise_mask;
                }               
            
                /* fall through to write out the parameter */
            }
            
            if (flag & BITWISE_OP_FLAG) {
                if (quiet()) {
                    printf("0x%x\n", origvalue);
                } else {
                    printf("BMI Bit-Wise (%s) modify Word (address: 0x%x, orig:0x%x, new: 0x%x,  mask:0x%X)\n", 
                       (flag & AND_OP_FLAG) ? "AND" : "OR", address, origvalue, param, bitwise_mask );   
                }
            } else{ 
                nqprintf("BMI Write Word (address: 0x%x, param: 0x%x)\n", address, param);
            }
            
            BMIWriteSOCWord(dev, address, param);
        }
        else usage();
        break;

    case BMI_EXECUTE:
        if ((flag & (ADDRESS_FLAG | PARAM_FLAG)) == (ADDRESS_FLAG | PARAM_FLAG))
        {
            A_UINT32 rv;

            nqprintf("BMI Execute (address: 0x%x, param: 0x%x)\n", address, param);
            rv = BMIExecute(dev, address, param);
            param = rv;

            if (quiet()) {
                printf("0x%x\n", param);
            } else {
                printf("Return Value from target: 0x%x\n", param);
            }
        }
        else usage();
        break;

    case BMI_SET_APP_START:
        if ((flag & ADDRESS_FLAG) == ADDRESS_FLAG)
        {
            nqprintf("BMI Set App Start (address: 0x%x)\n", address);
            BMISetAppStart(dev, address);
        }
        else usage();
        break;

    case BMI_GET_TARGET_INFO:
    {
        struct bmi_target_info targ_info;

        nqprintf("BMI Target Info:\n");
        BMIGetTargetInfo(dev, &targ_info);
        target_version = targ_info.target_ver;
        target_type = targ_info.target_type;

        printf("TARGET_TYPE=%s\n",
                ((target_type == TARGET_TYPE_AR9888) ? "AR9888" : 
                ((target_type == TARGET_TYPE_AR6320) ? "AR6320" : 
                ((target_type == TARGET_TYPE_AR900B) ? "AR900B" : 
                ((target_type == TARGET_TYPE_QCA9984)? "QCA9984": 
                ((target_type == TARGET_TYPE_QCA9888)? "QCA9888": 
                ((target_type == TARGET_TYPE_IPQ4019)? "IPQ4019" : 
                  "unknown"))))));
        printf("TARGET_VERSION=0x%x\n", target_version);
        break;
    }

    case BMI_NVRAM_PROCESS:
    {
        int rv;

        nqprintf("BMINvram (name: %s)\n", nvramname);
        rv = BMINvram(dev, nvramname);

        if (quiet()) {
            printf("0x%x\n", rv);
        } else {
            printf("Return Value from target: 0x%x\n", rv);
        }
        break;
    }

    default:
        usage();
    }

    exit (0);
}
