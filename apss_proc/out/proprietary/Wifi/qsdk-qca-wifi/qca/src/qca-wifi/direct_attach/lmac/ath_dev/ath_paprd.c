/* Copyright (c) 2011-2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

/*! \file ath_paprd.c
**  \brief ATH CF-END Processing
*/

#include "ath_internal.h"
#include "ratectrl.h"
#include "wbuf.h"

#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#endif

#if ATH_SUPPORT_PAPRD

extern unsigned int paprd_enable;

#define ATH_PAPRD_BUF_COUNT         1
#define ATH_PAPRD_COUNT_TIMEOUT     (10 * 1000) /* usec */
#define ATH_WAITTIME_QUANTUM        10
#define ATH_PAPRD_RETRAIN_MAXCOUNT  5
#define ATH_PAPRD_FAILED_THRESHOLD  100
int ath_paprdq_config(struct ath_softc *sc);
int ath_sendpaprd_single(struct ath_softc *sc,  int count, int chain_num); 
void ath_paprd_send(struct ath_softc *sc,  int count); 
void ath_paprd_set_enable(struct ath_softc *sc,  int enable);
int ath_paprd_get_enable(struct ath_softc *sc); 
void ath_paprd_setrate(struct ath_softc *sc,  int rate); 
int ath_paprd_getrate(struct ath_softc *sc); 
void
ath_tx_paprd_draintxq(struct ath_softc *sc);

int ath_apply_paprd_table(struct ath_softc *sc)
{
    int      num_chain;
    
    if (sc->sc_paprd_enable == 0) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: PAPRD Not ENABLED %d\n", __func__, __LINE__, sc->sc_curchan.paprd_done);
        return -1;
    }
    if (sc->sc_curchan.paprd_done == 0)
    {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: PAPRD Not complete %d\n", __func__, __LINE__, sc->sc_curchan.paprd_done);
        return -1;
    }
    sc->sc_paprd_cal_started = 0;
    sc->sc_paprd_failed_count = 0;
    DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: paprd_done %d write %d\n", __func__, __LINE__, sc->sc_curchan.paprd_done, sc->sc_curchan.paprd_table_write_done);
    for (num_chain = 0; num_chain < 3; num_chain++) {
        if (!((1 << num_chain) & sc->sc_tx_chainmask)) {
            continue;
        }
        ath_hal_paprd_papulate_table(sc->sc_ah, &sc->sc_curchan, num_chain);
    }
    //ath_hal_paprd_enable() should set chan->paprd_table_write_done = 1
    ath_hal_hwgreentx_set_pal_spare(sc->sc_ah, 1);
    ath_hal_paprd_enable(sc->sc_ah, AH_TRUE, &sc->sc_curchan);

    return 0;
}

int ath_paprd_cal(struct ath_softc *sc)
{
    int      retval = -1, i, num_chain;

    DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: PAPRD enable %d channel %d\n", __func__, __LINE__, sc->sc_paprd_enable, sc->sc_curchan.channel);
    if (sc->sc_paprd_enable == 0) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: PAPRD Not ENABLED %d\n", __func__, __LINE__, sc->sc_curchan.paprd_done);
        return -1;
    }
    if (sc->sc_paprd_failed_count > ATH_PAPRD_FAILED_THRESHOLD)
    {
        /* Intentional print */
        printk("%s sc %pK chan_mask %d PAPRD excessive failure disabling PAPRD now\n", __func__, sc, sc->sc_tx_chainmask);
        sc->sc_paprd_enable = 0;
        return -1;
        
    }
    if (sc->sc_curchan.paprd_done)
    {
        /* PAPRD is done on current channel, return */
        DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: PAPRD Already done channel %d\n", __func__, __LINE__, sc->sc_curchan.channel);
        return 0;
    }
    sc->sc_curchan.paprd_done = 0;
    sc->sc_curchan.paprd_table_write_done = 0;
    sc->sc_paprd_abort = 0;
    if (sc->sc_paprd_lastchan_num == sc->sc_curchan.channel)
    {
        /* PAPRD is done on current channel, return */
        DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: PAPRD Already done channel %d\n", __func__, __LINE__, sc->sc_curchan.channel);
        sc->sc_curchan.paprd_done = 1;
        return 0;
    }
    DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: PAPRD start at chain %d\n", __func__, __LINE__, sc->sc_paprd_chain_num);
    if (sc->sc_paprd_done_chain_mask == 0) {
        /* PAPRD Chain MASK is reset Start with Chain 0 */
        sc->sc_paprd_chain_num = 0;
        DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: PAPRD start at chain %d\n", __func__, __LINE__, sc->sc_paprd_chain_num);
    }
    num_chain = sc->sc_paprd_chain_num;

    if (ath_hal_Dragonfly(sc->sc_ah) || ath_hal_Honeybee20(sc->sc_ah)) {
	    if(!sc->sc_paprd_ontraining) {
            ath_hal_paprd_enable(sc->sc_ah, AH_FALSE, &sc->sc_curchan);
            ath_hal_hwgreentx_set_pal_spare(sc->sc_ah, 0);
            if (ath_hal_paprd_init_table(sc->sc_ah, &sc->sc_curchan))
            {
                DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: ath_hal_paprd_init_table "
                    "return not 0 failed %d\n", __func__, __LINE__, sc->sc_paprd_failed_count);
                retval = -1;
                goto FAILED;      
            }
            if (!((1 << num_chain) & sc->sc_tx_chainmask)) {
                goto DONE;
            }
            sc->sc_paprd_ontraining = 1;
        }
	} else {
	    ath_hal_paprd_enable(sc->sc_ah, AH_FALSE, &sc->sc_curchan);
        ath_hal_hwgreentx_set_pal_spare(sc->sc_ah, 0);
        if (ath_hal_paprd_init_table(sc->sc_ah, &sc->sc_curchan))
        {
            DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: ath_hal_paprd_init_table "
                "return not 0 failed %d\n", __func__, __LINE__, sc->sc_paprd_failed_count);
            retval = -1;
            goto FAILED;
        }
        if (!((1 << num_chain) & sc->sc_tx_chainmask)) {
            goto DONE;
        }
	}
    

    if(!ath_hal_paprd_thermal_send(sc->sc_ah)) {
        sc->sc_paprd_thermal_packet_not_sent = 1;
        /* Return here to wait at least one packet was sent on only one chain. */
        retval = -1;
        sc->sc_paprd_cal_started = 0;
        DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: failed count %d!!\n", __func__, __LINE__, sc->sc_paprd_failed_count);
        goto FAILED;
    }

    ath_hal_paprd_setup_gain_table(sc->sc_ah, num_chain);
    for (i = 0; i < sc->sc_paprd_bufcount; i++) {
        if ((retval = ath_sendpaprd_single(sc, sc->sc_paprd_rate, num_chain)) !=0) {
            DPRINTF(sc, ATH_DEBUG_UNMASKABLE, "%s[%d]: failed count %d!!\n", __func__, __LINE__, sc->sc_paprd_failed_count);
            goto FAILED;
        }
    }
DONE: 
    return retval;   
FAILED: 
    sc->sc_paprd_failed_count++;
    DPRINTF(sc, ATH_DEBUG_UNMASKABLE, "%s[%d]: failcount %d failed!!\n", __func__, __LINE__, sc->sc_paprd_failed_count);
    ath_hal_hwgreentx_set_pal_spare(sc->sc_ah, 1);
    return retval;
}

void ath_paprd_send(struct ath_softc *sc,  int count) 
{
    if ( count == 0)
    {
        sc->sc_paprd_bufcount = count;
    }
    else {
        ath_hal_paprd_init_table(sc->sc_ah, &sc->sc_curchan);
        if (ath_hal_Jupiter(sc->sc_ah)){
            sc->sc_paprd_rate = 0xb; /* legacy 6 M*/
        } else if (ath_hal_Dragonfly(sc->sc_ah) || ath_hal_Honeybee20(sc->sc_ah)) {
            sc->sc_paprd_rate = 0x80; /* MCS0 */
        } else {
            sc->sc_paprd_rate = 0xc; /* legacy 54Mbps */
        }
        sc->sc_paprd_bufcount = count;
        DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: PAPRD pkt count = %x\n", __func__, __LINE__, count);
        sc->sc_paprd_done_chain_mask= 0;
        ath_paprd_cal(sc);
    }
}

void ath_paprd_set_enable(struct ath_softc *sc,  int enable) 
{
    if (sc->sc_reg_parm.paprdEnable) {
        sc->sc_paprd_enable = enable;
    }
}
int ath_paprd_get_enable(struct ath_softc *sc) 
{
    return sc->sc_paprd_enable;
}
void ath_paprd_setrate(struct ath_softc *sc,  int rate) 
{
    sc->sc_paprd_rate = rate;
}
int ath_paprd_getrate(struct ath_softc *sc) 
{
    return sc->sc_paprd_rate;
}

int ath_tx_paprd_init(struct ath_softc *sc)
{
    int              error = 0;
    struct           ath_buf *bf;
    wbuf_t           wbuf;
    struct           ath_desc *ds;
    u_int32_t        result = 0;
    struct ath_phys_mem_context *ctx;

#if 0
    {
        int debug_flag =  1 << ATH_DEBUG_CALIBRATE;
        ath_set_config(sc, ATH_PARAM_ATH_DEBUG, &debug_flag); 
        printk("%s sc %pK ---Enable PAPRD DEBUG ---- fail count %d\n", __func__, sc, sc->sc_paprd_failed_count);
    }
#endif

    if ((sc->sc_reg_parm.paprdEnable == 0) || (paprd_enable == 0) ) {
        DPRINTF(sc, ATH_DEBUG_UNMASKABLE, "%s PAPRD disabled in Registry \n",
                __func__);
        return 0;
    }

    /* initialize only for ar9300 chips */
    if ( ath_hal_getcapability(sc->sc_ah, HAL_CAP_PAPRD_ENABLED, 0, &result) != HAL_OK) {
        DPRINTF(sc, ATH_DEBUG_UNMASKABLE, "%s sc %pK PAPRD disabled in HAL\n",
                __func__, sc);
        return 0;
    }
    DPRINTF(sc, ATH_DEBUG_UNMASKABLE, "%s sc %pK PAPRD Enabled\n",
            __func__, sc);
    sc->sc_paprd_enable = 1;
    sc->sc_paprd_chain_num = 0;
    if (ath_hal_Jupiter(sc->sc_ah)){
        sc->sc_paprd_rate = 0xb; /* legacy 6 M*/
    } else if(ath_hal_Dragonfly(sc->sc_ah) || ath_hal_Honeybee20(sc->sc_ah)){
        sc->sc_paprd_rate = 0x80; /* MCS0 */
    } else {
        sc->sc_paprd_rate = 0xc; /* legacy 54Mbps */
    }
    sc->sc_paprd_bufcount = ATH_PAPRD_BUF_COUNT;

    /* initialize the spinlock */
    ATH_PAPRD_LOCK_INIT(sc);
    
    /* we should check if paprd flag installed here, but by default, this is
     * disable at init time, so not doing init would break the dynamic
     * enabling process, so do not check any enabled flag to do
     * initialization
     */


    error = ath_descdma_setup(sc, &sc->sc_paprddma, &sc->sc_paprdbuf,
            "paprd", ATH_PAPRD_BUF_COUNT * AH_MAX_CHAINS, ATH_TXDESC, 1, ATH_FRAG_PER_MSDU);

    if (error != 0) {
        DPRINTF(sc, ATH_DEBUG_CALIBRATE,
                "%s failed to allocate PAPRD tx descriptors: %d\n",
                __func__, error);
        sc->sc_paprd_enable = 0;
        goto bad;
    }

    /* Format frame */
    if (ath_hal_Dragonfly(sc->sc_ah) || ath_hal_Honeybee20(sc->sc_ah)) {
	    wbuf = wbuf_alloc(sc->sc_osdev, WBUF_TX_CTL, 2200);
	} else {
	    wbuf = wbuf_alloc(sc->sc_osdev, WBUF_TX_CTL, 2000);
	}
    
    if (wbuf == NULL) {
        error = -ENOMEM;
        DPRINTF(sc, ATH_DEBUG_CALIBRATE,
                "%s failed to allocate PAPRD wbuf\n",
                __func__);
        /* ath_tx_paprd_cleanup() will be called in ath_tx_cleanup() */
        //ath_tx_paprd_cleanup(sc);
        sc->sc_paprd_enable = 0;
        //goto free_desc;
        goto bad;
    }
   
    if (ath_hal_Dragonfly(sc->sc_ah) || ath_hal_Honeybee20(sc->sc_ah)) {
        wbuf_set_len(wbuf, 2000);
        wbuf_set_pktlen(wbuf, 2000);
    } else { 
        wbuf_set_len(wbuf, 1800);
        wbuf_set_pktlen(wbuf, 1800);
    }
    
    /* Just use same PAPRD wbuf to TX PAPRD frame as content of PAPRD frame is junk, only needed to train  PAPRD */
    TAILQ_FOREACH(bf, &sc->sc_paprdbuf, bf_list){
        ATH_TXBUF_RESET(bf, sc->sc_num_txmaps);
        
        if (bf) {
            DPRINTF(sc, ATH_DEBUG_CALIBRATE,
                    "%s * allocated paprd pointer at %pK \n", __func__, bf);
        }

        bf->bf_frmlen = wbuf_get_pktlen(wbuf);
        bf->bf_buf_len[0] = bf->bf_frmlen;
        bf->bf_mpdu = wbuf;
        bf->bf_isdata = 1;
        bf->bf_lastfrm = bf;
        bf->bf_lastbf = bf->bf_lastfrm; /* one single frame */
        bf->bf_isaggr  = 0;
        bf->bf_isbar  = bf->bf_isampdu = 0;
        bf->bf_next = NULL;
        bf->bf_flags =  HAL_TXDESC_CLRDMASK | HAL_TXDESC_NOACK;
        bf->bf_state.bfs_ispaprd = 0;
#ifndef REMOVE_PKT_LOG
        bf->bf_vdata = wbuf_header(wbuf);
#endif

        bf->bf_buf_addr[0] = wbuf_map_single(
                        sc->sc_osdev,
                        wbuf,
                        BUS_DMA_TODEVICE,
                        OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
        ctx = (struct ath_phys_mem_context *)OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext);
        //printk("%s[%d] buf %pK, seg %d, len %d\n", __func__, __LINE__,
        //                                    bf->bf_buf_addr[0],
         //                                   ctx->dm_nseg,
         //                                   ctx->dm_size);

        /* setup descriptor */
        ds = bf->bf_desc;
        ds->ds_link = 0;
        ds->ds_data = bf->bf_buf_addr[0];
#ifndef REMOVE_PKT_LOG
        ds->ds_vdata = wbuf_header(wbuf);
#endif

        bf->bf_flags = 0;
        //sc->sc_debug = ATH_DEBUG_FATAL;
        //dump_desc(sc, bf, bf->bf_qnum);
    }
    return 0;

    /* free all the resources and then clear the enabled flag, so that no
     * one uses
     */
#if 0
free_desc:
    if (sc->sc_paprddma.dd_desc_len != 0)
        ath_descdma_cleanup(sc, &sc->sc_paprddma, &sc->sc_paprdbuf);
    ATH_PAPRD_LOCK_DESTROY(sc);
#endif
bad:
    /* make sure that feature itself disbaled by default when not able to
     * initialize
     */
    return error;
}


int ath_sendpaprd_single(struct ath_softc *sc,  int count, int chain_num) /* sc->sc_myaddr */
{
    struct ath_buf       *bf;
    struct ath_desc      *ds;
    ath_bufhead          bf_head;
    u_int8_t             *bssid =  sc->sc_myaddr;
    //struct ath_node      *an = sc->sc_ieee_ops->paprd_get_node(sc->sc_ieee, &sc->sc_paprd_rate);
    struct ieee80211_frame *hdr=NULL;
    u_int32_t smartAntenna = 0;

#if UNIFIED_SMARTANTENNA
        u_int32_t antenna_array[4]= {0,0,0,0}; /* initilize to zero */
#endif   
    ATH_PAPRD_LOCK(sc);
    bf = TAILQ_FIRST(&sc->sc_paprdbuf);
    if (NULL == bf) {
        DPRINTF(sc, ATH_DEBUG_CALIBRATE,
            "%s paprd buffer seems NULL, last free has some bug bf %pK \n",
            __func__, bf);
        ATH_PAPRD_UNLOCK(sc);
        return -1;
    }
    TAILQ_REMOVE(&sc->sc_paprdbuf, bf, bf_list);
    ATH_PAPRD_UNLOCK(sc);

    ASSERT (! bf->bf_state.bfs_ispaprd);   
    
    TAILQ_INIT(&bf_head);
    TAILQ_INSERT_TAIL(&bf_head, bf, bf_list);
    ds = bf->bf_desc;
    bf->bf_node =  NULL;
    bf->bf_qnum = sc->sc_paprdq->axq_qnum;
    bf->bf_state.bfs_ispaprd = 1;
    wlan_wbuf_set_peer_node(bf->bf_mpdu, (struct ieee80211_node *)bf->bf_node);
    bf->bf_isaggr  = 0;
    bf->bf_isbar  = bf->bf_isampdu = 0;

#ifdef ATH_PAPRD_DEBUG
    DPRINTF(sc,
            ATH_DEBUG_CALIBRATE,
            "%s bf->bf_frmlen %d wbuf_get_len(%d), bf->flags %x "
            "que num %d  bf->bf_buf_len %d\n",
           __func__, bf->bf_frmlen,
           wbuf_get_pktlen(bf->bf_mpdu),
           bf->bf_flags, sc->sc_paprdq->axq_qnum, bf->bf_buf_len[0]);
#endif
    /* fill the bssid */
    hdr = wbuf_raw_data(bf->bf_mpdu);
    if (hdr) {
        ATH_ADDR_COPY(hdr->i_addr1, bssid);
        ATH_ADDR_COPY(hdr->i_addr2, bssid);
        ATH_ADDR_COPY(hdr->i_addr3, bssid);
        hdr->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
        hdr->i_fc[1] = IEEE80211_FC1_DIR_NODS;
        *(u_int16_t *)hdr->i_dur = htole16(10);
        *(u_int16_t *)hdr->i_seq = 0;
    }
    bf->bf_buf_len[0] = roundup(wbuf_get_pktlen(bf->bf_mpdu), 4);

    DPRINTF(sc,ATH_DEBUG_CALIBRATE,
    "==== %s[%d] bf %pK bf->bf_frmlen %d wbuf_get_len(%d), bf->flags %x "
        "que num %d  bf->bf_buf_len %d\n",
       __func__, __LINE__, bf, bf->bf_frmlen,
       wbuf_get_pktlen(bf->bf_mpdu),
       bf->bf_flags, sc->sc_paprdq->axq_qnum, bf->bf_buf_len[0]);
    
    ath_hal_setdesclink(sc->sc_ah, ds, 0);

    ath_hal_set11n_txdesc(sc->sc_ah
            , ds
            , wbuf_get_pktlen(bf->bf_mpdu) + IEEE80211_CRC_LEN
            , HAL_PKT_TYPE_NORMAL
            , MIN(sc->sc_curtxpow, 60)
            , HAL_TXKEYIX_INVALID
            , HAL_KEY_TYPE_CLEAR
            , bf->bf_flags);

    ath_hal_filltxdesc(sc->sc_ah
            , ds
            , bf->bf_buf_addr
            , bf->bf_buf_len
            , 0
            , bf->bf_qnum
            , HAL_KEY_TYPE_CLEAR
            , AH_TRUE
            , AH_TRUE
            , ds);

    ath_hal_set_paprd_txdesc(sc->sc_ah
            , ds
            , chain_num);

    ath_desc_swap(ds);
#if 1
{
    HAL_11N_RATE_SERIES  series[4];
    int i;

    u_int txpower = IS_CHAN_2GHZ(&sc->sc_curchan) ?
        sc->sc_config.txpowlimit2G :
        sc->sc_config.txpowlimit5G;                                
                                
    for(i = 0; i < 4 ; i++) {
        //series[i].Tries = 1;
        if (ath_hal_Dragonfly(sc->sc_ah) || ath_hal_Honeybee20(sc->sc_ah)) {
            series[i].ch_sel = sc->sc_tx_chainmask & (1 << chain_num);
		} else {
            series[i].ch_sel = sc->sc_tx_chainmask | (1 << chain_num);
		}
        series[i].RateFlags =  0;
        //NEW
        series[i].Tries = 1;
        if (ath_hal_Jupiter(sc->sc_ah)){
            series[i].rate_index = 0;
            series[i].Rate = 0xb; /* legacy 6 Mbps */
        } else if (ath_hal_Dragonfly(sc->sc_ah) || ath_hal_Honeybee20(sc->sc_ah)) {
            series[i].rate_index = 12; /* MCS0 */
            series[i].Rate = 0x80;     /* MCS0 */ 
		} else {
            series[i].rate_index = 7;
            series[i].Rate = 0xc; /* legacy 54Mbps */
        }
        series[i].tx_power_cap = txpower;
        series[i].PktDuration = ath_pkt_duration(
            sc, series[0].rate_index, bf,
            ( series[0].RateFlags & ATH_RC_CW40_FLAG) != 0,
             (series[0].RateFlags & ATH_RC_SGI_FLAG),
            bf->bf_shpreamble);
    }

    smartAntenna = SMARTANT_INVALID;

#if UNIFIED_SMARTANTENNA
    for (i =0 ;i <= sc->max_fallback_rates; i++) {
        antenna_array[i] = sc->smart_ant_tx_default;
    }

    ath_hal_set11n_ratescenario(sc->sc_ah, ds, ds, 0, 0, 0 ,
            series, 4, 0, 0, antenna_array);
#else
    ath_hal_set11n_ratescenario(sc->sc_ah, ds, ds, 0, 0, 0 ,
            series, 4, 0, smartAntenna);
#endif    
}
#endif
    //dump_desc(sc, bf, sc->sc_paprdq->axq_qnum);
    ATH_TXQ_LOCK(sc->sc_paprdq);
    if (ath_tx_txqaddbuf(sc, sc->sc_paprdq, &bf_head)) {
        DPRINTF(sc,ATH_DEBUG_ANY, "++++====%s[%d]: ath_tx_txqaddbuf failed \n", __func__, __LINE__);
        ATH_TXQ_UNLOCK(sc->sc_paprdq);
        ath_tx_paprd_complete(sc, bf, &bf_head);
        return -1;
    }
    ATH_TXQ_UNLOCK(sc->sc_paprdq);
    return 0;
} /* end ath_sendpaprd */


void
ath_tx_paprd_complete(struct ath_softc *sc, struct ath_buf *bf,
        ath_bufhead *bf_q)
{

    ath_bufhead bf_head;
    int retval;
    int num_chain;
    ASSERT(bf);
    if (sc->sc_paprd_enable == 0) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: PAPRD Not ENABLED %d\n", __func__, __LINE__, sc->sc_curchan.paprd_done);
        return;
    }
    bf->bf_state.bfs_ispaprd = 0;    
    TAILQ_INIT(&bf_head);
    TAILQ_REMOVE_HEAD_UNTIL(bf_q, &bf_head, bf, bf_list);
    ATH_PAPRD_LOCK(sc);
    TAILQ_CONCAT(&sc->sc_paprdbuf, &bf_head, bf_list);
    ATH_PAPRD_UNLOCK(sc);
#ifdef AR_DEBUG
{
    struct ath_buf *tbf;
    int count = 0;
    DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: count %d \n", __func__, __LINE__, sc->sc_paprd_bufcount);    
    TAILQ_FOREACH(tbf, &sc->sc_paprdbuf, bf_list){
        count++;
    }
    DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d] TOTAL PAPRD BUF %d\n", __func__, __LINE__, count);
}
#endif    

    if (sc->sc_paprd_thermal_packet_not_sent) {
        sc->sc_paprd_thermal_packet_not_sent = 0;
        return;
    }
    if (sc->sc_paprd_abort) {
        DPRINTF(sc,ATH_DEBUG_ANY, "%s[%d]: ABORT PAPRD CAL \n", __func__, __LINE__);
        //ASSERT(0);
        sc->sc_curchan.paprd_done = 0;
        return;
    }
    if ((retval = ath_hal_paprd_is_done(sc->sc_ah)) == 0) {
        DPRINTF(sc, ATH_DEBUG_CALIBRATE, 
              "%s[%d]:  PAPRD CAL retry(%d times) on chain %d\n", 
               __func__, __LINE__, 
              sc->sc_paprd_failed_count, sc->sc_paprd_chain_num);
        sc->sc_paprd_failed_count++;
        goto RE_CAL;
    }
    retval = ath_hal_paprd_create_curve(sc->sc_ah, &sc->sc_curchan, sc->sc_paprd_chain_num);
    if (retval == HAL_EINPROGRESS) {
        sc->sc_paprd_retrain_count++;
        DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: "
            "ath_hal_paprd_create_curve return re-Train, loop %d\n", 
            __func__, __LINE__, sc->sc_paprd_retrain_count);
        if (sc->sc_paprd_retrain_count >= ATH_PAPRD_RETRAIN_MAXCOUNT) {
           DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: failed count %d!!\n", 
                       __func__, __LINE__, sc->sc_paprd_failed_count);
            goto FAILED;
        }
        goto RE_CAL;
    } else if (retval != HAL_OK) {
        DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: "
            "ath_hal_paprd_create_curve return failed,"
            " retval %d, loop %d failcount %d\n", __func__, __LINE__, 
            retval, sc->sc_paprd_retrain_count, sc->sc_paprd_failed_count);
        goto FAILED;
    } else { 
        DPRINTF(sc, ATH_DEBUG_CALIBRATE, "%s[%d]: "
            "ath_hal_paprd_create_curve return ok,"
            " retval %d, loop %d\n", __func__, __LINE__, 
            retval, sc->sc_paprd_retrain_count );
        sc->sc_paprd_retrain_count = 0;
    }
    sc->sc_paprd_done_chain_mask |=  (1 << sc->sc_paprd_chain_num);
        
    if(sc->sc_paprd_done_chain_mask == sc->sc_tx_chainmask) {
        sc->sc_curchan.paprd_done = 1;
        sc->sc_paprd_lastchan_num = sc->sc_curchan.channel;
        sc->sc_paprd_chain_num = 0;
        if (ath_hal_Dragonfly(sc->sc_ah) || ath_hal_Honeybee20(sc->sc_ah)) {
            sc->sc_paprd_ontraining = 0;
        }
    } else {
        for (num_chain = 0; num_chain < AH_MAX_CHAINS; num_chain++) {
            if (!((1 << num_chain) & sc->sc_tx_chainmask)) {
                continue;
            }
            /* PAPRD Done on this chain */
            if (((1 << num_chain) & sc->sc_paprd_done_chain_mask)) {
                continue;
            }
            break; /* Found chain for PAPRD to be done */
        }
        sc->sc_paprd_chain_num = num_chain;
RE_CAL:
        ath_paprd_cal(sc); 
    }
    return;
FAILED:
	sc->sc_paprd_cal_started = 0;
    return;
}
/*
 * Reclaim CF-END resources.
 * Context: Tasklet
 *    */
void ath_tx_paprd_cleanup(struct ath_softc *sc)
{
    ieee80211_tx_status_t tx_status;
    struct ath_buf *bf;
    wbuf_t wbuf;
    uint32_t result=0;
    tx_status.flags = 0;

    if ( ath_hal_getcapability(sc->sc_ah, HAL_CAP_PAPRD_ENABLED, 0, &result) != HAL_OK) {
    	return;
    }

    ATH_PAPRD_LOCK(sc);

    /* We use single wbuf to TX across multiple ath_buf for PAPRD frame */
    bf = TAILQ_FIRST(&sc->sc_paprdbuf);
    if (bf) {
	    if (bf->bf_mpdu) {
		wbuf = bf->bf_mpdu;
		wbuf_free(wbuf);
		bf->bf_mpdu=NULL;
	    }
	    TAILQ_FOREACH(bf, &sc->sc_paprdbuf, bf_list) {
		/* Unmap this frame */
		//printk( "%s[%d]: txq dot11nrate = %x bf_buf_len %d\n", __func__, __LINE__, bf->bf_buf_len[0]);
	#if 0
		wbuf_unmap_sg(sc->sc_osdev, wbuf,
			OS_GET_DMA_MEM_CONTEXT(bf, bf_dmacontext));
	#endif
		TAILQ_REMOVE(&sc->sc_paprdbuf, bf, bf_list);
		if (bf) {
		     bf->bf_mpdu=NULL;
		}
	    }
    }
    ATH_PAPRD_UNLOCK(sc);

    if (sc->sc_paprddma.dd_desc_len != 0) {
        ath_descdma_cleanup(sc, &sc->sc_paprddma, &sc->sc_paprdbuf);
    }

    ATH_PAPRD_LOCK_DESTROY(sc);
}

void
ath_tx_paprd_draintxq(struct ath_softc *sc)
{
    struct ath_buf *bf;
    ath_bufhead bf_head;
    struct ath_txq *txq = sc->sc_paprdq;

    if (sc->sc_paprd_enable == 0) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: PAPRD Not ENABLED %d\n", __func__, __LINE__, sc->sc_curchan.paprd_done);
        return;
    }
    TAILQ_INIT(&bf_head);
    /*
     * NB: this assumes output has been stopped and
     *     we do not need to block ath_tx_tasklet
     */

    ATH_PAPRD_LOCK(sc);

    for (;;) {
        bf = TAILQ_FIRST(&txq->axq_q);
        if (bf == NULL) {
            txq->axq_link = NULL;
            txq->axq_linkbuf = NULL;
            break;
        }
        /* process CF End packet */
        if (bf && bf->bf_state.bfs_ispaprd) {

            ((struct ath_desc*)(bf->bf_desc))->ds_txstat.ts_flags =
                                                HAL_TX_SW_ABORTED;

            ATH_TXQ_MOVE_HEAD_UNTIL(txq, &bf_head, bf, bf_list);
            ath_tx_paprd_complete (sc, bf, &bf_head);
            TAILQ_INIT(&bf_head);
            continue;
        }
    }
    ATH_PAPRD_UNLOCK(sc);
}

#endif /* ATH_SUPPORT_PAPRD */

