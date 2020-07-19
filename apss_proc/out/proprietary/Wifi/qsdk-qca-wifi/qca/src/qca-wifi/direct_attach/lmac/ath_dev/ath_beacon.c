/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 * Copyright (c) 2005, Atheros Communications Inc.
 * All Rights Reserved.
 */

/*! \file ath_beacon.c
**  \brief ATH Beacon Processing
**
**  This file contains the implementation of the common beacon support code for
**  the ATH layer, including any tasklets/threads used for beacon support.
**
*/

#include "ath_internal.h"
#include "ath_txseq.h"

#if WLAN_SPECTRAL_ENABLE
#include "spectral.h"
#endif

#include "ratectrl.h"

#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif

#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#endif

#define IEEE80211_MS_TO_TU(x)   (((x) * 1000) / 1024)
#define MIN_TSF_TIMER_TIME  5000 /* 5 msec */

typedef struct _ath_vap_info {
    int if_id;
    HAL_OPMODE opmode;
} ath_vap_info;

typedef enum {
    ATH_ALLOC_BEACON_SLOT,
    ATH_FREE_BEACON_SLOT,
    ATH_ALLOC_FIRST_SLOT /* for sta vap chip in sta mode */
} ath_beacon_slot_op;

#define TU_TO_TSF(_tsf) ((_tsf)<<10)
#define TSF_TO_TU(_h,_l) \
    ((((u_int32_t)(_h)) << 22) | (((u_int32_t)(_l)) >> 10))

#ifdef ATH_SUPPORT_DFS
#define is_dfs_wait(_sc)            ((_sc)->sc_dfs_wait)
#endif

#if 0 //DFSUMAC:defined(ATH_SUPPORT_DFS) && !defined(ATH_DFS_RADAR_DETECTION_ONLY)
static u_int32_t ath_get_phy_err_rate(struct ath_softc *sc);
int get_dfs_hang_war_timeout(struct ath_softc *sc);
#endif
static u_int32_t ath_beacon_config_sta_timers(struct ath_softc *sc, ieee80211_beacon_config_t *conf, int tbtt_timer_period, u_int64_t tsf);
static void ath_beacon_config_ap_timers(struct ath_softc *sc, u_int32_t nexttbtt, u_int32_t intval, 
                                        u_int32_t intval_frac);
static void ath_beacon_config_ibss_timers(struct ath_softc *sc, u_int32_t nexttbtt, u_int32_t intval);
static void ath_beacon_config_sta_mode(struct ath_softc *sc, ath_beacon_config_reason reason, int if_id );
static void ath_beacon_config_ap_mode(struct ath_softc *sc, ath_beacon_config_reason reason, int if_id );
#if ATH_SUPPORT_IBSS
static void ath_beacon_config_ibss_mode(struct ath_softc *sc, ath_beacon_config_reason reason, int if_id );
#endif
static void ath_get_beacon_config(struct ath_softc *sc, int if_id, ieee80211_beacon_config_t *conf);
static void ath_set_num_slots(struct ath_softc *sc);
static void ath_beacon_config_beacon_slot(struct ath_softc *sc, ath_beacon_slot_op config_op, int if_id );
static void ath_beacon_config_tsf_offset(struct ath_softc *sc, int if_id );
static void ath_beacon_config_all_tsf_offset(struct ath_softc *sc );
static int ath_get_all_active_vap_info(struct ath_softc *sc, ath_vap_info *av_info, int exclude_id);
#if UMAC_SUPPORT_WNM
static void
ath_timbcast_setup(struct ath_softc *sc, struct ath_vap *avp, struct ath_buf *bf, u_int32_t smart_ant_bcn_txant);
#endif

/******************************************************************************/
/*!
**  \brief Setup a h/w transmit queue for beacons.
**
**  This function allocates an information structure (HAL_TXQ_INFO) on the stack,
**  sets some specific parameters (zero out channel width min/max, and enable aifs)
**  The info structure does not need to be persistant.
**
**  \param sc Pointer to the SoftC structure for the ATH object
**  \param nbufs Number of buffers to allocate for the transmit queue
**
**  \return Returns the queue index number (priority), or -1 for failure
*/

int
ath_beaconq_setup(struct ath_softc *sc, struct ath_hal *ah)
{
    HAL_TXQ_INFO qi;

    OS_MEMZERO(&qi, sizeof(qi));
    qi.tqi_aifs = 1;
    qi.tqi_cwmin = 0;
    qi.tqi_cwmax = 0;
#ifdef ATH_SUPERG_DYNTURBO
    qi.tqi_qflags = TXQ_FLAG_TXDESCINT_ENABLE;
#endif

    /*
     * For the Tx Beacon Notify feature, we need to enable the interrupt for 
     * Beacon Queue. But the individual beacon buffer still needs to set its 
     * enable completion interrupt in order to get interrupted.
     */
    qi.tqi_qflags = TXQ_FLAG_TXDESCINT_ENABLE;

    if (sc->sc_enhanceddmasupport) {
        qi.tqi_qflags = TXQ_FLAG_TXOKINT_ENABLE | TXQ_FLAG_TXERRINT_ENABLE;
    }

    /* NB: don't enable any interrupts */
    return ath_hal_setuptxqueue(ah, HAL_TX_QUEUE_BEACON, &qi);
}


/******************************************************************************/
/*!
**  \brief Configure parameters for the beacon queue
**
**  This function will modify certain transmit queue properties depending on
**  the operating mode of the station (AP or AdHoc).  Parameters are AIFS
**  settings and channel width min/max
**
**  \param sc Pointer to ATH object ("this" pointer)
**
**  \return zero for failure, or 1 for success
*/

static int
ath_beaconq_config(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    HAL_TXQ_INFO qi;

    ath_hal_gettxqueueprops(ah, sc->sc_bhalq, &qi);
    if (sc->sc_opmode == HAL_M_HOSTAP || sc->sc_opmode == HAL_M_STA)
    {
        /*
         * Always burst out beacon and CAB traffic.
         */
        qi.tqi_aifs = 1;
        qi.tqi_cwmin = 0;
        qi.tqi_cwmax = 0;
    }
    else
    {
        /*
         * Adhoc mode; important thing is to use 2x cwmin.
         */
        qi.tqi_aifs = sc->sc_beacon_qi.tqi_aifs;
        qi.tqi_cwmin = 2*sc->sc_beacon_qi.tqi_cwmin;
        qi.tqi_cwmax = sc->sc_beacon_qi.tqi_cwmax;
    }

    if (!ath_hal_settxqueueprops(ah, sc->sc_bhalq, &qi))
    {
        printk("%s: unable to update h/w beacon queue parameters\n",
               __func__);
        return 0;
    }
    else
    {
        ath_hal_resettxqueue(ah, sc->sc_bhalq); /* push to h/w */
        return 1;
    }
}

/******************************************************************************/
/*!
**  \brief Allocate and setup an initial beacon frame.
**
**  Allocate a beacon state variable for a specific VAP instance created on
**  the ATH interface.  This routine also calculates the beacon "slot" for
**  staggared beacons in the mBSSID case.
**
**  \param sc Pointer to ATH object ("this" pointer).
**  \param if_id Index of the specific VAP of interest.
**
**  \return -EINVAL if there is no function in the upper layer assigned to
**  \return         beacon transmission
**  \return -ENOMEM if no wbuf is available
**  \return   0     for success
*/

int
ath_beacon_alloc(struct ath_softc *sc, int if_id)
{
    struct ath_vap *avp;
    struct ath_buf *bf;
    wbuf_t wbuf;
    struct ieee80211com* ic = (struct ieee80211com*)(sc->sc_ieee);

    /*
    ** Code Begins
    */

    avp = sc->sc_vaps[if_id];
    ASSERT(avp);
    ATH_BEACON_ALLOC_LOCK(ic);

    /* Allocate a beacon descriptor if we haven't done so. */
    if(!avp->av_bcbuf)
    {
        /*
         * Allocate beacon state for hostap/ibss.  We know
         * a buffer is available.
         */

        avp->av_bcbuf = TAILQ_FIRST(&sc->sc_bbuf);
        TAILQ_REMOVE(&sc->sc_bbuf, avp->av_bcbuf, bf_list);

        /* set up this buffer */
        ATH_TXBUF_RESET(avp->av_bcbuf, sc->sc_num_txmaps);

    }

    /*
     * release the previous beacon frame , if it already exists.
     */
    bf = avp->av_bcbuf;
    if (bf->bf_mpdu != NULL)
    {
        ieee80211_tx_status_t tx_status;

        wbuf = (wbuf_t)bf->bf_mpdu;
        wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                          OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        tx_status.flags = 0;
        tx_status.retries = 0;
#ifdef  ATH_SUPPORT_TxBF
        tx_status.txbf_status = 0;
#endif
        sc->sc_ieee_ops->tx_complete(wbuf, &tx_status, 0);
        bf->bf_mpdu = NULL;
    }

    /*
     * NB: the beacon data buffer must be 32-bit aligned;
    * we assume the wbuf routines will return us something
     * with this alignment (perhaps should assert).
     */
    if (!sc->sc_ieee_ops->get_beacon)
    {
        /* Protocol layer doesn't support beacon generation for host driver */
        ATH_BEACON_ALLOC_UNLOCK(ic);
        return -EINVAL;
    }
    wbuf = sc->sc_ieee_ops->get_beacon(sc->sc_ieee, if_id, &avp->av_boff, &avp->av_btxctl);
    if (wbuf == NULL)
    {
        DPRINTF(sc, ATH_DEBUG_BEACON, "%s: cannot get wbuf\n",
                __func__);
#ifdef QCA_SUPPORT_CP_STATS
        pdev_cp_stats_be_nobuf_inc(sc->sc_pdev, 1);
#else
        sc->sc_stats.ast_be_nobuf++;
#endif
        ATH_BEACON_ALLOC_UNLOCK(ic);
        return -ENOMEM;
    }

    bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                                      OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
    bf->bf_mpdu = wbuf;
    ATH_BEACON_ALLOC_UNLOCK(ic);

    /*
     * if min rate is set or user is trying to modify
     * beacon rate then get the corresponsding rate code
     * and save it in the vap.
     */
    if (avp->av_modify_bcn_rate) {
        avp->av_beacon_rate_code = ath_rate_findrc((ath_dev_t) sc, avp->av_modify_bcn_rate);
    } else if(avp->av_btxctl.min_rate) {
        avp->av_beacon_rate_code = ath_rate_findrc((ath_dev_t) sc, avp->av_btxctl.min_rate); /* get the rate code */
    }

    return 0;
}

/******************************************************************************/
/*!
**  \brief Setup the beacon frame for transmit.
**
**  Associates the beacon frame buffer with a transmit descriptor.  Will set
**  up all required antenna switch parameters, rate codes, and channel flags.
**  Beacons are always sent out at the lowest rate, and are not retried.
**
**  \param sc Pointer to ATH object ("this" pointer)
**  \param avp Pointer to VAP object (ieee802.11 layer object)for the beacon
**  \param bf Pointer to ATH buffer containing the beacon frame.
**  \return N/A
*/
#if UNIFIED_SMARTANTENNA
static void
ath_beacon_setup(struct ath_softc *sc, struct ath_vap *avp, struct ath_buf *bf, u_int32_t smart_ant_bcn_txant)
#else
static void
ath_beacon_setup(struct ath_softc *sc, struct ath_vap *avp, struct ath_buf *bf)
#endif
{
    wbuf_t wbuf = (wbuf_t)bf->bf_mpdu;
    struct ath_hal *ah = sc->sc_ah;
    struct ath_desc *ds;
    int flags;
    const HAL_RATE_TABLE *rt;
    u_int8_t rix, rate;
    int ctsrate = 0;
    int ctsduration = 0;
    HAL_11N_RATE_SERIES  series[4];
#if UNIFIED_SMARTANTENNA
    u_int32_t antenna_array[4]= {0,0,0,0}; /* initilize to zero */
#endif        

    /*
    ** Code Begins
    */

    DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s: m %pK len %u\n",
            __func__, wbuf, wbuf_get_pktlen(wbuf));

    /* setup descriptors */
    ds = bf->bf_desc;

    flags = HAL_TXDESC_NOACK;
#ifdef ATH_SUPERG_DYNTURBO
    if (sc->sc_dturbo_switch)
        flags |= HAL_TXDESC_INTREQ;
#endif

    if (atomic_read(&sc->sc_has_tx_bcn_notify)) {
        /* Let the completion of this Beacon buffer Tx triggers an interrupt */
        /* NOTE: This per descriptor interrrupt is only available in Non-Osprey hw. */
        flags |= HAL_TXDESC_INTREQ;
    }

    if (sc->sc_opmode == HAL_M_IBSS && 
        sc->sc_hasveol 
#ifdef ATH_SUPPORT_DFS
        && !is_dfs_wait(sc)
#endif        
        ) 
    {
        ath_hal_setdesclink(ah, ds, bf->bf_daddr); /* self-linked */
        flags |= HAL_TXDESC_VEOL;
    }
    else
    {
        ath_hal_setdesclink(ah, ds, 0);
    }

    /*
     * Calculate rate code.
     */
    rix = sc->sc_minrateix;
    rt = sc->sc_currates;

    /*
     * if the vap requests a specific rate for beacon then
     * use it insted of minrateix. 
     */
    if (avp->av_beacon_rate_code) {
        rate = avp->av_beacon_rate_code;
    } else {
       rate = rt->info[rix].rate_code;
    }
    if (avp->av_btxctl.shortPreamble)
        rate |= rt->info[rix].shortPreamble;

#ifndef REMOVE_PKT_LOG
    bf->bf_vdata = wbuf_header(wbuf);
