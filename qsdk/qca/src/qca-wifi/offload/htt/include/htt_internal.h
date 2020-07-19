/*
 * Copyright (c) 2011-2015, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2011-2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef _HTT_INTERNAL__H_
#define _HTT_INTERNAL__H_

#include <athdefs.h>     /* A_STATUS */
#include <qdf_nbuf.h>    /* qdf_nbuf_t */
#include <qdf_util.h> /* qdf_assert */
#include <htc_api.h>     /* HTC_PACKET */

#include <htt_types.h>

#ifndef offsetof
#define offsetof(type, field)   ((size_t)(&((type *)0)->field))
#endif

#undef MS
#define MS(_v, _f) (((_v) & _f##_MASK) >> _f##_LSB)
#undef SM
#define SM(_v, _f) (((_v) << _f##_LSB) & _f##_MASK)
#undef WO
#define WO(_f)      ((_f##_OFFSET) >> 2)

#define GET_FIELD(_addr, _f) MS(*((A_UINT32 *)(_addr) + WO(_f)), _f)

#include <wal_rx_desc.h> /* struct rx_attention, etc */

#define RX_STD_DESC_SIZE (pdev->ar_rx_ops->sizeof_rx_desc())

#define RX_STD_DESC_SIZE_DWORD              (RX_STD_DESC_SIZE >> 2)

/*
 * Make sure there is a minimum headroom provided in the rx netbufs
 * for use by the OS shim and OS and rx data consumers.
 */
#define HTT_RX_BUF_OS_MIN_HEADROOM 32
#define HTT_RX_STD_DESC_RESERVATION  \
    ((HTT_RX_BUF_OS_MIN_HEADROOM > RX_STD_DESC_SIZE) ? \
    HTT_RX_BUF_OS_MIN_HEADROOM : RX_STD_DESC_SIZE)
#define HTT_RX_STD_DESC_RESERVATION_DWORD \
    (HTT_RX_STD_DESC_RESERVATION >> 2)

#define HTT_RX_DESC_ALIGN_MASK 7 /* 8-byte alignment */
#ifndef HTT_RX_DESC_ALIGN_RESERVED
#define HTT_RX_DESC_ALIGN_RESERVED 0
#endif

#if (QCA_PARTNER_DIRECTLINK_RX || QCA_PARTNER_CBM_DIRECTPATH)
/* declare direct link specific API for rx desc processing */
#define QCA_PARTNER_DIRECTLINK_HTT_INTERNAL 1
#include "ath_carr_pltfrm.h"
#undef QCA_PARTNER_DIRECTLINK_HTT_INTERNAL
#endif

static inline
void *
htt_rx_desc(qdf_nbuf_t msdu)
{
#if (QCA_PARTNER_DIRECTLINK_RX || QCA_PARTNER_CBM_DIRECTPATH)
    if(is_partner_buffer(msdu))
    {
	return htt_rx_desc_partner(msdu);
    }
#endif
    return
        (void *)
            ((((size_t) (qdf_nbuf_head(msdu) + HTT_RX_DESC_ALIGN_MASK)) &
            ~HTT_RX_DESC_ALIGN_MASK) + HTT_RX_DESC_ALIGN_RESERVED);
}

#ifndef HTT_ASSERT_LEVEL
#define HTT_ASSERT_LEVEL 3
#endif

#define HTT_ASSERT_ALWAYS(condition) qdf_assert_always((condition))

#define HTT_ASSERT0(condition) qdf_assert((condition))
#if HTT_ASSERT_LEVEL > 0
#define HTT_ASSERT1(condition) qdf_assert((condition))
#else
#define HTT_ASSERT1(condition)
#endif

#if HTT_ASSERT_LEVEL > 1
#define HTT_ASSERT2(condition) qdf_assert((condition))
#else
#define HTT_ASSERT2(condition)
#endif

#if HTT_ASSERT_LEVEL > 2
#define HTT_ASSERT3(condition) qdf_assert((condition))
#else
#define HTT_ASSERT3(condition)
#endif


/*
 * HTT_MAX_SEND_QUEUE_DEPTH -
 * How many packets HTC should allow to accumulate in a send queue
 * before calling the EpSendFull callback to see whether to retain
 * or drop packets.
 * This is not relevant for LL, where tx descriptors should be immediately
 * downloaded to the target.
 * This is not very relevant for HL either, since it is anticipated that
 * the HL tx download scheduler will not work this far in advance - rather,
 * it will make its decisions just-in-time, so it can be responsive to
 * changing conditions.
 * Hence, this queue depth threshold spec is mostly just a formality.
 */
#define HTT_MAX_SEND_QUEUE_DEPTH 64


#define IS_PWR2(value) (((value) ^ ((value)-1)) == ((value) << 1) - 1)


/* FIX THIS
 * Should be: sizeof(struct htt_host_rx_desc) + max rx MSDU size,
 * rounded up to a cache line size.
 */
#define HTT_RX_BUF_SIZE 1920
/* the HW expects the buffer to be an integral number of 4-byte "words" */
A_COMPILE_TIME_ASSERT(htt_rx_buff_size_quantum1, (HTT_RX_BUF_SIZE & 0x3) == 0);
/*
 * DMA_MAP expects the buffer to be an integral number of cache lines.
 * Rather than checking the actual cache line size, this code makes a
 * conservative estimate of what the cache line size could be.
 */
#define HTT_LOG2_MAX_CACHE_LINE_SIZE 7 /* 2^7 = 128 */
#define HTT_MAX_CACHE_LINE_SIZE_MASK ((1 << HTT_LOG2_MAX_CACHE_LINE_SIZE) - 1)
A_COMPILE_TIME_ASSERT(htt_rx_buff_size_quantum2,
    (HTT_RX_BUF_SIZE & HTT_MAX_CACHE_LINE_SIZE_MASK) == 0);


#ifdef BIG_ENDIAN_HOST
/*
 * big-endian: bytes within a 4-byte "word" are swapped:
 * pre-swap  post-swap
 *  index     index
 *    0         3
 *    1         2
 *    2         1
 *    3         0
 *    4         7
 *    5         6
 * etc.
 * To compute the post-swap index from the pre-swap index, compute
 * the byte offset for the start of the word (index & ~0x3) and add
 * the swapped byte offset within the word (3 - (index & 0x3)).
 */
#define HTT_ENDIAN_BYTE_IDX_SWAP(idx) (((idx) & ~0x3) + (3 - ((idx) & 0x3)))
#else
/* little-endian: no adjustment needed */
#define HTT_ENDIAN_BYTE_IDX_SWAP(idx) idx
#endif


#define HTT_TX_MUTEX_INIT(_mutex)                          \
    qdf_spinlock_create(_mutex)

#define HTT_TX_MUTEX_ACQUIRE(_mutex)                       \
    qdf_spin_lock_bh(_mutex)

#define HTT_TX_MUTEX_RELEASE(_mutex)                       \
    qdf_spin_unlock_bh(_mutex)

#define HTT_TX_MUTEX_DESTROY(_mutex)                       \
    qdf_spinlock_destroy(_mutex)

#define HTT_TX_DESC_PADDR(_pdev, _tx_desc_vaddr)           \
    ((_pdev)->tx_descs.pool_paddr +  (u_int32_t)           \
     ((char *)(_tx_desc_vaddr) -                           \
      (char *)((_pdev)->tx_descs.pool_vaddr)))

#define HTT_ME_BUF_PADDR(_pdev, _me_buf_vaddr)           \
    ((_pdev)->me_buf.paddr +  (u_int32_t)                \
     ((char *)(_me_buf_vaddr) -                          \
      (char *)((_pdev)->me_buf.vaddr)))

#if ATH_11AC_TXCOMPACT

#define HTT_TX_NBUF_QUEUE_MUTEX_INIT(_pdev)                \
        qdf_spinlock_create(&_pdev->txnbufq_mutex)

#define HTT_TX_NBUF_QUEUE_MUTEX_DESTROY(_pdev)             \
        HTT_TX_MUTEX_DESTROY(&_pdev->txnbufq_mutex)

#define HTT_TX_NBUF_QUEUE_REMOVE(_pdev, _msdu)             \
        HTT_TX_MUTEX_ACQUIRE(&_pdev->txnbufq_mutex);       \
        _msdu =  qdf_nbuf_queue_remove(&_pdev->txnbufq);   \
        HTT_TX_MUTEX_RELEASE(&_pdev->txnbufq_mutex)

#define HTT_TX_NBUF_QUEUE_ADD(_pdev, _msdu)                \
        HTT_TX_MUTEX_ACQUIRE(&_pdev->txnbufq_mutex);       \
        qdf_nbuf_queue_add(&_pdev->txnbufq, _msdu);        \
        HTT_TX_MUTEX_RELEASE(&_pdev->txnbufq_mutex)

#define HTT_TX_NBUF_QUEUE_INSERT_HEAD(_pdev, _msdu)        \
        HTT_TX_MUTEX_ACQUIRE(&_pdev->txnbufq_mutex);       \
        qdf_nbuf_queue_insert_head(&_pdev->txnbufq, _msdu);\
        HTT_TX_MUTEX_RELEASE(&_pdev->txnbufq_mutex)

#ifdef BIG_ENDIAN_HOST

/* check assumption about vdev ID field position */
#if HTT_TX_DESC_VDEV_ID_S != 16
#error code update needed because tx desc vdev ID has moved
#endif

#if HTT_TX_DESC_FRM_LEN_S != 0
#error code update needed because tx desc frame lenfiled  has moved
#endif

#if HTT_TX_DESC_FRM_ID_S != 16
#error code update needed because tx desc frame ID has moved
#endif

#if HTT_TX_DESC_PKT_TYPE_S != 13
#error code update needed because tx desc packet type field has moved
#endif

#define BE_INV_HTT_TX_DESC_PKT_SUBTYPE_M     (0xFFE0FFFF)
#define BE_HTT_TX_DESC_PKT_SUBTYPE_S         (16)
#if HTT_TX_DESC_PKT_SUBTYPE_S != 8
#error code update needed because tx desc packet subtype field has moved
#endif

#define BE_HTT_TX_DESC_EXT_TID_M                (0xFFFF3FF8)
#define BE_HTT_TX_DESC_EXT_TID_BIT_0_TO_1_S     (14)
#define BE_TID_LOWER_TWO_BITS_M                 (0x3)
#define BE_TID_UPPER_THREE_BIT_S                (2)
#define BE_TID_UPPER_THREE_BITS_M               (0x7)
#define BE_HTT_TX_DESC_POSTPONED_M              (0x08)
#define BE_HTT_TX_DESC_POSTPONED_S              (3)

#define  htt_ffcache_update_vdev_id(_ffcache, _vdev_id) \
	    _ffcache[2] = (_ffcache[2]  & 0xFFFFC0FF )| ((_vdev_id) << 8)

#define  htt_ffcache_update_msdu_len_and_id(_ffcache, _msdu_len, _msdu_id) \
	    _ffcache[3] =  ((_msdu_len & 0xFF) << 24)   | ((_msdu_len & 0xFF00) << 8) | ((_msdu_id &0xFF) <<8) | (_msdu_id >> 8)

#define  htt_ffcache_update_pkttype(_ffcache, _pkt_type) \
            _ffcache[2] = (_ffcache[2]  & BE_INV_HTT_TX_DESC_PKT_TYPE_M) | ((_pkt_type) << BE_HTT_TX_DESC_PKT_TYPE_S)

#define  htt_ffcache_update_pktsubtype(_ffcache, _pkt_subtype) \
	    _ffcache[2] = (_ffcache[2] & BE_INV_HTT_TX_DESC_PKT_SUBTYPE_M) | ((_pkt_subtype) << BE_HTT_TX_DESC_PKT_SUBTYPE_S)

#define  htt_ffcache_update_tid(_ffcache, _tid) \
            ( _ffcache[2] = ((_ffcache[2]  & BE_HTT_TX_DESC_EXT_TID_M) |\
             (((_tid >> BE_TID_UPPER_THREE_BIT_S) & BE_TID_UPPER_THREE_BITS_M) |\
             ((_tid & BE_TID_LOWER_TWO_BITS_M) << BE_HTT_TX_DESC_EXT_TID_BIT_0_TO_1_S))))

#define  htt_ffcache_update_extvalid(_ffcache, _ext_valid) \
	    _ffcache[2] = (_ffcache[2]  & 0xFFFFFF7F )| ((_ext_valid) << 7)

#define  htt_ffcache_update_power(_ffcache, _power) \
	    _ffcache[6] =  (_ffcache[6]  & 0x00FFFFFF )| ((_power) << 24)
#define  htt_ffcache_update_ratecode(_ffcache, _ratecode) \
	    _ffcache[6] =  (_ffcache[6]  & 0xFF00FFFF )| ((_ratecode) << 16)
#define  htt_ffcache_update_rateretries(_ffcache, _rateretries) \
	    _ffcache[6] =  (_ffcache[6]  & 0xFFFFF0FF )| ((_rateretries) << 8)
#define  htt_ffcache_update_validflags(_ffcache, _validflags) \
	    _ffcache[6] =  (_ffcache[6]  & 0xFFFFFF00 )| ((_validflags))
#define  htt_ffcache_update_rate_info(_ffcache, _rate_info) \
	    _ffcache[6] = ((_rate_info & 0xFF) << 24) | ((_rate_info & 0xFF00) << 8) | ((_rate_info & 0xFF0000) >> 8) | ((_rate_info & 0xFF000000) >> 24)

#define  htt_ffcache_update_desc_postponed_set(_ffcache) \
        _ffcache[2] = (_ffcache[2] & ~(BE_HTT_TX_DESC_POSTPONED_M)) | ((1) << BE_HTT_TX_DESC_POSTPONED_S)
#define  htt_ffcache_update_desc_set_peer_id(_ffcache, _peerid) \
        _ffcache[5] = qdf_cpu_to_le32(_peerid)

#else /*BIG_ENDIAN_HOST*/

#define  htt_ffcache_update_vdev_id(_ffcache, _vdev_id) \
	    _ffcache[2] = (_ffcache[2] & ~(HTT_TX_DESC_VDEV_ID_M)) | ((_vdev_id) << HTT_TX_DESC_VDEV_ID_S)


#define  htt_ffcache_update_msdu_len_and_id(_ffcache, _msdu_len, _msdu_id) \
	    _ffcache[3] =  ((_msdu_len) << HTT_TX_DESC_FRM_LEN_S)  | (_msdu_id  << HTT_TX_DESC_FRM_ID_S)

#define  htt_ffcache_update_pkttype(_ffcache, _pkt_type) \
       _ffcache[2] = (_ffcache[2] & ~(HTT_TX_DESC_PKT_TYPE_M)) | ((_pkt_type) << HTT_TX_DESC_PKT_TYPE_S)

#define  htt_ffcache_update_pktsubtype(_ffcache, _pkt_subtype) \
	    _ffcache[2] = (_ffcache[2] & ~(HTT_TX_DESC_PKT_SUBTYPE_M)) | ((_pkt_subtype) << HTT_TX_DESC_PKT_SUBTYPE_S)

#define htt_ffcache_update_tid(_ffcache, _tid) \
        _ffcache[2] = (_ffcache[2] & ~(HTT_TX_DESC_EXT_TID_M)) | ((_tid) << HTT_TX_DESC_EXT_TID_S)

#define  htt_ffcache_update_extvalid(_ffcache, _ext_valid) \
	    _ffcache[2] = (_ffcache[2] & 0x7FFFFFFF) | ((_ext_valid) << 31)

#define  htt_ffcache_update_power(_ffcache, _power) \
	    _ffcache[6] =  (_ffcache[6]  & 0xFFFFFF00 )| (_power)
#define  htt_ffcache_update_ratecode(_ffcache, _ratecode) \
	    _ffcache[6] =  (_ffcache[6]  & 0xFFFF00FF )| ((_ratecode) << 8)
#define  htt_ffcache_update_rateretries(_ffcache, _rateretries) \
	    _ffcache[6] =  (_ffcache[6]  & 0xFFF0FFFF )| ((_rateretries) << 16)
#define  htt_ffcache_update_validflags(_ffcache, _validflags) \
	    _ffcache[6] =  (_ffcache[6]  & 0x00FFFFFF )| ((_validflags) << 24)
#define  htt_ffcache_update_rate_info(_ffcache, _rate_info) \
	    _ffcache[6] = (_rate_info)

#define  htt_ffcache_update_desc_postponed_set(_ffcache) \
	_ffcache[2] = (_ffcache[2] & ~(HTT_TX_DESC_POSTPONED_M)) | ((1) << HTT_TX_DESC_POSTPONED_S)
#define  htt_ffcache_update_desc_set_peer_id(_ffcache, _peerid) \
	_ffcache[5] = _peerid

#endif /*end of BIG_ENDIAN_HOST*/

#define  htt_data_get_vdev_id(_data)   (_data[10] & 0x3f)


#define  htt_ffcache_update_descphy_addr(_ffcache, _htt_word) \
	    _ffcache[4] = qdf_cpu_to_le32(_htt_word[2])

#define  htt_ffcache_update_msduavglen(_pdev, _ffcache, _len) \
	   _ffcache[4] = qdf_cpu_to_le32(_len) ;



#else

#define HTT_TX_NBUF_QUEUE_MUTEX_INIT(_pdev)
#define HTT_TX_NBUF_QUEUE_REMOVE(_pdev, _msdu)
#define HTT_TX_NBUF_QUEUE_ADD(_pdev, _msdu)
#define HTT_TX_NBUF_QUEUE_INSERT_HEAD(_pdev, _msdu)
#define HTT_TX_NBUF_QUEUE_MUTEX_DESTROY(_pdev)

#endif

#if QCA_OL_TX_PDEV_LOCK
#define HTT_TX_PDEV_LOCK(x) qdf_spin_lock_bh(x)
#define HTT_TX_PDEV_UNLOCK(x) qdf_spin_unlock_bh(x)
#else
#define HTT_TX_PDEV_LOCK(x)
#define HTT_TX_PDEV_UNLOCK(x)
#endif

/*
 * enums to set tx_params_mcs, tx_params_nss, tx_params_pream_type,
 * tx_params_num_retries variables of the  htt_mgmt_tx_desc_t
 */
enum HTT_HW_RATECODE_PREAM_TYPE {
    HTT_HW_RATECODE_PREAM_OFDM,
    HTT_HW_RATECODE_PREAM_CCK,
    HTT_HW_RATECODE_PREAM_HT,
    HTT_HW_RATECODE_PREAM_VHT,
};

enum HTT_HW_RATE_OFDM {
    HTT_HW_RATE_OFDM_48_MBPS, /* OFDM 48 Mbps */
    HTT_HW_RATE_OFDM_24_MBPS, /* OFDM 24 Mbps */
    HTT_HW_RATE_OFDM_12_MBPS, /* OFDM 12 Mbps */
    HTT_HW_RATE_OFDM_6_MBPS,  /* OFDM  6 Mbps */
    HTT_HW_RATE_OFDM_54_MBPS, /* OFDM 54 Mbps */
    HTT_HW_RATE_OFDM_36_MBPS, /* OFDM 36 Mbps */
    HTT_HW_RATE_OFDM_18_MBPS, /* OFDM 18 Mbps */
    HTT_HW_RATE_OFDM_9_MBPS,  /* OFDM  9 Mbps */
};

enum HTT_HW_RATE_CCK {
    HTT_HW_RATE_CCK_11_LONG_MBPS,   /* CCK 11 Mbps long */
    HTT_HW_RATE_CCK_5_5_LONG_MBPS,  /* CCK 5.5 Mbps long */
    HTT_HW_RATE_CCK_2_LONG_MBPS,    /* CCK  2 Mbps long */
    HTT_HW_RATE_CCK_1_LONG_MBPS,    /* CCK  1 Mbps long */
    HTT_HW_RATE_CCK_11_SHORT_MBPS,  /* CCK 11 Mbps short */
    HTT_HW_RATE_CCK_5_5_SHORT_MBPS, /* CCK 5.5 Mbps short */
    HTT_HW_RATE_CCK_2_SHORT_MBPS,   /* CCK  2 Mbps short */
};
#define HTT_GET_HW_RATECODE_PREAM(_rcode)     (((_rcode) >> 6) & 0x3)
#define HTT_GET_HW_RATECODE_NSS(_rcode)       (((_rcode) >> 4) & 0x3)
#define HTT_GET_HW_RATECODE_RATE(_rcode)      (((_rcode) >> 0) & 0xF)


int
htt_tx_attach(struct htt_pdev_t *pdev, int desc_pool_elems);

void
htt_tx_detach(struct htt_pdev_t *pdev);

int
htt_rx_attach(struct htt_pdev_t *pdev);

void
htt_rx_detach(struct htt_pdev_t *pdev);

int
htt_htc_attach(struct htt_pdev_t *pdev);

void
htt_t2h_msg_handler(void *context, HTC_PACKET *pkt);

void
htt_h2t_send_complete(void *context, HTC_PACKET *pkt);

A_STATUS
htt_h2t_ver_req_msg(struct htt_pdev_t *pdev);

A_STATUS
htt_h2t_frag_desc_bank_cfg_msg(struct htt_pdev_t *pdev);

extern A_STATUS (*htt_h2t_rx_ring_cfg_msg)(
        struct htt_pdev_t *pdev);

enum htc_send_full_action
htt_h2t_full(void *context, HTC_PACKET *pkt);

struct htt_htc_pkt *
htt_htc_pkt_alloc(struct htt_pdev_t *pdev);

void
htt_htc_pkt_free(struct htt_pdev_t *pdev, struct htt_htc_pkt *pkt);

void
htt_t2h_msg_handler_fast(void *htt_pdev, qdf_nbuf_t *nbuf_cmpl_arr,
                uint32_t num_cmpls);

void
htt_htc_pkt_pool_free(struct htt_pdev_t *pdev);

void htt_htc_misc_pkt_list_add(struct htt_pdev_t *pdev, struct htt_htc_pkt *pkt);
void htt_htc_misc_pkt_pool_free(struct htt_pdev_t *pdev);

void htt_pkt_buf_list_add(struct htt_pdev_t *pdev, void *buf, u_int64_t cookie,void *);
void htt_pkt_buf_list_del(struct htt_pdev_t *pdev, u_int64_t cookie);

void stats_deferred_work(void *ctx);

#endif /* _HTT_INTERNAL__H_ */
