/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

#ifndef _IEEE80211_ACL_H
#define _IEEE80211_ACL_H

#define ACL_ENTRY_MAX  256

typedef struct ieee80211_acl    *ieee80211_acl_t;

int ieee80211_acl_attach(wlan_if_t vap);
int ieee80211_acl_detach(wlan_if_t vap);
int ieee80211_acl_add(wlan_if_t vap, const u_int8_t mac[IEEE80211_ADDR_LEN], u_int8_t acl_list_id);
int ieee80211_acl_remove(wlan_if_t vap, const u_int8_t mac[IEEE80211_ADDR_LEN], u_int8_t acl_list_id);
int ieee80211_acl_get(wlan_if_t vap, u_int8_t *mac_list, int len, int *num_mac, u_int8_t acl_list_id);
int ieee80211_acl_check(wlan_if_t vap, const u_int8_t mac[IEEE80211_ADDR_LEN]);
int ieee80211_acl_flush(wlan_if_t vap, u_int8_t acl_list_id);
int ieee80211_acl_setpolicy(wlan_if_t vap, int policy, u_int8_t acl_list_id);
int ieee80211_acl_getpolicy(wlan_if_t vap, u_int8_t acl_list_id);
int ieee80211_acl_add_with_validity(struct ieee80211vap *vap,
                        const u_int8_t *mac_addr, u_int16_t validity);
#if ATH_ACL_SOFTBLOCKING
int wlan_acl_set_softblocking(struct ieee80211vap *vap, const u_int8_t *mac_addr, bool enable);
int wlan_acl_get_softblocking(struct ieee80211vap *vap, const u_int8_t *mac_addr);
bool wlan_acl_check_softblocking(struct ieee80211vap *vap, const u_int8_t *mac_addr);
#endif

/**
 * @brief Special flags that can be set by the band steering module (and
 *        potentially others in the future) on individual ACL entries.
 */
enum ieee80211_acl_flag {
    IEEE80211_ACL_FLAG_PROBE_RESP_WH = 1 << 0,  /* withhold probe responses */
    IEEE80211_ACL_FLAG_ACL_LIST_1    = 1 << 1,  /* Denotes ACL list 1 */
    IEEE80211_ACL_FLAG_ACL_LIST_2    = 1 << 2,  /* Denotes ACL list 2 */
    IEEE80211_ACL_FLAG_AUTH_ALLOW    = 1 << 3,  /* Denotes Auth Allow */
    IEEE80211_ACL_FLAG_SOFTBLOCKING  = 1 << 4,  /* Denotes softblocking */
    IEEE80211_ACL_FLAG_VALIDITY_TIMER = 1 << 5, /* Denotes timer validity */
    IEEE80211_ACL_FLAG_BLOCK_MGMT    = 1 << 6,  /* Denotes block mgmt */
};

#if defined(QCA_SUPPORT_SON) || defined(ATH_ACL_SOFTBLOCKING)
int
ieee80211_acl_flag_check(wlan_if_t vap, const u_int8_t mac[IEEE80211_ADDR_LEN],
                         enum ieee80211_acl_flag flag);
int
ieee80211_acl_set_flag(wlan_if_t vap, const u_int8_t mac[IEEE80211_ADDR_LEN],
                       enum ieee80211_acl_flag flag);
int
ieee80211_acl_clr_flag(wlan_if_t vap, const u_int8_t mac[IEEE80211_ADDR_LEN],
                       enum ieee80211_acl_flag flag);

#endif


#endif



