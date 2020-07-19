/*
 * Copyright (c) 2016-2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#if UMAC_SUPPORT_CFG80211
#include <ieee80211_cfg80211.h>
#include <ieee80211_defines.h>
#include <ieee80211_var.h>
#include <qdf_types.h>
/**
 *   wlan_cfg80211_wiphy_alloc: allocates new wiphy device
 *   @ops: pointer to operation supported operation table strcuture.
 *   @priv_size: size of private memory.
 *
 *   returns pointer to newly created wiphy.
 */
struct wiphy *wlan_cfg80211_wiphy_alloc(struct cfg80211_ops *ops, int priv_size)
{
    
    struct wiphy *wiphy;
    /*
     *   Create wiphy device
     */
    wiphy = wiphy_new(ops, priv_size);
    if (!wiphy) {
        qdf_print("%s : unable to allocate cfg80211 device ", __func__);
        return NULL;
    }
    return wiphy; 
}

/**
 *   wlan_cfg80211_free: free allocate wiphy.
 *   @wiphy: pointer to wiphy.
 */
void wlan_cfg80211_free(struct wiphy *wiphy)
{
    if (wiphy) {
        wiphy_free(wiphy);
    } else {
        qdf_print("%s : Error cfg80211 device is NULL ", __func__);
    }
}


/**
 *   wlan_cfg80211_register: Register wiphy with cfg80211 kernel module.
 *   @wiphy: pointer to wiphy.
 */
int wlan_cfg80211_register(struct wiphy *wiphy)
{
    /* Register our wiphy dev with cfg80211 */
    if (0 > wiphy_register(wiphy)) {
        qdf_print("%s : Unbale to register cfg80211 device ", __func__);
        return -EIO;
    }
    return 0;
}

/**
 *   wlan_cfg80211_unregister: unregister wiphy with cfg80211 kernel module.
 *   @wiphy: pointer to wiphy.
 */
void wlan_cfg80211_unregister(struct wiphy *wiphy)
{
     if (wiphy) {
         wiphy_unregister(wiphy);
     } else {
         qdf_print("%s : Error in un registering cfg80211 device ", __func__);
     }
}

/**
 *   wlan_cfg8011_set_wiphy_dev: Link wiphy with bus device.
 *   @wiphy: pointer to wiphy.
 *   @dev: bus device.
 */
void wlan_cfg8011_set_wiphy_dev (struct wiphy * wiphy, struct device * dev)
{
    if (!wiphy || !dev) {
        qdf_print("%s: Error in binding to bus device, wiphy: %pK device: %pK ", __func__, wiphy, dev);
        return;
    }
    set_wiphy_dev(wiphy, dev);
}

/**
 * is_hostie : return 1 to ignore hostapd IE
 * @ie: poniter to ie
 * @ie_len: Length of ie
 */
int is_hostie(const uint8_t *ie, uint8_t ie_len)
{
    uint8_t elem_id;
    static const uint8_t oui[4] = { WME_OUI_BYTES, WME_OUI_TYPE };
    elem_id = ie[0];

    switch(elem_id) {
        case IEEE80211_ELEMID_SSID:
        case IEEE80211_ELEMID_RATES:
        case IEEE80211_ELEMID_ERP:
        case IEEE80211_ELEMID_HTCAP_ANA:
        case IEEE80211_ELEMID_XRATES:
        case IEEE80211_ELEMID_SUPP_OP_CLASS:
        case IEEE80211_ELEMID_HTINFO_ANA:
        case IEEE80211_ELEMID_XCAPS:
        case IEEE80211_ELEMID_VHTCAP:
        case IEEE80211_ELEMID_VHTOP:
        case IEEE80211_ELEMID_VHT_TX_PWR_ENVLP:
        case IEEE80211_ELEMID_QBSS_LOAD:
            return 1;

        case IEEE80211_ELEMID_VENDOR:
/* Skip WMM ie from hostapd */
            if ( ie_len > 6 && (!OS_MEMCMP(ie+2, oui, sizeof(oui))) )
                return 1;

        default:
            return 0;
    }
}
#endif
