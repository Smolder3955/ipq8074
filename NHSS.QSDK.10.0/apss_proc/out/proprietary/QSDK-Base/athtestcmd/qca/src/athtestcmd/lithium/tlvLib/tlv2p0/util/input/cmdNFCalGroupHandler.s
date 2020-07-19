/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#ifndef TLV2_MAX_CHAIN
#define TLV2_MAX_CHAIN 16
#endif

# cmd
CMD= NFCalGroup

# cmd parm
PARM_START:
UINT8:cmdIdNFCalGroup:1:u:0
UINT8:phyId:1:u:0
UINT8:bandCode:1:u:0
UINT8:bwCode:1:u:0

UINT16:freq:1:u:5180
UINT16:txChMask:1:x:0x3

UINT16:rxChMask:1:x:0x3
UINT8:concMode:1:u:1
UINT8:pad1:1:u

UINT32:userParm1:1:u:0
UINT32:userParm2:1:u:0

UINT32:nfCalResults:TLV2_MAX_CHAIN:16:u

PARM_END:

# rsp
RSP= NFCalGroupRsp
# rsp param
PARM_START:
UINT8:phyId:1:u:0
UINT8:status:1:u:0
UINT8:miscFlags:1:u:0
UINT8:pad1:1:u

UINT32:nfCalResults:TLV2_MAX_CHAIN:16:u

UINT32:executionTime:1:u:0

PARM_END: