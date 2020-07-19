/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
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

#if ATH_SUPPORT_DFS

#include "ol_if_athvar.h"
#include "target_type.h"
#include "qdf_mem.h"   /* qdf_mem_malloc,free */
#include "qdf_lock.h"  /* qdf_spinlock_* */
#include "qdf_types.h" /* qdf_vprint */
#include "ol_regdomain.h"
#include <wlan_osif_priv.h>
#include "init_deinit_lmac.h"

#if ATH_PERF_PWR_OFFLOAD

QDF_STATUS
ol_dfs_get_caps(struct wlan_objmgr_pdev *pdev,
        struct wlan_dfs_caps *dfs_caps)
{
    struct ol_ath_softc_net80211 *scn;
    struct pdev_osif_priv *osif_priv;

    osif_priv = wlan_pdev_get_ospriv(pdev);

    if (osif_priv == NULL) {
        qdf_print("%s : osif_priv is NULL", __func__);
        return QDF_STATUS_E_FAILURE;
    }

    scn = (struct ol_ath_softc_net80211 *)osif_priv->legacy_osif_priv;

    dfs_caps->wlan_dfs_combined_rssi_ok = 0;
    dfs_caps->wlan_dfs_ext_chan_ok = 0;
    dfs_caps->wlan_dfs_use_enhancement = 0;
    dfs_caps->wlan_strong_signal_diversiry = 0;
    dfs_caps->wlan_fastdiv_val = 0;
    dfs_caps->wlan_chip_is_bb_tlv = 1;
    dfs_caps->wlan_chip_is_over_sampled = 0;
    dfs_caps->wlan_chip_is_ht160 = 0;


    /*
     * Disable check for strong OOB radar as this
     * has side effect (IR 095628, 094131
     * Set the capability to off (0) by default.
     * We will turn this on once we have resolved
     * issue with the fix
     */

    dfs_caps->wlan_chip_is_false_detect = 0;
    switch (lmac_get_tgt_type(scn->soc->psoc_obj)) {
        case TARGET_TYPE_AR900B:
            break;

        case TARGET_TYPE_IPQ4019:
            dfs_caps->wlan_chip_is_false_detect = 0;
            break;

        case TARGET_TYPE_AR9888:
            /* Peregrine is over sampled */
            dfs_caps->wlan_chip_is_over_sampled = 1;
            break;

        case TARGET_TYPE_QCA9984:
        case TARGET_TYPE_QCA9888:
            /* Cascade and Besra supports 160MHz channel */
            dfs_caps->wlan_chip_is_ht160 = 1;
            break;
        default:
            break;
    }

    return(0);
}

/*
 * ic_dfs_enable - enable DFS
 *
 * For offload solutions, radar PHY errors will be enabled by the target
 * firmware when DFS is requested for the current channel.
 */
QDF_STATUS
ol_if_dfs_enable(struct wlan_objmgr_pdev *pdev, int *is_fastclk,
        struct wlan_dfs_phyerr_param *param,
        uint32_t dfsdomain)
{

    qdf_print(KERN_DEBUG"%s: called", __func__);

    /*
     * XXX For peregrine, treat fastclk as the "oversampling" mode.
     *     It's on by default.  This may change at some point, so
     *     we should really query the firmware to find out what
     *     the current configuration is.
     */
    (* is_fastclk) = 1;

    return QDF_STATUS_SUCCESS;
}

/*
 * ic_dfs_disable
 */
QDF_STATUS
ol_if_dfs_disable(struct wlan_objmgr_pdev *pdev, int no_cac)
{
    qdf_print(KERN_DEBUG"%s: called", __func__);

    return (0);
}

/*
 * ic_dfs_get_thresholds
 */
QDF_STATUS ol_if_dfs_get_thresholds(struct wlan_objmgr_pdev *pdev,
        struct wlan_dfs_phyerr_param *param)
{
    /*
     * XXX for now, the hardware has no API for fetching
     * the radar parameters.
     */
    param->pe_firpwr = 0;
    param->pe_rrssi = 0;
    param->pe_height = 0;
    param->pe_prssi = 0;
    param->pe_inband = 0;
    param->pe_relpwr = 0;
    param->pe_relstep = 0;
    param->pe_maxlen = 0;

    return 0;
}

/*
 * ic_get_ext_busy
 */
QDF_STATUS
ol_if_dfs_get_ext_busy(struct wlan_objmgr_pdev *pdev, int *ext_chan_busy)
{
    *ext_chan_busy = 0;
    return (0);
}

/*
 * XXX this doesn't belong here, but the DFS code requires that it exists.
 * Please figure out how to fix this!
 */
QDF_STATUS
ol_if_get_tsf64(struct wlan_objmgr_pdev *pdev, uint64_t *tsf64)
{
	/* XXX TBD */
	return (0);
}

#endif /* ATH_PERF_PWR_OFFLOAD */

/*
 * host_dfs_check_support
 */
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
QDF_STATUS
ol_if_is_host_dfs_check_support_enabled(struct wlan_objmgr_pdev * pdev,
        bool *enabled)
{
    struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(pdev);
    struct common_wmi_handle* wmi_handle = lmac_get_wmi_hdl(psoc);

    if (!wmi_handle) {
        qdf_print("%s : wmi_handle is NULL", __func__);
        return QDF_STATUS_E_FAILURE;
    }

    *enabled = wmi_service_enabled(wmi_handle,
            wmi_service_host_dfs_check_support);

    return QDF_STATUS_SUCCESS;
}
#endif /* HOST_DFS_SPOOF_TEST */
#endif /* ATH_SUPPORT_DFS */
