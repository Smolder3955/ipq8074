/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_objmgr_priv.h>
#include <reg_services_public_struct.h>
#include <ieee80211_rateset.h>
#include <ieee80211_node_priv.h>
#include <wlan_mlme_dp_dispatcher.h>
#include <ieee80211_ucfg.h>
#include <_ieee80211.h>
#include <osif_private.h>
#include <osif_wrap_private.h>
#include <ieee80211_wds.h>
#include <ieee80211_vi_dbg.h>

#ifdef ATH_SUPPORT_TxBF
void ieee80211_tx_bf_completion_handler(struct ieee80211_node *ni,
					struct ieee80211_tx_status *ts);
#endif

/* Functions common to partial OL and DA */
int dbdc_tx_process(wlan_if_t vap, osif_dev ** osdev, struct sk_buff *skb);

osif_dev *osif_wrap_wdev_find(struct wrap_devt *wdt, unsigned char *mac);

int wlan_wnm_tfs_filter(wlan_if_t vaphandle, wbuf_t wbuf);

#if UMAC_SUPPORT_PROXY_ARP
int do_proxy_arp(wlan_if_t vap, qdf_nbuf_t netbuf);
#endif

void ieee80211_sta_power_tx_start(struct ieee80211vap *vap);
/* ==================== DA DP calls from DADP to UMAC ==================== */

/********************* DA specific components still in UMAC *********************/

ieee80211_pwrsave_mode wlan_vdev_get_powersave(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return -EINVAL;
	}
	return wlan_get_powersave(vap);
}

EXPORT_SYMBOL(wlan_vdev_get_powersave);

u_int16_t wlan_pdev_get_curchan_freq(struct wlan_objmgr_pdev * pdev)
{
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return 0;
	}
	return ieee80211_chan2freq(ic, ic->ic_curchan);
}

EXPORT_SYMBOL(wlan_pdev_get_curchan_freq);

struct ieee80211_ath_channel *wlan_pdev_get_curchan(struct wlan_objmgr_pdev
						    *pdev)
{
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return NULL;
	}
	return ic->ic_curchan;
}

EXPORT_SYMBOL(wlan_pdev_get_curchan);

struct ieee80211_ath_channel *wlan_pdev_find_channel(struct wlan_objmgr_pdev
						     *pdev, int freq)
{
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return NULL;
	}
	return ieee80211_find_channel(ic, freq,
				      ic->ic_curchan->ic_vhtop_ch_freq_seg2,
				      ic->ic_curchan->ic_flags);
}

EXPORT_SYMBOL(wlan_pdev_find_channel);

#if ATH_SUPPORT_WRAP
u_int8_t wlan_pdev_get_qwrap_isolation(struct wlan_objmgr_pdev * pdev)
{
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return 0;
	}
	return ic->ic_wrap_com->wc_isolation;
}

EXPORT_SYMBOL(wlan_pdev_get_qwrap_isolation);
#endif

u_int8_t wlan_vdev_is_radar_channel(struct wlan_objmgr_vdev * vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}

	return IEEE80211_IS_CHAN_RADAR(vap->iv_bsschan);
}

EXPORT_SYMBOL(wlan_vdev_is_radar_channel);

void wlan_vdev_sta_power_tx_start(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}
	ieee80211_sta_power_tx_start(vap);
}

EXPORT_SYMBOL(wlan_vdev_sta_power_tx_start);

void wlan_vdev_sta_power_tx_end(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}
	ieee80211_sta_power_tx_end(vap);
}

EXPORT_SYMBOL(wlan_vdev_sta_power_tx_end);

void
wlan_peer_saveq_queue(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
		      u_int8_t frame_type)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ieee80211_node_saveq_queue(ni, wbuf, frame_type);
}

EXPORT_SYMBOL(wlan_peer_saveq_queue);

struct wlan_objmgr_peer *wlan_find_wds_peer(struct wlan_objmgr_vdev *vdev,
					    u_int8_t * macaddr)
{
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return NULL;
	}

	nt = &vap->iv_ic->ic_sta;

	ni = ieee80211_find_wds_node(nt, macaddr);

	if (ni)
		peer = ni->peer_obj;

	return peer;

}

EXPORT_SYMBOL(wlan_find_wds_peer);

void wlan_set_wds_node_time(struct wlan_objmgr_vdev *vdev,
			    const u_int8_t * macaddr)
{
	struct ieee80211_node_table *nt;
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	nt = &vap->iv_ic->ic_sta;

	ieee80211_set_wds_node_time(nt, macaddr);

}

EXPORT_SYMBOL(wlan_set_wds_node_time);

void wlan_add_wds_addr(struct wlan_objmgr_vdev *vdev,
		       struct wlan_objmgr_peer *peer, const u_int8_t * macaddr,
		       u_int32_t flags)
{
	struct ieee80211_node_table *nt;
	struct ieee80211_node *ni;
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	nt = &vap->iv_ic->ic_sta;

	ieee80211_add_wds_addr(vap, nt, ni, macaddr, flags);

}

EXPORT_SYMBOL(wlan_add_wds_addr);

void wlan_remove_wds_addr(struct wlan_objmgr_vdev *vdev,
			  const u_int8_t * macaddr, u_int32_t flags)
{
	struct ieee80211_node_table *nt;
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	nt = &vap->iv_ic->ic_sta;

	ieee80211_remove_wds_addr(vap, nt, macaddr, flags);

}

EXPORT_SYMBOL(wlan_remove_wds_addr);

u_int32_t
wlan_find_wds_peer_age(struct wlan_objmgr_vdev * vdev, const u_int8_t * macaddr)
{
	struct ieee80211_node_table *nt;
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}

	nt = &vap->iv_ic->ic_sta;

	return ieee80211_find_wds_node_age(nt, macaddr);

}

EXPORT_SYMBOL(wlan_find_wds_peer_age);

void wlan_admctl_classify(struct wlan_objmgr_vdev *vdev,
			  struct wlan_objmgr_peer *peer, int *tid, int *ac)
{
	wlan_if_t vap;
	struct ieee80211_node *ni;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ieee80211_admctl_classify(vap, ni, tid, ac);
}

EXPORT_SYMBOL(wlan_admctl_classify);

int wlan_nawds_send_wbuf(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return -1;
	}
	return ieee80211_nawds_send_wbuf(vap, wbuf);
}

EXPORT_SYMBOL(wlan_nawds_send_wbuf);

int wlan_vdev_nawds_enable_learning(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return -1;
	}
	return ieee80211_nawds_enable_learning(vap);
}

EXPORT_SYMBOL(wlan_vdev_nawds_enable_learning);

void wlan_vdev_set_tim(struct wlan_objmgr_peer *peer, int set, bool isr_context)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	if (ni->ni_vap && ni->ni_vap->iv_set_tim != NULL)
		ni->ni_vap->iv_set_tim(ni, set, isr_context);
}

EXPORT_SYMBOL(wlan_vdev_set_tim);

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
struct ieee80211_key *wlan_crypto_get_txkey(struct wlan_objmgr_vdev *vdev,
					    wbuf_t wbuf)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return NULL;
	}
	return ieee80211_crypto_get_txkey(vap, wbuf);
}
#else
struct wlan_crypto_key *wlan_crypto_get_txkey(struct wlan_objmgr_vdev *vdev,
					    wbuf_t wbuf)
{
	/* get Tx key */
	return wlan_crypto_vdev_getkey(vdev, WLAN_CRYPTO_KEYIX_NONE);
}
#endif

EXPORT_SYMBOL(wlan_crypto_get_txkey);

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
struct ieee80211_key *wlan_crypto_peer_get_key(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return NULL;
	}

	return &ni->ni_ucastkey;
}
#else
struct wlan_crypto_key *wlan_crypto_peer_get_key(struct wlan_objmgr_peer *peer)
{
	/* get Tx key */
	return wlan_crypto_peer_getkey(peer, WLAN_CRYPTO_KEYIX_NONE);
}
#endif
EXPORT_SYMBOL(wlan_crypto_peer_get_key);

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
struct ieee80211_key *wlan_crypto_peer_encap(struct wlan_objmgr_peer *peer,
					     wbuf_t wbuf)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return NULL;
	}

	return ieee80211_crypto_encap(ni, wbuf);
}
#else
struct wlan_crypto_key *wlan_crypto_peer_encap(struct wlan_objmgr_peer *peer,
					     wbuf_t wbuf)
{
	return NULL;
}
#endif

EXPORT_SYMBOL(wlan_crypto_peer_encap);

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
struct ieee80211_key *wlan_crypto_peer_decap(struct wlan_objmgr_peer *peer,
					     wbuf_t wbuf, int hdrlen,
					     struct ieee80211_rx_status *rs)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return NULL;
	}

	return ieee80211_crypto_decap(ni, wbuf, hdrlen, rs);
}
#else
struct wlan_crypto_key *wlan_crypto_peer_decap(struct wlan_objmgr_peer *peer,
					     wbuf_t wbuf)
{
	return NULL;
}
#endif

EXPORT_SYMBOL(wlan_crypto_peer_decap);

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
u_int16_t wlan_vdev_get_def_txkey(struct wlan_objmgr_vdev * vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return IEEE80211_KEYIX_NONE;
	}
	return vap->iv_def_txkey;
}
EXPORT_SYMBOL(wlan_vdev_get_def_txkey);
#endif

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
struct ieee80211_key *wlan_vdev_get_nw_keys(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return NULL;
	}
	return &vap->iv_nw_keys[vap->iv_def_txkey];
}
#else
struct wlan_crypto_key *wlan_vdev_get_nw_keys(struct wlan_objmgr_vdev *vdev,
					    wbuf_t wbuf)
{
	/* get Tx key */
	return wlan_crypto_vdev_getkey(vdev, WLAN_CRYPTO_KEYIX_NONE);
}
#endif

EXPORT_SYMBOL(wlan_vdev_get_nw_keys);

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
int wlan_crypto_enmic(struct wlan_objmgr_vdev *vdev,
		      struct ieee80211_key *key, wbuf_t wbuf, int force,
		      bool encap)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return -1;
	}
	return ieee80211_crypto_enmic(vap, key, wbuf, force, encap);
}
EXPORT_SYMBOL(wlan_crypto_enmic);
#endif

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
int wlan_vdev_crypto_demic(struct wlan_objmgr_vdev *vdev,
			   struct ieee80211_key *key, wbuf_t wbuf, int hdrlen,
			   int force, struct ieee80211_rx_status *rs)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return -1;
	}
	return ieee80211_crypto_demic(vap, key, wbuf, hdrlen, force, rs);
}
#else
int wlan_vdev_crypto_demic(struct wlan_objmgr_vdev *vdev,
			   struct wlan_crypto_key *key, wbuf_t wbuf, int hdrlen,
			   int force, struct ieee80211_rx_status *rs)
{
	return -1;
}
#endif
EXPORT_SYMBOL(wlan_vdev_crypto_demic);

void
wlan_prepare_qosnulldata(struct wlan_objmgr_peer *peer, wbuf_t wbuf, int ac)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}
	ieee80211_prepare_qosnulldata(ni, wbuf, ac);
}

EXPORT_SYMBOL(wlan_prepare_qosnulldata);

u_int16_t wlan_vdev_get_mgt_rate(struct wlan_objmgr_vdev * vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}
	return vap->iv_mgt_rate;
}

EXPORT_SYMBOL(wlan_vdev_get_mgt_rate);

u_int8_t wlan_peer_get_bf_update_cv(struct wlan_objmgr_peer * peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return 0;
	}

	return ni->ni_bf_update_cv;
}

EXPORT_SYMBOL(wlan_peer_get_bf_update_cv);

void wlan_request_cv_update(struct wlan_objmgr_pdev *pdev,
			    struct wlan_objmgr_peer *peer, wbuf_t wbuf,
			    int use4addr)
{
	struct ieee80211_node *ni;
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return;
	}

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ieee80211_request_cv_update(ic, ni, wbuf, use4addr);
}

EXPORT_SYMBOL(wlan_request_cv_update);

u_int32_t wlan_vdev_forced_sleep_is_set(struct wlan_objmgr_vdev * vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}
	return ieee80211_vap_forced_sleep_is_set(vap);

}

EXPORT_SYMBOL(wlan_vdev_forced_sleep_is_set);

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
void wlan_ald_record_tx(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf, int datalen)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	ieee80211_ald_record_tx(vap, wbuf, datalen);
}

EXPORT_SYMBOL(wlan_ald_record_tx);
#endif

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
struct ieee80211_rsnparms *wlan_vdev_get_rsn(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return NULL;
	}

	return &vap->iv_rsn;
}
EXPORT_SYMBOL(wlan_vdev_get_rsn);
#endif

u_int32_t wlan_pdev_get_tx_staggered_sounding(struct wlan_objmgr_pdev * pdev)
{
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return 0;
	}

	return ic->ic_txbf.tx_staggered_sounding;
}

EXPORT_SYMBOL(wlan_pdev_get_tx_staggered_sounding);

u_int16_t wlan_pdev_get_ldpcap(struct wlan_objmgr_pdev * pdev)
{
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return 0;
	}

	return ic->ic_ldpccap;
}

EXPORT_SYMBOL(wlan_pdev_get_ldpcap);

u_int16_t wlan_pdev_get_curmode(struct wlan_objmgr_pdev * pdev)
{
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return 0;
	}

	return ic->ic_curmode;
}

EXPORT_SYMBOL(wlan_pdev_get_curmode);

/******************* Should be converged? *********************/

struct ieee80211_ath_channel *wlan_vdev_get_bsschan(struct wlan_objmgr_vdev
						    *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return NULL;
	}
	return vap->iv_bsschan;
}

EXPORT_SYMBOL(wlan_vdev_get_bsschan);

#if ATH_SUPPORT_WRAP
bool wlan_is_vdev_mpsta(struct wlan_objmgr_vdev * vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return false;
	}
	return wlan_is_mpsta(vap);
}

EXPORT_SYMBOL(wlan_is_vdev_mpsta);

bool wlan_is_vdev_psta(struct wlan_objmgr_vdev * vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return false;
	}
	return wlan_is_psta(vap);
}

EXPORT_SYMBOL(wlan_is_vdev_psta);
#endif

u_int16_t wlan_vdev_get_sta_assoc(struct wlan_objmgr_vdev * vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}
	return vap->iv_sta_assoc;
}

EXPORT_SYMBOL(wlan_vdev_get_sta_assoc);

u_int8_t wlan_pdev_get_wmep_acm(struct wlan_objmgr_pdev * pdev, int ac)
{
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	return ic->ic_wme.wme_wmeChanParams.cap_wmeParams[ac].wmep_acm;
}

EXPORT_SYMBOL(wlan_pdev_get_wmep_acm);

u_int8_t wlan_pdev_get_wmep_noackPolicy(struct wlan_objmgr_pdev * pdev, int ac)
{
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	return ic->ic_wme.wme_wmeChanParams.cap_wmeParams[ac].wmep_noackPolicy;
}

EXPORT_SYMBOL(wlan_pdev_get_wmep_noackPolicy);

int wlan_peer_get_pspoll(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return 0;
	}

	return ni->ni_pspoll;
}

EXPORT_SYMBOL(wlan_peer_get_pspoll);

u_int32_t wlan_peer_get_rx_staggered_sounding(struct wlan_objmgr_peer * peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return 0;
	}

	return ni->ni_txbf.rx_staggered_sounding;
}

EXPORT_SYMBOL(wlan_peer_get_rx_staggered_sounding);

u_int32_t wlan_peer_get_htcap(struct wlan_objmgr_peer * peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return 0;
	}

	return ni->ni_htcap;
}

EXPORT_SYMBOL(wlan_peer_get_htcap);

u_int32_t wlan_peer_get_channel_estimation_cap(struct wlan_objmgr_peer * peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return 0;
	}

	return ni->ni_txbf.channel_estimation_cap;
}

EXPORT_SYMBOL(wlan_peer_get_channel_estimation_cap);

u_int16_t wlan_peer_get_txseqs(struct wlan_objmgr_peer * peer, int tid)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return 0;
	}

	return ni->ni_txseqs[tid];
}

EXPORT_SYMBOL(wlan_peer_get_txseqs);

void wlan_peer_set_txseqs(struct wlan_objmgr_peer *peer, int tid, int val)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ni->ni_txseqs[tid] = val;
}

EXPORT_SYMBOL(wlan_peer_set_txseqs);

void wlan_peer_incr_txseq(struct wlan_objmgr_peer *peer, int tid)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ni->ni_txseqs[tid]++;
}

EXPORT_SYMBOL(wlan_peer_incr_txseq);

int wlan_vdev_wnm_is_set(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}
	return ieee80211_vap_wnm_is_set(vap);
}

EXPORT_SYMBOL(wlan_vdev_wnm_is_set);

int wlan_vdev_wnm_fms_is_set(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}
	return ieee80211_wnm_fms_is_set(vap->wnm);
}

EXPORT_SYMBOL(wlan_vdev_wnm_fms_is_set);

void wlan_fms_filter(struct wlan_objmgr_peer *peer,
		     wbuf_t wbuf, int ether_type, struct ip_header *ip,
		     int hdrsize)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ieee80211_fms_filter(ni, wbuf, ether_type, ip, hdrsize);
}

EXPORT_SYMBOL(wlan_fms_filter);

/********************* Common to OL & DA **********************/

int wlan_dbdc_tx_process(struct wlan_objmgr_vdev *vdev, osif_dev ** osdev,
			 struct sk_buff *skb)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return -EINVAL;
	}
	return dbdc_tx_process(vap, osdev, skb);
}

EXPORT_SYMBOL(wlan_dbdc_tx_process);

#if ATH_SUPPORT_WRAP
osif_dev *wlan_osif_wrap_wdev_find(struct wlan_objmgr_pdev * pdev,
				   unsigned char *mac)
{
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return NULL;
	}

	return osif_wrap_wdev_find(&ic->ic_wrap_com->wc_devt, mac);
}

EXPORT_SYMBOL(wlan_osif_wrap_wdev_find);
#endif

int wlan_vdev_wnm_tfs_filter(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return -EINVAL;
	}
	return wlan_wnm_tfs_filter(vap, wbuf);
}

EXPORT_SYMBOL(wlan_vdev_wnm_tfs_filter);

#if UMAC_SUPPORT_PROXY_ARP
int wlan_do_proxy_arp(struct wlan_objmgr_vdev *vdev, qdf_nbuf_t netbuf)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return -EINVAL;
	}
	return do_proxy_arp(vap, netbuf);
}

EXPORT_SYMBOL(wlan_do_proxy_arp);
#endif

int wlan_peer_pause(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	return ieee80211node_pause(ni);
}

EXPORT_SYMBOL(wlan_peer_pause);

int wlan_peer_unpause(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	return ieee80211node_unpause(ni);
}

EXPORT_SYMBOL(wlan_peer_unpause);

void wlan_vdev_set_lastdata_timestamp(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	vap->iv_lastdata = OS_GET_TIMESTAMP();
}

EXPORT_SYMBOL(wlan_vdev_set_lastdata_timestamp);

void wlan_vdev_set_last_directed_frame(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	vap->iv_last_directed_frame = OS_GET_TIMESTAMP();
}

EXPORT_SYMBOL(wlan_vdev_set_last_directed_frame);

u_int8_t wlan_vdev_get_lastbcn_phymode_mismatch(struct wlan_objmgr_vdev * vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 1;
	}

	return vap->iv_lastbcn_phymode_mismatch;
}

EXPORT_SYMBOL(wlan_vdev_get_lastbcn_phymode_mismatch);

int wlan_vdev_get_bmiss_count(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}

	return vap->iv_bmiss_count;
}

EXPORT_SYMBOL(wlan_vdev_get_bmiss_count);

void wlan_set_dadp_ops(struct wlan_objmgr_pdev *pdev, struct dadp_ops *dops)
{
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return;
	}

	ic->dops = (void *)dops;
}

EXPORT_SYMBOL(wlan_set_dadp_ops);

/* ==================== DA DP calls from UMAC to DADP ==================== */

struct dadp_ops *wlan_pdev_get_dadp_ops(struct wlan_objmgr_pdev *pdev)
{
	struct ieee80211com *ic;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return NULL;
	}
	if (ic->ic_is_mode_offload(ic))
		return NULL;
	else
		return (struct dadp_ops *)(ic->dops);
}

EXPORT_SYMBOL(wlan_pdev_get_dadp_ops);

void * wlan_pdev_get_extap_handle(struct wlan_objmgr_pdev *pdev)
{
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->pdev_get_extap_handle)
		return dops->pdev_get_extap_handle(pdev);

	return 0;
}

EXPORT_SYMBOL(wlan_pdev_get_extap_handle);

void wlan_vdev_set_qos_map(struct wlan_objmgr_vdev *vdev, void *qos_map,
			   int len)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_qos_map)
		dops->vdev_set_qos_map(vdev, qos_map, len);
}

void wlan_vdev_set_privacy_filters(struct wlan_objmgr_vdev *vdev, void *filters,
				   u_int32_t num)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_privacy_filters)
		dops->vdev_set_privacy_filters(vdev, filters, num);
}

void wlan_vdev_register_osif_events(struct wlan_objmgr_vdev *vdev,
				    wlan_event_handler_table * evtab)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_osif_event_handlers)
		dops->vdev_set_osif_event_handlers(vdev, evtab);
}

void wlan_vdev_register_ccx_event_handlers(struct wlan_objmgr_vdev *vdev,
					   os_if_t osif,
					   wlan_ccx_handler_table * evtab)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_register_ccx_events)
		dops->vdev_register_ccx_events(vdev, osif, evtab);
}

void wlan_vdev_set_smps(struct wlan_objmgr_vdev *vdev, int val)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_smps)
		dops->vdev_set_smps(vdev, val);
}

void wlan_vdev_set_smartnet_enable(struct wlan_objmgr_vdev *vdev, u_int32_t val)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_smartnet_enable)
		dops->vdev_set_smartnet_enable(vdev, val);
}

void wlan_vdev_set_prot_mode(struct wlan_objmgr_vdev *vdev, u_int32_t val)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_prot_mode)
		dops->vdev_set_prot_mode(vdev, val);
}

void wlan_vdev_set_tspecActive(struct wlan_objmgr_vdev *vdev, u_int8_t val)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_tspecActive)
		dops->vdev_set_tspecActive(vdev, val);
}

void wlan_vdev_set_txrxbytes(struct wlan_objmgr_vdev *vdev, u_int64_t val)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_txrxbytes)
		dops->vdev_set_txrxbytes(vdev, val);
}

u_int64_t wlan_vdev_get_txrxbytes(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_get_txrxbytes)
		return dops->vdev_get_txrxbytes(vdev);
	else
		return 0;
}

void wlan_peer_set_uapsd_maxsp(struct wlan_objmgr_peer *peer,
			       u_int8_t uapsd_maxsp)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->peer_set_uapsd_maxsp)
		dops->peer_set_uapsd_maxsp(peer, uapsd_maxsp);
}

u_int16_t wlan_peer_get_keyix(struct wlan_objmgr_peer *peer)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->peer_get_keyix)
		return dops->peer_get_keyix(peer);
	else
		return IEEE80211_KEYIX_NONE;

}

void wlan_peer_update_uapsd_ac_trigena(struct wlan_objmgr_peer *peer,
				       u_int8_t * uapsd_ac_trigena, int len)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->peer_update_uapsd_ac_trigena)
		dops->peer_update_uapsd_ac_trigena(peer, uapsd_ac_trigena, len);
}

void wlan_peer_update_uapsd_ac_delivena(struct wlan_objmgr_peer *peer,
					u_int8_t * uapsd_ac_delivena, int len)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->peer_update_uapsd_ac_delivena)
		dops->peer_update_uapsd_ac_delivena(peer, uapsd_ac_delivena,
						    len);
}

void wlan_peer_update_uapsd_dyn_delivena(struct wlan_objmgr_peer *peer,
					 int8_t * uapsd_dyn_delivena, int len)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->peer_update_uapsd_dyn_delivena)
		dops->peer_update_uapsd_dyn_delivena(peer, uapsd_dyn_delivena,
						     len);
}

EXPORT_SYMBOL(wlan_peer_update_uapsd_dyn_delivena);

void wlan_peer_update_uapsd_dyn_trigena(struct wlan_objmgr_peer *peer,
					int8_t * uapsd_dyn_trigena, int len)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->peer_update_uapsd_dyn_trigena)
		dops->peer_update_uapsd_dyn_trigena(peer, uapsd_dyn_trigena,
						    len);
}

EXPORT_SYMBOL(wlan_peer_update_uapsd_dyn_trigena);

void wlan_dp_vdev_detach(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);
	wlan_if_t vap = NULL;

	vap = wlan_vdev_get_vap(vdev);
	if (!vap)
		return;

	if (dops && dops->vdev_detach)
		dops->vdev_detach(vdev);
}

void wlan_dp_peer_detach(struct wlan_objmgr_peer *peer)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->peer_detach)
		dops->peer_detach(peer);
}

#if ATH_SUPPORT_IQUE
static int
wlan_me_SnoopInspecting(struct wlan_objmgr_vdev *vdev,
			struct wlan_objmgr_peer *peer, wbuf_t wbuf)
{
	wlan_if_t vap;
	struct ieee80211_node *ni;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return QDF_STATUS_E_FAILURE;
	}
	if (vap->iv_ique_ops.me_inspect)
		return vap->iv_ique_ops.me_inspect(vap, ni, wbuf);

	return QDF_STATUS_E_FAILURE;
}

#if ATH_SUPPORT_ME_SNOOP_TABLE
static int wlan_me_SnoopConvert(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (vap->iv_ique_ops.me_convert)
		return vap->iv_ique_ops.me_convert(vap, wbuf);

	return QDF_STATUS_E_FAILURE;
}
#endif

int wlan_me_hmmc_find(struct wlan_objmgr_vdev *vdev, u_int32_t dip)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (vap->iv_ique_ops.me_hmmc_find)
		return vap->iv_ique_ops.me_hmmc_find(vap, dip);

	return QDF_STATUS_E_FAILURE;
}

#if ATH_SUPPORT_ME_SNOOP_TABLE
void wlan_me_clean(struct wlan_objmgr_peer *peer)
{
	wlan_if_t vap;
	struct ieee80211_node *ni;
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	if (vap->iv_ique_ops.me_clean)
		vap->iv_ique_ops.me_clean(ni);
}
#endif

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
int wlan_me_hifi_convert(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (vap->iv_ique_ops.me_hifi_convert){
		if(vap->iv_ique_ops.me_hifi_convert(vap, wbuf) >= 0)
			return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_FAILURE;
}
#endif

#if ATH_SUPPORT_HBR
static int wlan_hbr_dropblocked(struct wlan_objmgr_vdev *vdev,
				struct wlan_objmgr_peer *peer, wbuf_t wbuf)
{
	wlan_if_t vap;
	struct ieee80211_node *ni;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	if (vap->iv_ique_ops.hbr_dropblocked)
		return vap->iv_ique_ops.hbr_dropblocked(vap, ni, wbuf);

	return QDF_STATUS_E_FAILURE;
}
#endif

#endif

int wlan_vdev_ique_attach(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);
	struct wlan_vdev_ique_ops ique_ops;
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_zero(&ique_ops, sizeof(struct wlan_vdev_ique_ops));

#if ATH_SUPPORT_IQUE
	/*Attach function entry points */
#if ATH_SUPPORT_ME_SNOOP_TABLE
	ique_ops.wlan_me_convert = wlan_me_SnoopConvert;
	ique_ops.wlan_me_clean = wlan_me_clean;
#endif
	ique_ops.wlan_me_inspect = wlan_me_SnoopInspecting;
	ique_ops.wlan_me_hmmc_find = wlan_me_hmmc_find;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
	ique_ops.wlan_me_hifi_convert = wlan_me_hifi_convert;
#endif
#if ATH_SUPPORT_HBR
	ique_ops.wlan_hbr_dropblocked = wlan_hbr_dropblocked;
#endif
#endif
	if (dops && dops->ique_attach)
		dops->ique_attach(vdev, &ique_ops);

	return QDF_STATUS_SUCCESS;
}

void wlan_vdev_set_me_hifi_enable(struct wlan_objmgr_vdev *vdev,
				  u_int32_t me_hifi_enable)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_me_hifi_enable)
		dops->vdev_set_me_hifi_enable(vdev, me_hifi_enable);
}

void wlan_vdev_set_mc_snoop_enable(struct wlan_objmgr_vdev *vdev,
				   u_int32_t mc_snoop_enable)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_mc_snoop_enable)
		dops->vdev_set_mc_snoop_enable(vdev, mc_snoop_enable);
}

void wlan_vdev_set_mc_mcast_enable(struct wlan_objmgr_vdev *vdev,
				   u_int32_t mc_mcast_enable)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_mc_mcast_enable)
		dops->vdev_set_mc_mcast_enable(vdev, mc_mcast_enable);
}

void wlan_vdev_set_mc_discard_mcast(struct wlan_objmgr_vdev *vdev,
				    u_int32_t mc_discard_mcast)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_mc_discard_mcast)
		dops->vdev_set_mc_discard_mcast(vdev, mc_discard_mcast);
}

void wlan_vdev_set_ampdu_subframes(struct wlan_objmgr_vdev *vdev,
				    u_int32_t ampdu)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_ampdu_subframes)
		dops->vdev_set_ampdu_subframes(vdev, ampdu);
}

