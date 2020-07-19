/*
 * Copyright (c) 2015-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*
 * osif_nss_wifiol_if.c
 *
 * This file used for for interface   NSS WiFi Offload Radio
 * ------------------------REVISION HISTORY-----------------------------
 * Qualcomm Atheros         15/june/2015              Created
 */

#include <linux/types.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <net/ipv6.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3,9,0))
#include <net/ipip.h>
#else
#include <net/ip_tunnels.h>
#endif

#include <ol_if_athvar.h>
/* header files for our internal definitions */
#include <ol_txrx_dbg.h>       /* TXRX_DEBUG_LEVEL */
#include <ol_txrx_internal.h>  /* ol_txrx_pdev_t, etc. */
#include <ol_txrx_api_internal.h>  /* ol_txrx_peer_find_inact_timeout_handler, etc. */
#include <ol_txrx.h>           /* ol_txrx_peer_unref_delete */
#include <ol_rx.h>           /* ol_rx_update_peer_stats */
#include <ol_txrx_types.h>
#include <ol_txrx_peer_find.h> /* ol_txrx_peer_find_by_id */
#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/in.h>
#include <asm/cacheflush.h>
#include <nss_api_if.h>
#include <nss_cmn.h>
#include "ath_pci.h"
#include <hif.h>
#include "target_type.h"
#include "osif_private.h"
#include "pktlog_ac_i.h"
#include <wdi_event.h>
#include "osif_nss_wifiol_vdev_if.h"
#include "osif_nss_wifiol_if.h"
#include <htt_internal.h>
#include <ol_txrx_dbg.h>
#include <ieee80211_ique.h>
#include <cdp_txrx_ctrl.h>
#include <ar_internal.h>
#include <init_deinit_lmac.h>

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

#define NSS_PEER_MEMORY_DEFAULT_LENGTH 204000
#define NSS_RRA_MEMORY_DEFAULT_LENGTH 35400
#define STATS_COOKIE_BASE_OFFSET 12
#define NSS_WIFIOL_TARGET_TYPE_QCA9984 10

#include <linux/module.h>
extern int glbl_allocated_radioidx;

#if ATH_SUPPORT_ME_SNOOP_TABLE
extern void
ieee80211_me_SnoopListUpdate_timestamp(struct MC_LIST_UPDATE* list_entry, u_int32_t ether_type);
#endif

extern int osif_nss_be_vdev_set_peer_nexthop(osif_dev *osif, uint8_t *addr, uint16_t if_num);

void osif_nss_vdev_aggregate_msdu(ar_handle_t arh, struct ol_txrx_pdev_t *pdev, qdf_nbuf_t msduorig, qdf_nbuf_t *head_msdu, uint8_t no_clone_reqd);
static int osif_nss_ol_ce_raw_send(struct ol_txrx_pdev_t *pdev, void *nss_wifiol_ctx, uint32_t radio_id, qdf_nbuf_t netbuf, uint32_t  len);

int osif_nss_ol_vap_hardstart(struct sk_buff *skb, struct net_device *dev);

void osif_nss_ol_event_receive(ol_ath_soc_softc_t *soc, struct nss_cmn_msg *wifimsg);
void osif_nss_ol_be_vdev_update_statsv2(struct ol_ath_softc_net80211 *scn, struct sk_buff * nbuf, void *rx_mpdu_desc, uint8_t htt_rx_status);
extern void
ol_ath_mgmt_handler(struct wlan_objmgr_pdev *pdev, struct ol_ath_softc_net80211 *scn, wbuf_t wbuf, struct ieee80211_frame * wh, ol_ath_ieee80211_rx_status_t rx_status, struct mgmt_rx_event_params rx_event, bool null_data_handler);

static int osif_nss_ol_ce_flush_raw_send(ol_ath_soc_softc_t *soc)
{
    int ifnum  = soc->nss_soc.nss_sifnum;
    struct nss_wifi_msg *wifimsg = NULL;
    nss_tx_status_t status;
    void *nss_wifiol_ctx = soc->nss_soc.nss_sctx;
    int radio_id = soc->nss_soc.nss_wifiol_id;

    qdf_print("%s :NSS %pK radio %d if_num ",__FUNCTION__, nss_wifiol_ctx, radio_id, soc->nss_soc.nss_sifnum);

    if (!nss_wifiol_ctx) {
        return 0;
    }

    ifnum = soc->nss_soc.nss_sifnum;
    if(ifnum == -1){
        qdf_print("%s: no if found return ",__FUNCTION__);
        return -1;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));

    /*
     * Send WIFI CE configure
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_FLUSH_HTT_CMD_MSG,
            sizeof(struct nss_wifi_rawsend_msg), NULL, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI CE flush raw send msg failed %d ", status);
        return -1;
    }

    qdf_print("delay 100ms ");
    mdelay(100);
    return 0;
}


void update_ring_info(
        struct nss_wifi_ce_ring_state_msg *sring,
        struct nss_wifi_ce_ring_state_msg *dring,
        struct hif_pipe_addl_info *hif_info) {

    sring->nentries = hif_info->ul_pipe.nentries;
    sring->nentries_mask = hif_info->ul_pipe.nentries_mask;
    sring->sw_index = hif_info->ul_pipe.sw_index;
    sring->write_index = hif_info->ul_pipe.write_index;
    sring->hw_index = hif_info->ul_pipe.hw_index;
    sring->base_addr_CE_space = hif_info->ul_pipe.base_addr_CE_space;
    sring->base_addr_owner_space = (long)hif_info->ul_pipe.base_addr_owner_space;
    qdf_print("src -> NSS:nentries %d mask %x swindex %d write_index %d hw_index %d CE_space %x ownerspace %x ",
            sring->nentries,
            sring->nentries_mask,
            sring->sw_index,
            sring->write_index,
            sring->hw_index,
            sring->base_addr_CE_space,
            sring->base_addr_owner_space);
    dring->nentries = hif_info->dl_pipe.nentries;
    dring->nentries_mask = hif_info->dl_pipe.nentries_mask;
    dring->sw_index = hif_info->dl_pipe.sw_index;
    dring->write_index = hif_info->dl_pipe.write_index;
    dring->hw_index = hif_info->dl_pipe.hw_index;
    dring->base_addr_CE_space = hif_info->dl_pipe.base_addr_CE_space;
    dring->base_addr_owner_space = (long)hif_info->dl_pipe.base_addr_owner_space;
    qdf_print("dest -> NSS:nentries %d mask %x swindex %d write_index %d hw_index %d CE_space %x ownerspace %x ",
            dring->nentries,
            dring->nentries_mask,
            dring->sw_index,
            dring->write_index,
            dring->hw_index,
            dring->base_addr_CE_space,
            dring->base_addr_owner_space);
}

#ifdef WLAN_FEATURE_FASTPATH
static int osif_nss_ol_send_peer_memory(struct ol_ath_softc_net80211 *scn)
{
    struct htt_pdev_t *htt_pdev = scn->soc->htt_handle;
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)htt_pdev->txrx_pdev;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_peer_freelist_append_msg *pfam;
    void *nss_wifiol_ctx = pdev->nss_wifiol_ctx;
    uint32_t len = NSS_PEER_MEMORY_DEFAULT_LENGTH;
    int if_num;
    int pool_id;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;

    if_num = pdev->nss_ifnum;
    if (if_num == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return -1;
    }

    pool_id = pdev->last_peer_pool_used + 1;
    if (pool_id > (OSIF_NSS_OL_MAX_PEER_POOLS - 1)) {
        qdf_print("pooil_id %d is out of limit", pool_id);
        return -1;
    }

    pdev->peer_mem_pool[pool_id] = qdf_mem_malloc(len);
    if (!pdev->peer_mem_pool[pool_id]) {
        qdf_print("Not able to allocate memory for peer mem pool with pool_id %d", pool_id);
        return -1;
    }

    pdev->last_peer_pool_used = pool_id;

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    pfam = &wifimsg->msg.peer_freelist_append;

    pfam->addr = dma_map_single((void *)scn->soc->qdf_dev->dev, pdev->peer_mem_pool[pool_id], len, DMA_TO_DEVICE);
    pfam->length = len;
    pfam->num_peers = pdev->max_peers_per_pool;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    qdf_print("sending peer pool with id %d start addr %x length %d num_peers %d",
            pool_id, pfam->addr, pfam->length, pfam->num_peers);
    /*
     * Send WIFI peer mem pool configure
     */
    nss_cmn_msg_init(&wifimsg->cm, if_num, NSS_WIFI_PEER_FREELIST_APPEND_MSG,
            sizeof(struct nss_wifi_peer_freelist_append_msg), msg_cb, NULL);


    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI peer memory send failed %d ", status);
        qdf_mem_free(pdev->peer_mem_pool[pool_id]);
        pdev->last_peer_pool_used--;
        return -1;
    }

    return 0;
}

static int osif_nss_ol_send_rra_memory(struct ol_ath_softc_net80211 *scn)
{
    struct htt_pdev_t *htt_pdev = scn->soc->htt_handle;
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)htt_pdev->txrx_pdev;
    struct nss_wifi_msg *wifimsg =  NULL;
    struct nss_wifi_rx_reorder_array_freelist_append_msg *pfam;
    void *nss_wifiol_ctx = pdev->nss_wifiol_ctx;
    uint32_t len = NSS_RRA_MEMORY_DEFAULT_LENGTH;
    int if_num;
    int pool_id;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;

    if_num = pdev->nss_ifnum;
    if (if_num == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return -1;
    }

    pool_id = pdev->last_rra_pool_used + 1;
    if (pool_id > (OSIF_NSS_OL_MAX_RRA_POOLS - 1)) {
        qdf_print("pooil_id %d is out of limit", pool_id);
        return -1;
    }

    pdev->rra_mem_pool[pool_id] = qdf_mem_malloc(len);
    if (!pdev->rra_mem_pool[pool_id]) {
        qdf_print("Not able to allocate memory for peer mem pool with pool_id %d", pool_id);
        return -1;
    }

    pdev->last_rra_pool_used = pool_id;

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    pfam = &wifimsg->msg.rx_reorder_array_freelist_append;

    pfam->addr = dma_map_single((void *)scn->soc->qdf_dev->dev, pdev->rra_mem_pool[pool_id], len, DMA_TO_DEVICE);
    pfam->length = len;
    pfam->num_rra = pdev->max_rra_per_pool;
    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    qdf_print("sending rra pool with id %d start addr %x length %d num_rra %d", pool_id, pfam->addr, pfam->length, pfam->num_rra);

    /*
     * Send WIFI rra mem pool configure
     */
    nss_cmn_msg_init(&wifimsg->cm, if_num, NSS_WIFI_RX_REORDER_ARRAY_FREELIST_APPEND_MSG,
            sizeof(struct nss_wifi_rx_reorder_array_freelist_append_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI rra memory send failed %d ", status);
        qdf_mem_free(pdev->rra_mem_pool[pool_id]);
        pdev->last_rra_pool_used--;
        return -1;
    }

    return 0;
}
#else
static int osif_nss_ol_send_peer_memory(struct ol_ath_softc_net80211 *scn)
{
   qdf_print("Error: Fastpath needs to be enabled for Wi-Fi Offload ");
   return -1;
}

static int osif_nss_ol_send_rra_memory(struct ol_ath_softc_net80211 *scn)
{
   qdf_print("Error: Fastpath needs to be enabled for Wi-Fi Offload ");
   return -1;
}
#endif

#define NSS_WIFI_BS_PEER_STATE_SECURED_MASK 0x8000
#define NSS_WIFI_BS_PEER_STATE_SECURED_SHIFT 15

/*
 * osif_nss_ol_process_bs_peer_state_msg
 *     Process the peer state message from NSS-FW
 *      and inform the bs handler
 */
int osif_nss_ol_process_bs_peer_state_msg(struct ol_ath_softc_net80211 *scn,  struct nss_wifi_bs_peer_activity *msg)
{
#if QCA_SUPPORT_SON
    struct ol_txrx_pdev_t *pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);
    struct ol_txrx_peer_t *peer;
    uint16_t i;

    struct ieee80211com *ic = &scn->sc_ic;
    struct ieee80211_node *ni = NULL;
    struct ieee80211vap *vap = NULL;
    uint16_t peer_id;
    bool secured;

    if (!pdev) {
        return 0;
    }

    /*
     *  NSS-FW has sent the message regarding the
     *  activity for each peer.
     *  This infomation has to be passed on to the
     *  band-steering module.
     */
    for (i = 0 ; i < msg->nentries ; i++) {
        secured = (msg->peer_id[i] & (NSS_WIFI_BS_PEER_STATE_SECURED_MASK)) >> NSS_WIFI_BS_PEER_STATE_SECURED_SHIFT;
        peer_id = msg->peer_id[i] & (~(NSS_WIFI_BS_PEER_STATE_SECURED_MASK));
        if (peer_id > pdev->max_peers) {
            continue;
        }
        qdf_spin_lock_bh(&pdev->peer_ref_mutex);
        peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, peer_id);
        if (peer) {
            vap = ol_ath_vap_get(scn, peer->vdev->vdev_id);
            if ((vap) && ieee80211_vap_wnm_is_set(vap) && ieee80211_wnm_bss_is_set(vap->wnm)) {
                ni = ieee80211_find_node(&ic->ic_sta, peer->mac_addr.raw);
                if (ni) {
                    if (ni != ni->ni_bss_node) {
                        ieee80211_wnm_bssmax_updaterx(ni, secured);
                    }
                    ieee80211_free_node(ni);
                }
            }
            if (vap) {
                ol_ath_release_vap(vap);
            }
        }
        qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
    }
#endif
    return 0;
}

/*
 * osif_nss_ol_process_ol_timestats_msg
 * Process the enhanced timestamp stats message from NSS-FW
 */
int osif_nss_ol_process_ol_timestats_msg(struct ol_ath_softc_net80211 *scn,  struct nss_wifi_ol_peer_time_msg *msg)
{
    uint32_t pindex, tid;
    struct ol_txrx_pdev_t *pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);
    struct ol_txrx_peer_t *peer = NULL;

    /*
     * Extract per peer ol stats from the message sent by NSS-FW.
     */
    for (pindex = 0 ; pindex < msg->npeers ; pindex++) {
        peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle) pdev, msg->tstats[pindex].peer_id);
        if (peer) {
            for (tid = 0; tid < NSS_WIFI_TX_NUM_TOS_TIDS; tid++) {
                peer->tstampstat.sum[tid] += msg->tstats[pindex].sum[tid].sum_tx;
                peer->tstampstat.nummsdu[tid] += msg->tstats[pindex].sum[tid].sum_msdus;
                peer->tstampstat.avg[tid] = msg->tstats[pindex].avg[tid];
            }
        }
    }
    return 0;
}

/*
 * osif_nss_ol_process_ol_stats_msg
 * Process the enhanced stats message from NSS-FW
 */
#if ENHANCED_STATS
int osif_nss_ol_process_ol_stats_msg(struct ol_ath_softc_net80211 *scn,  struct nss_wifi_ol_stats_msg *msg)
{
    struct ieee80211com *ic = NULL;
    struct ieee80211_node *ni = NULL;
    struct ol_txrx_vdev_t *vdev = NULL;
    struct ieee80211vap *vap = NULL;
    uint32_t num_msdus = 0, byte_cnt = 0, discard_cnt = 0, i = 0, tx_mgmt = 0, j = 0, bcast_cnt = 0;
    struct ol_txrx_peer_t *peer = NULL;
    struct ol_txrx_pdev_t *pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);
    struct cdp_tx_stats *tx_stats = NULL;
    struct cdp_rx_stats *rx_stats = NULL;

    ic  = &scn->sc_ic;
#ifdef QCA_SUPPORT_CP_STATS
    pdev_cp_stats_tx_beacon_update(ic->ic_pdev_obj, msg->bcn_cnt);
#endif
    OL_TXRX_STATS_ADD(pdev, tx.tx_bawadv, msg->bawadv_cnt);

    /*
     *  Extract per peer ol stats from the message sent by NSS-FW.
     */
    for (i = 0 ; i < msg->npeers ; i++) {
        peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, msg->peer_ol_stats[i].peer_id);
        if (peer) {
            tx_stats = &peer->stats.tx;
            rx_stats = &peer->stats.rx;
            num_msdus = msg->peer_ol_stats[i].tx_data;
            byte_cnt = msg->peer_ol_stats[i].tx_bytes;
            discard_cnt = msg->peer_ol_stats[i].tx_fail;
            tx_mgmt = msg->peer_ol_stats[i].tx_mgmt;
            bcast_cnt = msg->peer_ol_stats[i].tx_bcast_pkts;
            tx_stats->mcast.num += msg->peer_ol_stats[i].tx_mcast;
            tx_stats->ucast.num += msg->peer_ol_stats[i].tx_ucast;
            if (peer->bss_peer) {
                tx_stats->mcast.bytes += byte_cnt;
            } else {
                tx_stats->ucast.bytes += byte_cnt;
                tx_stats->tx_success.num += num_msdus;
                tx_stats->tx_success.bytes += byte_cnt;
            }

            tx_stats->comp_pkt.num +=num_msdus;
            tx_stats->comp_pkt.bytes += byte_cnt;
            tx_stats->bcast.num += bcast_cnt;
            tx_stats->retries += msg->peer_ol_stats[i].ppdu_retries;
            tx_stats->ampdu_cnt += msg->peer_ol_stats[i].tx_aggr;
            tx_stats->non_ampdu_cnt += msg->peer_ol_stats[i].tx_unaggr;
            tx_stats->is_tx_no_ack.num += discard_cnt;

            for (j = 0; j < WME_NUM_AC; j++) {
                tx_stats->wme_ac_type[j] += msg->peer_ol_stats[i].tx_wme[j];
                rx_stats->wme_ac_type[j] += msg->peer_ol_stats[i].rx_wme[j];
                tx_stats->rssi_chain[j] += msg->peer_ol_stats[i].rssi_chains[j];
            }

            vdev = peer->vdev;
            vap = ol_ath_vap_get(scn, vdev->vdev_id);
            if (qdf_likely(vap)) {
                ni = ieee80211_vap_find_node(vap, peer->mac_addr.raw);
                if (ni) {
#ifdef QCA_SUPPORT_CP_STATS
                    vdev_cp_stats_tx_not_ok_inc(vap->vdev_obj, discard_cnt);
                    peer_cp_stats_is_tx_not_ok_inc(ni->peer_obj, discard_cnt);
                    peer_cp_stats_tx_mgmt_inc(ni->peer_obj, tx_mgmt);
#endif
                    ieee80211_free_node(ni);
                }
                ol_ath_release_vap(vap);
            }
#ifdef QCA_SUPPORT_CP_STATS
            pdev_cp_stats_tx_mgmt_inc(ic->ic_pdev_obj, tx_mgmt);
#endif
        }
    }
    return 0;
}
#endif
/*
 * osif_nss_ol_process_sta_kickout_msg
 *     Process the sta kickout message from NSS-FW
 */
int osif_nss_ol_process_sta_kickout_msg(struct ol_ath_softc_net80211 *scn, struct nss_wifi_sta_kickout_msg *msg)
{
    struct ol_txrx_pdev_t *pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);
    struct ol_txrx_peer_t *peer;
    uint8_t peer_macaddr_raw[QDF_MAC_ADDR_SIZE];

    qdf_spin_lock_bh(&pdev->peer_ref_mutex);
    peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, msg->peer_id);
    if (peer) {
        memcpy(peer_macaddr_raw, peer->mac_addr.raw, QDF_MAC_ADDR_SIZE);
    }
    qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
    peer_sta_kickout(scn, (A_UINT8 *)&peer_macaddr_raw);
    return 0;
}


/*
 * osif_nss_ol_process_wnm_peer_rx_activity_msg
 *     Process the rx activity message from NSS-FW
 *      and inform the wnm handler
 */
int osif_nss_ol_process_wnm_peer_rx_activity_msg(struct ol_ath_softc_net80211 *scn, struct nss_wifi_wnm_peer_rx_activity_msg *msg)
{
#if UMAC_SUPPORT_WNM
    struct ol_txrx_pdev_t *pdev;
    struct ieee80211com *ic;
    struct ieee80211_node *ni;
    struct ol_txrx_peer_t *peer;
    struct ieee80211vap *vap;
    uint16_t i;

    ic = &scn->sc_ic;
    pdev = wlan_pdev_get_dp_handle(ic->ic_pdev_obj);

    /*
     *  NSS-FW has sent the message regarding the
     *  rx activity for each peer.
     *  This infomation has to be passed on to wnm.
     */
    for (i = 0 ; i < msg->nentries ; i++) {
        if (msg->peer_id[i] > pdev->max_peers) {
            continue;
        }
        qdf_spin_lock_bh(&pdev->peer_ref_mutex);

        peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, msg->peer_id[i]);
        if (peer) {
            vap = ol_ath_vap_get(scn, peer->vdev->vdev_id);
        } else {
            qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
            continue;
        }

        if (vap && ieee80211_vap_wnm_is_set(vap) && ieee80211_wnm_bss_is_set(vap->wnm)) {
            ni = ieee80211_find_node(&ic->ic_sta, peer->mac_addr.raw);
            if (ni) {
                /*
                 * The second  parameter specifies whether the timer
                 * needs to be update or not.The code flow will reach
                 * here only when NSS properly delivers a packet to the
                 * stack, and this means timer can be updated without any
                 * further checks
                 */
                ieee80211_wnm_bssmax_updaterx(ni, 1);
            }
            ieee80211_free_node(ni);
        }
        if (vap) {
            ol_ath_release_vap(vap);
        }
        qdf_spin_unlock_bh(&pdev->peer_ref_mutex);
    }
#endif
    return 0;
}

#ifdef WLAN_FEATURE_FASTPATH
static inline void osif_nss_wifiol_set_completion_status(ol_ath_soc_softc_t *soc, nss_tx_status_t status)
{
    soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response = status;
    complete(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete);
}

void osif_nss_ol_event_receive(ol_ath_soc_softc_t *soc, struct nss_cmn_msg *wifimsg)
{
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ol_txrx_pdev_t *pdev = NULL;
    struct htt_pdev_t *htt_pdev;
    uint32_t msg_type = wifimsg->type;
    enum nss_cmn_response response = wifimsg->response;
    struct wlan_objmgr_pdev *pdev_obj;

    pdev_obj = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0,
                                    WLAN_MLME_NB_ID);
    if (pdev_obj != NULL) {
         scn = lmac_get_pdev_feature_ptr(pdev_obj);
    }

    htt_pdev = soc->htt_handle;

    /*
     * Handle the message feedback returned from FW
     */

    switch (msg_type) {
        case NSS_WIFI_POST_RECV_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    qdf_print("NSS WIFI OFFLOAD - FAILED TO INITIALIZE CE for NSS radio %d", soc->nss_soc.nss_wifiol_id);
                    soc->nss_soc.nss_wifiol_ce_enabled = 0;
                    osif_nss_wifiol_set_completion_status(soc, NSS_TX_FAILURE);

                    if (pdev_obj != NULL)
                        wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
                    return;
                }
                soc->nss_soc.nss_wifiol_ce_enabled = 1;
                osif_nss_wifiol_set_completion_status(soc, NSS_TX_SUCCESS);
                qdf_print("NSS CE init succeeded for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_INIT_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    qdf_print("NSS WIFI INIT MSG Failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
                    osif_nss_wifiol_set_completion_status(soc, NSS_TX_FAILURE);
                    break;
                }
                osif_nss_wifiol_set_completion_status(soc, NSS_TX_SUCCESS);
                qdf_print("NSS WIFI init succeeded for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_TX_INIT_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    qdf_print("NSS WIFI TX INIT MSG failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
                    osif_nss_wifiol_set_completion_status(soc, NSS_TX_FAILURE);
                    break;
                }
                osif_nss_wifiol_set_completion_status(soc, NSS_TX_SUCCESS);
                qdf_print("NSS WIFI TX init succeeded for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_RAW_SEND_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    qdf_print("NSS WIFI RAW SEND MSG failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
                    break;
                }
                qdf_print("NSS WIFI RAW SEND succeeded for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_MGMT_SEND_MSG:
            {
                if (response == NSS_CMN_RESPONSE_EMSG) {
                    struct nss_wifi_mgmtsend_msg *msg=  &((struct nss_wifi_msg *)wifimsg)->msg.mgmtmsg;

                    uint32_t desc_id = msg->desc_id;

                    if (soc->soc_attached == 0) {
                        if (pdev_obj != NULL)
                            wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
                        return;
                    }

                    qdf_print("Mgmt send failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
                    htt_tx_mgmt_desc_free(htt_pdev, desc_id, IEEE80211_TX_ERROR);
                }
            }
            break;

        case NSS_WIFI_FW_STATS_MSG:
            {

                if (response == NSS_CMN_RESPONSE_EMSG) {
                    struct nss_wifi_fw_stats_msg *msg = &((struct nss_wifi_msg *)wifimsg)->msg.fwstatsmsg;
                    struct ol_txrx_stats_req_internal *req;
                    u_int64_t cookie;
                    uint8_t *data = msg->array;
                    uint8_t *cookie_base = (data + (HTC_HDR_LENGTH + HTC_HDR_ALIGNMENT_PADDING) + STATS_COOKIE_BASE_OFFSET);
                    memcpy(&cookie, cookie_base, sizeof(uint64_t));

                    if(soc->soc_attached == 0) {
                        if(pdev_obj != NULL)
                            wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
                        return;
                    }
                    /*
                     * Drop the packet;
                     */
                    qdf_print("Fw stats request send failed");
                    req = (struct ol_txrx_stats_req_internal *) ((size_t) cookie);

                    pdev = (struct ol_txrx_pdev_t *)htt_pdev->txrx_pdev;

                    htt_pkt_buf_list_del(htt_pdev, cookie);
                    if (req->base.wait.blocking) {
                        qdf_semaphore_release(&soc->stats_sem);
                    }

                    qdf_mem_free(req);
                }
            }
            break;

        case NSS_WIFI_STATS_MSG:
            {
                struct nss_wifi_stats_sync_msg *stats = &((struct nss_wifi_msg *)wifimsg)->msg.statsmsg;
                struct cdp_tx_stats *tx_stats = NULL;
                struct cdp_rx_stats *rx_stats = NULL;

                if (!scn) {
                    qdf_print("%s: scn is NULL, not processing NSS messgage ");
                    break;
                }
                if(soc->soc_attached == 0) {
                    if(pdev_obj != NULL)
                        wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
                    return;
                }

                if (stats->rx_bytes_deliverd) {
#if OL_ATH_SUPPORT_LED
                    scn->scn_led_byte_cnt += stats->rx_bytes_deliverd;
                    ol_ath_led_event(scn, OL_ATH_LED_RX);
#endif
                }

                if (stats->tx_bytes_transmit_completions) {
#if OL_ATH_SUPPORT_LED
                    scn->scn_led_byte_cnt += stats->tx_bytes_transmit_completions;
                    ol_ath_led_event(scn, OL_ATH_LED_TX);
#endif
                }

                htt_pdev = scn->soc->htt_handle;
                pdev = (struct ol_txrx_pdev_t *)htt_pdev->txrx_pdev;

                /*
                 * Update the pdev stats
                 */
                tx_stats = &pdev->pdev_stats.tx;
                rx_stats = &pdev->pdev_stats.rx;
                rx_stats->to_stack.num += stats->rx_pkts_deliverd;
                rx_stats->to_stack.bytes += stats->rx_bytes_deliverd;
                tx_stats->comp_pkt.num += stats->tx_transmit_completions;
                tx_stats->comp_pkt.bytes += stats->tx_bytes_transmit_completions;
                pdev->pdev_stats.tx_i.dropped.desc_na.num += stats->tx_transmit_dropped;
            }
            break;

        case NSS_WIFI_SEND_PEER_MEMORY_REQUEST_MSG:
            {
                if(!scn) {
                    qdf_print("%s: scn is NULL, not processing NSS messgage ");
                    break;
                }
                qdf_print("event: sending peer pool memory");
                osif_nss_ol_send_peer_memory(scn);
            }
            break;

        case NSS_WIFI_SEND_RRA_MEMORY_REQUEST_MSG:
            {
                if (!scn) {
                    qdf_print("%s: scn is NULL, not processing NSS messgage ");
                    break;
                }
                qdf_print("event: sending rra pool memory");
                osif_nss_ol_send_rra_memory(scn);
            }
            break;

        case NSS_WIFI_PEER_BS_STATE_MSG:
            {
                struct nss_wifi_bs_peer_activity *msg =  &((struct nss_wifi_msg *)wifimsg)->msg.peer_activity;
                if (!scn) {
                    qdf_print("%s: scn is NULL, not processing NSS messgage ");
                    break;
                }
                osif_nss_ol_process_bs_peer_state_msg(scn, msg);
            }
            break;

        case NSS_WIFI_OL_STATS_MSG:
            {
#if ENHANCED_STATS
                struct nss_wifi_ol_stats_msg *msg =  &((struct nss_wifi_msg *)wifimsg)->msg.ol_stats_msg;
#endif
                if (!scn) {
                    qdf_print("%s: scn is NULL, not processing NSS messgage ");
                    break;
                }
#if ENHANCED_STATS
                osif_nss_ol_process_ol_stats_msg(scn, msg);
#endif
            }
            break;

        case NSS_WIFI_STA_KICKOUT_MSG:
            {
                struct nss_wifi_sta_kickout_msg *msg =  &((struct nss_wifi_msg *)wifimsg)->msg.sta_kickout_msg;
                if (!scn) {
                    qdf_print("%s: scn is NULL, not processing NSS messgage ");
                    break;
                }
                osif_nss_ol_process_sta_kickout_msg(scn, msg);
            }
            break;

        case NSS_WIFI_WNM_PEER_RX_ACTIVITY_MSG:
            {
                struct nss_wifi_wnm_peer_rx_activity_msg *msg;
                msg = &((struct nss_wifi_msg *)wifimsg)->msg.wprm;
                if (!scn) {
                    qdf_print("%s: scn is NULL, not processing NSS messgage ");
                    break;
                }
                osif_nss_ol_process_wnm_peer_rx_activity_msg(scn, msg);
            }
            break;

        case NSS_WIFI_HTT_INIT_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("HTT RX init configuration failed with error %d for NSS radio %d", wifimsg->error, soc->nss_soc.nss_wifiol_id);
                osif_nss_wifiol_set_completion_status(soc, NSS_TX_FAILURE);

                if (pdev_obj != NULL)
                    wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
                return;
            }

            qdf_print("HTT RX INIT configuration success for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            osif_nss_wifiol_set_completion_status(soc, NSS_TX_SUCCESS);
            break;

        case NSS_WIFI_PEER_STATS_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("Peer Stats Message - send to NSS failed %d", soc->nss_soc.nss_wifiol_id);
                osif_nss_wifiol_set_completion_status(soc, NSS_TX_FAILURE);
                if (pdev_obj != NULL)
                    wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
                return;
            }

            osif_nss_wifiol_set_completion_status(soc, NSS_TX_SUCCESS);
            qdf_print("Peer Stats Message - success %d", soc->nss_soc.nss_wifiol_id);
	    break;

        case NSS_WIFI_WDS_PEER_ADD_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS WIFI PEER ADD MSG failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
                if (pdev_obj != NULL)
                    wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
                return;
            }
            break;

        case NSS_WIFI_WDS_PEER_DEL_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS WIFI PEER DEL MSG failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_PEER_FREELIST_APPEND_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS WIFI PEER FREELIST APPEND MSG failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_RX_REORDER_ARRAY_FREELIST_APPEND_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS WIFI RX REORDER ARRAY FREELIST APPEND MSG failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_WDS_VENDOR_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS WIFI WDS VENDOR MSG failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_DBDC_PROCESS_ENABLE_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS WIFI DBDC Process Enable failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_PRIMARY_RADIO_SET_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS WIFI Primary Radio Set failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_FORCE_CLIENT_MCAST_TRAFFIC_SET_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS Client MCast Traffic Set failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_STORE_OTHER_PDEV_STAVAP_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS Store Other Pdev STAVAP MSG failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_ENABLE_PERPKT_TXSTATS_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS Perpacket TX Stats MSG failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_IGMP_MLD_TOS_OVERRIDE_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS IGMP MLD TOS OVERRIDE failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_MSDU_TTL_SET_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS MSDU_TTL configure msg failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_MONITOR_FILTER_SET_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS Monitor Filter set msg failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_OL_STATS_CFG_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS OL_Stats configure msg failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_ENABLE_OL_STATSV2_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS OL_StatsV2 configure msg failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_OL_PEER_TIME_MSG:
            {
                struct nss_wifi_ol_peer_time_msg *msg =  &((struct nss_wifi_msg *)wifimsg)->msg.wopt_msg;
                if (!scn) {
                    qdf_print("%s: scn is NULL, not processing NSS messgage ");
                    break;
                }
                osif_nss_ol_process_ol_timestats_msg(scn, msg);
            }
            break;

        case NSS_WIFI_PKTLOG_CFG_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS PCKTLOG configure msg failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_STOP_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS WIFI STOP msg failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_TX_QUEUE_CFG_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS WIFI TX QUEUE config failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_TX_MIN_THRESHOLD_CFG_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS WIFI TX MIN THRESHOLD config failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        case NSS_WIFI_RESET_MSG:
            if (response == NSS_CMN_RESPONSE_EMSG) {
                qdf_print("NSS WIFI RESET config failed for NSS radio %d", soc->nss_soc.nss_wifiol_id);
            }
            break;

        default:
            break;
    }

    if (pdev_obj != NULL)
        wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
    /*
     * TODO : Need to add stats updates
     */
    return;
}
#else
void osif_nss_ol_event_receive(ol_ath_soc_softc_t *soc, struct nss_cmn_msg *wifimsg)
{
   qdf_print("Error: Fastpath needs to be enabled for Wi-Fi Offload ");
   return;
}
#endif

void
htt_t2h_msg_handler_fast(void *htt_pdev, qdf_nbuf_t *nbuf_cmpl_arr,
        uint32_t num_cmpls);

static void osif_nss_ol_wifi_ce_callback_func(ol_ath_soc_softc_t *soc, struct sk_buff * nbuf, __attribute__((unused)) struct napi_struct *napi) {

    struct ol_ath_softc_net80211 *scn;
    struct ol_txrx_pdev_t *pdev ;
    struct htt_pdev_t *htt_pdev ;
    qdf_nbuf_t  nbuf_cmpl_arr[32];
    /* uint32_t num_htt_cmpls; */
    uint32_t count;
    struct wlan_objmgr_pdev *pdev_obj;

    pdev_obj = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0,
                                    WLAN_MLME_NB_ID);
    if (pdev_obj == NULL) {
          qdf_print("%s: pdev object (id: 0) is NULL", __func__);
          goto fail1;
    }

    scn = lmac_get_pdev_feature_ptr(pdev_obj);

    nbuf_debug_add_record(nbuf);

    if (scn == NULL) {
        qdf_print("%s: scn is NULL free the nbuf: %pK", __func__, nbuf);
        wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
        goto fail;
    }
    pdev = wlan_pdev_get_dp_handle(pdev_obj);
    if(pdev == NULL) {
        qdf_print("%s: pdev is NULL free the nbuf: %pK for radio ID: %d",
                     __func__, nbuf, scn->radio_id);
        goto fail;
    }

    htt_pdev=  pdev->htt_pdev;

    if (htt_pdev == NULL) {
        qdf_print("%s: htt_pdev is NULL free the nbuf: %pK for radio ID: %d", __func__, nbuf, scn->radio_id);
        goto fail;
    }

    nbuf_cmpl_arr[0] = nbuf;

    htt_t2h_msg_handler_fast(htt_pdev, nbuf_cmpl_arr, 1);
    wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
    return;

fail:
    wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
fail1:
    qdf_print("nbuf->data: ");
    for (count = 0; count < 16; count++) {
        qdf_print("%x ", nbuf->data[count]);
    }
    qdf_print("\n");
    qdf_nbuf_free(nbuf);
}

void ol_rx_process_inv_peer(ol_txrx_pdev_handle pdev, void *rx_mpdu_desc, qdf_nbuf_t msdu);
void phy_data_type_chk(qdf_nbuf_t head_msdu, u_int32_t *cv_chk_status);
void cbf_type_chk(qdf_nbuf_t head_msdu, u_int32_t *cv_chk_status);

/*
 * To check whether msdu is rx_cbf_type
 */
int osif_nss_ol_is_rx_cbf(qdf_nbuf_t msdu, struct htt_pdev_t *htt_pdev)
{
	uint32_t mgmt_type = 0;
	uint32_t phy_data_type = 0;
	uint32_t cv_chk_status = 1;
	uint8_t rx_phy_data_filter;

	phy_data_type = htt_pdev->ar_rx_ops->check_desc_phy_data_type(msdu, &rx_phy_data_filter);
	mgmt_type = htt_pdev->ar_rx_ops->check_desc_mgmt_type(msdu);

	if (phy_data_type == 0) {
		phy_data_type_chk(msdu, &cv_chk_status);
	} else {
		if (mgmt_type == 0) {
			cbf_type_chk(msdu, &cv_chk_status);
		} else {
			return -1;
		}
	}
	return cv_chk_status;
}

/*
 * Extended data callback
 */
static void osif_nss_ol_wifi_ext_callback_func(ol_ath_soc_softc_t *soc, struct sk_buff * nbuf,  __attribute__((unused)) struct napi_struct *napi)
{
    struct ol_ath_softc_net80211 *scn;
    struct ieee80211_frame *wh;
    struct mgmt_rx_event_params rx_event = {0};
    struct nss_wifi_rx_ext_metadata *wrem = (struct nss_wifi_rx_ext_metadata *)nbuf->head;
    struct ol_txrx_pdev_t *pdev;
    struct htt_pdev_t *htt_pdev;
    ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(soc->psoc_obj);

    uint32_t rx_desc_size;
    void *rx_mpdu_desc = (void *)nbuf->head;
    qdf_nbuf_t mon_skb, skb_list_head, next;
    int discard = 1;
    uint32_t type  =  wrem->type;
    enum htt_rx_status status;
    struct wlan_objmgr_pdev *pdev_obj;
    mon_skb = NULL;

    nbuf_debug_add_record(nbuf);

    pdev_obj = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0,
                                    WLAN_MLME_SB_ID);
    if (pdev_obj == NULL) {
       qdf_print("%s: pdev object (id: 0) is NULL", __func__);
       goto free_buf;
    }

    pdev = wlan_pdev_get_dp_handle(pdev_obj);
    htt_pdev = pdev->htt_pdev;
    rx_desc_size = htt_pdev->ar_rx_ops->sizeof_rx_desc();
    scn = lmac_get_pdev_feature_ptr(pdev_obj);
    if (scn == NULL) {
       qdf_print("%s: scn (id: 0) is NULL", __func__);
       goto free_buf;
    }

    dma_unmap_single ((void *)scn->soc->qdf_dev->dev, virt_to_phys(rx_mpdu_desc), rx_desc_size, DMA_FROM_DEVICE);

    switch (type) {
        case NSS_WIFI_RX_EXT_PKTLOG_TYPE:
            {
                struct ol_txrx_peer_t *peer = NULL;
                uint32_t pad_bytes = 0;
                uint16_t peer_id = wrem->peer_id;

                if ((peer_id > OL_TXRX_MAX_PEER_IDS) && (peer_id != HTT_INVALID_PEER)) {
                    qdf_print("%s: Invalid Peer Id peer_id=%d ",__func__, peer_id);
                    break;
                }

                peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, peer_id);
                pad_bytes = htt_pdev->ar_rx_ops->get_l3_header_padding(nbuf);

                status = wrem->htt_rx_status;
                peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, peer_id);
                nbuf->data = nbuf->head + rx_desc_size + pad_bytes;

                if (((status == htt_rx_status_err_fcs) || ((status == htt_rx_status_ctrl_mgmt_null) && peer))
                        && (osif_nss_ol_is_rx_cbf(nbuf, htt_pdev) == 0)) {
                    wdi_event_handler(WDI_EVENT_RX_CBF_REMOTE, pdev, nbuf, peer_id, status);
                } else if (pdev->ap_stats_tx_cal_enable) {
                    ol_rx_update_peer_stats(pdev, nbuf, peer_id, status);
                }
            }
            break;

        case NSS_WIFI_RX_STATS_V2_EXCEPTION:
            {
                status = wrem->htt_rx_status;
                osif_nss_ol_be_vdev_update_statsv2(scn, nbuf, rx_mpdu_desc, status);
            }
            break;

        case NSS_WIFI_RX_MGMT_NULL_TYPE:
            {
                ol_ath_ieee80211_rx_status_t rx_status;
                memset(&rx_status, 0, sizeof(rx_status));
                memset(nbuf->cb, 0, sizeof(nbuf->cb));
                nbuf->protocol = ETH_P_CONTROL;
                wh = (struct ieee80211_frame *) nbuf->data;
                ol_ath_mgmt_handler(pdev_obj, scn, nbuf, wh, rx_status, rx_event, true);
                discard = 0;

	    }
            break;

        case NSS_WIFI_RX_EXT_INV_PEER_TYPE:

        default :
            {
                uint8_t is_mcast, drop_packet = 0;
                uint32_t not_mgmt_type, not_ctrl_type;
                struct ether_header *eh;

                ol_rx_process_inv_peer((ol_txrx_pdev_handle) pdev, rx_mpdu_desc, nbuf);

                qdf_spin_lock_bh(&pdev->mon_mutex);

                if ((pdev->monitor_vdev) && (!pdev->filter_neighbour_peers)) {
                    discard = 0;

                    osif_nss_vdev_aggregate_msdu(htt_pdev->arh, pdev, nbuf, &skb_list_head, 1);

                    if (skb_list_head) {
                        not_mgmt_type = htt_pdev->ar_rx_ops->check_desc_mgmt_type(skb_list_head);
                        not_ctrl_type = htt_pdev->ar_rx_ops->check_desc_ctrl_type(skb_list_head);
                        eh = (struct ether_header *)(nbuf->data);

                        if (qdf_likely((not_mgmt_type && not_ctrl_type) != 0)) {

                            is_mcast = IEEE80211_IS_MULTICAST(eh->ether_dhost);

                            if (qdf_likely(is_mcast != 1)) {
                                /*ucast data pkts*/
                                if (cdp_monitor_get_filter_ucast_data(soc_txrx_handle, (struct cdp_vdev *)pdev->monitor_vdev) == 1) {
                                    /*drop ucast pkts*/
                                    drop_packet = 1;
                                }
                            } else {
                                /*mcast data pkts*/
                                if (cdp_monitor_get_filter_mcast_data(soc_txrx_handle, (struct cdp_vdev *)pdev->monitor_vdev) == 1) {
                                    /*drop mcast pkts*/
                                    drop_packet = 1;
                                }
                            }
                        } else {
                            /*mgmt/ctrl etc. pkts*/
                            if (cdp_monitor_get_filter_non_data(soc_txrx_handle, (struct cdp_vdev *)pdev->monitor_vdev) == 1) {
                                drop_packet = 1;
                            }
                        }

                        if (drop_packet == 0) {
                            mon_skb = htt_pdev->ar_rx_ops->restitch_mpdu_from_msdus(
                                    htt_pdev->arh, skb_list_head, pdev->rx_mon_recv_status, 1);
                        }

                        if (mon_skb) {
                            pdev->monitor_vdev->osif_rx_mon(pdev->monitor_vdev->osif_vdev, mon_skb, pdev->rx_mon_recv_status);
                        } else {
                            mon_skb = skb_list_head;
                            while (mon_skb) {
                                next = qdf_nbuf_next(mon_skb);
                                qdf_nbuf_free(mon_skb);
                                mon_skb = next;
                            }
                        }
                    }
                }
                qdf_spin_unlock_bh(&pdev->mon_mutex);
            }
            break;
    }

    wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_SB_ID);
free_buf:
    if (discard) {
        qdf_nbuf_free(nbuf);
    }
}

#if WDS_VENDOR_EXTENSION
/*
 * nss_wifi_wds_extn_peer_cfg_msg
 *
 *   Send Peer CFG frame to NSS
 */
static int osif_nss_ol_wds_extn_peer_cfg_send(struct ol_ath_softc_net80211 *scn, void *peer_handle)
{
    struct ol_txrx_pdev_t *pdev = NULL;
    struct ol_txrx_peer_t *peer = NULL;
    int ifnum = -1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_wds_extn_peer_cfg_msg *wds_msg;
    nss_tx_status_t status;
    int radio_id;
    void *nss_wifiol_ctx;
    nss_wifi_msg_callback_t msg_cb;

    pdev = (struct ol_txrx_pdev_t *)scn->pdev_txrx_handle;
    if (!pdev) {
        qdf_print("%s: pdev is NULL ",__FUNCTION__);
        return 0;
    }

    nss_wifiol_ctx = pdev->nss_wifiol_ctx;
    if (!nss_wifiol_ctx) {
        return 0;
    }

    peer = (struct ol_txrx_peer_t *)peer_handle;
    radio_id = pdev->nss_wifiol_id;

    qdf_print("%s %pK %d Peer CFG ol stats enable ",__func__, nss_wifiol_ctx, radio_id);

    ifnum = pdev->nss_ifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return -1;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    wds_msg = &wifimsg->msg.wpeercfg;
    wds_msg->peer_id = peer->peer_ids[0];
    peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, wds_msg->peer_id);
    if (!peer) {
        qdf_mem_free(wifimsg);
        return -1;
    }
    wds_msg->wds_flags = (peer->wds_enabled |
      (peer->wds_tx_mcast_4addr << 1)
     | (peer->wds_tx_ucast_4addr << 2)
     | (peer->wds_rx_filter << 3)
     | (peer->wds_rx_ucast_4addr << 4)
     | (peer->wds_rx_mcast_4addr << 5));

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Display these info as it comes only during association
     */
    qdf_print("peer->wds_enabled %d", peer->wds_enabled);
    qdf_print("peer->wds_tx_mcast_4addr %d", peer->wds_tx_mcast_4addr);
    qdf_print("peer->wds_tx_ucast_4addr %d", peer->wds_tx_ucast_4addr);
    qdf_print("peer->wds_rx_filter %d", peer->wds_rx_filter);
    qdf_print("peer->wds_rx_ucast_4addr %d", peer->wds_rx_ucast_4addr);
    qdf_print("peer->wds_rx_mcast_4addr %d", peer->wds_rx_mcast_4addr);

    /*
     * Send wds vendor message
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_WDS_VENDOR_MSG,
            sizeof(struct nss_wifi_wds_extn_peer_cfg_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI Pause message send failed  %d ", status);
        return -1;
    }

    return 0;
}
#endif

/*
 * osif_nss_ol_mgmt_frame_send
 * 	Send Management frame to NSS
 */
int osif_nss_ol_mgmt_frame_send(struct ol_txrx_pdev_t *pdev, void *nss_wifiol_ctx, int32_t radio_id, uint32_t desc_id, qdf_nbuf_t netbuf)
{
    int32_t len;
    int32_t ifnum = -1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_mgmtsend_msg *mgmtmsg;
    nss_wifi_msg_callback_t msg_cb;
    nss_tx_status_t status;

    if (!nss_wifiol_ctx) {
        return 1;
    }

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    if (pdev) {
	    ifnum = pdev->nss_ifnum;
	    if (ifnum == -1) {
		    qdf_print("no i/f found return ");
		    return 1;
	    }
    }


    len = qdf_nbuf_len(netbuf);
    if (len > NSS_WIFI_MGMT_DATA_LEN) {
        qdf_print("%s: The specified length :%d is beyond allowed length:%d", __FUNCTION__, len, NSS_WIFI_MGMT_DATA_LEN);
        return 1;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return 1;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    mgmtmsg = &wifimsg->msg.mgmtmsg;
    mgmtmsg->len = len;
    mgmtmsg->desc_id = desc_id;
    memcpy(mgmtmsg->array, (uint8_t *)netbuf->data, len);

    /*
     * Send wifi Mgmnt Send message
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_MGMT_SEND_MSG,
            sizeof(struct nss_wifi_mgmtsend_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("%s: WIFI Management send failed %d ",__FUNCTION__, status);
        return 1;
    }

    return 0;
}

/*
 * osif_nss_ol_stats_frame_send
 * 	Send FW Stats frame to NSS
 */
int osif_nss_ol_stats_frame_send(struct ol_txrx_pdev_t *pdev, void *nss_wifiol_ctx, int32_t radio_id, qdf_nbuf_t netbuf)
{
    int32_t len = 0;
    int32_t ifnum = -1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_fw_stats_msg *statsmsg;
    nss_wifi_msg_callback_t msg_cb;
    nss_tx_status_t status;

    if (!nss_wifiol_ctx) {
        return 0;
    }

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    if (pdev) {
	    ifnum = pdev->nss_ifnum;
	    if (ifnum == -1) {
		    qdf_print("no i/f found return ");
		    return 1;
	    }
    }

    len = qdf_nbuf_len(netbuf);
    if (len > NSS_WIFI_FW_STATS_DATA_LEN) {
        qdf_print("%s: The specified length :%d is beyond allowed length:%d", __FUNCTION__, len, NSS_WIFI_FW_STATS_DATA_LEN);
        return 1;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Out of memory \n",__func__);
        return 1;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    statsmsg = &wifimsg->msg.fwstatsmsg;
    statsmsg->len = len;
    memcpy(statsmsg->array, (uint8_t *)netbuf->data, len);

    /*
     * Send wifi FW status message
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_FW_STATS_MSG,
                      sizeof(struct nss_wifi_fw_stats_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("%s: WIFI CE configure failed %d ",__FUNCTION__, status);
        return 1;
    }

    return 0;
}

/*
 * osif_nss_peer_stats_frame_send
 *      Send Peer Stats Request Message to NSS
 */
int osif_nss_peer_stats_frame_send(struct ol_txrx_pdev_t *pdev, uint16_t peer_id)
{
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_peer_stats_msg *peer_stats_msg;
    nss_wifi_msg_callback_t msg_cb;
    nss_tx_status_t status;
    uint32_t ret;
    struct ol_ath_softc_net80211 *scn;
    void *nss_wifiol_ctx;
    int32_t ifnum = -1;
    struct ol_txrx_peer_t *peer = NULL;
    int32_t radio_id = -1;
    uint8_t tid = 0;

    if (pdev) {
            radio_id = pdev->nss_wifiol_id;
            ifnum = pdev->nss_ifnum;
            if (ifnum == -1) {
                    qdf_print("no i/f found return ");
                    return 1;
            }
            nss_wifiol_ctx = pdev->nss_wifiol_ctx;
    }

    if (!pdev || !nss_wifiol_ctx) {
        return 0;
    }

    peer = ol_txrx_peer_find_by_id((ol_txrx_pdev_handle)pdev, peer_id);
    if (!peer) {
        qdf_print("%s: Invalid peer id :%d", __FUNCTION__, peer_id);
        return 1;
    }

    scn = (struct ol_ath_softc_net80211 *)
               lmac_get_pdev_feature_ptr((struct wlan_objmgr_pdev *)
                                        pdev->ctrl_pdev);
    if (!scn) {
        qdf_info ("scn is NULL");
        return 1;
    }
    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_info("Failed to allocate memory \n");
        return 1;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    peer_stats_msg = &wifimsg->msg.peer_stats_msg;
    peer_stats_msg->peer_id = peer->peer_ids[0];

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;
    init_completion(&scn->soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete);

    /*
     * Send wifi peer stats message
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_PEER_STATS_MSG,
                      sizeof(struct nss_wifi_peer_stats_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("%s: WiFi peer stats send request %d ",__FUNCTION__, status);
        qdf_mem_free(wifimsg);
        return 1;
    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     */
    ret = wait_for_completion_timeout(&scn->soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete, msecs_to_jiffies(OSIF_NSS_WIFI_CFG_TIMEOUT_MS));
    if (ret == 0) {
        qdf_print("Waiting for peer stats req msg ack timed out %d", radio_id);
        qdf_mem_free(wifimsg);
        return 1;
    }

    /*
     * ACK/NACK received from NSS FW
     */
    if (NSS_TX_FAILURE == scn->soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response) {
        qdf_print("NACK for peer_stats_send request from NSS FW %d", radio_id);
        qdf_mem_free(wifimsg);
        return 1;
    }

    for (tid = 0; tid < NSS_WIFI_TX_NUM_TOS_TIDS; tid++) {
        peer->tidq[tid].stats[TIDQ_ENQUEUE_CNT] += peer_stats_msg->tidq_enqueue_cnt[tid];
        peer->tidq[tid].stats[TIDQ_DEQUEUE_CNT] += peer_stats_msg->tidq_dequeue_cnt[tid];
        peer->tidq[tid].stats[TIDQ_MSDU_TTL_EXPIRY] += peer_stats_msg->tidq_ttl_expire_cnt[tid];
        peer->tidq[tid].stats[TIDQ_PEER_Q_FULL] += peer_stats_msg->tidq_full_cnt[tid];
        peer->tidq[tid].stats[TIDQ_DEQUEUE_REQ_PKTCNT] += peer_stats_msg->tidq_dequeue_req_cnt[tid];
        peer->tidq[tid].high_watermark = peer_stats_msg->tidq_queue_max[tid];
        peer->tidq[tid].byte_count = peer_stats_msg->tidq_byte_cnt[tid];
    }

    qdf_mem_free(wifimsg);
    return 0;
}

static void osif_nss_ol_enable_dbdc_process(struct ieee80211com* ic, uint32_t enable)
{
    void *nss_wifiol_ctx = NULL;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    uint32_t radio_id;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_dbdc_process_enable_msg *dbdc_enable = NULL;
    int ifnum = -1;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;

    nss_wifiol_ctx = scn->nss_radio.nss_rctx;
    if (!nss_wifiol_ctx) {
        return;
    }

    radio_id = scn->soc->nss_soc.nss_wifiol_id;
    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
	    qdf_print("Unable to find radio instance in NSS");
	    return;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    dbdc_enable = &wifimsg->msg.dbdcpe_msg;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

#if DBDC_REPEATER_SUPPORT
    if (enable) {
        qdf_print("Enabling dbdc repeater process");
    } else {
        qdf_print("Disabling dbdc repeater process");
    }

    dbdc_enable->dbdc_process_enable = enable;
#else
    dbdc_enable->dbdc_process_enable = 0;
#endif

    /*
     * Send enable dbdc process msg
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_DBDC_PROCESS_ENABLE_MSG,
            sizeof(struct nss_wifi_dbdc_process_enable_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI enable dbdc process message send failed  %d ", status);
        return;
    }

    return;
}

static void osif_nss_ol_set_primary_radio(struct ol_ath_softc_net80211 *scn , uint32_t enable)
{
    void *nss_wifiol_ctx = NULL;
    uint32_t radio_id;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_primary_radio_set_msg *primary_radio = NULL;
    int ifnum = -1;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;
    struct ol_txrx_pdev_t *pdev = NULL;

    nss_wifiol_ctx = scn->nss_radio.nss_rctx;
    if (!nss_wifiol_ctx) {
        return;
    }

    pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);
    if (!pdev) {
        return;
    }

    radio_id = pdev->nss_wifiol_id;

    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    primary_radio = &wifimsg->msg.wprs_msg;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

#if DBDC_REPEATER_SUPPORT
    if (enable) {
        qdf_print("Enabling primary_radio for if_num %d", ifnum);
    } else {
        qdf_print("Disabling primary_radio for if_num %d", ifnum);
    }

    primary_radio->flag = enable;
#else
    primary_radio->flag = 0;
#endif

    /*
     * Send set primary radio msg
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_PRIMARY_RADIO_SET_MSG,
            sizeof(struct nss_wifi_primary_radio_set_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI set primary radio message send failed  %d ", status);
        return;
    }

    return;
}

static void osif_nss_ol_set_always_primary(struct ieee80211com* ic, bool enable)
{
    void *nss_wifiol_ctx = NULL;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    uint32_t radio_id;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_always_primary_set_msg *primary_radio = NULL;
    int ifnum;
    nss_tx_status_t status;

    if (!scn) {
        return;
    }

    nss_wifiol_ctx = scn->nss_radio.nss_rctx;
    if (!nss_wifiol_ctx) {
        return;
    }

    radio_id = scn->soc->nss_soc.nss_wifiol_id;
    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    primary_radio = &wifimsg->msg.waps_msg;

    if (enable) {
        qdf_print("Enabling always primary ");
    } else {
        qdf_print("Disabling always primary ");
    }

    primary_radio->flag = enable;

    /*
     * Send set always primary msg
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_ALWAYS_PRIMARY_SET_MSG,
            sizeof(struct nss_wifi_always_primary_set_msg), NULL, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI set always primary msg failed  %d ", status);
    }

    qdf_mem_free(wifimsg);
    return;
}

static void osif_nss_ol_set_force_client_mcast_traffic(struct ieee80211com* ic, bool enable)
{
    void *nss_wifiol_ctx = NULL;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    uint32_t radio_id;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_force_client_mcast_traffic_set_msg *force_client_mcast_traffic = NULL;
    int ifnum;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;

    if (!scn) {
        return;
    }

    nss_wifiol_ctx = scn->nss_radio.nss_rctx;
    if (!nss_wifiol_ctx) {
        return;
    }

    radio_id = scn->soc->nss_soc.nss_wifiol_id;
    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Out of memory \n",__func__);
        return;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    force_client_mcast_traffic = &wifimsg->msg.wfcmts_msg;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

#if DBDC_REPEATER_SUPPORT
    qdf_print("force_client_mcast_traffic for if_num %d is %d", ifnum, ic->ic_global_list->force_client_mcast_traffic);

    force_client_mcast_traffic->flag = ic->ic_global_list->force_client_mcast_traffic;
#else
    force_client_mcast_traffic->flag = 0;
#endif

    /*
     * Send force client mcast traffic msg
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_FORCE_CLIENT_MCAST_TRAFFIC_SET_MSG,
            sizeof(struct nss_wifi_force_client_mcast_traffic_set_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI force client mcast traffic message send failed  %d ", status);
        return;
    }

    return;
}

static void osif_nss_ol_set_drop_secondary_mcast(struct ieee80211com* ic, bool enable)
{
    /*
     * This is not supported for Legacy systems.
     */
    return;
}

#if DBDC_REPEATER_SUPPORT
uint32_t osif_nss_ol_validate_dbdc_process(struct ieee80211com* ic) {
    struct global_ic_list *glist = ic->ic_global_list;
    int i;
    uint32_t nss_radio_sta_vap = 0;
    uint32_t non_nss_radio_sta_vap = 0;
    struct ieee80211com* tmpic;

    if (glist->dbdc_process_enable) {

        qdf_print("DBDC process check  num IC %d ",glist->num_global_ic);
        for(i=0; i < glist->num_global_ic; i++) {
            tmpic = glist->global_ic[i];
            if (tmpic && tmpic->ic_sta_vap) {
                if (tmpic->ic_is_mode_offload(tmpic)) {
                    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(tmpic);
                    if (!scn) {
                        continue;
                    }
                    if(scn->nss_radio.nss_rctx) {

                        nss_radio_sta_vap++;
                    }else {
                        non_nss_radio_sta_vap++;
                    }
                } else {
                    non_nss_radio_sta_vap++;
                }
            }
        }
        qdf_print("nss_radio sta vap %d non nss radio sta vap %d ", nss_radio_sta_vap, non_nss_radio_sta_vap);
        if( nss_radio_sta_vap && non_nss_radio_sta_vap) {
            qdf_print("NSS Offload: Disable dbdc process ,non supported configuration");
            glist->dbdc_process_enable = 0;

            for(i=0; i < glist->num_global_ic; i++) {
                tmpic = glist->global_ic[i];
                if (tmpic->ic_is_mode_offload(tmpic)) {
                    qdf_print("NSS Offload: Send command to Disable dbdc process ");
                    osif_nss_ol_enable_dbdc_process(tmpic, 0);
                }
            }
            return 1;
        }
    }
    return 0;
}
#endif

#if DBDC_REPEATER_SUPPORT
static void osif_nss_ol_store_other_pdev_stavap(struct ieee80211com* ic)
{
    void *nss_wifiol_ctx = NULL;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    uint32_t radio_id, i;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_store_other_pdev_stavap_msg *pdev_stavap = NULL;
    int ifnum = -1;
    nss_tx_status_t status;
    struct ieee80211vap *vap = NULL;
    osif_dev *osifp = NULL;
    struct ieee80211com* other_ic = NULL;
    struct ieee80211com* tmp_ic;
    struct ol_txrx_pdev_t *tpdev;
    nss_wifi_msg_callback_t msg_cb;

    if (!ic->ic_is_mode_offload(ic)) {
        qdf_print("NSS offload : DBDC repeater IC is not in partial offload mode discard command to NSS ");
        return;
    }

    nss_wifiol_ctx= scn->nss_radio.nss_rctx;
    if (!nss_wifiol_ctx) {
        return;
    }

    radio_id = scn->soc->nss_soc.nss_wifiol_id;
    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
	    qdf_print("Unable to find radio instance in NSS");
	    return;
    }


    if(osif_nss_ol_validate_dbdc_process(ic)){
        return;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Out of memory \n",__func__);
        return;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    pdev_stavap = &wifimsg->msg.wsops_msg;

    for (i = 0 ;i < MAX_RADIO_CNT-1; i++) {
        tmp_ic = ic->other_ic[i];

        if (tmp_ic && tmp_ic->ic_is_mode_offload(tmp_ic)) {
            tpdev = wlan_pdev_get_dp_handle(tmp_ic->ic_pdev_obj);
            if (!tpdev ) {
                continue ;
            }
            if (tpdev->nss_wifiol_ctx && tmp_ic->ic_sta_vap) {
                qdf_print("NSS : Other IC in NSS mode with station VAP ");
                other_ic = tmp_ic;
                break;
            }
        }
    }

    if (!other_ic) {
        qdf_print("No other sta vap in nss offload mode ");
        qdf_mem_free(wifimsg);
        return;
    }

    vap = other_ic->ic_sta_vap;
    osifp = (osif_dev *)(vap->iv_ifp);
    pdev_stavap->stavap_ifnum = osifp->nss_ifnum;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;
    qdf_print("pdev if_num %d other pdev stavap if_num is %d other_stavap: %pK ",ifnum,  pdev_stavap->stavap_ifnum, vap);

    /*
     * Send store other pdev stavap msg
     */
    if(ifnum == NSS_PROXY_VAP_IF_NUMBER) {
        qdf_print("Error : Psta vap not supported for mcast recv ");
        qdf_mem_free(wifimsg);
        return;
    }
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_STORE_OTHER_PDEV_STAVAP_MSG,
            sizeof(struct nss_wifi_store_other_pdev_stavap_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI store other pdev stavap message send failed  %d ", status);
        return;
    }

    return;
}

#endif

static void osif_nss_ol_set_perpkt_txstats(struct ol_ath_softc_net80211 *scn)
{
    void *nss_wifiol_ctx = NULL;
    uint32_t radio_id;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_enable_perpkt_txstats_msg *set_perpkt_txstats;
    int ifnum;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;

    if (!scn) {
        return;
    }

    nss_wifiol_ctx = scn->nss_radio.nss_rctx;
    if (!nss_wifiol_ctx) {
        return;
    }

    radio_id = scn->soc->nss_soc.nss_wifiol_id;
    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    set_perpkt_txstats = &wifimsg->msg.ept_msg;

#if ATH_DATA_TX_INFO_EN
    set_perpkt_txstats->perpkt_txstats_flag = scn->enable_perpkt_txstats;
#else
    set_perpkt_txstats->perpkt_txstats_flag = 0;
#endif

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send perpkt_txstats configure msg
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_ENABLE_PERPKT_TXSTATS_MSG,
            sizeof(struct nss_wifi_enable_perpkt_txstats_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI set perpkt txstats message send failed  %d ", status);
        return;
    }

    return;
}

static void osif_nss_ol_set_igmpmld_override_tos(struct ol_ath_softc_net80211 *scn)
{
    void *nss_wifiol_ctx = NULL;
    uint32_t radio_id;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_igmp_mld_override_tos_msg *igmp_mld_tos;
    int ifnum;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;

    if (!scn) {
        return;
    }

    nss_wifiol_ctx = scn->nss_radio.nss_rctx;
    if (!nss_wifiol_ctx) {
        return;
    }

    radio_id = scn->soc->nss_soc.nss_wifiol_id;
    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    igmp_mld_tos = &wifimsg->msg.wigmpmldtm_msg;

    igmp_mld_tos->igmp_mld_ovride_tid_en = scn->igmpmld_override;
    igmp_mld_tos->igmp_mld_ovride_tid_val = scn->igmpmld_tid;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send igmp_mld_tos override configure msg
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_IGMP_MLD_TOS_OVERRIDE_MSG,
            sizeof(struct nss_wifi_igmp_mld_override_tos_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI set igmp/mld tos configuration failed :%d ", status);
        return;
    }

    return;
}
void osif_nss_ol_set_msdu_ttl(ol_txrx_pdev_handle ol_pdev)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)ol_pdev;
    void *nss_wifiol_ctx = pdev->nss_wifiol_ctx;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_msdu_ttl_set_msg *set_msdu_ttl;
    int ifnum;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;

    if (!nss_wifiol_ctx) {
        return;
    }

    ifnum = pdev->nss_ifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    set_msdu_ttl = &wifimsg->msg.msdu_ttl_set_msg;
    set_msdu_ttl->msdu_ttl = pdev->pflow_msdu_ttl * pdev->pflow_cong_ctrl_timer_interval;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send msdu_ttl configure msg
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_MSDU_TTL_SET_MSG,
            sizeof(struct nss_wifi_msdu_ttl_set_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI set msdu ttl message send failed  %d ", status);
        return;
    }

    return;
}

static int osif_nss_ol_wifi_monitor_set_filter(struct ieee80211com* ic, uint32_t filter_type)
{

    struct ol_ath_softc_net80211 *scn;
    int ifnum ;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_monitor_set_filter_msg *set_filter_msg ;
    nss_tx_status_t status;
    void *nss_wifiol_ctx;
    int radio_id;
    nss_wifi_msg_callback_t msg_cb;

    if (!ic->ic_is_mode_offload(ic)) {
        return 0;
    }

    scn = OL_ATH_SOFTC_NET80211(ic);
    nss_wifiol_ctx= scn->nss_radio.nss_rctx;
    if (!nss_wifiol_ctx) {
        return 0;
    }

    radio_id = scn->soc->nss_soc.nss_wifiol_id;
    ifnum = scn->nss_radio.nss_rifnum;

    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return -1;
    }

    qdf_print("%s %pK %pK %d %d ",__func__,nss_wifiol_ctx, scn, radio_id, filter_type);

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    set_filter_msg = &wifimsg->msg.monitor_filter_msg;
    set_filter_msg->filter_type = filter_type;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send Monitor Filter Set message
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_MONITOR_FILTER_SET_MSG,
            sizeof(struct nss_wifi_monitor_set_filter_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI monitor set filter message send failed  %d ", status);
        return -1;
    }

    return 0;
}

/*
 *osif_nss_ol_stats_cfg
 * 	configuring pkt log for wifi
 */

static int osif_nss_ol_stats_cfg(struct ol_ath_softc_net80211 *scn, uint32_t cfg)
{
    int ifnum=-1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_ol_stats_cfg_msg *pcm_msg ;
    nss_tx_status_t status;
    void *nss_wifiol_ctx= scn->nss_radio.nss_rctx;
    int radio_id = scn->soc->nss_soc.nss_wifiol_id;
    nss_wifi_msg_callback_t msg_cb;

    if (!nss_wifiol_ctx) {
        return 0;
    }

    qdf_print("%s %pK %pK %d ol stats enable %d",__func__, nss_wifiol_ctx, scn, radio_id, cfg);

    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return -1;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    pcm_msg = &wifimsg->msg.scm_msg;
    pcm_msg->stats_cfg = cfg;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send ol_stats configure
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_OL_STATS_CFG_MSG,
            sizeof(struct nss_wifi_ol_stats_cfg_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI Pause message send failed  %d ", status);
        return -1;
    }

    return 0;
}

/*
 * osif_nss_gre_rx_stats_callback
 *  Register gre callback function.
 */
#ifdef WLAN_FEATURE_FASTPATH
void osif_nss_gre_rx_stats_callback(struct net_device *netdev, struct sk_buff *skb)
{
    uint32_t rx_desc_size = 0;
    void *rx_mpdu_desc = NULL;
    osif_dev *osifp;
    struct ieee80211vap *vap;
    struct ol_ath_vap_net80211 *avn;
    struct ol_ath_softc_net80211 *scn = NULL;

    htt_pdev_handle htt_pdev;
    qdf_nbuf_t fraglist;
    osifp = netdev_priv(netdev);

    if (!osifp) {
        qdf_print("osif_nss_gre_rx_stats_callback osifp null ");
        return;
    }

    vap = osifp->os_if;
    avn = OL_ATH_VAP_NET80211(vap);
    scn = avn->av_sc;
    fraglist = qdf_nbuf_get_ext_list(skb);

    if (fraglist) {
        skb = fraglist;
    }

    rx_mpdu_desc = (void *)skb->head;
    htt_pdev = scn->soc->htt_handle;
    rx_desc_size = htt_pdev->ar_rx_ops->sizeof_rx_desc();

    if (scn->enable_statsv2) {
        dma_unmap_single ((void *)scn->soc->qdf_dev->dev, virt_to_phys(rx_mpdu_desc), rx_desc_size, DMA_FROM_DEVICE);
        osif_nss_ol_be_vdev_update_statsv2(scn, skb, rx_mpdu_desc, HTT_RX_IND_MPDU_STATUS_OK);
    }
}
#else
void osif_nss_gre_rx_stats_callback(struct net_device *netdev, struct sk_buff *skb)
{
   qdf_print("Error: Fastpath needs to be enabled for Wi-Fi Offload ");
   return;
}
#endif

/*
 * osif_nss_ol_enable_ol_statsv2
 * 	Inform nss to send packet to host
 */
static void osif_nss_ol_enable_ol_statsv2(struct ol_ath_softc_net80211 *scn, uint32_t value)
{
    int ifnum=-1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_enable_ol_statsv2 *msg;
    nss_tx_status_t status;
    void *nss_wifiol_ctx= scn->nss_radio.nss_rctx;
    int radio_id = scn->soc->nss_soc.nss_wifiol_id;
    nss_wifi_msg_callback_t msg_cb;

    if (!nss_wifiol_ctx) {
        return;
    }

    qdf_print("%s %pK %pK %d ol statsV2 enable %d",__func__, nss_wifiol_ctx, scn, radio_id, value);

    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    msg = &wifimsg->msg.wesh_msg;
    msg->enable_ol_statsv2 = value;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send to host configure
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_ENABLE_OL_STATSV2_MSG,
            sizeof(struct nss_wifi_enable_ol_statsv2), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("ol statsV2 enable failed  %d ", status);
        return;
    }

    if (value) {
        nss_gre_register_pkt_callback((nss_gre_pkt_callback_t)osif_nss_gre_rx_stats_callback);
    } else {
        nss_gre_unregister_pkt_callback();
    }

    return;
}

static int osif_nss_ol_pktlog_cfg(struct ol_ath_softc_net80211 *scn, int enable)
{
    int ifnum=-1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_pktlog_cfg_msg *pcm_msg ;
    nss_tx_status_t status;
    void *nss_wifiol_ctx= scn->nss_radio.nss_rctx;
    int radio_id;
    nss_wifi_msg_callback_t msg_cb;

    if (!nss_wifiol_ctx) {
        return 0;
    }

    radio_id = scn->soc->nss_soc.nss_wifiol_id;
    ifnum = scn->nss_radio.nss_rifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return -1;
    }

    qdf_print("%s %pK %pK %d PKT log enable %d buf 64 ",__func__,nss_wifiol_ctx, scn, radio_id, enable);
#ifndef REMOVE_PKT_LOG
    qdf_print("pktlog hdrsize %d msdu_id_offset %d ", scn->pl_dev->pktlog_hdr_size, scn->pl_dev->msdu_id_offset);
#endif
    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    pcm_msg = &wifimsg->msg.pcm_msg;
    pcm_msg->enable = enable;
    pcm_msg->bufsize = 64;
#ifndef REMOVE_PKT_LOG
    pcm_msg->hdrsize = scn->pl_dev->pktlog_hdr_size;
#endif
    pcm_msg->msdu_id_offset = scn->pl_dev->msdu_id_offset;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send packet_log configure
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_PKTLOG_CFG_MSG,
            sizeof(struct nss_wifi_pktlog_cfg_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI Pause message send failed  %d ", status);
        return -1;
    }

    return 0;
}

/*
 *osif_nss_tx_queue_cfg
 * 	configuring tx queue size for wifi
 */
static int osif_nss_tx_queue_cfg(struct ol_ath_softc_net80211 *scn, uint32_t range, uint32_t buf_size)
{
    struct ol_txrx_pdev_t *pdev;
    void *nss_wifiol_ctx;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_tx_queue_cfg_msg *wtxqcm ;
    int ifnum = -1;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;

    pdev = (struct ol_txrx_pdev_t *) wlan_pdev_get_dp_handle(scn->sc_pdev);
    nss_wifiol_ctx = pdev->nss_wifiol_ctx;
    if (!nss_wifiol_ctx) {
        return 0;
    }

    ifnum = pdev->nss_ifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return 0;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    wtxqcm = &wifimsg->msg.wtxqcm;
    wtxqcm->range = range;
    wtxqcm->size = buf_size;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send tx_queue configure
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_TX_QUEUE_CFG_MSG,
            sizeof(struct nss_wifi_tx_queue_cfg_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI radio mode message failed  %d ", status);
        return -1;
    }

    return 0;
}

/*
 * osif_nss_tx_queue_min_threshold_cfg
 * 	configuring tx min threshold for queueing for wifi
 */
static int osif_nss_tx_queue_min_threshold_cfg(struct ol_ath_softc_net80211 *scn, uint32_t min_threshold)
{
    struct ol_txrx_pdev_t *pdev;
    void *nss_wifiol_ctx;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_tx_min_threshold_cfg_msg *wtx_min_threshold_cm;
    int ifnum = -1;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;

    pdev = (struct ol_txrx_pdev_t *)wlan_pdev_get_dp_handle(scn->sc_pdev);
    nss_wifiol_ctx = pdev->nss_wifiol_ctx;

    if (!nss_wifiol_ctx) {
        return 0;
    }

    ifnum = pdev->nss_ifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return 0;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    wtx_min_threshold_cm = &wifimsg->msg.wtx_min_threshold_cm;
    wtx_min_threshold_cm->min_threshold = min_threshold;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send min_threshold configure
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_TX_MIN_THRESHOLD_CFG_MSG,
            sizeof(struct nss_wifi_tx_min_threshold_cfg_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI radio mode message failed  %d ", status);
        return -1;
    }

    return 0;
}

/*
 * osif_nss_ol_wifi_tx_capture_set
 *  Enable Tx Data Capture from TX Completion MSG
 */
static int osif_nss_ol_wifi_tx_capture_set(struct ol_ath_softc_net80211 *scn, uint8_t tx_capture_enable)
{
    void *nss_wifiol_ctx = scn->nss_radio.nss_rctx;
    uint32_t radio_id;
    struct nss_wifi_msg *wifimsg = NULL;
    int ifnum  = -1;
    struct nss_wifi_tx_capture_msg *tx_capture_msg;
    nss_tx_status_t status;

    if (!nss_wifiol_ctx) {
        return 0;
    }

    radio_id = scn->soc->nss_soc.nss_wifiol_id;
    ifnum = scn->nss_radio.nss_rifnum;

    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return -1;
    }

    qdf_print("%s %pK %d %d ",__func__,nss_wifiol_ctx,radio_id,tx_capture_enable);

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    tx_capture_msg = &wifimsg->msg.tx_capture_msg;
    tx_capture_msg->tx_capture_enable = tx_capture_enable;

    /*
     * Send WIFI CE configure
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_TX_CAPTURE_SET_MSG,
            sizeof(struct nss_wifi_tx_capture_msg), NULL, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI TX Data Capture message send failed  %d ", status);
        return -1;
    }

    return 0;
}


static int osif_nss_ol_wifi_reset(ol_ath_soc_softc_t *soc, uint16_t delay)
{
    int ifnum=-1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_reset_msg *resetmsg ;
    nss_tx_status_t status;
    void *nss_wifiol_ctx;
    int radio_id, retrycnt = 0;
    int i = 0;
    nss_wifi_msg_callback_t msg_cb;
    struct ol_txrx_pdev_t *pdev = NULL;
    struct wlan_objmgr_pdev *pdev_obj;
    struct ol_ath_softc_net80211 *scn;

    pdev_obj = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0,
                                    WLAN_MLME_NB_ID);
    if (pdev_obj == NULL) {
          qdf_print("%s: pdev object (id: 0) is NULL", __func__);
          return 0;
    }
    pdev = (struct ol_txrx_pdev_t *)wlan_pdev_get_dp_handle(pdev_obj);
    scn = lmac_get_pdev_feature_ptr(pdev_obj);
    if (scn == NULL) {
       qdf_print("%s: scn (id: 0) is NULL", __func__);
       wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
       return 0;
    }

    /*
     * Reset the Global Allocated RadioIDX
     */
    if (soc->nss_soc.nss_sidx != -1) {
        glbl_allocated_radioidx &= ~(1 << soc->nss_soc.nss_sidx);
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_ERROR,
        "%s: Reset Glbl RadioIDX to %d", __func__, glbl_allocated_radioidx);
    }

    nss_wifiol_ctx= soc->nss_soc.nss_sctx;
    if (!nss_wifiol_ctx) {
        if (pdev_obj)
            wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);

        return 0;
    }

    radio_id = soc->nss_soc.nss_wifiol_id;

    qdf_print("%s %pK %pK %d ",__func__,nss_wifiol_ctx, soc, radio_id);

    ifnum = soc->nss_soc.nss_sifnum;
    if (ifnum == -1) {
        qdf_print("%s: NSS radio interface num is incorrect ",__FUNCTION__);
        if (pdev_obj)
            wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
        return -1;
    }

    /*
     * clear the wifiol ctx
     */
    soc->nss_soc.nss_sctx = NULL;
    scn->nss_radio.nss_rctx = NULL;

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    resetmsg = &wifimsg->msg.resetmsg;
    resetmsg->radio_id = radio_id;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send WIFI CE configure
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_RESET_MSG,
            sizeof(struct nss_wifi_reset_msg), msg_cb, NULL);

    while((status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg)) != NSS_TX_SUCCESS)
    {
        retrycnt++;
        qdf_print("WIFI reset message send failed  %d Retrying %d ", status, retrycnt);
        if (retrycnt >= OSIF_NSS_OL_MAX_RETRY_CNT){
            qdf_print("Max retry reached : Unregistering with NSS");
            break;
        }
    }

    qdf_mem_free(wifimsg);
    if (pdev) {

        qdf_print("freeing rra memory pool");
        for (i = 0; i < OSIF_NSS_OL_MAX_RRA_POOLS; i++) {
            if (pdev->rra_mem_pool[i]) {
                qdf_print("freeing rra mem pool for pool_id %d", i);
                qdf_mem_free(pdev->rra_mem_pool[i]);
                pdev->rra_mem_pool[i] = NULL;
            }
        }

        qdf_print("freeing peer memory pool");
        for (i = 0; i < OSIF_NSS_OL_MAX_PEER_POOLS; i++) {
            if (pdev->peer_mem_pool[i]) {
                qdf_print("freeing peer mem pool for pool_id %d", i);
                qdf_mem_free(pdev->peer_mem_pool[i]);
                pdev->peer_mem_pool[i] = NULL;
            }
        }
    }

    qdf_print("nss wifi offload reset");
    if (delay) {
        mdelay(delay);
    }
    /*
     * Unregister wifi with NSS
     */
    qdf_print("unregister wifi with nss");

    /*
     * Clear nss variable state
     */
    nss_unregister_wifi_if(ifnum);
    soc->nss_soc.nss_wifiol_ce_enabled = 0;

    wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
    return 0;
}

static int osif_nss_ol_wifi_stop(ol_ath_soc_softc_t *soc)
{
    int ifnum=-1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_stop_msg *pausemsg ;
    nss_tx_status_t status;
    void *nss_wifiol_ctx;
    int radio_id, retrycnt = 0;
    nss_wifi_msg_callback_t msg_cb;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct wlan_objmgr_pdev *pdev_obj;
    struct ieee80211com *ic = NULL, *tmp_ic;

    nss_wifiol_ctx= soc->nss_soc.nss_sctx;
    if (!nss_wifiol_ctx) {
        return 0;
    }
    radio_id = soc->nss_soc.nss_wifiol_id;

    qdf_print("%s %pK %pK %d ",__func__,nss_wifiol_ctx, soc, radio_id);

    ifnum = soc->nss_soc.nss_sifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return -1;
    }

    nss_gre_unregister_pkt_callback();

    /*
     * Check if DBDC is enabled.
     * If enabled, before sending soc stop, send dbdc disable to all the radios.
     */

    pdev_obj = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0,
                                          WLAN_MLME_NB_ID);
    if (pdev_obj != NULL) {
        scn = lmac_get_pdev_feature_ptr(pdev_obj);
    }

    if (scn) {
        ic = &scn->sc_ic;
        if (ic && ic->ic_global_list && ic->ic_global_list->dbdc_process_enable) {
            int i;
            for (i=0;i < ic->ic_global_list->num_global_ic;i++) {
                tmp_ic = ic->ic_global_list->global_ic[i];
                if (tmp_ic && tmp_ic->nss_radio_ops)
                    tmp_ic->nss_radio_ops->ic_nss_ol_enable_dbdc_process(tmp_ic, 0);
            }
        }
    }

    if (pdev_obj != NULL) {
        wlan_objmgr_pdev_release_ref(pdev_obj, WLAN_MLME_NB_ID);
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Out of memory \n",__func__);
        return -1;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    pausemsg = &wifimsg->msg.stopmsg;
    pausemsg->radio_id = radio_id;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send WIFI Stop Message
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_STOP_MSG,
            sizeof(struct nss_wifi_stop_msg), msg_cb, NULL);

    while((status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg)) != NSS_TX_SUCCESS)
    {
        retrycnt++;
        qdf_print("WIFI Pause message send failed  %d Retrying %d ", status, retrycnt);
        if (retrycnt >= OSIF_NSS_OL_MAX_RETRY_CNT){
            qdf_print("Max retry reached : Unregistering with NSS");
            qdf_mem_free(wifimsg);
            return -1;
        }
    }

    qdf_mem_free(wifimsg);
    return 0;
}

static int osif_nss_wifi_set_default_nexthop(ol_ath_soc_softc_t *soc)
{
    nss_tx_status_t status;

    if (!soc->nss_soc.nss_sctx) {
        qdf_err("%s: nss core ctx is invalid", __FUNCTION__);
        return -1;
    }

    status = nss_wifi_vdev_base_set_next_hop(soc->nss_soc.nss_sctx, NSS_ETH_RX_INTERFACE);
    if (status != NSS_TX_SUCCESS) {
        qdf_err("%s: Unable to send the vdev set next hop message to NSS %d", __FUNCTION__, status);
        return -1;
    }

    return 0;
}

static void osif_nss_wifi_peer_delete(ol_ath_soc_softc_t *soc, uint16_t peer_id, uint8_t vdev_id)
{
    qdf_print("NSS-WifOffoad: peer delete (nocode) ");
}

static void osif_nss_wifi_peer_create(ol_ath_soc_softc_t *soc, uint16_t peer_id,
        uint16_t hw_peer_id, uint8_t vdev_id, uint8_t *peer_mac_addr, uint32_t tx_ast_hash)
{
    qdf_print("NSS-WifOffoad: peer create (nocode) ");
}

static int osif_nss_ol_wifi_init(ol_ath_soc_softc_t *soc) {

#define CE_HTT_MSG_CE           5
#define CE_HTT_TX_CE            4

    struct hif_opaque_softc *osc;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_init_msg *msg;
    int ifnum = -1;
    void * nss_wifiol_ctx= NULL;
    int features = 0;
    struct hif_pipe_addl_info *hif_info_rx;
    struct hif_pipe_addl_info *hif_info_tx;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;
    uint32_t target_type = lmac_get_tgt_type(soc->psoc_obj);
    uint32_t ret;

    osc = lmac_get_ol_hif_hdl(soc->psoc_obj);
    if (!osc) {
        qdf_print(" hif handle is NULL : %s ", __func__ );
        return -1;
    }

    hif_set_nss_wifiol_mode(osc, 1);
    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    msg = &wifimsg->msg.initmsg;
    hif_info_rx = (struct hif_pipe_addl_info*)qdf_mem_malloc(sizeof(struct hif_pipe_addl_info));

    if (hif_info_rx == NULL) {
        qdf_print(" Unable to allocate memory : %s ", __func__);
        qdf_mem_free(wifimsg);
        return -1;
    }

    hif_info_tx = (struct hif_pipe_addl_info*)qdf_mem_malloc(sizeof(struct hif_pipe_addl_info));

    if (hif_info_tx == NULL) {
        qdf_print(" Unable to allocate memory : %s ", __func__);
        qdf_mem_free(hif_info_rx);
        qdf_mem_free(wifimsg);
        return -1;
    }

    msg->radio_id = soc->nss_soc.nss_wifiol_id;
    qdf_print("NSS %s :sc %pK NSS ID %d  Taret type %x ",__FUNCTION__, osc, soc->nss_soc.nss_wifiol_id, target_type);

    ifnum = soc->nss_soc.nss_sifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        qdf_mem_free(wifimsg);
        return -1;
    }

    memset(hif_info_rx, 0, sizeof(struct hif_pipe_addl_info));
    memset(hif_info_tx, 0, sizeof(struct hif_pipe_addl_info));

    hif_get_addl_pipe_info(osc, hif_info_tx, CE_HTT_TX_CE);
    hif_get_addl_pipe_info(osc, hif_info_rx, CE_HTT_MSG_CE);

    msg->pci_mem = hif_info_tx->pci_mem;

    msg->ce_tx_state.ctrl_addr = hif_info_tx->ctrl_addr;
    msg->ce_rx_state.ctrl_addr = hif_info_rx->ctrl_addr;

    update_ring_info(&msg->ce_tx_state.src_ring, &msg->ce_tx_state.dest_ring, hif_info_tx);
    update_ring_info(&msg->ce_rx_state.src_ring, &msg->ce_rx_state.dest_ring, hif_info_rx);

    qdf_mem_free(hif_info_tx);
    qdf_mem_free(hif_info_rx);
    if (target_type == TARGET_TYPE_QCA9984) {
        msg->target_type = NSS_WIFIOL_TARGET_TYPE_QCA9984;
    } else {
        msg->target_type = target_type;
    }

    /*
     * Register wifi with NSS
     */
    nss_wifiol_ctx = nss_register_wifi_if(ifnum,
            (nss_wifi_callback_t)osif_nss_ol_wifi_ce_callback_func,
            (nss_wifi_callback_t)osif_nss_ol_wifi_ext_callback_func,
            (nss_wifi_msg_callback_t)osif_nss_ol_event_receive,
             (struct net_device *)soc, features
             );

     if(nss_wifiol_ctx  == NULL){
         qdf_print("NSS-WifiOffload: NSS regsiter fail");
         qdf_mem_free(wifimsg);
         return -1;
     }

     /*
      * Enable new Data path for MU-MIMO
      */
     msg->mu_mimo_enhancement_en = 1;

     soc->nss_soc.nss_sctx = nss_wifiol_ctx;
     soc->nss_soc.dmadev = soc->qdf_dev->dev;
     qdf_print("NSS-WifiOffload: nss_wifiol_ctx %pK ",nss_wifiol_ctx);

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;
    init_completion(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete);

     /*
      * Send WIFI CE configure
      */
     nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_INIT_MSG,
             sizeof(struct nss_wifi_init_msg), msg_cb, NULL);

     status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
     qdf_mem_free(wifimsg);
     if (status != NSS_TX_SUCCESS) {
         qdf_print("NSS-WifiOffload: wifi initialization fail%d ", status);
         return -1;
     }

    /*
     * Blocking call, wait till we get ACK for this msg.
     */
    ret = wait_for_completion_timeout(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete, msecs_to_jiffies(OSIF_NSS_WIFI_CFG_TIMEOUT_MS));
    if (ret == 0) {
        qdf_print("Waiting for NSS WIFI INIT MSG ack timed out\n");
        return -1;
    }

    /*
     * ACK/NACK received from NSS FW
     */
    if (NSS_TX_FAILURE == soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response) {
        qdf_print("nack for NSS WIFI INIT MSG\n");
        return -1;
    }

    mdelay(100);
    return 0;
}

static void osif_nss_ol_post_recv_buffer(ol_ath_soc_softc_t *soc)
{

    struct hif_opaque_softc *sc;
    int ifnum  = -1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_init_msg *intmsg;
    nss_tx_status_t status;
    void *nss_wifiol_ctx;
    int radio_id;
    nss_wifi_msg_callback_t msg_cb;
    uint32_t ret;

    sc = lmac_get_ol_hif_hdl(soc->psoc_obj);
    nss_wifiol_ctx= soc->nss_soc.nss_sctx;
    if (!nss_wifiol_ctx) {
        return;
    }

    if (!sc) {
        qdf_print(" hif handle is NULL : %s ",__func__ );
        return;
    }

    radio_id = soc->nss_soc.nss_wifiol_id;

    qdf_print("%s %pK %pK %d ",__func__,nss_wifiol_ctx, sc, radio_id);

    ifnum = soc->nss_soc.nss_sifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return ;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return;
    }

    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    intmsg = &wifimsg->msg.initmsg;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;
    init_completion(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete);

    /*
     * Send WIFI Post Receive Message
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_POST_RECV_MSG,
            sizeof(struct nss_wifi_init_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI CE configure failed %d ", status);
        return;
    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     */
    ret = wait_for_completion_timeout(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete, msecs_to_jiffies(OSIF_NSS_WIFI_CFG_TIMEOUT_MS));
    if (ret == 0) {
        qdf_print("Waiting for NSS WIFI POST RECV MSG ack timed out\n");
        return;
    }

    /*
     * ACK/NACK received from NSS FW
     */
    if (NSS_TX_FAILURE == soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response) {
        qdf_print("nack for NSS WIFI POST RECV MSG\n");
        return;
    }

    /* CE_per_engine_disable_interupt((struct CE_handle *)CE_state); */
    hif_disable_interrupt(sc, CE_HTT_MSG_CE);
    mdelay(100);
}

#ifndef HTC_HEADER_LEN
#define HTC_HEADER_LEN 8
#endif
struct htc_frm_hdr_t {
    uint32_t   EndpointID : 8,
               Flags : 8,
               PayloadLen : 16;
    uint32_t   ControlBytes0 : 8,
               ControlBytes1 : 8,
               reserved : 16;
};

int osif_ol_nss_htc_send(struct ol_txrx_pdev_t *pdev, void *nss_wifiol_ctx, int nssid, qdf_nbuf_t netbuf, uint32_t transfer_id)
{

    struct htc_frm_hdr_t * htchdr;
    int status =0, len;

    if (!nss_wifiol_ctx) {
        return 0;
    }

    qdf_print("%s :NSS %pK NSS ID %d ",__FUNCTION__, nss_wifiol_ctx, nssid);

    len =  qdf_nbuf_len(netbuf);
    qdf_nbuf_push_head(netbuf, HTC_HEADER_LEN);
    netbuf->len += HTC_HEADER_LEN;
    htchdr = (struct htc_frm_hdr_t *)qdf_nbuf_data(netbuf);

    memset(htchdr, 0, HTC_HEADER_LEN);
    htchdr->EndpointID = transfer_id;
    htchdr->PayloadLen = len;

    status = osif_nss_ol_ce_raw_send(
            pdev,
            nss_wifiol_ctx,
            nssid,
            netbuf, len+HTC_HEADER_LEN);
    return status;
}


void
htt_t2h_msg_handler_fast(void *htt_pdev, qdf_nbuf_t *nbuf_cmpl_arr,
                         uint32_t num_cmpls);


int osif_nss_ol_htt_rx_init(void *htt_handle)
{
    struct htt_pdev_t *htt_pdev;
    void *nss_wifiol_ctx;
    int radio_id ;
    int ringsize;
    int fill_level;
    uint32_t paddrs_ringptr;
    uint32_t paddrs_ringpaddr;
    uint32_t alloc_idx_vaddr;
    uint32_t alloc_idx_paddr;
    int ifnum ;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_htt_init_msg *st ;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;
    ol_ath_soc_softc_t *soc;
    uint32_t ret;

    if (!htt_handle) {
        return 0;
    }


    htt_pdev = (struct htt_pdev_t *)htt_handle;
    soc = (ol_ath_soc_softc_t *)lmac_get_psoc_feature_ptr(
                (struct wlan_objmgr_psoc *)htt_pdev->ctrl_psoc);

    if (!soc) {
        return 0;
    }

    nss_wifiol_ctx = htt_pdev->nss_wifiol_ctx;
    if (!nss_wifiol_ctx) {
        return 0;
    }

    radio_id = htt_pdev->nss_wifiol_id;
    ringsize = htt_pdev->rx_ring.size;
    fill_level = htt_pdev->rx_ring.fill_level;
    paddrs_ringptr = (long)htt_pdev->rx_ring.buf.paddrs_ring;
    paddrs_ringpaddr = (uint32_t)htt_pdev->rx_ring.base_paddr;
    alloc_idx_vaddr = (long)htt_pdev->rx_ring.alloc_idx.vaddr;
    alloc_idx_paddr = (uint32_t)htt_pdev->rx_ring.alloc_idx.paddr;


    ifnum = ((struct ol_txrx_pdev_t *)htt_pdev->txrx_pdev)->nss_ifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return 1;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return 1;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    st = &wifimsg->msg.httinitmsg;

    st->radio_id = radio_id;
    st->ringsize         = ringsize;
    st->fill_level       = fill_level;
    st->paddrs_ringptr   = paddrs_ringptr;
    st->paddrs_ringpaddr = paddrs_ringpaddr;
    st->alloc_idx_vaddr  = alloc_idx_vaddr;
    st->alloc_idx_paddr  = alloc_idx_paddr;

    qdf_print("%s soc %pK:NSS %pK NSS ID %d ",__FUNCTION__, soc, nss_wifiol_ctx, radio_id);

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;
    init_completion(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete);
    /*
     * Send WIFI HTT Init
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_HTT_INIT_MSG,
            sizeof(struct nss_wifi_htt_init_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("NSS-WifiOffload: htt rx configure fail %d ", status);
        return 1;
    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     */
    ret = wait_for_completion_timeout(&soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete, msecs_to_jiffies(OSIF_NSS_WIFI_CFG_TIMEOUT_MS));
    if (ret == 0) {
        qdf_print("Waiting for htt rx init config msg ack timed out");
        return 1;
    }

    /*
     * ACK/NACK received from NSS FW
     */
    if (NSS_TX_FAILURE == soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response) {

        qdf_print("nack for htt_rx_init config msg");
        return 1;
    }

    qdf_print("NSS-WifiOffload: htt rx configure  delay 100ms start ");
    mdelay(100);
    return 0;
}

static int osif_nss_ol_ce_raw_send(struct ol_txrx_pdev_t *pdev, void *nss_wifiol_ctx,
        uint32_t radio_id, qdf_nbuf_t netbuf, uint32_t  len )
{
    int ifnum  = -1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_rawsend_msg *st ;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;

    qdf_print("%s :NSS %pK NSS ID %d ",__FUNCTION__, nss_wifiol_ctx, radio_id);

    ifnum = pdev->nss_ifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return 1;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return 1;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    st = &wifimsg->msg.rawmsg;

    st->len   = len;
    memcpy(st->array, netbuf->data, len);

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send WIFI Raw Send Message
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_RAW_SEND_MSG,
            sizeof(struct nss_wifi_rawsend_msg), msg_cb, NULL);

    qdf_nbuf_free(netbuf);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("htt config message failed %d ", status);
        return 1;
    }
    return 0;
}

static int osif_nss_ol_pdev_add_wds_peer(struct ol_ath_softc_net80211 *scn, uint8_t *peer_mac, uint8_t *dest_mac, uint8_t ast_type, struct cdp_peer *peer_handle)
{
    void *nss_wifiol_ctx;
    uint32_t radio_id;
    int ifnum = -1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_wds_peer_msg *st = NULL;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;
    struct ol_txrx_pdev_t *pdev;

    nss_wifiol_ctx = scn->nss_radio.nss_rctx;
    if (!nss_wifiol_ctx) {
        return 0;
    }

    ifnum = scn->nss_radio.nss_rifnum;
    if(ifnum == -1){
        qdf_print("%s: no if found return ",__FUNCTION__);
        return 1;
    }

    pdev = (struct ol_txrx_pdev_t *) wlan_pdev_get_dp_handle(scn->sc_pdev);
    if (!pdev) {
        return 0;
    }

    radio_id = pdev->nss_wifiol_id;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    ifnum = pdev->nss_ifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return 1;
    }

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Out of memory \n",__func__);
        return 1;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    st = &wifimsg->msg.pdevwdspeermsg;
    memcpy(st->peer_mac, peer_mac, 6);
    memcpy(st->dest_mac, dest_mac, 6);

    /*
     * Send peer add message
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_WDS_PEER_ADD_MSG,
            sizeof(struct nss_wifi_tx_init_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("WIFI CE configure failt %d ", status);
        return 1;
    }

    return 0;
}

static int osif_nss_ol_pdev_update_wds_peer(struct ol_ath_softc_net80211 *scn, uint8_t *peer_mac,
                                             uint8_t *dest_mac, uint8_t pdev_id)
{
    return 0;
}

/*
 * osif_nss_send_cmd()
 *	API for notifying the pdev related command to NSS.
 */
static void osif_nss_send_cmd(struct nss_ctx_instance *nss_wifiol_ctx, int32_t if_num,
        enum nss_wifi_cmd cmd, uint32_t value)

{
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_cmd_msg *wificmd;

    bool status;

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return;
    }
    wificmd = &wifimsg->msg.wcmdm;
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));

    wificmd->cmd = cmd;
    wificmd->value = value;
    nss_cmn_msg_init(&wifimsg->cm, if_num, NSS_WIFI_CMD_MSG,
            sizeof(struct nss_wifi_cmd_msg), NULL, NULL);

    /*
     * Send the pdev configure message down to NSS
     */
    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("Unable to send the pdev command message to NSS");
    }
}

/*
 * osif_nss_ol_set_cmd
 */
static void osif_nss_ol_set_cmd(struct ol_ath_softc_net80211 *scn, enum osif_nss_wifi_cmd osif_cmd)
{
    void *nss_wifiol_ctx;
    int ifnum;
    uint32_t cmd, val;
    struct ol_txrx_pdev_t *pdev;

    nss_wifiol_ctx = scn->nss_radio.nss_rctx;
    cmd = NSS_WIFI_MAX_CMD;

    if (!nss_wifiol_ctx) {
        return;
    }
    pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);
    ifnum = scn->nss_radio.nss_rifnum;
    if(ifnum == -1){
        qdf_print("%s: no if found return ",__FUNCTION__);
        return;
    }

    switch (osif_cmd) {
        case OSIF_NSS_WIFI_FILTER_NEIGH_PEERS_CMD:
            cmd = NSS_WIFI_FILTER_NEIGH_PEERS_CMD;
            val = pdev->filter_neighbour_peers;
            break;

        default:
            qdf_print("Command :%d is not supported in NSS", cmd);
            return;
    }

    osif_nss_send_cmd(nss_wifiol_ctx, ifnum, cmd, val);
}

static void osif_nss_ol_read_pkt_prehdr(struct ol_ath_softc_net80211 *scn, struct sk_buff *nbuf)
{
    return;
}

static int osif_nss_ol_pdev_del_wds_peer(struct ol_ath_softc_net80211 *scn, uint8_t *peer_mac, uint8_t *dest_mac, uint8_t ast_type, uint8_t pdev_id)
{
    void *nss_wifiol_ctx;
    uint32_t radio_id;
    int ifnum = -1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_wds_peer_msg *st;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;
    struct ol_txrx_pdev_t *pdev;

    nss_wifiol_ctx = scn->nss_radio.nss_rctx;
    if (!nss_wifiol_ctx) {
        return 0;
    }

    ifnum = scn->nss_radio.nss_rifnum;
    if(ifnum == -1){
        qdf_print("%s: no if found return ",__FUNCTION__);
        return 1;
    }

    pdev = (struct ol_txrx_pdev_t *) wlan_pdev_get_dp_handle(scn->sc_pdev);
    if (!pdev) {
        return 0;
    }

    radio_id = pdev->nss_wifiol_id;

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return 1;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    st = &wifimsg->msg.pdevwdspeermsg;
    memcpy(st->peer_mac, peer_mac, 6);
    memcpy(st->dest_mac, dest_mac, 6);

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    /*
     * Send peer del message
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_WDS_PEER_DEL_MSG,
            sizeof(struct nss_wifi_tx_init_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("NSS-WifOffoad: del wds fail %d ", status);
        return 1;
    }

    return 0;
}

void osif_nss_wifi_set_cfg(ol_ath_soc_softc_t *soc, uint32_t wifili_cfg)
{
    qdf_print("NSS-WifOffoad: cfg set (nocode) ");
}

static int osif_nss_wifi_detach(ol_ath_soc_softc_t *soc, uint32_t delay)
{
	preempt_disable();

	/* This should be renamed to soc instead of scn */
	if (soc->nss_soc.ops) {
		qdf_print("%s: nss wifi offload pause and delay %d ", __FUNCTION__, delay);
		osif_nss_ol_wifi_stop(soc);
		mdelay(delay);
		qdf_print("%s: nss wifi offload reset and delay %d ",__FUNCTION__, delay);
		osif_nss_ol_wifi_reset(soc, delay);
	}

	preempt_enable();
    return 0;
}

static int osif_nss_wifi_attach(ol_ath_soc_softc_t *soc)
{
    qdf_print("NSS-WifOffoad: wifi attach (nocode) ");
    return 0;
}

static int osif_nss_wifi_map_wds_peer(ol_ath_soc_softc_t *soc, uint16_t peer_id, uint16_t hw_ast_idx, uint8_t vdev_id, uint8_t *mac)
{
    qdf_print("NSS-WifOffoad: wifi wds peer map (nocode) ");
    return 0;
}

static void osif_nss_wifi_nawds_enable(ol_ath_soc_softc_t *soc, void *peer, uint16_t is_nawds)
{
    qdf_print("NSS-WifOffoad: wifi set nawds (nocode) ");
    return;
}

static void osif_nss_wifi_desc_pool_free(ol_ath_soc_softc_t *soc)
{
    qdf_print("NSS-WifOffoad: Free desc Pool (nocode) ");
    return;
}

/*
 * osif_nss_ol_set_peer_security_info()
 *      Set the security per peer
 */
static int osif_nss_ol_set_peer_security_info(struct ol_ath_softc_net80211 *scn, void *peer_handler,
                                                       uint8_t pkt_type, uint8_t cipher_type, uint8_t mic_key[])
{
    return 0;
}

/*
 * osif_nss_ol_set_hmmc_dscp_override()
 *      Set HMMC DSCP override
 */
static void osif_nss_ol_set_hmmc_dscp_override(struct ol_ath_softc_net80211 *scn, uint8_t value)
{
    return;
}

/*
 * osif_nss_ol_set_hmmc_dscp_tid()
 *      Set HMMC DSCP TID value
 */
static void osif_nss_ol_set_hmmc_dscp_tid(struct ol_ath_softc_net80211 *scn, uint8_t value)
{
    return;
}

/*
 * osif_nss_ol_v3_stats_enable()
 *      Set v3 stats flag
 */
static int osif_nss_ol_v3_stats_enable(struct ol_ath_softc_net80211 *scn, uint32_t flag)
{
    return 0;
}

#ifdef WLAN_FEATURE_FASTPATH
static struct nss_wifi_radio_ops nss_wifi_funcs_radio = {
#ifdef WDS_VENDOR_EXTENSION
    osif_nss_ol_wds_extn_peer_cfg_send,
#endif
    osif_nss_ol_set_primary_radio,
    osif_nss_ol_set_always_primary,
    osif_nss_ol_set_force_client_mcast_traffic,
    osif_nss_ol_set_perpkt_txstats,
    osif_nss_ol_set_igmpmld_override_tos,
    osif_nss_ol_stats_cfg,
    osif_nss_ol_enable_ol_statsv2,
    osif_nss_ol_pktlog_cfg,
    osif_nss_tx_queue_cfg,
    osif_nss_tx_queue_min_threshold_cfg,
    osif_nss_ol_wifi_tx_capture_set,
    osif_nss_ol_pdev_add_wds_peer,
    osif_nss_ol_pdev_del_wds_peer,
    osif_nss_ol_enable_dbdc_process,
    osif_nss_ol_set_cmd,
    osif_nss_ol_read_pkt_prehdr,
    osif_nss_ol_set_drop_secondary_mcast,
    osif_nss_ol_set_peer_security_info,
    osif_nss_ol_set_hmmc_dscp_override,
    osif_nss_ol_set_hmmc_dscp_tid,
    osif_nss_ol_v3_stats_enable,
    osif_nss_ol_pdev_update_wds_peer,
};

struct nss_vdev_ops nss_wifi_funcs_be = {
#if DBDC_REPEATER_SUPPORT
    osif_nss_ol_store_other_pdev_stavap,
#endif
    osif_nss_vdev_me_reset_snooplist,
    osif_nss_vdev_me_update_member_list,
    osif_nss_ol_vap_xmit,
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    osif_nss_vdev_me_update_hifitlb,
#endif
    osif_nss_vdev_me_dump_denylist,
    osif_nss_vdev_me_add_deny_member,
    osif_nss_vdev_set_cfg,
    osif_nss_vdev_process_mpsta_tx,
    osif_nss_ol_wifi_monitor_set_filter,
    osif_nss_vdev_get_nss_id,
    osif_nss_vdev_process_extap_tx,
    osif_nss_vdev_me_dump_snooplist,
    osif_nss_ol_vap_delete,
    osif_nss_vdev_me_add_member_list,
    osif_nss_vdev_vow_dbg_cfg,
    osif_nss_ol_enable_dbdc_process,
    osif_nss_vdev_set_wifiol_ctx,
    osif_nss_vdev_me_delete_grp_list,
    osif_nss_vdev_me_create_grp_list,
    osif_nss_vdev_me_delete_deny_list,
    osif_nss_vdev_me_remove_member_list,
    osif_nss_vdev_alloc,
    osif_nss_ol_vap_create,
    osif_nss_be_vap_updchdhdr,
    osif_nss_vdev_set_dscp_tid_map,
    osif_nss_vdev_set_dscp_tid_map_id,
    osif_nss_ol_vap_up,
    osif_nss_ol_vap_down,
    osif_nss_vdev_set_inspection_mode,
    osif_nss_vdev_extap_table_entry_add,
    osif_nss_vdev_extap_table_entry_del,
    osif_nss_vdev_psta_add_entry,
    osif_nss_vdev_psta_delete_entry,
    osif_nss_vdev_qwrap_isolation_enable,
    osif_nss_vdev_set_read_rxprehdr,
    osif_nss_be_vdev_set_peer_nexthop,
    osif_nss_vdev_update_vlan,
    osif_nss_vdev_set_vlan_type,
};

static struct nss_wifi_internal_ops nss_wifi_funcs_ol_be = {
    osif_nss_ol_be_vdev_set_cfg,
    osif_nss_ol_be_get_peerid,
    osif_nss_ol_be_peerid_find_hash_find,
    osif_nss_ol_be_get_vdevid_fromosif,
    osif_nss_ol_be_get_vdevid_fromvdev,
    osif_nss_ol_be_find_pstosif_by_id,
    osif_nss_ol_be_vdevcfg_set_offload_params,
    osif_nss_ol_be_vdev_handle_monitor_mode,
    osif_nss_ol_be_vdev_tx_inspect_handler,
    osif_nss_ol_be_vdev_data_receive_meshmode_rxinfo,
    osif_nss_ol_be_find_peer_by_id,
    osif_nss_ol_vdev_handle_rawbuf,
    osif_nss_ol_be_vdev_call_monitor_mode,
    osif_nss_ol_be_vdev_get_stats,
    osif_nss_ol_be_vdev_spl_receive_exttx_compl,
    osif_nss_ol_be_vdev_spl_receive_ext_mesh,
    osif_nss_ol_be_vdev_spl_receive_ext_wdsdata,
    osif_nss_ol_be_vdev_qwrap_mec_check,
    osif_nss_ol_be_vdev_spl_receive_ppdu_metadata,
#if ATH_SUPPORT_WRAP
    osif_nss_ol_be_vdev_get_nss_qwrap_en,
#else
    NULL,
#endif
#if WDS_VENDOR_EXTENSION
    osif_nss_ol_be_vdev_get_wds_peer,
#endif
    osif_nss_ol_be_vdev_txinfo_handler,
    osif_nss_ol_be_vdev_update_statsv2,
    osif_nss_ol_be_vdev_data_receive_mec_check,
    osif_nss_ol_be_peer_find_hash_find,
};
#endif

#ifdef WLAN_FEATURE_FASTPATH
int osif_nss_ol_pdev_attach(struct ol_ath_softc_net80211 *scn, void *nss_wifiol_ctx,
        int radio_id,
        uint32_t desc_pool_size,
        uint32_t *tx_desc_array,
        uint32_t wlanextdesc_addr,
        uint32_t wlanextdesc_size,
        uint32_t htt_tx_desc_base_vaddr, uint32_t htt_tx_desc_base_paddr ,
        uint32_t htt_tx_desc_offset,
        uint32_t pmap_addr)
{
    int ifnum = -1;
    struct nss_wifi_msg *wifimsg = NULL;
    struct nss_wifi_tx_init_msg *st ;
    nss_tx_status_t status;
    nss_wifi_msg_callback_t msg_cb;
    uint32_t ret;
    int mem_send = -1;
    struct htt_pdev_t *htt_pdev = scn->soc->htt_handle;
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)htt_pdev->txrx_pdev;
    struct ieee80211com *ic = &scn->sc_ic;

    if (!nss_wifiol_ctx) {
        return 0;
    }

    qdf_print("%s :NSS %pK NSS ID %d  target type  ",__FUNCTION__, nss_wifiol_ctx, radio_id);

    ifnum = scn->soc->nss_soc.nss_sifnum;
    if (ifnum == -1) {
        qdf_print("%s: no if found return ",__FUNCTION__);
        return -1;
    }

    if (scn->soc->nss_soc.nss_wifiol_ce_enabled != 1) {
        qdf_print(" CE init was unsuccesful ");
        return -1;
    }

    scn->nss_radio.nss_nxthop = scn->soc->nss_soc.nss_nxthop;
    scn->enable_statsv2 = 0;

    wifimsg = qdf_mem_malloc(sizeof(struct nss_wifi_msg));
    if (!wifimsg) {
        qdf_print("%s: Failed to allocate memory \n",__func__);
        return -1;
    }
    memset(wifimsg, 0, sizeof(struct nss_wifi_msg));
    st = &wifimsg->msg.pdevtxinitmsg;

    st->desc_pool_size   = desc_pool_size;
    st->tx_desc_array = 0;
    st->wlanextdesc_addr = wlanextdesc_addr;
    st->wlanextdesc_size = wlanextdesc_size;
    st->htt_tx_desc_base_vaddr = htt_tx_desc_base_vaddr;
    st->htt_tx_desc_base_paddr = htt_tx_desc_base_paddr;
    st->htt_tx_desc_offset = htt_tx_desc_offset;
    st->pmap_addr = pmap_addr;

    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;

    // Register the nss related functions
    ic->nss_vops = &nss_wifi_funcs_be;
    ic->nss_iops = &nss_wifi_funcs_ol_be;
    ic->nss_radio_ops = &nss_wifi_funcs_radio;
    osif_register_dev_ops_xmit(osif_nss_ol_vap_hardstart, OSIF_NETDEV_TYPE_NSS_WIFIOL);
    msg_cb = (nss_wifi_msg_callback_t)osif_nss_ol_event_receive;
    init_completion(&scn->soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete);

    /*
     * Send WIFI CE configure
     */
    nss_cmn_msg_init(&wifimsg->cm, ifnum, NSS_WIFI_TX_INIT_MSG,
            sizeof(struct nss_wifi_tx_init_msg), msg_cb, NULL);

    status = nss_wifi_tx_msg(nss_wifiol_ctx, wifimsg);
    qdf_mem_free(wifimsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("NSS-WifiOffload: pdev init fail %d ", status);
        return -1;
    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     */
    ret = wait_for_completion_timeout(&scn->soc->nss_soc.osif_nss_ol_wifi_cfg_complete.complete, msecs_to_jiffies(OSIF_NSS_WIFI_CFG_TIMEOUT_MS));
    if (ret == 0) {
        qdf_print("Waiting for peer stats req msg ack timed out %d\n", radio_id);
        return 1;
    }

    /*
     * ACK/NACK received from NSS FW
     */
    if (NSS_TX_FAILURE == scn->soc->nss_soc.osif_nss_ol_wifi_cfg_complete.response) {
        qdf_print("NACK for peer_stats_send request from NSS FW %d\n", radio_id);
        return 1;
    }

    pdev->last_rra_pool_used = -1;
    pdev->last_peer_pool_used = -1;
    pdev->max_peers_per_pool = 67;
    pdev->max_rra_per_pool = 67;

    qdf_print("sending peer pool memory");
    mem_send = osif_nss_ol_send_peer_memory(scn);
    if (mem_send != 0) {
        qdf_print("Not able to send memory for peer to NSS");
        return -1;
    }
    mdelay(100);

    qdf_print("sending rra pool memory");
    mem_send = osif_nss_ol_send_rra_memory(scn);
    if (mem_send != 0) {
        qdf_print("Not able to send memory for rx reorder array to NSS");
        return -1;
    }
    mdelay(100);

    if (scn->soc->nss_soc.nss_wifiol_id == 0) {
        osif_nss_ol_set_primary_radio(scn, 1);
    } else {
        osif_nss_ol_set_primary_radio(scn, 0);
    }

    return 0;
}
#else
int osif_nss_ol_pdev_attach(struct ol_ath_softc_net80211 *scn, void *nss_wifiol_ctx,
        int radio_id,
        uint32_t desc_pool_size,
        uint32_t *tx_desc_array,
        uint32_t wlanextdesc_addr,
        uint32_t wlanextdesc_size,
        uint32_t htt_tx_desc_base_vaddr, uint32_t htt_tx_desc_base_paddr ,
        uint32_t htt_tx_desc_offset,
        uint32_t pmap_addr)
{
   qdf_print("Error: Fastpath needs to be enabled for Wi-Fi Offload ");
   return 0;
}
#endif

struct nss_wifi_soc_ops nss_wifibe_soc_ops = {
    osif_nss_wifi_peer_delete,
    osif_nss_wifi_peer_create,
    osif_nss_ol_wifi_init,
    osif_nss_ol_post_recv_buffer,
    osif_nss_ol_ce_flush_raw_send,
    osif_nss_wifi_set_cfg,
    osif_nss_wifi_detach,
    osif_nss_wifi_attach,
    osif_nss_wifi_map_wds_peer,
    osif_nss_ol_wifi_stop,
    osif_nss_wifi_nawds_enable,
    osif_nss_wifi_set_default_nexthop,
    osif_nss_wifi_desc_pool_free,
};


