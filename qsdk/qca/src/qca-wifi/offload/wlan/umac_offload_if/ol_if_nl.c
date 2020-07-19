/*
 * Copyright (c) 2015, 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#if QCA_LTEU_SUPPORT

#include "ol_if_athvar.h"
#include "osif_private.h"
#include "qdf_mem.h"
#include <ieee80211_nl.h>
#include <init_deinit_lmac.h>

static int
ol_ath_mu_report_event_handler(ol_scn_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct ieee80211com *ic;
    wmi_host_mu_report_event *event = NULL;
    wmi_host_mu_db_entry db_en, *db_entry = &db_en;
    struct event_data_mu_rpt mu_rpt;
    struct ieee80211vap *vap = NULL;
    int i;
    struct ieee80211_nl_handle *nl_handle;
    void (*mu_cb)(wlan_if_t cb_arg, struct event_data_mu_rpt *mu_rpt) = NULL;
    wlan_if_t mu_cb_arg = NULL;
    struct common_wmi_handle *pdev_wmi_handle;
    struct wlan_objmgr_pdev *pdev;

    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0, WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_print("%s: pdev object (id: 0) is NULL", __func__);
         return -1;
    }

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (ic == NULL) {
         wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
         qdf_err("ic (id: 0) is NULL");
         return -1;
    }

    nl_handle = ic->ic_nl_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(pdev);
    event = (wmi_host_mu_report_event *) qdf_mem_malloc_atomic(sizeof(wmi_host_mu_report_event));
    if (!event) {
	qdf_print("%s: Unable to allocate temporary copy of mu report event,"
			" Dropping mu report event", __func__);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
	return -1;
    }

    if( wmi_extract_mu_ev_param(pdev_wmi_handle, data, event)) {
        qdf_print("Unable to extract mu report");
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }

    if (!ic->ic_nl_handle) {
        printk("%s: MU report (status=%d id=%d), but on non NL radio\n",
               __func__, event->status_reason, event->mu_request_id);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return 0;
    }

    spin_lock_bh(&nl_handle->mu_lock);
    if (atomic_read(&nl_handle->mu_in_progress) == 0) {
        spin_unlock_bh(&nl_handle->mu_lock);
        printk("%s: MU report id=%d (%d), but MU not active\n",
               __func__, event->mu_request_id, nl_handle->mu_id);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return 0;
    }

    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_NL,
             "%s: MU report id=%d, expected=%d, status=%d\n", __func__,
             event->mu_request_id, nl_handle->mu_id, event->status_reason);

    mu_rpt.mu_channel = nl_handle->mu_channel;
    mu_rpt.mu_actual_duration = nl_handle->mu_duration;
    atomic_set(&nl_handle->mu_in_progress, 0);
    spin_unlock_bh(&nl_handle->mu_lock);

    mu_rpt.mu_req_id = event->mu_request_id;
    mu_rpt.mu_status = event->status_reason;
    if (event->status_reason) {
        printk("%s: MU report id=%d failed with status=%d\n", __func__, event->mu_request_id, event->status_reason);
    }
    for (i = 0; i < (MU_MAX_ALGO-1); i++) {
        mu_rpt.mu_total_val[i] = event->total_mu[i];
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_NL,
                 "%s: MU report val[%d]=%d\n", __func__, i, event->total_mu[i]);
    }
    mu_rpt.mu_num_bssid = event->num_active_bssid;
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_NL,
             "%s: MU report num_bssid=%d\n", __func__, event->num_active_bssid);

    for (i = 0; i < LTEU_MAX_BINS; i++) {
        mu_rpt.mu_hidden_node_algo[i] = event->hidden_node_mu[i];
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_NL,
                             "%s: MU hidden node for bin number[%d]=%d\n", __func__, i, event->hidden_node_mu[i]);
    }

    if (event->num_TA_entries > MU_DATABASE_MAX_LEN) {
        printk("%s: num_TA_entries value =%d, exceeds MAX limit of %d",
                __func__, event->num_TA_entries, MU_DATABASE_MAX_LEN);
        event->num_TA_entries = MU_DATABASE_MAX_LEN;
    }

    mu_rpt.mu_num_ta_entries = event->num_TA_entries;
    IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_NL,
                         "%s: MU report num_TA_entries=%d\n", __func__, event->num_TA_entries);

    for (i = 0; i < event->num_TA_entries; i++) {

        if (wmi_extract_mu_db_entry(pdev_wmi_handle, data, i, db_entry)) {
            qdf_print("Extracting MU db entry failed");
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
            return -1;
	}

        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_NL,
                            "%s : TA based MU entry number : %d\n", __func__, i);

        mu_rpt.mu_database_entries[i].mu_device_type = db_entry->entry_type;
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_NL,
                             "Device type=%d\n",db_entry->entry_type);

        WMI_HOST_MAC_ADDR_TO_CHAR_ARRAY(&db_entry->bssid_mac_addr,
                                   mu_rpt.mu_database_entries[i].mu_device_bssid);
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_NL,
                             "BSSID mac address=%08x%08x\n", db_entry->bssid_mac_addr.mac_addr47to32,
                             db_entry->bssid_mac_addr.mac_addr31to0);

        WMI_HOST_MAC_ADDR_TO_CHAR_ARRAY(&db_entry->tx_addr,
                                   mu_rpt.mu_database_entries[i].mu_device_macaddr);
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_NL,
                             "TA mac address=%08x%08x \n", db_entry->tx_addr.mac_addr47to32,
                             db_entry->tx_addr.mac_addr31to0);

        mu_rpt.mu_database_entries[i].mu_avg_duration = db_entry->avg_duration_us;
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_NL,
                             "Average duration=%d\n", db_entry->avg_duration_us);

        mu_rpt.mu_database_entries[i].mu_avg_rssi = db_entry->avg_rssi;
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_NL,
                             "Average RSSI=%d\n", db_entry->avg_rssi);

        mu_rpt.mu_database_entries[i].mu_percentage = db_entry->mu_percent;
        IEEE80211_DPRINTF_IC(ic, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_NL,
                             "MU in percentage=%d\n", db_entry->mu_percent);
    }
    IEEE80211_COMM_LOCK(ic);
    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        spin_lock_bh(&nl_handle->mu_lock);
        if (nl_handle->mu_cb && vap == nl_handle->mu_cb_arg) {
            mu_cb = nl_handle->mu_cb;
            mu_cb_arg = nl_handle->mu_cb_arg;
            spin_unlock_bh(&nl_handle->mu_lock);
            break;
        }
        spin_unlock_bh(&nl_handle->mu_lock);
    }
    IEEE80211_COMM_UNLOCK(ic);

    if (mu_cb && mu_cb_arg)
        mu_cb(mu_cb_arg, &mu_rpt);

    qdf_mem_free(event);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    return 0;
}

static int
ol_ath_mu_scan(struct ieee80211com *ic, u_int8_t id, u_int32_t duration, u_int8_t type,
               u_int32_t lteu_tx_power, u_int32_t thr_bssid, u_int32_t thr_sta, u_int32_t thr_sc,
               u_int32_t home_plmnid, u_int32_t alpha_active_bssid)
{
    struct mu_scan_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(ic->ic_pdev_obj);
    qdf_mem_set(&param, sizeof(param), 0);
    param.id = id;
    param.duration = duration;
    param.type = type;
    param.lteu_tx_power = lteu_tx_power;
    param.rssi_thr_bssid = thr_bssid;
    param.rssi_thr_sta = thr_sta;
    param.rssi_thr_sc = thr_sc;
    param.plmn_id = home_plmnid & 0xFFFFFF;
    param.alpha_num_bssid = alpha_active_bssid;
    return wmi_unified_mu_scan_cmd_send(pdev_wmi_handle, &param);
}

static int
ol_ath_lteu_config(struct ieee80211com *ic,
                   ieee80211req_lteu_cfg_t *config, u_int32_t wifi_tx_power)
{
    struct lteu_config_params param;
    int i = 0;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(ic->ic_pdev_obj);
    qdf_mem_set(&param, sizeof(param), 0);
    param.lteu_gpio_start = config->lteu_gpio_start;
    param.lteu_num_bins = config->lteu_num_bins;
    for (i = 0; i < config->lteu_num_bins; i++) {
        param.lteu_thresh[i] = config->lteu_thresh[i];
        param.lteu_weight[i] = config->lteu_weight[i];
        param.lteu_gamma[i] = config->lteu_gamma[i];
    }
    param.lteu_scan_timeout = config->lteu_scan_timeout;
    param.alpha_num_bssid = config->alpha_num_bssid;
    param.use_actual_nf = config->use_actual_nf;
    param.wifi_tx_power = wifi_tx_power;
    param.allow_err_packets = config->lteu_cfg_reserved_1;
    return wmi_unified_lteu_config_cmd_send(pdev_wmi_handle, &param);
}

void
ol_ath_nl_attach(struct ieee80211com *ic)
{
    struct ieee80211_nl_handle *nl_handle = NULL;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (wlan_psoc_nif_feat_cap_get(scn->soc->psoc_obj, WLAN_SOC_F_LTEU_SUPPORT)) {
        nl_handle = OS_MALLOC(ic->ic_osdev, sizeof(*nl_handle), GFP_KERNEL);
        if (nl_handle) {
            OS_MEMSET(nl_handle, sizeof(*nl_handle), 0);
            atomic_set(&nl_handle->mu_in_progress, 0);
            atomic_set(&nl_handle->scan_in_progress, 0);
            spin_lock_init(&nl_handle->mu_lock);
            nl_handle->mu_scan = ol_ath_mu_scan;
            nl_handle->lteu_config = ol_ath_lteu_config;
            nl_handle->force_vdev_restart = 0;
            nl_handle->use_gpio_start = 0;
            wmi_unified_register_event_handler((void *)pdev_wmi_handle,
                                wmi_mu_report_event_id,
                                ol_ath_mu_report_event_handler,
                                WMI_RX_UMAC_CTX);
        }
    }
    ic->ic_nl_handle = nl_handle;
}

void
ol_ath_nl_detach(struct ieee80211com *ic)
{
    struct ieee80211_nl_handle *nl_handle = ic->ic_nl_handle;
    struct common_wmi_handle *pdev_wmi_handle;


    pdev_wmi_handle = lmac_get_pdev_wmi_handle(ic->ic_pdev_obj);
    if (nl_handle) {
        wmi_unified_unregister_event_handler((void *)pdev_wmi_handle,
                              wmi_mu_report_event_id);
        spin_lock_destroy(&nl_handle->mu_lock);
        OS_FREE(nl_handle);
    }
    ic->ic_nl_handle = NULL;
}

#endif /* QCA_LTEU_SUPPORT */
