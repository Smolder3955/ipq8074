/*
 * Copyright (c) 2015,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef __RX_DESC_UNIFIED_H
#define __RX_DESC_UNIFIED_H

#if UNIFIED_SMARTANTENNA
#include <wlan_sa_api_utils_defs.h>
#endif

/* #include <qdf_crypto.h> */
#include "reg_struct.h"
#include "regtable.h"
#include "if_meta_hdr.h"

#define AR_RX_DESC_FIELD_FUNCS(target, field) \
static u_int32_t _##target##_rx_desc_offsetof_##field(void) \
{ \
    return offsetof(struct rx_desc_base, field); \
} \
static u_int32_t _##target##_rx_desc_sizeof_##field(void) \
{ \
    return sizeof(struct rx_##field); \
}

#define FN_RX_DESC_OFFSETOF_FW_DESC(target) \
static u_int32_t _##target##_rx_desc_offsetof_fw_desc(void) \
{ \
    return offsetof(struct rx_desc_base, fw_desc); \
}

#define FN_RX_DESC_OFFSETOF_HDR_STATUS(target) \
static u_int32_t _##target##_rx_desc_offsetof_hdr_status(void) \
{ \
    return offsetof(struct rx_desc_base, rx_hdr_status); \
}

#define FN_RX_DESC_SIZE(target) \
static u_int32_t _##target##_rx_desc_size(void) \
{ \
    return sizeof(struct rx_desc_base); \
}

#define FN_RX_DESC_FW_DESC_SIZE(target) \
static ssize_t _##target##_fw_rx_desc_size(void) \
{ \
    return sizeof(struct fw_rx_desc); \
}

#define FN_RX_DESC_INIT(target) \
static void _##target##_rx_desc_init(void *rx_desc) \
{ \
    /* Clear rx_desc attention word before posting to Rx ring */ \
    struct rx_desc_base *desc = \
        (struct rx_desc_base *)rx_desc; \
    *(u_int32_t *)&desc->attention = 0; \
    *(((u_int32_t *)&desc->msdu_end) + 4) = 0; \
}

#define FN_RX_DESC_MPDU_WIFI_HDR_RETRIEVE(target) \
static char* _##target##_rx_desc_mpdu_wifi_hdr_retrieve(void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
        (struct rx_desc_base *) rx_desc; \
    return desc->rx_hdr_status; \
}

#define FN_RX_DESC_MPDU_DESC_SEQ_NUM(target) \
static u_int32_t _##target##_rx_desc_mpdu_desc_seq_num(ar_handle_t arh, \
        void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
        (struct rx_desc_base *) rx_desc; \
    int seq_num =\
            ((*((u_int32_t *) &desc->mpdu_start)) &\
             RX_MPDU_START_0_SEQ_NUM_MASK) >>\
             RX_MPDU_START_0_SEQ_NUM_LSB;\
    return seq_num; \
}

#define FN_RX_DESC_MPDU_DESC_PN(target) \
static void _##target##_rx_desc_mpdu_desc_pn(void *rx_desc, \
        u_int32_t *pn, \
        int pn_len_bits) \
{ \
    struct rx_desc_base *desc = \
        (struct rx_desc_base *) rx_desc; \
    switch (pn_len_bits) { \
        case 24: \
            /* bits 23:0 */ \
            pn[0] = \
            ((*((((u_int32_t *) &desc->mpdu_start) + 1)) &\
             RX_MPDU_START_1_PN_31_0_MASK) >>\
             RX_MPDU_START_1_PN_31_0_LSB);\
            break; \
        case 48: \
            /* bits 31:0 */ \
            pn[0] = \
            ((*((((u_int32_t *) &desc->mpdu_start) + 1)) &\
             RX_MPDU_START_1_PN_31_0_MASK) >>\
             RX_MPDU_START_1_PN_31_0_LSB) ;\
            /* bits 47:32 */ \
            pn[1] = \
            ((*((((u_int32_t *) &desc->mpdu_start) + 2)) &\
             RX_MPDU_START_2_PN_47_32_MASK) >>\
             RX_MPDU_START_2_PN_47_32_LSB) ;\
            break; \
        case 128: \
            /* bits 31:0 */ \
            pn[0] = \
            ((*((((u_int32_t *) &desc->mpdu_start) + 1)) &\
             RX_MPDU_START_1_PN_31_0_MASK) >>\
             RX_MPDU_START_1_PN_31_0_LSB) ;\
            /* bits 47:32 */ \
            pn[1] = \
            ((*((((u_int32_t *) &desc->mpdu_start) + 2)) &\
             RX_MPDU_START_2_PN_47_32_MASK) >>\
             RX_MPDU_START_2_PN_47_32_LSB) ;\
            /* bits 63:48 */ \
            pn[1] |= \
            ((*((((u_int32_t *) &desc->msdu_end) + 1)) &\
              RX_MSDU_END_1_EXT_WAPI_PN_63_48_MASK) >>\
             RX_MSDU_END_1_EXT_WAPI_PN_63_48_LSB) << 16;\
            /* bits 95:64 */ \
            pn[2] = \
            ((*((((u_int32_t *) &desc->msdu_end) + 2)) &\
              RX_MSDU_END_2_EXT_WAPI_PN_95_64_MASK) >>\
             RX_MSDU_END_2_EXT_WAPI_PN_95_64_LSB);\
            /* bits 127:96 */ \
            pn[3] = \
            ((*((((u_int32_t *) &desc->msdu_end) + 3)) &\
                RX_MSDU_END_3_EXT_WAPI_PN_127_96_MASK) >>\
             RX_MSDU_END_3_EXT_WAPI_PN_127_96_LSB);\
            break; \
        default: \
            qdf_print( \
                    "Error: invalid length spec (%d bits) for PN", pn_len_bits); \
    }; \
}

#define FN_RX_DESC_MPDU_DESC_FRDS(target) \
static u_int32_t _##target##_rx_desc_mpdu_desc_frds(void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
        (struct rx_desc_base *) rx_desc; \
    int fr_ds =\
            ((*((u_int32_t *) &desc->mpdu_start)) &\
             RX_MPDU_START_0_FR_DS_MASK) >>\
             RX_MPDU_START_0_FR_DS_LSB;\
    return fr_ds; \
}

#define FN_RX_DESC_MPDU_DESC_TODS(target) \
static u_int32_t _##target##_rx_desc_mpdu_desc_tods(void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
        (struct rx_desc_base *) rx_desc; \
    int to_ds =\
            ((*((u_int32_t *) &desc->mpdu_start)) &\
             RX_MPDU_START_0_TO_DS_MASK) >>\
             RX_MPDU_START_0_TO_DS_LSB;\
    return to_ds; \
}

#define FN_RX_DESC_MPDU_IS_ENCRYPTED(target) \
static u_int32_t _##target##_rx_desc_mpdu_is_encrypted(ar_handle_t arh, \
        void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
        (struct rx_desc_base *) rx_desc; \
    int encrypted =\
            ((*((u_int32_t *) &desc->mpdu_start)) &\
             RX_MPDU_START_0_ENCRYPTED_MASK) >>\
             RX_MPDU_START_0_ENCRYPTED_LSB;\
    return encrypted; \
}

#define FN_RX_DESC_GET_ATTN_WORD(target) \
static u_int32_t _##target##_rx_desc_get_attn_word(void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
                (struct rx_desc_base *) rx_desc; \
    return *(u_int32_t *)&desc->attention; \
}

#define FN_RX_DESC_GET_PPDU_START(target) \
static u_int32_t* _##target##_rx_desc_get_ppdu_start(void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
                (struct rx_desc_base *) rx_desc; \
    return (u_int32_t *)&desc->ppdu_start; \
}

#define FN_RX_DESC_ATTN_MSDU_DONE(target) \
static u_int32_t _##target##_rx_desc_attn_msdu_done(void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
                (struct rx_desc_base *) rx_desc; \
    return !!(((*(u_int32_t *)&desc->attention) & \
                             RX_ATTENTION_0_MSDU_DONE_MASK) >> RX_ATTENTION_0_MSDU_DONE_LSB); \
}

#define FN_RX_DESC_MSDU_LENGTH(target) \
u_int32_t _##target##_rx_desc_msdu_length(void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
                (struct rx_desc_base *) rx_desc; \
    int msdu_length =\
            ((*((u_int32_t *) &desc->msdu_start)) &\
             RX_MSDU_START_0_MSDU_LENGTH_MASK) >>\
             RX_MSDU_START_0_MSDU_LENGTH_LSB;\
    return msdu_length; \
}

