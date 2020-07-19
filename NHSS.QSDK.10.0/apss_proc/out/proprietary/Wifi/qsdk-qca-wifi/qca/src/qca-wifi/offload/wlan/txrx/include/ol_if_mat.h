/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _IF_ATH_MAT_H_
#define _IF_ATH_MAT_H_

#include "wbuf.h"
#include "ol_if_athvar.h"

int ol_if_wrap_mat_rx(struct ieee80211vap *in, wbuf_t m);
int ol_if_wrap_mat_tx(struct ieee80211vap * out, wbuf_t m);
#endif
