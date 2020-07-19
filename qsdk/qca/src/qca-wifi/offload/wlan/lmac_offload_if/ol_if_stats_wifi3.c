/*
 * Copyright (c) 2015, 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2011, Atheros Communications Inc.
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

/*
 * LMAC offload interface functions for UMAC - for power and performance offload model
 */
#if WLAN_SPECTRAL_ENABLE
#include "spectral.h"
#endif
#include "ol_if_athvar.h"
#include "ol_if_athpriv.h"
#include "init_deinit_lmac.h"
#include "ol_ath.h"
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include "qdf_lock.h"  /* qdf_spinlock_* */
#include "qdf_types.h" /* qdf_vprint */
#include "dbglog_host.h"
#include "a_debug.h"
#include <wdi_event_api.h>
#include <net.h>
#include <pktlog_ac_api.h>
#include <pktlog_ac_fmt.h>
#include <pktlog_ac_i.h>
#include "ol_if_stats_api.h"
#include "htt.h"
#include "ol_if_stats.h"
#include "osif_private.h"
#if QCA_AIRTIME_FAIRNESS
#include <target_if_atf.h>
#endif
#include "cepci.h"
#include "ath_pci.h"
#include "cdp_txrx_ctrl.h"
#include "enet.h"
#include <wlan_son_pub.h>
#if WLAN_SPECTRAL_ENABLE
#include <target_if_spectral.h>
#endif

#if ATH_PERF_PWR_OFFLOAD
#define GET_NEW_PER(nSucc, nTotal) ((((nTotal) - (nSucc)) * 100) / (nTotal))

uint32_t ol_if_getrateindex(uint16_t mcs, uint8_t nss, uint8_t preamble, uint8_t bw);

A_STATUS
ol_ath_update_dp_peer_stats(void *pdev, void *stats, uint16_t peer_id)
{
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct ieee80211_node *ni;
    struct cdp_peer_stats *peer_stats = (struct cdp_peer_stats *)stats;
    uint16_t vdev_id = CDP_INVALID_VDEV_ID;
    uint8_t peer_mac[6];
    struct ol_ath_softc_net80211 *scn;
    struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *)pdev;

    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_OSIF_ID) != QDF_STATUS_SUCCESS)
            return A_ERROR;

    scn = (struct ol_ath_softc_net80211 *)lmac_get_pdev_feature_ptr(pdev_obj);
    if (!scn) {
        qdf_warn("scn is NULL for pdev:%pK", pdev_obj);
        goto stats_done;
    }

    if (peer_id == HTT_INVALID_PEER)
            goto stats_done;

    vdev_id = cdp_get_peer_mac_addr_frm_id(wlan_psoc_get_dp_handle(scn->soc->psoc_obj),
                                peer_id, peer_mac);
    if(vdev_id == CDP_INVALID_VDEV_ID)
            goto stats_done;

    vap = ol_ath_vap_get(scn, vdev_id);

    if (!vap) {
            goto stats_done;
    }
    ni = ieee80211_vap_find_node(vap, peer_mac);
    ol_ath_release_vap(vap);
    if (!ni) {
            goto stats_done;
    }

    ic = vap->iv_ic;

    if (peer_stats->rx.avg_rssi != INVALID_RSSI) {
        ni->ni_rssi = peer_stats->rx.avg_rssi;
    }
    peer_stats->rx.rx_rssi_measured_time = OS_GET_TIMESTAMP();

    if (ic->ic_min_rssi_enable) {
        if (ni != ni->ni_bss_node && vap->iv_opmode == IEEE80211_M_HOSTAP) {
            /* compare the user provided rssi with peer rssi received */
            if (ni->ni_associd && ni->ni_rssi && (ic->ic_min_rssi > ni->ni_rssi)) {
                /* send de-auth to ni_macaddr */
                qdf_print("Client %s(snr = %u) de-authed due to insufficient SNR",
                        ether_sprintf(ni->ni_macaddr), ni->ni_rssi);
                ieee80211_try_mark_node_for_delayed_cleanup(ni);
                wlan_mlme_deauth_request(vap, ni->ni_macaddr,
                        IEEE80211_REASON_UNSPECIFIED);
                ieee80211_free_node(ni);
                goto stats_done;
            }
        }
    }

    if(ni->ni_rssi < ni->ni_rssi_min)
        ni->ni_rssi_min = ni->ni_rssi;
    else if (ni->ni_rssi > ni->ni_rssi_max)
        ni->ni_rssi_max = ni->ni_rssi;

#if OL_ATH_SUPPORT_LED
    scn->scn_led_byte_cnt += peer_stats->rx.to_stack.bytes + peer_stats->tx.tx_success.bytes;
#endif

    ieee80211_free_node(ni);

    wlan_objmgr_pdev_release_ref(pdev, WLAN_OSIF_ID);

    return 0;

stats_done:
    wlan_objmgr_pdev_release_ref(pdev, WLAN_OSIF_ID);
    return A_ERROR;

}

A_STATUS
ol_ath_update_dp_vdev_stats(void *pdev, void *stats, uint16_t vdev_id)
{
    return 0;
}

A_STATUS
ol_ath_update_dp_pdev_stats(void *pdev, void *stats, uint16_t pdev_id)
{
#if OL_ATH_SUPPORT_LED
    struct cdp_pdev_stats *pdev_stats = (struct cdp_pdev_stats *)stats;
#endif
    struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *)pdev;
    struct ol_ath_softc_net80211 *scn;

    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_OSIF_ID) != QDF_STATUS_SUCCESS)
            return A_ERROR;

    scn = (struct ol_ath_softc_net80211 *)lmac_get_pdev_feature_ptr(pdev_obj);
    if(!scn)
            goto stats_done;

#if OL_ATH_SUPPORT_LED
    scn->scn_led_byte_cnt = pdev_stats->rx.to_stack.bytes + pdev_stats->tx.tx_success.bytes;
#endif

    wlan_objmgr_pdev_release_ref(pdev, WLAN_OSIF_ID);
    return 0;

stats_done:
    wlan_objmgr_pdev_release_ref(pdev, WLAN_OSIF_ID);
    return A_ERROR;

}

void
ol_update_dp_stats(void *soc, void *stats, uint16_t id, uint8_t type)
{
    switch (type) {
        case UPDATE_PEER_STATS:
                ol_ath_update_dp_peer_stats(soc, stats, id);
            break;
        case UPDATE_VDEV_STATS:
                ol_ath_update_dp_vdev_stats(soc, stats, id);
            break;
        case UPDATE_PDEV_STATS:
                ol_ath_update_dp_pdev_stats(soc, stats, id);
            break;
        default:
            qdf_warn("apstats cannot be updated for this input type %d",type);
            break;
    }
}
EXPORT_SYMBOL(ol_update_dp_stats);
#endif /* ATH_PERF_PWR_OFFLOAD */
