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
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <adf_os_types.h>
#include <adf_nbuf.h>


/*
 * @brief This allocates an nbuf aligns if needed and reserves
 *        some space in the front, since the reserve is done
 *        after alignment the reserve value if being unaligned
 *        will result in an unaligned address.
 *
 * @param hdl
 * @param size
 * @param reserve
 * @param align
 *
 * @return nbuf or NULL if no memory
 */
struct sk_buff *
__adf_nbuf_alloc(adf_os_device_t osdev, size_t size, int reserve, int align, int prio)
{
    struct sk_buff *skb;
    unsigned long offset;

#if defined(USE_MULTIPLE_BUFFER_RCV) && USE_MULTIPLE_BUFFER_RCV
    size_t original_size = size;
#endif

    if(align)
        size += (align - 1);

    skb = dev_alloc_skb(size);

    if (!skb) {
        if (printk_ratelimit()) {
            printk("ERROR:NBUF alloc failed\n");
        }
        return NULL;
    }
    memset(skb->cb, 0x0, sizeof(skb->cb));

    /*
     * The default is for netbuf fragments to be interpreted
     * as wordstreams rather than bytestreams.
     * Set the CVG_NBUF_MAX_EXTRA_FRAGS+1 wordstream_flags bits,
     * to provide this default.
     */
    NBUF_EXTRA_FRAG_WORDSTREAM_FLAGS(skb) =
        (1 << (CVG_NBUF_MAX_EXTRA_FRAGS + 1)) - 1;

    /**
     * XXX:how about we reserve first then align
     */

    /**
     * Align & make sure that the tail & data are adjusted properly
     */
    if(align){
        offset = ((unsigned long) skb->data) % align;
        if(offset)
            skb_reserve(skb, align - offset);
    }

    /**
     * NOTE:alloc doesn't take responsibility if reserve unaligns the data
     * pointer
     */
    skb_reserve(skb, reserve);

#if defined(USE_MULTIPLE_BUFFER_RCV) && USE_MULTIPLE_BUFFER_RCV
    N_BUF_SIZE_SET(skb, original_size);
#endif

    return skb;
}

/*
 * @brief free the nbuf its interrupt safe
 * @param skb
 */
void
__adf_nbuf_free(struct sk_buff *skb)
{
    dev_kfree_skb_any(skb);
}


/*
 * @brief Reference the nbuf so it can get held until the last free.
 * @param skb
 */

void
__adf_nbuf_ref(struct sk_buff *skb)
{
    skb_get(skb);
}

/**
 *  @brief Check whether the buffer is shared
 *  @param skb: buffer to check
 *
 *  Returns true if more than one person has a reference to this
 *  buffer.
 */
int
__adf_nbuf_shared(struct sk_buff *skb)
{
    return skb_shared(skb);
}
/**
 * @brief create a nbuf map
 * @param osdev
 * @param dmap
 *
 * @return a_status_t
 */
a_status_t
__adf_nbuf_dmamap_create(adf_os_device_t osdev, __adf_os_dma_map_t *dmap)
{
    a_status_t error = A_STATUS_OK;
    /**
     * XXX: driver can tell its SG capablity, it must be handled.
     * XXX: Bounce buffers if they are there
     */
    (*dmap) = kzalloc(sizeof(struct __adf_os_dma_map), GFP_KERNEL);
    if(!(*dmap))
        error = A_STATUS_ENOMEM;

    return error;
}

/**
 * @brief free the nbuf map
 *
 * @param osdev
 * @param dmap
 */
void
__adf_nbuf_dmamap_destroy(adf_os_device_t osdev, __adf_os_dma_map_t dmap)
{
    kfree(dmap);
}

/**
 * @brief get the dma map of the nbuf
 *
 * @param osdev
 * @param bmap
 * @param skb
 * @param dir
 *
 * @return a_status_t
 */
a_status_t
__adf_nbuf_map(
    adf_os_device_t osdev,
    struct sk_buff *skb,
    adf_os_dma_dir_t dir)
{
#ifdef ADF_OS_DEBUG
    struct skb_shared_info  *sh = skb_shinfo(skb);
#endif
    adf_os_assert(
        (dir == ADF_OS_DMA_TO_DEVICE) || (dir == ADF_OS_DMA_FROM_DEVICE));

    /*
     * Assume there's only a single fragment.
     * To support multiple fragments, it would be necessary to change
     * adf_nbuf_t to be a separate object that stores meta-info
     * (including the bus address for each fragment) and a pointer
     * to the underlying sk_buff.
     */
    adf_os_assert(sh->nr_frags == 0);

    return __adf_nbuf_map_single(osdev, skb, dir);

#if 0
    {
        int i;
        adf_os_assert(sh->nr_frags <= __ADF_OS_MAX_SCATTER);

        for (i = 1; i <= sh->nr_frags; i++) {
            skb_frag_t *f           = &sh->frags[i-1]
            NBUF_MAPPED_FRAG_PADDR_LO(buf, i) = dma_map_page(
                osdev->dev, f->page, f->page_offset, f->size, dir);
        }
        adf_os_assert(sh->frag_list == NULL);
    }
#endif
    return A_STATUS_OK;
}

/**
 * @brief get the dma map of the nbuf
 *
 * @param osdev
 * @param bmap
 * @param skb
 * @param dir
 * @param nbytes
 *
 * @return a_status_t
 */
a_status_t
__adf_nbuf_map_nbytes(
    adf_os_device_t osdev,
    struct sk_buff *skb,
    adf_os_dma_dir_t dir,
    int nbytes)
{
#ifdef ADF_OS_DEBUG
    struct skb_shared_info  *sh = skb_shinfo(skb);
#endif
    adf_os_assert(
        (dir == ADF_OS_DMA_TO_DEVICE) || (dir == ADF_OS_DMA_FROM_DEVICE));

    /*
     * Assume there's only a single fragment.
     * To support multiple fragments, it would be necessary to change
     * adf_nbuf_t to be a separate object that stores meta-info
     * (including the bus address for each fragment) and a pointer
     * to the underlying sk_buff.
     */
    adf_os_assert(sh->nr_frags == 0);

    return __adf_nbuf_map_nbytes_single(osdev, skb, dir, nbytes);

#if 0
    {
        int i;
        adf_os_assert(sh->nr_frags <= __ADF_OS_MAX_SCATTER);

        for (i = 1; i <= sh->nr_frags; i++) {
            skb_frag_t *f           = &sh->frags[i-1]
            NBUF_MAPPED_FRAG_PADDR_LO(buf, i) = dma_map_page(
                osdev->dev, f->page, f->page_offset, f->size, dir);
        }
        adf_os_assert(sh->frag_list == NULL);
    }
    return A_STATUS_OK;
#endif
}


/**
 * @brief adf_nbuf_unmap() - to unmap a previously mapped buf
 */
void
__adf_nbuf_unmap(
    adf_os_device_t osdev,
    struct sk_buff *skb,
    adf_os_dma_dir_t dir)
{
    adf_os_assert(
        (dir == ADF_OS_DMA_TO_DEVICE) || (dir == ADF_OS_DMA_FROM_DEVICE));

    adf_os_assert(((dir == ADF_OS_DMA_TO_DEVICE) || (dir == ADF_OS_DMA_FROM_DEVICE)));
    /*
     * Assume there's a single fragment.
     * If this is not true, the assertion in __adf_nbuf_map will catch it.
     */
    __adf_nbuf_unmap_single(osdev, skb, dir);
}

/**
 * @brief adf_nbuf_unmap() - to unmap a previously mapped buf
 */
void
__adf_nbuf_unmap_nbytes(
    adf_os_device_t osdev,
    struct sk_buff *skb,
    adf_os_dma_dir_t dir,
    int nbytes)
{
    adf_os_assert(
        (dir == ADF_OS_DMA_TO_DEVICE) || (dir == ADF_OS_DMA_FROM_DEVICE));

    adf_os_assert(((dir == ADF_OS_DMA_TO_DEVICE) || (dir == ADF_OS_DMA_FROM_DEVICE)));
    /*
     * Assume there's a single fragment.
     * If this is not true, the assertion in __adf_nbuf_map will catch it.
     */
    __adf_nbuf_unmap_nbytes_single(osdev, skb, dir, nbytes);
}

a_status_t
__adf_nbuf_map_single(
    adf_os_device_t osdev, adf_nbuf_t buf, adf_os_dma_dir_t dir)
{
    u_int32_t paddr_lo;

/* tempory hack for simulation */
#ifdef A_SIMOS_DEVHOST
    NBUF_MAPPED_PADDR_LO(buf) = paddr_lo = (u_int32_t) buf->data;
    return A_STATUS_OK;
#else
    /* assume that the OS only provides a single fragment */
    NBUF_MAPPED_PADDR_LO(buf) = paddr_lo =
        dma_map_single(osdev->dev, buf->data,
                       buf->end - buf->data, dir);
    return dma_mapping_error(osdev->dev, paddr_lo) ?
        A_STATUS_FAILED : A_STATUS_OK;
#endif
}

a_status_t
__adf_nbuf_map_nbytes_single(
    adf_os_device_t osdev, adf_nbuf_t buf, adf_os_dma_dir_t dir, int nbytes)
{
    u_int32_t paddr_lo;

/* tempory hack for simulation */
#ifdef A_SIMOS_DEVHOST
    NBUF_MAPPED_PADDR_LO(buf) = paddr_lo = (u_int32_t) buf->data;
    return A_STATUS_OK;
#else
    /* assume that the OS only provides a single fragment */
    NBUF_MAPPED_PADDR_LO(buf) = paddr_lo =
        dma_map_single(osdev->dev, buf->data,
                       nbytes, dir);
    return dma_mapping_error(osdev->dev, paddr_lo) ?
        A_STATUS_FAILED : A_STATUS_OK;
#endif
}

void
__adf_nbuf_unmap_single(
    adf_os_device_t osdev, adf_nbuf_t buf, adf_os_dma_dir_t dir)
{
#if !defined(A_SIMOS_DEVHOST)
    if (0 ==  NBUF_MAPPED_PADDR_LO(buf)) {
        printk("ERROR: NBUF mapped physical address is NULL\n");
        return;
    }
    dma_unmap_single(osdev->dev, NBUF_MAPPED_PADDR_LO(buf),
                     buf->end - buf->data, dir);
#endif
}

void
__adf_nbuf_unmap_nbytes_single(
    adf_os_device_t osdev, adf_nbuf_t buf, adf_os_dma_dir_t dir, int nbytes)
{
#if !defined(A_SIMOS_DEVHOST)
    if (0 ==  NBUF_MAPPED_PADDR_LO(buf)) {
        printk("ERROR: NBUF mapped physical address is NULL\n");
        return;
    }
    dma_unmap_single(osdev->dev, NBUF_MAPPED_PADDR_LO(buf),
                     nbytes, dir);
#endif
}

/**
 * @brief return the dma map info
 *
 * @param[in]  bmap
 * @param[out] sg (map_info ptr)
 */
void
__adf_nbuf_dmamap_info(__adf_os_dma_map_t bmap, adf_os_dmamap_info_t *sg)
{
    adf_os_assert(bmap->mapped);
    adf_os_assert(bmap->nsegs <= ADF_OS_MAX_SCATTER);

    memcpy(sg->dma_segs, bmap->seg, bmap->nsegs *
           sizeof(struct __adf_os_segment));
    sg->nsegs = bmap->nsegs;
}
/**
 * @brief return the frag data & len, where frag no. is
 *        specified by the index
 *
 * @param[in] buf
 * @param[out] sg (scatter/gather list of all the frags)
 *
 */
void
__adf_nbuf_frag_info(struct sk_buff *skb, adf_os_sglist_t  *sg)
{
#if defined(ADF_OS_DEBUG) || defined(__ADF_SUPPORT_FRAG_MEM)
    struct skb_shared_info  *sh = skb_shinfo(skb);
#endif
    adf_os_assert(skb != NULL);
    sg->sg_segs[0].vaddr = skb->data;
    sg->sg_segs[0].len   = skb->len;
    sg->nsegs            = 1;

#ifndef __ADF_SUPPORT_FRAG_MEM
    adf_os_assert(sh->nr_frags == 0);
#else
    for(int i = 1; i <= sh->nr_frags; i++) {
        skb_frag_t    *f        = &sh->frags[i - 1];
        sg->sg_segs[i].vaddr    = (uint8_t *)(page_address(f->page) +
                                  f->page_offset);
        sg->sg_segs[i].len      = f->size;

        adf_os_assert(i < ADF_OS_MAX_SGLIST);
    }
    sg->nsegs += i;
#endif
}

a_status_t
__adf_nbuf_set_rx_cksum(struct sk_buff *skb, adf_nbuf_rx_cksum_t *cksum)
{
    switch (cksum->l4_result) {
    case ADF_NBUF_RX_CKSUM_NONE:
        skb->ip_summed = CHECKSUM_NONE;
        break;
    case ADF_NBUF_RX_CKSUM_TCP_UDP_UNNECESSARY:
        skb->ip_summed = CHECKSUM_UNNECESSARY;
        break;
    case ADF_NBUF_RX_CKSUM_TCP_UDP_HW:
#if LINUX_VERSION_CODE <= KERNEL_VERSION(2,6,19)
        skb->ip_summed = CHECKSUM_HW;
#else
        skb->ip_summed = CHECKSUM_PARTIAL;
#endif
        skb->csum      = cksum->val;
        break;
    default:
        printk("ADF_NET:Unknown checksum type\n");
        adf_os_assert(0);
	return A_STATUS_ENOTSUPP;
    }
    return A_STATUS_OK;
}

adf_nbuf_tx_cksum_t
__adf_nbuf_get_tx_cksum(struct sk_buff *skb)
{
    switch (skb->ip_summed) {
    case CHECKSUM_NONE:
        return ADF_NBUF_TX_CKSUM_NONE;
    case CHECKSUM_PARTIAL:
        /* XXX ADF and Linux checksum don't map with 1-to-1. This is not 100%
         * correct. */
        return ADF_NBUF_TX_CKSUM_TCP_UDP;
    default:
        return ADF_NBUF_TX_CKSUM_NONE;
    }
}

a_status_t
__adf_nbuf_get_vlan_info(adf_net_handle_t hdl, struct sk_buff *skb,
                         adf_net_vlanhdr_t *vlan)
{
     return A_STATUS_OK;
}

a_uint8_t
__adf_nbuf_get_tid(struct sk_buff *skb)
{
    return ADF_NBUF_TX_EXT_TID_INVALID;
}

void
__adf_nbuf_dmamap_set_cb(__adf_os_dma_map_t dmap, void *cb, void *arg)
{
    return;
}

a_uint32_t
__adf_nbuf_get_frag_size(__adf_nbuf_t nbuf, a_uint32_t cur_frag)
{
    struct skb_shared_info  *sh = skb_shinfo(nbuf);
    const skb_frag_t *frag = sh->frags + cur_frag;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
    return frag->size;
#else    
    return skb_frag_size(frag);
#endif
}

a_status_t
__adf_nbuf_frag_map(
    adf_os_device_t osdev, adf_nbuf_t nbuf, int offset, adf_os_dma_dir_t dir, int cur_frag)
{
    u_int32_t paddr_lo, frag_len;

/* tempory hack for simulation */
#ifdef A_SIMOS_DEVHOST
    NBUF_MAPPED_PADDR_LO(nbuf) = paddr_lo = (u_int32_t) nbuf->data;
    return A_STATUS_OK;
#else
    struct skb_shared_info *sh = skb_shinfo(nbuf);
    const skb_frag_t *frag = sh->frags + cur_frag;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,2,0)
    frag_len = frag->size;
    NBUF_EXTRA_FRAG_PADDR_LO(nbuf, 0) = paddr_lo =
         dma_map_page(osdev->dev,  frag->page,
                         frag->page_offset + offset, frag_len, dir);
#else
    frag_len = skb_frag_size(frag);

    NBUF_EXTRA_FRAG_PADDR_LO(nbuf, 0) = paddr_lo =
	     skb_frag_dma_map(osdev->dev, frag, offset, frag_len, dir);
#endif
    return dma_mapping_error(osdev->dev, paddr_lo) ?
        A_STATUS_FAILED : A_STATUS_OK;
#endif
}

EXPORT_SYMBOL(__adf_nbuf_alloc);
EXPORT_SYMBOL(__adf_nbuf_free);
EXPORT_SYMBOL(__adf_nbuf_ref);
EXPORT_SYMBOL(__adf_nbuf_shared);
EXPORT_SYMBOL(__adf_nbuf_frag_info);
EXPORT_SYMBOL(__adf_nbuf_dmamap_create);
EXPORT_SYMBOL(__adf_nbuf_dmamap_destroy);
EXPORT_SYMBOL(__adf_nbuf_map);
EXPORT_SYMBOL(__adf_nbuf_unmap);
EXPORT_SYMBOL(__adf_nbuf_map_nbytes);
EXPORT_SYMBOL(__adf_nbuf_unmap_nbytes);
EXPORT_SYMBOL(__adf_nbuf_map_single);
EXPORT_SYMBOL(__adf_nbuf_map_nbytes_single);
EXPORT_SYMBOL(__adf_nbuf_unmap_single);
EXPORT_SYMBOL(__adf_nbuf_unmap_nbytes_single);
EXPORT_SYMBOL(__adf_nbuf_dmamap_info);
EXPORT_SYMBOL(__adf_nbuf_set_rx_cksum);
EXPORT_SYMBOL(__adf_nbuf_get_tx_cksum);
EXPORT_SYMBOL(__adf_nbuf_get_vlan_info);
EXPORT_SYMBOL(__adf_nbuf_get_tid);
EXPORT_SYMBOL(__adf_nbuf_dmamap_set_cb);
EXPORT_SYMBOL(__adf_nbuf_get_frag_size);
EXPORT_SYMBOL(__adf_nbuf_frag_map);
