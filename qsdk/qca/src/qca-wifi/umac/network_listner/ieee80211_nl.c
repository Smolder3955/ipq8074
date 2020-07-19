/*
 * Copyright (c) 2015,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#if QCA_LTEU_SUPPORT

#include <ieee80211.h>
#include <ieee80211_var.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_scan_ucfg_api.h>
#include <ieee80211_nl.h>

int
ieee80211_init_mu_scan(struct ieee80211com *ic, ieee80211req_mu_scan_t *reqptr)
{
    struct ieee80211_nl_handle *nl_handle = ic->ic_nl_handle;
    int mu_in_progress, scan_in_progress;

    if (!ic->ic_nl_handle) {
        printk("%s: MU supported only on NL radio\n", __func__);
        return -EINVAL;
    }

    spin_lock_bh(&nl_handle->mu_lock);
    mu_in_progress = atomic_read(&nl_handle->mu_in_progress);
    scan_in_progress = atomic_read(&nl_handle->scan_in_progress);
    if (mu_in_progress || scan_in_progress) {
        printk("%s: Currently doing %s %d\n", __func__,
               mu_in_progress ? "MU" : "scan", mu_in_progress ?
               nl_handle->mu_id : nl_handle->scan_id);
        spin_unlock_bh(&nl_handle->mu_lock);
        return -EBUSY;
    }

    atomic_set(&nl_handle->mu_in_progress, 1);
    nl_handle->mu_id = reqptr->mu_req_id;
    nl_handle->mu_channel = reqptr->mu_channel;
    nl_handle->mu_duration = reqptr->mu_duration;
    spin_unlock_bh(&nl_handle->mu_lock);

    return 0;
}

int
ieee80211_mu_scan(struct ieee80211com *ic, ieee80211req_mu_scan_t *reqptr)
{
    struct ieee80211_nl_handle *nl_handle = ic->ic_nl_handle;
    u_int8_t mu_algo_max_val = (1 << MU_MAX_ALGO)-1;
    int retv;

    if ( (reqptr->mu_type == 0) || (reqptr->mu_type > mu_algo_max_val) ||
         (reqptr->mu_duration == 0) ) {
        if ( (reqptr->mu_type == 0) || (reqptr->mu_type > mu_algo_max_val) )
            printk("%s: Invalid value entered for MU algo type %u \n",
                    __func__,reqptr->mu_type);
        else if (reqptr->mu_duration == 0)
            printk("%s: Invalid value entered for MU scan duration %u \n",
                    __func__,reqptr->mu_duration);
        return -EINVAL;
    }

    retv = nl_handle->mu_scan(ic, reqptr->mu_req_id, reqptr->mu_duration,
                              reqptr->mu_type, reqptr->lteu_tx_power,
                              reqptr->mu_rssi_thr_bssid, reqptr->mu_rssi_thr_sta,
                              reqptr->mu_rssi_thr_sc, reqptr->home_plmnid, reqptr->alpha_num_bssid);
    if (retv == -1)
        return -ENOMEM;
    else
        return retv;
}

void
ieee80211_mu_scan_fail(struct ieee80211com *ic)
{
    struct ieee80211_nl_handle *nl_handle = ic->ic_nl_handle;
    atomic_set(&nl_handle->mu_in_progress, 0);
}

int
ieee80211_lteu_config(struct ieee80211com *ic,
                      ieee80211req_lteu_cfg_t *reqptr, u_int32_t wifi_tx_power)
{
    struct ieee80211_nl_handle *nl_handle = ic->ic_nl_handle;

    if (!ic->ic_nl_handle) {
        printk("%s: LTEu config supported only on NL radio\n", __func__);
        return -EINVAL;
    }

    if (atomic_read(&nl_handle->mu_in_progress) ||
        atomic_read(&nl_handle->scan_in_progress)) {
        printk("%s: LTEu config while doing MU/scan\n", __func__);
    }

    if (reqptr->lteu_num_bins >= LTEU_MAX_BINS)
        reqptr->lteu_num_bins = LTEU_MAX_BINS;

    nl_handle->use_gpio_start = reqptr->lteu_gpio_start;

    return nl_handle->lteu_config(ic, reqptr, wifi_tx_power);
}

void
ieee80211_nl_register_handler(struct ieee80211vap *vap,
             void (*mu_handler)(wlan_if_t, struct event_data_mu_rpt *),
             scan_event_handler scan_handler)
{
    struct ieee80211_nl_handle *nl_handle = vap->iv_ic->ic_nl_handle;
    struct wlan_objmgr_psoc *psoc = NULL;

    spin_lock_bh(&nl_handle->mu_lock);

    psoc = wlan_vap_get_psoc(vap);
    nl_handle->scanreq = ucfg_scan_register_requester(psoc, (uint8_t *)"network_listner",
            scan_handler, (void *)nl_handle);
    if (!nl_handle->scanreq) {
        scan_err("unable to allocate requester");
    }
    if (nl_handle->mu_cb == NULL && nl_handle->mu_cb_arg == NULL) {
        nl_handle->mu_cb = mu_handler;
        nl_handle->mu_cb_arg = vap;
    } else
        printk("%s: can't register MU handler\n", __func__);

    spin_unlock_bh(&nl_handle->mu_lock);
}

void
ieee80211_nl_unregister_handler(struct ieee80211vap *vap,
             void (*mu_handler)(wlan_if_t, struct event_data_mu_rpt *),
             scan_event_handler scan_handler)
{
    struct ieee80211_nl_handle *nl_handle = vap->iv_ic->ic_nl_handle;
    struct wlan_objmgr_psoc *psoc = NULL;

    spin_lock_bh(&nl_handle->mu_lock);

    if (nl_handle->mu_cb == mu_handler && nl_handle->mu_cb_arg == vap) {
        nl_handle->mu_cb = NULL;
        nl_handle->mu_cb_arg = NULL;
    } else
        printk("%s: can't unregister MU handler\n", __func__);
    psoc = wlan_vap_get_psoc(vap);
    ucfg_scan_unregister_requester(psoc, nl_handle->scanreq);
    spin_unlock_bh(&nl_handle->mu_lock);
}

int
ieee80211_ap_scan(struct ieee80211com *ic,
                  struct ieee80211vap *vap, ieee80211req_ap_scan_t *reqptr)
{
    struct ieee80211_nl_handle *nl_handle = ic->ic_nl_handle;
    int n_ssid, i;
    struct scan_start_request *scan_params = NULL;
    ieee80211_ssid ssid_list[IEEE80211_SCAN_MAX_SSID];
    IEEE80211_SCAN_PRIORITY scan_priority;
    u_int32_t channel_list[MAX_SCAN_CHANS];
    int mu_in_progress, scan_in_progress;
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    struct wlan_objmgr_vdev *vdev = NULL;
    struct wlan_objmgr_pdev *pdev = NULL;

    if (!ic->ic_nl_handle) {
        printk("%s: Scan supported only on NL radio\n", __func__);
        return -EINVAL;
    }

    vdev = vap->vdev_obj;
    status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_OSIF_SCAN_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        scan_info("unable to get reference");
        return -EBUSY;
    }

    spin_lock_bh(&nl_handle->mu_lock);
    mu_in_progress = atomic_read(&nl_handle->mu_in_progress);
    scan_in_progress = atomic_read(&nl_handle->scan_in_progress);
    if (mu_in_progress || scan_in_progress) {
        printk("%s: Currently doing %s %d\n", __func__,
               mu_in_progress ? "MU" : "scan", mu_in_progress ?
               nl_handle->mu_id : nl_handle->scan_id);
        spin_unlock_bh(&nl_handle->mu_lock);
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        return -EBUSY;
    }

    atomic_set(&nl_handle->scan_in_progress, 1);
    nl_handle->scan_id = reqptr->scan_req_id;
    spin_unlock_bh(&nl_handle->mu_lock);
    nl_handle->force_vdev_restart = 1;

    scan_params = (struct scan_start_request *)
        qdf_mem_malloc(sizeof(*scan_params));
    if (scan_params == NULL) {
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        printk("%s: Can't alloc memory for scan id=%d\n", __func__, reqptr->scan_req_id);
        atomic_set(&nl_handle->scan_in_progress, 0);
        return -ENOMEM;
    }
    OS_MEMZERO(scan_params, sizeof(*scan_params));
    if (ic->ic_nl_handle) {
        /*
         * Following parameters hard coded to ensure probe requests sent during
         * AP scan are sent with wildcard SSIDs and not with the AP's own SSID
         */
        n_ssid = 1;
        ssid_list[0].len = 0;
    }
    else {
        n_ssid = wlan_get_desired_ssidlist(vap, ssid_list, IEEE80211_SCAN_MAX_SSID);
    }

    status = wlan_update_scan_params(vap, scan_params, IEEE80211_M_HOSTAP,
            true, true, true, true, n_ssid, ssid_list, 0);
    if (status) {
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        atomic_set(&nl_handle->scan_in_progress, 0);
        qdf_mem_free(scan_params);
        return -EINVAL;
    }

    scan_params->scan_req.scan_flags = 0;
    if (reqptr->scan_type == SCAN_ACTIVE)
        scan_params->scan_req.scan_f_passive = false;
    else
        scan_params->scan_req.scan_f_passive = true;

    if (reqptr->scan_num_chan) {
        if (reqptr->scan_num_chan > MAX_SCAN_CHANS)
            reqptr->scan_num_chan = MAX_SCAN_CHANS;
        for (i = 0; i < reqptr->scan_num_chan; i++)
            channel_list[i] = reqptr->scan_channel_list[i];

        status = ucfg_scan_init_chanlist_params(scan_params,
                reqptr->scan_num_chan, channel_list, NULL);
        if (status) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
            atomic_set(&nl_handle->scan_in_progress, 0);
            qdf_mem_free(scan_params);
            return -EINVAL;
        }
    } else {
        scan_params->scan_req.scan_f_2ghz = true;
        scan_params->scan_req.scan_f_5ghz = true;
    }

    scan_params->legacy_params.scan_type = SCAN_TYPE_FOREGROUND;

    if (reqptr->scan_duration) {
        if (reqptr->scan_type == SCAN_ACTIVE) {
            scan_params->legacy_params.min_dwell_active =
                scan_params->scan_req.dwell_time_active = reqptr->scan_duration;
        } else {
            scan_params->legacy_params.min_dwell_passive =
                scan_params->scan_req.dwell_time_passive = reqptr->scan_duration;
        }
    } else {
        scan_params->legacy_params.min_dwell_passive = vap->min_dwell_time_passive;
        scan_params->scan_req.dwell_time_passive = vap->max_dwell_time_passive;
    }

    if (reqptr->scan_repeat_probe_time != (u_int32_t)-1)
        scan_params->scan_req.repeat_probe_time = reqptr->scan_repeat_probe_time;

    if (reqptr->scan_rest_time != (u_int32_t)-1) {
        scan_params->scan_req.min_rest_time =
            scan_params->scan_req.max_rest_time = reqptr->scan_rest_time;
    }

    if (reqptr->scan_idle_time != (u_int32_t)-1)
        scan_params->scan_req.idle_time = reqptr->scan_idle_time;
    if (reqptr->scan_probe_delay != (u_int32_t)-1)
        scan_params->scan_req.probe_delay = reqptr->scan_probe_delay;

    scan_priority = SCAN_PRIORITY_HIGH;

    pdev = wlan_vdev_get_pdev(vdev);
    ucfg_scan_flush_results(pdev, NULL);

    status = wlan_ucfg_scan_start(vap, scan_params,
            nl_handle->scanreq, scan_priority, &nl_handle->scanid, 0, NULL);
    if (status) {
        scan_err("scan_start failed scan id=%d, status: %d",
                reqptr->scan_req_id, status);
    } else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_NL, "%s: Scan started "
                "id=%d\n", __func__, nl_handle->scan_id);
    }

    wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
    return status;
}

#endif /* QCA_LTEU_SUPPORT */
