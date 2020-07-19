/*
 *
 * Copyright (c) 2011-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

#include "atf_cmn_api_i.h"
#include <osif_private.h>
#include <osdep.h>

extern uint64_t wlan_objmgr_get_current_channel_flag(
					struct wlan_objmgr_pdev *pdev);

static int32_t atf_set_da(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_atf *pa;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct atf_context *ac = NULL;
	int32_t retval = 0;

	if (NULL == vdev) {
		atf_err("vdev is NULL!\n");
		return -1;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	psoc = wlan_vdev_get_psoc(vdev);
	if (NULL == pdev) {
		atf_err("pdev is NULL!\n");
		return -1;
	}
	if (NULL == psoc) {
		atf_err("psoc is NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev_atf is NULL!\n");
		return -1;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return -1;
	}

	pa->atf_commit = 1;
	if (pa->atf_maxclient && (pa->atf_num_clients != IEEE80211_128_AID))
		retval = IEEE80211_128_AID;
	if (!pa->atf_maxclient && (pa->atf_num_clients != ATF_MAX_AID_DEF))
		retval = ATF_MAX_AID_DEF;
	if (pa->atf_tput_tbl_num && !pa->atf_maxclient) {
		psoc->soc_cb.tx_ops.atf_tx_ops.atf_set_enable_disable(pdev,
								ATF_TPUT_BASED);
		if (!pa->atf_tput_based)
			retval = ATF_MAX_AID_DEF;
	} else {
		psoc->soc_cb.tx_ops.atf_tx_ops.atf_set_enable_disable(pdev,
							ATF_AIRTIME_BASED);
	}

	if (pa->atf_tput_tbl_num && !pa->atf_maxclient)
		pa->atf_tput_based = 1;
	else if (pa->atf_tput_tbl_num)
		atf_info("can't enable tput based atf "
				"as maxclients is enabled\n");
	if (ac->atf_fmcap && ac->atf_mode) {
		atf_start_cfg_timer(vdev, pa, ac);
		qdf_timer_mod(&pa->atf_tokenalloc_timer, ATF_TOKEN_INTVL_MS);
	}

	return retval;
}

static void atf_peer_resume_iterater(struct wlan_objmgr_pdev *pdev,
				     void *object, void *arg)
{
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)arg;
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;

	if ((NULL == pdev) || (NULL == psoc) || (NULL == peer)) {
		atf_err("pdev or peer or psoc are NULL!\n");
		return;
	}

	atf_peer_resume(psoc, pdev, peer);
}

static int32_t atf_clear_da(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_atf *pa = NULL;
	int32_t retv = 0;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;

	if (NULL == vdev) {
		atf_err("vdev is NULL!\n");
		return -1;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (NULL == pdev) {
		atf_err("pdev is NULL!\n");
		return -1;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev_atf is NULL!\n");
		return -1;
	}

	/* When ATF is disabled, set atf_num_clients to default value */
	if (pa->atf_num_clients != IEEE80211_128_AID)
		retv = IEEE80211_128_AID;

	/* Before disabling ATF, resume any paused nodes */
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
			atf_peer_resume_iterater, psoc, 0, WLAN_ATF_ID);

	psoc->soc_cb.tx_ops.atf_tx_ops.atf_set_enable_disable(pdev,
							ATF_DISABLED);

	return retv;
}

static int32_t atf_set_maxclient_da(struct wlan_objmgr_pdev *pdev,
				    uint8_t enable)
{
	struct pdev_atf *pa = NULL;
	uint32_t numclients = 0;
	uint8_t reset = 0;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return -1;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev atf object is NULL\n");
		return -1;
	}
	if (pa->atf_tput_based && enable) {
		atf_err("can't enable maxclients as "
				"tput based atf is enabled\n");
		return -1;
	}
	pa->atf_maxclient = !!enable;
	if (pa->atf_maxclient) {
		if (pa->atf_num_clients != IEEE80211_128_AID) {
			numclients = IEEE80211_128_AID;
			reset = 1;
		}
	} else {
		if (pa->atf_commit
		&& (pa->atf_num_clients != ATF_MAX_AID_DEF)) {
			/* When ATF is enabled, set ATF_MAX_AID_DEF */
			numclients = ATF_MAX_AID_DEF;
			reset = 1;
		} else if (!pa->atf_commit
		&& (pa->atf_num_clients != IEEE80211_512_AID)) {
			/* When ATF is disabled, set IEEE80211_128_AID */
			numclients = IEEE80211_128_AID;
			reset = 1;
		}
	}
	if (reset)
		atf_update_number_of_clients(pdev, numclients);

	return 0;
}

static int32_t atf_get_maxclient_da(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa = NULL;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return -1;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev atf object is NULL\n");
		return -1;
	}

	return pa->atf_maxclient;
}

static void atf_set_group_unused_airtime_da(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	qdf_list_node_t *node = NULL;
	qdf_list_node_t *next_node = NULL;

	qdf_list_peek_front(&pa->atf_groups, &node);
	while(node) {
		group = qdf_container_of(node, struct atf_group_list,
								group_next);
		group->group_unused_airtime = 0;
		next_node = NULL;
		if (QDF_STATUS_SUCCESS !=
		qdf_list_peek_next(&pa->atf_groups, node, &next_node))
			break;
		node = next_node;
	}
}

static void atf_update_group_tbl_da(struct pdev_atf *pa, uint8_t stacnt,
				    uint8_t index, uint32_t vdev_per_unit)
{
	uint8_t peer_flg = 0, j = 0;
	uint32_t group_index = 0;
	struct atf_group_list *group = NULL;
	qdf_list_node_t *node = NULL;
	struct atf_config *atfcfg = NULL;

	atfcfg = &pa->atfcfg_set;
	for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
		if (atfcfg->peer_id[j].index_vdev == (index + 1)) {
			peer_flg = 1;
			group_index = atfcfg->peer_id[j].index_group;
			if (group_index == 0xFF) {
				/* Point to the default group */
				node = NULL;
				qdf_list_peek_front(&pa->atf_groups, &node);
				if (node)
					group = qdf_container_of(node,
							struct atf_group_list,
							group_next);
			} else if (group_index <= ATF_ACTIVED_MAX_ATFGROUPS) {
				group =
				atfcfg->atfgroup[group_index - 1].grplist_entry;
			}
			/* SSID has one or more peers with peer based ATF
			 * configured unusedairtime = SSID airtime -
			 * peer airtime
			 */
			if ((!stacnt) && (group != NULL))
				group->group_unused_airtime += vdev_per_unit;
		}
	}
	/* If no peers in an SSID, add it's airtime to unused pool */
	if (peer_flg == 0) {
		/* Only default group in SSID based configuration
		 * Add unused airtime to the default group
		 */
		node = NULL;
		qdf_list_peek_front(&pa->atf_groups, &node);
		if (node)
			group = qdf_container_of(node, struct atf_group_list,
								group_next);
		if (group)
			group->group_unused_airtime +=
					atfcfg->vdev[index].vdev_cfg_value;
	}
}

static int32_t atf_update_tbl_to_fm_da(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct atf_config *cfg = NULL;
	struct peer_atf *pa_obj = NULL;
	struct pdev_atf *pa = NULL;
	uint8_t i = 0;
	uint8_t peer_atf_state_prev = 0;
	uint8_t atfstate_change = 0;

	psoc = wlan_pdev_get_psoc(pdev);
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if ((NULL == pa) || (NULL == psoc))
		return -1;

	cfg = &pa->atfcfg_set;
	if (cfg->peer_num_cal == 0)
		return -1;

	/* For each entry in atfcfg structure,
	 * find corresponding entry in the node table
	 */
	for (i = 0; i < ATF_ACTIVED_MAX_CLIENTS; i++) {
		if ((cfg->peer_id[i].index_vdev != 0) &&
		    (cfg->peer_id[i].sta_assoc_status == 1)) {
			peer = wlan_objmgr_get_peer_nolock(psoc,
						wlan_objmgr_pdev_get_pdev_id(
						pdev), cfg->peer_id[i].sta_mac,
						WLAN_ATF_ID);
			if (peer == NULL)
				continue;

			pa_obj = wlan_objmgr_peer_get_comp_private_obj(peer,
							WLAN_UMAC_COMP_ATF);
			if (pa_obj == NULL)
				continue;

			/* getting the previous node state */
			peer_atf_state_prev = pa_obj->atfcapable;
			atfstate_change = 0;
			/* Mark atf capable clients - if there is a
			 * corresponding VAP entry or STA based config
			 */
			if (pa->atf_maxclient) {
				if ((cfg->peer_id[i].index_vdev != 0xFF)
					|| (cfg->peer_id[i].cfg_flag != 0)) {
					pa_obj->atfcapable = 1;
				} else {
					pa_obj->atfcapable = 0;
				}
			} else {
				pa_obj->atfcapable = 1;
			}
			/* Node ATF state changed */
			if (peer_atf_state_prev != pa_obj->atfcapable)
				atfstate_change = 1;

			psoc->soc_cb.tx_ops.atf_tx_ops.atf_capable_peer(pdev,
				peer, pa_obj->atfcapable, atfstate_change);
			if (peer)
				wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
		}
	}

	return 0;
}

static void atf_initial_airtime_estimate(struct wlan_objmgr_psoc *psoc,
					 struct wlan_objmgr_peer *peer,
					 struct wlan_objmgr_pdev *pdev)
{
	struct peer_atf *pa_obj = NULL;
	struct pdev_atf *pa = NULL;
	struct atf_context *ac = NULL;
	struct wlan_lmac_if_atf_tx_ops *tx_ops = NULL;

	if (NULL == pdev) {
		atf_err("pdev object is null!\n");
		return;
	}
	if (NULL == psoc) {
		atf_err("psoc is null!\n");
		return;
	}
	if (NULL == peer) {
		atf_err("peer object is null!\n");
		return;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pa is null!\n");
		return;
	}
	pa_obj = wlan_objmgr_peer_get_comp_private_obj(peer,
						       WLAN_UMAC_COMP_ATF);
	if (NULL == pa_obj) {
		atf_err("Peer atf object is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("Peer atf context is NULL!\n");
		return;
	}

	tx_ops = &psoc->soc_cb.tx_ops.atf_tx_ops;
	if (pa_obj->atf_tput) {
		pa_obj->atf_airtime_new =
			tx_ops->atf_airtime_estimate(pdev, peer,
							pa_obj->atf_tput,
							&pa_obj->throughput);
		pa_obj->atf_airtime_new *= ATF_DENOMINATION / 100;
		if (pa->atf_airtime_override &&	(pa->atf_airtime_override <
				(ATF_DENOMINATION - ac->airtime_resv))) {
			pa_obj->atf_airtime_new = pa->atf_airtime_override;
		}
		ac->airtime_limit = pa_obj->atf_airtime_cap *
						(ATF_DENOMINATION / 100);
		if (ac->airtime_limit &&
		(pa_obj->atf_airtime_new > ac->airtime_limit)) {
			pa_obj->atf_airtime_new = ac->airtime_limit;
		}
		pa_obj->atf_airtime_subseq = 0;
		if (pa_obj->atf_airtime_new <= pa_obj->atf_airtime) {
			pa_obj->atf_airtime = pa_obj->atf_airtime_new;
			pa_obj->atf_airtime_more = 0;
		} else {
			pa_obj->atf_airtime_more = pa_obj->atf_airtime_new -
							pa_obj->atf_airtime;
		}
		ac->configured_airtime += pa_obj->atf_airtime;
	} else {
		pa_obj->throughput = 0;
	}
}

static void atf_deconfig_airtime(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_peer *peer,
				 struct wlan_objmgr_pdev *pdev)
{
	struct peer_atf *pa_obj = NULL;
	struct atf_context *ac = NULL;

	if (NULL == peer) {
		atf_err("Peer object is NULL!\n");
		return;
	}
	if (NULL == psoc) {
		atf_err("psoc object is NULL!\n");
		return;
	}
	pa_obj = wlan_objmgr_peer_get_comp_private_obj(peer,
						       WLAN_UMAC_COMP_ATF);
	if (NULL == pa_obj) {
		atf_err("Peer atf object is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("Peer atf context is NULL!\n");
		return;
	}

	/* Stop processing further */
	if (ac->configured_airtime <= (ATF_DENOMINATION - ac->airtime_resv))
		return;

	if (pa_obj->atf_tput) {
		ac->configured_airtime -= pa_obj->atf_airtime;
		pa_obj->atf_airtime = 0;
		pa_obj->atf_airtime_more = pa_obj->atf_airtime_new;
	}
}

static void atf_allot_subsequent_airtime(struct wlan_objmgr_psoc *psoc,
					 struct wlan_objmgr_peer *peer,
					 struct wlan_objmgr_pdev *pdev)
{
	struct peer_atf *pa_obj = NULL;
	struct atf_context *ac = NULL;

	if (NULL == peer) {
		atf_err("Peer object is NULL!\n");
		return;
	}
	if (NULL == psoc) {
		atf_err("Psoc object is NULL!\n");
		return;
	}
	pa_obj = wlan_objmgr_peer_get_comp_private_obj(peer,
							WLAN_UMAC_COMP_ATF);
	if (NULL == pa_obj) {
		atf_err("Peer atf object is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("Peer atf context is NULL!\n");
		return;
	}
	if (pa_obj->atf_tput) {
		pa_obj->atf_airtime_subseq = ac->airtime;
		ac->airtime += pa_obj->atf_airtime;
	}
}

static void atf_peer_list_iterater(struct wlan_objmgr_pdev *pdev,
				   void *object, void *arg)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;
	struct peer_atf *prv_pa = (struct peer_atf *)arg;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct peer_atf *pa = NULL;
	struct atf_context *ac = NULL;
	struct pdev_atf *pa_obj = NULL;

	if ((NULL == pdev) || (NULL == peer) || (NULL == prv_pa)) {
		atf_err("pdev or peer or peer atf objects are null!\n");
		return;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL!\n");
		return;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("Peer atf object is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("Peer atf context is NULL!\n");
		return;
	}
	pa_obj = wlan_objmgr_pdev_get_comp_private_obj(pdev,
							WLAN_UMAC_COMP_ATF);
	if (NULL == pa_obj) {
		atf_err("Pdev atf object is NULL!\n");
		return;
	}

	if (pa_obj->stop_looping == 1)
		return;

	if (prv_pa->atf_airtime_more
	< (ATF_DENOMINATION - ac->configured_airtime - ac->airtime_resv)) {
		/* If we have enough airtime, configure the current station */
		prv_pa->atf_airtime = prv_pa->atf_airtime_new;
		ac->configured_airtime += prv_pa->atf_airtime_more;
		prv_pa->atf_airtime_more = 0;
		pa_obj->stop_looping = 1;
		return;
	}
	if (pa == prv_pa) {
		/*Something is wrong, we should have got enough airtime by now*/
		ac->configured_airtime -= prv_pa->atf_airtime;
		prv_pa->atf_airtime = 0;
		prv_pa->atf_airtime_more = prv_pa->atf_airtime_new;
		pa_obj->stop_looping = 1;
		return;
	}
	/*Try to deconfigure a newer station*/
	if (pa->atf_tput) {
		ac->airtime += pa->atf_airtime;
		ac->configured_airtime -= pa->atf_airtime;
		pa->atf_airtime = 0;
		pa->atf_airtime_more = pa->atf_airtime_new;
	}
}

static void atf_allot_extra_airtime(struct wlan_objmgr_psoc *psoc,
				    struct wlan_objmgr_peer *peer,
				    struct wlan_objmgr_pdev *pdev)
{
	struct peer_atf *pa_obj = NULL;
	struct atf_context *ac = NULL;
	struct pdev_atf *pa = NULL;

	if (NULL == peer) {
		atf_err("Peer object is NULL!\n");
		return;
	}
	if (NULL == psoc) {
		atf_err("Peer atf object is NULL!\n");
		return;
	}
	if (NULL == pdev) {
		atf_err("Pdev object is NULL!\n");
		return;
	}
	pa_obj = wlan_objmgr_peer_get_comp_private_obj(peer,
						       WLAN_UMAC_COMP_ATF);
	if (NULL == pa_obj) {
		atf_err("Peer atf object is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("Peer atf context is NULL!\n");
		return;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("Pdev atf object is NULL!\n");
		return;
	}

	if (pa_obj->atf_airtime_more) {
		if (pa_obj->atf_airtime_more < (ATF_DENOMINATION -
				ac->configured_airtime - ac->airtime_resv)) {
			/*
			 * There is enough unconfigured airtime to meet this
			 * extra requirement. So, we configure this station
			 * with it's new requirement.
			 */
			pa_obj->atf_airtime = pa_obj->atf_airtime_new;
			ac->configured_airtime += pa_obj->atf_airtime_more;
			pa_obj->atf_airtime_more = 0;
		} else {
			/* Adjust subsequent configured airtime for newer
			 * deconfigured stations
			 */
			pa_obj->atf_airtime_subseq -=
				(ac->airtime > pa_obj->atf_airtime_subseq ?
				pa_obj->atf_airtime_subseq : ac->airtime);
			if (pa_obj->atf_airtime_subseq <
						pa_obj->atf_airtime_more) {
				/*
				 * There isn't enough unconfigured airtime
				 * but at the same time deconfiguring newer
				 * stations won't help either. So, we just
				 * deconfigure the current station.
				 */
				ac->configured_airtime -= pa_obj->atf_airtime;
				pa_obj->atf_airtime = 0;
				pa_obj->atf_airtime_more =
							pa_obj->atf_airtime_new;
			} else {
				pa->stop_looping = 0;
				/* TODO: Traverse in reverse
				 * There isn't enough unconfigured airtime but
				 * we can deconfigure some newer stations to
				 * meet this station's extra requirement. So, we
				 * go from the newest station to the current
				 * station, deconfiguring newer stations one by
				 * one, till the current station's extra
				 * requirement can be met.
				 */
				wlan_objmgr_pdev_iterate_obj_list(pdev,
								WLAN_PEER_OP,
							atf_peer_list_iterater,
							pa_obj, 1, WLAN_ATF_ID);
			}
		}
	}
	if (WLAN_CONNECTED_STATE == wlan_peer_mlme_get_state(peer)) {
		ac->associated_sta++;
		if (pa_obj->atf_airtime)
			ac->configured_sta++;
	}
}

static void atf_notify_airtime(struct wlan_objmgr_vdev *vdev,
			       uint8_t *macaddr, uint8_t config)
{
	osif_dev *osifp = NULL;
	struct net_device *dev = NULL;
	union iwreq_data wreq;
	struct event_data_atf_config ev;

	if (vdev) {
		osifp = (osif_dev *)wlan_vdev_get_ospriv(vdev);
		if (osifp)
			dev = osifp->netdev;
	}

	if (dev) {
		ev.config = config;
		qdf_mem_copy(ev.macaddr, macaddr, QDF_MAC_ADDR_SIZE);
		qdf_mem_zero(&wreq, sizeof(wreq));
		wreq.data.flags = IEEE80211_EV_ATF_CONFIG;
		wreq.data.length = sizeof(struct event_data_atf_config);
		WIRELESS_SEND_EVENT(dev, IWEVCUSTOM, &wreq, (void *)&ev);
	}
}

static void atf_notify_configured_airtime(struct wlan_objmgr_psoc *psoc,
					  struct wlan_objmgr_peer *peer,
					  struct wlan_objmgr_pdev *pdev)
{
	struct peer_atf *pa_obj = NULL;
	struct pdev_atf *pa = NULL;
	struct atf_context *ac = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL;

	pa_obj = wlan_objmgr_peer_get_comp_private_obj(peer,
						       WLAN_UMAC_COMP_ATF);
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						   WLAN_UMAC_COMP_ATF);
	vdev = wlan_peer_get_vdev(peer);
	if (NULL == pa_obj) {
		atf_err("Peer atf object is NULL!\n");
		return;
	}
	if (NULL == pa) {
		atf_err("Pdev atf object is NULL!\n");
		return;
	}
	if (NULL == vdev) {
		atf_err("VDEV  is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("Peer atf context is NULL!\n");
		return;
	}
	if (pa_obj->atf_tput && pa_obj->atf_airtime) {
		group = pa_obj->atf_group;
		subgroup = pa_obj->ac_list_ptr[0];
		if ((NULL == group) || (NULL == subgroup)) {
			return;
		}
		if (subgroup->sg_type != ATF_CFGTYPE_PEER) {
			qdf_list_peek_front(&pa->atf_groups, &node);
			if (node) {
				group = qdf_container_of(node,
					  struct atf_group_list, group_next);
				atf_assign_subgroup(pa, pa_obj, group, 0,
						    ATF_CFGTYPE_PEER);
				subgroup = pa_obj->ac_list_ptr[0];
			}
		}
		if (subgroup) {
			subgroup->sg_atf_units = pa_obj->atf_airtime;
		}
		if (!pa_obj->atf_airtime_configured) {
			atf_notify_airtime(vdev, wlan_peer_get_macaddr(peer),
									1);
		}
		pa_obj->atf_airtime_configured = 1;
	} else if (ac->associated_sta > ac->configured_sta) {
		group = pa_obj->atf_group;
		subgroup = pa_obj->ac_list_ptr[0];
		if ((NULL == group) || (NULL ==subgroup)) {
			return;
		}
		/* Point unconfigured clients to default subgroup */
		if (!atf_config_is_default_subgroup(subgroup->sg_name) ||
		    !atf_config_is_default_group(group->group_name)) {
			qdf_list_peek_front(&pa->atf_groups, &node);
			if (node) {
				group = qdf_container_of(node,
					   struct atf_group_list, group_next);
				atf_assign_subgroup(pa, pa_obj, group, 0,
						    ATF_CFGTYPE_SSID);
				subgroup = pa_obj->ac_list_ptr[0];
			}
		}
		if (subgroup) {
			subgroup ->sg_atf_units =
				(ATF_DENOMINATION - ac->configured_airtime);
		}
		if (pa_obj->atf_airtime_configured) {
			atf_notify_airtime(vdev, wlan_peer_get_macaddr(peer),
									0);
		}
		pa_obj->atf_airtime_configured = 0;
	} else {
		group = pa_obj->atf_group;
		subgroup = pa_obj->ac_list_ptr[0];
		if ((NULL == group) || (NULL == subgroup)) {
			return;
		}
		/* Point unconfigured clients to default subgroup */
		if (!atf_config_is_default_subgroup(subgroup->sg_name) ||
			!atf_config_is_default_group(group->group_name)) {
			qdf_list_peek_front(&pa->atf_groups, &node);
			if (node) {
				group = qdf_container_of(node,
					   struct atf_group_list, group_next);
				atf_assign_subgroup(pa, pa_obj, group, 0,
						    ATF_CFGTYPE_SSID);
				subgroup = pa_obj->ac_list_ptr[0];
			}
		}
		if (subgroup) {
			subgroup->sg_atf_units = 0;
		}
		if (pa_obj->atf_airtime_configured) {
			atf_notify_airtime(vdev, wlan_peer_get_macaddr(peer),
									0);
		}
		pa_obj->atf_airtime_configured = 0;
	}
}

static void atf_iterate_peer_list_handler(struct wlan_objmgr_pdev *pdev,
					  void *object, void *arg)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)arg;
	struct atf_context *ac;

	if ((NULL == peer) || (NULL == pdev) || (NULL == psoc)) {
		atf_err("Peer or pdev or psoc objects are null!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("Peer atf context is NULL!\n");
		return;
	}
	switch (ac->atf_dist_state) {
	case ATF_AIRTIME_ESTIMATE:
		atf_initial_airtime_estimate(psoc, peer, pdev);
		break;
	case ATF_AIRTIME_DECONFIG:
		atf_deconfig_airtime(psoc, peer, pdev);
		break;
	case ATF_AIRTIME_SUBSEQUENT:
		atf_allot_subsequent_airtime(psoc, peer, pdev);
		break;
	case ATF_AIRTIME_EXTRA:
		atf_allot_extra_airtime(psoc, peer, pdev);
		break;
	case ATF_AIRTIME_NOTIFY:
		atf_notify_configured_airtime(psoc, peer, pdev);
		break;
	default:
		atf_err("Invalid state %d\n", ac->atf_dist_state);
	}
}

static void atf_distribute_airtime(struct pdev_atf *pa)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct atf_context *ac = NULL;

	if (NULL == pa) {
		atf_err("pa is null!\n");
		return;
	}
	pdev = pa->pdev_obj;
	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("Peer atf context is NULL!\n");
		return;
	}
	ac->airtime_resv = pa->atf_resv_airtime * (ATF_DENOMINATION / 100);

	ac->configured_airtime = 0;
	ac->airtime = 0;
	ac->associated_sta = 0;
	ac->airtime_limit = 0;
	ac->airtime_resv = 0;
	ac->configured_sta = 0;
	/*
	 * For each station having a throughput requirement, we find
	 * out the airtime needed. If the new airtime needed is less than
	 * or equal to what was previously needed, we configure the airtime
	 * now itself. Else we record how much extra airtime is needed. Also,
	 * we keep track of the total allotted airtime.
	 */
	ac->atf_dist_state = ATF_AIRTIME_ESTIMATE;
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
			atf_iterate_peer_list_handler, psoc, 1, WLAN_ATF_ID);

	/* Something wrong, TODO: Traverse reverse
	 * as we only gave lesser airtimes compared to prev cycle
	 */
	if (ac->configured_airtime > (ATF_DENOMINATION - ac->airtime_resv)) {
		ac->atf_dist_state = ATF_AIRTIME_DECONFIG;
		wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
						atf_iterate_peer_list_handler,
							psoc, 1, WLAN_ATF_ID);
	}
	/* TODO: Traverse reverse
	 * For each station having a throughput requirement, we find out the
	 * sum of airtimes allotted to stations newer than that station.
	 */
	ac->airtime = 0;
	ac->atf_dist_state = ATF_AIRTIME_SUBSEQUENT;
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
						atf_iterate_peer_list_handler,
							psoc, 1, WLAN_ATF_ID);
	/*
	 * Starting with oldest station, go one by one and check which all have
	 * extra airtime requirement. Either we give the extra airtime needed
	 * or deconfigure the station.
	 */
	ac->airtime = 0;
	ac->associated_sta = 0;
	ac->configured_sta = 0;
	ac->atf_dist_state = ATF_AIRTIME_EXTRA;
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
						atf_iterate_peer_list_handler,
							psoc, 1, WLAN_ATF_ID);
	ac->atf_dist_state = ATF_AIRTIME_NOTIFY;
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
						atf_iterate_peer_list_handler,
							psoc, 1, WLAN_ATF_ID);
}

/**
 * @brief Resets variables used for ATF buffer accounting
 *
 * @param [in] Reference to pdev ATF object
 *        [in] subgroup pointer to the subgroup
 *
 * @return None
 */
void atf_reset_buf_account(struct pdev_atf *pa,
			   struct atf_subgroup_list *subgroup)
{
	if (NULL == subgroup) {
		atf_err("Subgroup is NULL \n");
		return;
	}
	if (NULL == pa) {
		atf_err("Pdev ATF object is NULL\n");
		return;
	}
	subgroup->sg_min_num_buf_held = subgroup->sg_num_buf_held;
	subgroup->sg_max_num_buf_held = subgroup->sg_num_buf_held;
	subgroup->sg_pkt_drop_nobuf = 0;
	subgroup->sg_allowed_buf_updated = 0;
	subgroup->sg_num_bufs_sent = 0;
	subgroup->sg_num_bytes_sent = 0;

	if (ATF_IS_SG_ACTIVE(subgroup)) {
		if (subgroup->sg_num_buf_held > pa->atf_txbuf_min) {
			subgroup->sg_allowed_bufs = subgroup->sg_num_buf_held;
		} else {
			subgroup->sg_allowed_bufs = pa->atf_txbuf_min;
		}
	} else {
		subgroup->sg_allowed_bufs = 0;
	}
}

/**
 * atf_compute_actual_tokens
 * Deduct any excess tokens consumed in previous cycle
 * @pa       : Reference to Pdev Atf Object
 * @subgroup : Reference to subgroup
 *
 * Return: QDF_STATUS_SUCCES on success
 *         QDF_STATUS_E_INVAL on error
 */
QDF_STATUS
atf_compute_actual_tokens(struct pdev_atf *pa,
			  struct atf_subgroup_list *subgroup)
{
	if (NULL == subgroup) {
		atf_err("Subgroup is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	atf_reset_buf_account(pa, subgroup);
	if (subgroup->tokens_excess_consumed) {
		/* deduct any excess tokens consumed in the previous cycle */
		if(subgroup->tx_tokens >= subgroup->tokens_excess_consumed)
			subgroup->tx_tokens -= subgroup->tokens_excess_consumed;
		else
			subgroup->tx_tokens = 0;

		subgroup->tokens_excess_consumed = 0;
	}

	if (subgroup->tokens_less_consumed) {
		/* estimation < actual. Add the difference */
		subgroup->tx_tokens += subgroup->tokens_less_consumed;
		subgroup->tokens_less_consumed = 0;
	}
	subgroup->sg_act_tx_tokens = subgroup->tx_tokens;

	return QDF_STATUS_SUCCESS;
}

/**
 * @brief Derive txtokens based on the airtime assigned for the node.
 *
 * @param [in] pa Reference Pdev ATF object
 *        [in] airtime
 *        [in] token distribution timer interval.
 *
 * @return Tokens corresponding to the airtime
 */
static uint32_t atf_compute_txtokens(struct pdev_atf *pa, uint32_t atf_units,
				     uint32_t token_interval_ms)
{
	uint32_t tx_tokens;

	if (!atf_units)
		return 0;

	if (NULL == pa) {
		atf_err("pa is null!\n");
		return 0;
	}

	if (pa->atf_sched & ATF_SCHED_OBSS) {
		/* If OBSS scheduling is enabled,
		 * use the actual availabe tokens
		 */
		token_interval_ms = pa->atf_avail_tokens;
	}
	/*
	 * If token interval is 1 sec & atf_units assigned is 100 %,
	 * tx_tokens = 1000000
	 * Convert total token time to uses.
	 */
	tx_tokens = token_interval_ms * 1000;
	/*
	 * Derive tx_tokens for this peer, w.r.t. ATF denomination and
	 * scheduler token_units
	 */
	tx_tokens = (atf_units * tx_tokens) / ATF_DENOMINATION;

	return tx_tokens;
}

static uint32_t atf_get_available_tokens(struct pdev_atf *pa)
{
	uint8_t ctlrxc, extrxc, rfcnt, tfcnt, obss;
	uint32_t avail = ATF_TOKEN_INTVL_MS;
	struct wlan_objmgr_pdev *pdev = NULL;

	if (NULL == pa) {
		atf_err("pa is null!\n");
		return 0;
	}
	pdev = pa->pdev_obj;
	if (NULL == pdev) {
		atf_err("pdev is null!\n");
		return 0;
	}
	/* get individual percentages */
	ctlrxc = pa->atf_chbusy & 0xff;
	extrxc = (pa->atf_chbusy & 0xff00) >> 8;
	rfcnt = (pa->atf_chbusy & 0xff0000) >> 16;
	tfcnt = (pa->atf_chbusy & 0xff000000) >> 24;

	if ((ctlrxc == 255) || (extrxc == 255) ||
	    (rfcnt == 255) || (tfcnt == 255))
		return pa->atf_avail_tokens;

	/* Get curchan ic_flags */
	if (wlan_objmgr_get_current_channel_flag(pdev) & IEEE80211_CHAN_HT20)
		obss = ctlrxc - tfcnt;
	else
		obss = (ctlrxc + extrxc) - tfcnt;

	/* availabe % is 100 minus obss usage */
	avail = (100 - obss);
	/* Add a scaling factor and calculate the tokens*/
	if (pa->atf_obss_scale) {
		avail += avail * pa->atf_obss_scale / 100;
		avail = (avail * ATF_TOKEN_INTVL_MS / 100);
	} else {
		avail = (avail * ATF_TOKEN_INTVL_MS / 100) + 15;
	}
	/* Keep a min of 30 tokens */
	if (avail < 30)
		avail = 30;

	return (avail < ATF_TOKEN_INTVL_MS) ? avail : ATF_TOKEN_INTVL_MS;
}

/**
 * @brief Reset pa & group list variables each timer cycle
 *
 * @param [in] pa pointer to radio structure
 *
 * @return None
 */
void atf_init_pdev_group_var(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pa) {
		atf_err("Pdev atf is NULL. Return \n");
		return;
	}

	pa->atf_groups_borrow = 0;
	pa->atf_total_num_clients_borrow = 0;
	pa->atf_total_contributable_tokens = 0;
	pa->atf_num_active_sg = 0;
	pa->atf_num_active_sg_strict = 0;
	pa->atf_num_active_sg_fair_ub = 0;
	pa->atf_num_sg_borrow_strict = 0;
	pa->atf_num_sg_borrow_fair_ub = 0;

	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node,
					 struct atf_group_list, group_next);
		next_node = NULL;
		if (group) {
			group->shadow_atf_contributabletokens =
						group->atf_contributabletokens;
			group->atf_num_sg_borrow = 0;
			group->atf_contributabletokens = 0;
			group->atf_num_active_sg = 0;
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode,
						struct atf_subgroup_list,
						sg_next);
				tmpnode_next = NULL;
				ATF_RESET_SG_ACTIVE(subgroup);
				qdf_list_peek_next(&group->atf_subgroups,
						   tmpnode, &tmpnode_next);
				tmpnode = tmpnode_next;
			}
		}
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		node = next_node;
	}
}

/**
 * @brief Return the unassigned airtime
 * The airtime assigned to default subgroup is unassigned airtime
 * convert unalloted atf_units to tokens and add to the
 * contributable token pool
 *
 * @param [in] the handle to the radio
 *
 * @return Unalloted tokens
 */
int32_t atf_airtime_unassigned(struct pdev_atf *pa)
{
	uint32_t airtime_unassigned = 0;
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *tnode = NULL, *tmpnode = NULL;

	if (NULL == pa) {
		atf_err("Pdev atf is NULL. Return \n");
		return -1;
	}

	if (!qdf_list_empty(&pa->atf_groups)) {
		qdf_list_peek_front(&pa->atf_groups, &tnode);
		if (!tnode) {
			atf_err("group node is NULL. Return \n");
			return -1;
		}
		group = qdf_container_of(tnode, struct atf_group_list,
					 group_next);
		if (group) {
			if (!qdf_list_empty(&group->atf_subgroups)) {
				qdf_list_peek_front(&group->atf_subgroups,
						    &tmpnode);
				if (!tmpnode) {
					atf_err("group node is NULL\n");
					return -1;
				}
				subgroup = qdf_container_of(tmpnode,
						       struct atf_subgroup_list,
						       sg_next);
				if (subgroup) {
					airtime_unassigned =
						subgroup->sg_atf_units;
				}
			}
		}
	}

	return airtime_unassigned;
}

/**
 * @brief Get unused txtokens of each subgroup per timer tick
 *
 * @param [in] subgroup pointer to subgroup list
 *
 * @return unused tokens
 *         -EINVAL on error
 */
uint32_t atf_get_unused_txtokens(struct atf_subgroup_list *subgroup)
{
	uint32_t unusedtokens = 0;

	if (NULL == subgroup)
		return -EINVAL;

	qdf_spin_lock_bh(&subgroup->atf_sg_lock);
	unusedtokens = subgroup->tx_tokens;
	qdf_spin_unlock_bh(&subgroup->atf_sg_lock);

	return unusedtokens;
}

/**
 * @brief Compute unassigned tokens
 *
 * @param [in] pa  pointer to radio structure
 *
 * @return unassigned tokens on success
 *         -EINVAL on error
 */
int32_t atf_get_unassigned_tokens(struct pdev_atf *pa)
{
	uint32_t txtokens_unassigned = 0, airtime_unassigned = 0;

	if (!pa) {
		atf_err("pdev atf is null. Return \n");
		return -EINVAL;
	}

	if (!pa->atf_tput_based) {
		airtime_unassigned = atf_airtime_unassigned(pa);
		if (airtime_unassigned < 0) {
			atf_err("Error in reading unassigned airtime\n");
		}
		txtokens_unassigned = atf_compute_txtokens(pa,
							   airtime_unassigned,
							   ATF_TOKEN_INTVL_MS);
	}
	atf_info("airtime_unassigned : %d tokens : %d \n",
			 airtime_unassigned, txtokens_unassigned);

	return txtokens_unassigned;
}


/**
 * @brief Assign unassigned tokens to pa
 *
 * @param [in] pa  pointer to pa radio structure
 *
 * @return QDF_STATUS_SUCCESS on success
 *         QDF_STATUS_E_INVAL on error
 */
QDF_STATUS atf_manage_unassigned_tokens(struct wlan_objmgr_pdev *pdev)
{
	uint32_t txtokens_unassigned = 0;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct pdev_atf *pa = NULL;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return QDF_STATUS_E_INVAL;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("PDEV ATF object is NULL!\n");
		return QDF_STATUS_E_INVAL;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return QDF_STATUS_E_INVAL;
	}

	/* If max client support is enabled & if the total number of clients
	 * exceeds the number supported in ATF, do not contribute unalloted
	 * tokens.Unalloted tokens will be used by non-atf capable clients
	 */
	txtokens_unassigned = atf_get_unassigned_tokens(pa);
	if (pa->atf_maxclient) {
		psoc->soc_cb.tx_ops.atf_tx_ops.atf_tokens_unassigned(pdev,
							   txtokens_unassigned);
		/* With Maxclient feature enabled, unassigned tokens are used
		 * by non-atf clients. Hence, do not add unassigned tokens to
		 * node tokens */
		pa->atf_tokens_unassigned = 0;
	} else {
		/* Add unassigned tokens to the contributable token pool*/
		if (txtokens_unassigned) {
			txtokens_unassigned -= ((
					ATF_RESERVED_UNALLOTED_TOKEN_PERCENT *
					txtokens_unassigned) / 100);
		}
		/* Unassigned tokens will be added to node tokens */
		pa->atf_tokens_unassigned = txtokens_unassigned;
	}

	return QDF_STATUS_SUCCESS;
}

void atf_update_stats_tidstate(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is null. Return\n");
		return;
	}
	/* These stats are applicable only if there is one Node
	 * associated to a subgroup. If there are multiple nodes
	 * linked to the subgroup, this won't reflect the actual state
	 */
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		next_node = NULL;
		if (group) {
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode,
						       struct atf_subgroup_list,
						       sg_next);
				tmpnode_next = NULL;
				qdf_spin_lock_bh(&subgroup->atf_sg_lock);
				subgroup->sg_atf_stats.atftidstate =
						       subgroup->sg_atftidstate;
				qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
				qdf_list_peek_next(&group->atf_subgroups,
						   tmpnode, &tmpnode_next);
				tmpnode = tmpnode_next;
			}
		}
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		node = next_node;
	}
}

/**
 * @brief Updates atf statistics
 *
 * @param [in] pa  pointer to pa radio structure
 * @param [group] pa  pointer to group list
 * @param [subgroup] pa  pointer to subgroup list
 * @sg_unusedtokens Unused tokens in the previous iteration
 *
 * @return None
 */
void atf_update_stats(struct pdev_atf *pa, struct atf_group_list *group,
		      struct atf_subgroup_list *subgroup,
		      uint32_t sg_unusedtokens)
{
	if (NULL == pa) {
		atf_err("pdev atf is null\n");
		return;
	}
	if (NULL == group) {
		atf_err("Group is null\n");
		return;
	}
	if (NULL == subgroup) {
		atf_err("Subgroup is null\n");
		return;
	}

	/* Clear the stats structure before updating */
	qdf_mem_set((uint8_t *)&subgroup->sg_atf_stats,
		    sizeof(struct atf_stats), 0);

	/* Stats related to token */
	if (!group->group_sched)
		subgroup->sg_atf_stats.tot_contribution =
					group->shadow_atf_contributabletokens;
	else
		subgroup->sg_atf_stats.tot_contribution = 0;

	subgroup->sg_atf_stats.contribution = subgroup->sg_contributedtokens;
	subgroup->sg_atf_stats.contr_level = subgroup->sg_contribute_level;
	subgroup->sg_atf_stats.borrow = subgroup->sg_borrowedtokens;
	subgroup->sg_atf_stats.unused = sg_unusedtokens;
	subgroup->sg_atf_stats.tokens = subgroup->sg_shadow_tx_tokens;
	subgroup->sg_atf_stats.raw_tx_tokens = subgroup->sg_raw_tx_tokens;
	subgroup->sg_atf_stats.total = pa->shadow_alloted_tx_tokens;
	subgroup->sg_atf_stats.act_tokens = subgroup->sg_act_tx_tokens;
	subgroup->sg_atf_stats.timestamp = OS_GET_TIMESTAMP();

	/*buffer accouting stats */
	subgroup->sg_atf_stats.min_num_buf_held = subgroup->sg_min_num_buf_held;
	subgroup->sg_atf_stats.max_num_buf_held = subgroup->sg_max_num_buf_held;
	subgroup->sg_atf_stats.pkt_drop_nobuf = subgroup->sg_pkt_drop_nobuf;
	subgroup->sg_atf_stats.allowed_bufs = subgroup->sg_allowed_bufs;
	subgroup->sg_atf_stats.num_tx_bufs = subgroup->sg_num_bufs_sent;
	subgroup->sg_atf_stats.num_tx_bytes = subgroup->sg_num_bytes_sent;

	if ((subgroup->sg_atf_stats.act_tokens > sg_unusedtokens) &&
		(subgroup->sg_atf_stats.total > 0)) {
		/* Note the math: 200k tokens every 200 ms =>
		 * 1000k tokens / second => 1 token = 1 us
		 */
		subgroup->sg_atf_stats.total_used_tokens +=
					(subgroup->sg_atf_stats.act_tokens -
				 	 sg_unusedtokens);
		if (pa->atf_logging) {
			QDF_TRACE(QDF_MODULE_ID_ATF, QDF_TRACE_LEVEL_INFO,
				  "subgroup %s is currently using %d usecs \
				  which is %d%% of available airtime\n",
				  subgroup->sg_name,
				  (subgroup->sg_atf_stats.act_tokens -
				   sg_unusedtokens),
				  (((subgroup->sg_atf_stats.act_tokens -
				  sg_unusedtokens)*100) /
				   subgroup->sg_atf_stats.total));
		}
	}
	subgroup->sg_atf_stats.tokens_common = pa->txtokens_common;
	subgroup->sg_atf_stats.tokens_unassigned = pa->atf_tokens_unassigned;
	pa->atf_tokens_unassigned = 0;
}

void atf_stats_history(struct pdev_atf *pa)
{
	struct atf_subgroup_list *subgroup = NULL;
	struct atf_group_list *group = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is null. Return\n");
		return;
	}
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		next_node = NULL;
		if (group) {
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode,
						 struct atf_subgroup_list,
						 sg_next);
				tmpnode_next = NULL;
				qdf_spin_lock_bh(&subgroup->atf_sg_lock);
				if (subgroup->sg_atf_debug) {
					/* Make sure that the history buffer was
					 * not freed while taking the lock */
					if (subgroup->sg_atf_debug) {
						subgroup->sg_atf_debug[
						  subgroup->sg_atf_debug_id++] =
							subgroup->sg_atf_stats;
						subgroup->sg_atf_debug_id &=
						    subgroup->sg_atf_debug_mask;
					}
				}
				qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
				qdf_list_peek_next(&group->atf_subgroups,
						   tmpnode, &tmpnode_next);
				tmpnode = tmpnode_next;
			}
		}
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		node = next_node;
	}
}

/*
 * @brief Log atf data (initial few values, till ATF_DATA_LOG_SIZE);
 *
 * @param pa  pointer to radio structure
 *        subgroup pointer to subgroup list
 *
 * @return  0 on success
 *          -EINVAL on error
 */
QDF_STATUS atf_logdata_init(struct atf_subgroup_list *subgroup,
			    uint32_t sg_unusedtokens)
{
	if (NULL == subgroup) {
		atf_err("subgroup is NULL\n");
		return QDF_STATUS_E_INVAL;
	}

	if (subgroup->sg_atfindex) {
		if (!ATF_IS_SG_ACTIVE(subgroup) ||
			!subgroup->sg_shadow_tx_tokens) {
			subgroup->sg_unusedtokenpercent[subgroup->sg_atfindex
				                        - 1] = 100;
		} else if (sg_unusedtokens <= subgroup->sg_shadow_tx_tokens) {
			subgroup->sg_unusedtokenpercent[subgroup->sg_atfindex
				                        - 1] =
				((sg_unusedtokens * 100)/
				 	subgroup->sg_shadow_tx_tokens);
		} else {
			subgroup->sg_unusedtokenpercent[subgroup->sg_atfindex
							- 1] = 0;
		}
		atf_info("(sg %s) atfdata_logged : %d sg_atfindex : %d\
			 unused token per : %d\n", subgroup->sg_name,
			 subgroup->sg_atfdata_logged,
			 subgroup->sg_atfindex - 1,
			 subgroup->sg_unusedtokenpercent[subgroup->sg_atfindex
			 				 - 1]);
	}
	subgroup->sg_atfdata_logged++;
	subgroup->sg_atfindex++;

	if (subgroup->sg_atfindex >= ATF_DATA_LOG_SIZE) {
			atf_info("%s sg_atfindex > %d . reset to 0 \n\r",
				 subgroup->sg_name, subgroup->sg_atfindex);
		subgroup->sg_atfindex = 0;
	}
	subgroup->sg_atf_stats.weighted_unusedtokens_percent = 0;

	return QDF_STATUS_SUCCESS;
}

/**
 * @brief log unused tokens for each subgroup
 *
 * @param[in]   subgroup pointer to subgroup list
 * @param[in]   sg_unusedtokens unused tokens from previous cycle
 *
 * @return  Index to the ATF history array
 *          -EINVAL on error
 */
int32_t atf_subgroup_compute_usage(struct atf_subgroup_list *subgroup,
				   uint32_t sg_unusedtokens)
{
	int32_t sg_index = 0;

	if (NULL == subgroup) {
		atf_err("subgroup is NULL\n");
		return -EINVAL;
	}

	switch (subgroup->sg_atfindex) {
		case ATF_DATA_LOG_ZERO:
			sg_index = (ATF_DATA_LOG_SIZE - 1);
			break;
		case ATF_DATA_LOG_SIZE:
			sg_index = 0;
			break;
		default:
			sg_index = (subgroup->sg_atfindex - 1);
	}

	/* If subgroup had borrowed tokens in the previous iteration,
	 * Do not account for it while calculating unusedtoken percent
	 */
	if (!ATF_IS_SG_ACTIVE(subgroup) ||
		(!subgroup->sg_shadow_tx_tokens)) {
		subgroup->sg_unusedtokenpercent[sg_index] = 100;
	} else if (subgroup->sg_atfborrow) {
		if (sg_unusedtokens >= subgroup->sg_borrowedtokens) {
			sg_unusedtokens = sg_unusedtokens -
						subgroup->sg_borrowedtokens;
		} else {
			sg_unusedtokens = 0;
		}
		if (subgroup->sg_shadow_tx_tokens >=
		    subgroup->sg_borrowedtokens) {
			subgroup->sg_unusedtokenpercent[sg_index] =
						((sg_unusedtokens * 100) /
					 	(subgroup->sg_shadow_tx_tokens -
						 subgroup->sg_borrowedtokens));
		} else {
			subgroup->sg_unusedtokenpercent[sg_index] = 0;
		}
	} else {
      if (sg_unusedtokens <= subgroup->sg_shadow_tx_tokens) {
		subgroup->sg_unusedtokenpercent[sg_index] =
						((sg_unusedtokens * 100) /
						 subgroup->sg_shadow_tx_tokens);
		} else {
			subgroup->sg_unusedtokenpercent[sg_index] = 100;
		}
	}
	/* update unused token percent stat for the previous cycle */
	subgroup->sg_atf_stats.unusedtokens_percent =
			subgroup->sg_unusedtokenpercent[sg_index];
	atf_info("%s unused token percent [%d]: %d \n", subgroup->sg_name,
			  sg_index, subgroup->sg_unusedtokenpercent[sg_index]);

	return sg_index;
}

/**
 * @brief log usage
 *
 * @param[in]  subgroup pointer to subgroup list
 * @param[in]  unused tokens of subgroup
 *
 * @return  index to the ATF usage log
 *          -EINVAL on error
 */
int atf_log_usage(struct atf_subgroup_list *subgroup, uint32_t sg_unusedtokens)
{
	if (subgroup == NULL) {
		atf_err("subgroup is NULL\n");
		return -EINVAL;
	}

	if (subgroup->sg_atfdata_logged < ATF_DATA_LOG_SIZE) {
		/* log the initial values */
		atf_logdata_init(subgroup, sg_unusedtokens);
		return -EINVAL;
	}

	/* compute unused tokens of subgroup and log*/
	return atf_subgroup_compute_usage(subgroup, sg_unusedtokens);
}

/**
 * @brief compute weighted unused percentage
 * Compute weighted unused percentage. The recent entries
 * are given more weight than older entries
 *
 * @param :     pa pointer to radio structure
 * @subgroup :  subgroup pointer to subgroup list
 * @sg_index :  index to the Log
 *
 * @return Weighted unused token percent of subgroup
 */
int atf_compute_weighted_tokens(struct atf_subgroup_list *subgroup,
				int32_t sg_index)
{
	int32_t j = 0, i = 0;
	uint32_t weighted_unusedtokens_percent = 0;
	int32_t unusedairtime_weights[ATF_DATA_LOG_SIZE] = {60, 30, 10};

	if (NULL == subgroup) {
		atf_err("subgroup is NULL\n");
		return -1;
	}

	for (j = sg_index, i =0 ; i < ATF_DATA_LOG_SIZE; i++) {
		weighted_unusedtokens_percent +=
					((subgroup->sg_unusedtokenpercent[j] *
					 unusedairtime_weights[i]) / 100);
		atf_info("i: %dindex: %dweight: %d , unusedtokenpercent: %d \
			 weighted_cal: %d \n\r", i, j, unusedairtime_weights[i],
			 subgroup->sg_unusedtokenpercent[j],
			 weighted_unusedtokens_percent);
		j++;
		if (j == ATF_DATA_LOG_SIZE) {
			j = 0;
		}
	}
	/* Weighted unused token percent stat for the previous cycle */
	subgroup->sg_atf_stats.weighted_unusedtokens_percent =
						weighted_unusedtokens_percent;

	return weighted_unusedtokens_percent;
}

QDF_STATUS atf_compute_tokens_contribute(struct pdev_atf *pa,
					 struct atf_group_list *group,
			 	  	 struct atf_subgroup_list *subgroup,
				  	 uint32_t wt_unusedtokens_per)
{
	uint32_t compensation = 0;
	int32_t reserve_percent[ATF_MAX_CONTRIBUTE_LEVEL + 1] =
							{75, 50, 20, 5, 1};
	uint32_t contribute_level = 0, contribute_percent = 0;

	if (NULL == subgroup) {
		atf_err("subgroup is NULL \n");
		return QDF_STATUS_E_INVAL;
	}
	if (NULL == group) {
		atf_err("group is NULL \n");
		return QDF_STATUS_E_INVAL;
	}
	if (subgroup->sg_atfdata_logged < ATF_DATA_LOG_SIZE) {
		return QDF_STATUS_SUCCESS;
	}
	if (!subgroup->sg_shadow_tx_tokens) {
		return QDF_STATUS_SUCCESS;
	}
	/* subgroup with unusedtokens greater than a threshold can contribute */
	if (wt_unusedtokens_per > ATF_UNUSEDTOKENS_CONTRIBUTE_THRESHOLD) {
		/* Compute the node tokens that can be contributed and
		 * deduct it from node tokens */
		subgroup->sg_atfborrow = subgroup->sg_borrowedtokens = 0;
		/* tx_token will be zero until atfcfg_timer updates atf_units */
		if (subgroup->sg_shadow_tx_tokens) {
			contribute_level = subgroup->sg_contribute_level;
			contribute_percent = wt_unusedtokens_per -
				((reserve_percent[contribute_level] *
				  wt_unusedtokens_per) / 100);
			subgroup->sg_contributedtokens = (contribute_percent *
					   subgroup->sg_shadow_tx_tokens) / 100;
			if (subgroup->sg_contributedtokens <=
			    subgroup->sg_shadow_tx_tokens) {
				subgroup->sg_shadow_tx_tokens -=
					subgroup->sg_contributedtokens;
			} else {
				atf_err("%s CHECK!!! sg_contributed tokens %d >\
					shadow_tx_tokens : %d \n", __func__,
					subgroup->sg_contributedtokens,
					subgroup->sg_shadow_tx_tokens);
				subgroup->sg_contributedtokens = 0;
			}

			/* set a lower threshold for ni->tx_tokens */
			if (subgroup->sg_shadow_tx_tokens <
			    (ATF_RESERVERD_TOKEN_PERCENT *
			    ATF_TOKEN_INTVL_MS * 10) &&
			    subgroup->sg_num_buf_held &&
			    ATF_IS_SG_ACTIVE(subgroup)) {
				compensation =
					(ATF_RESERVERD_TOKEN_PERCENT *
					 ATF_TOKEN_INTVL_MS * 10) -
					 subgroup->sg_shadow_tx_tokens;
				/* can compensate back upto a max of what the
				 * subgroup was contributing */
				if (compensation >
				    subgroup->sg_contributedtokens) {
					compensation =
						subgroup->sg_contributedtokens;
				}
				subgroup->sg_shadow_tx_tokens += compensation;
				subgroup->sg_contributedtokens -= compensation;
			}

			/* Set the level of contribution
			 * Increment contribution level every iteration count
			 */
			subgroup->sg_contribute_id++;
			subgroup->sg_contribute_id &= (pa->atf_contr_level_incr
							- 1);
			if (!ATF_IS_SG_ACTIVE(subgroup)) {
				subgroup->sg_contribute_level =
						ATF_MAX_CONTRIBUTE_LEVEL;
			} else if (!subgroup->sg_contribute_id &&
				   subgroup->sg_contribute_level <
				   ATF_MAX_CONTRIBUTE_LEVEL) {
				subgroup->sg_contribute_level++;
			}
		} else {
			subgroup->sg_contributedtokens =
					subgroup->sg_shadow_tx_tokens = 0;
			subgroup->sg_contribute_id = 0;
			subgroup->sg_contribute_level = 0;
		}
		group->atf_contributabletokens +=
					subgroup->sg_contributedtokens;
		atf_info("%sTokensto contribute: %dtotalcontributabletokens: %d\
			 tx_tokens : %d\n\r", subgroup->sg_name,
			 subgroup->sg_contributedtokens,
			 group->atf_contributabletokens,
			 subgroup->sg_shadow_tx_tokens);
	} else {
		/* If unused tokens percentage is less than a min threshold
		 * set borrow flag */
		subgroup->sg_atfborrow = 1;
		subgroup->sg_contributedtokens = 0;
		group->atf_num_sg_borrow++;
		atf_info("subgroup: %s, borrow enabled! atf_num_sg_borrow : %d \
			 tx_tokens : %d \n\r", subgroup->sg_name,
			 group->atf_num_sg_borrow,
			 subgroup->sg_shadow_tx_tokens);
		/* reset the contribution level */
		subgroup->sg_contribute_id = 0;
		subgroup->sg_contribute_level = 0;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS atf_strictq_algo(struct wlan_objmgr_pdev *pdev)
{
	struct atf_subgroup_list *subgroup = NULL;
	struct atf_group_list *group = NULL;
	struct pdev_atf *pa = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pdev) {
		atf_err("PDEV object is NULL!\n");
		return QDF_STATUS_E_INVAL;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("PDEV ATF object is NULL!\n");
		return QDF_STATUS_E_INVAL;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return QDF_STATUS_E_INVAL;
	}

	pa->alloted_tx_tokens = 0;
	atf_manage_unassigned_tokens(pdev);
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		next_node = NULL;
		if (group) {
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode,
						       struct atf_subgroup_list,
						       sg_next);
				tmpnode_next = NULL;
				atf_update_stats(pa, group, subgroup,
						 subgroup->tx_tokens);
				subgroup->tx_tokens = atf_compute_txtokens(pa,
							subgroup->sg_atf_units,
							ATF_TOKEN_INTVL_MS);
				subgroup->sg_raw_tx_tokens =
							subgroup->tx_tokens;
				subgroup->sg_shadow_tx_tokens =
							subgroup->tx_tokens;
				pa->alloted_tx_tokens += subgroup->tx_tokens;
				atf_compute_actual_tokens(pa, subgroup);
				qdf_list_peek_next(&group->atf_subgroups,
						   tmpnode, &tmpnode_next);
				tmpnode = tmpnode_next;
			}
		}
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		node = next_node;
	}
	if (pa->atf_maxclient) {
		pa->alloted_tx_tokens += pa->atf_tokens_unassigned;
		pa->txtokens_common = pa->atf_tokens_unassigned;
	} else
		pa->txtokens_common = 0;
	pa->shadow_alloted_tx_tokens = pa->alloted_tx_tokens;

	/* Iterate Node table & unblock all nodes at every timer tick */
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
					  atf_peer_resume_iterater, psoc, 0,
					  WLAN_ATF_ID);
	/* Updated TIDstate stat */
	atf_update_stats_tidstate(pa);
	/* Update stats pointer */
	atf_stats_history(pa);

	return QDF_STATUS_SUCCESS;
}

/**
 * @brief Iterates through group & subgroup list
 *        Computes the contributable tokens in the common pool (across groups)
 *
 * @param [in] pa  pointer to radio structure
 *
 * @return QDF_STATUS_SUCCESS on success
 *         QDF_STATUS_E_INVAL on error
 */
QDF_STATUS atf_common_contribute_pool(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	uint32_t group_noclients_txtokens = 0;
	qdf_list_node_t *node = NULL;
	qdf_list_node_t *next_node = NULL;
	struct atf_config *cfg = &pa->atfcfg_set;

	if (NULL == pa) {
		atf_err("PDEV ATF object is NULL!\n");
		return QDF_STATUS_E_INVAL;
	}
	pa->atf_total_num_clients_borrow = 0;
	pa->atf_total_contributable_tokens = 0;
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		next_node = NULL;
		if (group && (group->group_sched != ATF_GROUP_SCHED_STRICT) &&
			cfg->grp_num_cfg) {
			/* If there aren't any clients in the group, add group's
			 * airtime to the common contributable pool
			 */
			if (!group->atf_num_active_sg) {
				group_noclients_txtokens = atf_compute_txtokens(
							   pa,
							   group->group_airtime,
							   ATF_TOKEN_INTVL_MS);
				pa->atf_total_contributable_tokens +=
						(group_noclients_txtokens -
						((ATF_RESERVERD_TOKEN_PERCENT *
					  	  group_noclients_txtokens) /
						  100));
				group->atf_contributabletokens = 0;
			}
			/* Total number of 'subgroups looking to borrow' per
			 * radio */
			pa->atf_total_num_clients_borrow +=
						group->atf_num_sg_borrow;
			/* If group sched is set to 'Fair with Upper Bound' &
			 * if there are subgroups looking to borrow, do not add
			 * contributable tokens in
			 * the common contributable pool
			 */
			if (!(group->group_sched == ATF_GROUP_SCHED_FAIR_UB &&
				group->atf_num_sg_borrow)) {
				pa->atf_total_contributable_tokens +=
						 group->atf_contributabletokens;
			}
			/* Update number of subgroups looking to borrow in
			 * STRICT & FAIR_UB groups */
			if (group->group_sched == ATF_GROUP_SCHED_STRICT) {
				pa->atf_num_sg_borrow_strict +=
						       group->atf_num_sg_borrow;
			} else if (group->group_sched ==
				   ATF_GROUP_SCHED_FAIR_UB) {
				pa->atf_num_sg_borrow_fair_ub +=
						       group->atf_num_sg_borrow;
			}
		}
		/* Total number of groups that have subgroups
		 * looking to borrow */
		if (group->atf_num_sg_borrow) {
			pa->atf_groups_borrow++;
		}
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		node = next_node;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * @brief Iterates through group & subgroup list
 * Identifies subgroups looking to borrow & contribute tokens
 * Computes total tokens available for contribution
 *
 * @param [in] pa  pointer to radio structure
 *
 * @return 0
 */
QDF_STATUS atf_fairq_compute_tokens(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	uint32_t sg_unused_tokens = 0;
	uint32_t wt_unusedtokens_per = 0;
	int32_t sg_index = 0;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pa) {
		atf_err("PDEV ATF object is NULL!\n");
		return QDF_STATUS_E_INVAL;
	}
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		next_node = NULL;
		if (group) {
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode,
						       struct atf_subgroup_list,
						       sg_next);
				tmpnode_next = NULL;

				sg_index = 0;
				/* unused tokens of the subgroup in the
				 * previous cycle
				 */
				sg_unused_tokens = atf_get_unused_txtokens(
								      subgroup);

				/* Update the statistics */
				atf_update_stats(pa, group, subgroup,
						 sg_unused_tokens);

				/* log subgroup usage */
				sg_index = atf_log_usage(subgroup,
						         sg_unused_tokens);
				if (sg_index >= 0) {
					wt_unusedtokens_per =
						atf_compute_weighted_tokens(
							subgroup, sg_index);
				}

				/* Compute tokens based on user allocation */
				subgroup->sg_shadow_tx_tokens =
						atf_compute_txtokens(pa,
							 subgroup->sg_atf_units,
							 ATF_TOKEN_INTVL_MS);
				atf_info("subgroup: %s,atf_units: %d \
					 shadow_tx_tokens: %dunused tokens : %d\
					 INTVL : %d \n", subgroup->sg_name,
					 subgroup->sg_atf_units,
					 subgroup->sg_shadow_tx_tokens,
					 sg_unused_tokens,
					 ATF_TOKEN_INTVL_MS);
				subgroup->sg_contributedtokens = 0;
				subgroup->sg_raw_tx_tokens =
						subgroup->sg_shadow_tx_tokens;
				if (sg_index >= 0) {
					atf_info("group: %s,Fairq enabled\n",
						  group->group_name);
					atf_compute_tokens_contribute(pa,
							group, subgroup,
							wt_unusedtokens_per);
				} else {
					/* Strict policy configured for SSID */
					atf_info("grp: %s,Database not built\n",
						 group->group_name);
					subgroup->sg_borrowedtokens = 0;
					subgroup->sg_atfborrow = 0;
				}

				/* Increment subgroup index */
				subgroup->sg_atfindex++;
				if(subgroup->sg_atfindex >= ATF_DATA_LOG_SIZE) {
					subgroup->sg_atfindex = 0;
				}
				qdf_list_peek_next(&group->atf_subgroups,
						   tmpnode, &tmpnode_next);
				tmpnode = tmpnode_next;
			}
		}
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		node = next_node;
	}

	return QDF_STATUS_SUCCESS;
}

int atf_intergroup_sched_fair_ub(struct pdev_atf *pa,
				 struct atf_group_list *group,
				 struct atf_subgroup_list *subgroup)
{
	uint32_t contrtokens_persg = 0;

	if (NULL == pa) {
		atf_err("Pdev atf is NULL\n");
		return -1;
	}
	if (NULL == group) {
		atf_err("group is NULL\n");
		return -1;
	}
	if (NULL == subgroup) {
		atf_err("Subgroup is NULL\n");
		return -1;
	}
	/* Handle unassigned token distribution ???*/
	if (subgroup->sg_atfborrow &&
		ATF_IS_SG_ACTIVE(subgroup)) {
		contrtokens_persg =
		    group->atf_contributabletokens / group->atf_num_sg_borrow;
		subgroup->sg_borrowedtokens = contrtokens_persg;
		subgroup->sg_shadow_tx_tokens += contrtokens_persg;
		if (qdf_container_of(&subgroup->sg_next,
				     struct atf_subgroup_list,
				     sg_next) == NULL) {
			group->atf_contributabletokens = 0;
		}
	}

	return 0;
}

int atf_intergroup_sched_strict(struct pdev_atf *pa,
				struct atf_group_list *group,
				struct atf_subgroup_list *subgroup)
{
	uint32_t contrtokens_persg = 0;

	if (NULL == pa) {
		atf_err("Pdev atf is NULL\n");
		return -1;
	}
	if (NULL == group) {
		atf_err("group is NULL\n");
		return -1;
	}
	if (NULL == subgroup) {
		atf_err("Subgroup is NULL\n");
		return -1;
	}
	/* Handle unassigned token distribution ???*/
	if (group->atf_contributabletokens) {
		contrtokens_persg = group->atf_contributabletokens /
							group->num_subgroup;
		subgroup->sg_shadow_tx_tokens += contrtokens_persg;
		subgroup->sg_borrowedtokens = 0;
		subgroup->sg_contributedtokens = 0;
		if (qdf_container_of(&subgroup->sg_next,
				     struct atf_subgroup_list,
				     sg_next) == NULL) {
			group->atf_contributabletokens = 0;
		}
	}

	return 0;
}

QDF_STATUS atf_intergroup_sched_fair(struct pdev_atf *pa,
			      	     struct atf_group_list *group,
			      	     struct atf_subgroup_list *subgroup)
{
	uint32_t active_sgs = 0, clients_borrow = 0;
	uint32_t tot_num_active_sg_strict = 0;
	uint32_t tot_num_sg_borrow_strict = 0;
	uint32_t contrtokens_persg = 0;

	/* Total number of subgroups assoicated with groups marked
	 * Strict or FAIR_UB sched policy */
	tot_num_sg_borrow_strict =
		pa->atf_num_sg_borrow_strict + pa->atf_num_sg_borrow_fair_ub;
	if (pa->atf_total_num_clients_borrow >= tot_num_sg_borrow_strict) {
		clients_borrow = pa->atf_total_num_clients_borrow -
						 tot_num_sg_borrow_strict;
	} else {
		atf_info("Check!!! num_borrow %d < atf_num_sg_borrow_strict %d \
			 + atf_num_sg_borrow_fair_ub %d \n",
			 pa->atf_total_num_clients_borrow,
			 pa->atf_num_sg_borrow_strict,
			 pa->atf_num_sg_borrow_fair_ub);
	}

	/* if there aren't any clients looking to borrow,
	 * Distribute unassigned tokens to all active subgroups equally */
	if (!clients_borrow &&
		ATF_IS_SG_ACTIVE(subgroup)) {
		tot_num_active_sg_strict =
		   pa->atf_num_active_sg_strict + pa->atf_num_active_sg_fair_ub;
		if (pa->atf_num_active_sg >= tot_num_active_sg_strict) {
		  active_sgs = pa->atf_num_active_sg - tot_num_active_sg_strict;
		} else {
			atf_info("Check!atf_num_active_sg %d < atf_num_active_sg_strict %d\
				 + atf_num_active_sg_fair_ub %d \n",
				 pa->atf_num_active_sg,
				 pa->atf_num_active_sg_strict,
				 pa->atf_num_active_sg_fair_ub);
		}
		if (!atf_config_is_default_subgroup(subgroup->sg_name)) {
			subgroup->sg_borrowedtokens =
				pa->atf_tokens_unassigned / active_sgs;
		} else {
			subgroup->sg_borrowedtokens = 0;
		}
		subgroup->sg_shadow_tx_tokens += subgroup->sg_borrowedtokens;
		contrtokens_persg =
			pa->atf_total_contributable_tokens / active_sgs;
		subgroup->sg_shadow_tx_tokens += contrtokens_persg;
		subgroup->sg_contributedtokens = 0;
		group->atf_contributabletokens = 0;
	} else if (subgroup->sg_atfborrow && ATF_IS_SG_ACTIVE(subgroup)) {
		/* For subgroups looking to borrow:
		 * Distribute any unassigned tokens (if any) equally
		 * Distribute tokens from global contributable pool equally
		 */
		if (!atf_config_is_default_subgroup(subgroup->sg_name)) {
			contrtokens_persg =
				(pa->atf_total_contributable_tokens +
				 pa->atf_tokens_unassigned) / clients_borrow;
		} else {
			contrtokens_persg =
			    pa->atf_total_contributable_tokens / clients_borrow;
		}
		subgroup->sg_borrowedtokens = contrtokens_persg;
		subgroup->sg_shadow_tx_tokens += contrtokens_persg;
	}

	return QDF_STATUS_SUCCESS;
}

void atf_compute_sg_tokens(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
				group_next);
		next_node = NULL;
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		if (group) {
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode,
						struct atf_subgroup_list,
						sg_next);
				tmpnode_next = NULL;
				qdf_list_peek_next(&group->atf_subgroups,
						   tmpnode, &tmpnode_next);
				if ((group->group_sched ==
				     ATF_GROUP_SCHED_STRICT)) {
				    atf_intergroup_sched_strict(pa, group,
						    		subgroup);
				}else if (group->group_sched ==
					  ATF_GROUP_SCHED_FAIR_UB) {
				    atf_intergroup_sched_fair_ub(pa, group,
						    		 subgroup);
				} else if (group->group_sched ==
					   ATF_GROUP_SCHED_FAIR) {
				    /* can contribute & borrow */
				    atf_intergroup_sched_fair(pa, group,
						   	      subgroup);
				} else {
					atf_info("unknown group sched policy");
				}
				tmpnode = tmpnode_next;
			}
		}
		node = next_node;
	}
}


/**
 * @brief Distributes tokens to subgroup
 *
 * @param [in] pa  pointer to radio structure
 *
 * @return 0
 */
int atf_distribute_tokens_subgroup(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	uint32_t txtokens_unassigned = 0;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pa) {
		atf_err("PDEV ATF Object is NULL");
		return -1;
	}
	pa->alloted_tx_tokens = 0;
	atf_compute_sg_tokens(pa);

	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
								 group_next);
		next_node = NULL;
		if (group) {
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode,
						struct atf_subgroup_list,
						sg_next);
				tmpnode_next = NULL;
				/* Scheduler works on tx_tokens.
				 * Assign tx_tokens
				 */
				qdf_spin_lock_bh(&subgroup->atf_sg_lock);
				subgroup->tx_tokens =
					subgroup->sg_shadow_tx_tokens;
				pa->alloted_tx_tokens += subgroup->tx_tokens;
				atf_compute_actual_tokens(pa, subgroup);
				qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
				qdf_list_peek_next(&group->atf_subgroups,
						   tmpnode, &tmpnode_next);
				tmpnode = tmpnode_next;
			}
		}
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		node = next_node;
	}
	if (pa->atf_maxclient) {
		txtokens_unassigned = atf_get_unassigned_tokens(pa);
		pa->alloted_tx_tokens += txtokens_unassigned;
		pa->txtokens_common = txtokens_unassigned;
	} else {
		pa->txtokens_common = 0;
	}
	pa->shadow_alloted_tx_tokens = pa->alloted_tx_tokens;

	return 0;
}

/**
 * @brief Iterates through the node table.
 *        Subgroups with atleast a node pointing to it
 *        will be marked active
 *
 * @param [in] pdev Pointer to the pdev object
 *        [in] object Pointer to the peer object
 *        [in] arg
 *
 * @return none
 */
void node_iter_mark_active_sg(struct wlan_objmgr_pdev *pdev, void *object,
			      void *arg)
{
	int8_t ac_id = 0;
	struct atf_subgroup_list *subgroup = NULL;
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;
	struct peer_atf *pa_obj = NULL;

	if (peer == NULL) {
		atf_err("Peer is NULL\n");
		return;
	}
	if (WLAN_PEER_AP == wlan_peer_get_peer_type(peer)) {
		return;
	}
	pa_obj = wlan_objmgr_peer_get_comp_private_obj(peer,
						       WLAN_UMAC_COMP_ATF);
	if (pa_obj == NULL) {
		atf_err("Peer ATF is NULL\n");
		return;
	}

	for (ac_id = 0; ac_id < WME_NUM_AC; ac_id++) {
		subgroup = pa_obj->ac_list_ptr[ac_id];
		if (subgroup) {
			ATF_SET_SG_ACTIVE(subgroup);
		}
	}
}

/**
 * @brief Find active subgroups
 *        subgroup with atleast one node pointing to it is marked active
 *
 * @param [in] pa  pointer to radio structure
 *
 * @return 0
 */
void atf_find_active_subgroups(struct wlan_objmgr_pdev *pdev)
{
	struct atf_group_list *group = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct pdev_atf *pa = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("PDEV ATF object is NULL!\n");
		return;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (psoc == NULL) {
		atf_err("psoc is NULL\n");
		return;
	}

	/* iterate over node table and mark active subgroups */
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
					  node_iter_mark_active_sg, psoc,
					  0, WLAN_ATF_ID);

	/* Find the number of active subgroups within each group */
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
				group_next);
		next_node = NULL;
		if (group) {
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode,
						struct atf_subgroup_list,
						sg_next);
				tmpnode_next = NULL;
				if (ATF_IS_SG_ACTIVE(subgroup)) {
					group->atf_num_active_sg++;
				}
				qdf_list_peek_next(&group->atf_subgroups,
						   tmpnode, &tmpnode_next);
				tmpnode = tmpnode_next;
			}
			pa->atf_num_active_sg += group->atf_num_active_sg;

			/* Find number of subgroups in STRICT & FAIR_UB Active
			 * groups
			 */
			if (group->group_sched == ATF_GROUP_SCHED_STRICT) {
				pa->atf_num_active_sg_strict +=
						group->atf_num_active_sg;
			} else if (group->group_sched ==
				   ATF_GROUP_SCHED_FAIR_UB) {
				pa->atf_num_active_sg_fair_ub +=
						group->atf_num_active_sg;
			}
		}
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		node = next_node;
	}
}

/**
 * @brief Implements ATF Fair-queue scheduling algorithm
 *
 * @param [in] pa  pointer to radio structure
 *
 * @return 0
 */
int atf_fairq_algo(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct pdev_atf *pa = NULL;

	psoc = wlan_pdev_get_psoc(pdev);
	if (psoc == NULL) {
		atf_err("Psoc is NULL\n");
		return -1;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pa is null!\n");
		return -1;
	}

	/* initialize group & ic variables */
	atf_init_pdev_group_var(pa);

	/* Find subgroups that are active */
	atf_find_active_subgroups(pdev);

	/* Compute tokens for contribution */
	atf_fairq_compute_tokens(pa);

	/* Compute tokens for contribution across groups */
	atf_common_contribute_pool(pa);

	/* get unassigned tokens */
	atf_manage_unassigned_tokens(pdev);

	/* Distribute tokens to subgroup */
	atf_distribute_tokens_subgroup(pa);

	/* Iterate Node table & unblock all nodes at every timer tick */
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
					  atf_peer_resume_iterater, psoc, 0,
					  WLAN_ATF_ID);

	/* Updated TIDstate stat */
	atf_update_stats_tidstate(pa);

	/* Update stats pointer */
	atf_stats_history(pa);

	return 0;
}

/**
 * @brief Create Default subgroup & link to default group
 * @param [in] pa the handle to the radio
 *        [in] group Reference to the group list
 *
 * @return subgroup on Success
 *         NULL on error
 */
struct atf_subgroup_list *atf_create_default_subgroup(struct pdev_atf *pa,
						struct atf_group_list *group)
{
	struct atf_subgroup_list *subgroup = NULL;

	if (NULL == group) {
		atf_err(" group is NULL \n");
		return NULL;
	}

	/* Creating Default sub-group */
	subgroup = (struct atf_subgroup_list *)
	qdf_mem_malloc(sizeof(struct atf_subgroup_list));

	if (subgroup) {
		if (qdf_str_lcopy(subgroup->sg_name, DEFAULT_SUBGROUPNAME,
		    sizeof(DEFAULT_SUBGROUPNAME) + 1) >=
		    sizeof(DEFAULT_SUBGROUPNAME)) {
			qdf_print("source too long");
			return NULL;
		}
		qdf_list_insert_back(&group->atf_subgroups, &subgroup->sg_next);
		subgroup->sg_id  = group->subgroup_count;
	}

	return subgroup;
}

/**
 * @brief Create Default group
 * @param [in] pa  the handle to the radio
 *
 * @return Reference to the group on SUCCESS
 *         NULL on error
 */
struct atf_group_list *atf_create_default_group(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;

	group = (struct atf_group_list *)
		qdf_mem_malloc(sizeof(struct atf_group_list));
	if (group) {
		if (qdf_str_lcopy(group->group_name, DEFAULT_GROUPNAME,
		    sizeof(DEFAULT_GROUPNAME) + 1) >=
		    sizeof(DEFAULT_GROUPNAME)) {
			atf_err("source too long\n");
			return NULL;
		}

		group->atf_num_sg_borrow = 0;
		group->atf_num_active_sg = 0;
		group->atf_contributabletokens = 0;
		group->subgroup_count = 0;
		group->shadow_atf_contributabletokens = 0;
		qdf_list_insert_back(&pa->atf_groups, &group->group_next);
	}

	return group;
}

static int32_t atf_get_dynamic_enable_disable_da(struct pdev_atf *pa)
{
	return pa->atf_commit;
}

/**
 * atf_token_allocate_timer_handler() - Token allocation timer expiry handler
 * @arg : Reference to passed argument during initialization
 *
 * This is token allocation timer expiry handler which takes global pdev
 * reference as argument.
 *
 * Don't take lock in this timer context.
 * Lock is avoided in all API invoked from this context.
 *
 * Return : none
 */
void atf_token_allocate_timer_handler(void *arg)
{
	struct wlan_objmgr_pdev *pdev = (struct wlan_objmgr_pdev *)arg;
	struct pdev_atf *pa = NULL;
	struct atf_context *ac = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	uint32_t airtime_unassigned = 0;
	uint32_t txtokens_unassigned = 0;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("PDEV ATF object is NULL!\n");
		return;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("ATF context is NULL!\n");
		return;
	}

	if (pa->atf_tput_based) {
		/*
		 * Instead of using airtimes configured by user, we
		 * try to determine the airtimes for different nodes.
		 */
		atf_distribute_airtime(pa);
	} else {
		/*
		 * Calculate 'Unassigned airtime'
		 * (1000 - Total configured airtime for VAPS)
		 * & update lmac layer
		 */
		airtime_unassigned = atf_airtime_unassigned(pa);
		txtokens_unassigned =
			atf_compute_txtokens(pa, airtime_unassigned,
							ATF_TOKEN_INTVL_MS);
	}
	/* Is OBSS scheduling enabled */
	if (pa->atf_sched & ATF_SCHED_OBSS) {
		/* get the channel busy stats percentage */
		pa->atf_chbusy =
			psoc->soc_cb.tx_ops.atf_tx_ops.atf_get_chbusyper(pdev);
		/* calculate the actual available tokens based on
		 * channel busy percentage
		 */
		pa->atf_avail_tokens = atf_get_available_tokens(pa);
	} else {
		/* Just use the total tokens */
		pa->atf_avail_tokens = ATF_TOKEN_INTVL_MS;
	}

	if (pa->atf_sched & ATF_SCHED_STRICT) {
		/* ATF - strictq algorithm Parse Node table,
		 * Derive txtokens & update node structure
		 */
		atf_strictq_algo(pdev);
	} else {
		/* ATF - fairq alogrithm
		 * Reset the atf_ic variables at the start
		 * set if there are clients looking to borrow
		 */
		atf_fairq_algo(pdev);
	}
	if (ac->atf_update_tbl_to_fm)
		ac->atf_update_tbl_to_fm(pdev);
	if (pa->atf_commit)
		qdf_timer_mod(&pa->atf_tokenalloc_timer, ATF_TOKEN_INTVL_MS);
}

void atf_pdev_atf_init_da(struct wlan_objmgr_pdev *pdev, struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	uint8_t i = 0, groupindex = 0;

	if ((NULL == pdev) || (NULL == pa)) {
		atf_err("Invalid input!\n");
		return;
	}

	qdf_spinlock_create(&pa->atf_lock);
	pa->atf_ssidgroup = 0;
	qdf_list_create(&pa->atf_groups, ATF_MAX_GROUPS);
	atf_config_add_pdev_group(pa, DEFAULT_GROUPNAME);
	group = atf_add_new_group(pa, DEFAULT_GROUPNAME);
	if (group == NULL) {
		atf_config_del_pdev_group(pa, groupindex);
		qdf_list_destroy(&pa->atf_groups);
		qdf_spinlock_destroy(&pa->atf_lock);
		return;
	}
	subgroup = atf_add_new_subgroup(pa, group,
					NULL, 0, ATF_SUBGROUP_TYPE_SSID);
	if (subgroup == NULL) {
		group->group_del = 1;
		atf_config_del_pdev_group(pa, groupindex);
		atf_remove_group(pa);
		qdf_list_destroy(&pa->atf_groups);
		qdf_spinlock_destroy(&pa->atf_lock);
		return;
	}
	/* reset */
	pa->atf_sched = 0;
	/* disable strict queue */
	pa->atf_sched &= ~ATF_SCHED_STRICT;
	/* enable OBSS */
	pa->atf_sched |= ATF_SCHED_OBSS;
	pa->atf_txbuf_share = 1;
	pa->atf_contr_level_incr = ATF_NUM_ITER_CONTRIBUTE_LEVEL_INCR;
	pa->atf_num_clients = wlan_pdev_get_max_peer_count(pdev);
	pa->atf_txbuf_max = ATF_MAX_BUFS;
	pa->atf_txbuf_min = ATF_MIN_BUFS;
	pa->atf_tput_based = 0;
	pa->atf_resv_airtime = ATF_TPUT_RESV_AIRTIME;

	for (i = 0; i < ATF_TPUT_MAX_STA; i++) {
		qdf_mem_zero(pa->atf_tput_tbl[i].mac_addr, QDF_MAC_ADDR_SIZE);
		pa->atf_tput_tbl[i].order = 0;
	}

	pa->atf_tput_tbl_num = 0;
	pa->atf_tput_order_max = 0;
	pa->atf_airtime_override = 0;
	qdf_timer_init(NULL, &pa->atfcfg_timer, atf_cfg_timer_handler,
				(void *)pdev, QDF_TIMER_TYPE_WAKE_APPS);
	qdf_timer_init(NULL, &pa->atf_tokenalloc_timer,
				atf_token_allocate_timer_handler,
				(void *)pdev, QDF_TIMER_TYPE_WAKE_APPS);
}

void atf_pdev_atf_deinit_da(struct wlan_objmgr_pdev *pdev, struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *tnode = NULL, *tmpnode = NULL;

	if ((NULL == pdev) || (NULL == pa)) {
		atf_err("Invalid input!\n");
		return;
	}
	qdf_spin_lock(&pa->atf_lock);
	pa->atf_tasksched = 0;
	qdf_spin_unlock(&pa->atf_lock);
	pa->atf_sched = 0;
	qdf_timer_free(&pa->atfcfg_timer);
	qdf_timer_free(&pa->atf_tokenalloc_timer);
	qdf_spinlock_destroy(&pa->atf_lock);
	while (!qdf_list_empty(&pa->atf_groups)) {
		qdf_list_remove_front(&pa->atf_groups, &tnode);
		if (!tnode)
			break;
		group = qdf_container_of(tnode, struct atf_group_list,
								group_next);
		if (group != NULL) {
			while (!qdf_list_empty(&group->atf_subgroups)) {
				qdf_list_remove_front(&group->atf_subgroups,
						      &tmpnode);
				if (!tmpnode) {
					break;
				}
				subgroup = qdf_container_of(tmpnode,
						       struct atf_subgroup_list,
						       sg_next);
				if (subgroup != NULL) {
					qdf_mem_free(subgroup);
				}
				tmpnode = NULL;
				subgroup = NULL;
			}
			qdf_list_destroy(&group->atf_subgroups);
			qdf_mem_free(group);
		}
		tnode = NULL;
		group = NULL;
	}
	qdf_list_destroy(&pa->atf_groups);
	atf_info("ATF Terminate!\n");
}

void atf_open_da(struct wlan_objmgr_psoc *psoc)
{
	struct atf_context *ac = NULL;

	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("ATF context is NULL!\n");
		return;
	}
	/* Enable ATF by default - for DA
	 * ATF scheduling can be enabled/disabled using commitatf command
	 */
	ac->atf_mode = 1;
	ac->atf_fmcap = 1;
}

void atf_ctx_init_da(struct atf_context *ac)
{
	if (NULL == ac) {
		atf_err("ATF context is null!\n");
		return;
	}

	ac->atf_pdev_atf_init = atf_pdev_atf_init_da;
	ac->atf_pdev_atf_deinit = atf_pdev_atf_deinit_da;
	ac->atf_set = atf_set_da;
	ac->atf_clear = atf_clear_da;
	ac->atf_set_maxclient = atf_set_maxclient_da;
	ac->atf_get_maxclient = atf_get_maxclient_da;
	ac->atf_set_group_unused_airtime = atf_set_group_unused_airtime_da;
	ac->atf_update_group_tbl = atf_update_group_tbl_da;
	ac->atf_update_tbl_to_fm = atf_update_tbl_to_fm_da;
	ac->atf_get_dynamic_enable_disable = atf_get_dynamic_enable_disable_da;
	ac->atf_open = atf_open_da;
}
