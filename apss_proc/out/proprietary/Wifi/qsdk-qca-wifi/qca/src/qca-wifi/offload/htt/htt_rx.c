/*
 * Copyright (c) 2011-2015,2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file htt_rx.c
 * @brief Implement receive aspects of HTT.
 * @details
 *  This file contains three categories of HTT rx code:
 *  1.  An abstraction of the rx descriptor, to hide the
 *      differences between the HL vs. LL rx descriptor.
 *  2.  Functions for providing access to the (series of)
 *      rx descriptor(s) and rx frame(s) associated with
 *      an rx indication message.
 *  3.  Functions for setting up and using the MAC DMA
 *      rx ring (applies to LL only).
 */

#include <qdf_mem.h>   /* qdf_mem_malloc,free, etc. */
#include <qdf_types.h> /* qdf_print */
#include <qdf_nbuf.h>     /* qdf_nbuf_t, etc. */
#include <qdf_timer.h> /* qdf_timer_free */

#include <htt.h>          /* HTT_HL_RX_DESC_SIZE */
#include <ol_cfg.h>
#include <ol_htt_rx_api.h>
#include <ol_ratetable.h>
#include <htt_internal.h> /* HTT_ASSERT, htt_pdev_t, HTT_RX_BUF_SIZE */
#include <cdp_txrx_stats_struct.h>  /* ol_txrx_get_vdev_rx_decap_type, etc. */
#include <ol_txrx_api_internal.h>  /* ol_txrx_get_vdev_rx_decap_type, etc. */

#include <ieee80211.h>         /* ieee80211_frame, ieee80211_qoscntl */
#include <ieee80211_defines.h> /* ieee80211_rx_status */

#if RX_CHECKSUM_OFFLOAD
#include <ol_txrx_internal.h>
#endif /* RX_CHECKSUM_OFFLOAD */
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#include <osif_nss_wifiol_2_0_if.h>
#endif
#include <wlan_lmac_dispatcher.h>
#include <ol_if_athvar.h>

#if WIFI_MEM_MANAGER_SUPPORT
#include "mem_manager.h"
#endif

#define RSSI_COMB_OFFSET           4
#define RSSI_COMB_MASK             0x000000ff
#define L_SIG_OFFSET               5
#define L_SIG_RATE_MASK            0x00000007
#define L_SIG_RATE_SELECT_MASK     0X00000008
#define L_SIG_PREAMBLE_TYPE_OFFSET 24
#define HT_SIG_VHT_SIG_A_1_OFFSET  6
#define HT_SIG_VHT_SIG_A_1_MASK    0x00ffffff
#define HT_SIG_VHT_SIG_A_2_OFFSET  7
#define HT_SIG_VHT_SIG_A_2_MASK    0x00ffffff
#define SERVICE_RESERVED_OFFSET    9
#define SERVICE_MASK               0x0000ffff

 /* AR9888v1 WORKAROUND for EV#112367 */
/* FIX THIS - remove this WAR when the bug is fixed */
#define PEREGRINE_1_0_ZERO_LEN_PHY_ERR_WAR

/*--- setup / tear-down functions -------------------------------------------*/

#ifndef HTT_RX_RING_SIZE_MIN
#define HTT_RX_RING_SIZE_MIN 128  /* slightly larger than one large A-MPDU */
#endif

#ifndef HTT_RX_RING_SIZE_MAX
#define HTT_RX_RING_SIZE_MAX 2048 /* roughly 20 ms @ 1 Gbps of 1500B MSDUs */
#endif

#ifndef HTT_RX_AVG_FRM_BYTES
#define HTT_RX_AVG_FRM_BYTES 1000
#endif

#ifndef HTT_RX_HOST_LATENCY_MAX_MS
#define HTT_RX_HOST_LATENCY_MAX_MS 20 /* ms */ /* very conservative */
#endif

#ifndef HTT_RX_HOST_LATENCY_WORST_LIKELY_MS
#define HTT_RX_HOST_LATENCY_WORST_LIKELY_MS 10 /* ms */ /* conservative */
#endif

#ifndef HTT_RX_RING_REFILL_RETRY_TIME_MS
#define HTT_RX_RING_REFILL_RETRY_TIME_MS    50
#endif

#if QCA_PARTNER_DIRECTLINK_RX
#define QCA_PARTNER_DIRECTLINK_HTT_RX 1
#include "ath_carr_pltfrm.h"
#undef QCA_PARTNER_DIRECTLINK_HTT_RX
#endif
#if QCA_PARTNER_CBM_DIRECTPATH
extern qdf_nbuf_t qdf_nbuf_alloc_partner(struct htt_pdev_t *pdev, qdf_size_t size, int reserve, int align, int prio);
#endif

static int
CEIL_PWR2(int value)
{
    int log2;
    if (IS_PWR2(value)) {
        return value;
    }
    log2 = 0;
    while (value) {
        value >>= 1;
        log2++;
    }
    return (1 << log2);
}

u_int8_t htt_txrx_is_decap_type_raw(void *dev)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)dev;
    return ((vdev != NULL) &&
            (ol_txrx_get_vdev_rx_decap_type((struct cdp_vdev *)vdev) == htt_cmn_pkt_type_raw));
}

u_int32_t htt_rx_ind_fw_desc_bytes_get(rx_handle_t rxh, qdf_nbuf_t rx_ind_msg)
{
    u_int32_t *msg_word;
    u_int8_t *rx_ind_data;

    rx_ind_data = qdf_nbuf_data(rx_ind_msg);
    msg_word = (u_int32_t *)rx_ind_data;
    return HTT_RX_IND_FW_RX_DESC_BYTES_GET(*(msg_word + (HTT_RX_IND_HDR_SUFFIX_BYTE_OFFSET >> 2)));
}

u_int32_t htt_rx_buf_size(rx_handle_t rxh)
{
    return HTT_RX_BUF_SIZE;
}

u_int32_t htt_rx_desc_reservation(rx_handle_t rxh)
{
    htt_pdev_handle pdev = (htt_pdev_handle)rxh;

    return HTT_RX_STD_DESC_RESERVATION;
}

int htt_rx_ind_msdu_byte_offset(rx_handle_t rxh, int num_msdu_bytes, int *byte_offset)
{
    htt_pdev_handle pdev = (htt_pdev_handle)rxh;

    if(pdev->rx_ind_msdu_byte_idx < num_msdu_bytes)
    {
        *byte_offset = HTT_ENDIAN_BYTE_IDX_SWAP(
                HTT_RX_IND_FW_RX_DESC_BYTE_OFFSET +
                pdev->rx_ind_msdu_byte_idx);
        return 1;
    }
    else //failed
        return 0;
}

void htt_rx_ind_msdu_byte_idx_inc(rx_handle_t rxh)
{
    htt_pdev_handle pdev = (htt_pdev_handle)rxh;

    pdev->rx_ind_msdu_byte_idx += 1;
}

static inline int
htt_rx_msdu_first_msdu_flag_hl(htt_pdev_handle pdev, void *msdu_desc)
{
    return ((u_int8_t*)msdu_desc - sizeof(struct hl_htt_rx_ind_base))
        [HTT_ENDIAN_BYTE_IDX_SWAP(HTT_RX_IND_HL_FLAG_OFFSET)] & HTT_RX_IND_HL_FLAG_FIRST_MSDU ? 1:0;
}

int
htt_rx_msdu_rx_desc_size_hl(
	htt_pdev_handle pdev,
    void *msdu_desc
    )
{
	return ((u_int8_t*)(msdu_desc) - HTT_RX_IND_HL_BYTES)
        [HTT_ENDIAN_BYTE_IDX_SWAP(HTT_RX_IND_HL_RX_DESC_LEN_OFFSET)];
}

static int
htt_rx_ring_size(struct htt_pdev_t *pdev)
{
    int size;

    /*
     * It is expected that the host CPU will typically be able to service
     * the rx indication from one A-MPDU before the rx indication from
     * the subsequent A-MPDU happens, roughly 1-2 ms later.
     * However, the rx ring should be sized very conservatively, to
     * accomodate the worst reasonable delay before the host CPU services
     * a rx indication interrupt.
     * The rx ring need not be kept full of empty buffers.  In theory,
     * the htt host SW can dynamically track the low-water mark in the
     * rx ring, and dynamically adjust the level to which the rx ring
     * is filled with empty buffers, to dynamically meet the desired
     * low-water mark.
     * In contrast, it's difficult to resize the rx ring itself, once
     * it's in use.
     * Thus, the ring itself should be sized very conservatively, while
     * the degree to which the ring is filled with empty buffers should
     * be sized moderately conservatively.
     */
    size =
        ol_cfg_max_thruput_mbps(pdev->ctrl_psoc) *
        1000 /* 1e6 bps/mbps / 1e3 ms per sec = 1000 */ /
        (8 * HTT_RX_AVG_FRM_BYTES) *
        HTT_RX_HOST_LATENCY_MAX_MS;

    if (size < OL_CFG_RX_RING_SIZE_MIN) {
        size = OL_CFG_RX_RING_SIZE_MIN;
    }
    if (size > OL_CFG_RX_RING_SIZE_MAX) {
        size = OL_CFG_RX_RING_SIZE_MAX;
    }
    size = CEIL_PWR2(size);
    return size;
}

static int
htt_rx_ring_fill_level(struct htt_pdev_t *pdev)
{
    int size;

    size =
        ol_cfg_max_thruput_mbps(pdev->ctrl_psoc)  *
        1000 /* 1e6 bps/mbps / 1e3 ms per sec = 1000 */ /
        (8 * HTT_RX_AVG_FRM_BYTES) *
        HTT_RX_HOST_LATENCY_WORST_LIKELY_MS;
    /*
     * Make sure the fill level is at least 1 less than the ring size.
     * Leaving 1 element empty allows the SW to easily distinguish
     * between a full ring vs. an empty ring.
     */
    if (size >= pdev->rx_ring.size) {
        size = pdev->rx_ring.size - 1;
    }
    return size;
}

static void
htt_rx_ring_refill_retry(void *arg)
{
    htt_pdev_handle pdev = (htt_pdev_handle)arg;
    htt_rx_msdu_buff_replenish(pdev);
}

void
htt_rx_ring_fill_n(struct htt_pdev_t *pdev, int num)
{
    int idx;
    QDF_STATUS status;
    void *rx_desc;

    idx = *(pdev->rx_ring.alloc_idx.vaddr);
    while (num > 0) {
        u_int32_t paddr;
        qdf_nbuf_t rx_netbuf;
        int headroom;
#if QCA_PARTNER_CBM_DIRECTPATH
        rx_netbuf = qdf_nbuf_alloc_partner(pdev, HTT_RX_BUF_SIZE,0, 4, FALSE);
#else
        rx_netbuf = qdf_nbuf_alloc(pdev->osdev, HTT_RX_BUF_SIZE, 0, 4, FALSE);
#endif
        if (!rx_netbuf) {
            /*
             * Failed to fill it to the desired level -
             * we'll start a timer and try again next time.
             * As long as enough buffers are left in the ring for
             * another A-MPDU rx, no special recovery is needed.
             */
            qdf_timer_mod(&pdev->rx_ring.refill_retry_timer,
                               HTT_RX_RING_REFILL_RETRY_TIME_MS);
            goto fail;
        }

        /* Clear rx_desc attention word before posting to Rx ring */
        rx_desc = htt_rx_desc(rx_netbuf);

        pdev->ar_rx_ops->init_desc(rx_desc);

        /*
         * Adjust qdf_nbuf_data to point to the location in the buffer
         * where the rx descriptor will be filled in.
         */
        headroom = qdf_nbuf_data(rx_netbuf) - (u_int8_t *) rx_desc;
        qdf_nbuf_push_head(rx_netbuf, headroom);

        status = qdf_nbuf_map(pdev->osdev, rx_netbuf, QDF_DMA_FROM_DEVICE);
        if (status != QDF_STATUS_SUCCESS) {
            qdf_nbuf_free(rx_netbuf);
            goto fail;
        }
        paddr = qdf_nbuf_get_frag_paddr(rx_netbuf, 0);
        pdev->rx_ring.buf.netbufs_ring[idx] = rx_netbuf;
        pdev->rx_ring.buf.paddrs_ring[idx] = paddr;
        pdev->rx_ring.fill_cnt++;

        num--;
        idx++;
        idx &= pdev->rx_ring.size_mask;
    }

fail:
    /* Fix for MSDU done bit issue */
    qdf_mb();

    *(pdev->rx_ring.alloc_idx.vaddr) = idx;
    return;
}

unsigned
htt_rx_ring_elems(rx_handle_t rxh)
{
    htt_pdev_handle pdev = (htt_pdev_handle)rxh;
    return
        (*pdev->rx_ring.alloc_idx.vaddr - pdev->rx_ring.sw_rd_idx.msdu_payld) &
        pdev->rx_ring.size_mask;
}

void
htt_rx_detach(struct htt_pdev_t *pdev)
{
    int sw_rd_idx = pdev->rx_ring.sw_rd_idx.msdu_payld;
#if WIFI_MEM_MANAGER_SUPPORT
    uint32_t soc_idx = wlan_ucfg_get_config_param((void *)pdev->ctrl_psoc, SOC_ID);
#endif
#if !WIFI_MEM_MANAGER_SUPPORT
    qdf_device_t qdf_dev = wlan_psoc_get_qdf_dev((void *)pdev->ctrl_psoc);
#endif

    if (pdev->cfg.is_high_latency) {
        return;
    }

#if QCA_PARTNER_DIRECTLINK_RX
    /*
     * For Direct Link RX, rx ring buffers are created by partner API.
     * Also need to be freed by partner API.
     */
    if (CE_is_directlink(((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->ce_tx_hdl)) {
        htt_rx_detach_partner(pdev);
    } else
#endif /* QCA_PARTNER_DIRECTLINK_RX */
    {

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (!pdev->nss_wifiol_ctx)
#endif
        {
            while (sw_rd_idx != *(pdev->rx_ring.alloc_idx.vaddr)) {
                qdf_nbuf_unmap(
                        pdev->osdev, pdev->rx_ring.buf.netbufs_ring[sw_rd_idx],
                        QDF_DMA_FROM_DEVICE);
                qdf_nbuf_free(pdev->rx_ring.buf.netbufs_ring[sw_rd_idx]);
                sw_rd_idx++;
                sw_rd_idx &= pdev->rx_ring.size_mask;
            }
        } /* QCA_PARTNER_DIRECTLINK_RX */
    }

    qdf_timer_free(&pdev->rx_ring.refill_retry_timer);
#if WIFI_MEM_MANAGER_SUPPORT
    wifi_cmem_free(soc_idx, 0, CM_RX_ATTACH_PADDR_RING, (void *)pdev->rx_ring.buf.paddrs_ring);
    wifi_cmem_free(soc_idx, 0, CM_RX_ATTACH_ALLOC_IDX_VADDR, (void *)pdev->rx_ring.alloc_idx.vaddr);
#else
    qdf_mem_free_consistent(
        pdev->osdev,
        &(((struct pci_dev *)(qdf_dev->drv_hdl))->dev),
        sizeof(u_int32_t),
        pdev->rx_ring.alloc_idx.vaddr,
        pdev->rx_ring.alloc_idx.paddr,
        qdf_get_dma_mem_context((&pdev->rx_ring.alloc_idx), memctx));
    qdf_mem_free_consistent(
        pdev->osdev,
        &(((struct pci_dev *)(qdf_dev->drv_hdl))->dev),
        pdev->rx_ring.size * sizeof(u_int32_t),
        pdev->rx_ring.buf.paddrs_ring,
        pdev->rx_ring.base_paddr,
        qdf_get_dma_mem_context((&pdev->rx_ring.buf), memctx));
#endif
    qdf_mem_free(pdev->rx_ring.buf.netbufs_ring);
}

int
htt_rx_mpdu_desc_seq_num_hl(htt_pdev_handle pdev, void *mpdu_desc)
{
    if (pdev->rx_desc_size_hl) {
        return pdev->cur_seq_num_hl =
            HTT_WORD_GET(*(A_UINT32*)mpdu_desc,
                    HTT_HL_RX_DESC_MPDU_SEQ_NUM);
    } else {
        return pdev->cur_seq_num_hl;
    }
}

int
htt_rx_mpdu_desc_more_frags(htt_pdev_handle pdev, void *mpdu_desc)
{
HTT_ASSERT1(0); /* FILL IN HERE */
return 0;
}

/* FIX THIS: APPLIES TO LL ONLY */
void
htt_rx_mpdu_desc_pn_ll(
    htt_pdev_handle pdev,
    void *mpdu_desc,
    union htt_rx_pn_t *pn,
    int pn_len_bits)
{
    u_int32_t pn_words[4];

    pdev->ar_rx_ops->mpdu_desc_pn(mpdu_desc, &pn_words[0], pn_len_bits);
    switch(pn_len_bits)
    {
        case 24:
            pn->pn24 = pn_words[0];
            break;
        case 48:
             pn->pn48 = pn_words[0];
             pn->pn48 |= ((u_int64_t)pn_words[1] << 32);
            break;
        case 128:
             pn->pn128[0] = pn_words[0];
             pn->pn128[0] |= ((u_int64_t)pn_words[1] << 32);
             pn->pn128[1] = pn_words[2];
             pn->pn128[1] |= ((u_int64_t)pn_words[3] << 32);
    }
}

/* HL case */
void
htt_rx_mpdu_desc_pn_hl(
    htt_pdev_handle pdev,
    void *mpdu_desc,
    union htt_rx_pn_t *pn,
    int pn_len_bits)
{
    if (htt_rx_msdu_first_msdu_flag_hl(pdev, mpdu_desc)) {
        /* Fix Me: only for little endian */
        struct hl_htt_rx_desc_base *rx_desc =
            (struct hl_htt_rx_desc_base *) mpdu_desc;
        u_int32_t *word_ptr = (u_int32_t *)pn->pn128;

        /* TODO: for Host of big endian */
        switch (pn_len_bits) {
            case 128:
                /* bits 128:64 */
                *(word_ptr + 3) = rx_desc->pn_127_96;
                /* bits 63:0 */
                *(word_ptr + 2) = rx_desc->pn_95_64;
            case 48:
                /* bits 48:0
                 * copy 64 bits
                 */
                *(word_ptr + 1) = rx_desc->u0.pn_63_32;
            case 24:
                /* bits 23:0
                 * copy 32 bits
                 */
                *(word_ptr + 0) = rx_desc->pn_31_0;
                break;
            default:
                qdf_print(
                        "Error: invalid length spec (%d bits) for PN", pn_len_bits);
                qdf_assert(0);
        };
    } else {
        /* not first msdu, no pn info */
        qdf_print(
                "Error: get pn from a not-first msdu.");
        qdf_assert(0);
    }
}

u_int32_t
htt_rx_mpdu_desc_tsf32(
    htt_pdev_handle pdev,
    void *mpdu_desc)
{
/* FIX THIS */
return 0;
}


#if RX_DEBUG
int
htt_rx_mpdu_htt_status(htt_pdev_handle pdev)
{
    return pdev->g_htt_status;
}
#endif

int
htt_rx_msdu_desc_completes_mpdu_hl(htt_pdev_handle pdev, void *msdu_desc)
{
    return (
            ((u_int8_t*)(msdu_desc) - sizeof(struct hl_htt_rx_ind_base))
            [HTT_ENDIAN_BYTE_IDX_SWAP(HTT_RX_IND_HL_FLAG_OFFSET)]
            & HTT_RX_IND_HL_FLAG_LAST_MSDU)
        ? 1:0;
}

int
htt_rx_msdu_has_wlan_mcast_flag_hl(htt_pdev_handle pdev, void *msdu_desc)
{
    /* currently, only first msdu has hl rx_desc */
    return htt_rx_msdu_first_msdu_flag_hl(pdev, msdu_desc);
}

int
htt_rx_msdu_is_wlan_mcast_hl(htt_pdev_handle pdev, void *msdu_desc)
{
    struct hl_htt_rx_desc_base *rx_desc =
        (struct hl_htt_rx_desc_base *) msdu_desc;
    return
        HTT_WORD_GET(*(u_int32_t*)rx_desc, HTT_HL_RX_DESC_MCAST_BCAST);
}

int
htt_rx_msdu_is_frag_hl(htt_pdev_handle pdev, void *msdu_desc)
{
    struct hl_htt_rx_desc_base *rx_desc =
        (struct hl_htt_rx_desc_base *) msdu_desc;

    return
        HTT_WORD_GET(*(u_int32_t*)rx_desc, HTT_HL_RX_DESC_MCAST_BCAST);
}

static inline
u_int8_t
htt_rx_msdu_fw_desc_get(htt_pdev_handle pdev, void *msdu_desc)
{
    /*
     * HL and LL use the same format for FW rx desc, but have the FW rx desc
     * in different locations.
     * In LL, the FW rx descriptor has been copied into the same
     * htt_host_rx_desc_base struct that holds the HW rx desc.
     * In HL, the FW rx descriptor, along with the MSDU payload,
     * is in the same buffer as the rx indication message.
     *
     * Use the FW rx desc offset configured during startup to account for
     * this difference between HL vs. LL.
     *
     * An optimization would be to define the LL and HL msdu_desc pointer
     * in such a way that they both use the same offset to the FW rx desc.
     * Then the following functions could be converted to macros, without
     * needing to expose the htt_pdev_t definition outside HTT.
     */
    return *(((u_int8_t *) msdu_desc) + pdev->rx_fw_desc_offset);
}

int
htt_rx_msdu_discard(htt_pdev_handle pdev, void *msdu_desc)
{
    return htt_rx_msdu_fw_desc_get(pdev, msdu_desc) & FW_RX_DESC_DISCARD_M;
}
qdf_export_symbol(htt_rx_msdu_discard);

int
htt_rx_msdu_forward(htt_pdev_handle pdev, void *msdu_desc)
{
    return htt_rx_msdu_fw_desc_get(pdev, msdu_desc) & FW_RX_DESC_FORWARD_M;
}
qdf_export_symbol(htt_rx_msdu_forward);

int
htt_rx_msdu_inspect(htt_pdev_handle pdev, void *msdu_desc)
{
    return htt_rx_msdu_fw_desc_get(pdev, msdu_desc) & FW_RX_DESC_INSPECT_M;
}

void
htt_rx_msdu_actions(
    htt_pdev_handle pdev,
    void *msdu_desc,
    int *discard,
    int *forward,
    int *inspect)
{
    u_int8_t rx_msdu_fw_desc = htt_rx_msdu_fw_desc_get(pdev, msdu_desc);
#if HTT_DEBUG_DATA
    HTT_PRINT("act:0x%x ",rx_msdu_fw_desc);
#endif
    *discard = rx_msdu_fw_desc & FW_RX_DESC_DISCARD_M;
    *forward = rx_msdu_fw_desc & FW_RX_DESC_FORWARD_M;
    *inspect = rx_msdu_fw_desc & FW_RX_DESC_INSPECT_M;
}

static inline qdf_nbuf_t
htt_rx_netbuf_pop(rx_handle_t rxh)
{
    htt_pdev_handle pdev = (htt_pdev_handle)rxh;
#if QCA_PARTNER_DIRECTLINK_RX
    /*
     * For Direct Link RX, need to get Rx net buffer by partner API.
     */
    if (CE_is_directlink(((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->ce_tx_hdl)) {
        printk("ERROR: %s called in Directlink path\n",__func__);
        A_ASSERT(0);
	return NULL;
    } else
#endif /* QCA_PARTNER_DIRECTLINK_RX */
{
    int idx;
    qdf_nbuf_t msdu;
    qdf_nbuf_t next_msdu;

    HTT_ASSERT1(htt_rx_ring_elems(pdev) != 0);
    idx = pdev->rx_ring.sw_rd_idx.msdu_payld;
    msdu = pdev->rx_ring.buf.netbufs_ring[idx];
    idx++;
    idx &= pdev->rx_ring.size_mask;
    next_msdu = pdev->rx_ring.buf.netbufs_ring[idx];
    if (next_msdu) {
	    prefetch(next_msdu->data);
	    prefetch(&next_msdu->tail);
    }
    pdev->rx_ring.sw_rd_idx.msdu_payld = idx;
    pdev->rx_ring.fill_cnt--;
    return msdu;
} /* QCA_PARTNER_DIRECTLINK_RX */
}

#if RX_DEBUG
static inline qdf_nbuf_t
htt_rx_netbuf_peek(rx_handle_t rxh)
{
    htt_pdev_handle pdev = (htt_pdev_handle)rxh;
#if QCA_PARTNER_DIRECTLINK_RX
    /*
     * For Direct Link RX, need to get Rx net buffer by partner API.
     */
    if (CE_is_directlink(((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->ce_tx_hdl)) {
        printk("ERROR: %s called in Directlink path\n",__func__);
        A_ASSERT(0);
	return NULL;
    } else
#endif /* QCA_PARTNER_DIRECTLINK_RX */
{
    int idx;
    qdf_nbuf_t msdu;

    idx = pdev->rx_ring.sw_rd_idx.msdu_payld;
    msdu = pdev->rx_ring.buf.netbufs_ring[idx];
    return msdu;
} /* QCA_PARTNER_DIRECTLINK_RX */

}
#endif

/* FIX ME: this function applies only to LL rx descs. An equivalent for HL rx descs is needed. */
#if RX_CHECKSUM_OFFLOADDDDDDDDDDDDDDDD

static inline
void
htt_set_checksum_result_hl( qdf_nbuf_t msdu, void *rx_desc)
{
    u_int8_t flag = ((u_int8_t*)rx_desc - sizeof(struct hl_htt_rx_ind_base))[HTT_ENDIAN_BYTE_IDX_SWAP(HTT_RX_IND_HL_FLAG_OFFSET)];
    int is_ipv6 = flag & HTT_RX_IND_HL_FLAG_IPV6 ? 1:0;
    int is_tcp = flag & HTT_RX_IND_HL_FLAG_TCP ? 1:0;
    int is_udp = flag & HTT_RX_IND_HL_FLAG_UDP ? 1:0;

    qdf_nbuf_rx_cksum_t cksum = {
        qdf_nbuf_RX_CKSUM_TYPE_NONE,
        qdf_nbuf_RX_CKSUM_NONE,
        0
    };

    switch ((is_udp << 2) | (is_tcp << 1) | (is_ipv6 << 0)) {
        case 0x4:
            cksum.l4_type = qdf_nbuf_RX_CKSUM_UDP;
            break;
        case 0x2:
            cksum.l4_type = qdf_nbuf_RX_CKSUM_TCP;
            break;
        case 0x5:
            cksum.l4_type = qdf_nbuf_RX_CKSUM_UDPIPV6;
            break;
        case 0x3:
            cksum.l4_type = qdf_nbuf_RX_CKSUM_TCPIPV6;
            break;
        default:
            cksum.l4_type = qdf_nbuf_RX_CKSUM_TYPE_NONE;
            break;
    }
    if (cksum.l4_type != qdf_nbuf_RX_CKSUM_TYPE_NONE) {
        cksum.l4_result = flag & HTT_RX_IND_HL_FLAG_C4_FAILED ?
                    qdf_nbuf_RX_CKSUM_NONE : qdf_nbuf_RX_CKSUM_TCP_UDP_UNNECESSARY;
    }
    qdf_nbuf_set_rx_cksum(msdu, &cksum );
}

#else
#define htt_set_checksum_result_hl(msdu, rx_desc) /* no-op */
#endif

#if RX_DEBUG
void
dump_pkt(qdf_nbuf_t msdu, int len)
{
    int i;
    char *buf = qdf_nbuf_data(msdu);

    printk("++++++++++++++++++++++++++++++++++++++++++++\n");
    printk("Dumping %d bytes of data\n", len);

    for (i = 0; i < len; i++) {
        printk("0x%x ", *buf++);
        if (i && (i % 8 == 0))
            printk("\n");
    }

    printk("++++++++++++++++++++++++++++++++++++++++++++\n");
}

void
dump_htt_rx_desc(htt_pdev_handle pdev, void *rx_desc)
{
    int i;
    u_int32_t *buf = (u_int32_t *)rx_desc;
    int len = RX_STD_DESC_SIZE_DWORD;

    printk("++++++++++++++++++++++++++++++++++++++++++++\n");
    printk("Dumping %d words of rx_desc of the msdu \n", len);

    for (i = 0; i < len; i++) {
        printk("0x%08x ", *buf);
        buf++;
        if (i && (i % 4 == 0))
            printk("\n");
    }

    printk("++++++++++++++++++++++++++++++++++++++++++++\n");
}
uint32_t
HIF_Read_Reg(void *ol_scn, A_UINT32 addr);

qdf_nbuf_t
htt_rx_debug(rx_handle_t rxh,
    qdf_nbuf_t rx_ind_msg,
        qdf_nbuf_t msdu,
        int *npackets,
        int msdu_done)
{
    htt_pdev_handle pdev = (htt_pdev_handle)rxh;
    void *rx_desc;

    rx_desc = htt_rx_desc(msdu);

    if (!msdu_done) {

            if (pdev->htt_rx_donebit_assertflag == 0) {
                if ((pdev->g_htt_status != htt_rx_status_ok) && ((*npackets) == 0)) {
                    pdev->htt_rx_donebit_assertflag = 1;

                    printk("DONE BIT NOT SET FOR FIRST MSDU and PEER IS INVALID \n");
                    printk("rx_desc %pK msdu %pK rx_ind_msg %pK \n", rx_desc, msdu, rx_ind_msg);
                    printk("status %d peer_id %d tid %d no. of pkts %d\n", pdev->g_htt_status, pdev->g_peer_id, pdev->g_tid, *npackets);
                    printk("peer_mac 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",
                            pdev->g_peer_mac[0], pdev->g_peer_mac[1], pdev->g_peer_mac[2],
                            pdev->g_peer_mac[3], pdev->g_peer_mac[4], pdev->g_peer_mac[5]);
                    printk("num_mpdus %d mpdu_ranges %d msdu_chained %d\n",
                            pdev->g_num_mpdus, pdev->g_mpdu_ranges, pdev->g_msdu_chained);
                printk("Rx Attention 0x%x\n", pdev->ar_rx_ops->get_attn_word(rx_desc));

                    qdf_nbuf_map_nbytes(pdev->osdev, msdu, QDF_DMA_FROM_DEVICE, HTT_RX_BUF_SIZE);
                    return NULL;
                } else {
                    printk("DONE BIT NOT SET but EITHER PEER IS VALID (OR) NOT A FIRST MSDU\n");
                    printk("rx_desc %pK msdu %pK rx_ind_msg %pK \n", rx_desc, msdu, rx_ind_msg);
                    printk("status %d peer_id %d tid %d no. of pkts %d\n", pdev->g_htt_status, pdev->g_peer_id, pdev->g_tid, *npackets);
                    printk("peer_mac 0x%x:0x%x:0x%x:0x%x:0x%x:0x%x\n",
                            pdev->g_peer_mac[0], pdev->g_peer_mac[1], pdev->g_peer_mac[2],
                            pdev->g_peer_mac[3], pdev->g_peer_mac[4], pdev->g_peer_mac[5]);
                    printk("num_mpdus %d mpdu_ranges %d msdu_chained %d\n",
                            pdev->g_num_mpdus, pdev->g_mpdu_ranges, pdev->g_msdu_chained);
                printk("Rx Attention 0x%x\n", pdev->ar_rx_ops->get_attn_word(rx_desc));

                HTT_ASSERT_ALWAYS(pdev->ar_rx_ops->attn_msdu_done(rx_desc));
                }
            } else {
                int idx;
                qdf_nbuf_t next_msdu;

                idx = pdev->rx_ring.sw_rd_idx.msdu_payld;
                idx++;
                idx &= pdev->rx_ring.size_mask;
                next_msdu = pdev->rx_ring.buf.netbufs_ring[idx];
                qdf_nbuf_unmap(pdev->osdev, next_msdu, QDF_DMA_FROM_DEVICE);
                rx_desc = htt_rx_desc(next_msdu);

                printk("next_rx_desc %pK next_msdu %pK \n", rx_desc, next_msdu);
                printk("Rx Attention of next_msdu 0x%x\n",
                    pdev->ar_rx_ops->get_attn_word(rx_desc));

            if (pdev->ar_rx_ops->attn_msdu_done(rx_desc)) {
                    printk("RECOVERED FROM DONE BIT NOT SET ERROR. USING NEXT MSDU\n");
                msdu = htt_rx_netbuf_pop((rx_handle_t)pdev);
                    htt_rx_desc_frame_free(pdev, msdu);
                    /* Proceed with next msdu if recovered from Done bit */
                msdu = htt_rx_netbuf_pop((rx_handle_t)pdev);
                    pdev->htt_rx_donebit_assertflag = 0;
                } else {
                    printk("DONE BIT NOT SET FOR CURRENT AND NEXT MSDU\n");
                HTT_ASSERT_ALWAYS(pdev->ar_rx_ops->attn_msdu_done(rx_desc));
                }
            }
        } else {
            if (pdev->htt_rx_donebit_assertflag == 1) {
                printk("rx_desc %pK msdu %pK \n", rx_desc, msdu);
                printk("Rx Attention of msdu 0x%x\n",
                    pdev->ar_rx_ops->get_attn_word(rx_desc));
                printk("RECOVERED FROM DONE BIT NOT SET ERROR. USING SAME MSDU\n");
                pdev->htt_rx_donebit_assertflag = 0;
            }
            if ((*npackets) == 0) {
            msdu = htt_rx_netbuf_pop((rx_handle_t)pdev);
            }
        }
    return msdu;
}
#endif

int
htt_rx_amsdu_pop_hl(
    htt_pdev_handle pdev,
#if QCA_OL_SUPPORT_RAWMODE_TXRX
    struct ol_txrx_vdev_t *vdev,
#endif
    qdf_nbuf_t rx_ind_msg,
    qdf_nbuf_t *head_msdu,
    qdf_nbuf_t *tail_msdu,
    u_int32_t *npackets
    )
{
    pdev->rx_desc_size_hl =
        (qdf_nbuf_data(rx_ind_msg))
        [HTT_ENDIAN_BYTE_IDX_SWAP(
            HTT_RX_IND_HL_RX_DESC_LEN_OFFSET)];

    /* point to the rx desc */
    qdf_nbuf_pull_head(rx_ind_msg,
            sizeof(struct hl_htt_rx_ind_base));
    *head_msdu = *tail_msdu = rx_ind_msg;

#if RX_CHECKSUM_OFFLOAD
    htt_set_checksum_result_hl(rx_ind_msg, (struct htt_host_rx_desc_base *)(qdf_nbuf_data(rx_ind_msg)));
#endif

    qdf_nbuf_set_next(*tail_msdu, NULL);
    /* here the defrag has not been taken into account */
    return 0;
}

void *
htt_rx_mpdu_desc_list_next_ll(htt_pdev_handle pdev, qdf_nbuf_t rx_ind_msg)
{

    int idx = pdev->rx_ring.sw_rd_idx.msdu_desc;
    qdf_nbuf_t netbuf = pdev->rx_ring.buf.netbufs_ring[idx];
    pdev->rx_ring.sw_rd_idx.msdu_desc = pdev->rx_ring.sw_rd_idx.msdu_payld;

    return (void *) htt_rx_desc(netbuf);

}


void *
htt_rx_mpdu_desc_list_next_hl(htt_pdev_handle pdev, qdf_nbuf_t rx_ind_msg)
{
    /*
     * for HL, the returned value is not mpdu_desc,
     * it's translated hl_rx_desc just after the hl_ind_msg
     */
    void *mpdu_desc = (void *) qdf_nbuf_data(rx_ind_msg);

    /* for HL AMSDU, we can't point to payload now, because
     * hl rx desc is not fixed, we can't retrive the desc
     * by minus rx_desc_size when release. keep point to hl rx desc
     * now.
     */
#if 0
	qdf_nbuf_pull_head(rx_ind_msg, pdev->rx_desc_size_hl);
#endif

    return mpdu_desc;
}

void *
htt_rx_msdu_desc_retrieve_ll(htt_pdev_handle pdev, qdf_nbuf_t msdu)
{
    return htt_rx_desc(msdu);
}

void *
htt_rx_msdu_desc_retrieve_hl(htt_pdev_handle pdev, qdf_nbuf_t msdu)
{
    /* currently for HL AMSDU, we don't point to payload.
     * we shift to payload in ol_rx_deliver later
     */
    return qdf_nbuf_data(msdu);
}

int htt_rx_mpdu_is_encrypted_hl(htt_pdev_handle pdev, void *mpdu_desc)
{
    if (htt_rx_msdu_first_msdu_flag_hl(pdev, mpdu_desc)) {
        /* Fix Me: only for little endian */
        struct hl_htt_rx_desc_base *rx_desc =
            (struct hl_htt_rx_desc_base *) mpdu_desc;

        return HTT_WORD_GET(*(u_int32_t*)rx_desc, HTT_HL_RX_DESC_MPDU_ENC);
    }else {
        /* not first msdu, no encrypt info for hl */
        qdf_print(
                "Error: get encrypted from a not-first msdu.");
        qdf_assert(0);
		return -1;
    }
}

void
htt_rx_desc_frame_free(
    htt_pdev_handle htt_pdev,
    qdf_nbuf_t msdu)
{
    qdf_nbuf_free(msdu);
}

void
htt_rx_msdu_desc_free(htt_pdev_handle htt_pdev, qdf_nbuf_t msdu)
{
    /*
     * The rx descriptor is in the same buffer as the rx MSDU payload,
     * and does not need to be freed separately.
     */
}
void
htt_rx_msdu_buff_replenish(htt_pdev_handle pdev)
{

    if (qdf_atomic_dec_and_test(&pdev->rx_ring.refill_ref_cnt)) {
        if (!pdev->cfg.is_high_latency) {
            int num_to_fill;
            num_to_fill = pdev->rx_ring.fill_level - pdev->rx_ring.fill_cnt;
            htt_rx_ring_fill_n(pdev, num_to_fill /* okay if <= 0 */);
        }
    }
    qdf_atomic_inc(&pdev->rx_ring.refill_ref_cnt);
}

void ol_ath_add_vow_extstats(rx_handle_t rxh, qdf_nbuf_t msdu);
void ol_ath_arp_debug(rx_handle_t rxh, qdf_nbuf_t msdu);

struct htt_rx_callbacks htt_callbacks = {
    .txrx_is_decap_type_raw = htt_txrx_is_decap_type_raw,
    .rx_ring_elems = htt_rx_ring_elems,
    .rx_ind_fw_rx_desc_bytes_get = htt_rx_ind_fw_desc_bytes_get,
#if RX_DEBUG
    .rx_netbuf_peek = htt_rx_netbuf_peek,
#endif
    .rx_netbuf_pop = htt_rx_netbuf_pop,
    .rx_buf_size = htt_rx_buf_size,
#if RX_DEBUG
    .rx_debug = htt_rx_debug,
#endif
    .rx_desc_reservation = htt_rx_desc_reservation,
    .rx_ind_msdu_byte_offset = htt_rx_ind_msdu_byte_offset,
    .rx_ind_msdu_byte_idx_inc = htt_rx_ind_msdu_byte_idx_inc,
    .ath_add_vow_extstats = ol_ath_add_vow_extstats,
    .ath_arp_debug = ol_ath_arp_debug,
};

struct ar_rx_desc_ops*
ar_rx_attach(ar_handle_t arh, rx_handle_t htt_pdev, qdf_device_t osdev, struct htt_rx_callbacks *cbs);
/* move the function to the end of file
 * to omit ll/hl pre-declaration
 */
int
htt_rx_attach(struct htt_pdev_t *pdev)
{
#if WIFI_MEM_MANAGER_SUPPORT
    int intr_ctxt;
    uint32_t soc_idx = wlan_ucfg_get_config_param((void *)pdev->ctrl_psoc, SOC_ID);
#endif
    qdf_dma_addr_t paddr = 0;
    qdf_device_t qdf_dev = wlan_psoc_get_qdf_dev((void *)pdev->ctrl_psoc);

    if (!pdev->cfg.is_high_latency) {

        //Attach the RX desc functions
        pdev->ar_rx_ops = ar_rx_attach(pdev->arh, (rx_handle_t)pdev, pdev->osdev, &htt_callbacks);
        if(pdev->ar_rx_ops == NULL) {
            printk("%s:%d Invalid target",__func__,__LINE__);
            goto fail1;
        }

        pdev->rx_ring.size = htt_rx_ring_size(pdev);
        HTT_ASSERT2(IS_PWR2(pdev->rx_ring.size));
        pdev->rx_ring.size_mask = pdev->rx_ring.size - 1;

        /*
         * Set the initial value for the level to which the rx ring should
         * be filled, based on the max throughput and the worst likely
         * latency for the host to fill the rx ring with new buffers.
         * In theory, this fill level can be dynamically adjusted from
         * the initial value set here, to reflect the actual host latency
         * rather than a conservative assumption about the host latency.
         */
#if QCA_PARTNER_CBM_DIRECTPATH
        pdev->rx_ring.fill_level = QCA_PARTNER_CBM_DIRECTPATH_RX_RING_SIZE;
#else
        pdev->rx_ring.fill_level = htt_rx_ring_fill_level(pdev);
#endif
        pdev->rx_ring.buf.netbufs_ring = qdf_mem_malloc(
            pdev->rx_ring.size * sizeof(qdf_nbuf_t));
        if (!pdev->rx_ring.buf.netbufs_ring) {
            goto fail1;
        }
#if WIFI_MEM_MANAGER_SUPPORT
        intr_ctxt = (in_interrupt() || irqs_disabled()) ? 1 : 0;
        pdev->rx_ring.buf.paddrs_ring = wifi_cmem_allocation(soc_idx,
					0, CM_RX_ATTACH_PADDR_RING,
					pdev->rx_ring.size * sizeof(u_int32_t),
					(void*) qdf_dev->drv_hdl,
					&paddr,intr_ctxt);
#else
        pdev->rx_ring.buf.paddrs_ring = qdf_mem_alloc_consistent(
            pdev->osdev,
            &(((struct pci_dev *)(qdf_dev->drv_hdl))->dev),
            pdev->rx_ring.size * sizeof(u_int32_t),
            &paddr);
#endif
        if (!pdev->rx_ring.buf.paddrs_ring) {
            goto fail2;
        }
        pdev->rx_ring.base_paddr = paddr;
#if WIFI_MEM_MANAGER_SUPPORT
        pdev->rx_ring.alloc_idx.vaddr = wifi_cmem_allocation(soc_idx, 0,
                                        CM_RX_ATTACH_ALLOC_IDX_VADDR,
                                        sizeof(u_int32_t),
					(void*) qdf_dev->drv_hdl,
					&paddr,intr_ctxt);
#else
        pdev->rx_ring.alloc_idx.vaddr = qdf_mem_alloc_consistent(
            pdev->osdev,
            &(((struct pci_dev *)(qdf_dev->drv_hdl))->dev),
            sizeof(u_int32_t),
            &paddr);
#endif
        if (!pdev->rx_ring.alloc_idx.vaddr) {
            goto fail3;
        }
        pdev->rx_ring.alloc_idx.paddr = paddr;
        pdev->rx_ring.sw_rd_idx.msdu_payld = 0;
        pdev->rx_ring.sw_rd_idx.msdu_desc = 0;
        *pdev->rx_ring.alloc_idx.vaddr = 0;

        /*
         * Initialize the Rx refill reference counter to be one so that
         * only one thread is allowed to refill the Rx ring.
         */
        qdf_atomic_init(&pdev->rx_ring.refill_ref_cnt);
        qdf_atomic_inc(&pdev->rx_ring.refill_ref_cnt);

        /* Initialize the Rx refill retry timer */
        qdf_timer_init(pdev->osdev, &pdev->rx_ring.refill_retry_timer,
                          htt_rx_ring_refill_retry, (void *)pdev, QDF_TIMER_TYPE_WAKE_APPS);

        pdev->rx_ring.fill_cnt = 0;

#if QCA_PARTNER_DIRECTLINK_RX
        /*
         * For Direct Link RX, rx ring buffers are created by partner API.
         */
        if (CE_is_directlink(((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->ce_tx_hdl)) {
            if (htt_rx_attach_partner(pdev)) {
                goto fail4;
            }
        } else
#endif /* QCA_PARTNER_DIRECTLINK_RX */
        {
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            if (pdev->nss_wifiol_ctx) {
                if (osif_nss_ol_htt_rx_init(pdev)) {
                    goto fail3;
                }

            } else
#endif
            {
                htt_rx_ring_fill_n(pdev, pdev->rx_ring.fill_level);
            }
        } /* QCA_PARTNER_DIRECTLINK_RX */

	pdev->htt_rx_amsdu_pop = (htt_rx_amsdu_pop_cb)pdev->ar_rx_ops->amsdu_pop;
        pdev->htt_rx_mpdu_desc_list_next = htt_rx_mpdu_desc_list_next_ll;
	pdev->htt_rx_mpdu_desc_seq_num = (htt_rx_mpdu_desc_seq_num_cb)pdev->ar_rx_ops->mpdu_desc_seq_num;
        pdev->htt_rx_mpdu_desc_pn = htt_rx_mpdu_desc_pn_ll;
        pdev->htt_rx_msdu_desc_completes_mpdu = (htt_rx_msdu_desc_completes_mpdu_cb)pdev->ar_rx_ops->msdu_desc_completes_mpdu;
        pdev->htt_rx_msdu_first_msdu_flag = (htt_rx_msdu_first_msdu_flag_cb)pdev->ar_rx_ops->msdu_first_msdu_flag;
        pdev->htt_rx_msdu_key_id_octet = (htt_rx_msdu_key_id_octet_cb)pdev->ar_rx_ops->msdu_key_id_octet;
        pdev->htt_rx_msdu_has_wlan_mcast_flag = (htt_rx_msdu_has_wlan_mcast_flag_cb)pdev->ar_rx_ops->msdu_has_wlan_mcast_flag;
        pdev->htt_rx_msdu_is_wlan_mcast = (htt_rx_msdu_is_wlan_mcast_cb)pdev->ar_rx_ops->msdu_is_wlan_mcast;
        pdev->htt_rx_msdu_is_frag = (htt_rx_msdu_is_frag_cb)pdev->ar_rx_ops->msdu_is_frag;
	pdev->htt_rx_msdu_desc_retrieve = htt_rx_msdu_desc_retrieve_ll;
        pdev->htt_rx_mpdu_is_encrypted = (htt_rx_mpdu_is_encrypted_cb)pdev->ar_rx_ops->mpdu_is_encrypted;

#if RX_DEBUG
        pdev->htt_rx_mpdu_status = htt_rx_mpdu_htt_status;
#endif
    } else {
        pdev->rx_ring.size = OL_CFG_RX_RING_SIZE_MIN;
        HTT_ASSERT2(IS_PWR2(pdev->rx_ring.size));
        pdev->rx_ring.size_mask = pdev->rx_ring.size - 1;

        /* host can force ring base address if it wish to do so */
        pdev->rx_ring.base_paddr = 0;
        /* FIX THIS: HL has no ar handle */
        pdev->arh = (ar_handle_t *)pdev;

        pdev->htt_rx_amsdu_pop = (htt_rx_amsdu_pop_cb)htt_rx_amsdu_pop_hl;
        pdev->htt_rx_mpdu_desc_list_next = htt_rx_mpdu_desc_list_next_hl;
        pdev->htt_rx_mpdu_desc_seq_num = (htt_rx_mpdu_desc_seq_num_cb)htt_rx_mpdu_desc_seq_num_hl;
        pdev->htt_rx_mpdu_desc_pn = htt_rx_mpdu_desc_pn_hl;
        pdev->htt_rx_msdu_desc_completes_mpdu = (htt_rx_msdu_desc_completes_mpdu_cb)htt_rx_msdu_desc_completes_mpdu_hl;
        pdev->htt_rx_msdu_first_msdu_flag = (htt_rx_msdu_first_msdu_flag_cb)htt_rx_msdu_first_msdu_flag_hl;
        pdev->htt_rx_msdu_has_wlan_mcast_flag = (htt_rx_msdu_has_wlan_mcast_flag_cb)htt_rx_msdu_has_wlan_mcast_flag_hl;
        pdev->htt_rx_msdu_is_wlan_mcast = (htt_rx_msdu_is_wlan_mcast_cb)htt_rx_msdu_is_wlan_mcast_hl;
        pdev->htt_rx_msdu_is_frag = (htt_rx_msdu_is_frag_cb)htt_rx_msdu_is_frag_hl;
        pdev->htt_rx_msdu_desc_retrieve = htt_rx_msdu_desc_retrieve_hl;
        pdev->htt_rx_mpdu_is_encrypted = (htt_rx_mpdu_is_encrypted_cb)htt_rx_mpdu_is_encrypted_hl;
#if RX_DEBUG
        pdev->htt_rx_mpdu_status = htt_rx_mpdu_htt_status;
#endif
        /*
         * HL case, the rx descriptor can be different sizes for
         * different sub-types of RX_IND messages, e.g. for the
         * initial vs. interior vs. final MSDUs within a PPDU.
         * The size of each RX_IND message's rx desc is read from
         * a field within the RX_IND message itself.
         * In the meantime, until the rx_desc_size_hl variable is
         * set to its real value based on the RX_IND message,
         * initialize it to a reasonable value (zero).
         */
        pdev->rx_desc_size_hl = 0;
    }
#if RX_DEBUG
    pdev->htt_rx_donebit_assertflag = 0;
#endif
    return 0; /* success */

#if QCA_PARTNER_DIRECTLINK_RX
/*
 * For Direct Link RX, clean up rx ring buffers by partner API if attach fail.
 */
fail4:
    if (CE_is_directlink(((struct ol_txrx_pdev_t*)pdev->txrx_pdev)->ce_tx_hdl)) {
        htt_rx_detach_partner(pdev);
    }
#endif /* QCA_PARTNER_DIRECTLINK_RX */

fail3:
#if !WIFI_MEM_MANAGER_SUPPORT
    qdf_mem_free_consistent(
        pdev->osdev,
        &(((struct pci_dev *)(qdf_dev->drv_hdl))->dev),
        pdev->rx_ring.size * sizeof(u_int32_t),
        pdev->rx_ring.buf.paddrs_ring,
        pdev->rx_ring.base_paddr,
        qdf_get_dma_mem_context((&pdev->rx_ring.buf), memctx));
#else
     wifi_cmem_free(soc_idx, 0, CM_RX_ATTACH_PADDR_RING ,
		    (void *)pdev->rx_ring.buf.paddrs_ring);
#endif

fail2:
    qdf_mem_free(pdev->rx_ring.buf.netbufs_ring);

fail1:
    return 1; /* failure */
}

