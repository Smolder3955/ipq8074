/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
#ifndef _IF_ATH_MAT_H_
#define _IF_ATH_MAT_H_

#include "wbuf.h"
#include "if_athvar.h"

int ath_wrap_mat_rx(struct wlan_objmgr_vdev *in, wbuf_t m);
int ath_wrap_mat_tx(struct wlan_objmgr_vdev *out, wbuf_t m);
#endif
