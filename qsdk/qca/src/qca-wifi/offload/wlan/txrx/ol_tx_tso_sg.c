/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <qdf_nbuf.h>         /* qdf_nbuf_t, etc. */
#include <ol_tx_desc.h>       /* ol_tx_desc */
#include <ol_tx_send.h>       /* ol_tx_send */
#include <ol_txrx_api.h>       /* ol_tx_send */
#include <enet.h>
#include <ol_txrx.h>

#include <AR900B/hw/datastruct/msdu_link_ext.h>

#define TSO_SG_DBG 0

static inline struct ol_tx_tso_desc_t *
ol_tx_tso_sg_desc_alloc(struct ol_txrx_pdev_t *pdev)
{
    struct ol_tx_tso_desc_t *tx_tso_desc = NULL;

    qdf_spin_lock_bh(&pdev->tx_tso_mutex);
    if (qdf_likely(pdev->tx_tso_desc.freelist)) {
        tx_tso_desc = &pdev->tx_tso_desc.freelist->tx_tso_desc;
        pdev->tx_tso_desc.freelist = pdev->tx_tso_desc.freelist->next;
        pdev->pdev_stats.tso_desc_cnt++;
    }
    qdf_spin_unlock_bh(&pdev->tx_tso_mutex);

    return tx_tso_desc;
}


void
ol_tx_tso_sg_desc_display(void *tx_desc)
{
    struct ol_tx_tso_desc_t *tx_tso_sg_desc = (struct ol_tx_tso_desc_t *)tx_desc;
    int count;

    if(!tx_tso_sg_desc) return;
    qdf_print("==========TSO SG Descriptor==============>");
    qdf_print("tx_tso_sg_desc %pK ", tx_tso_sg_desc);
    qdf_print("HOST_TSO_SG::l2_length %x",tx_tso_sg_desc->l2_length);
    qdf_print("HOST_TSO_SG::l3_hdr_length %x",tx_tso_sg_desc->l3_hdr_size);
    qdf_print("HOST_TSO_SG::l2_l3_l4_hdr_size %x",tx_tso_sg_desc->l2_l3_l4_hdr_size);
    qdf_print("HOST_TSO_SG::last_segment %x",tx_tso_sg_desc->last_segment);
    qdf_print("HOST_TSO_SG::data_len %x",tx_tso_sg_desc->data_len);
    qdf_print("HOST_TSO_SG::segment_count %x",tx_tso_sg_desc->segment_count);

    for (count = 0; count < tx_tso_sg_desc->segment_count; count++)
        qdf_print("seg addr[%d]: %x, seg_len: %d", count, tx_tso_sg_desc->seg_paddr_lo[count], tx_tso_sg_desc->seg_len[count]);
}

void
ol_tx_msdu_desc_display(void *msdu_desc)
{
    struct msdu_link_ext *msdu_link_ext_p = (struct msdu_link_ext *)msdu_desc;

    if(msdu_link_ext_p) {
        qdf_print("===========MSDU Link extension descriptor======");
        qdf_print("HOST_TSO_SG::tso_enable %x",msdu_link_ext_p->tso_enable);
        qdf_print("HOST_TSO_SG::tcp_flag %x",msdu_link_ext_p->tcp_flag);
        qdf_print("HOST_TSO_SG::tcp_flag_mask %x",msdu_link_ext_p->tcp_flag_mask);
        qdf_print("HOST_TSO_SG::l2_length %x",msdu_link_ext_p->l2_length);
        qdf_print("HOST_TSO_SG::ip_length %x",msdu_link_ext_p->ip_length);
        qdf_print("HOST_TSO_SG::tcp_seq_number %x",msdu_link_ext_p->tcp_seq_number);
        qdf_print("HOST_TSO_SG::ip_identification %x",msdu_link_ext_p->ip_identification);
        qdf_print("HOST_TSO_SG::ipv4_checksum_en %x",msdu_link_ext_p->ipv4_checksum_en);
        qdf_print("HOST_TSO_SG::udp_over_ipv4_checksum_en %x",msdu_link_ext_p->udp_over_ipv4_checksum_en);
        qdf_print("HOST_TSO_SG::udp_over_ipv6_checksum_en %x",msdu_link_ext_p->udp_over_ipv6_checksum_en);
        qdf_print("HOST_TSO_SG::tcp_over_ipv4_checksum_en %x",msdu_link_ext_p->tcp_over_ipv4_checksum_en);
        qdf_print("HOST_TSO_SG::tcp_over_ipv6_checksum_en %x",msdu_link_ext_p->tcp_over_ipv6_checksum_en);
        qdf_print("HOST_TSO_SG::buf0_ptr_31_0 %x",msdu_link_ext_p->buf0_ptr_31_0);
        qdf_print("HOST_TSO_SG::buf0_len %x",msdu_link_ext_p->buf0_len);
        qdf_print("HOST_TSO_SG::buf1_ptr_31_0 %x",msdu_link_ext_p->buf1_ptr_31_0);
        qdf_print("HOST_TSO_SG::buf1_len %x",msdu_link_ext_p->buf1_len);
        qdf_print("HOST_TSO_SG::buf2_ptr_31_0 %x",msdu_link_ext_p->buf2_ptr_31_0);
        qdf_print("HOST_TSO_SG::buf2_len %x",msdu_link_ext_p->buf2_len);
        qdf_print("HOST_TSO_SG::buf3_ptr_31_0 %x",msdu_link_ext_p->buf3_ptr_31_0);
        qdf_print("HOST_TSO_SG::buf3_len %x",msdu_link_ext_p->buf3_len);
        qdf_print("HOST_TSO_SG::buf4_ptr_31_0 %x",msdu_link_ext_p->buf4_ptr_31_0);
        qdf_print("HOST_TSO_SG::buf4_len %x",msdu_link_ext_p->buf4_len);
        qdf_print("HOST_TSO_SG::buf5_ptr_31_0 %x",msdu_link_ext_p->buf5_ptr_31_0);
        qdf_print("HOST_TSO_SG::buf5_len %x",msdu_link_ext_p->buf5_len);
        qdf_print("--------> ENd of MSDU Desc ");
    }
}

void
ol_tx_print_tso_stats(
   void *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    qdf_warn("++++++++++ TSO SG STATISTICS +++++++++++\n");
    qdf_warn("TSO SG TX Pkts       : %u\n", pdev->pdev_stats.tx_i.tso.tso_pkt.num);
    qdf_warn("TSO SG TX Bytes      : %llu\n", pdev->pdev_stats.tx_i.tso.tso_pkt.bytes);
    qdf_warn("Non-TSO SG TX Pkts   : %u\n", pdev->pdev_stats.tx_i.tso.non_tso_pkts.num);
    qdf_warn("Non-TSO SG TX Bytes  : %llu\n", pdev->pdev_stats.tx_i.tso.non_tso_pkts.bytes);
    qdf_warn("TSO SG Dropped Pkts  : %u\n", pdev->pdev_stats.tx_i.tso.dropped_host.num);
    qdf_warn("TSO SG Dropped Bytes : %llu\n", pdev->pdev_stats.tx_i.tso.dropped_host.bytes);
    qdf_warn("TSO SG Desc Total %d In Use : %d\n", pdev->tx_tso_desc.pool_size, \
            pdev->pdev_stats.tso_desc_cnt);

}

void
ol_tx_rst_tso_stats(void *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    qdf_print(".....Resetting TSO SG Stats ");
    pdev->pdev_stats.tx_i.tso.tso_pkt.num = 0;
    pdev->pdev_stats.tx_i.tso.tso_pkt.bytes = 0;
    pdev->pdev_stats.tx_i.tso.non_tso_pkts.num = 0;
    pdev->pdev_stats.tx_i.tso.non_tso_pkts.bytes = 0;
    pdev->pdev_stats.tx_i.tso.dropped_host.num = 0;
    pdev->pdev_stats.tx_i.tso.dropped_host.bytes = 0;
    qdf_warn("tso_stats %u %llu %u %llu \n", pdev->pdev_stats.tx_i.tso.tso_pkt.num,
        pdev->pdev_stats.tx_i.tso.tso_pkt.bytes, pdev->pdev_stats.tx_i.tso.non_tso_pkts.num,
        pdev->pdev_stats.tx_i.tso.non_tso_pkts.bytes);
}

/*  Prepares TX Descriptor per TCP Segment for TSO Packets
 *  Fills MSDU Link Ext descriptor with TSO details */
void
ol_tx_tso_sg_desc_prepare(
    struct ol_txrx_pdev_t *pdev,
    struct ol_tx_desc_t *tx_desc,
    struct ol_tx_tso_desc_t *sw_tso_sg_desc)
{
    A_UINT16 v6_opt_len;
    struct msdu_link_ext *msdu_link_ext_p;

#if TSO_SG_DBG
    ol_tx_tso_sg_desc_display(sw_tso_sg_desc);
#endif

#define EXT_P_FLAG_FIN  0x1
#define EXT_P_FLAG_SYN  0x2
#define EXT_P_FLAG_RST  0x4
#define EXT_P_FLAG_PSH  0x8
#define EXT_P_FLAG_ACK  0x10
#define EXT_P_FLAG_URG  0x20
#define EXT_P_FLAG_ECE  0x40
#define EXT_P_FLAG_CWR  0x80
#define EXT_P_FLAG_NS   0x100
#define IPV6_STD_HDR_SIZE   0x28
#define TSO_TCP_FLAG_MASK   0x1ff /* All 1's for 9 of TCP flags */

    /* Now, fill in the msdu_link_ext structure */
    msdu_link_ext_p = (struct msdu_link_ext *)tx_desc->htt_frag_desc;

    (*(u_int32_t *)msdu_link_ext_p) |= (1 << MSDU_LINK_EXT_0_TSO_ENABLE_OFFSET);
    (*(u_int32_t *)msdu_link_ext_p) &= ~(MSDU_LINK_EXT_0_TCP_FLAG_MASK);

    if (sw_tso_sg_desc->fin && sw_tso_sg_desc->last_segment) {
        /* Set fin in last segment, if set in original */
	(*(u_int32_t *)msdu_link_ext_p) |= (EXT_P_FLAG_FIN << MSDU_LINK_EXT_0_TCP_FLAG_LSB);
    }
    if (sw_tso_sg_desc->psh) {
	(*(u_int32_t *)msdu_link_ext_p) |= (EXT_P_FLAG_PSH << MSDU_LINK_EXT_0_TCP_FLAG_LSB);
    }
    if (sw_tso_sg_desc->ack) {
	(*(u_int32_t *)msdu_link_ext_p) |= (EXT_P_FLAG_ACK << MSDU_LINK_EXT_0_TCP_FLAG_LSB);
    }

    (*(u_int32_t *)msdu_link_ext_p) |= (TSO_TCP_FLAG_MASK << MSDU_LINK_EXT_0_TCP_FLAG_MASK_LSB);
    (*(((u_int32_t *)msdu_link_ext_p)+1)) |= (sw_tso_sg_desc->l2_length << MSDU_LINK_EXT_1_L2_LENGTH_LSB);


    /* Fill IP Length and ip_id only incase of IPV4 */
    if(sw_tso_sg_desc->ip_hdr_type == TSO_IPV4)
    {
        /* IPV4 Length includes data length + l3 + l4 */
        (*(((u_int32_t *)msdu_link_ext_p)+1)) |= ((sw_tso_sg_desc->data_len +
                                                sw_tso_sg_desc->l2_l3_l4_hdr_size -
                                                sw_tso_sg_desc->l2_length) << MSDU_LINK_EXT_1_IP_LENGTH_LSB);

        (*(((u_int32_t *)msdu_link_ext_p)+3)) |= (sw_tso_sg_desc->ipv4_id << MSDU_LINK_EXT_3_IP_IDENTIFICATION_LSB);
        (*(((u_int32_t *)msdu_link_ext_p)+3)) |= (1 << MSDU_LINK_EXT_3_IPV4_CHECKSUM_EN_LSB);
        (*(((u_int32_t *)msdu_link_ext_p)+3)) |= (1 << MSDU_LINK_EXT_3_TCP_OVER_IPV4_CHECKSUM_EN_LSB);

        msdu_link_ext_p->ip_identification = sw_tso_sg_desc->ipv4_id;
    }
    else
    {
        /* IPV6 Length includes data length + l4 */
        /* IPV6 length should include IPV6 ext. header length also, extracting as follows.... */
        v6_opt_len = sw_tso_sg_desc->l3_hdr_size - IPV6_STD_HDR_SIZE;

        (*(((u_int32_t *)msdu_link_ext_p)+1)) |= ((sw_tso_sg_desc->data_len +
                                                sw_tso_sg_desc->l2_l3_l4_hdr_size -
                                                sw_tso_sg_desc->l2_length -
                                                sw_tso_sg_desc->l3_hdr_size + v6_opt_len) << MSDU_LINK_EXT_1_IP_LENGTH_LSB);

        (*(((u_int32_t *)msdu_link_ext_p)+3)) |= (1 << MSDU_LINK_EXT_3_TCP_OVER_IPV6_CHECKSUM_EN_LSB);
    }

    (*(((u_int32_t *)msdu_link_ext_p)+2)) |= (sw_tso_sg_desc->tcp_seq << MSDU_LINK_EXT_2_TCP_SEQ_NUMBER_LSB);
    msdu_link_ext_p->tcp_seq_number = sw_tso_sg_desc->tcp_seq;

#if TSO_SG_DBG
    qdf_print("Seq No. %u ",msdu_link_ext_p->tcp_seq_number);
#endif

#if TSO_SG_DBG
    qdf_print("Freeing TX TSo Descriptor %x",sw_tso_sg_desc);
#endif
    ol_tx_tso_sg_desc_free(pdev, sw_tso_sg_desc);

#if TSO_SG_DBG
    ol_tx_msdu_desc_display(msdu_link_ext_p);
#endif
    return;
}

static struct ol_tx_tso_desc_t *
ol_tx_tso_sg_sw_desc_alloc_fill(qdf_nbuf_t msdu,
                             struct ol_txrx_pdev_t *pdev)
{
    struct ol_tx_tso_desc_t *sw_tso_sg_desc = NULL;

    A_UINT8  *ip6hdr = NULL;
    qdf_net_iphdr_t *iphdr = NULL; /* Ip header ptr */
    qdf_net_tcphdr_t *tcphdr;      /* Tcp header ptr */

    /* Allocate SW TSO Descriptor for storing parsed info. */
    sw_tso_sg_desc = ol_tx_tso_sg_desc_alloc(pdev);
    if (qdf_unlikely(!sw_tso_sg_desc)) {
        qdf_print("HOST_TSO_SG:Dropping Pkt->SW TSO Descriptor allocation \
                      failed ");
        return NULL;
    }
    sw_tso_sg_desc->last_segment = 0;
    sw_tso_sg_desc->data_len = 0;

    tcphdr = (qdf_net_tcphdr_t *)(qdf_nbuf_transport_header(msdu));

    if (qdf_nbuf_tso_tcp_v4(msdu))  {
        iphdr = (qdf_net_iphdr_t *)(qdf_nbuf_network_header(msdu));
        sw_tso_sg_desc->ipv4_id = qdf_ntohs(iphdr->ip_id);
        sw_tso_sg_desc->ip_hdr_type = TSO_IPV4;
        sw_tso_sg_desc->l3_hdr_size = ((A_UINT8 *)tcphdr - (A_UINT8 *)iphdr);
        sw_tso_sg_desc->l2_length = ((A_UINT8 *)iphdr - qdf_nbuf_data(msdu));
    } else if (qdf_nbuf_tso_tcp_v6(msdu)) {
      /* For IPV6 */
        ip6hdr = (A_UINT8 *)(qdf_nbuf_network_header(msdu));
        sw_tso_sg_desc->ip_hdr_type = TSO_IPV6;
        sw_tso_sg_desc->l3_hdr_size = ((A_UINT8 *)tcphdr - (A_UINT8 *)ip6hdr);
        sw_tso_sg_desc->l2_length = ((A_UINT8 *)ip6hdr - qdf_nbuf_data(msdu));
    }

    sw_tso_sg_desc->mss_size = qdf_nbuf_tcp_tso_size(msdu);
    sw_tso_sg_desc->fin = tcphdr->fin;
    sw_tso_sg_desc->syn = tcphdr->syn;
    sw_tso_sg_desc->rst = tcphdr->rst;
    sw_tso_sg_desc->psh = tcphdr->psh;
    sw_tso_sg_desc->ack = tcphdr->ack;
    sw_tso_sg_desc->urg = tcphdr->urg;
    sw_tso_sg_desc->ece = tcphdr->ece;
    sw_tso_sg_desc->cwr = tcphdr->cwr;
    sw_tso_sg_desc->tcp_seq = qdf_nbuf_tcp_seq(msdu);

    /* tcphdr->doff is number of 4-byte words in tcp header */
    sw_tso_sg_desc->l2_l3_l4_hdr_size = qdf_nbuf_l2l3l4_hdr_len(msdu);  /* Number of bytes to payload start */

    if(QDF_STATUS_E_FAILURE == qdf_nbuf_map_nbytes_single(pdev->osdev, msdu,
                                                     QDF_DMA_TO_DEVICE,
						     qdf_nbuf_headlen(msdu))) {
        qdf_print("HOST_TSO_SG:Dropping Pkt, failed to map fragment buffer of msdu ");
	ol_tx_tso_sg_desc_free(pdev, sw_tso_sg_desc);
	return NULL;
    }

    sw_tso_sg_desc->seg_paddr_lo[0] = qdf_nbuf_get_frag_paddr(msdu, 0);
    sw_tso_sg_desc->seg_paddr_hi[0] = 0x0;
    sw_tso_sg_desc->seg_len[0] = sw_tso_sg_desc->l2_l3_l4_hdr_size;

   /* SW TSO descriptor is allocated and filled, ready to be returned */
   return sw_tso_sg_desc;
}



inline int
ol_tx_tso_sg_process_skb(ol_txrx_vdev_handle vdev,qdf_nbuf_t msdu)
{
    A_UINT16 total_payload_len, fragment_len, segment_count = 1;
    A_UINT32 segment_space, mss_size, cur_frag = 0;
    A_UINT32 skb_data_verified = 0;
    A_UINT32 fragment_paddr;
    A_UINT32 ret = 0;

    struct ol_txrx_pdev_t *pdev = ((struct ol_txrx_vdev_t *)vdev)->pdev;
    struct ol_tx_tso_desc_t *sw_tso_sg_desc = NULL;
    struct ol_tx_tso_desc_t *seg_tso_sg_desc = NULL;
    qdf_nbuf_t seg_msdu;

    TXRX_VDEV_STATS_MSDU_INCR((struct ol_txrx_vdev_t *)vdev, tx_i.tso.tso_pkt, msdu);

    sw_tso_sg_desc = ol_tx_tso_sg_sw_desc_alloc_fill(msdu, pdev);
    if (sw_tso_sg_desc == NULL) {
        qdf_nbuf_free(msdu);
        return -ENOMEM;
    }
    total_payload_len = qdf_nbuf_len(msdu) - sw_tso_sg_desc->l2_l3_l4_hdr_size;
    mss_size = sw_tso_sg_desc->mss_size;
    segment_space = sw_tso_sg_desc->mss_size;
    qdf_nbuf_set_tx_fctx_type(msdu, (void *)sw_tso_sg_desc, CB_FTYPE_TSO_SG);

    do {
        if (!skb_data_verified && (qdf_nbuf_headlen(msdu) > sw_tso_sg_desc->l2_l3_l4_hdr_size)) {
            /* We cannot assume the linear buffer of skb only has headers and no payload
             * for this reason we need to check for payload in linear buffer and if found
             * The payload always starts after the l2, l3 and l4 header.
             */
            fragment_len = qdf_nbuf_headlen(msdu) - sw_tso_sg_desc->l2_l3_l4_hdr_size;
            fragment_paddr = sw_tso_sg_desc->seg_paddr_lo[0] + sw_tso_sg_desc->l2_l3_l4_hdr_size;

            total_payload_len -= fragment_len;
            skb_data_verified = 1;
        } else {
            /* once we have verified the linear buffer of skb for payload, we now look
             * for data in the fragments starting from first fragment.
             */
            fragment_len = qdf_nbuf_get_frag_size(msdu, cur_frag);
            if (QDF_STATUS_E_FAILURE == qdf_nbuf_frag_map(pdev->osdev,
                                                     msdu,
                                                     0,
                                                     QDF_DMA_TO_DEVICE,
                                                     cur_frag)) {
                qdf_print("HOST_TSO_SG:Dropping Pkt, failed to map fragment buffer of msdu ");
                ol_tx_tso_sg_desc_free(pdev, (struct ol_tx_tso_desc_t *)qdf_nbuf_get_tx_fctx(msdu));
                qdf_nbuf_free(msdu);
                return -EIO;
                }
            fragment_paddr = qdf_nbuf_get_tx_frag_paddr(msdu);
            total_payload_len -= fragment_len;
            cur_frag++;
        }
        /* once we have the fragment dma-able address and length, we need to create
         * segments of these fragments which are no bigger then MSS.
         * The below code will address two scenarios.
         * Scenario - 1: the length of the fragment is more then the MSS. In this case
         * we store the fragment dma-able address and MSS  in the sw_tso_sg_desc segment list
         * and give it to next level for TX.
         * Scenario - 2: The length of the fragment is less then the MSS. In this case
         * we store the fragment dma-able address and fragment length in the sw_tso_sg_desc
         * segment list and read the next fragment dma-able address and next fragment length
         * till the time our segment is equal to MSS or we reach end of the payload.
         */
        do {
            if (fragment_len < mss_size) {
                mss_size = fragment_len;
            }
            sw_tso_sg_desc->seg_paddr_lo[segment_count] = (unsigned int)fragment_paddr;
            sw_tso_sg_desc->seg_paddr_hi[segment_count] = 0x0;
            sw_tso_sg_desc->seg_len[segment_count] = mss_size;

            fragment_paddr += mss_size;
            fragment_len -= mss_size;
            segment_count++;

            if ((total_payload_len == 0) && (fragment_len == 0)) {
                segment_space = mss_size;
            }

            segment_space -= mss_size;
            sw_tso_sg_desc->data_len += mss_size;

            if (segment_space == 0 || segment_count >= MAX_FRAGS) {
            /* if no more space left in the segment or we reached max
             * frags hardware can handle per segment then send the msdu
             * to next level for TX
             */
                segment_space = sw_tso_sg_desc->mss_size;
                mss_size = sw_tso_sg_desc->mss_size;
                sw_tso_sg_desc->segment_count = segment_count;
                if ((total_payload_len == 0) && (fragment_len == 0)) {
                    sw_tso_sg_desc->last_segment = 1;
                }

                qdf_nbuf_num_frags_init(msdu);
                if (!sw_tso_sg_desc->last_segment) {
                    if(!(seg_tso_sg_desc = ol_tx_tso_sg_desc_alloc(pdev))) {
                        qdf_print("HOST_TSO_SG:Dropping Pkt->SW TSO Descriptor allocation \
                                failed ");
                        ol_tx_tso_sg_desc_free(pdev,(struct ol_tx_tso_desc_t *) qdf_nbuf_get_tx_fctx(msdu));
                        qdf_nbuf_free(msdu);
                        return -ENOMEM;
                    }
                    if(!(seg_msdu = qdf_nbuf_clone(msdu))) {
                        ol_tx_tso_sg_desc_free(pdev, seg_tso_sg_desc);
                        ol_tx_tso_sg_desc_free(pdev,(struct ol_tx_tso_desc_t *) qdf_nbuf_get_tx_fctx(msdu));
                        qdf_nbuf_free(msdu);
                        return -ENOMEM;
                    }
                    memcpy(seg_tso_sg_desc, sw_tso_sg_desc, sizeof(struct ol_tx_tso_desc_t));
                    qdf_nbuf_set_tx_fctx_type(seg_msdu, (void *)seg_tso_sg_desc, CB_FTYPE_TSO_SG);
                    sw_tso_sg_desc->ipv4_id += 1;
                    sw_tso_sg_desc->tcp_seq += sw_tso_sg_desc->data_len;
                    sw_tso_sg_desc->data_len = 0;
                    ret = OL_TX_LL_FORWARD_PACKET(vdev, seg_msdu, 1);
                } else {
                    ret = OL_TX_LL_FORWARD_PACKET(vdev, msdu, 1);
                }
#if !PEER_FLOW_CONTROL
                if (qdf_unlikely(ret)) {
                    qdf_nbuf_free(seg_msdu);
                }
#endif
                segment_count = 1;
            } else {
                /* if there is still space left in the segment then
                 * read the next fragment and fill up this space.
                 */
                mss_size = segment_space;
            }

        } while (fragment_len);
    } while(total_payload_len);
    return ret;
}


