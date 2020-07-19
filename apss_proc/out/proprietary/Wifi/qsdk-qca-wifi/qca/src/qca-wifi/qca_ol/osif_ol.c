/*
 * Copyright (c) 2016-2019  Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "osif_private.h"
#include "target_type.h"
#include <wlan_opts.h>
#include <ieee80211_var.h>
#include <dp_extap.h>
#include <ieee80211_api.h>
#include "if_athvar.h"
#include <ieee80211_acfg.h>
#include <acfg_drv_event.h>
#if MESH_MODE_SUPPORT
#include <if_meta_hdr.h>
#endif
#include <qdf_perf.h>
#include "ath_netlink.h"

#include "ieee80211_ev.h"
#include "ald_netlink.h"
#include <ol_txrx_api.h>
#include <cdp_txrx_ctrl.h>
#include <qdf_trace.h>
#include <wlan_utility.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#include <fs/proc/internal.h>
#else
#include <linux/proc_fs.h>
#endif

#include <linux/seq_file.h>

#include <ieee80211_vi_dbg.h>

#if UMAC_SUPPORT_PROXY_ARP
int wlan_proxy_arp(wlan_if_t vap, wbuf_t wbuf);
#endif

#include <linux/ethtool.h>

#include <ieee80211_nl.h>
#include <qdf_nbuf.h> /* qdf_nbuf_map_single */

#if ATH_SUPPORT_WRAP
#include "ieee80211_api.h"
#endif
#if ATH_PERF_PWR_OFFLOAD
#include <ol_cfg_raw.h>
#include <osif_rawmode.h>
#include <ol_if_athvar.h>
#endif /* ATH_PERF_PWR_OFFLOAD */
#include "ol_ath.h"
#include <linux/ethtool.h>
#include <osif_ol.h>
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#include <osif_nss_wifiol_vdev_if.h>
#endif
#if QCA_AIRTIME_FAIRNESS
#include <target_if_atf.h>
#endif
#include <init_deinit_lmac.h>
#include <wlan_son_pub.h>
#include <wlan_utility.h>

#define DP_LAG_SEC_VAP_SEND true
#define DP_LAG_SEC_SKIP_VAP_SEND false
#if (HOST_SW_LRO_ENABLE && ATH_PERF_PWR_OFFLOAD)
extern void ath_lro_process_skb(struct sk_buff *skb, struct net_device *dev);
extern void ath_lro_flush_all(void *priv);
extern void transcap_nwifi_to_8023(qdf_nbuf_t msdu);
#endif

#if DBDC_REPEATER_SUPPORT
extern int dbdc_rx_process (os_if_t *osif ,struct net_device **dev, struct sk_buff *skb, int *nwifi);
extern int dbdc_tx_process (wlan_if_t vap, osif_dev **osdev , struct sk_buff *skb);
#endif

#if ATH_SUPPORT_WRAP
extern osif_dev * osif_wrap_wdev_find(struct wrap_devt *wdt,unsigned char *mac);
#endif

#if UMAC_VOW_DEBUG
extern void update_vow_dbg_counters(osif_dev  *osifp,
                        qdf_nbuf_t msdu, unsigned long *vow_counter, int rx, int peer);
#endif

#if QCA_NSS_PLATFORM
extern void osif_send_to_nss(os_if_t osif, struct sk_buff *skb, int nwifi);
#endif

#ifdef QCA_PARTNER_PLATFORM
extern bool osif_pltfrm_deliver_data(os_if_t osif, wbuf_t wbuf);
#endif
#if MESH_MODE_SUPPORT
extern void
os_if_tx_free_ext(struct sk_buff *skb);
#endif

#if UMAC_VOW_DEBUG
static inline void
osif_ol_hadrstart_vap_vow_debug(osif_dev  *osdev, struct sk_buff *skb){

    if(osdev->vow_dbg_en) {
        //This needs to be changed if multiple skbs are sent
        struct ether_header *eh = (struct ether_header *)skb->data;
        int i=0;

        for( i = 0; i < MAX_VOW_CLIENTS_DBG_MONITOR; i++ )
        {
            if( eh->ether_dhost[4] == osdev->tx_dbg_vow_peer[i][0] &&
                    eh->ether_dhost[5] == osdev->tx_dbg_vow_peer[i][1] ) {
                update_vow_dbg_counters(osdev, (qdf_nbuf_t) skb, &osdev->tx_dbg_vow_counter[i], 0, i);
                break;
            }
        }
    }
    return;
}

#endif /* UMAC_VOW_DEBUG*/


#if QCA_NSS_PLATFORM
#if UMAC_VOW_DEBUG || UMAC_SUPPORT_VI_DBG
#define OLE_HEADER_PADDING 2
int
transcap_nwifi_hdrsize(qdf_nbuf_t msdu)
{
    struct ieee80211_frame_addr4 *wh;
    uint32_t hdrsize;
    uint8_t fc1;

    wh = (struct ieee80211_frame_addr4 *)qdf_nbuf_data(msdu);
    fc1 = wh->i_fc[1] & IEEE80211_FC1_DIR_MASK;

    /* Native Wifi header  give only 80211 non-Qos packets */
    if (fc1 == IEEE80211_FC1_DIR_DSTODS )
    {
        hdrsize = sizeof(struct ieee80211_frame_addr4);

        /* In case of WDS frames, , padding is enabled by default
         * in Native Wifi mode, to make ipheader 4 byte aligned
         */
        hdrsize = hdrsize + OLE_HEADER_PADDING;
    } else {
        hdrsize = sizeof(struct ieee80211_frame);
    }

    /*
     *
     * header size (wifhdrsize + llchdrsize )
     */
    hdrsize +=sizeof(struct llc);
    return hdrsize;
}
#endif

#if UMAC_VOW_DEBUG
#define VOW_DBG_RX_OFFSET 14
#define VOW_DBG_INVALID_IDX -1
#define    UMAC_VOW_NSSRX_DELIVER_DEBUG(_osif, _skb, _nwifi) \
        {\
            osif_dev  *osifp = (osif_dev *) _osif; \
            int rx_offset =0;\
            if (osifp->vow_dbg_en) { \
                if (_nwifi) { \
    			rx_offset = VOW_DBG_RX_OFFSET - transcap_nwifi_hdrsize(_skb); \
                } \
                update_vow_dbg_counters(osifp, (qdf_nbuf_t)_skb, &osifp->umac_vow_counter, rx_offset, \
                        VOW_DBG_INVALID_IDX); \
            } \
        }
#elif UMAC_SUPPORT_VI_DBG
#define UMAC_VOW_NSSRX_DELIVER_DEBUG(_osif, _skb, _nwifi) \
{\
        osif_dev  *osifp = (osif_dev *) _osif; \
        if (osifp->vi_dbg) { \
                ieee80211_vi_dbg_input(osifp->os_if, _skb); \
        } \
}
#else /* UMAC_VOW_DEBUG */
#define    UMAC_VOW_NSSRX_DELIVER_DEBUG(osif, skb, nwifi)
#endif /* UMAC_VOW_DEBUG */


#if ATH_SUPPORT_VLAN
#if LINUX_VERSION_CODE <  KERNEL_VERSION(3,1,0)
#error "KERNEL_VERSION less then 3.1.0 not supported in NWIFI offload"
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#define ATH_ADD_VLAN_TAG(_osif, _skb) \
{ \
    osif_dev  *osifp = (osif_dev *) _osif; \
    if ( osifp->vlanID != 0) { \
        __vlan_hwaccel_put_tag(_skb, osifp->vlanID); \
    } \
}
#else
#define ATH_ADD_VLAN_TAG(_osif, _skb) \
{ \
    osif_dev  *osifp = (osif_dev *) _osif; \
    if ( osifp->vlanID != 0) { \
        __vlan_hwaccel_put_tag(_skb, htons(ETH_P_8021Q), osifp->vlanID); \
    } \
}
#endif
#else
#define ATH_ADD_VLAN_TAG(_osif, _skb)
#endif /*ATH_SUPPORT_VLAN*/


#endif /* QCA_NSS_PLATFORM*/




#if UMAC_VOW_DEBUG
#define  OL_TX_LL_UMAC_VAP_HARDSTART_VOW_DEBUG(_osdev, _skb) osif_ol_hadrstart_vap_vow_debug(_osdev, _skb)
#elif UMAC_SUPPORT_VI_DBG
#define  OL_TX_LL_UMAC_VAP_HARDSTART_VOW_DEBUG(_osdev, _skb) \
{\
	osif_dev  *osifp = (osif_dev *) _osdev; \
			   if (osifp->vi_dbg) { \
				   ieee80211_vi_dbg_input(osifp->os_if, _skb); \
			   } \
}
#else
#define OL_TX_LL_UMAC_VAP_HARDSTART_VOW_DEBUG(_osdev, _skb)
#endif /* UMAC_VOW_DEBUG */

#if ATH_SUPPORT_WRAP
inline int wrap_tx_bridge_nolock ( wlan_if_t *vap , osif_dev **osdev,  struct sk_buff **skb) {

    /* Assuming native wifi or raw mode is not
     * enabled in beeliner, to be revisted later
     */
    struct ether_header *eh = (struct ether_header *) ((*skb)->data);
    wlan_if_t prev_vap = *vap;
    osif_dev *tx_osdev;

    /* Mpsta vap here, find the correct tx vap from the wrap common based on src address */
    tx_osdev = osif_wrap_wdev_find(&prev_vap->iv_ic->ic_wrap_com->wc_devt,eh->ether_shost);
    if (tx_osdev) {
        if(qdf_unlikely((IEEE80211_IS_MULTICAST(eh->ether_dhost) || IEEE80211_IS_BROADCAST(eh->ether_dhost)))) {
            *skb = qdf_nbuf_unshare(*skb);
        }
        *vap = tx_osdev->os_if;
        *osdev  = tx_osdev;
        return 0;
    } else {
        /* When proxysta is not created, drop the packet. Donot send this
         * packet on mainproxysta. Return 1 here to drop the packet when psta
         * is not yet created.
         */
        return 1;
    }
}

inline int wrap_tx_bridge ( wlan_if_t *vap , osif_dev **osdev,  struct sk_buff **skb) {

    /* Assuming native wifi or raw mode is not
     * enabled in beeliner, to be revisted later
     */
    struct ether_header *eh = (struct ether_header *) ((*skb)->data);
    wlan_if_t prev_vap = *vap;
    osif_dev *tx_osdev;
    osif_dev *prev_osdev = *osdev;
    ol_txrx_soc_handle soc_txrx_handle =
        wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(prev_vap->iv_ic->ic_pdev_obj));
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    void *nss_wifiol_ctx = prev_osdev->nss_wifiol_ctx;
#endif


    /* Mpsta vap here, find the correct tx vap from the wrap common based on src address */
    tx_osdev = osif_wrap_wdev_find(&prev_vap->iv_ic->ic_wrap_com->wc_devt,eh->ether_shost);
    if (tx_osdev) {
        if(qdf_unlikely((IEEE80211_IS_MULTICAST(eh->ether_dhost) || IEEE80211_IS_BROADCAST(eh->ether_dhost)))) {
            *skb = qdf_nbuf_unshare(*skb);
        }
        /* since tx vap gets changed , handle tx vap synchorization */
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (!nss_wifiol_ctx)
#endif
        {
            OSIF_VAP_TX_UNLOCK(soc_txrx_handle, prev_osdev);
        }
        *vap = tx_osdev->os_if;
        *osdev  = tx_osdev;

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (!nss_wifiol_ctx)
#endif
        {
            OSIF_VAP_TX_LOCK(soc_txrx_handle, *osdev);
        }
        return 0;
    } else {
        /* When proxysta is not created, drop the packet. Donot send this
         * packet on mainproxysta. Return 1 here to drop the packet when psta
         * is not yet created.
         */
        return 1;
    }
}

#define OL_WRAP_TX_PROCESS_NO_LOCK(_osdev, _vap, _skb) \
{ \
    if (qdf_unlikely(wlan_is_mpsta(_vap)))   { \
        if (wrap_tx_bridge_nolock (&_vap, _osdev , _skb)) {\
            goto bad;\
        }\
        if (*(_skb) == NULL) {\
            goto bad;\
        }\
        if (wlan_is_psta(_vap))   { \
            vap->iv_wrap_mat_tx(_vap, (wbuf_t)*_skb); \
        } \
    } \
    if (wlan_vdev_is_up(_vap->vdev_obj) != QDF_STATUS_SUCCESS) {\
        goto bad; \
   } \
}

#define OL_WRAP_TX_PROCESS(_osdev, _vap, _skb) \
{ \
    if (qdf_unlikely(wlan_is_mpsta(_vap)))   { \
        if(wrap_tx_bridge (&_vap, _osdev , _skb)) {\
            goto bad;\
        }\
        if (*(_skb) == NULL) {\
            goto bad;\
        }\
        if (wlan_is_psta(_vap))   { \
            vap->iv_wrap_mat_tx(_vap, (wbuf_t)*_skb); \
        } \
    } \
    if (wlan_vdev_is_up(_vap->vdev_obj) != QDF_STATUS_SUCCESS) {\
        goto bad; \
   } \
}

/*
 * osif_ol_wrap_tx_process()
 *  wrap tx process
 */
int
osif_ol_wrap_tx_process(osif_dev **osifp, struct ieee80211vap *vap, struct sk_buff **skb)
{
    /*
     * The below addiitonal check is to make compilation when the ATH_WRAP_TX Macro is disabled.
     * the bad label will come as unused variable
     */
    if (vap == NULL) {
        goto bad;
    }
    OL_WRAP_TX_PROCESS(osifp, vap, skb);
    return 0;
bad :
    return 1;
}
#else /* ATH_SUPPORT_WRAP */

#define OL_WRAP_TX_PROCESS(_osdev, _vap, _skb)
int
osif_ol_wrap_tx_process(osif_dev **osifp, struct ieee80211vap *vap, struct sk_buff **skb)
{
    return 0;
}
#endif /* ATH_SUPPORT_WRAP */


#if ATH_DATA_RX_INFO_EN || MESH_MODE_SUPPORT
u_int32_t osif_rx_status_dump(void* rs)
{
#if ATH_DATA_RX_INFO_EN
    struct per_msdu_data_recv_status *rs1 = rs;
#else
    struct mesh_recv_hdr_s *rs1 = rs;
    u_int32_t count;
#endif
    u_int32_t rate1 = rs1->rs_ratephy1;
    u_int32_t rate2 = rs1->rs_ratephy2 & 0xFFFFFF;
//    u_int32_t rate3 = rs->rs_ratephy3 & 0x1FFFFFFF;
    u_int32_t rate1_1 = ((rate1 & 0xFFFFFF0) >> 4);

    if(!(rs1->rs_flags & IEEE80211_RX_FIRST_MSDU)){
        /*Only for the 1st msdu, we populate the rx status struct,
          check unified_rx_desc_update_pkt_info() */
        return 0;
    }

    /*We save receive status info in skb->cb[48] after struct cvg_nbuf_cb,
      so we have struct cvg_nbuf_cb + struct per_msdu_data_recv_status in cb.
      Since, sizeof(struct cvg_nbuf_cb)=44, only 48-44=4 bytes left for rs here,
      we only have space to save the 1st int(rs_flags) of the struct per_msdu_data_recv_status.
    */
    qdf_print("%s: rs_flags=0x%x ",__FUNCTION__, rs1->rs_flags);
#if MESH_MODE_SUPPORT
    qdf_print("%s: frame is decrypted=0x%x keyix %d", __FUNCTION__, (rs1->rs_flags & MESH_RX_DECRYPTED)
            ? 1 : 0, rs1->rs_keyix);
#endif


    /*Below fields only valid when skb->cb has enough space to store them*/
    qdf_print("%s: rs_rssi=0x%x ",__FUNCTION__, rs1->rs_rssi);
    qdf_print("%s: rs_ratephy1=0x%x ",__FUNCTION__, rs1->rs_ratephy1);
    qdf_print("%s: rs_ratephy2=0x%x ",__FUNCTION__, rs1->rs_ratephy2);
    qdf_print("%s: rs_ratephy3=0x%x ",__FUNCTION__, rs1->rs_ratephy3);

#if MESH_MODE_SUPPORT
    qdf_print("%s: rs_channel=0x%x ",__FUNCTION__,  rs1->rs_channel);
    qdf_print("%s: rs_key=",__FUNCTION__);
    for (count = 0; count < 32; count++) {
        qdf_print("0x%x ", rs1->rs_decryptkey[count]);
    }
    qdf_print("\n");
    if ((rs1->rs_flags & MESH_RXHDR_VER) == MESH_RXHDR_VER1) {
        qdf_print("pkt type %d ", (rs1->rs_ratephy1 >> 16) & 0xFF);
        qdf_print("mcs %d ", rs1->rs_ratephy1 & 0xFF);
        qdf_print("nss %d ", (rs1->rs_ratephy1 >> 8) & 0xFF);
        qdf_print("bw %d", (rs1->rs_ratephy1 >> 24) & 0xFF);
    } else
#endif
    {
        switch (rate1 & 0xF) // preamble
        {
            case 0: //CCK
                {
                    switch(rate1_1) //l_sig_rate
                    {
                        case 0x1:  //long 1M
                            qdf_print("CCK 1 Mbps long preamble");
                            break;
                        case 0x2: //long 2M
                            qdf_print("CCK 2 Mbps long preamble");
                            break;
                        case 0x3: //long 5.5M
                            qdf_print("CCK 5.5 Mbps long preamble");
                            break;
                        case 0x4: //long 11M
                            qdf_print("CCK 11 Mbps long preamble");
                            break;
                        case 0x5: //short 2M
                            qdf_print("CCK 2 Mbps short preamble");
                            break;
                        case 0x6: //short 5.5M
                            qdf_print("CCK 5.5 Mbps short preamble");
                            break;
                        case 0x7: //short 11M
                            qdf_print("CCK 11 Mbps short preamble");
                            break;
                    }
                }
                break;
            case 1: //OFDM
                {
                    switch(rate1_1) //l_sig_rate
                    {
                        case 0x8:
                            qdf_print("OFDM 48 Mbps");
                            break;

                        case 0x9:
                            qdf_print("OFDM 24 Mbps");
                            break;

                        case 0xa:
                            qdf_print("OFDM 12 Mbps");
                            break;

                        case 0xb:
                            qdf_print("OFDM 6 Mbps");
                            break;

                        case 0xc:
                            qdf_print("OFDM 54 Mbps");
                            break;

                        case 0xd:
                            qdf_print("OFDM 36 Mbps");
                            break;

                        case 0xe:
                            qdf_print("OFDM 18 Mbps");
                            break;

                        case 0xf:
                            qdf_print("OFDM 9 Mbps");
                            break;
                    }
                }
                break;
            case 2:
                {
                    if(rate1_1 & 0x80) //HT40
                    {
                        qdf_print("HT40 MCS%c", '0' + (rate1_1 & 0x1f));
                    }
                    else // HT20
                    {
                        qdf_print("HT20 MCS%c", '0' + (rate1_1 & 0x1f));
                    }
                }
                break;
            case 3:
                switch (rate1_1 & 0x3)
                {
                    case 0x0: // VHT20
                        qdf_print("VHT20 NSS%c MCS%c", '1' + ((rate1_1 >> 10) & 0x3),
                                '0' + ((rate2 >> 4) & 0xf));

                        break;

                    case 0x1: // VHT40
                        qdf_print("VHT40 NSS%c MCS%c", '1' + ((rate1_1 >> 10) & 0x3),
                                '0' + ((rate2 >> 4) & 0xf));

                        break;

                    case 0x2: // VHT80
                        qdf_print("VHT80 NSS%c MCS%c", '1' + ((rate1_1 >> 10) & 0x3),
                                '0' + ((rate2 >> 4) & 0xf));

                        break;
                }
                break;
        }
    }
    return 0;
}
EXPORT_SYMBOL(osif_rx_status_dump);
#endif /*end of ATH_DATA_RX_INFO_EN*/



#if QCA_NSS_PLATFORM
void
osif_deliver_data_ol(os_if_t osif, struct sk_buff *skb_list)
{
    struct net_device *dev = OSIF_TO_NETDEV(osif);
    osif_dev  *osdev = (osif_dev *)osif;
    int nwifi = ((osif_dev *)osif)->nss_nwifi;
    struct wlan_objmgr_vdev *vdev = osdev->ctrl_vdev;
    ol_txrx_pdev_handle pdev_txrx_handle;
    ol_txrx_soc_handle soc_txrx_handle;

#if ATH_SUPPORT_WRAP || MESH_MODE_SUPPORT
    wlan_if_t vap = osdev->os_if;
#endif

#if QCA_OL_VLAN_WAR || CONFIG_DP_TRACE || DBDC_REPEATER_SUPPORT
    struct net_device *comdev;
    struct ol_ath_softc_net80211 *scn;
    struct ether_header *eh;
    uint8_t pdev_id;
    uint32_t is_lithium;

    comdev = ((osif_dev *)osif)->os_comdev;
    scn = ath_netdev_priv(comdev);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    is_lithium = ol_target_lithium(scn->soc->psoc_obj);
    pdev_id = cdp_get_pdev_id_frm_pdev(soc_txrx_handle,
		    (void *)pdev_txrx_handle);
#endif /* QCA_OL_VLAN_WAR || CONFIG_DP_TRACE */

    while (skb_list) {
        struct sk_buff *skb;
        skb = skb_list;
        skb_list = skb_list->next;
        skb->dev = dev;
        skb->next = NULL;

#if QCA_OL_VLAN_WAR
	eh = (struct ether_header *)(skb->data);

	/* For VLAN, remove the extra two bytes of length field inserted by OLE.
	 * This is standard behavior for OLE RX any format that looks weird
	 * it keeps it in 802.3 format as that preserves all the information.
	 * Frame Format : {dst_addr[6], src_addr[6], 802.1Q header[4], EtherType[2], Payload}
	 * Example :
	 * Before Removal : xx xx xx xx xx xx xx xx xx xx xx xx 81 00 00 02 00 4e 08 00 45 00 00...
	 * After Removal  : xx xx xx xx xx xx xx xx xx xx xx xx 81 00 00 02 08 00 45 00 00...
	 */

	if (htons(eh->ether_type) == ETH_P_8021Q) {
	    transcap_dot3_to_eth2(skb);

	}
#endif /* QCA_OL_VLAN_WAR */

#if ATH_DATA_RX_INFO_EN
        if (qdf_nbuf_get_rx_ftype(skb) == CB_FTYPE_RX_INFO)
        /*
         * provide callback to collect rx_status here if required.
         * The data will no longer be available after this point
         */
            osif_rx_status_dump((void *)qdf_nbuf_get_rx_fctx(skb));
#elif MESH_MODE_SUPPORT
        if ((vap->mdbg & 0x2) && (qdf_nbuf_get_rx_ftype(skb) == CB_FTYPE_MESH_RX_INFO)) {
        /*
         * provide callback to collect rx_status here if required.
         * The data will no longer be available after this point
         */
            osif_rx_status_dump((void *)qdf_nbuf_get_rx_fctx(skb));
        }
#endif

#if ATH_DATA_RX_INFO_EN
        if (qdf_nbuf_get_rx_ftype(skb) == CB_FTYPE_RX_INFO){
            qdf_mem_free((void *)qdf_nbuf_get_rx_fctx(skb));
            qdf_nbuf_set_rx_fctx_type(skb, 0, CB_FTYPE_INVALID);
        }
#elif MESH_MODE_SUPPORT
        if (qdf_nbuf_get_rx_ftype(skb) == CB_FTYPE_MESH_RX_INFO) {
            qdf_mem_free((void *)qdf_nbuf_get_rx_fctx(skb));
            qdf_nbuf_set_rx_fctx_type(skb, 0, CB_FTYPE_INVALID);
       }
#endif

#if CONFIG_DP_TRACE
    qdf_nbuf_classify_pkt(skb);
    qdf_dp_trace_log_pkt(0, skb, QDF_RX, pdev_id);
    qdf_dp_trace_set_track(skb, QDF_RX);
    QDF_NBUF_CB_TX_PACKET_TRACK(skb) = QDF_NBUF_TX_PKT_DATA_TRACK;
    DPTRACE(qdf_dp_trace(skb, QDF_DP_TRACE_HDD_RX_PACKET_RECORD, pdev_id,
        (uint8_t *)skb->data, qdf_nbuf_len(skb), QDF_RX));
    if (qdf_nbuf_len(skb) > QDF_DP_TRACE_RECORD_SIZE) {
        DPTRACE(qdf_dp_trace(skb, QDF_DP_TRACE_HDD_RX_PACKET_RECORD,
            pdev_id, (uint8_t *)&skb->data[QDF_DP_TRACE_RECORD_SIZE],
            (qdf_nbuf_len(skb)-QDF_DP_TRACE_RECORD_SIZE), QDF_RX));
    }
#endif

#if ATH_SUPPORT_WRAP
        if(OL_WRAP_RX_PROCESS(&osif, &dev, vap, skb, &nwifi))
            continue;
#endif

        UMAC_VOW_NSSRX_DELIVER_DEBUG(osif, skb, nwifi);

        if (ADP_EXT_AP_RX_PROCESS(vdev, skb, &nwifi)) {
            dev_kfree_skb(skb);
            continue;
        }
#if DBDC_REPEATER_SUPPORT
        if (is_lithium && dp_lag_is_enabled(vdev)) {
            if(dp_lag_rx_process(vdev, skb, DP_LAG_SEC_VAP_SEND))
                continue;
        } else {
            if (dbdc_rx_process(&osif, &dev, skb, &nwifi))
                continue;
        }
#endif

#ifdef QCA_PARTNER_PLATFORM
        if ( osif_pltfrm_deliver_data (osif, skb))
            continue;
#endif

	ATH_ADD_VLAN_TAG(osif, skb)

#if HOST_SW_LRO_ENABLE
        if (dev->features & NETIF_F_LRO) /* LRO is enabled via ethtool */
        {
        if (nwifi) {
           transcap_nwifi_to_8023(skb);
        }
            skb->protocol = eth_type_trans(skb, dev);
            ath_lro_process_skb(skb,dev);
	    continue;
	}
#endif  /* HOST_SW_LRO_ENABLE */

        osif_send_to_nss(osif, skb, nwifi);
    }

#if HOST_SW_LRO_ENABLE
    if ((dev->features & NETIF_F_LRO) || osdev->os_if->lro_to_flush) {
        ath_lro_flush_all(dev);         /* flush all LRO pkts */
    }
#endif /* HOST_SW_LRO_ENABLE */

    dev->last_rx = jiffies;
}
#else /*QCA_NSS_PLATFORM*/
void
osif_deliver_data_ol(os_if_t osif, struct sk_buff *skb_list)
{
    struct net_device *dev = OSIF_TO_NETDEV(osif);
#if ATH_SUPPORT_VLAN || UMAC_VOW_DEBUG
    osif_dev  *osifp = (osif_dev *) osif;
#endif
    osif_dev  *osdev = (osif_dev *)osif;
    struct wlan_objmgr_vdev *vdev = osdev->ctrl_vdev;
#if ATH_SUPPORT_WRAP || MESH_MODE_SUPPORT
    wlan_if_t vap = osdev->os_if;
#endif
#if ATH_RXBUF_RECYCLE
    struct net_device *comdev;
    struct ath_softc_net80211 *scn;
    struct ath_softc *sc;
#endif /* ATH_RXBUF_RECYCLE */

    int nwifi = ((osif_dev *)osif)->nss_nwifi;
#if QCA_OL_VLAN_WAR || CONFIG_DP_TRACE
    ol_txrx_pdev_handle pdev;
    ol_txrx_soc_handle soc_txrx_handle;
    struct net_device *comdev_war;
    struct ol_ath_softc_net80211 *scn_war;
#if UMAC_VOW_DEBUG
    struct ol_txrx_nbuf_classify nbuf_class;
#endif
    uint8_t pdev_id = 0;
    bool carrier_vow_config = 0;
    comdev_war = ((osif_dev *)osif)->os_comdev;
    scn_war = ath_netdev_priv(comdev_war);
    soc_txrx_handle = wlan_psoc_get_dp_handle(scn_war->soc->psoc_obj);
    pdev = wlan_pdev_get_dp_handle(scn_war->sc_pdev);
    pdev_id =  cdp_get_pdev_id_frm_pdev(soc_txrx_handle, (struct cdp_pdev *)pdev);
    carrier_vow_config = cdp_get_vow_config_frm_pdev(soc_txrx_handle, (struct cdp_pdev *)pdev);
#endif /* QCA_OL_VLAN_WAR || CONFIG_DP_TRACE */

#if ATH_RXBUF_RECYCLE
    comdev = ((osif_dev *)osif)->os_comdev;
    scn = ath_netdev_priv(comdev);
    sc = ATH_DEV_TO_SC(scn->sc_dev);
#endif /* ATH_RXBUF_RECYCLE */

    while (skb_list) {
        struct sk_buff *skb;
#if QCA_OL_VLAN_WAR
        u_int16_t typeorlen;
#endif

        skb = skb_list;
        skb_list = skb_list->next;

        skb->dev = dev;
        /*
         * SF#01368954
         * Thanks to customer for the fix.
         *
         * Each skb of the list is delivered to the OS.
         * Thus, we need to unlink each skb.
         * Otherwise, the OS processes the linked skbs, as well,
         * which results in sending the same skb twice to the LAN driver.
         * This, also leads to unpredictable frame drops.
         * Note that this (most likely) only occurs when sending frames
         * using dev_queue_xmit().
         * Delivering linked skbs through netif_rx() seems not to be a problem.
         */
        skb->next = NULL;

#if ATH_DATA_RX_INFO_EN
        if (qdf_nbuf_get_rx_ftype(skb) == CB_FTYPE_RX_INFO)
        /*
         * provide callback to collect rx_status here if required.
         * The data will no longer be available after this point
         */
            osif_rx_status_dump((void *)qdf_nbuf_get_rx_fctx(skb));
#elif MESH_MODE_SUPPORT
        if ((vap->mdbg & 0x2) && (qdf_nbuf_get_rx_ftype(skb) == CB_FTYPE_MESH_RX_INFO)) {
        /*
         * provide callback to collect rx_status here if required.
         * The data will no longer be available after this point
         */
            osif_rx_status_dump((void *)qdf_nbuf_get_rx_fctx(skb));
        }
#endif

#if ATH_DATA_RX_INFO_EN
        if (qdf_nbuf_get_rx_ftype(skb) == CB_FTYPE_RX_INFO){
            qdf_mem_free((void *)qdf_nbuf_get_rx_fctx(skb));
            qdf_nbuf_set_rx_fctx_type(skb, 0, CB_FTYPE_INVALID);
       }
#elif MESH_MODE_SUPPORT
       if (qdf_nbuf_get_rx_ftype(skb) == CB_FTYPE_MESH_RX_INFO) {
            qdf_mem_free((void *)qdf_nbuf_get_rx_fctx(skb));
            qdf_nbuf_set_rx_fctx_type(skb, 0, CB_FTYPE_INVALID);
       }
#endif

#if QCA_OL_VLAN_WAR
       typeorlen = ntohs(*(u_int16_t *)(skb->data + IEEE80211_ADDR_LEN * 2));
       /* For VLAN, remove the extra two bytes of length field inserted by OLE.
	* This is standard behavior for OLE RX any format that looks weird
	* it keeps it in 802.3 format as that preserves all the information.
	*/
       if (typeorlen == ETH_P_8021Q) {
	   transcap_dot3_to_eth2(skb);
       }
#endif /* QCA_OL_VLAN_WAR */

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)) && MIPS_TP_ENHC
    prefetch(skb->data);
    prefetch(&skb->protocol);
    prefetch(&skb->__pkt_type_offset);
#endif

#if CONFIG_DP_TRACE
    qdf_nbuf_classify_pkt(skb);
    qdf_dp_trace_log_pkt(0, skb, QDF_RX, pdev_id);
    qdf_dp_trace_set_track(skb, QDF_RX);
    QDF_NBUF_CB_TX_PACKET_TRACK(skb) = QDF_NBUF_TX_PKT_DATA_TRACK;
    DPTRACE(qdf_dp_trace(skb, QDF_DP_TRACE_HDD_RX_PACKET_RECORD, pdev_id,
        (uint8_t *)skb->data, qdf_nbuf_len(skb), QDF_RX));
    if (qdf_nbuf_len(skb) > QDF_DP_TRACE_RECORD_SIZE) {
        DPTRACE(qdf_dp_trace(skb, QDF_DP_TRACE_HDD_RX_PACKET_RECORD,
            pdev_id, (uint8_t *)&skb->data[QDF_DP_TRACE_RECORD_SIZE],
            (qdf_nbuf_len(skb)-QDF_DP_TRACE_RECORD_SIZE), QDF_RX));
    }
#endif

#if ATH_SUPPORT_WRAP
    if(OL_WRAP_RX_PROCESS(&osif, &dev, vap, skb, &nwifi))
            return;
#endif

    if (ADP_EXT_AP_RX_PROCESS(vdev, skb, &nwifi)) {
        qdf_nbuf_free(skb);
        return;
    }
#if DBDC_REPEATER_SUPPORT
    if(dbdc_rx_process(&osif, &dev, skb, &nwifi))
       continue;
#endif

#ifdef HOST_OFFLOAD
        /* For the Full Offload solution, diverting the data packet into the
           offload stack for further processing and hand-off to Host processor */
        atd_rx_from_wlan(skb);
        continue;
#endif

#ifdef QCA_PARTNER_PLATFORM
        if ( osif_pltfrm_deliver_data (osif, skb))
            continue;
#endif

#if UMAC_VOW_DEBUG
	/*  Classify packet for Video */
	if (qdf_unlikely(carrier_vow_config))
		cdp_txrx_classify_and_update(soc_txrx_handle,
			wlan_vdev_get_dp_handle(osdev->ctrl_vdev), skb, rx_direction, &nbuf_class);
#endif

        skb->protocol = eth_type_trans(skb, dev);

#if ATH_RXBUF_RECYCLE
	    /*
	     * Do not recycle the received mcast frame b/c it will be cloned twice
	     */
        if (sc->sc_osdev->rbr_ops.osdev_wbuf_collect && !(wbuf_is_cloned(skb)))
        {
            sc->sc_osdev->rbr_ops.osdev_wbuf_collect((void *)sc, (void *)skb);
        }
#endif /* ATH_RXBUF_RECYCLE */

#if UMAC_VOW_DEBUG
#define VOW_DBG_RX_OFFSET 14 /*RX packet ethernet header is stripped off. Need to adjust offset accordingly*/
#define VOW_DBG_INVALID_IDX -1
        if(osifp->vow_dbg_en) {
            update_vow_dbg_counters(osifp, (qdf_nbuf_t)skb, &osifp->umac_vow_counter, VOW_DBG_RX_OFFSET,
                    VOW_DBG_INVALID_IDX);
        }
#endif

#if ATH_SUPPORT_VLAN
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
        if ( osifp->vlanID != 0)
        {
            /* attach vlan tag */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
            __vlan_hwaccel_put_tag(skb, osifp->vlanID);
#else
            __vlan_hwaccel_put_tag(skb, htons(ETH_P_8021Q), osifp->vlanID);
#endif
        }
#else
        if ( osifp->vlanID != 0 && osifp->vlgrp != NULL)
        {
            /* attach vlan tag */
            vlan_hwaccel_rx(skb, osifp->vlgrp, osifp->vlanID);
        }
        else  /*XXX NOTE- There is an else here. Be careful while adding any code below */
#endif
#endif

#if HOST_SW_LRO_ENABLE
        if (dev->features & NETIF_F_LRO) /* LRO is enabled via ethtool */
        {
            ath_lro_process_skb(skb,dev);
            continue;
        }
#endif  /* HOST_SW_LRO_ENABLE */

        if (qdf_unlikely(carrier_vow_config)) {
            cdp_calculate_delay_stats(soc_txrx_handle, wlan_vdev_get_dp_handle(osdev->ctrl_vdev), skb);
        }

        nbuf_debug_del_record(skb);
        netif_rx(skb);
    }

#if HOST_SW_LRO_ENABLE
    if ((dev->features & NETIF_F_LRO) || osdev->os_if->lro_to_flush) {
        ath_lro_flush_all(dev);         /* flush all LRO pkts */
    }
#endif /* HOST_SW_LRO_ENABLE */

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
    dev->last_rx = jiffies;
#endif
}
#endif /*QCA_NSS_PLATFORM*/


#if (MESH_MODE_SUPPORT && QCA_SUPPORT_RAWMODE_PKT_SIMULATION)
extern int add_mesh_meta_hdr(qdf_nbuf_t nbuf, struct ieee80211vap *vap);
#endif

#ifdef WLAN_FEATURE_FASTPATH
/*
 * TODO: Move this to a header file
 */
extern void
ol_tx_stats_inc_map_error(ol_txrx_vdev_handle vdev,
                             uint32_t num_map_error);
/**
 * osif_ol_process_tx() - Process common Tx operations for legacy and lithium.
 *
 * @skb - buffer from network stack
 * @dev - net device handle
 *
 * Return - 0 on failure, 1 on success
 */
static int osif_ol_process_tx(struct sk_buff *skb, struct net_device *dev)
{
    osif_dev  *osdev = ath_netdev_priv(dev);
    wlan_if_t vap = osdev->os_if;
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *pscn = (struct ol_ath_softc_net80211 *)ic;
    ol_txrx_pdev_handle pdev;
    ol_txrx_soc_handle soc_txrx_handle;
#if QCA_AIRTIME_FAIRNESS
    struct ether_header *eh;
#endif

    if (qdf_unlikely((dev->flags & (IFF_RUNNING|IFF_UP)) != (IFF_RUNNING|IFF_UP))) {
        goto bad;
    }

    if (qdf_unlikely(osdev->os_opmode == IEEE80211_M_MONITOR)) {
        goto bad;
    }

    if ((wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) ||
        (IEEE80211_IS_CHAN_RADAR(vap->iv_bsschan))) {
        goto bad;
    }

#if (MESH_MODE_SUPPORT && QCA_SUPPORT_RAWMODE_PKT_SIMULATION)
    if (vap->iv_mesh_vap_mode && (vap->mdbg & 0x1)) {
        if (OL_CFG_NONRAW_TX_LIKELINESS(vap->iv_tx_encap_type != osif_pkt_type_raw)) {
            if(add_mesh_meta_hdr(skb, vap)) {
                goto bad;
            }
        }
    }
#endif

#if QCA_AIRTIME_FAIRNESS
    eh = (struct ether_header *)(wbuf_header(skb) + vap->mhdr_len);
    if (lmac_get_tgt_type(pscn->soc->psoc_obj) == TARGET_TYPE_AR9888 &&
        lmac_get_tgt_version(pscn->soc->psoc_obj) == AR9888_REV2_VERSION) {
        if (target_if_atf_is_tx_traffic_blocked(vap->vdev_obj, eh->ether_dhost, skb)) {
            goto bad;
        }
    }
#endif

#if UMAC_SUPPORT_WNM
    if (wlan_wnm_tfs_filter(vap, (wbuf_t) skb)) {
        goto bad;
    }
#endif

    return 1;

bad:
    pdev = wlan_pdev_get_dp_handle(pscn->sc_pdev);
    soc_txrx_handle = wlan_psoc_get_dp_handle(pscn->soc->psoc_obj);
    cdp_txrx_set_pdev_param(soc_txrx_handle, (struct cdp_pdev *)pdev, CDP_OSIF_DROP, 1);
    qdf_nbuf_free(skb);
    return 0;
}

/**
 * osif_ol_process_tx_synchronous() - Process common Tx operations for legacy and lithium.
 *
 * @skb - buffer from network stack
 * @dev - net device handle
 *
 * This function is called with lock held.
 *
 * Return - 0 on failure, 1 on success
 */
static int osif_ol_process_tx_synchronous(struct sk_buff **skb, osif_dev **osdev,
                                          struct cdp_tx_exception_metadata *tx_exc_param,
                                          bool *is_exception)
{
    wlan_if_t vap = (*osdev)->os_if;
    struct wlan_objmgr_vdev *vdev = (*osdev)->ctrl_vdev;
    uint8_t src_mac[IEEE80211_ADDR_LEN];
    struct cdp_peer *peer = NULL;
    qdf_ether_header_t *eh;
    bool extap_repeater = false;
    struct cdp_ast_entry_info ast_entry_info = {0};
    int ast_entry_found = 0;
#ifdef QCA_OL_DMS_WAR
    int ret;
    uint8_t peer_addr[IEEE80211_ADDR_LEN];
#endif
    struct net_device *comdev = (*osdev)->os_comdev;
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(comdev);
    ol_txrx_pdev_handle pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);
    ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
#if CONFIG_DP_TRACE
    uint8_t pdev_id = 0;
    pdev_id =  cdp_get_pdev_id_frm_pdev(soc_txrx_handle, (struct cdp_pdev *)pdev);
#endif /*CONFIG_DP_TRACE*/
    if ((OL_CFG_RAW_TX_LIKELINESS(vap->iv_tx_encap_type == osif_pkt_type_raw))
	    || (vap->iv_tx_encap_type == osif_pkt_type_native_wifi)) {
        /* In Raw Mode, the payload normally comes encrypted by an external
         * Access Controller and we won't have the keys. Besides, the format
         * isn't 802.3/Ethernet II.
         * Hence, VLAN WAR, Ext AP functionality, VoW debug and Multicast to
         * Unicast conversion aren't applicable, and are skipped.
         *
         * Additionally, TSO and nr_frags based scatter/gather are currently
         * not required and thus not supported with Raw Mode.
         *
         * Error conditions are handled internally by the below function.
         */
        OL_TX_LL_UMAC_RAW_PROCESS((*osdev)->netdev, skb);
        return 0;
    }
#if MESH_MODE_SUPPORT
    if (!vap->iv_mesh_vap_mode)
#endif
    {


        /*
         * Perform DBDC tx process before the EXTAP TX
         */
        if (qdf_unlikely(dp_is_extap_enabled(vdev))) {
            if ((lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA8074V2) &&
                dp_lag_is_enabled(vdev)) {
                qdf_mem_copy(src_mac, ((*skb)->data + vap->mhdr_len + ETH_ALEN), ETH_ALEN);
                extap_repeater = true;
                if (dp_lag_tx_process(vdev, *skb, DP_LAG_SEC_SKIP_VAP_SEND)) {
                    return 0;
                }
            }
        }

        /* Raw mode or native wifi mode not
         * supported in qwrap , revisit later
         */
        if (ADP_EXT_AP_TX_PROCESS(vdev, skb, vap->mhdr_len, NULL)) {
            goto bad;
        }

        /*
         * For HKv2, add HM_SEC ast entry for EXTAP Repeater ethernet backend
         */
        if (qdf_unlikely(extap_repeater)) {
            if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
                eh = (qdf_ether_header_t *)((*skb)->data + vap->mhdr_len);
                peer = (struct cdp_peer *)wlan_peer_get_dp_handle(wlan_vdev_get_bsspeer(vdev));
                if (IEEE80211_IS_MULTICAST(eh->ether_dhost) && peer) {
                    ast_entry_found = cdp_peer_get_ast_info_by_soc(soc_txrx_handle, src_mac,
                                                                    &ast_entry_info);
                    if (!ast_entry_found) {
                        cdp_peer_add_ast(soc_txrx_handle, peer, src_mac, CDP_TXRX_AST_TYPE_WDS_HM_SEC, IEEE80211_NODE_F_WDS_HM);
                    }
                }
            }
        }

#if QCA_OL_VLAN_WAR
	if(OL_TX_VLAN_WAR(skb, scn))
	    goto bad;
#endif /* QCA_OL_VLAN_WAR */

#ifdef QCA_OL_DMS_WAR
	if (vap->dms_amsdu_war) {
            if (ol_check_valid_dms(skb, vap, &peer_addr[0])) {
                ret = OL_DMS_AMSDU_WAR(skb, scn, vap, &peer_addr[0], tx_exc_param);
                if (ret)
                    goto bad;
                *is_exception = true;
            }
        }
#endif

        (*skb)->next = NULL;

        OL_TX_LL_UMAC_VAP_HARDSTART_VOW_DEBUG(*osdev, *skb);
    }

#if CONFIG_DP_TRACE
    qdf_nbuf_classify_pkt(*skb);
    qdf_dp_trace_log_pkt(0, *skb, QDF_TX, pdev_id);
    QDF_NBUF_CB_TX_PACKET_TRACK(*skb) = QDF_NBUF_TX_PKT_DATA_TRACK;
    QDF_NBUF_UPDATE_TX_PKT_COUNT(*skb, QDF_NBUF_TX_PKT_HDD);
    qdf_dp_trace_set_track(*skb, QDF_TX);
    DPTRACE(qdf_dp_trace(*skb, QDF_DP_TRACE_HDD_TX_PACKET_RECORD, pdev_id,
            (uint8_t *)(*skb)->data, qdf_nbuf_len(*skb), QDF_TX));
    if (qdf_nbuf_len(*skb) > QDF_DP_TRACE_RECORD_SIZE) {
        DPTRACE(qdf_dp_trace(*skb, QDF_DP_TRACE_HDD_TX_PACKET_RECORD,
            pdev_id, (uint8_t *)&(*skb)->data[QDF_DP_TRACE_RECORD_SIZE],
            (qdf_nbuf_len(*skb)-QDF_DP_TRACE_RECORD_SIZE), QDF_TX));
    }
#endif

    return 1;

bad:
    cdp_txrx_set_pdev_param(soc_txrx_handle, (struct cdp_pdev *)pdev, CDP_OSIF_DROP, 1);
    if ((*skb) != NULL)
        qdf_nbuf_free(*skb);
    return 0;
}

static inline __attribute__((always_inline)) int
osif_ol_vap_send_wifi3(struct sk_buff *skb, struct net_device *dev, struct cdp_tx_exception_metadata *tx_exc_param, bool is_exception)
{
    osif_dev  *osdev = ath_netdev_priv(dev);
    struct ether_header *eh;
    wlan_if_t vap = osdev->os_if;
    void *soc;
    uint8_t pdev_id;
    uint8_t is_son_enabled = 0;
    struct cdp_ast_entry_info ast_entry_info = {0};
    int ast_entry_found = 0;
    struct wlan_objmgr_vdev *vdev = osdev->ctrl_vdev;
    struct ol_ath_softc_net80211 *scn = (struct ol_ath_softc_net80211 *)vap->iv_ic;
    ol_txrx_pdev_handle pdev = wlan_pdev_get_dp_handle(scn->sc_pdev);
    ol_txrx_soc_handle soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);;

#if defined(QCA_WIFI_QCA8074) && defined (BUILD_X86)
    uint32_t lowmem_alloc_tries = 0;
    struct sk_buff *skb_orig;

    /* Hawkeye M2M emulation cannot handle memory addresses below 0x50000000
     * Though we are trying to reserve low memory upfront to prevent this,
     * we sometimes see SKBs allocated from low memory.
     */
    skb_orig = skb;
    while (virt_to_phys(skb->data) < 0x50000040) {
        lowmem_alloc_tries++;
        if (lowmem_alloc_tries > 100) {
            if (skb_orig->sk) {
                /* account system memory */
                atomic_sub(skb_orig->truesize,
                    &(skb_orig->sk->sk_wmem_alloc));
            }
            return 0;
        } else {
            skb = skb_copy(skb_orig, GFP_KERNEL);
        }
    }
#endif

    cdp_txrx_set_pdev_param(soc_txrx_handle, (struct cdp_pdev *)pdev, CDP_INGRESS_STATS, 1);
    qdf_nbuf_set_timestamp(skb);

    nbuf_debug_add_record(skb);
    if (!osif_ol_process_tx(skb, dev)) {
        return 0;
    }

    spin_lock_bh(&osdev->tx_lock);

#if DBDC_REPEATER_SUPPORT
    /*
     *Skip dbdc process when vap is mpsta or if extap is enabled
     */
#if ATH_SUPPORT_WRAP
    if (qdf_unlikely(wlan_is_mpsta(vap))) {
        goto skip_lag_tx_process;
    }
#endif
    if (qdf_unlikely(dp_is_extap_enabled(vdev))) {
        goto skip_lag_tx_process;
    }
    if (dp_lag_tx_process(vdev, skb, DP_LAG_SEC_VAP_SEND)) {
        spin_unlock_bh(&osdev->tx_lock);
        return 0;
    }
#endif

    is_son_enabled = wlan_get_param(vap, IEEE80211_CONFIG_FEATURE_SON_NUM_VAP);

    if (vap->iv_ic->ic_get_tgt_type(vap->iv_ic) != TARGET_TYPE_QCA8074V2) {
        if (
#if DBDC_REPEATER_SUPPORT
            !vap->iv_ic->ic_primary_radio &&
            (dp_lag_soc_is_multilink(vdev) ||
            is_son_enabled) &&
#else
	    is_son_enabled &&
#endif
            wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {

            is_exception = true;

            tx_exc_param->tid = CDP_INVALID_TID;
            tx_exc_param->peer_id = CDP_INVALID_PEER;
            tx_exc_param->tx_encap_type = htt_cmn_pkt_type_ethernet;
            tx_exc_param->sec_type = cdp_sec_type_none;
        }

        if (is_son_enabled &&
#if DBDC_REPEATER_SUPPORT
                !vap->iv_ic->ic_primary_radio &&
#endif
                wlan_vdev_mlme_get_opmode(vdev) == QDF_SAP_MODE) {
            eh = (struct ether_header *)skb->data;
            soc = wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(vap->iv_ic->ic_pdev_obj));

            pdev_id = wlan_objmgr_pdev_get_pdev_id(vap->iv_ic->ic_pdev_obj);
            ast_entry_found = cdp_peer_get_ast_info_by_pdev((struct cdp_soc_t *)soc, eh->ether_dhost,
                                           pdev_id, &ast_entry_info);

            if (ast_entry_found && ast_entry_info.type == CDP_TXRX_AST_TYPE_WDS_HM_SEC) {
                is_exception = true;
                tx_exc_param->tid = CDP_INVALID_TID;
                tx_exc_param->tx_encap_type = htt_cmn_pkt_type_ethernet;
                tx_exc_param->sec_type = cdp_sec_type_none;
                tx_exc_param->peer_id = ast_entry_info.peer_id;
            }
        }
    }
skip_lag_tx_process:
#if ATH_SUPPORT_WRAP
#if MESH_MODE_SUPPORT
    if (!vap->iv_mesh_vap_mode) {
#endif
       spin_unlock_bh(&osdev->tx_lock);
       OL_WRAP_TX_PROCESS_NO_LOCK(&osdev, vap, &skb);
       spin_lock_bh(&osdev->tx_lock);
#if MESH_MODE_SUPPORT
    }
#endif
#endif

    if (!osif_ol_process_tx_synchronous(&skb, &osdev, tx_exc_param, &is_exception)) {
        spin_unlock_bh(&osdev->tx_lock);
        return 0;
    }

    spin_unlock_bh(&osdev->tx_lock);

    if (is_exception) {
        /*
         * Additional reference taken here so that on MSDU completions nbuf is not freed.
         * And on PPDU completions the ppdu cookie can be given back to the customer stack
         */
        if (tx_exc_param->is_tx_sniffer)
            qdf_nbuf_ref(skb);

        skb = ((ol_txrx_tx_exc_fp)osdev->iv_vap_send_exc)(wlan_vdev_get_dp_handle(osdev->ctrl_vdev), skb,
                tx_exc_param);

        /*
         * Release the additional reference taken for sniffer
         */
        if (skb != NULL && tx_exc_param->is_tx_sniffer)
            qdf_nbuf_free(skb);
    } else {
        skb = ((ol_txrx_tx_fp)osdev->iv_vap_send)(wlan_vdev_get_dp_handle(osdev->ctrl_vdev), skb);
    }

#if ATH_SUPPORT_WRAP
bad:
#endif
    if (skb != NULL) {
        qdf_nbuf_free(skb);
    }
    return 0;
}

static inline void
osif_ol_vap_init_exception_metadata(struct cdp_tx_exception_metadata *tx_exc_param)
{
    memset(tx_exc_param, 0, sizeof(struct cdp_tx_exception_metadata));
    tx_exc_param->tx_encap_type = CDP_INVALID_TX_ENCAP_TYPE;
    tx_exc_param->sec_type = CDP_INVALID_SEC_TYPE;
    tx_exc_param->peer_id = CDP_INVALID_PEER;
    tx_exc_param->tid = CDP_INVALID_TID;
}

int
osif_ol_vap_send_exception_wifi3(struct sk_buff *skb, struct net_device *dev, void *mdata)
{
    struct tx_sniffer_meta_hdr *mhdr = (struct tx_sniffer_meta_hdr *)mdata;
    struct cdp_tx_exception_metadata tx_exc_param;

    osif_ol_vap_init_exception_metadata(&tx_exc_param);
    tx_exc_param.is_tx_sniffer = 1;
    tx_exc_param.ppdu_cookie = mhdr->ppdu_cookie;
    return osif_ol_vap_send_wifi3(skb, dev, &tx_exc_param, true);
}

int
osif_ol_vap_hardstart_wifi3(struct sk_buff *skb, struct net_device *dev)
{
    struct cdp_tx_exception_metadata tx_exc_param;
    osif_ol_vap_init_exception_metadata(&tx_exc_param);
    return osif_ol_vap_send_wifi3(skb, dev, &tx_exc_param, false);
}

/*
 * OS entry point for Fast Path 11AC offload data-path
 * NOTE : It is unlikely that we need lock protection
 * here, since we are called under OS lock (HARD_TX_LOCK()))
 * in linux. So this function is can be called by a single
 * at a given instance of time.

 * TODO : This function does not implement full fledged packet
 * batching. This function receives a single packet and calls
 * the underlying API. The subsequent OL layer API's however,
 * can operate on batch of packets and hence is provided a packet
 * array (special case, array size = 1).
 */
int
osif_ol_ll_vap_hardstart(struct sk_buff *skb, struct net_device *dev)
{

    osif_dev  *osdev = ath_netdev_priv(dev);
    wlan_if_t vap = osdev->os_if;
    ol_txrx_soc_handle soc_txrx_handle =
        wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(vap->iv_ic->ic_pdev_obj));
    struct cdp_vdev *iv_txrx_handle = wlan_vdev_get_dp_handle(vap->vdev_obj);
    struct cdp_host_stats_ops *stats_ops = soc_txrx_handle->ops->host_stats_ops;
#if QCA_PARTNER_DIRECTLINK_TX
    struct ether_header *eh;
#endif
    struct cdp_tx_exception_metadata tx_exc_param = {0};
    bool is_exception = false;
#if UMAC_VOW_DEBUG
    struct ol_txrx_nbuf_classify nbuf_class;
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 0)) && MIPS_TP_ENHC
    prefetch(&skb->len);
    prefetch(skb->data);
#endif
    if (stats_ops->txrx_update_vdev_stats) {
        stats_ops->txrx_update_vdev_stats(iv_txrx_handle, skb,
                                          OL_TXRX_VDEV_STATS_TX_INGRESS);
    }

    nbuf_debug_add_record(skb);
    qdf_nbuf_set_ext_cb(skb, NULL);
    if (!osif_ol_process_tx(skb, dev)) {
        return 0;
    }

    /*
     * Classify packet for Video and query nss driver action whether
     * to accelerate the packet or drop it
     */
#if UMAC_VOW_DEBUG
    if (qdf_unlikely(osdev->carrier_vow_config)) {
        cdp_txrx_classify_and_update(soc_txrx_handle,
                wlan_vdev_get_dp_handle(osdev->ctrl_vdev), skb, tx_direction, &nbuf_class);
    }
#endif
    OSIF_VAP_TX_LOCK(soc_txrx_handle, osdev);

    /* Update packet count */
    /* Note: it is for the user to use skb_unshare with caution */

#if QCA_PARTNER_DIRECTLINK_TX
    if (qdf_unlikely(osdev->is_directlink)) {
            OSIF_VAP_TX_UNLOCK(soc_txrx_handle, osdev);
            ol_tx_partner(skb, dev, CDP_INVALID_PEER);
            return 0;
    } else
#endif /* QCA_PARTNER_DIRECTLINK_TX */

    {
#if MESH_MODE_SUPPORT
        if (!vap->iv_mesh_vap_mode)
#endif
        {
#if DBDC_REPEATER_SUPPORT
            if(dbdc_tx_process(vap, &osdev, skb)) {
		goto out;
            }
#endif
            OL_WRAP_TX_PROCESS(&osdev, vap, &skb);
        }

        if (!osif_ol_process_tx_synchronous(&skb, &osdev, &tx_exc_param, &is_exception)) {
            goto out;
        }

        if (qdf_unlikely(is_exception)) {
            skb = ((ol_txrx_tx_exc_fp)osdev->iv_vap_send_exc)(wlan_vdev_get_dp_handle(osdev->ctrl_vdev),
                                                              skb, &tx_exc_param);
        } else {
            skb = ((ol_txrx_tx_fp)osdev->iv_vap_send)(wlan_vdev_get_dp_handle(osdev->ctrl_vdev), skb);
        }

        if (skb != NULL)
	    goto bad;

    out:
        OSIF_VAP_TX_UNLOCK(soc_txrx_handle, osdev);
        return 0;
    }

bad:
    OSIF_VAP_TX_UNLOCK(soc_txrx_handle, osdev);

    if (skb != NULL)
        qdf_nbuf_free(skb);
    return 0;
}

#endif /* WLAN_FEATURE_FASTPATH */

#if ATH_PERF_PWR_OFFLOAD
#if UMAC_SUPPORT_PROXY_ARP
extern int do_proxy_arp(wlan_if_t vap, qdf_nbuf_t netbuf);
#endif /* UMAC_SUPPORT_PROXY_ARP */
extern void osif_receive_monitor_80211_base (os_if_t osif, wbuf_t wbuf,
                                        ieee80211_recv_status *rs);

extern int
osif_mcast_me_convert(osif_dev *osdev, qdf_nbuf_t netbuf)
{
#if (ATH_SUPPORT_HYFI_ENHANCEMENTS || ATH_SUPPORT_ME_SNOOP_TABLE)
    wlan_if_t vap = osdev->os_if;
#endif

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    if (vap->iv_me->me_hifi_enable && vap->iv_ique_ops.me_hifi_convert)
        return vap->iv_ique_ops.me_hifi_convert(vap, netbuf);
#endif

#if ATH_SUPPORT_ME_SNOOP_TABLE
    if (vap->iv_ique_ops.me_convert)
        return vap->iv_ique_ops.me_convert(vap, netbuf);
#endif
    return 0;
}

QDF_STATUS osif_getkey_ol(osif_dev *osdev, uint8_t *key_buf, uint8_t *mac_addr, uint8_t keyix)
{
    wlan_if_t vap = osdev->os_if;
    ieee80211_keyval k = {0};

    k.keydata = key_buf;
    if (wlan_get_key(vap, keyix, mac_addr, &k, IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE)) {
        return QDF_STATUS_E_FAILURE;
    }

    return QDF_STATUS_SUCCESS;
}

#if UMAC_SUPPORT_PROXY_ARP
int
osif_proxy_arp_ol(os_if_t osif, qdf_nbuf_t netbuf)
{
    osif_dev  *osdev = (osif_dev *)osif;
    wlan_if_t vap = osdev->os_if;

    return(do_proxy_arp(vap, netbuf));
}
#endif /* UMAC_SUPPORT_PROXY_ARP */

#if ATH_SUPPORT_WAPI
extern bool osif_wai_check(os_if_t osif,
                struct sk_buff *skb_list_head, struct sk_buff *skb_list_tail);
#endif /* ATH_SUPPORT_WAPI */

void osif_vap_setup_ol (struct ieee80211vap *vap, osif_dev *osifp) {
        struct ol_txrx_ops ops = {0};
        struct ieee80211com *ic = vap->iv_ic;
        ol_txrx_vdev_handle iv_txrx_handle;
        ol_txrx_soc_handle soc_txrx_handle;

        /*
         * Get vdev data handle for all down calls to offload data path.
         */
        iv_txrx_handle = wlan_vdev_get_dp_handle(vap->vdev_obj);
        if (!iv_txrx_handle) {
            IEEE80211_DPRINTF(
                vap, IEEE80211_MSG_ANY, "%s : Bad ol_data_handle\n", __func__);
            return;
        }
        soc_txrx_handle = wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(ic->ic_pdev_obj));
        /*
         * This function registers rx and monitor functions,
         * and a callback handle.
         * It fills in the transmit handler to be called from shim.
         */
        ops.rx.rx = (ol_txrx_rx_fp) osif_deliver_data_ol;
        ops.get_key = (ol_txrx_get_key_fp) osif_getkey_ol;
#if QCA_SUPPORT_RAWMODE_PKT_SIMULATION
        ops.rx.rsim_rx_decap = (ol_txrx_rsim_rx_decap_fp) osif_rsim_rx_decap;
#endif
#if MESH_MODE_SUPPORT
	ops.tx.tx_free_ext = (ol_txrx_tx_free_ext_fp)os_if_tx_free_ext;
#endif

#if ATH_SUPPORT_WAPI
        ops.rx.wai_check = (ol_txrx_rx_check_wai_fp) osif_wai_check;
#endif
        ops.rx.mon = (ol_txrx_rx_mon_fp) osif_receive_monitor_80211_base;
#if UMAC_SUPPORT_PROXY_ARP
        ops.proxy_arp = (ol_txrx_proxy_arp_fp) osif_proxy_arp_ol;
#endif
        ops.me_convert = (ol_txrx_mcast_me_fp) osif_mcast_me_convert;

        cdp_vdev_register(soc_txrx_handle,
            (struct cdp_vdev *)iv_txrx_handle, (void *) osifp,
            (void *)vap->vdev_obj, &ops);
        osifp->iv_vap_send = ops.tx.tx;
        osifp->iv_vap_send_exc = ops.tx.tx_exception;

        /* Override the Tx lock for OL with OL specific lock */
        osifp->tx_dev_lock_acquire = ol_ath_vap_tx_lock;
        osifp->tx_dev_lock_release = ol_ath_vap_tx_unlock;

        osifp->nss_nwifi = cdp_get_nwifi_mode(soc_txrx_handle, (struct cdp_vdev *)iv_txrx_handle);
        osifp->is_ar900b = cdp_is_target_ar900b(soc_txrx_handle, (struct cdp_vdev *)iv_txrx_handle);
        return;
}
#endif /* ATH_PER_PWR_OFFLOAD */



#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
int osif_nss_ol_extap_rx(struct net_device *dev, struct sk_buff *skb)
{
    osif_dev  *osdev = ath_netdev_priv(dev);
    struct wlan_objmgr_vdev *vdev = osdev->ctrl_vdev;
    int nwifi = osdev->nss_nwifi;

    return ADP_EXT_AP_RX_PROCESS(vdev, skb, &nwifi);
}

int osif_nss_ol_extap_tx(struct net_device *dev, struct sk_buff *skb)
{
    osif_dev  *osdev = ath_netdev_priv(dev);
    wlan_if_t vap = osdev->os_if;
    int status;
    struct wlan_objmgr_vdev *vdev = osdev->ctrl_vdev;
    struct dp_extap_nssol extap_nssol;
    uint16_t ip_version;
    struct net_device *comdev = (osdev)->os_comdev;
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(comdev);
    uint8_t src_mac[IEEE80211_ADDR_LEN];
    struct cdp_peer *peer = (struct cdp_peer *)wlan_peer_get_dp_handle(wlan_vdev_get_bsspeer(vdev));
    struct cdp_ast_entry_info ast_entry_info = {0};
    int ast_entry_found = 0;
    void *soc;
    qdf_ether_header_t *eh;

    OS_MEMSET(&extap_nssol, 0, sizeof(struct dp_extap_nssol));

    qdf_mem_copy(src_mac, (skb->data + vap->mhdr_len + ETH_ALEN), ETH_ALEN);
    status = ADP_EXT_AP_TX_PROCESS(vdev, &skb, vap->mhdr_len, &extap_nssol);

    if (!status) {
        /*
         * For HKv2, add HM_SEC ast entry for EXTAP Repeater ethernet backend
         */
        if ((lmac_get_tgt_type(scn->soc->psoc_obj) == TARGET_TYPE_QCA8074V2) &&
            dp_lag_is_enabled(vdev) &&
            (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)) {
            eh = (qdf_ether_header_t *)(skb->data + vap->mhdr_len);
            if (IEEE80211_IS_MULTICAST(eh->ether_dhost) && vap->iv_ic->nss_radio_ops && peer) {
                soc = wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(vap->iv_ic->ic_pdev_obj));
                ast_entry_found = cdp_peer_get_ast_info_by_soc((struct cdp_soc_t *)soc, src_mac,
                                                                &ast_entry_info);
                if (!ast_entry_found) {
                    cdp_peer_add_ast((struct cdp_soc_t *)soc, peer, src_mac, CDP_TXRX_AST_TYPE_WDS_HM_SEC, IEEE80211_NODE_F_WDS_HM);
                    vap->iv_ic->nss_radio_ops->ic_nss_ol_pdev_add_wds_peer(scn, NULL, src_mac, CDP_TXRX_AST_TYPE_WDS_HM_SEC, peer);
                }
            }
        }
    }

    ip_version = extap_nssol.ip_version;
    if ((ip_version != 0) && osdev->nss_wifiol_ctx && vap->iv_ic->nss_vops) {
        vap->iv_ic->nss_vops->ic_osif_nss_vdev_extap_table_entry_add(osdev, ip_version,
                (uint8_t *)&extap_nssol.ip, extap_nssol.mac);
    }

    return status;
}


int
osif_nss_ol_vap_hardstart(struct sk_buff *skb, struct net_device *dev)
{
    osif_dev  *osdev ;
    wlan_if_t vap ;
    struct wlan_objmgr_vdev *vdev;
    ol_txrx_soc_handle soc_txrx_handle;
    struct cdp_vdev *iv_txrx_handle;
    struct cdp_host_stats_ops *stats_ops;
#if DBDC_REPEATER_SUPPORT || QCA_OL_VLAN_WAR
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *pscn = NULL;
#endif

    nbuf_debug_add_record(skb);
    qdf_nbuf_set_ext_cb(skb, NULL);
    if (qdf_unlikely((dev->flags & (IFF_RUNNING|IFF_UP)) != (IFF_RUNNING|IFF_UP))) {
        qdf_nbuf_free(skb);
        return 0;
    }

    osdev = ath_netdev_priv(dev);

    if (qdf_unlikely(osdev->os_opmode == IEEE80211_M_MONITOR)) {
        qdf_nbuf_free(skb);
        return 0;
    }

    vap = osdev->os_if;
    vdev = osdev->ctrl_vdev;

    soc_txrx_handle = wlan_psoc_get_dp_handle(
		              wlan_pdev_get_psoc(vap->iv_ic->ic_pdev_obj));
    stats_ops = soc_txrx_handle->ops->host_stats_ops;
    iv_txrx_handle = wlan_vdev_get_dp_handle(vap->vdev_obj);
    if (stats_ops->txrx_update_vdev_stats) {
        stats_ops->txrx_update_vdev_stats(iv_txrx_handle, skb,
                                          OL_TXRX_VDEV_STATS_TX_INGRESS);
    }

    if ((wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) ||
                    (IEEE80211_IS_CHAN_RADAR(vap->iv_bsschan))) {
        qdf_nbuf_free(skb);
        return 0;
    }

    if (qdf_unlikely(!qdf_nbuf_is_tso(skb) != 0)) {
        skb = qdf_nbuf_unshare(skb);
        if (skb == NULL)
            return 0;
    }

#if UMAC_SUPPORT_WNM
    if (wlan_wnm_tfs_filter(vap, (wbuf_t) skb)) {
        goto bad;
    }
#endif

#if DBDC_REPEATER_SUPPORT || QCA_OL_VLAN_WAR
        ic = vap->iv_ic;
        pscn = (struct ol_ath_softc_net80211 *)ic;
#endif

#if ATH_SUPPORT_WRAP
    if (vap->iv_mpsta || vap->iv_psta) {
#if DBDC_REPEATER_SUPPORT
        if ((lmac_get_tgt_type(pscn->soc->psoc_obj) != TARGET_TYPE_QCA8074) &&
                (lmac_get_tgt_type(pscn->soc->psoc_obj) != TARGET_TYPE_QCA8074V2) &&
                (lmac_get_tgt_type(pscn->soc->psoc_obj) != TARGET_TYPE_QCA6018)) {
                if (dbdc_tx_process(vap, &osdev, skb)) {
                    return 0;
                }
                vap = osdev->os_if;
        } else {
            /*
             * TODO: ADD DBDC QWRAP Host processing changes
             */
        }
#endif
        if (osdev->nss_wifiol_ctx && vap->iv_ic->nss_vops)
            vap->iv_ic->nss_vops->ic_osif_nss_vdev_process_mpsta_tx(dev, skb);
        return 0;
    }
#endif
    if (OL_CFG_RAW_TX_LIKELINESS(vap->iv_tx_encap_type == osif_pkt_type_raw)) {
        /* In Raw Mode, the payload normally comes encrypted by an external
         * Access Controller and we won't have the keys. Besides, the format
         * isn't 802.3/Ethernet II.
         * Hence, VLAN WAR, Ext AP functionality, VoW debug and Multicast to
         * Unicast conversion aren't applicable, and are skipped.
         *
         * Additionally, TSO and nr_frags based scatter/gather are currently
         * not required and thus not supported with Raw Mode.
         *
         * Error conditions are handled internally by the below function.
         */
        OL_TX_LL_UMAC_RAW_PROCESS(dev, &skb);
        goto out;
    }

#if (MESH_MODE_SUPPORT && QCA_SUPPORT_RAWMODE_PKT_SIMULATION)
    if (vap->iv_mesh_vap_mode && (vap->mdbg & MESH_DEBUG_ENABLED)) {
        if (add_mesh_meta_hdr(skb, vap)) {
            goto bad;
        }
    }
#endif

    if (dp_is_extap_enabled(vdev) &&
            (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)) {
#if DBDC_REPEATER_SUPPORT
        if ((lmac_get_tgt_type(pscn->soc->psoc_obj) != TARGET_TYPE_QCA8074) &&
                (lmac_get_tgt_type(pscn->soc->psoc_obj) != TARGET_TYPE_QCA8074V2) &&
                (lmac_get_tgt_type(pscn->soc->psoc_obj) != TARGET_TYPE_QCA6018)) {

            if (dbdc_tx_process(vap, &osdev, skb)) {
                return 0;
            }
            vap = osdev->os_if;
        } else {
            if (dp_lag_tx_process(vdev, skb, DP_LAG_SEC_SKIP_VAP_SEND)) {
                return 0;
            }
        }
#endif

        if (osdev->nss_wifiol_ctx && vap->iv_ic->nss_vops)
            vap->iv_ic->nss_vops->ic_osif_nss_vdev_process_extap_tx(dev, skb);

        return 0;
    }

#if QCA_OL_VLAN_WAR
    if(OL_TX_VLAN_WAR(&skb, pscn))
        goto bad;
#endif /* QCA_OL_VLAN_WAR */

    if (osdev->nss_wifiol_ctx && vap->iv_ic->nss_vops && vap->iv_ic->nss_vops->ic_osif_nss_vap_xmit(osdev, skb)) {
        goto bad;
    }

out:
    return 0;

bad:
    if (skb != NULL) {
        qdf_nbuf_free(skb);
    }
    return 0;
}
qdf_export_symbol(osif_nss_ol_vap_hardstart);

#endif /* QCA_NSS_WIFI_OFFLOAD_SUPPORT */

void osif_rawsim_getastentry (struct ieee80211vap *vap, qdf_nbuf_t *pnbuf, void *raw_ast)
{
    struct ieee80211com *ic = vap->iv_ic;

    ol_txrx_soc_handle soc_txrx_handle =
        wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(ic->ic_pdev_obj));
    ol_txrx_vdev_handle vdev = wlan_vdev_get_dp_handle(vap->vdev_obj);

    cdp_rawsim_get_astentry(soc_txrx_handle, (struct cdp_vdev *)vdev, pnbuf, (struct cdp_raw_ast *)raw_ast);
}

void osif_get_peer_mac_from_peer_id(struct wlan_objmgr_pdev *pdev, uint32_t peer_id, uint8_t *peer_mac)
{
    struct wlan_objmgr_psoc *psoc = NULL;
    ol_txrx_pdev_handle pdev_txrx_handle = NULL;
    ol_txrx_soc_handle soc_txrx_handle = NULL;

    if (!pdev) {
        return;
    }
    psoc = wlan_pdev_get_psoc(pdev);
    if (!psoc) {
        return;
    }

    pdev_txrx_handle = wlan_pdev_get_dp_handle(pdev);
    soc_txrx_handle = wlan_psoc_get_dp_handle(psoc);
    if (pdev_txrx_handle && peer_mac) {
        cdp_get_peer_mac_from_peer_id(soc_txrx_handle, (void *)pdev_txrx_handle, peer_id, peer_mac);
    }
}

int osif_get_sec_type(wlan_if_t vap, struct ieee80211_node *peer, uint8_t sec_idx)
{
    ol_txrx_soc_handle soc_txrx_handle =
                    wlan_psoc_get_dp_handle(wlan_pdev_get_psoc(vap->iv_ic->ic_pdev_obj));

    return cdp_get_sec_type(soc_txrx_handle, (struct cdp_peer *) peer, sec_idx);
}

void osif_deliver_tx_capture_data(osif_dev *osifp, struct sk_buff *skb)
{
    skb->dev = osifp->netdev;
    skb->pkt_type = PACKET_USER;
    skb->ip_summed = CHECKSUM_UNNECESSARY;
    skb->protocol = eth_type_trans(skb, osifp->netdev);
    netif_rx(skb);
}
