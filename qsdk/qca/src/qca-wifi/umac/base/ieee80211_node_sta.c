/*
 *
 * Copyright (c) 2011-2016,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 */

#include "ieee80211_node_priv.h"
#if WLAN_SUPPORT_GREEN_AP
#include <wlan_green_ap_api.h>
#endif

/* should this be configurable ?*/
#define IEE80211_STA_MAX_NODE_SAVEQ_LEN 300
/*
 * Join an infrastructure network
 */
int
ieee80211_sta_join(struct ieee80211vap *vap, ieee80211_scan_entry_t scan_entry,
        bool *thread_started)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni = NULL;
    const u_int8_t *macaddr = util_scan_entry_macaddr(scan_entry);
    struct ieee80211vap *tempvap = NULL;
    int error = 0;


    ASSERT(vap->iv_opmode == IEEE80211_M_STA);
    /*If AP mac to which our sta vap is trying to connect has
    same mac as one of our ap vaps ,dont set that as sta bssid */
    TAILQ_FOREACH(tempvap, &ic->ic_vaps, iv_next) {
        if (tempvap->iv_opmode == IEEE80211_M_HOSTAP && IEEE80211_ADDR_EQ(tempvap->iv_myaddr,macaddr)) {
             qdf_print("[%s] Mac collision for [%s]",__func__,ether_sprintf(tempvap->iv_myaddr));
             return -EINVAL;
        }
    }

    ni = ieee80211_vap_find_node(vap, macaddr);
    if (ni) {
        /*
         * reusing old node has a potential for several bugs . The old node may have some state info from previous association.
         * get rid of the old bss node and create a new bss node.
         */
        ieee80211_sta_leave(ni);
        ieee80211_free_node(ni);
    }
    /*
     * Create a BSS node.
     */
    ni = wlan_objmgr_alloc_ap_node(vap, (uint8_t *)macaddr);
    if (!ni) {
        if (wlan_vdev_get_peer_count(vap->vdev_obj) >=
                wlan_vdev_get_max_peer_count(vap->vdev_obj)) {
            ieee80211_vap_peer_create_failed_set(vap);
        }
        qdf_err("Unable to allocate AP peer, peer cnt:%d, max_peer:%d vapid:%d",
                wlan_vdev_get_peer_count(vap->vdev_obj),
                wlan_vdev_get_max_peer_count(vap->vdev_obj),
                vap->iv_unit);
        return -ENOMEM;
    }

    /* set the maximum number frmaes to be queued when the vap is in fake sleep */
    ieee80211_node_saveq_set_param(ni,IEEE80211_NODE_SAVEQ_DATA_Q_LEN,IEE80211_STA_MAX_NODE_SAVEQ_LEN);
    /* To become a bss node, a node need an extra reference count, which alloc node already gives */

    wlan_node_set_peer_state(ni, WLAN_JOIN_STATE);
    /* setup the bss node for association */
    error = ieee80211_setup_node(ni, scan_entry);
    if (error != 0) {
        wlan_objmgr_delete_node(ni);
        return error;
    }

    /* copy the beacon timestamp */
    OS_MEMCPY(ni->ni_tstamp.data,
              util_scan_entry_tsf(scan_entry),
              sizeof(ni->ni_tstamp));

    /*
     * Join the BSS represented by this new node.
     * This function will free up the old BSS node
     * and use this one as the new BSS node.
     */
    ieee80211_sta_join_bss(ni);

    IEEE80211_ADD_NODE_TARGET(ni, ni->ni_vap, 0);

    /* Save our home channel */
    vap->iv_bsschan = ni->ni_chan;
    vap->iv_cur_mode = ieee80211_chan2mode(ni->ni_chan);

    /* Update the DotH falg */
    ieee80211_update_spectrumrequirement(vap, thread_started);

    /*
     *  The OS will control our security keys.
     *  If clear, keys will be cleared.
     *  If static WEP, keys will be plumbed before JoinInfra.
     *  If WPA/WPA2, ciphers will be setup, but no keys will be plumbed until
     *    after they are negotiated.
     *  XXX We should ASSERT that all of the foregoing is true.
     */
    return 0;
}

