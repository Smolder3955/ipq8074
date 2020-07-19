/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= DACPlybck

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:DACPlybckMode:1:u
UINT8:DACPlybckOptions:1:u
UINT8:DACPlybckRadarOnLen:1:u
UINT8:DACPlybckRadarOffLen:1:u
UINT8:pad3:3:u
UINT16:chainMask:1:x:0x1
UINT16:DACPlybckCount:1:u
DATA4096:data4k:4096:x
PARM_END:

# rsp
RSP= DACPlybckRsp

# rsp parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:status:1:u:0
PARM_END:
