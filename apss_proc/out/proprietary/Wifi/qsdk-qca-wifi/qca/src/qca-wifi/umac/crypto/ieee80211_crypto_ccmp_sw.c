/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 *
 * Notifications and licenses are retained for attribution purposes only.
 */
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
#include <ieee80211_crypto_ccmp_priv.h>
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

/*
 * Host AP crypt: host-based CCMP encryption implementation for Host AP driver
 *
 * Copyright (c) 2003-2004, Jouni Malinen <jkmaline@cc.hut.fi>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. See README and COPYING for
 * more details.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 */
  /* 
 *QCA chooses to take this file subject to the terms of the BSD license. 
 */

#if UMAC_SUPPORT_CCMP_SW_CRYPTO

extern const struct ieee80211_cipher ccmp; 

#define	CCMP_ENCRYPT(_i, _b, _b0, _pos, _e, _len) do {	\
	/* Authentication */				\
	xor_block(_b, _pos, _len);			\
	rijndael_encrypt(&ctx->cc_aes, _b, _b);		\
	/* Encryption, with counter */			\
	_b0[14] = (_i >> 8) & 0xff;			\
	_b0[15] = _i & 0xff;				\
	rijndael_encrypt(&ctx->cc_aes, _b0, _e);	\
	xor_block(_pos, _e, _len);			\
    } while (0)

int
ccmp_encrypt(struct ieee80211_key *key, wbuf_t wbuf0, int hdrlen, int mfp)
{
    struct ccmp_ctx *ctx = key->wk_private;
    struct ieee80211_frame *wh;
    wbuf_t wbuf = wbuf0;
    int data_len, i, space;
    uint8_t aad[2 * AES_BLOCK_LEN], b0[AES_BLOCK_LEN], b[AES_BLOCK_LEN],
        e[AES_BLOCK_LEN], s0[AES_BLOCK_LEN];
    uint8_t *pos;
#ifdef QCA_SUPPORT_CP_STATS
    vdev_cp_stats_crypto_ccmp_inc(ctx->cc_vap->vdev_obj, 1);
#endif
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    data_len = wbuf_get_pktlen(wbuf) - (hdrlen + ccmp.ic_header);
    ccmp_init_blocks(&ctx->cc_aes, wh, key->wk_keytsc,
                     data_len, b0, aad, b, s0, mfp);

    i = 1;
    pos = (u_int8_t *)wbuf_header(wbuf) + hdrlen + ccmp.ic_header;
    /* NB: assumes header is entirely in first mbuf */
    space = wbuf_get_pktlen(wbuf) - (hdrlen + ccmp.ic_header);
    for (;;) {
        if (space > data_len)
            space = data_len;
        /*
         * Do full blocks.
         */
        while (space >= AES_BLOCK_LEN) {
            CCMP_ENCRYPT(i, b, b0, pos, e, AES_BLOCK_LEN);
            pos += AES_BLOCK_LEN, space -= AES_BLOCK_LEN;
            data_len -= AES_BLOCK_LEN;
            i++;
        }
        if (data_len <= 0)		/* no more data */
            break;
        wbuf = wbuf_next(wbuf);
        if (wbuf == NULL) {		/* last buffer */
            if (space != 0) {
                /*
                 * Short last block.
                 */
                CCMP_ENCRYPT(i, b, b0, pos, e, space);
            }
            break;
        }
#if 1 /* assume only one chunk */
        break;
#else
        if (space != 0) {
            uint8_t *pos_next;
            int space_next;
            int len, dl, sp;
            wbuf_t wbufi_new;

            /*
             * Block straddles one or more mbufs, gather data
             * into the block buffer b, apply the cipher, then
             * scatter the results back into the mbuf chain.
             * The buffer will automatically get space bytes
             * of data at offset 0 copied in+out by the
             * CCMP_ENCRYPT request so we must take care of
             * the remaining data.
             */
            wbuf_new = wbuf;
            dl = data_len;
            sp = space;
            for (;;) {
                pos_next = (u_int8_t *)wbuf_header(wbuf_new);
                len = min(dl, AES_BLOCK_LEN);
                space_next = len > sp ? len - sp : 0;
                if (wbuf_get_len(wbuf_new) >= space_next) {
                    /*
                     * This mbuf has enough data; just grab
                     * what we need and stop.
                     */
                    xor_block(b+sp, pos_next, space_next);
                    break;
                }
                /*
                 * This mbuf's contents are insufficient,
                 * take 'em all and prepare to advance to
                 * the next mbuf.
                 */
                xor_block(b+sp, pos_next, n->m_len);
                sp += wbuf_get_len(wbuf_new), dl -= wbuf_get_len(wbuf_new);
                wbuf_next = m_next;
                if (n == NULL)
                    break;
            }

            CCMP_ENCRYPT(i, b, b0, pos, e, space);

            /* NB: just like above, but scatter data to mbufs */
            dl = data_len;
            sp = space;
            for (;;) {
                pos_next = mtod(m, uint8_t *);
                len = min(dl, AES_BLOCK_LEN);
                space_next = len > sp ? len - sp : 0;
                if (m->m_len >= space_next) {
                    xor_block(pos_next, e+sp, space_next);
                    break;
                }
                xor_block(pos_next, e+sp, m->m_len);
                sp += m->m_len, dl -= m->m_len;
                m = m->m_next;
                if (m == NULL)
                    goto done;
            }
            /*
             * Do bookkeeping.  m now points to the last mbuf
             * we grabbed data from.  We know we consumed a
             * full block of data as otherwise we'd have hit
             * the end of the mbuf chain, so deduct from data_len.
             * Otherwise advance the block number (i) and setup
             * pos+space to reflect contents of the new mbuf.
             */
            data_len -= AES_BLOCK_LEN;
            i++;
            pos = pos_next + space_next;
            space = m->m_len - space_next;
        } else {
            /*
             * Setup for next buffer.
             */
            pos = mtod(m, uint8_t *);
            space = m->m_len;
        }
#endif
    }
//done:
    /* tack on MIC */
    xor_block(b, s0, ccmp.ic_trailer);
    wbuf_append(wbuf0, ccmp.ic_trailer);
    pos = (u_int8_t *) wbuf_header(wbuf0) + wbuf_get_pktlen(wbuf0) - ccmp.ic_trailer;
    OS_MEMCPY(pos, b, ccmp.ic_trailer);
    return 1;
}
#undef CCMP_ENCRYPT