#define FN_RX_DESC_MSDU_DESC_COMPLETES_MPDU(target) \
static u_int32_t _##target##_rx_desc_msdu_desc_completes_mpdu(ar_handle_t arh, \
        void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
                (struct rx_desc_base *) rx_desc; \
    int last_msdu =\
            ((*(((u_int32_t *) &desc->msdu_end) + 4)) &\
             RX_MSDU_END_4_LAST_MSDU_MASK) >>\
             RX_MSDU_END_4_LAST_MSDU_LSB;\
    return last_msdu; \
}

#define FN_RX_DESC_MSDU_HAS_WLAN_MCAST_FLAG(target) \
static u_int32_t _##target##_rx_desc_msdu_has_wlan_mcast_flag(ar_handle_t arh, \
        void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
        (struct rx_desc_base *) rx_desc; \
    int first_msdu =\
            ((*(((u_int32_t *) &desc->msdu_end) + 4)) &\
             RX_MSDU_END_4_FIRST_MSDU_MASK) >>\
             RX_MSDU_END_4_FIRST_MSDU_LSB;\
    return first_msdu; \
}

#define FN_RX_DESC_MSDU_IS_WLAN_MCAST(target) \
static u_int32_t _##target##_rx_desc_msdu_is_wlan_mcast(ar_handle_t arh, \
        void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
        (struct rx_desc_base *) rx_desc; \
    int mcast_bcast =\
            ((*((u_int32_t *) &desc->attention)) &\
             RX_ATTENTION_0_MCAST_BCAST_MASK) >>\
             RX_ATTENTION_0_MCAST_BCAST_LSB;\
    return mcast_bcast; \
}

#define FN_RX_DESC_MSDU_IS_FRAG(target) \
static u_int32_t _##target##_rx_desc_msdu_is_frag(ar_handle_t arh, \
        void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
             (struct rx_desc_base *) rx_desc; \
    int fragment =\
            ((*((u_int32_t *) &desc->attention)) &\
             RX_ATTENTION_0_FRAGMENT_MASK) >>\
             RX_ATTENTION_0_FRAGMENT_LSB;\
     return fragment; \
}

#define FN_RX_DESC_MSDU_FIRST_MSDU_FLAG(target) \
static u_int32_t _##target##_rx_desc_msdu_first_msdu_flag(ar_handle_t arh, \
        void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
        (struct rx_desc_base *) rx_desc; \
    int first_msdu =\
            ((*(((u_int32_t *) &desc->msdu_end) + 4)) &\
             RX_MSDU_END_4_FIRST_MSDU_MASK) >>\
             RX_MSDU_END_4_FIRST_MSDU_LSB;\
    return first_msdu; \
}

#define FN_RX_DESC_MSDU_KEY_ID_OCTET(target) \
static u_int32_t _##target##_rx_desc_msdu_key_id_octet(ar_handle_t arh, \
        void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
        (struct rx_desc_base *) rx_desc; \
    u_int32_t key_id_octet =\
            ((*(((u_int32_t *) &desc->msdu_end) + 1)) &\
             RX_MSDU_END_1_KEY_ID_OCTET_MASK) >>\
             RX_MSDU_END_1_KEY_ID_OCTET_LSB;\
    return key_id_octet; \
}

static inline void unified_rx_desc_set_checksum_result(
        void *rx_desc,
        qdf_nbuf_t msdu)
{
#define MAX_IP_VER          2
#define MAX_PROTO_VAL       4
    struct rx_desc_base *desc = rx_desc;
    struct rx_msdu_start *rx_msdu = &desc->msdu_start;
    unsigned int proto = (rx_msdu->tcp_proto) | (rx_msdu->udp_proto << 1);
     /*  HW supports TCP & UDP checksum offload for ipv4 and ipv6 */
    static const qdf_nbuf_l4_rx_cksum_type_t
        cksum_table[][MAX_PROTO_VAL][MAX_IP_VER] =
        {
            {
                /* non-fragmented IP packet */
                /* non TCP/UDP packet */
                { QDF_NBUF_RX_CKSUM_ZERO, QDF_NBUF_RX_CKSUM_ZERO },
                /* TCP packet */
                { QDF_NBUF_RX_CKSUM_TCP,  QDF_NBUF_RX_CKSUM_TCPIPV6},
                /* UDP packet */
                { QDF_NBUF_RX_CKSUM_UDP,  QDF_NBUF_RX_CKSUM_UDPIPV6 },
                /* invalid packet type */
                { QDF_NBUF_RX_CKSUM_ZERO, QDF_NBUF_RX_CKSUM_ZERO },
            },
            {
                /* fragmented IP packet */
                { QDF_NBUF_RX_CKSUM_ZERO, QDF_NBUF_RX_CKSUM_ZERO },
                { QDF_NBUF_RX_CKSUM_ZERO, QDF_NBUF_RX_CKSUM_ZERO },
                { QDF_NBUF_RX_CKSUM_ZERO, QDF_NBUF_RX_CKSUM_ZERO },
                { QDF_NBUF_RX_CKSUM_ZERO, QDF_NBUF_RX_CKSUM_ZERO },
            }
        };
    qdf_nbuf_rx_cksum_t cksum = {
        cksum_table[rx_msdu->ip_frag][proto][rx_msdu->ipv6_proto],
        QDF_NBUF_RX_CKSUM_ZERO,
        0
    } ;
    if (cksum.l4_type != QDF_NBUF_RX_CKSUM_ZERO) {
        cksum.l4_result = ((*(u_int32_t *) &desc->attention) &
                RX_ATTENTION_0_TCP_UDP_CHKSUM_FAIL_MASK) ?
            QDF_NBUF_RX_CKSUM_ZERO :
            QDF_NBUF_RX_CKSUM_TCP_UDP_UNNECESSARY;
    }
    qdf_nbuf_set_rx_cksum(msdu, &cksum);
#undef MAX_IP_VER
#undef MAX_PROTO_VAL
}

#define FN_RX_DESC_SET_CHECKSUM_RESULT(target) \
static void _##target##_rx_desc_set_checksum_result(void *rx_desc, \
        qdf_nbuf_t msdu) \
{ \
    unified_rx_desc_set_checksum_result(rx_desc, \
            msdu); \
}

#define FN_RX_DESC_DUMP(target) \
static void _##target##_rx_desc_dump(void *rx_desc) \
{ \
    int i; \
    u_int32_t *buf = (u_int32_t *)rx_desc; \
    int len = sizeof(struct rx_desc_base)/4; \
    printk("++++++++++++++++++++++++++++++++++++++++++++\n"); \
    printk("Dumping %d words of rx_desc of the msdu \n", len); \
    for (i = 0; i < len; i++) { \
        printk("0x%08x ", *buf); \
        buf++; \
        if (i && (i % 4 == 0)) \
            printk("\n"); \
    } \
    printk("++++++++++++++++++++++++++++++++++++++++++++\n"); \
}

#define FN_RX_DESC_CHECK_DESC_MGMT_TYPE(target) \
static u_int32_t _##target##_rx_desc_check_desc_mgmt_type(qdf_nbuf_t head_msdu) \
{ \
    struct rx_desc_base *cbf_rx_desc; \
    qdf_nbuf_t next_msdu = head_msdu; \
    int        msdu_chained = 0; \
    uint32_t   flag_cbf_data = 0; \
    while(next_msdu) \
    { \
        cbf_rx_desc = ar_rx_desc(next_msdu); \
        msdu_chained = (((*(u_int32_t *) &cbf_rx_desc->frag_info) &\
                    RX_FRAG_INFO_0_RING2_MORE_COUNT_MASK) >>\
                RX_FRAG_INFO_0_RING2_MORE_COUNT_LSB);\
        if ((*(u_int32_t *) &cbf_rx_desc->attention) & \
                RX_ATTENTION_0_MGMT_TYPE_MASK) \
        { \
            flag_cbf_data = 1; \
            break; \
        }else \
            next_msdu = qdf_nbuf_next(next_msdu); \
        while((next_msdu) && (msdu_chained--)) \
        { \
            next_msdu = qdf_nbuf_next(next_msdu); \
        } \
    } \
    if(flag_cbf_data) \
        return 0; \
    else \
        return 1; \
}

#define FN_RX_DESC_CHECK_DESC_CTRL_TYPE(target) \
static u_int32_t _##target##_rx_desc_check_desc_ctrl_type(qdf_nbuf_t head_msdu) \
{ \
    struct rx_desc_base *cbf_rx_desc; \
    qdf_nbuf_t next_msdu = head_msdu; \
    int        msdu_chained = 0; \
    uint32_t   flag_cbf_data = 0; \
    while(next_msdu) \
    { \
        cbf_rx_desc = ar_rx_desc(next_msdu); \
        msdu_chained = (((*(u_int32_t *) &cbf_rx_desc->frag_info) &\
                    RX_FRAG_INFO_0_RING2_MORE_COUNT_MASK) >>\
                RX_FRAG_INFO_0_RING2_MORE_COUNT_LSB);\
        if ((*(u_int32_t *) &cbf_rx_desc->attention) & \
                RX_ATTENTION_0_CTRL_TYPE_MASK ) \
        { \
            flag_cbf_data = 1; \
            break; \
        }else \
            next_msdu = qdf_nbuf_next(next_msdu); \
        while((next_msdu) && (msdu_chained--)) \
        { \
            next_msdu = qdf_nbuf_next(next_msdu); \
        } \
    } \
    if(flag_cbf_data) \
        return 0; \
    else \
        return 1; \
}

#define FN_RX_DESC_MSDU_DESC_MSDU_CHAINED(target) \
static u_int32_t _##target##_msdu_desc_msdu_chained(void *rx_desc) \
{ \
    struct rx_desc_base *desc = \
        (struct rx_desc_base *) rx_desc; \
    int msdu_chained = (((*(u_int32_t *) &desc->frag_info) &\
                    RX_FRAG_INFO_0_RING2_MORE_COUNT_MASK) >>\
                RX_FRAG_INFO_0_RING2_MORE_COUNT_LSB);\
    return msdu_chained; \
}

#define FN_RX_DESC_MSDU_DESC_TSF_TIMESTAMP(target) \
static u_int32_t _##target##_msdu_desc_tsf_timestamp(void *rx_desc) \
{ \
    return ((struct rx_desc_base *)rx_desc)->ppdu_end.tsf_timestamp; \
}

static inline u_int32_t unified_rx_desc_amsdu_pop(ar_handle_t arh,
#if QCA_OL_SUPPORT_RAWMODE_TXRX
        void *vdev,
#endif
        qdf_nbuf_t rx_ind_msg,
        qdf_nbuf_t* head_msdu,
        qdf_nbuf_t* tail_msdu,
        u_int32_t* npackets)
{
    struct ar_s *ar = (struct ar_s *)arh;
    int msdu_len, msdu_chaining = 0;
    qdf_nbuf_t msdu;
    struct rx_desc_base *rx_desc;
    u_int8_t *rx_ind_data;
    u_int32_t num_msdu_bytes;
    u_int8_t pad_bytes = 0;
#if ATH_GEN_RANDOMNESS
    uint8_t  rssi_chain0_pri20, rssi_chain0_sec80;
#endif
#if QCA_OL_SUPPORT_RAWMODE_TXRX
    /* We have a corner case where vdev is NULL due to peer being unmappable. In
     * this case, we allow the default processing on the nbufs which anyway get
     * discarded.
     */
    u_int8_t use_raw_decap = ar->rx_callbacks->txrx_is_decap_type_raw(vdev);
#endif /* QCA_OL_SUPPORT_RAWMODE_TXRX */

    qdf_assert(ar->rx_callbacks->rx_ring_elems(ar->htt_handle) != 0);
    rx_ind_data = qdf_nbuf_data(rx_ind_msg);
    num_msdu_bytes = ar->rx_callbacks->rx_ind_fw_rx_desc_bytes_get(ar->htt_handle, rx_ind_msg);

#if RX_DEBUG
    msdu = *head_msdu = ar->rx_callbacks->rx_netbuf_peek(ar->htt_handle);
#else
    msdu = *head_msdu = ar->rx_callbacks->rx_netbuf_pop(ar->htt_handle);
#endif
    while (1) {
        int last_msdu, msdu_len_invalid, fcs_error, msdu_chained;
        int byte_offset;
        u_int8_t is_validfragchain = 0;
        int rx_buf_size = ar->rx_callbacks->rx_buf_size(ar->htt_handle);

        /*
         * Set the netbuf length to be the entire buffer length initially,
         * so the unmap will unmap the entire buffer.
         */
        qdf_nbuf_set_pktlen(msdu, rx_buf_size);

        qdf_nbuf_unmap_nbytes(ar->osdev, msdu, QDF_DMA_FROM_DEVICE, rx_buf_size);

        /* cache consistency has been taken care of by the qdf_nbuf_unmap */

        /*
         * Now read the rx descriptor.
         * Set the length to the appropriate value.
         * Check if this MSDU completes a MPDU.
         */

        rx_desc = ar_rx_desc(msdu);

#if RX_DEBUG

        msdu = ar->rx_callbacks->rx_debug(ar->htt_handle,
               rx_ind_msg,
               msdu,
               npackets,
               (((*(u_int32_t *) &rx_desc->attention) & RX_ATTENTION_0_MSDU_DONE_MASK) >> RX_ATTENTION_0_MSDU_DONE_LSB));
        if( msdu == NULL ) {
            return 0;
        }
        if (((*npackets) == 0) && (msdu != *head_msdu)) {
            /*
             * Update head_msdu in case if previous msdu had DONE BIT
             * not set and recovered in the next msdu.
             */
            *head_msdu = msdu;
        }
#else
        /*
         * Sanity check - confirm the HW is finished filling in the rx data.
         * If the HW and SW are working correctly, then it's guaranteed that
         * the HW's MAC DMA is done before this point in the SW.
         * To prevent the case that we handle a stale Rx descriptor, just
         * assert for now until we have a way to recover.
         */
        HTT_ASSERT_ALWAYS(
                (((*(u_int32_t *) &rx_desc->attention) &
                  RX_ATTENTION_0_MSDU_DONE_MASK) >> RX_ATTENTION_0_MSDU_DONE_LSB));
#endif

#if ATH_GEN_RANDOMNESS
        if (likely(rx_desc)) {
        rssi_chain0_pri20= (((*(u_int32_t *) &rx_desc->ppdu_start) &
                    RX_PPDU_START_0_RSSI_PRI_CHAIN0_MASK)   >> RX_PPDU_START_0_RSSI_PRI_CHAIN0_LSB);
        rssi_chain0_sec80= (((*(u_int32_t *) &rx_desc->ppdu_start) &
                    RX_PPDU_START_0_RSSI_SEC80_CHAIN0_MASK) >> RX_PPDU_START_0_RSSI_SEC80_CHAIN0_LSB);
	    /* ath_gen_randomness API present under ATH_GEN_RANDOMNESS flag
	     * Needs replacement with QDF API once implemented */
            //ath_gen_randomness(rssi_chain0_pri20 | rssi_chain0_sec80);
        }
#endif

        /*
         * Make the netbuf's data pointer point to the payload rather
         * than the descriptor.
         */
        pad_bytes = GET_L3_HEADER_PADDING(rx_desc);
        qdf_nbuf_pull_head(msdu,
            ar->rx_callbacks->rx_desc_reservation(ar->htt_handle) + pad_bytes);
        (*npackets)++;

        /*
         * Copy the FW rx descriptor for this MSDU from the rx indication
         * message into the MSDU's netbuf.
         * HL uses the same rx indication message definition as LL, and
         * simply appends new info (fields from the HW rx desc, and the
         * MSDU payload itself).
         * So, the offset into the rx indication message only has to account
         * for the standard offset of the per-MSDU FW rx desc info within
         * the message, and how many bytes of the per-MSDU FW rx desc info
         * have already been consumed.  (And the endianness of the host,
         * since for a big-endian host, the rx ind message contents,
         * including the per-MSDU rx desc bytes, were byteswapped during
         * upload.)
         */
        if(1 == ar->rx_callbacks->rx_ind_msdu_byte_offset(ar->htt_handle, num_msdu_bytes, &byte_offset))
        {
            *((u_int8_t *) &rx_desc->fw_desc.u.val) = rx_ind_data[byte_offset];
            /*
             * The target is expected to only provide the basic per-MSDU rx
             * descriptors.  Just to be sure, verify that the target has not
             * attached extension data (e.g. LRO flow ID).
             */
            /*
             * The assertion below currently doesn't work for RX_FRAG_IND
             * messages, since their format differs from the RX_IND format
             * (no FW rx PPDU desc in the current RX_FRAG_IND message).
             * If the RX_FRAG_IND message format is updated to match the
             * RX_IND message format, then the following assertion can be
             * restored.
             */
            //qdf_assert((rx_ind_data[byte_offset] & FW_RX_DESC_EXT_M) == 0);
            ar->rx_callbacks->rx_ind_msdu_byte_idx_inc(ar->htt_handle); // or more, if there's ext data
        } else {
            /*
             * When an oversized AMSDU happened, FW will lost some of
             * MSDU status - in this case, the FW descriptors provided
             * will be less than the actual MSDUs inside this MPDU.
             * Mark the FW descriptors so that it will still deliver to
             * upper stack, if no CRC error for this MPDU.
             *
             * FIX THIS - the FW descriptors are actually for MSDUs in
             * the end of this A-MSDU instead of the beginning.
             */
            *((u_int8_t *) &rx_desc->fw_desc.u.val) = 0;
        }
        if (OL_CFG_NONRAW_RX_LIKELINESS(!use_raw_decap))
        {
            /* Note: Since in Raw Mode the payload is encrypted in most cases and
             * we won't have the keys, VoW stats, TCP/UDP checksum offload, and
             * HW LRO support for TCP won't apply for now.
             */
            /*
             * VoW ext stats
             */
            ar->rx_callbacks->ath_add_vow_extstats(ar->htt_handle, msdu);
            /*
             * ARP DEBUG
             */

            ar->rx_callbacks->ath_arp_debug(ar->htt_handle, msdu);
            /*
             *  TCP/UDP checksum offload support
             */
            rx_desc_set_checksum_result(rx_desc, msdu);
            /*
             *  HW LRO support for TCP traffic
             */
            rx_desc_set_lro_info(rx_desc, msdu);
        }

        msdu_len_invalid = (*(u_int32_t *) &rx_desc->attention) &
            RX_ATTENTION_0_MPDU_LENGTH_ERR_MASK;
        msdu_chained = (((*(u_int32_t *) &rx_desc->frag_info) &
                    RX_FRAG_INFO_0_RING2_MORE_COUNT_MASK) >>
                RX_FRAG_INFO_0_RING2_MORE_COUNT_LSB);
        msdu_len =
            ((*((u_int32_t *) &rx_desc->msdu_start)) &
             RX_MSDU_START_0_MSDU_LENGTH_MASK) >>
            RX_MSDU_START_0_MSDU_LENGTH_LSB;

        do {
            if (!msdu_len_invalid && !msdu_chained) {
#if defined(PEREGRINE_1_0_ZERO_LEN_PHY_ERR_WAR)
                if (msdu_len > 0x3000) {
                    break;
                }
#endif
                qdf_nbuf_trim_tail(
                        msdu, rx_buf_size - (RX_STD_DESC_SIZE + msdu_len + pad_bytes));
            }
        } while (0);
        if (OL_CFG_RAW_RX_LIKELINESS(use_raw_decap) && msdu_chained) {
            fcs_error = (*(u_int32_t *) &rx_desc->attention) &
                RX_ATTENTION_0_FCS_ERR_MASK;

            /* Determine whether this is the first fragment of a valid sequence
             * of fragments. In case of an FCS error, we could get bogus
             * fragments and we should disregard them.
             * XXX - May not be necessary to check for FCS error here. To be
             * revisited. Kept as a safeguard currently.
             *
             * Note that we currently carry out fragment processing only for Raw
             * mode.
             */
            if (qdf_likely(!fcs_error)) {
                is_validfragchain = 1;
                qdf_nbuf_set_rx_chfrag_start(msdu, 1);
            }
        }

        while (msdu_chained--) {
            qdf_nbuf_t next =
                ar->rx_callbacks->rx_netbuf_pop(ar->htt_handle);
            qdf_nbuf_set_pktlen(next, rx_buf_size);
            qdf_nbuf_unmap_nbytes(ar->osdev, next, QDF_DMA_FROM_DEVICE, rx_buf_size);
            msdu_len -= rx_buf_size;
            qdf_nbuf_set_next(msdu, next);
            msdu = next;
            msdu_chaining = 1;

            if (msdu_chained == 0) {
                /* Trim the last one to the correct size - accounting for
                 * inconsistent HW lengths cuasing length overflows and
                 * underflows
                 */
                if (((unsigned)msdu_len) >
                        ((unsigned)(rx_buf_size - RX_STD_DESC_SIZE))) {
                    msdu_len = (rx_buf_size - RX_STD_DESC_SIZE);
                }

                qdf_nbuf_trim_tail(
                        next, rx_buf_size - (RX_STD_DESC_SIZE + msdu_len));

                /* Currently, is_validfragchain can be set only if Raw Mode is
                 * enabled. Hence, we assign it the same likeliness.
                 */
                if (OL_CFG_RAW_RX_LIKELINESS(is_validfragchain)) {
                    qdf_nbuf_set_rx_chfrag_end(next, 1);
                }
            }
        }

       last_msdu =
            ((*(((u_int32_t *) &rx_desc->msdu_end) + 4)) &
             RX_MSDU_END_4_LAST_MSDU_MASK) >>
             RX_MSDU_END_4_LAST_MSDU_LSB;

        if (last_msdu) {
            qdf_nbuf_set_next(msdu, NULL);
            break;
        } else {
            qdf_nbuf_t next =
                ar->rx_callbacks->rx_netbuf_pop(ar->htt_handle);
            qdf_nbuf_set_next(msdu, next);
            msdu = next;

        }
    }
    *tail_msdu = msdu;

    /*
     * Don't refill the ring yet.
     * First, the elements popped here are still in use - it is
     * not safe to overwrite them until the matching call to
     * mpdu_desc_list_next.
     * Second, for efficiency it is preferable to refill the rx ring
     * with 1 PPDU's worth of rx buffers (something like 32 x 3 buffers),
     * rather than one MPDU's worth of rx buffers (something like 3 buffers).
     * Consequently, we'll rely on the txrx SW to tell us when it is done
     * pulling all the PPDU's rx buffers out of the rx ring, and then
     * refill it just once.
     */
    return msdu_chaining;
}

#if QCA_OL_SUPPORT_RAWMODE_TXRX
#define FN_RX_DESC_AMSDU_POP(target) \
static u_int32_t _##target##_rx_desc_amsdu_pop(ar_handle_t arh, \
        void *vdev, \
        qdf_nbuf_t rx_ind_msg, \
        qdf_nbuf_t* head_msdu, \
        qdf_nbuf_t* tail_msdu, \
        u_int32_t* npackets) \
{ \
    return unified_rx_desc_amsdu_pop(arh, \
            vdev, \
            rx_ind_msg, \
            head_msdu, \
            tail_msdu, \
            npackets); \
}
#else
#define FN_RX_DESC_AMSDU_POP(target) \
static u_int32_t _##target##_rx_desc_amsdu_pop(ar_handle_t arh, \
        qdf_nbuf_t rx_ind_msg, \
        qdf_nbuf_t* head_msdu, \
        qdf_nbuf_t* tail_msdu, \
        u_int32_t* npackets) \
{ \
    return unified_rx_desc_amsdu_pop(arh, \
            rx_ind_msg, \
            head_msdu, \
            tail_msdu, \
            npackets); \
}
#endif //QCA_OL_SUPPORT_RAWMODE_TXRX

static inline void unified_rx_desc_parse_ppdu_start_status(void *rx_desc,
        struct ieee80211_rx_status *rs)
{
    struct rx_desc_base *desc = (struct rx_desc_base *)rx_desc;
    struct rx_ppdu_start *ppdu_start = &desc->ppdu_start;
    uint32_t preamble_type;
    /* RSSI */
    rs->rs_rssi = ((*((u_int32_t *)ppdu_start+4) &
                   RX_PPDU_START_4_RSSI_COMB_MASK) >> RX_PPDU_START_4_RSSI_COMB_LSB);
    /* PHY rate */
    /* rs_ratephy coding
       [b3 - b0]
       0 -> OFDM
       1 -> CCK
       2 -> HT
       3 -> VHT
       OFDM / CCK
       [b7  - b4 ] => LSIG rate
       [b23 - b8 ] => service field (b'12 static/dynamic, b'14..b'13 BW for VHT)
       [b31 - b24 ] => Reserved
       HT / VHT
       [b15 - b4 ] => SIG A_2 12 LSBs
       [b31 - b16] => SIG A_1 16 LSBs

       HT / VHT MU
       [b27 - b4 ] => SIG A_1 24 LSBs

       rs_ratephy2
       [b23 - b0 ] => SIG_A_2 24 LSBs

       rs_ratephy3
       [b28 - b0 ] => SIG_B   29 LSBs
     */
    preamble_type = ((*((u_int32_t *)ppdu_start+5) &
                     RX_PPDU_START_5_PREAMBLE_TYPE_MASK) >> RX_PPDU_START_5_PREAMBLE_TYPE_LSB);
    if (preamble_type == 0x4 ) {
        uint32_t l_sig_rate_select;
        uint32_t l_sig_rate;
        uint32_t service;

        l_sig_rate_select = ((*((u_int32_t *)ppdu_start+5) &
                            RX_PPDU_START_5_L_SIG_RATE_SELECT_MASK) >> RX_PPDU_START_5_L_SIG_RATE_SELECT_LSB);
        l_sig_rate = ((*((u_int32_t *)ppdu_start+5) &
                     RX_PPDU_START_5_L_SIG_RATE_MASK) >> RX_PPDU_START_5_L_SIG_RATE_LSB);
        service = ((*((u_int32_t *)ppdu_start+9) &
                    RX_PPDU_START_9_SERVICE_MASK) >> RX_PPDU_START_9_SERVICE_LSB);
        rs->rs_ratephy1 = l_sig_rate_select;
        rs->rs_ratephy1 |= l_sig_rate << 4;
        rs->rs_ratephy1 |= service << 8;
    }  else {
        uint32_t ht_sig_vht_sig_a_1;
        uint32_t ht_sig_vht_sig_a_2;
        uint32_t vht_sig_b;
        /*        rs->rs_ratephy =
                  (ppdu_start->preamble_type & 0x4) ? 3 : 2;
                  rs->rs_ratephy |=
                  (ppdu_start->ht_sig_vht_sig_a_2 & 0xFFF) << 4;
                  rs->rs_ratephy |=
                  (ppdu_start->ht_sig_vht_sig_a_1 & 0xFFFF) << 16; */

        ht_sig_vht_sig_a_1 = ((*((u_int32_t *)ppdu_start+6) &
                        RX_PPDU_START_6_HT_SIG_VHT_SIG_A_1_MASK) >> RX_PPDU_START_6_HT_SIG_VHT_SIG_A_1_LSB);
        ht_sig_vht_sig_a_2 = ((*((u_int32_t *)ppdu_start+7) &
                        RX_PPDU_START_7_HT_SIG_VHT_SIG_A_2_MASK) >> RX_PPDU_START_7_HT_SIG_VHT_SIG_A_2_LSB);
        vht_sig_b = ((*((u_int32_t *)ppdu_start+8) &
                        RX_PPDU_START_8_VHT_SIG_B_MASK) >> RX_PPDU_START_8_VHT_SIG_B_LSB);

        rs->rs_ratephy1 =
            (preamble_type & 0x4) ? 3 : 2;

        rs->rs_ratephy1 |=
            (ht_sig_vht_sig_a_1 & 0xFFFFFF) << 4;

        rs->rs_ratephy2 = ht_sig_vht_sig_a_2 & 0xFFFFFF;
        rs->rs_ratephy3 = vht_sig_b & 0x1FFFFFFF;

    }
    rs->rs_lsig_word = *((u_int32_t *)ppdu_start+5);
    return;
}

