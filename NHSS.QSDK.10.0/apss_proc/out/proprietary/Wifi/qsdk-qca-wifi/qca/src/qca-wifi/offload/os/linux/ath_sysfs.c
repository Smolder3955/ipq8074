/*
 * Copyright (c) 2013,2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/*
 * User agent special file support for Linux systems.
 */

#include <linux/device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "ol_if_athvar.h"
#include "ol_ath.h"
#include "hif.h"
#include "hif_napi.h"
#include <osapi_linux.h>
#include <osif_private.h>
#include <ath_pci.h>
#include <init_deinit_lmac.h>

#if defined(CONFIG_ATH_SYSFS_DIAG_SUPPORT)
/* Diagnostic access to Target space. */

static ssize_t
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
ath_sysfs_diag_write(struct file *filp, struct kobject *kobj,
        struct bin_attribute *bin_attr, char *buf, loff_t pos, size_t count)
#else
ath_sysfs_diag_write(struct kobject *kobj, struct bin_attribute *bin_attr,
        char *buf, loff_t pos, size_t count)
#endif
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *)(bin_attr->private);
    int rv;
    struct common_hif_handle *hif_hdl = NULL;

    hif_hdl = lmac_get_hif_hdl(soc->psoc_obj);

    if (hif_hdl == NULL) {
        return -EINVAL;
    }

    if ((count == 4) && (((A_UINT32)pos & 3) == 0)) { /* reading a word? */
        A_UINT32 value = *((A_UINT32 *)buf);
        rv = hif_diag_write_access((struct hif_opaque_softc *)hif_hdl, (A_UINT32)pos, value);
    } else {
        rv = hif_diag_write_mem((struct hif_opaque_softc *)hif_hdl, (A_UINT32)pos, (A_UINT8 *)buf, count);
    }

    if (rv == 0) {
        return count;
    } else {
        return -EIO;
    }
}

static ssize_t
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,35)
ath_sysfs_diag_read(struct file *filp, struct kobject *kobj,
        struct bin_attribute *bin_attr, char *buf, loff_t pos, size_t count)
#else
ath_sysfs_diag_read(struct kobject *kobj, struct bin_attribute *bin_attr,
        char *buf, loff_t pos, size_t count)
#endif
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *)(bin_attr->private);
    int rv;
    struct common_hif_handle *hif_hdl = NULL;

    hif_hdl = lmac_get_hif_hdl(soc->psoc_obj);

    if (hif_hdl == NULL) {
        return -EINVAL;
    }

    if ((count == 4) && (((A_UINT32)pos & 3) == 0)) { /* reading a word? */
        rv = hif_diag_read_access((struct hif_opaque_softc *)hif_hdl, (A_UINT32)pos, (A_UINT32 *)buf);
    } else {
        rv = hif_diag_read_mem((struct hif_opaque_softc *)hif_hdl, (A_UINT32)pos, (A_UINT8 *)buf, count);
    }

    if (rv == 0) {
        return count;
    } else {
        return -EIO;
    }
}

void
ath_sysfs_diag_init(struct ol_ath_soc_softc *soc)
{
    struct bin_attribute *diag_fsattr;
    int ret;

    diag_fsattr = OS_MALLOC(soc->sc_osdev, sizeof(*diag_fsattr), GFP_KERNEL);
    if (!diag_fsattr) {
        printk("%s: Memory allocation failed\n", __func__);
		return;
    }
    OS_MEMZERO(diag_fsattr, sizeof(*diag_fsattr));

    diag_fsattr->attr.name = "athdiag";
    diag_fsattr->attr.mode = 0600;
    diag_fsattr->read = ath_sysfs_diag_read;
    diag_fsattr->write = ath_sysfs_diag_write;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,34)
    sysfs_bin_attr_init(diag_fsattr);
#endif
    ret = sysfs_create_bin_file(&soc->sc_osdev->device->kobj, diag_fsattr);
    if (ret) {
        printk("%s: sysfs create failed\n", __func__);
        OS_FREE(diag_fsattr);
        soc->diag_ol_priv = NULL;
        return;
    }

    diag_fsattr->private = soc;
    soc->diag_ol_priv = (void *)diag_fsattr;
}

void
ath_sysfs_diag_fini(ol_ath_soc_softc_t *soc)
{
    struct bin_attribute *diag_fsattr = (struct bin_attribute *)soc->diag_ol_priv;

    if (diag_fsattr) {
        sysfs_remove_bin_file(&soc->sc_osdev->device->kobj, diag_fsattr);
        soc->diag_ol_priv = NULL;
        OS_FREE(diag_fsattr);
    }
}
#endif /* CONFIG_ATH_SYSFS_DIAG_SUPPORT */


#if defined(CONFIG_ATH_SYSFS_CE)
#error
/*
 * Provide user agent access to Copy Engine hardware in order to transfer data
 * between an application on the Target and a user-level application on the Host.
 * NB: Used by endpointping test
 */

/* Maximum length of syfs CE data */
size_t ath_ce_msgsz_max = 1024;
module_param(ath_ce_msgsz_max, int, 0644);


/* Destination ring size */
int ath_ce_recv_cnt = 32;
module_param(ath_ce_recv_cnt, int, 0644);

/* Source ring size */
int ath_ce_send_cnt = 32;
module_param(ath_ce_send_cnt, int, 0644);

#define kobj_to_dev(k)          container_of((k), struct device, kobj)

/* Pass a data from the Target to a Host-side user application over CE */
static ssize_t
ath_sysfs_CE_read(struct file *file, struct kobject *kobj,
                   struct bin_attribute *bin_attr,
                   char *buf, loff_t pos, size_t count)
{
    struct device *dev = kobj_to_dev(kobj);
    struct ol_ath_softc *sc = dev_get_drvdata(dev);
    unsigned long irq_flags;
    struct ath_sysfs_buf_list *recv_item;
    size_t nbytes;

    count = min(count, ath_ce_msgsz_max);

    /* Wait until a buffer has been received */
    if (down_interruptible(&sc->CE_recvdata_sem)) {
        return -EINTR;
    }

    spin_lock_irqsave(&sc->CE_recvq_lock, irq_flags);
    recv_item = sc->CE_recvq_head;
    BUG_ON(!recv_item);
    sc->CE_recvq_head = recv_item->next;
    if (sc->CE_recvq_head == NULL) {
        sc->CE_recvq_tail = NULL;
    }
    spin_unlock_irqrestore(&sc->CE_recvq_lock, irq_flags);
        recv_item->next = NULL; /* sanity */

    nbytes = min(count, recv_item->nbytes);
    A_MEMCPY(buf, recv_item->host_data, nbytes);

    /* Enqueue recv buf back to Copy Engine */
    dma_cache_sync(dev, recv_item->host_data, ath_ce_msgsz_max, DMA_FROM_DEVICE);
    CE_recv_buf_enqueue(sc->sysfs_copyeng_recv, recv_item, recv_item->CE_data);

    return nbytes;
}

/* Send completion - recycle the buffer */
void
ath_sysfs_CE_send_done(struct CE_handle *copyeng, void *ce_context, void *transfer_context,
    CE_addr_t data, unsigned int nbytes, unsigned int transfer_id)
{
    struct ol_ath_softc *sc = (struct ol_ath_softc *)ce_context;
    struct ath_sysfs_buf_list *send_item = (struct ath_sysfs_buf_list *)transfer_context;
    unsigned long irq_flags;

    BUG_ON(data != send_item->CE_data);

    send_item->next = NULL;

    /* Enqueue send buf back to free list */
    spin_lock_irqsave(&sc->CE_sendq_lock, irq_flags);
    if (sc->CE_sendq_freehead) {
        sc->CE_sendq_freetail->next = send_item;
    } else {
        sc->CE_sendq_freehead = send_item;
    }
    sc->CE_sendq_freetail = send_item;
    spin_unlock_irqrestore(&sc->CE_sendq_lock, irq_flags);
    up(&sc->CE_sendbuf_sem);
}

/* Send data from a Host-side user application to the Target via CE */
static ssize_t
ath_sysfs_CE_write(struct file *file, struct kobject *kobj,
                   struct bin_attribute *bin_attr,
                   char *buf, loff_t pos, size_t count)
{
    struct device *dev = kobj_to_dev(kobj);
    struct ol_ath_softc *sc = dev_get_drvdata(dev);
    struct ath_sysfs_buf_list *send_item;
    unsigned long irq_flags;
    int status;

    count = min(count, ath_ce_msgsz_max);

    /* Wait for a free buffer */
    if (down_interruptible(&sc->CE_sendbuf_sem)) {
        return -EINTR;
    }

    spin_lock_irqsave(&sc->CE_sendq_lock, irq_flags);
    send_item = sc->CE_sendq_freehead;
    BUG_ON(!send_item);
    sc->CE_sendq_freehead = send_item->next;
    /* NB: Allow stale sendq_freetail */
    spin_unlock_irqrestore(&sc->CE_sendq_lock, irq_flags);
    send_item->next = NULL; /* sanity */

    A_MEMCPY(send_item->host_data, buf, count);
    dma_cache_sync(dev, send_item->host_data, count, DMA_TO_DEVICE);
    status = CE_send(sc->sysfs_copyeng_send, send_item, send_item->CE_data, count, 0, 0);
    /*
     * The number of send buffers is one less than source ring size,
     * so CE_send should always succeed.
     */
    BUG_ON(status != 0);

    return count;
}

static struct bin_attribute CE_fsattr = {
    .attr = {.name = "ce", .mode = 0600},
    .read = ath_sysfs_CE_read,
    .write = ath_sysfs_CE_write,
};

void
ath_sysfs_CE_recv_data(struct CE_handle *copyeng, void *ce_context, void *transfer_context,
    CE_addr_t data, unsigned int nbytes, unsigned int transfer_id, unsigned int flags)
{
    struct ol_ath_softc *sc = (struct ol_ath_softc *)ce_context;
    struct ath_sysfs_buf_list *recv_item = (struct ath_sysfs_buf_list *)transfer_context;
    unsigned long irq_flags;

    BUG_ON(data != recv_item->CE_data);

    recv_item->nbytes = nbytes;
    recv_item->next = NULL;

    spin_lock_irqsave(&sc->CE_recvq_lock, irq_flags);
    if (sc->CE_recvq_head == NULL) {
        sc->CE_recvq_head = recv_item;
    } else {
        sc->CE_recvq_tail->next = recv_item;
    }
    sc->CE_recvq_tail = recv_item;
    spin_unlock_irqrestore(&sc->CE_recvq_lock, irq_flags);

    up(&sc->CE_recvdata_sem);
}

/* Create sysfs file to support Copy Engine */
int
ath_sysfs_CE_init(struct ol_ath_softc *sc)
{
    struct device *dev = sc->dev;
    struct ath_sysfs_buf_list *item, *first_item, *last_item;
    struct CE_handle *copyeng_send, *copyeng_recv;
    struct CE_attr attr;
    int ret;
    int i;

    sema_init(&sc->CE_recvdata_sem, 0);
    sema_init(&sc->CE_sendbuf_sem, 0);

    /* Initialize recv Copy Engine */
    attr.flags = 0;
    attr.src_nentries = 0;
    attr.src_sz_max = ath_ce_msgsz_max;
    attr.dest_nentries = ath_ce_recv_cnt;
    copyeng_recv = CE_init(sc, 0, &attr); /* Copy Engine 0 for RECV */
    sc->sysfs_copyeng_recv = copyeng_recv;

    /* Establish a handler for received data */
    CE_recv_cb_register(copyeng_recv, ath_sysfs_CE_recv_data, sc);

    /* Allocate and enqueue receive buffers */
    spin_lock_init(&sc->CE_recvq_lock);
    sc->CE_recvq_head = NULL;
    sc->CE_recvq_tail = NULL;
    item = qdf_mem_malloc((ath_ce_recv_cnt-1)*sizeof(*item));
    for (i=0; i<ath_ce_recv_cnt-1; i++) {
        item->host_data = qdf_mem_malloc(ath_ce_msgsz_max);
        item->CE_data = dma_map_single(dev, item->host_data, ath_ce_msgsz_max, DMA_FROM_DEVICE);
        if (DMA_MAPPING_ERROR(dev, item->CE_data)) {
            return -ENOMEM; /* TBDXXX */
        }
        dma_cache_sync(dev, item->host_data, ath_ce_msgsz_max, DMA_FROM_DEVICE);
        CE_recv_buf_enqueue(copyeng_recv, item, item->CE_data);
        item++;
    }

    /* Initialize send Copy Engine */
    attr.flags = 0;
    attr.src_nentries = ath_ce_send_cnt;
    attr.src_sz_max = ath_ce_msgsz_max;
    attr.dest_nentries = 0;
    copyeng_send = CE_init(sc, 1, &attr); /* Copy Engine 1 for SEND */
    sc->sysfs_copyeng_send = copyeng_send;

    /* Establish a handler for send completions */
    CE_send_cb_register(copyeng_send, ath_sysfs_CE_send_done, sc);

    /* Allocate send buffer free list */
    spin_lock_init(&sc->CE_sendq_lock);
    last_item = NULL;
    first_item = NULL;
    item = qdf_mem_malloc((ath_ce_send_cnt-1)*sizeof(*item));
    for (i=0; i<ath_ce_send_cnt-1; i++) {
        if (!first_item) {
            first_item = item;
        }
        if (last_item) {
            last_item->next = item;
        }
        item->host_data = qdf_mem_malloc(ath_ce_msgsz_max);
        item->CE_data = dma_map_single(dev, item->host_data, ath_ce_msgsz_max, DMA_TO_DEVICE);
        if (DMA_MAPPING_ERROR(dev, item->CE_data)) {
            return -ENOMEM; /* TBDXXX */
        }
        item->nbytes = 0;
        last_item = item;
        up(&sc->CE_sendbuf_sem);
        item++;
    }
    last_item->next = NULL; /* terminate free list */
    sc->CE_sendq_freehead = first_item;
    sc->CE_sendq_freetail = last_item;

    /* Finally, make CE pipe accessible to applications */
    ret = sysfs_create_bin_file(&sc->dev->kobj, &CE_fsattr);
    BUG_ON(ret);

    return ret;
}

void
ath_sysfs_CE_finish(struct ol_ath_softc *sc)
{
    sysfs_remove_bin_file(&sc->dev->kobj, &CE_fsattr);

    /* TBDXXX: free buffers */
}

#endif /* CONFIG_ATH_SYSFS_CE */
