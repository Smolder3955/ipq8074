/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= GenWaveform

# cmd parm
PARM_START:
UINT16:waveformIndex:1:u:0
UNIT16:waveformAmp:1:u:0
UINT8:phyId:1:u:0
PARM_END:

# rsp
RSP= GenWaveformRsp

# rsp parm
PARM_START:
UINT16:dataI:2048:x
UINT16:dataQ:2048:x
UINT8:status:1:u:0
UINT8:phyId:1:u:0
PARM_END:

