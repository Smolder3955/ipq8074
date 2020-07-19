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

#ifdef AH_SUPPORT_AR5212

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"

#include "ar5212/ar5212.h"
#include "ar5212/ar5212reg.h"
#include "ar5212/ar5212phy.h"
#ifdef AH_SUPPORT_AR5311
#include "ar5212/ar5311reg.h"
#endif

/* Add static register initialization vectors */
#define AH_5212_COMMON
#include "ar5212/ar5212.ini"

/* Additional Time delay to wait after activiting the Base band */
#define BASE_ACTIVATE_DELAY	100	/* 100 usec */
#define PLL_SETTLE_DELAY	300	/* 300 usec */

static HAL_BOOL		ar5212SetResetReg(struct ath_hal *, u_int32_t resetMask);
/* NB: public for 5312 use */
HAL_BOOL		ar5212IsSpurChannel(struct ath_hal *, HAL_CHANNEL *);
HAL_BOOL		ar5212ChannelChange(struct ath_hal *, HAL_CHANNEL *);
static void     ar5212LoadNF(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan);
int16_t			ar5212GetNf(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
static void     ar5212UpdateNFHistBuff(HAL_NFCAL_HIST_FULL *h, int16_t nf);
static int16_t  ar5212GetNfHistMid(HAL_NFCAL_HIST_FULL *h, int reading);
HAL_BOOL		ar5212SetBoardValues(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
void			ar5212SetDeltaSlope(struct ath_hal *, HAL_CHANNEL *);
HAL_BOOL		ar5212SetTransmitPower(struct ath_hal *ah,
				HAL_CHANNEL_INTERNAL *chan, u_int16_t *rfXpdGain,
				u_int16_t powerLimit, HAL_BOOL calledDuringReset, u_int16_t tpcInDb);
static HAL_BOOL		ar5212SetRateTable(struct ath_hal *, 
				HAL_CHANNEL *, int16_t tpcScaleReduction, int16_t powerLimit,
				int16_t *minPower, int16_t *maxPower);
static void		ar5212CorrectGainDelta(struct ath_hal *, int twiceOfdmCckDelta);
static void		ar5212GetTargetPowers(struct ath_hal *, HAL_CHANNEL *,
				TRGT_POWER_INFO *pPowerInfo, u_int16_t numChannels,
				TRGT_POWER_INFO *pNewPower);
static u_int16_t	ar5212GetMaxEdgePower(u_int16_t channel,
				RD_EDGES_POWER  *pRdEdgesPower);
static void		ar5212RequestRfgain(struct ath_hal *);
static HAL_BOOL		ar5212InvalidGainReadback(struct ath_hal *, GAIN_VALUES *);
static HAL_BOOL		ar5212IsGainAdjustNeeded(struct ath_hal *, const GAIN_VALUES *);
static int32_t		ar5212AdjustGain(struct ath_hal *, GAIN_VALUES *);
void			ar5212SetRateDurationTable(struct ath_hal *, HAL_CHANNEL *);
static u_int32_t	ar5212GetRfField(u_int32_t *rfBuf, u_int32_t numBits,
				u_int32_t firstBit, u_int32_t column);
static void		ar5212GetGainFCorrection(struct ath_hal *ah);
void			ar5212SetCompRegs(struct ath_hal *ah);
void			ar5212SetIFSTiming(struct ath_hal *, HAL_CHANNEL *);
void			ar5212SetSpurMitigation(struct ath_hal *, HAL_CHANNEL_INTERNAL *);

/* NB: public for RF backend use */
void			ar5212GetLowerUpperValues(u_int16_t value,
				u_int16_t *pList, u_int16_t listSize,
				u_int16_t *pLowerValue, u_int16_t *pUpperValue);
void			ar5212ModifyRfBuffer(u_int32_t *rfBuf, u_int32_t reg32,
				u_int32_t numBits, u_int32_t firstBit, u_int32_t column);

/*
 * WAR for bug 6773.  OS_DELAY() does a PIO READ on the PCI bus which allows
 * other cards' DMA reads to complete in the middle of our reset.
 */
#define WAR_6773(x) do {		\
	if ((++(x) % 64) == 0)		\
		OS_DELAY(1);		\
} while (0)

#define IS_NO_RESET_TIMER_ADDR(x)                      \
    ( (((x) >= AR_BEACON) && ((x) <= AR_CFP_DUR)) || \
      (((x) >= AR_SLEEP1) && ((x) <= AR_SLEEP3)))

#define REG_WRITE_ARRAY(regArray, column, regWr) do {                  	\
	int r;								\
	for (r = 0; r < N(regArray); r++) {				\
		OS_REG_WRITE(ah, (regArray)[r][0], (regArray)[r][(column)]);\
		WAR_6773(regWr);					\
	}								\
} while (0)

#define IS_DISABLE_FAST_ADC_CHAN(x) (((x) == 2462) || ((x) == 2467))

/* 
 * XXX NDIS 5.x code had MAX_RESET_WAIT set to 2000 for AP code 
 * and 10 for Client code 
 */
#define MAX_RESET_WAIT                         10

#define TX_QUEUEPEND_CHECK                     1
#define TX_ENABLE_CHECK                        2
#define RX_ENABLE_CHECK                        4

/*
 * Places the device in and out of reset and then places sane
 * values in the registers based on EEPROM config, initialization
 * vectors (as determined by the mode), and station configuration
 *
 * bChannelChange is used to preserve DMA/PCU registers across
 * a HW Reset during channel change.
 */
HAL_BOOL ar5212Reset(struct ath_hal *ah, HAL_OPMODE opmode, HAL_CHANNEL *halChan,
                     HAL_HT_MACMODE macmode, u_int8_t txchainmask,
                     u_int8_t rxchainmask, HAL_HT_EXTPROTSPACING extprotspacing,
                     HAL_BOOL bChannelChange, HAL_STATUS *status, int is_scan)
{
#define	N(a)	(sizeof (a) / sizeof (a[0]))
#define	FAIL(_code)	do { ecode = _code; goto bad; } while (0)
    u_int32_t              softLedCfg, softLedState;
    struct ath_hal_5212    *ahp = AH5212(ah);
    HAL_CHANNEL_INTERNAL   *tempichan = AH_NULL;
    HAL_CHANNEL            localchan    = *halChan, *chan = &localchan;
    HAL_CHANNEL_INTERNAL   localichan, *ichan = &localichan;
    u_int32_t              saveFrameSeqCount, saveDefAntenna, saveLedState;
    u_int32_t              macStaId1, synthDelay, txFrm2TxDStart;
    u_int16_t              rfXpdGain[2];
    int16_t                powerLimit = MAX_RATE_POWER;
    int16_t                cckOfdmPwrDelta = 0;
    u_int                  modesIndex, freqIndex;
    HAL_STATUS             ecode;
    int                    i, regWrites = 0;
    u_int32_t              testReg, powerVal,tpcPow, rssiThrReg;
    int8_t                 twiceAntennaGain, twiceAntennaReduction;
    HAL_BOOL               isBmode = AH_FALSE;
    HAL_BOOL               ichan_isBmode = AH_FALSE;


	OS_MARK(ah, AH_MARK_RESET, bChannelChange);
#define	IS(_c,_f)	(((_c)->channel_flags & _f) || 0)
	if ((IS(chan, CHANNEL_2GHZ) ^ IS(chan, CHANNEL_5GHZ)) == 0) {
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel %u/0x%x; not marked as "
			 "2GHz or 5GHz\n", __func__,
			chan->channel, chan->channel_flags);
		FAIL(HAL_EINVAL);
	}
	if ((IS(chan, CHANNEL_OFDM) ^ IS(chan, CHANNEL_CCK)) == 0) {
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel %u/0x%x; not marked as "
			"OFDM or CCK\n", __func__,
			chan->channel, chan->channel_flags);
		FAIL(HAL_EINVAL);
	}
#undef IS


	/* Bring out of sleep mode */
	if (!ar5212SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE)) {
		return AH_FALSE;
	}

	/*
	 * Map public channel to private.
	 */
	tempichan = ath_hal_checkchannel(ah, chan);
	if (tempichan == AH_NULL) {
                HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel %u/0x%x; no mapping\n",
                            __func__, chan->channel, chan->channel_flags);
                FAIL(HAL_EINVAL);
	}
    
    localichan = *tempichan;
	
    switch (opmode) {
	case HAL_M_STA:
	case HAL_M_IBSS:
	case HAL_M_HOSTAP:
	case HAL_M_MONITOR:
		break;
	default:
		HDPRINTF(ah, HAL_DBG_RESET, "%s: invalid operating mode %u\n",
			__func__, opmode);
		FAIL(HAL_EINVAL);
		break;
	}
	HALASSERT(ahp->ah_eeversion >= AR_EEPROM_VER3);

	CHECK_CCK(ah, ichan, ichan_isBmode);
	CHECK_CCK(ah, chan, isBmode);

    /* Need to save the sequence number to restore it after
     * the reset!
     */
    saveFrameSeqCount = OS_REG_READ(ah, AR_D_SEQNUM);


	/* Preserve certain DMA hardware registers on a channel change */
	if (bChannelChange) {
		/*
		 * AR5212 WAR
		 *
		 * On Venice, the TSF is almost preserved across a reset;
		 * it requires the WAR of doubling writes to the RESET_TSF
		 * bit in the AR_BEACON register; it also has the quirk
		 * of the TSF going back in time on the station (station
		 * latches onto the last beacon's tsf during a reset 50%
		 * of the times); the latter is not a problem for adhoc
		 * stations since as long as the TSF is behind, it will
		 * get resynchronized on receiving the next beacon; the
		 * TSF going backwards in time could be a problem for the
		 * sleep operation (supported on infrastructure stations
		 * only) - the best and most general fix for this situation
		 * is to resynchronize the various sleep/beacon timers on
		 * the receipt of the next beacon i.e. when the TSF itself
		 * gets resynchronized to the AP's TSF - power save is
		 * needed to be temporarily disabled until that time
		 */
		ar5212GetNf(ah, ichan);

		if (DO_ANI(ah)) {
	    	ar5212AniReset(ah, 0);
		}

	}

	/* If the channel change is across the same mode - perform a fast channel change */

	/*
	 * Temp. work around for Griffin PCI card (mac_ver = 7, mac_rev = 9).
	 * Cannot program RxDP after fast channel change, even rx dma seems
	 * stopped.
	 */
	if ((IS_2413(ah) || IS_5413(ah)) &&
		(AH_PRIVATE(ah)->ah_mac_version != AR_SREV_2413)) {
		/*
		 * Channel change can only be used when:
		 *  -channel change requested - so it's not the initial reset.
		 *  -it's not a change to the current channel - often called
		 * when switching modes on a channel
		 *  -the modes of the previous and requested channel are the
		 * same - some ugly code for XR
		 */
		if (bChannelChange &&
		    (ahp->ah_chipFullSleep != AH_TRUE) &&
		    (AH_PRIVATE(ah)->ah_curchan != AH_NULL) &&
		    (chan->channel != AH_PRIVATE(ah)->ah_curchan->channel) &&
		    ((chan->channel_flags & CHANNEL_ALL) ==
		     (AH_PRIVATE(ah)->ah_curchan->channel_flags & CHANNEL_ALL))) {
			if (ar5212ChannelChange(ah, chan)) {
				if (!(ichan->priv_flags & CHANNEL_DFS)) 
                                	ichan->priv_flags &= ~CHANNEL_INTERFERENCE;
                                chan->channel_flags = ichan->channel_flags;
                                chan->priv_flags = ichan->priv_flags;

				/* If ChannelChange completed - skip the rest of reset */
				return AH_TRUE;
			}
		}
	}

   	/*
 	 * Preserve the bmiss rssi threshold and count threshold
 	 * across resets
 	 */
 	rssiThrReg = OS_REG_READ(ah, AR_RSSI_THR);
 	/* If reg is zero, first time thru set to default val */
 	if (rssiThrReg == 0) {
 		rssiThrReg = INIT_RSSI_THR;
 	}


	/*
	 * Preserve the antenna on a channel change
	 */
	saveDefAntenna = OS_REG_READ(ah, AR_DEF_ANTENNA);
	if (saveDefAntenna == 0)		/* XXX magic constants */
		saveDefAntenna = 1;

	/* Save hardware flag before chip reset clears the register */
 	macStaId1 = OS_REG_READ(ah, AR_STA_ID1) & 
 		(AR_STA_ID1_BASE_RATE_11B | AR_STA_ID1_USE_DEFANT);


	/* Save led state from pci config register */
	saveLedState = OS_REG_READ(ah, AR_PCICFG) &
		(AR_PCICFG_LEDCTL | AR_PCICFG_LEDMODE | AR_PCICFG_LEDBLINK | AR_PCICFG_LEDSLOW);
	softLedCfg = OS_REG_READ(ah, AR_GPIOCR);
	softLedState = OS_REG_READ(ah, AR_GPIODO);

	ar5212RestoreClock(ah, opmode);		/* move to refclk operation */

	/*
	 * Adjust gain parameters before reset if
	 * there's an outstanding gain updated.
	 */
	(void) ar5212GetRfgain(ah);

	if (!ar5212ChipReset(ah, chan)) {
		HDPRINTF(ah, HAL_DBG_RESET, "%s: chip reset failed\n", __func__);
		FAIL(HAL_EIO);
	}

 	/* Restore bmiss rssi & count thresholds */
 	OS_REG_WRITE(ah, AR_RSSI_THR, rssiThrReg);


	/* Setup the indices for the next set of register array writes */
	switch (chan->channel_flags & CHANNEL_ALL) {
	case CHANNEL_A:
//	case CHANNEL_XR_A:
		modesIndex = 1;
		freqIndex  = 1;
		break;
	case CHANNEL_T:
//	case CHANNEL_XR_T:
		modesIndex = 2;
		freqIndex  = 1;
		break;
	case CHANNEL_B:
		modesIndex = 3;
		freqIndex  = 2;
		break;
	case CHANNEL_PUREG:
//	case CHANNEL_XR_G:
		modesIndex = 4;
		freqIndex  = 2;
		break;
	case CHANNEL_108G:
		modesIndex = 5;
		freqIndex  = 2;
		break;
	default:
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel flags 0x%x\n",
			__func__, chan->channel_flags);
		FAIL(HAL_EINVAL);
	}

	OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);

	/* Set correct Baseband to analog shift setting to access analog chips. */
	OS_REG_WRITE(ah, AR_PHY(0), 0x00000007);

	REG_WRITE_ARRAY(ar5212Modes, modesIndex, regWrites);
	/* Write Common Array Parameters */
	for (i = 0; i < N(ar5212Common); i++) {
		u_int32_t reg = ar5212Common[i][0];
		/* XXX timer/beacon setup registers? */
		/* On channel change, don't reset the PCU registers */
		if (!(bChannelChange && IS_NO_RESET_TIMER_ADDR(reg))) {
			OS_REG_WRITE(ah, reg, ar5212Common[i][1]);
			WAR_6773(regWrites);
		}
	}
	ahp->ah_rfHal.writeRegs(ah, modesIndex, freqIndex, regWrites);

	OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);

	if (IS_CHAN_HALF_RATE(chan) || IS_CHAN_QUARTER_RATE(chan)) {
		ar5212SetIFSTiming(ah, chan);
	}

	/* Overwrite INI values for revised chipsets */
	if (AH_PRIVATE(ah)->ah_phy_rev >= AR_PHY_CHIP_ID_REV_2) {
		/* ADC_CTL */
		OS_REG_WRITE(ah, AR_PHY_ADC_CTL,
			SM(2, AR_PHY_ADC_CTL_OFF_INBUFGAIN) |
			SM(2, AR_PHY_ADC_CTL_ON_INBUFGAIN) |
			AR_PHY_ADC_CTL_OFF_PWDDAC |
			AR_PHY_ADC_CTL_OFF_PWDADC);

		/* TX_PWR_ADJ */
		if (chan->channel == 2484) {
		    cckOfdmPwrDelta = SCALE_OC_DELTA(ahp->ah_cckOfdmPwrDelta - ahp->ah_scaledCh14FilterCckDelta);
		} else {
		    cckOfdmPwrDelta = SCALE_OC_DELTA(ahp->ah_cckOfdmPwrDelta);
		}

		if (IS_CHAN_G(chan)) {
		    OS_REG_WRITE(ah, AR_PHY_TXPWRADJ,
			SM((ahp->ah_cckOfdmPwrDelta*-1), AR_PHY_TXPWRADJ_CCK_GAIN_DELTA) |
			SM((cckOfdmPwrDelta*-1), AR_PHY_TXPWRADJ_CCK_PCDAC_INDEX));
		} else {
		    OS_REG_WRITE(ah, AR_PHY_TXPWRADJ, 0);
		}

		/* Add barker RSSI thresh enable as disabled */
		OS_REG_CLR_BIT(ah, AR_PHY_DAG_CTRLCCK,
			AR_PHY_DAG_CTRLCCK_EN_RSSI_THR);
		OS_REG_RMW_FIELD(ah, AR_PHY_DAG_CTRLCCK,
			AR_PHY_DAG_CTRLCCK_RSSI_THR, 2);

		/* Set the mute mask to the correct default */
		OS_REG_WRITE(ah, AR_SEQ_MASK, 0x0000000F);
	}

	if (AH_PRIVATE(ah)->ah_phy_rev >= AR_PHY_CHIP_ID_REV_3) {
		/* Clear reg to alllow RX_CLEAR line debug */
		OS_REG_WRITE(ah, AR_PHY_BLUETOOTH,  0);
	}

	/* Set ADC/DAC select values */
	OS_REG_WRITE(ah, AR_PHY_SLEEP_SCAL, 0x0e);

	if (IS_5413(ah) || IS_2417(ah)) {
		u_int32_t newReg=1;
		if (IS_DISABLE_FAST_ADC_CHAN(chan->channel))
			newReg = 0;
		/* As it's a clock changing register, only write when the value needs to be changed */
		if (OS_REG_READ(ah, AR_PHY_FAST_ADC) != newReg)
			OS_REG_WRITE(ah, AR_PHY_FAST_ADC, newReg);
	}

	/* Setup the transmit power values. */
	if (!ar5212SetTransmitPower(ah, ichan, rfXpdGain, powerLimit, AH_TRUE, 0)) {
		HDPRINTF(ah, HAL_DBG_RESET, "%s: error init'ing transmit power\n", __func__);
		FAIL(HAL_EIO);
	}

	/* Write the analog registers */
	if (!ahp->ah_rfHal.setRfRegs(ah, ichan, modesIndex, rfXpdGain)) {
		HDPRINTF(ah, HAL_DBG_RESET, "%s: ar5212SetRfRegs failed\n", __func__);
		FAIL(HAL_EIO);
	}

	/* Write delta slope for OFDM enabled modes (A, G, Turbo) */
	if (IS_CHAN_OFDM(chan)) {
		if ((IS_5413(ah) || (ahp->ah_eeprom.ee_version >= AR_EEPROM_VER5_3)) &&
		    (!IS_CHAN_B(chan)))
			ar5212SetSpurMitigation(ah, ichan);
		ar5212SetDeltaSlope(ah, chan);
	}

	/* Setup board specific options for EEPROM version 3 */
	if (!ar5212SetBoardValues(ah, ichan)) {
		HDPRINTF(ah, HAL_DBG_RESET, "%s: error setting board options\n", __func__);
		FAIL(HAL_EIO);
	}

	/* Restore certain DMA hardware registers on a channel change */
    OS_REG_WRITE(ah, AR_D_SEQNUM, saveFrameSeqCount);

	OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);

	OS_REG_WRITE(ah, AR_STA_ID0, LE_READ_4(ahp->ah_macaddr));
	OS_REG_WRITE(ah, AR_STA_ID1, LE_READ_2(ahp->ah_macaddr + 4)
		| macStaId1
		| AR_STA_ID1_RTS_USE_DEF
		| ahp->ah_staId1Defaults
	);
	ar5212SetOperatingMode(ah, opmode);

	/* Set Venice BSSID mask according to current state */
	OS_REG_WRITE(ah, AR_BSSMSKL, LE_READ_4(ahp->ah_bssidmask));
	OS_REG_WRITE(ah, AR_BSSMSKU, LE_READ_2(ahp->ah_bssidmask + 4));

	/* Restore previous led state */
	OS_REG_WRITE(ah, AR_PCICFG, OS_REG_READ(ah, AR_PCICFG) | saveLedState);

	/* Restore soft Led state to GPIO */
	OS_REG_WRITE(ah, AR_GPIOCR, softLedCfg);
	OS_REG_WRITE(ah, AR_GPIODO, softLedState);

	/* Restore previous antenna */
	OS_REG_WRITE(ah, AR_DEF_ANTENNA, saveDefAntenna);

	/* then our BSSID and assocID */
	OS_REG_WRITE(ah, AR_BSS_ID0, LE_READ_4(ahp->ah_bssid));
	OS_REG_WRITE(ah, AR_BSS_ID1, LE_READ_2(ahp->ah_bssid + 4) |
                         ((ahp->ah_assocId & 0x3fff) << AR_BSS_ID1_AID_S));

	OS_REG_WRITE(ah, AR_ISR, ~0);		/* cleared on write */

    *tempichan = *ichan;
	if (!ahp->ah_rfHal.setChannel(ah, tempichan))
		FAIL(HAL_EIO);

	OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);

	ar5212SetCoverageClass(ah, AH_PRIVATE(ah)->ah_coverageClass, 1);

	ar5212SetRateDurationTable(ah, chan);

	/* Set Tx frame start to tx data start delay */
	if (IS_5112(ah) && (IS_CHAN_HALF_RATE(AH_PRIVATE(ah)->ah_curchan) ||
			IS_CHAN_QUARTER_RATE(AH_PRIVATE(ah)->ah_curchan))) {
		txFrm2TxDStart = 
			(IS_CHAN_HALF_RATE(AH_PRIVATE(ah)->ah_curchan)) ?
					TX_FRAME_D_START_HALF_RATE:
					TX_FRAME_D_START_QUARTER_RATE;
		OS_REG_RMW_FIELD(ah, AR_PHY_TX_CTL, 
			AR_PHY_TX_FRAME_TO_TX_DATA_START, txFrm2TxDStart);
	}

	/*
	 * Setup fast diversity.
	 * Fast diversity can be enabled or disabled via regadd.txt.
	 * Default is enabled.
	 * For reference,
	 *    Disable: reg        val
	 *             0x00009860 0x00009d18 (if 11a / 11g, else no change)
	 *             0x00009970 0x192bb514
	 *             0x0000a208 0xd03e4648
	 *
	 *    Enable:  0x00009860 0x00009d10 (if 11a / 11g, else no change)
	 *             0x00009970 0x192fb514
	 *             0x0000a208 0xd03e6788
	 */

	/* XXX Setup pre PHY ENABLE EAR additions */
	/*
	 * Wait for the frequency synth to settle (synth goes on
	 * via AR_PHY_ACTIVE_EN).  Read the phy active delay register.
	 * Value is in 100ns increments.
	 */
	synthDelay = OS_REG_READ(ah, AR_PHY_RX_DELAY) & AR_PHY_RX_DELAY_DELAY;
	if (IS_CHAN_CCK(chan)) {
		synthDelay = (4 * synthDelay) / 22;
	} else {
		synthDelay /= 10;
	}

	/* Activate the PHY (includes baseband activate and synthesizer on) */
	OS_REG_WRITE(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);

	/* 
	 * There is an issue if the AP starts the calibration before
	 * the base band timeout completes.  This could result in the
	 * rx_clear false triggering.  As a workaround we add delay an
	 * extra BASE_ACTIVATE_DELAY usecs to ensure this condition
	 * does not happen.
	 */
	if (IS_CHAN_HALF_RATE(AH_PRIVATE(ah)->ah_curchan)) {
		OS_DELAY((synthDelay << 1) + BASE_ACTIVATE_DELAY);
	} else if (IS_CHAN_QUARTER_RATE(AH_PRIVATE(ah)->ah_curchan)) {
		OS_DELAY((synthDelay << 2) + BASE_ACTIVATE_DELAY);
	} else {
		OS_DELAY(synthDelay + BASE_ACTIVATE_DELAY);
	}

 	/*
 	 * WAR for bug 9031
 	 * The udelay method is not reliable with notebooks.
 	 * Need to check to see if the baseband is ready
 	 */
 	testReg = OS_REG_READ(ah, AR_PHY_TESTCTRL);
 	/* Selects the Tx hold */
 	OS_REG_WRITE(ah, AR_PHY_TESTCTRL, AR_PHY_TESTCTRL_TXHOLD);
 	i = 0;
 	while ((i++ < 20) &&
 	       (OS_REG_READ(ah, 0x9c24) & 0x10)) /* test if baseband not ready */		OS_DELAY(200);
 	OS_REG_WRITE(ah, AR_PHY_TESTCTRL, testReg);
 


	/* Calibrate the AGC and start a NF calculation */
	OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL,
		  OS_REG_READ(ah, AR_PHY_AGC_CONTROL)
		| AR_PHY_AGC_CONTROL_CAL
		| AR_PHY_AGC_CONTROL_NF);

	if (!IS_CHAN_B(chan) && ahp->ah_bIQCalibration != IQ_CAL_DONE) {
		/* Start IQ calibration w/ 2^(INIT_IQCAL_LOG_COUNT_MAX+1) samples */
		OS_REG_RMW_FIELD(ah, AR_PHY_TIMING_CTRL4, 
			AR_PHY_TIMING_CTRL4_IQCAL_LOG_COUNT_MAX,
			INIT_IQCAL_LOG_COUNT_MAX);
		OS_REG_SET_BIT(ah, AR_PHY_TIMING_CTRL4,
			AR_PHY_TIMING_CTRL4_DO_IQCAL);
		ahp->ah_bIQCalibration = IQ_CAL_RUNNING;
	} else
		ahp->ah_bIQCalibration = IQ_CAL_INACTIVE;

	/* Setup compression registers */
	ar5212SetCompRegs(ah);

	/* Set 1:1 QCU to DCU mapping for all queues */
	for (i = 0; i < AR_NUM_DCU; i++)
		OS_REG_WRITE(ah, AR_DQCUMASK(i), 1 << i);

	ahp->ah_intrTxqs = 0;
	for (i = 0; i < AH_PRIVATE(ah)->ah_caps.hal_total_queues; i++)
		ar5212ResetTxQueue(ah, i);

	/*
	 * Setup interrupt handling.  Note that ar5212ResetTxQueue
	 * manipulates the secondary IMR's as queues are enabled
	 * and disabled.  This is done with RMW ops to insure the
	 * settings we make here are preserved.
	 */
	ahp->ah_maskReg = AR_IMR_TXOK | AR_IMR_TXERR | AR_IMR_TXURN
			| AR_IMR_RXOK | AR_IMR_RXERR | AR_IMR_RXORN
			| AR_IMR_HIUERR
			;
	if (opmode == HAL_M_HOSTAP)
		ahp->ah_maskReg |= AR_IMR_MIB;
	OS_REG_WRITE(ah, AR_IMR, ahp->ah_maskReg);
	/* Enable bus errors that are OR'd to set the HIUERR bit */
	OS_REG_WRITE(ah, AR_IMR_S2,
		OS_REG_READ(ah, AR_IMR_S2)
		| AR_IMR_S2_MCABT | AR_IMR_S2_SSERR | AR_IMR_S2_DPERR);

	if (AH_PRIVATE(ah)->ah_rfkillEnabled)
		ar5212EnableRfKill(ah);

	if (!ath_hal_wait(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_CAL, 0, AH_WAIT_TIMEOUT)) {
		HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: offset calibration failed to complete in 1ms;"
			" noisy environment?\n", __func__);
	}

	/*
	 * Set clocks back to 32kHz if they had been using refClk, then
	 * use an external 32kHz crystal when sleeping, if one exists.
	 */
	ar5212SetupClock(ah, opmode);

	/*
	 * Writing to AR_BEACON will start timers. Hence it should
	 * be the last register to be written. Do not reset tsf, do
	 * not enable beacons at this point, but preserve other values
	 * like beaconInterval.
	 */
	OS_REG_WRITE(ah, AR_BEACON,
		(OS_REG_READ(ah, AR_BEACON) &~ (AR_BEACON_EN | AR_BEACON_RESET_TSF)));

	/* QoS support */
	if (AH_PRIVATE(ah)->ah_mac_version > AR_SREV_VERSION_VENICE ||
	    (AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_VENICE &&
	     AH_PRIVATE(ah)->ah_mac_rev >= AR_SREV_GRIFFIN_LITE)) {
		OS_REG_WRITE(ah, AR_QOS_CONTROL, 0x100aa);	/* XXX magic */
		OS_REG_WRITE(ah, AR_QOS_SELECT, 0x3210);	/* XXX magic */
	}

	/* Turn on NOACK Support for QoS packets */
	OS_REG_WRITE(ah, AR_NOACK,
		SM(2, AR_NOACK_2BIT_VALUE) |
		SM(5, AR_NOACK_BIT_OFFSET) |
		SM(0, AR_NOACK_BYTE_OFFSET));

	/* TPC for self-generated frames */
 
 	tpcPow = MS(ahp->ah_macTPC, AR_TPC_ACK);
 	if (tpcPow > ichan->max_tx_power)
 		tpcPow = ichan->max_tx_power;
 
 	/* Get Antenna Gain reduction */
 	if (IS_CHAN_5GHZ(chan)) {
 		twiceAntennaGain = ahp->ah_antennaGainMax[0];
 	} else {
 		twiceAntennaGain = ahp->ah_antennaGainMax[1];
 	}
 	twiceAntennaReduction =
 		ath_hal_getantennareduction(ah, chan, twiceAntennaGain);
 
 	if (tpcPow > (2*ichan->max_reg_tx_power - twiceAntennaReduction))
 		tpcPow = 2*ichan->max_reg_tx_power - twiceAntennaReduction;
 
 	powerVal = SM(tpcPow, AR_TPC_ACK) |
 		SM(tpcPow, AR_TPC_CTS) |
 		SM(tpcPow, AR_TPC_CHIRP);
 
 	OS_REG_WRITE(ah, AR_TPC, powerVal);


	/* Restore user-specified settings */
	if (ahp->ah_miscMode != 0)
		OS_REG_WRITE(ah, AR_MISC_MODE, ahp->ah_miscMode);
	if (ahp->ah_slottime != (u_int) -1)
		ar5212SetSlotTime(ah, ahp->ah_slottime);
	if (ahp->ah_acktimeout != (u_int) -1)
		ar5212SetAckTimeout(ah, ahp->ah_acktimeout);
    {
        u_int32_t   reg_value;

        if (AH_PRIVATE(ah)->ah_diagreg != 0)
            reg_value = AH_PRIVATE(ah)->ah_diagreg;
        else
            reg_value = OS_REG_READ(ah, AR_DIAG_SW);

        /*
         * Disable the RxPCU explicitly since it is enabled by default.
         * Otherwise, the Rx engine will start when we queue the first
         * Rx buffer and enable the Rx without calling start RX PCU.
         */
        reg_value = reg_value | AR_DIAG_RX_DIS;

        OS_REG_WRITE(ah, AR_DIAG_SW, reg_value);
    }

	AH_PRIVATE(ah)->ah_opmode = opmode;	/* record operating mode */

	/* 
	 * Value of TX trigger level not maintained acrosss resets, so we must
	 * read it again from HW.
	 */
	AH_PRIVATE(ah)->ah_tx_trig_level = MS(OS_REG_READ(ah, AR_TXCFG), AR_FTRIG);

	if (bChannelChange) {
		if (!(ichan->priv_flags & CHANNEL_DFS)) 
			ichan->priv_flags &= ~CHANNEL_INTERFERENCE;
		chan->channel_flags = ichan->channel_flags;
		chan->priv_flags = ichan->priv_flags;
		AH_PRIVATE(ah)->ah_curchan->ah_channel_time=0;
		AH_PRIVATE(ah)->ah_curchan->ah_tsf_last = ar5212GetTsf64(ah);
#ifdef AH_SUPPORT_DFS
		if (opmode == HAL_M_HOSTAP && (chan->priv_flags & CHANNEL_DFS) &&
			(chan->priv_flags & CHANNEL_DFS_CLEAR) == 0 )
			ar5212TxEnable(ah,AH_FALSE);
		else
#endif
			ar5212TxEnable(ah,AH_TRUE);
	}

    UNCHECK_CCK(ah, ichan, ichan_isBmode);
    UNCHECK_CCK(ah, chan, isBmode);

	chan->channel_flags = ichan->channel_flags;
	chan->priv_flags = ichan->priv_flags;

	OS_MARK(ah, AH_MARK_RESET_DONE, 0);

    *halChan = localchan;
    *tempichan = localichan;
	return AH_TRUE;
bad:
        if (ichan) {
            UNCHECK_CCK(ah, ichan, ichan_isBmode);
        }
        UNCHECK_CCK(ah, chan, isBmode);

	OS_MARK(ah, AH_MARK_RESET_DONE, ecode);
	if (*status)
		*status = ecode;
    *halChan = localchan;
    *tempichan = localichan;
	return AH_FALSE;
#undef FAIL
#undef N
}

/*
 * This channel change evaluates whether the selected hardware can
 * perform a synthesizer-only channel change (no reset).  If the
 * TX is not stopped, or the RFBus cannot be granted in the given
 * time, the function returns false as a reset is necessary
 */
HAL_BOOL
ar5212ChannelChange(struct ath_hal *ah, HAL_CHANNEL *chan)
{
	u_int32_t               ulCount;
	u_int32_t               data, synthDelay, qnum;
	u_int16_t               rfXpdGain[4];
	u_int16_t               powerLimit = MAX_RATE_POWER;
	HAL_BOOL                txStopped = AH_TRUE;
	struct ath_hal_5212     *ahp = AH5212(ah);
	HAL_CHANNEL_INTERNAL    *ichan;

	/*
	 * Map public channel to private.
	 */
	ichan = ath_hal_checkchannel(ah, chan);

	/* Modify for static analysis, prevent ichan is NULL */
	if (ichan == AH_NULL)
	{
		return AH_FALSE;
	}
	
	/* TX must be stopped or RF Bus grant will not work */
	for (qnum = 0; qnum < AH_PRIVATE(ah)->ah_caps.hal_total_queues; qnum++) {
		if (ar5212NumTxPending(ah, qnum)) {
			txStopped = AH_FALSE;
			break;
		}
	}

	if (!txStopped)
		return AH_FALSE;

	/* Kill last Baseband Rx Frame */
	OS_REG_WRITE(ah, AR_PHY_RFBUS_REQ, AR_PHY_RFBUS_REQ_REQUEST); /* Request analog bus grant */
	for (ulCount = 0; ulCount < 100; ulCount++) {
		if (OS_REG_READ(ah, AR_PHY_RFBUS_GNT))
			break;
		OS_DELAY(5);
	}

	if (ulCount >= 100)
		return AH_FALSE;

	/* Change the synth */
	if (!ahp->ah_rfHal.setChannel(ah, ichan))
		return AH_FALSE;

	/*
	 * Wait for the frequency synth to settle (synth goes on via PHY_ACTIVE_EN).
	 * Read the phy active delay register. Value is in 100ns increments.
	 */
	data = OS_REG_READ(ah, AR_PHY_RX_DELAY) & AR_PHY_RX_DELAY_DELAY;
	if (IS_CHAN_CCK(ichan))
		synthDelay = (4 * data) / 22;
	else
		synthDelay = data / 10;
	OS_DELAY(synthDelay + BASE_ACTIVATE_DELAY);

	/* Setup the transmit power values. */
	if (!ar5212SetTransmitPower(ah, ichan, rfXpdGain, powerLimit, AH_TRUE, 0)) {
		HDPRINTF(ah, HAL_DBG_RESET, "%s: error init'ing transmit power\n", __func__);
		return AH_FALSE;
	}

	/* Release the RFBus Grant */
	OS_REG_WRITE(ah, AR_PHY_RFBUS_REQ, 0);

	/* Write delta slope for OFDM enabled modes (A, G, Turbo) */
	if (IS_CHAN_OFDM(ichan)) {
		if ((IS_5413(ah) || (ahp->ah_eeprom.ee_version >= AR_EEPROM_VER5_3)) &&
		    (!IS_CHAN_B(chan)))
			ar5212SetSpurMitigation(ah, ichan);
		ar5212SetDeltaSlope(ah, chan);
	}

	/* Start Noise Floor Cal */
	OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);

	return AH_TRUE;
}

void
ar5212SetOperatingMode(struct ath_hal *ah, int opmode)
{
	u_int32_t val;

	val = OS_REG_READ(ah, AR_STA_ID1);
	val &= ~(AR_STA_ID1_STA_AP | AR_STA_ID1_ADHOC);
	switch (opmode) {
	case HAL_M_HOSTAP:
		OS_REG_WRITE(ah, AR_STA_ID1, val | AR_STA_ID1_STA_AP
					| AR_STA_ID1_KSRCH_MODE);
		OS_REG_CLR_BIT(ah, AR_CFG, AR_CFG_AP_ADHOC_INDICATION);
		break;
	case HAL_M_IBSS:
		OS_REG_WRITE(ah, AR_STA_ID1, val | AR_STA_ID1_ADHOC
					| AR_STA_ID1_KSRCH_MODE);
		OS_REG_SET_BIT(ah, AR_CFG, AR_CFG_AP_ADHOC_INDICATION);
		break;
	case HAL_M_STA:
	case HAL_M_MONITOR:
		OS_REG_WRITE(ah, AR_STA_ID1, val | AR_STA_ID1_KSRCH_MODE);
		break;
	}
}

/*
 * Places the PHY and Radio chips into reset.  A full reset
 * must be called to leave this state.  The PCI/MAC/PCU are
 * not placed into reset as we must receive interrupt to
 * re-enable the hardware.
 */
HAL_BOOL
ar5212PhyDisable(struct ath_hal *ah)
{
	return ar5212SetResetReg(ah, AR_RC_BB);
}

/*
 * Places all of hardware into reset
 */
HAL_BOOL
ar5212Disable(struct ath_hal *ah)
{
	if (!ar5212SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE))
		return AH_FALSE;
	/*
	 * Reset the HW - PCI must be reset after the rest of the
	 * device has been reset.
	 */
	return ar5212SetResetReg(ah, AR_RC_MAC | AR_RC_BB | AR_RC_PCI);
}

/*
 * Places the hardware into reset and then pulls it out of reset
 *
 * TODO: Only write the PLL if we're changing to or from CCK mode
 * 
 * WARNING: The order of the PLL and mode registers must be correct.
 */
HAL_BOOL
ar5212ChipReset(struct ath_hal *ah, HAL_CHANNEL *chan)
{

	struct ath_hal_5212 *ahp = AH5212(ah);

	OS_MARK(ah, AH_MARK_CHIPRESET, chan ? chan->channel : 0);

	/*
	 * Reset the HW - PCI must be reset after the rest of the
	 * device has been reset
	 */
	if (!ar5212SetResetReg(ah, AR_RC_MAC | AR_RC_BB | AR_RC_PCI))
		return AH_FALSE;

	/* Bring out of sleep mode (AGAIN) */
	if (!ar5212SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE))
		return AH_FALSE;

	/* Clear warm reset register */
	if (!ar5212SetResetReg(ah, 0))
		return AH_FALSE;

	ahp->ah_chipFullSleep = AH_FALSE;

	/*
	 * Perform warm reset before the mode/PLL/turbo registers
	 * are changed in order to deactivate the radio.  Mode changes
	 * with an active radio can result in corrupted shifts to the
	 * radio device.
	 */

	/*
	 * Set CCK and Turbo modes correctly.
	 */
	if (chan != AH_NULL) {		/* NB: can be null during attach */
		u_int32_t rfMode, phyPLL = 0, curPhyPLL, turbo;

		if (IS_5413(ah)) {
			rfMode = AR_PHY_MODE_AR5112;
			/* XXX: need quarter/half rate code based on clock mode */
			if (IS_CHAN_CCK(chan) || IS_CHAN_G(chan))
				phyPLL = AR_PHY_PLL_CTL_44_5112;
			else
				phyPLL = AR_PHY_PLL_CTL_40_5413;
		} else if (IS_5112(ah) || IS_2413(ah) || IS_2425(ah) || IS_2417(ah)) {
			rfMode = AR_PHY_MODE_AR5112;
			if (IS_CHAN_CCK(chan) || IS_CHAN_G(chan)) {
				phyPLL = AR_PHY_PLL_CTL_44_5112;
			} else {
				if (IS_CHAN_HALF_RATE(chan)) {
					phyPLL = AR_PHY_PLL_CTL_40_5112_HALF;
				} else if (IS_CHAN_QUARTER_RATE(chan)) {
					phyPLL = AR_PHY_PLL_CTL_40_5112_QUARTER;
				} else {
					phyPLL = AR_PHY_PLL_CTL_40_5112;
				}
			}
		} else {
			rfMode = AR_PHY_MODE_AR5111;
			if (IS_CHAN_CCK(chan) || IS_CHAN_G(chan)) {
				phyPLL = AR_PHY_PLL_CTL_44;
			} else {
				if (IS_CHAN_HALF_RATE(chan)) {
					phyPLL = AR_PHY_PLL_CTL_40_HALF;
				} else if (IS_CHAN_QUARTER_RATE(chan)) {
					phyPLL = AR_PHY_PLL_CTL_40_QUARTER;
				} else {
					phyPLL = AR_PHY_PLL_CTL_40;
				}
			}
		}
		if (IS_CHAN_OFDM(chan) && (IS_CHAN_CCK(chan) || 
					   IS_CHAN_G(chan)))
			rfMode |= AR_PHY_MODE_DYNAMIC;
		else if (IS_CHAN_OFDM(chan))
			rfMode |= AR_PHY_MODE_OFDM;
		else
			rfMode |= AR_PHY_MODE_CCK;
		if (IS_CHAN_5GHZ(chan))
			rfMode |= AR_PHY_MODE_RF5GHZ;
		else
			rfMode |= AR_PHY_MODE_RF2GHZ;
		turbo = IS_CHAN_TURBO(chan) ?
			(AR_PHY_FC_TURBO_MODE | AR_PHY_FC_TURBO_SHORT) : 0;
		curPhyPLL = OS_REG_READ(ah, AR_PHY_PLL_CTL);
		/*
		 * PLL, Mode, and Turbo values must be written in the correct
		 * order to ensure:
		 * - The PLL cannot be set to 44 unless the CCK or DYNAMIC
		 *   mode bit is set
		 * - Turbo cannot be set at the same time as CCK or DYNAMIC
		 */
		if (IS_CHAN_CCK(chan) || IS_CHAN_G(chan)) {
			OS_REG_WRITE(ah, AR_PHY_TURBO, turbo);
			OS_REG_WRITE(ah, AR_PHY_MODE, rfMode);
			if (curPhyPLL != phyPLL) {
				OS_REG_WRITE(ah,  AR_PHY_PLL_CTL,  phyPLL);
				/* Wait for the PLL to settle */
				OS_DELAY(PLL_SETTLE_DELAY);
			}
		} else {
			if (curPhyPLL != phyPLL) {
				OS_REG_WRITE(ah,  AR_PHY_PLL_CTL,  phyPLL);
				/* Wait for the PLL to settle */
				OS_DELAY(PLL_SETTLE_DELAY);
			}
			OS_REG_WRITE(ah, AR_PHY_TURBO, turbo);
			OS_REG_WRITE(ah, AR_PHY_MODE, rfMode);
		}
	}
	return AH_TRUE;
}

/*
 * Recalibrate the lower PHY chips to account for temperature/environment
 * changes.
 */
HAL_BOOL
ar5212PerCalibration(struct ath_hal *ah,  HAL_CHANNEL *halChan, 
        u_int8_t rxchainmask, HAL_BOOL longcal, HAL_BOOL *isIQdone, int is_scan, u_int32_t *sched_cals)
{
#define IQ_CAL_TRIES    10
	struct ath_hal_5212 *ahp = AH5212(ah);
    HAL_CHANNEL_INTERNAL   *tempichan = AH_NULL;
    HAL_CHANNEL            localchan    = *halChan, *chan = &localchan;
    HAL_CHANNEL_INTERNAL   localichan, *ichan = &localichan;
	int32_t qCoff, qCoffDenom;
	int32_t iqCorrMeas = 0, iCoff, iCoffDenom;
	u_int32_t powerMeasQ = 0, powerMeasI = 0;
	HAL_BOOL ichan_isBmode = AH_FALSE;
	HAL_BOOL isBmode = AH_FALSE;

	OS_MARK(ah, AH_MARK_PERCAL, chan->channel);

	*isIQdone = AH_FALSE;

	tempichan = ath_hal_checkchannel(ah, chan);
	if (tempichan == AH_NULL) {
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel %u/0x%x; no mapping\n",
			__func__, chan->channel, chan->channel_flags);
		return AH_FALSE;
	}
    localichan = *tempichan;

	CHECK_CCK(ah, ichan, ichan_isBmode);
	CHECK_CCK(ah, chan, isBmode);

#ifdef AH_SUPPORT_DFS
	if (((ichan->priv_flags & CHANNEL_DFS) != 0) &&
	    ((ichan->priv_flags & CHANNEL_INTERFERENCE) != 0)) {
		    HDPRINTF(ah, HAL_DBG_DFS, "%s: invalid channel %u/0x%x; Radar detected on channel\n",
			         __func__, chan->channel, chan->channel_flags);
            UNCHECK_CCK(ah, ichan, ichan_isBmode);
            UNCHECK_CCK(ah, chan, isBmode);
		return AH_FALSE;
	}
#endif

	/* XXX EAR */
	if ((ahp->ah_bIQCalibration == IQ_CAL_DONE) ||
	    (ahp->ah_bIQCalibration == IQ_CAL_INACTIVE)) {
		*isIQdone = AH_TRUE;
		*sched_cals = 0;
	}

	/* IQ calibration in progress. Check to see if it has finished. */
	if (ahp->ah_bIQCalibration == IQ_CAL_RUNNING &&
	    !(OS_REG_READ(ah, AR_PHY_TIMING_CTRL4) & AR_PHY_TIMING_CTRL4_DO_IQCAL)) {
		int i;

		/* IQ Calibration has finished. */
		ahp->ah_bIQCalibration = IQ_CAL_INACTIVE;
		*isIQdone = AH_TRUE;
		*sched_cals = 0;

		/* workaround for misgated IQ Cal results */
		for (i = 0; i < IQ_CAL_TRIES; i++) {
			/* Read calibration results. */
			powerMeasI = OS_REG_READ(ah, AR_PHY_IQCAL_RES_PWR_MEAS_I);
			powerMeasQ = OS_REG_READ(ah, AR_PHY_IQCAL_RES_PWR_MEAS_Q);
			iqCorrMeas = OS_REG_READ(ah, AR_PHY_IQCAL_RES_IQ_CORR_MEAS);
			if (powerMeasI && powerMeasQ)
				break;
			/* Do we really need this??? */
			OS_REG_WRITE (ah,  AR_PHY_TIMING_CTRL4,
				      OS_REG_READ(ah,  AR_PHY_TIMING_CTRL4) |
				      AR_PHY_TIMING_CTRL4_DO_IQCAL);
		}

		/*
		 * Prescale these values to remove 64-bit operation
		 * requirement at the loss of a little precision.
		 */
		iCoffDenom = (powerMeasI / 2 + powerMeasQ / 2) / 128;
		qCoffDenom = powerMeasQ / 128;

		/* Protect against divide-by-0 and loss of sign bits. */
		if (iCoffDenom != 0 && qCoffDenom >= 2) {
			iCoff = (int8_t)(-iqCorrMeas) / iCoffDenom;
			/* IQCORR_Q_I_COFF is a signed 6 bit number */
			if (iCoff < -32) {
				iCoff = -32;
			} else if (iCoff > 31) {
				iCoff = 31;
			}

			/* IQCORR_Q_Q_COFF is a signed 5 bit number */
			qCoff = (powerMeasI / qCoffDenom) - 128;
			if (qCoff < -16) {
				qCoff = -16;
			} else if (qCoff > 15) {
				qCoff = 15;
			}
#ifdef CALIBRATION_DEBUG
			HDPRINTF(ah, HAL_DBG_CALIBRATE, 
                "****************** MISGATED IQ CAL! *******************\n");
			HDPRINTF(ah, HAL_DBG_CALIBRATE, 
                "time       = %lu, i = %d, \n",
				(unsigned long) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP()), i);
			HDPRINTF(ah, HAL_DBG_CALIBRATE, "powerMeasI = 0x%08x\n", powerMeasI);
			HDPRINTF(ah, HAL_DBG_CALIBRATE, "powerMeasQ = 0x%08x\n", powerMeasQ);
			HDPRINTF(ah, HAL_DBG_CALIBRATE, "iqCorrMeas = 0x%08x\n", iqCorrMeas);
			HDPRINTF(ah, HAL_DBG_CALIBRATE, "iCoff      = %d\n", iCoff);
			HDPRINTF(ah, HAL_DBG_CALIBRATE, "qCoff      = %d\n", qCoff);
#endif
			/* Write values and enable correction */
			OS_REG_RMW_FIELD(ah, AR_PHY_TIMING_CTRL4,
				AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF, iCoff);
			OS_REG_RMW_FIELD(ah, AR_PHY_TIMING_CTRL4,
				AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF, qCoff);
			OS_REG_SET_BIT(ah, AR_PHY_TIMING_CTRL4, 
				AR_PHY_TIMING_CTRL4_IQCORR_ENABLE);

			ahp->ah_bIQCalibration = IQ_CAL_DONE;
			ichan->iqCalValid = AH_TRUE;
			ichan->i_coff = iCoff;
			ichan->q_coff = qCoff;
		}
	} else if (!IS_CHAN_B(chan) &&
		   ahp->ah_bIQCalibration == IQ_CAL_DONE &&
		   !ichan->iqCalValid) {
		/*
		 * Start IQ calibration if configured channel has changed.
		 * Use a magic number of 15 based on default value.
		 */
		OS_REG_RMW_FIELD(ah, AR_PHY_TIMING_CTRL4,
			AR_PHY_TIMING_CTRL4_IQCAL_LOG_COUNT_MAX,
			INIT_IQCAL_LOG_COUNT_MAX);
		OS_REG_SET_BIT(ah, AR_PHY_TIMING_CTRL4,
			AR_PHY_TIMING_CTRL4_DO_IQCAL);
		ahp->ah_bIQCalibration = IQ_CAL_RUNNING;
	}
	/* XXX EAR */

	/* Check noise floor results and update history buffer */
	ar5212GetNf(ah, ichan);

    /*
     * Load the NF from history buffer.
     * NF is slow time-variant, so it is OK to use a historical value.
     */
    ar5212LoadNF(ah, AH_PRIVATE(ah)->ah_curchan);

    /* run noise floor calibration */
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);

    /* Perform calibration for 5Ghz channels and any OFDM on 5112 */
    if ( ( IS_CHAN_5GHZ(chan) ||
           ( IS_5112(ah) && IS_CHAN_OFDM(chan) ) ) &&
         !( IS_2413(ah) || IS_5413(ah) || IS_2417(ah) ) ) {
        ar5212RequestRfgain(ah);
    }

	UNCHECK_CCK(ah, ichan, ichan_isBmode);
    UNCHECK_CCK(ah, chan, isBmode);

    *tempichan = localichan;
    *halChan   = *chan;

	return AH_TRUE;
#undef IQ_CAL_TRIES
}

/**************************************************************
 * ar5212MacStop
 *
 * Disables all active QCUs and ensure that the mac is in a
 * quiessence state.
 */
static HAL_BOOL
ar5212MacStop(struct ath_hal *ah)
{
    HAL_BOOL     status;
    u_int32_t    count;
    u_int32_t    pendFrameCount;
    u_int32_t    macStateFlag;
    u_int32_t    queue;

    status = AH_FALSE;

    /* Disable Rx Operation ***********************************/
    OS_REG_SET_BIT(ah, AR_CR, AR_CR_RXD);

    /* Disable TX Operation ***********************************/
    OS_REG_SET_BIT(ah, AR_Q_TXD, AR_Q_TXD_M);

    /* Polling operation for completion of disable ************/
    macStateFlag = TX_ENABLE_CHECK | RX_ENABLE_CHECK;

    for (count = 0; count < MAX_RESET_WAIT; count++) {
        if (macStateFlag & RX_ENABLE_CHECK) {
            if (!OS_REG_IS_BIT_SET(ah, AR_CR, AR_CR_RXE)) {
                macStateFlag &= ~RX_ENABLE_CHECK;
            }
        }

        if (macStateFlag & TX_ENABLE_CHECK) {
            if (!OS_REG_IS_BIT_SET(ah, AR_Q_TXE, AR_Q_TXE_M)) {
                macStateFlag &= ~TX_ENABLE_CHECK;
                macStateFlag |= TX_QUEUEPEND_CHECK;
            }
        }

        if (macStateFlag & TX_QUEUEPEND_CHECK) {
            pendFrameCount = 0;
            for (queue = 0; queue < AR_NUM_DCU; queue++) {
                pendFrameCount += OS_REG_READ(ah, AR_Q0_STS + (queue * 4)) &
                                  AR_Q_STS_PEND_FR_CNT;
            }
            if (pendFrameCount == 0) {
                macStateFlag &= ~TX_QUEUEPEND_CHECK;
            }
        }

        if (macStateFlag == 0) {
            status = AH_TRUE;
            break;
        }
        OS_DELAY(50);
    }

    if (status != AH_TRUE) {
		HDPRINTF(ah, HAL_DBG_RESET, "%s:Failed to stop the MAC state 0x%x\n",
		         __func__, macStateFlag);
    }

    return status;
}

/*
 * Write the given reset bit mask into the reset register
 */
static HAL_BOOL
ar5212SetResetReg(struct ath_hal *ah, u_int32_t resetMask)
{
	u_int32_t mask = resetMask ? resetMask : ~0;
	HAL_BOOL rt;

	/* _Never_ reset PCI Express core */
	if (IS_PCIE(ah)) {
		resetMask &= ~AR_RC_PCI;
	}

	if (resetMask & (AR_RC_MAC | AR_RC_PCI)) {
		/*
		 * To ensure that the driver can reset the
		 * MAC, wake up the chip
		 */
		rt = ar5212SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE);

		if (rt != AH_TRUE) {
			return rt;
		}

		/*
		 * Disable interrupts
		 */
		OS_REG_WRITE(ah, AR_IER, AR_IER_DISABLE);
		OS_REG_READ(ah, AR_IER);

		if (ar5212MacStop(ah) != AH_TRUE) {
			/*
			 * Failed to stop the MAC gracefully; let's be more forceful then
			 */

			/* need some delay before flush any pending MMR writes */
			OS_DELAY(15);
			OS_REG_READ(ah, AR_RXDP);

			resetMask |= AR_RC_MAC | AR_RC_BB;
			/* _Never_ reset PCI Express core */
			if (! IS_PCIE(ah)) {
				resetMask |= AR_RC_PCI;
			}

			/*
			 * Flush the park address of the PCI controller
			 * WAR 7428
			 */
			/* Read PCI slot information less than Hainan revision */
			if (AH_PRIVATE(ah)->ah_bustype == HAL_BUS_TYPE_PCI) {
				if (!IS_5112_REV5_UP(ah)) {
#define PCI_COMMON_CONFIG_STATUS    0x06
					u_int32_t    i;
					u_int16_t    reg16;

					for (i = 0; i < 32; i++) {
						ath_hal_read_pci_config_space(ah, PCI_COMMON_CONFIG_STATUS, &reg16, sizeof(reg16));
					}
				}
#undef PCI_COMMON_CONFIG_STATUS
			}
		} else {
			/*
			 * MAC stopped gracefully; no need to warm-reset the PCI bus
			 */

			resetMask &= ~AR_RC_PCI;

			/* need some delay before flush any pending MMR writes */
			OS_DELAY(15);
			OS_REG_READ(ah, AR_RXDP);
		}
	}

	(void) OS_REG_READ(ah, AR_RXDP);/* flush any pending MMR writes */
	OS_REG_WRITE(ah, AR_RC, resetMask);
	OS_DELAY(15);			/* need to wait at least 128 clocks
					   when reseting PCI before read */
	mask &= (AR_RC_MAC | AR_RC_BB);
	resetMask &= (AR_RC_MAC | AR_RC_BB);
	rt = ath_hal_wait(ah, AR_RC, mask, resetMask, AH_WAIT_TIMEOUT);
	if ((resetMask & AR_RC_MAC) == 0) {
		if (isBigEndian()) {
			/*
			 * Set CFG, little-endian for register
			 * and descriptor accesses.
			 */
#if defined(__LINUX_MIPS32_ARCH__) || defined(__LINUX_MIPS64_ARCH__)
			/* For Hydra */
                        mask = INIT_CONFIG_STATUS | AR_CFG_SWRD;
#ifndef AH_NEED_DESC_SWAP
                        mask |= AR_CFG_SWTD;
#endif
                        OS_REG_WRITE(ah, AR_CFG, mask);

#else
			mask = INIT_CONFIG_STATUS | AR_CFG_SWRD | AR_CFG_SWRG;
#ifndef AH_NEED_DESC_SWAP
			mask |= AR_CFG_SWTD;
#endif
			OS_REG_WRITE(ah, AR_CFG, LE_READ_4(&mask));
#endif
		} else
			OS_REG_WRITE(ah, AR_CFG, INIT_CONFIG_STATUS);
		if (ar5212SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE))
			(void) OS_REG_READ(ah, AR_ISR_RAC);
	}
	return rt;
}

int16_t
ar5212GetNoiseFloor(struct ath_hal *ah)
{
	int16_t nf = (OS_REG_READ(ah, AR_PHY(25)) >> 19) & 0x1ff;
	if (nf & 0x100)
		nf = 0 - ((nf ^ 0x1ff) + 1);
	return nf;
}

static HAL_BOOL
getNoiseFloorThresh(struct ath_hal *ah, const HAL_CHANNEL_INTERNAL *chan,
	int16_t *nft)
{
	struct ath_hal_5212 *ahp = AH5212(ah);

	switch (chan->channel_flags & CHANNEL_ALL_NOTURBO) {
	case CHANNEL_A:
		*nft = ahp->ah_noiseFloorThresh[headerInfo11A];
		break;
	case CHANNEL_B:
		*nft = ahp->ah_noiseFloorThresh[headerInfo11B];
		break;
	case CHANNEL_PUREG:
		*nft = ahp->ah_noiseFloorThresh[headerInfo11G];
		break;
	default:
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel flags 0x%x\n",
			__func__, chan->channel_flags);
		return AH_FALSE;
	}
	return AH_TRUE;
}

static void
ar5212LoadNF(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
    HAL_NFCAL_HIST_FULL *h;
    int32_t val;
    int i;

    /*
     * Write filtered NF values into maxCCApwr register parameter
     * so we can load below.
     */
#ifdef ATH_NF_PER_CHAN
    h = &chan->nf_cal_hist;
#else
    h = &AH_PRIVATE(ah)->nf_cal_hist;
#endif

    val = OS_REG_READ(ah, AR_PHY_CCA);
    val &= 0xFFFFFE00;
    val |= (((u_int32_t)(h->base.priv_nf[0]) << 1) & 0x1ff);
    OS_REG_WRITE(ah, AR_PHY_CCA, val);

    /* Load software filtered NF value into baseband internal minCCApwr variable. */
    OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
    OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);

    /* Wait for load to complete, should be fast, a few 10s of us. */
    for (i = 0; i < 1000; i++) {
        if ((OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0)
            break;
        OS_DELAY(10);
    }
    if (i == 1000) {
        /*
         * We timed out waiting for the noisefloor to load, probably due to an in-progress rx.
         * Simply return here and allow the load plenty of time to complete before the next 
         * calibration interval.  We need to avoid trying to load -50 (which happens below) 
         * while the previous load is still in progress as this can cause rx deafness 
         * (see EV 66368,62830).  Instead by returning here, the baseband nf cal will 
         * just be capped by our present noisefloor until the next calibration timer.
         */
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: *** TIMEOUT while waiting for nf to load: AR_PHY_AGC_CONTROL=0x%x ***\n", 
                 __func__, OS_REG_READ(ah, AR_PHY_AGC_CONTROL));
        return;
    }

    /*
     * Restore maxCCAPower register parameter again so that we're not capped
     * by the median we just loaded.  This will be initial (and max) value
     * of next noise floor calibration the baseband does.
     */
    val = OS_REG_READ(ah, AR_PHY_CCA);
    val &= 0xFFFFFE00;
    val |= (((u_int32_t)(-50) << 1) & 0x1ff);
    OS_REG_WRITE(ah, AR_PHY_CCA, val);

    /* push the value written into minCCApwr above into the calibration HW */
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);
}

/*
 * Read the NF and check it against the noise floor threshhold
 */
int16_t
ar5212GetNf(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
	int16_t nf, nfThresh;
    HAL_NFCAL_HIST_FULL *h;

	if (OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) {
		HDPRINTF(ah, HAL_DBG_NF_CAL, "%s: NF did not complete in calibration window\n",
			__func__);
		nf = 0;
	} else {
		/* Finished NF cal, check against threshold */
		nf = ar5212GetNoiseFloor(ah);
		if (getNoiseFloorThresh(ah, chan, &nfThresh)) {
			if (nf > nfThresh) {
				HDPRINTF(ah, HAL_DBG_NF_CAL, "%s: noise floor failed detected; "
					"detected %u, threshold %u\n", __func__,
					nf, nfThresh);
				/*
				 * NB: Don't discriminate 2.4 vs 5Ghz, if this
				 *     happens it indicates a problem regardless
				 *     of the band.
				 */
				chan->channel_flags |= CHANNEL_CW_INT;
				nf = 0;
			}
		} else
			nf = 0;
	}

    /* Update the NF buffer */
#ifdef ATH_NF_PER_CHAN
    h = &chan->nf_cal_hist;
#else
    h = &AH_PRIVATE(ah)->nf_cal_hist;
#endif

    ar5212UpdateNFHistBuff(h, nf);
    chan->raw_noise_floor = h->base.priv_nf[0];

    return (chan->raw_noise_floor);
}

/*
 *  Update the noise floor buffer as a ring buffer
 */
static void
ar5212UpdateNFHistBuff(HAL_NFCAL_HIST_FULL *h, int16_t nf)
{
    h->nf_cal_buffer[h->base.curr_index][0] = nf;

    if (++h->base.curr_index >= HAL_NF_CAL_HIST_LEN_FULL) {
        h->base.curr_index = 0;
    }
    if (h->base.invalidNFcount > 0) {
        if (nf < AR_PHY_CCA_MIN_BAD_VALUE || nf > AR_PHY_CCA_MAX_HIGH_VALUE) {
            h->base.invalidNFcount = HAL_NF_CAL_HIST_LEN_FULL;
        } else {
            h->base.invalidNFcount--;
            h->base.priv_nf[0] = nf;
        }
    } else {
        h->base.priv_nf[0] = ar5212GetNfHistMid(h, 0);
    }
    return;
}

/*
 *  Pick up the medium one in the noise floor buffer and update the correspondin
g range
 *  for valid noise floor values
 */
static int16_t
ar5212GetNfHistMid(HAL_NFCAL_HIST_FULL *h, int reading)
{
    int16_t nfval;
    int16_t sort[HAL_NF_CAL_HIST_LEN_FULL];
    int i,j;

    for (i = 0; i < HAL_NF_CAL_HIST_LEN_FULL; i ++) {
        sort[i] = h->nf_cal_buffer[i][reading];
    }
    for (i = 0; i < HAL_NF_CAL_HIST_LEN_FULL-1; i ++) {
        for (j = 1; j < HAL_NF_CAL_HIST_LEN_FULL-i; j ++) {
            if (sort[j] > sort[j-1]) {
                nfval = sort[j];
                sort[j] = sort[j-1];
                sort[j-1] = nfval;
            }
        }
    }
    nfval = sort[(HAL_NF_CAL_HIST_LEN_FULL-1)>>1];

    return nfval;
}

/*
 * Set up compression configuration registers
 */
void
ar5212SetCompRegs(struct ath_hal *ah)
{
	u_int32_t i;

        /* Check if h/w supports compression */
	if (AH5212(ah)->ah_compressSupport != AH_TRUE) {
		return;
	}

	OS_REG_WRITE(ah, AR_DCCFG, 1);

	OS_REG_WRITE(ah, AR_CCFG,
		(AR_COMPRESSION_WINDOW_SIZE >> 8) & AR_CCFG_WIN_M);

	OS_REG_WRITE(ah, AR_CCFG,
		OS_REG_READ(ah, AR_CCFG) | AR_CCFG_MIB_INT_EN);
	OS_REG_WRITE(ah, AR_CCUCFG,
		AR_CCUCFG_RESET_VAL | AR_CCUCFG_CATCHUP_EN);

	OS_REG_WRITE(ah, AR_CPCOVF, 0);

	/* reset decompression mask */
	for (i = 0; i < HAL_DECOMP_MASK_SIZE; i++) {
		OS_REG_WRITE(ah, AR_DCM_A, i);
		OS_REG_WRITE(ah, AR_DCM_D, 0);
	}
}

