/*
 * Copyright (c) 2011-2014,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010-2011, Atheros Communications Inc.
 */
#include <qdf_nbuf.h>
#include <qdf_module.h>
#include "dp_txrx.h"
#include <cdp_txrx_cmn.h>
#include <ol_if_athvar.h>
#include <ieee80211.h>
#include <ieee80211_defines.h>
#include <osif_private.h>
#include <cdp_txrx_ctrl.h>
#include <htt_common.h>
#include <dp_extap.h>

void dp_lag_pdev_set_priority(struct wlan_objmgr_pdev *pdev, uint8_t value)
{
    dp_pdev_link_aggr_t *pdev_lag;
    pdev_lag = dp_pdev_get_lag_handle(pdev);
    if (!pdev_lag)
        return;

    pdev_lag->priority = value;
}
qdf_export_symbol(dp_lag_pdev_set_priority);

void dp_lag_pdev_set_primary_radio(struct wlan_objmgr_pdev *pdev, bool value)
{
    dp_pdev_link_aggr_t *pdev_lag;
    pdev_lag = dp_pdev_get_lag_handle(pdev);
    if (!pdev_lag)
        return;

    pdev_lag->is_primary = value;

    qdf_print("pdev(%p) is_primary %d ", pdev_lag, value);
}
qdf_export_symbol(dp_lag_pdev_set_primary_radio);

void dp_lag_pdev_set_sta_vdev(struct wlan_objmgr_pdev *pdev, void *vdev)
{
    dp_pdev_link_aggr_t *pdev_lag;
    pdev_lag = dp_pdev_get_lag_handle(pdev);
    if (!pdev_lag)
        return;

    pdev_lag->sta_vdev = vdev;

    qdf_print("%s pdev(%p) sta_vdev %p ", __func__, pdev_lag, vdev);
}
qdf_export_symbol(dp_lag_pdev_set_sta_vdev);

void dp_lag_pdev_set_ap_vdev(struct wlan_objmgr_pdev *pdev, void *vdev)
{
    dp_pdev_link_aggr_t *pdev_lag;
    pdev_lag = dp_pdev_get_lag_handle(pdev);
    if (!pdev_lag)
        return;

    pdev_lag->ap_vdev = vdev;

    qdf_print("%s pdev(%p) ap_vdev %p ", __func__, pdev_lag, vdev);
}

qdf_export_symbol(dp_lag_pdev_set_ap_vdev);

void dp_lag_soc_enable(struct wlan_objmgr_pdev *pdev, bool value)
{
    dp_pdev_link_aggr_t *pdev_lag;
    dp_soc_link_aggr_t *soc_lag;
    pdev_lag = dp_pdev_get_lag_handle(pdev);
    if (!pdev_lag)
        return;
    soc_lag = pdev_lag->soc_lag;
    if (!soc_lag)
        return;

    soc_lag->enable = value;

    qdf_print("pdev(%p) Enabling DBDC Repeater %d ", pdev_lag, value);
}
qdf_export_symbol(dp_lag_soc_enable);

void dp_lag_soc_set_force_client_mcast_traffic(struct wlan_objmgr_pdev *pdev, bool value)
{
    dp_pdev_link_aggr_t *pdev_lag;
    dp_soc_link_aggr_t *soc_lag;
    pdev_lag = dp_pdev_get_lag_handle(pdev);
    if (!pdev_lag)
        return;

    soc_lag = pdev_lag->soc_lag;
    if (!soc_lag)
        return;

    soc_lag->force_client_mcast_traffic = value;
}
qdf_export_symbol(dp_lag_soc_set_force_client_mcast_traffic);

void dp_lag_soc_set_always_primary(struct wlan_objmgr_pdev *pdev, bool value)
{
    dp_pdev_link_aggr_t *pdev_lag;
    dp_soc_link_aggr_t *soc_lag;
    pdev_lag = dp_pdev_get_lag_handle(pdev);
    if (!pdev_lag)
        return;

    soc_lag = pdev_lag->soc_lag;
    if (!soc_lag)
        return;

    soc_lag->always_primary = value;

    qdf_print("pdev(%p) always_primary %d ", pdev, value);
}
qdf_export_symbol(dp_lag_soc_set_always_primary);

void dp_lag_soc_set_num_sta_vdev(struct wlan_objmgr_pdev *pdev, int num)
{
    dp_pdev_link_aggr_t *pdev_lag;
    dp_soc_link_aggr_t *soc_lag;
    struct ieee80211vap *vap = NULL;
    struct cdp_vdev *cdp_vdev = NULL;
    uint8_t i;

    pdev_lag = dp_pdev_get_lag_handle(pdev);
    if (!pdev_lag)
        return;

    soc_lag = pdev_lag->soc_lag;
    if (!soc_lag)
        return;

    qdf_print("%p DP AST flush ", soc_lag->soc);

    /* Flush AST table, whenever topology changes i.e STA VAP gets connected
     * or disconnected */
    cdp_peer_flush_ast_table(soc_lag->soc);

    soc_lag->num_stavaps_up = num;

    if (soc_lag->num_stavaps_up == 1) {
        soc_lag->is_multilink = 0;
        /*
         * Enable WDS learning on secondary radio if primary radio is down
         */
        for (i = 0; i < soc_lag->num_radios; i++) {
            pdev_lag = soc_lag->pdev[i];
            vap = wlan_vdev_get_mlme_ext_obj((struct wlan_objmgr_vdev *)pdev_lag->sta_vdev);
            if (!vap)
                return;

            if (!pdev_lag->is_primary && pdev_lag->sta_vdev) {
#if ATH_SUPPORT_WRAP
                if (qdf_unlikely(dp_is_extap_enabled(pdev_lag->sta_vdev) || wlan_is_psta(vap)))
                    return;
#endif
                cdp_vdev = (struct cdp_vdev *)wlan_vdev_get_dp_handle(pdev_lag->sta_vdev);
                cdp_txrx_set_vdev_param(soc_lag->soc,
                    cdp_vdev, CDP_ENABLE_WDS, 1);
                cdp_txrx_set_vdev_param(soc_lag->soc,
                    cdp_vdev, CDP_ENABLE_MEC, 1);
                cdp_txrx_set_vdev_param(soc_lag->soc,
                    cdp_vdev, CDP_ENABLE_DA_WAR, 1);
            }
        }
    }
}
qdf_export_symbol(dp_lag_soc_set_num_sta_vdev);

void dp_lag_soc_set_drop_secondary_mcast(struct wlan_objmgr_pdev *pdev, bool flag)
{
    dp_pdev_link_aggr_t *pdev_lag;
    dp_soc_link_aggr_t *soc_lag;
    pdev_lag = dp_pdev_get_lag_handle(pdev);
    if (!pdev_lag)
        return;

    soc_lag = pdev_lag->soc_lag;
    if (!soc_lag)
        return;

    soc_lag->drop_secondary_mcast = flag;
}
qdf_export_symbol(dp_lag_soc_set_drop_secondary_mcast);

bool dp_lag_pdev_is_primary(struct wlan_objmgr_vdev *vdev)
{
    struct dp_pdev_link_aggr *pdev_lag = dp_get_lag_handle(vdev);
    if (!pdev_lag)
        return false;

    return pdev_lag->is_primary;
}
qdf_export_symbol(dp_lag_pdev_is_primary);

bool dp_lag_soc_is_multilink(struct wlan_objmgr_vdev *vdev)
{
    struct dp_pdev_link_aggr *pdev_lag = dp_get_lag_handle(vdev);
    struct dp_soc_link_aggr *soc_lag;

    if (!pdev_lag)
        return false;

    soc_lag = pdev_lag->soc_lag;

    if (soc_lag)
        return (soc_lag->enable && soc_lag->is_multilink);

    return false;
}
qdf_export_symbol(dp_lag_soc_is_multilink);

bool dp_lag_is_enabled(struct wlan_objmgr_vdev *vdev)
{
    struct dp_pdev_link_aggr *pdev_lag = dp_get_lag_handle(vdev);
    struct dp_soc_link_aggr *soc_lag;

    if (!pdev_lag)
        return false;

    soc_lag = pdev_lag->soc_lag;

    if (soc_lag)
        return soc_lag->enable;

    return false;
}
qdf_export_symbol(dp_lag_is_enabled);

static void dp_vap_send_sec_sta(struct dp_pdev_link_aggr *pdev_lag, qdf_nbuf_t nbuf)
{
    struct cdp_tx_exception_metadata tx_exc_param = {0};
    struct dp_soc_link_aggr *soc_lag = pdev_lag->soc_lag;
    osif_dev *osdev = NULL;
    struct ieee80211vap *vap = wlan_vdev_get_mlme_ext_obj(
                                 (struct wlan_objmgr_vdev *)pdev_lag->sta_vdev);

    if (!vap)
        return;

    osdev = (osif_dev *)vap->iv_ifp;

    if (soc_lag->ast_override_support) {
        ((ol_txrx_tx_fp)osdev->iv_vap_send)(wlan_vdev_get_dp_handle(pdev_lag->sta_vdev), nbuf);
    } else {
        tx_exc_param.tid = HTT_INVALID_TID;
        tx_exc_param.peer_id = HTT_INVALID_PEER;
        tx_exc_param.tx_encap_type = htt_cmn_pkt_type_ethernet;
        tx_exc_param.sec_type = cdp_sec_type_none;

        ((ol_txrx_tx_exc_fp)osdev->iv_vap_send_exc)(wlan_vdev_get_dp_handle(pdev_lag->sta_vdev),
                nbuf, &tx_exc_param);
    }
}

/**
 * dp_lag_rx_ap() - Rx LAG processing for frames recieved on AP VAP
 * @pdev_la: pdev lag handle
 * @nbuf: frame
 *
 * Return: false: frame not consumed and should be processed further by caller
 *         true: frame consumed
 */
bool dp_lag_rx_ap(struct dp_pdev_link_aggr *pdev_lag, qdf_nbuf_t nbuf)
{
    struct dp_soc_link_aggr *soc_lag = pdev_lag->soc_lag;
    qdf_ether_header_t *eh = (qdf_ether_header_t *) qdf_nbuf_data(nbuf);
    osif_dev *osdev = NULL;
    uint8_t is_mcast;
    uint8_t is_eapol;
    uint8_t pdev_id;
    uint8_t ast_type;
    bool enqueue_to_sta_vap = false;
    struct net_device *dev = NULL;
    struct ieee80211vap *vap = NULL;
    struct cdp_ast_entry_info ast_entry_info = {0};
    int ast_entry_found = 0;

    /* If root AP is only single-band, just give the frame up the stack */
    if (!soc_lag->is_multilink || pdev_lag->is_primary
            || (soc_lag->num_stavaps_up < 2)) {
        return false;
    }

    is_mcast = IEEE80211_IS_MULTICAST(eh->ether_dhost);
    is_eapol = (eh->ether_type == htons(ETHERTYPE_PAE));

    /*
     * If it is mcast/broadcast frame, AST search cannot be done, so give
     * the frame up the stack
     * If it is EAPOL frame, just give the frame up the stack
     */
    if (is_mcast || is_eapol) {
        return false;
    }

    vap = wlan_vdev_get_mlme_ext_obj((struct wlan_objmgr_vdev *)pdev_lag->sta_vdev);
    if (!vap)
        return false;

    osdev = (osif_dev *)vap->iv_ifp;
    if (osdev)
        dev = osdev->netdev;

    ast_entry_found = cdp_peer_get_ast_info_by_soc(soc_lag->soc, eh->ether_dhost, &ast_entry_info);

#if ATH_SUPPORT_WRAP
    if (wlan_is_mpsta(vap) || dp_is_extap_enabled(pdev_lag->sta_vdev)) {
        if (ast_entry_found) {
            ast_type = ast_entry_info.type;
            if ((ast_type == CDP_TXRX_AST_TYPE_SELF) || (ast_type == CDP_TXRX_AST_TYPE_STATIC)) {
                return false;
            }
            /*
             * In EXTAP DBDC scenario, HMSEC ast entry is added for repeater backend.
             * Send the frame to bridge for further forwarding.
             */
            if (dp_is_extap_enabled(pdev_lag->sta_vdev) && (ast_type == CDP_TXRX_AST_TYPE_WDS_HM_SEC)) {
                return false;
            }
        } else if (dev) {
            dev->netdev_ops->ndo_start_xmit(nbuf, dev);
            return true;
        }
        return true;
    }
#else
    if (dp_is_extap_enabled(pdev_lag->sta_vdev)) {
        if (ast_entry_found) {
            ast_type = ast_entry_info.type;
            if ((ast_type == CDP_TXRX_AST_TYPE_SELF) || (ast_type == CDP_TXRX_AST_TYPE_STATIC))
                return false;
        } else if (dev) {
            dev->netdev_ops->ndo_start_xmit(nbuf, dev);
            return true;
        }
        return true;
    }
#endif

    /*
     * If the destination address is found in secondary radio's AST table
     * send the frame for transmission over secondary radio's STA VAP
     */

    /*
     * This condition is to ensure that when ast-entry is not found, the packet is given
     * to stack on HK V1, but on HK V2 the packet is given to RootAP via secondary STA.
     * This is because on HK V1 wds learning is enabled on STA and hence, RooTAP bridge and
     * backend will be added as WDS entries on primary STA Vap. But, on HK V2 wds learning on
     * STA is disabled, hence the only way unicast packets for whom ast-entry is not present
     * and are coming from Sta's behind secondary AP can reach RootAP bridge and backend is
     * by giving these packets to RootAP via secondary STA.
     */
    if (!ast_entry_found) {
        if (!soc_lag->ast_override_support) {
            QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,FL("shost %pM dhost %pM \n"), eh->ether_shost, eh->ether_dhost);
            return false;
        }


        if (pdev_lag->sta_vdev) {
            enqueue_to_sta_vap = true;
        }

    } else {
        /*
         * For a RAP <-> REP <-> REP cascade setup , for wds nodes behind
         * Repeater's primary AP VAP following condition needs some changes
         */
        pdev_id = ast_entry_info.pdev_id;
        ast_type = ast_entry_info.type;
        if ((pdev_id != pdev_lag->pdev_id) && pdev_lag->sta_vdev && (ast_type != CDP_TXRX_AST_TYPE_MEC)
            && (ast_type != CDP_TXRX_AST_TYPE_STATIC)) {
            enqueue_to_sta_vap = true;
        }
    }

    if (enqueue_to_sta_vap) {
        QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
                FL("shost %pM dhost %pM \n"), eh->ether_shost, eh->ether_dhost);
        dp_vap_send_sec_sta(pdev_lag, nbuf);
        return true;
    }

    return false;
}

/**
 * dp_lag_rx_sta() - Rx LAG processing for frames recieved on STA VAP
 * @pdev_la: pdev lag handle
 * @nbuf: frame
 *
 * Return: false: frame not consumed and should be processed further by caller
 *         true: frame consumed
 */
bool dp_lag_rx_sta(struct dp_pdev_link_aggr *pdev_lag, qdf_nbuf_t nbuf, bool vap_send)
{
    uint8_t pdev_id;
    uint8_t is_eapol;
    uint8_t is_mcast;
    uint8_t ast_type;
    int i;
    struct dp_soc_link_aggr *soc_lag = pdev_lag->soc_lag;
    qdf_ether_header_t *eh = (qdf_ether_header_t *) qdf_nbuf_data(nbuf);
    struct cdp_ast_entry_info ast_entry_info = {0};
    int ast_entry_found = 0;

    is_eapol = (eh->ether_type == htons(ETHERTYPE_PAE));

    if (is_eapol) {
        return false;
    }

    if (soc_lag->num_stavaps_up < 2) {
	    is_mcast = IEEE80211_IS_MULTICAST(eh->ether_dhost);

	    if (is_mcast) {
		ast_entry_found = cdp_peer_get_ast_info_by_soc(soc_lag->soc,
							       eh->ether_shost,
							       &ast_entry_info);

		if (!ast_entry_found) {
			QDF_TRACE(QDF_MODULE_ID_TXRX,
				  QDF_TRACE_LEVEL_DEBUG,FL("shost %pM dhost %pM \n"),
				  eh->ether_shost, eh->ether_dhost);
			return false;
		}

		pdev_id = ast_entry_info.pdev_id;
		ast_type = ast_entry_info.type;

                if (pdev_id != pdev_lag->pdev_id) {
                    qdf_nbuf_free(nbuf);
                    return true;
                }
	    }

            QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
                      "shost %pM dhost %pM \n",
		      eh->ether_shost, eh->ether_dhost);
            return false;
    }

    if (soc_lag->always_primary) {
        if(!pdev_lag->is_primary) {
            qdf_nbuf_free(nbuf);
            return true;
        }
    }

    is_mcast = IEEE80211_IS_MULTICAST(eh->ether_dhost);

    if (is_mcast) {
        if (soc_lag->is_multilink && !pdev_lag->is_primary) {
            QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
                FL("shost %pM dhost %pM \n"), eh->ether_shost, eh->ether_dhost);

            /* Todo Send Notification to IfaceMgr Daemon */
            qdf_nbuf_free(nbuf);
            return true;
        }

        ast_entry_found = cdp_peer_get_ast_info_by_soc(soc_lag->soc, eh->ether_shost, &ast_entry_info);

        if (!ast_entry_found) {
            QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,FL("shost %pM dhost %pM \n"), eh->ether_shost, eh->ether_dhost);
            return false;
        }

        pdev_id = ast_entry_info.pdev_id;
        ast_type = ast_entry_info.type;

        if (pdev_id != pdev_lag->pdev_id) {
            QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,FL("shost %pM dhost %pM \n"), eh->ether_shost, eh->ether_dhost);
            if (!soc_lag->is_multilink && (ast_type == CDP_TXRX_AST_TYPE_SELF)) {

                soc_lag->is_multilink = 1;

                /*
                 * Disable WDS learning  and MEC learning on secondary radio once multicast loop
                 * is detected
                 */
                for (i = 0; i < soc_lag->num_radios; i++) {
                    pdev_lag = soc_lag->pdev[i];
                    if (!pdev_lag->is_primary && pdev_lag->sta_vdev) {
                        cdp_txrx_set_vdev_param(soc_lag->soc,
                                (struct cdp_vdev *)wlan_vdev_get_dp_handle(pdev_lag->sta_vdev),
                                CDP_ENABLE_WDS, 0);
                        cdp_txrx_set_vdev_param(soc_lag->soc,
                                (struct cdp_vdev *)wlan_vdev_get_dp_handle(pdev_lag->sta_vdev),
                                CDP_ENABLE_MEC, 0);
                        cdp_txrx_set_vdev_param(soc_lag->soc,
                                (struct cdp_vdev *)wlan_vdev_get_dp_handle(pdev_lag->sta_vdev),
                                CDP_ENABLE_DA_WAR, 0);
                    }
                }

                qdf_print("Detected root AP shost %pM dhost %pM tx_pdev_id %d rx_pdev_id %d ",
                     eh->ether_shost, eh->ether_dhost, pdev_id,
                     pdev_lag->pdev_id);

                cdp_peer_flush_ast_table(soc_lag->soc);

                qdf_nbuf_free(nbuf);
                return true;
            }

            /*
             * In case of dual sta in one device where both the stations are connected to different AP device,
             * the backend's of the Root AP will be added as a MEC entry on other Radio Sta.
             * In this scenario we should not treat the MEC entry of other radio to drop a packet.
             */
             if (!soc_lag->is_multilink) {
                 if ((soc_lag->ast_override_support) && (ast_type == CDP_TXRX_AST_TYPE_MEC)
                         && (!soc_lag->drop_secondary_mcast)) {
                        return false;
                 }
             }

             /* Todo Send Notification to IfaceMgr Daemon */
             qdf_nbuf_free(nbuf);
             return true;
        }

        if (soc_lag->drop_secondary_mcast && !pdev_lag->is_primary) {
            QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,FL("shost %pM dhost %pM \n"), eh->ether_shost, eh->ether_dhost);
            qdf_nbuf_free(nbuf);
            return true;
        }
    }

    /*
     * Drop all multicast/broadcast frames received on secondary
     * radio
     */
    if (!pdev_lag->is_primary && soc_lag->is_multilink && pdev_lag->ap_vdev) {
            struct ieee80211vap *vap = wlan_vdev_get_mlme_ext_obj(
                                 (struct wlan_objmgr_vdev *)pdev_lag->ap_vdev);
            osif_dev *osdev;

            if (!vap) {
                qdf_debug("vap is NULL\n");
                return false;
            }
            osdev = (osif_dev *)vap->iv_ifp;

            QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,FL("shost %pM dhost %pM \n"), eh->ether_shost, eh->ether_dhost);
            if (vap_send) {
                ((ol_txrx_tx_fp)osdev->iv_vap_send)(wlan_vdev_get_dp_handle(pdev_lag->ap_vdev), nbuf);
                return true;
            }
    }

    return false;
}

/**
 * dp_lag_tx() - LAG processing for transmit frames on STA VAP
 * @pdev_la: pdev lag handle
 * @nbuf: frame
 *
 * Return: false: frame not consumed and should be processed further by caller
 *         true: frame consumed
 */
static bool dp_lag_tx(struct dp_pdev_link_aggr *pdev_lag, qdf_nbuf_t nbuf, bool vap_send)
{
    qdf_ether_header_t *eh = (qdf_ether_header_t *) qdf_nbuf_data(nbuf);
    struct dp_soc_link_aggr *soc_lag = pdev_lag->soc_lag;
    struct ieee80211vap *vap;
    osif_dev *osdev;
    uint8_t ast_type;
    uint8_t pdev_id = 7;
    struct cdp_ast_entry_info ast_entry_info = {0};
    int ast_entry_found = 0;

#ifdef HK_2_0
    bool is_mcast = 0;

    /*
     * Only Mcast/Broadcast frames are required to be processed
     * for LAG
     */
    is_mcast = IEEE80211_IS_MULTICAST(eh->ether_dhost);
    if (!is_mcast) {
        return false;
    }
#endif

    if (!soc_lag)
	    return false;

    if (soc_lag->is_multilink || soc_lag->always_primary ||
            soc_lag->force_client_mcast_traffic || soc_lag->drop_secondary_mcast) {

        ast_entry_found = cdp_peer_get_ast_info_by_soc(soc_lag->soc, eh->ether_shost, &ast_entry_info);

        if (ast_entry_found) {
            pdev_id = ast_entry_info.pdev_id;
	    ast_type = ast_entry_info.type;
        }

        QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
                FL("shost=%pM, pdev_id=%d , pdev_lag->pdev_id =%d, pdev_lag->is_primary=%d\n"),
                eh->ether_shost, pdev_id, pdev_lag->pdev_id, pdev_lag->is_primary);

        if (pdev_lag->is_primary) {
            if (soc_lag->always_primary)
                return false;

            if (!ast_entry_found) {
                QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
                        FL("shost %pM dhost %pM \n"), eh->ether_shost, eh->ether_dhost);
                return false;
            }

            /* The sender is a client of a non-primary radio */
            if (pdev_id != pdev_lag->pdev_id) {
                QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG, FL("shost %pM dhost %pM \n"), eh->ether_shost, eh->ether_dhost);
                goto drop_frame;
            }

        } else {
            /* Secondary Radio */

            /* If secondary STA vap delete is in progress, return from here */
            if (!pdev_lag->sta_vdev)
                return false;

            if (QDF_STATUS_SUCCESS !=
                wlan_objmgr_vdev_try_get_ref(pdev_lag->sta_vdev,
                                             WLAN_DEBUG_ID)) {
                QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
                          FL("Unable to get vdev reference \n"));
                return false;
            }

            vap = (wlan_if_t)wlan_vdev_get_mlme_ext_obj(
                                 (struct wlan_objmgr_vdev *)pdev_lag->sta_vdev);
             if (!vap) {
                wlan_objmgr_vdev_release_ref(pdev_lag->sta_vdev,
                                             WLAN_DEBUG_ID);
                QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
                          FL("VAP is NULL \n"));
                 return false;
             }

            osdev = (osif_dev *)vap->iv_ifp;

            if (!ast_entry_found) {
                wlan_objmgr_vdev_release_ref(pdev_lag->sta_vdev,
                                             WLAN_DEBUG_ID);
                goto drop_frame;
            }

            /* The sender is a client of a non-primary radio */
            if (pdev_id != pdev_lag->pdev_id) {
                wlan_objmgr_vdev_release_ref(pdev_lag->sta_vdev,
                                             WLAN_DEBUG_ID);
                goto drop_frame;
            }

            if (soc_lag->drop_secondary_mcast &&
                    (ast_type != CDP_TXRX_AST_TYPE_SELF)) {
                wlan_objmgr_vdev_release_ref(pdev_lag->sta_vdev,
                                             WLAN_DEBUG_ID);
                goto drop_frame;
            }

            if (soc_lag->always_primary) {
                if(qdf_likely(!(eh->ether_type == htons(ETHERTYPE_PAE)))) {
                    wlan_objmgr_vdev_release_ref(pdev_lag->sta_vdev,
                                                 WLAN_DEBUG_ID);
                    goto drop_frame;
                }
            }


            QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
                    FL("shost %pM dhost %pM \n"), eh->ether_shost,
                    eh->ether_dhost);

            if (vap_send) {
                dp_vap_send_sec_sta(pdev_lag, nbuf);
                wlan_objmgr_vdev_release_ref(pdev_lag->sta_vdev, WLAN_DEBUG_ID);
                return true;
            }
            wlan_objmgr_vdev_release_ref(pdev_lag->sta_vdev, WLAN_DEBUG_ID);
        }
    }

    return false;

drop_frame:
    QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_DEBUG,
            FL("dropping shost %pM dhost %pM \n"),
            eh->ether_shost, eh->ether_dhost);
    qdf_nbuf_free(nbuf);
    return true;
}

bool dp_lag_tx_process(struct wlan_objmgr_vdev *vdev, qdf_nbuf_t nbuf, bool vap_send)
{
    struct dp_pdev_link_aggr *pdev_lag = dp_get_lag_handle(vdev);

    if (qdf_unlikely(!pdev_lag))
	    return false;

    if (qdf_unlikely(pdev_lag->soc_lag->enable &&
                wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE)) {
        return dp_lag_tx(pdev_lag, nbuf, vap_send);
    }

    return false;
}
qdf_export_symbol(dp_lag_tx_process);

bool dp_lag_rx_process(struct wlan_objmgr_vdev *vdev, qdf_nbuf_t nbuf, bool vap_send)
{
    struct dp_pdev_link_aggr *pdev_lag = dp_get_lag_handle(vdev);

    if (qdf_unlikely(!pdev_lag))
        return false;

    if (qdf_likely(!pdev_lag->soc_lag->enable))
        return false;

    /*
     * For HK 2.0, since AST entries for nodes behind Root AP are also learnt in
     * AST table on STA VAP, DA based routing cannot be used (on AP VAP)
     * For HK 1.x , we can only depend on SA based filtering on Tx side on STA
     * VAP.
     */
    if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
        return dp_lag_rx_sta(pdev_lag, nbuf, vap_send);
    } else {
        return dp_lag_rx_ap(pdev_lag, nbuf);
    }

    return false;
}
qdf_export_symbol(dp_lag_rx_process);

void dp_lag_pdev_init(struct dp_pdev_link_aggr *pdev_lag,
        dp_soc_link_aggr_t *soc_lag,
        uint8_t pdev_id)
{
    int i;
    if (!pdev_lag || !soc_lag)
        return;

    i = soc_lag->num_radios;
    qdf_assert_always(i < DP_MAX_RADIO_CNT);

    soc_lag->enable = 1;

    soc_lag->pdev[i] = pdev_lag;
    soc_lag->num_radios++;

    pdev_lag->soc_lag = soc_lag;
    pdev_lag->pdev_id = pdev_id;
}
qdf_export_symbol(dp_lag_pdev_init);
