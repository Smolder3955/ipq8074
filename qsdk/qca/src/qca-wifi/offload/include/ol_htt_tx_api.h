/*
 * Copyright (c) 2011-2014,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file ol_htt_tx_api.h
 * @brief Specify the tx HTT API functions called by the host data SW.
 * @details
 *  This file declares the HTT API functions that are specifically
 *  related to transmit processing.
 *  In particular, the methods of the abstract HTT tx descriptor are
 *  specified.
 */
#ifndef _OL_HTT_TX_API__H_
#define _OL_HTT_TX_API__H_

//#include <a_osapi.h>    /* u_int16_t, etc. */
#include <osdep.h>      /* u_int16_t, etc. */
#include <qdf_nbuf.h>   /* qdf_nbuf_t */
#include <ol_cfg.h>     /* wlan_frm_fmt */

#include <htt.h>        /* needed by inline functions */
#include <qdf_net_types.h>
#include <ol_htt_api.h> /* htt_pdev_handle */
#include <htt_types.h>

/*================ tx completion message field access methods ===============*/


/**
 * @brief Look up the descriptor ID of the nth MSDU from a tx completion msg.
 * @details
 *  A tx completion message tells the host that the target is done
 *  transmitting a series of MSDUs.  The message uses a descriptor ID
 *  to identify each such MSDU.  This function/macro is used to
 *  find the ID of one such MSDU referenced by the tx completion message.
 *
 * @param iterator - tx completion message context provided by HTT to the
 *      tx completion message handler.  This abstract reference to the
 *      HTT tx completion message's payload allows the data SW's tx
 *      completion handler to not care about the format of the HTT
 *      tx completion message.
 * @param num - (zero-based) index to specify a single MSDU within the
 *      series of MSDUs referenced by the tx completion message
 * @return descriptor ID for the specified MSDU
 */
u_int16_t htt_tx_compl_desc_id(void *iterator, int num);


/*========================= tx descriptor operations ========================*/


/**
 * @brief Allocate a HTT abstract tx descriptor.
 * @details
 *  Allocate a HTT abstract tx descriptor from a pool within "consistent"
 *  memory, which is accessible by HIF and/or MAC DMA as well as by the
 *  host CPU.
 *  It is expected that the tx datapath will allocate HTT tx descriptors
 *  and link them with datapath SW tx descriptors up front as the driver
 *  is loaded.  Thereafter, the link from datapath SW tx descriptor to
 *  HTT tx descriptor will be maintained until the driver is unloaded.
 *
 * @param htt_pdev - handle to the HTT instance making the allocation
 * @return success -> descriptor handle, -OR- failure -> NULL
 */
void *
htt_tx_desc_assign(htt_pdev_handle htt_pdev, u_int16_t index, u_int32_t *paddr_lo);

/**
 * @brief return  htt tx desc pool physical addres
 *
 * @param htt_pdev - handle to the HTT instance that made the allocation
 * @return pool paaddr
 */
void *
htt_tx_desc_pool_paddr(htt_pdev_handle pdev);

/**
 * @brief return  htt tx frag desc size
 *
 * @param htt_pdev - handle to the HTT instance that made the allocation
 * @return htt tx frage desc size
 */
uint32_t
htt_tx_desc_frag_desc_size(htt_pdev_handle pdev);



/**
 * @brief Free a HTT abstract tx descriptor.
 *
 * @param htt_pdev - handle to the HTT instance that made the allocation
 * @param htt_tx_desc - the descriptor to free
 */
void *
htt_tx_frag_assign(htt_pdev_handle pdev, u_int16_t index);

/**
 * @brief Download a MSDU descriptor and (a portion of) the MSDU payload.
 * @details
 *  This function is used within LL systems to download a tx descriptor and
 *  the initial portion of the tx MSDU payload, and within HL systems to
 *  download the tx descriptor and the entire tx MSDU payload.
 *  The HTT layer determines internally how much of the tx descriptor
 *  actually needs to be downloaded. In particular, the HTT layer does not
 *  download the fragmentation descriptor, and only for the LL case downloads
 *  the physical address of the fragmentation descriptor.
 *  In HL systems, the tx descriptor and the entire frame are downloaded.
 *  In LL systems, only the tx descriptor and the header of the frame are
 *  downloaded.  To determine how much of the tx frame to download, this
 *  function assumes the tx frame is the default frame type, as specified
 *  by ol_cfg_frame_type.  "Raw" frames need to be transmitted through the
 *  alternate htt_tx_send_nonstd function.
 *
 * @param htt_pdev - the handle of the physical device sending the tx data
 * @param desc - the virtual handle of the descriptor, indirectly including
 *      the MSDU it refers to
 * @param msdu - the frame being transmitted
 * @param msdu_id - unique ID for the frame being transmitted
 * @return 0 -> success, -OR- 1 -> failure
 */
int
htt_tx_send_std(
    htt_pdev_handle htt_pdev,
    void *desc,
    qdf_nbuf_t msdu,
    u_int16_t msdu_id);

/* The htt scheduler for queued packets in htt
 * htt when unable to send to HTC because of lack of resource
 * forms a nbuf queue which is flushed when tx completion event from
 * target is recieved
 */

void
htt_tx_sched(htt_pdev_handle pdev);

/**
 * Initialize Fast path Header Cached
 * @param htt_pdev - the handle of the physical device sending the tx data
 * @param pkt_type  - the packet type
 *
 */
void
htt_ff_cache_init(
		htt_pdev_handle htt_pdev,
		enum htt_pkt_type pkt_type);
/**
 * Update header cache from the parent htt
 * @param htt_pdev - the handle of the physical device sending the tx data
 * @param hdrcache  - pointer to cached buffer
 *
 */
void
htt_hdrcache_update(
		htt_pdev_handle htt_pdev,
		int *hdrcache);

/**
 * Update packet type in header cache
 * @param hdrcache  - pointer to cached buffer
 * @param pkt_type  - new packet type
 *
 */
void
htt_hdrcache_update_pkttype(
        int *hdrcache,
        enum htt_pkt_type pkt_type);

/**
 * Update packet sub type in header cache
 * @param hdrcache  - pointer to cached buffer
 * @param pkt_type  - new packet sub type
 *
 */
void
htt_hdrcache_update_pktsubtype(
        int *hdrcache,
        u_int8_t pkt_subtype);

/**
 * @param htt_pdev - the handle of the physical device sending the tx data
 * @param netbuf  - the network buffer
 * @param msdu_id - unique ID for the frame being transmitted
 * @param htt_desc_ctx - host instance of htt_Tx_desc
 * @param vdev_id  - ID of the virtual device sending the tx data
 */
#define HTT_FF_CACHE_INIT(_htt_pdev, _pkt_type) htt_ff_cache_init(_htt_pdev, _pkt_type)
#define HTT_HDRCACHE_UPDATE(_pdev, _vdev) \
    htt_hdrcache_update(_pdev->htt_pdev, _vdev->hdrcache); \
    _vdev->osdev = _pdev->osdev; \
    _vdev->epid = HTT_EPID_GET(_pdev->htt_pdev); \
    _vdev->downloadlen = HTT_DOWNLOADLEN_GET(_pdev->htt_pdev);
#define HTT_HDRCACHE_UPDATE_PKTTYPE(_vdev, _pkt_type) \
    htt_hdrcache_update_pkttype((_vdev)->hdrcache, (_pkt_type))
#define HTT_HDRCACHE_UPDATE_PKTSUBTYPE(_vdev, _pkt_subtype) \
    htt_hdrcache_update_pktsubtype((_vdev)->hdrcache, (_pkt_subtype))

/**
 * @brief Same as htt_tx_send_std, but can handle raw frames.
 */
int
htt_tx_send_nonstd(
    htt_pdev_handle htt_pdev,
    void *desc,
    qdf_nbuf_t msdu,
    u_int16_t msdu_id,
    enum htt_pkt_type pkt_type);

/**
 * @brief Initialize the tx descriptor.
 * @details
 *  This function initializes the tx descriptor.
 *  The values for the standard tx descriptor fields are provided as
 *  function arguments.  Non-standard descriptor fields, which don't
 *  have function arguments to specify their value, are set to zero.
 *  An exception to this initialization of non-standard fields to zero
 *  is the "extended TID" field, which is initialized to the "invalid"
 *  value (0x1f).
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param desc - abstract handle to the tx descriptor
 * @param desc_id - ID to tag the descriptor with.
 *      The target FW uses this ID to identify to the host which MSDUs
 *      the target is referring to in its tx completion / postpone / drop
 *      messages.
 *      This ID is abstract - it is only interpreted inside the host
 *      tx datapath SW.  In practice, though, the ID is an index into an
 *      array of tx descriptor structs.
 *      This ID is used for both HL and LL systems, since in both systems
 *      the target may need to refer to a particular MSDU to explicitly tell
 *      the host when it may free the MSDU descriptor and network buffer.
 * @param len - length of the MSDU, in bytes
 * @param fmt - format of the MSDU (802.3, native WiFi, raw, or mgmt)
 *      Based on attach-time configuration, the tx frames provided by the
 *      OS to the tx data SW are expected to be either 802.3 format or
 *      the "native WiFi" variant of 802.11 format.
 *      Internally, the driver may also inject tx frames into the tx datapath,
 *      and these frames may be either 802.3 format, or 802.11 "raw" format,
 *      with no further 802.11 encapsulation needed.
 *      The tx frames are tagged with their frame format, so the target FW/HW
 *      will know how to interpret the packet's encapsulation headers when
 *      doing tx classification, and what form of 802.11 header encapsulation
 *      is needed, if any.
 * @param cksum_offload - Specify whether checksum offload is enabled or not
 *      Target FW uses this flag to turn on HW checksumming
 *      0x0 - No checksum offload
 *      0x1 - L3 header checksum only
 *      0x2 - L4 checksum only
 *      0x3 - L3 header checksum + L4 checksum
 */
#define HTT_TX_DESC_VADDR_OFFSET 8

static inline
void
htt_tx_desc_init(
    htt_pdev_handle pdev,
    void *htt_tx_desc,
    u_int16_t msdu_id,
    int msdu_len,
    u_int8_t vdev_id,
    enum wlan_frm_fmt fmt,
    qdf_nbuf_tx_cksum_t cksum_offload,
    u_int8_t tidno)
{
    u_int32_t *word0, *word1, *word3;
    A_COMPILE_TIME_ASSERT(cksum_type_match,
		(QDF_NBUF_TX_CKSUM_IP		== HTT_TX_L3_CKSUM_OFFLOAD) &&
		(QDF_NBUF_TX_CKSUM_TCP_UDP	== HTT_TX_L4_CKSUM_OFFLOAD));

    A_COMPILE_TIME_ASSERT(ext_tid_match,
		(QDF_NBUF_TX_EXT_TID_INVALID == HTT_TX_EXT_TID_INVALID));

    /* Dummy code added to suppress compilation warning because of
     *  cksum_type_match and ext_tid_match */
    cksum_type_match cksum;
    ext_tid_match ext_tid;

    *cksum = *ext_tid = 0;
    word0 = (u_int32_t *) htt_tx_desc;
    word1 = word0 + 1;
    /*
     * word2 is frag desc pointer
     * word3 is peer_id
     */
    word3 = word0 + 3; // Dword 3

    *word0 = 0;

    HTT_H2T_MSG_TYPE_SET(*word0, HTT_H2T_MSG_TYPE_TX_FRM);
    HTT_TX_DESC_PKT_TYPE_SET(*word0, fmt);
    HTT_TX_DESC_VDEV_ID_SET(*word0, vdev_id);
    HTT_TX_DESC_EXT_TID_SET(*word0, tidno);
    HTT_TX_DESC_CKSUM_OFFLOAD_SET(*word0, cksum_offload);

    *word1 = 0;
    HTT_TX_DESC_FRM_LEN_SET(*word1, msdu_len);
    HTT_TX_DESC_FRM_ID_SET(*word1, msdu_id);
    /* Initialize peer_id to INVALID_PEER bcoz this is NOT Reinjection path*/
    *word3 = HTT_INVALID_PEER;
}

#if MESH_MODE_SUPPORT
static inline
void
htt_tx_desc_rateinfo_init(
    htt_pdev_handle pdev,
    void *htt_tx_desc,
    u_int8_t sub_type,
    u_int8_t power,
    u_int8_t ratecode,
    u_int8_t retries,
    u_int8_t valid_flags)
{
    u_int32_t *word4,*word0;

    word0 = (u_int32_t *) htt_tx_desc;
    HTT_TX_DESC_PKT_SUBTYPE_SET(*word0, sub_type);

    word4 = word0 + 4;
    *word0 = *word0 | 0x80000000;/* enabling extension valid */
    *word4 = power | (ratecode << 8) | (retries << 16) | (valid_flags << 24);
}
#endif

/**
 * @brief Set a flag to indicate that the MSDU in question was postponed.
 * @details
 *  In systems in which the host retains its tx frame until the target sends
 *  a tx completion, the target has the option of discarding it's copy of
 *  the tx descriptor (and frame, for HL) and sending a "postpone" message
 *  to the host, to inform the host that it must eventually download the
 *  tx descriptor (and frame, for HL).
 *  Before the host downloads the postponed tx desc/frame again, it will use
 *  this function to set a flag in the HTT tx descriptor indicating that this
 *  is a re-send of a postponed frame, rather than a new frame.  The target
 *  uses this flag to keep the correct order between re-sent and new tx frames.
 *  This function is relevant for LL systems.
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param desc - abstract handle to the tx descriptor
 */
void
htt_tx_desc_flag_postponed(htt_pdev_handle pdev, void *desc);

/**
 * @brief Set a flag to tell the target that more tx downloads are en route.
 * @details
 *  At times, particularly in response to a U-APSD trigger in a HL system, the
 *  host will download multiple tx descriptors (+ frames, in HL) in a batch.
 *  The host will use this function to set a "more" flag in the initial
 *  and interior frames of the batch, to tell the target that more tx frame
 *  downloads within the batch are imminent.
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param desc - abstract handle to the tx descriptor
 */
void
htt_tx_desc_flag_batch_more(htt_pdev_handle pdev, void *desc);

/**
 * @brief Record the TID from host-side tx classification.
 * @details
 *  In a HL system, the host examines tx frames to determine which peer they
 *  are addressed to, and what traffic class (TID) they are.
 *  In a LL system, the host does not determine the TID through packet
 *  inspection, but the TID may be provided as a out-of-band spec by the
 *  OS (e.g. from a VLAN tag within the network buffer structure).
 *  For the cases when the TID is known by the host, this function is used
 *  to specify it to the target.
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param desc - abstract handle to the tx descriptor
 * @param tid - traffic ID of the tx data.  This is an extended TID value.
 *      Values 0-15 are for regular QoS data TIDs, while a value of 16
 *      Indicates non-QoS data, multicast data, or broadcast data.
 */
static inline
void
htt_tx_desc_tid(
    htt_pdev_handle pdev,
    void *desc,
    u_int8_t tid)
{
    /* clear old value */
    *(u_int32_t *) desc &= ~HTT_TX_DESC_EXT_TID_M;
    /* set new value */
    HTT_TX_DESC_EXT_TID_SET(*(u_int32_t *) desc, tid);
}

/**
 * @brief Specify the number of fragments in the fragmentation descriptor.
 * @details
 *  Specify the number of fragments within the MSDU, i.e. the number of
 *  elements within the fragmentation descriptor.
 *  For LL, this is used to terminate the list of fragments used by the
 *  HW's tx MAC DMA.
 *  For HL, this is used to terminate the list of fragments provided to
 *  HTC for download.
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param desc - abstract handle to the tx descriptor
 * @param num_frags - the number of fragments comprising the MSDU
 */
static inline
void
htt_tx_desc_num_frags(htt_pdev_handle pdev, void *desc, int num_frags)
{
    /*
     * Set the element after the valid frag elems to 0x0,
     * to terminate the list of fragments.
     */
    if (HTT_WIFI_IP(pdev,2,0)) {
        /** Skip TSO related 4 dwords WIFI2.0*/
        desc = (void *)&(((struct msdu_ext_desc_t *)desc)->frag_ptr0);
        /* Fragmentation ptr is 48 bit wide so clear the next dword as well */
        *((u_int32_t *)(((char *)desc) + (num_frags << 3))) = 0;
        *((u_int32_t *)(((char *)desc) + (num_frags << 3) + 4)) = 0;

    } else {
        *((u_int32_t *)
                (((char *) desc) + num_frags * 8)) = 0;
    }
}

