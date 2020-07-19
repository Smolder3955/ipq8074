/*
 *
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _WLAN_FD_UCFG_API_H_
#define _WLAN_FD_UCFG_API_H_

#include <wlan_objmgr_cmn.h>

/**
 * ucfg_fd_get_enable_period() - API to get FILS enable and fd period
 * @value1: enable flag
 * @value2: FILS Discovery period
 *
 * Return: Value combinded from value1 and value2
 */
uint32_t ucfg_fd_get_enable_period(uint32_t value1, uint32_t value2);

/**
 * ucfg_fils_config() - API to configure FILS
 * @vdev:  Pointer to VDEV object
 * @value: Value from which FILS enable flag to be extracted
 *
 * Return: None
 */
void ucfg_fils_config(struct wlan_objmgr_vdev* vdev, uint32_t value);

#endif /* _WLAN_FD_UCFG_API_H_ */
