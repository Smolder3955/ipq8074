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

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/slab.h>

#if LINUX_VERSION_CODE  <= KERNEL_VERSION(2,6,19)
#include <linux/vmalloc.h>
#endif

#if LINUX_VERSION_CODE  <= KERNEL_VERSION(3,3,8)
#include <asm/system.h>
#else
#if defined (__LINUX_MIPS32_ARCH__) || defined (__LINUX_MIPS64_ARCH__)
#include <asm/dec/system.h>
#else
#include <asm/system.h>
#endif
#endif

#include <asm/checksum.h>
#include <asm/io.h>

#include <linux/stddef.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/device.h>



#include <asf_queue.h>
#include <adf_os_pci.h>



typedef struct __adf_os_pci_drv{
    asf_list_entry(__adf_os_pci_drv)   mlist;
    adf_os_pci_drvinfo_t              *drv_info;
    struct pci_driver                  drv_pci;
    struct pci_device_id               id_tbl[ADF_OS_MAX_PCI_IDS];
}__adf_os_pci_drv_t;

#define pci_drv_info(_mod)   ((_mod)->drv_info)  

#define pci_drv_attach(_mod, _data, _osdev)       \
    pci_drv_info(_mod)->drv_attach(_data, _osdev)

#define pci_drv_detach(_mod, _drv_hdl)  \
    pci_drv_info(_mod)->drv_detach(_drv_hdl)
/**
 * List of drivers getting registered
 */
asf_list_head(__adf_drv_hnode, __adf_os_pci_drv) __adf_pci_drv_head;

/**
 * @brief return the module structure
 * @param[in] drv_name
 * 
 * @return __adf_os_pci_drv_t*
 */
static __adf_os_pci_drv_t *
__adf_os_pci_find_drv(char *drv_name)
{
    __adf_os_pci_drv_t *entry = NULL;

    asf_list_foreach(entry, &__adf_pci_drv_head, mlist) {
        if(!strcmp(entry->drv_info->mod_name, drv_name))
            break;
    }

    return entry;
}

/**
 * @brief allocate the driver structure
 * 
 * @param drv_info
 * 
 * @return __adf_os_pci_drv_t*
 */
static __adf_os_pci_drv_t *
__adf_os_pci_alloc_drv(adf_os_pci_drvinfo_t *drv_info)
{
    __adf_os_pci_drv_t  *elem;

    elem = vmalloc(sizeof(struct __adf_os_pci_drv));
    if(elem){
        memset(elem, 0, sizeof(struct __adf_os_pci_drv));
        elem->drv_info = drv_info;
        asf_list_insert_head(&__adf_pci_drv_head, elem, mlist);
    }
    return elem;
}

static void
__adf_os_pci_free_drv(__adf_os_pci_drv_t *drv)
{
    asf_list_remove(drv, mlist);
    vfree(drv);
}

/**
 * @brief Create a IO map in the CPU address space
 * 
 * @param os_res[in] (BUS IO map)
 * @param res[out] (CPU address space map for the driver)
 * 
 * @return int (status)
 */
static int
__adf_os_pci_setup_iomap(__adf_os_resource_t *os_res, adf_os_resource_t *res)
{
    os_res->vaddr = ioremap(os_res->paddr, os_res->len);
    if (!os_res->vaddr) {
        printk("cant map region\n");
        return EIO;
    }

    res->start = (uint32_t) os_res->vaddr;
    res->end   = res->start + os_res->len - 1;
    res->type  = ADF_OS_RESOURCE_TYPE_MEM;

    return 0;
}

/**
 * @brief release the IO map
 * @param osdev
 */
static inline void
__adf_os_pci_free_iomap(__adf_os_device_t osdev)
{
    iounmap((void __iomem *)osdev->res.vaddr);
}

/**
 * @brief PCI Bus specific setup
 * 
 * @param[in] pdev (PCI device)
 * @param[in] drv_name (driver name)
 * @param[in] ent (PCI device IDs)
 * @param[out] osdev (resource specific)
 * @param[out] data (driver data)
 * 
 * @return int
 */
static int
__adf_os_pci_setup_res(__adf_os_device_t osdev, char *drv_name, 
                        const struct pci_device_id *ent, 
                        adf_os_pci_devid_t *data)
{
    int err;
    struct pci_dev *pdev = NULL;

    pdev = to_pci_dev(osdev->dev);

    if((err = pci_enable_device(pdev)))
        return err;

    pci_set_master(pdev);

    if ((err = pci_request_region(pdev, ADF_OS_PCI_BAR_0, drv_name))){
        printk("ADF_PCI:request region failed\n");
        goto req_reg_fail;
    }

    osdev->res.paddr = pci_resource_start(pdev, ADF_OS_PCI_BAR_0);
    osdev->res.len   = pci_resource_len(pdev, ADF_OS_PCI_BAR_0);

    /**
     * device on the current system (driver supports a list of
     * devices)
     */
    data->vendor    = ent->vendor;
    data->device    = ent->device;
    data->subvendor = ent->subvendor;
    data->subdevice = ent->subdevice;
    /**
     * XXX:64bit
     */
    if ((err = pci_set_dma_mask(pdev, DMA_32BIT_MASK))) {
        printk("ADF_PCI:no usable DMA config\n");
        goto dma_fail;
    }

    return 0;
dma_fail:
    pci_release_region(pdev, ADF_OS_PCI_BAR_0);
req_reg_fail:
    pci_disable_device(pdev);
    return err;
}

/**
 * @brief release all PCI resources
 * @param pdev
 */

static void
__adf_os_pci_free_res(struct pci_dev *pdev)
{
    pci_release_region(pdev, ADF_OS_PCI_BAR_0);
    pci_disable_device(pdev);
}



static int
__adf_os_pci_attach(struct pci_dev *pdev, const struct pci_device_id *ent)
{
    __adf_os_device_t     osdev;
    __adf_os_pci_drv_t   *elem = NULL;
    adf_os_resource_t     res = {0};
    adf_os_pci_data_t     pci_data={{0}};
    struct pci_driver    *pdrv;
    int                   error = EIO;
    

    pdrv = to_pci_driver(pdev->dev.driver);
     if (pdrv == NULL) {
         printk("ADF_PCI:no driver found!\n");
         return EIO;
     }
    /**
     * Allocate the softc
     */
    osdev = vmalloc(sizeof(struct __adf_device));
    if(!osdev){
        printk("ADF_PCI: unable to allocate osdev \n");
        goto sc_fail;
    }

    memset(osdev, 0, sizeof(struct __adf_device));
    osdev->dev      = &pdev->dev;
    osdev->irq      = pdev->irq;
    osdev->drv_name = pdrv->name;

    pci_set_drvdata(pdev, osdev);
    
    if((error = __adf_os_pci_setup_res(osdev, osdev->drv_name, ent, &pci_data.pci_id))){
        printk("ADF_PCI:Bus setup failed\n");
        goto pci_fail;
    }

    if((error = __adf_os_pci_setup_iomap(&osdev->res, &res))){
        printk("ADF_PCI:resource mapping failed\n");
        goto pci_fail;
    }

    elem     = __adf_os_pci_find_drv(osdev->drv_name);
    osdev->drv = elem;

    adf_os_assert(pci_drv_info(elem));

    osdev->drv_hdl = pci_drv_attach(elem, &pci_data, osdev);

    if(!osdev->drv_hdl) {
        printk("ADF_PCI:attach failed \n");
        goto attach_fail;
    }
    return 0;

attach_fail:
    iounmap(osdev->res.vaddr);
pci_fail:
    vfree(osdev);
sc_fail:
    return error;
}
/**
 * @brief Detach the device
 * @param pdev
 */
static void
__adf_os_pci_detach(struct pci_dev *pdev)
{
    __adf_os_device_t osdev = pci_get_drvdata(pdev);
    __adf_os_pci_drv_t   *mod;
    
    adf_os_assert(osdev);
    adf_os_assert(osdev->drv);
    
    mod = (__adf_os_pci_drv_t *)osdev->drv;
    
    pci_drv_detach(mod, osdev->drv_hdl);

    __adf_os_pci_free_res(pdev);
    __adf_os_pci_free_iomap(osdev);
    vfree(osdev);
}




/**
 * @brief register the device driver as PCI driver
 * 
 * @param mod
 * @param pdrv
 * 
 * @return int
 */
static int
__adf_os_pci_register(__adf_os_pci_drv_t *drv, struct pci_driver *pdrv)
{
    struct pci_device_id *id_tbl = drv->id_tbl;
    adf_os_pci_devid_t  *pci_ids = drv->drv_info->pci_id;
    int i;

    /*
     * We have to copy the table, because linux' pci device id table has
     * some extra fields
     */
    for(i = 0; pci_ids[i].vendor ; i++) {
        id_tbl[i].vendor      = pci_ids[i].vendor;
        id_tbl[i].device      = pci_ids[i].device;
        id_tbl[i].subvendor   = pci_ids[i].subvendor;
        id_tbl[i].subdevice   = pci_ids[i].subdevice;
    }

    pdrv->name         = drv->drv_info->mod_name;
    pdrv->id_table     = id_tbl;
    pdrv->probe        = __adf_os_pci_attach;
    pdrv->remove       = __adf_os_pci_detach;

    return(pci_register_driver(pdrv));
}



/********************************EXPORTED***********************/

/**
 * @brief register a driver to the shim (PCI or any)
 * @param drv_info
 * 
 * @return a_status_t
 */
a_status_t
__adf_os_pci_drv_reg(adf_os_pci_drvinfo_t *drv_info)
{
    __adf_os_pci_drv_t   *drv;

    drv = __adf_os_pci_alloc_drv(drv_info);

    __adf_os_pci_register(drv, &drv->drv_pci);

    return A_STATUS_OK;
}

/**
 * @brief unregister the driver from shim
 * @param name
 */
void
__adf_os_pci_drv_unreg(uint8_t *name)
{
    __adf_os_pci_drv_t   *elem;

    elem = __adf_os_pci_find_drv(name);
    if (!elem) {
        printk("ADF_PCI:driver doesn't exist %s\n", name);
        return;
    }

    pci_unregister_driver(&elem->drv_pci);

    __adf_os_pci_free_drv(elem);
}

EXPORT_SYMBOL(__adf_os_pci_drv_reg);
EXPORT_SYMBOL(__adf_os_pci_drv_unreg);

