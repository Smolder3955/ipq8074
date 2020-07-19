/*
 * Copyright (c) 2013, 2016-2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013,2016 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/pci.h>
#include <asm/hardirq.h>
#include <linux/slab.h>
#include "mem_manager.h"

unsigned int mm_dbg_enable = 0x0;
module_param(mm_dbg_enable, uint, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
MODULE_PARM_DESC(mm_dbg_enable,"mm_dbg enable");
EXPORT_SYMBOL(mm_dbg_enable);

struct kmem_obj_s g_kmem_obj[SOC_IDMAX][RADIO_IDMAX][KM_MAXREQ];
struct cmem_obj_s g_cmem_obj[SOC_IDMAX][RADIO_IDMAX][CM_MAXREQ];

/* Alloates kmalloc memory once and will use the same for subsequent allocataions for the radio and type of allocation */
void* __wifi_kmem_allocation(int soc_id, int radio_id, int type, int size, int flags)
{
    struct kmem_obj_s *kmem_obj;
    MM_DEBUG_PRINT(MM_DEBUG_LVL1," %s SocId %d: RadioId: %d type %d, size %d, flags %x\n", __func__, soc_id, radio_id, type, size, flags);

    if ((type >= KM_MAXREQ) || (radio_id >= RADIO_IDMAX) || (soc_id >= SOC_IDMAX)) {
        return NULL;
    }

    kmem_obj = &g_kmem_obj[soc_id][radio_id][type];

    if (kmem_obj->addr != NULL) {
        MM_DEBUG_PRINT(MM_DEBUG_LVL2," %s already allocated size %zu\n", __func__, kmem_obj->size);
        if (kmem_obj->size >= size) {
            return kmem_obj->addr;
        } else {
            /* handle error condition */
            MM_DEBUG_PRINT(MM_DEBUG_LVL1," %s size req %d size prev allocated %zu", __func__, size,kmem_obj->size);
            kfree(kmem_obj->addr);
            kmem_obj->size = 0;
        }
    }
    MM_DEBUG_PRINT(MM_DEBUG_LVL2," %s new allocation size %d\n", __func__, size);

    kmem_obj->addr = kmalloc(size, flags);
    if (kmem_obj->addr) {
        kmem_obj->size = size;
    }

    return kmem_obj->addr;
}

/* validates address that is getting freed but wont free the memory */
void __wifi_kmem_free(int soc_id, int radio_id, int type, void *addr)
{
    struct kmem_obj_s *kmem_obj;

    if ((type >= KM_MAXREQ) || (radio_id >= RADIO_IDMAX) || (soc_id >= SOC_IDMAX)) {
        MM_DEBUG_PRINT(MM_DEBUG_LVL1," %s unsupported type %d ", __func__, type);
        return;
    }

    kmem_obj = &g_kmem_obj[soc_id][radio_id][type];

    MM_DEBUG_PRINT(MM_DEBUG_LVL1," %s SocId %d: RadioId: %d type %d, addr %pK\n", __func__, soc_id, radio_id,type,addr);
    if (kmem_obj->addr != addr) {
        MM_DEBUG_PRINT(MM_DEBUG_LVL1," %s Error addr req %pK addr prev allocated %pK", __func__, addr,kmem_obj->addr);
    }
}

/* Alloates dma memory once and will use the same for subsequent allocataions for the radio and type of allocation */
void* __wifi_cmem_allocation(int soc_id, int radio_id, int type, int size, void *pdev, dma_addr_t *paddr, int intr_ctxt)
{
    struct pci_dev *pcidev;
    int flags = GFP_KERNEL;
    struct cmem_obj_s *cmem_obj;

    MM_DEBUG_PRINT(MM_DEBUG_LVL1," %s SocId %d: RadioId: %d type %d, size %d, pdev %pK, intr_ctxt %d\n", __func__, soc_id, radio_id,type,size,pdev,intr_ctxt);

    pcidev = (struct pci_dev *)pdev;

    if ((type >= CM_MAXREQ) || (radio_id >= RADIO_IDMAX) || (soc_id >= SOC_IDMAX)) {
        return NULL;
    }

    if(intr_ctxt) {
        flags = GFP_ATOMIC;
    }

    cmem_obj = &g_cmem_obj[soc_id][radio_id][type];

    if (cmem_obj->vaddr != NULL) {
        MM_DEBUG_PRINT(MM_DEBUG_LVL2," %s already allocated size %zu\n", __func__,cmem_obj->size);
        if (cmem_obj->size >= size) {
            *paddr = cmem_obj->paddr;
            return  cmem_obj->vaddr;
        } else {
            /* handle error condition */
            MM_DEBUG_PRINT(MM_DEBUG_LVL1," %s size req %d size prev allocated %zu\n", __func__, size,cmem_obj->size);
            dma_free_coherent(pcidev == NULL ? NULL : &(pcidev->dev),cmem_obj->size , cmem_obj->vaddr, cmem_obj->paddr);
            cmem_obj->size = 0;
        }
    }

    MM_DEBUG_PRINT(MM_DEBUG_LVL2," %s new allocation size %d\n", __func__, size);

    cmem_obj->vaddr = (void *)dma_alloc_coherent(pcidev == NULL ? NULL : &(pcidev->dev), size, (dma_addr_t *)&cmem_obj->paddr, flags);
    if (cmem_obj->vaddr) {
        cmem_obj->size = size;
        *paddr = cmem_obj->paddr;
    }

    return cmem_obj->vaddr;
}

/* validates address that is getting freed but wont free the memory */
void __wifi_cmem_free(int soc_id, int radio_id, int type, void *addr)
{
    struct cmem_obj_s *cmem_obj;

    MM_DEBUG_PRINT(MM_DEBUG_LVL1," %s SocId %d: RadioId: %d type %d, addr %pK\n", __func__, soc_id, radio_id,type,addr);

    if ((type >= CM_MAXREQ) || (radio_id >= RADIO_IDMAX) || (soc_id >= SOC_IDMAX)) {
        MM_DEBUG_PRINT(MM_DEBUG_LVL1," %s unsupported type %d ", __func__, type);
        return;
    }
    cmem_obj = &g_cmem_obj[soc_id][radio_id][type];

    if (cmem_obj->vaddr != addr) {
        MM_DEBUG_PRINT(MM_DEBUG_LVL1," %s Error addr req %pK addr prev allocated %pK", __func__, addr,cmem_obj->vaddr);
    }
}

static int __mm_init_module(void)
{
    int i ,j ,k;

    for (i = 0; i < SOC_IDMAX; i++) {
        for (j = 0; j < RADIO_IDMAX; j++) {
            for (k = 0; k < KM_MAXREQ; k++) {
                g_kmem_obj[i][j][k].addr = NULL;
                g_kmem_obj[i][j][k].size = 0;
            }
        }
    }

    for (i = 0; i < SOC_IDMAX; i++) {
        for (j = 0; j < RADIO_IDMAX; j++) {
            for (k = 0; k < CM_MAXREQ; k++) {
                g_cmem_obj[i][j][k].paddr = 0;
                g_cmem_obj[i][j][k].vaddr = NULL;
                g_cmem_obj[i][j][k].size = 0;
            }
        }
    }

    printk("%s \n", __func__);
    return 0;
}

static void __mm_exit_module(void)
{
    int i, j, k;

    for (i = 0; i < SOC_IDMAX; i++) {
        for (j = 0; j < RADIO_IDMAX; j++) {
            for (k = 0; k < KM_MAXREQ; k++) {
                if(g_kmem_obj[i][j][k].addr != NULL) {
                    kfree(g_kmem_obj[i][j][k].addr);
                }
            }
        }
    }

    for (i = 0; i < SOC_IDMAX; i++) {
        for (j = 0; j < RADIO_IDMAX; j++) {
            for (k = 0; k < CM_MAXREQ; k++) {
                if(g_cmem_obj[i][j][k].vaddr != NULL) {
                    pci_free_consistent(NULL, g_cmem_obj[i][j][k].size, g_cmem_obj[i][j][k].vaddr, g_cmem_obj[i][j][k].paddr);
                }
            }
        }
    }

    printk("%s \n", __func__);
}

EXPORT_SYMBOL(__wifi_kmem_allocation);
EXPORT_SYMBOL(__wifi_kmem_free);
EXPORT_SYMBOL(__wifi_cmem_allocation);
EXPORT_SYMBOL(__wifi_cmem_free);

module_init(__mm_init_module);
module_exit(__mm_exit_module);
