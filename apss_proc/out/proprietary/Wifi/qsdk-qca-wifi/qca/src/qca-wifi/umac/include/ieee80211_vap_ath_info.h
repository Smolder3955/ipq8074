/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2010 Atheros Communications Inc.  All rights reserved.
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#ifndef _IEEE80211_VAP_ATH_INFO_H
#define _IEEE80211_VAP_ATH_INFO_H

#include <umac_lmac_common.h>

struct  ieee80211_vap_ath_info;
typedef struct ieee80211_vap_ath_info *ieee80211_vap_ath_info_t;

struct  ieee80211_vap_ath_info_notify;
typedef struct ieee80211_vap_ath_info_notify *ieee80211_vap_ath_info_notify_t;

typedef void (*ieee80211_vap_ath_info_notify_func)(void *arg, ath_vap_infotype type,
                                               u_int32_t param1, u_int32_t param2);

#if ATH_SUPPORT_ATHVAP_INFO
    
ieee80211_vap_ath_info_t
ieee80211_vap_ath_info_attach(wlan_if_t vap);

void
ieee80211_vap_ath_info_detach(ieee80211_vap_ath_info_t handle);

ieee80211_vap_ath_info_notify_t
ieee80211_vap_ath_info_reg_notify(
    wlan_if_t                           vap,
    ath_vap_infotype                    infotype_mask,
    ieee80211_vap_ath_info_notify_func  vap_ath_info_callback,
    void                                *arg);

int
ieee80211_vap_ath_info_dereg_notify(
    ieee80211_vap_ath_info_notify_t    h_notify);

int
ieee80211_vap_ath_info_get(
    wlan_if_t                   vap,
    ath_vap_infotype            infotype,
    u_int32_t                   *param1,
    u_int32_t                   *param2);

#else   //ATH_SUPPORT_ATHVAP_INFO

/* Benign Functions */

#define ieee80211_vap_ath_info_attach(vap)                               (NULL) 
#define ieee80211_vap_ath_info_detach(handle)                            /**/ 

static INLINE ieee80211_vap_ath_info_notify_t
ieee80211_vap_ath_info_reg_notify(
    wlan_if_t                           vap,
    ath_vap_infotype                    infotype_mask,
    ieee80211_vap_ath_info_notify_func  vap_ath_info_callback,
    void                                *arg)
{
    printk("%s: WARNING: module ATH_SUPPORT_ATHVAP_INFO is not included\n", __func__);
    return NULL;
}

#define ieee80211_vap_ath_info_dereg_notify(handle)           \
    /*                                                        \
     * Do a dummy assignment action to avoid compiler warning \
     * of a statement with no effect                          \
     */                                                       \
    ((handle=NULL) ? EOK : EOK)

static INLINE int
ieee80211_vap_ath_info_get(
    wlan_if_t                   vap,
    ath_vap_infotype            infotype,
    u_int32_t                   *param1,
    u_int32_t                   *param2)
{
    printk("%s: WARNING: module ATH_SUPPORT_ATHVAP_INFO is not included\n", __func__);
    return -EINVAL;
}

#endif  //ATH_SUPPORT_ATHVAP_INFO

#endif  //_IEEE80211_VAP_ATH_INFO_H

