/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_txrx_prot_an.h
 * @brief API defs for the protocol analyzer debug tool within the data SW.
 */
#ifndef _OL_TXRX_PROT_AN__H_
#define _OL_TXRX_PROT_AN__H_

#include <qdf_nbuf.h>      /* qdf_nbuf_t */
#include <ol_txrx_dbg.h>   /* ENABLE_TXRX_PROT_ANALYZE */
#include <ol_txrx_types.h> /* ol_txrx_pdev_t */

/*--- data type defs ---*/

struct ol_txrx_prot_an_t;
typedef struct ol_txrx_prot_an_t *ol_txrx_prot_an_handle;

enum ol_txrx_prot_an_action {
    TXRX_PROT_ANALYZE_ACTION_PARSE, /* and count */
    TXRX_PROT_ANALYZE_ACTION_PRINT, /* and count */
    TXRX_PROT_ANALYZE_ACTION_COUNT, /* just count */
};

enum ol_txrx_prot_an_period_type {
    TXRX_PROT_ANALYZE_PERIOD_COUNT,
    TXRX_PROT_ANALYZE_PERIOD_TIME,
};


/*--- conditional compilation API cover macros ---*/

#ifndef ENABLE_TXRX_PROT_ANALYZE
/* protocol analyzer debug feature disabled */

#define OL_TXRX_PROT_AN_CREATE_802_3(pdev, name) NULL

#define OL_TXRX_PROT_AN_ADD_IPV4(pdev, l2_prot) NULL
#define OL_TXRX_PROT_AN_ADD_IPV6(pdev, l2_prot) NULL
#define OL_TXRX_PROT_AN_ADD_ARP(pdev, l2_prot) NULL

#define OL_TXRX_PROT_AN_ADD_TCP(\
        pdev, l3_prot, period_type, period_mask, period_msec) /*NULL*/
#define OL_TXRX_PROT_AN_ADD_UDP(\
        pdev, l3_prot, period_type, period_mask, period_msec) /*NULL*/
#define OL_TXRX_PROT_AN_ADD_ICMP(\
        pdev, l3_prot, period_type, period_mask, period_msec) /*NULL*/

#define OL_TXRX_PROT_AN_LOG(prot_an, netbuf)

#define OL_TXRX_PROT_AN_DISPLAY(prot_an)

#define OL_TXRX_PROT_AN_FREE(prot_an)

#else
/* protocol analyzer debug feature enabled */

#define OL_TXRX_PROT_AN_CREATE_802_3(pdev, name) \
    ol_txrx_prot_an_create_802_3(pdev, name)


#define OL_TXRX_PROT_AN_ADD_IPV4(pdev, l2_prot) \
    ol_txrx_prot_an_add_ipv4(pdev, l2_prot)

#define OL_TXRX_PROT_AN_ADD_IPV6(pdev, l2_prot) \
    ol_txrx_prot_an_add_ipv6(pdev, l2_prot)

#define OL_TXRX_PROT_AN_ADD_ARP(pdev, l2_prot) \
    ol_txrx_prot_an_add_arp(pdev, l2_prot)


#define OL_TXRX_PROT_AN_ADD_TCP( \
        pdev, l3_prot, period_type, period_mask, period_msec) \
    ol_txrx_prot_an_add_tcp( \
        pdev, l3_prot, period_type, period_mask, period_msec)

#define OL_TXRX_PROT_AN_ADD_UDP( \
        pdev, l3_prot, period_type, period_mask, period_msec) \
    ol_txrx_prot_an_add_udp( \
        pdev, l3_prot, period_type, period_mask, period_msec)

#define OL_TXRX_PROT_AN_ADD_ICMP( \
        pdev, l3_prot, period_type, period_mask, period_msec) \
    ol_txrx_prot_an_add_icmp( \
        pdev, l3_prot, period_type, period_mask, period_msec)


#define OL_TXRX_PROT_AN_LOG(prot_an, netbuf) \
    ol_txrx_prot_an_log(prot_an, netbuf)

#define OL_TXRX_PROT_AN_DISPLAY(prot_an) ol_txrx_prot_an_display(prot_an)

#define OL_TXRX_PROT_AN_FREE(prot_an) ol_txrx_prot_an_free(prot_an);

/*--- API functions ---*/

/* base (layer 2) protocol: 802.3 or 802.11 */

ol_txrx_prot_an_handle
ol_txrx_prot_an_create_802_3(struct ol_txrx_pdev_t *pdev, const char *name);

ol_txrx_prot_an_handle
ol_txrx_prot_an_create_802_11(struct ol_txrx_pdev_t *pdev, const char *name);

/* 2nd-tier (layer 3) protocol: IPv4, IPv6, ARP */

ol_txrx_prot_an_handle
ol_txrx_prot_an_add_ipv4(
    struct ol_txrx_pdev_t *pdev,
    ol_txrx_prot_an_handle l2_prot);

ol_txrx_prot_an_handle
ol_txrx_prot_an_add_ipv6(
    struct ol_txrx_pdev_t *pdev,
    ol_txrx_prot_an_handle l2_prot);

ol_txrx_prot_an_handle
ol_txrx_prot_an_add_arp(
    struct ol_txrx_pdev_t *pdev,
    ol_txrx_prot_an_handle l2_prot);

/* 3nd-tier (layer 4) protocol: TCP, UDP, ICMP */

ol_txrx_prot_an_handle
ol_txrx_prot_an_add_tcp(
    struct ol_txrx_pdev_t *pdev,
    ol_txrx_prot_an_handle l3_prot,
    enum ol_txrx_prot_an_period_type period_type,
    u_int32_t period_mask,
    u_int32_t period_msec);

ol_txrx_prot_an_handle
ol_txrx_prot_an_add_udp(
    struct ol_txrx_pdev_t *pdev,
    ol_txrx_prot_an_handle l3_prot,
    enum ol_txrx_prot_an_period_type period_type,
    u_int32_t period_mask,
    u_int32_t period_msec);

ol_txrx_prot_an_handle
ol_txrx_prot_an_add_icmp(
    struct ol_txrx_pdev_t *pdev,
    ol_txrx_prot_an_handle l3_prot,
    enum ol_txrx_prot_an_period_type period_type,
    u_int32_t period_mask,
    u_int32_t period_msec);

/**
 * @brief Log a new packet through the specified protocol analyzer.
 * @details
 *  Apply the pre-configured protocol analyzer to the packet carried
 *  in the network buffer.
 *  The protocol analyzer will classify the packet, increment the
 *  counters for each layer of classification, and potentially print
 *  a brief description of the packet.
 *  Caveats:
 *    - The protocol analyzer assumes the layers of encapsulation headers are
 *      all contiguous within a single fragment of the network buffer.
 *
 * @param prot_an - the protocol analyzer to pass the packet through
 * @param netbuf - the packet to analyze
 */
void
ol_txrx_prot_an_log(ol_txrx_prot_an_handle prot_an, qdf_nbuf_t netbuf);

/**
 * @brief Report on the traffic that has gone through the protocol analyzer.
 * @details
 *  Show how many packets of each type recognized by the protocol analyzer
 *  have gone through the protocol analyzer.
 *
 * @param prot_an - the protocol analyzer whose traffic should be reported
 */
void
ol_txrx_prot_an_display(ol_txrx_prot_an_handle prot_an);


void
ol_txrx_prot_an_free(ol_txrx_prot_an_handle prot_an);

#endif /* ENABLE_TXRX_PROT_ANALYZE */

#endif /* _OL_TXRX_PROT_AN__H_ */
