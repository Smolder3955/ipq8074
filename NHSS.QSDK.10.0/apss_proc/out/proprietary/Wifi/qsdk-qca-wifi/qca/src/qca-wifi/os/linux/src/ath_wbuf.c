/*
 * Copyright (c) 2016-2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
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

#include "ath_internal.h"
#include <osdep.h>
#include <wbuf.h>


#define MIN_HEAD_ROOM  64
#define NBUF_ALLOC_FAIL_LIMIT 100


qdf_nbuf_t
ath_rxbuf_alloc(struct ath_softc *sc, u_int32_t len)
{
    qdf_nbuf_t nbf;
    struct net_device *dev = sc->sc_osdev->netdev;
    static u_int32_t nbuf_fail_limit_print = 0;

    if (sc->sc_opmode == HAL_M_MONITOR) {
        /*
         * Allocate buffer for monitor mode with space for the
         * wlan-ng style physical layer header at the start.
         */
        nbf = wbuf_alloc(NULL, WBUF_RX_INTERNAL, len +
                            sizeof(wlan_ng_prism2_header) +
                            sc->sc_cachelsz - 1);
        if (nbf == NULL)
        {
            if(!(nbuf_fail_limit_print % NBUF_ALLOC_FAIL_LIMIT))
            {
                qdf_print(
                        "%s: nbuf alloc of size %zu failed",
                        __func__,
                        len
                        + sizeof(wlan_ng_prism2_header)
                        + sc->sc_cachelsz -1);
            }
            nbuf_fail_limit_print++;
            return NULL;
        }
#if LIMIT_RXBUF_LEN_4K
        if (unlikely(qdf_nbuf_headroom(nbf) < NET_SKB_PAD)) {
            dump_stack();
            panic("%s: nbf (nbf->data=%pK) doesn't have required headroom, "
                    "has %d bytes (len=%d+%zd)\n", __func__, nbf->data,
                    qdf_nbuf_headroom(nbf), len, sizeof(wlan_ng_prism2_header));
        } else {
            nbf->data -= NET_SKB_PAD;
            skb_reset_tail_pointer(nbf); /*nbf->tail = nbf->data;*/
        }
#endif
    } else {
        /*
         * Cache-line-align.  This is important (for the
         * 5210 at least) as not doing so causes bogus data
         * in rx'd frames.
         */
#if LIMIT_RXBUF_LEN_4K
        nbf = wbuf_alloc(NULL, WBUF_RX_INTERNAL, len - NET_SKB_PAD);
#else
        nbf = qdf_nbuf_alloc(NULL, len, 0, 1, FALSE);
#endif
        if (nbf == NULL)
        {
            if(!(nbuf_fail_limit_print % NBUF_ALLOC_FAIL_LIMIT))
            {
                qdf_print(
                        "%s: nbuf alloc of size %u failed",
                        __func__, len);
            }
            nbuf_fail_limit_print++;
            return NULL;
        }
#if LIMIT_RXBUF_LEN_4K
        if (unlikely((uintptr_t)(nbf->data) & (sc->sc_cachelsz - 1))) {
            dump_stack();
            panic("%s: nbf->data=%pK (nbf->head=%pK) is not aligned "
                    "to %d bytes (len=%d)\n", __func__, nbf->data, nbf->head,
                    sc->sc_cachelsz, len);
        }
        if (unlikely(qdf_nbuf_headroom(nbf) < NET_SKB_PAD)) {
            dump_stack();
            panic("%s: nbf (nbf->data=%pK) doesn't have required headroom, "
                    "has %d bytes (len=%d)\n", __func__, nbf->data,
                    qdf_nbuf_headroom(nbf), len);
        } else {
            nbf->data -= NET_SKB_PAD;
            skb_reset_tail_pointer(nbf);
        }
#endif
    }

    nbf->dev = dev;

    /*
     * setup rx context in nbuf
     * XXX: this violates the rule of ath_dev,
     * which is supposed to be protocol independent.
     */
    N_CONTEXT_SET(nbf, &((struct ieee80211_cb *)(qdf_nbuf_get_ext_cb(nbf)))[1]);

    return nbf;
}
EXPORT_SYMBOL(ath_rxbuf_alloc);

#ifdef MEMORY_DEBUG
qdf_nbuf_t wbuf_alloc_debug(osdev_t os_handle, enum wbuf_type type,
			    u_int32_t len, uint8_t *file, uint32_t line)
#else
qdf_nbuf_t wbuf_alloc(osdev_t os_handle, enum wbuf_type type, u_int32_t len)
#endif
{
    const u_int align = sizeof(u_int32_t);
    qdf_nbuf_t  nbf;
    u_int buflen, reserve;
#if defined(QCA_WIFI_QCA8074) && defined (BUILD_X86)
    uint32_t lowmem_alloc_tries = 0;
#endif

    if ((type == WBUF_TX_DATA) || (type == WBUF_TX_MGMT) ||
            (type == WBUF_TX_BEACON) || (type == WBUF_TX_INTERNAL)
            || (type == WBUF_TX_CTL)) {
        reserve = MIN_HEAD_ROOM;
        buflen = roundup(len+MIN_HEAD_ROOM, 4);
    } else {
        reserve = 0;
        buflen = roundup(len, 4);
    }
#if defined(QCA_WIFI_QCA8074) && defined (BUILD_X86)
realloc:
#endif

#ifdef MEMORY_DEBUG
    nbf = qdf_nbuf_alloc_debug(NULL, buflen, 0, align, FALSE, file, line);
#else
    nbf = qdf_nbuf_alloc(NULL, buflen, 0, align, FALSE);
#endif

#if defined(QCA_WIFI_QCA8074) && defined (BUILD_X86)
    /* Hawkeye M2M emulation cannot handle memory addresses below 0x50000000
     * Though we are trying to reserve low memory upfront to prevent this,
     * we sometimes see SKBs allocated from low memory.
     */
    if (nbf != NULL) {
        if (virt_to_phys(qdf_nbuf_data(nbf)) < 0x50000040) {
            lowmem_alloc_tries++;
            if (lowmem_alloc_tries > 100) {
                return NULL;
            } else {
               /* Not freeing to make sure it
                * will not get allocated again
                */
                goto realloc;
            }
        }
    }
#endif
    if (nbf != NULL)
    {
        if (wbuf_alloc_mgmt_ctrl_block(nbf) == NULL) {
#ifdef MEMORY_DEBUG
            qdf_nbuf_free_debug(nbf, file, line);
#else
            qdf_nbuf_free(nbf);
#endif
            return NULL;
        }
        N_NODE_SET(nbf, NULL);
        N_FLAG_KEEP_ONLY(nbf, 0);
        N_TYPE_SET(nbf, type);
        N_COMPLETE_HANDLER_SET(nbf, NULL);
        N_COMPLETE_HANDLER_ARG_SET(nbf, NULL);
#if defined(ATH_SUPPORT_P2P)
        N_COMPLETE_HANDLER_SET(nbf, NULL);
        N_COMPLETE_HANDLER_ARG_SET(nbf, NULL);
#endif  /* ATH_SUPPORT_P2P */
        if (reserve)
            qdf_nbuf_reserve(nbf, reserve);
    }
    return nbf;
}
#ifdef MEMORY_DEBUG
EXPORT_SYMBOL(wbuf_alloc_debug);
#else
EXPORT_SYMBOL(wbuf_alloc);
#endif

void
wbuf_dealloc_mgmt_ctrl_block(__wbuf_t wbuf)
{
    struct ieee80211_cb *mgmt_cb_ptr = qdf_nbuf_get_ext_cb(wbuf);

    if(mgmt_cb_ptr) {

        /*
         * Call the destructor in saved in ext_cb at
         * alloc time
         */
        if (mgmt_cb_ptr->destructor)
            mgmt_cb_ptr->destructor(wbuf);

        qdf_nbuf_set_ext_cb(wbuf, NULL);
        qdf_mem_free(mgmt_cb_ptr);
    }
    return;
}
EXPORT_SYMBOL (wbuf_dealloc_mgmt_ctrl_block);

dma_addr_t
__wbuf_map_single_tx(osdev_t osdev, struct sk_buff *skb, int direction, dma_addr_t *pa)
{
    /*
     * NB: do NOT use skb->len, which is 0 on initialization.
     * Use skb's entire data area instead.
     */
    *pa = bus_map_single(osdev, skb->data, UNI_SKB_END_POINTER(skb) - skb->data, direction);

    return *pa;
}

void
__wbuf_uapsd_update(qdf_nbuf_t nbf)
{
    /* DO NOTHING */
}

#if ATH_PERF_PWR_OFFLOAD
EXPORT_SYMBOL(__wbuf_map_single);
EXPORT_SYMBOL(__wbuf_map_single_tx);
EXPORT_SYMBOL(__wbuf_unmap_single);
#endif  /* ATH_PERF_PWR_OFFLOAD */

