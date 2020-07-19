/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= RegWrite

# cmd parm
PARM_START:
UINT32:regaddress:1:x:0
UINT32:regvalue:1:x:0
UINT8:radioId:1:u:0
PARM_END:

# rsp
RSP= RegWriteRsp

# rsp parm
PARM_START:
UINT8:radioId:1:u:0
UINT8:status:1:u:0
PARM_END:
