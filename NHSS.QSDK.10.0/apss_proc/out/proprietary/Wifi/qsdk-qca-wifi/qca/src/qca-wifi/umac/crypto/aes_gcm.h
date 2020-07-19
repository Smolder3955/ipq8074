/*
 * Copyright (c) 2003-2012, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef AES_GCM_H
#define AES_GCM_H

#include <osdep.h>
#include <ieee80211_var.h>
#include "rijndael.h"

#define AES_BLOCK_SIZE 16

int aes_gmac(struct ieee80211_key *key,
			  const u8 *iv, size_t iv_len,
			  const u8 *aad, size_t aad_len, u8 *tag);

int aes_gcm_ae(struct ieee80211_key *key, const u8 *iv, size_t iv_len,
               const u8 *plain, size_t plain_len,
               const u8 *aad, size_t aad_len, u8 *crypt, u8 *tag);

int aes_gcm_ad(struct ieee80211_key *key, const u8 *iv, size_t iv_len,
               const u8 *crypt, size_t crypt_len,
               const u8 *aad, size_t aad_len, const u8 *tag, u8 *plain);

#endif /* AES_GCM_H */
