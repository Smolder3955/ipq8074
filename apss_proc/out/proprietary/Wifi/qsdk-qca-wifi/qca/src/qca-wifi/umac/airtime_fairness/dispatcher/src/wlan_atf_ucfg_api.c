/*
 *
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#include <wlan_atf_ucfg_api.h>
#include "../../core/atf_cmn_api_i.h"
#include <wlan_atf_utils_api.h>

#define OTHER_SSID "Others   \0"

int32_t ucfg_atf_set(struct wlan_objmgr_vdev *vdev)
{
	struct atf_context *ac = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	int32_t retval = 0;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return -1;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_pdev(pdev);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return -1;
	}
	if (ac->atf_set)
		retval = ac->atf_set(vdev);

	atf_update_number_of_clients(pdev, retval);

	return retval;
}

int32_t ucfg_atf_clear(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_atf *pa = NULL;
	struct atf_context *ac = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	int32_t retval = 0;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return -1;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return -1;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev_atf object is NULL!\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_pdev(pdev);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return -1;
	}

	if (ac->atf_clear)
		retval = ac->atf_clear(vdev);

	pa->atf_commit = 0;
	if (pa->atf_tput_based && pa->atf_maxclient)
		atf_info("tput based atf & maxclients enabled\n");

	pa->atf_tput_based = 0;
	qdf_spin_lock(&pa->atf_lock);
	if (pa->atf_tasksched == 1) {
		pa->atf_tasksched = 0;
	}
	qdf_spin_unlock(&pa->atf_lock);

	atf_update_number_of_clients(pdev, retval);

	return retval;
}

int32_t ucfg_atf_set_txbuf_share(struct wlan_objmgr_vdev *vdev, uint8_t value)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct pdev_atf *pa = NULL;
	int reset = 0;
	int32_t retv = 0;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return -1;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return -1;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("PDEV atf object is NULL!\n");
		return -1;
	}

	if (pa->atf_txbuf_share && !(value & 0xF))
		reset = 1;
	else if (!pa->atf_txbuf_share && (value & 0xF))
		reset = 1;
	pa->atf_txbuf_share = value & 0xF;

	if (reset) {
		wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
						  atf_vdev_init, 0, 1,
						  WLAN_ATF_ID);
	}
	return retv;
}

void ucfg_atf_set_max_txbufs(struct wlan_objmgr_vdev *vdev, uint16_t value)
{
	struct pdev_atf *pa = NULL;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("PDEV atf object is NULL!\n");
		return;
	}

	if (value >= 0 && value <= ATH_TXBUF)
		pa->atf_txbuf_max = value;
	else
		pa->atf_txbuf_max = ATF_MAX_BUFS;
}

void ucfg_atf_set_min_txbufs(struct wlan_objmgr_vdev *vdev, uint16_t value)
{
	struct pdev_atf *pa = NULL;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("PDEV atf object is NULL!\n");
		return;
	}

	if (value >= 0 && value <= ATH_TXBUF)
		pa->atf_txbuf_min = value;
	else
		pa->atf_txbuf_min = ATF_MIN_BUFS;
}

void ucfg_atf_set_airtime_tput(struct wlan_objmgr_vdev *vdev, uint16_t value)
{
	struct pdev_atf *pa;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("PDEV atf object is NULL!\n");
		return;
	}

	pa->atf_airtime_override = value;
}

int32_t ucfg_atf_set_maxclient(struct wlan_objmgr_pdev *pdev, uint8_t enable)
{
	struct atf_context *ac;
	int32_t retval = 0;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_pdev(pdev);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return -1;
	}
	if (ac->atf_set_maxclient)
		retval = ac->atf_set_maxclient(pdev, enable);

	return retval;
}

void ucfg_atf_set_ssidgroup(struct wlan_objmgr_pdev *pdev,
			    struct wlan_objmgr_vdev *vdev, uint8_t enable)
{
	int8_t atfgroup_state = 0;
	struct pdev_atf *pa;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("PDEV atf object is NULL!\n");
		return;
	}

	/* Check if previous & current val are the same */
	if (pa->atf_ssidgroup != enable) {
		atfgroup_state = 1;
	}

	/* if the ATF group state has changed from the previous*/
	if (atfgroup_state) {
		/* If Grouping is being disabled & group exists */
		if ((!enable && pa->atfcfg_set.grp_num_cfg)) {
			atf_info("Group configuration exists already.\
					  Flushing ATF configuration\n");
			ucfg_atf_flush_table(vdev);
		}

		/* If Grouping is being enabled & ATF config(SSID/PEER) exists */
		if (enable &&
			(pa->atfcfg_set.peer_num_cfg || pa->atfcfg_set.vdev_num_cfg
			 || pa->atfcfg_set.peer_num_cal)) {
			atf_info("Flushing existing ATF configuration\n");
			ucfg_atf_flush_table(vdev);
		}
	}

	pa->atf_ssidgroup = !!enable;
}

int32_t ucfg_atf_set_ssid_sched_policy(struct wlan_objmgr_vdev *vdev,
				       uint16_t value)
{
	struct atf_context *ac;
	int32_t retval = 0;
	struct pdev_atf *pa = NULL;
	enum QDF_OPMODE opmode;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_vdev(vdev);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("PDEV atf object is NULL!\n");
		return -1;
	}
	opmode = wlan_vdev_mlme_get_opmode(vdev);
	if (QDF_SAP_MODE == opmode) {
		if ((value < 0) || (value >= ATF_GROUP_NUM_SCHED_POLICIES)) {
			atf_err("Invalid Sched Policy\n");
			atf_err("set 0 for 'Fair',1 for 'Strict' or '2' for 'fair with Upper bound'\n");
			return -1;
		}
		if (pa->atf_ssidgroup) {
			atf_err("Cannot configure SSID sched policy when grouping is enabled\n");
			return -1;
		}
		atf_set_ssid_sched_policy(vdev, value);
	} else {
		atf_err("config valid only in HostAP mode\n");
	}

	return retval;
}

void ucfg_atf_set_per_unit(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("PDEV atf object is NULL!\n");
		return;
	}

	pa->atfcfg_set.percentage_unit = PER_UNIT_1000;
}

static void atf_iter_vdev_ssid_validate(struct wlan_objmgr_pdev *pdev,
					void *object, void *arg)
{
	enum QDF_OPMODE opmode;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct ssid_val *sv = (struct ssid_val *)arg;
	uint8_t ssid[WLAN_SSID_MAX_LEN + 1] = {0};
	uint8_t ssid_len = 0;
	uint32_t len = 0;

	if ((NULL == pdev) || (NULL == vdev) || (NULL == sv)) {
		atf_err("pdev, vdev or ssid val is NULL!\n");
		return;
	}

	opmode = wlan_vdev_mlme_get_opmode(vdev);
	wlan_vdev_mlme_get_ssid(vdev, ssid, &ssid_len);

	if (opmode == QDF_SAP_MODE) {
		len = qdf_str_len((char *)sv->ssid);
		/* Compare VAP ssid with user provided SSID */
		if ((len == ssid_len) && !qdf_str_cmp((char *)(sv->ssid),
						      (char *)ssid)) {
			sv->ssid_exist = 1;
		}
	}
}

int32_t ucfg_atf_set_ssid(struct wlan_objmgr_vdev *vdev, struct ssid_val *value)
{
	uint8_t i = 0, vdev_flag = 0, first_index = 0xff, groupindex = 0;
	uint8_t num_vdevs = 0, vdev_index = 0;
	uint32_t cumulative_vdev_cfg_value = 0;
	struct pdev_atf *pa = NULL;
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	struct atf_config *cfg = NULL;
	qdf_list_node_t *node = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;

	if ((NULL == vdev) || (NULL == value)) {
		atf_err("vdev or value are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;
	if(pa->atf_ssidgroup) {
		atf_err("ATF SSID group is enabled. Cannot configure airtime for SSID\n");
		return -1;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (NULL == pdev) {
		atf_err("pdev is NULL\n");
		return -1;
	}
	/* Validate the SSID provided by the user display a message
	 * if configuration was done for a non-existing SSID.
	 * Note that the configuration will be applied even for a
	 * non-existing SSID
	 */
	value->ssid_exist = 0;
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  atf_iter_vdev_ssid_validate, value,
					  0, WLAN_ATF_ID);
	if (!value->ssid_exist) {
		atf_info("Airtime configuration applied for a non-existing "
			"SSID - %s:%d %%\n", value->ssid, (value->value / 10));
	}

	num_vdevs = cfg->vdev_num_cfg;
	for (vdev_index = 0; (vdev_index < ATF_CFG_NUM_VDEV) &&
	     (num_vdevs != 0); vdev_index++) {
		if (cfg->vdev[vdev_index].cfg_flag) {
			if (qdf_str_cmp((char *)(cfg->vdev[vdev_index].essid),
					(char *)(value->ssid))) {
				cumulative_vdev_cfg_value +=
					cfg->vdev[vdev_index].vdev_cfg_value;
			}
			num_vdevs--;
		}
	}
	cumulative_vdev_cfg_value += value->value;
	if (cumulative_vdev_cfg_value > PER_UNIT_1000) {
		atf_err("WRONG CUMULATIVE CONFIGURATION VALUE %d, MAX VALUE "
				"IS 1000!!!!!\n", cumulative_vdev_cfg_value);
		return -1;
	}
	for (i = 0, vdev_flag = 0; i < ATF_CFG_NUM_VDEV; i++) {
		if (cfg->vdev[i].cfg_flag) {
			if (!qdf_str_cmp((char *)(cfg->vdev[i].essid),
					 (char *)(value->ssid))) {
				break;
			}
		} else {
			if (vdev_flag == 0) {
				first_index = i;
				vdev_flag = 1;
			}
		}
	}
	if (i == ATF_CFG_NUM_VDEV) {
		if (first_index != 0xff) {
			i = first_index;
			qdf_mem_copy(cfg->vdev[i].essid, value->ssid,
				     qdf_str_len(value->ssid));
			cfg->vdev_num_cfg++;
			cfg->vdev[i].cfg_flag = 1;
			cfg->vdev[i].vdev_cfg_value = value->value;
		} else {
			atf_info("\n Vdev number over %d vdevs\n",
							ATF_CFG_NUM_VDEV);
		}
	} else {
		cfg->vdev[i].vdev_cfg_value = value->value;
	}

	/* Create group if group doesn't exist & ssid not part of group */
	if (!atf_config_is_group_exist(cfg, value->ssid, &groupindex) &&
		!atf_config_ssid_in_group(pa, value->ssid)) {
		if (cfg->grp_num_cfg == ATF_ACTIVED_MAX_ATFGROUPS) {
			atf_info("Num of groups(%d) > MAX GROUPS(%d)\n",
			 cfg->grp_num_cfg, ATF_ACTIVED_MAX_ATFGROUPS);
			return -1;
		}

		groupindex = cfg->grp_num_cfg;
		atf_config_add_pdev_group(pa, value->ssid);
		group = atf_add_new_group(pa, value->ssid);
		if (group == NULL) {
			atf_config_del_pdev_group(pa, groupindex);
			return -1;
		}

		subgroup = atf_add_new_subgroup(pa, group, NULL, value->value,
						ATF_SUBGROUP_TYPE_SSID);
		if (subgroup == NULL) {
			group->group_del = 1;
			atf_config_del_pdev_group(pa, groupindex);
			atf_remove_group(pa);
			return -1;
		}

		atf_config_ssid_group(pa, value->ssid, groupindex, group);
		atf_config_group_val_sched(pa, value->value, groupindex);
		group->group_airtime = value->value;
	} else {
		/* if ssid is already part of the group, modify airtime */
		cfg->atfgroup[groupindex].grp_cfg_value = value->value;
		group = cfg->atfgroup[groupindex].grplist_entry;
		if (group) {
			group->group_airtime = value->value;
			qdf_list_peek_front(&group->atf_subgroups, &node);
			if (node) {
				subgroup = qdf_container_of(node,
					   struct atf_subgroup_list, sg_next);
				subgroup->usr_cfg_val = value->value;
			}
		}
	}

	return 0;
}

int32_t
ucfg_atf_delete_ssid(struct wlan_objmgr_vdev *vdev, struct ssid_val *value)
{
	struct pdev_atf *pa = NULL;
	uint8_t groupindex = 0;
	uint8_t i = 0, j = 0, k = 0;
	struct atf_config *cfg = NULL;
	struct atf_group_list *group = NULL;

	if ((NULL == vdev) || (NULL == value)) {
		atf_err("vdev or value are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;

	for (i = 0; (i < ATF_CFG_NUM_VDEV); i++) {
		if (cfg->vdev[i].cfg_flag) {
			if (qdf_str_cmp((char *)(cfg->vdev[i].essid),
					(char *)(value->ssid)) == 0) {
				break;
			}
		}
	}

	if (i == ATF_CFG_NUM_VDEV) {
		atf_err(" The input ssid does not exist\n");
	} else {
		qdf_mem_zero(&cfg->vdev[i].essid[0], WLAN_SSID_MAX_LEN + 1);
		cfg->vdev[i].cfg_flag = 0;
		cfg->vdev[i].vdev_cfg_value = 0;
		if ((i + 1) <= cfg->vdev_num_cfg) {
			for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
				/* SSID at index (i+1) will be replaced with
				the one at last location*/
				cfg->peer_id[j].sta_cfg_value[i+1] =
						cfg->peer_id[j].sta_cfg_value[
							cfg->vdev_num_cfg];
				cfg->peer_id[j].sta_cfg_value[cfg->vdev_num_cfg]
									= 0;
				if (cfg->peer_id[j].index_vdev ==
							cfg->vdev_num_cfg) {
					cfg->peer_id[j].index_vdev = i + 1;
				}
			}
			k = cfg->vdev_num_cfg - 1;
			qdf_mem_copy((char *)&cfg->vdev[i].essid[0],
				     (char *)(&cfg->vdev[k].essid[0]),
				     WLAN_SSID_MAX_LEN + 1);

			cfg->vdev[i].cfg_flag = cfg->vdev[k].cfg_flag;
			cfg->vdev[i].vdev_cfg_value =
						cfg->vdev[k].vdev_cfg_value;
			qdf_mem_zero(&cfg->vdev[k].essid[0],
				     WLAN_SSID_MAX_LEN + 1);
			cfg->vdev[k].cfg_flag = 0;
			cfg->vdev[k].vdev_cfg_value = 0;
		}
		atf_info("SSID deleted\n"
			"Peer percentage linked to this SSID is cleared.\n");
		cfg->vdev_num_cfg--;
	}

	if (!pa->atf_ssidgroup) {
		if (atf_config_is_group_exist(cfg, value->ssid, &groupindex)) {
			/* Delete all subgroups in this group */
			group = cfg->atfgroup[groupindex].grplist_entry;
			atf_mark_del_all_sg(group);
			if (atf_config_ssid_in_group(pa, value->ssid)) {
				atf_config_del_ssid_group(pa, value->ssid,
							  groupindex);
			}
			/* Mark group for deletion */
			group->group_del = 1;
		}
	}

	return 0;
}

static int32_t ucfg_check_sta_per(struct pdev_atf *pa, struct sta_val *val)
{
	uint8_t j = 0;
	uint8_t groupindex = 0;
	struct atf_config *cfg = &pa->atfcfg_set;

	if (!val->ssid[0]) {
		atf_err("Peer atf configuration should have an ssid associated!!!");
		return ATF_SSID_NOT_EXIST;
	}

	/*NULL validation done in caller function*/
	/*check if we have some ssid*/
	if (pa->atf_ssidgroup) {
		if(atf_config_is_group_exist(cfg, val->ssid, &groupindex)) {
			if (atf_verify_sta_cumulative(cfg, groupindex, val) !=
							QDF_STATUS_SUCCESS) {
				return ATF_PER_NOT_ALLOWED;
			}
		} else {
			atf_err("Group %s does not exist\n",val->ssid);
			return ATF_SSID_NOT_EXIST;
		}
	} else {
		for (j = 0; (j < ATF_CFG_NUM_VDEV); j++) {
			if (cfg->vdev[j].cfg_flag) {
				if (qdf_str_cmp(cfg->vdev[j].essid, val->ssid)
									== 0)
					break;
			}
		}
		if (j == ATF_CFG_NUM_VDEV) {
			atf_err("ATF SSID config doesn't exist for %s. Configure airtime for SSID followed by STA\n",val->ssid);
			return ATF_SSID_NOT_EXIST;
		} else {
			if (atf_verify_sta_cumulative(cfg, j, val) !=
							QDF_STATUS_SUCCESS) {
				return ATF_PER_NOT_ALLOWED;
			}
		}
	}

	return 0;
}

int32_t ucfg_atf_set_sta(struct wlan_objmgr_vdev *vdev, struct sta_val *value)
{
	struct pdev_atf *pa = NULL;
	uint8_t i = 0, sta_flag = 0, sta_index = 0xff;
	uint64_t calbitmap;
	uint8_t sta_mac[QDF_MAC_ADDR_SIZE] = {0};
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	uint8_t groupindex = 0;
	struct atf_config *cfg = NULL;

	if ((NULL == vdev) || (NULL == value)) {
		atf_err("vdev or value are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;

	/* check for valid configuration */
	switch(ucfg_check_sta_per(pa, value)) {
		case ATF_PER_NOT_ALLOWED:
			atf_err("Percentage not allowed as cumulative sum exceeds 100%%\n");
			return -1;
		case ATF_SSID_NOT_EXIST:
			return -1;
	}

	calbitmap = 1;
	for (i = 0; i < ATF_ACTIVED_MAX_CLIENTS; i++) {
		if (cfg->peer_id[i].cfg_flag) {
			if (!qdf_mem_cmp((char *)(cfg->peer_id[i].sta_mac),
					 (char *)(value->sta_mac),
					 QDF_MAC_ADDR_SIZE)) {
				break;
			}
		} else {
			if (!qdf_mem_cmp((char *)(cfg->peer_id[i].sta_mac),
				(char *)(value->sta_mac), QDF_MAC_ADDR_SIZE)) {
				cfg->peer_num_cfg++;
				cfg->peer_id[i].cfg_flag = 1;
				cfg->peer_id[i].sta_cfg_mark = 1;
				cfg->peer_cal_bitmap |= (calbitmap << i);
				break;
			} else {
				if ((sta_flag == 0) &&
				    (!qdf_mem_cmp(
				    (char *)cfg->peer_id[i].sta_mac,
				    (char *)sta_mac, QDF_MAC_ADDR_SIZE))) {
					sta_index = i;
					sta_flag = 1;
				}
			}
		}
	}
	if (i == ATF_ACTIVED_MAX_CLIENTS) {
		if (sta_index != 0xff) {
			i = sta_index;
			qdf_mem_copy((char *)(cfg->peer_id[i].sta_mac),
				(char *)(value->sta_mac), QDF_MAC_ADDR_SIZE);
			cfg->peer_num_cfg++;
			cfg->peer_id[i].cfg_flag = 1;
			atf_new_sta_cfg(pa, value, sta_index);
		} else {
			atf_info("\n STA number over %d\n",
				   ATF_ACTIVED_MAX_CLIENTS);
		}
	} else {
		atf_modify_sta_cfg(pa, value, i);
	}

	/*creating subgroups for peer based configuration */
	if (atf_config_is_group_exist(cfg, value->ssid, &groupindex)) {
		group = cfg->atfgroup[groupindex].grplist_entry;
		if (!group) {
			atf_info("Group is NULL. Returning\n");
			return -1;
        }
		if (!atf_config_is_subgroup_exist(cfg, group, value->sta_mac,
						  &subgroup,
						  ATF_SUBGROUP_TYPE_PEER)) {
			subgroup = atf_add_new_subgroup(pa, group,
							value->sta_mac,
							value->value,
							ATF_SUBGROUP_TYPE_PEER);
			if (!subgroup) {
				atf_info("Subgroup is NULL. Returning\n");
				return -1;
			}
		} else {
			subgroup->usr_cfg_val = value->value;
		}
		if (!subgroup) {
			atf_info("Subgroup is NULL. Returning\n");
			return -1;
		}
	} else {
		atf_err("ERROR!!! shouldn't have reached here \
				 atf_group_exist : %d atf_ssid_in_group : %d\n",
				 atf_config_is_group_exist(cfg, value->ssid, &groupindex),
				 atf_config_ssid_in_group(pa, value->ssid));
	}

	return 0;
}

int32_t
ucfg_atf_delete_sta(struct wlan_objmgr_vdev *vdev, struct sta_val *value)
{
	struct pdev_atf *pa = NULL;
	uint8_t i = 0, j = 0, k = 0;
	uint64_t calbitmap = 1;
	struct atf_config *cfg = NULL;

	if ((NULL == vdev) || (NULL == value)) {
		atf_err("vdev or value are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;

	for (i = 0; i < ATF_ACTIVED_MAX_CLIENTS; i++) {
		if (cfg->peer_id[i].cfg_flag == 1) {
			if (!qdf_mem_cmp((char *)(cfg->peer_id[i].sta_mac),
					 (char *)(value->sta_mac),
					 QDF_MAC_ADDR_SIZE)) {
				break;
			}
		}
	}

	if (i == ATF_ACTIVED_MAX_CLIENTS) {
		atf_info(" The input sta does not exist\n");
	} else {
		cfg->peer_id[i].cfg_flag = 0;
		qdf_mem_zero(&(cfg->peer_id[i].sta_cfg_value[0]),
					sizeof(cfg->peer_id[i].sta_cfg_value));
		cfg->peer_num_cfg--;
		cfg->peer_id[i].sta_cfg_mark = 0;
		if ((cfg->peer_id[i].sta_cal_value == 0) &&
		    (cfg->peer_id[i].sta_assoc_status == 0)) {
			for (k = 0, j = 0; k < ATF_ACTIVED_MAX_CLIENTS; k++) {
				if (cfg->peer_id[k].index_vdev != 0)
					j = k;
			}
			if (j == i) {
				/*Delete this entry*/
				qdf_mem_zero(&cfg->peer_id[i].sta_mac[0],
							QDF_MAC_ADDR_SIZE);
				/*Flush out*/
				qdf_mem_zero(
					&(cfg->peer_id[i].sta_cfg_value[0]),
					sizeof(cfg->peer_id[i].sta_cfg_value));
				cfg->peer_id[i].sta_cal_value = 0;
				cfg->peer_id[i].sta_assoc_status = 0;
				cfg->peer_id[i].index_vdev = 0;
				cfg->peer_cal_bitmap &= ~(calbitmap << i);
			} else {
				cfg->peer_id[i].cfg_flag =
						cfg->peer_id[j].cfg_flag;
				cfg->peer_id[i].sta_cfg_mark =
						cfg->peer_id[j].sta_cfg_mark;
				cfg->peer_id[i].sta_cfg_value[
						cfg->peer_id[i].index_vdev] =
						cfg->peer_id[j].sta_cfg_value[
						cfg->peer_id[j].index_vdev];
				cfg->peer_id[i].index_vdev =
						cfg->peer_id[j].index_vdev;
				cfg->peer_id[i].sta_cal_value =
						cfg->peer_id[j].sta_cal_value;
				cfg->peer_id[i].sta_assoc_status =
					cfg->peer_id[j].sta_assoc_status;
				qdf_mem_copy(cfg->peer_id[i].sta_mac,
					     cfg->peer_id[j].sta_mac,
					     QDF_MAC_ADDR_SIZE);
				qdf_mem_copy(cfg->peer_id[i].sta_cfg_value,
						cfg->peer_id[j].sta_cfg_value,
					sizeof(cfg->peer_id[i].sta_cfg_value));
				cfg->peer_id[j].cfg_flag = 0;
				cfg->peer_id[j].sta_cfg_mark = 0;
				/*Flush out*/
				qdf_mem_zero(
					&(cfg->peer_id[j].sta_cfg_value[0]),
					sizeof(cfg->peer_id[i].sta_cfg_value));
				qdf_mem_zero(&cfg->peer_id[j].sta_mac[0],
					     QDF_MAC_ADDR_SIZE);
				cfg->peer_id[j].index_vdev = 0;
				cfg->peer_id[j].sta_cal_value = 0;
				cfg->peer_id[j].sta_assoc_status = 0;
				cfg->peer_cal_bitmap &= ~(calbitmap << j);
			}
		}
	}

	atf_config_mark_del_peer_subgroup(pa, value->sta_mac);
	return 0;
}

int32_t ucfg_atf_add_ac(struct wlan_objmgr_vdev *vdev, struct atf_ac *buf)
{
	struct pdev_atf *pa = NULL;
	struct atf_group_list *group = NULL;
	uint8_t groupindex = 0, vdevindex = 0;
	struct atf_ac_config *atf_ac = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	struct atf_config *cfg = NULL;

	if ((NULL == vdev) || (NULL == buf)) {
		atf_err("vdev or ATF AC data is NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;

	/* AC can be configured only after SSID or GROUP is configured */
	if (!atf_config_is_group_exist(cfg, buf->ac_ssid, &groupindex) ) {
		atf_err("ATF SSID config doesn't exist for %s\n", __func__);
		atf_err("Configure airtime for SSID followed by AC\n");
		return -1;
	}
	if (buf->ac_id > (WME_NUM_AC - 1)) {
		atf_err("invalid ac_id : %d \n", buf->ac_id);
		return -1;
	}
	if (pa->atf_ssidgroup) {
		atf_ac = &cfg->atfgroup[groupindex].atf_cfg_ac;
	} else {
		vdevindex = atf_find_vdev_ssid_index(pa, buf->ac_ssid);
		if ((vdevindex < 0) || (vdevindex >= ATF_CFG_NUM_VDEV)) {
			atf_err("Vap index could not be found. Return Error \n");
			return -1;
		}
		atf_ac = &cfg->vdev[vdevindex].atf_cfg_ac;
	}
	if (!atf_ac) {
		atf_err(" atf_ac is NULL. Return \n");
		return -1;
	}
	if (atf_validate_acval(atf_ac, buf->ac_id, buf->ac_val) !=
							QDF_STATUS_SUCCESS) {
		atf_err("Total Airtime assigned to AC's in a group/ssid \
				 cannot exceed 1000\n");
		return -1;
	}
	atf_ac->ac[buf->ac_id][0] = buf->ac_val;
	atf_ac->ac_bitmap = (atf_ac->ac_bitmap | (1 << buf->ac_id));
	atf_ac->ac_cfg_flag = 1;

	group = cfg->atfgroup[groupindex].grplist_entry;
	if (!group) {
		atf_err("Group is null\n");
		return -1;
	}
	/* Create New subgroup if it doesn't exist */
	if (!atf_config_is_subgroup_exist(cfg, group, &buf->ac_id, &subgroup,
					  ATF_SUBGROUP_TYPE_AC)) {
		subgroup = atf_add_new_subgroup(pa, group, &buf->ac_id, buf->ac_val,
						ATF_SUBGROUP_TYPE_AC);
		if (subgroup == NULL) {
			atf_err("subgroup is NULL\n");
			return -1;
		}
	} else {
		subgroup->usr_cfg_val = buf->ac_val;
	}
	if (!subgroup) {
		atf_err("subgroup is NULL\n");
		return -1;
	}

	return 0;
}

int32_t ucfg_atf_del_ac(struct wlan_objmgr_vdev *vdev, struct atf_ac *buf)
{
	struct pdev_atf *pa = NULL;
	struct atf_group_list *group = NULL;
	uint8_t groupindex = 0, vdevindex = 0;
	struct atf_ac_config *atf_ac = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	struct atf_config *cfg = NULL;

	if ((NULL == vdev) || (NULL == buf)) {
		atf_err("vdev or ATF AC data is NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;

	/* AC can be configured only after SSID or GROUP is configured */
	if (!atf_config_is_group_exist(cfg, buf->ac_ssid, &groupindex) ) {
		atf_err("group %s Doesn't exist. Invalid Configuration \n",
				buf->ac_ssid);
		return -1;
	}
	if (buf->ac_id > (WME_NUM_AC - 1)) {
		atf_err("invalid ac_id : %d \n", buf->ac_id);
		return -1;
	}
	/* Clear AC val in ic config structure */
	if (pa->atf_ssidgroup) {
		atf_ac = &cfg->atfgroup[groupindex].atf_cfg_ac;
	} else {
		vdevindex = atf_find_vdev_ssid_index(pa, buf->ac_ssid);
		if ((vdevindex < 0) || (vdevindex >= ATF_CFG_NUM_VDEV)) {
			atf_err("Vdev index could not be found. Return Error \n");
			return -1;
		}
		atf_ac = &cfg->vdev[vdevindex].atf_cfg_ac;
	}
	if (!atf_ac) {
		atf_err("atf_ac is NULL. Return \n");
		return -1;
	}
	atf_ac->ac[buf->ac_id][0] = 0;
	atf_ac->ac_bitmap =  (atf_ac->ac_bitmap & ~(1 << buf->ac_id));
	if (!atf_ac->ac_bitmap) {
		atf_ac->ac_cfg_flag = 0;
	}

	group = cfg->atfgroup[groupindex].grplist_entry;
	if (!group) {
		atf_err("Group is null\n");
		return -1;
	}
	/* Mark subgroup for deletion */
	if (atf_config_is_subgroup_exist(cfg, group, &buf->ac_id, &subgroup,
					 ATF_SUBGROUP_TYPE_AC)) {
		subgroup->sg_del = 1;
	} else {
		atf_err("AC subgroup Doesn't exist. Cannot Delete\n");
		return -1;
	}

	return 0;
}

uint8_t ucfg_atf_get(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_atf *pa;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("PDEV object is NULL!\n");
		return -1;
	}

	return pa->atf_commit;
}

uint8_t ucfg_atf_get_txbuf_share(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_atf *pa;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return 0;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("PDEV object is NULL!\n");
		return 0;
	}

	return pa->atf_txbuf_share;
}

uint16_t ucfg_atf_get_max_txbufs(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_atf *pa;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return 0;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("PDEV object is NULL!\n");
		return 0;
	}

	return pa->atf_txbuf_max;
}

uint16_t ucfg_atf_get_min_txbufs(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_atf *pa;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return 0;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("PDEV object is NULL!\n");
		return 0;
	}

	return pa->atf_txbuf_min;
}

uint16_t ucfg_atf_get_airtime_tput(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_atf *pa;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return 0;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("VDEV object is NULL!\n");
		return 0;
	}

	return pa->atf_airtime_override;
}

uint8_t ucfg_atf_get_maxclient(struct wlan_objmgr_pdev *pdev)
{
	struct atf_context *ac;
	int32_t retval = 0;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return 0;
	}
	ac = atf_get_atf_ctx_from_pdev(pdev);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return 0;
	}
	if (ac->atf_get_maxclient)
		retval = ac->atf_get_maxclient(pdev);

	return retval;
}

uint8_t ucfg_atf_get_ssidgroup(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return 0;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("PDEV atf object is NULL!\n");
		return 0;
	}

	return pa->atf_ssidgroup;
}

uint8_t ucfg_atf_get_ssid_sched_policy(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_atf *va = NULL;

	if (NULL == vdev) {
		atf_err("vdev is NULL\n");
		return 0;
	}
	va = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_ATF);
	if (NULL == va) {
		atf_err("vdev atf object is NULL\n");
		return 0;
	}

	return va->vdev_atf_sched;
}

uint16_t ucfg_atf_get_per_unit(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return 0;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("PDEV atf object is NULL!\n");
		return 0;
	}

	return pa->atfcfg_set.percentage_unit;
}

void ucfg_atf_get_peer_stats(struct wlan_objmgr_vdev *vdev, uint8_t *mac,
						struct atf_stats *astats)
{
	struct peer_atf *pa = NULL;
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	uint8_t pdev_id;

	if ((NULL == vdev) || (NULL == mac) || (NULL == astats)) {
		atf_err("peer or mac or astats are NULL\n");
		return;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL\n");
		return;
	}

	pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));
	peer = wlan_objmgr_get_peer(psoc, pdev_id, mac, WLAN_ATF_ID);
	if (NULL == peer) {
		atf_err("peer is NULL\n");
		return;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);

	qdf_mem_copy(astats, &pa->atf_peer_stats, sizeof(struct atf_stats));
	if (peer)
		wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
}

uint32_t ucfg_atf_get_mode(struct wlan_objmgr_psoc *psoc)
{
	struct atf_context *ac;

	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return 0;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return 0;
	}

	return ac->atf_mode;
}

uint32_t ucfg_atf_get_peer_airtime(struct wlan_objmgr_peer *peer)
{
	return wlan_atf_get_peer_airtime(peer);
}

void ucfg_atf_peer_join_leave(struct wlan_objmgr_peer *peer, uint8_t type)
{
	wlan_atf_peer_join_leave(peer, type);
}

/**
 * @brief function to show the configured groups & associated SSID's
 *
 */
static int32_t atf_group_table(struct wlan_objmgr_vdev *vdev,
			       struct atftable *buf)
{
	struct pdev_atf *pa = NULL;
	uint8_t *sta_mac = NULL;
	uint8_t i = 0, j = 0, k = 0, l = 0;
	uint8_t init_flag = 0, len = 0;
	uint8_t grp_index[ATF_CFG_NUM_VDEV] = {0};
	uint32_t leftperuint = 0;
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct atf_config *cfg = NULL;
	struct wlan_objmgr_vdev *vdev_temp = NULL;
	uint8_t ssid[WLAN_SSID_MAX_LEN+1] = {0};
	uint8_t pdev_id;

	if ((NULL == vdev) || (NULL == buf)) {
		atf_err("vdev or atf table are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;
	psoc = wlan_vdev_get_psoc(vdev);
	pdev_id = wlan_objmgr_pdev_get_pdev_id(wlan_vdev_get_pdev(vdev));

	if (!cfg->grp_num_cfg)
		return -1;

	buf->atf_group = 1;
	buf->info_cnt = 0;
	for (i = 0; i < cfg->grp_num_cfg; i++) {
		init_flag = 0;
		for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
			peer = wlan_objmgr_get_peer(psoc, pdev_id,
						    cfg->peer_id[j].sta_mac,
						    WLAN_ATF_ID);
			if (NULL == peer)
				continue;

			if ((peer != NULL) && !atf_valid_peer(peer)) {
				wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
				continue;
			}
			vdev_temp = wlan_peer_get_vdev(peer);
			if (vdev_temp == NULL) {
				wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
				continue;
			}
			if (cfg->peer_id[j].index_group != (i + 1)) {
				wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
				continue;
			}
			if (init_flag == 0) {
				l = buf->info_cnt;
				len = qdf_str_len(cfg->atfgroup[i].grpname);
				qdf_mem_copy(buf->atf_info[l].grpname,
					     cfg->atfgroup[i].grpname, len);
				buf->atf_info[l].value =
						cfg->atfgroup[i].grp_cfg_value;
				buf->atf_info[l].cfg_value =
						cfg->atfgroup[i].grp_cfg_value;
				buf->atf_info[l].info_mark = 0;
				buf->atf_info[l].atf_ac_cfg =
						cfg->atfgroup[i].atf_cfg_ac.ac_bitmap;
				buf->info_cnt++;
				init_flag++;
				grp_index[i] = i + 1;
				k += 1;
			}
			sta_mac = &cfg->peer_id[j].sta_mac[0];
			l = buf->info_cnt;
			qdf_mem_zero(ssid, WLAN_SSID_MAX_LEN + 1);
			wlan_vdev_mlme_get_ssid(vdev_temp, ssid, &len);
			qdf_mem_copy(buf->atf_info[l].ssid, ssid, len);
			buf->atf_info[l].value = cfg->peer_id[j].sta_cal_value;
			/**
			* Assigning peer percentage from Global value, though we
			* don't use this value if grouping is enabled in ATF.
			**/
			buf->atf_info[l].cfg_value =
			cfg->peer_id[j].sta_cfg_value[ATF_CFG_GLOBAL_INDEX];
			buf->atf_info[l].assoc_status =
					cfg->peer_id[j].sta_assoc_status;
			qdf_mem_copy(buf->atf_info[l].sta_mac, sta_mac,
				     QDF_MAC_ADDR_SIZE);
			buf->atf_info[l].info_mark = 1;
			buf->info_cnt++;
			if (peer)
				wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
		}
		leftperuint -= cfg->atfgroup[i].grp_cfg_value;
	}
	if ((cfg->grp_num_cfg != k) && (peer != NULL)) {
		vdev_temp = wlan_peer_get_vdev(peer);
		if (vdev_temp) {
			for (i = 0; i < cfg->grp_num_cfg; i++) {
				if ((cfg->vdev[i].cfg_flag == 1) &&
				    (grp_index[i] == 0)) {
					l = buf->info_cnt;
					qdf_mem_zero(ssid, WLAN_SSID_MAX_LEN+1);
					wlan_vdev_mlme_get_ssid(vdev_temp, ssid,
								&len);
					qdf_mem_copy(buf->atf_info[l].ssid,
						     ssid, len);
					buf->atf_info[l].value =
						cfg->vdev[i].vdev_cfg_value;
					buf->atf_info[l].cfg_value =
						cfg->vdev[i].vdev_cfg_value;
					buf->atf_info[l].info_mark = 0;
					buf->atf_info[l].atf_ac_cfg =
						cfg->atfgroup[i].atf_cfg_ac.ac_bitmap;
					buf->info_cnt++;
				}
			}
		}
	}
	l = buf->info_cnt;
	qdf_mem_copy(buf->atf_info[l].grpname, OTHER_SSID,
		     qdf_str_len(OTHER_SSID));
	buf->atf_info[l].value = leftperuint;
	buf->atf_info[l].cfg_value = 0;
	buf->atf_info[l].info_mark = 0;
	buf->info_cnt++;
	for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
		peer = wlan_objmgr_get_peer(psoc, pdev_id, cfg->peer_id[j].sta_mac,
					    WLAN_ATF_ID);
		if (NULL == peer)
			continue;

		vdev_temp = wlan_peer_get_vdev(peer);
		if (vdev_temp == NULL) {
			wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
			continue;
		}
		if ((peer != NULL) && !atf_valid_peer(peer)) {
			wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
			continue;
		} else if (cfg->peer_id[j].index_group == 0xFF) {
			sta_mac = &cfg->peer_id[j].sta_mac[0];
			l = buf->info_cnt;
			buf->atf_info[l].value = cfg->peer_id[j].sta_cal_value;
			buf->atf_info[l].cfg_value =
			cfg->peer_id[j].sta_cfg_value[ATF_CFG_GLOBAL_INDEX];
			buf->atf_info[l].assoc_status =
					cfg->peer_id[j].sta_assoc_status;
			qdf_mem_copy(buf->atf_info[l].sta_mac, sta_mac,
				     QDF_MAC_ADDR_SIZE);
			qdf_mem_zero(ssid, WLAN_SSID_MAX_LEN + 1);
			wlan_vdev_mlme_get_ssid(vdev_temp, ssid, &len);
			qdf_mem_copy(buf->atf_info[l].ssid, ssid, len);
			buf->atf_info[l].info_mark = 1;
			buf->info_cnt++;
		}
		buf->busy = pa->atf_chbusy;
		if (peer)
			wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
	}

	return 0;
}

int32_t ucfg_atf_show_table(struct wlan_objmgr_vdev *vdev, struct atftable *tbl)
{
	struct pdev_atf *pa = NULL;
	uint8_t *sta_mac = NULL;
	uint8_t i = 0, j = 0, k = 0, l = 0, z = 0;
	uint8_t init_flag = 0, len = 0;
	int8_t retv = 0;
	uint8_t vdev_index[ATF_CFG_NUM_VDEV] = {0};
	uint32_t leftperuint = 0;
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct atf_config *cfg = NULL;
	struct atf_ac_config *atfac = NULL;
	struct atf_context *ac = NULL;
	struct wlan_lmac_if_atf_tx_ops *tx = NULL;
	uint8_t pdev_id;

	if ((NULL == vdev) || (NULL == tbl)) {
		atf_err("vdev or atf table are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_vdev(vdev);
	if (NULL == ac) {
		atf_err("atf context is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;
	psoc = wlan_vdev_get_psoc(vdev);
	pdev = wlan_vdev_get_pdev(vdev);
	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	tx = &psoc->soc_cb.tx_ops.atf_tx_ops;

	if (cfg->percentage_unit == 0)
		leftperuint = 1000;
	else
		leftperuint = cfg->percentage_unit;

	/* Update the atf status from ic struct */
	/* Dynamic enable/disable is not supported in offload arch */
	if (ac->atf_get_dynamic_enable_disable)
		tbl->atf_status = ac->atf_get_dynamic_enable_disable(pa);
	else
		tbl->atf_status = -1;

	if ((cfg->grp_num_cfg) && (pa->atf_ssidgroup)) {
		retv = atf_group_table(vdev, tbl);
	} else {
		tbl->info_cnt = 0;
		k = 0;
		for (i = 0; i < cfg->vdev_num_cfg; i++) {
			atfac = &cfg->vdev[i].atf_cfg_ac;
			init_flag = 0;
			for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
				peer = wlan_objmgr_get_peer(psoc, pdev_id,
							    cfg->peer_id[j].sta_mac,
							    WLAN_ATF_ID);
				if ((peer != NULL) && !atf_valid_peer(peer)) {
					wlan_objmgr_peer_release_ref(peer,
								   WLAN_ATF_ID);
					continue;
				}
				if (cfg->peer_id[j].index_vdev != (i + 1)) {
					if (peer)
						wlan_objmgr_peer_release_ref(
							peer, WLAN_ATF_ID);
					continue;
				}
				if (init_flag == 0) {
					l = tbl->info_cnt;
					len = qdf_str_len(cfg->vdev[i].essid);
					qdf_mem_copy(tbl->atf_info[l].ssid,
						     cfg->vdev[i].essid, len);
					tbl->atf_info[l].value =
						cfg->vdev[i].vdev_cfg_value;
					tbl->atf_info[l].cfg_value =
						cfg->vdev[i].vdev_cfg_value;
					tbl->atf_info[l].info_mark = 0;
					tbl->atf_info[l].atf_ac_cfg =
						atfac->ac_bitmap;
					tbl->info_cnt++;
					init_flag++;
					vdev_index[i] = i + 1;
					k += 1;
				}
				sta_mac = &cfg->peer_id[j].sta_mac[0];
				tbl->atf_info[tbl->info_cnt].value =
						cfg->peer_id[j].sta_cal_value;
				len = qdf_str_len(cfg->vdev[i].essid);
				l = tbl->info_cnt;
				qdf_mem_copy(tbl->atf_info[l].ssid,
					     cfg->vdev[i].essid, len);
				tbl->atf_info[l].cfg_value =
						cfg->peer_id[j].sta_cfg_value[
						cfg->peer_id[j].index_vdev];
				/* show global configuration if we don't have
				vap specific percentage */
				if (!tbl->atf_info[l].cfg_value) {
					tbl->atf_info[l].cfg_value =
						cfg->peer_id[j].sta_cfg_value[
							ATF_CFG_GLOBAL_INDEX];
				}
				/* Showing ATF PER PEER TABEL */
				if (tbl->show_per_peer_table == 1) {
					atf_debug("\n::ATF PER PEER[%d]-%s "
						"PERCENTAGE TABLE::\n", j,
						ether_sprintf(sta_mac));
					qdf_print("__________________________"
					"_________________________________");
					for (z = 0; z <= ATF_CFG_NUM_VDEV; z++)
						qdf_print("%d ", cfg->peer_id[
							j].sta_cfg_value[z]);
					qdf_print("\n__________________________"
					"_________________________________");
				}
				tbl->atf_info[l].assoc_status =
					cfg->peer_id[j].sta_assoc_status;
				qdf_mem_copy(tbl->atf_info[l].sta_mac,
					     sta_mac, QDF_MAC_ADDR_SIZE);
				tbl->atf_info[l].info_mark = 1;
				if ((peer != NULL) && tx->atf_tokens_used) {
					tbl->atf_info[l].all_tokens_used
					= tx->atf_tokens_used(pdev, peer);
				} else {
					tbl->atf_info[l].all_tokens_used = 0;
				}
				tbl->info_cnt++;
				if (peer)
					wlan_objmgr_peer_release_ref(peer,
								   WLAN_ATF_ID);
			}
			leftperuint -= cfg->vdev[i].vdev_cfg_value;
		}
		if (cfg->vdev_num_cfg != k) {
			for (i = 0; i < cfg->vdev_num_cfg; i++) {
				if ((cfg->vdev[i].cfg_flag == 1) &&
				    (vdev_index[i] == 0)) {
					len = qdf_str_len(cfg->vdev[i].essid);
					l = tbl->info_cnt;
					qdf_mem_copy(tbl->atf_info[l].ssid,
						     cfg->vdev[i].essid, len);
					tbl->atf_info[l].value =
						cfg->vdev[i].vdev_cfg_value;
					tbl->atf_info[l].cfg_value =
						cfg->vdev[i].vdev_cfg_value;
					tbl->atf_info[l].info_mark = 0;
					tbl->atf_info[l].atf_ac_cfg =
						cfg->vdev[i].atf_cfg_ac.ac_bitmap;
					tbl->info_cnt++;
				}
			}
		}
		qdf_mem_copy(tbl->atf_info[tbl->info_cnt].ssid, "Others   ",
			     qdf_str_len(OTHER_SSID));
		tbl->atf_info[tbl->info_cnt].value = leftperuint;
		tbl->atf_info[tbl->info_cnt].cfg_value = 0;
		tbl->atf_info[tbl->info_cnt].info_mark = 0;
		tbl->info_cnt++;
		for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
			peer = wlan_objmgr_get_peer(psoc, pdev_id,
						    cfg->peer_id[j].sta_mac,
						    WLAN_ATF_ID);
			if ((peer != NULL) && !atf_valid_peer(peer)) {
				wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
				continue;
			} else if (cfg->peer_id[j].index_vdev == 0xff) {
				sta_mac = &cfg->peer_id[j].sta_mac[0];
				l = tbl->info_cnt;
				tbl->atf_info[l].value =
						cfg->peer_id[j].sta_cal_value;
				tbl->atf_info[l].cfg_value =
						cfg->peer_id[j].sta_cfg_value[
							ATF_CFG_GLOBAL_INDEX];
				tbl->atf_info[l].assoc_status =
					cfg->peer_id[j].sta_assoc_status;
				qdf_mem_copy(tbl->atf_info[l].sta_mac, sta_mac,
					     QDF_MAC_ADDR_SIZE);
				qdf_mem_copy(tbl->atf_info[l].ssid, "Others   ",
					     strlen(OTHER_SSID));
				tbl->atf_info[l].info_mark = 1;
				if ((peer != NULL) && tx->atf_tokens_used) {
					tbl->atf_info[l].all_tokens_used =
					tx->atf_tokens_used(pdev, peer);
				} else {
					tbl->atf_info[l].all_tokens_used = 0;
				}
				tbl->info_cnt++;
			}
			tbl->busy = pa->atf_chbusy;
			if (peer)
				wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
		}
	}

	return retv;
}

int32_t ucfg_atf_show_airtime(struct wlan_objmgr_vdev *vdev,
			      struct atftable *buf)
{
	struct pdev_atf *pa = NULL;
	uint8_t *sta_mac = NULL;
	uint8_t i = 0, j = 0;

	if ((NULL == vdev) || (NULL == buf)) {
		atf_err("vdev or atf table are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}

	for (i = 0, buf->info_cnt = 0; i < ATF_ACTIVED_MAX_CLIENTS; i++) {
		if (pa->atfcfg_set.peer_id[i].index_vdev != 0) {
			sta_mac = &pa->atfcfg_set.peer_id[i].sta_mac[0];
			j = buf->info_cnt;
			buf->atf_info[j].value =
				pa->atfcfg_set.peer_id[i].sta_cal_value;
			buf->atf_info[j].cfg_value =
					pa->atfcfg_set.peer_id[i].sta_cfg_value[
							ATF_CFG_GLOBAL_INDEX];
			buf->atf_info[j].assoc_status =
				pa->atfcfg_set.peer_id[i].sta_assoc_status;
			qdf_mem_copy(buf->atf_info[j].sta_mac, sta_mac,
				     QDF_MAC_ADDR_SIZE);
			buf->atf_info[j].info_mark = 1;
			buf->info_cnt++;
		}
	}

	return 0;
}

int32_t ucfg_atf_flush_table(struct wlan_objmgr_vdev *vdev)
{
	struct pdev_atf *pa = NULL;
	struct atf_context *ac = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	struct atf_config *atfcfg = NULL;
	struct atf_group_list *group = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;

	if (NULL == vdev) {
		atf_err("vdev is NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_vdev(vdev);
	if (NULL == ac) {
		atf_err("atf context is NULL\n");
		return -1;
	}
	pdev = wlan_vdev_get_pdev(vdev);
	if (NULL == pdev) {
		atf_err("pdev is NULL!\n");
		return -1;
	}

	atfcfg = &pa->atfcfg_set;
	qdf_mem_zero(&(atfcfg->peer_id[0]),
		     (sizeof(atfcfg->peer_id[0]) * ATF_ACTIVED_MAX_CLIENTS));
	qdf_mem_zero(&(atfcfg->vdev[0]),
		     (sizeof(atfcfg->vdev[0]) * ATF_CFG_NUM_VDEV));
	qdf_mem_zero(&(atfcfg->atfgroup[1]),
		     (sizeof(atfcfg->vdev[0]) * (ATF_ACTIVED_MAX_ATFGROUPS - 1)));
	atfcfg->grp_num_cfg = 1;
	atfcfg->peer_num_cfg = 0;
	atfcfg->peer_num_cal = 0;
	atfcfg->peer_cal_bitmap = 0;
	atfcfg->vdev_num_cfg = 0;

	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		next_node = NULL;
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		if (group) {
			if (!atf_config_is_default_group(group->group_name)) {
				atf_mark_del_all_sg(group);
				group->group_del = 1;
			}
		}
		node = next_node;
	}
	qdf_timer_mod(&pa->atfcfg_timer, 0);
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  atf_vdev_init, 0, 1, WLAN_ATF_ID);

	return 0;
}

int32_t ucfg_atf_add_group(struct wlan_objmgr_vdev *vdev, struct atf_group *buf)
{
	struct pdev_atf *pa = NULL;
	uint8_t groupindex = 0;
	int32_t err = 0;
	uint8_t addssid = 1, addgroup = 1;
	struct atf_config *cfg = NULL;
	struct atf_context *ac = NULL;
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;

	if ((NULL == vdev) || (NULL == buf)) {
		atf_err("vdev or atf group are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_vdev(vdev);
	if (NULL == ac) {
		atf_err("atf context is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;

	if (!pa->atf_ssidgroup) {
		atf_err("SSID grouping need to be enabled before group"
							" configuration\n");
		return -1;
	}
	if ((pa->atf_sched & ATF_SCHED_STRICT) &&
	    (!(pa->atf_sched & ATF_GROUP_SCHED_POLICY))) {
		atf_err("\nStrict queue within groups is enabled and fair"
			" queue across groups is enabled. Invalid combination."
						" Cannot addatfgroup\n");
		return -1;
	}

	if (qdf_str_len(buf->name) == qdf_str_len(DEFAULT_GROUPNAME) &&
	    !qdf_str_cmp(buf->name, DEFAULT_GROUPNAME)) {
		atf_err("Group name can't be%s\n", buf->name);
		return -1;
	}

	/* Check if group already exists */
	if (atf_config_is_group_exist(cfg, buf->name, &groupindex)) {
		addgroup = 0;
		group = cfg->atfgroup[groupindex].grplist_entry;
	}
	/* check if ssid is already part of the group */
	if (atf_config_ssid_in_group(pa, buf->ssid)) {
		addssid = 0;
		err = 1;
	}

	/* Create New Group & Increment number of groups */
	if (addgroup) {
		if (cfg->grp_num_cfg >= ATF_ACTIVED_MAX_ATFGROUPS) {
			atf_err("Error! Cannot configure more than %d "
					"groups\n", ATF_ACTIVED_MAX_ATFGROUPS);
			return -1;
		}
		groupindex = cfg->grp_num_cfg;
		atf_config_add_pdev_group(pa, buf->name);
		/* Create a new entry in the group list */
		group = atf_add_new_group(pa, buf->name);
		if (!group) {
			atf_config_del_pdev_group(pa, groupindex);
			atf_info(" Failed to add group\n");
			err = 1;
		}

		subgroup = atf_add_new_subgroup(pa, group, NULL,
			buf->value, ATF_SUBGROUP_TYPE_SSID);
		if (subgroup == NULL) {
			/* Delete group entry if subgroup cannot be added */
			atf_info("Failed to add group \n");
			atf_config_del_pdev_group(pa, groupindex);
			atf_group_delete(pa, buf->name);
			err = 1;
		}
	}

	/* add new ssid to the group & increment number of ssids in the group */
	if (addssid) {
		if (cfg->atfgroup[groupindex].grp_num_ssid >=
		    ATF_CFG_NUM_VDEV) {
			atf_err("Error! Cannot configure more than %d SSIDs "
						"in the group\n", CFG_NUM_VDEV);
			return -1;
		}
		atf_config_ssid_group(pa, buf->ssid, groupindex, group);
	}

	return err;
}

int32_t ucfg_atf_config_group(struct wlan_objmgr_vdev *vdev,
			      struct atf_group *buf)
{
	uint32_t i = 0, err = 0, airtime_group = 0;
	uint32_t availableairtime = 0, len = 0;
	uint8_t *gname = 0;
	struct pdev_atf *pa = NULL;
	struct atf_config *cfg = NULL;
	struct atf_context *ac = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	struct atf_group_list *group = NULL;
	qdf_list_node_t *node = NULL;

	if ((NULL == vdev) || (NULL == buf)) {
		atf_err("vdev or atf group are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_vdev(vdev);
	if (NULL == ac) {
		atf_err("atf context is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;

	if (cfg->percentage_unit == 0)
		availableairtime = 1000;
	else
		availableairtime = cfg->percentage_unit;

	if (!pa->atf_ssidgroup) {
		atf_err("SSID grouping need to be enabled before group "
							"configuration\n");
		return -1;
	}
	/* Find the Total Airtime configured (for all groups) */
	for (i = 0; i < cfg->grp_num_cfg; i++) {
		if (!atf_config_is_default_group(cfg->atfgroup[i].grpname))
			airtime_group += cfg->atfgroup[i].grp_cfg_value;
	}

	for (i = 0; i < cfg->grp_num_cfg; i++) {
		/* Check if group exists */
		gname = cfg->atfgroup[i].grpname;
		len = qdf_str_len(gname);
		if ((len == qdf_str_len(buf->name)) &&
		    !qdf_str_cmp(gname, (char *)(buf->name))) {
			if ((airtime_group - cfg->atfgroup[i].grp_cfg_value +
					buf->value) <= availableairtime) {
				atf_config_group_val_sched(pa, buf->value, i);
				/*Add group airtime in the group list*/
				group = cfg->atfgroup[i].grplist_entry;
                                atf_add_group_airtime(group, buf->value);
				qdf_list_peek_front(&group->atf_subgroups, &node);
				if (node) {
					subgroup = qdf_container_of(node,
					struct atf_subgroup_list, sg_next);
					if (subgroup) {
						subgroup->usr_cfg_val = buf->value;
					}
				}
			} else {
				atf_err("Error!!Total Airtime configured for"
				" groups cannot be greater than %d\n",
							availableairtime / 10);
				err = 1;
			}
			break;
		}
	}

	if (i == cfg->grp_num_cfg) {
		atf_err("Error!! Invalid group name\n");
		err = 1;
	}

	return err;
}

int ucfg_atf_group_sched(struct wlan_objmgr_vdev *vdev, struct atf_group *buf)
{
	u_int32_t error = 0, i = 0;
	struct pdev_atf *pa = NULL;
	struct atf_config *cfg = NULL;
	struct atf_context *ac = NULL;
	struct atf_group_list *group = NULL;

	if ((NULL == vdev) || (NULL == buf)) {
		atf_err("vdev or atf group are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_vdev(vdev);
	if (NULL == ac) {
		atf_err("atf context is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;

	for (i = 0; i < cfg->grp_num_cfg; i++) {
		/* Check if group exists */
		if ((qdf_str_len(cfg->atfgroup[i].grpname) == qdf_str_len(buf->name)) &&
			!(qdf_str_cmp((char *)(cfg->atfgroup[i].grpname),
				      (char *)(buf->name)))) {
			cfg->atfgroup[i].grp_sched_policy = buf->value;
			/* Add group scheduling policy in the group list */
			group = cfg->atfgroup[i].grplist_entry;
			if (group != NULL) {
				group->group_sched = buf->value;
			}
			else {
				atf_err("Error!! Not a valid scheduling Policy\n");
				error = 1;
			}
			break;
		}
	}
	if (i == cfg->grp_num_cfg) {
		atf_err("Error!! Invalid group name\n\r");
		error = 1;
	}

	return error;
}

int32_t ucfg_atf_delete_group(struct wlan_objmgr_vdev *vdev,
			      struct atf_group *buf)
{
	uint32_t i = 0, err = 0, delgroup = 0, len = 0;
	uint8_t *gname = 0;
	struct pdev_atf *pa = NULL;
	struct atf_config *cfg = NULL;
	struct atf_context *ac = NULL;

	if ((NULL == vdev) || (NULL == buf)) {
		atf_err("vdev or atf group are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_vdev(vdev);
	if (NULL == ac) {
		atf_err("atf context is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;

	if (!pa->atf_ssidgroup) {
		atf_err("SSID grouping need to be enabled before group "
							"configuration\n");
		return -1;
	}

	if ((qdf_str_len(buf->name) == qdf_str_len(DEFAULT_GROUPNAME)) &&
	    !qdf_str_cmp(buf->name, DEFAULT_GROUPNAME)) {
		atf_err("Group '%s' cannot be deleted\n\r", buf->name);
		return -1;
	}

	for (i = 0; i < cfg->grp_num_cfg; i++) {
		/* Check if group exists */
		gname = cfg->atfgroup[i].grpname;
		len = qdf_str_len(gname);
		if ((len == qdf_str_len(buf->name)) &&
		    !qdf_str_cmp(gname, (char *)(buf->name))) {
			delgroup = 1;
			/* Clear Elements at this index */
			qdf_mem_zero(&cfg->atfgroup[i],
				     sizeof(cfg->atfgroup[0]));
			/* Copy the 'i+1' the element to 'i'th position & clear
			 * the last element
			 */
			if (i != (cfg->grp_num_cfg - 1)) {
				qdf_mem_copy(&cfg->atfgroup[i],
					     &cfg->atfgroup[i+1],
					     (sizeof(cfg->atfgroup[0]) *
					     (cfg->grp_num_cfg - i - 1)));
			}
			/* Clear Last Element */
			qdf_mem_zero(&cfg->atfgroup[cfg->grp_num_cfg - 1],
				     sizeof(cfg->atfgroup[0]));
			cfg->grp_num_cfg--;
			/* Parse the group list and mark for deletion
			 * The group entry will be removed from the
			 * list in cal_atf_alloc_tbl routine
			 */
			atf_group_delete(pa, buf->name);
			break;
		}
	}

	if (!delgroup) {
		atf_err("Error!! Invalid group name.\n");
		err = 1;
	}

	return err;
}

int32_t ucfg_atf_show_group(struct wlan_objmgr_vdev *vdev,
			    struct atfgrouptable *buf)
{
	uint32_t i = 0, k = 0;
	struct pdev_atf *pa = NULL;
	struct atf_config *cfg = NULL;
	struct atf_context *ac = NULL;

	if ((NULL == vdev) || (NULL == buf)) {
		atf_err("vdev or atf group are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_vdev(vdev);
	if (NULL == ac) {
		atf_err("atf context is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;

	if (!pa->atf_ssidgroup) {
		atf_err("SSID grouping need be enabled for this command to "
								"work\n\r");
		return -1;
	}

	if (!cfg->grp_num_cfg) {
		atf_err("No groups configured\n");
		return -1;
	}

	for (i = 0; i < cfg->grp_num_cfg; i++) {
		qdf_mem_copy(buf->atf_groups[buf->info_cnt].grpname,
			     cfg->atfgroup[i].grpname, WLAN_SSID_MAX_LEN);
		buf->atf_groups[buf->info_cnt].grp_num_ssid =
						cfg->atfgroup[i].grp_num_ssid;
		buf->atf_groups[buf->info_cnt].grp_cfg_value =
						cfg->atfgroup[i].grp_cfg_value;
		buf->atf_groups[buf->info_cnt].grp_sched =
						cfg->atfgroup[i].grp_sched_policy;
		for (k = 0; k < cfg->atfgroup[i].grp_num_ssid; k++) {
			qdf_mem_copy(buf->atf_groups[buf->info_cnt].grp_ssid[k],
				     cfg->atfgroup[i].grp_ssid[k],
				     WLAN_SSID_MAX_LEN);
		}
		buf->info_cnt++;
	}

	return 0;
}

int32_t ucfg_atf_show_subgroup(struct wlan_objmgr_vdev *vdev,
			       struct atfgrouplist_table *buf)
{
	uint32_t i = 0, k = 0;
	struct pdev_atf *pa = NULL;
	struct atf_config *cfg = NULL;
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	struct atf_context *ac = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if ((NULL == vdev) || (NULL == buf)) {
		atf_err("vdev or atf group are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_vdev(vdev);
	if (NULL == ac) {
		atf_err("atf context is NULL\n");
		return -1;
	}
	cfg = &pa->atfcfg_set;
	if (!cfg->grp_num_cfg) {
		atf_info("No groups configured\n");
		return -1;
	}
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
	    i = 0;
	    k = buf->info_cnt;
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		next_node = NULL;
		qdf_mem_copy((char *)(buf->atf_list[k].grpname),
			     (char *)group->group_name, WLAN_SSID_MAX_LEN);
		buf->atf_list[k].grp_value = group->group_airtime;
		qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
		while (tmpnode) {
			subgroup = qdf_container_of(tmpnode,
					struct atf_subgroup_list, sg_next);
			tmpnode_next = NULL;
			i = buf->atf_list[buf->info_cnt].num_subgroup;
			qdf_mem_copy(buf->atf_list[k].sg_table[i].subgrpname,
				     (char*)subgroup->sg_name,
				     WLAN_SSID_MAX_LEN);
			buf->atf_list[k].sg_table[i].subgrp_type =
							subgroup->sg_type;
			buf->atf_list[k].sg_table[i].subgrp_value =
							subgroup->sg_atf_units;
			if (subgroup->sg_type == ATF_SUBGROUP_TYPE_PEER) {
			    qdf_mem_copy(buf->atf_list[k].sg_table[i].peermac,
					 subgroup->sg_peermac, QDF_MAC_ADDR_SIZE);
			} else if(subgroup->sg_type == ATF_SUBGROUP_TYPE_AC) {
			    buf->atf_list[k].sg_table[i].subgrp_ac_id =
				    				subgroup->ac_id;
			}
			buf->atf_list[k].sg_table[i].subgrp_cfg_value =
							subgroup->usr_cfg_val;
			buf->atf_list[buf->info_cnt].num_subgroup++;

			qdf_list_peek_next(&group->atf_subgroups, tmpnode,
					   &tmpnode_next);
			tmpnode = tmpnode_next;
		}
		buf->info_cnt++;
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		node = next_node;
	}

	return 0;
}

int32_t ucfg_atf_add_sta_tput(struct wlan_objmgr_vdev *vdev,
			      struct sta_val *buf)
{
	uint8_t i = 0;
	uint8_t mac[QDF_MAC_ADDR_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	struct pdev_atf *pa = NULL;
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL;

	if ((NULL == vdev) || (NULL == buf)) {
		atf_err("vdev or sta val are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}

	if (pa->atf_tput_based) {
		atf_err("throughput based airtime allocation in "
		"use, disable atf and try later\n");
		return -1;
	}

	if (!qdf_mem_cmp(buf->sta_mac, mac, QDF_MAC_ADDR_SIZE)) {
		/* Mac Addr of all 0xFF is used for reserved airtime */
		pa->atf_resv_airtime = (buf->value & ATF_AIRTIME_MASK) >>
							ATF_AIRTIME_SHIFT;
		if (pa->atf_resv_airtime > 100)
			pa->atf_resv_airtime = 100;

		atf_info("reserved airtime %u\n",
			    pa->atf_resv_airtime);
	} else {
		qdf_mem_zero(mac, QDF_MAC_ADDR_SIZE);
		for (i = 0; i < ATF_TPUT_MAX_STA; i++) {
			if (!qdf_mem_cmp(pa->atf_tput_tbl[i].mac_addr,
			    buf->sta_mac, QDF_MAC_ADDR_SIZE)) {
				/* Update existing entry */
				pa->atf_tput_tbl[i].tput = buf->value &
								ATF_TPUT_MASK;
				pa->atf_tput_tbl[i].airtime = (buf->value &
					ATF_AIRTIME_MASK) >> ATF_AIRTIME_SHIFT;

				if (pa->atf_tput_tbl[i].airtime > 100)
					pa->atf_tput_tbl[i].airtime = 100;

				atf_info("%02x:%02x:%02x:%02x:%02x:%02x "
				"has minimum throughput %u, max airtime %u, num"
				" entries %u\n",
						pa->atf_tput_tbl[i].mac_addr[0],
						pa->atf_tput_tbl[i].mac_addr[1],
						pa->atf_tput_tbl[i].mac_addr[2],
						pa->atf_tput_tbl[i].mac_addr[3],
						pa->atf_tput_tbl[i].mac_addr[4],
						pa->atf_tput_tbl[i].mac_addr[5],
						pa->atf_tput_tbl[i].tput,
						pa->atf_tput_tbl[i].airtime,
						pa->atf_tput_tbl_num);
				break;
			}
		}

		if (i == ATF_TPUT_MAX_STA) {
			for (i = 0; i < ATF_TPUT_MAX_STA; i++) {
				if (!qdf_mem_cmp(pa->atf_tput_tbl[i].mac_addr,
				    mac, QDF_MAC_ADDR_SIZE)) {
					/* Create new entry */
					qdf_mem_copy(
					pa->atf_tput_tbl[i].mac_addr,
					buf->sta_mac, QDF_MAC_ADDR_SIZE);

					pa->atf_tput_tbl[i].tput = buf->value &
								ATF_TPUT_MASK;
					pa->atf_tput_tbl[i].airtime =
					(buf->value & ATF_AIRTIME_MASK) >>
							ATF_AIRTIME_SHIFT;
					if (pa->atf_tput_tbl[i].airtime > 100)
						pa->atf_tput_tbl[i].airtime =
									100;
					pa->atf_tput_tbl_num++;
					atf_info("%02x:%02x:%02x:%02x:%02x"
					":%02x has minimum throughput %u, max "
					"airtime %u,num entries %u\n",
						pa->atf_tput_tbl[i].mac_addr[0],
						pa->atf_tput_tbl[i].mac_addr[1],
						pa->atf_tput_tbl[i].mac_addr[2],
						pa->atf_tput_tbl[i].mac_addr[3],
						pa->atf_tput_tbl[i].mac_addr[4],
						pa->atf_tput_tbl[i].mac_addr[5],
						pa->atf_tput_tbl[i].tput,
						pa->atf_tput_tbl[i].airtime,
							pa->atf_tput_tbl_num);
						qdf_list_peek_front(&pa->atf_groups, &node);
						if (node) {
							group = qdf_container_of(node,
								struct atf_group_list, group_next);
							if (group) {
								subgroup = atf_add_new_subgroup(pa, group, buf->sta_mac,
									0,	ATF_SUBGROUP_TYPE_PEER);
								if (!subgroup) {
									atf_err("Error! Failed to create subgroup\n");
								}
							} else {
								atf_err("Default group could not be found. "
									 "Failed to create new subgroup\n");
							}
						}
					break;
				}
			}

			if (i == ATF_TPUT_MAX_STA) {
				atf_info("out of entries\n");
				return -1;
			}
		}
	}

	return 0;
}

int32_t ucfg_atf_delete_sta_tput(struct wlan_objmgr_vdev *vdev,
				 struct sta_val *buf)
{
	uint8_t i = 0;
	uint8_t mac[QDF_MAC_ADDR_SIZE] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	struct pdev_atf *pa = NULL;

	if ((NULL == vdev) || (NULL == buf)) {
		atf_err("vdev or sta val are NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}

	if (pa->atf_tput_based) {
		atf_err("throughput based airtime allocation in "
		"use, disable atf and try later\n");
		return -1;
	}

	if (!qdf_mem_cmp(buf->sta_mac, mac, QDF_MAC_ADDR_SIZE)) {
		/* Clear whole table */
		for (i = 0; i < ATF_TPUT_MAX_STA; i++) {
			atf_config_mark_del_peer_subgroup(pa, pa->atf_tput_tbl[i].mac_addr);
			qdf_mem_zero(pa->atf_tput_tbl[i].mac_addr,
				     QDF_MAC_ADDR_SIZE);
		}
		pa->atf_tput_tbl_num = 0;
		atf_info("flushing all entries, num entries %u\n",
							pa->atf_tput_tbl_num);
	} else {
		for (i = 0; i < ATF_TPUT_MAX_STA; i++) {
			if (!qdf_mem_cmp(pa->atf_tput_tbl[i].mac_addr,
			    buf->sta_mac, QDF_MAC_ADDR_SIZE)) {
				atf_config_mark_del_peer_subgroup(pa, pa->atf_tput_tbl[i].mac_addr);
				/* Remove one entry */
				qdf_mem_zero(pa->atf_tput_tbl[i].mac_addr,
					     QDF_MAC_ADDR_SIZE);
				pa->atf_tput_tbl_num--;
				atf_info("flushing entry,num entries%u\n",
							pa->atf_tput_tbl_num);
				break;
			}
		}
		if (i == ATF_TPUT_MAX_STA)
			atf_info("entry not found\n");
	}

	return 0;
}

int32_t ucfg_atf_show_tput(struct wlan_objmgr_vdev *vdev)
{
	uint8_t i = 0;
	uint8_t mac[QDF_MAC_ADDR_SIZE] = { 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 };
	struct pdev_atf *pa = NULL;

	if (NULL == vdev) {
		atf_err("vdev is NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}

	qdf_print("\n");
	qdf_print("reserved airtime      : %u", pa->atf_resv_airtime);
	qdf_print("number of entries     : %u", pa->atf_tput_tbl_num);
	qdf_print("tput based atf in use : %u", pa->atf_tput_based);
	qdf_print("      mac address     tput  airtime");
	qdf_print("-----------------------------------");
	for (i = 0; i < ATF_TPUT_MAX_STA; i++) {
		if (qdf_mem_cmp(pa->atf_tput_tbl[i].mac_addr, mac,
				QDF_MAC_ADDR_SIZE)) {
			qdf_print("%02x:%02x:%02x:%02x:%02x:%02x %8u %8u",
				   pa->atf_tput_tbl[i].mac_addr[0],
				   pa->atf_tput_tbl[i].mac_addr[1],
				   pa->atf_tput_tbl[i].mac_addr[2],
				   pa->atf_tput_tbl[i].mac_addr[3],
				   pa->atf_tput_tbl[i].mac_addr[4],
				   pa->atf_tput_tbl[i].mac_addr[5],
				   pa->atf_tput_tbl[i].tput,
				   pa->atf_tput_tbl[i].airtime);
		}
	}
	qdf_print("\n");

	return 0;
}

int32_t ucfg_atf_set_debug_size(struct wlan_objmgr_vdev *vdev, int32_t size)
{
	struct atf_stats *ptr = NULL;
	struct pdev_atf *pa = NULL;
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == vdev) {
		atf_err("vdev is NULL!\n");
		return -1;
	}
	if (size <= 0) {
		atf_err("Size cannot be %d\n", size);
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}

	if (size <= 16)
		size = 16;
	else if (size <= 32)
		size = 32;
	else if (size <= 64)
		size = 64;
	else if (size <= 128)
		size = 128;
	else if (size <= 256)
		size = 256;
	else if (size <= 512)
		size = 512;
	else
		size = 1024;

	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list, group_next);
		next_node = NULL;
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		if (group) {
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode, struct atf_subgroup_list,
											sg_next);
				tmpnode_next = NULL;
				qdf_list_peek_next(&group->atf_subgroups, tmpnode,
								   &tmpnode_next);
				ptr = NULL;
				qdf_spin_lock_bh(&subgroup->atf_sg_lock);
				if (subgroup->sg_atf_debug) {
					ptr = subgroup->sg_atf_debug;
				 	subgroup->sg_atf_debug = NULL;
				}
				subgroup->sg_atf_debug_mask = 0;
				subgroup->sg_atf_debug_id = 0;
				qdf_spin_unlock_bh(&subgroup->atf_sg_lock);

				if (ptr) {
					/* Free old history */
					qdf_mem_free(ptr);
					ptr = NULL;
				}

				/* Allocate new history */
				ptr = (struct atf_stats *)qdf_mem_malloc(size *
							sizeof(struct atf_stats));

				if (ptr) {
					qdf_spin_lock_bh(&subgroup->atf_sg_lock);
					subgroup->sg_atf_debug = ptr;
					subgroup->sg_atf_debug_mask = size - 1;
					subgroup->sg_atf_debug_id = 0;
					qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
				}
				tmpnode = tmpnode_next;
			}
		}
			node = next_node;
	}

	return 0;
}

int32_t ucfg_atf_get_debug_peerstate(struct wlan_objmgr_vdev *vdev,
				     uint8_t *mac, struct atf_peerstate *peerstate)
{
	struct wlan_objmgr_peer *peer = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	struct wlan_objmgr_pdev *pdev = NULL;
	uint8_t pdev_id;
	struct peer_atf *pa = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	int i;

	if ((NULL == vdev) || (NULL == mac)) {
		atf_err("vdev or mac are NULL!\n");
		return -1;
	}
	psoc = wlan_vdev_get_psoc(vdev);
	pdev = wlan_vdev_get_pdev(vdev);
	if (NULL == psoc) {
		atf_err("psoc is NULL\n");
		return -1;
	}
	if (NULL == pdev) {
		atf_err("pdev is NULL\n");
		return -1;
	}
	pdev_id = wlan_objmgr_pdev_get_pdev_id(pdev);
	peer = wlan_objmgr_get_peer(psoc, pdev_id, mac, WLAN_ATF_ID);
	if (NULL == peer) {
		atf_err("peer is NULL\n");
		return -1;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("peer atf is NULL\n");
		return -1;
	}
	if (peer) {
		if (psoc->soc_cb.tx_ops.atf_tx_ops.atf_debug_peerstate) {
			peerstate->peerstate =
			    psoc->soc_cb.tx_ops.atf_tx_ops.atf_debug_peerstate(
							 pdev, peer, peerstate);
		}
		wlan_objmgr_peer_release_ref(peer, WLAN_ATF_ID);
		for (i = 0; i < WME_NUM_AC; i++) {
			subgroup = NULL;
			subgroup = pa->ac_list_ptr[i];
			if (subgroup) {
				qdf_mem_copy(&peerstate->subgroup_name[i][0],
					     &subgroup->sg_name,
					     WLAN_SSID_MAX_LEN);
			}
		}
	}

	return 0;
}

int32_t ucfg_atf_get_lists(struct wlan_objmgr_vdev *vdev,
			   struct atfgrouplist_table *buf)
{
	struct pdev_atf *pa = NULL;
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;
	int8_t i = 0, k = 0;

	if (NULL == vdev) {
		atf_err("Vdev is NULL\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		next_node = NULL;
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		if (group) {
			k = buf->info_cnt;
			OS_MEMCPY((char *)(buf->atf_list[k].grpname),
				  (char *)group->group_name, WLAN_SSID_MAX_LEN);
		}
		qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
		while (tmpnode) {
			subgroup = qdf_container_of(tmpnode,
						    struct atf_subgroup_list,
						    sg_next);
			tmpnode_next = NULL;
			qdf_list_peek_next(&group->atf_subgroups, tmpnode,
					   &tmpnode_next);
			if (subgroup) {
				i = buf->atf_list[buf->info_cnt].num_subgroup;
				OS_MEMCPY(
					buf->atf_list[k].sg_table[i].subgrpname,
					(char*)subgroup->sg_name,
					WLAN_SSID_MAX_LEN);
					buf->atf_list[buf->info_cnt].num_subgroup++;
			}
			tmpnode = tmpnode_next;
		}
		buf->info_cnt++;
		node = next_node;
	}

	return 0;
}

int32_t ucfg_atf_get_debug_dump(struct wlan_objmgr_vdev *vdev,
				struct atf_stats *stats, void **buf,
				uint32_t *buf_sz, uint32_t *id)
{
	struct pdev_atf *pa = NULL;
	struct atf_stats *ptr = NULL;
	struct atf_group_list *group = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	int32_t size;

	if (NULL == vdev) {
		atf_err("vdev is NULL!\n");
		return -1;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf is NULL\n");
		return -1;
	}
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list, group_next);
		next_node = NULL;
		if (group) {
			if ((qdf_str_cmp(group->group_name, stats->group_name)) ||
				(qdf_str_len(group->group_name) !=
				 qdf_str_len(stats->group_name))) {
				goto group_next;
			}
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode, struct atf_subgroup_list,
											sg_next);
				tmpnode_next = NULL;
				if ((qdf_str_cmp(subgroup->sg_name, stats->subgroup_name)) ||
					(qdf_str_len(subgroup->sg_name) !=
					 qdf_str_len(stats->subgroup_name))) {
					goto subgroup_next;
				}
				qdf_spin_lock_bh(&subgroup->atf_sg_lock);
				size = subgroup->sg_atf_debug_mask + 1;
				qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
				if (size == 1)
					ptr = NULL;
				else
					ptr = (struct atf_stats *)qdf_mem_malloc(size *
									sizeof(struct atf_stats));

				if (ptr) {
					qdf_spin_lock_bh(&subgroup->atf_sg_lock);
					*buf = subgroup->sg_atf_debug;
					*buf_sz = size * sizeof(struct atf_stats);
					*id = subgroup->sg_atf_debug_id;
					subgroup->sg_atf_debug = ptr;
					subgroup->sg_atf_debug_id = 0;
					qdf_spin_unlock_bh(&subgroup->atf_sg_lock);
				}
subgroup_next:
				qdf_list_peek_next(&group->atf_subgroups, tmpnode, &tmpnode_next);
				tmpnode = tmpnode_next;
			}
		}
group_next:
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		node = next_node;
	}

	return 0;
}
