/*
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <ol_cfg.h>
#include <ol_if_athvar.h>
#include "wlan_defs.h"
#include "ol_ath.h"
#include "hif.h"
#include "target_type.h"
#if QCA_AIRTIME_FAIRNESS
#include <target_if_atf.h>
#endif
#include <target_if.h>
#include <init_deinit_lmac.h>
#include <wlan_lmac_dispatcher.h>
#include "cfg_ucfg_api.h"

/* FIX THIS -
 * For now, all these configuration parameters are hardcoded.
 * Many of these should actually be determined dynamically instead.
 */

#if defined (ATHR_WIN_NWF)
#include <wbuf_private.h>
#pragma warning(disable:4100)   // unreferenced formal parameter
#endif

#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif

int ol_cfg_is_high_latency(ol_soc_handle soc)
{
/* FIX THIS: later the selection of high-latency vs. low-latency
 * will need to be done at run-time rather than compile-time
 */
#if defined(CONFIG_HL_SUPPORT)
    return 1;
#else
    return 0;
#endif
}
qdf_export_symbol(ol_cfg_is_high_latency);

int  ol_cfg_is_cce_disable(ol_soc_handle psoc)
{
     ol_ath_soc_softc_t *soc = lmac_get_psoc_feature_ptr(
                                   (struct wlan_objmgr_psoc *)psoc);
     if (!soc){
         qdf_print("%s: soc pointer is NULL.",__func__);
         return -EINVAL;
     }
     if (soc->cce_disable)
         return 1;
     else
         return 0;
}

/* Temp until this is added to bmi_msg.h */
#define TARGET_TYPE_QCA8074   20
int ol_cfg_max_peer_id(ol_soc_handle psoc)
{
    /*
     * TBDXXX - this value must match the peer table
     * size allocated in FW
     */
    int peer_id, max_peer_id = 1;
    uint32_t num_vdev = 0;
    uint32_t target_type;

#if PEER_CACHEING_HOST_ENABLE
    int def_tgt_max_peer = (cfg_get((struct wlan_objmgr_psoc *)psoc,
            CFG_OL_LOW_MEM_SYSTEM))?(CFG_TGT_NUM_QCACHE_PEERS_MAX_LOW_MEM):
            (CFG_TGT_NUM_QCACHE_PEERS_MAX);
#else
    int def_tgt_max_peer = CFG_TGT_NUM_PEERS_MAX;
#endif

    target_type = lmac_get_tgt_type((struct wlan_objmgr_psoc *)psoc);
    if (target_type == TARGET_TYPE_QCA9984) {
        num_vdev = CFG_TGT_NUM_VDEV_QCA9984;
    } else if (target_type == TARGET_TYPE_IPQ4019) {
        num_vdev = CFG_TGT_NUM_VDEV_AR900B;
    } else if (target_type == TARGET_TYPE_AR900B) {
        num_vdev = CFG_TGT_NUM_VDEV_AR900B;
    } else if (target_type == TARGET_TYPE_QCA9888) {
        num_vdev = CFG_TGT_NUM_VDEV_QCA9888;
#ifdef QCA_WIFI_QCA8074
    } else if(target_type == TARGET_TYPE_QCA8074) {
        num_vdev = CFG_TGT_NUM_VDEV_QCA8074;
        def_tgt_max_peer = CFG_TGT_NUM_PEERS_QCA8074;
#endif
#ifdef QCA_WIFI_QCA8074V2
    } else if(target_type == TARGET_TYPE_QCA8074V2) {
        num_vdev = CFG_TGT_NUM_VDEV_QCA8074;
        def_tgt_max_peer = CFG_TGT_NUM_PEERS_QCA8074;
#endif
#ifdef QCA_WIFI_QCA6018
    } else if(target_type == TARGET_TYPE_QCA6018) {
        num_vdev = CFG_TGT_NUM_VDEV_QCA8074;
        def_tgt_max_peer = CFG_TGT_NUM_PEERS_QCA8074;
#endif
    } else if(target_type == TARGET_TYPE_QCA6290) {
        num_vdev = CFG_TGT_NUM_VDEV_QCA6290;
        def_tgt_max_peer = CFG_TGT_NUM_PEERS_QCA6290;
    } else {
        num_vdev = CFG_TGT_NUM_VDEV_AR988X;
    }
    /* unfortunately, when we allocate the peer_id_to_obj_map,
     * we don't know yet if LARGE_AP is enabled or not. so, we
     * assume the worst and allocate more (PEERS_MAX instead
     * of PEERS).
     */
    peer_id = ( def_tgt_max_peer + num_vdev + 1 +
               CFG_TGT_WDS_ENTRIES) * CFG_TGT_NUM_PEER_AST;
    /* nearest power of 2 - as per FW algo */
    while (peer_id > max_peer_id) {
        max_peer_id <<= 1;
    }
    /* limit the size to target max ast size */
    if ( max_peer_id > CFG_TGT_MAX_AST_TABLE_SIZE ) {
        max_peer_id = CFG_TGT_MAX_AST_TABLE_SIZE;
    }
    return max_peer_id + CFG_TGT_AST_SKID_LIMIT - 1;
}
qdf_export_symbol(ol_cfg_max_peer_id);

int ol_cfg_max_vdevs(ol_pdev_handle pdev)
{
    struct ol_ath_softc_net80211 *scn =
        (struct ol_ath_softc_net80211 *)pdev;
    uint32_t num_vdev = 0;
    uint32_t target_type;

    target_type = lmac_get_tgt_type(scn->soc->psoc_obj);
    if (target_type == TARGET_TYPE_QCA9984) {
        num_vdev = CFG_TGT_NUM_VDEV_QCA9984;
    } else if (target_type == TARGET_TYPE_IPQ4019) {
        num_vdev = CFG_TGT_NUM_VDEV_AR900B;
    } else if (target_type == TARGET_TYPE_AR900B) {
        num_vdev = CFG_TGT_NUM_VDEV_AR900B;
    } else if (target_type == TARGET_TYPE_QCA9888) {
        num_vdev = CFG_TGT_NUM_VDEV_QCA9888;
    } else {
        num_vdev = CFG_TGT_NUM_VDEV_AR988X;
    }
    return num_vdev;
}

int ol_cfg_rx_pn_check(ol_pdev_handle pdev)
{
    return 1;
}

int ol_cfg_rx_fwd_check(ol_pdev_handle pdev)
{
    return 1;
}

int ol_cfg_rx_fwd_inter_bss(ol_pdev_handle pdev)
{
    return 0;
}

enum wlan_frm_fmt ol_cfg_frame_type(ol_pdev_handle pdev)
{
#ifdef ATHR_WIN_NWF
    return wlan_frm_fmt_native_wifi;
#else
    return wlan_frm_fmt_802_3;
#endif
}

enum htt_cmn_pkt_type ol_cfg_pkt_type(ol_pdev_handle pdev)
{
#if defined(ATHR_WIN_NWF)
    return htt_cmn_pkt_type_native_wifi;
#else
    return htt_cmn_pkt_type_ethernet;
#endif
}

int ol_cfg_max_thruput_mbps(ol_soc_handle psoc)
{
#ifdef QCA_LOWMEM_PLATFORM
    return 400;
#else
    return 800;
#endif
}

int ol_cfg_netbuf_frags_max(ol_soc_handle psoc)
{
#ifdef ATHR_WIN_NWF
    return CVG_NBUF_MAX_FRAGS;
#else
    return 1;
#endif
}

int ol_cfg_tx_free_at_download(ol_soc_handle psoc)
{
    return 0;
}

int
ol_cfg_target_tx_credit(ol_soc_handle ol_soc)
{
    struct wlan_objmgr_psoc *psoc_obj = (struct wlan_objmgr_psoc *) ol_soc;
    uint32_t vow_config;
    u_int16_t vow_max_sta;
    u_int16_t vow_max_desc_persta;
    uint32_t num_tgt_desc = 0;
    struct target_psoc_info *tgt_psoc_info;

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
            psoc_obj);
    if(tgt_psoc_info == NULL) {
        qdf_print("%s: target_psoc_info is null ", __func__);
        return num_tgt_desc;
    }
    vow_config = wlan_ucfg_get_config_param(psoc_obj, VOW_CONFIG);
    vow_max_sta = ((vow_config) & 0xffff0000) >> 16;
    vow_max_desc_persta = ((vow_config) & 0x0000ffff);

    if (lmac_is_target_ar900b(psoc_obj)) {
        num_tgt_desc = CFG_TGT_NUM_MSDU_DESC_AR900B;
    }
#if QCA_AIRTIME_FAIRNESS
    else if(target_if_atf_get_mode(psoc_obj) && !(ol_target_lithium(psoc_obj))) {
        num_tgt_desc = target_if_atf_get_num_msdu_desc(psoc_obj);
    }
#endif
    else {
        num_tgt_desc = CFG_TGT_NUM_MSDU_DESC_AR988X;
    }

#if defined(CONFIG_HL_SUPPORT)
    return num_tgt_desc;
#else

    if ((vow_max_sta * vow_max_desc_persta) > TOTAL_VOW_ALLOCABLE) {
        u_int16_t vow_unrsvd_sta_num, vow_rsvd_num;

        vow_rsvd_num = TOTAL_VOW_ALLOCABLE/vow_max_desc_persta;

        vow_unrsvd_sta_num = vow_max_sta - vow_rsvd_num;

        if( (vow_unrsvd_sta_num * vow_max_desc_persta) <= VOW_DESC_GRAB_MAX ) {

            vow_max_sta = vow_rsvd_num;
        }
        else {
            A_ASSERT(0);
        }
    }

    if (target_psoc_get_max_descs(tgt_psoc_info)) {
        return target_psoc_get_max_descs(tgt_psoc_info);
    } else {
        return (num_tgt_desc + (vow_max_sta*vow_max_desc_persta));
    }

#endif
}
qdf_export_symbol(ol_cfg_target_tx_credit);

int ol_cfg_tx_download_size(ol_pdev_handle pdev)
{
    /* FIX THIS - use symbolic constants rather than hard-coded numbers */
    /* 802.1Q and SNAP / LLC headers are accounted for elsewhere */
#if defined(CONFIG_HL_SUPPORT)
    return 1500; /* include payload, not only desc */
#else
    /* FIX THIS - may need more bytes in the future for deeper packet inspection */
    /* Need to change HTT_LL_TX_HDR_SIZE_IP accordingly */
    return 38; /* include payload, up to the end of UDP header for IPv4 case */
#endif
}

int ol_cfg_rx_host_defrag_timeout_duplicate_check(ol_pdev_handle pdev)
{
#if CFG_TGT_DEFAULT_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK
    return 1;
#else
    return 0;
#endif
}

qdf_export_symbol(ol_cfg_netbuf_frags_max);
qdf_export_symbol(ol_cfg_rx_pn_check);
qdf_export_symbol(ol_cfg_max_thruput_mbps);
qdf_export_symbol(ol_cfg_tx_free_at_download);
qdf_export_symbol(ol_cfg_pkt_type);
qdf_export_symbol(ol_cfg_rx_host_defrag_timeout_duplicate_check);
qdf_export_symbol(ol_cfg_frame_type);
qdf_export_symbol(ol_cfg_rx_fwd_check);
qdf_export_symbol(ol_cfg_tx_download_size);

#if defined (ATHR_WIN_NWF)
#pragma warning(default:4100)   // unreferenced formal parameter
#endif
