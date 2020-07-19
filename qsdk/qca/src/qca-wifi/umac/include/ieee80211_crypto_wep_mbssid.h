/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2005 Atheros Communications Inc.  All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef IEEE80211_CRYPTO_WEP_MBSSID_H
#define IEEE80211_CRYPTO_WEP_MBSSID_H

#include <ieee80211_crypto.h>

#if ATH_SUPPORT_WEP_MBSSID
struct ieee80211_wep_mbssid {  
    struct ieee80211_key   *rxvapkey;   /* vap crypto key for the receive entry */
    struct ieee80211_key    mcastkey;    /* multicast key */
    int                     mcastkey_idx;    /* Receive multicast key index */
};

#define ieee80211_crypto_wep_mbssid_enabled(void) (1)
#define wep_dump(_k, _wbuf, _hdrlen)
void wep_mbssid_node_cleanup(struct ieee80211_node *ni);
int ieee80211_crypto_handle_keymiss(struct wlan_objmgr_pdev *pdev, wbuf_t wbuf, struct ieee80211_rx_status *rs);
int ieee80211_wep_mbssid_cipher_check(struct ieee80211_key *k);
int ieee80211_wep_mbssid_mac(struct ieee80211vap *vap, const struct ieee80211_key *k, u_int8_t *gmac);
#else
#define ieee80211_crypto_wep_mbssid_enabled(void) (0)
#define wep_mbssid_node_cleanup(_ni)
#define ieee80211_crypto_handle_keymiss(_pdev, _wbuf, _rs) (0)
#define wep_dump(_k, _wbuf, _hdrlen)
#define ieee80211_wep_mbssid_cipher_check(_k) (0)
#define ieee80211_wep_mbssid_mac(_vap, _k, _gmac) (0)
#endif /*ATH_SUPPORT_WEP_MBSSID*/
#endif /*IEEE80211_CRYPTO_WEP_MBSSID_H*/
