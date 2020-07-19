/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
#include <da_dp.h>
#include <da_output.h>
#include <dp_txrx.h>
void ath_amsdu_node_detach(struct wlan_objmgr_pdev *pdev,
			   struct wlan_objmgr_peer *peer);
struct dadp_ops *wlan_pdev_get_dadp_ops(struct wlan_objmgr_pdev *pdev);
void wlan_set_dadp_ops(struct wlan_objmgr_pdev *pdev, struct dadp_ops *dops);
void dadp_vdev_detach(struct wlan_objmgr_vdev *vdev);
void dadp_peer_detach(struct wlan_objmgr_peer *peer);

void dadp_vdev_set_privacy_filters(struct wlan_objmgr_vdev *vdev, void *filters,
				   u_int32_t num)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	qdf_mem_copy(&dp_vdev->privacy_filters, filters,
		     num * sizeof(privacy_exemption));
	dp_vdev->filters_num = num;
}

void dadp_vdev_set_osif_event_handlers(struct wlan_objmgr_vdev *vdev,
				       wlan_event_handler_table * evtab)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_evtable = evtab;
}

void dadp_vdev_set_ccx_event_handlers(struct wlan_objmgr_vdev *vdev,
				      os_if_t osif,
				      wlan_ccx_handler_table * evtab)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_ccx_arg = osif;
	dp_vdev->vdev_ccx_evtable = evtab;
}

void dadp_vdev_set_mcast_rate(struct wlan_objmgr_vdev *vdev, int mcast_rate)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_mcast_rate = mcast_rate;
}

int dadp_ique_attach(struct wlan_objmgr_vdev *vdev,
		     struct wlan_vdev_ique_ops *ique)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return QDF_STATUS_E_FAILURE;

	qdf_mem_copy(&dp_vdev->ique_ops, ique,
		     sizeof(struct wlan_vdev_ique_ops));
	return QDF_STATUS_SUCCESS;
}

void dadp_vdev_set_me_hifi_enable(struct wlan_objmgr_vdev *vdev,
				  u_int32_t me_hifi_enable)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->me_hifi_enable = me_hifi_enable;
}

void dadp_vdev_set_mc_snoop_enable(struct wlan_objmgr_vdev *vdev,
				   u_int32_t mc_snoop_enable)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->mc_snoop_enable = mc_snoop_enable;
}

void dadp_vdev_set_mc_mcast_enable(struct wlan_objmgr_vdev *vdev,
				   u_int32_t mc_mcast_enable)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->mc_mcast_enable = mc_mcast_enable;
}

void dadp_vdev_set_mc_discard_mcast(struct wlan_objmgr_vdev *vdev,
				    u_int32_t mc_discard_mcast)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->mc_discard_mcast = mc_discard_mcast;
}

void dadp_vdev_set_ampdu_subframes(struct wlan_objmgr_vdev *vdev,
				    u_int32_t ampdu)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->av_ampdu_subframes = ampdu;
}

u_int8_t dadp_vdev_get_ampdu_subframes(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return 0;

	return (dp_vdev->av_ampdu_subframes);
}

void dadp_vdev_set_amsdu(struct wlan_objmgr_vdev *vdev,
				    u_int32_t amsdu)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->av_amsdu = amsdu;
}

u_int8_t dadp_vdev_get_amsdu(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return 0;

	return (dp_vdev->av_amsdu);
}

void dadp_vdev_set_dscp_map_id(struct wlan_objmgr_vdev *vdev,
			       u_int32_t dscp_map_id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_dscp_map_id = dscp_map_id;
}

void dadp_vdev_set_smps(struct wlan_objmgr_vdev *vdev, int val)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_smps = val;
}

void dadp_peer_update_uapsd_ac_trigena(struct wlan_objmgr_peer *peer,
				       u_int8_t * uapsd_ac_trigena, int len)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	qdf_mem_copy(&dp_peer->peer_uapsd_ac_trigena, uapsd_ac_trigena, len);
}

void dadp_peer_update_uapsd_ac_delivena(struct wlan_objmgr_peer *peer,
					u_int8_t * uapsd_ac_delivena, int len)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	qdf_mem_copy(&dp_peer->peer_uapsd_ac_delivena, uapsd_ac_delivena, len);
}

void dadp_peer_set_uapsd_maxsp(struct wlan_objmgr_peer *peer,
			       u_int8_t uapsd_maxsp)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	dp_peer->peer_uapsd_maxsp = uapsd_maxsp;
}

void dadp_peer_update_uapsd_dyn_trigena(struct wlan_objmgr_peer *peer,
					int8_t * uapsd_dyn_trigena, int len)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	qdf_mem_copy(&dp_peer->peer_uapsd_dyn_trigena, uapsd_dyn_trigena, len);
}

void dadp_peer_update_uapsd_dyn_delivena(struct wlan_objmgr_peer *peer,
					 int8_t * uapsd_dyn_delivena, int len)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	qdf_mem_copy(&dp_peer->peer_uapsd_dyn_delivena, uapsd_dyn_delivena,
		     len);
}

void dadp_peer_set_nawds(struct wlan_objmgr_peer *peer)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	dp_peer->nawds = 1;
}

void dadp_pdev_set_enable_min_rssi(struct wlan_objmgr_pdev *pdev, u_int8_t val)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return;

	dp_pdev->pdev_min_rssi_enable = val;
}

void dadp_pdev_set_min_rssi(struct wlan_objmgr_pdev *pdev, int val)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return;

	dp_pdev->pdev_min_rssi = val;
}

void dadp_pdev_set_dropstaquery(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return;

	dp_pdev->pdev_dropstaquery = val;
}

void dadp_pdev_set_blkreportflood(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return;

	dp_pdev->pdev_blkreportflood = val;
}

void dadp_pdev_set_tid_override_queue_mapping(struct wlan_objmgr_pdev *pdev,
					      u_int32_t val)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return;

	dp_pdev->pdev_tid_override_queue_mapping = val;
}

