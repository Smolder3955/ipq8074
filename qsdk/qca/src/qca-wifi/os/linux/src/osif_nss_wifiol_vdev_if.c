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
 * osif_nss_wifiol_vdev_if.c
 *
 * This file used for for interface to NSS WiFi Offload  VAP
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

#include <linux/if_arp.h>
#include <linux/etherdevice.h>
#include <linux/in.h>
#include <asm/cacheflush.h>

#include <nss_api_if.h>
#include <nss_cmn.h>
#include <qdf_nbuf.h>

#include "osif_private.h"
#include "ol_if_athvar.h"
#include "osif_private.h"
#include "osif_nss_wifiol_vdev_if.h"
#include "osif_nss_wifiol_if.h"
#include <ar_internal.h>
#include <dp_extap.h>

#if MESH_MODE_SUPPORT
#include <if_meta_hdr.h>
#endif

#define DP_LAG_SEC_SKIP_VAP_SEND false
struct osif_nss_vdev_cfg_pvt osif_nss_vdev_cfgp, osif_nss_vdev_recovp;
#if QCA_SUPPORT_SON
#include <wlan_son_pub.h>
#endif
static struct osif_nss_vdev_vow_dbg_stats_pvt osif_nss_vdev_vowp;

#if ATH_SUPPORT_ME_SNOOP_TABLE
extern void
ieee80211_me_SnoopListUpdate_timestamp(struct MC_LIST_UPDATE* list_entry, u_int32_t ether_type);
#endif

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
 * NSS capwap mgr debug macros
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

#define OSIF_NSS_EXTAP_ENTRY_IPV4 1
#define OSIF_NSS_EXTAP_ENTRY_IPV6 2

#if DBDC_REPEATER_SUPPORT
extern int dbdc_rx_process (os_if_t *osif ,struct net_device **dev ,struct sk_buff *skb, int *nwifi);
extern int dbdc_tx_process (wlan_if_t vap, osif_dev **osdev , struct sk_buff *skb);
#endif


#define ETHERTYPE_IPV4 0x0800
int osif_nss_vdev_tx_raw(void *osif_vdev, qdf_nbuf_t *pnbuf)
{
    qdf_nbuf_t nbuf_list_head = NULL;
    qdf_nbuf_t next = NULL;

    if (!osif_vdev) {
        return 1;
    }

    nbuf_list_head = *pnbuf;
    if (nbuf_list_head->next == NULL) {
        return 0;
    }

    skb_frag_list_init(nbuf_list_head);

    nbuf_list_head->data_len = 0;
    next = nbuf_list_head->next;
    nbuf_list_head->next = NULL;
    if (!skb_has_frag_list(nbuf_list_head)) {
        skb_shinfo(nbuf_list_head)->frag_list = next;
    }

    skb_walk_frags(nbuf_list_head, next) {
        nbuf_list_head->len += qdf_nbuf_len(next);
        nbuf_list_head->data_len += qdf_nbuf_len(next);
        nbuf_list_head->truesize += qdf_nbuf_len(next);
    }

    return 0;
}

void osif_nss_vdev_deliver_rawbuf(struct net_device *netdev, qdf_nbuf_t msdu_list,  struct napi_struct *napi)
{
    qdf_nbuf_t nbuf = msdu_list;
    qdf_nbuf_t next = NULL;

    dev_hold(netdev);
    while (nbuf)
    {
        next = nbuf->next;
        nbuf->dev = netdev;
        nbuf->next = NULL;
        nbuf->protocol = eth_type_trans(nbuf, netdev);
        /*
         * GRO enable on wifi is causing 4n aligned frame sent to NSS
         * avoid GRO processing in stack for raw simulator output frames
         */
        skb_orphan(nbuf);
        nbuf_debug_del_record(nbuf);
        netif_receive_skb(nbuf);
        nbuf = next;
    }
    dev_put(netdev);
}


void osif_nss_ol_vdev_handle_rawbuf(struct net_device *netdev,
        qdf_nbuf_t nbuf, __attribute__((unused)) struct napi_struct *napi)
{
    void *peer = NULL;
    struct nss_wifi_vdev_rawmode_rx_metadata *wmrm;
    uint16_t peer_id;
    struct ieee80211com* ic = NULL;

    qdf_nbuf_t deliver_list_head = NULL;
    qdf_nbuf_t deliver_list_tail = NULL;
    qdf_nbuf_t tmp;
    osif_dev  *osdev;
    struct ieee80211vap *vap = NULL;

    osdev = ath_netdev_priv(netdev);
    vap = osdev->os_if;
    ic = vap->iv_ic;

    wmrm = (struct nss_wifi_vdev_rawmode_rx_metadata *)nbuf->data;
    peer_id = wmrm->peer_id;
    skb_pull(nbuf, sizeof(struct nss_wifi_vdev_rawmode_rx_metadata));

    if (ic->nss_iops) {
        peer = ic->nss_iops->ic_osif_nss_ol_find_peer_by_id(osdev, peer_id);
        if (!peer) {
            qdf_print("no peer available free packet ");
            qdf_nbuf_free(nbuf);
            return;
        }
    }
    /*
     * Check if SKB has fraglist and convert the fraglist to
     * skb->next chains
     */
    memset(nbuf->cb, 0x0, sizeof(nbuf->cb));
    /* qdf_print("\nRcvd pkt:0x%x of length:%d head:0x%X data:0x%x", nbuf, nbuf->len, nbuf->head, nbuf->data); */
    OSIF_TXRX_LIST_APPEND(deliver_list_head, deliver_list_tail, nbuf);
    if (skb_has_frag_list(nbuf)) {
        qdf_nbuf_set_rx_chfrag_start(nbuf, 1);
        nbuf->len  = skb_headlen(nbuf);
        nbuf->truesize -= nbuf->data_len;
        nbuf->data_len = 0;
        nbuf->next = skb_shinfo(nbuf)->frag_list;

        /* qdf_print("\nSKB has fraglist"); */
        skb_walk_frags(nbuf, tmp) {
            OSIF_TXRX_LIST_APPEND(deliver_list_head, deliver_list_tail, tmp);
        }
        qdf_nbuf_set_rx_chfrag_end(deliver_list_tail, 1);
        /*
         * Reset the skb frag list
         */
        skb_frag_list_init(nbuf);
    } else {
        if (nbuf->next != NULL) {
            qdf_print("\nUn Expected");
        }
    }

#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
    if (vap->iv_rawmode_pkt_sim) {
        osif_rsim_rx_decap((os_if_t)osdev, &deliver_list_head, &deliver_list_tail, (struct cdp_peer *) peer);
    }
#endif
    osif_nss_vdev_deliver_rawbuf(netdev, deliver_list_head, napi);
}
qdf_export_symbol(osif_nss_ol_vdev_handle_rawbuf);

void osif_nss_vdev_frag_to_chain(void *osif_vdev, qdf_nbuf_t nbuf, qdf_nbuf_t *list_head, qdf_nbuf_t *list_tail)
{
    qdf_nbuf_t deliver_list_head = NULL;
    qdf_nbuf_t deliver_list_tail = NULL;
    qdf_nbuf_t tmp;

    /*
     * Check if SKB has fraglist and convert the fraglist to
     * skb->next chains
     */
    memset(nbuf->cb, 0x0, sizeof(nbuf->cb));
    OSIF_TXRX_LIST_APPEND(deliver_list_head, deliver_list_tail, nbuf);
    if (skb_has_frag_list(nbuf)) {
        qdf_nbuf_set_rx_chfrag_start(nbuf, 1);
        nbuf->len  = skb_headlen(nbuf);
        nbuf->truesize -= nbuf->data_len;
        nbuf->data_len = 0;
        nbuf->next = skb_shinfo(nbuf)->frag_list;

        /* qdf_print("\nSKB has fraglist"); */
        skb_walk_frags(nbuf, tmp) {
            OSIF_TXRX_LIST_APPEND(deliver_list_head, deliver_list_tail, tmp);
        }
        qdf_nbuf_set_rx_chfrag_end(deliver_list_tail, 1);

        /*
         * Reset the skb frag list
         */
        skb_frag_list_init(nbuf);
    } else {
        if (nbuf->next != NULL) {
            qdf_print("\nUn Expected");
        }
    }

    *list_head = deliver_list_head;
    *list_tail = deliver_list_tail;
}

/*
 * osif_nss_vdev_event_receive()
 * 	Event callback to receive event data from NSS
 */
static void osif_nss_vdev_event_receive(void *dev, struct nss_cmn_msg *wifivdevmsg)
{
    struct net_device *netdev;
    osif_dev *osdev;
    struct ieee80211vap *vap;
    uint32_t msg_type = wifivdevmsg->type;
    struct ieee80211com *ic = NULL;

    /*
     * Need to move this code to wifi driver
     */
    if(dev == NULL) {
        qdf_print(KERN_CRIT "%s , netdev is NULL, freeing skb", __func__);
        return;
    }

    netdev = (struct net_device *)dev;

    osdev = ath_netdev_priv(netdev);
    if (!osdev) {
        qdf_print("dev is NULL");
        return;
    }

    vap = osdev->os_if;
    if (!vap) {
        qdf_print("""THIS IS NOT EXPECTED""""");
        return;
    }
    ic = vap->iv_ic;

    switch (msg_type) {
        case NSS_WIFI_VDEV_ME_SYNC_MSG:
            {
                struct MC_LIST_UPDATE list;
                struct nss_wifi_vdev_me_host_sync_msg *me_sync_msg =
                    (struct nss_wifi_vdev_me_host_sync_msg *)&((struct nss_wifi_vdev_msg *)wifivdevmsg)->msg.vdev_me_sync;
                struct nss_wifi_vdev_me_host_sync_grp_entry *me_sync =
                    (struct nss_wifi_vdev_me_host_sync_grp_entry *)&me_sync_msg->grp_entry[0];
                int idx = 0;
                u_int32_t *swap_grp_ip;

                while(1) {
                    list.src_ip_addr = me_sync[idx].src_ip_addr;
                    memcpy(&list.grp_addr[0], me_sync[idx].group_addr, IEEE80211_ADDR_LEN);
                    memcpy(&list.grp_member[0], me_sync[idx].grp_member_addr, IEEE80211_ADDR_LEN);

                    list.vap = vap;
                    list.u.grpaddr_ip4 = me_sync[idx].u.grpaddr_ip4;
                    swap_grp_ip = &list.u.grpaddr_ip4;
                    *swap_grp_ip = qdf_htonl(list.u.grpaddr_ip4);
                    list.cmd = 0;

#if ATH_SUPPORT_ME_SNOOP_TABLE
                    ieee80211_me_SnoopListUpdate_timestamp(&list, ETHERTYPE_IP);
#endif

                    if(idx == me_sync_msg->nentries)
                        break;
                    idx++;
                }
            }
            break;

        case   NSS_WIFI_VDEV_STATS_MSG:
            if (ic->nss_iops)
                ic->nss_iops->ic_osif_nss_ol_vdev_get_stats(osdev, wifivdevmsg);
                    break;
    }
    return;
}

void osif_nss_vdev_peer_tx_buf(void *osif_vdev, struct sk_buff *skb, uint16_t peer_id)
{
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_txmsg *vdev_tx_special_data;
    bool status;
    uint8_t *data;
    osif_dev *osif =  (osif_dev *)osif_vdev;
    void *nss_wifiol_ctx = osif->nss_wifiol_ctx;
    qdf_nbuf_t nbuf;
    uint32_t msg_len;
    uint32_t cmd_len;

    if (!nss_wifiol_ctx) {
        return;
    }

    cmd_len = sizeof(struct nss_cmn_msg) + sizeof(struct nss_wifi_vdev_txmsg) ;
    msg_len =  cmd_len + skb->len;

    vdev_tx_special_data = &wifivdevmsg.msg.vdev_txmsgext;
    memset(&wifivdevmsg, 0, cmd_len);

    vdev_tx_special_data->peer_id = peer_id;

    nss_wifi_vdev_msg_init(&wifivdevmsg, osif->nss_ifnum, NSS_WIFI_VDEV_SPECIAL_DATA_TX_MSG,
                           msg_len , NULL, NULL);

    /*
     * Allocate a container SKB for carrying the message + SKB
     */
    if(!(nbuf = qdf_nbuf_alloc(NULL, msg_len, 0, 4, FALSE))) {
        osif_nss_warn("Skb allocation failed \n");
        qdf_nbuf_free(skb);
        return;
    }

    /*
     * Copy the message to the container skb
     */
    data = skb_put(nbuf, cmd_len);
    memcpy(data, &wifivdevmsg, cmd_len);

    /*
     * Copy buffer data into the newly allocated container buffer
     */
    data = skb_put(nbuf, skb->len);
    memcpy(data, skb->data, skb->len);

    /*
     * Send the vdev special data to NSS
     */
    status = nss_wifi_vdev_tx_msg_ext(nss_wifiol_ctx, nbuf);

    if (status != NSS_TX_SUCCESS) {
        qdf_nbuf_free(nbuf);
        osif_nss_warn("Unable to send the grp_list create message to NSS\n");
    }

    qdf_nbuf_free(skb);
}
qdf_export_symbol(osif_nss_vdev_peer_tx_buf);

/*
 * osif_nss_vdev_skb_needs_linearize()
 * 	Check if skb needs linearlize
 */
bool osif_nss_vdev_skb_needs_linearize(struct net_device *dev, struct sk_buff *skb)
{
    osif_dev *osdev;
    struct ieee80211vap *vap;

    osdev = netdev_priv(dev);
    vap = osdev->os_if;

    /*
     * if skb does not have fraglist return true
     */
    if (!skb_has_frag_list(skb)) {
        return true;
    }

    /*
     * Linearize skb if the mode in non monitor mode and rx decap type is not raw
     */
    if ((osdev->os_opmode != IEEE80211_M_MONITOR) && (vap->iv_rx_decap_type != osif_pkt_type_raw)) {
        return true;
    }

    return false;
}

/*
 * osif_nss_ol_vdev_special_data_receive()
 *  Handler for data packets exceptioned from WIFI
 */
void osif_nss_vdev_special_data_receive(struct net_device *dev, struct sk_buff *skb, __attribute__((unused)) struct napi_struct *napi)
{
    struct net_device *netdev;
    struct nss_wifi_vdev_per_packet_metadata *wifi_metadata = NULL;
    struct nss_wifi_vdev_igmp_per_packet_metadata *igmp_metadata = NULL;
#if ATH_SUPPORT_WRAP
    struct nss_wifi_vdev_mpsta_per_packet_rx_metadata *mpsta_rx_metadata = NULL;
#endif
    struct nss_wifi_vdev_rx_err_per_packet_metadata *rx_err_metadata = NULL;

    struct nss_wifi_vdev_mesh_per_packet_metadata *mesh_metadata = NULL;
    struct nss_wifi_vdev_tx_compl_metadata *tx_compl_metadata = NULL;
    osif_dev *osifp;
    int discard = 0;
    struct ieee80211vap *vap;
    struct ol_ath_vap_net80211 *avn;
    struct ol_ath_softc_net80211 *scn;
#if ATH_SUPPORT_WRAP
    uint32_t peer_id = OL_TXRX_INVALID_LOCAL_PEER_ID;
    struct net_device *rxnetdev;
#endif
    struct ieee80211com *ic = NULL;
    uint8_t vdev_id = 0;

    bool is_raw = false;
    struct wlan_objmgr_vdev *vdev;

    nbuf_debug_add_record(skb);

    if(dev == NULL) {
        qdf_print(KERN_CRIT "%s , netdev is NULL, freeing skb", __func__);
        qdf_nbuf_free(skb);
        return;
    }

    netdev = (struct net_device *)dev;
    dev_hold(netdev);

    osifp = netdev_priv(netdev);
    skb->dev = netdev;
    vap = osifp->os_if;
    avn = OL_ATH_VAP_NET80211(vap);
    scn = avn->av_sc;
    ic = vap->iv_ic;
    vdev = osifp->ctrl_vdev;

    /*
     * non linear  skb with fraglist are processed in nss offload only if monitor mode is enabled
     * or decap type is raw otherwise linearize them
     */
    if (skb_is_nonlinear(skb)) {
        if (osif_nss_vdev_skb_needs_linearize(netdev, skb) && (__skb_linearize(skb))) {
            qdf_nbuf_free(skb);
            dev_put(netdev);
            return;
        }
    }

    skb = qdf_nbuf_unshare(skb);
    if (!skb) {
        dev_put(netdev);
        return;
    }

    qdf_nbuf_set_next(skb, NULL);

    dma_unmap_single (scn->soc->nss_soc.dmadev, virt_to_phys(skb->head), NSS_WIFI_VDEV_PER_PACKET_METADATA_OFFSET + sizeof(struct nss_wifi_vdev_per_packet_metadata), DMA_FROM_DEVICE);

    wifi_metadata = (struct nss_wifi_vdev_per_packet_metadata *)(skb->head + NSS_WIFI_VDEV_PER_PACKET_METADATA_OFFSET);

    if (vap->iv_tx_encap_type == htt_cmn_pkt_type_raw) {
        is_raw = true;
    }

    if (ic->nss_iops) {
        if (!ic->nss_iops->ic_osif_nss_ol_get_vdevid(osifp, &vdev_id)) {
            qdf_nbuf_free(skb);
            dev_put(netdev);
            return;
        }
    }

    switch (wifi_metadata->pkt_type) {
        case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_IGMP: {
            igmp_metadata = (struct nss_wifi_vdev_igmp_per_packet_metadata *)&wifi_metadata->metadata.igmp_metadata;

            /*
             * Notify UMAC to update wifi snooptable.
             */
            discard = ol_rx_notify((ol_pdev_handle)scn->sc_pdev, vdev_id,
                        igmp_metadata->peer_mac_addr, igmp_metadata->tid,
                        igmp_metadata->tsf32, OL_RX_NOTIFY_IPV4_IGMP, skb);

            if (discard) {
                qdf_nbuf_free(skb);
                break;
            }

            skb->protocol = eth_type_trans(skb, netdev);
            /*
             * Send this skb to stack also.
             */
            nbuf_debug_del_record(skb);
            napi_gro_receive(napi, skb);
            break;
       }

        case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_MESH: {
            mesh_metadata = (struct nss_wifi_vdev_mesh_per_packet_metadata *)&wifi_metadata->metadata.mesh_metadata;
            if (ic->nss_iops) {
                ic->nss_iops->ic_osif_nss_ol_vdev_spl_receive_ext_mesh(netdev, skb, mesh_metadata);
            }

            break;
        }

        case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_INSPECT: {
            if (ic->nss_iops)
                ic->nss_iops->ic_osif_nss_ol_vdev_tx_inspect_handler(
                             wlan_vdev_get_dp_handle(osifp->ctrl_vdev), skb);
            break;
        }

        case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_TXINFO: {
            if (ic->nss_iops) {
                ic->nss_iops->ic_osif_nss_ol_vdev_txinfo_handler(scn, skb, wifi_metadata, is_raw);
            }
            qdf_nbuf_free(skb);
            break;
        }
        case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_MPSTA_RX: {
#if ATH_SUPPORT_WRAP
            if (vap->iv_mpsta) {
                mpsta_rx_metadata = (struct nss_wifi_vdev_mpsta_per_packet_rx_metadata *)&wifi_metadata->metadata.mpsta_rx_metadata;
                peer_id = mpsta_rx_metadata->peer_id;
                rxnetdev  = osif_nss_vdev_process_mpsta_rx(netdev, skb, peer_id);
                dev_put(netdev);
                if (!rxnetdev) {
                    return;
                }
                netdev = rxnetdev;
                dev_hold(netdev);
                skb->dev = netdev;
                osif_nss_vdev_process_mpsta_rx_to_tx(netdev, skb, napi);
                dev_put(netdev);
                return;
            }
#endif
            break;
        }

        case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_MPSTA_TX: {
#if ATH_SUPPORT_WRAP
            if (vap->iv_mpsta || vap->iv_psta) {
#if UMAC_SUPPORT_WNM
            if (wlan_wnm_tfs_filter(vap, (wbuf_t) skb)) {
                if (skb != NULL) {
                    qdf_nbuf_free(skb);
                    dev_put(netdev);
                    return;
                }
            }
#endif

#if DBDC_REPEATER_SUPPORT
            /*
             * Do DBDC processing only for legacy.
             * For Hawkeye in case of MPSTA , DBDC processing is not required
             * as Qwrap processing takes care of dropping the packet in oma/vma
             * is not found in its own table.
             */
            if (!ol_target_lithium(scn->soc->psoc_obj)) {
                if (dbdc_tx_process(vap, &osifp, skb)) {
                    dev_put(netdev);
                    return;
                }
            }
#endif
                osif_nss_vdev_process_mpsta_tx(netdev, skb);
                dev_put(netdev);
                return;
            } else {
                qdf_nbuf_free(skb);
                dev_put(netdev);
                return;
            }
#endif
            break;
        }

        case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_EXTAP_TX: {
            if (dp_is_extap_enabled(vdev) &&
                    (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)) {
#if UMAC_SUPPORT_WNM
            if (wlan_wnm_tfs_filter(vap, (wbuf_t) skb)) {
                if (skb != NULL) {
                    qdf_nbuf_free(skb);
                    dev_put(netdev);
                    return;
                }
            }
#endif

#if DBDC_REPEATER_SUPPORT
            if (!ol_target_lithium(scn->soc->psoc_obj)) {
                if (dbdc_tx_process(vap, &osifp, skb)) {
                    dev_put(netdev);
                    return;
                }
            }
#endif
                osif_nss_vdev_process_extap_tx(netdev, skb);
                dev_put(netdev);
                return;
            } else {
                qdf_nbuf_free(skb);
                dev_put(netdev);
                return;
            }
            break;
        }

        case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_EXTAP_RX: {
            if (dp_is_extap_enabled(vdev) &&
                    (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)) {
                osif_nss_vdev_process_extap_rx_to_tx(netdev, skb);
                dev_put(netdev);
                return;
            } else {
                qdf_nbuf_free(skb);
                dev_put(netdev);
                return;
            }
            break;
        }

        case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_RX_ERR: {
            rx_err_metadata = (struct nss_wifi_vdev_rx_err_per_packet_metadata*)&wifi_metadata->metadata.rx_err_metadata;

            /*
             * Notify hostapd for WPA countermeasures
             */
            ol_rx_err((ol_pdev_handle)scn->sc_pdev, rx_err_metadata->vdev_id,
                        rx_err_metadata->peer_mac_addr,
                        rx_err_metadata->tid, 0, OL_RX_ERR_TKIP_MIC, skb);
            break;
        }

        case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_WNM_TFS: {
            osif_nss_vdev_process_wnm_tfs_tx(netdev, skb);
            break;
        }

        case NSS_WIFI_VDEV_EXT_TX_COMPL_PKT_TYPE: {

            tx_compl_metadata = (struct nss_wifi_vdev_tx_compl_metadata *)&wifi_metadata->metadata.tx_compl_metadata;
            if (ic->nss_iops) {
                ic->nss_iops->ic_osif_nss_ol_vdev_spl_receive_exttx_compl(netdev, skb, tx_compl_metadata);
            }
            break;
        }

    case NSS_WIFI_VDEV_EXT_DATA_PKT_TYPE_WDS_LEARN: {
        struct nss_wifi_vdev_wds_per_packet_metadata * wds_metadata = &wifi_metadata->metadata.wds_metadata;
        if (ic->nss_iops) {
            if (ic->nss_iops->ic_osif_nss_ol_vdev_spl_receive_extwds_data(netdev, skb, wds_metadata) == false) {
                qdf_nbuf_free(skb);
                dev_put(netdev);
                return;
            }
        }
        dev_hold(netdev);
        skb->dev = netdev;
        skb->protocol = eth_type_trans(skb, netdev);
        nbuf_debug_del_record(skb);
        napi_gro_receive(napi, skb);
        dev_put(netdev);

        break;
        }
        case NSS_WIFI_VDEV_EXT_DATA_PPDU_INFO: {
            struct nss_wifi_vdev_ppdu_metadata *ppdu_mdata = &wifi_metadata->metadata.ppdu_metadata;
            if ((ic->nss_iops) && (ppdu_mdata->dir == WIFI_VDEV_PPDU_MDATA_TX)) {
                if (ic->nss_iops->ic_osif_nss_ol_vdev_spl_receive_ppdu_metadata(netdev, skb, ppdu_mdata) == false) {
                    qdf_nbuf_free(skb);
                }

            } else {
                qdf_print("Rx: ppdu_id %d peer_id %d", ppdu_mdata->ppdu_id,  ppdu_mdata->peer_id);
                qdf_nbuf_free(skb);
            }
        }
        break;

        default:
            qdf_print("wrong special pkt type %d", wifi_metadata->pkt_type);
            qdf_nbuf_free(skb);
    }

    dev_put(netdev);
}

/*
 * osif_nss_vdev_data_wlan_hdr_get_saddr
 * Get source address from the packet
 */
static void osif_nss_vdev_data_wlan_hdr_get_saddr(struct sk_buff *skb, uint8_t *pkt_src_addr)
{
    struct ieee80211_qosframe_addr4 *wh_ptr = NULL;
    wh_ptr = (struct ieee80211_qosframe_addr4 *)qdf_nbuf_data(skb);

    /* We don't need to bother about whether the frame is QoS, here.
     * The QoS field occurs after the 4th address, and we don't access
     * the field.
     */
    switch (wh_ptr->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
        case IEEE80211_FC1_DIR_NODS:
            qdf_mem_copy(pkt_src_addr, wh_ptr->i_addr2, IEEE80211_ADDR_LEN);
            break;

        case IEEE80211_FC1_DIR_TODS:
            qdf_mem_copy(pkt_src_addr, wh_ptr->i_addr2, IEEE80211_ADDR_LEN);
            break;

        case IEEE80211_FC1_DIR_FROMDS:
            qdf_mem_copy(pkt_src_addr, wh_ptr->i_addr3, IEEE80211_ADDR_LEN);
            break;

        case IEEE80211_FC1_DIR_DSTODS:
            qdf_mem_copy(pkt_src_addr, wh_ptr->i_addr4, IEEE80211_ADDR_LEN);
            break;
    }
    return;
}

/*
 * osif_nss_vdev_data_receive()
 *	Handler for data packets exceptioned from WIFI
 */
static void
osif_nss_vdev_data_receive(struct net_device *dev, struct sk_buff *skb, __attribute__((unused)) struct napi_struct *napi)
{
    struct net_device *netdev;
    osif_dev  *osdev;
    struct ieee80211vap *vap = NULL;
#if ATH_SUPPORT_WRAP
#if DBDC_REPEATER_SUPPORT
    int nwifi;
#endif
#endif
    os_if_t osif = NULL;
    struct nss_wifi_vdev_rawmode_rx_metadata *wmrm = NULL;
    uint8_t is_chain = 0;
    void *peer = NULL;
    qdf_nbuf_t skb_head , skb_next;
    struct ieee80211com* ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    nss_tx_status_t nss_tx_status;

#ifdef WLAN_FEATURE_FASTPATH
    void *rx_mpdu_desc = NULL;
#endif
    uint8_t pkt_saddr[IEEE80211_ADDR_LEN];

    qdf_nbuf_t skb_list_head = skb;
    qdf_nbuf_t skb_list_tail = skb;

    nbuf_debug_add_record(skb);

    /*
     * Need to move this code to wifi driver
     */
    if(dev == NULL) {
        qdf_print(KERN_CRIT "%s , netdev is NULL, freeing skb", __func__);
        qdf_nbuf_free(skb);
        return;
    }

    netdev = (struct net_device *)dev;

    osdev = (osif_dev  *)ath_netdev_priv(netdev);
    vap = osdev->os_if;
    if(!vap) {
        qdf_nbuf_free(skb);
        return;
    }

    ic = vap->iv_ic;
    scn = OL_ATH_SOFTC_NET80211(ic);
    if(!scn) {
        qdf_print("\n Null Argument(scn) ");
        qdf_nbuf_free(skb);
        return;
    }

    /*
     * Forward the packet if vap inspection mode is enabled.
     *
     * NOTE: nss_redir_ctx should only NULL in normal data
     * exception path if its not vap inspection mode.
     */
    if (osdev->nss_redir_ctx) {
        nss_tx_status = nss_virt_if_tx_buf(osdev->nss_redir_ctx, skb) ;
        if (nss_tx_status != NSS_TX_SUCCESS) {
            qdf_print("could not send packet through redirect interface in vap inspection mode");
            qdf_nbuf_free(skb);
        }

        return;
    }

    /*
     * It is possible that invalid_peer, monitor_vap frames reach here through wifi exception
     * path and VAP is being deleted. Adding this check for protection against such scenarios.
     */
    if (wlan_vdev_get_dp_handle(osdev->ctrl_vdev) == NULL) {
        qdf_nbuf_free(skb);
        return;
    }
    osif = vap->iv_ifp;

    /*
     * non linear  skb with fraglist are processed in nss offload only if monitor mode is enabled
     * or decap type is raw otherwise linearize them
     */
    if (skb_is_nonlinear(skb)) {
        if (osif_nss_vdev_skb_needs_linearize(netdev, skb) && (__skb_linearize(skb))) {
            qdf_nbuf_free(skb);
            return;
        }
    }

    if (ic->nss_iops && ic->nss_iops->ic_osif_nss_ol_vdev_data_receive_meshmode_rxinfo) {
        ic->nss_iops->ic_osif_nss_ol_vdev_data_receive_meshmode_rxinfo(netdev, skb);
    }

#ifdef WLAN_FEATURE_FASTPATH
    if (scn->enable_statsv2) {
        rx_mpdu_desc = (void *)skb->head;
    if (ic->nss_iops)
        ic->nss_iops->ic_osif_nss_ol_vdev_update_statsv2(scn, skb, rx_mpdu_desc, 0);
    }
#endif

    /*
     * If MEC pkt on STA mode, drop the frame.
     */
    if (osdev->os_opmode == IEEE80211_M_STA) {
        if ((ic->nss_iops) && (ic->nss_iops->ic_osif_nss_ol_vdev_data_receive_mec_check(osdev, skb))) {
            qdf_nbuf_free(skb);
            return;
        }
    }

    /*
     * If decap mode is raw, remove the metadata
     */
    wmrm = (struct nss_wifi_vdev_rawmode_rx_metadata *)skb->data;

    qdf_nbuf_set_next(skb, NULL);

    if (skb_has_frag_list(skb)) {
        osif_nss_vdev_frag_to_chain(osdev, skb, &skb_list_head, &skb_list_tail);
        is_chain = 1;
    }

    if (ic->nss_iops) {
        if(ic->nss_iops->ic_osif_nss_ol_vdev_call_monitor_mode(netdev, osdev, skb_list_head, is_chain)) {
            return;
        }
    }

    if ((vap->iv_opmode == IEEE80211_M_MONITOR) || vap->iv_special_vap_mode || vap->iv_smart_monitor_vap) {
        if(is_chain) {
            skb_head = skb_list_head;
            while (skb_head) {
                skb_next = qdf_nbuf_next(skb_head);
                qdf_nbuf_free(skb_head);
                skb_head = skb_next;
            }
        } else {
            qdf_nbuf_free(skb);
        }
        return;
    }

    /*
     * Avoid skb cb area updates for raw frames as chfrag start and chfrag end fields
     * for raw frame indicate data boundaries
     */
    if((osdev->nssol_rxprehdr_read && ic->nss_radio_ops)&& (vap->iv_rx_decap_type != htt_cmn_pkt_type_raw)) {
        ic->nss_radio_ops->ic_nss_ol_read_pkt_prehdr(scn, skb);
    }

    if (vap->iv_rx_decap_type == htt_cmn_pkt_type_raw) {

        /*
         * If decap mode is raw, retrieve peer_id information as simulation code needs
         * peer pointer.
         */
        osif_nss_vdev_data_wlan_hdr_get_saddr(skb, &pkt_saddr[0]);

        if (ic->nss_iops) {
            peer = ic->nss_iops->ic_osif_nss_ol_peer_find_hash_find(vap, &pkt_saddr[0], 0);
        }

        if (!peer) {
            qdf_print("no peer available free packet \n");
            if (is_chain) {
                skb_head = skb_list_head;
                while (skb_head) {
                    skb_next = qdf_nbuf_next(skb_head);
                    qdf_nbuf_free(skb_head);
                    skb_head = skb_next;
                }
            } else {
                qdf_nbuf_free(skb);
            }
            return;
        }

#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
        if (vap->iv_rawmode_pkt_sim) {
            osif_rsim_rx_decap(osif, &skb_list_head, &skb_list_tail, (struct cdp_peer *) peer);
        }
#endif
        osif_nss_vdev_deliver_rawbuf(netdev, skb_list_head, napi);
        return;
    }
#if ATH_SUPPORT_WRAP
    if (wlan_is_wrap(vap)) {
        if(osif_ol_wrap_rx_process(&osdev, &netdev, vap, skb)) {
            return;
        }
#if DBDC_REPEATER_SUPPORT
        nwifi = ((osif_dev *)osif)->nss_nwifi;
        if (!ol_target_lithium(scn->soc->psoc_obj)) {
            if (dbdc_rx_process(&osif, &netdev, skb, &nwifi)) {
                return;
            }
        }
#endif
    }

#endif
    dev_hold(netdev);
    skb->dev = netdev;
    skb->protocol = eth_type_trans(skb, netdev);

    nbuf_debug_del_record(skb);
    napi_gro_receive(napi, skb);

    dev_put(netdev);
}

/*
 * osif_nss_vdev_cfg_callback()
 *  call back function for the vdev configuration handler
 */
void osif_nss_vdev_cfg_callback(void *app_data, struct nss_cmn_msg *msg) {

    struct nss_wifi_vdev_vow_dbg_stats *vow_dbg_stats;
    int count;

    switch (msg->type) {
        case NSS_WIFI_VDEV_INTERFACE_CONFIGURE_MSG:
            if (msg->response != NSS_CMN_RESPONSE_ACK) {
                qdf_print("VDEV configuration failed with error: %d", msg->error);
                osif_nss_vdev_cfgp.response = NSS_TX_FAILURE;
                complete(&osif_nss_vdev_cfgp.complete);
                return;
            }

            qdf_print("VDEV configuration success: %d", msg->error);
            osif_nss_vdev_cfgp.response = NSS_TX_SUCCESS;
            complete(&osif_nss_vdev_cfgp.complete);
            break;

        case NSS_WIFI_VDEV_VOW_DBG_STATS_REQ_MSG:
            if (msg->response != NSS_CMN_RESPONSE_ACK) {
                osif_nss_warn("VDEV VoW DBG stats failed with error: %d\n", msg->error);
                osif_nss_vdev_vowp.response = NSS_TX_FAILURE;
                complete(&osif_nss_vdev_vowp.complete);
                return;
            }

            vow_dbg_stats = &((struct nss_wifi_vdev_msg *)msg)->msg.vdev_vow_dbg_stats;
            osif_nss_vdev_vowp.rx_vow_dbg_counter = vow_dbg_stats->rx_vow_dbg_counters;
            for (count = 0; count < 8; count++) {
                osif_nss_vdev_vowp.tx_vow_dbg_counter[count] = vow_dbg_stats->tx_vow_dbg_counters[count];
            }

            osif_nss_vdev_vowp.response = NSS_TX_SUCCESS;
            complete(&osif_nss_vdev_vowp.complete);
            break;

        case NSS_WIFI_VDEV_QWRAP_PSTA_DELETE_ENTRY:
            if (msg->response != NSS_CMN_RESPONSE_ACK) {
                qdf_print(" MPSTA VDEV PSTA delete entry failed: %d", msg->error);
                return;
            }

            qdf_print(" MPSTA VDEV PSTA delete entry Success: %d", msg->error);
            break;

        case NSS_WIFI_VDEV_QWRAP_PSTA_ADD_ENTRY:
            if (msg->response != NSS_CMN_RESPONSE_ACK) {
                qdf_print(" MPSTA VDEV PSTA add entry failed: %d", msg->error);
                return;
            }

            qdf_print(" MPSTA VDEV PSTA add entry Success: %d", msg->error);
            break;

        case NSS_WIFI_VDEV_QWRAP_ISOLATION_ENABLE:
            if (msg->response != NSS_CMN_RESPONSE_ACK) {
                qdf_print(" Qwrap Isolation Enable failure: %d", msg->error);
                return;
            }

            qdf_print(" Qwrap Isolation Enable success: %d", msg->error);
            break;

    }
}

/*
 * osif_nss_vdev_recov_callback()
 *  call back function for the vdev recovery handler
 */
void osif_nss_vdev_recov_callback(void *app_data, struct nss_cmn_msg *msg)
{
    /*
     * Mark response based on ACK/NACK from NSS FW
     */
    if (msg->response != NSS_CMN_RESPONSE_ACK) {
        osif_nss_vdev_recovp.response = NSS_TX_FAILURE;
        complete(&osif_nss_vdev_recovp.complete);
        return;
    }

    osif_nss_vdev_recovp.response = NSS_TX_SUCCESS;
    complete(&osif_nss_vdev_recovp.complete);
}

/*
 * osif_nss_vdev_recovery_reconf()
 *  API for notifying the vdev interface reconfigure during recovery to NSS.
 */
static int32_t osif_nss_vdev_recovery_reconf(struct nss_ctx_instance *nss_wifiol_ctx, int32_t if_num)
{
    struct nss_wifi_vdev_msg wifivdevmsg;
    bool status;

    /*
     * Prepare recovery reconfigure msg to NSS
     */
    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_INTERFACE_RECOVERY_RECONF_MSG,
            sizeof(struct nss_wifi_vdev_recovery_msg), NULL, NULL);

    /*
     * Send the vdev recovery message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_wifiol_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the vdev recovery reconfigure message to NSS\n");
        return -1;

    }
    return 0;
}

/*
 * osif_nss_vdev_recovery_reset()
 *  API for notifying NSS to handle recovery interface deletion.
 */
static int32_t osif_nss_vdev_recovery_reset(struct nss_ctx_instance *nss_wifiol_ctx, int32_t if_num)
{
    struct nss_wifi_vdev_msg wifivdevmsg;
    bool status;
    int32_t ret;

    /*
     * Vdev recovery pvt for synchronous completion
     */
    init_completion(&osif_nss_vdev_recovp.complete);

    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_INTERFACE_RECOVERY_RESET_MSG,
            sizeof(struct nss_wifi_vdev_recovery_msg),
            (nss_wifi_vdev_msg_callback_t *)osif_nss_vdev_recov_callback, NULL);

    /*
     * Send the vdev recovery reset message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_wifiol_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the vdev recovery reset message to NSS\n");
        return -1;

    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     */
    ret = wait_for_completion_timeout(&osif_nss_vdev_recovp.complete,
                                        msecs_to_jiffies(OSIF_NSS_VDEV_RECOV_TIMEOUT_MS));
    if (ret == 0) {
        osif_nss_warn("Timeout waiting for vdev recovery reset msg\n");
        return -1;
    }

    /*
     * ACK/NACK received from NSS FW
     */
    if (NSS_TX_FAILURE == osif_nss_vdev_recovp.response) {
        osif_nss_warn("NACK received for vdev recovery reset msg\n");
        return -1;
    }
    return 0;
}

int32_t osif_nss_vdev_alloc(struct ol_ath_softc_net80211 *scn, struct ieee80211vap *vap, osif_dev *osifp)
{
    enum nss_dynamic_interface_type di_type;
    int32_t if_num;
    void *nss_wifiol_ctx = NULL;

    if((!scn) || (!vap)){
        qdf_print("\n Null Arguments(scn or Vap) ");
        return 0;
    }

    nss_wifiol_ctx = scn->nss_radio.nss_rctx;
    if (!nss_wifiol_ctx){
        qdf_print("\n Null Argument(nss_wifiol_ctx) ");
        return 0;
    }

    if (osifp->nss_recov_mode) {
            if_num = osifp->nss_recov_if;
            /*
             * Reset recovery interface and flag.
             */
            osifp->nss_recov_mode = false;
            osifp->nss_recov_if = -1;

            /*
             * Dynamic interface cannot be negative here.
             */
            qdf_assert(nss_is_dynamic_interface(if_num));

            /*
             * Send vdev interface alloc msg to NSS during recovery
             */
            if (osif_nss_vdev_recovery_reconf(nss_wifiol_ctx, if_num) < 0) {
                osif_nss_warn(" Msg sending failed to NSS FW: %d\n",if_num);
                return -1;
            }
            return if_num;
    }

#if ATH_SUPPORT_WRAP
    if (vap->iv_psta && !vap->iv_mpsta) {
        return NSS_PROXY_VAP_IF_NUMBER;
    }
#endif
    di_type = NSS_DYNAMIC_INTERFACE_TYPE_VAP;

    /*
     * Allocate a dynamic interface in NSS which represents the vdev
     */
    if_num = nss_dynamic_interface_alloc_node(di_type);
    if (if_num < 0) {
        osif_nss_warn(" di returned error : %d\n",if_num);
        return -1;
    }

    return if_num;
}
qdf_export_symbol(osif_nss_vdev_alloc);

int32_t osif_nss_ol_vap_create(void * vdev, struct ieee80211vap *vap, osif_dev *os_dev, uint32_t if_num)
{

    void *nss_wifiol_ctx;
    struct net_device *dev;
    uint32_t features = 0;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_config_msg *wifivdevcfg;
    enum nss_dynamic_interface_type di_type;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ieee80211com* ic = NULL;
    bool status;
    int32_t ret;
    uint8_t radio_ifnum;

    ic = vap->iv_ic;
    scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);
    if(!scn) {
        qdf_print("\n Null Argument(scn) ");
        return 0;
    }

    nss_wifiol_ctx = scn->nss_radio.nss_rctx;
    if (!nss_wifiol_ctx) {
        return 0;
    }

    os_dev->nss_wifiol_ctx = nss_wifiol_ctx;

    dev = OSIF_TO_NETDEV(os_dev);
#if ATH_SUPPORT_WRAP
    if (vap->iv_psta && !vap->iv_mpsta) {
        qdf_print("Dont create NSS vap for proxy ");
        os_dev->nss_ifnum = if_num;
        return 0;
    }
#endif
    qdf_print("NSS wifi offload VAP create IF %d nss_id %d ",
            if_num,scn->soc->nss_soc.nss_wifiol_id);

    /*
     * send raw send idx flush msg to NSS
     */
    if (scn->soc->nss_soc.ops && scn->soc->nss_soc.ops->nss_soc_ce_flush(scn->soc)) {
        qdf_print("%s: Failed to create vap \n", __func__);
        return -1;
    }

    di_type = NSS_DYNAMIC_INTERFACE_TYPE_VAP;

    /*
     * Initialize queue
     */
    spin_lock_init(&os_dev->queue_lock);
    __skb_queue_head_init(&os_dev->wifiol_txqueue);

    OS_INIT_TIMER(NULL, &os_dev->wifiol_stale_pkts_timer,
       osif_nss_vdev_stale_pkts_timer, (void *)os_dev, QDF_TIMER_TYPE_WAKE_APPS);

    os_dev->stale_pkts_timer_interval = OSIF_NSS_VDEV_STALE_PKT_INTERVAL;

    /*
     * Register the WIFI vap with NSS
     */
    status = nss_register_wifi_vdev_if(nss_wifiol_ctx, if_num,
            osif_nss_vdev_data_receive,
            osif_nss_vdev_special_data_receive,
            osif_nss_vdev_event_receive,
            dev,
            features);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to register the vap with nss\n");
        goto dealloc;
    }

    radio_ifnum = scn->nss_radio.nss_rifnum;
    if (!radio_ifnum) {
        osif_nss_warn("Unable to find radio instance in NSS\n");
        goto dealloc;
    }

    qdf_nofl_info("NSS radio_if %d", radio_ifnum);

    /*
     * Send the vap configure message down to NSS
     */
    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));
    wifivdevcfg = &wifivdevmsg.msg.vdev_config;
    wifivdevcfg->radio_ifnum = radio_ifnum;

#if MESH_MODE_SUPPORT
    wifivdevcfg->mesh_mode_en = vap->iv_mesh_vap_mode;
#endif

#if ATH_SUPPORT_WRAP
    wifivdevcfg->is_psta = vap->iv_psta;
    wifivdevcfg->is_mpsta = vap->iv_mpsta;
    wifivdevcfg->is_wrap = vap->iv_wrap;

    if (ic->nss_iops) {
        wifivdevcfg->is_nss_qwrap_en =
            ic->nss_iops->ic_osif_nss_ol_vdev_get_nss_qwrap_en(vap);
    }
#endif
    wifivdevcfg->special_vap_mode = vap->iv_special_vap_mode;
    wifivdevcfg->smartmesh_mode_en = vap->iv_smart_monitor_vap;

    if (ic->nss_iops)
        ic->nss_iops->ic_osif_nss_ol_vdevcfg_set_offload_params(vdev, &wifivdevcfg);

    init_completion(&osif_nss_vdev_cfgp.complete);
    /*
     * Send vdev configure command to NSS
     */
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_INTERFACE_CONFIGURE_MSG,
            sizeof(struct nss_wifi_vdev_config_msg), (nss_wifi_vdev_msg_callback_t *)osif_nss_vdev_cfg_callback, NULL);

    /*
     * Send the vdev configure message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_wifiol_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the vdev config message to NSS\n");
        goto unregister;
    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     */
    ret = wait_for_completion_timeout(&osif_nss_vdev_cfgp.complete, msecs_to_jiffies(OSIF_NSS_VDEV_CFG_TIMEOUT_MS));

    if (ret == 0) {
        osif_nss_warn("Waiting for vdev config msg ack timed out\n");
        goto unregister;
    }

    /*
     * ACK/NACK received from NSS FW
     */
    if (NSS_TX_FAILURE == osif_nss_vdev_cfgp.response) {

        osif_nss_warn("nack for vdev config msg\n");
        goto unregister;
    }

    os_dev->nss_ifnum = if_num;
    qdf_print("vap create %pK : if_num %d ", os_dev, os_dev->nss_ifnum );

    return if_num;

unregister:
    nss_unregister_wifi_vdev_if(if_num);

dealloc:
    if (nss_dynamic_interface_dealloc_node(if_num, di_type) != NSS_TX_SUCCESS) {
        qdf_print("%pK: Dynamic interface destroy %d failed", nss_wifiol_ctx, if_num);
    }
    return -1;
}
qdf_export_symbol(osif_nss_ol_vap_create);

/*
 * osif_nss_vdev_update_vlan()
 * 	API to update vlan to nss vdev.
 */
void osif_nss_vdev_update_vlan(struct ieee80211vap *vap, osif_dev *osif)
{
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_vlan_config_msg *wifivdevlanvcfg;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ieee80211com* ic = NULL;
    int32_t if_num;
    bool status;
    struct nss_ctx_instance *nss_wifiol_ctx;

    if (!osif) {
        return;
    }

    nss_wifiol_ctx = osif->nss_wifiol_ctx;
    if (!nss_wifiol_ctx) {
        return;
    }

    ic = vap->iv_ic;
    scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);
    if(!scn) {
        return;
    }
    if_num = osif->nss_ifnum;

    /*
     * Send the vlan configuration for this vap down to NSS
     */
    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));
    wifivdevlanvcfg = &wifivdevmsg.msg.vdev_vlan_config;
    wifivdevlanvcfg->vlan_id = osif->vlanID;
    /*
     * Send vdev configure command to NSS
     */
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_CONFIG_VLAN_ID_MSG,
            sizeof(struct nss_wifi_vdev_vlan_config_msg), (nss_wifi_vdev_msg_callback_t *)osif_nss_vdev_cfg_callback, NULL);

    /*
     * Send the vdev configure message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_wifiol_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the vdev vlan config message to NSS\n");
    }
}
qdf_export_symbol(osif_nss_vdev_update_vlan);

/*
 * osif_nss_vdev_set_vlan_type()
 * 	API to enable Default VLAN and
 * 	Port Based VLAN tagging support
 */
void osif_nss_vdev_set_vlan_type(osif_dev *osif, uint8_t default_vlan, uint8_t port_vlan)
{
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_vlan_enable_msg *wifivdevlanenb;
    int32_t if_num;
    bool status;
    struct nss_ctx_instance *nss_wifiol_ctx;

    if (!osif) {
        return ;
    }

    nss_wifiol_ctx = osif->nss_wifiol_ctx;
    if (!nss_wifiol_ctx) {
        return;
    }

    if_num = osif->nss_ifnum;

    /*
     * Send the vlan configuration for this vap down to NSS
     */
    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));
    wifivdevlanenb = &wifivdevmsg.msg.vdev_vlan_enable;
    wifivdevlanenb->vlan_tagging_mode = NSS_WIFI_VDEV_VLAN_NONE;

    if (default_vlan) {
        wifivdevlanenb->vlan_tagging_mode = NSS_WIFI_VDEV_VLAN_INGRESS_ADD_EGRESS_STRIP_ON_ID_MATCH;
    } else if (port_vlan) {
        wifivdevlanenb->vlan_tagging_mode = NSS_WIFI_VDEV_VLAN_INGRESS_ADD_EGRESS_STRIP_ALWAYS;
    }

    /*
     * Send vdev configure command to NSS
     */
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_CONFIG_VLAN_MODE_MSG,
            sizeof(struct nss_wifi_vdev_vlan_enable_msg), (nss_wifi_vdev_msg_callback_t *)osif_nss_vdev_cfg_callback, NULL);

    /*
     * Send the vdev configure message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_wifiol_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the vdev vlan enable message to NSS\n");
    }
}
qdf_export_symbol(osif_nss_vdev_set_vlan_type);
/*
 * osif_nss_vdev_create_redirect_if()
 *	API to create the redirect interface.
 */
static int32_t osif_nss_vdev_create_redirect_if(osif_dev *osifp, struct net_device *dev)
{
    /*
     * Creating redirect interface for vap inspection.
     */
    osifp->nss_redir_ctx = nss_virt_if_create_sync(dev);
    if (osifp->nss_redir_ctx == NULL) {
        qdf_print("%s: nss_redirect_ctx is null", __FUNCTION__);
        return -1;
    }

    return 0;
}

/*
 * osif_nss_vdev_del_redirect_if()
 *	API to delete the redirect interface.
 */
static int osif_nss_vdev_del_redirect_if(osif_dev *osifp)
{
    if (!osifp->nss_redir_ctx) {
        qdf_print("%s: nss_redirect_ctx is null", __FUNCTION__);
        return -1;
    }

    qdf_print("%s: detach vap redirect interface", __FUNCTION__);
    if (nss_virt_if_destroy_sync(osifp->nss_redir_ctx) != NSS_TX_SUCCESS) {
        qdf_print("%s: could not destroy vap redirect", __FUNCTION__);
        return -1;
    }
    osifp->nss_redir_ctx = NULL;
    return 0;
}


/*
 * osif_nss_vdev_detach()
 *	API for deleting nss interface for wifi vdev.
 */
static void osif_nss_vdev_detach(struct nss_ctx_instance *nss_wifiol_ctx, int32_t if_num)
{
    enum nss_dynamic_interface_type di_type;

    if (unlikely(if_num == NSS_PROXY_VAP_IF_NUMBER)) {
        qdf_print("interface number :%d not valid for dynamic interface node ", if_num);
        return;
    }

    di_type = NSS_DYNAMIC_INTERFACE_TYPE_VAP;

    qdf_print("Dealloc Dynamic interface Node :%d of type:%d", if_num, di_type);

    if (nss_dynamic_interface_dealloc_node(if_num, di_type) != NSS_TX_SUCCESS) {
        qdf_print("%pK: Dynamic interface destroy %d failed", nss_wifiol_ctx, if_num);
    }

    nss_unregister_wifi_vdev_if(if_num);
}

/*
 * osif_nss_vdev_dealloc()
 *	API for deallocating nss interface for wifi vdev.
 */
void osif_nss_vdev_dealloc(osif_dev *osif, int32_t if_num) {
    enum nss_dynamic_interface_type di_type;
    /* uint32_t radio_id; */

    if (unlikely(if_num == NSS_PROXY_VAP_IF_NUMBER)) {
        qdf_print("interface number :%d not valid for dynamic interface node ", if_num);
        return;
    }
    /* radio_id = osif->radio_id; */
    di_type = NSS_DYNAMIC_INTERFACE_TYPE_VAP;

    qdf_print("Dealloc Dynamic interface Node: %d of type: %d ", if_num, di_type);

    if (nss_dynamic_interface_dealloc_node(if_num, di_type) != NSS_TX_SUCCESS) {
        qdf_print("%pK: Dynamic interface %d destroy failed", osif, if_num);
    }
}

/*
 * osif_nss_vdev_xmit()
 *	API for sending the packet to radio.
 */
static enum osif_nss_error_types osif_nss_vdev_xmit(void *nss_wifiol_ctx, int32_t if_num, struct sk_buff *skb)
{
    nss_tx_status_t status = NSS_TX_SUCCESS;
    uint32_t needed_headroom = HTC_HTT_TRANSFER_HDRSIZE;

    /*
     * Check that valid interface number is passed in
     */
    BUG_ON(if_num == -1);
    if (unlikely(if_num == NSS_PROXY_VAP_IF_NUMBER)) {
        return OSIF_NSS_VDEV_XMIT_FAIL_PROXY_ARP;
    }

    /*
     * Sanity check the SKB to ensure that it's suitable for us
     */
    if (unlikely(skb->len <= ETH_HLEN)) {
        osif_nss_warn("%pK: Rx packet: %pK too short", nss_wifiol_ctx, skb);
        return OSIF_NSS_VDEV_XMIT_FAIL_MSG_TOO_SHORT;
    }

    /*
     * Check for minimum headroom availability
     */
    if ((skb_headroom(skb) < needed_headroom) && (skb->len > ETH_FRAME_LEN)) {
        if (pskb_expand_head(skb, needed_headroom, 0, GFP_ATOMIC)) {
            osif_nss_warn("%pK: Insufficient headroom %d needed %d", nss_wifiol_ctx, skb_headroom(skb), needed_headroom);
            return OSIF_NSS_VDEV_XMIT_FAIL_NO_HEADROOM;
        }
    }

    qdf_nbuf_set_next(skb, NULL);

    status = nss_wifi_vdev_tx_buf(nss_wifiol_ctx, skb, if_num);
    if (status != NSS_TX_SUCCESS) {
        return OSIF_NSS_VDEV_XMIT_FAIL_NSS_QUEUE_FULL;
    }

    return OSIF_NSS_VDEV_XMIT_SUCCESS;
}

/*
 * osif_nss_vdev_up()
 *	API for notifying the interface up status to NSS.
 */
static int32_t osif_nss_vdev_up(struct nss_ctx_instance *nss_wifiol_ctx, int32_t if_num, struct net_device *os_dev)
{
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_enable_msg *wifivdeven;
    bool status;

    /*
     * Send the vdev mac address to NSS
     */
    wifivdeven = &wifivdevmsg.msg.vdev_enable;
    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));
    memcpy(wifivdeven->mac_addr, os_dev->dev_addr, 6);

    /*
     * Send vdev configure command to NSS
     */
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_INTERFACE_UP_MSG,
            sizeof(struct nss_wifi_vdev_enable_msg), NULL, NULL);

    /*
     * Send the vdev configure message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_wifiol_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the vdev enable message to NSS\n");
        return -1;
    }
    return 0;
}

/*
 * osif_nss_vdev_down()
 *	API for notifying the interface down status to NSS.
 */
static int32_t osif_nss_vdev_down(struct nss_ctx_instance *nss_wifiol_ctx, int32_t if_num, struct net_device *os_dev)
{
    struct nss_wifi_vdev_msg wifivdevmsg;
    bool status;

    /*
     * Send the vdev mac address to NSS
     */
    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));

    /*
     * Send vdev configure command to NSS
     */
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_INTERFACE_DOWN_MSG,
            sizeof(struct nss_wifi_vdev_disable_msg), NULL, NULL);

    /*
     * Send the vdev configure message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_wifiol_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the vdev disable message to NSS\n");
        return -1;

    }

    return 0;
}

/*
 * osif_nss_vdev_send_cmd()
 *	API for notifying the vdev related command to NSS.
 */
void osif_nss_vdev_send_cmd(struct nss_ctx_instance *nss_wifiol_ctx, int32_t if_num,
        enum nss_wifi_vdev_cmd cmd, uint32_t value)
{
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_cmd_msg *wifivdevcmd;

    bool status;

    wifivdevcmd = &wifivdevmsg.msg.vdev_cmd;
    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));

    wifivdevcmd->cmd = cmd;
    wifivdevcmd->value = value;
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_INTERFACE_CMD_MSG,
            sizeof(struct nss_wifi_vdev_cmd_msg), NULL, NULL);

    /*
     * Send the vdev configure message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_wifiol_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the vdev command message to NSS\n");
    }
}

/*
 * osif_nss_ol_vap_delete
 */
int32_t osif_nss_ol_vap_delete(osif_dev *osif, uint8_t is_recovery)
{
    int32_t nss_ifnum = osif->nss_ifnum;
    void *nss_wifiol_ctx = osif->nss_wifiol_ctx;

    if (!nss_wifiol_ctx) {
        return 0;
    }

    if (nss_ifnum == -1 || (nss_ifnum >= NSS_MAX_NET_INTERFACES)) {
        qdf_print(" vap delete called in invalid interface");
        return -1;
    }

    /*
     * Delete the redirect interface created for vap inspection
     */
    if (osif->inspect_mode) {
        (void)osif_nss_vdev_del_redirect_if(osif);
    }

    qdf_print("vap detach %p: if_num %d ",osif, osif->nss_ifnum);

    if (is_recovery) {
        /*
         * NSS does not delete dynamic interface allocated
         * to vdev during recovery. Dynamic interface are stored
         * and provided during vdev create.
         */
        qdf_info("vap recovery %p: if_num %d \n",osif, osif->nss_ifnum);
        osif->nss_recov_mode = true;
        osif->nss_recov_if = nss_ifnum;
        osif_nss_vdev_recovery_reset(nss_wifiol_ctx, nss_ifnum);
    } else {
        /*
         * Attach the vdev with NSS and store the associated interface number
         * returned by NSS in VAP private info
         */
        osif_nss_vdev_detach(nss_wifiol_ctx,  nss_ifnum);
        osif->nss_ifnum = -1;
    }

    OS_FREE_TIMER(&osif->wifiol_stale_pkts_timer);

    /*
     * Empty the queue
     */
    spin_lock_bh(&osif->queue_lock);
    osif->stale_pkts_timer_interval = 0;
    __skb_queue_purge(&osif->wifiol_txqueue);
    spin_unlock_bh(&osif->queue_lock);

    return 0;
}
qdf_export_symbol(osif_nss_ol_vap_delete);

/*
 * osif_nss_ol_vap_up
 */
int32_t osif_nss_ol_vap_up(osif_dev *osif)
{
    int32_t nss_ifnum;
    void *nss_wifiol_ctx;
    struct net_device *dev;

    if (!osif) {
        qdf_print("%s() - NULL osif_dev pointer ", __FUNCTION__);
        return 0;
    }
    dev = OSIF_TO_NETDEV(osif);

    nss_wifiol_ctx = osif->nss_wifiol_ctx;
    if (!nss_wifiol_ctx) {
        return 0;
    }

    nss_ifnum = osif->nss_ifnum;
    if (osif->nss_ifnum == -1) {
        qdf_print("vap up called on invalid interface");
        return -1;
    }
    /*
     * Bring up the VAP interface
     */
    if (osif_nss_vdev_up(nss_wifiol_ctx, nss_ifnum, dev) ){
        return -1;
    }
    return 0;

}
qdf_export_symbol(osif_nss_ol_vap_up);

/*
 * osif_nss_ol_vap_down
 */
int32_t osif_nss_ol_vap_down(osif_dev *osif) {

    struct net_device *dev;
    int32_t nss_ifnum;
    void *nss_wifiol_ctx;

    if (!osif) {
        qdf_print("%s() - NULL osif_dev pointer ", __FUNCTION__);
        return 0;
    }
    dev = OSIF_TO_NETDEV(osif);

    nss_wifiol_ctx = osif->nss_wifiol_ctx;
    if (!nss_wifiol_ctx) {
        return 0;
    }

    nss_ifnum = osif->nss_ifnum;
    if ((nss_ifnum == -1) || (nss_ifnum == NSS_PROXY_VAP_IF_NUMBER)) {
        qdf_print("%s() vap down called on invalid interface nss_ifnum %d", __FUNCTION__, nss_ifnum);
        return -1;
    }

    /*
     * Bring down the VAP interface
     */
    osif_nss_vdev_down(nss_wifiol_ctx, nss_ifnum, dev);
    return 0;
}
qdf_export_symbol(osif_nss_ol_vap_down);

/*
 * osif_nss_ol_vap_xmit
 */
enum osif_nss_error_types osif_nss_ol_vap_xmit(osif_dev *osif, struct sk_buff *skb)
{
    int32_t nss_ifnum;
    void *nss_wifiol_ctx;

    if (!osif) {
        return OSIF_NSS_VDEV_XMIT_IF_NULL;
    }

    nss_wifiol_ctx = osif->nss_wifiol_ctx;
    if (!nss_wifiol_ctx) {
        return OSIF_NSS_VDEV_XMIT_CTX_NULL;
    }
    nss_ifnum = osif->nss_ifnum;
    if (nss_ifnum == -1) {
        qdf_print(" vap transmit called on invalid interface %pK ",osif);
        return OSIF_NSS_VDEV_XMIT_IF_INVALID;
    }

    nbuf_debug_del_record(skb);

    return osif_nss_vdev_xmit(nss_wifiol_ctx, nss_ifnum, skb);
}
qdf_export_symbol(osif_nss_ol_vap_xmit);

/*
 * osif_nss_vdev_set_cfg
 */
void osif_nss_vdev_set_cfg(void *osifp, enum osif_nss_vdev_cmd osif_cmd)
{
    struct net_device *dev;
    int32_t nss_ifnum;
    uint32_t val = 0;
    enum nss_wifi_vdev_cmd cmd = 0;
    void *nss_wifiol_ctx;
    struct ieee80211vap *vap = NULL;
    osif_dev *osif;
    struct ieee80211com* ic = NULL;
    struct wlan_objmgr_vdev *vdev;

    if (!osifp) {
        return;
    }

    osif = (osif_dev *)osifp;
    nss_wifiol_ctx = osif->nss_wifiol_ctx;
    if (!nss_wifiol_ctx) {
        return;
    }

    dev = OSIF_TO_NETDEV(osif);
    nss_ifnum = osif->nss_ifnum;
    vap = osif->os_if;
    ic = (struct ieee80211com *)vap->iv_ic;
    vdev = osif->ctrl_vdev;

    if (osif->nss_ifnum == -1) {
        qdf_print(" vap transmit called on invalid interface");
        return;
    }

    switch (osif_cmd) {
        case OSIF_NSS_VDEV_ENCAP_TYPE:
            cmd = NSS_WIFI_VDEV_ENCAP_TYPE_CMD;
            val = vap->iv_tx_encap_type;
            break;

        case OSIF_NSS_VDEV_DECAP_TYPE:
            cmd = NSS_WIFI_VDEV_DECAP_TYPE_CMD;
            val = vap->iv_rx_decap_type;
            break;

        case OSIF_NSS_VDEV_ENABLE_ME:
            cmd = NSS_WIFI_VDEV_ENABLE_ME_CMD;

            if (vap->iv_me->me_hifi_enable) {
                qdf_print("setting hifi mode 5");
                val = MC_HYFI_ENABLE;
            } else {
                val = osif->os_if->iv_me->mc_mcast_enable;
            }
            break;

        case OSIF_NSS_VDEV_EXTAP_CONFIG:
            cmd = NSS_WIFI_VDEV_EXTAP_CONFIG_CMD;
            val = dp_is_extap_enabled(vdev);
            break;

        case OSIF_NSS_WIFI_VDEV_CFG_BSTEER:
            cmd = NSS_WIFI_VDEV_CFG_BSTEER_CMD;
#if QCA_SUPPORT_SON
            val = wlan_son_is_vdev_event_enabled(vap->vdev_obj) ? 1 : 0;
#else
            val = 0;
#endif
            break;

        case OSIF_NSS_WIFI_VDEV_VOW_DBG_MODE:
            cmd = NSS_WIFI_VDEV_VOW_DBG_MODE_CMD;
            val = osif->vow_dbg_en;
            break;

        case OSIF_NSS_WIFI_VDEV_VOW_DBG_RST_STATS:
            cmd = NSS_WIFI_VDEV_VOW_DBG_RST_STATS_CMD;
            val = 0;
            break;

        case OSIF_NSS_WIFI_VDEV_CFG_DSCP_OVERRIDE:
            cmd = NSS_WIFI_VDEV_CFG_DSCP_OVERRIDE_CMD;
#if ATH_SUPPORT_DSCP_OVERRIDE
            if (vap->iv_dscp_map_id > 1) {
                val = 1;
            } else {
                val = vap->iv_dscp_map_id;
            }
#endif
            break;

        case OSIF_NSS_WIFI_VDEV_CFG_WNM_CAP:
            cmd = NSS_WIFI_VDEV_CFG_WNM_CAP_CMD;
            val = vap->iv_wnm;
            break;

        case OSIF_NSS_WIFI_VDEV_CFG_WNM_TFS:
            cmd = NSS_WIFI_VDEV_CFG_WNM_TFS_CMD;
            val = vap->iv_wnm;
            break;

        case OSIF_NSS_WIFI_VDEV_NAWDS_MODE:
            cmd = NSS_WIFI_VDEV_NAWDS_MODE_CMD;
            if (ic->nss_iops)
                 val = ic->nss_iops->ic_osif_nss_vdev_set_cfg(osif, osif_cmd);
            break;

        case OSIF_NSS_VDEV_DROP_UNENC:
            cmd = NSS_WIFI_VDEV_DROP_UNENC_CMD;
            if (ic->nss_iops)
                 val = ic->nss_iops->ic_osif_nss_vdev_set_cfg(osif, osif_cmd);
            break;

#ifdef WDS_VENDOR_EXTENSION
        case OSIF_NSS_WIFI_VDEV_WDS_EXT_ENABLE:
            cmd = NSS_WIFI_VDEV_CFG_WDS_EXT_ENABLE_CMD;
            if (ic->nss_iops)
                 val = ic->nss_iops->ic_osif_nss_vdev_set_cfg(osif, osif_cmd);
            break;
#endif

        case OSIF_NSS_VDEV_WDS_CFG:
            cmd = NSS_WIFI_VDEV_CFG_WDS_CMD;
            if (ic->nss_iops)
                val = ic->nss_iops->ic_osif_nss_vdev_set_cfg(osif, osif_cmd);
            break;

        case OSIF_NSS_VDEV_AP_BRIDGE_CFG:
            cmd = NSS_WIFI_VDEV_CFG_AP_BRIDGE_CMD;
            if (ic->nss_iops)
                val = ic->nss_iops->ic_osif_nss_vdev_set_cfg(osif, osif_cmd);
            break;

        case OSIF_NSS_VDEV_SECURITY_TYPE_CFG:
            cmd = NSS_WIFI_VDEV_SECURITY_TYPE_CMD;
            if (ic->nss_iops)
                val = ic->nss_iops->ic_osif_nss_vdev_set_cfg(osif, osif_cmd);
            break;

        case OSIF_NSS_VDEV_AST_OVERRIDE_CFG:
            cmd = NSS_WIFI_VDEV_CFG_AST_OVERRIDE_CMD;
            val = 1;
            break;

        case OSIF_NSS_WIFI_VDEV_CFG_SON_CAP:
            cmd = NSS_WIFI_VDEV_CFG_SON_CAP_CMD;
            val = wlan_get_param(vap, IEEE80211_CONFIG_FEATURE_SON_NUM_VAP) ? 1 : 0;
            break;

        default:
            qdf_print("Command :%d is not supported in NSS", cmd);
            return;
    }

    osif_nss_vdev_send_cmd(nss_wifiol_ctx, nss_ifnum, cmd, val);
}
qdf_export_symbol(osif_nss_vdev_set_cfg);

/*
 * osif_nss_vdev_get_nss_id
 *  Get the nss id of the vdev
 */
int osif_nss_vdev_get_nss_id( osif_dev *osifp)
{
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ieee80211vap *vap;
    int nss_id;

    if(!osifp) {
        return -1;
    }
    vap = osifp->os_if;
    scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);
    nss_id = scn->soc->nss_soc.nss_wifiol_id;
    return (nss_id) ;
}
qdf_export_symbol(osif_nss_vdev_get_nss_id);

/*
 * osif_vdev_get_nss_wifiol_ctx
 *  	Get the nss id of the vdev
 */
void *osif_nss_vdev_get_nss_wifiol_ctx(osif_dev *osifp)
{
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ieee80211vap *vap;
    void *nss_ctx;

    if(!osifp) {
        return NULL;
    }

    vap = osifp->os_if;
    scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);
    nss_ctx = scn->nss_radio.nss_rctx;
    return (nss_ctx);
}

/*
 * osif_nss_vdev_set_wifiol_ctx
 *	Set the nss ctx in osif_dev
 */
void *osif_nss_vdev_set_wifiol_ctx(struct ieee80211com *ic)
{
    struct ol_ath_softc_net80211 *scn = NULL;
    scn = OL_ATH_SOFTC_NET80211(ic);
    return scn->nss_radio.nss_rctx;
}
qdf_export_symbol(osif_nss_vdev_set_wifiol_ctx);
/*
 * osif_nss_vdev_me_create_grp_list()
 * 	API to notify grp_list creation in snooplist in NSS
 */
void osif_nss_vdev_me_create_grp_list(osif_dev *osifp, uint8_t *grp_addr, uint8_t *grp_ipaddr, uint32_t ether_type, uint32_t length)
{
    struct nss_ctx_instance *nss_ctx;
    int32_t if_num;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_me_snptbl_grp_create_msg *wifivdevsglc;
    bool status;
    int8_t *dest = NULL;
    struct ieee80211vap *vap = NULL;
    uint32_t *swap_ipv4;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }
    vap = osifp->os_if;
    if_num = osifp->nss_ifnum;

    wifivdevsglc = &wifivdevmsg.msg.vdev_grp_list_create;

    if (!grp_addr) {
        OS_MEMSET(wifivdevsglc->grp_addr, 0, IEEE80211_ADDR_LEN);
    } else {
        IEEE80211_ADDR_COPY(wifivdevsglc->grp_addr, grp_addr);
    }

    dest = (int8_t *)(&wifivdevsglc->u);

    if (ether_type == ETHERTYPE_IPV4) {
        OS_MEMCPY(dest, grp_ipaddr, IGMP_IP_ADDR_LENGTH);
        swap_ipv4 = (uint32_t *)dest;
        *swap_ipv4 = htonl(*swap_ipv4);
    } else {
        OS_MEMCPY(dest, grp_ipaddr, MLD_IP_ADDR_LENGTH);
    }

    wifivdevsglc->ether_type = ether_type;

    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_SNOOPLIST_GRP_LIST_CREATE_MSG,
                       sizeof(struct nss_wifi_vdev_me_snptbl_grp_create_msg), NULL, NULL);
    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("Unable to send the grp_list create message to NSS");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_me_create_grp_list);


/*
 * osif_nss_vdev_me_delete_grp_list()
 * 	API to notify grp_list deletion in snooplist in NSS
 */
void osif_nss_vdev_me_delete_grp_list(osif_dev *osifp, uint8_t *grp_addr, uint8_t *grp_ipaddr, uint32_t ether_type)
{
    struct nss_ctx_instance *nss_ctx;
    int32_t if_num;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_me_snptbl_grp_delete_msg *wifivdevsgld;
    int8_t *dest = NULL;
    bool status;
    uint32_t *swap_ipv4;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }

    if_num = osifp->nss_ifnum;
    wifivdevsgld = &wifivdevmsg.msg.vdev_grp_list_delete;

    IEEE80211_ADDR_COPY(wifivdevsgld->grp_addr, grp_addr);

    dest = (int8_t *)(&wifivdevsgld->u);
    if (ether_type == ETHERTYPE_IPV4) {
        OS_MEMCPY(dest, grp_ipaddr, IGMP_IP_ADDR_LENGTH);
        swap_ipv4 = (uint32_t *)dest;
        *swap_ipv4 = htonl(*swap_ipv4);
    } else {
        OS_MEMCPY(dest, grp_ipaddr, MLD_IP_ADDR_LENGTH);
    }

    wifivdevsgld->ether_type = ether_type;

    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_SNOOPLIST_GRP_LIST_DELETE_MSG,
                       sizeof(struct nss_wifi_vdev_me_snptbl_grp_delete_msg), NULL, NULL);

    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the grp_list delete message to NSS\n");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_me_delete_grp_list);

/*
 * osif_nss_vdev_me_add_member_list_pid()
 * 	API to notify grp_member addition to grp_list in NSS.
 */
void osif_nss_vdev_me_add_member_list_pid(osif_dev *osifp, struct MC_LIST_UPDATE* list_entry, uint32_t ether_type, uint32_t length, uint32_t peer_id)
{
    struct nss_ctx_instance *nss_ctx;
    int32_t if_num;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_me_snptbl_grp_mbr_add_msg *wifivdevsgma;
    int8_t *dest = NULL;
    int8_t *src = NULL;
    bool status;
    struct ieee80211vap *vap = NULL;
    struct ieee80211com* ic = NULL;
    uint32_t *swap_ipv4;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }

    if_num = osifp->nss_ifnum;
    vap = osifp->os_if;
    ic = vap->iv_ic;


    if (peer_id == OL_TXRX_INVALID_LOCAL_PEER_ID) {
        if (list_entry->ni == NULL) {
            qdf_print("listentry ni is NULL and peer is is invalid, hence returning");
            return;
        }
        if (ic->nss_iops)
            ic->nss_iops->ic_osif_nss_ol_get_peerid(list_entry, &peer_id);
    }

    wifivdevsgma = &wifivdevmsg.msg.vdev_grp_member_add;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    if (vap->iv_me->me_hifi_enable) {
        memcpy(wifivdevsgma->src_ip_addr, list_entry->srcs, IEEE80211_ME_HIFI_IP6_SIZE * IEEE80211_ME_HIFI_SRCS_MAX);
    } else
#endif
    {
        memcpy(wifivdevsgma->src_ip_addr, (uint8_t *)&list_entry->src_ip_addr, IEEE80211_IPV4_LEN);
    }

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    wifivdevsgma->nsrcs = list_entry->nsrcs;
#else
    wifivdevsgma->nsrcs = 0;
#endif
    dest = (int8_t *)(&wifivdevsgma->u);
    src = (int8_t *)(&list_entry->u);

    if (ether_type == ETHERTYPE_IPV4) {
        OS_MEMCPY(dest, src, IGMP_IP_ADDR_LENGTH);
        swap_ipv4 = (uint32_t *)dest;
        *swap_ipv4 = htonl(*swap_ipv4);
    } else {
        OS_MEMCPY(dest, src, MLD_IP_ADDR_LENGTH);
    }


    wifivdevsgma->peer_id = peer_id;
    wifivdevsgma->mode = list_entry->cmd;
    wifivdevsgma->ether_type = ether_type;
    IEEE80211_ADDR_COPY(wifivdevsgma->grp_addr, list_entry->grp_addr);
    IEEE80211_ADDR_COPY(wifivdevsgma->grp_member_addr, list_entry->grp_member);

    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_SNOOPLIST_GRP_MEMBER_ADD_MSG,
                       sizeof(struct nss_wifi_vdev_me_snptbl_grp_mbr_add_msg), NULL, NULL);

    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the grp_list add member message to NSS\n");
    }
}

void osif_nss_vdev_me_add_member_list(osif_dev *osifp, struct MC_LIST_UPDATE* list_entry, uint32_t ether_type, uint32_t length)
{
    osif_nss_vdev_me_add_member_list_pid(osifp, list_entry, ether_type, length, OL_TXRX_INVALID_LOCAL_PEER_ID);
}

EXPORT_SYMBOL(osif_nss_vdev_me_add_member_list);
/*
 * osif_nss_vdev_me_remove_member_list()
 * 	API_to notify grp_member got removed from grp_list in NSS.
 */
void osif_nss_vdev_me_remove_member_list(osif_dev *osifp, uint8_t *grp_ipaddr, uint8_t *grp_addr, uint8_t *grp_member_addr, uint32_t ether_type)
{
    struct nss_ctx_instance *nss_ctx;
    int32_t if_num;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_me_snptbl_grp_mbr_delete_msg *wifivdevsgmr;
    int8_t *dest = NULL;
    bool status;
    uint32_t *swap_ipv4;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }
    if_num = osifp->nss_ifnum;

    wifivdevsgmr = &wifivdevmsg.msg.vdev_grp_member_remove;
    dest = (int8_t *)(&wifivdevsgmr->u);


    if (ether_type == ETHERTYPE_IPV4) {
        OS_MEMCPY(dest, grp_ipaddr, IGMP_IP_ADDR_LENGTH);
        swap_ipv4 = (uint32_t *)dest;
        *swap_ipv4 = htonl(*swap_ipv4);
    } else {
        OS_MEMCPY(dest, grp_ipaddr, MLD_IP_ADDR_LENGTH);
    }

    wifivdevsgmr->ether_type = ether_type;
    IEEE80211_ADDR_COPY(wifivdevsgmr->grp_addr, grp_addr);
    IEEE80211_ADDR_COPY(wifivdevsgmr->grp_member_addr, grp_member_addr);

    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_SNOOPLIST_GRP_MEMBER_REMOVE_MSG,
                        sizeof(struct nss_wifi_vdev_me_snptbl_grp_mbr_delete_msg), NULL, NULL);

    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the grp_list remove member message to NSS\n");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_me_remove_member_list);

/*
 * osif_nss_vdev_me_update_member_list()
 * 	API to notify updation in grp_member to grp_list in NSS.
 */
void osif_nss_vdev_me_update_member_list(osif_dev *osifp, struct MC_LIST_UPDATE* list_entry, uint32_t ether_type)
{
    struct nss_ctx_instance *nss_ctx;
    int32_t if_num;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_me_snptbl_grp_mbr_update_msg *wifivdevsgmu;
    uint32_t length;
    int8_t *dest = NULL;
    int8_t *src = NULL;
    bool status;
    uint32_t *swap_ipv4;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }

    if_num = osifp->nss_ifnum;

    if (ether_type == ETHERTYPE_IPV4) {
        length = IGMP_IP_ADDR_LENGTH;
    } else if (ether_type == ETHERTYPE_IPV6) {
        length = MLD_IP_ADDR_LENGTH;
    } else {
        qdf_print("%s wrong ethertye %d",__FUNCTION__, ether_type);
        return;
    }

    wifivdevsgmu = &wifivdevmsg.msg.vdev_grp_member_update;

    dest = (int8_t *)(&wifivdevsgmu->u);
    src = (int8_t *)(&list_entry->u);

    if (ether_type == ETHERTYPE_IPV4) {
        OS_MEMCPY(dest, src, IGMP_IP_ADDR_LENGTH);
        swap_ipv4 = (uint32_t *)dest;
        *swap_ipv4 = htonl(*swap_ipv4);
    } else {
        OS_MEMCPY(dest, src, MLD_IP_ADDR_LENGTH);
    }

    wifivdevsgmu->ether_type = ether_type;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    memcpy(wifivdevsgmu->src_ip_addr, list_entry->srcs, IEEE80211_ME_HIFI_IP6_SIZE * IEEE80211_ME_HIFI_SRCS_MAX);
#endif
    wifivdevsgmu->mode = list_entry->cmd;
    IEEE80211_ADDR_COPY(wifivdevsgmu->grp_addr, list_entry->grp_addr);
    IEEE80211_ADDR_COPY(wifivdevsgmu->grp_member_addr, list_entry->grp_member);

    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_SNOOPLIST_GRP_MEMBER_UPDATE_MSG,
                        sizeof(struct nss_wifi_vdev_me_snptbl_grp_mbr_update_msg), NULL, NULL);

    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the grp_list update member message to NSS\n");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_me_update_member_list);

/*
 * osif_nss_vdev_me_add_deny_member()
 * 	API to notify addition of member in Deny list in NSS.
 */
void osif_nss_vdev_me_add_deny_member(osif_dev *osifp, uint32_t grp_ipaddr)
{
    struct nss_ctx_instance *nss_ctx;
    struct nss_wifi_vdev_msg wifivdevmsg;
    int32_t if_num;
    struct nss_wifi_vdev_me_snptbl_deny_grp_add_msg *wifivdevsdma;
    bool status;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }

    if_num = osifp->nss_ifnum;

    wifivdevsdma = &wifivdevmsg.msg.vdev_deny_member_add;

    wifivdevsdma->grpaddr = grp_ipaddr;

    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_SNOOPLIST_DENY_MEMBER_ADD_MSG,
                        sizeof(struct nss_wifi_vdev_me_snptbl_deny_grp_add_msg), NULL, NULL);

    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the grp_list deny member add message to NSS\n");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_me_add_deny_member);

/*
 * osif_nss_vdev_me_delete_deny_list()
 * 	API to notify Deletion of Deny table from Snooplist in NSS.
 *
 */
void osif_nss_vdev_me_delete_deny_list(osif_dev *osifp)
{
    struct nss_ctx_instance *nss_ctx;
    int32_t if_num;
    struct nss_wifi_vdev_msg wifivdevmsg;
    bool status;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }

    if_num = osifp->nss_ifnum;

    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_SNOOPLIST_DENY_LIST_DELETE_MSG, 0, NULL, NULL);

    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the grp_list deny table message to NSS\n");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_me_delete_deny_list);

/*
 * osif_nss_vdev_me_dump_snooplist()
 * 	API to notify dump snooplist in NSS.
 */
void osif_nss_vdev_me_dump_snooplist(osif_dev *osifp)
{
    struct nss_ctx_instance *nss_ctx;
    int32_t if_num;
    struct nss_wifi_vdev_msg wifivdevmsg;
    bool status;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }

    if_num = osifp->nss_ifnum;

    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_SNOOPLIST_DUMP_MSG, 0, NULL, NULL);

    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the grp_list dump message to NSS\n");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_me_dump_snooplist);

/*
 * osif_nss_vdev_me_dump_snooplist()
 * 	API to reset snooplist in NSS.
 */
void osif_nss_vdev_me_reset_snooplist(osif_dev *osifp)
{
    struct nss_ctx_instance *nss_ctx;
    int32_t if_num;
    struct nss_wifi_vdev_msg wifivdevmsg;
    bool status;
    struct ieee80211vap *vap = NULL;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }

    if_num = osifp->nss_ifnum;

    vap = osifp->os_if;
    if (!vap->iv_me) {
        return;
    }

    if ( !vap->iv_me->me_hifi_enable) {
        return;
    }

    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_SNOOPLIST_RESET_MSG, 0, NULL, NULL);

    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the grp_list reset message to NSS\n");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_me_reset_snooplist);

/*
 * osif_nss_vdev_me_toggle_snooplist()
 * 	API to toggle snooplist table in NSS.
 */
void osif_nss_vdev_me_toggle_snooplist(osif_dev *osifp)
{
    struct nss_ctx_instance *nss_ctx;
    int32_t if_num;
    struct nss_wifi_vdev_msg wifivdevmsg;
    bool status;
    struct ieee80211vap *vap = NULL;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }

    if_num = osifp->nss_ifnum;

    vap = osifp->os_if;
    if (!vap->iv_me) {
        return;
    }

    if ( !vap->iv_me->me_hifi_enable) {
        return;
    }

    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_SNOOPLIST_TOGGLE_MSG, 0, NULL, NULL);

    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        qdf_print("Unable to send toggle message to NSS");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_me_toggle_snooplist);

/*
 * osif_nss_vdev_me_update_hifitlb()
 *  API to update snooplist from hifi table.
 */
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
void osif_nss_vdev_me_update_hifitlb(osif_dev *osifp, struct ieee80211_me_hifi_table *table)
{
    void *nss_wifiol_ctx = NULL;
    struct ieee80211_me_hifi_entry *entry = NULL;
    struct ieee80211_me_hifi_group *group = NULL;
    struct MC_LIST_UPDATE member;
    int cnt, n;
    struct ieee80211vap *vap = NULL;
    struct ieee80211_node *ni = NULL;
    struct ieee80211com* ic = NULL;
    u_int32_t peer_id = OL_TXRX_INVALID_LOCAL_PEER_ID;
    u_int32_t length = 0;
    int8_t *dest = NULL;
    int8_t *src = NULL;
    u_int32_t ether_type;
    union {
        u_int32_t ip4;
        u_int8_t ip6[MLD_IP_ADDR_LENGTH];
    }u;

    if (!osifp) {
        return;
    }

    nss_wifiol_ctx = osifp->nss_wifiol_ctx;
    if (!nss_wifiol_ctx) {
        return;
    }
    vap = osifp->os_if;

    if (!vap->iv_me) {
        return;
    }
    if (!vap->iv_me->me_hifi_enable) {
        return;
    }
    ic = vap->iv_ic;

    for (cnt = 0; cnt < table->entry_cnt; cnt++) {
        entry = &table->entry[cnt];
        group = &entry->group;

        ether_type = ntohs(group->pro);
        if (ether_type == ETHERTYPE_IPV4) {
            length = IGMP_IP_ADDR_LENGTH;
            u.ip4 = ntohl(group->u.ip4);
        } else if (ether_type == ETHERTYPE_IPV6) {
            length = MLD_IP_ADDR_LENGTH;
            OS_MEMCPY(u.ip6, group->u.ip6, length);
        } else {
            qdf_print("protocol %d not supported for mcast enhancement in NSS", group->pro);
            continue;
        }
        osif_nss_vdev_me_create_grp_list(osifp, NULL, (uint8_t *)&u, ether_type, length);

        for (n = 0; n < entry->node_cnt; n++) {
            ni = ieee80211_vap_find_node(vap, entry->nodes[n].mac);
            if (!ni) {
                if (ic->nss_iops)
                    peer_id = ic->nss_iops->ic_osif_nss_ol_peerid_find_hash_find(vap, entry->nodes[n].mac, 1);
                if (peer_id == OL_TXRX_INVALID_LOCAL_PEER_ID) {
                    qdf_print("No peer found");
                    continue;
                }
            }

            memset(&member, 0, sizeof(struct MC_LIST_UPDATE));
            member.nsrcs = entry->nodes[n].nsrcs;
            memcpy(&member.srcs, entry->nodes[n].srcs, IEEE80211_ME_HIFI_IP6_SIZE * IEEE80211_ME_HIFI_SRCS_MAX);
            dest = (int8_t *)(&member.u);
            src = (int8_t *)(&u);
            OS_MEMCPY(dest, src, length);
            member.ni = ni;
            member.cmd = entry->nodes[n].filter_mode;
            OS_MEMSET(member.grp_addr, 0, IEEE80211_ADDR_LEN);
            IEEE80211_ADDR_COPY(member.grp_member, entry->nodes[n].mac);

            osif_nss_vdev_me_add_member_list_pid(osifp, &member, ether_type, length, peer_id);
            goto free_node;
free_node:
            /* remove extra node ref count added by find_node above */
            ieee80211_free_node(ni);
        }
    }
    osif_nss_vdev_me_toggle_snooplist(osifp);
}
EXPORT_SYMBOL(osif_nss_vdev_me_update_hifitlb);
#endif

/*
 * osif_nss_vdev_me_dump_denylist()
 * API to notify dump denylist in NSS.
 */
void osif_nss_vdev_me_dump_denylist(osif_dev *osifp)
{
    struct nss_ctx_instance *nss_ctx;
    int32_t if_num;
    struct nss_wifi_vdev_msg wifivdevmsg;
    bool status;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }
    if_num = osifp->nss_ifnum;

    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_SNOOPLIST_DENY_LIST_DUMP_MSG, 0, NULL, NULL);

    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the grp_list deny dump table message to NSS\n");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_me_dump_denylist);

/*
 * osif_nss_vdev_vow_dbg_cfg()
 * 	API to configure the peer MAC for which VoW debug stats will be incremented in NSS.
 */
void osif_nss_vdev_vow_dbg_cfg(osif_dev *osifp, int32_t vow_peer_list_idx)
{
    struct nss_ctx_instance *nss_ctx;
    int32_t if_num;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_vow_dbg_cfg_msg *wifivdevvdbgcfg;
    bool status;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }

    if_num = osifp->nss_ifnum;
    wifivdevvdbgcfg = &wifivdevmsg.msg.vdev_vow_dbg_cfg;
    wifivdevvdbgcfg->vow_peer_list_idx = vow_peer_list_idx;
    wifivdevvdbgcfg->tx_dbg_vow_peer_mac4 = osifp->tx_dbg_vow_peer[vow_peer_list_idx][0];
    wifivdevvdbgcfg->tx_dbg_vow_peer_mac5 = osifp->tx_dbg_vow_peer[vow_peer_list_idx][1];

    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_VOW_DBG_CFG_MSG, sizeof(struct nss_wifi_vdev_vow_dbg_cfg_msg), NULL, NULL);

    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the vow debug configuration to NSS\n");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_vow_dbg_cfg);

/*
 * osif_nss_vdev_vow_dbg_cfg()
 * 	API to configure the peer MAC for which VoW debug stats will be incremented in NSS.
 */
void osif_nss_vdev_get_vow_dbg_stats(osif_dev *osifp)
{
    struct nss_ctx_instance *nss_ctx;
    int32_t if_num;
    struct nss_wifi_vdev_msg wifivdevmsg;
    int ret, i;
    bool status;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }

    if_num = osifp->nss_ifnum;

    init_completion(&osif_nss_vdev_vowp.complete);
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_VOW_DBG_STATS_REQ_MSG,
            sizeof(struct nss_wifi_vdev_vow_dbg_stats), (nss_wifi_vdev_msg_callback_t *)osif_nss_vdev_cfg_callback, NULL);

    /*
     * Send the vdev snooplist message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the vow debug stats to NSS\n");
    }

    /*
     * Blocking call, wait till we get ACK for this msg.
     */
    ret = wait_for_completion_timeout(&osif_nss_vdev_vowp.complete, msecs_to_jiffies(OSIF_NSS_VDEV_CFG_TIMEOUT_MS));
    if (ret == 0) {
        osif_nss_warn("Waiting for vdev config msg ack timed out\n");
        return;
    }
    osifp->umac_vow_counter = osif_nss_vdev_vowp.rx_vow_dbg_counter;
    for (i = 0; i < 8; i++) {
        osifp->tx_dbg_vow_counter[i] = osif_nss_vdev_vowp.tx_vow_dbg_counter[i];
    }
}
EXPORT_SYMBOL(osif_nss_vdev_get_vow_dbg_stats);

/*
 * osif_nss_vdev_extap_table_entry_add()
 *  Extap vdev entry add message
 */
void osif_nss_vdev_extap_table_entry_add(osif_dev *osifp, uint16_t ip_version, uint8_t *ip,
        uint8_t *dest_mac)
{
    struct nss_ctx_instance *nss_ctx = NULL;
    int32_t if_num = 0;
    struct ieee80211vap *vap = NULL;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_extap_map *vdev_extap_map = NULL;
    bool status;
    uint8_t *ip_dest = NULL;
    uint8_t nullIP[QDF_IPV6_ADDR_SIZE] = {0};
    uint8_t cmp_sz = (ip_version == OSIF_NSS_EXTAP_ENTRY_IPV4) ? QDF_IPV4_ADDR_SIZE : QDF_IPV6_ADDR_SIZE;
    uint32_t *dip;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }

    vap = osifp->os_if;
    if (!vap) {
        return;
    }

    if_num = osifp->nss_ifnum;
    vdev_extap_map = &wifivdevmsg.msg.vdev_extap_map;
    if (!memcmp(ip, nullIP, cmp_sz)) {
        osif_nss_warn("Rejecting NULL ip addresses \n");
        return;
    }

    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));
    ip_dest = (uint8_t *)(&vdev_extap_map->u);
    vdev_extap_map->ip_version = ip_version;
    IEEE80211_ADDR_COPY(vdev_extap_map->h_dest, dest_mac);
    OS_MEMCPY(ip_dest, ip, cmp_sz);
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_EXTAP_ADD_ENTRY, sizeof(struct nss_wifi_vdev_extap_map), NULL, NULL);

    /*
     * Send the extap entry
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send extap add entry message to NSS\n");
    }

    dip = (uint32_t *)ip_dest;
    if (ip_version == OSIF_NSS_EXTAP_ENTRY_IPV4) {
        osif_nss_info("\n* EXTAP ADD-Entry: mac %s, IPv4 %lx *\n", ether_sprintf(vdev_extap_map->h_dest), *dip);
    } else {
        osif_nss_info("\n* EXTAP ADD-Entry: mac %s, IPv6 %lx:%lx:%lx:%x *\n", ether_sprintf(vdev_extap_map->h_dest), dip[0], dip[1], dip[2], dip[3]);
    }
}
EXPORT_SYMBOL(osif_nss_vdev_extap_table_entry_add);

/*
 * osif_nss_vdev_extap_table_entry_del()
 *  Extap vdev entry remove message
 */
void osif_nss_vdev_extap_table_entry_del(osif_dev *osifp, uint16_t ip_version, uint8_t *ip)
{
    struct nss_ctx_instance *nss_ctx = NULL;
    int32_t if_num = 0;
    struct ieee80211vap *vap = NULL;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_extap_map *vdev_extap_map = NULL;
    bool status;
    uint8_t *ip_dest = NULL;
    uint8_t nullIP[QDF_IPV6_ADDR_SIZE] = {0};
    uint8_t cmp_sz = (ip_version == OSIF_NSS_EXTAP_ENTRY_IPV4) ? QDF_IPV4_ADDR_SIZE : QDF_IPV6_ADDR_SIZE;
    uint32_t *dip;
    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }

    vap = osifp->os_if;
    if (!vap) {
        return;
    }

    if_num = osifp->nss_ifnum;
    vdev_extap_map = &wifivdevmsg.msg.vdev_extap_map;
    if (!memcmp(ip, nullIP, cmp_sz)) {
        qdf_print("Rejecting NULL ip addresses");
        return;
    }

    ip_dest = (uint8_t *)(&vdev_extap_map->u);
    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));
    vdev_extap_map->ip_version = ip_version;
    OS_MEMCPY(ip_dest, ip, cmp_sz);
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_EXTAP_REMOVE_ENTRY, sizeof(struct nss_wifi_vdev_extap_map), NULL, NULL);

    /*
     * Send the extap entry
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send extap remove entry message to NSS\n");
    }

    dip = (uint32_t *)ip_dest;
    if (ip_version == OSIF_NSS_EXTAP_ENTRY_IPV4) {
        qdf_print("\n* EXTAP DEL-Entry: IPv4 %lx *", ether_sprintf(vdev_extap_map->h_dest), *dip);
    } else {
        qdf_print("\n* EXTAP DEL-Entry: IPv6 %lx:%lx:%lx:%x *", ether_sprintf(vdev_extap_map->h_dest), dip[0],dip[1], dip[2], dip[3]);
    }
}
EXPORT_SYMBOL(osif_nss_vdev_extap_table_entry_del);

void osif_nss_vdev_stale_pkts_timer(void *arg)
{
    struct sk_buff *tmp = NULL;
    uint32_t queue_cnt = 0;
    enum osif_nss_error_types status;

    osif_dev *osifp = (osif_dev *)arg;

    spin_lock_bh(&osifp->queue_lock);

    if (osifp->stale_pkts_timer_interval == 0) {
        spin_unlock_bh(&osifp->queue_lock);
        return;
    }

    queue_cnt = skb_queue_len(&osifp->wifiol_txqueue);
    if (queue_cnt) {
        while ((tmp = __skb_dequeue(&osifp->wifiol_txqueue)) != NULL)
        {
            status = osif_nss_ol_vap_xmit(osifp, tmp);
            if (status == OSIF_NSS_VDEV_XMIT_FAIL_NSS_QUEUE_FULL) {
                break;
            }

            if (status != OSIF_NSS_VDEV_XMIT_SUCCESS) {
                dev_kfree_skb_any(tmp);
            }
        }
    }

    if (tmp) {
        __skb_queue_head(&osifp->wifiol_txqueue, tmp);
        OS_SET_TIMER(&osifp->wifiol_stale_pkts_timer, osifp->stale_pkts_timer_interval);
    }

    spin_unlock_bh(&osifp->queue_lock);
}

int32_t osif_nss_vdev_enqueue_packet_for_tx(osif_dev *osifp, struct sk_buff *skb)
{
    struct sk_buff *tmp = NULL;
    uint32_t queue_cnt = 0;
    void *nss_wifiol_ctx;
    enum osif_nss_error_types status;

    if (!osifp) {
        return -1;
    }

    nss_wifiol_ctx = osifp->nss_wifiol_ctx;
    if (!nss_wifiol_ctx) {
        return -1;
    }

    spin_lock_bh(&osifp->queue_lock);
    queue_cnt = skb_queue_len(&osifp->wifiol_txqueue);

    if (queue_cnt) {
        if (queue_cnt > OSIF_NSS_VDEV_PKT_QUEUE_LEN) {
            qdf_nbuf_free(skb);
        } else {
            __skb_queue_tail(&osifp->wifiol_txqueue, skb);
        }

        while ((tmp = __skb_dequeue(&osifp->wifiol_txqueue)) != NULL)
        {
            status = osif_nss_ol_vap_xmit(osifp, tmp);
            if (status == OSIF_NSS_VDEV_XMIT_FAIL_NSS_QUEUE_FULL) {
                break;
            }

            if (status != OSIF_NSS_VDEV_XMIT_SUCCESS) {
                dev_kfree_skb_any(tmp);
            }
        }

        if (tmp) {
            __skb_queue_head(&osifp->wifiol_txqueue, tmp);
            OS_SET_TIMER(&osifp->wifiol_stale_pkts_timer, osifp->stale_pkts_timer_interval);
        }

    } else {
        status = osif_nss_ol_vap_xmit(osifp, skb);
        if (status == OSIF_NSS_VDEV_XMIT_FAIL_NSS_QUEUE_FULL) {
            __skb_queue_tail(&osifp->wifiol_txqueue, skb);
            OS_SET_TIMER(&osifp->wifiol_stale_pkts_timer, osifp->stale_pkts_timer_interval);
        } else if (status != OSIF_NSS_VDEV_XMIT_SUCCESS) {
            dev_kfree_skb_any(skb);
        }
    }

    spin_unlock_bh(&osifp->queue_lock);
    return 0;
}

/*
 * osif_nss_vdev_psta_delete_entry()
 * 	Psta vdev entry delete message
 */
void osif_nss_vdev_psta_delete_entry(osif_dev *osif)
{
    struct nss_ctx_instance *nss_ctx = NULL;
    struct ieee80211vap *mpsta_vap = NULL;
    uint32_t mpsta_ifnum;
    osif_dev  *mpsta_osdev = NULL;
    struct ieee80211vap *vap = NULL;
    struct ieee80211com *ic = NULL;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_qwrap_psta_msg *psta_map_msg = NULL;
    struct net_device *netdev = NULL;
    bool status = false;

    if (!osif) {
        return;
    }

    netdev = OSIF_TO_NETDEV(osif);
    if (!netdev) {
        qdf_print("Get MPSTA: netdev is NULL");
        return;
    }

    vap = osif->os_if;
    if (!vap) {
        return;
    }

#if ATH_SUPPORT_WRAP
    mpsta_vap = vap->iv_ic->ic_mpsta_vap;
#endif
    if (!mpsta_vap) {
        qdf_print("Get MPSTA: mpsta_vap is NULL");
        return;
    }

    mpsta_osdev = (osif_dev *)mpsta_vap->iv_ifp;
    if (!mpsta_osdev) {
        qdf_print("Get MPSTA: mpsta_osdev is NULL");
	return;
    }

    mpsta_ifnum = mpsta_osdev->nss_ifnum;
    nss_ctx = mpsta_osdev->nss_wifiol_ctx;
    if (!nss_ctx) {
        qdf_print("Get MPSTA: nss_ctx is NULL");
        return;
    }

    ic = mpsta_vap->iv_ic;
    if (!ic) {
        return;
    }

    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));
    psta_map_msg = &wifivdevmsg.msg.vdev_qwrap_psta_map;
#if ATH_SUPPORT_WRAP
    psta_map_msg->is_wired = vap->iv_wired_pvap;
    memcpy(&psta_map_msg->oma[0], &osif->osif_dev_oma[0], 6);
    memcpy(&psta_map_msg->vma[0], &osif->osif_dev_vma[0], 6);
#endif

    if (ic->nss_iops) {
        if (!ic->nss_iops->ic_osif_nss_ol_get_vdevid(osif, (uint8_t *)&psta_map_msg->vdev_id)) {
            qdf_print("osif_nss_ol_get_vdevid failed ");
            return;
        }
    }

    nss_wifi_vdev_msg_init(&wifivdevmsg, mpsta_ifnum, NSS_WIFI_VDEV_QWRAP_PSTA_DELETE_ENTRY,
        sizeof(struct nss_wifi_vdev_qwrap_psta_msg), (nss_wifi_vdev_msg_callback_t *)osif_nss_vdev_cfg_callback, NULL);

    /*
     * Send the dscp to tid map id message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the dscp-tid map message to NSS\n");
    }

    qdf_print("\n*****PSTA Del-Entry oma %pM, vma %pM*****", psta_map_msg->oma, psta_map_msg->vma);
}
EXPORT_SYMBOL(osif_nss_vdev_psta_delete_entry);

/*
 * osif_nss_vdev_psta_add_entry()
 * 	Psta vdev entry add message
 */
bool osif_nss_vdev_psta_add_entry(osif_dev *osif)
{
    struct nss_ctx_instance *nss_ctx = NULL;
    struct ieee80211vap *mpsta_vap = NULL;
    uint32_t mpsta_ifnum;
    osif_dev  *mpsta_osdev = NULL;
    struct ieee80211com *ic = NULL;
    struct ieee80211vap *vap = NULL;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_qwrap_psta_msg *psta_map_msg = NULL;
    struct net_device *netdev = NULL;
    bool status = false;

    if (!osif) {
        return status;
    }

    vap = osif->os_if;
    if (!vap) {
        return status;
    }

    netdev = OSIF_TO_NETDEV(osif);
    if (!netdev) {
        qdf_print("Get MPSTA: netdev is NULL");
        return status;
    }

#if ATH_SUPPORT_WRAP
    mpsta_vap = vap->iv_ic->ic_mpsta_vap;
#endif
    if (!mpsta_vap) {
        qdf_print("Get MPSTA: mpsta_vap is NULL");
        return status;
    }

    mpsta_osdev = (osif_dev *)mpsta_vap->iv_ifp;
    if (!mpsta_osdev) {
        qdf_print("Get MPSTA: mpsta_osdev is NULL");
        return status;
    }

    mpsta_ifnum = mpsta_osdev->nss_ifnum;
    nss_ctx = mpsta_osdev->nss_wifiol_ctx;
    if (!nss_ctx) {
        qdf_print("Get MPSTA: nss_ctx is NULL");
        return status;
    }

    ic = mpsta_vap->iv_ic;
    if (!ic) {
        return status;
    }

    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));
    psta_map_msg = &wifivdevmsg.msg.vdev_qwrap_psta_map;
#if ATH_SUPPORT_WRAP
    psta_map_msg->is_wired = vap->iv_wired_pvap;
    memcpy(&psta_map_msg->oma[0], &osif->osif_dev_oma[0], 6);
    memcpy(&psta_map_msg->vma[0], &osif->osif_dev_vma[0], 6);
#endif

    if (ic->nss_iops) {
        if (!ic->nss_iops->ic_osif_nss_ol_get_vdevid(osif, (uint8_t *)&psta_map_msg->vdev_id)) {
            qdf_print("osif_nss_ol_get_vdevid failed ");
            return status;
        }
    }

    nss_wifi_vdev_msg_init(&wifivdevmsg, mpsta_ifnum, NSS_WIFI_VDEV_QWRAP_PSTA_ADD_ENTRY,
        sizeof(struct nss_wifi_vdev_qwrap_psta_msg), (nss_wifi_vdev_msg_callback_t *)osif_nss_vdev_cfg_callback, NULL);

    /*
     * Send the dscp to tid map id message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send psta add entry message to NSS\n");
    }

    qdf_print("\n*****PSTA ADD-Entry: oma %pM, vma %pM*****", psta_map_msg->oma, psta_map_msg->vma);

    return status;
}
EXPORT_SYMBOL(osif_nss_vdev_psta_add_entry);

/*
 * osif_nss_vdev_qwrap_isolation_enable()
 * 	Qwrap enable isolation mode
 */
bool osif_nss_vdev_qwrap_isolation_enable(osif_dev *osif)
{
    struct nss_ctx_instance *nss_ctx = NULL;
    struct ieee80211vap *mpsta_vap = NULL;
    uint32_t mpsta_ifnum;
    osif_dev  *mpsta_osdev = NULL;
    struct ieee80211com *ic = NULL;
    struct ieee80211vap *vap = NULL;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_qwrap_isolation_en_msg *qwrap_isolation_en = NULL;
    struct net_device *netdev = NULL;
    bool status = false;

    if (!osif) {
        return status;
    }

    vap = osif->os_if;
    if (!vap) {
        return status;
    }

    netdev = OSIF_TO_NETDEV(osif);
    if (!netdev) {
        qdf_print("Get MPSTA: netdev is NULL");
        return status;
    }

#if ATH_SUPPORT_WRAP
    mpsta_vap = vap->iv_ic->ic_mpsta_vap;
#endif
    if (!mpsta_vap) {
        qdf_print("Get MPSTA: mpsta_vap is NULL");
        return status;
    }

    mpsta_osdev = (osif_dev *)mpsta_vap->iv_ifp;
    if (!mpsta_osdev) {
        qdf_print("Get MPSTA: mpsta_osdev is NULL");
        return status;
    }

    mpsta_ifnum = mpsta_osdev->nss_ifnum;
    nss_ctx = mpsta_osdev->nss_wifiol_ctx;
    if (!nss_ctx) {
        qdf_print("Get MPSTA: nss_ctx is NULL");
        return status;
    }

    ic = mpsta_vap->iv_ic;
    if (!ic) {
        return status;
    }

    memset(&wifivdevmsg, 0, sizeof(struct nss_wifi_vdev_msg));
    qwrap_isolation_en = &wifivdevmsg.msg.vdev_qwrap_isolation_en;
    qwrap_isolation_en->isolation_enable = 1;

    nss_wifi_vdev_msg_init(&wifivdevmsg, mpsta_ifnum, NSS_WIFI_VDEV_QWRAP_ISOLATION_ENABLE,
        sizeof(struct nss_wifi_vdev_qwrap_isolation_en_msg), (nss_wifi_vdev_msg_callback_t *)osif_nss_vdev_cfg_callback, NULL);

    /*
     * Send the dscp to tid map id message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send psta add entry message to NSS\n");
    }

    qdf_print("\n*****Qwrap Isolation Enable Msg sent *****");

    return status;
}
EXPORT_SYMBOL(osif_nss_vdev_qwrap_isolation_enable);

void osif_nss_vdev_process_mpsta_rx_to_tx(struct net_device *netdev, struct sk_buff *skb, __attribute__((unused)) struct napi_struct *napi)
{
    osif_dev *osifp;
    osif_dev *osifpmpsta;
    struct ieee80211vap *pstavap;
    struct ieee80211com *ic = NULL;

    struct nss_wifi_vdev_mpsta_per_packet_tx_metadata *nwtm;

    osifp = netdev_priv(netdev);
    pstavap = osifp->os_if;
    osifpmpsta = osifp; /*Store the present osifp*/
    ic = pstavap->iv_ic;

#if ATH_SUPPORT_WRAP
    if (!pstavap->iv_mpsta) {
        skb->protocol = eth_type_trans(skb, netdev);
        nbuf_debug_del_record(skb);
        napi_gro_receive(napi, skb);
        return;
    }
#endif
    if (unlikely(skb_headroom(skb) < sizeof(struct nss_wifi_vdev_mpsta_per_packet_tx_metadata))){
        if(pskb_expand_head(skb, sizeof(struct nss_wifi_vdev_mpsta_per_packet_tx_metadata), 0, GFP_ATOMIC)){
            qdf_print("Unable to allocated skb data%pK head%pK tail%pK end %pK", skb->data, skb->head, skb->tail, skb->end);
            qdf_nbuf_free(skb);
            return ;
        }
    }

    nwtm = (struct nss_wifi_vdev_mpsta_per_packet_tx_metadata *) skb_push(skb, sizeof(struct nss_wifi_vdev_mpsta_per_packet_tx_metadata));
    if (ic->nss_iops) {
        if (!ic->nss_iops->ic_osif_nss_ol_get_vdevid(osifp, (uint8_t *)&nwtm->vdev_id)) {
            qdf_print("osif_nss_ol_get_vdevid failed ");
            qdf_nbuf_free(skb);
            return;
        }
    }
    nwtm->metadata_type = NSS_WIFI_VDEV_QWRAP_TYPE_RX_TO_TX;

    osif_nss_vdev_enqueue_packet_for_tx(osifpmpsta, skb);
}

void osif_nss_vdev_process_extap_rx_to_tx(struct net_device *netdev, struct sk_buff *skb)
{
    osif_dev *osifp;
#if DBDC_REPEATER_SUPPORT
    wlan_if_t vap = NULL;
    os_if_t osif = NULL;
    int nwifi;
    struct ol_ath_softc_net80211 *scn;
    struct ol_ath_vap_net80211 *avn;
    struct wlan_objmgr_vdev *vdev = NULL;
#endif
    struct nss_wifi_vdev_extap_per_packet_metadata *rxtm = NULL;

    osifp = netdev_priv(netdev);
    if (osif_nss_ol_extap_rx(netdev, skb)) {
        qdf_nbuf_free(skb);
        return;
    }

#if DBDC_REPEATER_SUPPORT
    vap = osifp->os_if;
    osif = vap->iv_ifp;
    vdev = osifp->ctrl_vdev;
    nwifi = ((osif_dev *)osif)->nss_nwifi;
    avn = OL_ATH_VAP_NET80211(vap);
    scn = avn->av_sc;
    if (!ol_target_lithium(scn->soc->psoc_obj)) {
        if (dbdc_rx_process(&osif, &netdev, skb, &nwifi)) {
            return;
        }
    } else {
        if (dp_lag_is_enabled(vdev)) {
            if(dp_lag_rx_process(vdev, skb, DP_LAG_SEC_SKIP_VAP_SEND)) {
                return;
            }
        }
    }
#endif

    if (unlikely(skb_headroom(skb) < sizeof(struct nss_wifi_vdev_extap_per_packet_metadata))){
        if(pskb_expand_head(skb, sizeof(struct nss_wifi_vdev_extap_per_packet_metadata), 0, GFP_ATOMIC)){
            qdf_print("Unable to allocated skb data%pK head%pK tail%pK end %pK", skb->data, skb->head, skb->tail, skb->end);
            qdf_nbuf_free(skb);
            return ;
        }
    }

    rxtm = (struct nss_wifi_vdev_extap_per_packet_metadata *) skb_push(skb, sizeof(struct nss_wifi_vdev_extap_per_packet_metadata));
    rxtm->pkt_type = NSS_WIFI_VDEV_EXTAP_PKT_TYPE_RX_TO_TX;

    osif_nss_vdev_enqueue_packet_for_tx(osifp, skb);
}

void osif_nss_vdev_process_extap_tx(struct net_device *netdev, struct sk_buff *skb)
{
    osif_dev *osifp;
    struct nss_wifi_vdev_extap_per_packet_metadata *txtm = NULL;

    osifp = netdev_priv(netdev);
    if (osif_nss_ol_extap_tx(netdev, skb)) {
        qdf_nbuf_free(skb);
        return;
    }

    if (unlikely(skb_headroom(skb) < sizeof(struct nss_wifi_vdev_extap_per_packet_metadata))){
        if(pskb_expand_head(skb, sizeof(struct nss_wifi_vdev_extap_per_packet_metadata), 0, GFP_ATOMIC)){
            qdf_print("Unable to allocated skb data%pK head%pK tail%pK end %pK", skb->data, skb->head, skb->tail, skb->end);
            qdf_nbuf_free(skb);
            return ;
        }
    }

    txtm = (struct nss_wifi_vdev_extap_per_packet_metadata *) skb_push(skb, sizeof(struct nss_wifi_vdev_extap_per_packet_metadata));
    txtm->pkt_type = NSS_WIFI_VDEV_EXTAP_PKT_TYPE_TX;

    osif_nss_vdev_enqueue_packet_for_tx(osifp, skb);
}
qdf_export_symbol(osif_nss_vdev_process_extap_tx);

void osif_nss_vdev_process_mpsta_tx(struct net_device *netdev, struct sk_buff *skb)
{
    osif_dev *osifp;
    osif_dev *osifpmpsta = NULL;
    struct ieee80211vap *vap;
    uint8_t vdev_id = 0;
    struct nss_wifi_vdev_mpsta_per_packet_tx_metadata *nwtm;
    struct ol_ath_softc_net80211 *scn = NULL;
    struct ieee80211com* ic = NULL;

    osifp = netdev_priv(netdev);
    vap = osifp->os_if;
    ic = vap->iv_ic;
    scn = OL_ATH_SOFTC_NET80211(ic);

#if ATH_SUPPORT_WRAP
    if (vap->iv_psta  && !vap->iv_mpsta){
        struct ieee80211vap *mvap;

        qdf_spin_lock_bh(&scn->sc_mpsta_vap_lock);
        mvap = scn->sc_mcast_recv_vap;
        if (!mvap) {
            qdf_spin_unlock_bh(&scn->sc_mpsta_vap_lock);
            qdf_nbuf_free(skb);
            return;
        }

        osifpmpsta  = (osif_dev *)mvap->iv_ifp;
        if (!osifpmpsta) {
            qdf_spin_unlock_bh(&scn->sc_mpsta_vap_lock);
            qdf_nbuf_free(skb);
            return;
        }
        qdf_spin_unlock_bh(&scn->sc_mpsta_vap_lock);
    } else  {
        osifpmpsta = osifp; /*Store the present osifp*/
        if (osif_ol_wrap_tx_process(&osifp, vap, &skb)) {
            if (skb) {
                qdf_nbuf_free(skb);
            }
            return;
        }
    }
#endif
    if (unlikely(skb_headroom(skb) < sizeof(struct nss_wifi_vdev_mpsta_per_packet_tx_metadata))){

        if(pskb_expand_head(skb, sizeof(struct nss_wifi_vdev_mpsta_per_packet_tx_metadata), 0, GFP_ATOMIC)){
            qdf_print("Unable to allocated skb data%pK head%pK tail%pK end %pK", skb->data, skb->head, skb->tail, skb->end);
            qdf_nbuf_free(skb);
            return ;
        }
    }

    nwtm = (struct nss_wifi_vdev_mpsta_per_packet_tx_metadata *) skb_push(skb, sizeof(struct nss_wifi_vdev_mpsta_per_packet_tx_metadata));

    if (ic->nss_iops) {
        if (!ic->nss_iops->ic_osif_nss_ol_get_vdevid(osifp, &vdev_id)) {
            qdf_nbuf_free(skb);
            return;
        }
    }
    nwtm->vdev_id = vdev_id;
    nwtm->metadata_type = NSS_WIFI_VDEV_QWRAP_TYPE_TX;

    if (osif_nss_vdev_enqueue_packet_for_tx(osifpmpsta, skb) != 0) {
        qdf_print("Unable to enqueue: Freeing the packet ");
        qdf_nbuf_free(skb);
        return;
    }

}
EXPORT_SYMBOL(osif_nss_vdev_process_mpsta_tx);

struct net_device *osif_nss_vdev_process_mpsta_rx(struct net_device *netdev, struct sk_buff *skb, uint32_t peer_id)
{
    osif_dev *osifp;
    struct ieee80211vap *vap;
    struct net_device *pst_netdev = NULL;
    struct ieee80211vap *pst_vap = NULL;
    osif_dev *pst_osifp = NULL;
    struct ieee80211com* ic = NULL;

#if DBDC_REPEATER_SUPPORT
    os_if_t osif = NULL;
    int nwifi;
    struct wlan_objmgr_vdev *psta_vdev = NULL;
    struct ol_ath_softc_net80211 *scn;
    struct ol_ath_vap_net80211 *avn;
#endif

    osifp = netdev_priv(netdev);
    vap = osifp->os_if;
    ic = vap->iv_ic;
#if DBDC_REPEATER_SUPPORT
    avn = OL_ATH_VAP_NET80211(vap);
    scn = avn->av_sc;
#endif

    if (ic->nss_iops) {
        if (ic->nss_iops->ic_osif_nss_ol_vdev_qwrap_mec_check(osifp, skb)) {
            qdf_nbuf_free(skb);
            return NULL;
        }

        if (!ic->nss_iops->ic_osif_nss_ol_find_pstosif_by_id(netdev, peer_id, &pst_osifp)) {
            qdf_nbuf_free(skb);
            return NULL;
        }
    }

    if (!pst_osifp) {
        qdf_print("no vdev available free packet ");
        qdf_nbuf_free(skb);
        return NULL;
    }

    pst_vap =  pst_osifp->os_if;
    pst_netdev = pst_osifp->netdev;
    skb->dev = pst_netdev;
    /*
     * When return value is 1 packet is already sent out
     */
#if ATH_SUPPORT_WRAP
    if(osif_ol_wrap_rx_process(&osifp, &pst_netdev, pst_vap, skb)) {
        return NULL;
    }
#endif

#if DBDC_REPEATER_SUPPORT
    osif = pst_vap->iv_ifp;
    psta_vdev = pst_osifp->ctrl_vdev;
    nwifi = ((osif_dev *)osif)->nss_nwifi;

    /*
     * Do DBDC processing only for legacy.
     * For Hawkeye in case of MPSTA , DBDC processing is not required
     * as Qwrap processing takes care of dropping the packet in oma/vma
     * is not found in its own table.
     */
    if (!ol_target_lithium(scn->soc->psoc_obj)) {
        if(dbdc_rx_process(&osif, &pst_netdev, skb, &nwifi)) {
            return NULL;
        }
    }
#endif
    return  pst_netdev;
}

/*
 * osif_nss_vdev_set_dscp_tid_map()
 * 	API to notify dscp-tid mapping in NSS.
 */
void osif_nss_vdev_set_dscp_tid_map(osif_dev *osifp, uint32_t* dscp_map)
{
    struct nss_ctx_instance *nss_ctx = NULL;
    int32_t if_num = 0;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_dscp_tid_map *dscpmap_msg = &wifivdevmsg.msg.vdev_dscp_tid_map;
    bool status;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }
    if_num = osifp->nss_ifnum;

    memcpy(dscpmap_msg->dscp_tid_map, dscp_map, sizeof (dscpmap_msg->dscp_tid_map));
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_DSCP_TID_MAP_MSG, sizeof(struct nss_wifi_vdev_dscp_tid_map), NULL, NULL);

    /*
     * Send the dscp to tid map message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the dscp-tid map message to NSS\n");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_set_dscp_tid_map);

/*
 * osif_nss_vdev_set_dscp_tid_map_id()
 * 	API to notify dscp-tid mapping in NSS.
 */
void osif_nss_vdev_set_dscp_tid_map_id(osif_dev *osifp, uint8_t map_id)
{
    struct nss_ctx_instance *nss_ctx = NULL;
    int32_t if_num = 0;
    struct nss_wifi_vdev_msg wifivdevmsg;
    struct nss_wifi_vdev_dscptid_map_id *dscpmapid_msg = &wifivdevmsg.msg.vdev_dscp_tid_map_id;
    bool status;

    if (!osifp) {
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;
    if (!nss_ctx) {
        return;
    }
    if_num = osifp->nss_ifnum;

    dscpmapid_msg->dscp_tid_map_id = map_id;
    nss_wifi_vdev_msg_init(&wifivdevmsg, if_num, NSS_WIFI_VDEV_DSCP_TID_MAP_ID_MSG, sizeof(struct nss_wifi_vdev_dscptid_map_id), NULL, NULL);

    /*
     * Send the dscp to tid map id message down to NSS
     */
    status = nss_wifi_vdev_tx_msg(nss_ctx, &wifivdevmsg);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the dscp-tid map message to NSS\n");
    }
}
EXPORT_SYMBOL(osif_nss_vdev_set_dscp_tid_map_id);

/*
 * osif_nss_vdev_process_wnm_tfs_tx()
 *     API to process pkts exceptioned from NSS-FW when wnm_tfs is set.
 */
void osif_nss_vdev_process_wnm_tfs_tx(struct net_device *netdev, struct sk_buff *skb)
{
    osif_dev *osifp;
    struct ieee80211vap *vap;

    osifp = netdev_priv(netdev);
    vap = osifp->os_if;

    if (vap) {
        if (wlan_wnm_tfs_filter(vap, skb)) {
            qdf_nbuf_free(skb);
        } else {
            osif_nss_vdev_enqueue_packet_for_tx(osifp, skb);
        }

    } else {
        qdf_nbuf_free(skb);
    }
}

/*
 * osif_nss_vdev_set_inspection_mode()
 *	API to set vap inspection mode.
 */
void osif_nss_vdev_set_inspection_mode(osif_dev *osifp, uint32_t value)
{
    struct net_device *dev = NULL;
    int32_t if_num = 0, next_hop_ifnum = 0;
    struct nss_ctx_instance *nss_ctx = NULL;
    nss_tx_status_t status;
    enum nss_wifi_vdev_dp_type vap_dp_type;

    if (!osifp) {
        qdf_print("%s: osifp is null", __FUNCTION__);
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;

    if(!nss_ctx) {
        qdf_print("%s: nss_ctx is null", __FUNCTION__);
        return;
    }

    dev = OSIF_TO_NETDEV(osifp);
    qdf_print("set vap inspection mode = %d", value);

    switch (value) {
        case 0:
            {
                if (!osifp->inspect_mode) {
                    qdf_print("vap inspect mode is not set");
                    return;
                }

                osifp->inspect_mode = 0;

                if (osif_nss_vdev_del_redirect_if(osifp) != 0) {
                    qdf_print("vap deletion failed in vap inspection mode");
                    return;
                }

                osifp->nss_redir_ctx = NULL;
                next_hop_ifnum = NSS_ETH_RX_INTERFACE;
                vap_dp_type = NSS_WIFI_VDEV_DP_ACCELERATED;
            }
            break;
        case 1:
            {
                if (osifp->inspect_mode) {
                    qdf_print("vap inspect mode is already set");
                    return;
                }

                if (osifp->nss_redir_ctx) {
                    qdf_print("nss_redir_ctx not null while setting vap inspection mode");
                    return;
                }

                if (osif_nss_vdev_create_redirect_if(osifp, dev) != 0) {
                    qdf_print("Vap redirect interface creation failed");
                    return;
                }

                next_hop_ifnum = NSS_N2H_INTERFACE;
                vap_dp_type = NSS_WIFI_VDEV_DP_NON_ACCELERATED;

                /*
                 * setting the inspect mode param
                 */
               osifp->inspect_mode = 1;
            }

            break;
        default:
            {
                qdf_print("command %d is not supported", value);
                return;
            }
            break;
    }

    if_num = osifp->nss_ifnum;

    status = nss_wifi_vdev_set_next_hop(nss_ctx, if_num, next_hop_ifnum);
    if (status != NSS_TX_SUCCESS) {
        osif_nss_warn("Unable to send the vdev next node message to NSS\n");
        goto fail;
    }

    /*
     * disable the network graph for vap
     */
    if (nss_wifi_vdev_set_dp_type(nss_ctx, dev, if_num, vap_dp_type) == false) {
        qdf_print("vap set dp type failed");
        goto fail;
    }
    return;

fail:
    (void)osif_nss_vdev_del_redirect_if(osifp);
    osifp->nss_redir_ctx = NULL;
    osifp->inspect_mode = 0;
    return;
}
EXPORT_SYMBOL(osif_nss_vdev_set_inspection_mode);

/*
 * osif_nss_vdev_set_read_rxprehdr()
 *	API to set read rx pre header.
 */
void osif_nss_vdev_set_read_rxprehdr(osif_dev *osifp, uint32_t value)
{
    struct net_device *dev = NULL;
    struct nss_ctx_instance *nss_ctx = NULL;

    if (!osifp) {
        qdf_print("%s: osifp is null", __FUNCTION__);
        return;
    }

    nss_ctx = osifp->nss_wifiol_ctx;

    if(!nss_ctx) {
        qdf_print("%s: nss_ctx is null", __FUNCTION__);
        return;
    }

    dev = OSIF_TO_NETDEV(osifp);
    qdf_print("set vap rx preheader read mode = %d", value);

    if (value > 1) {
        return;
    }

    /*
     * setting the rxpreheader read param
     */
    osifp->nssol_rxprehdr_read = value;
}
EXPORT_SYMBOL(osif_nss_vdev_set_read_rxprehdr);

