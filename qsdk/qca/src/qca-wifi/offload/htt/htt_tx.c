/*
 * Copyright (c) 2011-2014,2017-2018 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 *
 * 2011-2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 * @file htt_tx.c
 * @brief Implement transmit aspects of HTT.
 * @details
 *  This file contains three categories of HTT tx code:
 *  1.  An abstraction of the tx descriptor, to hide the
 *      differences between the HL vs. LL tx descriptor.
 *  2.  Functions for allocating and freeing HTT tx descriptors.
 *  3.  The function that accepts a tx frame from txrx and sends the
 *      tx frame to HTC.
 */
#include <osdep.h>          /* u_int32_t, offsetof, etc. */
#include <qdf_types.h>   /* qdf_dma_addr_t */
#include <qdf_mem.h>     /* qdf_mem_malloc_consistent,free_consistent */
#include <qdf_nbuf.h>       /* qdf_nbuf_t, etc. */

#include <htt.h>            /* htt_tx_msdu_desc_t */
#include <htc.h>            /* HTC_HDR_LENGTH */
#include <ol_cfg.h>         /* ol_cfg_netbuf_frags_max, etc. */
#include <ol_htt_tx_api.h>

#include <htt_internal.h>
#include <ol_txrx_types.h>

#if WIFI_MEM_MANAGER_SUPPORT
#include "mem_manager.h"
#endif
#include <wlan_lmac_dispatcher.h>

/*--- setup / tear-down functions -------------------------------------------*/

#if PEER_FLOW_CONTROL
/* Queue map flush timer
 * Configurable
 */
enum qdf_hrtimer_restart_status htt_qmap_flush_hightimer_cb(qdf_hrtimer_data_t *arg)
{
    qdf_hrtimer_data_t *hrtimer_data = arg;
    struct pmap_ctxt *pmapctxt = (struct pmap_ctxt *)arg;
    struct htt_pdev_t *pdev    = (struct htt_pdev_t *)pmapctxt->httpdev;
    unsigned long modified_time_ns = 0;
    ktime_t modified_ktime;
    uint32_t paddr_lo = 0;
    qdf_device_t qdf_dev;

