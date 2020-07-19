/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#ifndef CMD_MEM_NUMBYTES_MAX
#define CMD_MEM_NUMBYTES_MAX	256
#endif

# cmd
CMD= MemRead

# cmd parm
PARM_START:
UINT32:memaddress:1:x:0
UINT16:numbytes:1:u:0
UINT8:valuetype:1:u:1
PARM_END:

# rsp
RSP= MemReadRsp

# rsp parm
PARM_START:
UINT32:memaddress:1:x:0
UINT16:numbytes:1:u:0
UINT8:valuetype:1:u:1
UINT8:memvalue:CMD_MEM_NUMBYTES_MAX:256:x
UINT8:status:1:u:0
PARM_END:

