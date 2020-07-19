/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
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
#include "ah.h"
#include "ah_internal.h"
#include "ah_eeprom.h"
#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"
#include "ar5416eep.h"

#define TXPOWER_0 0x000000ff
#define TXPOWER_1 0x0000ff00
#define TXPOWER_2 0x00ff0000
#define TXPOWER_3 0xff000000
#define TXPOWER(_c) TXPOWER_##_c

#ifdef AH_SUPPORT_AR5416
#ifdef AH_SUPPORT_EEPROM_DEF

#define N(a)            (sizeof (a) / sizeof (a[0]))

#ifdef EEPROM_DUMP
static void ar5416EepDefPrintPowerPerRate(struct ath_hal *ah, int16_t pRatesPower[]);
static void ar5416EepromDefDump(struct ath_hal *ah, ar5416_eeprom_def_t *ar5416Eep);
#endif

#ifdef AH_AR5416_OVRD_TGT_PWR
static void ar5416EepDefOverrideTgtPower(struct ath_hal *ah, ar5416_eeprom_def_t *eep);
#ifdef EEPROM_DUMP
static void ar5416EepDefDumpTgtPower(struct ath_hal *ah, ar5416_eeprom_def_t *eep);
#endif
#endif /* AH_AR5416_OVRD_TGT_PWR */

static inline void
ar5416EepromDefUpdateChecksum(struct ath_hal_5416 *ahp);
HAL_BOOL ar5416EepromDefSetPowerPerRateTable(struct ath_hal *ah,
    ar5416_eeprom_def_t *pEepData,
    HAL_CHANNEL_INTERNAL *chan, int16_t *ratesArray,
    u_int16_t cfgCtl, u_int16_t AntennaReduction,
    u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit, u_int8_t chainmask);
static inline HAL_BOOL ar5416EepromDefSetPowerCalTable(struct ath_hal *ah,
    ar5416_eeprom_def_t *pEepData,
    HAL_CHANNEL_INTERNAL *chan,
    int16_t *pTxPowerIndexOffset);
static inline HAL_BOOL
ar5416EepDefGetGainBoundariesAndPdadcs(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan, CAL_DATA_PER_FREQ_EEPDEF *pRawDataSet,
    u_int8_t * bChans,  u_int16_t availPiers,
    u_int16_t tPdGainOverlap, int16_t *pMinCalPower, u_int16_t * pPdGainBoundaries,
    u_int8_t * pPDADCValues, u_int16_t numXpdGains);

static inline void
ar5416OpenLoopPowerControlGetPdadcs(struct ath_hal *ah, u_int32_t initTxGain, int32_t txPower, u_int8_t* pPDADCValues);

static inline HAL_STATUS
ar5416EepromDefFixCTLs(struct ath_hal *ah);

static inline void
ar5416GetTxGainIndex(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan, CAL_DATA_PER_FREQ_OP_LOOP *pRawDatasetOpLoop,
    u_int8_t *pCalChans,  u_int16_t availPiers,
    u_int8_t *pPwr, u_int8_t *pPcdacIdx);


/**************************************************************
 * ar5416EepDefGetMaxEdgePower
 *
 * Find the maximum conformance test limit for the given channel and CTL info
 */
static inline u_int16_t
ar5416EepDefGetMaxEdgePower(u_int16_t freq, CAL_CTL_EDGES *pRdEdgesPower, HAL_BOOL is2GHz)
{
    u_int16_t twiceMaxEdgePower = AR5416_MAX_RATE_POWER;
    int      i;

    /* Get the edge power */
    for (i = 0; (i < AR5416_EEPDEF_NUM_BAND_EDGES) && (pRdEdgesPower[i].bChannel != AR5416_BCHAN_UNUSED) ; i++)
    {
        /*
        * If there's an exact channel match or an inband flag set
        * on the lower channel use the given rdEdgePower
        */
        if (freq == fbin2freq(pRdEdgesPower[i].bChannel, is2GHz))
        {
            twiceMaxEdgePower = pRdEdgesPower[i].tPower;
            break;
        }
        else if ((i > 0) && (freq < fbin2freq(pRdEdgesPower[i].bChannel, is2GHz)))
        {
            if (fbin2freq(pRdEdgesPower[i - 1].bChannel, is2GHz) < freq && pRdEdgesPower[i - 1].flag)
            {
                twiceMaxEdgePower = pRdEdgesPower[i - 1].tPower;
            }
            /* Leave loop - no more affecting edges possible in this monotonic increasing list */
            break;
        }
    }
    HALASSERT(twiceMaxEdgePower > 0);
    return twiceMaxEdgePower;
}


u_int32_t
ar5416EepromDefGet(struct ath_hal_5416 *ahp, EEPROM_PARAM param)
{
    ar5416_eeprom_def_t *eep = &ahp->ah_eeprom.map.def;
    MODAL_EEPDEF_HEADER *pModal = eep->modalHeader;
    BASE_EEPDEF_HEADER  *pBase  = &eep->baseEepHeader;

    switch (param)
    {
    case EEP_NFTHRESH_5:
        return pModal[0].noiseFloorThreshCh[0];
    case EEP_NFTHRESH_2:
        return pModal[1].noiseFloorThreshCh[0];
    case EEP_MAC_LSW:
        return pBase->macAddr[0] << 8 | pBase->macAddr[1];
    case EEP_MAC_MID:
        return pBase->macAddr[2] << 8 | pBase->macAddr[3];
    case EEP_MAC_MSW:
        return pBase->macAddr[4] << 8 | pBase->macAddr[5];
    case EEP_REG_0:
        return pBase->regDmn[0];
    case EEP_REG_1:
        return pBase->regDmn[1];
    case EEP_OP_CAP:
        return pBase->deviceCap;
    case EEP_OP_MODE:
        return pBase->opCapFlags;
    case EEP_RF_SILENT:
        return pBase->rfSilent;
    case EEP_OB_5:
        return pModal[0].ob;
    case EEP_DB_5:
        return pModal[0].db;
    case EEP_OB_2:
        return pModal[1].ob;
    case EEP_DB_2:
        return pModal[1].db;
    case EEP_MINOR_REV:
        return pBase->version & AR5416_EEP_VER_MINOR_MASK;
    case EEP_TX_MASK:
        return pBase->txMask;
    case EEP_RX_MASK:
        return pBase->rxMask;
    case EEP_FSTCLK_5G:
        return pBase->fastClk5g;
    case EEP_RXGAIN_TYPE:
        return pBase->rxGainType;
    case EEP_OL_PWRCTRL:
        if ((pBase->version & AR5416_EEP_VER_MINOR_MASK) >=
            AR5416_EEP_MINOR_VER_19) {
            return pBase->openLoopPwrCntl ? AH_TRUE : AH_FALSE;
        } else {
            return AH_FALSE;
        }
    case EEP_TXGAIN_TYPE:
        return pBase->txGainType;
    case EEP_RC_CHAIN_MASK:
        if ((pBase->version & AR5416_EEP_VER_MINOR_MASK) >=
            AR5416_EEP_MINOR_VER_19) {
            return pBase->rcChainMask;
        } else {
            return 0;
        }
    case EEP_DAC_HPWR_5G:
        if ((pBase->version & AR5416_EEP_VER_MINOR_MASK) >=
            AR5416_EEP_MINOR_VER_20) {
            return pBase->dacHiPwrMode_5G;
        } else {
            return 0;
        }
    case EEP_FRAC_N_5G:
        if ((pBase->version & AR5416_EEP_VER_MINOR_MASK) >=
            AR5416_EEP_MINOR_VER_22) {
            return pBase->fracN5g;
        } else {
            return 0;
        }
    case EEP_DEV_TYPE:
        return pBase->deviceType;
    case EEP_PWR_TABLE_OFFSET:
        if ((pBase->version & AR5416_EEP_VER_MINOR_MASK) >=
            AR5416_EEP_MINOR_VER_21) {
            return pBase->pwrTableOffset;
        } else {
            return AR5416_PWR_TABLE_OFFSET_DB;
        }
    default:
        HALASSERT(0);
        return 0;
    }
}

static inline void
ar5416EepromDefUpdateChecksum(struct ath_hal_5416 *ahp)
{
    ar5416_eeprom_def_t *eep = &ahp->ah_eeprom.map.def;
    BASE_EEPDEF_HEADER  *pBase  = &eep->baseEepHeader;
    u_int16_t sum = 0, *pHalf, i;
    u_int16_t lengthStruct;

    lengthStruct = pBase->length;  /* length in bytes */

    /* clear intermediate value */
    pBase->checksum = 0;
    /* calc */
    pHalf = (u_int16_t *)eep;
    for (i = 0; i < lengthStruct/2; i++) {
        sum ^= *pHalf++;
    }

    pBase->checksum = 0xFFFF ^ sum;
}


#ifdef AH_SUPPORT_WRITE_EEPROM
/**************************************************************
 * ar5416EepromSetParam
 */
HAL_BOOL
ar5416EepromDefSetParam(struct ath_hal *ah, EEPROM_PARAM param, u_int32_t value)
{
    HAL_BOOL result = AH_TRUE;
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_def_t *eep = &ahp->ah_eeprom.map.def;
    BASE_EEPDEF_HEADER  *pBase  = &eep->baseEepHeader;
    int offsetRd = 0;
    int offsetChksum = 0;
    u_int16_t checksum;

    offsetRd = AR5416_EEPDEF_START_LOC + (int) (offsetof(struct BaseEepDefHeader, regDmn[0]) >> 1);
    offsetChksum = AR5416_EEPDEF_START_LOC + (offsetof(struct BaseEepDefHeader, checksum) >> 1);

    switch (param) {
    case EEP_REG_0:
        pBase->regDmn[0] = (u_int16_t) value;

        result = AH_FALSE;
        if (ar5416EepromWrite(ah, offsetRd, (u_int16_t) value)) {
            ar5416EepromDefUpdateChecksum(ahp);
            checksum = pBase->checksum;
#if AH_BYTE_ORDER == AH_BIG_ENDIAN
            checksum  = SWAP16(checksum );
#endif
            if (ar5416EepromWrite(ah, offsetChksum, checksum )) {
                result = AH_TRUE;
            }
        }
        break;
    default:
        HALASSERT(0);
        break;
    }
    return result;
}
#endif /* AH_SUPPORT_WRITE_EEPROM */

int32_t ar5416EepDefMinCCAPwrThresGet(struct ath_hal *ah, int chain, u_int16_t channel)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_def_t *eep = &ahp->ah_eeprom.map.def;

    if (channel < 4000) {
        return eep->modalHeader[1].noiseFloorThreshCh[chain];
    } else {
        return eep->modalHeader[0].noiseFloorThreshCh[chain];
    }
}

HAL_BOOL ar5416EepDefMinCCAPwrThresApply(struct ath_hal *ah, u_int16_t channel)
{
    int32_t value;
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_def_t *eep = &ahp->ah_eeprom.map.def;

    if ((eep->baseEepHeader.eepMisc & AR5416_EEPMISC_MINCCAPWR_EN) == 0) {
        return 0;
    }

    /* Applying individual chain values */
    if (eep->baseEepHeader.rxMask & 0x1) {

        value = ar5416EepDefMinCCAPwrThresGet(ah, 0, channel);
        OS_REG_RMW_FIELD(ah, AR_PHY_CCA_CTRL_0, AR_PHY_EXT_CCA0_THRESH62_1, value);

    }
    if (eep->baseEepHeader.rxMask & 0x2) {

        value = ar5416EepDefMinCCAPwrThresGet(ah, 1, channel);
        OS_REG_RMW_FIELD(ah, AR_PHY_CCA_CTRL_1, AR_PHY_EXT_CCA0_THRESH62_1, value);

    }
    if (eep->baseEepHeader.rxMask & 0x4) {

        value = ar5416EepDefMinCCAPwrThresGet(ah, 2, channel);
        OS_REG_RMW_FIELD(ah, AR_PHY_CCA_CTRL_2, AR_PHY_EXT_CCA0_THRESH62_1, value);

    }

    OS_REG_RMW_FIELD(ah, AR_PHY_CCA_CTRL_0, AR_PHY_EXT_CCA0_THRESH62_ENABLE, 1);

    return 0;
}

/*
 * Read EEPROM header info and program the device for correct operation
 * given the channel value.
 */
HAL_BOOL
ar5416EepromDefSetBoardValues(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
    MODAL_EEPDEF_HEADER *pModal;
    int i, regChainOffset;
    struct ath_hal_private  *ap = AH_PRIVATE(ah);
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_def_t *eep = &ahp->ah_eeprom.map.def;
    u_int8_t    txRxAttenLocal;    /* workaround for eeprom versions <= 14.2 */
    u_int32_t   ant_config = 0;

    static u_int32_t regList [9][2] = {
      { AR_PHY_POWER_TX_RATE1, 0 },
      { AR_PHY_POWER_TX_RATE2, 0 },
      { AR_PHY_POWER_TX_RATE3, 0 },
      { AR_PHY_POWER_TX_RATE4, 0 },
      { AR_PHY_POWER_TX_RATE5, 0 },
      { AR_PHY_POWER_TX_RATE6, 0 },
      { AR_PHY_POWER_TX_RATE7, 0 },
      { AR_PHY_POWER_TX_RATE8, 0 },
      { AR_PHY_POWER_TX_RATE9, 0 },
    };


    HALASSERT(owl_get_eepdef_ver(ahp) == AR5416_EEP_VER);
    
    pModal = &(eep->modalHeader[IS_CHAN_2GHZ(chan)]);

    txRxAttenLocal = IS_CHAN_2GHZ(chan) ? 23 : 44;    /* workaround for eeprom versions < 14.3 */

    if (ahp->ah_emu_eeprom) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                 "!!! IS EMU EEPROM: %s\n", __func__);
        return AH_TRUE;
    }

    if ( ar5416EepromGetAntCfg(
             ahp, chan, ap->ah_config.ath_hal_defaultAntCfg, &ant_config) ==
         HAL_OK )
    {
        OS_REG_WRITE(ah, AR_PHY_SWITCH_COM, ant_config);
    }

    for (i = 0; i < AR5416_MAX_CHAINS; i++) {
        if (AR_SREV_MERLIN(ah)) {
            if (i >= 2) break;
        }

        if(AR_SREV_5416_V20_OR_LATER(ah) &&
            (ahp->ah_rxchainmask == 5 || ahp->ah_txchainmask == 5) &&
            (i != 0))
        {
            /*
             * Regs are swapped from chain 2 to 1 for 5416 2_0 with
             * only chains 0 and 2 populated
             */
            regChainOffset = (i == 1) ? 0x2000 : 0x1000;
        } else {
            regChainOffset = i * 0x1000;
        }

        OS_REG_WRITE(ah, AR_PHY_SWITCH_CHAIN_0 + regChainOffset,
                     pModal->antCtrlChain[i]);

        OS_REG_WRITE(ah, AR_PHY_TIMING_CTRL4(0) + regChainOffset, 
            ( OS_REG_READ(ah, AR_PHY_TIMING_CTRL4(0) + regChainOffset) &
              ~( AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF |
                 AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF ) ) |
            SM(pModal->iqCalICh[i], AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF) |
            SM(pModal->iqCalQCh[i], AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF));

        if ((i == 0) || AR_SREV_5416_V20_OR_LATER(ah)) {
            if ((eep->baseEepHeader.version & AR5416_EEP_VER_MINOR_MASK) >=
                AR5416_EEP_MINOR_VER_3)
            {
                /*
                 * Large signal upgrade,
                 * If 14.3 or later EEPROM, use
                 * txRxAttenLocal = pModal->txRxAttenCh[i]
                 * else txRxAttenLocal is fixed value above.
                 */
                txRxAttenLocal = pModal->txRxAttenCh[i];

                if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
                    /* 
                     * From Merlin(AR9280),
                     *     bswAtten[] maps to chain specific
                     *         xatten1_db(0xa20c/0xb20c 5:0)
                     *     bswMargin[] maps to chain specific
                     *         xatten1_margin (0xa20c/0xb20c 16:12)
                     */
                    OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
                        AR_PHY_GAIN_2GHZ_XATTEN1_MARGIN, pModal->bswMargin[i]);
                    OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
                        AR_PHY_GAIN_2GHZ_XATTEN1_DB, pModal->bswAtten[i]);
                    OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
                        AR_PHY_GAIN_2GHZ_XATTEN2_MARGIN,
                        pModal->xatten2Margin[i]);
                    OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
                        AR_PHY_GAIN_2GHZ_XATTEN2_DB, pModal->xatten2Db[i]);
                } else {
                    OS_REG_WRITE(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
                        ( OS_REG_READ(ah, AR_PHY_GAIN_2GHZ + regChainOffset) &
                          ~AR_PHY_GAIN_2GHZ_BSW_MARGIN ) |
                        SM(pModal->bswMargin[i], AR_PHY_GAIN_2GHZ_BSW_MARGIN));
                    OS_REG_WRITE(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
                        ( OS_REG_READ(ah, AR_PHY_GAIN_2GHZ + regChainOffset) &
                          ~AR_PHY_GAIN_2GHZ_BSW_ATTEN ) |
                        SM(pModal->bswAtten[i], AR_PHY_GAIN_2GHZ_BSW_ATTEN));
                }
            }

            if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
                /* 
                 * From Merlin(AR9280),
                 *     txRxAttenCh[] maps to chain specific
                 *         xatten1_hyst_margin(0x9848/0xa848 13:7)
                 *     rxTxMarginCh[] maps to chain specific
                 *         xatten2_hyst_margin (0x9848/0xa848 20:14)
                 */
                OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN + regChainOffset,
                    AR9280_PHY_RXGAIN_TXRX_ATTEN, txRxAttenLocal);
                OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN + regChainOffset,
                    AR9280_PHY_RXGAIN_TXRX_MARGIN, pModal->rxTxMarginCh[i]);
            } else {
                OS_REG_WRITE(ah, AR_PHY_RXGAIN + regChainOffset, 
                    ( OS_REG_READ(ah, AR_PHY_RXGAIN + regChainOffset) &
                      ~AR_PHY_RXGAIN_TXRX_ATTEN ) |
                    SM(txRxAttenLocal, AR_PHY_RXGAIN_TXRX_ATTEN));
                OS_REG_WRITE(ah, AR_PHY_GAIN_2GHZ + regChainOffset,
                    ( OS_REG_READ(ah, AR_PHY_GAIN_2GHZ + regChainOffset) &
                      ~AR_PHY_GAIN_2GHZ_RXTX_MARGIN ) |
                    SM(pModal->rxTxMarginCh[i], AR_PHY_GAIN_2GHZ_RXTX_MARGIN));
            }
        }
    }

#if 0   /* Do not run IQ cal on AR5416 */
    /* write previous IQ results */
    OS_REG_SET_BIT(ah, AR_PHY_TIMING_CTRL4, AR_PHY_TIMING_CTRL4_IQCORR_ENABLE);
#endif

    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        u_int32_t regVal;

        if (IS_CHAN_2GHZ(chan)) {
            regVal = OS_REG_READ(ah, AR_AN_RF2G1_CH0);
            regVal &= ~(AR_AN_RF2G1_CH0_OB | AR_AN_RF2G1_CH0_DB);
            regVal |= (SM(pModal->ob, AR_AN_RF2G1_CH0_OB) |
                       SM(pModal->db, AR_AN_RF2G1_CH0_DB));
            analogShiftRegWrite(ah, AR_AN_RF2G1_CH0, regVal);

            regVal= OS_REG_READ(ah, AR_AN_RF2G1_CH1);
            regVal &= ~(AR_AN_RF2G1_CH1_OB | AR_AN_RF2G1_CH1_DB);
            regVal |= (SM(pModal->ob_ch1, AR_AN_RF2G1_CH1_OB) |
                       SM(pModal->db_ch1, AR_AN_RF2G1_CH1_DB));
            analogShiftRegWrite(ah, AR_AN_RF2G1_CH1, regVal);
        } else {
            regVal = OS_REG_READ(ah, AR_AN_RF5G1_CH0);
            regVal &= ~(AR_AN_RF5G1_CH0_OB5 | AR_AN_RF5G1_CH0_DB5);
            regVal |= (SM(pModal->ob, AR_AN_RF5G1_CH0_OB5) |
                       SM(pModal->db, AR_AN_RF5G1_CH0_DB5));
            analogShiftRegWrite(ah, AR_AN_RF5G1_CH0, regVal);

            regVal = OS_REG_READ(ah, AR_AN_RF5G1_CH1);
            regVal &= ~(AR_AN_RF5G1_CH1_OB5 | AR_AN_RF5G1_CH1_DB5);
            regVal |= (SM(pModal->ob_ch1, AR_AN_RF5G1_CH1_OB5) |
                       SM(pModal->db_ch1, AR_AN_RF5G1_CH1_DB5));
            analogShiftRegWrite(ah, AR_AN_RF5G1_CH1, regVal);
        }
        // Update XPABias and LocalBias
        regVal = OS_REG_READ(ah, AR_AN_TOP2);
        regVal &= ~(AR_AN_TOP2_XPABIAS_LVL | AR_AN_TOP2_LOCALBIAS);
        regVal |= (SM(pModal->xpaBiasLvl, AR_AN_TOP2_XPABIAS_LVL) |
                   SM(pModal->local_bias, AR_AN_TOP2_LOCALBIAS));
        analogShiftRegWrite(ah, AR_AN_TOP2, regVal);

        HDPRINTF(ah, HAL_DBG_EEPROM, "ForceXPAon: %d\n",pModal->force_xpaon);
        OS_REG_RMW_FIELD(ah, AR_PHY_XPA_CFG, AR_PHY_FORCE_XPA_CFG,
                         pModal->force_xpaon); 

        /* Green Tx */
        /* for UB94, which ath_hal_sta_update_tx_pwr_enable is enabled */
        if (ap->ah_opmode == HAL_M_STA && ap->ah_config.ath_hal_sta_update_tx_pwr_enable) {
            for (i = 0; i < 9; i++) {
                regList[i][1] = OS_REG_READ(ah, regList[i][0]);
            }
       
            for (i = 0; i < 9; i++) {
                AH5416(ah)->txPowerRecord[0][i]  = 0x0;
                if ((regList[i][1]&TXPOWER(0)) < 0x14 ) {
                    AH5416(ah)->txPowerRecord[0][i] |= (regList[i][1]&TXPOWER(0));
                } else {
                    AH5416(ah)->txPowerRecord[0][i] |= 0x14;
                }
                if ((regList[i][1]&TXPOWER(1)) < 0x1400 ) {
                    AH5416(ah)->txPowerRecord[0][i] |= (regList[i][1]&TXPOWER(1));
                } else {
                    AH5416(ah)->txPowerRecord[0][i] |= 0x1400;
                }
                if ((regList[i][1]&TXPOWER(2)) < 0x140000 ) {
                    AH5416(ah)->txPowerRecord[0][i] |= (regList[i][1]&TXPOWER(2));
                } else {
                    AH5416(ah)->txPowerRecord[0][i] |= 0x140000;
                }
                if ((regList[i][1]&TXPOWER(3)) < 0x14000000 ) {
                    AH5416(ah)->txPowerRecord[0][i] |= (regList[i][1]&TXPOWER(3));
                } else {
                    AH5416(ah)->txPowerRecord[0][i] |= 0x14000000;
                }
            }
                   
            for (i = 0; i < 9; i++) {
                AH5416(ah)->txPowerRecord[1][i]  = 0x0;
                if ((regList[i][1]&TXPOWER(0)) < 0x1E ) {
                    AH5416(ah)->txPowerRecord[1][i] |= (regList[i][1]&TXPOWER(0));
                } else {
                    AH5416(ah)->txPowerRecord[1][i] |= 0x1E;
                }
                if ((regList[i][1]&TXPOWER(1)) < 0x1E00 ) {
                    AH5416(ah)->txPowerRecord[1][i] |= (regList[i][1]&TXPOWER(1));
                } else {
                    AH5416(ah)->txPowerRecord[1][i] |= 0x1E00;
                }
                if ((regList[i][1]&TXPOWER(2)) < 0x1E0000 ) {
                    AH5416(ah)->txPowerRecord[1][i] |= (regList[i][1]&TXPOWER(2));
                } else {
                    AH5416(ah)->txPowerRecord[1][i] |= 0x1E0000;
                }
                if ((regList[i][1]&TXPOWER(3)) < 0x1E000000 ) {
                    AH5416(ah)->txPowerRecord[1][i] |= (regList[i][1]&TXPOWER(3));
                } else {
                    AH5416(ah)->txPowerRecord[1][i] |= 0x1E000000;
                }
            }
          
            //original target power for status 3
            AH5416(ah)->txPowerRecord[2][0] = regList[0][1];
            AH5416(ah)->txPowerRecord[2][1] = regList[1][1];
            AH5416(ah)->txPowerRecord[2][2] = regList[2][1];
            AH5416(ah)->txPowerRecord[2][3] = regList[3][1];
            AH5416(ah)->txPowerRecord[2][4] = regList[4][1];
            AH5416(ah)->txPowerRecord[2][5] = regList[5][1];
            AH5416(ah)->txPowerRecord[2][6] = regList[6][1];
            AH5416(ah)->txPowerRecord[2][7] = regList[7][1];
            AH5416(ah)->txPowerRecord[2][8] = regList[8][1];
        }
    }

	ar5416EepDefMinCCAPwrThresApply(ah, chan->channel);

    OS_REG_RMW_FIELD(ah, AR_PHY_SETTLING, AR_PHY_SETTLING_SWITCH,
                     pModal->switchSettling);
    OS_REG_RMW_FIELD(ah, AR_PHY_DESIRED_SZ, AR_PHY_DESIRED_SZ_ADC,
                     pModal->adcDesiredSize);
    
    if (!AR_SREV_MERLIN_10_OR_LATER(ah)) {
        OS_REG_RMW_FIELD(ah, AR_PHY_DESIRED_SZ, AR_PHY_DESIRED_SZ_PGA,
                         pModal->pgaDesiredSize);
    }
    
    OS_REG_WRITE(ah, AR_PHY_RF_CTL4,
        SM(pModal->txEndToXpaOff, AR_PHY_RF_CTL4_TX_END_XPAA_OFF)
        | SM(pModal->txEndToXpaOff, AR_PHY_RF_CTL4_TX_END_XPAB_OFF)
        | SM(pModal->txFrameToXpaOn, AR_PHY_RF_CTL4_FRAME_XPAA_ON)
        | SM(pModal->txFrameToXpaOn, AR_PHY_RF_CTL4_FRAME_XPAB_ON));

    OS_REG_RMW_FIELD(ah, AR_PHY_RF_CTL3, AR_PHY_TX_END_TO_A2_RX_ON,
                     pModal->txEndToRxOn);
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        OS_REG_RMW_FIELD(ah, AR_PHY_CCA, AR9280_PHY_CCA_THRESH62,
                         pModal->thresh62);
        OS_REG_RMW_FIELD(ah, AR_PHY_EXT_CCA0, AR_PHY_EXT_CCA0_THRESH62,
                         pModal->thresh62);
    } else {
        OS_REG_RMW_FIELD(ah, AR_PHY_CCA, AR_PHY_CCA_THRESH62,
                         pModal->thresh62);
        OS_REG_RMW_FIELD(ah, AR_PHY_EXT_CCA, AR_PHY_EXT_CCA_THRESH62,
                         pModal->thresh62);
    }

    /* Minor Version Specific Application */
    if ((eep->baseEepHeader.version & AR5416_EEP_VER_MINOR_MASK) >=
        AR5416_EEP_MINOR_VER_2)
    {
        OS_REG_RMW_FIELD(ah, AR_PHY_RF_CTL2, AR_PHY_TX_END_DATA_START,
                         pModal->txFrameToDataStart);
        OS_REG_RMW_FIELD(ah, AR_PHY_RF_CTL2, AR_PHY_TX_END_PA_ON,
                         pModal->txFrameToPaOn);
    }

    if ((eep->baseEepHeader.version & AR5416_EEP_VER_MINOR_MASK) >=
        AR5416_EEP_MINOR_VER_3)
    {
        if (IS_CHAN_HT40(chan))
        {
            /* Overwrite switch settling with HT40 value */
            OS_REG_RMW_FIELD(ah, AR_PHY_SETTLING, AR_PHY_SETTLING_SWITCH,
                             pModal->swSettleHt40);
        }
    }

    if (AR_SREV_MERLIN_20_OR_LATER(ah) && 
        ((eep->baseEepHeader.version & AR5416_EEP_VER_MINOR_MASK) >=
         AR5416_EEP_MINOR_VER_19))
    {
        OS_REG_RMW_FIELD(ah, AR_PHY_CCK_TX_CTRL,
            AR_PHY_CCK_TX_CTRL_TX_DAC_SCALE_CCK, pModal->miscBits);
    }

    if (AR_SREV_MERLIN_20(ah) && 
        ((eep->baseEepHeader.version & AR5416_EEP_VER_MINOR_MASK) >=
         AR5416_EEP_MINOR_VER_20))
    {
        if (IS_CHAN_2GHZ(chan)) {
            analogShiftRegRMW(ah, AR_AN_TOP1, AR_AN_TOP1_DACIPMODE,
                AR_AN_TOP1_DACIPMODE_S, eep->baseEepHeader.dacLpMode);
        } else {
            if (eep->baseEepHeader.dacHiPwrMode_5G) {
                analogShiftRegRMW(ah, AR_AN_TOP1, AR_AN_TOP1_DACIPMODE,
                    AR_AN_TOP1_DACIPMODE_S, 0);
            } else {
                analogShiftRegRMW(ah, AR_AN_TOP1, AR_AN_TOP1_DACIPMODE,
                    AR_AN_TOP1_DACIPMODE_S, eep->baseEepHeader.dacLpMode);
            }
        }
        OS_DELAY(100);

        OS_REG_RMW_FIELD(ah, AR_PHY_FRAME_CTL, AR_PHY_FRAME_CTL_TX_CLIP,
                         pModal->miscBits >> 2);
        OS_REG_RMW_FIELD(ah, AR_PHY_TX_PWRCTRL9, AR_PHY_TX_DESIRED_SCALE_CCK,
                         eep->baseEepHeader.desiredScaleCCK);
    }

    return AH_TRUE;
}


u_int32_t
ar5416EepromDefINIFixup(struct ath_hal *ah,ar5416_eeprom_def_t *pEepData, u_int32_t reg, u_int32_t value)
{
    BASE_EEPDEF_HEADER  *pBase  = &(pEepData->baseEepHeader);
    
    switch (AH_PRIVATE(ah)->ah_devid)
    {
    case AR5416_DEVID_AR9280_PCI:
        /*
        ** Need to set the external/internal regulator bit to the proper value.
        ** Can only write this ONCE.
        */
        
        if ( reg == 0x7894 )
        {
            /*
            ** Check for an EEPROM data structure of "0x0b" or better
            */
        
            HDPRINTF(ah, HAL_DBG_EEPROM, "ini VAL: %x  EEPROM: %x\n", 
                     value,(pBase->version & 0xff));

            if ( (pBase->version & 0xff) > 0x0a) {
                HDPRINTF(ah, HAL_DBG_EEPROM, "PWDCLKIND: %d\n",pBase->pwdclkind);
                value &= ~AR_AN_TOP2_PWDCLKIND;
                value |= AR_AN_TOP2_PWDCLKIND & ( pBase->pwdclkind <<  AR_AN_TOP2_PWDCLKIND_S);
            } else {
                HDPRINTF(ah, HAL_DBG_EEPROM, "PWDCLKIND Earlier Rev\n");
            }

            HDPRINTF(ah, HAL_DBG_EEPROM, "final ini VAL: %x\n", value);
        }
        break;
        
    }
    
    return (value);
}


/**************************************************************
 * ar5416EepromDefSetPowerPerRateTable
 *
 * Sets the transmit power in the baseband for the given
 * operating channel and mode.
 */
HAL_BOOL
ar5416EepromDefSetPowerPerRateTable(struct ath_hal *ah, ar5416_eeprom_def_t *pEepData,
    HAL_CHANNEL_INTERNAL *chan,
    int16_t *ratesArray, u_int16_t cfgCtl,
    u_int16_t AntennaReduction,
    u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit, u_int8_t chainmask)
{
    /* Local defines to distinguish between extension and control CTL's */
#define EXT_ADDITIVE (0x8000)
#define CTL_11A_EXT (CTL_11A | EXT_ADDITIVE)
#define CTL_11G_EXT (CTL_11G | EXT_ADDITIVE)
#define CTL_11B_EXT (CTL_11B | EXT_ADDITIVE)
#define REDUCE_SCALED_POWER_BY_TWO_CHAIN     6  /* 10*log10(2)*2 */
#define REDUCE_SCALED_POWER_BY_THREE_CHAIN  10  /* 10*log10(3)*2 */
#define PWRINCR_3_TO_1_CHAIN      9             /* 10*log(3)*2 */
#define PWRINCR_3_TO_2_CHAIN      3             /* floor(10*log(3/2)*2) */
#define PWRINCR_2_TO_1_CHAIN      6             /* 10*log(2)*2 */

    u_int16_t twiceMaxEdgePower = AR5416_MAX_RATE_POWER;
    static const u_int16_t tpScaleReductionTable[5] = { 0, 3, 6, 9, AR5416_MAX_RATE_POWER };

    int i;
    int16_t  twiceLargestAntenna;
    CAL_CTL_DATA_EEPDEF *rep;
    CAL_TARGET_POWER_LEG targetPowerOfdm, targetPowerCck = {0, {0, 0, 0, 0}};
    CAL_TARGET_POWER_LEG targetPowerOfdmExt = {0, {0, 0, 0, 0}}, targetPowerCckExt = {0, {0, 0, 0, 0}};
    CAL_TARGET_POWER_HT  targetPowerHt20, targetPowerHt40 = {0, {0, 0, 0, 0}};
    int16_t scaledPower=0, minCtlPower, maxRegAllowedPower;
    #define SUB_NUM_CTL_MODES_AT_5G_40 2    /* excluding HT40, EXT-OFDM */
    #define SUB_NUM_CTL_MODES_AT_2G_40 3    /* excluding HT40, EXT-OFDM, EXT-CCK */
    u_int16_t ctlModesFor11a[] = {CTL_11A, CTL_5GHT20, CTL_11A_EXT, CTL_5GHT40};
    u_int16_t ctlModesFor11g[] = {CTL_11B, CTL_11G, CTL_2GHT20, CTL_11B_EXT, CTL_11G_EXT, CTL_2GHT40};
    u_int16_t numCtlModes, *pCtlMode, ctlMode, freq;
    CHAN_CENTERS centers;
    int tx_chainmask;
    u_int16_t twiceMinEdgePower;
    struct ath_hal_5416 *ahp = AH5416(ah);

    tx_chainmask = chainmask ? chainmask : ahp->ah_txchainmask;

    ar5416GetChannelCenters(ah, chan, &centers);

    /* Compute TxPower reduction due to Antenna Gain */
    twiceLargestAntenna = AH_MAX(AH_MAX(pEepData->modalHeader[IS_CHAN_2GHZ(chan)].antennaGainCh[0],
        pEepData->modalHeader[IS_CHAN_2GHZ(chan)].antennaGainCh[1]),
        pEepData->modalHeader[IS_CHAN_2GHZ(chan)].antennaGainCh[2]);
    twiceLargestAntenna =  (int16_t)AH_MIN((AntennaReduction) - twiceLargestAntenna, 0);

    /* scaledPower is the minimum of the user input power level and the regulatory allowed power level */
    maxRegAllowedPower = twiceMaxRegulatoryPower + twiceLargestAntenna;

    /* Use ah_tp_scale - see bug 30070.*/
    if (AH_PRIVATE(ah)->ah_tp_scale != HAL_TP_SCALE_MAX) { 
        maxRegAllowedPower -= (tpScaleReductionTable[(AH_PRIVATE(ah)->ah_tp_scale)] * 2);
    }

    scaledPower = AH_MIN(powerLimit, maxRegAllowedPower);

    /* Reduce scaled Power by number of chains active to get to per chain tx power level */
    /* TODO: better value than these? */
    switch (owl_get_ntxchains(tx_chainmask))
    {
    case 1:
        break;
    case 2:
        scaledPower -= REDUCE_SCALED_POWER_BY_TWO_CHAIN;
        break;
    case 3:
        scaledPower -= REDUCE_SCALED_POWER_BY_THREE_CHAIN;
        break;
    default:
        HALASSERT(0); /* Unsupported number of chains */
        return AH_FALSE;
    }

    scaledPower = AH_MAX(0, scaledPower);

    /*
    * Get target powers from EEPROM - our baseline for TX Power
    */
    if (IS_CHAN_2GHZ(chan))
    {
        /* Setup for CTL modes */
        numCtlModes = N(ctlModesFor11g) - SUB_NUM_CTL_MODES_AT_2G_40; /* CTL_11B, CTL_11G, CTL_2GHT20 */
        pCtlMode = ctlModesFor11g;

        ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPowerCck,
            AR5416_EEPDEF_NUM_2G_CCK_TARGET_POWERS, &targetPowerCck, 4, AH_FALSE);
        ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPower2G,
            AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS, &targetPowerOfdm, 4, AH_FALSE);
        ar5416GetTargetPowers(ah, chan, pEepData->calTargetPower2GHT20,
            AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS, &targetPowerHt20, 8, AH_FALSE);

        if (IS_CHAN_HT40(chan))
        {
            numCtlModes = N(ctlModesFor11g);    /* All 2G CTL's */
            ar5416GetTargetPowers(ah, chan, pEepData->calTargetPower2GHT40,
                AR5416_EEPDEF_NUM_2G_40_TARGET_POWERS, &targetPowerHt40, 8, AH_TRUE);
            ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPowerCck,
                AR5416_EEPDEF_NUM_2G_CCK_TARGET_POWERS, &targetPowerCckExt, 4, AH_TRUE);
            ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPower2G,
                AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS, &targetPowerOfdmExt, 4, AH_TRUE);
        }
    }
    else
    {
        /* Setup for CTL modes */
        numCtlModes = N(ctlModesFor11a) - SUB_NUM_CTL_MODES_AT_5G_40; /* CTL_11A, CTL_5GHT20 */
        pCtlMode = ctlModesFor11a;

        ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPower5G,
            AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS, &targetPowerOfdm, 4, AH_FALSE);
        ar5416GetTargetPowers(ah, chan, pEepData->calTargetPower5GHT20,
            AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS, &targetPowerHt20, 8, AH_FALSE);

        if (IS_CHAN_HT40(chan))
        {
            numCtlModes = N(ctlModesFor11a); /* All 5G CTL's */
            ar5416GetTargetPowers(ah, chan, pEepData->calTargetPower5GHT40,
                AR5416_EEPDEF_NUM_5G_40_TARGET_POWERS, &targetPowerHt40, 8, AH_TRUE);
            ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPower5G,
                AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS, &targetPowerOfdmExt, 4, AH_TRUE);
        }
    }

    /*
    * For MIMO, need to apply regulatory caps individually across dynamically
    * running modes: CCK, OFDM, HT20, HT40
    *
    * The outer loop walks through each possible applicable runtime mode.
    * The inner loop walks through each ctlIndex entry in EEPROM.
    * The ctl value is encoded as [7:4] == test group, [3:0] == test mode.
    *
    */
    for (ctlMode = 0; ctlMode < numCtlModes; ctlMode++)
    {

        HAL_BOOL isHt40CtlMode = (pCtlMode[ctlMode] == CTL_5GHT40) || (pCtlMode[ctlMode] == CTL_2GHT40);
        if (isHt40CtlMode)
        {
            freq = centers.synth_center;
        }
        else if (pCtlMode[ctlMode] & EXT_ADDITIVE)
        {
            freq = centers.ext_center;
        }
        else
        {
            freq = centers.ctl_center;
        }



         /*
         * WAR for AP71 eeprom <= 14.2, does not have HT ctls's 
         * as a workaround, keep previously calculated twiceMaxEdgePower for
         * those CTL modes that do not match in EEPROM.  This is hacky and
         * works because 11g is processed before any HT -- but there is already 
         * other hacky code that has the same dependency.
         * To remove WAR, uncomment the following line.
         */
           
        if (owl_get_eepdef_ver(ahp) == 14 && owl_get_eepdef_rev(ahp) <= 2) 
          twiceMaxEdgePower = AR5416_MAX_RATE_POWER;
         // TODO: Does 14.3 still have this restriction???

        HDPRINTF(ah, HAL_DBG_POWER_MGMT, "LOOP-Mode ctlMode %d < %d, isHt40CtlMode %d, EXT_ADDITIVE %d\n",
            ctlMode, numCtlModes, isHt40CtlMode, (pCtlMode[ctlMode] & EXT_ADDITIVE));
        /* walk through each CTL index stored in EEPROM */
        for (i = 0; (i < AR5416_EEPDEF_NUM_CTLS) && pEepData->ctlIndex[i]; i++)
        {

            HDPRINTF(ah, HAL_DBG_POWER_MGMT, 
                "  LOOP-Ctlidx %d: cfgCtl 0x%2.2x pCtlMode 0x%2.2x ctlIndex 0x%2.2x chan %d chanctl 0x%x\n",
                i, cfgCtl, pCtlMode[ctlMode], pEepData->ctlIndex[i], 
                chan->channel, chan->conformance_test_limit);

            /* compare test group from regulatory channel list with test mode from pCtlMode list */
            if ((((cfgCtl & ~CTL_MODE_M) | (pCtlMode[ctlMode] & CTL_MODE_M)) == pEepData->ctlIndex[i]) ||
                (((cfgCtl & ~CTL_MODE_M) | (pCtlMode[ctlMode] & CTL_MODE_M)) == ((pEepData->ctlIndex[i] & CTL_MODE_M) | SD_NO_CTL)))
            {
                rep = &(pEepData->ctlData[i]);
                
                twiceMinEdgePower = ar5416EepDefGetMaxEdgePower(freq,
                                                          rep->ctlEdges[owl_get_ntxchains(tx_chainmask) - 1], 
                                                          IS_CHAN_2GHZ(chan));

                HDPRINTF(ah, HAL_DBG_POWER_MGMT,
                         "    MATCH-EE_IDX %d: ch %d is2 %d 2xMinEdge %d chainmask %d chains %d\n",
                         i, freq, IS_CHAN_2GHZ(chan), twiceMinEdgePower, tx_chainmask,
                         owl_get_ntxchains(tx_chainmask));

                if ((cfgCtl & ~CTL_MODE_M) == SD_NO_CTL)
                {
                    /* Find the minimum of all CTL edge powers that apply to this channel */
                    twiceMaxEdgePower = AH_MIN(twiceMaxEdgePower, twiceMinEdgePower);
                }
                else
                {
                    /* specific */
                    twiceMaxEdgePower = twiceMinEdgePower;
                    break;
                }
            }
        }

        minCtlPower = (u_int8_t)AH_MIN(twiceMaxEdgePower, scaledPower);

        HDPRINTF(ah, HAL_DBG_POWER_MGMT, 
                 "    SEL-Min ctlMode %d pCtlMode %d 2xMaxEdge %d sP %d minCtlPwr %d\n",
                 ctlMode, pCtlMode[ctlMode], twiceMaxEdgePower, scaledPower, minCtlPower);


        /* Apply ctl mode to correct target power set */
        switch(pCtlMode[ctlMode])
        {
        case CTL_11B:
            for (i = 0; i < N(targetPowerCck.tPow2x); i++)
            {
                targetPowerCck.tPow2x[i] = (u_int8_t)AH_MIN(targetPowerCck.tPow2x[i], minCtlPower);
            }
            break;
        case CTL_11A:
        case CTL_11G:
            for (i = 0; i < N(targetPowerOfdm.tPow2x); i++)
            {
                targetPowerOfdm.tPow2x[i] = (u_int8_t)AH_MIN(targetPowerOfdm.tPow2x[i], minCtlPower);
            }
            break;
        case CTL_5GHT20:
        case CTL_2GHT20:
            for (i = 0; i < N(targetPowerHt20.tPow2x); i++)
            {
                targetPowerHt20.tPow2x[i] = (u_int8_t)AH_MIN(targetPowerHt20.tPow2x[i], minCtlPower);
            }
            break;
        case CTL_11B_EXT:
            targetPowerCckExt.tPow2x[0] = (u_int8_t)AH_MIN(targetPowerCckExt.tPow2x[0], minCtlPower);
            break;
        case CTL_11A_EXT:
        case CTL_11G_EXT:
            targetPowerOfdmExt.tPow2x[0] = (u_int8_t)AH_MIN(targetPowerOfdmExt.tPow2x[0], minCtlPower);
            break;
        case CTL_5GHT40:
        case CTL_2GHT40:
            for (i = 0; i < N(targetPowerHt40.tPow2x); i++)
            {
                targetPowerHt40.tPow2x[i] = (u_int8_t)AH_MIN(targetPowerHt40.tPow2x[i], minCtlPower);
            }
            break;
        default:
            HALASSERT(0);
            break;
        }
    } /* end ctl mode checking */

    /* Set rates Array from collected data */
    ratesArray[rate6mb] = ratesArray[rate9mb] = ratesArray[rate12mb] = ratesArray[rate18mb] = ratesArray[rate24mb] = targetPowerOfdm.tPow2x[0];
    ratesArray[rate36mb] = targetPowerOfdm.tPow2x[1];
    ratesArray[rate48mb] = targetPowerOfdm.tPow2x[2];
    ratesArray[rate54mb] = targetPowerOfdm.tPow2x[3];
    ratesArray[rateXr] = targetPowerOfdm.tPow2x[0];

    for (i = 0; i < N(targetPowerHt20.tPow2x); i++)
    {
        ratesArray[rateHt20_0 + i] = targetPowerHt20.tPow2x[i];
    }

    if (IS_CHAN_2GHZ(chan))
    {
        ratesArray[rate1l]  = targetPowerCck.tPow2x[0];
        ratesArray[rate2s] = ratesArray[rate2l]  = targetPowerCck.tPow2x[1];
        ratesArray[rate5_5s] = ratesArray[rate5_5l] = targetPowerCck.tPow2x[2];
        ;
        ratesArray[rate11s] = ratesArray[rate11l] = targetPowerCck.tPow2x[3];
        ;
    }
    if (IS_CHAN_HT40(chan))
    {
        for (i = 0; i < N(targetPowerHt40.tPow2x); i++)
        {
            ratesArray[rateHt40_0 + i] = targetPowerHt40.tPow2x[i];
        }
        ratesArray[rateDupOfdm] = targetPowerHt40.tPow2x[0];
        ratesArray[rateDupCck]  = targetPowerHt40.tPow2x[0];
        ratesArray[rateExtOfdm] = targetPowerOfdmExt.tPow2x[0];
        if (IS_CHAN_2GHZ(chan))
        {
            ratesArray[rateExtCck]  = targetPowerCckExt.tPow2x[0];
        }
    }
    return AH_TRUE;
#undef EXT_ADDITIVE
#undef CTL_11A_EXT
#undef CTL_11G_EXT
#undef CTL_11B_EXT
#undef REDUCE_SCALED_POWER_BY_TWO_CHAIN
#undef REDUCE_SCALED_POWER_BY_THREE_CHAIN
}

/**************************************************************
 * ar5416EepDefGetGainBoundariesAndPdadcs
 *
 * Uses the data points read from EEPROM to reconstruct the pdadc power table
 * Called by ar5416SetPowerCalTable only.
 */
static inline HAL_BOOL
ar5416EepDefGetGainBoundariesAndPdadcs(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan, CAL_DATA_PER_FREQ_EEPDEF *pRawDataSet,
    u_int8_t * bChans,  u_int16_t availPiers,
    u_int16_t tPdGainOverlap, int16_t *pMinCalPower, u_int16_t * pPdGainBoundaries,
    u_int8_t * pPDADCValues, u_int16_t numXpdGains)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int       i, j, k;
    int16_t   ss;         /* potentially -ve index for taking care of pdGainOverlap */
    u_int16_t  idxL = 0, idxR = 0, numPiers; /* Pier indexes */
    u_int8_t   *pVpdL, *pVpdR, *pPwrL, *pPwrR;
    u_int8_t   minPwrT4[AR5416_EEPDEF_NUM_PD_GAINS] = {0};
    u_int8_t   maxPwrT4[AR5416_EEPDEF_NUM_PD_GAINS];
    int16_t   vpdStep;
    int16_t   tmpVal;
    u_int16_t  sizeCurrVpdTable, maxIndex, tgtIndex;
    HAL_BOOL    match;
    int16_t  minDelta = 0;
    CHAN_CENTERS centers;

    ar5416GetChannelCenters(ah, chan, &centers);

    /* Trim numPiers for the number of populated channel Piers */
    for (numPiers = 0; numPiers < availPiers; numPiers++)
    {
        if (bChans[numPiers] == AR5416_BCHAN_UNUSED)
        {
            break;
        }
    }

    /* Find pier indexes around the current channel */
    match = getLowerUpperIndex((u_int8_t)FREQ2FBIN(centers.synth_center, IS_CHAN_2GHZ(chan)),
        bChans, numPiers, &idxL,
        &idxR);

    if (match)
    {
        /* Directly fill both vpd tables from the matching index */
        for (i = 0; i < numXpdGains; i++)
        {
            minPwrT4[i] = pRawDataSet[idxL].pwrPdg[i][0];
            maxPwrT4[i] = pRawDataSet[idxL].pwrPdg[i][4];
            ar5416FillVpdTable(minPwrT4[i], maxPwrT4[i], pRawDataSet[idxL].pwrPdg[i],
                pRawDataSet[idxL].vpdPdg[i], AR5416_EEPDEF_PD_GAIN_ICEPTS, ahp->ah_vpdTableI[i]);
        }
    }
    else
    {
        for (i = 0; i < numXpdGains; i++)
        {
            pVpdL = pRawDataSet[idxL].vpdPdg[i];
            pPwrL = pRawDataSet[idxL].pwrPdg[i];
            pVpdR = pRawDataSet[idxR].vpdPdg[i];
            pPwrR = pRawDataSet[idxR].pwrPdg[i];

            /* Start Vpd interpolation from the max of the minimum powers */
            minPwrT4[i] = AH_MAX(pPwrL[0], pPwrR[0]);

            /* End Vpd interpolation from the min of the max powers */
            maxPwrT4[i] = AH_MIN(pPwrL[AR5416_EEPDEF_PD_GAIN_ICEPTS - 1], pPwrR[AR5416_EEPDEF_PD_GAIN_ICEPTS - 1]);
            HALASSERT(maxPwrT4[i] > minPwrT4[i]);

            /* Fill pier Vpds */
            ar5416FillVpdTable(minPwrT4[i], maxPwrT4[i], pPwrL, pVpdL, AR5416_EEPDEF_PD_GAIN_ICEPTS, ahp->ah_vpdTableL[i]);
            ar5416FillVpdTable(minPwrT4[i], maxPwrT4[i], pPwrR, pVpdR, AR5416_EEPDEF_PD_GAIN_ICEPTS, ahp->ah_vpdTableR[i]);

            /* Interpolate the final vpd */
            for (j = 0; j <= (maxPwrT4[i] - minPwrT4[i]) / 2; j++)
            {
                ahp->ah_vpdTableI[i][j] = (u_int8_t)(interpolate((u_int16_t)FREQ2FBIN(centers.synth_center, IS_CHAN_2GHZ(chan)),
                    bChans[idxL], bChans[idxR], ahp->ah_vpdTableL[i][j], ahp->ah_vpdTableR[i][j]));
            }
        }
    }
    *pMinCalPower = (int16_t)(minPwrT4[0] / 2);

    k = 0; /* index for the final table */
    for (i = 0; i < numXpdGains; i++)
    {
        if (i == (numXpdGains - 1))
        {
            pPdGainBoundaries[i] = (u_int16_t)(maxPwrT4[i] / 2);
        }
        else
        {
            pPdGainBoundaries[i] = (u_int16_t)((maxPwrT4[i] + minPwrT4[i+1]) / 4);
        }

        pPdGainBoundaries[i] = (u_int16_t)AH_MIN(AR5416_MAX_RATE_POWER, pPdGainBoundaries[i]);


        /*
        * WORKAROUND for 5416 1.0 until we get a per chain gain boundary 
        * register. This is not the best solution
        */
        if ((i == 0) &&
            !AR_SREV_5416_V20_OR_LATER(ah))
        {
            //fix the gain delta, but get a delta that can be applied to min to
            //keep the upper power values accurate, don't think max needs to
            //be adjusted because should not be at that area of the table?
            minDelta = pPdGainBoundaries[0] - 23;
            pPdGainBoundaries[0] = 23;
        }
        else
        {
            minDelta = 0;
        }

        /* Find starting index for this pdGain */
        if (i == 0)
        {
            if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
                ss = (int16_t)(0 - (minPwrT4[i] / 2));
            } else {
                ss = 0;  /* for the first pdGain, start from index 0 */
            }
        }
        else
        {
            /* Need overlap entries extrapolated below */
            ss = (int16_t)((pPdGainBoundaries[i-1] - (minPwrT4[i] / 2)) - tPdGainOverlap + 1 + minDelta);
        }
        vpdStep = (int16_t)(ahp->ah_vpdTableI[i][1] - ahp->ah_vpdTableI[i][0]);
        vpdStep = (int16_t)((vpdStep < 1) ? 1 : vpdStep);
        /*
        *-ve ss indicates need to extrapolate data below for this pdGain
        */
        while ((ss < 0) && (k < (AR5416_NUM_PDADC_VALUES - 1)))
        {
            tmpVal = (int16_t)(ahp->ah_vpdTableI[i][0] + ss * vpdStep);
            pPDADCValues[k++] = (u_int8_t)((tmpVal < 0) ? 0 : tmpVal);
            ss++;
        }

        sizeCurrVpdTable = (u_int8_t)((maxPwrT4[i] - minPwrT4[i]) / 2 + 1);
        tgtIndex = (u_int8_t)(pPdGainBoundaries[i] + tPdGainOverlap - (minPwrT4[i] / 2));
        maxIndex = (tgtIndex < sizeCurrVpdTable) ? tgtIndex : sizeCurrVpdTable;

        while ((ss < maxIndex) && (k < (AR5416_NUM_PDADC_VALUES - 1)))
        {
            pPDADCValues[k++] = ahp->ah_vpdTableI[i][ss++];
        }

        vpdStep = (int16_t)(ahp->ah_vpdTableI[i][sizeCurrVpdTable - 1] - ahp->ah_vpdTableI[i][sizeCurrVpdTable - 2]);
        vpdStep = (int16_t)((vpdStep < 1) ? 1 : vpdStep);
        /*
        * for last gain, pdGainBoundary == Pmax_t2, so will
        * have to extrapolate
        */
        if (tgtIndex >= maxIndex)
        {  /* need to extrapolate above */
            while ((ss <= tgtIndex) && (k < (AR5416_NUM_PDADC_VALUES - 1)))
            {
                tmpVal = (int16_t)((ahp->ah_vpdTableI[i][sizeCurrVpdTable - 1] +
                    (ss - maxIndex + 1) * vpdStep));
                pPDADCValues[k++] = (u_int8_t)((tmpVal > 255) ? 255 : tmpVal);
                ss++;
            }
        }               /* extrapolated above */
    }                   /* for all pdGainUsed */

    /* Fill out pdGainBoundaries - only up to 2 allowed here, but hardware allows up to 4 */
    while (i < AR5416_EEPDEF_PD_GAINS_IN_MASK)
    {
        pPdGainBoundaries[i] = pPdGainBoundaries[i-1];
        i++;
    }

    while (k < AR5416_NUM_PDADC_VALUES)
    {
        pPDADCValues[k] = pPDADCValues[k-1];
        k++;
    }

    return AH_TRUE;
}

