/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= CalCalTime

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:calOp:1:u:0
PARM_END:

# rsp
RSP= CalCalTimeRsp

# rsp parm
PARM_START:
UINT32:elapsedTime:1:u:0
UINT8:status:1:u:0
UINT8:phyId:1:u:0
PARM_END:

