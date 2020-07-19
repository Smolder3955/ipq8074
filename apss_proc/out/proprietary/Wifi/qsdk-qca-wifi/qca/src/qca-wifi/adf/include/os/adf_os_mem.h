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
 * @file adf_os_mem.h
 * This file abstracts memory operations.
 */

#ifndef _ADF_OS_MEM_H
#define _ADF_OS_MEM_H

#include <adf_os_types.h>
#include <adf_os_mem_pvt.h>

/**
 * @brief Allocate a memory buffer. Note this call can block.
 *
 * @param[in] size    buffer size
 *
 * @return Buffer pointer or NULL if there's not enough memory.
 */
static inline void *
adf_os_mem_alloc(adf_os_device_t osdev, adf_os_size_t size)
{
    return __adf_os_mem_alloc(osdev, size);
}

void *
adf_os_mem_alloc_outline(adf_os_device_t osdev, adf_os_size_t size);

/**
 * @brief Free malloc'ed buffer
 *
 * @param[in] buf     buffer pointer allocated by @ref adf_os_mem_alloc
 */
static inline void
adf_os_mem_free(void *buf)
{
    __adf_os_mem_free(buf);
}

void
adf_os_mem_free_outline(void *buf);

static inline  void *
adf_os_mem_alloc_consistent(
    adf_os_device_t osdev, adf_os_size_t size, adf_os_dma_addr_t *paddr, adf_os_dma_context_t mctx)
{
    return __adf_os_mem_alloc_consistent(osdev, size, paddr, mctx);
}

static inline void
adf_os_mem_free_consistent(
    adf_os_device_t osdev,
    adf_os_size_t size,
    void *vaddr,
    adf_os_dma_addr_t paddr,
    adf_os_dma_context_t memctx)
{
    __adf_os_mem_free_consistent(osdev, size, vaddr, paddr, memctx);
}

/**
 * @brief Move a memory buffer. Overlapping regions are not allowed.
 *
 * @param[in] dst     destination address
 * @param[in] src     source address
 * @param[in] size    buffer size
 */
static inline __ahdecl void
adf_os_mem_copy(void *dst, const void *src, adf_os_size_t size)
{
    __adf_os_mem_copy(dst, src, size);
}

/**
 * @brief Does a non-destructive copy of memory buffer
 *
 * @param[in] dst     destination address
 * @param[in] src     source address
 * @param[in] size    buffer size
 */
static inline void 
adf_os_mem_move(void *dst, void *src, adf_os_size_t size)
{
	__adf_os_mem_move(dst,src,size);
}


/**
 * @brief Fill a memory buffer
 * 
 * @param[in] buf   buffer to be filled
 * @param[in] b     byte to fill
 * @param[in] size  buffer size
 */
static inline void
adf_os_mem_set(void *buf, a_uint8_t b, adf_os_size_t size)
{
    __adf_os_mem_set(buf, b, size);
}


/**
 * @brief Zero a memory buffer
 * 
 * @param[in] buf   buffer to be zeroed
 * @param[in] size  buffer size
 */
static inline __ahdecl void
adf_os_mem_zero(void *buf, adf_os_size_t size)
{
    __adf_os_mem_zero(buf, size);
}

void
adf_os_mem_zero_outline(void *buf, adf_os_size_t size);

/**
 * @brief Compare two memory buffers
 *
 * @param[in] buf1  first buffer
 * @param[in] buf2  second buffer
 * @param[in] size  buffer size
 *
 * @retval    0     equal
 * @retval    1     not equal
 */
static inline int
adf_os_mem_cmp(const void *buf1, const void *buf2, adf_os_size_t size)
{
    return __adf_os_mem_cmp(buf1, buf2, size);
}

/**
 * @brief Compare two strings
 *
 * @param[in] str1  First string
 * @param[in] str2  Second string
 *
 * @retval    0     equal
 * @retval    >0    not equal, if  str1  sorts lexicographically after str2
 * @retval    <0    not equal, if  str1  sorts lexicographically before str2
 */
static inline a_int32_t
adf_os_str_cmp(const char *str1, const char *str2)
{
    return __adf_os_str_cmp(str1, str2);
}
/**
 * @brief Copy from one string to another
 *
 * @param[in] dest  destination string
 * @param[in] src   source string
 * @param[in] bytes limit of num bytes to copy
 *
 * @return    0     returns the initial value of dest
 */
static inline char *
adf_os_str_ncopy(char *dest, const char *src, a_uint32_t bytes)
{
    return __adf_os_str_ncopy(dest, src, bytes);
}

/**
 * @brief Returns the length of a string
 *
 * @param[in] str   input string
 *
 * @return    length of string
 */
static inline a_int32_t
adf_os_str_len(const char *str)
{
    return (a_int32_t)__adf_os_str_len(str);
}

static inline a_status_t
adf_os_mem_map_nbytes_single(
    adf_os_device_t osdev, void* buf, adf_os_dma_dir_t dir, int nbytes, a_uint32_t *phy_addr)
{
#if defined(HIF_PCI)
    return __adf_os_mem_map_nbytes_single(osdev, buf, dir, nbytes, phy_addr);
#else
    return 0;
#endif
}

static inline void
adf_os_mem_unmap_nbytes_single(
    adf_os_device_t osdev, a_uint32_t phy_addr, adf_os_dma_dir_t dir, int nbytes)
{
#if defined(HIF_PCI)
    __adf_os_mem_unmap_nbytes_single(osdev, phy_addr, dir, nbytes);
#endif
}

#ifdef __KERNEL__
typedef __adf_mempool_t adf_mempool_t;

/**
 * @brief Create and initialize memory pool
 *
 * @param[in] osdev  platform device object
 * @param[out] pool_addr address of the pool created
 * @param[in] elem_cnt  no. of elements in pool
 * @param[in] elem_size  size of each pool element in bytes
 * @param[in] flags  flags
 *
 * @retrun   Handle to memory pool or NULL if allocation failed
 */
static inline int adf_mempool_init(adf_os_device_t osdev, adf_mempool_t *pool_addr, int elem_cnt, size_t elem_size, u_int32_t flags)
{
    return __adf_mempool_init(osdev, pool_addr, elem_cnt, elem_size, flags);
}

/**
 * @brief Destroy memory pool
 *
 * @param[in] osdev  platform device object
 * @param[in] Handle to memory pool
 */
static inline void adf_mempool_destroy(adf_os_device_t osdev, adf_mempool_t pool)
{
    __adf_mempool_destroy(osdev, pool);
}

/**
 * @brief Allocate an element memory pool
 *
 * @param[in] osdev  platform device object
 * @param[in] Handle to memory pool
 *
 * @return   Pointer to the allocated element or NULL if the pool is empty
 */
static inline void *adf_mempool_alloc(adf_os_device_t osdev, adf_mempool_t pool)
{
    return (void *)__adf_mempool_alloc(osdev, pool);
}

/**
 * @brief Free a memory pool element
 *
 * @param[in] osdev Platform device object
 * @param[in] pool  Handle to memory pool
 * @param[in] buf   Element to be freed
 */
static inline void adf_mempool_free(adf_os_device_t osdev, adf_mempool_t pool, void *buf)
{
    __adf_mempool_free(osdev, pool, buf);
}
#endif
#endif
