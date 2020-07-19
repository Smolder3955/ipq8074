/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <qdf_nbuf.h>         /* qdf_nbuf_t, etc. */

#include <ol_txrx_internal.h> /* TXRX_ASSERT1 */
#include <ol_txrx_types.h>    /* pdev stats */
#include <ol_tx_desc.h>       /* ol_tx_desc */
#include <ol_tx_send.h>       /* ol_tx_send */
#include <ol_txrx_api.h>       /* ol_tx_send */
#include <enet.h>


#include <AR900B/hw/datastruct/msdu_link_ext.h>

#define TSO_DBG 0

#ifndef IS_ETHERTYPE
#define IS_ETHERTYPE(_typeOrLen) ((_typeOrLen) >= 0x0600)
#endif

static inline struct ol_tx_tso_desc_t *
ol_tx_tso_desc_alloc(struct ol_txrx_pdev_t *pdev)
{
    struct ol_tx_tso_desc_t *tx_tso_desc = NULL;
    struct ol_txrx_stats *stats = &pdev->stats.pub;

    qdf_spin_lock_bh(&pdev->tx_tso_mutex);
    if (qdf_likely(pdev->tx_tso_desc.freelist)) {
        tx_tso_desc = &pdev->tx_tso_desc.freelist->tx_tso_desc;
        pdev->tx_tso_desc.freelist = pdev->tx_tso_desc.freelist->next;
        stats->tx.tso.tso_desc_cnt++;
        stats->pdev_stats.tx_i.tso.tso_desc_cnt++;
    }
    qdf_spin_unlock_bh(&pdev->tx_tso_mutex);

    return tx_tso_desc;
}
static inline void
ol_tx_tso_desc_free(struct ol_txrx_pdev_t *pdev,
                    struct ol_tx_tso_desc_t *tx_tso_desc)
{
    struct ol_txrx_stats *stats = &pdev->stats.pub;
    qdf_spin_lock_bh(&pdev->tx_tso_mutex);
    ((union ol_tx_tso_desc_list_elem_t *) tx_tso_desc)->next =
                                      pdev->tx_tso_desc.freelist;
    pdev->tx_tso_desc.freelist =
                (union ol_tx_tso_desc_list_elem_t *) tx_tso_desc;
    stats->tx.tso.tso_desc_cnt--;
    stats->pdev_stats.tx_i.tso.tso_desc_cnt--;
    qdf_spin_unlock_bh(&pdev->tx_tso_mutex);
}

#define link_segment(head, tail, seg) \
    do { \
        if (head) { \
            qdf_nbuf_set_next(tail, seg); \
            tail = seg; \
        } else { \
            head = tail = seg; \
        } \
        qdf_nbuf_set_next(tail, NULL); \
    } while (0)

void
ol_tx_tso_failure_cleanup( struct ol_txrx_pdev_t *pdev, qdf_nbuf_t segment_list)
{
    qdf_nbuf_t tso_header_nbuf;
    struct ol_tx_tso_desc_t *tx_tso_desc=NULL;

    /* The list of network buffers will be freed by the caller at OS Shim */
    /* Only free the additional Header buffer and SW TSO descriptor */

    tso_header_nbuf = qdf_nbuf_get_parent(segment_list);
    if (tso_header_nbuf) {
        /* SW TSO Descriptor is stored as part of frag[0]
           inside header buffer */
        tx_tso_desc =
          (struct ol_tx_tso_desc_t *)QDF_NBUF_CB_TX_EXTRA_FRAG_VADDR(tso_header_nbuf,0);
        if (tx_tso_desc) ol_tx_tso_desc_free(pdev,tx_tso_desc);
        qdf_nbuf_free(tso_header_nbuf);
#if TSO_DBG
        qdf_print("HOST_TSO::Dropping Pkt->tso_failure_cleanup ");
#endif
    } else {
#if TSO_DBG
        qdf_print("HOST_TSO::Dropping Pkt->tso_failure_cleanup  \
                      Header buffer is NULL ");
#endif
        return;
    }
}

void
ol_tx_tso_desc_display(void *tx_desc)
{
    struct ol_tx_tso_desc_t *tx_tso_desc = (struct ol_tx_tso_desc_t *)tx_desc;

    if(!tx_tso_desc) return;

    qdf_print("==========TSO Descriptor==============>");
    qdf_print("tx_tso_desc %x ",(A_UINT32 )tx_tso_desc);
    qdf_print("HOST_TSO::tso_final_segment %x",(A_UINT32 )tx_tso_desc->tso_final_segment);
    qdf_print("HOST_TSO::ip_hdr_type %x",(A_UINT32 )tx_tso_desc->ip_hdr_type);
    if(tx_tso_desc->ip_hdr_type == TSO_IPV4)
        qdf_print("HOST_TSO::iphdr %x",(A_UINT32 )(tx_tso_desc->iphdr));
    else
        qdf_print("HOST_TSO::ip6hdr %x",(A_UINT32 )(tx_tso_desc->iphdr));

    qdf_print("HOST_TSO::tcphdr %x",(A_UINT32 )tx_tso_desc->tcphdr);
    qdf_print("HOST_TSO::l2_length %x",tx_tso_desc->l2_length);
    qdf_print("HOST_TSO::l3_hdr_length %x",tx_tso_desc->l3_hdr_size);
    qdf_print("HOST_TSO::l2_l3_l4_hdr_size %x",tx_tso_desc->l2_l3_l4_hdr_size);
    qdf_print("HOST_TSO::ref_cnt %x",tx_tso_desc->ref_cnt);
}

void
ol_tx_msdu_desc_display(void *msdu_desc)
{
    struct msdu_link_ext *msdu_link_ext_p = (struct msdu_link_ext *)msdu_desc;

    qdf_print("===========MSDU Link extension descriptor======");
    qdf_print("HOST_TSO::tso_enable %x",msdu_link_ext_p->tso_enable);
    qdf_print("HOST_TSO::tcp_flag %x",msdu_link_ext_p->tcp_flag);
    qdf_print("HOST_TSO::tcp_flag_mask %x",msdu_link_ext_p->tcp_flag_mask);
    qdf_print("HOST_TSO::l2_length %x",msdu_link_ext_p->l2_length);
    qdf_print("HOST_TSO::ip_length %x",msdu_link_ext_p->ip_length);
    qdf_print("HOST_TSO::tcp_seq_number %x",msdu_link_ext_p->tcp_seq_number);
    qdf_print("HOST_TSO::ip_identification %x",msdu_link_ext_p->ip_identification);
    qdf_print("HOST_TSO::ipv4_checksum_en %x",msdu_link_ext_p->ipv4_checksum_en);
    qdf_print("HOST_TSO::udp_over_ipv4_checksum_en %x",msdu_link_ext_p->udp_over_ipv4_checksum_en);
    qdf_print("HOST_TSO::udp_over_ipv6_checksum_en %x",msdu_link_ext_p->udp_over_ipv6_checksum_en);
    qdf_print("HOST_TSO::tcp_over_ipv4_checksum_en %x",msdu_link_ext_p->tcp_over_ipv4_checksum_en);
    qdf_print("HOST_TSO::tcp_over_ipv6_checksum_en %x",msdu_link_ext_p->tcp_over_ipv6_checksum_en);
    qdf_print("HOST_TSO::buf0_ptr_31_0 %x",msdu_link_ext_p->buf0_ptr_31_0);
    qdf_print("HOST_TSO::buf0_len %x",msdu_link_ext_p->buf0_len);
    qdf_print("HOST_TSO::buf1_ptr_31_0 %x",msdu_link_ext_p->buf1_ptr_31_0);
    qdf_print("HOST_TSO::buf1_len %x",msdu_link_ext_p->buf1_len);
    qdf_print("--------> ENd of MSDU Desc ");
}

static struct ol_tx_tso_desc_t *
ol_tx_tso_sw_desc_alloc_fill(qdf_nbuf_t msdu,
                             struct ol_txrx_pdev_t *pdev)
{
    struct ol_tx_tso_desc_t *sw_tso_desc = NULL;
    A_UINT16 typeorlength;
#ifndef TSO_PKT_VALIDATION
    A_UINT16 frag_off;
#endif
    A_UINT8  *ehdr,*datap,*ip6hdr = NULL;
    qdf_net_iphdr_t *iphdr = NULL; /* Ip header ptr */
    qdf_net_tcphdr_t *tcphdr;      /* Tcp header ptr */

    ehdr = qdf_nbuf_data(msdu);
    typeorlength = qdf_ntohs(*(A_UINT16*)(ehdr + ADF_NET_ETH_LEN * 2));

    if (typeorlength == ETHERTYPE_VLAN) {
        typeorlength = qdf_ntohs(*(A_UINT16*)(ehdr
                        + ADF_NET_ETH_LEN * 2
                        + ADF_NET_VLAN_LEN));
    }

    if (!IS_ETHERTYPE(typeorlength)) { /* 802.3 header */
        struct llc_snap_hdr_t *llc_hdr =
                (struct llc_snap_hdr_t *)(ehdr + sizeof(struct ethernet_hdr_t));

        typeorlength = (llc_hdr->ethertype[0] << 8) | llc_hdr->ethertype[1];
    }

    if (qdf_unlikely((typeorlength != ADF_ETH_TYPE_IPV4) &&
                        (typeorlength != ADF_ETH_TYPE_IPV6))) {
        qdf_print("HOST_TSO::Dropping Pkt->Not an IPV4/V6 packet %d ",typeorlength);
        return NULL;
    }
    if (typeorlength == ADF_ETH_TYPE_IPV4)  {
        iphdr = (qdf_net_iphdr_t *)(qdf_nbuf_network_header(msdu));

#ifndef TSO_PKT_VALIDATION
        /* Following checks will be removed after different pkt tests
           for HW validation ?? */
        if (iphdr->ip_proto != IPPROTO_TCP) {
            qdf_print("HOST_TSO:Dropping Pkt->Not an TCPV4 packet ");
            return NULL;
        }

        /* XXX If the pkt is an ip fragment, TSO not expected to happen */
        frag_off = qdf_ntohs(iphdr->ip_frag_off);
        if (frag_off & (IP_MF | IP_OFFSET)) {
            qdf_print("HOST_TSO::Dropping Pkt->Fragmented IP packet ");
            return NULL;
        }
#endif
    } else {
        ip6hdr = (A_UINT8 *)(qdf_nbuf_network_header(msdu));
    }
    tcphdr = (qdf_net_tcphdr_t *)(qdf_nbuf_transport_header(msdu));

#ifndef TSO_PKT_VALIDATION
    /* Following checks will be removed after different pkt tests
       for HW validation ?? */
    if (tcphdr->urg || tcphdr->syn || tcphdr->rst || tcphdr->urg_ptr) {
         qdf_print("HOST_TSO:Dropping Pkt-> has invalid TCP flags syn %d \
                       rst %d urg %d ",tcphdr->syn,tcphdr->rst,tcphdr->urg);
         return NULL;
    }
#endif

    /* Allocate SW TSO Descriptor for storing parsed info. */
    sw_tso_desc = ol_tx_tso_desc_alloc(pdev);
    if (qdf_unlikely(!sw_tso_desc)) {
        qdf_print("HOST_TSO:Dropping Pkt->SW TSO Descriptor allocation \
                      failed ");
        return NULL;
    }
    qdf_atomic_init(&sw_tso_desc->ref_cnt);
    if (typeorlength == ADF_ETH_TYPE_IPV4)  {
        sw_tso_desc->iphdr = iphdr;
        sw_tso_desc->ip_hdr_type = TSO_IPV4;
        sw_tso_desc->l3_hdr_size = ((A_UINT8 *)tcphdr - (A_UINT8 *)iphdr);
    } else {
      /* For IPV6 */
        sw_tso_desc->iphdr = ip6hdr;
        sw_tso_desc->ip_hdr_type = TSO_IPV6;
        sw_tso_desc->l3_hdr_size = ((A_UINT8 *)tcphdr - (A_UINT8 *)ip6hdr);
    }
    sw_tso_desc->tcphdr = tcphdr;
    sw_tso_desc->l2_length = ((A_UINT8 *)(sw_tso_desc->iphdr) - ehdr);

    /* tcphdr->doff is number of 4-byte words in tcp header */
    datap = (A_UINT8 *)((A_UINT32 *)tcphdr + tcphdr->doff);
    sw_tso_desc->l2_l3_l4_hdr_size = datap - ehdr;      /* Number of bytes to payload start */

   /* SW TSO descriptor is allocated and filled, ready to be returned */
   return sw_tso_desc;
}

/* Carve a netbuf into multiple pieces. Parse the enet, ipv4/v6 and tcp headers
 * and record relevant information in the tso descriptor structure
 * input: vdev & msdu - Jumbo TCP packet as single network buffer
 * return value -
      On Success : List of segmented network buffers,
      On Failure : return NULL, and free the original network buffer
*/
qdf_nbuf_t
ol_tx_tso_segment(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu)
{
    struct ol_txrx_pdev_t *pdev;
    struct ol_tx_tso_desc_t *sw_tso_desc = NULL;

    A_INT32  msdu_len, mss_size;
    A_UINT32 header_paddr_lo;
    A_INT32  payload_offset, payload_len; /* Length of input - headers */
    A_UINT8  *header_vaddr = NULL;

    qdf_nbuf_t tso_header_nbuf = NULL,segment = NULL;
    qdf_nbuf_t segment_list_head = NULL, segment_list_tail = NULL;

    pdev = vdev->pdev;

    if (qdf_unlikely(ol_cfg_pkt_type(pdev) != htt_pkt_type_ethernet)) {
        qdf_print("HOST_TSO:Dropping Pkt->Device Frame Format is not 8023");
        goto fail;
    }

    msdu_len = qdf_nbuf_len(msdu);
    /* Extract MSS_size received as part of network buffer,
     * set by upper layers of network stack */
    mss_size = qdf_nbuf_tcp_tso_size(msdu);
    if(qdf_unlikely(mss_size <= 0 || msdu_len <=0 )) {
        qdf_print("HOST_TSO::Dropping Pkt->Invalid Jumbo Buffer length \
                      or Invalid TCP MSS received");
        goto fail;
    }
#if TSO_DBG
    qdf_print("MSS Size %d Pkt Len %d ",mss_size,msdu_len);
#endif

    tso_header_nbuf = msdu;/*Use the original netbuf recived as header buffer */

    qdf_nbuf_map_single(pdev->osdev, tso_header_nbuf, QDF_DMA_TO_DEVICE);

    header_vaddr = qdf_nbuf_data(tso_header_nbuf);
    header_paddr_lo = qdf_nbuf_get_frag_paddr(tso_header_nbuf,
                                      qdf_nbuf_get_num_frags(tso_header_nbuf));

    sw_tso_desc = ol_tx_tso_sw_desc_alloc_fill(msdu,pdev);
    if(qdf_unlikely(sw_tso_desc == NULL))
        goto fail_unmap;

    payload_offset = sw_tso_desc->l2_l3_l4_hdr_size;
    payload_len = msdu_len - payload_offset;

#if TSO_DBG
    qdf_print("payload %d offset %d hdr %d mss %d ",payload_len,payload_offset,sw_tso_desc->l2_l3_l4_hdr_size,mss_size);
#endif
    /* Now, carve the netbuf into multiple pieces */
    while (payload_len > 0) {

       if (payload_len < mss_size) {
            /* Last chunk - include the remainder */
            mss_size = payload_len;
        }

        segment = qdf_nbuf_clone(msdu);
        if (qdf_unlikely(segment == NULL)) {
            /*   This segment is dropped.
             *   Let's continue and try clone for other segments.
             *   Updating following 2 fields to proceed with segmentation
             *   while (....) loop exits without cloning any buffers
             *   incase of system memory exhaust
            */
            payload_offset += mss_size;
            payload_len -= mss_size;
            TXRX_VDEV_STATS_MSDU_INCR((struct ol_txrx_vdev_t *)vdev, tx_i.tso.dropped_host, msdu);
            continue;
        }

        /* Link all cloned skb's with each other for later processing */
        link_segment(segment_list_head, segment_list_tail, segment);

        qdf_nbuf_frag_push_head(segment, sw_tso_desc->l2_l3_l4_hdr_size,
                                header_vaddr, header_paddr_lo);

        /* Specify that this just-added fragment is a bytestream,
         * and should not be byte-swapped by the target during download.
         */

        /* Since qdf_nbuf_frag_push_head() always copies at the location "0"
         * safe to assume the latest header(l2/l3/l4) frag is at 0 */
        qdf_nbuf_set_frag_is_wordstream(segment, 0, 0);

        qdf_nbuf_pull_head(segment, payload_offset);    /* Trim from front */
        qdf_nbuf_trim_tail(segment, payload_len - mss_size);

        payload_offset += mss_size;
        payload_len -= mss_size;

         /*  maintain refcnt for TSO allocated SW desc
          *  increment once per cloned network buffer */
         qdf_atomic_inc(&sw_tso_desc->ref_cnt);

         /* Store header nbuf as part of cloned skb for later retrieval */
         qdf_nbuf_put_parent(segment,tso_header_nbuf);

        /* DMA mapping for cloned network buffers */
        qdf_nbuf_map_single(pdev->osdev, segment, QDF_DMA_TO_DEVICE);
    }

    /* None of the segments are created,
       free the header buffer & TSO descriptor */
    if( qdf_unlikely(qdf_atomic_read(&sw_tso_desc->ref_cnt) == 0)) {
        ol_tx_tso_desc_free(pdev,sw_tso_desc);
        goto fail_unmap;
    }

    sw_tso_desc->tso_final_segment = segment;

    /* Link SW TSO Descriptor to Header network buffer for later usage */
    qdf_nbuf_frag_push_head(tso_header_nbuf, sizeof(struct ol_tx_tso_desc_t),
                                (char *)sw_tso_desc, 0);

#if TSO_DBG
    ol_tx_tso_desc_display(sw_tso_desc);
#endif
    return segment_list_head;

fail_unmap:
    /* DMA mapping for cloned network buffers */
    qdf_nbuf_unmap_single(pdev->osdev, msdu, QDF_DMA_TO_DEVICE);

fail:
    TXRX_VDEV_STATS_MSDU_INCR((struct ol_txrx_vdev_t *)vdev, tx_i.tso.dropped_host, msdu);
    qdf_nbuf_free(msdu);
    return NULL;
}

/*  Prepares TX Descriptor per TCP Segment for TSO Packets
 *  Fills MSDU Link Ext descriptor with TSO details */
void
ol_tx_tso_desc_prepare(
    struct ol_txrx_pdev_t *pdev,
    qdf_nbuf_t tso_header_nbuf,
    qdf_nbuf_t segment,
    struct ol_tx_desc_t *tx_desc,
    struct ol_tx_tso_desc_t *sw_tso_desc)
{
    A_UINT32 frag_offset,data_len;
    A_UINT16 ip_id,v6_opt_len;
    A_UINT8 *header_vaddr;
    A_UINT8 *segment_vaddr,frag_no;
    qdf_net_tcphdr_t *tcphdr;
    qdf_net_iphdr_t *iphdr;
    struct msdu_link_ext *msdu_link_ext_p;
    A_UINT32 tcp_seq;


#if TSO_DBG
    ol_tx_tso_desc_display(sw_tso_desc);
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
    tcphdr = sw_tso_desc->tcphdr;
    iphdr = sw_tso_desc->iphdr;

    /* Now, fill in the msdu_link_ext structure */
    msdu_link_ext_p = (struct msdu_link_ext *)tx_desc->htt_frag_desc;

    msdu_link_ext_p->tso_enable = 1;
    msdu_link_ext_p->tcp_flag = 0;

    if (tcphdr->fin && segment == sw_tso_desc->tso_final_segment) {
        /* Set fin in last segment, if set in original */
        msdu_link_ext_p->tcp_flag |= EXT_P_FLAG_FIN;
    }
    if (tcphdr->psh) {
        msdu_link_ext_p->tcp_flag |= EXT_P_FLAG_PSH;
    }
    if (tcphdr->ack) {
        msdu_link_ext_p->tcp_flag |= EXT_P_FLAG_ACK;
    }
    msdu_link_ext_p->tcp_flag_mask = TSO_TCP_FLAG_MASK;
    msdu_link_ext_p->l2_length = sw_tso_desc->l2_length;


    frag_no = qdf_nbuf_get_num_frags(segment);
    data_len = qdf_nbuf_get_frag_len(segment,frag_no);

