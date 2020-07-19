/*
 *  Copyright (c) 2005 Atheros Communications Inc.  All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _WBUF_PRIVATE_LINUX_H
#define _WBUF_PRIVATE_LINUX_H

/*
 * Linux Definition and Implementation for wbuf
 */
#include <osdep_adf.h>
#define M_FLAG_SET    N_FLAG_SET
#define M_FLAG_GET    N_FLAG_GET
#define M_FLAG_CLR    N_FLAG_CLR

#define M_AMSDU       N_AMSDU
#define M_EAPOL       N_EAPOL
#define M_QOSNULL     N_QOSNULL
#define M_ARP         N_ARP
#define M_OFFCHANTX   N_OFFCHANTX
#define M_DHCP        N_DHCP
#define M_ERROR       N_ERROR
#define M_FF          N_FF
#define M_ENCAP_DONE  N_ENCAP_DONE
#define M_UAPSD       N_UAPSD

#define M_FMSS        N_FMSS
#define M_FMSQID      N_FMSQID

#define M_CLONED_CLR    N_CLONED_CLR
#define M_CLONED_GET    N_CLONED_GET
#define M_CLONED_SET    N_CLONED_SET

#define M_MOREDATA_CLR  N_MOREDATA_CLR
#define M_MOREDATA_GET  N_MOREDATA_GET
#define M_MOREDATA_SET  N_MOREDATA_SET

#define M_PROBING_CLR   N_PROBING_CLR
#define M_PROBING_GET   N_PROBING_GET
#define M_PROBING_SET   N_PROBING_SET

#define M_PWR_SAV_CLR   N_PWR_SAV_CLR
#define M_PWR_SAV_GET   N_PWR_SAV_GET
#define M_PWR_SAV_SET   N_PWR_SAV_SET

#if UNIFIED_SMARTANTENNA
#define __wbuf_is_smart_ant_train_packet          N_SMART_ANT_TRAIN_PKT_IS
#define __wbuf_smart_ant_set_train_packet         N_SMART_ANT_TRAIN_PKT_SET
#define __wbuf_smart_ant_unset_train_packet       N_SMART_ANT_TRAIN_PKT_UNSET
#endif

#ifdef ATH_SUPPORT_WAPI
#define M_WAI           N_WAI
#endif

#if ATH_SUPPORT_WIFIPOS
#define M_LOC_SET(skb)  M_FLAG_SET((skb), M_LOCATION)
#define M_LOC_CLR(skb)  M_FLAG_CLR((skb), M_LOCATION)
#define M_LOC_GET(skb)  M_FLAG_GET((skb), M_LOCATION)

#define M_LOC_KEEPALIVE_SET(skb)  M_FLAG_SET((skb), M_LOCATION_KEEPALIVE)
#define M_LOC_KEEPALIVE_GET(skb)  M_FLAG_GET((skb), M_LOCATION_KEEPALIVE)
#define __wbuf_set_wifipos              N_WIFIPOS_SET
#define __wbuf_get_wifipos              N_WIFIPOS_GET
#define __wbuf_set_wifipos_req_id       N_WIFIPOS_REQ_ID_SET
#define __wbuf_get_wifipos_req_id       N_WIFIPOS_REQ_ID_GET
#define __wbuf_set_pos                  N_POSITION_SET
#define __wbuf_is_pos                   N_POSITION_IS
#define __wbuf_clear_pos                N_POSITION_CLR
#define __wbuf_set_keepalive            N_POSITION_KEEPALIVE_SET
#define __wbuf_is_keepalive             N_POSITION_KEEPALIVE_IS
#define __wbuf_set_vmf                  N_POSITION_VMF_SET
#define __wbuf_is_vmf                   N_POSITION_VMF_IS
#define __wbuf_set_cts_frame            N_CTS_FRAME_SET
#define __wbuf_get_cts_frame            N_CTS_FRAME_GET
#endif
#define __wbuf_set_bsteering            N_BSTEERING_SET
#define __wbuf_get_bsteering            N_BSTEERING_GET


#include <ieee80211.h>

typedef struct sk_buff *__wbuf_t;
#define OSDEP_EAPOL_TID 6  /* send it on VO queue */


#if LMAC_SUPPORT_POWERSAVE_QUEUE
#define M_LEGACY_PS     N_LEGACY_PS
#endif
/*
 * WBUF private API's for Linux
 */

/*
 * NB: This function shall only be called for wbuf
 * with type WBUF_RX or WBUF_RX_INTRENAL.
 */
#if USE_MULTIPLE_BUFFER_RCV
/*
   When using multiple recv buffer, wbuf_init
   should reset skb tailroom to zero.
 */
#define __wbuf_init(_skb, _pktlen)  do {    \
    skb_trim(_skb, _skb->len);              \
    skb_put(_skb, _pktlen);                 \
    (_skb)->protocol = ETH_P_CONTROL;       \
} while (0)
#else
#define __wbuf_init(_skb, _pktlen)  do {    \
    skb_put(_skb, _pktlen);                 \
    (_skb)->protocol = ETH_P_CONTROL;       \
} while (0)
#endif

/**
 * APIs for MGMT Control Block
 **/
#define EXTRA_BUFFER sizeof(void*)

void wbuf_dealloc_mgmt_ctrl_block(__wbuf_t wbuf);

static INLINE void *
wbuf_alloc_mgmt_ctrl_block(__wbuf_t wbuf)
{
    struct ieee80211_cb *mgmt_cb_ptr = NULL;

    /*
     * For the packets which are requeued from the driver
     * ext_cb will be already allocated and destructor is
     * set to wbuf_dealloc_mgmt_ctrl_block. Do not allocate
     * ext_cb memory in this case.
     */
    if (unlikely(wbuf->destructor == wbuf_dealloc_mgmt_ctrl_block)) {
        return qdf_nbuf_get_ext_cb(wbuf);
    }

    mgmt_cb_ptr =  (struct ieee80211_cb *)
             qdf_mem_malloc(sizeof(struct ieee80211_cb) + EXTRA_BUFFER);

    if (mgmt_cb_ptr) {
        /*
         * For DA in TX path we allocate ext_cb memory when
         * packet is received in driver at vap xmit function
         *
         * In this case destructor might be set for the skb
         * in linux/ other subsytem. Save this destructor
         * pointer in the ext_cb to call this while freeing
         * skb and overwrite the destructor in skb to
         * wbuf_dealloc_mgmt_ctrl_block().
         */
        mgmt_cb_ptr->destructor = wbuf->destructor;
        qdf_nbuf_set_ext_cb(wbuf, mgmt_cb_ptr);
        wbuf->destructor = wbuf_dealloc_mgmt_ctrl_block;
    }
    return (void *)mgmt_cb_ptr;
}

static INLINE QDF_STATUS
wbuf_alloc_mgmt_ctrl_block_alloc_copy(__wbuf_t dest, __wbuf_t src)
{
    void *mgmt_cb = NULL;
    QDF_STATUS status = QDF_STATUS_SUCCESS;

    if (qdf_nbuf_get_ext_cb(src) == NULL)
        return status;

    mgmt_cb = wbuf_alloc_mgmt_ctrl_block(dest);
    if (mgmt_cb)
        qdf_mem_copy (qdf_nbuf_get_ext_cb(dest), qdf_nbuf_get_ext_cb(src),
                sizeof(struct ieee80211_cb) + EXTRA_BUFFER);
    else
        status = QDF_STATUS_E_NOMEM;

    return status;
}

#define __wbuf_header(_skb)                 ((_skb)->data)
#define __wbuf_getcb(_skb)          ((_skb)->cb)
#define __wbuf_get_cboffset(_skb)          (((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(_skb))->offset)
#define __wbuf_set_cboffset(_skb,hdrlen)          (((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(_skb))->offset = hdrlen)

/*
 * NB: The following two API's only work when skb's header
 * has not been ajusted, i.e., no one calls skb_push or skb_pull
 * on this skb yet.
 */
#define __wbuf_raw_data(_skb)               ((_skb)->data)

#if USE_MULTIPLE_BUFFER_RECV
#define __wbuf_get_len(_skb)                (((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->buf_size)
#else
#define __wbuf_get_len(_skb)                skb_tailroom(_skb)
#endif

#define __wbuf_get_datalen_temp(_skb)       ((_skb)->len)

#define __wbuf_get_pktlen(_skb)             ((_skb)->len)
#define __wbuf_get_tailroom(_skb)           skb_tailroom(_skb)
#define __wbuf_get_priority(_skb)           ((_skb)->priority)
#define __wbuf_set_priority(_skb, _p)       ((_skb)->priority = (_p))

#define __wbuf_hdrspace(_skb)               skb_headroom(_skb)

#define __wbuf_next(_skb)                   ((_skb)->next)
#define __wbuf_next_buf(_skb)               ((_skb)->next)
#define __wbuf_set_next(_skb,_next)         ((_skb)->next = (_next))
#define __wbuf_setnextpkt(_skb,_next)       ((_skb)->next = (_next))

#define __wbuf_free(_skb)                   qdf_nbuf_free(_skb)
#define __wbuf_ref(skb)                     skb_get(skb)

#define __wbuf_push(_skb, _len)             skb_push(_skb, _len)
#define __wbuf_clone(_osdev, _skb)          qdf_nbuf_copy(_skb)
#define __wbuf_trim(_skb, _len)             skb_trim(_skb, ((_skb)->len - (_len)))
#define __wbuf_pull(_skb, _len)             skb_pull(_skb, _len)
#define __wbuf_set_age(_skb,v)              ((_skb)->csum = v)
#define __wbuf_get_age(_skb)                ((_skb)->csum)
#define __wbuf_complete(_skb)               qdf_nbuf_free(_skb);
#define __wbuf_complete_any(_skb)           qdf_nbuf_free(_skb);
#define __wbuf_copydata(_skb, _offset, _len, _to)   \
    skb_copy_bits((_skb), (_offset), (_to), (_len))
#define __wbuf_copy(_skb)                   qdf_nbuf_copy(_skb)

#define wbuf_set_pktlen(_wbuf, _len)         __wbuf_set_pktlen(_wbuf, _len)
#define wbuf_set_len                         __wbuf_set_pktlen
#define wbuf_set_smpsactframe(_skb)
#define wbuf_is_smpsactframe(wbuf)                0
#define wbuf_set_moredata(wbuf)             __wbuf_set_moredata(wbuf)
#define wbuf_set_smpsframe(wbuf)
#define wbuf_set_status(_wbuf, _s)          __wbuf_set_status(_wbuf, _s)
#define wbuf_get_status(_wbuf)              __wbuf_get_status(_wbuf)
#define wbuf_is_moredata(wbuf)             __wbuf_is_moredata(wbuf)
#define wbuf_set_type(wbuf,type)           __wbuf_set_type(wbuf,type)
#define wbuf_get_type(wbuf)                __wbuf_get_type(wbuf)
#define wbuf_set_initimbf(wbuf)

#if 0
int __wbuf_is_smpsframe(struct sk_buff *skb);
void __wbuf_set_qosframe(struct sk_buff *skb);
int __wbuf_is_qosframe(struct sk_buff *skb);
#endif

int __wbuf_map_sg(osdev_t osdev, struct sk_buff *skb, dma_context_t context, void *arg);
void __wbuf_unmap_sg(osdev_t osdev, struct sk_buff *skb, dma_context_t context);
dma_addr_t __wbuf_map_single_tx(osdev_t osdev, struct sk_buff *skb, int direction, dma_context_t context);
void __wbuf_uapsd_update(struct sk_buff *skb);

int wbuf_start_dma(struct sk_buff *skb, sg_t *sg, u_int32_t n_sg, void *arg);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22))
  #define  UNI_SKB_END_POINTER(skb)   (skb)->end
#else
  #define  UNI_SKB_END_POINTER(skb)    skb_end_pointer(skb)
#endif

#define DHCP_SERVERPORT 67
#define DHCP_CLIENTPORT 68

static INLINE dma_addr_t
__wbuf_map_single(osdev_t osdev, struct sk_buff *skb, int direction, dma_addr_t *pa)
{
    /*
     * NB: do NOT use skb->len, which is 0 on initialization.
     * Use skb's entire data area instead.
     */
    *pa = bus_map_single(osdev, skb->data, UNI_SKB_END_POINTER(skb) - skb->data, direction);

    return *pa;
}

static INLINE void
__wbuf_unmap_single(osdev_t osdev, struct sk_buff *skb, int direction, dma_addr_t *pa)
{
    /*
     * Unmap skb's entire data area.
     */
    bus_unmap_single(osdev, *pa, UNI_SKB_END_POINTER(skb) - skb->data, direction);
}

static INLINE void
__wbuf_set_pktlen(struct sk_buff *skb, uint32_t len)
{
    if (skb->len > len) {
        skb_trim(skb, len);
    }
    else {
        skb_put(skb, (len - skb->len));
    }
}
static INLINE int
__wbuf_append(struct sk_buff *skb, u_int16_t size)
{
    skb_put(skb, size);
    return 0;
}

static INLINE __wbuf_t
__wbuf_coalesce(osdev_t os_handle, struct sk_buff *skb)
{
    /* The sk_buff is already contiguous in memory, no need to coalesce. */
    return skb;
}

static INLINE void
__wbuf_set_pwrsaveframe(struct sk_buff *skb)
{
    M_PWR_SAV_SET(skb);
}

static INLINE int
__wbuf_is_pwrsaveframe(struct sk_buff *skb)
{
    if (M_PWR_SAV_GET(skb))
        return 1;
    else
        return 0;
}

static INLINE void
__wbuf_set_peer(struct sk_buff *skb, struct wlan_objmgr_peer *peer)
{
    ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->peer_desc.peer = peer;
}

static INLINE void
__wbuf_set_txrx_desc_id(struct sk_buff *skb, u_int32_t txrx_desc_id)
{
    ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->peer_desc.mgmt_txrx_desc_id = txrx_desc_id;
}

static INLINE void
__wbuf_set_exemption_type(struct sk_buff *skb, int type)
{
        ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->exemptiontype = type;

}

static INLINE int
__wbuf_get_exemption_type(struct sk_buff *skb)
{
    return ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->exemptiontype;
}

static INLINE void
__wbuf_set_type(struct sk_buff *skb, int type)
{
        ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->type = type;
}

static INLINE int
__wbuf_get_type(struct sk_buff *skb)
{
    return ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->type;
}

static INLINE int
__wbuf_get_tid(struct sk_buff *skb)
{
    return ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->u_tid;
}

static INLINE int
__wbuf_is_qosframe(struct sk_buff *skb)
{
    return 0;
}

static INLINE int
__wbuf_is_smpsframe(struct sk_buff *skb)
{
    return 0;
}

static INLINE void
__wbuf_set_eapol(struct sk_buff *skb)
{
     M_FLAG_SET(skb, M_EAPOL);
}

static INLINE int
__wbuf_is_eapol(struct sk_buff *skb)
{
    if (M_FLAG_GET(skb, M_EAPOL))
        return 1;
    else
        return 0;
}

