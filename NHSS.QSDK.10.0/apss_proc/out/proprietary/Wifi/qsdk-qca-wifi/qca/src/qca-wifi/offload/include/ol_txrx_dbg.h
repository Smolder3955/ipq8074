/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_txrx_dbg.h
 * @brief Functions provided for visibility and debugging.
 */
#ifndef _OL_TXRX_DBG__H_
#define _OL_TXRX_DBG__H_

#include <athdefs.h>       /* A_STATUS, u_int64_t */
#include <qdf_lock.h>   /* qdf_mutex_t */
#include <cdp_txrx_cmn_struct.h> /* ol_txrx_pdev_handle, etc. */
#include <cdp_txrx_stats_struct.h> /* ol_txrx_pdev_handle, etc. */

struct ol_txrx_stats_req_internal {
    struct ol_txrx_stats_req base;
    int serviced; /* state of this request */
    int offset;
};


struct ieee80211com;
enum {
    TXRX_DBG_MASK_OBJS             = 0x01,
    TXRX_DBG_MASK_STATS            = 0x02,
    TXRX_DBG_MASK_PROT_ANALYZE     = 0x04,
    TXRX_DBG_MASK_RX_REORDER_TRACE = 0x08,
    TXRX_DBG_MASK_RX_PN_TRACE      = 0x10
};

/*--- txrx printouts ---*/

/*
 * Uncomment this to enable txrx printouts with dynamically adjustable
 * verbosity.  These printouts should not impact performance.
 */
#define TXRX_PRINT_ENABLE 0
/* uncomment this for verbose txrx printouts (may impact performance) */
//#define TXRX_PRINT_VERBOSE_ENABLE 1

/*--- txrx object (pdev, vdev, peer) display debug functions ---*/

#ifndef TXRX_DEBUG_LEVEL
#define TXRX_DEBUG_LEVEL 0 /* no debug info */
#endif

#if TXRX_DEBUG_LEVEL > 5
void ol_txrx_pdev_display(ol_txrx_pdev_handle pdev, int indent);
void ol_txrx_vdev_display(ol_txrx_vdev_handle vdev, int indent);
void ol_txrx_peer_display(ol_txrx_peer_handle peer, int indent);
#else
#define ol_txrx_pdev_display(pdev, indent)
#define ol_txrx_vdev_display(vdev, indent)
#define ol_txrx_peer_display(peer, indent)
#endif

/*--- txrx stats display debug functions ---*/

#if TXRX_STATS_LEVEL != TXRX_STATS_LEVEL_OFF

void ol_txrx_stats_display(ol_txrx_pdev_handle pdev);


#else
#define ol_txrx_stats_display(pdev)
#define ol_txrx_stats_publish(pdev, buf) TXRX_STATS_LEVEL_OFF
#endif /* TXRX_STATS_LEVEL */

/*--- txrx protocol analyzer debug feature ---*/

/* uncomment this to enable the protocol analzyer feature */
//#define ENABLE_TXRX_PROT_ANALYZE 1

#if defined(ENABLE_TXRX_PROT_ANALYZE)

void ol_txrx_prot_ans_display(ol_txrx_pdev_handle pdev);

#else

#define ol_txrx_prot_ans_display(pdev)

#endif /* ENABLE_TXRX_PROT_ANALYZE */

/*--- txrx sequence number trace debug feature ---*/

/* uncomment this to enable the rx reorder trace feature */
//#define ENABLE_RX_REORDER_TRACE 1

#define ol_txrx_seq_num_trace_display(pdev) \
    ol_rx_reorder_trace_display(pdev, 0, 0)

#if defined(ENABLE_RX_REORDER_TRACE)

void
ol_rx_reorder_trace_display(ol_txrx_pdev_handle pdev, int just_once, int limit);

#else

#define ol_rx_reorder_trace_display(pdev, just_once, limit)

#endif /* ENABLE_RX_REORDER_TRACE */

/*--- txrx packet number trace debug feature ---*/

/* uncomment this to enable the rx PN trace feature */
//#define ENABLE_RX_PN_TRACE 1

#define ol_txrx_pn_trace_display(pdev) ol_rx_pn_trace_display(pdev, 0)

#if defined(ENABLE_RX_PN_TRACE)

void
ol_rx_pn_trace_display(ol_txrx_pdev_handle pdev, int just_once);

#else

#define ol_rx_pn_trace_display(pdev, just_once)

#endif /* ENABLE_RX_PN_TRACE */

#endif /* _OL_TXRX_DBG__H_ */
