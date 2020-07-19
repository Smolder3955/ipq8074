/*
 * Copyright (c) 2013,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
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

#include <osdep.h>
#include <ieee80211_var.h>

#if ATH_SUPPORT_WEP_MBSSID

/*
 * Clean up the resources in a WEP node if WEP BSSID is 
 * used. 
 */
void
wep_mbssid_node_cleanup(struct ieee80211_node *ni)
{
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211_key key;

	/* 
	 * Create a temporary key and use it for deletion 
	 * create the key from the appropriate vap key.
	 */
    if (ni->ni_wep_mbssid.rxvapkey) {
	  memcpy(&key, ni->ni_wep_mbssid.rxvapkey, sizeof(key));
	  key.wk_keyix = ni->ni_rxkeyoff;
	  ni->ni_wep_mbssid.rxvapkey = NULL;
          ni->ni_rxkeyoff = 0;
	  ieee80211_crypto_delkey(ni->ni_vap, &key, ni);
    } 

    if (ni->ni_wep_mbssid.mcastkey_idx) {
        ni->ni_wep_mbssid.mcastkey.wk_keyix = ni->ni_wep_mbssid.mcastkey_idx;
        ieee80211_crypto_delkey(ni->ni_vap, &ni->ni_wep_mbssid.mcastkey, ni);
        ni->ni_wep_mbssid.mcastkey_idx = 0;
    }
#endif
}

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
static int crypto_installkey(struct ieee80211_key *key, struct ieee80211vap *vap, 
					  struct ieee80211_node *ni)
{
    /* If a key cache slot is already allocated 
     * for this node use that, otherwise allocate
     * a new one.
     */
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: ni_rxkeyoff:%d, wk_keyidx:%d\n", 
	    __func__, ni->ni_rxkeyoff, key->wk_keyix);

    if (ni->ni_rxkeyoff)
        key->wk_keyix = ni->ni_rxkeyoff;
    else
        key->wk_keyix = IEEE80211_KEYIX_NONE;

    if(ieee80211_crypto_newkey(vap, IEEE80211_CIPHER_WEP,
                IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV, key)) {
        if (!ieee80211_crypto_setkey(vap, key, ni->ni_macaddr, ni)) {
		  IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: Couldn't set key, fatal.\n",
							__func__);
            return 0;
        }
        else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: Keyset successful on entry %d.\n",
							  __func__, key->wk_keyix);
            ni->ni_rxkeyoff = key->wk_keyix;
        }
    }
    return 1;
}
 
static int crypto_install_mcastkey(struct ieee80211_key *key, struct ieee80211vap *vap, 
                      struct ieee80211_node *ni)
{
    struct ieee80211_key *mc_key = &ni->ni_wep_mbssid.mcastkey;
    u_int8_t flags = /*IEEE80211_KEY_XMIT | IEEE80211_KEY_RECV | */IEEE80211_KEY_GROUP;

    /* If a key cache slot is already allocated 
     * for this node use that, otherwise allocate
     * a new one.
     */
    mc_key->wk_keylen = key->wk_keylen;
    memset(&mc_key->wk_key, 0, sizeof(mc_key->wk_key));
    memcpy(&mc_key->wk_key, key->wk_key, key->wk_keylen);
    mc_key->wk_flags = flags;
    mc_key->wk_cipher = key->wk_cipher;

    if (ni->ni_wep_mbssid.mcastkey_idx)
        mc_key->wk_keyix = ni->ni_wep_mbssid.mcastkey_idx;
    else
        mc_key->wk_keyix = IEEE80211_KEYIX_NONE;

    if(ieee80211_crypto_newkey(vap, IEEE80211_CIPHER_WEP, flags, mc_key)) {
        if (!ieee80211_crypto_setkey(vap, mc_key, ni->ni_macaddr, ni)) {
          IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: Couldn't set MCast key, fatal.\n",
                            __func__);
            return 0;
        }
        else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: MCast Keyset successful on entry %d.\n",
                              __func__, mc_key->wk_keyix);
            ni->ni_wep_mbssid.mcastkey_idx = mc_key->wk_keyix;
        }
    }

    return 1;
}

/*
 * Install a keycache entry
 * return 0 on failure and 1 on success.
 * Have to set the IEEE80211_KEY_SWDECRYPT before decap 
 * as decap is done only if the flag is set.
 * The caller should handle locking using 
 * ieee80211_key_update_begin(vap);
 * ieee80211_key_update_end(vap);
 */ 

static int ieee80211_crypto_keymiss(struct ieee80211_node *ni, wbuf_t wbuf, struct ieee80211_rx_status *rs)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_frame *wh;
    int off, kid, hdrspace;
    u_int8_t *buf = NULL;
    struct ieee80211_key k, *key = NULL; 
    const struct ieee80211_cipher *cip;
    struct ieee80211_node_table *nt = &ic->ic_sta;
    struct ieee80211_node *sender=NULL;

    /* 
     * Verify if WEP is set and
     * retrieve the key index from the packet.
     */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    buf = (u_int8_t*)wbuf_raw_data(wbuf);

    if (wh->i_fc[1] & IEEE80211_FC1_WEP) {

        off = ieee80211_anyhdrspace(ic->ic_pdev_obj, wh);
        kid = buf[off+IEEE80211_WEP_IVLEN] >> 6;

        sender = ieee80211_find_node(nt, wh->i_addr2);
        if(sender == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: Node not found\n",
                                                __func__);
            goto bad; 
        }

        /* 
         * Using the key index specified in the packet.
         */
        if (kid >= IEEE80211_WEP_NKID) {
	    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: Incorrect keyid (%d) specified in the packet!\n",
                                             __func__, kid);
            goto bad;
        }
        key = &vap->iv_nw_keys[kid];
        cip = key->wk_cipher;
        if (cip->ic_cipher != IEEE80211_CIPHER_WEP) {
            ieee80211_free_node(sender);
            return 1;
        }
        hdrspace = ieee80211_hdrspace(ic, wh);

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: kid=%d, ni=0x%pK, sender=0x%pK, vap=0x%pK\n",
                                             __func__, kid, ni, sender, vap);
        /*
         * Create a temporary key for installing the
         * rx key for the station.
         */
        OS_MEMCPY(&k, key, sizeof(*key));
        k.wk_flags |= IEEE80211_KEY_SWDECRYPT;

        if (cip->ic_decap(&k, wbuf, hdrspace, rs) ) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "Decrypt using entry(s) %d worked.\n",
                                                 key->wk_keyix);
            wh = (struct ieee80211_frame *)wbuf_header(wbuf);
            /*
             * The packet has been decrypted correctly, therefore the WEP bit 
             * should be cleared.
             */
            wh->i_fc[1] &= ~IEEE80211_FC1_WEP;
            k.wk_flags &= ~IEEE80211_KEY_SWDECRYPT;

            if (!crypto_installkey(&k, vap, sender))
                goto bad; 
            sender->ni_wep_mbssid.rxvapkey = key;
	    
            if(vap->iv_opmode == IEEE80211_M_STA) {
                if (!crypto_install_mcastkey(&k, vap, sender))
                    goto bad;
            }
            ieee80211_free_node(sender);
            key->wk_private = k.wk_private;
            return 1;
        } else 
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "Decrypt using entry(s) %d didn't work.\n", 
                                                  key->wk_keyix);
    } /* if wep is enabled */
bad:
    if (sender)
        ieee80211_free_node(sender);
    return 0;
}

int ieee80211_crypto_handle_keymiss(struct wlan_objmgr_pdev *pdev,
                                    wbuf_t wbuf,
                                    struct ieee80211_rx_status *rs)
{
    struct ieee80211_node *ni = NULL;
    struct ieee80211com *ic = NULL;

    ic = wlan_pdev_get_mlme_ext_obj(pdev);

    /*   
     * Handle packets with keycache miss
     */
    if ((rs->rs_flags & IEEE80211_RX_KEYMISS)) {
        ni = ieee80211_find_rxnode(ic,
                (const struct ieee80211_frame_min *) wbuf_header(wbuf));

        if( ni != NULL) {
            struct ieee80211vap *vap = ni->ni_vap;

            ieee80211_key_update_begin(vap);
            if (!ieee80211_crypto_keymiss(ni, wbuf, rs)) {
	            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CRYPTO, "%s: Couldn't decrypt, dropping packet.\n",
                                             __func__);
                wbuf_free(wbuf);
                ieee80211_key_update_end(vap);
                ieee80211_free_node(ni); 	
                return 1;
            }else {
                ieee80211_key_update_end(vap);
                ieee80211_free_node(ni);
            }
        }
    }
	return 0;
}

#ifdef WEP_DUMP_DEBUG
static void dump_hex_buf(unsigned char *buf, int len)
{
	int idx;
	for(idx=0;idx<len;idx++) {
		printk("%02x ", buf[idx]);
		if (((idx+1)%16)==0) 
			printk("\n");

	}
	printk("\n");
}

void
wep_dump(struct ieee80211_key *k, wbuf_t wbuf, int hdrlen)
{
	struct ieee80211_frame *wh;
    	struct wep_ctx *ctx = k->wk_private;
    	struct ieee80211vap *vap = ctx->wc_vap;
	unsigned int iv,idx,icv;
	unsigned char *ptr;
	unsigned char kbuf[64];


	wh = (struct ieee80211_frame *)wbuf_header(wbuf);
	ptr = (unsigned char*)wh;       

	idx = iv = 0;
	memcpy(&iv, ptr+hdrlen, 3);
	idx = ptr[hdrlen+3];
	memcpy(&icv, ptr+wbuf_get_pktlen(wbuf)-4, 4);

	memcpy(kbuf, k->wk_key, k->wk_keylen);
 
	IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr1,
            "%s and seq=%02x-%02x\n", "addr1", wh->i_seq[0], wh->i_seq[1]);
	IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr3,
            "%s", "addr3");
	IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr2,
            "IV=%08x idx=%d ICV=%08x, hdrlen=%d",iv, idx&0xff, icv, hdrlen);

	printk("key dump:len=%d\n", k->wk_keylen);
	dump_hex_buf(kbuf, (int)k->wk_keylen);
	printk("packet dump:pktlen=%d,len=%d\n", wbuf_get_pktlen(wbuf), wbuf_get_len(wbuf));
	dump_hex_buf((uint8_t*)ptr/*wbuf_raw_data(wbuf)*/, (int)wbuf_get_pktlen(wbuf));
}
#endif
#endif

int ieee80211_wep_mbssid_cipher_check(struct ieee80211_key *k)
{
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    /*
     * WEP slots are alloc'd from 4 when WEP MBSSID is enabled.
     */
    return (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_WEP);
#endif
}

int ieee80211_wep_mbssid_mac(struct ieee80211vap *vap,
		                     const struct ieee80211_key *k,
                             u_int8_t *gmac)
{
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    if (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_WEP) {
        if (k->wk_flags & IEEE80211_KEY_RECV) {
            /*
             * This is for the WEP tx key by "iwconfig" or WEP rx key that is installed
             * by crypto_installkey().
             */
            IEEE80211_ADDR_COPY(gmac, vap->iv_myaddr);
        } else {
            /*
             * For _M_STA, set the multicast key that is installed by
             * crypto_install_mcastkey() for decypting the received
             * multicast frames from root AP.
             */
            IEEE80211_ADDR_COPY(gmac, vap->iv_bss->ni_macaddr);
            gmac[0] |= 0x01;
        }
        return 1;
    }
    else {
        /*
         * If not WEP mode, no changes.
         */
         return 0;
    }
#endif
}
#endif /*ATH_SUPPORT_WEP_MBSSID*/
