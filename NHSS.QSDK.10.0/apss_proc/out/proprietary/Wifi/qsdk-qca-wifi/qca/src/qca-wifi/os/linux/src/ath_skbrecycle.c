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

#if ATH_RXBUF_RECYCLE
#include <osdep.h>
#include <wbuf.h>
#include "osif_private.h"
#include "if_athvar.h"

/*
 * Queue the sk_buff to the recycle list before being indicated to the kernel stack.
 * The sk_buff is a cloned one of the original skb, so that the dataref (ref count 
 * of the data region) of the rx buffer is increased, therefore the data region of
 * the rx buffer will not be released by the kernel stack. When we need a new sk_buff
 * for DMA receiving, just find the one in the recycle list with dataref = 1, as in the 
 * function ath_rxbuf_recycle()
 */

static void
ath_rxbuf_collect(void *devhandle, void *buf)
{
    struct ath_softc *sc;    
    osdev_t osdev; 
    struct ath_freebuf_bin *bufbin;
    ath_freebufhead *buf_head, *bufbin_head;

    wbuf_t skb = (wbuf_t)buf;
    sc = (struct ath_softc *)devhandle;
    osdev = sc->sc_osdev;
    buf_head = &(osdev->bufhead);
    bufbin_head = &(osdev->bufhead_empty);

    if (skb->truesize >= (sc->sc_rxbufsize + sizeof(struct sk_buff))) {
        spin_lock(&(osdev->buflist_lock));
        if (!TAILQ_EMPTY(bufbin_head)) {
            bufbin = TAILQ_FIRST(bufbin_head);                
            TAILQ_REMOVE(bufbin_head, bufbin, buf_list);
        } else {
            bufbin = OS_MALLOC(osdev, sizeof(struct ath_freebuf_bin), GFP_ATOMIC);
        }

        if (likely(bufbin)) {
            /*
             * a fake clone here: just atomically increase the dataref of the skb_shared_info
             * to pretend that this skb is cloned, so that the pair kfree_skbmem and alloc_skb/skb_clone 
             * is inbalanced, and the data will be hold from being released.
             */
            bufbin->skb = qdf_nbuf_clone(skb);
            if (bufbin->skb) {
                bufbin->head = skb->head;
                bufbin->end = skb->end;
                bufbin->lifetime = 0;
                TAILQ_INSERT_TAIL(buf_head, bufbin, buf_list);
            } else {
                OS_FREE(bufbin);
            }
        }
        spin_unlock(&(osdev->buflist_lock));
    }
}

/*
 * Re-use the sk_buffs in the recycle list with dataref = 1, which implies that
 * we are the last one to hold this data region. Re-initialize the sk_buff header
 * and data pointers then we are ready to use the recycled rx buffer.
 */
static void * ath_rxbuf_recycle(void *devhandle)
{
    struct ath_softc *sc;    
    wbuf_t nwbuf;
    osdev_t osdev; 
    struct ath_freebuf_bin *bufbin, *tbufbin, *tbufbin1;
    ath_freebufhead buf_timeout;
    struct skb_shared_info *shinfo;
#if !LIMIT_RXBUF_LEN_4K
    int align;
    unsigned long offset = 0;
#endif
    
    sc = (struct ath_softc *)devhandle;
    osdev = sc->sc_osdev;
    bufbin = NULL; 
    TAILQ_INIT(&buf_timeout);
    spin_lock(&(osdev->buflist_lock));
    if (!TAILQ_EMPTY(&(osdev->bufhead))) {
        tbufbin = TAILQ_FIRST(&(osdev->bufhead));
        if (likely(tbufbin->skb)) {
            TAILQ_FOREACH_SAFE(tbufbin, &(osdev->bufhead), buf_list, tbufbin1) {
#ifdef CONFIG_WLAN_4K_SKB_OPT
		shinfo = skb_shinfo(tbufbin->skb);
#else
                shinfo = (struct skb_shared_info *)(tbufbin->end);    
#endif
                /*
                 * If the first item in the recycle list is with dataref = 1, 
                 * then simply remove it from the list and re-initialize it.
                 * If the first item is with dataref > 1, which means it is
                 * not ready for re-use, search the list to find the first availabl
                 * one from the top of the list, meanwhile check the lifetime
                 * of all the items in the list ahead of the first available 
                 * one, and move them to the end of the list if lifetime exceeds
                 * the threshold. In this way the blocking items are moved to 
                 * the end. Tune up the max lifetime by command iwpriv ath0 rxbuf_lifetime XX
                 */
                if (atomic_read(&(shinfo->dataref)) == 1) {
                    bufbin = tbufbin;
                    break;
                } else {
                    tbufbin->lifetime ++;    
                    if (tbufbin->lifetime > osdev->rxbuf_lifetime) {
                        TAILQ_REMOVE(&(osdev->bufhead), tbufbin, buf_list);
                        TAILQ_INSERT_TAIL(&buf_timeout, tbufbin, buf_list);
                    }
                }
            } 
        }
    }
    if (bufbin) {
        /*
         * remove the sk_buff to be re-used from the recycle list
         */    
        TAILQ_REMOVE(&(osdev->bufhead), bufbin, buf_list);
        spin_unlock(&(osdev->buflist_lock));
        nwbuf = bufbin->skb;
        /*
         * Re-initialize the sk_buff
         */
        /*
         * release the ref cont to the routing table in the kernel
         * which was hold in skb_clone when the sk_buff was put into
         * the list
         */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,31)
        dst_release(nwbuf->dst);
#else
        skb_dst_drop(nwbuf);
#endif
#ifdef CONFIG_NETFILTER
#if defined(CONFIG_NF_CONNTRACK) || defined(CONFIG_NF_CONNTRACK_MODULE)
    	nf_conntrack_put(nwbuf->nfct);
	    nf_conntrack_put_reasm(nwbuf->nfct_reasm);
#endif
#ifdef CONFIG_BRIDGE_NETFILTER
    	nf_bridge_put(nwbuf->nf_bridge);
#endif
#endif
#ifdef CONFIG_NET_SCHED
	    nwbuf->tc_index = 0;
#ifdef CONFIG_NET_CLS_ACT
    	nwbuf->tc_verd = 0;
#endif
#endif
        
    	memset(nwbuf, 0, offsetof(struct sk_buff, truesize));
	    nwbuf->truesize = bufbin->end - bufbin->head + sizeof(struct sk_buff);
    	atomic_set(&nwbuf->users, 1);
#if !LIMIT_RXBUF_LEN_4K
        align = sc->sc_cachelsz;
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,15)
        if(align){
            offset = ((unsigned long) (bufbin->head) + NET_SKB_PAD) % align;
            if(offset) {
                offset = align - offset + NET_SKB_PAD;
            }
        }
#else
        if(align){
            offset = ((unsigned long) (bufbin->head) + 16) % align;
            if(offset) {
                offset = align - offset + 16;
            }
        }
#endif
#endif
        nwbuf->head = bufbin->head;
#if LIMIT_RXBUF_LEN_4K
    	nwbuf->data = bufbin->head;
#else
    	nwbuf->data = bufbin->head + offset;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
    	skb_reset_tail_pointer(nwbuf);        
#else
	    nwbuf->tail = nwbuf->data;
#endif
    	nwbuf->end  = bufbin->end;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
#ifdef NET_SKBUFF_DATA_USES_OFFSET
    	nwbuf->mac_header = ~0U;
#endif
#endif
        /*
         * Since the data buffer is reused instead of newly allocated, no need to 
         * call bus_map_single again; Simply get the physical address for DMA receiving,
         * and this saves the CPU cycles from redundant dma_cache_inv calls
         */
        /*
         * reset the shared info for this new skb, as a linear one, unshared, uncloned
         */

#ifndef CONFIG_WLAN_4K_SKB_OPT
        shinfo = (struct skb_shared_info *)(bufbin->end);    
#endif
        OS_MEMZERO(shinfo, sizeof(struct skb_shared_info));
    	atomic_set(&(shinfo->dataref), 1);
        /*
         * Queue the empty bufbin back to a specific bufbin pool
         */
        bufbin->head = NULL;
        bufbin->end = NULL;
        bufbin->skb = NULL;
        bufbin->lifetime = 0;        
        TAILQ_INSERT_HEAD(&(osdev->bufhead_empty), bufbin, buf_list);
        
        /*
         * re-set the dev to the sk_buff
         */
        nwbuf->dev = osdev->netdev;
        /*
         * setup rx context in nbuf
         */
        N_CONTEXT_SET(nwbuf, &((struct ieee80211_cb *)(nwbuf->cb))[1]);
    } else {
        /*
         * If no available sk_buff in the list, alloc it from kernel stack.
         * The recycle list is empty at the beginning when the wlan device is up,
         * and there is no pre-allocation of the rx buffers. So the rx buffers
         * have to be allocated in the normal way in the beginning, and 
         * the recycle list is sef-growing.
         */    
        spin_unlock(&(osdev->buflist_lock));
        nwbuf = ath_rxbuf_alloc(sc, sc->sc_rxbufsize);
    }
    /*
     * Put the blocking buffers to the discarded list
     */
    if (!TAILQ_EMPTY(&buf_timeout)) {
        TAILQ_CONCAT(&(osdev->bufhead_discard), &buf_timeout, buf_list);
    }
    return (void *)nwbuf;
}

/*
 * Initiate the data structures related to rxbuf recycleing
 */
void ath_rxbuf_recycle_init(osdev_t osdev)
{
    TAILQ_INIT(&(osdev->bufhead));
    TAILQ_INIT(&(osdev->bufhead_empty));
    TAILQ_INIT(&(osdev->bufhead_discard));
    osdev->rxbuf_lifetime = MAX_RXBUF_RECYCLE_LIFETIME;
    osdev->rbr_ops.osdev_wbuf_collect = ath_rxbuf_collect;
    osdev->rbr_ops.osdev_wbuf_recycle = ath_rxbuf_recycle;
    spin_lock_init(&(osdev->buflist_lock));
}

/*
 * Release the data structures related to rxbuf recycleing
 */
void ath_rxbuf_recycle_destroy(osdev_t osdev)
{
    struct ath_freebuf_bin *bufbin, *tbufbin;
        
    spin_lock(&(osdev->buflist_lock));
    TAILQ_FOREACH_SAFE(bufbin, &(osdev->bufhead), buf_list, tbufbin) {
        if (bufbin->skb) {
            qdf_nbuf_free(bufbin->skb);
        }
        TAILQ_REMOVE(&(osdev->bufhead), bufbin, buf_list);
        OS_FREE(bufbin);
    }
    TAILQ_FOREACH_SAFE(bufbin, &(osdev->bufhead_discard), buf_list, tbufbin) {
        if (bufbin->skb) {
            qdf_nbuf_free(bufbin->skb);
        }
        TAILQ_REMOVE(&(osdev->bufhead_discard), bufbin, buf_list);
        OS_FREE(bufbin);
    }
    TAILQ_FOREACH_SAFE(bufbin, &(osdev->bufhead_empty), buf_list, tbufbin) {
        TAILQ_REMOVE(&(osdev->bufhead_empty), bufbin, buf_list);
        OS_FREE(bufbin);
    }
    osdev->rbr_ops.osdev_wbuf_collect = NULL;
    osdev->rbr_ops.osdev_wbuf_recycle = NULL;
    spin_unlock(&(osdev->buflist_lock));
    spin_lock_destroy(&(osdev->buflist_lock));
}

#endif /* ATH_RXBUF_RECYCLE */

