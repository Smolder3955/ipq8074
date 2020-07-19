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

/**
 * @ingroup adf_os_public
 * @file adf_os_crypto.h
 * This file defines crypto APIs
 */

#ifndef __ADF_OS_CRYPTO_H
#define __ADF_OS_CRYPTO_H

#include <adf_os_crypto_pvt.h>

/**
 * @brief Representation of a cipher context.
 */ 
typedef __adf_os_cipher_t     adf_os_cipher_t;

/**
 * @brief Types of crypto algorithms
 */ 
typedef enum adf_os_crypto_alg{
    ADF_OS_CRYPTO_AES = __ADF_OS_CRYPTO_AES,
    ADF_OS_CRYPTO_OTHER = __ADF_OS_CRYPTO_OTHER,
}adf_os_crypto_alg_t;


/**
 * @brief allocate the cipher context
 * @param[in] type crypto algorithm
 * 
 * @return the new cipher context
 */
static inline adf_os_cipher_t
adf_os_crypto_alloc_cipher(adf_os_crypto_alg_t type)
{
    return __adf_os_crypto_alloc_cipher(type);
}

/**
 * @brief free the cipher context
 * 
 * @param[in] cipher cipher context
 */
static inline void
adf_os_crypto_free_cipher(adf_os_cipher_t cipher)
{
    __adf_os_crypto_free_cipher(cipher);
}

/**
 * @brief set the key for cipher context with length keylen
 * 
 * @param[in] cipher    cipher context
 * @param[in] key       key material
 * @param[in] keylen    length of key material
 * 
 * @return a_uint32_t
 */
static inline a_uint32_t
adf_os_crypto_cipher_setkey(adf_os_cipher_t cipher, const a_uint8_t *key, a_uint8_t keylen)
{
    return __adf_os_crypto_cipher_setkey(cipher, key, keylen);
}

/**
 * @brief encrypt the data with AES
 * 
 * @param[in]   cipher  cipher context
 * @param[in]   src     unencrypted data
 * @param[out]  dst     encrypted data
 */
static inline void
adf_os_crypto_rijndael_encrypt(adf_os_cipher_t cipher, const void *src, void *dst)
{
    __adf_os_crypto_rijndael_encrypt(cipher, src, dst);
}
#if ATH_GEN_RANDOMNESS

#ifndef NBBY
#define NBBY 8
#endif
/**
 * For KERNEL_VERSION >= 3.3.8  random_input_words() is used
 * and in KERNEL_VERSION 2.6.31 add_randomness() is used
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,8)
#define OS_ADD_RANDOMNESS(b,s,n)  random_input_words(b,s,n)
#define BIT_POS NBBY*4
#else
#define OS_ADD_RANDOMNESS(b,s,n)  add_randomness(b,s,n)
#define BIT_POS NBBY
#endif

static inline void
adf_os_ath_gen_randomness(uint8_t random_rssi)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,3,8)
    static uint32_t random_value;
#else
    static uint8_t random_value;
#endif
    static uint8_t  random_pos;
    random_value |= ((random_rssi)&(0x01)) << random_pos;
    if (++random_pos == BIT_POS) {
        OS_ADD_RANDOMNESS(&random_value, 1, BIT_POS);
        random_value = 0;
        random_pos = 0;
    }
}
#endif

#endif
