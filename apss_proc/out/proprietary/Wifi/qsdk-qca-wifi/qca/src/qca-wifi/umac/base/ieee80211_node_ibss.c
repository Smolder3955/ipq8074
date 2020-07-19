/*
 * Copyright (c) 2011-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 * 
 */


#include "ieee80211_node_priv.h"
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_util_api.h>
#endif

#if UMAC_SUPPORT_IBSS
void
ieee80211_adjustcoext(struct ieee80211vap *vap);

/*
 * Test a node for suitability/compatibility.
 */
static int
check_bss(struct ieee80211vap *vap, ieee80211_scan_entry_t  scan_entry)
{
    struct ieee80211com *ic = vap->iv_ic;
    u_int8_t ssid_len, *ssid;
    u_int16_t capinfo = ieee80211_scan_entry_capinfo(scan_entry);
    struct ieee80211_ath_channel *chan = ieee80211_scan_entry_channel(scan_entry);

    if (isclr(ic->ic_chan_active, ieee80211_chan2ieee(ic,chan )))
        return 0;

    if ((capinfo & IEEE80211_CAPINFO_IBSS) == 0)
        return 0;
    if (IEEE80211_IS_CHAN_DISALLOW_ADHOC(chan)) 
        return 0;

    ssid = ieee80211_scan_entry_ssid(scan_entry, &ssid_len);
    if (!ssid || !ieee80211_vap_match_ssid(vap, ssid, ssid_len))
        return 0;

    /* privacy */
    if (((capinfo & IEEE80211_CAPINFO_PRIVACY) && !IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) ||
        (!(capinfo & IEEE80211_CAPINFO_PRIVACY) && IEEE80211_VAP_IS_PRIVACY_ENABLED(vap))) {
        /* privacy mismatch */
        return 0;
    }
        
    return 1;
}

/*
 * Adjust coexist HT additional info based on mode
 */
void
ieee80211_adjustcoext(struct ieee80211vap *vap)
{
    switch (vap->iv_cur_mode) {
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NG_HT40PLUS:
            vap->iv_chextoffset = 2;
            vap->iv_chwidth     = IEEE80211_CWM_WIDTH40;
        break;
        
        case IEEE80211_MODE_11NA_HT40MINUS:
        case IEEE80211_MODE_11NG_HT40MINUS:
            vap->iv_chextoffset = 3;
            vap->iv_chwidth     = IEEE80211_CWM_WIDTH40;
        break;

        default:
            vap->iv_chextoffset = 1;
            vap->iv_chwidth     = IEEE80211_CWM_WIDTH20;
        break;
    }
}

/*
 * Duplicate and join a IBSS node
 */
int
ieee80211_dup_ibss(struct ieee80211vap *vap, struct ieee80211_node *org_ibss)
{
    struct ieee80211_node *ni;

     /* Allocate BSS peer object */
    ni = wlan_objmgr_alloc_ibss_node(vap, vap->iv_myaddr);
    if (ni == NULL)
        return -ENOMEM;

    /* Save our home channel */
    vap->iv_bsschan = org_ibss->ni_chan;
    /* set default rate and channel */
    ieee80211_node_set_chan(ni);
    vap->iv_cur_mode = ieee80211_chan2mode(ni->ni_chan);

    IEEE80211_ADDR_COPY(ni->ni_bssid, org_ibss->ni_bssid);
    ni->ni_intval = org_ibss->ni_intval;
    ni->ni_capinfo = org_ibss->ni_capinfo;
    ni->ni_erp = org_ibss->ni_erp;
    ni->ni_esslen = org_ibss->ni_esslen;
    OS_MEMCPY(ni->ni_essid, org_ibss->ni_essid, ni->ni_esslen);
    OS_MEMCPY(ni->ni_tstamp.data, org_ibss->ni_tstamp.data, sizeof(ni->ni_tstamp));

    if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
        ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;
        ni->ni_rsn = org_ibss->ni_rsn; /* use the peer RSN settings */
    }

    /* Now we can leave the original ibss and join the new one */
    ieee80211_sta_leave(org_ibss);
    return ieee80211_sta_join_bss(ni);
}

/*
 * Create a IBSS node based on info in the "peer" node.
 */
int
ieee80211_join_ibss(struct ieee80211vap *vap, ieee80211_scan_entry_t scan_entry)