void dadp_pdev_set_override_dscp(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return;

	dp_pdev->pdev_override_dscp = val;
}

void dadp_pdev_set_override_igmp_dscp(struct wlan_objmgr_pdev *pdev,
				      u_int32_t val)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return;

	dp_pdev->pdev_override_igmp_dscp = val;
}

void dadp_pdev_set_override_hmmc_dscp(struct wlan_objmgr_pdev *pdev,
				      u_int32_t val)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return;

	dp_pdev->pdev_override_hmmc_dscp = val;
}

void dadp_pdev_set_dscp_igmp_tid(struct wlan_objmgr_pdev *pdev, u_int8_t tid)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return;

	dp_pdev->pdev_dscp_igmp_tid = tid;
}

void dadp_pdev_set_dscp_hmmc_tid(struct wlan_objmgr_pdev *pdev, u_int8_t tid)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return;

	dp_pdev->pdev_dscp_hmmc_tid = tid;
}

void dadp_vdev_set_priority_dscp_tid_map(struct wlan_objmgr_vdev *vdev, u_int8_t tid)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(wlan_vdev_get_pdev(vdev));
	int count =0;
	if (!dp_pdev || !dp_vdev)
		return;

    while(count < DA_HOST_DSCP_MAP_MAX) {
        dp_pdev->pdev_dscp_tid_map[dp_vdev->
            vdev_dscp_map_id][count] = tid;
        count++;
    }
}
void dadp_vdev_set_dscp_tid_map(struct wlan_objmgr_vdev *vdev, u_int8_t tos,
				u_int8_t tid)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(wlan_vdev_get_pdev(vdev));
	if (!dp_pdev)
		return;

	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return;
	}

	dp_pdev->pdev_dscp_tid_map[dp_vdev->
				   vdev_dscp_map_id][(tos >> IP_DSCP_SHIFT) &
						     IP_DSCP_SHIFT] = tid;
}

void dadp_vdev_set_frag_threshold(struct wlan_objmgr_vdev *vdev,
				  u_int16_t fragthreshold)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return;
	}

	dp_vdev->vdev_fragthreshold = fragthreshold;
}

void dadp_vdev_set_rtsthreshold(struct wlan_objmgr_vdev *vdev,
				u_int16_t rtsthreshold)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return;
	}

	dp_vdev->vdev_rtsthreshold = rtsthreshold;
}

struct wlan_pdev_phy_stats *dadp_pdev_get_phy_stats(struct wlan_objmgr_pdev
						    *pdev)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return NULL;

	return (dp_pdev->pdev_phy_stats);
}

void dadp_vdev_set_smartnet_enable(struct wlan_objmgr_vdev *vdev, u_int32_t val)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_smartnet_enable = val;
}

void dadp_vdev_set_prot_mode(struct wlan_objmgr_vdev *vdev, u_int32_t val)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_protmode = val;
}

void dadp_vdev_set_tspecActive(struct wlan_objmgr_vdev *vdev, u_int8_t val)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_tspecActive = val;
}

void dadp_vdev_set_txrxbytes(struct wlan_objmgr_vdev *vdev, u_int64_t val)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_txrxbytes = val;
}

u_int64_t dadp_vdev_get_txrxbytes(struct wlan_objmgr_vdev * vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return 0;

	return dp_vdev->vdev_txrxbytes;
}

systime_t dadp_vdev_get_last_ap_frame(struct wlan_objmgr_vdev * vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return 0;

	return dp_vdev->vdev_last_ap_frame;
}

systime_t dadp_vdev_get_last_traffic_indication(struct wlan_objmgr_vdev * vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return 0;

	return dp_vdev->vdev_last_traffic_indication;
}
void * dadp_pdev_get_extap_handle(struct wlan_objmgr_pdev * pdev)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return NULL;

	return dp_pdev->extap_hdl;
}

#ifdef ATH_COALESCING
void dadp_pdev_set_tx_coalescing(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_pdev *dp_pdev = NULL;
	if (!dp_pdev)
		return;

	dp_pdev->pdev_tx_coalescing = val;
}
#endif
void dadp_pdev_set_extap_handle(struct wlan_objmgr_pdev *pdev, void *ext_hdl)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	if (!dp_pdev)
		return;

	dp_pdev->extap_hdl = ext_hdl;
}
void dadp_pdev_set_minframesize(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_pdev *dp_pdev = NULL;
	if (!dp_pdev)
		return;

	dp_pdev->pdev_minframesize = val;
}

void dadp_pdev_set_mon_vdev(struct wlan_objmgr_pdev *pdev,
			    struct wlan_objmgr_vdev *vdev)
{
	struct dadp_pdev *dp_pdev = NULL;
	if (!dp_pdev)
		return;

	dp_pdev->pdev_mon_vdev = vdev;
}

void dadp_pdev_set_athextcap(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_pdev *dp_pdev = NULL;
	if (!dp_pdev)
		return;

	dp_pdev->pdev_athextcap |= val;
}

void dadp_pdev_clear_athextcap(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_pdev *dp_pdev = NULL;
	if (!dp_pdev)
		return;

	dp_pdev->pdev_athextcap &= ~val;
}

void dadp_pdev_set_addba_mode(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_pdev *dp_pdev = NULL;
	if (!dp_pdev)
		return;

	dp_pdev->pdev_addba_mode = val;
}

