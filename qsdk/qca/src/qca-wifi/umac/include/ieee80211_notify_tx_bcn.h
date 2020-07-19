/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2005 Atheros Communications Inc.  All rights reserved.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#ifndef _IEEE80211_NOTIFY_TX_BCN_H
#define _IEEE80211_NOTIFY_TX_BCN_H

struct ieee80211_tx_bcn_notify;
typedef struct ieee80211_tx_bcn_notify *ieee80211_tx_bcn_notify_t;

typedef void (*ieee80211_tx_bcn_notify_func)(void *arg,
                                             int beacon_id, u_int32_t tx_status);

struct ieee80211_notify_tx_bcn_mgr;
typedef struct ieee80211_notify_tx_bcn_mgr *ieee80211_notify_tx_bcn_mgr_t;

typedef void (*tx_bcn_notify_func)(ieee80211_tx_bcn_notify_t h_notify, 
                                   int beacon_id, u_int32_t tx_status,
                                   void *arg);

#if UMAC_SUPPORT_P2P

ieee80211_tx_bcn_notify_t
ieee80211_reg_notify_tx_bcn(
    ieee80211_notify_tx_bcn_mgr_t   h_mgr,
    tx_bcn_notify_func              callback,
    int                             beacon_id,
    void                            *arg);

void
ieee80211_dereg_notify_tx_bcn(
    ieee80211_tx_bcn_notify_t   h_notify);

ieee80211_notify_tx_bcn_mgr_t ieee80211_notify_tx_bcn_attach(struct ieee80211com *ic);
void ieee80211_notify_tx_bcn_detach(ieee80211_notify_tx_bcn_mgr_t h_mgr);

#else   //if UMAC_SUPPORT_P2P

/* Benign Functions */
#define ieee80211_notify_tx_bcn_attach(ic)      NULL
static INLINE void ieee80211_notify_tx_bcn_detach(ieee80211_notify_tx_bcn_mgr_t h_mgr)
{
}

#endif  //if UMAC_SUPPORT_P2P

#endif  //_IEEE80211_NOTIFY_TX_BCN_H

