/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= RxGainCtrl

# cmd parm
PARM_START:
UINT8:phyId:1:u:0			#Phy ID
UINT8:xlnaCtrl:1:u:0		#1 = ON, 0 = OFF
UINT8:gainCtrlMode:1:u:0	#1 = AGC ON, 0 = AGC OFF
UINT8:band:1:u:0			#2G or 5G
UINT8:gainIdx:1:u:0			#index into gain table
PARM_END:

# rsp
RSP= RxGainCtrlRsp

# rsp parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:status:1:u:0
PARM_END:
