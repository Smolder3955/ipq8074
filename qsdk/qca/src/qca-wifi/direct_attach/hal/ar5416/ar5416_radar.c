/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2008-2010, Atheros Communications Inc.
 * All Rights Reserved.
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5416

#include "ah.h"
#include "ah_desc.h"
#include "ah_internal.h"

#include "ar5416/ar5416phy.h"
#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"

#ifdef ATH_SUPPORT_DFS
/*
 * Default 5413/5416 radar phy parameters
 */
#define AR5416_DFS_FIRPWR	-33
#define AR5416_DFS_RRSSI	20
#define AR5416_DFS_HEIGHT	10
#define AR5416_DFS_PRSSI	15
#define AR5416_DFS_INBAND	15
#define AR5416_DFS_RELPWR   8
#define AR5416_DFS_RELSTEP  12
#define AR5416_DFS_MAXLEN   255

#define AR5416_DEFAULT_DIFS     0x002ffc0f

/*
 * Enable radar detection and set the radar parameters per the
 * values in pe
 */

void
ar5416EnableDfs(struct ath_hal *ah, HAL_PHYERR_PARAM *pe)
{
    u_int32_t val;
    struct ath_hal_private  *ahp = AH_PRIVATE(ah);
    HAL_CHANNEL_INTERNAL *ichan=ahp->ah_curchan;

    val = OS_REG_READ(ah, AR_PHY_RADAR_0);
    if (pe->pe_firpwr != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_FIRPWR;
        val |= SM(pe->pe_firpwr, AR_PHY_RADAR_0_FIRPWR);
    }
    if (pe->pe_rrssi != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_RRSSI;
        val |= SM(pe->pe_rrssi, AR_PHY_RADAR_0_RRSSI);
    }
    if (pe->pe_height != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_HEIGHT;
        val |= SM(pe->pe_height, AR_PHY_RADAR_0_HEIGHT);
    }
    if (pe->pe_prssi != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_PRSSI;
        val |= SM(pe->pe_prssi, AR_PHY_RADAR_0_PRSSI);
    }
    if (pe->pe_inband != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_INBAND;
        val |= SM(pe->pe_inband, AR_PHY_RADAR_0_INBAND);
    }
    
    /*Enable FFT data*/
    val |= AR_PHY_RADAR_0_FFT_ENA;

    OS_REG_WRITE(ah, AR_PHY_RADAR_0, val | AR_PHY_RADAR_0_ENA);
    
    val = OS_REG_READ(ah, AR_PHY_RADAR_1);
    val |=( AR_PHY_RADAR_1_MAX_RRSSI |
            AR_PHY_RADAR_1_BLOCK_CHECK );

    if (pe->pe_maxlen != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_1_MAXLEN;
        val |= SM(pe->pe_maxlen, AR_PHY_RADAR_1_MAXLEN);
    }

    OS_REG_WRITE(ah, AR_PHY_RADAR_1, val);
            
    if (ath_hal_getcapability(ah, HAL_CAP_EXT_CHAN_DFS, 0, 0) == HAL_OK) {
        if (IS_CHAN_HT40(ichan)) {
            /*Enable extension channel radar detection*/
            val = OS_REG_READ(ah, AR_PHY_RADAR_EXT);
            OS_REG_WRITE(ah, AR_PHY_RADAR_EXT, val | AR_PHY_RADAR_EXT_ENA);
        } else {
            /*HT20 mode, disable extension channel radar detect*/
            val = OS_REG_READ(ah, AR_PHY_RADAR_EXT);
            OS_REG_WRITE(ah, AR_PHY_RADAR_EXT, val & ~AR_PHY_RADAR_EXT_ENA);
        }
    }

    if (pe->pe_relstep != HAL_PHYERR_PARAM_NOVAL) {
        val = OS_REG_READ(ah, AR_PHY_RADAR_1);
        val &= ~AR_PHY_RADAR_1_RELSTEP_THRESH;
        val |= SM(pe->pe_relstep, AR_PHY_RADAR_1_RELSTEP_THRESH);
        OS_REG_WRITE(ah, AR_PHY_RADAR_1, val);
    }
    if (pe->pe_relpwr != HAL_PHYERR_PARAM_NOVAL) {
        val = OS_REG_READ(ah, AR_PHY_RADAR_1);
        val &= ~AR_PHY_RADAR_1_RELPWR_THRESH;
        val |= SM(pe->pe_relpwr, AR_PHY_RADAR_1_RELPWR_THRESH);
        OS_REG_WRITE(ah, AR_PHY_RADAR_1, val);
    }
}

/*
 * Get the radar parameter values and return them in the pe
 * structure
 */

void
ar5416GetDfsThresh(struct ath_hal *ah, HAL_PHYERR_PARAM *pe)
{
    u_int32_t val,temp;
    
    val = OS_REG_READ(ah, AR_PHY_RADAR_0);

    temp = MS(val,AR_PHY_RADAR_0_FIRPWR);
    temp |= 0xFFFFFF80;
    pe->pe_firpwr = temp;
    pe->pe_rrssi = MS(val, AR_PHY_RADAR_0_RRSSI);
    pe->pe_height =  MS(val, AR_PHY_RADAR_0_HEIGHT);
    pe->pe_prssi = MS(val, AR_PHY_RADAR_0_PRSSI);
    pe->pe_inband = MS(val, AR_PHY_RADAR_0_INBAND);

    val = OS_REG_READ(ah, AR_PHY_RADAR_1);
    temp = val & AR_PHY_RADAR_1_RELPWR_ENA;
    pe->pe_relpwr = MS(val, AR_PHY_RADAR_1_RELPWR_THRESH);
    if (temp)
        pe->pe_relpwr |= HAL_PHYERR_PARAM_ENABLE;
    temp = val & AR_PHY_RADAR_1_RELSTEP_CHECK;
    pe->pe_relstep = MS(val, AR_PHY_RADAR_1_RELSTEP_THRESH);
    if (temp)
        pe->pe_relstep |= HAL_PHYERR_PARAM_ENABLE;
    pe->pe_maxlen = MS(val, AR_PHY_RADAR_1_MAXLEN);
}

HAL_BOOL
ar5416RadarWait(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    struct ath_hal_private *ahp= AH_PRIVATE(ah);

    if(!ahp->ah_curchan) {
        return AH_TRUE;
    }

    /* Rely on the upper layers to determine that we have spent enough time waiting */
    chan->channel = ahp->ah_curchan->channel;
    chan->channel_flags = ahp->ah_curchan->channel_flags;
    chan->max_reg_tx_power = ahp->ah_curchan->max_reg_tx_power;

    ahp->ah_curchan->priv_flags |= CHANNEL_DFS_CLEAR;
    chan->priv_flags  = ahp->ah_curchan->priv_flags;
    return AH_FALSE;

}

void ar5416_adjust_difs(struct ath_hal *ah, u_int32_t val)
{
    if (val == 0) {
        /*
         * EV 116936:
         * Restore the register values with that of the HAL structure.
         *
         */
        struct ath_hal_5416 *ahp = AH5416(ah);
        HAL_TX_QUEUE_INFO *qi;
        int q;

        HDPRINTF(ah, HAL_DBG_DFS, "%s: restore DIFS \n", __func__);
        for (q = 0; q < 4; q++) {
            qi = &ahp->ah_txq[q];
            OS_REG_WRITE(ah, AR_DLCL_IFS(q),
                    SM(qi->tqi_cwmin, AR_D_LCL_IFS_CWMIN)
                    | SM(qi->tqi_cwmax, AR_D_LCL_IFS_CWMAX)
                    | SM(qi->tqi_aifs, AR_D_LCL_IFS_AIFS));
        }
    } else {
        /*
         * These are values from George Lai and are specific to
         * FCC domain. They are yet to be determined for other domains. 
         */

        HDPRINTF(ah, HAL_DBG_DFS, "%s: set DIFS to default\n", __func__);
        
        OS_REG_WRITE(ah, AR_DLCL_IFS(0), 0x05fffc0f);
        OS_REG_WRITE(ah, AR_DLCL_IFS(1), 0x05f0fc0f);
        OS_REG_WRITE(ah, AR_DLCL_IFS(2), 0x05f03c07);
        OS_REG_WRITE(ah, AR_DLCL_IFS(3), 0x05f01c03);
    }
}

u_int32_t ar5416_dfs_config_fft(struct ath_hal *ah, bool is_enable)
{
    u_int32_t val;

    val = OS_REG_READ(ah, AR_PHY_RADAR_0);

    if (is_enable) {
        val |= AR_PHY_RADAR_0_FFT_ENA;
    } else {
        val &= ~AR_PHY_RADAR_0_FFT_ENA;
    }

    OS_REG_WRITE(ah, AR_PHY_RADAR_0, val);
    val = OS_REG_READ(ah, AR_PHY_RADAR_0);
    return val;
}

void
ar5416_dfs_cac_war(struct ath_hal *ah, u_int32_t start)
{
}
#endif /* ATH_SUPPORT_DFS */

HAL_CHANNEL *ar5416GetExtensionChannel(struct ath_hal *ah)
{
    struct ath_hal_private  *ahp = AH_PRIVATE(ah);
    struct ath_hal_private_tables  *aht = AH_TABLES(ah);
    int i=0;

    HAL_CHANNEL_INTERNAL *ichan=AH_NULL;
    CHAN_CENTERS centers;

    ichan = ahp->ah_curchan;
    ar5416GetChannelCenters(ah, ichan, &centers);
    if (centers.ctl_center == centers.ext_center) {
        return AH_NULL;
    }
    for (i = 0; i < ahp->ah_nchan; i++) {
        ichan = &aht->ah_channels[i];
        if (ichan->channel == centers.ext_center) {
            return (HAL_CHANNEL*)ichan;
        }
    }
    return AH_NULL;
}


HAL_BOOL ar5416IsFastClockEnabled(struct ath_hal *ah)
{
    struct ath_hal_private *ahp= AH_PRIVATE(ah);

    if (AR_SREV_MERLIN_20(ah) && IS_5GHZ_FAST_CLOCK_EN(ah, ahp->ah_curchan)) {
        return AH_TRUE;
    }
    return AH_FALSE;
}

uint16_t
ar5416GetAHDevid(struct ath_hal *ah)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);

    return ahp->ah_devid;
}

#endif
