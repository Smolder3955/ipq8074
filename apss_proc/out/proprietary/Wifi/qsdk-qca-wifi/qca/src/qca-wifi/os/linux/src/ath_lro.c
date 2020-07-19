/*
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/module.h>
#include <linux/if_vlan.h>
#include <linux/time.h>
#include <net/checksum.h>
#include <net/tcp.h>
#include "osif_private.h"
#include <ieee80211_var.h>

#define in6_not_equal(in6_addr_a, in6_addr_b) \
        (memcmp(&in6_addr_a, &in6_addr_b, sizeof(struct in6_addr)) != 0)

#define TCP_HEADER_LEN(tcp_header) (tcp_header->doff << 2)
#define IP_HEADER_LEN(ip4_header) (ip4_header->ihl << 2)

#define IPV4_TCP_PAYLOAD_LENGTH(ip4_header, tcp_header) \
        (qdf_ntohs(ip4_header->tot_len) - IP_HEADER_LEN(ip4_header) - \
	TCP_HEADER_LEN(tcp_header))
#define IPV6_TCP_PAYLOAD_LENGTH(ip6_header, tcp_header) \
        (qdf_ntohs(ip6_header->payload_len) - TCP_HEADER_LEN(tcp_header))

#define TCP_TIMESTAMP_LEN 8

/*
 * Add two 16-bit TCP checksums and return the cumulative
 * checksum. (used when aggregating received packets TCP payload with
 * LRO parent packet)
 */

static
unsigned short ath_cksum_add(unsigned short ck_sum1, unsigned short ck_sum2)
{
    unsigned int ck_sum;

    ck_sum = ck_sum1 + ck_sum2;
    ck_sum = (ck_sum >> 16) + (ck_sum & 0xffff);
    ck_sum += (ck_sum >> 16);
    return (unsigned short)ck_sum;
}

/*
 * The ath_get_skb_hdr function will return the IP & TCP header
 * pointers from the passed skb.
 */

static inline void ath_get_nbuf_hdr(qdf_nbuf_t nbuf,
                                    void **ip_header,
                                    void **tcp_header)
{

    /* We take advantage of the HW as it provides the IP & TCP
     * offset position in the rx msdu descriptors
     */
    unsigned char *l3_start = qdf_nbuf_data(nbuf);

    if (nbuf->protocol == qdf_htons(ETH_P_8021Q)) {
        l3_start += VLAN_HLEN;
    }

    *ip_header = (void *)l3_start;
    *tcp_header = (struct tcphdr *)(l3_start + QDF_NBUF_CB_RX_TCP_OFFSET(nbuf));
}

/*
 * @brief: Basic tcp checks whether packet is suitable for LRO
 * We make use of HW LRO to achieve better performance.
 */

static int ath_lro_elegibility_check(const void *ip_header,
                                     const struct tcphdr *tcp_header,
                                     const ath_lro_entry_t *le,
                                     qdf_nbuf_t nbuf)
{
    /* We use HW LRO info to check if pkt is LRO eligible */
    if ((!QDF_NBUF_CB_RX_LRO_ELIGIBLE(nbuf) ||
                         QDF_NBUF_CB_RX_TCP_PURE_ACK(nbuf))) {
        return (ATH_TCP_LRO_NOT_SUPPORTED);
    }

    /* check if pkt is IPv6: don't aggregate if IPv6 header
     * is followed by extension headers */
    if (QDF_NBUF_CB_RX_IPV6_PROTO(nbuf)) {
        struct ipv6hdr *ip6_header = (struct ipv6hdr *)ip_header;

        if (ip6_header->nexthdr != IPPROTO_TCP) {
            return (ATH_TCP_LRO_NOT_SUPPORTED);
        }
    }

    /* if TCP options has timestamp, check its validity  */
    if (tcp_header->doff == TCP_TIMESTAMP_LEN) {
        unsigned int *ts_ptr;
        ts_ptr = (unsigned int *)(tcp_header + 1);

        if (*ts_ptr != qdf_htonl((TCPOPT_NOP << 24) |
                                    (TCPOPT_NOP << 16) |
                                    (TCPOPT_TIMESTAMP << 8) |
                                    TCPOLEN_TIMESTAMP)) {
            return (ATH_TCP_LRO_FAILURE);
        }
        /* Make sure timestamp values are increasing */
        if (le) {
            if (qdf_ntohl(le->tcp_tsval) > qdf_ntohl(*(ts_ptr + 1))) {
                return (ATH_TCP_LRO_FAILURE);
            }
        }
        /* Make sure timestamp reply is not zero */
        if (*(ts_ptr + 2) == 0) {
            return (ATH_TCP_LRO_FAILURE);
        }
    }
    return (ATH_TCP_LRO_SUCCESS);
}

/*
 * This function updates the IP and TCP headers of the LRO
 * parent packet, This function is called when we are about to
 * flush the parent packet.
 */

static void ath_lro_update_head_nbuf_header(ath_lro_entry_t *le)
{
    struct iphdr *ip_header = NULL;
    struct ipv6hdr *ip6_header = NULL;
    struct tcphdr *tcp_header = le->tcp_header;

    unsigned int *ts_ptr;
    unsigned int tcp_header_ck_sum;
    unsigned int tcp_payload_ck_sum;
    unsigned int tcp_header_payload_ck_sum;

    tcp_header->ack_seq = le->tcp_ack;
    tcp_header->window = le->tcp_window_size;

    if (le->tcp_tstamp_enabled) {
         ts_ptr = (unsigned int *)(tcp_header + 1);
         ts_ptr[2] = le->tcp_tser;
    }

    if (le->ipv6_proto) {
        ip6_header = (struct ipv6hdr *)le->ip_header;
        ip6_header->payload_len = qdf_htons(le->ip_total_len);
    }
    else {
        ip_header = (struct iphdr *)le->ip_header;
        csum_replace2(&ip_header->check,
                      ip_header->tot_len,
                      qdf_htons(le->ip_total_len));

        ip_header->tot_len = qdf_htons(le->ip_total_len);
    }

    tcp_header->check = 0;

    tcp_header_ck_sum = csum_partial(tcp_header, TCP_HEADER_LEN(tcp_header), 0);
    tcp_payload_ck_sum = ~csum_unfold(qdf_htons(le->tcp_payload_ck_sum));
    tcp_header_payload_ck_sum = csum_add(tcp_payload_ck_sum, tcp_header_ck_sum);

    if (le->ipv6_proto) {
        tcp_header->check = csum_ipv6_magic(&ip6_header->saddr,
                                            &ip6_header->daddr,
                                            le->ip_total_len,
                                            IPPROTO_TCP,
                                            tcp_header_payload_ck_sum);
    }
    else {
        tcp_header->check = csum_tcpudp_magic(ip_header->saddr,
                                              ip_header->daddr,
                                              le->ip_total_len - IP_HEADER_LEN(ip_header),
                                              IPPROTO_TCP,
                                              tcp_header_payload_ck_sum);
    }
}

/*
 * initialize the LRO entry for the pkt which is LRO eligible
 * and is the first packet (parent) in the aggregation list.
 */

static void ath_lro_init_entry(ath_lro_entry_t *le, qdf_nbuf_t nbuf,
                               void *ip_header, struct tcphdr *tcp_header)
{
    unsigned int *ts_ptr;
    unsigned int tcp_data_len;
    struct ipv6hdr *ip6_header;
    struct iphdr *ip4_header;


    if (QDF_NBUF_CB_RX_IPV6_PROTO(nbuf)) {
        ip6_header = (struct ipv6hdr *)ip_header;
        tcp_data_len = IPV6_TCP_PAYLOAD_LENGTH(ip6_header, tcp_header);
        le->ip_total_len = qdf_ntohs(ip6_header->payload_len);
        le->ipv6_proto = 1;
    }
    else {
        ip4_header = (struct iphdr *)ip_header;
        tcp_data_len = IPV4_TCP_PAYLOAD_LENGTH(ip4_header, tcp_header);
        le->ip_total_len = qdf_ntohs(ip4_header->tot_len);
    }

    le->head = nbuf;
    le->ip_header = ip_header;

    le->tcp_header = tcp_header;
    le->tcp_next_seq = qdf_ntohl(tcp_header->seq) + tcp_data_len;
    le->tcp_ack = tcp_header->ack_seq;
    le->tcp_window_size = tcp_header->window;


    le->no_pkts = 1;

    if (tcp_header->doff == TCP_TIMESTAMP_LEN) {
        ts_ptr = (unsigned int *)(tcp_header + 1);
        le->tcp_tstamp_enabled = 1;
        le->tcp_tsval = ts_ptr[1];
        le->tcp_tser = ts_ptr[2];
    }

    le->mss = tcp_data_len;
    le->active = 1;
    le->tcp_payload_ck_sum = QDF_NBUF_CB_RX_TCP_CHKSUM(nbuf);
    le->dev = nbuf->dev;

}

/*
 * This function is called for every pkt which matches to the
 * existing lro entry, the lro entry is updated with
 * the current pkt's tcp header fields which will be later
 * used to update the parent pkt's TCP header.
 */

static void ath_lro_add_common(ath_lro_entry_t *le, void *ip_header,
                               struct tcphdr *tcp_header, int tcp_data_len,
                               qdf_nbuf_t nbuf)
{
    qdf_nbuf_t head = le->head;
    unsigned int *ts_ptr;

    le->no_pkts++;

    if (le->tcp_tstamp_enabled) {
         ts_ptr = (unsigned int *) (tcp_header + 1);
         le->tcp_tser = *(ts_ptr + 2);
    }

    le->ip_total_len += tcp_data_len;
    le->tcp_next_seq += tcp_data_len;
    le->tcp_window_size = tcp_header->window;
    le->tcp_ack = tcp_header->ack_seq;

    le->tcp_payload_ck_sum = ath_cksum_add(le->tcp_payload_ck_sum,
                                        QDF_NBUF_CB_RX_TCP_CHKSUM(nbuf));

    head->len += tcp_data_len;
    head->data_len += tcp_data_len;
    if (tcp_data_len > le->mss) {
        le->mss = tcp_data_len;
    }
}

/*
 * This function is where we aggregate the packets (chaining)
 * which fall under the same LRO entry.
 */

static void ath_lro_add_packet(ath_lro_entry_t *le, qdf_nbuf_t nbuf,
                               void *ip_header, struct tcphdr *tcp_header)
{

    qdf_nbuf_t head = le->head;
    int tcp_data_len;
    struct ipv6hdr *ip6_header;
    struct iphdr *ip4_header;

     if (QDF_NBUF_CB_RX_IPV6_PROTO(nbuf)) {
        ip6_header = (struct ipv6hdr *)ip_header;
        tcp_data_len = IPV6_TCP_PAYLOAD_LENGTH(ip6_header, tcp_header);
     }
     else {
        ip4_header = (struct iphdr *)ip_header;
        tcp_data_len = IPV4_TCP_PAYLOAD_LENGTH(ip4_header, tcp_header);
     }

    ath_lro_add_common(le, ip_header, tcp_header, tcp_data_len, nbuf);

    qdf_nbuf_pull_head(nbuf, (nbuf->len - tcp_data_len));
    head->truesize += nbuf->truesize;

    if (le->last) {
        le->last->next = nbuf;
    }
    else {
        skb_shinfo(head)->frag_list = nbuf;
    }
    le->last = nbuf;
}

/*
 * This function is to verify if the received pkt matches
 * to the lro entry.
 */

static int ath_lro_check_tcp_conn(ath_lro_entry_t *le,
                                  void *ip_header,
                                  struct tcphdr *tcp_header,
                                  qdf_nbuf_t nbuf)
{
    /* check if source port and dest port of the pkt match the lro entry */
    if (le->tcp_header->source == tcp_header->source ||
        le->tcp_header->dest == tcp_header->dest) {
        /* if it is a IPv6 pkt and descriptor is also IPv6 */
        if (le->ipv6_proto && QDF_NBUF_CB_RX_IPV6_PROTO(nbuf)) {
            struct ipv6hdr *le_ip6h = (struct ipv6hdr *)le->ip_header;
            struct ipv6hdr *pkt_ip6h = (struct ipv6hdr *)ip_header;

            if (in6_not_equal(le_ip6h->saddr, pkt_ip6h->saddr) ||
                in6_not_equal(le_ip6h->daddr, pkt_ip6h->daddr))
                return (ATH_TCP_LRO_FAILURE);
        }
        /* if it is a IPv4 pkt and descriptor is also IPv4 */
        else if(!le->ipv6_proto && !QDF_NBUF_CB_RX_IPV6_PROTO(nbuf)) {
            struct iphdr *le_ip4h = (struct iphdr *)le->ip_header;
            struct iphdr *pkt_ip4h = (struct iphdr *)ip_header;

            if ((le_ip4h->saddr != pkt_ip4h->saddr) ||
                (le_ip4h->daddr != pkt_ip4h->daddr)) {
                return (ATH_TCP_LRO_FAILURE);
            }
        }
        return (ATH_TCP_LRO_SUCCESS);
    }
    return (ATH_TCP_LRO_FAILURE);
}

/*
 * This function returns an lro entry from the
 * LRO array which matches to the received pkt or
 * an inactive lro entry is returned which can
 * be initialized later on.
 * If all the entries of the lro array are active
 * and the received pkt does not match to any of the
 * entries then NULL is returned.
 */

static ath_lro_entry_t *ath_retrieve_le(ath_lro_entry_t *ath_le_arr,
                                        void *ip_header,
                                        struct tcphdr *tcp_header,
                                        qdf_nbuf_t nbuf)
{
    ath_lro_entry_t *le = NULL;
    ath_lro_entry_t *tmp_le;
    int i, inactive_le_no, inactive_le_found = 0;

    for (i = 0; i < LRO_MAX_ENTRY; i++) {
        tmp_le = &ath_le_arr[i];

        if (tmp_le->active) {
            if (!ath_lro_check_tcp_conn(tmp_le, ip_header, tcp_header, nbuf)) {
                 le = tmp_le;
                 return (le);
            }
        }
        else if (!inactive_le_found) {
            inactive_le_no = i;
            inactive_le_found = 1;
        }
    }

    if (inactive_le_found) {
        le = &ath_le_arr[inactive_le_no];
        return (le);
    }
    return (le);
}

