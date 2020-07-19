/*
 * Copyright (c) 2016,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#if UMAC_SUPPORT_CFG80211
#ifndef IEEE80211_CRYPTO_NL80211_H
#define IEEE80211_CRYPTO_NL80211_H
#define TKIP_RXMIC_OFFSET 24;
#define TKIP_TXMIC_OFFSET 16;

#define ELEM_LEN(x)                     ( x[1] )
#define IE_LEN(x)                       ( ELEM_LEN(x) + 2 )
#define MAX_SEQ_LEN                     sizeof(u_int64_t)


/* AKM suite selectors */
#define WLAN_AKM_SUITE_8021X            0x000FAC01
#define WLAN_AKM_SUITE_PSK              0x000FAC02
#define WLAN_AKM_SUITE_FT_8021X         0x000FAC03
#define WLAN_AKM_SUITE_FT_PSK           0x000FAC04
#define WLAN_AKM_SUITE_8021X_SHA256     0x000FAC05
#define WLAN_AKM_SUITE_PSK_SHA256       0x000FAC06
#define WLAN_AKM_SUITE_TPK_HANDSHAKE    0x000FAC07
#define WLAN_AKM_SUITE_SAE              0x000FAC08
#define WLAN_AKM_SUITE_FT_OVER_SAE      0x000FAC09
#define WLAN_AKM_SUITE_8021X_SUITE_B    0x000FAC0B
#define WLAN_AKM_SUITE_8021X_SUITE_B_192        0x000FAC0C
#define WLAN_AKM_SUITE_FILS_SHA256      0x000FAC0E
#define WLAN_AKM_SUITE_FILS_SHA384      0x000FAC0F
#define WLAN_AKM_SUITE_FT_FILS_SHA256   0x000FAC10
#define WLAN_AKM_SUITE_FT_FILS_SHA384   0x000FAC11
#define WLAN_AKM_SUITE_OWE              0x000FAC12
#define WLAN_AKM_SUITE_CCKM             0x00409600
#define WLAN_AKM_SUITE_OSEN             0x506F9A01
#define WLAN_AKM_SUITE_DPP              0x506F9A02

/* cipher suite selectors */
#define WLAN_CIPHER_SUITE_USE_GROUP     0x000FAC00
#define WLAN_CIPHER_SUITE_WEP40         0x000FAC01
#define WLAN_CIPHER_SUITE_TKIP          0x000FAC02
/* reserved:                            0x000FAC03 */
#define WLAN_CIPHER_SUITE_CCMP          0x000FAC04
#define WLAN_CIPHER_SUITE_WEP104        0x000FAC05
#define WLAN_CIPHER_SUITE_AES_CMAC      0x000FAC06
#define WLAN_CIPHER_SUITE_NO_GROUP_ADDR 0x000FAC07
#define WLAN_CIPHER_SUITE_GCMP          0x000FAC08
#define WLAN_CIPHER_SUITE_GCMP_256      0x000FAC09
#define WLAN_CIPHER_SUITE_CCMP_256      0x000FAC0A
#define WLAN_CIPHER_SUITE_BIP_GMAC_128  0x000FAC0B
#define WLAN_CIPHER_SUITE_BIP_GMAC_256  0x000FAC0C
#define WLAN_CIPHER_SUITE_BIP_CMAC_256  0x000FAC0D

int wlan_cfg80211_add_key(struct wiphy *wiphy, struct net_device *ndev, u8 key_index, bool pairwise, const u8 *mac_addr, struct key_params *params);
int wlan_cfg80211_get_key(struct wiphy *wiphy, struct net_device *ndev, u8 key_index, bool pairwise, const u8 *mac_addr, void *cookie, void (*callback)(void *cookie, struct key_params *));
int wlan_cfg80211_del_key(struct wiphy *wiphy, struct net_device *dev, u8 key_index, bool pairwise, const u8 *mac_addr);
int wlan_cfg80211_set_default_key(struct wiphy *wiphy, struct net_device *ndev, u8 key_index, bool unicast, bool multicast);

int wlan_cfg80211_security_init(struct cfg80211_ap_settings *params,struct net_device *dev);
int wlan_cfg80211_crypto_setting(struct net_device *dev,
                                 struct cfg80211_crypto_settings *params,
                                 enum nl80211_auth_type auth_type);
int wlan_set_beacon_ies(struct  net_device *dev, struct cfg80211_beacon_data *beacon);

#endif /* IEEE80211_CRYPTO_NL80211_H */
#endif /* UMAC_SUPPORT_CFG80211 */