/**************************************************************
 * ar5416GetTxGainIndex
 * - Description  : When the calibrated using the open loop power 
 *                  control scheme, this routine retrieves the Tx gain table
 *                  index for the pcdac that was used to calibrate the board.
 * - Arguments
 *     -      
 * - Returns      : 
 */
static inline void
ar5416GetTxGainIndex(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan, CAL_DATA_PER_FREQ_OP_LOOP *pRawDatasetOpLoop,
    u_int8_t *pCalChans,  u_int16_t availPiers,
    u_int8_t *pPwr, u_int8_t *pPcdacIdx)
{
    u_int8_t pcdac, i = 0;
    u_int16_t  idxL=0, idxR=0, numPiers; /* Pier indexes */
    HAL_BOOL match;
    struct ath_hal_5416     *ahp = AH5416(ah);
    CHAN_CENTERS centers;

    ar5416GetChannelCenters(ah, chan, &centers);

    /* Get the number of populated channel Piers */
    for (numPiers = 0; numPiers < availPiers; numPiers++) {
        if (pCalChans[numPiers] == AR5416_BCHAN_UNUSED) {
            break;
        }
    }

    /* Find where the current channel is in relation to the pier frequencies 
     * that were used during calibration
     */
    match = getLowerUpperIndex(
        (u_int8_t)FREQ2FBIN(centers.synth_center, IS_CHAN_2GHZ(chan)),
        pCalChans, 
        numPiers, 
        &idxL, 
        &idxR);

    if (match) {
        /* Directly fill both vpd tables from the matching index */
        pcdac = pRawDatasetOpLoop[idxL].pcdac[0][0];
        *pPwr = pRawDatasetOpLoop[idxL].pwrPdg[0][0];
    } else {
        pcdac = pRawDatasetOpLoop[idxR].pcdac[0][0];
        *pPwr = (pRawDatasetOpLoop[idxL].pwrPdg[0][0] + pRawDatasetOpLoop[idxR].pwrPdg[0][0])/2;
    }

    /* find entry closest */
    while ((pcdac > ahp->originalGain[i]) && (i < (MERLIN_TX_GAIN_TABLE_SIZE -1)) ) 
        {i++;}

    *pPcdacIdx = i;
    return;
}

/**************************************************************
 * ar5416EepromDefSetPowerCalTable
 *
 * Pull the PDADC piers from cal data and interpolate them across the given
 * points as well as from the nearest pier(s) to get a power detector
 * linear voltage to power level table.
 */
static inline HAL_BOOL
ar5416EepromDefSetPowerCalTable(struct ath_hal *ah, ar5416_eeprom_def_t *pEepData, HAL_CHANNEL_INTERNAL *chan, int16_t *pTxPowerIndexOffset)
{
    CAL_DATA_PER_FREQ_EEPDEF *pRawDataset;
    u_int8_t  *pCalBChans = AH_NULL;
    u_int16_t pdGainOverlap_t2;
    u_int8_t  pdadcValues[AR5416_NUM_PDADC_VALUES] = {0};
    u_int16_t gainBoundaries[AR5416_EEPDEF_PD_GAINS_IN_MASK] = {0};
    u_int16_t numPiers, i, j, k;
    int16_t  tMinCalPower, diff = 0;
    u_int16_t numXpdGain, xpdMask;
    u_int16_t xpdGainValues[AR5416_EEPDEF_NUM_PD_GAINS] = {0, 0, 0, 0};
    u_int32_t reg32, regOffset, regChainOffset;
    int16_t   modalIdx;
    struct ath_hal_5416 *ahp = AH5416(ah);
    int8_t pwrTableOffset = (int8_t)ar5416EepromGet(ahp, EEP_PWR_TABLE_OFFSET);

#ifdef ATH_TX99_DIAG
    AH_PRIVATE(ah)->ah_pwr_offset = pwrTableOffset;
#endif

    modalIdx = IS_CHAN_2GHZ(chan) ? 1 : 0;
    xpdMask = pEepData->modalHeader[modalIdx].xpdGain;

    if ((pEepData->baseEepHeader.version & AR5416_EEP_VER_MINOR_MASK) >= AR5416_EEP_MINOR_VER_2)
    {
        pdGainOverlap_t2 = pEepData->modalHeader[modalIdx].pdGainOverlap;
    }
    else
    {
        pdGainOverlap_t2 = (u_int16_t)(MS(OS_REG_READ(ah, AR_PHY_TPCRG5), AR_PHY_TPCRG5_PD_GAIN_OVERLAP));
    }

    if (IS_CHAN_2GHZ(chan))
    {
        pCalBChans = pEepData->calFreqPier2G;
        numPiers = AR5416_EEPDEF_NUM_2G_CAL_PIERS;
    }
    else
    {
        pCalBChans = pEepData->calFreqPier5G;
        numPiers = AR5416_EEPDEF_NUM_5G_CAL_PIERS;
    }

    if (AR_SREV_MERLIN_20_OR_LATER(ah) && ar5416EepromGet(ahp, EEP_OL_PWRCTRL) && IS_CHAN_2GHZ(chan)) {
        pRawDataset = pEepData->calPierData2G[0];
        ahp->initPDADC = ((CAL_DATA_PER_FREQ_OP_LOOP*)pRawDataset)->vpdPdg[0][0];
    }

    numXpdGain = 0;
    /* Calculate the value of xpdgains from the xpdGain Mask */
    for (i = 1; i <= AR5416_EEPDEF_PD_GAINS_IN_MASK; i++)
    {
        if ((xpdMask >> (AR5416_EEPDEF_PD_GAINS_IN_MASK - i)) & 1)
        {
            if (numXpdGain >= AR5416_EEPDEF_NUM_PD_GAINS)
            {
                HALASSERT(0);
                break;
            }
            xpdGainValues[numXpdGain] = (u_int16_t)(AR5416_EEPDEF_PD_GAINS_IN_MASK - i);
            numXpdGain++;
        }
    }

    /* write the detector gain biases and their number */
    OS_REG_RMW_FIELD(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_NUM_PD_GAIN, (numXpdGain - 1) & 0x3);
    OS_REG_RMW_FIELD(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_GAIN_1, xpdGainValues[0]);
    OS_REG_RMW_FIELD(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_GAIN_2, xpdGainValues[1]);
    OS_REG_RMW_FIELD(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_GAIN_3, xpdGainValues[2]);

    for (i = 0; i < AR5416_MAX_CHAINS; i++)
    {
        if (AR_SREV_5416_V20_OR_LATER(ah) &&
            (ahp->ah_rxchainmask == 5 || ahp->ah_txchainmask == 5) &&
            (i != 0))
        {
            /* Regs are swapped from chain 2 to 1 for 5416 2_0 with
            * only chains 0 and 2 populated 
            */
            regChainOffset = (i == 1) ? 0x2000 : 0x1000;
        }
        else
        {
            regChainOffset = i * 0x1000;
        }
        if (pEepData->baseEepHeader.txMask & (1 << i))
        {
            if (IS_CHAN_2GHZ(chan))
            {
                pRawDataset = pEepData->calPierData2G[i];
            }
            else
            {
                pRawDataset = pEepData->calPierData5G[i];
            }

            if (AR_SREV_MERLIN_20_OR_LATER(ah) && ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
                u_int8_t pcdacIdx;
                u_int8_t txPower;
                ar5416GetTxGainIndex(
                    ah, 
                    chan, 
                    (CAL_DATA_PER_FREQ_OP_LOOP*)pRawDataset, 
                    pCalBChans, 
                    numPiers,
                    &txPower, 
                    &pcdacIdx);
                ar5416OpenLoopPowerControlGetPdadcs(ah, pcdacIdx, txPower/2, pdadcValues);
            } else {

                if (!ar5416EepDefGetGainBoundariesAndPdadcs(ah, chan, pRawDataset,
                    pCalBChans, numPiers,
                    pdGainOverlap_t2,
                    &tMinCalPower, gainBoundaries,
                    pdadcValues, numXpdGain)) {
                    HDPRINTF(ah, HAL_DBG_CHANNEL, "%s:get pdadc failed\n", __func__);
                    return AH_FALSE;
                }
            }

            /* Prior to writing the boundaries or the pdadc vs. power table
             * into the chip registers the default starting point on the pdadc
             * vs. power table needs to be checked and the curve boundaries
             * adjusted accordingly
             */
            if (AR_SREV_MERLIN_20_OR_LATER(ah)) {
                u_int16_t gainBoundLimit;
                if (AR5416_PWR_TABLE_OFFSET_DB != pwrTableOffset) {
                    /* get the difference in dB */
                    diff = (u_int16_t)(pwrTableOffset - AR5416_PWR_TABLE_OFFSET_DB);
                    /* get the number of half dB steps */
                    diff *= 2; 
                    /* change the original gain boundary settings 
                     * by the number of half dB steps
                     */
                    for (k = 0; k < numXpdGain; k++) {
                        gainBoundaries[k] = (u_int16_t)(gainBoundaries[k] - diff);
                    }
                }
                /* Because of a hardware limitation, ensure the gain boundary 
                 * is not larger than (63 - overlap) 
                 */
                gainBoundLimit = (u_int16_t)(AR5416_MAX_RATE_POWER - pdGainOverlap_t2);

                for (k = 0; k < numXpdGain; k++) {
                    gainBoundaries[k] = (u_int16_t)AH_MIN(gainBoundLimit, gainBoundaries[k]);
                }
            }

            if ((i == 0) ||
                AR_SREV_5416_V20_OR_LATER(ah))
            {
                /*
                * Note the pdadc table may not start at 0 dBm power, could be
                * negative or greater than 0.  Need to offset the power
                * values by the amount of minPower for griffin
                */

                if (AR_SREV_MERLIN_20_OR_LATER(ah) && ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {

                    OS_REG_WRITE(ah, AR_PHY_TPCRG5 + regChainOffset,
                        SM(0x6, AR_PHY_TPCRG5_PD_GAIN_OVERLAP) |
                        SM(0x38, AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_1)  |
                        SM(0x38, AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_2)  |
                        SM(0x38, AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_3)  |
                        SM(0x38, AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_4));
                } else {
                    OS_REG_WRITE(ah, AR_PHY_TPCRG5 + regChainOffset,
                        SM(pdGainOverlap_t2, AR_PHY_TPCRG5_PD_GAIN_OVERLAP) |
                        SM(gainBoundaries[0], AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_1)  |
                        SM(gainBoundaries[1], AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_2)  |
                        SM(gainBoundaries[2], AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_3)  |
                        SM(gainBoundaries[3], AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_4));
                }
            }

            /* If this is a board that has a pwrTableOffset that differs from 
             * the default AR5416_PWR_TABLE_OFFSET_DB then the start of the
             * pdadc vs pwr table needs to be adjusted prior to writing to the
             * chip.
             */
            if (AR_SREV_MERLIN_20_OR_LATER(ah)) {
                if (AR5416_PWR_TABLE_OFFSET_DB != pwrTableOffset) {
                    /* shift the table to start at the new offset */
                    for (k = 0; k<((u_int16_t)AR5416_NUM_PDADC_VALUES-diff); k++ ) {
                        pdadcValues[k] = pdadcValues[k+diff];
                    }

                    /* fill the back of the table */
                    for (k = (u_int16_t)(AR5416_NUM_PDADC_VALUES-diff); k < AR5416_NUM_PDADC_VALUES; k++) {
                        pdadcValues[k] = pdadcValues[AR5416_NUM_PDADC_VALUES-diff];
                    }
                }
            }

            /*
            * Write the power values into the baseband power table
            */
			ENABLE_REG_WRITE_BUFFER

            regOffset = AR_PHY_BASE + (672 << 2) + regChainOffset;
            for (j = 0; j < 32; j++)
            {
                reg32 = ((pdadcValues[4*j + 0] & 0xFF) << 0)  |
                    ((pdadcValues[4*j + 1] & 0xFF) << 8)  |
                    ((pdadcValues[4*j + 2] & 0xFF) << 16) |
                    ((pdadcValues[4*j + 3] & 0xFF) << 24) ;
                OS_REG_WRITE(ah, regOffset, reg32);

                HDPRINTF(ah, HAL_DBG_PHY_IO, "PDADC (%d,%4x): %4.4x %8.8x\n", i, regChainOffset, regOffset, reg32);
                HDPRINTF(ah, HAL_DBG_PHY_IO, "PDADC: Chain %d | PDADC %3d Value %3d | PDADC %3d Value %3d | PDADC %3d Value %3d | PDADC %3d Value %3d |\n",
                    i,
                    4*j, pdadcValues[4*j],
                    4*j+1, pdadcValues[4*j + 1],
                    4*j+2, pdadcValues[4*j + 2],
                    4*j+3, pdadcValues[4*j + 3]);

                regOffset += 4;
            }
            OS_REG_WRITE_FLUSH(ah);

			DISABLE_REG_WRITE_BUFFER
        }
    }
    *pTxPowerIndexOffset = 0;

    return AH_TRUE;
}

HAL_STATUS
ar5416EepromDefSetTransmitPower(struct ath_hal *ah,
    ar5416_eeprom_def_t *pEepData, HAL_CHANNEL_INTERNAL *chan, u_int16_t cfgCtl,
    u_int16_t twiceAntennaReduction, u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit)
{
#define POW_SM(_r, _s)     (((_r) & 0x3f) << (_s))

#define INCREASE_MAXPOW_BY_TWO_CHAIN     6  /* 10*log10(2)*2 */
#define INCREASE_MAXPOW_BY_THREE_CHAIN   10 /* 10*log10(3)*2 */

    MODAL_EEPDEF_HEADER *pModal = &(pEepData->modalHeader[IS_CHAN_2GHZ(chan)]);
    struct ath_hal_5416 *ahp = AH5416(ah);
    int16_t ratesArray[Ar5416RateSize];
    int16_t  txPowerIndexOffset = 0;
    u_int16_t  extra_txpow = 0;
    u_int8_t ht40PowerIncForPdadc = 2;
    int32_t  cck_ofdm_delta = 0;
    int i;
    u_int mode;

    HALASSERT(owl_get_eepdef_ver(ahp) == AR5416_EEP_VER);
    
    OS_MEMZERO(ratesArray, sizeof(ratesArray));
    if (ahp->ah_emu_eeprom) return HAL_OK;

    mode = ath_hal_get_curmode(ah, chan);

    /*
     * Frequently, to reduce regulatory testing costs, rather than
     * calibrating the transmit power of 1 chain, 2 chains, and 3 chains,
     * only the maximum supported number of chains will have the transmit
     * power calibrated (e.g. Kite would be calibrated for 1 chain, Merlin
     * for 2 chains, and Owl for 3 chains).
     * If this is the case, then optionally increase the per-chain power
     * when fewer than the maximum number of chains are being used.
     * This feature should usually not be used, because if the reduced
     * number of chains has not been calibrated, applying a general
     * increase could result in exceeding regulatory power limits, if
     * the RF runs a little higher-powered than expected.
     * However, this adjustment can be useful to demonstrate the possibility
     * of increasing the per-chain power used by a reduced number of chains.
     * (But to increase the per-chain power used by a reduced number of
     * chains in a commercial system, there should be separate calibrations
     * for each number of reduced chains.)
     */
    if (AH_PRIVATE(ah)->ah_config.ath_hal_redchn_maxpwr) {
        ar5416EepromDefFixCTLs(ah);
    }

    if ((pEepData->baseEepHeader.version & AR5416_EEP_VER_MINOR_MASK) >=
        AR5416_EEP_MINOR_VER_2)
    {
        ht40PowerIncForPdadc = pModal->ht40PowerIncForPdadc;
    }

    if (ahp->ah_dynconf) {
        for (i = 0; i < Ar5416RateSize; i++) {
            ratesArray[i] = ahp->ah_ratesArray[i];
        }
    } else {
        if (!ar5416EepromDefSetPowerPerRateTable(ah, pEepData, chan,
            &ratesArray[0],cfgCtl,
            twiceAntennaReduction,
            twiceMaxRegulatoryPower, powerLimit, 0))
        {
            HDPRINTF(ah, HAL_DBG_EEPROM,
                "ar5416EepromSetTransmitPower: "
                "unable to set tx power per rate table\n");
            return HAL_EIO;
        }
        for (i = 0; i < Ar5416RateSize; i++) {
            ahp->ah_ratesArray[i] = ratesArray[i];
        }
    }

    if (!ar5416EepromDefSetPowerCalTable(ah, pEepData, chan, &txPowerIndexOffset))
    {
        HDPRINTF(ah, HAL_DBG_EEPROM, "ar5416EepromSetTransmitPower: unable to set power table\n");
        return HAL_EIO;
    }

    /*
    * txPowerIndexOffset is set by the SetPowerTable() call -
    *  adjust the rate table (0 offset if rates EEPROM not loaded)
    */
    for (i = 0; i < N(ratesArray); i++)
    {
        ratesArray[i] = (int16_t)(txPowerIndexOffset + ratesArray[i]);
        if (ratesArray[i] > AR5416_MAX_RATE_POWER)
            ratesArray[i] = AR5416_MAX_RATE_POWER;
    }

#ifdef EEPROM_DUMP
    ar5416EepDefPrintPowerPerRate(ah, ratesArray, twiceAntennaReduction,
                                  twiceMaxRegulatoryPower, powerLimit);
#endif

    /*
     * Return tx power used to iwconfig.
     * Since power is rate dependent, use one of the indices from the 
     * AR5416_RATES enum to select an entry from ratesArray[] to report.
     * Currently returns the power for HT40 MCS 0, HT20 MCS 0, or OFDM 6 Mbps
     * as CCK power is less interesting (?).
     */
    i = rate6mb;            /* legacy */
    if (IS_CHAN_HT40(chan))
    {
        i = rateHt40_0;     /* ht40 */
    }
    else if (IS_CHAN_HT20(chan))
    {
        i = rateHt20_0;     /* ht20 */
    }
    AH_PRIVATE(ah)->ah_max_power_level = ratesArray[i];

    switch (owl_get_ntxchains(ahp->ah_txchainmask))
    {
        case 1:
            break;
        case 2:
            AH_PRIVATE(ah)->ah_max_power_level += INCREASE_MAXPOW_BY_TWO_CHAIN;
            break;
        case 3:
            AH_PRIVATE(ah)->ah_max_power_level += INCREASE_MAXPOW_BY_THREE_CHAIN;
            break;
        default:
            HALASSERT(0); /* Unsupported number of chains */
    }

    /*
     * Adjust ratesArray to be w.r.t. the base value used by the WLAN chip.
     * For pre-merlin, no adjustment is needed, since the chip's reference
     * value is 0 dBm.  For merlin and beyond, an adjustment is needed,
     * since the chip's reference value is -5 dBm.
     */
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        for (i = 0; i < Ar5416RateSize; i++) {
            int8_t pwrTableOffset =
              (int8_t)ar5416EepromGet(ahp, EEP_PWR_TABLE_OFFSET);
            ratesArray[i] -= pwrTableOffset * 2;
        }
    }

#if ATH_TEST_LOW_RATE_POWER_BOOST
    /*
     * Only for testing purposes, allow a power boost for certain low-rate
     * modulation / coding schemes.
     * This should not be enabled for commercial operation - to safely
     * add transmit power, the boosted power would have to be compared
     * against the regulatory and HW capability limits stored in the
     * EEPROM tables.
     */
    extra_txpow = AH_PRIVATE(ah)->ah_extra_txpow;
#endif

    /* Write the OFDM power per rate set */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE1,
        POW_SM(ratesArray[rate18mb] + extra_txpow, 24)
      | POW_SM(ratesArray[rate12mb] + extra_txpow, 16)
      | POW_SM(ratesArray[rate9mb]  + extra_txpow, 8)
      | POW_SM(ratesArray[rate6mb]  + extra_txpow, 0)
    );
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE2,
        POW_SM(ratesArray[rate54mb], 24)
      | POW_SM(ratesArray[rate48mb], 16)
      | POW_SM(ratesArray[rate36mb], 8)
      | POW_SM(ratesArray[rate24mb], 0)
    );

    if (IS_CHAN_2GHZ(chan)) {
        if(AR_SREV_MERLIN_20(ah) && ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
            /*
             * There is a 1 dB Tx power delta between OFDM and CCK
             * with open-loop Tx power control
             */
            cck_ofdm_delta = 2; // 1dB

            /* Write the CCK power per rate set */
            OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE3,
                POW_SM(ratesArray[rate2s] - cck_ofdm_delta, 24)
              | POW_SM(ratesArray[rate2l] - cck_ofdm_delta, 16)
              | POW_SM(ratesArray[rateXr], 8) /* XR target power */
              | POW_SM(ratesArray[rate1l] - cck_ofdm_delta, 0)
            );
            OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE4,
                POW_SM(ratesArray[rate11s]  - cck_ofdm_delta, 24)
              | POW_SM(ratesArray[rate11l]  - cck_ofdm_delta, 16)
              | POW_SM(ratesArray[rate5_5s] - cck_ofdm_delta, 8)
              | POW_SM(ratesArray[rate5_5l] - cck_ofdm_delta, 0)
            );
        } else {
            /* Write the CCK power per rate set */
            OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE3,
                POW_SM(ratesArray[rate2s] + extra_txpow, 24)
              | POW_SM(ratesArray[rate2l] + extra_txpow, 16)
                /* XR target power */
              | POW_SM(ratesArray[rateXr] + extra_txpow, 8)
              | POW_SM(ratesArray[rate1l] + extra_txpow, 0)
            );
            OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE4,
                POW_SM(ratesArray[rate11s]  + extra_txpow, 24)
              | POW_SM(ratesArray[rate11l]  + extra_txpow, 16)
              | POW_SM(ratesArray[rate5_5s] + extra_txpow, 8)
              | POW_SM(ratesArray[rate5_5l] + extra_txpow, 0)
            );
        }
    }

    /* Write the HT20 power per rate set */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE5,
        POW_SM(ratesArray[rateHt20_3], 24)
      | POW_SM(ratesArray[rateHt20_2] + extra_txpow, 16)
      | POW_SM(ratesArray[rateHt20_1] + extra_txpow, 8)
      | POW_SM(ratesArray[rateHt20_0] + extra_txpow, 0)
    );
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE6,
        POW_SM(ratesArray[rateHt20_7], 24)
      | POW_SM(ratesArray[rateHt20_6], 16)
      | POW_SM(ratesArray[rateHt20_5], 8)
      | POW_SM(ratesArray[rateHt20_4], 0)
    );

    if (IS_CHAN_HT40(chan)) {
        /* Write the HT40 power per rate set */
        /* Correct the difference between HT40 and HT20/Legacy */
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE7,
            POW_SM(ratesArray[rateHt40_3] + ht40PowerIncForPdadc, 24)
          | POW_SM(ratesArray[rateHt40_2] + ht40PowerIncForPdadc +
                   extra_txpow,  16)
          | POW_SM(ratesArray[rateHt40_1] + ht40PowerIncForPdadc +
                   extra_txpow,  8)
          | POW_SM(ratesArray[rateHt40_0] + ht40PowerIncForPdadc +
                   extra_txpow,   0)
        );
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE8,
            POW_SM(ratesArray[rateHt40_7] + ht40PowerIncForPdadc, 24)
          | POW_SM(ratesArray[rateHt40_6] + ht40PowerIncForPdadc, 16)
          | POW_SM(ratesArray[rateHt40_5] + ht40PowerIncForPdadc, 8)
          | POW_SM(ratesArray[rateHt40_4] + ht40PowerIncForPdadc, 0)
        );

        if(AR_SREV_MERLIN_20(ah) && ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
            /* Write the Dup/Ext 40 power per rate set */
            OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE9,
                POW_SM(ratesArray[rateExtOfdm], 24)
              | POW_SM(ratesArray[rateExtCck] - cck_ofdm_delta,  16)
              | POW_SM(ratesArray[rateDupOfdm],  8)
              | POW_SM(ratesArray[rateDupCck] - cck_ofdm_delta,   0)
            );
        } else {
            /* Write the Dup/Ext 40 power per rate set */
            OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE9,
                POW_SM(ratesArray[rateExtOfdm], 24)
              | POW_SM(ratesArray[rateExtCck],  16)
              | POW_SM(ratesArray[rateDupOfdm], 8)
              | POW_SM(ratesArray[rateDupCck],  0)
            );
        }
    }

    /* Write the Power subtraction for dynamic chain changing, for per-packet powertx */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_SUB,
        POW_SM(pModal->pwrDecreaseFor3Chain, 6)
      | POW_SM(pModal->pwrDecreaseFor2Chain, 0)
    );

