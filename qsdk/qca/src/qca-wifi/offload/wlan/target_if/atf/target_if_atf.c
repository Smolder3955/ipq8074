/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <wlan_tgt_def_config.h>
#include <hif.h>
#include <hif_hw_version.h>
#include <wmi_unified_api.h>
#include <target_if_atf.h>
#include <wlan_lmac_if_def.h>
#include <wlan_osif_priv.h>
#include <target_if.h>
#include <wlan_utility.h>
#include <target_type.h>
#include <wlan_vdev_mgr_tgt_if_tx_defs.h>

extern void osif_get_peer_mac_from_peer_id(struct wlan_objmgr_pdev *pdev,
				uint32_t peer_id, uint8_t *peer_mac);

uint32_t target_if_atf_get_num_msdu_desc(struct wlan_objmgr_psoc *psoc)
{
	uint32_t atf_msdu_desc = 0;
	uint32_t num_msdu_desc = 0;

	if (psoc) {
		atf_msdu_desc = target_if_atf_get_msdu_desc(psoc);
		if (atf_msdu_desc) {
			if (atf_msdu_desc < CFG_TGT_NUM_MSDU_DESC_AR988X) {
				num_msdu_desc = CFG_TGT_NUM_MSDU_DESC_AR988X;
			} else {
				num_msdu_desc = atf_msdu_desc;
			}
		} else {
			num_msdu_desc = CFG_TGT_NUM_MSDU_DESC_ATF;
		}
	}

	return num_msdu_desc;
}
qdf_export_symbol(target_if_atf_get_num_msdu_desc);

uint8_t
target_if_atf_is_tx_traffic_blocked(struct wlan_objmgr_vdev *vdev,
				    uint8_t *peer_mac, struct sk_buff *skb)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	uint8_t retval = 0;
	uint8_t pdev_id;
	uint8_t ac_bitmap = 0;
	int8_t ac = 0;
	int32_t tid = 0;

	if (NULL == vdev) {
		atf_err("vdev is NULL!\n");
		return retval;
	}

	pdev_id =  wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	psoc = wlan_vdev_get_psoc(vdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL!\n");
		return retval;
	}

	if (!target_if_atf_get_mode(psoc)) {
		return retval;
	}

	if (target_if_atf_get_vdev_blk_txtraffic(psoc, vdev)) {
		atf_debug("vdev blk count set.Return \n");
		retval = 1;
	} else if (target_if_atf_get_vdev_ac_blk_cnt(psoc, vdev)) {
		if (skb) {
			tid = wbuf_classify(skb, 0);
		}
		if (NULL == peer_mac) {
			atf_err("MAC address is NULL\n");
			return retval;
		}
		peer = wlan_objmgr_get_peer(psoc, pdev_id, peer_mac,
					    WLAN_ATF_ID);
		if (peer) {
			ac_bitmap = target_if_atf_get_peer_blk_txbitmap(
								psoc, peer);
			if (ac_bitmap == ATF_TX_BLOCK_AC_ALL) {
				retval = 1;
			} else {
				ac = TID_TO_WME_AC(tid);
				if (ac_bitmap & (1 << ac)) {
					retval = 1;
				}
			}
			wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
		}
	}

	return retval;
}

uint32_t target_if_atf_peer_getairtime(struct wlan_objmgr_peer *peer)
{
	uint32_t airtime = 0;
	uint32_t token_allocated;
	uint32_t token_utilized;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_psoc *psoc;

	vdev = wlan_peer_get_vdev(peer);
	if (vdev == NULL) {
		return 0;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (psoc == NULL) {
		return 0;
	}
	token_allocated = target_if_atf_get_token_allocated(psoc, peer);
	token_utilized = target_if_atf_get_token_utilized(psoc, peer);

	/* calc airtime %age */
	if (token_allocated)
		airtime = (token_utilized / token_allocated) * 100;

	return airtime;
}

uint32_t target_if_atf_peer_buf_held(struct wlan_objmgr_peer *peer)
{
	return 0;
}

int32_t target_if_atf_enable_disable(struct wlan_objmgr_vdev *vdev,
				     uint8_t value)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	int32_t retval = 0;
	uint8_t pdev_id = 0;
	struct pdev_params pparam;
	void *wmi_handle = NULL;

	if (NULL == vdev) {
		atf_err("vdev is NULL\n");
		return -1;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (NULL == pdev) {
		atf_err("pdev is NULL\n");
		return -1;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL\n");
		return -1;
	}
	wmi_handle = GET_WMI_HDL_FROM_PSOC(psoc);
	if (NULL == wmi_handle) {
		atf_err("Invalid WMI handle!\n");
		return -1;
	}

	qdf_mem_zero(&pparam, sizeof(pparam));
	if (!value) {
		pparam.param_id = wmi_pdev_param_atf_peer_stats;
		pparam.param_value = WMI_HOST_ATF_PEER_STATS_DISABLED;
		retval = wmi_unified_pdev_param_send(wmi_handle,
						     &pparam, pdev_id);
	}
	qdf_mem_zero(&pparam, sizeof(pparam));
	pparam.param_id = wmi_pdev_param_atf_dynamic_enable;
	pparam.param_value = value;
	retval = wmi_unified_pdev_param_send(wmi_handle, &pparam, pdev_id);
	qdf_mem_zero(&pparam, sizeof(pparam));
	if (value) {
		pparam.param_id = wmi_pdev_param_atf_peer_stats;
		pparam.param_value = WMI_HOST_ATF_PEER_STATS_ENABLED;
		retval = wmi_unified_pdev_param_send(wmi_handle,
						     &pparam, pdev_id);
	}

	return retval;
}

int32_t target_if_atf_ssid_sched_policy(struct wlan_objmgr_vdev *vdev,
								uint8_t value)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct vdev_set_params vparam;
	int32_t retval = 0;
	uint8_t vdev_id = 0;
	void *wmi_handle = NULL;

	if (NULL == vdev) {
		atf_err("vdev is NULL\n");
		return -1;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	vdev_id = wlan_vdev_get_id(vdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL\n");
		return -1;
	}
	wmi_handle = GET_WMI_HDL_FROM_PSOC(psoc);
	if (NULL == wmi_handle) {
		atf_err("Invalid WMI handle!\n");
		return -1;
	}

	qdf_mem_zero(&vparam, sizeof(vparam));
	vparam.vdev_id = vdev_id;
	vparam.param_id = wmi_vdev_param_atf_ssid_sched_policy;
	vparam.param_value = value;
	retval = wmi_unified_vdev_set_param_send(wmi_handle, &vparam);

	return retval;
}

int32_t target_if_atf_set_bwf(struct wlan_objmgr_pdev *pdev,
						struct pdev_bwf_req *bwf_req)
{
	int i = 0, retval = 0;
	struct set_bwf_params *param = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	void *wmi_handle = NULL;

	if (!bwf_req || !pdev) {
		atf_err("Invalid parameter\n");
		return -EINVAL;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL!\n");
		return -EINVAL;
	}
	wmi_handle = GET_WMI_HDL_FROM_PSOC(psoc);
	if (NULL == wmi_handle) {
		atf_err("Invalid WMI handle!\n");
		return -EINVAL;
	}
	param = (struct set_bwf_params *)qdf_mem_malloc(
				sizeof(struct set_bwf_params) +
				((ATF_ACTIVED_MAX_CLIENTS - 1) *
				sizeof(bwf_peer_info)));

	if (!param) {
		atf_err("Unable to allocate temporary copy of mu report "
			"event,	Dropping mu report event\n");
		return -EINVAL;
	}

	param->num_peers = bwf_req->num_peers;
	for (i = 0; i < bwf_req->num_peers; i++) {
		qdf_mem_copy(&(param->peer_info[i]),
		&(bwf_req->bwf_peer_info[i]), sizeof(bwf_peer_info));
	}

	retval = wmi_unified_set_bwf_cmd_send(wmi_handle, param);
	qdf_mem_free(param);

	return retval;
}

int32_t target_if_atf_set(struct wlan_objmgr_pdev *pdev,
			struct pdev_atf_req *atf_req, uint8_t atf_tput_based)
{
	int i = 0, retval = 0;
	struct set_atf_params param;
	struct wlan_objmgr_psoc *psoc = NULL;
	void *wmi_handle = NULL;

	if (!atf_req || !pdev) {
		atf_err("Invalid parameter\n");
		return -EINVAL;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL!\n");
		return -EINVAL;
	}
	wmi_handle = GET_WMI_HDL_FROM_PSOC(psoc);
	if (NULL == wmi_handle) {
		atf_err("Invalid WMI handle!\n");
		return -EINVAL;
	}
	qdf_mem_zero(&param, sizeof(param));
	param.num_peers = atf_req->num_peers;
	for (i = 0; i < atf_req->num_peers; i++) {
		qdf_mem_copy((void *)&(param.peer_info[i]),
				(void *)&(atf_req->atf_peer_info[i]),
				sizeof(atf_peer_info));
	}
	if (!atf_tput_based) {
		retval = wmi_unified_set_atf_cmd_send(wmi_handle, &param);
	}

	return retval;
}

int32_t target_if_atf_send_peer_request(struct wlan_objmgr_pdev *pdev,
	struct pdev_atf_peer_ext_request *atf_peer_req, uint8_t atf_tput_based)
{
	int i = 0, retval = 0;
	struct atf_peer_request_params peer_params;
	struct wlan_objmgr_psoc *psoc = NULL;
	void *wmi_handle = NULL;

	if (!atf_peer_req || !pdev) {
		atf_err("Invalid parameter\n");
		return -EINVAL;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL!\n");
		return -EINVAL;
	}
	wmi_handle = GET_WMI_HDL_FROM_PSOC(psoc);
	if (NULL == wmi_handle) {
		atf_err("Invalid WMI handle!\n");
		return -EINVAL;
	}
	qdf_mem_zero(&peer_params, sizeof(peer_params));
	peer_params.num_peers = atf_peer_req->num_peers;
	for (i = 0; i < atf_peer_req->num_peers; i++) {
		qdf_mem_copy((void *)&(peer_params.peer_ext_info[i]),
				(void *)&(atf_peer_req->atf_peer_ext_info[i]),
				sizeof(atf_peer_ext_info));
	}

	if (!atf_tput_based) {
		retval = wmi_send_atf_peer_request_cmd(wmi_handle,
						       &peer_params);
	}

	return retval;
}

int32_t target_if_atf_set_grouping(struct wlan_objmgr_pdev *pdev,
	struct pdev_atf_ssid_group_req *atf_group_req, uint8_t atf_tput_based)
{
	int i = 0, retval = 0;
	struct atf_grouping_params group_params;
	struct wlan_objmgr_psoc *psoc = NULL;
	void *wmi_handle = NULL;

	if (!atf_group_req || !pdev) {
		atf_err("Invalid parameter\n");
		return -EINVAL;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL!\n");
		return -EINVAL;
	}
	wmi_handle = GET_WMI_HDL_FROM_PSOC(psoc);
	if (NULL == wmi_handle) {
		atf_err("Invalid WMI handle!\n");
		return -EINVAL;
	}
	qdf_mem_zero(&group_params, sizeof(group_params));
	group_params.num_groups = atf_group_req->num_groups;

	for (i = 0; i < atf_group_req->num_groups; i++) {
		qdf_mem_copy((void *)&(group_params.group_info[i]),
				(void *)&(atf_group_req->atf_group_info[i]),
				sizeof(atf_group_info));
	}

	if (!atf_tput_based) {
		retval = wmi_send_set_atf_grouping_cmd(wmi_handle,
						       &group_params);
	}

	return retval;
}

int32_t target_if_atf_set_group_ac(struct wlan_objmgr_pdev *pdev,
	struct pdev_atf_group_wmm_ac_req *atf_group_wmm_ac_req,
	uint8_t atf_tput_based)
{
	int i,retval = 0;
	struct atf_group_ac_params group_ac_params;
	struct wlan_objmgr_psoc *psoc = NULL;
	void *wmi_handle = NULL;

	if (!atf_group_wmm_ac_req || !pdev) {
		atf_err("Invalid parameter\n");
		return -EINVAL;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL!\n");
		return -EINVAL;
	}
	wmi_handle = GET_WMI_HDL_FROM_PSOC(psoc);
	if (NULL == wmi_handle) {
		atf_err("Invalid WMI handle!\n");
		return -EINVAL;
	}
	qdf_mem_zero(&group_ac_params, sizeof(group_ac_params));
	group_ac_params.num_groups = atf_group_wmm_ac_req->num_groups;
	for (i =0; i< atf_group_wmm_ac_req->num_groups; i++)
	{
		qdf_mem_copy((void *)&(group_ac_params.group_info[i]),
		      (void *)&(atf_group_wmm_ac_req->atf_group_wmm_ac_info[i]),
		      sizeof(struct atf_group_wmm_ac_info));
	}
	if (!atf_tput_based){
		retval = wmi_send_set_atf_group_ac_cmd(wmi_handle,
						       &group_ac_params);
	}

	return retval;
}

int target_if_atf_peer_stats_event_handler(ol_scn_t sc,
						uint8_t *data, uint32_t datalen)
{
	ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
	wmi_host_atf_peer_stats_event airtime;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_peer *peer_obj = NULL;
	struct atf_stats astats;
	uint32_t used = 0, unused = 0;
	uint32_t total = 0, node_unusedtokens = 0;
	wmi_host_atf_peer_stats_info token_info;
	uint32_t i = 0, peer_id = 0;
	u_int32_t airtime_avl = 0;
	void *tgt_if_handle = 0;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE] = {0};

	psoc = soc->psoc_obj;
	if (NULL == psoc) {
		atf_err("psoc is NULL\n");
		return -EINVAL;
	}
	tgt_if_handle = GET_WMI_HDL_FROM_PSOC(psoc);
	if (NULL == tgt_if_handle) {
		atf_err("tgt_if_handle is NULL\n");
		return -EINVAL;
	}
	if (wmi_extract_atf_peer_stats_ev(tgt_if_handle, data, &airtime)) {
		atf_err("Unable to extract atf peer stats event\n");
		return -EINVAL;
	}

	pdev = wlan_objmgr_get_pdev_by_id(psoc, airtime.pdev_id, WLAN_ATF_ID);
	if (NULL == pdev) {
		atf_err("Pdev NULL\n");
		return -EINVAL;
	}
	for (i = 0; i < airtime.num_atf_peers; i++) {
		if (wmi_extract_atf_token_info_ev(tgt_if_handle, data, i,
								&token_info)) {
			atf_err("Unable to extract atf token info\n");
			wlan_objmgr_pdev_release_ref(pdev, WLAN_ATF_ID);
			return -EINVAL;
		}
		used = WMI_HOST_ATF_PEER_STATS_GET_USED_TOKENS(token_info) * 32;
		unused = WMI_HOST_ATF_PEER_STATS_GET_UNUSED_TOKENS(token_info)
									* 32;
		total += used + unused;
	}

	/* printk("recv atf peer evt. num %d max AT %d total %d\n",
	airtime->num_atf_peers, airtime->comp_usable_airtime, total);
	*/
	if (total < airtime.comp_usable_airtime)
		total = airtime.comp_usable_airtime;
	for (i = 0; i < airtime.num_atf_peers; i++) {
		if (wmi_extract_atf_token_info_ev(tgt_if_handle, data, i,
								&token_info)) {
			atf_err("Unable to extract atf token info\n");
			if (peer_obj)
				wlan_objmgr_peer_release_ref(peer_obj,
								WLAN_ATF_ID);
			wlan_objmgr_pdev_release_ref(pdev, WLAN_ATF_ID);
			return -EINVAL;
		}
		used = WMI_HOST_ATF_PEER_STATS_GET_USED_TOKENS(token_info);
		unused = WMI_HOST_ATF_PEER_STATS_GET_UNUSED_TOKENS(token_info);
		peer_id = WMI_HOST_ATF_PEER_STATS_GET_PEER_AST_IDX(token_info);
		/* printk("%d) ast idx 0x%x us 0x%x un 0x%x t 0x%x\n", i,
		peer_id, used, unused, used+unused); */
		used = used * 32;
		unused = unused * 32;
		if (used+unused == 0)
			continue;
		qdf_mem_zero(peer_mac, QDF_MAC_ADDR_SIZE);
		osif_get_peer_mac_from_peer_id(pdev, peer_id, peer_mac);
		peer_obj = wlan_objmgr_get_peer(psoc, airtime.pdev_id,
						peer_mac, WLAN_ATF_ID);
		if (!peer_obj) {
			continue;
		}
		/* Get the existing peer stats and update it */
		psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_peer_stats(peer_obj,
								&astats);

		astats.act_tokens = used + unused;
		astats.unused = node_unusedtokens = unused;
		astats.total = total;

		if ((astats.act_tokens > node_unusedtokens) &&
							(astats.total > 0)) {
			/* Note the math: 200k tokens every 200 ms => 1000k
			 tokens / second => 1 token = 1 us.*/
			astats.total_used_tokens += (astats.act_tokens -
							node_unusedtokens);
			if (psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_logging(
									pdev)) {
				airtime_avl = (((astats.act_tokens -
				       node_unusedtokens) *100) / astats.total);
				atf_info("client %s is currently using %d "
				"usecs which is %d%% of available airtime\n",
				ether_sprintf(peer_mac),
				(astats.act_tokens - node_unusedtokens),
				airtime_avl);
			}
		}
		/* Set updated peer stats */
		psoc->soc_cb.rx_ops.atf_rx_ops.atf_set_peer_stats(peer_obj,
								&astats);
		wlan_objmgr_peer_release_ref(peer_obj, WLAN_ATF_ID);
	}
	wlan_objmgr_pdev_release_ref(pdev, WLAN_ATF_ID);

	return 0;
}

static void atf_block_unblock_peer_traffic(struct wlan_objmgr_psoc *psoc,
					   uint32_t cmd,
					   struct wlan_objmgr_vdev *vdev,
					   struct wlan_objmgr_peer *peer,
					   uint8_t ac_id)
{
	struct wlan_lmac_if_atf_rx_ops *rx_ops = NULL;

	if ((psoc == NULL) || (peer == NULL) || (vdev == NULL)) {
		atf_err("psoc or vdev or peer are NULL\n");
		return;
	}
	rx_ops = &psoc->soc_cb.rx_ops.atf_rx_ops;

	switch (cmd) {
	case WMI_HOST_TX_DATA_TRAFFIC_CTRL_UNBLOCK:
		atf_debug("unblock peer : %s ac : %d \n",
			  ether_sprintf(peer->macaddr), ac_id);
		rx_ops->atf_peer_unblk_txtraffic(peer, ac_id);
		break;
	case WMI_HOST_TX_DATA_TRAFFIC_CTRL_BLOCK:
		atf_debug("block peer : %s ac : %d \n",
			  ether_sprintf(peer->macaddr), ac_id);
		rx_ops->atf_peer_blk_txtraffic(peer, ac_id);
		break;
	default:
		break;
	}

	return;
}

int target_if_atf_tx_data_traffic_ctrl_event_handler(ol_scn_t sc,
					uint8_t *data, uint32_t datalen)
{
	ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
	wmi_host_tx_data_traffic_ctrl_event evt;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_peer *peer_obj = NULL;
	struct wlan_lmac_if_atf_rx_ops *rx_ops = NULL;
	void *tgt_if_handle = 0;
	uint8_t peer_mac[QDF_MAC_ADDR_SIZE] = {0};
	uint8_t pdev_id;

	psoc = soc->psoc_obj;
	if (NULL == psoc) {
		atf_err("psoc is NULL\n");
		return -EINVAL;
	}
	tgt_if_handle = GET_WMI_HDL_FROM_PSOC(psoc);
	if (NULL == tgt_if_handle) {
		atf_err("tgt_if_handle is NULL\n");
		return -EINVAL;
	}

	atf_debug("atf Traffic ctrl Event Received \n");
	if (wmi_extract_tx_data_traffic_ctrl_ev(tgt_if_handle, data, &evt)) {
		atf_err("Unable to extract tx data traffic\n");
		return -EINVAL;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, evt.vdev_id,
								WLAN_ATF_ID);
	if (vdev == NULL) {
		atf_err("Unable to find vdev for %d vdev_id\n", evt.vdev_id);
		return -EINVAL;
	}
	rx_ops = &psoc->soc_cb.rx_ops.atf_rx_ops;
	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev == NULL) {
		atf_err("Unable to find pdev for %d vdev_id\n", evt.vdev_id);
	        wlan_objmgr_vdev_release_ref(vdev, WLAN_ATF_ID);
		return -EINVAL;
        }
	pdev_id =  wlan_objmgr_pdev_get_pdev_id(pdev);

	if (evt.peer_ast_idx == WMI_HOST_INVALID_PEER_AST_INDEX) {
		atf_debug("Stop traffic for VAP peer_idx : %d cmd : %d\n",
			  evt.peer_ast_idx, evt.ctrl_cmd);
		/* Invalid peer_ast_idx.
		 * Stop data tx traffic for a particular vap/vdev
		*/
		switch (evt.ctrl_cmd) {
		case WMI_HOST_TX_DATA_TRAFFIC_CTRL_UNBLOCK:
			/* allow traffic */
			rx_ops->atf_set_vdev_blk_txtraffic(vdev, 0);
			break;
		case WMI_HOST_TX_DATA_TRAFFIC_CTRL_BLOCK:
			/* stop traffic */
			rx_ops->atf_set_vdev_blk_txtraffic(vdev, 1);
			break;
		default:
			break;
		}
	} else {
		atf_debug("Stop traffic for a peer.peer index : %d\n",
			  evt.peer_ast_idx, evt.ctrl_cmd);
		/* Stop data tx traffic for a particular node/peer */
		qdf_mem_zero(peer_mac, QDF_MAC_ADDR_SIZE);
		osif_get_peer_mac_from_peer_id(pdev, evt.peer_ast_idx,
					       peer_mac);
		peer_obj = wlan_objmgr_get_peer(psoc, pdev_id,
						peer_mac, WLAN_ATF_ID);
		if (peer_obj) {
			atf_block_unblock_peer_traffic(psoc,
						evt.ctrl_cmd, vdev, peer_obj,
						(evt.wmm_ac & 0xF));
			wlan_objmgr_peer_release_ref(peer_obj, WLAN_ATF_ID);
		}
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_ATF_ID);

	return 0;
}

void target_if_atf_open(struct wlan_objmgr_psoc *psoc)
{
	struct target_psoc_info *tgt_hdl = NULL;
	uint32_t target_type = 0, target_ver = 0;

	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return;
	}

	tgt_hdl = wlan_psoc_get_tgt_if_handle(psoc);
	if (!tgt_hdl) {
		atf_err("psoc target info is NULL!\n");
		return;
	}

	target_type = target_psoc_get_target_type(tgt_hdl);
	target_ver = target_psoc_get_target_ver(tgt_hdl);
	/*
	 * In target type TARGET_TYPE_AR9888 and target version
	 * AR9888_REV2_VERSION (i.e. beeliner) we need to set a fixed
	 * value. Currently default higher value is being set.
	 */
	if (target_if_atf_get_mode(psoc)) {
		if ((target_type == TARGET_TYPE_AR9888) &&
		    (target_ver == AR9888_REV2_VERSION)) {
			psoc->soc_cb.rx_ops.atf_rx_ops.atf_set_msdu_desc(
					psoc, 0);
			psoc->soc_cb.rx_ops.atf_rx_ops.atf_set_peers(
					psoc, 0);
			psoc->soc_cb.rx_ops.atf_rx_ops.atf_set_max_vdevs(
					psoc, 0);
		} else {
			psoc->soc_cb.rx_ops.atf_rx_ops.atf_set_msdu_desc(
					psoc, CFG_TGT_NUM_MSDU_DESC_ATF);
			psoc->soc_cb.rx_ops.atf_rx_ops.atf_set_peers(
					psoc, CFG_TGT_NUM_PEERS_MAX);
			psoc->soc_cb.rx_ops.atf_rx_ops.atf_set_max_vdevs(
					psoc, CFG_TGT_NUM_VDEV_VOW);
		}
		atf_info("User provided peer = %d,vdevs = %d,msdu_desc = %d\n",
			 CFG_TGT_NUM_PEERS_MAX, CFG_TGT_NUM_VDEV_VOW,
			 CFG_TGT_NUM_MSDU_DESC_ATF);
	}
}

void target_if_atf_register_wmi_event_handler(struct wlan_objmgr_psoc *psoc)
{
	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return;
	}

	wmi_unified_register_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_atf_peer_stats_event_id,
			target_if_atf_peer_stats_event_handler,
			WMI_RX_UMAC_CTX);

	wmi_unified_register_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_tx_data_traffic_ctrl_event_id,
			target_if_atf_tx_data_traffic_ctrl_event_handler,
			WMI_RX_UMAC_CTX);
}

void target_if_atf_unregister_wmi_event_handler(struct wlan_objmgr_psoc *psoc)
{
	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return;
	}

	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_atf_peer_stats_event_id);

	wmi_unified_unregister_event_handler(
			get_wmi_unified_hdl_from_psoc(psoc),
			wmi_tx_data_traffic_ctrl_event_id);
}

void target_if_atf_tx_ops_register(struct wlan_lmac_if_tx_ops *tx_ops)
{
	tx_ops->atf_tx_ops.atf_node_unblock = NULL;
	tx_ops->atf_tx_ops.atf_set_enable_disable = NULL;
	tx_ops->atf_tx_ops.atf_tokens_used = NULL;
	tx_ops->atf_tx_ops.atf_get_unused_txtoken = NULL;
	tx_ops->atf_tx_ops.atf_peer_resume = NULL;
	tx_ops->atf_tx_ops.atf_tokens_unassigned = NULL;
	tx_ops->atf_tx_ops.atf_capable_peer = NULL;
	tx_ops->atf_tx_ops.atf_airtime_estimate = NULL;
	tx_ops->atf_tx_ops.atf_debug_peerstate = NULL;
	tx_ops->atf_tx_ops.atf_enable_disable = target_if_atf_enable_disable;
	tx_ops->atf_tx_ops.atf_ssid_sched_policy =
						target_if_atf_ssid_sched_policy;
	tx_ops->atf_tx_ops.atf_set = target_if_atf_set;
	tx_ops->atf_tx_ops.atf_set_grouping = target_if_atf_set_grouping;
	tx_ops->atf_tx_ops.atf_set_group_ac = target_if_atf_set_group_ac;
	tx_ops->atf_tx_ops.atf_send_peer_request =
						target_if_atf_send_peer_request;
	tx_ops->atf_tx_ops.atf_set_bwf = target_if_atf_set_bwf;
	tx_ops->atf_tx_ops.atf_peer_buf_held = target_if_atf_peer_buf_held;
	tx_ops->atf_tx_ops.atf_get_peer_airtime = target_if_atf_peer_getairtime;
	tx_ops->atf_tx_ops.atf_open = target_if_atf_open;
	tx_ops->atf_tx_ops.atf_register_event_handler =
				target_if_atf_register_wmi_event_handler;
	tx_ops->atf_tx_ops.atf_unregister_event_handler =
				target_if_atf_unregister_wmi_event_handler;
}

