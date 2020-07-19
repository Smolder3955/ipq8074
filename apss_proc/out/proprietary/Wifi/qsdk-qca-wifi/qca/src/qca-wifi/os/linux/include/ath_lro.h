/*
 * Copyright (c) 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#include <net/ip.h>
#include <qdf_nbuf.h>

#define ATH_TCP_LRO_SUCCESS             0
#define ATH_TCP_LRO_FAILURE             1
#define ATH_TCP_LRO_NOT_SUPPORTED       2

#define LRO_MAX_ENTRY    16
#define LRO_MAX_AGGR     32

/*
 * HW provided LRO info
 */

/*
 * LRO descriptor for a tcp session
 */
typedef struct ath_lro_entry {
        qdf_nbuf_t head;
        qdf_nbuf_t last;
        void *ip_header;
        struct tcphdr *tcp_header;
        unsigned int tcp_seq;
        unsigned int tcp_next_seq;
        unsigned short tcp_payload_ck_sum;
        unsigned short ip_total_len;
        unsigned short tcp_tstamp_enabled;
        unsigned short tcp_window_size;
        unsigned int tcp_tser;
        unsigned int tcp_tsval;
        unsigned int tcp_ack;
        int no_pkts;
        int mss;
        int active;
        int ipv6_proto;
        struct net_device *dev;
} ath_lro_entry_t;

