/*
 * Copyright (c) 2015-2016,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef __OSIF_NSS_WIFI_2_0_IF_H
#define __OSIF_NSS_WIFI_2_0_IF_H
#include <ol_if_athvar.h>
int osif_nss_ol_pdev_attach(struct ol_ath_softc_net80211 *scn, void *nss_wifiol_ctx,
        int radio_id,
        uint32_t desc_pool_size,
        uint32_t *tx_desc_array,
        uint32_t wlanextdesc_addr,
        uint32_t wlanextdesc_size,
        uint32_t htt_tx_desc_base_vaddr, uint32_t htt_tx_desc_base_paddr ,
        uint32_t htt_tx_desc_offset,
        uint32_t pmap_addr);

int osif_nss_ol_htt_rx_init(void *htt_handle );

int osif_ol_nss_htc_send(ol_txrx_pdev_handle pdev, void *nss_wifiol_ctx, int nssid, qdf_nbuf_t netbuf, uint32_t transfer_id );
int osif_nss_ol_mgmt_frame_send(ol_txrx_pdev_handle pdev, void *nss_wifiol_ctx, int32_t radio_id, uint32_t desc_id, qdf_nbuf_t netbuf);
int osif_nss_ol_stats_frame_send(ol_txrx_pdev_handle pdev, void *nss_wifiol_ctx, int32_t radio_id, qdf_nbuf_t netbuf);
void osif_nss_ol_set_msdu_ttl(ol_txrx_pdev_handle pdev);
int osif_nss_peer_stats_frame_send(struct ol_txrx_pdev_t *pdev, uint16_t peer_id);
#endif
