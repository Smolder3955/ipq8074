/*
Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved.
Qualcomm Atheros Confidential and Proprietary.
*/

/* qcmbr.h - header file for qcmbr.c */

#ifndef _Qcmbr_H_
#define _Qcmbr_H_

#include "Socket.h"

#define MPWORD 3

#define MBUFFER 1024*4

#define MCLIENT 4

// ###############  CAL data in file #################
#define CALDATA_HEADER_PATH "/tmp/header.caldata.bin"
#define CALDATA0_FILE_PATH "/tmp/wifi0.caldata.bin"
#define CALDATA1_FILE_PATH "/tmp/wifi1.caldata.bin"


// ----------------------------------------------------------------

// From manlib.h

#define SIZE_ERROR_BUFFER       256
#define MAX_RESP_DATA_SIZE (MBUFFER + 8)

#ifndef NULL
#define NULL    0
#endif
#ifndef FALSE
#define FALSE   0
#endif
#ifndef TRUE
#define TRUE    1
#endif

// From wlantype.h

#ifdef WIN32
#undef ARCH_BIG_ENDIAN
#define DEVDRVIF_API __declspec(dllimport)

typedef char                    A_CHAR;
typedef unsigned char           A_UCHAR;
typedef A_CHAR                  A_INT8;
typedef A_UCHAR                 A_UINT8;
typedef short                   A_INT16;
typedef unsigned short          A_UINT16;
typedef long int                A_INT32;
typedef unsigned long int       A_UINT32;
typedef unsigned long int       A_UINT;

typedef unsigned char           u8;
typedef unsigned short          u16;
//typedef unsigned int            u32;

typedef A_UCHAR                 A_BOOL;
typedef void *                  OS_DEVICE_ID;

typedef __int64                 A_INT64;
typedef unsigned __int64        A_UINT64;

typedef long                    A_INT_PTR;
typedef unsigned long           A_UINT_PTR;

#define A_LL                    "I64"

typedef A_UINT64                A_LONGSTATS;

typedef unsigned __int64        u64;
typedef __int64                 s64;
#endif /* WIN32 */

#ifdef LINUX_X86
#undef ARCH_BIG_ENDIAN

#include <inttypes.h>

typedef char                    A_CHAR;
typedef unsigned char           A_UCHAR;
typedef A_CHAR                  A_INT8;
typedef A_UCHAR                 A_UINT8;
typedef short                   A_INT16;
typedef unsigned short          A_UINT16;
typedef long int                A_INT32;
typedef unsigned long int       A_UINT32;
typedef unsigned long int       A_UINT;

typedef unsigned char           u8;
typedef unsigned short          u16;

typedef A_UCHAR                 A_BOOL;
typedef void *                  OS_DEVICE_ID;

typedef int64_t                 A_INT64;
typedef uint64_t                A_UINT64;

typedef long                    A_INT_PTR;
typedef unsigned long           A_UINT_PTR;

#define A_LL                    "I64"

typedef A_UINT64                A_LONGSTATS;

typedef uint64_t                u64;
typedef int64_t                 s64;

#define DEVDRVIF_API

void (*rx_cb)(void *buf);

#define Sleep     sleep

#endif /*LINUX_X86 */

// From dk_cmds.h

#define CMD_OK 0x0

#define COMMS_ERR_MDK_ERROR 0x12

// structures for the commands
typedef struct cmdReply {
    A_UINT32 replyCmdLen;
    A_UINT32 replyCmdId; // command ID of command to which this is a reply
    A_UINT32 status; // status of the command
    A_UCHAR cmdBytes[4000]; // bytes of the command reply
} CMD_REPLY;

extern int console;

#define DbgPrintf(_fmt, arg...) \
    do{ if (console > 0) { \
          printf("%s, Line %d: "_fmt, __FILE__, __LINE__, ##arg);} \
    }while(0)

// Command status 32 bits
// Lower 16 bits gives the Error Number.
// Upper 16 bits contains some more information for that Error.
#define COMMS_ERR_MASK 0xffff
#define COMMS_ERR_SHIFT 0
#define COMMS_ERR_INFO_MASK 0xffff0000
#define COMMS_ERR_INFO_SHIFT 16
#define MAX_GENERIC_CMD_LEN     1024

// From ParameterSelect.h
struct _ParameterList
{
    int code;           // code returned by ParameterSelect()
    char *word[MPWORD]; // keyword and aliases
    char *help;
    char type;          // d(ecimal), u(nsigned), h(ex) also x, f(loat), t(text) also s
    char *units;        // only used in help message
    int nx;             // MAXIMUM NUMBER OF VALUES
    int ny;
    int nz;
    void *minimum;      // ptr to minimum value of the correct type, 0 if none
    void *maximum;      // ptr to maximum value of the correct type, 0 if none
    void *def;          // ptr to default value of the correct type, 0 if none
    int nspecial;
    struct _ParameterList *special;
};


// ----------------------------------------------------------------

extern int CommandRead();

extern int CommandNext(unsigned char *command, int max, int *client);

extern void  UserPrintSocketSet(int client);

void Qcmbr_Run(int port );

extern int processDiagPacket(int client, unsigned char *inputMsg, unsigned int cmdLen);

extern int SendItDiag(int client, unsigned char *buffer, int length);

extern char terminationChar;

extern void SetStrTerminationChar( char tc );

extern char GetStrterminationChar();

#define DIAG_TERM_CHAR 0x7E

extern int cmd_init (char *ifname, void (*rx_cb)(void *buf));
extern void cmd_send2 (void *buf, int len, unsigned char *returnBuf, unsigned int *returnBufSize , unsigned int devid);

extern unsigned long probe_device(void);
extern int IsDeviceOpened(void);

#endif //_Qcmbr_H_