#endif

    /* NB: beacon's BufLen must be a multiple of 4 bytes */
    bf->bf_buf_len[0] = roundup(wbuf_get_pktlen(wbuf), 4);

    ath_hal_set11n_txdesc(ah, ds
                          , wbuf_get_pktlen(wbuf) + IEEE80211_CRC_LEN /* frame length */
                          , HAL_PKT_TYPE_BEACON                 /* Atheros packet type */
                          , avp->av_btxctl.txpower              /* txpower XXX */
                          , HAL_TXKEYIX_INVALID                 /* no encryption */
                          , HAL_KEY_TYPE_CLEAR                  /* no encryption */
                          , flags                               /* no ack, veol for beacons */
                          );

    ath_hal_filltxdesc(ah, ds
                       , bf->bf_buf_addr           /* buffer address */
                       , bf->bf_buf_len            /* buffer length */
                       , 0                                      /* descriptor id */
                       , sc->sc_bhalq                           /* QCU number */
                       , HAL_KEY_TYPE_CLEAR                     /* key type */
                       , AH_TRUE                                /* first segment */
                       , AH_TRUE                                /* last segment */
                       , ds                                     /* first descriptor */
                       );

    OS_MEMZERO(series, sizeof(HAL_11N_RATE_SERIES) * 4);
    series[0].Tries = 1;
    series[0].Rate = rate;
    series[0].rate_index = rix;
    series[0].tx_power_cap = IS_CHAN_2GHZ(&sc->sc_curchan) ?
        sc->sc_config.txpowlimit2G : sc->sc_config.txpowlimit5G;
    if (avp->av_txpow_mgt_frm[IEEE80211_FC0_SUBTYPE_BEACON >> IEEE80211_FC0_SUBTYPE_SHIFT] != 0xFF){
        series[0].tx_power_cap = ( series[0].tx_power_cap <= avp->av_txpow_mgt_frm[IEEE80211_FC0_SUBTYPE_BEACON >> IEEE80211_FC0_SUBTYPE_SHIFT])?
            series[0].tx_power_cap  : avp->av_txpow_mgt_frm[IEEE80211_FC0_SUBTYPE_BEACON >> IEEE80211_FC0_SUBTYPE_SHIFT];
    }

    if (avp->av_txpow_frm[IEEE80211_FC0_TYPE_MGT][IEEE80211_FC0_SUBTYPE_BEACON >> IEEE80211_FC0_SUBTYPE_SHIFT] != 0xFF){
        series[0].tx_power_cap = ( series[0].tx_power_cap <= avp->av_txpow_frm[IEEE80211_FC0_TYPE_MGT][IEEE80211_FC0_SUBTYPE_BEACON >> IEEE80211_FC0_SUBTYPE_SHIFT])?
            series[0].tx_power_cap  : avp->av_txpow_frm[IEEE80211_FC0_TYPE_MGT][IEEE80211_FC0_SUBTYPE_BEACON >> IEEE80211_FC0_SUBTYPE_SHIFT];
    }

#if WLAN_SUPPORT_GREEN_AP
    series[0].ch_sel = ath_txchainmask_reduction(sc,
            ath_green_ap_is_powersave_on(sc) ? ATH_CHAINMASK_ONE_CHAIN :sc->sc_tx_chainmask,
            rate);
#else
    series[0].ch_sel = ath_txchainmask_reduction(sc, sc->sc_tx_chainmask, rate);
#endif

    series[0].RateFlags = 0;

#if UNIFIED_SMARTANTENNA
    antenna_array[0] = smart_ant_bcn_txant;
    ath_hal_set11n_ratescenario(ah, ds, ds, 0, ctsrate, ctsduration, series, 4, 0, smart_ant_bcn_txant, antenna_array);
#else
    ath_hal_set11n_ratescenario(ah, ds, ds, 0, ctsrate, ctsduration, series, 4, 0, SMARTANT_INVALID);
#endif        


    /* NB: The desc swap function becomes void, 
     * if descriptor swapping is not enabled
     */
    ath_desc_swap(ds);
}

/******************************************************************************/
/*!
**  \brief Generate beacon frame and queue cab data for a vap.
**
**  Updates the contents of the beacon frame.  It is assumed that the buffer for
**  the beacon frame has been allocated in the ATH object, and simply needs to
**  be filled for this cycle.  Also, any CAB (crap after beacon?) traffic will
**  be added to the beacon frame at this point.
**
**  \param sc Pointer to ATH object ("this" pointer)
**  \param if_id Index to VAP object that is sending the beacon
**  \param needmark Pointer to integer.  Only used in dynturbo mode, and deprecated at that!
**  \return a pointer to an allocated ath buffer containing the beacon frame,
**  \return or NULL if not successful
*/
/*
 *
 */
static struct ath_buf *
ath_beacon_generate(struct ath_softc *sc, int if_id, int *needmark)
{
    struct ath_hal *ah = sc->sc_ah;
    struct ath_buf *bf;
    struct ath_vap *avp;
    wbuf_t wbuf;
    struct ieee80211com* ic = (struct ieee80211com*)(sc->sc_ieee);
#if UMAC_SUPPORT_WNM
    struct ath_buf *bf_tim_highrate = NULL,  *bf_tim_lowrate = NULL;
    wbuf_t wbuf_tim_highrate = NULL, wbuf_tim_lowrate = NULL;
    u_int8_t *fms_counters_off;
    u_int8_t fms_counter, fms_curr_count;
    int nfms_counters, i;
    u_int32_t nfmsq_mask = 0;
#endif /* UMAC_SUPPORT_WNM */
    int ncabq;
    u_int32_t smart_ant_bcn_txant = 0;
    avp = sc->sc_vaps[if_id];
    ASSERT(avp);

    atomic_inc(&avp->av_beacon_cabq_use_cnt);
    if (atomic_read(&avp->av_stop_beacon)) {
        atomic_dec(&avp->av_beacon_cabq_use_cnt);
        return NULL;
    }

#if ATH_SUPPORT_AP_WDS_COMBO
    if (avp->av_config.av_no_beacon) {
        atomic_dec(&avp->av_beacon_cabq_use_cnt);
        return NULL;
    }
#endif

    if (avp->av_bcbuf == NULL)
    {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: avp=%pK av_bcbuf=%pK\n",
                __func__, avp, avp->av_bcbuf);
        atomic_dec(&avp->av_beacon_cabq_use_cnt);
        return NULL;
    }
    bf = avp->av_bcbuf;
    wbuf = (wbuf_t)bf->bf_mpdu;

#ifdef ATH_SUPERG_DYNTURBO
    /*
     * If we are using dynamic turbo, update the
     * capability info and arrange for a mode change
     * if needed.
     */
    if (sc->sc_dturbo)
    {
        u_int8_t dtim;
        dtim = ((avp->av_boff.bo_tim[2] == 1) || (avp->av_boff.bo_tim[3] == 1));
    }
#endif
    /*
     * Update dynamic beacon contents.  If this returns
     * non-zero then we need to remap the memory because
     * the beacon frame changed size (probably because
     * of the TIM bitmap).
     */
    if (sc->sc_nslots == 1 && sc->sc_stagbeacons) {  /* For single VAP case */
        ncabq = (avp->av_mcastq.axq_depth || sc->sc_cabq->axq_depth);
    } else {
        ncabq = avp->av_mcastq.axq_depth;
    }
#if UMAC_SUPPORT_WNM
    for (i = 0; i < ATH_MAX_FMS_QUEUES; i++) {
        if(avp->av_fmsq[i].axq_depth) {
            nfmsq_mask |= (1 << i); 
        } 
    }
#endif

    if (!sc->sc_ieee_ops->update_beacon)
    {
        /*
         * XXX: if protocol layer doesn't support update beacon at run-time,
         * we have to free the old beacon and allocate a new one.
         */
        if (sc->sc_ieee_ops->get_beacon)
        {
            ieee80211_tx_status_t tx_status;

            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            tx_status.flags = 0;
            tx_status.retries = 0;
#ifdef  ATH_SUPPORT_TxBF
            tx_status.txbf_status = 0;
#endif
            sc->sc_ieee_ops->tx_complete(wbuf, &tx_status, 0);

            wbuf = sc->sc_ieee_ops->get_beacon(sc->sc_ieee, if_id, &avp->av_boff, &avp->av_btxctl);
            if (wbuf == NULL) {
                atomic_dec(&avp->av_beacon_cabq_use_cnt);
                return NULL;
            }

            bf->bf_mpdu = wbuf;
            bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        }
    }
    else
    {
        int retVal = 0;
        ATH_BEACON_ALLOC_LOCK(ic);
#if UMAC_SUPPORT_WNM
        retVal = sc->sc_ieee_ops->update_beacon(sc->sc_ieee, if_id, &avp->av_boff, wbuf, ncabq, nfmsq_mask, &smart_ant_bcn_txant);
#else
        retVal = sc->sc_ieee_ops->update_beacon(sc->sc_ieee, if_id, &avp->av_boff, wbuf, ncabq, &smart_ant_bcn_txant);
#endif
        ATH_BEACON_ALLOC_UNLOCK(ic);
        if (retVal == 1) {
            ath_beacon_seqno_set(avp->av_btxctl.an, wbuf);
            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        } else if (retVal == 0) {
            ath_beacon_seqno_set(avp->av_btxctl.an, wbuf);
            OS_SYNC_SINGLE(sc->sc_osdev,
                       bf->bf_buf_addr[0], wbuf_get_pktlen(wbuf), BUS_DMA_TODEVICE,
                       OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        } else if (retVal < 0) {
            /* ieee layer does not want to send beacon */
            atomic_dec(&avp->av_beacon_cabq_use_cnt);
            return NULL;
        }
    }

 #ifdef ATH_SUPPORT_DFS
   if (is_dfs_wait(sc)) {
        atomic_dec(&avp->av_beacon_cabq_use_cnt);
        return NULL;        
    }
#endif
#if UMAC_SUPPORT_WNM
     if(sc->sc_ieee_ops->wnm_fms_enabled(sc->sc_ieee, if_id)) {
        /*
         * It it is a DTIM and the FMS counters are added,
         * we need to check the count value of the counters and move frames
         * from  av_fmsq[] to av_mcastq if needed.
         */
        if (avp->av_boff.bo_fms_desc) {
            nfms_counters = (int)avp->av_boff.bo_fms_desc[2]; /* 3rd byte */

            if ((avp->av_boff.bo_tim[4] & 1) && (nfms_counters != 0)) {
                /* counters start from 4th byte */
                fms_counters_off = avp->av_boff.bo_fms_desc + 3;

                for (i = 0; i < nfms_counters; i++) {
                    fms_counter = fms_counters_off[i];
                    fms_curr_count = (fms_counter & FMS_CURR_COUNT_MASK) >> 3;

                    if (fms_curr_count == 0) {
                        struct ath_txq *fmsq = &avp->av_fmsq[i];
                        struct ath_txq *mcastq = &avp->av_mcastq;
                        struct ath_buf *bffms;

                        ATH_TXQ_LOCK(fmsq);
                        ATH_TXQ_LOCK(mcastq);
                        /*
                         * move all the buffers from av_fmsq[i] to av_mcastq.
                         */
                        bffms = TAILQ_FIRST(&fmsq->axq_q);
                        if (bffms != NULL) {
                            DPRINTF(sc, ATH_DEBUG_WNM_FMS, 
                                    "%s: Moving frames from fmsq_id %d (counter_id %d) to mcastq\n", 
                                    __FUNCTION__, i, (fms_counter & 0x7));
                            ATH_TXQ_MOVE_MCASTQ(fmsq, mcastq);
                        }
                        ATH_TXQ_UNLOCK(mcastq);
                        ATH_TXQ_UNLOCK(fmsq);
                    }
                } /* for loop */
            }
        }    
    }
#endif /* UMAC_SUPPORT_WNM */

        /*
         * if the CABQ traffic from previous DTIM is pending and the current
         *  beacon is also a DTIM.
         *  1) if there is only one vap let the cab traffic continue.
         *  2) if there are more than one vap and we are using staggered
         *     beacons, then drain the cabq by dropping all the frames in
         *     the cabq so that the current vaps cab traffic can be scheduled.
         */
    if (ncabq && (avp->av_boff.bo_tim[4] & 1) && sc->sc_cabq->axq_depth)
    {
        if (sc->sc_nbcnvaps > 1 && sc->sc_stagbeacons)
        {
            if (ath_hal_stoptxdma_indvq(sc->sc_ah, sc->sc_cabq->axq_qnum, 0)) {
                ath_tx_draintxq(sc, sc->sc_cabq, AH_FALSE);
                DPRINTF(sc, ATH_DEBUG_BEACON,
                        "%s: flush previous cabq traffic\n", __func__);
            } else {
                DPRINTF(sc,ATH_DEBUG_BEACON,"%s[%d]  Unable to stop CABQ. Now Stopping all Queues \n", __func__, __LINE__);
              ath_draintxq(sc, AH_FALSE, 0); /*Failed to stop cabq. Now Big Hammer: Stop all queue */
            }
                
        }
    }

        /*
         * Construct tx descriptor.
         */
#if UNIFIED_SMARTANTENNA         
        ath_beacon_setup(sc, avp, bf, smart_ant_bcn_txant);
#else        
        ath_beacon_setup(sc, avp, bf);
#endif
                
#if UMAC_SUPPORT_WNM
     if(sc->sc_ieee_ops->timbcast_enabled(sc->sc_ieee, if_id)) {
         if (sc->sc_ieee_ops->timbcast_cansend(sc->sc_ieee, if_id) > 0) {
             if (sc->sc_ieee_ops->timbcast_highrate(sc->sc_ieee, if_id) > 0) {
                 bf_tim_highrate = avp->av_tbuf_highrate;
                 wbuf_tim_highrate = (wbuf_t)bf_tim_highrate->bf_mpdu;
                 if (sc->sc_ieee_ops->update_timbcast(sc->sc_ieee, if_id, wbuf_tim_highrate) == 0) {
                     OS_SYNC_SINGLE(sc->sc_osdev,
                            bf_tim_highrate->bf_buf_addr[0], wbuf_get_pktlen(wbuf_tim_highrate), BUS_DMA_TODEVICE,
                             OS_GET_DMA_MEM_CONTEXT(bf_tim_highrate, bf_dmacontext));
                     ath_timbcast_setup(sc, avp, bf_tim_highrate, smart_ant_bcn_txant);
                 }
             }
             if (sc->sc_ieee_ops->timbcast_lowrate(sc->sc_ieee, if_id) > 0) {
                 bf_tim_lowrate = avp->av_tbuf_lowrate;
                 wbuf_tim_lowrate = (wbuf_t)bf_tim_lowrate->bf_mpdu;
                 if (sc->sc_ieee_ops->update_timbcast(sc->sc_ieee, if_id, wbuf_tim_lowrate) == 0) {
                     OS_SYNC_SINGLE(sc->sc_osdev,
                        bf_tim_lowrate->bf_buf_addr[0], wbuf_get_pktlen(wbuf_tim_lowrate), BUS_DMA_TODEVICE,
                        OS_GET_DMA_MEM_CONTEXT(bf_tim_lowrate, bf_dmacontext));
                     ath_timbcast_setup(sc, avp, bf_tim_lowrate, smart_ant_bcn_txant);
                     if (bf_tim_highrate && wbuf_tim_highrate) {
                         ath_hal_setdesclink(sc->sc_ah, bf_tim_highrate->bf_desc, bf_tim_lowrate->bf_daddr);
                         OS_SYNC_SINGLE(sc->sc_osdev,
                         bf_tim_highrate->bf_buf_addr[0], wbuf_get_pktlen(wbuf_tim_highrate), BUS_DMA_TODEVICE,
                         OS_GET_DMA_MEM_CONTEXT(bf_tim_highrate, bf_dmacontext));
                     }
                 }
             }
             if (bf_tim_highrate) {
                 ath_hal_setdesclink(sc->sc_ah, bf->bf_desc, bf_tim_highrate->bf_daddr);
                 OS_SYNC_SINGLE(sc->sc_osdev,
                     bf->bf_buf_addr[0], wbuf_get_pktlen(wbuf), BUS_DMA_TODEVICE,
                     OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
             } else if (bf_tim_lowrate) {
                 ath_hal_setdesclink(sc->sc_ah, bf->bf_desc, bf_tim_lowrate->bf_daddr);
                 OS_SYNC_SINGLE(sc->sc_osdev,
                     bf->bf_buf_addr[0], wbuf_get_pktlen(wbuf), BUS_DMA_TODEVICE,
                     OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
             }
         }
    }

#endif
    /*
     * Enable the CAB queue before the beacon queue to
     * insure cab frames are triggered by this beacon.
     */
    if (avp->av_boff.bo_tim[4] & 1)
    {   /* NB: only at DTIM */
        struct ath_txq *cabq = sc->sc_cabq;
        struct ath_buf *bfmcast;

        /*
         * Move everything from the vap's mcast queue
         * to the hardware cab queue.
         * XXX MORE_DATA bit?
         */
        ATH_TXQ_LOCK(&avp->av_mcastq);
        ATH_TXQ_LOCK(cabq);
        
        if (sc->sc_enhanceddmasupport)
        {
            /*
             * move all the buffers from av_mcastq to
             * the CAB queue.
             */
            bfmcast = TAILQ_FIRST(&avp->av_mcastq.axq_q);
            if (bfmcast)
            {
                if (cabq->axq_depth < HAL_TXFIFO_DEPTH)
                {
                    if (sc->sc_nvaps > 1 && !sc->sc_stagbeacons) {
                        struct ath_txq *burstbcn_cabq = &sc->sc_burstbcn_cabq;
                        struct ath_buf *lbf;

#define DESC2PA(_sc, _va)       \
                (dma_addr_t)(((caddr_t)(_va) - (caddr_t)((_sc)->sc_txdma.dd_desc)) + \
                 (_sc)->sc_txdma.dd_desc_paddr)
                 
                        if (!TAILQ_EMPTY(&burstbcn_cabq->axq_q)) {
                            if (!burstbcn_cabq->axq_link) {
                                printk("Oops! burstbcn_cabq not empty but axq_link is NULL?\n");
                                lbf = TAILQ_LAST(&burstbcn_cabq->axq_q, ath_bufhead_s);
                                burstbcn_cabq->axq_link = lbf->bf_desc;
                            }

                            ath_hal_setdesclink(ah, burstbcn_cabq->axq_link, bfmcast->bf_daddr);
                            OS_SYNC_SINGLE(sc->sc_osdev, (dma_addr_t)(DESC2PA(sc, burstbcn_cabq->axq_link)),
                                    sc->sc_txdesclen, BUS_DMA_TODEVICE, NULL);
                            DPRINTF(sc, ATH_DEBUG_BEACON_PROC, " %s: link(%pK)=%llx (%pK)\n", __func__,
                                    burstbcn_cabq->axq_link, ito64(bfmcast->bf_daddr), bfmcast->bf_desc);
                        }

                        lbf = TAILQ_LAST(&avp->av_mcastq.axq_q, ath_bufhead_s);
                        ASSERT(avp->av_mcastq.axq_link == lbf->bf_desc);

                        sc->sc_burstbcn_cabq.axq_totalqueued += avp->av_mcastq.axq_totalqueued;
                        TAILQ_CONCAT(&burstbcn_cabq->axq_q, &avp->av_mcastq.axq_q, bf_list);
                        burstbcn_cabq->axq_link = avp->av_mcastq.axq_link;
                        avp->av_mcastq.axq_depth=0;
                        avp->av_mcastq.axq_totalqueued = 0;
                        avp->av_mcastq.axq_linkbuf = 0;
                        avp->av_mcastq.axq_link = NULL;
#undef DESC2PA
                    }
                    else {
                        ATH_EDMA_TXQ_MOVE_MCASTQ(&avp->av_mcastq, cabq);
                        /* Write the tx descriptor address into the hw fifo */
                        ASSERT(bfmcast->bf_daddr != 0);
                        ath_hal_puttxbuf(ah, cabq->axq_qnum, bfmcast->bf_daddr);
                    }
                }
            }
        }
        else if ((bfmcast = TAILQ_FIRST(&avp->av_mcastq.axq_q)) != NULL)
        {
            /* link the descriptors */
            if (cabq->axq_link == NULL)
            {
                ath_hal_puttxbuf(ah, cabq->axq_qnum, bfmcast->bf_daddr);
            }
            else
            {
#ifdef AH_NEED_DESC_SWAP
                *cabq->axq_link = cpu_to_le32(bfmcast->bf_daddr);
#else
                *cabq->axq_link = bfmcast->bf_daddr;
#endif
            }
            /* append the private vap mcast list to  the cabq */
            ATH_TXQ_MOVE_MCASTQ(&avp->av_mcastq, cabq);
        }
#ifdef ATH_ADDITIONAL_STATS
        sc->sc_stats.ast_txq_packets[cabq->axq_qnum]++;
#endif
        /* NB: gated by beacon so safe to start here */
        if (!TAILQ_EMPTY(&(cabq->axq_q)))
        {
            ath_hal_txstart(ah, cabq->axq_qnum);
        }
        ATH_TXQ_UNLOCK(cabq);
        ATH_TXQ_UNLOCK(&avp->av_mcastq);
    }

    atomic_dec(&avp->av_beacon_cabq_use_cnt);

    return bf;
}

#if ATH_SUPPORT_IBSS
/******************************************************************************/
/*!
**  \brief Startup beacon transmission for adhoc mode
**
** Startup beacon transmission for adhoc mode when they are sent entirely
** by the hardware using the self-linked descriptor + veol trick.
**
**  \param sc Pointer to ATH object. "This" pointer.
**  \param if_id Integer containing index of VAP interface to start the beacon on.
**
**  \return N/A
*/

static void
ath_beacon_start_adhoc(struct ath_softc *sc, int if_id)
{
    struct ath_hal *ah = sc->sc_ah;
    struct ath_buf *bf;
    struct ath_vap *avp;

    /*
    ** Code Begins
    */
#if UNIFIED_SMARTANTENNA
    u_int32_t smart_ant_bcn_txant = SMARTANT_INVALID;
#endif

    avp = sc->sc_vaps[if_id];
    ASSERT(avp);
    
    if (!avp || avp->av_bcbuf == NULL)
    {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s: avp=%pK av_bcbuf=%pK\n",
                __func__, avp, avp != NULL ? avp->av_bcbuf : NULL);
        return;
    }
    bf = avp->av_bcbuf;

    /* XXX: We don't do ATIM, so we don't need to update beacon contents */
#if 0
    wbuf_t wbuf;
    wbuf = (wbuf_t)bf->bf_mpdu;
    /*
     * Update dynamic beacon contents.  If this returns
     * non-zero then we need to remap the memory because
     * the beacon frame changed size (probably because
     * of the TIM bitmap).
     */
    /*
     * XXX: This function should be called from ath_beacon_tasklet,
     * which is in ISR context, thus use the locked version.
     * Better to have an assertion to verify that.
     */
    if (sc->sc_ieee_ops->update_beacon &&
        sc->sc_ieee_ops->update_beacon(ni, &avp->av_boff, wbuf, 0) == 0)
    {
        wbuf_unmap_single(sc->sc_osdev, wbuf,
                          BUS_DMA_TODEVICE,
                          OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        bf->bf_buf_addr = wbuf_map_single(sc->sc_osdev, wbuf,
                                          BUS_DMA_TO_DEVICE,
                                          OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
    }
#endif

    /*
     * Construct tx descriptor.
     */
#if UNIFIED_SMARTANTENNA         
        ath_beacon_setup(sc, avp, bf, smart_ant_bcn_txant);
#else        
        ath_beacon_setup(sc, avp, bf);
#endif

    /* NB: caller is known to have already stopped tx dma */
    ath_hal_puttxbuf(ah, sc->sc_bhalq, bf->bf_daddr);
    ath_hal_txstart(ah, sc->sc_bhalq);
    DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s: TXDP%u = %llx (%pK)\n", __func__,
            sc->sc_bhalq, ito64(bf->bf_daddr), bf->bf_desc);
}
#endif

/******************************************************************************/
/*!
**  \brief Reclaim beacon resources and return buffer to the pool.
**
**  Checks the VAP to put the beacon frame buffer back to the ATH object
**  queue, and de-allocates any wbuf frames that were sent as CAB traffic.
**
**  \param sc Pointer to ATH object, "this" pointer.
**  \param avp Pointer to VAP object that sent the beacon
**
**  \return N/A
*/

void
ath_beacon_return(struct ath_softc *sc, struct ath_vap *avp)
{
    if (avp->av_bcbuf != NULL)
    {
        struct ath_buf *bf;

        bf = avp->av_bcbuf;
        if (bf->bf_mpdu != NULL)
        {
            wbuf_t wbuf = (wbuf_t)bf->bf_mpdu;
            ieee80211_tx_status_t tx_status;

            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            tx_status.flags = 0;
            tx_status.retries = 0;
#ifdef  ATH_SUPPORT_TxBF
            tx_status.txbf_status = 0;
#endif
            sc->sc_ieee_ops->tx_complete(wbuf, &tx_status, 0);
            bf->bf_mpdu = NULL;
        }
        TAILQ_INSERT_TAIL(&sc->sc_bbuf, bf, bf_list);

        avp->av_bcbuf = NULL;
    }
}

/******************************************************************************/
/*!
**  \brief Reclaim beacon resources and return buffer to the pool.
**
**  This function will free any wbuf frames that are still attached to the
**  beacon buffers in the ATH object.  Note that this does not de-allocate
**  any wbuf objects that are in the transmit queue and have not yet returned
**  to the ATH object.
**
**  \param sc Pointer to ATH object, "this" pointer
**
**  \return N/A
*/

void
ath_beacon_free(struct ath_softc *sc)
{
    struct ath_buf *bf;

    TAILQ_FOREACH(bf, &sc->sc_bbuf, bf_list) {
        if (bf->bf_mpdu != NULL)
        {
            wbuf_t wbuf = (wbuf_t)bf->bf_mpdu;
            ieee80211_tx_status_t tx_status;

            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            tx_status.flags = 0;
            tx_status.retries = 0;
#ifdef  ATH_SUPPORT_TxBF
            tx_status.txbf_status = 0;
#endif
            sc->sc_ieee_ops->tx_complete(wbuf, &tx_status, 0);
            bf->bf_mpdu = NULL;
        }
    }
}

#if 0 //DFSUMAC:defined(ATH_SUPPORT_DFS) && !defined(ATH_DFS_RADAR_DETECTION_ONLY)
static int
ath_handle_dfs_bb_hang(struct ath_softc *sc)
{
    if (sc->sc_dfs_hang.hang_war_activated) {
        sc->sc_bmisscount=0;
        printk("ALREADY ACTIVATED\n");
        return 0;
    }

    if (sc->sc_curchan.channel_flags & CHANNEL_HT20) {
        /*=========DFS HT20 HANG========*/
        /* moved marking PHY inactive to HAL reset */
        sc->sc_dfs_hang.hang_war_ht20count++;
        return 0;
    } else {
        /*=========DFS HT40 HANG========*/
        sc->sc_dfs_hang.hang_war_ht40count++;
        sc->sc_dfs_hang.hang_war_activated = 1;
        OS_CANCEL_TIMER(&sc->sc_dfs_hang.hang_war_timer);
                        sc->sc_dfs_hang.hang_war_timeout =
        get_dfs_hang_war_timeout(sc);
        OS_SET_TIMER(&sc->sc_dfs_hang.hang_war_timer,
                     sc->sc_dfs_hang.hang_war_timeout);
        /* Switch to static20 mode, clear 0x981C before
         * channel change
         */
        /* moved marking PHY inactive to HAL reset */
        sc->sc_ieee_ops->ath_net80211_switch_mode_static20(sc->sc_ieee);
        sc->sc_bmisscount=0;
        return 1;
    } /*End HT40 WAR*/
}
#endif

/*
 * Determines if the device currently has a BB or MAC hang.
 */
int
ath_hw_hang_check(struct ath_softc *sc)
{
    sc->sc_hang_check = AH_FALSE;

    /* Do we have a BB hang?
     */
    if (ATH_BB_HANG_WAR_REQ(sc)) {
        if (AH_TRUE == ath_hal_is_bb_hung(sc->sc_ah)) {
#if 0 //DFSUMAC:defined(ATH_SUPPORT_DFS) && !defined(ATH_DFS_RADAR_DETECTION_ONLY)
            /* Found a DFS related BB hang */
            if (ATH_DFS_BB_HANG_WAR_REQ(sc) && !ath_get_phy_err_rate(sc)) {
                if (ath_handle_dfs_bb_hang(sc))
                    return 0;
            }
#endif
            /* Found a generic BB hang */
            if (ATH_BB_GENERIC_HANG_WAR_REQ(sc)) {
                ATH_BB_GENERIC_HANG(sc);
                return 1;
            }
        }
    }

    /* Do we have a MAC hang?
     */
    if (ATH_MAC_HANG_WAR_REQ(sc)) {
        if (AH_TRUE == ath_hal_is_mac_hung(sc->sc_ah)) {
            ATH_MAC_GENERIC_HANG(sc);
            return 1;
        }
    }

    return 0; 
}

/******************************************************************************/
/*!
**  \brief Task for Sending Beacons
**
** Transmit one or more beacon frames at SWBA.  Dynamic updates to the frame
** contents are done as needed and the slot time is also adjusted based on
** current state.
**
** \warning This task is not scheduled, it's called in ISR context.
**
**  \param sc Pointer to ATH object ("this" pointer).
**  \param needmark Pointer to integer value indicating that the beacon miss
**                  threshold exceeded
**  \return Describe return value, or N/A for void
*/

void
ath_beacon_tasklet(struct ath_softc *sc, int *needmark)
{
    
    struct ath_hal *ah = sc->sc_ah;
    struct ath_buf *bf=NULL;
    int slot, if_id;
    u_int32_t bfaddr = 0;
    u_int32_t rx_clear = 0, rx_frame = 0, tx_frame = 0;
    u_int32_t show_cycles = 0;
    u_int32_t bc = 0; /* beacon count */

#ifndef ATH_SUPPORT_P2P
    /* While scanning/in off channel do not send beacons */
    if (sc->sc_scanning) {
        return;
    }
#endif

#if 0 //DFS moved to UMAC :DFSUMAC:defined(ATH_SUPPORT_DFS) && !defined(ATH_DFS_RADAR_DETECTION_ONLY)
    /* Make sure no beacons go out during the DFS wait period (CAC timer)*/
    if (sc->sc_dfs && sc->sc_dfs->sc_dfswait) {
       return;
    }
#endif


    /*
     * Check if the previous beacon has gone out.  If
     * not don't try to post another, skip this period
     * and wait for the next.  Missed beacons indicate
     * a problem and should not occur.  If we miss too
     * many consecutive beacons reset the device.
     */
    if (ath_hal_numtxpending(ah, sc->sc_bhalq) != 0)
    {

		/* Fix for EV 108445 - Continuous beacon stuck */
		/* check for abnormal value for NAV register and reset if required */
		(void) ath_hal_reset_nav(ah);

        show_cycles = ath_hal_getMibCycleCountsPct(ah, 
                      &rx_clear, &rx_frame, &tx_frame);

        sc->sc_bmisscount++;
#ifdef ATH_SUPPORT_DFS         
        if((sc->sc_curchan.priv_flags & CHANNEL_DFS) && (sc->sc_bmisscount == 1))
		sc->sc_3stream_sigflag1 = ath_hal_get3StreamSignature(ah);

	if((sc->sc_curchan.priv_flags & CHANNEL_DFS) && (sc->sc_3stream_sigflag1) && (sc->sc_bmisscount == 2))
		sc->sc_3stream_sigflag2 = ath_hal_get3StreamSignature(ah);

	if((sc->sc_3stream_sigflag1) && (sc->sc_3stream_sigflag2)) {
		sc->sc_3stream_sigflag1 = 0;
		sc->sc_3stream_sigflag2 = 0;
		ath_hal_dmaRegDump(ah);
		ath_hal_dfs3streamfix(sc->sc_ah, 1);
		ath_hal_forcevcs(sc->sc_ah);
		sc->sc_3streamfix_engaged = 1;
#if 0 //ndef ATH_DFS_RADAR_DETECTION_ONLY
                OS_SET_TIMER(&sc->sc_dfs->sc_dfs_war_timer, (10 * 60 * 1000));
		printk(KERN_DEBUG "%s engaing fix 0x9944 %x 0x9970 %x\n",__FUNCTION__,OS_REG_READ(sc->sc_ah,0x9944),
				OS_REG_READ(sc->sc_ah,0x9970));
#endif /*ATH_DFS_RADAR_DETECTION_ONLY*/
}
#endif
        /* XXX: doth needs the chanchange IE countdown decremented.
         *      We should consider adding a net80211 call to indicate
         *      a beacon miss so appropriate action could be taken
         *      (in that layer).
         */
        if (sc->sc_bmisscount < (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)) {

            if (show_cycles && !tx_frame && (rx_clear >= 99)) {
                /* Meant to reduce PHY errors and potential false detects */
                if (sc->sc_toggle_immunity)
                    ath_hal_set_immunity(ah, AH_TRUE);

                sc->sc_noise++;
            }

            /* If the HW requires hang detection, notify caller
             * and detect in the tasklet context.
             */
            if (ATH_HANG_WAR_REQ(sc)) {
                sc->sc_hang_check = AH_TRUE;
                *needmark = 1;
                return;
            }

            if (sc->sc_noreset) {
                printk("%s: missed %u consecutive beacons\n",
                       __func__, sc->sc_bmisscount);
                if (show_cycles) {
                    /*
                     * Display cycle counter stats from HAL to
                     * aide in debug of stickiness.
                     */
                    printk("%s: busy times: rx_clear=%d, rx_frame=%d, tx_frame=%d\n", __func__, rx_clear, rx_frame, tx_frame);
                } else {
                    printk("%s: unable to obtain busy times\n", __func__);
                }
            } else {
                DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                        "%s: missed %u consecutive beacons\n",
                        __func__, sc->sc_bmisscount);
            }
        } else if (sc->sc_bmisscount >= (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)) {
            if (sc->sc_noreset) {
                if (sc->sc_bmisscount == (BSTUCK_THRESH_PERVAP * sc->sc_nvaps)) {
                    printk("%s: beacon is officially stuck\n",
                           __func__);
                    ath_hal_dmaRegDump(ah);
                }
            } else {
                DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                        "%s: beacon is officially stuck\n",
                        __func__);

                if (show_cycles && rx_clear >= 99) {
                    sc->sc_hang_war |= HAL_BB_HANG_DETECTED;

                    if (rx_frame >= 99) {
                        printk("Busy environment detected\n");
                    } else {
                        printk("Interference detected\n");
                    }

                    printk("rx_clear=%d, rx_frame=%d, tx_frame=%d\n",
                           rx_clear, rx_frame, tx_frame);

                    sc->sc_noise = 0;
                }
                *needmark = 1;
            }
        }

        return;
    }

    if (sc->sc_toggle_immunity)
        ath_hal_set_immunity(ah, AH_FALSE);
    sc->sc_noise = 0;
    sc->sc_hang_check = AH_FALSE;

    /* resetting consecutive gtt count as beacon got 
    successfully transmitted. Eventhough this variable 
    is getting modified in two different contexts it is
    not protected as in worst case we loose reseting the 
    counter due to which we might have extra 
    check of mac tx hang which is not a problem */
    sc->sc_consecutive_gtt_count = 0;

    if (sc->sc_bmisscount != 0)
    {
        if (sc->sc_noreset) {
            printk("%s: resume beacon xmit after %u misses\n",
                   __func__, sc->sc_bmisscount);
        } else {
            DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                    "%s: resume beacon xmit after %u misses\n",
                    __func__, sc->sc_bmisscount);
        }
        sc->sc_bmisscount = 0;
    }

    /*
     * Generate beacon frames.  If we are sending frames
     * staggered then calculate the slot for this frame based
     * on the tsf to safeguard against missing an swba.
     * Otherwise we are bursting all frames together and need
     * to generate a frame for each vap that is up and running.
     */
    if (sc->sc_stagbeacons)
    {       /* staggered beacons */
        u_int64_t tsf;
        u_int32_t tsftu;
        u_int16_t intval;
        int nslots;

        nslots = sc->sc_nslots;

        intval = sc->sc_intval;
        
        if (intval == 0 || nslots == 0) { 
            /*
             * This should not happen. We're seeing zero bintval sometimes 
             * in WDS testing but is not easily reproducible 
             */
            return;
        }

        tsf = ath_hal_gettsf64(ah);
        tsftu = TSF_TO_TU(tsf>>32, tsf);
        slot = ((tsftu % intval) * nslots) / intval;
        if_id = sc->sc_bslot[(slot + 1) % nslots];
        DPRINTF(sc, ATH_DEBUG_BEACON_PROC,
                "%s: slot %d [tsf %llu tsftu %u intval %u] if_id %d\n",
                __func__, slot, (unsigned long long)tsf, tsftu, intval, if_id);
        bfaddr = 0;
        if (if_id != ATH_IF_ID_ANY)
        {
            bf = ath_beacon_generate(sc, if_id, needmark);
            if (bf != NULL) {
                bfaddr = bf->bf_daddr;
                bc = 1;
            }
        }
    }
    else
    {                        /* burst'd beacons */
        u_int32_t *bflink;
        struct ath_desc *prev_ds = NULL;
        struct ath_txq *cabq = sc->sc_cabq;
        struct ath_txq *burstbcn_cabq = &sc->sc_burstbcn_cabq;

            /* 
             * bursted beacons and edma: we shall move mcast traffic from 
             * all vaps to a temp q (burstbcn_cabq) and then move the tempq
             * to cabq. we could have just used the cabq instead of tempq
             * but that makes locking cabq unneccessarily complex.
             *
             * we don't need to lock burstbcn_cabq as it is accessed only in
             * tasklet context and linux makes sure that only one instance
             * of tasklet is running. what about other OSes?
             */

        if (sc->sc_enhanceddmasupport)
            TAILQ_INIT(&burstbcn_cabq->axq_q);

        bflink = &bfaddr;
        /* XXX rotate/randomize order? */
        for (slot = 0; slot < ATH_BCBUF; slot++)
        {
            if_id = sc->sc_bslot[slot];
            if (if_id != ATH_IF_ID_ANY)
            {
                bf = ath_beacon_generate(sc, if_id, needmark);
                if (bf != NULL)
                {
                    if (sc->sc_enhanceddmasupport) {
                        if (slot == 0)
                            bfaddr = bf->bf_daddr;
                        else 
                            ath_hal_setdesclink(ah, prev_ds, bf->bf_daddr);
                        prev_ds = bf->bf_desc;
                    } 
                    else {
#ifdef AH_NEED_DESC_SWAP
                        if(bflink != &bfaddr)
                            *bflink = cpu_to_le32(bf->bf_daddr);
                        else
                            *bflink = bf->bf_daddr;
#else
                        *bflink = bf->bf_daddr;
#endif
                        ath_hal_getdesclinkptr(ah, bf->bf_desc, &bflink);
                    }
                    bc ++;
                }
            }
        }
        /* this may not be needed as ath_becon_setup will zero it out */
        if (!sc->sc_enhanceddmasupport)
            *bflink = 0;    /* link of last frame */
        else {
            struct ath_buf *bfmcast;
            bfmcast = TAILQ_FIRST(&burstbcn_cabq->axq_q);
            if (bfmcast) {
                ATH_TXQ_LOCK(cabq);
                ATH_EDMA_TXQ_MOVE_MCASTQ(burstbcn_cabq, cabq);

                ASSERT(bfmcast->bf_daddr != 0);
                ath_hal_puttxbuf(ah, cabq->axq_qnum, bfmcast->bf_daddr);
                ath_hal_txstart(ah, cabq->axq_qnum);
                ATH_TXQ_UNLOCK(cabq);
            }
        }
    }

    /*
     * Handle slot time change when a non-ERP station joins/leaves
     * an 11g network.  The 802.11 layer notifies us via callback,
     * we mark updateslot, then wait one beacon before effecting
     * the change.  This gives associated stations at least one
     * beacon interval to note the state change.
     *
     * NB: The slot time change state machine is clocked according
     *     to whether we are bursting or staggering beacons.  We
     *     recognize the request to update and record the current
     *     slot then don't transition until that slot is reached
     *     again.  If we miss a beacon for that slot then we'll be
     *     slow to transition but we'll be sure at least one beacon
     *     interval has passed.  When bursting slot is always left
     *     set to ATH_BCBUF so this check is a noop.
     */
    /* XXX locking */
    if (sc->sc_updateslot == ATH_UPDATE)
    {
        sc->sc_updateslot = ATH_COMMIT; /* commit next beacon */
        sc->sc_slotupdate = slot;
    }
    else if (sc->sc_updateslot == ATH_COMMIT && sc->sc_slotupdate == slot)
        ath_setslottime(sc);        /* commit change to hardware */

#if !(UNIFIED_SMARTANTENNA)
    if ((!sc->sc_stagbeacons || slot == 0) && (!sc->sc_diversity))
    {
        int otherant;
        /*
         * Check recent per-antenna transmit statistics and flip
         * the default rx antenna if noticeably more frames went out
         * on the non-default antenna.  Only do this if rx diversity
         * is off.
         * XXX assumes 2 anntenae
         */
        otherant = sc->sc_defant & 1 ? 2 : 1;
        if (sc->sc_ant_tx[otherant] > sc->sc_ant_tx[sc->sc_defant] + ATH_ANTENNA_DIFF)
        {
            DPRINTF(sc, ATH_DEBUG_BEACON,
                    "%s: flip defant to %u, %u > %u\n",
                    __func__, otherant, sc->sc_ant_tx[otherant],
                    sc->sc_ant_tx[sc->sc_defant]);
            ath_setdefantenna(sc, otherant);
        }
        sc->sc_ant_tx[1] = sc->sc_ant_tx[2] = 0;
    }
#endif
    if (bfaddr != 0)
    {
        /*
         * WAR - the TXD to Q9 - TXDP fifo has a logic defect, so that after couple 
         * of times stopping/starting TXD[9], we'll see the beacon stuck.
         *
         * All the EDMA platform has similiar issue, per designer's suggestion to 
         * remove it temporarily.
         *
         * TBD: once HW fix this issue, we may revert this...
         */
        if (!sc->sc_enhanceddmasupport) {

           /*
            * Stop any current dma and put the new frame(s) on the queue.
            * This should never fail since we check above that no frames
            * are still pending on the queue.
            */
            if (!ath_hal_stoptxdma(ah, sc->sc_bhalq, 0))
            {
                DPRINTF(sc, ATH_DEBUG_ANY,
                        "%s: beacon queue %u did not stop?\n",
                        __func__, sc->sc_bhalq);
                /* NB: the HAL still stops DMA, so proceed */
            }
        }

#ifndef REMOVE_PKT_LOG
        if(bf)
        {
            struct log_tx log_data = {0};
            log_data.firstds = bf->bf_desc;
            log_data.bf = bf;
#ifdef ATH_BEACON_DEFERRED_PROC
            ath_log_txctl(sc, &log_data, 0);
#else
            ath_log_txctl(sc, &log_data, PHFLAGS_INTERRUPT_CONTEXT);
#endif
            log_data.lastds = bf->bf_desc;
#ifdef ATH_BEACON_DEFERRED_PROC
            ath_log_txstatus(sc, &log_data, 0);
#else
            ath_log_txstatus(sc, &log_data, PHFLAGS_INTERRUPT_CONTEXT);
#endif
        }

#endif
        /* NB: cabq traffic should already be queued and primed */
        ath_hal_puttxbuf(ah, sc->sc_bhalq, bfaddr);
        ath_hal_txstart(ah, sc->sc_bhalq);

#ifdef QCA_SUPPORT_CP_STATS
        pdev_cp_stats_tx_beacon_inc(sc->sc_pdev, bc);
        pdev_cp_stats_tx_mgmt_inc(sc->sc_pdev, bc);
#else
        sc->sc_stats.ast_be_xmit += bc;     /* XXX per-vap? */
        sc->sc_stats.ast_tx_mgmt += bc;     /* XXX per-vap? */
#endif
        if(bf != NULL)
        {
            sc->sc_stats.ast_tx_bytes += wbuf_get_pktlen(bf->bf_mpdu);
        }
#ifdef ATH_ADDITIONAL_STATS
        sc->sc_stats.ast_txq_packets[sc->sc_bhalq]++;
#endif
    }
}

/******************************************************************************/
/*!
**  \brief Task or Beacon Stuck processing
**
**  Processing for Beacon Stuck. 
**  Basically calls the ath_internal_reset function to reset the chip.
**
**  \param data Pointer to the ATH object (ath_softc).
**
**  \return N/A
*/

void
ath_bstuck_tasklet(struct ath_softc *sc)
{

#if 0 //DFSUMAC:defined(ATH_SUPPORT_DFS) && !defined(ATH_DFS_RADAR_DETECTION_ONLY)
    sc->sc_dfs_hang.total_stuck++;
#endif
    if (!ATH_HANG_DETECTED(sc)) {
        printk(KERN_DEBUG "%s: stuck beacon; resetting (bmiss count %u)\n",
               __func__, sc->sc_bmisscount);
    }
    sc->sc_reset_type = ATH_RESET_NOLOSS;
    ath_internal_reset(sc);
    sc->sc_reset_type = ATH_RESET_DEFAULT;
#ifdef QCA_SUPPORT_CP_STATS
    pdev_lmac_cp_stats_ast_reset_on_error_inc(sc->sc_pdev, 1);
#else
    sc->sc_stats.ast_resetOnError++;
#endif

#if WLAN_SPECTRAL_ENABLE
   /* If CW interference is severe, then HW goes into a loop of continuous stuck beacons and resets. 
    * The check interference function checks if a single NF value exceeds bounds. The CHANNEL_CW_INT flag 
    * cannot be used becuase it is set only if the median of several NFs in a history buffer exceed bounds.
    * Continuous resets will not allow the history buffer to be populated.
    */
    if (ath_hal_interferenceIsPresent(sc->sc_ah)) {
       struct ath_spectral* spectral = (struct ath_spectral*)sc->sc_spectral;
       DPRINTF(sc, ATH_DEBUG_BEACON, "%s: Detected interference, sending eacs message\n", __func__);
       spectral_send_intf_found_msg(spectral, 1, 0);
       return;
    }
#endif

#ifdef ATH_SUPPORT_DFS
    if (sc->sc_3streamfix_engaged) {
	    ath_hal_dfs3streamfix(sc->sc_ah, 1);
	    ath_hal_forcevcs(sc->sc_ah);
    }
#endif
}


/*
 * return information about all active vaps excluding the one passed.
 */
static int ath_get_all_active_vap_info(struct ath_softc *sc, ath_vap_info *av_info, int exclude_id)
{
    int i,j=0;
    struct ath_vap *avp;

    for (i=0; i < sc->sc_nvaps; i++) {
        avp = sc->sc_vaps[i];
        if (avp) {
            if(avp->av_up && i != exclude_id){
                av_info[j].if_id = i;
                ++j;
            }
        }
    }
    return j;
}

static void ath_get_beacon_config(struct ath_softc *sc, int if_id, ieee80211_beacon_config_t *conf)
{
    if (sc->sc_ieee_ops->get_beacon_config)
    {
        sc->sc_ieee_ops->get_beacon_config(sc->sc_ieee, if_id, conf);
    } else {
        /*
         * Protocol stack doesn't support dynamic beacon configuration,
         * use default configurations.
         */
        conf->beacon_interval = ATH_DEFAULT_BINTVAL;
        conf->listen_interval = 1;
        conf->dtim_period = conf->beacon_interval;
        conf->dtim_count = 1;
        conf->bmiss_timeout = ATH_DEFAULT_BMISS_LIMIT * conf->beacon_interval;
    }
}


/*
 * @get the next tbtt in TUs.
 * @param curr_tsf64   : current tsf in usec.
 * @param bintval      : beacon interval in  TUs.
 *  @return next tbtt in TUs (lower 32 bits )
 */
static u_int32_t get_next_tbtt_tu_32(u_int64_t curr_tsf64, u_int32_t bintval)
{
   u_int64_t           nexttbtt_tsf64;

   bintval = bintval << 10; /* convert into usec */
    
   nexttbtt_tsf64 =  curr_tsf64 + bintval;

   nexttbtt_tsf64  = nexttbtt_tsf64 - OS_MOD64_TBTT_OFFSET(nexttbtt_tsf64, bintval);

   if ((nexttbtt_tsf64 - curr_tsf64) < MIN_TSF_TIMER_TIME ) {  /* if the immediate next tbtt is too close go to next one */
       nexttbtt_tsf64 += bintval;
   }

   return (u_int32_t) (nexttbtt_tsf64 >> 10);  /* convert to TUs */
}



static void ath_beacon_config_tsf_offset(struct ath_softc *sc, int if_id )
{
    struct ath_vap *avp;
    wbuf_t wbuf ;
    struct ieee80211_frame  *wh;

    KASSERT( (if_id != ATH_IF_ID_ANY), ("for vap up reason if_id can not be any" ));
    avp = sc->sc_vaps[if_id];

    if(avp->av_opmode == HAL_M_STA ) {
        return;
    }

    KASSERT( (avp->av_bcbuf != NULL), ("beacon buf can not be null" ));
    KASSERT( (avp->av_bcbuf->bf_mpdu != NULL), ("beacon wbuf can not be null" ));
   
    wbuf= (wbuf_t)avp->av_bcbuf->bf_mpdu;
    /*
     * Calculate a TSF adjustment factor required for
     * staggered beacons.  Note that we assume the format
     * of the beacon frame leaves the tstamp field immediately
     * following the header.
     */
    if (sc->sc_stagbeacons)
    {
        u_int64_t tsfadjust, tsfadjust_le;
        int intval;

        if (sc->sc_ieee_ops->get_beacon_config)
        {
            ieee80211_beacon_config_t conf;
            
            sc->sc_ieee_ops->get_beacon_config(sc->sc_ieee, if_id, &conf);
            intval = conf.beacon_interval;
        }
        else
            intval = ATH_DEFAULT_BINTVAL;
        
        /*
         * The beacon interval is in TU's; the TSF in usecs.
         * We figure out how many TU's to add to align the
         * timestamp then convert to TSF units and handle
         * byte swapping before writing it in the frame.
         * The hardware will then add this each time a beacon
         * frame is sent.  Note that we align vap's 1..N
         * and leave vap 0 untouched.  This means vap 0
         * has a timestamp in one beacon interval while the
         * others get a timestamp aligned to the next interval.
         */
        tsfadjust =
        tsfadjust_le = 0;
        if (avp->av_bslot) {
            tsfadjust = ((intval*1024) * (sc->sc_nslots - avp->av_bslot)) / sc->sc_nslots;
            tsfadjust_le = cpu_to_le64(tsfadjust);
        }
        avp->av_tsfadjust = tsfadjust_le;
        DPRINTF(sc, ATH_DEBUG_BEACON,
                 "%s: %s beacons, bslot %d intval %u tsfadjust %llu tsfadjust_le %llu\n",
                 __func__, sc->sc_stagbeacons ? "stagger" : "burst",
                 avp->av_bslot, intval, (unsigned long long)tsfadjust, (unsigned long long)tsfadjust_le);

        wh = (struct ieee80211_frame *)wbuf_header(wbuf);
        OS_MEMCPY(&wh[1], &tsfadjust_le, sizeof(tsfadjust_le));

        ath_bslot_info_notify_change(avp, le64_to_cpu(tsfadjust_le));
    }


}

static void ath_beacon_config_all_tsf_offset(struct ath_softc *sc)
{
    int id;
    for (id = 0; id < ATH_BCBUF; id++) {
        if (sc->sc_bslot[id] != ATH_IF_ID_ANY) {
            ath_beacon_config_tsf_offset(sc, sc->sc_bslot[id]);
        }
    }
}

      
/*
* allocate/free a beacon slot for AP vaps based on the reason and vap id.
* when a vap is up allocate beacon slot.
* when a vap is down free the beacon slot and to keep
* the allocated beacon slots contiguous move the last one to the free slot.
*/
static void ath_beacon_config_beacon_slot(struct ath_softc *sc, ath_beacon_slot_op config_op, int if_id )
{
    struct ath_vap *avp;
    bool config_tsf_offset = false;

    if (config_op == ATH_ALLOC_BEACON_SLOT) {     /* alloc slot */
        /*
         * Assign the vap to a beacon xmit slot.  As
         * above, this cannot fail to find one.
         */
        KASSERT( (if_id != ATH_IF_ID_ANY), ("for vap up reason if_id can not be any" ));
        avp = sc->sc_vaps[if_id];
        if (avp->av_bslot == -1) { /* coming up from down state */
            avp->av_bslot = sc->sc_nbcnvaps;
            sc->sc_bslot[avp->av_bslot] = if_id;
            sc->sc_nbcnvaps++;
            ath_set_num_slots(sc);
            config_tsf_offset = true;
        }
    }

    if (config_op == ATH_FREE_BEACON_SLOT) {     /* deallocate slot */
        KASSERT( (if_id != ATH_IF_ID_ANY), ("for vap down reason if_id can not be any" ));
        avp = sc->sc_vaps[if_id];
        if (avp->av_bslot != -1) { /* if slot was allocated, free it */
            KASSERT( (sc->sc_bslot[avp->av_bslot] == if_id), ("if_id does not match in the beacon slot" ));
            sc->sc_nbcnvaps--;
            ath_set_num_slots(sc);
            /*
             * move the last entry to 
             * to th entry corresponding to the vap that is down.
             */
            if (avp->av_bslot < sc->sc_nbcnvaps) {
                sc->sc_bslot[avp->av_bslot] = sc->sc_bslot[sc->sc_nbcnvaps];
                sc->sc_vaps[sc->sc_bslot[sc->sc_nbcnvaps]]->av_bslot = avp->av_bslot;
                sc->sc_bslot[sc->sc_nbcnvaps] = ATH_IF_ID_ANY;
                config_tsf_offset = true;
            } else {
                sc->sc_bslot[avp->av_bslot] = ATH_IF_ID_ANY;
            }
            avp->av_bslot = -1;
        }
    }

    if (config_op == ATH_ALLOC_FIRST_SLOT) {     /* alloc fisrt slot */
            bool insert_vap=false;
            if ( sc->sc_nbcnvaps > 0) {
            /* 
             * if it does not have the slot 0 then
             * push out the other vap from slot 0  and insert the vap on to slot 0.
             */      
                if ( sc->sc_bslot[0] != if_id) {
                    avp = sc->sc_vaps[sc->sc_bslot[0]];
                    sc->sc_bslot[sc->sc_nbcnvaps] = sc->sc_bslot[0];
                    avp->av_bslot = sc->sc_nbcnvaps;
                    insert_vap = true;
                    config_tsf_offset = true;
                    DPRINTF(sc, ATH_DEBUG_BEACON, "%s: move VAP %d from slot 0 \n", __func__, sc->sc_bslot[0]);
                }
            } else if (sc->sc_nbcnvaps == 0) {
                /* 
                 * if the STA vap is coming up and no other vaps are up 
                 */      
                insert_vap=true;
            }   
            if (insert_vap) {
                DPRINTF(sc, ATH_DEBUG_BEACON, "%s: insert STA Vap in slot 0 \n", __func__);
                /* insert STA vap at slot 0 */
                avp = sc->sc_vaps[if_id];
                avp->av_bslot = 0;
                sc->sc_bslot[0] = if_id;
                sc->sc_nbcnvaps++;
                ath_set_num_slots(sc);
            }
    }
    /* configure tsf offset for all beacon slots */
    if (config_tsf_offset) {
        ath_beacon_config_all_tsf_offset(sc);
    }
}

/*
 * find the smallest number that is power of 2 and is larger than the number of beacon vaps. 
 */
static void ath_set_num_slots(struct ath_softc *sc)
{
    int i, num_slots=0;
    KASSERT( (sc->sc_nbcnvaps <= ATH_BCBUF),  ("too many beacon vaps "));  
    if ( sc->sc_nbcnvaps == 0) {
        sc->sc_nslots = num_slots; 
        return;
    }
    for (i=0;i<5;++i) {
       num_slots = (1 << i);
       if ( sc->sc_nbcnvaps <= num_slots)
              break;
    }
    sc->sc_nslots = num_slots; 
}

static void ath_beacon_config_sta_mode(struct ath_softc *sc, ath_beacon_config_reason reason, int if_id )
{
    int num_slots,tbtt_timer_period;
    ieee80211_beacon_config_t conf;
    u_int32_t nexttbtt, intval;         /* units of TU   */
    int vap_index;
    struct ath_vap *avp;
    struct ath_vap *avpOther;
    u_int64_t tsf;
    u_int32_t tsftu, intval_frac=0;

    /*
     * assume only one sta vap.
     */
    switch (reason) {
    case ATH_BEACON_CONFIG_REASON_VAP_DOWN:     /* vap is down */
        /*
         *  vap is going down. adjust the slots.
         */
        avp = sc->sc_vaps[if_id]; /* a STA vap coming down may not have a beacon slot ,so check before freeing the slot */
        DPRINTF(sc, ATH_DEBUG_BEACON, "%s: reason %s if_id %d opmode %s \n",
            __func__, ath_beacon_config_reason_name(reason), if_id,
            ath_hal_opmode_name(sc->sc_opmode));
        /*
         * if a STA vap is going down and if another STA vap exists and is up,
         * then allocate the first slot to the existing STA vap.
         */ 
        if(avp->av_opmode == HAL_M_STA) {
            int i, j;
            /* if the sta had beacon slot allocated originally  then free it */
            if(avp->av_bslot != -1) {
                ath_beacon_config_beacon_slot(sc,ATH_FREE_BEACON_SLOT,if_id);
            }
            j = 0;
            for (i=0; ((i < ATH_VAPSIZE) && (j < sc->sc_nvaps)); i++) {
                avpOther = sc->sc_vaps[i];
                if (avpOther) {
                    j++;
                    if (i != if_id && avpOther->av_opmode == HAL_M_STA && avpOther->av_up && avpOther->av_bslot == -1) {
                        ath_beacon_config_beacon_slot(sc,ATH_ALLOC_FIRST_SLOT,i);
                        DPRINTF(sc, ATH_DEBUG_BEACON, "%s:allocating slot 0 to STA Vap with if_id %d\n",
                                __func__, i);
                    } 
                }
            }
        } else { /* Host AP mode */
           ath_beacon_config_beacon_slot(sc,ATH_FREE_BEACON_SLOT,if_id);
        }
        break;
    case ATH_BEACON_CONFIG_REASON_VAP_UP:     /* vap is up */
        if(sc->sc_vaps[if_id]->av_opmode == HAL_M_STA) {
            /* 
             * if a STA vap already exists on the slot 0 then 
             * do not allocate a slot for this seconds STA vap coming
             * up. (STA+STA ) case only one vap gets to have beacon slot
             * and the same vaps beacon information is programmed
             * into hardware. For the second STA vap the beacon offset
             * is maintained. 
             */
            if (sc->sc_nbcnvaps > 0 && sc->sc_bslot[0] != if_id &&
                sc->sc_vaps[sc->sc_bslot[0]]->av_opmode == HAL_M_STA) {
                DPRINTF(sc, ATH_DEBUG_BEACON, "%s:(STA+STA case) primary STA vap with if_id %d exists \n",
                            __func__, sc->sc_bslot[0]);
                return;
            }
            ath_beacon_config_beacon_slot(sc,ATH_ALLOC_FIRST_SLOT,if_id);
        } else {
            ath_beacon_config_beacon_slot(sc,ATH_ALLOC_BEACON_SLOT,if_id);
        }
        break;
    case ATH_BEACON_CONFIG_REASON_OPMODE_SWITCH:
        {
            int j;
            /*
             * allocate beacon slot for the station vap.
             * protect vap list ?
             */ 
            j = 0;
            for(vap_index=0;((vap_index < ATH_VAPSIZE) && (j < sc->sc_nvaps)); vap_index++) {
                avp=sc->sc_vaps[vap_index];
                if (avp) {
                    j++;
                    if ((avp->av_opmode == HAL_M_STA) && avp->av_up) {
                        ath_beacon_config_beacon_slot(sc,ATH_ALLOC_BEACON_SLOT,vap_index);
                    }
                }
            }
        }
        break;
    case ATH_BEACON_CONFIG_REASON_RESET:
        break;
      
    }
    

    if (sc->sc_nbcnvaps == 0 ) {
        return;
    }
    /*
     * assert that the first beacon slot always points to a valid VAP.
     */
    KASSERT((sc->sc_bslot[0] != ATH_IF_ID_ANY),("the first slot does not have a valid vap id "));
    KASSERT((sc->sc_vaps[sc->sc_bslot[0]]),("the first slot does not point to a valid vap "));
#if 0
    KASSERT((sc->sc_vaps[sc->sc_bslot[0]]->av_opmode == HAL_M_STA),("the first slot does not point to a STA vap "));
#endif

    ath_get_beacon_config(sc,sc->sc_bslot[0],&conf); /* get the first VAPs beacon config */

    num_slots = sc->sc_nslots; 

    tbtt_timer_period = conf.beacon_interval & HAL_BEACON_PERIOD;
    if (sc->sc_stagbeacons)  { /* when  multiple vaps are present */
        tbtt_timer_period /= num_slots;    /* for staggered beacons */
    }
    nexttbtt = tbtt_timer_period;
    tsf = ath_hal_gettsf64(sc->sc_ah);
    /*
     * configure the sta beacon timers.
     */ 
    if (sc->sc_vaps[sc->sc_bslot[0]]->av_opmode == HAL_M_STA) {
        nexttbtt = ath_beacon_config_sta_timers(sc,&conf,tbtt_timer_period, tsf);
    }

    /*
     * configure the ap beacon timers.
     */ 
    if (num_slots > 1 || (sc->sc_vaps[sc->sc_bslot[0]]->av_opmode == HAL_M_HOSTAP)) {
        u_int32_t slot_duration ;
        u_int32_t sta_intval;      /* station beacon interval   */
        /* 
         * get the AP VAP beacon config
         * VAP at slot 1 is always AP VAP.
         */
        sta_intval = conf.beacon_interval & HAL_BEACON_PERIOD;
        ath_get_beacon_config(sc,sc->sc_bslot[1],&conf); 
        intval = conf.beacon_interval & HAL_BEACON_PERIOD;
        if (sc->sc_stagbeacons)  { /* when in WDS or multi AP vap mode */
            intval_frac = ((intval % sc->sc_nslots) * 1024)/sc->sc_nslots;
            tsftu = TSF_TO_TU(tsf>>32, tsf);
            
            /*
             * increment the nextbtt by sta beacon interval until
             * the first slot is above current tsf.
             */
            slot_duration = (intval)/num_slots;
            nexttbtt += slot_duration;            
            
            if (sta_intval == 0) sta_intval = ATH_DEFAULT_BINTVAL;
                        
            if (nexttbtt > tsftu)
            {
                u_int32_t   remainder = (nexttbtt - tsftu)%sta_intval;                
                nexttbtt = tsftu + remainder;
            }
            else //nexttbtt <= tsftu
            {
                u_int32_t   remainder = (tsftu - nexttbtt)%sta_intval;
                nexttbtt = tsftu - remainder + sta_intval;
            }

             /*
              * special case if  num slots is 2 ( 1 STA VAP + 1 AP VAP) then
              * program SWBA timer to start at intval/2 from the tbtt of the STA
              * where intval is the beacon interval(usually 100TUs).
              */
            if (num_slots != 2) {
                intval /= num_slots;    /* for staggered beacons */
            }
        }
        ath_beacon_config_ap_timers(sc,nexttbtt,intval, intval_frac);
        DPRINTF(sc, ATH_DEBUG_BEACON, "%s: AP nexttbtt %d intval %d \n", __func__,nexttbtt,intval);
    }
}

static void ath_beacon_config_ap_mode(struct ath_softc *sc, ath_beacon_config_reason reason, int if_id )
{
    u_int32_t nexttbtt, intval;         /* units of TU   */
    u_int32_t intval_frac=0;
    int  num_slots;
    ieee80211_beacon_config_t conf;
    int slot=0;
    bool event_reset=0;

    switch (reason) {
    case ATH_BEACON_CONFIG_REASON_VAP_UP:
    case ATH_BEACON_CONFIG_REASON_VAP_DOWN:
        /*
         * When this is going to be the only active VAP
         * then do a TSF reset. All other cases do not
         * disturb the TSF
         */
        if(sc->sc_nbcnvaps != 0)  
            event_reset=1;
     
        if( sc->sc_vaps[if_id]->av_opmode == HAL_M_STA) {
            struct ath_vap *avp = sc->sc_vaps[if_id];
            /* 
             * STA vap is either going up (or) down 
             * we are in AP mode which means there wont be any network sleep support
             * we do not need to do any thing when a sta vap goes up/down.
             */  
            if( avp->av_bslot != -1) {
                /*
                 * if beacon slot was allocated to STA VAP. 
                 * this can happen if the STA VAP is brought down  with chip in STA mode and
                 * the chip is switched to AP mode while bringing down the VAP.
                 */ 
                ath_beacon_config_beacon_slot(sc,
                        reason==ATH_BEACON_CONFIG_REASON_VAP_UP?ATH_ALLOC_BEACON_SLOT:ATH_FREE_BEACON_SLOT,if_id);
 
            }
            return;
        }
        /* AP vap is coming up/down, allocate/deallocate beacon slots */
        ath_beacon_config_beacon_slot(sc,
             reason==ATH_BEACON_CONFIG_REASON_VAP_UP?ATH_ALLOC_BEACON_SLOT:ATH_FREE_BEACON_SLOT,if_id);
        break;
    case ATH_BEACON_CONFIG_REASON_RESET:
        event_reset=1;
        /* fall through */
    case ATH_BEACON_CONFIG_REASON_OPMODE_SWITCH:
        /*
         * deallocate beacon slot for station vap.
         */ 
        for(slot=0;slot < sc->sc_nbcnvaps; ++slot) {
            if( sc->sc_vaps[sc->sc_bslot[slot]]->av_opmode == HAL_M_STA) {
                ath_beacon_config_beacon_slot(sc,ATH_FREE_BEACON_SLOT,sc->sc_bslot[slot]);
            }
        }
        break;
    }

    if (sc->sc_nbcnvaps == 0 ) {
       return;
    }

    ath_get_beacon_config(sc,if_id,&conf); /* assume all the vaps have the same beacon config */
    num_slots = sc->sc_nslots; 
    intval = conf.beacon_interval & HAL_BEACON_PERIOD;
    if (sc->sc_stagbeacons)  { /* when in WDS or multi AP vap mode */
          /* Keep track of fractions if any to add it later when period
           * configured in the HW */
          intval_frac = ((intval % sc->sc_nslots) * 1024)/sc->sc_nslots;
          intval /= num_slots;    /* for staggered beacons */
    }
    if (event_reset) {
        u_int64_t curr_tsf64;
        u_int32_t bcn_intval;


        bcn_intval = (conf.beacon_interval & HAL_BEACON_PERIOD);
 
        /* 
         * reset event, calculate the next tbtt based on current tsf and 
         * reprogram the SWBA based on the calculated tbtt, do not let the 
         * ath_beacon_config_ap_timers reset the tsf to 0.
         */ 
        curr_tsf64 = ath_hal_gettsf64(sc->sc_ah);

        nexttbtt =  get_next_tbtt_tu_32(curr_tsf64, bcn_intval);  /* convert to tus */

    } else {
       nexttbtt = intval;
    }

    /* configure the HAL for the IBSS mode */
    ath_beacon_config_ap_timers(sc,nexttbtt,intval, intval_frac);
}

#if ATH_SUPPORT_IBSS

static void ath_beacon_config_ibss_mode(struct ath_softc *sc, ath_beacon_config_reason reason, int if_id )
{
    ieee80211_beacon_config_t conf;
    u_int32_t nexttbtt, intval;         /* units of TU   */
    ath_vap_info av_info[ATH_VAPSIZE];
    u_int32_t num_active_vaps; 

    num_active_vaps =  ath_get_all_active_vap_info(sc,av_info,if_id);

    OS_MEMZERO(&conf, sizeof(ieee80211_beacon_config_t));

    switch (reason) {
    case ATH_BEACON_CONFIG_REASON_VAP_UP:
    case ATH_BEACON_CONFIG_REASON_VAP_DOWN:
        if (reason == ATH_BEACON_CONFIG_REASON_VAP_UP) {     /* vap is up */
            KASSERT( (num_active_vaps == 0), ("can not configure IBSS vap when other vaps are active" ));
        }
        ath_beacon_config_beacon_slot(sc,
             reason==ATH_BEACON_CONFIG_REASON_VAP_UP?ATH_ALLOC_BEACON_SLOT:ATH_FREE_BEACON_SLOT,if_id);
    default: 
        break;
    }

    ath_get_beacon_config(sc,if_id,&conf);
    
    /* extract tstamp from last beacon and convert to TU */
    nexttbtt = TSF_TO_TU(LE_READ_4(conf.u.last_tstamp + 4),
                         LE_READ_4(conf.u.last_tstamp));

    intval = conf.beacon_interval & HAL_BEACON_PERIOD;

    /* configure the HAL for the IBSS mode */
    ath_beacon_config_ibss_timers(sc,nexttbtt,intval);

}
#endif

/*
 * Configure the beacon and sleep timers.
 * supports combination of multiple vaps 
 * with multiple chip mode.
 *
 * When operating in station mode this sets up the beacon
 * timers according to the timestamp of the last received
 * beacon and the current TSF, configures PCF and DTIM
 * handling, programs the sleep registers so the hardware
 * will wakeup in time to receive beacons, and configures
 * the beacon miss handling so we'll receive a BMISS
 * interrupt when we stop seeing beacons from the AP
 * we've associated with.
 * 
 * also supports multiple combinations of VAPS.
 * a) multiple AP VAPS.
 * b) one AP VAP and one station VAP with the chip mode in AP (for WDS).
 * b) one AP VAP and one station VAP with the chip mode in STA(for P2P).
 */

void
ath_beacon_config(struct ath_softc *sc,ath_beacon_config_reason reason, int if_id)
{

    DPRINTF(sc, ATH_DEBUG_BEACON,
        "%s: reason %s if_id %d opmode %s  nslots %d\n",
        __func__, ath_beacon_config_reason_name(reason), if_id,
        ath_hal_opmode_name(sc->sc_opmode), sc->sc_nslots);

    ATH_BEACON_LOCK(sc);
    if (sc->sc_nbcnvaps ==0 && reason != ATH_BEACON_CONFIG_REASON_VAP_UP) { 
        /*
         * currently no active beacon vaps and none coming up.
         * ignore the call.
         */
         ATH_BEACON_UNLOCK(sc);
         DPRINTF(sc, ATH_DEBUG_BEACON, "%s: no beacon vaps ignore reason %d if_id %d \n", __func__, reason, if_id);
         return;
    }

    if (sc->sc_opmode  ==  HAL_M_HOSTAP) {
        ath_beacon_config_ap_mode(sc,reason,if_id);
    } else if (sc->sc_opmode  ==  HAL_M_STA) {
        ath_beacon_config_sta_mode(sc,reason,if_id);
    }
#if ATH_SUPPORT_IBSS
    else if (sc->sc_opmode  ==  HAL_M_IBSS) {
        ath_beacon_config_ibss_mode(sc,reason,if_id);
    }
#endif
    do {
        int i;
        DPRINTF(sc, ATH_DEBUG_BEACON, "%s: # beacon vaps %d nslots %d \n", __func__, sc->sc_nbcnvaps, sc->sc_nslots);
        for (i=0;i<sc->sc_nbcnvaps;++i) {
            DPRINTF(sc, ATH_DEBUG_BEACON, "slot %d vapid %d vap mode %s \n",
                i, sc->sc_bslot[i],
                ath_hal_opmode_name(sc->sc_vaps[sc->sc_bslot[i]]->av_opmode));
        }
    } while(0);
    ATH_BEACON_UNLOCK(sc);
}


static u_int32_t ath_beacon_config_sta_timers(struct ath_softc *sc, ieee80211_beacon_config_t *conf, int tbtt_timer_period, u_int64_t tsf)
{
    u_int32_t nexttbtt, intval;         /* units of TU   */
    HAL_BEACON_STATE bs;
    u_int32_t tsftu;
    int dtimperiod, dtimcount, sleepduration;
    int cfpperiod, cfpcount;
    struct ath_hal *ah = sc->sc_ah;

    /* extract tstamp from last beacon and convert to TU */
    nexttbtt = TSF_TO_TU(LE_READ_4(conf->u.last_tstamp + 4),
                         LE_READ_4(conf->u.last_tstamp));
     
    intval = conf->beacon_interval & HAL_BEACON_PERIOD;
    /*
     * Setup dtim and cfp parameters according to
     * last beacon we received (which may be none).
     */
    dtimperiod = conf->dtim_period;
    if (dtimperiod <= 0)        /* NB: 0 if not known */
        dtimperiod = 1;
    dtimcount = conf->dtim_count;
    if (dtimcount >= dtimperiod)    /* NB: sanity check */
        dtimcount = 0;      /* XXX? */
    cfpperiod = 1;          /* NB: no PCF support yet */
    cfpcount = 0;

    sleepduration = conf->listen_interval * intval;
    if ((sleepduration <= 0)  || sc->sc_up_beacon_purge)
        sleepduration = intval;

    /*
     * Pull nexttbtt forward to reflect the current
     * TSF and calculate dtim+cfp state for the result.
     */
    tsftu = TSF_TO_TU(tsf>>32, tsf);

    if (intval == 0) intval = ATH_DEFAULT_BINTVAL;
   
    /*

          oldtbtt                    tsftu                     nexttbtt
             |                         |                          |
       -----------------------------------------------------------------------
       
       dtimperiod is the number of beacon interval between two dtim
       
       dtimcount  is the remained number of beacon interval before next dtim

    */
    if (nexttbtt > tsftu)
    {
        u_int32_t   countdiff, oldtbtt, remainder;
        
        oldtbtt = nexttbtt;
        
        remainder = (nexttbtt - tsftu) % intval;
        
        nexttbtt = tsftu + remainder;

        countdiff = (oldtbtt - nexttbtt) / intval % dtimperiod;
        
        if (dtimcount > countdiff)
        {
            dtimcount -= countdiff;
        }
        else
        {
            dtimcount += dtimperiod - countdiff;
        }
    }
    else //nexttbtt <= tsftu
    {   
        u_int32_t   countdiff, oldtbtt, remainder;

        oldtbtt = nexttbtt;
        
        remainder = (tsftu - nexttbtt) % intval;
        
        nexttbtt = tsftu - remainder + intval;
        
        countdiff = (nexttbtt - oldtbtt) / intval % dtimperiod;

        if (dtimcount > countdiff)
        {
            dtimcount -= countdiff;
        }
        else
        {
            dtimcount += dtimperiod - countdiff;
        }
    }

    OS_MEMZERO(&bs, sizeof(bs));
    bs.bs_intval = tbtt_timer_period;
    bs.bs_nexttbtt = nexttbtt;
    bs.bs_dtimperiod = dtimperiod*intval;
    bs.bs_nextdtim = bs.bs_nexttbtt + dtimcount*intval;
    bs.bs_cfpperiod = cfpperiod*bs.bs_dtimperiod;
    bs.bs_cfpnext = bs.bs_nextdtim + cfpcount*bs.bs_dtimperiod;
    bs.bs_cfpmaxduration = 0;
    /*
     * Calculate the number of consecutive beacons to miss
     * before taking a BMISS interrupt.  The configuration
     * is specified in TU so we only need calculate based
     * on the beacon interval.  Note that we clamp the
     * result to at most 15 beacons.
     */
    if (sleepduration > intval)
    {
        bs.bs_bmissthreshold = conf->listen_interval * ATH_DEFAULT_BMISS_LIMIT/2;
    }
    else
    {
        bs.bs_bmissthreshold = howmany(conf->bmiss_timeout, tbtt_timer_period);
        if (bs.bs_bmissthreshold > 15)
            bs.bs_bmissthreshold = 15;
        else if (bs.bs_bmissthreshold <= 0)
            bs.bs_bmissthreshold = 1;
    }

#ifdef ATH_BT_COEX
    if (sc->sc_btinfo.bt_on) {
        bs.bs_bmissthreshold = 50;
    }
#endif
    /*
     * Calculate sleep duration.  The configuration is
     * given in ms.  We insure a multiple of the beacon
     * period is used.  Also, if the sleep duration is
     * greater than the DTIM period then it makes senses
     * to make it a multiple of that.
     *
     * XXX fixed at 100ms
     */

    bs.bs_sleepduration =
        roundup(IEEE80211_MS_TO_TU(100), sleepduration);
    if (bs.bs_sleepduration > bs.bs_dtimperiod) {
       bs.bs_sleepduration =
        roundup(bs.bs_dtimperiod, sleepduration);
    }

    /* TSF out of range threshold fixed at 1 second */
    bs.bs_tsfoor_threshold = HAL_TSFOOR_THRESHOLD;
    DPRINTF(sc, ATH_DEBUG_BEACON, " beacon tsf stamp 0x%x:0x%x \n", 
                            LE_READ_4(conf->u.last_tstamp + 4), LE_READ_4(conf->u.last_tstamp));
    DPRINTF(sc, ATH_DEBUG_BEACON, 
            "%s: tsf %llu tsf:tu %u intval %u nexttbtt %u dtim %u nextdtim %u bmiss %u sleep %u cfp:period %u maxdur %u next %u timoffset %u\n"
            , __func__
            , (unsigned long long)tsf, tsftu
            , bs.bs_intval
            , bs.bs_nexttbtt
            , bs.bs_dtimperiod
            , bs.bs_nextdtim
            , bs.bs_bmissthreshold
            , bs.bs_sleepduration
            , bs.bs_cfpperiod
            , bs.bs_cfpmaxduration
            , bs.bs_cfpnext
            , bs.bs_timoffset
        );
        
    ATH_INTR_DISABLE(sc);

    ath_hal_beacontimers(ah, &bs);
    sc->sc_imask |= HAL_INT_BMISS;

    ATH_INTR_ENABLE(sc);


    return  nexttbtt;
}

static void ath_beacon_config_ap_timers(struct ath_softc *sc, u_int32_t nexttbtt,
                                        u_int32_t intval, u_int32_t intval_frac)
{
    u_int32_t nexttbtt_tu8, intval_tu8; /* units of TU/8 */
    struct ath_hal *ah = sc->sc_ah;

    ATH_INTR_DISABLE(sc);
        
    intval_tu8 = (intval & HAL_BEACON_PERIOD) << 3;
    if (nexttbtt == intval) {
        intval |= HAL_BEACON_RESET_TSF;
        intval_tu8 |= HAL_BEACON_RESET_TSF;
    } 

#if ATH_TX_DUTY_CYCLE
    if (sc->sc_tx_dc_enable) {
        /* disable quiet time till we're done */
        ath_hal_setQuiet(ah, 0, 0, 0, 0);
    }
#endif

    /*
     * In AP mode we enable the beacon timers and
     * SWBA interrupts to prepare beacon frames.
     */
    intval |= HAL_BEACON_ENA;
    intval_tu8 |= HAL_BEACON_ENA;
    sc->sc_imask |= HAL_INT_SWBA;   /* beacon prepare */
    ath_beaconq_config(sc);

    /* ath_hal_beaconinit now requires units of TU/8 for nexttbtt */
    nexttbtt_tu8 = nexttbtt << 3;
    ath_hal_beaconinit(ah, nexttbtt_tu8, intval_tu8, intval_frac, HAL_M_HOSTAP);
    DPRINTF(sc, ATH_DEBUG_BEACON, 
            "%s: nexttbtt %u intval %u flags 0x%x \n",__func__,nexttbtt, intval & HAL_BEACON_PERIOD, intval & (~HAL_BEACON_PERIOD));
    sc->sc_bmisscount = 0;
    sc->sc_noise = 0;

#if ATH_TX_DUTY_CYCLE
    if (sc->sc_tx_dc_enable) {
        u_int32_t duration = ((100-sc->sc_tx_dc_active_pct)*sc->sc_tx_dc_period)/100;
        u_int32_t next_start = nexttbtt + (intval & (~HAL_BEACON_PERIOD)) - (sc->sc_tx_dc_period - duration) - 1;
        ath_hal_setQuiet(ah, sc->sc_tx_dc_period, duration, next_start, 1);
    }
#endif
    /* Calling ath_cabq_update() to update the cabq readytime and  other params */
    ath_cabq_update(sc);

    ATH_INTR_ENABLE(sc);
}

#if ATH_SUPPORT_IBSS
static void ath_beacon_config_ibss_timers(struct ath_softc *sc, u_int32_t nexttbtt, u_int32_t intval)
{

    u_int64_t tsf;
    u_int32_t tsftu;
    u_int32_t nexttbtt_tu8, intval_tu8; /* units of TU/8 */
    struct ath_hal *ah = sc->sc_ah;

    ATH_INTR_DISABLE(sc);

    if (nexttbtt == 0)      /* e.g. for ap mode */
        nexttbtt = intval;
    else if (intval)        /* NB: can be 0 for monitor mode */
        nexttbtt = roundup(nexttbtt, intval);

    intval_tu8 = (intval & HAL_BEACON_PERIOD) << 3;
    if (nexttbtt == intval) {
        intval |= HAL_BEACON_RESET_TSF;
        intval_tu8 |= HAL_BEACON_RESET_TSF;
    }
    /*
     * Pull nexttbtt forward to reflect the current
     * TSF .
     */

    tsf = ath_hal_gettsf64(ah);
    
    tsftu = TSF_TO_TU(tsf>>32, tsf);

    if (intval == 0) intval = ATH_DEFAULT_BINTVAL;

    if (nexttbtt > tsftu)
    {
        u_int32_t remainder = (nexttbtt - tsftu) % intval;
        nexttbtt = tsftu + remainder;
    }
    else // nextbtt <= tsftu
    {
        u_int32_t remainder = (tsftu - nexttbtt) % intval;
        nexttbtt = tsftu - remainder + intval;
    }

    DPRINTF(sc, ATH_DEBUG_BEACON, "%s:IBSS nexttbtt %u intval %u flags 0x%x \n",
                   __func__,nexttbtt, intval & HAL_BEACON_PERIOD, intval & (~HAL_BEACON_PERIOD));

    /*
     * In IBSS mode enable the beacon timers but only
     * enable SWBA interrupts if we need to manually
     * prepare beacon frames.  Otherwise we use a
     * self-linked tx descriptor and let the hardware
     * deal with things.
     */
    intval |= HAL_BEACON_ENA;
    intval_tu8 |= HAL_BEACON_ENA;
    if (!sc->sc_hasveol)
        sc->sc_imask |= HAL_INT_SWBA;
    ath_beaconq_config(sc);

    /* ath_hal_beaconinit now requires units of TU/8 for nexttbtt */
    nexttbtt_tu8 = nexttbtt << 3;
    ath_hal_beaconinit(ah, nexttbtt_tu8, intval_tu8, 0, HAL_M_IBSS);
    sc->sc_bmisscount = 0;
    sc->sc_noise = 0;
    ATH_INTR_ENABLE(sc);
   /*
    * When using a self-linked beacon descriptor in
    * ibss mode load it once here.
    */
    if (sc->sc_hasveol)
        ath_beacon_start_adhoc(sc, 0);


}
#endif


/*
 * Function to collect beacon rssi data and resync beacon if necessary
 */
void
ath_beacon_sync(ath_dev_t dev, int if_id)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    HAL_OPMODE av_opmode;


    ATH_FUNC_ENTRY_VOID(sc);

    if ( if_id != ATH_IF_ID_ANY )
    {
        av_opmode = sc->sc_vaps[if_id]->av_opmode;
        if ((av_opmode == HAL_M_STA) &&
            (sc->sc_opmode == HAL_M_HOSTAP)) {
            /*
             * no need to program beacon timers for a STA vap
             * when the chip is operating in AP mode (WDS,Direct Connect).
             */
            DPRINTF(sc, ATH_DEBUG_BEACON,"%s\n", "chip op mode is AP, ignore beacon config request from sta vap");
            return;
        }
    }
    /*
     * Resync beacon timers using the tsf of the
     * beacon frame we just received.
     */
    ATH_PS_WAKEUP(sc);
    ath_beacon_config(sc,ATH_BEACON_CONFIG_REASON_RESET,ATH_IF_ID_ANY);
    ATH_PS_SLEEP(sc);
}

/******************************************************************************/
/*!
**  \brief Task for Beacon Miss Interrupt
**
**  This tasklet will notify the upper layer when a beacon miss interrupt
**  has occurred.  The upper layer will perform any required processing.  If
**  the upper layer does not supply a function for this purpose, no other action
**  is taken.
**
**  \param sc Pointer to ATH object, "this" pointer
**
**  \return N/A
*/

void
ath_bmiss_tasklet(struct ath_softc *sc)
{
    DPRINTF(sc, ATH_DEBUG_ANY, "%s\n", __func__);
    if (sc->sc_ieee_ops->notify_beacon_miss)
        sc->sc_ieee_ops->notify_beacon_miss(sc->sc_ieee);
}

/*
 * Hardware beacon processing
 */

/*
 * Query if hw beacon processing is enabled.
 *
 * If hw beacon processing is in use, then beacons are not passed
 * to software unless there is a change in the beacon
 * (i.e. the hw computed CRC changes).
 *
 * Hw beacon processing is only active if
 * - there is a single active STA VAP
 * - the hardware supports hw beacon processing (Osprey and later)
 * - software is configured for hw beacon processing
 */
int ath_get_hwbeaconproc_active(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    /* Return 1 if hw beacon processing can be enabled */
    if (ath_hal_hashwbeaconprocsupport(ah) && !sc->sc_config.swBeaconProcess) {
        /* Hw beacon processing only supported for STA (not IBSS)
         * and one STA vap.
         * */
        if ((sc->sc_opmode == HAL_M_STA) &&
                atomic_read(&sc->sc_nsta_vaps_up) == 1 &&
                atomic_read(&sc->sc_nap_vaps_up) == 0)
        {
#if ATH_SUPPORT_MCI
            if (ath_hal_hasmci(sc->sc_ah) && sc->sc_btinfo.mciHWBcnProcOff) {
                return 0;
            }
#endif
            return 1;
        }
    }
    return 0;
}

void ath_hwbeaconproc_enable(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;

    /* Reset hardware beacon CRC.
     *
     * This is necessary to make sure at least one beacon
     * is passed to software upon returning to the home
     * channel after a background scan. The UMAC scan
     * logic needs to see one beacon to make sure the
     * TSF is synchronized.
     */
    ath_hal_reset_hw_beacon_proc_crc(ah);

    /* Enable interrupts necessary for hw beacon processing */
    ath_hal_intrset(ah, 0);
    sc->sc_imask |= HAL_INT_TIM | HAL_INT_DTIM;
    ath_hal_intrset(ah, sc->sc_imask);
}

void ath_hwbeaconproc_disable(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;

    /* Disable interrupts related to hw beacon processing */
    ath_hal_intrset(ah, 0);
    sc->sc_imask &= ~(HAL_INT_TIM | HAL_INT_DTIM);
    ath_hal_intrset(ah, sc->sc_imask);
}


/*
 * Hardware beacon RSSI and RSSI threshold interrupt management.
 */

void ath_hw_beacon_rssi_threshold_enable(ath_dev_t dev,
                                        u_int32_t rssi_threshold)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ATH_PS_WAKEUP(sc);

    /* Set RSSI threshold. The hardware will interrupt  */
    ath_hal_set_hw_beacon_rssi_threshold(ah, rssi_threshold);

    /* Enable interrupts related to beacon rssi threshold */
    ath_hal_intrset(ah, 0);
    sc->sc_imask |= HAL_INT_BRSSI;
    ath_hal_intrset(ah, sc->sc_imask);

    ATH_PS_SLEEP(sc);
}

void ath_hw_beacon_rssi_threshold_disable(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_hal *ah = sc->sc_ah;

    ATH_PS_WAKEUP(sc);

    ath_hal_set_hw_beacon_rssi_threshold(ah, 0);

    /* Disable interrupts related to beacon rssi threshold */
    ath_hal_intrset(ah, 0);
    sc->sc_imask &= ~HAL_INT_BRSSI;
    ath_hal_intrset(ah, sc->sc_imask);

    ATH_PS_SLEEP(sc);
}

u_int32_t ath_hw_beacon_rssi(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;

    return ath_hal_get_hw_beacon_rssi(ah);
}


void ath_hw_beacon_rssi_reset(struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;

    /* Reset average beacon RSSI computed by hardware.
     * This should be done when joining a new AP.
     */
    ath_hal_reset_hw_beacon_rssi(ah);
}

/******************************************************************************/
/*!
**  \brief Task for Beacon RSSI threshold Interrupt
**
**  This tasklet will notify the upper layer when a beacon RSSI threshold interrupt
**  has occurred.  The upper layer will perform any required processing.  If
**  the upper layer does not supply a function for this purpose, no other action
**  is taken.
**
**  \param sc Pointer to ATH object, "this" pointer
**
**  \return N/A
*/

void
ath_brssi_tasklet(struct ath_softc *sc)
{
    DPRINTF(sc, ATH_DEBUG_ANY, "%s\n", __func__);

    /* Notify UMAC */
    if (sc->sc_ieee_ops->notify_beacon_rssi)
        sc->sc_ieee_ops->notify_beacon_rssi(sc->sc_ieee);
}

void ath_beacon_attach(struct ath_softc *sc)
{
    ATH_BEACON_LOCK_INIT(sc);

    ath_notify_tx_beacon_attach(sc);
}

void ath_beacon_detach(struct ath_softc *sc)
{
    ath_notify_tx_beacon_detach(sc);

    ATH_BEACON_LOCK_DESTROY(sc);
}


#if UMAC_SUPPORT_WNM

/******************************************************************************/
/*!
**  \brief Allocate and setup an initial TIM frame.
**
**  Allocate a tim state variable for a specific VAP instance created on
**  the ATH interface.
**
**  \param sc Pointer to ATH object ("this" pointer).
**  \param if_id Index of the specific VAP of interest.
**
**  \return -EINVAL if there is no function in the upper layer assigned to
**  \return        tim ransmission
**  \return -ENOMEM if no wbuf is available
**  \return   0     for success
*/

int
ath_timbcast_alloc(struct ath_softc *sc, int if_id)
{
    struct ath_vap *avp;
    struct ath_buf *bf;
    wbuf_t wbuf;
    int i;
    ieee80211_tx_control_t *txctl;
    u_int16_t *ratecode = NULL;
    u_int8_t *rix = NULL;

    /*
    ** Code Begins
    */

    avp = sc->sc_vaps[if_id];
    ASSERT(avp);

    /* Allocate a Tim descriptor if we haven't done so. */
    if(!avp->av_tbuf_highrate)
    {
        /*
         * Allocate TIM state for hostap.  We know
         * a buffer is available.
         */

        avp->av_tbuf_highrate = TAILQ_FIRST(&sc->sc_tbuf);
        TAILQ_REMOVE(&sc->sc_tbuf, avp->av_tbuf_highrate, bf_list);

        /* set up this buffer */
        ATH_TXBUF_RESET(avp->av_tbuf_highrate, sc->sc_num_txmaps);
    }

    if(!avp->av_tbuf_lowrate)
    {
        /*
         * Allocate TIM state for hostap.  We know
         * a buffer is available.
         */

        avp->av_tbuf_lowrate = TAILQ_FIRST(&sc->sc_tbuf);
        TAILQ_REMOVE(&sc->sc_tbuf, avp->av_tbuf_lowrate, bf_list);

        /* set up this buffer */
        ATH_TXBUF_RESET(avp->av_tbuf_lowrate, sc->sc_num_txmaps);

    }

    for (i = 0; i < MAX_TIMBUFFERS_PER_VAP; i++) {
        /*
         * release the previous TIM frame , if it already exists.
         */
        if (i == 0) {
            bf = avp->av_tbuf_highrate;
            txctl = &avp->av_timtxctl_highrate;
            ratecode = &avp->av_tim_highrate_code;
			rix = &avp->av_tim_highrix;
        } else {
            bf = avp->av_tbuf_lowrate;
            txctl = &avp->av_timtxctl_lowrate;
            ratecode = &avp->av_tim_lowrate_code;
			rix = &avp->av_tim_lowrix;
        }
        if (bf->bf_mpdu != NULL)
        {
            ieee80211_tx_status_t tx_status;

            wbuf = (wbuf_t)bf->bf_mpdu;
            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            tx_status.flags = 0;
            tx_status.retries = 0;
#ifdef  ATH_SUPPORT_TxBF
            tx_status.txbf_status = 0;
#endif
            sc->sc_ieee_ops->tx_complete(wbuf, &tx_status, 0);
            bf->bf_mpdu = NULL;
        }
        /*
         * NB: the TIM data buffer must be 32-bit aligned;
        * we assume the wbuf routines will return us something
         * with this alignment (perhaps should assert).
         */
        if (!sc->sc_ieee_ops->get_timbcast)
        {
            /* Protocol layer doesn't support beacon generation for host driver */
            return -EINVAL;
        }

        wbuf = sc->sc_ieee_ops->get_timbcast(sc->sc_ieee, if_id, 
                        ((bf == avp->av_tbuf_highrate) ? 1 : 0),  txctl);
        if (wbuf == NULL)
        {
            DPRINTF(sc, ATH_DEBUG_BEACON, "%s: cannot get wbuf\n",
                    __func__);
#ifdef QCA_SUPPORT_CP_STATS
            pdev_cp_stats_be_nobuf_inc(sc->sc_pdev, 1);
#else
            sc->sc_stats.ast_be_nobuf++;
#endif
            return -ENOMEM;
        }

        bf->bf_buf_addr[0] = wbuf_map_single(sc->sc_osdev, wbuf,
                                BUS_DMA_TODEVICE,
                                OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        bf->bf_mpdu = wbuf;
        if(txctl->min_rate) { 
            *ratecode = ath_rate_findrc((ath_dev_t) sc, txctl->min_rate); /* get the rate code */
			*rix = ath_rate_findrix(sc->sc_rates[sc->sc_curmode], txctl->min_rate);
        }
    }

    return 0;
}

/******************************************************************************/
/*!
**  \brief Reclaim Timbcast resources and return buffer to the pool.
**
**  Checks the VAP to put the Tim frame buffer back to the ATH object
**
**  \param sc Pointer to ATH object, "this" pointer.
**  \param avp Pointer to VAP object that sent the beacon
**
**  \return N/A
*/

void
ath_timbcast_return(struct ath_softc *sc, struct ath_vap *avp)
{
    if (avp->av_tbuf_highrate != NULL)
    {
        struct ath_buf *bf;

        bf = avp->av_tbuf_highrate;
        if (bf->bf_mpdu != NULL)
        {
            wbuf_t wbuf = (wbuf_t)bf->bf_mpdu;
            ieee80211_tx_status_t tx_status;

            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            tx_status.flags = 0;
            tx_status.retries = 0;
#ifdef  ATH_SUPPORT_TxBF
            tx_status.txbf_status = 0;
#endif
            sc->sc_ieee_ops->tx_complete(wbuf, &tx_status, 0);
            bf->bf_mpdu = NULL;
        }
        TAILQ_INSERT_TAIL(&sc->sc_tbuf, bf, bf_list);

        avp->av_tbuf_highrate = NULL;
    }
    if (avp->av_tbuf_lowrate != NULL)
    {
        struct ath_buf *bf;

        bf = avp->av_tbuf_lowrate;
        if (bf->bf_mpdu != NULL)
        {
            wbuf_t wbuf = (wbuf_t)bf->bf_mpdu;
            ieee80211_tx_status_t tx_status;

            wbuf_unmap_single(sc->sc_osdev, wbuf, BUS_DMA_TODEVICE,
                              OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
            tx_status.flags = 0;
            tx_status.retries = 0;
#ifdef  ATH_SUPPORT_TxBF
            tx_status.txbf_status = 0;
#endif
            sc->sc_ieee_ops->tx_complete(wbuf, &tx_status, 0);
            bf->bf_mpdu = NULL;
        }
        TAILQ_INSERT_TAIL(&sc->sc_tbuf, bf, bf_list);

        avp->av_tbuf_lowrate = NULL;
    }
}

/******************************************************************************/
/*!
**  \brief Setup the TIM frame for transmit.
**
**  Associates the beacon frame buffer with a transmit descriptor.  Will set
**  up all required antenna switch parameters, rate codes, and channel flags.
**  Beacons are always sent out at the lowest rate, and are not retried.
**
**  \param sc Pointer to ATH object ("this" pointer)
**  \param avp Pointer to VAP object (ieee802.11 layer object)for the beacon
**  \param bf Pointer to ATH buffer containing the beacon frame.
**  \return N/A
*/

static void
ath_timbcast_setup(struct ath_softc *sc, struct ath_vap *avp, struct ath_buf *bf, u_int32_t smart_ant_bcn_txant)
{
    wbuf_t wbuf = (wbuf_t)bf->bf_mpdu;
    struct ath_hal *ah = sc->sc_ah;
    struct ath_desc *ds;
    int flags;
    const HAL_RATE_TABLE *rt;
    u_int8_t rix, rate;
    int ctsrate = 0;
    int ctsduration = 0;
    HAL_11N_RATE_SERIES  series[4];
    ieee80211_tx_control_t *txctl;
    int ratecode;
    u_int32_t smartAntenna = 0;
#if UNIFIED_SMARTANTENNA
    u_int32_t antenna_array[4]= {0,0,0,0}; /* initilize to zero */
#endif  

    /*
    ** Code Begins
    */

    DPRINTF(sc, ATH_DEBUG_BEACON_PROC, "%s: m %pK len %u\n",
            __func__, wbuf, wbuf_get_pktlen(wbuf));

    /* setup descriptors */
    ds = bf->bf_desc;

    flags = HAL_TXDESC_NOACK;
#ifdef ATH_SUPERG_DYNTURBO
    if (sc->sc_dturbo_switch)
        flags |= HAL_TXDESC_INTREQ;
#endif


    ath_hal_setdesclink(ah, ds, 0);

    /*
     * Calculate rate code.
     */
    rix = sc->sc_minrateix;
    rt = sc->sc_currates;

    if (bf == avp->av_tbuf_highrate) {
        txctl = &avp->av_timtxctl_highrate;
        ratecode = avp->av_tim_highrate_code;
		rix = avp->av_tim_highrix;
    } else {
        txctl = &avp->av_timtxctl_lowrate;
        ratecode = avp->av_tim_lowrate_code;
		rix = avp->av_tim_lowrix;
    }
    /*
     * if the vap requests a specific rate for beacon then
     * use it insted of minrateix. 
     */
    if (ratecode) {
        rate = ratecode;
    } else {
       rate = rt->info[rix].rate_code;
    }
    if (txctl->shortPreamble)
        rate |= rt->info[rix].shortPreamble;


#ifndef REMOVE_PKT_LOG
    bf->bf_vdata = wbuf_header(wbuf);
#endif

    /* NB: beacon's BufLen must be a multiple of 4 bytes */
    bf->bf_buf_len[0] = roundup(wbuf_get_pktlen(wbuf), 4);

    ath_hal_set11n_txdesc(ah, ds
                          , wbuf_get_pktlen(wbuf) + IEEE80211_CRC_LEN /* frame length */
                          , HAL_PKT_TYPE_NORMAL                 /* Atheros packet type */
                          , avp->av_btxctl.txpower              /* txpower XXX */
                          , HAL_TXKEYIX_INVALID                 /* no encryption */
                          , HAL_KEY_TYPE_CLEAR                  /* no encryption */
                          , flags                               /* no ack, veol for beacons */
                          );

    ath_hal_filltxdesc(ah, ds
                       , bf->bf_buf_addr           /* buffer address */
                       , bf->bf_buf_len            /* buffer length */
                       , 0                                      /* descriptor id */
                       , sc->sc_bhalq                           /* QCU number */
                       , HAL_KEY_TYPE_CLEAR                     /* key type */
                       , AH_TRUE                                /* first segment */
                       , AH_TRUE                                /* last segment */
                       , ds                                     /* first descriptor */
                       );

    OS_MEMZERO(series, sizeof(HAL_11N_RATE_SERIES) * 4);
    series[0].Tries = 1;
    series[0].Rate = rate;
    series[0].rate_index = rix;
    series[0].tx_power_cap = IS_CHAN_2GHZ(&sc->sc_curchan) ?
        sc->sc_config.txpowlimit2G : sc->sc_config.txpowlimit5G;
    if (avp->av_txpow_mgt_frm[IEEE80211_FC0_SUBTYPE_BEACON >> IEEE80211_FC0_SUBTYPE_SHIFT] != 0xFF){
        series[0].tx_power_cap = ( series[0].tx_power_cap <= avp->av_txpow_mgt_frm[IEEE80211_FC0_SUBTYPE_BEACON >> IEEE80211_FC0_SUBTYPE_SHIFT])?
            series[0].tx_power_cap  : avp->av_txpow_mgt_frm[IEEE80211_FC0_SUBTYPE_BEACON >> IEEE80211_FC0_SUBTYPE_SHIFT];
    }
    if (avp->av_txpow_frm[IEEE80211_FC0_TYPE_MGT][IEEE80211_FC0_SUBTYPE_BEACON >> IEEE80211_FC0_SUBTYPE_SHIFT] != 0xFF){
        series[0].tx_power_cap = ( series[0].tx_power_cap <= avp->av_txpow_frm[IEEE80211_FC0_TYPE_MGT][IEEE80211_FC0_SUBTYPE_BEACON >> IEEE80211_FC0_SUBTYPE_SHIFT])?
            series[0].tx_power_cap  : avp->av_txpow_frm[IEEE80211_FC0_TYPE_MGT][IEEE80211_FC0_SUBTYPE_BEACON >> IEEE80211_FC0_SUBTYPE_SHIFT];
    }
#if WLAN_SUPPORT_GREEN_AP
    series[0].ch_sel = ath_txchainmask_reduction(sc,
              ath_green_ap_is_powersave_on(sc) ? ATH_CHAINMASK_ONE_CHAIN :sc->sc_tx_chainmask,
            rate);
#else
    series[0].ch_sel = ath_txchainmask_reduction(sc, sc->sc_tx_chainmask, rate);
#endif

    series[0].RateFlags = (ctsrate) ? HAL_RATESERIES_RTS_CTS : 0;

    smartAntenna = SMARTANT_INVALID;

#if UNIFIED_SMARTANTENNA
    antenna_array[0] = smart_ant_bcn_txant;
    ath_hal_set11n_ratescenario(ah, ds, ds, 0, ctsrate, ctsduration, series, 4, 0, smart_ant_bcn_txant, antenna_array);
#else
    ath_hal_set11n_ratescenario(ah, ds, ds, 0, ctsrate, ctsduration, series, 4, 0, smartAntenna);
#endif        

    /* NB: The desc swap function becomes void, 
     * if descriptor swapping is not enabled
     */
    ath_desc_swap(ds);
}
#endif
