/*
 * Copyright (c) 2011-2105,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_txrx_types.h
 * @brief Define the major data types used internally by the host datapath SW.
 */
#ifndef _OL_TXRX_TYPES__H_
#define _OL_TXRX_TYPES__H_

#include <qdf_nbuf.h>         /* qdf_nbuf_t */
#include <qdf_types.h>     /* qdf_device_t, etc. */
#include <qdf_lock.h>      /* qdf_spinlock */
#include <qdf_atomic.h>    /* qdf_atomic_t */
#include <qdf_util.h>    /* qdf_atomic_t */
#include <qdf_hrtimer.h>    /* qdf_atomic_t */
#include <qdf_time.h>    /* qdf_atomic_t */
#include <queue.h>            /* TAILQ */

#include <htt.h>              /* htt_pkt_type, etc. */
#include <wlan_defs.h>
#include <ol_cfg.h>           /* wlan_frm_fmt */
#include <cdp_txrx_stats_struct.h>
#include <cdp_txrx_cmn_struct.h>
#include <ol_htt_api.h>       /* htt_pdev_handle */
#include <ol_htt_rx_api.h>    /* htt_rx_pn_t */
#include <ol_htt_tx_api.h>    /* htt_msdu_info_t */
#include <ol_txrx_htt_api.h>  /* htt_tx_status */
#include <ol_txrx_osif_api.h> /* ol_txrx_tx_fp */
#include <ol_tx.h>            /* OL_TX_MUTEX_TYPE */
#include <ol_if_athvar.h>

#include <ol_txrx_dbg.h>      /* txrx stats */
#include <ol_txrx_prot_an.h>  /* txrx protocol analysis */
#include <wdi_event_api.h>    /* WDI subscriber event list */

#include <pktlog_ac_api.h>
#include <ol_txrx_stats.h>
#include <ol_txrx_cfg.h>

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#endif
#ifdef QCA_LOWMEM_PLATFORM
#define MAX_ME_BUF_CHUNK 100 /*Maximum number of pool elements to be allocated*/
#define MAX_POOL_BUFF_COUNT 5000 /*Maximun number of me buff allowed in non pool based allocation*/
#else
#define MAX_ME_BUF_CHUNK 1424 /*Maximum number of pool elements to be allocated*/
#define MAX_POOL_BUFF_COUNT 10000 /*Maximun number of me buff allowed in non pool based allocation*/
#endif
/*
 * The target may allocate multiple IDs for a peer.
 * In particular, the target may allocate one ID to represent the
 * multicast key the peer uses, and another ID to represent the
 * unicast key the peer uses.
 */
#define MAX_NUM_PEER_ID_PER_PEER 8

#define MAX_PDEV_COUNT 1

#if PEER_FLOW_CONTROL
/* AST to Peerid FW mapping reduce peerids
 * CFG_TGT_NUM_VDEV_AR900B is 17
 */
#ifdef BIG_ENDIAN_HOST
/*
 * For Big-Endian host roundoff the number of peers to multiples of 4
 * To make sure peermap->byte_cnt[tid][] table is word aligned
 */
#define OL_TXRX_MAX_PEER_IDS  ((((CFG_TGT_NUM_QCACHE_PEERS_MAX + CFG_TGT_NUM_VDEV_AR900B)+3)/4) * 4)
#else
#define OL_TXRX_MAX_PEER_IDS  (CFG_TGT_NUM_QCACHE_PEERS_MAX + CFG_TGT_NUM_VDEV_AR900B)
#endif
#else
#error "OL_TXRX_MAX_PEER_IDS should not be defined with PEER_FLOW_CONTROL disabled"
#define OL_TXRX_MAX_PEER_IDS (CFG_TGT_MAX_AST_TABLE_SIZE+CFG_TGT_AST_SKID_LIMIT)
#endif

/* Max mcast desc blocked in host for Macst2unicast conv in target */
#define MAX_BLOCKED_MCAST_DESC 400
#define MAX_BLOCKED_MCAST_VOW_DESC_PER_NODE 300

/* OL_TXRX_NUM_EXT_TIDS -
 * 16 "real" TIDs + 3 pseudo-TIDs for mgmt, mcast/bcast & non-QoS data
 */
#define OL_TXRX_NUM_EXT_TIDS 19

#define OL_TXRX_MGMT_TYPE_BASE htt_cmn_pkt_num_types
#define OL_TXRX_MGMT_NUM_TYPES 8

#define OL_TX_MUTEX_TYPE qdf_spinlock_t
#define OL_RX_MUTEX_TYPE qdf_spinlock_t

#ifdef QCA_LOWMEM_PLATFORM
#define OL_TX_FLOW_CTRL_QUEUE_LEN               4000
#else
#define OL_TX_FLOW_CTRL_QUEUE_LEN               9000
#endif
#define OL_TX_FLOW_CTRL_QUEUE_WAKEUP_PERCENT    4
#define OL_TX_FLOW_CTRL_QUEUE_WAKEUP_THRESOLD   \
    ((OL_TX_FLOW_CTRL_QUEUE_WAKEUP_PERCENT * OL_TX_FLOW_CTRL_QUEUE_LEN) / 100)

#define OL_TXRX_SUCCESS			0
#define OL_TXRX_FAILURE			-1

#define OL_TX_FLOW_CTRL_ENQ_DONE	  1
#define OL_TX_FLOW_CTRL_DEQ_DONE	  2
#define OL_TX_FLOW_CTRL_Q_FULL		  3
#define OL_TX_FLOW_CTRL_Q_EMPTY		  4
#define OL_TX_FLOW_CTRL_ENQ_DONE_PAUSE_THRESOLD 5

#define OL_TX_PFLOW_CTRL_MAX_TIDS	    8
#define OL_TX_PFLOW_CTRL_HOST_MAX_TIDS	(OL_TX_PFLOW_CTRL_MAX_TIDS + 1)
#define OL_TX_PFLOW_CTRL_PRIORITY_TID   OL_TX_PFLOW_CTRL_MAX_TIDS

#define OL_TX_PFLOW_CTRL_MIN_THRESHOLD          256
#ifdef QCA_LOWMEM_PLATFORM
#define OL_TX_PFLOW_CTRL_MAX_BUF_GLOBAL         (4*1024)
#define OL_TX_PFLOW_CTRL_MAX_QUEUE_LEN          (1024)
#define OL_TX_PFLOW_CTRL_MIN_QUEUE_LEN          64
#elif MIPS_LOW_PERF_SUPPORT
#define OL_TX_PFLOW_CTRL_MAX_BUF_GLOBAL         (8*1024)
#define OL_TX_PFLOW_CTRL_MAX_QUEUE_LEN          (1024)
#define OL_TX_PFLOW_CTRL_MIN_QUEUE_LEN          64
#else
#define OL_TX_PFLOW_CTRL_MAX_BUF_GLOBAL         (32*1024)
#define OL_TX_PFLOW_CTRL_MAX_QUEUE_LEN          (4*1024)
#define OL_TX_PFLOW_CTRL_MIN_QUEUE_LEN          256
#define OL_TX_PFLOW_CTRL_ACTIVE_PEERS_RANGE0_MAX     64             /* Low range value for number of active peers */
#define OL_TX_PFLOW_CTRL_ACTIVE_PEERS_RANGE1_MAX     128            /* Medium range value value for number of active peers */
#define OL_TX_PFLOW_CTRL_ACTIVE_PEERS_RANGE2_MAX     256            /* High range value for number of active peers */
#define OL_TX_PFLOW_CTRL_MAX_BUF_0                   (8*1024)       /* Max buf for Peer range0 */
#define OL_TX_PFLOW_CTRL_MAX_BUF_1                   (16*1024)      /* Max buf for Peer range 1 */
#define OL_TX_PFLOW_CTRL_MAX_BUF_2                   (24*1024)      /* Max buf for Peer range 2 */
#endif
#define OL_TX_PFLOW_CARRIER_VOW_MAX_BUF_GLOBAL (32*1024)
#define OL_TX_PFLOW_CARRIER_VOW_MAX_Q_GLOBAL   (8*1024)
#define OL_TX_PFLOW_CTRL_QDEPTH_FLUSH_INTERVAL  256
#define OL_TX_PFLOW_CTRL_CONG_CTRL_TIMER_MS     1000
#define OL_TX_PFLOW_CTRL_STATS_TIMER_MS         1000
#define OL_TX_PFLOW_CTRL_ROT_TIMER_MS           2
#define OL_TX_PFLOW_CTRL_DEQUEUE_TIMER_MS       2
#define OL_MSDU_DEFAULT_TTL                     10000

#define OL_TX_PFLOW_CTRL_MAX_BUF_GLOBAL_IPQ4019         (8*1024)
#define OL_TX_PFLOW_CTRL_MAX_QUEUE_LEN_IPQ4019          (1024)
#define OL_TX_PFLOW_CTRL_MIN_QUEUE_LEN_IPQ4019          64

#define OL_TX_PKT_CNT_MULTIPLE                  4
#define OL_TX_QUEUE_MAP_RESERVE_PADDING         128 /* Cache Line Size */

#define BA_BMAP_LSB_OFFSET 17
#define BA_BMAP_MSB_OFFSET 18
#define RATE_IDX_OFFSET 0
#define RATE_IDX_MASK 0x000000ff

#define SGI_SERIES_OFFSET 20
#define SGI_SERIES_MASK 0xff000000
#define SGI_SERIES_SHIFT 24
#define SERIES_BW_START_OFFSET 21
#define SERIES_BW_SIZE 4
#define SERIES_BW_MASK 0x30000000
#define SERIES_BW_SHIFT 28


#define RX_D_PREAMBLE_MASK 0xff000000
#define RX_D_PREAMBLE_SHIFT 24
#define RX_D_PREAMBLE_OFFSET 5
#define RX_D_MASK_LSIG_SEL  0x00000010
#define RX_D_MASK_LSIG_RATE 0x0000000f
#define RX_D_HT_RATE_OFFSET 6
#define RX_D_HT_MCS_MASK 0x7f
#define RX_D_VHT_RATE_OFFSET 7
#define RX_D_VHT_MCS_MASK 0xf
#define RX_D_VHT_NSS_MASK 0x7
#define RX_D_VHT_BW_MASK 3
#define RX_D_VHT_SGI_MASK 1

#define SEQ_NUM_OFFSET 2
#define SEQ_NUM_MASK 0xfff

#define OL_GET_PPDU_SEQUENCE_ID(pdev) \
    (pdev->ppdu_seq_id > 255) ? 0: pdev->ppdu_seq_id++

#define OL_TX_PCP_SHIFT 13

struct ol_txrx_psoc_t {
    struct cdp_soc_t cdp_soc;
    /* PSoC object */
    void *psoc_obj;
    struct ol_txrx_cfg_soc_ctxt *ol_txrx_cfg_ctx;
    int is_high_latency;
    struct htt_pdev_t *htt_pdev;
    int soc_nss_wifiol_id;
    void *soc_nss_wifiol_ctx;
    uint32_t pflow_msdu_ttl;
    bool is_ar900b;
    ar_handle_t arh;
    int desc_pool_size;
    int pflow_cong_ctrl_timer_interval;
    ol_txrx_pdev_handle pdev[MAX_PDEV_COUNT];
    void *external_txrx_handle; /* External data path handle */
    void *rate_stats_ctx;
    bool wlanstats_enabled;
};


int ol_soc_pdev_attach(
    struct ol_txrx_psoc_t *dp_soc,
    void *ctrl_pdev_handle,
    HTC_HANDLE htc_pdev,
    qdf_device_t osdev);


enum ol_tx_pflow_ctrl_desc_stat {
    OL_TX_PFLOW_CTRL_DESCS_AVL = 0,
    OL_TX_PFLOW_CTRL_START_ENQ
};

enum ol_tx_pflow_ctrl_enqueue_stat {
    OL_TX_PFLOW_CTRL_ENQ_DONE = 1,
    OL_TX_PFLOW_CTRL_Q_FULL,
    OL_TX_PFLOW_CTRL_Q_EMPTY,
    OL_TX_PFLOW_CTRL_ENQ_FAIL,
    OL_TX_PFLOW_CTRL_DESC_AVAIL,
};

enum ol_tx_pflow_ctrl_dequeue_stat {
    OL_TX_PFLOW_CTRL_DEQ_DONE = 1,
    OL_TX_PFLOW_CTRL_DEQ_REMAINING,
    OL_TX_PFLOW_CTRL_DEQ_DESC_FAIL,
};

enum {
    QUEUE_MAP_COUNT_RESOLUTION_128 = 0,
    QUEUE_MAP_COUNT_RESOLUTION_1K,
    QUEUE_MAP_COUNT_RESOLUTION_8K,
    QUEUE_MAP_COUNT_RESOLUTION_64K,
};

/*
 * The priority order for TID-mapping
 * The values are the same as 3.0
 * to maintain uniformity
 */
enum {
    OL_TX_SVLAN_CVLAN_DSCP = 3,
    OL_TX_CVLAN_SVLAN_DSCP = 4,
};
#if PEER_FLOW_CONTROL_HOST_SCHED
#define TXRX_USED_DESCS(pdev) (pdev->legacy_stats.tx.desc_in_use)
#define TXRX_FREE_DESCS(pdev) (pdev->tx_desc.pool_size - pdev->legacy_stats.tx.desc_in_use)
#define TXRX_PFLOW_CTL_QUEUE_CNT(pdev) (pdev->pdev_data_stats.tx.fl_ctrl.fl_ctrl_enqueue)
#endif

static inline
int ol_txrx_is_dhcp(qdf_nbuf_t nbf, struct iphdr *ip)
{

    struct udphdr *udp ;
    int udpSrc, udpDst;
    uint8_t *datap = (uint8_t *) ip;

    if (ip->protocol == IP_PROTO_UDP)
    {
        udp = (struct udphdr *) (datap + sizeof(struct iphdr));
        udpSrc = qdf_htons(udp->source);
        udpDst = qdf_htons(udp->dest);
        if ((udpSrc == 67) || (udpSrc == 68)) {
            return 1;
        }
    }

    return 0;
}

struct ol_txrx_pdev_t;
struct ol_txrx_vdev_t;
struct ol_txrx_peer_t;
struct ol_tx_fetch_info_t;
#if PEER_FLOW_CONTROL
struct ol_tx_tidq_t;
#endif

/* rx filter related */
#define MAX_PRIVACY_FILTERS           4 /* max privacy filters */

typedef enum _privacy_filter {
    PRIVACY_FILTER_ALWAYS,
    PRIVACY_FILTER_KEY_UNAVAILABLE,
} privacy_filter ;

typedef enum _privacy_filter_packet_type {
    PRIVACY_FILTER_PACKET_UNICAST,
    PRIVACY_FILTER_PACKET_MULTICAST,
    PRIVACY_FILTER_PACKET_BOTH
} privacy_filter_packet_type ;

typedef struct _privacy_excemption_filter {
    u_int16_t                     ether_type; /* type of ethernet to apply this filter, in host byte order*/
    privacy_filter                filter_type;
    privacy_filter_packet_type    packet_type;
} privacy_exemption;

enum ol_tx_frm_type {
    ol_tx_frm_std = 0, /* regular frame - no added header fragments */
    ol_tx_frm_tso,     /* TSO segment, with a modified IP header added */
    ol_tx_frm_sg,      /* SG segment */
    ol_tx_frm_audio,   /* audio frames, with a custom LLC/SNAP header added */
    ol_tx_frm_meucast,   /* audio frames, with a custom LLC/SNAP header added */
};

enum ext_desc_type {
    ext_desc_none = 0,
    ext_desc_chdr,
    ext_desc_fp,
};

struct ol_tx_desc_t {
    qdf_nbuf_t netbuf;
    void *htt_frag_desc;
    void *htt_tx_desc;
    u_int32_t htt_tx_desc_paddr;
    uint16_t            id;
    qdf_atomic_t ref_cnt;
    enum htt_tx_status status;

    /*
     * Allow tx descriptors to be stored in (doubly-linked) lists.
     * This is mainly used for HL tx queuing and scheduling, but is
     * also used by LL+HL for batch processing of tx frames.
     */
    TAILQ_ENTRY(ol_tx_desc_t) tx_desc_list_elem;

    /*
     * Remember whether the tx frame is a regular packet, or whether
     * the driver added extra header fragments (e.g. a modified IP header
     * for TSO fragments, or an added LLC/SNAP header for audio interworking
     * data) that need to be handled in a special manner.
     * This field is filled in with the ol_tx_frm_type enum.
     */
    u_int8_t pkt_type;

    /* To keep track of allocated/freed tx descriptors */
    u_int8_t allocated;

    /*
     * Keep track of the Transmit encap type (i.e. Raw, Native Wi-Fi, or
     * Ethernet). This allows more efficient processing in completion event
     * processing code.
     * This field is filled in with the htt_pkt_type enum.
     */
    u_int16_t tx_encap_type;
#if MESH_MODE_SUPPORT
    u_int16_t extnd_desc;
#endif
};

typedef TAILQ_HEAD(, ol_tx_desc_t) ol_tx_desc_list;

union ol_tx_desc_list_elem_t {
    union ol_tx_desc_list_elem_t *next;
    struct ol_tx_desc_t tx_desc;
};

struct ol_tx_fetch_info_t {
    u_int16_t	peer_id;		/* Peer ID */
    u_int8_t	tid;			/* TID */
	/* @todo does this need to be 16 bit*/
    u_int16_t	requested_frms; /* how many frames to download from the queue */
};

struct htt_t2h_fetch_info_elem_t {
    u_int32_t	seq_num;			/* Reserved for future */
    u_int32_t	num_records;		/* how many elements are present in the following array */
    struct ol_tx_fetch_info_t fetch_info[256];	/* combination of peer ID , TID , and number of frames to download */
};

#if (HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE)
/* Host SW TSO descriptor ->
 * Used for storing header parsing info.
 * headers Parsed once per Jumbo Frame,
 * Used my multiple TCP segments of the same frame
 * Parsed info. stored as part of following struct for sharing across segments
 * */
struct ol_tx_tso_desc_t {
#if HOST_SW_TSO_ENABLE
	qdf_nbuf_t tso_final_segment;
	void  *iphdr; /* points to either IPV4 or IPV6 Hdrs based on ip_hdr_type field */
	void  *tcphdr;
	qdf_atomic_t ref_cnt;
#endif /* HOST_SW_TSO_ENABLE */
#ifdef BIG_ENDIAN_HOST
	A_UINT32 ip_hdr_type       : 2,
		 l2_l3_l4_hdr_size : 14,
		 l3_hdr_size       : 8,
		 l2_length         : 8;

#else
	A_UINT32 l2_length         : 8,
		 l3_hdr_size       : 8,
		 l2_l3_l4_hdr_size : 14,
		 ip_hdr_type       : 2;
#endif
#define TSO_IPV4 1
#define TSO_IPV6 2

#if HOST_SW_TSO_SG_ENABLE
#define MAX_FRAGS	6
        A_UINT16 ipv4_id;
        A_UINT32 tcp_seq;
#if BIG_ENDIAN_HOST
        A_UINT32 reserved               :4,
                 last_segment           :1,
                 segment_count          :3,
                 data_len               :12,
                 mss_size               :12;

        A_UINT8  cwr                    :1,
                 ece                    :1,
                 urg                    :1,
                 ack                    :1,
                 psh                    :1,
                 rst                    :1,
                 syn                    :1,
                 fin                    :1;
#else
        A_UINT32 mss_size               :12,
                 data_len               :12,
                 segment_count          :3,
                 last_segment           :1,
                 reserved               :4;

        A_UINT8  fin                    :1,
                 syn                    :1,
                 rst                    :1,
                 psh                    :1,
                 ack                    :1,
                 urg                    :1,
                 ece                    :1,
                 cwr                    :1;
#endif
        A_UINT32 seg_paddr_lo[MAX_FRAGS];
        A_UINT16 seg_paddr_hi[MAX_FRAGS];

        A_UINT16 seg_len[MAX_FRAGS];
#endif /* HOST_SW_TSO_SG_ENABLE */
};

union ol_tx_tso_desc_list_elem_t {
	union ol_tx_tso_desc_list_elem_t *next;
	struct ol_tx_tso_desc_t tx_tso_desc;
};
#endif /* HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE */

#if HOST_SW_SG_ENABLE
#define MAX_FRAGS	6
/* Host SW SG descriptor ->
 * Used for storing skb fragment info.
 * this info is later used to populate the htt_frag_desc.
 * */
struct ol_tx_sg_desc_t {
    A_UINT32 frag_paddr_lo[MAX_FRAGS];
    A_UINT16 frag_paddr_hi[MAX_FRAGS];
    A_UINT16 frag_len[MAX_FRAGS];

    A_UINT16 frag_cnt;
    A_UINT16 total_len;
};

union ol_tx_sg_desc_list_elem_t {
	union ol_tx_sg_desc_list_elem_t *next;
	struct ol_tx_sg_desc_t tx_sg_desc;
};
#endif /* HOST_SW_SG_ENABLE */


union ol_txrx_align_mac_addr_t {
    u_int8_t raw[QDF_MAC_ADDR_SIZE];
    struct {
        u_int16_t bytes_ab;
        u_int16_t bytes_cd;
        u_int16_t bytes_ef;
    } align2;
    struct {
        u_int32_t bytes_abcd;
        u_int16_t bytes_ef;
    } align4;
};

struct ol_tx_stats {
    u_int32_t num_msdu;
    u_int32_t msdu_bytes;
    u_int32_t peer_id;
    u_int32_t seq_num;
    u_int32_t no_ack;
};

