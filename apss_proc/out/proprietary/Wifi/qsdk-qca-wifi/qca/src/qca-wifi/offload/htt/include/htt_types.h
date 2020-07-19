/*
 * Copyright (c) 2011-2015, 2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2011-2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef _HTT_TYPES__H_
#define _HTT_TYPES__H_

#include <osdep.h>        /* u_int16_t, dma_addr_t */
#include <qdf_types.h> /* qdf_device_t */
#include <qdf_lock.h>  /* qdf_spinlock_t */
#include <qdf_timer.h> /* qdf_timer_t */
#include <qdf_atomic.h>/* qdf_atomic_inc */
#include <qdf_nbuf.h>     /* qdf_nbuf_t */
#include <htc_api.h>      /* HTC_PACKET */
#include <ol_htt_api.h>
#include <ar_ops.h>

struct ol_txrx_pdev_t;
struct ol_txrx_vdev_t;
struct ol_txrx_peer_t;

#define HTT_TX_MUTEX_TYPE qdf_spinlock_t

#if MESH_MODE_SUPPORT
#define HTT_EXTND_DESC_SIZE 4
#else
#define HTT_EXTND_DESC_SIZE 0
#endif

struct htt_htc_pkt {
    void *pdev_ctxt;
    dma_addr_t nbuf_paddr;
    HTC_PACKET htc_pkt;
    u_int16_t  msdu_id;
};

struct htt_htc_pkt_union {
    union {
        struct htt_htc_pkt pkt;
        struct htt_htc_pkt_union *next;
    } u;
};

/*
 * HTT host descriptor:
 * Include the htt_tx_msdu_desc that gets downloaded to the target,
 * but also include the HTC_FRAME_HDR and alignment padding that
 * precede the htt_tx_msdu_desc.
 * HTCSendDataPkt expects this header space at the front of the
 * initial fragment (i.e. tx descriptor) that is downloaded.
 */
struct htt_host_tx_desc_t {
    u_int8_t htc_header[HTC_HEADER_LEN];
    /* force the tx_desc field to begin on a 4-byte boundary */
    union {
        u_int32_t dummy_force_align;
        struct htt_tx_msdu_desc_t tx_desc;
    } align32;
};
struct htt_tx_mgmt_desc_buf {
    qdf_nbuf_t   msg_buf;
    A_BOOL       is_inuse;
    qdf_nbuf_t   mgmt_frm;
    A_UINT8      desc_id;
    A_UINT8      vdev_id;
    A_UINT8      delayed_free;
    struct htt_tx_mgmt_desc_buf *next;
};

#define HTT_TX_MGMT_DELAYED_FREE_FALSE 0
#define HTT_TX_MGMT_DELAYED_FREE_TRUE 1
#define HTT_TX_MGMT_DELAYED_FREE_PENDING 2
#define HTT_TX_MGMT_DELAYED_FREE_TXCOMPL 3

struct htt_tx_mgmt_desc_ctxt {
    struct htt_tx_mgmt_desc_buf *pool;
    struct htt_tx_mgmt_desc_buf *freelist;
    A_UINT32    pending_cnt;
};

#if PEER_FLOW_CONTROL
/* START : HTT fetch response Descriptor and buffer  */

/* To be removed after htt.h update */
#define HTT_MGMT_FRM_HDR_DOWNLOAD_LEN    32
PREPACK struct htt_fetch_tx_msg_t {
    A_UINT32    msg_type;
    A_UINT32    frag_paddr; /* DMAble address of the data */
    A_UINT32    desc_id;    /* returned to host during completion
                             * to free the meory*/
    A_UINT32    len;    /* length */
} POSTPACK;


struct htt_tx_fetch_desc_buf {
    qdf_nbuf_t   msg_buf;
    A_BOOL       is_inuse;
    A_UINT32      desc_id;
    struct htt_tx_fetch_desc_buf *next;
};

struct htt_tx_fetch_desc_ctxt {
    struct htt_tx_fetch_desc_buf *pool;
    struct htt_tx_fetch_desc_buf *freelist;
    A_UINT32    pending_cnt;
};
#endif
/* END : HTT fetch response Descriptor and buffer  */

struct htt_pdev_buf_entry{
    void* buf;
    u_int64_t cookie;
    void *htt_htc_packet;
    LIST_ENTRY(htt_pdev_buf_entry) buf_list;
};

