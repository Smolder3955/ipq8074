/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= AgcHistoryConfig

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:agcControl:1:u:0		#1=enable, 0=disable
PARM_END:

# rsp
RSP= AgcHistoryConfigRsp

# rsp parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:status:1:u:0
PARM_END:
