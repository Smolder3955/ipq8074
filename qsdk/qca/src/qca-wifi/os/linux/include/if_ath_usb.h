/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _DEV_ATH_COMMON_H_
#define _DEV_ATH_COMMON_H_

static inline dma_addr_t bus_map_single(void *hwdev, void *ptr,
								 size_t size, int direction)
{
    return __pa(ptr);
}

static inline void bus_unmap_single(void *hwdev, dma_addr_t dma_addr,
			     size_t size, int direction)
{
}
static inline void bus_dma_sync_single(void *hwdev,
				dma_addr_t dma_handle,
				size_t size, int direction)
{
}

#define bus_alloc_consistent(a, b, c)    (c = (dma_addr_t *)kmalloc(b, GFP_KERNEL))
#define bus_free_consistent(a, b, c, d)  kfree(c)


#define BUS_DMA_FROMDEVICE	0
#define BUS_DMA_TODEVICE	0

#endif   /* _DEV_ATH_COMMON_H_ */