struct targetdef_s;
#if PEER_FLOW_CONTROL
#ifdef BIG_ENDIAN_HOST

#if ((OL_TXRX_MAX_PEER_IDS & 3) != 0)
#error "Please define OL_TXRX_MAX_PEER_IDS as multiples of 4"
#endif

#define NUM_PEER_BYTE_CNTS_IN_WORD 4

struct peermap
{
#define MAX_PEER_MAP_WORD ((OL_TXRX_MAX_PEER_IDS)/32 + 1)

    uint32_t byte_cnt[OL_TX_PFLOW_CTRL_MAX_TIDS][OL_TXRX_MAX_PEER_IDS/NUM_PEER_BYTE_CNTS_IN_WORD];
    uint32_t tid_peermap[OL_TX_PFLOW_CTRL_MAX_TIDS][MAX_PEER_MAP_WORD];
    uint32_t seq;
};
#define QUEUE_MAP_BYTE_CNT_GET(_pdev, _peer_id, _tid) ((_pdev->pmap->byte_cnt[_tid][_peer_id/NUM_PEER_BYTE_CNTS_IN_WORD] >> ((_peer_id & 3)*8)) & 0xFF)
#define QUEUE_MAP_BYTE_CNT_SET(_pdev, _peer_id, _tid, _val) \
{ \
    _pdev->pmap->byte_cnt[_tid][_peer_id/NUM_PEER_BYTE_CNTS_IN_WORD] &= ~(0xFF <<((_peer_id & 3)*8)); \
    _pdev->pmap->byte_cnt[_tid][_peer_id/NUM_PEER_BYTE_CNTS_IN_WORD] |= (_val <<((_peer_id & 3)*8)); \
}

#define QUEUE_MAP_CNT_SET(_pdev, _peer_id, _tid, _val) \
{ \
    QUEUE_MAP_BYTE_CNT_SET(_pdev, _peer_id, _tid, _val); \
    _pdev->pmap->tid_peermap[_tid][_peer_id >> 5] |= (1 << (_peer_id & 0x1F)); \
}
#define QUEUE_MAP_CNT_RESET(_pdev, _peer_id, _tid) \
{ \
    _pdev->pmap->byte_cnt[_tid][_peer_id/NUM_PEER_BYTE_CNTS_IN_WORD] &= ~(0xFF <<((_peer_id & 3)*8)); \
    _pdev->pmap->tid_peermap[_tid][_peer_id >> 5] &= ~(1 << (_peer_id & 0x1F)); \
    ol_tx_pflow_ctrl_flush_queue_map(_pdev); \
    _pdev->pflow_ctl_queue_max_len[_peer_id][_tid] = _pdev->pflow_ctl_max_queue_len; \
}

#define QUEUE_MAP_CNT_GET(_pdev, _peer_id, _tid) ((_pdev->pmap->byte_cnt[_tid][_peer_id/NUM_PEER_BYTE_CNTS_IN_WORD] >> ((_peer_id & 3)*8)) & 0x3F)
#else /*BIG_ENDIAN_HOST*/
#define QUEUE_MAP_CNT_SET(_pdev, _peer_id, _tid, _val) \
{ \
    _pdev->pmap->byte_cnt[_tid][_peer_id] = _val; \
    _pdev->pmap->tid_peermap[_tid][_peer_id >> 5] |= (1 << (_peer_id & 0x1F)); \
}

#define QUEUE_MAP_CNT_RESET(_pdev, _peer_id, _tid) \
{ \
    _pdev->pmap->byte_cnt[_tid][_peer_id] = 0; \
    _pdev->pmap->tid_peermap[_tid][_peer_id >> 5] &= ~(1 << (_peer_id & 0x1F)); \
    ol_tx_pflow_ctrl_flush_queue_map(_pdev); \
    _pdev->pflow_ctl_queue_max_len[_peer_id][_tid] = _pdev->pflow_ctl_max_queue_len; \
}

#define QUEUE_MAP_CNT_GET(_pdev, _peer_id, _tid) (_pdev->pmap->byte_cnt[_tid][_peer_id] & 0x3F)

struct peermap
{
#define MAX_PEER_MAP_WORD ((OL_TXRX_MAX_PEER_IDS)/32 + 1)

    uint8_t  byte_cnt[OL_TX_PFLOW_CTRL_MAX_TIDS][OL_TXRX_MAX_PEER_IDS];
    uint32_t tid_peermap[OL_TX_PFLOW_CTRL_MAX_TIDS][MAX_PEER_MAP_WORD];
    uint32_t seq;
};
#endif /*BIG_ENDIAN_HOST*/

struct pmap_ctxt
{
    qdf_hrtimer_data_t htimer;
    struct htt_pdev_t *httpdev;
};


struct ol_pflow_delay_video_stats {
    u_int64_t value;
};

/*
 * Video Delay Counters
 */
enum pflow_ctrl_video_delay_stats {
    /* Delay Counters */
    HOST_DF_MIN,
    HOST_DF_AVG,
    HOST_DF_MAX,
    HOST_DF_BUCKET_0,
    HOST_DF_BUCKET_10,
    HOST_DF_BUCKET_20,
    HOST_DF_BUCKET_30,
    HOST_DF_BUCKET_40,
    HOST_DF_BUCKET_50,
    HOST_DF_BUCKET_60,
    HOST_DF_BUCKET_70,
    HOST_DF_BUCKET_80,
    HOST_DF_BUCKET_90,
    HOST_DF_BUCKET_MAX,
    HOST_FW_DF_MIN,
    HOST_FW_DF_AVG,
    HOST_FW_DF_MAX,
    HOST_FW_DF_BUCKET_0,
    HOST_FW_DF_BUCKET_10,
    HOST_FW_DF_BUCKET_20,
    HOST_FW_DF_BUCKET_30,
    HOST_FW_DF_BUCKET_40,
    HOST_FW_DF_BUCKET_50,
    HOST_FW_DF_BUCKET_60,
    HOST_FW_DF_BUCKET_70,
    HOST_FW_DF_BUCKET_80,
    HOST_FW_DF_BUCKET_90,
    HOST_FW_DF_BUCKET_MAX,
    HOST_INTERFRAME_BUCKET_200,
    HOST_INTERFRAME_BUCKET_400,
    HOST_INTERFRAME_BUCKET_600,
    HOST_INTERFRAME_BUCKET_800,
    HOST_INTERFRAME_BUCKET_10,
    HOST_INTERFRAME_BUCKET_20,
    HOST_INTERFRAME_BUCKET_30,
    HOST_INTERFRAME_BUCKET_40,
    HOST_INTERFRAME_BUCKET_50,
    HOST_INTERFRAME_BUCKET_MAX,
    HOST_INTER_RADIO_BUCKET_200,
    HOST_INTER_RADIO_BUCKET_400,
    HOST_INTER_RADIO_BUCKET_600,
    HOST_INTER_RADIO_BUCKET_800,
    HOST_INTER_RADIO_BUCKET_MAX,
    HOST_DATA_DF_MAX_ELEMS,
};

enum pflow_ctrl_global_video_stats {
    /* Tx Video Counters */
    TX_VIDEO_TID_MSDU_TOTAL_LINUX_SUBSYSTEM,
    TX_VIDEO_TID_MSDU_TOTAL_FROM_OSIF,
    TX_VIDEO_TID_MSDU_TX_COMP_PKT_CNT,
    /* Rx Video Counters */
    RX_VIDEO_TID_MSDU_TOTAL_FROM_FW,
    RX_VIDEO_TID_MSDU_MCAST_FROM_FW,
    RX_VIDEO_TID_MISMATCH_FROM_FW,
    RX_VIDEO_MSDU_MISC_PKTS,
    RX_VIDEO_AGGREGATE_10,
    RX_VIDEO_AGGREGATE_20,
    RX_VIDEO_AGGREGATE_30,
    RX_VIDEO_AGGREGATE_40,
    RX_VIDEO_AGGREGATE_50,
    RX_VIDEO_AGGREGATE_60,
    RX_VIDEO_AGGREGATE_MORE,
    RX_VIDEO_AMSDU_1,
    RX_VIDEO_AMSDU_2,
    RX_VIDEO_AMSDU_3,
    RX_VIDEO_AMSDU_4,
    RX_VIDEO_AMSDU_MORE,
    RX_VIDEO_TID_MSDU_CHAINED_FROM_FW,
    RX_VIDEO_TID_MSDU_REORDER_FAILED_FROM_FW,
    RX_VIDEO_TID_MSDU_REORDER_FLUSHED_FROM_FW,
    RX_VIDEO_TID_MSDU_DISCARD_FROM_FW,
    RX_VIDEO_TID_MSDU_DUPLICATE_FROM_FW,
    RX_VIDEO_TID_MSDU_DELIVERED_TO_STACK,
    OL_TXRX_VIDEO_MAX_STATS,
};

enum pflow_ctrl_tidq_stats {
    TIDQ_ENQUEUE_CNT,
    TIDQ_DEQUEUE_CNT,
    TIDQ_DEQUEUE_BYTECNT,
    TIDQ_ENQUEUE_BYTECNT,
    TIDQ_DEQUEUE_REQ_BYTECNT,
    TIDQ_DEQUEUE_REQ_PKTCNT,
    TIDQ_PEER_Q_FULL,
    TIDQ_MSDU_TTL_EXPIRY,
    TIDQ_HOL_CNT,
    PEER_TX_DESC_FAIL,
    PFLOW_CTRL_TIDQ_STATS_MAX,
};

enum pflow_low_latency_mode {
    LOW_LATENCY_MODE_DISABLED = 0,
    LOW_LATENCY_MODE_ENABLED = 1,
};

/*
 * This structure holds all VOW related members
 * Add any member only in this structure
 */
struct carrier_vow {
    struct ol_pflow_stats vistats[OL_TXRX_VIDEO_MAX_STATS];
    struct ol_pflow_delay_video_stats delaystats[HOST_DATA_DF_MAX_ELEMS];

    u_int64_t pflow_ctl_host_min;
    u_int64_t pflow_ctl_host_max;
    u_int64_t pflow_ctl_host_avg;
    u_int32_t pflow_ctl_host_sum;
    u_int32_t pflow_ctl_host_cnt;
    u_int64_t pflow_ctl_host_fw_min;
    u_int64_t pflow_ctl_host_fw_max;
    u_int64_t pflow_ctl_host_fw_avg;
    u_int32_t pflow_ctl_host_fw_sum;
    u_int32_t pflow_ctl_host_fw_cnt;
};
#define PFLOW_CTRL_PDEV_STATS_SET(_pdev,_var,_val) _pdev->pdev_data_stats.tx.pstats[_var].value = _val
#define PFLOW_CTRL_PDEV_STATS_GET(_pdev,_var) _pdev->pdev_data_stats.tx.pstats[_var].value
#define PFLOW_CTRL_PDEV_STATS_ADD(_pdev,_var,_val) _pdev->pdev_data_stats.tx.pstats[_var].value += _val
#define PFLOW_CTRL_PEER_STATS_ADD(_peer,_tid,_var,_val) _peer->tidq[_tid].stats[_var] += _val

#endif

struct ol_txrx_fw_stats_info {
    struct ol_ath_softc_net80211 *scn;
    u_int32_t vdev_id;
    TAILQ_ENTRY(ol_txrx_fw_stats_info) stats_info_list_elem;
    u_int8_t stats_info[0];
};

struct ol_txrx_pdev_t {
    /* ctrl_pdev - handle for querying config info */
    ol_pdev_handle ctrl_pdev;

    /* osdev - handle for mem alloc / free, map / unmap */
    qdf_device_t osdev;

    htt_pdev_handle htt_pdev;
    osdev_t    p_osdev;
    void *scnctx;
    uint8_t igmpmld_override;
    uint8_t enable_perpkt_txstats;
    uint8_t pdev_id;
    uint8_t arp_dbg_conf;
    uint8_t igmpmld_tid;
    uint8_t res[3];

    struct targetdef_s *targetdef;


#ifndef REMOVE_PKT_LOG
    struct {
       u_int8_t    ia_category;
       u_int8_t    ia_action;
       u_int8_t    member_status[24];
       u_int8_t    user_position[16];
    } gid_mgmt;
    u_int8_t       gid_flag;
#endif

     struct cdp_soc_t *soc;
#if QCA_OL_TX_PDEV_LOCK
    qdf_spinlock_t tx_lock;
#endif
    qdf_nbuf_queue_t acnbufq;
    u_int32_t acqcnt;
    u_int32_t acqcnt_len;
    qdf_spinlock_t acnbufqlock;

#if PEER_FLOW_CONTROL
    u_int32_t pflow_ctl_min_threshold;
    u_int32_t pflow_ctl_desc_count;
    u_int32_t pflow_cong_ctrl_timer_interval;
    u_int32_t pflow_ctl_stats_timer_interval;
    u_int32_t pflow_ctl_global_queue_cnt;
    u_int32_t pflow_ctl_min_queue_len;
    u_int32_t pflow_ctl_max_queue_len;
    u_int32_t pflow_ctl_max_buf_global;
    u_int32_t pflow_ctl_queue_max_len[OL_TXRX_MAX_PEER_IDS][OL_TX_PFLOW_CTRL_MAX_TIDS];
    u_int16_t pflow_ctl_total_dequeue_cnt;
    u_int16_t pflow_ctl_total_dequeue_byte_cnt;

    /*
     * List of active peers is maintained as a bitmap
     * No.of 32-bit words required per TID = total_num_peers/32
     */
    u_int32_t pflow_ctl_active_peer_map[OL_TX_PFLOW_CTRL_MAX_TIDS][(OL_TXRX_MAX_PEER_IDS >> 5)];

    /* Congestion control timer */
    qdf_timer_t pflow_ctl_cong_timer;

    /* Stats Display timer */
    /* @todo Change to Workqueue */
    qdf_timer_t pflow_ctl_stats_timer;

    u_int32_t pflow_ctl_stats_queue_depth;

#if !QCA_OL_TX_PDEV_LOCK
    qdf_spinlock_t peer_lock[OL_TXRX_MAX_PEER_IDS];
#endif

    u_int32_t pmap_qdepth_flush_interval;
    u_int32_t pmap_qdepth_flush_count;
    u_int32_t pmap_rotting_timer_interval;
    u_int32_t pflow_ctrl_stats_enable;
    uint8_t pflow_hostq_dump;
    uint8_t pflow_tidq_map;
    uint8_t pflow_ctrl_low_latency_mode;
    uint8_t pflow_ctrl_mode;

    /* Latency measurement variables */
    uint8_t prof_trigger;
    ktime_t delta_intr_tasklet_usecs;
    uint32_t pflow_msdu_ttl;
    uint16_t pflow_msdu_ttl_cnt;
    uint16_t pflow_ttl_cntr;
    bool delay_counters_enabled;
#endif

#ifdef WLAN_FEATURE_FASTPATH
    struct CE_handle    *ce_tx_hdl; /* Handle to Tx packet posting CE */
    struct CE_handle    *ce_htt_msg_hdl; /* Handle to TxRx completion CE */
#endif /* WLAN_FEATURE_FASTPATH */

    struct {
        int is_high_latency;
    } cfg;

    /* WDI subscriber's event list */
    wdi_event_subscribe **wdi_event_list;

    /* Pktlog pdev */
    struct ol_pktlog_dev_t *pl_dev;

    /*
     * target tx credit -
     * not needed for LL, but used for HL download scheduler to keep
     * track of roughly how much space is available in the target for
     * tx frames
     */
    qdf_atomic_t target_tx_credit;

    /* ol_txrx_vdev list */
    TAILQ_HEAD(, ol_txrx_vdev_t) vdev_list;

    /* peer ID to peer object map (array of pointers to peer objects) */
    struct ol_txrx_peer_t **peer_id_to_obj_map;

    /* Vdev ID to vdevobject map (array of pointers to vdev objects) */
    struct ol_txrx_vdev_t **vdev_id_to_obj_map;

    struct {
        unsigned mask;
        unsigned idx_bits;
        TAILQ_HEAD(, ol_txrx_peer_t) *bins;
    } peer_hash;

    struct {
        TAILQ_HEAD(, ol_txrx_ast_entry_t) *bins;
    } ast_entry_hash;

    /* rx specific processing */
    struct {
        struct {
            TAILQ_HEAD(, ol_rx_reorder_t) waitlist;
            u_int32_t timeout_ms;
        } defrag;
        struct {
            int defrag_timeout_check;
            int dup_check;
        } flags;
    } rx;

    /* rx proc function */
    void (*rx_opt_proc)(
        struct ol_txrx_vdev_t *vdev,
        struct ol_txrx_peer_t *peer,
        unsigned tid,
        qdf_nbuf_t msdu_list);

    /* tx management delivery notification callback functions */
    struct {
        struct {
            ol_txrx_mgmt_tx_cb download_cb;
            ol_txrx_mgmt_tx_cb ota_ack_cb;
            void *ctxt;
        } callbacks[OL_TXRX_MGMT_NUM_TYPES];
    } tx_mgmt;

    /* tx descriptor pool */
    struct {
        int pool_size;
        union ol_tx_desc_list_elem_t *array;
        union ol_tx_desc_list_elem_t *freelist;
    } tx_desc;

#if (HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE)
    /* tx tso descriptor pool */
    struct {
	    int pool_size;
	    union ol_tx_tso_desc_list_elem_t *array;
	    union ol_tx_tso_desc_list_elem_t *freelist;
    } tx_tso_desc;
    /* tx mutex */
    OL_TX_MUTEX_TYPE tx_tso_mutex;
#endif /* HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE */

#if HOST_SW_SG_ENABLE
    /* tx sg descriptor pool */
    struct {
	    int pool_size;
	    union ol_tx_sg_desc_list_elem_t *array;
	    union ol_tx_sg_desc_list_elem_t *freelist;
    } tx_sg_desc;
    /* tx mutex */
    OL_TX_MUTEX_TYPE tx_sg_mutex;
#endif /* HOST_SW_SG_ENABLE */

    /* pool addr for mcast enhance buff */
    struct {
        int size;
        uint32_t paddr;
        char* vaddr;
        struct ol_tx_me_buf_t* freelist;
        int buf_in_use;
        int nonpool_buf_in_use;
        qdf_dma_mem_context(memctx);
    } me_buf;

    struct {
        int (*cmp)(
            union htt_rx_pn_t *new,
            union htt_rx_pn_t *old,
            int is_unicast,
            int opmode);
        int len;
    } rx_pn[htt_num_sec_types];

    /* Monitor mode interface and status storage */
    struct ol_txrx_vdev_t *monitor_vdev;
    struct ieee80211_rx_status *rx_mon_recv_status;

    /* tx mutex */
    OL_TX_MUTEX_TYPE tx_mutex;

    /*
     * peer ref mutex:
     * 1. Protect peer object lookups until the returned peer object's
     *    reference count is incremented.
     * 2. Provide mutex when accessing peer object lookup structures.
     */
    OL_RX_MUTEX_TYPE peer_ref_mutex;

    /* monitor mode mutex */
    qdf_spinlock_t mon_mutex;

    /*
     * Even if the txrx protocol analyzer debug feature is disabled,
     * go ahead and allocate the handles (pointers), since it's only
     * a few bytes of mem.
     */
    //ol_txrx_prot_an_handle tx_host_reject;
    ol_txrx_prot_an_handle prot_an_tx_sent; /* sent to HTT */
    //ol_txrx_prot_an_handle tx_completed;
    ol_txrx_prot_an_handle prot_an_rx_sent; /* sent to OS shim */
    //ol_txrx_prot_an_handle rx_forwarded;

    #if defined(ENABLE_RX_REORDER_TRACE)
    struct {
        u_int32_t mask;
        u_int32_t idx;
        u_int64_t cnt;
        #define TXRX_RX_REORDER_TRACE_SIZE_LOG2 8 /* 256 entries */
        struct {
            u_int16_t seq_num;
            u_int8_t  num_mpdus;
            u_int8_t  tid;
        } *data;
    } rx_reorder_trace;
    #endif /* ENABLE_RX_REORDER_TRACE */

    #if defined(ENABLE_RX_PN_TRACE)
    struct {
        u_int32_t mask;
        u_int32_t idx;
        u_int64_t cnt;
        #define TXRX_RX_PN_TRACE_SIZE_LOG2 5 /* 32 entries */
        struct {
            struct ol_txrx_peer_t *peer;
            u_int32_t pn32;
            u_int16_t seq_num;
            u_int8_t  unicast;
            u_int8_t  tid;
        } *data;
    } rx_pn_trace;
    #endif /* ENABLE_RX_PN_TRACE */

    bool        filter_neighbour_peers; /*filter in neighbour peer for nac peer capture */
    bool        host_80211_enable;
    bool        nss_nwifi_offload;
    bool        is_ar900b;

    /* Rate-control context */
    struct {
        u_int32_t is_ratectrl_on_host;
        u_int32_t dyn_bw;
    } ratectrl;

	struct ol_tx_stats tx_stats;
    /* Global RX decap mode for the device */
    enum htt_pkt_type  rx_decap_mode;

    /* Whether Enhanced Stats is enabled. This is a copy of a similar variable
     * from ol_ath_softc_net80211, maintained here for better cache performance
     * where ol_ath_softc_net80211 isn't accessed in the neighborhood.
     */
    bool ap_stats_tx_cal_enable;

    /* Number of vdevs this device have */
    u_int16_t vdev_count;
    /* Number of VAP attached*/
    atomic_t mc_num_vap_attached;
#if PEER_FLOW_CONTROL
    /* Peer map host q view, peerid, tid and sequence no update */
    qdf_spinlock_t peermap_lock;
    struct peermap  *pmap;
    struct pmap_ctxt qmap_ctxt;
    u_int32_t num_active_peers; /* total no of currently active peers */
    struct carrier_vow vow;
#endif
    TAILQ_HEAD(, ol_txrx_fw_stats_info) stats_buffer_list;
    qdf_work_t stats_wq;
    qdf_spinlock_t stats_buffer_lock;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    int nss_wifiol_id ;
    int nss_ifnum;
    void *nss_wifiol_ctx;
    int last_peer_pool_used;
    u_int8_t *peer_mem_pool[OSIF_NSS_OL_MAX_PEER_POOLS];
    u_int32_t max_peers_per_pool;
    int last_rra_pool_used;
    u_int8_t *rra_mem_pool[OSIF_NSS_OL_MAX_RRA_POOLS];
    u_int32_t max_rra_per_pool;
    qdf_nbuf_t mon_mpdu_skb_list_head;
    qdf_nbuf_t mon_mpdu_skb_list_tail;
#endif
    int max_peers; /* maximum value for peer_id */
    u_int8_t tid_override_queue_mapping;
    u_int8_t fw_supported_enh_stats_version;
    u_int8_t mon_filter_ucast_data:1,
             mon_filter_mcast_data:1,
             mon_filter_non_data:1;
    u_int8_t tx_capture;    /* is tx capture enabled*/
    u_int8_t tx_cap_hdr;    /* is tx capture hdr present*/
    void *dp_txrx_handle; /* Advanced data path handle */
    struct cdp_pdev_stats pdev_stats;
    struct ol_txrx_data_stats  pdev_data_stats;
    ol_dbg_stats dbg_stats;
    bool tx_sniffer_enable; /* Tx capture mode */
    bool mcopy_mode; /* mirror copy mode */
    struct  cdp_tx_sojourn_stats  sojourn_stats; /* added for plume */
    struct cdp_tx_completion_ppdu_user ppdu_tx_stats;
    bool carrier_vow_config;
    void *cal_client_ctx;
    uint32_t pcp_tid_map[IEEE80211_PCP_MAX]; /* default pcp-tid mapping */
    uint8_t tidmap_prty; /* default tidmap priority */
    uint64_t ppdu_seq_id;
    uint64_t rx_ppdu_seq_id;
    struct cdp_rx_indication_ppdu cdp_rx_ppdu;
    qdf_nbuf_t sojourn_nbuf;
    uint32_t next_peer_cookie; /* unique cookie required for peer session */
};

/**
 * Interframe packet structure
 * @start_tick: This gets updated with current tick
 *              value when first frame is received for a particular group
 * @max_interframe_delay:This stores the maximum relative value of
 *                       interframe delay
 * @max_delay: The ticks value in absolute terms when the
 *             max_interframe delay is observed
 * @last_frame_tick: The ticks value in absolute terms when the
 *                   max_interframe delay is observed
 */
struct ol_txrx_interframe{
    qdf_ktime_t start_tick;
    qdf_ktime_t max_interframe_delay;
    qdf_ktime_t max_delay;
    qdf_ktime_t last_frame_tick;
};

struct ol_txrx_vdev_t {
    /* pdev - the physical device that is the parent of this virtual device */
    struct ol_txrx_pdev_t *pdev;

    /*
     * osif_vdev -
     * handle to the OS shim SW's virtual device
     * that matched this txrx virtual device
     */
    void *osif_vdev;
    /* Handle to the UMAC handle */
    struct cdp_ctrl_objmgr_vdev *ctrl_vdev;
    int hdrcache[(HTC_HTT_TRANSFER_HDRSIZE + HTT_EXTND_DESC_SIZE + 3)/4];
    qdf_device_t osdev;
    int epid;
    int downloadlen;

    /* vdev_id - ID used to specify a particular vdev to the target */
    u_int8_t vdev_id;

    /* MAC address */
    union ol_txrx_align_mac_addr_t mac_addr;

    /* tx paused - NO LONGER NEEDED? */

    /* node in the pdev's list of vdevs */
    TAILQ_ENTRY(ol_txrx_vdev_t) vdev_list_elem;

    /* ol_txrx_peer list */
    TAILQ_HEAD(, ol_txrx_peer_t) peer_list;

    /* transmit function used by this vdev */
    ol_txrx_tx_fp tx;

    /* receive function used by this vdev to hand rx frames to the OS shim */
    ol_txrx_rx_fp osif_rx;
    ol_txrx_rsim_rx_decap_fp osif_rsim_rx_decap;

#if ATH_SUPPORT_WAPI
    /* receive function used by this vdev to check if the msdu is an WAI (WAPI) frame */
    ol_txrx_rx_check_wai_fp osif_check_wai;
#endif

#if UMAC_SUPPORT_PROXY_ARP
    /* proxy arp function */
    ol_txrx_proxy_arp_fp osif_proxy_arp;
#endif
    /*
     * receive function used by this vdev to hand rx monitor promiscuous
     * 802.11 MPDU to the OS shim
     */
    ol_txrx_rx_mon_fp osif_rx_mon;

    struct {
        /*
         * If the vdev object couldn't be deleted immediately because it still
         * had some peer objects left, remember that a delete was requested,
         * so it can be deleted once all its peers have been deleted.
         */
        int pending;
        /*
         * Store a function pointer and a context argument to provide a
         * notification for when the vdev is deleted.
         */
        ol_txrx_vdev_delete_cb callback;
        void *context;
    } delete;

    /* safe mode control to bypass the encrypt and decipher process*/
    u_int32_t safemode;

    /* rx filter related */
    u_int32_t drop_unenc;
    privacy_exemption privacy_filters[MAX_PRIVACY_FILTERS];
    u_int32_t filters_num;

    enum wlan_op_mode opmode;
    uint32_t sta_peer_id;           /* only for sta vap to store BSS peer-id */

    /* Rate-control context */
    /* TODO: hbrc - Move pRatectrl and vdev_rc_info in one context struct */
    void *pRateCtrl;
    struct {
        u_int8_t  data_rc;          /* HW rate code for data frame */
        u_int8_t  max_bw;           /* maxmium allowed bandwidth */
        u_int8_t  max_nss;          /* Max NSS to use */
        u_int8_t  rts_cts;          /* RTS/CTS enabled */
        u_int8_t  def_tpc;          /* default TPC value */
        u_int8_t  def_tries;        /* Total Transmission attempt */
        u_int8_t  non_data_rc;      /* HW rate for mgmt/control frames */
        u_int32_t rc_flags;         /* RC flags SGI/LDPC/STBC etc */
        u_int32_t aggr_dur_limit;   /* max duraton any aggregate may have */
        u_int8_t  tx_chain_mask[IEEE80211_MAX_TX_CHAINS];
    } vdev_rc_info;

    /* Tx encapsulation type for this VAP. HTT packet type will be filled using this */
    enum htt_pkt_type tx_encap_type;
    /* Rx Decapsulation type for this VAP */
    enum htt_pkt_type rx_decap_type;

    u_int64_t   tx_overflow_start;  /* The time when TX overflow start */
    struct ol_txrx_peer_t *vap_bss_peer;
    bool nawds_enabled;
    u_int32_t htc_htt_hdr_size;
#if MESH_MODE_SUPPORT
    u_int32_t mesh_vdev;
#endif
    u_int8_t txpow_mgt_frm[MAX_NUM_TXPOW_MGT_ENTRY];
    bool carrier_vow_config;
    /* Interframe delay stats */
    struct ol_txrx_interframe interframe_delay_stats;
    /* Disconnected peer stats will be updated below */
    struct cdp_vdev_stats stats;
    /* extended statst received by WMI*/
    struct ol_txrx_host_vdev_stats host_vdev_stats;
    struct ol_txrx_host_vdev_extd_stats vdev_extd_stats;
    uint32_t pcp_tid_map[IEEE80211_PCP_MAX];
    uint8_t tidmap_tbl_id;
    uint8_t tidmap_prty;
};

struct ol_rx_reorder_array_elem_t {
    qdf_nbuf_t head;
    qdf_nbuf_t tail;
};

struct ol_rx_reorder_t {
    unsigned win_sz_mask;
    struct ol_rx_reorder_array_elem_t *array;
    /* base - single rx reorder element used for non-aggr cases */
    struct ol_rx_reorder_array_elem_t base;

    /* only used for defrag right now */
    TAILQ_ENTRY(ol_rx_reorder_t) defrag_waitlist_elem;
    u_int32_t defrag_timeout_ms;
    /* get back to parent ol_txrx_peer_t when ol_rx_reorder_t is in a
     * waitlist */
    u_int16_t tid;
};

enum {
    txrx_sec_mcast = 0,
    txrx_sec_ucast
};

   /*The below structure is used for tx stats, we cache the number of
    * the packets and number of btyes transmitted in this structure
    * we even cache the bytes inorder to calculate the throughput
    */
struct ol_tx_peer_stats {
    u_int64_t data_bytes;
    u_int64_t data_packets;
    u_int64_t discard_cnt;
    u_int32_t thrup_bytes;
};

struct ol_txrx_ast_entry_t {
    struct ol_txrx_peer_t *base_peer;
    u_int16_t peer_id;
    union ol_txrx_align_mac_addr_t dest_mac_addr;
    TAILQ_ENTRY(ol_txrx_ast_entry_t) hash_list_elem;
};

#if PEER_FLOW_CONTROL
struct ol_tx_tidq_t {
    qdf_nbuf_queue_t nbufq;
    u_int16_t dequeue_cnt;
    u_int32_t byte_count;
    u_int32_t avg_size_compute_cnt;
    u_int32_t msdu_avg_size;
    u_int16_t enq_cnt;
    u_int32_t stats[PFLOW_CTRL_TIDQ_STATS_MAX];
    u_int32_t high_watermark;   /*the largest queue length a queue has seen*/
};
#endif

struct ol_peer_tstamp_stats {
    uint64_t sum[OL_TXRX_NUM_EXT_TIDS];
    uint64_t nummsdu[OL_TXRX_NUM_EXT_TIDS];
    uint64_t avg[OL_TXRX_NUM_EXT_TIDS];
};

struct ol_txrx_peer_t {
    struct ol_txrx_vdev_t *vdev;
    struct cdp_ctrl_objmgr_peer *ctrl_peer;
    qdf_atomic_t ref_cnt;

    /* peer ID(s) for this peer */
    u_int16_t peer_ids[MAX_NUM_PEER_ID_PER_PEER];

    union ol_txrx_align_mac_addr_t mac_addr;

    /* node in the vdev's list of peers */
    TAILQ_ENTRY(ol_txrx_peer_t) peer_list_elem;
    /* node in the hash table bin's list of peers */
    TAILQ_ENTRY(ol_txrx_peer_t) hash_list_elem;

    /*
     * per TID info -
     * stored in separate arrays to avoid alignment padding mem overhead
     */
    struct ol_rx_reorder_t tids_rx_reorder[OL_TXRX_NUM_EXT_TIDS];
    union htt_rx_pn_t      tids_last_pn[OL_TXRX_NUM_EXT_TIDS];
    u_int8_t               tids_last_pn_valid[OL_TXRX_NUM_EXT_TIDS];
    u_int16_t              tids_last_seq[OL_TXRX_NUM_EXT_TIDS];
    union htt_rx_pn_t      global_pn;
    union htt_rx_pn_t      tids_suspect_pn[OL_TXRX_NUM_EXT_TIDS];
    u_int16_t              pn_replay_counter;
#if RX_DEBUG
    u_int8_t               tids_PN_drop_BA[OL_TXRX_NUM_EXT_TIDS];
    u_int8_t               tids_PN_drop_noBA[OL_TXRX_NUM_EXT_TIDS];
#endif

    struct {
        enum htt_sec_type sec_type;
        u_int32_t michael_key[2]; /* relevant for TKIP */
    } security[2]; /* 0 -> multicast, 1 -> unicast */

    /*
     * rx proc function: this either is a copy of pdev's rx_opt_proc for
     * regular rx processing, or has been redirected to a /dev/null discard
     * function when peer deletion is in progress.
     */
    void (*rx_opt_proc)(
        struct ol_txrx_vdev_t *vdev,
        struct ol_txrx_peer_t *peer,
        unsigned tid,
        qdf_nbuf_t msdu_list);

    /* Rate-control context */
    void *rc_node;
    struct ol_tx_peer_stats peer_data_stats;

    u_int8_t authorize:1; /* set when node is authorized */
    /* NAWDS Flag and Bss Peer bit */
    u_int8_t nawds_enabled:1,
                  bss_peer:1
#if WDS_VENDOR_EXTENSION
                            ,
               wds_enabled:1,
        wds_tx_mcast_4addr:1,
        wds_tx_ucast_4addr:1,
             wds_rx_filter:1, /* enforce rx filter */
        wds_rx_ucast_4addr:1, /* when set, accept 4addr unicast frames    */
        wds_rx_mcast_4addr:1  /* when set, accept 4addr multicast frames  */
#endif
        ;

    u_int8_t mcast_pn_reset_keyidx;

#if PEER_FLOW_CONTROL
    struct ol_tx_tidq_t tidq[OL_TX_PFLOW_CTRL_HOST_MAX_TIDS];
    struct cdp_tidq_stats tidq_stats[OL_TX_PFLOW_CTRL_HOST_MAX_TIDS];
#endif

#if ATH_SUPPORT_NAC
    u_int8_t nac; /* non associated client*/
#endif
    struct ol_peer_tstamp_stats tstampstat;
    struct cdp_peer_stats stats; /* cdp peer stats for updating stats */
    struct ol_txrx_host_peer_extd_stats extd_stats; /* extended wmi peer stats */
    void *wlanstats_ctx; /* rate stats context */
    qdf_ewma_tx_lag avg_sojourn_msdu[CDP_DATA_TID_MAX]; /* average sojourn time */
};

struct ol_tx_me_buf_t {
    struct ol_tx_me_buf_t *next;
    u_int8_t nonpool_based_alloc;
#if AH_NEED_TX_DATA_SWAP
    /* To sure the mcast buff is word aligned */
    u_int8_t reserved[3];
#endif
    /*buf[1] is to represent data of variable length*/
    u_int8_t buf[1];

};
#if PEER_FLOW_CONTROL
extern uint16_t ol_tx_classify_get_peer_id(struct ol_txrx_vdev_t *vdev,
        qdf_nbuf_t nbuf);
#endif
#endif /* _OL_TXRX_TYPES__H_ */
