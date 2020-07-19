/*
 * Copyright (c) 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef __AR_OPS_H
#define __AR_OPS_H

typedef void * rx_handle_t;
typedef void * ar_handle_t;

struct vow_extstats;
struct sa_rx_feedback;
struct ieee80211_rx_status;

struct htt_rx_callbacks {
   u_int8_t   (*txrx_is_decap_type_raw)(void *vdev);
   u_int32_t  (*rx_ring_elems)(rx_handle_t rxh);
   u_int32_t  (*rx_ind_fw_rx_desc_bytes_get)(rx_handle_t rxh, qdf_nbuf_t rx_ind_msg);
   qdf_nbuf_t (*rx_netbuf_peek)(rx_handle_t rxh);
   qdf_nbuf_t (*rx_netbuf_pop)(rx_handle_t rxh);
   u_int32_t  (*rx_buf_size)(rx_handle_t rxh);
   qdf_nbuf_t (*rx_debug)(rx_handle_t rxh, qdf_nbuf_t rx_ind_msg, qdf_nbuf_t msdu, int *npackets, int msdu_done);
   u_int32_t  (*rx_desc_reservation)(rx_handle_t rxh);
   int        (*rx_ind_msdu_byte_offset)(rx_handle_t rxh, int num_msdu_bytes, int *byte_offset);
   void       (*rx_ind_msdu_byte_idx_inc)(rx_handle_t rxh);
   void       (*ath_add_vow_extstats)(rx_handle_t rxh, qdf_nbuf_t msdu);
   void       (*ath_arp_debug)(rx_handle_t rxh, qdf_nbuf_t msdu);
};

struct ar_rx_desc_ops {
    u_int32_t (*sizeof_rx_desc)(void);
    u_int32_t (*offsetof_attention)(void);
    u_int32_t (*offsetof_frag_info)(void);
    u_int32_t (*offsetof_mpdu_start)(void);
    u_int32_t (*offsetof_msdu_start)(void);
    u_int32_t (*offsetof_msdu_end)(void);
    u_int32_t (*offsetof_mpdu_end)(void);
    u_int32_t (*offsetof_ppdu_start)(void);
    u_int32_t (*offsetof_ppdu_end)(void);
    u_int32_t (*offsetof_fw_desc)(void);
    u_int32_t (*offsetof_hdr_status)(void);
    u_int32_t (*sizeof_attention)(void);
    u_int32_t (*sizeof_frag_info)(void);
    u_int32_t (*sizeof_mpdu_start)(void);
    u_int32_t (*sizeof_msdu_start)(void);
    u_int32_t (*sizeof_msdu_end)(void);
    u_int32_t (*sizeof_mpdu_end)(void);
    u_int32_t (*sizeof_ppdu_start)(void);
    u_int32_t (*sizeof_ppdu_end)(void);
    void      (*init_desc)(void* rx_desc);
    char*     (*wifi_hdr_retrieve)(void* rx_desc);
    u_int32_t (*mpdu_desc_seq_num)(ar_handle_t arh,
                void *rx_desc);
    void      (*mpdu_desc_pn)(void *rx_desc,
                u_int32_t *pn,
                int pn_len_bits);
    u_int32_t (*mpdu_desc_frds)(void *rx_desc);
    u_int32_t (*mpdu_desc_tods)(void *rx_desc);
    u_int32_t (*mpdu_is_encrypted)(ar_handle_t arh, void *rx_desc);
    u_int32_t (*get_attn_word)(void *rx_desc);
    u_int32_t* (*get_ppdu_start)(void *rx_desc);
    u_int32_t (*attn_msdu_done)(void *rx_desc);
    u_int32_t (*msdu_desc_msdu_length)(void *rx_desc);
    u_int32_t (*msdu_desc_completes_mpdu)(ar_handle_t arh,
                void *rx_desc);
    u_int32_t (*msdu_has_wlan_mcast_flag)(ar_handle_t arh,
                void *rx_desc);
    u_int32_t (*msdu_is_wlan_mcast)(ar_handle_t arh,
                void *rx_desc);
    u_int32_t (*msdu_is_frag)(ar_handle_t arh,
                void *rx_desc);
    u_int32_t (*msdu_first_msdu_flag)(ar_handle_t arh,
                void *rx_desc);
    u_int32_t (*msdu_key_id_octet)(ar_handle_t arh,
                void *rx_desc);
    void      (*dump_desc)(void *rx_desc);
    u_int32_t (*check_desc_mgmt_type)(qdf_nbuf_t head_msdu);
    u_int32_t (*check_desc_ctrl_type)(qdf_nbuf_t head_msdu);
    u_int32_t (*check_desc_phy_data_type)(qdf_nbuf_t head_msdu, uint8_t *rx_phy_data_filter);
    u_int32_t (*get_l3_header_padding)(qdf_nbuf_t head_msdu);
    u_int32_t (*amsdu_pop)(ar_handle_t arh,
#if QCA_OL_SUPPORT_RAWMODE_TXRX
                void *vdev,
#endif
                qdf_nbuf_t rx_ind_msg,
                qdf_nbuf_t* head_msdu,
                qdf_nbuf_t* tail_msdu,
                u_int32_t* npackets);
    qdf_nbuf_t (*restitch_mpdu_from_msdus)(ar_handle_t arh,
                 qdf_nbuf_t head_msdu,
                 struct ieee80211_rx_status *rx_status,
                 unsigned clone_not_reqd);
    void      (*get_vowext_stats)(qdf_nbuf_t msdu,
                struct vow_extstats *vowstats);
    int       (*get_smart_ant_stats)(ar_handle_t arh,
                qdf_nbuf_t rx_ind_msg,
                qdf_nbuf_t head_msdu,
                qdf_nbuf_t tail_msdu,
                struct sa_rx_feedback *feedback);
#if ATH_DATA_RX_INFO_EN || MESH_MODE_SUPPORT
    int       (*update_pkt_info)(ar_handle_t arh,
                qdf_nbuf_t head_msdu,
                qdf_nbuf_t tail_msdu,
                uint32_t permsdu_flag);
#endif
/*
    update_msdu_info function is used to get
    msdu status from the A-MPDU/MPDU frame
    to indicate the status of mpdu flags in sniffer,
    when we capture packets in monitor mode
*/
    int       (*update_msdu_info)(ar_handle_t arh,
                qdf_nbuf_t head_msdu,
                qdf_nbuf_t tail_msdu);

    u_int32_t (*msdu_desc_phy_data_type)(void* rx_desc);
    u_int32_t (*msdu_desc_msdu_chained)(void* rx_desc);
    u_int32_t (*msdu_desc_tsf_timestamp)(void* rx_desc);
    ssize_t   (*fw_rx_desc_size)(void);
    int       (*is_first_mpdu)(void *rxd);
    int       (*is_last_mpdu)(void *rxd);
    int       (*is_retry)(void *rxd);
    void*     (*get_rssi)(void *rxd, int *co, int *cm, int *cs);
};

#define RX_DESC_ALIGN_MASK 7 /* 8-byte alignment */
#ifndef RX_DESC_ALIGN_RESERVED
#define RX_DESC_ALIGN_RESERVED 0
#endif

#endif //__AR_OPS_H