struct msdu_ext_desc_t {
    u_int32_t tso_flag0;
    u_int32_t tso_flag1;
    u_int32_t tso_flag2;
    u_int32_t tso_flag3;
    u_int32_t frag_ptr0;
    u_int32_t frag_len0;
    u_int32_t frag_ptr1;
    u_int32_t frag_len1;
    u_int32_t frag_ptr2;
    u_int32_t frag_len2;
    u_int32_t frag_ptr3;
    u_int32_t frag_len3;
    u_int32_t frag_ptr4;
    u_int32_t frag_len4;
    u_int32_t frag_ptr5;
    u_int32_t frag_len5;
};

/*
 * For now, the host HTT -> host data rx status enum
 * exactly matches the target HTT -> host HTT rx status enum;
 * no translation is required.
 * However, the host data SW should only use the htt_rx_status,
 * so that in the future a translation from target HTT rx status
 * to host HTT rx status can be added, if the need ever arises.
 */
enum htt_rx_status {
    htt_rx_status_unknown = HTT_RX_IND_MPDU_STATUS_UNKNOWN,
    htt_rx_status_ok = HTT_RX_IND_MPDU_STATUS_OK,
    htt_rx_status_err_fcs = HTT_RX_IND_MPDU_STATUS_ERR_FCS,
    htt_rx_status_err_dup = HTT_RX_IND_MPDU_STATUS_ERR_DUP,
    htt_rx_status_err_replay = HTT_RX_IND_MPDU_STATUS_ERR_REPLAY,
    htt_rx_status_err_inv_peer = HTT_RX_IND_MPDU_STATUS_ERR_INV_PEER,
    htt_rx_status_ctrl_mgmt_null = HTT_RX_IND_MPDU_STATUS_MGMT_CTRL,
    htt_rx_status_tkip_mic_err = HTT_RX_IND_MPDU_STATUS_TKIP_MIC_ERR,
    htt_rx_status_decrypt_err = HTT_RX_IND_MPDU_STATUS_DECRYPT_ERR,
    htt_rx_status_mpdu_length_err = HTT_RX_IND_MPDU_STATUS_MPDU_LENGTH_ERR,

    htt_rx_status_err_misc = HTT_RX_IND_MPDU_STATUS_ERR_MISC
};

union htt_rx_pn_t;

struct htt_pdev_t {
    ol_soc_handle ctrl_psoc;
    ol_txrx_pdev_handle txrx_pdev;
    ol_txrx_soc_handle soc;
    HTC_HANDLE htc_pdev;
    qdf_device_t osdev;
    ar_handle_t *arh;
    struct ar_rx_desc_ops *ar_rx_ops;
    struct hif_opaque_softc *osc;
    u_int32_t htt_hdr_cache[(HTC_HTT_TRANSFER_HDRSIZE + HTT_EXTND_DESC_SIZE + 3)/4];

    int download_len;
    HTC_ENDPOINT_ID htc_endpoint;
#if ATH_11AC_TXCOMPACT
    HTT_TX_MUTEX_TYPE    txnbufq_mutex;
    qdf_nbuf_queue_t     txnbufq;
#endif

    struct htt_htc_pkt_union *htt_htc_pkt_misclist;
    struct htt_htc_pkt_union *htt_htc_pkt_freelist;
    struct {
        int is_high_latency;
    } cfg;
    struct {
        u_int8_t major;
        u_int8_t minor;
    } tgt_ver;
    struct {
        u_int8_t major;
        u_int8_t minor;
    } wifi_ip_ver;
    struct {
        struct {
            /*
             * Ring of network buffer objects -
             * This ring is used exclusively by the host SW.
             * This ring mirrors the dev_addrs_ring that is shared
             * between the host SW and the MAC HW.
             * The host SW uses this netbufs ring to locate the network
             * buffer objects whose data buffers the HW has filled.
             */
            qdf_nbuf_t *netbufs_ring;
            /*
             * Ring of buffer addresses -
             * This ring holds the "physical" device address of the
             * rx buffers the host SW provides for the MAC HW to fill.
             */
            u_int32_t *paddrs_ring;
            qdf_dma_mem_context(memctx);
        } buf;
        /*
         * Base address of ring, as a "physical" device address rather than a
         * CPU address.
         */
        u_int32_t base_paddr;
        int size;           /* how many elems in the ring (power of 2) */
        unsigned size_mask; /* size - 1 */

        int fill_level; /* how many rx buffers to keep in the ring */
        int fill_cnt;   /* how many rx buffers (full+empty) are in the ring */

        /*
         * alloc_idx - where HTT SW has deposited empty buffers
         * This is allocated in consistent mem, so that the FW can read
         * this variable, and program the HW's FW_IDX reg with the value
         * of this shadow register.
         */
        struct {
            u_int32_t *vaddr;
            u_int32_t paddr;
            qdf_dma_mem_context(memctx);
        } alloc_idx;

        /* sw_rd_idx - where HTT SW has processed bufs filled by rx MAC DMA */
        struct {
            unsigned msdu_desc;
            unsigned msdu_payld;
        } sw_rd_idx;

        /*
         * refill_retry_timer - timer triggered when the ring is not
         * refilled to the level expected
         */
        qdf_timer_t refill_retry_timer;

        /*
         * refill_ref_cnt - ref cnt for Rx buffer replenishment - this
         * variable is used to guarantee that only one thread tries
         * to replenish Rx ring.
         */
        qdf_atomic_t refill_ref_cnt;
    } rx_ring;
    int rx_desc_size_hl;
    int rx_fw_desc_offset;
    int rx_mpdu_range_offset_words;
    int rx_ind_msdu_byte_idx;

#if QCA_PARTNER_DIRECTLINK_RX
    /* for keep some direct link information from partner API */
    #define HTT_NBUF_ARRAY_SZ_PARTNER 32
    uint32_t alloc_idx_ptr_partner;
    uint32_t t2h_rxpb_ring_sz_partner;
    uint32_t rxpb_ring_base_partner;
    qdf_nbuf_t t2h_buf_tmp[HTT_NBUF_ARRAY_SZ_PARTNER];
    qdf_atomic_t t2h_buf_ref_cnt[HTT_NBUF_ARRAY_SZ_PARTNER];
#endif /* QCA_PARTNER_DIRECTLINK_RX */

    struct {
        int size; /* of each HTT tx desc */
        int pool_elems;
        int alloc_cnt;
        char *pool_vaddr;
        u_int32_t pool_paddr;
        u_int32_t *freelist;
        qdf_dma_mem_context(memctx);
    } tx_descs;
    struct {
        int size; /* of each Fragment/MSDU-Ext descriptor */
        int pool_elems;
        char *pool_vaddr;
        u_int32_t pool_paddr;
        qdf_dma_mem_context(memctx); /** @todo: not sure if this is required. */
    } frag_descs;

    void (*tx_send_complete_part2)(
            void *pdev, A_STATUS status, qdf_nbuf_t msdu, u_int16_t msdu_id);

    HTT_TX_MUTEX_TYPE htt_tx_mutex;

    struct {
        int htc_err_cnt;
    } stats;

    int cur_seq_num_hl;
    struct htt_tx_mgmt_desc_ctxt tx_mgmt_desc_ctxt;
    qdf_spinlock_t    htt_buf_list_lock;
    ATH_LIST_HEAD(, htt_pdev_buf_entry)  buf_list_head;
    int rs_freq;
#if PEER_FLOW_CONTROL
    struct htt_tx_fetch_desc_ctxt tx_fetch_desc_ctxt;
    struct {
        int size; /* of each Fragment/MSDU-Ext descriptor */
        int peerid_elems;
        int tid_size;
        uint8_t *pool_vaddr;
        uint32_t pool_paddr;
        qdf_dma_mem_context(memctx); /** @todo: not sure if this is required. */
    } host_q_mem;
#endif
#if RX_DEBUG
    uint32_t g_peer_id;
    uint8_t g_peer_mac[6];
    uint32_t g_num_mpdus;
    uint32_t g_mpdu_ranges;
    uint8_t g_msdu_chained;
    uint8_t g_tid;
    enum htt_rx_status g_htt_status;

    int htt_rx_donebit_assertflag;
#endif
/* HTT RX function pointers for HL and LL*/
    int (*htt_rx_amsdu_pop)(
            void *hdl,
#if QCA_OL_SUPPORT_RAWMODE_TXRX
            struct ol_txrx_vdev_t *vdev,
#endif
            qdf_nbuf_t rx_ind_msg,
            qdf_nbuf_t *head_msdu,
            qdf_nbuf_t *tail_msdu,
            u_int32_t *npackets
            );
    void *(*htt_rx_mpdu_desc_list_next)(
            htt_pdev_handle pdev,
            qdf_nbuf_t rx_ind_msg);
    int (*htt_rx_mpdu_desc_seq_num)(
            void *hdl,
            void *mpdu_desc);
    void (*htt_rx_mpdu_desc_pn)(
            htt_pdev_handle pdev,
            void *mpdu_desc,
            union htt_rx_pn_t *pn,
            int pn_len_bits);
    int (*htt_rx_msdu_desc_completes_mpdu)(
            void *hdl, void *msdu_desc);
    int (*htt_rx_msdu_first_msdu_flag)(
            void *hdl, void *msdu_desc);
    uint32_t (*htt_rx_msdu_key_id_octet)(
            void *hdl, void *msdu_desc);
    int (*htt_rx_msdu_has_wlan_mcast_flag)(
            void *hdl, void *msdu_desc);
    int (*htt_rx_msdu_is_wlan_mcast)(
            void *hdl, void *msdu_desc);
    int (*htt_rx_msdu_is_frag)(
            void *hdl, void *msdu_desc);
    void *(*htt_rx_msdu_desc_retrieve)(
            htt_pdev_handle pdev, qdf_nbuf_t msdu);
    int (*htt_rx_mpdu_is_encrypted)(
            void *hdl,
            void *mpdu_desc);
#if RX_DEBUG
    int (*htt_rx_mpdu_status) (htt_pdev_handle pdev);
#endif
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    uint32_t nss_wifiol_id;
    void *nss_wifiol_ctx;
#endif
    void (*htt_process_data_tx_frames)(qdf_nbuf_t msdu);
    void (*htt_process_nondata_tx_frames)(qdf_nbuf_t msdu);
    void (*htt_process_tx_metadata)(void *buf);
};

/*
 * Macro to htc_endpoint from outside HTT
 * e.g. OL in the data-path
 */
#define HTT_EPID_GET(_htt_pdev_hdl)  \
    (((struct htt_pdev_t *)(_htt_pdev_hdl))->htc_endpoint)

#define HTT_DOWNLOADLEN_GET(_htt_pdev_hdl)  \
    (((struct htt_pdev_t *)(_htt_pdev_hdl))->download_len)

#define HTT_WIFI_IP(pdev,x,y) (((pdev)->wifi_ip_ver.major == (x)) && \
                                              ((pdev)->wifi_ip_ver.minor == (y)))

#define HTT_SET_WIFI_IP(pdev,x,y) (((pdev)->wifi_ip_ver.major = (x)) && \
                                              ((pdev)->wifi_ip_ver.minor = (y)))

typedef    int (*htt_rx_amsdu_pop_cb)(
            void *hdl,
#if QCA_OL_SUPPORT_RAWMODE_TXRX
	    struct ol_txrx_vdev_t *vdev,
#endif
            qdf_nbuf_t rx_ind_msg,
            qdf_nbuf_t *head_msdu,
            qdf_nbuf_t *tail_msdu,
            u_int32_t *npackets
            );
typedef    void *(*htt_rx_mpdu_desc_list_next_cb)(
            htt_pdev_handle pdev,
            qdf_nbuf_t rx_ind_msg);
typedef    int (*htt_rx_mpdu_desc_seq_num_cb)(
            void *hdl,
            void *mpdu_desc);
typedef    void (*htt_rx_mpdu_desc_pn_cb)(
            htt_pdev_handle pdev,
            void *mpdu_desc,
            union htt_rx_pn_t *pn,
            int pn_len_bits);
typedef    int (*htt_rx_msdu_desc_completes_mpdu_cb)(
            void *hdl, void *msdu_desc);
typedef    int (*htt_rx_msdu_first_msdu_flag_cb)(
            void *hdl, void *msdu_desc);
typedef    uint32_t (*htt_rx_msdu_key_id_octet_cb)(
            void *hdl, void *msdu_desc);
typedef    int (*htt_rx_msdu_has_wlan_mcast_flag_cb)(
            void *hdl, void *msdu_desc);
typedef    int (*htt_rx_msdu_is_wlan_mcast_cb)(
            void *hdl, void *msdu_desc);
typedef    int (*htt_rx_msdu_is_frag_cb)(
            void *hdl, void *msdu_desc);
typedef    void *(*htt_rx_msdu_desc_retrieve_cb)(
            htt_pdev_handle pdev, qdf_nbuf_t msdu);
typedef    int (*htt_rx_mpdu_is_encrypted_cb)(
            void *hdl,
            void *mpdu_desc);
#if RX_DEBUG
typedef    int (*htt_rx_mpdu_status_cb) (htt_pdev_handle pdev);
#endif


#endif /* _HTT_TYPES__H_ */