    if(pdev && pdev->txrx_pdev) {
        qdf_dev = wlan_psoc_get_qdf_dev((void *)pdev->ctrl_psoc);
        modified_time_ns = (((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->pmap_rotting_timer_interval * 1000000);
        modified_ktime = ktime_set(0, modified_time_ns);

        /* Cache flush only in Queuing mode */
        if(((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->pflow_ctl_global_queue_cnt) {
            /* If the trylock fails, it means that some other thread is
               already doing the flush. So, no need to flush again here.
               Besides, it reduces the overhead since we are in IRQ context */
            if (qdf_spin_trylock(&((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->peermap_lock)) {
                ((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->pmap->seq++;

                OS_SYNC_SINGLE((osdev_t)qdf_dev->drv, pdev->host_q_mem.pool_paddr,
                        pdev->host_q_mem.size, BUS_DMA_TODEVICE, (dma_context_t)&paddr_lo);
                qdf_spin_unlock((&((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->peermap_lock));
            }
        }

        qdf_hrtimer_forward(hrtimer_data, qdf_hrtimer_cb_get_time(hrtimer_data), modified_ktime);
    }

    return QDF_HRTIMER_RESTART;
}

void
htt_peer_map_timer_init(struct ol_txrx_pdev_t *pdev)
{
    unsigned long modified_time_ns = pdev->pmap_rotting_timer_interval * 1000000;
    ktime_t modified_ktime = ktime_set(0, modified_time_ns);

    qdf_hrtimer_init(&pdev->qmap_ctxt.htimer, htt_qmap_flush_hightimer_cb,
		    QDF_CLOCK_MONOTONIC, QDF_HRTIMER_MODE_REL, QDF_CONTEXT_HARDWARE);

    qdf_hrtimer_start(&pdev->qmap_ctxt.htimer, modified_ktime, QDF_HRTIMER_MODE_REL);

    return;
}

int
htt_alloc_peer_map_mem(struct htt_pdev_t *pdev, int desc_pool_elems)
{
    uint8_t *vaddr;
    uint32_t paddr_lo, pool_size;
    struct ol_txrx_pdev_t *txrx_pdev = (struct ol_txrx_pdev_t *)pdev->txrx_pdev;
    /**
     * Allocate the Shared memory for Peer Map view between Host - Target
     */
    qdf_mem_set((uint8_t *)&pdev->host_q_mem, sizeof(pdev->host_q_mem), 0);
    pdev->host_q_mem.peerid_elems = OL_TXRX_MAX_PEER_IDS;
    pdev->host_q_mem.tid_size     = OL_TX_PFLOW_CTRL_MAX_TIDS;

    pdev->host_q_mem.size = pool_size = OL_TX_QUEUE_MAP_RESERVE_PADDING + sizeof(struct peermap);

    pdev->host_q_mem.pool_vaddr = qdf_mem_malloc(pool_size);
    if (!pdev->host_q_mem.pool_vaddr) {
        qdf_print("\n Host Q Alloc Failed");
        return 1; /* failure */
    }

    vaddr = pdev->host_q_mem.pool_vaddr;

    /* Map and store paddr  */
    pdev->host_q_mem.pool_paddr = paddr_lo = (uint32_t )OS_DMA_MAP_SINGLE(pdev->osdev, vaddr,
                                                                   pdev->host_q_mem.size,QDF_DMA_TO_DEVICE);

    if(OS_DMA_MAP_ERROR(pdev->osdev, paddr_lo)) {
        qdf_print("\n %s : Host Q Memory Map Failed ",__FUNCTION__);
        return 1;
    }

    txrx_pdev->pmap = (struct peermap  *) pdev->host_q_mem.pool_vaddr;
    /* Initialize the Queue Map Flush timer */
    qdf_spinlock_create(&((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->peermap_lock);

    txrx_pdev->qmap_ctxt.httpdev = (struct htt_pdev_t *)pdev;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if (!pdev->nss_wifiol_ctx)
#endif
    {
        htt_peer_map_timer_init(txrx_pdev);
    }

    qdf_print("\n %s : Alloc Success : host q vaddr %pK paddr %x",
                               __FUNCTION__,pdev->host_q_mem.pool_vaddr, pdev->host_q_mem.pool_paddr);
    qdf_print("\n %s : Flush Interval Configured to %d pkts",__FUNCTION__, txrx_pdev->pmap_qdepth_flush_interval);

    return 0;
}
#endif

int
htt_tx_attach(struct htt_pdev_t *pdev, int desc_pool_elems)
{
    int i, pool_size;
    u_int32_t **p;
    qdf_dma_addr_t pool_paddr = 0;
    qdf_device_t qdf_dev = wlan_psoc_get_qdf_dev((void *)pdev->ctrl_psoc);
#if WIFI_MEM_MANAGER_SUPPORT
    uint32_t soc_idx = wlan_ucfg_get_config_param((void *)pdev->ctrl_psoc, SOC_ID);
    int intr_ctxt;
#endif
	if (pdev->cfg.is_high_latency) {
		pdev->tx_descs.size = sizeof(struct htt_host_tx_desc_t);
	} else {
		/*
		 * Start with the size of the base struct
		 * that actually gets downloaded.
		 */
		pdev->tx_descs.size =
			sizeof(struct htt_host_tx_desc_t);

		if (HTT_WIFI_IP_VERSION(pdev->wifi_ip_ver.major, 0x2)) {
			/*
			 * sizeof MSDU_EXT/Fragmentation descriptor.
			 */
			pdev->frag_descs.size = sizeof(struct msdu_ext_desc_t);
		} else {
			/*
			 * Add the fragmentation descriptor elements.
			 * Add the most that the OS may deliver, plus one more in
			 * case the txrx code adds a prefix fragment (for TSO or
			 * audio interworking SNAP header)
			 */
			pdev->frag_descs.size = (ol_cfg_netbuf_frags_max(pdev->ctrl_psoc)+1) * 8 + 4; /* u_int32_t fragmentation list terminator */
		}

	}
	if (pdev->tx_descs.size < sizeof(u_int32_t *)) {
		pdev->tx_descs.size = sizeof(u_int32_t *);
	}
	/*
	 * Make sure tx_descs.size is a multiple of 4-bytes.
	 * It should be, but round up just to be sure.
	 */
	pdev->tx_descs.size = (pdev->tx_descs.size + 3) & (~0x3);

	pdev->tx_descs.pool_elems = desc_pool_elems;
	pdev->tx_descs.alloc_cnt = 0;
	pdev->frag_descs.pool_elems = desc_pool_elems;

	pool_size = pdev->tx_descs.pool_elems * pdev->tx_descs.size;
#if WIFI_MEM_MANAGER_SUPPORT
    intr_ctxt = (in_interrupt() || irqs_disabled()) ? 1 : 0;
	/* passing 0 as pdev id since there will be 1 radio */
        pdev->tx_descs.pool_vaddr = (char*)wifi_cmem_allocation(soc_idx, 0,
                CM_TX_ATTACH_POOL_VADDR,pool_size,(void*) qdf_dev->drv_hdl, &pool_paddr,intr_ctxt);
 #else
	pdev->tx_descs.pool_vaddr = qdf_mem_alloc_consistent(
			pdev->osdev, &(((struct pci_dev *)(qdf_dev->drv_hdl))->dev), pool_size, &pool_paddr);
#endif
	pdev->tx_descs.pool_paddr = pool_paddr;

	if (!pdev->tx_descs.pool_vaddr) {
		return 1; /* failure */
	}

	/**
	 * Allocate the fragment descriptor this needs to contigous/consistent memory.
	 * because the hardware expects it.
	 */
	pool_size = pdev->frag_descs.pool_elems * pdev->frag_descs.size;
#if WIFI_MEM_MANAGER_SUPPORT
    pdev->frag_descs.pool_vaddr = (char*)wifi_cmem_allocation(soc_idx, 0,
            CM_TX_ATTACH_FRAG_POOL_VADDR,pool_size,(void*) qdf_dev->drv_hdl,&pool_paddr,intr_ctxt);
#else

    pdev->frag_descs.pool_vaddr = qdf_mem_alloc_consistent(
            pdev->osdev, &(((struct pci_dev *)(qdf_dev->drv_hdl))->dev), pool_size, &pool_paddr);
#endif
    pdev->frag_descs.pool_paddr = pool_paddr;

    if (!pdev->frag_descs.pool_vaddr) {
#if !WIFI_MEM_MANAGER_SUPPORT
        qdf_mem_free_consistent(
                pdev->osdev,
                &(((struct pci_dev *)(qdf_dev->drv_hdl))->dev),
                pdev->tx_descs.pool_elems * pdev->tx_descs.size, /* pool_size */
                pdev->tx_descs.pool_vaddr,
                pdev->tx_descs.pool_paddr,
                qdf_get_dma_mem_context((&pdev->tx_descs), memctx));
#else
        wifi_cmem_free(soc_idx, 0, CM_TX_ATTACH_POOL_VADDR, (void *)pdev->tx_descs.pool_vaddr);
#endif
		return 1; /* failure */
	}
	/* link tx descriptors into a freelist */
	pdev->tx_descs.freelist = (u_int32_t *) pdev->tx_descs.pool_vaddr;
	p = (u_int32_t **) pdev->tx_descs.freelist;
	for (i = 0; i < desc_pool_elems - 1; i++) {
		*p = (u_int32_t *) (((char *) p) + pdev->tx_descs.size);
		p = (u_int32_t **) *p;
	}
	*p = NULL;
	htt_tx_mgmt_desc_pool_alloc(pdev, HTT_MAX_NUM_MGMT_DESCS);

#if PEER_FLOW_CONTROL
	htt_tx_fetch_desc_pool_alloc(pdev, HTT_MAX_NUM_FETCH_DESCS);

        if(htt_alloc_peer_map_mem(pdev, 0)) {
            return 1;
        }
#endif
	qdf_spinlock_create(&pdev->htt_buf_list_lock);

	return 0; /* success */
}

void
htt_tx_detach(struct htt_pdev_t *pdev)
{
#if WIFI_MEM_MANAGER_SUPPORT
        uint32_t soc_idx = wlan_ucfg_get_config_param((void *)pdev->ctrl_psoc, SOC_ID);
        wifi_cmem_free(soc_idx, 0, CM_TX_ATTACH_POOL_VADDR, (void *)pdev->tx_descs.pool_vaddr);
        wifi_cmem_free(soc_idx, 0, CM_TX_ATTACH_FRAG_POOL_VADDR, (void *)pdev->frag_descs.pool_vaddr);
#else
        qdf_device_t qdf_dev = wlan_psoc_get_qdf_dev((void *)pdev->ctrl_psoc);
	qdf_mem_free_consistent(
			pdev->osdev,
            &(((struct pci_dev *)(qdf_dev->drv_hdl))->dev),
			pdev->tx_descs.pool_elems * pdev->tx_descs.size, /* pool_size */
			pdev->tx_descs.pool_vaddr,
			pdev->tx_descs.pool_paddr,
			qdf_get_dma_mem_context((&pdev->tx_descs), memctx));

	qdf_mem_free_consistent(
			pdev->osdev,
            &(((struct pci_dev *)(qdf_dev->drv_hdl))->dev),
			pdev->frag_descs.pool_elems * pdev->frag_descs.size, /* pool_size */
			pdev->frag_descs.pool_vaddr,
			pdev->frag_descs.pool_paddr,
			qdf_get_dma_mem_context((&pdev->frag_descs), memctx));
#endif
	htt_tx_mgmt_desc_pool_free(pdev);
#if PEER_FLOW_CONTROL
        htt_tx_fetch_desc_pool_free(pdev);
    /* Peer map cancel the timer and unmap the shared memory between host/target */
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    if(!pdev->nss_wifiol_ctx)
#endif
    {
      qdf_hrtimer_cancel(&((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->qmap_ctxt.htimer);
    }
    OS_DMA_UNMAP_SINGLE(pdev->osdev, pdev->host_q_mem.pool_paddr, pdev->host_q_mem.size,
            QDF_DMA_TO_DEVICE);

    qdf_spinlock_destroy(&((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->peermap_lock);

    qdf_mem_free(pdev->host_q_mem.pool_vaddr);
#endif
}

/*--- descriptor allocation functions ---------------------------------------*/

#define HTT_MSG_BUF_SIZE(msg_bytes) \
   ((msg_bytes) + HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING)

#define HTT_INVALID_VDEV_ID     0xFF

void
htt_tx_mgmt_desc_pool_alloc(struct htt_pdev_t *pdev, A_UINT32 num_elems)
{
    struct htt_tx_mgmt_desc_buf *msg_pool;
    int i = 0;

    msg_pool = (struct htt_tx_mgmt_desc_buf *)qdf_mem_malloc(
                                    (num_elems *
                                    sizeof(struct htt_tx_mgmt_desc_buf)));
    if (!msg_pool) {
        qdf_print("%s,line:%d alloc failed",__func__,__LINE__);
        return ;
    }

    qdf_mem_zero(msg_pool, (num_elems *
                                    sizeof(struct htt_tx_mgmt_desc_buf)));
    pdev->tx_mgmt_desc_ctxt.freelist = msg_pool;

    for(i = 0; i < num_elems; i++) {
        msg_pool[i].msg_buf = qdf_nbuf_alloc(pdev->osdev,
                                             HTT_MSG_BUF_SIZE(sizeof
                                             (struct htt_mgmt_tx_desc_t)),
       				     /* reserve room for HTC header */
                                             HTC_HEADER_LEN +
                                             HTC_HDR_ALIGNMENT_PADDING ,
                                             4,
                                             TRUE);
        qdf_nbuf_put_tail(msg_pool[i].msg_buf,
                          sizeof(struct htt_mgmt_tx_desc_t));
        msg_pool[i].desc_id = i;
        msg_pool[i].vdev_id = HTT_INVALID_VDEV_ID;
        msg_pool[i].delayed_free = HTT_TX_MGMT_DELAYED_FREE_FALSE;

        if (i < (num_elems - 1)) {
            msg_pool[i].next = &msg_pool[i + 1];
        } else {
            msg_pool[i].next = NULL;
        }
    }


    pdev->tx_mgmt_desc_ctxt.pool = msg_pool;
    pdev->tx_mgmt_desc_ctxt.pending_cnt = 0;
}


A_UINT32
htt_tx_mgmt_pending_frames_cnt(struct htt_pdev_t *pdev)
{
    return pdev->tx_mgmt_desc_ctxt.pending_cnt;
}

qdf_nbuf_t
htt_tx_mgmt_desc_alloc(struct htt_pdev_t *pdev, A_UINT8 vdev_id,
                       A_UINT32 *desc_id, qdf_nbuf_t mgmt_frm)
{
    qdf_nbuf_t msg_buf;

    /* acquire the lock */
    qdf_spin_lock_bh(&pdev->htt_tx_mutex);

    if (pdev->tx_mgmt_desc_ctxt.freelist) {

        pdev->tx_mgmt_desc_ctxt.freelist->is_inuse = TRUE;
        pdev->tx_mgmt_desc_ctxt.freelist->vdev_id = vdev_id;
        pdev->tx_mgmt_desc_ctxt.freelist->delayed_free = HTT_TX_MGMT_DELAYED_FREE_FALSE;
        pdev->tx_mgmt_desc_ctxt.freelist->mgmt_frm = mgmt_frm;
        pdev->tx_mgmt_desc_ctxt.pending_cnt++;
        msg_buf = pdev->tx_mgmt_desc_ctxt.freelist->msg_buf;
        *desc_id = pdev->tx_mgmt_desc_ctxt.freelist->desc_id;
        pdev->tx_mgmt_desc_ctxt.freelist =
        pdev->tx_mgmt_desc_ctxt.freelist->next;

        qdf_spin_unlock_bh(&pdev->htt_tx_mutex);
        qdf_mem_zero(qdf_nbuf_data(msg_buf), sizeof(struct htt_mgmt_tx_desc_t));
        return msg_buf;
    }

    /* We do not have any buffers available for now */
    qdf_spin_unlock_bh(&pdev->htt_tx_mutex);

	/*
	 * Ok Now Queuing in HTT and no need to Free it now
	 * Failed print is not necessary.
     */
    return NULL;
}

void
htt_tx_mgmt_desc_free(struct htt_pdev_t *pdev, A_UINT8 desc_id, A_UINT32 status)
{
    ol_txrx_mgmt_tx_cb  cb = NULL;
    qdf_nbuf_t    mgmt_frm, mgmt_frm_cpy;
    A_UINT16 mgmt_type = (OL_TXRX_MGMT_NUM_TYPES-1);
    A_UINT8 delayed_free = HTT_TX_MGMT_DELAYED_FREE_FALSE;
    void *ctxt = ((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->tx_mgmt.callbacks[mgmt_type].ctxt;

    cb = ((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->tx_mgmt.callbacks[mgmt_type].ota_ack_cb;

    ASSERT(desc_id < HTT_MAX_NUM_MGMT_DESCS);

    /* acquire lock here */
    qdf_spin_lock_bh(&pdev->htt_tx_mutex);
    if(pdev->tx_mgmt_desc_ctxt.pool[desc_id].mgmt_frm == NULL){
        qdf_spin_unlock_bh(&pdev->htt_tx_mutex);
        return;
    }
    mgmt_frm = pdev->tx_mgmt_desc_ctxt.pool[desc_id].mgmt_frm;
    delayed_free = pdev->tx_mgmt_desc_ctxt.pool[desc_id].delayed_free;

    if (delayed_free == HTT_TX_MGMT_DELAYED_FREE_PENDING){
        pdev->tx_mgmt_desc_ctxt.pool[desc_id].delayed_free = HTT_TX_MGMT_DELAYED_FREE_TXCOMPL;
        qdf_spin_unlock_bh(&pdev->htt_tx_mutex);

        /*
         * Clone and give it for tx capture
         */
        if (mgmt_frm &&(((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->tx_capture)
                && pdev->htt_process_nondata_tx_frames) {
            mgmt_frm_cpy = qdf_nbuf_clone((qdf_nbuf_t)mgmt_frm);
            if (mgmt_frm_cpy) {
                pdev->htt_process_nondata_tx_frames(mgmt_frm_cpy);
            }
        }

        return;
    }
    pdev->tx_mgmt_desc_ctxt.pool[desc_id].delayed_free = HTT_TX_MGMT_DELAYED_FREE_FALSE;
    pdev->tx_mgmt_desc_ctxt.pool[desc_id].is_inuse = FALSE;
    pdev->tx_mgmt_desc_ctxt.pool[desc_id].vdev_id = HTT_INVALID_VDEV_ID;
    if(!mgmt_frm){
        qdf_spin_unlock_bh(&pdev->htt_tx_mutex);
        qdf_print("ERROR: %s: unexpected mgmt frame: NULL for desc_id: %d", __func__, desc_id);
        return;
    }
    /* free the management frame */
    pdev->tx_mgmt_desc_ctxt.pending_cnt--;

    pdev->tx_mgmt_desc_ctxt.pool[desc_id].mgmt_frm = NULL;

    qdf_nbuf_unmap_single(((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->osdev, mgmt_frm, QDF_DMA_TO_DEVICE);
    /* pull adf here by HTT_HTC_HDR_SIZE bytes */
    qdf_nbuf_pull_head(pdev->tx_mgmt_desc_ctxt.pool[desc_id].msg_buf,
                       (HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING));

    pdev->tx_mgmt_desc_ctxt.pool[desc_id].next =
                                 pdev->tx_mgmt_desc_ctxt.freelist;
    pdev->tx_mgmt_desc_ctxt.freelist = &pdev->tx_mgmt_desc_ctxt.pool[desc_id];
    qdf_spin_unlock_bh(&pdev->htt_tx_mutex);

    if (delayed_free == HTT_TX_MGMT_DELAYED_FREE_TRUE) {
        status = IEEE80211_SKB_FREE_ONLY;
    }

    /* call back function to freeup management frame */
    if (cb){
        cb(ctxt, mgmt_frm, status);
    }

}
EXPORT_SYMBOL(htt_tx_mgmt_desc_free);


A_UINT32
htt_tx_mgmt_desc_drain(struct htt_pdev_t *pdev,
                           A_UINT8 vdev_id)
{
    int desc_id = 0;

    for(desc_id = 0; desc_id < HTT_MAX_NUM_MGMT_DESCS; desc_id++) {
        if((pdev->tx_mgmt_desc_ctxt.pool[desc_id].mgmt_frm == NULL) ||
           (pdev->tx_mgmt_desc_ctxt.pool[desc_id].vdev_id != vdev_id)) {
            continue;
        }

        htt_tx_mgmt_desc_free(pdev, desc_id, IEEE80211_TX_ERROR);
    }
    return 0;
}
EXPORT_SYMBOL(htt_tx_mgmt_desc_drain);

A_UINT32
htt_tx_mgmt_desc_mark_delayed_free(struct htt_pdev_t *pdev,
                                   A_UINT8 vdev_id,
                                   A_UINT32 status)
{
    ol_txrx_mgmt_tx_cb  cb = NULL;
    qdf_nbuf_t    mgmt_frm;
    A_UINT16 mgmt_type = (OL_TXRX_MGMT_NUM_TYPES-1);
    void *ctxt = ((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->tx_mgmt.callbacks[mgmt_type].ctxt;
    int desc_id = 0;
    A_UINT32 matches = 0;

    cb = ((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->tx_mgmt.callbacks[mgmt_type].ota_ack_cb;

    for(desc_id = 0; desc_id < HTT_MAX_NUM_MGMT_DESCS; desc_id++) {
        /* acquire lock here */
        qdf_spin_lock_bh(&pdev->htt_tx_mutex);

        /*
         * Here delayed_free can have 4 states : HTT_TX_MGMT_DELAYED_FREE_FALSE
         *                                       HTT_TX_MGMT_DELAYED_FREE_TRUE
         *                                       HTT_TX_MGMT_DELAYED_FREE_PENDING
         *                                       HTT_TX_MGMT_DELAYED_FREE_TXCOMPL
         *
         * Initially the delayed free will have value HTT_TX_MGMT_DELAYED_FREE_FALSE.
         * 1. If the completion for the mgmt frame comes first, before this function
         *    is invoked, htt_tx_mgmt_desc_free will clean-up and free the mgmt frame.
         * 2. If this function is invoked first (presently from a process context),
         *    it acquires htt_tx_mutex lock, sets delayed free to HTT_TX_MGMT_DELAYED_FREE_PENDING,
         *    unlock the htt_tx_mutex, and goes on to call ota_ack_cb for the mgmt frame.
         *    (This ota_ack_cb might try to send some mgmt frames again, there by trying to acqiure
         *     htt_tx_mutex again; So, it is important ota_ack_cb is called without holding
         *     the htt_tx_mutex lock).
         *     Now,
         *     i)  If the Tx completion comes for the mgmt frame before we aquire
         *         htt_tx_mutex lock again, htt_tx_mgmt_desc_free will set delayed_free to
         *         HTT_TX_MGMT_DELAYED_FREE_TXCOMPL and simply exits without freeing
         *         the mgmt frame, without cleaning up anything. After this we will come back
         *         and acquire htt_tx_mutex lock, do the cleanup and free the mgmt frame.
         *     ii) If the Tx completion comes for the mgmt frame after we aquire
         *         htt_tx_mutex lock again, It will wait until we set the delayed free
         *         to HTT_TX_MGMT_DELAYED_FREE_TRUE. Then htt_tx_mgmt_desc_free acquires
         *         htt_tx_mutex lock, and does the cleanup and frees the mgmt frame.
         */

        /*
         * We don't need notify umac layer completion handler if any
         * of below conditions are true. If any of these conditions become
         * TRUE, it simply means we have already invoked umac layer handler.
         */
        if((pdev->tx_mgmt_desc_ctxt.pool[desc_id].mgmt_frm == NULL) ||
           (pdev->tx_mgmt_desc_ctxt.pool[desc_id].vdev_id != vdev_id) ||
           (pdev->tx_mgmt_desc_ctxt.pool[desc_id].delayed_free != HTT_TX_MGMT_DELAYED_FREE_FALSE)){
            qdf_spin_unlock_bh(&pdev->htt_tx_mutex);
            continue;
        }
        ++matches;
        pdev->tx_mgmt_desc_ctxt.pool[desc_id].delayed_free = HTT_TX_MGMT_DELAYED_FREE_PENDING;
        mgmt_frm = pdev->tx_mgmt_desc_ctxt.pool[desc_id].mgmt_frm;

        qdf_spin_unlock_bh(&pdev->htt_tx_mutex);

        if (cb){
            cb(ctxt, mgmt_frm, status);
        }

        qdf_spin_lock_bh(&pdev->htt_tx_mutex);

        if (pdev->tx_mgmt_desc_ctxt.pool[desc_id].delayed_free == HTT_TX_MGMT_DELAYED_FREE_TXCOMPL)
        {
            pdev->tx_mgmt_desc_ctxt.pool[desc_id].delayed_free = HTT_TX_MGMT_DELAYED_FREE_FALSE;
            pdev->tx_mgmt_desc_ctxt.pool[desc_id].is_inuse = FALSE;
            pdev->tx_mgmt_desc_ctxt.pool[desc_id].vdev_id = HTT_INVALID_VDEV_ID;

            pdev->tx_mgmt_desc_ctxt.pending_cnt--;

            pdev->tx_mgmt_desc_ctxt.pool[desc_id].mgmt_frm = NULL;

            qdf_nbuf_unmap_single( pdev->osdev, pdev->tx_mgmt_desc_ctxt.pool[desc_id].msg_buf , QDF_DMA_TO_DEVICE);
            /* pull adf here by HTT_HTC_HDR_SIZE bytes */
            qdf_nbuf_pull_head(pdev->tx_mgmt_desc_ctxt.pool[desc_id].msg_buf,
                    (HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING));

            pdev->tx_mgmt_desc_ctxt.pool[desc_id].next =
                pdev->tx_mgmt_desc_ctxt.freelist;
            pdev->tx_mgmt_desc_ctxt.freelist = &pdev->tx_mgmt_desc_ctxt.pool[desc_id];

            /* Unmap mgmt frame */
            qdf_nbuf_unmap_single(((struct ol_txrx_pdev_t *)pdev->txrx_pdev)->osdev, mgmt_frm, QDF_DMA_TO_DEVICE);

            qdf_spin_unlock_bh(&pdev->htt_tx_mutex);

            /* call back function to freeup management frame */
            if (cb){
                cb(ctxt, mgmt_frm, IEEE80211_SKB_FREE_ONLY);
            }
        }
        else {
            pdev->tx_mgmt_desc_ctxt.pool[desc_id].delayed_free = HTT_TX_MGMT_DELAYED_FREE_TRUE;
            qdf_spin_unlock_bh(&pdev->htt_tx_mutex);
        }
    }
    return matches;
}
EXPORT_SYMBOL(htt_tx_mgmt_desc_mark_delayed_free);


void
htt_tx_mgmt_desc_pool_free(struct htt_pdev_t *pdev)
{
    A_UINT16 i = 0;
    struct htt_tx_mgmt_desc_buf *msg_pool;
#define TX_MGMT_DONE_WAIT_TIMEOUT 100
    for (i = 0; i < TX_MGMT_DONE_WAIT_TIMEOUT; i++) {
        if (!pdev->tx_mgmt_desc_ctxt.pending_cnt) {
            break;
        }
        OS_DELAY(10);
    }

    msg_pool = pdev->tx_mgmt_desc_ctxt.pool ;
    for(i = 0; i < HTT_MAX_NUM_MGMT_DESCS ; i++) {
        qdf_nbuf_free(msg_pool[i].msg_buf);
    }

    qdf_mem_free(pdev->tx_mgmt_desc_ctxt.pool);
    pdev->tx_mgmt_desc_ctxt.freelist = NULL;
}

#if PEER_FLOW_CONTROL
/* Pool allocation for htt fetch response. No real hw desc associated with this */

void
htt_tx_fetch_desc_pool_alloc(struct htt_pdev_t *pdev, A_UINT32 num_elems)
{
    struct htt_tx_fetch_desc_buf *msg_pool;
    int i = 0;

    msg_pool = (struct htt_tx_fetch_desc_buf *)qdf_mem_malloc(
                                    (num_elems *
                                    sizeof(struct htt_tx_fetch_desc_buf)));
    if (!msg_pool) {
        qdf_print("%s,line:%d alloc failed",__func__,__LINE__);
        return;
    }

    qdf_mem_zero(msg_pool, (num_elems *
                                    sizeof(struct htt_tx_fetch_desc_buf)));
    pdev->tx_fetch_desc_ctxt.freelist = msg_pool;

    for(i = 0; i < num_elems; i++) {

        msg_pool[i].desc_id = i;

        if (i < (num_elems - 1)) {
            msg_pool[i].next = &msg_pool[i + 1];
        } else {
            msg_pool[i].next = NULL;
        }
    }


    pdev->tx_fetch_desc_ctxt.pool = msg_pool;
    pdev->tx_fetch_desc_ctxt.pending_cnt = 0;
}


struct htt_tx_fetch_desc_buf *
htt_tx_fetch_desc_alloc(struct htt_pdev_t *pdev)
{
    struct htt_tx_fetch_desc_buf *fetch_desc = NULL;

    /* acquire the lock */
    qdf_spin_lock_bh(&pdev->htt_tx_mutex);

    if (pdev->tx_fetch_desc_ctxt.freelist) {

        pdev->tx_fetch_desc_ctxt.pending_cnt++;
		pdev->tx_fetch_desc_ctxt.freelist->msg_buf = NULL;
        pdev->tx_fetch_desc_ctxt.freelist->is_inuse = TRUE;
        fetch_desc = pdev->tx_fetch_desc_ctxt.freelist;

        pdev->tx_fetch_desc_ctxt.freelist =
        pdev->tx_fetch_desc_ctxt.freelist->next;

    }

    /* We do not have any buffers available for now */
    qdf_spin_unlock_bh(&pdev->htt_tx_mutex);

    return fetch_desc;
}

A_UINT32
htt_tx_fetch_pending_frames_cnt(struct htt_pdev_t *pdev)
{
    return pdev->tx_fetch_desc_ctxt.pending_cnt;
}

void
htt_tx_fetch_desc_pool_free(struct htt_pdev_t *pdev)
{
    A_UINT16 i = 0;
    struct htt_tx_fetch_desc_buf *msg_pool;

#define TX_FETCH_DONE_WAIT_TIMEOUT 100
    for (i = 0; i < TX_FETCH_DONE_WAIT_TIMEOUT; i++) {
        if (!pdev->tx_fetch_desc_ctxt.pending_cnt) {
            break;
        }
        OS_DELAY(10);
    }

    msg_pool = pdev->tx_fetch_desc_ctxt.pool ;
    for(i = 0; i < HTT_MAX_NUM_FETCH_DESCS ; i++) {
		if(msg_pool[i].is_inuse)
			qdf_nbuf_free(msg_pool[i].msg_buf);
    }

    qdf_mem_free(pdev->tx_fetch_desc_ctxt.pool);
    pdev->tx_fetch_desc_ctxt.freelist = NULL;

}

void
htt_tx_fetch_desc_free(struct htt_pdev_t *pdev, A_UINT32 desc_id)
{
    struct htt_tx_fetch_desc_buf *msg_pool;
    qdf_nbuf_t msg_buf;

    qdf_assert(desc_id < HTT_MAX_NUM_FETCH_DESCS);
    msg_pool = pdev->tx_fetch_desc_ctxt.pool ;
    /* acquire lock here */
    qdf_spin_lock_bh(&pdev->htt_tx_mutex);

    msg_buf = pdev->tx_fetch_desc_ctxt.pool[desc_id].msg_buf;
    if(!msg_buf){
        qdf_spin_unlock_bh(&pdev->htt_tx_mutex);
        qdf_print("\n %s : msg_buf NULL for desc id %d",__FUNCTION__, desc_id);
        return;
    }

    pdev->tx_fetch_desc_ctxt.pool[desc_id].is_inuse = FALSE;

    /* free the management frame */
    pdev->tx_fetch_desc_ctxt.pending_cnt--;

    /* do we need pull here */
    /* pull adf here by HTT_HTC_HDR_SIZE bytes */
    /* adf_nbuf_pull_head(pdev->tx_fetch_desc_ctxt.pool[desc_id].msg_buf, */
    /* (HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING)); */

    qdf_nbuf_unmap_single( pdev->osdev, pdev->tx_fetch_desc_ctxt.pool[desc_id].msg_buf , QDF_DMA_TO_DEVICE);
	qdf_nbuf_free(msg_pool[desc_id].msg_buf);

    pdev->tx_fetch_desc_ctxt.pool[desc_id].next =
                                 pdev->tx_fetch_desc_ctxt.freelist;
    pdev->tx_fetch_desc_ctxt.freelist = &pdev->tx_fetch_desc_ctxt.pool[desc_id];
    qdf_spin_unlock_bh(&pdev->htt_tx_mutex);

}

#endif
void *
htt_tx_desc_pool_paddr(htt_pdev_handle pdev){
    return (void *)(uintptr_t)pdev->frag_descs.pool_paddr;
}

uint32_t
htt_tx_desc_frag_desc_size(htt_pdev_handle pdev){
    return pdev->frag_descs.size;
}


void *
htt_tx_desc_assign(htt_pdev_handle pdev, u_int16_t index, u_int32_t *paddr_lo)
{
    struct htt_host_tx_desc_t *htt_host_tx_desc; /* includes HTC hdr space */
    struct htt_tx_msdu_desc_t *htt_tx_desc;      /* doesn't include HTC hdr */

    if (index >= pdev->tx_descs.pool_elems) {
        return NULL;
    }

    htt_host_tx_desc = ((struct htt_host_tx_desc_t *) pdev->tx_descs.pool_vaddr) + index;
    htt_tx_desc = &htt_host_tx_desc->align32.tx_desc;

    /*
     * For LL, set up the fragmentation descriptor address.
     * Currently, this HTT tx desc allocation is performed once up front.
     * If this is changed to have the allocation done during tx, then it
     * would be helpful to have separate htt_tx_desc_alloc functions for
     * HL vs. LL, to remove the below conditional branch.
     */
    if (!pdev->cfg.is_high_latency) {
        u_int32_t *fragmentation_descr_field_ptr;

        fragmentation_descr_field_ptr = (u_int32_t *) (uintptr_t)
            ((u_int32_t *) htt_tx_desc) +
            HTT_TX_DESC_FRAGS_DESC_PADDR_OFFSET_DWORD;
        /*
         * The fragmentation descriptor is allocated from consistent mem.
         * Therefore, we can use the address directly rather than having
         * to map it from a virtual/CPU address to a physical/bus address.
         */

        *fragmentation_descr_field_ptr = (uintptr_t) (((char *)(uintptr_t)pdev->frag_descs.pool_paddr) + (pdev->frag_descs.size * index));

    }
   	/*
     * Include the headroom for the HTC frame header when specifying the
     * physical address for the HTT tx descriptor.
     */
    *paddr_lo = (u_int32_t) HTT_TX_DESC_PADDR(pdev, htt_host_tx_desc);
    /*
     * The allocated tx descriptor space includes headroom for a
     * HTC frame header.  Hide this headroom, so that we don't have
     * to jump past the headroom each time we program a field within
     * the tx desc, but only once when we download the tx desc (and
     * the headroom) to the target via HTC.
     * Skip past the headroom and return the address of the HTT tx desc.
     */
    return (void *) htt_tx_desc;
}

void *
htt_tx_frag_assign(htt_pdev_handle pdev, u_int16_t index)
{
    /** Index should never be 0, since its used by the hardware to terminate the link. */
    if (index >= pdev->tx_descs.pool_elems) {
        return NULL;
    }

    return (((char *) pdev->frag_descs.pool_vaddr) + (pdev->frag_descs.size * index));
}
/*--- descriptor field access methods ---------------------------------------*/

/* PUT THESE AS INLINE IN ol_htt_tx_api.h */

void
htt_tx_desc_flag_postponed(htt_pdev_handle pdev, void *desc)
{
}

void
htt_tx_desc_flag_batch_more(htt_pdev_handle pdev, void *desc)
{
}

/*--- tx send function ------------------------------------------------------*/

#if ATH_11AC_TXCOMPACT


/* Scheduling the Queued packets in HTT which could not be sent out because of No CE desc*/
void
htt_tx_sched(htt_pdev_handle pdev)
{
    qdf_nbuf_t msdu;
    int download_len = pdev->download_len;
    int packet_len;

    HTT_TX_NBUF_QUEUE_REMOVE(pdev, msdu);
    while (msdu != NULL){
        /* packet length includes HTT tx desc frag added above */
        packet_len = qdf_nbuf_len(msdu);
        if (packet_len < download_len) {
            /*
            * This case of packet length being less than the nominal download
            * length can happen for a couple reasons:
            * In HL, the nominal download length is a large artificial value.
            * In LL, the frame may not have the optional header fields
            * accounted for in the nominal download size (LLC/SNAP header,
            * IPv4 or IPv6 header).
             */
            download_len = packet_len;
        }
        if(htc_send_data_pkt(pdev->htc_pdev, msdu, pdev->htc_endpoint, download_len)){
            HTT_TX_NBUF_QUEUE_INSERT_HEAD(pdev, msdu);
            return;
        }
        HTT_TX_NBUF_QUEUE_REMOVE(pdev, msdu);
    }
}


int
htt_tx_send_std(
        htt_pdev_handle pdev,
        void *desc,
        qdf_nbuf_t msdu,
        u_int16_t msdu_id)
{

    int download_len = pdev->download_len;

    struct htt_host_tx_desc_t *htt_host_tx_desc;
    int packet_len;

    htt_host_tx_desc = (struct htt_host_tx_desc_t *)
        (((char *) desc) -
         offsetof(struct htt_host_tx_desc_t, align32.tx_desc));

    /*
    * Specify that the data provided by the OS is a bytestream,
    * and thus should not be byte-swapped during the HIF download
    * even if the host is big-endian.
    * There could be extra fragments added before the OS's fragments,
    * e.g. for TSO, so it's incorrect to clear the frag 0 wordstream flag.
    * Instead, clear the wordstream flag for the final fragment, which
    * is certain to be (one of the) fragment(s) provided by the OS.
    * Setting the flag for this final fragment suffices for specifying
    * all fragments provided by the OS rather than added by the driver.
     */
    qdf_nbuf_set_frag_is_wordstream(msdu, qdf_nbuf_get_num_frags(msdu) - 1, 0);
    qdf_nbuf_frag_push_head(
            msdu,
            sizeof(struct htt_host_tx_desc_t),
            (char *) htt_host_tx_desc, /* virtual addr */
            (u_int32_t) HTT_TX_DESC_PADDR(pdev, htt_host_tx_desc));

    /*
    * Indicate that the HTT header (and HTC header) is a meta-data
    * "wordstream", i.e. series of u_int32_t, rather than a data
    * bytestream.
    * This allows the HIF download to byteswap the HTT + HTC headers if
    * the host is big-endian, to convert to the target's little-endian
    * format.
     */
    qdf_nbuf_set_frag_is_wordstream(msdu, 0, 1);

    /* packet length includes HTT tx desc frag added above */
    packet_len = qdf_nbuf_len(msdu);
    if (packet_len < download_len) {
        /*
        * This case of packet length being less than the nominal download
        * length can happen for a couple reasons:
        * In HL, the nominal download length is a large artificial value.
        * In LL, the frame may not have the optional header fields
        * accounted for in the nominal download size (LLC/SNAP header,
        * IPv4 or IPv6 header).
         */
        download_len = packet_len;
    }
    if (qdf_nbuf_queue_len(&pdev->txnbufq) > 0){
        HTT_TX_NBUF_QUEUE_ADD(pdev, msdu);
        htt_tx_sched(pdev);
        return 0;
    }
    if (htc_send_data_pkt(pdev->htc_pdev, msdu, pdev->htc_endpoint, download_len)){
        HTT_TX_NBUF_QUEUE_ADD(pdev, msdu);
    }

    return 0; /* success */

}

#if  BIG_ENDIAN_HOST
#define htt_h2t_header_swap(_buff, _bytes) \
    do { \
        int i;\
        int *word = (int *) _buff;\
        for (i = 0; i < _bytes/4; i++){\
            word[i] =  qdf_cpu_to_le32(word[i]);\
        }\
    }while(0);

#else
#define htt_h2t_header_swap(_buff, _bytes)
#endif
void
htt_ff_cache_init(
        htt_pdev_handle pdev,
        enum htt_pkt_type pkt_type)
{
    u_int32_t htc_htt_hdr_size = ((((HTC_HTT_TRANSFER_HDRSIZE + HTT_EXTND_DESC_SIZE) + 3)/4) * 4);

    HTC_FRAME_HDR * pHtcHdr;

    unsigned char * buff = (unsigned char *) pdev->htt_hdr_cache;

    qdf_mem_zero(buff,htc_htt_hdr_size);

    pHtcHdr = (HTC_FRAME_HDR *) buff;
    HTC_WRITE32(pHtcHdr, SM( pdev->htc_endpoint, HTC_FRAME_HDR_ENDPOINTID));

    htt_tx_desc_init(
            pdev, (buff + HTC_HEADER_LEN),
            0,
            0,
            0,
            pkt_type,
            QDF_NBUF_TX_CKSUM_NONE,
            QDF_NBUF_TX_EXT_TID_INVALID);

    htt_h2t_header_swap(buff, htc_htt_hdr_size);
}
void htt_hdrcache_update(
        htt_pdev_handle pdev,
        int *hdrcache)
{
    u_int32_t htc_htt_hdr_size = ((((HTC_HTT_TRANSFER_HDRSIZE + HTT_EXTND_DESC_SIZE) + 3)/4) * 4);

	qdf_mem_copy(hdrcache, pdev->htt_hdr_cache, htc_htt_hdr_size);
}

void
htt_hdrcache_update_pkttype(
        int *hdrcache,
        enum htt_pkt_type pkt_type)
{
    htt_ffcache_update_pkttype(hdrcache, pkt_type);
}

void
htt_hdrcache_update_pktsubtype(
        int *hdrcache,
        u_int8_t pkt_subtype)
{
    htt_ffcache_update_pktsubtype(hdrcache, pkt_subtype);
}


#else  /*ATH_11AC_TXCOMPACT*/

static inline int
htt_tx_send_base(
    htt_pdev_handle pdev,
    void *desc,
    qdf_nbuf_t msdu,
    u_int16_t msdu_id,
    int download_len)
{
    struct htt_host_tx_desc_t *htt_host_tx_desc;
    //struct htt_tx_msdu_desc_t *htt_tx_desc = desc;
    struct htt_htc_pkt *pkt;
    int packet_len;

     htt_host_tx_desc = (struct htt_host_tx_desc_t *)
         (((char *) desc) -
         offsetof(struct htt_host_tx_desc_t, align32.tx_desc));

    /*
     * alternative -
     * Have the caller (ol_tx) provide an empty HTC_PACKET struct
     * (allocated as part of the ol_tx_desc).
     */
    pkt = htt_htc_pkt_alloc(pdev);
    if (!pkt) {
        return 1; /* failure */
    }

    pkt->msdu_id = msdu_id;
    pkt->pdev_ctxt = pdev->txrx_pdev;

    /*
     * Specify that the data provided by the OS is a bytestream,
     * and thus should not be byte-swapped during the HIF download
     * even if the host is big-endian.
     * There could be extra fragments added before the OS's fragments,
     * e.g. for TSO, so it's incorrect to clear the frag 0 wordstream flag.
     * Instead, clear the wordstream flag for the final fragment, which
     * is certain to be (one of the) fragment(s) provided by the OS.
     * Setting the flag for this final fragment suffices for specifying
     * all fragments provided by the OS rather than added by the driver.
     */
    qdf_nbuf_set_frag_is_wordstream(msdu, qdf_nbuf_get_num_frags(msdu) - 1, 0);
    qdf_nbuf_frag_push_head(
        msdu,
        sizeof(struct htt_host_tx_desc_t),
        (char *) htt_host_tx_desc, /* virtual addr */
        (u_int32_t) HTT_TX_DESC_PADDR(pdev, htt_host_tx_desc));
    /*
     * Indicate that the HTT header (and HTC header) is a meta-data
     * "wordstream", i.e. series of u_int32_t, rather than a data
     * bytestream.
     * This allows the HIF download to byteswap the HTT + HTC headers if
     * the host is big-endian, to convert to the target's little-endian
     * format.
     */
    qdf_nbuf_set_frag_is_wordstream(msdu, 0, 1);

    /* packet length includes HTT tx desc frag added above */
    packet_len = qdf_nbuf_len(msdu);
    if (packet_len < download_len) {
        /*
         * This case of packet length being less than the nominal download
         * length can happen for a couple reasons:
         * In HL, the nominal download length is a large artificial value.
         * In LL, the frame may not have the optional header fields
         * accounted for in the nominal download size (LLC/SNAP header,
         * IPv4 or IPv6 header).
         */
        download_len = packet_len;
    }

    SET_HTC_PACKET_INFO_TX(
        &pkt->htc_pkt,
        pdev->tx_send_complete_part2,
        (unsigned char *) htt_host_tx_desc,
        download_len - HTC_HDR_LENGTH,
        pdev->htc_endpoint,
        1); /* tag - not relevant here */

    SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msdu);

    htc_send_data_pkt(pdev->htc_pdev, &pkt->htc_pkt);

    return 0; /* success */
}

int
htt_tx_send_std(
    htt_pdev_handle pdev,
    void *desc,
    qdf_nbuf_t msdu,
    u_int16_t msdu_id)
{
    return htt_tx_send_base(pdev, desc, msdu, msdu_id, pdev->download_len);
}

int
htt_tx_send_nonstd(
    htt_pdev_handle pdev,
    void *desc, qdf_nbuf_t msdu,
    u_int16_t msdu_id,
    enum htt_pkt_type pkt_type)
{
    int download_len;

    download_len =
        sizeof(struct htt_host_tx_desc_t) +
        HTT_TX_HDR_SIZE_OUTER_HDR_MAX + /* worst case */
        HTT_TX_HDR_SIZE_802_1Q +
        HTT_TX_HDR_SIZE_LLC_SNAP +
        ol_cfg_tx_download_size(pdev->ctrl_psoc);
    return htt_tx_send_base(pdev, desc, msdu, msdu_id, download_len);
}

#endif /*ATH_11AC_TXCOMPACT*/
#ifdef HTT_DBG
void
htt_tx_desc_display(htt_pdev_handle pdev, void *tx_desc)
{
    struct htt_tx_msdu_desc_t *htt_tx_desc;
    A_UINT32 frags_desc_ptr;

    htt_tx_desc = (struct htt_tx_msdu_desc_t *) tx_desc;

    /* only works for little-endian */
    qdf_print("HTT tx desc (@ %pK):", htt_tx_desc);
    qdf_print("  msg type = %d", htt_tx_desc->msg_type);
    qdf_print("  pkt subtype = %d", htt_tx_desc->pkt_subtype);
    qdf_print("  pkt type = %d", htt_tx_desc->pkt_type);
    qdf_print("  vdev ID = %d", htt_tx_desc->vdev_id);
    qdf_print("  ext TID = %d", htt_tx_desc->ext_tid);
    qdf_print("  postponed = %d", htt_tx_desc->postponed);
    qdf_print("  batch more = %d", htt_tx_desc->more_in_batch);
    qdf_print("  length = %d", htt_tx_desc->len);
    qdf_print("  id = %d", htt_tx_desc->id);
    frags_desc_ptr = (A_UINT32) htt_tx_get_va_frag_desc(pdev, htt_tx_desc->id);
    qdf_print("  frag desc addr = %#x", frags_desc_ptr);
    if (!HTT_WIFI_IP(pdev,2,0)) {
        if (frags_desc_ptr) {
            int frag = 0;
            u_int32_t *base;
            u_int32_t addr;
            u_int32_t len;
            do {
                base = ((u_int32_t *) htt_tx_desc->frags_desc_ptr) + (frag * 2);
                addr = *base;
                len = *(base + 1);
                if (addr) {
                    qdf_print(
                            "    frag %d: addr = %#x, len = %d", frag, addr, len);
                }
                frag++;
            } while (addr);
        }
    }
}
#endif
