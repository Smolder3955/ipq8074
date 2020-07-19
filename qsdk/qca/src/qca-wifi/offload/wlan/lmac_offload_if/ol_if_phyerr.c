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
 * LMAC management specific offload interface functions - for PHY error handling
 */
#include "ol_if_athvar.h"
#include "target_type.h"

/* This enables verbose printing of PHY error debugging */
#define ATH_PHYERR_DEBUG        0

#if ATH_PERF_PWR_OFFLOAD

#if WLAN_SPECTRAL_ENABLE
#include "ol_if_spectral.h"
#include <target_if_spectral.h>
#endif

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_lmac_if_api.h>
#include "init_deinit_lmac.h"

#define AR900B_DFS_PHYERR_MASK      0x4
#define AR900B_SPECTRAL_PHYERR_MASK 0x4000000

static int
ol_ath_phyerr_rx_event_handler_wifi2_0(ol_ath_soc_softc_t *soc, u_int8_t *data,
        u_int16_t datalen);
/*
 * XXX TODO: talk to Nitin about the endian-ness of this - what's
 * being byteswapped by the firmware?!
 */

static int
ol_ath_phyerr_rx_event_handler(ol_scn_t sc, u_int8_t *data,
    u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    wmi_host_phyerr_t phyerr;
#if ATH_SUPPORT_DFS || WLAN_SPECTRAL_ENABLE
    struct ieee80211com *ic;
#endif /* ATH_SUPPORT_DFS || WLAN_SPECTRAL_ENABLE */
    uint16_t buf_offset = 0;
#if WLAN_SPECTRAL_ENABLE
    struct target_if_spectral_acs_stats acs_stats;
#endif
    struct wlan_objmgr_psoc *psoc = soc->psoc_obj;
    struct wlan_objmgr_pdev *pdev;
    uint32_t target_type;
    struct common_wmi_handle *wmi_handle;
#if ATH_SUPPORT_DFS
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
#endif
    if (psoc == NULL) {
        qdf_print("%s : psoc is null", __func__);
        return -1;
    }

    target_type = lmac_get_tgt_type(psoc);
    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    if (target_type == TARGET_TYPE_AR900B  || target_type == TARGET_TYPE_QCA9984 || \
                        target_type == TARGET_TYPE_QCA9888 || target_type == TARGET_TYPE_IPQ4019) {
        return ol_ath_phyerr_rx_event_handler_wifi2_0(soc, data, datalen);
    }

    /* Ensure it's at least the size of the header */
    if(wmi_extract_comb_phyerr(wmi_handle, data,
                       datalen, &buf_offset, &phyerr )) {
#if WLAN_SPECTRAL_ENABLE
        soc->spectral_stats.phydata_rx_errors++;
#endif
        return (1);             /* XXX what should errors be? */
    }

    pdev = wlan_objmgr_get_pdev_by_id(psoc, PDEV_UNIT(phyerr.pdev_id),
                                   WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_print("%s: pdev object (id: %d) is NULL ", __func__,
                   PDEV_UNIT(phyerr.pdev_id));
         return -1;
    }

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (ic == NULL) {
         wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
         qdf_err("ic (id: %d) is NULL", PDEV_UNIT(phyerr.pdev_id));
         return -1;
    }

#if ATH_SUPPORT_DFS || WLAN_SPECTRAL_ENABLE
    dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
#endif
    /* Loop over the bufp, extracting out phyerrors */
    /*
     * XXX wmi_unified_comb_phyerr_rx_event.bufp is a char pointer,
     * which isn't correct here - what we have received here
     * is an array of TLV-style PHY errors.
     */
    while (buf_offset < datalen) {

        if(wmi_extract_single_phyerr(wmi_handle, data,
                    datalen, &buf_offset, &phyerr )) {
#if WLAN_SPECTRAL_ENABLE
            soc->spectral_stats.phydata_rx_errors++;
#endif
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
            return (1);             /* XXX what should errors be? */
        }
#if ATH_SUPPORT_DFS && WLAN_DFS_PARTIAL_OFFLOAD
        /*
         * If required, pass radar events to the dfs pattern matching code.
         *
         * Don't pass radar events with no buffer payload.
         */
        if (phyerr.phy_err_code == 0x5 || phyerr.phy_err_code == 0x24) {
            if (dfs_rx_ops && dfs_rx_ops->dfs_process_phyerr) {
               dfs_rx_ops->dfs_process_phyerr(pdev,
                        &phyerr.bufp[0], phyerr.buf_len,
                        phyerr.rf_info.rssi_comb & 0xff,
                        phyerr.rf_info.rssi_comb & 0xff, /* XXX Extension RSSI */
                        phyerr.tsf_timestamp,
                        phyerr.tsf64);
            }
        }
#endif /* ATH_SUPPORT_DFS && WLAN_DFS_PARTIAL_OFFLOAD */

#if WLAN_SPECTRAL_ENABLE

        /*
         * If required, pass spectral events to the spectral module
         *
         */
        if (phyerr.phy_err_code == PHY_ERROR_FALSE_RADAR_EXT ||
            phyerr.phy_err_code == PHY_ERROR_SPECTRAL_SCAN) {
            if (phyerr.buf_len > 0) {
                struct target_if_spectral_rfqual_info rfqual_info;
                struct target_if_spectral_chan_info   chan_info;

                OS_MEMCPY(&rfqual_info, &phyerr.rf_info, sizeof(wmi_host_rf_info_t));
                OS_MEMCPY(&chan_info, &phyerr.chan_info, sizeof(wmi_host_chan_info_t));

                if (phyerr.phy_err_code == PHY_ERROR_SPECTRAL_SCAN) {
                    target_if_spectral_process_phyerr(ic->ic_pdev_obj, phyerr.bufp, phyerr.buf_len,
                                    &rfqual_info, &chan_info, phyerr.tsf64, &acs_stats);
                }

#if ATH_ACS_SUPPORT_SPECTRAL && WLAN_SPECTRAL_ENABLE
                if (phyerr.phy_err_code == PHY_ERROR_SPECTRAL_SCAN) {
                    ieee80211_init_spectral_chan_loading(ic, chan_info.center_freq1, 0);
                    ieee80211_update_eacs_counters(ic, acs_stats.nfc_ctl_rssi,
                                                       acs_stats.nfc_ext_rssi,
                                                       acs_stats.ctrl_nf,
                                                       acs_stats.ext_nf);
                }
#endif
            }
        }
#endif  /* WLAN_SPECTRAL_ENABLE */

    }
     wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
     return (0);
}

