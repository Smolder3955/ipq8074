/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#
# This is a sample .s file, not a real command
#

# cmd
CMD= PHYDBGdump

# cmd parm
PARM_START:
UINT32:PHYDBGdumpOption:1:u:1
UINT8:phyId:1:u:0
PARM_END:

# rsp
RSP= PHYDBGdumpRsp

# rsp parm
PARM_START:
DATA4096:data4k:4096:x
UINT8:status:1:u:0
UINT8:phyId:1:u:0
PARM_END:
