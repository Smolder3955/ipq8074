/*
 * Copyright (c) 2011,2015,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011, 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file htt_h2t.c
 * @brief Provide functions to send host->target HTT messages.
 * @details
 *  This file contains functions related to host->target HTT messages.
 *  There are a couple aspects of this host->target messaging:
 *  1.  This file contains the function that is called by HTC when
 *      a host->target send completes.
 *      This send-completion callback is primarily relevant to HL,
 *      to invoke the download scheduler to set up a new download,
 *      and optionally free the tx frame whose download is completed.
 *      For both HL and LL, this completion callback frees up the
 *      HTC_PACKET object used to specify the download.
 *  2.  This file contains functions for creating messages to send
 *      from the host to the target.
 */

#include <qdf_mem.h>  /* qdf_mem_copy */
#include <qdf_nbuf.h>    /* qdf_nbuf_map_single */
#include <htc_api.h>     /* HTC_PACKET */
#include <htt.h>         /* HTT host->target msg defs */
#include <ol_txrx_htt_api.h> /* ol_tx_completion_handler, htt_tx_status */
#include <ol_htt_tx_api.h>
#include <hif.h>

#include <htt_internal.h>

#include <ol_txrx_types.h>
#include <htc.h>

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#include <osif_nss_wifiol_2_0_if.h>
#endif
#include <wlan_lmac_dispatcher.h>

#if QCA_PARTNER_DIRECTLINK_TX
extern uint32_t wlan_pltfrm_directlink_get_bank_base(void);
extern int CE_is_directlink(struct CE_handle* ce_tx_hdl);
#endif /* QCA_PARTNER_DIRECTLINK_TX */

#define HTT_MSG_BUF_SIZE(msg_bytes) \
   ((msg_bytes) + HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING)

#ifndef container_of
#define container_of(ptr, type, member) ((type *)( \
                (char *)(ptr) - (char *)(&((type *)0)->member) ) )
#endif

static void
htt_h2t_send_complete_free_netbuf(
    void *pdev, A_STATUS status, qdf_nbuf_t netbuf, u_int16_t msdu_id)
{
    qdf_nbuf_free(netbuf);
}

void
htt_h2t_send_complete(void *context, HTC_PACKET *htc_pkt)
{
    void (*send_complete_part2)(
        void *pdev, A_STATUS status, qdf_nbuf_t msdu, u_int16_t msdu_id);
    struct htt_pdev_t *pdev =  (struct htt_pdev_t *) context;
    struct htt_htc_pkt *htt_pkt;
    qdf_nbuf_t netbuf;

    send_complete_part2 = htc_pkt->pPktContext;

    htt_pkt = container_of(htc_pkt, struct htt_htc_pkt, htc_pkt);

    /* process (free or keep) the netbuf that held the message */
    netbuf = (qdf_nbuf_t) htc_pkt->pNetBufContext;
    /*
     * adf sendcomplete is required for windows only
     */
    /* qdf_nbuf_set_sendcompleteflag(netbuf, TRUE); */
    if (send_complete_part2 != NULL) {
        send_complete_part2(
            htt_pkt->pdev_ctxt, htc_pkt->Status, netbuf, htt_pkt->msdu_id);
    }
    /* free the htt_htc_pkt / HTC_PACKET object */
    htt_htc_pkt_free(pdev, htt_pkt);
}

enum htc_send_full_action
htt_h2t_full(void *context, HTC_PACKET *pkt)
{
/* FIX THIS */
    return HTC_SEND_FULL_KEEP;
}

A_STATUS
htt_h2t_frag_desc_bank_cfg_msg(struct htt_pdev_t *pdev)
{
    struct htt_htc_pkt *pkt;
    qdf_nbuf_t msg;
    u_int32_t *msg_word;
    struct htt_tx_frag_desc_bank_cfg_t *bank_cfg;
    qdf_device_t qdf_dev;
#if PEER_FLOW_CONTROL
    u_int32_t paddr_lo = 0;
#endif

    qdf_dev = wlan_psoc_get_qdf_dev((void *)pdev->ctrl_psoc);
    msg = qdf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(sizeof(struct htt_tx_frag_desc_bank_cfg_t)),
        /* reserve room for the HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, TRUE);
    if (!msg) {
        return A_ERROR; /* failure */
    }

    /*
     * Set the length of the message.
     * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
     * separately during the below call to qdf_nbuf_push_head.
     * The contribution from the HTC header is added separately inside HTC.
     */
    if (qdf_nbuf_put_tail(msg, sizeof(struct htt_tx_frag_desc_bank_cfg_t)) == NULL) {
        qdf_print("%s: Failed to expand head for HTT_H2T_MSG_TYPE_FRAG_DESC_BANK_CFG msg", __func__);
        return A_ERROR; /* failure */
    }

    /* fill in the message contents */
    msg_word = (u_int32_t *) qdf_nbuf_data(msg);

    memset(msg_word, 0 , sizeof(struct htt_tx_frag_desc_bank_cfg_t));
    /* rewind beyond alignment pad to get to the HTC header reserved area */
    qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_FRAG_DESC_BANK_CFG);

    bank_cfg = (struct htt_tx_frag_desc_bank_cfg_t *)msg_word;

    /** @note @todo Hard coded to 0 Assuming just one pdev for now.*/
    HTT_H2T_FRAG_DESC_BANK_PDEVID_SET(*msg_word, 0);
    /** @note Hard coded to 1.*/
    HTT_H2T_FRAG_DESC_BANK_NUM_BANKS_SET(*msg_word, 1);
    HTT_H2T_FRAG_DESC_BANK_DESC_SIZE_SET(*msg_word, pdev->frag_descs.size);
#if BIG_ENDIAN_HOST && !AH_NEED_TX_DATA_SWAP
    HTT_H2T_FRAG_DESC_BANK_SWAP_SET(*msg_word, 1);
#else
    HTT_H2T_FRAG_DESC_BANK_SWAP_SET(*msg_word, 0);
#endif

    /** Bank specific data structure.*/
#if QCA_PARTNER_DIRECTLINK_TX
    if (CE_is_directlink(((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->ce_tx_hdl)) {
        bank_cfg->bank_base_address[0] = wlan_pltfrm_directlink_get_bank_base();
    } else {
        bank_cfg->bank_base_address[0] = pdev->frag_descs.pool_paddr;
    }
#else /* QCA_PARTNER_DIRECTLINK_TX */
    bank_cfg->bank_base_address[0] = pdev->frag_descs.pool_paddr;
#endif /* QCA_PARTNER_DIRECTLINK_TX */

#if PEER_FLOW_CONTROL
    HTT_H2T_FRAG_DESC_HOST_Q_STATE_VALID_SET(*msg_word, 1);
    bank_cfg->host_q_state_buf_base_addr = pdev->host_q_mem.pool_paddr;
    HTT_H2T_FRAG_DESC_HOST_Q_STATE_MAX_PEER_ID_SET(*(msg_word + 10), OL_TXRX_MAX_PEER_IDS);
    HTT_H2T_FRAG_DESC_HOST_Q_STATE_MAX_TID_SET(*(msg_word + 10), OL_TX_PFLOW_CTRL_MAX_TIDS);
    HTT_H2T_FRAG_DESC_HOST_Q_STATE_QDEPTH_SIZE_BYTES_SET(*(msg_word + 11), OL_TX_PFLOW_CTRL_MAX_QUEUE_LEN);
    HTT_H2T_FRAG_DESC_HOST_Q_STATE_QDEPTH_CNT_MULTIPLE_SET(*(msg_word + 11), OL_TX_PKT_CNT_MULTIPLE);
#endif
    /* Logical Min index */
    HTT_H2T_FRAG_DESC_BANK_MIN_IDX_SET(bank_cfg->bank_info[0], 0);
    /* Logical Max index */
    HTT_H2T_FRAG_DESC_BANK_MAX_IDX_SET(bank_cfg->bank_info[0], pdev->frag_descs.pool_elems-1);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (pdev->nss_wifiol_ctx) {
        if (osif_ol_nss_htc_send(pdev->txrx_pdev, pdev->nss_wifiol_ctx, pdev->nss_wifiol_id, msg, pdev->htc_endpoint))
            return A_ERROR;
    } else
#endif
    {
        pkt = htt_htc_pkt_alloc(pdev);
        if (!pkt) {
            qdf_nbuf_free(msg);
            return A_ERROR; /* failure */
        }
        /* show that this is not a tx frame download (not required, but helpful) */
        pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
        pkt->pdev_ctxt = NULL; /* not used during send-done callback */

        SET_HTC_PACKET_INFO_TX(
            &pkt->htc_pkt,
            htt_h2t_send_complete_free_netbuf,
            qdf_nbuf_data(msg),
            qdf_nbuf_len(msg),
            pdev->htc_endpoint,
            1); /* tag - not relevant here */

        SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

        if(htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt) == QDF_STATUS_SUCCESS)
	    htt_htc_misc_pkt_list_add(pdev, pkt);

#if PEER_FLOW_CONTROL
        /* cache flush to avoid early htt fetch */
        OS_SYNC_SINGLE((osdev_t)qdf_dev->drv, pdev->host_q_mem.pool_paddr,
                pdev->host_q_mem.size, BUS_DMA_TODEVICE, (dma_context_t)&paddr_lo);
#endif
    }
    qdf_info("HTT_H2T_MSG_TYPE_FRAG_DESC_BANK_CFG sent to FW for SoC ID = %d",
             wlan_ucfg_get_config_param((void *)pdev->ctrl_psoc, SOC_ID));
    return A_OK;
}

A_STATUS
htt_h2t_ver_req_msg(struct htt_pdev_t *pdev)
{
    struct htt_htc_pkt *pkt;
    qdf_nbuf_t msg;
    u_int32_t *msg_word;

    msg = qdf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_VER_REQ_BYTES),
        /* reserve room for the HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, TRUE);
    if (!msg) {
        return A_ERROR; /* failure */
    }

    /*
     * Set the length of the message.
     * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
     * separately during the below call to qdf_nbuf_push_head.
     * The contribution from the HTC header is added separately inside HTC.
     */
    if (qdf_nbuf_put_tail(msg, HTT_VER_REQ_BYTES) == NULL) {
        qdf_print("%s: Failed to expand head for HTT_H2T_MSG_TYPE_VERSION_REQ msg", __func__);
        return A_ERROR; /* failure */
    }

    /* fill in the message contents */
    msg_word = (u_int32_t *) qdf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_VERSION_REQ);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (pdev->nss_wifiol_ctx) {
        if (osif_ol_nss_htc_send(pdev->txrx_pdev, pdev->nss_wifiol_ctx, pdev->nss_wifiol_id, msg, pdev->htc_endpoint))
            return A_ERROR;
    } else
#endif
    {
        pkt = htt_htc_pkt_alloc(pdev);
        if (!pkt) {
            qdf_nbuf_free(msg);
            return A_ERROR; /* failure */
        }
        /* show that this is not a tx frame download (not required, but helpful) */
        pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
        pkt->pdev_ctxt = NULL; /* not used during send-done callback */

        SET_HTC_PACKET_INFO_TX(
            &pkt->htc_pkt,
            htt_h2t_send_complete_free_netbuf,
            qdf_nbuf_data(msg),
            qdf_nbuf_len(msg),
            pdev->htc_endpoint,
            1); /* tag - not relevant here */

        SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
        if (htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt) == QDF_STATUS_SUCCESS)
	    htt_htc_misc_pkt_list_add(pdev, pkt);
    }
    return A_OK;
}

A_STATUS
htt_h2t_rx_ring_cfg_msg_ll(struct htt_pdev_t *pdev)
{
    struct htt_htc_pkt *pkt;
    qdf_nbuf_t msg;
    u_int32_t *msg_word;
    int enable_ctrl_data, enable_mgmt_data,
        enable_null_data, enable_phy_data,
        enable_ppdu_start, enable_ppdu_end;
    struct ar_rx_desc_ops *ar_rx_ops = pdev->ar_rx_ops;

    msg = qdf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_RX_RING_CFG_BYTES(1)),
        /* reserve room for the HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, TRUE);
    if (!msg) {
        return A_ERROR; /* failure */
    }
    /*
     * Set the length of the message.
     * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
     * separately during the below call to qdf_nbuf_push_head.
     * The contribution from the HTC header is added separately inside HTC.
     */
    if (qdf_nbuf_put_tail(msg, HTT_RX_RING_CFG_BYTES(1)) == NULL) {
        qdf_print("%s: Failed to expand head for HTT_H2T_MSG_TYPE_RX_RING_CFG msg", __func__);
        return A_ERROR; /* failure */
    }

    /* fill in the message contents */
    msg_word = (u_int32_t *) qdf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_RX_RING_CFG);
    HTT_RX_RING_CFG_NUM_RINGS_SET(*msg_word, 1);

    msg_word++;
    *msg_word = 0;

#if QCA_PARTNER_DIRECTLINK_RX
    /*
     * For Direct Link RX, need to set specific rx ring index address
     * get from partner API.
     */
    if (CE_is_directlink(((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->ce_tx_hdl)) {
        HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_SET(
            *msg_word, (pdev->alloc_idx_ptr_partner & PADDR_MASK_PARTNER));
    } else {
        HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_SET(
            *msg_word, pdev->rx_ring.alloc_idx.paddr);
    }
#else /* QCA_PARTNER_DIRECTLINK_RX */
    HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_SET(
        *msg_word, pdev->rx_ring.alloc_idx.paddr);
#endif /* QCA_PARTNER_DIRECTLINK_RX */

    msg_word++;
    *msg_word = 0;

#if QCA_PARTNER_DIRECTLINK_RX
    /*
     * For Direct Link RX, need to set specific rx ring base address
     * get from partner API.
     */
    if (CE_is_directlink(((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->ce_tx_hdl)) {
        HTT_RX_RING_CFG_BASE_PADDR_SET(*msg_word, pdev->rxpb_ring_base_partner);
    } else {
        HTT_RX_RING_CFG_BASE_PADDR_SET(*msg_word, pdev->rx_ring.base_paddr);
    }
#else /* QCA_PARTNER_DIRECTLINK_RX */
    HTT_RX_RING_CFG_BASE_PADDR_SET(*msg_word, pdev->rx_ring.base_paddr);
#endif /* QCA_PARTNER_DIRECTLINK_RX */

    msg_word++;
    *msg_word = 0;

#if QCA_PARTNER_DIRECTLINK_RX
    /*
     * For Direct Link RX, need to set specific rx ring size
     * get from partner API.
     */
    if (CE_is_directlink(((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->ce_tx_hdl)) {
        HTT_RX_RING_CFG_LEN_SET(*msg_word, pdev->t2h_rxpb_ring_sz_partner);
    } else {
        HTT_RX_RING_CFG_LEN_SET(*msg_word, pdev->rx_ring.size);
    }
#else /* QCA_PARTNER_DIRECTLINK_RX */
    HTT_RX_RING_CFG_LEN_SET(*msg_word, pdev->rx_ring.size);
#endif /* QCA_PARTNER_DIRECTLINK_RX */

    HTT_RX_RING_CFG_BUF_SZ_SET(*msg_word, HTT_RX_BUF_SIZE);

/* FIX THIS: if the FW creates a complete translated rx descriptor, then the MAC DMA of the HW rx descriptor should be disabled. */
    msg_word++;
    *msg_word = 0;
#ifndef REMOVE_PKT_LOG
   /*If Packet log is enable (pl_dev handle is valid) inform FW to start enable pktlog*/
   if(pdev->txrx_pdev && ((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->pl_dev) {
        enable_ctrl_data = 1;
        enable_mgmt_data = 1;
        enable_null_data = 1;
        enable_phy_data  = 1;
        enable_ppdu_start= 1;
        enable_ppdu_end  = 1;
    } else
#endif
    {
        enable_ctrl_data = 0;
        enable_mgmt_data = 0;
        enable_null_data = 0;
        enable_phy_data  = 0;
        enable_ppdu_start= 0;
        enable_ppdu_end  = 0;
    }

    HTT_RX_RING_CFG_ENABLED_802_11_HDR_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_PPDU_START_SET(*msg_word, enable_ppdu_start);
    HTT_RX_RING_CFG_ENABLED_PPDU_END_SET(*msg_word, enable_ppdu_end);
    HTT_RX_RING_CFG_ENABLED_MPDU_START_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_MPDU_END_SET(*msg_word,   1);
    HTT_RX_RING_CFG_ENABLED_MSDU_START_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_MSDU_END_SET(*msg_word,   1);
    HTT_RX_RING_CFG_ENABLED_RX_ATTN_SET(*msg_word,    1);
    HTT_RX_RING_CFG_ENABLED_FRAG_INFO_SET(*msg_word,  1); /* always present? */
    HTT_RX_RING_CFG_ENABLED_UCAST_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_MCAST_SET(*msg_word, 1);
    /* Must change to dynamic enable at run time
     * rather than at compile time
     */
    HTT_RX_RING_CFG_ENABLED_CTRL_SET(*msg_word, enable_ctrl_data);
    HTT_RX_RING_CFG_ENABLED_MGMT_SET(*msg_word, enable_mgmt_data);
    HTT_RX_RING_CFG_ENABLED_NULL_SET(*msg_word, enable_null_data);
    HTT_RX_RING_CFG_ENABLED_PHY_SET(*msg_word, enable_phy_data);

#if QCA_PARTNER_DIRECTLINK_RX
    /*
     * For Direct Link RX, need to set specific value in rx ring index[0]
     * get from partner API.
     */
    if (CE_is_directlink(((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->ce_tx_hdl)) {
        HTT_RX_RING_CFG_IDX_INIT_VAL_SET(*msg_word,
	        *((u_int32_t *)pdev->alloc_idx_ptr_partner));
    } else {
        HTT_RX_RING_CFG_IDX_INIT_VAL_SET(*msg_word,
                *pdev->rx_ring.alloc_idx.vaddr);
    }
#else
    HTT_RX_RING_CFG_IDX_INIT_VAL_SET(*msg_word,
            *pdev->rx_ring.alloc_idx.vaddr);
#endif

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_802_11_HDR_SET(*msg_word,
            ar_rx_ops->offsetof_hdr_status() >> 2);
    HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_SET(*msg_word,
            HTT_RX_STD_DESC_RESERVATION_DWORD);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_PPDU_START_SET(*msg_word,
            ar_rx_ops->offsetof_ppdu_start() >> 2);
    HTT_RX_RING_CFG_OFFSET_PPDU_END_SET(*msg_word,
            ar_rx_ops->offsetof_ppdu_end() >> 2);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_MPDU_START_SET(*msg_word,
            ar_rx_ops->offsetof_mpdu_start() >> 2);
    HTT_RX_RING_CFG_OFFSET_MPDU_END_SET(*msg_word,
            ar_rx_ops->offsetof_mpdu_end() >> 2);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_MSDU_START_SET(*msg_word,
            ar_rx_ops->offsetof_msdu_start() >> 2);
    HTT_RX_RING_CFG_OFFSET_MSDU_END_SET(*msg_word,
            ar_rx_ops->offsetof_msdu_end() >> 2);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_RX_ATTN_SET(*msg_word,
            ar_rx_ops->offsetof_attention() >> 2);
    HTT_RX_RING_CFG_OFFSET_FRAG_INFO_SET(*msg_word,
            ar_rx_ops->offsetof_frag_info() >> 2);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (pdev->txrx_pdev && pdev->nss_wifiol_ctx) {
        if (osif_ol_nss_htc_send(pdev->txrx_pdev, pdev->nss_wifiol_ctx, pdev->nss_wifiol_id, msg, pdev->htc_endpoint))
            return A_ERROR;
    } else
#endif
    {
        pkt = htt_htc_pkt_alloc(pdev);
        if (!pkt) {
            qdf_nbuf_free(msg);
            return A_ERROR; /* failure */
        }
        /* show that this is not a tx frame download (not required, but helpful) */
        pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
        pkt->pdev_ctxt = NULL; /* not used during send-done callback */

        SET_HTC_PACKET_INFO_TX(
            &pkt->htc_pkt,
            htt_h2t_send_complete_free_netbuf,
            qdf_nbuf_data(msg),
            qdf_nbuf_len(msg),
            pdev->htc_endpoint,
            1); /* tag - not relevant here */

        SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
        if(htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt) == QDF_STATUS_SUCCESS)
	    htt_htc_misc_pkt_list_add(pdev, pkt);
    }
    return A_OK;
}

A_STATUS
htt_h2t_rx_ring_cfg_msg_hl(struct htt_pdev_t *pdev)
{
    struct htt_htc_pkt *pkt;
    qdf_nbuf_t msg;
    u_int32_t *msg_word;

    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return A_ERROR; /* failure */
    }

    /* show that this is not a tx frame download (not required, but helpful) */
    pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
    pkt->pdev_ctxt = NULL; /* not used during send-done callback */

    msg = qdf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_RX_RING_CFG_BYTES(1)),
        /* reserve room for the HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, TRUE);
    if (!msg) {
        htt_htc_pkt_free(pdev, pkt);
        return A_ERROR; /* failure */
    }
    /*
     * Set the length of the message.
     * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
     * separately during the below call to qdf_nbuf_push_head.
     * The contribution from the HTC header is added separately inside HTC.
     */
    if (qdf_nbuf_put_tail(msg, HTT_RX_RING_CFG_BYTES(1)) == NULL) {
        qdf_print("%s: Failed to expand head for HTT_H2T_MSG_TYPE_RX_RING_CFG msg", __func__);
        htt_htc_pkt_free(pdev, pkt);
        return A_ERROR; /* failure */
    }

    /* fill in the message contents */
    msg_word = (u_int32_t *) qdf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_RX_RING_CFG);
    HTT_RX_RING_CFG_NUM_RINGS_SET(*msg_word, 1);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_SET(
        *msg_word, pdev->rx_ring.alloc_idx.paddr);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_BASE_PADDR_SET(*msg_word, pdev->rx_ring.base_paddr);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_LEN_SET(*msg_word, pdev->rx_ring.size);
    HTT_RX_RING_CFG_BUF_SZ_SET(*msg_word, HTT_RX_BUF_SIZE);

/* FIX THIS: if the FW creates a complete translated rx descriptor, then the MAC DMA of the HW rx descriptor should be disabled. */
    msg_word++;
    *msg_word = 0;

    HTT_RX_RING_CFG_ENABLED_802_11_HDR_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_PPDU_START_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_PPDU_END_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_MPDU_START_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_MPDU_END_SET(*msg_word,   0);
    HTT_RX_RING_CFG_ENABLED_MSDU_START_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_MSDU_END_SET(*msg_word,   0);
    HTT_RX_RING_CFG_ENABLED_RX_ATTN_SET(*msg_word,    0);
    HTT_RX_RING_CFG_ENABLED_FRAG_INFO_SET(*msg_word,  0); /* always present? */
    HTT_RX_RING_CFG_ENABLED_UCAST_SET(*msg_word, 1);
    HTT_RX_RING_CFG_ENABLED_MCAST_SET(*msg_word, 1);
    /* Must change to dynamic enable at run time
     * rather than at compile time
     */
    HTT_RX_RING_CFG_ENABLED_CTRL_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_MGMT_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_NULL_SET(*msg_word, 0);
    HTT_RX_RING_CFG_ENABLED_PHY_SET(*msg_word, 0);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_802_11_HDR_SET(*msg_word,
            0);
    HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_SET(*msg_word,
            0);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_PPDU_START_SET(*msg_word,
            0);
    HTT_RX_RING_CFG_OFFSET_PPDU_END_SET(*msg_word,
            0);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_MPDU_START_SET(*msg_word,
            0);
    HTT_RX_RING_CFG_OFFSET_MPDU_END_SET(*msg_word,
            0);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_MSDU_START_SET(*msg_word,
            0);
    HTT_RX_RING_CFG_OFFSET_MSDU_END_SET(*msg_word,
            0);

    msg_word++;
    *msg_word = 0;
    HTT_RX_RING_CFG_OFFSET_RX_ATTN_SET(*msg_word,
            0);
    HTT_RX_RING_CFG_OFFSET_FRAG_INFO_SET(*msg_word,
            0);

    SET_HTC_PACKET_INFO_TX(
        &pkt->htc_pkt,
        htt_h2t_send_complete_free_netbuf,
        qdf_nbuf_data(msg),
        qdf_nbuf_len(msg),
        pdev->htc_endpoint,
        1); /* tag - not relevant here */

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

    htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt);

    return A_OK;
}

int
htt_h2t_dbg_stats_get(
    struct htt_pdev_t *pdev,
    u_int32_t stats_type_upload_mask,
    u_int32_t stats_type_reset_mask,
    u_int8_t cfg_stat_type,
    u_int32_t cfg_val,
    u_int64_t cookie,
    u_int32_t vdev_id)
{
    qdf_nbuf_t msg;
    u_int32_t *msg_word;
    HTC_FRAME_HDR       *pHtcHdr;
    struct ol_txrx_pdev_t *txrx_pdev = (struct ol_txrx_pdev_t *)pdev->txrx_pdev;
    int status;

    if (stats_type_upload_mask >= 1 << HTT_DBG_NUM_STATS ||
        stats_type_reset_mask >= 1 << HTT_DBG_NUM_STATS)
    {
        /* FIX THIS - add more details? */
        qdf_print("%#x %#x stats not supported",
            stats_type_upload_mask, stats_type_reset_mask);
        return -1; /* failure */
    }

    msg = qdf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_H2T_STATS_REQ_MSG_EXTN1_SZ),
        /* reserve room for HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, FALSE);

    if(!msg) {
        return -1;
    }
    /* set the length of the message */
    if (qdf_nbuf_put_tail(msg, HTT_H2T_STATS_REQ_MSG_EXTN1_SZ) == NULL) {
        qdf_print("%s: Failed to expand head for HTT_H2T_MSG_TYPE_STATS_REQ msg", __func__);
        return -1;
    }

    /* fill in the message contents */
    msg_word = (u_int32_t *) qdf_nbuf_data(msg);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_STATS_REQ);
    HTT_H2T_STATS_REQ_UPLOAD_TYPES_SET(*msg_word, stats_type_upload_mask);

    msg_word++;
    *msg_word = 0;
    /* set the reserved field of reset mask to specify extended stats request */
    if (vdev_id){
        HTT_H2T_STATS_HEADER_EXTN_FLG_SET(*msg_word, PER_VDEV_FW_STATS_REQUEST);
    } else {
        HTT_H2T_STATS_HEADER_EXTN_FLG_SET(*msg_word, PER_RADIO_FW_STATS_REQUEST);
    }
    HTT_H2T_STATS_REQ_RESET_TYPES_SET(*msg_word, stats_type_reset_mask);

    msg_word++;
    *msg_word = 0;
    HTT_H2T_STATS_REQ_CFG_VAL_SET(*msg_word, cfg_val);
    HTT_H2T_STATS_REQ_CFG_STAT_TYPE_SET(*msg_word, cfg_stat_type);

    /* cookie LSBs */
    msg_word++;
    *msg_word = cookie & 0xffffffff;

    /* cookie MSBs */
    msg_word++;
    *msg_word = cookie >> 32;

   /* vap id for which stats has been requested + 1 */
    msg_word++;
    *msg_word = vdev_id;

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING + HTC_HEADER_LEN);
    pHtcHdr = (HTC_FRAME_HDR *) qdf_nbuf_get_frag_vaddr(msg, 0);
    HTC_WRITE32(pHtcHdr, SM(qdf_nbuf_len(msg), HTC_FRAME_HDR_PAYLOADLEN) |
                         SM(pdev->htc_endpoint, HTC_FRAME_HDR_ENDPOINTID));
    HTC_WRITE32(((A_UINT32 *)pHtcHdr) + 1, SM(1,
                        HTC_FRAME_HDR_CONTROLBYTES1));

#ifdef BIG_ENDIAN_HOST
    {
        /* Big endian host swap the HTC and HTT header */
        int i;
        u_int32_t *destp, *srcp, length;
        length = HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING +
                                  HTT_H2T_STATS_REQ_MSG_EXTN1_SZ;
        length =length/4 ;
        destp = srcp = (u_int32_t *)pHtcHdr;
        for(i=0; i < length ; i++) {
            *destp = cpu_to_le32(*srcp);
            destp++; srcp++;
        }
    }
#endif

    qdf_nbuf_map_single( pdev->osdev, (qdf_nbuf_t) msg , QDF_DMA_TO_DEVICE);
    htt_pkt_buf_list_add(pdev,(void *)msg,cookie, NULL);

    HTT_TX_PDEV_LOCK(&txrx_pdev->tx_lock);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (pdev->nss_wifiol_ctx) {
        status = osif_nss_ol_stats_frame_send((ol_txrx_pdev_handle) txrx_pdev, pdev->nss_wifiol_ctx, pdev->nss_wifiol_id, msg);
    } else
#endif
    {
        status = hif_send_single(pdev->osc, msg,
                                 pdev->htc_endpoint, qdf_nbuf_len(msg));
    }
    HTT_TX_PDEV_UNLOCK(&txrx_pdev->tx_lock);
    if(status){
        htt_pkt_buf_list_del(pdev, cookie);
        return -1;
    }

    return 0;
}

A_STATUS
htt_h2t_sync_msg(struct htt_pdev_t *pdev, u_int8_t sync_cnt)
{
    struct htt_htc_pkt *pkt;
    qdf_nbuf_t msg;
    u_int32_t *msg_word;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (pdev->nss_wifiol_ctx) {
        printk("NSS WiFi Offload :sending sync message not supported \n");
        return A_NO_MEMORY;
    }
#endif


    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return A_NO_MEMORY;
    }

    /* show that this is not a tx frame download (not required, but helpful) */
    pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
    pkt->pdev_ctxt = NULL; /* not used during send-done callback */

    msg = qdf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_H2T_SYNC_MSG_SZ),
        /* reserve room for HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, FALSE);
    if (!msg) {
        htt_htc_pkt_free(pdev, pkt);
        return A_NO_MEMORY;
    }
    /* set the length of the message */
    if (qdf_nbuf_put_tail(msg, HTT_H2T_SYNC_MSG_SZ) == NULL) {
        qdf_print("%s: Failed to expand head for HTT_H2T_MSG_TYPE_SYNC msg", __func__);
        htt_htc_pkt_free(pdev, pkt);
        return A_NO_MEMORY;
    }

    /* fill in the message contents */
    msg_word = (u_int32_t *) qdf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_SYNC);
    HTT_H2T_SYNC_COUNT_SET(*msg_word, sync_cnt);

    SET_HTC_PACKET_INFO_TX(
        &pkt->htc_pkt,
        htt_h2t_send_complete_free_netbuf,
        qdf_nbuf_data(msg),
        qdf_nbuf_len(msg),
        pdev->htc_endpoint,
        1); /* tag - not relevant here */

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
    htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt);

    return A_OK;
}

int
htt_h2t_aggr_cfg_msg(struct htt_pdev_t *pdev,
                     int max_subfrms_ampdu,
                     int max_subfrms_amsdu)
{
    qdf_nbuf_t msg;
    u_int32_t *msg_word;
    u_int32_t cookie ;
    HTC_FRAME_HDR       *pHtcHdr;
    struct ol_txrx_pdev_t *txrx_pdev = (struct ol_txrx_pdev_t *)pdev->txrx_pdev;
    int status;

    msg = qdf_nbuf_alloc(
        pdev->osdev,
        HTT_MSG_BUF_SIZE(HTT_AGGR_CFG_MSG_SZ),
        /* reserve room for HTC header */
        HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, FALSE);
    if(!msg) {
        return -1;
    }

    /* set the length of the message */
    if (qdf_nbuf_put_tail(msg, HTT_AGGR_CFG_MSG_SZ) == NULL) {
        qdf_print("%s: Failed to expand head for HTT_H2T_MSG_TYPE_AGGR_CFG msg", __func__);
        return -1;
    }

    /* fill in the message contents */
    msg_word = (u_int32_t *) qdf_nbuf_data(msg);

    *msg_word = 0;
    HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_AGGR_CFG);

    if (max_subfrms_ampdu && (max_subfrms_ampdu <= 64)) {
        HTT_AGGR_CFG_MAX_NUM_AMPDU_SUBFRM_SET(*msg_word, max_subfrms_ampdu);
    }

    if (max_subfrms_amsdu && (max_subfrms_amsdu < 32)) {
        HTT_AGGR_CFG_MAX_NUM_AMSDU_SUBFRM_SET(*msg_word, max_subfrms_amsdu);
    }
    cookie = (u_int32_t)(uintptr_t)(msg);
    msg_word++;
    *msg_word = cookie ;

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (pdev->nss_wifiol_ctx) {
        if (osif_ol_nss_htc_send(pdev->txrx_pdev, pdev->nss_wifiol_ctx, pdev->nss_wifiol_id, msg, pdev->htc_endpoint))
            return -1;
    } else
#endif
    {
        /* rewind beyond alignment pad to get to the HTC header reserved area */
        qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING + HTC_HEADER_LEN);
        pHtcHdr = (HTC_FRAME_HDR *) qdf_nbuf_get_frag_vaddr(msg, 0);
        HTC_WRITE32(pHtcHdr, SM(qdf_nbuf_len(msg), HTC_FRAME_HDR_PAYLOADLEN) |
                SM(pdev->htc_endpoint, HTC_FRAME_HDR_ENDPOINTID));
        HTC_WRITE32(((A_UINT32 *)pHtcHdr) + 1, SM(1,
                    HTC_FRAME_HDR_CONTROLBYTES1));

#ifdef BIG_ENDIAN_HOST
        {
            /* Big endian host swap the HTC and HTT header */
            int i;
            u_int32_t *destp, *srcp, length;
            length = HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING +
                HTT_AGGR_CFG_MSG_SZ;
            length =length/4 ;
            destp = srcp = (u_int32_t *)pHtcHdr;
            for(i=0; i < length ; i++) {
                *destp = cpu_to_le32(*srcp);
                destp++; srcp++;
            }
        }
#endif

        qdf_nbuf_map_single( pdev->osdev, (qdf_nbuf_t) msg , QDF_DMA_TO_DEVICE);
        htt_pkt_buf_list_add(pdev,(void *)msg,cookie, NULL);
        HTT_TX_PDEV_LOCK(&txrx_pdev->tx_lock);
        status = hif_send_single(pdev->osc, msg, pdev->htc_endpoint,
                qdf_nbuf_len(msg));
        HTT_TX_PDEV_UNLOCK(&txrx_pdev->tx_lock);

        if(status){
            htt_pkt_buf_list_del(pdev, cookie);
            return -1;
        }
    }
    return 0;
}


int
htt_h2t_mgmt_tx(struct ol_txrx_pdev_t *pdev,
        A_UINT32             paddr,
        qdf_nbuf_t           mgmt_frm,
        A_UINT32             frag_len,
        A_UINT16             vdev_id,
        A_UINT8              *hdr,
        ol_txrx_vdev_handle  vdev_handle)
{
    qdf_nbuf_t msg;
    A_UINT32   desc_id;
    int subtype;
    int status;
    HTC_FRAME_HDR       *pHtcHdr;
    struct ieee80211_frame *wh;
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct htt_mgmt_tx_desc_t *tx_mgmt_desc;
    struct htt_pdev_t *hpdev = pdev->htt_pdev;
    A_UINT8 tid, retry, power, subtype_index;
    A_UINT8 nss, preamble, mcs;
    A_UINT32 *tx_params, rate;

    /* If mgmt_frm is NULL, return with error */
    if(mgmt_frm == NULL) {
        return -1;
    }

    wh = (struct ieee80211_frame *)wbuf_header(mgmt_frm);
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

    subtype_index = (subtype >> IEEE80211_FC0_SUBTYPE_SHIFT);

    msg = htt_tx_mgmt_desc_alloc(hpdev, vdev_id, &desc_id, mgmt_frm);

    /* If msg is descriptor is NULL, return */
    if (!msg) {
        qdf_nbuf_unmap_single(pdev->osdev, mgmt_frm, QDF_DMA_TO_DEVICE);
        return -1;
    }

    /* fill in the message contents */
    tx_mgmt_desc = (struct htt_mgmt_tx_desc_t *) qdf_nbuf_data(msg);

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING + HTC_HEADER_LEN);


    pHtcHdr = (HTC_FRAME_HDR *) qdf_nbuf_get_frag_vaddr(msg, 0);
    HTC_WRITE32(pHtcHdr, SM(qdf_nbuf_len(msg), HTC_FRAME_HDR_PAYLOADLEN) |
            SM(hpdev->htc_endpoint, HTC_FRAME_HDR_ENDPOINTID));

    HTC_WRITE32(((A_UINT32 *)pHtcHdr) + 1, SM(1,
                HTC_FRAME_HDR_CONTROLBYTES1));

    tx_mgmt_desc->msg_type = HTT_H2T_MSG_TYPE_MGMT_TX;
    tx_mgmt_desc->frag_paddr  = paddr;
    tx_mgmt_desc->desc_id     = desc_id;
    tx_mgmt_desc->len         = frag_len;
    tx_mgmt_desc->vdev_id     = vdev_id;

    /* located tx_desc_paramters's offset*/
    tx_params = (A_UINT32 *)tx_mgmt_desc->hdr;
    tx_params = (A_UINT32 *)(tx_params + (HTT_MGMT_FRM_HDR_DOWNLOAD_LEN / 4));
    OS_MEMZERO(tx_params, sizeof(*tx_params));

    tid = wbuf_get_tid(mgmt_frm);
    if (tid == HTT_TX_EXT_TID_NONPAUSE) {
        HTT_MGMT_TX_DESC_PARAMS_EXTENSION_SET(*tx_params, 1);
        HTT_MGMT_TX_DESC_PARAMS_VALID_MASK_SET(*tx_params, 0x3F);
        HTT_MGMT_TX_DESC_PARAMS_BW_SET(*tx_params, 0);
        HTT_MGMT_TX_DESC_PARAMS_ENCRYPTION_SET_SET(*tx_params, 1);

        wbuf_get_tx_ctrl(mgmt_frm, &retry, &power, NULL);
        wbuf_get_tx_rate(mgmt_frm, &rate);

        preamble = HTT_GET_HW_RATECODE_PREAM(rate);
        nss = HTT_GET_HW_RATECODE_NSS(rate);
        mcs = HTT_GET_HW_RATECODE_RATE(rate);
        if ((preamble == HTT_HW_RATECODE_PREAM_OFDM) ||
                (preamble == HTT_HW_RATECODE_PREAM_CCK)) {
            nss = 0;
        }
        if (subtype_index <= (IEEE80211_FC0_SUBTYPE_DEAUTH >> IEEE80211_FC0_SUBTYPE_SHIFT)){
            power = (power <= vdev->txpow_mgt_frm[subtype_index])? power : vdev->txpow_mgt_frm[subtype_index];
        }
        if(power == 0){
            tx_mgmt_desc->tx_params_valid_mask &= ~HTT_MGMT_TX_DESC_PARAMS_VALID_POWER;
        }

        if(retry == 0){
            retry = 1;
        }

        HTT_MGMT_TX_DESC_PARAMS_POWER_SET(*tx_params, power&0xFF);
        HTT_MGMT_TX_DESC_PARAMS_NUM_RETRIES_SET(*tx_params, retry&0x0F);
        HTT_MGMT_TX_DESC_PARAMS_MCS_SET(*tx_params, mcs);
        HTT_MGMT_TX_DESC_PARAMS_NSS_SET(*tx_params, nss);
        HTT_MGMT_TX_DESC_PARAMS_PREAM_TYPE_SET(*tx_params, preamble);
    } else {
        HTT_MGMT_TX_DESC_PARAMS_EXTENSION_SET(*tx_params, 0);
        HTT_MGMT_TX_DESC_PARAMS_VALID_MASK_SET(*tx_params, 0);
        HTT_MGMT_TX_DESC_PARAMS_BW_SET(*tx_params, 0);
        HTT_MGMT_TX_DESC_PARAMS_ENCRYPTION_SET_SET(*tx_params, 0);
        HTT_MGMT_TX_DESC_PARAMS_POWER_SET(*tx_params, 0);
        HTT_MGMT_TX_DESC_PARAMS_NUM_RETRIES_SET(*tx_params, 0);
        HTT_MGMT_TX_DESC_PARAMS_MCS_SET(*tx_params, 0);
        HTT_MGMT_TX_DESC_PARAMS_NSS_SET(*tx_params, 0);
        HTT_MGMT_TX_DESC_PARAMS_PREAM_TYPE_SET(*tx_params, 0);
        HTT_MGMT_TX_DESC_PARAMS_DYN_BW_SET(*tx_params, 0);
        HTT_MGMT_TX_DESC_PARAMS_FRAME_TYPE_SET(*tx_params, 0);
        if (subtype_index <= (IEEE80211_FC0_SUBTYPE_DEAUTH >> IEEE80211_FC0_SUBTYPE_SHIFT)) {
            power = vdev->txpow_mgt_frm[subtype_index];
            if (power != 0xFF) {
                HTT_MGMT_TX_DESC_PARAMS_POWER_SET(*tx_params, power&0xFF);
                HTT_MGMT_TX_DESC_PARAMS_EXTENSION_SET(*tx_params, 1);
                HTT_MGMT_TX_DESC_PARAMS_VALID_MASK_SET(*tx_params, 0x01);
            }
        }
    }
    OS_MEMCPY(tx_mgmt_desc->hdr, hdr, HTT_MGMT_FRM_HDR_DOWNLOAD_LEN);

#ifdef BIG_ENDIAN_HOST
    {
        /*
         * Big endian host swap the entire message except
         * the frame header.
         * The simple approach is to swap the entire message
         * and then do a 2nd swap on the header to restore it
         * to network byte order.
         * This is slightly inefficient, but since management
         * frame download is an infrequent event, the inefficiency
         * doesn.t matter.
         */
        int i;
        u_int32_t *destp, *srcp, length;

        /* first swap the whole message */
        length = sizeof(struct htt_mgmt_tx_desc_t) + HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING ;
        length =length/4;
        destp = srcp = (u_int32_t *)pHtcHdr;
        for(i=0; i < length ; i++) {
            *destp = cpu_to_le32(*srcp);
            destp++; srcp++;
        }

        /* now swap the hdr part back */
        length = HTT_MGMT_FRM_HDR_DOWNLOAD_LEN;
        length = length/4;
        destp = srcp = (u_int32_t *)
            (((u_int8_t *) pHtcHdr) +
             HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING +
             offsetof(struct htt_mgmt_tx_desc_t, hdr));
        for(i=0; i < length ; i++) {
            *destp = cpu_to_le32(*srcp);
            destp++; srcp++;
        }
    }
#endif

    qdf_nbuf_map_single( pdev->osdev, (qdf_nbuf_t) msg , QDF_DMA_TO_DEVICE);

    HTT_TX_PDEV_LOCK(&pdev->tx_lock);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (pdev->nss_wifiol_ctx) {
        status = osif_nss_ol_mgmt_frame_send((ol_txrx_pdev_handle) pdev, pdev->nss_wifiol_ctx, pdev->nss_wifiol_id, desc_id, msg);
    } else {
        status = hif_send_single(hpdev->osc, msg, hpdev->htc_endpoint, qdf_nbuf_len(msg));
    }
#else
     status = hif_send_single(hpdev->osc, msg, hpdev->htc_endpoint, qdf_nbuf_len(msg));
#endif
    HTT_TX_PDEV_UNLOCK(&pdev->tx_lock);


     if(status) {
         htt_tx_mgmt_desc_free(hpdev, desc_id, IEEE80211_TX_ERROR);
     }

    return 0;
}



#if PEER_FLOW_CONTROL
/* Transmit Fetch response for HTT Fetch message from target */
struct htt_tx_fetch_resp_t *
htt_h2t_fetch_resp_prepare(struct ol_txrx_pdev_t *pdev,
                struct htt_tx_fetch_desc_buf *fetch_desc,
                A_UINT32             len)
{
    qdf_nbuf_t msg;
    struct htt_tx_fetch_resp_t *tx_fetch_resp_msg;

    /* If fetch _frm is NULL, return with error */
    if(len < 0 || fetch_desc == NULL) {
        qdf_print("\n Fail : fetch_desc %pK len %u", fetch_desc, len);
       return NULL;
    }

	msg = qdf_nbuf_alloc(pdev->osdev,
			HTT_MSG_BUF_SIZE(sizeof
				(struct htt_tx_fetch_resp_t) + len),
			/* reserve room for HTC header */
			HTC_HEADER_LEN +
			HTC_HDR_ALIGNMENT_PADDING ,
			4,
			TRUE);

	if(!msg) {
		qdf_print("\n fetch resp buf alloc Failed");
		/* let the caller free it */
		return NULL;
	}

    if (qdf_nbuf_put_tail(msg, (sizeof(struct htt_tx_fetch_resp_t) - 8)) == NULL) {
        qdf_print("%s: Failed to expand head for HTT_H2T_MSG_TYPE_TX_FETCH_RESP msg", __func__);
        return NULL;
    }

	qdf_mem_zero(msg->data, qdf_nbuf_len(msg));
	fetch_desc->msg_buf     = msg;

    /* fill in the message contents */
    tx_fetch_resp_msg = (struct htt_tx_fetch_resp_t *) qdf_nbuf_data(msg);

    tx_fetch_resp_msg->word0 = HTT_H2T_MSG_TYPE_TX_FETCH_RESP;
    HTT_H2T_TX_FETCH_RESP_ID_SET(tx_fetch_resp_msg->word0, fetch_desc->desc_id);

    return tx_fetch_resp_msg;
}

int
htt_h2t_fetch_resp_tx(struct ol_txrx_pdev_t *pdev,
        struct htt_tx_fetch_desc_buf *fetch_desc)
{
    qdf_nbuf_t msg = fetch_desc->msg_buf;
    int status = 0;
    struct htt_pdev_t *hpdev = (struct htt_pdev_t *)pdev->htt_pdev;
    HTC_FRAME_HDR       *pHtcHdr;
#ifdef BIG_ENDIAN_HOST
    uint32_t num_resp_records;
    struct htt_tx_fetch_resp_t *tx_fetch_resp_msg;

    tx_fetch_resp_msg = (struct htt_tx_fetch_resp_t *) qdf_nbuf_data(msg);
    num_resp_records = HTT_H2T_TX_FETCH_RESP_NUM_FETCH_RECORDS_GET(tx_fetch_resp_msg->word1);
#endif

    /* rewind beyond alignment pad to get to the HTC header reserved area */
    qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING + HTC_HEADER_LEN);

    pHtcHdr = (HTC_FRAME_HDR *) qdf_nbuf_get_frag_vaddr(msg, 0);
    HTC_WRITE32(pHtcHdr, SM(qdf_nbuf_len(msg), HTC_FRAME_HDR_PAYLOADLEN) |
                         SM(hpdev->htc_endpoint, HTC_FRAME_HDR_ENDPOINTID));

    HTC_WRITE32(((A_UINT32 *)pHtcHdr) + 1, SM(1,
                        HTC_FRAME_HDR_CONTROLBYTES1));



#ifdef BIG_ENDIAN_HOST
{
      /* Big endian host swap the HTC and HTT header */
    int i;
    u_int32_t *destp, *srcp, length;
    length = HTC_HEADER_LEN + sizeof( struct htt_tx_fetch_resp_t)-sizeof(tx_fetch_resp_msg->fetch_resp_record)+
                              num_resp_records*sizeof(tx_fetch_resp_msg->fetch_resp_record)+
                              HTC_HDR_ALIGNMENT_PADDING;
    length =length/4 ;
    destp = srcp = (u_int32_t *)pHtcHdr;
    for(i=0; i < length ; i++) {
        *destp = cpu_to_le32(*srcp);
        destp++; srcp++;
    }
}
#endif

    qdf_nbuf_map_single(pdev->osdev, (qdf_nbuf_t) msg , QDF_DMA_TO_DEVICE);

    HTT_TX_PDEV_LOCK(&pdev->tx_lock);
    status = hif_send_single(hpdev->osc, msg, hpdev->htc_endpoint, qdf_nbuf_len(msg));
    HTT_TX_PDEV_UNLOCK(&pdev->tx_lock);

    if(status) {
      htt_tx_fetch_desc_free(hpdev, fetch_desc->desc_id);
    }
    return status;
}
#endif

