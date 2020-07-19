/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= HwCal

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:calOp:1:u:0
UINT8:calOpFlag:1:x:0
UINT8:loopCnt:1:u:1
UINT16:chainMask:1:x:0x3
UINT8:loopBack:1:u:0
UINT8:saveCal:1:u:0
UINT8:noiseFloorCal:1:u:0
UINT8:calOpOrder:30:u
PARM_END:
