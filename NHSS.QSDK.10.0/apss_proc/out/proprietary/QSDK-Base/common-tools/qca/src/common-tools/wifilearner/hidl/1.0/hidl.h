/*
 * Copyright (c) 2018 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef WIFILEARNER_HIDL_HIDL_H
#define WIFILEARNER_HIDL_HIDL_H

#ifdef __cplusplus
extern "C"
{
#endif  // _cplusplus
#include "wifilearner.h"

int wifilearner_hidl_process(struct wifilearner_ctx *wlc);

#ifdef __cplusplus
}
#endif  // _cplusplus

#endif  // WIFILEARNER_HIDL_HIDL_H
