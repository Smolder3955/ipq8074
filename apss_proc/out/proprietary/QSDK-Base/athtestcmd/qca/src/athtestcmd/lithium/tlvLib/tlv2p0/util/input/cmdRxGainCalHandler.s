/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= rxgaincal

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:chainIdx:1:u:0
UINT8:band:1:u:0
UINT8:pad1:1:u
PARM_END:

# rsp
RSP= rxgaincalrsp
# rsp param
PARM_START:
UINT8:phyId:1:u:0
UINT8:status:1:u:0
UINT8:band:1:u:0
INT8:refISS:1:u:0
UINT8:rate:1:u:0
UINT8:bandWidth:1:u:0
UINT8:chanIdx:1:u:0
UINT8:chainIdx:1:u:0
UINT16:numPackets:1:u:0
UINT8:pad2:2:u
PARM_END:
