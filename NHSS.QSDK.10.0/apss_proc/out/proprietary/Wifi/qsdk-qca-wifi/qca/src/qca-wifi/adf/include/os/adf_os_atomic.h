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
 * @file adf_os_atomic.h
 * This file abstracts an atomic counter.
 */
 
#ifndef _ADF_OS_ATOMIC_H
#define _ADF_OS_ATOMIC_H

#include <adf_os_atomic_pvt.h>
/**
 * @brief Atomic type of variable.
 * Use this when you want a simple resource counter etc. which is atomic
 * across multiple CPU's. These maybe slower than usual counters on some
 * platforms/OS'es, so use them with caution.
 */
typedef __adf_os_atomic_t    adf_os_atomic_t;

/** 
 * @brief Initialize an atomic type variable
 * @param[in] v a pointer to an opaque atomic variable
 */
static inline void
adf_os_atomic_init(adf_os_atomic_t *v)
{
    __adf_os_atomic_init(v);
}

/**
 * @brief Read the value of an atomic variable.
 * @param[in] v a pointer to an opaque atomic variable
 *
 * @return the current value of the variable
 */
static inline a_uint32_t
adf_os_atomic_read(adf_os_atomic_t *v)
{
    return (__adf_os_atomic_read(v));
}

/**
 * @brief Increment the value of an atomic variable.
 * @param[in] v a pointer to an opaque atomic variable
 */
static inline void
adf_os_atomic_inc(adf_os_atomic_t *v)
{
    __adf_os_atomic_inc(v);
}

/**
 * @brief Decrement the value of an atomic variable.
 * @param v a pointer to an opaque atomic variable
 */
static inline void
adf_os_atomic_dec(adf_os_atomic_t *v)
{
    __adf_os_atomic_dec(v);
}

/**
 * @brief Add a value to the value of an atomic variable.
 * @param v a pointer to an opaque atomic variable
 * @param i the amount by which to increase the atomic counter
 */
static inline void
adf_os_atomic_add(int i, adf_os_atomic_t *v)
{
    __adf_os_atomic_add(i, v);
}

/**
 * @brief Decrement an atomic variable and check if the new value is zero.
 * @param v a pointer to an opaque atomic variable
 * @return
 *      true (non-zero) if the new value is zero,
 *      or false (0) if the new value is non-zero
 */
static inline a_uint32_t
adf_os_atomic_dec_and_test(adf_os_atomic_t *v)
{
    return __adf_os_atomic_dec_and_test(v);
}

/**
 * @brief set the value of an atomic variable to given value.
 * @param v a pointer to an opaque atomic variable
 * @param i the amount to set to the atomic counter
 */
static inline void
adf_os_atomic_set(adf_os_atomic_t *v, int i)
{
    __adf_os_atomic_set(v, i);
}

#endif
