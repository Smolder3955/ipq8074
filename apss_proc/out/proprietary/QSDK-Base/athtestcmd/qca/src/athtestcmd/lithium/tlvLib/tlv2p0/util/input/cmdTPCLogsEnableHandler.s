/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= TPCLogsEnable

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:enable:1:u:0
PARM_END:

# cmd
RSP= TPCLogsEnableRsp

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:status:1:u:0
PARM_END:
