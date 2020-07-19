/*
 * Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2005 Atheros Communications Inc. All rights reserved.
 */


#ifndef _IEEE80211_TXBF_H
#define _IEEE80211_TXBF_H

#include <osdep.h>

struct ieee80211_txbfcal_sm;
typedef struct ieee80211_txbfcal_sm *ieee80211_txbfcal_sm_t;

#ifdef ATH_SUPPORT_TxBF
#define TXBF_CV_REPORT_TIMEOUT 5    /* 5ms for CV report time out after sounding frame*/
#define TXBF_CV_RETRY_LIMIT 3       /* set CV retry limit to 3*/

void ieee80211_set_TxBF_keycache(struct ieee80211com *ic, struct ieee80211_node *ni);
void ieee80211_match_txbfcapability(struct ieee80211com *ic, struct ieee80211_node *ni);
void ieee80211_del_TxBF_keycache(struct ieee80211vap *vap, struct ieee80211_key *key, struct ieee80211_node *ni);
void ieee80211_request_cv_update(struct ieee80211com *ic,struct ieee80211_node *ni, wbuf_t wbuf, int use4addr);
int	ieee80211_send_cal_qos_nulldata(struct ieee80211_node *ni,int Cal_type);// for TxBF RC
OS_TIMER_FUNC(txbf_cv_timeout);
void ieee80211_init_txbf(struct ieee80211com *ic,struct ieee80211_node *ni);
#endif /* ATH_SUPPORT_TxBF */

#endif /* _IEEE80211_TXBF_H */
