/*
 * Copyright (c) 2015-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/types.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <net/ipv6.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3,9,0))
#include <net/ipip.h>
#else
#include <net/ip_tunnels.h>
#endif

#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/in.h>
#include <asm/cacheflush.h>
#include "osif_private.h"
#include <nss_api_if.h>
#include <nss_cmn.h>
#include <qdf_nbuf.h>
#include "ol_if_athvar.h"
#include <ol_txrx_types.h>
#include <ol_txrx_peer_find.h>
#include <ol_txrx_internal.h>  /* ol_txrx_pdev_t, etc. */
#include <ol_txrx_api_internal.h>  /* ol_txrx_peer_find_inact_timeout_handler, etc. */


#include "osif_private.h"
#include "osif_nss_wifiol_vdev_if.h"
#include <ar_internal.h>

#if MESH_MODE_SUPPORT
#include <if_meta_hdr.h>
#endif

#if ATH_DATA_RX_INFO_EN || MESH_MODE_SUPPORT
u_int32_t osif_rx_status_dump(void* rs);
#endif

extern struct osif_nss_vdev_cfg_pvt osif_nss_vdev_cfgp;

/*
 * This file is responsible for interacting with qca-nss-drv's
 * WIFI to manage WIFI VDEVs.
 *
 * This driver also exposes few APIs which can be used by
 * another module to perform operations on CAPWAP tunnels. However, we create
 * one netdevice for all the CAPWAP tunnels which is done at the module's
 * init time if NSS_wifimgr_ONE_NETDEV is set in the Makefile.
 *
 * If your requirement is to create one netdevice per-CAPWAP tunnel, then
 * netdevice needs to be created before CAPWAP tunnel create. Netdevice are
 * created using nss_wifimgr_netdev_create() API.
 *
 */
#define OSIF_NSS_DEBUG_LEVEL 1

/*
 * NSS WiFi offload debug macros
 */
#if (OSIF_NSS_DEBUG_LEVEL < 1)
#define osif_nss_assert(fmt, args...)
#else
#define osif_nss_assert(c) if (!(c)) { BUG_ON(!(c)); }
#endif /* OSIF_NSS_DEBUG_LEVEL */

/*
 * Compile messages for dynamic enable/disable
 */
#if !defined(CONFIG_DYNAMIC_DEBUG)
#define osif_nss_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define osif_nss_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define osif_nss_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else /* CONFIG_DYNAMIC_DEBUG */
/*
 * Statically compile messages at different levels
 */
#if (OSIF_NSS_DEBUG_LEVEL < 2)
#define osif_nss_warn(s, ...)
#else
#define osif_nss_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (OSIF_NSS_DEBUG_LEVEL < 3)
#define osif_nss_info(s, ...)
#else
#define osif_nss_info(s, ...)   pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (OSIF_NSS_DEBUG_LEVEL < 4)
#define osif_nss_trace(s, ...)
#else
#define osif_nss_trace(s, ...)  pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif /* CONFIG_DYNAMIC_DEBUG */

void populate_tx_info(struct ol_ath_softc_net80211 *scn, qdf_nbuf_t  netbuf,  int i, int num_msdus,
                      enum htt_tx_status status, uint8_t mhdr_len, bool is_raw);

#if DBDC_REPEATER_SUPPORT
int dbdc_rx_process (os_if_t *osif ,struct net_device **dev ,wlan_if_t vap, struct sk_buff *skb, int *nwifi);
int dbdc_tx_process (wlan_if_t vap, osif_dev **osdev , struct sk_buff *skb);
#endif
/*
 * osif_nss_ni_stats_update
 *  Update ni stats when enable_statsv2 is on
 */
bool osif_nss_ni_stats_update(struct ol_ath_softc_net80211 *scn, struct sk_buff * nbuf, void *rx_mpdu_desc, uint8_t htt_rx_status, struct nss_wifi_append_statsv2_metahdr *metaheader)
{
    uint64_t msdus = metaheader->num_msdus;
    uint64_t bytes = metaheader->num_bytes;
    uint64_t retries = metaheader->num_retries;
    uint64_t mpdus = metaheader->num_mpdus;
    uint16_t peer_id = metaheader->peer_id;
    struct ol_txrx_pdev_t *pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);
    int is_mcast;
    struct ol_txrx_peer_t *peer = (peer_id == HTT_INVALID_PEER) ?
        NULL : pdev->peer_id_to_obj_map[peer_id];
    struct cdp_rx_stats *rx_stats = NULL;

    if (qdf_likely(peer)) {
        rx_stats = &peer->stats.rx;
    }

    is_mcast = ((htt_rx_msdu_forward(pdev->htt_pdev,
                    rx_mpdu_desc)) &&
            !htt_rx_msdu_discard(pdev->htt_pdev,
                rx_mpdu_desc));

    switch (htt_rx_status) {
        case HTT_RX_IND_MPDU_STATUS_OK:
            if (qdf_unlikely(!rx_stats)) {
                return 0;
            }
            rx_stats->to_stack.num += msdus;;
            rx_stats->to_stack.bytes += bytes;
            rx_stats->rx_mpdus += mpdus;
            rx_stats->rx_retries += retries;
            rx_stats->rx_ppdus++;
            if (is_mcast) {
                rx_stats->multicast.num += msdus;
                rx_stats->multicast.bytes += bytes;
            } else {
                rx_stats->unicast.num += msdus;
                rx_stats->unicast.bytes += bytes;
            }
            /* scn->scn_stats.rx_aggr = pdev->pdev_stats.rx.rx_aggr; */
            break;
        case HTT_RX_IND_MPDU_STATUS_ERR_FCS:

            /*
             * Make data_length = 0, because an invalid FCS Error data length is received.
             * We ignore this data length as it updates the Rx bytes stats in wrong way.
             * Peer can be NULL in fcs error, so updating in pdev stats.
             */
            bytes = 0;

            pdev->pdev_data_stats.rx.rx_packets += msdus;
            pdev->pdev_data_stats.rx.rx_bytes += bytes;
            pdev->pdev_data_stats.rx.rx_crcerr++;
            break;
        case HTT_RX_IND_MPDU_STATUS_TKIP_MIC_ERR:
            if (qdf_unlikely(!rx_stats)) {
                return 0;
            }
            rx_stats->to_stack.num += msdus;;
            rx_stats->to_stack.bytes += bytes;
            rx_stats->err.mic_err++;
            break;
        case HTT_RX_IND_MPDU_STATUS_DECRYPT_ERR:
            if (qdf_unlikely(!rx_stats)) {
                return 0;
            }
            rx_stats->to_stack.num += msdus;;
            rx_stats->to_stack.bytes += bytes;
            rx_stats->err.decrypt_err++;
            break;
        default:
            if (qdf_unlikely(!rx_stats)) {
                return 0;
            }
            rx_stats->to_stack.num += msdus;;
            rx_stats->to_stack.bytes += bytes;
            break;
    }
    return 1;
}

#define NSS_WIFI_RX_STATS_MAGIC 0xabec
#define RX_FIRST_MPDU_MASK 0x00000001

/*
 * osif_nss_ol_be_vdev_update_statsv2()
 * Update statsv2 per ppdu
 */
void osif_nss_ol_be_vdev_update_statsv2(struct ol_ath_softc_net80211 *scn, struct sk_buff * nbuf, void *rx_mpdu_desc, uint8_t htt_rx_status)
{
    uint32_t rx_desc_size = 0;
    struct ol_txrx_pdev_t *pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);
    struct htt_pdev_t *htt_pdev = pdev->htt_pdev;
    uint32_t attn;
    uint8_t is_first_msdu;
    struct nss_wifi_append_statsv2_metahdr *metaheader;
    void *tail;
    if (htt_rx_status == 0) {
   	    htt_rx_status = HTT_RX_IND_MPDU_STATUS_OK;
    }

    rx_desc_size = htt_pdev->ar_rx_ops->sizeof_rx_desc();
    dma_unmap_single(scn->soc->nss_soc.dmadev, virt_to_phys(rx_mpdu_desc), rx_desc_size, DMA_FROM_DEVICE);
    attn = htt_pdev->ar_rx_ops->get_attn_word(rx_mpdu_desc);
    is_first_msdu = htt_rx_msdu_first_msdu_flag(htt_pdev, rx_mpdu_desc);
    if (rx_mpdu_desc == NULL) {
        qdf_print("%pK Rx desc is NULL ",scn);
        return;
    }

    if (scn->enable_statsv2) {
        if ((attn & RX_FIRST_MPDU_MASK) && is_first_msdu) {
            tail = skb_tail_pointer(nbuf);
            dma_unmap_single(scn->soc->nss_soc.dmadev, virt_to_phys(tail), sizeof(struct nss_wifi_append_statsv2_metahdr), DMA_FROM_DEVICE);
            metaheader = (struct nss_wifi_append_statsv2_metahdr *)tail;
            if (metaheader->rxstatsmagic == NSS_WIFI_RX_STATS_MAGIC) {
                osif_nss_ni_stats_update(scn, nbuf, rx_mpdu_desc, htt_rx_status, metaheader);
            }
        }
    }
}
/*
 * osif_nss_ol_be_vdev_txinfo_handler()
 * Handler for tx info packets exceptioned from WIFI
 */
void osif_nss_ol_be_vdev_txinfo_handler(struct ol_ath_softc_net80211 *scn, struct sk_buff *skb, struct nss_wifi_vdev_per_packet_metadata *wifi_metadata, bool is_raw)
{

    struct nss_wifi_vdev_txinfo_per_packet_metadata *txinfo_metadata = NULL;
    txinfo_metadata = (struct nss_wifi_vdev_txinfo_per_packet_metadata *)&wifi_metadata->metadata.txinfo_metadata;

#if ATH_DATA_TX_INFO_EN
            if(scn->enable_perpkt_txstats) {
                uint8_t mhdr_len = 0;
                struct ieee80211_tx_status * ts = scn->tx_status_buf;
                ts->ppdu_rate = txinfo_metadata->ppdu_rate;
                ts->ppdu_num_mpdus_success = txinfo_metadata->ppdu_num_mpdus_success;
                ts->ppdu_num_mpdus_fail = txinfo_metadata->ppdu_num_mpdus_fail;
                ts->ppdu_num_msdus_success = txinfo_metadata->ppdu_num_msdus_success;
                ts->ppdu_bytes_success = txinfo_metadata->ppdu_bytes_success;
                ts->ppdu_duration = txinfo_metadata->ppdu_duration;
                ts->ppdu_retries = txinfo_metadata->ppdu_retries;
                ts->ppdu_is_aggregate = txinfo_metadata->ppdu_is_aggregate;
                ts->version = txinfo_metadata->version;
                ts->msdu_q_time = txinfo_metadata->msdu_q_time;

                if (txinfo_metadata->version == PPDU_STATS_VERSION_3) {
                    ts->ppdu_ack_timestamp = txinfo_metadata->ppdu_ack_timestamp;
                    ts->start_seq_num = txinfo_metadata->start_seq_num;
                    ts->ppdu_bmap_enqueued_hi = txinfo_metadata->ppdu_bmap_enqueued_hi;
                    ts->ppdu_bmap_enqueued_lo = txinfo_metadata->ppdu_bmap_enqueued_lo;
                    ts->ppdu_bmap_tried_hi = txinfo_metadata->ppdu_bmap_tried_hi;
                    ts->ppdu_bmap_tried_lo = txinfo_metadata->ppdu_bmap_tried_lo;
                    ts->ppdu_bmap_failed_hi = txinfo_metadata->ppdu_bmap_failed_hi;
                    ts->ppdu_bmap_failed_lo = txinfo_metadata->ppdu_bmap_failed_lo;
                }

                populate_tx_info(scn, skb, txinfo_metadata->msdu_count, txinfo_metadata->num_msdu, txinfo_metadata->status, mhdr_len, is_raw);
            }
#endif
}
/*
 * osif_nss_ol_be_vdev_tx_inspect_handler()
 *	Handler for tx inspect packets exceptioned from WIFI
 */
void osif_nss_ol_be_vdev_tx_inspect_handler(void *vdev_handle, struct sk_buff *skb)
{
    struct ol_txrx_peer_t *peer;
    struct sk_buff *skb_copy;
    uint16_t peer_id = HTT_INVALID_PEER;
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;

#if UMAC_SUPPORT_PROXY_ARP
    if (vdev->osif_proxy_arp(vdev->osif_vdev, skb)) {
        goto out;
    }
#endif
    TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
        if (peer && (peer->peer_ids[0] != HTT_INVALID_PEER) && (peer->bss_peer)) {
            peer_id = ol_tx_classify_get_peer_id(vdev, skb);
            if (peer_id == HTT_INVALID_PEER) {
                goto out;
            }

            skb_copy = qdf_nbuf_copy(skb);
            if (skb_copy) {
                qdf_nbuf_reset_ctxt(skb_copy);
                osif_nss_vdev_peer_tx_buf(vdev->osif_vdev, skb_copy, peer_id);
            }
        }
    }

out:
    qdf_nbuf_free(skb);
}

/**
 * osif_nss_vdev_aggregate_msdu - Aggregate MSDUs belonging to one MPDU into one SKB list
 */
void osif_nss_vdev_aggregate_msdu(ar_handle_t arh, struct ol_txrx_pdev_t *pdev, qdf_nbuf_t msduorig, qdf_nbuf_t *head_msdu, uint8_t no_clone_reqd)
{
    void *rx_desc = NULL;
    int is_head = 0;
    int is_tail = 0;
    qdf_nbuf_t nbuf, next, msdu = NULL;

    *head_msdu = NULL;

    rx_desc = (void *)msduorig->head;

    is_head = htt_rx_msdu_first_msdu_flag(pdev->htt_pdev, rx_desc);
    is_tail = htt_rx_msdu_desc_completes_mpdu(pdev->htt_pdev, rx_desc);

    if(no_clone_reqd) {
        msdu = msduorig;
    } else {
        msdu = qdf_nbuf_clone(msduorig);
        if(!msdu) {
            /*
             * Free/drop all the previous frames attached to
             * the list and start a new list
             */
            if(pdev->mon_mpdu_skb_list_head != NULL) {
                goto fail;
            }

            return;
        }
    }

    if (is_head) {

        /*
         * If a head msdu (first msdu in mpdu) arrived without a tail (last msdu),
         * free/drop all the previous frames attached to list and start a new
         * list with current msdu as head
         */
        if(pdev->mon_mpdu_skb_list_head != NULL) {
            nbuf = pdev->mon_mpdu_skb_list_head;

            while (nbuf) {
                next = qdf_nbuf_next(nbuf);
                qdf_nbuf_free(nbuf);
                nbuf = next;
            }
        }

        pdev->mon_mpdu_skb_list_head = msdu;
        pdev->mon_mpdu_skb_list_tail = msdu;

        /*
         * For non-aggregate , both is_tail and is_head is set for same MSDU
         * return MSDU
         */
        if (is_tail) {
            *head_msdu = msdu;
            pdev->mon_mpdu_skb_list_head = NULL;
            pdev->mon_mpdu_skb_list_tail = NULL;
            qdf_nbuf_set_next(msdu, NULL);
        }

        goto done;
    }

    /*
     * If a frame arrived without a previous head msdu, free/drop it
     */
    if(pdev->mon_mpdu_skb_list_head == NULL) {
        qdf_nbuf_free(msdu);
        goto done;
    }

    /*
     * Passed all the checks, so attach the frame to the list
     */
    qdf_nbuf_set_next(pdev->mon_mpdu_skb_list_tail, msdu);
    pdev->mon_mpdu_skb_list_tail = msdu;
    qdf_nbuf_set_next(msdu, NULL);

    if (is_tail) {
        /*
         * Return the pointer to list to the caller, once we get the tail frame
         */
        *head_msdu = pdev->mon_mpdu_skb_list_head;
        pdev->mon_mpdu_skb_list_head = NULL;
        pdev->mon_mpdu_skb_list_tail = NULL;
    }

    goto done;

fail:
    nbuf = pdev->mon_mpdu_skb_list_head;

    while (nbuf) {
        next = qdf_nbuf_next(nbuf);
        qdf_nbuf_free(nbuf);
        nbuf = next;
    }

    pdev->mon_mpdu_skb_list_head = NULL;
    pdev->mon_mpdu_skb_list_tail = NULL;

done:
    return;
}
qdf_export_symbol(osif_nss_vdev_aggregate_msdu);

/*
 * osif_nss_ol_be_vdev_handle_monitor_mode()
 *  handle monitor mode, returns false if packet is consumed
 */
#ifdef WLAN_FEATURE_FASTPATH
bool osif_nss_ol_be_vdev_handle_monitor_mode(struct net_device *netdev, struct sk_buff *skb, uint8_t is_chain)
{
    osif_dev  *osdev;
    struct ol_txrx_vdev_t *vdev;
    struct ieee80211vap *vap;
    struct ol_txrx_pdev_t *pdev;
    htt_pdev_handle htt_pdev;
    struct ol_ath_vap_net80211 *avn;
    struct ol_ath_softc_net80211 *scn = NULL;
    uint32_t is_mcast = 0, is_monitor = 0;
    qdf_nbuf_t skb_list_head = skb;
    qdf_nbuf_t skb_next = NULL;
    qdf_nbuf_t mon_skb = NULL;
    uint32_t rx_desc_size = 0;
    void *rx_mpdu_desc = NULL;
    struct ether_header *eh = NULL;
    struct ieee80211_frame *wh = NULL;
    uint32_t not_mgmt_type;
    uint32_t not_ctrl_type;

    osdev = ath_netdev_priv(netdev);
    vap = osdev->os_if;
    avn = OL_ATH_VAP_NET80211(vap);
    scn = avn->av_sc;
    vdev = wlan_vdev_get_dp_handle(osdev->ctrl_vdev);
    pdev = vdev->pdev;
    htt_pdev = scn->soc->htt_handle;

    if ((vdev->opmode == wlan_op_mode_monitor) || vap->iv_special_vap_mode || vap->iv_smart_monitor_vap) {
        is_monitor = 1;
    }

    /*
     * Skip monitor mode handling for chained MSDUs for now.
     * Freeing of buffers when restitch functins returns failure needs some special handling.
     * Current Host driver also doesnt support monitor mode for chained MSDUs
     */
    if (is_chain) {
        if (is_monitor) {
            /* Free the buffers and return , if restitch failed */
            mon_skb = skb;
            while (mon_skb) {
                skb_next = qdf_nbuf_next(mon_skb);
                qdf_nbuf_free(mon_skb);
                mon_skb = skb_next;
            }
            return false;
        }
        return true;
    }

    if (pdev->filter_neighbour_peers && vap->iv_smart_monitor_vap) {
        /* Deliver all non associated packets to smart
         * monitor vap.htt status check not needed, as
         * we might receive varied htt status and all
         * packets are passed to interface & not dropped
         */
        /*Filter based on pkt types to monitor VAP*/
    } else {

        /*
         * Always use the RA for peer lookup when vap is operating in 802.11 (raw) mode and expects 802.11 frames
         * because we always have to queue to corresponding  STA bss peer queue.
         *
         * In Normal mode (802.3) use the destination mac address from eth_hdr.
         */
        if (vdev->rx_decap_type == htt_pkt_type_raw) {
            wh = (struct ieee80211_frame *)skb->data;
            is_mcast = IEEE80211_IS_MULTICAST(wh->i_addr1);
        } else {
            eh = (struct ether_header *)(skb->data);
            is_mcast = IEEE80211_IS_MULTICAST(eh->ether_dhost);
        }

        /*
         * If monitor mode filters are not enabled for the given packet type, return from here for monitor mode VAP
         */
        not_mgmt_type = htt_pdev->ar_rx_ops->check_desc_mgmt_type(skb_list_head);
        not_ctrl_type = htt_pdev->ar_rx_ops->check_desc_ctrl_type(skb_list_head);

        if (likely((not_mgmt_type && not_ctrl_type) != 0)) {
            if ((is_mcast && (ol_txrx_monitor_get_filter_mcast_data((struct cdp_vdev *)vdev) == 1))
                    || ((!is_mcast) && (ol_txrx_monitor_get_filter_ucast_data((struct cdp_vdev *)vdev) == 1))) {

                /* if the vap mode is monitor mode, free the frame/chain of frames here and return  */
                if (is_monitor)  {
                    qdf_nbuf_free(skb);
                }
                goto skip_monitor;
            }
        } else {
            if (ol_txrx_monitor_get_filter_non_data((struct cdp_vdev *)vdev) == 1) {
                if (is_monitor) {
                    qdf_nbuf_free(skb);
                }
                goto skip_monitor;
            }
        }
    }

    rx_desc_size = htt_pdev->ar_rx_ops->sizeof_rx_desc();
    rx_mpdu_desc = (void *)skb->head;
    dma_unmap_single(scn->soc->nss_soc.dmadev, virt_to_phys(rx_mpdu_desc), rx_desc_size, DMA_FROM_DEVICE);

    /*
     * Form a chain of MSDUs which are part of one MPDU
     * Set no_clone_reqd flag if it is only for moniotr mode VAP
     */
    osif_nss_vdev_aggregate_msdu(htt_pdev->arh, pdev, skb, &skb_list_head, is_monitor);

    if (!skb_list_head) {
        goto skip_monitor;
    }

    mon_skb = htt_pdev->ar_rx_ops->restitch_mpdu_from_msdus(
            htt_pdev->arh, skb_list_head, pdev->rx_mon_recv_status, 1);

    if (mon_skb) {
        pdev->monitor_vdev->osif_rx_mon(pdev->monitor_vdev->osif_vdev, mon_skb, pdev->rx_mon_recv_status);
    } else {
        mon_skb = skb_list_head;
        while (mon_skb) {
            skb_next = qdf_nbuf_next(mon_skb);
            qdf_nbuf_free(mon_skb);
            mon_skb = skb_next;
        }
    }

skip_monitor:
    /* If only monitor mode , return from here */
    if (is_monitor) {
        return false;
    }

    return true;
}
#else
bool osif_nss_ol_be_vdev_handle_monitor_mode(struct net_device *netdev, struct sk_buff *skb, uint8_t is_chain)
{
    return true;
}
#endif

/*
 * osif_nss_be_vdev_set_peer_nexthop()
 *  Handler for set_peer_nexthop command
 */
int osif_nss_be_vdev_set_peer_nexthop(osif_dev *osif, uint8_t *addr, uint16_t if_num)
{
    qdf_print("Not supported!!");
    return 1;
}

/*
 * osif_nss_ol_be_vdev_spl_receive_exttx_compl()
 *  Handler for EXT TX Compl data packets exceptioned from WIFI
 */
void osif_nss_ol_be_vdev_spl_receive_exttx_compl(struct net_device *dev, struct sk_buff *skb, struct nss_wifi_vdev_tx_compl_metadata *tx_compl_metadata)
{
#if MESH_MODE_SUPPORT
    struct tx_capture_hdr *txhdr = NULL;
#endif
    struct ol_txrx_pdev_t *pdev;
    struct ol_txrx_vdev_t *vdev;
    osif_dev *osifp;

    osifp = netdev_priv(dev);
    vdev = (struct ol_txrx_vdev_t *)wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    pdev = vdev->pdev;

    if (!vdev->pdev->htt_pdev->htt_process_data_tx_frames){
	qdf_print("Drop frame !!.. HTT Function pointer NULL");
	qdf_nbuf_free(skb);
	return;
    }

#if MESH_MODE_SUPPORT
    if (qdf_nbuf_headroom(skb) < sizeof(struct tx_capture_hdr)) {
	qdf_print("Less Head room : Drop Frame!!");
	qdf_nbuf_free(skb);
	return;
    }

    qdf_nbuf_push_head(skb, sizeof(struct tx_capture_hdr));

    txhdr = (struct tx_capture_hdr *)qdf_nbuf_data(skb);
    qdf_mem_copy(txhdr->ta, tx_compl_metadata->ta, IEEE80211_ADDR_LEN);
    qdf_mem_copy(txhdr->ra, tx_compl_metadata->ra, IEEE80211_ADDR_LEN);
    txhdr->ppdu_id = tx_compl_metadata->ppdu_id;
#endif
    vdev->pdev->htt_pdev->htt_process_data_tx_frames(skb);
    return;
}

#if MESH_MODE_SUPPORT
extern void os_if_tx_free_ext(struct sk_buff *skb);
#endif

/*
 * osif_nss_ol_be_vdev_spl_receive_ext_mesh()
 *  Handler for EXT Mesh data packets exceptioned from WIFI
 */
void osif_nss_ol_be_vdev_spl_receive_ext_mesh(struct net_device *dev, struct sk_buff *skb, struct nss_wifi_vdev_mesh_per_packet_metadata *mesh_metadata)
{
    struct ol_txrx_pdev_t *pdev;
    struct ol_txrx_vdev_t *vdev;
    osif_dev *osifp;
    struct ieee80211vap *vap = NULL;
    struct ieee80211com* ic = NULL;
#if MESH_MODE_SUPPORT
    struct meta_hdr_s *mhdr = NULL;
#endif

    osifp = netdev_priv(dev);
    vdev = (struct ol_txrx_vdev_t *)wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    pdev = vdev->pdev;
    vap = osifp->os_if;
    ic = vap->iv_ic;

#if MESH_MODE_SUPPORT
    if (qdf_nbuf_headroom(skb) < sizeof(struct meta_hdr_s)) {
	qdf_print("Unable to accomodate mesh mode meta header");
	qdf_nbuf_free(skb);
	return;
    }

    qdf_nbuf_push_head(skb, sizeof(struct meta_hdr_s));

    mhdr = (struct meta_hdr_s *)qdf_nbuf_data(skb);
    mhdr->rssi =  mesh_metadata->rssi;
    mhdr->channel = ol_ath_mhz2ieee(ic, pdev->rx_mon_recv_status->rs_freq, 0);
    os_if_tx_free_ext(skb);
#else
    qdf_nbuf_free(skb);
#endif
    return;
}

/*
 * osif_nss_ol_be_vdev_data_receive_meshmode_rxinfo()
 *	Meshmode and Rxinfor part of Receive
 */
void osif_nss_ol_be_vdev_data_receive_meshmode_rxinfo(struct net_device *dev, struct sk_buff *skb)
{
    struct net_device *netdev;
    osif_dev  *osdev;
    struct ol_txrx_vdev_t *vdev = NULL;
    struct ieee80211vap *vap = NULL;
    struct ol_txrx_pdev_t *pdev = NULL;
    struct ieee80211com* ic = NULL;
    struct ol_ath_vap_net80211 *avn;
    struct ol_ath_softc_net80211 *scn;

#if MESH_MODE_SUPPORT
    uint16_t peer_id;
    struct ol_txrx_peer_t *peer = NULL;
    struct nss_wifi_vdev_meshmode_rx_metadata *mesh_metadata = NULL;
#endif

#if ATH_DATA_RX_INFO_EN || MESH_MODE_SUPPORT
    uint32_t rx_desc_size = 0;
    void *rx_mpdu_desc = NULL;
    htt_pdev_handle htt_pdev;
    qdf_nbuf_t msdu = (qdf_nbuf_t) skb;
#endif

    /*
     * Need to move this code to wifi driver
     */
    if(dev == NULL) {
        qdf_print(KERN_CRIT "%s , netdev is NULL, freeing skb", __func__);
        qdf_nbuf_free(skb);
        return;
    }

    netdev = (struct net_device *)dev;

    osdev = ath_netdev_priv(netdev);
    vap = osdev->os_if;
    avn = OL_ATH_VAP_NET80211(vap);
    scn = avn->av_sc;
    vdev = wlan_vdev_get_dp_handle(osdev->ctrl_vdev);
    ic = vap->iv_ic;
    pdev = vdev->pdev;

    if(pdev == NULL) {
        qdf_print(KERN_CRIT "%s , netdev is NULL, freeing skb", __func__);
        qdf_nbuf_free(skb);
        return;
    }

#if ATH_DATA_RX_INFO_EN || MESH_MODE_SUPPORT
    htt_pdev = pdev->htt_pdev;
    rx_desc_size = htt_pdev->ar_rx_ops->sizeof_rx_desc();
    rx_mpdu_desc = (void *)skb->head;
#endif

#if ATH_DATA_RX_INFO_EN
    if (vap->rxinfo_perpkt) {
        dma_unmap_single(scn->soc->nss_soc.dmadev, virt_to_phys(rx_mpdu_desc), rx_desc_size, DMA_FROM_DEVICE);
        htt_pdev->ar_rx_ops->update_pkt_info(htt_pdev->arh, msdu, NULL, 1);
        if (qdf_nbuf_get_rx_ftype(skb) == CB_FTYPE_RX_INFO) {
            osif_rx_status_dump((void *)qdf_nbuf_get_rx_fctx(skb));
        }
    }
#elif  MESH_MODE_SUPPORT
    if (vap->iv_mesh_vap_mode && (vap->mdbg & MESH_DEBUG_DUMP_STATS)) {
        dma_unmap_single(scn->soc->nss_soc.dmadev, virt_to_phys(rx_mpdu_desc), rx_desc_size, DMA_FROM_DEVICE);
        mesh_metadata = (struct nss_wifi_vdev_meshmode_rx_metadata *)skb->data;
        peer_id = mesh_metadata->peer_id;
        skb_pull(skb, sizeof(struct nss_wifi_vdev_meshmode_rx_metadata));
        peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle) pdev, peer_id);
        if (peer) {
            ol_rx_mesh_mode_per_pkt_rx_info(msdu, peer, vdev);
            if (qdf_nbuf_get_rx_ftype(skb) == CB_FTYPE_MESH_RX_INFO) {
                osif_rx_status_dump((void *)qdf_nbuf_get_rx_fctx(skb));
            }
        } else {
            qdf_print("No peer available ");
        }
    }
#endif

#if ATH_DATA_RX_INFO_EN
    if (qdf_nbuf_get_rx_ftype(skb) == CB_FTYPE_RX_INFO){
        qdf_mem_free((void *)qdf_nbuf_get_rx_fctx(skb));
        qdf_nbuf_set_rx_fctx_type(skb, 0, CB_FTYPE_INVALID);
    }
#elif MESH_MODE_SUPPORT
    if (qdf_nbuf_get_rx_ftype(skb) == CB_FTYPE_MESH_RX_INFO) {
        qdf_mem_free((void *)qdf_nbuf_get_rx_fctx(skb));
        qdf_nbuf_set_rx_fctx_type(skb, 0, CB_FTYPE_INVALID);
    }
#endif

    return;
}

/*
 * osif_nss_ol_be_vdev_spl_receive_ext_wdsdata()
 * Handler for EXT wds meta data
 */
bool osif_nss_ol_be_vdev_spl_receive_ext_wdsdata(struct net_device *dev, struct sk_buff *skb,
							struct nss_wifi_vdev_wds_per_packet_metadata *wds_metadata)
{
	return false;
}

/*
 * osif_nss_ol_be_vdev_spl_receive_ppdu_metadata
 * Handler for PPDU meta data
 */
bool osif_nss_ol_be_vdev_spl_receive_ppdu_metadata(struct net_device *dev, struct sk_buff *skb,
        struct nss_wifi_vdev_ppdu_metadata *ppdu_metadata)
{
    return true;
}

uint8_t osif_nss_ol_be_vdev_call_monitor_mode(struct net_device *netdev, osif_dev  *osdev, qdf_nbuf_t skb_list_head, uint8_t is_chain)
{
    struct ol_txrx_vdev_t *vdev = NULL;
    struct ieee80211vap *vap = NULL;
    struct ol_txrx_pdev_t *pdev = NULL;
    struct ieee80211com* ic = NULL;

    if ((!netdev) || (!osdev))
        return 1;

    if (!is_chain  && (qdf_nbuf_len(skb_list_head) == 0)) {
        qdf_nbuf_free(skb_list_head);
        return 1;
    }

    vap = osdev->os_if;
    vdev = wlan_vdev_get_dp_handle(osdev->ctrl_vdev);
    ic = vap->iv_ic;
    pdev = vdev->pdev;

    qdf_spin_lock_bh(&pdev->mon_mutex);
    if (pdev->monitor_vdev && ic->nss_iops) {
        if (!ic->nss_iops->ic_osif_nss_ol_vdev_handle_monitor_mode(netdev, skb_list_head, is_chain)) {
            qdf_spin_unlock_bh(&pdev->mon_mutex);
            return 1;
        }
    }
    qdf_spin_unlock_bh(&pdev->mon_mutex);
    return 0;
}

void osif_nss_ol_be_vdevcfg_set_offload_params(void * vdev, struct nss_wifi_vdev_config_msg **p_wifivdevcfg)
{
    struct nss_wifi_vdev_config_msg *wifivdevcfg = *p_wifivdevcfg;
    struct ol_txrx_vdev_t *vdev_handle = (struct ol_txrx_vdev_t *)vdev;
    uint32_t i;

    wifivdevcfg->vdev_id = vdev_handle->vdev_id;
    wifivdevcfg->opmode = vdev_handle->opmode;
    memcpy(wifivdevcfg->mac_addr,  &vdev_handle->mac_addr.raw[0], 6);

    wifivdevcfg->epid = vdev_handle->epid;
    wifivdevcfg->downloadlen = vdev_handle->downloadlen;
    wifivdevcfg->hdrcachelen = HTC_HTT_TRANSFER_HDRSIZE;
    memcpy(wifivdevcfg->hdrcache, vdev_handle->hdrcache, HTC_HTT_TRANSFER_HDRSIZE);
    osif_nss_info("Vdev config data: vdev_id : %d, epid : %d, downloadlen : %d\n",
            wifivdevcfg->vdev_id, wifivdevcfg->epid, wifivdevcfg->downloadlen);
    osif_nss_info("Sending hdr cache data of length :%d\n", wifivdevcfg->hdrcachelen);

    for ( i = 0; i < (wifivdevcfg->hdrcachelen) / 4; i++) {
        osif_nss_info("hdrcache[%d]:0x%x\n", i, wifivdevcfg->hdrcache[i]);
    }
}

void osif_nss_ol_be_get_peerid( struct MC_LIST_UPDATE* list_entry, uint32_t *peer_id)
{
    struct ol_txrx_peer_t *peer = NULL;
    peer = wlan_peer_get_dp_handle((list_entry->ni)->peer_obj);
    if (!peer)
        return;

    *peer_id = peer->peer_ids[0];
    return;
}

uint8_t osif_nss_ol_be_get_vdevid_fromvdev(void *vdev, uint8_t *vdev_id)
{
    struct ol_txrx_vdev_t *vdev_handle = (struct ol_txrx_vdev_t *)vdev;
    if (!vdev_handle)
        return 0;

    *vdev_id = vdev_handle->vdev_id;
    return 1;
}

uint8_t osif_nss_ol_be_get_vdevid_fromosif(osif_dev *osifp, uint8_t *vdev_id)
{
    struct ol_txrx_vdev_t *vdev = NULL;

    vdev = wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    if (!vdev) {
        return 0;
    }

    return (osif_nss_ol_be_get_vdevid_fromvdev(vdev, vdev_id));
}

/*
 * osif_nss_ol_be_peerid_find_hash_find()
 *  Get the peer_id using the hash index.
 */
uint32_t osif_nss_ol_be_peerid_find_hash_find(struct ieee80211vap *vap, uint8_t *peer_mac_addr, int mac_addr_is_aligned)
{
    struct ol_txrx_ast_entry_t *ast_entry = NULL;
    uint32_t peer_id;
    ol_txrx_vdev_handle vdev = (ol_txrx_vdev_handle) wlan_vdev_get_dp_handle(vap->vdev_obj);
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)(((struct ol_txrx_vdev_t *)vdev)->pdev);

    ast_entry = ol_txrx_ast_find_hash_find(pdev, peer_mac_addr, mac_addr_is_aligned);
    if (!(ast_entry)) {
        qdf_print("No ast entry found");
        return HTT_INVALID_PEER;
    }
    peer_id = ast_entry->peer_id;
    return peer_id;
}

/*
 * osif_nss_ol_be_peerid_find_hash_find()
 *  Get the peer_id using the hash index.
 */
void *osif_nss_ol_be_peer_find_hash_find(struct ieee80211vap *vap, uint8_t *peer_mac_addr, int mac_addr_is_aligned)
{
    struct ol_txrx_peer_t *peer = NULL;
#if ATH_SUPPORT_WRAP
    ol_txrx_vdev_handle vdev = (ol_txrx_vdev_handle)wlan_vdev_get_dp_handle(vap->vdev_obj);
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)(((struct ol_txrx_vdev_t *)vdev)->pdev);

    peer = ol_txrx_peer_find_hash_find(pdev, peer_mac_addr, mac_addr_is_aligned, ((struct ol_txrx_vdev_t *)vdev)->vdev_id);
#endif
    if (!(peer)) {
        qdf_print("No peer found\n");
        return NULL;
    }
    qdf_atomic_dec(&peer->ref_cnt);
    return peer;
}


void * osif_nss_ol_be_find_peer_by_id(osif_dev *osifp, uint32_t peer_id)
{
    struct ol_txrx_vdev_t *vdev = NULL;
    struct ol_txrx_pdev_t *pdev = NULL;
    struct ol_txrx_peer_t *peer;
    vdev = wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    if (!vdev) {
        return NULL;
    }
    pdev = (struct ol_txrx_pdev_t *)vdev->pdev;
    peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, peer_id);
    return ((void *)peer);
}

uint8_t osif_nss_ol_be_find_pstosif_by_id(struct net_device *netdev, uint32_t peer_id, osif_dev **psta_osifp)
{
    osif_dev *osifp;
    struct ol_txrx_vdev_t *vdev = NULL;
    struct ol_txrx_pdev_t *pdev = NULL;
    struct ol_txrx_vdev_t *pstavdev = NULL;
    struct ol_txrx_peer_t *pstapeer;

    osifp = netdev_priv(netdev);
    vdev = wlan_vdev_get_dp_handle(osifp->ctrl_vdev);

    if (!vdev) {
        return 0;
    }
    pdev = vdev->pdev;
    pstapeer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, peer_id);

    if (!pstapeer) {
        qdf_print("no peer available free packet ");
        return 0;
    }
    pstavdev = pstapeer->vdev;
    if (!pstavdev) {
        qdf_print("no vdev available free packet ");
        return 0;
    }
    *psta_osifp = (osif_dev *)pstavdev->osif_vdev;
    return 1;
}

/*
 * osif_nss_be_vap_updchdhdr()
 *	API for updating header cache in NSS.
 */
/*
 * Note: This function has been created seperately for 8064
 * and 8074 as there is no element called "hdrcache" in dp_vdev
 * (vdev->hdrcache).
 */
int32_t osif_nss_be_vap_updchdhdr(osif_dev *osifp)
{
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_updchdr_msg *wifivdevupdchdr;
    bool status;
    void *nss_wifiol_ctx;
    int32_t nss_ifnum;
    struct ieee80211vap *vap = NULL;
    struct ol_txrx_vdev_t * vdev = NULL;

    if (!osifp) {
        qdf_print("Invalid os dev passed");
	return -1;
    }

    nss_wifiol_ctx = osifp->nss_wifiol_ctx;
    if (!nss_wifiol_ctx) {
        return -1;
    }

    nss_ifnum = osifp->nss_ifnum;
    if (nss_ifnum == -1) {
        qdf_print(" vap transmit called on invalid interface");
        return -1;
    }
    vap = osifp->os_if;
    vdev = (struct ol_txrx_vdev_t *)wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    wifivdevupdchdr = &wifivdevmsg.msg.vdev_updchdr;
    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));

    wifivdevupdchdr->vdev_id = vdev->vdev_id;
    memcpy(wifivdevupdchdr->hdrcache, vdev->hdrcache, HTC_HTT_TRANSFER_HDRSIZE);

    nss_wifi_vdev_msg_init(&wifivdevmsg, nss_ifnum, NSS_WIFI_VDEV_UPDATECHDR_MSG,
            sizeof(struct nss_wifi_vdev_updchdr_msg), NULL, NULL);

    /*
     * Send the vdev configure message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_wifiol_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the vdev command message to NSS\n");
        return -1;
    }
    return 0;
}

/*
 * osif_nss_ol_be_vdev_set_cfg
 */
uint32_t osif_nss_ol_be_vdev_set_cfg(osif_dev *osifp, enum osif_nss_vdev_cmd osif_cmd)
{
    uint32_t val = 0;
    struct ol_txrx_vdev_t *vdev = NULL;

    if (!osifp) {
        return 0;
    }

    if (osifp->nss_ifnum == -1) {
        qdf_print(" vap transmit called on invalid interface");
        return 0;
    }

    vdev = (struct ol_txrx_vdev_t *)wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    switch (osif_cmd) {
        case OSIF_NSS_VDEV_DROP_UNENC:
            val = vdev->drop_unenc;
            break;

        case OSIF_NSS_WIFI_VDEV_NAWDS_MODE:
            val = vdev->nawds_enabled;
            break;

#ifdef WDS_VENDOR_EXTENSION
	    case OSIF_NSS_WIFI_VDEV_WDS_EXT_ENABLE:
            val = WDS_EXT_ENABLE;
            break;
#endif

        default:
            break;
    }
    return val;
}

/*
 * osif_nss_ol_be_vdev_get_stats
 */
uint8_t osif_nss_ol_be_vdev_get_stats(osif_dev *osifp, struct nss_cmn_msg *wifivdevmsg)
{
    struct nss_wifi_vdev_mcast_enhance_stats *nss_mestats = NULL;
    struct nss_wifi_vdev_stats_sync_msg *stats =
        (struct nss_wifi_vdev_stats_sync_msg *)&((struct nss_wifi_vdev_msg *)wifivdevmsg)->msg.vdev_stats;
    struct ol_txrx_vdev_t *vdev = NULL;
    struct cdp_tx_ingress_stats *txi_stats = NULL;
    struct cdp_tx_stats *tx_stats = NULL;
    struct cdp_rx_stats *rx_stats = NULL;
    struct ieee80211vap *vap = NULL;

    if (!osifp) {
        return 0;
    }

    vdev = (struct ol_txrx_vdev_t *)wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    nss_mestats = &stats->wvmes;
    txi_stats = &vdev->stats.tx_i;
    tx_stats = &vdev->stats.tx;
    rx_stats = &vdev->stats.rx;

    /*
     * Update the vdev stats
     */
    txi_stats->mcast_en.mcast_pkt.num +=nss_mestats->mcast_rcvd;
    txi_stats->mcast_en.mcast_pkt.bytes += nss_mestats->mcast_rcvd_bytes;
    txi_stats->mcast_en.ucast += nss_mestats->mcast_ucast_converted;
    txi_stats->mcast_en.fail_seg_alloc += nss_mestats->mcast_alloc_fail;
    txi_stats->mcast_en.dropped_send_fail += (nss_mestats->mcast_pbuf_enq_fail +
                        nss_mestats->mcast_pbuf_copy_fail + nss_mestats->mcast_peer_flow_ctrl_send_fail);
    txi_stats->mcast_en.dropped_self_mac += (nss_mestats->mcast_loopback_err +
                    nss_mestats->mcast_dst_address_err + nss_mestats->mcast_no_enhance_drop_cnt);

    txi_stats->rcvd.num += stats->tx_rcvd;
    txi_stats->rcvd.bytes += stats->tx_rcvd_bytes;
    txi_stats->processed.num += stats->tx_enqueue_cnt;
    txi_stats->processed.bytes += stats->tx_enqueue_bytes;
    txi_stats->dropped.ring_full += stats->tx_hw_ring_full;
    txi_stats->dropped.desc_na.num += stats->tx_desc_alloc_fail;
    txi_stats->dropped.dma_error += stats->tx_dma_map_fail;
    txi_stats->tso.tso_pkt.num += stats->tx_tso_pkt;
    txi_stats->tso.num_seg += stats->tx_num_seg;
    txi_stats->cce_classified += stats->cce_classified;
    txi_stats->cce_classified_raw += stats->cce_classified_raw;
    txi_stats->nawds_mcast.num += stats->nawds_tx_mcast_cnt;
    txi_stats->nawds_mcast.bytes += stats->nawds_tx_mcast_bytes;

    vap = osifp->os_if;

    if (qdf_unlikely(!vap)) {
        return 0;
    }

    if (qdf_unlikely(vap->iv_tx_encap_type == htt_cmn_pkt_type_raw)) {
        txi_stats->raw.raw_pkt.num += stats->tx_rcvd;
        txi_stats->raw.raw_pkt.bytes += stats->tx_rcvd_bytes;
    }


    if (vdev->pdev && !vdev->pdev->ap_stats_tx_cal_enable) {
        /*
         * Update net device statistics
         */
        rx_stats->to_stack.num += (stats->rx_enqueue_cnt + stats->rx_except_enqueue_cnt);
        rx_stats->to_stack.bytes += stats->rx_enqueue_bytes;
        rx_stats->rx_discard += (stats->rx_enqueue_fail_cnt + stats->rx_except_enqueue_fail_cnt);

        tx_stats->comp_pkt.num += (stats->tx_enqueue_cnt + stats->tx_intra_bss_enqueue_cnt);
        tx_stats->comp_pkt.bytes += stats->tx_enqueue_bytes;
        tx_stats->mcast.num += stats->tx_intra_bss_mcast_send_cnt;
        tx_stats->tx_failed += (stats->tx_enqueue_fail_cnt + stats->tx_intra_bss_enqueue_fail_cnt +
                stats->tx_intra_bss_mcast_send_fail_cnt);
        txi_stats->dropped.dropped_pkt.num = txi_stats->dropped.desc_na.num + txi_stats->dropped.ring_full +
            txi_stats->dropped.dma_error + tx_stats->tx_failed;
    }

    return 1;
}


#if ATH_SUPPORT_WRAP
uint8_t osif_nss_ol_be_vdev_get_nss_qwrap_en(struct ieee80211vap *vap)
{
    return 0;
}
#endif

/*
 * osif_nss_ol_be_vdev_qwrap_mec_check
 */
uint8_t osif_nss_ol_be_vdev_qwrap_mec_check(osif_dev *mpsta_osifp, struct sk_buff *skb)
{
	return 0;
}

/*
 * osif_nss_ol_be_vdev_data_receive_mec_check
 */
bool osif_nss_ol_be_vdev_data_receive_mec_check(osif_dev *osdev, struct sk_buff *skb)
{
	return false;
}

#ifdef WDS_VENDOR_EXTENSION
/*
 * osif_nss_wifili_vdev_get_wds_peer()
 *  Get the peer pointer from the vdev list.
 *  This is used for wds vendor extension.
 */
void *osif_nss_ol_be_vdev_get_wds_peer(void *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev =  (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_peer_t *peer = NULL;

    if (!vdev) {
	    qdf_print("Vdev handle is NULL");
        return NULL;
    }

    if (vdev->opmode == wlan_op_mode_ap) {
        /* for ap, set it on bss_peer */
        TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
            if (peer->bss_peer) {
                break;
            }
        }
    } else if (vdev->opmode == wlan_op_mode_sta) {
        peer = TAILQ_FIRST(&vdev->peer_list);
    }

    return (void *)peer;
}
#endif /* WDS_VENDOR_EXTENSION */

