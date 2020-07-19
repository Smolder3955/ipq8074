/*
 * Copyright (c) 2013,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/* This will be registered when smart antenna is not enabled. So that WMI doesnt print
 * unhandled message.
 */
#include <ieee80211_var.h>
#include <ol_if_athvar.h>
#include "qdf_mem.h"
#include "ol_tx_desc.h"
#include "ol_if_athpriv.h"
#include <htt.h>
#include "cdp_txrx_ctrl.h"
#include <init_deinit_lmac.h>
#if UNIFIED_SMARTANTENNA
#include <wlan_sa_api_utils_defs.h>
#include <target_if_sa_api.h>
#include "ol_if_smart_ant.h"
#include "wlan_osif_priv.h"

int ol_ath_smart_ant_stats_handler(struct ol_txrx_pdev_t *txrx_pdev, uint32_t* stats_base, uint32_t msg_len)
{
#if ENHANCED_STATS
    struct sa_tx_feedback tx_feedback;
    struct ol_txrx_peer_t *txrxpeer;
    ppdu_common_stats_v3 *ppdu_stats;
    ppdu_sant_stats  *sant_stats;
    uint32_t version = 0;
    uint32_t status = 0;
#if __FW_DEBUG__
    uint32_t *dwords;
#endif
    int i = 0;
    struct wlan_objmgr_psoc *psoc;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_peer *peer;
    uint32_t sa_mode;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_ath_soc_softc_t *ol_soc;

    pdev = (struct wlan_objmgr_pdev *)txrx_pdev->ctrl_pdev;

    status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_SA_API_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_print("%s, %d unable to get reference", __func__, __LINE__);
        return status;
    }

    /* intentionally avoiding locks in data path code */
    psoc = wlan_pdev_get_psoc(pdev);
    if (!psoc) {
        wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
        return A_ERROR;
    }
    ol_soc = (ol_ath_soc_softc_t *)
            lmac_get_psoc_feature_ptr((struct wlan_objmgr_psoc *)psoc);

    if (!target_if_sa_api_is_tx_feedback_enabled(psoc, pdev)) {
        wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
        return A_ERROR;
    }

    sa_mode = target_if_sa_api_get_sa_mode(psoc, pdev);

#if ENHANCED_STATS
    if (ol_soc && ol_soc->ol_if_ops->ol_txrx_get_en_stats_base)
       ppdu_stats = (ppdu_common_stats_v3 *)ol_soc->ol_if_ops->ol_txrx_get_en_stats_base(
                       (struct cdp_pdev *)txrx_pdev, stats_base, msg_len, &version, &status);
#endif

    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    sant_stats = (ppdu_sant_stats *)
                 cdp_get_stats_base(soc_txrx_handle, (void *)txrx_pdev,
                         stats_base, msg_len, HTT_T2H_EN_STATS_TYPE_SANT);

    if ((ppdu_stats == NULL) || sant_stats == NULL) {
        wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
        return A_ERROR;
    }

#if __FW_DEBUG__
    if (ppdu_stats) {
        dwords = ppdu_stats;
        printk("PPDU Raw Bytes: 0x%08x 0x%08x 0x%08x 0x%08x \n", dwords[0], dwords[1], dwords[2], dwords[3]);
    }

    if (ppdu_stats) {
        dwords = sant_stats;
        printk("SANT Raw Bytes: 0x%08x 0x%08x 0x%08x  0x%08x \n", dwords[0], dwords[1], dwords[2], dwords[3]);
    }
