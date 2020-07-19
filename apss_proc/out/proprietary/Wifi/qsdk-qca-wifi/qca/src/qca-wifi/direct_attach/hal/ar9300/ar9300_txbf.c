/*
 * Copyright (c) 2008-2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */


#include "opt_ah.h"

#ifdef AH_SUPPORT_AR9300

#include "ah.h"
#include "ah_internal.h"
#include "ah_desc.h"
#include "ar9300.h"
#include "ar9300desc.h"
#include "ar9300reg.h"
#include "ar9300phy.h"

#ifdef ATH_SUPPORT_TxBF
#include "ar9300_txbf.h"

/* number of carrier mappings under different bandwidth and grouping, ex: 
   bw = 1 (40M), Ng=0 (no group), number of carrier = 114*/
static u_int8_t const Ns[NUM_OF_BW][NUM_OF_Ng] = {
    { 56, 30, 16},
    {114, 58, 30}};

static u_int8_t const Valid_bits[MAX_BITS_PER_SYMBOL] = {
    0x1, 0x3, 0x7, 0xf, 0x1f, 0x3f, 0x7f, 0xff};

static u_int8_t Num_bits_on[NUM_OF_CHAINMASK] = {
    0 /* 000 */,
    1 /* 001 */,
    1 /* 010 */,
    2 /* 011 */,
    1 /* 100 */,
    2 /* 101 */,
    2 /* 110 */,
    3 /* 111 */
};

u_int8_t
ar9300_get_ntx(struct ath_hal *ah)
{
    return Num_bits_on[AH9300(ah)->ah_tx_chainmask];
}

u_int8_t
ar9300_get_nrx(struct ath_hal *ah)
{
    return Num_bits_on[AH9300(ah)->ah_rx_chainmask];
}

void ar9300_set_hw_cv_timeout(struct ath_hal *ah, bool opt)
{
    struct ath_hal_private  *ap = AH_PRIVATE(ah);

    /* 
     * if true use H/W settings to update cv timeout values;
     * otherwise set the cv timeout value to 1 ms to trigger S/W timer
     */
    if (opt) {
        AH_PRIVATE(ah)->ah_txbf_hw_cvtimeout = ap->ah_config.ath_hal_cvtimeout;
    } else {
        AH_PRIVATE(ah)->ah_txbf_hw_cvtimeout = ONE_MS;
    }
    OS_REG_WRITE(ah, AR_TXBF_TIMER,
        (AH_PRIVATE(ah)->ah_txbf_hw_cvtimeout << AR_TXBF_TIMER_ATIMEOUT_S) |
        (AH_PRIVATE(ah)->ah_txbf_hw_cvtimeout << AR_TXBF_TIMER_TIMEOUT_S));
}

void ar9300_init_txbf(struct ath_hal *ah)
{
    u_int32_t tmp;
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    u_int8_t txbf_ctl;

    /* set default settings for tx_bf */
    OS_REG_WRITE(ah, AR_TXBF,
        /* size of codebook entry set to psi 3bits, phi 6bits */
        (AR_TXBF_PSI_4_PHI_6 << AR_TXBF_CB_TX_S)    |
        /* set Number of bit_to 8 bits */
        (AR_TXBF_NUMBEROFBIT_8 << AR_TXBF_NB_TX_S)  |
        /* NG_RPT_TX set t0 No_GROUP */
        (AR_TXBF_No_GROUP << AR_TXBF_NG_RPT_TX_S)   |
        /* TXBF_NG_CVCACHE set to 16 clients */
        (AR_TXBF_SIXTEEN_CLIENTS << AR_TXBF_NG_CVCACHE_S)  |
        /* set weighting method to max power */
        (SM(AR_TXBF_MAX_POWER, AR_TXBF_TXCV_BFWEIGHT_METHOD)));

    /* when ah_txbf_hw_cvtimeout = 0, use setting values */
    if (AH_PRIVATE(ah)->ah_txbf_hw_cvtimeout == 0) {
        ar9300_set_hw_cv_timeout(ah, true);
    } else {
        ar9300_set_hw_cv_timeout(ah, false);
    }

    /*
     * Set CEC to 2 stream for self_gen.
     * Set spacing to 8 us.
     * Initial selfgen Minimum MPDU.
     */
    tmp = OS_REG_READ(ah, AR_SELFGEN);
    tmp |= SM(AR_SELFGEN_CEC_TWO_SPACETIMESTREAM, AR_CEC);
    tmp |= SM(AR_SELFGEN_MMSS_EIGHT_us, AR_MMSS);
    OS_REG_WRITE(ah, AR_SELFGEN, tmp);
   
    /*  set initial for basic set */
    ahpriv->ah_lowest_txrate = MAXMCSRATE;
    ahpriv->ah_basic_set_buf = (1 << (ahpriv->ah_lowest_txrate+ 1))- 1;
    OS_REG_WRITE(ah, AR_BASIC_SET, ahpriv->ah_basic_set_buf);

#if 0
    tmp = OS_REG_READ(ah, AR_PCU_MISC_MODE2);
    /* force H upload */
    tmp |= (1 << 28);
    OS_REG_WRITE(ah, AR_PCU_MISC_MODE2, tmp);
#endif

    /* enable HT fine timing */
    tmp = OS_REG_READ(ah, AR_PHY_TIMING2);
    tmp |= AR_PHY_TIMING2_HT_Fine_Timing_EN;
    OS_REG_WRITE(ah, AR_PHY_TIMING2, tmp);

    /* enable description decouple */
    tmp = OS_REG_READ(ah, AR_PCU_MISC_MODE2);
    tmp |= AR_DECOUPLE_DECRYPTION;
    OS_REG_WRITE(ah, AR_PCU_MISC_MODE2, tmp);

    /* enable flt SVD */
    tmp = OS_REG_READ(ah, AR_PHY_SEARCH_START_DELAY);
    tmp |= AR_PHY_ENABLE_FLT_SVD;
    OS_REG_WRITE(ah, AR_PHY_SEARCH_START_DELAY, tmp);

    /* initial sequence generate for auto reply mgmt frame*/
    OS_REG_WRITE(ah, AR_MGMT_SEQ, AR_MIN_HW_SEQ | 
        SM(AR_MAX_HW_SEQ,AR_MGMT_SEQ_MAX));
    
    txbf_ctl = ahpriv->ah_config.ath_hal_txbf_ctl;
    if ((txbf_ctl & TXBF_CTL_NON_EX_BF_IMMEDIATELY_RPT) ||
        (txbf_ctl & TXBF_CTL_COMP_EX_BF_IMMEDIATELY_RPT)) {
        tmp = OS_REG_READ(ah, AR_H_XFER_TIMEOUT);
        tmp |= AR_EXBF_IMMDIATE_RESP;
        tmp &= ~(AR_EXBF_NOACK_NO_RPT);
        /* enable immediately report */
        OS_REG_WRITE(ah, AR_H_XFER_TIMEOUT, tmp);
    }
}

int
ar9300_get_ness(struct ath_hal *ah, u_int8_t code_rate, u_int8_t cec)
{
    u_int8_t ndltf = 0, ness = 0, ntx;

    ntx = ar9300_get_ntx(ah);

    /* cec+1 remote cap's for channel estimation in stream.*/
    if (ntx > (cec + 2)) {  /* limit by remote's cap */
        ntx = cec + 2;
    }

    if (code_rate < MIN_TWO_STREAM_RATE) {
        ndltf = 1;
    } else if (code_rate < MIN_THREE_STREAM_RATE) {
        ndltf = 2;
    } else {

        ndltf = 4;
    }

    /* NESS is used for setting neltf and NTX =<NDLTF + NELTF, NDLTF 
     * is 2^(stream-1), if NTX < NDLTF, NESS=0, other NESS = NTX-NDLTF */
    if (code_rate >= MIN_HT_RATE) { /* HT rate */
        if (ntx > ndltf) {
            ness = ntx - ndltf;
        }
    }
    return ness;
}

/*
 * function: ar9300_set_11n_txbf_sounding
 * purpose:  Set sounding frame
 * inputs:   
 *           series: rate series of sounding frame
 *           cec: Channel Estimation Capability . Extract from subfields
 *               of the Transmit Beamforming Capabilities field of remote.
 *           opt: control flag of current frame
 */
void
ar9300_set_11n_txbf_sounding(
    struct ath_hal *ah,
    void *ds,
    HAL_11N_RATE_SERIES series[],
    u_int8_t cec,
    u_int8_t opt)
{
    struct ar9300_txc *ads = AR9300TXC(ds);
    u_int8_t ness = 0, ness1 = 0, ness2 = 0, ness3 = 0;

    ads->ds_ctl19 &= (~AR_not_sounding); /*set sounding frame */

    if (opt == HAL_TXDESC_STAG_SOUND) {
        ness  = ar9300_get_ness(ah, series[0].Rate, cec);
        ness1 = ar9300_get_ness(ah, series[1].Rate, cec);
        ness2 = ar9300_get_ness(ah, series[2].Rate, cec);
        ness3 = ar9300_get_ness(ah, series[3].Rate, cec);
    }
    ads->ds_ctl19 |= SM(ness, AR_ness);
    ads->ds_ctl20 |= SM(ness1, AR_ness1);
    ads->ds_ctl21 |= SM(ness2, AR_ness2);
    ads->ds_ctl22 |= SM(ness3, AR_ness3);

#if 0
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:sounding rate series %x,%x,%x,%x \n",
        __func__,
        series[0].Rate, series[1].Rate, series[2].Rate, series[3].Rate);
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:Ness1 %d, NESS2 %d, NESS3 %d, NESS4 %d\n",
        __func__, ness, ness1, ness2, ness3);
#endif

    /* disable other series that don't support sounding at legacy rate */
    if (series[1].Rate < MIN_HT_RATE) {
        ads->ds_ctl13 &=
            ~(AR_xmit_data_tries1 | AR_xmit_data_tries2 | AR_xmit_data_tries3);
    } else if (series[2].Rate < MIN_HT_RATE) {
        ads->ds_ctl13 &= ~(AR_xmit_data_tries2 | AR_xmit_data_tries3);
    } else if (series[3].Rate < MIN_HT_RATE) {
        ads->ds_ctl13 &= ~(AR_xmit_data_tries3);
    }
}

/* search CV cache address according to key index */
static u_int16_t
ar9300_txbf_lru_search(struct ath_hal *ah, u_int8_t key_idx)
{
    u_int32_t tmp = 0;
    u_int16_t cv_cache_idx = 0;

    /* make sure LRU search is initially disabled */
    OS_REG_WRITE(ah, AR_TXBF_SW, 0);
    tmp = (SM(key_idx, AR_DEST_IDX) | AR_LRU_EN);
    /* enable LRU search */
    OS_REG_WRITE(ah, AR_TXBF_SW, tmp);

    /* wait for LRU search to finish */
    do {
        tmp = OS_REG_READ(ah, AR_TXBF_SW);
    } while ((tmp & AR_LRU_ACK) != 1);
    
    cv_cache_idx = MS(tmp, AR_LRU_ADDR);  

    /* disable LRU search */
    OS_REG_WRITE(ah, AR_TXBF_SW, 0);
    return cv_cache_idx;
}


