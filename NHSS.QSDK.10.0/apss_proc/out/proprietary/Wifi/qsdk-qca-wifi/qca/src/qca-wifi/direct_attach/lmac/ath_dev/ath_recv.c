/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2009, Atheros Communications Inc.
 * All Rights Reserved.
 */

/*
 *  Implementation of receive path in atheros OS-independent layer.
 */

#include "ath_internal.h"

#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#endif

#if WLAN_SPECTRAL_ENABLE
#include "spectral.h"
#endif

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

#if ATH_SUPPORT_KEYPLUMB_WAR
#define     MAX_DECRYPT_ERR_KEYPLUMB   15   /* Max. decrypt error before plumbing the hw key again */
#endif

static u_int8_t ath_rx_detect_antenna(struct ath_softc *sc, struct ath_rx_status *rxstat);

#if ATH_VOW_EXT_STATS
void ath_add_ext_stats(struct ath_rx_status *rxs, wbuf_t wbuf, struct ath_softc *sc,
                       struct ath_phy_stats *phy_stats, ieee80211_rx_status_t *rx_status);
#endif

#if ATH_SUPPORT_CFEND
extern void ath_check_n_send_cfend(struct ath_softc *sc, struct ath_rx_status *rxs,
                                   int proc_desc_status, struct ath_buf *bf,
                                   u_int8_t *bssid);
#endif

#if UMAC_SUPPORT_VI_DBG
static void ath_add_vi_dbg_stats(struct ath_softc *sc, const struct ath_rx_status *rxs);
#else
#define ath_add_vi_dbg_stats(sc, rxsp)
#endif

#if ATH_SUPPORT_KEYPLUMB_WAR
extern void ath_keycache_print(ath_dev_t);
extern int ath_checkandplumb_key(ath_dev_t, ath_node_t, u_int16_t);
#endif
#if UNIFIED_SMARTANTENNA
static inline uint32_t ath_smart_ant_rxfeedback(struct ath_softc *sc,  wbuf_t wbuf, struct ath_rx_status *rxs, uint32_t pkts);
#endif

int
ath_rx_init(ath_dev_t dev, int nbufs)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    wbuf_t wbuf;
    struct ath_buf *bf;
    int error = 0;

    do {
        ATH_RXFLUSH_LOCK_INIT(sc);
        sc->sc_rxflush = 0;
        ATH_RXBUF_LOCK_INIT(sc);

        /*
         * Cisco's VPN software requires that drivers be able to
         * receive encapsulated frames that are larger than the MTU.
         * Since we can't be sure how large a frame we'll get, setup
         * to handle the larges on possible.
         */
        sc->sc_rxbufsize = IEEE80211_MAX_MPDU_LEN;

        DPRINTF(sc,ATH_DEBUG_RESET, "%s: cachelsz %u rxbufsize %u\n",
                __func__, sc->sc_cachelsz, sc->sc_rxbufsize);

        /*
         * Initialize rx descriptors
         */
        error = ath_descdma_setup(sc, &sc->sc_rxdma, &sc->sc_rxbuf,
                                  "rx", nbufs, 1, 0, ATH_FRAG_PER_MSDU);
        if (error != 0) {
            DPRINTF(sc, ATH_DEBUG_ANY,
                    "failed to allocate rx descriptors: %d\n", error);
            break;
        }

        sc->sc_num_rxbuf = nbufs;

        /*
         * Pre-allocate a wbuf for each rx buffer
         */
        TAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
            wbuf = ath_rxbuf_alloc(sc, sc->sc_rxbufsize);
            if (wbuf == NULL) {
                error = -ENOMEM;
                break;
            }

            bf->bf_mpdu = wbuf;
            bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, wbuf, BUS_DMA_FROMDEVICE,
                                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            ATH_SET_RX_CONTEXT_BUF(wbuf, bf);
        }
        sc->sc_rxlink = NULL;

    } while (0);

    if (error) {
        ath_rx_cleanup(sc);
    }

    return error;
}

/*
 * Reclaim all rx queue resources
 */
void
ath_rx_cleanup(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    wbuf_t wbuf;
    struct ath_buf *bf;
    u_int32_t   nbuf = 0;

    TAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
        wbuf = bf->bf_mpdu;
        if (wbuf) {
            nbuf++;
            wbuf_release(sc->sc_osdev, wbuf);
        }
    }

    ASSERT(sc->sc_num_rxbuf == nbuf);

    /* cleanup rx descriptors */
    if (sc->sc_rxdma.dd_desc_len != 0)
        ath_descdma_cleanup(sc, &sc->sc_rxdma, &sc->sc_rxbuf);

    ATH_RXBUF_LOCK_DESTROY(sc);
    ATH_RXFLUSH_LOCK_DESTROY(sc);
}

/*
 * Setup and link descriptors.
 *
 * 11N: we can no longer afford to self link the last descriptor.
 * MAC acknowledges BA status as long as it copies frames to host
 * buffer (or rx fifo). This can incorrectly acknowledge packets
 * to a sender if last desc is self-linked.
 *
 * NOTE: Caller should hold the rxbuf lock.
 */
static void
ath_rx_buf_link(struct ath_softc *sc, struct ath_buf *bf, bool rxenable)
{
#define DESC2PA(_sc, _va)\
    ((caddr_t)(_va) - (caddr_t)((_sc)->sc_rxdma.dd_desc) +  \
     (_sc)->sc_rxdma.dd_desc_paddr)

    struct ath_hal *ah = sc->sc_ah;
    struct ath_desc *ds;
    wbuf_t wbuf;

    ATH_RXBUF_RESET(bf);
    /* Acquire lock to have mutual exclusion with the Reset code calling ath_hal_reset */
    ATH_RESET_LOCK(sc);

    ds = bf->bf_desc;
    ds->ds_link = 0;    /* link to null */
    ds->ds_data = bf->bf_buf_addr[0];
    /* XXX For RADAR?
     * virtual addr of the beginning of the buffer. */
    wbuf = bf->bf_mpdu;
    ASSERT(wbuf != NULL);
    bf->bf_vdata = wbuf_raw_data(wbuf);

    /* setup rx descriptors */
    ath_hal_setuprxdesc(ah, ds
                        , wbuf_get_len(wbuf)   /* buffer size */
                        , 0
        );

#if ATH_RESET_SERIAL
    if (atomic_read(&sc->sc_hold_reset)) { //hold lock
        ATH_RESET_UNLOCK(sc);
        return;
    } else {
        if (atomic_read(&sc->sc_rx_hw_en) > 0) {
            atomic_inc(&sc->sc_rx_return_processing);
            ATH_RESET_UNLOCK(sc);
        } else {
            ATH_RESET_UNLOCK(sc);
            return;
        }
    }
#else

    if (atomic_read(&sc->sc_rx_hw_en) <= 0) {
        /* ath_stoprecv() has already being called. Do not queue to hardware. */
        ATH_RESET_UNLOCK(sc);
        return;
    }
#endif

    OS_SYNC_SINGLE(sc->sc_osdev,
                   bf->bf_daddr, sc->sc_txdesclen, BUS_DMA_TODEVICE,
                   OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

    if (sc->sc_rxlink == NULL)
        ath_hal_putrxbuf(ah, bf->bf_daddr, 0);
    else {
        *sc->sc_rxlink = bf->bf_daddr;

        OS_SYNC_SINGLE(sc->sc_osdev, (dma_addr_t)(DESC2PA(sc, sc->sc_rxlink)),
                       sizeof(*sc->sc_rxlink), BUS_DMA_TODEVICE, NULL);
    }

    sc->sc_rxlink = &ds->ds_link;

    if (rxenable && !atomic_read(&sc->sc_in_reset)) {
#ifdef DBG
        if (ath_hal_getrxbuf(ah, 0) == 0)
        {
            /* This will cause an NMI since RXDP is 0 */
            DPRINTF(sc, ATH_DEBUG_ANY ,
                    "%s: FATAL ERROR: NULL RXDP while enabling RX.\n",
                    __func__);
            ASSERT(FALSE);
        } else
#endif
        ath_hal_rxena(ah);
    }
#if ATH_RESET_SERIAL
    atomic_dec(&sc->sc_rx_return_processing);
#else
    ATH_RESET_UNLOCK(sc);
#endif
#undef DESC2PA
}

/*
 * Calculate the receive filter according to the
 * operating mode and state:
 *
 * o always accept unicast, broadcast, and multicast traffic
 * o maintain current state of phy error reception (the hal
 *   may enable phy error frames for noise immunity work)
 * o probe request frames are accepted only when operating in
 *   hostap, adhoc, or monitor modes
 * o enable promiscuous mode according to the interface state
 * o accept beacons:
 *   - when operating in adhoc mode so the 802.11 layer creates
 *     node table entries for peers,
 *   - when operating in station mode for collecting rssi data when
 *     the station is otherwise quiet, or
 *   - when operating as a repeater so we see repeater-sta beacons
 *   - when scanning
 */
u_int32_t
ath_calcrxfilter(struct ath_softc *sc)
{
#define    RX_FILTER_PRESERVE    (HAL_RX_FILTER_PHYERR | HAL_RX_FILTER_PHYRADAR)
    u_int32_t rfilt;
    int netif = 0;

    rfilt = (ath_hal_getrxfilter(sc->sc_ah) & RX_FILTER_PRESERVE)
        | HAL_RX_FILTER_UCAST | HAL_RX_FILTER_BCAST | HAL_RX_FILTER_MCAST;

    /*
    * If not a STA,
    * or if an AP VAP is up and running
    * or if scanning(scan is also used for listen operation during P2P discoevery )
    *    the affect of allowing probe requests during scanning is that a station VAP
    *    will receive the probe requests while scanning and it will throw them away
    *    any way.
    * then enable processing of Probe Requests.
    */
    if (sc->sc_opmode != HAL_M_STA || atomic_read(&sc->sc_nap_vaps_up) || sc->sc_scanning) {
        rfilt |= HAL_RX_FILTER_PROBEREQ;
    }

    /*
    ** Can't set HOSTAP into promiscous mode unless scanning.
    *
    *  WDS+STA mode with promiscous bit do not work well, when chip is station mode.
    *  So do not let the promiscuous setting pass through for WDS+STA. User can bypass
    *  and set that by setting allowpromisc flag'
    */
    if (((sc->sc_opmode != HAL_M_HOSTAP || (sc->sc_opmode == HAL_M_HOSTAP && sc->sc_scanning)) &&
        (sc->sc_opmode != HAL_M_STA || (!sc->sc_ieee_ops->wds_is_enabled(sc->sc_ieee))) &&
        ((netif = sc->sc_ieee_ops->get_netif_settings(sc->sc_ieee)) & ATH_NETIF_PROMISCUOUS)) ||
        (sc->sc_opmode == HAL_M_MONITOR) || (sc->sc_allow_promisc && (atomic_read(&sc->sc_nap_vaps_up) <= 4))) {
        rfilt |= HAL_RX_FILTER_PROM;
        if (sc->sc_opmode == HAL_M_MONITOR) {
            rfilt &= ~HAL_RX_FILTER_UCAST; /* ??? To prevent from sending ACK */
        }
    }
#if ATH_SUPPORT_WRAP
    if(sc->sc_enableproxysta && (ath_hal_getcapability(sc->sc_ah, HAL_CAP_WRAP_PROMISC, 0, NULL) == HAL_OK)) {
        rfilt |= HAL_RX_FILTER_PROM;
    }
#endif
    if ((sc->sc_opmode == HAL_M_STA && !(netif & ATH_NETIF_NO_BEACON)) ||
        sc->sc_opmode == HAL_M_IBSS ||
        sc->sc_nostabeacons || sc->sc_scanning)
    {
        if (sc->sc_opmode == HAL_M_IBSS || sc->sc_scanning ||
                ((atomic_read(&sc->sc_nsta_vaps_up) +
                  atomic_read(&sc->sc_nap_vaps_up)) > 1)) {
            rfilt |= HAL_RX_FILTER_BEACON;
        } else {
            if (ath_get_hwbeaconproc_active(sc)) {
                /* enable hardware beacon filtering */
                rfilt &= ~(HAL_RX_FILTER_BEACON | HAL_RX_FILTER_MYBEACON);
                rfilt |= HAL_RX_FILTER_RX_HWBCNPROC_EN;
            } else {
                rfilt &= ~HAL_RX_FILTER_BEACON;
                rfilt |= HAL_RX_FILTER_MYBEACON;
            }
        }
    }

    if (rfilt & HAL_RX_FILTER_RX_HWBCNPROC_EN) {
        ath_hwbeaconproc_enable(sc);
    } else {
        ath_hwbeaconproc_disable(sc);
    }

    /*
    ** If in HOSTAP mode, want to enable reception of PSPOLL frames & beacon frames
    */
    if (sc->sc_opmode == HAL_M_HOSTAP || atomic_read(&sc->sc_nap_vaps_up)) {
        rfilt &= ~HAL_RX_FILTER_MYBEACON;
        rfilt |= (HAL_RX_FILTER_BEACON | HAL_RX_FILTER_PSPOLL);

#if ATH_SUPPORT_WIRESHARK
            if (sc->sc_tapmonenable == 1) {
                rfilt |= HAL_RX_FILTER_PROM;
                rfilt |= HAL_RX_FILTER_BEACON;
                rfilt |= HAL_RX_FILTER_MYBEACON;
            }
#endif
    }
    /*
     * if more than one STA vap is up then we need to disable bssid
     * filtring for multicast and receive all the multicast on the channel
     * and do SW filtering in each vap .
     */

    if ((atomic_read(&sc->sc_nsta_vaps_up) > 1)
#if ATH_SUPPORT_WRAP
            || (sc->sc_enableproxysta && (ath_hal_getcapability(sc->sc_ah, HAL_CAP_WRAP_PROMISC, 0, NULL) == HAL_ENOTSUPP))
#endif
	) {
       rfilt |= HAL_RX_FILTER_ALL_MCAST_BCAST;
    }

    return rfilt;
#undef RX_FILTER_PRESERVE
}

void
ath_opmode_init(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    u_int32_t rfilt, mfilt[2];

    ATH_PS_WAKEUP(sc);

    /* configure rx filter */
    rfilt = ath_calcrxfilter(sc);
    ath_hal_setrxfilter(ah, rfilt);

    /* configure bssid mask */
    if (sc->sc_hasbmask) {
        ath_hal_setbssidmask(ah, sc->sc_bssidmask);
    }

    /* configure operational mode */
    ath_hal_setopmode(ah);

    /* Handle any link-level address change. */
    ath_hal_setmac(ah, sc->sc_myaddr);

    /* calculate and install multicast filter */
    if (sc->sc_ieee_ops->get_netif_settings(sc->sc_ieee) & ATH_NETIF_ALLMULTI) {
        mfilt[0] = mfilt[1] = ~0;
    }
    else {
        sc->sc_ieee_ops->netif_mcast_merge(sc->sc_ieee, mfilt);
    }

    ath_hal_setmcastfilter(ah, mfilt[0], mfilt[1]);
    DPRINTF(sc, ATH_DEBUG_RECV ,
            "%s: RX filter 0x%x, MC filter %08x:%08x\n",
            __func__, rfilt, mfilt[0], mfilt[1]);

    ATH_PS_SLEEP(sc);
}

void
ath_allocRxbufsForFreeList(struct ath_softc *sc)
{
    struct ath_buf *bf;
    wbuf_t new_wbuf;
    int count = 0;
    while ((bf = sc->sc_rxfreebuf) != NULL) {
        if ((new_wbuf = ATH_ALLOCATE_RXBUFFER(sc, sc->sc_rxbufsize)) != NULL) {
            // Map the ath_buf to the mbuf, load the physical mem map.
            bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, new_wbuf, BUS_DMA_FROMDEVICE,
                                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            if (bf->bf_buf_addr[0]) {
                bf->bf_mpdu = new_wbuf;
                // Set the context area to point to the ath_buf.
                ATH_SET_RX_CONTEXT_BUF(new_wbuf, bf);
                // We are always working with the free queue head. Since we allocated a buffer for it, remove it
                sc->sc_rxfreebuf = bf->bf_next;
                bf->bf_next = NULL;
                if (sc->sc_rxfreebuf == NULL) {
                    sc->sc_rxfreetail = NULL;
                }
                ath_rx_requeue(sc, new_wbuf);
                count++;
            }
            else {
                // Some weird problem ?
                DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s[%d]: Could not map mbuf into physical memory. Retrying.\n", __func__, __LINE__);
                // Dont delink bf. Just free the mbuf  and try to allocate.
                bf->bf_mpdu = NULL;
                wbuf_free(new_wbuf);
                continue;
            }
        }
        else {
            DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s[%d] --- Could not allocate Mbuf - try again later \n", __func__, __LINE__);
            // Could not alloc Rx bufs. Try again later.
            break;
        }
    }
    DPRINTF(sc, ATH_DEBUG_RX_PROC, "---- %s[%d] ---- Fed back %d buffers to HW Q ---\n", __func__, __LINE__, count);
}


/*
 * Enable the receive h/w following a reset.
 */
int
ath_startrecv(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    struct ath_buf *bf, *tbf;

    if (sc->sc_enableproxysta) {
        int ret = ath_hal_set_proxysta(ah, AH_TRUE);

        if (ret != HAL_OK) {
            DPRINTF(sc, ATH_DEBUG_ANY, "%s: ath_hal_set_proxysta failed %d\n", __func__, ret);
            return ret;
        }
    }

    /* Check for enhanced DMA support on the MAC */
    if (sc->sc_enhanceddmasupport) {
        int err = ath_edma_startrecv(sc);
        return err;
 }


    ATH_RXBUF_LOCK(sc);

    /*
     * Set this flag so that the next ath_rx_buf_link() will init. RxDP and Enable Rx */
    atomic_inc(&sc->sc_rx_hw_en);

    if (atomic_read(&sc->sc_rx_hw_en) != 1) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: Warning: Multiple Rx starts or stops;"
                " count=%d. Ignored this one!\n",
               __func__, atomic_read(&sc->sc_rx_hw_en));

        ATH_RXBUF_UNLOCK(sc);
        return 0;
    }

    sc->sc_rxlink = NULL;
    TAILQ_FOREACH_SAFE(bf, &sc->sc_rxbuf, bf_list, tbf) {

        bool  bLastBf = (TAILQ_NEXT((bf), bf_list) == NULL);

        if (bf->bf_status & ATH_BUFSTATUS_STALE) {
            bf->bf_status &= ~ATH_BUFSTATUS_STALE; /* restarting h/w, no need for holding descriptors */
            /*
             * Upper layer may not be done with the frame yet so we can't
             * just re-queue it to hardware. Remove it from h/w queue. It'll be
             * re-queued when upper layer returns the frame and ath_rx_requeue_mpdu
             * is called.
             */
#if USE_MULTIPLE_BUFFER_RCV || AP_MULTIPLE_BUFFER_RCV
            ASSERT (bf != sc->sc_bfpending || !(bf->bf_status & ATH_BUFSTATUS_FREE));
#endif
            if (!(bf->bf_status & ATH_BUFSTATUS_FREE)) {
                TAILQ_REMOVE(&sc->sc_rxbuf, bf, bf_list);
                continue;
            }
        }
        /* chain descriptors */
        ath_rx_buf_link(sc, bf, bLastBf);
    }
    // Check the free athbuf list. If there are athbufs here, try to alloc some mbufs for them.
    if (sc->sc_rxfreebuf) {
        ath_allocRxbufsForFreeList(sc);
    }

#if USE_MULTIPLE_BUFFER_RCV || AP_MULTIPLE_BUFFER_RCV
    if (sc->sc_bfpending) {
        /*
         * The sc_bufpending is not on the rxbuf list.
         * if it was on the list,  the above loop would have found it
         * and removed it from the list.
         */
#if AP_MULTIPLE_BUFFER_RCV
        wbuf_t	pointbuf,tempbuf;
        pointbuf = tempbuf = sc->sc_rxpending;
        while (pointbuf!=NULL)
        {
            pointbuf = wbuf_next(tempbuf);
            wbuf_set_next(tempbuf,NULL);
            wbuf_free(tempbuf);
            tempbuf = pointbuf;
        }
        sc->sc_bfpending->bf_mpdu = NULL;
        sc->sc_prebuflen=0;
        ath_rx_buf_relink(sc,sc->sc_bfpending);
#else
        TAILQ_INSERT_TAIL(&sc->sc_rxbuf, sc->sc_bfpending, bf_list);
        ath_rx_buf_link(sc, sc->sc_bfpending, true);
#endif
        sc->sc_rxpending = NULL;
        sc->sc_bfpending = NULL;
    }
#endif /* USE_MULTIPLE_BUFFER_RCV */
    bf = TAILQ_FIRST(&sc->sc_rxbuf);
    if (bf != NULL) {
        /* SYNC may not be needed here, we just SYNCed above in ath_rx_buf_link */
        OS_SYNC_SINGLE(sc->sc_osdev,
                       bf->bf_daddr, sc->sc_txdesclen, BUS_DMA_TODEVICE,
                       OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

        /* Acquire lock to have mutual exclusion with the Reset code calling ath_hal_reset */
        ATH_RESET_LOCK(sc);
        ath_hal_putrxbuf(ah, bf->bf_daddr, 0);
        ath_hal_rxena(ah);      /* enable recv descriptors */
        ATH_RESET_UNLOCK(sc);
    }
    ATH_RXBUF_UNLOCK(sc);

    ath_opmode_init(sc);        /* set filters, etc. */
    ath_hal_startpcurecv(ah, sc->sc_scanning);    /* re-enable PCU/DMA engine */
    return 0;
}

/*
 * Disable the receive h/w in preparation for a reset.
 */
HAL_BOOL
ath_stoprecv(struct ath_softc *sc, int timeout)
{
#define    PA2DESC(_sc, _pa)                                               \
    ((struct ath_desc *)((caddr_t)(_sc)->sc_rxdma.dd_desc +             \
                         ((_pa) - (_sc)->sc_rxdma.dd_desc_paddr)))
    struct ath_hal *ah = sc->sc_ah;
#ifdef AR_DEBUG
    u_int64_t tsf;
#endif
    HAL_BOOL stopped;

    /* Check for enhanced DMA support on the MAC */
    if (sc->sc_enhanceddmasupport) {
        return ath_edma_stoprecv(sc, timeout);
    }

    if (sc->sc_removed) {
        return AH_TRUE;
    }

    ATH_RXBUF_LOCK(sc);

    /* Set the Rx Stop flag so that we do not enable Rx hardware again. */
    atomic_dec(&sc->sc_rx_hw_en);

    if (atomic_read(&sc->sc_rx_hw_en) != 0) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: Warning: Multiple Rx starts or"
                " stops; count=%d. Ignored this one!\n",
               __func__, atomic_read(&sc->sc_rx_hw_en));

        ATH_RXBUF_UNLOCK(sc);
        return AH_TRUE;     /* Assumed that the Rx DMA has stopped */
    }

    ATH_RXBUF_UNLOCK(sc);

    ath_hal_stoppcurecv(ah);    /* disable PCU */
    ath_hal_setrxfilter(ah, 0);    /* clear recv filter */
    stopped = ath_hal_stopdmarecv(ah, timeout); /* disable DMA engine */
    if (sc->sc_opmode == HAL_M_HOSTAP) {
        /* Adding this delay back, only for chip in AP mode
         * since not having this delay causing pci data bus error
         * in WDS mode - See bug 37962
         */
        OS_DELAY(3000); /* 3ms is long enough for 1 frame */
    }
#ifdef AR_DEBUG
    tsf = ath_hal_gettsf64(ah);
    if (CHK_SC_DEBUG(sc, ATH_DEBUG_RESET) ||
        CHK_SC_DEBUG(sc, ATH_DEBUG_FATAL)) {
        struct ath_buf *bf;

        DPRINTF(sc, ATH_DEBUG_RESET, "ath_stoprecv: rx queue %08x, link %pK\n",
               ath_hal_getrxbuf(ah, 0), sc->sc_rxlink);
        TAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list) {
            struct ath_desc *ds = bf->bf_desc;

            OS_SYNC_SINGLE(sc->sc_osdev,
                           bf->bf_daddr, sc->sc_txdesclen, BUS_DMA_FROMDEVICE,
                           OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

            HAL_STATUS status = ath_hal_rxprocdesc(ah, ds,
									bf->bf_daddr, PA2DESC(sc, ds->ds_link), tsf, &(bf->bf_rx_status));
            if (status == HAL_OK || CHK_SC_DEBUG(sc, ATH_DEBUG_FATAL))
                ath_printrxbuf(bf, status == HAL_OK);
        }
    }
#endif

    return stopped;
#undef PA2DESC
}

/*
 * Flush receive queue
 */
void
ath_flushrecv(struct ath_softc *sc)
{
    /*
     * ath_rx_tasklet may be used to hande rx interrupt and flush receive
     * queue at the same time. Use a lock to serialize the access of rx
     * queue.
     * ath_rx_tasklet cannot hold the spinlock while indicating packets.
     * Instead, do not claim the spinlock but check for a flush in
     * progress (see references to sc_rxflush)
     */

    /*
     * Since we cannot hold a lock, use cmpxchg and allow only one thread to flush.
     */
    if (cmpxchg(&sc->sc_rxflush, 0, 1) == 0) {
        ATH_RX_TASKLET(sc, RX_DROP);
        if (cmpxchg(&sc->sc_rxflush, 1, 0) != 1)
            DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s: flush is not protected.\n", __func__);
    }
}

void
ath_rx_requeue(ath_dev_t dev, wbuf_t wbuf)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_buf *bf = ATH_GET_RX_CONTEXT_BUF(wbuf);

    ASSERT(bf != NULL);

    /* Check for enhanced DMA support on the MAC */
    if (sc->sc_enhanceddmasupport) {
        ath_rx_edma_requeue(dev, wbuf);
        return;
    }

    ATH_RXBUF_LOCK(sc);
    if (bf->bf_status & ATH_BUFSTATUS_STALE) {
        /*
         * This buffer is still held for hw acess.
         * Mark it as free to be re-queued it later.
         */
        bf->bf_status |= ATH_BUFSTATUS_FREE;
    } else {
        TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf, bf_list);
        ath_rx_buf_link(sc, bf, true);
    }
    ATH_RXBUF_UNLOCK(sc);
}

#ifndef ATHHTC_AP_REMOVE_STATS

static void
_ath_node_rx_stats_update(struct ath_softc *sc , wbuf_t wbuf , struct ath_node  * an,ieee80211_rx_status_t *rx_status)
{
    struct ieee80211_frame *wh;
    int i;
    /*
     * update node statistics
     */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);

    if (IEEE80211_IS_DATA(wh)) {
        ATH_RATE_LPF(an->an_avgrxrate, rx_status->rateKbps);
        an->an_lastrxrate = rx_status->rateKbps;
        an->an_rxratecode = rx_status->ratecode;
    }

    /* For STA, update RSSI info from associated BSSID only. Don't update RSSI, if we
       recv pkt from another BSSID(probe resp etc.)
       */
    if (rx_status->flags & ATH_RX_RSSI_VALID &&
            ((sc->sc_opmode != HAL_M_STA) ||
             ATH_ADDR_EQ(wh->i_addr2, sc->sc_curbssid))) {

        ATH_RSSI_LPF(an->an_avgrssi, rx_status->rssi);
        if (IEEE80211_IS_BEACON(wh))
            ATH_RSSI_LPF(an->an_avgbrssi, rx_status->rssi);
        else
            ATH_RSSI_LPF(an->an_avgdrssi, rx_status->rssi);

        if (rx_status->flags & ATH_RX_CHAIN_RSSI_VALID) {
            for(i = 0; i < ATH_HOST_MAX_ANTENNA; ++i) {
                ATH_RSSI_LPF(an->an_avgchainrssi[i], rx_status->rssictl[i]);
                if (IEEE80211_IS_BEACON(wh))
                    ATH_RSSI_LPF(an->an_avgbchainrssi[i], rx_status->rssictl[i]);
                else
                    ATH_RSSI_LPF(an->an_avgdchainrssi[i], rx_status->rssictl[i]);
            }
            if (rx_status->flags & ATH_RX_RSSI_EXTN_VALID) {
                for(i = 0; i < ATH_HOST_MAX_ANTENNA; ++i) {
                    ATH_RSSI_LPF(an->an_avgchainrssiext[i], rx_status->rssiextn[i]);
                    if (IEEE80211_IS_BEACON(wh))
                        ATH_RSSI_LPF(an->an_avgbchainrssiext[i], rx_status->rssiextn[i]);
                    else
                        ATH_RSSI_LPF(an->an_avgdchainrssiext[i], rx_status->rssiextn[i]);
                }
            }
        }

    }


}

#define ATH_NODE_RX_STATS_UPDATE(_sc, _wbuf, _an, _rx_status) _ath_node_rx_stats_update(_sc, _wbuf, _an, _rx_status)
#else
#define ATH_NODE_RX_STATS_UPDATE(_sc, _wbuf, _an, _rx_status) \
            if(_rx_status->rssi != -1 )          \
                    ATH_RSSI_LPF(_an->an_avgrssi, _rx_status->rssi);
#endif





/*
 * Process an individual frame
 */
int
ath_rx_input(ath_dev_t dev, ath_node_t node, int is_ampdu,
             wbuf_t wbuf, ieee80211_rx_status_t *rx_status,
             ATH_RX_TYPE *status)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_node *an = ATH_NODE(node);
    if (an == NULL) {
        *status = ATH_RX_NON_CONSUMED;
        return -1;
    }
    ATH_NODE_RX_STATS_UPDATE(sc, wbuf, an, rx_status);

#if ATH_SUPPORT_KEYPLUMB_WAR
    if (rx_status->flags & ATH_RX_DECRYPT_ERROR) {
        an->an_decrypt_err++;
        DPRINTF(sc, ATH_DEBUG_RECV, "%s decrypt error. STA (%s)  keyix (%d) rx_status flags (%d)\n",
                __FUNCTION__, ether_sprintf(an->an_macaddr), an->an_keyix, rx_status->flags);

        if (an->an_decrypt_err >= MAX_DECRYPT_ERR_KEYPLUMB) {
            DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: Check hw key for STA (%s). Key might be corrupted\n",
                    __FUNCTION__, ether_sprintf(an->an_macaddr));
            if(!ath_checkandplumb_key(dev, node, an->an_keyix)) {
                DPRINTF(sc, ATH_DEBUG_KEYCACHE, "%s: Key plumbed again for STA (%s). Key is corrupted\n",
                        __FUNCTION__, ether_sprintf(an->an_macaddr));
            }
        }
    }
    else
    {
        an->an_decrypt_err = 0;
    }
#endif

    if (is_ampdu && sc->sc_rxaggr) {
        *status = ATH_RX_CONSUMED;
        return ath_ampdu_input(sc, an, wbuf, rx_status);
    } else {
        *status = ATH_RX_NON_CONSUMED;
        return -1;
    }
}

#ifdef ATH_SUPPORT_TxBF
#define ACTION_CAT_HT             7	  /* HT per IEEE802.11n-D1.06 */
#define ACTION_HT_NONCOMP_BF      5   /* Non-compressed Beamforming action*/
#define ACTION_HT_COMP_BF         6   /* Compressed Beamforming action*/
void
ath_rx_bf_process(struct ath_softc *sc, struct ath_buf *bf, struct ath_rx_status *rxs, ieee80211_rx_status_t *rx_status)
{
    wbuf_t wbuf = bf->bf_mpdu;
    enum {
        Rx_ACK = 0x0A,			/* To be ACK */
        Rx_HTC_QoS_NULL = 0x0D,	/* QoS NULL with HTC */
        Rx_NONCOMP_BF = 0xC0,	/* CSI_NONCOMP_BF */
        Rx_COMP_BF = 0xC1,	    /* CSI_COMP_BF */
        Rx_Req_Inaggr = 0xC2	/* Sounding Request within aggr */
    };

    /*not bf mode*/
    if (sc->sc_txbfsupport == AH_FALSE) {
        return;
    }

    if ((sc->last_rx_type != 0) && (rxs->rx_hw_upload_data == 0)) {
        /* Due to be invalid, the H or V/CV was discarded */
        sc->last_rx_type = 0;
    }

    if (rxs->rx_hw_upload_data) {
        if ((sc->last_rx_type == Rx_NONCOMP_BF) || (sc->last_rx_type == Rx_COMP_BF) ||
            (sc->last_rx_type == Rx_Req_Inaggr)) {
            /* Handle the V/CV to transmit the action frame
               For WinXp, client mode only does not need to store the node information*/
            if ((sc->do_node_check == 0) || (sc->v_cv_node != NULL)) {
                u_int8_t	*v_cv_data = (u_int8_t *) wbuf_header(wbuf);
                u_int16_t	buf_len;
                u_int8_t	Do_send = 1;

                buf_len = wbuf_get_pktlen(wbuf);

                DPRINTF(sc, ATH_DEBUG_ANY, "Get the V/CV packet (%x) prepare to send the action frame\n",
                    sc->last_rx_type );

                if ((sc->last_rx_type == Rx_Req_Inaggr) && (*v_cv_data == ACTION_CAT_HT)) {
                    if ((*(v_cv_data+1) != ACTION_HT_COMP_BF) &&
                        (*(v_cv_data+1) != ACTION_HT_NONCOMP_BF)) {
	                    Do_send = 0;
	                    DPRINTF(sc, ATH_DEBUG_ANY, "Not to be the desired V/CV Data 0x%02x \n",
                            *(v_cv_data+1));
                    }
                }

                if (Do_send) {
                    DPRINTF(sc, ATH_DEBUG_ANY, "%s rxs->rs_datalen(%d) buf_len(%d)\n",
                        __FUNCTION__, rxs->rs_datalen, buf_len);
                    rx_status->bf_flags = ATH_BFRX_DATA_UPLOAD_DELAY;
                } else {
                    sc->v_cv_node = NULL;
                }
            } else {
                DPRINTF(sc, ATH_DEBUG_ANY, "==>%s:sc->V_CV_node to be NULL do nothing \n",
                    __func__);
            }
        } else {
            DPRINTF(sc, ATH_DEBUG_ANY, "==>%s:To be H but the last pkt not as expected sc->Last_Rx_type(%x) is_agg(%x)\n",
                __func__, sc->last_rx_type, rxs->rs_isaggr);
        }
        /*After copy the frame content release that.*/
        sc->last_rx_type=0;
    }
    do {
        if (rxs->rs_more) {
            struct ieee80211_frame_min_one *wh;
            struct ieee80211_qosframe_htc *wh1;
            u_int8_t *htc;

            wh = (struct ieee80211_frame_min_one *)wbuf_header(wbuf);
            wh1 = (struct ieee80211_qosframe_htc *)wbuf_header(wbuf);

            /* check for four address or three address frame */
            if((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
                htc = ((struct ieee80211_qosframe_htc_addr4 *)wh1)->i_htc;
            else
                htc = wh1->i_htc;

            if ((sc->last_be_aggr) ||((htc[2] == IEEE80211_HTC2_CSI_COMP_BF) ||
                (htc[2] == IEEE80211_HTC2_CSI_NONCOMP_BF))) {
                /* For Delay Report */
                DPRINTF(sc, ATH_DEBUG_ANY,"%s Delay Report rxs->rs_datalen(%d)\n",
                     __FUNCTION__, rxs->rs_datalen);
                sc->v_cv_node = NULL;
                if (sc->last_be_aggr) {
                    /* During aggr */
                    sc->last_rx_type = Rx_Req_Inaggr;
                } else {
                    if (htc[2]== IEEE80211_HTC2_CSI_COMP_BF) {
                        /* CV */
                        sc->last_rx_type = Rx_COMP_BF;
                    } else {
                        /* V */
                        sc->last_rx_type = Rx_NONCOMP_BF;
                    }
                }
                rx_status->bf_flags = ATH_BFRX_PKT_MORE_DELAY;

                {
                    u_int8_t used_rate = rxs->rs_rate;

#define MAP_INTO_ONESTREAM(_rate) (_rate & 0x87)

                    used_rate = MAP_INTO_ONESTREAM(used_rate);
                    sc->sounding_rx_kbps = sc->sc_hwmap[used_rate].rateKbps;
                }
                break;/* continue process such packet */
            } else {
                /*others*/
                DPRINTF(sc, ATH_DEBUG_ANY,"%s to be rs_more packet (%d) wh1->i_htc[2] (%02x) aggr (%x)\n",
                    __FUNCTION__ ,rxs->rs_datalen ,htc[2],rxs->rs_isaggr);
                sc->last_rx_type = 0x0;
            }
        }
    } while (0);

    /* For handle the upload V/CV during Aggr */
    if (rxs->rs_isaggr && rxs->rs_moreaggr) {
        sc->last_be_aggr = 1;
    } else {
        sc->last_be_aggr = 0;
    }
}
#endif

void
ath_rx_process(struct ath_softc *sc, struct ath_buf *bf, struct ath_rx_status *rxs, u_int8_t frame_fc0,
        ieee80211_rx_status_t *rx_status, u_int8_t *chainreset)
{
    u_int16_t buf_len;
    u_int8_t rxchainmask;
	wbuf_t wbuf = bf->bf_mpdu;
    int type;
    int selevm;
#if UNIFIED_SMARTANTENNA
    static uint32_t total_pkts = 0;
#endif
#if ATH_VOW_EXT_STATS
    struct ath_phy_stats *phy_stats = &sc->sc_phy_stats[sc->sc_curmode];
#endif
#if defined(ATH_ADDITIONAL_STATS) || ATH_SLOW_ANT_DIV || defined(ATH_SUPPORT_TxBF)
    struct ieee80211_frame *wh = (struct ieee80211_frame *) wbuf_header(wbuf) ;
#endif
#ifdef ATH_SUPPORT_TxBF
    ath_rx_bf_process(sc, bf, rxs, rx_status);

    if (ath_txbf_chk_rpt_frm(wh)){
        /* get time stamp for txbf report frame only*/
        rx_status->rpttstamp = rxs->rs_tstamp;
    } else {
        rx_status->rpttstamp = 0;
    }
#endif

    /*
     * Sync and unmap the frame.  At this point we're
     * committed to passing the sk_buff somewhere so
     * clear buf_skb; this means a new sk_buff must be
     * allocated when the rx descriptor is setup again
     * to receive another frame.
     */
    buf_len = wbuf_get_pktlen(wbuf);
    /*
     * Read two TSF registers for each rreceived packet would waste too many CPU cycles,
     * and the rx_status->tsf will only be used for three purposes: CCX, IBSS merge, and MONITOR
     * mode. For CCX and IBBS merge, the tsf will be updated seperately in OS shim layer
     * (for Vista) and in umac layer for beacon frames only; So here in the DPC of Rx path,
     * only read the registers if working in monitor mode.
     */
    if (sc->sc_opmode != HAL_M_HOSTAP) {
        rx_status->tsf = ath_extend_tsf(sc, rxs->rs_tstamp);
    } else {
        /* For HOSTAP mode, the 64-bit tsf is not needed, so set it to 0 to make as -invalid- */
        rx_status->tsf = 0;
    }
    rx_status->rateieee = sc->sc_hwmap[rxs->rs_rate].ieeerate;
    rx_status->rateKbps = sc->sc_hwmap[rxs->rs_rate].rateKbps;
    rx_status->ratecode = rxs->rs_rate;
    rx_status->nomoreaggr = rxs->rs_moreaggr ? 0:1;

    rx_status->isapsd = rxs->rs_isapsd;
    rx_status->noisefloor = (sc->sc_noise_floor == 0) ?
                   ATH_DEFAULT_NOISE_FLOOR : sc->sc_noise_floor;
    /*
     * During channel switch, frms already in requeue should be marked as the
     * previous channel rather than sc_curchan.channel
     */
    rx_status->channel = rxs->rs_channel;

    selevm = ath_hal_setrxselevm(sc->sc_ah, 0, 1);
    if(!selevm) {
            rx_status->lsig[0] = (rxs->evm0 >> 24) & 0xFF;
            rx_status->lsig[1] = (rxs->evm0 >> 16) & 0xFF;
            rx_status->lsig[2] = (rxs->evm0 >> 8) & 0xFF;
            rx_status->servicebytes[0] = (rxs->evm0) & 0xFF;
            rx_status->servicebytes[1] = (rxs->evm1 >> 24) & 0xFF;
            rx_status->htsig[0] = (rxs->evm1 >> 16) & 0xFF;
            rx_status->htsig[1] = (rxs->evm1 >> 8) & 0xFF;
            rx_status->htsig[2] = (rxs->evm1)  & 0xFF;
            rx_status->htsig[3] = (rxs->evm2 >> 24) & 0xFF;
            rx_status->htsig[4] = (rxs->evm2 >> 16) & 0xFF;
            rx_status->htsig[5] = (rxs->evm2 >> 8) & 0xFF;
    }

    /* HT rate */
    if (rx_status->ratecode & 0x80) {
        /* TODO - add table to avoid division */
        /* For each case, do division only one time */
        if (rxs->rs_flags & HAL_RX_2040) {
            rx_status->flags |= ATH_RX_40MHZ;
            if (rxs->rs_flags & HAL_RX_GI) {
                rx_status->rateKbps =
                    ATH_MULT_30_DIV_13(rx_status->rateKbps);
            } else {
                rx_status->rateKbps =
                    ATH_MULT_27_DIV_13(rx_status->rateKbps);
                rx_status->flags |= ATH_RX_SHORT_GI;
            }
        } else {
            if (rxs->rs_flags & HAL_RX_GI) {
                rx_status->rateKbps =
                    ATH_MULT_10_DIV_9(rx_status->rateKbps);
            } else {
                rx_status->flags |= ATH_RX_SHORT_GI;
            }
        }
    }

    /* sc->sc_noise_floor is only available when the station attaches to an AP,
     * so we use a default value if we are not yet attached.
     */
    /* XXX we should use either sc->sc_noise_floor or
     * ath_hal_getChanNoise(ah, &sc->sc_curchan)
     * to calculate the noise floor.
     * However, the value returned by ath_hal_getChanNoise seems to be incorrect
     * (-31dBm on the last test), so we will use a hard-coded value until we
     * figure out what is going on.
     */
    if (rxs->rs_rssi != ATH_RSSI_BAD) {
        rx_status->abs_rssi = rxs->rs_rssi + ATH_DEFAULT_NOISE_FLOOR;
    }

    if (!(bf->bf_status & ATH_BUFSTATUS_SYNCED)) {
        OS_SYNC_SINGLE(sc->sc_osdev,
                   bf->bf_buf_addr[0], wbuf_get_pktlen(wbuf), BUS_DMA_FROMDEVICE,
                   OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        bf->bf_status |= ATH_BUFSTATUS_SYNCED;
    }

    if (wbuf_next_buf(wbuf) == NULL)
    {
        OS_UNMAP_SINGLE(sc->sc_osdev, bf->bf_buf_addr[0],
                        sc->sc_rxbufsize, BUS_DMA_FROMDEVICE,
                        OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
    }
    else
    {
        struct ath_buf *bftmp;
        wbuf_t wbx = wbuf;
        while (wbx) {
            bftmp  = ATH_RX_CONTEXT(wbx)->ctx_rxbuf;
            OS_UNMAP_SINGLE(sc->sc_osdev, bftmp->bf_buf_addr[0],
                            sc->sc_rxbufsize, BUS_DMA_FROMDEVICE,
                            OS_GET_DMA_MEM_CONTEXT(bftmp, bf_dmacontext));
            wbx = wbuf_next_buf(wbx);
        }
    }


    /*
     * ast_ant_rx can only accommodate 8 antennas
     */
    sc->sc_stats.ast_ant_rx[rxs->rs_antenna & 0x7]++;

#if WLAN_SUPPORT_GREEN_AP
    /* This is the debug feature to print out the RSSI. This is the only
     * way to check if the Rx chains are disabled and enabled correctly.
     */
    {
        static u_int32_t print_now = 0;
        if (ath_green_ap_get_enable_print(sc)) {
            if (!(print_now & 0xff)) {
                  qdf_print("Rx rssi0 %d rssi1 %d rssi2 %d",
                          (int8_t)rxs->rs_rssi_ctl0,
                          (int8_t)rxs->rs_rssi_ctl1,
                          (int8_t)rxs->rs_rssi_ctl2);
            }
        }
        print_now++;
    }
#endif /* WLAN_SUPPORT_GREEN_AP */

    if (sc->sc_hashtsupport) {
#if UNIFIED_SMARTANTENNA
        if (SMART_ANT_RX_FEEDBACK_ENABLED(sc) && ((frame_fc0 & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA)) {
            total_pkts++;
        }
#endif
        rx_status->isaggr = rxs->rs_isaggr;
        if (rxs->rs_moreaggr == 0) {
            int rssi_chain_valid_count = 0;
            int numchains = sc->sc_rx_numchains;

            rx_status->numchains = numchains;

            if (rxs->rs_rssi != ATH_RSSI_BAD) {
                rx_status->rssi = rxs->rs_rssi;
                rx_status->flags |= ATH_RX_RSSI_VALID;
                sc->sc_stats.ast_rx_rssi = rxs->rs_rssi;
            }
            if (rxs->rs_rssi_ctl0 != ATH_RSSI_BAD) {
                rx_status->rssictl[0]  = rxs->rs_rssi_ctl0;
                rssi_chain_valid_count++;
                sc->sc_stats.ast_rx_rssi_ctl0 = rxs->rs_rssi_ctl0;
            }

            if (rxs->rs_rssi_ctl1 != ATH_RSSI_BAD) {
                rx_status->rssictl[1]  = rxs->rs_rssi_ctl1;
                rssi_chain_valid_count++;
                sc->sc_stats.ast_rx_rssi_ctl1 = rxs->rs_rssi_ctl1;
            }

            if (rxs->rs_rssi_ctl2 != ATH_RSSI_BAD) {
                rx_status->rssictl[2]  = rxs->rs_rssi_ctl2;
                rssi_chain_valid_count++;
                sc->sc_stats.ast_rx_rssi_ctl2 = rxs->rs_rssi_ctl2;
            }

            if (rxs->rs_flags & HAL_RX_2040) {
                int rssi_extn_valid_count = 0;
                if (rxs->rs_rssi_ext0 != ATH_RSSI_BAD) {
                    rx_status->rssiextn[0]  = rxs->rs_rssi_ext0;
                    rssi_extn_valid_count++;
                    sc->sc_stats.ast_rx_rssi_ext0 = rxs->rs_rssi_ext0;
                }
                if (rxs->rs_rssi_ext1 != ATH_RSSI_BAD) {
                    rx_status->rssiextn[1]  = rxs->rs_rssi_ext1;
                    rssi_extn_valid_count++;
                    sc->sc_stats.ast_rx_rssi_ext1 = rxs->rs_rssi_ext1;
                }
                if (rxs->rs_rssi_ext2 != ATH_RSSI_BAD) {
                    rx_status->rssiextn[2]  = rxs->rs_rssi_ext2;
                    rssi_extn_valid_count++;
                    sc->sc_stats.ast_rx_rssi_ext2 = rxs->rs_rssi_ext2;
                }
                if (rssi_extn_valid_count == numchains) {
                    rx_status->flags |= ATH_RX_RSSI_EXTN_VALID;
                }
            }
            if (rssi_chain_valid_count == numchains) {
                rx_status->flags |= ATH_RX_CHAIN_RSSI_VALID;
            }
#if UNIFIED_SMARTANTENNA
            if (SMART_ANT_RX_FEEDBACK_ENABLED(sc) && ((frame_fc0&IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA)) {
                ath_smart_ant_rxfeedback(sc, wbuf, rxs, total_pkts);
                total_pkts = 0;
            }
#endif
        }
    } else {
        /*
         * Need to insert the "combined" rssi into the status structure
         * for upper layer processing
         */

         rx_status->rssi = rxs->rs_rssi;
         rx_status->flags |= ATH_RX_RSSI_VALID;
         rx_status->isaggr = 0;
#if UNIFIED_SMARTANTENNA
         if ((frame_fc0&IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA) {
             ath_smart_ant_rxfeedback(sc, wbuf, rxs, 1);
         }
#endif
    }

    if (rxs->rs_flags & HAL_RX_HI_RX_CHAIN)
        rx_status->flags |= ATH_RX_SM_ENABLE;

#ifdef ATH_ADDITIONAL_STATS
    do {
		u_int8_t frm_type;
        u_int8_t frm_subtype;
        frm_type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
        frm_subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
        if (frm_type == IEEE80211_FC0_TYPE_DATA ) {
            if (frm_subtype == IEEE80211_FC0_SUBTYPE_QOS) {
                struct ieee80211_qosframe *whqos;
                int tid;
                whqos = (struct ieee80211_qosframe *) wh;
				tid = whqos->i_qos[0] & IEEE80211_QOS_TID;
				sc->sc_stats.ast_rx_num_qos_data[tid]++;
            } else {
                sc->sc_stats.ast_rx_num_nonqos_data++;
            }
        }
    } while(0);
#endif

#ifndef __ubicom32__
    /*
     * XXX rx antennas should keep the same for all received packets in
     * one aggregation. Remove this part of code after further confirmation.
     */
    if (sc->sc_diversity) {
        /*
         * When using hardware fast diversity, change the default rx
         * antenna if rx diversity chooses the other antenna 3
         * times in a row.
         */

        /*
         * TODO: vicks discusses with team regarding this
         * beacuse of rx diversity def antenna is changing ..
         */

        if (sc->sc_defant != rxs->rs_antenna) {
            if (++sc->sc_rxotherant >= 3) {
#if UNIFIED_SMARTANTENNA
                if (!SMART_ANT_ENABLED(sc)) {
                    ath_setdefantenna(sc, rxs->rs_antenna);
                }
#else
                    ath_setdefantenna(sc, rxs->rs_antenna);
#endif
            }
        }
        else {
            sc->sc_rxotherant = 0;
        }
    }
#endif

    /*
     * Increment rx_pkts count.
     */
    __11nstats(sc, rx_pkts);
    sc->sc_stats.ast_rx_packets++;
    sc->sc_stats.ast_rx_bytes += wbuf_get_pktlen(wbuf);

    /*
     * WAR 25033: redo antenna detection for Lenovo devices
     */
    if (sc->sc_rx_chainmask_detect && sc->sc_rx_chainmask_start) {
        rxchainmask = ath_rx_detect_antenna(sc, rxs);
        if (rxchainmask) {
            sc->sc_rx_chainmask_detect = 0;
            sc->sc_rx_chainmask_start  = 0;
            if (sc->sc_rx_chainmask != rxchainmask) {
                sc->sc_rx_chainmask = rxchainmask;

                /* we have to do an reset to change chain mask */
                *chainreset = 1;
                sc->sc_rx_numchains = sc->sc_mask2chains[sc->sc_rx_chainmask];
            }
        }
    }

#if ATH_SLOW_ANT_DIV
    if (sc->sc_slowAntDiv && (rx_status->flags & ATH_RX_RSSI_VALID) && IEEE80211_IS_BEACON(wh)) {
        ath_slow_ant_div(&sc->sc_antdiv, wh, rxs);
    }
#endif

#if ATH_ANT_DIV_COMB
        if (sc->sc_smart_antenna)
            ath_smartant_rx_scan(sc->sc_sascan, wh, rxs, rx_status->rateKbps);
        else if (sc->sc_antDivComb)
            ath_ant_div_comb_scan(&sc->sc_antcomb, rxs);
#endif

#if ATH_VOW_EXT_STATS
    /* make sure we do not corrupt non-decrypted frame */
    if (sc->sc_vowext_stats && !(rx_status->flags & ATH_RX_KEYMISS))
        ath_add_ext_stats(rxs, wbuf, sc, phy_stats, rx_status);
#endif //

    /*
     * Pass frames up to the stack.
     * Note: After calling ath_rx_indicate(), we should not assumed that the
     * contents of wbuf and wh are valid.
     */
    type = ath_rx_indicate(sc, wbuf, rx_status, rxs->rs_keyix);

    if (type == IEEE80211_FC0_TYPE_MGT) {
#ifdef QCA_SUPPORT_CP_STATS
        pdev_cp_stats_rx_num_mgmt_inc(sc->sc_pdev, 1);
#else
        sc->sc_stats.ast_rx_num_mgmt++;
#endif
    }
    else if (type == IEEE80211_FC0_TYPE_DATA) {
        sc->sc_stats.ast_rx_num_data++;
    }
    else if (type == IEEE80211_FC0_TYPE_CTL) {
#ifdef QCA_SUPPORT_CP_STATS
        pdev_cp_stats_rx_num_ctl_inc(sc->sc_pdev, 1);
#else
        sc->sc_stats.ast_rx_num_ctl++;
#endif
    }
    else {
#ifdef QCA_SUPPORT_CP_STATS
        pdev_cp_stats_rx_num_ctl_inc(sc->sc_pdev, 1);
#else
        sc->sc_stats.ast_rx_num_unknown++;
#endif
    }

    /* report data flow to LED module */
#if ATH_SUPPORT_LED || defined(ATH_BT_COEX)
    if (type == IEEE80211_FC0_TYPE_DATA) {
        int subtype = frame_fc0 & IEEE80211_FC0_SUBTYPE_MASK;
        if (subtype != IEEE80211_FC0_SUBTYPE_NODATA &&
            subtype != IEEE80211_FC0_SUBTYPE_QOS_NULL)
        {
#if ATH_SUPPORT_LED
            ath_led_report_data_flow(&sc->sc_led_control, buf_len);
#endif
#ifdef ATH_BT_COEX
            sc->sc_btinfo.wlanRxPktNum++;
#endif
        }
    }
#endif
}

/*
 * Process receive queue, as well as LED, etc.
 * Arg "flush":
 * 0: Process rx frames in rx interrupt.
 * 1: Drop rx frames in flush routine.
 * 2: Flush and indicate rx frames, must be synchronized with other flush threads.
 */
int
ath_rx_tasklet(ath_dev_t dev, int flush)
{
#define PA2DESC(_sc, _pa)                                               \
    ((struct ath_desc *)((caddr_t)(_sc)->sc_rxdma.dd_desc +             \
                         ((_pa) - (_sc)->sc_rxdma.dd_desc_paddr)))

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_buf *bf, *bf_held = NULL;
    struct ath_desc *ds;
    struct ath_rx_status *rxs;
    HAL_STATUS retval;
    u_int phyerr;
    struct ieee80211_frame *wh;
    wbuf_t wbuf = NULL;
    ieee80211_rx_status_t rx_status;
    struct ath_phy_stats *phy_stats = &sc->sc_phy_stats[sc->sc_curmode];
    u_int8_t chainreset = 0;
    int rx_processed = 0;
#if ATH_SUPPORT_RX_PROC_QUOTA
    u_int process_frame_cnt = sc->sc_process_rx_num;
#endif

    UNREFERENCED_PARAMETER(ah);
    UNREFERENCED_PARAMETER(retval);
    UNREFERENCED_PARAMETER(phy_stats);

    DPRINTF(sc, ATH_DEBUG_RX_PROC, "%s\n", __func__);
    do {
        u_int8_t frame_fc0;

        /* If handling rx interrupt and flush is in progress => exit */
        if (sc->sc_rxflush && (flush == RX_PROCESS)) {
            break;
        }

        ATH_RXBUF_LOCK(sc);
        bf = TAILQ_FIRST(&sc->sc_rxbuf);
        if (bf == NULL) {
            sc->sc_rxlink = NULL;
            ATH_RXBUF_UNLOCK(sc);
            break;
        }
        bf_held = NULL;

        /*
         * There is a race condition that DPC gets scheduled after sw writes RxE
         * and before hw re-load the last descriptor to get the newly chained one.
         * Software must keep the last DONE descriptor as a holding descriptor -
         * software does so by marking it with the STALE flag.
         */
        if (bf->bf_status & ATH_BUFSTATUS_STALE) {
            bf_held = bf;
            bf = TAILQ_NEXT(bf_held, bf_list);
            if (bf == NULL) {
                /*
                 * The holding descriptor is the last descriptor in queue.
                 * It's safe to remove the last holding descriptor in DPC context.
                 */
                TAILQ_REMOVE(&sc->sc_rxbuf, bf_held, bf_list);
                bf_held->bf_status &= ~ATH_BUFSTATUS_STALE;
                sc->sc_rxlink = NULL;

                if (bf_held->bf_status & ATH_BUFSTATUS_FREE) {
                    TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf_held, bf_list);
                    ath_rx_buf_link(sc, bf_held, true); /* try to requeue this descriptor */
                }
                ATH_RXBUF_UNLOCK(sc);
                break;
            }
        }
#if (ATH_SUPPORT_DESCFAST && ATH_SUPPORT_CFEND)
        /*
         * Descriptors are now processed in the first-level
         * interrupt handler to support U-APSD trigger search.
         * This must also be done even when U-APSD is not active to support
         * other error handling that requires immediate attention.
         * We check bf_status to find out if the bf's descriptors have
         * been processed by the HAL.
         */
        if (!(bf->bf_status & ATH_BUFSTATUS_DONE)) {
            ATH_RXBUF_UNLOCK(sc);
            break;
        }
#else
        OS_SYNC_SINGLE(sc->sc_osdev,
                       bf->bf_daddr, sc->sc_txdesclen, BUS_DMA_FROMDEVICE,
                       OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
#endif

        ds = bf->bf_desc;
		rxs = &(bf->bf_rx_status);
        ++rx_processed;

#if !(ATH_SUPPORT_DESCFAST && ATH_SUPPORT_CFEND)
        /* Non-UAPSD case: descriptor is processed here at DPC level */

        /*
         * Must provide the virtual address of the current
         * descriptor, the physical address, and the virtual
         * address of the next descriptor in the h/w chain.
         * This allows the HAL to look ahead to see if the
         * hardware is done with a descriptor by checking the
         * done bit in the following descriptor and the address
         * of the current descriptor the DMA engine is working
         * on.  All this is necessary because of our use of
         * a self-linked list to avoid rx overruns.
         */
        retval = ath_hal_rxprocdesc(ah, ds, bf->bf_daddr, PA2DESC(sc, ds->ds_link), 0, rxs);
        if (HAL_EINPROGRESS == retval) {
            struct ath_buf *tbf = TAILQ_NEXT(bf, bf_list);

            /*
             * Due to a h/w bug the descriptor status words could
             * get corrupted, including the done bit. Hence, check
             * if the next descriptor's done bit is set or not.
             */
            if (tbf) {
                struct ath_desc *tds = tbf->bf_desc;

                /*
                 * If next descriptor's done bit is also not set,
                 * we can't tell if current descriptor is corrupted.
                 */
                OS_SYNC_SINGLE(sc->sc_osdev,
                               tbf->bf_daddr, sc->sc_txdesclen, BUS_DMA_FROMDEVICE,
                               OS_GET_DMA_MEM_CONTEXT(tbf, bf_dmacontext));

                retval = ath_hal_rxprocdesc(ah, tds, tbf->bf_daddr, PA2DESC(sc, tds->ds_link),
								0, &(tbf->bf_rx_status));
                if (HAL_EINPROGRESS == retval) {
#ifdef ATH_ADDITIONAL_STATS
#ifdef QCA_SUPPORT_CP_STATS
                    pdev_lmac_cp_stats_ast_rx_hal_in_progress_inc(sc->sc_pdev, 1);
#else
                    sc->sc_stats.ast_rx_hal_in_progress++;
#endif
#endif
                    ATH_RXBUF_UNLOCK(sc);
                    break;
                }
                OS_SYNC_SINGLE(sc->sc_osdev,
                               bf->bf_daddr, sc->sc_txdesclen, BUS_DMA_FROMDEVICE,
                               OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

                /*
                 * Next descriptor's done bit is set. The current
                 * descriptor may be corrupted. However, check current
                 * descriptor's done bit because it may have been set
                 * while we were checking the next descriptor.
                 */
                ath_hal_rxprocdesc(ah, ds, bf->bf_daddr, PA2DESC(sc, ds->ds_link), 0, rxs);
            } else {
                ATH_RXBUF_UNLOCK(sc);
                break;
            }
        }
        /* XXX: we do not support frames spanning multiple descriptors */
        bf->bf_status |= ATH_BUFSTATUS_DONE;
#if !(ATH_SUPPORT_DESCFAST)
        bf->bf_status &= ~ATH_BUFSTATUS_SYNCED;
#endif /* ! ATH_SUPPORT_DESCFAST */
#endif /* !(ATH_SUPPORT_DESCFAST && ATH_SUPPORT_CFEND) */

        /* Force PPM tracking */
        ath_force_ppm_logic(&sc->sc_ppm_info, bf, HAL_OK, rxs);

        wbuf = bf->bf_mpdu;
        if (wbuf == NULL) {        /* XXX ??? can this happen */
            DPRINTF(sc, ATH_DEBUG_RX_PROC, "no mpdu (%s)\n", __func__);
            ATH_RXBUF_UNLOCK(sc);
            continue;
        }

#ifdef AR_DEBUG
        if (CHK_SC_DEBUG(sc, ATH_DEBUG_RECV_DESC))
            ath_printrxbuf(bf, 1);
#endif
        /*
         * Now we know it's a completed frame, we can indicate the frame.
         * Remove the previous holding descriptor and leave this one
         * in the queue as the new holding descriptor.
         */
        if (bf_held) {
            TAILQ_REMOVE(&sc->sc_rxbuf, bf_held, bf_list);
            bf_held->bf_status &= ~ATH_BUFSTATUS_STALE;
            if (bf_held->bf_status & ATH_BUFSTATUS_FREE) {
                TAILQ_INSERT_TAIL(&sc->sc_rxbuf, bf_held, bf_list);
                ath_rx_buf_link(sc, bf_held, true); /* try to requeue this descriptor */
            }
        }
        bf->bf_status |= ATH_BUFSTATUS_STALE;
        bf_held = bf;
        if (flush == RX_DROP) {
            /*
             * If we're asked to flush receive queue, directly
             * chain it back at the queue without processing it.
             */
            goto rx_next;
        }

        /* Make sure descriptor/buffer is not corrupted before use */
        if (rxs->rs_status & HAL_RXERR_INCOMP) {
            __11nstats(sc, rx_dsstat_err);
            goto rx_next;
        }

        wh = (struct ieee80211_frame *)wbuf_raw_data(wbuf);
        frame_fc0 = wh->i_fc[0];
        OS_MEMZERO(&rx_status, sizeof(ieee80211_rx_status_t));

#ifndef REMOVE_PKT_LOG
        /* do pktlog */
        {
            struct log_rx log_data = {0};
            log_data.ds = ds;
            log_data.status = rxs;
            log_data.bf = bf;
            ath_log_rx(sc, &log_data, 0);
        }
#endif
#if ATH_SUPPORT_CFEND && !(ATH_SUPPORT_DESCFAST)
        if (0 == rxs->rs_status) {
            struct ieee80211_frame *frm;

            if (!(bf->bf_status & ATH_BUFSTATUS_SYNCED)) {
                OS_SYNC_SINGLE(sc->sc_osdev,
                    bf->bf_buf_addr[0],
                    wbuf_get_len(wbuf),
                    BUS_DMA_FROMDEVICE,
                    OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
                bf->bf_status |= ATH_BUFSTATUS_SYNCED;
            }
            frm = (struct ieee80211_frame*) wbuf_raw_data(wbuf);
            /* check if 4 address or 3 address format */
            if ((frm->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
                IEEE80211_FC1_DIR_DSTODS) {
                sc->sc_ieee_ops->get_bssid(sc->sc_ieee, (struct
                            ieee80211_frame_min*)frm,
                            sc->rxLastAggrBssid);
            } else {
                if ((frm->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
                    IEEE80211_FC1_DIR_FROMDS) {
                    ATH_ADDR_COPY(sc->rxLastAggrBssid, frm->i_addr2);
                } else {
                    ATH_ADDR_COPY(sc->rxLastAggrBssid, frm->i_addr1);
                }
            }
        }
#endif
#if !(ATH_SUPPORT_DESCFAST)
#ifdef ATH_SUPPORT_UAPSD
        if ((flush == RX_PROCESS) || (flush == RX_FORCE_PROCESS)) {
            if (rxs->rs_status == 0) {
                /* Check for valid UAPSD trigger */
                /* Note: assumed that this routine is in non-ISR context */
                rxs->rs_isapsd = sc->sc_ieee_ops->check_uapsdtrigger(sc->sc_pdev, wbuf_raw_data(wbuf), rxs->rs_keyix, false);
            }
        }
#else
        rxs->rs_isapsd = 0;
#endif
#endif


        if (unlikely(rxs->rs_status != 0)) {
			WLAN_PHY_STATS(phy_stats, ast_rx_err);
#if ATH_SUPPORT_CFEND && !(ATH_SUPPORT_DESCFAST)
            /* at last subframe of aggregate, look for bad last frame and
             * start WAR
             */
            if (rxs->rs_isaggr && !rxs->rs_moreaggr) {
                ath_check_n_send_cfend(sc, rxs, retval, bf,
                        sc->rxLastAggrBssid);
            }
#endif
            if (rxs->rs_status & HAL_RXERR_CRC) {
                rx_status.flags |= ATH_RX_FCS_ERROR;
                WLAN_PHY_STATS(phy_stats, ast_rx_crcerr);
            }

            if (rxs->rs_flags & HAL_RX_DECRYPT_BUSY) {
                WLAN_PHY_STATS(phy_stats, ast_rx_decrypt_busyerr);
            }
            if (rxs->rs_status & HAL_RXERR_FIFO) {
				WLAN_PHY_STATS(phy_stats, ast_rx_fifoerr);
            }
            if (rxs->rs_status & HAL_RXERR_PHY) {
                WLAN_PHY_STATS(phy_stats, ast_rx_phyerr);
                phyerr = rxs->rs_phyerr & 0x1f;
                WLAN_PHY_STATS(phy_stats, ast_rx_phy[phyerr]);
#ifdef ATH_SUPPORT_DFS
                {
                    u_int64_t tsf = ath_hal_gettsf64(ah);
                    /* Process phyerrs */
                    ath_process_phyerr(sc, bf, rxs, tsf);
                }
#endif

#if WLAN_SPECTRAL_ENABLE
                {
                    struct ath_spectral *spectral = (struct ath_spectral*)sc->sc_spectral;
                    u_int64_t tsf = ath_hal_gettsf64(ah);
                    if ((spectral)&&(is_spectral_phyerr(spectral, bf, rxs))) {
                        SPECTRAL_LOCK(spectral);
                        ath_process_spectraldata(spectral,bf, rxs, tsf);
                        SPECTRAL_UNLOCK(spectral);
                    }
                }
#endif  /* WLAN_SPECTRAL_ENABLE */

                if (sc->sc_opmode != HAL_M_MONITOR ||
                    ATH_WIRESHARK_FILTER_PHY_ERR(sc) )
                {
                    goto rx_next;
				}
            }

            if (rxs->rs_status & HAL_RXERR_DECRYPT) {
                /*
                 * Decrypt error. We only mark packet status here
                 * and always push up the frame up to let NET80211 layer
                 * handle the actual error case, be it no decryption key
                 * or real decryption error.
                 * This let us keep statistics there.
                 */
                WLAN_PHY_STATS(phy_stats, ast_rx_decrypterr);
                rx_status.flags |= ATH_RX_DECRYPT_ERROR;
            }
            else if (rxs->rs_status & HAL_RXERR_MIC) {
                /*
                 * Demic error. We only mark frame status here
                 * and always push up the frame up to let NET80211 layer
                 * handle the actual error case.
                 * This let us keep statistics there and also apply the
                 * WAR for bug 6903: (Venice?) Hardware may
                 * post a false-positive MIC error.  Need to expose this
                 * error to tkip_demic() to really declare a failure.
                 */
                if ((frame_fc0 & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_CTL) {
                    /*
                     * As doc. in hardware bug 30127, sometimes, we get invalid
                     * MIC failures on valid control frames. Remove these mic errors.
                     */
                    rxs->rs_status &= ~HAL_RXERR_MIC;
					WLAN_PHY_STATS(phy_stats, ast_rx_demicok);
                }
                else {
                    WLAN_PHY_STATS(phy_stats, ast_rx_demicerr);
                    rx_status.flags |= ATH_RX_MIC_ERROR;
                }
            } else {
                WLAN_PHY_STATS(phy_stats, ast_rx_demicok);
            }

            /*
             * Reject error frames with the exception of decryption, MIC,
             * and key-miss failures.
             * For monitor mode, we also ignore the CRC error.
             */

            /*
             * The bad CRC frames need to be shipped up the stack
             * recover the audio data.
             *
             */
            if ((sc->sc_opmode == HAL_M_MONITOR)) {

                u_int32_t mask;
                mask = HAL_RXERR_DECRYPT | HAL_RXERR_MIC |
                       HAL_RXERR_KEYMISS | HAL_RXERR_CRC;
				if ( ! ATH_WIRESHARK_FILTER_PHY_ERR(sc) ) {
                    mask |= HAL_RXERR_PHY;
				}
                if (rxs->rs_status & ~mask) {
                    goto rx_next;
                }
            }
            else {
                if (rxs->rs_status &
                    ~(HAL_RXERR_DECRYPT | HAL_RXERR_MIC | HAL_RXERR_KEYMISS))
                {
                    goto rx_next;
                } else {
                    if (rxs->rs_status & HAL_RXERR_KEYMISS)
                        rx_status.flags |= ATH_RX_KEYMISS;
                }
            }
        }

        /*
         * To workaround a h/w issue due to which the status
         * portion of the descriptor could get corrupted.
         * Refer to BUG#28934 for additional details.
         */
        /*
         * Check for 0 length also, for frames with corrupted
         * descriptor status
         */
        if ((sc->sc_opmode != HAL_M_MONITOR ||
#if ATH_SUPPORT_WIRESHARK
            (sc->sc_tapmonenable == 0) ||
#endif
             ATH_WIRESHARK_FILTER_PHY_ERR(sc)) &&
            (sc->sc_rxbufsize < rxs->rs_datalen ||
             0 == rxs->rs_datalen))
        {
#if AP_MULTIPLE_BUFFER_RCV
            if ((!rxs->rs_more) && (sc->sc_rxpending==NULL))
#endif
            {
                __11nstats(sc, rx_dsstat_err);
                if (sc->sc_opmode != HAL_M_MONITOR) {
                    DPRINTF(sc,ATH_DEBUG_ANY,
                            "%s: Incorrect frame length %d "
                            "due to rx ds status corruption\n",
                            __func__, rxs->rs_datalen);
                }

                goto rx_next;
            }
        }

        if (rxs->rs_more) {
#if USE_MULTIPLE_BUFFER_RCV || AP_MULTIPLE_BUFFER_RCV
#if USE_MULTIPLE_BUFFER_RCV
            /*
             * Frame spans multiple descriptors; save
             * it for the next completed descriptor, it
             * will be used to construct a jumbogram.
             */
            if (sc->sc_rxpending != NULL) {
                /* Max frame size is currently 2 clusters, So if we get a 2nd one with More to follow, discard the first */
                DPRINTF(sc, ATH_DEBUG_ANY,
                        "%s: already a pending wbuf %pK len = %d n",
                        __func__, sc->sc_rxpending, rxs->rs_datalen);

                phy_stats->ast_rx_toobig++;
                goto rx_next;
            }

            // Set RxPending to Wbuf - hopefully, the next one will complete the chain.
            sc->sc_rxpending = wbuf;
            sc->sc_bfpending = bf;
            // Set the length. Packet length will be set when the 2nd buffer is processed.
            wbuf_init(sc->sc_rxpending, rxs->rs_datalen);
            ATH_RXBUF_UNLOCK(sc);
            continue;
#endif
#if AP_MULTIPLE_BUFFER_RCV
            if (sc->sc_rxpending)
            {
                wbuf_t	tempbuf = sc->sc_rxpending;
                while (wbuf_next(tempbuf)!=NULL)
                {
                    tempbuf = wbuf_next(tempbuf);
                }
                wbuf_init(wbuf, rxs->rs_datalen);
                wbuf_set_next(wbuf,NULL);
                bf->bf_mpdu = sc->sc_rxpending;
                sc->sc_bfpending->bf_mpdu = NULL;
                wbuf_set_next(tempbuf,wbuf);
                ath_rx_buf_relink(dev,sc->sc_bfpending);
                sc->sc_bfpending = bf;
                sc->sc_prebuflen += rxs->rs_datalen;
            }
            else
            {
                sc->sc_rxpending = wbuf;
                sc->sc_bfpending = bf;
                sc->sc_prebuflen = rxs->rs_datalen;
                wbuf_init(wbuf,rxs->rs_datalen);
                wbuf_set_next(wbuf,NULL);
            }
            ATH_RXBUF_UNLOCK(sc);
            continue;
#endif
#else /* !USE_MULTIPLE_BUFFER_RCV && !AP_MULTIPLE_BUFFER_RCV*/
            /*
             * Frame spans multiple descriptors; this
             * cannot happen yet as we don't support
             * jumbograms.    If not in monitor mode,
             * discard the frame.
             */
#ifndef ERROR_FRAMES
            /*
             * Enable this if you want to see
             * error frames in Monitor mode.
             */
            if (sc->sc_opmode != HAL_M_MONITOR) {
                WLAN_PHY_STATS(phy_stats, ast_rx_toobig);
                goto rx_next;
            }
#endif
#endif /* USE_MULTIPLE_BUFFER_RCV || AP_MULTIPLE_BUFFER_RCV*/
        }
#if USE_MULTIPLE_BUFFER_RCV || AP_MULTIPLE_BUFFER_RCV
#if USE_MULTIPLE_BUFFER_RCV
        else if (sc->sc_rxpending != NULL) {
            /*
             * This is the second part of a jumbogram,
             * chain it to the first wbuf, adjust the
             * frame length, and clear the rxpending state.
             */

            wbuf_setnextpkt(sc->sc_rxpending, wbuf);
            // Set the length of the buffer for the 2nd one in the chain. We wont init it later.
            wbuf_init(wbuf, rxs->rs_datalen);
			//printk("%s len1 %d, len2 %d\n", __func__, wbuf_get_pktlen(sc->sc_rxpending), wbuf_get_pktlen(wbuf));
            // Switch the wbuf to the pending one : we've chained them and now ready to indicate.
            bf->bf_mpdu = wbuf = sc->sc_rxpending;
            // Prep for the next "more" handling.
            sc->sc_rxpending = NULL;
            sc->sc_bfpending = NULL;
        }
#endif
#if AP_MULTIPLE_BUFFER_RCV
        else if (sc->sc_rxpending != NULL) {
            wbuf_t databuf;
            databuf = ath_rx_buf_merge(sc,rxs->rs_datalen,sc->sc_rxpending,wbuf);
            if (databuf == NULL) {
                DPRINTF(sc, ATH_DEBUG_ANY,"%s: no buffer to used!\n",__func__);
                goto rx_next;
            }
            wbuf_init(databuf,sc->sc_prebuflen+rxs->rs_datalen);
            rxs->rs_datalen = wbuf_get_pktlen(databuf);
            ATH_SET_RX_CONTEXT_BUF(databuf, bf);
            wbuf_free(wbuf);
            bf->bf_mpdu = wbuf = databuf;
            sc->sc_bfpending->bf_mpdu = NULL;
            ath_rx_buf_relink(dev,sc->sc_bfpending);
            sc->sc_rxpending = NULL;
            sc->sc_bfpending = NULL;
            sc->sc_prebuflen = 0;
        }
#endif
#endif /* USE_MULTIPLE_BUFFER_RCV || AP_MULTIPLE_BUFFER_RCV*/
        else {
            // this wbuf doesn't span multiple rx buffers.. We can set buflen and pktlen.
            wbuf_init(wbuf, rxs->rs_datalen);
        }
        /*
         * Release the lock here in case ieee80211_input() return
         * the frame immediately by calling ath_rx_mpdu_requeue().
         */
        ATH_RXBUF_UNLOCK(sc);

        ath_rx_process(sc, bf, rxs, frame_fc0, &rx_status, &chainreset);

        /*
         * For frames successfully indicated, the buffer will be
         * returned to us by upper layers by calling ath_rx_mpdu_requeue,
         * either synchronusly or asynchronously.
         * So we don't want to do it here in this loop.
         */
        continue;

rx_next:

#if USE_MULTIPLE_BUFFER_RCV || AP_MULTIPLE_BUFFER_RCV
        if (sc->sc_rxpending) {
            // We come to rx_next whenever we have packet errors of any kind.
            // It's now safe to requeue a pending packet...
#if USE_MULTIPLE_BUFFER_RCV
            TAILQ_INSERT_TAIL(&sc->sc_rxbuf, sc->sc_bfpending, bf_list);
            ath_rx_buf_link(sc, sc->sc_bfpending, true);
#endif
#if AP_MULTIPLE_BUFFER_RCV
            wbuf_t	pointbuf,tempbuf;
            pointbuf = tempbuf = sc->sc_rxpending;
            while (pointbuf!=NULL)
            {
                pointbuf = wbuf_next(tempbuf);
                wbuf_set_next(tempbuf,NULL);
                wbuf_free(tempbuf);
                tempbuf = pointbuf;
            }
            sc->sc_bfpending->bf_mpdu = NULL;
            ath_rx_buf_relink(dev,sc->sc_bfpending);
#endif
            sc->sc_rxpending = NULL;
            sc->sc_bfpending = NULL;
        }
#endif /* USE_MULTIPLE_BUFFER_RCV || AP_MULTIPLE_BUFFER_RCV*/
        bf->bf_status |= ATH_BUFSTATUS_FREE;

        ATH_RXBUF_UNLOCK(sc);
#if ATH_SUPPORT_RX_PROC_QUOTA
    } while (process_frame_cnt > rx_processed);
#else
    } while (TRUE);
#endif

#if WLAN_SPECTRAL_ENABLE
    if(sc->sc_spectral){
    spectral_skb_dequeue((unsigned long)sc->sc_spectral);
    }
#endif  /* WLAN_SPECTRAL_ENABLE */

#ifdef ATH_ADDITIONAL_STATS
    if (rx_processed < ATH_RXBUF ) {
        sc->sc_stats.ast_pkts_per_intr[rx_processed]++;
    }
    else {
        sc->sc_stats.ast_pkts_per_intr[ATH_RXBUF]++;
    }
#endif

    if (chainreset) {
        DPRINTF(sc, ATH_DEBUG_RESET, "Reset rx chain mask. Do internal"
                " reset. (%s)\n",__func__);
        ASSERT(flush == 0);
        ath_internal_reset(sc);
    }
   /*setting Rx interupt uncondionally as when we schedule tasklet
     from below condition we disable RX interrupt and in further
     iterations if we genuinely exit tasklet then Rx interrupt will
     remain disabled for ever */
#if ATH_SUPPORT_RX_PROC_QUOTA
    sc->sc_imask |= HAL_INT_RX;
    if(rx_processed >= process_frame_cnt) {
        sc->sc_rx_work_lp=1;
        sc->sc_imask &= ~HAL_INT_RX;
        /*
         * Looplimit feature is only supported for osprey+.
         * If we are here, it must be disabled.
         */
#if !ATH_RX_LOOPLIMIT_TIMER
        ATH_SCHEDULE_TQUEUE(&sc->sc_osdev->intr_tq, &needmark);
#else
        printk("RX_LOOPLIMIT enabled in non-supported device\n");
#endif
    }
#endif
    return 0;
#undef PA2DESC
}


#if AP_MULTIPLE_BUFFER_RCV
HAL_BOOL ath_rx_buf_relink(ath_dev_t dev,struct ath_buf *bf)

{
	wbuf_t	tempbuf;
	struct ath_softc *sc = ATH_DEV_TO_SC(dev);
	tempbuf = bf->bf_mpdu;

	if (tempbuf!=NULL)
		wbuf_free(tempbuf);
	tempbuf = ath_rxbuf_alloc(sc, sc->sc_rxbufsize);
	 if (tempbuf == NULL) {
		DPRINTF(sc, ATH_DEBUG_ANY,
				"%s: no buffer to used!\n",
				__func__);
		return AH_FALSE;
	}

	bf->bf_mpdu = tempbuf;
	/*
	 * do not invalidate the cache for the new/recycled skb,
	 * because the cache will be invalidated in rx ISR/tasklet
	 */
	bf->bf_buf_addr[0] = bf->bf_dmacontext = virt_to_phys(tempbuf->data);
	ATH_SET_RX_CONTEXT_BUF(tempbuf, bf);
	bf->bf_status = ATH_BUFSTATUS_FREE;
	//unlock here to avoid dobuld lock
	ATH_RXBUF_UNLOCK(sc);
	/* queue the new wbuf to H/W */
	ath_rx_requeue(sc, tempbuf);
	ATH_RXBUF_LOCK(sc);
	return AH_TRUE;
}

wbuf_t ath_rx_buf_merge(struct ath_softc *sc, u_int32_t len,wbuf_t sbuf,wbuf_t wbuf)
{
	wbuf_t	databuf,pointbuf,tempbuf;
	u_int16_t datalen = 0;

	pointbuf = tempbuf = sbuf;
	databuf = ath_rxbuf_alloc(sc, sc->sc_prebuflen + len);
	if (databuf == NULL) {
		DPRINTF(sc, ATH_DEBUG_ANY,
				"%s: no buffer to used!\n",
				__func__);
		return NULL;
	}

	while (pointbuf!=NULL) // here will release all wbuf, it needs to set sc->sc_bfpending->bf_mpdu = NULL; before re-link
	{
		wbuf_copydata(pointbuf, 0, wbuf_get_pktlen(pointbuf), wbuf_header(databuf)+datalen);
		datalen+=wbuf_get_pktlen(pointbuf);
		pointbuf = wbuf_next(tempbuf);
		wbuf_set_next(tempbuf,NULL);
		wbuf_free(tempbuf);
		tempbuf = pointbuf;
	}

	wbuf_init(wbuf, len);
	wbuf_copydata(wbuf, 0, wbuf_get_pktlen(wbuf), wbuf_header(databuf)+datalen);
	datalen += len;
	return databuf;
}
#endif

/*
 * Cleanup per-node receive state
 */
void
ath_rx_node_free(struct ath_softc *sc, struct ath_node *an)
{
    ath_rx_node_cleanup(sc,an);
    if (sc->sc_rxaggr) {
        struct ath_arx_tid *rxtid;
        int tidno;

        /* Init per tid rx state */
        for (tidno = 0, rxtid = &an->an_rx_tid[tidno]; tidno < WME_NUM_TID;
             tidno++, rxtid++) {

            /* must cancel timer first */
            ATH_CANCEL_RXTIMER(&rxtid->timer, CANCEL_NO_SLEEP);
            ATH_FREE_RXTIMER(&rxtid->timer);

            ATH_RXTID_LOCK_DESTROY(rxtid);
        }
    }
}

static u_int8_t
ath_rx_detect_antenna(struct ath_softc *sc, struct ath_rx_status *rxstat)
{
#define    IS_CHAN_5GHZ(_c)    (((_c).channel_flags & CHANNEL_5GHZ) != 0)
#define ATH_RX_CHAINMASK_CLR(_chainmask, _chain) ((_chainmask) &= ~(1 << (_chain)))
    u_int8_t rx_chainmask = sc->sc_rx_chainmask;
    int rssiRef, detectThresh, detectDelta;

    if (IS_CHAN_5GHZ(sc->sc_curchan)) {
        detectThresh = sc->sc_rxchaindetect_thresh5GHz;
        detectDelta = sc->sc_rxchaindetect_delta5GHz;
    } else {
        detectThresh = sc->sc_rxchaindetect_thresh2GHz;
        detectDelta = sc->sc_rxchaindetect_delta2GHz;
    }

    switch (sc->sc_rxchaindetect_ref) {
    case 0:
        rssiRef = rxstat->rs_rssi;
        if (rssiRef < detectThresh) {
            return 0;
        }

        if (rssiRef - rxstat->rs_rssi_ctl1 > detectDelta) {
            ATH_RX_CHAINMASK_CLR(rx_chainmask, 1);
        }

        if (rssiRef - rxstat->rs_rssi_ctl2 > detectDelta) {
            ATH_RX_CHAINMASK_CLR(rx_chainmask, 2);
        }
        break;

    case 1:
        rssiRef = rxstat->rs_rssi_ctl1;
        if (rssiRef < detectThresh) {
            return 0;
        }

        if (rssiRef - rxstat->rs_rssi_ctl2 > detectDelta) {
            ATH_RX_CHAINMASK_CLR(rx_chainmask, 2);
        }
        break;

    case 2:
        rssiRef = rxstat->rs_rssi_ctl2;
        if (rssiRef < detectThresh) {
            return 0;
        }

        if (rssiRef - rxstat->rs_rssi_ctl1 > detectDelta) {
            ATH_RX_CHAINMASK_CLR(rx_chainmask, 1);
        }
        break;
    }

    return rx_chainmask;
#undef IS_CHAN_5GHZ
#undef ATH_RX_CHAINMASK_CLR
}

#if ATH_SUPPORT_DESCFAST
/*
 * Check for UAPSD Triggers.
 * Context: Rx Interrupt
 */
void
ath_rx_proc_descfast(ath_dev_t dev)
{
#define PA2DESC(_sc, _pa)                                               \
    ((struct ath_desc *)((caddr_t)(_sc)->sc_rxdma.dd_desc +             \
                         ((_pa) - (_sc)->sc_rxdma.dd_desc_paddr)))

    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_buf *bf, *bf_first, *bf_last;
    struct ath_desc *ds;
    struct ath_rx_status *rxs;
    HAL_STATUS retval;
    wbuf_t wbuf;
    bf_first = TAILQ_FIRST(&sc->sc_rxbuf);

    for (bf = bf_first; bf; bf = TAILQ_NEXT(bf, bf_list)) {

        bf_last = TAILQ_NEXT(bf, bf_list);
        if ((bf_last == NULL) && (bf->bf_status & ATH_BUFSTATUS_DONE)) {
            /*
             * The holding descriptor is the last descriptor in queue.
             */
            break;
        }

        if (bf->bf_status & ATH_BUFSTATUS_DONE){
            continue;
        }

        OS_SYNC_SINGLE(sc->sc_osdev,
                       bf->bf_daddr, sc->sc_txdesclen, BUS_DMA_FROMDEVICE,
                       OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

        ds = bf->bf_desc;
        bf->bf_status &= ~ATH_BUFSTATUS_SYNCED;
		rxs = &(bf->bf_rx_status);
        wbuf = bf->bf_mpdu;
        if (wbuf == NULL) {  /* XXX ??? can this happen */
            continue;
        }
#if ATH_SUPPORT_CFEND
        retval = ath_hal_rxprocdesc(ah, ds, bf->bf_daddr,
                    PA2DESC(sc, ds->ds_link), 0, rxs);
#else
        retval = ath_hal_get_rxkeyidx(ah, ds, &(rxs->rs_keyix), &(rxs->rs_status));
#endif
        if (HAL_EINPROGRESS == retval) {
            /*
             * h/w bug, the RXS could get corrupted. Hence, check
             * if the next descriptor's done bit is set or not.
             */
            struct ath_buf *tbf = TAILQ_NEXT(bf, bf_list);
            if (tbf) {
                struct ath_desc *tds = tbf->bf_desc;

                OS_SYNC_SINGLE(sc->sc_osdev,
                               tbf->bf_daddr, sc->sc_txdesclen, BUS_DMA_FROMDEVICE,
                               OS_GET_DMA_MEM_CONTEXT(tbf, bf_dmacontext));

#if ATH_SUPPORT_CFEND
                retval = ath_hal_rxprocdesc(ah, tds, tbf->bf_daddr,
                                PA2DESC(sc, tds->ds_link), 0, &(tbf->bf_rx_status));
#else
                retval = ath_hal_get_rxkeyidx(ah, tds,
								&(tbf->bf_rx_status.rs_keyix), &(tbf->bf_rx_status.rs_status));
#endif
                if (HAL_EINPROGRESS == retval)
                    break;
            } else {
                break;
            }

            OS_SYNC_SINGLE(sc->sc_osdev,
                           bf->bf_daddr, sc->sc_txdesclen, BUS_DMA_FROMDEVICE,
                           OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));

#if ATH_SUPPORT_CFEND
            /* process desc again to get the updated value */
            retval = ath_hal_rxprocdesc(ah, ds, bf->bf_daddr,
                        PA2DESC(sc, ds->ds_link), 0, rxs);
#else
            retval = ath_hal_get_rxkeyidx(ah, ds, &(rxs->rs_keyix), &(rxs->rs_status));
#endif
        }
#if ATH_SUPPORT_CFEND
        bf->bf_status |= ATH_BUFSTATUS_DONE;
#endif
        /* Skip frames with error - except HAL_RXERR_KEYMISS since
         * for static WEP case, all the frames will be marked with HAL_RXERR_KEYMISS
         * since there is no key cache entry added for associated station in that case
         */
        if (rxs->rs_status & ~HAL_RXERR_KEYMISS) {

#if ATH_SUPPORT_CFEND
            /* at last subframe of aggregate, look for bad last frame and
             * start WAR
             */
            if ((rxs->rs_isaggr && !rxs->rs_moreaggr)) {
                ath_check_n_send_cfend(sc, rxs, retval, bf,
                        sc->rxLastAggrBssid);
            }
#endif

            continue;
        } else {
#if ATH_SUPPORT_CFEND
            struct ieee80211_frame *wh;
            if (!(bf->bf_status & ATH_BUFSTATUS_SYNCED)) {
                OS_SYNC_SINGLE(sc->sc_osdev,
                    bf->bf_buf_addr[0],
                    wbuf_get_len(wbuf),
                    BUS_DMA_FROMDEVICE,
                    OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
                bf->bf_status |= ATH_BUFSTATUS_SYNCED;
            }
            wh = (struct ieee80211_frame*) wbuf_raw_data(wbuf);
            /* check if 4 address or 3 address format */
            if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
                IEEE80211_FC1_DIR_DSTODS) {
                /* Most of the time bssid would be the ic_macaddr. But
                 * that may not be correct if some one chooses to run the
                 * MBSSID+wds ???. To get the BSSID we should go up to the
                 * IEEE layer access. Then we should be doing it in
                 * tasklet context, so it looses meaning of sending cfend.
                 * We should not be taking any locks in interrupt context,
                 * other non-Linux operating systems do not work well. So we
                 * try to have another interupt safe routine that simply
                 * reads and returns the bssid.
                 * if we fail to get the bssid, we do not loose much here,
                 * as we only drop sending cfend frame. That should not be
                 * much costly, than taking lock and going panic.
                 * When frame is not a 4 address frame, FROM_DS and TO_DS
                 * should help finding the correct bssid, and that works.
                 */
                sc->sc_ieee_ops->get_bssid(sc->sc_ieee, (struct
                            ieee80211_frame_min*)wh,
                            sc->rxLastAggrBssid);
            } else {
                if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) ==
                    IEEE80211_FC1_DIR_FROMDS) {
                    ATH_ADDR_COPY(sc->rxLastAggrBssid, wh->i_addr2);
                } else {
                    ATH_ADDR_COPY(sc->rxLastAggrBssid,wh->i_addr1);
                }
            }
#endif
        }
#ifdef ATH_SUPPORT_UAPSD
        /* Sync before using rx buffer */
        if (!(bf->bf_status & ATH_BUFSTATUS_SYNCED)) {
            OS_SYNC_SINGLE(sc->sc_osdev,
                       bf->bf_buf_addr[0], wbuf_get_len(wbuf), BUS_DMA_FROMDEVICE,
                       OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            bf->bf_status |= ATH_BUFSTATUS_SYNCED;
        }

        /* Check for valid UAPSD trigger */
        /* Note: assumed that this routine is in ISR context */
        rxs->rs_isapsd = sc->sc_ieee_ops->check_uapsdtrigger(sc->sc_pdev, wbuf_raw_data(wbuf), rxs->rs_keyix, true);
#else
    rxs->rs_isapsd = 0;
#endif    // ATH_SUPPORT_UAPSD

    }
}
#endif

#if ATH_SUPPORT_WIRESHARK
/*
 * Ask the HAL to copy rx status info from the PHY frame descriptor
 * into the packet's radiotap header.
 */
void ath_fill_radiotap_hdr(ath_dev_t dev, struct ah_rx_radiotap_header *rh, struct ah_ppi_data *ppi,
						   wbuf_t wbuf)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;
    struct ath_buf *bf = ATH_GET_RX_CONTEXT_BUF(wbuf);
    struct ath_desc *ds = NULL;

    /* avoid NULL pointer crash in case bf is NULL */
    if (bf) {
        ds = bf->bf_desc;
    }

    ath_hal_fill_radiotap_hdr(ah, rh, ppi, ds, wbuf_raw_data(wbuf));
}
#endif /* ATH_SUPPORT_WIRESHARK */

#ifdef AR_DEBUG
void ath_printrxbuf(struct ath_buf *bf, u_int8_t desc_has_status)
{

 /* TO-DO */
}
#endif

#if ATH_SUPPORT_VOW_DCS
#define OFDM_SIFS_TIME 16
#define OFDM_SLOT_TIME 9
#define OFDM_DIFS_TIME 34 /*(16+2*9)*/
#define RECV_OVERHEAD  82 /*L_STF + L_LTF + L_SIG + HT_SIG + HT_STF + OFDM_SIFS_TIME + OFDM_DIFS_TIME*/

static INLINE u_int32_t
ath_get_ba_period(struct ath_softc *sc, u_int8_t rix)
{
    u_int8_t     cix, rate_code;
    const HAL_RATE_TABLE *rt;
    u_int32_t ba_period[3]={32, 44, 68};
    rt = sc->sc_currates;
    cix = rt->info[rix].controlRate;
    rate_code = (rt->info[cix].rate_code) & 0x03;
    return ba_period[rate_code? (rate_code-1):2];
 }

/*
 * ath_rx_dur - compute packet duration (NB: not NAV)
 * ref: depot/chips/owl/2.0/rtl/mac/doc/rate_to_duration_ht.xls
 *
 * rix - rate index
 * pktlen - total bytes (delims + data + fcs + pads + pad delims)
 * width  - 0 for 20 MHz, 1 for 40 MHz
 * half_gi - to use 4us v/s 3.6 us for symbol time
 */
u_int32_t
ath_rx_duration(struct ath_softc *sc, u_int8_t rix, int pktlen, int more, int error,
                 int width, int half_gi, HAL_BOOL shortPreamble)
{
    const HAL_RATE_TABLE    *rt = sc->sc_currates;
    u_int32_t               nbits, nsymbits, duration, nsymbols;
    u_int8_t                rc;
    int                     streams;
    const u_int32_t bits_per_symbol[][2] = {
  /* 20MHz 40MHz */
        {    26,   54 },     //  0: BPSK
        {    52,  108 },     //  1: QPSK 1/2
        {    78,  162 },     //  2: QPSK 3/4
        {   104,  216 },     //  3: 16-QAM 1/2
        {   156,  324 },     //  4: 16-QAM 3/4
        {   208,  432 },     //  5: 64-QAM 2/3
        {   234,  486 },     //  6: 64-QAM 3/4
        {   260,  540 },     //  7: 64-QAM 5/6
        {    52,  108 },     //  8: BPSK
        {   104,  216 },     //  9: QPSK 1/2
        {   156,  324 },     // 10: QPSK 3/4
        {   208,  432 },     // 11: 16-QAM 1/2
        {   312,  648 },     // 12: 16-QAM 3/4
        {   416,  864 },     // 13: 64-QAM 2/3
        {   468,  972 },     // 14: 64-QAM 3/4
        {   520, 1080 },     // 15: 64-QAM 5/6
        {    78,  162 },     // 16: BPSK
        {   156,  324 },     // 17: QPSK 1/2
        {   234,  486 },     // 18: QPSK 3/4
        {   312,  648 },     // 19: 16-QAM 1/2
        {   468,  972 },     // 20: 16-QAM 3/4
        {   624, 1296 },     // 21: 64-QAM 2/3
        {   702, 1458 },     // 22: 64-QAM 3/4
        {   780, 1620 },     // 23: 64-QAM 5/6
        {   104,  216 },     // 24: BPSK
        {   208,  432 },     // 25: QPSK 1/2
        {   312,  648 },     // 26: QPSK 3/4
        {   416,  864 },     // 27: 16-QAM 1/2
        {   624, 1296 },     // 28: 16-QAM 3/4
        {   832, 1728 },     // 29: 64-QAM 2/3
        {   936, 1944 },     // 30: 64-QAM 3/4
        {  1040, 2160 },     // 31: 64-QAM 5/6
    };
    if (rt && ((rix >= 0) && (rix < ARRAY_INFO_SIZE))) {
        rc = rt->info[rix].rate_code;
    }
    else {
        return  1000 ;
    }

    /*
     * for legacy rates, use old function to compute packet duration
     */
    nbits = (pktlen << 3) + (more ? 0 : OFDM_PLCP_BITS);
    nsymbits = bits_per_symbol[HT_RC_2_MCS(rc)][width];
    nsymbols = (nbits + nsymbits - 1) / nsymbits;

    if (!IS_HT_RATE(rc)) {
        duration = ath_hal_computetxtime(sc->sc_ah, rt, pktlen, rix,
                                     shortPreamble);
    }
    else {
        /*
         * find number of symbols: PLCP + data
         */

        if (!half_gi)
            duration = SYMBOL_TIME(nsymbols);
        else
            duration = SYMBOL_TIME_HALFGI(nsymbols);

        /*
         * addup duration for legacy/ht training and signal fields
         */
        streams = HT_RC_2_STREAMS(rc);
        if(error) {
          duration += (more ? 0 : (RECV_OVERHEAD + HT_LTF(streams) + ath_get_ba_period(sc,rix) ));
        }
        else {
          duration += (more ? 0 : (RECV_OVERHEAD + HT_LTF(streams)));
        }
    }
    return duration;
}
#undef OFDM_SIFS_TIME
#undef OFDM_SLOT_TIME
#undef OFDM_DIFS_TIME
#undef RECV_OVERHEAD
#endif


#if ATH_VOW_EXT_STATS
/*
 * Insert some stats info into the test packet's header.
 *
 * Test packets are Data type frames in the Clear or encrypted
 *   with WPA2-PSK CCMP, identified by a specific length && UDP
 *   && RTP && RTP eXtension && magic number.
 */
void
ath_add_ext_stats(struct ath_rx_status *rxs, wbuf_t wbuf,
                    struct ath_softc *sc, struct ath_phy_stats *phy_stats,
                    ieee80211_rx_status_t *rx_status)
{
    /*
     * TODO:
     *  - packet size is hardcoded, should be configurable
     *  - assumes no security, or WPA2-PSK CCMP security
     *  - EXT_HDR_SIZE is hardcoded, should calc from hdr size field
     *  - EXT_HDR fields are hardcoded, should be defined
     *  - EXT_HDR src fields are hardcoded, should be read from hdr
     */

#define IPV4_PROTO_OFF  38
#define UDP_PROTO_OFF   47
#define UDP_CKSUM_OFF   64
#define RTP_HDR_OFF     66
#define EXT_HDR_OFF     78
#define EXT_HDR_SIZE    ((8+1) * 4)     // should determine from ext hdr
#define ATH_EXT_STAT_DFL_LEN 1434
#define IP_VER4_N_NO_EXTRA_HEADERS 0x45
#define IP_PDU_PROTOCOL_UDP 0x11
#define UDP_PDU_RTP_EXT    ((2 << 6) | (1 << 4))  /* RTP Version 2 + X bit */
    unsigned char *bp;
    u_int32_t reg_rccnt;
    u_int32_t reg_cccnt;
    struct ieee80211_frame *wh;
    u_int16_t seqctrl;
    u_int16_t buf_len;
    int test_len = ATH_EXT_STAT_DFL_LEN;
    int frm_type, frm_subtype;
    HAL_VOWSTATS pcounts;
    int offset;

    wh = (struct ieee80211_frame *)wbuf_raw_data(wbuf);
    frm_type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    frm_subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;
    seqctrl = *(u_int16_t *)(wh->i_seq);
    bp = (unsigned char *)wbuf_raw_data(wbuf);
    buf_len = wbuf_get_pktlen(wbuf);

    /* Ignore non Data Types */
    if (!(frm_type & IEEE80211_FC0_TYPE_DATA)) {
        return;
    }

    /* Adjust for WDS */
    if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) {
        bp += QDF_MAC_ADDR_SIZE;
        test_len += QDF_MAC_ADDR_SIZE;
        offset = 4;
    } else {
        offset = 2;
    }

    /* Adjust for QoS Header */
    if (!(frm_subtype & IEEE80211_FC0_SUBTYPE_QOS)) {
        offset += 4;
    }

    bp -= offset;
    test_len -= offset;

    /* Adjust for AES security */
    /* Assumes WPA2-PSK CCMP only if security enabled, else open */
    if (wh->i_fc[1] & IEEE80211_FC1_WEP) {
        bp += IEEE80211_WEP_IVLEN + IEEE80211_WEP_KIDLEN
                    + IEEE80211_WEP_CRCLEN;
        test_len += 16;
    }

    /* only mark very specifc packets */
    if ((buf_len == test_len) &&
        (*(bp+RTP_HDR_OFF) == UDP_PDU_RTP_EXT) &&
        (*(bp+UDP_PROTO_OFF) == IP_PDU_PROTOCOL_UDP) &&
        (*(bp+IPV4_PROTO_OFF) == IP_VER4_N_NO_EXTRA_HEADERS))
    {
        /* Check for magic number and header length */
        if ((*(bp+EXT_HDR_OFF+0) == 0x12) &&
            (*(bp+EXT_HDR_OFF+1) == 0x34) &&
            (*(bp+EXT_HDR_OFF+2) == 0x00) &&
            (*(bp+EXT_HDR_OFF+3) == 0x08))
        {
            if(wh->i_fc[1] & IEEE80211_FC1_WEP)
            {
                /* don't clear the udp checksum here. In case of security, we may need
                * to do SW MIC on this packet. clear the checksum in ieee layer after
                * passing thru crpto layer. Store the udp checksum offset value in rx_stats */
                rx_status->vow_extstats_offset = (bp - (uint8_t *)wh) + UDP_CKSUM_OFF;
            }
            else
            {
                /* clear udp checksum so we do not have to recalculate it after
                 * filling in status fields */
                *(bp+UDP_CKSUM_OFF) = 0x00;
                *(bp+UDP_CKSUM_OFF+1) = 0x00;
            }

            ath_hal_getVowStats(sc->sc_ah, &pcounts,
                                AR_REG_RX_CLR_CNT | AR_REG_CYCLE_CNT);
            reg_rccnt = pcounts.rx_clear_count;
            reg_cccnt = pcounts.cycle_count;

            bp += EXT_HDR_OFF + 12;  // skip hdr and src fields

            /* Store the ext stats offset in rx_status which will be used at the time of SW MIC */
            rx_status->vow_extstats_offset =  (rx_status->vow_extstats_offset) | (((uint32_t)(bp - (uint8_t *)wh)) << 16);

            *bp++ = rxs->rs_rssi_ctl0;
            *bp++ = rxs->rs_rssi_ctl1;
            *bp++ = rxs->rs_rssi_ctl2;
            *bp++ = rxs->rs_rssi_ext0;
            *bp++ = rxs->rs_rssi_ext1;
            *bp++ = rxs->rs_rssi_ext2;
            *bp++ = rxs->rs_rssi;

            *bp++ = (unsigned char)(rxs->rs_flags & 0xff);

            *bp++ = (unsigned char)((rxs->rs_tstamp >> 8) & 0x7f);
            *bp++ = (unsigned char)(rxs->rs_tstamp & 0xff);

            *bp++ = (unsigned char)((phy_stats->ast_rx_phyerr >> 8) & 0xff);
            *bp++ = (unsigned char)(phy_stats->ast_rx_phyerr & 0xff);

            *bp++ = (unsigned char)((reg_rccnt >> 24) & 0xff);
            *bp++ = (unsigned char)((reg_rccnt >> 16) & 0xff);
            *bp++ = (unsigned char)((reg_rccnt >>  8) & 0xff);
            *bp++ = (unsigned char)(reg_rccnt & 0xff);

            *bp++ = (unsigned char)((reg_cccnt >> 24) & 0xff);
            *bp++ = (unsigned char)((reg_cccnt >> 16) & 0xff);
            *bp++ = (unsigned char)((reg_cccnt >>  8) & 0xff);
            *bp++ = (unsigned char)(reg_cccnt & 0xff);

            *bp++ = rxs->rs_rate;
            *bp++ = rxs->rs_moreaggr;

            *bp++ = (unsigned char)((seqctrl >> 8) & 0xff);
            *bp++ = (unsigned char)(seqctrl & 0xff);
        }
    }
#undef AR_RCCNT
#undef AR_CCCNT
}
#endif // EXT_STATS

#ifdef AR_DEBUG
static void dump_rx_desc(struct ath_softc *sc, int quenum)
{
    struct ath_buf *bf = NULL;
    int i = 0;

    TAILQ_FOREACH(bf, &sc->sc_rxbuf, bf_list){
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: [%d] RX bf 0x%pK bf->bf_mpdu %pK\n",__func__,__LINE__,
            i++, bf, bf->bf_mpdu);
     }
}
void ath_dump_rx_desc(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    if (sc->sc_enhanceddmasupport) {
        ath_dump_rx_edma_desc(dev);
    }
    else {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: sc->sc_rxlink %pK sc->sc_rxpending %pK\n",__func__,__LINE__,sc->sc_rxlink, sc->sc_rxpending);
        dump_rx_desc(sc, 0);
    }
}
#endif /* AR_DEBUG */

#if UMAC_SUPPORT_VI_DBG

/* This function is used to collect received pkt statistics which are used for debugging
 * video. The pkt statistics are maintained on a per second basis & a 10s history is
 * maintained. These stats can be read by the application when a pkt loss is encountered
 * by invoking athstats & this information can be used to find out the reason for pkt loss.
 */
void
ath_add_vi_dbg_stats(struct ath_softc *sc, const struct ath_rx_status *rxs)
{
    struct ath_stats *stats = (struct ath_stats *) &sc->sc_stats;
    u_int8_t  index = 0;
    u_int8_t  prev  = 0;
    u_int32_t timestamp_diff;
    u_int8_t  rate;
    int16_t   evm_temp1, evm_temp2;
    HAL_VOWSTATS pcounts;

#define MICROSEC_PER_SEC       1000000

    /* Do not log info for legacy rates or if logging is disabled */
    if (sc->sc_en_vi_dbg_log == 0 || (rxs->rs_rate & 0x80) == 0) {
	    return;
    }

    if (!sc->sc_vi_dbg_start) {
        stats->vi_stats_index = 0;
    }

    index = stats->vi_stats_index;

    prev = (index == 0) ? (ATH_STATS_VI_LOG_LEN - 1) : (index  - 1);

    if (sc->sc_vi_dbg_start) {
	    timestamp_diff = rxs->rs_tstamp - stats->vi_timestamp[prev];
    } else {
	    timestamp_diff = 0;
    }

    /* Stats are collected on a per second basis */
	/* EVM				Note: EVM is a signed number of dB above noise floor.
  	 * 	  			    The value 0x80 (-128) means invalid.
	 * 				          3-Stream      2-Stream      1-Stream
	 *				        ------------  ------------  ------------
	 *				        HT40   HT20   HT40   HT20   HT40   HT20
	 *				        -----  -----  -----  -----  -----  -----
	 *	Byte 1	Bit[7:0]	P0_S0  P0_S0  P0_S0  P0_S0  P0_S0  P0_S0
	 *	Byte 2	Bit[7:0]	P0_S1  P0_S1  P0_S1  P0_S1  P1_S0  P1_S0
	 * 	Byte 3	Bit[7:0]	P0_S2  P0_S2  P1_S0  P1_S0  P2_S0  P2_S0
	 * 	Byte 4	Bit[7:0]	P1_S0  P1_S0  P1_S1  P1_S1  P3_S0  P3_S0
	 *	Byte 5	Bit[7:0]	P1_S1  P1_S1  P2_S0  P2_S0  P4_S0   0x0
	 *  Byte 6	Bit[7:0]	P1_S2  P1_S2  P2_S1  P2_S1  P5_S0   0x0
	 *	Byte 7	Bit[7:0]	P2_S0  P2_S0  P3_S0  P3_S0   0x0    0x0
	 *	Byte 8	Bit[7:0]	P2_S1  P2_S1  P3_S1  P3_S1   0x0    0x0
	 * 	Byte 9	Bit[7:0]	P2_S2  P2_S2  P4_S0 notSent  0x0    0x0
	 *	Byte 10	Bit[7:0]	P3_S0  P3_S0  P4_S1 notSent  0x0    0x0
	 *	Byte 11	Bit[7:0]	P3_S1  P3_S1  P5_S0 notSent  0x0    0x0
	 *	Byte 12	Bit[7:0]	P3_S2  P3_S2  P5_S1 notSent  0x0    0x0
	 *	Byte 13	Bit[7:0]	P4_S0
	 *	Byte 14	Bit[7:0]	P4_S1
	 *	Byte 15	Bit[7:0]	P4_S2	 (bytes 13-18 are only sent by BB
	 *	Byte 16	Bit[7:0]	P5_S0	  if RX frame is 3-Stream HT40)
	 *	Byte 17	Bit[7:0]	P5_S1
	 *	Byte 18	Bit[7:0]	P5_S2
     */
    if (sc->sc_vi_dbg_start == 0 || (sc->sc_vi_dbg_start == 1 && timestamp_diff >= MICROSEC_PER_SEC)) {
	    stats->vi_timestamp[index]    = rxs->rs_tstamp;
	    stats->vi_rssi_ctl0[index]    = rxs->rs_rssi_ctl0;
        stats->vi_rssi_ctl1[index]    = rxs->rs_rssi_ctl1;
        stats->vi_rssi_ctl2[index]    = rxs->rs_rssi_ctl2;
        stats->vi_rssi_ext0[index]    = rxs->rs_rssi_ext0;
        stats->vi_rssi_ext1[index]    = rxs->rs_rssi_ext1;
        stats->vi_rssi_ext2[index]    = rxs->rs_rssi_ext2;
	    stats->vi_rssi[index]         = rxs->rs_rssi;
	    rate                          = rxs->rs_rate & 0x7F;

	    if (IS_SINGLE_STREAM(rate)) { /* Single stream rates */
	        evm_temp1  = (((rxs->evm0 >> 24) & 0xff) + ((rxs->evm0 >> 16) & 0xff) +
	                      ((rxs->evm0 >> 8) & 0xff)  + (rxs->evm0 & 0xff)) >> 2;
	        evm_temp2  = (((rxs->evm1 >> 8) & 0xff)  + (rxs->evm1 & 0xff)) >> 1;
	        if (rxs->rs_flags & HAL_RX_2040) { /* 40MHz mode */
	            stats->vi_evm0[index] = (evm_temp1 + evm_temp2) >> 1;
	        } else {
	            stats->vi_evm0[index] = evm_temp1;
	        }
	        stats->vi_evm1[index]     = 0x80;
	        stats->vi_evm2[index]     = 0x80;
	    } else if (IS_THREE_STREAM(rate)) {  /* Three stream rates */
	        evm_temp1  = (((rxs->evm0 >> 24) & 0xff) +  (rxs->evm0 & 0xff) +
	                      ((rxs->evm1 >> 16) & 0xff) + ((rxs->evm2 >> 8) & 0xff)) >> 2;
	        evm_temp2  = (((rxs->evm3 >> 24) & 0xff)  + (rxs->evm3 & 0xff)) >> 1;

	        if (rxs->rs_flags & HAL_RX_2040) { /* 40MHz mode */
	            stats->vi_evm0[index] = (evm_temp1 + evm_temp2) >> 1;
	        } else {
	            stats->vi_evm0[index] = evm_temp1;
	        }
	        evm_temp1  = (((rxs->evm0 >> 8) & 0xff)  +  (rxs->evm1 & 0xff) +
	                      ((rxs->evm1 >> 24) & 0xff) + ((rxs->evm2 >> 16) & 0xff)) >> 2;
	        evm_temp2  = (((rxs->evm3 >> 8) & 0xff)  + (rxs->evm4 & 0xff)) >> 1;

	        if (rxs->rs_flags & HAL_RX_2040) { /* 40MHz mode */
	            stats->vi_evm1[index] = (evm_temp1 + evm_temp2) >> 1;
	        } else {
	            stats->vi_evm1[index] = evm_temp1;
	        }

	        evm_temp1  = (((rxs->evm0 >> 16) & 0xff) + ((rxs->evm1 >> 8) & 0xff) +
	                      ((rxs->evm2 >> 24) & 0xff) +  (rxs->evm2 & 0xff)) >> 2;
	        evm_temp2  = (((rxs->evm3 >> 16) & 0xff) + ((rxs->evm4 >> 8) & 0xff)) >> 1;

	        if (rxs->rs_flags & HAL_RX_2040) { /* 40MHz mode */
	            stats->vi_evm2[index] = (evm_temp1 + evm_temp2) >> 1;
	        } else {
	            stats->vi_evm2[index] = evm_temp1;
	        }

	    } else { /* Two stream rates */
	    	evm_temp1  = (((rxs->evm0 >> 16) & 0xff) + (rxs->evm0 & 0xff) +
	                      ((rxs->evm1 >> 16) & 0xff) + (rxs->evm1 & 0xff)) >> 2;
	        evm_temp2  = (((rxs->evm2 >> 16) & 0xff) + (rxs->evm2 & 0xff)) >> 1;

	        if (rxs->rs_flags & HAL_RX_2040) { /* 40MHz mode */
	            stats->vi_evm0[index] = (evm_temp1 + evm_temp2) >> 1;
	        } else {
	            stats->vi_evm0[index] = evm_temp1;
	        }

	    	evm_temp1 = (((rxs->evm0 >> 24) & 0xff) + ((rxs->evm0 >> 8) & 0xff) +
	                     ((rxs->evm1 >> 24) & 0xff) + ((rxs->evm1 >> 8) & 0xff)) >> 2;
	        evm_temp2 = (((rxs->evm2 >> 24) & 0xff) + ((rxs->evm2 >> 8) & 0xff)) >> 1;

	        if (rxs->rs_flags & HAL_RX_2040) { /* 40MHz mode */
	            stats->vi_evm1[index] = (evm_temp1 + evm_temp2) >> 1;
	        } else {
	            stats->vi_evm1[index] = evm_temp1;
	        }

	        stats->vi_evm2[index]     = 0x80;
	    }
	    stats->vi_rs_rate[index]        = rxs->rs_rate;
	    ath_hal_getVowStats(sc->sc_ah, &pcounts, AR_REG_VOW_ALL);
	    stats->vi_tx_frame_cnt[index]   = pcounts.tx_frame_count;
	    stats->vi_rx_frame_cnt[index]   = pcounts.rx_frame_count;
	    stats->vi_rx_clr_cnt[index]     = pcounts.rx_clear_count;
	    stats->vi_cycle_cnt[index]      = pcounts.cycle_count;
	    stats->vi_rx_ext_clr_cnt[index] = pcounts.ext_cycle_count;

	    index                 = (index + 1) % 10;
	    stats->vi_stats_index = index;
	    sc->sc_vi_dbg_start   = 1;

    }
}

#endif

#if UNIFIED_SMARTANTENNA
static inline uint32_t ath_smart_ant_rxfeedback(struct ath_softc *sc,  wbuf_t wbuf, struct ath_rx_status *rxs, uint32_t pkts)
{
    struct sa_rx_feedback rx_feedback;
    if (sc->sc_ieee_ops->smart_ant_update_rxfeedback) {

        rx_feedback.rx_rate_mcs = rxs->rs_rate;
        rx_feedback.rx_antenna = rxs->rs_antenna;

        rx_feedback.rx_rssi[0] = ((rxs->rs_rssi_ctl0 | rxs->rs_rssi_ext0 << 8) | INVALID_RSSI_WORD);
        rx_feedback.rx_rssi[1] = ((rxs->rs_rssi_ctl1 | rxs->rs_rssi_ext1 << 8) | INVALID_RSSI_WORD);
        rx_feedback.rx_rssi[2] = ((rxs->rs_rssi_ctl2 | rxs->rs_rssi_ext2 << 8) | INVALID_RSSI_WORD);
        rx_feedback.rx_rssi[3] = INVALID_RSSI_DWORD;

        rx_feedback.rx_evm[0] = rxs->evm0;
        rx_feedback.rx_evm[1] = rxs->evm1;
        rx_feedback.rx_evm[2] = rxs->evm2;
        rx_feedback.rx_evm[3] = rxs->evm3;
        rx_feedback.rx_evm[4] = rxs->evm4;
        rx_feedback.npackets = pkts;

        sc->sc_ieee_ops->smart_ant_update_rxfeedback(sc->sc_ieee, wbuf, (void *)&rx_feedback);
    }

    return 0;
}

#endif
