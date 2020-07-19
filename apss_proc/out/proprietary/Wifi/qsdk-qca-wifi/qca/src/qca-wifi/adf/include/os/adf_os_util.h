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
 * @file adf_os_util.h
 * This file defines utility functions.
 */

#ifndef _ADF_OS_UTIL_H
#define _ADF_OS_UTIL_H

#include <adf_os_util_pvt.h>

/**
 * @brief Compiler-dependent macro denoting code likely to execute.
 */
#define adf_os_unlikely(_expr)     __adf_os_unlikely(_expr)

/**
 * @brief Compiler-dependent macro denoting code unlikely to execute.
 */
#define adf_os_likely(_expr)       __adf_os_likely(_expr)

/**
 * @brief read memory barrier. 
 */
#define adf_os_wmb()                __adf_os_wmb()

/**
 * @brief write memory barrier. 
 */
#define adf_os_rmb()                __adf_os_rmb()

/**
 * @brief read + write memory barrier. 
 */
#define adf_os_mb()                 __adf_os_mb()

/**
 * @brief return the lesser of a, b
 */ 
#define adf_os_min(_a, _b)          __adf_os_min(_a, _b)

/**
 * @brief return the larger of a, b
 */ 
#define adf_os_max(_a, _b)          __adf_os_max(_a, _b)

/**
 * @brief assert "expr" evaluates to false.
 */ 
#ifdef ADF_OS_DEBUG
#define adf_os_assert(expr)         __adf_os_assert(expr)
#else
#define adf_os_assert(expr)
#endif /* ADF_OS_DEBUG */

/**
 * @brief alway assert "expr" evaluates to false.
 */
#define adf_os_assert_always(expr)  __adf_os_assert(expr)

/**
 * @brief alway target assert "expr" evaluates to false.
 */

#define adf_os_target_assert_always(expr)  __adf_os_target_assert(expr)

/**
 * @brief warn & dump backtrace if expr evaluates true
 */
#define adf_os_warn(expr)           __adf_os_warn(expr)
/**
 * @brief supply pseudo-random numbers
 */
static inline void adf_os_get_rand(adf_os_handle_t  hdl, 
                                   a_uint8_t       *ptr, 
                                   a_uint32_t       len)
{
    __adf_os_get_rand(hdl, ptr, len);
}



/**
 * @brief return the absolute value of a
 */
#define adf_os_abs(_a)              __adf_os_abs(_a)

/**
 * @brief replace with the name of the current function
 */
#define adf_os_function             __adf_os_function



/**
 * @brief return square root
 */

/** 
 * @brief  Math function for getting a square root
 * 
 * @param[in] x     Number to compute the sqaure root
 * 
 * @return  Sqaure root as integer
 */
static adf_os_inline a_uint32_t 
adf_os_int_sqrt(a_uint32_t x)
{
	return __adf_os_int_sqrt(x);
}

/**
 * @brief Get total ram size in Kb
 *
 * @param               none
 *
 * @retval - total ram size in Kb
 */
static inline a_uint64_t adf_os_get_totalramsize(void)
{
    return __adf_os_get_totalramsize();
}

#endif /*_ADF_OS_UTIL_H*/
