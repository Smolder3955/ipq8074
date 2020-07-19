/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 *
 */


#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5416

#include "ah.h"
#include "ah_desc.h"
#include "ah_internal.h"

#if WLAN_SPECTRAL_ENABLE

#include "ar5416/ar5416phy.h"
#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416desc.h"

/*
 * Default 5416 spectral scan parameters
 */
#define AR5416_SPECTRAL_SCAN_ENA                0
#define AR5416_SPECTRAL_SCAN_ACTIVE             0
#define AR5416_SPECTRAL_SCAN_FFT_PERIOD         8 
#define AR5416_SPECTRAL_SCAN_PERIOD             1
#define AR5416_SPECTRAL_SCAN_COUNT              16 //used to be 128 
#define AR5416_SPECTRAL_SCAN_SHORT_REPEAT       1

/* constants */
#define MAX_RADAR_DC_PWR_THRESH 127 
#define MAX_RADAR_RSSI_THRESH 0x3f   
#define MAX_RADAR_HEIGHT 0x3f   
#define MAX_CCA_THRESH 127   
#define ENABLE_ALL_PHYERR 0xffffffff

void ar5416DisableCCK(struct ath_hal *ah);
void ar5416DisableRadar(struct ath_hal *ah);
void ar5416DisableRestart(struct ath_hal *ah);
void ar5416SetRadarDcThresh(struct ath_hal *ah);
void ar5416DisableWeakSignal(struct ath_hal *ah);
void ar5416DisableStrongSignal(struct ath_hal *ah);
void ar5416PrepSpectralScan(struct ath_hal *ah);

void
ar5416DisableCCK(struct ath_hal *ah)
{
    u_int32_t val;

    val = OS_REG_READ(ah, AR_PHY_MODE);
    val &= ~(AR_PHY_MODE_DYN_CCK_DISABLE);

    OS_REG_WRITE(ah, AR_PHY_MODE, val);
}
void
ar5416DisableRadar(struct ath_hal *ah)
{
    u_int32_t val;

    // Enable radar FFT
    val = OS_REG_READ(ah, AR_PHY_RADAR_0);
    val |= AR_PHY_RADAR_0_FFT_ENA;

    // set radar detect thresholds to max to effectively disable radar
    val &= ~AR_PHY_RADAR_0_RRSSI;
    val |= SM(MAX_RADAR_RSSI_THRESH, AR_PHY_RADAR_0_RRSSI);

    val &= ~AR_PHY_RADAR_0_HEIGHT;
    val |= SM(MAX_RADAR_HEIGHT, AR_PHY_RADAR_0_HEIGHT);

    val &= ~(AR_PHY_RADAR_0_ENA);
    OS_REG_WRITE(ah, AR_PHY_RADAR_0, val);
    
    // disable extension radar detect 
    val = OS_REG_READ(ah, AR_PHY_RADAR_EXT);
    OS_REG_WRITE(ah, AR_PHY_RADAR_EXT, val & ~AR_PHY_RADAR_EXT_ENA);

    val = OS_REG_READ(ah, AR_RX_FILTER);
    val |= (1<<13);
    OS_REG_WRITE(ah, AR_RX_FILTER, val);
}

void ar5416DisableRestart(struct ath_hal *ah)
{
    u_int32_t val;
    val = OS_REG_READ(ah, AR_PHY_RESTART);
    val &= ~AR_PHY_RESTART_ENA;
    OS_REG_WRITE(ah, AR_PHY_RESTART, val);

    val = OS_REG_READ(ah, AR_PHY_RESTART);
}

void ar5416SetRadarDcThresh(struct ath_hal *ah)
{
    u_int32_t val;
    val = OS_REG_READ(ah, AR_PHY_RADAR_EXT);
    val &= ~AR_PHY_RADAR_DC_PWR_THRESH;
    val |= SM(MAX_RADAR_DC_PWR_THRESH, AR_PHY_RADAR_DC_PWR_THRESH);
    OS_REG_WRITE(ah, AR_PHY_RADAR_EXT, val);
    
    val = OS_REG_READ(ah, AR_PHY_RADAR_EXT);
}

void
ar5416DisableWeakSignal(struct ath_hal *ah)
{
    // set firpwr to max (signed)
    OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_FIRPWR, 0x7f);
    OS_REG_CLR_BIT(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_FIRPWR_SIGN_BIT);

    // set firstep to max
    OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_FIRSTEP, 0x3f);

    // set relpwr to max (signed)
    OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_RELPWR, 0x1f);
    OS_REG_CLR_BIT(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_RELPWR_SIGN_BIT);

    // set relstep to max (signed)
    OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_RELSTEP, 0x1f);
    OS_REG_CLR_BIT(ah, AR_PHY_FIND_SIG, AR_PHY_FIND_SIG_RELSTEP_SIGN_BIT);

    // set firpwr_low to max (signed)
    OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG_LOW, AR_PHY_FIND_SIG_LOW_FIRPWR, 0x7f);
    OS_REG_CLR_BIT(ah, AR_PHY_FIND_SIG_LOW, AR_PHY_FIND_SIG_LOW_FIRPWR_SIGN_BIT);

    // set firstep_low to max
    OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG_LOW, AR_PHY_FIND_SIG_LOW_FIRSTEP, 0x3f);
 
    // set relstep_low to max (signed)
    OS_REG_RMW_FIELD(ah, AR_PHY_FIND_SIG_LOW, AR_PHY_FIND_SIG_LOW_RELSTEP, 0x1f);
    OS_REG_CLR_BIT(ah, AR_PHY_FIND_SIG_LOW, AR_PHY_FIND_SIG_LOW_RELSTEP_SIGN_BIT);
}

void
ar5416DisableStrongSignal(struct ath_hal *ah)
{
    u_int32_t val;

    val = OS_REG_READ(ah, AR_PHY_TIMING5);
    val |= AR_PHY_TIMING5_RSSI_THR1A_ENA;
    OS_REG_WRITE(ah, AR_PHY_TIMING5, val);

    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING5, AR_PHY_TIMING5_RSSI_THR1A, 0x7f);

}
void
ar5416SetCcaThreshold(struct ath_hal *ah, u_int8_t thresh62)
{
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        OS_REG_RMW_FIELD(ah, AR_PHY_CCA, AR9280_PHY_CCA_THRESH62, thresh62);
        OS_REG_RMW_FIELD(ah, AR_PHY_EXT_CCA0, AR_PHY_EXT_CCA0_THRESH62, thresh62);
        OS_REG_RMW_FIELD(ah, AR_PHY_EXT_CCA, AR_PHY_EXT_CCA_THRESH62, thresh62);
    } else {
        OS_REG_RMW_FIELD(ah, AR_PHY_CCA, AR_PHY_CCA_THRESH62, thresh62);
        OS_REG_RMW_FIELD(ah, AR_PHY_EXT_CCA, AR_PHY_EXT_CCA_THRESH62, thresh62);
    }
}

void ar5416PrepSpectralScan(struct ath_hal *ah)
{
    ar5416DisableRadar(ah);
#ifdef DEMO_MODE
    ar5416DisableStrongSignal(ah);
    ar5416DisableWeakSignal(ah);
    ar5416SetRadarDcThresh(ah);
    ar5416SetCcaThreshold(ah, MAX_CCA_THRESH);
   // ar5416DisableRestart(ah);
#endif
    OS_REG_WRITE(ah, AR_PHY_ERR, ENABLE_ALL_PHYERR);
}


