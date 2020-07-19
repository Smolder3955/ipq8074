/*
 * Copyright (c) 2017,2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#include <ieee80211_var.h>
#include "ieee80211_ioctl.h"
#include "ald_netlink.h"

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
#include "ieee80211_ald_priv.h"    /* Private to ALD module */
#include "ath_dev.h"
#include "if_athvar.h"
#include "ol_if_athvar.h"
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

#define IEEE80211_ALD_COMPUTE_UTILITY(value) ((value) * 100 / 256)

int ieee80211_ald_vattach(wlan_if_t vap)
{
    struct ieee80211com     *ic;
    struct ath_linkdiag     *ald_priv;
    int  ret = 0;

    ic  = vap->iv_ic;

    if (vap->iv_ald) {
        ASSERT(vap->iv_ald == 0);
        return -1; /* already attached ? */
    }

    ald_priv = (struct ath_linkdiag *) OS_MALLOC(ic->ic_osdev, (sizeof(struct ath_linkdiag)), GFP_ATOMIC);
    vap->iv_ald = ald_priv;

    if (ald_priv == NULL) {
       return -ENOMEM;
    } else {
        OS_MEMZERO(ald_priv, sizeof(struct ath_linkdiag));
        spin_lock_init(&ald_priv->ald_lock);
        vap->iv_ald->ald_phyerr = UINT_MAX;
        vap->iv_ald->ald_maxcu = WLAN_MAX_MEDIUM_UTILIZATION;
        return ret;
    }
}

int ieee80211_ald_vdetach(wlan_if_t vap)
{
    if(vap->iv_ald){
        spin_lock_destroy(&ald_priv->ald_lock);
        OS_FREE(vap->iv_ald);
    }
    return 0;
}

/**
 * @brief Calculate the PHY error rate (and update the link
 *        diagnostic stats)
 *
 * @param [in] ald  pointer to link diagnostic stats
 * @param [in] new_phyerr  current PHY error rate reported on
 *                         the radio
 */
void ieee80211_ald_update_phy_error_rate(struct ath_linkdiag *ald,
                                   u_int32_t new_phyerr)
{
    u_int32_t old_phyerr;
    u_int32_t old_ostime, new_ostime, time_diff = 0;
    u_int32_t phyerr_rate = 0, phyerr_diff;
    old_phyerr = ald->ald_phyerr;
    old_ostime = ald->ald_ostime;
    new_ostime = CONVERT_SYSTEM_TIME_TO_SEC(OS_GET_TIMESTAMP());

    /*
     * The PHY error counts (received from firmware) is a running count
     * (ie monotonically increasing until wrap-around).  Get the difference
     * since last time the PHY error has been read to calculate how many PHY
     * errors have occurred in the last sampling interval.
     */
    if (new_ostime > old_ostime) {
        time_diff = new_ostime - old_ostime;
    } else if (new_ostime < old_ostime) {
        time_diff = UINT_MAX - old_ostime + new_ostime;
    }
    if (old_phyerr == UINT_MAX) {
        /*
         * This is probably the first time sample - ignore
         */
        time_diff = 0;
    }

    if (time_diff) {
        if (new_phyerr >= old_phyerr) {
            phyerr_diff = new_phyerr - old_phyerr;
        } else {
            phyerr_diff = (UINT_MAX - old_phyerr) + new_phyerr;
        }
        phyerr_rate = phyerr_diff / time_diff;
    }

    ald->ald_phyerr = new_phyerr;
    ald->ald_ostime = new_ostime;
    ald->phyerr_rate = phyerr_rate;
}

