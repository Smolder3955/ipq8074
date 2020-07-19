/*
 * Copyright (c) 2004 Atheros Communications, Inc.
 * All rights reserved.
 *
 */
/*
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _DEV_BUS_H_
#define _DEV_BUS_H_


#include <linux/kallsyms.h>
#include <ah_osdep.h>
#include <osdep_adf.h>

#define BUS_DMA_FROMDEVICE     0
#define BUS_DMA_TODEVICE       1

#ifdef ATH_PCI
void pci_read_cachesize(osdev_t, int *);
#ifdef ATH_BUS_PM
int pci_suspend(osdev_t);
int pci_resume(osdev_t);
#endif /* ATH_BUS_PM */
#else
#define pci_alloc_consistent(hwdev, size, dma_handle) NULL
#define pci_free_consistent(hwdev, size, vaddr, dma_handle)
#define pci_read_cachesize(osdev, csz)
#define pci_unmap_singl(hwdev, dma_addr, size,direction)
#define pci_map_single(hwdev, ptr, size, direction)
#define pci_suspend(osdev)
#define pci_resume(osdev)
#endif

#ifdef ATH_AHB
void *ahb_alloc_consistent(void *, size_t, dma_addr_t *);
void ahb_free_consistent(void *, size_t, void *, dma_addr_t);
void ahb_read_cachesize(osdev_t, int *);
#else
#define ahb_alloc_consistent(hwdev, size, dma_handle) NULL
#define ahb_free_consistent(hwdev, size, vaddr, dma_handle)
#define ahb_read_cachesize(osdev, csz)
#endif

#ifndef __ubicom32__
dma_addr_t bus_map_single(void *, void *, size_t, int);
#endif


#ifdef ATH_BUS_PM
int bus_device_suspend(osdev_t);
int bus_device_resume(osdev_t);
#endif /* ATH_BUS_PM */


extern const char * get_system_type(void);
#if defined(AR9100)
int valid_wmac_num(u_int16_t);
int get_wmac_irq(u_int16_t);
unsigned long get_wmac_base(u_int16_t);
unsigned long get_wmac_mem_len(u_int16_t);
#endif

#define sysRegRead(phys)      (*(volatile u_int32_t *)phys)

#endif    /* _DEV_BUS_H_ */
