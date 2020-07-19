/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#ifndef CMD_EFUSE_NUMBYTES_MAX
#define CMD_EFUSE_NUMBYTES_MAX 256
#endif

# cmd
CMD= EfuseWrite

# cmd parm
PARM_START:
UINT32:offset:1:x:0
UINT16:numBytes:1:u:0
UINT8:efuseData:CMD_EFUSE_NUMBYTES_MAX:256:x
PARM_END:

# rsp
RSP= EfuseWriteRsp

# rsp parm
PARM_START:
UINT8:status:1:u:0
PARM_END:
