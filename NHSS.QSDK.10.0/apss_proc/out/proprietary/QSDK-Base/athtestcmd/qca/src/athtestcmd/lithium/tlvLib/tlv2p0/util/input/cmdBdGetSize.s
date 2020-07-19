/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= BdGetSize

# cmd parm
PARM_START:
PARM_END:

# rsp
RSP= BdGetSizeRsp

# rsp parm
PARM_START:
UINT8:status:1:u:0
UINT8:pad3:3:u
UINT32:bdSize:1:u:0
PARM_END:

