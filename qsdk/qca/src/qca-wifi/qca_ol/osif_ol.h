/*
 * Copyright (c) 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "ieee80211_api.h"
#if ATH_PERF_PWR_OFFLOAD
#include <ol_cfg_raw.h>
#include <osif_rawmode.h>
#include <ol_if_athvar.h>
#endif /* ATH_PERF_PWR_OFFLOAD */
#include "ol_ath.h"
#include <linux/ethtool.h>

struct tx_sniffer_meta_hdr {
	uint16_t ppdu_cookie;
};

#if ATH_PERF_PWR_OFFLOAD
void osif_vap_setup_ol(struct ieee80211vap *vap,
                   osif_dev *osifp);
#endif /* ATH_PERF_PWR_OFFLOAD */
extern int dp_extap_rx_process(struct wlan_objmgr_vdev *, struct sk_buff *, int *nwifi);
extern int dp_extap_tx_process(struct wlan_objmgr_vdev *, struct sk_buff **, uint8_t, struct dp_extap_nssol *);
#define ADP_EXT_AP_RX_PROCESS(_vdev, _skb, _nwifi) \
	dp_extap_rx_process(_vdev, _skb, _nwifi)
#define ADP_EXT_AP_TX_PROCESS(_vdev, _skb, _mhdr_len, _extap_nss) \
	dp_extap_tx_process(_vdev, _skb, _mhdr_len, _extap_nss)

#if ATH_SUPPORT_WRAP
extern int ol_wrap_rx_process (os_if_t *osif ,struct net_device **dev ,wlan_if_t vap, struct sk_buff *skb, int *nwifi);
#define OL_WRAP_RX_PROCESS(_osif, _dev, _vap, _skb, _nwifi) ol_wrap_rx_process(_osif, _dev, _vap, _skb, _nwifi)
#else  /* ATH_SUPPORT_WRAP */
#define OL_WRAP_RX_PROCESS(_osif, _dev, _vap, _skb, _nwifi) 0
#endif /* ATH SUPPORT_WRAP */

#if ATH_SUPPORT_WRAP
extern bool wlan_is_mpsta(wlan_if_t vaphandle);
extern bool wlan_is_psta(wlan_if_t vaphandle);
#endif /* ATH_SUPPORT_WRAP */


#if QCA_OL_VLAN_WAR
extern int _ol_tx_vlan_war(struct sk_buff **skb ,struct ol_ath_softc_net80211 *scn);
#define  OL_TX_VLAN_WAR(_skb, _scn)  _ol_tx_vlan_war(_skb, _scn)
#else
#define  OL_TX_VLAN_WAR(_skb, _scn)  0
#endif

#ifdef QCA_OL_DMS_WAR
extern int _ol_dms_amsdu_war(struct sk_buff **skb ,struct ol_ath_softc_net80211 *scn, wlan_if_t vap, uint8_t *peer_addr, struct cdp_tx_exception_metadata *tx_exc_param);
#define  OL_DMS_AMSDU_WAR(_skb, _scn, _vap, _peer, _tx_exc_param)  _ol_dms_amsdu_war(_skb, _scn, _vap, _peer, _tx_exc_param)
extern bool ol_check_valid_dms(struct sk_buff **skb, wlan_if_t vap, uint8_t *peer_addr);
#endif