void
ar5416ConfigureSpectralScan(struct ath_hal *ah, HAL_SPECTRAL_PARAM *ss)
{
    u_int32_t val;

    ar5416PrepSpectralScan(ah);

    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);

    if (ss->ss_fft_period != HAL_SPECTRAL_PARAM_NOVAL) {
        val &= ~AR_PHY_SPECTRAL_SCAN_FFT_PERIOD;
        val |= SM(ss->ss_fft_period, AR_PHY_SPECTRAL_SCAN_FFT_PERIOD);
    }
    
    if (ss->ss_period != HAL_SPECTRAL_PARAM_NOVAL) {
        val &= ~AR_PHY_SPECTRAL_SCAN_PERIOD;
        val |= SM(ss->ss_period, AR_PHY_SPECTRAL_SCAN_PERIOD);
    }
    
    if (ss->ss_period != HAL_SPECTRAL_PARAM_NOVAL) {
        val &= ~AR_PHY_SPECTRAL_SCAN_PERIOD;
        val |= SM(ss->ss_period, AR_PHY_SPECTRAL_SCAN_PERIOD);
    }
    /* This section is different for Kiwi and Merlin */
    if( AR_SREV_MERLIN(ah) ) {
    
        if (ss->ss_count != HAL_SPECTRAL_PARAM_NOVAL) {
            val &= ~AR_PHY_SPECTRAL_SCAN_COUNT;
            val |= SM(ss->ss_count, AR_PHY_SPECTRAL_SCAN_COUNT);
        }
    
        if (ss->ss_short_report == AH_TRUE) {
            val |= AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT;
        } else {
            val &= ~AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT;
        }
    } else {
        if (ss->ss_count != HAL_SPECTRAL_PARAM_NOVAL) {
            /* In Merlin, for continous scan, scan_count = 128.
            * In case of Kiwi, this value should be 0
            */
            if( ss->ss_count == 128 ) ss->ss_count = 0;
            val &= ~AR_PHY_SPECTRAL_SCAN_COUNT_KIWI;
            val |= SM(ss->ss_count, AR_PHY_SPECTRAL_SCAN_COUNT_KIWI);
        }
    
        if (ss->ss_short_report == AH_TRUE) {
            val |= AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT_KIWI;
        } else {
            val &= ~AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT_KIWI;
        }
        //Select the mask to be same as before
        val |= AR_PHY_SPECTRAL_SCAN_PHYERR_MASK_SELECT_KIWI;
    }
    // Enable spectral scan
    OS_REG_WRITE(ah, AR_PHY_SPECTRAL_SCAN, val | AR_PHY_SPECTRAL_SCAN_ENA);

    ar5416GetSpectralParams(ah, ss);
}

/*
 * Get the spectral parameter values and return them in the pe
 * structure
 */

void
ar5416GetSpectralParams(struct ath_hal *ah, HAL_SPECTRAL_PARAM *ss)
{
    u_int32_t val;
    
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);

    ss->ss_fft_period = MS(val, AR_PHY_SPECTRAL_SCAN_FFT_PERIOD);
    ss->ss_period = MS(val, AR_PHY_SPECTRAL_SCAN_PERIOD);
    if( AR_SREV_MERLIN(ah) ) {
        ss->ss_count = MS(val, AR_PHY_SPECTRAL_SCAN_COUNT);
        ss->ss_short_report = MS(val, AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT);
    } else {
        ss->ss_count = MS(val, AR_PHY_SPECTRAL_SCAN_COUNT_KIWI);
        ss->ss_short_report = MS(val, AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT_KIWI);
    }
    val = OS_REG_READ(ah, AR_PHY_RADAR_1);
    ss->radar_bin_thresh_sel = MS(val, AR_PHY_RADAR_1_BIN_THRESH_SELECT);
}

HAL_BOOL
ar5416IsSpectralActive(struct ath_hal *ah)
{
    u_int32_t val;
    
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    return MS(val, AR_PHY_SPECTRAL_SCAN_ACTIVE);
}

HAL_BOOL
ar5416IsSpectralEnabled(struct ath_hal *ah)
{
    u_int32_t val;
    
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    return MS(val,AR_PHY_SPECTRAL_SCAN_ENA);
}

void ar5416StartSpectralScan(struct ath_hal *ah)
{
    u_int32_t val;

    ar5416PrepSpectralScan(ah);

    // Activate spectral scan
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    val |= AR_PHY_SPECTRAL_SCAN_ENA;
    val |= AR_PHY_SPECTRAL_SCAN_ACTIVE;
    OS_REG_WRITE(ah, AR_PHY_SPECTRAL_SCAN, val);
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    val = OS_REG_READ(ah, AR_PHY_ERR_MASK_REG);
    OS_REG_WRITE(ah, AR_PHY_ERR_MASK_REG, val | AR_PHY_ERR_RADAR);
}

void ar5416StopSpectralScan(struct ath_hal *ah)
{
    u_int32_t val;
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);

    // Deactivate spectral scan
    val &= ~AR_PHY_SPECTRAL_SCAN_ENA;
    val &= ~AR_PHY_SPECTRAL_SCAN_ACTIVE;
    OS_REG_WRITE(ah, AR_PHY_SPECTRAL_SCAN, val);
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    val = OS_REG_READ(ah, AR_PHY_ERR_MASK_REG) & (~AR_PHY_ERR_RADAR);
    OS_REG_WRITE(ah, AR_PHY_ERR_MASK_REG, val);
}

u_int32_t ar5416GetSpectralConfig(struct ath_hal *ah)
{
    u_int32_t val;
    val = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);
    return val;
}

void ar5416RestoreSpectralConfig(struct ath_hal *ah, u_int32_t restoreval)
{
    u_int32_t curval;

    ar5416PrepSpectralScan(ah);

    curval = OS_REG_READ(ah, AR_PHY_SPECTRAL_SCAN);

    if (restoreval != curval) {
        restoreval |= AR_PHY_SPECTRAL_SCAN_SHORT_REPEAT;         
        OS_REG_WRITE(ah, AR_PHY_SPECTRAL_SCAN, restoreval);
    }
    return;
}

int16_t ar5416GetCtlChanNF(struct ath_hal *ah)
{
    int16_t nf;
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);

    if ( (OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0) {
        /* Noise floor calibration value is ready */
        if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
            nf = MS(OS_REG_READ(ah, AR_PHY_CCA), AR9280_PHY_MINCCA_PWR);
        } else {
            nf = MS(OS_REG_READ(ah, AR_PHY_CCA), AR_PHY_MINCCA_PWR);
        }
    } else {
        /* NF calibration is not done, return nominal value */
        nf = ahpriv->nfp->nominal;    
    }
    if (nf & 0x100)
        nf = (0 - ((nf ^ 0x1ff) + 1));
    return nf;
}

int16_t ar5416GetExtChanNF(struct ath_hal *ah)
{
    int16_t nf;
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);

    if ((OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0) {
        /* Noise floor calibration value is ready */
        if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
            nf = MS(OS_REG_READ(ah, AR_PHY_EXT_CCA), AR9280_PHY_EXT_MINCCA_PWR);
        } else {
            nf = MS(OS_REG_READ(ah, AR_PHY_EXT_CCA), AR_PHY_EXT_MINCCA_PWR);
        }
    } else {
        /* NF calibration is not done, return nominal value */
        nf = ahpriv->nfp->nominal;    
    }
    if (nf & 0x100)
        nf = (0 - ((nf ^ 0x1ff) + 1));
    return nf;
}

int16_t ar5416GetNominalNF(struct ath_hal *ah, HAL_FREQ_BAND band)
{
    int16_t nominal_nf;
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);

    if (band == HAL_FREQ_BAND_5GHZ) {
        nominal_nf = ahpriv->nf_5GHz.nominal;
    } else {
        nominal_nf = ahpriv->nf_2GHz.nominal;
    }

    return nominal_nf;
}

#else
/* spectral scan not enabled - insert dummy code to keep the compiler happy */
typedef int ar5416_spectral_dummy;
#endif /* WLAN_SPECTRAL_ENABLE */

#endif /* AH_SUPPORT_AR5416*/
