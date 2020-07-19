/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _WLAN_ATF_UCFG_API_H_
#define _WLAN_ATF_UCFG_API_H_

#include "../../core/atf_defs_i.h"

/* ATF specific UCFG set operations */

/**
 * ucfg_atf_set() - API to set airtime fairness enable
 * @vdev   : Reference to global vdev object
 *
 * This API is used to enable ATF feature. This sets 5 secs timer to for
 * ATF table update and for direct attach 200ms timer for token allocation.
 * Internally sends wmi command to enable ATF in firmware for offload radio.
 *
 * Return : 0 in case of offload radio,
 *          Number of clients allowed to associate in Direct Attach radio
 */
int32_t ucfg_atf_set(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_atf_clear() - API to set airtime fairness disable
 * @vdev   : Reference to global vdev object
 *
 * This API is used to disable ATF feature.
 * Internally sends wmi command to disable ATF in firmware for offload radio.
 *
 * Return : 0 in case of offload radio,
 *          Number of clients allowed to associate in Direct Attach radio
 */
int32_t ucfg_atf_clear(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_atf_set_txbuf_share() - API to enable/disable txbuf share
 * @vdev  : Reference to global vdev object
 * @value : 0 to disable, 1 to enable and 2 for debug purpose
 *
 * This API is used to enable/disable txbuf disribution among different clirnts.
 * It enables UDP support with ATF clients. Change in previous state requires
 * reset.
 *
 * Return : 0 on success
 *          -1 on failure
 */
int32_t ucfg_atf_set_txbuf_share(struct wlan_objmgr_vdev *vdev, uint8_t value);

/**
 * ucfg_atf_set_max_txbufs() - API to set maximum number of tx buffers for atf
 * @vdev  : Reference to global vdev object
 * @value : Maximum number of Tx bufs
 *
 * This API is used to set maximum number of Tx buffer for ATF.
 *
 * Return : None
 */
void ucfg_atf_set_max_txbufs(struct wlan_objmgr_vdev *vdev, uint16_t value);

/**
 * ucfg_atf_set_min_txbufs() - API to set minimum number of tx buffers for atf
 * @vdev  : Reference to global vdev object
 * @value : Minimum number of Tx bufs
 *
 * This API is used to set minimum number of Tx buffer for ATF.
 *
 * Return : None
 */
void ucfg_atf_set_min_txbufs(struct wlan_objmgr_vdev *vdev, uint16_t value);

/**
 * ucfg_atf_set_airtime_tput() - API to set atf throughput
 * @vdev  : Reference to global vdev object
 * @value : Value to set atf throughput
 *
 * This API is used to override ATF throughput.
 *
 * Return : None
 */
void ucfg_atf_set_airtime_tput(struct wlan_objmgr_vdev *vdev, uint16_t value);

/**
 * ucfg_atf_set_maxclient() - API to enable/disable atf maxclient feature
 * @pdev   : Reference to global pdev object
 * @enable : Value to enable/disable maxclient feature
 *
 * This API is used to enable/disable maxclient feature of atf.
 * This feature is supported only in direct attach radio.
 *
 * Return : 0 in case of success,
 *          -1 incase of failure
 */
int32_t ucfg_atf_set_maxclient(struct wlan_objmgr_pdev *pdev, uint8_t enable);

/**
 * ucfg_atf_set_ssidgroup() - API to enable/disable ssid group for a radio
 * @pdev   : Reference to global pdev object
 * @vdev   : Reference to global vdev object
 * @enable : Value to enable/disable ssid grouping
 *
 * This API is used to enable/disable ssid grouping feature for atf.
 *
 * Return : None
 */
void ucfg_atf_set_ssidgroup(struct wlan_objmgr_pdev *pdev,
                            struct wlan_objmgr_vdev *vdev, uint8_t enable);

/**
 * ucfg_atf_set_ssid_sched_policy() - API to set a ssid scheduling policy
 * @vdev  : Reference to global vdev object
 * @value : 0 for fairq and 1 for strict scheduling
 *
 * This API is used to set an atf scheduling for a particular ssid/vdev.
 * vdev should be running as host AP mode.
 *
 * Return : 0 on success
 *          -EINVAL on invalid input/vdev running in invalid mode
 */
int32_t ucfg_atf_set_ssid_sched_policy(struct wlan_objmgr_vdev *vdev,
				       uint16_t value);

/**
 * ucfg_atf_set_per_unit() - API to configuring ATF units to default
 * @pdev : Reference to global pdev object
 *
 * This API is used to set ATF units to default 1000.
 *
 * Return : None
 */
void ucfg_atf_set_per_unit(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_atf_set_ssid(): API to configure particular SSID
 * @vdev:  reference to global vdev object
 * @value: reference to ssid_val object
 *
 * This API is used to configure user requested SSID.
 * Note that the configuration will be applied even for non-existing SSID.
 *
 * Return: 0 on success
 *         -EFAULT on invalid configuration
 */
int32_t ucfg_atf_set_ssid(struct wlan_objmgr_vdev *vdev,
			  struct ssid_val *value);

/**
 * ucfg_atf_delete_ssid(): API to remove configuration of particular SSID
 * @vdev:  reference to global vdev object
 * @value: reference to ssid_val object
 *
 * This API is used to remove configuration of user requested SSID.
 *
 * Return: 0
 */
int32_t ucfg_atf_delete_ssid(struct wlan_objmgr_vdev *vdev,
			     struct ssid_val *value);

/**
 * ucfg_atf_set_sta(): API to configure particular STA
 * @vdev:  reference to global vdev object
 * @value: reference to sta_val object
 *
 * This API is used to configure user requested STA.
 *
 * Return: 0
 */
int32_t ucfg_atf_set_sta(struct wlan_objmgr_vdev *vdev,
			 struct sta_val *value);

/**
 * ucfg_atf_delete_sta(): API to remove configuration of particular sta
 * @vdev:  reference to global vdev object
 * @value: reference to sta_val object
 *
 * This API is used to remove configuration of user requested STA.
 *
 * Return: 0
 */
int32_t ucfg_atf_delete_sta(struct wlan_objmgr_vdev *vdev,
			    struct sta_val *value);

/**
 * ucfg_atf_add_ac(): API to configure ac based airtime
 * @vdev:    Reference to global vdev object
 * @buf:     User configuration
 *
 * This API is used to add ac based airtime configuration
 *
 * Return: 0
 */
int32_t ucfg_atf_add_ac(struct wlan_objmgr_vdev *vdev,
			struct atf_ac *buf);
/**
 * ucfg_atf_del_ac(): API to remove particular ac based configuration
 * @vdev:   Reference to global vdev object
 * @buf: User input
 *
 * This API is used to remove configuration of user requested AC.
 *
 * Return: 0
 */
int32_t ucfg_atf_del_ac(struct wlan_objmgr_vdev *vdev,
			struct atf_ac *buf);

/* ATF specific UCFG get operations */

/**
 * ucfg_atf_get(): API to get airtime fairness enable/disable
 * @vdev: reference to global vdev object
 *
 * This API is used to get ATF feature enable/disable.
 *
 * Return: 0 if disabled,
 *         1 if enabled
 */
uint8_t ucfg_atf_get(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_atf_get_txbuf_share() - API to get configured txbuf share
 * @vdev : Reference to global vdev object
 *
 * This API is used to get configured txbuf share value.
 *
 * Return : Configued value for txbuf share
 *          0 to disable, 1 to enable and 2 for debug purpose
 */
uint8_t ucfg_atf_get_txbuf_share(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_atf_get_max_txbufs() - API to get maximum number of tx buffers for atf
 * @vdev : Reference to global vdev object
 *
 * This API is used to get maximum number of Tx buffers configured for ATF.
 *
 * Return : Number of maximum txbufs configured
 */
uint16_t ucfg_atf_get_max_txbufs(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_atf_get_min_txbufs() - API to get minimum number of tx buffers for atf
 * @vdev : Reference to global vdev object
 *
 * This API is used to get minimum number of Tx buffers configured for ATF.
 *
 * Return : Number of minimum txbufs configured
 */
uint16_t ucfg_atf_get_min_txbufs(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_atf_get_airtime_tput() - API to get atf throughput
 * @vdev : Reference to global vdev object
 *
 * This API is used to get configured ATF throughput.
 *
 * Return : Configured throughput
 */
uint16_t ucfg_atf_get_airtime_tput(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_atf_get_maxclient() - API to get state of maxclient feature
 * @pdev : reference to global pdev object
 *
 * This API is used to get state of maxclient feature.
 * This feature is supported only in direct attach radio.
 *
 * Return : 0 if disabled,
 *          1 if enabled
 */
uint8_t ucfg_atf_get_maxclient(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_atf_get_ssidgroup() - API to get ssid grouping state for a radio
 * @pdev : reference to global pdev object
 *
 * This API is used to get state of ssid grouping feature for a radio.
 *
 * Return : 0 if disabled
 *          1 if enabled
 */
uint8_t ucfg_atf_get_ssidgroup(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_atf_get_ssid_sched_policy() - API to get scheduled atf policy
 * @vdev : reference to global vdev object
 *
 * This API is used to get scheduled atf policy for a particular ssid/vdev.
 * vdev should be running as host AP mode.
 *
 * Return : 0 for fairq
 *          1 for strict
 */
uint8_t ucfg_atf_get_ssid_sched_policy(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_atf_get_per_unit() - API to get configured ATF units
 * @pdev : reference to global pdev object
 *
 * This API is used to get ATF units, default 1000.
 *
 * Return : Value per units
 */
uint16_t ucfg_atf_get_per_unit(struct wlan_objmgr_pdev *pdev);

/**
 * ucfg_atf_get_peer_stats() - API to get peer stats
 * @vdev   : Reference to global vdev object
 * @mac    : Peer mac
 * @astats : Regerence to atf stats (out)
 *
 * This API is used to get peer stats.
 *
 * Return : None
 */
void ucfg_atf_get_peer_stats(struct wlan_objmgr_vdev *vdev, uint8_t *mac,
			     struct atf_stats *astats);

/**
 * ucfg_atf_get_mode() - API to get atf mode
 * @psoc : Reference to global psoc object
 *
 * This API is used to get atf mode.
 *
 * Return : Configured atf mode
 */
uint32_t ucfg_atf_get_mode(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_atf_get_peer_airtime() - API to get airtime for the peer
 * @peer : Reference to global peer object
 *
 * This API is used to get airtime of the peer.
 *
 * Return : Configured airtime of peer
 */
uint32_t ucfg_atf_get_peer_airtime(struct wlan_objmgr_peer *peer);

/**
 * ucfg_atf_peer_join_leave() - API to indicate peer join and leave
 * @peer : Reference to global peer object
 * @type : 1 for join 0 for leave
 *
 * This API is used to indicate one peer join or leave.
 *
 * Return : None
 */
void ucfg_atf_peer_join_leave(struct wlan_objmgr_peer *peer, uint8_t type);

/**
 * ucfg_atf_show_table() - API to show atf table
 * @vdev : Reference to global vdev object
 * @tbl  : Reference to atf table
 *
 * This API is used to show atf table entries.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t
ucfg_atf_show_table(struct wlan_objmgr_vdev *vdev, struct atftable *tbl);

/**
 * ucfg_atf_show_airtime() - API to show airtime
 * @vdev : Reference to global vdev object
 * @tbl  : Reference to atf table
 *
 * This API is used to show alloted airtime to each table entry.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t
ucfg_atf_show_airtime(struct wlan_objmgr_vdev *vdev, struct atftable *tbl);

/**
 * ucfg_atf_flush_table() - API to flush atf table
 * @vdev : Reference to global vdev object
 *
 * This API is used to flush atf table entries.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t ucfg_atf_flush_table(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_atf_add_group() - API to add atf group
 * @vdev : Reference to global vdev object
 * @buf  : Reference to atf group
 *
 * This API is used to add entry in atf group table.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t
ucfg_atf_add_group(struct wlan_objmgr_vdev *vdev, struct atf_group *buf);

/**
 * ucfg_atf_config_group() - API to configure atf group
 * @vdev : Reference to global vdev object
 * @buf  : Reference to atf group
 *
 * This API is used to configure requested atf group.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t
ucfg_atf_config_group(struct wlan_objmgr_vdev *vdev, struct atf_group *buf);

/**
 * ucgf_atf_group_sched() - API to set group scheduling policy
 * @vdev : Reference to global vdev object
 * @buf : Reference to atf_group
 *
 * This API is used to configure scheduling policy for user requested group
 *
 * Return : 0 on Success
 *        : -1 on failure
 */
int32_t
ucfg_atf_group_sched(struct wlan_objmgr_vdev *vdev, struct atf_group *buf);

/**
 * ucfg_atf_delete_group() - API to delete atf group
 * @vdev : Reference to global vdev object
 * @buf  : Reference to atf group
 *
 * This API is used to delete requested atf group.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t
ucfg_atf_delete_group(struct wlan_objmgr_vdev *vdev, struct atf_group *buf);

/**
 * ucfg_atf_show_group() - API to show atf group table
 * @vdev : Reference to global vdev object
 * @buf  : Reference to atf group table
 *
 * This API is used to show atf group table entries.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t
ucfg_atf_show_group(struct wlan_objmgr_vdev *vdev, struct atfgrouptable *buf);

/**
 * ucfg_atf_show_subgroup() - API to show atf subgroup table
 * @vdev : Reference to global vdev object
 * @buf  : Reference to atf group list table
 *
 * This API is used to show atf group table entries.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t
ucfg_atf_show_subgroup(struct wlan_objmgr_vdev *vdev,
		       struct atfgrouplist_table *buf);

/**
 * ucfg_atf_add_ac() - API to add AC config
 * @vdev : Reference to global vdev object
 * @buf  : Reference to atf_ac struct
 *
 * This API is used to show atf group table entries.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t
ucfg_atf_add_ac(struct wlan_objmgr_vdev *vdev, struct atf_ac *buf);

/**
 * ucfg_atf_del_ac() - API to del AC config
 * @vdev : Reference to global vdev object
 * @buf  : Reference to atf_ac object
 *
 * This API is used to show atf group table entries.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t
ucfg_atf_del_ac(struct wlan_objmgr_vdev *vdev, struct atf_ac *buf);

/**
 * ucfg_atf_add_sta_tput() - API to add sta throughput
 * @vdev : Reference to global vdev object
 * @buf  : Reference to sta val object
 *
 * This API is used to add sta throughput in sta throughput table entries.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t ucfg_atf_add_sta_tput(struct wlan_objmgr_vdev *vdev,
			      struct sta_val *buf);

/**
 * ucfg_atf_delete_sta_tput() - API to delete sta from throughput table
 * @vdev : Reference to global vdev object
 * @buf  : Reference to sta val object
 *
 * This API is used to delete sta from throughput table entries.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t ucfg_atf_delete_sta_tput(struct wlan_objmgr_vdev *vdev,
				 struct sta_val *buf);

/**
 * ucfg_atf_show_tput() - API to show throughput table
 * @vdev : Reference to global vdev object
 *
 * This API is used to show atf throughput table entries.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t ucfg_atf_show_tput(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_atf_set_debug_size() - API to set debug size
 * @vdev : Reference to global vdev object
 * @size : Requested debug size
 *
 * This API is used to set debug size.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t ucfg_atf_set_debug_size(struct wlan_objmgr_vdev *vdev, int32_t size);

/**
 * ucfg_atf_get_debug_peerstate() - API to get debug state of peer
 * @vdev      : Reference to global vdev object
 * @mac       : Reference to peer mac address
 * @peerstate : Reference to peer state (out)
 *
 * This API is used to get debug state of peer.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t ucfg_atf_get_debug_peerstate(struct wlan_objmgr_vdev *vdev,
				     uint8_t *mac,
				     struct atf_peerstate *peerstate);

/**
 * ucfg_atf_get_lists() - API to get group and subgroup lists
 * @vdev   : Reference to global vdev object
 * @buf    : Reference to atfgrouplist_table
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t ucfg_atf_get_lists(struct wlan_objmgr_vdev *vdev,
			   struct atfgrouplist_table *buf);
/**
 * ucfg_atf_get_debug_dump() - API to get debug dump messages
 * @vdev   : Reference to global vdev object
 * @stats  : Reference to atf stats struct
 * @buf    : Double pointer to buffer for holding debug dump (out)
 * @buf_sz : Reference to size of buffer (out)
 * @id     : Reference to ID
 *
 * This API is used to get debug dump messages.
 *
 * Return : 0 on Success
 *          -1 on Failure
 */
int32_t ucfg_atf_get_debug_dump(struct wlan_objmgr_vdev *vdev,
				struct atf_stats *stats, void **buf,
				uint32_t *buf_sz, uint32_t *id);
#endif /* _WLAN_ATF_UCFG_API_H_*/
