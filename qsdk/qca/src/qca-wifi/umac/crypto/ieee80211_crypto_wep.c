/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 *
 *
 */
#include <osdep.h>

#include <ieee80211_var.h>

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

static	void *wep_attach(struct ieee80211vap *, struct ieee80211_key *);
static	void wep_detach(struct ieee80211_key *);
static	int wep_setkey(struct ieee80211_key *);
static	int wep_encap(struct ieee80211_key *, wbuf_t, u_int8_t keyid);
static	int wep_decap(struct ieee80211_key *, wbuf_t, int, struct ieee80211_rx_status *);
static	int wep_enmic(struct ieee80211_key *, wbuf_t, int, bool);
static	int wep_demic(struct ieee80211_key *, wbuf_t, int, int, struct ieee80211_rx_status *);

static const struct ieee80211_cipher wep = {
    "WEP",
    IEEE80211_CIPHER_WEP,
    IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN,
    IEEE80211_WEP_CRCLEN,
    0,
    wep_attach,
    wep_detach,
    wep_setkey,
    wep_encap,
    wep_decap,
    wep_enmic,
    wep_demic,
};

#if UMAC_SUPPORT_TKIP_SW_CRYPTO
extern void wep_encrypt(u8 *key, wbuf_t wbuf0, u_int off, u_int16_t data_len);
#endif
static int __wep_encrypt(struct ieee80211_key *key, wbuf_t wbuf, int hdrlen);

static	int wep_decrypt(struct ieee80211_key *, wbuf_t, int hdrlen);

struct wep_ctx {
    struct ieee80211vap *wc_vap;	/* for diagnostics + statistics */
    struct ieee80211com *wc_ic;	/* for diagnostics */
    u_int32_t	wc_iv;		/* initial vector for crypto */
};

static void *
wep_attach(struct ieee80211vap *vap, struct ieee80211_key *k)
{
    struct wep_ctx *ctx;
    struct ieee80211com *ic = vap->iv_ic;

    ctx = (struct wep_ctx *)OS_MALLOC(ic->ic_osdev, sizeof(struct wep_ctx), GFP_KERNEL);
    if (ctx == NULL) {
#ifdef QCA_SUPPORT_CP_STATS
        vdev_cp_stats_crypto_nomem_inc(vap->vdev_obj, 1);
#endif
        return NULL;
    }

    ctx->wc_vap = vap;
    ctx->wc_ic = vap->iv_ic;
    ctx->wc_iv = 0;
    return ctx;
}

static void
wep_detach(struct ieee80211_key *k)
{
    struct wep_ctx *ctx = k->wk_private;
    OS_FREE(ctx);
}

/*
 * findWeakWep - determine which key byte an iv is useful in resolving
 * parm     - p, pointer to the first byte of an IV
 * return   -  n - this IV is weak for byte n of a WEP key
 *            -1 - this IV is not weak for any key bytes
 *
 * This function tests for IVs that are known to satisfy the criteria
 * for a weak IV as specified in FMS section 7.1
 *
 * It also takes care of the following old check in frameIVSetup:
 *   Filter keys that are weak according to "Weaknesses in the
 *   Key Scheduling Algorithm of RC4" by Scott Fluhrer, Itsik
 *   Martin and Adi Shimir.  We do not filter other "weak"
 *   keys, so this is only a patch.
 *
 *   Form: any, 0xff, 0x3-0x8, 0xf, 0x13 (40, 104, 128b keys)
 *
 */
static int8_t 
findWeakWep(uint8_t *p) 
{
    uint8_t sum, k;

    //test for the FMS (A+3, N-1, X) form of IV
    if (p[1] == 255 && p[0] > 2 && p[0] < 16) {
       return p[0] - 3;
    }

    //test for other IVs for which it is known that
    // Si[1] < i and (Si[1] + Si[Si[1]]) = i + n  (see FMS 7.1)
    sum = p[0] + p[1];
    if (sum == 1) {
        if (p[2] <= 0x0A) {
            return p[2] + 2;
        }
        else if (p[2] == 0xFF) {
            return 0;
        }
    }
    k = 0xFE - p[2];
    if (sum == k && (p[2] >= 0xF2 && p[2] <= 0xFE && p[2] != 0xFD)) {
        return k;
    }
    return -1;
}

static int
wep_setkey(struct ieee80211_key *k)
{
    return (k->wk_keylen >= (40/NBBY));
}

#ifndef _BYTE_ORDER
#error "Don't know native byte order"
#endif

/*
 * Add privacy headers appropriate for the specified key.
 */
static int
wep_encap(struct ieee80211_key *k, wbuf_t wbuf, u_int8_t keyid)
{
    struct wep_ctx *ctx = k->wk_private;
    struct ieee80211com *ic = ctx->wc_ic;
    #if UMAC_SUPPORT_TKIP_SW_CRYPTO
    struct ieee80211vap *vap = ctx->wc_vap;
    struct ieee80211_frame *wh;
    #endif
    u_int32_t iv;
    u_int8_t *ivp, *origHdr = (u_int8_t*)wbuf_header(wbuf);
    int hdrlen;
    int skipCount = 0;
    int ivMask;

    if (vap->iv_opmode == IEEE80211_M_MONITOR) {
        /*
          For offchan encrypted TX, key's saved in mon vap.
          For 3-addr Qos frame(26 bytes) encryption, don't do 4-byte alignment,
          otherwise, the dot11 header in OTA pkts will have 2 more bytes, which's from LLC
        */
        hdrlen = ieee80211_hdrsize(origHdr);
    }else{
        hdrlen = ieee80211_hdrspace(ic, origHdr);
    }
    /*
     * Copy down 802.11 header and add the IV + KeyID.
     */
#ifndef QCA_PARTNER_PLATFORM
    ivp = (u_int8_t*)wbuf_push(wbuf, wep.ic_header);
    memmove(ivp, ivp + wep.ic_header, hdrlen);
#else
    if (wbuf_is_encap_done(wbuf)) {
        ivp = (u_int8_t*)wbuf_header(wbuf);
    } else {
        ivp = (u_int8_t*)wbuf_push(wbuf, wep.ic_header);
        memmove(ivp, ivp + wep.ic_header, hdrlen);
    }
#endif
    ivp += hdrlen;

    /*
     * XXX
     * IV must not duplicate during the lifetime of the key.
     * But no mechanism to renew keys is defined in IEEE 802.11
     * for WEP.  And the IV may be duplicated at other stations
     * because the session key itself is shared.  So we use a
     * pseudo random IV for now, though it is not the right way.
     *
     * NB: Rather than use a strictly random IV we 
     * increment the value for
     * each frame.  This is an explicit tradeoff between
     * overhead and security.  Given the basic insecurity of
     * WEP this seems worthwhile.
     */


    /*
     * Skip 'bad' IVs from Fluhrer/Mantin/Shamir:
     * (B, 255, N) with 3 <= B < 16 and 0 <= N <= 255
     * findWeakWep() will take care of those and other forms of weak IVs
     */
    iv = ctx->wc_iv;
 
    ivMask = 0x00ffffff;   /* 24 bit IV */

    while ((-1 != findWeakWep((unsigned char *) &iv)) &&
           (skipCount++ < ivMask))
    {
        iv++;
    }
    ctx->wc_iv = iv + 1;

    /*
     * NB: Preserve byte order of IV for packet
     *     sniffers; it doesn't matter otherwise.
     */
#if _BYTE_ORDER == _BIG_ENDIAN
    ivp[2] = iv >> 0;
    ivp[1] = iv >> 8;
    ivp[0] = iv >> 16;
#else
    ivp[0] = iv >> 0;
    ivp[1] = iv >> 8;
    ivp[2] = iv >> 16;
#endif
    ivp[3] = keyid;

    /*
     * Finally, do software encrypt if neeed.
     */
    if ((k->wk_flags & IEEE80211_KEY_SWENCRYPT) &&
        !__wep_encrypt(k, wbuf, hdrlen))
    {
        return 0;
    }

#if UMAC_SUPPORT_TKIP_SW_CRYPTO
    /*
     * Frame #3 in shared key authentication needs to be encrypted.
     * If chip is initialized for MFP passthru mode, it won't even do
     * WEP encryption for auth frame #3. Must do it in software.
     */
    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    if (IEEE80211_IS_AUTH(wh)) {
        u_int32_t hwmfp;
        hwmfp = vap->iv_ic->ic_get_mfpsupport(vap->iv_ic);
        if (hwmfp  == IEEE80211_MFP_PASSTHRU) {
            u_int16_t pktlen;
            u_int8_t rc4key[IEEE80211_WEP_IVLEN + IEEE80211_KEYBUF_SIZE];
            pktlen = wbuf_get_pktlen(wbuf);
            /* NB: this assumes the header was pulled up */
            OS_MEMCPY(rc4key, ((u_int8_t *) wh) + hdrlen, IEEE80211_WEP_IVLEN);
            OS_MEMCPY(rc4key + IEEE80211_WEP_IVLEN, k->wk_key, k->wk_keylen);
            if (k->wk_keylen == IEEE80211_WEP_KEYLEN) {
                /* 
                 * 40 bit key is used.
                 * Only first 8 bytes of rc4key are filled. wep_encrypt algorithm
                 * needs 16 bytes. Duplicate first 8 into 16.
                 */
                OS_MEMCPY(rc4key + (IEEE80211_WEP_IVLEN + k->wk_keylen), 
                          rc4key, (IEEE80211_WEP_IVLEN + k->wk_keylen));
            }
            /* wep_encrypt() is in ieee80211_crypto_tkip_sw.c */
            wep_encrypt(rc4key, wbuf, hdrlen +wep.ic_header,
                        pktlen - (hdrlen + wep.ic_header));
            return 1;
        }
    }
#endif

    return 1;
}

/*
 * Add MIC to the frame as needed.
 */
static int
wep_enmic(struct ieee80211_key *k, wbuf_t wbuf, int force, bool encap)
{
    return 1;
}

/*
 * Validate and strip privacy headers (and trailer) for a
 * received frame.  If necessary, decrypt the frame using
 * the specified key.
 */
static int
wep_decap(struct ieee80211_key *k, wbuf_t wbuf, int hdrlen, struct ieee80211_rx_status *rs)
{
    struct wep_ctx *ctx = k->wk_private;
    struct ieee80211vap *vap = ctx->wc_vap;
    struct ieee80211_frame *wh;
    struct ieee80211_mac_stats *mac_stats;
    u_int8_t *origHdr = (u_int8_t*)wbuf_header(wbuf);
    bool is_bcast = 0;

    wh = (struct ieee80211_frame *)origHdr;
    is_bcast = IEEE80211_IS_MULTICAST(wh->i_addr1) ? 1:0;
    mac_stats = is_bcast ? &vap->iv_multicast_stats : &vap->iv_unicast_stats;

    if (rs->rs_flags & IEEE80211_RX_DECRYPT_ERROR) {
        /* The frame already failed decryption in hardware,
         * just update statistics and return. */
#ifdef QCA_SUPPORT_CP_STATS
        is_bcast ? vdev_mcast_cp_stats_rx_wepfail_inc(vap->vdev_obj, 1) :
                   vdev_ucast_cp_stats_rx_wepfail_inc(vap->vdev_obj, 1);
        WLAN_PEER_CP_STAT_ADDRBASED(vap, wh->i_addr2, rx_wepfail);
#endif
        return 0;
    }

    if (k->wk_flags & IEEE80211_KEY_SWDECRYPT){
        wep_dump(k, wbuf, hdrlen);	
    }

    /*
     * Check if the device handled the decrypt in hardware.
     * If so we just strip the header; otherwise we need to
     * handle the decrypt in software.
     */
    if ((k->wk_flags & IEEE80211_KEY_SWDECRYPT) &&
        !wep_decrypt(k, wbuf, hdrlen)) {
        IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO, wh->i_addr2,
                           "%s", "WEP ICV mismatch on decrypt");
#ifdef QCA_SUPPORT_CP_STATS
        is_bcast ? vdev_mcast_cp_stats_rx_wepfail_inc(vap->vdev_obj, 1) :
                   vdev_ucast_cp_stats_rx_wepfail_inc(vap->vdev_obj, 1);
        WLAN_PEER_CP_STAT_ADDRBASED(vap, wh->i_addr2, rx_wepfail);
#endif
        return 0;
    }

    /*
     * Copy up 802.11 header and strip crypto bits.
     */
    memmove(origHdr + wep.ic_header, origHdr, hdrlen);
    wbuf_pull(wbuf, wep.ic_header);
    wbuf_trim(wbuf, wep.ic_trailer);

    return 1;
}

/*
 * Verify and strip MIC from the frame.
 */
static int
wep_demic(struct ieee80211_key *k, wbuf_t wbuf, int hdrlen, int force, struct ieee80211_rx_status *rs)
{
    return 1;
}

static const u_int32_t crc32_table[256] = {
    0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
    0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
    0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
    0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
    0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
    0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
    0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
    0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
    0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
    0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
    0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
    0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
    0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
    0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
    0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
    0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
    0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
    0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
    0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
    0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
    0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
    0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
    0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
    0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
    0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
    0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
    0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
    0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
    0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
    0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
    0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
    0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
    0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
    0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
    0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
    0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
    0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
    0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
    0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
    0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
    0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
    0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
    0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
    0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
    0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
    0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
    0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
    0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
    0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
    0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
    0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
    0x2d02ef8dL
};

static int
__wep_encrypt(struct ieee80211_key *key, wbuf_t wbuf, int hdrlen)
{
#define S_SWAP(a,b) do { uint8_t t = S[a]; S[a] = S[b]; S[b] = t; } while(0)
    struct wep_ctx *ctx = key->wk_private;
    struct ieee80211vap *vap = ctx->wc_vap;
    u_int8_t rc4key[IEEE80211_WEP_IVLEN + IEEE80211_KEYBUF_SIZE];
    uint8_t *icv;
    uint32_t i, j, k, crc;
    size_t buflen, data_len;
    uint8_t S[256];
    uint8_t *pos;
    u_int off, keylen;

#ifdef QCA_SUPPORT_CP_STATS
    vdev_cp_stats_crypto_wep_inc(vap->vdev_obj, 1);
#endif

    /* NB: this assumes the header was pulled up */
    OS_MEMCPY(rc4key, wbuf_header(wbuf) + hdrlen, IEEE80211_WEP_IVLEN);
    OS_MEMCPY(rc4key + IEEE80211_WEP_IVLEN, key->wk_key, key->wk_keylen);

    /* Setup RC4 state */
    for (i = 0; i < 256; i++)
        S[i] = i;
    j = 0;
    keylen = key->wk_keylen + IEEE80211_WEP_IVLEN;
    for (i = 0; i < 256; i++) {
        j = (j + S[i] + rc4key[i % keylen]) & 0xff;
        S_SWAP(i, j);
    }

    off = hdrlen + wep.ic_header;
    data_len = wbuf_get_pktlen(wbuf) - off;

    /* Compute CRC32 over unencrypted data and apply RC4 to data */
    crc = ~0;
    i = j = 0;
    pos = wbuf_header(wbuf) + off;
    buflen = wbuf_get_pktlen(wbuf) - off;
    for (;;) {
        if (buflen > data_len)
            buflen = data_len;
        data_len -= buflen;
        for (k = 0; k < buflen; k++) {
            crc = crc32_table[(crc ^ *pos) & 0xff] ^ (crc >> 8);
            i = (i + 1) & 0xff;
            j = (j + S[i]) & 0xff;
            S_SWAP(i, j);
            *pos++ ^= S[(S[i] + S[j]) & 0xff];
        }
        if (wbuf_next(wbuf) == NULL) {
            if (data_len != 0) {		/* out of data */
#ifdef IEEE80211_DEBUG
                const struct ieee80211_frame *wh =
                    (const struct ieee80211_frame *)wbuf_header(wbuf);
                IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO,
                                   wh->i_addr2,
                                   "out of data for WEP (data_len %lu)",
                                   (unsigned long) data_len);
#endif
                return 0;
            }
            break;
        }
        wbuf = wbuf_next(wbuf);
        pos = wbuf_header(wbuf);
        buflen = wbuf_get_pktlen(wbuf);
    }
    crc = ~crc;

    if (wbuf_get_tailroom(wbuf) < wep.ic_trailer) {
#ifdef IEEE80211_DEBUG
        const struct ieee80211_frame *wh =
            (const struct ieee80211_frame *)wbuf_header(wbuf);
        /* NB: should not happen */
        IEEE80211_NOTE_MAC(ctx->wc_vap, IEEE80211_MSG_CRYPTO,
                           wh->i_addr1, "no room for %s ICV, tailroom %u",
                           wep.ic_name, wbuf_get_tailroom(wbuf));
#endif
        /* XXX statistic */
        return 0;
    }
    /* Append little-endian CRC32 and encrypt it to produce ICV */
    icv = wbuf_header(wbuf) + wbuf_get_pktlen(wbuf);
    wbuf_append(wbuf, IEEE80211_WEP_CRCLEN);
    icv[0] = crc;
    icv[1] = crc >> 8;
    icv[2] = crc >> 16;
    icv[3] = crc >> 24;
    for (k = 0; k < IEEE80211_WEP_CRCLEN; k++) {
        i = (i + 1) & 0xff;
        j = (j + S[i]) & 0xff;
        S_SWAP(i, j);
        icv[k] ^= S[(S[i] + S[j]) & 0xff];
    }
    return 1;
#undef S_SWAP
}

static int
wep_decrypt(struct ieee80211_key *key, wbuf_t wbuf, int hdrlen)
{
#define S_SWAP(a,b) do { uint8_t t = S[a]; S[a] = S[b]; S[b] = t; } while(0)
    struct wep_ctx *ctx = key->wk_private;
    struct ieee80211vap *vap = ctx->wc_vap;
    u_int8_t *origHdr = (u_int8_t*)wbuf_header(wbuf);
    u_int16_t pkt_len = wbuf_get_pktlen(wbuf);
    u_int8_t rc4key[IEEE80211_WEP_IVLEN + IEEE80211_KEYBUF_SIZE];
    uint8_t icv[IEEE80211_WEP_CRCLEN];
    uint32_t i, j, k, crc;
    size_t buflen, data_len;
    uint8_t S[256];
    uint8_t *pos;
    u_int off, keylen;

#ifdef QCA_SUPPORT_CP_STATS
    vdev_cp_stats_crypto_wep_inc(vap->vdev_obj, 1);
#endif

    /* NB: this assumes the header was pulled up */
    OS_MEMCPY(rc4key, origHdr + hdrlen, IEEE80211_WEP_IVLEN);
    OS_MEMCPY(rc4key + IEEE80211_WEP_IVLEN, key->wk_key, key->wk_keylen);

    /* Setup RC4 state */
    for (i = 0; i < 256; i++)
        S[i] = i;
    j = 0;
    keylen = key->wk_keylen + IEEE80211_WEP_IVLEN;
    for (i = 0; i < 256; i++) {
        j = (j + S[i] + rc4key[i % keylen]) & 0xff;
        S_SWAP(i, j);
    }

    off = hdrlen + wep.ic_header;
    data_len = pkt_len - (off + wep.ic_trailer),

    /* Compute CRC32 over unencrypted data and apply RC4 to data */
    crc = ~0;
    i = j = 0;
    pos = origHdr + off;
    buflen = pkt_len - off;
    for (;;) {
        if (buflen > data_len)
            buflen = data_len;
        data_len -= buflen;
        for (k = 0; k < buflen; k++) {
            i = (i + 1) & 0xff;
            j = (j + S[i]) & 0xff;
            S_SWAP(i, j);
            *pos ^= S[(S[i] + S[j]) & 0xff];
            crc = crc32_table[(crc ^ *pos) & 0xff] ^ (crc >> 8);
            pos++;
        }
        if (wbuf_next(wbuf) == NULL) {
            if (data_len != 0) {		/* out of data */
#ifdef IEEE80211_DEBUG
                struct ieee80211_frame *wh =
                    (struct ieee80211_frame *) origHdr;

                IEEE80211_NOTE_MAC(vap, IEEE80211_MSG_CRYPTO,
                                   wh->i_addr2,
                                   "out of data for WEP (data_len %lu)",
                                   (unsigned long) data_len);
#endif
                return 0;
            }
            break;
        }
        wbuf = wbuf_next(wbuf);
        pos = (u_int8_t*)wbuf_header(wbuf);
        buflen = wbuf_get_pktlen(wbuf);
    }
    crc = ~crc;

    /* Encrypt little-endian CRC32 and verify that it matches with
     * received ICV */
    icv[0] = crc;
    icv[1] = crc >> 8;
    icv[2] = crc >> 16;
    icv[3] = crc >> 24;
    for (k = 0; k < IEEE80211_WEP_CRCLEN; k++) {
        i = (i + 1) & 0xff;
        j = (j + S[i]) & 0xff;
        S_SWAP(i, j);
        /* XXX assumes ICV is contiguous in sk_buf */
        if ((icv[k] ^ S[(S[i] + S[j]) & 0xff]) != *pos++) {
            /* ICV mismatch - drop frame */
            return 0;
        }
    }
    return 1;
#undef S_SWAP
}

/* Module attachment */
void
ieee80211_crypto_register_wep(struct ieee80211com *ic)
{
    ieee80211_crypto_register(ic, &wep);
}
void
ieee80211_crypto_unregister_wep(struct ieee80211com *ic)
{
    ieee80211_crypto_unregister(ic, &wep);
}
#endif
