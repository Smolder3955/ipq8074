/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2008-2010, Atheros Communications Inc. 
 * 
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 * 
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 * 
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5416

#include "ah.h"
#include "ah_internal.h"

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"

/* shorthands to compact tables for readability */
#define	OFDM	IEEE80211_T_OFDM
#define	CCK	IEEE80211_T_CCK
#define	TURBO	IEEE80211_T_TURBO
#define	XR	ATHEROS_T_XR
#define HT      IEEE80211_T_HT

#define AR5416_NUM_OFDM_RATES   8
#define AR5416_NUM_HT_SS_RATES  8
#define AR5416_NUM_HT_DS_RATES  8

static inline void ar5416InitRateTxPowerCCK(struct ath_hal *ah,
       const HAL_RATE_TABLE *rt, int16_t ratesArray[], u_int8_t chainmask);
static inline void ar5416InitRateTxPowerOFDM(struct ath_hal* ah,
       const HAL_RATE_TABLE *rt, int16_t ratesArray[], int rt_offset,
       u_int8_t chainmask);
static inline void ar5416InitRateTxPowerHT(struct ath_hal *ah,
       const HAL_RATE_TABLE *rt, HAL_BOOL is40, int ht40Correction,
       int16_t ratesArray[], int rt_ss_offset, int rt_ds_offset,
       u_int8_t chainmask);

#define AR5416_11A_RT_OFDM_OFFSET    0
HAL_RATE_TABLE ar5416_11a_table = {
	8,  /* number of rates */
	{ 0 },
	{
/*                                                  short            ctrl  */
/*                valid                 rateCode Preamble  dot11Rate Rate */
/*--- OFDM rates ---*/
/*   6 Mb */ {  AH_TRUE, OFDM,    6000,     0x0b,    0x00, (0x80|12),   0 },
/*   9 Mb */ {  AH_TRUE, OFDM,    9000,     0x0f,    0x00,        18,   0 },
/*  12 Mb */ {  AH_TRUE, OFDM,   12000,     0x0a,    0x00, (0x80|24),   2 },
/*  18 Mb */ {  AH_TRUE, OFDM,   18000,     0x0e,    0x00,        36,   2 },
/*  24 Mb */ {  AH_TRUE, OFDM,   24000,     0x09,    0x00, (0x80|48),   4 },
/*  36 Mb */ {  AH_TRUE, OFDM,   36000,     0x0d,    0x00,        72,   4 },
/*  48 Mb */ {  AH_TRUE, OFDM,   48000,     0x08,    0x00,        96,   4 },
/*  54 Mb */ {  AH_TRUE, OFDM,   54000,     0x0c,    0x00,       108,   4 }
	},
};

HAL_RATE_TABLE ar5416_turbo_table = {
	8,  /* number of rates */
	{ 0 },
	{
/*                                                 short            ctrl  */
/*                valid                rateCode Preamble  dot11Rate Rate */
/*   6 Mb */ {  AH_TRUE, TURBO,   6000,    0x0b,    0x00, (0x80|12),   0 },
/*   9 Mb */ {  AH_TRUE, TURBO,   9000,    0x0f,    0x00,        18,   0 },
/*  12 Mb */ {  AH_TRUE, TURBO,  12000,    0x0a,    0x00, (0x80|24),   2 },
/*  18 Mb */ {  AH_TRUE, TURBO,  18000,    0x0e,    0x00,        36,   2 },
/*  24 Mb */ {  AH_TRUE, TURBO,  24000,    0x09,    0x00, (0x80|48),   4 },
/*  36 Mb */ {  AH_TRUE, TURBO,  36000,    0x0d,    0x00,        72,   4 },
/*  48 Mb */ {  AH_TRUE, TURBO,  48000,    0x08,    0x00,        96,   4 },
/*  54 Mb */ {  AH_TRUE, TURBO,  54000,    0x0c,    0x00,       108,   4 }
	},
};

HAL_RATE_TABLE ar5416_11b_table = {
	4,  /* number of rates */
	{ 0 },
	{
/*                                                 short            ctrl  */
/*                valid                rateCode Preamble  dot11Rate Rate */
/*   1 Mb */ {  AH_TRUE,  CCK,    1000,    0x1b,    0x00, (0x80| 2),   0 },
/*   2 Mb */ {  AH_TRUE,  CCK,    2000,    0x1a,    0x04, (0x80| 4),   1 },
/* 5.5 Mb */ {  AH_TRUE,  CCK,    5500,    0x19,    0x04, (0x80|11),   1 },
/*  11 Mb */ {  AH_TRUE,  CCK,   11000,    0x18,    0x04, (0x80|22),   1 }
	},
};


/* Venice TODO: roundUpRate() is broken when the rate table does not represent rates
 * in increasing order  e.g.  5.5, 11, 6, 9.    
 * An average rate of 6 Mbps will currently map to 11 Mbps. 
 */
#define AR5416_11G_RT_OFDM_OFFSET    4
HAL_RATE_TABLE ar5416_11g_table = {
	12,  /* number of rates */
	{ 0 },
	{
/*                                                 short            ctrl  */
/*                valid                rateCode Preamble  dot11Rate Rate */
/*   1 Mb */ {  AH_TRUE, CCK,     1000,    0x1b,    0x00, (0x80| 2),   0 },
/*   2 Mb */ {  AH_TRUE, CCK,     2000,    0x1a,    0x04, (0x80| 4),   1 },
/* 5.5 Mb */ {  AH_TRUE, CCK,     5500,    0x19,    0x04, (0x80|11),   2 },
/*  11 Mb */ {  AH_TRUE, CCK,    11000,    0x18,    0x04, (0x80|22),   3 },
/* Hardware workaround - remove rates 6, 9 from rate ctrl */
/*--- OFDM rates ---*/
/*   6 Mb */ { AH_FALSE, OFDM,    6000,    0x0b,    0x00,        12,   4 },
/*   9 Mb */ { AH_FALSE, OFDM,    9000,    0x0f,    0x00,        18,   4 },
/*  12 Mb */ {  AH_TRUE, OFDM,   12000,    0x0a,    0x00,        24,   6 },
/*  18 Mb */ {  AH_TRUE, OFDM,   18000,    0x0e,    0x00,        36,   6 },
/*  24 Mb */ {  AH_TRUE, OFDM,   24000,    0x09,    0x00,        48,   8 },
/*  36 Mb */ {  AH_TRUE, OFDM,   36000,    0x0d,    0x00,        72,   8 },
/*  48 Mb */ {  AH_TRUE, OFDM,   48000,    0x08,    0x00,        96,   8 },
/*  54 Mb */ {  AH_TRUE, OFDM,   54000,    0x0c,    0x00,       108,   8 }
	},
};

HAL_RATE_TABLE ar5416_xr_table = {
	13,		/* number of rates */
	{ 0 },
	{
/*                                                 short            ctrl  */
/*                valid                rateCode Preamble  dot11Rate Rate */
/* 0.25 Mb */{  AH_TRUE,   XR,    250,     0x03,    0x00, (0x80| 1),   0, 612, 612 },
/* 0.5 Mb */ {  AH_TRUE,   XR,    500,     0x07,    0x00, (0x80| 1),   0, 457, 457},
/*   1 Mb */ {  AH_TRUE,   XR,   1000,     0x02,    0x00, (0x80| 2),   1, 228, 228 },
/*   2 Mb */ {  AH_TRUE,   XR,   2000,     0x06,    0x00, (0x80| 4),   2, 160, 160,},
/*   3 Mb */ {  AH_TRUE,   XR,   3000,     0x01,    0x00, (0x80| 6),   3, 140, 140 },
/*   6 Mb */ {  AH_TRUE, OFDM,    6000,    0x0b,    0x00, (0x80|12),   4, 60,  60  },
/*   9 Mb */ {  AH_TRUE, OFDM,    9000,    0x0f,    0x00,        18,   4, 60,  60  },
/*  12 Mb */ {  AH_TRUE, OFDM,   12000,    0x0a,    0x00, (0x80|24),   6, 48,  48  },
/*  18 Mb */ {  AH_TRUE, OFDM,   18000,    0x0e,    0x00,        36,   6, 48,  48  },
/*  24 Mb */ {  AH_TRUE, OFDM,   24000,    0x09,    0x00,        48,   8, 44,  44  },
/*  36 Mb */ {  AH_TRUE, OFDM,   36000,    0x0d,    0x00,        72,   8, 44,  44  },
/*  48 Mb */ {  AH_TRUE, OFDM,   48000,    0x08,    0x00,        96,   8, 44,  44  },
/*  54 Mb */ {  AH_TRUE, OFDM,   54000,    0x0c,    0x00,       108,   8, 44,  44  }
	},
};

#define AR5416_11NG_RT_OFDM_OFFSET       4
#define AR5416_11NG_RT_HT_SS_OFFSET      12
#define AR5416_11NG_RT_HT_DS_OFFSET      20
HAL_RATE_TABLE ar5416_11ng_table = {

    28,  /* number of rates */
    { 0 },
    {
/*                                                 short            ctrl  */
/*                valid                rateCode Preamble  dot11Rate Rate */
/*   1 Mb */ {  AH_TRUE, CCK,     1000,    0x1b,    0x00, (0x80| 2),   0 },
/*   2 Mb */ {  AH_TRUE, CCK,     2000,    0x1a,    0x04, (0x80| 4),   1 },
/* 5.5 Mb */ {  AH_TRUE, CCK,     5500,    0x19,    0x04, (0x80|11),   2 },
/*  11 Mb */ {  AH_TRUE, CCK,    11000,    0x18,    0x04, (0x80|22),   3 },
/* Hardware workaround - remove rates 6, 9 from rate ctrl */
/*--- OFDM rates ---*/
/*   6 Mb */ { AH_FALSE, OFDM,    6000,    0x0b,    0x00,        12,   4 },
/*   9 Mb */ { AH_FALSE, OFDM,    9000,    0x0f,    0x00,        18,   4 },
/*  12 Mb */ {  AH_TRUE, OFDM,   12000,    0x0a,    0x00,        24,   6 },
/*  18 Mb */ {  AH_TRUE, OFDM,   18000,    0x0e,    0x00,        36,   6 },
/*  24 Mb */ {  AH_TRUE, OFDM,   24000,    0x09,    0x00,        48,   8 },
/*  36 Mb */ {  AH_TRUE, OFDM,   36000,    0x0d,    0x00,        72,   8 },
/*  48 Mb */ {  AH_TRUE, OFDM,   48000,    0x08,    0x00,        96,   8 },
/*  54 Mb */ {  AH_TRUE, OFDM,   54000,    0x0c,    0x00,       108,   8 },
/*--- HT SS rates ---*/
/* 6.5 Mb */ {  AH_TRUE, HT,      6500,    0x80,    0x00,      	  0,   4 },
/*  13 Mb */ {  AH_TRUE, HT,  	 13000,    0x81,    0x00,      	  1,   6 },
/*19.5 Mb */ {  AH_TRUE, HT,     19500,    0x82,    0x00,         2,   6 },
/*  26 Mb */ {  AH_TRUE, HT,  	 26000,    0x83,    0x00,         3,   8 },
/*  39 Mb */ {  AH_TRUE, HT,  	 39000,    0x84,    0x00,         4,   8 },
/*  52 Mb */ {  AH_TRUE, HT,   	 52000,    0x85,    0x00,         5,   8 },
/*58.5 Mb */ {  AH_TRUE, HT,  	 58500,    0x86,    0x00,         6,   8 },
/*  65 Mb */ {  AH_TRUE, HT,  	 65000,    0x87,    0x00,         7,   8 },
/*--- HT DS rates ---*/
/*  13 Mb */ {  AH_TRUE, HT,  	 13000,    0x88,    0x00,         8,   4 },
/*  26 Mb */ {  AH_TRUE, HT,  	 26000,    0x89,    0x00,         9,   6 },
/*  39 Mb */ {  AH_TRUE, HT,  	 39000,    0x8a,    0x00,        10,   6 },
/*  52 Mb */ {  AH_TRUE, HT,  	 52000,    0x8b,    0x00,        11,   8 },
/*  78 Mb */ {  AH_TRUE, HT,  	 78000,    0x8c,    0x00,        12,   8 },
/* 104 Mb */ {  AH_TRUE, HT, 	104000,    0x8d,    0x00,        13,   8 },
/* 117 Mb */ {  AH_TRUE, HT, 	117000,    0x8e,    0x00,        14,   8 },
/* 130 Mb */ {  AH_TRUE, HT, 	130000,    0x8f,    0x00,        15,   8 },
	},
};

#define AR5416_11NA_RT_OFDM_OFFSET       0
#define AR5416_11NA_RT_HT_SS_OFFSET      8
#define AR5416_11NA_RT_HT_DS_OFFSET      16
static HAL_RATE_TABLE ar5416_11na_table = {

    24,  /* number of rates */
    { 0 },
    {
/*                                                 short            ctrl  */
/*                valid                rateCode Preamble  dot11Rate Rate */
/*--- OFDM rates ---*/
/*   6 Mb */ {  AH_TRUE, OFDM,    6000,    0x0b,    0x00, (0x80|12),   0 },
/*   9 Mb */ {  AH_TRUE, OFDM,    9000,    0x0f,    0x00,        18,   0 },
/*  12 Mb */ {  AH_TRUE, OFDM,   12000,    0x0a,    0x00, (0x80|24),   2 },
/*  18 Mb */ {  AH_TRUE, OFDM,   18000,    0x0e,    0x00,        36,   2 },
/*  24 Mb */ {  AH_TRUE, OFDM,   24000,    0x09,    0x00, (0x80|48),   4 },
/*  36 Mb */ {  AH_TRUE, OFDM,   36000,    0x0d,    0x00,        72,   4 },
/*  48 Mb */ {  AH_TRUE, OFDM,   48000,    0x08,    0x00,        96,   4 },
/*  54 Mb */ {  AH_TRUE, OFDM,   54000,    0x0c,    0x00,       108,   4 },
/*--- HT SS rates ---*/
/* 6.5 Mb */ {  AH_TRUE, HT,      6500,    0x80,    0x00,         0,   0 },
/*  13 Mb */ {  AH_TRUE, HT,  	 13000,    0x81,    0x00,         1,   2 },
/*19.5 Mb */ {  AH_TRUE, HT,     19500,    0x82,    0x00,         2,   2 },
/*  26 Mb */ {  AH_TRUE, HT,  	 26000,    0x83,    0x00,         3,   4 },
/*  39 Mb */ {  AH_TRUE, HT,  	 39000,    0x84,    0x00,         4,   4 },
/*  52 Mb */ {  AH_TRUE, HT,   	 52000,    0x85,    0x00,         5,   4 },
/*58.5 Mb */ {  AH_TRUE, HT,  	 58500,    0x86,    0x00,         6,   4 },
/*  65 Mb */ {  AH_TRUE, HT,  	 65000,    0x87,    0x00,         7,   4 },
/*--- HT DS rates ---*/
/*  13 Mb */ {  AH_TRUE, HT,  	 13000,    0x88,    0x00,         8,   0 },
/*  26 Mb */ {  AH_TRUE, HT,  	 26000,    0x89,    0x00,         9,   2 },
/*  39 Mb */ {  AH_TRUE, HT,  	 39000,    0x8a,    0x00,        10,   2 },
/*  52 Mb */ {  AH_TRUE, HT,  	 52000,    0x8b,    0x00,        11,   4 },
/*  78 Mb */ {  AH_TRUE, HT,  	 78000,    0x8c,    0x00,        12,   4 },
/* 104 Mb */ {  AH_TRUE, HT, 	104000,    0x8d,    0x00,        13,   4 },
/* 117 Mb */ {  AH_TRUE, HT, 	117000,    0x8e,    0x00,        14,   4 },
/* 130 Mb */ {  AH_TRUE, HT, 	130000,    0x8f,    0x00,        15,   4 },
	},
};

#undef	OFDM
#undef	CCK
#undef	TURBO
#undef	XR
#undef	HT
#undef	HT_HGI

const HAL_RATE_TABLE *
ar5416GetRateTable(struct ath_hal *ah, u_int mode)
{
	HAL_RATE_TABLE *rt;
	switch (mode) {
	case HAL_MODE_11A:
		rt = &ar5416_11a_table;
		break;
	case HAL_MODE_11B:
		rt = &ar5416_11b_table;
		break;
	case HAL_MODE_11G:
		rt =  &ar5416_11g_table;
		break;
	case HAL_MODE_TURBO:
	case HAL_MODE_108G:
		rt =  &ar5416_turbo_table;
		break;
	case HAL_MODE_XR:
		rt = &ar5416_xr_table;
		break;
	case HAL_MODE_11NG_HT20:
	case HAL_MODE_11NG_HT40PLUS:
	case HAL_MODE_11NG_HT40MINUS:
		rt = &ar5416_11ng_table;
		break;
	case HAL_MODE_11NA_HT20:
	case HAL_MODE_11NA_HT40PLUS:
	case HAL_MODE_11NA_HT40MINUS:
		rt = &ar5416_11na_table;
		break;
	default:
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid mode 0x%x\n",
            __func__, mode);
		return AH_NULL;
	}
	ath_hal_setupratetable(ah, rt);
	return rt;
}

int16_t
ar5416GetRateTxPower(struct ath_hal *ah, u_int mode, u_int8_t rate_index,
                     u_int8_t chainmask)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int numChains;

    switch(chainmask) {
    case AR5416_1_CHAINMASK:
        numChains = 1;
        break;
    case AR5416_2LOHI_CHAINMASK:
    case AR5416_2LOMID_CHAINMASK:
        numChains = 2;
        break;
    case AR5416_3_CHAINMASK:
        numChains = 3;
        break;
    default:
        numChains = 3;
    }
    
    return ahp->txPower[rate_index][numChains-1];
}

extern void
ar5416InitRateTxPower(struct ath_hal *ah, u_int mode,
                      HAL_CHANNEL_INTERNAL *chan, int ht40Correction,
                      int16_t powerPerRate[], u_int8_t chainmask)
{
    const HAL_RATE_TABLE *rt;
    HAL_BOOL is40 = IS_CHAN_HT40(chan);

    rt = (const HAL_RATE_TABLE *)ar5416GetRateTable(ah, mode);
    HALASSERT(rt != NULL);

    switch (mode) {
    case HAL_MODE_11A:
        ar5416InitRateTxPowerOFDM(ah, rt, powerPerRate,
                                  AR5416_11A_RT_OFDM_OFFSET, chainmask);
        break;
    case HAL_MODE_11NA_HT20:
    case HAL_MODE_11NA_HT40PLUS:
    case HAL_MODE_11NA_HT40MINUS:
        ar5416InitRateTxPowerOFDM(ah, rt, powerPerRate,
                                  AR5416_11NA_RT_OFDM_OFFSET, chainmask);
        ar5416InitRateTxPowerHT(ah, rt, is40, ht40Correction, powerPerRate,
                                AR5416_11NA_RT_HT_SS_OFFSET,
                                AR5416_11NA_RT_HT_DS_OFFSET, chainmask);
        break;
    case HAL_MODE_11G:
        ar5416InitRateTxPowerCCK(ah, rt, powerPerRate, chainmask);
        ar5416InitRateTxPowerOFDM(ah, rt, powerPerRate,
                                  AR5416_11G_RT_OFDM_OFFSET, chainmask);
        break;
    case HAL_MODE_11B:
        ar5416InitRateTxPowerCCK(ah, rt, powerPerRate, chainmask);
        break;
    case HAL_MODE_11NG_HT20:
    case HAL_MODE_11NG_HT40PLUS:
    case HAL_MODE_11NG_HT40MINUS:
        ar5416InitRateTxPowerCCK(ah, rt, powerPerRate,  chainmask);
        ar5416InitRateTxPowerOFDM(ah, rt, powerPerRate,
                                  AR5416_11NG_RT_OFDM_OFFSET, chainmask);
        ar5416InitRateTxPowerHT(ah, rt, is40, ht40Correction, powerPerRate,
                                AR5416_11NG_RT_HT_SS_OFFSET,
                                AR5416_11NG_RT_HT_DS_OFFSET, chainmask);
        break;
    default:
        HDPRINTF(ah, HAL_DBG_POWER_MGMT, "%s: invalid mode 0x%x\n",
                 __func__, mode);
        HALASSERT(0);
    }

    /* 
     * send txpower read from EEPROM to target 
     */
    ar5416_send_txpower_to_target(ah, mode);
}

#ifdef AH_DEBUG
void
ar5416DumpRateTxPower(struct ath_hal *ah, u_int mode)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    const HAL_RATE_TABLE *rt;
    int i;

    /*
     * Return without wasting cycles in the loops below if it
     * turns out that the printout would be filtered out anyway.
     */
    if ((AH_PRIVATE(ah)->ah_config.ath_hal_debug & HAL_DBG_POWER_MGMT) == 0) {
        return;
    }

    rt = (const HAL_RATE_TABLE *)ar5416GetRateTable(ah, mode);
    HALASSERT(rt != NULL);

    for (i = 0; i < rt->rateCount; i++) {
        int16_t txPower[AR5416_MAX_CHAINS]; 
        int j;

        for ( j = 0 ; j < AR5416_MAX_CHAINS ; j++ ) {
            txPower[j] = ahp->txPower[i][j];
            if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
                /* adjust from -5 dBm reference to 0 dBm reference */
                txPower[j] += AR5416_PWR_TABLE_OFFSET_DB*2;
            }
        }
        HDPRINTF(ah, HAL_DBG_POWER_MGMT,
                       "Index[%2d] Rate[0x%02x] %6d kbps "
                       "Power (1 Chain) [%2d.%1d dBm] "
                       "Power (2 Chain) [%2d.%1d dBm] "
                       "Power (3 Chain) [%2d.%1d dBm]\n",
                       i, rt->info[i].rate_code, rt->info[i].rateKbps,
                       txPower[0]/2, txPower[0]%2 * 5,
                       txPower[1]/2, txPower[1]%2 * 5,
                       txPower[2]/2, txPower[2]%2 * 5);
    }
}
#endif /* AH_DEBUG */

static inline void
ar5416InitRateTxPowerCCK(struct ath_hal *ah, const HAL_RATE_TABLE *rt,
                         int16_t ratesArray[], u_int8_t chainmask)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    /*
     * Pick the lower of the long-preamble txpower, and short-preamble tx power.
     * Unfortunately, the rate table doesn't have separate entries for these!.
     */
    switch(chainmask) {
    case AR5416_1_CHAINMASK:
        ahp->txPower[0][0] = ratesArray[rate1l];
        ahp->txPower[1][0] = AH_MIN(ratesArray[rate2l], ratesArray[rate2s]);
        ahp->txPower[2][0] = AH_MIN(ratesArray[rate5_5l], ratesArray[rate5_5s]);
        ahp->txPower[3][0] = AH_MIN(ratesArray[rate11l], ratesArray[rate11s]);
        break;
    case AR5416_2LOHI_CHAINMASK:
    case AR5416_2LOMID_CHAINMASK:
        ahp->txPower[0][1] = ratesArray[rate1l];
        ahp->txPower[1][1] = AH_MIN(ratesArray[rate2l], ratesArray[rate2s]);
        ahp->txPower[2][1] = AH_MIN(ratesArray[rate5_5l], ratesArray[rate5_5s]);
        ahp->txPower[3][1] = AH_MIN(ratesArray[rate11l], ratesArray[rate11s]);
        break;
    case AR5416_3_CHAINMASK:
        ahp->txPower[0][2] = ratesArray[rate1l];
        ahp->txPower[1][2] = AH_MIN(ratesArray[rate2l], ratesArray[rate2s]);
        ahp->txPower[2][2] = AH_MIN(ratesArray[rate5_5l], ratesArray[rate5_5s]);
        ahp->txPower[3][2] = AH_MIN(ratesArray[rate11l], ratesArray[rate11s]);
        break;
    default:
        HDPRINTF(ah, HAL_DBG_POWER_MGMT, "%s: invalid chainmask 0x%x\n",
                 __func__, chainmask);
        break;
    }
}

