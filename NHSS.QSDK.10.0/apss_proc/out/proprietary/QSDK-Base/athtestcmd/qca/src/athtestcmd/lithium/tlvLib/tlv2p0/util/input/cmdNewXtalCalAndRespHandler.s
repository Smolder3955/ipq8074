/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= NEWXTALCALPROC

# cmd parm
PARM_START:
UINT16:newCapIn:1:u:639
UINT16:newCapOut:1:u:255
UINT8:ctrlFlag:1:u:0
UINT8:phyId:1:u:0
UINT8:band:1:u:0
UINT8:pad1:1:u
PARM_END:

# rsp
RSP= NEWXTALCALPROCRSP

# rsp parm
PARM_START:
UINT8:status:1:u:0
UINT8:pllLocked:1:u:0
UINT8:phyId:1:u:0
UINT8:band:1:u:0
UINT16:capInValMin:1:u:0
UINT16:capInValMax:1:u:0
UINT16:capOutValMin:1:u:0
UINT16:capOutValMax:1:u:0
UINT16:newCapIn:1:u:639
UINT16:newCapOut:1:u:255
PARM_END:
