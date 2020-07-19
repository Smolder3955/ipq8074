/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
*/

/*
 *This File provides basic framework for SON for direct
 * attach architecture.
 */
#include "wlan_son_internal.h"
#include "wlan_son_pub.h"
#if QCA_SUPPORT_SON
/**
 * @brief Get correct sample interval for channel utlization for direct
 * attach hardware.
 * @param [in] pdev for which command is executed.
 * @param [in] sample period
 * @param [in] number of sample.
 * @return EOK if successful otherwise -EINVAL
 */

int8_t son_da_sanitize_util_intvl(struct wlan_objmgr_pdev *pdev,
				  u_int32_t *sample_period,
				  u_int32_t *num_sample)
{
	u_int32_t conf_average_period = 0;
#define MIN_SAMPLE_PERIOD_DIRECT_ATTACH 30
	conf_average_period = *sample_period * *num_sample;

	*sample_period = MIN_SAMPLE_PERIOD_DIRECT_ATTACH;

	if ((conf_average_period % *num_sample)!= 0) {
		SON_LOGW("For direct Attach:Incompatible Sample Period and average number of samples \n");
		return -EINVAL;
	}

	*num_sample = conf_average_period / *sample_period;
	SON_LOGI("For direct Attach:Changing sampling period to %d \n",MIN_SAMPLE_PERIOD_DIRECT_ATTACH);
	SON_LOGI("For direct Attach:Changing Average number of sample to %d \n",*num_sample);

#undef MIN_SAMPLE_PERIOD_DIRECT_ATTACH

	return EOK;
}
EXPORT_SYMBOL(son_da_sanitize_util_intvl);

bool son_da_enable(struct wlan_objmgr_pdev *pdev, bool enable)
{
	struct son_pdev_priv *pd_priv = NULL;
#define STA_STATS_UPDATE_INTVL_DEF    500 /* Unit is msecs */
	if(!pdev)
		return false;

	pd_priv = wlan_son_get_pdev_priv(pdev);

	if (!(pd_priv->sta_stats_update_interval_da))
		pd_priv->sta_stats_update_interval_da = STA_STATS_UPDATE_INTVL_DEF;

	/* if acs busy we don't need delayed report for direct attach hardware */

	pd_priv->son_delayed_ch_load_rpt = true;

	/*till RSSI changes by 2 db we dont send any xing events */

	pd_priv->son_rssi_xing_report_delta = 2;

	SON_LOGI("band steering initialized for direct attach hardware \n");

	if(enable) {
		qdf_timer_start(&pd_priv->son_interference_stats_timer,
				pd_priv->sta_stats_update_interval_da);
	} else {
		qdf_timer_stop(&pd_priv->son_interference_stats_timer);
	}

#undef STA_STATS_UPDATE_INTERVAL_DA

	return EOK;
}
EXPORT_SYMBOL(son_da_enable);

QDF_STATUS son_da_send_null(struct wlan_objmgr_pdev *pdev,
			    u_int8_t *macaddr,
			    struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	QDF_STATUS retv = QDF_STATUS_SUCCESS;

	if (!wlan_son_is_pdev_enabled(pdev)) {
		return QDF_STATUS_E_INVAL;
	}

	if (!wlan_son_is_vdev_enabled(vdev)) {
		return QDF_STATUS_E_INVAL;
	}

	psoc = wlan_pdev_get_psoc(pdev);

	peer = wlan_objmgr_get_peer(psoc,
				    wlan_objmgr_pdev_get_pdev_id(pdev),
				    macaddr,
				    WLAN_SON_ID);

	if(!peer) {
		return QDF_STATUS_E_INVAL;
	}

	wlan_peer_mlme_flag_set(peer, WLAN_PEER_F_BSTEERING_CAPABLE);

	retv = wlan_peer_send_null(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
	return retv;
}
EXPORT_SYMBOL(son_da_send_null);

static void son_iterate_peer_interferecne_handler(struct wlan_objmgr_pdev *pdev,
						  void *obj,
						  void *arg)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)obj;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct son_peer_priv *pe_priv = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct bs_sta_stats_ind sta_stats = {0};
	bool rssi_changed = false;
	struct wlan_objmgr_vdev *current_vdev = NULL;
	u_int32_t txrate;
	u_int8_t ni_rssi = 0;

	vdev = wlan_peer_get_vdev(peer);

	pe_priv = wlan_son_get_peer_priv(peer);
	if (!(peer->peer_mlme.peer_flags & WLAN_PEER_F_AUTH) ||
	    !wlan_son_is_vdev_enabled(vdev)) {
		/* Inactivity check only interested in connected node */
		return;
	}

	psoc = wlan_pdev_get_psoc(pdev);

	txrate = SON_TARGET_IF_GET_PEER_RATE(peer, IEEE80211_LASTRATE_TX);
	ni_rssi = wlan_peer_get_rssi(peer);

	if (!(pe_priv->son_last_rssi))
		pe_priv->son_last_rssi = ni_rssi;

	if (ni_rssi != pe_priv->son_last_rssi) {
		pe_priv->son_last_rssi = ni_rssi;
		rssi_changed = true;
	}

	/* TBD: Why ni->ni_stats.ns_last_tx_rate is not equal to value retrieved from an */
	if (pe_priv->son_last_tx_rate_da != txrate)
		pe_priv->son_last_tx_rate_da = txrate;

	if (rssi_changed && pe_priv->son_last_tx_rate_da) {
		/* As per DP stats convergence FR we modify way of updation to SON
		 * with new change, following API should be called with modified signature
		 * from SON component itself.
		 * to satisy compilation issues passing NULL as last argument to this API
		 */
		if (son_update_sta_stats( peer, vdev, &sta_stats, NULL))
			current_vdev = vdev;
	}

	if (current_vdev && (sta_stats.peer_count != 0)) {
		son_send_sta_stats_event(vdev, &sta_stats);
	}

	return;
}

u_int32_t son_da_get_peer_rate(struct wlan_objmgr_peer *peer, u_int8_t type)
{
	return wlan_peer_get_rate(peer);
}

EXPORT_SYMBOL(son_da_get_peer_rate);

static OS_TIMER_FUNC(son_da_interference_stats_event_handler)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct son_pdev_priv *pd_priv = NULL;
	OS_GET_TIMER_ARG(pdev, struct wlan_objmgr_pdev *);
	pd_priv = wlan_son_get_pdev_priv(pdev);

	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
					  son_iterate_peer_interferecne_handler,
					  (void *)pd_priv, 1,
					  WLAN_SON_ID);

	qdf_timer_start(&pd_priv->son_interference_stats_timer,
			pd_priv->sta_stats_update_interval_da);
	return;
}

int son_da_lmac_create(struct wlan_objmgr_pdev *pdev)
{
	struct son_pdev_priv *pd_priv = NULL;

	if(!pdev)
		return ENOMEM;

	pd_priv = wlan_son_get_pdev_priv(pdev);

	SON_LOGI("band steering initialized for direct attach hardware \n");
	qdf_timer_init(NULL, &pd_priv->son_interference_stats_timer,
		       son_da_interference_stats_event_handler,
		       (void *)pdev, QDF_TIMER_TYPE_WAKE_APPS);
	return EOK;

}
EXPORT_SYMBOL(son_da_lmac_create);

void  son_da_rx_rssi_update(struct wlan_objmgr_pdev *pdev ,u_int8_t *macaddr,
			    u_int8_t status ,int8_t rssi, u_int8_t subtype)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_peer *peer = NULL;

	if(!wlan_son_is_pdev_enabled(pdev))
		return;

	psoc = wlan_pdev_get_psoc(pdev);

	peer = wlan_objmgr_get_peer(psoc,
				    wlan_objmgr_pdev_get_pdev_id(pdev),
				    macaddr,
				    WLAN_SON_ID);

	if( !peer)
		return;

	if ((subtype != IEEE80211_FC0_SUBTYPE_NODATA) && (subtype != IEEE80211_FC0_SUBTYPE_QOS_NULL)) {
		son_record_peer_rssi(peer, rssi);
	} else {
		if(status) /* is it okay to differntiate based on status ? TBD :test to find out */
			son_record_inst_peer_rssi(peer, rssi);
		else
			son_record_inst_peer_rssi_err(peer);
	}

	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);
	return;
}
EXPORT_SYMBOL(son_da_rx_rssi_update);

void son_da_rx_rate_update(struct wlan_objmgr_pdev *pdev, u_int8_t *macaddress,
			   u_int8_t status ,u_int32_t txrateKbps)
{
	struct wlan_objmgr_peer *peer = NULL;
	struct son_peer_priv *pe_priv = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;

	psoc = wlan_pdev_get_psoc(pdev);

	peer = wlan_objmgr_get_peer(psoc,
				    wlan_objmgr_pdev_get_pdev_id(pdev),
				    macaddress,
				    WLAN_SON_ID);

	if( !peer) {
		SON_LOGI("%s: Requested STA %02x:%02x:%02x:%02x:%02x:%02x is not "
			 "associated\n", __func__, macaddress[0], macaddress[1],
			 macaddress[1], macaddress[3], macaddress[4], macaddress[5]);
		return;
	}

	pe_priv = wlan_son_get_peer_priv(peer);

	if (pe_priv->son_last_tx_rate_da) {
		if (txrateKbps != pe_priv->son_last_tx_rate_da)
			son_update_peer_rate(peer, txrateKbps, pe_priv->son_last_tx_rate_da);
	} else {
		son_update_peer_rate(peer, txrateKbps, pe_priv->son_last_tx_rate_da);
	}

	pe_priv->son_last_tx_rate_da = txrateKbps;
	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);

	return;
}

EXPORT_SYMBOL(son_da_rx_rate_update);

int son_da_lmac_destroy(struct wlan_objmgr_pdev *pdev)
{
	struct son_pdev_priv *pd_priv = NULL;

	if(!pdev)
		return ENOMEM;

	pd_priv = wlan_son_get_pdev_priv(pdev);

	qdf_timer_stop(&pd_priv->son_interference_stats_timer);
	qdf_timer_free(&pd_priv->son_interference_stats_timer);
	SON_LOGI("band steering terminated  for direct attach hardware \n");

	return EOK;

}
EXPORT_SYMBOL(son_da_lmac_destroy);
#else

int8_t son_da_sanitize_util_intvl(struct wlan_objmgr_pdev *pdev,
				  u_int32_t *sample_period,
				  u_int32_t *num_of_sample)
{
	return -EINVAL;

}
EXPORT_SYMBOL(son_da_sanitize_util_intvl);

u_int32_t son_da_get_peer_rate(struct wlan_objmgr_peer *peer, u_int8_t type)
{
	return 0;
}

EXPORT_SYMBOL(son_da_get_peer_rate);

bool son_da_enable (struct wlan_objmgr_pdev *pdev, bool enable)
{
	return false;

}
EXPORT_SYMBOL(son_da_enable);

QDF_STATUS son_da_send_null(struct wlan_objmgr_pdev *pdev,
			    u_int8_t *macaddr,
			    struct wlan_objmgr_vdev *vdev)
{
	return -EINVAL;
}
EXPORT_SYMBOL(son_da_send_null);

int son_da_lmac_create(struct wlan_objmgr_pdev *pdev)
{
	return -EINVAL;
}
EXPORT_SYMBOL(son_da_lmac_create);

int son_da_lmac_destroy(struct wlan_objmgr_pdev *pdev)
{
	return -EINVAL;

}
EXPORT_SYMBOL(son_da_lmac_destroy);

void  son_da_rx_rssi_update(struct wlan_objmgr_pdev *pdev ,u_int8_t *macaddres,
			    u_int8_t status ,int8_t rssi, u_int8_t subtype)
{
	return;

}
EXPORT_SYMBOL(son_da_rx_rssi_update);

void son_da_rx_rate_update(struct wlan_objmgr_pdev *pdev, u_int8_t *macaddres,
			   u_int8_t status ,u_int32_t rateKbps)
{
	return;
}
EXPORT_SYMBOL(son_da_rx_rate_update);

#endif
