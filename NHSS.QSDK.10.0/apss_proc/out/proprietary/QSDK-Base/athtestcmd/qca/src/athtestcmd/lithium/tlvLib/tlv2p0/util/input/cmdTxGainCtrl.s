/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= TxGainCtrl

# cmd parm
PARM_START:
UINT8:phyId:1:u:0           #Phy ID
UINT8:gainIdx:1:u:0         #Analog gain idx (16 possibilities)
UINT16:chainMask:1:x:0x1    #Chain Mask
INT8:dacGain:1:u:0         #Digital gain
UINT8:forceCal:1:u:0        #0/1, if set, applies anaGain to forced cal gain
PARM_END:

# rsp
RSP= TxGainCtrlRsp

# rsp parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:status:1:u:0
PARM_END:
PARM_DACGAIN
