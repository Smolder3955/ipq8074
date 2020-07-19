/*
 * Copyright (c) 2015-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2015-2016 Qualcomm Atheros, Inc.
 *
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
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

#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/in.h>
#include <asm/cacheflush.h>
#include "osif_private.h"
#include <nss_api_if.h>
#include <nss_cmn.h>
#include <qdf_nbuf.h>
#include "ol_if_athvar.h"


#include "osif_private.h"
#include "osif_nss_wifiol_vdev_if.h"
#include <ar_internal.h>
#include "dp_htt.h"
#include "dp_types.h"
#include "dp_internal.h"
#include "dp_rx.h"
#include "dp_peer.h"

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#endif

#include <if_meta_hdr.h>
#if ATH_SUPPORT_WRAP
#include "osif_wrap_private.h"
extern osif_dev * osif_wrap_wdev_vma_find(struct wrap_devt *wdt,unsigned char *mac);
#endif

#if ATH_DATA_RX_INFO_EN || MESH_MODE_SUPPORT
u_int32_t osif_rx_status_dump(void* rs);
#endif

#define ETHERNET_HDR_LEN sizeof(struct ether_header)

extern struct osif_nss_vdev_cfg_pvt osif_nss_vdev_cfgp;

/*
 * This file is responsible for interacting with qca-nss-drv's
 * WIFI to manage WIFI VDEVs.
 *
 * This driver also exposes few APIs which can be used by
 * another module to perform operations on CAPWAP tunnels. However, we create
 * one netdevice for all the CAPWAP tunnels which is done at the module's
 * init time if NSS_wifimgr_ONE_NETDEV is set in the Makefile.
 *
 * If your requirement is to create one netdevice per-CAPWAP tunnel, then
 * netdevice needs to be created before CAPWAP tunnel create. Netdevice are
 * created using nss_wifimgr_netdev_create() API.
 *
 */
#define OSIF_NSS_DEBUG_LEVEL 1

/*
 * NSS WiFi offload debug macros
 */
#if (OSIF_NSS_DEBUG_LEVEL < 1)
#define osif_nss_assert(fmt, args...)
#else
#define osif_nss_assert(c) if (!(c)) { BUG_ON(!(c)); }
#endif /* OSIF_NSS_DEBUG_LEVEL */

/*
 * Compile messages for dynamic enable/disable
 */
#if !defined(CONFIG_DYNAMIC_DEBUG)
#define osif_nss_warn(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define osif_nss_info(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define osif_nss_trace(s, ...) pr_debug("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#else /* CONFIG_DYNAMIC_DEBUG */
/*
 * Statically compile messages at different levels
 */
#if (OSIF_NSS_DEBUG_LEVEL < 2)
#define osif_nss_warn(s, ...)
#else
#define osif_nss_warn(s, ...) pr_warn("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (OSIF_NSS_DEBUG_LEVEL < 3)
#define osif_nss_info(s, ...)
#else
#define osif_nss_info(s, ...)   pr_notice("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif

#if (OSIF_NSS_DEBUG_LEVEL < 4)
#define osif_nss_trace(s, ...)
#else
#define osif_nss_trace(s, ...)  pr_info("%s[%d]:" s, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#endif
#endif /* CONFIG_DYNAMIC_DEBUG */

/*
 * TODO: Currently function dp_peer_find_hash_find is defined
 * as static inside dp_peer.c. We have to decide upon a way to expose
 * this function here.
 */
extern struct dp_peer *dp_peer_find_hash_find(struct dp_soc *soc,
        uint8_t *peer_mac_addr, int mac_addr_is_aligned, uint8_t vdev_id);

#if DBDC_REPEATER_SUPPORT
int dbdc_rx_process (os_if_t *osif ,struct net_device **dev ,wlan_if_t vap, struct sk_buff *skb, int *nwifi);
int dbdc_tx_process (wlan_if_t vap, osif_dev **osdev , struct sk_buff *skb);
#endif

/*
 * osif_nss_wifili_vdev_update_statsv2()
 * Update statsv2 per ppdu
 */
void osif_nss_wifili_vdev_update_statsv2(struct ol_ath_softc_net80211 *scn, struct sk_buff * nbuf, void *rx_mpdu_desc, uint8_t htt_rx_status)
{
    /* NA */
}

/*
 * osif_nss_wifili_vdev_txinfo_handler()
 * Handler for tx info packets exceptioned from WIFI
 */
void osif_nss_wifili_vdev_txinfo_handler(struct ol_ath_softc_net80211 *scn, struct sk_buff *skb, struct nss_wifi_vdev_per_packet_metadata *wifi_metadata, bool is_raw)
{
	/* NA */
}

/*
 * osif_nss_wifili_vdev_tx_inspect_handler()
 *	Handler for tx inspect packets exceptioned from WIFI
 */
void osif_nss_wifili_vdev_tx_inspect_handler(void *vdev_handle, struct sk_buff *skb)
{
    struct dp_peer *peer;
    struct sk_buff *skb_copy;
    uint16_t peer_id = HTT_INVALID_PEER;
    struct dp_vdev *vdev = (struct dp_vdev *)vdev_handle;

    if (vdev->osif_proxy_arp(vdev->osif_vdev, skb)) {
        goto out;
    }

    TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
        if (peer && (peer->peer_ids[0] != HTT_INVALID_PEER) && (peer->bss_peer)) {
            peer_id = peer->peer_ids[0];
            if (peer_id == HTT_INVALID_PEER) {
                goto out;
            }

            skb_copy = qdf_nbuf_copy(skb);
            if (skb_copy) {
                qdf_nbuf_reset_ctxt(skb_copy);
                osif_nss_vdev_peer_tx_buf(vdev->osif_vdev, skb_copy, peer_id);
            }
        }
    }

out:
    qdf_nbuf_free(skb);
}

/*
 * osif_nss_wifili_vdev_handle_monitor_mode()
 *	handle monitor mode, returns false if packet is consumed
 */
bool osif_nss_wifili_vdev_handle_monitor_mode(struct net_device *netdev, struct sk_buff *skb, uint8_t is_chain)
{
    /*
     * Not required for 8074 as of now.
     *  Decided as per internal discussions
     */
    return true;
}


/*
 * osif_nss_wifili_vdev_spl_receive_exttx_compl()
 *  Handler for data packets exceptioned from WIFI
 */
/*
 * Li currently does not have special data receive implemented
 * TODO: Implement this for 8074 later when needed
 */
void osif_nss_wifili_vdev_spl_receive_exttx_compl(struct net_device *dev, struct sk_buff *skb, struct nss_wifi_vdev_tx_compl_metadata *tx_compl_metadata)
{
    return;
}

#if MESH_MODE_SUPPORT
extern void os_if_tx_free_ext(struct sk_buff *skb);
#endif

/*
 * osif_nss_wifili_vdev_spl_receive_ext_mesh()
 *  Handler for EXT Mesh data packets exceptioned from WIFI
 */
void osif_nss_wifili_vdev_spl_receive_ext_mesh(struct net_device *dev, struct sk_buff *skb, struct nss_wifi_vdev_mesh_per_packet_metadata *mesh_metadata)
{
    struct dp_pdev *pdev;
    struct dp_vdev *vdev;
    osif_dev *osifp;
    struct ieee80211vap *vap = NULL;
    struct ieee80211com* ic = NULL;
#if MESH_MODE_SUPPORT
    struct meta_hdr_s *mhdr = NULL;
#endif
    osifp = netdev_priv(dev);
    vdev = wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    pdev = vdev->pdev;
    vap = osifp->os_if;
    ic = vap->iv_ic;

#if MESH_MODE_SUPPORT
    if (qdf_nbuf_headroom(skb) < sizeof(struct meta_hdr_s)) {
        qdf_print("Unable to accomodate mesh mode meta header");
        qdf_nbuf_free(skb);
        return;
    }

    qdf_nbuf_push_head(skb, sizeof(struct meta_hdr_s));

    mhdr = (struct meta_hdr_s *)qdf_nbuf_data(skb);
    mhdr->rssi =  mesh_metadata->rssi;
    mhdr->retries = mesh_metadata->tx_retries;
    mhdr->channel = pdev->operating_channel;
    os_if_tx_free_ext(skb);
#else
    qdf_nbuf_free(skb);
#endif
    return;
}

/*
 * osif_nss_ol_li_vdev_spl_receive_ext_wdsdata
 * 	WDS special data receive
 */
bool osif_nss_ol_li_vdev_spl_receive_ext_wdsdata(struct net_device *dev, struct sk_buff *nbuf, struct nss_wifi_vdev_wds_per_packet_metadata *wds_metadata)
{
    struct net_device *netdev;
    osif_dev  *osdev;
    struct dp_vdev *vdev = NULL;
    struct ieee80211vap *vap = NULL;
    struct dp_pdev *pdev = NULL;
    struct dp_soc *soc = NULL;
    uint32_t flags = IEEE80211_NODE_F_WDS_HM;
    int ret = 0;
    uint8_t wds_src_mac[IEEE80211_ADDR_LEN];
    uint8_t dest_mac[IEEE80211_ADDR_LEN];
    uint8_t sa_is_valid = 0, addr4_valid = 0;
    uint16_t peer_id;
    struct dp_peer *ta_peer = NULL;
    struct dp_peer *sa_peer = NULL;
    enum wifi_vdev_ext_wds_info_type wds_type;
    uint8_t *tx_status;
    struct dp_ast_entry *ast_entry = NULL;
    struct dp_neighbour_peer *neighbour_peer = NULL;
#if ATH_SUPPORT_WRAP
    osif_dev *psta_osdev = NULL;
    uint8_t mac_addr[IEEE80211_ADDR_LEN];
    uint8_t i = 0;
#endif
    /*
     * Need to move this code to wifi driver
     */
    if(dev == NULL) {
        qdf_print(KERN_CRIT "%s , netdev is NULL, freeing skb", __func__);
        return false;
    }

    netdev = (struct net_device *)dev;

    osdev = ath_netdev_priv(netdev);
    vap = osdev->os_if;
    vdev = wlan_vdev_get_dp_handle(osdev->ctrl_vdev);
    pdev = vdev->pdev;
    soc = pdev->soc;

    sa_is_valid = wds_metadata->is_sa_valid;
    peer_id = wds_metadata->peer_id;
    ta_peer = dp_peer_find_by_id(pdev->soc, peer_id);
    wds_type = wds_metadata->wds_type;
    addr4_valid = wds_metadata->addr4_valid;

    switch (wds_type) {
        case NSS_WIFI_VDEV_WDS_TYPE_RX:
            if (!ta_peer) {
                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_WARN,
                        "Unable to find peer %d", peer_id);
                return false;
            }

            memcpy(wds_src_mac, (qdf_nbuf_data(nbuf) + IEEE80211_ADDR_LEN),
                    IEEE80211_ADDR_LEN);

            qdf_spin_lock_bh(&soc->ast_lock);

            if (soc->ast_override_support) {
                ast_entry = dp_peer_ast_hash_find_by_pdevid(soc, wds_src_mac, pdev->pdev_id);
            } else {
                ast_entry = dp_peer_ast_hash_find_soc(soc, wds_src_mac);
            }

            if (ast_entry) {
                /*
                 * If WDS update is coming back on same peer it indicates that it is not roamed
                 * This situation can happen if a MEC packet reached in Rx direction even before the
                 * ast entry installation in happend in HW
                 */
                if ((ast_entry->peer == ta_peer) && (vdev->opmode == wlan_op_mode_sta)) {
                    qdf_spin_unlock_bh(&soc->ast_lock);
                    return false;
                }

                if ((ast_entry->type == CDP_TXRX_AST_TYPE_WDS_HM)
                        || (ast_entry->type == CDP_TXRX_AST_TYPE_WDS_HM_SEC)) {
                    qdf_spin_unlock_bh(&soc->ast_lock);
                    return false;
                }

                if (!sa_is_valid) {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                            "Not expected sa invalid with AST present for WDS mac:%pM\n", wds_src_mac);
                    sa_is_valid = 1;
                }
            }

            qdf_spin_unlock_bh(&soc->ast_lock);

            /*
             * add/update the wds entries only 4addr valid frames
             */
            if (addr4_valid) {
                if (!sa_is_valid) {
                    ret = dp_peer_add_ast(soc, ta_peer, wds_src_mac, CDP_TXRX_AST_TYPE_WDS, flags);
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                            "WDS Source port learn path ADD at peer id:%d for WDS mac:%pM\n", peer_id, wds_src_mac);
                    if (ret == -1) {
                        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                                "WDS ADD command to NSS failed for mac:%pM at peer_id: %d\n", wds_src_mac, peer_id);
                    }
                } else {
                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                            "WDS Source port learn path update at peer id:%d for WDS mac:%pM\n", peer_id, wds_src_mac);
                    if (!ast_entry) {
                        if (!soc->ast_override_support) {
                            ret = dp_peer_add_ast(soc, ta_peer, wds_src_mac, CDP_TXRX_AST_TYPE_WDS, flags);
                            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                                    "WDS Source port learn path ADD at peer id:%d for WDS mac:%pM\n", peer_id, wds_src_mac);
                            if (ret == -1) {
                                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                                        "WDS ADD command to NSS failed for mac:%pM at peer_id: %d\n", wds_src_mac, peer_id);
                            }
                        } else {

                            /*
                             * In HKv2 smart monitor case, when NAC client is
                             * added first and this client roams within BSS to
                             * connect to RE, since we have an AST entry for
                             * NAC we get sa_is_valid bit set. So we check if
                             * smart monitor is enabled and send add_ast command
                             * to FW.
                             */
                             if (pdev->neighbour_peers_added) {
                                 qdf_spin_lock_bh(&pdev->neighbour_peer_mutex);
                                 TAILQ_FOREACH(neighbour_peer, &pdev->neighbour_peers_list,
                                         neighbour_peer_list_elem) {
                                     if (!qdf_mem_cmp(&neighbour_peer->neighbour_peers_macaddr,
                                             wds_src_mac, QDF_MAC_ADDR_SIZE)) {
                                         ret = dp_peer_add_ast(soc, ta_peer, wds_src_mac, CDP_TXRX_AST_TYPE_WDS, flags);
                                         QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO, "sa valid and nac roamed to wds");
                                         break;
                                     }
                                 }
                                 qdf_spin_unlock_bh(&pdev->neighbour_peer_mutex);
                             }
                        }
                        return false;
                    }

                    /*
                     * Update ast entry only if its not STATIC or SELF
                     */
                    if ((ast_entry->type != CDP_TXRX_AST_TYPE_SELF) && (ast_entry->type != CDP_TXRX_AST_TYPE_STATIC) &&
                            (ast_entry->type != CDP_TXRX_AST_TYPE_STA_BSS)) {

                        if (ast_entry->pdev_id != ta_peer->vdev->pdev->pdev_id) {
                            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                                    "WDS Source port learn path DEL/ADD at peer id:%d for WDS mac:%pM for pdev-id %d",
                                    peer_id, wds_src_mac, ta_peer->vdev->pdev->pdev_id);
                            qdf_spin_lock_bh(&soc->ast_lock);
                            dp_peer_del_ast(soc, ast_entry);
                            qdf_spin_unlock_bh(&soc->ast_lock);
                        } else {
                            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                                    "WDS Source port learn path UPDATE at peer id:%d for WDS mac:%pM for pdev-id %d",
                                    peer_id, wds_src_mac, ta_peer->vdev->pdev->pdev_id);
                            qdf_spin_lock_bh(&soc->ast_lock);
                            ret = dp_peer_update_ast(soc, ta_peer, ast_entry, flags);
                            qdf_spin_unlock_bh(&soc->ast_lock);
                            if (ret == -1) {
                                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                                        "WDS update command to NSS failed for mac:%pM at peer_id: %d\n", wds_src_mac, peer_id);
                            }
                        }
                        return false;
                    }

                    /*
                     * Do not kickout STA if it belongs to a different radio.
                     * For DBDC repeater, it is possible to arrive here
                     * for multicast loopback frames originated from connected
                     * clients and looped back (intrabss) by Root AP
                     */
                     if (ast_entry->pdev_id != ta_peer->vdev->pdev->pdev_id) {
                         return false;
                     }

                     /*
                      * Kickout, when direct associated peer(SA) roams
                      * to another AP and reachable via TA peer
                      */
                     if (ast_entry->type == CDP_TXRX_AST_TYPE_STATIC) {
                         sa_peer = ast_entry->peer;
			 if (!sa_peer)
                             return false;

                         if ((sa_peer->vdev->opmode == wlan_op_mode_ap) &&
                             !sa_peer->delete_in_progress) {
                             sa_peer->delete_in_progress = true;
                             if (soc->cdp_soc.ol_ops->peer_sta_kickout) {
                                 soc->cdp_soc.ol_ops->peer_sta_kickout(sa_peer->vdev->pdev->ctrl_pdev,
                                                                   wds_src_mac);
                             }
                         }
		     }
                }
            } else {
                /*
                 * For AP mode : Do wds source port learning only if it is a 4-address mpdu.
                 * For STA mode : Frames from RootAP backend will be in 3-address mode,
                 * till RootAP does the WDS source port learning; Hence in repeater/STA
                 * mode, we enable learning even in 3-address mode , to avoid RootAP
                 * backbone getting wrongly learnt as MEC on repeater.
                 */
                if (vdev->opmode == wlan_op_mode_sta) {
                    if (!sa_is_valid) {
                        ret = dp_peer_add_ast(soc, ta_peer, wds_src_mac, CDP_TXRX_AST_TYPE_WDS, flags);
                        QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                                "STA 3 addr WDS Source port learn path ADD at peer id:%d for WDS mac:%pM\n", peer_id, wds_src_mac);
                        if (ret == -1) {
                            QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                                    "STA 3 addr WDS ADD command to NSS failed for mac:%pM at peer_id: %d\n", wds_src_mac, peer_id);
                        }
                    } else {
                        if (!ast_entry) {
                            if (!soc->ast_override_support) {
                                ret = dp_peer_add_ast(soc, ta_peer, wds_src_mac, CDP_TXRX_AST_TYPE_WDS, flags);
                                QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                                        "WDS Source port learn path ADD at peer id:%d for WDS mac:%pM\n", peer_id, wds_src_mac);
                                if (ret == -1) {
                                    QDF_TRACE(QDF_MODULE_ID_NSS, QDF_TRACE_LEVEL_INFO,
                                            "WDS ADD command to NSS failed for mac:%pM at peer_id: %d\n", wds_src_mac, peer_id);
                                }
                            }
                            return false;
                        }
                    }
                }
            }
            return false;

        case NSS_WIFI_VDEV_WDS_TYPE_MEC:
            /*
             * Need to free the buffer here
             */
            tx_status = (uint8_t *)qdf_nbuf_data(nbuf);
#if ATH_SUPPORT_WRAP
            if (vap->iv_mpsta) {
                for (i = 0; i < IEEE80211_ADDR_LEN; i++) {
                    mac_addr[(IEEE80211_ADDR_LEN - 1) - i] = tx_status[(IEEE80211_ADDR_LEN - 2) + i];
                }

                /* Mpsta vap here, find the correct tx vap from the wrap common based on src address */
                psta_osdev = osif_wrap_wdev_vma_find(&vap->iv_ic->ic_wrap_com->wc_devt, mac_addr);
                if (psta_osdev) {
                    vdev = (struct dp_vdev *)wlan_vdev_get_dp_handle(
                                      psta_osdev->ctrl_vdev);
                }
            }
#endif
            dp_tx_mec_handler(vdev, tx_status);
            break;

        case NSS_WIFI_VDEV_WDS_TYPE_DA:

            if (!ta_peer) {
                qdf_print("Unable to find peer %d", peer_id);
                break;
            }

            /* Donot add AST type DA if DA was is not enabled.*/
            if (!soc->da_war_enabled) {
                break;
            }

            memcpy(dest_mac, qdf_nbuf_data(nbuf), IEEE80211_ADDR_LEN);

            /*
             * Add ast for destination address
             */
            dp_peer_add_ast(soc, ta_peer, dest_mac, CDP_TXRX_AST_TYPE_DA, IEEE80211_NODE_F_WDS_HM);
            break;

        case NSS_WIFI_VDEV_WDS_TYPE_NONE:
            qdf_print("WDS Source port learn path invalid type %d", peer_id);
            break;
    }

    return false;
}

/*
 * osif_nss_ol_li_vdev_spl_receive_ppdu_metadata
 * 	PPDU meta data.
 */
bool osif_nss_ol_li_vdev_spl_receive_ppdu_metadata(struct net_device *dev, struct sk_buff *nbuf, struct nss_wifi_vdev_ppdu_metadata *ppdu_mdata)
{
    struct net_device *netdev;
    osif_dev  *osdev;
    struct dp_vdev *vdev = NULL;
    struct dp_pdev *pdev = NULL;
    struct dp_soc *soc = NULL;
    struct dp_peer *peer = NULL;
    struct hal_tx_completion_status ts;

    /*
     * Need to move this code to wifi driver
     */
    if(dev == NULL) {
        qdf_print(KERN_CRIT "%s , netdev is NULL, freeing skb", __func__);
        return false;
    }

    netdev = (struct net_device *)dev;
    osdev = ath_netdev_priv(netdev);

    vdev = wlan_vdev_get_dp_handle(osdev->ctrl_vdev);
    pdev = vdev->pdev;
    soc = pdev->soc;
    peer = (ppdu_mdata->peer_id == HTT_INVALID_PEER) ? NULL : dp_peer_find_by_id(soc, ppdu_mdata->peer_id);

    ts.ppdu_id = ppdu_mdata->ppdu_id;
    ts.peer_id = ppdu_mdata->peer_id;
    ts.first_msdu = ppdu_mdata->first_msdu;
    ts.last_msdu = ppdu_mdata->last_msdu;

    if (dp_get_completion_indication_for_stack(soc, pdev, peer, &ts, nbuf, 0) == QDF_STATUS_SUCCESS) {
        dp_send_completion_to_stack(soc, pdev, ppdu_mdata->peer_id, ppdu_mdata->ppdu_id, nbuf);
    } else {
        if (peer) {
            dp_peer_unref_del_find_by_id(peer);
        }
        return false;
    }

    if (peer) {
        dp_peer_unref_del_find_by_id(peer);
    }
    return true;
}

/*
 * osif_nss_wifili_fill_mesh_stats
 *      Fill mesh stats.
 */
static
void osif_nss_wifili_fill_mesh_stats(struct ieee80211vap *vap, qdf_nbuf_t nbuf, struct nss_wifi_vdev_meshmode_rx_metadata *mesh_metadata)
{
    struct mesh_recv_hdr_s *rx_info = NULL;

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    struct wlan_crypto_key *key = NULL;
#endif
    /* fill recv mesh stats */
    rx_info = qdf_mem_malloc(sizeof(struct mesh_recv_hdr_s));
    qdf_mem_zero(rx_info, (sizeof(struct mesh_recv_hdr_s)));

    /* upper layers are resposible to free this memory */
    if (rx_info == NULL) {
        QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
                "Memory allocation failed for mesh rx stats");
        /* DP_STATS_INC(vdev->pdev, mesh_mem_alloc, 1);  */
        return;
    }

    rx_info->rs_flags = mesh_metadata->rs_flags;
    rx_info->rs_keyix = mesh_metadata->rs_keyix;
    rx_info->rs_rssi = mesh_metadata->rs_rssi;
    rx_info->rs_channel = mesh_metadata->rs_channel;
    rx_info->rs_ratephy1 = mesh_metadata->rs_ratephy;

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    if ((rx_info->rs_flags & MESH_RX_DECRYPTED) && (rx_info->rs_flags & MESH_KEY_NOTFILLED)) {
        qdf_mem_copy(rx_info->rs_decryptkey, vap->iv_nw_keys[rx_info->rs_keyix & 0x3].wk_key,IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE);
    }
#else
    key = wlan_crypto_vdev_getkey(vap->vdev_obj, rx_info->rs_keyix & 0x3);
    if (!key)
        return;
    if ((rx_info->rs_flags & MESH_RX_DECRYPTED) && (rx_info->rs_flags & MESH_KEY_NOTFILLED)) {
        qdf_mem_copy(rx_info->rs_decryptkey, key->keyval, WLAN_CRYPTO_KEYBUF_SIZE
                                                           + WLAN_CRYPTO_MICBUF_SIZE);
    }
#endif
    qdf_nbuf_set_rx_fctx_type(nbuf, (void *)rx_info, CB_FTYPE_MESH_RX_INFO);
}

/*
 * osif_nss_li_vdev_set_peer_nexthop()
 *      Handles set_peer_nexthop message
 */
int osif_nss_li_vdev_set_peer_nexthop(osif_dev *osif, uint8_t *addr, uint32_t if_num)
{
    struct nss_ctx_instance *nss_ctx = NULL;
    int status = 0;
    nss_tx_status_t nss_status;

    if (!osif) {
        return status;
    }

    nss_ctx = osif->nss_wifiol_ctx;
    if (!nss_ctx) {
        return status;
    }

    nss_status = nss_wifi_vdev_set_peer_next_hop(nss_ctx, osif->nss_ifnum, addr, if_num);
    if (nss_status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the peer next hop message to NSS\n");
        return status;
    }

    qdf_print("\nSetting next hop of peer %pM to %d interface number", addr, if_num);

    return 1;
}

/*
 * osif_nss_wifili_vdev_data_receive_meshmode_rxinfo()
 *       Handler for data packets exceptioned from WIFI
 */
#if ATH_DATA_RX_INFO_EN || MESH_MODE_SUPPORT
void osif_nss_wifili_vdev_data_receive_meshmode_rxinfo(struct net_device *dev, struct sk_buff *skb)
{
    osif_dev  *osdev;
    struct dp_vdev *vdev = NULL;
    struct ieee80211vap *vap = NULL;
    struct dp_pdev *pdev = NULL;
    struct nss_wifi_vdev_meshmode_rx_metadata *mesh_metadata = NULL;
    qdf_nbuf_t msdu = (qdf_nbuf_t)skb;
    uint8_t ftype = CB_FTYPE_INVALID;

    /*
     * Need to move this code to wifi driver
     */
    if(dev == NULL) {
        qdf_print(KERN_CRIT "%s , netdev is NULL, freeing skb", __func__);
        qdf_nbuf_free(skb);
        return;
    }

    osdev = ath_netdev_priv(dev);
    vap = osdev->os_if;
    vdev = wlan_vdev_get_dp_handle(osdev->ctrl_vdev);
    pdev = vdev->pdev;

    if(pdev == NULL) {
        qdf_print(KERN_CRIT "%s , netdev is NULL, freeing skb", __func__);
        qdf_nbuf_free(skb);
        return;
    }

    if (vap->iv_mesh_vap_mode) {
        mesh_metadata = (struct nss_wifi_vdev_meshmode_rx_metadata *)skb->data;
        skb_pull(skb, sizeof(struct nss_wifi_vdev_meshmode_rx_metadata));
        if (vap->mdbg & MESH_DEBUG_DUMP_STATS) {
            osif_nss_wifili_fill_mesh_stats(vap, msdu, mesh_metadata);
        }
    }

    ftype = qdf_nbuf_get_rx_ftype(skb);
    if((ftype == CB_FTYPE_RX_INFO) || (ftype == CB_FTYPE_MESH_RX_INFO)){
        osif_rx_status_dump((void *)qdf_nbuf_get_rx_fctx(skb));
        qdf_mem_free((void *)qdf_nbuf_get_rx_fctx(skb));
        qdf_nbuf_set_rx_fctx_type(skb, 0, CB_FTYPE_INVALID);
    }

}
#endif

uint8_t osif_nss_wifili_vdev_call_monitor_mode(struct net_device *netdev, osif_dev  *osdev, qdf_nbuf_t skb_list_head, uint8_t is_chain)
{

    /*
     * Handle Monitor Mode is not required for 8074
     */
    return 0;
}

void osif_nss_wifili_vdevcfg_set_offload_params(void * vdev, struct nss_wifi_vdev_config_msg **p_wifivdevcfg)
{
    struct nss_wifi_vdev_config_msg *wifivdevcfg = *p_wifivdevcfg;
    struct dp_vdev *vdev_handle = (struct dp_vdev *)vdev;

    wifivdevcfg->vdev_id = vdev_handle->vdev_id;
    wifivdevcfg->opmode = vdev_handle->opmode;
    memcpy(wifivdevcfg->mac_addr,  &vdev_handle->mac_addr.raw[0], 6);
    return;
}

void osif_nss_wifili_get_peerid( struct MC_LIST_UPDATE* list_entry, uint32_t *peer_id)
{
    struct dp_peer *peer = NULL;
    peer = (struct dp_peer *)wlan_peer_get_dp_handle((list_entry->ni)->peer_obj);
    if (!peer)
        return;

    *peer_id = peer->peer_ids[0];
    return;
}

uint8_t osif_nss_wifili_get_vdevid_fromvdev(void *vdev, uint8_t *vdev_id)
{
    struct dp_vdev *vdev_handle = (struct dp_vdev *)vdev;
    if (!vdev_handle)
        return 0;

    *vdev_id = vdev_handle->vdev_id;
    return 1;
}

uint8_t osif_nss_wifili_get_vdevid_fromosif(osif_dev *osifp, uint8_t *vdev_id)
{
    struct dp_vdev *pstavdev = NULL;
    pstavdev = wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    if (!pstavdev) {
        return 0;
    }

    return (osif_nss_wifili_get_vdevid_fromvdev(pstavdev, vdev_id));
}

/*
 * osif_nss_ol_peerid_find_hash_find()
 * 	Get the peer_id using the hash index.
 */
uint32_t osif_nss_wifili_peerid_find_hash_find(struct ieee80211vap *vap, uint8_t *peer_mac_addr, int mac_addr_is_aligned)
{

    uint32_t peer_id = 0;

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT_LI
    struct dp_peer *peer = NULL;
    struct dp_vdev *vdev = (struct dp_vdev *) wlan_vdev_get_dp_handle(vap->vdev_obj);
    struct dp_pdev *pdev = (struct dp_pdev *)(vdev->pdev);

    peer = dp_peer_find_hash_find(pdev->soc, peer_mac_addr, mac_addr_is_aligned, vdev->vdev_id);

    if (!(peer)) {
        return HTT_INVALID_PEER;
    }
    peer_id = peer->local_id;
    dp_peer_unref_delete(peer);
#endif

    return peer_id;
}

void *osif_nss_wifili_peer_find_hash_find(struct ieee80211vap *vap, uint8_t *peer_mac_addr, int mac_addr_is_aligned)
{

    struct dp_peer *peer = NULL;
    struct dp_vdev *vdev = (struct dp_vdev *) wlan_vdev_get_dp_handle(vap->vdev_obj);
    struct dp_pdev *pdev = (struct dp_pdev *)(vdev->pdev);

    peer = dp_peer_find_hash_find(pdev->soc, peer_mac_addr, mac_addr_is_aligned, vdev->vdev_id);

    if (!(peer)) {
        return NULL;
    }
    dp_peer_unref_delete(peer);

    return peer;
}


void * osif_nss_wifili_find_peer_by_id(osif_dev *osifp, uint32_t peer_id)
{
    struct dp_vdev *vdev = NULL;
    struct dp_pdev *pdev = NULL;
    struct dp_peer *peer = NULL;

    vdev = wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    if (!vdev) {
        return NULL;
    }
    pdev = (struct dp_pdev *)vdev->pdev;
    peer = dp_peer_find_by_id(pdev->soc, peer_id);
    return ((void *)peer);
}

uint8_t osif_nss_wifili_find_pstosif_by_id(struct net_device *netdev, uint32_t peer_id, osif_dev **psta_osifp)
{
    osif_dev *osifp;
    struct dp_vdev *vdev = NULL;
    struct dp_pdev *pdev = NULL;
    struct dp_vdev *pstavdev = NULL;
    struct dp_peer *pstapeer;

    osifp = netdev_priv(netdev);
    vdev = wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    if (!vdev) {
        return 0;
    }
    pdev = vdev->pdev;
    pstapeer = dp_peer_find_by_id(pdev->soc, peer_id);
    if (!pstapeer) {
        qdf_print("no peer available free packet ");
        return 0;
    }
    pstavdev = pstapeer->vdev;
    if (!pstavdev) {
        qdf_print("no vdev available free packet ");
        return 0;
    }
    *psta_osifp = (osif_dev *)pstavdev->osif_vdev;
    return 1;
}

/*
 * osif_nss_wifili_vap_updchdhdr()
 *	API for updating header cache in NSS.
 */
/*
 * Note: This function has been created seperately for 8064
 * and 8074 as there is no element called "hdrcache" in dp_vdev
 * (vdev->hdrcache). And this function might not be required
 * for 8074, so this is a dummy function here.
 */
int32_t osif_nss_wifili_vap_updchdhdr(osif_dev *osifp)
{
    return 0;
}

/*
 * osif_nss_wifili_vdev_set_cfg
 */
uint32_t osif_nss_wifili_vdev_set_cfg(osif_dev *osifp, enum osif_nss_vdev_cmd osif_cmd)
{
    uint32_t val = 0;
    enum nss_wifi_vdev_cmd cmd = 0;
    struct dp_vdev *vdev = NULL;

    if (!osifp) {
        return 0;
    }

    vdev = (struct dp_vdev *)wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    if (!vdev) {
        qdf_print("DP vdev handler is NULL");
	return 0;
    }

    if (osifp->nss_ifnum == -1) {
        qdf_print(" vap transmit called on invalid interface");
        return 0;
    }

    switch (osif_cmd) {
        case OSIF_NSS_VDEV_DROP_UNENC:
            cmd = NSS_WIFI_VDEV_DROP_UNENC_CMD;
            val = vdev->drop_unenc;
            break;

        case OSIF_NSS_WIFI_VDEV_NAWDS_MODE:
            cmd = NSS_WIFI_VDEV_NAWDS_MODE_CMD;
            val = vdev->nawds_enabled;
            break;

#ifdef WDS_VENDOR_EXTENSION
        case OSIF_NSS_WIFI_VDEV_WDS_EXT_ENABLE:
            cmd = NSS_WIFI_VDEV_CFG_WDS_EXT_ENABLE_CMD;
            val = WDS_EXT_ENABLE;
            break;
#endif

        case OSIF_NSS_VDEV_WDS_CFG:
            cmd = NSS_WIFI_VDEV_CFG_WDS_CMD;
            val = vdev->wds_enabled;
            break;

        case OSIF_NSS_VDEV_AP_BRIDGE_CFG:
            cmd = NSS_WIFI_VDEV_CFG_AP_BRIDGE_CMD;
            val = vdev->ap_bridge_enabled;
            break;

        case OSIF_NSS_VDEV_SECURITY_TYPE_CFG:
            cmd = NSS_WIFI_VDEV_SECURITY_TYPE_CMD;
            val = vdev->sec_type;
            break;

        default:
            break;
    }
    return val;
}

/*
 * osif_nss_wifili_vdev_get_stats
 *
 * Note: dp_pdev structure stats structure is different from "ol_txrx_stats"
 * structure in ol_txrx_pdev_t.
 * Also one-to-one mapping for 8074 could not be found so this is a dummy
 * function for 8074
 */
uint8_t osif_nss_wifili_vdev_get_stats(osif_dev *osifp, struct nss_cmn_msg *wifivdevmsg)
{
    wlan_if_t vap;
    struct nss_wifi_vdev_stats_sync_msg *stats =
    (struct nss_wifi_vdev_stats_sync_msg *)&((struct nss_wifi_vdev_msg *)wifivdevmsg)->msg.vdev_stats;
    struct ieee80211_mac_stats *unimacstats ;
    struct nss_wifi_vdev_mcast_enhance_stats *nss_mestats = NULL;
    struct dp_vdev *vdev = NULL;
    struct dp_pdev *pdev = NULL;
    struct cdp_tx_ingress_stats *txi_stats = NULL;
    struct cdp_tx_stats *tx_stats = NULL;
    struct cdp_rx_stats *rx_stats = NULL;

    if (!osifp) {
        return 0;
    }

    vdev = (struct dp_vdev *)wlan_vdev_get_dp_handle(osifp->ctrl_vdev);
    if (!vdev) {
        qdf_print("DP vdev handler is NULL");
        return 0;
    }

    pdev = vdev->pdev;
    if (!pdev) {
        qdf_print("DP pdev handler is NULL");
        return 0;
    }

    nss_mestats = &stats->wvmes;
    txi_stats = &vdev->stats.tx_i;
    tx_stats = &vdev->stats.tx;
    rx_stats = &vdev->stats.rx;

    txi_stats->mcast_en.mcast_pkt.num += nss_mestats->mcast_rcvd;
    txi_stats->mcast_en.mcast_pkt.bytes += nss_mestats->mcast_rcvd_bytes;
    txi_stats->mcast_en.ucast += nss_mestats->mcast_ucast_converted;
    txi_stats->mcast_en.fail_seg_alloc += nss_mestats->mcast_alloc_fail;
    txi_stats->mcast_en.dropped_send_fail += (nss_mestats->mcast_pbuf_enq_fail +
            nss_mestats->mcast_pbuf_copy_fail + nss_mestats->mcast_peer_flow_ctrl_send_fail);
    txi_stats->mcast_en.dropped_self_mac += (nss_mestats->mcast_loopback_err +
            nss_mestats->mcast_dst_address_err + nss_mestats->mcast_no_enhance_drop_cnt);


    txi_stats->rcvd.num += stats->tx_rcvd;
    txi_stats->rcvd.bytes += stats->tx_rcvd_bytes;
    txi_stats->processed.num += stats->tx_enqueue_cnt;
    txi_stats->processed.bytes += stats->tx_enqueue_bytes;
    txi_stats->dropped.ring_full += stats->tx_hw_ring_full;
    txi_stats->dropped.desc_na.num += stats->tx_desc_alloc_fail;
    txi_stats->dropped.dma_error += stats->tx_dma_map_fail;
    txi_stats->tso.tso_pkt.num += stats->tx_tso_pkt;
    txi_stats->tso.num_seg += stats->tx_num_seg;
    txi_stats->cce_classified += stats->cce_classified;
    txi_stats->cce_classified_raw += stats->cce_classified_raw;
    txi_stats->nawds_mcast.num += stats->nawds_tx_mcast_cnt;
    txi_stats->nawds_mcast.bytes += stats->nawds_tx_mcast_bytes;

    if (qdf_unlikely(vdev->tx_encap_type == htt_cmn_pkt_type_raw)) {
        txi_stats->raw.raw_pkt.num += stats->tx_rcvd;
        txi_stats->raw.raw_pkt.bytes += stats->tx_rcvd_bytes;
    }

    vap = osifp->os_if;
    if (!vap || osifp->is_delete_in_progress || (vap->iv_opmode == IEEE80211_M_MONITOR)) {
    return 0;
    }

    unimacstats = &vap->iv_unicast_stats;
    unimacstats->ims_tx_eapol_packets += stats->tx_eapol_cnt;

    if (!pdev->enhanced_stats_en) {
        /*
         * Update net device statistics
         */
        rx_stats->to_stack.num += (stats->rx_enqueue_cnt + stats->rx_except_enqueue_cnt);
        rx_stats->to_stack.bytes += stats->rx_enqueue_bytes;
        rx_stats->rx_discard += (stats->rx_enqueue_fail_cnt + stats->rx_except_enqueue_fail_cnt);

        tx_stats->comp_pkt.num += (stats->tx_enqueue_cnt + stats->tx_intra_bss_enqueue_cnt);
        tx_stats->comp_pkt.bytes += stats->tx_enqueue_bytes;
        tx_stats->mcast.num += stats->tx_intra_bss_mcast_send_cnt;
        tx_stats->tx_failed += (stats->tx_enqueue_fail_cnt + stats->tx_intra_bss_enqueue_fail_cnt +
                stats->tx_intra_bss_mcast_send_fail_cnt);
        txi_stats->dropped.dropped_pkt.num = txi_stats->dropped.desc_na.num + txi_stats->dropped.ring_full +
            txi_stats->dropped.dma_error + tx_stats->tx_failed;

    }

    return 1;
}

#if ATH_SUPPORT_WRAP
/*
 * osif_nss_wifili_vdev_get_mpsta_vdevid
 */
uint8_t osif_nss_wifili_vdev_get_mpsta_vdevid(ol_ath_soc_softc_t *soc, uint16_t peer_id, uint8_t vdev_id, uint8_t *mpsta_vdev_id)
{
    struct ieee80211vap *mpsta_vap = NULL;
    osif_dev  *mpsta_osdev = NULL;
    struct ieee80211vap *vap = NULL;
    struct wlan_objmgr_vdev *vdev = NULL;
    struct wlan_objmgr_psoc *psoc = soc->psoc_obj;

    if (!psoc) {
        qdf_print("Get MPSTA: psoc is NULL");
        return 0;
    }

    vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id, WLAN_MLME_SB_ID);
    if (!vdev) {
        qdf_print("Get MPSTA: vdev is NULL");
        return 0;
    }

    vap = wlan_vdev_get_vap(vdev);
    if (!vap) {
        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
        qdf_print("Get MPSTA: vap is NULL");
        return 0;
    }

    wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);
    /*
     * Vap is psta and not mpsta
     */
    if (!vap->iv_mpsta && vap->iv_psta) {
        mpsta_vap = vap->iv_ic->ic_mpsta_vap;
        if (!mpsta_vap) {
            qdf_print("Get MPSTA: mpsta_vap is NULL");
            return 0;
        }
    } else {
        return 0;
    }

    mpsta_osdev = (osif_dev *)mpsta_vap->iv_ifp;
    if (!mpsta_osdev) {
        qdf_print("Get MPSTA: mpsta_osdev is NULL");
        return 0;
    }

    return osif_nss_wifili_get_vdevid_fromosif(mpsta_osdev, mpsta_vdev_id);
}

/*
 * osif_nss_ol_li_vdev_qwrap_mec_check
 */
uint8_t osif_nss_ol_li_vdev_qwrap_mec_check(osif_dev *mpsta_osifp, struct sk_buff *skb)
{
    struct ether_header *eh = NULL;
    uint8_t src_mac[QDF_MAC_ADDR_SIZE];
    struct dp_vdev *vdev = NULL;
    osif_dev *psta_osdev = NULL;
    struct ieee80211vap *mpsta_vap = NULL;

    if (!mpsta_osifp) {
        return 0;
    }

    mpsta_vap = mpsta_osifp->os_if;
    if (!mpsta_vap) {
        return 0;
    }

    /*
     * If Qwrap Isolation Mode is enabled, the mec check is not
     * required.
     */
    if (mpsta_vap->iv_ic->ic_wrap_com->wc_isolation) {
        return 0;
    }

    eh = (struct ether_header *)(skb->data);
    memcpy(src_mac, (uint8_t *)eh->ether_shost, QDF_MAC_ADDR_SIZE);

    /* Mpsta vap here, find the correct vap from the wrap common based on src address */
    psta_osdev = osif_wrap_wdev_vma_find(&mpsta_vap->iv_ic->ic_wrap_com->wc_devt, src_mac);
    if (!psta_osdev) {
        return 0;
    }

    vdev = (struct dp_vdev *)wlan_vdev_get_dp_handle(
                                      psta_osdev->ctrl_vdev);
    if (!vdev) {
        return 0;
    }

    if (!(memcmp(src_mac, vdev->mac_addr.raw, QDF_MAC_ADDR_SIZE))) {
        return 1;
    }

    return 0;
}

uint8_t osif_nss_ol_li_vdev_get_nss_qwrap_en(struct ieee80211vap *vap)
{
    return vap->iv_nss_qwrap_en;
}

#endif

#ifdef WDS_VENDOR_EXTENSION
/*
 * osif_nss_wifili_vdev_get_wds_peer()
 *  Get the peer pointer from the vdev list.
 *  This is used for wds vendor extension.
 */
void *osif_nss_wifili_vdev_get_wds_peer(void *vdev_handle)
{
    struct dp_vdev *vdev =  (struct dp_vdev *)vdev_handle;
    struct dp_peer *peer = NULL;

    if(!vdev) {
        QDF_TRACE(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_ERROR,
                "Vdev handle NULL\n");
        return NULL;
    }

    if (vdev->opmode == wlan_op_mode_ap) {
        /* For ap, set it on bss_peer */
        TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
            if (peer->bss_peer) {
                break;
            }
        }
    } else if (vdev->opmode == wlan_op_mode_sta) {
        peer = TAILQ_FIRST(&vdev->peer_list);
    }

    return (void *)peer;
}
#endif /* WDS_VENDOR_EXTENSION */

/*
 * osif_nss_ol_li_vdev_data_receive_mec_check()
 *  In STA mode, Pass/Drop frame based on ast entry of type MEC
 */
bool osif_nss_ol_li_vdev_data_receive_mec_check(osif_dev *osdev, struct sk_buff *nbuf)
{
    struct dp_vdev *vdev = NULL;
    struct dp_pdev *pdev = NULL;
    struct dp_soc *soc = NULL;
    struct dp_ast_entry *ast_entry = NULL;
    uint8_t wds_src_mac[IEEE80211_ADDR_LEN];

    vdev = wlan_vdev_get_dp_handle(osdev->ctrl_vdev);
    pdev = vdev->pdev;
    soc = pdev->soc;
    memcpy(wds_src_mac, (qdf_nbuf_data(nbuf) + IEEE80211_ADDR_LEN),
            IEEE80211_ADDR_LEN);

    /*
     * check if ast entry is of type MEC
     * if so drop the frame here
     */
    qdf_spin_lock_bh(&soc->ast_lock);
    if (soc->ast_override_support) {
        ast_entry = dp_peer_ast_hash_find_by_pdevid(soc, wds_src_mac, pdev->pdev_id);
    } else {
        ast_entry = dp_peer_ast_hash_find_soc(soc, wds_src_mac);
    }
    if (ast_entry && ast_entry->type == CDP_TXRX_AST_TYPE_MEC) {
        qdf_spin_unlock_bh(&soc->ast_lock);
        return true;
    }
    qdf_spin_unlock_bh(&soc->ast_lock);
    return false;
}
