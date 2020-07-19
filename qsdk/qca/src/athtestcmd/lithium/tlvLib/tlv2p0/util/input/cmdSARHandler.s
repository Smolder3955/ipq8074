/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= SAR

# cmd parm
PARM_START:
UINT16:cmdId:1:u:0
UINT8:CCK2gLimit:1:u:0
UINT8:Ofdm2gLimit:1:u:0

UINT8:Ofdm5gLimit:1:u:0
UINT8:chain:1:u:0
UINT8:index8:1:u:0
PARM_END:

# rsp
RSP= SARRsp

# rsp parm
PARM_START:
UINT8:status:1:u:0
UINT8:CCK2gLimit:1:u:0
UINT8:Ofdm2gLimit:1:u:0
UINT8:Ofdm5gLimit:1:u:0
PARM_END:

