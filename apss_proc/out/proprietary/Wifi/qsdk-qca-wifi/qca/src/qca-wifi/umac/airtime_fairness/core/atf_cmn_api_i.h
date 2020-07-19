/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _ATF_CMN_API_I_H_
#define _ATF_CMN_API_I_H_

#include "atf_defs_i.h"
#include "qdf_str.h"

/**
 * atf_cfg_timer_handler() - ATF config timer expiry handler
 * @arg : Reference to passed argument during initialization
 *
 * This is ATF config timer expiry handler which takes global pdev
 * reference as argument.
 *
 * Return : none
 */
void atf_cfg_timer_handler(void *arg);

/**
 * atf_start_cfg_timer() : Internal API to start config timer
 * @vdev : Reference to vdev global object
 * @pa   : Reference to pdev_atf object
 * @ac   : Reference to atf_context object
 *
 * This API used to set config timer for 5secs. On timer expiry timer handler
 * recalculate and rebuild the atf table.
 *
 * Return : none
 */
void atf_start_cfg_timer(struct wlan_objmgr_vdev *vdev, struct pdev_atf *pa,
			 struct atf_context *ac);

/**
 * wlan_atf_psoc_obj_create_handler(): handler for pdev object create
 * @psoc: reference to global psoc object
 * @arg:  reference to argument provided during registration of handler
 *
 * This is a handler to indicate psoc object created. Hence atf_context
 * object can be created and attched to psoc component list.
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         QDF_STATUS_E_FAILURE if psoc is null
 *         QDF_STATUS_E_NOMEM on failure of atf object allocation
 */
QDF_STATUS wlan_atf_psoc_obj_create_handler(struct wlan_objmgr_psoc *psoc,
					    void *arg);

/**
 * wlan_atf_psoc_obj_destroy_handler(): handler for psoc object delete
 * @psoc: reference to global psoc object
 * @arg:  reference to argument provided during registration of handler
 *
 * This is a handler to indicate psoc object going to be deleted.
 * Hence atf_context object can be detached from psoc component list.
 * Then atf_context object can be deleted.
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS wlan_atf_psoc_obj_destroy_handler(struct wlan_objmgr_psoc *psoc,
					     void *arg);

/**
 * wlan_atf_pdev_obj_create_handler(): handler for pdev object create
 * @pdev: reference to global pdev object
 * @arg:  reference to argument provided during registration of handler
 *
 * This is a handler to indicate pdev object created. Hence pdev specific
 * atf object can be created and attched to pdev component list.
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         QDF_STATUS_E_FAILURE if pdev is null
 *         QDF_STATUS_E_NOMEM on failure of atf object allocation
 */
QDF_STATUS wlan_atf_pdev_obj_create_handler(struct wlan_objmgr_pdev *pdev,
					    void *arg);

/**
 * wlan_atf_pdev_obj_destroy_handler(): handler for pdev object delete
 * @pdev: reference to global pdev object
 * @arg:  reference to argument provided during registration of handler
 *
 * This is a handler to indicate pdev object going to be deleted.
 * Hence pdev specific atf object can be detached from pdev component list.
 * Then pdev_atf object can be deleted.
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS wlan_atf_pdev_obj_destroy_handler(struct wlan_objmgr_pdev *pdev,
					     void *arg);

/**
 * wlan_atf_vdev_obj_create_handler(): handler for vdev object create
 * @vdev: reference to global vdev object
 * @arg:  reference to argument provided during registration of handler
 *
 * This is a handler to indicate vdev object created. Hence vdev specific
 * atf object can be created and attched to vdev component list.
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         QDF_STATUS_E_FAILURE if vdev is null
 *         QDF_STATUS_E_NOMEM on failure of atf object allocation
 */
QDF_STATUS wlan_atf_vdev_obj_create_handler(struct wlan_objmgr_vdev *vdev,
					    void *arg);

/**
 * wlan_atf_vdev_obj_destroy_handler(): handler for vdev object delete
 * @vdev: reference to global vdev object
 * @arg:  reference to argument provided during registration of handler
 *
 * This is a handler to indicate vdev object going to be deleted.
 * Hence vdev specific atf object can be detached from vdev component list.
 * Then vdev_atf object can be deleted.
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS wlan_atf_vdev_obj_destroy_handler(struct wlan_objmgr_vdev *vdev,
					     void *arg);

/**
 * wlan_atf_peer_obj_create_handler(): handler for peer object create
 * @peer: reference to global peer object
 * @arg:  reference to argument provided during registration of handler
 *
 * This is a handler to indicate peer object created. Hence peer specific
 * atf object can be created and attched to peer component list.
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         QDF_STATUS_E_FAILURE if peer is null
 *         QDF_STATUS_E_NOMEM on failure of atf object allocation
 */
QDF_STATUS wlan_atf_peer_obj_create_handler(struct wlan_objmgr_peer *peer,
					    void *arg);

/**
 * wlan_atf_peer_obj_destroy_handler(): handler for peer object delete
 * @peer : reference to global peer object
 * @arg  : reference to argument provided during registration of handler
 *
 * This is a handler to indicate peer object going to be deleted.
 * Hence peer specific atf object can be detached from peer component list.
 * Then peer_atf object can be deleted.
 *
 * Return : QDF_STATUS_SUCCESS on success
 *          QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS wlan_atf_peer_obj_destroy_handler(struct wlan_objmgr_peer *peer,
					     void *arg);

/**
 * atf_update_tput_tbl() - Function to update throughput table
 * @pdev : Reference to pdev global object
 * @peer : Reference to peer global object
 * @type : 1 for join 0 for leave
 *
 * This function is used to update throughput table during peer join/leave.
 *
 * Return : void
 */
void atf_update_tput_tbl(struct wlan_objmgr_pdev *pdev,
			 struct wlan_objmgr_peer *peer, uint8_t type);

/**
 * atf_update_number_of_clients() - Function to update number of clients
 * @pdev  : Reference to pdev global object
 * @value : Number of clients
 *
 * This function is used to update maximum number of clients per radio.
 *
 * Return : 0 if success
 *          -1 if failed
 */
int32_t
atf_update_number_of_clients(struct wlan_objmgr_pdev *pdev, int32_t value);

/**
 * atf_reset_airtime_sg_all() - Reset airtime for all subgroups
 * @pa : Reference to pdev_atf object
 *
 * Return : void
 */
void atf_reset_airtime_sg_all(struct pdev_atf *pa);

/**
 * atf_assign_airtime_subgroup() - Assign airtime for all subgroups
 * @pa : Reference to pdev_atf object
 *
 * Return : void
 */
void atf_assign_airtime_subgroup(struct pdev_atf *pa);

/**
 * calculate_airtime_group_peer_ac() - Total Airtime assigned to
 * 				       AC & Peer subgroup
 * @pa    : Reference to pdev_atf object
 *
 * Return : void
 */
void calculate_airtime_group_peer_ac(struct pdev_atf *pa);

/**
 * atf_assign_airtime_sg_ssid() - Assign airtime to SSID subgroup
 * @pa    : Reference to pdev_atf object
 *
 * Return : QDF_STATUS_SUCCESS on success
 *          QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS atf_assign_airtime_sg_ssid(struct pdev_atf *pa);

/**
 * atf_config_ssid_group() - Add ssid to group created
 * @pa    : Reference to pdev_atf object
 * @ssid  : SSID name
 * @index : group index
 * @group : Reference to atf_group_list object
 *
 * Return : QDF_STATUS_SUCCESS on success
 *          QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS atf_config_ssid_group(struct pdev_atf *pa, uint8_t *ssid,
				 uint32_t index, struct atf_group_list *group);

/**
 * atf_config_group_val_sched() - Set group airtime and default sched policy
 * @pa : Reference to pdev_atf object
 * @value: Airtime value
 * @groupindex : group index
 *
 * Return : QDF_STATUS_SUCCESS on success
 *
 */
QDF_STATUS atf_config_group_val_sched(struct pdev_atf *pa,
				      uint32_t value, uint32_t groupindex);

/**
 * atf_config_mark_del_peer_subgroup() - Mark subgroup list entries for delete.
 * @pa : Reference to pdev_Atf object
 * @mac: Reference to peer mac address
 *
 * Return : QDF_STATUS_SUCCESS on success
 */
QDF_STATUS atf_config_mark_del_peer_subgroup(struct pdev_atf *pa, void *mac);

/**
 * atf_mark_del_all_sg() - Mark all subgroups for deletion
 * @group : Reference to atf_group_list_object
 *
 * Return : QDF_STATUS_SUCCESS on success
 *          QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS atf_mark_del_all_sg(struct atf_group_list *group);

/**
 * atf_group_vap_sched() - Set Scheduling policy per SSID
 * @vdev : Reference to atf_group_list_object
 * @sched_policy : Scheduling policy
 *                  0 - Fair
 *                  1 - Strict
 *                  2 - Fair with Upper-Bound
 *
 * Return : QDF_STATUS_SUCCESS on success
 *          QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS atf_group_vap_sched(struct wlan_objmgr_vdev *vdev,
			       u_int32_t sched_policy);

/** atf_assign_group() - Assign group
 * @pa       : Reference to pdev atf object
 * @peer     : Reference to peer atf object
 * @group_index  : Group index
 *
 * Return    : None
 */
void atf_set_assign_group(struct pdev_atf *pa, struct wlan_objmgr_peer *peer,
			  uint32_t group_index);

/** atf_assign_subgroup() - Assign subgroup
 * @pa       : Reference to pdev atf object
 * @peer     : Reference to peer atf object
 * @group    : Reference to atf group object
 * @acbitmap : ac bitmap to which subgroup is assigned
 * @cfg_type  : Type of subgroup
 *
 * Return : QDF_STATUS_SUCCESS on success
 *          QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS
atf_assign_subgroup(struct pdev_atf *pa, struct peer_atf *peer,
		    struct atf_group_list *group, uint8_t acbitmap,
		    uint8_t cfg_type);

/**
 * atf_config_is_default_subgroup() - Check if it's default subgroup
 * @sgname  :   Subgroup name
 *
 * Return   :   TRUE if subgroup name matches with default subgroup
 *              FALSE if subgroup name doesn't match with default subgroup
 */
int8_t atf_config_is_default_subgroup(uint8_t *sgname);

/**
 * atf_config_is_default_group() - Check if it's default group
 * @grpname :    Group name
 *
 * Return   :   TRUE if group name matches with default group
 *              FALSE if group name doesn't match with default group
 */
int8_t atf_config_is_default_group(uint8_t *grpname);

/**
 * atf_config_is_group_exist() - Check if this group already exists
 * @cfg    : Reference to atf_config object
 * @gname  : Group name
 * @gindex : Group index
 *
 * Return : TRUE on Success
 *          FALSE on failure
 */
int8_t atf_config_is_group_exist(struct atf_config *cfg,
				  uint8_t *gname, uint8_t *gindex);

/**
 * atf_validate_acval() - Validate ATF AC configuration val
 * @atf_ac : Reference to atf_ac_config object
 * @ac_id  : AC id
 * @ac_val : user configured airtime for AC
 *
 * Return : QDF_STATUS_SUCCESS on success
 *          QDF_STATUS_E_INVAL on error
 */
QDF_STATUS atf_validate_acval(struct atf_ac_config *atf_ac, int8_t ac_id,
			      uint32_t ac_val);

/**
 * atf_config_is_subgroup_exist() - Check if subgroup exists
 * @cfg     :   Reference to atf_config object
 * @group   :   Reference to atf_group_list object
 * @param   :   Primary identifier of subgroup type characateristic
 * @subgroup:   Reference to atf_subgroup_list object
 * @cfg_type:   Subgroup type
 *
 * Return : TRUE if subgroup exist
 *          FALSE if subggroup doesn't exist
 */
int8_t atf_config_is_subgroup_exist(struct atf_config *cfg,
				      struct atf_group_list *group, void *param,
				      struct atf_subgroup_list **subgroup,
				      int8_t cfg_type);

/**
 * atf_dump_subgroup_allocation () - Print subgroup allocation
 * @pa   :   Reference to pdev atf object
 *
 * Return : None
 */
void atf_dump_subgroup_allocation(struct pdev_atf *pa);

/**
 * atf_config_ssid_in group() - Check if ssid is part of group
 * @pa   : Reference to pdev_atf object
 * @ssid : SSID name
 *
 * Return : TRUE on Success
 *          FALSE on failure
 */
bool atf_config_ssid_in_group(struct pdev_atf *pa, uint8_t *ssid);

/**
 * atf_config_init_ac_val() - initialize ac based parameters
 * @pa    : Reference to pdev_atf object
 * @index : Group/VAP index
 *
 * Return : QDF_STATUS_SUCCESS on success
 */
QDF_STATUS atf_config_init_ac_val(struct pdev_atf *pa, uint8_t index);

/**
 * atf_config_del_ssid_group() - Delete group when ssid is unconfigured
 * @pa : Reference to pdev_atf object
 * @ssid : SSID name
 * @index : Group index when SSID was added
 *
 * Return : QDF_STATUS_SUCCESS on success
 *          QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS atf_config_del_ssid_group(struct pdev_atf *pa,
				     uint8_t *ssid, uint32_t groupindex);

/**
 * atf_add_pdev_group() - Add group to radio structure
 * @pa :	Reference to pdev_atf object
 * @gname :	group name
 *
 * Return : QDF_STATUS_SUCCESS on success
 */
QDF_STATUS atf_config_add_pdev_group(struct pdev_atf *pa, uint8_t *gname);

/**
 * atf_modify_sta_cfg_index() - Modify airtime allotted to peer on group delete
 * @pa : Reference to pdev_atf object
 * @gindex : Group index
 *
 * Return : QDF_STATUS_SUCCESS on success
 */
QDF_STATUS atf_modify_sta_cfg_index(struct pdev_atf *pa, uint8_t gindex);

/**
 * atf_del_pdev_group() - Delete group entry from radio structure
 * @pa : Reference to pdev_atf object
 * @gindex : Group index
 *
 * Return : QDF_STATUS_SUCCESS on success
 *          QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS atf_config_del_pdev_group(struct pdev_atf *pa, uint8_t gindex);

/**
 * atf_group_delete() - Delete group entry in the List
 * @pa : Reference to pdev_atf object
 * @gname : Group name
 *
 * Return : None
 */
void atf_group_delete(struct pdev_atf *pa, uint8_t *gname);

/**
 * atf_add_group_airtime() - Assign group airtime to the list entry
 * @group : Reference to the group entry
 * @value : Airtime to be configured
 *
 * Return : None
 */
void atf_add_group_airtime(struct atf_group_list *group, uint32_t value);

/**
 * atf_add_new_group() - Configure new group
 * @pa : Reference to pdev_atf object
 * @gname : Group name
 *
 * Return : Reference to group configured
 */
struct atf_group_list *atf_add_new_group(struct pdev_atf *pa, uint8_t *gname);

/**
 * atf_set_ssid_sched_policy() - Configure SSID scheduling policy
 * @pa : Reference to vdev object
 * @value : scheduling policy
 *
 * Return : QDF_STATUS_SUCCESS on success
 *          QDF_STATUS_E_INVAL on error
 */
QDF_STATUS
atf_set_ssid_sched_policy(struct wlan_objmgr_vdev *vdev, uint16_t value);

/**
 * atf_remove_group() - Delete all groups in radio structure
 * @pa : reference to pdev_atf object
 *
 * Return : void
 */
void atf_remove_group(struct pdev_atf *pa);

/**
 * atf_del_group_subgroup_() - Delete all groups & subgroups marked for deletion
 * @pa : reference to pdev_atf object
 *
 * Return : void
 */
void atf_del_group_subgroup(struct pdev_atf *pa);

/**
 * atf_add_new_subgroup() - Add new subgroup entry
 * @pa : Reference to pdev_atf object
 * @group : Reference to atf_group_list object
 * @param : MAC & AC ID for Peer & AC subgroup respectively
 *          NULL for other subgroup types
 * @val : Airtime value
 * @sg_type : Type of subgroup
 *
 * Return : Reference to the subgroup created
 */
struct atf_subgroup_list *atf_add_new_subgroup(struct pdev_atf *pa,
					       struct atf_group_list *group,
					       void *param, uint32_t val,
					       uint32_t sg_type);

/**
 * atf_remove_subgroup() - Remove subgroup entry
 * @group : Reference to atf_group_list object
 *
 * Return : None
 */
void atf_remove_subgroup(struct atf_group_list *group);

/**
 * atf_remove_pdev_subgroup() - Remove subgroups in radio structure
 * @pa      :    Reference to pdev_atf object
 *
 * Return   :    Void
 */
void atf_remove_pdev_subgroup(struct pdev_atf *pa);

/**
 * atf_remove_subgroup_all() - Remove All Subgroups
 * @group : Reference to atf_group_list object
 *
 * Return : void
 */
void atf_remove_subgroup_all(struct atf_group_list *group);

/**
 * atf_verify_sta_cumulative() - Cumulative val of peer config
 * @cfg : Reference to atf_config object
 * @index : VAP/ Group index
 * @val : User configured airtime
 *
 * Return : QDF_STATUS_SUCCESS on Success
 *          QDF_STATUS_E_INVAL on Error
 */
QDF_STATUS atf_verify_sta_cumulative(struct atf_config *cfg,
				     uint32_t index, struct sta_val *val);

/**
 * atf_new_sta_cfg() - Add ATF STA configuration in pdev structure
 * @pa : Reference to pdev_atf object
 * @val : user configuration
 * @peer_index : Peer index in radio structure
 *
 * Return : QDF_STATUS_SUCCESS on Success
 *          QDF_STATUS_E_INVAL on Error
 */
QDF_STATUS atf_new_sta_cfg(struct pdev_atf *pa, struct sta_val *val,
			   uint32_t peer_index);

/**
 * atf_modify_sta_cfg() - Modify existing STA based ATF config
 * @pa     : Reference to pdev_atf object
 * @val    : User configuration
 * @peer_index: Peer index in ic structure
 *
 * Return  : QDF_STATUS_SUCCESS on success
 *           QDF_STATUS_E_FAILURE on error
 */
QDF_STATUS atf_modify_sta_cfg(struct pdev_atf *pa, struct sta_val *val,
			      uint32_t peer_index);

/**
 * atf_dump_pdev_sta () - Debug API to dump ATF STA configuration
 * @pa : Reference to pdev_atf object
 *
 * Return : None
 */
void atf_dump_pdev_sta(struct pdev_atf *pa);

/**
 * atf_find_vdev_index() - Find vdev index to which peer belongs
 * @pa    :   Reference to pdev_atf object
 * @vdev  :   Reference to vdev global object
 *
 * Return :   0 on success
 *            1 on failure
 */
uint8_t atf_find_vdev_index(struct pdev_atf *pa, struct wlan_objmgr_vdev *vdev);

/**
 * atf_find_vdev_ssid_index() - Find vdev index to which peer_ac belongs
 * @pa    :   Reference to pdev_atf object
 * @ssid  :   SSID name
 *
 * Return :   0 on success
 *            1 on failure
 */
uint8_t atf_find_vdev_ssid_index(struct pdev_atf *pa, uint8_t *ssid);

/**
 * atf_valid_peer() - Function to check valid peer
 * @peer : Reference to peer global object
 *
 * This function is used to validate the peer is a non-AP sta and
 * associated to AP sta with valid MAC address.
 *
 * Return : peer object if valid
 *          NULL if invalid
 */
struct wlan_objmgr_peer *atf_valid_peer(struct wlan_objmgr_peer *peer);

/**
 * atf_vdev_init() - Function to iterate and do init of all vdev
 * @pdev : Reference to pdev global object
 * @obj  : Reference to vdev global object
 * @args : Null
 *
 * This Function is exposed for internal use to traverse all vdevs of pdev
 * and perform osif_vap_init.
 *
 * Return : None
 */
void atf_vdev_init(struct wlan_objmgr_pdev *pdev, void *obj, void *args);

/**
 * atf_get_pdev_atf_from_vdev() - API to get pdev_atf object from vdev
 * @vdev : Reference to vdev global object
 *
 * This API used to get pdev specific atf object from global vdev reference.
 * Null check should be done before invoking this inline function.
 *
 * Return : Reference to pdev_atf object
 */
static inline
struct pdev_atf *atf_get_pdev_atf_from_vdev(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_pdev *pdev = NULL;
	struct pdev_atf *pa = NULL;

	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev) {
		pa = wlan_objmgr_pdev_get_comp_private_obj(pdev,
							   WLAN_UMAC_COMP_ATF);
	}

	return pa;
}

/**
 * atf_get_atf_ctx_from_pdev() - API to get atf context object from pdev
 * @pdev : Reference to pdev global object
 *
 * This API used to get atf context object from global pdev reference.
 * Null check should be done before invoking this inline function.
 *
 * Return : Reference to atf_context object
 */
static inline
struct atf_context *atf_get_atf_ctx_from_pdev(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct atf_context *ac = NULL;

	psoc = wlan_pdev_get_psoc(pdev);
	if (psoc) {
		ac = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							   WLAN_UMAC_COMP_ATF);
	}

	return ac;
}

/**
 * atf_get_atf_ctx_from_vdev() - API to get atf context object from vdev
 * @vdev : Reference to vdev global object
 *
 * This API used to get atf context object from global vdev reference.
 * Null check should be done before invoking this inline function.
 *
 * Return : Reference to atf_context object
 */
static inline
struct atf_context *atf_get_atf_ctx_from_vdev(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_psoc *psoc = NULL;
	struct atf_context *ac = NULL;

	psoc = wlan_vdev_get_psoc(vdev);
	if (psoc) {
		ac = wlan_objmgr_psoc_get_comp_private_obj(psoc,
							   WLAN_UMAC_COMP_ATF);
	}

	return ac;
}

/**
 * atf_get_atf_ctx_from_vdev() - API to get atf context object from vdev
 * @vdev : Reference to vdev global object
 *
 * This API used to get atf context object from global vdev reference.
 * Null check should be done before invoking this inline function.
 *
 * Return : Reference to atf_context object
 */
static inline
void atf_peer_resume(struct wlan_objmgr_psoc *psoc,
		     struct wlan_objmgr_pdev *pdev,
		     struct wlan_objmgr_peer *peer)
{
	if (psoc->soc_cb.tx_ops.atf_tx_ops.atf_peer_resume) {
		psoc->soc_cb.tx_ops.atf_tx_ops.atf_peer_resume(pdev, peer);
	}

}

#endif /* _ATF_CMN_API_I_H_*/
