/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#
# tlv capture control
#

# cmd
SYSCMD= tlvCaptureCtrl

# cmd parm
PARM_START:
UINT32:tlvCaptureCtrl_cmdID:1:u:0
UINT32:tlvCaptureCtrl_01:1:u:0
UINT32:tlvCaptureCtrl_02:1:u:0
UINT32:tlvCaptureCtrl_03:1:u:0
UINT32:tlvCaptureCtrl_04:1:u:0
UINT8:phyId:1:u:0
PARM_END:

# rsp
SYSRSP= tlvCaptureCtrlRsp


# cmd parm
PARM_START:
UINT32:tlvCaptureCtrl_cmdID:1:u:0
UINT32:tlvCaptureCtrl_01:1:u:0
UINT32:tlvCaptureCtrl_02:1:u:0
UINT32:tlvCaptureCtrl_03:1:u:0
UINT32:tlvCaptureCtrl_04:1:u:0
UINT8:phyId:1:u:0
PARM_END:
