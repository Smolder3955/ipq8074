/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= dpdTrainingQuality

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
PARM_END:

# rsp
RSP= dpdTrainingQualityRsp

# rsp parm
PARM_START:
UINT16:DPDTrainQual:1:u:0
UINT8:status:1:u:0
UINT8:phyId:1:u:0
PARM_END:
