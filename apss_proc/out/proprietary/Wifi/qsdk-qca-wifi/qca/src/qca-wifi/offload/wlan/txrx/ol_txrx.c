/*
 * Copyright (c) 2011-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*=== includes ===*/
/* header files for OS primitives */
#include <osdep.h>         /* u_int32_t, etc. */
#include <qdf_mem.h>    /* qdf_mem_malloc,free */
#include <qdf_types.h>  /* qdf_device_t, qdf_print */
#include <qdf_lock.h>   /* qdf_spinlock */
#include <qdf_atomic.h> /* qdf_atomic_read */
#include "osif_private.h"
#include <target_type.h>

#if QCA_AIRTIME_FAIRNESS
#include "target_if_atf.h"
#endif
#ifdef WLAN_FEATURE_FASTPATH
#include <hif.h> /* struct hif_softc */
#endif
#include <osdep.h>
#include "osif_private.h"
/* header files for utilities */
#include <queue.h>         /* TAILQ */

/* header files for configuration API */
#include <ol_cfg.h>        /* ol_cfg_is_high_latency */
#include <ol_if_athvar.h>
#include "wlan_defs.h"
#include "ol_ath.h"
#include <wlan_utility.h>

/* header files for HTT API */
#include <ol_htt_api.h>
#include <ol_htt_tx_api.h>

/* header files for our own APIs */
#include <cdp_txrx_cmn_struct.h>
#include <ol_txrx_dbg.h>
#include <ol_txrx_ctrl_api.h>
#include <ol_txrx_osif_api.h>
#include <dp_ratetable.h>

/* header files for our internal definitions */
#include "dp_cal_client_api.h"
#include <ol_txrx_internal.h>  /* TXRX_ASSERT, etc. */
#include <ol_txrx_types.h>     /* ol_txrx_pdev_t, etc. */
#include <ol_tx.h>             /* ol_tx_hl, ol_tx_ll */
#include <ol_rx.h>             /* ol_rx_deliver */
#include <ol_txrx_peer_find.h> /* ol_txrx_peer_find_attach, etc. */
#include <ol_rx_pn.h>          /* ol_rx_pn_check, etc. */
#include <ol_rx_fwd.h>         /* ol_rx_fwd_check, etc. */
#include <ol_tx_desc.h>        /* ol_tx_desc_frame_free */
#include <wdi_event.h>         /* WDI events */
#include <ol_ratectrl_11ac_if.h>    /* attaching/freeing rate-control contexts */
#include <ol_txrx_api_internal.h>
#include <ol_if_txrx_handles.h>
#include <cdp_txrx_ops.h>
#include <pktlog_ac_i.h>
#include <htt.h>              /* HTT_TX_EXT_TID_MGMT */
#include <htt_internal.h>     /* */
#include <htt_types.h>        /* htc_endpoint */
#if QCA_SUPPORT_SON
#include <wlan_son_pub.h>
#endif
#include "bmi.h"
#include "hif.h"
#include <wlan_lmac_if_api.h>
/*=== local definitions ===*/
#ifndef OL_TX_AVG_FRM_BYTES
#define OL_TX_AVG_FRM_BYTES 1000
#endif

#ifndef OL_TX_DESC_POOL_SIZE_MIN
#define OL_TX_DESC_POOL_SIZE_MIN 500
#endif

#ifndef OL_TX_DESC_POOL_SIZE_MAX
#define OL_TX_DESC_POOL_SIZE_MAX 5000
#endif

#if QCA_PARTNER_DIRECTLINK_RX
#define QCA_PARTNER_DIRECTLINK_OL_TXRX 1
#include "ath_carr_pltfrm.h"
#undef QCA_PARTNER_DIRECTLINK_OL_TXRX
#endif

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_private.h>
#include <osif_nss_wifiol_if.h>
#include <osif_nss_wifiol_vdev_if.h>
#include <osif_nss_wifiol_2_0_if.h>
#endif

#include <init_deinit_lmac.h>
#include "cfg_ucfg_api.h"

extern void ol_txrx_classify(struct ol_txrx_vdev_t *vdev, qdf_nbuf_t nbuf,
        enum txrx_direction dir, struct ol_txrx_nbuf_classify *nbuf_class);
/* header files for WMI event processing */

extern ar_handle_t ar_attach(int target_type);
extern void ar_detach(ar_handle_t arh);
extern int whal_mcs_to_kbps(int preamb, int mcs, int htflag, int gintval);

/*=== function definitions ===*/
static int
ol_tx_desc_pool_size(ol_soc_handle ctrl_psoc)
{
    int desc_pool_size;
    int steady_state_tx_lifetime_ms;
    int safety_factor;

    /*
     * Steady-state tx latency:
     *     roughly 1-2 ms flight time
     *   + roughly 1-2 ms prep time,
     *   + roughly 1-2 ms target->host notification time.
     * = roughly 6 ms total
     * Thus, steady state number of frames =
     * steady state max throughput / frame size * tx latency, e.g.
     * 1 Gbps / 1500 bytes * 6 ms = 500
     *
     */
    steady_state_tx_lifetime_ms = 6;

    safety_factor = 8;

    desc_pool_size =
        ol_cfg_max_thruput_mbps(ctrl_psoc) *
        1000 /* 1e6 bps/mbps / 1e3 ms per sec = 1000 */ /
        (8 * OL_TX_AVG_FRM_BYTES) *
        steady_state_tx_lifetime_ms *
        safety_factor;

    /* minimum */
    if (desc_pool_size < OL_TX_DESC_POOL_SIZE_MIN) {
        desc_pool_size = OL_TX_DESC_POOL_SIZE_MIN;
    }
    /* maximum */
    if (desc_pool_size > OL_TX_DESC_POOL_SIZE_MAX) {
        desc_pool_size = OL_TX_DESC_POOL_SIZE_MAX;
    }
    return desc_pool_size;
}

static ol_txrx_prot_an_handle
ol_txrx_prot_an_attach(struct ol_txrx_pdev_t *pdev, const char *name)
{
    ol_txrx_prot_an_handle base;

    base = OL_TXRX_PROT_AN_CREATE_802_3(pdev, name);
    if (base) {
        ol_txrx_prot_an_handle ipv4;
        ol_txrx_prot_an_handle ipv6;
        ol_txrx_prot_an_handle arp;

        arp = OL_TXRX_PROT_AN_ADD_ARP(pdev, base);
        ipv6 = OL_TXRX_PROT_AN_ADD_IPV6(pdev, base);
        ipv4 = OL_TXRX_PROT_AN_ADD_IPV4(pdev, base);

        if (ipv4) {
            /* limit TCP printouts to once per 5 sec */
            OL_TXRX_PROT_AN_ADD_TCP(
                    pdev, ipv4, TXRX_PROT_ANALYZE_PERIOD_TIME, 0x0, 5000);
            /* limit UDP printouts to once per 5 sec */
            OL_TXRX_PROT_AN_ADD_UDP(
                    pdev, ipv4, TXRX_PROT_ANALYZE_PERIOD_TIME, 0x3, 5000);
            /* limit ICMP printouts to two per sec */
            OL_TXRX_PROT_AN_ADD_ICMP(
                    pdev, ipv4, TXRX_PROT_ANALYZE_PERIOD_TIME, 0x0, 500);
        }
        /* could add TCP, UDP, and ICMP for IPv6 too */
    }
    return base;
}

#if defined(WLAN_FEATURE_FASTPATH) && PEER_FLOW_CONTROL
int
ol_tx_pflow_ctrl_init(struct ol_txrx_pdev_t *pdev)
{
    int i, tid;
    uint32_t tgt_type;

    /* Peer / TID Q related members */
    pdev->pflow_ctl_min_threshold = OL_TX_PFLOW_CTRL_MIN_THRESHOLD;
    lmac_get_pdev_target_type((struct wlan_objmgr_pdev *)pdev->ctrl_pdev, &tgt_type);
    if (tgt_type == TARGET_TYPE_IPQ4019){
        pdev->pflow_ctl_min_queue_len = OL_TX_PFLOW_CTRL_MIN_QUEUE_LEN_IPQ4019;
        pdev->pflow_ctl_max_queue_len = OL_TX_PFLOW_CTRL_MAX_QUEUE_LEN_IPQ4019;
        pdev->pflow_ctl_max_buf_global = OL_TX_PFLOW_CTRL_MAX_BUF_GLOBAL_IPQ4019;
    } else {
        pdev->pflow_ctl_min_queue_len = OL_TX_PFLOW_CTRL_MIN_QUEUE_LEN;
        pdev->pflow_ctl_max_queue_len = OL_TX_PFLOW_CTRL_MAX_QUEUE_LEN;
#if MIPS_LOW_PERF_SUPPORT
        pdev->pflow_ctl_max_buf_global = OL_TX_PFLOW_CTRL_MAX_BUF_GLOBAL;
#else
        /* Initialize with the lowest level */
        pdev->pflow_ctl_max_buf_global = OL_TX_PFLOW_CTRL_MAX_BUF_0;
#endif
    }

    /* Carrier VOW Override Configuration section */
    if (pdev->carrier_vow_config) {
        pdev->pflow_ctl_max_buf_global = OL_TX_PFLOW_CARRIER_VOW_MAX_BUF_GLOBAL;
        pdev->pflow_ctl_max_queue_len = OL_TX_PFLOW_CARRIER_VOW_MAX_Q_GLOBAL;
        qdf_print("Carrier VOW Config Enabled : Max Buf %d Max Q %d Configured\n",
                pdev->pflow_ctl_max_buf_global, pdev->pflow_ctl_max_queue_len);
    }

    pdev->pflow_cong_ctrl_timer_interval = OL_TX_PFLOW_CTRL_CONG_CTRL_TIMER_MS;
    pdev->pflow_ctl_stats_timer_interval = OL_TX_PFLOW_CTRL_STATS_TIMER_MS;

    pdev->pflow_ctl_global_queue_cnt = 0;
    pdev->pflow_ctl_total_dequeue_cnt = 0;
    pdev->pflow_ctl_total_dequeue_byte_cnt  = 0;
    pdev->pflow_ctl_desc_count = 0;

#if PEER_FLOW_CONTROL_HOST_SCHED
    qdf_mem_zero(pdev->pflow_ctl_next_peer_idx, 4 * OL_TX_PFLOW_CTRL_MAX_TIDS);
#endif
    qdf_mem_zero(pdev->pflow_ctl_active_peer_map,
            4 * (OL_TX_PFLOW_CTRL_MAX_TIDS * (OL_TXRX_MAX_PEER_IDS >> 5)));

    for (i = 0; i < OL_TXRX_MAX_PEER_IDS; i++) {

        for (tid = 0; tid < OL_TX_PFLOW_CTRL_MAX_TIDS; tid++) {
            pdev->pflow_ctl_queue_max_len[i][tid] = pdev->pflow_ctl_max_queue_len;
        }

    }

    qdf_timer_init(pdev->osdev, &pdev->pflow_ctl_cong_timer,
            ol_tx_pflow_ctrl_cong_ctrl_timer, (void *)pdev, QDF_TIMER_TYPE_WAKE_APPS);

    qdf_timer_mod(&pdev->pflow_ctl_cong_timer,
            OL_TX_PFLOW_CTRL_CONG_CTRL_TIMER_MS);

    qdf_timer_init(pdev->osdev, &pdev->pflow_ctl_stats_timer,
            ol_tx_pflow_ctrl_stats_timer, (void *)pdev, QDF_TIMER_TYPE_WAKE_APPS);

    /* Qmap related members */
    pdev->pmap_qdepth_flush_interval  = OL_TX_PFLOW_CTRL_QDEPTH_FLUSH_INTERVAL;
    pdev->pmap_rotting_timer_interval = OL_TX_PFLOW_CTRL_ROT_TIMER_MS;
    pdev->pmap_qdepth_flush_count     = 0;
    /* Default Mode 0 ON */
    pdev->pflow_ctrl_mode             = HTT_TX_MODE_PUSH_NO_CLASSIFY;
    qdf_print("Startup Mode-%d set", pdev->pflow_ctrl_mode);
    pdev->pflow_msdu_ttl              = OL_MSDU_DEFAULT_TTL/OL_TX_PFLOW_CTRL_CONG_CTRL_TIMER_MS;
    pdev->pflow_msdu_ttl_cnt          = 0;
    pdev->pflow_ttl_cntr              = 0;

    return 0;
}

void
ol_tx_pflow_ctrl_clean(struct ol_txrx_pdev_t *pdev)
{
    qdf_timer_stop(&pdev->pflow_ctl_cong_timer);
    qdf_timer_free(&pdev->pflow_ctl_cong_timer);
    qdf_timer_free(&pdev->pflow_ctl_stats_timer);
}

#endif

#define BUF_PEER_ENTRIES	32

int
ol_txrx_mempools_attach(void *ctrl_pdev_handle)
{
    ol_pdev_handle ctrl_pdev = (ol_pdev_handle)ctrl_pdev_handle;
    ol_ath_soc_softc_t *soc =
        (ol_ath_soc_softc_t *)ctrl_pdev;

    if (qdf_mempool_init(soc->qdf_dev, &soc->mempool_ol_ath_peer,
                (BUF_PEER_ENTRIES +  soc->max_vaps + soc->max_clients), sizeof(struct ol_txrx_peer_t), 0)) {
        soc->mempool_ol_ath_peer = NULL;
        printk(KERN_ERR "%s: ol_ath_peer memory pool init failed\n", __func__);
        return -ENOMEM;
    }

    return 0;
}

int ol_soc_pdev_attach(
    struct ol_txrx_psoc_t *dp_soc,
    void *ctrl_psoc_handle,
    HTC_HANDLE htc_pdev,
    qdf_device_t osdev)
{
    ol_soc_handle ctrl_psoc = (ol_soc_handle)ctrl_psoc_handle;
    ol_txrx_soc_handle soc = (ol_txrx_soc_handle)dp_soc;
    int desc_pool_size;
    ol_ath_soc_softc_t *ol_soc;
    struct hif_opaque_softc *sc;

    ol_soc = (ol_ath_soc_softc_t *)
                  lmac_get_psoc_feature_ptr((struct wlan_objmgr_psoc *)ctrl_psoc);

    TXRX_ASSERT2(ol_soc != NULL);

    sc = lmac_get_ol_hif_hdl((struct wlan_objmgr_psoc *)ctrl_psoc);

    dp_soc->pflow_msdu_ttl = OL_MSDU_DEFAULT_TTL/OL_TX_PFLOW_CTRL_CONG_CTRL_TIMER_MS;
    dp_soc->pflow_cong_ctrl_timer_interval = OL_TX_PFLOW_CTRL_CONG_CTRL_TIMER_MS;

    /* init LL/HL cfg here */
    dp_soc->is_high_latency = ol_cfg_is_high_latency(ctrl_psoc);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    qdf_info("soc=%pK %pK %d ",ol_soc,ol_soc->nss_soc.nss_sctx,ol_soc->nss_soc.nss_wifiol_id);
    if (ol_soc->nss_soc.nss_sctx){
        dp_soc->soc_nss_wifiol_id = ol_soc->nss_soc.nss_wifiol_id;
        dp_soc->soc_nss_wifiol_ctx = ol_soc->nss_soc.nss_sctx;
    }else {
        dp_soc->soc_nss_wifiol_id = -1;
        dp_soc->soc_nss_wifiol_ctx = NULL;
    }
#endif


    if (ol_cfg_is_high_latency(ctrl_psoc)) {
        desc_pool_size = ol_tx_desc_pool_size(ctrl_psoc);
    } else {
        /* In LL data path having more descriptors than target raises a
         * race condition with the current target credit implememntation.
         */
        desc_pool_size = ol_cfg_target_tx_credit(ctrl_psoc);
    }
    dp_soc->desc_pool_size = desc_pool_size;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    ol_soc->buff_thresh.pool_size = desc_pool_size;
    ol_soc->buff_thresh.ald_buffull_wrn = 0;
#endif

    if (lmac_get_tgt_type((struct wlan_objmgr_psoc *)ctrl_psoc))
        dp_soc->is_ar900b = lmac_is_target_ar900b((struct wlan_objmgr_psoc *)ctrl_psoc);

    dp_soc->htt_pdev = htt_attach(
        soc, ctrl_psoc, sc, htc_pdev, dp_soc->arh, osdev, desc_pool_size);
    if (!dp_soc->htt_pdev) {
        return -1;
    }

    return 0;
}
/* ol_tx_update_peer_stats: update legacy per peer stats into cdp_peer_stats
 *
 */
#if ENHANCED_STATS
#define PPDU_STATS_TX_ERROR_MASK 0xFEC
int ol_tx_update_peer_stats (struct ol_txrx_pdev_t *txrx_pdev, uint32_t* msg_word, uint32_t msg_len)
{
    struct wlan_objmgr_pdev *pdev;
    struct ol_txrx_peer_t *peer;
    u_int8_t num_mpdus = 0;
    uint16_t start_seq_num;
    u_int16_t num_msdus;
    u_int64_t byte_cnt = 0;
    struct ol_txrx_vdev_t *vdev = NULL;
    struct ieee80211vap *vap;
#if ATH_DATA_TX_INFO_EN
    uint8_t tid = 0, ac = 0;
#endif
    uint8_t bw;
    uint16_t mcs;
    uint32_t rate_code;
    uint8_t  nss, preamble;
    uint32_t ratekbps;
    uint32_t version = 0;
    uint32_t version2 = 0;
    uint32_t status = 0;
    uint8_t tx_status = 0;
    uint8_t peer_id = 0;
    uint32_t rix = 0;
    struct cdp_tx_completion_ppdu *ppdu_info;
    ppdu_common_stats_v3 *ppdu_stats = NULL;
    struct ol_ath_softc_net80211 *scn;
    qdf_nbuf_t ppdu_nbuf;
    pdev = (struct wlan_objmgr_pdev *)txrx_pdev->ctrl_pdev;
    scn = (struct ol_ath_softc_net80211 *)lmac_get_pdev_feature_ptr(pdev);

#if ENHANCED_STATS && defined(WLAN_FEATURE_FASTPATH)
    ppdu_stats = (ppdu_common_stats_v3 *)ol_txrx_get_en_stats_base(
            (struct cdp_pdev *)txrx_pdev, msg_word, msg_len, &version, &status);
#endif
    if (!ppdu_stats) {
       return A_EINVAL;
    }
    txrx_pdev->tx_stats.peer_id = HTT_T2H_EN_STATS_PEER_ID_GET(ppdu_stats);
    peer_id = txrx_pdev->tx_stats.peer_id;

        switch (version) {
#if ATH_DATA_TX_INFO_EN

            case HTT_T2H_EN_STATS_TYPE_COMMON:
                version2 = PPDU_STATS_VERSION_1;
                break;

            case HTT_T2H_EN_STATS_TYPE_COMMON_V2:
                version2 = PPDU_STATS_VERSION_2;
                break;

            case HTT_T2H_EN_STATS_TYPE_COMMON_V3:
                version2 = PPDU_STATS_VERSION_3;
                break;
#endif

            default:
                IEEE80211_DPRINTF_IC(wlan_pdev_get_mlme_ext_obj(pdev), IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_ANY, "Invalid stats version received from FW\n");
                break;
        }

        peer = (HTT_T2H_EN_STATS_PEER_ID_GET(ppdu_stats) == HTT_INVALID_PEER) ?
            NULL : txrx_pdev->peer_id_to_obj_map[HTT_T2H_EN_STATS_PEER_ID_GET(ppdu_stats)];
        if (peer) {
            /* extract the seq_num  */
            start_seq_num = HTT_T2H_EN_STATS_STARTING_SEQ_NUM_GET(ppdu_stats);
            vdev = peer->vdev;
            vap = ol_ath_pdev_vap_get(pdev, vdev->vdev_id);
            if (!vap) {
                return A_ERROR;
            }
        if (HTT_T2H_EN_STATS_PKT_TYPE_GET(ppdu_stats) == TX_FRAME_TYPE_BEACON) {
#ifdef QCA_SUPPORT_CP_STATS
            pdev_cp_stats_tx_beacon_inc(pdev, 1);
#endif
        } else if (HTT_T2H_EN_STATS_PKT_TYPE_GET(ppdu_stats) == TX_FRAME_TYPE_DATA) {
            /* We don't Check for the wrap around condition, we are
             *   interested only in seeing if we have advanced the
             *   block ack window.
             */
             if (txrx_pdev->tx_stats.seq_num != start_seq_num) {
                 txrx_pdev->pdev_data_stats.tx.tx_bawadv++;
             }
             /* cache the seq_num in the structure for the next ppdu */
             txrx_pdev->tx_stats.seq_num = start_seq_num;

             ppdu_nbuf = qdf_nbuf_alloc(NULL,
                     sizeof(struct cdp_tx_completion_ppdu), 0, 0, FALSE);
             if (!ppdu_nbuf)
                 return A_ERROR;
             ppdu_info = (struct cdp_tx_completion_ppdu *)qdf_nbuf_data(ppdu_nbuf);
             qdf_mem_zero(ppdu_info, sizeof(*ppdu_info));
             txrx_pdev->ppdu_tx_stats.peer_id = peer_id;
             qdf_mem_copy(txrx_pdev->ppdu_tx_stats.mac_addr, peer->mac_addr.raw,
                     QDF_MAC_ADDR_SIZE);

             num_mpdus = HTT_T2H_EN_STATS_MPDUS_QUEUED_GET(ppdu_stats) - HTT_T2H_EN_STATS_MPDUS_FAILED_GET(ppdu_stats);
             num_msdus = HTT_T2H_EN_STATS_MSDU_SUCCESS_GET(ppdu_stats);

             byte_cnt = ppdu_stats->success_bytes;
             rate_code = HTT_T2H_EN_STATS_RATE_GET(ppdu_stats);
             bw = HTT_T2H_EN_STATS_BW_IDX_GET(ppdu_stats);

             preamble = GET_HW_RATECODE_PREAM(rate_code);
             mcs = GET_HW_RATECODE_RATE(rate_code);
             nss = GET_HW_RATECODE_NSS(rate_code);
             ratekbps = dp_getrateindex(CDP_SGI_0_8_US, mcs, nss, preamble, bw, &rix);

#if ATH_DATA_TX_INFO_EN || defined(QCA_SUPPORT_RDK_STATS)
             txrx_pdev->ppdu_tx_stats.rix = rix;
             txrx_pdev->ppdu_tx_stats.tx_ratekbps = ratekbps;
             txrx_pdev->ppdu_tx_stats.tx_ratecode = rate_code;
             txrx_pdev->ppdu_tx_stats.bw = bw;
             txrx_pdev->ppdu_tx_stats.mcs = mcs;
             txrx_pdev->ppdu_tx_stats.preamble = preamble;
             txrx_pdev->ppdu_tx_stats.mpdu_success = num_mpdus;
             txrx_pdev->ppdu_tx_stats.mpdu_failed = HTT_T2H_EN_STATS_MPDUS_FAILED_GET(ppdu_stats);
             txrx_pdev->ppdu_tx_stats.mpdu_tried_ucast = HTT_T2H_EN_STATS_MPDUS_TRIED_GET(ppdu_stats);
             txrx_pdev->ppdu_tx_stats.success_msdus = num_msdus;
             txrx_pdev->ppdu_tx_stats.success_bytes = byte_cnt;
             txrx_pdev->ppdu_tx_stats.duration = ppdu_stats->ppdu_duration;
             txrx_pdev->ppdu_tx_stats.long_retries = HTT_T2H_EN_STATS_LONG_RETRIES_GET(ppdu_stats);
             txrx_pdev->ppdu_tx_stats.is_ampdu = HTT_T2H_EN_STATS_IS_AGGREGATE_GET(ppdu_stats);
             txrx_pdev->ppdu_tx_stats.completion_status = HTT_T2H_EN_STATS_TX_STATUS_GET(ppdu_stats);

             if (version2 == PPDU_STATS_VERSION_3) {

                 txrx_pdev->ppdu_tx_stats.tx_duration = ppdu_stats->ppdu_ack_timestamp;
                 txrx_pdev->ppdu_tx_stats.start_seq = start_seq_num;
                 txrx_pdev->ppdu_tx_stats.enq_bitmap[CDP_BA_256_BIT_MAP_SIZE_DWORDS - 1] = ppdu_stats->ppdu_bmap_enqueued_hi;
                 txrx_pdev->ppdu_tx_stats.enq_bitmap[CDP_BA_256_BIT_MAP_SIZE_DWORDS - 8] = ppdu_stats->ppdu_bmap_enqueued_lo;
                 txrx_pdev->ppdu_tx_stats.ba_bitmap[CDP_BA_256_BIT_MAP_SIZE_DWORDS - 1] = ppdu_stats->ppdu_bmap_tried_hi;
                 txrx_pdev->ppdu_tx_stats.ba_bitmap[CDP_BA_256_BIT_MAP_SIZE_DWORDS - 8] = ppdu_stats->ppdu_bmap_tried_lo;
                 txrx_pdev->ppdu_tx_stats.failed_bitmap[CDP_BA_256_BIT_MAP_SIZE_DWORDS - 1] = ppdu_stats->ppdu_bmap_failed_hi;
                 txrx_pdev->ppdu_tx_stats.failed_bitmap[CDP_BA_256_BIT_MAP_SIZE_DWORDS - 8] = ppdu_stats->ppdu_bmap_failed_lo;
             }
#endif
             ppdu_info->frame_type = CDP_PPDU_FTYPE_DATA;
             ppdu_info->num_users = 1;
             ppdu_info->num_mpdu = num_mpdus;
             ppdu_info->num_msdu = num_msdus;
             ppdu_info->tx_duration = ppdu_stats->ppdu_ack_timestamp;
             txrx_pdev->ppdu_tx_stats.cookie = peer->wlanstats_ctx;
             qdf_mem_copy(&ppdu_info->user[0], &txrx_pdev->ppdu_tx_stats, sizeof (struct cdp_tx_completion_ppdu_user));
             wdi_event_handler(WDI_EVENT_TX_PPDU_DESC, txrx_pdev,
                               ppdu_nbuf, peer_id, status);
             qdf_mem_zero(&txrx_pdev->ppdu_tx_stats, sizeof (struct cdp_tx_completion_ppdu_user));

                if (peer->bss_peer) {
                    peer->stats.tx.mcast.num += num_msdus;
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
                    peer->stats.tx.mcast.bytes += byte_cnt;
#endif
                } else {
                    peer->stats.tx.ucast.num += num_msdus;
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
                    peer->stats.tx.ucast.bytes += byte_cnt;
#endif
                    peer->stats.tx.tx_success.num += num_msdus;
                    peer->stats.tx.tx_success.bytes += byte_cnt;
                }
                peer->stats.tx.comp_pkt.num +=num_msdus;
                peer->stats.tx.comp_pkt.bytes += byte_cnt;
                peer->stats.tx.retries += HTT_T2H_EN_STATS_LONG_RETRIES_GET(ppdu_stats);
                /* ack rssi of separate chains */
                RSSI_CHAIN_PRI20(ppdu_stats->rssi[0], peer->stats.tx.rssi_chain[0]);
                RSSI_CHAIN_PRI20(ppdu_stats->rssi[1], peer->stats.tx.rssi_chain[1]);
                RSSI_CHAIN_PRI20(ppdu_stats->rssi[2], peer->stats.tx.rssi_chain[2]);
                RSSI_CHAIN_PRI20(ppdu_stats->rssi[3], peer->stats.tx.rssi_chain[3]);

                /* Mask out excessive retry error. Dont treat excessive retry as tx error */
                tx_status =  HTT_T2H_EN_STATS_TX_STATUS_GET(ppdu_stats);

#if ATH_DATA_TX_INFO_EN
                if ((version2 <= HTT_T2H_EN_STATS_MAX_VER) &&
                        (tx_status == 0)) {
                    tid = HTT_T2H_EN_STATS_TID_NUM_GET(ppdu_stats);
                    if (tid < 8) {
                        ac = TID_TO_WME_AC(tid);
                        peer->stats.tx.wme_ac_type[ac]++;
                    }
                }
#endif
                peer->stats.tx.comp_pkt.num +=num_msdus;
                peer->stats.tx.comp_pkt.bytes +=byte_cnt;
                tx_status &= PPDU_STATS_TX_ERROR_MASK;
                if (tx_status) {
                    peer->stats.tx.is_tx_no_ack.num++;
                }

                if (HTT_T2H_EN_STATS_IS_AGGREGATE_GET(ppdu_stats)) {
                    peer->stats.tx.ampdu_cnt++;
                }
                else {
                    peer->stats.tx.non_ampdu_cnt++;
                }
        } else if (HTT_T2H_EN_STATS_PKT_TYPE_GET(ppdu_stats) == TX_FRAME_TYPE_MGMT) {

            if (!peer->bss_peer) {
#ifdef QCA_SUPPORT_CP_STATS
                vdev_ucast_cp_stats_tx_mgmt_inc(vap->vdev_obj, 1);
#endif
            }

#ifdef QCA_SUPPORT_CP_STATS
          peer_cp_stats_tx_mgmt_inc((struct wlan_objmgr_peer *)peer->ctrl_peer, 1);
#endif
        }
        ol_ath_release_vap(vap);
        }
        return A_OK;
}
#endif /*ENHANCE_STATS*/

A_STATUS
ol_tx_pkt_log_event_handler(void *_pdev,
        void *data)
{
#ifndef REMOVE_PKT_LOG
    ol_pktlog_dev_t *pl_dev;
    struct ol_txrx_pdev_t *txrx_pdev = (struct ol_txrx_pdev_t *)_pdev;
    struct ol_txrx_peer_t *peer = NULL;
    struct wlan_objmgr_pdev *pdev;
    struct ol_ath_softc_net80211 *scn;
    u_int16_t log_type;
    uint32_t *pl_tgt_hdr;
    uint8_t gintval = 0;
    size_t pktlog_hdr_size;

    if (!txrx_pdev) {
        qdf_print("Invalid pdev in %s", __func__);
        return A_ERROR;
    }
    pdev = (struct wlan_objmgr_pdev *)txrx_pdev->ctrl_pdev;
    scn = (struct ol_ath_softc_net80211 *)lmac_get_pdev_feature_ptr(pdev);
    qdf_assert(txrx_pdev->pl_dev);
    qdf_assert(data);

    pl_dev = txrx_pdev->pl_dev;
    pktlog_hdr_size = pl_dev->pktlog_hdr_size;
    pl_tgt_hdr = (uint32_t *)data;

    log_type =  (*(pl_tgt_hdr + OL_PKTLOG_STATS_HDR_LOG_TYPE_OFFSET) &
                                OL_PKTLOG_STATS_HDR_LOG_TYPE_MASK) >>
                                OL_PKTLOG_STATS_HDR_LOG_TYPE_SHIFT;
    if (log_type == OL_PKTLOG_STATS_TYPE_TX_CTRL) {
        int frame_type;
        uint8_t is_aggr;
#if defined(CONFIG_AR900B_SUPPORT) || defined(CONFIG_AR9888_SUPPORT)
        u_int64_t num_msdus;
        u_int64_t byte_cnt = 0;
#endif
        void *tx_ppdu_ctrl_desc;
        int series_bw_offset;
        uint8_t sgi_series;
        u_int32_t start_seq_num;
        int       peer_id;
        int max_peers;
        struct ol_txrx_psoc_t *dp_soc = (struct ol_txrx_psoc_t *) txrx_pdev->soc;

        tx_ppdu_ctrl_desc = (void *)data + txrx_pdev->pl_dev->pktlog_hdr_size;
        /*  The peer_id is filled in the target in the ppdu_done function, the
         *              peer_id istaken from the tid structure
         *                      */
        peer_id = *((u_int32_t *)tx_ppdu_ctrl_desc + OL_TX_PEER_ID_OFFSET);
        max_peers = ol_cfg_max_peer_id(dp_soc->psoc_obj) + 1;
        if (peer_id > max_peers) {
            IEEE80211_DPRINTF_IC(wlan_pdev_get_mlme_ext_obj(pdev), IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_ANY, "Peer ID Invalid\n");
            return -1;
        }

        txrx_pdev->tx_stats.peer_id = peer_id;
        peer = (peer_id == HTT_INVALID_PEER) ?
            NULL : txrx_pdev->peer_id_to_obj_map[peer_id];
        if (peer) {
            /* extract the seq_num  */
            start_seq_num = ((*((u_int32_t *)tx_ppdu_ctrl_desc + SEQ_NUM_OFFSET))
                                           & SEQ_NUM_MASK);
            /* We don't Check for the wrap around condition, we are
            *   interested only in seeing if we have advanced the
            *   block ack window.
            */
            if (txrx_pdev->tx_stats.seq_num != start_seq_num) {
                txrx_pdev->pdev_data_stats.tx.tx_bawadv++;
            }
            /* cache the seq_num in the structure for the next ppdu */
            txrx_pdev->tx_stats.seq_num = start_seq_num;
            /* cache the no_ack in the structure for the next ppdu */
            txrx_pdev->tx_stats.no_ack = ((*((u_int32_t *)tx_ppdu_ctrl_desc + TX_FRAME_OFFSET)) &
                                                TX_FRAME_TYPE_NOACK_MASK) >> TX_FRAME_TYPE_NOACK_SHIFT;

#if defined(CONFIG_AR900B_SUPPORT) || defined(CONFIG_AR9888_SUPPORT)
            num_msdus = peer->stats.tx.dot11_tx_pkts.num;
            byte_cnt = peer->stats.tx.dot11_tx_pkts.bytes;
            peer->stats.tx.comp_pkt.num += num_msdus;
            peer->stats.tx.comp_pkt.bytes += byte_cnt;
#endif
#if defined(CONFIG_AR900B_SUPPORT) || defined(CONFIG_AR9888_SUPPORT)
            if (peer->bss_peer) {
                peer->stats.tx.mcast.num += num_msdus;
                peer->stats.tx.mcast.bytes += byte_cnt;
            } else {
                peer->stats.tx.ucast.num += num_msdus;
                peer->stats.tx.ucast.bytes += byte_cnt;
                peer->stats.tx.tx_success.num += num_msdus;
                peer->stats.tx.tx_success.bytes += byte_cnt;
            }
            peer->stats.tx.comp_pkt.num += num_msdus;
            peer->stats.tx.comp_pkt.bytes += byte_cnt;

#endif
            peer->stats.tx.dot11_tx_pkts.num = 0;
            peer->stats.tx.dot11_tx_pkts.bytes = 0;
            peer->stats.tx.tx_failed = 0;
            is_aggr = ((*((u_int32_t *)tx_ppdu_ctrl_desc + TX_FRAME_OFFSET))
                    & TX_AMPDU_MASK) >> TX_AMPDU_SHIFT;
            if (is_aggr) {
                peer->stats.tx.ampdu_cnt++;
            }
            else {
                peer->stats.tx.non_ampdu_cnt++;
            }
        }
        frame_type = ((*((u_int32_t *)tx_ppdu_ctrl_desc + TX_FRAME_OFFSET))
                          & TX_FRAME_TYPE_MASK) >> TX_FRAME_TYPE_SHIFT;
        /*	Here the frame type is 3 for beacon frames, this is defined
            in the tx_ppdu_start.h
            Frame type indication.  Indicates what type of frame is
           	being sent.  Supported values:
            0: default
            1: Reserved (Used to be used for ATIM)
            2: PS-Poll
            3: Beacon
            4: Probe response
            5-15: Reserved
            <legal:0,2,3,4>
        */

        if (frame_type == 3) {
#ifdef QCA_SUPPORT_CP_STATS
            pdev_cp_stats_tx_beacon_inc(pdev, 1);
#endif
        }

        sgi_series = ((*((u_int32_t *)tx_ppdu_ctrl_desc + SGI_SERIES_OFFSET))
                           & SGI_SERIES_MASK) >> SGI_SERIES_SHIFT;
        switch (sgi_series) {
            case 0x1:
                series_bw_offset = SERIES_BW_START_OFFSET + 0 * SERIES_BW_SIZE;
            break;
            case 0x2:
                series_bw_offset = SERIES_BW_START_OFFSET + 1 * SERIES_BW_SIZE;
            break;
            case 0x4:
                series_bw_offset = SERIES_BW_START_OFFSET + 2 * SERIES_BW_SIZE;
            break;
            case 0x8:
                series_bw_offset = SERIES_BW_START_OFFSET + 3 * SERIES_BW_SIZE;
            break;
            case 0x10:
                series_bw_offset = SERIES_BW_START_OFFSET + 4 * SERIES_BW_SIZE;
            break;
            case 0x20:
                series_bw_offset = SERIES_BW_START_OFFSET + 5 * SERIES_BW_SIZE;
            break;
            case 0x40:
                series_bw_offset = SERIES_BW_START_OFFSET + 6 * SERIES_BW_SIZE;
            break;
            case 0x80:
                series_bw_offset = SERIES_BW_START_OFFSET + 7 * SERIES_BW_SIZE;
            break;
            default:
                series_bw_offset = 0;   /* The valid bw series s0/s1 bits are not set properly */
            break;
        }
        if (series_bw_offset) {
            gintval = ((*((u_int32_t *)tx_ppdu_ctrl_desc + series_bw_offset))
                               & SERIES_BW_MASK) >> SERIES_BW_SHIFT;
        }

    }
    if (log_type == OL_PKTLOG_STATS_TYPE_TX_STAT) {
        void *tx_ppdu_status_desc = (void *)data + pl_dev->pktlog_hdr_size;

        uint32_t ppdu_bmap_failed_lo;
        uint32_t ppdu_bmap_failed_hi;

        ppdu_bmap_failed_lo = (*((u_int32_t *)tx_ppdu_status_desc + BA_BMAP_LSB_OFFSET));
        ppdu_bmap_failed_hi = (*((u_int32_t *)tx_ppdu_status_desc + BA_BMAP_MSB_OFFSET));

#if ATH_DATA_TX_INFO_EN
        txrx_pdev->ppdu_tx_stats.failed_bitmap[CDP_BA_256_BIT_MAP_SIZE_DWORDS - 1] = ppdu_bmap_failed_hi;
        txrx_pdev->ppdu_tx_stats.failed_bitmap[CDP_BA_256_BIT_MAP_SIZE_DWORDS - 8] = ppdu_bmap_failed_lo;
#endif
    }
    if (log_type == OL_PKTLOG_STATS_TYPE_RC_UPDATE) {
        void *tx_ppdu_rcu_desc = (void *)data + pl_dev->pktlog_hdr_size;

        uint8_t  rate_idx;
        uint32_t ratekbps;

        rate_idx = ((*((u_int32_t *)tx_ppdu_rcu_desc + RATE_IDX_OFFSET)) & RATE_IDX_MASK);
        ratekbps = dp_rate_idx_to_kbps(rate_idx, gintval);

#if ATH_DATA_TX_INFO_EN
        txrx_pdev->ppdu_tx_stats.tx_rate = ratekbps;
#endif
        /* Peer_id is cached in PKTLOG_STATS_TYPE_TX_CTRL.
            It's not clean but we need to live with it for now. */
        peer = (txrx_pdev->tx_stats.peer_id == HTT_INVALID_PEER) ?
            NULL : txrx_pdev->peer_id_to_obj_map[txrx_pdev->tx_stats.peer_id];
        if (peer && peer->vdev) {
            if (ratekbps)
                dp_ath_rate_lpf(peer->stats.tx.avg_tx_rate, ratekbps);
            peer->stats.tx.avg_tx_rate = dp_ath_rate_out(peer->stats.tx.avg_tx_rate);
        }
    }
    if (log_type == OL_PKTLOG_STATS_TYPE_TX_STAT) {
        uint32_t no_ack;
        void *tx_ppdu_ctrl_desc = (void *)data + pl_dev->pktlog_hdr_size;
        void *tx_ppdu_status_desc = (void *)data + pl_dev->pktlog_hdr_size;
        /* cache the no_ack in the structure for the next ppdu */
        no_ack = ((*((u_int32_t *)tx_ppdu_ctrl_desc + TX_FRAME_OFFSET)) &
                TX_FRAME_TYPE_NOACK_MASK) >> TX_FRAME_TYPE_NOACK_SHIFT;
        if ( no_ack &&
            !((*((u_int32_t *)tx_ppdu_status_desc + TX_OK_OFFSET)) & OL_TX_OK_MASK)) {
            peer = (txrx_pdev->tx_stats.peer_id == HTT_INVALID_PEER) ?
                NULL : txrx_pdev->peer_id_to_obj_map[txrx_pdev->tx_stats.peer_id];
            if (peer && peer->vdev) {
                peer->stats.tx.is_tx_no_ack.num++;
            }
        }
    }
#endif
    return A_OK;
}

/* ol_txrx_aggregate_vdev_stats: consolidate VDEV stats
 * @vdev: DP VDEV handle
 *
 * return void
 */
void
ol_txrx_aggregate_vdev_stats(struct ol_txrx_vdev_t *vdev, struct cdp_vdev_stats *stats)
{
    struct ol_txrx_peer_t *peer = NULL;

    qdf_mem_set(&(stats->tx), sizeof(vdev->stats.tx), 0x0);
    qdf_mem_set(&(stats->rx), sizeof(vdev->stats.rx), 0x0);
    qdf_mem_set(&(stats->tx_i), sizeof(vdev->stats.tx_i), 0x0);
    qdf_mem_copy(stats, &vdev->stats, sizeof(vdev->stats));

    TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
        if (stats)
            ol_txrx_vdev_update_stats(stats, peer);
    }

}

/* ol_txrx_aggregate_pdev_stats: consolidate PDEV stats
 * @pdev: DP PDEV handle
 *
 * return void
 */
void
ol_txrx_aggregate_pdev_stats(struct ol_txrx_pdev_t *pdev)
{
    struct ol_txrx_vdev_t *vdev = NULL;
    struct cdp_vdev_stats *vdev_stats =
        qdf_mem_malloc(sizeof(struct cdp_vdev_stats));

    if (!vdev_stats)
        return;

    qdf_mem_set(&(pdev->pdev_stats.tx), sizeof(pdev->pdev_stats.tx), 0x0);
    qdf_mem_set(&(pdev->pdev_stats.rx), sizeof(pdev->pdev_stats.rx), 0x0);
    qdf_mem_set(&(pdev->pdev_stats.tx_i), sizeof(pdev->pdev_stats.tx_i), 0x0);

    qdf_spin_lock_bh(&pdev->tx_lock);
    TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
        ol_txrx_aggregate_vdev_stats(vdev, vdev_stats);
        ol_txrx_pdev_update_stats(pdev, vdev_stats);
        OL_TXRX_STATS_AGGR_PKT_PDEV(pdev, vdev, tx_i.rcvd);
        OL_TXRX_STATS_AGGR_PKT_PDEV(pdev, vdev, tx_i.dropped.desc_na);
        OL_TXRX_STATS_AGGR_PDEV(pdev, vdev, tx_i.raw.dma_map_error);
        OL_TXRX_STATS_AGGR_PDEV(pdev, vdev, tx_i.sg.dma_map_error);
        OL_TXRX_STATS_AGGR_PDEV(pdev, vdev, tx_i.dropped.dma_error);
        OL_TXRX_STATS_AGGR_PKT_PDEV(pdev, vdev, tx_i.sg.sg_pkt);
        OL_TXRX_STATS_AGGR_PKT_PDEV(pdev, vdev, tx_i.tso.tso_pkt);
        OL_TXRX_STATS_AGGR_PKT_PDEV(pdev, vdev, tx_i.tso.non_tso_pkts);
        OL_TXRX_STATS_AGGR_PKT_PDEV(pdev, vdev, tx_i.sg.non_sg_pkts);
        OL_TXRX_STATS_AGGR_PKT_PDEV(pdev, vdev, tx_i.sg.dropped_host);
        OL_TXRX_STATS_AGGR_PKT_PDEV(pdev, vdev, tx_i.tso.dropped_host);
        OL_TXRX_STATS_AGGR_PDEV(pdev, vdev, tx_i.mcast_en.mcast_pkt.num);
        OL_TXRX_STATS_AGGR_PDEV(pdev, vdev, tx_i.mcast_en.dropped_map_error);
        OL_TXRX_STATS_AGGR_PDEV(pdev, vdev, tx_i.mcast_en.fail_seg_alloc);
        OL_TXRX_STATS_AGGR_PDEV(pdev, vdev, tx_i.mcast_en.dropped_self_mac);
        OL_TXRX_STATS_AGGR_PDEV(pdev, vdev, tx_i.mcast_en.ucast);
        OL_TXRX_STATS_AGGR_PDEV(pdev, vdev, tx_i.mcast_en.dropped_send_fail);
    }
    qdf_spin_unlock_bh(&pdev->tx_lock);
    qdf_mem_free(vdev_stats);
}

#if ATH_SUPPORT_EXT_STAT
/* ol_iterate_update_peer_list - iterate over all the vdev
 * inside pdev and update peer stats
 * @pdev_hdl : pdev handle
 */
void ol_iterate_update_peer_list(void *pdev_hdl)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *) pdev_hdl;
    struct ol_txrx_vdev_t *vdev;
    struct ol_txrx_peer_t *peer;

    if(!pdev)
        return;

    TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
       TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
           dp_cal_client_update_peer_stats(&peer->stats);
        }
    }
}
#else
void ol_iterate_update_peer_list(void *pdev_hdl)
{
}
#endif

struct cdp_pdev *
ol_txrx_pdev_attach(
        ol_txrx_soc_handle soc,
        struct cdp_ctrl_objmgr_pdev *ctrl_pdev_handle,
        HTC_HANDLE htc_pdev,
        qdf_device_t osdev,
        uint8_t pdev_id)
{
    int i, desc_pool_size;
    struct ol_txrx_pdev_t *pdev;
    ol_pdev_handle ctrl_pdev = (ol_pdev_handle)ctrl_pdev_handle;
    struct ol_txrx_psoc_t *dp_soc = (struct ol_txrx_psoc_t*)soc;
    struct wlan_objmgr_psoc *psoc = ((struct ol_txrx_psoc_t *)soc)->psoc_obj;
    struct ol_ath_softc_net80211 *scn;
    struct hif_opaque_softc *sc;
    A_STATUS ret;
    uint32_t vdev_map_size;
#ifdef QCA_SUPPORT_RDK_STATS
    void *sojourn_buf;
#endif

    sc = lmac_get_ol_hif_hdl(dp_soc->psoc_obj);
    scn = (struct ol_ath_softc_net80211 *)
               lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)ctrl_pdev);

    pdev = qdf_mem_malloc(sizeof(*pdev));
    if (!pdev || !scn) {
        goto fail0;
    }
    qdf_mem_zero(pdev, sizeof(*pdev));

    pdev->p_osdev = scn->sc_osdev;
    pdev->scnctx = (void *)scn;
    qdf_nbuf_queue_init(&pdev->acnbufq);
    qdf_spinlock_create(&pdev->acnbufqlock);

    pdev->acqcnt_len = OL_TX_FLOW_CTRL_QUEUE_LEN;
    pdev->acqcnt = 0;
    pdev->ctrl_pdev = ctrl_pdev;
    pdev->targetdef = scn->soc->targetdef;
    pdev->soc = soc;
    pdev->pdev_id = pdev_id;
    pdev->carrier_vow_config = cfg_get(psoc, CFG_OL_CARRIER_VOW_CONFIG);
    pdev->cal_client_ctx = NULL;
#if defined(WLAN_FEATURE_FASTPATH) && PEER_FLOW_CONTROL
    ol_tx_pflow_ctrl_init(pdev);
#endif
    desc_pool_size = dp_soc->desc_pool_size;

    pdev->cfg.is_high_latency = dp_soc->is_high_latency;

    /* store provided params */
    pdev->osdev = osdev;
    dp_soc->htt_pdev->txrx_pdev = (ol_txrx_pdev_handle)pdev;
    pdev->htt_pdev = dp_soc->htt_pdev;

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    qdf_info("pdev attach scn = %pK, scn ctx = %pK scn ifnum = %d", scn, scn->nss_radio.nss_rctx, scn->nss_radio.nss_rifnum);
    qdf_info("pdev attach soc = %pK, soc ctx = %pK, soc id = %d soc ifnum = %d ", scn->soc, scn->soc->nss_soc.nss_sctx,scn->soc->nss_soc.nss_wifiol_id, scn->soc->nss_soc.nss_sifnum);
    if (scn->soc->nss_soc.nss_sctx) {
        pdev->nss_wifiol_id = scn->soc->nss_soc.nss_wifiol_id;
        pdev->nss_wifiol_ctx = scn->soc->nss_soc.nss_sctx;
        pdev->nss_ifnum = scn->soc->nss_soc.nss_sifnum;
        osif_nss_ol_set_msdu_ttl((ol_txrx_pdev_handle)pdev);
    } else {
        pdev->nss_wifiol_id = -1;
        pdev->nss_wifiol_ctx = NULL;
        pdev->nss_ifnum = -1;
    }
#endif

    if(htt_attach_tx_rx(soc, pdev->htt_pdev, htc_pdev, desc_pool_size) != 0)
    {
        goto fail1;
    }

    OL_TXRX_STATS_INIT(pdev);

    TAILQ_INIT(&pdev->vdev_list);

    /* do initial set up of the peer ID -> peer object lookup map */
    if (ol_txrx_peer_find_attach(pdev)) {
        goto fail1;
    }

    if (lmac_get_tgt_type(dp_soc->psoc_obj))
        pdev->is_ar900b = lmac_is_target_ar900b(dp_soc->psoc_obj);

#ifdef WLAN_FEATURE_FASTPATH
    scn->soc->htt_handle = pdev->htt_pdev;
    pdev->htt_pdev->osc = sc;
#endif /* WLAN_FEATURE_FASTPATH */

    qdf_spinlock_create(&pdev->stats_buffer_lock);
    TAILQ_INIT(&pdev->stats_buffer_list);
    qdf_create_work(pdev->osdev, &(pdev->stats_wq), stats_deferred_work, pdev);

    pdev->tx_desc.array = qdf_mem_malloc(
            desc_pool_size * sizeof(union ol_tx_desc_list_elem_t));
    if (!pdev->tx_desc.array) {
        goto fail3;
    }
    qdf_mem_set(
            pdev->tx_desc.array,
            desc_pool_size * sizeof(union ol_tx_desc_list_elem_t), 0);

    qdf_info(KERN_INFO"%d tx desc's allocated ; range starts from %pK",
             desc_pool_size, pdev->tx_desc.array );
    /*
     * Each SW tx desc (used only within the tx datapath SW) has a
     * matching HTT tx desc (used for downloading tx meta-data to FW/HW).
     * Go ahead and allocate the HTT tx desc and link it with the SW tx
     * desc now, to avoid doing it during time-critical transmit.
     */
    pdev->tx_desc.pool_size = desc_pool_size;
    for (i = 0; i < desc_pool_size; i++) {
        void *htt_tx_desc;
        void *htt_frag_desc;
        u_int32_t paddr_lo;

        htt_tx_desc = htt_tx_desc_assign(pdev->htt_pdev, i, &paddr_lo);
        if (! htt_tx_desc) {
            qdf_print("%s: failed to alloc HTT tx desc (%d of %d)",
                    __func__, i, desc_pool_size);
            goto fail4;
        }
        pdev->tx_desc.array[i].tx_desc.htt_tx_desc = htt_tx_desc;

        htt_frag_desc = htt_tx_frag_assign(pdev->htt_pdev, i);
        if (! htt_frag_desc) {
            qdf_print("%s: failed to alloc HTT frag desc (%d of %d)",
                    __func__, i, desc_pool_size);
            goto fail4;
        }

        pdev->tx_desc.array[i].tx_desc.htt_frag_desc = htt_frag_desc;
        pdev->tx_desc.array[i].tx_desc.htt_tx_desc_paddr = paddr_lo;

#ifdef WLAN_FEATURE_FASTPATH
        /* Initialize ID once for all */
        pdev->tx_desc.array[i].tx_desc.id = i;
        qdf_atomic_init(&pdev->tx_desc.array[i].tx_desc.ref_cnt);
#endif /* WLAN_FEATURE_FASTPATH */
    }

    /* link SW tx descs into a freelist */
    pdev->tx_desc.freelist = &pdev->tx_desc.array[0];
    for (i = 0; i < desc_pool_size-1; i++) {
        pdev->tx_desc.array[i].next = &pdev->tx_desc.array[i+1];
    }
    pdev->tx_desc.array[i].next = NULL;

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (pdev->nss_wifiol_ctx) {
        uint32_t htt_tx_desc_base_paddr;
        uint32_t htt_tx_desc_base_vaddr = (long)htt_tx_desc_assign(pdev->htt_pdev, 0, &htt_tx_desc_base_paddr);
        uint32_t htt_tx_desc_offset = sizeof(struct htt_host_tx_desc_t) ;
        if (osif_nss_ol_pdev_attach(scn, pdev->nss_wifiol_ctx, pdev->nss_wifiol_id,
                    desc_pool_size, (uint32_t *)pdev->tx_desc.array,
                    (long)htt_tx_desc_pool_paddr(pdev->htt_pdev),
                    htt_tx_desc_frag_desc_size(pdev->htt_pdev),
                    htt_tx_desc_base_vaddr, htt_tx_desc_base_paddr , htt_tx_desc_offset, pdev->htt_pdev->host_q_mem.pool_paddr)){
            printk("NSS: Pdev attach failed");
            goto fail4;
        }
    }
#endif

#if (HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE)

    /* Allocating TSO desc pool of desc_pool_size/2, since only one TSO desc required per Jumbo frame
     *      * and each jumbo frame leads to a minimim of 2 TX data packets consuming 2 TX descriptors */
    pdev->tx_tso_desc.array = qdf_mem_malloc(
            (desc_pool_size/2) * sizeof(union ol_tx_tso_desc_list_elem_t));
    if (!pdev->tx_tso_desc.array) {
        goto fail4;
    }

    qdf_mem_set(
            pdev->tx_tso_desc.array,
            (desc_pool_size/2) * sizeof(union ol_tx_tso_desc_list_elem_t), 0);

    pdev->tx_tso_desc.pool_size = desc_pool_size/2;

    /* link SW tx tso descs into a freelist */
    pdev->tx_tso_desc.freelist = &pdev->tx_tso_desc.array[0];
    for (i = 0; i < pdev->tx_tso_desc.pool_size-1; i++) {
        pdev->tx_tso_desc.array[i].next = &pdev->tx_tso_desc.array[i+1];
    }

    pdev->tx_tso_desc.array[i].next = NULL;
#endif /* HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE */

#if HOST_SW_SG_ENABLE

    /* Allocating SG desc pool of desc_pool_size */
    pdev->tx_sg_desc.array = qdf_mem_malloc(
            (desc_pool_size) * sizeof(union ol_tx_sg_desc_list_elem_t));
    if (!pdev->tx_sg_desc.array) {
        goto fail4;
    }

    qdf_mem_set(
            pdev->tx_sg_desc.array,
            (desc_pool_size) * sizeof(union ol_tx_sg_desc_list_elem_t), 0);

    pdev->tx_sg_desc.pool_size = desc_pool_size;

    /* link SW tx tso descs into a freelist */
    pdev->tx_sg_desc.freelist = &pdev->tx_sg_desc.array[0];
    for (i = 0; i < pdev->tx_sg_desc.pool_size-1; i++) {
        pdev->tx_sg_desc.array[i].next = &pdev->tx_sg_desc.array[i+1];
    }

    pdev->tx_sg_desc.array[i].next = NULL;
#endif /* HOST_SW_SG_ENABLE */

    /* initialize the counter of the target's tx buffer availability */
    qdf_atomic_init(&pdev->target_tx_credit);
    qdf_atomic_add(
    ol_cfg_target_tx_credit(dp_soc->psoc_obj), &pdev->target_tx_credit);

    /* check what format of frames are expected to be delivered by the OS */
    pdev->rx_decap_mode = ol_cfg_pkt_type((ol_pdev_handle)pdev);

    /* Header Cache Initialization for LL cached  path*/
    HTT_FF_CACHE_INIT(pdev->htt_pdev, ol_cfg_pkt_type((ol_pdev_handle)pdev));

    /* setup the global rx defrag waitlist */
    TAILQ_INIT(&pdev->rx.defrag.waitlist);

    /* configure where defrag timeout and duplicate detection is handled */
    pdev->rx.flags.defrag_timeout_check = ol_cfg_rx_host_defrag_timeout_duplicate_check(ctrl_pdev);
    pdev->rx.flags.dup_check = ol_cfg_rx_host_defrag_timeout_duplicate_check(ctrl_pdev);

    /*
     * Determine what rx processing steps are done within the host.
     * Possibilities:
     * 1.  Nothing - rx->tx forwarding and rx PN entirely within target.
     *     (This is unlikely; even if the target is doing rx->tx forwarding,
     *     the host should be doing rx->tx forwarding too, as a back up for
     *     the target's rx->tx forwarding, in case the target runs short on
     *     memory, and can't store rx->tx frames that are waiting for missing
     *     prior rx frames to arrive.)
     * 2.  Just rx -> tx forwarding.
     *     This is the typical configuration for HL, and a likely
     *     configuration for LL STA or small APs (e.g. retail APs).
     * 3.  Both PN check and rx -> tx forwarding.
     *     This is the typical configuration for large LL APs.
     * Host-side PN check without rx->tx forwarding is not a valid
     * configuration, since the PN check needs to be done prior to
     * the rx->tx forwarding.
     */
    if (ol_cfg_rx_pn_check(pdev->ctrl_pdev)) {
        if (ol_cfg_rx_fwd_check(pdev->ctrl_pdev)) {
            /*
             * Both PN check and rx->tx forwarding done on host.
             */
            pdev->rx_opt_proc = ol_rx_pn_check;
        } else {
            qdf_print(
                    "%s: invalid config: if rx PN check is on the host,"
                    "rx->tx forwarding check needs to also be on the host.",
                    __func__);
            goto fail5;
        }
    } else {
        /* PN check done on target */
        if (ol_cfg_rx_fwd_check(pdev->ctrl_pdev)) {
            /*
             * rx->tx forwarding done on host (possibly as
             * back-up for target-side primary rx->tx forwarding)
             */
            pdev->rx_opt_proc = ol_rx_fwd_check;
        } else {
            pdev->rx_opt_proc = ol_rx_deliver;
        }
    }

    /* Allocate space for holding monitor mode status for RX packets */
    pdev->monitor_vdev = NULL;
    pdev->rx_mon_recv_status = qdf_mem_malloc(
            sizeof(struct ieee80211_rx_status));
    if (pdev->rx_mon_recv_status == NULL) {
        goto fail5;
    }
    pdev->rx_mon_recv_status->rs_freq = pdev->htt_pdev->rs_freq;

    /* initialize mutexes for tx desc alloc and peer lookup */
    qdf_spinlock_create(&pdev->tx_mutex);
    qdf_spinlock_create(&pdev->peer_ref_mutex);
    qdf_spinlock_create(&pdev->mon_mutex);

    pdev->prot_an_tx_sent = ol_txrx_prot_an_attach(pdev, "xmit 802.3");
    pdev->prot_an_rx_sent = ol_txrx_prot_an_attach(pdev, "recv 802.3");

    if (OL_RX_REORDER_TRACE_ATTACH(pdev) != A_OK) {
        goto fail6;
    }

    if (OL_RX_PN_TRACE_ATTACH(pdev) != A_OK) {
        goto fail7;
    }

    if (wlan_psoc_nif_feat_cap_get(dp_soc->psoc_obj,
                                          WLAN_SOC_F_HOST_80211_ENABLE)) {
          pdev->host_80211_enable = ol_scn_host_80211_enable_get(scn);
    }

    if (scn->soc->nss_nwifi_offload)
        pdev->nss_nwifi_offload = ol_scn_nss_nwifi_offload_get(scn);


    /*
     * WDI event attach
     */
    if ((ret = wdi_event_attach(pdev)) == A_ERROR) {
        qdf_print("WDI event attach unsuccessful");
    }

    /*
     * pktlog pdev initialization
     */
#ifndef REMOVE_PKT_LOG
    pdev->pl_dev = scn->pl_dev;
#endif
    /*
     * Initialize rx PN check characteristics for different security types.
     */
    qdf_mem_set(&pdev->rx_pn[0],sizeof(pdev->rx_pn), 0);

    /* TKIP: 48-bit TSC, CCMP: 48-bit PN */
    pdev->rx_pn[htt_sec_type_tkip].len =
        pdev->rx_pn[htt_sec_type_tkip_nomic].len =
        pdev->rx_pn[htt_sec_type_aes_ccmp].len =
        pdev->rx_pn[htt_sec_type_aes_ccmp_256].len =
        pdev->rx_pn[htt_sec_type_aes_gcmp].len =
        pdev->rx_pn[htt_sec_type_aes_gcmp_256].len = 48;

    pdev->rx_pn[htt_sec_type_tkip].cmp =
        pdev->rx_pn[htt_sec_type_tkip_nomic].cmp =
        pdev->rx_pn[htt_sec_type_aes_ccmp].cmp =
        pdev->rx_pn[htt_sec_type_aes_ccmp_256].cmp =
        pdev->rx_pn[htt_sec_type_aes_gcmp].cmp =
        pdev->rx_pn[htt_sec_type_aes_gcmp_256].cmp = ol_rx_pn_cmp48;

    /* WAPI: 128-bit PN */
    pdev->rx_pn[htt_sec_type_wapi].len = 128;
    pdev->rx_pn[htt_sec_type_wapi].cmp = ol_rx_pn_wapi_cmp;


    atomic_set(&pdev->mc_num_vap_attached,0);


    pdev->tid_override_queue_mapping = 0;
    pdev->fw_supported_enh_stats_version = HTT_T2H_EN_STATS_MAX_VER;
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Created pdev %pK\n", pdev);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "TX stats version supported in FW %d\n", pdev->fw_supported_enh_stats_version);

    dp_soc->pdev[pdev_id] = (ol_txrx_pdev_handle)pdev;


    vdev_map_size = WLAN_UMAC_PDEV_MAX_VDEVS * sizeof(pdev->vdev_id_to_obj_map[0]);
    pdev->vdev_id_to_obj_map = qdf_mem_malloc(vdev_map_size);
    if (!pdev->vdev_id_to_obj_map) {
        goto fail7;
    }
    qdf_mem_set(pdev->vdev_id_to_obj_map, vdev_map_size, 0);

    dp_cal_client_attach(&(pdev->cal_client_ctx), pdev, pdev->osdev,
                        &ol_iterate_update_peer_list);

#ifdef QCA_SUPPORT_RDK_STATS
    qdf_mem_zero(&pdev->sojourn_stats, sizeof(struct cdp_tx_sojourn_stats));
    pdev->sojourn_nbuf = qdf_nbuf_alloc(pdev->osdev,
                                        sizeof(struct cdp_tx_sojourn_stats), 0, 4,
                                        TRUE);
    if (!pdev->sojourn_nbuf) {
        qdf_warn("failed to allocate nbuf for sojourn stats");
        goto fail8;
    }
    sojourn_buf = qdf_nbuf_data(pdev->sojourn_nbuf);
    qdf_mem_zero(sojourn_buf, sizeof(struct cdp_tx_sojourn_stats));
#endif
    return (struct cdp_pdev *)pdev; /* success */

#ifdef QCA_SUPPORT_RDK_STATS
fail8:
    dp_cal_client_detach(&(pdev->cal_client_ctx));
#endif

fail7:
    OL_RX_REORDER_TRACE_DETACH(pdev);

fail6:
    qdf_spinlock_destroy(&pdev->tx_mutex);
    qdf_spinlock_destroy(&pdev->peer_ref_mutex);
    qdf_spinlock_destroy(&pdev->mon_mutex);

    qdf_mem_free(pdev->rx_mon_recv_status);

fail5:
#if (HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE)
    qdf_mem_free(pdev->tx_tso_desc.array);
#endif /* HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE */
#if HOST_SW_SG_ENABLE
    qdf_mem_free(pdev->tx_sg_desc.array);
#endif /* HOST_SW_SG_ENABLE */

fail4:
    qdf_mem_free(pdev->tx_desc.array);

fail3:
    qdf_spinlock_destroy(&pdev->stats_buffer_lock);
    htt_detach(pdev->htt_pdev);
#ifdef WLAN_FEATURE_FASTPATH
    scn->soc->htt_handle = NULL;
#endif
    ol_txrx_peer_find_detach(pdev);

fail1:
    qdf_spinlock_destroy(&pdev->acnbufqlock);

#if defined(WLAN_FEATURE_FASTPATH) && PEER_FLOW_CONTROL
    ol_tx_pflow_ctrl_clean(pdev);
#endif
    qdf_mem_free(pdev);

fail0:
    return NULL; /* fail */
}

A_STATUS
ol_txrx_pdev_attach_target(struct cdp_pdev *pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    return htt_attach_target(pdev->htt_pdev);
}



void
ol_txrx_pdev_detach(struct cdp_pdev *pdev_handle, int force)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    struct ol_txrx_psoc_t *dp_soc = (struct ol_txrx_psoc_t *)pdev->soc;
    int i;
    struct ol_txrx_fw_stats_info *stats_buffer, *temp_buffer;
    A_STATUS ret;
    /* preconditions */
    TXRX_ASSERT2(pdev);

    /*
     * Initialize this Pdev id array to NULL to avoid
     * double free
     */
    dp_soc->pdev[pdev->pdev_id] = NULL;

    /* check that the pdev has no vdevs allocated */
    TXRX_ASSERT1(TAILQ_EMPTY(&pdev->vdev_list));

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (!pdev->nss_wifiol_ctx)
#endif
    {
        for (i = 0; i < pdev->tx_desc.pool_size; i++) {
            //void *htt_tx_desc;

            /*
             * Confirm that each tx descriptor is "empty", i.e. it has
             * no tx frame attached.
             * In particular, check that there are no frames that have
             * been given to the target to transmit, for which the
             * target has never provided a response.
             */
            if (qdf_atomic_read(&pdev->tx_desc.array[i].tx_desc.ref_cnt)) {
                TXRX_PRINT(TXRX_PRINT_LEVEL_WARN,
                        "Warning: freeing tx frame "
                        "(no tx completion from the target)\n");
                ol_tx_desc_frame_free_nonstd(
                        pdev, &pdev->tx_desc.array[i].tx_desc, 1);
            }

        }
    }

    qdf_spinlock_destroy(&pdev->acnbufqlock);

    qdf_mem_free(pdev->tx_desc.array);
#if (HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE)
    qdf_mem_free(pdev->tx_tso_desc.array);
#endif /* HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE */

#if HOST_SW_SG_ENABLE
    qdf_mem_free(pdev->tx_sg_desc.array);
#endif /* HOST_SW_SG_ENABLE */

#if ATH_SUPPORT_IQUE
    ol_tx_me_exit((ol_txrx_pdev_handle) pdev);
#endif /*ATH_SUPPORT_IQUE*/

    /*Free pending requests if any*/
    qdf_spin_lock_bh(&pdev->stats_buffer_lock);
    TAILQ_FOREACH_SAFE(stats_buffer, &pdev->stats_buffer_list, stats_info_list_elem, temp_buffer) {
        qdf_mem_free(stats_buffer);
    }
    qdf_spin_unlock_bh(&pdev->stats_buffer_lock);
    qdf_spinlock_destroy(&pdev->stats_buffer_lock);
    htt_detach(pdev->htt_pdev);

    if (force) {
        /*
         * The assertion above confirms that all vdevs within this pdev
         * were detached.  However, they may not have actually been deleted.
         * If the vdev had peers which never received a PEER_UNMAP message
         * from the target, then there are still zombie peer objects, and
         * the vdev parents of the zombie peers are also zombies, hanging
         * around until their final peer gets deleted.
         * Go through the peer hash table and delete any peers left in it.
         * As a side effect, this will complete the deletion of any vdevs
         * that are waiting for their peers to finish deletion.
         */
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Force delete for pdev %p\n", pdev);
        ol_txrx_peer_find_hash_erase(pdev);
    }

#if PEER_FLOW_CONTROL
    pdev->pflow_cong_ctrl_timer_interval = 0;
    qdf_timer_stop(&pdev->pflow_ctl_cong_timer);

    if(pdev->pflow_ctl_stats_timer_interval) {
        qdf_timer_stop(&pdev->pflow_ctl_stats_timer);
    }

    qdf_timer_free(&pdev->pflow_ctl_cong_timer);
    qdf_timer_free(&pdev->pflow_ctl_stats_timer);
#if PEER_FLOW_CONTROL_HOST_SCHED
    qdf_timer_stop(&pdev->pflow_ctl_dequeue_timer);
    qdf_timer_free(&pdev->pflow_ctl_dequeue_timer);
#endif
#endif

    ol_txrx_peer_find_detach(pdev);

    qdf_spinlock_destroy(&pdev->tx_mutex);
    qdf_spinlock_destroy(&pdev->peer_ref_mutex);
    qdf_spinlock_destroy(&pdev->mon_mutex);

    qdf_mem_free(pdev->rx_mon_recv_status);

    OL_TXRX_PROT_AN_FREE(pdev->prot_an_tx_sent);
    OL_TXRX_PROT_AN_FREE(pdev->prot_an_rx_sent);

    OL_RX_REORDER_TRACE_DETACH(pdev);
    OL_RX_PN_TRACE_DETACH(pdev);
    /*
     * WDI event detach
     */
    if ((ret = wdi_event_detach(pdev)) == A_ERROR) {
        qdf_print("WDI detach unsuccessful");
    }

    dp_cal_client_detach(&(pdev->cal_client_ctx));
    qdf_mem_free(pdev->vdev_id_to_obj_map);
    qdf_mem_free(pdev->dp_txrx_handle);
#ifdef QCA_SUPPORT_RDK_STATS
    qdf_nbuf_free(pdev->sojourn_nbuf);
#endif
    qdf_mem_free(pdev);
}

struct cdp_vdev *
ol_txrx_vdev_attach(
        struct cdp_pdev *pdev_handle,
        u_int8_t *vdev_mac_addr,
        u_int8_t vdev_id,
        enum wlan_op_mode op_mode)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    struct ol_txrx_vdev_t *vdev;
    struct ol_ath_softc_net80211 *scn;

    scn = (struct ol_ath_softc_net80211 *)
               lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)pdev->ctrl_pdev);

    if (!scn) {
        qdf_info ("scn is NULL");
        return NULL;
    }

    /* preconditions */
    TXRX_ASSERT2(pdev);
    TXRX_ASSERT2(vdev_mac_addr);

    vdev = qdf_mem_malloc(sizeof(*vdev));
    if (!vdev) {
        return NULL; /* failure */
    }

    /* store provided params */
    vdev->pdev = pdev;
    vdev->vdev_id = vdev_id;
    pdev->vdev_id_to_obj_map[vdev_id] = vdev;
    vdev->opmode = op_mode;

    vdev->sta_peer_id =  HTT_INVALID_PEER;
    vdev->osif_rx =     NULL;
    vdev->osif_rx_mon = NULL;
    vdev->osif_vdev =   NULL;

    vdev->delete.pending = 0;
    vdev->safemode = 0;
    vdev->drop_unenc = 1;
    vdev->filters_num = 0;
    vdev->htc_htt_hdr_size = HTC_HTT_TRANSFER_HDRSIZE;

    qdf_mem_copy(
            &vdev->mac_addr.raw[0], vdev_mac_addr, QDF_MAC_ADDR_SIZE);

    vdev->tx_encap_type = ol_cfg_pkt_type((ol_pdev_handle)pdev);
    vdev->rx_decap_type = ol_cfg_pkt_type((ol_pdev_handle)pdev);
    /* Header cache update for each vdev */
    HTT_HDRCACHE_UPDATE(pdev, vdev);

#if 0 // BEELINER Specific
    if(vdev->opmode != wlan_op_mode_monitor) {
        vdev->pRateCtrl = NULL;

        if(pdev->ratectrl.is_ratectrl_on_host) {
            /* Attach the context for rate-control. */
            vdev->pRateCtrl = ol_ratectrl_vdev_ctxt_attach(pdev, vdev);

            if (!(vdev->pRateCtrl)) {
                /* failure case */
                qdf_mem_free(vdev);
                vdev = NULL;
                return NULL;
            }
        }

    }
#endif

    OS_MEMSET(vdev->txpow_mgt_frm, 0xff, sizeof(vdev->txpow_mgt_frm));
    TAILQ_INIT(&vdev->peer_list);

    /* add this vdev into the pdev's list */
    TAILQ_INSERT_TAIL(&pdev->vdev_list, vdev, vdev_list_elem);

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
            "Created vdev %pK (%02x:%02x:%02x:%02x:%02x:%02x)\n",
            vdev,
            vdev->mac_addr.raw[0], vdev->mac_addr.raw[1], vdev->mac_addr.raw[2],
            vdev->mac_addr.raw[3], vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);

    pdev->vdev_count = scn->vdev_count;

    return (struct cdp_vdev *)vdev;
}

void
ol_txrx_vdev_register(
        struct cdp_vdev *vdev_handle,
        void *osif_vdev, struct cdp_ctrl_objmgr_vdev *ctrl_vdev,
        struct ol_txrx_ops *txrx_ops)
{
    struct ol_txrx_psoc_t *dp_soc = (struct ol_txrx_psoc_t *)((struct ol_txrx_vdev_t *)vdev_handle)->pdev->soc;
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    vdev->osif_vdev = osif_vdev;
    vdev->ctrl_vdev = ctrl_vdev;
    vdev->osif_rx = txrx_ops->rx.rx;
    vdev->osif_rsim_rx_decap = txrx_ops->rx.rsim_rx_decap;
#if ATH_SUPPORT_WAPI
    vdev->osif_check_wai = txrx_ops->rx.wai_check;
#endif
    vdev->osif_rx_mon = txrx_ops->rx.mon;
#if UMAC_SUPPORT_PROXY_ARP
    vdev->osif_proxy_arp = txrx_ops->proxy_arp;
#endif
    if (ol_cfg_is_high_latency((ol_soc_handle)dp_soc->psoc_obj)) {
        txrx_ops->tx.tx = vdev->tx = ol_tx_hl;
    } else {
        txrx_ops->tx.tx = vdev->tx = ol_tx_ll;
    }
    txrx_ops->tx.tx_exception = ol_tx_exception;
}

int
ol_txrx_get_nwifi_mode(
        struct cdp_vdev *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    return pdev->nss_nwifi_offload;
}

int
ol_txrx_is_target_ar900b(
        struct cdp_vdev *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    return pdev->is_ar900b;
}

void ol_txrx_set_peer_nawds(struct cdp_peer *peer_handle, uint8_t value)
{
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)peer_handle;
    peer->vdev->nawds_enabled = 1;
    peer->nawds_enabled = 1;
}

QDF_STATUS
ol_txrx_set_monitor_mode(
        struct cdp_vdev *vdev_handle, uint8_t smart_monitor)
{
    /* Many monitor VAPs can exists in a system but only one can be up at
     * anytime
     */
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    struct ol_txrx_vdev_t *mon_vdev = pdev->monitor_vdev;
    wlan_if_t vap = NULL;

    qdf_spin_lock_bh(&pdev->mon_mutex);
    pdev->monitor_vdev = vdev;
    qdf_spin_unlock_bh(&pdev->mon_mutex);

    if (smart_monitor)
	    return QDF_STATUS_SUCCESS;

    /*Check if current pdev's monitor_vdev exists, and if it's already up*/
    if(mon_vdev){
        vap = ((osif_dev *)(mon_vdev->osif_vdev))->os_if;
        TXRX_ASSERT2(vap);
        if (wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
            qdf_print("Monitor mode VAP already up");
            return QDF_STATUS_E_ALREADY;
        }
    }

    return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ol_txrx_set_pdev_tx_capture(struct cdp_pdev *pdev_handle, int val)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    if (val == 1)
        pdev->tx_capture = 1;
    else
        pdev->tx_capture = 0;

    return QDF_STATUS_SUCCESS;
}

uint8_t
ol_txrx_get_pdev_id_frm_pdev(struct cdp_pdev *pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    return pdev->pdev_id;
}

bool
ol_txrx_get_vow_config_frm_pdev(struct cdp_pdev *pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    return pdev->carrier_vow_config;
}

void
ol_txrx_get_peer_mac_from_peer_id(struct cdp_pdev *pdev_handle,
        uint32_t peer_id, uint8_t *peer_mac)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    struct ol_txrx_peer_t *peer;

    if (pdev && peer_mac) {
        peer = (peer_id == HTT_INVALID_PEER) ? NULL :
            (struct ol_txrx_peer_t *) pdev->peer_id_to_obj_map[peer_id];
        if (peer && peer->mac_addr.raw) {
            qdf_mem_copy(peer_mac, peer->mac_addr.raw,
                    QDF_MAC_ADDR_SIZE);
        }
    }

}

void
ol_txrx_vdev_tx_lock(struct cdp_vdev *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    qdf_spin_lock_bh(&pdev->tx_lock);
}

void
ol_txrx_vdev_tx_unlock(struct cdp_vdev *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    qdf_spin_unlock_bh(&pdev->tx_lock);
}

void ol_txrx_ath_get_vdev_stats(void *handle, struct cdp_dev_stats *stats)
{
    return;
}

void ol_txrx_ath_get_pdev_stats(void *handle, struct cdp_dev_stats *stats)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)handle;
    ol_txrx_aggregate_pdev_stats(pdev);
    stats->tx_packets = pdev->pdev_stats.tx.comp_pkt.num;
    stats->tx_bytes = pdev->pdev_stats.tx.comp_pkt.bytes;
    stats->tx_errors = pdev->pdev_stats.tx_i.dropped.dropped_pkt.num
        + pdev->pdev_data_stats.tx.dropped.download_fail.pkts
        + pdev->pdev_stats.tx.is_tx_no_ack.num;
    stats->tx_dropped = stats->tx_errors;

    stats->rx_packets = pdev->pdev_stats.rx.to_stack.num
        + pdev->pdev_data_stats.rx.forwarded.pkts;
    stats->rx_bytes = pdev->pdev_stats.rx.to_stack.bytes
        + pdev->pdev_data_stats.rx.forwarded.bytes;


}

void ol_txrx_ath_getstats(void *handle, struct cdp_dev_stats *stats, uint8_t type)
{
    switch (type) {
        case UPDATE_PDEV_STATS:
            ol_txrx_ath_get_pdev_stats(handle, stats);
	    break;
        case UPDATE_VDEV_STATS:
            ol_txrx_ath_get_vdev_stats(handle, stats);
            break;
        default:
            qdf_print("apstats cannot be updated for this input type %d",type);
            break;
    }
}

void
ol_txrx_set_gid_flag(struct cdp_pdev *pdev_handle,
        u_int8_t *mem_status, u_int8_t *user_position)
{
#ifndef REMOVE_PKT_LOG
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    pdev->gid_flag = 1;
    qdf_mem_copy((void *)&(pdev->gid_mgmt.member_status[0]),
            ((void *)mem_status),
            sizeof(pdev->gid_mgmt.member_status));
    qdf_mem_copy((void *)&(pdev->gid_mgmt.user_position[0]),
            ((void *)user_position),
            sizeof(pdev->gid_mgmt.user_position));
#endif
}

uint32_t
ol_txrx_fw_supported_enh_stats_version(struct cdp_pdev *pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    return pdev->fw_supported_enh_stats_version;
}

struct cdp_pdev *ol_txrx_rx_peer_invalid(struct cdp_pdev *pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    return ((struct cdp_pdev *)pdev->ctrl_pdev);
}

/* flush the mgmt packets when vap is going down */
void
ol_txrx_if_mgmt_drain(void *ni_handle, int force)
{
    struct ieee80211_node *ni = (struct ieee80211_node *)ni_handle;
    struct ieee80211com *ic = ni->ni_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_txrx_pdev_handle pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    struct htt_pdev_t *hpdev =((struct ol_txrx_pdev_t *)pdev_txrx_handle)->htt_pdev;
    uint8_t vdev_id = (uint8_t)(OL_ATH_VAP_NET80211(ni->ni_vap))->av_if_id;

    if (force) {
        (void)htt_tx_mgmt_desc_drain(hpdev, vdev_id);
    } else {
        /* Mark frames waiting completions as delayed free.*/
        (void)htt_tx_mgmt_desc_mark_delayed_free(hpdev, vdev_id,
                IEEE80211_TX_ERROR_NO_SKB_FREE);
    }
}

QDF_STATUS
ol_txrx_reset_monitor_mode(struct cdp_pdev *pdev_handle)
{
    /* Many monitor VAPs can exists in a system but only one can be up at
     * anytime
     */
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    qdf_spin_lock_bh(&pdev->mon_mutex);
    pdev->monitor_vdev = NULL;
    qdf_spin_unlock_bh(&pdev->mon_mutex);
    return QDF_STATUS_SUCCESS;
}

void
ol_txrx_set_nac(struct cdp_peer *peer_handle)
{
#if ATH_SUPPORT_NAC
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)peer_handle;
    peer->nac = 1;
#endif
}

void ol_txrx_monitor_set_filter_ucast_data(struct cdp_pdev *pdev_handle, u_int8_t val)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    pdev->mon_filter_ucast_data = val;
}

void ol_txrx_monitor_set_filter_mcast_data(struct cdp_pdev *pdev_handle, u_int8_t val)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    pdev->mon_filter_mcast_data = val;
}

void ol_txrx_monitor_set_filter_non_data(struct cdp_pdev *pdev_handle, u_int8_t val)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    pdev->mon_filter_non_data = val;
}

bool ol_txrx_monitor_get_filter_ucast_data(struct cdp_vdev *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    return vdev->pdev->mon_filter_ucast_data;
}
qdf_export_symbol(ol_txrx_monitor_get_filter_ucast_data);

bool ol_txrx_monitor_get_filter_mcast_data(struct cdp_vdev *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    return vdev->pdev->mon_filter_mcast_data;
}
qdf_export_symbol(ol_txrx_monitor_get_filter_mcast_data);

bool ol_txrx_monitor_get_filter_non_data(struct cdp_vdev *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    return vdev->pdev->mon_filter_non_data;
}
qdf_export_symbol(ol_txrx_monitor_get_filter_non_data);

int
ol_txrx_set_filter_neighbour_peers(
        struct cdp_pdev *pdev_handle,
        u_int32_t val)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    pdev->filter_neighbour_peers = val;
    return 0;
}

void
ol_txrx_set_curchan(
        struct cdp_pdev *pdev_handle,
        u_int32_t chan_mhz)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    pdev->rx_mon_recv_status->rs_freq = chan_mhz;
    return;
}

void
ol_txrx_set_safemode(
        struct cdp_vdev *vdev_handle,
        u_int32_t val)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    vdev->safemode = val;
}

void
ol_txrx_set_tx_encap_type(struct cdp_vdev *vdev_handle, u_int32_t val)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    u_int8_t sub_type = 0;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    wlan_if_t vap;
    osif_dev *osifp;
    struct ieee80211com *ic;
    osifp = vdev->osif_vdev;
    vap = osifp->os_if;
    ic = vap->iv_ic;
#endif
    vdev->tx_encap_type = val;

    HTT_HDRCACHE_UPDATE_PKTTYPE(vdev, val);

    if (val == htt_pkt_type_raw) {
        /* 802.11 MAC Header present. */
        sub_type |= 0x1 << HTT_TX_MSDU_DESC_RAW_SUBTYPE_80211_HDR_S;

        /* Don't allow aggregation. */
        sub_type |= 0x1 << HTT_TX_MSDU_DESC_RAW_SUBTYPE_NO_AGGR_S;

        /* Important note for end system integrators: The following encryption
         * related flag needs to be set, or kept clear, according to the desired
         * configuration.
         *
         * The flag is being kept clear in this code base since the reference
         * code does not interact with any external entity that could carry out
         * the requisite encryption.
         */
#if 0
        /* Illustration only. */
        if (condition) {
            /*  Don't perform encryption */
            sub_type |= 0x1 << HTT_TX_MSDU_DESC_RAW_SUBTYPE_NO_ENCRYPT_S;
        }
#endif /* 0 */
    }

    HTT_HDRCACHE_UPDATE_PKTSUBTYPE(vdev, sub_type);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (ic->nss_vops) {
        ic->nss_vops->ic_osif_nss_vdev_set_cfg(osifp, OSIF_NSS_VDEV_ENCAP_TYPE);
        ic->nss_vops->ic_osif_nss_vap_updchdhdr(osifp);
    }
#endif

}

inline enum htt_pkt_type
ol_txrx_get_tx_encap_type(ol_txrx_vdev_handle vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    return vdev->tx_encap_type;
}

void
ol_txrx_set_vdev_rx_decap_type(struct cdp_vdev *vdev_handle, u_int32_t val)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    wlan_if_t vap;
    osif_dev *osifp;
    struct ieee80211com *ic;
#endif

    vdev->rx_decap_type = val;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    osifp = vdev->osif_vdev;
    vap = osifp->os_if;
    ic = vap->iv_ic;
    if (ic->nss_vops) {
        ic->nss_vops->ic_osif_nss_vap_updchdhdr(osifp);
    }
#endif
}

inline enum htt_cmn_pkt_type
ol_txrx_get_vdev_rx_decap_type(struct cdp_vdev *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    return vdev->rx_decap_type;
}

#if MESH_MODE_SUPPORT
void
ol_txrx_set_mesh_mode(struct cdp_vdev *vdev_handle, u_int32_t val)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    printk("%s val %d \n",__func__,val);
    vdev->mesh_vdev = val;
    vdev->htc_htt_hdr_size = HTC_HTT_TRANSFER_HDRSIZE + HTT_EXTND_DESC_SIZE;
}
#endif

#if WDS_VENDOR_EXTENSION
void
ol_txrx_set_wds_rx_policy(
        struct cdp_vdev *vdev_handle,
        u_int32_t val)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_peer_t *peer;
    if (vdev->opmode == wlan_op_mode_ap) {
        /* for ap, set it on bss_peer */
        TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
            if (peer->bss_peer) {
                peer->wds_rx_filter = 1;
                peer->wds_rx_ucast_4addr = (val & WDS_POLICY_RX_UCAST_4ADDR) ? 1:0;
                peer->wds_rx_mcast_4addr = (val & WDS_POLICY_RX_MCAST_4ADDR) ? 1:0;
                break;
            }
        }
    }
    else if (vdev->opmode == wlan_op_mode_sta) {
        peer = TAILQ_FIRST(&vdev->peer_list);
        peer->wds_rx_filter = 1;
        peer->wds_rx_ucast_4addr = (val & WDS_POLICY_RX_UCAST_4ADDR) ? 1:0;
        peer->wds_rx_mcast_4addr = (val & WDS_POLICY_RX_MCAST_4ADDR) ? 1:0;
    }
}
#endif

void
ol_txrx_set_privacy_filters(
        struct cdp_vdev *vdev_handle,
        void *filters,
        u_int32_t num)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    qdf_mem_copy(vdev->privacy_filters, filters, num*sizeof(privacy_exemption));
    vdev->filters_num = num;
}

void
ol_txrx_set_drop_unenc(
        struct cdp_vdev *vdev_handle,
        u_int32_t val)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    wlan_if_t vap;
    osif_dev *osifp;
    struct ieee80211com *ic;
#endif

    vdev->drop_unenc = val;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    osifp = vdev->osif_vdev;
    vap = osifp->os_if;
    ic = vap->iv_ic;
    if (ic->nss_vops)
        ic->nss_vops->ic_osif_nss_vdev_set_cfg(vdev->osif_vdev, OSIF_NSS_VDEV_DROP_UNENC);
#endif
}

void
ol_txrx_vdev_detach(
        struct cdp_vdev *vdev_handle,
        ol_txrx_vdev_delete_cb callback,
        void *context)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;

    /* preconditions */
    TXRX_ASSERT2(vdev);

    pdev->vdev_id_to_obj_map[vdev->vdev_id] = NULL;
    vdev->sta_peer_id =  HTT_INVALID_PEER;

    /* remove the vdev from its parent pdev's list */
    TAILQ_REMOVE(&pdev->vdev_list, vdev, vdev_list_elem);

    /*
     * Use peer_ref_mutex while accessing peer_list, in case
     * a peer is in the process of being removed from the list.
     */
    qdf_spin_lock_bh(&pdev->peer_ref_mutex);
    /* check that the vdev has no peers allocated */
    if (!TAILQ_EMPTY(&vdev->peer_list)) {
        /* debug print - will be removed later */
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
                "%s: not deleting vdev object %pK (%02x:%02x:%02x:%02x:%02x:%02x)"
                "until deletion finishes for all its peers\n",
                __func__, vdev,
                vdev->mac_addr.raw[0], vdev->mac_addr.raw[1],
                vdev->mac_addr.raw[2], vdev->mac_addr.raw[3],
                vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);
        /* indicate that the vdev needs to be deleted */
        vdev->delete.pending = 1;
        vdev->delete.callback = callback;
        vdev->delete.context = context;
        qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
        return;
    }
    qdf_spin_unlock_bh(&pdev->peer_ref_mutex);

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
            "%s: deleting vdev object %pK (%02x:%02x:%02x:%02x:%02x:%02x)\n",
            __func__, vdev,
            vdev->mac_addr.raw[0], vdev->mac_addr.raw[1], vdev->mac_addr.raw[2],
            vdev->mac_addr.raw[3], vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);

#if 0 // BEELINER COMMENTED THIS
    /* Free the rate-control context. */
    ol_ratectrl_vdev_ctxt_detach(vdev->pRateCtrl);
#endif

    /*
     * Doesn't matter if there are outstanding tx frames -
     * they will be freed once the target sends a tx completion
     * message for them.
     */

    qdf_mem_free(vdev);
    if (callback) {
        callback(context);
    }
}

void *
ol_txrx_peer_attach(
        struct cdp_vdev *vdev_handle,
        u_int8_t *peer_mac_addr, struct cdp_ctrl_objmgr_peer *ctrl_peer)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_peer_t *peer;
    int i;
    struct ol_txrx_pdev_t *pdev;
    struct ol_txrx_psoc_t *soc;
    struct ol_ath_softc_net80211 *scn;
    struct cdp_peer_cookie peer_cookie;

    /* preconditions */
    TXRX_ASSERT2(vdev);
    TXRX_ASSERT2(peer_mac_addr);

    pdev = vdev->pdev;
    soc = (struct ol_txrx_psoc_t *)pdev->soc;
    scn = (struct ol_ath_softc_net80211 *)
               lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)pdev->ctrl_pdev);

    if (!scn) {
        return NULL; /* failure */
    }
/* CHECK CFG TO DETERMINE WHETHER TO ALLOCATE BASE OR EXTENDED PEER STRUCT */
/* USE vdev->pdev->osdev, AND REMOVE PDEV FUNCTION ARG? */
    peer = (struct ol_txrx_peer_t *)qdf_mempool_alloc(scn->soc->qdf_dev, scn->soc->mempool_ol_ath_peer);

    if (!peer) {
        return NULL; /* failure */
    }
    OS_MEMZERO(peer, sizeof(struct ol_txrx_peer_t));
    /* store provided params */
    peer->vdev = vdev;
    peer->ctrl_peer = ctrl_peer;
    qdf_mem_copy(
            &peer->mac_addr.raw[0], peer_mac_addr, QDF_MAC_ADDR_SIZE );

#if 0
    if(vdev->opmode != wlan_op_mode_monitor) {
        peer->rc_node = NULL;

        if(pdev->ratectrl.is_ratectrl_on_host) {
            /* Attach the context for rate-control. */
            peer->rc_node = ol_ratectrl_peer_ctxt_attach(pdev, vdev, peer);

            if (!(peer->rc_node)) {
                /* failure case */
                qdf_mem_free(peer);
                peer = NULL;
                return NULL;
            }
        }

    }
#endif

    peer->rx_opt_proc = pdev->rx_opt_proc;

    ol_rx_peer_init(pdev, peer);

    //ol_tx_peer_init(pdev, peer);

    /* initialize the peer_id */
    for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
        peer->peer_ids[i] = HTT_INVALID_PEER;
    }

#if PEER_FLOW_CONTROL
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if(!pdev->nss_wifiol_ctx)
#endif
    qdf_mem_zero(&peer_cookie, sizeof (peer_cookie));
    {
        for (i = 0; i < OL_TX_PFLOW_CTRL_HOST_MAX_TIDS; i++) {
            peer->tidq[i].dequeue_cnt = 0;
            peer->tidq[i].byte_count = 0;
            qdf_nbuf_queue_init(&peer->tidq[i].nbufq);
        }
    }
#endif

    qdf_spin_lock_bh(&pdev->peer_ref_mutex);
    qdf_atomic_init(&peer->ref_cnt);

    /* keep one reference for attach */
    qdf_atomic_inc(&peer->ref_cnt);

    /* add this peer into the vdev's list */
    TAILQ_INSERT_TAIL(&vdev->peer_list, peer, peer_list_elem);
    qdf_spin_unlock_bh(&pdev->peer_ref_mutex);

    ol_txrx_peer_find_hash_add(pdev, peer);

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2,
            "vdev %pK created peer %pK (%02x:%02x:%02x:%02x:%02x:%02x)\n",
            vdev, peer,
            peer->mac_addr.raw[0], peer->mac_addr.raw[1], peer->mac_addr.raw[2],
            peer->mac_addr.raw[3], peer->mac_addr.raw[4], peer->mac_addr.raw[5]);
    /*
     * For every peer MAp message search and set if bss_peer
     */
    if (memcmp(peer->mac_addr.raw, vdev->mac_addr.raw, 6) == 0){
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2, "vdev bss_peer!!!! \n");
        peer->bss_peer = 1;
        vdev->vap_bss_peer = peer;
    }
    qdf_mem_copy(peer_cookie.mac_addr, peer_mac_addr, QDF_MAC_ADDR_SIZE);
    peer_cookie.ctx = NULL;
    peer_cookie.cookie = pdev->next_peer_cookie++;
    wdi_event_handler(WDI_EVENT_PEER_CREATE, (struct ol_txrx_pdev_t *)pdev, (void *)&peer_cookie,
                      peer->peer_ids[0], WDI_NO_VAL);

    /* Initialize peet rate stats module if wlanstats are enabled*/
    if (soc->wlanstats_enabled) {
        if (!peer_cookie.ctx) {
            pdev->next_peer_cookie--;
            qdf_err("Failed to initialize peer rate stats");
        }
        else
            peer->wlanstats_ctx = peer_cookie.ctx;
    }

    return peer;
}

void ol_txrx_peer_teardown(struct cdp_vdev *vdev_handle, void *peer_hdl)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;

    if (qdf_unlikely(!vdev)) {
        return;
    }
    vdev->sta_peer_id =  HTT_INVALID_PEER;
}

int ol_txrx_peer_add_ast(ol_txrx_soc_handle soc_hdl,
        struct cdp_peer *peer_hdl, uint8_t *mac_addr,
        enum cdp_txrx_ast_entry_type type, uint32_t flags)
{
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)peer_hdl;

    if (!soc_hdl->ol_ops || !soc_hdl->ol_ops->peer_add_wds_entry)
        return 0;

    return soc_hdl->ol_ops->peer_add_wds_entry(peer->vdev->osif_vdev, (struct cdp_peer *)peer,
            mac_addr, peer->mac_addr.raw, flags);
}

void ol_txrx_peer_del_ast(ol_txrx_soc_handle soc_hdl,
        void *ast_entry_hdl)
{
    struct ol_txrx_ast_entry_t *ast_entry = ast_entry_hdl;
    struct ol_txrx_peer_t *peer = ast_entry->base_peer;

    if (soc_hdl->ol_ops && soc_hdl->ol_ops->peer_del_wds_entry)
        soc_hdl->ol_ops->peer_del_wds_entry(peer->vdev->osif_vdev,
                ast_entry->dest_mac_addr.raw, CDP_TXRX_AST_TYPE_WDS);
}

int ol_txrx_peer_update_ast(ol_txrx_soc_handle soc_hdl,
        struct cdp_peer *peer_hdl, uint8_t *wds_macaddr,
        uint32_t flags)
{
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)peer_hdl;

    if (!soc_hdl->ol_ops || !soc_hdl->ol_ops->peer_update_wds_entry)
        return 0;

    return soc_hdl->ol_ops->peer_update_wds_entry(peer->vdev->osif_vdev,
            wds_macaddr, peer->mac_addr.raw, flags);
}

void ol_txrx_wds_reset_ast(ol_txrx_soc_handle soc_hdl, uint8_t *wds_macaddr,
	uint8_t *peer_macaddr, void *vdev_handle)
{
    uint32_t flags = 0;
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;

    if (!soc_hdl->ol_ops || !soc_hdl->ol_ops->peer_delete_multiple_wds_entries)
        return;

    soc_hdl->ol_ops->peer_delete_multiple_wds_entries(vdev->osif_vdev, wds_macaddr,
            peer_macaddr, flags);
}

void ol_txrx_wds_reset_ast_table(ol_txrx_soc_handle soc_hdl,
        void *vdev_handle)
{
    uint8_t *wds_macaddr = NULL;
    uint8_t *peer_macaddr = NULL;
    uint32_t flags = 0;
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;

    if (!soc_hdl->ol_ops || !soc_hdl->ol_ops->peer_delete_multiple_wds_entries)
        return;

    soc_hdl->ol_ops->peer_delete_multiple_wds_entries(vdev->osif_vdev, wds_macaddr,
            peer_macaddr, flags);
}

void
ol_txrx_peer_authorize(struct cdp_peer *peer_handle, uint32_t authorize)
{
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)peer_handle;
    struct ol_txrx_vdev_t *vdev = NULL;
    struct ol_txrx_pdev_t *pdev = NULL;

    if(peer!=NULL)
    {
        vdev = peer->vdev;
        pdev = vdev->pdev;

        qdf_spin_lock_bh(&pdev->peer_ref_mutex);
        peer->authorize = authorize & 0x03;
        qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
    }
}

void
ol_txrx_peer_update(struct ol_txrx_peer_t *peer,
        struct peer_ratectrl_params_t *peer_ratectrl_params)
{
}

#if WDS_VENDOR_EXTENSION
void
ol_txrx_peer_wds_tx_policy_update(struct cdp_peer *peer_handle,
        int wds_tx_ucast, int wds_tx_mcast)
{
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)peer_handle;
    if (wds_tx_ucast || wds_tx_mcast) {
        peer->wds_enabled = 1;
        peer->wds_tx_ucast_4addr = wds_tx_ucast;
        peer->wds_tx_mcast_4addr = wds_tx_mcast;
    }
    else {
        peer->wds_enabled = 0;
        peer->wds_tx_ucast_4addr = 0;
        peer->wds_tx_mcast_4addr = 0;
    }
    return;
}
#endif

void
ol_txrx_peer_unref_delete(ol_txrx_peer_handle peer_handle)
{
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)peer_handle;
    struct ol_txrx_vdev_t *vdev;
    struct ol_txrx_pdev_t *pdev;
    struct ol_txrx_psoc_t *soc;
    struct ol_ath_softc_net80211 *scn;
    struct ol_txrx_peer_t *tmppeer;
    int found = 0;
    bool is_connected_sta_peer = 0;
    struct cdp_peer_cookie peer_cookie;

    u_int16_t peer_id;

    /* preconditions */
    TXRX_ASSERT2(peer);

    vdev = peer->vdev;
    pdev = vdev->pdev;
    soc = (struct ol_txrx_psoc_t *)pdev->soc;
    scn = (struct ol_ath_softc_net80211 *)
               lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)pdev->ctrl_pdev);

    is_connected_sta_peer = (vdev->opmode != wlan_op_mode_sta
                            && !peer->bss_peer);
    /*
     * Hold the lock all the way from checking if the peer ref count
     * is zero until the peer references are removed from the hash
     * table and vdev list (if the peer ref count is zero).
     * This protects against a new HL tx operation starting to use the
     * peer object just after this function concludes it's done being used.
     * Furthermore, the lock needs to be held while checking whether the
     * vdev's list of peers is empty, to make sure that list is not modified
     * concurrently with the empty check.
     */
    qdf_spin_lock_bh(&pdev->peer_ref_mutex);
    if (qdf_atomic_dec_and_test(&peer->ref_cnt)) {
        union ol_txrx_align_mac_addr_t vdev_mac_addr = {0};
        enum wlan_op_mode opmode = vdev->opmode;
        peer_id = peer->peer_ids[0];

        qdf_mem_copy(&vdev_mac_addr, &vdev->mac_addr, QDF_MAC_ADDR_SIZE);

        /*
         * Make sure that the reference to the peer in
         * peer object map is removed
         */
        if (peer_id != HTT_INVALID_PEER) {
            /*
             * Use a PDEV Tx Lock here, because peer used in Tx path for peer/TID enqueue and dequeu
             */
            OL_TX_PEER_UPDATE_LOCK(pdev, peer_id);
            pdev->peer_id_to_obj_map[peer_id] = NULL;
            OL_TX_PEER_UPDATE_UNLOCK(pdev, peer_id);
        }

        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2,
                "Deleting peer %pK (%02x:%02x:%02x:%02x:%02x:%02x)\n",
                peer,
                peer->mac_addr.raw[0], peer->mac_addr.raw[1],
                peer->mac_addr.raw[2], peer->mac_addr.raw[3],
                peer->mac_addr.raw[4], peer->mac_addr.raw[5]);

#if QCA_PARTNER_DIRECTLINK_RX
        /* provide peer unref delete information to partner side */
        if (CE_is_directlink(pdev->ce_tx_hdl)) {
            ol_txrx_peer_unref_delete_partner(peer);
        }
#endif /* QCA_PARTNER_DIRECTLINK_RX */

        /* remove the reference to the peer from the hash table */
        ol_txrx_peer_find_hash_remove(pdev, peer);

        TAILQ_FOREACH(tmppeer, &peer->vdev->peer_list, peer_list_elem) {
            if (tmppeer == peer) {
                found = 1;
                break;
            }
        }
        if (found) {
            TAILQ_REMOVE(&peer->vdev->peer_list, peer, peer_list_elem);
        } else {
            /*Ignoring the remove operation as peer not found*/
            printk ("WARN peer %pK not found in vdev (%pK)->peer_list:%pK\n", peer, vdev,
                    &peer->vdev->peer_list);
        }

        /* cleanup the Rx reorder queues for this peer */
        ol_rx_peer_cleanup(vdev, peer);
        /* Collect disconnected peer stats before deleting */
        ol_txrx_update_stats(vdev, peer);

        /* check whether the parent vdev has no peers left */
        if (TAILQ_EMPTY(&vdev->peer_list)) {
            /*
             * Now that there are no references to the peer, we can
             * release the peer reference lock.
             */
            qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
            /*
             * Check if the parent vdev was waiting for its peers to be
             * deleted, in order for it to be deleted too.
             */
            if (vdev->delete.pending) {
                ol_txrx_vdev_delete_cb vdev_delete_cb = vdev->delete.callback;
                void *vdev_delete_context = vdev->delete.context;

                TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1,
                        "%s: deleting vdev object %pK "
                        "(%02x:%02x:%02x:%02x:%02x:%02x)"
                        " - its last peer is done\n",
                        __func__, vdev,
                        vdev->mac_addr.raw[0], vdev->mac_addr.raw[1],
                        vdev->mac_addr.raw[2], vdev->mac_addr.raw[3],
                        vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);
                /* all peers are gone, go ahead and delete it */
                qdf_mem_free(vdev);
                if (vdev_delete_cb) {
                    vdev_delete_cb(vdev_delete_context);
                }
            }
        } else {
            qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
        }

        qdf_mem_copy(peer_cookie.mac_addr, peer->mac_addr.raw, QDF_MAC_ADDR_SIZE);
        peer_cookie.ctx = NULL;
        /* Deinitialize peer rate stats module if PEER_RATE_STATS is enabled in INI cfg*/
        if (soc->wlanstats_enabled) {
            peer_cookie.ctx = peer->wlanstats_ctx;
        }
        wdi_event_handler(WDI_EVENT_PEER_DESTROY, (struct ol_txrx_pdev_t *)pdev, (void *)&peer_cookie,
                          peer->peer_ids[0], WDI_NO_VAL);

        if (soc->wlanstats_enabled) {
            peer->wlanstats_ctx = NULL;
        }
        if (scn) {

            /* If we can't find scn, it is a forcefull delete, in which case
               the mempool_ol_ath_peer would have already been destroyed  */
            if (scn->soc) {
                ol_txrx_soc_handle soc = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);

                if (soc->ol_ops->peer_unref_delete) {
                    soc->ol_ops->peer_unref_delete(pdev->ctrl_pdev,
                    peer->mac_addr.raw, vdev_mac_addr.raw,
                    opmode, peer->ctrl_peer, NULL);
                }

                qdf_mempool_free(scn->soc->qdf_dev, scn->soc->mempool_ol_ath_peer, peer);

            } else {
                /* In case prealloc_disabled is true, we need to free the peer anyway */
                qdf_mempool_free(pdev->osdev, NULL, peer);

                if (is_connected_sta_peer)
                    qdf_atomic_inc(&scn->peer_count);

            }

        }

    } else {
        qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
    }
}

void
ol_txrx_peer_detach(void *peer_handle, uint32_t flags)
{
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)peer_handle;
    /* redirect the peer's rx delivery function to point to a discard func */
    peer->rx_opt_proc = ol_rx_discard;

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO2,
            "%s:peer %pK (%02x:%02x:%02x:%02x:%02x:%02x)\n",
            __func__, peer,
            peer->mac_addr.raw[0], peer->mac_addr.raw[1],
            peer->mac_addr.raw[2], peer->mac_addr.raw[3],
            peer->mac_addr.raw[4], peer->mac_addr.raw[5]);

#if 0 // BEELINER COMMENTED THIS
    /* Free the rate-control context. */
    ol_ratectrl_peer_ctxt_detach(peer->rc_node);
#endif

    /*
     * Remove the reference added during peer_attach.
     * The peer will still be left allocated until the
     * PEER_UNMAP message arrives to remove the other
     * reference, added by the PEER_MAP message.
     */
    ol_txrx_peer_unref_delete((ol_txrx_peer_handle) peer);
}

int
ol_txrx_get_tx_pending(struct cdp_pdev *pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    union ol_tx_desc_list_elem_t *p_tx_desc;
    int total;
    int unused = 0;
    struct ol_txrx_psoc_t *dp_soc = (struct ol_txrx_psoc_t *) pdev->soc;

    total = ol_cfg_target_tx_credit(dp_soc->psoc_obj);

    /*
     * Iterate over the tx descriptor freelist to see how many are available,
     * and thus by inference, how many are in use.
     * This iteration is inefficient, but this code is called during
     * cleanup, when performance is not paramount.  It is preferable
     * to do have a large inefficiency during this non-critical
     * cleanup stage than to have lots of little inefficiencies of
     * updating counters during the performance-critical tx "fast path".
     *
     * Use the lock to ensure there are no new allocations made while
     * we're trying to count the number of allocations.
     * This function is expected to be used only during cleanup, at which
     * time there should be no new allocations made, but just to be safe...
     */
    qdf_spin_lock_bh(&pdev->tx_mutex);
    p_tx_desc = pdev->tx_desc.freelist;
    while (p_tx_desc) {
        p_tx_desc = p_tx_desc->next;
        unused++;
    }
    qdf_spin_unlock_bh(&pdev->tx_mutex);

    return (total - unused);

}

/*--- debug features --------------------------------------------------------*/

unsigned g_txrx_print_level = TXRX_PRINT_LEVEL_INFO1; /* default */

void ol_txrx_print_level_set(unsigned level)
{
#if !TXRX_PRINT_ENABLE
    qdf_print(
            "The driver is compiled without TXRX prints enabled.\n"
            "To enable them, recompile with TXRX_PRINT_ENABLE defined.");
#else
    qdf_print("TXRX printout level changed from %d to %d",
            g_txrx_print_level, level);
    g_txrx_print_level = level;
#endif
}

static inline
u_int64_t OL_TXRX_STATS_PTR_TO_U64(struct ol_txrx_stats_req_internal *req)
{
    return (u_int64_t) ((size_t) req);
}

static inline
struct ol_txrx_stats_req_internal * OL_TXRX_U64_TO_STATS_PTR(u_int64_t cookie)
{
    return (struct ol_txrx_stats_req_internal *) ((size_t) cookie);
}

void
ol_txrx_fw_stats_cfg(
        struct cdp_vdev *vdev_handle,
        u_int8_t cfg_stats_type,
        u_int32_t cfg_val)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    u_int64_t dummy_cookie = 0;
    htt_h2t_dbg_stats_get(
            vdev->pdev->htt_pdev,
            0 /* upload mask */,
            0 /* reset mask */,
            cfg_stats_type,
            cfg_val,
            dummy_cookie,
            0 /* vdevid */);
}

#define OL_TXRX_HOST_PDEV_STATS 0x04
#define OL_TXRX_HOST_PDEV_EXTD_STATS 0x100
static void
ol_txrx_update_pdev_dbg_stats(struct cdp_pdev *pdev_handle,
                              void *data)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    ol_dbg_stats *stats = (ol_dbg_stats *)data;
    ol_dbg_tx_stats *tx = &stats->tx;
    ol_dbg_rx_stats *rx = &stats->rx;

    ol_dbg_stats *pdev_stats = &pdev->dbg_stats;
    ol_dbg_tx_stats *pdev_tx_stats = &pdev_stats->tx;
    ol_dbg_rx_stats *pdev_rx_stats = &pdev_stats->rx;

    /* Tx stats */
    pdev_tx_stats->comp_queued = tx->comp_queued;
    pdev_tx_stats->comp_delivered = tx->comp_delivered;
    pdev_tx_stats->msdu_enqued = tx->msdu_enqued;
    pdev_tx_stats->mpdu_enqued = tx->mpdu_enqued;
    pdev_tx_stats->wmm_drop = tx->wmm_drop;
    pdev_tx_stats->local_enqued = tx->local_enqued;
    pdev_tx_stats->local_freed = tx->local_freed;
    pdev_tx_stats->hw_queued = tx->hw_queued;
    pdev_tx_stats->hw_reaped = tx->hw_reaped;
    pdev_tx_stats->underrun = tx->underrun;
    pdev_tx_stats->hw_paused = tx->hw_paused;
    pdev_tx_stats->tx_abort = tx->tx_abort;
    pdev_tx_stats->mpdus_requed = tx->mpdus_requed;
    pdev_tx_stats->tx_xretry = tx->tx_xretry;
    pdev_tx_stats->data_rc = tx->data_rc;
    pdev_tx_stats->self_triggers = tx->self_triggers;
    pdev_tx_stats->sw_retry_failure = tx->sw_retry_failure;
    pdev_tx_stats->illgl_rate_phy_err = tx->illgl_rate_phy_err;
    pdev_tx_stats->pdev_cont_xretry = tx->pdev_cont_xretry;
    pdev_tx_stats->pdev_tx_timeout = tx->pdev_tx_timeout;
    pdev_tx_stats->pdev_resets = tx->pdev_resets;
    pdev_tx_stats->stateless_tid_alloc_failure = tx->stateless_tid_alloc_failure;
    pdev_tx_stats->phy_underrun = tx->phy_underrun;
    pdev_tx_stats->txop_ovf = tx->txop_ovf;
    pdev_tx_stats->seq_posted = tx->seq_posted;
    pdev_tx_stats->seq_failed_queueing = tx->seq_failed_queueing;
    pdev_tx_stats->seq_completed = tx->seq_completed;
    pdev_tx_stats->seq_restarted = tx->seq_restarted;
    pdev_tx_stats->mu_seq_posted = tx->mu_seq_posted;
    pdev_tx_stats->mpdus_sw_flush = tx->mpdus_sw_flush;
    pdev_tx_stats->mpdus_hw_filter = tx->mpdus_hw_filter;
    pdev_tx_stats->mpdus_truncated = tx->mpdus_truncated;
    pdev_tx_stats->mpdus_ack_failed = tx->mpdus_ack_failed;
    pdev_tx_stats->mpdus_expired = tx->mpdus_expired;

    /* Only NON-TLV */
    pdev_tx_stats->mc_drop = tx->mc_drop;

    /* Rx stats */
    pdev_rx_stats->mid_ppdu_route_change = rx->mid_ppdu_route_change;
    pdev_rx_stats->status_rcvd = rx->status_rcvd;
    pdev_rx_stats->r0_frags = rx->r0_frags;
    pdev_rx_stats->r1_frags = rx->r1_frags;
    pdev_rx_stats->r2_frags = rx->r2_frags;
    pdev_rx_stats->htt_msdus = rx->htt_msdus;
    pdev_rx_stats->htt_mpdus = rx->htt_mpdus;
    pdev_rx_stats->loc_msdus = rx->loc_msdus;
    pdev_rx_stats->loc_mpdus = rx->loc_mpdus;
    pdev_rx_stats->oversize_amsdu = rx->oversize_amsdu;
    pdev_rx_stats->phy_errs = rx->phy_errs;
    pdev_rx_stats->phy_err_drop = rx->phy_err_drop;
    pdev_rx_stats->mpdu_errs = rx->mpdu_errs;
    pdev_rx_stats->pdev_rx_timeout = rx->pdev_rx_timeout;
    pdev_rx_stats->rx_ovfl_errs = rx->rx_ovfl_errs;

    /* mem stats */
    pdev_stats->mem.iram_free_size = stats->mem.iram_free_size;
    pdev_stats->mem.dram_free_size = stats->mem.dram_free_size;

    /* Only Non-TLV */
    pdev_stats->mem.sram_free_size = stats->mem.sram_free_size;

    return;
}

static void
ol_txrx_update_pdev_extd_stats(struct cdp_pdev *pdev_handle, void *data)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    struct ol_txrx_pdev_extd_stats *extd_stats = (struct ol_txrx_pdev_extd_stats *)data;

    qdf_mem_copy(pdev->pdev_data_stats.rx.rx_mcs,
                 extd_stats->rx_mcs, sizeof(extd_stats->rx_mcs));
    qdf_mem_copy(pdev->pdev_data_stats.tx.tx_mcs,
                 extd_stats->tx_mcs, sizeof(extd_stats->tx_mcs));
}

void
ol_txrx_update_pdev_host_stats(struct cdp_pdev *pdev_handle,
                               void *data,
                               uint16_t stats_id)
{
    switch (stats_id) {
        case OL_TXRX_HOST_PDEV_STATS:
             ol_txrx_update_pdev_dbg_stats(pdev_handle, data);
             break;
        case OL_TXRX_HOST_PDEV_EXTD_STATS:
             ol_txrx_update_pdev_extd_stats(pdev_handle, data);
             break;
        default:
              qdf_print("%s invalid stats_id %d ", __func__, stats_id);
    }
}

void ol_txrx_update_vdev_ingress(struct cdp_vdev *vdev, void *data)
{
   ol_tx_stats_inc_pkt_cnt((ol_txrx_vdev_handle)vdev, data);
}

void
ol_txrx_update_vdev_host_stats(struct cdp_vdev *vdev_handle,
                               void *data,
                               uint16_t stats_id)
{
    switch (stats_id) {
        case OL_TXRX_VDEV_STATS_TX_INGRESS:
             ol_txrx_update_vdev_ingress(vdev_handle, data);
             break;
        default:
             qdf_info("invalid stats_id %d", stats_id);
             break;
    }
}

A_STATUS
ol_txrx_fw_stats_get(
        struct cdp_vdev *vdev_handle,
        struct ol_txrx_stats_req *req,
        bool per_vdev,
        bool response_expected)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    u_int64_t cookie;
    struct ol_txrx_stats_req_internal *non_volatile_req;
    struct ol_ath_softc_net80211 *scn = NULL;
    int32_t mutex_ret = 0;
    u_int8_t vdev_id;

    if (!pdev ||
            req->stats_type_upload_mask >= 1 << HTT_DBG_NUM_STATS ||
            req->stats_type_reset_mask >= 1 << HTT_DBG_NUM_STATS )
    {
        return A_ERROR;
    }

    /*
     * If per vap stats is requested send vdev+1 as vdevid to FW.
     * Since FW sends consolidated radio stats on vdevid 0
     * For example if stats is requested for vdev 1, send 2 to FW.
     * This logic need to be fixed and 1->1 mapping to be introduced.
     */
    if (per_vdev){
        vdev_id = vdev->vdev_id + 1;
    } else {
        vdev_id = 0;
    }
    scn = (struct ol_ath_softc_net80211 *)
               lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)pdev->ctrl_pdev);
    if (!scn) {
        return A_EINVAL;
    }

    /*
     * Allocate a non-transient stats request object.
     * (The one provided as an argument is likely allocated on the stack.)
     */
    non_volatile_req = qdf_mem_malloc(sizeof(*non_volatile_req));
    if (! non_volatile_req) {
        return A_NO_MEMORY;
    }
    /* copy the caller's specifications */
    non_volatile_req->base = *req;
    non_volatile_req->serviced = 0;
    non_volatile_req->offset = 0;

    /* use the non-volatile request object's address as the cookie */
    cookie = OL_TXRX_STATS_PTR_TO_U64(non_volatile_req);
    if (htt_h2t_dbg_stats_get(
                pdev->htt_pdev,
                req->stats_type_upload_mask,
                req->stats_type_reset_mask,
                HTT_H2T_STATS_REQ_CFG_STAT_TYPE_INVALID, 0,
                cookie, (u_int32_t)vdev_id))
    {
        qdf_mem_free(non_volatile_req);
        return A_ERROR;
    }

    if (req->wait.blocking) {
        /* if fw hang, apps e.g. athstats will totally hang the whole AP, user has to power cycle the AP,
           mutex acquire here should timeout in 3000ms */
        mutex_ret = qdf_semaphore_acquire_timeout(&scn->soc->stats_sem, 3000);
    }

    if (!req->stats_type_upload_mask) {
        qdf_mem_free(non_volatile_req);
    }

    if (mutex_ret){
        return A_EBUSY;
    }else{
        return A_OK;
    }
}

#ifdef WLAN_FEATURE_FASTPATH

#if PEER_FLOW_CONTROL
A_STATUS
ol_txrx_host_msdu_ttl_stats(
        struct cdp_vdev *vdev_handle,
        struct ol_txrx_stats_req *req)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;

    if(pdev) {
        /* Display MSDU TTL counter */
        qdf_print("### HOST MSDU TTL Stats ###\nHost_msdu_ttl     :\t%d",pdev->pflow_msdu_ttl_cnt);
    }
    return 0;
}
#endif
A_STATUS
ol_txrx_host_stats_get(
        struct cdp_vdev *vdev_handle,
        struct ol_txrx_stats_req *req)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    u_int8_t i;
    struct ol_ath_softc_net80211 *scn;
    scn = (struct ol_ath_softc_net80211 *)
               lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)pdev->ctrl_pdev);

    qdf_print("++++++++++ CE STATISTICS +++++++++++");
    for (i=0; scn && i<STATS_MAX_RX_CES; i++) {
       /* print only 8 CE stats for Peregrine */
       if ((lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_AR9888) &&
                      (i >= STATS_MAX_RX_CES_PEREGRINE)) {
           break;
       }
       qdf_print("CE%d Host sw_index (dst_ring):     %d",i, scn->soc->pkt_stats.sw_index[i]);
       qdf_print("CE%d Host write_index (dst_ring):  %d",i, scn->soc->pkt_stats.write_index[i]);
    }

    /* HACK */
    qdf_print("++++++++++ HOST TX STATISTICS +++++++++++");
    qdf_print("Ol Tx Desc In Use\t:  %u", pdev->pdev_data_stats.tx.desc_in_use);
    qdf_print("Ol Tx Desc Failed\t:  %u", pdev->pdev_stats.err.desc_alloc_fail);
    qdf_print("CE Ring (4) Full \t:  %u", pdev->pdev_stats.tx_i.dropped.ring_full);
    qdf_print("DMA Map Error    \t:  %u", pdev->pdev_stats.tx_i.dropped.dma_error);
    qdf_print("Tx pkts completed\t:  %u", pdev->pdev_stats.tx.comp_pkt.num);
    qdf_print("Tx bytes completed\t:  %llu", pdev->pdev_stats.tx.comp_pkt.bytes);
    qdf_print("Tx pkts from stack\t:  %u", pdev->pdev_stats.tx_i.rcvd.num);

    qdf_print("\n");
    qdf_print("++++++++++ HOST RX STATISTICS +++++++++++");
    qdf_print("Rx pkts completed\t:  %u", pdev->pdev_stats.rx.to_stack.num);
    qdf_print("Rx bytes completed\t:  %llu", pdev->pdev_stats.rx.to_stack.bytes);

    qdf_print("++++++++++ HOST FLOW CONTROL STATISTICS +++++++++++");
    qdf_print("Receive from stack count: %u", pdev->pdev_stats.tx_i.rcvd.num);
    qdf_print("non queued pkt count: %u", pdev->pdev_data_stats.tx.fl_ctrl.fl_ctrl_avoid);
    qdf_print("queued pkt count: %u", pdev->pdev_data_stats.tx.fl_ctrl.fl_ctrl_enqueue);
    qdf_print("queue overflow count: %u", pdev->pdev_data_stats.tx.fl_ctrl.fl_ctrl_discard);
    return 0;
}

void
ol_txrx_host_stats_clr(struct cdp_vdev *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    pdev->pdev_data_stats.tx.desc_in_use = 0;
    pdev->pdev_stats.err.desc_alloc_fail = 0;
    pdev->pdev_stats.tx_i.dropped.ring_full = 0;
    pdev->pdev_stats.tx.comp_pkt.num = 0;
    pdev->pdev_stats.tx.comp_pkt.bytes = 0;
    pdev->pdev_stats.tx_i.rcvd.num = 0;
    pdev->pdev_stats.tx_i.rcvd.bytes = 0;
    /*Rx*/
    pdev->pdev_stats.rx.to_stack.num = 0;
    pdev->pdev_stats.rx.to_stack.bytes = 0;
    pdev->pdev_data_stats.tx.fl_ctrl.fl_ctrl_avoid = 0;
    pdev->pdev_data_stats.tx.fl_ctrl.fl_ctrl_enqueue = 0;
    pdev->pdev_data_stats.tx.fl_ctrl.fl_ctrl_discard = 0;
#if PEER_FLOW_CONTROL
    pdev->pflow_msdu_ttl_cnt = 0;
#endif

}

#define OL_TXRX_WMI_PEER_STATS_OFFSET 2
static void ol_txrx_update_host_peer_extd_stats(void *handle, void *buf)
{
    /* Add offset to buf to point it ol_txrx_host_peer_extd_stats */
    struct ol_txrx_host_peer_extd_stats *peer_stats = (struct ol_txrx_host_peer_extd_stats *)
                                                      (((uint32_t *)buf) + OL_TXRX_WMI_PEER_STATS_OFFSET);
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)handle;
    struct ol_txrx_host_peer_extd_stats *extd_stats;

    extd_stats = &peer->extd_stats;
    peer->stats.tx.inactive_time = peer_stats->inactive_time;
    peer->stats.tx.dot11_tx_pkts.bytes = peer_stats->peer_tx_bytes;
    peer->stats.rx.dot11_rx_pkts.bytes = peer_stats->peer_rx_bytes;
    extd_stats->peer_chain_rssi = peer_stats->peer_chain_rssi;
    extd_stats->rx_duration = peer_stats->rx_duration;

    if (peer_stats->last_tx_rate_code) {
       peer->stats.tx.tx_ratecode = peer_stats->last_tx_rate_code & 0xff;
       peer->stats.tx.tx_flags = ((peer_stats->last_tx_rate_code >> 8) & 0xff);
    }

    if (OL_TXRX_PEER_EXTD_STATS_SGI_CONFIG_GET(peer_stats->sgi_count)) {
        peer->stats.tx.sgi_count[1] = OL_TXRX_PEER_EXTD_STATS_SGI_COUNT_GET(peer_stats->sgi_count);
    }

    if (peer_stats->last_tx_power) {
        peer->stats.tx.tx_power = (uint8_t)peer_stats->last_tx_power;
    }
}

static void ol_txrx_update_host_peer_retry_stats(void *handle, void *buf)
{
    /* Add offset to buf to point it ol_txrx_host_peer_retry_stats */
    struct ol_txrx_host_peer_retry_stats *peer_retry_stats = (struct ol_txrx_host_peer_retry_stats *)
                                                      (((uint32_t *)buf) + OL_TXRX_WMI_PEER_STATS_OFFSET);

    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)handle;
    struct cdp_peer_stats *stats = &peer->stats;
    struct cdp_tx_stats *tx = &stats->tx;
    uint32_t msdus_retried;
    uint32_t msdus_success;
    uint32_t msdus_mul_retried;
    uint32_t msdus_failed;

    msdus_retried = peer_retry_stats->msdus_retried ;
    msdus_success = peer_retry_stats->msdus_success;
    msdus_mul_retried = peer_retry_stats->msdus_mul_retried;
    msdus_failed = peer_retry_stats->msdus_failed;
    tx->failed_retry_count += msdus_failed;
    tx->retry_count += msdus_retried - msdus_failed;
    tx->multiple_retry_count += msdus_mul_retried - msdus_failed;
 }

/* This API should not be called on per-packet basis*/
#if QCA_SUPPORT_SON
static int32_t ol_txrx_compute_ack_rssi(struct cdp_tx_stats *tx,
                                        struct wlan_objmgr_pdev *pdev,
                                        uint8_t vdev_id) {
    int32_t rssi_avg, rssi_total;
    uint8_t stream, num_rxchain, rx_chainmask;
    struct wlan_objmgr_vdev *vdev;

    if ((tx == NULL) || (pdev == NULL)) {
        qdf_err("pdev or tx stats is NULL");
        return -EINVAL;
    }

    vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id,
                                                WLAN_MLME_SB_ID);
    if (vdev == NULL) {
        qdf_err("vdev is NULL");
        return -EINVAL;
    }

    rx_chainmask = wlan_vdev_mlme_get_rxchainmask(vdev);
    num_rxchain = stream = rssi_total = 0;
    while (rx_chainmask && (stream < WME_AC_MAX)) {
        if (rx_chainmask & 0x1) {
            num_rxchain++;
            rssi_total += tx->rssi_chain[stream];
        }
        rx_chainmask = rx_chainmask >> 1;
        stream++;
    }
    rssi_avg = rssi_total / num_rxchain;
    wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);

    return rssi_avg;
}
#endif

static void ol_txrx_update_host_peer_stats(void *handle,
                                           uint32_t last_tx_rate_mcs,
                                           void *buf)
{
    /* Add offset to buf to point it ol_txrx_host_peer_stats */
    struct ol_txrx_host_peer_stats *peer_stats = (struct ol_txrx_host_peer_stats *)
                                                 (((uint32_t *)buf) +
                                                 OL_TXRX_WMI_PEER_STATS_OFFSET);
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)handle;
    struct cdp_peer_stats *stats = &peer->stats;
    struct cdp_tx_stats *tx = &stats->tx;
    struct cdp_rx_stats *rx = &stats->rx;
    uint8_t ac_ind;
#if QCA_SUPPORT_SON
    struct cdp_interface_peer_stats son_bs_stats_intf;
    struct ol_txrx_pdev_t *pdev;
    uint32_t ack_rssi;
#endif

    if (!peer->vdev)
        return;

#if QCA_SUPPORT_SON
    pdev = peer->vdev->pdev;
    if (!peer->bss_peer) {
        qdf_mem_zero(&son_bs_stats_intf, sizeof(son_bs_stats_intf));

        if (ol_txrx_is_target_ar900b((struct cdp_vdev *)peer->vdev)) {
            if (peer_stats->peer_rssi &&
                       son_match_peer_rssi_seq((struct wlan_objmgr_peer *)peer->ctrl_peer,
                                               peer_stats->peer_rssi_seq_num)) {
                son_bs_stats_intf.rssi_changed = true;
            }
        } else {
            if (peer_stats->peer_rssi &&
                                    peer_stats->peer_rssi_changed) {
                son_bs_stats_intf.rssi_changed = true;
            }
        }
	/* This function should not be called per packet */
        ack_rssi = ol_txrx_compute_ack_rssi(tx,
                        (struct wlan_objmgr_pdev *)pdev->ctrl_pdev,
                        peer->vdev->vdev_id);
        if ((peer_stats->peer_rssi &&  son_bs_stats_intf.rssi_changed) ||
            (peer_stats->peer_tx_rate && (tx->last_tx_rate !=  peer_stats->peer_tx_rate)) ||
            ((ack_rssi >= 0) && (son_bs_stats_intf.ack_rssi != ack_rssi))) {
             son_bs_stats_intf.peer_hdl = peer->ctrl_peer;
             son_bs_stats_intf.last_peer_tx_rate = tx->last_tx_rate;
             son_bs_stats_intf.peer_tx_rate = peer_stats->peer_tx_rate;
             son_bs_stats_intf.peer_rssi = peer_stats->peer_rssi;
             son_bs_stats_intf.tx_packet_count = tx->ucast.num;
             son_bs_stats_intf.rx_packet_count = rx->to_stack.num;
             son_bs_stats_intf.tx_byte_count = tx->tx_success.bytes;
             son_bs_stats_intf.rx_byte_count = rx->to_stack.bytes;
             son_bs_stats_intf.per = tx->last_per;
             son_bs_stats_intf.ack_rssi = ack_rssi;
             wdi_event_handler(WDI_EVENT_PEER_STATS, (struct ol_txrx_pdev_t *)pdev, (void *)&son_bs_stats_intf,
                               peer->peer_ids[0], WDI_NO_VAL);

        }
    }
#endif

    if (peer->bss_peer) {
        tx->mcast_last_tx_rate = peer_stats->peer_tx_rate;
        tx->mcast_last_tx_rate_mcs = last_tx_rate_mcs;
    }
    else {
        tx->last_tx_rate = peer_stats->peer_tx_rate;
        tx->last_tx_rate_used += peer_stats->peer_tx_rate/1000;
        rx->last_rx_rate = peer_stats->peer_rx_rate;
        tx->last_tx_rate_mcs = last_tx_rate_mcs;
    }
    rx->rx_rssi_measured_time = OS_GET_TIMESTAMP();

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    tx->fw_tx_cnt++;
    tx->fw_tx_bytes += peer_stats->txbytes;
    if (!tx->fw_txcount && peer_stats->totalsubframes) {
             /* No previous packet count to include in PER
                and some frames transmitted */
        tx->last_per = peer_stats->currentper;
    } else if (peer_stats->totalsubframes) {
            /* Calculate a weighted average PER */
                     tx->last_per =
                     ((peer_stats->currentper * peer_stats->totalsubframes +
                       tx->last_per * tx->fw_txcount) /
                     (peer_stats->totalsubframes + tx->fw_txcount));
    } else {
            /* Else there is no updated packet count - decrease by 25% */
             tx->last_per = (tx->last_per * 3) >> 2;
    }
    tx->fw_txcount += peer_stats->totalsubframes;
    tx->fw_max4msframelen += peer_stats->max4msframelen;
    tx->fw_ratecount += peer_stats->txratecount;
    tx->retries +=  peer_stats->retries;
    for (ac_ind = 0; ac_ind < WME_AC_MAX; ac_ind++) {
         tx->ac_nobufs[ac_ind] += peer_stats->nobuffs[ac_ind] ;
    }
#endif
    for (ac_ind = 0; ac_ind < WME_AC_MAX; ac_ind++) {
         tx->excess_retries_per_ac[ac_ind] += peer_stats->excretries[ac_ind];
    }
}

#define OL_TXRX_HOST_REQUEST_PEER_STATS 0x01
#define OL_TXRX_HOST_REQUEST_PEER_EXTD_STATS 0x80
#define OL_TXRX_HOST_REQUEST_PEER_RETRY_STAT 0x2000

void ol_txrx_update_peer_stats(void *handle,
                               void *buf,
                               uint32_t last_tx_rate_mcs,
                               uint32_t stats_id)
{
    if (!handle)
        return;

    switch(stats_id)
    {
        case OL_TXRX_HOST_REQUEST_PEER_STATS:
             ol_txrx_update_host_peer_stats(handle, last_tx_rate_mcs, buf);
             break;
        case OL_TXRX_HOST_REQUEST_PEER_EXTD_STATS:
             ol_txrx_update_host_peer_extd_stats(handle, buf);
             break;
        case OL_TXRX_HOST_REQUEST_PEER_RETRY_STAT:
             ol_txrx_update_host_peer_retry_stats(handle, buf);
             break;
        default:
             qdf_print("Invalid stats ID received");
    }
}

void
ol_txrx_host_ce_stats(struct cdp_vdev *vdev_handle)
{
}

#if ATH_SUPPORT_IQUE
A_STATUS
ol_txrx_host_me_stats(
        struct cdp_vdev *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    ol_txrx_aggregate_pdev_stats(pdev);
    qdf_info("++++++++++ HOST MCAST Ehance STATISTICS +++++++++++");
    qdf_info("Mcast recieved\t: %d", pdev->pdev_stats.tx_i.mcast_en.mcast_pkt.num);
    qdf_info("ME converted\t: %d", pdev->pdev_stats.tx_i.mcast_en.ucast);
    qdf_info("ME dropped (Map)\t: %d", pdev->pdev_stats.tx_i.mcast_en.dropped_map_error);
    qdf_info("ME dropped (alloc)\t: %d", pdev->pdev_stats.tx_i.mcast_en.fail_seg_alloc);
    qdf_info("ME dropped(internal)\t: %d", pdev->pdev_stats.tx_i.mcast_en.dropped_send_fail);
    qdf_info("ME dropped(own address)\t: %d", pdev->pdev_stats.tx_i.mcast_en.dropped_self_mac);
    qdf_info("ME bufs in use\t: %d", pdev->pdev_data_stats.mcast_enhance.num_me_buf);
    qdf_info("ME bufs in non pool allocation in use\t: %d", pdev->pdev_data_stats.mcast_enhance.num_me_nonpool);
    qdf_info("ME bufs in non pool allocation\t: %d", pdev->pdev_data_stats.mcast_enhance. num_me_nonpool_count);
    qdf_info("\n");

    return 0;
}
#endif
#endif

int
ol_txrx_fw_stats_handler(
        struct ol_txrx_pdev_t *pdev,
        u_int64_t cookie,
        u_int8_t *stats_info_list,
        u_int32_t vdev_id)
{
    enum htt_dbg_stats_type type;
    enum htt_dbg_stats_status status;
    int length = 0;
    u_int8_t *stats_data;
#if OL_STATS_WORK_QUEUE
    struct ol_txrx_fw_stats_info *stats_buffer = NULL;
#endif
    struct ol_txrx_stats_req_internal *req;
    int more = 0;
    struct ol_ath_softc_net80211 *scn;
    uint32_t target_type;

    lmac_get_pdev_target_type((struct wlan_objmgr_pdev *)pdev->ctrl_pdev,
				&target_type);

    scn = (struct ol_ath_softc_net80211 *)
               lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)pdev->ctrl_pdev);

    req = OL_TXRX_U64_TO_STATS_PTR(cookie);
    do {
        htt_t2h_dbg_stats_hdr_parse(
                stats_info_list, &type, &status, &length, &stats_data);
        if (status == HTT_DBG_STATS_STATUS_SERIES_DONE) {
            break;
        }
        if (status == HTT_DBG_STATS_STATUS_PRESENT ||
                status == HTT_DBG_STATS_STATUS_PARTIAL)
        {
            u_int8_t *buf;
            int bytes = 0;

            if (status == HTT_DBG_STATS_STATUS_PARTIAL) {
                more = 1;
            }

            if ((status != HTT_DBG_STATS_STATUS_PARTIAL) &&
                    (req->base.print.verbose || req->base.print.concise)) {
                /* provide the header along with the data */
#if OL_STATS_WORK_QUEUE
                {
                    stats_buffer = (struct ol_txrx_fw_stats_info *) qdf_mem_malloc(sizeof(struct ol_txrx_fw_stats_info) + length);
                    if(!stats_buffer) {
                        htt_t2h_stats_print(stats_info_list, req->base.print.concise, target_type, vdev_id);
                        break;
                    }
                    stats_buffer->scn = scn;
                    stats_buffer->vdev_id = vdev_id;
                    qdf_mem_copy(stats_buffer->stats_info, stats_info_list, length);

                    qdf_spin_lock_bh(&pdev->stats_buffer_lock);
                    TAILQ_INSERT_TAIL(&pdev->stats_buffer_list, stats_buffer, stats_info_list_elem);
                    qdf_spin_unlock_bh(&pdev->stats_buffer_lock);

                    qdf_sched_work(pdev->osdev, &(pdev->stats_wq));
                }
#else
                htt_t2h_stats_print(stats_info_list, req->base.print.concise, target_type, vdev_id);
#endif
            }
            switch (type) {
                case HTT_DBG_STATS_WAL_PDEV_TXRX:
                    bytes = sizeof(struct wlan_dbg_stats);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(struct wlan_dbg_stats);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;
                case HTT_DBG_STATS_RX_REORDER:
                    bytes = sizeof(struct rx_reorder_stats);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(struct rx_reorder_stats);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;
                case HTT_DBG_STATS_RX_RATE_INFO:
                    bytes = sizeof(wlan_dbg_rx_rate_info_t);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(wlan_dbg_rx_rate_info_t);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_TX_RATE_INFO:
                    bytes = sizeof(wlan_dbg_tx_rate_info_t);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(wlan_dbg_tx_rate_info_t);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;
                case HTT_DBG_STATS_TIDQ:
                    bytes = sizeof(struct wlan_dbg_tidq_stats);
                    if (req->base.copy.buf) {
#ifdef BIG_ENDIAN_HOST
                        struct wlan_dbg_tidq_stats *tidq_stats = NULL;
                        int *tmp_word = NULL;
                        int *tmp_word1 = NULL;
                        u_int16_t *tmp_short = NULL;
                        int i;
#endif
                        int limit;

                        limit = sizeof(struct wlan_dbg_tidq_stats);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
#ifdef BIG_ENDIAN_HOST
                        tidq_stats = (struct wlan_dbg_tidq_stats *)buf;

                        tmp_word = (int *)&tidq_stats->txq_st.num_pkts_queued[0];
                        for(i = 0; i < DBG_STATS_MAX_HWQ_NUM/sizeof(u_int16_t); i++)
                        {
                            *tmp_word = __le32_to_cpu(*tmp_word);
                            tmp_short = (u_int16_t *)tmp_word;
                            *tmp_short = __le16_to_cpu(*tmp_short);
                            tmp_short++;
                            *tmp_short = __le16_to_cpu(*tmp_short);

                            tmp_word++;
                        }

                        tmp_word = (int *)&tidq_stats->txq_st.tid_sw_qdepth[0];
                        tmp_word1 = (int *)&tidq_stats->txq_st.tid_hw_qdepth[0];
                        for(i = 0; i < DBG_STATS_MAX_TID_NUM/sizeof(u_int16_t); i++)
                        {
                            *tmp_word = __le32_to_cpu(*tmp_word);
                            tmp_short = (u_int16_t *)tmp_word;
                            *tmp_short = __le16_to_cpu(*tmp_short);
                            tmp_short++;
                            *tmp_short = __le16_to_cpu(*tmp_short);

                            *tmp_word1 = __le32_to_cpu(*tmp_word1);
                            tmp_short = (u_int16_t *)tmp_word1;
                            *tmp_short = __le16_to_cpu(*tmp_short);
                            tmp_short++;
                            *tmp_short = __le16_to_cpu(*tmp_short);

                            tmp_word++;
                            tmp_word1++;
                        }
#endif
                    }
                    break;

                case HTT_DBG_STATS_TXBF_INFO:
                    bytes = sizeof(struct wlan_dbg_txbf_data_stats);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(struct wlan_dbg_txbf_data_stats);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_SND_INFO:
                    bytes = sizeof(struct wlan_dbg_txbf_snd_stats);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(struct wlan_dbg_txbf_snd_stats);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_TX_SELFGEN_INFO:
                    bytes = sizeof(struct wlan_dbg_tx_selfgen_stats);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(struct wlan_dbg_tx_selfgen_stats);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_TX_MU_INFO:
                    bytes = sizeof(struct wlan_dbg_tx_mu_stats);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(struct wlan_dbg_tx_mu_stats);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_SIFS_RESP_INFO:
                    bytes = sizeof(wlan_dgb_sifs_resp_stats_t);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(wlan_dgb_sifs_resp_stats_t);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_TX_PPDU_LOG:
                    bytes = 0; /* TO DO: specify how many bytes are present */
                    /* TO DO: add copying to the requestor's buffer */
                    break;

                case HTT_DBG_STATS_ERROR_INFO:
                    bytes = sizeof(wlan_dbg_wifi2_error_stats_t);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(wlan_dbg_wifi2_error_stats_t);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_RESET_INFO:
                    bytes = sizeof(wlan_dbg_reset_stats_t);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(wlan_dbg_reset_stats_t);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_MAC_WDOG_INFO:
                    bytes = sizeof(wlan_dbg_mac_wdog_stats_t);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(wlan_dbg_mac_wdog_stats_t);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_TX_DESC_INFO:
                    bytes = sizeof(wlan_dbg_tx_desc_stats_t);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(wlan_dbg_tx_desc_stats_t);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_TX_FETCH_MGR_INFO:
                    bytes = sizeof(wlan_dbg_tx_fetch_mgr_stats_t);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(wlan_dbg_tx_fetch_mgr_stats_t);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_TX_PFSCHED_INFO:
                    bytes = sizeof(wlan_dbg_tx_pf_sched_stats_t);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(wlan_dbg_tx_pf_sched_stats_t);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_TX_PATH_STATS_INFO:
                    bytes = sizeof(wlan_dbg_tx_path_stats_t);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(wlan_dbg_tx_path_stats_t);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_HALPHY_INFO:
                    bytes = sizeof(wlan_halphy_dbg_stats_t);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(wlan_halphy_dbg_stats_t);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                case HTT_DBG_STATS_COEX_INFO:
                    bytes = sizeof(wlan_coex_dbg_stats_t);
                    if (req->base.copy.buf) {
                        int limit;

                        limit = sizeof(wlan_coex_dbg_stats_t);
                        if (req->base.copy.byte_limit < limit) {
                            limit = req->base.copy.byte_limit;
                        }
                        buf = req->base.copy.buf + req->offset;
                        qdf_mem_copy(buf, stats_data, limit);
                    }
                    break;

                default:
                    break;
            }
            buf = req->base.copy.buf ? req->base.copy.buf : stats_data;
            if (req->base.callback.fp) {
                req->base.callback.fp(
                        req->base.callback.ctxt, type, buf, bytes);
            }
        }
        stats_info_list += length;
    } while (1);

    if (! more) {
        if (scn && req->base.wait.blocking) {
            qdf_semaphore_release(&scn->soc->stats_sem);
        }
        qdf_mem_free(req);
    }

    return more;
}

int ol_txrx_debug(struct cdp_vdev *vdev_handle, int debug_specs)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    if (debug_specs & TXRX_DBG_MASK_OBJS) {
#if TXRX_DEBUG_LEVEL > 5
        ol_txrx_pdev_display(vdev->pdev, 0);
#else
        qdf_print(
                "The pdev,vdev,peer display functions are disabled.\n"
                "To enable them, recompile with TXRX_DEBUG_LEVEL > 5.");
#endif
    }
    if (debug_specs & TXRX_DBG_MASK_STATS) {
#if TXRX_STATS_LEVEL != TXRX_STATS_LEVEL_OFF
        ol_txrx_stats_display((ol_txrx_pdev_handle)vdev->pdev);
#else
        qdf_print(
                "txrx stats collection is disabled.\n"
                "To enable it, recompile with TXRX_STATS_LEVEL on.");
#endif
    }
    if (debug_specs & TXRX_DBG_MASK_PROT_ANALYZE) {
#if defined(ENABLE_TXRX_PROT_ANALYZE)
        ol_txrx_prot_ans_display(vdev->pdev);
#else
        qdf_print(
                "txrx protocol analysis is disabled.\n"
                "To enable it, recompile with "
                "ENABLE_TXRX_PROT_ANALYZE defined.");
#endif
    }
    if (debug_specs & TXRX_DBG_MASK_RX_REORDER_TRACE) {
#if defined(ENABLE_RX_REORDER_TRACE)
        ol_rx_reorder_trace_display(vdev->pdev, 0, 0);
#else
        qdf_print(
                "rx reorder seq num trace is disabled.\n"
                "To enable it, recompile with "
                "ENABLE_RX_REORDER_TRACE defined.");
#endif

    }
    return 0;
}

int ol_txrx_aggr_cfg(struct cdp_vdev *vdev_handle,
        int max_subfrms_ampdu,
        int max_subfrms_amsdu)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    return htt_h2t_aggr_cfg_msg(vdev->pdev->htt_pdev,
            max_subfrms_ampdu,
            max_subfrms_amsdu);
}

#if TXRX_DEBUG_LEVEL > 5
void
ol_txrx_pdev_display(ol_txrx_pdev_handle pdev, int indent)
{
    struct ol_txrx_vdev_t *vdev;

    qdf_print("%*s%s:", indent, " ", "txrx pdev");
    qdf_print("%*spdev object: %pK", indent+4, " ", pdev);
    qdf_print("%*svdev list:", indent+4, " ");
    TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
        ol_txrx_vdev_display(vdev, indent+8);
    }
    ol_txrx_peer_find_display(pdev, indent+4);
    qdf_print("%*stx desc pool: %d elems @ %pK", indent+4, " ",
            pdev->tx_desc.pool_size, pdev->tx_desc.array);
    qdf_print("\n");
    htt_display(pdev->htt_pdev, indent);
}

void
ol_txrx_vdev_display(ol_txrx_vdev_handle vdev_handle, int indent)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_peer_t *peer;

    qdf_print("%*stxrx vdev: %pK", indent, " ", vdev);
    qdf_print("%*sID: %d", indent+4, " ", vdev->vdev_id);
    qdf_print("%*sMAC addr: %d:%d:%d:%d:%d:%d",
            indent+4, " ",
            vdev->mac_addr.raw[0], vdev->mac_addr.raw[1], vdev->mac_addr.raw[2],
            vdev->mac_addr.raw[3], vdev->mac_addr.raw[4], vdev->mac_addr.raw[5]);
    qdf_print("%*speer list:", indent+4, " ");
    TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
        ol_txrx_peer_display(peer, indent+8);
    }
}

void
ol_txrx_peer_display(ol_txrx_peer_handle peer, int indent)
{
    int i;

    qdf_print("%*stxrx peer: %pK", indent, " ", peer);
    for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++) {
        if (peer->peer_ids[i] != HTT_INVALID_PEER) {
            qdf_print("%*sID: %d", indent+4, " ", peer->peer_ids[i]);
        }
    }
}

#endif /* TXRX_DEBUG_LEVEL */

#if TXRX_STATS_LEVEL != TXRX_STATS_LEVEL_OFF
void
ol_txrx_stats_display(ol_txrx_pdev_handle pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;

    ol_txrx_aggregate_pdev_stats(pdev);
    qdf_print("txrx stats:");
    if (TXRX_STATS_LEVEL == TXRX_STATS_LEVEL_BASIC) {
        qdf_print("  tx: %llu msdus (%llu B)",
                pdev->pdev_stats.tx.comp_pkt.num,
                pdev->pdev_stats.tx.comp_pkt.bytes);
    } else { /* full */
        qdf_print(
                "  tx: sent %llu msdus (%llu B), "
                "rejected %llu (%llu B), dropped %llu (%llu B)",
                pdev->pdev_stats.tx.comp_pkt.num,
                pdev->pdev_stats.tx.comp_pkt.bytes,
                pdev->pdev_stats.tx_i.dropped.dropped_pkt.num,
                pdev->pdev_stats.tx_i.dropped.dropped_pkt.bytes,
                pdev->pdev_data_stats.tx.dropped.download_fail.pkts
                + pdev->pdev_stats.tx_i.dropped.dropped_pkt.num,
                + pdev->pdev_stats.tx.is_tx_no_ack.num,
                  pdev->pdev_data_stats.tx.dropped.download_fail.bytes
                + pdev->pdev_stats.tx_i.dropped.dropped_pkt.bytes
                + pdev->pdev_stats.tx.is_tx_no_ack.bytes);
        qdf_print(
                "    download fail: %llu (%llu B), "
                "target discard: %llu (%llu B), "
                "no ack: %llu ",
                pdev->pdev_data_stats.tx.dropped.download_fail.pkts,
                pdev->pdev_data_stats.tx.dropped.download_fail.bytes,
                pdev->pdev_stats.tx_i.dropped.dropped_pkt.num,
                pdev->pdev_stats.tx_i.dropped.dropped_pkt.bytes,
                pdev->pdev_stats.tx_i.dropped.desc_na.num);
    }
    qdf_print(
            "  rx: %lld ppdus %lld mpdus %llu msdus, %llu bytes %lld errs",
            pdev->pdev_stats.rx.rx_ppdus,
            pdev->pdev_stats.rx.rx_mpdus,
            pdev->pdev_stats.rx.to_stack.num,
            pdev->pdev_stats.rx.to_stack.bytes,
            pdev->pdev_data_stats.rx.err.mpdu_bad);
    if (TXRX_STATS_LEVEL == TXRX_STATS_LEVEL_FULL) {
        qdf_print(
                "    forwarded %llu msdus, %llu bytes",
                pdev->pdev_stats.rx.to_stack.num,
                pdev->pdev_stats.rx.to_stack.bytes);
    }
}

static void
ol_txrx_process_host_vdev_extd_stats(struct wlan_objmgr_psoc *psoc,
                                     void *data,
                                     uint8_t len)
{
     struct ol_txrx_host_vdev_extd_stats *vdev_extd_stats;
     struct wlan_objmgr_vdev *vdev;
     struct ol_txrx_vdev_t  *vdev_handle;
     vdev_extd_stats = (struct ol_txrx_host_vdev_extd_stats *)data;

     vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
                                                 vdev_extd_stats->vdev_id,
                                                 WLAN_MLME_SB_ID);
     if (vdev) {
         vdev_handle = wlan_vdev_get_dp_handle(vdev);
         if(vdev_handle)
            qdf_mem_copy(&(vdev_handle->vdev_extd_stats), vdev_extd_stats,
                             sizeof(*vdev_extd_stats));
         wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
     }
}

static void
ol_txrx_process_host_vdev_stats(struct wlan_objmgr_psoc *psoc,
                                void *data,
                                uint8_t len)
{
     struct ol_txrx_host_vdev_stats *vdev_stats;
     struct wlan_objmgr_vdev *vdev;
     struct ol_txrx_vdev_t *vdev_handle;
     vdev_stats = (struct ol_txrx_host_vdev_stats *)data;

     vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
                                                 vdev_stats->vdev_id,
                                                 WLAN_MLME_SB_ID);
     if (vdev) {
         vdev_handle = wlan_vdev_get_dp_handle(vdev);
         if(vdev_handle)
            qdf_mem_copy(&(vdev_handle->host_vdev_stats), vdev_stats,
                         sizeof(*vdev_stats));
         wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
     }
}

#define OL_TXRX_HOST_REQUEST_VDEV_STAT      0x08
#define OL_TXRX_HOST_REQUEST_VDEV_EXTD_STAT 0x100

int ol_txrx_process_wmi_host_vdev_stats(ol_txrx_soc_handle soc, void *data,
                                   uint32_t len, uint32_t stats_id)
{
    struct wlan_objmgr_psoc *psoc = ((struct ol_txrx_psoc_t *)soc)->psoc_obj;

    switch (stats_id) {
            case OL_TXRX_HOST_REQUEST_VDEV_STAT:
                 ol_txrx_process_host_vdev_stats(psoc, data, len);
                 break;
            case OL_TXRX_HOST_REQUEST_VDEV_EXTD_STAT:
                 ol_txrx_process_host_vdev_extd_stats(psoc, data, len);
                 break;
            default:
                 qdf_print("%s Invalid stats_id", __func__);
    }
    return 0;
}

int ol_txrx_get_vdev_extd_stats(struct cdp_vdev *vdev_handle, void *buffer)
{
     struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;

     qdf_mem_copy(buffer, &vdev->vdev_extd_stats,
                  sizeof(struct ol_txrx_host_vdev_extd_stats));

     return 0;
}

struct cdp_peer_stats *
ol_txrx_get_peer_stats(struct cdp_peer *peer_handle)
{
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)peer_handle;

    if (!peer)
        return NULL;

    return (struct cdp_peer_stats *)&peer->stats;
}

void ol_txrx_reset_peer_stats(struct cdp_peer *peer_handle)
{
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)peer_handle;

    if (!peer)
        return;

    qdf_mem_zero(&peer->stats, sizeof(peer->stats));
}

int
ol_txrx_get_vdev_stats(struct cdp_vdev *vdev_handle, void *buffer, bool is_aggr)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;

    qdf_assert(buffer);
    qdf_assert(vdev);

    if (is_aggr)
        ol_txrx_aggregate_vdev_stats(vdev, (struct cdp_vdev_stats *)buffer);
    else {
        qdf_mem_copy(buffer, &vdev->stats, sizeof(struct cdp_vdev_stats));
    }

    return 0;
}

static void
ol_txrx_prepare_radiostats(struct ol_txrx_data_stats *stats,
                           struct cdp_pdev_stats *pdev_stats,
                           struct ol_ath_radiostats *scn_stats)
{
    /* for scn_stats add stats from aggregated as well as non-aggregated
     * to get stats for error cases as well */
    if (!scn_stats)
        return;

    scn_stats->tx_bytes = pdev_stats->tx.ucast.bytes ;
    scn_stats->tx_num_data = pdev_stats->tx.ucast.num;
    scn_stats->tx_bawadv = stats->tx.tx_bawadv;
    scn_stats->tx_compaggr = pdev_stats->tx.ampdu_cnt;
    scn_stats->tx_compunaggr = pdev_stats->tx.non_ampdu_cnt;
    qdf_mem_copy(scn_stats->tx_mcs, stats->tx.tx_mcs,
            sizeof(scn_stats->tx_mcs));
    scn_stats->rx_bytes = stats->rx.rx_bytes + pdev_stats->rx.to_stack.bytes;
    scn_stats->rx_packets = stats->rx.rx_packets + pdev_stats->rx.to_stack.num;
    scn_stats->rx_num_data = pdev_stats->rx.unicast.num;
    scn_stats->rx_crcerr = stats->rx.rx_crcerr;
    scn_stats->rx_badmic = pdev_stats->rx.err.mic_err;
    scn_stats->rx_badcrypt = pdev_stats->rx.err.decrypt_err;
    scn_stats->rx_aggr = pdev_stats->rx.rx_aggr;;
    qdf_mem_copy(scn_stats->rx_mcs, stats->rx.rx_mcs,
            sizeof(scn_stats->rx_mcs));
    scn_stats->rx_last_msdu_unset_cnt = stats->rx.rx_last_msdu_unset_cnt;
}

int
ol_txrx_stats_publish(struct cdp_pdev *pdev_handle, void *buffer)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    struct ol_stats *buf = (struct ol_stats *) buffer;
    struct ol_ath_softc_net80211 * scn;
    struct ol_ath_radiostats *scn_stats = NULL;

    qdf_assert(buf);
    qdf_assert(pdev);

    ol_txrx_aggregate_pdev_stats(pdev);
    qdf_mem_copy(&buf->pdev_stats, &pdev->pdev_stats, sizeof(pdev->pdev_stats));
    qdf_mem_copy(&buf->legacy_stats, &pdev->pdev_data_stats, sizeof(pdev->pdev_data_stats));
    qdf_mem_copy(&buf->stats, &pdev->dbg_stats, sizeof(pdev->dbg_stats));
    scn = (struct ol_ath_softc_net80211 *)
               lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)pdev->ctrl_pdev);
    scn_stats = &buf->interface_stats;
    ol_txrx_prepare_radiostats(&pdev->pdev_data_stats, &pdev->pdev_stats, scn_stats);
    return TXRX_STATS_LEVEL;
}

#endif /* TXRX_STATS_LEVEL */

static int ol_txrx_get_ratekbps(int preamb, int mcs,
                                 int htflag, int gintval)
{
     return whal_mcs_to_kbps(preamb, mcs, htflag, gintval);
}

static void ol_txrx_set_rate_stats_cap(struct cdp_soc_t *soc_hdl,
                                       uint8_t val)
{
    struct ol_txrx_psoc_t *soc = (struct ol_txrx_psoc_t *)soc_hdl;

    soc->wlanstats_enabled = val;
}

static struct cdp_pdev_stats*
ol_txrx_get_pdev_stats(struct cdp_pdev *pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;

    qdf_assert_always(pdev);

    ol_txrx_aggregate_pdev_stats(pdev);

    return &pdev->pdev_stats;
}

static int
ol_txrx_get_radiostats(struct cdp_pdev *pdev_handle,
                       void *buf)
{
    struct ol_ath_radiostats *scn_stats = (struct ol_ath_radiostats *)buf;
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;

    qdf_assert_always(pdev);

    ol_txrx_aggregate_pdev_stats(pdev);
    ol_txrx_prepare_radiostats(&pdev->pdev_data_stats, &pdev->pdev_stats, scn_stats);

    return 0;
}

#if RX_CHECKSUM_OFFLOAD
void
ol_print_rx_cksum_stats(
        struct cdp_vdev *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    struct ol_txrx_data_stats *stats = &pdev->pdev_data_stats;

    if(!stats)
        return;
    qdf_print("++++++++++ RX Checksum Error STATISTICS +++++++++++");
    qdf_print(
            "    ipv4_cksum_err %llu msdus, %llu bytes",
            stats->rx.ipv4_cksum_err.pkts,
            stats->rx.ipv4_cksum_err.bytes);
    qdf_print(
            "    tcp_ipv4_cksum_err %llu msdus, %llu bytes",
            stats->rx.tcp_ipv4_cksum_err.pkts,
            stats->rx.tcp_ipv4_cksum_err.bytes);
    qdf_print(
            "    tcp_ipv6_cksum_err %llu msdus, %llu bytes",
            stats->rx.tcp_ipv6_cksum_err.pkts,
            stats->rx.tcp_ipv6_cksum_err.bytes);
    qdf_print(
            "    udp_ipv4_cksum_err %llu msdus, %llu bytes",
            stats->rx.udp_ipv4_cksum_err.pkts,
            stats->rx.udp_ipv4_cksum_err.bytes);
    qdf_print(
            "    udp_ipv6_cksum_err %llu msdus, %llu bytes",
            stats->rx.udp_ipv6_cksum_err.pkts,
            stats->rx.udp_ipv6_cksum_err.bytes);
}

void
ol_rst_rx_cksum_stats(struct cdp_vdev *vdev_handle)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    struct ol_txrx_data_stats *stats = &pdev->pdev_data_stats;

    qdf_print(".....Resetting RX Checksum Error Stats ");
    /* Rx */
    stats->rx.ipv4_cksum_err.pkts = 0;
    stats->rx.ipv4_cksum_err.bytes = 0;
    stats->rx.tcp_ipv4_cksum_err.pkts = 0;
    stats->rx.tcp_ipv4_cksum_err.bytes = 0;
    stats->rx.tcp_ipv6_cksum_err.pkts = 0;
    stats->rx.tcp_ipv6_cksum_err.bytes = 0;
    stats->rx.udp_ipv4_cksum_err.pkts = 0;
    stats->rx.udp_ipv4_cksum_err.bytes = 0;
    stats->rx.udp_ipv6_cksum_err.pkts = 0;
    stats->rx.udp_ipv6_cksum_err.bytes = 0;
}
#endif /* RX_CHECKSUM_OFFLOAD */
#if defined(ENABLE_TXRX_PROT_ANALYZE)

void
ol_txrx_prot_ans_display(ol_txrx_pdev_handle pdev)
{
    ol_txrx_prot_an_display(pdev->prot_an_tx_sent);
    ol_txrx_prot_an_display(pdev->prot_an_rx_sent);
}

#endif /* ENABLE_TXRX_PROT_ANALYZE */

void
ol_txrx_enable_enhanced_stats(struct cdp_pdev *pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;

    if(pdev->ap_stats_tx_cal_enable == 0) {
        if(pdev->cal_client_ctx)
            dp_cal_client_timer_start(pdev->cal_client_ctx);
    }
    pdev->ap_stats_tx_cal_enable = 1;
}

void
ol_txrx_disable_enhanced_stats(struct cdp_pdev *pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;

    if(pdev->ap_stats_tx_cal_enable == 1) {
        if(pdev->cal_client_ctx)
           dp_cal_client_timer_stop(pdev->cal_client_ctx);
    }
    pdev->ap_stats_tx_cal_enable = 0;
}

#define TX_DIR 1
static inline
void interframe_delay_stats(struct ol_txrx_vdev_t * vdev, struct sk_buff *skb, uint32_t dir)
{
    struct ol_txrx_interframe *interfrm = NULL;
    struct ol_txrx_pdev_t *pdev = NULL;
    qdf_ktime_t tstamp,cur_ifdtstamp,msdu_tstamp;

    if(!vdev){
        return;
    }

    pdev = vdev->pdev;
    if (!pdev)
        return;

    interfrm = (struct ol_txrx_interframe *)&vdev->interframe_delay_stats;

    /* Interframe Section */
    /* check first pkt */
    tstamp = ktime_get_real();
    if (interfrm->start_tick.tv64 == 0) {
        interfrm->start_tick = ktime_get_real();
        interfrm->last_frame_tick = ktime_get_real();
        interfrm->max_delay = ktime_get_real();
        interfrm->max_interframe_delay.tv64 = 0;
    } else {
        cur_ifdtstamp.tv64 = ktime_to_us(ktime_sub(ktime_get_real(), interfrm->last_frame_tick));
        interfrm->max_interframe_delay.tv64 = cur_ifdtstamp.tv64;

        if(interfrm->max_interframe_delay.tv64 < 200) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTERFRAME_BUCKET_200, 1);
        } else if(interfrm->max_interframe_delay.tv64 < 400) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTERFRAME_BUCKET_400, 1);
        } else if(interfrm->max_interframe_delay.tv64 < 600) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTERFRAME_BUCKET_600, 1);
        } else if(interfrm->max_interframe_delay.tv64 < 800) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTERFRAME_BUCKET_800, 1);
        } else if(interfrm->max_interframe_delay.tv64 < (10 * 1000)) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTERFRAME_BUCKET_10, 1);
        } else if(interfrm->max_interframe_delay.tv64 < (20 * 1000)) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTERFRAME_BUCKET_20, 1);
        } else if(interfrm->max_interframe_delay.tv64 < (30 * 1000)) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTERFRAME_BUCKET_30, 1);
        } else if(interfrm->max_interframe_delay.tv64 < (40 * 1000)) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTERFRAME_BUCKET_40, 1);
        } else if(interfrm->max_interframe_delay.tv64 < (50 * 1000)) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTERFRAME_BUCKET_50, 1);
        } else
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTERFRAME_BUCKET_MAX, 1);
    }
    interfrm->last_frame_tick.tv64 = tstamp.tv64;

    /*
     * Inter Radio section
     * Here only SKB tstamp will be taken, which was set
     * by in Rx path
     */
    if (skb->tstamp.tv64) {
        msdu_tstamp.tv64 = qdf_nbuf_get_timedelta_us(skb);
        if(msdu_tstamp.tv64 < 200) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTER_RADIO_BUCKET_200, 1);
        } else if(msdu_tstamp.tv64 < 400) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTER_RADIO_BUCKET_400, 1);
        } else if(msdu_tstamp.tv64 < 600) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTER_RADIO_BUCKET_600, 1);
        } else if(msdu_tstamp.tv64 < 800) {
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTER_RADIO_BUCKET_800, 1);
        } else
            PFLOW_CTRL_PDEV_DELAY_VIDEO_STATS_ADD(pdev, HOST_INTER_RADIO_BUCKET_200, 1);
    }
}

int
ol_txrx_classify_update(struct cdp_vdev *vdev_handle, qdf_nbuf_t msdu,
        enum txrx_direction dir,
        struct ol_txrx_nbuf_classify *nbuf_class)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;
    struct ol_txrx_peer_t *peer = NULL;

    if (!vdev->pdev->carrier_vow_config)
        return 1;

    ol_txrx_classify(vdev, msdu, dir, nbuf_class);

    if (dir == rx_direction && nbuf_class->pkt_tid == QDF_TID_VI)
        PFLOW_CTRL_PDEV_VIDEO_STATS_ADD(vdev->pdev,
                RX_VIDEO_TID_MSDU_DELIVERED_TO_STACK, 1);

    if (dir == tx_direction) {
        if (nbuf_class->pkt_tid == QDF_TID_VI) {
            PFLOW_CTRL_PDEV_VIDEO_STATS_ADD(vdev->pdev,
                    TX_VIDEO_TID_MSDU_TOTAL_LINUX_SUBSYSTEM, 1);
        }
        peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)vdev->pdev,
                nbuf_class->peer_id);
        if (peer)
            PFLOW_TXRX_TIDQ_STATS_ADD(peer, nbuf_class->pkt_tid,
                    TX_MSDU_TOTAL_LINUX_SUBSYSTEM, 1);
    }

    if (vdev->pdev->delay_counters_enabled && nbuf_class->pkt_tid == QDF_TID_VI) {
        interframe_delay_stats(vdev, (struct sk_buff *)msdu, TX_DIR);
        qdf_nbuf_set_timestamp((struct sk_buff *)msdu);
    } else
        msdu->tstamp.tv64 = 0;

    return 0;

}

int ol_txrx_get_total_per(struct cdp_pdev *pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    if (!pdev)
        return 0;

    ol_txrx_aggregate_pdev_stats(pdev);
    if ((pdev->pdev_stats.tx.tx_success.num + pdev->pdev_stats.tx.retries) == 0)
        return 0;
    return ((pdev->pdev_stats.tx.retries * 100) /
            ((pdev->pdev_stats.tx.tx_success.num) + (pdev->pdev_stats.tx.retries)));
}

#if ENHANCED_STATS && defined(WLAN_FEATURE_FASTPATH)
uint32_t* ol_txrx_get_en_stats_base(struct cdp_pdev *txrx_pdev, uint32_t* stats_base, uint32_t msg_len, uint32_t *type,  uint32_t *status)
{
    htt_t2h_dbg_enh_stats_hdr_parse(stats_base, type, status);
    return (ol_txrx_get_stats_base(txrx_pdev, stats_base, msg_len, *type));
}
qdf_export_symbol(ol_txrx_get_en_stats_base);

uint32_t* ol_txrx_get_stats_base(struct cdp_pdev *txrx_pdev_handle, uint32_t* stats_base, uint32_t msg_len, uint8_t type)
{
    //ol_txrx_pdev_handle txrx_pdev = (ol_txrx_pdev_handle)txrx_pdev_handle; Enable if needed
    int len = msg_len;
    uint8_t stat_type, status;
    uint16_t stat_len;
    uint32_t *msg_word;
    int found = 0;
    msg_word = stats_base;

#define EN_ST_ROUND_UP_TO_4(val) (((val) + 3) & ~0x3)

    /*Convert it to DWORD */
    len = len>>2;

    /*skip first word. It is already checked by the caller*/
    msg_word = msg_word + 1;

    stat_type = HTT_T2H_EN_STATS_CONF_TLV_TYPE_GET(*msg_word);
    status = HTT_T2H_EN_STATS_CONF_TLV_STATUS_GET(*msg_word);
    stat_len = HTT_T2H_EN_STATS_CONF_TLV_LENGTH_GET(*msg_word);


    while( status != HTT_T2H_EN_STATS_STATUS_SERIES_DONE ) {

        if (type == stat_type) {
            found = 1;
            break;
        }

        len  = EN_ST_ROUND_UP_TO_4(stat_len);
        len = len >> 2;
        msg_word = (msg_word + 1 + len);

        stat_type = HTT_T2H_EN_STATS_CONF_TLV_TYPE_GET(*msg_word);
        status = HTT_T2H_EN_STATS_CONF_TLV_STATUS_GET(*msg_word);
        stat_len = HTT_T2H_EN_STATS_CONF_TLV_LENGTH_GET(*msg_word);
    }

    if (found) {
        return (msg_word+1);
    } else {
        return NULL;
    }
}
#endif

void ol_txrx_soc_detach(void *soc)
{
    struct ol_txrx_psoc_t *dp_soc = (struct ol_txrx_psoc_t *)soc;
    int i;

    if(dp_soc != NULL) {
        for (i = 0; i < MAX_PDEV_COUNT; i++) {
            if(dp_soc->pdev[i] != NULL)
            {
                ol_txrx_pdev_detach((struct cdp_pdev *)dp_soc->pdev[i], 1);
                dp_soc->pdev[i] = NULL;
            }
        }
        ar_detach(dp_soc->arh);
        ol_txrx_cfg_soc_detach(dp_soc->ol_txrx_cfg_ctx);
        qdf_mem_free(dp_soc);
        qdf_print("Soc detach Success");
    }
    else {
        qdf_print("soc is NULL, could not free the memory ");
    }
}

/*
 * ol_get_sec_type() - Get the security type
 * @peer:		Datapath peer handle
 * @sec_idx:    Security id (mcast, ucast)
 *
 * return sec_type: Security type
 */
static int ol_get_sec_type(struct cdp_peer *peer, uint8_t sec_idx)
{
    struct ol_txrx_peer_t *ol_peer = (struct ol_txrx_peer_t *)peer;
    return ol_peer->security[sec_idx].sec_type;
}

static void
ol_update_txpow_vdev(struct cdp_vdev *vdev_handle, uint8_t subtype,uint8_t transmit_power)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)vdev_handle;

    vdev->txpow_mgt_frm[(subtype >> IEEE80211_FC0_SUBTYPE_SHIFT)] = transmit_power;

    return;
}

static void ol_txrx_config_debug_sniffer(struct cdp_pdev *pdev_handle, uint8_t val)
{
        struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;

        switch (val) {
        case 0:
                pdev->tx_sniffer_enable = 0;
                pdev->mcopy_mode = 0;
                break;
        case 1:
                pdev->tx_sniffer_enable = 1;
                pdev->mcopy_mode = 0;
                break;
        case 2:
                pdev->mcopy_mode = 1;
                pdev->tx_sniffer_enable = 0;
                break;
        default:
                QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
                        "Invalid value\n");
                break;
        }
}

QDF_STATUS ol_txrx_set_pdev_param(struct cdp_pdev *pdev_handle,
    enum cdp_pdev_param_type param, uint8_t val)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;

    if (qdf_unlikely(!pdev)) {
        return QDF_STATUS_E_FAILURE;
    }

    switch(param){
        case CDP_CONFIG_DEBUG_SNIFFER:
            ol_txrx_config_debug_sniffer(pdev_handle, val);
            break;
        case CDP_CONFIG_ENABLE_PERPKT_TXSTATS:
            pdev->enable_perpkt_txstats = val;
            break;
        case CDP_CONFIG_IGMPMLD_OVERRIDE:
            pdev->igmpmld_override = val;
            break;
        case CDP_CONFIG_IGMPMLD_TID:
            pdev->igmpmld_tid = val;
            break;
        case CDP_CONFIG_ARP_DBG_CONF:
            pdev->arp_dbg_conf = val;
            break;
        default:
            break;
    }
    return QDF_STATUS_SUCCESS;
}

/**
 * ol_txrx_pdev_get_dp_txrx_handle() - get dp handle from pdev
 * @pdev_hdl: datapath pdev handle
 *
 * Return: opaque pointer to dp_txrx_handle
 */
static void *ol_txrx_pdev_get_dp_txrx_handle(struct cdp_pdev *pdev_hdl)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_hdl;

    if (pdev)
        return pdev->dp_txrx_handle;

    return NULL;
}

#if ATH_SUPPORT_NAC_RSSI
QDF_STATUS ol_config_for_nac_rssi(struct cdp_vdev *vdev_handle, enum cdp_nac_param_cmd cmd, char *bssid,
                                 char *client_macaddr, uint8_t chan_num)
{
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *) vdev_handle;

    if (vdev->pdev->htt_pdev->soc->ol_ops->config_fw_for_nac_rssi)
        vdev->pdev->htt_pdev->soc->ol_ops->config_fw_for_nac_rssi((struct wlan_objmgr_pdev *)vdev->pdev->ctrl_pdev,
                                                                 vdev->vdev_id, cmd, bssid, client_macaddr, chan_num);
    return QDF_STATUS_SUCCESS;
}
#endif

/**
 * ol_txrx_pdev_set_dp_txrx_handle() - set dp handle in pdev
 * @pdev_hdl: datapath pdev handle
 * @dp_txrx_hdl: opaque pointer for dp_txrx_handle
 *
 * Return: void
 */
static void
ol_txrx_pdev_set_dp_txrx_handle(struct cdp_pdev *pdev_hdl, void *dp_txrx_hdl)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_hdl;

    pdev->dp_txrx_handle = dp_txrx_hdl;
}

/**
 * ol_txrx_soc_get_dp_txrx_handle() - get external dp handle in soc
 * @soc_handle: datapath soc handle
 *
 * Return: external dp handle
 **/
static void *ol_txrx_soc_get_dp_txrx_handle(struct cdp_soc *soc_handle)
{
     struct ol_txrx_psoc_t *soc = (struct ol_txrx_psoc_t*)soc_handle;

     return soc->external_txrx_handle;
}

/*
 * ol_txrx_soc_set_dp_txrx_handle() - set external dp handle in soc
 * @soc_handle: datapath soc handle
 * @txrx_handle: opaque pointer to external dp (non-core DP)
 *
 * Return: void
 **/
static void ol_txrx_soc_set_dp_txrx_handle(struct cdp_soc *soc_handle, void *txrx_handle)
{
    struct ol_txrx_psoc_t *soc = (struct ol_txrx_psoc_t*)soc_handle;

    soc->external_txrx_handle = txrx_handle;
}

/**
 * ol_txrx_vdev_flush_peers() - Forcibily Flush peers of vdev
 * @vdev: Datapath VDEV handle
 * @unmap_only: Flag to indicate "only unmap"
 *
 * Return: void
 */
static void ol_txrx_vdev_flush_peers(struct cdp_vdev *vdev_handle, bool unmap_only)
{
	struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *) vdev_handle;
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	struct ol_txrx_peer_t *peer;
	uint16_t *peer_ids;
	uint8_t i = 0, j = 0;

	peer_ids = qdf_mem_malloc(pdev->max_peers * sizeof(peer_ids[0]));
	if (!peer_ids) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			"DP alloc failure - unable to flush peers");
		return;
	}

	qdf_spin_lock_bh(&pdev->peer_ref_mutex);
	TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
		for (i = 0; i < MAX_NUM_PEER_ID_PER_PEER; i++)
			if (peer->peer_ids[i] != HTT_INVALID_PEER)
				if (j < pdev->max_peers)
					peer_ids[j++] = peer->peer_ids[i];
	}
	qdf_spin_unlock_bh(&pdev->peer_ref_mutex);

	for (i = 0; i < j ; i++) {
		peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, peer_ids[i]);
		if (peer) {
			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
				  "peer: %pM is getting flush",
				  peer->mac_addr.raw);
		}
		ol_rx_peer_unmap_handler(pdev, peer_ids[i]);
	}

	qdf_mem_free(peer_ids);

	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_HIGH,
		FL("Flushed peers for vdev object %pK "), vdev);
}


/**
 * ol_txrx_soc_set_rate_stats_ctx () - Set rate stats context
 * @soc_handle: soc handle
 * @stats_ctx: context
 */
void ol_txrx_soc_set_rate_stats_ctx(struct cdp_soc_t *soc_handle,
                                    void *stats_ctx)
{
    struct ol_txrx_psoc_t *soc = (struct ol_txrx_psoc_t *)soc_handle;

    soc->rate_stats_ctx = stats_ctx;
}

void ol_txrx_flush_rate_stats_req(struct cdp_soc_t *soc_hdl,
                                  struct cdp_pdev *pdev_hdl)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_hdl;
    struct ol_txrx_vdev_t *vdev;
    struct ol_txrx_peer_t *peer;

    TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
       qdf_spin_lock_bh(&pdev->peer_ref_mutex);
       TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
               if (peer) {
                   wdi_event_handler(WDI_EVENT_FLUSH_RATE_STATS_REQ,
                                     (struct ol_txrx_pdev_t *)pdev, peer->wlanstats_ctx,
                                     HTT_INVALID_PEER, WDI_NO_VAL);
               }
        }
        qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
    }
}

void ol_txrx_peer_flush_rate_stats(struct cdp_soc_t *soc, struct cdp_pdev *pdev,
                                   void *buf)
{
    wdi_event_handler(WDI_EVENT_PEER_FLUSH_RATE_STATS,
                      (struct ol_txrx_pdev_t *)pdev, buf,
                      HTT_INVALID_PEER, WDI_NO_VAL);
}
/**
 * ol_txrx_soc_get_rate_stats_ctx () - Get rate stats context
 * @soc_handle: soc handle
 */
void* ol_txrx_soc_get_rate_stats_ctx(struct cdp_soc_t *soc_handle)
{
    struct ol_txrx_psoc_t *soc = (struct ol_txrx_psoc_t *)soc_handle;

    return soc->rate_stats_ctx;
}

/**
 * ol_txrx_get_cfg_tso_enabled() - get dp capabilities
 * @soc_handle: datapath soc handle
 * @dp_caps: enum of dp capabilities
 *
 * Return: bool to determine if dp caps is enabled
 */
static bool
ol_txrx_get_cfg_capabilities(struct cdp_soc_t *soc_handle,
				enum cdp_capabilities dp_caps)
{
    struct ol_txrx_psoc_t *soc = (struct ol_txrx_psoc_t *)soc_handle;

    return ol_txrx_cfg_get_dp_caps(soc->ol_txrx_cfg_ctx, dp_caps);
}

static QDF_STATUS
ol_txrx_set_pdev_pcp_tid_map(struct cdp_pdev *pdev,
                             uint8_t pcp, uint8_t tid)
{
    struct ol_txrx_pdev_t *pdev_hndl = (struct ol_txrx_pdev_t *)pdev;
    pdev_hndl->pcp_tid_map[pcp] = tid;

    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
ol_txrx_set_pdev_tidmap_prty(struct cdp_pdev *pdev, uint8_t val)
{
    struct ol_txrx_pdev_t *pdev_hndl = (struct ol_txrx_pdev_t *)pdev;
    pdev_hndl->tidmap_prty = val;

    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
ol_txrx_set_vdev_pcp_tid_map(struct cdp_vdev *vdev,
                             uint8_t pcp, uint8_t tid)
{
    struct ol_txrx_vdev_t *vdev_hndl = (struct ol_txrx_vdev_t *)vdev;
    uint8_t tblid = vdev_hndl->tidmap_tbl_id;
    vdev_hndl->pcp_tid_map[pcp] = tid;

    return ol_ath_set_pcp_tid_map(vdev_hndl->osif_vdev, tblid);
}

static QDF_STATUS
ol_txrx_set_vdev_tidmap_tbl_id(struct cdp_vdev *vdev, uint8_t mapid)
{
    struct ol_txrx_vdev_t *vdev_hndl = (struct ol_txrx_vdev_t *)vdev;
    vdev_hndl->tidmap_tbl_id = mapid;

    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
ol_txrx_set_vdev_tidmap_prty(struct cdp_vdev *vdev, uint8_t prio)
{
    struct ol_txrx_vdev_t *vdev_hndl = (struct ol_txrx_vdev_t *)vdev;

    vdev_hndl->tidmap_prty = prio;

    return ol_ath_set_tidmap_prty(vdev_hndl->osif_vdev, prio);
}

static struct cdp_cmn_ops dp_ops_cmn = {
    .txrx_soc_attach_target = NULL,
    .txrx_pdev_attach_target = ol_txrx_pdev_attach_target,
    .txrx_vdev_attach = ol_txrx_vdev_attach,
    .txrx_vdev_detach = ol_txrx_vdev_detach,
    .txrx_pdev_attach = ol_txrx_pdev_attach,
    .txrx_pdev_post_attach = NULL,
    .txrx_pdev_pre_detach = NULL,
    .txrx_pdev_detach = NULL,
    .txrx_pdev_deinit = ol_txrx_pdev_detach,
    .txrx_peer_create = ol_txrx_peer_attach,
    .txrx_peer_setup =  NULL,
    .txrx_peer_teardown = ol_txrx_peer_teardown,
    .txrx_peer_add_ast = ol_txrx_peer_add_ast,
    .txrx_peer_update_ast = ol_txrx_peer_update_ast,
    .txrx_peer_delete = ol_txrx_peer_detach,
    .txrx_vdev_flush_peers = ol_txrx_vdev_flush_peers,
    .txrx_set_monitor_mode = ol_txrx_set_monitor_mode,
    .txrx_get_pdev_id_frm_pdev = ol_txrx_get_pdev_id_frm_pdev,
    .txrx_get_vow_config_frm_pdev = ol_txrx_get_vow_config_frm_pdev,
    .txrx_pdev_set_chan_noise_floor = NULL,
    .txrx_set_nac = ol_txrx_set_nac,
    .txrx_set_pdev_tx_capture = ol_txrx_set_pdev_tx_capture,
    .txrx_get_peer_mac_from_peer_id = ol_txrx_get_peer_mac_from_peer_id,
    .txrx_vdev_tx_lock =  ol_txrx_vdev_tx_lock,
    .txrx_vdev_tx_unlock =  ol_txrx_vdev_tx_unlock,
    .txrx_ath_getstats =  ol_txrx_ath_getstats,
    .txrx_set_gid_flag = ol_txrx_set_gid_flag,
    .txrx_fw_supported_enh_stats_version = ol_txrx_fw_supported_enh_stats_version,
    .txrx_if_mgmt_drain =             ol_txrx_if_mgmt_drain,
    .txrx_set_curchan        =    ol_txrx_set_curchan,
    .txrx_set_privacy_filters =   ol_txrx_set_privacy_filters,
    .txrx_vdev_register       =   ol_txrx_vdev_register,
    .txrx_mgmt_send           =   ol_txrx_mgmt_send,
    .txrx_mgmt_send_ext       =   ol_txrx_mgmt_send_ext,
    .txrx_mgmt_tx_cb_set      =   ol_txrx_mgmt_tx_cb_set,
    .txrx_get_tx_pending      =   ol_txrx_get_tx_pending,
    .txrx_data_tx_cb_set      =   NULL,
    .txrx_aggr_cfg            =   ol_txrx_aggr_cfg,
    .txrx_fw_stats_get        =   ol_txrx_fw_stats_get,
    .txrx_debug               =   ol_txrx_debug,
    .txrx_fw_stats_cfg        =   ol_txrx_fw_stats_cfg,
    .txrx_print_level_set     =   ol_txrx_print_level_set,
    .txrx_get_vdev_mac_addr   =   NULL,
    .txrx_get_vdev_struct_mac_addr =   NULL,
    .txrx_get_pdev_from_vdev       =   NULL,
    .txrx_get_ctrl_pdev_from_vdev  =   NULL,
    .txrx_get_vdev_from_vdev_id  =   NULL,
    .txrx_soc_detach = ol_txrx_soc_detach,
    .get_dp_txrx_handle = ol_txrx_pdev_get_dp_txrx_handle,
    .set_dp_txrx_handle = ol_txrx_pdev_set_dp_txrx_handle,
    .get_soc_dp_txrx_handle = ol_txrx_soc_get_dp_txrx_handle,
    .set_soc_dp_txrx_handle = ol_txrx_soc_set_dp_txrx_handle,
    .txrx_peer_reset_ast = ol_txrx_wds_reset_ast,
    .txrx_peer_reset_ast_table = ol_txrx_wds_reset_ast_table,
    .txrx_classify_update = ol_txrx_classify_update,
    .get_dp_capabilities = ol_txrx_get_cfg_capabilities,
    .txrx_peer_get_ast_info_by_soc = NULL,
    .txrx_peer_get_ast_info_by_pdev = ol_txrx_peer_get_ast_info_by_pdevid,
    .txrx_get_total_per = NULL,
    .set_pdev_pcp_tid_map = ol_txrx_set_pdev_pcp_tid_map,
    .set_pdev_tidmap_prty = ol_txrx_set_pdev_tidmap_prty,
    .set_vdev_pcp_tid_map = ol_txrx_set_vdev_pcp_tid_map,
    .set_vdev_tidmap_tbl_id = ol_txrx_set_vdev_tidmap_tbl_id,
    .set_vdev_tidmap_prty = ol_txrx_set_vdev_tidmap_prty,
    .set_rate_stats_ctx = ol_txrx_soc_set_rate_stats_ctx,
    .get_rate_stats_ctx = ol_txrx_soc_get_rate_stats_ctx,
    .txrx_peer_flush_rate_stats = ol_txrx_peer_flush_rate_stats,
    .txrx_flush_rate_stats_request = ol_txrx_flush_rate_stats_req,
};

static struct cdp_ctrl_ops dp_ops_ctrl = {
    ol_txrx_mempools_attach,                    /*txrx_mempools_attach */
    ol_txrx_set_filter_neighbour_peers,         /*txrx_set_filter_neighbour_peers*/
    NULL,					/*txrx_set_filter_neighbour_peer_addr*/
    ol_txrx_set_safemode,                       /*txrx_set_safemode*/
    ol_txrx_set_drop_unenc,                     /*txrx_set_drop_unenc*/
    ol_txrx_set_tx_encap_type,                  /*txrx_set_tx_encap_type*/
    ol_txrx_set_vdev_rx_decap_type,             /*txrx_set_vdev_rx_decap_type*/
    ol_txrx_get_vdev_rx_decap_type,             /*txrx_get_vdev_rx_decap_type*/
    ol_txrx_peer_authorize,                     /*txrx_peer_authorize*/
#if MESH_MODE_SUPPORT
    ol_txrx_set_mesh_mode,                      /*txrx_set_mesh_mode*/
#else
    NULL,
#endif
    NULL,                                       /*txrx_set_mesh_rx_filter*/
    ol_tx_flush_buffers,                        /*tx_flush_buffers*/
    ol_txrx_is_target_ar900b,                   /* txrx_is_target_ar900b*/
    NULL,                                       /* set vdev params */
    ol_txrx_set_peer_nawds,                     /* set nawds enable fag in vdev */
    NULL,                                       /*txrx_set_pdev_reo_dest*/
    NULL,                                       /*txrx_get_pdev_reo_dest*/
    wdi_event_sub,                              /*txrx_wdi_event_sub*/
    wdi_event_unsub,                            /*txrx_wdi_event_unsub*/
    ol_get_sec_type,                            /*txrx_get_sec_type*/
    ol_update_txpow_vdev,                       /*txrx_update_mgmt_txpow_vdev*/
    ol_txrx_set_pdev_param,
    NULL,
#if ATH_SUPPORT_NAC_RSSI
    .txrx_vdev_config_for_nac_rssi = ol_config_for_nac_rssi,
    .txrx_vdev_get_neighbour_rssi = NULL,
#endif
#ifdef WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
    .txrx_update_pdev_rx_protocol_tag = NULL,
#ifdef WLAN_SUPPORT_RX_TAG_STATISTICS
    .txrx_dump_pdev_rx_protocol_tag_stats = NULL,
#endif //WLAN_SUPPORT_RX_TAG_STATISTICS
#endif //WLAN_SUPPORT_RX_PROTOCOL_TYPE_TAG
};

static struct cdp_me_ops dp_ops_me = {
    NULL,
    NULL,
    NULL,
    ol_tx_me_alloc_descriptor,                  /*tx_me_alloc_descriptor*/
    ol_tx_me_free_descriptor,                   /*tx_me_free_descriptor*/
    ol_tx_me_convert_ucast,                     /*tx_me_convert_ucast*/
#if ATH_MCAST_HOST_INSPECT
    ol_mcast_notify,                            /*mcast_notify*/
#else
    NULL,
#endif
};

static struct cdp_mon_ops dp_ops_mon = {
    ol_txrx_monitor_set_filter_ucast_data,      /*txrx_monitor_set_filter_ucast_data*/
    ol_txrx_monitor_set_filter_mcast_data,      /*txrx_monitor_set_filter_mcast_data*/
    ol_txrx_monitor_set_filter_non_data,        /*txrx_monitor_set_filter_non_data*/
    ol_txrx_monitor_get_filter_ucast_data,      /*txrx_monitor_get_filter_ucast_data*/
    ol_txrx_monitor_get_filter_mcast_data,      /*txrx_monitor_get_filter_mcast_data*/
    ol_txrx_monitor_get_filter_non_data,        /*txrx_monitor_get_filter_non_data*/
    ol_txrx_reset_monitor_mode,                 /*txrx_reset_monitor_mode*/
    NULL,                                       /*txrx_set_advance_monitor_filter*/
};

static struct cdp_host_stats_ops dp_ops_host_stats = {
#ifdef WLAN_FEATURE_FASTPATH
    ol_txrx_host_stats_get,                     /*txrx_host_stats_get*/
    ol_txrx_host_stats_clr,                     /*txrx_host_stats_clr*/
    ol_txrx_host_ce_stats,                      /*txrx_host_ce_stats*/
    ol_txrx_stats_publish,                      /*txrx_stats_publish*/
    ol_txrx_enable_enhanced_stats,              /*txrx_enable_enhanced_stats*/
    ol_txrx_disable_enhanced_stats,             /*txrx_disable_enhanced_stats*/
#if ENHANCED_STATS
    ol_txrx_get_stats_base,                      /*txrx_get_stats_base*/
#else
    NULL,
#endif
#else
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
#endif /* WLAN_FEATURE_FASTPATH*/
#if (HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE)
    ol_tx_print_tso_stats,                      /*tx_print_tso_stats*/
    ol_tx_rst_tso_stats,                        /*tx_rst_tso_stats*/
#else
    NULL,
    NULL,
#endif /* HOST_SW_TSO_ENABLE || HOST_SW_TSO_SG_ENABLE */
#if HOST_SW_SG_ENABLE
    ol_tx_print_sg_stats,                      /* txrx_vdev_handle vdev*/
    ol_tx_rst_sg_stats,                        /* txrx_vdev_handle vdev*/
#else
    NULL,
    NULL,
#endif /* HOST_SW_SG_ENABLE */

#if RX_CHECKSUM_OFFLOAD
    ol_print_rx_cksum_stats,                   /* print_rx_cksum_stats*/
    ol_rst_rx_cksum_stats,                     /* rst_rx_cksum_stats*/
#else
    NULL,
    NULL,
#endif /* RX_CHEKSUM_OFFLOAD */

#if ATH_SUPPORT_IQUE && defined(WLAN_FEATURE_FASTPATH)
    ol_txrx_host_me_stats,                     /* txrx_host_me_stats*/
#else
    NULL,
#endif /* WLAN_FEATURE_FASTPATH */
#if PEER_FLOW_CONTROL
    ol_txrx_per_peer_stats,                    /* txrx_per_peer_stats*/
#else
    NULL,
#endif
#if defined(WLAN_FEATURE_FASTPATH) && PEER_FLOW_CONTROL
    ol_txrx_host_msdu_ttl_stats,               /* txrx_host_msdu_ttl_stats*/
#else
    NULL,
#endif
    NULL,                                      /* print_lro_stats*/
    NULL,                                      /* reset_lro_stats*/
    NULL,                                      /* get_fw_peer_stats */
    NULL,                                      /* get_htt_stats */
    ol_txrx_update_pdev_host_stats,            /* txrx_update_pdev_stats */
    ol_txrx_get_peer_stats,                    /* txrx_get_peer_stats */
    NULL,
    ol_txrx_reset_peer_stats,                  /* txrx_reset_peer_stats */
    ol_txrx_get_vdev_stats,                    /* txrx_get_vdev_stats */
    ol_txrx_process_wmi_host_vdev_stats,       /* process wmi host vdev stats */
    ol_txrx_get_vdev_extd_stats,               /* get vdev extd stats */
    NULL,                                      /* update vdev stats */
    ol_txrx_get_radiostats,                    /* get radiostats */
    ol_txrx_get_pdev_stats,                    /* get cdp_pdev_stats */
    ol_txrx_get_ratekbps,                      /* get ratekbps */
    ol_txrx_set_rate_stats_cap                 /* set rate stats capabilities */
};
static struct cdp_wds_ops dp_ops_wds = {
#if WDS_VENDOR_EXTENSION
    ol_txrx_set_wds_rx_policy,            /* txrx_set_wds_rx_policy*/
    ol_txrx_peer_wds_tx_policy_update,    /* txrx per peer wds tx policy */
#else
    NULL,
#endif
};

static struct cdp_raw_ops dp_ops_raw = {
    ol_txrx_get_nwifi_mode,                    /* txrx_get_nwifi_mode*/
#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
    ol_tx_rawsim_getastentry,                  /* rsim_getastentry */
#else
    NULL,
#endif
};

static struct cdp_pflow_ops dp_ops_pflow = {
#if PEER_FLOW_CONTROL
    ol_pflow_update_pdev_params,               /* pflow_update_pdev_params*/
#else
    NULL,
#endif
};

static struct cdp_ops dp_txrx_ops = {
    .cmn_drv_ops = &dp_ops_cmn,
    .ctrl_ops = &dp_ops_ctrl,
    .me_ops = &dp_ops_me,
    .mon_ops = &dp_ops_mon,
    .host_stats_ops = &dp_ops_host_stats,
    .wds_ops = &dp_ops_wds,
    .raw_ops = &dp_ops_raw,
    .pflow_ops = &dp_ops_pflow,
};

ol_txrx_soc_handle ol_txrx_soc_attach(void *soc_handle, struct ol_if_ops *dp_ol_if_ops)
{
    struct ol_txrx_psoc_t *dp_soc = qdf_mem_malloc(sizeof(*dp_soc));
    struct common_htc_handle *htc_handle;
    qdf_device_t qdf_dev;
    if (!dp_soc) {
        QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
                "%s: DP SOC memory allocation failed\n", __func__);
        return NULL;
    }
    dp_soc->cdp_soc.ops = &dp_txrx_ops;
    dp_soc->cdp_soc.ol_ops = dp_ol_if_ops;
    dp_soc->psoc_obj = soc_handle;

    dp_soc->arh = ar_attach(
                      lmac_get_tgt_type((struct wlan_objmgr_psoc *)soc_handle));
    if (!(dp_soc->arh)) {
        qdf_mem_free(dp_soc);
        QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
                "%s: DP SOC ar_attach failed\n", __func__);
        return NULL;
    }

    dp_soc->ol_txrx_cfg_ctx = ol_txrx_cfg_soc_attach(dp_soc->psoc_obj);
    qdf_dev = wlan_psoc_get_qdf_dev((struct wlan_objmgr_psoc *)soc_handle);
    htc_handle = lmac_get_htc_hdl((struct wlan_objmgr_psoc *)soc_handle);
    ol_soc_pdev_attach(dp_soc,
                       (void *)soc_handle,
                       htc_handle,
		               qdf_dev);
    return (ol_txrx_soc_handle)dp_soc;
}
