/* 
Copyright (c) 2013 Qualcomm Atheros, Inc.
All Rights Reserved. 
Qualcomm Atheros Confidential and Proprietary. 
*/
#define UCHAR unsigned char
#define BOOL unsigned short
#define UINT16 unsigned short int
#define UINT32 unsigned int
#define SINT16 signed short int
#define UINT8 unsigned char
#define SINT8 signed char

#define TCP_BUF_LENGTH         256

#define HCI_MAX_EVENT_SIZE     260
#define HCI_CMD_IND            (1)
#define HCI_COMMAND_HDR_SIZE   3

typedef unsigned char BYTE;

typedef enum 
{
    SERIAL,
    USB,
    INVALID
} ConnectionType;

typedef struct {
    UINT16 opcode;      /* OCF & OGF */
    UINT8   plen;
} __attribute__((packed)) hci_command_hdr;
