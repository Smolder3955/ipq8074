/*
* Copyright (c) 2013, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc.
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
/*
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef __ADF_HST_OS_CRYPTO_PVT_H
#define __ADF_HST_OS_CRYPTO_PVT_H

#include <linux/version.h>
#include <linux/crypto.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
#include <asm/page.h>
#include <linux/mm.h>
#include <linux/scatterlist.h>
#endif

#define __ADF_AES_BLOCK_LEN       16

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
typedef struct crypto_tfm *   __adf_os_cipher_t;
#else
typedef struct crypto_cipher * __adf_os_cipher_t;
#endif

typedef enum __adf_os_crypto_alg{
    __ADF_OS_CRYPTO_AES,
    __ADF_OS_CRYPTO_OTHER
}__adf_os_crypto_alg_t;


static inline __adf_os_cipher_t 
__adf_os_crypto_alloc_cipher(__adf_os_crypto_alg_t type)
{
    __adf_os_cipher_t ctx = NULL;
    switch (type){
    case __ADF_OS_CRYPTO_AES:
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
         ctx = crypto_alloc_tfm("aes", 0);
#else
         ctx = crypto_alloc_cipher("aes", 0,CRYPTO_ALG_ASYNC);
#endif
         break;
    default:
         printk("%s:unsupported algorithm\n",__FUNCTION__);
         break;
    }
    return ctx;
}

static inline void
__adf_os_crypto_free_cipher(__adf_os_cipher_t ctx)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
        crypto_free_tfm(ctx);
#else
        crypto_free_cipher(ctx);
#endif
}

static inline uint32_t
__adf_os_crypto_cipher_setkey(__adf_os_cipher_t cipher,
                            const uint8_t *key, uint8_t keylen)
{
    crypto_cipher_setkey(cipher, key, keylen);
    return 0;
}
static inline a_status_t
__adf_os_crypto_rijndael_encrypt(__adf_os_cipher_t ctx, const void *src, 
                                 void *dst)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,19)
    crypto_cipher_encrypt_one(ctx, dst, src);
#else
    struct scatterlist sg_src;
    struct scatterlist sg_dst;

    sg_src.page = virt_to_page(src);
    sg_src.offset = offset_in_page(src);
    sg_src.length = __ADF_AES_BLOCK_LEN;

    sg_dst.page = virt_to_page(dst);
    sg_dst.offset = offset_in_page(dst);
    sg_dst.length = __ADF_AES_BLOCK_LEN;
    crypto_cipher_encrypt(ctx, &sg_dst, &sg_src, __ADF_AES_BLOCK_LEN);
#endif
    
    return A_STATUS_OK;
}
#endif
