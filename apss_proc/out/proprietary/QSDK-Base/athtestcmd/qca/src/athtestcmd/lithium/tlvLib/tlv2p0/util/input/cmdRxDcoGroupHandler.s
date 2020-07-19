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
CMD= RxDcoGroup

# cmd parm
PARM_START:
UINT8:cmdIdRxDcoGroup:1:u:0
UINT8:phyId:1:u:0
UINT8:bandCode:1:u:0
UINT8:bwCode:1:u:0

UINT16:chainMask:1:x:0x3
UINT16:rxdco_cal_mode:1:x:

UINT8:homeCh:1:u:0
UINT8:initLUT:1:u:0
UINT8:verbose:1:u:0
UINT8:pad1:1:u

UINT16:bbOvrdI:1:u:256
UINT16:bbOvrdQ:1:u:256
UINT16:rfOvrdI:1:u:256
UINT16:rfOvrdQ:1:u:256

UINT32:userParm1:1:u:0
UINT32:userParm2:1:u:0
UINT32:userParm3:1:u:0
UINT32:userParm4:1:u:0

UINT8:bbDcocRange0I:1:u:0
UINT8:bbDcocRange0Q:1:u:0
UINT8:rfDcocRange0I:1:u:0
UINT8:rfDcocRange0Q:1:u:0

DATA256:corrRF_DCO_LUT_i:256:x
DATA256:corrRF_DCO_LUT_q:256:x
DATA256:corrBB_DCO_LUT_i:256:x
DATA256:corrBB_DCO_LUT_q:256:x

PARM_END:

# rsp
RSP= RxDcoGroupRsp
# rsp param
PARM_START:
UINT8:phyId:1:u:0
UINT8:pad3:3:u

UINT8:calStatus:TLV2_MAX_CHAIN:16:u

UINT8:bbDcocRange0I:1:u:0
UINT8:bbDcocRange0Q:1:u:0
UINT8:rfDcocRange0I:1:u:0
UINT8:rfDcocRange0Q:1:u:0

DATA256:corrRF_DCO_LUT_i:256:x
DATA256:corrRF_DCO_LUT_q:256:x
DATA256:corrBB_DCO_LUT_i:256:x
DATA256:corrBB_DCO_LUT_q:256:x
PARM_END: