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

#ifdef AH_SUPPORT_5112

#include "ah.h"
#include "ah_internal.h"

#include "ar5212/ar5212.h"
#include "ar5212/ar5212reg.h"
#include "ar5212/ar5212phy.h"

/* Add static register initialization vectors */
#define AH_5212_5112
#include "ar5212/ar5212.ini"

#define	N(a)	(sizeof(a)/sizeof(a[0]))

/*
 * WAR for bug 6773.  OS_DELAY() does a PIO READ on the PCI bus which allows
 * other cards' DMA reads to complete in the middle of our reset.
 */
#define WAR_6773(x) do {		\
	if ((++(x) % 64) == 0)		\
		OS_DELAY(1);		\
} while (0)

#define REG_WRITE_ARRAY(regArray, column, regWr) do {                  	\
	int r;								\
	for (r = 0; r < N(regArray); r++) {				\
		OS_REG_WRITE(ah, (regArray)[r][0], (regArray)[r][(column)]);\
		WAR_6773(regWr);					\
	}								\
} while (0)

#define REG_WRITE_RF_ARRAY(regArray, regData, regWr) do {               \
	int r;								\
	for (r = 0; r < N(regArray); r++) {				\
		OS_REG_WRITE(ah, (regArray)[r][0], (regData)[r]);	\
		WAR_6773(regWr);					\
	}								\
} while (0)

static	void ar5212GetLowerUpperIndex(u_int16_t v,
		u_int16_t *lp, u_int16_t listSize,
		u_int32_t *vlo, u_int32_t *vhi);
static HAL_BOOL getFullPwrTable(u_int16_t numPcdacs, u_int16_t *pcdacs,
		int16_t *power, int16_t maxPower, int16_t *retVals);
static int16_t getPminAndPcdacTableFromPowerTable(int16_t *pwrTableT4,
		u_int16_t retVals[]);
static int16_t getPminAndPcdacTableFromTwoPowerTables(int16_t *pwrTableLXpdT4,
		int16_t *pwrTableHXpdT4, u_int16_t retVals[], int16_t *pMid);
static int16_t interpolate_signed(u_int16_t target,
		u_int16_t srcLeft, u_int16_t srcRight,
		int16_t targetLeft, int16_t targetRight);

extern	void ar5212ModifyRfBuffer(u_int32_t *rfBuf, u_int32_t reg32,
		u_int32_t numBits, u_int32_t firstBit, u_int32_t column);

typedef struct {
	u_int32_t Bank1Data[N(ar5212Bank1_5112)];
	u_int32_t Bank2Data[N(ar5212Bank2_5112)];
	u_int32_t Bank3Data[N(ar5212Bank3_5112)];
	u_int32_t Bank6Data[N(ar5212Bank6_5112)];
	u_int32_t Bank7Data[N(ar5212Bank7_5112)];
} AR5212_RF_BANKS_5112;

typedef	struct {
		int16_t     pwr_table0[64];
		int16_t     pwr_table1[64];
		int16_t     powTableLXPD[2][64];
		int16_t     powTableHXPD[2][64];
		int16_t     tmpPowerTable[64];
} AR5112_POWER_TABLE;

static void
ar5112WriteRegs(struct ath_hal *ah, u_int modesIndex, u_int freqIndex, int regWrites)
{
	REG_WRITE_ARRAY(ar5212Modes_5112, modesIndex, regWrites);
	REG_WRITE_ARRAY(ar5212Common_5112, 1, regWrites);
	REG_WRITE_ARRAY(ar5212BB_RfGain_5112, freqIndex, regWrites);
}

/*
 * Take the MHz channel value and set the Channel value
 *
 * ASSUMES: Writes enabled to analog bus
 */
static HAL_BOOL
ar5112SetChannel(struct ath_hal *ah,  HAL_CHANNEL_INTERNAL *chan)
{
	u_int32_t channelSel  = 0;
	u_int32_t bModeSynth  = 0;
	u_int32_t aModeRefSel = 0;
	u_int32_t reg32       = 0;
	u_int16_t freq;

	OS_MARK(ah, AH_MARK_SETCHANNEL, chan->channel);

	if (chan->channel < 4800) {
		u_int32_t txctl;

		if (((chan->channel - 2192) % 5) == 0) {
			channelSel = ((chan->channel - 672) * 2 - 3040)/10;
			bModeSynth = 0;
		} else if (((chan->channel - 2224) % 5) == 0) {
			channelSel = ((chan->channel - 704) * 2 - 3040) / 10;
			bModeSynth = 1;
		} else {
			HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel %u MHz\n",
				__func__, chan->channel);
			return AH_FALSE;
		}

		channelSel = (channelSel << 2) & 0xff;
		channelSel = ath_hal_reverse_bits(channelSel, 8);

		txctl = OS_REG_READ(ah, AR_PHY_CCK_TX_CTRL);
		if (chan->channel == 2484) {
			/* Enable channel spreading for channel 14 */
			OS_REG_WRITE(ah, AR_PHY_CCK_TX_CTRL,
				txctl | AR_PHY_CCK_TX_CTRL_JAPAN);
		} else {
			OS_REG_WRITE(ah, AR_PHY_CCK_TX_CTRL,
				txctl &~ AR_PHY_CCK_TX_CTRL_JAPAN);
		}
	} else if (((chan->channel % 5) == 2) && (chan->channel <= 5435)) {
		freq = chan->channel - 2; /* Align to even 5MHz raster */
		channelSel = ath_hal_reverse_bits(
			(u_int32_t)(((freq - 4800)*10)/25 + 1), 8);
            	aModeRefSel = ath_hal_reverse_bits(0, 2);
	} else if ((chan->channel % 20) == 0 && chan->channel >= 5120) {
		channelSel = ath_hal_reverse_bits(
			((chan->channel - 4800) / 20 << 2), 8);
		aModeRefSel = ath_hal_reverse_bits(3, 2);
	} else if ((chan->channel % 10) == 0) {
		channelSel = ath_hal_reverse_bits(
			((chan->channel - 4800) / 10 << 1), 8);
		aModeRefSel = ath_hal_reverse_bits(2, 2);
	} else if ((chan->channel % 5) == 0) {
		channelSel = ath_hal_reverse_bits(
			(chan->channel - 4800) / 5, 8);
		aModeRefSel = ath_hal_reverse_bits(1, 2);
	} else {
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel %u MHz\n",
			__func__, chan->channel);
		return AH_FALSE;
	}

	reg32 = (channelSel << 4) | (aModeRefSel << 2) | (bModeSynth << 1) |
			(1 << 12) | 0x1;
	OS_REG_WRITE(ah, AR_PHY(0x27), reg32 & 0xff);

	reg32 >>= 8;
	OS_REG_WRITE(ah, AR_PHY(0x36), reg32 & 0x7f);

	AH_PRIVATE(ah)->ah_curchan = chan;

#ifdef AH_SUPPORT_DFS
	if (chan->priv_flags & CHANNEL_DFS) {
		struct ar5212RadarState *rs;
		u_int8_t index;

		rs = ar5212GetRadarChanState(ah, &index);
		if (rs != AH_NULL) {
			AH5212(ah)->ah_curchanRadIndex = (int16_t) index;
		} else {
			HDPRINTF(ah, HAL_DBG_DFS, "%s: Couldn't find radar state information\n",
				 __func__);
			return AH_FALSE;
		}
	} else
#endif
		AH5212(ah)->ah_curchanRadIndex = -1;
	return AH_TRUE;
}

/*
 * Return a reference to the requested RF Bank.
 */
static u_int32_t *
ar5112GetRfBank(struct ath_hal *ah, int bank)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	AR5212_RF_BANKS_5112 *pRfBank5112 = ahp->ah_analogBanks;

	HALASSERT(ahp->ah_analogBanks != AH_NULL);
	switch (bank) {
	case 1: return pRfBank5112->Bank1Data;
	case 2: return pRfBank5112->Bank2Data;
	case 3: return pRfBank5112->Bank3Data;
	case 6: return pRfBank5112->Bank6Data;
	case 7: return pRfBank5112->Bank7Data;
	}
	HDPRINTF(ah, HAL_DBG_RF_PARAM, "%s: unknown RF Bank %d requested\n", __func__, bank);
	return AH_NULL;
}

/*
 * Reads EEPROM header info from device structure and programs
 * all rf registers
 *
 * REQUIRES: Access to the analog rf device
 */
static HAL_BOOL
ar5112SetRfRegs(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan,
	u_int16_t modesIndex, u_int16_t *rfXpdGain)
{
#define	RF_BANK_SETUP(_pb, _ix, _col) do {				    \
	int i;								    \
	for (i = 0; i < N(ar5212Bank##_ix##_5112); i++)			    \
		(_pb)->Bank##_ix##Data[i] = ar5212Bank##_ix##_5112[i][_col];\
} while (0)
	struct ath_hal_5212 *ahp = AH5212(ah);
	u_int16_t rfXpdSel, gainI;
	u_int16_t ob5GHz = 0, db5GHz = 0;
	u_int16_t ob2GHz = 0, db2GHz = 0;
	AR5212_RF_BANKS_5112 *pRfBanks = ahp->ah_analogBanks;
	GAIN_VALUES *gv = &ahp->ah_gainValues;
	int regWrites = 0;

	HALASSERT(pRfBanks);

	/* Setup rf parameters */
	switch (chan->channel_flags & CHANNEL_ALL) {
	case CHANNEL_A:
	case CHANNEL_T:
//	case CHANNEL_XR:
		if (chan->channel > 4000 && chan->channel < 5260) {
			ob5GHz = ahp->ah_ob1;
			db5GHz = ahp->ah_db1;
		} else if (chan->channel >= 5260 && chan->channel < 5500) {
			ob5GHz = ahp->ah_ob2;
			db5GHz = ahp->ah_eeprom.ee_db2;
		} else if (chan->channel >= 5500 && chan->channel < 5725) {
			ob5GHz = ahp->ah_ob3;
			db5GHz = ahp->ah_db3;
		} else if (chan->channel >= 5725) {
			ob5GHz = ahp->ah_ob4;
			db5GHz = ahp->ah_db4;
		} else {
			/* XXX else */
		}
		rfXpdSel = ahp->ah_xpd[headerInfo11A];
		gainI = ahp->ah_gainI[headerInfo11A];
		break;
	case CHANNEL_B:
		ob2GHz = ahp->ah_ob2GHz[0];
		db2GHz = ahp->ah_db2GHz[0];
		rfXpdSel = ahp->ah_xpd[headerInfo11B];
		gainI = ahp->ah_gainI[headerInfo11B];
		break;
	case CHANNEL_G:
	case CHANNEL_108G:
		ob2GHz = ahp->ah_ob2GHz[1];
		db2GHz = ahp->ah_ob2GHz[1];
		rfXpdSel = ahp->ah_xpd[headerInfo11G];
		gainI = ahp->ah_gainI[headerInfo11G];
		break;
	default:
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel flags 0x%x\n",
			__func__, chan->channel_flags);
		return AH_FALSE;
	}

	/* Setup Bank 1 Write */
	RF_BANK_SETUP(pRfBanks, 1, 1);

	/* Setup Bank 2 Write */
	RF_BANK_SETUP(pRfBanks, 2, modesIndex);

	/* Setup Bank 3 Write */
	RF_BANK_SETUP(pRfBanks, 3, modesIndex);

	/* Setup Bank 6 Write */
	RF_BANK_SETUP(pRfBanks, 6, modesIndex);

	ar5212ModifyRfBuffer(pRfBanks->Bank6Data, rfXpdSel,     1, 302, 0);

	ar5212ModifyRfBuffer(pRfBanks->Bank6Data, rfXpdGain[0], 2, 270, 0);
	ar5212ModifyRfBuffer(pRfBanks->Bank6Data, rfXpdGain[1], 2, 257, 0);

	if (IS_CHAN_OFDM(chan)) {
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data,
			gv->currStep->paramVal[GP_PWD_138], 1, 168, 3);
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data,
			gv->currStep->paramVal[GP_PWD_137], 1, 169, 3);
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data,
			gv->currStep->paramVal[GP_PWD_136], 1, 170, 3);
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data,
			gv->currStep->paramVal[GP_PWD_132], 1, 174, 3);
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data,
			gv->currStep->paramVal[GP_PWD_131], 1, 175, 3);
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data,
			gv->currStep->paramVal[GP_PWD_130], 1, 176, 3);
	}

	/* Only the 5 or 2 GHz OB/DB need to be set for a mode */
	if (IS_CHAN_2GHZ(chan)) {
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data, ob2GHz, 3, 287, 0);
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data, db2GHz, 3, 290, 0);
	} else {
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data, ob5GHz, 3, 279, 0);
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data, db5GHz, 3, 282, 0);
	}
	
	/* Lower synth voltage for X112 Rev 2.0 only */
	if (IS_RADX112_REV2(ah)) {
		/* Non-Reversed analyg registers - so values are pre-reversed */
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data, 2, 2, 90, 2);
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data, 2, 2, 92, 2);
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data, 2, 2, 94, 2);
		ar5212ModifyRfBuffer(pRfBanks->Bank6Data, 2, 1, 254, 2);
	}

    /* Decrease Power Consumption for 5312/5213 and up */
    if (AH_PRIVATE(ah)->ah_phy_rev >= AR_PHY_CHIP_ID_REV_2) {
        ar5212ModifyRfBuffer(pRfBanks->Bank6Data, 1, 1, 281, 1);
        ar5212ModifyRfBuffer(pRfBanks->Bank6Data, 1, 2, 1, 3);
        ar5212ModifyRfBuffer(pRfBanks->Bank6Data, 1, 2, 3, 3);
        ar5212ModifyRfBuffer(pRfBanks->Bank6Data, 1, 1, 139, 3);
        ar5212ModifyRfBuffer(pRfBanks->Bank6Data, 1, 1, 140, 3);
    }

	/* Setup Bank 7 Setup */
	RF_BANK_SETUP(pRfBanks, 7, modesIndex);
	if (IS_CHAN_OFDM(chan))
		ar5212ModifyRfBuffer(pRfBanks->Bank7Data,
			gv->currStep->paramVal[GP_MIXGAIN_OVR], 2, 37, 0);

	ar5212ModifyRfBuffer(pRfBanks->Bank7Data, gainI, 6, 14, 0);

	/* Adjust params for Derby TX power control */
	if (IS_CHAN_HALF_RATE(chan) || IS_CHAN_QUARTER_RATE(chan)) {
        	u_int32_t	rfDelay, rfPeriod;

        	rfDelay = 0xf;
        	rfPeriod = (IS_CHAN_HALF_RATE(chan)) ?  0x8 : 0xf;
        	ar5212ModifyRfBuffer(pRfBanks->Bank7Data, rfDelay, 4, 58, 0);
        	ar5212ModifyRfBuffer(pRfBanks->Bank7Data, rfPeriod, 4, 70, 0);
	}

	/* Write Analog registers */
	REG_WRITE_RF_ARRAY(ar5212Bank1_5112, pRfBanks->Bank1Data, regWrites);
	REG_WRITE_RF_ARRAY(ar5212Bank2_5112, pRfBanks->Bank2Data, regWrites);
	REG_WRITE_RF_ARRAY(ar5212Bank3_5112, pRfBanks->Bank3Data, regWrites);
	REG_WRITE_RF_ARRAY(ar5212Bank6_5112, pRfBanks->Bank6Data, regWrites);
	REG_WRITE_RF_ARRAY(ar5212Bank7_5112, pRfBanks->Bank7Data, regWrites);

	/* Now that we have reprogrammed rfgain value, clear the flag. */
	ahp->ah_rfgainState = HAL_RFGAIN_INACTIVE;
	return AH_TRUE;
#undef	RF_BANK_SETUP
}

/*
 * Read the transmit power levels from the structures taken from EEPROM
 * Interpolate read transmit power values for this channel
 * Organize the transmit power values into a table for writing into the hardware
 */
static HAL_BOOL
ar5112SetPowerTable(struct ath_hal *ah,
	int16_t *pPowerMin, int16_t *pPowerMax, HAL_CHANNEL_INTERNAL *chan,
	u_int16_t *rfXpdGain)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
    AR5112_POWER_TABLE *pPowTable = ahp->ah_vpdTable;
	u_int32_t numXpdGain = IS_RADX112_REV2(ah) ? 2 : 1;
	u_int32_t    xpdGainMask = 0;
	int16_t     powerMid, *pPowerMid = &powerMid;

	EXPN_DATA_PER_CHANNEL_5112 *pRawCh;
	EEPROM_POWER_EXPN_5112     *pPowerExpn = AH_NULL;

	u_int32_t    ii, jj, kk;
	int16_t     minPwr_t4, maxPwr_t4, Pmin, Pmid;

	u_int32_t    chan_idx_L = 0, chan_idx_R = 0;
	u_int16_t    chan_L, chan_R;

	u_int16_t    pcdacs[10];
	int16_t     powers[10];
	u_int16_t    numPcd;
	u_int16_t    xgainList[2];
	u_int16_t    xpdMask;
	HAL_BOOL    result = AH_TRUE;

	switch (chan->channel_flags & CHANNEL_ALL) {
	case CHANNEL_A:
	case CHANNEL_T:
//	case CHANNEL_XR:
		pPowerExpn = &ahp->ah_modePowerArray5112[headerInfo11A];
		xpdGainMask = ahp->ah_xgain[headerInfo11A];
		break;
	case CHANNEL_B:
		pPowerExpn = &ahp->ah_modePowerArray5112[headerInfo11B];
		xpdGainMask = ahp->ah_xgain[headerInfo11B];
		break;
	case CHANNEL_G:
	case CHANNEL_108G:
		pPowerExpn = &ahp->ah_modePowerArray5112[headerInfo11G];
		xpdGainMask = ahp->ah_xgain[headerInfo11G];
		break;
	default:
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: unknown channel flags 0x%x\n",
			__func__, chan->channel_flags & CHANNEL_ALL);
		result = AH_FALSE;
		goto exit;
	}

	if ((xpdGainMask & pPowerExpn->xpdMask) < 1) {
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: desired xpdGainMask 0x%x not supported by "
			"calibrated xpdMask 0x%x\n", __func__,
			xpdGainMask, pPowerExpn->xpdMask);
		result = AH_FALSE;
		goto exit;
	}

	maxPwr_t4 = (int16_t)(2*(*pPowerMax));	/* pwr_t2 -> pwr_t4 */
	minPwr_t4 = (int16_t)(2*(*pPowerMin));	/* pwr_t2 -> pwr_t4 */

	xgainList[0] = 0xDEAD;
	xgainList[1] = 0xDEAD;

	kk = 0;
	xpdMask = pPowerExpn->xpdMask;
	for (jj = 0; jj < NUM_XPD_PER_CHANNEL; jj++) {
		if (((xpdMask >> jj) & 1) > 0) {
			if (kk > 1) {
				HDPRINTF(ah, HAL_DBG_CHANNEL,
						 "A maximum of 2 xpdGains supported in pExpnPower data\n");
				result = AH_FALSE;
				goto exit;
			}
			xgainList[kk++] = (u_int16_t)jj;
		}
	}

	ar5212GetLowerUpperIndex(chan->channel, &pPowerExpn->p_channels[0],
		pPowerExpn->num_channels, &chan_idx_L, &chan_idx_R);

	kk = 0;
	for (ii = chan_idx_L; ii <= chan_idx_R; ii++) {
		pRawCh = &(pPowerExpn->pDataPerChannel[ii]);
		if (xgainList[1] == 0xDEAD) {
			jj = xgainList[0];
			numPcd = pRawCh->pDataPerXPD[jj].numPcdacs;
			OS_MEMCPY(&pcdacs[0], &pRawCh->pDataPerXPD[jj].pcdac[0],
				numPcd * sizeof(u_int16_t));
			OS_MEMCPY(&powers[0], &pRawCh->pDataPerXPD[jj].pwr_t4[0],
				numPcd * sizeof(int16_t));
			if (!getFullPwrTable(numPcd, &pcdacs[0], &powers[0],
				pRawCh->maxPower_t4, &pPowTable->tmpPowerTable[0])) {
				result = AH_FALSE;
				goto exit;
			}
			OS_MEMCPY(&pPowTable->powTableLXPD[kk][0], &pPowTable->tmpPowerTable[0],
				64*sizeof(int16_t));
		} else {
			jj = xgainList[0];
			numPcd = pRawCh->pDataPerXPD[jj].numPcdacs;
			OS_MEMCPY(&pcdacs[0], &pRawCh->pDataPerXPD[jj].pcdac[0],
				numPcd*sizeof(u_int16_t));
			OS_MEMCPY(&powers[0],
				&pRawCh->pDataPerXPD[jj].pwr_t4[0],
				numPcd*sizeof(int16_t));
			if (!getFullPwrTable(numPcd, &pcdacs[0], &powers[0],
				pRawCh->maxPower_t4, &pPowTable->tmpPowerTable[0])) {
				result = AH_FALSE;
				goto exit;
			}
			OS_MEMCPY(&pPowTable->powTableLXPD[kk][0], &pPowTable->tmpPowerTable[0],
				64 * sizeof(int16_t));

			jj = xgainList[1];
			numPcd = pRawCh->pDataPerXPD[jj].numPcdacs;
			OS_MEMCPY(&pcdacs[0], &pRawCh->pDataPerXPD[jj].pcdac[0],
				numPcd * sizeof(u_int16_t));
			OS_MEMCPY(&powers[0],
				&pRawCh->pDataPerXPD[jj].pwr_t4[0],
				numPcd * sizeof(int16_t));
			if (!getFullPwrTable(numPcd, &pcdacs[0], &powers[0],
				pRawCh->maxPower_t4, &pPowTable->tmpPowerTable[0])) {
				result = AH_FALSE;
				goto exit;
			}
			OS_MEMCPY(&pPowTable->powTableHXPD[kk][0], &pPowTable->tmpPowerTable[0],
				64 * sizeof(int16_t));
		}
		kk++;
	}

	chan_L = pPowerExpn->p_channels[chan_idx_L];
	chan_R = pPowerExpn->p_channels[chan_idx_R];
	kk = chan_idx_R - chan_idx_L;

	if (xgainList[1] == 0xDEAD) {
		for (jj = 0; jj < 64; jj++) {
			pPowTable->pwr_table0[jj] = interpolate_signed(
				chan->channel, chan_L, chan_R,
				pPowTable->powTableLXPD[0][jj], pPowTable->powTableLXPD[kk][jj]);
		}
		Pmin = getPminAndPcdacTableFromPowerTable(&pPowTable->pwr_table0[0],
				ahp->ah_pcdacTable);
		*pPowerMin = (int16_t) (Pmin / 2);
		*pPowerMid = (int16_t) (pPowTable->pwr_table0[63] / 2);
		*pPowerMax = (int16_t) (pPowTable->pwr_table0[63] / 2);
		rfXpdGain[0] = xgainList[0];
		rfXpdGain[1] = rfXpdGain[0];
	} else {
		for (jj = 0; jj < 64; jj++) {
			pPowTable->pwr_table0[jj] = interpolate_signed(
				chan->channel, chan_L, chan_R,
				pPowTable->powTableLXPD[0][jj], pPowTable->powTableLXPD[kk][jj]);
			pPowTable->pwr_table1[jj] = interpolate_signed(
				chan->channel, chan_L, chan_R,
				pPowTable->powTableHXPD[0][jj], pPowTable->powTableHXPD[kk][jj]);
		}
		if (numXpdGain == 2) {
			Pmin = getPminAndPcdacTableFromTwoPowerTables(
				&pPowTable->pwr_table0[0], &pPowTable->pwr_table1[0],
				ahp->ah_pcdacTable, &Pmid);
			*pPowerMin = (int16_t) (Pmin / 2);
			*pPowerMid = (int16_t) (Pmid / 2);
			*pPowerMax = (int16_t) (pPowTable->pwr_table0[63] / 2);
			rfXpdGain[0] = xgainList[0];
			rfXpdGain[1] = xgainList[1];
		} else if (minPwr_t4 <= pPowTable->pwr_table1[63] &&
			   maxPwr_t4 <= pPowTable->pwr_table1[63]) {
			Pmin = getPminAndPcdacTableFromPowerTable(
				&pPowTable->pwr_table1[0], ahp->ah_pcdacTable);
			rfXpdGain[0] = xgainList[1];
			rfXpdGain[1] = rfXpdGain[0];
			*pPowerMin = (int16_t) (Pmin / 2);
			*pPowerMid = (int16_t) (pPowTable->pwr_table1[63] / 2);
			*pPowerMax = (int16_t) (pPowTable->pwr_table1[63] / 2);
		} else {
			Pmin = getPminAndPcdacTableFromPowerTable(
				&pPowTable->pwr_table0[0], ahp->ah_pcdacTable);
			rfXpdGain[0] = xgainList[0];
			rfXpdGain[1] = rfXpdGain[0];
			*pPowerMin = (int16_t) (Pmin/2);
			*pPowerMid = (int16_t) (pPowTable->pwr_table0[63] / 2);
			*pPowerMax = (int16_t) (pPowTable->pwr_table0[63] / 2);
		}
	}

	/*
	 * Move 5112 rates to match power tables where the max
	 * power table entry corresponds with maxPower.
	 */
	HALASSERT(*pPowerMax <= PCDAC_STOP);
	ahp->ah_txPowerIndexOffset = PCDAC_STOP - *pPowerMax;

exit:
	return result;
}

/*
 * Returns interpolated or the scaled up interpolated value
 */
static int16_t
interpolate_signed(u_int16_t target, u_int16_t srcLeft, u_int16_t srcRight,
	int16_t targetLeft, int16_t targetRight)
{
	int16_t rv;

	if (srcRight != srcLeft) {
		rv = ((target - srcLeft)*targetRight +
		      (srcRight - target)*targetLeft) / (srcRight - srcLeft);
	} else {
		rv = targetLeft;
	}
	return rv;
}

/*
 * Return indices surrounding the value in sorted integer lists.
 *
 * NB: the input list is assumed to be sorted in ascending order
 */
static void
ar5212GetLowerUpperIndex(u_int16_t v, u_int16_t *lp, u_int16_t listSize,
                          u_int32_t *vlo, u_int32_t *vhi)
{
	u_int32_t target = v;
	u_int16_t *ep = lp+listSize;
	u_int16_t *tp;

	/*
	 * Check first and last elements for out-of-bounds conditions.
	 */
	if (target < lp[0]) {
		*vlo = *vhi = 0;
		return;
	}
	if (target >= ep[-1]) {
		*vlo = *vhi = listSize - 1;
		return;
	}

	/* look for value being near or between 2 values in list */
	for (tp = lp; tp < ep; tp++) {
		/*
		 * If value is close to the current value of the list
		 * then target is not between values, it is one of the values
		 */
		if (*tp == target) {
			*vlo = *vhi = tp - lp;
			return;
		}
		/*
		 * Look for value being between current value and next value
		 * if so return these 2 values
		 */
		if (target < tp[1]) {
			*vlo = tp - lp;
			*vhi = *vlo + 1;
			return;
		}
	}
}

static HAL_BOOL
getFullPwrTable(u_int16_t numPcdacs, u_int16_t *pcdacs, int16_t *power, int16_t maxPower, int16_t *retVals)
{
	u_int16_t    ii;
	u_int16_t    idxL = 0;
	u_int16_t    idxR = 1;

	if (numPcdacs < 2) {
		HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: at least 2 pcdac values needed [%d]\n",
			__func__, numPcdacs);
		return AH_FALSE;
	}
	for (ii = 0; ii < 64; ii++) {
		if (ii>pcdacs[idxR] && idxR < numPcdacs-1) {
			idxL++;
			idxR++;
		}
		retVals[ii] = interpolate_signed(ii,
			pcdacs[idxL], pcdacs[idxR], power[idxL], power[idxR]);
		if (retVals[ii] >= maxPower) {
			while (ii < 64)
				retVals[ii++] = maxPower;
		}
	}
	return AH_TRUE;
}

/*
 * Takes a single calibration curve and creates a power table.
 * Adjusts the new power table so the max power is relative
 * to the maximum index in the power table.
 *
 * WARNING: rates must be adjusted for this relative power table
 */
static int16_t
getPminAndPcdacTableFromPowerTable(int16_t *pwrTableT4, u_int16_t retVals[])
{
    int16_t ii, jj, jjMax;
    int16_t pMin, currPower, pMax;

    /* If the spread is > 31.5dB, keep the upper 31.5dB range */
    if ((pwrTableT4[63] - pwrTableT4[0]) > 126) {
        pMin = pwrTableT4[63] - 126;
    } else {
        pMin = pwrTableT4[0];
    }

    pMax = pwrTableT4[63];
    jjMax = 63;

    /* Search for highest pcdac 0.25dB below maxPower */
    while ((pwrTableT4[jjMax] > (pMax - 1) ) && (jjMax >= 0)) {
        jjMax--;
    }

    jj = jjMax;
    currPower = pMax;
    for (ii = 63; ii >= 0; ii--) {
        while ((jj < 64) && (jj > 0) && (pwrTableT4[jj] >= currPower)) {
            jj--;
        }
        if (jj == 0) {
            while (ii >= 0) {
                retVals[ii] = retVals[ii + 1];
                ii--;
            }
            break;
        }
        retVals[ii] = jj;
        currPower -= 2;  // corresponds to a 0.5dB step
    }
    return pMin;
}

/*
 * Combines the XPD curves from two calibration sets into a single
 * power table and adjusts the power table so the max power is relative
 * to the maximum index in the power table
 *
 * WARNING: rates must be adjusted for this relative power table
 */
static int16_t
getPminAndPcdacTableFromTwoPowerTables(int16_t *pwrTableLXpdT4,
	int16_t *pwrTableHXpdT4, u_int16_t retVals[], int16_t *pMid)
{
    int16_t     ii, jj, jjMax;
    int16_t     pMin, pMax, currPower;
    int16_t     *pwrTableT4;
    u_int16_t    msbFlag = 0x40;  // turns on the 7th bit of the pcdac

    /* If the spread is > 31.5dB, keep the upper 31.5dB range */
    if ((pwrTableLXpdT4[63] - pwrTableHXpdT4[0]) > 126) {
        pMin = pwrTableLXpdT4[63] - 126;
    } else {
        pMin = pwrTableHXpdT4[0];
    }

    pMax = pwrTableLXpdT4[63];
    jjMax = 63;
    /* Search for highest pcdac 0.25dB below maxPower */
    while ((pwrTableLXpdT4[jjMax] > (pMax - 1) ) && (jjMax >= 0)){
        jjMax--;
    }

    *pMid = pwrTableHXpdT4[63];
    jj = jjMax;
    ii = 63;
    currPower = pMax;
    pwrTableT4 = &(pwrTableLXpdT4[0]);
    while (ii >= 0) {
        if ((currPower <= *pMid) || ( (jj == 0) && (msbFlag == 0x40))){
            msbFlag = 0x00;
            pwrTableT4 = &(pwrTableHXpdT4[0]);
            jj = 63;
        }
        while ((jj > 0) && (pwrTableT4[jj] >= currPower)) {
            jj--;
        }
        if ((jj == 0) && (msbFlag == 0x00)) {
            while (ii >= 0) {
                retVals[ii] = retVals[ii+1];
                ii--;
            }
            break;
        }
        retVals[ii] = jj | msbFlag;
        currPower -= 2;  // corresponds to a 0.5dB step
        ii--;
    }
    return pMin;
}

/*
 * Free memory for analog bank scratch buffers
 */
static void
ar5112Detach(struct ath_hal *ah)
{
	struct ath_hal_5212 *ahp = AH5212(ah);

	if (ahp->ah_pcdacTable != AH_NULL) {
		ath_hal_free(ah, ahp->ah_pcdacTable);
		ahp->ah_pcdacTable = AH_NULL;
	}
	if (ahp->ah_analogBanks != AH_NULL) {
		ath_hal_free(ah, ahp->ah_analogBanks);
		ahp->ah_analogBanks = AH_NULL;
	}
	if (ahp->ah_vpdTable != AH_NULL) {
		ath_hal_free(ah, ahp->ah_vpdTable);
		ahp->ah_vpdTable = AH_NULL;
	}
}

static int16_t
ar5112GetMinPower(struct ath_hal *ah, EXPN_DATA_PER_CHANNEL_5112 *data)
{
	int i, minIndex;
	int16_t minGain,minPwr,minPcdac,retVal;

	/* Assume NUM_POINTS_XPD0 > 0 */
	minGain = data->pDataPerXPD[0].xpd_gain;
	for (minIndex=0,i=1; i<NUM_XPD_PER_CHANNEL; i++) {
		if (data->pDataPerXPD[i].xpd_gain < minGain) {
			minIndex = i;
			minGain = data->pDataPerXPD[i].xpd_gain;
		}
	}
	minPwr = data->pDataPerXPD[minIndex].pwr_t4[0];
	minPcdac = data->pDataPerXPD[minIndex].pcdac[0];
	for (i=1; i<NUM_POINTS_XPD0; i++) {
		if (data->pDataPerXPD[minIndex].pwr_t4[i] < minPwr) {
			minPwr = data->pDataPerXPD[minIndex].pwr_t4[i];
			minPcdac = data->pDataPerXPD[minIndex].pcdac[i];
		}
	}
	retVal = minPwr - (minPcdac*2);
	return(retVal);
}
	
static HAL_BOOL
ar5112GetChannelMaxMinPower(struct ath_hal *ah, HAL_CHANNEL *chan, int16_t *maxPow,
			    int16_t *minPow)
{
	struct ath_hal_5212 *ahp = (struct ath_hal_5212 *) ah;
	int numChannels=0,i,last;
	int totalD, totalF,totalMin;
	EXPN_DATA_PER_CHANNEL_5112 *data=AH_NULL;
	EEPROM_POWER_EXPN_5112 *powerArray=AH_NULL;

	*maxPow = 0;
	if (IS_CHAN_A(chan)) {
		powerArray = ahp->ah_modePowerArray5112;
		data = powerArray[headerInfo11A].pDataPerChannel;
		numChannels = powerArray[headerInfo11A].num_channels;
	} else if (IS_CHAN_G(chan) || IS_CHAN_108G(chan)) {
		/* XXX - is this correct? Should we also use the same power for turbo G? */
		powerArray = ahp->ah_modePowerArray5112;
		data = powerArray[headerInfo11G].pDataPerChannel;
		numChannels = powerArray[headerInfo11G].num_channels;
	} else if (IS_CHAN_B(chan)) {
		powerArray = ahp->ah_modePowerArray5112;
		data = powerArray[headerInfo11B].pDataPerChannel;
		numChannels = powerArray[headerInfo11B].num_channels;
	} else {
		return (AH_TRUE);
	}
	/* Make sure the channel is in the range of the TP values 
	 *  (freq piers)
	 */
	if (numChannels < 1)
		return(AH_FALSE);

	if ((chan->channel < data[0].channelValue) ||
	    (chan->channel > data[numChannels-1].channelValue)) {
		if (chan->channel < data[0].channelValue) {
			*maxPow = data[0].maxPower_t4;
			*minPow = ar5112GetMinPower(ah, &data[0]);
			return(AH_TRUE);
		} else {
			*maxPow = data[numChannels - 1].maxPower_t4;
			*minPow = ar5112GetMinPower(ah, &data[numChannels - 1]);
			return(AH_TRUE);
		}
	}

	/* Linearly interpolate the power value now */
	for (last=0,i=0;
	     (i<numChannels) && (chan->channel > data[i].channelValue);
	     last=i++);
	totalD = data[i].channelValue - data[last].channelValue;
	if (totalD > 0) {
		totalF = data[i].maxPower_t4 - data[last].maxPower_t4;
		*maxPow = (int8_t) ((totalF*(chan->channel-data[last].channelValue) + data[last].maxPower_t4*totalD)/totalD);

		totalMin = ar5112GetMinPower(ah,&data[i]) - ar5112GetMinPower(ah, &data[last]);
		*minPow = (int8_t) ((totalMin*(chan->channel-data[last].channelValue) + ar5112GetMinPower(ah, &data[last])*totalD)/totalD);
		return (AH_TRUE);
	} else {
		if (chan->channel == data[i].channelValue) {
			*maxPow = data[i].maxPower_t4;
			*minPow = ar5112GetMinPower(ah, &data[i]);
			return(AH_TRUE);
		} else
			return(AH_FALSE);
	}
}
	
static HAL_BOOL
ar5112GetChipPowerLimits(struct ath_hal *ah, HAL_CHANNEL *chans, u_int32_t nchans)
{
	HAL_BOOL retVal = AH_TRUE;
	int i;
	int16_t maxPow = 0,minPow = 0;
	
	for (i=0; i<nchans; i++) {
		if (ar5112GetChannelMaxMinPower(ah, &chans[i], &maxPow, &minPow)) {
			/* XXX -Need to adjust pcdac values to indicate dBm */
			chans[i].max_tx_power = maxPow;
			chans[i].min_tx_power = minPow;
		} else {
			HDPRINTF(ah, HAL_DBG_CHANNEL, "Failed setting power table for nchans=%d\n",i);
			retVal= AH_FALSE;
		}
	}
#ifdef AH_DEBUG
	for (i=0; i<nchans; i++) {
		HDPRINTF(ah, HAL_DBG_CHANNEL, "Chan %d: MaxPow = %d MinPow = %d\n",
			 chans[i].channel,chans[i].max_tx_power, chans[i].min_tx_power);
	}
#endif
	return (retVal);
}

/*
 * Allocate memory for analog bank scratch buffers
 * Scratch Buffer will be reinitialized every reset so no need to zero now
 */
HAL_BOOL
ar5112RfAttach(struct ath_hal *ah, HAL_STATUS *status)
{
	struct ath_hal_5212 *ahp = AH5212(ah);

	HALASSERT(ahp->ah_analogBanks == AH_NULL);
	ahp->ah_analogBanks = ath_hal_malloc(ah, sizeof(AR5212_RF_BANKS_5112));
	if (ahp->ah_analogBanks == AH_NULL) {
		HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: cannot allocate RF banks\n", __func__);
		*status = HAL_ENOMEM;		/* XXX */
		return AH_FALSE;
	}
	HALASSERT(ahp->ah_pcdacTable == AH_NULL);
	ahp->ah_pcdacTableSize = PWR_TABLE_SIZE * sizeof(u_int16_t);
	ahp->ah_pcdacTable = ath_hal_malloc(ah, ahp->ah_pcdacTableSize);
	if (ahp->ah_pcdacTable == AH_NULL) {
		HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: cannot allocate PCDAC table\n", __func__);
		*status = HAL_ENOMEM;		/* XXX */
		return AH_FALSE;
	}
	HALASSERT(ahp->ah_vpdTable == AH_NULL);
	ahp->ah_vpdTable = ath_hal_malloc(ah, sizeof(AR5112_POWER_TABLE));
	if (ahp->ah_vpdTable == AH_NULL) {
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: cannot allocate VPD table\n", __func__);
		*status = HAL_ENOMEM;		/* XXX */
		return AH_FALSE;
	}

	ahp->ah_pcdacTableSize = PWR_TABLE_SIZE;
	ahp->ah_rfHal.rfDetach		= ar5112Detach;
	ahp->ah_rfHal.writeRegs		= ar5112WriteRegs;
	ahp->ah_rfHal.getRfBank		= ar5112GetRfBank;
	ahp->ah_rfHal.setChannel	= ar5112SetChannel;
	ahp->ah_rfHal.setRfRegs		= ar5112SetRfRegs;
	ahp->ah_rfHal.setPowerTable	= ar5112SetPowerTable;
	ahp->ah_rfHal.getChipPowerLim	= ar5112GetChipPowerLimits;
	ahp->ah_rfHal.getNfAdjust	= ar5212GetNfAdjust;

	return AH_TRUE;
}

#endif /* AH_SUPPORT_5112 */