/**
 * @brief Specify the location and size of a fragment of a tx MSDU.
 * @details
 *  In LL systems, the tx MAC DMA needs to know how the MSDU is constructed
 *  from fragments.
 *  In LL and HL systems, the HIF's download DMA to the target (LL: tx desc
 *  + header of tx payload; HL: tx desc + entire tx payload) needs to know
 *  where to find the fragments to download.
 *  The tx data SW uses this function to specify the location and size of
 *  each of the MSDU's fragments.
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param desc - abstract handle to the HTT tx descriptor
 * @param frag_num - which fragment is being specified (zero-based indexing)
 * @param frag_phys_addr - DMA/physical address of the fragment
 * @param frag_len - number of bytes within the fragment
 */
static inline
void
htt_tx_desc_frag(
    htt_pdev_handle pdev,
    void *desc,
    int frag_num,
    u_int32_t frag_phys_addr,
    u_int16_t frag_len)
{
    u_int32_t *word ;


    if (HTT_WIFI_IP(pdev,2,0)) {
        /** Skip TSO related 4 dwords WIFI2.0*/
        desc = (void *)&(((struct msdu_ext_desc_t *)desc)->frag_ptr0);
        word = (u_int32_t *) (((char *)desc) + (frag_num << 3));
        *word = frag_phys_addr;
        word++;
        *word = (frag_len<<16);
    } else {
        word = (u_int32_t *) (((char *) desc) + frag_num * 8);
        *word = frag_phys_addr;
        word++;
        *word = frag_len;
    }
}

/**
 * @brief Get the fragment location and size of a tx MSDU.
 * @details
 *  In LL systems, the tx MAC DMA needs to know how the MSDU is constructed
 *  from fragments.
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param desc - abstract handle to the HTT tx descriptor
 * @param frag_num - which fragment is being specified (zero-based indexing)
 * @param frag_phys_addr - DMA/physical address of the fragment
 * @param frag_len - number of bytes within the fragment
 */
static inline
void
htt_tx_desc_get_frag(
    htt_pdev_handle pdev,
    void *desc,
    int frag_num,
    u_int32_t *frag_phys_addr,
    u_int16_t *frag_len)
{
    u_int32_t *word ;


    if (HTT_WIFI_IP(pdev,2,0)) {
        /** Skip TSO related 4 dwords WIFI2.0*/
        desc = (void *)&(((struct msdu_ext_desc_t *)desc)->frag_ptr0);
        word = (u_int32_t *) (((char *)desc) + (frag_num << 3));
        *frag_phys_addr = *word;
        word++;
        *frag_len = *word;
    } else {
        word = (u_int32_t *) (((char *) desc) + frag_num * 8);
        *frag_phys_addr = *word;
        word++;
        *frag_len = *word;
    }
}
/**
 * @brief Specify the type and subtype of a tx frame.
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param type - format of the MSDU (802.3, native WiFi, raw, or mgmt)
 * @param sub_type - sub_type (relevant for raw frames)
 */
static inline
void
htt_tx_desc_type(
    htt_pdev_handle pdev,
    void *htt_tx_desc,
    enum wlan_frm_fmt type,
    u_int8_t sub_type)
{
    u_int32_t *word0;

    word0 = (u_int32_t *) htt_tx_desc;
    /* clear old values */
    *word0 &= ~(HTT_TX_DESC_PKT_TYPE_M | HTT_TX_DESC_PKT_SUBTYPE_M);
    /* write new values */
    HTT_TX_DESC_PKT_TYPE_SET(*word0, type);
    HTT_TX_DESC_PKT_SUBTYPE_SET(*word0, sub_type);
}

/***** HTT FETCH DESC APIs ****/
#ifdef QCA_LOWMEM_PLATFORM
#define HTT_MAX_NUM_FETCH_DESCS 512
#else
#define HTT_MAX_NUM_FETCH_DESCS 1024
#endif
/** htt_tx_fetch_desc_pool_alloc
 * @description - allocates the memory for fetch frame descriptors
 * @param  - htt pdev object
 * @param  - num of descriptors to be allocated in the pool
 */
void
htt_tx_fetch_desc_pool_alloc(struct htt_pdev_t *pdev, A_UINT32 num_elems);

/** htt_tx_fetch_desc_alloc
 * @description - reserves a fetch descriptor from the pool
 * @param  - htt pdev object
 */
struct htt_tx_fetch_desc_buf *
htt_tx_fetch_desc_alloc(struct htt_pdev_t *pdev);

/** htt_tx_fetch_desc_free
 * @description - releases the fetch descriptor back to the pool
 * @param  - htt pdev object
 * @param  - descriptor ID
 */
void
htt_tx_fetch_desc_free(struct htt_pdev_t *pdev, A_UINT32 desc_id);

/** htt_tx_fetch_desc_pool_free
 * @description - releases all the resources allocated for fetch desc pool
 * @param  - htt pdev object
 */
void
htt_tx_fetch_desc_pool_free(struct htt_pdev_t *pdev);

/***** TX MGMT DESC management APIs ****/

/* Number of mgmt descriptors in the pool */
#define HTT_MAX_NUM_MGMT_DESCS 32

/** htt_tx_mgmt_desc_pool_alloc
 * @description - allocates the memory for mgmt frame descriptors
 * @param  - htt pdev object
 * @param  - num of descriptors to be allocated in the pool
 */
void
htt_tx_mgmt_desc_pool_alloc(struct htt_pdev_t *pdev, A_UINT32 num_elems);

/** htt_tx_mgmt_desc_alloc
 * @description - reserves a mgmt descriptor from the pool
 * @param  - htt pdev object
 * @param  - pointer to variable to hold the allocated desc id
 * @param  - pointer to the mamangement from UMAC
 * @return - pointer the allocated mgmt descriptor
 */
qdf_nbuf_t
htt_tx_mgmt_desc_alloc(struct htt_pdev_t *pdev, A_UINT8 vdev_id, A_UINT32 *desc_id, qdf_nbuf_t mgmt_frm);

/** htt_tx_mgmt_desc_free
 * @description - releases the management descriptor back to the pool
 * @param  - htt pdev object
 * @param  - descriptor ID
 */
void
htt_tx_mgmt_desc_free(struct htt_pdev_t *pdev, A_UINT8 desc_id, A_UINT32 status);


/** htt_tx_mgmt_desc_mark_delayed_free
 * @description - This method is called while doing bss reset.
                  It notifies umac layer to do required cleanup for associated ni and
                  mark this management descriptor as delayed free. Once completion is received,
                  descriptor will be put back to the free pool and associated skb will be freed.

 * @param  - htt pdev object
 * @param  - vdev ID
 */
A_UINT32
htt_tx_mgmt_desc_mark_delayed_free(struct htt_pdev_t *pdev, A_UINT8 vdev_id, A_UINT32 status);


/** htt_tx_mgmt_desc_drain
 * @description - This method is called while doing bss recover.
                  Descriptor will be put back to the free pool and associated skb will be freed.

 * @param  - htt pdev object
 * @param  - vdev ID
 */
A_UINT32
htt_tx_mgmt_desc_drain(struct htt_pdev_t *pdev, A_UINT8 vdev_id);

/** htt_tx_mgmt_desc_pool_free
 * @description - releases all the resources allocated for mgmt desc pool
 * @param  - htt pdev object
 */
void
htt_tx_mgmt_desc_pool_free(struct htt_pdev_t *pdev);

#ifdef HTT_DBG
void
htt_tx_desc_display(htt_pdev_handle pdev, void *tx_desc);
#else
#define htt_tx_desc_display(pdev, tx_desc)
#endif

static inline
void htt_tx_desc_set_peer_id(uint32_t *htt_tx_desc, uint32_t peer_id)
{
    uint32_t *peer_id_field_ptr;

    peer_id_field_ptr  = (uint32_t *)(htt_tx_desc + HTT_TX_DESC_PEERID_DESC_PADDR_OFFSET_DWORD);

    *peer_id_field_ptr = (uint32_t)peer_id;
}

/**
 * @brief htt_h2t_fetch_resp_prepare
 *
 * @param pdev
 * @param fetch_desc
 * @param len
 *
 * @return
 */
struct htt_tx_fetch_resp_t *
htt_h2t_fetch_resp_prepare(struct ol_txrx_pdev_t *pdev,
        struct htt_tx_fetch_desc_buf *fetch_desc, A_UINT32 len);

/**
 * @brief htt_h2t_fetch_resp_tx
 *
 * @param pdev
 * @param fetch_desc
 *
 * @return
 */
int
htt_h2t_fetch_resp_tx(struct ol_txrx_pdev_t *pdev, struct htt_tx_fetch_desc_buf *fetch_desc);

#endif /* _OL_HTT_TX_API__H_ */
