/*
 * Copyright (c) 2011, 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*=== includes ===*/
/* header files for OS primitives */
#include <osdep.h>         /* u_int32_t, etc. */
#include <qdf_mem.h>    /* qdf_mem_malloc,free */

#include "wlan_defs.h"
/* header files for our internal definitions */
/* TODO: Replace ol_txrx_internal.h with an equivalent for rc */
#include <ol_txrx_internal.h>  /* TXRX_ASSERT, etc. */
#include "ol_txrx_peer_find.h"
#include "ol_if_athvar.h"

#include "ratectrl_11ac.h"
#include "ratectrl_11ac_internal.h"
#include "ratectrl_11ac_api.h"
#include "ratectrl_11ac_common.h"

#include "ieee80211_var.h"

#include "wmi_unified_api.h"
#include "htt.h"

/*=== local definitions ===*/

/*=== local functions ===*/
static int ratectrl_map_phymode(int phymode)
{

    static const u_int modeflags[] = {
        0,                /* IEEE80211_MODE_AUTO           */
        MODE_11A,         /* IEEE80211_MODE_11A            */
        MODE_11B,         /* IEEE80211_MODE_11B            */
        MODE_11G,         /* IEEE80211_MODE_11G            */
        0,                /* IEEE80211_MODE_FH             */
        0,                /* IEEE80211_MODE_TURBO_A        */
        0,                /* IEEE80211_MODE_TURBO_G        */
        MODE_11NA_HT20,   /* IEEE80211_MODE_11NA_HT20      */
        MODE_11NG_HT20,   /* IEEE80211_MODE_11NG_HT20      */
        MODE_11NA_HT40,   /* IEEE80211_MODE_11NA_HT40PLUS  */
        MODE_11NA_HT40,   /* IEEE80211_MODE_11NA_HT40MINUS */
        MODE_11NG_HT40,   /* IEEE80211_MODE_11NG_HT40PLUS  */
        MODE_11NG_HT40,   /* IEEE80211_MODE_11NG_HT40MINUS */
        MODE_11NG_HT40,   /* IEEE80211_MODE_11NG_HT40      */
        MODE_11NA_HT40,   /* IEEE80211_MODE_11NA_HT40      */
        MODE_11AC_VHT20,  /* IEEE80211_MODE_11AC_VHT20     */
        MODE_11AC_VHT40,  /* IEEE80211_MODE_11AC_VHT40PLUS */
        MODE_11AC_VHT40,  /* IEEE80211_MODE_11AC_VHT40MINUS*/
        MODE_11AC_VHT40,  /* IEEE80211_MODE_11AC_VHT40     */
        MODE_11AC_VHT80,  /* IEEE80211_MODE_11AC_VHT80     */
    };

    return(modeflags[phymode]);
}

static int ratectrl_parse_ni_flags(
        struct peer_ratectrl_params_t *peer_ratectrl_params)
{
    u_int32_t txrx_peer_flags = 0;

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Enter: %s \n", __func__);
#endif

    if (peer_ratectrl_params->ni_flags & IEEE80211_NODE_QOS) {
        txrx_peer_flags |= RC_PEER_QOS;
    }
    if (peer_ratectrl_params->ni_flags & IEEE80211_NODE_UAPSD ) {
        txrx_peer_flags |= RC_PEER_APSD;
    }
    if (peer_ratectrl_params->ni_flags & IEEE80211_NODE_HT) {
        txrx_peer_flags |= RC_PEER_HT;
    }
    if ((peer_ratectrl_params->ni_chwidth == IEEE80211_CWM_WIDTH40) ||
            (peer_ratectrl_params->ni_chwidth == IEEE80211_CWM_WIDTH80)){
        txrx_peer_flags |= RC_PEER_40MHZ;
#ifdef DEBUG_HOST_RC
        printk("%s : 40Mhz\n", __func__);
#endif
    }

    if (peer_ratectrl_params->ni_chwidth == IEEE80211_CWM_WIDTH80) {
        txrx_peer_flags |= RC_PEER_80MHZ;
#ifdef DEBUG_HOST_RC
        printk("%s : 80Mhz\n", __func__);
#endif
    }

    if (peer_ratectrl_params->ni_htcap & IEEE80211_HTCAP_C_RXSTBC) {
        txrx_peer_flags |= RC_PEER_STBC;
    }
    if (peer_ratectrl_params->ni_htcap & IEEE80211_HTCAP_C_ADVCODING) {
        txrx_peer_flags |= RC_PEER_LDPC;
    }
    if (peer_ratectrl_params->ni_htcap & IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC) {
        txrx_peer_flags |= RC_PEER_STATIC_MIMOPS;
    }
    if (peer_ratectrl_params->ni_htcap & IEEE80211_HTCAP_C_SMPOWERSAVE_DYNAMIC) {
        txrx_peer_flags |= RC_PEER_DYN_MIMOPS;
    }
    if (peer_ratectrl_params->ni_htcap & IEEE80211_HTCAP_C_SMPOWERSAVE_DISABLED) {
        txrx_peer_flags |= RC_PEER_SPATIAL_MUX;
    }
    if (peer_ratectrl_params->ni_flags & IEEE80211_NODE_VHT) {
        txrx_peer_flags |= RC_PEER_VHT;
    }
    if (peer_ratectrl_params->ni_ext_flags & IEEE80211_NODE_HE) {
        txrx_peer_flags |= RC_PEER_HE;
    }
    if (peer_ratectrl_params->ni_flags & IEEE80211_NODE_AUTH) {
        txrx_peer_flags |= RC_PEER_AUTH;
    }
    if (peer_ratectrl_params->is_auth_wpa ||
            peer_ratectrl_params->is_auth_wpa2 ||
            peer_ratectrl_params->is_auth_8021x) {
        txrx_peer_flags |= RC_PEER_NEED_PTK_4_WAY;
    }

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Exit: %s\n", __func__);
#endif

    return txrx_peer_flags;
}

/*=== function definitions ===*/
void* ol_ratectrl_vdev_ctxt_attach(
        struct ol_txrx_pdev_t *pdev,
        struct ol_txrx_vdev_t *vdev)
{
    RATE_CONTEXT *g_pRATE = NULL;

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Enter %s\n", __func__);
#endif

    /* preconditions */
    TXRX_ASSERT2(pdev);
    TXRX_ASSERT2(vdev);

    g_pRATE = qdf_mem_malloc(sizeof(*g_pRATE));
    if (!g_pRATE) {
        return NULL; /* failure */
    }

    qdf_mem_set(g_pRATE,sizeof(*g_pRATE), 0);

    rate_init_vdev_ratectxt(g_pRATE, vdev);

    /*WAR TODO*/
    {
        vdev->vdev_rc_info.data_rc = 0x3;
        vdev->vdev_rc_info.max_bw = 0;
        vdev->vdev_rc_info.max_nss = 3;
        vdev->vdev_rc_info.rts_cts = 0;
        vdev->vdev_rc_info.def_tpc = 0;
        vdev->vdev_rc_info.def_tries = 2;
        vdev->vdev_rc_info.non_data_rc = 0x3;
        vdev->vdev_rc_info.rc_flags = 0;
        vdev->vdev_rc_info.aggr_dur_limit = 4000;
        vdev->vdev_rc_info.tx_chain_mask[0] = 0x1;
        vdev->vdev_rc_info.tx_chain_mask[1] = 0x3;
        vdev->vdev_rc_info.tx_chain_mask[2] = 0x7;
    }

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Exit %s | g_pRATE: 0x%x\n",
            __func__, g_pRATE);
#endif

    return ((void *)g_pRATE);
}

void rate_init_vdev_params(RATE_CONTEXT *g_pRATE)
{
    struct ol_txrx_vdev_t *vdev = NULL;

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Enter: %s | g_pRATE: 0x%x\n",
            __func__, g_pRATE);
#endif

    if(g_pRATE != NULL) {
        vdev = (struct ol_txrx_vdev_t *)g_pRATE->dev_context;

        switch(vdev->opmode) {
        case wlan_op_mode_ap:
            g_pRATE->vdev_params.ic_opmode = RC_VDEV_M_AP;
            break;
        case wlan_op_mode_ibss:
            g_pRATE->vdev_params.ic_opmode = RC_VDEV_M_IBSS;
            break;
        case wlan_op_mode_sta:
            g_pRATE->vdev_params.ic_opmode = RC_VDEV_M_STA;
            break;
        case wlan_op_mode_monitor:
            g_pRATE->vdev_params.ic_opmode = RC_VDEV_M_MONITOR;
            break;
        default:
            A_ASSERT(0); /* assert here? */
            break;
        }
    }

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Exit: %s | ic_opmode: 0x%x\n",
            __func__, g_pRATE->vdev_params.ic_opmode);
#endif

    return;
}

void ol_ratectrl_vdev_ctxt_detach(void *vdev_ratectxt)
{
#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Enter: %s | g_pRATE: 0x%pK\n",
            __func__, g_pRATE);
#endif

    RATE_CONTEXT *g_pRATE = (RATE_CONTEXT *)vdev_ratectxt;

    if (g_pRATE != NULL) {
        qdf_mem_free((void *)g_pRATE);
        g_pRATE = NULL;
    }

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Exit: %s | g_pRATE: 0x%x\n", __func__, g_pRATE);
#endif

    return;
}

void* ol_ratectrl_peer_ctxt_attach(
        struct ol_txrx_pdev_t *pdev,
        struct ol_txrx_vdev_t *vdev,
        struct ol_txrx_peer_t *peer)
{
    struct rate_node *rc_node = NULL;

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Enter %s\n", __func__);
#endif

    /* preconditions */
    TXRX_ASSERT2(pdev);
    TXRX_ASSERT2(vdev);
    TXRX_ASSERT2(peer);

    rc_node = qdf_mem_malloc(sizeof(*rc_node));
    if(!rc_node) {
        return NULL; /* failure */
    }

    qdf_mem_set(rc_node, sizeof(*rc_node), 0);

    /* allocate per peer Rate control state */
    rc_node->txRateCtrl = qdf_mem_malloc(sizeof(*(rc_node->txRateCtrl)));
    if(!rc_node->txRateCtrl) {
        qdf_mem_free((void *)rc_node);
        return NULL; /* failure */
    }

    rate_init_peer_ratectxt(rc_node, peer);

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Exit %s | rc_node: 0x%x\n",
            __func__, rc_node);
#endif

    return ((void *)rc_node);
}


void rate_init_peer_params(struct rate_node *rc_node,
        struct peer_ratectrl_params_t *peer_ratectrl_params)
{
    u_int8_t peer_nss = (peer_ratectrl_params->ni_streams==0)?1:(peer_ratectrl_params->ni_streams);

#ifdef DEBUG_HOST_RC
    printk("%s : peer_nss: %d ni_streams: %d\n", __func__, peer_nss,
            peer_ratectrl_params->ni_streams);
#endif

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Enter: %s | rc_node: 0x%x | peer: 0x%x | ni: 0x%x\n",
            __func__, rc_node, peer, ni);
#endif

    rc_node->peer_params.ni_flags = ratectrl_parse_ni_flags(peer_ratectrl_params);

    rc_node->peer_params.valid_tx_chainmask = (1 << peer_nss) - 1;

    rc_node->peer_params.phymode =
            ratectrl_map_phymode(peer_ratectrl_params->ni_phymode);
//
    {
         if (rc_node->peer_params.ni_flags & RC_PEER_VHT) {

            if (rc_node->peer_params.ni_flags & RC_PEER_80MHZ)
                rc_node->peer_params.phymode = MODE_11AC_VHT80;
            else if (rc_node->peer_params.ni_flags & RC_PEER_40MHZ)
                rc_node->peer_params.phymode = MODE_11AC_VHT40;
            else
                rc_node->peer_params.phymode = MODE_11AC_VHT20;
        }

    }

#if 1
    {
        u_int8_t j=0;
        for (; j < (MAX_SPATIAL_STREAM * 8); j++) {
            rc_node->peer_params.ni_ht_mcs_set[(peer_ratectrl_params->ht_rates[j])/8] |=
                    (1 << ((peer_ratectrl_params->ht_rates[j])%8));
        }
    }
#endif

    rc_node->peer_params.ni_vht_mcs_set = peer_ratectrl_params->ni_rx_vhtrates;

    rc_node->peer_params.ht_caps = peer_ratectrl_params->ni_htcap;
    rc_node->peer_params.vht_caps = peer_ratectrl_params->ni_vhtcap;

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Exit: %s \n",
            __func__);
#endif

    return;
}

void ol_ratectrl_peer_ctxt_update(struct ol_txrx_peer_t *peer,
        struct peer_ratectrl_params_t *peer_ratectrl_params)
{
    struct ol_txrx_vdev_t *vdev = peer->vdev;

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Enter: %s | vdev: 0x%x | peer: 0x%x | pRateCtrl: 0x%x | rc_node: 0x%x\n",
            __func__, vdev, peer, vdev->pRateCtrl, peer->rc_node);
#endif

    /* The condition is to check whether rate-control runs on target. */
    if( (vdev->pRateCtrl != NULL) && (peer->rc_node != NULL) ) {
        rate_init_peer_params(peer->rc_node, peer_ratectrl_params);
        rcSibUpdate(vdev->pRateCtrl, peer->rc_node, peer);
    }

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Exit: %s\n", __func__);
#endif

    return;
}

void ol_ratectrl_peer_ctxt_detach(void *peer_ratectxt)
{
#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Enter: %s | rc_node: 0x%pK\n",
            __func__, rc_node);
#endif

    struct rate_node *rc_node = (struct rate_node *)peer_ratectxt;

    if (rc_node != NULL) {
        if(!rc_node->txRateCtrl) {
            qdf_mem_free((void *)rc_node->txRateCtrl);
        }
        qdf_mem_free((void *)rc_node);
        rc_node = NULL;
    }

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Exit: %s\n", __func__);
#endif

    return;
}

void ol_ratectrl_enable_host_ratectrl(ol_txrx_pdev_handle pdev,
        u_int32_t enable)
{
#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Enter: %s\n", __func__);
#endif

    pdev->ratectrl.is_ratectrl_on_host = enable;

    /* TODO: This should come from the umac (or above?). */
    pdev->ratectrl.dyn_bw = 1;

    /* Enable the rate-table */
    ar6000ChipSpecificPhyAttach();

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Exit: %s\n", __func__);
#endif

    return;
}

static int
wmi_node_update_rate_sched(
        void *scn,
        const u_int8_t *peer_addr,
        u_int32_t vdev_id,
        RC_TX_RATE_SCHEDULE *rate_sched)
{
    static int __cnt=0;
    wmi_peer_rate_retry_sched_cmd cmd;

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Enter %s\n", __func__);
#endif

    WMI_CHAR_ARRAY_TO_MAC_ADDR(peer_addr, &cmd.peer_macaddr);
    cmd.vdev_id = vdev_id;

    {
        int i=0;
        for(; i<(NUM_DYN_BW * NUM_SCHED_ENTRIES); i++) {
            cmd.psdu_len[i] = rate_sched->psdu_len[i];
            cmd.flags[i] = rate_sched->flags[i];
            cmd.rix[i] = rate_sched->rix[i];
            cmd.tpc[i] = rate_sched->tpc[i];
            cmd.num_mpdus[i] = rate_sched->num_mpdus[i];
        }

        for(i=0; i<NUM_SCHED_ENTRIES; i++) {
            cmd.antmask[i] = rate_sched->antmask[i];
            cmd.tries[i] = rate_sched->tries[i];
        }
    }

    /* TODO: remove WHAL-related members. */
    cmd.txbf_cv_ptr = rate_sched->txbf_cv_ptr;
    cmd.txbf_cv_len = rate_sched->txbf_cv_len;
    cmd.num_valid_rates = rate_sched->num_valid_rates;
    cmd.paprd_mask = rate_sched->paprd_mask;
    cmd.rts_rix = rate_sched->rts_rix;
    cmd.sh_pream = rate_sched->sh_pream;
    cmd.min_spacing_1_4_us = rate_sched->min_spacing_1_4_us;
    cmd.fixed_delims = rate_sched->fixed_delims;
    cmd.bw_in_service = rate_sched->bw_in_service;
    cmd.probe_rix = rate_sched->probe_rix;

    if(cmd.probe_rix != 0) {
#ifdef DEBUG_HOST_RC
        TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "probe_rix %d\n", cmd.probe_rix);
#endif
    }
    __cnt++;

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "probe5 H2T MSG Cnt %d\n", __cnt);

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Exit %s\n", __func__);
#endif
    return (wmi_send_node_rate_sched(scn, &cmd));
}

void
ratectrl_update_rate_sched(struct rate_node *an, RC_TX_RATE_SCHEDULE *rate_sched, void *pkt_info_rcf)
{
#if 1
    struct ol_txrx_peer_t *peer = (struct ol_txrx_peer_t *)an->peer;
    struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)peer->vdev;
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)vdev->pdev;

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Enter %s\n", __func__);

    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "psdu_len: %d %d %d %d %d %d\n",
            rate_sched->psdu_len[0],rate_sched->psdu_len[1],rate_sched->psdu_len[2],
            rate_sched->psdu_len[3],rate_sched->psdu_len[4],rate_sched->psdu_len[5]);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "flags: %d %d %d %d %d %d\n",
            rate_sched->flags[0],rate_sched->flags[1],rate_sched->flags[2],
            rate_sched->flags[3],rate_sched->flags[4],rate_sched->flags[5]);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "rix: %d %d %d %d %d %d\n",
            rate_sched->rix[0],rate_sched->rix[1],rate_sched->rix[2],
            rate_sched->rix[3],rate_sched->rix[4],rate_sched->rix[5]);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "tpc: %d %d %d %d %d %d\n",
            rate_sched->tpc[0],rate_sched->tpc[1],rate_sched->tpc[2],
            rate_sched->tpc[3],rate_sched->tpc[4],rate_sched->tpc[5]);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "num_mpdus: %d %d %d %d %d %d\n",
            rate_sched->num_mpdus[0],rate_sched->num_mpdus[1],rate_sched->num_mpdus[2],
            rate_sched->num_mpdus[3],rate_sched->num_mpdus[4],rate_sched->num_mpdus[5]);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "antmask: %d %d \n",
            rate_sched->antmask[0],rate_sched->antmask[1]);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "tries: %d %d \n",
            rate_sched->tries[0],rate_sched->tries[1]);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "txbf_cv_ptr: %d | txbf_cv_len: %d \n",
            rate_sched->txbf_cv_ptr,rate_sched->txbf_cv_len);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "num_valid_rates: %d | paprd_mask: %d \n",
            rate_sched->num_valid_rates,rate_sched->paprd_mask);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "rts_rix: %d | sh_pream: %d \n",
            rate_sched->rts_rix,rate_sched->sh_pream);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "min_spacing_1_4_us: %d | fixed_delims: %d \n",
            rate_sched->min_spacing_1_4_us,rate_sched->fixed_delims);
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "bw_in_service: %d | probe_rix: %d \n",
            rate_sched->bw_in_service,rate_sched->probe_rix);
#endif

    if(wmi_node_update_rate_sched(pdev->ctrl_pdev, peer->mac_addr.raw,
            vdev->vdev_id, rate_sched)) {
        printk("%s: Unable to send the updated rate-schedule peer\n", __func__);
    }
#endif

#ifdef DEBUG_HOST_RC
    TXRX_PRINT(TXRX_PRINT_LEVEL_INFO1, "Exit %s\n", __func__);
#endif

    return;
}

void ol_rc_update_handler(ol_txrx_pdev_handle pdev, u_int16_t peer_id,
        u_int8_t vdev_id, u_int8_t *peer_mac_addr, u_int8_t num_elems,
        void *args)
{
    struct ol_txrx_peer_t *peer = NULL;
    struct ol_txrx_vdev_t *vdev = NULL;

    const WHAL_RATE_TABLE *pRateTable;
    RATE_CONTEXT *g_pRATE = NULL;
    struct rate_node *pSib = NULL;
    struct TxRateCtrl_s *pRc = NULL;

    A_UINT32 nowMsec = 0;

    A_UINT32 rc_arg_idx=0;

    RC_TX_DONE_PARAMS rc_args[MAX_SERIES_SUPPORTED];
    qdf_mem_set(rc_args,MAX_SERIES_SUPPORTED * sizeof(RC_TX_DONE_PARAMS), 0);

    {
        A_UINT32 i=0;
        for(; i<num_elems; i++) {
            RC_TX_DONE_PARAMS     *tmp_args = &(rc_args[i]);
            HTT_RC_TX_DONE_PARAMS *tmp_htt  = (HTT_RC_TX_DONE_PARAMS *)(args) + i;

            tmp_args->ptx_rc.rateCode = tmp_htt->rate_code;
            tmp_args->ptx_rc.flags    = tmp_htt->rate_code_flags;
            tmp_args->flags           = tmp_htt->flags;
            tmp_args->num_enqued      = tmp_htt->num_enqued;
            tmp_args->num_retries     = tmp_htt->num_retries;
            tmp_args->num_failed      = tmp_htt->num_failed;
            tmp_args->ack_rssi        = tmp_htt->ack_rssi;
            tmp_args->time_stamp      = tmp_htt->time_stamp;
            tmp_args->is_probe        = tmp_htt->is_probe;
        }
    }

#ifdef DEBUG_HOST_RC
    printk("Enter: %s\n", __func__);
#endif

#if ATH_SUPPORT_WRAP
    peer = ol_txrx_peer_find_hash_find(pdev, peer_mac_addr, 1, vdev_id);
#else
    peer = ol_txrx_peer_find_hash_find(pdev, peer_mac_addr, 1);
#endif
    if (peer == NULL) {
        return;
    }

    if ((vdev = peer->vdev) == NULL) {
        return;
    }

    if ((g_pRATE = (RATE_CONTEXT *)(vdev->pRateCtrl)) == NULL) {
        return;
    }

    if ((pSib = (struct rate_node *)(peer->rc_node)) == NULL) {
        return;
    }

    /*TODO: need to update is_probe in rc_args in the target */

    pRc = pSib->txRateCtrl;

    nowMsec = rc_args[0].time_stamp;

    for( rc_arg_idx=0; rc_arg_idx < num_elems; rc_arg_idx++ ) {
        rcUpdate_HT(g_pRATE, pSib, NULL, &(rc_args[rc_arg_idx]), 0);
        /* todo: hbrc: probe_aborted should be filled as for tbrc.
         *       putting as 0 for transparency for now. */
    }

    if (!(pRc->probeRate[0] || pRc->probeRate[1] || pRc->probeRate[2])) {
        RATE_GetTxRetrySchedule(pdev, &pRateTable, peer, (void *)nowMsec, NULL);
    }

#ifdef DEBUG_HOST_RC
    printk("Exit:  %s\n", __func__);
#endif

    return;

}