static INLINE int
__wbuf_is_dhcp(struct sk_buff *skb)
{
    if (M_FLAG_GET(skb, M_DHCP))
        return 1;
    else
        return 0;
}

static INLINE void
__wbuf_set_dhcp(struct sk_buff *skb)
{
     M_FLAG_SET(skb, M_DHCP);
}

static INLINE int
__wbuf_is_arp(struct sk_buff *skb)
{
    if (M_FLAG_GET(skb, M_ARP))
        return 1;
    else
        return 0;
}

static INLINE void
__wbuf_set_arp(struct sk_buff *skb)
{
     M_FLAG_SET(skb, M_ARP);
}

static INLINE int
__wbuf_is_offchan_tx(struct sk_buff *skb)
{
    if (M_FLAG_GET(skb, M_OFFCHANTX))
        return 1;
    else
        return 0;
}

static INLINE void
__wbuf_set_offchan_tx(struct sk_buff *skb)
{
     M_FLAG_SET(skb, M_OFFCHANTX);
}

static INLINE int
__wbuf_is_qosnull(struct sk_buff *skb)
{
    if (M_FLAG_GET(skb, M_QOSNULL))
        return 1;
    else
        return 0;
}

static INLINE void
__wbuf_set_qosnull(struct sk_buff *skb)
{
     M_FLAG_SET(skb, M_QOSNULL);
}

#ifdef ATH_SUPPORT_WAPI
static INLINE void
__wbuf_set_wai(struct sk_buff *skb)
{
     M_FLAG_SET(skb, M_WAI);
}

static INLINE int
__wbuf_is_wai(struct sk_buff *skb)
{
    if (M_FLAG_GET(skb, M_WAI))
        return 1;
    else
        return 0;
}
#endif

static INLINE int
__wbuf_is_encap_done(struct sk_buff *skb)
{
    if (M_FLAG_GET(skb, M_ENCAP_DONE))
        return 1;
    else
        return 0;
}
static INLINE void
__wbuf_set_encap_done(struct sk_buff *skb)
{
    M_FLAG_SET(skb, M_ENCAP_DONE);
}
static INLINE void
__wbuf_clr_encap_done(struct sk_buff *skb)
{
    M_FLAG_CLR(skb, M_ENCAP_DONE);
}

static INLINE struct wlan_objmgr_peer *
__wbuf_get_peer(struct sk_buff *skb)
{
    return ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->peer_desc.peer;
}

static INLINE u_int32_t
__wbuf_get_txrx_desc_id(struct sk_buff *skb)
{
    return ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->peer_desc.mgmt_txrx_desc_id;
}

static INLINE void
__wbuf_set_qosframe(struct sk_buff *skb)
{

}

static INLINE void
__wbuf_set_tid(struct sk_buff *skb, u_int8_t tid)
{
    ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->u_tid = tid;
}

static INLINE void
__wbuf_set_rate(struct sk_buff *skb, int8_t rate)
{

}

static INLINE int32_t
__wbuf_get_rate(struct sk_buff *skb)
{
    return 0;
}

static INLINE void
__wbuf_set_retries(struct sk_buff *skb, int8_t retries)
{

}

static INLINE int32_t
__wbuf_get_retries(struct sk_buff *skb)
{
    return 0;
}

static INLINE void
__wbuf_set_txpower(struct sk_buff *skb, int8_t txpower)
{

}

static INLINE int32_t
__wbuf_get_txpower(struct sk_buff *skb)
{
    return 0;
}

static INLINE void
__wbuf_set_txchainmask(struct sk_buff *skb, int8_t txchainmask)
{

}

static INLINE int32_t
__wbuf_get_txchainmask(struct sk_buff *skb)
{
    return 0;
}

static INLINE void
__wbuf_set_status(struct sk_buff *skb, u_int8_t status)
{
    if(status != WB_STATUS_OK)
        ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->flags |= M_ERROR;

}

static INLINE int
__wbuf_get_status(struct sk_buff *skb)
{
    if((((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->flags & M_ERROR) == 0)
        return WB_STATUS_OK;

    return WB_STATUS_TX_ERROR;
}


static INLINE void *
__wbuf_get_context(struct sk_buff *skb)
{
    return (void *)skb;
}
static INLINE void
__wbuf_set_amsdu(struct sk_buff *skb)
{
    M_FLAG_SET(skb, M_AMSDU);
}
static INLINE int
__wbuf_is_amsdu(struct sk_buff *skb)
{
    if (M_FLAG_GET(skb, M_AMSDU))
        return 1;
    else
        return 0;
}

static INLINE u_int8_t *
__wbuf_get_scatteredbuf_header(__wbuf_t wbuf, u_int16_t len)
{
    u_int8_t *pHead = NULL;
    return pHead;
}

static INLINE void
__wbuf_set_fastframe(struct sk_buff *skb)
{
    M_FLAG_SET(skb, M_FF);
}

static INLINE int
__wbuf_is_fastframe(struct sk_buff *skb)
{
    if (M_FLAG_GET(skb, M_FF))
        return 1;
    else
        return 0;
}

#ifdef ATH_SUPPORT_UAPSD
static INLINE void
__wbuf_set_uapsd(struct sk_buff *skb)
{
   M_FLAG_SET(skb, M_UAPSD);
}

static INLINE void
__wbuf_clear_uapsd(struct sk_buff *skb)
{
   M_FLAG_CLR(skb, M_UAPSD);
}

static INLINE int
__wbuf_is_uapsd(struct sk_buff *skb)
{
    if (M_FLAG_GET(skb, M_UAPSD))
        return 1;
    else
        return 0;
}
#endif

#if UMAC_SUPPORT_WNM
static INLINE void
__wbuf_set_fmsstream(struct sk_buff *skb)
{
   M_FLAG_SET(skb, M_FMSS);
}

static INLINE void
__wbuf_clear_fmsstream(struct sk_buff *skb)
{
   M_FLAG_CLR(skb, M_FMSS);
}

static INLINE int
__wbuf_is_fmsstream(struct sk_buff *skb)
{
    if (M_FLAG_GET(skb, M_FMSS))
        return 1;
    else
        return 0;
}

static INLINE u_int8_t
__wbuf_get_fmsqid(struct sk_buff *skb)
{
    if (!skb) return 0;
    return ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->fmsq_id;
}

static INLINE void
__wbuf_set_fmsqid(struct sk_buff *skb, u_int8_t fmsq_id)
{
    if (!skb) return;
    ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->fmsq_id = fmsq_id;
}

#endif /* UMAC_SUPPORT_WNM */


#if LMAC_SUPPORT_POWERSAVE_QUEUE
static INLINE void
__wbuf_set_legacy_ps(struct sk_buff *skb)
{
   M_FLAG_SET(skb, M_LEGACY_PS);
}

static INLINE void
__wbuf_clear_legacy_ps(struct sk_buff *skb)
{
   M_FLAG_CLR(skb, M_LEGACY_PS);
}

static INLINE int
__wbuf_is_legacy_ps(struct sk_buff *skb)
{
    if (M_FLAG_GET(skb, M_LEGACY_PS))
        return 1;
    else
        return 0;
}
#endif

static INLINE int
__wbuf_is_initimbf(struct sk_buff *skb)
{
    return 0;
}

static INLINE void
wbuf_tx_remove_8021q(struct sk_buff *skb)
{
}

static INLINE void
wbuf_tx_recovery_8021q(struct sk_buff *skb)
{
}

static INLINE void
wbuf_SetWMMInfo(struct sk_buff *skb, int tid)
{
}

static INLINE int
wbuf_get_tid_override_queue_mapping(struct sk_buff *skb, int tid_override_queue_mapping)
{
    u_int16_t map;

    /*
     * Here skb->queue_mapping is checked to find out the access category.
     * Streamboost classification engine will set the skb->queue_mapping.
     * However, when multi-queue nedev is enabled, then kernel may set
     * skb->queue_mapping itself. Need to check with streamboost team what
     * will happen when alloc_netdev_mq() is used.
     * TODO : check with streamboost team
     */
    map = skb->queue_mapping;
    if (tid_override_queue_mapping && map) {
        map = (map - 1) & 0x3;
        /* This will be overridden if dscp override is enabled */
        return WME_AC_TO_TID(map);
    }

    return -1;
}

static INLINE int
is_dhcp(struct sk_buff *skb, struct ether_header * eh)
{
    struct udphdr *udp;
    int udpSrc, udpDst;
    const struct iphdr *ip = (struct iphdr *) (skb->data + sizeof (struct ether_header));

    if (ip->protocol == IP_PROTO_UDP)
    {
        udp = (struct udphdr *) (skb->data + sizeof (struct ether_header) + sizeof(struct iphdr) );
        udpSrc = __constant_htons(udp->source);
        udpDst = __constant_htons(udp->dest);
        if ((udpSrc == DHCP_SERVERPORT) || (udpDst == DHCP_CLIENTPORT)) {
            return 1;
        }
    }
    return 0;
}

static INLINE int
wbuf_classify(struct sk_buff *skb, int tid_override_queue_mapping)
{
    struct ether_header *eh = (struct ether_header *) skb->data;
    u_int8_t tos = 0;
    int tid_q_map;
    /*
     * Find priority from IP TOS DSCP field
     */
    if (eh->ether_type == __constant_htons(ETHERTYPE_IP))
    {
        const struct iphdr *ip = (struct iphdr *)
                    (skb->data + sizeof (struct ether_header));
        /*
         * IP frame: exclude ECN bits 0-1 and map DSCP bits 2-7
         * from TOS byte.
         */
        tid_q_map = wbuf_get_tid_override_queue_mapping(skb, tid_override_queue_mapping);
        if (tid_q_map >= 0) {
            tos = tid_q_map;
        } else {
            tos = (ip->tos & (~INET_ECN_MASK)) >> IP_PRI_SHIFT;
        }
        if (is_dhcp(skb, eh)) {
            M_FLAG_SET(skb, M_DHCP);
        }
    }
    else if (eh->ether_type == htons(ETHERTYPE_IPV6)) {
        /* TODO
	 * use flowlabel
	 */
        unsigned long ver_pri_flowlabel;
	unsigned long pri;
	/*
            IPv6 Header.
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |Version| TrafficClass. |                   Flow Label          |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |         Payload Length        |  Next Header  |   Hop Limit   |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |                                                               |
            +                                                               +
            |                                                               |
            +                         Source Address                        +
            |                                                               |
            +                                                               +
            |                                                               |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
            |                                                               |
            +                                                               +
            |                                                               |
            +                      Destination Address                      +
            |                                                               |
            +                                                               +
            |                                                               |
            +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	*/

        ver_pri_flowlabel = *(unsigned long*)(eh + 1);
        pri = (ntohl(ver_pri_flowlabel) & IPV6_PRIORITY_MASK) >> IPV6_PRIORITY_SHIFT;
        tid_q_map = wbuf_get_tid_override_queue_mapping(skb, tid_override_queue_mapping);
        if (tid_q_map >= 0) {
            tos = tid_q_map;
        } else {
            tos = (pri & (~INET_ECN_MASK)) >> IP_PRI_SHIFT;
        }
    }
    else if (eh->ether_type == __constant_htons(ETHERTYPE_PAE)) {
        /* mark as EAPOL frame */
         M_FLAG_SET(skb, M_EAPOL);
         tos = OSDEP_EAPOL_TID;  /* send it on VO queue */;
    }
#ifdef ATH_SUPPORT_WAPI
    else if (eh->ether_type == __constant_htons(ETHERTYPE_WAI)) {
        /* mark as WAI frame */
         M_FLAG_SET(skb, M_WAI);
         tos = OSDEP_EAPOL_TID;  /* send it on VO queue */;
    }
#endif
    else if (eh->ether_type == __constant_htons(ETHERTYPE_ARP)) {
        M_FLAG_SET(skb, M_ARP);
    }
    return tos;
}

static INLINE int
wbuf_mark_eapol(struct sk_buff *skb)
{
    struct ether_header *eh = (struct ether_header *) skb->data;
    if (eh->ether_type == __constant_htons(ETHERTYPE_PAE)) {
        /* mark as EAPOL frame */
         M_FLAG_SET(skb, M_EAPOL);
         return 1;
    }

    return 0;
}

static INLINE int
wbuf_get_iptos(struct sk_buff *skb, u_int32_t *is_igmp, void **iph)
{
    struct ether_header *eh = (struct ether_header *) skb->data;
    u_int8_t tos = 0;

    *iph = NULL;
    *is_igmp = 0;
    /*
     * Find priority from IP TOS DSCP field
     */
    if (eh->ether_type == __constant_htons(ETHERTYPE_IP))
    {
        const struct iphdr *ip = (struct iphdr *)
            (skb->data + sizeof (struct ether_header));

        *iph = (void *)ip;
        tos = ip->tos;
        if (ip->protocol == IPPROTO_IGMP)
            *is_igmp = 1;
    }
	else if(__constant_ntohs(eh->ether_type) == ETHERTYPE_IPV6)
	{
		struct ipv6hdr *ip6h=(struct ipv6hdr *)(eh+1);
	//	u_int8_t *nexthdr = (u_int8_t *)(ip6h + 1);
		if (ip6h->nexthdr == IPPROTO_HOPOPTS) {
	            *is_igmp = 1;
        }
	}

    return tos;
}

static INLINE int
wbuf_UserPriority(struct sk_buff *skb)
{
    return 0;
}

static INLINE int
wbuf_VlanId(struct sk_buff *skb)
{
    return 0;
}

#if ATH_SUPPORT_VLAN

#define VLAN_PRI_SHIFT  13
#define VLAN_PRI_MASK   7

/*
** Public Prototypes
*/

unsigned short qdf_net_get_vlan(osdev_t osif);

int qdf_net_is_vlan_defined(osdev_t osif);


static INLINE int
wbuf_8021p(struct sk_buff *skb)
{
    struct vlan_ethhdr *veth = (struct vlan_ethhdr *) skb->data;
    u_int8_t tos = 0;

    /*
    ** Determine if this is an 802.1p frame, and get the proper
    ** priority information as required
    */

    if ( veth->h_vlan_proto == __constant_htons(ETH_P_8021Q) )
    {
        tos = (veth->h_vlan_TCI >> VLAN_PRI_SHIFT) & VLAN_PRI_MASK;
    }

    return tos;
}

static INLINE int
qdf_net_get_vlan_tag(struct sk_buff *skb)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0)
    return ( vlan_tx_tag_get(skb) );
#else
    return ( skb_vlan_tag_get(skb) );
#endif
}

static INLINE int
qdf_net_vlan_tag_present(struct sk_buff *skb)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,0,0)
    return ( vlan_tx_tag_present(skb) );
#else
    return ( skb_vlan_tag_present(skb) );
#endif
}

#endif

static INLINE void
wbuf_concat(struct sk_buff *head, struct sk_buff *skb)
{
    if (skb_is_nonlinear(head))
    {
        KASSERT(0,("wbuf_concat: skb is nonlinear"));
    }
    if (skb_tailroom(head) < skb->len)
    {
#if USE_MULTIPLE_BUFFER_RCV

        /* For multiple buffer case, it's not a abnormal case if tailroom is
         * not enough. We should not call assert but handle it carefully.
         */

        if (!pskb_expand_head(head,
                              0, __wbuf_get_len(skb),
                              GFP_ATOMIC))
        {
            qdf_nbuf_free(skb);
            return;
        }
#else
        KASSERT(0,("wbuf_concat: Not enough space to concat"));
#endif
    }
    /* copy the skb data to the head */
    memcpy(skb_tail_pointer(head), skb->data, skb->len);
    /* Update tail and length */
    skb_put(head, skb->len);
    /* free the skb */
    qdf_nbuf_free(skb);
}
static INLINE void
__wbuf_set_moredata(struct sk_buff *skb)
{
    M_MOREDATA_SET(skb);
}

static INLINE int
__wbuf_is_moredata(struct sk_buff *skb)
{
    if (M_MOREDATA_GET(skb))
        return 1;
    else
        return 0;
}

static INLINE void
__wbuf_set_probing(struct sk_buff *skb)
{
    M_PROBING_SET(skb);
}

static INLINE void
__wbuf_clear_probing(struct sk_buff *skb)
{
    M_PROBING_CLR(skb);
}

static INLINE int
__wbuf_is_probing(struct sk_buff *skb)
{
    if (M_PROBING_GET(skb))
        return 1;
    else
        return 0;
}

static INLINE void
__wbuf_set_cloned(struct sk_buff *skb)
{
    M_CLONED_SET(skb);
}

static INLINE void
__wbuf_clear_cloned(struct sk_buff *skb)
{
    M_CLONED_CLR(skb);
}

static INLINE int
__wbuf_is_cloned(struct sk_buff *skb)
{
    if (M_CLONED_GET(skb))
        return 1;
    else
        return 0;
}
static INLINE void
__wbuf_set_complete_handler(struct sk_buff *skb,void *handler, void *arg)
{
    struct ieee80211_cb *ctx = ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb));
    ctx->complete_handler = handler;
    ctx->complete_handler_arg = arg;
}

static INLINE void
__wbuf_get_complete_handler(struct sk_buff *skb,void **handler, void **arg)
{
    struct ieee80211_cb *ctx = ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb));
    *handler = ctx->complete_handler;
    *arg = ctx->complete_handler_arg;
}

#if defined(ATH_SUPPORT_VOWEXT) || defined(ATH_SUPPORT_IQUE) || UMAC_SUPPORT_NAWDS != 0
static INLINE u_int8_t
__wbuf_get_firstxmit(struct sk_buff *skb)
{
    if (!skb) return 0;
    return ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->firstxmit;
}
static INLINE void
__wbuf_set_firstxmit(struct sk_buff *skb, u_int8_t firstxmit)
{
    if (!skb) return;
    ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->firstxmit = firstxmit;
}

static INLINE u_int32_t
__wbuf_get_firstxmitts(struct sk_buff *skb)
{
    if (!skb) return 0;
    return ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->_u.firstxmitts;
}
static INLINE void
__wbuf_set_firstxmitts(struct sk_buff *skb, u_int32_t firstxmitts)
{
    if (!skb) return;
    ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->_u.firstxmitts = firstxmitts;
}

static INLINE void
__wbuf_clear_flags(struct sk_buff *skb)
{
    M_FLAG_CLR(skb, 0xFFFF);
}
#endif /* ATH_SUPPORT_VOWEXT*/

#if defined (ATH_SUPPORT_IQUE)
static INLINE u_int32_t
__wbuf_get_qin_timestamp(struct sk_buff *skb)
{
    if (!skb) return 0;
    return ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->qin_timestamp;
}
static INLINE void
__wbuf_set_qin_timestamp(struct sk_buff *skb, u_int32_t qin_timestamp)
{
    if (!skb) return;
    ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->qin_timestamp = qin_timestamp;
}
#endif

#if ATH_SUPPORT_WIFIPOS
static INLINE void
__wbuf_set_netlink_pid(struct sk_buff *skb, u_int32_t val)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
    NETLINK_CB(skb).pid = val;
#else
    NETLINK_CB(skb).portid = val;
#endif
}
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static INLINE void
__wbuf_set_netlink_dst_pid(struct sk_buff *skb, u_int32_t val)
{
    NETLINK_CB(skb).dst_pid = val;
}
#endif
static INLINE void
__wbuf_set_netlink_dst_group(struct sk_buff *skb, u_int32_t val)
{
    NETLINK_CB(skb).dst_group = val;
}
#endif /* ATH_SUPPORT_WIFIPOS*/

typedef struct _acfg_extra_data_cb{
    u_int8_t acfg_nl_msg_resp_flag;
    u_int32_t acfg_nl_msg_resp_pid;
} acfg_extra_data_cb;

/*Linux netlink puts struct netlink_skb_parms in skb->cb */
/*This macro returns the end of the netlink struct, */
/*where's safe to put QCA stuff */
#define ACFG_END_OF_NL_CB(skb) ((u_int8_t *)&(NETLINK_CB(skb)) + sizeof(struct netlink_skb_parms))

static INLINE void
__wbuf_set_netlink_acfg_flags(struct sk_buff *skb, u_int8_t val)
{
    acfg_extra_data_cb *data = (acfg_extra_data_cb *)ACFG_END_OF_NL_CB(skb);

    (data->acfg_nl_msg_resp_flag) = val;
}
static INLINE u_int8_t
__wbuf_get_netlink_acfg_flags(struct sk_buff *skb)
{
    acfg_extra_data_cb *data = (acfg_extra_data_cb *)ACFG_END_OF_NL_CB(skb);

    return (data->acfg_nl_msg_resp_flag);
}

static INLINE void
__wbuf_set_netlink_acfg_pid(struct sk_buff *skb, u_int32_t val)
{
    acfg_extra_data_cb *data = (acfg_extra_data_cb *)ACFG_END_OF_NL_CB(skb);

    (data->acfg_nl_msg_resp_pid) = val;
}
static INLINE u_int32_t
__wbuf_get_netlink_acfg_pid(struct sk_buff *skb)
{
    acfg_extra_data_cb *data = (acfg_extra_data_cb *)ACFG_END_OF_NL_CB(skb);

    return (data->acfg_nl_msg_resp_pid);
}


static INLINE void
__wbuf_set_rx_info(struct sk_buff *skb, void * info, u_int32_t len)
{
    u_int32_t max = sizeof(((struct sk_buff *)0)->cb);
    len = (len > max ? max : len);
    memcpy(skb->cb, info, len);
}

static INLINE void
__wbuf_set_tx_rate(struct sk_buff *skb, uint8_t nss, uint8_t preamble,
       uint8_t mcs, bool he_target)
{
#define RC_V1_PREAMBLE_MASK             0x7
#define RC_V1_NSS_MASK                  0x7
#define RC_V1_MCS_MASK                  0x1f
#define PREAMBLE_OFFSET_IN_LEGACY_RC    6
#define NSS_OFFSET_IN_LEGACY_RC         4
#define ASSEMBLE_RATE_LEGACY(_mcs, _nss, _pream)            \
            (((_pream) << PREAMBLE_OFFSET_IN_LEGACY_RC) |   \
             ((_nss) << NSS_OFFSET_IN_LEGACY_RC) | (_mcs))

#define RC_LEGACY_PREAMBLE_MASK       0x3
#define RC_LEGACY_NSS_MASK            0x3
#define RC_LEGACY_MCS_MASK            0xf
#define VERSION_OFFSET_IN_V1_RC       28
#define PREAMBLE_OFFSET_IN_V1_RC      8
#define NSS_OFFSET_IN_V1_RC           5
#define ASSEMBLE_RATE_V1(_mcs, _nss, _pream)                \
            ((((1) << VERSION_OFFSET_IN_V1_RC) |            \
             ((_pream) << PREAMBLE_OFFSET_IN_V1_RC)) |      \
             ((_nss) << NSS_OFFSET_IN_V1_RC) | (_mcs))

    if (!skb) return;

    if(he_target) {
        ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->_uu.tx_ctrl.rate =
            ASSEMBLE_RATE_V1((mcs & RC_V1_MCS_MASK),
                    (nss & RC_V1_NSS_MASK),
                    (preamble & RC_V1_PREAMBLE_MASK));
    }
    else {
        ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->_uu.tx_ctrl.rate =
            ASSEMBLE_RATE_LEGACY(mcs & RC_LEGACY_MCS_MASK,
                    nss & RC_LEGACY_NSS_MASK,
                    preamble & RC_LEGACY_PREAMBLE_MASK);
    }
}


static INLINE void
__wbuf_set_tx_ctrl(struct sk_buff *skb, uint8_t retry, uint8_t power, uint8_t chain)
{
    if (!skb) return;

    ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->_uu.tx_ctrl.retries = retry;
    ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->_uu.tx_ctrl.txchainmask = chain;
    ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->_uu.tx_ctrl.txpower = power;
}

static INLINE void
__wbuf_get_tx_ctrl(struct sk_buff *skb, uint8_t *retry, uint8_t *power, uint8_t *chain)
{
    if (!skb) return;

    if (retry)
        *retry = ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->_uu.tx_ctrl.retries;
    if (chain)
        *chain = ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->_uu.tx_ctrl.txchainmask;
    if (power)
        *power = ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->_uu.tx_ctrl.txpower;
}

static INLINE void
__wbuf_get_tx_rate(struct sk_buff *skb, uint32_t *rate)
{
    if (!skb) return;

    if (rate)
        *rate = ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->_uu.tx_ctrl.rate;
}

static INLINE void
__wbuf_set_pid(struct sk_buff *skb, u_int32_t pid)
{
    if (!skb) return;

    ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->request_id = pid;
}

static INLINE void
__wbuf_get_pid(struct sk_buff *skb, u_int32_t *pid)
{
    if (!skb) return;

    *pid = ((struct ieee80211_cb *)qdf_nbuf_get_ext_cb(skb))->request_id;
}
#endif // #ifdef WBUF_PRIVATE_LINUX_H
