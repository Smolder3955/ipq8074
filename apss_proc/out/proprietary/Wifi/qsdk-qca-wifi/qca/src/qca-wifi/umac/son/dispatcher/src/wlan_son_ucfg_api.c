/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*/

/*
*This File provides framework user space ioctl handling for SON.
*/


#include "../../core/src/wlan_son_internal.h"
#include "wlan_son_ucfg_api.h"
#include <wlan_son_utils_api.h>
#include <wlan_son_pub.h>

#if QCA_SUPPORT_SON
/**
 * @brief Enable/Disable SON events on a VAP
 *
 * @pre  wlan_son_enable must be called
 *
 * @param [inout] vdev  the VAP whose band steering status
 *                     changes
 * @param [in] req request from user space containing the flag
 *                 indicating enable or disable
 *
 * @return QDF_STATUS_E_INVAL if SON not initialized or enabled on
 *         the radio, QDF_STATUS_E_ALREADY if SON on the VAP is
 *         already in the requested state, otherwise return QDF_STATUS_SUCCESS
 */


/**
 * @brief Determine whether an ioctl request is valid or not, along with the
 *        associated parameters.
 *
 * @param [in] vap  the VAP on which the ioctl was made
 * @param [in] req  the parameters provided in the ioctl
 *
 * @return true if all parameters are valid; otherwise false
 */

bool wlan_son_is_req_valid(struct wlan_objmgr_vdev *vdev,
			   struct ieee80211req_athdbg *req)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	/* Check in future if we can get psoc from vdev directly */

	if (wlan_son_is_vdev_valid(vdev)) {
		pdev = wlan_vdev_get_pdev(vdev);
		if (pdev) {
			return wlan_son_is_pdev_valid(pdev)
				&& NULL !=req;
		}
	}

	return false;
}

int son_enable_disable_vdev_events(struct wlan_objmgr_vdev *vdev,
				   void  *req)
{
	bool enabled;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct ieee80211req_athdbg *req_t = NULL;

	req_t = (struct ieee80211req_athdbg *)req;

	if (!wlan_son_is_req_valid (vdev , req_t)) {
		return -EINVAL;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	/* Make sure band steering is enabled at the radio level first */
	/* TO DO: Ensure whether the below check is required */
	if (pdev && !(wlan_son_is_pdev_enabled(pdev))) {
		return -EINVAL;
	}

	/* Make sure this isn't a set to the same state we are already in */
	enabled = wlan_son_is_vdev_enabled(vdev);

	if ((req_t->data.bsteering_enable && enabled) ||
	    (!req_t->data.bsteering_enable && !enabled)) {
		return -EALREADY;
	}

	return son_core_enable_disable_vdev_events(vdev,
						   req_t->data.bsteering_enable);
}

int8_t son_ucfg_set_get_peer_class_group(struct wlan_objmgr_vdev *vdev, void *data, bool set)
{
	struct ieee80211req_athdbg *req_t = NULL;
	int8_t retv = -EINVAL;

	req_t = (struct ieee80211req_athdbg *)data;

	if (!wlan_son_is_req_valid(vdev, req_t)) {
		return retv;
	}

	retv = son_core_set_get_peer_class_group(vdev,
					 req_t->dstmac,
					 &(req_t->data.bsteering_peer_class_group),
					 set);
	return retv;
}

int8_t son_ucfg_set_get_overload(struct wlan_objmgr_vdev *vdev, void *data, bool set)
{
	struct ieee80211req_athdbg *req_t = NULL;
	int8_t retv = -EINVAL;

	req_t = (struct ieee80211req_athdbg *)data;

	if (!wlan_son_is_req_valid(vdev, req_t))
		return retv;

	retv = son_core_set_get_overload(vdev,
					 &(req_t->data.bsteering_overload),
					 set);
	return retv;
}

int son_ucfg_send_event(struct  wlan_objmgr_vdev *vdev,
			SON_DISPATCHER_CMD cmd,
			void *data)
{
	return EOK;


}

int son_pdev_steering_enable_disable(struct wlan_objmgr_vdev *vdev, void *req)
{
	int retv = -EINVAL;
	struct ieee80211req_athdbg *req_t = NULL;

	req_t = (struct ieee80211req_athdbg *)req;

	if (!wlan_son_is_req_valid(vdev, req_t))
		return retv;

	retv = son_core_pdev_enable_disable_steering(vdev,
						     req_t->data.bsteering_enable);
	return retv;
}

int son_pdev_steering_enable_ackrssi(struct wlan_objmgr_vdev *vdev, void *req)
{
        int retv = -EINVAL;
        struct ieee80211req_athdbg *req_t = NULL;

        req_t = (struct ieee80211req_athdbg *)req;

        if (!wlan_son_is_req_valid(vdev, req_t))
                return retv;

        retv = son_core_pdev_enable_ackrssi(vdev,
                                             req_t->data.ackrssi_enable);
        return retv;

}

int son_trigger_null_frame_tx(struct wlan_objmgr_vdev *vdev,
			      void *req)
{
	int retv = -EINVAL;
	struct ieee80211req_athdbg *req_t = NULL;

	req_t = (struct ieee80211req_athdbg *)req;

	if (!wlan_son_is_req_valid(vdev, req_t))
		return retv;

	retv = son_core_null_frame_tx(vdev, req_t->dstmac,
				      req_t->data.bsteering_rssi_num_samples);

	return retv;
}


int son_pdev_set_dbg_param(struct wlan_objmgr_vdev *vdev,
			   void *req)
{
	int retv = -EINVAL;
	struct ieee80211req_athdbg *req_t = NULL;

	req_t = (struct ieee80211req_athdbg *)req;

	if (!wlan_son_is_req_valid(vdev, req_t))
		return retv;

	retv = son_core_set_dbg_params(vdev, &req_t->data.bsteering_dbg_param);

	return retv;
}

int son_pdev_get_dbg_param(struct wlan_objmgr_vdev *vdev,
			   void *req)
{
	int retv = -EINVAL;
	struct ieee80211req_athdbg *req_t = NULL;

	req_t = (struct ieee80211req_athdbg *)req;

	if (!wlan_son_is_req_valid(vdev, req_t))
		return retv;

	retv = son_core_get_dbg_params(vdev,
				       &req_t->data.bsteering_dbg_param);

	return retv;
}

int son_set_get_steering_params(struct wlan_objmgr_vdev *vdev,
				void *req, bool action)
{
	int retv = -EINVAL;
	struct ieee80211req_athdbg *req_t = NULL;

	req_t = (struct ieee80211req_athdbg *)req;

	if (!wlan_son_is_req_valid(vdev, req_t))
		return retv;

	retv = son_core_set_get_steering_params(vdev, &req_t->data.bsteering_param,
						action);

	return retv ;

}

int son_auth_allow(struct wlan_objmgr_vdev *vdev,
		   void *req,
		   bool set /*true means set */)
{
	struct ieee80211req_athdbg *req_t = NULL;

	req_t = (struct ieee80211req_athdbg *)req;

	if (!wlan_son_is_req_valid(vdev, req_t))
		return -EINVAL;

	return wlan_vdev_acl_auth(vdev, set, req_t);

}
int son_peer_set_probe_resp_allow_2G(struct wlan_objmgr_vdev *vdev,
				     void *req)
{
	int retv = -EINVAL;
	struct ieee80211req_athdbg *req_t = NULL;

	req_t = (struct ieee80211req_athdbg *)req;

	if (!wlan_son_is_req_valid(vdev, req_t))
		return retv;

	retv = son_core_peer_set_probe_resp_allow_2g(vdev,
						     req_t->dstmac,
						     req_t->data.bsteering_probe_resp_allow_24g);

	return retv;
}

int son_probe_response_wh(struct wlan_objmgr_vdev *vdev,
			  void *req,
			  bool set /*true means set */)
{
	int retv = -EINVAL;
	struct ieee80211req_athdbg *req_t = NULL;

	req_t = (struct ieee80211req_athdbg *)req;

	if (!wlan_son_is_req_valid(vdev, req_t))
		return retv;

	return wlan_vdev_acl_probe(vdev, set, req_t);
}

int son_peer_set_stastats_intvl(struct wlan_objmgr_vdev *vdev,
				void *req)
{
	int retv = -EINVAL;
	struct ieee80211req_athdbg *req_t = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	uint8_t pdev_id;

	req_t = (struct ieee80211req_athdbg *)req;

	if (!wlan_son_is_req_valid(vdev, req_t))
		return retv;

	psoc = wlan_vdev_get_psoc(vdev);

	pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	peer = wlan_objmgr_get_peer(psoc, pdev_id, req_t->dstmac, WLAN_SON_ID);
	if (!peer) {
		return retv;
	}

	retv = son_core_set_stastats_intvl(peer,
			   req_t->data.bsteering_sta_stats_update_interval_da);

	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);

	return retv;
}



int son_peer_set_steering(struct wlan_objmgr_vdev *vdev,
			  void *req)
{
	int retv = -EINVAL;
	struct ieee80211req_athdbg *req_t = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	uint8_t pdev_id;

	req_t = (struct ieee80211req_athdbg *)req;

	if (!wlan_son_is_req_valid(vdev, req_t))
		return retv;

	psoc = wlan_vdev_get_psoc(vdev);

	pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	peer = wlan_objmgr_get_peer(psoc, pdev_id, req_t->dstmac, WLAN_SON_ID);

	if (!peer) {
		if(req_t->data.bsteering_steering_in_progress) {
			qdf_print("%s: Requested STA %02x:%02x:%02x:%02x:%02x:%02x is not "
				"associated", __func__, req_t->dstmac[0], req_t->dstmac[1],
				req_t->dstmac[2], req_t->dstmac[3], req_t->dstmac[4], req_t->dstmac[5]);
			return -EINVAL;
		} else {
			/* special case station is already left
			   still consider it valid case
			   for reseting flag */
			return EOK;
		}
	}

	retv = son_core_set_steer_in_prog(peer,
					  req_t->data.bsteering_steering_in_progress);

	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);

	return retv;

}

int son_peer_local_disassoc(struct wlan_objmgr_vdev *vdev,
			    void *req)
{
	int retv = -EINVAL;
	struct ieee80211req_athdbg *req_t = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	uint8_t pdev_id;

	req_t = (struct ieee80211req_athdbg *)req;

	if (!wlan_son_is_req_valid(vdev, req_t))
		return retv;

	psoc = wlan_vdev_get_psoc(vdev);

	pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	peer = wlan_objmgr_get_peer(psoc, pdev_id, req_t->dstmac, WLAN_SON_ID);

	if (!peer)
		return retv;

	retv = wlan_vdev_local_disassoc(vdev, peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_SON_ID);

	return retv;
}

u_int8_t ucfg_son_get_scaling_factor(struct wlan_objmgr_vdev *vdev)
{

	if (vdev)
		return(son_core_get_scaling_factor(vdev));
	else
		return 0;
}

u_int8_t ucfg_son_get_skip_hyst(struct wlan_objmgr_vdev *vdev)
{

	if (vdev)
		return(son_core_get_skip_hyst(vdev));
	else
		return 0;
}

int8_t ucfg_son_get_cap_rssi(struct wlan_objmgr_vdev *vdev)
{
	if (vdev &&
	    wlan_son_is_vdev_valid(vdev) &&
	    wlan_son_is_vdev_enabled(vdev))
		return son_core_get_cap_rssi(vdev);
	else
		return -EINVAL;
}

int ucfg_son_get_cap_snr(struct wlan_objmgr_vdev *vdev, int *cap_snr)
{
	if (vdev)
		son_core_get_cap_snr(vdev, cap_snr);

	return 0;

}

int8_t ucfg_son_set_cap_rssi(struct wlan_objmgr_vdev *vdev,
			     u_int32_t rssi)
{
	if (vdev)
		son_core_set_cap_rssi(vdev, (int8_t)rssi);
	else
		return -EINVAL;

	return EOK;
}


int8_t ucfg_son_set_uplink_rate(struct wlan_objmgr_vdev *vdev,
				u_int32_t uplink_rate)
{
	if (vdev)
		son_core_set_uplink_rate(vdev, uplink_rate);
	else
		return -EINVAL;

	return EOK;
}

int16_t ucfg_son_get_uplink_rate(struct wlan_objmgr_vdev *vdev)
{
	if (vdev)
		return son_core_get_uplink_rate(vdev);
	else
		return -EINVAL;
}

int8_t ucfg_son_get_uplink_snr(struct wlan_objmgr_vdev *vdev)
{
	if (vdev)
		return son_core_get_uplink_snr(vdev);
	else
		return -EINVAL;
}

int8_t ucfg_son_set_scaling_factor(struct wlan_objmgr_vdev *vdev ,
				   int8_t scaling_factor)
{
	if (vdev &&
	    (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) &&
	    (scaling_factor && scaling_factor <= 100)) {
			son_core_set_scaling_factor(vdev, scaling_factor);
			return EOK;
	}

	return -EINVAL;
}

int8_t ucfg_son_set_skip_hyst(struct wlan_objmgr_vdev *vdev ,
				   int8_t skip_hyst)
{
	if (vdev &&
	    ((skip_hyst == 0) || (skip_hyst == 1))) {
			son_core_set_skip_hyst(vdev, skip_hyst);
			return EOK;
	}

	return -EINVAL;
}


int son_get_peer_info(struct wlan_objmgr_vdev *vdev, void *data)
{

	struct ieee80211req_athdbg *req = NULL;

	req = (struct ieee80211req_athdbg *)data;

	if (!wlan_son_is_req_valid(vdev, req))
		return -EINVAL;

	return wlan_vdev_get_node_info(vdev, req);
}

int son_set_innetwork_2g_mac(struct wlan_objmgr_vdev *vdev, void *data)
{

	struct ieee80211req_athdbg *req = NULL;

	req = (struct ieee80211req_athdbg *)data;

	if (!wlan_son_is_req_valid(vdev, req))
		return -EINVAL;

	return son_core_set_innetwork_2g_mac(vdev, req->dstmac, req->data.param[0]);
}

int son_get_innetwork_2g_mac(struct wlan_objmgr_vdev *vdev, void *data)
{

	struct ieee80211req_athdbg *req = NULL;

	req = (struct ieee80211req_athdbg *)data;

	if (!wlan_son_is_req_valid(vdev, req))
		return -EINVAL;

	return son_core_get_innetwork_2g_mac(vdev, (void *)req->data.innetwork_2g_req.data_addr,
					     req->data.innetwork_2g_req.index,
					     req->data.innetwork_2g_req.ch);
}

int son_set_map_rssi_policy(struct wlan_objmgr_vdev *vdev, void *data)
{
	struct ieee80211req_athdbg *req = NULL;
	req = (struct ieee80211req_athdbg *)data;

	if (!wlan_son_is_req_valid(vdev, req))
		return -EINVAL;

	return son_core_set_map_rssi_policy(vdev, req->data.map_rssi_policy.rssi,
					    req->data.map_rssi_policy.rssi_hysteresis);
}

int son_get_assoc_frame(struct wlan_objmgr_vdev *vdev, void *data)
{
	struct ieee80211req_athdbg *req = NULL;
	req = (struct ieee80211req_athdbg *)data;

	if (!wlan_son_is_req_valid(vdev, req))
		return -EINVAL;

	return son_core_get_assoc_frame(vdev, req->dstmac,
					req->data.mapclientcap.assocReqFrame,
					&req->data.mapclientcap.frameSize);
}

int son_get_map_esp_info(struct wlan_objmgr_vdev *vdev, void *data)
{
	struct ieee80211req_athdbg *req = NULL;
	req = (struct ieee80211req_athdbg *)data;

	if (!wlan_son_is_req_valid(vdev, req))
		return -EINVAL;

	return son_core_get_map_esp_info(vdev, &req->data.map_esp_info);
}

int son_set_map_timer_policy(struct wlan_objmgr_vdev *vdev, void *data)
{
	struct ieee80211req_athdbg *req = NULL;
	req = (struct ieee80211req_athdbg *)data;

	if (!wlan_son_is_req_valid(vdev, req))
		return -EINVAL;

	return wlan_vdev_add_acl_validity_timer(vdev, req->data.client_assoc_req_acl.stamac,
						req->data.client_assoc_req_acl.validity_period);
}

int son_get_map_operable_channels(struct wlan_objmgr_vdev *vdev, void *data)
{
	struct ieee80211req_athdbg *req = NULL;
	req = (struct ieee80211req_athdbg *)data;

	if (!wlan_son_is_req_valid(vdev, req))
		return -EINVAL;

	return son_core_get_map_operable_channels(vdev, &req->data.map_op_chan);
}

int son_get_map_apcap(struct wlan_objmgr_vdev *vdev, void *data)
{
	struct ieee80211req_athdbg *req = NULL;
	req = (struct ieee80211req_athdbg *)data;

	if (!wlan_son_is_req_valid(vdev, req))
		return -EINVAL;

	return son_core_get_map_apcap(vdev, &req->data.mapapcap);
}

int son_ucfg_get_cmd(struct wlan_objmgr_vdev *vdev,
		     SON_DISPATCHER_CMD cmd,
		     void *data)
{
	int retv = true;

	switch(cmd) {
		/* Set/GET the static band steering parameters */
	case SON_DISPATCHER_CMD_BSTEERING_PARAMS:
		retv = son_set_get_steering_params(vdev, data, false);
		break;
		/* Set the static band steering parameters */
	case SON_DISPATCHER_CMD_BSTEERING_DBG_PARAMS:
		retv = son_pdev_get_dbg_param(vdev, data);
		break;
		/* Enable/Disable band steering */
	case SON_DISPATCHER_CMD_BSTEERING_ENABLE:
		/* Not required */
		break;
		/* Enable ackrssi */
	case SON_DISPATCHER_CMD_BSTEERING_ENABLE_ACK_RSSI:
		/* Not required */
		break;
		/* GET Peer Class Group */
	case SON_DISPATCHER_CMD_BSTEERING_PEER_CLASS_GROUP:
		retv = son_ucfg_set_get_peer_class_group(vdev, data, false /*get*/);
		break;
		/* SET/GET overload status */
	case SON_DISPATCHER_CMD_BSTEERING_OVERLOAD:
		retv = son_ucfg_set_get_overload(vdev, data, false /*get*/);
		break;
		/* Request RSSI measurement */
	case SON_DISPATCHER_CMD_BSTEERING_TRIGGER_RSSI:
		/* Not required */
		break;
		/* Control whether probe responses are withheld for a MAC */
	case SON_DISPATCHER_CMD_BSTERRING_PROBE_RESP_WH:
		retv = son_probe_response_wh(vdev, data ,false /*get */);
		break;
		/* Data rate info for node */
	case SON_DISPATCHER_CMD_BSTEERING_DATARATE_INFO:
		retv = son_get_peer_info(vdev, data);
		break;
		/* Enable/Disable Band steering events */
	case SON_DISPATCHER_CMD_BSTEERING_ENABLE_EVENTS:
		/* Not required */
		break;
		/* Set Local disassociation*/
	case SON_DISPATCHER_CMD_BSTEERING_LOCAL_DISASSOC:
		/* Not required */
		break;
		/* set steering in progress for node */
	case SON_DISPATCHER_CMD_BSTEERING_INPROG_FLAG:
		/* Not required */
		break;
		/* set stats interval for da */
	case SON_DISPATCHER_CMD_BSTEERING_DA_STAT_INTVL:
		/* Not required */
		break;
		/* AUTH ALLOW during steering prohibit time */
	case SON_DISPATCHER_CMD_BSTERING_AUTH_ALLOW:
		retv = son_auth_allow(vdev, data , false);
		break;
		/* Control whether probe responses are allowed for a MAC in 2.4g band */
	case SON_DISPATCHER_CMD_BSTEERING_PROBE_RESP_ALLOW_24G:
		/* Not required */
		break;
		/* get the in network MAC addresses in 2.4g band */
	case SON_DISPATCHER_CMD_BSTEERING_GET_INNETWORK_24G:
		retv = son_get_innetwork_2g_mac(vdev, data);
		break;
	case SON_DISPATCHER_CMD_MAP_GET_ASSOC_FRAME:
		retv = son_get_assoc_frame(vdev, data);
		break;
		/* get esp info for MAP */
	case SON_DISPATCHER_CMD_MAP_GET_ESP_INFO:
		retv = son_get_map_esp_info(vdev,data);
		break;
		/* get operable channels for MAP */
	case SON_DISPATCHER_CMD_MAP_GET_OP_CHANNELS:
		retv = son_get_map_operable_channels(vdev, data);
		break;
		/* get hardware ap capabilities for MAP */
	case SON_DISPATCHER_CMD_MAP_GET_AP_HWCAP:
		retv = son_get_map_apcap(vdev, data);
		break;

	default:
		SON_LOGF("Invalid cmd %d",cmd);
		retv = -EINVAL;
	}

	return retv;
}

int son_ucfg_set_cmd(struct  wlan_objmgr_vdev *vdev,
		     SON_DISPATCHER_CMD cmd,
		     void *data)
{
	int retv = EOK;

	switch(cmd) {
		/* Set/GET the static band steering parameters */
	case SON_DISPATCHER_CMD_BSTEERING_PARAMS:
		retv = son_set_get_steering_params(vdev, data, true);
		break;
		/* Set the static band steering parameters */
	case SON_DISPATCHER_CMD_BSTEERING_DBG_PARAMS:
		retv = son_pdev_set_dbg_param(vdev, data);
		break;
		/* Enable/Disable band steering */
	case SON_DISPATCHER_CMD_BSTEERING_ENABLE:
		retv = son_pdev_steering_enable_disable(vdev, data);
		break;
		/* Enable Ack Rssi for band steering */
	case SON_DISPATCHER_CMD_BSTEERING_ENABLE_ACK_RSSI:
		retv = son_pdev_steering_enable_ackrssi(vdev, data);
		break;
		/* SET Peer Class Group */
	case SON_DISPATCHER_CMD_BSTEERING_PEER_CLASS_GROUP:
		retv = son_ucfg_set_get_peer_class_group(vdev, data, true /*set*/);
		break;
		/* SET/GET overload status */
	case SON_DISPATCHER_CMD_BSTEERING_OVERLOAD:
		retv = son_ucfg_set_get_overload(vdev, data, true /*set*/);
		break;
		/* Request RSSI measurement */
	case SON_DISPATCHER_CMD_BSTEERING_TRIGGER_RSSI:
		retv = son_trigger_null_frame_tx(vdev, data);
		break;
		/* Control whether probe responses are withheld for a MAC */
	case SON_DISPATCHER_CMD_BSTERRING_PROBE_RESP_WH:
		retv = son_probe_response_wh(vdev, data , true);
		break;
		/* Data rate info for node */
	case SON_DISPATCHER_CMD_BSTEERING_DATARATE_INFO:
		/* Not required */
		break;
		/* Enable/Disable Band steering events */
	case SON_DISPATCHER_CMD_BSTEERING_ENABLE_EVENTS:
		retv = son_enable_disable_vdev_events(vdev, data);
		break;
		/* Set Local disassociation*/
	case SON_DISPATCHER_CMD_BSTEERING_LOCAL_DISASSOC:
		retv = son_peer_local_disassoc(vdev, data);
		break;
		/* set steering in progress for node */
	case SON_DISPATCHER_CMD_BSTEERING_INPROG_FLAG:
		retv = son_peer_set_steering(vdev, data);
		break;
		/* set stats interval for da */
	case SON_DISPATCHER_CMD_BSTEERING_DA_STAT_INTVL:
		retv = son_peer_set_stastats_intvl(vdev, data);
		break;
		/* AUTH ALLOW during steering prohibit time */
	case SON_DISPATCHER_CMD_BSTERING_AUTH_ALLOW:
		retv = son_auth_allow(vdev, data , true);
		break;
		/* Control whether probe responses are allowed for a MAC in 2.4g band */
	case SON_DISPATCHER_CMD_BSTEERING_PROBE_RESP_ALLOW_24G:
		retv = son_peer_set_probe_resp_allow_2G(vdev, data);
		break;
		/* set in network for a MAC in 2.4g band */
	case SON_DISPATCHER_CMD_BSTEERING_SET_INNETWORK_24G:
		retv = son_set_innetwork_2g_mac(vdev, data);
		break;
	case SON_DISPATCHER_CMD_MAP_SET_RSSI_POLICY:
		retv = son_set_map_rssi_policy(vdev, data);
		break;
		/* set acl timer policy for MAP */
	case SON_DISPATCHER_CMD_MAP_SET_TIMER_POLICY:
		retv = son_set_map_timer_policy(vdev, data);
		break;
	default:
		SON_LOGF("Invalid cmd %d",cmd);
		retv = -EINVAL;
	}

	return retv;
}

int ucfg_son_dispatcher(struct wlan_objmgr_vdev *vdev,
		   SON_DISPATCHER_CMD cmd, SON_DISPATCHER_ACTION action,
		   void *data)
{
	int retv = QDF_STATUS_SUCCESS;

	switch (action) {
	case SON_DISPATCHER_SET_CMD:
		retv = son_ucfg_set_cmd(vdev, cmd, data);
		break;
	case SON_DISPATCHER_GET_CMD:
		retv = son_ucfg_get_cmd(vdev, cmd, data);
		break;

	case SON_DISPATCHER_SEND_EVENT:
		break;
		retv = son_ucfg_send_event(vdev, cmd, data);
		break;
	default:
		SON_LOGF("Invalid action %d",action);
		retv = -EINVAL;
	}

	return retv;
}

int son_ucfg_rep_datarate_estimator(u_int16_t backhaul_rate,
				    u_int16_t ap_rate,
				    u_int8_t root_distance,
				    u_int8_t scaling_factor)
{
	int rate;
	if (root_distance == 0) {
		/* Root AP, there is no STA backhaul link */
		return ap_rate;
	} else {
		if (!backhaul_rate || !ap_rate)
			return 0;

		/* Estimate the data rate of repeater AP, (a*b)/(a+b) * (factor/100).
		* 64-bit by 64-bit divison can be an issue in some platform */
		rate = (backhaul_rate * ap_rate)/(backhaul_rate + ap_rate);
		rate = (rate * scaling_factor)/100;
		return rate;
	}
}


static QDF_STATUS
ieee80211_get_bssid_info(void *arg, wlan_scan_entry_t se)
{
	struct ieee80211_uplinkinfo *bestUL = (struct ieee80211_uplinkinfo *)arg;
	struct ieee80211com *ic = NULL;
	u_int8_t se_ssid_len = 0;
	u_int8_t *se_ssid = util_scan_entry_ssid(se)->ssid;
	u_int8_t *se_bssid = util_scan_entry_bssid(se);
	struct ieee80211_ie_whc_apinfo *se_sonadv = NULL;
	u_int16_t se_uplinkrate, se_currentrate, tmprate;
	u_int8_t se_rootdistance;
	wlan_chwidth_e chwidth;
	struct wlan_objmgr_pdev *pdev = NULL;
	u_int8_t *se_otherband_bssid;
	u_int8_t se_snr = util_scan_entry_rssi(se);
	wlan_chan_t chan = NULL;
	u_int8_t zero_bssid[IEEE80211_ADDR_LEN] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

	pdev = wlan_vdev_get_pdev(bestUL->vdev);

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
        if (!ic)
		return 0;

	/* make sure ssid is same */
	se_ssid_len = util_scan_entry_ssid(se)->length;

	if (se_ssid_len == 0 || (se_ssid_len != strlen(bestUL->essid)) || OS_MEMCMP(se_ssid, bestUL->essid, se_ssid_len) != 0)
		return 0;

	se_sonadv = (struct ieee80211_ie_whc_apinfo *)util_scan_entry_sonie(se);
	if (se_sonadv == NULL)
		return 0;

	/* Parse and get the hop count */
	se_rootdistance = se_sonadv->whc_apinfo_root_ap_dist;
	/* Get the backhaul rate from scan entry */
	se_uplinkrate = LE_READ_2(&se_sonadv->whc_apinfo_uplink_rate);

	if ((se_rootdistance == SON_INVALID_ROOT_AP_DISTANCE)
	    || (se_uplinkrate == 0)) {
		/* Isolated independent repeater entry, ignore it.
		 * Also, If currently associated with
		 * isolated independent repeater,
		 * mark it for recovery */
		if (IEEE80211_ADDR_EQ(wlan_vdev_mlme_get_macaddr(bestUL->vdev), se_bssid))
			bestUL->island_detected = 1;

		SON_LOGI("%s: Ignored, island detected %d rootdistance=%d uplinkrate=%d \n",
			 ether_sprintf(se_bssid),
			 bestUL->island_detected,
			 se_rootdistance,
			 se_uplinkrate);
		return 0;
	}

	/* Parse and get the uplink partner BSSID */
	chan = wlan_vdev_get_channel(bestUL->vdev);
	if (chan == NULL)
		se_otherband_bssid = zero_bssid;
	else if (IEEE80211_IS_CHAN_2GHZ(chan))
		se_otherband_bssid = se_sonadv->whc_apinfo_5g_bssid;
	else
		se_otherband_bssid = se_sonadv->whc_apinfo_24g_bssid;

	chwidth = (wlan_chwidth_e)wlan_vdev_get_chwidth(bestUL->vdev);

	/* Current scan entry bssid rate estimate*/
	se_currentrate = son_SNRToPhyRateTablePerformLookup(se_snr,
				    wlan_vdev_get_rx_streams(bestUL->vdev),
				    convert_phymode(bestUL->vdev),
				    chwidth);

	if(se_rootdistance == 0) {
		/* Estimate the rate from RootAP beacon */
		if (son_core_get_cap_rssi(bestUL->vdev) &&
		    se_snr >= son_core_get_cap_rssi(bestUL->vdev))
			tmprate = 0xffff;/* Max data rate, so cap is always preferred */
		else
		    tmprate = se_currentrate;
	} else {
		wlan_if_t tmpvap = NULL;

		/* Get the uplink BSSID from scan entry and compare it with our VAPs BSSID.
		 * If it matches with any of our VAP's BSSID, discard this scan entry.
		 * This will avoid unnecessary restarts by repacd */
		TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
			if (tmpvap->iv_opmode == IEEE80211_M_HOSTAP)
			{
				if (IEEE80211_ADDR_EQ(tmpvap->iv_myaddr, &se_sonadv->whc_apinfo_uplink_bssid))
					return 0;
			}
		}

		/* Estimate the rate from repeater AP beacon */
		tmprate = son_ucfg_rep_datarate_estimator(se_uplinkrate,
			  se_currentrate,
			  se_rootdistance,
			  ucfg_son_get_scaling_factor(bestUL->vdev));
	}

	qdf_print("%s: fronthaul_snr %d backhaul_rate %d fronthaul_rate %d estimate %d",
		  ether_sprintf(se_bssid),
		  se_snr,
		  se_uplinkrate,
		  se_currentrate,
		  tmprate);

	/* update the bssid if better than previous one */
	if(tmprate > bestUL->rate_estimate) {
		bestUL->rate_estimate = tmprate;
		qdf_mem_copy(bestUL->bssid, se_bssid, IEEE80211_ADDR_LEN);
		bestUL->root_distance = se_rootdistance;
		qdf_mem_copy(bestUL->otherband_bssid, se_otherband_bssid,
			     IEEE80211_ADDR_LEN);
	}
	return 0;
}


int ucfg_son_find_best_uplink_bssid(struct wlan_objmgr_vdev *vdev, char *bssid, struct wlan_ssid *ssidname)
{
	struct ieee80211_uplinkinfo bestUL = {0};
	u_int16_t current_rate, max_rate;
	u_int8_t hyst;
	struct wlan_objmgr_peer *bss_peer;
	u_int8_t bss_peer_mac[IEEE80211_ADDR_LEN];

	bestUL.vdev = vdev;
	OS_MEMCPY(&bestUL.essid, &ssidname->ssid[0], ssidname->length);
	max_rate = wlan_ucfg_get_maxphyrate(vdev)/ 1000;
	current_rate = son_ucfg_rep_datarate_estimator(
		son_get_backhaul_rate(vdev, true),
		son_get_backhaul_rate(vdev, false),
		(ucfg_son_get_root_dist(vdev) - 1),
		ucfg_son_get_scaling_factor(vdev));

	if (!ucfg_son_get_skip_hyst(vdev)) {
		hyst = ucfg_son_get_bestul_hyst(vdev);
	} else {
		hyst = 0;
	}

	bss_peer = wlan_vdev_get_bsspeer(vdev);
	if (!bss_peer)
		return -EINVAL;
	OS_MEMCPY(bss_peer_mac, wlan_peer_get_macaddr(bss_peer), IEEE80211_ADDR_LEN);
	if (ucfg_scan_db_iterate(wlan_vdev_get_pdev(vdev), ieee80211_get_bssid_info, &bestUL) == 0) {
		if ((wlan_vdev_is_up(vdev) == QDF_STATUS_SUCCESS)
		    && !IEEE80211_ADDR_EQ(bss_peer_mac, &bestUL.bssid)
		    && !bestUL.island_detected) {
			if (bestUL.rate_estimate < (current_rate + ((max_rate * hyst) / 100))) {
				/* Keep currently serving AP as best bssid, populate otherband bssid as well */
				char ob_bssid[IEEE80211_ADDR_LEN] = {0, 0, 0, 0, 0, 0};
				ucfg_son_get_otherband_uplink_bssid(vdev, ob_bssid);
				son_core_set_best_otherband_uplink_bssid(vdev, &ob_bssid[0]);
				OS_MEMCPY(bssid, bss_peer_mac, IEEE80211_ADDR_LEN);
				return 0;
			}
		}
		OS_MEMCPY(bssid, &bestUL.bssid, IEEE80211_ADDR_LEN);
		son_core_set_best_otherband_uplink_bssid(vdev, &bestUL.otherband_bssid[0]);
	}

	return 0;
}

static QDF_STATUS
ieee80211_get_cap_bssid(void *arg, wlan_scan_entry_t se)
{
	struct ieee80211_uplinkinfo *rootinfo =
		(struct ieee80211_uplinkinfo *)arg;
	u_int8_t se_ssid_len = 0;
	u_int8_t  *se_ssid = util_scan_entry_ssid(se)->ssid;
	u_int8_t *se_bssid = util_scan_entry_bssid(se);
	struct ieee80211_ie_whc_apinfo *se_sonadv = NULL;
	u_int8_t se_rootdistance, se_isrootap;

	/* make sure ssid is same */
	se_ssid_len = util_scan_entry_ssid(se)->length;
	if(se_ssid_len == 0 || (se_ssid_len != strlen(rootinfo->essid)) || OS_MEMCMP(se_ssid, rootinfo->essid, se_ssid_len) != 0)
		return 0;

	se_sonadv = (struct ieee80211_ie_whc_apinfo *)util_scan_entry_sonie(se);
	if(se_sonadv == NULL)
		return 0;

	/* Parse and get the hop count */
	se_rootdistance = se_sonadv->whc_apinfo_root_ap_dist;
	se_isrootap = se_sonadv->whc_apinfo_is_root_ap;

	if(se_rootdistance == 0 && se_isrootap)
		OS_MEMCPY(rootinfo->bssid, se_bssid, IEEE80211_ADDR_LEN);

	return 0;
}

int son_ucfg_find_cap_bssid(struct wlan_objmgr_vdev *vdev, char *bssid)
{
	struct ieee80211_uplinkinfo rootinfo;
	uint8_t ssid[WLAN_SSID_MAX_LEN + 1] = {0};
	u_int8_t len = 0;

	if (QDF_STATUS_SUCCESS !=
			wlan_vdev_mlme_get_ssid(vdev, ssid, &len))
		return -EINVAL;

	memset(&rootinfo, 0, sizeof(struct ieee80211_uplinkinfo));

	OS_MEMCPY(&rootinfo.essid, &ssid[0], len);

	if (ucfg_scan_db_iterate(wlan_vdev_get_pdev(vdev),
			 ieee80211_get_cap_bssid, &rootinfo) == 0)
		OS_MEMCPY(bssid, &rootinfo.bssid, IEEE80211_ADDR_LEN);

	return 0;
}
int ucfg_son_set_otherband_bssid(struct wlan_objmgr_vdev *vdev, int *val)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);

	if (pdev &&
	    wlan_son_is_pdev_valid(pdev))
		return(son_core_set_otherband_bssid(pdev, val));
	else {
		SON_LOGI("SON on Pdev Needs to be enabled for Setting otherband bssid");
		return 0;
	}
	return 0;
}

int ucfg_son_get_best_otherband_uplink_bssid(struct wlan_objmgr_vdev *vdev,
					     char *bssid)
{
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);

	if (pdev &&
	    wlan_son_is_pdev_valid(pdev))
		return(son_core_get_best_otherband_uplink_bssid(pdev, bssid));
	else {
		SON_LOGI("SON on Pdev Needs to be enabled for Setting otherband bssid");
		return 0;
	}

	return 0;
}
static QDF_STATUS
ieee80211_get_otherband_uplink_bssid(void *arg, wlan_scan_entry_t se)
{
	struct ieee80211_uplinkinfo *uplink =
		(struct ieee80211_uplinkinfo *)arg;
	struct wlan_objmgr_vdev *vdev = uplink->vdev;
	u_int8_t se_ssid_len = 0;
	u_int8_t *se_ssid = util_scan_entry_ssid(se)->ssid;
	u_int8_t *se_bssid = util_scan_entry_bssid(se);
	struct ieee80211_ie_whc_apinfo *se_sonadv = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_peer *peer;
	u_int8_t *se_otherband_bssid;
	wlan_chan_t chan = NULL;
	u_int8_t zero_bssid[IEEE80211_ADDR_LEN] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };

	pdev = wlan_vdev_get_pdev(vdev);

	/* make sure ssid is same */
	se_ssid_len = util_scan_entry_ssid(se)->length;

	if(se_ssid_len == 0 || (se_ssid_len != strlen(uplink->essid)) || OS_MEMCMP(se_ssid, uplink->essid, se_ssid_len) != 0)
		return 0;

	se_sonadv = (struct ieee80211_ie_whc_apinfo *)util_scan_entry_sonie(se);

	if(se_sonadv == NULL)
		return 0;

	peer = wlan_vdev_get_bsspeer(vdev);
	if(IEEE80211_ADDR_EQ(se_bssid, peer->macaddr)) {
		/* Parse and get the uplink partner BSSID */
		chan = wlan_vdev_get_channel(vdev);
		if (chan == NULL)
			se_otherband_bssid = zero_bssid;
		else if (IEEE80211_IS_CHAN_2GHZ(chan))
			se_otherband_bssid = se_sonadv->whc_apinfo_5g_bssid;
		else
			se_otherband_bssid = se_sonadv->whc_apinfo_24g_bssid;

		OS_MEMCPY(uplink->bssid, se_otherband_bssid, IEEE80211_ADDR_LEN);
	}

	return 0;
}

int ucfg_son_get_otherband_uplink_bssid(struct wlan_objmgr_vdev *vdev,
					char *addr)
{
	struct ieee80211_uplinkinfo uplink = {0};
	uint8_t ssid[WLAN_SSID_MAX_LEN + 1] = {0};
	u_int8_t len = 0;

	if (QDF_STATUS_SUCCESS !=
			wlan_vdev_mlme_get_ssid(vdev, ssid, &len))
		return -EINVAL;

	uplink.vdev = vdev;
	OS_MEMCPY(&uplink.essid, &ssid[0], len);

	if (ucfg_scan_db_iterate(wlan_vdev_get_pdev(vdev),
				 ieee80211_get_otherband_uplink_bssid,
				 &uplink) == 0) {
		qdf_mem_copy(addr, &uplink.bssid, IEEE80211_ADDR_LEN);
	}

	return 0;
}

u_int8_t ucfg_son_get_bestul_hyst(struct wlan_objmgr_vdev *vdev)
{
	return son_core_get_bestul_hyst(vdev);
}

void ucfg_son_set_bestul_hyst(struct wlan_objmgr_vdev *vdev, u_int8_t hyst)
{
	son_core_set_bestul_hyst(vdev, hyst);
}

#else

int ucfg_son_dispatcher(struct wlan_objmgr_vdev *vdev,
			SON_DISPATCHER_CMD cmd,
			SON_DISPATCHER_ACTION action, void *data)
{
	return -EINVAL;

}

int son_ucfg_rep_datarate_estimator(u_int16_t backhaul_rate,
				    u_int16_t ap_rate,
				    u_int8_t root_distance,
				    u_int8_t scaling_factor)
{
	return ap_rate;

}

static inline int wlan_son_enable_events(struct wlan_objmgr_vdev *vdev,
					 struct ieee80211req_athdbg *req)
{
	return -EINVAL;
}

u_int8_t ucfg_son_get_scaling_factor(struct wlan_objmgr_vdev *vdev)
{
	return 0;

}
int8_t ucfg_son_set_scaling_factor(struct wlan_objmgr_vdev *vdev , int8_t scaling_factor)
{
	return EOK;

}

u_int8_t ucfg_son_get_skip_hyst(struct wlan_objmgr_vdev *vdev)
{
	return 0;

}
int8_t ucfg_son_set_skip_hyst(struct wlan_objmgr_vdev *vdev , int8_t skip_hyst)
{
	return EOK;

}

int ucfg_son_find_best_uplink_bssid(struct wlan_objmgr_vdev *vdev, char *bssid, struct wlan_ssid *ssidname)
{
	return 0;

}

int son_ucfg_find_cap_bssid(struct wlan_objmgr_vdev *vdev, char *bssid)
{
	return 0;
}

int ucfg_son_set_otherband_bssid(struct wlan_objmgr_vdev *vdev, int *val)
{
	return EOK;
}

int ucfg_son_get_best_otherband_uplink_bssid(struct wlan_objmgr_vdev *vdev,
					     char *bssid)
{
	return EOK;
}
int ucfg_son_get_otherband_uplink_bssid(struct wlan_objmgr_vdev *vdev, char *addr)
{
	return EOK;
}

int8_t ucfg_son_set_uplink_rate(struct wlan_objmgr_vdev *vdev,
				u_int32_t uplink_rate)
{
	return EOK;
}
int8_t ucfg_son_get_cap_rssi(struct wlan_objmgr_vdev *vdev)
{
	return EOK;
}

int8_t ucfg_son_set_cap_rssi(struct wlan_objmgr_vdev *vdev,
			     u_int32_t rssi)
{
	return EOK;
}

int ucfg_son_get_cap_snr(struct wlan_objmgr_vdev *vdev, int *cap_snr)
{
	return 0;

}

u_int8_t ucfg_son_get_bestul_hyst(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

void ucfg_son_set_bestul_hyst(struct wlan_objmgr_vdev *vdev, u_int8_t hyst)
{
}

#endif