void
ar9300_fill_txbf_capabilities(struct ath_hal *ah)
{
    struct ath_hal_9300     *ahp = AH9300(ah);
    HAL_TXBF_CAPS           *txbf = &ahp->txbf_caps;
    struct ath_hal_private  *ahpriv = AH_PRIVATE(ah);
    u_int32_t val;
    u_int8_t txbf_ctl;

    OS_MEMZERO(txbf, sizeof(HAL_TXBF_CAPS));
    if (ahpriv->ah_config.ath_hal_txbf_ctl == 0) {
        /* doesn't support tx_bf, let txbf ie = 0*/
        return;
    }
    /* CEC for osprey is always 1 (2 stream) */ 
    txbf->channel_estimation_cap = 1;

    /* For calibration, always 2 (3 stream) for osprey */
    txbf->csi_max_rows_bfer = 2;
    
    /*
     * Compressed Steering Number of Beamformer Antennas Supported is
     * limited by local's antenna
     */
    txbf->comp_bfer_antennas = ar9300_get_ntx(ah)-1;
    
    /*
     * Compressed Steering Number of Beamformer Antennas Supported
     * is limited by local's antenna
     */
    txbf->noncomp_bfer_antennas = ar9300_get_ntx(ah)-1;
    
    /* NOT SUPPORT CSI */
    txbf->csi_bfer_antennas = 0;
    
    /* 1, 2, 4 group is supported */
    txbf->minimal_grouping = ALL_GROUP;
   
    /* Explicit compressed Beamforming Feedback Capable */
    if (ahpriv->ah_config.ath_hal_txbf_ctl & TXBF_CTL_COMP_EX_BF_DELAY_RPT) {
        /* support delay report by settings */
        txbf->explicit_comp_bf |= Delay_Rpt;
    }
    if (ahpriv->ah_config.ath_hal_txbf_ctl &
        TXBF_CTL_COMP_EX_BF_IMMEDIATELY_RPT)
    {
        /* support immediately report by settings.*/
        txbf->explicit_comp_bf |= Immediately_Rpt;
    }

    /* Explicit non-Compressed Beamforming Feedback Capable */
    if (ahpriv->ah_config.ath_hal_txbf_ctl & TXBF_CTL_NON_EX_BF_DELAY_RPT) {
        txbf->explicit_noncomp_bf |= Delay_Rpt;
    }
    if (ahpriv->ah_config.ath_hal_txbf_ctl &
        TXBF_CTL_NON_EX_BF_IMMEDIATELY_RPT)
    {
        txbf->explicit_noncomp_bf |= Immediately_Rpt;
    }

    /* not support csi feekback */
    txbf->explicit_csi_feedback = 0; 

    /* Explicit compressed Steering Capable from settings */
    txbf->explicit_comp_steering =
        MS(ahpriv->ah_config.ath_hal_txbf_ctl, TXBF_CTL_COMP_EX_BF);
        
    /* Explicit Non-compressed Steering Capable from settings */
    txbf->explicit_noncomp_steering =
        MS(ahpriv->ah_config.ath_hal_txbf_ctl, TXBF_CTL_NON_EX_BF);
    
    /* not support CSI */
    txbf->explicit_csi_txbf_capable = false; 
    
    /* not support imbf and calibration */
    txbf->calibration = NO_CALIBRATION;
    txbf->implicit_txbf_capable = false;
    txbf->implicit_rx_capable  = false;
    /* not support NDP */
    txbf->tx_ndp_capable = false;    
    txbf->rx_ndp_capable = false;
    
    /* support stagger sounding. */
    txbf->tx_staggered_sounding = true;  
    txbf->rx_staggered_sounding = true;

    /* set immediately or delay report to H/W */
    val = OS_REG_READ(ah, AR_H_XFER_TIMEOUT);
    txbf_ctl = ahpriv->ah_config.ath_hal_txbf_ctl;
    if (((txbf_ctl & TXBF_CTL_NON_EX_BF_IMMEDIATELY_RPT) == 0) &&
        ((txbf_ctl & TXBF_CTL_COMP_EX_BF_IMMEDIATELY_RPT) == 0)) {
        /* enable delayed report */
        val &= ~(AR_EXBF_IMMDIATE_RESP);
        val |= AR_EXBF_NOACK_NO_RPT;
        OS_REG_WRITE(ah, AR_H_XFER_TIMEOUT, val);
    } else {
        /* enable immediately report */
        val |= AR_EXBF_IMMDIATE_RESP;
        val &= ~(AR_EXBF_NOACK_NO_RPT);
        OS_REG_WRITE(ah, AR_H_XFER_TIMEOUT, val);
    }
    #if 0
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%sTxBfCtl= %x \n", __func__, ahpriv->ah_config.ath_hal_txbf_ctl);
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:Compress ExBF= %x FB %x\n",
        __func__, txbf->explicit_comp_steering, txbf->explicit_comp_bf);
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:NonCompress ExBF= %x FB %x\n",
        __func__, txbf->explicit_noncomp_steering, txbf->explicit_noncomp_bf);
    HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s:ImBF= %x FB %x\n",
        __func__, txbf->implicit_txbf_capable, txbf->implicit_rx_capable);
    #endif
}

HAL_TXBF_CAPS *
ar9300_get_txbf_capabilities(struct ath_hal *ah)
{
    return &(AH9300(ah)->txbf_caps);
}

/*
 * ar9300_txbf_set_key is used to set TXBF related field in key cache.
 */
void ar9300_txbf_set_key(
    struct ath_hal *ah,
    u_int16_t entry,
    u_int8_t rx_staggered_sounding,
    u_int8_t channel_estimation_cap,
    u_int8_t mmss)
{
    u_int32_t tmp, txbf;

    /* 1 for 2 stream, 0 for 1 stream, should add 1 for H/W */
    channel_estimation_cap += 1;

    txbf = (
        SM(rx_staggered_sounding, AR_KEYTABLE_STAGGED) |
        SM(channel_estimation_cap, AR_KEYTABLE_CEC)    |
        SM(mmss, AR_KEYTABLE_MMSS));
    tmp = OS_REG_READ(ah, AR_KEYTABLE_TYPE(entry));
    if (txbf !=
        (tmp & (AR_KEYTABLE_STAGGED | AR_KEYTABLE_CEC | AR_KEYTABLE_MMSS))) {
        /* update key cache for txbf */
        OS_REG_WRITE(ah, AR_KEYTABLE_TYPE(entry), tmp | txbf);
        #if 0
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "==>%s: update keyid %d, value %x, orignal %x\n",
            __func__, entry, txbf, tmp);
        #endif
    }
    #if 0
    else {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
        "==>%s: parameters no changes : %x, don't need updtate key!\n",
        __func__, tmp);
    }
    #endif
}