/*
 * This function is called to flush the existing
 * aggregated pkt. This happens when we get the
 * a. MAX_AGGR limit.
 * b. out of sequence pkt.
 * c. non-LRO eligible pkt.
 */

static void ath_lro_flush(ath_lro_entry_t *le)
{
    osif_dev *osdev;
    struct ieee80211vap *os_if;
    osdev = ath_netdev_priv(le->head->dev);
    os_if = osdev->os_if;

    if (le->no_pkts > 1) {
        ath_lro_update_head_nbuf_header(le);
    }

    skb_shinfo(le->head)->gso_size = le->mss;

#if ATH_SUPPORT_VLAN
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
    if ( osdev->vlanID != 0)
    {
        /* attach vlan tag */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
        __vlan_hwaccel_put_tag(le->head, osdev->vlanID);
#else
        __vlan_hwaccel_put_tag(le->head, qdf_htons(ETH_P_8021Q), osdev->vlanID);
#endif
    }
#else
    if ( osdev->vlanID != 0 && osdev->vlgrp != NULL)
    {
        /* attach vlan tag */
        vlan_hwaccel_rx(le->head, osdev->vlgrp, osdev->vlanID);
    }
    else  /*XXX NOTE- There is an else here. Be careful while adding any code below */
#endif
#endif

    skb_orphan(le->head);
    nbuf_debug_del_record(le->head);
    netif_rx(le->head);

    os_if->flushed++;
    memset(le, 0, sizeof(ath_lro_entry_t));
}

/*
 * The main finction which will decide if a pkt is LRO
 * eligible or not and how to further process the pkt.
 */

unsigned int ath_lro_process_nbuf(qdf_nbuf_t nbuf, void *priv)
{
    ath_lro_entry_t *le;
    void *ip_header;
    struct tcphdr *tcp_header;

    osif_dev *osdev;
    struct ieee80211vap *os_if;
    struct net_device *dev;

    dev = (struct net_device *)priv;
    osdev = ath_netdev_priv(nbuf->dev);
    os_if = osdev->os_if;


    /* if packet is not a TCP packet,
     * hand it over to stack immediately
     */
    if(!QDF_NBUF_CB_RX_TCP_PROTO(nbuf)) {
        return (ATH_TCP_LRO_NOT_SUPPORTED);
    }
    ath_get_nbuf_hdr(nbuf, (void *)&ip_header, (void *)&tcp_header);

    le = ath_retrieve_le(os_if->ath_le_arr, ip_header, tcp_header, nbuf);
    if (!le) {
        return (ATH_TCP_LRO_FAILURE);
    }
    if (!le->active) {
        if (ath_lro_elegibility_check((void *)ip_header, tcp_header,
                                      NULL, nbuf)) {
            return (ATH_TCP_LRO_FAILURE);
        }
        ath_lro_init_entry(le, nbuf, ip_header, tcp_header);
        os_if->lro_to_flush = 1;
        os_if->aggregated++;

        return (ATH_TCP_LRO_SUCCESS);
    }

    if (le->tcp_next_seq != qdf_ntohl(tcp_header->seq)) {
        ath_lro_flush(le);
        return (ATH_TCP_LRO_FAILURE);
    }

    if (ath_lro_elegibility_check(ip_header, tcp_header, le, nbuf)) {
        ath_lro_flush(le);
        return (ATH_TCP_LRO_FAILURE);
    }

    ath_lro_add_packet(le, nbuf, ip_header, tcp_header);
    os_if->aggregated++;

    if ((le->no_pkts >= LRO_MAX_AGGR) ||
        le->head->len > (0xFFFF - dev->mtu)) {
        ath_lro_flush(le);
    }
    return (ATH_TCP_LRO_SUCCESS);

}

/*
 * function to flush pkts all the Active descriptors
 */

void ath_lro_flush_all(void *priv)
{
    int i;
    struct net_device *dev;
    struct ieee80211vap *os_if;
    osif_dev *osdev;
    ath_lro_entry_t *le;

    dev = (struct net_device *)priv;
    osdev = ath_netdev_priv(dev);
    os_if = osdev->os_if;

    le = os_if->ath_le_arr;

    for (i = 0; i < LRO_MAX_ENTRY; i++) {
        if ((le[i].active) && (le[i].dev == dev)) {
            ath_lro_flush(&le[i]);
        }
    }

    osdev->os_if->lro_to_flush = 0;

}
EXPORT_SYMBOL(ath_lro_flush_all);
