/*
 * Copyright (c) 2011-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2005, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#ifndef _WBUF_H
#define _WBUF_H

/*
 * Definition and API's for OS-independent packet data structure
 */
enum wbuf_type{
    WBUF_TX_DATA = 0,               /* normal tx data frame sent from NDIS */
    WBUF_TX_MGMT,                   /* internally generated management frame */
    WBUF_TX_BEACON,                 /* internally generated beacon frame */
    WBUF_RX,                        /* rx buffer that will be used for DMA */
    WBUF_RX_INTERNAL,               /* rx buffer that won't be used for DMA */
    WBUF_TX_INTERNAL,
#ifndef UMAC
    WBUF_TX_COALESCING,
#endif
    WBUF_TX_CTL,                    /* used to send control frames,
                                     * currently cfend */
    WBUF_MAX_TYPE
};

#define WB_STATUS_OK                0x0000
#define WB_STATUS_TX_ERROR          0x0001

#include <wbuf_private.h>
enum wbuf_exemption_type {
    WBUF_EXEMPT_NO_EXEMPTION,
    WBUF_EXEMPT_ALWAYS,
    WBUF_EXEMPT_ON_KEY_MAPPING_KEY_UNAVAILABLE
};

/*
 * OS independent packet data structure. Each OS should define its own wbuf
 * in wbuf_private.h and implement __wbuf_xxx() functions.
 * Other layers should not directly reference the members in wbuf structure.
 */
typedef __wbuf_t wbuf_t;

/*
 * API's for wbuf
 */
#ifdef MEMORY_DEBUG
wbuf_t wbuf_alloc_debug(osdev_t os_handle, enum wbuf_type type,
			u_int32_t len, uint8_t *file, uint32_t line);
#define wbuf_alloc(os_handle, wbuf_type, len) \
	wbuf_alloc_debug(os_handle, wbuf_type,len, __FILE__, __LINE__)
#else
wbuf_t wbuf_alloc(osdev_t os_handle, enum wbuf_type, u_int32_t len);
#endif

#define wbuf_release(os_handle, wbuf) qdf_nbuf_free(wbuf)

static INLINE u_int8_t *wbuf_getcb(wbuf_t wbuf) { return __wbuf_getcb(wbuf); }
static INLINE void wbuf_set_cboffset(wbuf_t wbuf, u_int8_t hdrlen) { __wbuf_set_cboffset(wbuf, hdrlen); }
static INLINE u_int8_t wbuf_get_cboffset(wbuf_t wbuf) { return __wbuf_get_cboffset(wbuf); }

static INLINE u_int8_t *wbuf_header(wbuf_t wbuf) { return __wbuf_header(wbuf); }
static INLINE u_int16_t wbuf_get_pktlen(wbuf_t wbuf) { return __wbuf_get_pktlen(wbuf); }
static INLINE void *wbuf_raw_data(wbuf_t wbuf) { return __wbuf_raw_data(wbuf); }
static INLINE u_int32_t wbuf_get_len(wbuf_t wbuf) { return __wbuf_get_len(wbuf); }

/*
 * wbuf_get_datalen_temp *** Should NOT *** be used again
 * in any other part of the code. This is a temporary fix
 * to a semantic problem with the use of wbuf_get_len.
 * wbuf_get_len has been interchangeably used to mean
 * wbuf_get_tailroom, which causes issues for darwin an
 * netbsd, and presumably for windows as well. This macro
 * and its use should be removed as soon as the right fix
 * is implemented.
 */

static INLINE u_int32_t wbuf_get_datalen_temp(wbuf_t wbuf) { return __wbuf_get_datalen_temp(wbuf); }

static INLINE u_int32_t wbuf_get_tailroom(wbuf_t wbuf) { return __wbuf_get_tailroom(wbuf); }

static INLINE int wbuf_get_priority(wbuf_t wbuf) { return __wbuf_get_priority(wbuf); }
static INLINE void wbuf_set_priority(wbuf_t wbuf, int priority) { __wbuf_set_priority(wbuf, priority); }
static INLINE int wbuf_get_tid(wbuf_t wbuf) { return __wbuf_get_tid(wbuf); }
static INLINE void wbuf_set_tid(wbuf_t wbuf, u_int8_t tid) { __wbuf_set_tid(wbuf, tid); }

#if UMAC_PER_PACKET_DEBUG
static INLINE void wbuf_set_rate(wbuf_t wbuf, int8_t rate) { __wbuf_set_rate(wbuf, rate); }
static INLINE int32_t wbuf_get_rate(wbuf_t wbuf) { return __wbuf_get_rate(wbuf); }
static INLINE void wbuf_set_retries(wbuf_t wbuf, int8_t retries) {  __wbuf_set_retries(wbuf, retries); }
static INLINE int32_t wbuf_get_retries(wbuf_t wbuf) { return __wbuf_get_retries(wbuf); }
static INLINE void wbuf_set_txpower(wbuf_t wbuf, int8_t txpower) { __wbuf_set_txpower(wbuf, txpower); }
static INLINE int32_t wbuf_get_txpower(wbuf_t wbuf) { return __wbuf_get_txpower(wbuf); }
static INLINE void wbuf_set_txchainmask(wbuf_t wbuf,int8_t txchainmask) { __wbuf_set_txchainmask(wbuf, txchainmask); }
static INLINE int32_t wbuf_get_txchainmask(wbuf_t wbuf) { return __wbuf_get_txchainmask(wbuf); }
#endif

static INLINE struct wlan_objmgr_peer *wbuf_get_peer(wbuf_t wbuf) { return __wbuf_get_peer(wbuf); }
static INLINE void wbuf_set_peer(wbuf_t wbuf, struct wlan_objmgr_peer *peer) { __wbuf_set_peer(wbuf, peer); }
static INLINE void *wbuf_get_context(wbuf_t wbuf) { return __wbuf_get_context(wbuf); }
static INLINE u_int32_t wbuf_get_txrx_desc_id(wbuf_t wbuf) {
    return __wbuf_get_txrx_desc_id(wbuf);
}
static INLINE void wbuf_set_txrx_desc_id(wbuf_t wbuf, u_int32_t desc_id) {
    __wbuf_set_txrx_desc_id(wbuf, desc_id);
}

#ifdef QCA_PARTNER_PLATFORM
static INLINE int wbuf_is_encap_done(wbuf_t wbuf) { return __wbuf_is_encap_done(wbuf); }
static INLINE void wbuf_set_encap_done(wbuf_t wbuf) { __wbuf_set_encap_done(wbuf); }
static INLINE void wbuf_clr_encap_done(wbuf_t wbuf) { __wbuf_clr_encap_done(wbuf); }
#endif

static INLINE int wbuf_is_eapol(wbuf_t wbuf) { return __wbuf_is_eapol(wbuf); }
static INLINE void wbuf_set_eapol(wbuf_t wbuf) { __wbuf_set_eapol(wbuf); }
static INLINE void wbuf_set_dhcp(wbuf_t wbuf) { __wbuf_set_dhcp(wbuf); }
static INLINE int wbuf_is_dhcp(wbuf_t wbuf) { return __wbuf_is_dhcp(wbuf); }
static INLINE void wbuf_set_qosnull(wbuf_t wbuf) {  __wbuf_set_qosnull(wbuf); }
static INLINE int wbuf_is_qosnull(wbuf_t wbuf) { return __wbuf_is_qosnull(wbuf); }
static INLINE void wbuf_set_arp(wbuf_t wbuf) {  __wbuf_set_arp(wbuf); }
static INLINE int wbuf_is_arp(wbuf_t wbuf) { return __wbuf_is_arp(wbuf); }
static INLINE void wbuf_set_offchan_tx(wbuf_t wbuf) {  __wbuf_set_offchan_tx(wbuf); }
static INLINE int wbuf_is_offchan_tx(wbuf_t wbuf) { return __wbuf_is_offchan_tx(wbuf); }


static INLINE int wbuf_is_highpriority(wbuf_t wbuf)
{
    if(unlikely(wbuf_is_qosnull(wbuf)))
            return 1;
    if(unlikely(wbuf_is_eapol(wbuf)))
            return 1;
    if(unlikely(wbuf_is_dhcp(wbuf)))
            return 1;
    if(unlikely(wbuf_is_arp(wbuf)))
            return 1;

    return 0;
}

static INLINE void wbuf_set_amsdu(wbuf_t wbuf) { __wbuf_set_amsdu(wbuf); }
static INLINE int wbuf_is_amsdu(wbuf_t wbuf) { return __wbuf_is_amsdu(wbuf); }
static INLINE void wbuf_set_pwrsaveframe(wbuf_t wbuf) { __wbuf_set_pwrsaveframe(wbuf); }
static INLINE int wbuf_is_pwrsaveframe(wbuf_t wbuf) { return __wbuf_is_pwrsaveframe(wbuf); }
static INLINE int wbuf_is_smpsframe(wbuf_t wbuf) { return __wbuf_is_smpsframe(wbuf); }
static INLINE int wbuf_is_fastframe(wbuf_t wbuf) { return __wbuf_is_fastframe(wbuf); }
static INLINE void wbuf_set_fastframe(wbuf_t wbuf) { __wbuf_set_fastframe(wbuf); }
static INLINE int wbuf_is_qosframe(wbuf_t wbuf) { return __wbuf_is_qosframe(wbuf); }
static INLINE void wbuf_set_qosframe(wbuf_t wbuf) { __wbuf_set_qosframe(wbuf); }
#ifdef ATH_SUPPORT_WAPI
#ifdef __linux__
static INLINE int wbuf_is_wai(wbuf_t wbuf) { return __wbuf_is_wai(wbuf); }
#else //other os need implement this function to support WAPI as AP
#define wbuf_is_wai(wbuf) false
#endif
#endif

static INLINE int wbuf_get_exemption_type(wbuf_t wbuf) { return __wbuf_get_exemption_type(wbuf); }
static INLINE void wbuf_set_exemption_type(wbuf_t wbuf, int type) { __wbuf_set_exemption_type(wbuf, type); }

#ifdef ATH_SUPPORT_UAPSD
static INLINE int wbuf_is_uapsd(wbuf_t wbuf) { return __wbuf_is_uapsd(wbuf); }
static INLINE void wbuf_set_uapsd(wbuf_t wbuf) { __wbuf_set_uapsd(wbuf);}
static INLINE void wbuf_clear_uapsd(wbuf_t wbuf) { __wbuf_clear_uapsd(wbuf);}
static INLINE void wbuf_uapsd_update(wbuf_t wbuf) { __wbuf_uapsd_update(wbuf); }
#else
#define wbuf_is_uapsd(wbuf)      false
#define wbuf_set_uapsd(wbuf)
#define wbuf_clear_uapsd(wbuf)
#define wbuf_uapsd_update(wbuf)
#endif

#if UMAC_SUPPORT_WNM
static INLINE int wbuf_is_fmsstream(wbuf_t wbuf) { return __wbuf_is_fmsstream(wbuf); }
static INLINE void wbuf_set_fmsstream(wbuf_t wbuf) { __wbuf_set_fmsstream(wbuf);}
static INLINE void wbuf_clear_fmsstream(wbuf_t wbuf) { __wbuf_clear_fmsstream(wbuf);}
static INLINE void wbuf_set_fmsqid(wbuf_t wbuf, int qid) { __wbuf_set_fmsqid(wbuf, qid); }
static INLINE int wbuf_get_fmsqid(wbuf_t wbuf) { return __wbuf_get_fmsqid(wbuf);}
#else
#define wbuf_is_fmsstream(wbuf)      false
#define wbuf_set_fmsstream(wbuf)
#define wbuf_clear_fmsstream(wbuf)
#endif

#if LMAC_SUPPORT_POWERSAVE_QUEUE
static INLINE int wbuf_is_legacy_ps(wbuf_t wbuf) { return __wbuf_is_legacy_ps(wbuf); }
static INLINE void wbuf_set_legacy_ps(wbuf_t wbuf) { __wbuf_set_legacy_ps(wbuf);}
static INLINE void wbuf_clear_legacy_ps(wbuf_t wbuf) { __wbuf_clear_legacy_ps(wbuf);}
#else
#define wbuf_is_legacy_ps(wbuf)      false
#define wbuf_set_legacy_ps(wbuf)
#define wbuf_clear_legacy_ps(wbuf)
#endif

static INLINE int wbuf_is_initimbf(wbuf_t wbuf) { return __wbuf_is_initimbf(wbuf); }

static INLINE void wbuf_complete(wbuf_t wbuf) {

        __wbuf_complete(wbuf);
}

static INLINE void wbuf_complete_any(wbuf_t wbuf) {

        __wbuf_complete_any(wbuf);
}

static INLINE int
wbuf_map_sg(osdev_t os_handle, wbuf_t wbuf,  dma_context_t context, void *arg)
{
    return __wbuf_map_sg(os_handle, wbuf, context, arg);
}

static INLINE void
wbuf_unmap_sg(osdev_t os_handle, wbuf_t wbuf, dma_context_t context)
{
    __wbuf_unmap_sg(os_handle, wbuf, context);
}

static INLINE dma_addr_t
wbuf_map_single(osdev_t os_handle, wbuf_t wbuf, int direction, dma_context_t context)
{
    return __wbuf_map_single(os_handle, wbuf, direction, context);
}

static INLINE void
wbuf_unmap_single(osdev_t os_handle, wbuf_t wbuf, int direction, dma_context_t context)
{
    __wbuf_unmap_single(os_handle, wbuf, direction, context);
}

static INLINE dma_addr_t
wbuf_map_single_tx(osdev_t os_handle, wbuf_t wbuf, int direction, dma_context_t context)
{
    return __wbuf_map_single_tx(os_handle, wbuf, direction, context);
}

static INLINE void wbuf_init(wbuf_t wbuf, u_int16_t pktlen) { __wbuf_init(wbuf, pktlen); }
static INLINE void wbuf_free(wbuf_t wbuf)
{
    __wbuf_free(wbuf);
}
static INLINE void wbuf_ref(wbuf_t wbuf) { __wbuf_ref(wbuf); }

static INLINE u_int8_t *wbuf_push(wbuf_t wbuf, u_int16_t size) { return __wbuf_push(wbuf, size); }
static INLINE u_int16_t wbuf_hdrspace(wbuf_t wbuf) { return __wbuf_hdrspace(wbuf); }
static INLINE void wbuf_trim(wbuf_t wbuf, u_int16_t size) { __wbuf_trim(wbuf, size); }
static INLINE u_int8_t *wbuf_pull(wbuf_t wbuf, u_int16_t size) { return __wbuf_pull(wbuf, size); }
static INLINE int wbuf_append(wbuf_t wbuf, u_int16_t size) { return __wbuf_append(wbuf, size); }
static INLINE u_int8_t *wbuf_get_scatteredbuf_header(wbuf_t wbuf, u_int16_t len) { return __wbuf_get_scatteredbuf_header(wbuf, len); }

static INLINE int wbuf_copydata(wbuf_t wbuf, u_int16_t off, u_int16_t len, void *out_data)
{
    return __wbuf_copydata(wbuf, off, len, out_data);
}

#if ATH_SUPPORT_IQUE || UMAC_SUPPORT_NAWDS != 0
static INLINE wbuf_t wbuf_copy(wbuf_t wbuf) {
    wbuf_t wbuf1;
    QDF_STATUS status = QDF_STATUS_SUCCESS;

    wbuf1 = __wbuf_copy(wbuf);
    if (wbuf1) {
        status = wbuf_alloc_mgmt_ctrl_block_alloc_copy(wbuf1, wbuf);
        if (QDF_IS_STATUS_ERROR(status)) {
            wbuf_free(wbuf1);
            wbuf1 = NULL;
        }
    }

    return wbuf1;
}
static INLINE int wbuf_is_probing(wbuf_t wbuf) { return __wbuf_is_probing(wbuf); }
static INLINE void wbuf_set_probing(wbuf_t wbuf) { __wbuf_set_probing(wbuf); }
static INLINE void wbuf_clear_probing(wbuf_t wbuf) { __wbuf_clear_probing(wbuf); }
#endif
#if ATH_RXBUF_RECYCLE
static INLINE int wbuf_is_cloned(wbuf_t wbuf) { return __wbuf_is_cloned(wbuf); }
static INLINE void wbuf_set_cloned(wbuf_t wbuf) { __wbuf_set_cloned(wbuf); }
static INLINE void wbuf_clear_cloned(wbuf_t wbuf) { __wbuf_clear_cloned(wbuf); }
#endif

#if ATH_SUPPORT_WIFIPOS
static INLINE void wbuf_set_wifipos(wbuf_t wbuf, void *data) { __wbuf_set_wifipos(wbuf, data); }
static INLINE void *wbuf_get_wifipos(wbuf_t wbuf) { return __wbuf_get_wifipos(wbuf); }
static INLINE void wbuf_set_wifipos_req_id(wbuf_t wbuf, u_int32_t request_id) { __wbuf_set_wifipos_req_id(wbuf, request_id); }
static INLINE u_int32_t wbuf_get_wifipos_req_id(wbuf_t wbuf) { return __wbuf_get_wifipos_req_id(wbuf); }
static INLINE int wbuf_is_pos(wbuf_t wbuf) { return __wbuf_is_pos(wbuf); }
static INLINE void wbuf_set_pos(wbuf_t wbuf) { __wbuf_set_pos(wbuf); }
static INLINE void wbuf_clear_pos(wbuf_t wbuf) { __wbuf_clear_pos(wbuf); }
static INLINE int wbuf_is_vmf(wbuf_t wbuf) { return __wbuf_is_vmf(wbuf); }
static INLINE void wbuf_set_vmf(wbuf_t wbuf) { __wbuf_set_vmf(wbuf); }
static INLINE int wbuf_is_keepalive(wbuf_t wbuf) { return __wbuf_is_keepalive(wbuf); }
static INLINE void wbuf_set_keepalive(wbuf_t wbuf) { __wbuf_set_keepalive(wbuf); }
static INLINE void wbuf_set_cts_frame(wbuf_t wbuf) { __wbuf_set_cts_frame(wbuf); }
static INLINE int wbuf_get_cts_frame(wbuf_t wbuf) { return __wbuf_get_cts_frame(wbuf); }
#endif
static INLINE void wbuf_set_bsteering(wbuf_t wbuf) { __wbuf_set_bsteering(wbuf); }
static INLINE int  wbuf_get_bsteering(wbuf_t wbuf) { return __wbuf_get_bsteering(wbuf); }
static INLINE wbuf_t wbuf_next(wbuf_t wbuf) { return __wbuf_next(wbuf); }
static INLINE wbuf_t wbuf_next_buf(wbuf_t wbuf) { return __wbuf_next_buf(wbuf); }
static INLINE void  wbuf_set_next(wbuf_t wbuf, wbuf_t next) {  __wbuf_set_next(wbuf, next); }
static INLINE void  wbuf_setnextpkt(wbuf_t wbuf, wbuf_t next) {  __wbuf_setnextpkt(wbuf, next); }
static INLINE void  wbuf_set_age(wbuf_t wbuf, u_int32_t age) { __wbuf_set_age(wbuf,age); }
static INLINE u_int32_t wbuf_get_age(wbuf_t wbuf) { return __wbuf_get_age(wbuf); }
static INLINE wbuf_t wbuf_clone(osdev_t os_handle, wbuf_t wbuf) {
    wbuf_t wbuf1 = NULL;
    QDF_STATUS status = QDF_STATUS_SUCCESS;

    wbuf1 =  __wbuf_clone(os_handle, wbuf);
    if (wbuf1) {
        status = wbuf_alloc_mgmt_ctrl_block_alloc_copy(wbuf1, wbuf);
        if (QDF_IS_STATUS_ERROR(status)) {
            wbuf_free(wbuf1);
            wbuf1 = NULL;
        }
    }

    return wbuf1;
}
static INLINE wbuf_t wbuf_coalesce(osdev_t os_handle, wbuf_t wbuf) { return __wbuf_coalesce(os_handle, wbuf); }
static INLINE void wbuf_set_complete_handler(wbuf_t wbuf,void *handler, void *arg)
{
    __wbuf_set_complete_handler( wbuf,handler,arg);
}

static INLINE void wbuf_get_complete_handler(wbuf_t wbuf,void **phandler, void **parg)
{
    __wbuf_get_complete_handler(wbuf,phandler,parg);
}

#if defined(ATH_SUPPORT_VOWEXT) || defined(ATH_SUPPORT_IQUE) ||  UMAC_SUPPORT_NAWDS != 0
static INLINE u_int8_t wbuf_get_firstxmit(wbuf_t __wbuf) { return __wbuf_get_firstxmit(__wbuf); }
static INLINE u_int32_t wbuf_get_firstxmitts(wbuf_t __wbuf) { return __wbuf_get_firstxmitts(__wbuf); }

static INLINE void wbuf_set_firstxmit(wbuf_t __wbuf, int __val) { __wbuf_set_firstxmit(__wbuf, __val); }
static INLINE void wbuf_set_firstxmitts(wbuf_t __wbuf, u_int32_t __val) { __wbuf_set_firstxmitts(__wbuf, __val); }
static INLINE void wbuf_clear_flags(wbuf_t __wbuf) { __wbuf_clear_flags(__wbuf); }
#endif

#if defined(ATH_SUPPORT_IQUE)
static INLINE void wbuf_set_qin_timestamp(wbuf_t __wbuf, u_int32_t __val) { __wbuf_set_qin_timestamp(__wbuf, __val); }
static INLINE u_int32_t wbuf_get_qin_timestamp(wbuf_t __wbuf) { return __wbuf_get_qin_timestamp(__wbuf); }
#endif

#if ATH_SUPPORT_WIFIPOS
static INLINE void wbuf_set_netlink_pid(wbuf_t __wbuf, u_int32_t __val) { __wbuf_set_netlink_pid(__wbuf, __val); }
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
static INLINE void wbuf_set_netlink_dst_pid(wbuf_t __wbuf, u_int32_t __val) { __wbuf_set_netlink_dst_pid(__wbuf, __val); }
#endif
static INLINE void wbuf_set_netlink_dst_group(wbuf_t __wbuf, u_int32_t __val) { __wbuf_set_netlink_dst_group(__wbuf, __val); }
#endif
static INLINE void wbuf_set_netlink_acfg_flags(wbuf_t __wbuf, u_int8_t __val) { __wbuf_set_netlink_acfg_flags(__wbuf, __val); }
static INLINE u_int8_t wbuf_get_netlink_acfg_flags(wbuf_t __wbuf) { return __wbuf_get_netlink_acfg_flags(__wbuf); }
static INLINE void wbuf_set_netlink_acfg_pid(wbuf_t __wbuf, u_int32_t __val) { __wbuf_set_netlink_acfg_pid(__wbuf, __val); }
static INLINE u_int32_t wbuf_get_netlink_acfg_pid(wbuf_t __wbuf) { return __wbuf_get_netlink_acfg_pid(__wbuf); }

#if UNIFIED_SMARTANTENNA
static INLINE int wbuf_is_smart_ant_train_packet(wbuf_t wbuf) { return __wbuf_is_smart_ant_train_packet(wbuf); }
static INLINE int wbuf_smart_ant_set_train_packet(wbuf_t wbuf) { return __wbuf_smart_ant_set_train_packet(wbuf); }
static INLINE int wbuf_smart_ant_unset_train_packet(wbuf_t wbuf) { return __wbuf_smart_ant_unset_train_packet(wbuf); }
#else
static INLINE int wbuf_is_smart_ant_train_packet(wbuf_t wbuf) { return 0; }
#endif

static INLINE void wbuf_set_rx_info(wbuf_t wbuf, void * info, u_int32_t size) { __wbuf_set_rx_info(wbuf, info, size); }

static INLINE void wbuf_set_tx_ctrl(wbuf_t wbuf, uint8_t retry, uint8_t power, uint8_t chain) {__wbuf_set_tx_ctrl(wbuf, retry, power, chain); }

static INLINE void wbuf_set_tx_rate(wbuf_t wbuf, uint8_t nss, uint8_t preamble, uint8_t mcs, bool he_target) {__wbuf_set_tx_rate(wbuf, nss, preamble, mcs, he_target); }

static INLINE void wbuf_get_tx_ctrl(wbuf_t wbuf, uint8_t *retry, uint8_t *power, uint8_t *chain) {__wbuf_get_tx_ctrl(wbuf, retry, power, chain); }

static INLINE void wbuf_get_tx_rate(wbuf_t wbuf, uint32_t *rate) {__wbuf_get_tx_rate(wbuf, rate); }

static INLINE void wbuf_set_pid(wbuf_t wbuf, u_int32_t pid) { __wbuf_set_pid(wbuf, pid); }

static INLINE void wbuf_get_pid(wbuf_t wbuf, u_int32_t *pid) { __wbuf_get_pid(wbuf, pid); }
#endif //_WBUF_H