u_int32_t
ar9300_read_cv_cache(struct ath_hal *ah, u_int32_t addr)
{
    u_int32_t tmp, value;

    OS_REG_WRITE(ah, addr, AR_CVCACHE_RD_EN);
    do {
        tmp = OS_REG_READ(ah, AR_TXBF_SW);
    } while ((tmp & AR_LRU_RD_ACK) == 0);
    
    value = OS_REG_READ(ah, addr);
    tmp &= ~(AR_LRU_RD_ACK);
    OS_REG_WRITE(ah, AR_TXBF_SW, tmp);
    return (value & AR_CVCACHE_DATA);
}

void
ar9300_txbf_get_cv_cache_nr(struct ath_hal *ah, u_int16_t key_idx, u_int8_t *nr)
{
    u_int32_t idx, value;
    u_int8_t  nr_idx;
    
    /* get current CV cache addess offset from key index */
    idx = ar9300_txbf_lru_search(ah, key_idx);
    
    /* read the cvcache header */
    value = ar9300_read_cv_cache(ah, AR_CVCACHE(idx));
    nr_idx = MS(value, AR_CVCACHE_Nr_IDX);  
    *nr = nr_idx + 1;      
}

/*
 * Workaround for HW issue EV [69449] Chip::Osprey HW does not filter 
 * non-directed frame for uploading TXBF delay report 
 */
void 
ar9300_reconfig_h_xfer_timeout(struct ath_hal *ah, bool is_reset)
{
#define DEFAULT_TIMEOUT_VALUE      0xD

    u_int32_t val;

    val = OS_REG_READ(ah, AR_H_XFER_TIMEOUT);

    if (is_reset) {
        val = DEFAULT_TIMEOUT_VALUE + 
            (val & (~AR_H_XFER_TIMEOUT_COUNT));
    } else {
        val = ((val - (1 << AR_H_XFER_TIMEOUT_COUNT_S)) & 
               AR_H_XFER_TIMEOUT_COUNT) + 
            (val & (~AR_H_XFER_TIMEOUT_COUNT));
    }

    OS_REG_WRITE(ah, AR_H_XFER_TIMEOUT, val);
    return;

#undef DEFAULT_TIMEOUT_VALUE
}

/*
 * Limit self-generate frame rate (such as CV report ) by lowest Tx rate to
 * guarantee the success of CV report frame 
 */
void
ar9300_set_selfgenrate_limit(struct ath_hal *ah, u_int8_t rateidx)
{
    struct      ath_hal_private *ahp = AH_PRIVATE(ah);
    u_int32_t   selfgen_rate; 
   
    if (rateidx & HT_RATE){
        rateidx &= ~ (HT_RATE);
        if (rateidx < ahp->ah_lowest_txrate){
            ahp->ah_lowest_txrate = rateidx;
            selfgen_rate = (1 << ((ahp->ah_lowest_txrate) + 1))- 1;
            if (selfgen_rate !=  ahp->ah_basic_set_buf){
                ahp->ah_basic_set_buf = selfgen_rate;
                OS_REG_WRITE(ah, AR_BASIC_SET, ahp->ah_basic_set_buf);
            }
        }
    }
}

/*
 * Reset current lower Tx rate to max MCS 
 */
void
ar9300_reset_lowest_txrate(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);

    if (ahp->ah_reduced_self_gen_mask){
        /* limit self gen rate to one stream for BT coex */
        ahpriv->ah_lowest_txrate = MAXONESTREAMRATE;
    }
    else{
        ahpriv->ah_lowest_txrate = MAXMCSRATE;
    }
}   

/*
 * Update basic set according to self gen mask.
 */ 
void
ar9300_txbf_set_basic_set(struct ath_hal *ah)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);
    struct ath_hal_9300 *ah9300 = AH9300(ah);

    if (ah9300->ah_reduced_self_gen_mask){
        /* set max rate to mcs7 when self gen mask is single chain. */
        ahp->ah_lowest_txrate = MAXONESTREAMRATE;
        
    }else{
        ahp->ah_lowest_txrate = MAXMCSRATE;
    }
    ahp->ah_basic_set_buf = (1 << (ahp->ah_lowest_txrate+ 1))- 1;
    OS_REG_WRITE(ah, AR_BASIC_SET, ahp->ah_basic_set_buf); 
}

#endif /* ATH_SUPPORT_TxBF*/
#endif /* AH_SUPPORT_AR9300 */
