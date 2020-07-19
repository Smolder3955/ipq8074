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
CMD= EfuseRead

# cmd parm
PARM_START:
UINT32:offset:1:x:0
UINT16:numBytes:1:u:0
PARM_END:

# rsp
RSP= EfuseReadRsp

# rsp parm
PARM_START:
UINT16:numBytes:1:u:0
UINT8:efuseData:CMD_EFUSE_NUMBYTES_MAX:256:x
UINT8:status:1:u:0
PARM_END:

