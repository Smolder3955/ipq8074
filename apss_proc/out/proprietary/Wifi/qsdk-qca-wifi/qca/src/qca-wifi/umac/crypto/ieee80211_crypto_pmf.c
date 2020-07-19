/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 *
 * cmac mic calculation function is copied from open source wireless-testing
 * project net/mac80211 tree.
 *
 */

/*
 * IEEE 802.11w PMF crypto support.
 */
#include <ieee80211_crypto_pmf_priv.h>
#include "aes_gcm.h"
bool
ieee80211_is_pmf_enabled(struct ieee80211vap *vap,struct ieee80211_node *ni)
{

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    bool status;
    status = wlan_crypto_is_pmf_enabled(vap->vdev_obj, ni->peer_obj);
    return status;
#else
 return ((vap->iv_rsn.rsn_caps & RSN_CAP_MFP_ENABLED) &&
            (ni->ni_rsn.rsn_caps & RSN_CAP_MFP_ENABLED)) || 
            (vap->iv_rsn.rsn_caps & RSN_CAP_MFP_REQUIRED);
#endif
}

bool
wlan_vap_is_pmf_enabled(wlan_if_t vaphandle)
{
    struct ieee80211vap      *vap = vaphandle;
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
    bool status;
    status = wlan_crypto_vdev_is_pmf_enabled(vap->vdev_obj);
    return status;
#else
    if (vap->iv_rsn.rsn_caps & RSN_CAP_MFP_ENABLED) {
        return TRUE;
    } else {
        return FALSE;
    }
#endif
}

bool ieee80211_is_pmf_frame(qdf_nbuf_t wbuf)
{
    int type = -1, subtype = -1;
    struct ieee80211_frame *wh;

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

    if((type == IEEE80211_FC0_TYPE_MGT)
          && ((subtype == IEEE80211_FC0_SUBTYPE_DEAUTH)
             || (subtype == IEEE80211_FC0_SUBTYPE_DISASSOC)
             || ((subtype == IEEE80211_FC0_SUBTYPE_ACTION) &&
             ieee80211_is_robust_action_frame(*((uint8_t *)(wh+1)))))) {
          return true;
   }
   return false;
}

void
wlan_crypto_set_hwmfpQos(struct ieee80211vap *vap, u_int32_t dot11w)
{
    struct ieee80211com *ic = vap->iv_ic;
    ic->ic_set_hwmfpQos(ic, dot11w);
}

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
int
ieee80211_set_igtk_key(struct ieee80211vap *vap, u_int16_t keyix, ieee80211_keyval *kval)
{
    struct ieee80211_key      *k = &vap->iv_igtk_key;
    
    if (kval->keylen > IEEE80211_KEYBUF_SIZE)
        return -EINVAL;

    /*
     * Update the key entry for valid key. We cannot fail after this point. Otherwise, 
     * the existing key will be modified when we fail to set the new key.
     */
    IEEE80211_CRYPTO_KEY_LOCK_INIT(k);
    k->wk_keylen = kval->keylen;
    k->wk_keyix = keyix;

    OS_MEMCPY(k->wk_key, kval->keydata, kval->keylen);
    OS_MEMZERO(k->wk_key + kval->keylen, (sizeof(k->wk_key) - kval->keylen));
    k->wk_keyrsc[0] = kval->keyrsc;
    k->wk_keytsc = kval->keytsc;

    if (k->wk_private == NULL) {
        k->wk_private = OS_MALLOC(vap->iv_ic->ic_osdev, sizeof(rijndael_ctx), GFP_KERNEL);
        if (k->wk_private == NULL) {
            IEEE80211_CRYPTO_KEY_LOCK_DESTROY(k);
            // Failed to alloc mem
            return 1;
        }
    }

    rijndael_set_key((rijndael_ctx *)k->wk_private, k->wk_key, k->wk_keylen*NBBY);
    return 0;
}

static void gf_mulx(u8 *pad)
{
    int i, carry;

    carry = pad[0] & 0x80;
    for (i = 0; i < AES_BLOCK_SIZE - 1; i++){
        pad[i] = (pad[i] << 1) | (pad[i + 1] >> 7);
    }
    pad[AES_BLOCK_SIZE - 1] <<= 1;
    if (carry){
        pad[AES_BLOCK_SIZE - 1] ^= 0x87;
    }
}

int
ieee80211_cmac_calc_mic(struct ieee80211_key *key, u_int8_t *aad, u_int8_t *pkt, u_int32_t pktlen , u_int8_t *mic)
{
    const u_int8_t *addr[3];
    u_int32_t len[3];
    u_int8_t zero[CMAC_TLEN];
    u_int8_t cbc[AES_BLOCK_SIZE], pad[AES_BLOCK_SIZE];
    const u_int8_t *pos, *end;
    u_int32_t i, e, left, total_len;

    OS_MEMSET(zero, 0, CMAC_TLEN);
    addr[0] = aad;
    len[0] = AAD_LEN;
    addr[1] = pkt;
    len[1] = pktlen - CMAC_TLEN;
    addr[2] = zero;
    len[2] = CMAC_TLEN;

    //aes_128_cmac_vector(tfm, scratch, 3, addr, len, mic);

    OS_MEMSET(cbc, 0, AES_BLOCK_SIZE);

    total_len = 0;
    for (e = 0; e < 3; e++)
        total_len += len[e];
    left = total_len;

    e = 0;
    pos = addr[0];
    end = pos + len[0];

    while (left >= AES_BLOCK_SIZE) {
        for (i = 0; i < AES_BLOCK_SIZE; i++) {
            cbc[i] ^= *pos++;
            if (pos >= end) {
                e++;
                pos = addr[e];
                end = pos + len[e];
            }
        }
        if (left > AES_BLOCK_SIZE){
            rijndael_encrypt((rijndael_ctx *)key->wk_private, cbc, cbc);
        }
        left -= AES_BLOCK_SIZE;
    }

    OS_MEMSET(pad, 0, AES_BLOCK_SIZE);
    rijndael_encrypt((rijndael_ctx *)key->wk_private, pad, pad);
    gf_mulx(pad);

    if (left || total_len == 0) {
        for (i = 0; i < left; i++) {
            cbc[i] ^= *pos++;
            if (pos >= end) {
                e++;
                pos = addr[e];
                end = pos + len[e];
            }
        }
        cbc[left] ^= 0x80;
        gf_mulx(pad);
    }

    for (i = 0; i < AES_BLOCK_SIZE; i++){
        pad[i] ^= cbc[i];
    }
    rijndael_encrypt((rijndael_ctx *)key->wk_private, pad, pad);
    OS_MEMCPY(mic, pad, CMAC_TLEN);

    return 0;
}

int
ieee80211_gmac_calc_mic(struct ieee80211_key *key, u_int8_t *aad, u_int8_t *pkt,
                                u_int32_t aadlen , u_int8_t *mic, u_int8_t *nonce)
{
    if (aes_gmac(key, nonce, 12, aad, aadlen, mic) < 0) {
        printk("aes_gmac :: %s[%d] \n",__func__,__LINE__);
    }
    return 0;
}
#endif
