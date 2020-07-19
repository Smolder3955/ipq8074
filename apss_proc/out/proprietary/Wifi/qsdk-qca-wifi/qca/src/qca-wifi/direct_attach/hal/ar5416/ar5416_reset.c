/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2010, Atheros Communications Inc. 
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
#include "ah_devid.h"

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"

#define N(a)    (sizeof(a)/sizeof(a[0]))

/* Additional Time delay to wait after activiting the Base band */
#define BASE_ACTIVATE_DELAY         300     /* usec */
#define RTC_PLL_SETTLE_DELAY        100     /* usec */
#define COEF_SCALE_S                24
#define HT40_CHANNEL_CENTER_SHIFT   10      /* MHz      */

extern  HAL_BOOL ar5416ResetTxQueue(struct ath_hal *ah, u_int q);
extern  u_int32_t ar5416NumTxPending(struct ath_hal *ah, u_int q);

static void ar5416Set11nRegs(struct ath_hal *ah, HAL_CHANNEL *chan, HAL_HT_MACMODE macmode);

static inline HAL_BOOL ar5416SetResetPowerOn(struct ath_hal *ah);
static HAL_BOOL ar5416SetReset(struct ath_hal *ah, int type);

#ifndef ATH_NF_PER_CHAN
static inline void
ar5416ResetNfHistBuff(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichan);
#endif
static void     ar5416StartNFCal(struct ath_hal *ah);
static void     ar5416LoadNF(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan,
                             int);
static int      ar5416StoreNewNf(struct ath_hal *, HAL_CHANNEL_INTERNAL *, int);
static int16_t  ar5416UpdateNFHistBuff(struct ath_hal *, HAL_NFCAL_HIST_FULL *h,
                                       int16_t *nfarray, int hist_len);
static int16_t  ar5416GetNfHistMid(HAL_NFCAL_HIST_FULL *h, int reading,
                                   int hist_len);
static void     ar5416SetDeltaSlope(struct ath_hal *, HAL_CHANNEL_INTERNAL *);
static void     ar5416SpurMitigate(struct ath_hal *ah, HAL_CHANNEL *chan);
static void     ar9280SpurMitigate(struct ath_hal *, HAL_CHANNEL *, HAL_CHANNEL_INTERNAL *);
#ifdef AH_SUPPORT_K2
static void     ar9271LoadAniReg(struct ath_hal *ah, HAL_CHANNEL *chan);
#endif

#if 0
HAL_BOOL        ar5416SetTransmitPower(struct ath_hal *ah,
                                       HAL_CHANNEL_INTERNAL *chan);
static HAL_BOOL ar5416SetRateTable(struct ath_hal *,
                HAL_CHANNEL *, int16_t tpcScaleReduction, int16_t powerLimit,
                int16_t *minPower, int16_t *maxPower);
static void ar5416GetTargetPowers(struct ath_hal *, HAL_CHANNEL *,
                TRGT_POWER_INFO *pPowerInfo, u_int16_t numChannels,
                TRGT_POWER_INFO *pNewPower);
static u_int16_t ar5416GetMaxEdgePower(u_int16_t channel,
                RD_EDGES_POWER  *pRdEdgesPower);
#endif
static inline HAL_CHANNEL_INTERNAL* ar5416CheckChan(struct ath_hal *ah,
                                                    HAL_CHANNEL *chan);
static inline HAL_STATUS ar5416ProcessIni(struct ath_hal *ah,
                                          HAL_CHANNEL *chan,
                                          HAL_CHANNEL_INTERNAL *ichan,
                                          HAL_HT_MACMODE macmode);
static inline void ar5416SetRfMode(struct ath_hal *ah, HAL_CHANNEL *chan);
static void ar5416GetDeltaSlopeValues(struct ath_hal *ah,
                                             u_int32_t coef_scaled,
                                             u_int32_t *coef_mantissa,
                                             u_int32_t *coef_exponent);

#ifdef AH_SUPPORT_2133
/* NB: public for RF backend use */
void    ar5416ModifyRfBuffer(u_int32_t *rfBuf, u_int32_t reg32,
                u_int32_t numBits, u_int32_t firstBit, u_int32_t column);
#endif /* AH_SUPPORT_2133 */

static inline HAL_BOOL ar5416ChannelChange(struct ath_hal *ah,
                                           HAL_CHANNEL *chan,
                                           HAL_CHANNEL_INTERNAL *ichan,
                                           HAL_HT_MACMODE macmode);

static inline void ar5416OverrideIni(struct ath_hal *ah, HAL_CHANNEL *chan);
void ar5416InitPLL(struct ath_hal *ah, HAL_CHANNEL *chan);
static inline void ar5416InitChainMasks(struct ath_hal *ah);
static inline HAL_BOOL ar5416InitCal(struct ath_hal *ah, HAL_CHANNEL *chan);
HAL_BOOL ar5416IsCalSupp(struct ath_hal *ah, HAL_CHANNEL *chan,
                         HAL_CAL_TYPES calType);
void ar5416SetupCalibration(struct ath_hal *ah,
                            HAL_CAL_LIST *currCal);
void ar5416ResetCalibration(struct ath_hal *ah,
                            HAL_CAL_LIST *perCal);
inline HAL_BOOL ar5416RunInitCals(struct ath_hal *ah,
                                  int init_cal_count);
void ar5416PerCalibration(struct ath_hal *ah,
                          HAL_CHANNEL_INTERNAL *ichan,
                          u_int8_t rxchainmask,
                          HAL_CAL_LIST *perCal,
                          HAL_BOOL *isCalDone);

static inline void ar5416SetDma(struct ath_hal *ah);
static inline void ar5416InitBB(struct ath_hal *ah, HAL_CHANNEL *chan);
static inline void ar5416InitInterruptMasks(struct ath_hal *ah,
                                            HAL_OPMODE opmode);
static inline void ar5416InitQOS(struct ath_hal *ah);
static inline void ar5416InitUserSettings(struct ath_hal *ah);
static inline void ar5416AttachHwPlatform(struct ath_hal *ah);
static void ar5416OpenLoopPowerControlInit(struct ath_hal *ah);
static inline void ar5416InitMFP(struct ath_hal *ah);
#ifdef AH_SUPPORT_KITE_ANY
static inline void ar9285PACal(struct ath_hal *ah, HAL_BOOL isReset);
#endif /* AH_SUPPORT_KITE_ANY */
#ifdef AH_SUPPORT_K2
static inline void ar9271PACal(struct ath_hal *ah, HAL_BOOL isReset);
static inline void ar9271CCKfirCoeff(struct ath_hal *ah, HAL_CHANNEL *chan);
#endif /* AH_SUPPORT_K2 */

#define HAL_GREEN_AP_RX_MASK 0x1

#define ar5416CheckOpMode(_opmode) \
    ((_opmode == HAL_M_STA) || (_opmode == HAL_M_IBSS) ||\
     (_opmode == HAL_M_HOSTAP) || (_opmode == HAL_M_MONITOR))


/*
 * Places the device in and out of reset and then places sane
 * values in the registers based on EEPROM config, initialization
 * vectors (as determined by the mode), and station configuration
 *
 * bChannelChange is used to preserve DMA/PCU registers across
 * a HW Reset during channel change.
 */
HAL_BOOL
ar5416Reset(struct ath_hal *ah, HAL_OPMODE opmode, HAL_CHANNEL *chan,
           HAL_HT_MACMODE macmode, u_int8_t txchainmask, u_int8_t rxchainmask,
           HAL_HT_EXTPROTSPACING extprotspacing, HAL_BOOL bChannelChange,
           HAL_STATUS *status, int is_scan)
{
#define FAIL(_code)     do { ecode = _code; goto bad; } while (0)
    u_int32_t               saveLedState;
    struct ath_hal_5416     *ahp = AH5416(ah);
    struct ath_hal_private  *ap  = AH_PRIVATE(ah);
    HAL_CHANNEL_INTERNAL    *ichan;
    u_int32_t               saveDefAntenna;
    u_int32_t               macStaId1;
    HAL_STATUS              ecode;
    int                     i, rx_chainmask;
    u_int64_t               tsf = 0;
#ifdef ATH_FORCE_PPM
    u_int32_t saveForceVal, tmpReg;
#endif

#if WLAN_SPECTRAL_ENABLE
    u_int32_t               cached_spectral_config=0;
    HAL_BOOL                restoreSpectral = AH_FALSE;
#endif

    if (OS_REG_READ(ah, AR_IER) == AR_IER_ENABLE) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "** Reset called with WLAN "
                "interrupt enabled %08x **\n", ar5416GetInterrupts(ah));
    }

    ahp->ah_extprotspacing = extprotspacing;
    ahp->ah_txchainmask = txchainmask;
    ahp->ah_rxchainmask = rxchainmask;

    if ((AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable)) {
        AH_PRIVATE(ah)->green_tx_status = HAL_RSSI_TX_POWER_NONE;
    }
    
    if (AR_SREV_KITE(ah) || AR_SREV_K2(ah)) {
        /* Kite has only one chain support, clear chain 1 & 2 setting */
        ahp->ah_txchainmask &= 0x1;
        ahp->ah_rxchainmask &= 0x1;
    } else if (AR_SREV_MERLIN(ah) || AR_SREV_KIWI(ah)) {
        /* Merlin has only two chain support, clear chain 2 setting */
        ahp->ah_txchainmask &= 0x3;
        ahp->ah_rxchainmask &= 0x3;
    }

    HALASSERT(ar5416CheckOpMode(opmode));

    OS_MARK(ah, AH_MARK_RESET, bChannelChange);

    /*
     * Map public channel to private.
     */
    ichan = ar5416CheckChan(ah, chan);
    if (ichan == AH_NULL) {
        HDPRINTF(ah, HAL_DBG_CHANNEL,
            "%s: invalid channel %u/0x%x; no mapping\n",
             __func__, chan->channel, chan->channel_flags);
        FAIL(HAL_EINVAL);
    }

    /* Bring out of sleep mode */
    if (!ar5416SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE)) return AH_FALSE;

    /* Get the value from the previous NF cal and update history buffer */
    /* If chip has gone through full sleep, noise floor is invalid */
    if (ap->ah_curchan && (ahp->ah_chipFullSleep != AH_TRUE)) {
        ar5416StoreNewNf(ah, ap->ah_curchan, is_scan);
    }

    /*
     * Account for the effect of being in either the 2 GHz or 5 GHz band
     * on the nominal, max allowable, and min allowable noise floor values.
     */
    ap->nfp = IS_CHAN_2GHZ(chan) ? &ap->nf_2GHz : &ap->nf_5GHz;

#ifndef ATH_NF_PER_CHAN
    /*
     * If there's only one full-size home-channel NF history buffer
     * rather than a full-size NF history buffer per channel, decide
     * whether to (re)initialize the home-channel NF buffer.
     * If this is just a channel change for a scan, or if the channel
     * is not being changed, don't mess up the home channel NF history
     * buffer with NF values from this scanned channel.  If we're
     * changing the home channel to a new channel, reset the home-channel
     * NF history buffer with the most accurate NF known for the new channel.
     */
    if (!is_scan && /* just scanning -> don't reset home NF buffer */
        (!ap->ah_curchan || /* no old chan -> reset home NF buffer */
         /* new chan different than old chan -> reset home NF buffer */
         ap->ah_curchan->channel != chan->channel ||
         ap->ah_curchan->channel_flags != chan->channel_flags)) {
        ar5416ResetNfHistBuff(ah, ichan);
    }
#endif

#if WLAN_SPECTRAL_ENABLE
    if (ar5416IsSpectralActive(ah)) {
            HDPRINTF(ah, HAL_DBG_RESET, "%s: spectral is active cache config\n",__func__);
            cached_spectral_config = ar5416GetSpectralConfig(ah);
            restoreSpectral = AH_TRUE;
    }
#endif

#ifdef AH_SUPPORT_K2
    if (AR_SREV_K2(ah)) {
        ar9271CCKfirCoeff(ah, chan);
    }
#endif

    /* reset the counters */
    AH5416(ah)->ah_cycleCount = 0;
    AH5416(ah)->ah_ctlBusy = 0;
    AH5416(ah)->ah_extBusy = 0;
    AH5416(ah)->ah_Rf = 0;
    AH5416(ah)->ah_Tf = 0;
    OS_REG_WRITE(ah, AR_RCCNT, 0);
    OS_REG_WRITE(ah, AR_EXTRCCNT, 0);
    OS_REG_WRITE(ah, AR_CCCNT, 0);
    OS_REG_WRITE(ah, AR_RFCNT, 0);
    OS_REG_WRITE(ah, AR_TFCNT, 0);

    /*
     * Fast channel change (Change synthesizer based on channel freq
     * without resetting chip)
     * Don't do it when
     *   - Flag is not set
     *   - Chip is just coming out of full sleep
     *   - Channel to be set is same as current channel
     *   - Channel flags are different, like when moving from
     *     2GHz to 5GHz channels
     *   - Merlin: Switching in/out of fast clock enabled channels
     *             (not currently coded, since fast clock is enabled
     *             across the 5GHz band and we already do a full reset
     *             when switching in/out of 5GHz channels)
     *  If chip is Merlin, do not do fast channel change in 5 Ghz,  
     *  
     * 
     */
    if (
        (bChannelChange &&
        !(ap->ah_config.ath_hal_fullResetWarEnable && AR_SREV_MERLIN(ah)) &&
         (ahp->ah_chipFullSleep != AH_TRUE) &&
         (ap->ah_curchan != AH_NULL) &&
         !(IS_CHAN_5GHZ(chan) && AR_SREV_MERLIN(ah)) &&
         (chan->channel != ap->ah_curchan->channel) &&
         ((chan->channel_flags & CHANNEL_ALL) ==
          (ap->ah_curchan->channel_flags & CHANNEL_ALL)))
        ||
        (AR_SREV_K2(ah) &&
         (ahp->ah_chipFullSleep != AH_TRUE) &&
         (ap->ah_curchan != AH_NULL) &&
         ((chan->channel_flags & CHANNEL_ALL) ==
          (ap->ah_curchan->channel_flags & CHANNEL_ALL)) &&
         (ap->ah_opmode == opmode))
       ) {
        if (ar5416ChannelChange(ah, chan, ichan, macmode)) {
            chan->channel_flags = ichan->channel_flags;
            chan->priv_flags = ichan->priv_flags;
            ap->ah_curchan->ah_channel_time = 0;
            ap->ah_curchan->ah_tsf_last = ar5416GetTsf64(ah);
            /*
             * Load the NF from history buffer of the current channel.
             * NF is slow time-variant, so it is OK to use a historical value.
             */
            ar5416LoadNF(ah, ap->ah_curchan, is_scan);

            /* start NF calibration, without updating BB NF register*/
            ar5416StartNFCal(ah);

#ifdef AH_SUPPORT_K2
            if ( AR_SREV_K2(ah)) {
                /* Load ANI related registers */
                ar9271LoadAniReg(ah, chan);
            }
#endif

#if WLAN_SPECTRAL_ENABLE
            if (restoreSpectral ==  AH_TRUE) {
                HDPRINTF(ah, HAL_DBG_RESET, "%s %d: restore spectral config 0x%x\n",
                    __func__, __LINE__, cached_spectral_config);
                ar5416RestoreSpectralConfig(ah, cached_spectral_config);
                restoreSpectral = AH_FALSE;
             }
#endif
            /*
             * If ChannelChange completed and DMA was stopped
             * successfully - skip the rest of reset
             */
            if (AH5416(ah)->ah_dma_stuck != AH_TRUE) return AH_TRUE;
        }
    }
    AH5416(ah)->ah_dma_stuck = AH_FALSE;

#ifdef ATH_FORCE_PPM
    /* Preserve force ppm state */
    saveForceVal =
        OS_REG_READ(ah, AR_PHY_TIMING2) &
        (AR_PHY_TIMING2_USE_FORCE_PPM | AR_PHY_TIMING2_FORCE_PPM_VAL);
#endif

    /*
     * Preserve the antenna on a channel change
     */
    saveDefAntenna = OS_REG_READ(ah, AR_DEF_ANTENNA);
    if (saveDefAntenna == 0) saveDefAntenna = 1;

    /* Save hardware flag before chip reset clears the register */
    macStaId1 = OS_REG_READ(ah, AR_STA_ID1) & AR_STA_ID1_BASE_RATE_11B;

    /* For chips on which RTC reset is done, save TSF before it gets cleared */
    if (AR_SREV_MERLIN(ah) && ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
        tsf = ar5416GetTsf64(ah);
    }

    /* Save led state from pci config register */
    saveLedState = OS_REG_READ(ah, AR_CFG_LED) &
            (AR_CFG_LED_ASSOC_CTL | AR_CFG_LED_MODE_SEL |
             AR_CFG_LED_BLINK_THRESH_SEL | AR_CFG_LED_BLINK_SLOW);

    /* Mark PHY inactive prior to reset, to be undone in ar5416InitBB () */
    ar5416MarkPhyInactive(ah);


    if (!ar5416ChipReset(ah, chan)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: chip reset failed\n", __func__);
        FAIL(HAL_EIO);
    }


    OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);

    /* Restore TSF for chips on which RTC reset has been done */
    if (tsf && AR_SREV_MERLIN(ah) && ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
        ar5416SetTsf64(ah, tsf);
    }

    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        /* Disable JTAG */
        OS_REG_SET_BIT(ah, AR_GPIO_INPUT_EN_VAL, AR_GPIO_JTAG_DISABLE);
    }

    if( AR_SREV_KIWI_13_OR_LATER(ah) ) {
        /* Enable ASYNC FIFO */
        OS_REG_SET_BIT(ah, AR_MAC_PCU_ASYNC_FIFO_REG3, AR_MAC_PCU_ASYNC_FIFO_REG3_DATAPATH_SEL);
        OS_REG_SET_BIT(ah, AR_PHY_MODE, AR_PHY_MODE_ASYNCFIFO);
        OS_REG_CLR_BIT(ah, AR_MAC_PCU_ASYNC_FIFO_REG3, AR_MAC_PCU_ASYNC_FIFO_REG3_SOFT_RESET);
        OS_REG_SET_BIT(ah, AR_MAC_PCU_ASYNC_FIFO_REG3, AR_MAC_PCU_ASYNC_FIFO_REG3_SOFT_RESET);
    }

    /*
     * Note that ar5416InitChainMasks() is called from within
     * ar5416ProcessIni() to ensure the swap bit is set before
     * the pdadc table is written.
     */

    ecode = ar5416ProcessIni(ah, chan, ichan, macmode);
    if (ecode != HAL_OK) goto bad;

    ahp->ah_immunity_on = AH_FALSE;

    if (ath_hal_getcapability(ah, HAL_CAP_RIFS_RX, 0, AH_NULL) ==
        HAL_ENOTSUPP) {
        /* For devices that need SW assistance for RIFS Rx (Owl), disable
         * RIFS Rx enablement as part of reset.
         */
        if (ahp->ah_rifs_enabled) {
            ahp->ah_rifs_enabled = AH_FALSE;
            OS_MEMZERO(ahp->ah_rifs_reg, sizeof(ahp->ah_rifs_reg));
        }
    } else {
        /* For devices with full HW RIFS Rx support (Sowl/Howl/Merlin, etc),
         * restore register settings from prior to reset.
         */
        if ((ap->ah_curchan != AH_NULL) &&
            (ar5416GetCapability(ah, HAL_CAP_BB_RIFS_HANG, 0, AH_NULL)
             == HAL_OK)) {
            /* Re-program RIFS Rx policy after reset */
            ar5416SetRifsDelay(ah, ahp->ah_rifs_enabled);
        }
    }

    /* Initialize Management Frame Protection */
    ar5416InitMFP(ah);

    ahp->ah_immunity_vals[0] = OS_REG_READ_FIELD(ah,
        AR_PHY_SFCORR_LOW, AR_PHY_SFCORR_LOW_M1_THRESH_LOW);
    ahp->ah_immunity_vals[1] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR_LOW,
        AR_PHY_SFCORR_LOW_M2_THRESH_LOW);
    ahp->ah_immunity_vals[2] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR,
        AR_PHY_SFCORR_M1_THRESH);
    ahp->ah_immunity_vals[3] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR,
        AR_PHY_SFCORR_M2_THRESH);
    ahp->ah_immunity_vals[4] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR,
        AR_PHY_SFCORR_M2COUNT_THR);
    ahp->ah_immunity_vals[5] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR_LOW,
        AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW);

    /* Write delta slope for OFDM enabled modes (A, G, Turbo) */
    if (IS_CHAN_OFDM(chan)|| IS_CHAN_HT(chan)) {
            ar5416SetDeltaSlope(ah, ichan);
    }

    /*
     * For Merlin, spur can break CCK MRC algorithm. SpurMitigate needs to
     * be called in all 11A/B/G/HT modes to disable CCK MRC if spur is found
     * in this channel.
     */
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) ar9280SpurMitigate(ah, chan, ichan);
    else ar5416SpurMitigate(ah, chan);

    if (!ar5416EepromSetBoardValues(ah, ichan)) {
        HDPRINTF(ah, HAL_DBG_EEPROM,
            "%s: error setting board options\n", __func__);
        FAIL(HAL_EIO);
    }

#ifndef ATH_FORCE_BIAS
    /*
     * Antenna Control without forceBias.
     * This function must be called after
     * ar5416SetRfRegs() and ar5416EepromSetBoardValues().
     * This function is only used for Owl.
     * Apply to other chip like Kite may cause BSOD.
     */
    if (AR_SREV_OWL(ah)) ahp->ah_rfHal.decreaseChainPower(ah, chan);
#endif /* !ATH_FORCE_BIAS */

    OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);

    ar5416SetOperatingMode(ah, opmode);

    ENABLE_REG_WRITE_BUFFER

    OS_REG_WRITE(ah, AR_STA_ID0, LE_READ_4(ahp->ah_macaddr));
    OS_REG_WRITE(ah, AR_STA_ID1, LE_READ_2(ahp->ah_macaddr + 4)
            | macStaId1
            | AR_STA_ID1_RTS_USE_DEF
            | (ap->ah_config.ath_hal_6mb_ack ? AR_STA_ID1_ACKCTS_6MB : 0)
            | ahp->ah_staId1Defaults
    );

    /* Set Venice BSSID mask according to current state */
    OS_REG_WRITE(ah, AR_BSSMSKL, LE_READ_4(ahp->ah_bssidmask));
    OS_REG_WRITE(ah, AR_BSSMSKU, LE_READ_2(ahp->ah_bssidmask + 4));

    /* Restore previous antenna */
    OS_REG_WRITE(ah, AR_DEF_ANTENNA, saveDefAntenna);

#ifdef ATH_FORCE_PPM
    /* Restore force ppm state */
    tmpReg = OS_REG_READ(ah, AR_PHY_TIMING2) &
                ~(AR_PHY_TIMING2_USE_FORCE_PPM | AR_PHY_TIMING2_FORCE_PPM_VAL);
    OS_REG_WRITE(ah, AR_PHY_TIMING2, tmpReg | saveForceVal);
#endif

    /* then our BSSID and assocID */
    OS_REG_WRITE(ah, AR_BSS_ID0, LE_READ_4(ahp->ah_bssid));
    OS_REG_WRITE(ah, AR_BSS_ID1,
        LE_READ_2(ahp->ah_bssid + 4) |
        ((ahp->ah_assocId & 0x3fff) << AR_BSS_ID1_AID_S));

    OS_REG_WRITE(ah, AR_ISR, ~0);           /* cleared on write */

    OS_REG_WRITE(ah, AR_RSSI_THR, INIT_RSSI_THR);

    OS_REG_WRITE_FLUSH(ah);
    DISABLE_REG_WRITE_BUFFER

    /*
     * Set Channel now modifies bank 6 parameters for FOWL workaround
     * to force rf_pwd_icsyndiv bias current as function of synth
     * frequency.Thus must be called after ar5416ProcessIni() to ensure
     * analog register cache is valid.
     */
    if (!ahp->ah_rfHal.setChannel(ah, ichan)) FAIL(HAL_EIO);


    OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);

    /* Set 1:1 QCU to DCU mapping for all queues */
    ENABLE_REG_WRITE_BUFFER
    for (i = 0; i < AR_NUM_DCU; i++) {
        OS_REG_WRITE(ah, AR_DQCUMASK(i), 1 << i);
    }
    OS_REG_WRITE_FLUSH(ah);
    DISABLE_REG_WRITE_BUFFER

    ahp->ah_intrTxqs = 0;
    for (i = 0; i < ap->ah_caps.hal_total_queues; i++) {
        ar5416ResetTxQueue(ah, i);
    }

    ar5416InitInterruptMasks(ah, opmode);

    if (ath_hal_isrfkillenabled(ah)) ar5416EnableRfKill(ah);

    ar5416InitQOS(ah);

    ar5416InitUserSettings(ah);

    if (AR_SREV_KIWI_13_OR_LATER(ah)) {
        /* Enable ASYNC FIFO
         * If Async FIFO is enabled, the following counters change as MAC now runs at 117 Mhz
         * instead of 88/44MHz when async FIFO is disabled.
         * *NOTE* THE VALUES BELOW TESTED FOR HT40 2 CHAIN
         * Overwrite the delay/timeouts initialized in ProcessIni() above.
		 */
        OS_REG_WRITE(ah, AR_D_GBL_IFS_SIFS, AR_D_GBL_IFS_SIFS_ASYNC_FIFO_DUR);
        OS_REG_WRITE(ah, AR_D_GBL_IFS_SLOT, AR_D_GBL_IFS_SLOT_ASYNC_FIFO_DUR);
        OS_REG_WRITE(ah, AR_D_GBL_IFS_EIFS, AR_D_GBL_IFS_EIFS_ASYNC_FIFO_DUR);

        OS_REG_WRITE(ah, AR_TIME_OUT, AR_TIME_OUT_ACK_CTS_ASYNC_FIFO_DUR);
        OS_REG_WRITE(ah, AR_USEC, AR_USEC_ASYNC_FIFO_DUR);

        OS_REG_SET_BIT(ah, AR_MAC_PCU_LOGIC_ANALYZER, AR_MAC_PCU_LOGIC_ANALYZER_DISBUG20768);
        OS_REG_RMW_FIELD(ah, AR_AHB_MODE, AR_AHB_CUSTOM_BURST_EN, AR_AHB_CUSTOM_BURST_ASYNC_FIFO_VAL);
    }

#if ATH_SUPPORT_WAPI
    /*
     * Enable WAPI deaggregation and AR_PCU_MISC_MODE2_BC_MC_WAPI_MODE
     */
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        OS_REG_SET_BIT(ah, AR_MAC_PCU_LOGIC_ANALYZER, AR_MAC_PCU_LOGIC_WAPI_DEAGGR_ENABLE);
        if(AH_PRIVATE(ah)->ah_hal_keytype == HAL_CIPHER_WAPI) {
            OS_REG_SET_BIT(ah, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_BC_MC_WAPI_MODE);
        }
    }
#endif

    /* Keep the following piece of code separae from the above block to facilitate selective
     * turning-off through some registry setting or some thing like that */
    if (AR_SREV_KIWI_13_OR_LATER(ah)) {
        /* Enable AGGWEP to accelerate encryption engine */
        OS_REG_SET_BIT(ah, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_ENABLE_AGGWEP);
    }

    ap->ah_opmode = opmode;     /* record operating mode */

    OS_MARK(ah, AH_MARK_RESET_DONE, 0);

#ifdef AR5416_EMULATION
    /*
     * For mac to mac emulation, turn on rate throttling
     * but not half-rate (encryption)
     */
    if (AH5416(ah)->ah_hwp == HAL_MAC_TO_MAC_EMU) {
        OS_REG_WRITE(ah, AR_EMU, AR_EMU_RATETHROT);
    }
#endif

    /*
     * disable seq number generation in hw
     */
    OS_REG_WRITE(ah, AR_STA_ID1,
                 OS_REG_READ(ah, AR_STA_ID1) | AR_STA_ID1_PRESERVE_SEQNUM);

    ar5416SetDma(ah);

#if WLAN_SPECTRAL_ENABLE
    if (restoreSpectral ==  AH_TRUE) {
            HDPRINTF(ah, HAL_DBG_RESET, "%s %d: restore spectral config 0x%x\n",
                    __func__, __LINE__, cached_spectral_config);
            ar5416RestoreSpectralConfig(ah, cached_spectral_config);
            restoreSpectral = AH_FALSE;
    }
#endif

    /*
     * program OBS bus to see MAC interrupts
     */
    OS_REG_WRITE(ah, AR_OBS, 8);

#ifdef AR5416_EMULATION
    /*
     * configure MAC logic analyzer (emulation only)
     */
    if (IS_5416_EMU(ah)) {
        ar5416InitMacTrace(ah);
    }
#endif

    /*
     * Disable general interrupt mitigation by setting MIRT = 0x0
     * Rx and tx interrupt mitigation are conditionally enabled below.
     */
    OS_REG_WRITE(ah, AR_MIRT, 0);
    if (ahp->ah_intrMitigationRx) {
        /*
         * Enable Interrupt Mitigation for Rx.
         * If no build-specific limits for the rx interrupt mitigation
         * timer have been specified, use conservative defaults.
         */
        #ifndef AH_RIMT_LAST_MICROSEC
            #define AH_RIMT_LAST_MICROSEC 250
        #endif
        #ifndef AH_RIMT_FIRST_MICROSEC
            #define AH_RIMT_FIRST_MICROSEC 700
        #endif
        OS_REG_RMW_FIELD(ah, AR_RIMT, AR_RIMT_LAST, AH_RIMT_LAST_MICROSEC);
        OS_REG_RMW_FIELD(ah, AR_RIMT, AR_RIMT_FIRST, AH_RIMT_FIRST_MICROSEC);
    }
    if (ahp->ah_intrMitigationTx) {
        /*
         * Enable Interrupt Mitigation for Tx.
         * If no build-specific limits for the tx interrupt mitigation
         * timer have been specified, use the values preferred for
         * the carrier group's products.
         */
        #ifndef AH_TIMT_LAST_MICROSEC
            #define AH_TIMT_LAST_MICROSEC 300
        #endif
        #ifndef AH_TIMT_FIRST_MICROSEC
            #define AH_TIMT_FIRST_MICROSEC 750
        #endif
        OS_REG_RMW_FIELD(ah, AR_TIMT, AR_TIMT_LAST, AH_TIMT_LAST_MICROSEC);
        OS_REG_RMW_FIELD(ah, AR_TIMT, AR_TIMT_FIRST, AH_TIMT_FIRST_MICROSEC);
    }

    ar5416InitBB(ah, chan);

    if (!ar5416InitCal(ah, chan)) FAIL(HAL_ESELFTEST);

    ENABLE_REG_WRITE_BUFFER

    /*
     * WAR for owl 1.0 - restore chain mask for 2-chain cfgs after cal
     */
    rx_chainmask = ahp->ah_rxchainmask;
    if ((rx_chainmask == 0x5) || (rx_chainmask == 0x3)) {
        OS_REG_WRITE(ah, AR_PHY_RX_CHAINMASK,
            ap->green_ap_ps_on ? HAL_GREEN_AP_RX_MASK : rx_chainmask);
        OS_REG_WRITE(ah, AR_PHY_CAL_CHAINMASK, rx_chainmask);
    }

    if (AR_SREV_HOWL(ah)) {
        OS_REG_WRITE(ah, AR_CFG_LED,
            (AR_CFG_LED_ASSOC_ACTIVE << AR_CFG_LED_ASSOC_CTL_S) |
            AR_CFG_SCLK_32KHZ);
    } else {
        /* Restore previous led state */
        OS_REG_WRITE(ah, AR_CFG_LED, saveLedState | AR_CFG_SCLK_32KHZ);
    }

    OS_REG_WRITE_FLUSH(ah);
    DISABLE_REG_WRITE_BUFFER

#ifdef ATH_BT_COEX
    if (ahp->ah_btCoexConfigType != HAL_BT_COEX_CFG_NONE) {
        ar5416InitBTCoex(ah);
    }
#endif

    /* Start TSF2 for generic timer 8-15. (Kiwi)*/
    if (AR_SREV_KIWI(ah)) {
        ar5416StartTsf2(ah);
    }

    /* MIMO Power save setting */
    if ((ar5416GetCapability(ah, HAL_CAP_DYNAMIC_SMPS, 0, AH_NULL) == HAL_OK)) {
        ar5416SetSmPowerMode(ah, ahp->ah_smPowerMode);
    }

    /*
     * For big endian systems turn on swapping for descriptors
     */
    if (AR_SREV_HOWL(ah)) {
        u_int32_t mask;
        mask = OS_REG_READ(ah, AR_CFG);
        if (mask & (AR_CFG_SWRB | AR_CFG_SWTB | AR_CFG_SWRG)) {
            HDPRINTF(ah, HAL_DBG_RESET, "%s CFG Byte Swap Set 0x%x\n",
                __func__, mask);
        } else {
            mask = INIT_CONFIG_STATUS | AR_CFG_SWRB | AR_CFG_SWTB;
            OS_REG_WRITE(ah, AR_CFG, mask);
            HDPRINTF(ah, HAL_DBG_RESET, "%s Setting CFG 0x%x\n",
                __func__, OS_REG_READ(ah, AR_CFG));
        }
    } else {

#if AH_BYTE_ORDER == AH_BIG_ENDIAN
			ar5416_init_cfg_reg(ah);
#endif
    }

    if (AR_SREV_HOWL(ah)) {
        /*
         * Enable the MBSSID block-ack fix for HOWL.
         * This feature is only supported on Howl 1.4, but it is safe to
         * set bit 22 of STA_ID1 on other Howl revisions (1.1, 1.2, 1.3),
         * since bit 22 is unused in those Howl revisions.
         */
        unsigned int reg;
        reg = (OS_REG_READ(ah, AR_STA_ID1) | (1<<22));
        OS_REG_WRITE(ah,AR_STA_ID1, reg);
        ath_hal_printf(ah,"MBSSID Set bit 22 of AR_STA_ID 0x%x\n", reg);
    }

	/* Fix for EVID 108445 - Continuous beacon stuck */
	/* set AR_PCU_MISC_MODE2 bit 7 CFP_IGNORE to 1 */
	OS_REG_WRITE(ah, AR_PCU_MISC_MODE2,
				 (OS_REG_READ(ah, AR_PCU_MISC_MODE2)|0x80));

    if (status) *status = HAL_FULL_RESET;

    chan->channel_flags = ichan->channel_flags;
    chan->priv_flags = ichan->priv_flags;
    ap->ah_curchan->ah_channel_time=0;
    ap->ah_curchan->ah_tsf_last = ar5416GetTsf64(ah);
    return AH_TRUE;
bad:
    OS_MARK(ah, AH_MARK_RESET_DONE, ecode);
    if (status) *status = ecode;
    return AH_FALSE;
#undef FAIL
}

/**************************************************************
 * ar5416ChannelChange
 * Assumes caller wants to change channel, and not reset.
 */
static inline HAL_BOOL
ar5416ChannelChange(struct ath_hal *ah, HAL_CHANNEL *chan,
                    HAL_CHANNEL_INTERNAL *ichan, HAL_HT_MACMODE macmode)
{
    u_int32_t synthDelay, qnum;
    struct ath_hal_5416 *ahp = AH5416(ah);

    /* TX must be stopped by now */
    for (qnum = 0; qnum < AR_NUM_QCU; qnum++) {
        if (ar5416NumTxPending(ah, qnum)) {
            HDPRINTF(ah, HAL_DBG_QUEUE, "%s: Transmit frames pending on queue %d\n", __func__, qnum);
            HALASSERT(0);
            return AH_FALSE;
        }
    }

    /* Kill last Baseband Rx Frame - Request analog bus grant */
    OS_REG_WRITE(ah, AR_PHY_RFBUS_REQ, AR_PHY_RFBUS_REQ_EN);
    if (!ath_hal_wait(ah, AR_PHY_RFBUS_GRANT, AR_PHY_RFBUS_GRANT_EN,
                          AR_PHY_RFBUS_GRANT_EN, AH_WAIT_TIMEOUT)) {
        HDPRINTF(ah, HAL_DBG_PHY_IO, "%s: Could not kill baseband RX\n", __func__);
        return AH_FALSE;
    }

    /* Setup 11n MAC/Phy mode registers */
    ar5416Set11nRegs(ah, chan, macmode);

    /*
     * Change the synth
     */
    if (!ahp->ah_rfHal.setChannel(ah, ichan)) {
        HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: failed to set channel\n", __func__);
        return AH_FALSE;
    }

    /*
     * Setup the transmit power values.
     *
     * After the public to private hal channel mapping, ichan contains the
     * valid regulatory power value.
     * ath_hal_getctl and ath_hal_getantennaallowed look up ichan from chan.
     */
    if (ar5416EepromSetTransmitPower(ah, &ahp->ah_eeprom, ichan,
         ath_hal_getctl(ah,chan), ath_hal_getantennaallowed(ah, chan),
         ichan->max_reg_tx_power * 2,
         AH_MIN(MAX_RATE_POWER, AH_PRIVATE(ah)->ah_power_limit)) != HAL_OK) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: error init'ing transmit power\n", __func__);
        return AH_FALSE;
    }

    /*
     * Wait for the frequency synth to settle (synth goes on via PHY_ACTIVE_EN).
     * Read the phy active delay register. Value is in 100ns increments.
     */
    synthDelay = OS_REG_READ(ah, AR_PHY_RX_DELAY) & AR_PHY_RX_DELAY_DELAY;
    if (IS_CHAN_CCK(chan)) {
        synthDelay = (4 * synthDelay) / 22;
    } else {
        synthDelay /= 10;
    }

    OS_DELAY(synthDelay + BASE_ACTIVATE_DELAY);

    /*
     * Release the RFBus Grant.
     */
     OS_REG_WRITE(ah, AR_PHY_RFBUS_REQ, 0);

    /*
     * Calibrations need to be triggered after RFBus Grant is released.
     * Otherwise, cals may not be able to complete.
     */
    if (!ichan->one_time_cals_done) {
        /*
         * Start offset and carrier leak cals
         */
    }

    /*
     * Write spur immunity and delta slope for OFDM enabled modes (A, G, Turbo)
     */
    if (IS_CHAN_OFDM(chan)|| IS_CHAN_HT(chan)) {
        ar5416SetDeltaSlope(ah, ichan);
    }

    /*
     * For Merlin, spur can break CCK MRC algorithm. SpurMitigate needs to
     * be called in all 11A/B/G/HT modes to disable CCK MRC if spur is found
     * in this channel.
     */
    if (AR_SREV_MERLIN_10_OR_LATER(ah))
        ar9280SpurMitigate(ah, chan, ichan);
    else
        ar5416SpurMitigate(ah, chan);

    if (!ichan->one_time_cals_done) {
        /*
         * wait for end of offset and carrier leak cals
         */
        ichan->one_time_cals_done = AH_TRUE;
    }

#if 0
    if (chan->channel_flags & CHANNEL_108G)
        ar5416ArEnable(ah);
    else if ((chan->channel_flags &
             (CHANNEL_A|CHANNEL_ST|CHANNEL_A_HT20| CHANNEL_A_HT40))
             && (chan->priv_flags & CHANNEL_DFS))
        ar5416RadarEnable(ah);
#endif

    return AH_TRUE;
}

static void
ar5416Set11nRegs(struct ath_hal *ah, HAL_CHANNEL *chan, HAL_HT_MACMODE macmode)
{
    u_int32_t phymode;
    u_int32_t enableDacFifo = 0;
    struct ath_hal_5416 *ahp = AH5416(ah);

    if (AR_SREV_KITE_10_OR_LATER(ah)) {
        enableDacFifo = (OS_REG_READ(ah, AR_PHY_TURBO) & AR_PHY_FC_ENABLE_DAC_FIFO);
    }

    /* Enable 11n HT, 20 MHz */
    phymode = AR_PHY_FC_HT_EN | AR_PHY_FC_SHORT_GI_40
              | AR_PHY_FC_SINGLE_HT_LTF1 | AR_PHY_FC_WALSH | enableDacFifo;

    /* Configure baseband for dynamic 20/40 operation */
    if (IS_CHAN_HT40(chan)) {
        phymode |= AR_PHY_FC_DYN2040_EN;

        /* Configure control (primary) channel at +-10MHz */
        if (chan->channel_flags & CHANNEL_HT40PLUS) {
            phymode |= AR_PHY_FC_DYN2040_PRI_CH;
        }

        /* Configure 20/25 spacing */
        if (ahp->ah_extprotspacing == HAL_HT_EXTPROTSPACING_25) {
            phymode |= AR_PHY_FC_DYN2040_EXT_CH;
        }
    }
    OS_REG_WRITE(ah, AR_PHY_TURBO, phymode);

    /* Configure MAC for 20/40 operation */
    ar5416Set11nMac2040(ah, macmode);

    ENABLE_REG_WRITE_BUFFER;

    /* global transmit timeout (25 TUs default)*/
    /* XXX - put this elsewhere??? */
    OS_REG_WRITE(ah, AR_GTXTO, 25 << AR_GTXTO_TIMEOUT_LIMIT_S) ;

    /* carrier sense timeout */
    OS_REG_WRITE(ah, AR_CST, 0xF << AR_CST_TIMEOUT_LIMIT_S);

    OS_REG_WRITE_FLUSH(ah);

	DISABLE_REG_WRITE_BUFFER
}

void
ar5416SetOperatingMode(struct ath_hal *ah, int opmode)
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
ar5416PhyDisable(struct ath_hal *ah)
{
    if (!ar5416SetResetReg(ah, HAL_RESET_WARM)) {
        return AH_FALSE;
    }
    ar5416InitPLL(ah, AH_NULL);
    return AH_TRUE;
}

/*
 * Places all of hardware into reset
 */
HAL_BOOL
ar5416Disable(struct ath_hal *ah)
{
    if (!ar5416SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE))
        return AH_FALSE;

    if (!ar5416SetResetReg(ah, HAL_RESET_COLD)) {
        return AH_FALSE;
    }
    ar5416InitPLL(ah, AH_NULL);
    return AH_TRUE;
}

/*
 * Places the hardware into reset and then pulls it out of reset
 */
HAL_BOOL
ar5416ChipReset(struct ath_hal *ah, HAL_CHANNEL *chan)
{

        struct ath_hal_5416     *ahp = AH5416(ah);
        OS_MARK(ah, AH_MARK_CHIPRESET, chan ? chan->channel : 0);

        /*
         * Warm reset is optimistic.
         */

        /*
         * Merlin with open loop power control needs RTC reset.
         */
        if (AR_SREV_MERLIN(ah) && ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
            if (!ar5416SetResetReg(ah, HAL_RESET_POWER_ON))
                return AH_FALSE;
        } else {
        if (!ar5416SetResetReg(ah, HAL_RESET_WARM))
                return AH_FALSE;
        }

        /* Bring out of sleep mode (AGAIN) */
        if (!ar5416SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE))
                return AH_FALSE;

        ahp->ah_chipFullSleep = AH_FALSE;

        ar5416InitPLL(ah, chan);

        /*
         * Perform warm reset before the mode/PLL/turbo registers
         * are changed in order to deactivate the radio.  Mode changes
         * with an active radio can result in corrupted shifts to the
         * radio device.
         */
        ar5416SetRfMode(ah, chan);

        return AH_TRUE;
}

/* ar5416SetupCalibration
 * Setup HW to collect samples used for current cal
 */
void
ar5416SetupCalibration(struct ath_hal *ah, HAL_CAL_LIST *currCal)
{
    /* Start calibration w/ 2^(INIT_IQCAL_LOG_COUNT_MAX+1) samples */
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING_CTRL4(0),
                     AR_PHY_TIMING_CTRL4_IQCAL_LOG_COUNT_MAX,
                     currCal->calData->calCountMax);

    /* Select calibration to run */
    switch(currCal->calData->calType) {
    case IQ_MISMATCH_CAL:
        OS_REG_WRITE(ah, AR_PHY_CALMODE, AR_PHY_CALMODE_IQ);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: starting IQ Mismatch Calibration\n", __func__);
        break;
    case ADC_GAIN_CAL:
        OS_REG_WRITE(ah, AR_PHY_CALMODE, AR_PHY_CALMODE_ADC_GAIN);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: starting ADC Gain Calibration\n", __func__);
        break;
    case ADC_DC_CAL:
        OS_REG_WRITE(ah, AR_PHY_CALMODE, AR_PHY_CALMODE_ADC_DC_PER);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: starting ADC DC Calibration\n", __func__);
        break;
    case ADC_DC_INIT_CAL:
        OS_REG_WRITE(ah, AR_PHY_CALMODE, AR_PHY_CALMODE_ADC_DC_INIT);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: starting Init ADC DC Calibration\n", __func__);
        break;
    }

    /* Kick-off cal */
    OS_REG_SET_BIT(ah, AR_PHY_TIMING_CTRL4(0), AR_PHY_TIMING_CTRL4_DO_CAL);
}

/* ar5416ResetCalibration
 * Initialize shared data structures and prepare a cal to be run.
 */
void
ar5416ResetCalibration(struct ath_hal *ah, HAL_CAL_LIST *currCal)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int i;

    /* Setup HW for new calibration */
    ar5416SetupCalibration(ah, currCal);

    /* Change SW state to RUNNING for this calibration */
    currCal->calState = CAL_RUNNING;

    /* Reset data structures shared between different calibrations */
    for(i = 0; i < AR5416_MAX_CHAINS; i++) {
        ahp->ah_Meas0.sign[i] = 0;
        ahp->ah_Meas1.sign[i] = 0;
        ahp->ah_Meas2.sign[i] = 0;
        ahp->ah_Meas3.sign[i] = 0;
    }

    ahp->ah_CalSamples = 0;
}

/* ar5416Calibration
 * Wrapper for a more generic Calibration routine. Primarily to abstract to
 * upper layers whether there is 1 or more calibrations to be run.
 */
HAL_BOOL
ar5416Calibration(struct ath_hal *ah,  HAL_CHANNEL *chan, u_int8_t rxchainmask,
                  HAL_BOOL do_nf_cal, HAL_BOOL *isCalDone, int is_scan, u_int32_t *sched_cals)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    HAL_CAL_LIST *currCal = ahp->ah_cal_list_curr;
    HAL_CHANNEL_INTERNAL *ichan = ath_hal_checkchannel(ah, chan);

    *isCalDone = AH_TRUE;

    /* Invalid channel check */
    if (ichan == AH_NULL) {
        HDPRINTF(ah, HAL_DBG_CHANNEL,
                 "%s: invalid channel %u/0x%x; no mapping\n",
                 __func__, chan->channel, chan->channel_flags);
        return AH_FALSE;
    }

    OS_MARK(ah, AH_MARK_PERCAL, chan->channel);

    /* For given calibration:
     * 1. Call generic cal routine
     * 2. When this cal is done (isCalDone) if we have more cals waiting
     *    (eg after reset), mask this to upper layers by not propagating
     *    isCalDone if it is set to TRUE.
     *    Instead, change isCalDone to FALSE and setup the waiting cal(s)
     *    to be run.
     */
    if (currCal &&
        (currCal->calState == CAL_RUNNING ||
         currCal->calState == CAL_WAITING)) {
        ar5416PerCalibration(ah, ichan, rxchainmask, currCal, isCalDone);

        if (*isCalDone == AH_TRUE) {
            ahp->ah_cal_list_curr = currCal = currCal->calNext;

            if (currCal && currCal->calState == CAL_WAITING) {
                *isCalDone = AH_FALSE;
                ar5416ResetCalibration(ah, currCal);
            } else {
                *sched_cals = 0;
			}
        }
    }

    /* Do NF cal only at longer intervals */
    if (do_nf_cal) {
        int nf_done;
        /* Do periodic PAOffset Cal */
        if (AR_SREV_K2(ah)) {
#ifdef AH_SUPPORT_K2
            /* check, if we can skip PA offset CAL */
            if (!(AH_PRIVATE(ah)->ah_paCalInfo.skipCount)) {
                ar9271PACal(ah, AH_FALSE);
                if ((AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable)) {
                    AH_PRIVATE(ah)->green_tx_status = HAL_RSSI_TX_POWER_NONE;
                }
            } else {
                AH_PRIVATE(ah)->ah_paCalInfo.skipCount--;
            }
#endif /* AH_SUPPORT_K2 */
        }
        else if (AR_SREV_KITE(ah) && AR_SREV_KITE_11_OR_LATER(ah)) {
#ifdef AH_SUPPORT_KITE_ANY
            if (!(AH_PRIVATE(ah)->ah_config.ath_hal_disPeriodicPACal)) {
                /* check, if we can skip PA offset CAL */
                if (!(AH_PRIVATE(ah)->ah_paCalInfo.skipCount)) {
                    ar9285PACal(ah, AH_FALSE);
                    if ((AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable)) {
                        AH_PRIVATE(ah)->green_tx_status = HAL_RSSI_TX_POWER_NONE;
                    }
                } else {
                    AH_PRIVATE(ah)->ah_paCalInfo.skipCount--;
                }
            }
#endif /* AH_SUPPORT_KITE_ANY */
        }

        /* Get the value from the previous NF cal and update history buffer */
        nf_done = ar5416StoreNewNf(ah, ichan, is_scan);
        if (ichan->channel_flags & CHANNEL_CW_INT) {
             chan->channel_flags |= CHANNEL_CW_INT;
        }
        ichan->channel_flags &= (~CHANNEL_CW_INT);
        
        if (nf_done) {
            /*
             * Load the NF from history buffer of the current channel.
             * NF is slow time-variant, so it is OK to use a historical value.
             */
            ar5416LoadNF(ah, AH_PRIVATE(ah)->ah_curchan, is_scan);

            /* start NF calibration, without updating BB NF register*/
            ar5416StartNFCal(ah);
        }
    }

    return AH_TRUE;
}

/* ar5416IQCalCollect
 * Collect data from HW to later perform IQ Mismatch Calibration
 */
