/*
 * Copyright (c) 2014, 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 * 2014 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*
 * This file contains the definitions for wmiconfig utility
 *
 * $Id: //depot/sw/branches/qca_main_wifi2.0/perf_pwr_offload/drivers/host/tools/systemtools/testcmd_tlv/athtestcmd.h#2 $
 */

#ifndef _TCMD_H_
#define _TCMD_H_

#ifdef __cplusplus
extern "C" {
#endif

enum {
    TESTMODE_CONT_TX = 801,     /* something that doesn't collide with ascii */
    TESTMODE_CONT_RX,
    TESTMODE_PM,
    TESTMODE_SETLPREAMBLE,
    TESTMODE_SETREG,
    TESTMODE_CMDS,
};
     
enum {
    TCMD_SET_RX_LPL=501,     /* something that doesn't collide with ascii */
    TCMD_EFUSE_START=502,
    TCMD_EFUSE_END=503,
    TCMD_EFUSE_DATA=504,
    TCMD_EFUSE_DUMP=505,
    TCMD_EFUSE_WRITE=506,
    TCMD_OTP_DUMP=507,
    TCMD_OTP_WRITE=508,
    TCMD_READ_THERMAL=509,
    TCMD_SET_BT_MODE=510,
    TCMD_TX_FORCED_GAIN=511,
    TCMD_TX_FORCED_GAINIDX=512,
    TCMD_TX_DAC_GAIN=513,
    TCMD_CAL=514,
    TCMD_TX_PA_CONFIG=515,
    TCMD_EEPROM_DUMP=516,
    TCMD_EEPROM_WRITE=517,
    TCMD_EEPROM_TOTAL_SIZE=518,
    TCMD_EEPROM_OFFSET=519,
    TCMD_EEPROM_BLOCK_SIZE=520,
    TCMD_EEPROM_BLOCK_DATA = 521,
    TCMD_TX_STATUS=522,
    TCMD_HWCAL=523,
    TCMD_LOOP=524,
    TCMD_CHAIN_MASK=525,
    TCMD_SETUP=526,
    TCMD_GO=527,
    TCMD_REG_READ=528,
    TCMD_REG_WRITE=529,
    TCMD_ADDR=530,
    TCMD_VALUE=531,
    TCMD_GET_NV_MAC_ADDRESS = 532,
    TCMD_SET_NV_MAC_ADDRESS = 533,
    TCMD_COMMIT_NV_BDATA = 534,
    TCMD_COMMIT_NV_BDATA_TYPE = 535,
    TCMD_COMMIT_NV_BDATA_SEGMENT = 536,
    TCMD_COMMIT_NV_BDATA_COMPRESSED =537,
    TCMD_GET_CAL_BDATA = 543,
    TCMD_GET_CAL_BDATA_SIZE = 544,
    TCMD_CAL_LM = 545,
    TCMD_CAL_PWR = 546,
    TCMD_LOOPBACK = 547,
    TCMD_SAVE_CAL = 548,
    TCMD_NF_CAL = 549,
    TCMD_EEPROM_ERASE=550,
    TCMD_EEPROM_CHECK=551,
    TCMD_OTP_RESET=552,
    TCMD_MEM_READ                  = 553,
    TCMD_MEM_WRITE                 = 554,
    TCMD_NUMBYTES                  = 555,
    TCMD_VALUETYPE                 = 556,  
    TCMD_GET_TX_OFFLINE_POWER      = 557, 
    TCMD_CAL_XTAL                  = 558,
    TCMD_CONFIG                    = 559,
    TCMD_CALORDER                  = 560,
    TCMD_PORT                      = 561,
    TCMD_PHYID                     = 562,
    TCMD_SETPHYRFMODE              = 563,
    TCMD_SYNTH                     = 564,
    TCMD_STOPRX                    = 565,
    TCMD_STICKYWRITE               = 566,
    TCMD_STICKYCLEAR               = 567,
    TCMD_REG_MASK                  = 568,
    TCMD_STICKY_PREPOST            = 569,
    TCMD_SETREGDMN                 = 570,
    TCMD_CMD                       = 571,
    TCMD_PARAM                     = 572,
    TCMD_VAL                       = 573,
};

#define BOARD_DATA_SZ_MAX  ((0x1) << 15)
#define _MAX_FILENAME_LEN	255

#ifdef __cplusplus
}
#endif

#endif /* _TCMD_H_ */

