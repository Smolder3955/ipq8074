/*
 * Copyright (c) 2016,2017,2019 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _IEEE80211_OBJMGR_H
#define _IEEE80211_OBJMGR_H

#include <wlan_objmgr_cmn.h>
#include <ieee80211_defines.h>
#include <wlan_objmgr_peer_obj.h>

#define WLAN_VDEV_ID_MAP_DWORDS     2
#define WLAN_VDEV_PDEV_MAP_MAX     64

struct wlan_mlme_psoc_context {
    volatile unsigned long pending_vdev_del_resp_map[WLAN_VDEV_ID_MAP_DWORDS];
    volatile uint8_t vdev_pdev_map[WLAN_VDEV_PDEV_MAP_MAX];
};

void wlan_ic_psoc_set_flag(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_psoc_clear_flag(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_psoc_set_extflag(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_psoc_clear_extflag(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_psoc_set_ext2flag(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_psoc_clear_ext2flag(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_pdev_set_flag(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_pdev_clear_flag(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_pdev_set_extflag(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_pdev_clear_extflag(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_pdev_set_ext2flag(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_pdev_clear_ext2flag(struct ieee80211com *ic, uint32_t flag);
void wlan_vap_vdev_set_flag(struct ieee80211vap *vap, uint32_t flag);
void wlan_vap_vdev_clear_flag(struct ieee80211vap *vap, uint32_t flag);
void wlan_vap_vdev_set_extflag(struct ieee80211vap *vap, uint32_t flag);
void wlan_vap_vdev_clear_extflag(struct ieee80211vap *vap, uint32_t flag);
void wlan_vap_vdev_set_ext2flag(struct ieee80211vap *vap, uint32_t flag);
void wlan_vap_vdev_clear_ext2flag(struct ieee80211vap *vap, uint32_t flag);
void wlan_ic_psoc_set_cap(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_psoc_clear_cap(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_psoc_set_extcap(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_psoc_clear_extcap(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_pdev_set_cap(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_pdev_clear_cap(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_pdev_set_extcap(struct ieee80211com *ic, uint32_t flag);
void wlan_ic_pdev_clear_extcap(struct ieee80211com *ic, uint32_t flag);
void wlan_vap_vdev_set_cap(struct ieee80211vap *vap, uint32_t flag);
void wlan_vap_vdev_clear_cap(struct ieee80211vap *vap, uint32_t flag);
void wlan_vap_vdev_set_extflag(struct ieee80211vap *vap, uint32_t flag);
void wlan_vap_vdev_clear_extflag(struct ieee80211vap *vap, uint32_t flag);
void wlan_vap_vdev_set_ext22flag(struct ieee80211vap *vap, uint32_t flag);
void wlan_vap_vdev_clear_ext22flag(struct ieee80211vap *vap, uint32_t flag);
void wlan_vap_vdev_set_ext222flag(struct ieee80211vap *vap, uint32_t flag);
void wlan_vap_vdev_clear_ext222flag(struct ieee80211vap *vap, uint32_t flag);
void wlan_node_peer_set_flag(struct ieee80211_node *ni, u_int32_t flag);
void wlan_node_peer_clear_flag(struct ieee80211_node *ni, u_int32_t flag);
void wlan_node_peer_set_extflag(struct ieee80211_node *ni, u_int32_t flag);
void wlan_node_peer_clear_extflag(struct ieee80211_node *ni, u_int32_t flag);

void mlme_set_ops_cb(void);
int wlan_umac_init(void);
void wlan_umac_deinit(void);

struct ieee80211_node *wlan_wbuf_get_peer_node(wbuf_t wbuf);

void wlan_wbuf_set_peer_node(wbuf_t wbuf, struct ieee80211_node *ni);
void wlan_wbuf_set_peer(wbuf_t wbuf, struct wlan_objmgr_peer *peer);

void wlan_objmgr_free_node(struct ieee80211_node *ni, wlan_objmgr_ref_dbgid id);
void wlan_objmgr_delete_node(struct ieee80211_node *ni);
void wlan_objmgr_ref_node(struct ieee80211_node *node, wlan_objmgr_ref_dbgid id);
QDF_STATUS wlan_objmgr_try_ref_node(struct ieee80211_node *node,
                                    wlan_objmgr_ref_dbgid id);
void wlan_objmgr_unref_node(struct ieee80211_node *ni, wlan_objmgr_ref_dbgid id);
void wlan_node_set_peer_state(struct ieee80211_node *ni,
                                    enum wlan_peer_state state);
enum wlan_peer_state wlan_node_get_peer_state(struct ieee80211_node *ni);

enum QDF_OPMODE ieee80211_opmode2qdf_opmode(enum ieee80211_opmode opmode);

struct wlan_objmgr_pdev* wlan_vap_get_pdev(wlan_if_t vap);
struct wlan_objmgr_psoc* wlan_vap_get_psoc(wlan_if_t vap);

void wlan_objmgr_print_ref_cnts(struct ieee80211com *ic);

void wlan_objmgr_update_txchainmask_to_allvdevs(wlan_dev_t ic);
void wlan_objmgr_update_rxchainmask_to_allvdevs(wlan_dev_t ic);

#endif
