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
 *
 */

#include "atf_cmn_api_i.h"
#include <osif_private.h>

extern uint32_t atf_mode;

void atf_pdev_atf_init_ol(struct wlan_objmgr_pdev *pdev, struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	uint8_t groupindex = 0;

	if ((NULL == pdev) || (NULL == pa)) {
		atf_err("Invalid inputs!\n");
		return;
	}

	qdf_spinlock_create(&pa->atf_lock);
	pa->atf_ssidgroup = 0;
	/* reset */
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
	pa->atf_sched = 0;
	/* disable strict queue */
	pa->atf_sched &= ~ATF_SCHED_STRICT;
	/* enable OBSS */
	pa->atf_sched |= ATF_SCHED_OBSS;
	pa->atf_contr_level_incr = ATF_NUM_ITER_CONTRIBUTE_LEVEL_INCR;
	pa->atf_txbuf_max = -1;
	pa->atf_txbuf_min = -1;
	pa->atf_txbuf_share = 0;
	pa->atf_num_clients = wlan_pdev_get_max_peer_count(pdev);
	qdf_timer_init(NULL, &pa->atfcfg_timer, atf_cfg_timer_handler,
		       (void *)pdev, QDF_TIMER_TYPE_WAKE_APPS);
}

void atf_pdev_atf_deinit_ol(struct wlan_objmgr_pdev *pdev, struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *tnode = NULL, *tmpnode = NULL;

	if ((NULL == pdev) || (NULL == pa)) {
		atf_err("Invalid inputs!\n");
		return;
	}

	qdf_spin_lock(&pa->atf_lock);
	pa->atf_tasksched = 0;
	qdf_spin_unlock(&pa->atf_lock);
	pa->atf_sched = 0;
	qdf_timer_free(&pa->atfcfg_timer);
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
					     struct atf_subgroup_list, sg_next);
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

int32_t atf_set_ol(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_atf *pa;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct atf_context *ac = NULL;
	int32_t retv = 0;

	if (NULL == vdev) {
		atf_err("vdev is null!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pa is null!\n");
		return -1;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (NULL == psoc) {
		atf_err("psoc is null!\n");
		return -1;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf_context is null!\n");
		return -1;
	}

	if (pa->atf_tput_tbl_num)
		pa->atf_tput_based = 1;
	if (ac->atf_fmcap && ac->atf_mode) {
		pa->atf_commit = 1;
		atf_start_cfg_timer(vdev, pa, ac);

		psoc->soc_cb.tx_ops.atf_tx_ops.atf_enable_disable(vdev, 1);
	} else {
		atf_err("Either firmware capability or host ATF "
				"configuration not supported!!\n");
	}

	return retv;
}

int32_t atf_clear_ol(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_atf *pa = NULL;
	int32_t retv = 0;
	struct wlan_objmgr_psoc *psoc = NULL;

	if (NULL == vdev) {
		atf_err("vdev is NULL!\n");
		return -1;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev_atf is NULL!\n");
		return -1;
	}

	psoc->soc_cb.tx_ops.atf_tx_ops.atf_enable_disable(vdev, 0);

	return retv;
}

int32_t atf_set_maxclient_ol(struct wlan_objmgr_pdev *pdev, uint8_t enable)
{
	atf_info("ATF_MAX_CLIENT not valid for this VAP\n");
	return -1;
}

int32_t atf_get_maxclient_ol(struct wlan_objmgr_pdev *pdev)
{
	atf_info("ATF_MAX_CLIENT not valid for this VAP\n");
	return -1;
}

#define ATF_SET_EXT_REQ_PEER_MAC() \
	ext_req->atf_peer_ext_info[j].peer_macaddr.mac_addr31to0 = 0; \
	ext_req->atf_peer_ext_info[j].peer_macaddr.mac_addr47to32 = 0; \
	ext_req->atf_peer_ext_info[j].peer_macaddr.mac_addr31to0 |= \
				(cfg->peer_id[i].sta_mac[0] & 0xff); \
	ext_req->atf_peer_ext_info[j].peer_macaddr.mac_addr31to0 |= \
				cfg->peer_id[i].sta_mac[1] << 8; \
	ext_req->atf_peer_ext_info[j].peer_macaddr.mac_addr31to0 |= \
				cfg->peer_id[i].sta_mac[2] << 16; \
	ext_req->atf_peer_ext_info[j].peer_macaddr.mac_addr31to0 |= \
				cfg->peer_id[i].sta_mac[3] << 24; \
	ext_req->atf_peer_ext_info[j].peer_macaddr.mac_addr47to32 |= \
				(cfg->peer_id[i].sta_mac[4] & 0xff); \
	ext_req->atf_peer_ext_info[j].peer_macaddr.mac_addr47to32 |= \
				cfg->peer_id[i].sta_mac[5] << 8;

#define ATF_SET_ATF_REQ_PEER_MAC() \
	atf_req->atf_peer_info[j].peer_macaddr.mac_addr31to0 = 0; \
	atf_req->atf_peer_info[j].peer_macaddr.mac_addr47to32 = 0; \
	atf_req->atf_peer_info[j].peer_macaddr.mac_addr31to0 |= \
						cfg->peer_id[i].sta_mac[0]; \
	atf_req->atf_peer_info[j].peer_macaddr.mac_addr31to0 |= \
					cfg->peer_id[i].sta_mac[1] << 8; \
	atf_req->atf_peer_info[j].peer_macaddr.mac_addr31to0 |= \
					cfg->peer_id[i].sta_mac[2] << 16; \
	atf_req->atf_peer_info[j].peer_macaddr.mac_addr31to0 |= \
					cfg->peer_id[i].sta_mac[3] << 24; \
	atf_req->atf_peer_info[j].peer_macaddr.mac_addr47to32 |= \
						cfg->peer_id[i].sta_mac[4]; \
	atf_req->atf_peer_info[j].peer_macaddr.mac_addr47to32 |= \
						cfg->peer_id[i].sta_mac[5] << 8;
#define ATF_IS_PEER_CONFIGURED() \
	(cfg->peer_id[peer_index].cfg_flag && \
	(cfg->peer_id[peer_index].sta_cfg_value[index] || \
	cfg->peer_id[peer_index].sta_cfg_value[ATF_CFG_GLOBAL_INDEX]))

#define ATF_PEER_UNITS_RESERVED(index) \
	ext_req->atf_peer_ext_info[index].atf_units_reserved

int32_t atf_configured_peer_indic(struct pdev_atf *pa,
				  struct pdev_atf_peer_ext_request *ext_req,
				  u_int8_t peer_index, u_int8_t peer_index_fw)
{
	int32_t retv = 0, index = 0;
	struct atf_config *cfg = NULL;

	if (NULL == pa) {
		atf_err ("ATF pdev is NULL\n");
		return -1;
	}
	if (NULL == ext_req) {
		atf_err ("ATF extended req is NULL\n");
		return -1;
	}
	ATF_PEER_UNITS_RESERVED(peer_index_fw) = 0x0;
	cfg = &pa->atfcfg_set;
	if (cfg->peer_id[peer_index].index_group == 0xff) {
		ext_req->atf_peer_ext_info[peer_index].group_index = 0;
		return retv;
	}
	if (pa->atf_ssidgroup) {
		/* SSID grouping enabled */
		index = cfg->peer_id[peer_index].index_group;
	} else {
		/* VAP based configuration */
		index = cfg->peer_id[peer_index].index_vdev;
	}
	if ((index < ATF_CFG_NUM_VDEV) && ATF_IS_PEER_CONFIGURED()) {
		ATF_SET_PEER_CFG(ATF_PEER_UNITS_RESERVED(peer_index_fw));
	} else {
		ATF_RESET_PEER_CFG(ATF_PEER_UNITS_RESERVED(peer_index_fw));
	}

	return retv;
}

int32_t
atf_wmm_ac_config_fw(struct pdev_atf *pa,
		  struct pdev_atf_group_wmm_ac_req * grp_ac_req)
{
	struct atf_config *cfg = NULL;
	u_int8_t   i = 0, j = 0, k = 0;
	u_int8_t acbitmap = 0;

	if (NULL == pa) {
		atf_err ("ATF pdev is NULL\n");
		return -1;
	}
	if (NULL == grp_ac_req) {
		atf_err ("ATF extended req is NULL\n");
		return -1;
	}

	cfg = &pa->atfcfg_set;
	if (pa->atf_ssidgroup) {
		grp_ac_req->num_groups = cfg->grp_num_cfg;
		atf_debug("Num groups : %d \n",grp_ac_req->num_groups);
		for (i = 0; (i < cfg->grp_num_cfg); i++) {
			acbitmap = cfg->atfgroup[i].atf_cfg_ac.ac_bitmap;
			for(k = 0; k < WME_NUM_AC; k++) {
				if (!(1 & acbitmap))
					cfg->atfgroup[i].atf_cfg_ac.ac[k][0] = 0;
				acbitmap = acbitmap >> 1;
			}
			grp_ac_req->atf_group_wmm_ac_info[i].atf_config_ac_be =
				cfg->atfgroup[i].atf_cfg_ac.ac[WME_AC_BE][0];
			grp_ac_req->atf_group_wmm_ac_info[i].atf_config_ac_bk =
				cfg->atfgroup[i].atf_cfg_ac.ac[WME_AC_BK][0];
			grp_ac_req->atf_group_wmm_ac_info[i].atf_config_ac_vi =
				cfg->atfgroup[i].atf_cfg_ac.ac[WME_AC_VI][0];
			grp_ac_req->atf_group_wmm_ac_info[i].atf_config_ac_vo =
				cfg->atfgroup[i].atf_cfg_ac.ac[WME_AC_VO][0];
			atf_debug("Group index:%d BE:%d BK:%d VI:%d VO:%d\n",
			i,grp_ac_req->atf_group_wmm_ac_info[i].atf_config_ac_be,
			grp_ac_req->atf_group_wmm_ac_info[i].atf_config_ac_bk,
			grp_ac_req->atf_group_wmm_ac_info[i].atf_config_ac_vi,
			grp_ac_req->atf_group_wmm_ac_info[i].atf_config_ac_vo);
		}
	} else {
		grp_ac_req->num_groups = cfg->vdev_num_cfg + 1;
		atf_debug("Num groups : %d \n",grp_ac_req->num_groups);
		for (i = 0, j = 1; (i < grp_ac_req->num_groups) &&
		     (i < ATF_CFG_NUM_VDEV) && (j < ATF_ACTIVED_MAX_ATFGROUPS); i++, j++) {
			acbitmap = cfg->vdev[i].atf_cfg_ac.ac_bitmap;
			for(k = 0; k < WME_NUM_AC; k++) {
				if (!(1 & acbitmap))
					cfg->vdev[i].atf_cfg_ac.ac[k][0] = 0;
				acbitmap = acbitmap >>1;
			}
			grp_ac_req->atf_group_wmm_ac_info[j].atf_config_ac_be =
				cfg->vdev[i].atf_cfg_ac.ac[WME_AC_BE][0];
			grp_ac_req->atf_group_wmm_ac_info[j].atf_config_ac_bk =
				cfg->vdev[i].atf_cfg_ac.ac[WME_AC_BK][0];
			grp_ac_req->atf_group_wmm_ac_info[j].atf_config_ac_vi =
				cfg->vdev[i].atf_cfg_ac.ac[WME_AC_VI][0];
			grp_ac_req->atf_group_wmm_ac_info[j].atf_config_ac_vo =
				cfg->vdev[i].atf_cfg_ac.ac[WME_AC_VO][0];
			atf_debug("Vdev index:%d BE:%d BK:%d VI:%d VO:%d\n",
			i,grp_ac_req->atf_group_wmm_ac_info[i].atf_config_ac_be,
			grp_ac_req->atf_group_wmm_ac_info[i].atf_config_ac_bk,
			grp_ac_req->atf_group_wmm_ac_info[i].atf_config_ac_vi,
			grp_ac_req->atf_group_wmm_ac_info[i].atf_config_ac_vo);
		}
	}

	return 0;
}

int32_t atf_build_for_fm(struct pdev_atf *pa)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	struct pdev_atf_req *atf_req = NULL;
	struct pdev_atf_peer_ext_request *ext_req = NULL;
	struct pdev_atf_ssid_group_req *grp_req = NULL;
	struct pdev_atf_group_wmm_ac_req *grp_ac_req = NULL;
	struct atf_config *cfg = NULL;
	int32_t retv = 0;
	uint8_t i = 0, j = 0;
	uint32_t pdev_id = 0;

	pdev = pa->pdev_obj;
	atf_req = &pa->atf_req;
	ext_req = &pa->atf_peer_req;
	grp_req = &pa->atf_group_req;
	grp_ac_req = &pa->atf_group_ac_req;
	cfg = &pa->atfcfg_set;
	psoc = wlan_pdev_get_psoc(pdev);
	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);

	if (cfg->peer_num_cal != 0) {
		atf_req->percentage_uint = cfg->percentage_unit;
		atf_req->num_peers = cfg->peer_num_cal;
		ext_req->num_peers = cfg->peer_num_cal;
		for (i = 0; (i < ATF_ACTIVED_MAX_CLIENTS) &&
		     (j < cfg->peer_num_cal); i++) {
			if (cfg->peer_id[i].sta_assoc_status == 1) {
				if (cfg->peer_id[i].index_group == 0xff) {
				    ext_req->atf_peer_ext_info[j].group_index =
						cfg->peer_id[i].index_group;
				} else {
				    ext_req->atf_peer_ext_info[j].group_index =
					(cfg->peer_id[i].index_group - 1);
				}
				atf_configured_peer_indic(pa, ext_req, i, j);
				ATF_SET_EXT_REQ_PEER_MAC();
				ext_req->atf_peer_ext_info[j].pdev_id = pdev_id;
				peer = wlan_objmgr_get_peer(psoc, pdev_id,
					cfg->peer_id[i].sta_mac,
					WLAN_ATF_ID);
				if (peer) {
					vdev = wlan_peer_get_vdev(peer);
					ext_req->atf_peer_ext_info[j].vdev_id =
							wlan_vdev_get_id(vdev);
					atf_req->atf_peer_info[j].vdev_id =
							wlan_vdev_get_id(vdev);
					wlan_objmgr_peer_release_ref(peer,
								WLAN_ATF_ID);
				}
				atf_req->atf_peer_info[j].pdev_id = pdev_id;
				atf_req->atf_peer_info[j].percentage_peer =
						cfg->peer_id[i].sta_cal_value;
				atf_debug("Peer MAC: %s, GroupIndex: %u, \
					  atf_units_reserved : 0x%08x \
					  airtime : %d\n",
					  ether_sprintf(cfg->peer_id[i].sta_mac),
					  ext_req->atf_peer_ext_info[j].group_index,
					  ext_req->atf_peer_ext_info[j].atf_units_reserved,
					  atf_req->atf_peer_info[j].percentage_peer);
				ATF_SET_ATF_REQ_PEER_MAC();
				j++;
			}
		}
	} else {
		retv = -1;
		atf_info("No peer in allocation table, "
						"no action to firmware!\n");
	}
	/* Fill WMM AC configurations per group */
	atf_wmm_ac_config_fw(pa, grp_ac_req);
	grp_req->num_groups = cfg->grp_num_cfg;
	for (i = 0; (i < ATF_ACTIVED_MAX_ATFGROUPS) &&
	     (i < cfg->grp_num_cfg); i++) {
		grp_req->atf_group_info[i].percentage_group =
						cfg->atfgroup[i].grp_cfg_value;
		grp_req->atf_group_info[i].atf_group_units_reserved =
					cfg->atfgroup[i].grp_sched_policy;
		grp_req->atf_group_info[i].pdev_id = pdev_id;
		atf_debug("GroupIndex: %d, Airtime: %u, Sched policy: %u\n",
			  i, grp_req->atf_group_info[i].percentage_group,
			  grp_req->atf_group_info[i].atf_group_units_reserved);
	}

	return retv;
}

int32_t atf_update_tbl_to_fm_ol(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	int32_t retv = 0;

	if (NULL == pdev) {
		atf_err("Invalid input\n");
		return -1;
	}

	psoc = wlan_pdev_get_psoc(pdev);
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if ((NULL == pa) || (NULL == psoc)) {
		atf_err("pdev_atf or psoc is NULL\n");
		return -1;
	}
	retv = atf_build_for_fm(pa);
	if (retv == 0) {
		psoc->soc_cb.tx_ops.atf_tx_ops.atf_set_grouping(pdev,
					&pa->atf_group_req, pa->atf_tput_based);
		psoc->soc_cb.tx_ops.atf_tx_ops.atf_set_group_ac(pdev,
					&pa->atf_group_ac_req, pa->atf_tput_based);
		psoc->soc_cb.tx_ops.atf_tx_ops.atf_set(pdev, &pa->atf_req,
							pa->atf_tput_based);
		psoc->soc_cb.tx_ops.atf_tx_ops.atf_send_peer_request(pdev,
					&pa->atf_peer_req, pa->atf_tput_based);
	}

	return retv;
}

#define	ATF_SET_BWF_PEER_INFO() \
	bwf->bwf_peer_info[k].peer_macaddr.mac_addr31to0 = 0; \
	bwf->bwf_peer_info[k].peer_macaddr.mac_addr47to32 = 0; \
	bwf->bwf_peer_info[k].peer_macaddr.mac_addr31to0 |= \
				(pa->atf_tput_tbl[i].mac_addr[0] & 0xff); \
	bwf->bwf_peer_info[k].peer_macaddr.mac_addr31to0 |= \
				(pa->atf_tput_tbl[i].mac_addr[1] << 8); \
	bwf->bwf_peer_info[k].peer_macaddr.mac_addr31to0 |= \
				(pa->atf_tput_tbl[i].mac_addr[2] << 16); \
	bwf->bwf_peer_info[k].peer_macaddr.mac_addr31to0 |= \
				(pa->atf_tput_tbl[i].mac_addr[3] << 24); \
	bwf->bwf_peer_info[k].peer_macaddr.mac_addr47to32 |= \
				(pa->atf_tput_tbl[i].mac_addr[4] & 0xff); \
	bwf->bwf_peer_info[k].peer_macaddr.mac_addr47to32 |= \
				(pa->atf_tput_tbl[i].mac_addr[5] << 8); \

int32_t atf_build_bwf_for_fm_ol(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	struct pdev_bwf_req *bwf = NULL;
	uint8_t i = 0, j = 0, k = 0;
	int32_t retv = 0;
	uint32_t pdev_id = 0;

	if (NULL == pdev) {
		atf_err("Invalid input\n");
		return -1;
	}

	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	psoc = wlan_pdev_get_psoc(pdev);
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if ((NULL == pa) || (NULL == psoc)) {
		atf_err("pdev_atf or psoc is NULL\n");
		return -1;
	}
	bwf = &pa->bwf_req;

	if (pa->atf_tput_order_max) {
		for (i = 0; i < ATF_TPUT_MAX_STA; i++) {
			if (pa->atf_tput_tbl[i].order) {
				k = (pa->atf_tput_tbl[i].order) - 1;
				ATF_SET_BWF_PEER_INFO();
				bwf->bwf_peer_info[k].throughput =
						pa->atf_tput_tbl[i].tput;
				bwf->bwf_peer_info[k].max_airtime =
						pa->atf_tput_tbl[i].airtime;
				bwf->bwf_peer_info[k].priority =
						pa->atf_tput_tbl[i].order;
				bwf->bwf_peer_info[k].pdev_id = pdev_id;
				peer = wlan_objmgr_get_peer(psoc, pdev_id,
						pa->atf_tput_tbl[i].mac_addr,
						WLAN_ATF_ID);
				if (peer) {
					vdev = wlan_peer_get_vdev(peer);
					bwf->bwf_peer_info[k].vdev_id =
							wlan_vdev_get_id(vdev);
					wlan_objmgr_peer_release_ref(peer,
								WLAN_ATF_ID);
				}
				j++;
			}
		}
	}
	bwf->bwf_peer_info[j].peer_macaddr.mac_addr31to0 = 0xffffffff;
	bwf->bwf_peer_info[j].peer_macaddr.mac_addr47to32 = 0xffffffff;
	bwf->bwf_peer_info[j].throughput = 0;
	bwf->bwf_peer_info[j].max_airtime = pa->atf_resv_airtime;
	bwf->bwf_peer_info[j].priority = j + 1;
	bwf->bwf_peer_info[j].pdev_id = pdev_id;
	bwf->bwf_peer_info[j].vdev_id = 0;
	bwf->num_peers = pa->atf_tput_order_max + 1;
	psoc->soc_cb.tx_ops.atf_tx_ops.atf_set_bwf(pdev, bwf);

	return retv;
}

static void atf_reset_vdev_ol(struct wlan_objmgr_pdev *pdev)
{
	if (NULL == pdev) {
		atf_err("pdev is NULL!\n");
		return;
	}

	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  atf_vdev_init, 0, 1, WLAN_ATF_ID);
}

int32_t atf_get_dynamic_enable_disable_ol(struct pdev_atf *pa)
{
	/* Not supported */
	return -1;
}

void atf_open_ol(struct wlan_objmgr_psoc *psoc)
{
	if (NULL == psoc) {
		atf_err("PSOC is null!\n");
		return;
	}

	psoc->soc_cb.tx_ops.atf_tx_ops.atf_open(psoc);
}

void atf_enable_ol(struct wlan_objmgr_psoc *psoc)
{
	if (NULL == psoc) {
		atf_err("PSOC is null!\n");
		return;
	}

	psoc->soc_cb.tx_ops.atf_tx_ops.atf_register_event_handler(psoc);
}

void atf_disable_ol(struct wlan_objmgr_psoc *psoc)
{
	if (NULL == psoc) {
		atf_err("PSOC is null!\n");
		return;
	}

	psoc->soc_cb.tx_ops.atf_tx_ops.atf_unregister_event_handler(psoc);
}

void atf_ctx_init_ol(struct atf_context *ac)
{
	ac->atf_mode = atf_mode;

	ac->atf_pdev_atf_init = atf_pdev_atf_init_ol;
	ac->atf_pdev_atf_deinit = atf_pdev_atf_deinit_ol;
	ac->atf_set = atf_set_ol;
	ac->atf_clear = atf_clear_ol;
	ac->atf_set_maxclient = atf_set_maxclient_ol;
	ac->atf_get_maxclient = atf_get_maxclient_ol;
	ac->atf_update_tbl_to_fm = atf_update_tbl_to_fm_ol;
	ac->atf_build_bwf_for_fm = atf_build_bwf_for_fm_ol;
	ac->atf_reset_vdev = atf_reset_vdev_ol;
	ac->atf_get_dynamic_enable_disable = atf_get_dynamic_enable_disable_ol;
	ac->atf_open = atf_open_ol;
	ac->atf_enable = atf_enable_ol;
	ac->atf_disable = atf_disable_ol;
}