void dadp_pdev_set_amsdu_max_size(struct wlan_objmgr_pdev *pdev, u_int32_t val)
{
	struct dadp_pdev *dp_pdev = NULL;
	if (!dp_pdev)
		return;

	dp_pdev->pdev_amsdu_max_size = val;
}

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
void dadp_peer_set_key(struct wlan_objmgr_peer *peer, struct ieee80211_key *key)
{
	struct dadp_peer *dp_peer = NULL;

	if (peer == NULL) {
		qdf_print("%s: peer is NULL!", __func__);
		return;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	dp_peer->key = key;
}
#endif

u_int16_t dadp_peer_get_keyix(struct wlan_objmgr_peer * peer)
{
	struct dadp_peer *dp_peer = NULL;

	if (peer == NULL) {
		qdf_print("%s: peer is NULL!", __func__);
		return 0;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return 0;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	return dp_peer->keyix;
#else
	return dp_peer->key->wk_keyix;
#endif
}


#ifdef WLAN_CONV_CRYPTO_SUPPORTED
u_int16_t dadp_vdev_get_keyix(struct wlan_objmgr_vdev *vdev, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);

	if(!dp_vdev)
		return WLAN_CRYPTO_KEYIX_NONE;
	else
		return dp_vdev->vkey[id].keyix;
}

u_int16_t dadp_vdev_get_clearkeyix(struct wlan_objmgr_vdev *vdev, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);

	if(!dp_vdev)
		return WLAN_CRYPTO_KEYIX_NONE;
	else
		return dp_vdev->vkey[id].clearkeyix;
}

u_int16_t dadp_vdev_get_default_keyix(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);

	if(!dp_vdev)
		return WLAN_CRYPTO_KEYIX_NONE;
	else
		return dp_vdev->def_tx_keyix;
}

u_int16_t dadp_vdev_get_psta_keyix(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);

	if(!dp_vdev)
		return WLAN_CRYPTO_KEYIX_NONE;
	else
		return dp_vdev->psta_keyix;
}

uint8_t dadp_vdev_get_key_type(struct wlan_objmgr_vdev *vdev, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);

	if(!dp_vdev)
		return 0;
	else
		return dp_vdev->vkey[id].key_type;
}

uint8_t dadp_vdev_get_key_header(struct wlan_objmgr_vdev *vdev, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);

	if(!dp_vdev)
		return 0;
	else
		return dp_vdev->vkey[id].key_header;
}

uint8_t dadp_vdev_get_key_trailer(struct wlan_objmgr_vdev *vdev, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);

	if(!dp_vdev)
		return 0;
	else
		return dp_vdev->vkey[id].key_trailer;
}

uint8_t dadp_vdev_get_key_miclen(struct wlan_objmgr_vdev *vdev, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);

	if(!dp_vdev)
		return 0;
	else
		return dp_vdev->vkey[id].key_miclen;
}

u_int8_t dadp_vdev_get_key_valid(struct wlan_objmgr_vdev *vdev, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);

	if(!dp_vdev)
		return 0;
	else
		return dp_vdev->vkey[id].key_valid;
}

u_int16_t dadp_peer_get_default_keyix(struct wlan_objmgr_peer *peer)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);

	if(!dp_peer)
		return WLAN_CRYPTO_KEYIX_NONE;
	else
		return dp_peer->def_tx_keyix;
}

uint8_t dadp_peer_get_key_type(struct wlan_objmgr_peer *peer)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);

	if(!dp_peer)
		return 0;
	else
		return dp_peer->key_type;
}

uint8_t dadp_peer_get_key_header(struct wlan_objmgr_peer *peer)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);

	if(!dp_peer)
		return 0;
	else
		return dp_peer->key_header;
}

uint8_t dadp_peer_get_key_trailer(struct wlan_objmgr_peer *peer)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);

	if(!dp_peer)
		return 0;
	else
		return dp_peer->key_trailer;
}

uint8_t dadp_peer_get_key_miclen(struct wlan_objmgr_peer *peer)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);

	if(!dp_peer)
		return 0;
	else
		return dp_peer->key_miclen;
}

u_int8_t dadp_peer_get_key_valid(struct wlan_objmgr_peer *peer)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);

	if(!dp_peer)
		return 0;
	else
		return dp_peer->key_valid;
}

void dadp_peer_set_keyix(struct wlan_objmgr_peer *peer,
			 u_int16_t keyix)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	dp_peer->keyix = keyix;
}

void dadp_peer_set_clearkeyix(struct wlan_objmgr_peer *peer,
			      u_int16_t keyix)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	dp_peer->clearkeyix = keyix;
}

void dadp_peer_set_default_keyix(struct wlan_objmgr_peer *peer,
				 u_int16_t keyix)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	dp_peer->def_tx_keyix = keyix;
}

void dadp_peer_set_key_type(struct wlan_objmgr_peer *peer,
			    u_int8_t key_type)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	dp_peer->key_type = key_type;
}

void dadp_peer_set_key_valid(struct wlan_objmgr_peer *peer,
			     u_int8_t valid)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	dp_peer->key_valid = valid;
}

void dadp_peer_set_key_header(struct wlan_objmgr_peer *peer,
			      uint8_t header)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	dp_peer->key_header = header;
}

void dadp_peer_set_key_trailer(struct wlan_objmgr_peer *peer,
			       uint8_t trailer)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	dp_peer->key_trailer = trailer;
}

void dadp_peer_set_key_miclen(struct wlan_objmgr_peer *peer,
			      uint8_t miclen)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	dp_peer->key_miclen = miclen;
}

u_int16_t dadp_peer_get_clearkeyix(struct wlan_objmgr_peer * peer)
{
	struct dadp_peer *dp_peer = NULL;

	if (peer == NULL) {
		qdf_print("%s: peer is NULL!", __func__);
		return 0;
	}

	dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return 0;
	return dp_peer->clearkeyix;
}

void dadp_vdev_set_keyix(struct wlan_objmgr_vdev *vdev,
			 u_int16_t keyix, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vkey[id].keyix = keyix;
}

void dadp_vdev_set_clearkeyix(struct wlan_objmgr_vdev *vdev,
			      u_int16_t clearkeyix, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vkey[id].clearkeyix = clearkeyix;
}

void dadp_vdev_set_default_keyix(struct wlan_objmgr_vdev *vdev,
				 u_int16_t keyix)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->def_tx_keyix = keyix;
}

void dadp_vdev_set_psta_keyix(struct wlan_objmgr_vdev *vdev,
			      u_int16_t keyix)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->psta_keyix = keyix;
}

void dadp_vdev_set_key_type(struct wlan_objmgr_vdev *vdev,
			    u_int8_t key_type, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vkey[id].key_type = key_type;
}

void dadp_vdev_set_key_valid(struct wlan_objmgr_vdev *vdev,
			     u_int8_t valid, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vkey[id].key_valid = valid;
}

void dadp_vdev_set_key_header(struct wlan_objmgr_vdev *vdev,
			      uint8_t header, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vkey[id].key_header = header;
}

void dadp_vdev_set_key_trailer(struct wlan_objmgr_vdev *vdev,
			       uint8_t trailer, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vkey[id].key_trailer = trailer;
}

void dadp_vdev_set_key_miclen(struct wlan_objmgr_vdev *vdev,
			      uint8_t miclen, int id)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vkey[id].key_miclen = miclen;
}
#endif

void dadp_vdev_set_userrate(struct wlan_objmgr_vdev *vdev, int16_t val)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_userrate = val;
}

void dadp_vdev_set_userretries(struct wlan_objmgr_vdev *vdev, int8_t val)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_userretries = val;
}

void dadp_vdev_set_txchainmask(struct wlan_objmgr_vdev *vdev, int8_t val)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_txchainmask = val;
}

void dadp_vdev_set_txpower(struct wlan_objmgr_vdev *vdev, int8_t val)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_txpower = val;
}

void dadp_vdev_set_sko_th(struct wlan_objmgr_vdev *vdev, u_int8_t sko_th)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_sko_th = sko_th;
}

static void
wlan_timeout_fragments_iter(struct wlan_objmgr_pdev *pdev, void *object,
			    void *arg)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;
	u_int32_t lifetime = *(u_int32_t *) arg;
	systime_t now;
	wbuf_t wbuf = NULL;
	struct dadp_peer *dp_peer = NULL;

	now = OS_GET_TIMESTAMP();
	dp_peer = wlan_get_dp_peer(peer);

	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		return;
	}

	if (OS_ATOMIC_CMPXCHG(&(dp_peer->peer_rxfrag_lock), 0, 1) == 1) {
		/* somebody is using the rxfrag */
		return;
	}
	if ((dp_peer->peer_rxfrag[0] != NULL) &&
	    (CONVERT_SYSTEM_TIME_TO_MS(now - dp_peer->peer_rxfragstamp) >
	     lifetime)) {
		wbuf = dp_peer->peer_rxfrag[0];
		dp_peer->peer_rxfrag[0] = NULL;
	}

	if (wbuf) {
		wbuf_free(wbuf);
	}
	(void)OS_ATOMIC_CMPXCHG(&(dp_peer->peer_rxfrag_lock), 1, 0);
}

/*
 * Function to free all stale fragments
 */
void
dadp_pdev_timeout_fragments(struct wlan_objmgr_pdev *pdev, u_int32_t lifetime)
{
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
					  wlan_timeout_fragments_iter,
					  (void *)&lifetime, 1,
					  WLAN_MGMT_SB_ID);
}

void dadp_vdev_get_txrx_activity(struct wlan_objmgr_vdev *vdev,
				 ieee80211_vap_activity * activity)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return;
	}

	IEEE80211_STAT_LOCK(&dp_vdev->vdev_pause_info.vdev_pause_lock);
	activity->tx_data_frame_len =
	    dp_vdev->vdev_pause_info.vdev_tx_data_frame_len;
	activity->rx_data_frame_len =
	    dp_vdev->vdev_pause_info.vdev_rx_data_frame_len;
	activity->rx_data_frame_count =
	    dp_vdev->vdev_pause_info.vdev_rx_data_frame_count;
	activity->tx_data_frame_count =
	    dp_vdev->vdev_pause_info.vdev_tx_data_frame_count;
	dp_vdev->vdev_pause_info.vdev_tx_data_frame_len = 0;
	dp_vdev->vdev_pause_info.vdev_rx_data_frame_len = 0;
	dp_vdev->vdev_pause_info.vdev_rx_data_frame_count = 0;
	dp_vdev->vdev_pause_info.vdev_tx_data_frame_count = 0;
	IEEE80211_STAT_UNLOCK(&dp_vdev->vdev_pause_info.vdev_pause_lock);
}

#define WLAN_VAP_LOCK(_dp_vdev)                   spin_lock_dpc(&_dp_vdev->iv_lock);
#define WLAN_VAP_UNLOCK(_dp_vdev)                 spin_unlock_dpc(&_dp_vdev->iv_lock);
int ieee80211_vap_txrx_unregister_event_handler(struct wlan_objmgr_vdev *vdev,
						ieee80211_vap_txrx_event_handler
						evhandler, void *arg)
{
	int i, j;
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	ieee80211_txrx_event_info *info = NULL;

	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return -EINVAL;
	}

	info = &dp_vdev->iv_txrx_event_info;
	if (!info) {
		qdf_print("%s:info is NULL ", __func__);
		return -EINVAL;
	}

	WLAN_VAP_LOCK(dp_vdev);
	for (i = 0; i < IEEE80211_MAX_VAP_TXRX_EVENT_HANDLERS; ++i) {
		if (info->iv_txrx_event_handler[i] == evhandler
		    && info->iv_txrx_event_handler_arg[i] == arg) {
			info->iv_txrx_event_handler[i] = NULL;
			info->iv_txrx_event_handler_arg[i] = NULL;
			/* recompute event filters */
			info->iv_txrx_event_filter = 0;
			for (j = 0; j < IEEE80211_MAX_VAP_TXRX_EVENT_HANDLERS;
			     ++j) {
				if (info->iv_txrx_event_handler[j]) {
					info->iv_txrx_event_filter |=
					    info->iv_txrx_event_filters[j];
				}
			}
			WLAN_VAP_UNLOCK(dp_vdev);
			return EOK;
		}
	}
	WLAN_VAP_UNLOCK(dp_vdev);
	return -EEXIST;
}

int ieee80211_vap_txrx_register_event_handler(struct wlan_objmgr_vdev *vdev,
					      ieee80211_vap_txrx_event_handler
					      evhandler, void *arg,
					      u_int32_t event_filter)
{
	int i;
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	ieee80211_txrx_event_info *info = NULL;

	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return -EINVAL;
	}

	info = &dp_vdev->iv_txrx_event_info;
	if (!info) {
		qdf_print("%s:info is NULL ", __func__);
		return -EINVAL;
	}
	ieee80211_vap_txrx_unregister_event_handler(vdev, evhandler, arg);
	WLAN_VAP_LOCK(dp_vdev);
	for (i = 0; i < IEEE80211_MAX_VAP_TXRX_EVENT_HANDLERS; ++i) {
		if (info->iv_txrx_event_handler[i] == NULL) {
			info->iv_txrx_event_handler[i] = evhandler;
			info->iv_txrx_event_handler_arg[i] = arg;
			info->iv_txrx_event_filters[i] = event_filter;
			info->iv_txrx_event_filter |= event_filter;
			WLAN_VAP_UNLOCK(dp_vdev);
			return EOK;
		}
	}
	WLAN_VAP_UNLOCK(dp_vdev);
	return -ENOMEM;
}

void ieee80211_vap_txrx_deliver_event(struct wlan_objmgr_vdev *vdev,
				      ieee80211_vap_txrx_event * evt)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	ieee80211_txrx_event_info *info = NULL;
	int i;

	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return;
	}

	info = &dp_vdev->iv_txrx_event_info;
	if (!info) {
		qdf_print("%s:info is NULL ", __func__);
		return;
	}
	WLAN_VAP_LOCK(dp_vdev);
	for (i = 0; i < IEEE80211_MAX_VAP_TXRX_EVENT_HANDLERS; ++i) {
		if (info->iv_txrx_event_handler[i]
		    && (info->iv_txrx_event_filters[i] & evt->type)) {
			ieee80211_vap_txrx_event_handler evhandler =
			    info->iv_txrx_event_handler[i];
			void *arg = info->iv_txrx_event_handler_arg[i];

			WLAN_VAP_UNLOCK(dp_vdev);
			(*evhandler)
			    (vdev, evt, arg);
			WLAN_VAP_LOCK(dp_vdev);
		}
	}
	WLAN_VAP_UNLOCK(dp_vdev);
}

int
dadp_vdev_send_wbuf(struct wlan_objmgr_vdev *vdev,
		    struct wlan_objmgr_peer *peer, wbuf_t wbuf);

void dadp_peer_set_minbasicrate(struct wlan_objmgr_peer *peer,
				u_int8_t minbasicrate)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	if (!dp_peer)
		return;

	dp_peer->peer_minbasicrate = minbasicrate;
}

void dadp_vdev_deliver_tx_event(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
				struct ieee80211_frame *wh,
				struct ieee80211_tx_status *ts)
{
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct dadp_vdev *dp_vdev = NULL;

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return;
	}
	/*
	 * update power management module about wbuf completion
	 */
	if (dp_vdev->iv_txrx_event_info.
	    iv_txrx_event_filter & (IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL
				    | IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_TX |
				    IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_SMPS_ACT))
	{
		ieee80211_vap_txrx_event evt;
		evt.wh = wh;
		if ((dp_vdev->iv_txrx_event_info.
		     iv_txrx_event_filter &
		     IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL)
		    && wbuf_is_pwrsaveframe(wbuf)) {
			evt.type = IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_PS_NULL;
			evt.peer = peer;
			evt.u.status = ts->ts_flags;
			ieee80211_vap_txrx_deliver_event(vdev, &evt);
		} else if (dp_vdev->iv_txrx_event_info.
			   iv_txrx_event_filter &
			   IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_TX) {
			evt.type = IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_TX;
			/* Ipeertialize peer member */
			evt.peer = peer;
			evt.u.status = ts->ts_flags;
			ieee80211_vap_txrx_deliver_event(vdev, &evt);
		}
		if ((dp_vdev->iv_txrx_event_info.
		     iv_txrx_event_filter &
		     IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_SMPS_ACT)
		    && wbuf_is_smpsactframe(wbuf)) {
			evt.type = IEEE80211_VAP_OUTPUT_EVENT_COMPLETE_SMPS_ACT;
			evt.u.status = ts->ts_flags;
			ieee80211_vap_txrx_deliver_event(vdev, &evt);
		}
	}
}

void dadp_vdev_set_qos_map(struct wlan_objmgr_vdev *vdev, void *qos_map,
			   int len)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	qdf_mem_copy(&dp_vdev->iv_qos_map, qos_map, len);
}

QDF_STATUS dadp_init_ops(struct wlan_objmgr_pdev * pdev)
{
	struct dadp_ops *dops = NULL;
	/* alloc dadp ops */
	dops = qdf_mem_malloc(sizeof(struct dadp_ops));
	if (!dops) {
		qdf_print("dadp_ops (creation) failed ");
		wlan_set_dadp_ops(pdev, NULL);
		return QDF_STATUS_E_NOMEM;
	}

	dops->vdev_me_Convert = ieee80211_me_Convert;
	dops->vdev_me_hifi_forward = ieee80211_me_hifi_forward;
	dops->vdev_detach = dadp_vdev_detach;
	dops->peer_detach = dadp_peer_detach;
	dops->ique_attach = dadp_ique_attach;

	dops->vdev_set_privacy_filters = dadp_vdev_set_privacy_filters;
	dops->vdev_set_qos_map = dadp_vdev_set_qos_map;
	dops->vdev_set_osif_event_handlers = dadp_vdev_set_osif_event_handlers;
	dops->vdev_register_ccx_events = dadp_vdev_set_ccx_event_handlers;
	dops->vdev_set_me_hifi_enable = dadp_vdev_set_me_hifi_enable;
	dops->vdev_set_mc_snoop_enable = dadp_vdev_set_mc_snoop_enable;
	dops->vdev_set_mc_mcast_enable = dadp_vdev_set_mc_mcast_enable;
	dops->vdev_set_mc_discard_mcast = dadp_vdev_set_mc_discard_mcast;
	dops->vdev_set_ampdu_subframes = dadp_vdev_set_ampdu_subframes;
	dops->vdev_set_amsdu = dadp_vdev_set_amsdu;
	dops->vdev_set_dscp_map_id = dadp_vdev_set_dscp_map_id;
	dops->vdev_set_smps = dadp_vdev_set_smps;
	dops->vdev_set_smartnet_enable = dadp_vdev_set_smartnet_enable;
	dops->vdev_set_tspecActive = dadp_vdev_set_tspecActive;
	dops->vdev_set_txrxbytes = dadp_vdev_set_txrxbytes;
	dops->vdev_get_txrxbytes = dadp_vdev_get_txrxbytes;
	dops->vdev_get_last_ap_frame = dadp_vdev_get_last_ap_frame;
	dops->vdev_get_last_traffic_indication =
	    dadp_vdev_get_last_traffic_indication;
	dops->vdev_set_priority_dscp_tid_map = dadp_vdev_set_priority_dscp_tid_map;
	dops->vdev_set_dscp_tid_map = dadp_vdev_set_dscp_tid_map;
	dops->vdev_set_frag_threshold = dadp_vdev_set_frag_threshold;
	dops->vdev_set_rtsthreshold = dadp_vdev_set_rtsthreshold;
	dops->vdev_set_userrate = dadp_vdev_set_userrate;
	dops->vdev_set_userretries = dadp_vdev_set_userretries;
	dops->vdev_set_txpower = dadp_vdev_set_txpower;
	dops->vdev_set_txchainmask = dadp_vdev_set_txchainmask;
	dops->vdev_set_sko_th = dadp_vdev_set_sko_th;
	dops->vdev_get_txrx_activity = dadp_vdev_get_txrx_activity;
	dops->vdev_deliver_tx_event = dadp_vdev_deliver_tx_event;
	dops->vdev_send_wbuf = dadp_vdev_send_wbuf;
	dops->vdev_send = wlan_vap_send;
	dops->vdev_txrx_register_event_handler =
	    ieee80211_vap_txrx_register_event_handler;
	dops->vdev_txrx_unregister_event_handler =
	    ieee80211_vap_txrx_unregister_event_handler;
	dops->vdev_txrx_deliver_event = ieee80211_vap_txrx_deliver_event;
	dops->vdev_set_mcast_rate = dadp_vdev_set_mcast_rate;
	dops->vdev_set_prot_mode = dadp_vdev_set_prot_mode;

	dops->peer_update_uapsd_ac_trigena = dadp_peer_update_uapsd_ac_trigena;
	dops->peer_update_uapsd_ac_delivena =
	    dadp_peer_update_uapsd_ac_delivena;
	dops->peer_update_uapsd_dyn_trigena =
	    dadp_peer_update_uapsd_dyn_trigena;
	dops->peer_update_uapsd_dyn_delivena =
	    dadp_peer_update_uapsd_dyn_delivena;
	dops->peer_set_nawds = dadp_peer_set_nawds;
	dops->peer_set_minbasicrate = dadp_peer_set_minbasicrate;
	dops->peer_set_uapsd_maxsp = dadp_peer_set_uapsd_maxsp;
	dops->peer_get_keyix = dadp_peer_get_keyix;

#ifdef ATH_COALESCING
	dops->pdev_set_tx_coalescing = dadp_pdev_set_tx_coalescing;
#endif
	dops->pdev_set_minframesize = dadp_pdev_set_minframesize;
	dops->pdev_set_mon_vdev = dadp_pdev_set_mon_vdev;
	dops->pdev_set_addba_mode = dadp_pdev_set_addba_mode;
	dops->pdev_set_amsdu_max_size = dadp_pdev_set_amsdu_max_size;
	dops->pdev_set_athextcap = dadp_pdev_set_athextcap;
	dops->pdev_clear_athextcap = dadp_pdev_clear_athextcap;
	dops->pdev_timeout_fragments = dadp_pdev_timeout_fragments;
	dops->pdev_get_extap_handle = dadp_pdev_get_extap_handle;

	wlan_set_dadp_ops(pdev, dops);

	return QDF_STATUS_SUCCESS;

}

void dadp_deinit_ops(struct wlan_objmgr_pdev *pdev)
{
	struct dadp_ops *dops = wlan_pdev_get_dadp_ops(pdev);

	wlan_set_dadp_ops(pdev, NULL /* ic->dops = NULL */ );

	if (dops)
		qdf_mem_free(dops);
}

#define NUM_DSCP_MAP 16
#define IP_DSCP_MAP_LEN 64
A_UINT32 dscp_tid_map[IP_DSCP_MAP_LEN] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	1, 1, 1, 1, 1, 1, 1, 1,
	1, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	5, 5, 5, 5, 5, 5, 5, 5,
	5, 5, 5, 5, 5, 5, 5, 5,
	6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6,
};

