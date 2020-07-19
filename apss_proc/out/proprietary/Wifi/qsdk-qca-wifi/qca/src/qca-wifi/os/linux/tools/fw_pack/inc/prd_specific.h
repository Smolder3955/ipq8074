/*
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef __PRD_SPECIFC_H_
#define __PRD_SPECIFC_H_

#include "fw_osdep.h"
/* known product magic numbers */
#define FW_IMG_MAGIC_BEELINER       0x424c4e52U                      /* BLNR */
#define FW_IMG_MAGIC_CASCADE        0x43534345U                      /* CSCE */
#define FW_IMG_MAGIC_SWIFT          0x53574654U                      /* SWFT */
#define FW_IMG_MAGIC_PEREGRINE      0x5052474eU                      /* PRGN */
#define FW_IMG_MAGIC_DAKOTA         0x44414b54U                      /* DAKT */
#define FW_IMG_MAGIC_UNKNOWN        0x00000000U                      /* zero*/
/* chip idenfication numbers
 * Most of the times chip identifier is pcie device id. In few cases that could
 * be a non-pci device id if the device is not pci device. It is assumed at 
 * at build time, it is known to the tools for which the firmware is getting 
 * built.
 */
typedef struct _fw_device_id{
    uint32      dev_id;                  /* this pcieid or internal device id */
    char       *dev_name;                    /* short form of the device name */
    uint32      img_magic;                    /* image magic for this product */
} fw_device_id;

const static fw_device_id fw_auth_supp_devs[] =
    { 
        {0x3Cu, "PEREGRINE",    FW_IMG_MAGIC_PEREGRINE},
        {0x50u, "SWIFT",        FW_IMG_MAGIC_SWIFT},
        {0x40u, "BEELINER",     FW_IMG_MAGIC_BEELINER},
        {0x46u, "CASCADE",      FW_IMG_MAGIC_CASCADE},
        {0x12ef,"DAKOTA",       FW_IMG_MAGIC_DAKOTA},
        {0x0u,  "UNSUPP",       FW_IMG_MAGIC_UNKNOWN}
    };
#endif