#define	CCMP_DECRYPT(_i, _b, _b0, _pos, _a, _len) do {	\
	/* Decrypt, with counter */			\
	_b0[14] = (_i >> 8) & 0xff;			\
	_b0[15] = _i & 0xff;				\
	rijndael_encrypt(&ctx->cc_aes, _b0, _b);	\
	xor_block(_pos, _b, _len);			\
	/* Authentication */				\
	xor_block(_a, _pos, _len);			\
	rijndael_encrypt(&ctx->cc_aes, _a, _a);		\
    } while (0)

int
ccmp_decrypt(struct ieee80211_key *key, u_int64_t pn, wbuf_t wbuf, int hdrlen ,int mfp)
{
    struct ccmp_ctx *ctx = key->wk_private;
    struct ieee80211_frame *wh;
    uint8_t aad[2 * AES_BLOCK_LEN];
    uint8_t b0[AES_BLOCK_LEN], b[AES_BLOCK_LEN], a[AES_BLOCK_LEN];
    uint8_t mic[AES_BLOCK_LEN];
    u_int data_len;
    wbuf_t wbuf0 = wbuf;
    int i;
    uint8_t *pos;
    u_int space;

#ifdef QCA_SUPPORT_CP_STATS
    vdev_cp_stats_crypto_ccmp_inc(ctx->cc_vap->vdev_obj, 1);
#endif

    wh = (struct ieee80211_frame *)wbuf_header(wbuf);

    data_len = wbuf_get_pktlen(wbuf) - (hdrlen + ccmp.ic_header + ccmp.ic_trailer);
    ccmp_init_blocks(&ctx->cc_aes, wh, pn, data_len, b0, aad, a, b, mfp);

    // Extract mic for later verification
    OS_MEMCPY(mic, wbuf_header(wbuf) + wbuf_get_pktlen(wbuf) - ccmp.ic_trailer, ccmp.ic_trailer);
    xor_block(mic, b, ccmp.ic_trailer);

    i = 1;
    pos = (u_int8_t *)wbuf_header(wbuf) + hdrlen + ccmp.ic_header;
    space = wbuf_get_pktlen(wbuf) - (hdrlen + ccmp.ic_header);
    for (;;) {
        if (space > data_len)
            space = data_len;
        while (space >= AES_BLOCK_LEN) {
            CCMP_DECRYPT(i, b, b0, pos, a, AES_BLOCK_LEN);
            pos += AES_BLOCK_LEN, space -= AES_BLOCK_LEN;
            data_len -= AES_BLOCK_LEN;
            i++;
        }
        if (data_len <= 0)		/* no more data */
            break;

        wbuf0 = NULL;  // RNWF only supports single buffer rx

        if (space != 0)		/* short last block */
           CCMP_DECRYPT(i, b, b0, pos, a, space);

        break;
#if 0
        if (space != 0) {
            uint8_t *pos_next;
            u_int space_next;
            u_int len;

            /*
             * Block straddles buffers, split references.  We
             * do not handle splits that require >2 buffers
             * since rx'd frames are never badly fragmented
             * because drivers typically recv in clusters.
             */
            pos_next = mtod(m, uint8_t *);
            len = min(data_len, AES_BLOCK_LEN);
            space_next = len > space ? len - space : 0;
            KASSERT(m->m_len >= space_next,
                    ("not enough data in following buffer, "
                     "m_len %u need %u\n", m->m_len, space_next));

            xor_block(b+space, pos_next, space_next);
            CCMP_DECRYPT(i, b, b0, pos, a, space);
            xor_block(pos_next, b+space, space_next);
            data_len -= len;
            i++;

            pos = pos_next + space_next;
            space = m->m_len - space_next;
        } else {
            /*
             * Setup for next buffer.
             */
            pos = mtod(m, uint8_t *);
            space = m->m_len;
        }
#endif
    }
    if (memcmp(mic, a, ccmp.ic_trailer) != 0) {
        IEEE80211_NOTE_MAC(ctx->cc_vap, IEEE80211_MSG_CRYPTO,
                           wh->i_addr2,
                           "AES-CCM decrypt failed; MIC mismatch (keyix %u, rsc %llu)",
                           key->wk_keyix, pn);
        return 0;
    }
    return 1;
}
#undef CCMP_DECRYPT

#endif
#endif