#ifdef EEPROM_DUMP
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "2xMaxPowerLevel: %d (%s)\n",
        AH_PRIVATE(ah)->ah_max_power_level,
        (i == rateHt40_0) ? "HT40" : (i == rateHt20_0) ? "HT20" : "LEG");
#endif

    if (AH_PRIVATE(ah)->ah_config.ath_hal_desc_tpc) {
        /* Software Power Per Rate values in ath_hal_5416.txPowers[][]
           are initialized here. The software power per rate is configured
           with a power value for each possible number of transmit chains.
           The reason for doing this is that the transmit power used for
           different number of chains is different depending on whether the
           power has been limited by the target power, the regulatory domain
           or the CTL limits.
           In particular the CTL limits for different number of chains are
           specified in the eeprom and must be taken into account when doing
           dynamic transmit power and transmit chainmask selection.
         */
        u_int8_t chainmasks[AR5416_MAX_CHAINS] =
            {AR5416_1_CHAINMASK, AR5416_2LOHI_CHAINMASK, AR5416_3_CHAINMASK};
        for (i = 0; i < AR5416_MAX_CHAINS; i++) {
            ar5416EepromDefSetPowerPerRateTable(ah, pEepData, chan,
                &ratesArray[0], cfgCtl, twiceAntennaReduction,
                twiceMaxRegulatoryPower, powerLimit, chainmasks[i]);
            if (AR_SREV_MERLIN_10_OR_LATER(ah)) { /* adjust to -5 dBm ref */
                int j;
                for (j = 0; j < Ar5416RateSize; j++) {
                    ratesArray[j] -= AR5416_PWR_TABLE_OFFSET_DB*2;
                }
            }
            ar5416InitRateTxPower(ah, mode, chan, ht40PowerIncForPdadc,
                ratesArray, chainmasks[i]);
        }

        ar5416DumpRateTxPower(ah, mode);
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE_MAX,
                     AR_PHY_POWER_TX_RATE_MAX_TPC_ENABLE);
    }

    return HAL_OK;
}

void
ar5416EepromDefSetAddac(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
    MODAL_EEPDEF_HEADER *pModal;
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_def_t *eep = &ahp->ah_eeprom.map.def;
    u_int8_t biaslevel;

    if (AH_PRIVATE(ah)->ah_mac_version != AR_SREV_VERSION_SOWL)
        return;

    HALASSERT(owl_get_eepdef_ver(ahp) == AR5416_EEP_VER);

    /* Xpa bias levels in eeprom are valid from rev 14.7 */
    if (owl_get_eepdef_rev(ahp) < AR5416_EEP_MINOR_VER_7)
        return;

    if (ahp->ah_emu_eeprom) return;

    pModal = &(eep->modalHeader[IS_CHAN_2GHZ(chan)]);

    if (pModal->xpaBiasLvl != 0xff) {
        biaslevel = pModal->xpaBiasLvl;
    } else {
        /* Use freqeuncy specific xpa bias level */
        u_int16_t resetFreqBin, freqBin, freqCount=0;
        CHAN_CENTERS centers;

        ar5416GetChannelCenters(ah, chan, &centers);

        resetFreqBin = FREQ2FBIN(centers.synth_center, IS_CHAN_2GHZ(chan));
        freqBin = pModal->xpaBiasLvlFreq[0] & 0xff;
        biaslevel = (u_int8_t)(pModal->xpaBiasLvlFreq[0] >> 14);

        freqCount++;

        while (freqCount < 3) {
            if (pModal->xpaBiasLvlFreq[freqCount] == 0x0)
                break;

            freqBin = pModal->xpaBiasLvlFreq[freqCount] & 0xff;
            if (resetFreqBin >= freqBin) {
                biaslevel = (u_int8_t)(pModal->xpaBiasLvlFreq[freqCount] >> 14);
            } else {
                break;
            }
            freqCount++;
        }
    }

    /* Apply bias level to the ADDAC values in the INI array */
    if (IS_CHAN_2GHZ(chan)) {
        INI_RA(&ahp->ah_iniAddac, 7, 1) =
            (INI_RA(&ahp->ah_iniAddac, 7, 1) & (~0x18)) | biaslevel << 3;
    } else {
        INI_RA(&ahp->ah_iniAddac, 6, 1) =
            (INI_RA(&ahp->ah_iniAddac, 6, 1) & (~0xc0)) | biaslevel << 6;
    }
}

HAL_STATUS
ar5416CheckEepromDef(struct ath_hal *ah)
{
    u_int32_t sum = 0, el;
    u_int16_t *eepdata;
    int i;
    struct ath_hal_5416 *ahp = AH5416(ah);
    HAL_BOOL need_swap = AH_FALSE;
    ar5416_eeprom_def_t *eep = (ar5416_eeprom_def_t *)&ahp->ah_eeprom.map.def;
    u_int16_t magic, magic2;
    int addr;

    /*
    ** We need to check the EEPROM data regardless of if it's in flash or
    ** in EEPROM.
    */

    if (!ahp->ah_priv.priv.ah_eeprom_read(
                               ah, AR5416_EEPROM_MAGIC_OFFSET, &magic)) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Reading Magic # failed\n", __func__);
        return AH_FALSE;
    }
    
    HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Read Magic = 0x%04X\n", __func__, magic);

    if (!ar5416EepDataInFlash(ah))
    {

        if (magic != AR5416_EEPROM_MAGIC)
        {
            magic2 = SWAP16(magic);

            if (magic2 == AR5416_EEPROM_MAGIC)
            {
                need_swap = AH_TRUE;
                eepdata = (u_int16_t *)(&ahp->ah_eeprom);
            
                for (addr=0; 
                     addr<sizeof(ar5416_eeprom_t)/sizeof(u_int16_t);
                     addr++)
                {
                    
                    u_int16_t temp;
                    temp = SWAP16(*eepdata);
                    *eepdata = temp;
                    eepdata++;

                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "0x%04X  ", *eepdata);
                    if (((addr+1)%6) == 0)
                        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "\n");
                }
            }
            else
            {
                HDPRINTF(ah, HAL_DBG_EEPROM, "Invalid EEPROM Magic. endianness missmatch.\n");
                return HAL_EEBADSUM;
            }
        }
    }
    else
    {
        HDPRINTF(ah, HAL_DBG_EEPROM, "EEPROM being read from flash @0x%pK\n", 
                 (void *)AH_PRIVATE(ah)->ah_st);
    }

    HDPRINTF(ah, HAL_DBG_EEPROM, "need_swap = %s.\n", need_swap?"True":"False");

    if (need_swap) {
        el = SWAP16(ahp->ah_eeprom.map.def.baseEepHeader.length);
    } else {
        el = ahp->ah_eeprom.map.def.baseEepHeader.length;
    }

    eepdata = (u_int16_t *)(&ahp->ah_eeprom.map.def);
    for (i = 0; i < AH_MIN(el, sizeof(ar5416_eeprom_def_t))/sizeof(u_int16_t); i++)
        sum ^= *eepdata++;

    if (need_swap) {
        /*
        *  preddy: EEPROM endianness does not match. So change it
        *  8bit values in eeprom data structure does not need to be swapped
        *  Only >8bits (16 & 32) values need to be swapped
        *  If a new 16 or 32 bit field is added to the EEPROM contents,
        *  please make sure to swap the field here
        */
        u_int32_t integer,j;
        u_int16_t word;

        HDPRINTF(ah, HAL_DBG_EEPROM, "EEPROM Endianness is not native.. Changing \n");

        /* convert Base Eep header */
        word = SWAP16(eep->baseEepHeader.length);
        eep->baseEepHeader.length = word;

        word = SWAP16(eep->baseEepHeader.checksum);
        eep->baseEepHeader.checksum = word;

        word = SWAP16(eep->baseEepHeader.version);
        eep->baseEepHeader.version = word;

        word = SWAP16(eep->baseEepHeader.regDmn[0]);
        eep->baseEepHeader.regDmn[0] = word;

        word = SWAP16(eep->baseEepHeader.regDmn[1]);
        eep->baseEepHeader.regDmn[1] = word;

        word = SWAP16(eep->baseEepHeader.rfSilent);
        eep->baseEepHeader.rfSilent = word;

        word = SWAP16(eep->baseEepHeader.blueToothOptions);
        eep->baseEepHeader.blueToothOptions = word;

        word = SWAP16(eep->baseEepHeader.deviceCap);
        eep->baseEepHeader.deviceCap = word;

        /* convert Modal Eep header */
        for (j = 0; j < N(eep->modalHeader); j++) {
            MODAL_EEPDEF_HEADER *pModal = &eep->modalHeader[j];
            integer = SWAP32(pModal->antCtrlCommon);
            pModal->antCtrlCommon = integer;

            for (i = 0; i < AR5416_MAX_CHAINS; i++) {
                integer = SWAP32(pModal->antCtrlChain[i]);
                pModal->antCtrlChain[i] = integer;
            }

            for (i = 0; i < AR5416_EEPROM_MODAL_SPURS; i++) {
                word = SWAP16(pModal->spurChans[i].spurChan);
                pModal->spurChans[i].spurChan = word;
            }
        }
    }

    /* Check CRC - Attach should fail on a bad checksum */
    if (sum != 0xffff || owl_get_eepdef_ver(ahp) != AR5416_EEP_VER ||
        owl_get_eepdef_rev(ahp) < AR5416_EEP_NO_BACK_VER) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "Bad EEPROM checksum 0x%x or revision 0x%04x\n",
            sum, owl_get_eepdef_ver(ahp));
        return HAL_EEBADSUM;
    }

    if (AR_SREV_MERLIN(ah) && 
        ((AH_PRIVATE(ah)->ah_config.ath_hal_xpblevel5 & 0x100) == 0x100)) {
        //for UB94, which ath_hal_xpblevel5 is set
        eep->modalHeader[0].xpaBiasLvl = AH_PRIVATE(ah)->ah_config.ath_hal_xpblevel5 & 0xff;
    }

#ifdef EEPROM_DUMP
    ar5416EepromDefDump(ah, eep);
#endif



#ifdef AH_AR5416_OVRD_TGT_PWR

    /* 14.4 EEPROM contains low target powers.      Hardcode until EEPROM > 14.4 */
    if (owl_get_eepdef_ver(ahp) == 14 && owl_get_eepdef_rev(ahp) <= 4) {
        MODAL_EEPDEF_HEADER *pModal;

#ifdef EEPROM_DUMP
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "Original Target Powers\n");
        ar5416EepDefDumpTgtPower(ah, eep);
#endif
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, 
                "Override Target Powers. EEPROM Version is %d.%d, Device Type %d\n",
                owl_get_eepdef_ver(ahp),
                owl_get_eepdef_rev(ahp),
                eep->baseEepHeader.deviceType);


        ar5416EepDefOverrideTgtPower(ah, eep);

        if (eep->baseEepHeader.deviceType == 5) {
            /* for xb72 only: improve transmit EVM for interop */
            pModal = &eep->modalHeader[1];
            pModal->txFrameToDataStart = 0x23;
            pModal->txFrameToXpaOn = 0x23;
            pModal->txFrameToPaOn = 0x23;
        }

#ifdef EEPROM_DUMP
        HDPRINTF(ah, HAL_DBG_POWER_OVERRIDE, "Modified Target Powers\n");
        ar5416EepDefDumpTgtPower(ah, eep);
#endif
    }
#endif /* AH_AR5416_OVRD_TGT_PWR */

    return HAL_OK;
}

#ifdef AH_PRIVATE_DIAG
void
ar5416FillEmuEepromDef(struct ath_hal_5416 *ahp)
{
    ar5416_eeprom_t *eep = &ahp->ah_eeprom;

    eep->map.def.baseEepHeader.version = AR5416_EEP_VER << 12;
    eep->map.def.baseEepHeader.macAddr[0] = 0x00;
    eep->map.def.baseEepHeader.macAddr[1] = 0x03;
    eep->map.def.baseEepHeader.macAddr[2] = 0x7F;
    eep->map.def.baseEepHeader.macAddr[3] = 0xBA;
    eep->map.def.baseEepHeader.macAddr[4] = 0xD0;
    eep->map.def.baseEepHeader.regDmn[0] = 0;
    eep->map.def.baseEepHeader.opCapFlags =
        AR5416_OPFLAGS_11G | AR5416_OPFLAGS_11A;
    eep->map.def.baseEepHeader.deviceCap = 0;
    eep->map.def.baseEepHeader.rfSilent = 0;
    eep->map.def.modalHeader[0].noiseFloorThreshCh[0] = -1;
    eep->map.def.modalHeader[1].noiseFloorThreshCh[0] = -1;
}
#endif /* AH_PRIVATE_DIAG */

HAL_BOOL
ar5416FillEepromDef(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_def_t *eep = &ahp->ah_eeprom.map.def;
    u_int16_t *eep_data;
    int addr, owl_eep_start_loc = 0x100; // EEPROM data always starts 0x100 words into the block

#if defined(ATH_PYTHON) && defined(AH_CAL_OFFSET)
    /*
     * Allow for the possibility of calibration data being located at an
     * address specified in the build's config.<board-type> file.
     */
    owl_eep_start_loc = AH_CAL_OFFSET;
#endif

    eep_data = (u_int16_t *)eep;

    for (addr = 0; addr < sizeof(ar5416_eeprom_t) / sizeof(u_int16_t); 
        addr++)
    {
        if (!ahp->ah_priv.priv.ah_eeprom_read(ah, addr + owl_eep_start_loc, 
            eep_data))
        {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Unable to read eeprom region \n", __func__);
            return AH_FALSE;
        }
        eep_data ++;
    }
#ifdef EEPROM_DUMP
    ar5416EepromDefDump(ah, eep);
#endif
    return AH_TRUE;
}

u_int8_t
ar5416EepromDefGetNumAntConfig(struct ath_hal_5416 *ahp, HAL_FREQ_BAND freq_band)
{
    ar5416_eeprom_def_t  *eep = &ahp->ah_eeprom.map.def;
    MODAL_EEPDEF_HEADER *pModal = &(eep->modalHeader[HAL_FREQ_BAND_2GHZ == freq_band]);
    BASE_EEPDEF_HEADER  *pBase  = &eep->baseEepHeader;
    u_int8_t         num_ant_config;

    num_ant_config = 1; /* default antenna configuration */

    if (pBase->version >= 0x0E0D)
        if (pModal->useAnt1)
            num_ant_config += 1;

    return num_ant_config;
}

HAL_STATUS
ar5416EepromDefGetAntCfg(struct ath_hal_5416 *ahp, HAL_CHANNEL_INTERNAL *chan,
                   u_int8_t index, u_int32_t *config)
{
    ar5416_eeprom_def_t  *eep = &ahp->ah_eeprom.map.def;
    MODAL_EEPDEF_HEADER *pModal = &(eep->modalHeader[IS_CHAN_2GHZ(chan)]);
    BASE_EEPDEF_HEADER  *pBase  = &eep->baseEepHeader;

    switch (index) {
        case 0:
            *config = pModal->antCtrlCommon & 0xFFFF;
            return HAL_OK;
        case 1:
            if (pBase->version >= 0x0E0D) {
                if (pModal->useAnt1) {
                    *config = ((pModal->antCtrlCommon & 0xFFFF0000) >> 16);
                    return HAL_OK;
                }
            }
            break;

        default:
            break;
    }

    return HAL_EINVAL;
}

/* PDADC vs Tx power table on open-loop power control mode */
static void
ar5416OpenLoopPowerControlGetPdadcs(struct ath_hal *ah, u_int32_t initTxGain, int32_t txPower, u_int8_t* pPDADCValues)
{
    u_int32_t i;
    u_int32_t offset;

    // To enable open-loop power control
    OS_REG_RMW_FIELD(ah, AR_PHY_TX_PWRCTRL6_0,  AR_PHY_TX_PWRCTRL_ERR_EST_MODE, 3);
    OS_REG_RMW_FIELD(ah, AR_PHY_TX_PWRCTRL6_1,  AR_PHY_TX_PWRCTRL_ERR_EST_MODE, 3);


    // Set init Tx gain index for first frame
    OS_REG_RMW_FIELD(ah, AR_PHY_TX_PWRCTRL7,  AR_PHY_TX_PWRCTRL_INIT_TX_GAIN, initTxGain);

    offset = txPower;
    for(i = 0; i < AR5416_NUM_PDADC_VALUES; i++) {
      if(i < offset) {
         pPDADCValues[i] = 0x0;
      } else {
        pPDADCValues[i] = 0xFF;
      }
    }
}

/*
 * Fully-calibrated CTLs specify the power for each possible number of
 * chains supported by the WLAN chip (1 for kite, 1 and 2 for merlin,
 * 1, 2, and 3 for owl).  However, to save money by reducing regulatory
 * testing, CTLs are often only calibrated for the number of chains the
 * device has (1 for kite, 2 for merlin, 3 for owl), since in general
 * the device will use its full complement of chains.
 * For (rare) cases where the device uses fewer chains than it has,
 * this function provides a method to increase the per-chain power
 * so that the total power remains constant.  However, this really
 * should be done only if the power is separately calibrated for each
 * number of chains.  This function is appropriate for demonstrating
 * that the per-chain power can be increased to keep the total power
 * fixed regardless of the number of chains in use, but is not
 * appropriate for commercial use, since it may end up increasing the
 * power more than is appropriate for a particular board, and exceeding
 * regulatory power requirements.
 */
static inline HAL_STATUS
ar5416EepromDefFixCTLs(struct ath_hal *ah)
{
#ifdef AH_INCREASE_CHAIN_POWER
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_def_t  *eep = &ahp->ah_eeprom.map.def;
    CAL_CTL_DATA_EEPDEF *ctl_data;
    CAL_CTL_EDGES *ctl_edge_chain0;
    int i, j, ctl_val;
    int ref_num_chains;

    ref_num_chains = owl_get_ntxchains(eep->baseEepHeader.txMask);

    for (i = 0; ((i < AR5416_EEPDEF_NUM_CTLS) && (eep->ctlIndex[i])); i++) {
        ctl_data = &(eep->ctlData[i]);
        ctl_edge_chain0 = ctl_data->ctlEdges[0];

        /* Assumes if chain0 says unused channel, other chains do too. */
        for (j = 0; ((j < AR5416_EEPDEF_NUM_BAND_EDGES) &&
                     (ctl_edge_chain0->bChannel != AR5416_BCHAN_UNUSED)); j++) {
            if ((ctl_data->ctlEdges[0][j].tPower ==
                 ctl_data->ctlEdges[1][j].tPower) &&
                (ctl_data->ctlEdges[0][j].tPower ==
                 ctl_data->ctlEdges[2][j].tPower)) {
                if ( ref_num_chains == 3 )  {
                    /* make sure the result is not negative */
                    ctl_val = AH_MAX(0, ctl_data->ctlEdges[2][j].tPower
                                        + PWRINCR_3_TO_1_CHAIN);
                    ctl_data->ctlEdges[0][j].tPower = ctl_val;
    
                    ctl_val = AH_MAX(0, ctl_data->ctlEdges[2][j].tPower
                                        + PWRINCR_3_TO_2_CHAIN);
                    ctl_data->ctlEdges[1][j].tPower = ctl_val;
                } else if ( ref_num_chains == 2 )  {
                    /* make sure the result is not negative */
                    ctl_val = AH_MAX(0, ctl_data->ctlEdges[2][j].tPower
                                        + PWRINCR_2_TO_1_CHAIN);
                    ctl_data->ctlEdges[0][j].tPower = ctl_val;
                }
            }
        }
    }
#endif
    return HAL_OK;
}



/*----------All Debug functions ----------------*/
#ifdef AH_AR5416_OVRD_TGT_PWR
/*
 * Force override of Target Power values for AR5416, EEPROM versions <= 14.4
 */
static void
ar5416EepDefOverrideTgtPower(struct ath_hal *ah, ar5416_eeprom_def_t *eep)
{

    int ii;
    CAL_TARGET_POWER_LEG    *force_5G;
    CAL_TARGET_POWER_HT     *force_5GHT20;
    CAL_TARGET_POWER_HT     *force_5GHT40;
    CAL_TARGET_POWER_LEG    *force_Cck;
    CAL_TARGET_POWER_LEG    *force_2G;
    CAL_TARGET_POWER_HT     *force_2GHT20;
    CAL_TARGET_POWER_HT     *force_2GHT40;

#define FB5(_f) ((_f - 4800)/5)
#define FB2(_f) (_f - 2300)
#define DH(_d)  (_d*2)

/* XB72 overrides */
static  CAL_TARGET_POWER_LEG    xbForceTargetPower5G[AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS] = { \
    {FB5(5180), {DH(17), DH(16), DH(15), DH(14)}},
    {FB5(5320), {DH(17), DH(16), DH(15), DH(14)}},
    {FB5(5700), {DH(17), DH(16), DH(15), DH(14)}},
    {FB5(5825), {DH(17), DH(16), DH(15), DH(14)}},
    {0xff,      {0, 0, 0, 0}},
    {0xff,      {0, 0, 0, 0}},
    {0xff,      {0, 0, 0, 0}},
    {0xff,      {0, 0, 0, 0}}};

static  CAL_TARGET_POWER_HT xbForceTargetPower5GHT20[AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS] = { \
    {FB5(5180), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {FB5(5240), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {FB5(5320), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {FB5(5500), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {FB5(5700), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {FB5(5745), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {FB5(5825), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {0xff,      {0, 0, 0, 0, 0, 0, 0, 0}}};

static  CAL_TARGET_POWER_HT xbForceTargetPower5GHT40[AR5416_EEPDEF_NUM_5G_40_TARGET_POWERS] = { \
    {FB5(5180), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {FB5(5240), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {FB5(5320), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {FB5(5500), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {FB5(5700), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {FB5(5745), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {FB5(5825), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(16), DH(13), DH(10)}},
    {0xff,      {0, 0, 0, 0, 0, 0, 0, 0}}};

static  CAL_TARGET_POWER_LEG    xbForceTargetPowerCck[AR5416_EEPDEF_NUM_2G_CCK_TARGET_POWERS] = { \
    {FB2(2412), {DH(19), DH(19), DH(19), DH(19)}},
    {FB2(2484), {DH(19), DH(19), DH(19), DH(19)}},
    {0xff,      {0, 0, 0, 0}}};

static  CAL_TARGET_POWER_LEG    xbForceTargetPower2G[AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS] = { \
    {FB2(2412), {DH(18), DH(17), DH(16), DH(15)}},
    {FB2(2437), {DH(18), DH(17), DH(16), DH(15)}},
    {FB2(2472), {DH(18), DH(17), DH(16), DH(15)}},
    {0xff,      {0, 0, 0, 0}}};

static  CAL_TARGET_POWER_HT xbForceTargetPower2GHT20[AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS] = { \
    {FB2(2412), {DH(18), DH(18), DH(18), DH(18), DH(17), DH(17), DH(13), DH(12)}},
    {FB2(2437), {DH(18), DH(18), DH(18), DH(18), DH(17), DH(17), DH(13), DH(12)}},
    {FB2(2472), {DH(18), DH(18), DH(18), DH(18), DH(17), DH(17), DH(13), DH(12)}},
    {0xff,      {0, 0, 0, 0, 0, 0, 0, 0}}};

static  CAL_TARGET_POWER_HT xbForceTargetPower2GHT40[AR5416_EEPDEF_NUM_2G_40_TARGET_POWERS] = { \
    {FB2(2412), {DH(18), DH(18), DH(18), DH(18), DH(17), DH(17), DH(13), DH(12)}},
    {FB2(2437), {DH(18), DH(18), DH(18), DH(18), DH(17), DH(17), DH(13), DH(12)}},
    {FB2(2472), {DH(18), DH(18), DH(18), DH(18), DH(17), DH(17), DH(13), DH(12)}},
    {0xff,      {0, 0, 0, 0, 0, 0, 0, 0}}};

/* MB72 Overrides */
static  CAL_TARGET_POWER_LEG    mbForceTargetPower5G[AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS] = { \
    {FB5(5180), {DH(17), DH(16), DH(15), DH(13)}},
    {FB5(5320), {DH(17), DH(16), DH(15), DH(13)}},
    {FB5(5700), {DH(17), DH(16), DH(15), DH(13)}},
    {FB5(5825), {DH(17), DH(16), DH(15), DH(13)}},
    {0xff,      {0, 0, 0, 0}},
    {0xff,      {0, 0, 0, 0}},
    {0xff,      {0, 0, 0, 0}},
    {0xff,      {0, 0, 0, 0}}};

static  CAL_TARGET_POWER_HT mbForceTargetPower5GHT20[AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS] = { \
    {FB5(5180), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {FB5(5240), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {FB5(5320), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {FB5(5500), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {FB5(5700), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {FB5(5745), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {FB5(5825), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {0xff,      {0, 0, 0, 0, 0, 0, 0, 0}}};

static  CAL_TARGET_POWER_HT mbForceTargetPower5GHT40[AR5416_EEPDEF_NUM_5G_40_TARGET_POWERS] = { \
    {FB5(5180), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {FB5(5240), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {FB5(5320), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {FB5(5500), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {FB5(5700), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {FB5(5745), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {FB5(5825), {DH(17), DH(17), DH(17), DH(17), DH(17), DH(17), DH(12), DH(10)}},
    {0xff,      {0, 0, 0, 0, 0, 0, 0, 0}}};


static  CAL_TARGET_POWER_LEG    mbForceTargetPowerCck[AR5416_EEPDEF_NUM_2G_CCK_TARGET_POWERS] = { \
    {FB2(2412), {DH(19), DH(19), DH(19), DH(19)}},
    {FB2(2484), {DH(19), DH(19), DH(19), DH(19)}},
    {0xff,      {0, 0, 0, 0}}};

static  CAL_TARGET_POWER_LEG    mbForceTargetPower2G[AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS] = { \
    {FB2(2412), {DH(17), DH(17), DH(16), DH(13)}},
    {FB2(2437), {DH(17), DH(17), DH(16), DH(13)}},
    {FB2(2472), {DH(17), DH(17), DH(16), DH(13)}},
    {0xff,      {0, 0, 0, 0}}};

static  CAL_TARGET_POWER_HT mbForceTargetPower2GHT20[AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS] = { \
    {FB2(2412), {DH(19), DH(19), DH(19), DH(19), DH(17), DH(15), DH(13), DH(11)}},
    {FB2(2437), {DH(19), DH(19), DH(19), DH(19), DH(17), DH(15), DH(13), DH(11)}},
    {FB2(2472), {DH(19), DH(19), DH(19), DH(19), DH(17), DH(15), DH(13), DH(11)}},
    {0xff,      {0, 0, 0, 0, 0, 0, 0, 0}}};

static  CAL_TARGET_POWER_HT mbForceTargetPower2GHT40[AR5416_EEPDEF_NUM_2G_40_TARGET_POWERS] = { \
    {FB2(2412), {DH(19), DH(19), DH(19), DH(19), DH(19), DH(17), DH(13), DH(11)}},
    {FB2(2437), {DH(19), DH(19), DH(19), DH(19), DH(19), DH(17), DH(13), DH(11)}},
    {FB2(2472), {DH(19), DH(19), DH(19), DH(19), DH(19), DH(17), DH(13), DH(11)}},
    {0xff,      {0, 0, 0, 0, 0, 0, 0, 0}}};

#undef FB5
#undef FB2
#undef DH

    /* Device specific pointers */
    if (eep->baseEepHeader.deviceType == 3) {
        /* miniPci */
        force_5G     = &mbForceTargetPower5G[0];
        force_5GHT20 = mbForceTargetPower5GHT20;
        force_5GHT40 = mbForceTargetPower5GHT40;
        force_Cck    = mbForceTargetPowerCck;
        force_2G     = mbForceTargetPower2G;
        force_2GHT20 = mbForceTargetPower2GHT20;
        force_2GHT40 = mbForceTargetPower2GHT40;
    }
    else if (eep->baseEepHeader.deviceType == 5) {
        /* PciExpress */
        force_5G     = xbForceTargetPower5G;
        force_5GHT20 = xbForceTargetPower5GHT20;
        force_5GHT40 = xbForceTargetPower5GHT40;
        force_Cck    = xbForceTargetPowerCck;
        force_2G     = xbForceTargetPower2G;
        force_2GHT20 = xbForceTargetPower2GHT20;
        force_2GHT40 = xbForceTargetPower2GHT40;
    }
    else {
        return;
    }

    /* update ram copy of eeprom */
    ii = 0; /* happy compiler */
#ifdef AH_AR5416_OVRD_TGT_PWR_5G
    HDPRINTF(ah, HAL_DBG_POWER_OVERRIDE, "forced AH_AR5416_OVRD_TGT_PWR_5G\n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS; ii++) {
        eep->calTargetPower5G[ii] = force_5G[ii];
    }
#endif

#ifdef AH_AR5416_OVRD_TGT_PWR_5GHT20
    HDPRINTF(ah, HAL_DBG_POWER_OVERRIDE, "forced AH_AR5416_OVRD_TGT_PWR_5GHT20\n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS; ii++) {
        eep->calTargetPower5GHT20[ii] = force_5GHT20[ii];
    }
#endif

#ifdef AH_AR5416_OVRD_TGT_PWR_5GHT40
    HDPRINTF(ah, HAL_DBG_POWER_OVERRIDE, "forced AH_AR5416_OVRD_TGT_PWR_5GHT40\n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_5G_40_TARGET_POWERS; ii++) {
        eep->calTargetPower5GHT40[ii] = force_5GHT40[ii];
    }
#endif

#ifdef AH_AR5416_OVRD_TGT_PWR_CCK
    HDPRINTF(ah, HAL_DBG_POWER_OVERRIDE, "forced AH_AR5416_OVRD_TGT_PWR_CCK\n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_2G_CCK_TARGET_POWERS; ii++) {
        eep->calTargetPowerCck[ii] = force_Cck[ii];
    }
#endif

#ifdef AH_AR5416_OVRD_TGT_PWR_2G
    HDPRINTF(ah, HAL_DBG_POWER_OVERRIDE, "forced AH_AR5416_OVRD_TGT_PWR_2G\n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS; ii++) {
        eep->calTargetPower2G[ii] = force_2G[ii];
    }
#endif

#ifdef AH_AR5416_OVRD_TGT_PWR_2GHT20
    HDPRINTF(ah, HAL_DBG_POWER_OVERRIDE, "forced AH_AR5416_OVRD_TGT_PWR_2GHT20\n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS; ii++) {
        eep->calTargetPower2GHT20[ii] = force_2GHT20[ii];
    }
#endif

#ifdef AH_AR5416_OVRD_TGT_PWR_2GHT40
    HDPRINTF(ah, HAL_DBG_POWER_OVERRIDE, "forced AH_AR5416_OVRD_TGT_PWR_2GHT40\n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_2G_40_TARGET_POWERS; ii++) {
        eep->calTargetPower2GHT40[ii] = force_2GHT40[ii];
    }
#endif
}
#endif /* AH_AR5416_OVRD_TGT_PWR */


#ifdef EEPROM_DUMP
static void
ar5416EepDefPrintPowerPerRate(struct ath_hal *ah, int16_t *pRatesPower,
                              u_int16_t red, u_int16_t reg, u_int16_t limit)
{
    const char *rateString[] = {
        " 6M OFDM", " 9M OFDM", "12M OFDM", "18M OFDM",
        "24M OFDM", "36M OFDM", "48M OFDM", "54M OFDM",
        "1L   CCK", "2L   CCK", "2S   CCK", "5.5L CCK",
        "5.5S CCK", "11L  CCK", "11S  CCK", "XR      ",
        "HT20mc 0", "HT20mc 1", "HT20mc 2", "HT20mc 3",
        "HT20mc 4", "HT20mc 5", "HT20mc 6", "HT20mc 7",
        "HT40mc 0", "HT40mc 1", "HT40mc 2", "HT40mc 3",
        "HT40mc 4", "HT40mc 5", "HT40mc 6", "HT40mc 7",
        "Dup CCK ", "Dup OFDM", "Ext CCK ", "Ext OFDM",
    };
    int i;

    for (i = 0; i < Ar5416RateSize; i+=4) {
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE,
            " %s %2d.%1d dBm | %s %2d.%1d dBm |"
            " %s %2d.%1d dBm | %s %2d.%1d dBm\n",
            rateString[i], pRatesPower[i] / 2, (pRatesPower[i] % 2) * 5,
            rateString[i+1], pRatesPower[i+1]/2, (pRatesPower[i+1]%2) * 5,
            rateString[i+2], pRatesPower[i+2]/2, (pRatesPower[i+2]%2) * 5,
            rateString[i+3], pRatesPower[i+3]/2, (pRatesPower[i+3]%2) * 5);
    }
    ath_hal_printf(ah,
        "2xAntennaReduction: %d, 2xMaxRegulatory: %d, 2xPowerLimit: %d\n",
        red, reg, limit);
}

/**************************************************************
 * ar5416EepromDefDump
 *
 * Produce a formatted dump of the EEPROM structure
 */
static void 
ar5416EepromDefDump(struct ath_hal *ah, ar5416_eeprom_def_t *ar5416Eep)
{
    u_int16_t           i, j, k, c, r;
    struct ath_hal_5416     *ahp = AH5416(ah);
    BASE_EEPDEF_HEADER     *pBase = &(ar5416Eep->baseEepHeader);
    MODAL_EEPDEF_HEADER        *pModal =AH_NULL;
    CAL_TARGET_POWER_HT     *pPowerInfo = AH_NULL;
    CAL_TARGET_POWER_LEG    *pPowerInfoLeg = AH_NULL;
    HAL_BOOL            legacyPowers = AH_FALSE;
    CAL_DATA_PER_FREQ_EEPDEF       *pDataPerChannel;
    HAL_BOOL            is2Ghz = 0;
    u_int8_t            noMoreSpurs;
    u_int8_t            *pChannel;
    u_int16_t           *pSpurData;
    u_int16_t           channelCount, channelRowCnt, vpdCount, rowEndsAtChan;
    u_int16_t           xpdGainMapping[] = {0, 1, 2, 4};
    u_int16_t           xpdGainValues[AR5416_EEPDEF_NUM_PD_GAINS], numXpdGain = 0, xpdMask;
    u_int16_t           numPowers = 0, numRates = 0, ratePrint = 0, numChannels, tempFreq;
    u_int16_t           numTxChains = (u_int16_t)( ((pBase->txMask >> 2) & 1) +
                    ((pBase->txMask >> 1) & 1) + (pBase->txMask & 1) );
    u_int16_t           temp;
    u_int8_t            targetPower;  
    int8_t              tempPower;
    u_int16_t           channelValue;

    const static char *sRatePrint[3][8] = {
      {"     6-24     ", "      36      ", "      48      ", "      54      ", "", "", "", ""},
      {"       1      ", "       2      ", "     5.5      ", "      11      ", "", "", "", ""},
      {"   HT MCS0    ", "   HT MCS1    ", "   HT MCS2    ", "   HT MCS3    ",
       "   HT MCS4    ", "   HT MCS5    ", "   HT MCS6    ", "   HT MCS7    "},
    };

    const static char *sTargetPowerMode[7] = {
      "5G OFDM ", "5G HT20 ", "5G HT40 ", "2G CCK  ", "2G OFDM ", "2G HT20 ", "2G HT40 ",
    };

    const static char *sDeviceType[] = {
      "UNKNOWN [0] ",
      "Cardbus     ",
      "PCI         ",
      "MiniPCI     ",
      "Access Point",
      "PCIExpress  ",
      "UNKNOWN [6] ",
      "UNKNOWN [7] ",
    };

    const static char *sCtlType[] = {
        "[ 11A base mode ]",
        "[ 11B base mode ]",
        "[ 11G base mode ]",
        "[ INVALID       ]",
        "[ INVALID       ]",
        "[ 2G HT20 mode  ]",
        "[ 5G HT20 mode  ]",
        "[ 2G HT40 mode  ]",
        "[ 5G HT40 mode  ]",
    };

    if (!ar5416Eep) {
        return;
    }

    /* Print Header info */
    pBase = &(ar5416Eep->baseEepHeader);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "\n");
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " =======================Header Information======================\n");
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Major Version           %2d  |  Minor Version           %2d  |\n",
             pBase->version >> 12, pBase->version & 0xFFF);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |-------------------------------------------------------------|\n");
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Checksum           0x%04X   |  Length             0x%04X   |\n",
             pBase->checksum, pBase->length);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  RegDomain 1        0x%04X   |  RegDomain 2        0x%04X   |\n",
             pBase->regDmn[0], pBase->regDmn[1]);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  MacAddress: 0x%02X:%02X:%02X:%02X:%02X:%02X                            |\n",
             pBase->macAddr[0], pBase->macAddr[1], pBase->macAddr[2],
             pBase->macAddr[3], pBase->macAddr[4], pBase->macAddr[5]);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  TX Mask            0x%04X   |  RX Mask            0x%04X   |\n",
             pBase->txMask, pBase->rxMask);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  OpFlags: 5GHz %d, 2GHz %d, Disable 5HT40 %d, Disable 2HT40 %d  |\n",
             (pBase->opCapFlags & AR5416_OPFLAGS_11A) || 0, (pBase->opCapFlags & AR5416_OPFLAGS_11G) || 0,
         (pBase->opCapFlags & AR5416_OPFLAGS_N_5G_HT40) || 0, (pBase->opCapFlags & AR5416_OPFLAGS_N_2G_HT40) || 0);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  OpFlags: Disable 5HT20 %d, Disable 2HT20 %d                  |\n",
             (pBase->opCapFlags & AR5416_OPFLAGS_N_5G_HT20) || 0, (pBase->opCapFlags & AR5416_OPFLAGS_N_2G_HT20) || 0);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Misc: Big Endian %d                                         |\n",      
             (pBase->eepMisc & AR5416_EEPMISC_BIG_ENDIAN) || 0);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Cal Bin Maj Ver %3d Cal Bin Min Ver %3d Cal Bin Build %3d  |\n",
             (pBase->binBuildNumber >> 24) & 0xFF,
             (pBase->binBuildNumber >> 16) & 0xFF,
             (pBase->binBuildNumber >> 8) & 0xFF);
    if (IS_EEP_MINOR_V3(ahp)) {
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Device Type: %s                                  |\n",
                 sDeviceType[(pBase->deviceType & 0x7)]);
    }

    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Customer Data in hex                                       |\n");
    for (i = 0; i < 64; i++) {
        if ((i % 16) == 0) {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |= ");
        }
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "%02X ", ar5416Eep->custData[i]);
        if ((i % 16) == 15) {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "          =|\n");
        }
    }

    /* Print Modal Header info */
    for (i = 0; i < 2; i++) {
        if ((i == 0) && !(pBase->opCapFlags & AR5416_OPFLAGS_11A)) {
            continue;
        }
        if ((i == 1) && !(pBase->opCapFlags & AR5416_OPFLAGS_11G)) {
            continue;
        }
        pModal = &(ar5416Eep->modalHeader[i]);
        if (i == 0) {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |======================5GHz Modal Header======================|\n");
        } else {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |======================2GHz Modal Header======================|\n");
        }
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Ant Chain 0     0x%08X  |  Ant Chain 1     0x%08X  |\n",
                 pModal->antCtrlChain[0], pModal->antCtrlChain[1]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Ant Chain 2     0x%08X  |\n",
                 pModal->antCtrlChain[2]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Antenna Common  0x%08X  |  Antenna Gain Chain 0   %3d  |\n",
                 pModal->antCtrlCommon, pModal->antennaGainCh[0]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Antenna Gain Chain 1   %3d  |  Antenna Gain Chain 2   %3d  |\n",
                 pModal->antennaGainCh[1], pModal->antennaGainCh[2]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Switch Settling        %3d  |  TxRxAttenuation Ch 0   %3d  |\n",
                 pModal->switchSettling, pModal->txRxAttenCh[0]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  TxRxAttenuation Ch 1   %3d  |  TxRxAttenuation Ch 2   %3d  |\n",
                 pModal->txRxAttenCh[1], pModal->txRxAttenCh[2]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  RxTxMargin Chain 0     %3d  |  RxTxMargin Chain 1     %3d  |\n",
                 pModal->rxTxMarginCh[0], pModal->rxTxMarginCh[1]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  RxTxMargin Chain 2     %3d  |  adc desired size       %3d  |\n",
                 pModal->rxTxMarginCh[2], pModal->adcDesiredSize);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  pga desired size       %3d  |  tx end to xlna on      %3d  |\n",
                 pModal->pgaDesiredSize, pModal->txEndToRxOn);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  xlna gain Chain 0      %3d  |  xlna gain Chain 1      %3d  |\n",
                 pModal->xlnaGainCh[0], pModal->xlnaGainCh[1]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  xlna gain Chain 2      %3d  |  tx end to xpa off      %3d  |\n",
                 pModal->xlnaGainCh[2], pModal->txEndToXpaOff);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  tx frame to xpa on     %3d  |  thresh62               %3d  |\n",
                 pModal->txFrameToXpaOn, pModal->thresh62);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  noise floor thres 0    %3d  |  noise floor thres 1    %3d  |\n",
                 pModal->noiseFloorThreshCh[0], pModal->noiseFloorThreshCh[1]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  noise floor thres 2    %3d  |  Xpd Gain Mask 0x%X | Xpd %2d  |\n",
                 pModal->noiseFloorThreshCh[2], pModal->xpdGain, pModal->xpd);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  IQ Cal I Chain 0       %3d  |  IQ Cal Q Chain 0       %3d  |\n",
                 pModal->iqCalICh[0], pModal->iqCalQCh[0]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  IQ Cal I Chain 1       %3d  |  IQ Cal Q Chain 1       %3d  |\n",
                 pModal->iqCalICh[1], pModal->iqCalQCh[1]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  IQ Cal I Chain 2       %3d  |  IQ Cal Q Chain 2       %3d  |\n",
                 pModal->iqCalICh[2], pModal->iqCalQCh[2]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  pdGain Overlap     %2d.%d dB  |  Analog Output Bias (ob)%3d  |\n",
                 pModal->pdGainOverlap / 2, (pModal->pdGainOverlap % 2) * 5, pModal->ob);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Analog Driver Bias (db)%3d  |  Xpa bias level         %3d  |\n",
                 pModal->db, pModal->xpaBiasLvl);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |                              |  Xpa bias level         0x%04x  |\n",
                 pModal->db, pModal->xpaBiasLvlFreq[0]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |                              |  Xpa bias level         0x%04x  |\n",
                 pModal->db, pModal->xpaBiasLvlFreq[1]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |                              |  Xpa bias level         0x%04x  |\n",
                 pModal->db, pModal->xpaBiasLvlFreq[2]);
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  pwr dec 2 chain    %2d.%d dB  |  pwr dec 3 chain    %2d.%d dB  |\n",
                 pModal->pwrDecreaseFor2Chain / 2, (pModal->pwrDecreaseFor2Chain % 2) * 5,
                 pModal->pwrDecreaseFor3Chain / 2, (pModal->pwrDecreaseFor3Chain % 2) * 5);
        if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  xatten2Db  Chain 0     %3d  |  xatten2Db  Chain 1     %3d  |\n",
                     pModal->xatten2Db[0], pModal->xatten2Db[1]);
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  xatten2Margin  Chain 0 %3d  |  xatten2Margin  Chain 1 %3d  |\n",
                     pModal->xatten2Margin[0], pModal->xatten2Margin[1]);
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  ob_ch1                 %3d  |  db_ch1                 %3d  |\n",
                     pModal->ob_ch1, pModal->db_ch1);
        }
        if (IS_EEP_MINOR_V3(ahp)) {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  txFrameToDataStart     %3d  |  txFrameToPaOn          %3d  |\n",
                    pModal->txFrameToDataStart, pModal->txFrameToPaOn);
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  ht40PowerIncForPdadc   %3d  |  bswAtten Chain 0       %3d  |\n",
                    pModal->ht40PowerIncForPdadc, pModal->bswAtten[0]);
    }
    }
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |=============================================================|\n");