#endif

    if (HTT_T2H_EN_STATS_PKT_TYPE_GET(ppdu_stats) == TX_FRAME_TYPE_DATA) { /* data frame */

        txrxpeer = (HTT_T2H_EN_STATS_PEER_ID_GET(ppdu_stats) == HTT_INVALID_PEER) ?
            NULL : txrx_pdev->peer_id_to_obj_map[HTT_T2H_EN_STATS_PEER_ID_GET(ppdu_stats)];

        if (txrxpeer && !(txrxpeer->bss_peer)) {
            peer = wlan_objmgr_get_peer(psoc, wlan_objmgr_pdev_get_pdev_id(pdev), txrxpeer->mac_addr.raw, WLAN_SA_API_ID);
            if (!peer) {
                wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
                return A_ERROR;
            }
            OS_MEMZERO(&tx_feedback, sizeof(tx_feedback));

            /* common ppdu stats */
            tx_feedback.nPackets = HTT_T2H_EN_STATS_MPDUS_QUEUED_GET(ppdu_stats);
            tx_feedback.nBad = HTT_T2H_EN_STATS_MPDUS_FAILED_GET(ppdu_stats);

            tx_feedback.rate_mcs[0] = HTT_T2H_EN_STATS_RATE_GET(ppdu_stats);

            if (sa_mode == SMART_ANT_MODE_SERIAL) {
                /* Extract and fill */
                /* index0 - s0_bw20, index1 - s0_bw40  index4 - s1_bw20 ... index7: s1_bw160 */
                for (i = 0; i < MAX_RETRIES; i++) {
                    tx_feedback.nlong_retries[i] =  ((HTT_T2H_EN_STATS_LONG_RETRIES_GET(ppdu_stats) >> (i*NIBBLE_BITS)) & MASK_LOWER_NIBBLE);
                    tx_feedback.nshort_retries[i] = ((HTT_T2H_EN_STATS_SHORT_RETRIES_GET(ppdu_stats) >> (i*NIBBLE_BITS)) & MASK_LOWER_NIBBLE);
                    /* HW gives try counts and for SA module we need to provide failure counts
                     * So manipulate short failure count accordingly.
                     */
                    if (tx_feedback.nlong_retries[i]) {
                        if (tx_feedback.nshort_retries[i] == tx_feedback.nlong_retries[i]) {
                            tx_feedback.nshort_retries[i]--;
                        }
                    }
                }
            }

            tx_feedback.rssi[0] = ppdu_stats->rssi[0];
            tx_feedback.rssi[1] = ppdu_stats->rssi[1];
            tx_feedback.rssi[2] = ppdu_stats->rssi[2];
            tx_feedback.rssi[3] = ppdu_stats->rssi[3];

            /* Smart Antenna stats */
            tx_feedback.tx_antenna[0] = sant_stats->tx_antenna;
            /* succes_idx is comming from Firmware, with recent changes success_idx is comming from
               bw_idx of ppdu stats */
            tx_feedback.rate_index = HTT_T2H_EN_STATS_BW_IDX_GET(ppdu_stats);

            if (sa_mode == SMART_ANT_MODE_SERIAL) {
                if (tx_feedback.nPackets != tx_feedback.nBad) {

                    if (tx_feedback.nlong_retries[HTT_T2H_EN_STATS_BW_IDX_GET(ppdu_stats)]) {
                        tx_feedback.nlong_retries[HTT_T2H_EN_STATS_BW_IDX_GET(ppdu_stats)] -= 1;
                    }

                    if (tx_feedback.nshort_retries[HTT_T2H_EN_STATS_BW_IDX_GET(ppdu_stats)]) {
                        tx_feedback.nshort_retries[HTT_T2H_EN_STATS_BW_IDX_GET(ppdu_stats)] -= 1;
                    }
                }
            }


            tx_feedback.is_trainpkt = sant_stats->is_training;
            tx_feedback.ratemaxphy = sant_stats->sa_max_rates;
            tx_feedback.goodput = sant_stats->sa_goodput;
#if __COMBINED_FEEDBACK__
            tx_feedback.num_comb_feedback = sant_stats->num_comfeedbacks;
            *((A_UINT32 *)&tx_feedback.comb_fb[0]) = sant_stats->combined_fb[0];
            *((A_UINT32 *)&tx_feedback.comb_fb[1]) = sant_stats->combined_fb[1];
#endif

            target_if_sa_api_update_tx_feedback(psoc, pdev, peer, &tx_feedback);
            wlan_objmgr_peer_release_ref(peer, WLAN_SA_API_ID);
        }
    }

    wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
#endif
    return 0;
}
EXPORT_SYMBOL(ol_ath_smart_ant_stats_handler);

int ol_ath_smart_ant_rxfeedback(ol_txrx_pdev_handle txrxpdev, ol_txrx_peer_handle txrxpeer, struct  sa_rx_feedback *rx_feedback)
{
    int status = -1;

	struct wlan_objmgr_psoc *psoc;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_peer *peer;

    pdev = (struct wlan_objmgr_pdev *)((struct ol_txrx_pdev_t *)txrxpdev)->ctrl_pdev;

    status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_SA_API_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_print("%s, %d unable to get reference", __func__, __LINE__);
        return status;
    }

    /* intentionally avoiding locks in data path code */
    psoc = wlan_pdev_get_psoc(pdev);
    if (!psoc) {
        wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
        return A_ERROR;
    }

    peer = wlan_objmgr_get_peer(psoc, wlan_objmgr_pdev_get_pdev_id(pdev),
                                ((struct ol_txrx_peer_t *)txrxpeer)->mac_addr.raw,
			        WLAN_SA_API_ID);

    if (peer) {
        status = target_if_sa_api_update_rx_feedback(psoc, pdev, peer, rx_feedback);
        wlan_objmgr_peer_release_ref(peer, WLAN_SA_API_ID);
    }
    wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
    return status;
}
EXPORT_SYMBOL(ol_ath_smart_ant_rxfeedback);