    /* Fill IP Length and ip_id only incase of IPV4 */
    if(sw_tso_desc->ip_hdr_type == TSO_IPV4)
    {
        iphdr = (qdf_net_iphdr_t *)sw_tso_desc->iphdr;
        /* IPV4 Length includes data length + l3 + l4 */
        msdu_link_ext_p->ip_length = data_len + sw_tso_desc->l2_l3_l4_hdr_size - sw_tso_desc->l2_length;
        msdu_link_ext_p->ip_identification = qdf_ntohs(iphdr->ip_id);
        iphdr->ip_id = qdf_htons(msdu_link_ext_p->ip_identification + 1);

        msdu_link_ext_p->ipv4_checksum_en = 1;
        msdu_link_ext_p->tcp_over_ipv4_checksum_en = 1;
    }
    else
    {
        /* IPV6 Length includes data length + l4 */
        /* IPV6 length should include IPV6 ext. header length also, extracting as follows.... */
        v6_opt_len = sw_tso_desc->l3_hdr_size - IPV6_STD_HDR_SIZE;
        msdu_link_ext_p->ip_length = data_len + sw_tso_desc->l2_l3_l4_hdr_size-sw_tso_desc->l2_length - sw_tso_desc->l3_hdr_size + v6_opt_len;
        msdu_link_ext_p->tcp_over_ipv6_checksum_en = 1;
    }

    header_vaddr = qdf_nbuf_get_frag_vaddr(tso_header_nbuf, qdf_nbuf_get_num_frags(tso_header_nbuf) - 1);
    segment_vaddr = qdf_nbuf_get_frag_vaddr(segment, qdf_nbuf_get_num_frags(segment) - 1);

    /* compute tcp_seq for this fragment
     * this assumes that the header and all data segments of the netbuf
     * are virtually contiguous.
     * Need to review and modify once the support for non-contiguous physical memory(map_sg) for jumbo frame is added
     */
    tcp_seq = qdf_ntohl(tcphdr->seq);
    frag_offset = segment_vaddr - header_vaddr - sw_tso_desc->l2_l3_l4_hdr_size;
    msdu_link_ext_p->tcp_seq_number = tcp_seq + frag_offset;

#if TSO_DBG
    qdf_print("Seq No. %x ",msdu_link_ext_p->tcp_seq_number);
#endif

    /* Decrement ref_cnt in SW TSO Descriptor since
     * one TCP segment has already used it and no longer requires it */
    if (qdf_atomic_dec_and_test(&sw_tso_desc->ref_cnt)) {
        /* Free the header skb and SW TSO descriptor
         * since this is the last segment, No one else requires it further */
#if TSO_DBG
        qdf_print("Last Segment Transfer, Freeing header buffer %x  \
                      and TX TSo Descriptor %x -----",tso_header_nbuf,sw_tso_desc);
#endif

        qdf_nbuf_free(tso_header_nbuf);
        ol_tx_tso_desc_free(pdev, sw_tso_desc);
    }
#if TSO_DBG
    ol_tx_msdu_desc_display(msdu_link_ext_p);
#endif
    return;
}

void
ol_tx_print_tso_stats(
    void *vdev_handle)
{
    ol_txrx_vdev_handle vdev = (ol_txrx_vdev_handle)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    struct ol_txrx_stats *stats = &pdev->stats.pub;
    struct cdp_pdev_stats_cnvrged *pdev_stats = &pdev->pdev_stats;

    qdf_print("++++++++++ TSO STATISTICS +++++++++++");
    qdf_print("TSO TX Pkts       : %lu", pdev_stats->tx_i.tso.tso_pkts.num);
    qdf_print("TSO TX Bytes      : %lu", pdev_stats->tx_i.tso.tso_pkts.bytes);
    qdf_print("Non-TSO TX Pkts   : %lu", pdev_stats->tx_i.tso.non_tso_pkts.pkts);
    qdf_print("Non-TSO TX Bytes  : %lu", pdev_stats->tx_i.tso.non_tso_pkts.bytes);
    qdf_print("TSO Dropped Pkts  : %lu", stats->tx.tso.tso_dropped.pkts);
    qdf_print("TSO Dropped Bytes : %lu", stats->tx.tso.tso_dropped.bytes);
    qdf_print("TSO Desc Total %d In Use : %d", pdev->tx_tso_desc.pool_size, \
                                             pdev_stats->tx_i.tso.tso_desc_cnt);
}

void
ol_tx_rst_tso_stats(void *vdev_handle)
{
    ol_txrx_vdev_handle vdev = (ol_txrx_vdev_handle)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    struct ol_txrx_stats *stats = &pdev->stats.pub;

    qdf_print(".....Resetting TSO Stats ");
    /* Tx */
    stats->tx.tso.tso_pkts.pkts = 0;
    pdev_stats->tx_i.tso.tso_pkts.num = 0;
    stats->tx.tso.tso_pkts.bytes = 0;
    pdev_stats->tx_i.tso.tso_pkts.bytes = 0;
    stats->tx.tso.non_tso_pkts.pkts = 0;
    pdev_stats->tx_i.tso.non_tso_pkts.num = 0;
    pdev_stats->tx_i.tso.non_tso_pkts.bytes = 0;
    stats->tx.tso.non_tso_pkts.bytes = 0;
    stats->tx.tso.tso_dropped.pkts = 0;
    stats->tx.tso.tso_dropped.bytes = 0;

    printk("tso_stats %d %d %d %d \n",pdev_stats->tx_i.tso.tso_pkts.num,pdev_stats->tx_i.tso.tso_pkts.bytes,pdev_stats->tx_i.tso.non_tso_pkts.num,pdev_stats->tx_i.tso.non_tso_pkts.bytes);
}

void
tso_release_parent_tso_desc(ol_txrx_vdev_handle vdev, qdf_nbuf_t seg_msdu)
{
    qdf_nbuf_t tso_header_nbuf;
    struct ol_tx_tso_desc_t *tx_tso_desc=NULL;
    struct ol_txrx_pdev_t *pdev;

    pdev = vdev->pdev;

    /* free the additional Header buffer and SW TSO descriptor */

    tso_header_nbuf = qdf_nbuf_get_parent(seg_msdu);
    if (tso_header_nbuf) {
        /* SW TSO Descriptor is stored as part of frag[0]
           inside header buffer */
        tx_tso_desc =
          (struct ol_tx_tso_desc_t *)QDF_NBUF_CB_TX_EXTRA_FRAG_VADDR(tso_header_nbuf,0);
        if(tx_tso_desc)
        {
            /* Decrement ref_cnt in SW TSO Descriptor since
             * one TCP segment hit error and is going to be freed */
            if (qdf_atomic_dec_and_test(&tx_tso_desc->ref_cnt)) {
                /* Free the header skb and SW TSO descriptor
                 * since this is the last segment, No one else requires it further */
                qdf_nbuf_free(tso_header_nbuf);
                ol_tx_tso_desc_free(pdev, tx_tso_desc);
            }
        }
    }
}

inline int
ol_tx_tso_process_skb(ol_txrx_vdev_handle vdev,qdf_nbuf_t msdu)
{
    qdf_nbuf_t msdu_list;
    int tid_q_map;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    uint8_t tid;

    msdu = qdf_nbuf_unshare(msdu);
    if (msdu == NULL)
        return 1;

    TXRX_VDEV_STATS_MSDU_INCR((struct ol_txrx_vdev_t *)vdev, tx_i.tso.tso_pkts, msdu);

    /* Following function creates Multiple buffers (one per segment) and linked with each other
     * for further TX processing,
     * Original buffer used as header buffer, freed at later stage of TX
     * If NULL is returned, error encountered in segmentation
     * else proceed processing of individual TCP segments */

    msdu_list = ol_tx_tso_segment(vdev, msdu);

    if (qdf_unlikely(!msdu_list))  {
        qdf_print(".....Error in TSO Segmentation ");
        return 0;
    }

    /* Inject the TCP segments into regular TX Data path */
    while(msdu_list) {
        qdf_nbuf_t next;
        tid = HTT_TX_EXT_TID_INVALID;
        next = qdf_nbuf_next(msdu_list);
        msdu_list->next = NULL;
#if PEER_FLOW_CONTROL
        tid_q_map = ol_tx_get_tid_override_queue_mapping(pdev, netbuf);
        if (tid_q_map >= 0) {
            tid = tid_q_map;
        }
        if (ol_tx_ll_fast(vdev,&msdu_list,1,HTT_INVALID_PEER,tid))
#else
        if (ol_tx_ll_fast(vdev,&msdu_list,1))
#endif
        {
           /* Decrement ref cnt for parent skb & sw tso desc for this segment,
            * which is going to be dropped */
           tso_release_parent_tso_desc(vdev, msdu_list);
           qdf_nbuf_free(msdu_list);
        }
        msdu_list = next;
    }
    return 0;
}

