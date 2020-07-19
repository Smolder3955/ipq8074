/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

CMD= LMRXSTATUS

# cmd parm
PARM_START:
UINT8:lm_flag:1:u:0
UINT8:lm_itemNum:1:u:0
UINT8:pad2:2:u:0
PARM_END:

# rsp

RSP= LMRXSTATUSRsp
# rsp parm
PARM_START:
UINT8:lm_phyId:100:u
UINT32:lm_totalGoodPackets:100:u:0:repeat:99:1:0
UINT32:lm_goodPackets:100:u:0:repeat:99:1:0
UINT32:lm_otherErrors:100:u:0:repeat:99:1:0
UINT32:lm_crcPackets:100:u:0:repeat:99:1:0
UINT32:lm_decrypErrors:100:u:0:repeat:99:1:0
UINT32:lm_rateBit:100:u:0:repeat:99:1:0
UINT32:lm_statTime:100:u:0:repeat:99:1:0
UINT32:lm_endTime:100:u:0:repeat:99:1:0
UINT32:lm_byteCount:100:u:0:repeat:99:1:0
UINT32:lm_dontCount:100:u:0:repeat:99:1:0
UINT32:lm_rssi:100:s:0:repeat:99:1:0
PARM_END:
