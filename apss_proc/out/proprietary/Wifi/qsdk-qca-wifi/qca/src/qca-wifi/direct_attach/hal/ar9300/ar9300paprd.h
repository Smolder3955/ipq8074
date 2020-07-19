/*

 * Copyright (c) 2002-2010 Atheros Communications, Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 

 */



#ifndef __AR9300PAPRD_H__

#define __AR9300PAPRD_H__



#include <ah.h>

#include "ar9300.h"

#include "ar9300phy.h"

#define AH_PAPRD_AM_PM_MASK              0x1ffffff
#define AH_PAPRD_IDEAL_AGC2_PWR_RANGE    0xe0
extern int ar9300_paprd_init_table(struct ath_hal *ah, HAL_CHANNEL *chan);
extern HAL_STATUS ar9300_paprd_setup_gain_table(struct ath_hal *ah, int chain_num);
extern HAL_STATUS ar9300_paprd_create_curve(struct ath_hal *ah, HAL_CHANNEL *chan, int chain_num);
extern int ar9300_paprd_is_done(struct ath_hal *ah);
extern void ar9300_enable_paprd(struct ath_hal *ah, bool enable_flag, HAL_CHANNEL * chan);
extern void ar9300_swizzle_paprd_entries(struct ath_hal *ah, unsigned int txchain);
extern void ar9300_populate_paprd_single_table(struct ath_hal *ah, HAL_CHANNEL *chan, int chain_num);
extern void ar9300_paprd_dec_tx_pwr(struct ath_hal *ah);
extern int ar9300_paprd_thermal_send(struct ath_hal *ah);
#endif