static inline void
ar5416InitRateTxPowerOFDM(struct ath_hal *ah, const HAL_RATE_TABLE *rt,
                          int16_t ratesArray[], int rt_offset,
                          u_int8_t chainmask)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int i, j;

    j = AR5416_RATES_OFDM_OFFSET;
    for (i = rt_offset; i < rt_offset + AR5416_NUM_OFDM_RATES; i++) {
        switch(chainmask) {
        case AR5416_1_CHAINMASK:
            ahp->txPower[i][0] = ratesArray[j];
            break;
        case AR5416_2LOHI_CHAINMASK:
        case AR5416_2LOMID_CHAINMASK:
            ahp->txPower[i][1] = ratesArray[j];
            break;
        case AR5416_3_CHAINMASK:
            ahp->txPower[i][2] = ratesArray[j];
            break;
        default:
            HDPRINTF(ah, HAL_DBG_POWER_MGMT, "%s: invalid chainmask 0x%x\n",
                     __func__, chainmask);
            break;
        }
        j++;
    }
}

/*
 * TODO: We skip the dup and ext-only powers since nobody uses them.
 */
static inline void
ar5416InitRateTxPowerHT(struct ath_hal *ah, const HAL_RATE_TABLE *rt, HAL_BOOL is40,
                        int ht40Correction, int16_t ratesArray[],
                        int rt_ss_offset, int rt_ds_offset, u_int8_t chainmask)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int i,j, correction;

    correction = (is40) ? ht40Correction : 0;

    /*
     * The chip is either in 20MHz Phy mode or 20/40 Phy mode.
     * 20/40 Mac modes vary during operation.
     */
    j = (is40) ? AR5416_RATES_HT40_OFFSET : AR5416_RATES_HT20_OFFSET;
    for (i = rt_ss_offset; i < rt_ss_offset + AR5416_NUM_HT_SS_RATES; i++) {
        switch(chainmask) {
        case AR5416_1_CHAINMASK:
            ahp->txPower[i][0] = ratesArray[j]+correction;
            break;
        case AR5416_2LOHI_CHAINMASK:
        case AR5416_2LOMID_CHAINMASK:
            ahp->txPower[i][1] = ratesArray[j]+correction;
            break;
        case AR5416_3_CHAINMASK:
            ahp->txPower[i][2] = ratesArray[j]+correction;
            break;
        default:
            HDPRINTF(ah, HAL_DBG_POWER_MGMT, "%s: invalid chainmask 0x%x\n",
                     __func__, chainmask);
            break;
        }
        j++;
    }

    /*
     * The single stream MCS tx powers are re-used for dual stream.
     */
    j = (is40) ? AR5416_RATES_HT40_OFFSET : AR5416_RATES_HT20_OFFSET;
    for (i = rt_ds_offset; i < rt_ds_offset + AR5416_NUM_HT_DS_RATES; i++) {
        switch(chainmask) {
        case AR5416_1_CHAINMASK:
            ahp->txPower[i][0] = ratesArray[j]+correction;
            break;
        case AR5416_2LOHI_CHAINMASK:
        case AR5416_2LOMID_CHAINMASK:
            ahp->txPower[i][1] = ratesArray[j]+correction;
            break;
        case AR5416_3_CHAINMASK:
            ahp->txPower[i][2] = ratesArray[j]+correction;
            break;
        default:
            HDPRINTF(ah, HAL_DBG_POWER_MGMT, "%s: invalid chainmask 0x%x\n",
                     __func__, chainmask);
            break;
        }
        j++;
    }
}

#ifdef ART_BUILD
HAL_STATUS
ath_hal_get_rate_power_limit_from_eeprom(struct ath_hal *ah, u_int16_t freq,
                                        int8_t *max_rate_power, int8_t *min_rate_power)
{
    *max_rate_power = 0;
    *min_rate_power = 0;
    return HAL_OK;
}
#endif /* ART_BUILD */

#endif /* AH_SUPPORT_AR5416 */