#define	MAX_ANALOG_START	319		/* XXX */

/*
 * Find analog bits of given parameter data and return a reversed value
 */
static u_int32_t
ar5212GetRfField(u_int32_t *rfBuf, u_int32_t numBits, u_int32_t firstBit, u_int32_t column)
{
	u_int32_t reg32 = 0, mask, arrayEntry, lastBit;
	u_int32_t bitPosition, bitsShifted;
	int32_t bitsLeft;

	HALASSERT(column <= 3);
	HALASSERT(numBits <= 32);
	HALASSERT(firstBit + numBits <= MAX_ANALOG_START);

	arrayEntry = (firstBit - 1) / 8;
	bitPosition = (firstBit - 1) % 8;
	bitsLeft = numBits;
	bitsShifted = 0;
	while (bitsLeft > 0) {
		lastBit = (bitPosition + bitsLeft > 8) ?
			(8) : (bitPosition + bitsLeft);
		mask = (((1 << lastBit) - 1) ^ ((1 << bitPosition) - 1)) <<
			(column * 8);
		reg32 |= (((rfBuf[arrayEntry] & mask) >> (column * 8)) >>
			bitPosition) << bitsShifted;
		bitsShifted += lastBit - bitPosition;
		bitsLeft -= (8 - bitPosition);
		bitPosition = 0;
		arrayEntry++;
	}
	reg32 = ath_hal_reverse_bits(reg32, numBits);
	return reg32;
}

HAL_BOOL
ar5212IsSpurChannel(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t clockFreq = ((IS_5413(ah) || IS_2413(ah) || IS_5112(ah) || IS_2417(ah)) ? 40 : 32);
    return ( ((chan->channel % clockFreq) != 0)
          && (((chan->channel % clockFreq) < 10)
         || (((chan->channel) % clockFreq) > 22)) );
}

/*
 * Read EEPROM header info and program the device for correct operation
 * given the channel value.
 */
HAL_BOOL
ar5212SetBoardValues(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
#define NO_FALSE_DETECT_BACKOFF   2
#define CB22_FALSE_DETECT_BACKOFF 6
#define	AR_PHY_BIS(_ah, _reg, _mask, _val) \
	OS_REG_WRITE(_ah, AR_PHY(_reg), \
		(OS_REG_READ(_ah, AR_PHY(_reg)) & _mask) | (_val));
	struct ath_hal_5212 *ahp = AH5212(ah);
	int arrayMode, falseDectectBackoff;
	int is2GHz = IS_CHAN_2GHZ(chan);
	int8_t adcDesiredSize, pgaDesiredSize;
	u_int16_t switchSettling, txrxAtten, rxtxMargin;
	int iCoff, qCoff;

	switch (chan->channel_flags & CHANNEL_ALL) {
	case CHANNEL_A:
	case CHANNEL_T:
		arrayMode = headerInfo11A;
		if (!IS_5112(ah) && !IS_2413(ah) && !IS_5413(ah))
			OS_REG_RMW_FIELD(ah, AR_PHY_FRAME_CTL,
				AR_PHY_FRAME_CTL_TX_CLIP,
				ahp->ah_gainValues.currStep->paramVal[GP_TXCLIP]);
		break;
	case CHANNEL_B:
		arrayMode = headerInfo11B;
		break;
	case CHANNEL_G:
	case CHANNEL_108G:
//	case CHANNEL_XR_G:
		arrayMode = headerInfo11G;
		break;
	default:
		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel flags 0x%x\n",
			__func__, chan->channel_flags);
		return AH_FALSE;
	}

	/* Set the antenna register(s) correctly for the chip revision */
	AR_PHY_BIS(ah, 68, 0xFFFFFC06,
		(ahp->ah_antennaControl[0][arrayMode] << 4) | 0x1);

	ar5212SetAntennaSwitch(ah, ahp->ah_diversityControl,
		(HAL_CHANNEL *) chan, AH_NULL, AH_NULL, AH_NULL);

	/* Set the Noise Floor Thresh on ar5211 devices */
	OS_REG_WRITE(ah, AR_PHY(90),
		(ahp->ah_noiseFloorThresh[arrayMode] & 0x1FF)
		| (1 << 9));

	if (ahp->ah_eeversion >= AR_EEPROM_VER5_0 && IS_CHAN_TURBO(chan)) {
		switchSettling = ahp->ah_switchSettlingTurbo[is2GHz];
		adcDesiredSize = ahp->ah_adcDesiredSizeTurbo[is2GHz];
		pgaDesiredSize = ahp->ah_pgaDesiredSizeTurbo[is2GHz];
		txrxAtten = ahp->ah_txrxAttenTurbo[is2GHz];
		rxtxMargin = ahp->ah_rxtxMarginTurbo[is2GHz];
	} else {
		switchSettling = ahp->ah_switchSettling[arrayMode];
		adcDesiredSize = ahp->ah_adcDesiredSize[arrayMode];
		pgaDesiredSize = ahp->ah_pgaDesiredSize[is2GHz];
		txrxAtten = ahp->ah_txrxAtten[is2GHz];
		rxtxMargin = ahp->ah_rxtxMargin[is2GHz];
	}

	OS_REG_RMW_FIELD(ah, AR_PHY_SETTLING, 
			 AR_PHY_SETTLING_SWITCH, switchSettling);
	OS_REG_RMW_FIELD(ah, AR_PHY_DESIRED_SZ,
			 AR_PHY_DESIRED_SZ_ADC, adcDesiredSize);
	OS_REG_RMW_FIELD(ah, AR_PHY_DESIRED_SZ,
			 AR_PHY_DESIRED_SZ_PGA, pgaDesiredSize);
	OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN,
			 AR_PHY_RXGAIN_TXRX_ATTEN, txrxAtten);
	OS_REG_WRITE(ah, AR_PHY(13),
		(ahp->ah_txEndToXPAOff[arrayMode] << 24)
		| (ahp->ah_txEndToXPAOff[arrayMode] << 16)
		| (ahp->ah_txFrameToXPAOn[arrayMode] << 8)
		| ahp->ah_txFrameToXPAOn[arrayMode]);
	AR_PHY_BIS(ah, 10, 0xFFFF00FF,
		ahp->ah_txEndToXLNAOn[arrayMode] << 8);
	AR_PHY_BIS(ah, 25, 0xFFF80FFF,
		(ahp->ah_thresh62[arrayMode] << 12) & 0x7F000);

	/*
	 * False detect backoff - suspected 32 MHz spur causes false
	 * detects in OFDM, causing Tx Hangs.  Decrease weak signal
	 * sensitivity for this card.
	 */
	falseDectectBackoff = NO_FALSE_DETECT_BACKOFF;
	if (ahp->ah_eeversion >= AR_EEPROM_VER3_3) {
		if (ar5212IsSpurChannel(ah, (HAL_CHANNEL *)chan)) {
			falseDectectBackoff += ahp->ah_falseDetectBackoff[arrayMode];
		}
	}
	AR_PHY_BIS(ah, 73, 0xFFFFFF01, (falseDectectBackoff << 1) & 0xFE);

	if (chan->iqCalValid) {
		iCoff = chan->i_coff;
		qCoff = chan->q_coff;
	} else {
		iCoff = ahp->ah_iqCalI[is2GHz];
		qCoff = ahp->ah_iqCalQ[is2GHz];
	}

	/* write previous IQ results */
	OS_REG_RMW_FIELD(ah, AR_PHY_TIMING_CTRL4,
		AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF, iCoff);
	OS_REG_RMW_FIELD(ah, AR_PHY_TIMING_CTRL4,
		AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF, qCoff);
	OS_REG_SET_BIT(ah, AR_PHY_TIMING_CTRL4,
		AR_PHY_TIMING_CTRL4_IQCORR_ENABLE);

	if (ahp->ah_eeversion >= AR_EEPROM_VER4_1) {
		if (!IS_CHAN_108G(chan) ||
		    ahp->ah_eeversion >= AR_EEPROM_VER5_0)
			OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ,
				AR_PHY_GAIN_2GHZ_RXTX_MARGIN, rxtxMargin);
	}
	if (ahp->ah_eeversion >= AR_EEPROM_VER5_1) {
		/* for now always disabled */
		OS_REG_WRITE(ah,  AR_PHY_HEAVY_CLIP_ENABLE,  0);
	}

	return AH_TRUE;
#undef AR_PHY_BIS
#undef NO_FALSE_DETECT_BACKOFF
#undef CB22_FALSE_DETECT_BACKOFF
}

/*
 * Apply Spur Immunity to Boards that require it.
 * Applies only to OFDM RX operation.
 */

void
ar5212SetSpurMitigation(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichan)
{
	struct ath_hal_5212 *ahp=AH5212(ah);
	u_int32_t pilotMask[2] = {0, 0}, binMagMask[4] = {0, 0, 0 , 0};
	u_int16_t i, finalSpur, curChanAsSpur, binWidth = 0, spurDetectWidth, spurChan;
	int32_t spurDeltaPhase = 0, spurFreqSd = 0, spurOffset, binOffsetNumT16, curBinOffset;
	int16_t numBinOffsets;
	u_int16_t magMapFor4[4] = {1, 2, 2, 1};
	u_int16_t magMapFor3[3] = {1, 2, 1};
	u_int16_t *pMagMap;
	HAL_BOOL is2GHz = IS_CHAN_2GHZ(ichan);
	HAL_EEPROM *ee = &(ahp->ah_eeprom);
	u_int32_t val;

#define CHAN_TO_SPUR(_f, _freq)   ( ((_freq) - ((_f) ? 2300 : 4900)) * 10 )

        if (IS_2417(ah)) {
            HDPRINTF(ah, HAL_DBG_ANI, "%s no spur mitigation\n", __func__);
            return;
        }

	curChanAsSpur = CHAN_TO_SPUR(is2GHz, ichan->channel);

	if (ichan->mainSpur) {
		/* Pull out the saved spur value */
		finalSpur = ichan->mainSpur;
	} else {
		/*
		 * Check if spur immunity should be performed for this channel
		 * Should only be performed once per channel and then saved
		 */
		finalSpur = AR_NO_SPUR;
		spurDetectWidth = HAL_SPUR_CHAN_WIDTH;
		if (IS_CHAN_TURBO(ichan))
			spurDetectWidth *= 2;

		/* Decide if any spur affects the current channel */
		for (i = 0; i < AR_EEPROM_MODAL_SPURS; i++) {
			spurChan = ee->ee_spurChans[i][is2GHz];
			if (spurChan == AR_NO_SPUR) {
				break;
			}
			if ((curChanAsSpur - spurDetectWidth <= (spurChan & HAL_SPUR_VAL_MASK)) &&
			    (curChanAsSpur + spurDetectWidth >= (spurChan & HAL_SPUR_VAL_MASK))) {
				finalSpur = spurChan & HAL_SPUR_VAL_MASK;
				break;
			}
		}
		/* Save detected spur (or no spur) for this channel */
		ichan->mainSpur = finalSpur;
	}

	/* Write spur immunity data */
	if (finalSpur == AR_NO_SPUR) {
		/* Disable Spur Immunity Regs if they appear set */
		if (OS_REG_READ(ah, AR_PHY_TIMING_CTRL4) & AR_PHY_TIMING_CTRL4_ENABLE_SPUR_FILTER) {
			/* Clear Spur Delta Phase, Spur Freq, and enable bits */
			OS_REG_RMW_FIELD(ah, AR_PHY_MASK_CTL, AR_PHY_MASK_CTL_RATE, 0);
			val = OS_REG_READ(ah, AR_PHY_TIMING_CTRL4);
			val &= ~(AR_PHY_TIMING_CTRL4_ENABLE_SPUR_FILTER |
				 AR_PHY_TIMING_CTRL4_ENABLE_CHAN_MASK |
				 AR_PHY_TIMING_CTRL4_ENABLE_PILOT_MASK);
			OS_REG_WRITE(ah, AR_PHY_MASK_CTL, val);
			OS_REG_WRITE(ah, AR_PHY_TIMING11, 0);

			/* Clear pilot masks */
			OS_REG_WRITE(ah, AR_PHY_TIMING7, 0);
			OS_REG_RMW_FIELD(ah, AR_PHY_TIMING8, AR_PHY_TIMING8_PILOT_MASK_2, 0);
			OS_REG_WRITE(ah, AR_PHY_TIMING9, 0);
			OS_REG_RMW_FIELD(ah, AR_PHY_TIMING10, AR_PHY_TIMING10_PILOT_MASK_2, 0);

			/* Clear magnitude masks */
			OS_REG_WRITE(ah, AR_PHY_BIN_MASK_1, 0);
			OS_REG_WRITE(ah, AR_PHY_BIN_MASK_2, 0);
			OS_REG_WRITE(ah, AR_PHY_BIN_MASK_3, 0);
			OS_REG_RMW_FIELD(ah, AR_PHY_MASK_CTL, AR_PHY_MASK_CTL_MASK_4, 0);
			OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_1, 0);
			OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_2, 0);
			OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_3, 0);
			OS_REG_RMW_FIELD(ah, AR_PHY_BIN_MASK2_4, AR_PHY_BIN_MASK2_4_MASK_4, 0);
		}
	} else {
		spurOffset = finalSpur - curChanAsSpur;
		/*
		 * Spur calculations:
		 * spurDeltaPhase is (spurOffsetIn100KHz / chipFrequencyIn100KHz) << 21
		 * spurFreqSd is (spurOffsetIn100KHz / sampleFrequencyIn100KHz) << 11
		 */
		switch (ichan->channel_flags & CHANNEL_ALL) {
		case CHANNEL_A: /* Chip Frequency & sampleFrequency are 40 MHz */
			spurDeltaPhase = (spurOffset << 17) / 25;
			spurFreqSd = spurDeltaPhase >> 10;
			binWidth = HAL_BIN_WIDTH_BASE_100HZ;
			break;
		case CHANNEL_G: /* Chip Frequency is 44MHz, sampleFrequency is 40 MHz */
			spurFreqSd = (spurOffset << 8) / 55;
			spurDeltaPhase = (spurOffset << 17) / 25;
			binWidth = HAL_BIN_WIDTH_BASE_100HZ;
			break;
		case CHANNEL_T: /* Chip Frequency & sampleFrequency are 80 MHz */
		case CHANNEL_108G:
			spurDeltaPhase = (spurOffset << 16) / 25;
			spurFreqSd = spurDeltaPhase >> 10;
			binWidth = HAL_BIN_WIDTH_TURBO_100HZ;
			break;
		default:
			HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel flags 0x%x\n",
			__func__, ichan->channel_flags);
			return;
		}

		/* Compute Pilot Mask */
		binOffsetNumT16 = ((spurOffset * 1000) << 4) / binWidth;
		/* The spur is on a bin if it's remainder at times 16 is 0 */
		if (binOffsetNumT16 & 0xF) {
			numBinOffsets = 4;
			pMagMap = magMapFor4;
		} else {
			numBinOffsets = 3;
			pMagMap = magMapFor3;
		}
		for (i = 0; i < numBinOffsets; i++) {
			if ((binOffsetNumT16 >> 4) > HAL_MAX_BINS_ALLOWED) {
				HDPRINTF(ah, HAL_DBG_ANI, "Too many bins in spur mitigation\n");
				return;
			}

			/* Get Pilot Mask values */
			curBinOffset = (binOffsetNumT16 >> 4) + i + 25;
			if ((curBinOffset >= 0) && (curBinOffset <= 32)) {
				if (curBinOffset <= 25)
					pilotMask[0] |= 1 << curBinOffset;
				else if (curBinOffset >= 27)
					pilotMask[0] |= 1 << (curBinOffset - 1);
			} else if ((curBinOffset >= 33) && (curBinOffset <= 52))
				pilotMask[1] |= 1 << (curBinOffset - 33);

			/* Get viterbi values */
			if ((curBinOffset >= -1) && (curBinOffset <= 14))
				binMagMask[0] |= pMagMap[i] << (curBinOffset + 1) * 2;
			else if ((curBinOffset >= 15) && (curBinOffset <= 30))
				binMagMask[1] |= pMagMap[i] << (curBinOffset - 15) * 2;
			else if ((curBinOffset >= 31) && (curBinOffset <= 46))
				binMagMask[2] |= pMagMap[i] << (curBinOffset -31) * 2;
			else if((curBinOffset >= 47) && (curBinOffset <= 53))
				binMagMask[3] |= pMagMap[i] << (curBinOffset -47) * 2;
		}

		/* Write Spur Delta Phase, Spur Freq, and enable bits */
		OS_REG_RMW_FIELD(ah, AR_PHY_MASK_CTL, AR_PHY_MASK_CTL_RATE, 0xFF);
		val = OS_REG_READ(ah, AR_PHY_TIMING_CTRL4);
		val |= (AR_PHY_TIMING_CTRL4_ENABLE_SPUR_FILTER |
			AR_PHY_TIMING_CTRL4_ENABLE_CHAN_MASK | 
			AR_PHY_TIMING_CTRL4_ENABLE_PILOT_MASK);
		OS_REG_WRITE(ah, AR_PHY_TIMING_CTRL4, val);
		OS_REG_WRITE(ah, AR_PHY_TIMING11, AR_PHY_TIMING11_USE_SPUR_IN_AGC |		
			     SM(spurFreqSd, AR_PHY_TIMING11_SPUR_FREQ_SD) |
			     SM(spurDeltaPhase, AR_PHY_TIMING11_SPUR_DELTA_PHASE));

		/* Write pilot masks */
		OS_REG_WRITE(ah, AR_PHY_TIMING7, pilotMask[0]);
		OS_REG_RMW_FIELD(ah, AR_PHY_TIMING8, AR_PHY_TIMING8_PILOT_MASK_2, pilotMask[1]);
		OS_REG_WRITE(ah, AR_PHY_TIMING9, pilotMask[0]);
		OS_REG_RMW_FIELD(ah, AR_PHY_TIMING10, AR_PHY_TIMING10_PILOT_MASK_2, pilotMask[1]);

		/* Write magnitude masks */
		OS_REG_WRITE(ah, AR_PHY_BIN_MASK_1, binMagMask[0]);
		OS_REG_WRITE(ah, AR_PHY_BIN_MASK_2, binMagMask[1]);
		OS_REG_WRITE(ah, AR_PHY_BIN_MASK_3, binMagMask[2]);
		OS_REG_RMW_FIELD(ah, AR_PHY_MASK_CTL, AR_PHY_MASK_CTL_MASK_4, binMagMask[3]);
		OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_1, binMagMask[0]);
		OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_2, binMagMask[1]);
		OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_3, binMagMask[2]);
		OS_REG_RMW_FIELD(ah, AR_PHY_BIN_MASK2_4, AR_PHY_BIN_MASK2_4_MASK_4, binMagMask[3]);
	}