void wlan_vdev_set_amsdu(struct wlan_objmgr_vdev *vdev, u_int32_t amsdu)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_amsdu)
		dops->vdev_set_amsdu(vdev, amsdu);
}

void wlan_vdev_set_dscp_map_id(struct wlan_objmgr_vdev *vdev,
			       u_int32_t dscp_map_id)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_dscp_map_id)
		dops->vdev_set_dscp_map_id(vdev, dscp_map_id);
}

void wlan_vdev_update_last_ap_frame(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	if (dops && dops->vdev_get_last_ap_frame)
		vap->iv_last_ap_frame = dops->vdev_get_last_ap_frame(vdev);
}

void wlan_vdev_update_last_traffic_indication(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	if (dops && dops->vdev_get_last_traffic_indication)
		vap->iv_last_traffic_indication =
		    dops->vdev_get_last_traffic_indication(vdev);
}

void wlan_vdev_set_priority_dscp_tid_map(struct wlan_objmgr_vdev *vdev, u_int8_t tid)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	if (dops && dops->vdev_set_priority_dscp_tid_map)
		dops->vdev_set_priority_dscp_tid_map(vdev, tid);
}
void wlan_vdev_set_dscp_tid_map(struct wlan_objmgr_vdev *vdev, u_int8_t tos,
				u_int8_t tid)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	if (dops && dops->vdev_set_dscp_tid_map)
		dops->vdev_set_dscp_tid_map(vdev, tos, tid);
}

#ifdef ATH_COALESCING
void wlan_pdev_set_tx_coalescing(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->pdev_set_tx_coalescing)
		dops->pdev_set_tx_coalescing(pdev, val);

}
#endif
void wlan_pdev_set_minframesize(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->pdev_set_minframesize)
		dops->pdev_set_minframesize(pdev, val);

}

void wlan_pdev_set_mon_vdev(struct wlan_objmgr_pdev *pdev,
			    struct wlan_objmgr_vdev *vdev)
{
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->pdev_set_mon_vdev)
		dops->pdev_set_mon_vdev(pdev, vdev);

}

void wlan_vdev_set_frag_threshold(struct wlan_objmgr_vdev *vdev,
				  u_int16_t fragthreshold)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_frag_threshold)
		dops->vdev_set_frag_threshold(vdev, fragthreshold);
}

void wlan_vdev_set_rtsthreshold(struct wlan_objmgr_vdev *vdev,
				u_int16_t rtsthreshold)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_rtsthreshold)
		dops->vdev_set_rtsthreshold(vdev, rtsthreshold);
}

void wlan_peer_set_nawds(struct wlan_objmgr_peer *peer)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->peer_set_nawds)
		dops->peer_set_nawds(peer);
}

void wlan_pdev_set_addba_mode(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->pdev_set_addba_mode)
		dops->pdev_set_addba_mode(pdev, val);

}

void wlan_pdev_set_amsdu_max_size(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->pdev_set_amsdu_max_size)
		dops->pdev_set_amsdu_max_size(pdev, val);

}

void wlan_vdev_set_userrate(struct wlan_objmgr_vdev *vdev, int16_t val)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_userrate)
		dops->vdev_set_userrate(vdev, val);
}

void wlan_vdev_set_userretries(struct wlan_objmgr_vdev *vdev, int8_t val)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_userretries)
		dops->vdev_set_userretries(vdev, val);
}

void wlan_vdev_set_txchainmask(struct wlan_objmgr_vdev *vdev, int8_t val)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_txchainmask)
		dops->vdev_set_txchainmask(vdev, val);
}

void wlan_vdev_set_txpower(struct wlan_objmgr_vdev *vdev, int8_t val)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_txpower)
		dops->vdev_set_txpower(vdev, val);
}

int wlan_vdev_get_curmode(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}
	return vap->iv_cur_mode;
}

EXPORT_SYMBOL(wlan_vdev_get_curmode);

u_int16_t wlan_vdev_has_pssta(struct wlan_objmgr_vdev * vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}
	return vap->iv_ps_sta;
}

EXPORT_SYMBOL(wlan_vdev_has_pssta);

u_int16_t wlan_vdev_get_rtsthreshold(struct wlan_objmgr_vdev * vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}
	return vap->iv_rtsthreshold;
}

EXPORT_SYMBOL(wlan_vdev_get_rtsthreshold);

u_int8_t *wlan_peer_get_bssid(struct wlan_objmgr_peer * peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return NULL;
	}

	return (u_int8_t *) (ni->ni_bssid);
}

EXPORT_SYMBOL(wlan_peer_get_bssid);

void wlan_peer_reload_inact(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ni->ni_inact = ni->ni_inact_reload;
}

EXPORT_SYMBOL(wlan_peer_reload_inact);

void wlan_peer_set_inact(struct wlan_objmgr_peer *peer, u_int32_t val)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ni->ni_inact = val;
}

EXPORT_SYMBOL(wlan_peer_set_inact);

void wlan_pdev_set_athextcap(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->pdev_set_athextcap)
		dops->pdev_set_athextcap(pdev, val);
}

EXPORT_SYMBOL(wlan_pdev_set_athextcap);

void wlan_pdev_clear_athextcap(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->pdev_clear_athextcap)
		dops->pdev_clear_athextcap(pdev, val);
}

EXPORT_SYMBOL(wlan_pdev_clear_athextcap);

void wlan_vdev_set_sko_th(struct wlan_objmgr_vdev *vdev, u_int8_t sko_th)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_sko_th)
		dops->vdev_set_sko_th(vdev, sko_th);
}

void wlan_kick_peer(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ieee80211_kick_node(ni);
}

EXPORT_SYMBOL(wlan_kick_peer);

void wlan_wnm_bssmax_updaterx(struct wlan_objmgr_peer *peer, int secured)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ieee80211_wnm_bssmax_updaterx(ni, secured);
}

EXPORT_SYMBOL(wlan_wnm_bssmax_updaterx);

u_int16_t wlan_peer_get_associd(struct wlan_objmgr_peer * peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return 0;
	}

	return ni->ni_associd;
}

EXPORT_SYMBOL(wlan_peer_get_associd);

u_int16_t wlan_peer_get_txpower(struct wlan_objmgr_peer * peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return 0;
	}

	return ni->ni_txpower;
}

EXPORT_SYMBOL(wlan_peer_get_txpower);

int wlan_peer_check_cap(struct wlan_objmgr_peer *peer, u_int16_t cap)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return 0;
	}

	return ieee80211node_has_cap(ni, cap);
}

EXPORT_SYMBOL(wlan_peer_check_cap);

int wlan_send_deauth(struct wlan_objmgr_peer *peer, u_int16_t reason)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return -EINVAL;
	}

	return ieee80211_send_deauth(ni, reason);
}

EXPORT_SYMBOL(wlan_send_deauth);

int wlan_send_disassoc(struct wlan_objmgr_peer *peer, u_int16_t reason)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return -EINVAL;
	}

	return ieee80211_send_disassoc(ni, reason);
}

EXPORT_SYMBOL(wlan_send_disassoc);

void wlan_vdev_deliver_event_mlme_deauth_indication(struct wlan_objmgr_vdev
						    *vdev, u_int8_t * macaddr,
						    u_int16_t associd,
						    u_int16_t reason)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}
	IEEE80211_DELIVER_EVENT_MLME_DEAUTH_INDICATION(vap, macaddr,
						       associd, reason);
}

EXPORT_SYMBOL(wlan_vdev_deliver_event_mlme_deauth_indication);

void wlan_vdev_deliver_event_mlme_disassoc_indication(struct wlan_objmgr_vdev
						      *vdev, u_int8_t * macaddr,
						      u_int16_t associd,
						      u_int16_t reason)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}
	IEEE80211_DELIVER_EVENT_MLME_DISASSOC_INDICATION(vap, macaddr,
							 associd, reason);
}

EXPORT_SYMBOL(wlan_vdev_deliver_event_mlme_disassoc_indication);

void wlan_pdev_timeout_fragments(struct wlan_objmgr_pdev *pdev,
				 u_int32_t lifetime)
{
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->pdev_timeout_fragments)
		dops->pdev_timeout_fragments(pdev, lifetime);
}

void wlan_vdev_get_txrx_activity(struct wlan_objmgr_vdev *vdev,
				 ieee80211_vap_activity * activity)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_get_txrx_activity)
		dops->vdev_get_txrx_activity(vdev, activity);
}

#ifdef ATH_SUPPORT_TxBF
void
wlan_tx_bf_completion_handler(struct wlan_objmgr_peer *peer,
			      struct ieee80211_tx_status *ts)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}
	ieee80211_tx_bf_completion_handler(ni, ts);
}

EXPORT_SYMBOL(wlan_tx_bf_completion_handler);
#endif

void wlan_vdev_deliver_tx_event(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
				struct ieee80211_frame *wh,
				struct ieee80211_tx_status *ts)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_deliver_tx_event)
		dops->vdev_deliver_tx_event(peer, wbuf, wh, ts);
}

int wlan_vdev_send_wbuf(struct wlan_objmgr_vdev *vdev,
			struct wlan_objmgr_peer *peer, wbuf_t wbuf)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_send_wbuf)
		return dops->vdev_send_wbuf(vdev, peer, wbuf);
	else
		return -EINVAL;
}

int wlan_vdev_send(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_send)
		return dops->vdev_send(vdev, wbuf);
	else
		return -EINVAL;
}

struct ieee80211_node_table *wlan_peer_get_nodetable(struct wlan_objmgr_peer
						     *peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return NULL;
	}

	return ni->ni_table;
}

EXPORT_SYMBOL(wlan_peer_get_nodetable);

int wlan_vdev_txrx_register_event_handler(struct wlan_objmgr_vdev *vdev,
					  ieee80211_vap_txrx_event_handler
					  evhandler, void *arg,
					  u_int32_t event_filter)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_txrx_register_event_handler)
		return dops->vdev_txrx_register_event_handler(vdev, evhandler,
							      arg,
							      event_filter);
	else
		return -EINVAL;
}

int wlan_vdev_txrx_unregister_event_handler(struct wlan_objmgr_vdev *vdev,
					    ieee80211_vap_txrx_event_handler
					    evhandler, void *arg)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_txrx_unregister_event_handler)
		return dops->vdev_txrx_unregister_event_handler(vdev, evhandler,
								arg);
	else
		return -EINVAL;
}

void wlan_vdev_txrx_deliver_event(struct wlan_objmgr_vdev *vdev,
				  ieee80211_vap_txrx_event * evt)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_txrx_deliver_event)
		dops->vdev_txrx_deliver_event(vdev, evt);

}

void wlan_vdev_set_mcast_rate(struct wlan_objmgr_vdev *vdev, int mcast_rate)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_set_mcast_rate)
		dops->vdev_set_mcast_rate(vdev, mcast_rate);
}

void wlan_peer_set_minbasicrate(struct wlan_objmgr_peer *peer,
				u_int8_t minbasicrate)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->peer_set_minbasicrate)
		dops->peer_set_minbasicrate(peer, minbasicrate);
}

void wlan_vdev_reset_bmiss(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}
	ieee80211_mlme_reset_bmiss(vap);
}

EXPORT_SYMBOL(wlan_vdev_reset_bmiss);

void wlan_peer_set_rssi(struct wlan_objmgr_peer *peer, u_int8_t rssi)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ni->ni_rssi = rssi;

}

EXPORT_SYMBOL(wlan_peer_set_rssi);

void wlan_peer_set_rssi_min_max(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	if (ni->ni_rssi < ni->ni_rssi_min)
		ni->ni_rssi_min = ni->ni_rssi;
	else if (ni->ni_rssi > ni->ni_rssi_max)
		ni->ni_rssi_max = ni->ni_rssi;
}

EXPORT_SYMBOL(wlan_peer_set_rssi_min_max);

void wlan_peer_mlme_pwrsave(struct wlan_objmgr_peer *peer, int enable)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}
	ieee80211_mlme_node_pwrsave(ni, enable);
}

EXPORT_SYMBOL(wlan_peer_mlme_pwrsave);

#if ATH_SW_WOW
u_int8_t wlan_vdev_get_wow(struct wlan_objmgr_vdev * vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}
	return wlan_get_wow(vap);
}

EXPORT_SYMBOL(wlan_vdev_get_wow);

void wlan_peer_wow_magic_parser(struct ieee80211_node *ni, wbuf_t wbuf)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ieee80211_wow_magic_parser(ni, wbuf);
}

EXPORT_SYMBOL(wlan_peer_wow_magic_parser);
#endif

void
wlan_wds_update_rootwds_table(struct wlan_objmgr_peer *peer,
			      struct wlan_objmgr_pdev *pdev, wbuf_t wbuf)
{
	struct ieee80211com *ic;
	struct ieee80211_node *ni;

	ic = wlan_mlme_get_pdev_legacy_obj(pdev);
	if (ic == NULL) {
		qdf_print("%s:ic is NULL ", __func__);
		return;
	}

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	wds_update_rootwds_table(ni->ni_vap, ni, &ic->ic_sta, wbuf);
}

EXPORT_SYMBOL(wlan_wds_update_rootwds_table);

int
wlan_vdev_wds_sta_chkmcecho(struct wlan_objmgr_vdev *vdev,
			    const u_int8_t * sender)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return 0;
	}

	return wds_sta_chkmcecho(vap, sender);
}

EXPORT_SYMBOL(wlan_vdev_wds_sta_chkmcecho);

void wlan_peer_leave(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}
	IEEE80211_NODE_LEAVE(ni);
}

EXPORT_SYMBOL(wlan_peer_leave);

void wlan_peer_set_beacon_rstamp(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ni->ni_beacon_rstamp = OS_GET_TIMESTAMP();
}

EXPORT_SYMBOL(wlan_peer_set_beacon_rstamp);

void wlan_peer_set_probe_ticks(struct wlan_objmgr_peer *peer, int val)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	ni->ni_probe_ticks = val;
}

EXPORT_SYMBOL(wlan_peer_set_probe_ticks);

struct wlan_objmgr_peer *wlan_create_tmp_peer(struct wlan_objmgr_vdev *vdev,
					      const u_int8_t * macaddr)
{
	wlan_if_t vap;
	struct ieee80211_node *ni = NULL;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return NULL;
	}

	ni = ieee80211_tmp_node(vap, macaddr);
	if (ni)
		return ni->peer_obj;
	else
		return NULL;
}

EXPORT_SYMBOL(wlan_create_tmp_peer);

void wlan_peer_delete(struct wlan_objmgr_peer *peer)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return;
	}

	wlan_objmgr_delete_node(ni);
}

EXPORT_SYMBOL(wlan_peer_delete);

#if UMAC_SUPPORT_VI_DBG
void wlan_vdev_vi_dbg_input(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	ieee80211_vi_dbg_input(vap, wbuf);
}

EXPORT_SYMBOL(wlan_vdev_vi_dbg_input);
#endif

void wlan_vdev_nawds_learn(struct wlan_objmgr_vdev *vdev, u_int8_t * sender)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}
	IEEE80211_NAWDS_LEARN(vap, sender);
}

EXPORT_SYMBOL(wlan_vdev_nawds_learn);

int
wlan_peer_recv_ctrl(struct wlan_objmgr_peer *peer,
		    wbuf_t wbuf, int subtype, struct ieee80211_rx_status *rs)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return -EINVAL;
	}

	return ieee80211_recv_ctrl(ni, wbuf, subtype, rs);
}

EXPORT_SYMBOL(wlan_peer_recv_ctrl);

int wlan_vdev_rx_gate(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return -EINVAL;
	}
	return OS_ATOMIC_CMPXCHG(&vap->iv_rx_gate, 0, 1);
}

EXPORT_SYMBOL(wlan_vdev_rx_gate);

void wlan_vdev_rx_ungate(struct wlan_objmgr_vdev *vdev)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return;
	}

	(void)OS_ATOMIC_CMPXCHG(&vap->iv_rx_gate, 1, 0);
}

EXPORT_SYMBOL(wlan_vdev_rx_ungate);

int wlan_vdev_deauth_request(struct wlan_objmgr_vdev *vdev, u_int8_t * macaddr,
			     IEEE80211_REASON_CODE reason)
{
	wlan_if_t vap;

	vap = wlan_mlme_get_vdev_legacy_obj(vdev);
	if (vap == NULL) {
		qdf_print("%s:vap is NULL ", __func__);
		return -EINVAL;
	}
	return wlan_mlme_deauth_request(vap, macaddr, reason);
}

EXPORT_SYMBOL(wlan_vdev_deauth_request);

int
wlan_cmn_input_mgmt(struct wlan_objmgr_peer *peer,
		    wbuf_t wbuf, struct ieee80211_rx_status *rs)
{
	struct ieee80211_node *ni;

	ni = wlan_mlme_get_peer_legacy_obj(peer);
	if (ni == NULL) {
		qdf_print("%s:ni is NULL ", __func__);
		return -EINVAL;
	}

	return ieee80211_input(ni, wbuf, rs);
}

EXPORT_SYMBOL(wlan_cmn_input_mgmt);

int wlan_me_Convert(struct wlan_objmgr_vdev *vdev, wbuf_t wbuf,
		    u_int8_t newmac[][IEEE80211_ADDR_LEN], uint8_t newmaccnt)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_me_Convert)
		return dops->vdev_me_Convert(vdev, wbuf, newmac, newmaccnt);
	else
		return EOK;
}

void wlan_me_hifi_forward(struct wlan_objmgr_vdev *vdev,
			  wbuf_t wbuf, struct wlan_objmgr_peer *peer)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	if (dops && dops->vdev_me_hifi_forward)
		dops->vdev_me_hifi_forward(vdev, wbuf, peer);
}
