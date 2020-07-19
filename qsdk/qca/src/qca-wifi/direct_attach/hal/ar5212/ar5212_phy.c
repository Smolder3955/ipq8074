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
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5212

#include "ah.h"
#include "ah_internal.h"

#include "ar5212/ar5212.h"

/* shorthands to compact tables for readability */
#define	OFDM	IEEE80211_T_OFDM
#define	CCK	IEEE80211_T_CCK
#define	TURBO	IEEE80211_T_TURBO
#define	XR	ATHEROS_T_XR

HAL_RATE_TABLE ar5212_11a_table = {
	8,  /* number of rates */
	{ 0 },
	{
/*                                                  short            ctrl  */
/*                valid                 rateCode Preamble  dot11Rate Rate */
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

HAL_RATE_TABLE ar5212_11a_half_table = {
	8,  /* number of rates */
	{ 0 },
	{
/*                                                  short            ctrl  */
/*                valid                 rateCode Preamble  dot11Rate Rate */
/*   6 Mb */ {  AH_TRUE, OFDM,    3000,     0x0b,    0x00, (0x80|6),   0 },
/*   9 Mb */ {  AH_TRUE, OFDM,    4500,     0x0f,    0x00,        9,   0 },
/*  12 Mb */ {  AH_TRUE, OFDM,    6000,     0x0a,    0x00, (0x80|12),   2 },
/*  18 Mb */ {  AH_TRUE, OFDM,    9000,     0x0e,    0x00,        18,   2 },
/*  24 Mb */ {  AH_TRUE, OFDM,   12000,     0x09,    0x00, (0x80|24),   4 },
/*  36 Mb */ {  AH_TRUE, OFDM,   18000,     0x0d,    0x00,        36,   4 },
/*  48 Mb */ {  AH_TRUE, OFDM,   24000,     0x08,    0x00,        48,   4 },
/*  54 Mb */ {  AH_TRUE, OFDM,   27000,     0x0c,    0x00,       54,   4 }
	},
};

HAL_RATE_TABLE ar5212_11a_quarter_table = {
	8,  /* number of rates */
	{ 0 },
	{
/*                                                  short            ctrl  */
/*                valid                 rateCode Preamble  dot11Rate Rate */
/*   6 Mb */ {  AH_TRUE, OFDM,    1500,     0x0b,    0x00, (0x80|3),   0 },
/*   9 Mb */ {  AH_TRUE, OFDM,    2250,     0x0f,    0x00,        4,   0 },
/*  12 Mb */ {  AH_TRUE, OFDM,    3000,     0x0a,    0x00, (0x80|6),   2 },
/*  18 Mb */ {  AH_TRUE, OFDM,    4500,     0x0e,    0x00,        9,   2 },
/*  24 Mb */ {  AH_TRUE, OFDM,    6000,     0x09,    0x00, (0x80|12),   4 },
/*  36 Mb */ {  AH_TRUE, OFDM,    9000,     0x0d,    0x00,        18,   4 },
/*  48 Mb */ {  AH_TRUE, OFDM,   12000,     0x08,    0x00,        24,   4 },
/*  54 Mb */ {  AH_TRUE, OFDM,   13500,     0x0c,    0x00,       27,   4 }
	},
};

HAL_RATE_TABLE ar5212_turbog_table = {
	7,  /* number of rates */
	{ 0 },
	{
/*                                                 short            ctrl  */
/*                valid                rateCode Preamble  dot11Rate Rate */
/*   6 Mb */ {  AH_TRUE, TURBO,   6000,    0x0b,    0x00, (0x80|12),   0 },
/*  12 Mb */ {  AH_TRUE, TURBO,  12000,    0x0a,    0x00, (0x80|24),   2 },
/*  18 Mb */ {  AH_TRUE, TURBO,  18000,    0x0e,    0x00,        36,   2 },
/*  24 Mb */ {  AH_TRUE, TURBO,  24000,    0x09,    0x00, (0x80|48),   3 },
/*  36 Mb */ {  AH_TRUE, TURBO,  36000,    0x0d,    0x00,        72,   3 },
/*  48 Mb */ {  AH_TRUE, TURBO,  48000,    0x08,    0x00,        96,   3 },
/*  54 Mb */ {  AH_TRUE, TURBO,  54000,    0x0c,    0x00,       108,   3 }
	},
};

HAL_RATE_TABLE ar5212_turboa_table = {
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

HAL_RATE_TABLE ar5212_11b_table = {
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
HAL_RATE_TABLE ar5212_11g_table = {
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

HAL_RATE_TABLE ar5212_xr_table = {
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

#undef	OFDM
#undef	CCK
#undef	TURBO
#undef	XR

const HAL_RATE_TABLE *
ar5212GetRateTable(struct ath_hal *ah, u_int mode)
{
	HAL_RATE_TABLE *rt;
	switch (mode) {
	case HAL_MODE_11A:
		rt = &ar5212_11a_table;
		if (AH_PRIVATE(ah)->ah_curchan) {
			if (IS_CHAN_HALF_RATE(AH_PRIVATE(ah)->ah_curchan)) {
				rt = &ar5212_11a_half_table;
			} else if (IS_CHAN_QUARTER_RATE(
					AH_PRIVATE(ah)->ah_curchan)) {
				rt = &ar5212_11a_quarter_table;
			}
		}
		break;
	case HAL_MODE_11B:
		rt = &ar5212_11b_table;
		break;
	case HAL_MODE_11G:
		rt =  &ar5212_11g_table;
		break;
	case HAL_MODE_TURBO:
		rt =  &ar5212_turboa_table;
		break;
	case HAL_MODE_108G:
		rt =  &ar5212_turbog_table;
		break;
	case HAL_MODE_XR:
		rt = &ar5212_xr_table;
		break;
	case HAL_MODE_11A_HALF_RATE:
		rt = &ar5212_11a_half_table;
		break;
	case HAL_MODE_11A_QUARTER_RATE:
		rt = &ar5212_11a_quarter_table;
		break;
	default:
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid mode 0x%x\n", __func__, mode);
		return AH_NULL;
	}
	ath_hal_setupratetable(ah, rt);
	return rt;
}
#endif /* AH_SUPPORT_AR5212 */