#undef CHAN_TO_SPUR
}


/*
 * Delta slope coefficient computation.
 * Required for OFDM operation.
 */
void
ar5212SetDeltaSlope(struct ath_hal *ah, HAL_CHANNEL *chan)
{
#define COEF_SCALE_S 24
#define INIT_CLOCKMHZSCALED	0x64000000
	unsigned long coef_scaled, coef_exp, coef_man, ds_coef_exp, ds_coef_man;
	unsigned long clockMhzScaled = INIT_CLOCKMHZSCALED;

	if (IS_CHAN_TURBO(chan))
		clockMhzScaled *= 2;
	/* half and quarter rate can divide the scaled clock by 2 or 4 respectively */
	/* scale for selected channel bandwidth */ 
	if (IS_CHAN_HALF_RATE(chan)) {
		clockMhzScaled = clockMhzScaled >> 1;
	} else if (IS_CHAN_QUARTER_RATE(chan)) {
		clockMhzScaled = clockMhzScaled >> 2;
	} 

	/*
	 * ALGO -> coef = 1e8/fcarrier*fclock/40;
	 * scaled coef to provide precision for this floating calculation 
	 */
	coef_scaled = clockMhzScaled / chan->channel;

	/*
	 * ALGO -> coef_exp = 14-floor(log2(coef)); 
	 * floor(log2(x)) is the highest set bit position
	 */
	for (coef_exp = 31; coef_exp > 0; coef_exp--)
		if ((coef_scaled >> coef_exp) & 0x1)
			break;
	/* A coef_exp of 0 is a legal bit position but an unexpected coef_exp */
	HALASSERT(coef_exp);
	coef_exp = 14 - (coef_exp - COEF_SCALE_S);

	/*
	 * ALGO -> coef_man = floor(coef* 2^coef_exp+0.5);
	 * The coefficient is already shifted up for scaling
	 */
	coef_man = coef_scaled + (1 << (COEF_SCALE_S - coef_exp - 1));
	ds_coef_man = coef_man >> (COEF_SCALE_S - coef_exp);
	ds_coef_exp = coef_exp - 16;

	OS_REG_RMW_FIELD(ah, AR_PHY_TIMING3,
		AR_PHY_TIMING3_DSC_MAN, ds_coef_man);
	OS_REG_RMW_FIELD(ah, AR_PHY_TIMING3,
		AR_PHY_TIMING3_DSC_EXP, ds_coef_exp);
#undef INIT_CLOCKMHZSCALED
#undef COEF_SCALE_S
}

/*
 * Set a limit on the overall output power.  Used for dynamic
 * transmit power control and the like.
 *
 * NB: limit is in units of 0.5 dbM.
 */
HAL_BOOL
ar5212SetTxPowerLimit(struct ath_hal *ah, u_int32_t limit,
                      u_int16_t extra_txpow, u_int16_t tpcInDb)
{
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);    
    u_int16_t    dummyXpdGains[2];
    HAL_BOOL     ret, isBmode = AH_FALSE;
    HAL_CHANNEL_INTERNAL *pichan = ahpriv->ah_curchan;
    HAL_CHANNEL_INTERNAL localichan = *pichan, *ichan = &localichan;

    CHECK_CCK(ah, ichan, isBmode);

    AH_PRIVATE(ah)->ah_power_limit = AH_MIN(limit, MAX_RATE_POWER);
    AH_PRIVATE(ah)->ah_extra_txpow = extra_txpow;
    ret =  ar5212SetTransmitPower(ah, ichan, 
        dummyXpdGains, AH_PRIVATE(ah)->ah_power_limit, AH_FALSE, tpcInDb);
    UNCHECK_CCK(ah, ichan, isBmode);
    *pichan = *ichan;
    return ret;
}

/*
 * Set the transmit power in the baseband for the given
 * operating channel and mode.
 */
HAL_BOOL
ar5212SetTransmitPower(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan, u_int16_t *rfXpdGain,
                       u_int16_t powerLimit, HAL_BOOL calledDuringReset, u_int16_t tpcInDb)
{
#define	POW_OFDM(_r, _s)	(((0 & 1)<< ((_s)+6)) | (((_r) & 0x3f) << (_s)))
#define	POW_CCK(_r, _s)		(((_r) & 0x3f) << (_s))
#define	N(a)			(sizeof (a) / sizeof (a[0]))
	static const u_int16_t tpcScaleReductionTable[5] =
		{ 0, 3, 6, 9, MAX_RATE_POWER };
	struct ath_hal_private *ap = AH_PRIVATE(ah);
	struct ath_hal_5212 *ahp = AH5212(ah);
	int16_t minPower, maxPower;
	int i;

	OS_MEMZERO(ahp->ah_pcdacTable, ahp->ah_pcdacTableSize);
	OS_MEMZERO(ahp->ah_ratesArray, sizeof(ahp->ah_ratesArray));

	powerLimit = AH_MIN(powerLimit, AH_PRIVATE(ah)->ah_power_limit);

	/* Floor at minimum level needed by hardware. Device will be sub-optimal
	 * or non-calibrated under this level. Currently it is 0 dB.
	 * WAR for bug 21971. Limit the lowest value to 0.5 dBm. MIN_RATE_POWER = 1 in ar5212.h
	 * CCX also requires floor at the lowest level supported by hardware.
	 */
	powerLimit = AH_MAX(powerLimit, MIN_RATE_POWER);

	if (powerLimit >= MAX_RATE_POWER)
		tpcInDb = AH_MAX(tpcInDb, tpcScaleReductionTable[AH_PRIVATE(ah)->ah_tp_scale]);

	if (!ar5212SetRateTable(ah, (HAL_CHANNEL *) chan, tpcInDb, powerLimit,
		&minPower, &maxPower)) {
		HDPRINTF(ah, HAL_DBG_RESET, "%s: unable to set rate table\n", __func__);
		return AH_FALSE;
	}
	if (!ahp->ah_rfHal.setPowerTable(ah,
		&minPower, &maxPower, chan, rfXpdGain)) {
		HDPRINTF(ah, HAL_DBG_RESET, "%s: unable to set power table\n", __func__);
		return AH_FALSE;
	}

	/* 
	 * Adjust XR power/rate up by 2 dB to account for greater peak
	 * to avg ratio - except in newer avg power designs
	 */
	if (!IS_2413(ah) && !IS_5413(ah))
		ahp->ah_ratesArray[15] += 4;
	/* 
	 * txPowerIndexOffset is set by the SetPowerTable() call -
	 *  adjust the rate table 
	 */
	for (i = 0; i < N(ahp->ah_ratesArray); i++) {
		ahp->ah_ratesArray[i] += ahp->ah_txPowerIndexOffset;
		if (ahp->ah_ratesArray[i] > 63) 
			ahp->ah_ratesArray[i] = 63;
	}

	if (ahp->ah_eepMap < 2) {
		/* 
		 * WAR to correct gain deltas for 5212 G operation -
		 * Removed with revised chipset
		 */
		if (AH_PRIVATE(ah)->ah_phy_rev < AR_PHY_CHIP_ID_REV_2 &&
		    IS_CHAN_G(chan)) {
			u_int16_t cckOfdmPwrDelta;

			if (chan->channel == 2484) 
				cckOfdmPwrDelta = SCALE_OC_DELTA(
					ahp->ah_cckOfdmPwrDelta - 
					ahp->ah_scaledCh14FilterCckDelta);
			else 
				cckOfdmPwrDelta = SCALE_OC_DELTA(
					ahp->ah_cckOfdmPwrDelta);
			ar5212CorrectGainDelta(ah, cckOfdmPwrDelta);
		}
		/* 
		 * Finally, write the power values into the
		 * baseband power table
		 */
		for (i = 0; i < (PWR_TABLE_SIZE/2); i++) {
			OS_REG_WRITE(ah, AR_PHY_PCDAC_TX_POWER(i),
				 ((((ahp->ah_pcdacTable[2*i + 1] << 8) | 0xff) & 0xffff) << 16)
				| (((ahp->ah_pcdacTable[2*i]     << 8) | 0xff) & 0xffff)
			);
		}
	}

	/* Write the OFDM power per rate set */
	OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE1, 
		POW_OFDM(ahp->ah_ratesArray[3] + ap->ah_extra_txpow, 24)
	      | POW_OFDM(ahp->ah_ratesArray[2] + ap->ah_extra_txpow, 16)
	      | POW_OFDM(ahp->ah_ratesArray[1] + ap->ah_extra_txpow,  8)
	      | POW_OFDM(ahp->ah_ratesArray[0] + ap->ah_extra_txpow,  0)
	);
	OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE2, 
		POW_OFDM(ahp->ah_ratesArray[7], 24)
	      | POW_OFDM(ahp->ah_ratesArray[6], 16)
	      | POW_OFDM(ahp->ah_ratesArray[5],  8)
	      | POW_OFDM(ahp->ah_ratesArray[4],  0)
	);

	/* Write the CCK power per rate set */
	OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE3,
		POW_CCK(ahp->ah_ratesArray[10] + ap->ah_extra_txpow, 24)
	      | POW_CCK(ahp->ah_ratesArray[9] + ap->ah_extra_txpow,  16)
            /* XR target power */
	      | POW_CCK(ahp->ah_ratesArray[15] + ap->ah_extra_txpow,  8)
	      | POW_CCK(ahp->ah_ratesArray[8] + ap->ah_extra_txpow,   0)
	);
	OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE4,
		POW_CCK(ahp->ah_ratesArray[14] + ap->ah_extra_txpow, 24)
	      | POW_CCK(ahp->ah_ratesArray[13] + ap->ah_extra_txpow, 16)
	      | POW_CCK(ahp->ah_ratesArray[12] + ap->ah_extra_txpow,  8)
	      | POW_CCK(ahp->ah_ratesArray[11] + ap->ah_extra_txpow,  0)
	);

	/*
	 * Set max power to 30 dBm and, optionally,
	 * enable TPC in tx descriptors.
	 */
	OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE_MAX, MAX_RATE_POWER | 
		(ap->ah_config.ath_hal_desc_tpc) ?
        AR_PHY_POWER_TX_RATE_MAX_TPC_ENABLE : 0);

	return AH_TRUE;
#undef N
#undef POW_CCK
#undef POW_OFDM
}

/*
 * Sets the transmit power in the baseband for the given
 * operating channel and mode.
 */
