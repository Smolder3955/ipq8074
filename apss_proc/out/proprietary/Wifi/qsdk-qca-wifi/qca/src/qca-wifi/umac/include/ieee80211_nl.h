/*
 * Copyright (c) 2015,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#ifndef _IEEE80211_NL_H
#define _IEEE80211_NL_H

#if QCA_LTEU_SUPPORT

#include <ieee80211_ioctl.h>

struct ieee80211_nl_handle {
    int                                 (*mu_scan)(struct ieee80211com *ic, u_int8_t id, u_int32_t duration, u_int8_t type, u_int32_t lteu_tx_power,
                                                   u_int32_t rssi_thr_bssid, u_int32_t rssi_thr_sta, u_int32_t rssi_thr_sc,
                                                   u_int32_t home_plmnid, u_int32_t alpha_active_bssid);
    int                                 (*lteu_config)(struct ieee80211com *ic, ieee80211req_lteu_cfg_t *config, u_int32_t wifi_tx_power);
    atomic_t                            mu_in_progress;
    atomic_t                            scan_in_progress;
    u_int8_t                            mu_id;
    u_int8_t                            scan_id;
    u_int8_t                            mu_channel;
    u_int16_t                           mu_duration;
    spinlock_t                          mu_lock;
    void                                (*mu_cb)(wlan_if_t cb_arg, struct event_data_mu_rpt *mu_rpt);
    wlan_if_t                           mu_cb_arg;
    wlan_scan_id                        scanid;
    wlan_scan_requester                 scanreq;
    int                                 force_vdev_restart;
    int                                 use_gpio_start;
};

int
ieee80211_init_mu_scan(struct ieee80211com *ic, ieee80211req_mu_scan_t *reqptr);
int
ieee80211_mu_scan(struct ieee80211com *ic, ieee80211req_mu_scan_t *reqptr);
void
ieee80211_mu_scan_fail(struct ieee80211com *ic);
int
ieee80211_lteu_config(struct ieee80211com *ic,
                      ieee80211req_lteu_cfg_t *reqptr, u_int32_t wifi_tx_power);
void
ieee80211_nl_register_handler(struct ieee80211vap *vap,
             void (*mu_handler)(wlan_if_t, struct event_data_mu_rpt *),
             scan_event_handler scan_handler);
void
ieee80211_nl_unregister_handler(struct ieee80211vap *vap,
             void (*mu_handler)(wlan_if_t, struct event_data_mu_rpt *),
             scan_event_handler scan_handler);
int
ieee80211_ap_scan(struct ieee80211com *ic,
                  struct ieee80211vap *vap, ieee80211req_ap_scan_t *reqptr);

#define IEEE80211_MSG_NL             IEEE80211_MSG_AUTH
#define LTEU_DEFAULT_PRB_SPC_INT     2

#endif /* QCA_LTEU_SUPPORT */

#endif /* _IEEE80211_NL_H */