#define FN_RX_DESC_PARSE_PPDU_START_STATUS(target) \
static void _##target##_rx_desc_parse_ppdu_start_status(void *rx_desc, \
        struct ieee80211_rx_status *rs) \
{ \
    unified_rx_desc_parse_ppdu_start_status(rx_desc, \
            rs); \
}

enum {
    HW_RX_DECAP_FORMAT_RAW = 0,
    HW_RX_DECAP_FORMAT_NWIFI,
    HW_RX_DECAP_FORMAT_8023,
    HW_RX_DECAP_FORMAT_ETH2,
};

#define HTT_FCS_LEN (4)
#define KEY_EXTIV    0x20

static inline qdf_nbuf_t unified_rx_desc_restitch_mpdu_from_msdus(ar_handle_t arh,
        qdf_nbuf_t head_msdu,
        struct ieee80211_rx_status *rx_status,
        unsigned clone_not_reqd)
{
    struct ar_s *ar = (struct ar_s *)arh;
    qdf_nbuf_t msdu, mpdu_buf, prev_buf, msdu_orig, head_frag_list_cloned;
#ifndef NBUF_MEMORY_DEBUG
    qdf_nbuf_t (*clone_nbuf_fn)(qdf_nbuf_t buf);
#else
    qdf_nbuf_t (*clone_nbuf_fn)(qdf_nbuf_t buf, const char *func_name, uint32_t line_num);
#endif
    unsigned decap_format, wifi_hdr_len, sec_hdr_len, msdu_llc_len,
             mpdu_buf_len, decap_hdr_pull_bytes, frag_list_sum_len, dir,
             is_amsdu, is_first_frag, amsdu_pad;
    struct rx_desc_base *rx_desc;
    char *hdr_desc;
    unsigned char *dest;
    struct ieee80211_frame *wh;
    struct ieee80211_qoscntl*qos;
    int last_mpdu, first_mpdu;
    qdf_nbuf_t amsdu_llc_buf;
    uint8_t rx_phy_data_filter;
    head_frag_list_cloned = NULL;

    /* If this packet does not go up the normal stack path we dont need to
     * waste cycles cloning the packets
     */
#ifndef NBUF_MEMORY_DEBUG
    clone_nbuf_fn =
        clone_not_reqd ? rx_adf_noclone_buf : qdf_nbuf_clone;
#else
    clone_nbuf_fn =
        clone_not_reqd ? rx_adf_noclone_buf : qdf_nbuf_clone_debug;
#endif


    /* The nbuf has been pulled just beyond the status and points to the
     * payload
     */
    msdu_orig = head_msdu;
    rx_desc = ar_rx_desc(msdu_orig);


    /* Drop packets if it is mpdu Length Error */
    if (((*(u_int32_t *) &rx_desc->attention) & RX_ATTENTION_0_MPDU_LENGTH_ERR_MASK) >> RX_ATTENTION_0_MPDU_LENGTH_ERR_LSB) {
        return NULL;
    }

    /* Drop packets if it is radar error */
    ar->ar_rx_ops->check_desc_phy_data_type(head_msdu, &rx_phy_data_filter);

    if(rx_phy_data_filter & RX_PHY_DATA_RADAR) {
        return NULL;
    }

    rx_status->rs_fcs_error = (((*(u_int32_t *) &rx_desc->attention) &
                              RX_ATTENTION_0_FCS_ERR_MASK) >> RX_ATTENTION_0_FCS_ERR_LSB);

    /* Fill out the rx_status from the PPDU start and end fields */
    first_mpdu =
            (*(((u_int32_t *) &rx_desc->attention)) &
             RX_ATTENTION_0_FIRST_MPDU_MASK) >>
             RX_ATTENTION_0_FIRST_MPDU_LSB;
    if (first_mpdu) {
        rx_desc_parse_ppdu_start_status(rx_desc, rx_status);

        /* The timestamp is no longer valid - It will be valid only for the
         * last MPDU
         */
        rx_status->rs_tstamp.tsf = ~0;
    }

     decap_format = ((*((u_int32_t *) &rx_desc->msdu_start+2) &
                          RX_MSDU_START_2_DECAP_FORMAT_MASK) >>
                          RX_MSDU_START_2_DECAP_FORMAT_LSB);

    /* Easy case - The MSDU status indicates that this is a non-decapped
     * packet in RAW mode.
     * return
     */
    if (decap_format == HW_RX_DECAP_FORMAT_RAW) {
        /* Note that this path might suffer from headroom unavailabilty -
         * but the RX status is usually enough
         */
#ifndef NBUF_MEMORY_DEBUG
        mpdu_buf = clone_nbuf_fn(head_msdu);
#else
        mpdu_buf = clone_nbuf_fn(head_msdu, __FILE__, __LINE__);
#endif
        if (!mpdu_buf) {
            goto mpdu_stitch_fail;
        }

        prev_buf = mpdu_buf;

        frag_list_sum_len = 0;
        msdu_orig = qdf_nbuf_next(head_msdu);
        is_first_frag = 1;

        while (msdu_orig) {

            /* TODO: intra AMSDU padding - do we need it ??? */
#ifndef NBUF_MEMORY_DEBUG
            msdu = clone_nbuf_fn(msdu_orig);
#else
            msdu = clone_nbuf_fn(msdu_orig, __FILE__, __LINE__);
#endif
            if (!msdu) {
                goto mpdu_stitch_fail;
            }

            if (is_first_frag) {
                is_first_frag = 0;
                head_frag_list_cloned  = msdu;
            }

            frag_list_sum_len += qdf_nbuf_len(msdu);

            /* Maintain the linking of the cloned MSDUS */
            qdf_nbuf_set_next_ext(prev_buf, msdu);

            /* Move to the next */
            prev_buf = msdu;
            msdu_orig = qdf_nbuf_next(msdu_orig);
        }

        qdf_nbuf_trim_tail(prev_buf, HTT_FCS_LEN);

        /* If there were more fragments to this RAW frame */
        if (head_frag_list_cloned) {
            frag_list_sum_len -= HTT_FCS_LEN;
            qdf_nbuf_append_ext_list(mpdu_buf, head_frag_list_cloned,
                    frag_list_sum_len);
        }

        goto mpdu_stitch_done;
    }

    /* Decap mode:
     * Calculate the amount of header in decapped packet to knock off based
     * on the decap type and the corresponding number of raw bytes to copy
     * status header
     */

    hdr_desc = &rx_desc->rx_hdr_status[0];

    /* Base size */
    wifi_hdr_len = sizeof(struct ieee80211_frame);
    wh = (struct ieee80211_frame*)hdr_desc;

    dir = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;
    if (dir == IEEE80211_FC1_DIR_DSTODS) {
        wifi_hdr_len += 6;
    }

    is_amsdu = 0;
    if (wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_QOS) {
        qos = (struct ieee80211_qoscntl*)
            (hdr_desc + wifi_hdr_len);
        wifi_hdr_len += 2;

        is_amsdu = (qos->i_qos[0] & IEEE80211_QOS_AMSDU);
    }

    /* Calculate security header length based on 'Protected' and 'EXT_IV' flag */
	if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
		char *iv = (char *)wh + wifi_hdr_len;

		if (iv[3] & KEY_EXTIV) {
			sec_hdr_len = 8;
		} else {
			sec_hdr_len = 4;
		}
	} else {
		sec_hdr_len = 0;
	}
	wifi_hdr_len += sec_hdr_len;

    /* MSDU related stuff LLC - AMSDU subframe header etc */
    msdu_llc_len = is_amsdu ? (14 + 8) : 8;

    mpdu_buf_len = wifi_hdr_len + msdu_llc_len;

    /* "Decap" header to remove from MSDU buffer */
    decap_hdr_pull_bytes = 14;

    /* Allocate a new nbuf for holding the 802.11 header retrieved from the
     * status of the now decapped first msdu. Leave enough headroom for
     * accomodating any radio-tap /prism like PHY header
     */
#define MAX_MONITOR_HEADER (512)
    mpdu_buf = qdf_nbuf_alloc(ar->osdev,
            MAX_MONITOR_HEADER + mpdu_buf_len,
            MAX_MONITOR_HEADER, 4, FALSE);

    if (!mpdu_buf) {
        goto mpdu_stitch_done;
    }

    /* Copy the MPDU related header and enc headers into the first buffer
     * - Note that there can be a 2 byte pad between heaader and enc header
     */

    prev_buf = mpdu_buf;
    dest = qdf_nbuf_put_tail(prev_buf, wifi_hdr_len);
    if (!dest) {
        prev_buf = mpdu_buf = NULL;
        goto mpdu_stitch_done;
    }
    qdf_mem_copy(dest, hdr_desc, wifi_hdr_len);
    hdr_desc += wifi_hdr_len;

#if 0
    dest = qdf_nbuf_put_tail(prev_buf, sec_hdr_len);
    qdf_mem_copy(dest, hdr_desc, sec_hdr_len);
    hdr_desc += sec_hdr_len;
#endif

    /* The first LLC len is copied into the MPDU buffer */
    frag_list_sum_len = 0;
    frag_list_sum_len -= msdu_llc_len;

    msdu_orig = head_msdu;
    is_first_frag = 1;
    amsdu_pad = 0;

    while (msdu_orig) {

        /* TODO: intra AMSDU padding - do we need it ??? */

#ifndef NBUF_MEMORY_DEBUG
        msdu = clone_nbuf_fn(msdu_orig);
#else
        msdu = clone_nbuf_fn(msdu_orig, __FILE__, __LINE__);
#endif
        if (!msdu) {
            goto mpdu_stitch_fail;
        }

        if (is_first_frag) {
            head_frag_list_cloned  = msdu;
        } else {

            /* Reload the hdr ptr only on non-first MSDUs */
            rx_desc = ar_rx_desc(msdu_orig);
            hdr_desc = &rx_desc->rx_hdr_status[0];
        }

        /* Copy this buffers MSDU related status into the prev buffer */

        if(is_first_frag)
        {
                is_first_frag = 0;
                dest = qdf_nbuf_put_tail(prev_buf, msdu_llc_len + amsdu_pad);
                if (!dest) {
                    mpdu_buf = NULL;
                    qdf_nbuf_free(msdu);
                    goto mpdu_stitch_done;
                }

                dest += amsdu_pad;
                qdf_mem_copy(dest, hdr_desc, msdu_llc_len);

        } else {

                amsdu_llc_buf = qdf_nbuf_alloc(ar->osdev,
                    32 + 32,
                    32, 4, FALSE);

                if (!amsdu_llc_buf) {
                    goto mpdu_stitch_fail;
                }

                dest = qdf_nbuf_put_tail(amsdu_llc_buf, msdu_llc_len + amsdu_pad);
                if (!dest) {
                    goto mpdu_stitch_fail;
                }
                dest += amsdu_pad;
                qdf_mem_copy(dest, hdr_desc, msdu_llc_len);

                /* Maintain the linking of the MSDU header and cloned MSDUS */

                qdf_nbuf_set_next_ext(prev_buf, amsdu_llc_buf);
                prev_buf = amsdu_llc_buf;

                qdf_nbuf_set_next_ext(prev_buf, msdu);

	}

	/* Push the MSDU buffer beyond the decap header */
        qdf_nbuf_pull_head(msdu, decap_hdr_pull_bytes);
        frag_list_sum_len += msdu_llc_len + qdf_nbuf_len(msdu) + amsdu_pad;

        /* Set up intra-AMSDU pad to be added to start of next buffer -
         * AMSDU pad is 4 byte pad on AMSDU subframe */
        amsdu_pad = (msdu_llc_len + qdf_nbuf_len(msdu)) & 0x3;
        amsdu_pad = amsdu_pad ? ( 4 - amsdu_pad) : 0;

        /* TODO FIXME How do we handle MSDUs that have fraglist - Should
         * probably iterate all the frags cloning them along the way and
         * and also updating the prev_buf pointer
         */


        /* Move to the next */
        prev_buf = msdu;
        msdu_orig = qdf_nbuf_next(msdu_orig);

    }

    /* Last MSDU rx descriptor */
    if((((*(u_int32_t *) &rx_desc->attention) & RX_ATTENTION_0_FCS_ERR_MASK) >> RX_ATTENTION_0_FCS_ERR_LSB)
         || (((*(u_int32_t *) &rx_desc->attention) & RX_ATTENTION_0_MPDU_LENGTH_ERR_MASK) >> RX_ATTENTION_0_MPDU_LENGTH_ERR_LSB)) {
        rx_status->rs_fcs_error = 1;
    }
    else {
        rx_status->rs_fcs_error = 0;
    }

#if 0
    /* Add in the trailer section - encryption trailer + FCS */
    qdf_nbuf_put_tail(prev_buf, HTT_FCS_LEN);
    frag_list_sum_len += HTT_FCS_LEN;
#endif

    /* TODO: Convert this to suitable adf routines */
    qdf_nbuf_append_ext_list(mpdu_buf, head_frag_list_cloned,
            frag_list_sum_len);
mpdu_stitch_done:
    /* Check if this buffer contains the PPDU end status for TSF */
    last_mpdu =
            (*(((u_int32_t *)&rx_desc->attention)) &
             RX_ATTENTION_0_LAST_MPDU_MASK) >>
             RX_ATTENTION_0_LAST_MPDU_LSB;
    if (last_mpdu) {
        rx_status->rs_tstamp.tsf = rx_desc->ppdu_end.tsf_timestamp;
    }

    return (mpdu_buf);


mpdu_stitch_fail:
    if ((mpdu_buf) && (decap_format != HW_RX_DECAP_FORMAT_RAW || !clone_not_reqd)) {
        /* Free the head buffer */
        qdf_nbuf_free(mpdu_buf);
    }
    if (!clone_not_reqd) {
        /* Free the partial list */
        while (head_frag_list_cloned) {
            msdu = head_frag_list_cloned;
            head_frag_list_cloned = qdf_nbuf_next_ext(head_frag_list_cloned);
            qdf_nbuf_free(msdu);
        }
    }
    return NULL;

}

#define FN_RX_DESC_RESTITCH_MPDU_FROM_MSDUS(target) \
static qdf_nbuf_t _##target##_rx_desc_restitch_mpdu_from_msdus(ar_handle_t arh, \
        qdf_nbuf_t head_msdu, \
        struct ieee80211_rx_status *rx_status, \
        unsigned clone_not_reqd) \
{ \
    return unified_rx_desc_restitch_mpdu_from_msdus(arh, \
            head_msdu, \
            rx_status, \
            clone_not_reqd); \
}

static inline void unified_rx_desc_get_vowext_stats(qdf_nbuf_t msdu,
        struct vow_extstats *vowstats)
{
    u_int32_t *ppdu;
    u_int8_t preamble_type;
    u_int8_t rate = 0, nss=0, bw=0, sgi = 0, mcs = 0, rs_flags=0;
    struct rx_desc_base *rx_desc;
    rx_desc = ar_rx_desc(msdu);

    ppdu = ((u_int32_t *)&rx_desc->ppdu_start);
    preamble_type = (ppdu[5] & 0xff000000) >> 24;
    switch(preamble_type)
    {
        /* HT */
        case 8: /* HT w/o TxBF */
        case 9:/* HT w/ TxBF */
            mcs = (u_int8_t)(ppdu[6] & 0x7f);
            nss = mcs>>3;
            mcs %= 8;
            bw  = (u_int8_t)((ppdu[6] >> 7) & 1);
            sgi = (u_int8_t)((ppdu[6] >> 7) & 1);
            rate = AR600P_ASSEMBLE_HW_RATECODE(mcs, nss, AR600P_HW_RATECODE_PREAM_HT);
            if (bw) {
                rs_flags |= HAL_RX_40;
            }
            if (sgi) {
                rs_flags |= HAL_RX_GI;
            }
            break;
            /* VHT */
        case 0x0c: /* VHT w/o TxBF */
        case 0x0d: /* VHT w/ TxBF */
            mcs = (u_int8_t)((ppdu[7] >> 4) & 0xf);
            nss = (u_int8_t)((ppdu[6] >> 10) & 0x7);
            bw  = (u_int8_t)((ppdu[6] & 3));
            sgi = (u_int8_t)((ppdu[7]) & 1);
            rate = AR600P_ASSEMBLE_HW_RATECODE(mcs, nss, AR600P_HW_RATECODE_PREAM_VHT);
            break;
    }

    vowstats->rx_bw = bw; /* band width 0 - 20 , 1 - 40 , 2 - 80 */
    vowstats->rx_sgi = sgi; /* 1 - short GI */
    vowstats->rx_nss= nss; /* Nss */
    vowstats->rx_mcs = mcs;
    vowstats->rx_ratecode = rate;
    vowstats->rx_rs_flags= rs_flags; /* rsflags */

    vowstats->rx_rssi_ctl0 = (ppdu[0] & 0x000000ff); /* rssi ctl0 */
    vowstats->rx_rssi_ctl1 = (ppdu[1] & 0x000000ff); /* rssi ctl1 */
    vowstats->rx_rssi_ctl2 = (ppdu[2] & 0x000000ff); /* rssi ctl2 */
    vowstats->rx_rssi_ext0 = (ppdu[0] & 0x0000ff00) >> 8; /* rssi ext0 */
    vowstats->rx_rssi_ext1 = (ppdu[1] & 0x0000ff00) >> 8; /* rssi ext1 */
    vowstats->rx_rssi_ext2 = (ppdu[2] & 0x0000ff00) >> 8; /* rssi ext2 */
    vowstats->rx_rssi_comb = (ppdu[4] & 0x000000ff); /* rssi comb */

    ppdu = ((u_int32_t *)&rx_desc->ppdu_end);
    /* Time stamp */
    vowstats->rx_macTs = ppdu[16];

    ppdu = ((u_int32_t *)&rx_desc->attention);
    /* more data */
    vowstats->rx_moreaggr = (ppdu[0] & RX_ATTENTION_0_MORE_DATA_MASK);

    /* sequence number */
    ppdu = ((u_int32_t *)&rx_desc->mpdu_start);
    vowstats->rx_seqno = (ppdu[0] & 0x0fff0000) >> 16;
}

#define FN_RX_DESC_GET_VOWEXT_STATS(target) \
static void _##target##_rx_desc_get_vowext_stats(qdf_nbuf_t msdu, \
        struct vow_extstats *vowstats) \
{ \
    unified_rx_desc_get_vowext_stats(msdu, \
            vowstats); \
}
#if UNIFIED_SMARTANTENNA
static inline int unified_rx_desc_get_smart_ant_stats(ar_handle_t arh,
       qdf_nbuf_t rx_ind_msg,
       qdf_nbuf_t head_msdu,
       qdf_nbuf_t tail_msdu,
       struct sa_rx_feedback *feedback)
{
    struct ar_s *ar = (struct ar_s *)arh;
    u_int32_t *ppdu;
    int i = 0, status = 1;
    u_int8_t rate_code = 0, nss=0, bw=0, sgi = 0, mcs = 0, l_sig_rate_select,l_sig_rate;
    struct rx_desc_base *rx_desc;
    u_int8_t preamble_type;
    u_int32_t num_msdu, atten;

    num_msdu = ar->rx_callbacks->rx_ind_fw_rx_desc_bytes_get(ar->htt_handle, rx_ind_msg);

    /* If first MSDU set in head then only RSSI, Rate info is valid */
    rx_desc = ar_rx_desc(head_msdu);
    atten = *(u_int32_t *)&rx_desc->attention;
    if (atten & 0x1) { /* first MPDU */

        ppdu = ((u_int32_t *)&rx_desc->ppdu_start);

        for (i = 0; i < SMART_ANT_MAX_SA_CHAINS; i++) {
            feedback->rx_rssi[i] = ppdu[i];
        }

        preamble_type = (ppdu[RX_DESC_PREAMBLE_OFFSET] & MASK_BYTE3) >> SHIFT_BYTE3;
        switch(preamble_type)
        {
            case 4:
                l_sig_rate_select = (ppdu[RX_DESC_PREAMBLE_OFFSET] & RX_DESC_MASK_LSIG_SEL) ; /* Bit 4 */
                l_sig_rate = (ppdu[RX_DESC_PREAMBLE_OFFSET]  & RX_DESC_MASK_LSIG_RATE); /* Bit[3:0] */
                if( l_sig_rate_select == 1 ) { /* CCK */
                    switch(l_sig_rate) {
                        case 0xb:
                            rate_code=0x43;
                            break;
                        case 0xa:
                        case 0xe:
                            rate_code=0x42;
                            break;
                        case 0x9:
                        case 0xd:
                            rate_code=0x41;
                            break;
                        case 0x8:
                        case 0xc:
                            rate_code=0x40;
                            break;
                    }
                } else { /* OFDM, l_sig_rate_select==0 */
                    switch(l_sig_rate) {
                        case 0x8:
                            rate_code=0x00;
                            break;
                        case 0x9:
                            rate_code=0x01;
                            break;
                        case 0xa:
                            rate_code=0x02;
                            break;
                        case 0xb:
                            rate_code=0x03;
                            break;
                        case 0xc:
                            rate_code=0x04;
                            break;
                        case 0xd:
                            rate_code=0x05;
                            break;
                        case 0xe:
                            rate_code=0x06;
                            break;
                        case 0xf:
                            rate_code=0x07;
                            break;
                    }
                }
                break;
                /* HT */
            case 8: /* HT w/o TxBF */
            case 9:/* HT w/ TxBF */
                mcs = (u_int8_t)(ppdu[RX_DESC_HT_RATE_OFFSET] & RX_DESC_HT_MCS_MASK);
                nss = mcs>>3;
                mcs %= 8;
                bw  = (u_int8_t)((ppdu[RX_DESC_HT_RATE_OFFSET] >> 7) & 1);
                sgi = (u_int8_t)((ppdu[RX_DESC_HT_RATE_OFFSET] >> 7) & 1);
                rate_code = AR600P_ASSEMBLE_HW_RATECODE(mcs, nss, AR600P_HW_RATECODE_PREAM_HT);
                break;
                /* VHT */
            case 0x0c: /* VHT w/o TxBF */
            case 0x0d: /* VHT w/ TxBF */
                mcs = (u_int8_t)((ppdu[RX_DESC_VHT_RATE_OFFSET] >> 4) & RX_DESC_VHT_MCS_MASK);
                nss = (u_int8_t)((ppdu[RX_DESC_HT_RATE_OFFSET] >> 10) & RX_DESC_VHT_NSS_MASK);
                bw  = (u_int8_t)((ppdu[RX_DESC_HT_RATE_OFFSET] & RX_DESC_VHT_BW_MASK));
                sgi = (u_int8_t)((ppdu[RX_DESC_VHT_RATE_OFFSET]) & RX_DESC_VHT_SGI_MASK);
                rate_code = AR600P_ASSEMBLE_HW_RATECODE(mcs, nss, AR600P_HW_RATECODE_PREAM_VHT);
                break;
        }

        feedback->rx_rate_mcs = rate_code;
        feedback->rx_rate_mcs = (feedback->rx_rate_mcs << (bw*8));
    } else { /* invalid First MPDU */

        return 0;
    }


    /* If Last MSDU set in head then only EVM, Antenna info is valid */
    rx_desc = ar_rx_desc(tail_msdu);
    atten = *(u_int32_t *)&rx_desc->attention;
    if (atten & 0x2) { /* Last MPDU */
        ppdu = ((u_int32_t *)&rx_desc->ppdu_end);
        /* ppdu[0] - ppdu[15] is Pilot EVM */
        for (i = 0; i < MAX_EVM_SIZE; i++) {
            feedback->rx_evm[i] = ppdu[i];
        }
        feedback->rx_antenna = (ppdu[RX_DESC_ANTENNA_OFFSET] & SMART_ANT_ANTENNA_MASK);

    } else { /* invalid Lasdt MPDU */
        return 0;
    }
    return status;
}
#define FN_RX_DESC_GET_SMART_ANT_STATS(target) \
static int _##target##_rx_desc_get_smart_ant_stats(ar_handle_t arh, \
       qdf_nbuf_t rx_ind_msg, \
       qdf_nbuf_t head_msdu, \
       qdf_nbuf_t tail_msdu, \
       struct sa_rx_feedback *feedback) \
{ \
    return unified_rx_desc_get_smart_ant_stats(arh, \
            rx_ind_msg, \
            head_msdu, \
            tail_msdu, \
            feedback); \
}
#else
#define FN_RX_DESC_GET_SMART_ANT_STATS(target)
#endif

static inline int unified_rx_desc_update_msdu_info(qdf_nbuf_t head_msdu,
                                                  qdf_nbuf_t tail_msdu)
{
    int first_mpdu = 0, last_mpdu = 0;
    int msdu_status = 0;
    struct rx_desc_base *rx_desc = ar_rx_desc(head_msdu);

    first_mpdu = ((*((u_int32_t *) &rx_desc->attention)) &
                  RX_ATTENTION_0_FIRST_MPDU_MASK) >>
                  RX_ATTENTION_0_FIRST_MPDU_LSB;

    last_mpdu = ((*((u_int32_t *) &rx_desc->attention)) &
                 RX_ATTENTION_0_LAST_MPDU_MASK) >>
                 RX_ATTENTION_0_LAST_MPDU_LSB;

                 /* non-Aggregation frame */
                 if ((first_mpdu == 1) && (last_mpdu == 1)) {
                     msdu_status |= 0x00;
                 }
                 /* For A-MPDU Frame should be set for all last subframe is known */
                 else {
                      msdu_status |= 0x0400; //last subframe known
                      if (last_mpdu)
                         msdu_status |= 0x0800; // last subframe
                 }
    return msdu_status;
}

#if ATH_DATA_RX_INFO_EN || MESH_MODE_SUPPORT
static inline int unified_rx_desc_update_pkt_info(qdf_nbuf_t head_msdu,
                                                  qdf_nbuf_t tail_msdu,
                                                  uint32_t permsdu_flag)
{
    struct ieee80211_rx_status rx_info;
    int frag = 0;
    int first_mpdu = 0, last_mpdu = 0;
    int is_head = 0, is_tail = 0;
    struct rx_desc_base *rx_desc = ar_rx_desc(head_msdu);
#if ATH_DATA_RX_INFO_EN
    struct per_msdu_data_recv_status *rx_status = NULL;
#else
    struct mesh_recv_hdr_s *rx_status = NULL;
#endif

    if (permsdu_flag) {
       /*
        * single msdu has been passed; need to check
        * whether it's the first or last in the chain
        */
        is_head = ((*(((u_int32_t *) &rx_desc->msdu_end) + 4)) &
                                    RX_MSDU_END_4_FIRST_MSDU_MASK) >>
                                     RX_MSDU_END_4_FIRST_MSDU_LSB;

        is_tail = ((*(((u_int32_t *) &rx_desc->msdu_end) + 4)) &
                                     RX_MSDU_END_4_LAST_MSDU_MASK) >>
                                      RX_MSDU_END_4_LAST_MSDU_LSB;
        if (is_tail) {
            /*
             * now that the msdu is at the tail of
             * the chain, time to update the pointer
             */
            tail_msdu = head_msdu;
        }

    }

    memset(&rx_info, 0, sizeof(rx_info));

    if (!permsdu_flag || (is_head)) {
        frag = qdf_nbuf_is_rx_chfrag_start(head_msdu);
        first_mpdu =
                 ((*((u_int32_t *) &rx_desc->attention)) &
                  RX_ATTENTION_0_FIRST_MPDU_MASK) >>
                  RX_ATTENTION_0_FIRST_MPDU_LSB;
        if (first_mpdu) {
#if ATH_DATA_RX_INFO_EN
            if(!(rx_status = qdf_mem_malloc(sizeof(struct mesh_recv_hdr_s)))) {
                return -1;
            }
            qdf_nbuf_set_rx_fctx_type(head_msdu, (void *)rx_status, CB_FTYPE_RX_INFO);
#else
            if (!(rx_status = qdf_mem_malloc(sizeof(struct mesh_recv_hdr_s)))) {
                return -1;
            }
            qdf_nbuf_set_rx_fctx_type(head_msdu, (void *)rx_status, CB_FTYPE_MESH_RX_INFO);
#endif
            /* Fill out the rx_status from the PPDU head fields */
            rx_desc_parse_ppdu_start_status(rx_desc, (struct ieee80211_rx_status *)&rx_info);
            rx_status->rs_flags |= IEEE80211_RX_FIRST_MSDU;
            rx_status->rs_rssi = rx_info.rs_rssi;
            rx_status->rs_ratephy1 = rx_info.rs_ratephy1;
            rx_status->rs_ratephy2 = rx_info.rs_ratephy2;
            rx_status->rs_ratephy3 = rx_info.rs_ratephy3;
            if(rx_status->rs_rssi > 127){
                rx_status->rs_rssi = 127;
            }
        }
        /* restoring the value of frag */
        qdf_nbuf_set_rx_chfrag_start(head_msdu,frag);
    }

    if (!permsdu_flag || (is_tail)) {

        rx_desc = ar_rx_desc(tail_msdu);
        last_mpdu =
                ((*((u_int32_t *) &rx_desc->attention)) &
                 RX_ATTENTION_0_LAST_MPDU_MASK) >>
                 RX_ATTENTION_0_LAST_MPDU_LSB;
        if (last_mpdu) {
            if (!rx_status) {
#if ATH_DATA_RX_INFO_EN
                if(!(rx_status = qdf_mem_malloc(sizeof(struct per_msdu_data_recv_status)))) {
                    return -1;
                }
                qdf_nbuf_set_rx_fctx_type(tail_msdu, (void *)rx_status, CB_FTYPE_RX_INFO);
#else
                if (!(rx_status = qdf_mem_malloc(sizeof(struct mesh_recv_hdr_s)))) {
                    return -1;
                }
                qdf_nbuf_set_rx_fctx_type(tail_msdu, (void *)rx_status, CB_FTYPE_MESH_RX_INFO);
#endif
            }
            /*Set flag for last MSDU*/
            rx_status->rs_flags |= IEEE80211_RX_LAST_MSDU;
        }
    }

    return 0;
}

#define FN_RX_DESC_UPDATE_MSDU_INFO(target) \
static int _##target##_rx_desc_update_msdu_info(ar_handle_t arh,\
        qdf_nbuf_t head_msdu, \
        qdf_nbuf_t tail_msdu) \
{ \
    return unified_rx_desc_update_msdu_info(head_msdu, tail_msdu); \
}

#define FN_RX_DESC_UPDATE_PKT_INFO(target) \
static int _##target##_rx_desc_update_pkt_info(ar_handle_t arh,\
        qdf_nbuf_t head_msdu, \
        qdf_nbuf_t tail_msdu, \
        uint32_t permsdu_flag) \
{ \
    return unified_rx_desc_update_pkt_info(head_msdu, tail_msdu, permsdu_flag); \
}
#endif /*end of ATH_DATA_RX_INFO_EN || MESH_MODE_SUPPORT */

#endif //__RX_DESC_UNIFIED_H
