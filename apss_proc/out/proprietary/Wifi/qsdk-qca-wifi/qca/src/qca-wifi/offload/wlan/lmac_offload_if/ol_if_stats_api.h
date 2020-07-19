/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 */

#ifndef __OL_IF_STATS_API_H__
#define __OL_IF_STATS_API_H__

A_STATUS ol_ath_update_dp_peer_stats(void *scn_handle, void *stats, uint16_t peer_id);
A_STATUS ol_ath_update_dp_vdev_stats(void *scn_handle, void *stats, uint16_t vdev_id);
A_STATUS ol_ath_update_dp_pdev_stats(void *scn_handle, void *stats, uint16_t pdev_id);
void ol_ath_sched_ppdu_stats(struct ol_ath_softc_net80211 *scn, struct ieee80211com *ic, void *data, uint8_t dir);
void process_rx_ppdu_stats(void *context);
void process_tx_ppdu_stats(void *context);
void ol_ath_process_ppdu_stats(void *scn, enum WDI_EVENT event,
        void *data, uint16_t peer_id, enum htt_cmn_rx_status status);

#endif /* __OL_IF_STATS_API_H__ */