static int
ol_ath_phyerr_rx_event_handler_wifi2_0(ol_ath_soc_softc_t *soc, u_int8_t *data,
        u_int16_t datalen)
{
    struct ieee80211com *ic;
    wmi_host_phyerr_t phyerr;
#if WLAN_SPECTRAL_ENABLE
    struct target_if_spectral_acs_stats acs_stats;
#endif
    struct wlan_objmgr_psoc *psoc;
    struct wlan_objmgr_pdev *pdev;
#if ATH_SUPPORT_DFS
    struct wlan_lmac_if_dfs_rx_ops *dfs_rx_ops;
#endif
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
   /* sanity check on data length */
    if (wmi_extract_composite_phyerr(wmi_handle, data,
                       datalen, &phyerr )) {
#if WLAN_SPECTRAL_ENABLE
        soc->spectral_stats.phydata_rx_errors++;
#endif
        return (1);
    }
    psoc = soc->psoc_obj;
    if (psoc == NULL) {
        qdf_print("%s : psoc is null", __func__);
        return -1;
    }

    pdev = wlan_objmgr_get_pdev_by_id(psoc, PDEV_UNIT(phyerr.pdev_id),
                                   WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_print("%s: pdev object (id: %d) is NULL ", __func__, PDEV_UNIT(phyerr.pdev_id));
         return -1;
    }

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (ic == NULL) {
         wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
         qdf_err("ic (id: %d) is NULL", PDEV_UNIT(phyerr.pdev_id));
         return -1;
    }


#if ATH_SUPPORT_DFS
    dfs_rx_ops = wlan_lmac_if_get_dfs_rx_ops(psoc);
#endif
    /* handle different PHY Error conditions */
    if (((phyerr.phy_err_mask0 & (AR900B_DFS_PHYERR_MASK | AR900B_SPECTRAL_PHYERR_MASK)) == 0)) {
        /* unknown PHY error, currently only Spectral and DFS are handled */
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return (1);
    }

    /* Handle Spectral PHY Error */
    if ((phyerr.phy_err_mask0 & AR900B_SPECTRAL_PHYERR_MASK)) {
#if WLAN_SPECTRAL_ENABLE
        if (phyerr.buf_len > 0) {
            struct target_if_spectral_rfqual_info rfqual_info;
            struct target_if_spectral_chan_info   chan_info;

            OS_MEMCPY(&rfqual_info, &phyerr.rf_info, sizeof(wmi_host_rf_info_t));
            OS_MEMCPY(&chan_info, &phyerr.chan_info, sizeof(wmi_host_chan_info_t));

            if ( phyerr.phy_err_mask0 & AR900B_SPECTRAL_PHYERR_MASK) {
                target_if_spectral_process_phyerr(ic->ic_pdev_obj, phyerr.bufp, phyerr.buf_len,
                                &rfqual_info, &chan_info, phyerr.tsf64, &acs_stats);
             }

#if ATH_ACS_SUPPORT_SPECTRAL && WLAN_SPECTRAL_ENABLE
             if ( phyerr.phy_err_mask0 & AR900B_SPECTRAL_PHYERR_MASK) {
                    ieee80211_init_spectral_chan_loading(ic, chan_info.center_freq1, 0);
                    ieee80211_update_eacs_counters(ic, acs_stats.nfc_ctl_rssi,
                                                       acs_stats.nfc_ext_rssi,
                                                       acs_stats.ctrl_nf,
                                                       acs_stats.ext_nf);
             }
#endif
        }
#endif  /* WLAN_SPECTRAL_ENABLE */

    } else if ((phyerr.phy_err_mask0 & AR900B_DFS_PHYERR_MASK)) {
        if (phyerr.buf_len > 0) {
#if ATH_SUPPORT_DFS && WLAN_DFS_PARTIAL_OFFLOAD
            if (dfs_rx_ops && dfs_rx_ops->dfs_process_phyerr) {
                dfs_rx_ops->dfs_process_phyerr(pdev,
                        phyerr.bufp, phyerr.buf_len,
                        phyerr.rf_info.rssi_comb & 0xff,
                        phyerr.rf_info.rssi_comb & 0xff, /* XXX Extension RSSI */
                        phyerr.tsf_timestamp,
                        phyerr.tsf64);
            }
#endif /*ATH_SUPPORT_DFS && WLAN_DFS_PARTIAL_OFFLOAD */
        }
    }
     wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
     return (0);
}

/*
 * Enable PHY errors.
 *
 * For now, this just enables the DFS PHY errors rather than
 * being able to select which PHY errors to enable.
 */
void
ol_ath_phyerr_enable(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    wmi_unified_phyerr_enable_cmd_send(pdev_wmi_handle);
}

/*
 * Disbale PHY errors.
 *
 * For now, this just disables the DFS PHY errors rather than
 * being able to select which PHY errors to disable.
 */
void
ol_ath_phyerr_disable(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    wmi_unified_phyerr_disable_cmd_send(pdev_wmi_handle);
}

/*
 * PHY error attach functions for offload solutions
 */
void
ol_ath_phyerr_attach(ol_ath_soc_softc_t *soc)
{
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);

    /* Register WMI event handlers */
    wmi_unified_register_event_handler((void *)wmi_handle,
      wmi_phyerr_event_id,
      ol_ath_phyerr_rx_event_handler,
      WMI_RX_UMAC_CTX);
}

/*
 * Detach the PHY error module.
 */
void ol_ath_phyerr_detach(ol_ath_soc_softc_t *soc)
{

    /* XXX TODO: see what needs to be unregistered */
    qdf_print(KERN_INFO"%s: called", __func__);
}

#endif /* ATH_PERF_PWR_OFFLOAD */
