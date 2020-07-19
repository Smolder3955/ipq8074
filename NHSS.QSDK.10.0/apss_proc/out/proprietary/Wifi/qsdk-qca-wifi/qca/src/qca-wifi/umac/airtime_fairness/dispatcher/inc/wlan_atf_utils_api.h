/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _WLAN_ATF_UTILS_API_H_
#define _WLAN_ATF_UTILS_API_H_

#include <wlan_objmgr_cmn.h>

/**
 * wlan_atf_init(): API to init airtime fairness component
 *
 * This API is invoked from dispatcher init during all component init.
 * This API will register all required handlers for pdev and peer object
 * create/delete notification.
 *
 * Return: SUCCESS,
 *         Failure
 */
QDF_STATUS wlan_atf_init(void);

/**
 * wlan_atf_deinit(): API to deinit airtime fairness component
 *
 * This API is invoked from dispatcher deinit during all component deinit.
 * This API will unregister all registerd handlers for pdev and peer object
 * create/delete notification.
 *
 * Return: SUCCESS,
 *         Failure
 */
QDF_STATUS wlan_atf_deinit(void);

/**
 * wlan_atf_open(): API to open airtime fairness component
 *
 * This API is invoked from dispatcher psoc open.
 * This API will initialize psoc level atf object.
 *
 * Return: SUCCESS,
 *         Failure
 */
QDF_STATUS wlan_atf_open(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_atf_close(): API to close airtime fairness component
 *
 * This API is invoked from dispatcher psoc close.
 * This API will de-initialize psoc level atf object.
 *
 * Return: SUCCESS,
 *         Failure
 */
QDF_STATUS wlan_atf_close(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_atf_enable(): API to enable airtime fairness component
 *
 * This API is invoked from dispatcher psoc enable.
 * This API will register atf wim event handlers.
 *
 * Return: SUCCESS,
 *         Failure
 */
QDF_STATUS wlan_atf_enable(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_atf_disable(): API to disable airtime fairness component
 *
 * This API is invoked from dispatcher psoc disable.
 * This API will unregister atf wim event handlers.
 *
 * Return: SUCCESS,
 *         Failure
 */
QDF_STATUS wlan_atf_disable(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_atf_peer_join_leave(): API to indicate peer join and leave
 * @peer: Reference of global peer object
 * @type: 1 on join and 0 on leave
 *
 * This API get called from MLME to indicate peer join and leave.
 * So atf can take corresponding action to update atf table and rebuild
 * allocation table.
 *
 * Return: None
 */
void wlan_atf_peer_join_leave(struct wlan_objmgr_peer *peer,
			      const uint8_t type);

/**
 * wlan_atf_get_peer_airtime() - API to get configured airtime for peer
 * @peer : Reference of global peer object
 *
 * This API is used to get configured airtime for the peer.
 *
 * Return : Configured airtime
 */
uint32_t wlan_atf_get_peer_airtime(struct wlan_objmgr_peer *peer);

/**
 * wlan_atf_pdev_reset() - API to reset pdev atf object
 * @pdev : Reference of global pdev object
 *
 * This API is getting invoked from IC reset to reset pdev atf object.
 *
 * Return : None
 */
void wlan_atf_pdev_reset(struct wlan_objmgr_pdev *pdev);

/**
 * wlan_atf_peer_association_allowed() - API to check is new association allowed
 * @pdev : Reference of global pdev object
 *
 * This API is used to check new peer association is allowed or not only incase
 * of ATF is enabled. In ATF enabled case client limit is required to distribute
 * airtime proportionately.
 *
 * Return : 1 if allowed
 *          0 if not
 */
uint8_t wlan_atf_peer_association_allowed(struct wlan_objmgr_pdev *pdev);

#endif /* _WLAN_ATF_UTILS_API_H_*/