void
ar5416IQCalCollect(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int i;

    /*
     * Accumulate IQ cal measures for active chains
     */
    for (i = 0; i < AR5416_MAX_CHAINS; i++) {
        ahp->ah_totalPowerMeasI[i] += OS_REG_READ(ah, AR_PHY_CAL_MEAS_0(i));
        ahp->ah_totalPowerMeasQ[i] += OS_REG_READ(ah, AR_PHY_CAL_MEAS_1(i));
        ahp->ah_totalIqCorrMeas[i] += (int32_t)OS_REG_READ(ah,
                                               AR_PHY_CAL_MEAS_2(i));
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%d: Chn %d pmi=0x%08x;pmq=0x%08x;iqcm=0x%08x;\n",
                 ahp->ah_CalSamples, i, ahp->ah_totalPowerMeasI[i],
                 ahp->ah_totalPowerMeasQ[i], ahp->ah_totalIqCorrMeas[i]);
    }
}

/* ar5416AdcGainCalCollect
 * Collect data from HW to later perform ADC Gain Calibration
 */
void
ar5416AdcGainCalCollect(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int i;

    /*
     * Accumulate ADC Gain cal measures for active chains
     */
    for (i = 0; i < AR5416_MAX_CHAINS; i++) {
        ahp->ah_totalAdcIOddPhase[i]  += OS_REG_READ(ah, AR_PHY_CAL_MEAS_0(i));
        ahp->ah_totalAdcIEvenPhase[i] += OS_REG_READ(ah, AR_PHY_CAL_MEAS_1(i));
        ahp->ah_totalAdcQOddPhase[i]  += OS_REG_READ(ah, AR_PHY_CAL_MEAS_2(i));
        ahp->ah_totalAdcQEvenPhase[i] += OS_REG_READ(ah, AR_PHY_CAL_MEAS_3(i));

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
           "%d: Chn %d oddi=0x%08x; eveni=0x%08x; oddq=0x%08x; evenq=0x%08x;\n",
           ahp->ah_CalSamples, i, ahp->ah_totalAdcIOddPhase[i],
           ahp->ah_totalAdcIEvenPhase[i], ahp->ah_totalAdcQOddPhase[i],
           ahp->ah_totalAdcQEvenPhase[i]);
    }
}

void
ar5416AdcDcCalCollect(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int i;

    for (i = 0; i < AR5416_MAX_CHAINS; i++) {
        ahp->ah_totalAdcDcOffsetIOddPhase[i] +=
                               (int32_t) OS_REG_READ(ah, AR_PHY_CAL_MEAS_0(i));
        ahp->ah_totalAdcDcOffsetIEvenPhase[i] +=
                               (int32_t) OS_REG_READ(ah, AR_PHY_CAL_MEAS_1(i));
        ahp->ah_totalAdcDcOffsetQOddPhase[i] +=
                               (int32_t) OS_REG_READ(ah, AR_PHY_CAL_MEAS_2(i));
        ahp->ah_totalAdcDcOffsetQEvenPhase[i] +=
                               (int32_t) OS_REG_READ(ah, AR_PHY_CAL_MEAS_3(i));

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
           "%d: Chn %d oddi=0x%08x; eveni=0x%08x; oddq=0x%08x; evenq=0x%08x;\n",
           ahp->ah_CalSamples, i,
           ahp->ah_totalAdcDcOffsetIOddPhase[i],
           ahp->ah_totalAdcDcOffsetIEvenPhase[i],
           ahp->ah_totalAdcDcOffsetQOddPhase[i],
           ahp->ah_totalAdcDcOffsetQEvenPhase[i]);
    }
}

/* ar5416IQCalibration
 * Use HW data to perform IQ Mismatch Calibration
 */
void
ar5416IQCalibration(struct ath_hal *ah, u_int8_t numChains)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t powerMeasQ, powerMeasI, iqCorrMeas;
    u_int32_t qCoffDenom, iCoffDenom;
    int32_t qCoff, iCoff;
    int iqCorrNeg, i;

    for (i = 0; i < numChains; i++) {
        powerMeasI = ahp->ah_totalPowerMeasI[i];
        powerMeasQ = ahp->ah_totalPowerMeasQ[i];
        iqCorrMeas = ahp->ah_totalIqCorrMeas[i];

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "Starting IQ Cal and Correction for Chain %d\n", i);

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "Orignal: Chn %diq_corr_meas = 0x%08x\n",
                 i, ahp->ah_totalIqCorrMeas[i]);

        iqCorrNeg = 0;

        /* iqCorrMeas is always negative. */
        if (iqCorrMeas > 0x80000000)  {
            iqCorrMeas = (0xffffffff - iqCorrMeas) + 1;
            iqCorrNeg = 1;
        }

        HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d pwr_meas_i = 0x%08x\n",
                 i, powerMeasI);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d pwr_meas_q = 0x%08x\n",
                 i, powerMeasQ);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "iqCorrNeg is 0x%08x\n", iqCorrNeg);

        iCoffDenom = (powerMeasI/2 + powerMeasQ/2)/ 128;
        qCoffDenom = powerMeasQ / 64;
        /* Protect against divide-by-0 */
        if ((powerMeasQ != 0) && (iCoffDenom != 0) && (qCoffDenom != 0)) {
            /* IQ corr_meas is already negated if iqcorr_neg == 1 */
            iCoff = iqCorrMeas/iCoffDenom;
            qCoff = powerMeasI/qCoffDenom - 64;
            HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d iCoff = 0x%08x\n",
                     i, iCoff);
            HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d qCoff = 0x%08x\n",
                     i, qCoff);

            /* Negate iCoff if iqCorrNeg == 0 */
            iCoff = iCoff & 0x3f;
            HDPRINTF(ah, HAL_DBG_CALIBRATE, "New: Chn %d iCoff = 0x%08x\n", i,
                     iCoff);
            if (iqCorrNeg == 0x0) {
                iCoff = 0x40 - iCoff;
            }

            if (qCoff > 15) {
                qCoff = 15;
            } else if (qCoff <= -16) {
                qCoff = -16;
            }

            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                     "Chn %d : iCoff = 0x%x  qCoff = 0x%x\n", i, iCoff, qCoff);

            OS_REG_RMW_FIELD(ah, AR_PHY_TIMING_CTRL4(i),
                             AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF, iCoff);
            OS_REG_RMW_FIELD(ah, AR_PHY_TIMING_CTRL4(i),
                             AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF, qCoff);
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                     "IQ Cal and Correction done for Chain %d\n", i);
        }
    }

    OS_REG_SET_BIT(ah, AR_PHY_TIMING_CTRL4(0),
                   AR_PHY_TIMING_CTRL4_IQCORR_ENABLE);
}

/* ar5416AdcGainCalibration
 * Use HW data to perform ADC Gain Calibration
 */
void
ar5416AdcGainCalibration(struct ath_hal *ah, u_int8_t numChains)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t iOddMeasOffset, iEvenMeasOffset, qOddMeasOffset, qEvenMeasOffset;
    u_int32_t qGainMismatch, iGainMismatch, val, i;

    for (i = 0; i < numChains; i++) {
        iOddMeasOffset  = ahp->ah_totalAdcIOddPhase[i];
        iEvenMeasOffset = ahp->ah_totalAdcIEvenPhase[i];
        qOddMeasOffset  = ahp->ah_totalAdcQOddPhase[i];
        qEvenMeasOffset = ahp->ah_totalAdcQEvenPhase[i];

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "Starting ADC Gain Cal for Chain %d\n", i);

        HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d pwr_meas_odd_i = 0x%08x\n",
                 i, iOddMeasOffset);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d pwr_meas_even_i = 0x%08x\n",
                 i, iEvenMeasOffset);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d pwr_meas_odd_q = 0x%08x\n",
                 i, qOddMeasOffset);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d pwr_meas_even_q = 0x%08x\n",
                 i, qEvenMeasOffset);

        if (iOddMeasOffset != 0 && qEvenMeasOffset != 0) {
            iGainMismatch = ((iEvenMeasOffset*32)/iOddMeasOffset) & 0x3f;
            qGainMismatch = ((qOddMeasOffset*32)/qEvenMeasOffset) & 0x3f;

            HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d gain_mismatch_i = 0x%08x\n",
                     i, iGainMismatch);
            HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d gain_mismatch_q = 0x%08x\n",
                     i, qGainMismatch);

            val = OS_REG_READ(ah, AR_PHY_NEW_ADC_DC_GAIN_CORR(i));
            val &= 0xfffff000;
            val |= (qGainMismatch) | (iGainMismatch << 6);
            OS_REG_WRITE(ah, AR_PHY_NEW_ADC_DC_GAIN_CORR(i), val);

            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                     "ADC Gain Cal done for Chain %d\n", i);
        }
    }

    OS_REG_WRITE(ah, AR_PHY_NEW_ADC_DC_GAIN_CORR(0),
                 OS_REG_READ(ah, AR_PHY_NEW_ADC_DC_GAIN_CORR(0)) |
                 AR_PHY_NEW_ADC_GAIN_CORR_ENABLE);
}

void
ar5416AdcDcCalibration(struct ath_hal *ah, u_int8_t numChains)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t iOddMeasOffset, iEvenMeasOffset, val, i;
    int32_t   qOddMeasOffset, qEvenMeasOffset, qDcMismatch, iDcMismatch;
    const HAL_PERCAL_DATA *calData = ahp->ah_cal_list_curr->calData;
    u_int32_t numSamples = (1 << (calData->calCountMax + 5)) *
                            calData->calNumSamples;

    for (i = 0; i < numChains; i++) {
        iOddMeasOffset  = ahp->ah_totalAdcDcOffsetIOddPhase[i];
        iEvenMeasOffset = ahp->ah_totalAdcDcOffsetIEvenPhase[i];
        qOddMeasOffset  = ahp->ah_totalAdcDcOffsetQOddPhase[i];
        qEvenMeasOffset = ahp->ah_totalAdcDcOffsetQEvenPhase[i];

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "Starting ADC DC Offset Cal for Chain %d\n", i);

        HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d pwr_meas_odd_i = %d\n",
                 i, iOddMeasOffset);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d pwr_meas_even_i = %d\n",
                 i, iEvenMeasOffset);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d pwr_meas_odd_q = %d\n",
                 i, qOddMeasOffset);
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "Chn %d pwr_meas_even_q = %d\n",
                 i, qEvenMeasOffset);

        HALASSERT(numSamples);

        iDcMismatch = (((iEvenMeasOffset - iOddMeasOffset) * 2) /
                        numSamples) & 0x1ff;
        qDcMismatch = (((qOddMeasOffset - qEvenMeasOffset) * 2) /
                        numSamples) & 0x1ff;

        HDPRINTF(ah, HAL_DBG_CALIBRATE,"Chn %d dc_offset_mismatch_i = 0x%08x\n",
                 i, iDcMismatch);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,"Chn %d dc_offset_mismatch_q = 0x%08x\n",
                 i, qDcMismatch);

        val = OS_REG_READ(ah, AR_PHY_NEW_ADC_DC_GAIN_CORR(i));
        val &= 0xc0000fff;
        val |= (qDcMismatch << 12) |
               (iDcMismatch << 21);
        OS_REG_WRITE(ah, AR_PHY_NEW_ADC_DC_GAIN_CORR(i), val);

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "ADC DC Offset Cal done for Chain %d\n", i);
    }

    OS_REG_WRITE(ah, AR_PHY_NEW_ADC_DC_GAIN_CORR(0),
                 OS_REG_READ(ah, AR_PHY_NEW_ADC_DC_GAIN_CORR(0)) |
                 AR_PHY_NEW_ADC_DC_OFFSET_CORR_ENABLE);
}

/* ar5416PerCalibration
 * Generic calibration routine.
 * Recalibrate the lower PHY chips to account for temperature/environment
 * changes.
 */
void
ar5416PerCalibration(struct ath_hal *ah,  HAL_CHANNEL_INTERNAL *ichan,
                     u_int8_t rxchainmask, HAL_CAL_LIST *currCal,
                     HAL_BOOL *isCalDone)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    /* Cal is assumed not done until explicitly set below */
    *isCalDone = AH_FALSE;

    /* Calibration in progress. */
    if (currCal->calState == CAL_RUNNING) {
        /* Check to see if it has finished. */
        if (!(OS_REG_READ(ah,
            AR_PHY_TIMING_CTRL4(0)) & AR_PHY_TIMING_CTRL4_DO_CAL)) {

            /*
             * Accumulate cal measures for active chains
             */
            currCal->calData->calCollect(ah);

            ahp->ah_CalSamples++;

            if (ahp->ah_CalSamples >= currCal->calData->calNumSamples) {
                int i, numChains = 0;
                for (i = 0; i < AR5416_MAX_CHAINS; i++) {
                    if (rxchainmask & (1 << i)) {
                        numChains++;
                    }
                }

                /*
                 * Process accumulated data
                 */
                currCal->calData->calPostProc(ah, numChains);

                /* Calibration has finished. */
                ichan->cal_valid |= currCal->calData->calType;
                currCal->calState = CAL_DONE;
                *isCalDone = AH_TRUE;

           } else {
                /* Set-up collection of another sub-sample until we
                 * get desired number
                 */
                ar5416SetupCalibration(ah, currCal);
            }
        }
    } else if (!(ichan->cal_valid & currCal->calData->calType)) {
        /* If current cal is marked invalid in channel, kick it off */
        ar5416ResetCalibration(ah, currCal);
    }
}

void ar5416GreenApPsOnOff( struct ath_hal *ah, u_int16_t onOff)
{
    /* Set/reset the ps flag */
    AH_PRIVATE(ah)->green_ap_ps_on = !!onOff;
}

/* This function returns 1 for Merlin and OWL, where it is possible to do
 * single-chain power save. Since kite has only single chain, it returns 0
 */
u_int16_t ar5416IsSingleAntPowerSavePossible(struct ath_hal *ah)
{
    return(
           AR_SREV_OWL(ah)    || 
           AR_SREV_SOWL (ah)  || 
           AR_SREV_MERLIN(ah) ||
           AR_SREV_KIWI(ah)
          );   
}

/* Find out which of the RX chains are enabled */
static u_int32_t
ar5416GetRxChainMask( struct ath_hal *ah)
{
    /* Read the AR_PHY_RX_CHAINMASK register */
    u_int32_t retVal = OS_REG_READ(ah, AR_PHY_RX_CHAINMASK);
    /* The bits [2:0] indicate the rx chain mask and are to be
     * interpreted as follows:
     * 00x => Only chain 0 is enabled
     * 01x => Chain 1 and 0 enabled
     * 1xx => Chain 2,1 and 0 enabled
     */
    return (retVal & 0x7);
}

/*
 * Write the given reset bit mask into the reset register
 */
HAL_BOOL
ar5416SetResetReg(struct ath_hal *ah, u_int32_t type)
{
    /*
     * Set force wake
     */
    OS_REG_WRITE(ah, AR_RTC_FORCE_WAKE,
             AR_RTC_FORCE_WAKE_EN | AR_RTC_FORCE_WAKE_ON_INT);

    switch (type) {
    case HAL_RESET_POWER_ON:
        return ar5416SetResetPowerOn(ah);
        break;
    case HAL_RESET_WARM:
    case HAL_RESET_COLD:
        return ar5416SetReset(ah, type);
        break;
    default:
        return AH_FALSE;
    }
}

static inline HAL_BOOL
ar5416SetResetPowerOn(struct ath_hal *ah)
{
	ENABLE_REG_WRITE_BUFFER

    /* Force wake */
    OS_REG_WRITE(ah, AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN |
                                     AR_RTC_FORCE_WAKE_ON_INT);
    /*
     * PowerOn reset can be used in open loop power control or failure recovery.
     * If we do RTC reset while DMA is still running, hardware may corrupt memory.
     * Therefore, we need to reset AHB first to stop DMA. Please see EV# 57500.
     */
    if (!AR_SREV_HOWL(ah))
        OS_REG_WRITE(ah, AR_RC, AR_RC_AHB);

    /*
     * RTC reset and clear
     */
    OS_REG_WRITE(ah, AR_RTC_RESET, 0);

    OS_REG_WRITE_FLUSH(ah);
	DISABLE_REG_WRITE_BUFFER

    OS_DELAY(2);

    if (!AR_SREV_HOWL(ah)) {
        OS_REG_WRITE(ah, AR_RC, 0);
    }

    OS_REG_WRITE(ah, AR_RTC_RESET, 1);

    /*
     * Poll till RTC is ON
     */
    if (!ath_hal_wait(ah, AR_RTC_STATUS, AR_RTC_STATUS_M, AR_RTC_STATUS_ON,
        AH_WAIT_TIMEOUT)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: RTC not waking up\n", __FUNCTION__);
        return AH_FALSE;
    }

    /* 
     * Read Revisions from Chip right after RTC is on for the first time.
     * This helps us detect the chip type early and initialize it accordingly.
     */
    ar5416ReadRevisions(ah);

    if (AR_SREV_HOWL(ah)) {
        /* Howl needs additional AHB-WMAC interface reset in this case */
        return ar5416SetReset(ah, HAL_RESET_COLD);
    } else {
        /*
         * Warm reset if we aren't really powering on,
         * just restarting the driver.
         */
        return ar5416SetReset(ah, HAL_RESET_WARM);
    }
}

static HAL_BOOL
ar5416SetReset(struct ath_hal *ah, int type)
{
    u_int32_t rst_flags;
    u_int32_t tmpReg;

    HALASSERT(type == HAL_RESET_WARM || type == HAL_RESET_COLD);

#ifdef AR9100
    if (AR_SREV_HOWL(ah)) {
        u_int32_t val = OS_REG_READ(ah, AR_RTC_DERIVED_CLK);
        val &= ~AR_RTC_DERIVED_CLK_PERIOD;
        val |= SM(1, AR_RTC_DERIVED_CLK_PERIOD);
        OS_REG_WRITE(ah, AR_RTC_DERIVED_CLK, val);
        (void)OS_REG_READ(ah, AR_RTC_DERIVED_CLK);
    }
#endif

	ENABLE_REG_WRITE_BUFFER

    /*
     * RTC Force wake should be done before resetting the MAC.
     * MDK/ART does it that way.
     */
    OS_REG_WRITE(ah, AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN |
                                     AR_RTC_FORCE_WAKE_ON_INT);

#ifdef AR9100
    if (AR_SREV_HOWL(ah)) {
        /*
         * Howl has a reset module and without resetting that,
         * the AHB - WMAC module can't be reset. Hence, the
         * additional flags to reset the Reset control module
         * when doing a MAC reset (this is inline with MDK/ART
         * testing).
         */
        rst_flags = AR_RTC_RC_MAC_WARM | AR_RTC_RC_MAC_COLD |
                    AR_RTC_RC_COLD_RESET | AR_RTC_RC_WARM_RESET;
    } else {
#endif

        /* Reset AHB */
        /* Bug26871 */
        tmpReg = OS_REG_READ(ah, AR_INTR_SYNC_CAUSE);
        if (tmpReg & (AR_INTR_SYNC_LOCAL_TIMEOUT|AR_INTR_SYNC_RADM_CPL_TIMEOUT)) {
            OS_REG_WRITE(ah, AR_INTR_SYNC_ENABLE, 0);
            OS_REG_WRITE(ah, AR_RC, AR_RC_AHB|AR_RC_HOSTIF);
        }
        else {
            OS_REG_WRITE(ah, AR_RC, AR_RC_AHB);
        }

        rst_flags = AR_RTC_RC_MAC_WARM;
        if (type == HAL_RESET_COLD) {
            rst_flags |= AR_RTC_RC_MAC_COLD;
        }
#ifdef AR9100
    }
#endif

    /*
     * Set Mac(BB,Phy) Warm Reset
     */
    OS_REG_WRITE(ah, AR_RTC_RC, rst_flags);
    OS_REG_WRITE_FLUSH(ah);

	DISABLE_REG_WRITE_BUFFER

    if (AR_SREV_HOWL(ah)) {
       OS_DELAY( 10000); /* 10 msec */
    } else {
       OS_DELAY(100); /* 100 usec */
    }

    /*
     * Clear resets and force wakeup
     */
    OS_REG_WRITE(ah, AR_RTC_RC, 0);
    if (!ath_hal_wait(ah, AR_RTC_RC, AR_RTC_RC_M, 0, AH_WAIT_TIMEOUT)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: RTC stuck in MAC reset\n", __FUNCTION__);
        return AH_FALSE;
    }

    if (!AR_SREV_HOWL(ah)) {
        /* Clear AHB reset */
        OS_REG_WRITE(ah, AR_RC, 0);
    }

    ar5416AttachHwPlatform(ah);

#ifdef AR9100
    if (type != HAL_RESET_WARM) {
        /* Reset the AHB - WMAC interface in Howl. */
        ath_hal_ahb_mac_reset();
        OS_DELAY(50); /* 50 usec */
    }
#endif

    return AH_TRUE;
}

void
ar5416UploadNoiseFloor(struct ath_hal *ah, int16_t nfarray[NUM_NF_READINGS])
{
    int16_t nf;

    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        nf = MS(OS_REG_READ(ah, AR_PHY_CCA), AR9280_PHY_MINCCA_PWR);
    } else {
        nf = MS(OS_REG_READ(ah, AR_PHY_CCA), AR_PHY_MINCCA_PWR);
    }
    if (nf & 0x100) nf = 0 - ((nf ^ 0x1ff) + 1);
    HDPRINTF(ah, HAL_DBG_NF_CAL, "NF calibrated [ctl] [chain 0] is %d\n", nf);
    nfarray[0] = nf;

    if ((!AR_SREV_KITE(ah)) && (!AR_SREV_K2(ah))) {
        if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
            nf = MS(OS_REG_READ(ah, AR_PHY_CH1_CCA), AR9280_PHY_CH1_MINCCA_PWR);
        } else {
            nf = MS(OS_REG_READ(ah, AR_PHY_CH1_CCA), AR_PHY_CH1_MINCCA_PWR);
        }
        if (nf & 0x100)
                nf = 0 - ((nf ^ 0x1ff) + 1);
        HDPRINTF(ah, HAL_DBG_NF_CAL, "NF calibrated [ctl] [chain 1] is %d\n", nf);
        nfarray[1] = nf;

        if (!AR_SREV_MERLIN(ah) && !AR_SREV_KIWI(ah)) {
            nf = MS(OS_REG_READ(ah, AR_PHY_CH2_CCA), AR_PHY_CH2_MINCCA_PWR);
            if (nf & 0x100)
                nf = 0 - ((nf ^ 0x1ff) + 1);
            HDPRINTF(ah, HAL_DBG_NF_CAL, "NF calibrated [ctl] [chain 2] is %d\n", nf);
            nfarray[2] = nf;
        }
    }

    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        nf = MS(OS_REG_READ(ah, AR_PHY_EXT_CCA), AR9280_PHY_EXT_MINCCA_PWR);
    } else {
        nf = MS(OS_REG_READ(ah, AR_PHY_EXT_CCA), AR_PHY_EXT_MINCCA_PWR);
    }
    if (nf & 0x100)
            nf = 0 - ((nf ^ 0x1ff) + 1);
    HDPRINTF(ah, HAL_DBG_NF_CAL, "NF calibrated [ext] [chain 0] is %d\n", nf);
    nfarray[3] = nf;

    if((!AR_SREV_KITE(ah)) && (!AR_SREV_K2(ah))) {
        if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
            nf = MS(OS_REG_READ(ah, AR_PHY_CH1_EXT_CCA), AR9280_PHY_CH1_EXT_MINCCA_PWR);
        } else {
            nf = MS(OS_REG_READ(ah, AR_PHY_CH1_EXT_CCA), AR_PHY_CH1_EXT_MINCCA_PWR);
        }
        if (nf & 0x100)
                nf = 0 - ((nf ^ 0x1ff) + 1);
        HDPRINTF(ah, HAL_DBG_NF_CAL, "NF calibrated [ext] [chain 1] is %d\n", nf);
        nfarray[4] = nf;

        if (!AR_SREV_MERLIN(ah) && !AR_SREV_KIWI(ah)) {
            nf = MS(OS_REG_READ(ah, AR_PHY_CH2_EXT_CCA), AR_PHY_CH2_EXT_MINCCA_PWR);
            if (nf & 0x100)
                nf = 0 - ((nf ^ 0x1ff) + 1);
            HDPRINTF(ah, HAL_DBG_NF_CAL, "NF calibrated [ext] [chain 2] is %d\n", nf);
            nfarray[5] = nf;
        }
    }
}

static void
ar5416StartNFCal(struct ath_hal *ah)
{
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);
    AH5416(ah)->nf_tsf32 = ar5416GetTsf32(ah);
}

static void
ar5416LoadNF(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan, int is_scan)
{
    HAL_NFCAL_BASE *h_base;
    int i, j;
    int32_t val;
    const u_int32_t ar5416_cca_regs[6] = {
        AR_PHY_CCA,
        AR_PHY_CH1_CCA,
        AR_PHY_CH2_CCA,
        AR_PHY_EXT_CCA,
        AR_PHY_CH1_EXT_CCA,
        AR_PHY_CH2_EXT_CCA
    };
    u_int8_t rxChainStatus, chainmask;

    /* Find the chains that are active because of Power save mode*/
    rxChainStatus = ar5416GetRxChainMask(ah);
    /* Force NF calibration for all chains, otherwise Vista station
     * would conduct a bad performance
     */
    if ((AR_SREV_KITE(ah)) || (AR_SREV_K2(ah))){
        /* Kite has only one chain */
        chainmask = 0x9;
    } else if (AR_SREV_MERLIN(ah) || AR_SREV_KIWI(ah)) {
        /* Merlin and Kiwi has only two chains */
        if( (rxChainStatus & 0x2) || (rxChainStatus & 0x4))
            chainmask = 0x1B;
        else
            chainmask = 0x09;
    } else {
        /*
         * OWL has 3 chains. Check which of them are active and run
         * calibration only on those
         */
        if (rxChainStatus & 0x4) {
            chainmask = 0x3f;
        } else if( rxChainStatus & 0x2) {
            chainmask = 0x1B;
        } else {
            chainmask = 0x09;
        }
    }

    /*
     * Write filtered NF values into maxCCApwr register parameter
     * so we can load below.
     */
#ifdef ATH_NF_PER_CHAN
    h_base = &chan->nf_cal_hist.base;
#else
    if (is_scan) {
        /*
         * The channel we are currently on is not the home channel,
         * so we shouldn't use the home channel NF buffer's values on
         * this channel.  Instead, use the NF single value already
         * read for this channel.  (Or, if we haven't read the NF for
         * this channel yet, the SW default for this chip/band will
         * be used.)
         */
        h_base = &chan->nf_cal_hist.base;
    } else {
        /* use the home channel NF info */
        h_base = &AH_PRIVATE(ah)->nf_cal_hist.base;
    }
#endif

    for (i = 0; i < NUM_NF_READINGS; i ++) {
        if (chainmask & (1 << i)) {
            val = OS_REG_READ(ah, ar5416_cca_regs[i]);
            val &= 0xFFFFFE00;
            val |= (((u_int32_t)(h_base->priv_nf[i]) << 1) & 0x1ff);
            OS_REG_WRITE(ah, ar5416_cca_regs[i], val);
        }
    }

    /*
     * Load software filtered NF value into baseband internal minCCApwr
     * variable.
     */
    OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
    OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);

    /* Wait for load to complete, should be fast, a few 10s of us. */
    /* Changed the max delay 250us back to 10000us, since 250us often
     * results in NF load timeout and causes deaf condition
     * during stress testing 11/28/2009 */
    for (j = 0; j < NF_LOAD_DELAY; j++) {
        if ((OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0)
            break;
        OS_DELAY(10);
    }
    if (j == NF_LOAD_DELAY) {
        /*
         * We timed out waiting for the noisefloor to load, probably due to an in-progress rx.
         * Simply return here and allow the load plenty of time to complete before the next 
         * calibration interval.  We need to avoid trying to load -50 (which happens below) 
         * while the previous load is still in progress as this can cause rx deafness 
         * (see EV 66368,62830).  Instead by returning here, the baseband nf cal will 
         * just be capped by our present noisefloor until the next calibration timer.
         */
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "%s: *** TIMEOUT while waiting for nf to load: AR_PHY_AGC_CONTROL=0x%x ***\n", 
                 __func__,OS_REG_READ(ah, AR_PHY_AGC_CONTROL));
        return;
    }

    /*
     * Restore maxCCAPower register parameter again so that we're not capped
     * by the median we just loaded.  This will be initial (and max) value
     * of next noise floor calibration the baseband does.
     */
	ENABLE_REG_WRITE_BUFFER
    for (i = 0; i < NUM_NF_READINGS; i ++) {
        if (chainmask & (1 << i)) {
            val = OS_REG_READ(ah, ar5416_cca_regs[i]);
            val &= 0xFFFFFE00;
            val |= (((u_int32_t)(-50) << 1) & 0x1ff);
            OS_REG_WRITE(ah, ar5416_cca_regs[i], val);
        }
    }
    OS_REG_WRITE_FLUSH(ah);
	DISABLE_REG_WRITE_BUFFER
}

/*
 * Read the NF and check it against the noise floor threshhold
 */
static int
ar5416StoreNewNf(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan, int is_scan)
{
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    int nf_hist_len;
    int16_t nf_no_lim;
    int16_t nfarray[NUM_NF_READINGS]= {0};
    HAL_NFCAL_HIST_FULL *h;

    if (OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) {
        u_int32_t tsf32, nf_cal_dur_tsf;
        /*
         * The reason the NF calibration did not complete may just be that
         * not enough time has passed since the NF calibration was started,
         * because under certain conditions (when first moving to a new
         * channel) the NF calibration may be checked very repeatedly.
         * Or, there may be CW interference keeping the NF calibration
         * from completing.  Check the delta time between when the NF
         * calibration was started and now to see whether the NF calibration
         * should have already completed (but hasn't, probably due to CW
         * interference), or hasn't had enough time to finish yet.
         */
        /*
         * AH_NF_CAL_DUR_MAX_TSF - A conservative maximum time that the
         *     HW should need to finish a NF calibration.  If the HW
         *     does not complete a NF calibration within this time period,
         *     there must be a problem - probably CW interference.
         * AH_NF_CAL_PERIOD_MAX_TSF - A conservative maximum time between
         *     check of the HW's NF calibration being finished.
         *     If the difference between the current TSF and the TSF
         *     recorded when the NF calibration started is larger than this
         *     value, the TSF must have been reset.
         *     In general, we expect the TSF to only be reset during
         *     regular operation for STAs, not for APs.  However, an
         *     AP's TSF could be reset when joining an IBSS.
         *     There's an outside chance that this could result in the
         *     CW_INT flag being erroneously set, if the TSF adjustment
         *     is smaller than AH_NF_CAL_PERIOD_MAX_TSF but larger than
         *     AH_NF_CAL_DUR_TSF.  However, even if this does happen,
         *     it shouldn't matter, as the IBSS case shouldn't be
         *     concerned about CW_INT.
         */
        #define AH_NF_CAL_DUR_TSF (90*1000*1000) /* 90 sec in usec units */
        #define AH_NF_CAL_PERIOD_MAX_TSF (180*1000*1000) /* 180 sec in usec */
        /* wraparound handled by using unsigned values */
        tsf32 = ar5416GetTsf32(ah);
        nf_cal_dur_tsf = tsf32 - AH5416(ah)->nf_tsf32;
        if (nf_cal_dur_tsf > AH_NF_CAL_PERIOD_MAX_TSF) {
            /*
             * The TSF must have gotten reset during the NF cal -
             * just reset the NF TSF timestamp, so the next time
             * this function is called, the timestamp comparison
             * will be valid.
             */
            AH5416(ah)->nf_tsf32 = tsf32;
        } else if (nf_cal_dur_tsf > AH_NF_CAL_DUR_TSF) {
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s: NF did not complete in calibration window\n", __func__);
            /* the NF incompletion is probably due to CW interference */
            chan->channel_flags |= CHANNEL_CW_INT;
        }
        return 0; /* HW's NF measurement not finished */
    }

    
    ar5416UploadNoiseFloor(ah, nfarray);

    /* Update the NF buffer for each chain masked by chainmask */
#ifdef ATH_NF_PER_CHAN
    h = &chan->nf_cal_hist;
    nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
#else
    if (is_scan) {
        /*
         * This channel's NF cal info is just a HAL_NFCAL_HIST_SMALL struct
         * rather than a HAL_NFCAL_HIST_FULL struct.
         * As long as we only use the first history element of nf_cal_buffer
         * (nf_cal_buffer[0][0:NUM_NF_READINGS-1]), we can use
         * HAL_NFCAL_HIST_SMALL and HAL_NFCAL_HIST_FULL interchangeably.
         */
        h = (HAL_NFCAL_HIST_FULL *) &chan->nf_cal_hist;
        nf_hist_len = HAL_NF_CAL_HIST_LEN_SMALL;
    } else {
        h = &AH_PRIVATE(ah)->nf_cal_hist;
        nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
    }
#endif

    /*
     * nf_no_lim = median value from NF history buffer without bounds limits,
     * priv_nf = median value from NF history buffer with bounds limits.
     */
    nf_no_lim = ar5416UpdateNFHistBuff(ah, h, nfarray, nf_hist_len);
    chan->raw_noise_floor = h->base.priv_nf[0];

    /* check if there is interference */
    chan->channel_flags &= (~CHANNEL_CW_INT);
    if (!IS_5416_EMU(ah) &&
        nf_no_lim > ahpriv->nfp->nominal + ahpriv->nf_cw_int_delta) {
        /*
         * Since this CW interference check is being applied to the
         * median element of the NF history buffer, this indicates that
         * the CW interference is persistent.  A single high NF reading
         * will not show up in the median, and thus will not cause the
         * CW_INT flag to be set.
         */
         HDPRINTF(ah, HAL_DBG_NF_CAL,
                "%s: NF Cal: CW interferer detected through NF: %d \n", __func__, nf_no_lim); 
         chan->channel_flags |= CHANNEL_CW_INT;
    }
    return 1; /* HW's NF measurement finished */
}

/* 
 * Noise Floor values for all chains. 
 * Most recently updated values from the NF history buffer are used.
 */
void ar5416ChainNoiseFloor(struct ath_hal *ah, int16_t *nfBuf,
                           HAL_CHANNEL *chan, int is_scan)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int i, nf_hist_len, recentNFIndex = 0;
    HAL_NFCAL_HIST_FULL *h;
    u_int8_t rx_chainmask = ahp->ah_rxchainmask | (ahp->ah_rxchainmask << 3);
    HAL_CHANNEL_INTERNAL *ichan = ath_hal_checkchannel(ah, chan); 
    HALASSERT(ichan);

#ifdef ATH_NF_PER_CHAN
    /* Fill 0 if valid internal channel is not found */
    if (ichan == AH_NULL) {
        OS_MEMZERO(nfBuf, sizeof(nfBuf[0])*NUM_NF_READINGS);
        return;
    }
    h = &ichan->nf_cal_hist;
    nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
#else
    /*
     * If a scan is not in progress, then the most recent value goes
     * into ahpriv->nf_cal_hist.  If a scan is in progress, then
     * the most recent value goes into ichan->nf_cal_hist.
     * Thus, return the value from ahpriv->nf_cal_hist if there's
     * no scan, and if the specified channel is the current channel.
     * Otherwise, return the noise floor from ichan->nf_cal_hist.
     */
    if ((!is_scan) && chan->channel == AH_PRIVATE(ah)->ah_curchan->channel) {
        h = &AH_PRIVATE(ah)->nf_cal_hist;
        nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
    } else {
        /* Fill 0 if valid internal channel is not found */
        if (ichan == AH_NULL) {
            OS_MEMZERO(nfBuf, sizeof(nfBuf[0])*NUM_NF_READINGS);
            return;
        }
        /*
         * It is okay to treat a HAL_NFCAL_HIST_SMALL struct as if it were a
         * HAL_NFCAL_HIST_FULL struct, as long as only the index 0 of the
         * nf_cal_buffer is used (nf_cal_buffer[0][0:NUM_NF_READINGS-1])
         */
        h = (HAL_NFCAL_HIST_FULL *) &ichan->nf_cal_hist;
        nf_hist_len = HAL_NF_CAL_HIST_LEN_SMALL;
    }
#endif
    
    /* Get most recently updated values from nf cal history buffer */
    recentNFIndex = (h->base.curr_index) ?
        h->base.curr_index - 1 : nf_hist_len - 1;
    for (i = 0; i < NUM_NF_READINGS; i ++) {
        /* Fill 0 for unsupported chains */
        if (!(rx_chainmask & (1 << i))) {
            nfBuf[i] = 0;
            continue;
        }
        nfBuf[i] = h->nf_cal_buffer[recentNFIndex][i];
    }
}


static int16_t ar5416LimitNfRange(struct ath_hal *ah, int16_t nf)
{
    if (nf < AH_PRIVATE(ah)->nfp->min) {
        return AH_PRIVATE(ah)->nfp->nominal;
    } else if (nf > AH_PRIVATE(ah)->nfp->max) {
        return AH_PRIVATE(ah)->nfp->max;
    }
    return nf;
}

#ifndef ATH_NF_PER_CHAN
inline static void
ar5416ResetNfHistBuff(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichan)
{
    HAL_CHAN_NFCAL_HIST *h = &ichan->nf_cal_hist;
    HAL_NFCAL_HIST_FULL *home = &AH_PRIVATE(ah)->nf_cal_hist;
    int i;

    /* 
     * Copy the value for the channel in question into the home-channel
     * NF history buffer.  The channel NF is probably a value filled in by
     * a prior background channel scan, but if no scan has been done then
     * it is the nominal noise floor filled in by ath_hal_init_NF_buffer
     * for this chip and the channel's band.
     * Replicate this channel NF into all entries of the home-channel NF
     * history buffer.
     * If the channel NF was filled in by a channel scan, it has not had
     * bounds limits applied to it yet - do so now.  It is important to
     * apply bounds limits to the priv_nf value that gets loaded into the
     * WLAN chip's minCCApwr register field.  It is also necessary to
     * apply bounds limits to the nf_cal_buffer[] elements.  Since we are
     * replicating a single NF reading into all nf_cal_buffer elements,
     * if the single reading were above the CW_INT threshold, the CW_INT
     * check in ar5416GetNf would immediately conclude that CW interference
     * is present, even though we're not supposed to set CW_INT unless
     * NF values are _consistently_ above the CW_INT threshold.
     * Applying the bounds limits to the nf_cal_buffer contents fixes this
     * problem.
     */
    for (i = 0; i < NUM_NF_READINGS; i++) {
        int j;
        int16_t nf;
        /*
         * No need to set curr_index, since it already has a value in
         * the range [0..HAL_NF_CAL_HIST_LEN_FULL), and all nf_cal_buffer
         * values will be the same.
         */
        nf = ar5416LimitNfRange(ah, h->nf_cal_buffer[0][i]);
        for (j = 0; j < HAL_NF_CAL_HIST_LEN_FULL; j++) {
            home->nf_cal_buffer[j][i] = nf;
        }
        AH_PRIVATE(ah)->nf_cal_hist.base.priv_nf[i] = nf;
    }
}
#endif

/*
 *  Update the noise floor buffer as a ring buffer
 */
static int16_t
ar5416UpdateNFHistBuff(struct ath_hal *ah, HAL_NFCAL_HIST_FULL *h,
                       int16_t *nfarray, int hist_len)
{
    int i;
    int16_t nf_no_lim_chain0;

    
    nf_no_lim_chain0 = ar5416GetNfHistMid(h, 0, hist_len);
    for (i = 0; i < NUM_NF_READINGS; i++) {
        h->nf_cal_buffer[h->base.curr_index][i] = nfarray[i];
        h->base.priv_nf[i] = ar5416LimitNfRange(
            ah, ar5416GetNfHistMid(h, i, hist_len));
    }
    if (++h->base.curr_index >= hist_len) h->base.curr_index = 0;
    return nf_no_lim_chain0;
}

/*
 *  Pick up the medium one in the noise floor buffer and update the corresponding range
 *  for valid noise floor values
 */
static int16_t
ar5416GetNfHistMid(HAL_NFCAL_HIST_FULL *h, int reading, int hist_len)
{
    int16_t nfval;
    int16_t sort[HAL_NF_CAL_HIST_LEN_FULL]; /* upper bound for hist_len */
    int i,j;

    for (i = 0; i < hist_len; i ++) {
        sort[i] = h->nf_cal_buffer[i][reading];
    }
    for (i = 0; i < hist_len-1; i ++) {
        for (j = 1; j < hist_len-i; j ++) {
            if (sort[j] > sort[j-1]) {
                nfval = sort[j];
                sort[j] = sort[j-1];
                sort[j-1] = nfval;
            }
        }
    }
    nfval = sort[(hist_len-1)>>1];

    return nfval;
}

/* Used by the scan function for a quick read of the noise floor. This is used to detect
 * presence of CW interference such as video bridge. The noise floor is assumed to have been
 * already started during reset called during channel change. The function checks if the noise 
 * floor reading is done. In case it has been done, it reads the noise floor value. If the noise floor
 * calibration has not been finished, it assumes this is due to presence of CW interference an returns a high 
 * value for noise floor. derived from the CW interference threshold + margin fudge factor. 
 */
#define BAD_SCAN_NF_MARGIN (30)
int16_t ar5416GetMinCCAPwr(struct ath_hal *ah)
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
        if (nf & 0x100) {
            nf = 0 - ((nf ^ 0x1ff) + 1);
        }
    } else {
        /* NF calibration is not done, assume CW interference */
        nf = ahpriv->nfp->nominal + ahpriv->nf_cw_int_delta + BAD_SCAN_NF_MARGIN;    
    }
    return nf;
}

#define MAX_ANALOG_START        319             /* XXX */

/*
 * Delta slope coefficient computation.
 * Required for OFDM operation.
 */
void
ar5416SetDeltaSlope(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
        u_int32_t coef_scaled, ds_coef_exp, ds_coef_man;
        u_int32_t clockMhzScaled = 0x64000000; /* clock * 2.5 */
        CHAN_CENTERS centers;

        /* half and quarter rate can divide the scaled clock by 2 or 4 */
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
        ar5416GetChannelCenters(ah, chan, &centers);
        coef_scaled = clockMhzScaled / centers.synth_center;

        ar5416GetDeltaSlopeValues(ah, coef_scaled, &ds_coef_man, &ds_coef_exp);

        OS_REG_RMW_FIELD(ah, AR_PHY_TIMING3,
                AR_PHY_TIMING3_DSC_MAN, ds_coef_man);
        OS_REG_RMW_FIELD(ah, AR_PHY_TIMING3,
                AR_PHY_TIMING3_DSC_EXP, ds_coef_exp);

        /*
         * For Short GI,
         * scaled coeff is 9/10 that of normal coeff
         */
        coef_scaled = (9 * coef_scaled)/10;

        ar5416GetDeltaSlopeValues(ah, coef_scaled, &ds_coef_man, &ds_coef_exp);

        /* for short gi */
        OS_REG_RMW_FIELD(ah, AR_PHY_HALFGI,
                AR_PHY_HALFGI_DSC_MAN, ds_coef_man);
        OS_REG_RMW_FIELD(ah, AR_PHY_HALFGI,
                AR_PHY_HALFGI_DSC_EXP, ds_coef_exp);
}

static void
ar5416GetDeltaSlopeValues(struct ath_hal *ah, u_int32_t coef_scaled,
                          u_int32_t *coef_mantissa, u_int32_t *coef_exponent)
{
    u_int32_t coef_exp, coef_man;

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

    *coef_mantissa = coef_man >> (COEF_SCALE_S - coef_exp);
    *coef_exponent = coef_exp - 16;
}

/*
 * Convert to baseband spur frequency given input channel frequency
 * and compute register settings below.
 */
#define SPUR_RSSI_THRESH 40
#if 0
#define SPUR_INVALID     8888
int rf_spurs[] = { 24167, 24400 }; /* defined in 100kHz units */
#define N(a)    (sizeof (a) / sizeof (a[0]))
#endif
void
ar5416SpurMitigate(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    int bb_spur = AR_NO_SPUR;
    int bin, cur_bin;
    int spur_freq_sd;
    int spur_delta_phase;
    int denominator;
    int upper, lower, cur_vit_mask;
    int tmp, new;
    int i;
    int pilot_mask_reg[4] = { AR_PHY_TIMING7, AR_PHY_TIMING8,
                AR_PHY_PILOT_MASK_01_30, AR_PHY_PILOT_MASK_31_60 };
    int chan_mask_reg[4] = { AR_PHY_TIMING9, AR_PHY_TIMING10,
                AR_PHY_CHANNEL_MASK_01_30, AR_PHY_CHANNEL_MASK_31_60 };
    int inc[4] = { 0, 100, 0, 0 };

    int8_t mask_m[123] = { 0 };
    int8_t mask_p[123] = { 0 };
    int8_t mask_amt;
    int tmp_mask;
    int cur_bb_spur;
    HAL_BOOL is2GHz = IS_CHAN_2GHZ(chan);

    /*
     * Need to verify range +/- 9.5 for static ht20, otherwise spur
     * is out-of-band and can be ignored.
     */

    for (i = 0; i < AR_EEPROM_MODAL_SPURS; i++) {
        //spurChan = ee->ee_spurChans[i][is2GHz];
        cur_bb_spur = AH_PRIVATE(ah)->ah_eeprom_get_spur_chan(ah,i,is2GHz);
        if (AR_NO_SPUR == cur_bb_spur)
            break;
        cur_bb_spur = cur_bb_spur - (chan->channel * 10);
        if ((cur_bb_spur > -95) && (cur_bb_spur < 95)) {
            bb_spur = cur_bb_spur;
            break;
        }
    }
#if 0
    for (i = 0; i < N(rf_spurs); i++) {
        int cur_bb_spur = rf_spurs[i] - (chan->channel * 10);
        if ((cur_bb_spur > -95) && (cur_bb_spur < 95)) {
            bb_spur = cur_bb_spur;
            break;
        }
    }
#endif

    if (AR_NO_SPUR == bb_spur)
        return;

    bin = bb_spur * 32;

    tmp = OS_REG_READ(ah, AR_PHY_TIMING_CTRL4(0));
    new = tmp | (AR_PHY_TIMING_CTRL4_ENABLE_SPUR_RSSI |
        AR_PHY_TIMING_CTRL4_ENABLE_SPUR_FILTER |
        AR_PHY_TIMING_CTRL4_ENABLE_CHAN_MASK |
        AR_PHY_TIMING_CTRL4_ENABLE_PILOT_MASK);

    OS_REG_WRITE(ah, AR_PHY_TIMING_CTRL4(0), new);

    new = (AR_PHY_SPUR_REG_MASK_RATE_CNTL |
        AR_PHY_SPUR_REG_ENABLE_MASK_PPM |
        AR_PHY_SPUR_REG_MASK_RATE_SELECT |
        AR_PHY_SPUR_REG_ENABLE_VIT_SPUR_RSSI |
        SM(SPUR_RSSI_THRESH, AR_PHY_SPUR_REG_SPUR_RSSI_THRESH));
    OS_REG_WRITE(ah, AR_PHY_SPUR_REG, new);
    /*
     * Should offset bb_spur by +/- 10 MHz for dynamic 2040 MHz
     * config, no offset for HT20.
     * spur_delta_phase = bb_spur/40 * 2**21 for static ht20,
     * /80 for dyn2040.
     */
    spur_delta_phase = ((bb_spur * 524288) / 100) &
        AR_PHY_TIMING11_SPUR_DELTA_PHASE;
    /*
     * in 11A mode the denominator of spur_freq_sd should be 40 and
     * it should be 44 in 11G
     */
    denominator = IS_CHAN_2GHZ(chan) ? 440 : 400;
    spur_freq_sd = ((bb_spur * 2048) / denominator) & 0x3ff;

    new = (AR_PHY_TIMING11_USE_SPUR_IN_AGC |
        SM(spur_freq_sd, AR_PHY_TIMING11_SPUR_FREQ_SD) |
        SM(spur_delta_phase, AR_PHY_TIMING11_SPUR_DELTA_PHASE));
    OS_REG_WRITE(ah, AR_PHY_TIMING11, new);


    /*
     * ============================================
     * pilot mask 1 [31:0] = +6..-26, no 0 bin
     * pilot mask 2 [19:0] = +26..+7
     *
     * channel mask 1 [31:0] = +6..-26, no 0 bin
     * channel mask 2 [19:0] = +26..+7
     */
    //cur_bin = -26;
    cur_bin = -6000;
    upper = bin + 100;
    lower = bin - 100;

    for (i = 0; i < 4; i++) {
        int pilot_mask = 0;
        int chan_mask  = 0;
        int bp         = 0;
        for (bp = 0; bp < 30; bp++) {
            if ((cur_bin > lower) && (cur_bin < upper)) {
                pilot_mask = pilot_mask | 0x1 << bp;
                chan_mask  = chan_mask | 0x1 << bp;
            }
            cur_bin += 100;
        }
        cur_bin += inc[i];
        OS_REG_WRITE(ah, pilot_mask_reg[i], pilot_mask);
        OS_REG_WRITE(ah, chan_mask_reg[i], chan_mask);
    }

    /* =================================================
     * viterbi mask 1 based on channel magnitude
     * four levels 0-3
     *  - mask (-27 to 27) (reg 64,0x9900 to 67,0x990c)
     *      [1 2 2 1] for -9.6 or [1 2 1] for +16
     *  - enable_mask_ppm, all bins move with freq
     *
     *  - mask_select,    8 bits for rates (reg 67,0x990c)
     *  - mask_rate_cntl, 8 bits for rates (reg 67,0x990c)
     *      choose which mask to use mask or mask2
     */

    /*
     * viterbi mask 2  2nd set for per data rate puncturing
     * four levels 0-3
     *  - mask_select, 8 bits for rates (reg 67)
     *  - mask (-27 to 27) (reg 98,0x9988 to 101,0x9994)
     *      [1 2 2 1] for -9.6 or [1 2 1] for +16
     */
    cur_vit_mask = 6100;
    upper        = bin + 120;
    lower        = bin - 120;

    for (i = 0; i < 123; i++) {
        if ((cur_vit_mask > lower) && (cur_vit_mask < upper)) {
            /* workaround for mips 4.2.4 bug #37014 */
            volatile int tmp_abs = abs(cur_vit_mask - bin);
            if (tmp_abs < 75) {
                mask_amt = 1;
            } else {
                mask_amt = 0;
            }
            if (cur_vit_mask < 0) {
                mask_m[abs(cur_vit_mask / 100)] = mask_amt;
            } else {
                mask_p[cur_vit_mask / 100] = mask_amt;
            }
        }
        cur_vit_mask -= 100;
    }

    tmp_mask = (mask_m[46] << 30) | (mask_m[47] << 28)
          | (mask_m[48] << 26) | (mask_m[49] << 24)
          | (mask_m[50] << 22) | (mask_m[51] << 20)
          | (mask_m[52] << 18) | (mask_m[53] << 16)
          | (mask_m[54] << 14) | (mask_m[55] << 12)
          | (mask_m[56] << 10) | (mask_m[57] <<  8)
          | (mask_m[58] <<  6) | (mask_m[59] <<  4)
          | (mask_m[60] <<  2) | (mask_m[61] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK_1, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_VIT_MASK2_M_46_61, tmp_mask);

    tmp_mask =             (mask_m[31] << 28)
          | (mask_m[32] << 26) | (mask_m[33] << 24)
          | (mask_m[34] << 22) | (mask_m[35] << 20)
          | (mask_m[36] << 18) | (mask_m[37] << 16)
          | (mask_m[48] << 14) | (mask_m[39] << 12)
          | (mask_m[40] << 10) | (mask_m[41] <<  8)
          | (mask_m[42] <<  6) | (mask_m[43] <<  4)
          | (mask_m[44] <<  2) | (mask_m[45] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK_2, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_M_31_45, tmp_mask);

    tmp_mask = (mask_m[16] << 30) | (mask_m[16] << 28)
          | (mask_m[18] << 26) | (mask_m[18] << 24)
          | (mask_m[20] << 22) | (mask_m[20] << 20)
          | (mask_m[22] << 18) | (mask_m[22] << 16)
          | (mask_m[24] << 14) | (mask_m[24] << 12)
          | (mask_m[25] << 10) | (mask_m[26] <<  8)
          | (mask_m[27] <<  6) | (mask_m[28] <<  4)
          | (mask_m[29] <<  2) | (mask_m[30] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK_3, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_M_16_30, tmp_mask);

    tmp_mask = (mask_m[ 0] << 30) | (mask_m[ 1] << 28)
          | (mask_m[ 2] << 26) | (mask_m[ 3] << 24)
          | (mask_m[ 4] << 22) | (mask_m[ 5] << 20)
          | (mask_m[ 6] << 18) | (mask_m[ 7] << 16)
          | (mask_m[ 8] << 14) | (mask_m[ 9] << 12)
          | (mask_m[10] << 10) | (mask_m[11] <<  8)
          | (mask_m[12] <<  6) | (mask_m[13] <<  4)
          | (mask_m[14] <<  2) | (mask_m[15] <<  0);
    OS_REG_WRITE(ah, AR_PHY_MASK_CTL, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_M_00_15, tmp_mask);

    tmp_mask =             (mask_p[15] << 28)
          | (mask_p[14] << 26) | (mask_p[13] << 24)
          | (mask_p[12] << 22) | (mask_p[11] << 20)
          | (mask_p[10] << 18) | (mask_p[ 9] << 16)
          | (mask_p[ 8] << 14) | (mask_p[ 7] << 12)
          | (mask_p[ 6] << 10) | (mask_p[ 5] <<  8)
          | (mask_p[ 4] <<  6) | (mask_p[ 3] <<  4)
          | (mask_p[ 2] <<  2) | (mask_p[ 1] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_1, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_P_15_01, tmp_mask);

    tmp_mask =             (mask_p[30] << 28)
          | (mask_p[29] << 26) | (mask_p[28] << 24)
          | (mask_p[27] << 22) | (mask_p[26] << 20)
          | (mask_p[25] << 18) | (mask_p[24] << 16)
          | (mask_p[23] << 14) | (mask_p[22] << 12)
          | (mask_p[21] << 10) | (mask_p[20] <<  8)
          | (mask_p[19] <<  6) | (mask_p[18] <<  4)
          | (mask_p[17] <<  2) | (mask_p[16] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_2, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_P_30_16, tmp_mask);

    tmp_mask =             (mask_p[45] << 28)
          | (mask_p[44] << 26) | (mask_p[43] << 24)
          | (mask_p[42] << 22) | (mask_p[41] << 20)
          | (mask_p[40] << 18) | (mask_p[39] << 16)
          | (mask_p[38] << 14) | (mask_p[37] << 12)
          | (mask_p[36] << 10) | (mask_p[35] <<  8)
          | (mask_p[34] <<  6) | (mask_p[33] <<  4)
          | (mask_p[32] <<  2) | (mask_p[31] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_3, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_P_45_31, tmp_mask);

    tmp_mask = (mask_p[61] << 30) | (mask_p[60] << 28)
          | (mask_p[59] << 26) | (mask_p[58] << 24)
          | (mask_p[57] << 22) | (mask_p[56] << 20)
          | (mask_p[55] << 18) | (mask_p[54] << 16)
          | (mask_p[53] << 14) | (mask_p[52] << 12)
          | (mask_p[51] << 10) | (mask_p[50] <<  8)
          | (mask_p[49] <<  6) | (mask_p[48] <<  4)
          | (mask_p[47] <<  2) | (mask_p[46] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_4, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_P_61_45, tmp_mask);
}

void
ar9280SpurMitigate(struct ath_hal *ah, HAL_CHANNEL *chan, HAL_CHANNEL_INTERNAL *ichan)
{
    int bb_spur = AR_NO_SPUR;
    int freq;
    int bin, cur_bin;
    int bb_spur_off, spur_subchannel_sd;
    int spur_freq_sd;
    int spur_delta_phase;
    int denominator;
    int upper, lower, cur_vit_mask;
    int tmp, newVal;
    int i;
    int pilot_mask_reg[4] = { AR_PHY_TIMING7, AR_PHY_TIMING8,
                AR_PHY_PILOT_MASK_01_30, AR_PHY_PILOT_MASK_31_60 };
    int chan_mask_reg[4] = { AR_PHY_TIMING9, AR_PHY_TIMING10,
                AR_PHY_CHANNEL_MASK_01_30, AR_PHY_CHANNEL_MASK_31_60 };
    int inc[4] = { 0, 100, 0, 0 };
    CHAN_CENTERS centers;

    int8_t mask_m[123];
    int8_t mask_p[123];
    int8_t mask_amt;
    int tmp_mask;
    int cur_bb_spur;
    HAL_BOOL is2GHz = IS_CHAN_2GHZ(chan);

    OS_MEMZERO(&mask_m, sizeof(int8_t) * 123);
    OS_MEMZERO(&mask_p, sizeof(int8_t) * 123);

    ar5416GetChannelCenters(ah, ichan, &centers);
    freq = centers.synth_center;

    /*
     * Need to verify range +/- 9.38 for static ht20 and +/- 18.75 for ht40,
     * otherwise spur is out-of-band and can be ignored.
     */
    AH_PRIVATE(ah)->ah_config.ath_hal_spur_mode = SPUR_ENABLE_EEPROM;
    for (i = 0; i < AR_EEPROM_MODAL_SPURS; i++) {
        cur_bb_spur = AH_PRIVATE(ah)->ah_eeprom_get_spur_chan(ah, i, is2GHz);
        /* Get actual spur freq in MHz from EEPROM read value */
        if (is2GHz) {
            cur_bb_spur =  (cur_bb_spur / 10) + AR_BASE_FREQ_2GHZ;
        } else {
            cur_bb_spur =  (cur_bb_spur / 10) + AR_BASE_FREQ_5GHZ;
        }

        if (AR_NO_SPUR == cur_bb_spur)
            break;
        cur_bb_spur = cur_bb_spur - freq;

        if (IS_CHAN_HT40(chan)) {
            if ((cur_bb_spur > -AR_SPUR_FEEQ_BOUND_HT40) &&
                (cur_bb_spur < AR_SPUR_FEEQ_BOUND_HT40)) {
                bb_spur = cur_bb_spur;
                break;
            }
        } else if ((cur_bb_spur > -AR_SPUR_FEEQ_BOUND_HT20) &&
                   (cur_bb_spur < AR_SPUR_FEEQ_BOUND_HT20)) {
            bb_spur = cur_bb_spur;
            break;
        }
    }

    if (AR_NO_SPUR == bb_spur) {
#if 1
        /*
         * MRC CCK can interfere with beacon detection and cause deaf/mute.
         * Disable MRC CCK until it is fixed or a work around is applied.
         * See bug 33723 and 33590.
         */
        OS_REG_CLR_BIT(ah, AR_PHY_FORCE_CLKEN_CCK, AR_PHY_FORCE_CLKEN_CCK_MRC_MUX);
#else
        /* Enable MRC CCK if no spur is found in this channel. */
        OS_REG_SET_BIT(ah, AR_PHY_FORCE_CLKEN_CCK, AR_PHY_FORCE_CLKEN_CCK_MRC_MUX);
#endif
        return;
    } else {
        /*
         * For Merlin, spur can break CCK MRC algorithm. Disable CCK MRC if spur
         * is found in this channel.
         */
        OS_REG_CLR_BIT(ah, AR_PHY_FORCE_CLKEN_CCK, AR_PHY_FORCE_CLKEN_CCK_MRC_MUX);
    }

    bin = bb_spur * 320;

    tmp = OS_REG_READ(ah, AR_PHY_TIMING_CTRL4(0));

	ENABLE_REG_WRITE_BUFFER

    newVal = tmp | (AR_PHY_TIMING_CTRL4_ENABLE_SPUR_RSSI |
        AR_PHY_TIMING_CTRL4_ENABLE_SPUR_FILTER |
        AR_PHY_TIMING_CTRL4_ENABLE_CHAN_MASK |
        AR_PHY_TIMING_CTRL4_ENABLE_PILOT_MASK);
    OS_REG_WRITE(ah, AR_PHY_TIMING_CTRL4(0), newVal);

    newVal = (AR_PHY_SPUR_REG_MASK_RATE_CNTL |
        AR_PHY_SPUR_REG_ENABLE_MASK_PPM |
        AR_PHY_SPUR_REG_MASK_RATE_SELECT |
        AR_PHY_SPUR_REG_ENABLE_VIT_SPUR_RSSI |
        SM(SPUR_RSSI_THRESH, AR_PHY_SPUR_REG_SPUR_RSSI_THRESH));
    OS_REG_WRITE(ah, AR_PHY_SPUR_REG, newVal);

    /* Pick control or extn channel to cancel the spur */
    if (IS_CHAN_HT40(chan)) {
        if (bb_spur < 0) {
            spur_subchannel_sd = 1;
            bb_spur_off = bb_spur + 10;
        } else {
            spur_subchannel_sd = 0;
            bb_spur_off = bb_spur - 10;
        }
    } else {
        spur_subchannel_sd = 0;
        bb_spur_off = bb_spur;
    }

    /*
     * spur_delta_phase = bb_spur/40 * 2**21 for static ht20,
     * /80 for dyn2040.
     */
    if (IS_CHAN_HT40(chan))
        spur_delta_phase = ((bb_spur * 262144) / 10) & AR_PHY_TIMING11_SPUR_DELTA_PHASE;
    else
        spur_delta_phase = ((bb_spur * 524288) / 10) & AR_PHY_TIMING11_SPUR_DELTA_PHASE;

    /*
     * in 11A mode the denominator of spur_freq_sd should be 40 and
     * it should be 44 in 11G
     */
    denominator = IS_CHAN_2GHZ(chan) ? 44 : 40;
    spur_freq_sd = ((bb_spur_off * 2048) / denominator) & 0x3ff;

    newVal = (AR_PHY_TIMING11_USE_SPUR_IN_AGC |
        SM(spur_freq_sd, AR_PHY_TIMING11_SPUR_FREQ_SD) |
        SM(spur_delta_phase, AR_PHY_TIMING11_SPUR_DELTA_PHASE));
    OS_REG_WRITE(ah, AR_PHY_TIMING11, newVal);

    /* Choose to cancel between control and extension channels */
    newVal = spur_subchannel_sd << AR_PHY_SFCORR_SPUR_SUBCHNL_SD_S;
    OS_REG_WRITE(ah, AR_PHY_SFCORR_EXT, newVal);

    /*
     * ============================================
     * Set Pilot and Channel Masks
     *
     * pilot mask 1 [31:0] = +6..-26, no 0 bin
     * pilot mask 2 [19:0] = +26..+7
     *
     * channel mask 1 [31:0] = +6..-26, no 0 bin
     * channel mask 2 [19:0] = +26..+7
     */
    cur_bin = -6000;
    upper = bin + 100;
    lower = bin - 100;

    for (i = 0; i < 4; i++) {
        int pilot_mask = 0;
        int chan_mask  = 0;
        int bp         = 0;
        for (bp = 0; bp < 30; bp++) {
            if ((cur_bin > lower) && (cur_bin < upper)) {
                pilot_mask = pilot_mask | 0x1 << bp;
                chan_mask  = chan_mask | 0x1 << bp;
            }
            cur_bin += 100;
        }
        cur_bin += inc[i];
        OS_REG_WRITE(ah, pilot_mask_reg[i], pilot_mask);
        OS_REG_WRITE(ah, chan_mask_reg[i], chan_mask);
    }

    /* =================================================
     * viterbi mask 1 based on channel magnitude
     * four levels 0-3
     *  - mask (-27 to 27) (reg 64,0x9900 to 67,0x990c)
     *      [1 2 2 1] for -9.6 or [1 2 1] for +16
     *  - enable_mask_ppm, all bins move with freq
     *
     *  - mask_select,    8 bits for rates (reg 67,0x990c)
     *  - mask_rate_cntl, 8 bits for rates (reg 67,0x990c)
     *      choose which mask to use mask or mask2
     */

    /*
     * viterbi mask 2  2nd set for per data rate puncturing
     * four levels 0-3
     *  - mask_select, 8 bits for rates (reg 67)
     *  - mask (-27 to 27) (reg 98,0x9988 to 101,0x9994)
     *      [1 2 2 1] for -9.6 or [1 2 1] for +16
     */
    cur_vit_mask = 6100;
    upper        = bin + 120;
    lower        = bin - 120;

    for (i = 0; i < 123; i++) {
        if ((cur_vit_mask > lower) && (cur_vit_mask < upper)) {
            /* workaround for mips 4.2.4 bug #37014 */
            volatile int tmp_abs = abs(cur_vit_mask - bin);
            if (tmp_abs < 75) {
                mask_amt = 1;
            } else {
                mask_amt = 0;
            }
            if (cur_vit_mask < 0) {
                mask_m[abs(cur_vit_mask / 100)] = mask_amt;
            } else {
                mask_p[cur_vit_mask / 100] = mask_amt;
            }
        }
        cur_vit_mask -= 100;
    }

    tmp_mask = (mask_m[46] << 30) | (mask_m[47] << 28)
          | (mask_m[48] << 26) | (mask_m[49] << 24)
          | (mask_m[50] << 22) | (mask_m[51] << 20)
          | (mask_m[52] << 18) | (mask_m[53] << 16)
          | (mask_m[54] << 14) | (mask_m[55] << 12)
          | (mask_m[56] << 10) | (mask_m[57] <<  8)
          | (mask_m[58] <<  6) | (mask_m[59] <<  4)
          | (mask_m[60] <<  2) | (mask_m[61] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK_1, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_VIT_MASK2_M_46_61, tmp_mask);

    tmp_mask =             (mask_m[31] << 28)
          | (mask_m[32] << 26) | (mask_m[33] << 24)
          | (mask_m[34] << 22) | (mask_m[35] << 20)
          | (mask_m[36] << 18) | (mask_m[37] << 16)
          | (mask_m[48] << 14) | (mask_m[39] << 12)
          | (mask_m[40] << 10) | (mask_m[41] <<  8)
          | (mask_m[42] <<  6) | (mask_m[43] <<  4)
          | (mask_m[44] <<  2) | (mask_m[45] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK_2, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_M_31_45, tmp_mask);

    tmp_mask = (mask_m[16] << 30) | (mask_m[16] << 28)
          | (mask_m[18] << 26) | (mask_m[18] << 24)
          | (mask_m[20] << 22) | (mask_m[20] << 20)
          | (mask_m[22] << 18) | (mask_m[22] << 16)
          | (mask_m[24] << 14) | (mask_m[24] << 12)
          | (mask_m[25] << 10) | (mask_m[26] <<  8)
          | (mask_m[27] <<  6) | (mask_m[28] <<  4)
          | (mask_m[29] <<  2) | (mask_m[30] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK_3, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_M_16_30, tmp_mask);

    tmp_mask = (mask_m[ 0] << 30) | (mask_m[ 1] << 28)
          | (mask_m[ 2] << 26) | (mask_m[ 3] << 24)
          | (mask_m[ 4] << 22) | (mask_m[ 5] << 20)
          | (mask_m[ 6] << 18) | (mask_m[ 7] << 16)
          | (mask_m[ 8] << 14) | (mask_m[ 9] << 12)
          | (mask_m[10] << 10) | (mask_m[11] <<  8)
          | (mask_m[12] <<  6) | (mask_m[13] <<  4)
          | (mask_m[14] <<  2) | (mask_m[15] <<  0);
    OS_REG_WRITE(ah, AR_PHY_MASK_CTL, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_M_00_15, tmp_mask);

    tmp_mask =             (mask_p[15] << 28)
          | (mask_p[14] << 26) | (mask_p[13] << 24)
          | (mask_p[12] << 22) | (mask_p[11] << 20)
          | (mask_p[10] << 18) | (mask_p[ 9] << 16)
          | (mask_p[ 8] << 14) | (mask_p[ 7] << 12)
          | (mask_p[ 6] << 10) | (mask_p[ 5] <<  8)
          | (mask_p[ 4] <<  6) | (mask_p[ 3] <<  4)
          | (mask_p[ 2] <<  2) | (mask_p[ 1] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_1, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_P_15_01, tmp_mask);

    tmp_mask =             (mask_p[30] << 28)
          | (mask_p[29] << 26) | (mask_p[28] << 24)
          | (mask_p[27] << 22) | (mask_p[26] << 20)
          | (mask_p[25] << 18) | (mask_p[24] << 16)
          | (mask_p[23] << 14) | (mask_p[22] << 12)
          | (mask_p[21] << 10) | (mask_p[20] <<  8)
          | (mask_p[19] <<  6) | (mask_p[18] <<  4)
          | (mask_p[17] <<  2) | (mask_p[16] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_2, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_P_30_16, tmp_mask);

    tmp_mask =             (mask_p[45] << 28)
          | (mask_p[44] << 26) | (mask_p[43] << 24)
          | (mask_p[42] << 22) | (mask_p[41] << 20)
          | (mask_p[40] << 18) | (mask_p[39] << 16)
          | (mask_p[38] << 14) | (mask_p[37] << 12)
          | (mask_p[36] << 10) | (mask_p[35] <<  8)
          | (mask_p[34] <<  6) | (mask_p[33] <<  4)
          | (mask_p[32] <<  2) | (mask_p[31] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_3, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_P_45_31, tmp_mask);

    tmp_mask = (mask_p[61] << 30) | (mask_p[60] << 28)
          | (mask_p[59] << 26) | (mask_p[58] << 24)
          | (mask_p[57] << 22) | (mask_p[56] << 20)
          | (mask_p[55] << 18) | (mask_p[54] << 16)
          | (mask_p[53] << 14) | (mask_p[52] << 12)
          | (mask_p[51] << 10) | (mask_p[50] <<  8)
          | (mask_p[49] <<  6) | (mask_p[48] <<  4)
          | (mask_p[47] <<  2) | (mask_p[46] <<  0);
    OS_REG_WRITE(ah, AR_PHY_BIN_MASK2_4, tmp_mask);
    OS_REG_WRITE(ah, AR_PHY_MASK2_P_61_45, tmp_mask);
    OS_REG_WRITE_FLUSH(ah);

	DISABLE_REG_WRITE_BUFFER
}

/*
 * Set a limit on the overall output power.  Used for dynamic
 * transmit power control and the like.
 *
 * NB: limit is in units of 0.5 dbM.
 */
HAL_BOOL
ar5416SetTxPowerLimit(struct ath_hal *ah, u_int32_t limit, u_int16_t extra_txpow, u_int16_t tpcInDb)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CHANNEL_INTERNAL *ichan = ahpriv->ah_curchan;
    HAL_CHANNEL *chan = (HAL_CHANNEL *)ichan;

    if (NULL == chan) {
        return AH_FALSE;
    }

    ahpriv->ah_power_limit = AH_MIN(limit, MAX_RATE_POWER);
    ahpriv->ah_extra_txpow = extra_txpow;

    if(chan == NULL) {
    	return AH_FALSE;
    }
    
    if (ar5416EepromSetTransmitPower(ah, &ahp->ah_eeprom, ichan,
        ath_hal_getctl(ah, chan), ath_hal_getantennaallowed(ah, chan),
        chan->max_reg_tx_power * 2,
        AH_MIN(MAX_RATE_POWER, ahpriv->ah_power_limit)) != HAL_OK)
        return AH_FALSE;

    return AH_TRUE;
}

void ar5416GetChipMinAndMaxPowers(struct ath_hal *ah, int8_t *max_tx_power,
        int8_t *min_tx_power)
{
    return;
}

void ar5416FillHalChansFromRegdb(struct ath_hal *ah,
        uint16_t freq,
        int8_t maxregpower,
        int8_t maxpower,
        int8_t minpower,
        uint32_t flags,
        int index)
{
    return;
}

void ar5416Setsc(struct ath_hal *ah, HAL_SOFTC sc)
{
    AH_PRIVATE(ah)->ah_sc = sc;
}
/*
 * Exported call to check for a recent gain reading and return
 * the current state of the thermal calibration gain engine.
 */
HAL_RFGAIN
ar5416GetRfgain(struct ath_hal *ah)
{
    return HAL_RFGAIN_INACTIVE;
}

#ifdef AH_SUPPORT_2133
/*
 * Perform analog "swizzling" of parameters into their location
 */
void
ar5416ModifyRfBuffer(u_int32_t *rfBuf, u_int32_t reg32, u_int32_t numBits,
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
#endif /* AH_SUPPORT_2133 */

void
ar5416GetChannelCenters(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan,
                        CHAN_CENTERS *centers)
{
    int8_t      extoff;
    struct ath_hal_5416 *ahp = AH5416(ah);

    if (!IS_CHAN_HT40(chan)) {
        centers->ctl_center = centers->ext_center =
        centers->synth_center = chan->channel;
        return;
    }

    HALASSERT(IS_CHAN_HT40(chan));

    /*
     * In 20/40 phy mode, the center frequency is
     * "between" the primary and extension channels.
     */
    if (chan->channel_flags & CHANNEL_HT40PLUS) {
        centers->synth_center = chan->channel + HT40_CHANNEL_CENTER_SHIFT;
        extoff = 1;
    }
    else {
        centers->synth_center = chan->channel - HT40_CHANNEL_CENTER_SHIFT;
        extoff = -1;
    }

     centers->ctl_center = centers->synth_center - (extoff *
                            HT40_CHANNEL_CENTER_SHIFT);
     centers->ext_center = centers->synth_center + (extoff *
                ((ahp->ah_extprotspacing == HAL_HT_EXTPROTSPACING_20)? HT40_CHANNEL_CENTER_SHIFT : 15));

}

#define IS(_c,_f)       (((_c)->channel_flags & _f) || 0)

static inline HAL_CHANNEL_INTERNAL*
ar5416CheckChan(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    if ((IS(chan, CHANNEL_2GHZ) ^ IS(chan, CHANNEL_5GHZ)) == 0) {
        HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel %u/0x%x; not marked as "
                 "2GHz or 5GHz\n", __func__,
                chan->channel, chan->channel_flags);
        return AH_NULL;
    }

    if ((IS(chan, CHANNEL_OFDM) ^ IS(chan, CHANNEL_CCK) ^
         IS(chan, CHANNEL_HT20) ^ IS(chan, CHANNEL_HT40PLUS) ^ IS(chan, CHANNEL_HT40MINUS)) == 0) {
        HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel %u/0x%x; not marked as "
                "OFDM or CCK or HT20 or HT40PLUS or HT40MINUS\n", __func__,
                chan->channel, chan->channel_flags);
        return AH_NULL;
    }

    return (ath_hal_checkchannel(ah, chan));
}
#undef IS

/*
 * TODO: Only write the PLL if we're changing to or from CCK mode
 *
 * WARNING: The order of the PLL and mode registers must be correct.
 */
static inline void
ar5416SetRfMode(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t rfMode = 0;

    if (chan == AH_NULL)
        return;

    switch (AH5416(ah)->ah_hwp) {
        case HAL_TRUE_CHIP:
            rfMode |= (IS_CHAN_B(chan) || IS_CHAN_G(chan))
                      ? AR_PHY_MODE_DYNAMIC : AR_PHY_MODE_OFDM;
            break;
        default:
            HALASSERT(0);
            break;
    }

    if (!AR_SREV_MERLIN_10_OR_LATER(ah)) {
        rfMode |= (IS_CHAN_5GHZ(chan)) ? AR_PHY_MODE_RF5GHZ : AR_PHY_MODE_RF2GHZ;
    }

    /* Merlin 2.0/2.1 - Phy mode bits for 5GHz channels requiring Fast Clock */
    if (AR_SREV_MERLIN_20(ah) && IS_5GHZ_FAST_CLOCK_EN(ah, chan)) {
        rfMode |= (AR_PHY_MODE_DYNAMIC | AR_PHY_MODE_DYN_CCK_DISABLE);
    }

    OS_REG_WRITE(ah, AR_PHY_MODE, rfMode);
}

static inline HAL_STATUS
ar5416ProcessIni(struct ath_hal *ah, HAL_CHANNEL *chan,
                 HAL_CHANNEL_INTERNAL *ichan,
                 HAL_HT_MACMODE macmode)
{
    int i, regWrites = 0;
    struct ath_hal_5416 *ahp = AH5416(ah);
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    u_int modesIndex, freqIndex;
	HAL_CAPABILITIES *pCap = &ahpriv->ah_caps;
    HAL_STATUS status;

    /* Setup the indices for the next set of register array writes */
    switch (chan->channel_flags & CHANNEL_ALL) {
        /* TODO:
         * If the channel marker is indicative of the current mode rather
         * than capability, we do not need to check the phy mode below.
         */
        case CHANNEL_A:
        case CHANNEL_A_HT20:
            modesIndex = 1;
            freqIndex  = 1;
            break;

        case CHANNEL_A_HT40PLUS:
        case CHANNEL_A_HT40MINUS:
            modesIndex = 2;
            freqIndex  = 1;
            break;

        case CHANNEL_PUREG:
        case CHANNEL_G_HT20:
        case CHANNEL_B:
            modesIndex = 4;
            freqIndex  = 2;
            break;

        case CHANNEL_G_HT40PLUS:
        case CHANNEL_G_HT40MINUS:
            modesIndex = 3;
            freqIndex  = 2;
            break;

        case CHANNEL_108G:
            modesIndex = 5;
            freqIndex  = 2;
            break;

        default:
            HALASSERT(0);
            return HAL_EINVAL;
    }

    /* Set correct Baseband to analog shift setting to access analog chips. */
    OS_REG_WRITE(ah, AR_PHY(0), 0x00000007);

    /*
     * Write addac shifts
    */
    if (AH5416(ah)->ah_hwp == HAL_TRUE_CHIP) {
        OS_REG_WRITE(ah, AR_PHY_ADC_SERIAL_CTL, AR_PHY_SEL_EXTERNAL_RADIO);

        ar5416EepromSetAddac(ah, ichan);

        if (AR_SREV_5416_V22_OR_LATER(ah)) {
            REG_WRITE_ARRAY(&ahp->ah_iniAddac, 1, regWrites);
        } else {
        struct ar5416IniArray temp;
        u_int32_t addacSize = sizeof(u_int32_t) * ahp->ah_iniAddac.ia_rows * ahp->ah_iniAddac.ia_columns;

        /* Owl 2.1/2.0 */
        OS_MEMCPY(ahp->ah_addacOwl21, ahp->ah_iniAddac.ia_array, addacSize);

        /* override CLKDRV value at [row, column] = [31, 1] */
        (ahp->ah_addacOwl21)[31 *  ahp->ah_iniAddac.ia_columns + 1] = 0;

        temp.ia_array = ahp->ah_addacOwl21;
        temp.ia_columns = ahp->ah_iniAddac.ia_columns;
        temp.ia_rows = ahp->ah_iniAddac.ia_rows;
        REG_WRITE_ARRAY(&temp, 1, regWrites);
        }
        OS_REG_WRITE(ah, AR_PHY_ADC_SERIAL_CTL, AR_PHY_SEL_INTERNAL_ADDAC);
    }

    /*
    ** We need to expand the REG_WRITE_ARRAY macro here to ensure we can insert the 100usec
    ** delay if required.  This is to ensure any writes to the 0x78xx registers have the
    ** proper delay implemented in the Merlin case
    */
    ENABLE_REG_WRITE_BUFFER
    for (i = 0; i < ahp->ah_iniModes.ia_rows; i++) {
        u_int32_t reg = INI_RA(&ahp->ah_iniModes, i, 0);
        u_int32_t val = INI_RA(&ahp->ah_iniModes, i, modesIndex);

        if ( (AH_PRIVATE(ah)->ah_devid == AR5416_DEVID_AR9280_PCI) &&
             (pCap->hal_wireless_modes & HAL_MODE_11A) )
            val = ar5416INIFixup(ah, &ahp->ah_eeprom, reg, val);

        /*
        ** Determine if this is a shift register value, and insert the
        ** configured delay if so.
        */

        if (reg >= 0x7800 && reg < 0x7900) {
            analogShiftRegWrite(ah, reg, val);
        } else {
            OS_REG_WRITE(ah, reg, val);
        }

        WAR_6773(regWrites);
    }
	OS_REG_WRITE_FLUSH(ah);
	DISABLE_REG_WRITE_BUFFER

    /* Write rxgain Array Parameters */
    if (AR_SREV_MERLIN_20(ah) || AR_SREV_KIWI_10_OR_LATER(ah)) {
        REG_WRITE_ARRAY(&ahp->ah_iniModesRxgain, modesIndex, regWrites);
    }

    /* Write txgain Array Parameters */
    if (AR_SREV_MERLIN_20(ah) ||
        (AR_SREV_KITE(ah) && AR_SREV_KITE_12_OR_LATER(ah)) || AR_SREV_KIWI_10_OR_LATER(ah)) {
        REG_WRITE_ARRAY(&ahp->ah_iniModesTxgain, modesIndex, regWrites);
        /* change 0x7820[28:26](an_ch1_rf5g3_pdbictxpa) to 0x2 from 0x4 at A band 
           HT20 modesIndex = 1 and HT40 modesIndex = 2 for thermal issue*/
        if (AH_PRIVATE(ah)->ah_config.ath_hal_lowerHTtxgain) {
            if ((modesIndex == 1) || (modesIndex == 2)) {
                OS_REG_WRITE(ah, INI_RA(&(ahp->ah_iniModesTxgain), 29, 0), 0x8a592480);
            }
        }
    }

#ifdef AH_SUPPORT_K2
    if (AR_SREV_K2_10(ah)) {
        REG_WRITE_ARRAY(&ahp->ah_iniModes_K2_1_0_only, modesIndex, regWrites);
    }
#endif    

    ENABLE_REG_WRITE_BUFFER
    /* Write Common Array Parameters */
    for (i = 0; i < ahp->ah_iniCommon.ia_rows; i++) {
        u_int32_t reg = INI_RA(&ahp->ah_iniCommon, i, 0);
        u_int32_t val = INI_RA(&ahp->ah_iniCommon, i, 1);

        /*
        ** Determine if this is a shift register value, and insert the
        ** configured delay if so.
        */

        if (reg >= 0x7800 && reg < 0x7900) {
            analogShiftRegWrite(ah, reg, val);
        } else {
            OS_REG_WRITE(ah, reg, val);
        }

        WAR_6773(regWrites);
    }

#ifdef AH_SUPPORT_K2
    if (AR_SREV_K2(ah)) {
        ar9271CCKfirCoeff(ah, chan);
    }

    if (AR_SREV_K2(ah) && (ar5416EepromGet(ahp, EEP_TXGAIN_TYPE) == 1)) {
		REG_WRITE_ARRAY(&ahp->ah_iniModes_high_power_tx_gain_K2, modesIndex, regWrites);
    } else {
        REG_WRITE_ARRAY(&ahp->ah_iniModes_normal_power_tx_gain_K2, modesIndex, regWrites);
    }
#endif

    OS_REG_WRITE_FLUSH(ah);
	DISABLE_REG_WRITE_BUFFER

    ahp->ah_rfHal.writeRegs(ah, modesIndex, freqIndex, regWrites);

    /* Merlin 2.0/2.1 - For 5GHz channels requiring Fast Clock, apply different modal values */
    if (AR_SREV_MERLIN_20(ah) && IS_5GHZ_FAST_CLOCK_EN(ah, chan)) {
        HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: Fast clock enabled, use special ini values\n", __func__);
        REG_WRITE_ARRAY(&ahp->ah_iniModesAdditional, modesIndex, regWrites);
    }

    /* Override INI with chip specific configuration */
    ar5416OverrideIni(ah, chan);

    /* Setup 11n MAC/Phy mode registers */
    ar5416Set11nRegs(ah, chan, macmode);

    /*
     * Moved ar5416InitChainMasks() here to ensure the swap bit is set before
     * the pdadc table is written.  Swap must occur before any radio dependent
     * replicated register access.  The pdadc curve addressing in particular
     * depends on the consistent setting of the swap bit.
     */
     ar5416InitChainMasks(ah);

     if ((AR_SREV_MERLIN_20(ah) || AR_SREV_KIWI_10_OR_LATER(ah)) &&
         ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
         ar5416OpenLoopPowerControlInit(ah);
         ar5416OpenLoopPowerControlTempCompensation(ah);
     }

    /*
     * Setup the transmit power values.
     *
     * After the public to private hal channel mapping, ichan contains the
     * valid regulatory power value.
     * ath_hal_getctl and ath_hal_getantennaallowed look up ichan from chan.
     */
    status = ar5416EepromSetTransmitPower(ah, &ahp->ah_eeprom, ichan,
             ath_hal_getctl(ah, chan), ath_hal_getantennaallowed(ah, chan),
             ichan->max_reg_tx_power * 2,
             AH_MIN(MAX_RATE_POWER, ahpriv->ah_power_limit));
    if (status != HAL_OK) {
        HDPRINTF(ah, HAL_DBG_POWER_MGMT, "%s: error init'ing transmit power\n", __func__);
        return HAL_EIO;
    }

    /* Write the analog registers */
    if (!ahp->ah_rfHal.setRfRegs(ah, ichan, freqIndex)) {
        HDPRINTF(ah, HAL_DBG_REG_IO, "%s: ar5416SetRfRegs failed\n", __func__);
        return HAL_EIO;
    }

    return HAL_OK;
}

void
ar5416InitPLL(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t pll;


    if (AR_SREV_HOWL(ah)) {

        if (chan && IS_CHAN_5GHZ(chan))
            pll = 0x1450;
        else
            pll = 0x1458;
    }
    else
    {
        if (AR_SREV_MERLIN_10_OR_LATER(ah)) {

            pll = SM(0x5, AR_RTC_SOWL_PLL_REFDIV);

            if (chan && IS_CHAN_HALF_RATE(chan)) {
                pll |= SM(0x1, AR_RTC_SOWL_PLL_CLKSEL);
            } else if (chan && IS_CHAN_QUARTER_RATE(chan)) {
                pll |= SM(0x2, AR_RTC_SOWL_PLL_CLKSEL);
            }
            if (chan && IS_CHAN_5GHZ(chan)) {
                pll |= SM(0x28, AR_RTC_SOWL_PLL_DIV);

                /* PLL WAR for Merlin 2.0/2.1
                 * When doing fast clock, set PLL to 0x142c
                 * Else, set PLL to 0x2850 to prevent reset-to-reset variation
                 */
                if (AR_SREV_MERLIN_20(ah)) {
                    if (IS_5GHZ_FAST_CLOCK_EN(ah, chan)) {
                        pll = 0x142c;
                    }
                    else {
                        pll = 0x2850;
                    }
                }
            } else {
                pll |= SM(0x2c, AR_RTC_SOWL_PLL_DIV);
            }

        } else if (AR_SREV_SOWL_10_OR_LATER(ah)) {

              pll = SM(0x5, AR_RTC_SOWL_PLL_REFDIV);

              if (chan && IS_CHAN_HALF_RATE(chan)) {
                  pll |= SM(0x1, AR_RTC_SOWL_PLL_CLKSEL);
              } else if (chan && IS_CHAN_QUARTER_RATE(chan)) {
                  pll |= SM(0x2, AR_RTC_SOWL_PLL_CLKSEL);
              }
              if (chan && IS_CHAN_5GHZ(chan)) {
                  pll |= SM(0x50, AR_RTC_SOWL_PLL_DIV);
              } else {
                  pll |= SM(0x58, AR_RTC_SOWL_PLL_DIV);
              }
        } else {
            pll = AR_RTC_PLL_REFDIV_5 | AR_RTC_PLL_DIV2;

            if (chan && IS_CHAN_HALF_RATE(chan)) {
                pll |= SM(0x1, AR_RTC_PLL_CLKSEL);
            } else if (chan && IS_CHAN_QUARTER_RATE(chan)) {
                pll |= SM(0x2, AR_RTC_PLL_CLKSEL);
            }
            if (chan && IS_CHAN_5GHZ(chan)) {
                pll |= SM(0xa, AR_RTC_PLL_DIV);
            } else {
                pll |= SM(0xb, AR_RTC_PLL_DIV);
        }
        }
    }
    OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, pll);

    if (AR_SREV_K2(ah)) {
        /* Set CPU Clock to 117 Mhz */
        OS_DELAY(500);
        OS_REG_WRITE(ah, AR9271_CLOCK_CONTROL, (AR9271_UART_SEL | AR9271_CLOCK_SELECTION_117));
    }

    /* TODO:
    * For multi-band owl, switch between bands by reiniting the PLL.
    */

    OS_DELAY(RTC_PLL_SETTLE_DELAY);

    OS_REG_WRITE(ah, AR_RTC_SLEEP_CLK, AR_RTC_FORCE_DERIVED_CLK);
}

static inline void
ar5416InitChainMasks(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int rx_chainmask, tx_chainmask;

    rx_chainmask = ahp->ah_rxchainmask;
    tx_chainmask = ahp->ah_txchainmask;

    /* Force the chainmask to be 1 in case the power save is on */
    if( AH_PRIVATE(ah)->green_ap_ps_on ) rx_chainmask = HAL_GREEN_AP_RX_MASK;

    ENABLE_REG_WRITE_BUFFER

    switch (rx_chainmask) {
        case 0x5:
            DISABLE_REG_WRITE_BUFFER
            OS_REG_SET_BIT(ah, AR_PHY_ANALOG_SWAP, AR_PHY_SWAP_ALT_CHAIN);
            ENABLE_REG_WRITE_BUFFER
            /*
             * fall through !
             */
        case 0x3:
            if ((AH5416(ah)->ah_hwp == HAL_TRUE_CHIP) &&
                ((AH_PRIVATE(ah))->ah_mac_version == AR_SREV_REVISION_OWL_10)) {
                /*
                 * workaround for OWL 1.0 cal failure, always cal 3 chains for
                 * multi chain -- then after cal set true mask value
                 */
                OS_REG_WRITE(ah, AR_PHY_RX_CHAINMASK, 0x7);
                OS_REG_WRITE(ah, AR_PHY_CAL_CHAINMASK, 0x7);
                break;
            }
            /*
             * fall through !
             */
        case 0x1:
        case 0x2: /* Chainmask 0x2 included for Merlin antenna selection */
        case 0x7:
            OS_REG_WRITE(ah, AR_PHY_RX_CHAINMASK, rx_chainmask);
            OS_REG_WRITE(ah, AR_PHY_CAL_CHAINMASK, rx_chainmask);
            break;
        default:
            break;
    }

    OS_REG_WRITE(ah, AR_SELFGEN_MASK, tx_chainmask);
    OS_REG_WRITE_FLUSH(ah);

    DISABLE_REG_WRITE_BUFFER

    if (tx_chainmask == 0x5) {
        OS_REG_SET_BIT(ah, AR_PHY_ANALOG_SWAP, AR_PHY_SWAP_ALT_CHAIN);
    }

    /* if DCS is active, force CHAIN SWAP */
    if (ah->ah_get_dcs_mode(ah)) {
        u_int32_t cap_chainmask = 0;
        if ((ah->ah_get_capability(ah, HAL_CAP_TX_CHAINMASK, 0, &cap_chainmask)
                == HAL_OK ) && cap_chainmask == 7) {
            OS_REG_SET_BIT(ah, AR_PHY_ANALOG_SWAP, AR_PHY_SWAP_ALT_CHAIN);
        }
    }
    if (AR_SREV_HOWL(ah)) {
        OS_REG_WRITE(ah, AR_PHY_ANALOG_SWAP,
            OS_REG_READ(ah, AR_PHY_ANALOG_SWAP) | 0x00000001);
    }
#ifdef ATH_BT_COEX
     /* Update chainmask setting for BT coex */
     if (ahp->ah_btModule == HAL_BT_MODULE_HELIUS) {
         OS_REG_WRITE(ah, AR_SELFGEN_MASK, HAL_BT_COEX_HELIUS_CHAINMASK);
     }
#endif
}

/* ar5416IsCalSupp
 * Determine if calibration is supported by device and channel flags
 */
HAL_BOOL
ar5416IsCalSupp(struct ath_hal *ah, HAL_CHANNEL *chan, HAL_CAL_TYPES calType)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    HAL_BOOL retval = AH_FALSE;

    switch(calType & ahp->ah_suppCals) {
    case IQ_MISMATCH_CAL:
        /* Run IQ Mismatch for non-CCK only */
        if (!IS_CHAN_B(chan))
            {retval = AH_TRUE;}
        break;
    case ADC_GAIN_CAL:
    case ADC_DC_CAL:
        /* Run ADC Gain Cal for non-CCK & non 2GHz-HT20 only */
        if (!IS_CHAN_B(chan) && !(IS_CHAN_2GHZ(chan) && IS_CHAN_HT20(chan)))
            {retval = AH_TRUE;}
        break;
    }

    return retval;
}


#ifdef AH_SUPPORT_KITE_ANY
/* ar9285PACal
 * PA Calibration for Kite 1.1 and later versions of Kite.
 * - from system's team.
 */
static inline void
ar9285PACal(struct ath_hal *ah, HAL_BOOL isReset)
{

    u_int32_t regVal;
    int i, offset, offs_6_1, offs_0;
    u_int32_t ccomp_org;
    u_int32_t regList [][2] = {
        { 0x786c, 0 },
        { 0x7854, 0 },
        { 0x7820, 0 },
        { 0x7824, 0 },
        { 0x7868, 0 },
        { 0x783c, 0 },
        { 0x7838, 0 } ,
    };
    struct ath_hal_5416 *ahp = AH5416(ah);
    struct ath_hal_private  *ahpriv = AH_PRIVATE(ah);

    /* No need to do PA Cal for high power solution */
    if (ar5416EepromGet(ahp, EEP_TXGAIN_TYPE) == AR5416_EEP_TXGAIN_HIGH_POWER)
        return;

    /* Kite 1.1 WAR for Bug 35666
     * Increase the LDO value to 1.28V before accessing analog Reg */
    if (AR_SREV_KITE_11(ah)) {
        OS_REG_WRITE(ah, AR9285_AN_TOP4, (AR9285_AN_TOP4_DEFAULT | 0x14) );
        OS_DELAY(10);
    }

   for (i = 0; i < N(regList); i++) {
       regList[i][1] = OS_REG_READ(ah, regList[i][0]);
   }

   regVal = OS_REG_READ(ah, 0x7834);
   regVal &= (~(0x1));
   OS_REG_WRITE(ah, 0x7834, regVal);
   regVal = OS_REG_READ(ah, 0x9808);
   regVal |= (0x1 << 27);
   OS_REG_WRITE(ah, 0x9808, regVal);

    if((AH_PRIVATE(ah)->ah_opmode == HAL_M_STA) && 
        AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable) {
        OS_REG_WRITE(ah, AR9285_AN_RF2G3, AH_PRIVATE(ah)->ah_ob_db1[0]);
        OS_REG_WRITE(ah, AR9285_AN_RF2G4, AH_PRIVATE(ah)->ah_db2[0]);
    }
      
   OS_REG_RMW_FIELD(ah, AR9285_AN_TOP3, AR9285_AN_TOP3_PWDDAC, 1); // pwddac=1
   OS_REG_RMW_FIELD(ah, AR9285_AN_RXTXBB1, AR9285_AN_RXTXBB1_PDRXTXBB1, 1); // pdrxtxbb=1
   OS_REG_RMW_FIELD(ah, AR9285_AN_RXTXBB1, AR9285_AN_RXTXBB1_PDV2I, 1); // pdv2i=1
   OS_REG_RMW_FIELD(ah, AR9285_AN_RXTXBB1, AR9285_AN_RXTXBB1_PDDACIF, 1); // pddacinterface=1
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G2, AR9285_AN_RF2G2_OFFCAL, 0); // offcal=0
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G7, AR9285_AN_RF2G7_PWDDB, 0);   // pwddb=0
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G1, AR9285_AN_RF2G1_ENPACAL, 0); // enpacal=0
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G1, AR9285_AN_RF2G1_PDPADRV1, 0); // pdpadrv1=0
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G1,AR9285_AN_RF2G1_PDPADRV2,0); // pdpadrv2=0
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G1, AR9285_AN_RF2G1_PDPAOUT, 0); // pdpaout=0
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G8,AR9285_AN_RF2G8_PADRVGN2TAB0,7); // padrvgn2tab_0=7
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G7,AR9285_AN_RF2G7_PADRVGN2TAB0,0); // padrvgn1tab_0=0 - does not matter since we turn it off
   ccomp_org=OS_REG_READ_FIELD(ah, AR9285_AN_RF2G6,AR9285_AN_RF2G6_CCOMP);
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G6,AR9285_AN_RF2G6_CCOMP,0xf); // ccomp=f - shares reg with off_6_1

   /* Set localmode=1,bmode=1,bmoderxtx=1,synthon=1,txon=1,paon=1,oscon=1,synthon_force=1 */
   OS_REG_WRITE(ah, AR9285_AN_TOP2, 0xca0358a0);
   OS_DELAY(30);
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G6,AR9285_AN_RF2G6_OFFS,0);
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G3,AR9285_AN_RF2G3_PDVCCOMP,0); //clear off[6:0]

   /* find off_6_1; */
   for (i = 6;i > 0;i--)
   {
       regVal = OS_REG_READ(ah, 0x7834);
       regVal |= (1 << (19 + i));
       OS_REG_WRITE(ah, 0x7834, regVal);
       OS_DELAY(1);
       regVal = OS_REG_READ(ah, 0x7834);
       regVal &= (~(0x1 << (19 + i)));
       regVal |= ((OS_REG_READ_FIELD(ah, 0x7840, AR9285_AN_RXTXBB1_SPARE9)) << (19 + i));
       OS_REG_WRITE(ah, 0x7834, regVal);
   }

   /* find off_0; */
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G3,AR9285_AN_RF2G3_PDVCCOMP,1);
   OS_DELAY(1);
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G3,AR9285_AN_RF2G3_PDVCCOMP,OS_REG_READ_FIELD(ah, AR9285_AN_RF2G9,AR9285_AN_RXTXBB1_SPARE9));
   offs_6_1 = OS_REG_READ_FIELD(ah, AR9285_AN_RF2G6, AR9285_AN_RF2G6_OFFS);
   offs_0   = OS_REG_READ_FIELD(ah, AR9285_AN_RF2G3, AR9285_AN_RF2G3_PDVCCOMP);

   /*  Empirical offset correction  */
   offset= (offs_6_1<<1)|offs_0;
   offset = offset - 0; // here is the correction
   offs_6_1 = offset>>1;
   offs_0 = offset & 1;

   /* Update PA cal info */
   if ((!isReset) && (ahpriv->ah_paCalInfo.prevOffset == offset)) {
       if (ahpriv->ah_paCalInfo.maxSkipCount < MAX_PACAL_SKIPCOUNT)
           ahpriv->ah_paCalInfo.maxSkipCount = 2*ahpriv->ah_paCalInfo.maxSkipCount;
       ahpriv->ah_paCalInfo.skipCount = ahpriv->ah_paCalInfo.maxSkipCount;
   } else {
       ahpriv->ah_paCalInfo.maxSkipCount = 1;
       ahpriv->ah_paCalInfo.skipCount = 0;
       ahpriv->ah_paCalInfo.prevOffset = offset;
   }

   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G6, AR9285_AN_RF2G6_OFFS, offs_6_1);
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G3, AR9285_AN_RF2G3_PDVCCOMP, offs_0);

   regVal = OS_REG_READ(ah, 0x7834);
   regVal |= 0x1;
   OS_REG_WRITE(ah, 0x7834, regVal);
   regVal = OS_REG_READ(ah, 0x9808);
   regVal &= (~(0x1 << 27));
   OS_REG_WRITE(ah, 0x9808, regVal);

   for (i = 0; i < N(regList); i++) {
       OS_REG_WRITE(ah, regList[i][0], regList[i][1]);
   }
   // Restore Registers to original value
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G6,AR9285_AN_RF2G6_CCOMP,ccomp_org);

    /* Kite 1.1 WAR for Bug 35666
     * Decrease the LDO value back to 1.20V */
    if (AR_SREV_KITE_11(ah))
        OS_REG_WRITE(ah, AR9285_AN_TOP4, AR9285_AN_TOP4_DEFAULT);

}
#endif /* AH_SUPPORT_KITE_ANY */

#ifdef AH_SUPPORT_K2
/* ar9271PACal
 * PA Calibration for k2 1.0.
 * - from system's team.
 */
static inline void
ar9271PACal(struct ath_hal *ah, HAL_BOOL isReset)
{

    u_int32_t regVal;
    int i, j;
    static u_int32_t regList [][2] = {
        { 0x786c, 0 },
        { 0x7854, 0 },
        { 0x7820, 0 },
        { 0x7824, 0 },
        { 0x7868, 0 },
        { 0x783c, 0 },
        { 0x7838, 0 } ,
        { 0x7828, 0 } ,
        { 0x782c, 0 } ,
    };
    struct ath_hal_private  *ahpriv = AH_PRIVATE(ah);

   for (i = 0; i < N(regList); i++) {
       regList[i][1] = OS_REG_READ(ah, regList[i][0]);
   }

   regVal = OS_REG_READ(ah, 0x7834);
   regVal &= (~(0x1));
   OS_REG_WRITE(ah, 0x7834, regVal);
   regVal = OS_REG_READ(ah, 0x9808);
   regVal |= (0x1 << 27);
   OS_REG_WRITE(ah, 0x9808, regVal);

    if (AR_SREV_K2(ah) && 
        (AH_PRIVATE(ah)->ah_opmode == HAL_M_STA) && 
        (AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable)) {
        OS_REG_WRITE(ah, AR9285_AN_RF2G3, AH_PRIVATE(ah)->ah_ob_db1[0]);
        OS_REG_WRITE(ah, AR9285_AN_RF2G4, AH_PRIVATE(ah)->ah_db2[0]);
    }

   OS_REG_RMW_FIELD(ah, AR9285_AN_TOP3, AR9285_AN_TOP3_PWDDAC, 1);          // 786c,b23,1, pwddac=1
   OS_REG_RMW_FIELD(ah, AR9285_AN_RXTXBB1, AR9285_AN_RXTXBB1_PDRXTXBB1, 1); // 7854, b5,1, pdrxtxbb=1
   OS_REG_RMW_FIELD(ah, AR9285_AN_RXTXBB1, AR9285_AN_RXTXBB1_PDV2I, 1);     // 7854, b7,1, pdv2i=1
   OS_REG_RMW_FIELD(ah, AR9285_AN_RXTXBB1, AR9285_AN_RXTXBB1_PDDACIF, 1);   // 7854, b8,1, pddacinterface=1
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G2, AR9285_AN_RF2G2_OFFCAL, 0);        // 7824,b12,0, offcal=0
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G7, AR9285_AN_RF2G7_PWDDB, 0);         // 7838, b1,0, pwddb=0
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G1, AR9285_AN_RF2G1_ENPACAL, 0);       // 7820,b11,0, enpacal=0
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G1, AR9285_AN_RF2G1_PDPADRV1, 0);      // 7820,b25,1, pdpadrv1=0
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G1,AR9285_AN_RF2G1_PDPADRV2,0);        // 7820,b24,0, pdpadrv2=0
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G1, AR9285_AN_RF2G1_PDPAOUT, 0);       // 7820,b23,0, pdpaout=0
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G8,AR9285_AN_RF2G8_PADRVGN2TAB0,7);    // 783c,b14-16,7, padrvgn2tab_0=7
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G7,AR9285_AN_RF2G7_PADRVGN2TAB0,0);    // 7838,b29-31,0, padrvgn1tab_0=0 - does not matter since we turn it off

   //isK2SW
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G3, AR9271_AN_RF2G3_CCOMP, 0xfff);

   /* Set localmode=1,bmode=1,bmoderxtx=1,synthon=1,txon=1,paon=1,oscon=1,synthon_force=1 */
   OS_REG_WRITE(ah, AR9285_AN_TOP2, 0xca0358a0);
   OS_DELAY(30);
   OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G6, AR9271_AN_RF2G6_OFFS, 0);

   /* find off_6_1; */
   for (i = 6;i >= 0;i--)
   {
       regVal = OS_REG_READ(ah, 0x7834);
       regVal |= (1 << (20 + i));
       OS_REG_WRITE(ah, 0x7834, regVal);
       OS_DELAY(1);
       if (i==6) {
           j = 0;
           while ((OS_REG_READ(ah, 0x7840) & 0x1) == 1 && j < 10) {
               j++;
               OS_REG_RMW_FIELD(ah, AR9285_AN_RF2G8,AR9285_AN_RF2G8_PADRVGN2TAB0,3);
               OS_REG_WRITE(ah, 0x7824, (OS_REG_READ(ah, 0x7824) & ~(0x7<<9)) | 0x3<<9 );
               OS_DELAY(100*j);
           }
       }
       //regVal = OS_REG_READ(ah, 0x7834);
       regVal &= (~(0x1 << (20 + i)));
       regVal |= ((OS_REG_READ_FIELD(ah, 0x7840, AR9285_AN_RXTXBB1_SPARE9)) << (20 + i));
       OS_REG_WRITE(ah, 0x7834, regVal);
   }
   regVal = (regVal >>20) & 0x7f;

   /* Update PA cal info */
   if ((!isReset) && (ahpriv->ah_paCalInfo.prevOffset == regVal)) {
       if (ahpriv->ah_paCalInfo.maxSkipCount < MAX_PACAL_SKIPCOUNT)
           ahpriv->ah_paCalInfo.maxSkipCount = 2*ahpriv->ah_paCalInfo.maxSkipCount;
       ahpriv->ah_paCalInfo.skipCount = ahpriv->ah_paCalInfo.maxSkipCount;
   } else {
       ahpriv->ah_paCalInfo.maxSkipCount = 1;
       ahpriv->ah_paCalInfo.skipCount = 0;
       ahpriv->ah_paCalInfo.prevOffset = regVal;
   }

   ENABLE_REG_WRITE_BUFFER

   regVal = OS_REG_READ(ah, 0x7834);
   regVal |= 0x1;
   OS_REG_WRITE(ah, 0x7834, regVal);
   regVal = OS_REG_READ(ah, 0x9808);
   regVal &= (~(0x1 << 27));
   OS_REG_WRITE(ah, 0x9808, regVal);

   for (i = 0; i < N(regList); i++) {
       OS_REG_WRITE(ah, regList[i][0], regList[i][1]);
   }
   OS_REG_WRITE_FLUSH(ah);

   DISABLE_REG_WRITE_BUFFER
}

/* ar9271CCKfirCoeff
 * For Japanese regulatory requirements, 2484 MHz requires the following three
 * registers be programmed differently from the channel between 2412 and 2472
 * MHz.
 * - from system's team.
 */
static inline void
ar9271CCKfirCoeff(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int i, regWrites = 0;;

    if (chan->channel != 2484) {
        /* Write Common_normal_cck_fir_coeff_K2 Array Parameters */
        for (i = 0; i < ahp->ah_iniCommon_normal_cck_fir_coeff_K2.ia_rows; i++) {
            u_int32_t reg = INI_RA(&ahp->ah_iniCommon_normal_cck_fir_coeff_K2, i, 0);
            u_int32_t val = INI_RA(&ahp->ah_iniCommon_normal_cck_fir_coeff_K2, i, 1);
     
            OS_REG_WRITE(ah, reg, val);
    
            /*
            ** Determine if this is a shift register value, and insert the
            ** configured delay if so.
            */
            if (reg >= 0x7800 && reg < 0x7900) {
                analogShiftRegWrite(ah, reg, val);
            } else {
                OS_REG_WRITE(ah, reg, val);
            }

            WAR_6773(regWrites);
        }        
    } else {
        /* Write Common_japan_2484_cck_fir_coeff_K2 Array Parameters */
        for (i = 0; i < ahp->ah_iniCommon_japan_2484_cck_fir_coeff_K2.ia_rows; i++) {
            u_int32_t reg = INI_RA(&ahp->ah_iniCommon_japan_2484_cck_fir_coeff_K2, i, 0);
            u_int32_t val = INI_RA(&ahp->ah_iniCommon_japan_2484_cck_fir_coeff_K2, i, 1);
     
            OS_REG_WRITE(ah, reg, val);
    
            /*
            ** Determine if this is a shift register value, and insert the
            ** configured delay if so.
            */
            if (reg >= 0x7800 && reg < 0x7900) {
                analogShiftRegWrite(ah, reg, val);
            } else {
                OS_REG_WRITE(ah, reg, val);
            }

            WAR_6773(regWrites);
        }
    }
}

static void     
ar9271LoadAniReg(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int16_t i; 
    u_int modesIndex;

    switch (chan->channel_flags & CHANNEL_ALL) {
        case CHANNEL_A:
        case CHANNEL_A_HT20:
            modesIndex = 1;
            break;

        case CHANNEL_A_HT40PLUS:
        case CHANNEL_A_HT40MINUS:
            modesIndex = 2;
            break;

        case CHANNEL_PUREG:
        case CHANNEL_G_HT20:
        case CHANNEL_B:
            modesIndex = 4;
            break;

        case CHANNEL_G_HT40PLUS:
        case CHANNEL_G_HT40MINUS:
            modesIndex = 3;
            break;

        case CHANNEL_108G:
            modesIndex = 5;
            break;

        default:
            HALASSERT(0);
            return;
    }      

    for (i = 0; i < ahp->ah_iniModes_K2_ANI_reg.ia_rows; i++) {
 
        u_int32_t reg = INI_RA(&ahp->ah_iniModes_K2_ANI_reg, i, 0);
        u_int32_t val = INI_RA(&ahp->ah_iniModes_K2_ANI_reg, i, modesIndex);
        u_int32_t val_orig;

        if ( reg == AR_PHY_CCK_DETECT ) {
            val_orig = OS_REG_READ(ah, reg);
            val &= AR_PHY_CCK_DETECT_WEAK_SIG_THR_CCK;
            val_orig &= ~AR_PHY_CCK_DETECT_WEAK_SIG_THR_CCK;
            OS_REG_WRITE(ah, reg, val|val_orig);
        } else {
            OS_REG_WRITE(ah, reg, val);
        }
    }

    OS_REG_WRITE_FLUSH(ah);
}
#endif /* AH_SUPPORT_K2 */

/* ar5416RunInitCals
 * Runs non-periodic calibrations
 */
inline HAL_BOOL
ar5416RunInitCals(struct ath_hal *ah, int init_cal_count)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    HAL_CHANNEL_INTERNAL ichan; // bogus
    HAL_BOOL isCalDone;
    HAL_CAL_LIST *currCal = ahp->ah_cal_list_curr;
    int i;

	/* Modify for static analysis, prevent currCal is NULL */
    if (currCal == AH_NULL)
        return AH_FALSE;

    ichan.cal_valid=0;

    for (i=0; i < init_cal_count; i++) {
        /* Reset this Cal */
        ar5416ResetCalibration(ah, currCal);
        /* Poll for offset calibration complete */
        if (!ath_hal_wait(ah, AR_PHY_TIMING_CTRL4(0),
            AR_PHY_TIMING_CTRL4_DO_CAL, 0, AH_WAIT_TIMEOUT)) {
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                     "%s: Cal %d failed to complete in 100ms.\n",
                     __func__, currCal->calData->calType);
            /* Re-initialize list pointers for periodic cals */
            ahp->ah_cal_list = ahp->ah_cal_list_last = ahp->ah_cal_list_curr
               = AH_NULL;
            return AH_FALSE;
        }
        /* Run this cal */
        ar5416PerCalibration(ah, &ichan, ahp->ah_rxchainmask,
                             currCal, &isCalDone);
        if (isCalDone == AH_FALSE) {
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                     "%s: Not able to run Init Cal %d.\n", __func__,
                     currCal->calData->calType);
        }
        if (currCal->calNext) {
            currCal = currCal->calNext;
        }
    }

    /* Re-initialize list pointers for periodic cals */
    ahp->ah_cal_list = ahp->ah_cal_list_last = ahp->ah_cal_list_curr = AH_NULL;
    return AH_TRUE;
}

#define AR9285_MAX_TX_GAIN_TBL_SIZE      22
#define AR9285_MAX_CAL_TX_GAIN_TBL_SIZE  22
#define AR9285_MAX_BB_GAIN_TBL_SZIE      22

static inline HAL_BOOL 
ar9285ClCal(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int8_t  tx_gain_max, cal_cnt, new_bb_gain;
    u_int32_t tx_gain[AR9285_MAX_TX_GAIN_TBL_SIZE];
    u_int32_t cal_tx_gain[AR9285_MAX_CAL_TX_GAIN_TBL_SIZE];
    u_int8_t  bb_gain[AR9285_MAX_BB_GAIN_TBL_SZIE];
    int      i, j;
 
    /* Enable carrier leakage cal */
    OS_REG_SET_BIT(ah, AR_PHY_CL_CAL_CTL, AR_PHY_CL_CAL_ENABLE);

    /* Since there is a circuit design bug in the HT20 condition, here, a work-around solution is implemented for both HT20 and HT40 */
    /* Clear carrier result  */
    for (i = 0; i < 9; i++) {
        OS_REG_WRITE(ah, (AR_PHY_CLC_TBL1 + i * 4), 0);
    }

    /* Check if tx gain is the same */        
    tx_gain_max = (OS_REG_READ(ah, AR_PHY_TX_PWRCTRL7) & AR_PHY_TX_PWRCTRL_TX_GAIN_TAB_MAX) >> AR_PHY_TX_PWRCTRL_TX_GAIN_TAB_MAX_S;        
    cal_cnt     = 0;
    new_bb_gain = 0;

    if (tx_gain_max >= AR9285_MAX_TX_GAIN_TBL_SIZE) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s: invalid value of tx_gain_max(%d).\n",
                __func__, tx_gain_max);

        return AH_FALSE;
    }
    
    for (i = 0; i <= tx_gain_max; i++) {
        /* Save tx gain table, and bb gain */
        tx_gain[i] = OS_REG_READ(ah, (AR_PHY_TX_GAIN_TBL1 + i * 4));
        bb_gain[i] = (tx_gain[i] & AR_PHY_TX_GAIN_CLC) >> AR_PHY_TX_GAIN_CLC_S;
  
        /* Clear t/x gain table registers */
        OS_REG_WRITE(ah, (AR_PHY_TX_GAIN_TBL1 + i * 4), 0);
        
        if (i==0) {
            cal_tx_gain[cal_cnt] = tx_gain[i];
            cal_cnt++;
            new_bb_gain=1; 
        } else {
            new_bb_gain=0;

            for (j = 0; j < i; j++) {
                if (bb_gain[i] == bb_gain[j]) {
                    new_bb_gain=0;
                    break;
                } else {
                    cal_tx_gain[cal_cnt] = tx_gain[i];
                    new_bb_gain=1;         
                }
            }

            if (new_bb_gain==1) {
               cal_tx_gain[cal_cnt] = tx_gain[i];
               cal_cnt++;          
            }    
        }
    }

    /* Write valid tx gain table to bb reg to cal */
    for (i = 0; i < cal_cnt; i++) {
        OS_REG_WRITE(ah, (AR_PHY_TX_GAIN_TBL1 + i * 4) , cal_tx_gain[i]);
    }

    /* Write tx_table_max */
    OS_REG_WRITE(ah, AR_PHY_TX_PWRCTRL7, ((OS_REG_READ(ah, AR_PHY_TX_PWRCTRL7) & (~AR_PHY_TX_PWRCTRL_TX_GAIN_TAB_MAX)) | ((cal_cnt - 1) << AR_PHY_TX_PWRCTRL_TX_GAIN_TAB_MAX_S)));

    /* Disable enable_parallel_cal */
    OS_REG_CLR_BIT(ah, AR_PHY_CL_CAL_CTL, AR_PHY_PARALLEL_CAL_ENABLE);
    /* clear bb_off_pwdAdc bit */
    OS_REG_CLR_BIT(ah, AR_PHY_ADC_CTL, AR_PHY_ADC_CTL_OFF_PWDADC);
    /* enable filter calibration */
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_FLTR_CAL);
    /* Enable PDADC calibration */
    OS_REG_SET_BIT(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_CAL_ENABLE);
        
    /* Perform offset calibration */
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_CAL);

    /* Polling until calibration finish */
    if (!ath_hal_wait(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_CAL, 0, AH_WAIT_TIMEOUT)) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s: offset calibration failed to complete in 1ms; noisy environment?\n",
                __func__);

         return AH_FALSE;
    }

    /* Disable carrier leak calibration & parallel cal */        
    OS_REG_CLR_BIT(ah, AR_PHY_CL_CAL_CTL, AR_PHY_CL_CAL_ENABLE);

    /* Restore tx gain */
    for ( i = 0; i <= tx_gain_max; i++) {
        OS_REG_WRITE(ah, (AR_PHY_TX_GAIN_TBL1 + i * 4), tx_gain[i]);
    }  

    /* Write tx_table_max back */
    OS_REG_WRITE(ah, AR_PHY_TX_PWRCTRL7, ((OS_REG_READ(ah, AR_PHY_TX_PWRCTRL7) & (~AR_PHY_TX_PWRCTRL_TX_GAIN_TAB_MAX)) | (tx_gain_max << AR_PHY_TX_PWRCTRL_TX_GAIN_TAB_MAX_S)));  

    /* Clear registers */
    /* set bb_off_pwdAdc bit */
    OS_REG_SET_BIT(ah, AR_PHY_ADC_CTL, AR_PHY_ADC_CTL_OFF_PWDADC);
    /* disable carrier leak calibration */
    OS_REG_CLR_BIT(ah, AR_PHY_CL_CAL_CTL, AR_PHY_CL_CAL_ENABLE);
    /* disable filter calibration */
    OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_FLTR_CAL);

    return AH_TRUE;
}


/* ar9285ClCalWAR
 * Carrier leakage Calibration work around for Kite 
 */
#define AR9285_CLCAL_REDO_THRESH    1
static inline HAL_BOOL
ar9285ClCalWAR(struct ath_hal *ah, HAL_CHANNEL *chan)
{

    int i;
    u_int32_t txgain_max;
    u_int32_t clc_gain, gain_mask = 0, clc_num = 0;
    u_int32_t reg_clc_I0, reg_clc_Q0;
    u_int32_t i0_num = 0;
    u_int32_t q0_num = 0;
    u_int32_t total_num = 0;
    u_int32_t reg_rf2g5_org;
    HAL_BOOL retv = AH_TRUE;

    /* do carrier leakage calibration */
    if (ar9285ClCal(ah, chan) == AH_FALSE) {
        return AH_FALSE;
    }
    /* Check if there is carrier leakage error and 
     * redo carrier leakage cal, if necessary 
     */
    /* find how many tx gian table we use, find tx gain table max 
     * index=13 ,0xa274 
     */
    txgain_max = OS_REG_READ_FIELD(ah, AR_PHY_TX_PWRCTRL7, 
                                AR_PHY_TX_PWRCTRL_TX_GAIN_TAB_MAX);

    /* calculate how many carrier leakage data we will store 
     * and find clc used gain number 
     */
    for(i = 0; i < (txgain_max+1); i++)
    { 
        clc_gain =(OS_REG_READ(ah, (AR_PHY_TX_GAIN_TBL1+(i<<2))) & 
                    AR_PHY_TX_GAIN_CLC) >> AR_PHY_TX_GAIN_CLC_S;
        if (!(gain_mask & (1 << clc_gain))) {
            gain_mask |= (1 << clc_gain);
            clc_num++;
        }
    }

    /* record clc_I = 0,clc_q = 0 number */
    for(i = 0; i < clc_num; i++)
    { 
        reg_clc_I0 = (OS_REG_READ(ah, 
            (AR_PHY_CLC_TBL1 + (i << 2))) & AR_PHY_CLC_I0) >> AR_PHY_CLC_I0_S;
        reg_clc_Q0 = (OS_REG_READ(ah, 
            (AR_PHY_CLC_TBL1 + (i << 2))) & AR_PHY_CLC_Q0) >> AR_PHY_CLC_Q0_S;
        if(reg_clc_I0 == 0) { /* calculate i0_num */
            i0_num++;
        }
        if(reg_clc_Q0 == 0) { /* calculate q0_num */
            q0_num++;
        }
    }
    total_num = i0_num + q0_num;
    /* If Total num > AR9285_CLCAL_REDO_THRESH, set ic505tx=4, then recal */
    if(total_num > AR9285_CLCAL_REDO_THRESH)
    { 
        reg_rf2g5_org=OS_REG_READ(ah,AR9285_RF2G5); /* save reg AR9285_RF2G5 */
        if (AR_SREV_ELIJAH_20(ah)) {
            OS_REG_WRITE(ah,AR9285_RF2G5,
                     (reg_rf2g5_org & AR9285_RF2G5_IC50TX) | 
                       AR9285_RF2G5_IC50TX_ELIJAH_SET); /* set ic50tx=5 */
        }
        else {
            OS_REG_WRITE(ah,AR9285_RF2G5,
                (reg_rf2g5_org & AR9285_RF2G5_IC50TX) | 
                AR9285_RF2G5_IC50TX_SET); /* set ic50tx=4 */
        }

        /* do carrier leakage calibration */
        retv = ar9285ClCal(ah, chan);

        /* reset ic50tx to default CGF value */
        OS_REG_WRITE(ah,AR9285_RF2G5,reg_rf2g5_org);
    }     
    return retv;
}

/* ar5416InitCal
 * Initialize Calibration infrastructure
 */
static inline HAL_BOOL
ar5416InitCal(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    HAL_CHANNEL_INTERNAL *ichan = ath_hal_checkchannel(ah, chan);

    HALASSERT(ichan);

    if(ichan) { 
        if (AR_SREV_K2(ah)) {
            if (!ar9285ClCal(ah, chan))
                return AH_FALSE;
        }
        else if (AR_SREV_KITE(ah) && AR_SREV_KITE_12_OR_LATER(ah)) {
            if (!ar9285ClCalWAR(ah, chan)) return AH_FALSE;
        } else {
            if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
                /* Enable Rx Filter Cal */
                /* No need to disable ADC for Kiwi */
                if (!AR_SREV_KIWI_10_OR_LATER(ah)) {
                    OS_REG_CLR_BIT(ah, AR_PHY_ADC_CTL, AR_PHY_ADC_CTL_OFF_PWDADC);
                }
                OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_FLTR_CAL);
            }
            /* Calibrate the AGC */
            OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL,
                     OS_REG_READ(ah, AR_PHY_AGC_CONTROL) |
                     AR_PHY_AGC_CONTROL_CAL);

            /* Poll for offset calibration complete */
            if (!ath_hal_wait(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_CAL, 0,
                AH_WAIT_TIMEOUT)) {
                HDPRINTF(ah, HAL_DBG_CALIBRATE,
                        "%s: offset calibration failed to complete in 1ms; "
                        "noisy environment?\n", __func__);
                return AH_FALSE;
            }

            if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
                /* No need to disable/enable ADC for Kiwi */
                if (!AR_SREV_KIWI_10_OR_LATER(ah)) {
                    /* Filter Cal done, disable */
                    OS_REG_SET_BIT(ah, AR_PHY_ADC_CTL, AR_PHY_ADC_CTL_OFF_PWDADC);
                }
                OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_FLTR_CAL);
            }
        }

        /* Do PA Calibration */
        if (AR_SREV_K2(ah)) {
#ifdef AH_SUPPORT_K2
            ar9271PACal(ah, AH_TRUE);
            if ((AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable)) {
                AH_PRIVATE(ah)->green_tx_status = HAL_RSSI_TX_POWER_NONE;
            }
#endif /* AH_SUPPORT_K2 */
        } else if (AR_SREV_KITE(ah) && AR_SREV_KITE_11_OR_LATER(ah)) {
#ifdef AH_SUPPORT_KITE_ANY
            ar9285PACal(ah, AH_TRUE);
            if ((AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable)) {
                AH_PRIVATE(ah)->green_tx_status = HAL_RSSI_TX_POWER_NONE;
            }
#endif /* AH_SUPPORT_KITE_ANY */
        } else if (AR_SREV_KIWI(ah)) {
#ifdef AH_SUPPORT_KIWI_ANY
            if ((AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable)) {
                AH_PRIVATE(ah)->green_tx_status = HAL_RSSI_TX_POWER_NONE;	
            }
#endif /* AH_SUPPORT_KIWI_ANY */
        }

        /*
         * Do NF calibration after DC offset and other CALs.
         * Per system engineers, noise floor value can sometimes be 20 dB
         * higher than normal value if DC offset and noise floor cal are
         * triggered at the same time.
         */
        OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL,
                     OS_REG_READ(ah, AR_PHY_AGC_CONTROL) |
                     AR_PHY_AGC_CONTROL_NF);
        AH5416(ah)->nf_tsf32 = ar5416GetTsf32(ah);

        /* Initialize list pointers */
        ahp->ah_cal_list = ahp->ah_cal_list_last = ahp->ah_cal_list_curr = AH_NULL;

        /*
         * Enable IQ, ADC Gain, ADC DC Offset Cals
         */
        if (AR_SREV_HOWL(ah) || AR_SREV_SOWL_10_OR_LATER(ah)) {
            /* Setup all non-periodic, init time only calibrations */
            // XXX: Init DC Offset not working yet
#ifdef not_yet
            if (AH_TRUE == ar5416IsCalSupp(ah, chan, ADC_DC_INIT_CAL)) {
                INIT_CAL(&ahp->ah_adcDcCalInitData);
                INSERT_CAL(ahp, &ahp->ah_adcDcCalInitData);
            }

            /* Initialize current pointer to first element in list */
            ahp->ah_cal_list_curr = ahp->ah_cal_list;

            if (ahp->ah_cal_list_curr) {
                if (ar5416RunInitCals(ah, 0) == AH_FALSE)
                    {return AH_FALSE;}
            }
#endif
            /* end - Init time calibrations */

            /* If Cals are supported, add them to list via INIT/INSERT_CAL */
            if (AH_TRUE == ar5416IsCalSupp(ah, chan, ADC_GAIN_CAL)) {
                INIT_CAL(&ahp->ah_adcGainCalData);
                INSERT_CAL(ahp, &ahp->ah_adcGainCalData);
                HDPRINTF(ah, HAL_DBG_CALIBRATE,
                         "%s: enabling ADC Gain Calibration.\n", __func__);
            }
            if (AH_TRUE == ar5416IsCalSupp(ah, chan, ADC_DC_CAL)) {
                INIT_CAL(&ahp->ah_adcDcCalData);
                INSERT_CAL(ahp, &ahp->ah_adcDcCalData);
                HDPRINTF(ah, HAL_DBG_CALIBRATE,
                         "%s: enabling ADC DC Calibration.\n", __func__);
            }
            if (AH_TRUE == ar5416IsCalSupp(ah, chan, IQ_MISMATCH_CAL)) {
                INIT_CAL(&ahp->ah_iqCalData);
                INSERT_CAL(ahp, &ahp->ah_iqCalData);
                HDPRINTF(ah, HAL_DBG_CALIBRATE,
                         "%s: enabling IQ Calibration.\n", __func__);
            }

            /* Initialize current pointer to first element in list */
            ahp->ah_cal_list_curr = ahp->ah_cal_list;

            /* Reset state within current cal */
            if (ahp->ah_cal_list_curr)
                ar5416ResetCalibration(ah, ahp->ah_cal_list_curr);
        }

        /* Mark all calibrations on this channel as being invalid */
        ichan->cal_valid = 0;

        return AH_TRUE;
    }
    else {
        return AH_FALSE;
    }
}

