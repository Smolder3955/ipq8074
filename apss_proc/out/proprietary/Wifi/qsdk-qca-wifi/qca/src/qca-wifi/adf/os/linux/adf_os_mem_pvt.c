/*
* Copyright (c) 2018 Qualcomm Innovation Center, Inc.
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


#include "adf_os_mem.h"
#include "adf_os_module.h"


int adf_dbg_mask;
adf_os_declare_param(adf_dbg_mask, ADF_OS_PARAM_TYPE_INT32);

u_int8_t prealloc_disabled = 1;
adf_os_declare_param(prealloc_disabled, ADF_OS_PARAM_TYPE_UINT8);
EXPORT_SYMBOL(prealloc_disabled);

void __adf_mempool_free(adf_os_device_t osdev, __adf_mempool_t pool, void *buf);

/**
 * @brief Create and initialize memory pool
 *
 * @param[in] osdev  platform device object
 * @param[out] pool_addr address of the pool created
 * @param[in] elem_cnt  no. of elements in pool
 * @param[in] elem_size  size of each pool element in bytes
 * @param[in] flags  flags
 *
 * @return   Handle to memory pool or NULL if allocation failed
 */
int __adf_mempool_init(adf_os_device_t osdev, __adf_mempool_t *pool_addr, int elem_cnt, size_t elem_size, u_int32_t flags)
{
    __adf_mempool_ctxt_t *new_pool = NULL;
    u_int32_t align = L1_CACHE_BYTES;
    unsigned long aligned_pool_mem;
    int pool_id;
    int i;

    if (prealloc_disabled) {
        /* TBD: We can maintain a list of pools in adf_device_t to help debugging
         * when pre-allocation is not enabled */
        new_pool = (__adf_mempool_ctxt_t *)kmalloc(sizeof(__adf_mempool_ctxt_t), GFP_KERNEL);
        if (new_pool == NULL) {
            return -1;
        }
        memset(new_pool,0, sizeof(*new_pool));
        /* TBD: define flags for zeroing buffers etc */
        new_pool->flags = flags;
        new_pool->elem_size = elem_size;
        new_pool->max_elem = elem_cnt;
        *pool_addr = new_pool;
        return 0;
    }

    for (pool_id = 0; pool_id < MAX_MEM_POOLS; pool_id++) {
        if (osdev->mem_pool[pool_id] == NULL) {
            break;
        }
    }

    if (pool_id == MAX_MEM_POOLS) {
        return -ENOMEM;
    }

    new_pool = osdev->mem_pool[pool_id] = (__adf_mempool_ctxt_t *)kmalloc(sizeof(__adf_mempool_ctxt_t), GFP_KERNEL);
    if (new_pool == NULL) {
        return -ENOMEM;
    }

    memset(new_pool,0, sizeof(*new_pool));
    /* TBD: define flags for zeroing buffers etc */
    new_pool->flags = flags;
    new_pool->pool_id = pool_id;

    /* Round up the element size to cacheline */
    new_pool->elem_size = roundup(elem_size, L1_CACHE_BYTES);
    new_pool->mem_size = elem_cnt * new_pool->elem_size + ((align)?(align - 1):0);
    if ((new_pool->pool_mem = kzalloc(new_pool->mem_size, GFP_KERNEL)) == NULL) {
        /* TBD: Check if we need get_free_pages above */
        kfree(new_pool);
        osdev->mem_pool[pool_id] = NULL;
        return -ENOMEM;
    }

    spin_lock_init(&new_pool->lock);

    /* Initialize free list */
    aligned_pool_mem = (unsigned long)(new_pool->pool_mem) + ((align)? (unsigned long)(new_pool->pool_mem)%align:0);
    STAILQ_INIT(&new_pool->free_list);

    for (i=0; i<elem_cnt; i++) {
        STAILQ_INSERT_TAIL(&(new_pool->free_list), (mempool_elem_t *)(aligned_pool_mem + (new_pool->elem_size * i)), mempool_entry);
    }

    new_pool->free_cnt=elem_cnt;
    *pool_addr = new_pool;
    return 0;
}

/**
 * @brief Destroy memory pool
 *
 * @param[in] osdev  platform device object
 * @param[in] Handle to memory pool
 */
void __adf_mempool_destroy(adf_os_device_t osdev, __adf_mempool_t pool)
{
    int pool_id = 0;

    if (!pool) {
        return;
    }

    if (prealloc_disabled) {
        kfree(pool);
        return;
    }

    pool_id = pool->pool_id;

#if MEMPOOL_DEBUG
    /* TBD: Check if free count matches elem_cnt if debug is enabled */
#endif
    kfree(pool->pool_mem);
    kfree(pool);
    osdev->mem_pool[pool_id] = NULL;
}

/**
 * @brief Allocate an element memory pool
 *
 * @param[in] osdev  platform device object
 * @param[in] Handle to memory pool
 *
 * @return   Pointer to the allocated element or NULL if the pool is empty
 */
void *__adf_mempool_alloc(adf_os_device_t osdev, __adf_mempool_t pool)
{
    void *buf = NULL;

    if (!pool) {
         return NULL;
    }

    if (prealloc_disabled)
         return  adf_os_mem_alloc(osdev, pool->elem_size);

    spin_lock_bh(&pool->lock);

    if ((buf = STAILQ_FIRST(&pool->free_list)) != NULL) {
         STAILQ_REMOVE_HEAD(&pool->free_list, mempool_entry);
         pool->free_cnt--;
    }

#if MEMPOOL_DEBUG
        /* TBD: Update free count if debug is enabled */
#endif
    spin_unlock_bh(&pool->lock);

    return buf;
}

/**
 * @brief Free a memory pool element
 *
 * @param[in] osdev Platform device object
 * @param[in] pool  Handle to memory pool
 * @param[in] buf   Element to be freed
 */
void __adf_mempool_free(adf_os_device_t osdev, __adf_mempool_t pool, void *buf)
{
    if (!pool) {
         return;
    }

    if (prealloc_disabled)
         return adf_os_mem_free(buf);

    spin_lock_bh(&pool->lock);
    pool->free_cnt++;
#if MEMPOOL_DEBUG
    /* TBD: Check free count and whether buf address is within valid range if debug is enabled */
#endif
    STAILQ_INSERT_TAIL(&pool->free_list, (mempool_elem_t *)buf, mempool_entry);
    spin_unlock_bh(&pool->lock);
}

EXPORT_SYMBOL(__adf_mempool_init);
EXPORT_SYMBOL(__adf_mempool_destroy);
EXPORT_SYMBOL(__adf_mempool_alloc);
EXPORT_SYMBOL(__adf_mempool_free);

EXPORT_SYMBOL(adf_os_mem_alloc_outline);
EXPORT_SYMBOL(adf_os_mem_free_outline);
EXPORT_SYMBOL(adf_os_mem_zero_outline);
