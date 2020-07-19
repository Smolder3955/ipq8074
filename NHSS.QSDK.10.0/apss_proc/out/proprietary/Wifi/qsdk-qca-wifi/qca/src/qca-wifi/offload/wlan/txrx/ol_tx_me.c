/*
 * Copyright (c) 2014-2015, 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * 2014-2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <qdf_nbuf.h>         /* qdf_nbuf_t, etc. */


#include "ol_if_athvar.h"
#include <htt_types.h>        /* htc_endpoint */
#include <cdp_txrx_cmn_struct.h>      /* ol_txrx_vdev_handle */
#include <ol_htt_tx_api.h>    /* htt_tx_desc_tid */
#include <ol_txrx_ctrl_api.h> /* ol_txrx_sync */

#include <ol_txrx_internal.h> /* TXRX_ASSERT1 */
#include <ol_txrx_types.h>    /* pdev stats */
#include <ol_tx_desc.h>       /* ol_tx_desc */
#include <ol_tx_send.h>       /* ol_tx_send */
#include <ol_txrx_api.h>

#if QCA_PARTNER_DIRECTLINK_TX
extern void ol_tx_me_convert_ucast_partner(ol_txrx_vdev_handle vdev, qdf_nbuf_t wbuf,
                u_int8_t newmac[][6], uint8_t new_mac_cnt);
extern int CE_is_directlink(struct CE_handle* ce_tx_hdl);
#endif /* QCA_PARTNER_DIRECTLINK_TX */

#if ATH_SUPPORT_IQUE

/*
 * Function to allocate descriptors based on vap count
 * @pdev - pointer to txrx_pdev structure*/
void
ol_tx_me_alloc_descriptor(void *pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    if(atomic_read(&pdev->mc_num_vap_attached) == 0){
        ol_tx_me_init((ol_txrx_pdev_handle) pdev);
        qdf_print("Enable MCAST_TO_UCAST");
    }
    atomic_inc(&pdev->mc_num_vap_attached);
}

/* Function to free descriptor based on vap attached
 * @pdev - pointer to txrx_pdev structure*/
void
ol_tx_me_free_descriptor(void *pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    atomic_dec(&pdev->mc_num_vap_attached);
    if(atomic_read(&pdev->mc_num_vap_attached) == 0) {
        /* Set flag to disable so that there is no further mcast_to_ucast conversion */
        ol_tx_me_exit((ol_txrx_pdev_handle) pdev);
        qdf_print("Disable MCAST_TO_UCAST");
    }
}

/* Function to initialize memory required for multicast unicast conversion
 * @pdev - pointer to txrx_pdev structure. */
uint16_t
ol_tx_me_init(ol_txrx_pdev_handle pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    uint16_t i,dlen, num_pool_elems;
    uint32_t pool_size;

    struct ol_tx_me_buf_t *p;

    dlen = htt_pkt_dl_len_get(pdev->htt_pdev);
    /* Align dlen to word boundary */
    dlen = (dlen + 3) & (~0x3);
    num_pool_elems = MAX_ME_BUF_CHUNK;
    /* Add flow control buffer count */
    pool_size = (dlen + sizeof(struct ol_tx_me_buf_t)) * num_pool_elems;
    pdev->me_buf.size = dlen;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (!pdev->nss_wifiol_ctx && pdev->me_buf.vaddr == NULL) {
#else
    if (pdev->me_buf.vaddr == NULL) {
#endif
        qdf_spin_lock_bh(&pdev->tx_mutex);
        pdev->me_buf.vaddr = qdf_mem_malloc(pool_size );
        if (pdev->me_buf.vaddr == NULL) {
            qdf_spin_unlock_bh(&pdev->tx_mutex);
            printk("Error allocating memory pool\n");
            return 1;
        }
        pdev->me_buf.buf_in_use = 0;
        pdev->me_buf.freelist = (struct ol_tx_me_buf_t *) pdev->me_buf.vaddr;
        /*
         * me_buf looks like this
         * |=======+==========================|
         * | ptr   |    data (download len)   |
         * |=======+==========================|
         */
        p = pdev->me_buf.freelist;
        for (i = 0; i < num_pool_elems-1; i++) {
            p->nonpool_based_alloc = 0;
            p->next = (struct ol_tx_me_buf_t*)
                ((char*)p + pdev->me_buf.size + sizeof(struct ol_tx_me_buf_t));
            p = p->next;
        }
        p->next = NULL;
        qdf_spin_unlock_bh(&pdev->tx_mutex);
        printk("ME Pool succesfully initialized vaddr - %pK paddr - %x\n"
                "num_elems = %d buf_size - %d pool_size = %d\n",
                pdev->me_buf.vaddr,
                (unsigned int)pdev->me_buf.paddr,
                (unsigned int)num_pool_elems,
                (unsigned int)pdev->me_buf.size,
                (unsigned int)pool_size);
    } else {
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if(!pdev->nss_wifiol_ctx)
#endif
            printk("%s:Already Enabled!!\n", __func__);
    }
    return 0;
}

#define ME_US_TO_SEC(_x) ((_x)/(1000 * 1000))
#define ME_CLEAN_WAIT_TIMEOUT (200000) /*200ms*/
#define ME_CLEAN_WAIT_COUNT 400
/* Function to Free memory and other cleanup required for multicast unicast conversion
 * @pdev - pointer to txrx_pdev structure. */
void
ol_tx_me_exit(ol_txrx_pdev_handle pdev_handle)
{
    struct ol_txrx_pdev_t *pdev = (struct ol_txrx_pdev_t *)pdev_handle;
    uint16_t num_pool_elems;
    uint32_t pool_size;
    uint16_t dlen;
    struct ol_txrx_psoc_t *dp_soc = (struct ol_txrx_psoc_t *) pdev->soc;

    dlen = pdev->me_buf.size;
    num_pool_elems = ol_cfg_target_tx_credit(dp_soc->psoc_obj);
    /* Add flow control buffer count */
    num_pool_elems += OL_TX_FLOW_CTRL_QUEUE_LEN;
    pool_size = ((pdev->me_buf.size + sizeof(struct ol_tx_me_buf_t *)) * num_pool_elems);

    if (pdev->me_buf.vaddr) {
        uint16_t wait_cnt = 0;
        printk("Disabling Mcastenhance. This may take some time...\n");
        qdf_spin_lock_bh(&pdev->tx_mutex);
        while ((pdev->me_buf.buf_in_use > 0) &&
                (wait_cnt < ME_CLEAN_WAIT_COUNT) ) {
            qdf_spin_unlock_bh(&pdev->tx_mutex);
            OS_SLEEP(ME_CLEAN_WAIT_TIMEOUT);
            wait_cnt++;
            qdf_spin_lock_bh(&pdev->tx_mutex);
        }
        if (pdev->me_buf.buf_in_use > 0) {
            printk("Tx-comp pending for %d ME frames after waiting %ds!!\n",
                    pdev->me_buf.buf_in_use,
                    ME_US_TO_SEC(ME_CLEAN_WAIT_TIMEOUT * ME_CLEAN_WAIT_COUNT));
            qdf_assert_always(0);
        }

        qdf_mem_free( pdev->me_buf.vaddr);
        pdev->me_buf.vaddr = NULL;
        pdev->me_buf.freelist = NULL;
        qdf_spin_unlock_bh(&pdev->tx_mutex);
    } else {
        printk(KERN_INFO"%s: Already Disabled !!!\n", __func__);
    }
}

/* Function to convert mcast to ucast.
 * @vap - pounter to vap structure
 * @wbuf - pointer to multicast frame (skb)
 * @newmac - Array if unicast mac address interseted in this mcast group
 * @new_mac_cnt - Number of entries in newmac array. */
extern uint16_t
ol_tx_me_convert_ucast(void *vdev_handle, qdf_nbuf_t wbuf,
                             u_int8_t newmac[][6], uint8_t new_mac_cnt)
{
    ol_txrx_vdev_handle vdev = (ol_txrx_vdev_handle)vdev_handle;
    struct ol_txrx_vdev_t *dp_vdev = (struct ol_txrx_vdev_t *)vdev;
    struct ol_txrx_pdev_t *pdev = ((struct ol_txrx_vdev_t *)vdev)->pdev;
    struct ether_header *eh;
    u_int8_t *data;
    u_int16_t len, dwnld_len;
    u_int8_t *dstmac;                           /* reference to frame dst addr */
    u_int8_t srcmac[IEEE80211_ADDR_LEN];    /* copy of original frame src addr */
    u_int8_t empty_entry_mac[IEEE80211_ADDR_LEN];
    uint8_t new_mac_idx = 0;                        /* local index into newmac */
    struct ether_header *eh2;                /* eth hdr of tunnelled frame */
    struct ol_tx_me_buf_t *mc_uc_buf;
    qdf_nbuf_t  wbuf_cpy;

#if QCA_PARTNER_DIRECTLINK_TX
    if ((CE_is_directlink(pdev->ce_tx_hdl))) {
        ol_tx_me_convert_ucast_partner( vdev, wbuf, newmac, new_mac_cnt);
        return 1;
    }
#endif
    TXRX_VDEV_STATS_ADD(dp_vdev, tx_i.mcast_en.mcast_pkt.num, 1);

    /* TODO: Do we need this Empty entry mac */
    OS_MEMSET(empty_entry_mac, 0, IEEE80211_ADDR_LEN);

    eh = (struct ether_header *)wbuf_header(wbuf);
    dstmac = eh->ether_dhost;
    IEEE80211_ADDR_COPY(srcmac, eh->ether_shost);

    len = qdf_nbuf_len( wbuf );
    dwnld_len = pdev->me_buf.size;
    len = len < dwnld_len ? len : dwnld_len;

    data = qdf_nbuf_data( wbuf );

    if(QDF_STATUS_E_FAILURE == qdf_nbuf_map_single( pdev->osdev, wbuf, QDF_DMA_TO_DEVICE)) {
        qdf_nbuf_free(wbuf);
        TXRX_VDEV_STATS_ADD(dp_vdev, tx_i.mcast_en.dropped_map_error, 1);
        return 1;
    }
    /* Set Null to begin with */
    qdf_nbuf_set_tx_fctx_type( (qdf_nbuf_t) wbuf, (void *) 0, CB_FTYPE_MCAST2UCAST);

    for( new_mac_idx = 0; new_mac_idx < new_mac_cnt; new_mac_idx++ ) {

        dstmac = newmac[new_mac_idx];

        /* In case of loop */
        if(IEEE80211_ADDR_EQ(dstmac, srcmac) ||
                (IEEE80211_ADDR_EQ(dstmac, empty_entry_mac))) {
            TXRX_VDEV_STATS_ADD(dp_vdev, tx_i.mcast_en.dropped_self_mac, 1);
            /* frame to self mac. skip */
            continue;
        }


        mc_uc_buf = ol_tx_me_alloc_buf(pdev);
        if( mc_uc_buf == NULL ) {
            TXRX_VDEV_STATS_ADD(dp_vdev, tx_i.mcast_en.fail_seg_alloc, 1);
            break;
        }

        if( new_mac_idx < (new_mac_cnt - 1) ) {
            wbuf_cpy = qdf_nbuf_clone((qdf_nbuf_t)wbuf);
            if( wbuf_cpy == NULL ) {
                TXRX_VDEV_STATS_ADD(dp_vdev, tx_i.mcast_en.fail_seg_alloc, 1);
                ol_tx_me_free_buf(pdev, mc_uc_buf);
                /* No buffers to send data. break */
                break;
            }
        } else {
            /* Update the ref, to account for frame sent without cloning */
            qdf_nbuf_ref(wbuf);
            wbuf_cpy = wbuf;
        }

        qdf_mem_copy( &mc_uc_buf->buf[0], data, len );
        eh2 = (struct ether_header *)&mc_uc_buf->buf[0];

        /* copy new dest addr */
        IEEE80211_ADDR_COPY(&eh2->ether_dhost[0], dstmac);

        qdf_nbuf_set_tx_fctx_type( (qdf_nbuf_t) wbuf_cpy, (void *) mc_uc_buf,
                CB_FTYPE_MCAST2UCAST);

        if ((pdev->acqcnt < (pdev->acqcnt_len - (2 * pdev->vdev_count)))
#ifdef WLAN_FEATURE_FASTPATH
#if PEER_FLOW_CONTROL
        && (ol_tx_ll_pflow_ctrl(vdev, wbuf_cpy) == 0)
#else
        && (ol_tx_ll_fast(vdev, &wbuf_cpy, 1) == 0)
#endif
#endif
        ) {
            TXRX_VDEV_STATS_ADD(dp_vdev, tx_i.mcast_en.ucast, 1);
        } else {
            TXRX_VDEV_STATS_ADD(dp_vdev, tx_i.mcast_en.dropped_send_fail, 1);
#if !PEER_FLOW_CONTROL
            if(mc_uc_buf) {
                ol_tx_me_free_buf(pdev, mc_uc_buf);
            }
            qdf_nbuf_free(wbuf_cpy);
#endif
        }
    }

    /* Free one ref in successful translation case.
     * And free original SKB in failure case */
    qdf_nbuf_free(wbuf);
    return 1;
}

/*
 * Process multicast packets here.
 * If multicast ehancement is required call appropriate functions
 * to perform the mcast to ucast conversion.
 */

#define IS_MCAST_ENHANCE_REQUIRED(vap, eh) \
    (((vap)->iv_me->mc_mcast_enable) || ((vap)->iv_me->me_hifi_enable)) && \
    ((vap)->iv_sta_assoc > 0 )      && \
    (!IEEE80211_IS_BROADCAST((eh)->ether_dhost))

extern int
ol_tx_mcast_enhance_process(struct ieee80211vap *vap, qdf_nbuf_t skb)
{
    struct ether_header *eh = (struct ether_header *)skb->data;
    if (IS_MCAST_ENHANCE_REQUIRED(vap, eh)) {
        /*TODO: HYFI support needs to be verified.*/
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        if (vap->iv_me->me_hifi_enable && vap->iv_ique_ops.me_hifi_convert) {
            return vap->iv_ique_ops.me_hifi_convert(vap, skb);
        }
#endif
#if ATH_SUPPORT_ME_SNOOP_TABLE
        if (vap->iv_ique_ops.me_convert) {
            return vap->iv_ique_ops.me_convert(vap, skb);
        }
#endif
    }
    return 0;
}

#endif /*ATH_SUPPORT_IQUE*/
