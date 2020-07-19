/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

# cmd
CMD= EnableDfe

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:enable:1:u:1
PARM_END:

# cmd
RSP= EnableDfeRsp

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:status:1:u:0
PARM_END:
