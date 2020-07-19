/*
 * Copyright (c) 2016-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */
#ifndef _IEEE80211_OBJMGR_PRIV_H
#define _IEEE80211_OBJMGR_PRIV_H


#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_pdev_mlme_api.h>
#include <wlan_vdev_mlme_api.h>
#include <wlan_mlme_dbg.h>
#include <qdf_atomic.h>

int wlan_mlme_init(void);
void wlan_mlme_deinit(void);

struct ieee80211_node *wlan_objmgr_alloc_sta_node(struct ieee80211vap *vap, uint8_t *macaddr);

struct ieee80211_node *wlan_objmgr_alloc_ap_node(struct ieee80211vap *vap, uint8_t *macaddr);

struct ieee80211_node *wlan_objmgr_alloc_ibss_node(struct ieee80211vap *vap, uint8_t *macaddr);

struct ieee80211_node *wlan_objmgr_alloc_tmp_node(struct ieee80211vap *vap, uint8_t *macaddr);

void wlan_objmgr_nt_soc_attach(struct ieee80211_node_table *nt);

struct ieee80211_node *wlan_objmgr_find_node(struct ieee80211_node_table *nt, uint8_t *macaddr,
                                             wlan_objmgr_ref_dbgid id);

qdf_list_t *wlan_objmgr_populate_logically_deleted_node_list
			(struct ieee80211_node_table *nt, uint8_t *macaddr, uint8_t *bssid,
                                             wlan_objmgr_ref_dbgid id);

struct ieee80211_node *wlan_objmgr_find_node_by_mac_n_bssid(
                                               struct ieee80211_node_table *nt,
                                               uint8_t *macaddr,
                                               uint8_t *bssid,
                                               wlan_objmgr_ref_dbgid id);

struct ieee80211com* wlan_vdev_get_ic(struct wlan_objmgr_vdev *vdev);

struct ieee80211vap* wlan_vdev_get_vap(struct wlan_objmgr_vdev *vdev);

void wlan_objmgr_delete_vap(struct ieee80211vap *vap);

static inline struct wlan_objmgr_peer *wlan_node_get_peer_obj(
                                                    struct ieee80211_node *node)
{
   return node->peer_obj;
}

static inline struct ieee80211_node *wlan_peer_get_mlme_ext_obj(
                                                  struct wlan_objmgr_peer *peer)
{
    void *ni;

    if(peer == NULL) {
        qdf_print("PEER is NULL ");
        return NULL;
    }
    ni = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_MLME);

    return (struct ieee80211_node *)ni;
}

static inline void *wlan_get_psoc_mlme_obj(struct wlan_objmgr_psoc *psoc)
{
   return wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_MLME);
}

static inline struct ieee80211com *wlan_pdev_get_mlme_ext_obj(
                                                 struct wlan_objmgr_pdev *pdev)
{
    struct ieee80211com *ic;

    if(pdev == NULL) {
         mlme_err("pdev is NULL \n");
         return NULL;
    }

    ic = wlan_pdev_mlme_get_ext_hdl(pdev);

    return ic;
}

static inline void
wlan_vdev_set_mlme_ext_obj(struct wlan_objmgr_vdev *vdev, void *ext_hdl)
{
    if(vdev == NULL) {
         qdf_err("vdev is NULL ");
         return;
    }

    if(ext_hdl == NULL) {
         qdf_err("ext_hdl is NULL ");
         return;
    }

    wlan_vdev_mlme_set_ext_hdl(vdev, ext_hdl);
}

static inline wlan_if_t wlan_vdev_get_mlme_ext_obj(
                                                 struct wlan_objmgr_vdev *vdev)
{
    void *vap;

    if(vdev == NULL) {
         qdf_print("vdev is NULL ");
         return NULL;
    }

    vap = wlan_vdev_mlme_get_ext_hdl(vdev);
    return (wlan_if_t)vap;
}

static inline int32_t wlan_objmgr_node_refcnt(struct ieee80211_node *ni)
{
    struct wlan_objmgr_peer *peer;
    peer = ni->peer_obj;
    if(peer == NULL) {
        qdf_print("PEER is NULL ");
        return -1;
    }
    return qdf_atomic_read(&peer->peer_objmgr.ref_cnt);
}

static inline uint32_t
wlan_objmgr_node_comp_ref_cnt(struct ieee80211_node *ni,
        enum wlan_umac_comp_id id)
{
    struct wlan_objmgr_peer *peer;
    peer = ni->peer_obj;
    if(peer == NULL) {
        qdf_print("%s: PEER is NULL ", __func__);
        return 0;
    }
    return wlan_objmgr_peer_get_comp_ref_cnt(peer, id);
}
#endif /* _IEEE80211_OBJMGR_PRIV_H */