{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni;
    u_int8_t *ssid;
    int error = 0;

    ASSERT(vap->iv_opmode == IEEE80211_M_IBSS);
    
    KASSERT( ((ieee80211_scan_entry_capinfo(scan_entry) & IEEE80211_CAPINFO_IBSS) != 0),
        ("Can not join non IBSS networks"));
    ni = wlan_objmgr_alloc_ibss_node(vap, vap->iv_myaddr);
    if (ni == NULL)
        return -ENOMEM;

    vap->iv_bsschan = ieee80211_scan_entry_channel(scan_entry);
    if (IEEE80211_IS_CHAN_11N_HT40(vap->iv_bsschan) && !ieee80211_ic_ht40Adhoc_is_set(ic)) {
        if (IEEE80211_IS_CHAN_5GHZ(vap->iv_bsschan)) {
            vap->iv_bsschan = ieee80211_find_dot11_channel(ic, vap->iv_bsschan->ic_ieee, 0, IEEE80211_MODE_11NA_HT20);
        } else {
            vap->iv_bsschan = ieee80211_find_dot11_channel(ic, vap->iv_bsschan->ic_ieee, 0, IEEE80211_MODE_11NG_HT20);
        }
    }

    /* set default rate and channel */
    ieee80211_node_set_chan(ni);
    
#if ATH_SUPPORT_IBSS_HT
    /* 
     * update vap default rate and channel
     *  In case HT40 device connects to another HT20 device, 
     *  vap¡¦s rate and channel information is not correct, 
     *  so vap¡¦s rate and channel need to be updated here.
     */
    ieee80211_node_set_chan(vap->iv_bss);
#endif

    vap->iv_cur_mode = ieee80211_chan2mode(ni->ni_chan);
    ieee80211_adjustcoext(vap);

    IEEE80211_ADDR_COPY(ni->ni_bssid, ieee80211_scan_entry_bssid(scan_entry));
    ni->ni_intval = ieee80211_scan_entry_beacon_interval(scan_entry);
    ni->ni_capinfo = ieee80211_scan_entry_capinfo(scan_entry);
    ni->ni_erp = ieee80211_scan_entry_erpinfo(scan_entry);
    ssid = ieee80211_scan_entry_ssid(scan_entry, &ni->ni_esslen);
    if (ssid != NULL && (ni->ni_esslen < IEEE80211_NWID_LEN+1))
        OS_MEMCPY(ni->ni_essid, ssid, ni->ni_esslen);
    
   OS_MEMCPY(ni->ni_tstamp.data, ieee80211_scan_entry_tsf(scan_entry), sizeof(ni->ni_tstamp));

    ieee80211_setup_node(ni, scan_entry);
    ni->ni_chan = vap->iv_bsschan;
    if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
        ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;
        if ((error = ieee80211_setup_node_rsn(ni, scan_entry)) != 0)
            return error;
    }
#if ATH_SUPPORT_IBSS_DFS
    vap->iv_ibssdfs_state = IEEE80211_IBSSDFS_JOINER;
    IEEE80211_ADDR_COPY(vap->iv_ibssdfs_ie_data.owner, ieee80211_scan_entry_macaddr(scan_entry));
    vap->iv_ibssdfs_ie_data.rec_interval = INIT_IBSS_DFS_OWNER_RECOVERY_TIME_IN_TBTT;
    vap->iv_ibssdfs_ie_data.len = IBSS_DFS_ZERO_MAP_SIZE;
    vap->iv_ibssdfs_ie_data.ie = IEEE80211_ELEMID_IBSSDFS;

    vap->iv_channelswitch_ie_data.ie = IEEE80211_ELEMID_CHANSWITCHANN;
    vap->iv_channelswitch_ie_data.len = (IEEE80211_CHANSWITCHANN_BYTES - 2); /* fixed length */
    vap->iv_channelswitch_ie_data.switchmode = IEEE80211_CHANSWITCHANN_MODE_QUIET; /* stas get off for now */
#endif /* ATH_SUPPORT_IBSS_DFS */
    return ieee80211_sta_join_bss(ni);
}

/*
 * Create a IBSS node on current channel based on info passed in bssid and ssid.
 */
int
ieee80211_create_ibss(struct ieee80211vap *vap,
                      const u_int8_t *bssid,
                      const u_int8_t *essid,
                      const u_int16_t esslen)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni;

    ni = wlan_objmgr_alloc_ibss_node(vap, vap->iv_myaddr);
    if (ni == NULL)
        return -ENOMEM;

    IEEE80211_ADDR_COPY(ni->ni_bssid, bssid);
    ni->ni_esslen = esslen;
    OS_MEMCPY(ni->ni_essid, essid, ni->ni_esslen);

    if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap)) {
        ni->ni_capinfo |= IEEE80211_CAPINFO_PRIVACY;
        ni->ni_rsn = vap->iv_rsn; /* use the local RSN settings as the IBSS setting */
    }
