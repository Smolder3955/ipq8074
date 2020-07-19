/*
 *
 * Copyright (c) 2011-2017 Qualcomm Innovation Center, Inc.
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
#include "atf_da_api_i.h"
#include "atf_ol_api_i.h"
#include <qdf_types.h>
#include <osif_private.h>
#include <wlan_osif_priv.h>

extern QDF_STATUS
wlan_objmgr_vap_alloc_tim_bitmap(struct wlan_objmgr_vdev *vdev);

extern QDF_STATUS
wlan_objmgr_vap_alloc_aid_bitmap(struct wlan_objmgr_vdev *vdev, uint16_t value);

void atf_vdev_init(struct wlan_objmgr_pdev *pdev, void *obj, void *args)
{
	osif_dev *osifp = NULL;
	struct vdev_osif_priv *vdev_osifp = NULL;
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)obj;

	if ((NULL == pdev) || (NULL == vdev)) {
		atf_err("pdev or vdev are NULL!\n");
		return;
	}
	vdev_osifp = wlan_vdev_get_ospriv(vdev);
	if (NULL == vdev_osifp) {
		atf_err("vdev_osif is NULL\n");
		return;
	}
	osifp = (osif_dev *)vdev_osifp->legacy_osif_priv;
	if (NULL == osifp) {
		atf_err("osif_dev is NULL\n");
		return;
	}

	if (osifp->netdev && IS_UP(osifp->netdev)) {
		if (osif_vap_init(osifp->netdev, RESCAN))
			atf_err("Failed to do vdev init\n");
	}
}

static void atf_vdev_set_number_of_clients(struct wlan_objmgr_pdev *pdev,
					   void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)arg;
	struct atf_context *ac = NULL;
	osif_dev *osifp = NULL;
	struct vdev_osif_priv *vdev_osifp = NULL;
	uint16_t old_max_aid = 0, old_len = 0;
	int32_t retval = 0;

	if ((NULL == pdev) || (NULL == psoc) || (NULL == vdev)) {
		atf_err("pdev or psoc or vdev are NULL!\n");
		return;
	}
	ac = atf_get_atf_ctx_from_vdev(vdev);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return;
	}

	vdev_osifp = wlan_vdev_get_ospriv(vdev);
	old_max_aid = wlan_vdev_get_max_peer_count(vdev);
	if (NULL == vdev_osifp) {
		atf_err("vdev_osif is NULL\n");
		return;
	}

	osifp = (osif_dev *)vdev_osifp->legacy_osif_priv;
	if (NULL == osifp) {
		atf_err("osif_dev is NULL\n");
		return;
	}

	old_len = howmany(old_max_aid, 32) * sizeof(uint32_t);
	if (osifp->netdev && IS_UP(osifp->netdev)) {
		if (osif_vap_init(osifp->netdev, RESCAN))
			atf_err("Failed to do vdev init\n");
	}

	/* We will reject station when associated aid >= iv_max_aid,
	 * such that max associated station should be value + 1
	 */
	wlan_vdev_set_max_peer_count(vdev, ac->numclients);

	/* The interface is up, we may need to
	 * reallocation bitmap(tim, aid)
	 */
	if (osifp->netdev && IS_UP(osifp->netdev)) {
		retval = wlan_objmgr_vap_alloc_tim_bitmap(vdev);
		if (retval == QDF_STATUS_SUCCESS)
			retval = wlan_objmgr_vap_alloc_aid_bitmap(vdev,
								old_len);
	}
	if (retval != QDF_STATUS_SUCCESS) {
		atf_info("Error! Failed to change the number of max clients"
						" to %d\n", ac->numclients);
		wlan_vdev_set_max_peer_count(vdev, old_max_aid);
	}
}

/* Definition */
#define ATF_INVALID_MAC(addr) \
	((!addr[0]) && (!addr[1]) && (!addr[2]) && \
	(!addr[3]) && (!addr[4]) && (!addr[5]))

/**
 * @brief Check if the peer if valid
 *
 * @param [in] node table
 *
 * @return node table entry
 */
struct wlan_objmgr_peer *atf_valid_peer(struct wlan_objmgr_peer *peer)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	uint8_t *mac = NULL;

	mac = wlan_peer_get_macaddr(peer);
	vdev = wlan_peer_get_vdev(peer);
	if (ATF_INVALID_MAC(mac))
		goto peer_invalid;
	/* skip peers that aren't attached to a VDEV */
	if (vdev == NULL)
		goto peer_invalid;
	/* skip non-AP vdevs */
	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_SAP_MODE) {
		goto peer_invalid;
	}
	/* skip NAWDS-AP vdevs */
	/* skip AP BSS peer */
	if (WLAN_PEER_AP == wlan_peer_get_peer_type(peer)) {
		goto peer_invalid;
	}

	return peer;

peer_invalid:
	return NULL;
}

int32_t
atf_update_number_of_clients(struct wlan_objmgr_pdev *pdev, int32_t value)
{
	struct pdev_atf *pa = NULL;
	struct atf_context *ac = NULL;
	struct wlan_objmgr_psoc *psoc = NULL;
	int32_t retval = 0;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return -1;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	psoc = wlan_pdev_get_psoc(pdev);
	if (NULL == pa) {
		atf_err("pdev_atf object is NULL!\n");
		return -1;
	}
	if (NULL == psoc) {
		atf_err("PSOC is NULL!\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_pdev(pdev);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return -1;
	}

	if (value > 0) {
		ac->numclients = value;
		wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
						atf_vdev_set_number_of_clients,
						psoc, 1, WLAN_ATF_ID);
		pa->atf_num_clients = ac->numclients;
	}
	if (pa->atf_tput_tbl_num && ac->atf_reset_vdev)
		ac->atf_reset_vdev(pdev);

	return retval;
}

QDF_STATUS atf_config_ssid_group(struct pdev_atf *pa, uint8_t *ssid,
				 uint32_t index, struct atf_group_list *group)
{
	uint32_t groupssidindex;
	struct atf_config *cfg = NULL;

	if(NULL == pa) {
		atf_err("Pdev ATF object is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (NULL == group) {
		atf_err("Group is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	cfg = &pa->atfcfg_set;
	if(index >= 0 && index < ATF_ACTIVED_MAX_ATFGROUPS) {
		if (cfg->atfgroup[index].grp_num_ssid >= ATF_CFG_NUM_VDEV) {
			atf_err(" Num SSID > NUM VDEV\n");
			return QDF_STATUS_E_FAILURE;
		}
	}
	groupssidindex = cfg->atfgroup[index].grp_num_ssid;
	qdf_str_lcopy(cfg->atfgroup[index].grp_ssid[groupssidindex],
			 (uint8_t *) ssid, WLAN_SSID_MAX_LEN+1);
	cfg->atfgroup[index].grp_num_ssid++;
	/* Add reference to group list entry in config group structure */
	cfg->atfgroup[index].grplist_entry = group;

	return  QDF_STATUS_SUCCESS;
}

QDF_STATUS atf_config_group_val_sched(struct pdev_atf *pa, uint32_t value,
				      uint32_t groupindex)
{
	struct atf_config *cfg = NULL;

	if(NULL == pa) {
		atf_err("Pdev ATF object is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	cfg = &pa->atfcfg_set;
	cfg->atfgroup[groupindex].grp_sched_policy = ATF_GROUP_SCHED_FAIR;
	cfg->atfgroup[groupindex].grp_cfg_value = value;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS atf_config_mark_del_peer_subgroup(struct pdev_atf *pa, void *mac)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if(NULL == pa) {
		atf_err("Pdev ATF object is NULL");
		return QDF_STATUS_E_FAILURE;
	}
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
					     struct atf_subgroup_list, sg_next);
				tmpnode_next = NULL;
				qdf_list_peek_next(&group->atf_subgroups,
						   tmpnode, &tmpnode_next);
				if (atf_config_is_subgroup_exist(
						&pa->atfcfg_set, group, mac,
                                                &subgroup,
                                                ATF_SUBGROUP_TYPE_PEER)) {
						subgroup->sg_del = 1;
				}
				tmpnode = tmpnode_next;
			}
		}
		node = next_node;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS atf_mark_del_all_sg(struct atf_group_list *group)
{
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *tmpnode = NULL;
	qdf_list_node_t *tmpnode_next = NULL;

	if (!group || qdf_list_empty(&group->atf_subgroups)) {
		atf_err("Group is NULL or subgroup list is empty");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
	while(tmpnode) {
		subgroup = qdf_container_of(tmpnode, struct atf_subgroup_list,
					    sg_next);
		subgroup->sg_del = 1;
		tmpnode_next = NULL;
		group->num_subgroup--;
		if (QDF_STATUS_SUCCESS !=
			qdf_list_peek_next(&group->atf_subgroups, tmpnode,
					   &tmpnode_next))
			break;
		tmpnode = tmpnode_next;
	}

	return QDF_STATUS_SUCCESS;
}

int8_t atf_config_is_default_subgroup(uint8_t *sgname)
{
	int8_t ret = FALSE;

	if (!sgname) {
		atf_err("Subgroup name does not exist\n");
		return FALSE;
	}
	if (!(qdf_str_cmp(sgname, DEFAULT_SUBGROUPNAME)) &&
	    (qdf_str_len(sgname) == qdf_str_len(DEFAULT_SUBGROUPNAME))) {
		ret = TRUE;
	}

	return FALSE;
}

int8_t atf_config_is_default_group(uint8_t *grpname)
{
	int8_t ret = FALSE;

	if (!grpname) {
		return FALSE;
	}
	if (!(qdf_str_cmp(grpname, DEFAULT_GROUPNAME)) &&
	    (qdf_str_len(grpname) == qdf_str_len(DEFAULT_GROUPNAME))) {
		ret = TRUE;
	}

	return ret;
}

int8_t atf_config_is_group_exist(struct atf_config *cfg, uint8_t *gname,
				   uint8_t *gindex)
{
	uint8_t i = 0;

	if (!gname || !cfg) {
		atf_err("Group name or atf config is NULL. Returning \n");
		return FALSE;
	} else if (!gindex) {
		atf_err("Group index is NULL. Returning \n");
		return FALSE;
	}
	for (i = 0; i < cfg->grp_num_cfg; i++) {
		/* Check if group already exists */
		if ((qdf_str_len(cfg->atfgroup[i].grpname) ==
		     qdf_str_len(gname)) &&
		     (!qdf_str_cmp((char *)(cfg->atfgroup[i].grpname),
				   (uint8_t *)(gname)))) {
			*gindex = i;
			return TRUE;
		}
	}

	return FALSE;
}

int32_t atf_subgroup_match_peer(struct atf_subgroup_list *sgroup, char *mac)
{
	if (!sgroup || !mac) {
		atf_err("Subgroup or MAC is NULL");
		return FALSE;
	}

	if (sgroup->sg_type == ATF_SUBGROUP_TYPE_PEER &&
	    !qdf_mem_cmp((char *)(sgroup->sg_peermac), mac,
			     QDF_MAC_ADDR_SIZE)) {
		return TRUE;
	}

	return FALSE;
}

int32_t atf_subgroup_match_ac(struct atf_subgroup_list *sgroup, u_int8_t ac_id)
{
	if (!sgroup) {
		atf_err("Subgroup is NULL");
		return FALSE;
	}

	if (sgroup->sg_type == ATF_SUBGROUP_TYPE_AC &&
	    ac_id == sgroup->ac_id) {
		return TRUE;
	}

	return FALSE;
}

QDF_STATUS atf_validate_acval(struct atf_ac_config *atf_ac, int8_t ac_id,
			      uint32_t ac_val)
{
	int i = 0;
	int ac_cumulative = 0;

	if (!atf_ac) {
		atf_err("ATF AC config is NULL");
		return QDF_STATUS_E_INVAL;
	}

	for (i = 0; i < WME_NUM_AC; i++) {
		if ((i != ac_id) &&
		(1 & (atf_ac->ac_bitmap >> i))) {
			ac_cumulative += atf_ac->ac[i][0];
		}
	}

	if (ac_cumulative + ac_val > PER_UNIT_1000) {
		atf_err("%s Total Airtime for ACs %d > 1000 . Error \n",
			__func__, ac_cumulative + ac_val);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

int8_t atf_config_is_subgroup_exist(struct atf_config *cfg,
				      struct atf_group_list *group, void *param,
				      struct atf_subgroup_list **subgroup,
				      int8_t cfg_type)
{
	struct atf_subgroup_list *sgroup = NULL;
	qdf_list_node_t *tmpnode = NULL;
	qdf_list_node_t *tmpnode_next = NULL;

	if (!cfg || !param || !subgroup) {
		atf_err("user param or atf config or subgroup is NULL\n");
		return FALSE;
	}

	if (!group || qdf_list_empty(&group->atf_subgroups)) {
		atf_err("group is NULL. Return \n");
		return FALSE;
	}

	qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
	while(tmpnode) {
		sgroup = qdf_container_of(tmpnode, struct atf_subgroup_list,
					  sg_next);
		tmpnode_next = NULL;
		switch (cfg_type) {
			case ATF_SUBGROUP_TYPE_SSID:
				break;
			case ATF_SUBGROUP_TYPE_PEER:
				if (atf_subgroup_match_peer(sgroup,
							    (char *)param)) {
					*subgroup = sgroup;
					return TRUE;
				}
				break;
			case ATF_SUBGROUP_TYPE_AC:
				if (atf_subgroup_match_ac(sgroup,
							  *(u_int8_t*)param)) {
					*subgroup = sgroup;
					return TRUE;
				}
				break;
			default:
				atf_err("Invalid cfg_type : %d \n", cfg_type);
				return FALSE;
		}
		if (QDF_STATUS_SUCCESS !=
		qdf_list_peek_next(&group->atf_subgroups, tmpnode,
				   &tmpnode_next))
			break;
		tmpnode = tmpnode_next;
	}

	return FALSE;
}

bool atf_config_ssid_in_group(struct pdev_atf *pa, uint8_t *ssid)
{
	uint8_t i = 0, k = 0;
	struct atf_config *cfg = NULL;

	if ((NULL == pa) || (NULL == ssid)) {
		atf_err("pdev atf or SSID is NULL\n");
		return FALSE;
	}
	cfg = &pa->atfcfg_set;
	for (i = 0; i < cfg->grp_num_cfg; i++) {
		/* Loop through each ssid in the group */
		for (k = 0; k < cfg->atfgroup[i].grp_num_ssid; k++) {
			if((qdf_str_len(cfg->atfgroup[i].grp_ssid[k]) ==
			   qdf_str_len(ssid)) && (!qdf_str_cmp((char *)(
			   cfg->atfgroup[i].grp_ssid[k]), (char *)(ssid)))) {
				return TRUE;
			}
		}
	}

	return FALSE;
}

QDF_STATUS atf_config_init_ac_val(struct pdev_atf *pa, uint8_t index)
{
	uint8_t i = 0;
	struct atf_ac_config *atfac = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return QDF_STATUS_E_FAILURE;
	}
	if (pa->atf_ssidgroup) {
		if (index >= 0 && index < ATF_ACTIVED_MAX_ATFGROUPS) {
			atfac = &pa->atfcfg_set.atfgroup[index].atf_cfg_ac;
		} else {
			return QDF_STATUS_E_FAILURE;
		}
	} else if (index >= 0 && index < ATF_CFG_NUM_VDEV) {
		atfac = &pa->atfcfg_set.vdev[index].atf_cfg_ac;
	} else {
		return QDF_STATUS_E_FAILURE;
	}
	for (i = 0; i < WME_NUM_AC; i++) {
		atfac->ac[i][0] = 0;
	}
	atfac->ac_bitmap = 0;
	atfac->ac_cfg_flag = 0;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS atf_config_del_ssid_group(struct pdev_atf *pa, uint8_t *ssid,
				     uint32_t index)
{
	uint32_t i = 0;
	struct atf_config *cfg = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return QDF_STATUS_E_FAILURE;
	}
	cfg = &pa->atfcfg_set;
	if ((index >= ATF_ACTIVED_MAX_ATFGROUPS) ||
	    (index >= cfg->grp_num_cfg)) {
		atf_err(" Invalid group index\n");
		return QDF_STATUS_E_FAILURE;
	}
	if(!cfg->atfgroup[index].grp_num_ssid) {
		atf_err(" No SSIDs attached to this group\n");
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_zero(&(cfg->atfgroup[index]), sizeof(cfg->atfgroup[0]));
	if (cfg->grp_num_cfg > ATF_ACTIVED_MAX_ATFGROUPS) {
		atf_err(" Invalid group num cfg \n");
		return QDF_STATUS_E_FAILURE;
	}
	/* if group is not the last, move the other group indexes */
	if ((index != (cfg->grp_num_cfg -1)) &&
		(index < ATF_ACTIVED_MAX_ATFGROUPS)) {
		for (i = index; (i < cfg->grp_num_cfg) &&
		     (i < ATF_ACTIVED_MAX_ATFGROUPS); i++) {
			if (i != (cfg->grp_num_cfg - 1)) {
				qdf_mem_copy(&cfg->atfgroup[i],
					     &cfg->atfgroup[i+1],
					     (sizeof(cfg->atfgroup[0]) *
				             (cfg->grp_num_cfg -i -1)));
			}
		}
	}
	qdf_mem_zero(&(cfg->atfgroup[cfg->grp_num_cfg - 1]),
		     sizeof(cfg->atfgroup[0]));
	cfg->grp_num_cfg--;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS atf_config_add_pdev_group(struct pdev_atf *pa, uint8_t *gname)
{
	uint8_t groupindex = 0;
	struct atf_config *cfg = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return QDF_STATUS_E_FAILURE;
	}
	cfg = &pa->atfcfg_set;
	groupindex = cfg->grp_num_cfg;
	if (groupindex >= 0 && groupindex < ATF_ACTIVED_MAX_ATFGROUPS) {
		qdf_str_lcopy(cfg->atfgroup[groupindex].grpname, gname,
			 sizeof(cfg->atfgroup[groupindex].grpname));
		atf_config_init_ac_val(pa, groupindex);
		cfg->grp_num_cfg++;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS atf_modify_sta_cfg_index(struct pdev_atf *pa, uint8_t gindex)
{
	int i = 0, j = 0;
	struct atf_config *cfg = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return QDF_STATUS_E_FAILURE;
	}
	cfg = &pa->atfcfg_set;
	for (i = 0; i < ATF_ACTIVED_MAX_CLIENTS; i++) {
		for (j = gindex + 1; j < (ATF_CFG_NUM_VDEV - 1); j++) {
			cfg->peer_id[i].sta_cfg_value[j] =
			cfg->peer_id[i].sta_cfg_value[j + 1];
		}
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS atf_config_del_pdev_group(struct pdev_atf *pa, uint8_t gindex)
{
	struct atf_config *cfg = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return QDF_STATUS_E_FAILURE;
	}
	cfg = &pa->atfcfg_set;
	if (!cfg->grp_num_cfg) {
		atf_err("ERROR! No. of groups configured is %d\n",
			cfg->grp_num_cfg);
		return QDF_STATUS_E_FAILURE;
	}
	if (gindex >= 0 && gindex < ATF_ACTIVED_MAX_ATFGROUPS) {
		qdf_mem_zero(cfg->atfgroup[gindex].grpname,
			     sizeof(cfg->atfgroup[gindex].grpname));
		if (cfg->grp_num_cfg < ATF_ACTIVED_MAX_ATFGROUPS &&
		    (gindex < (cfg->grp_num_cfg -1))) {
			qdf_mem_copy(&cfg->atfgroup[gindex],
				     &cfg->atfgroup[gindex +1],
				     (sizeof(cfg->atfgroup[0]) *
				     (cfg->grp_num_cfg - gindex -1)));
			qdf_mem_zero(&(cfg->atfgroup[cfg->grp_num_cfg - 1]),
				     sizeof(cfg->atfgroup[0]));
		}
		atf_modify_sta_cfg_index(pa, gindex);
		cfg->grp_num_cfg--;
	}

	return QDF_STATUS_SUCCESS;
}

struct atf_group_list *atf_add_new_group(struct pdev_atf *pa, uint8_t *gname)
{
	struct atf_group_list *group = NULL;

	group = qdf_mem_malloc(sizeof(struct atf_group_list));
	if (group) {
		qdf_mem_zero(group->group_name, WLAN_SSID_MAX_LEN + 1);
		qdf_str_lcopy(group->group_name, (char *)gname,
		        WLAN_SSID_MAX_LEN + 1);
		group->atf_num_sg_borrow = 0;
		group->atf_num_clients = 0;
		group->atf_contributabletokens = 0;
		group->shadow_atf_contributabletokens = 0;
		group->group_del = 0;
		qdf_list_create(&group->atf_subgroups, ATF_MAX_SUBGROUPS);
		qdf_list_insert_back(&pa->atf_groups, &group->group_next);
	}

	return group;
}

QDF_STATUS atf_group_vap_sched(struct wlan_objmgr_vdev *vdev,
			       uint32_t sched_policy)
{
	struct pdev_atf *pa = NULL;
	uint8_t groupindex = 0, len = 0;
	struct atf_group_list *group = NULL;
	struct atf_config *cfg = NULL;
	uint8_t ssid[WLAN_SSID_MAX_LEN + 1] = {0};
	QDF_STATUS retval = QDF_STATUS_E_FAILURE;

	if (NULL == vdev) {
		atf_err("vdev is NULL!\n");
		return retval;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev_atf is NULL!\n");
		return retval;
	}
	cfg = &pa->atfcfg_set;

	if (QDF_STATUS_SUCCESS !=
			wlan_vdev_mlme_get_ssid(vdev, ssid, &len)) {
		 atf_err("Failed to get SSID\n");
		 return retval;
	}

	/* Find corresponding group */
	if (atf_config_is_group_exist(cfg, ssid, &groupindex)) {
		group = cfg->atfgroup[groupindex].grplist_entry;
		group->group_sched = sched_policy;
		cfg->atfgroup[groupindex].grp_sched_policy = sched_policy;
                retval = QDF_STATUS_SUCCESS;
		atf_info("group: %s found.Index: %d sched policy set to %d\n",
			 group->group_name, groupindex, group->group_sched);
	} else {
		atf_err("group could not be found for vap :  %s \n",
			cfg->vdev[groupindex].essid);
	}

        return retval;
}

void atf_group_delete(struct pdev_atf *pa, uint8_t *gname)
{
	struct atf_group_list *group = NULL;
	qdf_list_node_t *node = NULL;
	qdf_list_node_t *next_node = NULL;

	qdf_list_peek_front(&pa->atf_groups, &node);
	while(node) {
		group = qdf_container_of(node, struct atf_group_list,
							group_next);
		if (group &&
		(qdf_str_len(group->group_name) == qdf_str_len(gname)) &&
				!qdf_str_cmp(group->group_name, gname)) {
			group->group_del = 1;
		}
		next_node = NULL;
		if (QDF_STATUS_SUCCESS !=
		qdf_list_peek_next(&pa->atf_groups, node, &next_node))
			break;
		node = next_node;
	}
}

void atf_add_group_airtime(struct atf_group_list *group, uint32_t value)
{
	if (group)
		group->group_airtime = value;
}

void atf_reset_airtime_sg_all(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pa) {
		atf_err("Pdev ATF object is NULL\n");
		return;
	}
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					group_next);
		if (group) {
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode,
						struct atf_subgroup_list,
						sg_next);
				tmpnode_next = NULL;
				subgroup->sg_atf_units = 0;
				 /* reset contribute level when
				  * atf table is updated * (node joins)
				  */
				subgroup->sg_contribute_level = 0;
				subgroup->sg_contribute_id = 0;
				if (QDF_STATUS_SUCCESS !=
				       qdf_list_peek_next(&group->atf_subgroups,
							tmpnode, &tmpnode_next))
					break;
				tmpnode = tmpnode_next;
			}
		}
		next_node = NULL;
		if (QDF_STATUS_SUCCESS !=
			qdf_list_peek_next(&pa->atf_groups, node, &next_node))
			break;
		node = next_node;
	}
}

static int32_t
atf_find_peer_config_match(struct pdev_atf *pa, struct atf_group_list *group,
			   uint8_t *sta_mac)
{
	int32_t index = 0, ret = -EINVAL, i = 0;
	struct atf_config *cfg = NULL;

	if (NULL == pa) {
		atf_err("Pdev ATF object is NULL\n");
		return -EINVAL;
	}
	if (NULL == group) {
		atf_err("Group is NULL\n");
		return -EINVAL;
	}
	if (NULL == sta_mac) {
		atf_err("STA MAC is NULL\n");
		return -EINVAL;
	}
	cfg = &pa->atfcfg_set;
	for (index = 0; index < ATF_ACTIVED_MAX_CLIENTS; index++) {
		/* compare if peer mac in the ATF peer table & subgroup match */
		if (qdf_mem_cmp(cfg->peer_id[index].sta_mac, sta_mac,
						MAC_ADDR_SIZE) == 0) {
			/* The group/ssid name is the name to which the peer is
			 * associated.Match group/ssid name with the subgroup's
			 * parent group name
			 */
			if (pa->atf_ssidgroup) {
				i = cfg->peer_id[index].index_group;
				if ((cfg->peer_id[index].index_group != 0) &&
					(cfg->peer_id[index].sta_assoc_status ==
					 1)) {
					/* check if the SSID to which peer is
					 * connected to, is part of the group */
					if (qdf_str_cmp(
						cfg->atfgroup[i - 1].grpname,
						group->group_name) == 0) {
						ret = index;
						goto exit;
					}
				}
			} else {
				i = cfg->peer_id[index].index_vdev;
				if ((cfg->peer_id[index].index_vdev != 0) &&
					(cfg->peer_id[index].sta_assoc_status
					 				== 1)) {
					/* VAP SSID should match group name */
					if (qdf_str_cmp(cfg->vdev[i - 1].essid,
						      group->group_name) == 0) {
						ret = index;
						goto exit;
					}
				}
			}
		}
	}

exit:
	return ret;
}

static QDF_STATUS atf_assign_airtime_sg_peer(struct pdev_atf *pa)
{
	int32_t peer_index = 0;
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pa) {
		atf_err("Pdev ATF object is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
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
				if (subgroup->sg_type ==
				    ATF_SUBGROUP_TYPE_PEER) {
					peer_index = atf_find_peer_config_match(
							pa, group,
							subgroup->sg_peermac);
					if (peer_index >= 0) {
						subgroup->sg_atf_units =
						  pa->atfcfg_set.peer_id[
						    peer_index].sta_cal_value;
					}
				}
				tmpnode = tmpnode_next;
			}
		}
		node = next_node;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
atf_configure_ac_subgroup(struct pdev_atf *pa, struct atf_group_list *group,
			  struct atf_subgroup_list *subgroup)
{
	uint32_t peerindex = 0, index =0;
	struct atf_ac_config *atfac = NULL;
	uint8_t peermatch = 0, ac_id = 0, acbitmap = 0;
	struct atf_config *cfg = NULL;

	if (NULL == pa) {
		atf_err("Pdev ATF object is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	if (NULL == group) {
		atf_err("Group is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	if (NULL == subgroup) {
		atf_err("Subgroupis NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	cfg = &pa->atfcfg_set;
	for (peerindex = 0; peerindex < ATF_ACTIVED_MAX_CLIENTS; peerindex++) {
		if (!(cfg->peer_id[peerindex].sta_mac)) {
			continue;
		}
		ac_id = peermatch = acbitmap = 0;
		atfac = NULL;
		/* The ssid/Group name that the client is now associated to,
		 * should match with the group name passed as func argument
		 */
		if (pa->atf_ssidgroup) {
			index = cfg->peer_id[peerindex].index_group;
			/* check if Peer group name matches with the group name
			 */
			if ((cfg->peer_id[peerindex].index_group != 0) &&
			    (cfg->peer_id[peerindex].sta_assoc_status == 1) &&
			    (cfg->peer_id[peerindex].sta_cfg_value[index] == 0))
			{
				if (qdf_str_cmp(cfg->atfgroup[index -
								1].grpname,
						group->group_name) == 0) {
					atfac =
					   &cfg->atfgroup[index - 1].atf_cfg_ac;
					peermatch = 1;
				}
			}
		} else {
			index = cfg->peer_id[peerindex].index_vdev;
			if ((cfg->peer_id[peerindex].index_vdev != 0) &&
			    (cfg->peer_id[peerindex].sta_assoc_status == 1) &&
			    (cfg->peer_id[peerindex].sta_cfg_value[index] == 0))
			{
				/* compare ssid with node essid */
				if (qdf_str_cmp(cfg->vdev[index - 1].essid,
						group->group_name) == 0) {
					atfac =
					       &cfg->vdev[index - 1].atf_cfg_ac;
					peermatch = 1;
				}
			}
		}
		/* if the group name matched, check for ac based configuration
		 * in the respective VAP or Group index of the peer */
		if (atfac && peermatch && atfac->ac_bitmap) {
			acbitmap = atfac->ac_bitmap;
			while (acbitmap) {
				if ((1 & acbitmap) &&
					(ac_id == subgroup->ac_id)) {
					subgroup->sg_atf_units =
							cfg->peer_id[peerindex].
							sta_cal_ac_value[ac_id];
				}
				ac_id++;
				acbitmap = acbitmap >> 1;
			}
		}
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS atf_assign_airtime_sg_ac(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pa) {
		atf_err("Pdev ATF object is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
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
				if (subgroup->sg_type == ATF_SUBGROUP_TYPE_AC) {
					atf_configure_ac_subgroup(pa, group,
							          subgroup);
				}
				tmpnode = tmpnode_next;
			}
		}
		node = next_node;
	}

	return QDF_STATUS_SUCCESS;
}

void calculate_airtime_group_peer_ac(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pa) {
		atf_err("Pdev ATF object is NULL\n");
		return;
	}
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
		                    group_next);
		next_node = NULL;
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		if (group) {
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			group->tot_airtime_peer = 0;
			group->tot_airtime_ac = 0;
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode,
						       struct atf_subgroup_list,
						       sg_next);
				tmpnode_next = NULL;
				qdf_list_peek_next(&group->atf_subgroups,
						   tmpnode, &tmpnode_next);
				if (subgroup->sg_type ==
				    ATF_SUBGROUP_TYPE_PEER) {
				    group->tot_airtime_peer +=
					    subgroup->sg_atf_units;
				} else if (subgroup->sg_type ==
					   ATF_SUBGROUP_TYPE_AC) {
					group->tot_airtime_ac +=
						subgroup->sg_atf_units;
				}
				tmpnode = tmpnode_next;
			}
		}
		node = next_node;
	}
}

QDF_STATUS atf_assign_airtime_sg_ssid(struct pdev_atf *pa)
{
	int32_t airtime = 0, peer_ac_airtime = 0;
	struct atf_group_list *group = NULL;
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pa) {
		atf_err("Pdev ATF object is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
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
				if (subgroup->sg_type ==
				    ATF_SUBGROUP_TYPE_SSID) {
					subgroup->sg_atf_units = 0;
					peer_ac_airtime =
						group->tot_airtime_peer +
						group->tot_airtime_ac;
					if (peer_ac_airtime >
					    group->group_airtime) {
						peer_ac_airtime = 0;
					}
					airtime = group->group_airtime -
						  peer_ac_airtime;
					if (airtime >= 0) {
						subgroup->sg_atf_units =
									airtime;
					} else {
						subgroup->sg_atf_units = 0;
					}
				}
				tmpnode = tmpnode_next;
			}
		}
		node = next_node;
	}

	return QDF_STATUS_SUCCESS;
}


void atf_assign_airtime_subgroup(struct pdev_atf *pa)
{
	atf_reset_airtime_sg_all(pa);
	atf_assign_airtime_sg_peer(pa);
	atf_assign_airtime_sg_ac(pa);
	calculate_airtime_group_peer_ac(pa);
	atf_assign_airtime_sg_ssid(pa);
}

QDF_STATUS
atf_set_ssid_sched_policy(struct wlan_objmgr_vdev *vdev, uint16_t value)
{
	struct vdev_atf *va;
	struct wlan_objmgr_psoc *psoc = NULL;
	int32_t retval = QDF_STATUS_E_INVAL;
	struct wlan_lmac_if_atf_tx_ops *tx_ops = NULL;

	if (NULL == vdev) {
		atf_err("VDEV is NULL!\n");
		return QDF_STATUS_E_INVAL;
	}
	va = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_ATF);
	psoc = wlan_vdev_get_psoc(vdev);
	if (NULL == va) {
		atf_err("vdev atf object is NULL!\n");
		return QDF_STATUS_E_INVAL;
	}
	if (NULL == psoc) {
		atf_err("psoc is NULL!\n");
		return QDF_STATUS_E_INVAL;
	}

	if (atf_group_vap_sched(vdev, value) == QDF_STATUS_SUCCESS) {
		va->vdev_atf_sched = value;
		tx_ops = &psoc->soc_cb.tx_ops.atf_tx_ops;
		if (tx_ops && tx_ops->atf_ssid_sched_policy) {
			if (tx_ops->atf_ssid_sched_policy(vdev, value) ==
							QDF_STATUS_SUCCESS) {
				retval = QDF_STATUS_SUCCESS;
			}
		} else {
			retval = QDF_STATUS_SUCCESS;
		}
	}

	return retval;
}

void atf_remove_group(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;

	qdf_list_node_t *node = NULL;
	qdf_list_node_t *next_node = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return;
	}
	qdf_list_peek_front(&pa->atf_groups, &node);
	while(node) {
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		next_node = NULL;
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		if (group && (group->group_del == 1)) {
			atf_remove_subgroup_all(group);
			qdf_list_destroy(&group->atf_subgroups);
			if (qdf_list_remove_node(&pa->atf_groups, node) ==
			    QDF_STATUS_SUCCESS) {
				qdf_mem_free(group);
				group = NULL;
			}
		}
		node = next_node;
	}
}

void atf_del_group_subgroup(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	qdf_list_node_t *node = NULL;
	qdf_list_node_t *next_node = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return;
	}
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		next_node = NULL;
		atf_remove_subgroup(group);
		if (QDF_STATUS_SUCCESS !=
			qdf_list_peek_next(&pa->atf_groups, node, &next_node))
		    break;
		node = next_node;
	}
	atf_remove_group(pa);
}

struct atf_subgroup_list *atf_add_new_subgroup(struct pdev_atf *pa,
					       struct atf_group_list *group,
					       void *param, uint32_t val,
					       uint32_t sg_type)
{
	struct atf_subgroup_list *subgroup = NULL;
	uint8_t name[WLAN_SSID_MAX_LEN + 1] = {0};
	uint8_t unit_len = 0;

	if ((sg_type == ATF_SUBGROUP_TYPE_PEER ||
		sg_type == ATF_SUBGROUP_TYPE_AC) && !param) {
		atf_err("subgroup type : %d param cannot be NULL\n", sg_type);
		return NULL;
	}
	if (NULL == group) {
		atf_err("Group is NULL\n");
		return NULL;
	}
	subgroup = qdf_mem_malloc(sizeof(struct atf_subgroup_list));
	if (subgroup == NULL) {
		atf_err("subgroup alloc failed. Return \n");
		return NULL;
	}
	subgroup->sg_id = group->subgroup_count++;
	if (!qdf_str_cmp(group->group_name, DEFAULT_GROUPNAME)) {
		qdf_snprint(subgroup->sg_name, WLAN_SSID_MAX_LEN,
			    DEFAULT_SUBGROUPNAME);
	} else {
		unit_len = (subgroup->sg_id <= 9)? 1 : 2;
		/* Restricting the length of name */
		qdf_str_lcopy(name, group->group_name, WLAN_SSID_MAX_LEN
			      - unit_len - ATF_TRUNCATE_GRP_NAME);
		qdf_snprint(subgroup->sg_name, WLAN_SSID_MAX_LEN, "%s%*d%s%s",
			    "sg_", unit_len, (subgroup->sg_id), "_", name);
	}
	qdf_list_insert_back(&group->atf_subgroups, &subgroup->sg_next);
	group->num_subgroup++;
	subgroup->sg_type = sg_type;
	subgroup->usr_cfg_val = val;
	if (sg_type == ATF_SUBGROUP_TYPE_PEER) {
		qdf_mem_copy((char *)(subgroup->sg_peermac),
			     (char *)(param),QDF_MAC_ADDR_SIZE);
	} else if (sg_type == ATF_SUBGROUP_TYPE_AC) {
		subgroup->ac_id = *(u_int8_t *)param;
	}
	/* Initialize lock */
	qdf_spinlock_create(&subgroup->atf_sg_lock);

	return subgroup;
}

void atf_remove_subgroup(struct atf_group_list *group)
{
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *node = NULL;
	qdf_list_node_t *next_node = NULL;

	if (NULL == group) {
		return;
	}
	qdf_list_peek_front(&group->atf_subgroups, &node);
	while (node) {
		subgroup = qdf_container_of(node, struct atf_subgroup_list,
		                    sg_next);
		next_node = NULL;
		qdf_list_peek_next(&group->atf_subgroups, node, &next_node);
		if (subgroup && (subgroup->sg_del == 1)) {
			if (qdf_list_remove_node(&group->atf_subgroups,
						 node) == QDF_STATUS_SUCCESS) {
			        qdf_mem_free(subgroup);
			        subgroup = NULL;
			}
		}
		node = next_node;
	}
}

void atf_remove_pdev_subgroup(struct pdev_atf *pa)
{
	struct atf_group_list *group = NULL;
	qdf_list_node_t *node = NULL;
	qdf_list_node_t *next_node = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return;
	}
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		next_node = NULL;
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		if (group) {
			atf_remove_subgroup(group);
		}
		node = next_node;
	}
}

void atf_remove_subgroup_all(struct atf_group_list *group)
{
	atf_mark_del_all_sg(group);
	atf_remove_subgroup(group);
}

void
atf_set_assign_group(struct pdev_atf *pa, struct wlan_objmgr_peer *peer,
		     uint32_t group_index)
{
	struct peer_atf *pa_obj = NULL;
	struct atf_group_list *group = NULL;
	qdf_list_node_t *tnode = NULL;

	if ((NULL == pa) || (NULL == peer)) {
		atf_err("pdev_atf or peer is NULL\n");
		return;
	}
	pa_obj = wlan_objmgr_peer_get_comp_private_obj(peer,
						       WLAN_UMAC_COMP_ATF);
	if (NULL == pa_obj) {
		atf_err("Peer atf object is NULL!\n");
		return;
	}
	if (group_index == 0xFF) {
		qdf_list_peek_front(&pa->atf_groups, &tnode);
		if (tnode)
			group = qdf_container_of(tnode, struct atf_group_list,
						 group_next);
		pa_obj->atf_group = group;
		atf_assign_subgroup(pa, pa_obj, group, 0, ATF_CFGTYPE_SSID);
	} else if (group_index <= ATF_ACTIVED_MAX_ATFGROUPS) {
		pa_obj->atf_group =
			pa->atfcfg_set.atfgroup[group_index - 1].grplist_entry;
	}
}

QDF_STATUS atf_verify_sta_cumulative(struct atf_config *cfg, uint32_t index,
				     struct sta_val *val)
{
	int i = 0;
	u_int32_t cumulative = 0;

	if (NULL == cfg) {
		atf_err("atf config is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	if (NULL == val) {
		atf_err("sta config is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	for (i = 0; i < ATF_ACTIVED_MAX_CLIENTS; i++) {
		if (qdf_mem_cmp(cfg->peer_id[i].sta_mac, val->sta_mac,
				QDF_MAC_ADDR_SIZE) == 0) {
			cumulative += cfg->peer_id[i].sta_cfg_value[index + 1];
		}
	}
	if(cumulative + (val->value) > PER_UNIT_1000) {
		atf_err("%s returning PER_NOT_ALLOWED \n",__func__);
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS atf_new_sta_cfg(struct pdev_atf *pa, struct sta_val *val,
			   uint32_t peer_index)
{
	uint8_t cfg_index = 0;
	uint8_t global_config = 0;
	uint64_t calbitmap = 0;
	struct atf_config *cfg = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return QDF_STATUS_E_INVAL;
	}
	if (NULL == val) {
		atf_err("atf STA config is NULL \n");
		return QDF_STATUS_E_INVAL;
	}
	cfg = &pa->atfcfg_set;
	if  (!val->ssid) {
		global_config = 1;
	} else if (pa->atf_ssidgroup) {
		if (!atf_config_is_group_exist(cfg, val->ssid, &cfg_index)) {
			atf_err("%s group %s not found \n",__func__, val->ssid);
			return QDF_STATUS_E_INVAL;
		}
		cfg->peer_id[peer_index].sta_cfg_value[cfg_index + 1] =
								   val->value;
	} else {
		/* adding new configuration */
		if(val->ssid[0]) {
			for (cfg_index = 0; (cfg_index < ATF_CFG_NUM_VDEV);
			     cfg_index++) {
				if (cfg->vdev[cfg_index].cfg_flag) {
					if (qdf_str_cmp((char *)(
						cfg->vdev[cfg_index].essid),
						(char *)(val->ssid)) == 0) {
						break;
					}
				}
			}
			if (cfg_index == ATF_CFG_NUM_VDEV) {
				atf_err("The input ssid does not exist\n");
				return QDF_STATUS_E_INVAL;
			}
			cfg->peer_id[peer_index].sta_cfg_value[cfg_index + 1] =
								     val->value;
			cfg->peer_id[peer_index].index_vdev = cfg_index + 1;
		}
	}
	if(global_config) {
		/* storing global configuration */
		atf_info("Peer percentage should have an SSID\n");
		atf_info("Percentage without ssid/group is deprecated \n");
		cfg->peer_id[peer_index].sta_cfg_value[ATF_CFG_GLOBAL_INDEX] =
								     val->value;
		if (pa->atf_ssidgroup) {
			cfg->peer_id[peer_index].index_group = 0xff;
		} else {
			cfg->peer_id[peer_index].index_vdev = 0xff;
		}
	}
	cfg->peer_id[peer_index].sta_cfg_mark = 1;
	cfg->peer_id[peer_index].sta_assoc_status = 0;
	cfg->peer_cal_bitmap |= (calbitmap << peer_index);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS atf_modify_sta_cfg(struct pdev_atf *pa, struct sta_val *val,
			      uint32_t peer_index)
{
	uint8_t cfg_index = 0;
	uint8_t global_config = 0;
	struct atf_config *cfg = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return QDF_STATUS_E_INVAL;
	}
	cfg = &pa->atfcfg_set;
	if (!val->ssid) {
		global_config = 1;
	} else if (pa->atf_ssidgroup) {
		if (!atf_config_is_group_exist(cfg, val->ssid, &cfg_index)) {
			atf_err("%s group %s not found \n",__func__, val->ssid);
			return QDF_STATUS_E_FAILURE;
		}
	} else {
		/* editing already present  configuration */
		if (val->ssid[0]) {
			for (cfg_index = 0; (cfg_index < ATF_CFG_NUM_VDEV);
			     cfg_index++) {
				if (cfg->vdev[cfg_index].cfg_flag) {
					if (qdf_str_cmp((char *)(
					    cfg->vdev[cfg_index].essid),
					    (char *)(val->ssid)) == 0)
						break;
				}
			}
			if (cfg_index == ATF_CFG_NUM_VDEV) {
				atf_info("The input ssid does not exist\n");
				return QDF_STATUS_E_FAILURE;
			}
		}
	}

	cfg->peer_id[peer_index].sta_cfg_value[cfg_index + 1] = val->value;
	if (global_config) {
		/* storing global configuration */
		cfg->peer_id[peer_index].sta_cfg_value[ATF_CFG_GLOBAL_INDEX] =
								     val->value;
	}

	return QDF_STATUS_SUCCESS;
}

void atf_dump_pdev_sta(struct pdev_atf *pa)
{
	int i = 0, j = 0;

	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return;
	}
	for (i = 0; i < 4; i++) {
		atf_info("entry %d node : %s cfg : %d \n",
			 i, ether_sprintf(pa->atfcfg_set.peer_id[i].sta_mac),
			 pa->atfcfg_set.peer_id[i].sta_cfg_mark);
		for (j= 0; j < ATF_CFG_NUM_VDEV; j++)
			atf_info(" %d:%d ",
				 j, pa->atfcfg_set.peer_id[i].sta_cfg_value[j]);
		atf_info("\n");
	}
}

uint8_t
atf_find_vdev_index(struct pdev_atf *pa, struct wlan_objmgr_vdev *vdev)
{
	uint8_t vdev_index = 0xFF;
	uint8_t i = 0, len = 0;
	uint8_t ssid[WLAN_SSID_MAX_LEN + 1] = {0};

	if (QDF_STATUS_SUCCESS !=
			wlan_vdev_mlme_get_ssid(vdev, ssid, &len)) {
		atf_err("Failed to get SSID\n");
		return vdev_index;
	}
	for (i = 0; i < pa->atfcfg_set.vdev_num_cfg; i++) {
		if (!qdf_mem_cmp(pa->atfcfg_set.vdev[i].essid, ssid, len) &&
		    (qdf_str_len(pa->atfcfg_set.vdev[i].essid) == len)) {
			vdev_index = i + 1;
			break;
		}
	}

	return vdev_index;
}

uint8_t
atf_find_vdev_ssid_index(struct pdev_atf *pa, uint8_t *ssid)
{
	uint8_t vdev_index = 0xFF;
	uint8_t i = 0, len = 0;

	if ((NULL == ssid) || (NULL == pa)) {
		atf_err("SSID or pa is NULL \n");
		goto fail;
	}
	len = qdf_str_len(ssid);
	for (i = 0; i < pa->atfcfg_set.vdev_num_cfg; i++) {
		if (!qdf_mem_cmp(pa->atfcfg_set.vdev[i].essid, ssid, len) &&
		    (qdf_str_len(pa->atfcfg_set.vdev[i].essid) == len)) {
			vdev_index = i;
			break;
		}
	}

fail:
	return vdev_index;
}

static uint32_t
atf_find_group_index(struct pdev_atf *pa, struct wlan_objmgr_vdev *vdev)
{
	uint32_t group_index = 0xFF;
	uint8_t i = 0, k = 0, ssid_len = 0;
	uint8_t ssid[WLAN_SSID_MAX_LEN + 1] = {0};

	if (QDF_STATUS_SUCCESS !=
			wlan_vdev_mlme_get_ssid(vdev, ssid, &ssid_len)) {
		atf_err("Failed to get SSID\n");
		return group_index;
	}
	for (i = 0; i < pa->atfcfg_set.grp_num_cfg; i++) {
		for (k = 0; k < pa->atfcfg_set.atfgroup[i].grp_num_ssid; k++) {
			if (qdf_mem_cmp(pa->atfcfg_set.atfgroup[i].grp_ssid[k],
					ssid, ssid_len) == 0) {
				group_index = i + 1;
				break;
			}
		}
	}

	return group_index;
}

QDF_STATUS
atf_assign_ac_subgroup(struct pdev_atf *pa, struct peer_atf *peer,
		       struct atf_group_list *group, uint8_t acbitmap)
{
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *tmpnode = NULL;
	uint8_t ac_id = 0;
	struct atf_config *cfg = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return QDF_STATUS_E_INVAL;
	}
	if ((NULL == peer) || (NULL == group)) {
		atf_err("Peer atf or group is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	cfg = &pa->atfcfg_set;
	for (ac_id = 0; ac_id < WME_NUM_AC; ac_id++) {
		qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
		if (tmpnode) {
			subgroup = qdf_container_of(tmpnode,
					struct atf_subgroup_list, sg_next);
		}
		if (1 & acbitmap) {
			if (!atf_config_is_subgroup_exist(cfg, group, &ac_id,
					&subgroup, ATF_SUBGROUP_TYPE_AC)) {
				atf_err("%s AC subgroup doesn't exist in \
					group %s for ac : %d \n", __func__,
					group->group_name, ac_id);
				return QDF_STATUS_E_INVAL;
			} else {
				peer->ac_list_ptr[ac_id] = subgroup;
			}
		} else {
			peer->ac_list_ptr[ac_id] = subgroup;
		}
		acbitmap = acbitmap >> 1;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
atf_assign_peer_subgroup(struct pdev_atf *pa, struct peer_atf *peer,
			 struct atf_group_list *group)
{
	struct atf_subgroup_list *subgroup = NULL;
	struct atf_config *cfg = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;
	uint8_t ac_id = 0;
	struct wlan_objmgr_peer *peer_obj = NULL;

	if (NULL == peer) {
		atf_err("Peer atf NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	if (NULL == pa) {
		atf_err("pdev atf is NULL \n");
		return QDF_STATUS_E_INVAL;
	}
	peer_obj = peer->peer_obj;
	cfg = &pa->atfcfg_set;
	if(NULL == peer_obj) {
		atf_err("Peer object is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	for (ac_id = 0; ac_id < WME_NUM_AC; ac_id++) {
		qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
		while(tmpnode) {
			subgroup = qdf_container_of(tmpnode,
					struct atf_subgroup_list, sg_next);
			tmpnode_next = NULL;
			if (!atf_config_is_subgroup_exist(cfg, group,
					peer_obj->macaddr, &subgroup,
					ATF_SUBGROUP_TYPE_PEER)) {
				atf_err("%s %d Peer subgroup doesn't exist\n",
					__func__, __LINE__);
				return QDF_STATUS_E_INVAL;
			} else {
				peer->ac_list_ptr[ac_id] = subgroup;
			}
			if (QDF_STATUS_SUCCESS !=
				qdf_list_peek_next(&group->atf_subgroups,
						   tmpnode, &tmpnode_next))
				break;
			tmpnode = tmpnode_next;
		}
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
atf_assign_default_subgroup(struct pdev_atf *pa, struct peer_atf *peer,
			    struct atf_group_list *group)
{
	struct atf_subgroup_list *subgroup = NULL;
	qdf_list_node_t *tmpnode = NULL;
	uint8_t ac_id = 0;

	if(!peer) {
		atf_err("Peer atf is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
	if (tmpnode)
		subgroup = qdf_container_of(tmpnode, struct atf_subgroup_list,
					    sg_next);
	for (ac_id = 0 ; ac_id < WME_NUM_AC ; ac_id++) {
		peer->ac_list_ptr[ac_id] = subgroup;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
atf_assign_subgroup(struct pdev_atf *pa, struct peer_atf *peer,
		    struct atf_group_list *group, uint8_t acbitmap,
		    uint8_t cfg_type)
{
	if (NULL == peer) {
		atf_err("Peer atf is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	if (group == NULL) {
		atf_err("group is NULL\n");
		return QDF_STATUS_E_INVAL;
	}
	if (cfg_type == ATF_CFGTYPE_PEER) {
		atf_assign_peer_subgroup(pa, peer, group);
	} else  if (cfg_type == ATF_CFGTYPE_AC) {
		atf_assign_ac_subgroup(pa, peer, group, acbitmap);
	} else {
		atf_assign_default_subgroup(pa, peer, group);
	}

	return 0;
}

uint32_t
atf_get_peer_subgroup_cfgtype(struct pdev_atf *pa, struct atf_ac_config *atfac,
			      uint32_t peerindex)
{
	uint32_t cfg_type = 0;
	uint32_t index = 0;
	struct atf_config *cfg = &pa->atfcfg_set;

	if (NULL == atfac) {
		atf_err("ATF AC config struct is NULL\n");
		return EINVAL;
	}
	if(pa->atf_ssidgroup) {
		index = cfg->peer_id[peerindex].index_group;
	} else {
		index = cfg->peer_id[peerindex].index_vdev;
	}
	if (cfg->peer_id[peerindex].cfg_flag &&
		cfg->peer_id[peerindex].sta_cfg_value[index]) {
		cfg_type = ATF_CFGTYPE_PEER;
	} else if (atfac->ac_bitmap) {
		cfg_type = ATF_CFGTYPE_AC;
	} else {
		cfg_type = ATF_CFGTYPE_SSID;
	}

	return cfg_type;
}

static void
atf_fill_peer_alloc_table(struct pdev_atf *pa, struct atf_context *ac,
		uint32_t group_index, uint8_t vdev_index,
		struct wlan_objmgr_peer *peer, struct wlan_objmgr_vdev *vdev)
{
	uint8_t i = 0, ac_bitmap = 0, cfg_type = 0;
	uint8_t *mac = 0;
	struct atf_config *atf_cfg = &pa->atfcfg_set;
	struct atf_ac_config *atf_ac = NULL;
	struct peer_atf *pa_obj;
	struct atf_group_list *group = NULL;
	qdf_list_node_t *node = NULL;

	pa_obj = wlan_objmgr_peer_get_comp_private_obj(peer,
						       WLAN_UMAC_COMP_ATF);
	if (NULL == pa_obj) {
		atf_err("Peer atf object is NULL!\n");
		return;
	}

	if (WLAN_PEER_AP == wlan_peer_get_peer_type(peer)) {
		qdf_list_peek_front(&pa->atf_groups, &node);
		if (node) {
			group = qdf_container_of(node, struct atf_group_list,
						 group_next);
			if (group) {
				pa_obj->atf_group = group;
				atf_assign_subgroup(pa, pa_obj,
						    pa_obj->atf_group,
						    ac_bitmap, cfg_type);
			}
		}
	}

	for (i = 0; i < ATF_ACTIVED_MAX_CLIENTS; i++) {
		if (atf_cfg->peer_id[i].index_vdev != 0) {
			/* MACs are equal */
			if (qdf_mem_cmp(atf_cfg->peer_id[i].sta_mac,
			    wlan_peer_get_macaddr(peer),
			    QDF_MAC_ADDR_SIZE) == 0) {
				atf_cfg->peer_id[i].index_vdev = vdev_index;
				/* update the peer group index
				group index=0xFF,if vap not part of ATF group
				group index=1-32(index),if vap part of ATF group
				*/
				atf_cfg->peer_id[i].index_group = group_index;
				atf_set_assign_group(pa, peer, group_index);
				if (pa->atf_ssidgroup) {
					if (group_index &&
				    group_index <= ATF_ACTIVED_MAX_ATFGROUPS &&
				    group_index != 0xFF) {
						atf_ac =
						  &atf_cfg->atfgroup[group_index
								- 1].atf_cfg_ac;
					} else {
						atf_ac = NULL;
					}
				} else {
					if (vdev_index &&
					    vdev_index <= ATF_CFG_NUM_VDEV &&
					    vdev_index != 0xFF) {
						atf_ac =
						     &atf_cfg->vdev[vdev_index
						     		- 1].atf_cfg_ac;
					} else {
						atf_ac = NULL;
					}
				}
				if (atf_ac) {
					ac_bitmap = atf_ac->ac_bitmap;
				} else {
					ac_bitmap = 0;
				}
				atf_set_assign_group(pa, peer, group_index);
				if (!pa->atf_tput_based) {
				       cfg_type = atf_get_peer_subgroup_cfgtype(
								pa, atf_ac, i);
				} else {
				       cfg_type = ATF_CFGTYPE_PEER;
				}
				atf_assign_subgroup(pa, pa_obj,
						    pa_obj->atf_group,
						    ac_bitmap, cfg_type);
				if (atf_cfg->peer_id[i].sta_cfg_mark)
					atf_cfg->peer_id[i].sta_cfg_mark = 0;
				break;
			}
		} else {
			/* MACs are not equal */
			if (qdf_mem_cmp(wlan_vdev_mlme_get_macaddr(vdev),
			    wlan_peer_get_macaddr(peer), QDF_MAC_ADDR_SIZE)) {
				mac = &atf_cfg->peer_id[i].sta_mac[0];
				qdf_mem_copy(mac, wlan_peer_get_macaddr(peer),
					     QDF_MAC_ADDR_SIZE);
				atf_cfg->peer_id[i].index_vdev = vdev_index;
				atf_cfg->peer_id[i].index_group = group_index;

				if (pa->atf_ssidgroup) {
					if (group_index &&
				    group_index <= ATF_ACTIVED_MAX_ATFGROUPS &&
				    group_index != 0xFF) {
						atf_ac =
						  &atf_cfg->atfgroup[group_index
								- 1].atf_cfg_ac;
					} else {
						atf_ac = NULL;
					}
				} else {
					if (vdev_index &&
					    vdev_index <= ATF_CFG_NUM_VDEV &&
					    vdev_index != 0xFF) {
						atf_ac =
						     &atf_cfg->vdev[vdev_index
						     		- 1].atf_cfg_ac;
					} else {
						atf_ac = NULL;
					}
				}
				if (atf_ac) {
					ac_bitmap = atf_ac->ac_bitmap;
				} else {
					ac_bitmap = 0;
				}
				atf_set_assign_group(pa, peer,group_index);
				if (group_index == 0xFF) {
					if (!pa->atf_tput_based) {
						atf_assign_subgroup(pa, pa_obj,
							 pa_obj->atf_group, 0,
							 ATF_CFGTYPE_SSID);
					} else {
						atf_assign_subgroup(pa, pa_obj,
							 pa_obj->atf_group, 0,
							 ATF_CFGTYPE_PEER);
					}
				} else {
					if (!pa->atf_tput_based) {
						cfg_type =
						  atf_get_peer_subgroup_cfgtype(
								 pa, atf_ac, i);
					} else {
						cfg_type = ATF_CFGTYPE_PEER;
					}
					atf_assign_subgroup(pa, pa_obj,
					 	   	    pa_obj->atf_group,
							    ac_bitmap,
							    cfg_type);
				}
			}
			break;
		}
	}
	if (i == ATF_ACTIVED_MAX_CLIENTS)
		pa->stop_looping = 1;
}

static void
atf_peer_list_handler(struct wlan_objmgr_pdev *pdev, void *object, void *arg)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;
	struct wlan_objmgr_psoc *psoc = (struct wlan_objmgr_psoc *)arg;
	struct wlan_objmgr_vdev *vdev;
	struct pdev_atf *pa;
	struct atf_context *ac;
	uint8_t vdev_index = 0;
	uint32_t group_index = 0xFF;

	if ((NULL == peer) || (NULL == pdev) || (NULL == psoc)) {
		atf_err("Peer or pdev or psoc objects are null!\n");
		return;
	}

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pa is null!\n");
		return;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return;
	}
	vdev = wlan_peer_get_vdev(peer);
	if (NULL == vdev) {
		atf_err("Peer or is not associated with any vdev!"
						" vdev is NULL!");
		return;
	}
	if (wlan_vdev_mlme_get_opmode(vdev) == QDF_STA_MODE) {
		atf_info("Managed node %s, don't add in alloc table\n",
			wlan_peer_get_macaddr(peer));
		return;
	}

	if (pa->stop_looping == 1)
		return;

	if (pa->atf_ssidgroup || pa->atfcfg_set.grp_num_cfg) {
		/* Search for this vdev in the atfgroup table and
		 * find the group id
		 */
		group_index = atf_find_group_index(pa, vdev);
	}
	vdev_index = atf_find_vdev_index(pa, vdev);
	if (WLAN_CONNECTED_STATE == wlan_peer_mlme_get_state(peer)) {
		atf_fill_peer_alloc_table(pa, ac, group_index, vdev_index,
					  peer, vdev);
	}
}

static int32_t atf_build_alloc_tbl(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_atf *pa;
	struct wlan_objmgr_psoc *psoc;
	struct atf_group_list *group;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	int32_t retv = 0;
	uint8_t i = 0;

	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	psoc = wlan_pdev_get_psoc(pdev);
	if ((NULL == pa) || (NULL == psoc)) {
		atf_err("pa or psoc are null!\n");
		return -1;
	}
	/* Peer by Peer look up vap in alloc table, then program peer table*/
	for (i = 0, pa->atfcfg_set.peer_num_cal = 0;
					i < ATF_ACTIVED_MAX_CLIENTS; i++) {
		if ((pa->atfcfg_set.peer_id[i].index_vdev != 0) &&
		    (pa->atfcfg_set.peer_id[i].sta_assoc_status == 1))
			pa->atfcfg_set.peer_num_cal++;
	}
	if (pa->atfcfg_set.percentage_unit == 0)
		pa->atfcfg_set.percentage_unit = PER_UNIT_1000;
	/* Reset airtime allotted to all subgroups */
	atf_reset_airtime_sg_all(pa);
	/* Delete any group/subgroup marked */
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		next_node = NULL;
		atf_remove_subgroup(group);
		if (QDF_STATUS_SUCCESS !=
			qdf_list_peek_next(&pa->atf_groups, node, &next_node)) {
			break;
		}
		node = next_node;
	}
	atf_remove_group(pa);

	/* 1. Check vdev is in alloc table.
	 * yes-->save vdev (index+1) from vap table for peer table
	 * no--->save 0xff as vdev_index for peer table.
	 * 2. loop peer table and find match peer mac or new peer,
	 * put vdev_index and new peer mac addr.
	 */
	if (pa->atfcfg_set.peer_num_cal != 0) {
		pa->stop_looping = 0;
		wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
						  atf_peer_list_handler,
						  psoc, 0, WLAN_ATF_ID);
		pa->stop_looping = 0;
	} else {
		/* atf_err("Empty table,no setting to pass firmware!\n");*/
		retv = -1;
	}

	return retv;
}

static int32_t atf_vrf_cfg_value(struct atf_config *cfg)
{
	int32_t retv = 0;
	uint32_t vdev_cfg_added = 0;
	uint32_t peer_cfg_added = 0;
	uint32_t sta_value_cfg = 0;
	uint32_t sta_value_global = 0;
	uint32_t per_others = 0;
	uint8_t vdev_num = 0;
	uint8_t i = 0, j = 0;

	vdev_num = cfg->vdev_num_cfg;
	for (i = 0; (i < ATF_CFG_NUM_VDEV) && (vdev_num != 0); i++) {
		if (cfg->vdev[i].cfg_flag) {
			vdev_cfg_added += cfg->vdev[i].vdev_cfg_value;
			vdev_num--;
		}
	}

	if (vdev_cfg_added > cfg->percentage_unit) {
		retv = -1;
		atf_err("\nVDEVs configuration value assignment wrong!!\n");
		goto end_vrf_atf_cfg;
	} else {
		per_others = cfg->percentage_unit - vdev_cfg_added;
		atf_info("VDEVs percentage for other: %d\n", (per_others/10));
	}
	vdev_num = cfg->vdev_num_cfg;
	peer_cfg_added = 0;
	for (i = 0; (i < ATF_CFG_NUM_VDEV) && (vdev_num != 0); i++) {
		if (cfg->vdev[i].cfg_flag) {
			vdev_num--;
			peer_cfg_added = 0;
			for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
				if (cfg->peer_id[j].index_vdev == (i + 1)) {
					if (cfg->peer_id[j].cfg_flag) {
						sta_value_cfg =
						cfg->peer_id[j].sta_cfg_value[
						cfg->peer_id[j].index_vdev];

						peer_cfg_added +=
						((cfg->vdev[i].vdev_cfg_value
						* sta_value_cfg)
						/ cfg->percentage_unit);
						/**
						* Check for global percentage
						* for the sta as we will use
						* global if per ssid config
						* is not done.
						**/
						if (!sta_value_cfg) {
							sta_value_global =
							((cfg->vdev[
							i].vdev_cfg_value *
							cfg->peer_id[
							j].sta_cfg_value[
							ATF_CFG_GLOBAL_INDEX])/
							cfg->percentage_unit);
							peer_cfg_added +=
							sta_value_global;
						}
					}
				}
			}
			if (peer_cfg_added > cfg->vdev[i].vdev_cfg_value) {
				retv = -1;
				atf_err("Wrong Peer Value. Reassign\n");
				goto end_vrf_atf_cfg;
			}
		}
	}

	peer_cfg_added = 0;
	sta_value_global = 0;
	for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
		if (cfg->peer_id[j].index_vdev == 0xFF) {
			if (cfg->peer_id[j].cfg_flag) {
				sta_value_global =
						cfg->peer_id[j].sta_cfg_value[
							ATF_CFG_GLOBAL_INDEX];
				peer_cfg_added +=
						((per_others * sta_value_global)
						/ cfg->percentage_unit);
			}
		}
	}
	if (peer_cfg_added > per_others) {
		retv = -1;
		atf_err("\nPeers configuration Global value assignment wrong!!"
						" Reassign the values\n");
	}

end_vrf_atf_cfg:
	return retv;
}

static int32_t atf_vrf_peer_value(struct atf_config  *atfcfg)
{
	int32_t retv = 0;
	uint32_t peer_cfg_added = 0;
	uint32_t sta_value_cfg = 0;
	uint32_t sta_value_global = 0;
	uint8_t i = 0;

	for (i = 0; i < ATF_ACTIVED_MAX_CLIENTS; i++) {
		if (atfcfg->peer_id[i].cfg_flag) {
			sta_value_cfg = atfcfg->peer_id[i].sta_cfg_value[
						atfcfg->peer_id[i].index_vdev];
			peer_cfg_added += sta_value_cfg;
			/* Check for global percentage for the sta as we will
			use global if per ssid config is not done. */
			if (!sta_value_cfg) {
				sta_value_global =
					atfcfg->peer_id[i].sta_cfg_value[
							ATF_CFG_GLOBAL_INDEX];
				peer_cfg_added += sta_value_global;
				atf_info("\nVerifying global percentage %d of"
				" peer_id[%d]\n", (sta_value_global/10),i);
			}
		}
	}
	if (peer_cfg_added > atfcfg->percentage_unit) {
		retv = -1;
		atf_err("\nPeers configuration value assignment wrong!!"
						" Reassign the Values\n");
	}

	return retv;
}

uint32_t atf_sta_cal_unconfigured_peers(struct pdev_atf *pa,
					uint32_t tot_airtime, uint32_t peer_cnt,
					uint32_t peerbitmap)
{
	u_int32_t calavgval = 0;
	int calbitmap = 1;
	int j = 0;

	if (NULL == pa) {
		atf_err("pa is null!\n");
		return EINVAL;
	}
	calavgval = tot_airtime / peer_cnt;
	for (j = 0; j<ATF_ACTIVED_MAX_CLIENTS; j++) {
		if (peerbitmap & (calbitmap << j)) {
			pa->atfcfg_set.peer_id[j].sta_cal_value = calavgval;
			tot_airtime -= pa->atfcfg_set.peer_id[j].sta_cal_value;
		}
	}

	return tot_airtime;
}

int32_t atf_sta_cal_configured_peers(struct pdev_atf *pa, uint32_t peer_idx,
				     uint32_t cfg_value, uint32_t cfg_idx)
{
	int ret = 0;
	uint32_t sta_cal = 0, peer_global_per = 0;
	struct atf_config *cfg = NULL;

	if (NULL == pa) {
		atf_err("pa is null!\n");
		return EINVAL;
	}
	cfg = &pa->atfcfg_set;
	if ((peer_idx < ATF_ACTIVED_MAX_CLIENTS) &&
	    cfg->peer_id[peer_idx].sta_assoc_status == 1) {
		sta_cal = ((cfg_value *
			    cfg->peer_id[peer_idx].sta_cfg_value[cfg_idx])
			    / cfg->percentage_unit);
		peer_global_per =
		    cfg->peer_id[peer_idx].sta_cfg_value[ATF_CFG_GLOBAL_INDEX];
		if (sta_cal) {
			cfg->peer_id[peer_idx].sta_cal_value = sta_cal;
			ret = ATF_PEER_CFG_HIERARCHY;
		} else if(peer_global_per) {
			cfg->peer_id[peer_idx].sta_cal_value = peer_global_per;
			ret = ATF_PEER_CFG_GLOBAL;
		} else {
			cfg->peer_id[peer_idx].sta_cal_value = 0;
			ret = ATF_PEER_CFG_UNCONFIGURED;
		}
	} else {
		cfg->peer_id[peer_idx].sta_cal_value = 0;
		ret = ATF_PEER_CFG_NOTASSOCIATED;
	}

	return ret;
}

int32_t atf_get_total_ac_config_val(struct atf_ac_config *atfac)
{
	uint8_t acbitmap = 0;
	uint32_t config_val = 0, i = 0;

	if (NULL == atfac) {
		atf_err("ATF AC config is NULL\n");
		return -1;
	}

	acbitmap = atfac->ac_bitmap;
	while (acbitmap) {
		if (1 & acbitmap) {
			/* Get configured value */
			config_val += atfac->ac[i][0];
		}
		i++;
		acbitmap = acbitmap >> 1;
	}

	return config_val;
}

QDF_STATUS atf_assign_airtime_default_group(struct pdev_atf *pa,
					    uint32_t airtime_unconfigured)
{
	struct atf_group_list *group = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;

	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					 group_next);
		if (group) {
			if (!qdf_str_cmp(group->group_name,DEFAULT_GROUPNAME)) {
				group->group_airtime = airtime_unconfigured;
				pa->atfcfg_set.atfgroup[0].grp_cfg_value =
							group->group_airtime;
			}
		}
		next_node = NULL;
		if (QDF_STATUS_SUCCESS !=
		    qdf_list_peek_next(&pa->atf_groups, node, &next_node))
			break;
		node = next_node;
	}

	return QDF_STATUS_SUCCESS;
}

int32_t
cal_atf_ac_config(struct pdev_atf *pa, uint32_t index, uint64_t peerbitmap,
		  uint32_t residual_airtime, uint8_t cfg_type)
{
	uint32_t j = 0, i = 0, calbitmap = 0;
	struct atf_ac_config *atfac = NULL;
	uint32_t clientindex = 0;
	uint8_t acbitmap = 0;
	uint32_t config_ac_val = 0, tot_ac_val = 0, airtime_vdev = 0;
	struct atf_config *cfg = &pa->atfcfg_set;

	if (cfg_type == ATF_CFGTYPE_SSID) {
		atfac = &cfg->vdev[index].atf_cfg_ac;
	} else if (cfg_type == ATF_CFGTYPE_GROUP) {
		atfac = &cfg->atfgroup[index].atf_cfg_ac;
	}
	if (!atfac) {
		atf_err("atfac is NULL. Return... \n");
		return -1;
	}
	if (!atfac->ac_cfg_flag) {
		return -1;
	}

	for (j = 0, calbitmap = 1; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
		/* Index of vap/group to which the client is associated */
		if (cfg_type == ATF_CFGTYPE_SSID) {
			clientindex = cfg->peer_id[j].index_vdev;
			airtime_vdev = cfg->vdev[index].vdev_cfg_value;
		} else {
			clientindex = cfg->peer_id[j].index_group;
			airtime_vdev = cfg->atfgroup[index].grp_cfg_value;
		}
		if (!clientindex) {
			continue;
		}
		if (peerbitmap & (calbitmap << j)) {
			if ((clientindex - 1) == index) {
				i = 0;
				acbitmap = atfac->ac_bitmap;
				while (acbitmap) {
					if (1 & acbitmap) {
						/* Get configured value */
						config_ac_val = atfac->ac[i][0];
						cfg->peer_id[j].
						           sta_cal_ac_value[i] =
							((config_ac_val *
							  residual_airtime) /
							  cfg->percentage_unit);
					}
					i++;
					acbitmap = acbitmap >> 1;
				}
			}
		}
	}
	/* Get total airtime configured for AC per vap/group */
	tot_ac_val = atf_get_total_ac_config_val(atfac);

	return tot_ac_val;
}

static void atf_cal_tbl_group_based(struct pdev_atf *pa)
{
	uint8_t stacnt = 0, j = 0, i = 0;
	uint8_t peer_total_cnt = 0;
	uint8_t grp_num = 0;
	uint8_t un_assoc_cfg_peer = 0;
	uint32_t per_unit = 0;
	int32_t ac_config_val = 0, peer_cfg_type = 0;
	int32_t peer_cfg_cnt = 0;
	uint32_t calavgval = 0;
	uint32_t grp_per_unit = 0;
	uint64_t peerbitmap, calbitmap;
	struct atf_config *atfcfg = &pa->atfcfg_set;

	peer_total_cnt = atfcfg->peer_num_cal;
	grp_num = atfcfg->grp_num_cfg - 1;
	per_unit = atfcfg->percentage_unit;
	for (i = 1; (i < ATF_ACTIVED_MAX_ATFGROUPS) && (grp_num != 0); i++) {
		grp_per_unit = atfcfg->atfgroup[i].grp_cfg_value;
		per_unit -= grp_per_unit;
		grp_num--;
		stacnt = 0;
		peerbitmap = 0;
		calbitmap = 1;
		for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
			if (atfcfg->peer_id[j].index_group == (i + 1)) {
				if (atfcfg->peer_id[j].cfg_flag) {
					peer_cfg_type =
						atf_sta_cal_configured_peers(pa,
							 j,atfcfg->atfgroup[i].
							 grp_cfg_value,
							 atfcfg->peer_id[j].
							 index_group);
					if ((peer_cfg_type ==
					     ATF_PEER_CFG_HIERARCHY) ||
					     (peer_cfg_type ==
					      ATF_PEER_CFG_GLOBAL)) {
						grp_per_unit -=
					       atfcfg->peer_id[j].sta_cal_value;
						peer_total_cnt--;
					} else if (peer_cfg_type ==
						   ATF_PEER_CFG_UNCONFIGURED) {
						peerbitmap |= (calbitmap << j);
						stacnt++;
					} else if (peer_cfg_type ==
						ATF_PEER_CFG_NOTASSOCIATED) {
					}
					peer_cfg_cnt--;
				} else {
					peerbitmap |= (calbitmap << j);
					stacnt++;
				}
			}
		}
		if (stacnt) {
			ac_config_val = cal_atf_ac_config(pa, i, peerbitmap,
							  grp_per_unit,
							  ATF_CFGTYPE_GROUP);
			if ((ac_config_val >= 0) &&
				(ac_config_val > atfcfg->percentage_unit)) {
				atf_info("Wrong AC config %d, \
					  Cannot be more than %d\n",
					  ac_config_val,
					  atfcfg->percentage_unit);
			} else if (ac_config_val >= 0) {
				grp_per_unit = ((atfcfg->percentage_unit -
						 ac_config_val) * grp_per_unit)
						 / atfcfg->percentage_unit;
			}
			atf_sta_cal_unconfigured_peers(pa, grp_per_unit,
						       stacnt, peerbitmap);
			peer_total_cnt -= stacnt;
		}
	}
	/* Allocate unconfigured airtime to default group */
	atf_assign_airtime_default_group(pa, per_unit);

	/* Handle left stations that do not include in config vap */
	if (peer_total_cnt != 0) {
		calavgval = per_unit / peer_total_cnt;
		for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
			if (atfcfg->peer_id[j].index_group == 0xFF) {
				if (atfcfg->peer_id[j].sta_assoc_status == 1) {
					atfcfg->peer_id[j].sta_cal_value =
								calavgval;
				} else {
					atfcfg->peer_id[j].sta_cal_value = 0;
					un_assoc_cfg_peer++;
				}
			}
		}
	}
}

#define ATF_CFG_STA_CAL()                                                      \
	if (cfg->peer_id[j].sta_assoc_status == 1) {                           \
		cfg->peer_id[j].sta_cal_value = (cfg->vdev[i].vdev_cfg_value * \
		cfg->peer_id[j].sta_cfg_value[cfg->peer_id[j].index_vdev])     \
						/ cfg->percentage_unit;        \
		/* Get the global  percentage for this SSID if present */      \
		peer_global_per =                                              \
			cfg->peer_id[j].sta_cfg_value[ATF_CFG_GLOBAL_INDEX];   \
		if (cfg->peer_id[j].sta_cal_value) {                           \
			vdev_per_unit -= cfg->peer_id[j].sta_cal_value;        \
			peer_total_cnt--;                                      \
		} else if (peer_global_per) {                                  \
			cfg->peer_id[j].sta_cal_value =                        \
			(cfg->vdev[i].vdev_cfg_value * peer_global_per) /      \
							cfg->percentage_unit;  \
			vdev_per_unit -= cfg->peer_id[j].sta_cal_value;        \
			peer_total_cnt--;                                      \
		} else {                                                       \
			atf_info("Peer mac : %s\n",                            \
				       ether_sprintf(cfg->peer_id[j].sta_mac));\
			atf_info("Peer has no specific percentage for this " \
					"SSID neither global\n");              \
			peerbitmap |= (calbitmap << j);                        \
			stacnt++;                                              \
		}                                                              \
	} else {                                                               \
		cfg->peer_id[j].sta_cal_value = 0;                             \
		un_assoc_cfg_peer++;                                           \
	}

static void atf_cal_tbl_vdev_based(struct pdev_atf *pa, struct atf_context *ac)
{
	uint8_t stacnt = 0, j = 0, i = 0;
	uint8_t vdev_num = 0;
	uint8_t peer_total_cnt = 0;
	uint8_t peer_cfg_cnt = 0;
	uint8_t un_assoc_cfg_peer = 0;
	uint8_t peer_with_zero_val = 0;
	uint32_t peer_global_per = 0;
	uint32_t peerunits_others = 0;
	uint32_t vdev_per_unit = 0;
	uint32_t per_unit = 0;
	uint32_t calavgval = 0;
	int32_t ac_config_val = 0;
	uint64_t peerbitmap, calbitmap;
	struct atf_config *cfg = &pa->atfcfg_set;

	peer_total_cnt = cfg->peer_num_cal;
	vdev_num = cfg->vdev_num_cfg;
	per_unit = cfg->percentage_unit;
	peer_cfg_cnt = cfg->peer_num_cfg;
	for (i = 0; (i < ATF_CFG_NUM_VDEV) && (vdev_num != 0); i++) {
		if (cfg->vdev[i].cfg_flag) {
			vdev_per_unit = cfg->vdev[i].vdev_cfg_value;
			per_unit -= vdev_per_unit;
			vdev_num--;
			stacnt = 0;
			peerbitmap = 0;
			calbitmap = 1;
			for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
				if (cfg->peer_id[j].index_vdev == (i + 1)) {
					if (cfg->peer_id[j].cfg_flag) {
						ATF_CFG_STA_CAL();
						peer_cfg_cnt--;
					} else {
						peerbitmap |= (calbitmap << j);
						stacnt++;
					}
				}
			}
			if (stacnt) {
				ac_config_val = cal_atf_ac_config(pa, i,
							      peerbitmap,
							      vdev_per_unit,
							      ATF_CFGTYPE_SSID);
				if ((ac_config_val >= 0) &&
				    (ac_config_val > cfg->percentage_unit)) {
					atf_info("Wrong ac config %d.\
						  Cannot be more than %d\n",
						  ac_config_val,
						  cfg->percentage_unit);
				} else if (ac_config_val >= 0) {
					vdev_per_unit = ((cfg->percentage_unit -
							  ac_config_val)
							 * vdev_per_unit) /
							 cfg->percentage_unit;
				}
				calavgval = vdev_per_unit / stacnt;
				calbitmap = 1;
				for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
					if (peerbitmap & (calbitmap << j)) {
						cfg->peer_id[j].sta_cal_value
								= calavgval;
						/*Decrease net vap percentage*/
						vdev_per_unit -=
						cfg->peer_id[j].sta_cal_value;
					}
				}
				peer_total_cnt -= stacnt;
			}
			if (ac->atf_update_group_tbl) {
				ac->atf_update_group_tbl(pa, stacnt, i,
								vdev_per_unit);
			}
		}
	} /*End of loop*/
	/* Allocate unconfigured airtime to default group */
	atf_assign_airtime_default_group(pa, per_unit);
	/* Handle left stations that do not include in config vap */
	if (peer_total_cnt == 0)
		return;
	if (peer_cfg_cnt > 0) {
		for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
			if ((cfg->peer_id[j].index_vdev == 0xff) &&
			    (cfg->peer_id[j].cfg_flag == 1)) {
				if (cfg->peer_id[j].sta_assoc_status == 1) {
					/*Apply global percentage for the peer*/
					cfg->peer_id[j].sta_cal_value =
					((per_unit *
					cfg->peer_id[j].sta_cfg_value[
						ATF_CFG_GLOBAL_INDEX])
						/ cfg->percentage_unit);
					/* Total Airtime assigned to
					 * peers in 'Others' category
					 */
					if (cfg->peer_id[j].sta_cal_value) {
						peerunits_others +=
						cfg->peer_id[j].sta_cal_value;
						peer_total_cnt--;
					} else {
						peer_with_zero_val++;
					}
				} else {
					cfg->peer_id[j].sta_cal_value = 0;
					un_assoc_cfg_peer++;
				}
				peer_cfg_cnt--;
			}
		}
		per_unit -= peerunits_others;
		peerunits_others = 0;
		/*Split equally among peer with zero value as these have been
		configured but no % for this vdev */
		if (peer_with_zero_val) {
			calavgval = per_unit/peer_total_cnt;
			for ( j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
				if ((cfg->peer_id[j].index_vdev == 0xff) &&
					(cfg->peer_id[j].cfg_flag == 1) &&
					!(cfg->peer_id[j].sta_cal_value)) {
					if (cfg->peer_id[j].sta_assoc_status ==
						1) {
						cfg->peer_id[j].sta_cal_value =
								calavgval;
						peerunits_others +=
						cfg->peer_id[j].sta_cal_value;
						peer_total_cnt--;
					} else {
						cfg->peer_id[j].sta_cal_value =
									0;
						un_assoc_cfg_peer++;
					}
				}
			}
		}
	}
	per_unit -= peerunits_others;
	if (peer_total_cnt > 0) {
		calavgval = per_unit / peer_total_cnt;
		for (j = 0; j < ATF_ACTIVED_MAX_CLIENTS; j++) {
			if ((cfg->peer_id[j].index_vdev == 0xff) &&
			    (cfg->peer_id[j].cfg_flag == 0)) {
				if (cfg->peer_id[j].sta_assoc_status == 1) {
					cfg->peer_id[j].sta_cal_value =
								calavgval;
					peer_total_cnt--;
				} else {
					cfg->peer_id[j].sta_cal_value = 0;
					un_assoc_cfg_peer++;
				}
			}
		}
	}
}

static int32_t atf_cal_tbl_peer_based(struct pdev_atf *pa)
{
	int32_t retv = 0;
	uint8_t calcnt = 0, i = 0;
	uint8_t un_assoc_cfg_peer = 0;
	uint8_t peer_with_zero_val = 0;
	uint32_t peer_global_per = 0;
	uint32_t calavgval = 0;
	uint32_t per_unit = 0;
	uint64_t calbitmap;
	struct atf_config *atfcfg = &pa->atfcfg_set;

	per_unit = atfcfg->percentage_unit;
	peer_with_zero_val = 0;
	calcnt = atfcfg->peer_num_cfg;
	calbitmap = 1;
	for (i = 0; ((i < ATF_ACTIVED_MAX_CLIENTS) && (calcnt != 0)); i++) {
		if (atfcfg->peer_id[i].cfg_flag) {
			if (atfcfg->peer_id[i].sta_cfg_value[
					ATF_CFG_GLOBAL_INDEX] <= per_unit) {
				if (atfcfg->peer_id[i].sta_assoc_status == 1) {
					/*Get the global percentage for this
					SSID if present*/
					peer_global_per = atfcfg->peer_id[
					i].sta_cfg_value[ATF_CFG_GLOBAL_INDEX];
					/* This is when we are in no vap host
					config mode so will use global config
					if set for a  peer*/
					if (peer_global_per) {
						atfcfg->peer_id[i].sta_cal_value
							= peer_global_per;
						per_unit -=peer_global_per;
					} else {
						/*No global percentage;
						Lets treat these as
						unconfigured peers*/
						peer_with_zero_val++;
					}
				} else {
					atfcfg->peer_id[i].sta_cal_value = 0;
					un_assoc_cfg_peer++;
				}
				calcnt--;
			} else {
				atf_info("Wrong input percentage value "
								"for peer!!\n");
				retv = -1;
				break;
			}
		}
	}
	if (atfcfg->peer_num_cal >=
		(atfcfg->peer_num_cfg - un_assoc_cfg_peer)) {
		calcnt = atfcfg->peer_num_cal -
			(atfcfg->peer_num_cfg - un_assoc_cfg_peer);
		/* The peers for which no global percentage has been alloted */
		calcnt += peer_with_zero_val;
		if (calcnt) {
			calavgval = per_unit / calcnt;
			calbitmap = 1;
			for (i = 0; i < ATF_ACTIVED_MAX_CLIENTS; i++) {
				peer_global_per =
					atfcfg->peer_id[i].sta_cfg_value[
							ATF_CFG_GLOBAL_INDEX];
				if ( (atfcfg->peer_id[i].cfg_flag == 0) ||
					((atfcfg->peer_id[i].cfg_flag == 1) &&
							!peer_global_per) ) {
					if (atfcfg->peer_cal_bitmap &
					    (calbitmap << i)) {
						atfcfg->peer_id[i].sta_cal_value
								= calavgval;
					}
				}
			}
		}
	} else {
		atf_info("Wrong input percentage value for peer!\n");
		retv = -1;
	}

	atf_assign_airtime_default_group(pa, per_unit);

	return retv;
}

static int32_t atf_cal_alloc_tbl(struct wlan_objmgr_pdev *pdev)
{
	int32_t retv = 0;
	uint8_t i = 0;
	uint32_t calavgval = 0;
	uint64_t calbitmap;
	struct pdev_atf *pa;
	struct atf_config *atfcfg;
	struct atf_context *ac;

	if (NULL == pdev) {
		atf_err("pdev object is null!\n");
		return -1;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev atf is null!\n");
		return -1;
	}
	ac = atf_get_atf_ctx_from_pdev(pdev);
	if (NULL == ac) {
		atf_err("ac is null!\n");
		return -1;
	}
	atfcfg = &pa->atfcfg_set;

	if (pa->atf_ssidgroup) {
		/* Parse the group list and remove any group marked
		 * for deletion
		 */
		atf_del_group_subgroup(pa);
		atf_cal_tbl_group_based(pa);
	} else if (atfcfg->vdev_num_cfg) {
		if (ac->atf_set_group_unused_airtime)
			ac->atf_set_group_unused_airtime(pa);
		atf_del_group_subgroup(pa);
		retv = atf_vrf_cfg_value(atfcfg);
		if (retv != 0)
			goto end_cal_atf;

		atf_cal_tbl_vdev_based(pa, ac);
	} else {
		/* atf_info("cal_atf_alloc_tbl NO VAP host config mode\n");*/
		atf_del_group_subgroup(pa);
		/* Allocate unconfigured airtime to default group */
		atf_assign_airtime_default_group(pa,
				pa->atfcfg_set.percentage_unit);
		if (atfcfg->peer_num_cfg) {
			retv = atf_vrf_peer_value(atfcfg);
			if (retv != 0)
				goto end_cal_atf;

			retv = atf_cal_tbl_peer_based(pa);
		} else {
			if (atfcfg->peer_num_cal) {
				calavgval = (atfcfg->percentage_unit /
							atfcfg->peer_num_cal);
				calbitmap = 1;
				for (i = 0; i < ATF_ACTIVED_MAX_CLIENTS; i++) {
					if (atfcfg->peer_cal_bitmap &
							(calbitmap << i)) {
						atfcfg->peer_id[i].sta_cal_value
								= calavgval;
					}
				}
			} else {
				atf_info("Empty table, no para setting to "
							"pass firmware!\n");
				retv = -1;
			}
		}
	}

end_cal_atf:
	return retv;
}

static void atf_log_alloc(struct pdev_atf *pa)
{
	uint8_t i = 0, j = 0, group_index = 0xFF, vdev_index = 0xFF;
	uint8_t *ssid = NULL;
	u_int8_t acbitmap = 0, ac_id = 0;
	struct atf_ac_config *atfac = NULL;
	struct atf_config *cfg = &pa->atfcfg_set;

	if (NULL == pa) {
		atf_err("pdev ATF object is NULL \n");
		return;
	}
	/* only need to log atf while commitatf 1 */
	if (!pa->atf_commit)
		return;
	/* Peer by Peer look up vap in alloc table, then program peer table */
	for (i = 0; (i < ATF_ACTIVED_MAX_CLIENTS) && (j < cfg->peer_num_cal);
									i++) {
		if (cfg->peer_id[i].sta_assoc_status == 1) {
			ac_id = acbitmap = 0;
			/* Determine ssid_name */
			group_index = cfg->peer_id[i].index_group;
			vdev_index = cfg->peer_id[i].index_vdev;
			ssid = "Others";
			if (group_index &&
			    (group_index <= ATF_ACTIVED_MAX_ATFGROUPS)) {
				/* First SSID in group */
				ssid =
				cfg->atfgroup[group_index - 1].grp_ssid[0];
			} else if (vdev_index &&
				   (vdev_index <= ATF_CFG_NUM_VDEV)) {
				ssid = cfg->vdev[vdev_index - 1].essid;
			}
			if (pa->atf_ssidgroup) {
				if (group_index &&
				    (group_index < ATF_ACTIVED_MAX_ATFGROUPS)) {
					atfac = &cfg->atfgroup[group_index -
								  1].atf_cfg_ac;
				}
			} else {
				if (vdev_index &&
				    (vdev_index < ATF_CFG_NUM_VDEV)) {
					atfac = &cfg->vdev[vdev_index -
								  1].atf_cfg_ac;
				}
			}
			if (atfac) {
				acbitmap = atfac->ac_bitmap;
			}
			if (((vdev_index < ATF_CFG_NUM_VDEV) &&
				cfg->peer_id[i].cfg_flag &&
				cfg->peer_id[i].sta_cfg_value[vdev_index]) ||
				!acbitmap) {
				atf_info("client %s connected to SSID %s has \
					  %d%% airtime\n",
					 ether_sprintf(cfg->peer_id[i].sta_mac),
					 ssid, (cfg->peer_id[i].sta_cal_value /
						10));
			} else if (acbitmap) {
				atf_info("client %s connected to SSID %s has \
					 following ACs configured - ",
					 ether_sprintf(cfg->peer_id[i].sta_mac),
					 ssid);
				ac_id = 0;
				while (acbitmap) {
					if (1 & acbitmap) {
						atf_info("  AC%d:%d%%",
							 ac_id, cfg->peer_id[i].
							 sta_cal_ac_value[ac_id]
							 / 10);
					}
					ac_id++;
					acbitmap = acbitmap >> 1;
				}
				atf_info("\n");
			}
			j++;
		}
	}
}

void atf_dump_subgroup_allocation(struct pdev_atf *pa)
{
	struct atf_subgroup_list *subgroup = NULL;
	struct atf_group_list *group = NULL;
	qdf_list_node_t *node = NULL, *next_node = NULL;
	qdf_list_node_t *tmpnode = NULL, *tmpnode_next = NULL;

	if (NULL == pa) {
		atf_err("pdev atf is null!\n");
		return;
	}
	qdf_list_peek_front(&pa->atf_groups, &node);
	while (node) {
		group = qdf_container_of(node, struct atf_group_list,
					group_next);
		if (group) {
			next_node = NULL;
			qdf_list_peek_front(&group->atf_subgroups, &tmpnode);
			while (tmpnode) {
				subgroup = qdf_container_of(tmpnode,
						       struct atf_subgroup_list,
						       sg_next);
                tmpnode_next = NULL;
				qdf_print("\nGroup : %s Subgroup: %s \
					  Airtime: %d", group->group_name,
					  subgroup->sg_name,
					  subgroup->sg_atf_units);
				qdf_list_peek_next(&group->atf_subgroups,
						   tmpnode, &tmpnode_next);
				tmpnode = tmpnode_next;
			}
		}
		qdf_list_peek_next(&pa->atf_groups, node, &next_node);
		node = next_node;
		qdf_print("\n");
	}
}

static void atf_dump_peer_subgroup(struct wlan_objmgr_pdev *pdev,
				   void *object, void *arg)
{
	struct wlan_objmgr_peer *peer = (struct wlan_objmgr_peer *)object;
	struct peer_atf *pa;
	struct atf_subgroup_list *subgroup = NULL;
	int ac_id = 0;

    pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
    if (!pa) {
        return;
    }
    for (ac_id = 0; ac_id < 4; ac_id++) {
        subgroup = pa->ac_list_ptr[ac_id];
        if (NULL == subgroup) {
            return;
        }
    }
}

void atf_cfg_timer_handler(void *arg)
{
	struct wlan_objmgr_pdev *pdev = (struct wlan_objmgr_pdev *)arg;
	struct pdev_atf *pa;
	struct atf_context *ac;
	int32_t retv = 0;
	struct wlan_objmgr_psoc *psoc;

	if (NULL == pdev) {
		atf_err("PDEV is NULL!\n");
		return;
	}
	psoc = wlan_pdev_get_psoc(pdev);
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("PDEV ATF object is NULL!\n");
		return;
	}
	ac = atf_get_atf_ctx_from_pdev(pdev);
	if (NULL == ac) {
		atf_err("ATF context is NULL!\n");
		return;
	}

	qdf_spin_lock(&pa->atf_lock);
	/* 1. Build atf table */
	retv = atf_build_alloc_tbl(pdev);
	if (retv != 0)
		goto exit_timer;

	/* 2. Calculate vdev and sta % for whole table */
	retv = atf_cal_alloc_tbl(pdev);
	if (retv != 0)
		goto exit_timer;

	atf_log_alloc(pa);
	atf_assign_airtime_subgroup(pa);
	if (ac->atf_update_tbl_to_fm)
		retv = ac->atf_update_tbl_to_fm(pdev);
	if (retv != 0)
		atf_info("Failed to update ATF table to firmware!\n");

exit_timer:
	atf_remove_group(pa);
	pa->atf_tasksched = 0;
	qdf_spin_unlock(&pa->atf_lock);
	if (psoc != NULL) {
		wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_PEER_OP,
						  atf_dump_peer_subgroup, psoc,
						  0, WLAN_ATF_ID);
	}
}

void atf_start_cfg_timer(struct wlan_objmgr_vdev *vdev, struct pdev_atf *pa,
			 struct atf_context *ac)
{
	qdf_spin_lock(&pa->atf_lock);
	if ((ac->atf_fmcap) && (ac->atf_mode)) {
		if (pa->atf_tasksched == 0) {
			pa->atf_tasksched = 1;
			qdf_timer_mod(&pa->atfcfg_timer,
				      ATF_CFG_WAIT * 1000);
		}
	}
	qdf_spin_unlock(&pa->atf_lock);
}

void atf_update_tput_tbl(struct wlan_objmgr_pdev *pdev,
			 struct wlan_objmgr_peer *peer, uint8_t type)
{
	struct pdev_atf *pa = NULL;
	struct peer_atf *pa_obj = NULL;
	struct atf_context *ac = NULL;
	uint8_t i = 0, order = 0;
	uint8_t *peer_mac = NULL;

	if ((NULL == pdev) || (NULL == peer)) {
		atf_err("pdev or peer are NULL!\n");
		return;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL == pa) {
		atf_err("pdev atf object is NULL!\n");
		return;
	}
	pa_obj = wlan_objmgr_peer_get_comp_private_obj(peer,
						       WLAN_UMAC_COMP_ATF);
	wlan_peer_obj_lock(peer);
	peer_mac = wlan_peer_get_macaddr(peer);
	wlan_peer_obj_unlock(peer);
	if (NULL == pa_obj) {
		atf_err("peer atf object is NULL!\n");
		return;
	}

	ac = atf_get_atf_ctx_from_pdev(pdev);
	if (NULL == ac) {
		atf_err("atf context is NULL!\n");
		return;
	}

	if (type) {
		for (i = 0; i < ATF_TPUT_MAX_STA; i++) {
			if (!qdf_mem_cmp(pa->atf_tput_tbl[i].mac_addr, peer_mac,
					 QDF_MAC_ADDR_SIZE)) {
				if (!pa->atf_tput_tbl[i].order) {
					pa->atf_tput_tbl[i].order =
						++pa->atf_tput_order_max;
				}
				break;
			}
		}
	} else {
		for (i = 0; i < ATF_TPUT_MAX_STA; i++) {
			if (!qdf_mem_cmp(pa->atf_tput_tbl[i].mac_addr, peer_mac,
					 QDF_MAC_ADDR_SIZE)) {
				order = pa->atf_tput_tbl[i].order;
				pa->atf_tput_tbl[i].order = 0;
				break;
			}
		}
		if (order) {
			for (i = 0; i < ATF_TPUT_MAX_STA; i++) {
				if (pa->atf_tput_tbl[i].order > order)
					pa->atf_tput_tbl[i].order--;
			}
			pa->atf_tput_order_max--;
		}
	}
	if (ac->atf_build_bwf_for_fm)
		ac->atf_build_bwf_for_fm(pdev);
}

static void atf_peer_init(struct wlan_objmgr_peer *peer,
			  struct peer_atf *pa_obj)
{
	struct wlan_objmgr_vdev *vdev = NULL;
	struct pdev_atf *pa = NULL;
	uint8_t *mac_addr = NULL;
	uint8_t i = 0;

	if ((NULL == peer) || (NULL == pa_obj)) {
		atf_err("PEER or PEER ATF object is NULL\n");
		return;
	}
	vdev = wlan_peer_get_vdev(peer);
	if (NULL == vdev) {
		atf_err("vdev is NULL\n");
		return;
	}
	pa = atf_get_pdev_atf_from_vdev(vdev);
	if (NULL == pa) {
		atf_err("pdev atf object is NULL!\n");
		return;
	}
	mac_addr = wlan_peer_get_macaddr(peer);

	pa_obj->peer_obj = peer;
	pa_obj->block_tx_traffic = 0;
	pa_obj->atf_tput = 0;
	pa_obj->atf_airtime = 0;
	pa_obj->atf_airtime_new = 0;
	pa_obj->atf_airtime_more = 0;
	pa_obj->atf_airtime_subseq = 0;
	pa_obj->atf_airtime_cap = 0;
	pa_obj->atf_airtime_configured = 0;

	if (pa->atf_tput_based) {
		for (i = 0; i < ATF_TPUT_MAX_STA; i++) {
			if (!qdf_mem_cmp(pa->atf_tput_tbl[i].mac_addr, mac_addr,
					 QDF_MAC_ADDR_SIZE)) {
				pa_obj->atf_tput = pa->atf_tput_tbl[i].tput;
				pa_obj->atf_airtime_cap =
						pa->atf_tput_tbl[i].airtime;
			}
		}
	}
}

static void atf_ctx_deinit(struct atf_context *ac)
{
	if (NULL != ac) {
		ac->atf_pdev_atf_init = NULL;
		ac->atf_pdev_atf_deinit = NULL;
		ac->atf_set = NULL;
		ac->atf_clear = NULL;
		ac->atf_set_maxclient = NULL;
		ac->atf_get_maxclient = NULL;
	}
}

QDF_STATUS
wlan_atf_psoc_obj_create_handler(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct atf_context *ac = NULL;

	if (NULL == psoc) {
		atf_err("PSOC is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}
	ac = (struct atf_context *)qdf_mem_malloc(sizeof(struct atf_context));
	if (NULL == ac) {
		atf_err("Failed to allocate atf_ctx object\n");
		return QDF_STATUS_E_NOMEM;
	}
	ac->psoc_obj = psoc;
#if DA_SUPPORT
	if (WLAN_DEV_DA == wlan_objmgr_psoc_get_dev_type(psoc))
		atf_ctx_init_da(ac);
#endif
	if (WLAN_DEV_OL == wlan_objmgr_psoc_get_dev_type(psoc))
		atf_ctx_init_ol(ac);
	wlan_objmgr_psoc_component_obj_attach(psoc, WLAN_UMAC_COMP_ATF,
						(void *)ac, QDF_STATUS_SUCCESS);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_atf_psoc_obj_destroy_handler(struct wlan_objmgr_psoc *psoc, void *arg)
{
	struct atf_context *ac = NULL;

	if (NULL == psoc) {
		atf_err("PSOC is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}
	ac = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_ATF);
	if (NULL != ac) {
		wlan_objmgr_psoc_component_obj_detach(psoc, WLAN_UMAC_COMP_ATF,
						      (void *)ac);
		atf_ctx_deinit(ac);
		/* Deinitilise function pointers from atf context */
		qdf_mem_free(ac);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_atf_pdev_obj_create_handler(struct wlan_objmgr_pdev *pdev, void *arg)
{
	struct pdev_atf *pa = NULL;
	struct atf_context *ac = NULL;

	if (NULL == pdev) {
		atf_err("PDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	pa = (struct pdev_atf *)qdf_mem_malloc(sizeof(struct pdev_atf));
	if (NULL == pa) {
		atf_err("Failed to allocate pdev_atf object\n");
		return QDF_STATUS_E_NOMEM;
	}
	ac = atf_get_atf_ctx_from_pdev(pdev);
	if (NULL == ac) {
		atf_err("ATF context is NULL!\n");
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_zero(pa, sizeof(struct pdev_atf));
	pa->pdev_obj = pdev;
	if (ac->atf_pdev_atf_init)
		ac->atf_pdev_atf_init(pdev, pa);
	wlan_objmgr_pdev_component_obj_attach(pdev, WLAN_UMAC_COMP_ATF,
					      (void *)pa, QDF_STATUS_SUCCESS);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_atf_pdev_obj_destroy_handler(struct wlan_objmgr_pdev *pdev, void *arg)
{
	struct pdev_atf *pa = NULL;
	struct atf_context *ac = NULL;

	if (NULL == pdev) {
		atf_err("PDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}
	ac = atf_get_atf_ctx_from_pdev(pdev);
	if (NULL == ac) {
		atf_err("ATF context is NULL!\n");
		return QDF_STATUS_E_FAILURE;
	}
	pa = wlan_objmgr_pdev_get_comp_private_obj(pdev, WLAN_UMAC_COMP_ATF);
	if (NULL != pa) {
		wlan_objmgr_pdev_component_obj_detach(pdev, WLAN_UMAC_COMP_ATF,
						      (void *)pa);
		if (ac->atf_pdev_atf_deinit)
			ac->atf_pdev_atf_deinit(pdev, pa);
		qdf_mem_free(pa);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_atf_vdev_obj_create_handler(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct vdev_atf *va = NULL;

	if (NULL == vdev) {
		atf_err("VDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}
	if (QDF_SAP_MODE == wlan_vdev_mlme_get_opmode(vdev)) {
		va = (struct vdev_atf *)qdf_mem_malloc(sizeof(struct vdev_atf));
		if (NULL == va) {
			atf_err("Failed to allocate vdev_atf object\n");
			return QDF_STATUS_E_NOMEM;
		}
		qdf_mem_zero(va, sizeof(struct vdev_atf));
		va->vdev_obj = vdev;
		wlan_objmgr_vdev_component_obj_attach(vdev, WLAN_UMAC_COMP_ATF,
						      (void *)va,
						      QDF_STATUS_SUCCESS);
	} else {
		return QDF_STATUS_COMP_DISABLED;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_atf_vdev_obj_destroy_handler(struct wlan_objmgr_vdev *vdev, void *arg)
{
	struct vdev_atf *va = NULL;

	if (NULL == vdev) {
		atf_err("VDEV is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}
	va = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_ATF);
	if (NULL != va) {
		wlan_objmgr_vdev_component_obj_detach(vdev, WLAN_UMAC_COMP_ATF,
						      (void *)va);
		qdf_mem_free(va);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_atf_peer_obj_create_handler(struct wlan_objmgr_peer *peer, void *arg)
{
	struct peer_atf *pa = NULL;

	if (NULL == peer) {
		atf_err("PEER is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}

	if (WLAN_PEER_STA == wlan_peer_get_peer_type(peer)) {
		pa = (struct peer_atf *)qdf_mem_malloc(sizeof(struct peer_atf));
		if (NULL == pa) {
			atf_err("Failed to allocate peer_atf object\n");
			return QDF_STATUS_E_FAILURE;
		}
		qdf_mem_zero(pa, sizeof(struct peer_atf));
		atf_peer_init(peer, pa);
		wlan_objmgr_peer_component_obj_attach(peer, WLAN_UMAC_COMP_ATF,
						      (void *)pa,
						      QDF_STATUS_SUCCESS);
	} else {
		return QDF_STATUS_COMP_DISABLED;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_atf_peer_obj_destroy_handler(struct wlan_objmgr_peer *peer, void *arg)
{
	struct peer_atf *pa = NULL;

	if (NULL == peer) {
		atf_err("PEER is NULL\n");
		return QDF_STATUS_E_FAILURE;
	}
	pa = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_ATF);
	if (pa != NULL) {
		wlan_objmgr_peer_component_obj_detach(peer, WLAN_UMAC_COMP_ATF,
						      (void *)pa);
		if (pa->atf_debug) {
			qdf_mem_free(pa->atf_debug);
			pa->atf_debug = NULL;
		}
		qdf_mem_free(pa);
	}

	return QDF_STATUS_SUCCESS;
}
