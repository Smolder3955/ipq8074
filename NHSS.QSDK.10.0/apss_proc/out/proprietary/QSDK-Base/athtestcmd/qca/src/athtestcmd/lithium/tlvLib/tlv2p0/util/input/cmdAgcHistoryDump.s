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
CMD= AgcHistoryDump

# cmd parm
PARM_START:
UINT32:AGCHistorydumpOption:1:u:1
UINT8:phyId:1:u:0
PARM_END:

# rsp
RSP= AgcHistoryDumpRsp

# rsp parm
PARM_START:
UINT8:AGCHistoryData:320:x
UINT8:status:1:u:0
UINT8:phyId:1:u:0
PARM_END:
