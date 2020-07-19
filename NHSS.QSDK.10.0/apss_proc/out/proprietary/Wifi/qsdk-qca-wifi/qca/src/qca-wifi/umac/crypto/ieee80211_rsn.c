/*
 * Copyright (c) 2011-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c)2008 Atheros Communications Inc.
 * All Rights Reserved.

 */

#include <osdep.h>

#if ATH_SUPPORT_WRAP
#include <if_athvar.h>
#endif
#include <ieee80211_var.h>
#include <ieee80211_api.h>
#if UNIFIED_SMARTANTENNA
#include <wlan_sa_api_utils_api.h>
#endif
#include "ald_netlink.h"
#include <wlan_son_pub.h>

#include <wlan_cmn.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#endif
#include <wlan_mlme_dp_dispatcher.h>
#include <wlan_vdev_mlme.h>

bool ieee80211_auth_mode_needs_upper_auth( struct ieee80211vap *vap )
{
    ieee80211_auth_mode   auth_mode;

    auth_mode = IEEE80211_AUTH_OPEN;

    wlan_get_auth_modes(vap, &auth_mode, 1);

    return ( ( vap->iv_wps_mode )  ||  (auth_mode != IEEE80211_AUTH_OPEN && auth_mode != IEEE80211_AUTH_NONE  && auth_mode != IEEE80211_AUTH_SHARED) );
}

static int
ieee80211_set_key(struct ieee80211vap *vap, struct ieee80211_key *k,
        ieee80211_keyval *kval,
        struct ieee80211_node *ni)
{
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    int i;

    if (kval->keylen > IEEE80211_KEYBUF_SIZE)
        return -EINVAL;

    /*
     * for key mapping key, its algorithm must match current
     * unicast cipher set (unless the key is for multicast/broadcast
     * data frames).
     */
    if ((k->wk_flags & IEEE80211_KEY_GROUP) == 0) {
        if (! IEEE80211_IS_MULTICAST(kval->macaddr) &&
            ! RSN_HAS_UCAST_CIPHER(rsn, kval->keytype))
            return -EINVAL;
    }
    /*
     * for per-STA key, or key mapping key for multicast/broadcast
     * data frames, its algorithm must match the current multicast cipher.
     */
    if ((k->wk_flags & IEEE80211_KEY_PERSTA) ||
        IEEE80211_IS_MULTICAST(kval->macaddr)) {
        if (vap->iv_opmode != IEEE80211_M_MONITOR) {
            if (! RSN_HAS_MCAST_CIPHER(rsn, kval->keytype))
                return -EINVAL;
        }
    }
    /*
     * for default key, its algorithm must match either the
     * current unicast cipher set or the current multicast cipher.
     */
    if (vap->iv_opmode != IEEE80211_M_MONITOR) {
        if ((! RSN_HAS_UCAST_CIPHER(rsn, kval->keytype)) &&
            (! RSN_HAS_MCAST_CIPHER(rsn, kval->keytype))) {
            return -EINVAL;
        }
    }

    /*
     * Update the key entry for valid key. We cannot fail after this point. Otherwise,
     * the existing key will be modified when we fail to set the new key.
     */
    k->wk_keylen = kval->keylen;

    /* NB We'll bring in TKIP MIC keys below if needed */
    OS_MEMCPY(k->wk_key, kval->keydata, kval->keylen);
    OS_MEMZERO(k->wk_key + kval->keylen, (sizeof(k->wk_key) - kval->keylen));

    switch (kval->keytype) {
    case IEEE80211_CIPHER_WEP:
    case IEEE80211_CIPHER_NONE:
    case IEEE80211_CIPHER_CKIP:
        break;

    case IEEE80211_CIPHER_TKIP:
    case IEEE80211_CIPHER_WAPI:
        for (i = 0; i < IEEE80211_TID_SIZE; i++)
            k->wk_keyrsc[i] = kval->keyrsc;
        k->wk_keytsc = kval->keytsc;

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                        "%s: init. TKIP PN to 0x%x%x, tsc=0x%x%x.\n", __func__,
                        (u_int32_t)(kval->keyrsc>>32), (u_int32_t)(kval->keyrsc),
                        (u_int32_t)(kval->keytsc>>32), (u_int32_t)(kval->keytsc));

        /* copy the MIC keys */
        OS_MEMCPY(k->wk_rxmic, kval->keydata + kval->rxmic_offset, 8);
        OS_MEMCPY(k->wk_txmic, kval->keydata + kval->txmic_offset, 8);
        break;

    case IEEE80211_CIPHER_AES_CCM:
    case IEEE80211_CIPHER_AES_CCM_256:
    case IEEE80211_CIPHER_AES_GCM:
    case IEEE80211_CIPHER_AES_GCM_256:
    case IEEE80211_CIPHER_AES_OCB:
    case IEEE80211_CIPHER_AES_CMAC:
    case IEEE80211_CIPHER_AES_CMAC_256:
    case IEEE80211_CIPHER_AES_GMAC:
    case IEEE80211_CIPHER_AES_GMAC_256:
        for (i = 0; i < IEEE80211_TID_SIZE; i++)
            k->wk_keyrsc[i] = kval->keyrsc;
        k->wk_keytsc = kval->keytsc;

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                        "%s: init. AES PN to 0x%x%x, tsc=0x%x%x.\n", __func__,
                        (u_int32_t)(kval->keyrsc>>32), (u_int32_t)(kval->keyrsc),
                        (u_int32_t)(kval->keytsc>>32), (u_int32_t)(kval->keytsc));
        break;
     default:
	return -EINVAL;
    }

#if ATH_SUPPORT_WAPI
    if (kval->keytype == IEEE80211_CIPHER_WAPI) {
        sta_wapi_init(vap, k);
    } else {
    /* clear the WAPI flag in vap for non-WAPI keys/ciphers*/
        ieee80211_vap_wapi_clear(vap);
    }
#endif
    if (ieee80211_crypto_setkey(vap, k, kval->macaddr, ni) == 0) {
        return -EINVAL;
    }

    if (k->wk_flags & IEEE80211_KEY_PERSTA) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s", "key: ");
        for (i = 0; i < k->wk_keylen; i++) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%02x", k->wk_key[i]);
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s", "\n");
    }

    if(! IEEE80211_IS_MULTICAST(kval->macaddr)){
        ieee80211_vap_event           evt;

        if(vap->iv_opmode == IEEE80211_M_STA && ieee80211_auth_mode_needs_upper_auth(vap)){
            evt.type = IEEE80211_VAP_AUTH_COMPLETE;
            ieee80211_vap_deliver_event(vap, &evt);
        }
    }
#endif
    return 0;
}


#ifndef WLAN_CONV_CRYPTO_SUPPORTED
static int
ieee80211_set_persta_default_key(struct ieee80211vap *vap, u_int16_t keyix, ieee80211_keyval *kval)
{
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    struct ieee80211_node *ni = NULL;
    struct ieee80211_key *swkey, *hwkey;
    u_int8_t flags;
    int error = 0;

    KASSERT((vap->iv_opmode == IEEE80211_M_IBSS),
            ("WARN: %s called for non IBSS mode vap: %pK, opmode: %d\n", __func__, vap, vap->iv_opmode));

    if (!RSN_AUTH_IS_RSNA(rsn)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: per-station key is only supported in RSNA", __func__);
        error = -EINVAL;
        goto bad;
    }

    if (!RSN_HAS_UCAST_CIPHER(rsn, IEEE80211_CIPHER_AES_CCM) ||
        !RSN_HAS_MCAST_CIPHER(rsn, IEEE80211_CIPHER_AES_CCM)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                          "%s: per-station key is only supported for AES\n", __func__);
        error = -EINVAL;
        goto bad;
    }

    ni = ieee80211_vap_find_node(vap, kval->macaddr);
    if (ni == NULL) {
        /* If no peer, we can neigher install nor delete the key */
        error = -EIO;
        goto bad;
    }

    KASSERT((ni->ni_persta != NULL), ("ERROR: %s: ni->ni_persta is NULL\n", __func__));

    /*
     * In IBSS, the default group id is always set to 1 when the ID 1 key is valid.
     * If the default group key id is 1, we shall keep the persta key id same as group key .
     * Otherwise, the invalid persta key will be used.
     */
    if (vap->iv_nw_keys[1].wk_valid)
    {
        keyix = 1;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                          "%s: Keep the key id with the default group key, new id = %d\n",
                          __func__, keyix);
    }
    /* If the persta key already exists but now a new keyix is passed, we still use the original */
    if (ni->ni_persta->nips_swkey[1].wk_valid)
        keyix = 1;
#if ATH_SUPPORT_IBSS_WPA2
    else if(vap->iv_opmode == IEEE80211_M_IBSS)
        keyix = 1;
#endif

    /*
     * We will use ieee80211_set_key() to install the key material into the swkey.
     * We use ieee80211_crypto_newkey() to attach the crypto state to the swkey,
     * but there is no need for ieee80211_crypto_setkey() since we don't install
     * the swkey into the hw.
     */
    swkey = &ni->ni_persta->nips_swkey[keyix];

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                      "%s: install perSTA swkey keyID %d, keyLen %d\n",
                      __func__, keyix, kval->keylen);

    flags = IEEE80211_KEY_RECV | IEEE80211_KEY_GROUP | IEEE80211_KEY_PERSTA;
    flags |= (kval->persistent) ? IEEE80211_KEY_PERSISTENT : 0;

    /* if swkey is already in h/w, do not alloc new slot in keycache */
    if (swkey->wk_keyix < IEEE80211_WEP_NKID)
        swkey->wk_keyix = keyix;

    if (swkey->wk_keyix < IEEE80211_WEP_NKID)
        swkey->wk_keyix = IEEE80211_KEYIX_NONE;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                      "%s: wk_keyix = %d\n",
                       __func__, swkey->wk_keyix);

    if (ieee80211_crypto_newkey(vap, kval->keytype, flags, swkey) == 0) {
        error = -EIO;
        goto bad;
    }

    /* setup the sw key */
    error = ieee80211_set_key(vap, swkey, kval, ni);
    if (error)
        goto bad;

#if ATH_SUPPORT_IBSS
    /*
     * If we put the swkey in h/w, its keyidx will be re-alloced.
     * We do not use the clear key in such a case.
     */
    if (swkey->wk_keyix < IEEE80211_WEP_NKID) {
#endif
        /*
         * setup the hw key if not yet done
         * NB: for hardware clear key, we don't need to setup tsc/rsc, and
         * it's AES only, so we bypass ieee80211_set_key() to directly call
         * ieee80211_cryto_setkey() here.
         */
        hwkey = &ni->ni_persta->nips_hwkey;
        if (!hwkey->wk_valid) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "%s: install perSTA hwkey keyID %d, keyLen %d\n",
                              __func__, keyix, kval->keylen);

            /* newkey will turn on _SWCRYPT */
            flags = IEEE80211_KEY_RECV | IEEE80211_KEY_GROUP | IEEE80211_KEY_PERSTA;
            flags |= (kval->persistent) ? IEEE80211_KEY_PERSISTENT : 0;

            /*
             * We use a clear key for the persta hw key so we can fwd the
             * frame to the sw key.
             */
            if (ieee80211_crypto_newkey(vap, IEEE80211_CIPHER_NONE, flags, hwkey) == 0) {
                /* XXX have to back out the inserted swkey above */
                error = -EIO;
                goto bad;
            }

            /* clear the hwkey key material */
            OS_MEMZERO(hwkey->wk_key, sizeof(hwkey->wk_key));

            /* XXX setkey really wants a valid length */
            hwkey->wk_keylen = IEEE80211_KEYBUF_SIZE;
            if (ieee80211_crypto_setkey(vap, hwkey, kval->macaddr, ni) == 0) {
                /* XXX have to back out the inserted swkey above */
                error = -EIO;
                goto bad;
            }
        }
#if ATH_SUPPORT_IBSS
    }
#endif
bad:
    if (ni != NULL)
        ieee80211_free_node(ni);

    return error;
}

static int
ieee80211_set_default_key(struct ieee80211vap *vap, u_int16_t keyix, ieee80211_keyval *kval)
{
    struct ieee80211_key *k;
    u_int8_t flags;

    if ((keyix >= IEEE80211_WEP_NKID)
        && (kval->keytype != IEEE80211_CIPHER_AES_CMAC)
        && (kval->keytype != IEEE80211_CIPHER_AES_CMAC_256)
        && (kval->keytype != IEEE80211_CIPHER_AES_GMAC)
        && (kval->keytype != IEEE80211_CIPHER_AES_GMAC_256)){
        printk("%s: invalid key index %d\n", __func__, keyix);
        return -EINVAL;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                      "%s: Default keyID %d, persist %d, algoId %s, keyLen %d, mac = %s\n",
                      __func__, keyix, kval->persistent,
                      cipher2str(kval->keytype), kval->keylen,
                      ether_sprintf(kval->macaddr));

    if (vap->iv_opmode == IEEE80211_M_IBSS &&
#if ATH_SUPPORT_IBSS_PRIVATE_SECURITY
        !IEEE80211_IS_MULTICAST(kval->macaddr) &&
#endif
        (kval->macaddr[0] != 0 || kval->macaddr[1] != 0 ||
         kval->macaddr[2] != 0 || kval->macaddr[3] != 0 ||
         kval->macaddr[4] != 0 || kval->macaddr[5] != 0)) {
        /*
         * The key is a per-STA default key for an ad-hoc station
         */
        ASSERT(! IEEE80211_IS_MULTICAST(kval->macaddr));
        return ieee80211_set_persta_default_key(vap, keyix, kval);
    } else {
        /*
         * allocate a default key slot
         */
        flags = IEEE80211_KEY_GROUP; /* to mark it as a default key */
        flags |= (kval->persistent) ? IEEE80211_KEY_PERSISTENT : 0;

        if (vap->iv_opmode == IEEE80211_M_IBSS) {
            if (kval->keytype == IEEE80211_CIPHER_WEP)
                flags |= IEEE80211_KEY_RECV; /* RX only until OS sends default key index */
            else
                flags |= IEEE80211_KEY_XMIT;
        } else {
            if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
                /* trtanslate the key direction into flags */
                flags |= (kval->keydir == IEEE80211_KEY_DIR_BOTH) ? (IEEE80211_KEY_RECV|IEEE80211_KEY_XMIT) :
                          ((kval->keydir == IEEE80211_KEY_DIR_TX) ? (IEEE80211_KEY_XMIT) :
                           ((kval->keydir == IEEE80211_KEY_DIR_RX) ? (IEEE80211_KEY_RECV): IEEE80211_KEY_RECV));
            } else {
               flags |= IEEE80211_KEY_RECV; /* RX only until OS sends default key index */
            }
        }

        if (vap->iv_opmode == IEEE80211_M_IBSS) {
            /*
             * In IBSS, if the default key alreay exists but now a new keyix is passed,
             * we still use the original one.
             */
            if (vap->iv_nw_keys[1].wk_valid)
            {
                keyix = 1;
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                                  "%s: install default keyID change, new id = %d\n",
                                  __func__, keyix);
            }
        }

        if ((kval->keytype == IEEE80211_CIPHER_AES_CMAC)
             || (kval->keytype == IEEE80211_CIPHER_AES_CMAC_256)
             || (kval->keytype == IEEE80211_CIPHER_AES_GMAC)
             || (kval->keytype == IEEE80211_CIPHER_AES_GMAC_256)){
            /* IGTK Key. save it in VAP to compute MIC in MMIE
             * for broadcast/multicast Robust mgmt frames
             */
            // This key is used for Broadcast Robust management frames
            // to compute MIC in MMIE. so no need to set to hardware.
            RSN_RESET_MCASTMGMT_CIPHERS(&vap->iv_rsn);
            RSN_SET_MCASTMGMT_CIPHER(&vap->iv_rsn, kval->keytype);
            return ieee80211_set_igtk_key(vap, keyix, kval);
        }

        /* This key comes from the vap static store */
        k = &vap->iv_nw_keys[keyix];


        if (ieee80211_crypto_newkey(vap, kval->keytype, flags, k) == 0) {
            return -EIO;
        }
        /* save the new default key in the key table */
        return ieee80211_set_key(vap, k, kval, vap->iv_bss);
    }
}

static int
ieee80211_set_keymapping_key(struct ieee80211vap *vap, ieee80211_keyval *kval)
{
    struct ieee80211_key *k;
    struct ieee80211_node *ni = NULL;
    u_int8_t flags;
    int error = 0;

    /* AP must be ready */
    if (wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) {
        printk("%s:vap not ready.\n", __FUNCTION__);
        error = -EINVAL;
        return error;
    }

    /*
     * We don't support uni-directional key mapping keys
     */
    if (kval->keydir != IEEE80211_KEY_DIR_BOTH) {
        error = -EINVAL;
        goto bad;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                      "%s: Keymapping persist %d, algoId %s, keyLen %d, mac = %s\n",
                      __func__, kval->persistent, cipher2str(kval->keytype),
                      kval->keylen,
                      ether_sprintf(kval->macaddr));

    ni = ieee80211_vap_find_node(vap, kval->macaddr);
    if (ni == NULL) {
        /*
         * Node not found.
         * The STA might have left before hostapd has plumbed the keys.
         * Simply return failure. We expect the STA to reconnect again.
         */

        /*
         * XXX: In 8021x-WEP the keys might be needed here. But creating a node (dup_bss)
         * seems to cause race conditions with mlme_recv_auth_ap, where nodes are usually
         * created. So just leave with a message.
         */
        if (kval->keytype == IEEE80211_CIPHER_WEP)
            printk("%s:node not found, dropping WEP keys. check.\n", __FUNCTION__);

        error = -EINVAL;
        goto bad;
    }

    if (vap->iv_opmode == IEEE80211_M_STA && ni != vap->iv_bss) {
        /*
         * The peer mac address matches our associated AP, but the
         * node found is not the bss node.
         *
         * XXX Can this happen ???
         */
        error = -EINVAL;
        goto bad;
    }

    /*
     * The ni might have changed before this set key has come from hostapd.
     * Check this using the associd and return
     */
    if (ni && (ni != vap->iv_bss) && (ni->ni_associd == 0)) {
        error = -EINVAL;
        goto bad;
    }

#if ATH_SUPPORT_WAPI
    ni->ni_wkused = kval->key_used;
#endif
    k = &ni->ni_ucastkey;
    if (kval->keyindex != IEEE80211_KEYIX_NONE){
        /* use the new key index set from OS */
        k->wk_keyix = kval->keyindex;
    }

    /*
     * When already associated station sends auth frame, pending frames
     * already queued in hardware may have huge PN. Instead of starting
     * from zero, use tsc value in old node
     */
    if (ni->ni_flags & IEEE80211_NODE_TSC_SET) {
        kval->keytsc = k->wk_keytsc;
        ieee80211node_clear_flag(ni, IEEE80211_NODE_TSC_SET);
    }


    /*
     * If extant key, then might need to do cleanup if we're changing keytypes...
     * Will OS ever give us a new key on top of old key with no delete?
     */

    /*
     * Strategy:  allocate the slot here with _newkey().  Then use
     * _setkey() in ieee80211_set_key() where we would otherwise call _newkey().
     * We defer validation of the key material itself until _setkey().
     */
#if DBG
    if (!k->wk_valid) {
        if (k->wk_cipher != &ieee80211_cipher_none) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "cipher non-empty\n");
        }
        if (k->wk_keyix != IEEE80211_KEYIX_NONE) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "Key index incorrect\n");
        }
    }
#endif

    flags = IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV;
    flags |= (kval->persistent) ? IEEE80211_KEY_PERSISTENT : 0;
    flags |= (kval->mfp) ? IEEE80211_KEY_MFP : 0;

    if (ieee80211_crypto_newkey(vap, kval->keytype, flags, k) == 0) {
        error = -EIO;
        goto bad;
    }

    error = ieee80211_set_key(vap, k, kval, ni);

bad:
    if (ni != NULL)
        ieee80211_free_node(ni);

    return error;
}


int
cipher2cap(int cipher)
{
    switch (cipher)
    {
    case IEEE80211_CIPHER_WEP:  return IEEE80211_C_WEP;
    case IEEE80211_CIPHER_AES_OCB:  return IEEE80211_C_AES;
    case IEEE80211_CIPHER_AES_CCM:  return IEEE80211_C_AES_CCM;
    case IEEE80211_CIPHER_AES_CCM_256:  return IEEE80211_C_AES_CCM;
    case IEEE80211_CIPHER_AES_GCM:  return IEEE80211_C_AES_CCM;
    case IEEE80211_CIPHER_AES_GCM_256:  return IEEE80211_C_AES_CCM;
    case IEEE80211_CIPHER_CKIP: return IEEE80211_C_CKIP;
    case IEEE80211_CIPHER_TKIP: return IEEE80211_C_TKIP;
#if ATH_SUPPORT_WAPI
    case IEEE80211_CIPHER_WAPI: return IEEE80211_C_WAPI;
#endif
    }
    return 0;
}

/*
 * Set default ucast and mcast cipher set associated with the authentication mode
 */
static void
ieee80211_set_default_ciphers(struct ieee80211vap *vap, ieee80211_auth_mode auth)
{
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    int sup_wep, sup_tkip, sup_ccmp128, sup_ccmp256, sup_gcmp128, sup_gcmp256;
#if ATH_SUPPORT_WAPI
    int sup_sms4;
#endif

    sup_wep = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_WEP)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_WEP);
    sup_tkip = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_TKIP)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_TKIP);
    sup_ccmp128 = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_AES_CCM)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_AES_CCM);
    sup_ccmp256 = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_AES_CCM_256)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_AES_CCM_256);
    sup_gcmp128 = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_AES_GCM)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_AES_GCM);
    sup_gcmp256 = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_AES_GCM_256)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_AES_GCM_256);
#if ATH_SUPPORT_WAPI
    sup_sms4 = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_WAPI)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_WAPI);
#endif

    RSN_RESET_UCAST_CIPHERS(rsn);
    RSN_RESET_MCAST_CIPHERS(rsn);
    RSN_RESET_MCASTMGMT_CIPHERS(rsn);

    switch (auth) {
    case IEEE80211_AUTH_OPEN:
    case IEEE80211_AUTH_8021X:
        RSN_SET_MCAST_CIPHER(rsn, IEEE80211_CIPHER_NONE);
        RSN_SET_MCASTMGMT_CIPHER(rsn, IEEE80211_CIPHER_NONE);
        if (sup_wep) {
            RSN_SET_UCAST_CIPHER(rsn, IEEE80211_CIPHER_WEP);
            RSN_SET_MCAST_CIPHER(rsn, IEEE80211_CIPHER_WEP);
        } else {
            RSN_SET_UCAST_CIPHER(rsn, IEEE80211_CIPHER_NONE);
        }
        break;

    case IEEE80211_AUTH_SHARED:
    case IEEE80211_AUTH_AUTO:
        ASSERT(sup_wep);
        RSN_SET_UCAST_CIPHER(rsn, IEEE80211_CIPHER_WEP);
        RSN_SET_MCAST_CIPHER(rsn, IEEE80211_CIPHER_WEP);
        RSN_SET_MCASTMGMT_CIPHER(rsn, IEEE80211_CIPHER_NONE);
        break;

    case IEEE80211_AUTH_WPA:
    case IEEE80211_AUTH_RSNA:
    case IEEE80211_AUTH_CCKM:
        ASSERT(sup_tkip || sup_ccmp128 || sup_ccmp256 || sup_gcmp128 ||sup_gcmp256);
        /* only one default unicast cipher */

         if (sup_ccmp128){
            RSN_SET_UCAST_CIPHER(rsn, IEEE80211_CIPHER_AES_CCM);
            RSN_SET_MCAST_CIPHER(rsn, IEEE80211_CIPHER_AES_CCM);
            RSN_SET_MCASTMGMT_CIPHER(rsn, IEEE80211_CIPHER_AES_CMAC);
         } else if (sup_ccmp256){
            RSN_SET_UCAST_CIPHER(rsn, IEEE80211_CIPHER_AES_CCM_256);
            RSN_SET_MCAST_CIPHER(rsn, IEEE80211_CIPHER_AES_CCM_256);
            RSN_SET_MCASTMGMT_CIPHER(rsn, IEEE80211_CIPHER_AES_CMAC_256);
         } else if  (sup_gcmp128){
            RSN_SET_UCAST_CIPHER(rsn, IEEE80211_CIPHER_AES_GCM);
            RSN_SET_MCAST_CIPHER(rsn, IEEE80211_CIPHER_AES_GCM);
            RSN_SET_MCASTMGMT_CIPHER(rsn, IEEE80211_CIPHER_AES_GMAC);
         } else if (sup_gcmp256){
            RSN_SET_UCAST_CIPHER(rsn, IEEE80211_CIPHER_AES_GCM_256);
            RSN_SET_MCAST_CIPHER(rsn, IEEE80211_CIPHER_AES_GCM_256);
            RSN_SET_MCASTMGMT_CIPHER(rsn, IEEE80211_CIPHER_AES_GMAC_256);
         } else if (sup_tkip && vap->iv_opmode != IEEE80211_M_IBSS){
             RSN_SET_UCAST_CIPHER(rsn, IEEE80211_CIPHER_TKIP);
             RSN_SET_MCAST_CIPHER(rsn, IEEE80211_CIPHER_TKIP);
         }
        /*TKIP removed during integration*/
        if (vap->iv_opmode == IEEE80211_M_STA) {
            if (sup_wep)
                RSN_SET_MCAST_CIPHER(rsn, IEEE80211_CIPHER_WEP);
        }
	break;
#if ATH_SUPPORT_WAPI
    case IEEE80211_AUTH_WAPI:
        ASSERT(sup_sms4);
        RSN_SET_UCAST_CIPHER(rsn, IEEE80211_CIPHER_WAPI);
        RSN_SET_MCAST_CIPHER(rsn, IEEE80211_CIPHER_WAPI);
    break;
#endif /*ATH_SUPPORT_WAPI*/

	default:
	break;
    }
}

/*
 * Set default authentication mode
 */
static void
ieee80211_set_default_authmode(struct ieee80211vap *vap)
{
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    ieee80211_auth_mode m;
    int sup_tkip, sup_ccmp128, sup_ccmp256, sup_gcmp128, sup_gcmp256;
#if ATH_SUPPORT_WAPI
    int sup_sms4;
#endif

    sup_tkip = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_TKIP)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_TKIP);
    sup_ccmp128 = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_AES_CCM)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_AES_CCM);
    sup_ccmp256 = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_AES_CCM_256)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_AES_CCM_256);
    sup_gcmp128 = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_AES_GCM)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_AES_GCM);
    sup_gcmp256 = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_AES_GCM_256)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_AES_GCM_256);
#if ATH_SUPPORT_WAPI
    sup_sms4 = (vap->iv_caps & cipher2cap(IEEE80211_CIPHER_WAPI)) && ieee80211_crypto_available(vap->iv_ic, IEEE80211_CIPHER_WAPI);
#endif

    RSN_RESET_AUTHMODE(rsn);
    if (sup_ccmp128 || sup_ccmp256 || sup_gcmp128 ||sup_gcmp256 || sup_tkip) {
        if (vap->iv_opmode == IEEE80211_M_IBSS) {
            if (sup_ccmp128 || sup_ccmp256 || sup_gcmp128 ||sup_gcmp256) {
                m = IEEE80211_AUTH_RSNA;
                rsn->rsn_keymgmtset = WPA_ASE_8021X_PSK;
            } else {
                m = IEEE80211_AUTH_OPEN;
                rsn->rsn_keymgmtset = WPA_ASE_NONE;
            }
        } else {
            m = IEEE80211_AUTH_RSNA;
            rsn->rsn_keymgmtset = WPA_ASE_8021X_UNSPEC;
        }
    }
#if ATH_SUPPORT_WAPI
    else if (sup_sms4) {
        m = IEEE80211_AUTH_WAPI;
        rsn->rsn_keymgmtset = WAPI_ASE_WAI_AUTO;
    }
#endif
    else {
        m = IEEE80211_AUTH_OPEN;
        rsn->rsn_keymgmtset = WPA_ASE_NONE;
    }

    RSN_SET_AUTHMODE(rsn, m);
    ieee80211_set_default_ciphers(vap, m);
}

void
ieee80211_rsn_vattach(struct ieee80211vap *vap)
{
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;

    OS_MEMZERO(rsn, sizeof(*rsn));

    /* reset RSN to known state including auth mode set and cipher sets */
    ieee80211_rsn_reset(vap);

    /*
     * Default unicast cipher to WEP for 802.1x use.  If
     * WPA is enabled the management code will set these
     * values to reflect.
     */
    rsn->rsn_ucastkeylen = 104 / NBBY;

    rsn->rsn_mcastcipherset = rsn->rsn_ucastcipherset;
    rsn->rsn_mcastkeylen = 128 / NBBY;
}

void
ieee80211_rsn_reset(struct ieee80211vap *vap)
{
    /* delete global keys */
    ieee80211_crypto_delglobalkeys(vap);
    vap->iv_def_txkey = IEEE80211_DEFAULT_KEYIX;

    /* set default auth mode and cipher set */
    ieee80211_set_default_authmode(vap);

    /* clear privacy exemption list */
    OS_MEMZERO(vap->iv_privacy_filters,
               IEEE80211_MAX_PRIVACY_FILTERS * sizeof(ieee80211_privacy_exemption));
    vap->iv_num_privacy_filters = 0;

    /* clear PMKID list */
    OS_MEMZERO(vap->iv_pmkid_list,
               IEEE80211_MAX_PMKID * sizeof(ieee80211_pmkid_entry));
    vap->iv_pmkid_count = 0;

    /* accept unecrypted frame by default */
    IEEE80211_VAP_DROP_UNENC_DISABLE(vap);
    vap->vdev_mlme->mgmt.generic.drop_unencry = 0;
}

bool
ieee80211_match_rsn_info(
    wlan_if_t                     vap,
    struct ieee80211_rsnparms     *rsn_parms
    )
{
    struct ieee80211_rsnparms   *my_rsn = &vap->iv_rsn;

    /*
     * In adhoc mode, make sure there is exactly one pairwise cipher, one AKM in
     * the IE. Also, the RSN capability should match ours and there should be no PMKID.
     */

    printk("%s[%d] pairwise 0x%x, gtk 0x%x , keymgmt 0x%x \n",__func__, __LINE__,rsn_parms->rsn_ucastcipherset,rsn_parms->rsn_mcastcipherset,rsn_parms->rsn_keymgmtset);
    if (vap->iv_opmode == IEEE80211_M_IBSS) {
        if ((rsn_parms->rsn_ucastcipherset & (rsn_parms->rsn_ucastcipherset - 1)) != 0) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC, "%s",
                              " - Reject (AdHoc network does not specify exactly one pairwise cipher)\n");
            return false;
        }
        if ((rsn_parms->rsn_authmodeset & (rsn_parms->rsn_authmodeset - 1)) != 0) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC, "%s",
                              " - Reject (AdHoc network does not specify exactly one AKM)\n");
            return false;
        }

        /* comment out the checking, allow different capabilities in Adhoc mode.
        if (rsn_parms->rsn_caps != my_rsn->rsn_caps) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC, " - Reject (RSN capability mismatch)\n");
            return false;
        }
        */

    }

    /*
     * Check AKM suite list. At least one must match with our auth algorithm
     */
    if (!RSN_AUTH_MATCH(rsn_parms, my_rsn)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC, " - Reject (no matching AKM suite) 0x%x 0x%x \n",
				rsn_parms->rsn_authmodeset, my_rsn->rsn_authmodeset);
        return false;
    }

    /*
     * Check peer's pairwise ciphers. At least one must match with our unicast cipher
     */
    if (!RSN_UCAST_CIPHER_MATCH(rsn_parms, my_rsn)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC, " - Reject (no matching pairwise cipher) 0x%x 0x%x\n",
				rsn_parms->rsn_ucastcipherset, my_rsn->rsn_ucastcipherset);
        return false;
    }

    /*
     * Check peer's group cipher is our enabled multicast cipher.
     */
    if (!RSN_MCAST_CIPHER_MATCH(rsn_parms, my_rsn)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC, " - Reject (no matching group cipher)  0x%x 0x%x \n",
				rsn_parms->rsn_mcastcipherset, my_rsn->rsn_mcastcipherset);
        return false;
    }

    /*
     * Check peer's key management class set (PSK or UNSPEC)
     */
    if (!RSN_KEY_MGTSET_MATCH(rsn_parms, my_rsn)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ASSOC, " - Reject (no matching key mgtset) 0x%x 0x%x\n",
				rsn_parms->rsn_keymgmtset, my_rsn->rsn_keymgmtset);
        return false;
    }

    return true;
}

/*
 * External Interface for security module
 */
int
wlan_set_authmodes(wlan_if_t vaphandle, ieee80211_auth_mode modes[], u_int len)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    ieee80211_auth_mode m;
    int i;

    /* station can have only one authentication mode */
    if ((len > 1) &&
        (vap->iv_opmode == IEEE80211_M_STA ||
         vap->iv_opmode == IEEE80211_M_IBSS)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                          "%s: invalid authentcation type\n", __func__);
        return -EINVAL;
    }

    RSN_RESET_AUTHMODE(rsn);

    for (i = 0; i < len; i++) {
        m = modes[i];

        /*
         * IEEE80211_AUTH_NONE is deprecated.
         * Use the auth/cipher combination of IEEE80211_AUTH_OPEN and
         * IEEE80211_CIPHER_NONE instead.
         */
        if (m == IEEE80211_AUTH_NONE) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "%s: invalid authentcation type\n", __func__);
            return -EINVAL;
        }

        /*
         * IEEE80211_AUTH_AUTO really means combination of open
         * and shared key authentication modes.
         * NB: it is deprecated. We leave it just for binary compatibility
         * with hostapd.
         */
        if (m == IEEE80211_AUTH_AUTO) {
            RSN_SET_AUTHMODE(rsn, IEEE80211_AUTH_OPEN);
            RSN_SET_AUTHMODE(rsn, IEEE80211_AUTH_SHARED);
            ieee80211_set_default_ciphers(vap, m);
            return 0;
        }

        RSN_SET_AUTHMODE(rsn, m);
        ieee80211_set_default_ciphers(vap, m);
    }

    return 0;
}
#endif
int
wlan_get_auth_modes(wlan_if_t vaphandle, ieee80211_auth_mode modes[], u_int len)
{
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    ieee80211_auth_mode m;
    u_int32_t modeset;
    u_int count = 0;

    modeset = rsn->rsn_authmodeset;
    for (m = IEEE80211_AUTH_OPEN; m < IEEE80211_AUTH_MAX; m++) {
        if (RSN_HAS_AUTHMODE(rsn, m)) {
            /* Is input buffer big enough */
            if (len <= count)
                return -EINVAL;

            modes[count++] = m;
        }
    }

    return count;
#else
    return 0;
#endif
}

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
static INLINE int
check_auth_cipher(struct ieee80211_rsnparms *rsn, ieee80211_cipher_type cipher, int ismcast)
{
    if (RSN_AUTH_IS_OPEN(rsn)) {
        return (cipher == IEEE80211_CIPHER_WEP || cipher == IEEE80211_CIPHER_NONE);
    } else if (RSN_AUTH_IS_SHARED_KEY(rsn)) {
        return (cipher == IEEE80211_CIPHER_WEP);
    } else if (RSN_AUTH_IS_WPA(rsn) || RSN_AUTH_IS_RSNA(rsn)) {
        if (ismcast)
            return (cipher == IEEE80211_CIPHER_TKIP ||
                    cipher == IEEE80211_CIPHER_AES_CCM ||
                    cipher == IEEE80211_CIPHER_AES_CCM_256 ||
                    cipher == IEEE80211_CIPHER_AES_GCM ||
                    cipher == IEEE80211_CIPHER_AES_GCM_256 ||
                    cipher == IEEE80211_CIPHER_WEP);
        else
            return (cipher == IEEE80211_CIPHER_TKIP ||
                    cipher == IEEE80211_CIPHER_AES_CCM ||
                    cipher == IEEE80211_CIPHER_AES_CCM_256 ||
                    cipher == IEEE80211_CIPHER_AES_GCM ||
                    cipher == IEEE80211_CIPHER_AES_GCM_256
                   );
    } else if (RSN_AUTH_IS_CCKM(rsn)) {
        return (cipher == IEEE80211_CIPHER_TKIP ||
                cipher == IEEE80211_CIPHER_AES_CCM ||
                    cipher == IEEE80211_CIPHER_AES_CCM_256 ||
                    cipher == IEEE80211_CIPHER_AES_GCM ||
                    cipher == IEEE80211_CIPHER_AES_GCM_256 ||
                cipher == IEEE80211_CIPHER_WEP);
    }
#if ATH_SUPPORT_WAPI
    else if (RSN_AUTH_IS_WAI(rsn)) {
        return(cipher == IEEE80211_CIPHER_WAPI);
    }
#endif
    else {
        return 0;
    }
}
#endif
/*
 * NB: Atheros hw keep the cipher algo in the key cache, so we assoc the cipher
 * algo with each key as it is plumbed.  As a result, we do very little here.
 */
int
wlan_set_ucast_ciphers(wlan_if_t vaphandle, ieee80211_cipher_type types[], u_int len)
{
    struct ieee80211vap *vap = vaphandle;
    int i;
    uint32_t value = 0;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    ieee80211_cipher_type cipher;
#else
    wlan_crypto_cipher_type cipher;
#endif

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    RSN_RESET_UCAST_CIPHERS(rsn);

    for (i = 0; i < len; i++) {
        cipher = types[i];
        if (!check_auth_cipher(rsn, cipher, 0)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "%s: cipher doesn't match auth mode\n", __func__);
            return -EINVAL;
        }

        /* if any of the cipher is NONE, we treated as security being disabled */
        if (cipher == IEEE80211_CIPHER_NONE) {
            IEEE80211_VAP_PRIVACY_DISABLE(vap);
            return 0;
        }

        if (!(vap->iv_caps & cipher2cap(cipher)) ||
            !ieee80211_crypto_available(vap->iv_ic, cipher)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "%s: invalid cipher type\n", __func__);
            return -EINVAL;
        }
        RSN_SET_UCAST_CIPHER(rsn, cipher);
	value |= (1 << cipher);
    }
#else
    for (i = 0; i < len; i++) {
        cipher = (wlan_crypto_cipher_type)types[i];
        if (cipher == WLAN_CRYPTO_CIPHER_NONE) {
            return -EINVAL;
        }
        value |= (1 << cipher);
    }
    wlan_crypto_set_vdev_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_UCAST_CIPHER, value);
#endif
    IEEE80211_VAP_PRIVACY_ENABLE(vap);
    return 0;
}

int
wlan_set_mcast_ciphers(wlan_if_t vaphandle, ieee80211_cipher_type types[], u_int len)
{
    struct ieee80211vap *vap = vaphandle;
    int i;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    ieee80211_cipher_type cipher;
#else
    wlan_crypto_cipher_type cipher;
    uint32_t value = 0;
#endif
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    RSN_RESET_MCAST_CIPHERS(rsn);

    for (i = 0; i < len; i++) {
        cipher = types[i];
        if (!check_auth_cipher(rsn, cipher, 1)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "%s: cipher doesn't match auth mode\n", __func__);
            return -EINVAL;
        }

        if (cipher != IEEE80211_CIPHER_NONE) {
            if (!(vap->iv_caps & cipher2cap(cipher)) ||
                !ieee80211_crypto_available(vap->iv_ic, cipher)) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                                  "%s: invalid cipher type\n", __func__);
                return -EINVAL;
            }
        }

        RSN_SET_MCAST_CIPHER(rsn, cipher);
    }
#else
    for (i = 0; i < len; i++) {
        cipher = (wlan_crypto_cipher_type)types[i];
        if (cipher == WLAN_CRYPTO_CIPHER_NONE) {
            return -EINVAL;
        }
        value |= (1 << cipher);
    }

    wlan_crypto_set_vdev_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_MCAST_CIPHER, value);
#endif
    return 0;
}

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
int
wlan_get_ucast_ciphers(wlan_if_t vaphandle, ieee80211_cipher_type types[], u_int len)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    ieee80211_cipher_type cipher;
    u_int count = 0;

    for (cipher = IEEE80211_CIPHER_WEP; cipher < IEEE80211_CIPHER_MAX; cipher++) {
        if (RSN_HAS_UCAST_CIPHER(rsn, cipher)) {
            /* Is input buffer big enough */
            if (len <= count)
                return -EINVAL;

            types[count++] = cipher;
        }
    }

    return count;
}

int
wlan_get_mcast_ciphers(wlan_if_t vaphandle, ieee80211_cipher_type types[], u_int len)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;
    ieee80211_cipher_type cipher;
    /*
     * NB: certain OS (NDIS) requires us to return the array
     * in the order of priorities.
     */
    ieee80211_cipher_type mciphers[] = {
        IEEE80211_CIPHER_AES_CCM,
        IEEE80211_CIPHER_TKIP,
        IEEE80211_CIPHER_WEP,
#if ATH_SUPPORT_WAPI
        IEEE80211_CIPHER_WAPI,
#endif
        IEEE80211_CIPHER_NONE
    };
    u_int count = 0, i;

#define N(a)    (sizeof(a)/sizeof(a[0]))
    for (i = 0; i < N(mciphers); i++) {
        cipher = mciphers[i];
        if (RSN_HAS_MCAST_CIPHER(rsn, cipher)) {
            /* Is input buffer big enough */
            if (len <= count)
                return -EINVAL;
            types[count++] = cipher;
        }
    }

    return count;
}

ieee80211_cipher_type
ieee80211_get_current_mcastcipher(struct ieee80211vap *vap)
{
    struct ieee80211_rsnparms *rsn = &vap->iv_bss->ni_rsn;
    ieee80211_cipher_type cipher;

    for (cipher = IEEE80211_CIPHER_WEP; cipher < IEEE80211_CIPHER_MAX; cipher++) {
        if (RSN_HAS_MCAST_CIPHER(rsn, cipher))
            return cipher;
    }

    return IEEE80211_CIPHER_NONE;
}

ieee80211_cipher_type
wlan_get_current_mcast_cipher(wlan_if_t vaphandle)
{
    return ieee80211_get_current_mcastcipher(vaphandle);
}

static ieee80211_cipher_type
ieee80211_get_current_mcastmgmtcipher(struct ieee80211vap *vap)
{
    struct ieee80211_rsnparms *rsn = &vap->iv_bss->ni_rsn;
    ieee80211_cipher_type cipher;

    for (cipher = IEEE80211_CIPHER_WEP; cipher < IEEE80211_CIPHER_MAX; cipher++) {
        if (RSN_HAS_MCASTMGMT_CIPHER(rsn, cipher))
            return cipher;
    }

    return IEEE80211_CIPHER_NONE;
}
ieee80211_cipher_type
wlan_get_current_mcastmgmt_cipher(wlan_if_t vaphandle)
{
    return ieee80211_get_current_mcastmgmtcipher(vaphandle);
}
int
wlan_set_rsn_cipher_param(wlan_if_t vaphandle, ieee80211_rsn_param type, u_int32_t value)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;

    switch (type) {
    case IEEE80211_UCAST_CIPHER_LEN:
        rsn->rsn_ucastkeylen = value;
        break;

    case IEEE80211_MCAST_CIPHER_LEN:
        rsn->rsn_mcastkeylen = value;
        break;

    case IEEE80211_MCASTMGMT_CIPHER_LEN:
        rsn->rsn_mcastmgmtkeylen = value;
        break;

    case IEEE80211_KEYMGT_ALGS:
        rsn->rsn_keymgmtset = value;
        break;

    case IEEE80211_RSN_CAPS:
        rsn->rsn_caps = value;
        break;

    default:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                          "%s: unsupported rsn parameter\n", __func__);
        return -EINVAL;
    }

    return 0;
}
int32_t
wlan_get_rsn_cipher_param(wlan_if_t vaphandle, ieee80211_rsn_param type)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_rsnparms *rsn = &vap->iv_rsn;

    switch (type) {
    case IEEE80211_UCAST_CIPHER_LEN: return rsn->rsn_ucastkeylen;
    case IEEE80211_MCAST_CIPHER_LEN: return rsn->rsn_mcastkeylen;
    case IEEE80211_KEYMGT_ALGS: return rsn->rsn_keymgmtset;
    case IEEE80211_RSN_CAPS: return rsn->rsn_caps;
    default:
	break;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                      "%s: unsupported rsn parameter\n", __func__);
    return -EINVAL;
}
#endif

int
wlan_set_key(wlan_if_t vaphandle, u_int16_t keyix, ieee80211_keyval *key)
{
    struct ieee80211vap *vap = vaphandle;

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    struct wlan_crypto_req_key req_key;

    qdf_mem_zero(&req_key, sizeof(struct wlan_crypto_req_key));
    req_key.type   = key->keytype;
    if(vap->iv_opmode == IEEE80211_M_MONITOR) {
       req_key.flags  = IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV | IEEE80211_KEY_SWCRYPT;
       qdf_mem_copy(vap->mcast_encrypt_addr,key->macaddr,IEEE80211_ADDR_LEN);
    }
    else {
       req_key.flags  = IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV;
    }

    req_key.keylen = key->keylen;
    req_key.keyrsc = key->keyrsc;
    req_key.keytsc = key->keytsc;
    req_key.keyix  = keyix;

    if (key->keylen > (sizeof(req_key.keydata)))
        return -1;

    qdf_mem_copy(req_key.macaddr, key->macaddr, IEEE80211_ADDR_LEN);
    qdf_mem_copy(req_key.keydata, key->keydata, key->keylen);

    if ( req_key.type == IEEE80211_CIPHER_WEP ) {
        int32_t ucast_cipher;
        ucast_cipher = wlan_crypto_get_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_UCAST_CIPHER);
        if ( (ucast_cipher & (1<<WLAN_CRYPTO_CIPHER_NONE)) ) {
            wlan_crypto_set_vdev_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_MCAST_CIPHER,(1 << WLAN_CRYPTO_CIPHER_WEP));
            wlan_crypto_set_vdev_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_UCAST_CIPHER,(1 << WLAN_CRYPTO_CIPHER_WEP));
        }
    }
    return((int)wlan_crypto_setkey(vap->vdev_obj, &req_key));
#else
    if (keyix == IEEE80211_KEYIX_NONE)
        return ieee80211_set_keymapping_key(vap, key);
    else
        return ieee80211_set_default_key(vap, keyix, key);
#endif
}

int
wlan_set_default_keyid(wlan_if_t vaphandle, u_int keyix)
{
    struct ieee80211vap *vap = vaphandle;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211_key *k;
    int i;
    int invalid_wep_keyix=0;
#else
    uint8_t macaddr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
#endif
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                      "%s: Default keyID = %d\n", __func__, keyix);

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    if (vap->iv_opmode == IEEE80211_M_IBSS) {
        /*
         * In IBSS, if the default key alreay exists but now a new keyix is passed,
         * we still use the original one.
         */
        if (vap->iv_nw_keys[1].wk_valid)
        {
            keyix = 1;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "%s: Change default keyID to %d\n",
                              __func__, keyix);
        }
    }

    vap->iv_def_txkey = keyix;

    for (i = 0; i < IEEE80211_WEP_NKID; i++) {
        k = &vap->iv_nw_keys[i];
        if (k->wk_valid) {
            /* XXX Need to call crypto API to sync the key? */
            if (i == keyix) {
                k->wk_flags |= IEEE80211_KEY_XMIT;
            }
            else {
                k->wk_flags &= ~IEEE80211_KEY_XMIT;
                invalid_wep_keyix++;
            }
        }
        else {
            invalid_wep_keyix++;
        }
    }
    if ( invalid_wep_keyix  == IEEE80211_WEP_NKID )
        return -EINVAL;

    /* It sets the default transmit key id in the target
     * for offload architecture
     */
    wlan_set_param(vap,IEEE80211_DEFAULT_KEYID, keyix);
#else
    wlan_crypto_default_key(vap->vdev_obj, macaddr, keyix, 0);
#endif
    return 0;
}

u_int16_t
wlan_get_default_keyid(wlan_if_t vaphandle)
{
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211vap *vap = vaphandle;
    return vap->iv_def_txkey;
#else
        return IEEE80211_KEYIX_NONE;
#endif
}

int
wlan_get_key(wlan_if_t vaphandle, u_int16_t keyix, u_int8_t *macaddr, ieee80211_keyval *kval, u_int16_t keybuf_len)
{
    struct ieee80211vap *vap = vaphandle;
    int error = 0;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    struct wlan_crypto_req_key req_key;

    if(!kval->keydata)
        return -1;

    qdf_mem_zero((uint8_t*)&req_key, sizeof(req_key));
    req_key.keyix = keyix;

    error = wlan_crypto_getkey(vap->vdev_obj, &req_key, macaddr);
    if (error)
        return error;

    if (keybuf_len < req_key.keylen)
	return -1;
    kval->keytype  = req_key.type;
    kval->keydir   = (req_key.flags & (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV));
    kval->keylen   = req_key.keylen;
    kval->keyrsc   = req_key.keyrsc;
    kval->keytsc   = req_key.keytsc;
    kval->macaddr  = macaddr;
#if ATH_SUPPORT_WAPI
    qdf_mem_copy(kval->txiv, req_key.txiv, IEEE80211_WAPI_IV_SIZE);
    qdf_mem_copy(kval->recviv, req_key.recviv, IEEE80211_WAPI_IV_SIZE);
#endif
    qdf_mem_copy(kval->keydata, req_key.keydata, kval->keylen);
    return error;
#else
    struct ieee80211_key *k = NULL;
    struct ieee80211_node *ni = NULL;


    if (keyix == IEEE80211_KEYIX_NONE) {
        /*
         * Look for a key mapping key based on the mac address
         */
        ni = ieee80211_vap_find_node(vap, macaddr);
        if (ni == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                    "%s: can't find matching node", __func__);
            error = -ENOENT;
            goto bad;
        }
        k = &ni->ni_ucastkey;
    } else {
        if (keyix >= IEEE80211_WEP_NKID) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "%s: invalid key index = %d", __func__, keyix);
            error = -EINVAL;
            goto bad;
        }

        if (vap->iv_opmode == IEEE80211_M_IBSS && macaddr) {
            /* if it's a perSTA default key */
            ni = ieee80211_vap_find_node(vap, macaddr);
            if (ni == NULL) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                                  "%s: can't find matching node", __func__);
                error = -ENOENT;
                goto bad;
            }
            KASSERT((ni->ni_persta != NULL), ("ERROR: %s: ni->ni_persta is NULL\n", __func__));
            k = &ni->ni_persta->nips_swkey[keyix];
        } else {
            k = &vap->iv_nw_keys[keyix];
        }
    }

    /*
     * The ni might have changed before this del key has come from hostapd.
     * Check this using the associd and return
     */
    if (ni && (ni != vap->iv_bss) && (ni->ni_associd == 0)) {
        error = -EINVAL;
        goto bad;
    }

    if (!k || !k->wk_valid) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                          "%s: invalid key", __func__);
        error = -EINVAL;
        goto bad;
    }

    /* fill in key information */
    kval->keytype = k->wk_cipher->ic_cipher;
    switch (k->wk_flags & (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV)) {
    case IEEE80211_KEY_XMIT:
        kval->keydir = IEEE80211_KEY_DIR_TX;
        break;
    case IEEE80211_KEY_RECV:
        kval->keydir = IEEE80211_KEY_DIR_RX;
        break;
    case (IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV):
        kval->keydir = IEEE80211_KEY_DIR_BOTH;
        break;
    default:
        ASSERT(0);
    }

    kval->persistent = (k->wk_flags & IEEE80211_KEY_PERSISTENT) ? 1 : 0;
    kval->mfp = (k->wk_flags & IEEE80211_KEY_MFP) ? 1 : 0;
    kval->keylen = k->wk_keylen;
    kval->macaddr = macaddr;
    kval->keyrsc = k->wk_keyrsc[0]; /* which TID??? */
    kval->keytsc = k->wk_keytsc;

    if (keybuf_len < k->wk_keylen) {
        error = -EOVERFLOW;
        goto bad;
    }
    OS_MEMCPY(kval->keydata, k->wk_key, k->wk_keylen);

    if (kval->keytype == IEEE80211_CIPHER_TKIP) {
        if (keybuf_len < k->wk_keylen + IEEE80211_MICBUF_SIZE) {
            error = -EOVERFLOW;
            goto bad;
        }
        OS_MEMCPY(kval->keydata + kval->keylen, k->wk_key + k->wk_keylen, IEEE80211_MICBUF_SIZE);
        kval->txmic_offset = k->wk_txmic - k->wk_key;
        kval->rxmic_offset = k->wk_rxmic - k->wk_key;
    }
#if ATH_SUPPORT_WAPI
    if (kval->keytype == IEEE80211_CIPHER_WAPI) {
        if (keybuf_len < k->wk_keylen + IEEE80211_MICBUF_SIZE) {
            error = -EOVERFLOW;
            goto bad;
        }
        OS_MEMCPY(kval->keydata + kval->keylen, k->wk_key + k->wk_keylen, IEEE80211_MICBUF_SIZE);
        kval->txmic_offset = k->wk_txmic - k->wk_key;
        kval->rxmic_offset = k->wk_rxmic - k->wk_key;
    }

#endif
bad:
    if (ni != NULL)
        ieee80211_free_node(ni);
    return error;
#endif
}

int
wlan_del_key(wlan_if_t vaphandle, u_int16_t keyix, u_int8_t *macaddr)
{
    struct ieee80211vap *vap = vaphandle;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211_key *k = NULL;
    struct ieee80211_node *ni = NULL;
    int persta = 0;
    int error = 0;
    if (keyix == IEEE80211_KEYIX_NONE) {
        /*
         * Look for a key mapping key based on the mac address
         */
        ni = ieee80211_vap_find_node(vap, macaddr);
        if (ni == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "%s: can't find matching node", __func__);
            error = -EINVAL;
            goto bad;
        }

        k = &ni->ni_ucastkey;
    } else {
        if (keyix >= IEEE80211_WEP_NKID) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                              "%s: invalid key index = %d", __func__, keyix);
            error = -EINVAL;
            goto bad;
        }

        if (vap->iv_opmode == IEEE80211_M_IBSS &&
            (macaddr[0] != 0 || macaddr[1] != 0 ||
             macaddr[2] != 0 || macaddr[3] != 0 ||
             macaddr[4] != 0 || macaddr[5] != 0)) {
            /* if it's a perSTA default key */
            ni = ieee80211_vap_find_node(vap, macaddr);
            if (ni == NULL) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                                  "%s: can't find matching node", __func__);
                error = -EINVAL;
                goto bad;
            }
            KASSERT((ni->ni_persta != NULL), ("ERROR: %s: ni->ni_persta is NULL\n", __func__));
            persta = 1;
            k = &ni->ni_persta->nips_swkey[keyix];
        } else {
            k = &vap->iv_nw_keys[keyix];
        }
    }

    if (!k )  {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                          "%s: invalid key \n", __func__);
        error = -EINVAL;
        goto bad;
    }

    if (!k->wk_valid) {
        if ( (k->wk_keyix != IEEE80211_KEYIX_NONE && vap->iv_opmode == IEEE80211_M_STA) &&
                (ni && (ni->ni_explicit_compbf || ni->ni_explicit_noncompbf || ni->ni_implicit_bf))) {

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                    "%s:Delete Allocated TxBF key with keyix = %d macaddr:%s \n",
                    __func__,k->wk_keyix,ether_sprintf(macaddr));
            ieee80211_del_TxBF_keycache(vap,k,ni);
            goto bad;
        }
        error = -EINVAL;
        goto bad;
    }

    if (ni && ni->ni_flags & IEEE80211_NODE_DELAYED_CLEANUP) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO,
                          "%s: Node is in cleanup process %s\n", __func__,
                          ether_sprintf(macaddr));
        k->wk_valid = 0;
        goto bad;
    }

    /* Remove the key, could be either persta swkey or a static key or group rx key */
    if (ieee80211_crypto_delkey(vap, k, ni) == 0) {
        error = -EINVAL;
        goto bad;
    }

    if (vap->iv_opmode == IEEE80211_M_IBSS && persta) {
        int i;

        /* if all swkeys have been deleted, then remove the hwkey */
        for (i = 0; i < IEEE80211_WEP_NKID; i++) {
            if (ni->ni_persta->nips_swkey[i].wk_valid)
                break;
        }

        if (i == IEEE80211_WEP_NKID)
            ieee80211_crypto_delkey(vap, &ni->ni_persta->nips_hwkey, ni);
    }

bad:
    if (ni != NULL)
        ieee80211_free_node(ni);
    return error;
#else
    if (keyix == IEEE80211_KEYIX_NONE)
        keyix = 0;
    return wlan_crypto_delkey(vap->vdev_obj, macaddr, keyix);
#endif
}

int
ieee80211_del_key(wlan_if_t vaphandle, uint32_t authmode, u_int16_t keyix, u_int8_t *macaddr)
{
    struct ieee80211vap *vap = vaphandle;
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    if (authmode == IEEE80211_AUTH_AUTO) {
        /* Fix for CR#586870
         * If the auth mode is set to AUTO,
         * while switching between SHARED and OPEN
         * mode, the delkey should not happen;
         * If the delkey happens when the assoc goes
         * thru in OPEN mode, the ping will not pass.
         * This piece of code could have been put inside
         * wlan_del_key() function, but that could create
         * problems for other callers of that function.
         */
        ieee80211_auth_mode modes[IEEE80211_AUTH_MAX];
        uint8_t nmodes=0;
        nmodes=wlan_get_auth_modes(vap,modes,IEEE80211_AUTH_MAX);
        if (nmodes && ((modes[0] == IEEE80211_AUTH_SHARED)
                       || (modes[0] == IEEE80211_AUTH_OPEN))) {
            return 0;
        }
    }
#else
    authmode = wlan_crypto_get_param(vap->vdev_obj, WLAN_CRYPTO_PARAM_AUTH_MODE);
    if (authmode & (uint32_t)((1 << WLAN_CRYPTO_AUTH_OPEN) | (1 << WLAN_CRYPTO_AUTH_SHARED) | (1 << WLAN_CRYPTO_AUTH_SHARED))) {
        return 0;
    }
#endif

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,"DELETE CRYPTO KEY index %d, addr %s\n",
                    keyix, ether_sprintf(macaddr));
    if (keyix == KEYIX_INVALID) {
        return wlan_del_key(vap,IEEE80211_KEYIX_NONE,macaddr);
    } else {
        return wlan_del_key(vap,keyix,macaddr);
    }
}


int
wlan_set_privacy_filters(wlan_if_t vaphandle, ieee80211_privacy_exemption *filters, u_int32_t num_filters)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    int i;

    if (num_filters > IEEE80211_MAX_PRIVACY_FILTERS)
        return -EOVERFLOW;

    /* clear out old list first */
    OS_MEMZERO(vap->iv_privacy_filters,
               IEEE80211_MAX_PRIVACY_FILTERS * sizeof(ieee80211_privacy_exemption));

    for (i = 0; i < num_filters; i++) {
        vap->iv_privacy_filters[i] = filters[i];
    }

    vap->iv_num_privacy_filters = num_filters;

    wlan_vdev_set_privacy_filters(vap->vdev_obj, vap->iv_privacy_filters, vap->iv_num_privacy_filters);

    if (ic && ic->ic_set_privacy_filters) {
        ic->ic_set_privacy_filters(vap);
    }
    return 0;
}

int
wlan_get_privacy_filters(wlan_if_t vaphandle, ieee80211_privacy_exemption *filters, u_int32_t *num_filters, u_int32_t len)
{
    struct ieee80211vap *vap = vaphandle;
    int i;

    if (vap->iv_num_privacy_filters == 0) {
        *num_filters = 0;
        return 0;
    }

    /* check if the passed-in buffer has enough space to hold the entire list. */
    if (len < vap->iv_num_privacy_filters) {
        *num_filters = vap->iv_num_privacy_filters;
        return -EOVERFLOW;
    }

    for (i = 0; i < vap->iv_num_privacy_filters; i++) {
        filters[i] = vap->iv_privacy_filters[i];
    }
    *num_filters = vap->iv_num_privacy_filters;
    return 0;
}

int
wlan_node_authorize(wlan_if_t vaphandle, int authorize, u_int8_t *macaddr)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_node *ni=NULL;

    ni = ieee80211_vap_find_node(vap, macaddr);
    if (ni == NULL)
        return -EINVAL;


    if (authorize){
#if UNIFIED_SMARTANTENNA
        if (!ni->ni_ic->ic_is_mode_offload(ni->ni_ic)) { /* for direct attach */
            QDF_STATUS status = wlan_objmgr_peer_try_get_ref(ni->peer_obj, WLAN_SA_API_ID);
            if (QDF_IS_STATUS_ERROR(status)) {
                qdf_print("%s, %d unable to get reference", __func__, __LINE__);
            } else {
                wlan_sa_api_peer_connect(ni->ni_ic->ic_pdev_obj,ni->peer_obj, NULL);
                wlan_objmgr_peer_release_ref(ni->peer_obj, WLAN_SA_API_ID);
            }
        }
#endif

    /* SON events are not expected on the STA VAP,
     * Check if vap is not STA VAP, before notifiying events to SON
     */
    if (vap->iv_opmode == IEEE80211_M_HOSTAP || \
        vap->iv_opmode == IEEE80211_M_BTAMP || \
        vap->iv_opmode == IEEE80211_M_IBSS) {

#if QCA_SUPPORT_SON
        son_send_node_associated_event(vap->vdev_obj, ni->peer_obj);
#endif

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        /* send node authorized event */
        ald_assoc_notify(vap, macaddr, ALD_ACTION_ASSOC);
#endif

#if ATH_PARAMETER_API
        ieee80211_papi_send_assoc_event(vap, ni, PAPI_STA_ASSOCIATION);
#endif
    }

        ieee80211_node_authorize(ni);
    }
    else {
        ieee80211_node_unauthorize(ni);
    }

    ieee80211_free_node(ni);
    return 0;
}

int
wlan_set_pmkid_list(wlan_if_t vaphandle, ieee80211_pmkid_entry *pmkids, u_int16_t num)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_aplist_config *pconfig = ieee80211_vap_get_aplist_config(vap);
    u_int16_t npmkid = 0;
    int i, j;

    if (num > IEEE80211_MAX_PMKID || num < 1)
        return -EOVERFLOW;

    /* clear out old list first */
    OS_MEMZERO(vap->iv_pmkid_list,
               IEEE80211_MAX_PMKID * sizeof(ieee80211_pmkid_entry));

    for (i = 0; i < num; i++) {
        /*
         * Make sure all BSSID specified in the list are in our desired BSSID list.
         */
        if (!ieee80211_aplist_get_accept_any_bssid(pconfig)) {
            for (j = 0; j < ieee80211_aplist_get_desired_bssid_count(pconfig); j++) {
                u_int8_t *bssid = NULL;

                ieee80211_aplist_get_desired_bssid(pconfig, j, &bssid);

                if ((bssid != NULL) && (IEEE80211_ADDR_EQ(pmkids[i].bssid, bssid)))
                    break;
            }

            if (j == ieee80211_aplist_get_desired_bssid_count(pconfig))
                continue;       /* doesn't match any desired BSSID */
        }

        vap->iv_pmkid_list[npmkid++] = pmkids[i];
    }

    vap->iv_pmkid_count = npmkid;
    return 0;
}

int
wlan_get_pmkid_list(wlan_if_t vaphandle, ieee80211_pmkid_entry *pmkids, u_int16_t *count, u_int16_t len)
{
    struct ieee80211vap *vap = vaphandle;
    int i;

    if (vap->iv_pmkid_count == 0) {
        *count = 0;
        return 0;
    }

    /* check if the passed-in buffer has enough space to hold the entire list. */
    if (len < vap->iv_pmkid_count) {
        *count = vap->iv_pmkid_count;
        return -EOVERFLOW;
    }

    for (i = 0; i < vap->iv_pmkid_count; i++) {
        pmkids[i] = vap->iv_pmkid_list[i];
    }
    *count = vap->iv_pmkid_count;
    return 0;
}
