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
 */


#ifndef IEEE80211_CRYPTO_TKIP_PRIV_H
#define IEEE80211_CRYPTO_TKIP_PRIV_H

#ifndef WLAN_CONV_CRYPTO_SUPPORTED
#include <osdep.h>
#include <ieee80211_var.h>

struct tkip_ctx {
    struct ieee80211vap *tc_vap;	/* for diagnostics + statistics */
    struct ieee80211com *tc_ic;

    u16	tx_ttak[5];
    int	tx_phase1_done;
    u8	tx_rc4key[16];		/* XXX for test module; make locals? */

    u16	rx_ttak[5];
    int	rx_phase1_done;
    u8	rx_rc4key[16];		/* XXX for test module; make locals? */
    u_int64_t rx_rsc;		/* held until MIC verified */
};


static __inline u32 rotl(u32 val, int bits)
{
    return (val << bits) | (val >> (32 - bits));
}


static __inline u32 rotr(u32 val, int bits)
{
    return (val >> bits) | (val << (32 - bits));
}


static __inline u32 xswap(u32 val)
{
    return ((val & 0x00ff00ff) << 8) | ((val & 0xff00ff00) >> 8);
}


#define michael_block(l, r)                     \
    do {                                        \
	r ^= rotl(l, 17);                       \
	l += r;                                 \
	r ^= xswap(l);                          \
	l += r;                                 \
	r ^= rotl(l, 3);                        \
	l += r;                                 \
	r ^= rotr(l, 2);                        \
	l += r;                                 \
    } while (0)


static INLINE u32 get_le32_split(u8 b0, u8 b1, u8 b2, u8 b3)
{
    return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
}

static INLINE u32 get_le32(const u8 *p)
{
    return get_le32_split(p[0], p[1], p[2], p[3]);
}


static INLINE void put_le32(u8 *p, u32 v)
{
    p[0] = v;
    p[1] = v >> 8;
    p[2] = v >> 16;
    p[3] = v >> 24;
}

int  michael_mic(struct tkip_ctx *, const u8 *key,
                         wbuf_t, u_int off, u_int16_t data_len,
                         u8 mic[IEEE80211_WEP_MICLEN]);

#if UMAC_SUPPORT_TKIP_SW_CRYPTO
int tkip_encrypt(struct tkip_ctx *ctx, struct ieee80211_key *key,
                 wbuf_t wbuf0, int hdrlen);
int tkip_decrypt(struct tkip_ctx *ctx, struct ieee80211_key *key,
                 wbuf_t wbuf0, int hdrlen);
int tkip_mhdrie_check(wbuf_t wbuf, u_int16_t pktlen, u_int16_t miclen);
void tkip_mhdrie_create(wbuf_t wbuf);

#else 
static INLINE int tkip_encrypt(struct tkip_ctx *ctx, struct ieee80211_key *key,
                 wbuf_t wbuf0, int hdrlen)
{
    KASSERT(0,(" TKIP SW crypto module not supported \n"));
    return 0;
}

static INLINE int tkip_decrypt(struct tkip_ctx *ctx, struct ieee80211_key *key,
                 wbuf_t wbuf0, int hdrlen)
{
    KASSERT(0,(" TKIP SW crypto module not supported \n"));
    return 0;
}

static INLINE int tkip_mhdrie_check(wbuf_t wbuf, u_int16_t pktlen, u_int16_t miclen)
{
    KASSERT(0,(" TKIP SW crypto module not supported \n"));
    return 0;
}

#define    tkip_mhdrie_create(w) KASSERT(0,(" TKIP SW crypto module not supported \n"));
#endif
#endif
#endif