#if ATH_SUPPORT_IBSS_DFS
    if (ieee80211_ic_doth_is_set(ic) && ieee80211_vap_doth_is_set(vap))
    {
        ni->ni_capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;
    } 
    vap->iv_ibssdfs_state = IEEE80211_IBSSDFS_OWNER;
    IEEE80211_ADDR_COPY(vap->iv_ibssdfs_ie_data.owner, ni->ni_macaddr);
    vap->iv_ibssdfs_ie_data.rec_interval = vap->iv_ibss_dfs_enter_recovery_threshold_in_tbtt;
    vap->iv_ibssdfs_ie_data.len = IBSS_DFS_ZERO_MAP_SIZE;
    vap->iv_ibssdfs_ie_data.ie = IEEE80211_ELEMID_IBSSDFS;
    vap->iv_ibssdfs_recovery_count = vap->iv_ibssdfs_ie_data.rec_interval;
    vap->iv_measrep_action_count_per_tbtt = 0;
    vap->iv_csa_action_count_per_tbtt = 0;

    /* dfs owner change but channel change is not required , 
       used in case when dfs owner is gone and joiner has to take charge */
    vap->iv_ibss_dfs_no_channel_switch = false; 

    vap->iv_channelswitch_ie_data.ie = IEEE80211_ELEMID_CHANSWITCHANN;
    vap->iv_channelswitch_ie_data.len = (IEEE80211_CHANSWITCHANN_BYTES - 2); /* fixed length = ie_size - ie_header */
    vap->iv_channelswitch_ie_data.switchmode = IEEE80211_CHANSWITCHANN_MODE_QUIET; /* stas get off for now */
#endif /* ATH_SUPPORT_IBSS_DFS */
    return ieee80211_sta_join_bss(ni);
}

/*
 * Handle 802.11 ad hoc network merge.  The
 * convention, set by the Wireless Ethernet Compatibility Alliance
 * (WECA), is that an 802.11 station will change its BSSID to match
 * the "oldest" 802.11 ad hoc network, on the same channel, that
 * has the station's desired SSID.  The "oldest" 802.11 network
 * sends beacons with the greatest TSF timestamp.
 *
 * The caller is assumed to validate TSF's before attempting a merge.
 *
 * Return !0 if the BSSID changed, 0 otherwise.
 */
int
ieee80211_ibss_merge(struct ieee80211_node *ni, ieee80211_scan_entry_t  scan_entry )
{
    struct ieee80211vap *vap = ni->ni_vap;

    if (ni == ni->ni_bss_node) {
        /* If it is the first beacon from the node */
        if (!check_bss(vap, scan_entry)) {
            /* capabilities/essid mismatch */
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
                              "%s: merge failed, capabilities/ssid mismatch\n", __func__);
#ifdef QCA_SUPPORT_CP_STATS
            vdev_cp_stats_ibss_capmismatch_inc(vap->vdev_obj, 1);
#endif
            return 0;
        }
    } else {
        if (IEEE80211_ADDR_EQ(ni->ni_bssid, vap->iv_bss->ni_bssid)) {
            /* We need to sync to the oldest beacon even if bssid remains the same. */
            ieee80211_vap_start(vap);
            return 0;
        }
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC,
                      "ibss merge %s caps=%08X\n", 
                      ether_sprintf(ni->ni_macaddr),
                      ni->ni_capinfo);

    /* Prepare for adhoc merge */
    ieee80211_mlme_adhoc_merge_prepare(vap);


    if (ieee80211_join_ibss(vap, scan_entry)) {
        printk("%s: failed to merge with bssid: %s\n",
               __func__, ether_sprintf(ni->ni_bssid));
        return 0;
    }

    /* let the H/W sync up */
    ieee80211_vap_start(vap);
    return 1;
}

/*
 * Do node discovery in adhoc mode on receipt of a beacon
 * or probe response frame. In ad-hoc mode, a node in node table
 * doesn't have to be a station in this IBSS network. Each node
 * use ni_assoc_state to mark its association state.
 * There is a extra node ref count in this function.
 * User needs to decrement the ref count after calling this function.
 */
struct ieee80211_node *
ieee80211_add_neighbor(struct ieee80211_node *ni,
                       ieee80211_scan_entry_t scan_entry)
{
    struct ieee80211vap *vap = ni->ni_vap;
    bool   first_beacon = FALSE;

    if (ni == vap->iv_bss) {
        /* It is the first time we receive a beacon from this station,
         * create a new node for the station.*/
        ni = wlan_objmgr_alloc_ibss_node(vap,
                                  ieee80211_scan_entry_macaddr(scan_entry));
        if (ni == NULL)
            return NULL;

        ni->ni_bss_node = ieee80211_try_ref_bss_node(vap);
        if (!ni->ni_bss_node) {
            wlan_objmgr_delete_node(ni);
            return NULL;
        }
        IEEE80211_ADD_NODE_TARGET(ni, vap, 0);
        first_beacon = TRUE;
    } else {
       ni = ieee80211_try_ref_node(ni);
       if (!ni) {
           wlan_objmgr_delete_node(ni);
           return NULL;
       }
    }
    /* Update information */

    if (first_beacon == TRUE) {
        if (ieee80211_setup_node(ni, scan_entry) != 0) {
            ieee80211_sta_leave(ni);
            ieee80211_free_node(ni);
            return NULL;
        }

        /* Authorize node to allow traffic to pass */
        /*
         * Win7 TBD: Do we really need to authorize the node? 
         * It causes BSOD when beacon is received after
         * node_reclaim is called, so we only call authorize
         * when node is initially allocated.
         */
        if (ni->ni_table)
            ieee80211_node_authorize(ni);
    }

    return ni;
}

#endif
