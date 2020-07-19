/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= readCustOtpSpace

# cmd parm
PARM_START:
PARM_END:

# rsp
RSP= readCustOtpSpaceRsp

# rsp parm
PARM_START:
UINT8:custData:24:u
PARM_END:

