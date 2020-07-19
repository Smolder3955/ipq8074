/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2008 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */
 
/*
 * CWM (Channel Width Management) 
 *
 */
#ifndef _ATH_CWM_CB_H_
#define _ATH_CWM_CB_H_

#include "ath_cwm_project.h"

int ath_cwm_ht40allowed(struct ath_softc_net80211 *scn);
void ath_cwm_rate_updatenode(void *arg);
void ath_cwm_change_channel(struct ath_softc_net80211 *scn);
void ath_cwm_sendactionmgmt(struct ath_softc_net80211 *scn);
void ath_cwm_sendactionmgmt_aponly(struct ath_softc_net80211 *scn);
void ath_cwm_rate_updateallnodes(struct ath_softc_net80211 *scn);
int ath_cwm_ht40allowed(struct ath_softc_net80211 *scn);
void ath_cwm_set_macmode(struct ath_softc_net80211 *scn, enum cwm_ht_macmode cw_macmode);
void ath_cwm_set11nmac2040(struct ath_softc_net80211 *scn, enum cwm_ht_macmode cw_macmode);
void ath_cwm_set_curchannelflags_ht20(struct ath_softc_net80211 *scn);
void ath_cwm_set_curchannelflags_ht40(struct ath_softc_net80211 *scn);
void ath_cwm_stoptxdma(struct ath_softc_net80211 *scn);
void ath_cwm_resumetxdma(struct ath_softc_net80211 *scn);
void ath_cwm_requeue(void *arg);
void ath_cwm_process_tx_pkts(struct ath_softc_net80211 *scn);
void ath_cwm_requeue_tx_pkts(struct ath_softc_net80211 *scn);
enum cwm_phymode ath_cwm_get_curchannelmode(struct ath_softc_net80211 *scn);
enum cwm_opmode ath_cwm_get_icopmode(struct ath_softc_net80211 *scn);
int ath_cwm_get_extchbusyper(struct ath_softc_net80211 *scn);
int ath_cwm_get_wlan_scan_in_progress(struct ath_softc_net80211 *scn);
int ath_smart_ant_cwm_action(struct ath_softc_net80211 *scn);

/* Callback defines */
#define CWM_GET_IC_OPMODE_CB(g_cwm) ath_cwm_get_icopmode(g_cwm->wlandev)
#define CWM_RATE_UPDATENODE_CB(g_cwm, arg) ath_cwm_rate_updatenode(arg)
#define CWM_CHANGE_CHANNEL_CB(g_cwm) ath_cwm_change_channel(g_cwm->wlandev)
#define CWM_SENDACTIONMGMT_CB(g_cwm) ath_cwm_sendactionmgmt(g_cwm->wlandev)
#define CWM_SENDACTIONMGMT_APONLY_CB(g_cwm) ath_cwm_sendactionmgmt_aponly(g_cwm->wlandev)
#define CWM_RATE_UPDATEALLNODES_CB(g_cwm) ath_cwm_rate_updateallnodes(g_cwm->wlandev)
#define CWM_HT40_ALLOWED_CB(g_cwm) ath_cwm_ht40allowed(g_cwm->wlandev)
#define CWM_SET_MACMODE_CB(g_cwm, mode) ath_cwm_set_macmode(g_cwm->wlandev, mode)
#define CWM_SET_CURCHAN_FLAGS_HT20_CB(g_cwm) ath_cwm_set_curchannelflags_ht20(g_cwm->wlandev)
#define CWM_SET_CURCHAN_FLAGS_HT40_CB(g_cwm) ath_cwm_set_curchannelflags_ht40(g_cwm->wlandev)
#define CWM_GET_CURCH_MODE_CB(g_cwm) ath_cwm_get_curchannelmode(g_cwm->wlandev)
#define CWM_GET_EXTCHBUSYPER_CB(g_cwm) ath_cwm_get_extchbusyper(g_cwm->wlandev)
#define CWM_GET_WLAN_SCAN_IN_PROGRESS_CB(g_cwm) ath_cwm_get_wlan_scan_in_progress(g_cwm->wlandev)
#define CWM_SMART_ANT_ACTION(g_cwm) ath_smart_ant_cwm_action(g_cwm->wlandev)

#ifndef ATH_CWM_MAC_DISABLE_REQUEUE
#define CWM_STOPTXDMA_CB(g_cwm) ath_cwm_stoptxdma(g_cwm->wlandev)
#define CWM_RESUMETXDMA_CB(g_cwm) ath_cwm_resumetxdma(g_cwm->wlandev)
#define CWM_REQUEUE_CB(g_cwm, arg) ath_cwm_requeue(arg)
#define CWM_PROCESS_TX_PKTS_CB(g_cwm) ath_cwm_process_tx_pkts(g_cwm->wlandev)
#define CWM_REQUEUE_TX_PKTS_CB(g_cwm) ath_cwm_requeue_tx_pkts(g_cwm->wlandev)
#endif
 



#endif


