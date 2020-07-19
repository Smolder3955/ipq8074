/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= GetRssi

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:chain:1:u:0
PARM_END:

# rsp
RSP= GetRssiRsp

# rsp parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:status:1:u:0
INT32:rssi:1:d:0

PARM_END:

