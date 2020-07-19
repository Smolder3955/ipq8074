/* 
 * Copyright (c) 2014 Qualcomm Atheros, Inc. 
 * All Rights Reserved.
 * Not a Contribution. 
 * Qualcomm Atheros Confidential and Proprietary. 
 */
/*
 * Copyright 2012 The Android Open Source Project
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 * Not a Contribution.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
#ifndef HW_ROME_H
#define HW_ROME_H

/******************************************************************************
**  Constants & Macros
******************************************************************************/
#define HCI_MAX_CMD_SIZE        260
#define HCI_MAX_EVENT_SIZE     260
#define PRINT_BUF_SIZE              ((HCI_MAX_CMD_SIZE * 3) + 2)
/* HCI Command/Event Opcode */
#define HCI_RESET                       0x0C03
#define EVT_CMD_COMPLETE       0x0E
/* HCI Packet types */
#define HCI_COMMAND_PKT     0x01
#define HCI_ACLDATA_PKT      0x02
#define HCI_SCODATA_PKT     0x03
#define HCI_EVENT_PKT           0x04
#define HCI_VENDOR_PKT        0xff
#define cmd_opcode_pack(ogf, ocf) (unsigned short)((ocf & 0x03ff)|(ogf << 10))

#define NVITEM              0
#define RDWR_PROT           1
#define NVITEM_SIZE         2
#define PERSIST_HEADER_LEN  3
#define BD_ADDR_LEN         6

unsigned char vnd_local_bd_addr[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
typedef enum {
    USERIAL_OP_FLOW_ON,
    USERIAL_OP_FLOW_OFF,
    USERIAL_OP_NOP,
} userial_vendor_ioctl_op_t;


/* vendor serial control block */
typedef struct
{
    int fd;                     /* fd to Bluetooth device */
    struct termios termios;     /* serial terminal of BT port */
    char port_name[256];
} vnd_userial_cb_t;

/**** baud rates ****/
#define USERIAL_BAUD_300        0
#define USERIAL_BAUD_600        1
#define USERIAL_BAUD_1200       2
#define USERIAL_BAUD_2400       3
#define USERIAL_BAUD_9600       4
#define USERIAL_BAUD_19200      5
#define USERIAL_BAUD_57600      6
#define USERIAL_BAUD_115200     7
#define USERIAL_BAUD_230400     8
#define USERIAL_BAUD_460800     9
#define USERIAL_BAUD_921600     10
#define USERIAL_BAUD_1M         11
#define USERIAL_BAUD_1_5M       12
#define USERIAL_BAUD_2M         13
#define USERIAL_BAUD_3M         14
#define USERIAL_BAUD_4M         15
#define USERIAL_BAUD_AUTO       16

#ifndef FALSE
#define FALSE  0
#endif

#ifndef TRUE
#define TRUE   (!FALSE)
#endif

#define HCI_CHG_BAUD_CMD_OCF        0x0C
#define HCI_VENDOR_CMD_OGF             0x3F
#define WRITE_BDADDR_CMD_LEN        14
#define WRITE_BAUD_CMD_LEN             6
#define MAX_CMD_LEN                    WRITE_BDADDR_CMD_LEN
#define GET_VERSION_OCF            0x1E

#define PS_HDR_LEN                         4
#define HCI_VENDOR_CMD_OGF      0x3F
#define HCI_PS_CMD_OCF                0x0B

#define HCI_COMMAND_HDR_SIZE        3
#define EVT_CMD_COMPLETE_SIZE       3
#define EVT_CMD_STATUS                     0x0F
#define EVT_CMD_STATUS_SIZE           4
#define HCI_EVENT_HDR_SIZE              2
#define HCI_EV_SUCCESS                      0x00
/* HCI Socket options */
#define HCI_DATA_DIR            1
#define HCI_FILTER                  2
#define HCI_TIME_STAMP        3

#define P_ID_OFFSET                                     (0)
#define HCI_CMD_IND                                   (1)
#define EVENTCODE_OFFSET                      (1)
#define EVT_PLEN                                             (2)
#define PLEN                                                       (3)
#define CMD_RSP_OFFSET                             (3)
#define RSP_TYPE_OFFSET                            (4)
#define BAUDRATE_RSP_STATUS_OFFSET    (4)
#define CMD_STATUS_OFFSET                      (5)
#define P_ROME_VER_OFFSET                       (4)
#define P_BUILD_VER_OFFSET                      (6)
#define P_BASE_ADDR_OFFSET                     (8)
#define P_ENTRY_ADDR_OFFSET                   (12)
#define P_LEN_OFFSET                                   (16)
#define P_CRC_OFFSET                                  (20)
#define P_CONTROL_OFFSET                          (24)
#define PATCH_HDR_LEN                               (28)
#define MAX_DATA_PER_SEGMENT                (239)
#define VSEVENT_CODE                                 (0xFF)
#define HC_VS_MAX_CMD_EVENT                 (0xFF)
#define PATCH_PROD_ID_OFFSET                (5)
#define PATCH_PATCH_VER_OFFSET            (9)
#define PATCH_ROM_BUILD_VER_OFFSET       (11)
#define PATCH_SOC_VER_OFFSET             (13)
#define MAX_SIZE_PER_TLV_SEGMENT        (243)

/* VS Opcode */
#define HCI_PATCH_CMD_OCF                       (0)
#define EDL_SET_BAUDRATE_CMD_OCF        (0x48)

/* VS Commands */
#define VSC_SET_BAUDRATE_REQ_LEN        (1)
#define EDL_PATCH_CMD_LEN	                       (1)
#define EDL_PATCH_CMD_REQ_LEN               (1)
#define EDL_PATCH_DLD_REQ_CMD               (0x01)
#define EDL_PATCH_RST_REQ_CMD               (0x05)
#define EDL_PATCH_SET_REQ_CMD               (0x16)
#define EDL_PATCH_ATCH_REQ_CMD              (0x17)
#define EDL_PATCH_VER_REQ_CMD               (0x19)
#define EDL_PATCH_TLV_REQ_CMD               (0x1E)
#define VSC_DISABLE_IBS_LEN                 (0x04)

/* VS Event */
#define EDL_CMD_REQ_RES_EVT                 (0x00)
#define EDL_CMD_EXE_STATUS_EVT           (0x00)
#define EDL_SET_BAUDRATE_RSP_EVT       (0x92)
#define EDL_PATCH_VER_RES_EVT             (0x19)
#define EDL_TVL_DNLD_RES_EVT                (0x04)
#define EDL_APP_VER_RES_EVT                  (0x02)


/* Status Codes of HCI CMD execution*/
#define HCI_CMD_SUCCESS                     (0x0)
#define PATCH_LEN_ERROR                       (0x1)
#define PATCH_VER_ERROR                       (0x2)
#define PATCH_CRC_ERROR                     (0x3)
#define PATCH_NOT_FOUND                      (0x4)
#define TLV_TYPE_ERROR                         (0x10)
#define NVM_ACCESS_CODE                     (0x0B)
#define BAUDRATE_CHANGE_SUCCESS   (1)

/* TLV_TYPE */
#define TLV_TYPE_PATCH                  (1)
#define TLV_TYPE_NVM                      (2)

/* NVM */
#define MAX_TAG_CMD                 30
#define TAG_END                           0xFF
#define NVM_ACCESS_SET            0x01
#define TAG_NUM_OFFSET             5
#define TAG_NUM_2                       2
#define TAG_BDADDR_OFFSET     7

#define MAX_RETRY_CNT  1
#define SELECT_TIMEOUT 3

/* NVM Tags specifically used for ROME 1.0 */
#define ROME_1_0_100022_1       0x101000221
#define ROME_1_0_100019           0x101000190
#define ROME_1_0_6002               0x100600200

/* Default NVM Version setting for ROME 1.0 */
#define NVM_VERSION                  ROME_1_0_100022_1


#define LSH(val, n)     ((unsigned int)(val) << (n))
#define EXTRACT_BYTE(val, pos)      (char) (((val) >> (8 * (pos))) & 0xFF)
#define CALC_SEG_SIZE(len, max)   ((plen) % (max))?((plen/max)+1) : ((plen) / (max))

#define ROME_FW_PATH        "/lib/firmware/qca/rampatch.img"
#define ROME_RAMPATCH_TLV_PATH      "/lib/firmware/qca/rampatch_tlv.img"
#define ROME_NVM_TLV_PATH         "/lib/firmware/qca/nvm_tlv.bin"
#define ROME_RAMPATCH_TLV_1_0_3_PATH    "/lib/firmware/qca/rampatch_tlv_1.3.tlv"
#define ROME_NVM_TLV_1_0_3_PATH         "/lib/firmware/qca/nvm_tlv_1.3.bin"
#define ROME_RAMPATCH_TLV_2_0_1_PATH    "/lib/firmware/qca/rampatch_tlv_2.1.tlv"
#define ROME_NVM_TLV_2_0_1_PATH         "/lib/firmware/qca/nvm_tlv_2.1.bin"
#define ROME_RAMPATCH_TLV_3_0_0_PATH    "/lib/firmware/qca/rampatch_tlv_3.0.tlv"
#define ROME_NVM_TLV_3_0_0_PATH         "/lib/firmware/qca/nvm_tlv_3.0.bin"
#define ROME_RAMPATCH_TLV_3_0_2_PATH    "/lib/firmware/qca/rampatch_tlv_3.2.tlv"
#define ROME_NVM_TLV_3_0_2_PATH         "/lib/firmware/qca/nvm_tlv_3.2.bin"

/* This header value in rampatch file decides event handling mechanism in the HOST */
#define ROME_SKIP_EVT_NONE     0x00
#define ROME_SKIP_EVT_VSE      0x01
#define ROME_SKIP_EVT_CC       0x02
#define ROME_SKIP_EVT_VSE_CC   0x03

/******************************************************************************
**  Local type definitions
******************************************************************************/

typedef struct {
    unsigned char     ncmd;
    unsigned short    opcode;
} __attribute__ ((packed)) evt_cmd_complete;

typedef struct {
    unsigned char     status;
    unsigned char     ncmd;
    unsigned short    opcode;
} __attribute__ ((packed)) evt_cmd_status;

typedef struct {
    unsigned short    opcode;
    unsigned char     plen;
} __attribute__ ((packed))  hci_command_hdr;

typedef struct {
    unsigned char     evt;
    unsigned char     plen;
} __attribute__ ((packed))  hci_event_hdr;
typedef struct {
    unsigned short rom_version;
    unsigned short build_version;
} __attribute__ ((packed)) patch_version;

typedef struct {
    unsigned int patch_id;
    patch_version patch_ver;
    unsigned int patch_base_addr;
    unsigned int patch_entry_addr;
    unsigned short patch_length;
    int patch_crc;
    unsigned short patch_ctrl;
} __attribute__ ((packed)) patch_info;

typedef struct {
    unsigned int  tlv_data_len;
    unsigned int  tlv_patch_data_len;
    unsigned char sign_ver;
    unsigned char sign_algorithm;
    unsigned char dwnd_cfg;
    unsigned char reserved1;
    unsigned short prod_id;
    unsigned short build_ver;
    unsigned short patch_ver;
    unsigned short reserved2;
    unsigned int patch_entry_addr;
} __attribute__ ((packed)) tlv_patch_hdr;

typedef struct {
    unsigned short tag_id;
    unsigned short tag_len;
    unsigned int tag_ptr;
    unsigned int tag_ex_flag;
} __attribute__ ((packed)) tlv_nvm_hdr;

typedef struct {
    unsigned char tlv_type;
    unsigned char tlv_length1;
    unsigned char tlv_length2;
    unsigned char tlv_length3;

    union{
        tlv_patch_hdr patch;
        tlv_nvm_hdr nvm;
    }tlv;
} __attribute__ ((packed)) tlv_patch_info;

enum{
    BAUDRATE_115200     = 0x00,
    BAUDRATE_57600       = 0x01,
    BAUDRATE_38400       = 0x02,
    BAUDRATE_19200       = 0x03,
    BAUDRATE_9600         = 0x04,
    BAUDRATE_230400     = 0x05,
    BAUDRATE_250000     = 0x06,
    BAUDRATE_460800     = 0x07,
    BAUDRATE_500000     = 0x08,
    BAUDRATE_720000     = 0x09,
    BAUDRATE_921600     = 0x0A,
    BAUDRATE_1000000   = 0x0B,
    BAUDRATE_1250000   = 0x0C,
    BAUDRATE_2000000   = 0x0D,
    BAUDRATE_3000000   = 0x0E,
    BAUDRATE_4000000   = 0x0F,
    BAUDRATE_1600000   = 0x10,
    BAUDRATE_3200000   = 0x11,
    BAUDRATE_3500000   = 0x12,
    BAUDRATE_AUTO        = 0xFE,
    BAUDRATE_Reserved  = 0xFF
};

enum{
    ROME_PATCH_VER_0100 = 0x0100,
    ROME_PATCH_VER_0101 = 0x0101,
    ROME_PATCH_VER_0200 = 0x0200,
    ROME_PATCH_VER_0300 = 0x0300,
    ROME_PATCH_VER_0302 = 0x0302
 };

enum{
    ROME_SOC_ID_00 = 0x00000000,
    ROME_SOC_ID_11 = 0x00000011,
    ROME_SOC_ID_13 = 0x00000013,
    ROME_SOC_ID_22 = 0x00000022,
    ROME_SOC_ID_44 = 0x00000044
};

enum{
    ROME_VER_UNKNOWN = 0,
    ROME_VER_1_0 = ((ROME_PATCH_VER_0100 << 16 ) | ROME_SOC_ID_00 ),
    ROME_VER_1_1 = ((ROME_PATCH_VER_0101 << 16 ) | ROME_SOC_ID_00 ),
    ROME_VER_1_3 = ((ROME_PATCH_VER_0200 << 16 ) | ROME_SOC_ID_00 ),
    ROME_VER_2_1 = ((ROME_PATCH_VER_0200 << 16 ) | ROME_SOC_ID_11 ),
    ROME_VER_3_0 = ((ROME_PATCH_VER_0300 << 16 ) | ROME_SOC_ID_22 ),
    ROME_VER_3_2 = ((ROME_PATCH_VER_0302 << 16 ) | ROME_SOC_ID_44 ),
    TUFELLO_VER_1_0 = ((ROME_PATCH_VER_0300 << 16 ) | ROME_SOC_ID_13 )
};
#endif /* HW_ROME_H */