static HAL_BOOL
ar5212SetRateTable(struct ath_hal *ah, HAL_CHANNEL *chan,
		   int16_t tpcScaleReduction, int16_t powerLimit,
                   int16_t *pMinPower, int16_t *pMaxPower)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	u_int16_t *rpow = ahp->ah_ratesArray;
	u_int16_t twiceMaxEdgePower = MAX_RATE_POWER;
	u_int16_t twiceMaxEdgePowerCck = MAX_RATE_POWER;
	u_int16_t twiceMaxRDPower = MAX_RATE_POWER;
	int i;
	u_int8_t cfgCtl;
	int8_t twiceAntennaGain, twiceAntennaReduction;
	RD_EDGES_POWER *rep;
	TRGT_POWER_INFO targetPowerOfdm, targetPowerCck;
	int16_t scaledPower, maxAvailPower = 0, reportPower = MAX_RATE_POWER;

	twiceMaxRDPower = chan->max_reg_tx_power * 2;
	*pMaxPower = -MAX_RATE_POWER;
	*pMinPower = MAX_RATE_POWER;

	/* Get conformance test limit maximum for this channel */
	cfgCtl = ath_hal_getctl(ah, chan);

	for (i = 0; i < ahp->ah_numCtls; i++) {
		u_int16_t twiceMinEdgePower;

		if (ahp->ah_ctl[i] == 0)
			continue;
		if (ahp->ah_ctl[i] == cfgCtl ||
		    cfgCtl == ((ahp->ah_ctl[i] & CTL_MODE_M) | SD_NO_CTL)) {
			rep = &ahp->ah_rdEdgesPower[i * NUM_EDGES];
			twiceMinEdgePower = ar5212GetMaxEdgePower(chan->channel, rep);
			if ((cfgCtl & ~CTL_MODE_M) == SD_NO_CTL) {
				/* Find the minimum of all CTL edge powers that apply to this channel */
				twiceMaxEdgePower = AH_MIN(twiceMaxEdgePower, twiceMinEdgePower);
			} else {
				twiceMaxEdgePower = twiceMinEdgePower;
				break;
			}
		}
	}

	if (IS_CHAN_G(chan)) {
		/* Check for a CCK CTL for 11G CCK powers */
		cfgCtl = (cfgCtl & ~CTL_MODE_M) | CTL_11B;
		for (i = 0; i < ahp->ah_numCtls; i++) {
			u_int16_t twiceMinEdgePowerCck;

			if (ahp->ah_ctl[i] == 0)
				continue;
			if (ahp->ah_ctl[i] == cfgCtl ||
			    cfgCtl == ((ahp->ah_ctl[i] & CTL_MODE_M) | SD_NO_CTL)) {
				rep = &ahp->ah_rdEdgesPower[i * NUM_EDGES];
				twiceMinEdgePowerCck = ar5212GetMaxEdgePower(chan->channel, rep);
				if ((cfgCtl & ~CTL_MODE_M) == SD_NO_CTL) {
					/* Find the minimum of all CTL edge powers that apply to this channel */
					twiceMaxEdgePowerCck = AH_MIN(twiceMaxEdgePowerCck, twiceMinEdgePowerCck);
				} else {
					twiceMaxEdgePowerCck = twiceMinEdgePowerCck;
					break;
				}
			}
		}
	} else {
		/* Set the 11B cck edge power to the one found before */
		twiceMaxEdgePowerCck = twiceMaxEdgePower;
	}

	/* Get Antenna Gain reduction */
	if (IS_CHAN_5GHZ(chan)) {
		twiceAntennaGain = ahp->ah_antennaGainMax[0];
	} else {
		twiceAntennaGain = ahp->ah_antennaGainMax[1];
	}
	twiceAntennaReduction =
		ath_hal_getantennareduction(ah, chan, twiceAntennaGain);

	if (IS_CHAN_OFDM(chan)) {
		/* Get final OFDM target powers */
		if (IS_CHAN_2GHZ(chan)) { 
			ar5212GetTargetPowers(ah, chan, ahp->ah_trgtPwr_11g,
				ahp->ah_numTargetPwr_11g, &targetPowerOfdm);
		} else {
			ar5212GetTargetPowers(ah, chan, ahp->ah_trgtPwr_11a,
				ahp->ah_numTargetPwr_11a, &targetPowerOfdm);
		}

		/* Get Maximum OFDM power */
		/* Minimum of target and edge powers */
		scaledPower = AH_MIN(twiceMaxEdgePower,
				twiceMaxRDPower - twiceAntennaReduction);

		/*
		 * If turbo is set, reduce power to keep power
		 * consumption under 2 Watts.  Note that we always do
		 * this unless specially configured.  Then we limit
		 * power only for non-AP operation.
		 */
		if (IS_CHAN_TURBO(chan)
#ifdef AH_ENABLE_AP_SUPPORT
		    && AH_PRIVATE(ah)->ah_opmode != HAL_M_HOSTAP
#endif
		) {
			/*
			 * If turbo is set, reduce power to keep power
			 * consumption under 2 Watts
			 */
			if (ahp->ah_eeversion >= AR_EEPROM_VER3_1)
				scaledPower = AH_MIN(scaledPower,
					ahp->ah_turbo2WMaxPower5);
			/*
			 * EEPROM version 4.0 added an additional
			 * constraint on 2.4GHz channels.
			 */
			if (ahp->ah_eeversion >= AR_EEPROM_VER4_0 &&
			    IS_CHAN_2GHZ(chan))
				scaledPower = AH_MIN(scaledPower,
					ahp->ah_turbo2WMaxPower2);
		}

		maxAvailPower = AH_MIN(scaledPower,
					targetPowerOfdm.twicePwr6_24);

		/* Reduce power by max regulatory domain allowed restrictions */
		scaledPower = maxAvailPower - (tpcScaleReduction * 2);
		scaledPower = (scaledPower < 0) ? 0 : scaledPower;
		scaledPower = AH_MIN(scaledPower, powerLimit);

        /*
         * WAR to avoid setting hardware to use low tx power(like 0dBm) to transmit.
         * Save the original scaledPower for reporting purpose. See EV#57580.
         */
		if (scaledPower < (AH_PRIVATE(ah)->ah_config.ath_hal_legacyMinTxPowerLimit * 2)) {
			reportPower = scaledPower;
			scaledPower = AH_PRIVATE(ah)->ah_config.ath_hal_legacyMinTxPowerLimit * 2;
		}
		/* Set OFDM rates 9, 12, 18, 24 */
		rpow[0] = rpow[1] = rpow[2] = rpow[3] = rpow[4] = scaledPower;

		/* Set OFDM rates 36, 48, 54, XR */
		rpow[5] = AH_MIN(rpow[0], targetPowerOfdm.twicePwr36);
		rpow[6] = AH_MIN(rpow[0], targetPowerOfdm.twicePwr48);
		rpow[7] = AH_MIN(rpow[0], targetPowerOfdm.twicePwr54);

		if (ahp->ah_eeversion >= AR_EEPROM_VER4_0) {
			/* Setup XR target power from EEPROM */
			rpow[15] = AH_MIN(scaledPower, IS_CHAN_2GHZ(chan) ?
				ahp->ah_xrTargetPower2 : ahp->ah_xrTargetPower5);
		} else {
			/* XR uses 6mb power */
			rpow[15] = rpow[0];
		}

		*pMinPower = rpow[7];
		*pMaxPower = rpow[0];

		ahp->ah_ofdmTxPower = *pMaxPower;

		HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: MaxRD: %d TurboMax: %d MaxCTL: %d "
			"TPC_Reduction %d\n",
			__func__,
			twiceMaxRDPower, ahp->ah_turbo2WMaxPower5,
			twiceMaxEdgePower, tpcScaleReduction * 2);
	}

	if (IS_CHAN_CCK(chan) || IS_CHAN_G(chan)) {
		/* Get final CCK target powers */
		ar5212GetTargetPowers(ah, chan, ahp->ah_trgtPwr_11b,
			ahp->ah_numTargetPwr_11b, &targetPowerCck);

		/* Reduce power by max regulatory domain allowed restrictions */
		scaledPower = AH_MIN(twiceMaxEdgePowerCck,
			twiceMaxRDPower - twiceAntennaReduction);
		if (maxAvailPower < AH_MIN(scaledPower, targetPowerCck.twicePwr6_24))
			maxAvailPower = AH_MIN(scaledPower, targetPowerCck.twicePwr6_24);

		/* Reduce power by user selection */
		scaledPower = AH_MIN(scaledPower, targetPowerCck.twicePwr6_24) - (tpcScaleReduction * 2);
		scaledPower = (scaledPower < 0) ? 0 : scaledPower;
		scaledPower = AH_MIN(scaledPower, powerLimit);

        /*
         * WAR to avoid setting hardware to use low tx power(like 0dBm) to transmit.
         * Save the original scaledPower for reporting purpose. See EV#57580.
         */
		if (scaledPower < (AH_PRIVATE(ah)->ah_config.ath_hal_legacyMinTxPowerLimit * 2)) {
			reportPower = scaledPower;
			scaledPower = AH_PRIVATE(ah)->ah_config.ath_hal_legacyMinTxPowerLimit * 2;
		}

		/* Set CCK rates 2L, 2S, 5.5L, 5.5S, 11L, 11S */
		rpow[8]  = AH_MIN(scaledPower, targetPowerCck.twicePwr6_24);
		rpow[9]  = AH_MIN(scaledPower, targetPowerCck.twicePwr36);
		rpow[10] = rpow[9];
		rpow[11] = AH_MIN(scaledPower, targetPowerCck.twicePwr48);
		rpow[12] = rpow[11];
		rpow[13] = AH_MIN(scaledPower, targetPowerCck.twicePwr54);
		rpow[14] = rpow[13];

		/* Set min/max power based off OFDM values or initialization */
		if (rpow[13] < *pMinPower)
		    *pMinPower = rpow[13];
		if (rpow[9] > *pMaxPower)
		    *pMaxPower = rpow[9];

	}
	if (reportPower == MAX_RATE_POWER) {
		ahp->ah_tx6PowerInHalfDbm = *pMaxPower;
	}
	else {
		ahp->ah_tx6PowerInHalfDbm = reportPower;
    }
	AH_PRIVATE(ah)->ah_max_power_level = ahp->ah_tx6PowerInHalfDbm;
	return AH_TRUE;
}

/*
 * Correct for the gain-delta between ofdm and cck mode target
 * powers. Write the results to the rate table and the power table.
 *
 *   Conventions :
 *   1. rpow[ii] is the integer value of 2*(desired power
 *    for the rate ii in dBm) to provide 0.5dB resolution. rate
 *    mapping is as following :
 *     [0..7]  --> ofdm 6, 9, .. 48, 54
 *     [8..14] --> cck 1L, 2L, 2S, .. 11L, 11S
 *     [15]    --> XR (all rates get the same power)
 *   2. powv[ii]  is the pcdac corresponding to ii/2 dBm.
 */
static void
ar5212CorrectGainDelta(struct ath_hal *ah, int twiceOfdmCckDelta)
{
#define	N(_a)	(sizeof(_a) / sizeof(_a[0]))
	struct ath_hal_5212 *ahp = AH5212(ah);
	int16_t ratesIndex[N(ahp->ah_ratesArray)];
	u_int16_t ii, jj, iter;
	int32_t cckIndex;
	int16_t gainDeltaAdjust = ahp->ah_cckOfdmGainDelta;

	/* make a local copy of desired powers as initial indices */
	OS_MEMCPY(ratesIndex, ahp->ah_ratesArray, sizeof(ratesIndex));

	/* fix only the CCK indices */
	for (ii = 8; ii < 15; ii++) {
		/* apply a gain_delta correction of -15 for CCK */
		ratesIndex[ii] -= gainDeltaAdjust;

		/* Now check for contention with all ofdm target powers */
		jj = 0;
		iter = 0;
		/* indicates not all ofdm rates checked forcontention yet */
		while (jj < 16) {
			if (ratesIndex[ii] < 0)
				ratesIndex[ii] = 0;
			if (jj == 8) {		/* skip CCK rates */
				jj = 15;
				continue;
			}
			if (ratesIndex[ii] == ahp->ah_ratesArray[jj]) {
				if (ahp->ah_ratesArray[jj] == 0)
					ratesIndex[ii]++;
				else if (iter > 50) {
					/*
					 * To avoid pathological case of of
					 * dm target powers 0 and 0.5dBm
					 */
					ratesIndex[ii]++;
				} else
					ratesIndex[ii]--;
				/* check with all rates again */
				jj = 0;
				iter++;
			} else
				jj++;
		}
		if (ratesIndex[ii] >= PWR_TABLE_SIZE)
			ratesIndex[ii] = PWR_TABLE_SIZE -1;
		cckIndex = ahp->ah_ratesArray[ii] - twiceOfdmCckDelta;
		if (cckIndex < 0)
			cckIndex = 0;

		/* 
		 * Validate that the indexes for the powv are not
		 * out of bounds. BUG 6694
		 */
		HALASSERT(cckIndex < PWR_TABLE_SIZE);
		HALASSERT(ratesIndex[ii] < PWR_TABLE_SIZE);
		ahp->ah_pcdacTable[ratesIndex[ii]] =
			ahp->ah_pcdacTable[cckIndex];
	}
	/* Override rate per power table with new values */
	for (ii = 8; ii < 15; ii++)
		ahp->ah_ratesArray[ii] = ratesIndex[ii];
#undef N
}

/*
 * Find the maximum conformance test limit for the given channel and CTL info
 */
static u_int16_t
ar5212GetMaxEdgePower(u_int16_t channel, RD_EDGES_POWER *pRdEdgesPower)
{
	/* temp array for holding edge channels */
	u_int16_t tempChannelList[NUM_EDGES];
	u_int16_t clo, chi, twiceMaxEdgePower;
	int i, numEdges;

	/* Get the edge power */
	for (i = 0; i < NUM_EDGES; i++) {
		if (pRdEdgesPower[i].rdEdge == 0)
			break;
		tempChannelList[i] = pRdEdgesPower[i].rdEdge;
	}
	numEdges = i;

	ar5212GetLowerUpperValues(channel, tempChannelList,
		numEdges, &clo, &chi);
	/* Get the index for the lower channel */
	for (i = 0; i < numEdges && clo != tempChannelList[i]; i++)
		;
	/* Is lower channel ever outside the rdEdge? */
	HALASSERT(i != numEdges);

	if ((clo == chi && clo == channel) || (pRdEdgesPower[i].flag)) {
		/* 
		 * If there's an exact channel match or an inband flag set
		 * on the lower channel use the given rdEdgePower 
		 */
		twiceMaxEdgePower = pRdEdgesPower[i].twice_rdEdgePower;
		HALASSERT(twiceMaxEdgePower > 0);
	} else
		twiceMaxEdgePower = MAX_RATE_POWER;
	return twiceMaxEdgePower;
}

/*
 * Returns interpolated or the scaled up interpolated value
 */
static u_int16_t
interpolate(u_int16_t target, u_int16_t srcLeft, u_int16_t srcRight,
	u_int16_t targetLeft, u_int16_t targetRight)
{
	u_int16_t rv;
	int16_t lRatio;

	/* to get an accurate ratio, always scale, if want to scale, then don't scale back down */
	if ((targetLeft * targetRight) == 0)
		return 0;

	if (srcRight != srcLeft) {
		/*
		 * Note the ratio always need to be scaled,
		 * since it will be a fraction.
		 */
		lRatio = (target - srcLeft) * EEP_SCALE / (srcRight - srcLeft);
		if (lRatio < 0) {
		    /* Return as Left target if value would be negative */
		    rv = targetLeft;
		} else if (lRatio > EEP_SCALE) {
		    /* Return as Right target if Ratio is greater than 100% (SCALE) */
		    rv = targetRight;
		} else {
			rv = (lRatio * targetRight + (EEP_SCALE - lRatio) *
					targetLeft) / EEP_SCALE;
		}
	} else {
		rv = targetLeft;
	}
	return rv;
}

/*
 * Return the four rates of target power for the given target power table 
 * channel, and number of channels
 */
static void
ar5212GetTargetPowers(struct ath_hal *ah, HAL_CHANNEL *chan,
	TRGT_POWER_INFO *powInfo,
	u_int16_t numChannels, TRGT_POWER_INFO *pNewPower)
{
	/* temp array for holding target power channels */
	u_int16_t tempChannelList[NUM_TEST_FREQUENCIES];
	u_int16_t clo, chi, ixlo, ixhi;
	int i;

	/* Copy the target powers into the temp channel list */
	for (i = 0; i < numChannels; i++)
		tempChannelList[i] = powInfo[i].testChannel;

	ar5212GetLowerUpperValues(chan->channel, tempChannelList,
		numChannels, &clo, &chi);

	/* Get the indices for the channel */
	ixlo = ixhi = 0;
	for (i = 0; i < numChannels; i++) {
		if (clo == tempChannelList[i]) {
			ixlo = i;
		}
		if (chi == tempChannelList[i]) {
			ixhi = i;
			break;
		}
	}

	/*
	 * Get the lower and upper channels, target powers,
	 * and interpolate between them.
	 */
	pNewPower->twicePwr6_24 = interpolate(chan->channel, clo, chi,
		powInfo[ixlo].twicePwr6_24, powInfo[ixhi].twicePwr6_24);
	pNewPower->twicePwr36 = interpolate(chan->channel, clo, chi,
		powInfo[ixlo].twicePwr36, powInfo[ixhi].twicePwr36);
	pNewPower->twicePwr48 = interpolate(chan->channel, clo, chi,
		powInfo[ixlo].twicePwr48, powInfo[ixhi].twicePwr48);
	pNewPower->twicePwr54 = interpolate(chan->channel, clo, chi,
		powInfo[ixlo].twicePwr54, powInfo[ixhi].twicePwr54);
}

/*
 * Search a list for a specified value v that is within
 * EEP_DELTA of the search values.  Return the closest
 * values in the list above and below the desired value.
 * EEP_DELTA is a factional value; everything is scaled
 * so only integer arithmetic is used.
 *
 * NB: the input list is assumed to be sorted in ascending order
 */
void
ar5212GetLowerUpperValues(u_int16_t v, u_int16_t *lp, u_int16_t listSize,
                          u_int16_t *vlo, u_int16_t *vhi)
{
	u_int32_t target = v * EEP_SCALE;
	u_int16_t *ep = lp+listSize;

	/*
	 * Check first and last elements for out-of-bounds conditions.
	 */
	if (target < (u_int32_t)(lp[0] * EEP_SCALE - EEP_DELTA)) {
		*vlo = *vhi = lp[0];
		return;
	}
	if (target > (u_int32_t)(ep[-1] * EEP_SCALE + EEP_DELTA)) {
		*vlo = *vhi = ep[-1];
		return;
	}

	/* look for value being near or between 2 values in list */
	for (; lp < ep; lp++) {
		/*
		 * If value is close to the current value of the list
		 * then target is not between values, it is one of the values
		 */
		if (abs((int16_t)lp[0] * EEP_SCALE - (int16_t)target) < EEP_DELTA) {
			*vlo = *vhi = lp[0];
			return;
		}
		/*
		 * Look for value being between current value and next value
		 * if so return these 2 values
		 */
		if (target < (u_int32_t)(lp[1] * EEP_SCALE - EEP_DELTA)) {
			*vlo = lp[0];
			*vhi = lp[1];
			return;
		}
	}
}

static const GAIN_OPTIMIZATION_LADDER gainLadder = {
	9,					/* numStepsInLadder */
	4,					/* defaultStepNum */
	{ { {4, 1, 1, 1},  6, "FG8"},
	  { {4, 0, 1, 1},  4, "FG7"},
	  { {3, 1, 1, 1},  3, "FG6"},
	  { {4, 0, 0, 1},  1, "FG5"},
	  { {4, 1, 1, 0},  0, "FG4"},	/* noJack */
	  { {4, 0, 1, 0}, -2, "FG3"},	/* halfJack */
	  { {3, 1, 1, 0}, -3, "FG2"},	/* clip3 */
	  { {4, 0, 0, 0}, -4, "FG1"},	/* noJack */
	  { {2, 1, 1, 0}, -6, "FG0"} 	/* clip2 */
	}
};

const static GAIN_OPTIMIZATION_LADDER gainLadder5112 = {
	8,					/* numStepsInLadder */
	1,					/* defaultStepNum */
	{ { {3, 0,0,0, 0,0,0},   6, "FG7"},	/* most fixed gain */
	  { {2, 0,0,0, 0,0,0},   0, "FG6"},
	  { {1, 0,0,0, 0,0,0},  -3, "FG5"},
	  { {0, 0,0,0, 0,0,0},  -6, "FG4"},
	  { {0, 1,1,0, 0,0,0},  -8, "FG3"},
	  { {0, 1,1,0, 1,1,0}, -10, "FG2"},
	  { {0, 1,0,1, 1,1,0}, -13, "FG1"},
	  { {0, 1,0,1, 1,0,1}, -16, "FG0"},	/* least fixed gain */
	}
};

/*
 * Initialize the gain structure to good values
 */
void
ar5212InitializeGainValues(struct ath_hal *ah)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	GAIN_VALUES *gv = &ahp->ah_gainValues;

	/* initialize gain optimization values */
	if (IS_5112(ah)) {
		gv->currStepNum = gainLadder5112.defaultStepNum;
		gv->currStep =
			&gainLadder5112.optStep[gainLadder5112.defaultStepNum];
		gv->active = AH_TRUE;
		gv->loTrig = 20;
		gv->hiTrig = 85;
	} else {
		gv->currStepNum = gainLadder.defaultStepNum;
		gv->currStep = &gainLadder.optStep[gainLadder.defaultStepNum];
		gv->active = AH_TRUE;
		gv->loTrig = 20;
		gv->hiTrig = 35;
	}
}

static HAL_BOOL
ar5212InvalidGainReadback(struct ath_hal *ah, GAIN_VALUES *gv)
{
	u_int32_t gStep, g, mixOvr;
	u_int32_t L1, L2, L3, L4;

	if (IS_5112(ah)) {
		mixOvr = ar5212GetRfField(ar5212GetRfBank(ah, 7), 1, 36, 0);
		L1 = 0;
		L2 = 107;
		L3 = 0;
		L4 = 107;
		if (mixOvr == 1) {
			L2 = 83;
			L4 = 83;
			gv->hiTrig = 55;
		}
	} else {
		gStep = ar5212GetRfField(ar5212GetRfBank(ah, 7), 6, 37, 0);

		L1 = 0;
		L2 = (gStep == 0x3f) ? 50 : gStep + 4;
		L3 = (gStep != 0x3f) ? 0x40 : L1;
		L4 = L3 + 50;

		gv->loTrig = L1 + (gStep == 0x3f ? DYN_ADJ_LO_MARGIN : 0);
		/* never adjust if != 0x3f */
		gv->hiTrig = L4 - (gStep == 0x3f ? DYN_ADJ_UP_MARGIN : -5);
	}
	g = gv->currGain;

	return !((g >= L1 && g<= L2) || (g >= L3 && g <= L4));
}

/*
 * Enable the probe gain check on the next packet
 */
static void
ar5212RequestRfgain(struct ath_hal *ah)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	u_int32_t probePowerIndex;

	/* Enable the gain readback probe */
	probePowerIndex = ahp->ah_ofdmTxPower + ahp->ah_txPowerIndexOffset;
	OS_REG_WRITE(ah, AR_PHY_PAPD_PROBE,
		  SM(probePowerIndex, AR_PHY_PAPD_PROBE_POWERTX)
		| AR_PHY_PAPD_PROBE_NEXT_TX);

	ahp->ah_rfgainState = HAL_RFGAIN_READ_REQUESTED;
}

/*
 * Exported call to check for a recent gain reading and return
 * the current state of the thermal calibration gain engine.
 */
HAL_RFGAIN
ar5212GetRfgain(struct ath_hal *ah)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	GAIN_VALUES *gv = &ahp->ah_gainValues;
	u_int32_t rddata, probeType;

	if (!gv->active)
		return HAL_RFGAIN_INACTIVE;

	if (ahp->ah_rfgainState == HAL_RFGAIN_READ_REQUESTED) {
		/* Caller had asked to setup a new reading. Check it. */
		rddata = OS_REG_READ(ah, AR_PHY_PAPD_PROBE);

		if ((rddata & AR_PHY_PAPD_PROBE_NEXT_TX) == 0) {
			/* bit got cleared, we have a new reading. */
			gv->currGain = rddata >> AR_PHY_PAPD_PROBE_GAINF_S;
			probeType = MS(rddata, AR_PHY_PAPD_PROBE_TYPE);
			if (probeType == AR_PHY_PAPD_PROBE_TYPE_CCK) {
				HALASSERT(IS_5112(ah));
				if (AH_PRIVATE(ah)->ah_phy_rev >= AR_PHY_CHIP_ID_REV_2)
					gv->currGain += ahp->ah_cckOfdmGainDelta;
				else
					gv->currGain += PHY_PROBE_CCK_CORRECTION;
			}
			if (IS_5112(ah)) {
				ar5212GetGainFCorrection(ah);
				if (gv->currGain >= gv->gainFCorrection)
					gv->currGain -= gv->gainFCorrection;
				else
					gv->currGain = 0;
			}
			/* inactive by default */
			ahp->ah_rfgainState = HAL_RFGAIN_INACTIVE;

			if (!ar5212InvalidGainReadback(ah, gv) &&
			    ar5212IsGainAdjustNeeded(ah, gv) &&
			    ar5212AdjustGain(ah, gv) > 0) {
				/*
				 * Change needed. Copy ladder info
				 * into eeprom info.
				 */
				ahp->ah_rfgainState = HAL_RFGAIN_NEED_CHANGE;
                               /* WAR 18034 */
                                 AH_PRIVATE(ah)->ah_cwCalRequire = AH_TRUE;
				/* Request IQ recalibration for temperature chang */
				ahp->ah_bIQCalibration = IQ_CAL_INACTIVE;
			}
		}
	}
	return ahp->ah_rfgainState;
}

/*
 * Check to see if our readback gain level sits within the linear
 * region of our current variable attenuation window
 */
static HAL_BOOL
ar5212IsGainAdjustNeeded(struct ath_hal *ah, const GAIN_VALUES *gv)
{
	return (gv->currGain <= gv->loTrig || gv->currGain >= gv->hiTrig);
}

/*
 * Move the rabbit ears in the correct direction.
 */
static int32_t 
ar5212AdjustGain(struct ath_hal *ah, GAIN_VALUES *gv)
{
	const GAIN_OPTIMIZATION_LADDER *gl;

	if (IS_5112(ah))
		gl = &gainLadder5112;
	else
		gl = &gainLadder;
	gv->currStep = &gl->optStep[gv->currStepNum];
	if (gv->currGain >= gv->hiTrig) {
		if (gv->currStepNum == 0) {
			HDPRINTF(ah, HAL_DBG_RF_PARAM, "%s: Max gain limit.\n", __func__);
			return -1;
		}
		HDPRINTF(ah, HAL_DBG_RF_PARAM, "%s: Adding gain: currG=%d [%s] --> ",
			__func__, gv->currGain, gv->currStep->stepName);
		gv->targetGain = gv->currGain;
		while (gv->targetGain >= gv->hiTrig && gv->currStepNum > 0) {
			gv->targetGain -= 2 * (gl->optStep[--(gv->currStepNum)].stepGain -
				gv->currStep->stepGain);
			gv->currStep = &gl->optStep[gv->currStepNum];
		}
		HDPRINTF(ah, HAL_DBG_RF_PARAM, "targG=%d [%s]\n",
			gv->targetGain, gv->currStep->stepName);
		return 1;
	}
	if (gv->currGain <= gv->loTrig) {
		if (gv->currStepNum == gl->numStepsInLadder-1) {
			HDPRINTF(ah, HAL_DBG_RF_PARAM, "%s: Min gain limit.\n", __func__);
			return -2;
		}
		HDPRINTF(ah, HAL_DBG_RF_PARAM, "%s: Deducting gain: currG=%d [%s] --> ",
			__func__, gv->currGain, gv->currStep->stepName);
		gv->targetGain = gv->currGain;
		while (gv->targetGain <= gv->loTrig &&
		      gv->currStepNum < (gl->numStepsInLadder - 1)) {
			gv->targetGain -= 2 *
				(gl->optStep[++(gv->currStepNum)].stepGain - gv->currStep->stepGain);
			gv->currStep = &gl->optStep[gv->currStepNum];
		}
		HDPRINTF(ah, HAL_DBG_RF_PARAM, "targG=%d [%s]\n",
			gv->targetGain, gv->currStep->stepName);
		return 2;
	}
	return 0;		/* caller didn't call needAdjGain first */
}

/*
 * Read rf register to determine if gainF needs correction
 */
static void
ar5212GetGainFCorrection(struct ath_hal *ah)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
	GAIN_VALUES *gv = &ahp->ah_gainValues;

	HALASSERT(IS_RADX112_REV2(ah));

	gv->gainFCorrection = 0;
	if (ar5212GetRfField(ar5212GetRfBank(ah, 7), 1, 36, 0) == 1) {
		u_int32_t mixGain = gv->currStep->paramVal[0];
		u_int32_t gainStep =
			ar5212GetRfField(ar5212GetRfBank(ah, 7), 4, 32, 0);
		switch (mixGain) {
		case 0 :
			gv->gainFCorrection = 0;
			break;
		case 1 :
			gv->gainFCorrection = gainStep;
			break;
		case 2 :
			gv->gainFCorrection = 2 * gainStep - 5;
			break;
		case 3 :
			gv->gainFCorrection = 2 * gainStep;
			break;
		}
	}
}

/*
 * Perform analog "swizzling" of parameters into their location
 */
void
ar5212ModifyRfBuffer(u_int32_t *rfBuf, u_int32_t reg32, u_int32_t numBits,
                     u_int32_t firstBit, u_int32_t column)
{
	u_int32_t tmp32, mask, arrayEntry, lastBit;
	int32_t bitPosition, bitsLeft;

	HALASSERT(column <= 3);
	HALASSERT(numBits <= 32);
	HALASSERT(firstBit + numBits <= MAX_ANALOG_START);

	tmp32 = ath_hal_reverse_bits(reg32, numBits);
	arrayEntry = (firstBit - 1) / 8;
	bitPosition = (firstBit - 1) % 8;
	bitsLeft = numBits;
	while (bitsLeft > 0) {
		lastBit = (bitPosition + bitsLeft > 8) ?
			8 : bitPosition + bitsLeft;
		mask = (((1 << lastBit) - 1) ^ ((1 << bitPosition) - 1)) <<
			(column * 8);
		rfBuf[arrayEntry] &= ~mask;
		rfBuf[arrayEntry] |= ((tmp32 << bitPosition) <<
			(column * 8)) & mask;
		bitsLeft -= 8 - bitPosition;
		tmp32 = tmp32 >> (8 - bitPosition);
		bitPosition = 0;
		arrayEntry++;
	}
}

/*
 * Sets the rate to duration values in MAC - used for multi-
 * rate retry.
 * The rate duration table needs to cover all valid rate codes;
 * the XR table covers all ofdm and xr rates, while the 11b table
 * covers all cck rates => all valid rates get covered between
 * these two mode's ratetables!
 * But if we're turbo, the ofdm phy is replaced by the turbo phy
 * and xr or cck is not valid with turbo => all rates get covered
 * by the turbo ratetable only
 */
void
ar5212SetRateDurationTable(struct ath_hal *ah, HAL_CHANNEL *chan)
{
#define	WLAN_CTRL_FRAME_SIZE	(2+2+6+4)	/* ACK+FCS */
	const HAL_RATE_TABLE *rt;
	int i;

	if (IS_CHAN_HALF_RATE(chan)) {
		rt = ar5212GetRateTable(ah, HAL_MODE_11A_HALF_RATE);
	} else if (IS_CHAN_QUARTER_RATE(chan)) {
		rt = ar5212GetRateTable(ah, HAL_MODE_11A_QUARTER_RATE);
	} else {
		rt = ar5212GetRateTable(ah,
			IS_CHAN_TURBO(chan) ? HAL_MODE_TURBO : HAL_MODE_XR);
	}

	for (i = 0; i < rt->rateCount; ++i)
		OS_REG_WRITE(ah,
			AR_RATE_DURATION(rt->info[i].rate_code),
			ath_hal_computetxtime(ah, rt,
				WLAN_CTRL_FRAME_SIZE,
				rt->info[i].controlRate, AH_FALSE));
	if (!IS_CHAN_TURBO(chan)) {
		/* 11g Table is used to cover the CCK rates. */
		rt = ar5212GetRateTable(ah, HAL_MODE_11G);
		for (i = 0; i < rt->rateCount; ++i) {
			u_int32_t reg = AR_RATE_DURATION(rt->info[i].rate_code);

			if (rt->info[i].phy != IEEE80211_T_CCK)
				continue;

			OS_REG_WRITE(ah, reg,
				ath_hal_computetxtime(ah, rt,
					WLAN_CTRL_FRAME_SIZE,
					rt->info[i].controlRate, AH_FALSE));
			/* cck rates have short preamble option also */
			if (rt->info[i].shortPreamble) {
				reg += rt->info[i].shortPreamble << 2;
				OS_REG_WRITE(ah, reg,
					ath_hal_computetxtime(ah, rt,
						WLAN_CTRL_FRAME_SIZE,
						rt->info[i].controlRate,
						AH_TRUE));
			}
		}
	}
#undef WLAN_CTRL_FRAME_SIZE
}

/* Adjust various register settings based on half/quarter rate clock setting.
 * This includes: +USEC, TX/RX latency, 
 *                + IFS params: slot, eifs, misc etc.
 */
void 
ar5212SetIFSTiming(struct ath_hal *ah, HAL_CHANNEL *chan)
{
	u_int32_t txLat, rxLat, usec, slot, refClock, eifs, init_usec;

	refClock = OS_REG_READ(ah, AR_USEC) & AR_USEC_USEC32;
	if (IS_CHAN_HALF_RATE(chan)) {
		slot = IFS_SLOT_HALF_RATE;
		rxLat = RX_NON_FULL_RATE_LATENCY << AR5212_USEC_RX_LAT_S;
		txLat = TX_HALF_RATE_LATENCY << AR5212_USEC_TX_LAT_S;
		usec = HALF_RATE_USEC;
		eifs = IFS_EIFS_HALF_RATE;
		init_usec = INIT_USEC >> 1;
	} else { /* quarter rate */
		slot = IFS_SLOT_QUARTER_RATE;
		rxLat = RX_NON_FULL_RATE_LATENCY << AR5212_USEC_RX_LAT_S;
		txLat = TX_QUARTER_RATE_LATENCY << AR5212_USEC_TX_LAT_S;
		usec = QUARTER_RATE_USEC;
		eifs = IFS_EIFS_QUARTER_RATE;
		init_usec = INIT_USEC >> 2;
	}

	OS_REG_WRITE(ah, AR_USEC, (usec | refClock | txLat | rxLat));
	OS_REG_WRITE(ah, AR_D_GBL_IFS_SLOT, slot);
	OS_REG_WRITE(ah, AR_D_GBL_IFS_EIFS, eifs);
	OS_REG_RMW_FIELD(ah, AR_D_GBL_IFS_MISC,
				AR_D_GBL_IFS_MISC_USEC_DURATION, init_usec);
	return;
}


int ar5212GetSpurInfo(struct ath_hal * ah, int *enable, int len, u_int16_t *freq)
{
   return 0;
}

int ar5212SetSpurInfo(struct ath_hal * ah, int enable, int len, u_int16_t *freq)
{
    return 0;
}

HAL_BOOL
ar5212InterferenceIsPresent(struct ath_hal *ah)
{
    //Not yet implemented for ar5212
    return AH_FALSE;
}
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
HAL_BOOL ar5212_txbf_loforceon_update(struct ath_hal *ah,HAL_BOOL loforcestate)
{
    return AH_FALSE;    
}
#endif

#if ATH_ANT_DIV_COMB
HAL_BOOL
ar5212_ant_ctrl_set_lna_div_use_bt_ant(struct ath_hal *ah, HAL_BOOL enable, HAL_CHANNEL *chan)
{
    return AH_TRUE;
}
#endif /* ATH_ANT_DIV_COMB */
#endif /* AH_SUPPORT_AR5212 */