void ieee80211_get_ald_statistics(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = NULL;
    struct ath_softc_net80211 *scn = NULL;
    struct ol_ath_softc_net80211 *ol_scn = NULL;
    struct ieee80211_node_table *nt = NULL;
    struct ieee80211_node *ni = NULL, *next=NULL;
    rwlock_state_t lock_state;
    int index = 0;
    struct ieee80211_chanutil_info *cu = NULL;
    ath_node_t an;
    bool is_offload = 0;
#ifdef QCA_SUPPORT_CP_STATS
    struct pdev_ic_cp_stats *pdev_cps = NULL;
#endif


    if(!vap)
        return;

    ic = vap->iv_ic;
    cu = &vap->chanutil_info;
    scn = ATH_SOFTC_NET80211(ic);
    nt = &ic->ic_sta;
    is_offload = ic->ic_is_mode_offload(ic);
#ifdef QCA_SUPPORT_CP_STATS
    pdev_cps = wlan_get_pdev_cp_stats_ref(ic->ic_pdev_obj);
    if (!pdev_cps) {
         qdf_print("Failed to get pdev cp stats reference");
         return;
    }
#endif


    if (!is_offload) {
        scn = ATH_SOFTC_NET80211(ic);
        scn->sc_ops->ald_collect_data(scn->sc_dev, vap->iv_ald);
    } else {
        ol_scn = OL_ATH_SOFTC_NET80211(ic);
        ic->ic_check_buffull_condition(ic);
#ifdef QCA_SUPPORT_CP_STATS
        ieee80211_ald_update_phy_error_rate(
            vap->iv_ald, pdev_cps->stats.cs_phy_err_count);
#endif
    }

    if (cu->new_value) {
        vap->iv_ald->staticp->utility = cu->value * 100 / 256;
        cu->new_value = 0;
    } else {
        // Not report stale utility value
        vap->iv_ald->staticp->utility = IEEE80211_ALD_STAT_UTILITY_UNCHANGED;
    }

    vap->iv_ald->staticp->load = vap->iv_ald->ald_dev_load;
    vap->iv_ald->staticp->txbuf = vap->iv_ald->ald_txbuf_used;
    vap->iv_ald->staticp->curThroughput = vap->iv_ald->ald_curThroughput;

    OS_RWLOCK_READ_LOCK(&nt->nt_nodelock, &lock_state);
    TAILQ_FOREACH_SAFE(ni, &nt->nt_node, ni_list, next) {
        /* ieee80211_sta_leave may be called or RWLOCK_WRITE_LOCK may be acquired */
        /* TBD: this is not multi-thread safe. Should use wlan_iterate_station_list */
        if(!IEEE80211_ADDR_EQ( (ni->ni_vap)->iv_myaddr , vap->iv_myaddr ))
            continue;

        if(IEEE80211_ADDR_EQ(vap->iv_myaddr, ni->ni_macaddr))
            continue;
        if (!ieee80211_try_ref_node(ni))
            continue;

        an = ATH_NODE_NET80211(ni)->an_sta;

        memcpy(vap->iv_ald->staticp->lkcapacity[index].da, ni->ni_macaddr, 6);
        if (!is_offload)
            scn->sc_ops->ald_collect_ni_data(scn->sc_dev, an, vap->iv_ald, &ni->ni_ald);
        else
            ic->ic_collect_stats(ni);
        vap->iv_ald->staticp->lkcapacity[index].capacity = ni->ni_ald.ald_capacity;
        vap->iv_ald->staticp->lkcapacity[index].aggr = ni->ni_ald.ald_aggr;
        vap->iv_ald->staticp->lkcapacity[index].aggrmax = ni->ni_ald.ald_avgmax4msaggr;
        vap->iv_ald->staticp->lkcapacity[index].phyerr = ni->ni_ald.ald_phyerr;
        vap->iv_ald->staticp->lkcapacity[index].lastper = ni->ni_ald.ald_lastper;
        vap->iv_ald->staticp->lkcapacity[index].msdusize = ni->ni_ald.ald_msdusize;
        vap->iv_ald->staticp->lkcapacity[index].retries = ni->ni_ald.ald_retries;
        OS_MEMCPY( vap->iv_ald->staticp->lkcapacity[index].nobufs, ni->ni_ald.ald_ac_nobufs, sizeof(ni->ni_ald.ald_ac_nobufs) );
        OS_MEMCPY( vap->iv_ald->staticp->lkcapacity[index].excretries, ni->ni_ald.ald_ac_excretries, sizeof(ni->ni_ald.ald_ac_excretries) );
        OS_MEMCPY( vap->iv_ald->staticp->lkcapacity[index].txpktcnt, ni->ni_ald.ald_ac_txpktcnt, sizeof(ni->ni_ald.ald_ac_txpktcnt) );

        if (is_offload)
            ic->ic_reset_ald_stats(ni);

        index++;
        ieee80211_free_node(ni);
        if(index >= MAX_NODES_NETWORK)
            goto staticout;
    }
staticout:
    OS_RWLOCK_READ_UNLOCK(&nt->nt_nodelock, &lock_state);
    vap->iv_ald->staticp->nientry = index;
    vap->iv_ald->staticp->vapstatus =
                 (wlan_vdev_allow_connect_n_tx(vap->vdev_obj) == QDF_STATUS_SUCCESS ? 1 : 0);
}

int wlan_ald_get_statistics(wlan_if_t vap, void *para)
{
    struct ieee80211com     *ic = vap->iv_ic;
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ald_stat_info *param = (struct ald_stat_info *)para;
    u_int32_t rxc_pcnt, rxf_pcnt, txf_pcnt;
    int retval = 0;

    if (para == NULL) {
        ieee80211_get_ald_statistics(vap);
        return retval;
    }

    ic  = vap->iv_ic;
    vap->iv_ald->staticp = param;
    switch (param->cmd)
    {
    case IEEE80211_ALD_MAXCU:
        if (scn->sc_ops->ath_get_mib_cycle_counts_pct(scn->sc_dev, &rxc_pcnt, &rxf_pcnt, &txf_pcnt)) {
            vap->iv_ald->ald_maxcu = 100 - rxc_pcnt;
        }
        else {
            retval = -1;
        }
        break;
    default:
        break;
    }
    ieee80211_get_ald_statistics(vap);
    return retval;
}

void ieee80211_ald_record_tx(struct ieee80211vap *vap, wbuf_t wbuf, int datalen)
{
    struct ieee80211_frame *wh;
    int type;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;

    if((type == IEEE80211_FC0_TYPE_DATA) &&
          (!IEEE80211_IS_MULTICAST(wh->i_addr1)) &&
          (!IEEE80211_IS_BROADCAST(wh->i_addr1))){
        int32_t tmp;
        tmp = vap->iv_ald->ald_unicast_tx_bytes;
        spin_lock(&vap->iv_ald->ald_lock);
        vap->iv_ald->ald_unicast_tx_bytes += datalen;
        if(tmp > vap->iv_ald->ald_unicast_tx_bytes){
            vap->iv_ald->ald_unicast_tx_bytes = datalen;
            vap->iv_ald->ald_unicast_tx_packets = 0;
        }
        vap->iv_ald->ald_unicast_tx_packets++;
        spin_unlock(&vap->iv_ald->ald_lock);
    }
}

/* Enable ald statistics for a station with give MAC address */
int wlan_ald_sta_enable(wlan_if_t vaphandle, u_int8_t *macaddr, u_int32_t enable)
{
    struct ieee80211vap *vap = (struct ieee80211vap *)vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni;
    int retval = 0;
    u_int32_t ni_ext_stats_enable;

    if ((ni = ieee80211_find_node(&ic->ic_sta, macaddr))) {
        enable = !!enable;
        ni_ext_stats_enable = (ni->ni_flags & IEEE80211_NODE_EXT_STATS) ? 1 : 0;
        if ((ni_ext_stats_enable != enable) &&
            ((retval = ic->ic_node_ext_stats_enable(ni, enable)) == 0)) {
            if(ni->ni_flags & IEEE80211_NODE_EXT_STATS) {
               ieee80211node_clear_flag(ni, IEEE80211_NODE_EXT_STATS);
            }
            else {
               ieee80211node_set_flag(ni, IEEE80211_NODE_EXT_STATS);
            }
        }
        ieee80211_free_node(ni);
    } else {
        retval = -EINVAL;
    }

    return retval;
}

#endif


