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

#ifdef AH_SUPPORT_AR5212
#include "ah.h"
#include "ah_internal.h"

#include "ar5212/ar5212.h"
#include "ar5212/ar5212reg.h"
#include "ar5212/ar5212desc.h"
#include "ar5212/ar5212phy.h"

#define HAL_RADAR_WAIT_TIME		60*1000000	/* 1 minute in usecs */

/* 
 * Default 5212/5312 radar phy parameters
 */
#define AR5212_DFS_FIRPWR	-41
#define AR5212_DFS_RRSSI	12
#define AR5212_DFS_HEIGHT	20
#define AR5212_DFS_PRSSI	22
#define AR5212_DFS_INBAND	6

#ifdef ATH_SUPPORT_DFS

void
ar5212EnableDfs(struct ath_hal *ah, HAL_PHYERR_PARAM *pe)
{
	u_int32_t val;
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
	OS_REG_WRITE(ah, AR_PHY_RADAR_0, val | AR_PHY_RADAR_0_ENA);

}

void
ar5212GetDfsThresh(struct ath_hal *ah, HAL_PHYERR_PARAM *pe)
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

	pe->pe_relpwr = 0;
	pe->pe_relstep = 0;
	pe->pe_maxlen = 0;
}

void ar5212_adjust_difs(struct ath_hal *ah, u_int32_t val)
{
}

u_int32_t ar5212_dfs_config_fft(struct ath_hal *ah, bool is_enable)
{
    return 0;
}

void
ar5212_dfs_cac_war(struct ath_hal *ah, u_int32_t start)
{
}
#endif /* ATH_SUPPORT_DFS */

HAL_BOOL
ar5212RadarWait(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    struct ath_hal_private *ahp= AH_PRIVATE(ah);
#ifdef AH_SUPPORT_DFS
    u_int64_t tsf;

    if(!ahp->ah_curchan) return AH_TRUE;
    tsf = ar5212GetTsf64(ah);
    ahp->ah_curchan->ah_channel_time  += (tsf - ahp->ah_curchan->ah_tsf_last);
    ahp->ah_curchan->ah_tsf_last = tsf;
    chan->channel = ahp->ah_curchan->channel;
    chan->channel_flags = ahp->ah_curchan->channel_flags;
    chan->priv_flags  = ahp->ah_curchan->priv_flags;
    chan->max_reg_tx_power = ahp->ah_curchan->max_reg_tx_power;
    if (ahp->ah_curchan->ah_channel_time > (HAL_RADAR_WAIT_TIME)) {
        ahp->ah_curchan->priv_flags |= CHANNEL_DFS_CLEAR;
        chan->priv_flags  = ahp->ah_curchan->priv_flags;
        ar5212TxEnable(ah,AH_TRUE);
        return AH_FALSE;
    }
    return AH_TRUE;
#else
    chan->channel = ahp->ah_curchan->channel;
    ahp->ah_curchan->priv_flags |= CHANNEL_DFS_CLEAR;
    chan->channel_flags = ahp->ah_curchan->channel_flags;
    chan->priv_flags  = ahp->ah_curchan->priv_flags;
    chan->max_reg_tx_power = ahp->ah_curchan->max_reg_tx_power;
    return AH_FALSE;
#endif /* AH_SUPPORT_DFS */
}

HAL_CHANNEL *ar5212GetExtensionChannel(struct ath_hal *ah)
{
    return AH_NULL;
}

HAL_BOOL ar5212IsFastClockEnabled(struct ath_hal *ah)
{
    return AH_FALSE;
}

uint16_t
ar5212GetAHDevid(struct ath_hal *ah)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);

    return ahp->ah_devid;
}

#endif /* AH_SUPPORT_AR5212 */

