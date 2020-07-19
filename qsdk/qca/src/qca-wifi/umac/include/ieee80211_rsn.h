/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2008 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef _NET80211_IEEE80211_RSN_H_
#define _NET80211_IEEE80211_RSN_H_

#include <osdep.h>
#include <ieee80211_defines.h>

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
struct ieee80211_rsnparms {
    u_int32_t               rsn_authmodeset;        /* authentication mode set */
    u_int32_t               rsn_ucastcipherset;     /* unicast cipher set */
    u_int32_t               rsn_mcastcipherset;     /* mcast/group cipher set */
    u_int32_t               rsn_mcastmgmtcipherset; /* mcast/group management frames cipher set */
    u_int8_t                rsn_ucastkeylen;        /* unicast key length */
    u_int8_t                rsn_mcastkeylen;        /* mcast key length */
    u_int16_t               rsn_keymgmtset;         /* key mangement algorithms */
    u_int8_t                rsn_mcastmgmtkeylen;    /* mcast mamangement frame key length */
    u_int16_t               rsn_caps;               /* capabilities */
};
#define RSN_RESET_AUTHMODE(_rsn)        ((_rsn)->rsn_authmodeset = 0)
#define RSN_SET_AUTHMODE(_rsn, _mode)   ((_rsn)->rsn_authmodeset |= 1<<(_mode))
#define RSN_HAS_AUTHMODE(_rsn, _mode)   ((_rsn)->rsn_authmodeset & (1<<(_mode)))

#define RSN_AUTH_IS_OPEN(_rsn)          RSN_HAS_AUTHMODE((_rsn), IEEE80211_AUTH_OPEN)
#define RSN_AUTH_IS_WEP(_rsn)           RSN_HAS_AUTHMODE((_rsn), IEEE80211_AUTH_WEP)
#define RSN_AUTH_IS_SHARED_KEY(_rsn)    RSN_HAS_AUTHMODE((_rsn), IEEE80211_AUTH_SHARED)
#define RSN_AUTH_IS_8021X(_rsn)         RSN_HAS_AUTHMODE((_rsn), IEEE80211_AUTH_8021X)
#define RSN_AUTH_IS_WPA(_rsn)           RSN_HAS_AUTHMODE((_rsn), IEEE80211_AUTH_WPA)
#define RSN_AUTH_IS_RSNA(_rsn)          RSN_HAS_AUTHMODE((_rsn), IEEE80211_AUTH_RSNA)
#define RSN_AUTH_IS_CCKM(_rsn)          RSN_HAS_AUTHMODE((_rsn), IEEE80211_AUTH_CCKM)
#define RSN_AUTH_IS_WAI(_rsn)           RSN_HAS_AUTHMODE((_rsn), IEEE80211_AUTH_WAPI)
#define RSN_AUTH_IS_WPA2(_rsn)          RSN_AUTH_IS_RSNA(_rsn)

#define RSN_AUTH_MATCH(_rsn1, _rsn2)    (((_rsn1)->rsn_authmodeset & (_rsn2)->rsn_authmodeset) != 0)

#define RSN_RESET_UCAST_CIPHERS(_rsn)   ((_rsn)->rsn_ucastcipherset = 0)
#define RSN_SET_UCAST_CIPHER(_rsn, _c)  ((_rsn)->rsn_ucastcipherset |= (1<<(_c)))
#define RSN_HAS_UCAST_CIPHER(_rsn, _c)  ((_rsn)->rsn_ucastcipherset & (1<<(_c)))

#define RSN_CIPHER_IS_CLEAR(_rsn)       RSN_HAS_UCAST_CIPHER((_rsn), IEEE80211_CIPHER_NONE)
#define RSN_CIPHER_IS_WEP(_rsn)         RSN_HAS_UCAST_CIPHER((_rsn), IEEE80211_CIPHER_WEP)
#define RSN_CIPHER_IS_TKIP(_rsn)        RSN_HAS_UCAST_CIPHER((_rsn), IEEE80211_CIPHER_TKIP)
#define RSN_CIPHER_IS_CCMP128(_rsn)     RSN_HAS_UCAST_CIPHER((_rsn), IEEE80211_CIPHER_AES_CCM)
#define RSN_CIPHER_IS_CCMP256(_rsn)     RSN_HAS_UCAST_CIPHER((_rsn), IEEE80211_CIPHER_AES_CCM_256)
#define RSN_CIPHER_IS_GCMP128(_rsn)     RSN_HAS_UCAST_CIPHER((_rsn), IEEE80211_CIPHER_AES_GCM)
#define RSN_CIPHER_IS_GCMP256(_rsn)     RSN_HAS_UCAST_CIPHER((_rsn), IEEE80211_CIPHER_AES_GCM_256)
#define RSN_CIPHER_IS_SMS4(_rsn)        RSN_HAS_UCAST_CIPHER((_rsn), IEEE80211_CIPHER_WAPI)

#define RSN_RESET_MCAST_CIPHERS(_rsn)   ((_rsn)->rsn_mcastcipherset = 0)
#define RSN_SET_MCAST_CIPHER(_rsn, _c)  ((_rsn)->rsn_mcastcipherset |= (1<<(_c)))
#define RSN_HAS_MCAST_CIPHER(_rsn, _c)  ((_rsn)->rsn_mcastcipherset & (1<<(_c)))

#define RSN_RESET_MCASTMGMT_CIPHERS(_rsn)   ((_rsn)->rsn_mcastmgmtcipherset = 0)
#define RSN_SET_MCASTMGMT_CIPHER(_rsn, _c)  ((_rsn)->rsn_mcastmgmtcipherset |= (1<<(_c)))
#define RSN_HAS_MCASTMGMT_CIPHER(_rsn, _c)  ((_rsn)->rsn_mcastmgmtcipherset & (1<<(_c)))

#define RSN_CIPHER_IS_CMAC(_rsn)            RSN_HAS_MCASTMGMT_CIPHER((_rsn), IEEE80211_CIPHER_AES_CMAC)
#define RSN_CIPHER_IS_CMAC256(_rsn)         RSN_HAS_MCASTMGMT_CIPHER((_rsn), IEEE80211_CIPHER_AES_CMAC_256)
#define RSN_CIPHER_IS_GMAC(_rsn)            RSN_HAS_MCASTMGMT_CIPHER((_rsn), IEEE80211_CIPHER_AES_GMAC)
#define RSN_CIPHER_IS_GMAC256(_rsn)         RSN_HAS_MCASTMGMT_CIPHER((_rsn), IEEE80211_CIPHER_AES_GMAC_256)

#define RSN_UCAST_CIPHER_MATCH(_rsn1, _rsn2)    \
    (((_rsn1)->rsn_ucastcipherset & (_rsn2)->rsn_ucastcipherset) != 0)

#define RSN_MCAST_CIPHER_MATCH(_rsn1, _rsn2)    \
    (((_rsn1)->rsn_mcastcipherset & (_rsn2)->rsn_mcastcipherset) != 0)

#define RSN_MCASTMGMT_CIPHER_MATCH(_rsn1, _rsn2)    \
    (((_rsn1)->rsn_mcastmgmtcipherset & (_rsn2)->rsn_mcastmgmtcipherset) != 0)

#define RSN_KEY_MGTSET_MATCH(_rsn1, _rsn2)      \
    (((_rsn1)->rsn_keymgmtset & (_rsn2)->rsn_keymgmtset) != 0 ||    \
     (!(_rsn1)->rsn_keymgmtset && !(_rsn2)->rsn_keymgmtset))

int cipher2cap(int);
void ieee80211_rsn_vattach(struct ieee80211vap *vap);
void ieee80211_rsn_reset(struct ieee80211vap *vap);
bool ieee80211_match_rsn_info(struct ieee80211vap *vap,struct ieee80211_rsnparms *rsn_parms);
#endif
ieee80211_cipher_type ieee80211_get_current_mcastcipher(struct ieee80211vap *vap);
bool ieee80211_auth_mode_needs_upper_auth( struct ieee80211vap *vap );
#endif /* _NET80211_IEEE80211_RSN_H_ */
