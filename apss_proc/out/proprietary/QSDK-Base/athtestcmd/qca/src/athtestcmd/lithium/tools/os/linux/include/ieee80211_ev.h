/*
* Copyright (c) 2017 Qualcomm Technologies, Inc.
*
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*
*/

struct ev_msg {
    u_int8_t addr[6];
    u_int32_t status;
    u_int32_t reason;
};
