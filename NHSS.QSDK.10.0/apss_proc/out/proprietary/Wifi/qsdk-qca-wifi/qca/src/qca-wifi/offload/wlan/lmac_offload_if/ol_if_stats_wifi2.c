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
#include "ol_ath.h"
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include "qdf_lock.h"  /* qdf_spinlock_* */
#include "qdf_types.h" /* qdf_vprint */
#include "dbglog_host.h"
#include "a_debug.h"
#include <wdi_event_api.h>
#include <ol_txrx_types.h>
#include <ol_txrx_stats.h>
#include <init_deinit_lmac.h>
#include <htt_internal.h>
#include <net.h>
#include <pktlog_ac_api.h>
#include <pktlog_ac_fmt.h>
#include <pktlog_ac_i.h>
#include "ol_tx_desc.h"
#include "ol_ratetable.h"
#include "ol_if_stats.h"
#include "osif_private.h"
#if QCA_AIRTIME_FAIRNESS
#include <target_if_atf.h>
#endif
#include "cepci.h"
#include "ath_pci.h"
#include "legacy/htt.h"
#include "cdp_txrx_ctrl.h"

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#endif
#include <wlan_son_pub.h>
#if WLAN_SPECTRAL_ENABLE
#include <target_if_spectral.h>
#endif

#if ATH_PERF_PWR_OFFLOAD
void
ol_get_wlan_dbg_stats(struct ol_ath_softc_net80211 *scn,
        void *dbg_stats)
{
    OS_MEMCPY(dbg_stats, &scn->ath_stats, sizeof(ol_dbg_stats));
}
qdf_export_symbol(ol_get_wlan_dbg_stats);

void ol_ath_chan_change_msg_handler(struct ol_txrx_pdev_t *txrx_pdev,
                                  uint32_t* msg_word, uint32_t msg_len)
{
    struct ol_ath_softc_net80211 *scn = NULL;
    struct wlan_objmgr_pdev *pdev_obj = (struct wlan_objmgr_pdev *)txrx_pdev->ctrl_pdev;

    scn = (struct ol_ath_softc_net80211 *)lmac_get_pdev_feature_ptr(pdev_obj);
    if (!scn) {
        qdf_warn("scn is NULL for pdev:%pK", pdev_obj);
        return;
    }

    scn->sc_chan_freq = *(msg_word + 1);
    scn->sc_chan_band_center_f1 = *(msg_word + 2);
    scn->sc_chan_band_center_f2 = *(msg_word + 3);
    scn->sc_chan_phy_mode = *(msg_word + 4);
 /*   printk("\nfreq %d bc_f1 %d bc_f2 %d mode %d\n",
                  scn->sc_chan_freq,scn->sc_chan_band_center_f1,
                  scn->sc_chan_band_center_f2,scn->sc_chan_phy_mode);
*/
}
qdf_export_symbol(ol_ath_chan_change_msg_handler);

#define IEEE80211_DEFAULT_CHAN_STATS_PERIOD     (1000)
void ol_ath_stats_attach_wifi2(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);

    /* enable the pdev stats event */
    ol_ath_pdev_set_param(scn,
                wmi_pdev_param_pdev_stats_update_period,
                PDEV_DEFAULT_STATS_UPDATE_PERIOD);

    /* enable the pdev stats event */
    ol_ath_pdev_set_param(scn,
                wmi_pdev_param_vdev_stats_update_period,
                VDEV_DEFAULT_STATS_UPDATE_PERIOD);
    /* enable the pdev stats event */
    ol_ath_pdev_set_param(scn,
                wmi_pdev_param_peer_stats_update_period,
                PEER_DEFAULT_STATS_UPDATE_PERIOD);
    /* enable periodic chan stats event */
    ol_ath_periodic_chan_stats_config(scn, true,
                IEEE80211_DEFAULT_CHAN_STATS_PERIOD);

}
qdf_export_symbol(ol_ath_stats_attach_wifi2);
#endif /* ATH_PERF_PWR_OFFLOAD */