void dadp_init_pdev_params(struct dadp_pdev *dp_pdev)
{
	u_int8_t bcast[IEEE80211_ADDR_LEN] =
	    { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	int map_id;

	dp_pdev->pdev_tid_override_queue_mapping = 0;
	dp_pdev->pdev_override_dscp = 0;
	dp_pdev->pdev_override_igmp_dscp = 0;
	dp_pdev->pdev_override_hmmc_dscp = 0;
	dp_pdev->pdev_dscp_igmp_tid = 0;
	dp_pdev->pdev_dscp_hmmc_tid = 0;
	for (map_id = 0; map_id < NUM_DSCP_MAP; map_id++) {
		OS_MEMCPY(dp_pdev->pdev_dscp_tid_map[map_id], dscp_tid_map,
			  sizeof(u_int32_t) * IP_DSCP_MAP_LEN);
	}

	dp_pdev->wme_hipri_traffic = 0;
	dp_pdev->pdev_addba_mode = ADDBA_MODE_AUTO;
	/* set up broadcast address */
	IEEE80211_ADDR_COPY(dp_pdev->pdev_broadcast, bcast);
	dp_pdev->pdev_blkreportflood = 0;
	dp_pdev->pdev_dropstaquery = 0;
	dp_pdev->pdev_mon_vdev = NULL;
	dp_pdev->pdev_min_rssi_enable = false;
	dp_pdev->pdev_min_rssi = 0;
	dp_pdev->pdev_minframesize = sizeof(struct ieee80211_frame_min);
}

void dadp_pdev_attach(struct wlan_objmgr_pdev *pdev,
		      struct ath_softc_net80211 *scn, osdev_t osdev)
{
	struct dadp_pdev *dp_pdev = NULL;
	QDF_STATUS status;
	dp_extap_t *extap_hdl;

	if (NULL == pdev) {
		qdf_print("%s: pdev is NULL!", __func__);
		return;
	}

	status = dadp_init_ops(pdev);
	if (status != QDF_STATUS_SUCCESS)
		goto error;

	dp_pdev = qdf_mem_malloc(sizeof(*dp_pdev));
	if (!dp_pdev) {
		qdf_print("dadp_pdev (creation) failed ");
		goto error;
	}
	qdf_mem_zero(dp_pdev, sizeof(*dp_pdev));

	dadp_init_pdev_params(dp_pdev);

	pdev->dp_handle = (void *)dp_pdev;

	wlan_objmgr_pdev_get_ref(pdev, WLAN_MLME_SB_ID);
	dp_pdev->pdev = pdev;
	dp_pdev->scn = scn;
	dp_pdev->osdev = osdev;
	extap_hdl = qdf_mem_malloc(sizeof(dp_extap_t));
	dadp_pdev_set_extap_handle(pdev, extap_hdl);
	return;

error:
	dadp_deinit_ops(pdev);
	pdev->dp_handle = NULL;
	return;
}

void dadp_init_vdev_params(struct dadp_vdev *dp_vdev)
{
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	int i = 0;
#endif
	spin_lock_init(&dp_vdev->vdev_pause_info.vdev_pause_lock);
	spin_lock_init(&dp_vdev->iv_lock);
	dp_vdev->vdev_pause_info.vdev_tx_data_frame_len = 0;
	dp_vdev->vdev_pause_info.vdev_rx_data_frame_len = 0;
	dp_vdev->vdev_pause_info.vdev_rx_data_frame_count = 0;
	dp_vdev->vdev_pause_info.vdev_tx_data_frame_count = 0;
	dp_vdev->me_hifi_enable = 0;
	dp_vdev->mc_snoop_enable = 0;
	dp_vdev->mc_mcast_enable = 0;
	dp_vdev->mc_discard_mcast = 1;

	dp_vdev->vdev_smps = 0;
	dp_vdev->vdev_smartnet_enable = 0;
	dp_vdev->vdev_tspecActive = 0;
#if ATH_SUPPORT_DSCP_OVERRIDE
	dp_vdev->vdev_dscp_map_id = 0x00;
#endif
	dp_vdev->vdev_def_txkey = IEEE80211_DEFAULT_KEYIX;
	dp_vdev->vdev_fragthreshold = IEEE80211_FRAGMT_THRESHOLD_MAX;
	dp_vdev->vdev_rtsthreshold = IEEE80211_RTS_MAX;
	dp_vdev->av_ampdu_subframes = IEEE80211_AMPDU_SUBFRAME_DEFAULT;
	dp_vdev->av_amsdu = 1;
#ifdef ATH_SUPPORT_QUICK_KICKOUT
	dp_vdev->vdev_sko_th = ATH_TX_MAX_CONSECUTIVE_XRETRIES;
#endif
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	for(i = 0; i < WLAN_CRYPTO_MAXKEYIDX; i++)
	{
		dp_vdev->vkey[i].keyix = WLAN_CRYPTO_KEYIX_NONE;
		dp_vdev->vkey[i].clearkeyix = WLAN_CRYPTO_KEYIX_NONE;
		dp_vdev->vkey[i].key_type = WLAN_CRYPTO_CIPHER_NONE;
		dp_vdev->vkey[i].key_valid = 0;
		dp_vdev->vkey[i].key_header = 0;
		dp_vdev->vkey[i].key_trailer = 0;
		dp_vdev->vkey[i].key_miclen = 0;
	}
	dp_vdev->def_tx_keyix = 0;
#endif

}

void dadp_vdev_attach(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = NULL;

	if (NULL == vdev) {
		qdf_print("%s: vdev is NULL!", __func__);
		return;
	}

	dp_vdev = qdf_mem_malloc(sizeof(*dp_vdev));
	if (!dp_vdev) {
		qdf_print("dadp_vdev (creation) failed ");
		goto error;
	}
	qdf_mem_zero(dp_vdev, sizeof(*dp_vdev));

	dadp_init_vdev_params(dp_vdev);
	vdev->dp_handle = (void *)dp_vdev;

	wlan_objmgr_vdev_get_ref(vdev, WLAN_MLME_SB_ID);
	dp_vdev->vdev = vdev;

	return;

error:
	vdev->dp_handle = NULL;
	return;
}

void dadp_init_peer_params(struct dadp_peer *dp_peer)
{
	atomic_set(&(dp_peer->peer_rxfrag_lock), 0);
	dp_peer->nawds = 0;
	dp_peer->peer_consecutive_xretries = 0;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	dp_peer->keyix = WLAN_CRYPTO_KEYIX_NONE;
	dp_peer->clearkeyix = WLAN_CRYPTO_KEYIX_NONE;
	dp_peer->key_type = WLAN_CRYPTO_CIPHER_NONE;
	dp_peer->key_valid = 0;
#endif
}

void dadp_peer_attach(struct wlan_objmgr_peer *peer, void *an)
{
	struct dadp_peer *dp_peer = NULL;

	if (NULL == peer) {
		qdf_print("%s: peer is NULL!", __func__);
		return;
	}

	dp_peer = qdf_mem_malloc(sizeof(*dp_peer));
	if (!dp_peer) {
		qdf_print("dadp_peer (creation) failed ");
		goto error;
	}
	qdf_mem_zero(dp_peer, sizeof(*dp_peer));

	dadp_init_peer_params(dp_peer);

	dp_peer->an = an;
	peer->dp_handle = (void *)dp_peer;

	wlan_objmgr_peer_get_ref(peer, WLAN_MLME_SB_ID);
	dp_peer->peer = peer;
	return;

error:
	peer->dp_handle = NULL;
	return;
}

void dadp_pdev_detach(struct wlan_objmgr_pdev *pdev)
{
	struct dadp_pdev *dp_pdev = wlan_get_dp_pdev(pdev);
	dp_extap_t *ext_hdl;

	if (!dp_pdev) {
		qdf_print("%s:dp_pdev is NULL ", __func__);
		return;
	}
	dadp_deinit_ops(pdev);
	ext_hdl = (dp_extap_t *)wlan_pdev_get_extap_handle(pdev);
	if (ext_hdl)
		qdf_mem_free(ext_hdl);
	pdev->dp_handle = NULL;
	dp_pdev->pdev = NULL;
	wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);

	if (dp_pdev)
		qdf_mem_free(dp_pdev);
}

void dadp_vdev_detach(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);

	if (!dp_vdev) {
		qdf_print("%s:dp_vdev is NULL ", __func__);
		return;
	}
	spin_lock_destroy(&dp_vdev->vdev_pause_info.vdev_pause_lock);
	spin_lock_destroy(&dp_vdev->iv_lock);

	vdev->dp_handle = NULL;
	dp_vdev->vdev = NULL;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SB_ID);

	if (dp_vdev)
		qdf_mem_free(dp_vdev);
}

void dadp_peer_detach(struct wlan_objmgr_peer *peer)
{
	struct dadp_peer *dp_peer = wlan_get_dp_peer(peer);
	struct wlan_objmgr_vdev *vdev = wlan_peer_get_vdev(peer);
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	int tick_counter = 0;

	if (!dp_peer) {
		qdf_print("%s:dp_peer is NULL ", __func__);
		return;
	}
	while (OS_ATOMIC_CMPXCHG(&(dp_peer->peer_rxfrag_lock), 0, 1) == 1) {
		/* busy wait; can be executed at IRQL <= DISPATCH_LEVEL */
		if (tick_counter++ > 100) {	// no more than 1ms
			break;
		}
		OS_DELAY(10);
	}

	if (dp_peer->peer_rxfrag[0] != NULL) {
		wbuf_free(dp_peer->peer_rxfrag[0]);
		dp_peer->peer_rxfrag[0] = NULL;
	}
#ifdef ATH_AMSDU
	ath_amsdu_node_detach(pdev, peer);
#endif

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
	wlan_crypto_delkey(vdev, wlan_peer_get_macaddr(peer), 0);
#endif

	(void)OS_ATOMIC_CMPXCHG(&(dp_peer->peer_rxfrag_lock), 1, 0);
	peer->dp_handle = NULL;
	dp_peer->peer = NULL;
	wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);

	if (dp_peer)
		qdf_mem_free(dp_peer);
}

void dadp_vdev_deliver_event_txq_empty(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = NULL;

	dp_vdev = wlan_get_dp_vdev(vdev);
	if (dp_vdev != NULL &&
	    (dp_vdev->iv_txrx_event_info.
	     iv_txrx_event_filter & IEEE80211_VAP_OUTPUT_EVENT_TXQ_EMPTY)) {
		ieee80211_vap_txrx_event evt;
		evt.type = IEEE80211_VAP_OUTPUT_EVENT_TXQ_EMPTY;
		ieee80211_vap_txrx_deliver_event(vdev, &evt);
	}
}

void dadp_vdev_set_psta(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;
#if ATH_SUPPORT_WRAP
	dp_vdev->av_is_psta = 1;
#endif
}

int dadp_vdev_is_psta(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return 0;

#if ATH_SUPPORT_WRAP
	return (dp_vdev->av_is_psta);
#else
	return 0;
#endif
}

void dadp_vdev_set_mpsta(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;
#if ATH_SUPPORT_WRAP
	dp_vdev->av_is_mpsta = 1;
#endif
}

int dadp_vdev_is_mpsta(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return 0;
#if ATH_SUPPORT_WRAP
	return (dp_vdev->av_is_mpsta);
#else
	return 0;
#endif
}

void dadp_vdev_set_wrap(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;
#if ATH_SUPPORT_WRAP
	dp_vdev->av_is_wrap = 1;
#endif
}

int dadp_vdev_is_wrap(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return 0;
#if ATH_SUPPORT_WRAP
	return (dp_vdev->av_is_wrap);
#else
	return 0;
#endif
}

void dadp_vdev_set_use_mat(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;
#if ATH_SUPPORT_WRAP
	dp_vdev->av_use_mat = 1;
#endif
}

int dadp_vdev_use_mat(struct wlan_objmgr_vdev *vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return 0;
#if ATH_SUPPORT_WRAP
	return (dp_vdev->av_use_mat);
#else
	return 0;
#endif
}

void dadp_vdev_set_mataddr(struct wlan_objmgr_vdev *vdev,
			   const u_int8_t * mataddr)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;
#if ATH_SUPPORT_WRAP
	OS_MEMCPY(dp_vdev->av_mat_addr, mataddr, IEEE80211_ADDR_LEN);
#endif
}

u_int8_t *dadp_vdev_get_mataddr(struct wlan_objmgr_vdev * vdev)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return NULL;
#if ATH_SUPPORT_WRAP
	return (dp_vdev->av_mat_addr);
#else
	return NULL;
#endif
}

#if ATH_SUPPORT_FLOWMAC_MODULE
void dadp_vdev_set_flowmac(struct wlan_objmgr_vdev *vdev, int en)
{
	struct dadp_vdev *dp_vdev = wlan_get_dp_vdev(vdev);
	if (!dp_vdev)
		return;

	dp_vdev->vdev_flowmac = en;
}
#endif
