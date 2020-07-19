/*
 * Copyright (c) 2011, 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _OL_TXRX_INTERNAL__H_
#define _OL_TXRX_INTERNAL__H_

#include <qdf_util.h>   /* qdf_assert */
#include <qdf_nbuf.h>      /* qdf_nbuf_t */
#include <qdf_mem.h>    /* qdf_mem_set */

#include <ol_htt_rx_api.h> /* htt_rx_msdu_desc_completes_mpdu, etc. */
#include <ol_txrx_types.h>

#include <ol_txrx_dbg.h>
#include <ol_txrx_api.h>

#ifndef TXRX_ASSERT_LEVEL
#define TXRX_ASSERT_LEVEL 3
#endif

#define TXRX_PEER_STATS_ADD(_peer, _field, _delta) \
    _peer->stats._field += _delta

#if TXRX_ASSERT_LEVEL > 0
#define TXRX_ASSERT1(condition) qdf_assert((condition))
#else
#define TXRX_ASSERT1(condition)
#endif

#if TXRX_ASSERT_LEVEL > 1
#define TXRX_ASSERT2(condition) qdf_assert((condition))
#else
#define TXRX_ASSERT2(condition)
#endif

enum {
    /* FATAL_ERR - print only irrecoverable error messages */
    TXRX_PRINT_LEVEL_FATAL_ERR,

    /* ERR - include non-fatal err messages */
    TXRX_PRINT_LEVEL_ERR,

    /* WARN - include warnings */
    TXRX_PRINT_LEVEL_WARN,

    /* INFO1 - include fundamental, infrequent events */
    TXRX_PRINT_LEVEL_INFO1,

    /* INFO2 - include non-fundamental but infrequent events */
    TXRX_PRINT_LEVEL_INFO2,

    /* INFO3 - include frequent events */
    /* to avoid performance impact, don't use INFO3 unless explicitly enabled */
#if TXRX_PRINT_VERBOSE_ENABLE
    TXRX_PRINT_LEVEL_INFO3,
#endif /* TXRX_PRINT_VERBOSE_ENABLE */
};

extern unsigned g_txrx_print_level;

#if TXRX_PRINT_ENABLE
#include <stdarg.h>       /* va_list */
#include <qdf_types.h> /* qdf_vprint */

#define ol_txrx_print(level, fmt, ...) \
    if(level <= g_txrx_print_level) qdf_print(fmt, ## __VA_ARGS__)
#define TXRX_PRINT(level, fmt, ...) \
    ol_txrx_print(level, "TXRX: " fmt, ## __VA_ARGS__)

#if TXRX_PRINT_VERBOSE_ENABLE

#define ol_txrx_print_verbose(fmt, ...) \
    if(TXRX_PRINT_LEVEL_INFO3 <= g_txrx_print_level) qdf_print(fmt, ## __VA_ARGS__)
#define TXRX_PRINT_VERBOSE(fmt, ...) \
    ol_txrx_print_verbose("TXRX: " fmt, ## __VA_ARGS__)
#else
#define TXRX_PRINT_VERBOSE(fmt, ...)
#endif /* TXRX_PRINT_VERBOSE_ENABLE */

#else
#define TXRX_PRINT(level, fmt, ...)
#define TXRX_PRINT_VERBOSE(fmt, ...)
#endif /* TXRX_PRINT_ENABLE */
#if MESH_MODE_SUPPORT
void ol_rx_mesh_mode_per_pkt_rx_info(qdf_nbuf_t nbuf, struct ol_txrx_peer_t *peer, struct ol_txrx_vdev_t *vdev);
#endif

#define OL_TXRX_LIST_APPEND(head, tail, elem) \
do {                                            \
    if (!(head)) {                              \
        (head) = (elem);                        \
    } else {                                    \
        qdf_nbuf_set_next((tail), (elem));      \
    }                                           \
    (tail) = (elem);                            \
} while (0)

static inline void
ol_rx_mpdu_list_next(
    struct ol_txrx_pdev_t *pdev,
    void *mpdu_list,
    qdf_nbuf_t *mpdu_tail,
    qdf_nbuf_t *next_mpdu)
{
    htt_pdev_handle htt_pdev = pdev->htt_pdev;
    qdf_nbuf_t msdu;

    /*
     * For now, we use a simply flat list of MSDUs.
     * So, traverse the list until we reach the last MSDU within the MPDU.
     */
    TXRX_ASSERT2(mpdu_list);
    msdu = mpdu_list;
    while (!htt_rx_msdu_desc_completes_mpdu(
                htt_pdev, htt_rx_msdu_desc_retrieve(htt_pdev, msdu)))
    {
        /* WAR: If LAST MSDU is not set, but next_msdu is NULL, then break the loop */
        if(qdf_nbuf_next(msdu) == NULL) {
           pdev->pdev_data_stats.rx.rx_last_msdu_unset_cnt++;
           break;
        }

        msdu = qdf_nbuf_next(msdu);
        TXRX_ASSERT2(msdu);
    }
    /* msdu now points to the last MSDU within the first MPDU */
    *mpdu_tail = msdu;
    *next_mpdu = qdf_nbuf_next(msdu);
}


/*--- txrx stats macros ---*/


/* unconditional defs */
/* default conditional defs (may be undefed below) */

#define TXRX_STATS_INCR(pdev, field) TXRX_STATS_ADD(pdev, field, 1)
#define TXRX_VDEV_STATS_INCR(vdev, field) TXRX_VDEV_STATS_ADD(vdev, field, 1)
#define OL_TXRX_STATS_INCR(pdev, field) OL_TXRX_STATS_ADD(pdev, field, 1)
#define TXRX_STATS_ADD(_pdev, _field, _delta) \
    _pdev->pdev_stats._field += _delta
#define TXRX_VDEV_STATS_ADD(_vdev, _field, _delta) \
    _vdev->stats._field += _delta
#define TXRX_PEER_STATS_ADD(_peer, _field, _delta) \
    _peer->stats._field += _delta
#define OL_TXRX_STATS_ADD(_pdev, _field, _delta) \
    _pdev->pdev_data_stats._field += _delta
#define TXRX_STATS_INIT(_pdev) \
    qdf_mem_set(&((_pdev)->pdev_stats), sizeof((_pdev)->pdev_stats), 0x0)
#define OL_TXRX_STATS_INIT(_pdev) \
    qdf_mem_set(&((_pdev)->pdev_data_stats), sizeof((_pdev)->pdev_data_stats), 0x0)
#define TXRX_STATS_SUB(_pdev, _field, _delta) \
    _pdev->pdev_stats._field -= _delta
#define OL_TXRX_STATS_SUB(_pdev, _field, _delta) \
    _pdev->pdev_data_stats._field -= _delta

#define OL_TXRX_STATS_MSDU_INCR(_pdev, field, netbuf) \
    do { \
        OL_TXRX_STATS_INCR((pdev), field.pkts);\
        OL_TXRX_STATS_ADD((pdev), field.bytes, qdf_nbuf_len(netbuf)); \
    } while (0)
#define TXRX_STATS_MSDU_INCR(pdev, field, netbuf) \
    do { \
        TXRX_STATS_INCR((pdev), field.num); \
        TXRX_STATS_ADD((pdev), field.bytes, qdf_nbuf_len(netbuf)); \
    } while (0)
#define TXRX_VDEV_STATS_MSDU_INCR(vdev, field, netbuf) \
    do { \
        TXRX_VDEV_STATS_INCR((vdev), field.num); \
        TXRX_VDEV_STATS_ADD((vdev), field.bytes, qdf_nbuf_len(netbuf)); \
    } while (0)

/* conditional defs based on verbosity level */

#if /*---*/ TXRX_STATS_LEVEL == TXRX_STATS_LEVEL_FULL

#define TXRX_STATS_MSDU_LIST_INCR(pdev, field, netbuf_list) \
    do { \
        qdf_nbuf_t tmp_list = netbuf_list; \
        while (tmp_list) { \
            TXRX_STATS_MSDU_INCR(pdev, field, tmp_list); \
            tmp_list = qdf_nbuf_next(tmp_list); \
        } \
    } while (0)

#define TXRX_VDEV_STATS_MSDU_LIST_INCR(vdev, field, netbuf_list) \
    do { \
        qdf_nbuf_t tmp_list = netbuf_list; \
        while (tmp_list) { \
            TXRX_VDEV_STATS_MSDU_INCR(vdev, field, tmp_list); \
            tmp_list = qdf_nbuf_next(tmp_list); \
        } \
    } while (0)
#define OL_TXRX_STATS_MSDU_LIST_INCR(pdev, field, netbuf_list) \
   do { \
       qdf_nbuf_t tmp_list = netbuf_list; \
       while (tmp_list) { \
           OL_TXRX_STATS_MSDU_INCR(pdev, field, tmp_list); \
           tmp_list = qdf_nbuf_next(tmp_list); \
       }\
   } while (0)

#elif /*---*/ TXRX_STATS_LEVEL == TXRX_STATS_LEVEL_BASIC
#define TXRX_STATS_MSDU_LIST_INCR(pdev, field, netbuf_list)
#define TXRX_VDEV_STATS_MSDU_LIST_INCR(vdev, field, netbuf_list)
#define OL_TXRX_STATS_MSDU_LIST_INCR(pdev, field, netbuf_list)

#define TXRX_STATS_MSDU_INCR_TX_STATUS(status, pdev, netbuf) \
    do { \
        if (status == htt_tx_status_ok) { \
            TXRX_STATS_MSDU_INCR(pdev, tx.comp_pkt, netbuf); \
        } \
    } while (0)

#define TXRX_STATS_INIT(_pdev) \
    qdf_mem_set(&((_pdev)->pdev_stats), sizeof((_pdev)->pdev_stats), 0x0)
#define OL_TXRX_STATS_INIT(_pdev) \
    qdf_mem_set(&((_pdev)->pdev_data_stats), sizeof((_pdev)->pdev_data_stats), 0x0)

static inline void txrx_stats_update_tx_stats(struct ol_txrx_peer_t *peer,
        enum htt_tx_status status, int p_cntrs, uint32_t b_cntrs)
{
    switch (status) {
    case htt_tx_status_ok:
       TXRX_PEER_STATS_ADD(peer, tx.comp_pkt.num, p_cntrs);
       TXRX_PEER_STATS_ADD(peer, tx.comp_pkt.bytes, b_cntrs);
        break;
    case htt_tx_status_discard:
       if (peer) {
       TXRX_VDEV_STATS_ADD(peer, tx.dropped.fw_rem.num, p_cntrs);
       TXRX_VDEV_STATS_ADD(peer, tx.dropped.fw_rem.bytes, b_cntrs);
       }
        break;
    case htt_tx_status_no_ack:
       TXRX_PEER_STATS_ADD(peer, tx.is_tx_no_ack.bytes, b_cntrs);
       TXRX_PEER_STATS_ADD(peer, tx.is_tx_no_ack.num, p_cntrs);
        break;
    case htt_tx_status_download_fail:
       if (peer && peer->vdev) {
       OL_TXRX_STATS_ADD(peer->vdev->pdev, tx.dropped.download_fail.bytes, b_cntrs);
       OL_TXRX_STATS_ADD(peer->vdev->pdev, tx.dropped.download_fail.pkts, p_cntrs);
       }
       break;
    default:
        break;
    }
}

#else /*---*/ /* stats off */
#undef TXRX_STATS_INIT
#define TXRX_STATS_INIT(_pdev)
#undef OL_TXRX_STATS_INIT
#define OL_TXRX_STATS_INIT(_pdev)


#undef TXRX_STATS_ADD
#define TXRX_STATS_ADD(_pdev, _field, _delta)
#define TXRX_STATS_SUB(_pdev, _field, _delta)
#undef OL_TXRX_STATS_ADD
#define OL_TXRX_STATS_ADD(_pdev, _field, _delta)
#define OL_TXRX_STATS_SUB(_pdev, _field, _delta)

#undef TXRX_STATS_MSDU_INCR
#define TXRX_STATS_MSDU_INCR(pdev, field, netbuf)
#undef TXRX_VDEV_STATS_MSDU_INCR
#define TXRX_VDEV_STATS_MSDU_INCR(vdev, field, netbuf)
#undef OL_TXRX_STATS_MSDU_INCR
#define OL_TXRX_STATS_MSDU_INCR(pdev, field, netbuf)

#define TXRX_STATS_MSDU_LIST_INCR(pdev, field, netbuf_list)
#define TXRX_VDEV_STATS_MSDU_LIST_INCR(vdev, field, netbuf_list)
#define OL_TXRX_STATS_MSDU_LIST_INCR(pdev, field, netbuf_list)

#define TXRX_STATS_MSDU_INCR_TX_STATUS(status, pdev, netbuf)

#define TXRX_STATS_UPDATE_TX_STATS(_peer, _status, _p_cntrs, _b_cntrs)

#endif /*---*/ /* TXRX_STATS_LEVEL */

/*--- txrx sequence number trace macros ---*/


#define TXRX_SEQ_NUM_ERR(_status) (0xffff - _status)

#if defined(ENABLE_RX_REORDER_TRACE)

A_STATUS ol_rx_reorder_trace_attach(ol_txrx_pdev_handle pdev);
void ol_rx_reorder_trace_detach(ol_txrx_pdev_handle pdev);
void ol_rx_reorder_trace_add(
    ol_txrx_pdev_handle pdev,
    u_int8_t tid,
    u_int16_t seq_num,
    int num_mpdus);

#define OL_RX_REORDER_TRACE_ATTACH ol_rx_reorder_trace_attach
#define OL_RX_REORDER_TRACE_DETACH ol_rx_reorder_trace_detach
#define OL_RX_REORDER_TRACE_ADD    ol_rx_reorder_trace_add

#else

#define OL_RX_REORDER_TRACE_ATTACH(_pdev) A_OK
#define OL_RX_REORDER_TRACE_DETACH(_pdev)
#define OL_RX_REORDER_TRACE_ADD(pdev, tid, seq_num, num_mpdus)

#endif /* ENABLE_RX_REORDER_TRACE */


/*--- txrx packet number trace macros ---*/


#if defined(ENABLE_RX_PN_TRACE)

A_STATUS ol_rx_pn_trace_attach(ol_txrx_pdev_handle pdev);
void ol_rx_pn_trace_detach(ol_txrx_pdev_handle pdev);
void ol_rx_pn_trace_add(
    struct ol_txrx_pdev_t *pdev,
    struct ol_txrx_peer_t *peer,
    u_int16_t tid,
    void *rx_desc);

#define OL_RX_PN_TRACE_ATTACH ol_rx_pn_trace_attach
#define OL_RX_PN_TRACE_DETACH ol_rx_pn_trace_detach
#define OL_RX_PN_TRACE_ADD    ol_rx_pn_trace_add

#else

#define OL_RX_PN_TRACE_ATTACH(_pdev) A_OK
#define OL_RX_PN_TRACE_DETACH(_pdev)
#define OL_RX_PN_TRACE_ADD(pdev, peer, tid, rx_desc)

#endif /* ENABLE_RX_PN_TRACE */

#if defined(WLAN_FEATURE_FASTPATH)
extern void ol_vap_tx_lock(void *);
extern void ol_vap_tx_unlock(void *);

qdf_nbuf_t ol_tx_reinject_cachedhdr(
        struct ol_txrx_vdev_t *vdev,
        qdf_nbuf_t msdu, uint32_t peer_id);

#if !QCA_OL_TX_PDEV_LOCK
#define OL_TX_REINJECT(_pdev, _vdev, _msdu, peer_id)  \
{ \
	ol_vap_tx_lock(_vdev->osif_vdev); \
	ol_tx_reinject_cachedhdr(_vdev, _msdu, peer_id); \
	ol_vap_tx_unlock(_vdev->osif_vdev);\
}
#else

#define OL_TX_REINJECT(_pdev, _vdev, _msdu, peer_id)  \
{ \
	OL_TX_LOCK(_vdev); \
	ol_tx_reinject_cachedhdr(_vdev, _msdu, peer_id); \
	OL_TX_UNLOCK(_vdev);\
}
#endif


#define  HTT_REMOVE_CACHED_HDR(_msdu,_size) 	        qdf_nbuf_pull_head(_msdu, _size);

#elif WLAN_FEATURE_FASTPATH

qdf_nbuf_t ol_tx_reinject_fast(
        struct ol_txrx_vdev_t *vdev,
        qdf_nbuf_t msdu, uint32_t peer_id);
#define OL_TX_REINJECT(_pdev, _vdev, _msdu, peer_id)  \
	qdf_nbuf_map_single(_pdev->osdev, _msdu, QDF_DMA_TO_DEVICE); \
	ol_tx_reinject_fast(_vdev, _msdu, peer_id)

#define  HTT_REMOVE_CACHED_HDR(_msdu,_size)
#else

qdf_nbuf_t
ol_tx_reinject(
        struct ol_txrx_vdev_t *vdev,
        qdf_nbuf_t msdu, uint32_t peer_id);
#define OL_TX_REINJECT(_pdev, _vdev, _msdu, peer_id)  \
	qdf_nbuf_map_single(_pdev->osdev, _msdu, QDF_DMA_TO_DEVICE); \
        ol_tx_reinject(_vdev, _msdu, peer_id)

#define  HTT_REMOVE_CACHED_HDR(_msdu,_size)

#endif /*WLAN_FEATURE_FASTPATH*/

#if defined(WLAN_FEATURE_FASTPATH)

extern void ol_vap_tx_lock(void *);
#if !QCA_OL_TX_PDEV_LOCK
#define OL_VDEV_TX(_vdev, _msdu, _oshandle) \
{ \
    ol_vap_tx_lock(_vdev->osif_vdev); \
    OL_TX_LL_WRAPPER(_vdev, _msdu, _oshandle); \
    ol_vap_tx_unlock(_vdev->osif_vdev); \
}
#else
#define OL_VDEV_TX(_vdev, _msdu, _oshandle) \
{ \
    OL_TX_LOCK(_vdev); \
    OL_TX_LL_WRAPPER(_vdev, _msdu, _oshandle); \
    OL_TX_UNLOCK(_vdev); \
}
#endif

#elif WLAN_FEATURE_FASTPATH

#define OL_VDEV_TX(_vdev, _msdu, _oshandle) \
           OL_TX_LL_WRAPPER(_vdev, _msdu, _oshandle);

#else
#define OL_VDEV_TX(_vdev, _msdu, _oshandle) \
{\
    qdf_nbuf_map_single(_oshandle, _msdu, QDF_DMA_TO_DEVICE);\
    if (vdev->tx(_vdev, _msdu))  { \
        qdf_nbuf_unmap_single(_oshandle, _msdu, QDF_DMA_TO_DEVICE);\
        qdf_nbuf_free(_msdu); \
    }\
}

#endif /* WLAN_FEATURE_FASTPATH */

#if defined(WLAN_FEATURE_FASTPATH) && (!QCA_OL_TX_PDEV_LOCK)
#define OL_VDEV_TX_MCAST_ENHANCE(_vdev, _msdu, _vap) \
{ \
        ol_vap_tx_lock(_vdev->osif_vdev); \
        if( ol_tx_mcast_enhance_process(_vap, _msdu) > 0 ) { \
            ol_vap_tx_unlock(_vdev->osif_vdev); \
            ol_ath_release_vap(_vap);\
            return; \
        } \
        ol_vap_tx_unlock(_vdev->osif_vdev); \
}
#else
#define OL_VDEV_TX_MCAST_ENHANCE(_vdev, _msdu, _vap) \
{ \
        OL_TX_LOCK(_vdev); \
        if( ol_tx_mcast_enhance_process(_vap, _msdu) > 0 ) { \
            OL_TX_UNLOCK(_vdev); \
            ol_ath_release_vap(_vap);\
            return; \
        } \
        OL_TX_UNLOCK(_vdev); \
}
#endif

#define OL_TXRX_STATS_AGGR(_handle_a, _handle_b, _field) \
{ \
	_handle_a->stats._field += _handle_b->stats._field; \
}

#define OL_TXRX_STATS_AGGR_PKT(_handle_a, _handle_b, _field) \
{ \
	OL_TXRX_STATS_AGGR(_handle_a, _handle_b, _field.num); \
	OL_TXRX_STATS_AGGR(_handle_a, _handle_b, _field.bytes);\
}

#define OL_TXRX_STATS_UPD_STRUCT(_handle_a, _handle_b, _field) \
{ \
	_handle_a->stats._field = _handle_b->stats._field; \
}

static inline void ol_txrx_update_stats(struct ol_txrx_vdev_t *_tgtobj,
        struct ol_txrx_peer_t *_srcobj)
{
		uint8_t i;
		uint8_t pream_type;
		for (pream_type = 0; pream_type < DOT11_MAX; pream_type++) {
			for (i = 0; i < MAX_MCS; i++) {
				OL_TXRX_STATS_AGGR(_tgtobj, _srcobj,
					tx.pkt_type[pream_type].mcs_count[i]);
				OL_TXRX_STATS_AGGR(_tgtobj, _srcobj,
					rx.pkt_type[pream_type].mcs_count[i]);
			}
		}

		for (i = 0; i < MAX_BW; i++) {
			OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.bw[i]);
			OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, rx.bw[i]);
		}

		for (i = 0; i < SS_COUNT; i++) {
			OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, rx.nss[i]);
			OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.nss[i]);
		}
		for (i = 0; i < WME_AC_MAX; i++) {
			OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.wme_ac_type[i]);
			OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, rx.wme_ac_type[i]);
			OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.excess_retries_per_ac[i]);

		}

		for (i = 0; i < MAX_GI; i++) {
			OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.sgi_count[i]);
			OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, rx.sgi_count[i]);
		}

		for (i = 0; i < MAX_RECEPTION_TYPES; i++)
			OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, rx.reception_type[i]);

		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.comp_pkt);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.ucast);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.mcast);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.bcast);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.tx_success);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.dot11_tx_pkts);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.nawds_mcast);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.tx_failed);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.ofdma);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.stbc);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.ldpc);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.retries);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.non_amsdu_cnt);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.amsdu_cnt);
		if (_srcobj->stats.tx.last_tx_rate != 0)
			OL_TXRX_STATS_UPD_STRUCT(_tgtobj, _srcobj, tx.last_tx_rate);
		if (_srcobj->stats.tx.last_tx_rate_mcs != 0)
			OL_TXRX_STATS_UPD_STRUCT(_tgtobj, _srcobj, tx.last_tx_rate_mcs);
		if (_srcobj->stats.tx.mcast_last_tx_rate != 0)
			OL_TXRX_STATS_UPD_STRUCT(_tgtobj, _srcobj, tx.mcast_last_tx_rate);
		if (_srcobj->stats.tx.mcast_last_tx_rate_mcs != 0)
			OL_TXRX_STATS_UPD_STRUCT(_tgtobj, _srcobj, tx.mcast_last_tx_rate_mcs);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.dropped.fw_rem);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_rem_tx);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_rem_notx);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_reason1);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_reason2);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_reason3);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.age_out);
								\
		OL_TXRX_STATS_UPD_STRUCT(_tgtobj, _srcobj, rx.rssi);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, rx.non_ampdu_cnt);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, rx.ampdu_cnt);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, rx.non_amsdu_cnt);
		OL_TXRX_STATS_AGGR(_tgtobj, _srcobj, rx.amsdu_cnt);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.to_stack);

		for (i = 0; i <  CDP_MAX_RX_RINGS; i++)
			OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.rcvd_reo[i]);

		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.unicast);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.multicast);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.bcast);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.raw);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.intra_bss.pkts);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.intra_bss.fail);

		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.raw);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.intra_bss.pkts);
		OL_TXRX_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.intra_bss.fail);

		_tgtobj->stats.tx.last_ack_rssi =
			_srcobj->stats.tx.last_ack_rssi;
}