void
ol_ath_smart_ant_get_txfeedback (void *txrxpdev, enum WDI_EVENT event, void *data,
                                 uint16_t peer_id, enum htt_rx_status status)
{
    struct ath_smart_ant_pktlog_hdr pl_hdr;
    uint32_t *pl_tgt_hdr;
    int txstatus = 0;
    int i = 0;
    struct sa_tx_feedback tx_feedback;
    struct ol_txrx_pdev_t *txrx_pdev = (struct ol_txrx_pdev_t *)txrxpdev;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_txrx_peer_t *txrxpeer;
    struct wlan_objmgr_psoc *psoc;
    struct wlan_objmgr_peer *peer;
    struct wlan_objmgr_pdev *pdev;
    uint32_t sa_mode;
    QDF_STATUS refstatus;

    if (!txrx_pdev) {
        qdf_print("Invalid txrx pdev in %s", __func__);
        return;
    }
    pdev = (struct wlan_objmgr_pdev *) txrx_pdev->ctrl_pdev;
    scn = (struct ol_ath_softc_net80211 *)lmac_get_pdev_feature_ptr(pdev);
    if (!scn) {
        qdf_warn("scn is NULL for pdev:%pK", pdev);
        return;
    }

    refstatus = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_SA_API_ID);
    if (QDF_IS_STATUS_ERROR(refstatus)) {
        qdf_print("%s, %d unable to get reference", __func__, __LINE__);
        return;
    }

    /* intentionally avoiding locks in data path code */
    psoc = wlan_pdev_get_psoc(pdev);
    if (!psoc) {
        wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
        return;
    }

    if (event != WDI_EVENT_TX_STATUS) {
        qdf_print("%s: Un Subscribed Event: %d ", __func__, event);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
        return;
    }

    if (!txrx_pdev) {
        qdf_print("Invalid pdev in %s", __func__);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
        return;
    }

    sa_mode = target_if_sa_api_get_sa_mode(psoc, pdev);

    pl_tgt_hdr = (uint32_t *)data;
    pl_hdr.log_type =  (*(pl_tgt_hdr + ATH_SMART_ANT_PKTLOG_HDR_LOG_TYPE_OFFSET) &
                                        ATH_SMART_ANT_PKTLOG_HDR_LOG_TYPE_MASK) >>
                                        ATH_SMART_ANT_PKTLOG_HDR_LOG_TYPE_SHIFT;

    if ((pl_hdr.log_type == SMART_ANT_PKTLOG_TYPE_TX_CTRL)) {
        int frame_type;
        int peer_id;
        void *tx_ppdu_ctrl_desc;
        u_int32_t *tx_ctrl_ppdu, try_status = 0;
        uint8_t total_tries =0, sbw_indx_succ = 0;

        tx_ppdu_ctrl_desc = (void *)data + sizeof(struct ath_smart_ant_pktlog_hdr);

        tx_ctrl_ppdu = (u_int32_t *)tx_ppdu_ctrl_desc;

        peer_id = tx_ctrl_ppdu[TX_PEER_ID_OFFSET];

        frame_type = (tx_ctrl_ppdu[TX_FRAME_OFFSET]
                          & TX_FRAME_TYPE_MASK) >> TX_FRAME_TYPE_SHIFT;

        if (frame_type == TX_FRAME_TYPE_DATA) { /* data frame */

            if (scn->tx_ppdu_end[SMART_ANT_FEEDBACK_OFFSET] == 0) {
                wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
                return;
            }

            txrxpeer = (peer_id == HTT_INVALID_PEER) ?
                NULL : txrx_pdev->peer_id_to_obj_map[peer_id];
            if (txrxpeer && !(txrxpeer->bss_peer)) {

                peer = wlan_objmgr_get_peer(psoc, wlan_objmgr_pdev_get_pdev_id(pdev), txrxpeer->mac_addr.raw, WLAN_SA_API_ID);
                if (!peer) {
                    wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
                    return;
                }

                total_tries = (scn->tx_ppdu_end[TX_TOTAL_TRIES_OFFSET] & TX_TOTAL_TRIES_MASK) >> TX_TOTAL_TRIES_SHIFT;

                OS_MEMZERO(&tx_feedback, sizeof(tx_feedback));
                tx_feedback.nPackets = (scn->tx_ppdu_end[SMART_ANT_FEEDBACK_OFFSET] & 0xffff);
                tx_feedback.nBad = (scn->tx_ppdu_end[SMART_ANT_FEEDBACK_OFFSET] & 0x1fff0000) >> 16;

                /* Rate code and Antenna values */
                tx_feedback.tx_antenna[0] = (tx_ctrl_ppdu[TX_ANT_OFFSET_S0] & TX_ANT_MASK);
                tx_feedback.tx_antenna[1] = (tx_ctrl_ppdu[TX_ANT_OFFSET_S1] & TX_ANT_MASK);

                /* RateCode */
                tx_feedback.rate_mcs[0] = ((tx_ctrl_ppdu[TXCTRL_S0_RATE_BW20_OFFSET] & TXCTRL_RATE_MASK) >> 24) |
                                          ((tx_ctrl_ppdu[TXCTRL_S0_RATE_BW40_OFFSET] & TXCTRL_RATE_MASK) >> 16) |
                                          ((tx_ctrl_ppdu[TXCTRL_S0_RATE_BW80_OFFSET] & TXCTRL_RATE_MASK) >> 8) |
                                          (tx_ctrl_ppdu[TXCTRL_S0_RATE_BW160_OFFSET] & TXCTRL_RATE_MASK);

                tx_feedback.rate_mcs[1] = ((tx_ctrl_ppdu[TXCTRL_S1_RATE_BW20_OFFSET] & TXCTRL_RATE_MASK) >> 24) |
                                          ((tx_ctrl_ppdu[TXCTRL_S1_RATE_BW40_OFFSET] & TXCTRL_RATE_MASK) >> 16) |
                                          ((tx_ctrl_ppdu[TXCTRL_S1_RATE_BW80_OFFSET] & TXCTRL_RATE_MASK) >> 8) |
                                          (tx_ctrl_ppdu[TXCTRL_S1_RATE_BW160_OFFSET] & TXCTRL_RATE_MASK);


                if (sa_mode == SMART_ANT_MODE_SERIAL) {
                    /* Extract and fill */
                    /* index0 - s0_bw20, index1 - s0_bw40  index4 - s1_bw20 ... index7: s1_bw160 */
                    for (i = 0; i < MAX_RETRIES; i++) {
                        tx_feedback.nlong_retries[i] =  ((scn->tx_ppdu_end[LONG_RETRIES_OFFSET] >> (i*4)) & 0x0f);
                        tx_feedback.nshort_retries[i] = ((scn->tx_ppdu_end[SHORT_RETRIES_OFFSET] >> (i*4)) & 0x0f);

                        /* HW gives try counts and for SA module we need to provide failure counts
                         * So manipulate short failure count accordingly.
                         */
                        if (tx_feedback.nlong_retries[i]) {
                            if (tx_feedback.nshort_retries[i] == tx_feedback.nlong_retries[i]) {
                                tx_feedback.nshort_retries[i]--;
                            }
                        }
                    }
                }
                /* ACK RSSI */
                tx_feedback.rssi[0] = scn->tx_ppdu_end[ACK_RSSI0_OFFSET];
                tx_feedback.rssi[1] = scn->tx_ppdu_end[ACK_RSSI1_OFFSET];
                tx_feedback.rssi[2] = scn->tx_ppdu_end[ACK_RSSI2_OFFSET];
                tx_feedback.rssi[3] = scn->tx_ppdu_end[ACK_RSSI3_OFFSET];

                try_status = scn->tx_ppdu_end[total_tries-1];
                sbw_indx_succ = (try_status & TX_TRY_SERIES_MASK)?NUM_DYN_BW_MAX:0;
                sbw_indx_succ += ((try_status & TX_TRY_BW_MASK) >> TX_TRY_BW_SHIFT);
                if (sa_mode == SMART_ANT_MODE_SERIAL) {
                    if (tx_feedback.nPackets != tx_feedback.nBad) {

                        if (tx_feedback.nlong_retries[sbw_indx_succ]) {
                            tx_feedback.nlong_retries[sbw_indx_succ] -= 1;
                        }

                        if (tx_feedback.nshort_retries[sbw_indx_succ]) {
                            tx_feedback.nshort_retries[sbw_indx_succ] -= 1;
                        }
                    }
                }

                tx_feedback.rate_index = sbw_indx_succ;
                tx_feedback.is_trainpkt = ((scn->tx_ppdu_end[SMART_ANT_FEEDBACK_OFFSET] & SMART_ANT_FEEDBACK_TRAIN_MASK) ? 1: 0);
                tx_feedback.ratemaxphy =  (scn->tx_ppdu_end[SMART_ANT_FEEDBACK_OFFSET_2]);
                tx_feedback.goodput =  (scn->tx_ppdu_end[(SMART_ANT_FEEDBACK_OFFSET_2+1)]);

                tx_feedback.num_comb_feedback = (scn->tx_ppdu_end[SMART_ANT_FEEDBACK_OFFSET]  & 0x60000000) >> 29;
                *((uint32_t *)&tx_feedback.comb_fb[0]) = scn->tx_ppdu_end[LONG_RETRIES_OFFSET];
                *((uint32_t *)&tx_feedback.comb_fb[1]) = scn->tx_ppdu_end[SHORT_RETRIES_OFFSET];

                /* Data recevied from the associated node, Prepare TX feed back structure and send to SA module */
                txstatus = target_if_sa_api_update_tx_feedback(psoc, pdev, peer, &tx_feedback);
                wlan_objmgr_peer_release_ref(peer, WLAN_SA_API_ID);
            }
        }
    } else {
        /* First We will get status */
        if (pl_hdr.log_type == SMART_ANT_PKTLOG_TYPE_TX_STAT) {
            void *tx_ppdu_status_desc;
            u_int32_t *tx_status_ppdu;
            tx_ppdu_status_desc = (void *)data + sizeof(struct ath_smart_ant_pktlog_hdr);
            tx_status_ppdu = (u_int32_t *)tx_ppdu_status_desc;
            /* cache ppdu end (tx status desc) for smart antenna txfeedback */
            OS_MEMCPY(&scn->tx_ppdu_end, tx_status_ppdu, (sizeof(uint32_t)*MAX_TX_PPDU_SIZE));
        }
    }
    wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
    return;
}

int ol_ath_smart_ant_enable_txfeedback(struct wlan_objmgr_pdev *pdev, int enable)
{
    struct ol_ath_softc_net80211 *scn;
    struct smart_ant_enable_tx_feedback_params param;
    struct pdev_osif_priv *osif_priv = NULL;
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;
    struct common_wmi_handle *pdev_wmi_handle;

    /* intentionally avoiding locks in data path code */
    osif_priv = wlan_pdev_get_ospriv(pdev);
    if (NULL == osif_priv) {
        qdf_print("%s: osif_priv is NULL!", __func__);
        return A_ERROR;
    }

    scn = (ol_scn_t)osif_priv->legacy_osif_priv;
    if (NULL == scn) {
        qdf_print("%s: scn is NULL!", __func__);
        return A_ERROR;
    }

    qdf_mem_set(&param, sizeof(param), 0);
    param.enable = enable;
    soc_txrx_handle = wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(pdev));
    pdev_txrx_handle = wlan_pdev_get_dp_handle(pdev);
    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);

    if (enable == 1) {
        /* Call back for txfeedback */
        ((scn->sa_event_sub).callback) = ol_ath_smart_ant_get_txfeedback;
        ((scn->sa_event_sub).context) = pdev_txrx_handle;
        if(cdp_wdi_event_sub(soc_txrx_handle,
                        (struct cdp_pdev *)pdev_txrx_handle,
                        &(scn->sa_event_sub),
                        WDI_EVENT_TX_STATUS)) {
            return A_ERROR;
        }
    } else if (enable == 0) {
        if(cdp_wdi_event_unsub(soc_txrx_handle,
                    (struct cdp_pdev *)pdev_txrx_handle,
                    &(scn->sa_event_sub),
                    WDI_EVENT_TX_STATUS)) {
            return A_ERROR;
        }
    }

    return wmi_unified_smart_ant_enable_tx_feedback_cmd_send(pdev_wmi_handle, &param);
}
EXPORT_SYMBOL(ol_ath_smart_ant_enable_txfeedback);

#endif
