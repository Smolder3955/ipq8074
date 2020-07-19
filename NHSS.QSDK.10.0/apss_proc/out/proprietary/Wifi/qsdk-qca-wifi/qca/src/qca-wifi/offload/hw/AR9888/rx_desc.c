/*
 * Copyright (c) 2015,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <qdf_mem.h>   /* qdf_mem_malloc,free, etc. */
#include <qdf_types.h> /* qdf_print */
#include <qdf_nbuf.h>
#include <ieee80211_defines.h>
#include <ol_vowext_dbg_defs.h>
#include <ol_ratetable.h>
#include <ol_cfg_raw.h>

/* AR9888 RX descriptor */
#include <AR9888/v2/hw/mac_descriptors/rx_attention.h>
#include <AR9888/v2/hw/mac_descriptors/rx_frag_info.h>
#include <AR9888/v2/hw/mac_descriptors/rx_msdu_start.h>
#include <AR9888/v2/hw/mac_descriptors/rx_msdu_end.h>
#include <AR9888/v2/hw/mac_descriptors/rx_mpdu_start.h>
#include <AR9888/v2/hw/mac_descriptors/rx_mpdu_end.h>
#include <AR9888/v2/hw/mac_descriptors/rx_ppdu_start.h>
#include <AR9888/v2/hw/mac_descriptors/rx_ppdu_end.h>

/* defines HW descriptor format */
#include "../include/rx_desc_internal.h"
#include "../include/ar_internal.h"

#define RX_DESC_ANTENNA_OFFSET 19
#define GET_L3_HEADER_PADDING(rx_desc) 0

#ifndef RX_PPDU_START_0_RSSI_PRI_CHAIN0_MASK
#define RX_PPDU_START_0_RSSI_PRI_CHAIN0_MASK     RX_PPDU_START_0_RSSI_CHAIN0_PRI20_MASK
#define RX_PPDU_START_0_RSSI_SEC80_CHAIN0_MASK   RX_PPDU_START_0_RSSI_CHAIN0_SEC80_MASK
#define RX_PPDU_START_0_RSSI_PRI_CHAIN0_LSB      RX_PPDU_START_0_RSSI_CHAIN0_PRI20_LSB
#define RX_PPDU_START_0_RSSI_SEC80_CHAIN0_LSB    RX_PPDU_START_0_RSSI_CHAIN0_SEC80_LSB
#endif

#define rx_desc_parse_ppdu_start_status(rx_desc, rx_status) \
    _ar9888_rx_desc_parse_ppdu_start_status(rx_desc, rx_status)

#if RX_CHECKSUM_OFFLOAD
#define rx_desc_set_checksum_result(rx_desc, msdu) \
    _ar9888_rx_desc_set_checksum_result(rx_desc, msdu)
#else
#define rx_desc_set_checksum_result(rx_desc, msdu)
#endif

#define rx_desc_set_lro_info(rx_desc, msdu)

static inline int
_ar9888_rx_desc_is_first_mpdu(void *_rxd)
{
    struct rx_desc_base *rxd;
    uint32_t *ptr;
    rxd = _rxd;
    ptr = (void *)&rxd->attention;
    return ptr[0] & RX_ATTENTION_0_FIRST_MPDU_MASK;
}

static inline int
_ar9888_rx_desc_is_last_mpdu(void *_rxd)
{
    struct rx_desc_base *rxd;
    uint32_t *ptr;
    rxd = _rxd;
    ptr = (void *)&rxd->attention;
    return ptr[0] & RX_ATTENTION_0_LAST_MPDU_MASK;
}

static int
_ar9888_rx_desc_is_retry(void *_rxd)
{
    struct rx_desc_base *rxd;
    uint32_t *ptr;
    rxd = _rxd;
    ptr = (void *)&rxd->mpdu_start;
    return ptr[0] & RX_MPDU_START_0_RETRY_MASK;
}

static void *
_ar9888_rx_desc_get_rssi(void *_rxd, int *co, int *cm, int *cs)
{
    struct rx_desc_base *rxd;
    rxd = _rxd;
    *co = RX_PPDU_START_4_RSSI_COMB_OFFSET / sizeof(uint32_t);
    *cm = RX_PPDU_START_4_RSSI_COMB_MASK;
    *cs = RX_PPDU_START_4_RSSI_COMB_LSB;
    return (void *)&rxd->ppdu_start;
}

static u_int32_t _ar9888_rx_desc_check_desc_phy_data_type(qdf_nbuf_t head_msdu, uint8_t *rx_phy_data_filter )
{
    /* Not Applicable */
    *rx_phy_data_filter = 0;
    return 1;
}

static u_int32_t _ar9888_rx_desc_get_l3_header_padding(qdf_nbuf_t head_msdu)
{
	return GET_L3_HEADER_PADDING(head_msdu);
}

static u_int32_t _ar9888_msdu_desc_phy_data_type(void *rx_desc)
{
    /* for AR9888, we dont have phy data type a
     * vailable in descriptor.
     */
    qdf_assert_always(false);
    return false;
}

#if RX_CHECKSUM_OFFLOAD
static void _ar9888_rx_desc_set_checksum_result(void *rx_desc,
        qdf_nbuf_t msdu);
#endif
static void _ar9888_rx_desc_parse_ppdu_start_status(void *rx_desc,
        struct ieee80211_rx_status *rs);

/* Unified function definitions */
#include "../include/rx_desc_unified.h"

/* definitions for size and offset functions */
AR_RX_DESC_FIELD_FUNCS(ar9888, attention)
AR_RX_DESC_FIELD_FUNCS(ar9888, frag_info)
AR_RX_DESC_FIELD_FUNCS(ar9888, mpdu_start)
AR_RX_DESC_FIELD_FUNCS(ar9888, msdu_start)
AR_RX_DESC_FIELD_FUNCS(ar9888, msdu_end)
AR_RX_DESC_FIELD_FUNCS(ar9888, mpdu_end)
AR_RX_DESC_FIELD_FUNCS(ar9888, ppdu_start)
AR_RX_DESC_FIELD_FUNCS(ar9888, ppdu_end)

FN_RX_DESC_OFFSETOF_FW_DESC(ar9888)
FN_RX_DESC_OFFSETOF_HDR_STATUS(ar9888)
FN_RX_DESC_SIZE(ar9888)
FN_RX_DESC_FW_DESC_SIZE(ar9888)
FN_RX_DESC_INIT(ar9888)
FN_RX_DESC_MPDU_WIFI_HDR_RETRIEVE(ar9888)
FN_RX_DESC_MPDU_DESC_SEQ_NUM(ar9888)
FN_RX_DESC_MPDU_DESC_PN(ar9888)
FN_RX_DESC_MPDU_DESC_FRDS(ar9888)
FN_RX_DESC_MPDU_DESC_TODS(ar9888)
FN_RX_DESC_MPDU_IS_ENCRYPTED(ar9888)
FN_RX_DESC_GET_ATTN_WORD(ar9888)
FN_RX_DESC_GET_PPDU_START(ar9888)
FN_RX_DESC_ATTN_MSDU_DONE(ar9888)
FN_RX_DESC_MSDU_LENGTH(ar9888)
FN_RX_DESC_MSDU_DESC_COMPLETES_MPDU(ar9888)
FN_RX_DESC_MSDU_HAS_WLAN_MCAST_FLAG(ar9888)
FN_RX_DESC_MSDU_IS_WLAN_MCAST(ar9888)
FN_RX_DESC_MSDU_IS_FRAG(ar9888)
FN_RX_DESC_MSDU_FIRST_MSDU_FLAG(ar9888)
FN_RX_DESC_MSDU_KEY_ID_OCTET(ar9888)
#if RX_CHECKSUM_OFFLOAD
FN_RX_DESC_SET_CHECKSUM_RESULT(ar9888)
#endif
FN_RX_DESC_DUMP(ar9888)
FN_RX_DESC_CHECK_DESC_MGMT_TYPE(ar9888)
FN_RX_DESC_CHECK_DESC_CTRL_TYPE(ar9888)
FN_RX_DESC_MSDU_DESC_MSDU_CHAINED(ar9888)
FN_RX_DESC_MSDU_DESC_TSF_TIMESTAMP(ar9888)
FN_RX_DESC_AMSDU_POP(ar9888)
FN_RX_DESC_PARSE_PPDU_START_STATUS(ar9888)
FN_RX_DESC_RESTITCH_MPDU_FROM_MSDUS(ar9888)
FN_RX_DESC_GET_VOWEXT_STATS(ar9888)
#if UNIFIED_SMARTANTENNA
FN_RX_DESC_GET_SMART_ANT_STATS(ar9888)
#endif
#if ATH_DATA_RX_INFO_EN || MESH_MODE_SUPPORT
FN_RX_DESC_UPDATE_PKT_INFO(ar9888)
FN_RX_DESC_UPDATE_MSDU_INFO(ar9888)
#endif

struct ar_rx_desc_ops ar9888_rx_desc_ops = {
    .sizeof_rx_desc = _ar9888_rx_desc_size,
    .offsetof_attention = _ar9888_rx_desc_offsetof_attention,
    .offsetof_frag_info = _ar9888_rx_desc_offsetof_frag_info,
    .offsetof_mpdu_start = _ar9888_rx_desc_offsetof_mpdu_start,
    .offsetof_msdu_start = _ar9888_rx_desc_offsetof_msdu_start,
    .offsetof_msdu_end = _ar9888_rx_desc_offsetof_msdu_end,
    .offsetof_mpdu_end = _ar9888_rx_desc_offsetof_mpdu_end,
    .offsetof_ppdu_start = _ar9888_rx_desc_offsetof_ppdu_start,
    .offsetof_ppdu_end = _ar9888_rx_desc_offsetof_ppdu_end,
    .offsetof_fw_desc = _ar9888_rx_desc_offsetof_fw_desc,
    .offsetof_hdr_status = _ar9888_rx_desc_offsetof_hdr_status,
    .sizeof_attention = _ar9888_rx_desc_sizeof_attention,
    .sizeof_frag_info = _ar9888_rx_desc_sizeof_frag_info,
    .sizeof_mpdu_start = _ar9888_rx_desc_sizeof_mpdu_start,
    .sizeof_msdu_start = _ar9888_rx_desc_sizeof_msdu_start,
    .sizeof_msdu_end = _ar9888_rx_desc_sizeof_msdu_end,
    .sizeof_mpdu_end = _ar9888_rx_desc_sizeof_mpdu_end,
    .sizeof_ppdu_start = _ar9888_rx_desc_sizeof_ppdu_start,
    .sizeof_ppdu_end = _ar9888_rx_desc_sizeof_ppdu_end,
    .init_desc = _ar9888_rx_desc_init,
    .wifi_hdr_retrieve = _ar9888_rx_desc_mpdu_wifi_hdr_retrieve,
    .mpdu_desc_seq_num = _ar9888_rx_desc_mpdu_desc_seq_num,
    .mpdu_desc_pn = _ar9888_rx_desc_mpdu_desc_pn,
    .mpdu_desc_frds = _ar9888_rx_desc_mpdu_desc_frds,
    .mpdu_desc_tods = _ar9888_rx_desc_mpdu_desc_tods,
    .mpdu_is_encrypted = _ar9888_rx_desc_mpdu_is_encrypted,
    .get_attn_word = _ar9888_rx_desc_get_attn_word,
    .get_ppdu_start = _ar9888_rx_desc_get_ppdu_start,
    .attn_msdu_done = _ar9888_rx_desc_attn_msdu_done,
    .msdu_desc_msdu_length = _ar9888_rx_desc_msdu_length,
    .msdu_desc_completes_mpdu = _ar9888_rx_desc_msdu_desc_completes_mpdu,
    .msdu_has_wlan_mcast_flag = _ar9888_rx_desc_msdu_has_wlan_mcast_flag,
    .msdu_is_wlan_mcast = _ar9888_rx_desc_msdu_is_wlan_mcast,
    .msdu_is_frag = _ar9888_rx_desc_msdu_is_frag,
    .msdu_first_msdu_flag = _ar9888_rx_desc_msdu_first_msdu_flag,
    .msdu_key_id_octet = _ar9888_rx_desc_msdu_key_id_octet,
    .dump_desc = _ar9888_rx_desc_dump,
    .check_desc_mgmt_type = _ar9888_rx_desc_check_desc_mgmt_type,
    .check_desc_ctrl_type = _ar9888_rx_desc_check_desc_ctrl_type,
#ifndef REMOVE_PKT_LOG
    .check_desc_phy_data_type = _ar9888_rx_desc_check_desc_phy_data_type,
#endif
    .get_l3_header_padding = _ar9888_rx_desc_get_l3_header_padding,
    .amsdu_pop = _ar9888_rx_desc_amsdu_pop,
    .restitch_mpdu_from_msdus = _ar9888_rx_desc_restitch_mpdu_from_msdus,
    .get_vowext_stats = _ar9888_rx_desc_get_vowext_stats,
#if UNIFIED_SMARTANTENNA
    .get_smart_ant_stats = _ar9888_rx_desc_get_smart_ant_stats,
#else
    .get_smart_ant_stats = NULL,
#endif
    .msdu_desc_phy_data_type = _ar9888_msdu_desc_phy_data_type,
    .msdu_desc_msdu_chained = _ar9888_msdu_desc_msdu_chained,
    .msdu_desc_tsf_timestamp = _ar9888_msdu_desc_tsf_timestamp,
    .fw_rx_desc_size = _ar9888_fw_rx_desc_size,
#if ATH_DATA_RX_INFO_EN || MESH_MODE_SUPPORT
    .update_pkt_info = _ar9888_rx_desc_update_pkt_info,
    .update_msdu_info = _ar9888_rx_desc_update_msdu_info,
#endif
    .is_first_mpdu = _ar9888_rx_desc_is_first_mpdu,
    .is_last_mpdu  = _ar9888_rx_desc_is_last_mpdu,
    .is_retry = _ar9888_rx_desc_is_retry,
    .get_rssi = _ar9888_rx_desc_get_rssi,
};

struct ar_rx_desc_ops* ar9888_rx_attach(struct ar_s *ar)
{
    /* Attach the function pointers table */
    ar->ar_rx_ops = &ar9888_rx_desc_ops;
    return ar->ar_rx_ops;
}