#define SPUR_TO_KHZ(is2GHz, spur)    ( ((spur) + ((is2GHz) ? 23000 : 49000)) * 100 )
#define NO_SPUR                      ( 0x8000 )

    /* Print spur data */
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "=======================Spur Information======================\n");
    for (i = 0; i < 2; i++) {
        if ((i == 0) && !(pBase->opCapFlags & AR5416_OPFLAGS_11A)) {
            continue;
        }
        if ((i == 1) && !(pBase->opCapFlags & AR5416_OPFLAGS_11G)) {
            continue;
        }
        if (i == 0) {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| 11A Spurs in MHz                                          |\n");
        } else {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| 11G Spurs in MHz                                          |\n");
        }
        pSpurData = (u_int16_t *)ar5416Eep->modalHeader[i].spurChans;
        noMoreSpurs = 0;
        for (j = 0; j < AR5416_EEPROM_MODAL_SPURS; j++) {
            if ((pModal->spurChans[j].spurChan == NO_SPUR) || noMoreSpurs) {
                noMoreSpurs = 1;
                HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|  NO SPUR  ");
            } else {
                HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|   %4d.%1d  ", SPUR_TO_KHZ(i,pModal->spurChans[j].spurChan )/1000,
                         ((SPUR_TO_KHZ(i, pModal->spurChans[j].spurChan))/100) % 10);
            }
        }
    for (j = 0; j < AR5416_EEPROM_MODAL_SPURS; j++) {
            if ((pModal->spurChans[j].spurChan == NO_SPUR) || noMoreSpurs) {
                noMoreSpurs = 1;
                HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|             ");
            } else {
                HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|<%2d.%1d-=-%2d.%1d>", 
                       pModal->spurChans[j].spurRangeLow / 10, pModal->spurChans[j].spurRangeLow % 10,
                       pModal->spurChans[j].spurRangeHigh / 10, pModal->spurChans[j].spurRangeHigh % 10);
            }
        }
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
    }
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|===========================================================|\n");

#undef SPUR_TO_KHZ
#undef NO_SPUR

    /* Print calibration info */
    for (i = 0; i < 2; i++) {
        if ((i == 0) && !(pBase->opCapFlags & AR5416_OPFLAGS_11A)) {
            continue;
        }
        if ((i == 1) && !(pBase->opCapFlags & AR5416_OPFLAGS_11G)) {
            continue;
        }
        for (c = 0; c < AR5416_MAX_CHAINS; c++) {
            if (!((pBase->txMask >> c) & 1)) {
                continue;
            }
            if (i == 0) {
                pDataPerChannel = ar5416Eep->calPierData5G[c];
                numChannels = AR5416_EEPDEF_NUM_5G_CAL_PIERS;
                pChannel = ar5416Eep->calFreqPier5G;
            } else {
                pDataPerChannel = ar5416Eep->calPierData2G[c];
                numChannels = AR5416_EEPDEF_NUM_2G_CAL_PIERS;
                pChannel = ar5416Eep->calFreqPier2G;
            }
            xpdMask = ar5416Eep->modalHeader[i].xpdGain;

            numXpdGain = 0;
            /* Calculate the value of xpdgains from the xpdGain Mask */
            for (j = 1; j <= AR5416_EEPDEF_PD_GAINS_IN_MASK; j++) {
                if ((xpdMask >> (AR5416_EEPDEF_PD_GAINS_IN_MASK - j)) & 1) {
                    if (numXpdGain >= AR5416_EEPDEF_NUM_PD_GAINS) {
                        HALASSERT(0);
                        break;
                    }
                    xpdGainValues[numXpdGain++] = (u_int16_t)(AR5416_EEPDEF_PD_GAINS_IN_MASK - j);
                }
            }

            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "===============Power Calibration Information Chain %d =======================\n", c);
            for (channelRowCnt = 0; channelRowCnt < numChannels; channelRowCnt += 5) {
                temp = channelRowCnt + 5;
                rowEndsAtChan = (u_int16_t)AH_MIN(numChannels, temp);
                for (channelCount = channelRowCnt; (channelCount < rowEndsAtChan)  && (pChannel[channelCount] != 0xff); channelCount++) {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|     %04d     ", fbin2freq(pChannel[channelCount], (HAL_BOOL)i));
                }
                HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n|==============|==============|==============|==============|==============|\n");
                for (channelCount = channelRowCnt; (channelCount < rowEndsAtChan)  && (pChannel[channelCount] != 0xff); channelCount++) {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|pdadc pwr(dBm)");
                }
                HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");

                for (k = 0; k < numXpdGain; k++) {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|              |              |              |              |              |\n");
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| PD_Gain %2d   |              |              |              |              |\n",
                             xpdGainMapping[xpdGainValues[k]]);

                    for (vpdCount = 0; vpdCount < AR5416_EEPDEF_PD_GAIN_ICEPTS; vpdCount++) {
                        for (channelCount = channelRowCnt; (channelCount < rowEndsAtChan)  && (pChannel[channelCount] != 0xff); channelCount++) {
                            tempPower = (int8_t)(pDataPerChannel[channelCount].pwrPdg[k][vpdCount] / 4);
                            if(AR_SREV_MERLIN_10_OR_LATER(ah)) {
                                tempPower += AR5416_PWR_TABLE_OFFSET_DB;
                            }
                            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|  %02d   %2d.%02d  ", pDataPerChannel[channelCount].vpdPdg[k][vpdCount],
                                     tempPower,
                                     (pDataPerChannel[channelCount].pwrPdg[k][vpdCount] % 4) * 25);
                        }
                        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
                    }
                }
                HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|              |              |              |              |              |\n");
                HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|==============|==============|==============|==============|==============|\n");
            }
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
        }
    }

    /* Print Target Powers */
    for (i = 0; i < 7; i++) {
        if ( (i <=2) && !(pBase->opCapFlags & AR5416_OPFLAGS_11A)) {
            continue;
        }
        if ((i >= 3 && i <=6) && !(pBase->opCapFlags & AR5416_OPFLAGS_11G)) {
            continue;
        }
        switch(i) {
        case 0:
            pPowerInfo = (CAL_TARGET_POWER_HT *)ar5416Eep->calTargetPower5G;
            pPowerInfoLeg = ar5416Eep->calTargetPower5G;
            numPowers = AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS;
            ratePrint = 0;
            numRates = 4;
            is2Ghz = AH_FALSE;
            legacyPowers = AH_TRUE;
            break;
        case 1:
            pPowerInfo = ar5416Eep->calTargetPower5GHT20;
            numPowers = AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS;
            ratePrint = 2;
            numRates = 8;
            is2Ghz = AH_FALSE;
            legacyPowers = AH_FALSE;
            break;
        case 2:
            pPowerInfo = ar5416Eep->calTargetPower5GHT40;
            numPowers = AR5416_EEPDEF_NUM_5G_40_TARGET_POWERS;
            ratePrint = 2;
            numRates = 8;
            is2Ghz = AH_FALSE;
            legacyPowers = AH_FALSE;
            break;
        case 3:
            pPowerInfo = (CAL_TARGET_POWER_HT *)ar5416Eep->calTargetPowerCck;
            pPowerInfoLeg = ar5416Eep->calTargetPowerCck;
            numPowers = AR5416_EEPDEF_NUM_2G_CCK_TARGET_POWERS;
            ratePrint = 1;
            numRates = 4;
            is2Ghz = AH_TRUE;
            legacyPowers = AH_TRUE;
            break;
        case 4:
            pPowerInfo = (CAL_TARGET_POWER_HT *)ar5416Eep->calTargetPower2G;
            pPowerInfoLeg = ar5416Eep->calTargetPower2G;
            numPowers = AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS;
            ratePrint = 0;
            numRates = 4;
            is2Ghz = AH_TRUE;
            legacyPowers = AH_TRUE;
           break;
        case 5:
            pPowerInfo = ar5416Eep->calTargetPower2GHT20;
            numPowers = AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS;
            ratePrint = 2;
            numRates = 8;
            is2Ghz = AH_TRUE;
            legacyPowers = AH_FALSE;
            break;
        case 6:
            pPowerInfo = ar5416Eep->calTargetPower2GHT40;
            numPowers = AR5416_EEPDEF_NUM_2G_40_TARGET_POWERS;
            ratePrint = 2;
            numRates = 8;
            is2Ghz = AH_TRUE;
            legacyPowers = AH_FALSE;
            break;
        }
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "============================Target Power Info===============================\n");
        for (j = 0; j < numPowers; j+=4) {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|   %s   ", sTargetPowerMode[i]);
            for (k = j; k < AH_MIN(j + 4, numPowers); k++) {
                if(legacyPowers) {
                    channelValue = fbin2freq(pPowerInfoLeg[k].bChannel, (HAL_BOOL)is2Ghz);
                } else {
                    channelValue = fbin2freq(pPowerInfo[k].bChannel, (HAL_BOOL)is2Ghz);
                }
                if (channelValue != 0xff) {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|     %04d     ", channelValue);
                } else {
                    numPowers = k;
                    continue;
                }
            }
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|==============|==============|==============|==============|==============|\n");
            for (r = 0; r < numRates; r++) {
                HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|%s", sRatePrint[ratePrint][r]);
                for (k = j; k < AH_MIN(j + 4, numPowers); k++) {
                    if(legacyPowers) {
                        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|     %2d.%d     ", pPowerInfoLeg[k].tPow2x[r] / 2,
                             (pPowerInfoLeg[k].tPow2x[r] % 2) * 5);
                    } else {
                        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|     %2d.%d     ", pPowerInfo[k].tPow2x[r] / 2,
                             (pPowerInfo[k].tPow2x[r] % 2) * 5);
                    }
                }
                HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
            }
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|==============|==============|==============|==============|==============|\n");
        }
    }
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "\n");

    /* Print Band Edge Powers */
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "=======================Test Group Band Edge Power========================\n");
    for (i = 0; (ar5416Eep->ctlIndex[i] != 0) && (i < AR5416_EEPDEF_NUM_CTLS); i++) {
        if (((ar5416Eep->ctlIndex[i] & CTL_MODE_M) == CTL_11A) || 
            ((ar5416Eep->ctlIndex[i] & CTL_MODE_M) ==  CTL_5GHT20) || 
            ((ar5416Eep->ctlIndex[i] & CTL_MODE_M) ==  CTL_5GHT40)) { 
            is2Ghz = AH_FALSE;
        } else {
            is2Ghz = AH_TRUE;
        }
        
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|                                                                       |\n");
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| CTL: 0x%02x %s                                           |\n",
               ar5416Eep->ctlIndex[i], 
         (ar5416Eep->ctlIndex[i] & CTL_MODE_M) < (sizeof(sCtlType)/sizeof(char *)) ? sCtlType[ar5416Eep->ctlIndex[i] & CTL_MODE_M]:"unknown mode");
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

        for (c = 0; c < numTxChains; c++) {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "======================== Edges for Chain %d ==============================\n", c);
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| edge  ");
            for (j = 0; j < AR5416_EEPDEF_NUM_BAND_EDGES; j++) {
                if (ar5416Eep->ctlData[i].ctlEdges[c][j].bChannel == AR5416_BCHAN_UNUSED) {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|  --   ");
                } else {
                    tempFreq = fbin2freq(ar5416Eep->ctlData[i].ctlEdges[c][j].bChannel, is2Ghz);
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| %04d  ", tempFreq);
                }
            }
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| power ");
            for (j = 0; j < AR5416_EEPDEF_NUM_BAND_EDGES; j++) {
                if (ar5416Eep->ctlData[i].ctlEdges[c][j].bChannel == AR5416_BCHAN_UNUSED) {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|  --   ");
                } else {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| %2d.%d  ", ar5416Eep->ctlData[i].ctlEdges[c][j].tPower / 2,
                        (ar5416Eep->ctlData[i].ctlEdges[c][j].tPower % 2) * 5);
                }
            }
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| flag  ");
            for (j = 0; j < AR5416_EEPDEF_NUM_BAND_EDGES; j++) {
                if (ar5416Eep->ctlData[i].ctlEdges[c][j].bChannel == AR5416_BCHAN_UNUSED) {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|  --   ");
                } else {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|   %1d   ", ar5416Eep->ctlData[i].ctlEdges[c][j].flag);
                }
            }
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "=========================================================================\n");
        }
    }

}

#ifdef AH_AR5416_OVRD_TGT_PWR
static void
ar5416EepDefDumpTgtPower(struct ath_hal *ah, ar5416_eeprom_def_t *eep)
{
    int ii, jj;

    HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "2G CCK  \n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_2G_CCK_TARGET_POWERS; ii++) {
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x: ", eep->calTargetPowerCck[ii].bChannel);
        for (jj = 0; jj < 4; jj++) {
            HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x ", eep->calTargetPowerCck[ii].tPow2x[jj]);
        }
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n");
    }

    HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n2G OFDM \n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS; ii++) {
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x: ", eep->calTargetPower2G[ii].bChannel);
        for (jj = 0; jj < 4; jj++) {
            HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x ", eep->calTargetPower2G[ii].tPow2x[jj]);
        }
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n");
    }

    HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n2G HT20 \n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS; ii++) {
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x: ", eep->calTargetPower2GHT20[ii].bChannel);
        for (jj = 0; jj < 8; jj++) {
            HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x ", eep->calTargetPower2GHT20[ii].tPow2x[jj]);
        }
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n");
    }

    HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n2G HT40 \n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_2G_40_TARGET_POWERS; ii++) {
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x: ", eep->calTargetPower2GHT40[ii].bChannel);
        for (jj = 0; jj < 8; jj++) {
            HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x ", eep->calTargetPower2GHT40[ii].tPow2x[jj]);
        }
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n");
    }

    HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n5G OFDM \n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS; ii++) {
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x: ", eep->calTargetPower5G[ii].bChannel);
        for (jj = 0; jj < 4; jj++) {
            HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x ", eep->calTargetPower5G[ii].tPow2x[jj]);
        }
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n");
    }

    HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n5G HT20 \n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS; ii++) {
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x: ", eep->calTargetPower5GHT20[ii].bChannel);
        for (jj = 0; jj < 8; jj++) {
            HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x ", eep->calTargetPower5GHT20[ii].tPow2x[jj]);
        }
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n");
    }

    HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n5G HT40 \n");
    for (ii = 0; ii < AR5416_EEPDEF_NUM_5G_40_TARGET_POWERS; ii++) {
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x: ", eep->calTargetPower5GHT40[ii].bChannel);
        for (jj = 0; jj < 8; jj++) {
            HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "%2x ", eep->calTargetPower5GHT40[ii].tPow2x[jj]);
        }
        HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n");
    }

    HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, "\n");
}

#endif /* AH_AR5416_OVRD_TGT_PWR */

#endif /* EEPROM_DUMP */
/*------- All debug functions End ------------------*/
#undef N
/*---------------------------------------------*/
#endif /* AH_SUPPORT_EEPROM_DEF */
#endif /* AH_SUPPORT_AR5416 */