#define OL_TXRX_VDEV_STATS_AGGR(_handle_a, _handle_b, _field) \
{ \
	_handle_a->_field += _handle_b->stats._field; \
}

#define OL_TXRX_VDEV_STATS_AGGR_PKT(_handle_a, _handle_b, _field) \
{ \
	OL_TXRX_VDEV_STATS_AGGR(_handle_a, _handle_b, _field.num); \
	OL_TXRX_VDEV_STATS_AGGR(_handle_a, _handle_b, _field.bytes);\
}

#define OL_TXRX_VDEV_STATS_UPD_STRUCT(_handle_a, _handle_b, _field) \
{ \
	_handle_a->_field = _handle_b->stats._field; \
}

static inline void ol_txrx_vdev_update_stats(struct cdp_vdev_stats *_tgtobj,
       struct ol_txrx_peer_t *_srcobj)
{
		uint8_t i;
		uint8_t pream_type;
		for (pream_type = 0; pream_type < DOT11_MAX; pream_type++) {
			for (i = 0; i < MAX_MCS; i++) {
				OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj,
					tx.pkt_type[pream_type].mcs_count[i]);
				OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj,
					rx.pkt_type[pream_type].mcs_count[i]);
			}
		}

		for (i = 0; i < MAX_BW; i++) {
			OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.bw[i]);
			OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, rx.bw[i]);
		}

		for (i = 0; i < SS_COUNT; i++) {
			OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, rx.nss[i]);
			OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.nss[i]);
		}
		for (i = 0; i < WME_AC_MAX; i++) {
			OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.wme_ac_type[i]);
			OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, rx.wme_ac_type[i]);
			OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.excess_retries_per_ac[i]);

		}

		for (i = 0; i < MAX_GI; i++) {
			OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.sgi_count[i]);
			OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, rx.sgi_count[i]);
		}

		for (i = 0; i < MAX_RECEPTION_TYPES; i++)
			OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, rx.reception_type[i]);

		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.comp_pkt);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.ucast);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.mcast);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.bcast);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.tx_success);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.nawds_mcast);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.tx_failed);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.ofdma);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.stbc);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.ldpc);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.retries);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.non_amsdu_cnt);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.amsdu_cnt);
		if (_srcobj->stats.tx.last_tx_rate != 0)
			OL_TXRX_VDEV_STATS_UPD_STRUCT(_tgtobj, _srcobj, tx.last_tx_rate);
		if (_srcobj->stats.tx.last_tx_rate_mcs != 0)
			OL_TXRX_VDEV_STATS_UPD_STRUCT(_tgtobj, _srcobj, tx.last_tx_rate_mcs);
		if (_srcobj->stats.tx.mcast_last_tx_rate != 0)
			OL_TXRX_VDEV_STATS_UPD_STRUCT(_tgtobj, _srcobj, tx.mcast_last_tx_rate);
		if (_srcobj->stats.tx.mcast_last_tx_rate_mcs != 0)
			OL_TXRX_VDEV_STATS_UPD_STRUCT(_tgtobj, _srcobj, tx.mcast_last_tx_rate_mcs);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.dropped.fw_rem);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_rem_tx);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_rem_notx);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_reason1);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_reason2);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_reason3);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.age_out);

		OL_TXRX_VDEV_STATS_UPD_STRUCT(_tgtobj, _srcobj, rx.rssi);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, rx.non_ampdu_cnt);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, rx.ampdu_cnt);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, rx.non_amsdu_cnt);
		OL_TXRX_VDEV_STATS_AGGR(_tgtobj, _srcobj, rx.amsdu_cnt);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.to_stack);

		for (i = 0; i <  CDP_MAX_RX_RINGS; i++)
			OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.rcvd_reo[i]);

		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.unicast);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.multicast);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.bcast);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.raw);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.intra_bss.pkts);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.intra_bss.fail);

		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.raw);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.intra_bss.pkts);
		OL_TXRX_VDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.intra_bss.fail);

		_tgtobj->tx.last_ack_rssi =
			_srcobj->stats.tx.last_ack_rssi;
}

#define OL_TXRX_PDEV_STATS_AGGR(_handle_a, _handle_b, _field) \
{ \
	_handle_a->pdev_stats._field += _handle_b->_field; \
}

#define OL_TXRX_PDEV_STATS_AGGR_PKT(_handle_a, _handle_b, _field) \
{ \
	OL_TXRX_PDEV_STATS_AGGR(_handle_a, _handle_b, _field.num); \
	OL_TXRX_PDEV_STATS_AGGR(_handle_a, _handle_b, _field.bytes);\
}

#define OL_TXRX_PDEV_STATS_UPD_STRUCT(_handle_a, _handle_b, _field) \
{ \
	_handle_a->pdev_stats._field = _handle_b->_field; \
}

static inline void ol_txrx_pdev_update_stats(struct ol_txrx_pdev_t *_tgtobj,
       struct cdp_vdev_stats * _srcobj)
{
		uint8_t i;
		uint8_t pream_type;
		for (pream_type = 0; pream_type < DOT11_MAX; pream_type++) {
			for (i = 0; i < MAX_MCS; i++) {
				OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj,
					tx.pkt_type[pream_type].mcs_count[i]);
				OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj,
					rx.pkt_type[pream_type].mcs_count[i]);
			}
		}

		for (i = 0; i < MAX_BW; i++) {
			OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.bw[i]);
			OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, rx.bw[i]);
		}

		for (i = 0; i < SS_COUNT; i++) {
			OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, rx.nss[i]);
			OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.nss[i]);
		}
		for (i = 0; i < WME_AC_MAX; i++) {
			OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.wme_ac_type[i]);
			OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, rx.wme_ac_type[i]);
			OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.excess_retries_per_ac[i]);

		}

		for (i = 0; i < MAX_GI; i++) {
			OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.sgi_count[i]);
			OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, rx.sgi_count[i]);
		}

		for (i = 0; i < MAX_RECEPTION_TYPES; i++)
			OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, rx.reception_type[i]);

		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.comp_pkt);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.ucast);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.mcast);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.bcast);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.tx_success);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.nawds_mcast);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.tx_failed);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.ofdma);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.stbc);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.ldpc);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.retries);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.non_amsdu_cnt);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.amsdu_cnt);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, tx.dropped.fw_rem);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_rem_tx);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_rem_notx);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_reason1);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_reason2);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.fw_reason3);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, tx.dropped.age_out);

		OL_TXRX_PDEV_STATS_UPD_STRUCT(_tgtobj, _srcobj, rx.rssi);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, rx.non_ampdu_cnt);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, rx.ampdu_cnt);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, rx.non_amsdu_cnt);
		OL_TXRX_PDEV_STATS_AGGR(_tgtobj, _srcobj, rx.amsdu_cnt);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.to_stack);

		for (i = 0; i <  CDP_MAX_RX_RINGS; i++)
			OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.rcvd_reo[i]);

		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.unicast);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.multicast);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.bcast);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.raw);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.intra_bss.pkts);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.intra_bss.fail);

		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.raw);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.intra_bss.pkts);
		OL_TXRX_PDEV_STATS_AGGR_PKT(_tgtobj, _srcobj, rx.intra_bss.fail);

		_tgtobj->pdev_stats.tx.last_ack_rssi =
			_srcobj->tx.last_ack_rssi;
}
#define OL_TXRX_STATS_AGGR_PDEV(_handle_a, _handle_b, _field) \
{ \
	_handle_a->pdev_stats._field += _handle_b->stats._field; \
}

#define OL_TXRX_STATS_AGGR_PKT_PDEV(_handle_a, _handle_b, _field) \
{ \
	OL_TXRX_STATS_AGGR_PDEV(_handle_a, _handle_b, _field.num); \
	OL_TXRX_STATS_AGGR_PDEV(_handle_a, _handle_b, _field.bytes);\
}

#define OL_TXRX_STATS_UPD_STRUCT_PDEV(_handle_a, _handle_b, _field) \
{ \
	_handle_a->pdev_stats._field = _handle_b->stats._field; \
}
void
ol_tx_stats_inc_pkt_cnt(ol_txrx_vdev_handle vdev, void *buf);
#endif /* _OL_TXRX_INTERNAL__H_ */
