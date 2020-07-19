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


#ifndef IEEE80211_CRYPTO_CCMP_PRIV_H
#define IEEE80211_CRYPTO_CCMP_PRIV_H

#ifndef WLAN_CONV_CRYPTO_SUPPORTED

#include <osdep.h>
#include <ieee80211_var.h>
#include "rijndael.h"

#define AES_BLOCK_LEN 16

struct ccmp_ctx {
    struct ieee80211vap      *cc_vap;	/* for diagnostics + stats */
    struct ieee80211com      *cc_ic;
    rijndael_ctx             cc_aes;
};

static INLINE void
xor_block(uint8_t *b, const uint8_t *a, size_t len)
{
    int i;
    for (i = 0; i < len; i++)
        b[i] ^= a[i];
}



void
ccmp_init_blocks(rijndael_ctx *ctx, struct ieee80211_frame *wh,
                 u_int64_t pn, u_int32_t dlen,
                 uint8_t b0[AES_BLOCK_LEN], uint8_t aad[2 * AES_BLOCK_LEN],
                 uint8_t auth[AES_BLOCK_LEN], uint8_t s0[AES_BLOCK_LEN], uint8_t mfp);


#if UMAC_SUPPORT_CCMP_SW_CRYPTO
int ccmp_encrypt(struct ieee80211_key *, wbuf_t, int hdrlen, int mfp);
int ccmp_decrypt(struct ieee80211_key *, u_int64_t pn, wbuf_t, int hdrlen, int mfp);
#else

static INLINE int ccmp_encrypt(struct ieee80211_key *k, wbuf_t w, int hdrlen, int mfp) 
{
    KASSERT(0,(" CCMP SW crypto module not supported \n"));
    return 0;
}
static INLINE int ccmp_decrypt(struct ieee80211_key *k, u_int64_t pn, wbuf_t w, int hdrlen, int mfp)
{
    KASSERT(0,(" CCMP SW crypto module not supported \n"));
    return 0;
}

#endif /* UMAC_SUPPORT_CCMP_SW_CRYPTO */
#endif
#endif /* IEEE80211_CRYPTO_CCMP_PRIV_H */