/* ar5416ResetCalValid
 * Entry point for upper layers to restart current cal.
 * Reset the calibration valid bit in channel.
 */
void
ar5416ResetCalValid(struct ath_hal *ah, HAL_CHANNEL *chan, HAL_BOOL *isCalDone, u_int32_t calType)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    HAL_CHANNEL_INTERNAL *ichan = ath_hal_checkchannel(ah, chan);
    HAL_CAL_LIST *currCal = ahp->ah_cal_list_curr;

    *isCalDone = AH_TRUE;

    if (!AR_SREV_HOWL(ah) && !AR_SREV_SOWL_10_OR_LATER(ah)) {
        return;
    }

    if (currCal == AH_NULL) {
        return;
    }

    if (ichan == AH_NULL) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: invalid channel %u/0x%x; no mapping\n",
                 __func__, chan->channel, chan->channel_flags);
        return;
    }

    /* Expected that this calibration has run before, post-reset.
     * Current state should be done
     */
    if (currCal->calState != CAL_DONE) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                 "%s: Calibration state incorrect, %d\n",
                 __func__, currCal->calState);
        return;
    }

    /* Verify Cal is supported on this channel */
    if (ar5416IsCalSupp(ah, chan, currCal->calData->calType) == AH_FALSE) {
        return;
    }

    HDPRINTF(ah, HAL_DBG_CALIBRATE,
             "%s: Resetting Cal %d state for channel %u/0x%x\n",
             __func__, currCal->calData->calType, chan->channel,
             chan->channel_flags);

    /* Disable cal validity in channel */
    ichan->cal_valid &= ~currCal->calData->calType;
    currCal->calState = CAL_WAITING;
    /* Indicate to upper layers that we need polling, Howl/Sowl bug */
    *isCalDone = AH_FALSE;
}

static inline void
ar5416SetDma(struct ath_hal *ah)
{
    u_int32_t   regval;

	ENABLE_REG_WRITE_BUFFER

    /*
     * set AHB_MODE not to do cacheline prefetches
     */
    regval = OS_REG_READ(ah, AR_AHB_MODE);
    OS_REG_WRITE(ah, AR_AHB_MODE, regval | AR_AHB_PREFETCH_RD_EN);

    /*
     * let mac dma reads be in 128 byte chunks
     */
    regval = OS_REG_READ(ah, AR_TXCFG) & ~AR_TXCFG_DMASZ_MASK;
    OS_REG_WRITE(ah, AR_TXCFG, regval | AR_TXCFG_DMASZ_128B);
    OS_REG_WRITE_FLUSH(ah);

	DISABLE_REG_WRITE_BUFFER

    /*
     * Restore TX Trigger Level to its pre-reset value.
     * The initial value depends on whether aggregation is enabled, and is
     * adjusted whenever underruns are detected.
     */
    OS_REG_RMW_FIELD(ah, AR_TXCFG, AR_FTRIG, AH_PRIVATE(ah)->ah_tx_trig_level);

	ENABLE_REG_WRITE_BUFFER

    /*
     * let mac dma writes be in 128 byte chunks
     */
    regval = OS_REG_READ(ah, AR_RXCFG) & ~AR_RXCFG_DMASZ_MASK;
    OS_REG_WRITE(ah, AR_RXCFG, regval | AR_RXCFG_DMASZ_128B);

    /*
     * Setup receive FIFO threshold to hold off TX activities
     */
    OS_REG_WRITE(ah, AR_RXFIFO_CFG, 0x200);

    /*
     * reduce the number of usable entries in PCU TXBUF to avoid
     * wrap around bugs. (bug 20428)
     */
	if (AR_SREV_KITE(ah)) {
        /* For Kite number of Fifos are reduced to half.
         * So set the usable tx buf size also to half to
         * avoid data/delimiter underruns
         * See bug #32553 for details
         */
        OS_REG_WRITE(ah, AR_PCU_TXBUF_CTRL, AR_KITE_PCU_TXBUF_CTRL_USABLE_SIZE);
    }
    else if (!AR_SREV_K2(ah)) {
        OS_REG_WRITE(ah, AR_PCU_TXBUF_CTRL, AR_PCU_TXBUF_CTRL_USABLE_SIZE);
    }
    OS_REG_WRITE_FLUSH(ah);

	DISABLE_REG_WRITE_BUFFER
}

/* Override INI values with chip specific configuration.
 *
 */
static inline void
ar5416OverrideIni(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t val;

    /* Set the RX_ABORT and RX_DIS and clear it only after
     * RXE is set for MAC. This prevents frames with
     * corrupted descriptor status.
     */
    OS_REG_SET_BIT(ah, AR_DIAG_SW, (AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT));
    /*
     * For Merlin and above, there is a new feature that allows Multicast search
     * based on both MAC Address and Key ID. By default, this feature is enabled.
     * But since the driver is not using this feature, we switch it off; otherwise
     * multicast search based on MAC addr only will fail.
     *
     * For Merlin and Kite, bit20 in register AR_PCU_MISC_MODE2 (a hw workaround)
     * can cause rx data corruption, hence diable it. ExtraView# 55602.
     *
     * For Kiwi, bit 25 in register AR_PCU_MISC_MODE2 (another hw workaround)
     * can also cause rx data corruption.
     */
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        val = OS_REG_READ(ah, AR_PCU_MISC_MODE2) & (~AR_PCU_MISC_MODE2_ADHOC_MCAST_KEYID_ENABLE);
        if (!AR_SREV_K2(ah)) {
             val &= (~AR_PCU_MISC_MODE2_HWWAR1);
        }
        if (AR_SREV_KIWI_10_OR_LATER(ah)) {
             val = val & (~AR_PCU_MISC_MODE2_HWWAR2);
        }

        OS_REG_WRITE(ah, AR_PCU_MISC_MODE2, val);
    }

    if (!AR_SREV_5416_V20_OR_LATER(ah) || AR_SREV_MERLIN_10_OR_LATER(ah)) {
        return;
    }

    /* Disable BB clock gating
     * Necessary to avoid hangs in Owl 2.0
     */
    OS_REG_WRITE(ah, 0x9800+(651<<2), 0x11);
}

static inline void
ar5416InitBB(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t synthDelay;

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

    /* Activate the PHY (includes baseband activate + synthesizer on) */
    OS_REG_WRITE(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);

    /*
     * There is an issue if the AP starts the calibration before
     * the base band timeout completes.  This could result in the
     * rx_clear false triggering.  As a workaround we add delay an
     * extra BASE_ACTIVATE_DELAY usecs to ensure this condition
     * does not happen.
     */
    OS_DELAY(synthDelay + BASE_ACTIVATE_DELAY);
}

static inline void
ar5416InitInterruptMasks(struct ath_hal *ah, HAL_OPMODE opmode)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    /*
     * Setup interrupt handling.  Note that ar5416ResetTxQueue
     * manipulates the secondary IMR's as queues are enabled
     * and disabled.  This is done with RMW ops to insure the
     * settings we make here are preserved.
     */
    ahp->ah_maskReg = AR_IMR_TXERR | AR_IMR_TXURN | AR_IMR_RXERR | AR_IMR_RXORN
                    | AR_IMR_BCNMISC;

    if (ahp->ah_intrMitigationRx) {
        /* enable interrupt mitigation for rx */
        ahp->ah_maskReg |= AR_IMR_RXINTM | AR_IMR_RXMINTR;
    } else {
        ahp->ah_maskReg |= AR_IMR_RXOK;
    }
    if (ahp->ah_intrMitigationTx) {
        /* enable interrupt mitigation for tx */
        ahp->ah_maskReg |= AR_IMR_TXINTM | AR_IMR_TXMINTR;
    } else {
        ahp->ah_maskReg |= AR_IMR_TXOK;
    }

    if (opmode == HAL_M_HOSTAP) ahp->ah_maskReg |= AR_IMR_MIB;

	ENABLE_REG_WRITE_BUFFER

    OS_REG_WRITE(ah, AR_IMR, ahp->ah_maskReg);

    OS_REG_WRITE(ah, AR_IMR_S2, OS_REG_READ(ah, AR_IMR_S2) | AR_IMR_S2_GTT);

#ifndef AR9100
    /*
     * debug - enable to see all synchronous interrupts status
     */
    /* Clear any pending sync cause interrupts */
    OS_REG_WRITE(ah, AR_INTR_SYNC_CAUSE, 0xFFFFFFFF);

    /* Allow host interface sync interrupt sources to set cause bit */
    OS_REG_WRITE(ah, AR_INTR_SYNC_ENABLE, AR_INTR_SYNC_DEFAULT);

    /* _Disable_ host interface sync interrupt when cause bits set */
    OS_REG_WRITE(ah, AR_INTR_SYNC_MASK, 0);
#endif
    OS_REG_WRITE_FLUSH(ah);

	DISABLE_REG_WRITE_BUFFER
}

static inline void
ar5416InitQOS(struct ath_hal *ah)
{
	ENABLE_REG_WRITE_BUFFER

    OS_REG_WRITE(ah, AR_MIC_QOS_CONTROL, 0x100aa);  /* XXX magic */
    OS_REG_WRITE(ah, AR_MIC_QOS_SELECT, 0x3210);    /* XXX magic */

    /* Turn on NOACK Support for QoS packets */
    OS_REG_WRITE(ah, AR_QOS_NO_ACK,
            SM(2, AR_QOS_NO_ACK_TWO_BIT) |
            SM(5, AR_QOS_NO_ACK_BIT_OFF) |
            SM(0, AR_QOS_NO_ACK_BYTE_OFF));

    /*
     * initialize TXOP for all TIDs
     */
    OS_REG_WRITE(ah, AR_TXOP_X, AR_TXOP_X_VAL);
    OS_REG_WRITE(ah, AR_TXOP_0_3, 0xFFFFFFFF);
    OS_REG_WRITE(ah, AR_TXOP_4_7, 0xFFFFFFFF);
    OS_REG_WRITE(ah, AR_TXOP_8_11, 0xFFFFFFFF);
    OS_REG_WRITE(ah, AR_TXOP_12_15, 0xFFFFFFFF);
    OS_REG_WRITE_FLUSH(ah);

	DISABLE_REG_WRITE_BUFFER
}

static inline void
ar5416InitUserSettings(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    /* Restore user-specified settings */
    HDPRINTF(ah, HAL_DBG_RESET, "--AP %s ahp->ah_miscMode 0x%x\n", __func__, ahp->ah_miscMode);
    if (ahp->ah_miscMode != 0)
            OS_REG_WRITE(ah, AR_PCU_MISC, OS_REG_READ(ah, AR_PCU_MISC) | ahp->ah_miscMode);
	if (ahp->ah_getPlcpHdr)
            OS_REG_CLR_BIT(ah, AR_PCU_MISC, AR_PCU_SEL_EVM);
    if (ahp->ah_slottime != (u_int) -1)
            ar5416SetSlotTime(ah, ahp->ah_slottime);
    if (ahp->ah_acktimeout != (u_int) -1)
            ar5416SetAckTimeout(ah, ahp->ah_acktimeout);
    if (AH_PRIVATE(ah)->ah_diagreg != 0)
        OS_REG_SET_BIT(ah, AR_DIAG_SW, AH_PRIVATE(ah)->ah_diagreg);
}

static inline void
ar5416AttachHwPlatform(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    ahp->ah_hwp = HAL_TRUE_CHIP;
    return;
}

int ar5416_getSpurInfo(struct ath_hal * ah, int *enable, int len, u_int16_t *freq)
{
    struct ath_hal_private *ap = AH_PRIVATE(ah);
    int i = 0, j = 0;
    for(i =0; i < len; i++)
    {
        freq[i] =  0;
    }
    *enable =  ap->ah_config.ath_hal_spur_mode;
    for(i = 0; i < AR_EEPROM_MODAL_SPURS; i++)
    {
        if(ap->ah_config.ath_hal_spur_chans[i][0] != AR_NO_SPUR)
        {
            freq[j++] = ap->ah_config.ath_hal_spur_chans[i][0];
            HDPRINTF(ah, HAL_DBG_ANI,
                     "1. get spur %d\n", ap->ah_config.ath_hal_spur_chans[i][0]);
        }
        if(ap->ah_config.ath_hal_spur_chans[i][1] != AR_NO_SPUR)
        {
            freq[j++] = ap->ah_config.ath_hal_spur_chans[i][1];
            HDPRINTF(ah, HAL_DBG_ANI,
                     "2. get spur %d\n", ap->ah_config.ath_hal_spur_chans[i][1]);
        }
    }

    return 0;
}

#define ATH_HAL_2GHZ_FREQ_MIN   20000
#define ATH_HAL_2GHZ_FREQ_MAX   29999
#define ATH_HAL_5GHZ_FREQ_MIN   50000
#define ATH_HAL_5GHZ_FREQ_MAX   59999

int ar5416_setSpurInfo(struct ath_hal * ah, int enable, int len, u_int16_t *freq)
{
    struct ath_hal_private *ap = AH_PRIVATE(ah);
    int i = 0, j = 0, k = 0;

    ap->ah_config.ath_hal_spur_mode = enable;
    if(ap->ah_config.ath_hal_spur_mode == SPUR_ENABLE_IOCTL)
    {
        for(i =0; i < AR_EEPROM_MODAL_SPURS; i++)
        {
            ap->ah_config.ath_hal_spur_chans[i][0] = AR_NO_SPUR;
            ap->ah_config.ath_hal_spur_chans[i][1] = AR_NO_SPUR;
        }
        for(i =0; i < len; i++)
        {
            /* 2GHz Spur */
            if(freq[i] > ATH_HAL_2GHZ_FREQ_MIN && freq[i] < ATH_HAL_2GHZ_FREQ_MAX)
            {
                if(j < AR_EEPROM_MODAL_SPURS)
                {
                    ap->ah_config.ath_hal_spur_chans[j++][1] =  freq[i];
                    HDPRINTF(ah, HAL_DBG_ANI, "1 set spur %d\n", freq[i]);
                }
            }
            /* 5Ghz Spur */
            else if(freq[i] > ATH_HAL_5GHZ_FREQ_MIN && freq[i] < ATH_HAL_5GHZ_FREQ_MAX)
            {
                if(k < AR_EEPROM_MODAL_SPURS)
                {
                    ap->ah_config.ath_hal_spur_chans[k++][0] =  freq[i];
                    HDPRINTF(ah, HAL_DBG_ANI, "2 set spur %d\n", freq[i]);
                }
            }
        }
    }

    return 0;
}

static void
ar5416OpenLoopPowerControlTempCompensationKiwi(struct ath_hal *ah)
{
    u_int32_t    rddata;
    int32_t      delta, currPDADC, slope;
    struct ath_hal_5416     *ahp = AH5416(ah);

    rddata = OS_REG_READ(ah, AR_PHY_TX_PWRCTRL4);

    currPDADC = MS(rddata, AR_PHY_TX_PWRCTRL_PD_AVG_OUT);
    if (ahp->initPDADC == 0 || currPDADC == 0) {
        /* Zero value indicate that no frames have been transmitted yet,
           can't do temperature compensation until frames are transmitted.
         */
        return;
    } else {
        slope = ar5416EepromGet(ahp, EEP_TEMPSENSE_SLOPE);
        if (slope == 0) { /* to avoid divide by zero case */
            delta = 0;
        } else {
            delta = ((currPDADC - ahp->initPDADC)*4) / slope;
        }
        OS_REG_RMW_FIELD(ah, AR_PHY_CH0_TX_PWRCTRL11,
            AR_PHY_TX_PWRCTRL_OLPC_TEMP_COMP, delta);
        OS_REG_RMW_FIELD(ah, AR_PHY_CH1_TX_PWRCTRL11,
            AR_PHY_TX_PWRCTRL_OLPC_TEMP_COMP, delta);
    }
}

void
ar5416OpenLoopPowerControlTempCompensation(struct ath_hal *ah)
{
    u_int32_t    rddata, i;
    int32_t      delta, currPDADC, regval;
    struct ath_hal_5416     *ahp = AH5416(ah);

    if (AR_SREV_KIWI_10_OR_LATER(ah) && ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
        ar5416OpenLoopPowerControlTempCompensationKiwi(ah);
    } else {
        rddata = OS_REG_READ(ah, AR_PHY_TX_PWRCTRL4);
        currPDADC = MS(rddata, AR_PHY_TX_PWRCTRL_PD_AVG_OUT);

        if (ahp->initPDADC == 0 || currPDADC == 0) { 
            /* Zero value indicate that no frames have been transmitted yet,
               can't do temperature compensation until frames are transmitted.
             */
            return;
        } else {
            if (ar5416EepromGet(ahp, EEP_DAC_HPWR_5G)) {
                delta = (currPDADC - ahp->initPDADC + 4) / 8;
            } else {
                delta = (currPDADC - ahp->initPDADC + 5) / 10;
            }
            if ((delta != ahp->PDADCdelta)) {
                ahp->PDADCdelta = delta;
                for(i = 1; i < MERLIN_TX_GAIN_TABLE_SIZE; i++) {
                    regval = ahp->originalGain[i] - delta;
                    if (regval < 0) regval = 0;
                    OS_REG_RMW_FIELD(ah, AR_PHY_TX_GAIN_TBL1 + i * 4,
                        AR_PHY_TX_GAIN, regval);
                }
            }
        } 
    }
}

/* PDADC vs Tx power table on open-loop power control mode */
static void
ar5416OpenLoopPowerControlInit(struct ath_hal *ah)
{
    u_int32_t i;
    struct ath_hal_5416     *ahp = AH5416(ah);

    if (AR_SREV_KIWI_10_OR_LATER(ah) && ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
        OS_REG_SET_BIT(ah, AR_PHY_TX_PWRCTRL9,
            AR_PHY_TX_PWRCTRL9_RES_DC_REMOVAL);
        analogShiftRegRMW(ah, AR9287_AN_TXPC0, AR9287_AN_TXPC0_TXPCMODE,
            AR9287_AN_TXPC0_TXPCMODE_S, AR9287_AN_TXPC0_TXPCMODE_TEMPSENSE);
        /* delay for RF register writes */
        OS_DELAY(100);
    } else {
        for (i = 0; i < MERLIN_TX_GAIN_TABLE_SIZE; i++) {
            ahp->originalGain[i] =
                MS(OS_REG_READ(ah, AR_PHY_TX_GAIN_TBL1 + i*4), AR_PHY_TX_GAIN);
        }
        ahp->PDADCdelta = 0;
    }
}

static inline void
ar5416InitMFP(struct ath_hal * ah)
{
    u_int32_t   mfpcap, mfp_qos;
    /* legacy hardware. No need to setup any MFP registers */
    if (!AR_SREV_SOWL_10_OR_LATER(ah)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s legacy hardware\n", __func__);
        return;
    }
    ath_hal_getcapability(ah, HAL_CAP_MFP, 0, &mfpcap);
    if(mfpcap == HAL_MFP_QOSDATA) {
        /* Treat like legacy hardware. Do not touch the MFP registers. */
        HDPRINTF(ah, HAL_DBG_RESET, "%s forced to use QOSDATA\n", __func__);
        return;
    }

    /* MFP support (Sowl 1.0 or greater) */
    if (mfpcap == HAL_MFP_HW_CRYPTO) {
        /* configure hardware MFP support */
        HDPRINTF(ah, HAL_DBG_RESET, "%s using HW crypto\n", __func__);
        OS_REG_RMW_FIELD(ah, AR_AES_MUTE_MASK1, AR_AES_MUTE_MASK1_FC_MGMT, AR_AES_MUTE_MASK1_FC_MGMT_MFP);
        OS_REG_SET_BIT(ah, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_MGMT_CRYPTO_ENABLE);
        OS_REG_CLR_BIT(ah, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_NO_CRYPTO_FOR_NON_DATA_PKT);
        /*
        * Mask used to construct AAD for CCMP-AES
        * Cisco spec defined bits 0-3 as mask 
        * IEEE802.11w defined as bit 4.
        */		
        if (ath_hal_get_mfp_qos(ah)) {
            mfp_qos = AR_MFP_QOS_MASK_IEEE;
        } else {
            mfp_qos = AR_MFP_QOS_MASK_CISCO;
        }
        OS_REG_RMW_FIELD(ah, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_MGMT_QOS, mfp_qos);
    } else if (mfpcap == HAL_MFP_PASSTHRU) {
        /* Disable en/decrypt by hardware */
        HDPRINTF(ah, HAL_DBG_RESET, "%s using passthru\n", __func__);
        OS_REG_CLR_BIT(ah, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_MGMT_CRYPTO_ENABLE);
        OS_REG_SET_BIT(ah, AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_NO_CRYPTO_FOR_NON_DATA_PKT);
    }
}

HAL_BOOL
ar5416InterferenceIsPresent(struct ath_hal *ah)
{
    int i;
    struct ath_hal_private  *ahpriv = AH_PRIVATE(ah);

    /* This function is called after a stuck beacon, if EACS is enabled. If CW interference is severe, then 
     * HW goes into a loop of continuous stuck beacons and resets. On reset the NF cal history is cleared.
     * So the median value of the history cannot be used - hence check if any value (Chain 0/Primary Channel)
     * is outside the bounds.
     */
    HAL_NFCAL_HIST_FULL *h = AH_HOME_CHAN_NFCAL_HIST(ah);
    for (i = 0; i < HAL_NF_CAL_HIST_LEN_FULL; i++) {
        if (h->nf_cal_buffer[i][0] > ahpriv->nfp->nominal + ahpriv->nf_cw_int_delta) {
                HDPRINTF(ah, HAL_DBG_NF_CAL,
                    "%s: NF Cal: CW interferer detected through NF: \n", __func__); 
    		return AH_TRUE;
        }

    }
    return AH_FALSE;
}

HAL_BOOL ar5416_txbf_loforceon_update(struct ath_hal *ah,HAL_BOOL loforcestate)
{
    return AH_FALSE;    
}
#if ATH_ANT_DIV_COMB
HAL_BOOL
ar5416_ant_ctrl_set_lna_div_use_bt_ant(struct ath_hal *ah, HAL_BOOL enable, HAL_CHANNEL *chan)
{
    return AH_TRUE;
}
#endif /* ATH_ANT_DIV_COMB */
#endif /* AH_SUPPORT_AR5416 */

