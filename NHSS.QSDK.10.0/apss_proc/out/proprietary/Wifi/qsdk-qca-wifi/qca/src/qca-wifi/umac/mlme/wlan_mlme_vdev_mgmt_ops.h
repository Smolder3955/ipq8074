/*
 * Copyright (c) 2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#ifndef __WLAN_MLME_VDEV_MGMT_OPS_H__
#define __WLAN_MLME_VDEV_MGMT_OPS_H__

#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_mlme.h>
#include <ieee80211_target.h>
#include <ieee80211_rateset.h>
#include <ieee80211_wds.h>
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_vdev_if.h>
#endif
#include <wlan_mlme_dp_dispatcher.h>
#include <wlan_vdev_mgr_tgt_if_rx_defs.h>
#include <wlan_cmn.h>

enum ieee80211_opmode ieee80211_new_opmode(struct ieee80211vap *vap, bool vap_active);

struct ieee80211vap
*mlme_ext_vap_create(struct ieee80211com *ic,
                     int                 opmode,
                     int                 scan_priority_base,
                     int                 flags,
                     const u_int8_t      bssid[IEEE80211_ADDR_LEN],
                     const u_int8_t      mataddr[IEEE80211_ADDR_LEN],
                     void                *vdev_handle);

QDF_STATUS mlme_ext_vap_delete_wait(struct ieee80211vap *vap);

QDF_STATUS mlme_ext_vap_delete(struct ieee80211vap *vap);

QDF_STATUS mlme_ext_vap_recover(struct ieee80211vap *vap);

int mlme_ext_vap_stop(struct ieee80211vap *vap);

QDF_STATUS mlme_ext_vap_up(struct ieee80211vap *, bool);

void mlme_ext_vap_beacon_stop(struct ieee80211vap *vap);

void mlme_ext_vap_defer_beacon_buf_free(struct ieee80211vap *vap);

QDF_STATUS mlme_ext_vap_down(struct ieee80211vap *vap);

int
mlme_ext_vap_start_response_event_handler(struct vdev_start_response *rsp,
                                          struct vdev_mlme_obj *vdev_mlme);

int mlme_ext_vap_start(struct ieee80211vap *vap,
                       struct ieee80211_ath_channel *chan,
                       u_int16_t reqid,
                       u_int16_t max_start_time,
                       u_int8_t restart);

void mlme_ext_vap_flush_bss_peer_tids(struct ieee80211vap *vap);

QDF_STATUS mlme_ext_multi_vdev_restart(
                                    struct ieee80211com *ic,
                                    uint32_t *vdev_ids, uint32_t num_vdevs);

int mlme_ext_update_channel_param(struct mlme_channel_param *ch_param,
                                  struct ieee80211com *ic);

void mlme_ext_update_multi_vdev_restart_param(struct ieee80211com *ic,
                                              uint32_t *vdev_ids,
                                              uint32_t num_vdevs,
                                              bool reset,
                                              bool restart_success);
QDF_STATUS mlme_ext_vap_custom_aggr_size_send(
                                        struct vdev_mlme_obj *vdev_mlme,
                                        bool is_amsdu);

#endif /* __WLAN_MLME_VDEV_MGMT_OPS_H__ */
