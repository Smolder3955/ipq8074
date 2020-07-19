/*
 * Copyright (c) 2016,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#if UMAC_SUPPORT_CFG80211
#ifndef __CFG80211_API_H__
#define __CFG80211_API_H__
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/etherdevice.h>
#include <linux/wireless.h>
#include <net/arp.h>
#include <linux/ieee80211.h>
#include <net/cfg80211.h>

/**
 *   wlan_cfg80211_wiphy_alloc: allocates new wiphy device
 *   @ops: pointer to operation supported operation table strcuture.
 *   @priv_size: size of private memory.
 *
 *   returns pointer to newly created wiphy.
 */
struct wiphy *wlan_cfg80211_wiphy_alloc(struct cfg80211_ops *ops, int priv_size);

/**
 *   wlan_cfg80211_free: free allocate wiphy.
 *   @wiphy: pointer to wiphy.
 */
void wlan_cfg80211_free(struct wiphy *wiphy);


/**
 *   wlan_cfg80211_register: Register wiphy with cfg80211 kernel module.
 *   @wiphy: pointer to wiphy.
 */
int wlan_cfg80211_register(struct wiphy *wiphy);

/**
 *   wlan_cfg80211_unregister: unregister wiphy with cfg80211 kernel module.
 *   @wiphy: pointer to wiphy.
 */
void wlan_cfg80211_unregister(struct wiphy *wiphy);

/**
 *   wlan_cfg8011_set_wiphy_dev: Link wiphy with bus device.
 *   @wiphy: pointer to wiphy.
 *   @dev: bus device.
 */
void wlan_cfg8011_set_wiphy_dev (struct wiphy *wiphy, struct device *dev);

/**
 * is_hostie
 * @ie: poniter to ie
 * @ie_len: IE length
 */
int is_hostie(const uint8_t *ie, uint8_t ie_len);
#endif
#endif
