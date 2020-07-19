/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
#ifdef ATH_HTC_MII_RXIN_TASKLET
#include "htc_thread.h"
#endif
//#include "if_upperproto.h"
#include <if_llc.h>
#include <if_athproto.h>
#include <if_athvar.h>
#include <osdep.h>
#include <ieee80211_ald.h>
#include <ieee80211_crypto.h>
#include <ieee80211_node.h>
#include <ieee80211_objmgr_priv.h>
#ifdef QCA_PARTNER_PLATFORM
#if ATH_TX_COMPACT && !ATH_SUPPORT_FLOWMAC_MODULE
#include <da_aponly.h>
#endif
#endif
#if ATH_SW_WOW
#include <ieee80211_wow.h>
#endif
#include "ieee80211_vi_dbg.h"
#include "wlan_mgmt_txrx_utils_api.h"
#include <wlan_son_pub.h>

int wlan_vdev_get_curmode(struct wlan_objmgr_vdev *vdev);
u_int8_t wlan_pdev_get_qwrap_isolation(struct wlan_objmgr_pdev *pdev);
int
wlan_vdev_wds_sta_chkmcecho(struct wlan_objmgr_vdev *vdev,
			    const u_int8_t * sender);
struct ieee80211_ath_channel *wlan_pdev_get_curchan(struct wlan_objmgr_pdev
						    *pdev);
void wlan_wnm_bssmax_updaterx(struct wlan_objmgr_peer *peer, int secured);
void wlan_peer_leave(struct wlan_objmgr_peer *peer);
void wlan_peer_set_beacon_rstamp(struct wlan_objmgr_peer *peer);
void wlan_peer_set_probe_ticks(struct wlan_objmgr_peer *peer, int val);
u_int8_t wlan_vdev_get_lastbcn_phymode_mismatch(struct wlan_objmgr_vdev *vdev);
int wlan_vdev_get_bmiss_count(struct wlan_objmgr_vdev *vdev);
void wlan_vdev_reset_bmiss(struct wlan_objmgr_vdev *vdev);
void
_ath_rxstat2ieee(struct ath_softc_net80211 *scn,
		 ieee80211_rx_status_t * rx_status,
		 struct ieee80211_rx_status *rs);
#ifndef ATHHTC_AP_REMOVE_STATS
#define ATH_RXSTAT2IEEE(_scn, _rx_status, _rs)  _ath_rxstat2ieee(_scn, _rx_status, _rs)
#else
#define ATH_RXSTAT2IEEE(_scn, _rx_status, _rs)    (_rs)->rs_flags=0
#endif
int wlan_input(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
	       ieee80211_rx_status_t * rx_status);
