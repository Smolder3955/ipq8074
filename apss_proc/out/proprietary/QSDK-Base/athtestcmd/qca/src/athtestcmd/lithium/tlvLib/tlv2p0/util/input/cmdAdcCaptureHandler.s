/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= AdcCapture

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:adcCompMode:1:u:0     #ADC Mode and Compression (bits 1-2 is mode, LSB bit is compression)
UINT16:chainMask:1:x:0x1    #Bitmap of chain to capture from
UINT8:adcTriggerMode:1:u:0  #What should capture trigger on?
PARM_END:

# rsp
RSP= AdcCaptureRsp

# rsp parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:status:1:u:0
PARM_END:

