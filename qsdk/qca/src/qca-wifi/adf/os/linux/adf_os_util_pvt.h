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

#ifndef _ADF_CMN_OS_UTIL_PVT_H
#define _ADF_CMN_OS_UTIL_PVT_H

#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/mm.h>

#include <linux/random.h>

#if LINUX_VERSION_CODE  <= KERNEL_VERSION(3,3,8)
#include <asm/system.h>
#else
#if defined (__LINUX_MIPS32_ARCH__) || defined (__LINUX_MIPS64_ARCH__)
#include <asm/dec/system.h>
#else
#if !defined ( __i386__) && !defined (__LINUX_POWERPC_ARCH__)
#include <asm/system.h>
#endif
#endif
#endif

#include <adf_os_types.h>
/*
 * Generic compiler-dependent macros if defined by the OS
 */

#define __adf_os_unlikely(_expr)   unlikely(_expr)
#define __adf_os_likely(_expr)     likely(_expr)

/**
 * @brief memory barriers. 
 */
#define __adf_os_wmb()                wmb()
#define __adf_os_rmb()                rmb()
#define __adf_os_mb()                 mb()

#if 0
#define __adf_os_min(_a, _b)         min(_a, _b)
#define __adf_os_max(_a, _b)         max(_a, _b)
#else
#define __adf_os_min(_a, _b)         ((_a) < (_b) ? _a : _b)
#define __adf_os_max(_a, _b)         ((_a) > (_b) ? _a : _b)
#endif

#define __adf_os_abs(_a)             __builtin_abs(_a)

#define MEMINFO_KB(x)  ((x) << (PAGE_SHIFT - 10))   // In kilobytes

/**
 * @brief Assert
 */
#define __adf_os_assert(expr)  do {    \
    if(unlikely(!(expr))) {                                 \
        printk(KERN_ERR "Assertion failed! %s:%s %s:%d\n",   \
              #expr, __FUNCTION__, __FILE__, __LINE__);      \
        dump_stack();                                      \
        panic("Take care of the HOST ASSERT  first\n");          \
    }     \
}while(0)



/**
 * @brief Assert
 */
#define __adf_os_target_assert(expr)  do {    \
    if(unlikely(!(expr))) {                                 \
        printk(KERN_ERR "Assertion failed! %s:%s %s:%d\n",   \
              #expr, __FUNCTION__, __FILE__, __LINE__);      \
        dump_stack();                                      \
        panic("Take care of the TARGET ASSERT first\n");          \
    }     \
}while(0)

/**
 * @brief Warning
 */
#define __adf_os_warn(cond) ({                      \
    int __ret_warn = !!(cond);              \
    if (unlikely(__ret_warn)) {                 \
        printk("WARNING: at %s:%d %s()\n", __FILE__,        \
            __LINE__, __FUNCTION__);            \
        dump_stack();                       \
    }                               \
    unlikely(__ret_warn);                   \
})

/**
 * @brief replace with the name of the function
 */ 
#define __adf_os_function   __FUNCTION__

static inline a_status_t 
__adf_os_get_rand(adf_os_handle_t  hdl, uint8_t *ptr, uint32_t  len)
{
    get_random_bytes(ptr, len);

    return A_STATUS_OK;
}

/**
 * @brief return square root
 */ 
static __adf_os_inline a_uint32_t __adf_os_int_sqrt(a_uint32_t x)  
{
	return int_sqrt(x);
}

/**
 * @brief Get total ram size in Kb
 *
 * @param               none
 *
 * @retval - total ram size in Kb
 */
static inline a_uint64_t
__adf_os_get_totalramsize(void)
{
    struct sysinfo meminfo;
    si_meminfo(&meminfo);
    return (MEMINFO_KB(meminfo.totalram));
}


#endif /*_ADF_CMN_OS_UTIL_PVT_H*/
