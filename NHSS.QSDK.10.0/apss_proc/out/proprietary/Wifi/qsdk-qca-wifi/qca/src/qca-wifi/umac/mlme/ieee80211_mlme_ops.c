/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
#include "ieee80211_var.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_global_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include <ieee80211_objmgr_priv.h>
#include "ieee80211_power.h"
#include "ieee80211_mlme_ops.h"
#include "ieee80211_channel.h"

void mlme_ic_scan_start(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic = NULL;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (ic)
        ic->ic_scan_start(ic);
    else
        qdf_err("ic is NULL");
}

static void ieee80211_scan_handle_sta_power_events(struct ieee80211vap *vap, ieee80211_sta_power_event *event, void *arg)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct wlan_objmgr_psoc *psoc = NULL;
    struct wlan_objmgr_pdev *pdev = ic->ic_pdev_obj;
    
    psoc = wlan_pdev_get_psoc(pdev);

    if (ieee80211_resmgr_active(ic)) {
        qdf_print("%s: Error: unexpected sta power event:%d", __func__, event->type);

        return;
    }

    if (!psoc->soc_cb.tx_ops.mops.scan_sta_power_events)
        return;
    psoc->soc_cb.tx_ops.mops.scan_sta_power_events(ic->ic_pdev_obj, event->type, event->status);

}

static void ieee80211_scan_handle_vap_events(ieee80211_vap_t vap, ieee80211_vap_event *event, void *arg)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct wlan_objmgr_psoc *psoc = NULL;
    struct wlan_objmgr_pdev *pdev = ic->ic_pdev_obj;
    
    psoc = wlan_pdev_get_psoc(pdev);

    switch(event->type)
    {
    case IEEE80211_VAP_FULL_SLEEP:
    case IEEE80211_VAP_DOWN:
	    if (!psoc->soc_cb.tx_ops.mops.scan_connection_lost)
            return;
        psoc->soc_cb.tx_ops.mops.scan_connection_lost(ic->ic_pdev_obj);
        break;

    default:
        break;
    }
}

void mlme_register_pm_event_handler(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_if_t vap;
    struct ieee80211com *ic = NULL;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);

    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if(vdev == NULL) {
       qdf_print("%s:vdev is not found (id:%d) ", __func__, vdev_id);
       return;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);

    if(vap == NULL)
    {
        qdf_print("%s:vap is NULL", __func__);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
        return;
    }
    if (! ieee80211_resmgr_active(ic)) {
        ieee80211_sta_power_register_event_handler(vap,
                                                   ieee80211_scan_handle_sta_power_events,
                                                   NULL);
    }

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
}

void mlme_unregister_pm_event_handler(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_if_t vap;
    struct ieee80211com *ic = NULL;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);

    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if(vdev == NULL) {
       qdf_print("%s:vdev is not found (id:%d) ", __func__, vdev_id);
       return;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);

    if(vap == NULL)
    {
        qdf_print("%s:vap is NULL", __func__);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
        return;
    }
    if (! ieee80211_resmgr_active(ic)) {
        ieee80211_sta_power_unregister_event_handler(vap,
                                                   ieee80211_scan_handle_sta_power_events,
                                                   NULL);
    }
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
}

QDF_STATUS mlme_register_vap_event_handler(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_if_t vap;
    struct ieee80211com *ic = NULL;
	QDF_STATUS status;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);

    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if(vdev == NULL) {
       qdf_print("%s:vdev is not found (id:%d) ", __func__, vdev_id);
       return QDF_STATUS_E_FAILURE;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);

    if(vap == NULL)
    {
        qdf_print("%s:vap is NULL", __func__);
        status = QDF_STATUS_E_FAILURE;
		goto exit;
    }

    if(ieee80211_vap_register_event_handler(vap,
                                             ieee80211_scan_handle_vap_events,
                                             NULL))
        status = QDF_STATUS_E_FAILURE;
    else
        status = QDF_STATUS_SUCCESS;

exit:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return status;
}

QDF_STATUS mlme_unregister_vap_event_handler(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_if_t vap;
    struct ieee80211com *ic = NULL;
	QDF_STATUS status;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);

    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if(vdev == NULL) {
       qdf_print("%s:vdev is not found (id:%d) ", __func__, vdev_id);
       return QDF_STATUS_E_FAILURE;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);

    if(vap == NULL)
    {
        qdf_print("%s:vap is NULL", __func__);
        status = QDF_STATUS_E_FAILURE;
		goto exit;
    }

    if(ieee80211_vap_unregister_event_handler(vap,
                                             ieee80211_scan_handle_vap_events,
                                             NULL))
        status = QDF_STATUS_E_FAILURE;
    else
        status = QDF_STATUS_SUCCESS;

exit:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return status;
}

int mlme_send_probe_request(struct wlan_objmgr_pdev *pdev,
                                uint8_t vdev_id,
                                u_int8_t  *destination,
                                u_int8_t  *bssid,
                                u_int8_t  *ssid,
                                u_int32_t  ssidlen,
                                u_int8_t  *ie,
                                size_t len)

{
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_if_t vap;
    struct ieee80211com *ic = NULL;
    int status = ENXIO;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);

    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if(vdev == NULL) {
       qdf_print("%s:vdev is not found (id:%d) ", __func__, vdev_id);
       return QDF_STATUS_E_FAILURE;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);

    if(vap == NULL)
    {
        qdf_print("%s:vap is NULL", __func__);
		status = ENXIO;
		goto exit;
    }

    status = wlan_send_probereq(vap,
                       destination,
                       bssid,
                       ssid,
                       ssidlen,
                       ie,
                       len);

exit:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return status;
}

/* Request ID used to communicate to Resource Manager */
#define SCANNER_RESMGR_REQID                           77
int mlme_resmgr_request_bsschan(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic = NULL;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);

    return ieee80211_resmgr_request_bsschan(ic->ic_resmgr, SCANNER_RESMGR_REQID);
}

int mlme_resmgr_request_offchan(struct wlan_objmgr_pdev *pdev, u_int32_t freq, u_int32_t flags, u_int32_t estimated_offchannel_time)
{
    struct ieee80211_ath_channel *chan = NULL;
    struct ieee80211com *ic = NULL;
    int ieee;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic)
    {
        qdf_err("ic is NULL");
        return EFAULT;
    }

    ieee = ieee80211_mhz2ieee(ic, freq, flags);
    chan = ieee80211_find_dot11_channel(ic, ieee, 0, IEEE80211_MODE_AUTO);
    if(!chan)
    {
        printk("%s:%d:Error finding channel\n", __func__, __LINE__);
        return EFAULT;
    }

    return ieee80211_resmgr_request_offchan(ic->ic_resmgr,
                                              chan,
                                              SCANNER_RESMGR_REQID,
                                              IEEE80211_RESMGR_MAX_LEAVE_BSS_DELAY,
                                              estimated_offchannel_time);
}

int mlme_resmgr_active(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic = NULL;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);

    if (ic)
       return ieee80211_resmgr_active(ic);
    else
       return false;
}

int mlme_get_cw_inter_found(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic = NULL;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);

    if (ic)
        return (ic->cw_inter_found);
    else
       return 0;
}

int mlme_set_home_channel(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_if_t vap;
    struct ieee80211com *ic = NULL;
	int status;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return EFAULT;
    }

    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if(vdev == NULL) {
       qdf_print("%s:vdev is not found (id:%d) ", __func__, vdev_id);
       return EFAULT;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);

    if(vap == NULL)
    {
        qdf_print("%s:vap is NULL", __func__);
        status = EFAULT;
		goto exit;
    }

    status = ieee80211_set_channel(ic, vap->iv_bsschan);

exit:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return status;
}

int mlme_set_channel(struct wlan_objmgr_pdev *pdev, u_int32_t freq, u_int32_t flags)
{
    struct ieee80211_ath_channel *chan = NULL;
    struct ieee80211com *ic = NULL;
    int ieee;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return EFAULT;
    }

    ieee = ieee80211_mhz2ieee(ic, freq, flags);
    chan = ieee80211_find_dot11_channel(ic, ieee, 0, IEEE80211_MODE_AUTO);
    if(!chan)
    {
        printk("%s:%d:Error finding channel\n", __func__, __LINE__);
        return EFAULT;
    }

    return ieee80211_set_channel(ic, chan);
}

#if WLAN_SPECTRAL_ENABLE
void mlme_start_record_stats(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic = NULL;
    struct ieee80211_chan_stats chan_stats;
    u_int16_t chan_num;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return;
    }

    chan_num = wlan_channel_ieee(ic->ic_curchan);

    (void) ic->ic_get_cur_chan_stats(ic, &chan_stats);
    ic->chan_clr_cnt = chan_stats.chan_clr_cnt;
    ic->cycle_cnt = chan_stats.cycle_cnt;
    ic->chan_num = chan_num;
}

void mlme_end_record_stats(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic = NULL;
    struct ieee80211_chan_stats chan_stats;
    u_int16_t chan_num;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return;
    }

    chan_num = wlan_channel_ieee(ic->ic_curchan);
    (void) ic->ic_get_cur_chan_stats(ic, &chan_stats);

    ic->ic_record_chan_info(ic,
                            chan_num,
                            true,  /* Whether Channel cycle
                                      counts are valid */
                            chan_stats.chan_clr_cnt,
                            ic->chan_clr_cnt,
                            chan_stats.cycle_cnt,
                            ic->cycle_cnt,
                            false,  /* Whether NF is valid */
                            0,      /* NF */
                            false,  /* Whether PER is valid */
                            0);     /* PER */
}
#endif /* WLAN_SPECTRAL_ENABLE */

int mlme_get_enh_rpt_ind(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic = NULL;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return 0;
    }

    return (ieee80211_ic_enh_ind_rpt_is_set(ic));
}

int mlme_ic_pause(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic = NULL;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return 0;
    }

    return ieee80211_ic_pause(ic);
}

void mlme_ic_unpause(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic = NULL;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return;
    }

    ieee80211_ic_unpause(ic);
}

int mlme_ic_vap_pause_control(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_if_t vap;
    struct ieee80211com *ic = NULL;
	int status;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return EFAULT;
    }
    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if(vdev == NULL) {
       qdf_print("%s:vdev is not found (id:%d) ", __func__, vdev_id);
       return EFAULT;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);

    if(vap == NULL)
    {
        qdf_print("%s:vap is NULL", __func__);
        status = EFAULT;
		goto exit;
    }

    status = ic->ic_vap_pause_control(ic,vap,false);

exit:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return status;
}

int mlme_sta_power_pause(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id, u_int32_t timeout)
{
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_if_t vap;
    struct ieee80211com *ic = NULL;
	int status;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return EFAULT;
    }

    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if(vdev == NULL) {
       qdf_print("%s:vdev is not found (id:%d) ", __func__, vdev_id);
       return EFAULT;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);

    if(vap == NULL)
    {
        qdf_err("vap is NULL");
        status = EFAULT;
	goto exit;
    }

    status = ieee80211_sta_power_pause(vap, timeout);

exit:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return status;
}

int mlme_sta_power_unpause(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_if_t vap;
    struct ieee80211com *ic = NULL;
	int status;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return EFAULT;
    }

    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if(vdev == NULL) {
       qdf_print("%s:vdev is not found (id:%d) ", __func__, vdev_id);
       return EFAULT;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);

    if(vap == NULL)
    {
        qdf_print("%s:vap is NULL", __func__);
        status = EFAULT;
		goto exit;
    }

    status = ieee80211_sta_power_unpause(vap);

exit:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return status;
}

int mlme_set_vap_sleep(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_if_t vap;
    struct ieee80211com *ic = NULL;
	int status = EOK;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return EFAULT;
    }

    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if(vdev == NULL) {
       qdf_print("%s:vdev is not found (id:%d) ", __func__, vdev_id);
       return EFAULT;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);

    if(vap == NULL)
    {
        qdf_err("vap is NULL");
        status = EFAULT;
	goto exit;
    }

    IEEE80211_VAP_GOTOSLEEP(vap);

exit:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return status;
}

int mlme_set_vap_wakeup(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_if_t vap;
    struct ieee80211com *ic = NULL;
	int status = EOK;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return EFAULT;
    }

    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if(vdev == NULL) {
       qdf_print("%s:vdev is not found (id:%d) ", __func__, vdev_id);
       return EFAULT;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);

    if(vap == NULL)
    {
        qdf_print("%s:vap is NULL", __func__);
        status = EFAULT;
		goto exit;
    }

    IEEE80211_VAP_WAKEUP(vap);

exit:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return status;
}

qdf_time_t mlme_get_traffic_indication_timestamp(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic = NULL;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return 0;
    }

    return ieee80211com_get_traffic_indication_timestamp(ic);
}

int mlme_get_acs_in_progress(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id)
{
    struct wlan_objmgr_vdev *vdev = NULL;
    wlan_if_t vap;
    struct ieee80211com *ic = NULL;
	int status = EOK;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return EFAULT;
    }

    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_MLME_SB_ID);
    if(vdev == NULL) {
       qdf_print("%s:vdev is not found (id:%d) ", __func__, vdev_id);
       return EFAULT;
    }

    vap = wlan_vdev_get_mlme_ext_obj(vdev);

    if(vap == NULL)
    {
        qdf_print("%s:vap is NULL", __func__);
        status = EFAULT;
		goto exit;
    }

    if (!vap->iv_ic->ic_acs)
	{
        qdf_print("%s:ic_acs is NULL", __func__);
        status = EFAULT;
		goto exit;
	}

    status = wlan_autoselect_in_progress(vap);

exit:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
	return status;
}

void mlme_end_scan(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic = NULL;
    struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
       qdf_err("ic is NULL");
       return;
    }

    if (!psoc->soc_cb.tx_ops.mops.scan_end)
        return;

    psoc->soc_cb.tx_ops.mops.scan_end(ic->ic_pdev_obj);
}

u_int8_t mlme_unified_stats_status(struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic = NULL;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
        return 0;
    }

    return ic->ic_unified_stats;
}

void wlan_register_mlme_ops(struct wlan_objmgr_psoc *psoc)
{
	WLAN_DEV_TYPE dev_type;
    struct wlan_lmac_if_mlme_rx_ops *mlme_rx_ops = NULL;
    struct wlan_lmac_if_mlme_tx_ops *mlme_tx_ops = NULL;

	dev_type = psoc->soc_nif.phy_type;
    mlme_rx_ops =  &psoc->soc_cb.rx_ops.mops;
    mlme_tx_ops =  &psoc->soc_cb.tx_ops.mops;

    if(dev_type == WLAN_DEV_DA)
    {
        mlme_rx_ops->wlan_mlme_scan_start = mlme_ic_scan_start;
        mlme_rx_ops->wlan_mlme_register_pm_event_handler = mlme_register_pm_event_handler;
        mlme_rx_ops->wlan_mlme_unregister_pm_event_handler = mlme_unregister_pm_event_handler;
        mlme_rx_ops->wlan_mlme_register_vdev_event_handler = mlme_register_vap_event_handler;
        mlme_rx_ops->wlan_mlme_unregister_vdev_event_handler = mlme_unregister_vap_event_handler;
        mlme_rx_ops->wlan_mlme_send_probe_request = mlme_send_probe_request;
        mlme_rx_ops->wlan_mlme_resmgr_request_bsschan = mlme_resmgr_request_bsschan;
        mlme_rx_ops->wlan_mlme_resmgr_request_offchan = mlme_resmgr_request_offchan;
        mlme_rx_ops->wlan_mlme_resmgr_active = mlme_resmgr_active;
        mlme_rx_ops->wlan_mlme_get_cw_inter_found = mlme_get_cw_inter_found;
        mlme_rx_ops->wlan_mlme_set_home_channel = mlme_set_home_channel;
        mlme_rx_ops->wlan_mlme_set_channel = mlme_set_channel;
#if WLAN_SPECTRAL_ENABLE
        mlme_rx_ops->wlan_mlme_start_record_stats = mlme_start_record_stats;
        mlme_rx_ops->wlan_mlme_end_record_stats = mlme_end_record_stats;
#endif /* WLAN_SPECTRAL_ENABLE */
        mlme_rx_ops->wlan_mlme_get_enh_rpt_ind = mlme_get_enh_rpt_ind;
        mlme_rx_ops->wlan_mlme_pause = mlme_ic_pause;
        mlme_rx_ops->wlan_mlme_unpause = mlme_ic_unpause;
        mlme_rx_ops->wlan_mlme_vdev_pause_control = mlme_ic_vap_pause_control;
        mlme_rx_ops->wlan_mlme_sta_power_pause = mlme_sta_power_pause;
        mlme_rx_ops->wlan_mlme_sta_power_unpause = mlme_sta_power_unpause;
        mlme_rx_ops->wlan_mlme_set_vdev_sleep = mlme_set_vap_sleep;
        mlme_rx_ops->wlan_mlme_set_vdev_wakeup = mlme_set_vap_wakeup;
        mlme_rx_ops->wlan_mlme_get_traffic_indication_timestamp = mlme_get_traffic_indication_timestamp;
        mlme_rx_ops->wlan_mlme_get_acs_in_progress = mlme_get_acs_in_progress;
        mlme_rx_ops->wlan_mlme_end_scan = mlme_end_scan;
    }
    else if(dev_type ==  WLAN_DEV_OL)
    {
        mlme_rx_ops->wlan_mlme_scan_start = NULL;
    }
}

