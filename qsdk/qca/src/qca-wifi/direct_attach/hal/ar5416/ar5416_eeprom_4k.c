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
#include "ah.h"
#include "ah_internal.h"
#include "ah_eeprom.h"
#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"
#include "ar5416eep.h"

#ifdef AH_SUPPORT_AR5416
#ifdef AH_SUPPORT_EEPROM_4K

#define N(a)            (sizeof (a) / sizeof (a[0]))

static inline void
ar5416Eeprom4kUpdateChecksum(struct ath_hal_5416 *ahp);
HAL_BOOL ar5416Eep4kSetPowerPerRateTable(struct ath_hal *ah,
    ar5416_eeprom_4k_t *pEepData,
    HAL_CHANNEL_INTERNAL *chan, int16_t *ratesArray,
    u_int16_t cfgCtl, u_int16_t AntennaReduction,
    u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit, u_int8_t chainmask);
static inline HAL_BOOL ar5416Eep4kSetPowerCalTable(struct ath_hal *ah,
    ar5416_eeprom_4k_t *pEepData,
    HAL_CHANNEL_INTERNAL *chan,
    int16_t *pTxPowerIndexOffset);
static inline HAL_BOOL
ar5416Eep4kGetGainBoundariesAndPdadcs(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan, CAL_DATA_PER_FREQ_EEP4K *pRawDataSet,
    u_int8_t * bChans,  u_int16_t availPiers,
    u_int16_t tPdGainOverlap, int16_t *pMinCalPower, u_int16_t * pPdGainBoundaries,
    u_int8_t * pPDADCValues, u_int16_t numXpdGains);


/**************************************************************
 * ar5416Eep4kGetMaxEdgePower
 *
 * Find the maximum conformance test limit for the given channel and CTL info
 */
static inline u_int16_t
ar5416Eep4kGetMaxEdgePower(u_int16_t freq, CAL_CTL_EDGES *pRdEdgesPower, HAL_BOOL is2GHz)
{
    u_int16_t twiceMaxEdgePower = AR5416_MAX_RATE_POWER;
    int      i;

    /* Get the edge power */
    for (i = 0; (i < AR5416_EEP4K_NUM_BAND_EDGES) && (pRdEdgesPower[i].bChannel != AR5416_BCHAN_UNUSED) ; i++)
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
ar5416Eeprom4kGet(struct ath_hal_5416 *ahp, EEPROM_PARAM param)
{
    ar5416_eeprom_4k_t *eep = &ahp->ah_eeprom.map.map4k;
    MODAL_EEP4K_HEADER *pModal = &eep->modalHeader;
    BASE_EEP4K_HEADER  *pBase  = &eep->baseEepHeader;

    switch (param)
    {
    case EEP_NFTHRESH_2:
        return pModal->noiseFloorThreshCh[0];
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
    case EEP_OB_2:
        return pModal->ob_0;
    case EEP_DB_2:
        return pModal->db1_1;
    case EEP_MINOR_REV:
        return pBase->version & AR5416_EEP_VER_MINOR_MASK;
    case EEP_TX_MASK:
        return pBase->txMask;
    case EEP_RX_MASK:
        return pBase->rxMask;
    case EEP_PWR_TABLE_OFFSET:
        return AR5416_PWR_TABLE_OFFSET_DB;
    case EEP_FRAC_N_5G:
        return 0;
    case EEP_DEV_TYPE:
        return pBase->deviceType;
    case EEP_TXGAIN_TYPE:
        return pBase->txGainType;
    case EEP_RC_CHAIN_MASK:
        return pBase->txMask;
    case EEP_MODAL_VER:
        return pModal->version;
    case EEP_ANT_DIV_CTL1:
        return pModal->antdiv_ctl1;
    case EEP_ANT_DIV_CTL2:
        return pModal->antdiv_ctl2;
    case EEP_SMART_ANTENNA:
        return pModal->smart_antenna;
    default:
        HALASSERT(0);
        return 0;
    }
}

static inline void
ar5416Eeprom4kUpdateChecksum(struct ath_hal_5416 *ahp)
{
    ar5416_eeprom_4k_t *eep = &ahp->ah_eeprom.map.map4k;
    BASE_EEP4K_HEADER  *pBase  = &eep->baseEepHeader;
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
ar5416Eeprom4kSetParam(struct ath_hal *ah, EEPROM_PARAM param, u_int32_t value)
{
    HAL_BOOL result = AH_TRUE;
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_4k_t *eep = &ahp->ah_eeprom.map.map4k;
    BASE_EEP4K_HEADER  *pBase  = &eep->baseEepHeader;
    int offsetRd = 0;
    int offsetChksum = 0;
    u_int16_t checksum;

    offsetRd = AR5416_EEP4K_START_LOC + (int) (offsetof(struct BaseEep4kHeader, regDmn[0]) >> 1);
    offsetChksum = AR5416_EEP4K_START_LOC + (offsetof(struct BaseEep4kHeader, checksum) >> 1);

    switch (param) {
    case EEP_REG_0:
        pBase->regDmn[0] = (u_int16_t) value;

        result = AH_FALSE;
        if (ar5416EepromWrite(ah, offsetRd, (u_int16_t) value)) {
            ar5416Eeprom4kUpdateChecksum(ahp);
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


int32_t ar5416Eep4kMinCCAPwrThresGet(struct ath_hal *ah, int chain, u_int16_t channel)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_4k_t *eep = &ahp->ah_eeprom.map.map4k;

    return eep->modalHeader.noiseFloorThreshCh[chain];
}

HAL_BOOL ar5416Eep4kMinCCAPwrThresApply(struct ath_hal *ah, u_int16_t channel)
{
    int32_t value;
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_4k_t *eep = &ahp->ah_eeprom.map.map4k;

    if ((eep->baseEepHeader.eepMisc & AR5416_EEPMISC_MINCCAPWR_EN) == 0) {
        return 0;
    }

    /* Applying individual chain values */
    if (eep->baseEepHeader.rxMask & 0x1) {

        value = ar5416Eep4kMinCCAPwrThresGet(ah, 0, channel);
        OS_REG_RMW_FIELD(ah, AR_PHY_CCA_CTRL_0, AR_PHY_EXT_CCA0_THRESH62_1, value);

    }

    OS_REG_RMW_FIELD(ah, AR_PHY_CCA_CTRL_0, AR_PHY_EXT_CCA0_THRESH62_ENABLE, 1);

    return 0;
}

/*
 * Read EEPROM header info and program the device for correct operation
 * given the channel value.
 */
HAL_BOOL
ar5416Eeprom4kSetBoardValues(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
    MODAL_EEP4K_HEADER *pModal;
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_4k_t *eep = &ahp->ah_eeprom.map.map4k;
    u_int8_t    txRxAttenLocal;    /* workaround for eeprom versions <= 14.2 */
    u_int32_t   ant_config = 0;
    u_int8_t ob[5], db1[5], db2[5];
    u_int8_t ant_div_control1, ant_div_control2;
    u_int32_t regVal;
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CAPABILITIES *pCap = &ahpriv->ah_caps;
    int i;

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


    HALASSERT(owl_get_eep4k_ver(ahp) == AR5416_EEP_VER);
    HALASSERT(IS_CHAN_2GHZ(chan));

    pModal = &eep->modalHeader;

    txRxAttenLocal =  23;    /* workaround for eeprom versions < 14.3 */

    if (ahp->ah_emu_eeprom) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                 "!!! IS EMU EEPROM: %s\n",__func__);
        return AH_TRUE;
    }

    if (HAL_OK == ar5416EepromGetAntCfg(ahp, chan, 0, &ant_config)) {
        OS_REG_WRITE(ah, AR_PHY_SWITCH_COM, ant_config);
    }

    /* Only single chain is supported for chips with 4Kbits EEPROM map */

    OS_REG_WRITE(ah, AR_PHY_SWITCH_CHAIN_0,
                 pModal->antCtrlChain[0]);
    OS_REG_WRITE(ah, AR_PHY_TIMING_CTRL4(0), 
        ( OS_REG_READ(ah, AR_PHY_TIMING_CTRL4(0)) &
          ~( AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF |
             AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF ) ) |
        SM(pModal->iqCalICh[0], AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF) |
        SM(pModal->iqCalQCh[0], AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF));

    if ((eep->baseEepHeader.version & AR5416_EEP_VER_MINOR_MASK) >=
        AR5416_EEP_MINOR_VER_3)
    {
        /*
        * Large signal upgrade,
        * If 14.3 or later EEPROM, use txRxAttenLocal = pModal->txRxAttenCh[i]
        * else txRxAttenLocal is fixed value above.
        */
        txRxAttenLocal = pModal->txRxAttenCh[0];
            /* 
             * From Merlin(AR9280),
             *     bswAtten[] maps to chain specific
             *         xatten1_db(0xa20c/0xb20c 5:0)
             *     bswMargin[] maps to chain specific
             *         xatten1_margin (0xa20c/0xb20c 16:12)
             */
            OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ,
                AR_PHY_GAIN_2GHZ_XATTEN1_MARGIN, pModal->bswMargin[0]);
            OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ,
                AR_PHY_GAIN_2GHZ_XATTEN1_DB, pModal->bswAtten[0]);
            OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ,
                AR_PHY_GAIN_2GHZ_XATTEN2_MARGIN, pModal->xatten2Margin[0]);
            OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ,
                AR_PHY_GAIN_2GHZ_XATTEN2_DB, pModal->xatten2Db[0]);

            /* Set the block 1 value to block 0 value - see EV#55076 */
            OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + 0x1000,
                AR_PHY_GAIN_2GHZ_XATTEN1_MARGIN, pModal->bswMargin[0]);
            OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + 0x1000,
                AR_PHY_GAIN_2GHZ_XATTEN1_DB, pModal->bswAtten[0]);
            OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + 0x1000,
                AR_PHY_GAIN_2GHZ_XATTEN2_MARGIN, pModal->xatten2Margin[0]);
            OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + 0x1000,
                AR_PHY_GAIN_2GHZ_XATTEN2_DB, pModal->xatten2Db[0]);

    }

    /* 
     * From Merlin(AR9280),
     *     txRxAttenCh[] maps to chain specific
     *         xatten1_hyst_margin(0x9848/0xa848 13:7)
     *     rxTxMarginCh[] maps to chain specific
     *         xatten2_hyst_margin (0x9848/0xa848 20:14)
     */
    OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN,
                     AR9280_PHY_RXGAIN_TXRX_ATTEN, txRxAttenLocal);
    OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN,
                     AR9280_PHY_RXGAIN_TXRX_MARGIN, pModal->rxTxMarginCh[0]);

    /* Set the block 1 value to block 0 value - see EV#55076 */
    OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN + 0x1000,
                     AR9280_PHY_RXGAIN_TXRX_ATTEN, txRxAttenLocal);
    OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN + 0x1000,
                     AR9280_PHY_RXGAIN_TXRX_MARGIN, pModal->rxTxMarginCh[0]);

    /* Kite 1.1 WAR for Bug 35666 
     * Increase the LDO value to 1.28V before accessing analog Reg */
    if (AR_SREV_KITE_11(ah))
        OS_REG_WRITE(ah, AR9285_AN_TOP4, (AR9285_AN_TOP4_DEFAULT | 0x14) );

    /* Initialize Ant Diversity settings from EEPROM */
    if (pModal->version >= 3) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "EEPROM Model version is set to 3\n");
        ant_div_control1 = pModal->antdiv_ctl1;
        ant_div_control2 = pModal->antdiv_ctl2;

        /*
        # Table for Diversity
        #-------------------------------------------------------------------------------
        # Diversity                |Enable  | Disable, RX=LNA2 |Disable, RX=LNA1
        # ------------------------------------------------------------------------------
        # bb_enable_ant_div_lnadiv |   1    |         0        |        0
        # bb_ant_div_alt_lnaconf   |   2    |         2        |        1
        # bb_ant_div_main_lnaconf  |   1    |         1        |        2
        # bb_ant_div_alt_gaintb    |   1    |         1        |        0
        # bb_ant_div_main_gaintb   |   0    |         0        |        1
        # bb_enable_ant_fast_div   |   1    |         0        |        0
        #-------------------------------------------------------------------------------
         */
        /* The following bits are only defined for Kite to configure antenna
         * Bit 24:      enable_ant_div_lnadiv
         * Bit [26:25]  ant_div_alt_lnaconf
         * Bit [27:28]  ant_div_main_lnaconf
         * 10=LNA1 01=LNA2 11=LNA1+LNA2 00=LNA1-LNA2
         * Bit 29:      ant_div_alt_gaintb	
         * Bit 30:      ant_div_main_gaintb	
         */
        regVal = OS_REG_READ(ah, AR_PHY_MULTICHAIN_GAIN_CTL);
        regVal &= (~(AR_PHY_KITE_ANT_DIV_CTL_ALL));

        /* enable antenna diversity only if diversityControl == HAL_ANT_VARIABLE */
        if (ahp->ah_diversityControl == HAL_ANT_VARIABLE) 
            regVal |= SM(ant_div_control1, AR_PHY_KITE_ANT_DIV_CTL);

        if (ahp->ah_diversityControl == HAL_ANT_VARIABLE) {
            regVal |= SM(ant_div_control2, AR_PHY_KITE_ANT_DIV_ALT_LNACONF);
            regVal |= SM((ant_div_control2 >> 2), AR_PHY_KITE_ANT_DIV_MAIN_LNACONF);
            regVal |= SM((ant_div_control1 >> 1), AR_PHY_KITE_ANT_DIV_ALT_GAINTB);
            regVal |= SM((ant_div_control1 >> 2), AR_PHY_KITE_ANT_DIV_MAIN_GAINTB);
        } else {
            if (ahp->ah_diversityControl == HAL_ANT_FIXED_A) {
                /* Diversity disabled, RX = LNA1 */
                regVal |= SM(HAL_ANT_DIV_COMB_LNA2, AR_PHY_KITE_ANT_DIV_ALT_LNACONF);
                regVal |= SM(HAL_ANT_DIV_COMB_LNA1, AR_PHY_KITE_ANT_DIV_MAIN_LNACONF);
                regVal |= SM(AR_PHY_KITE_ANT_DIV_GAINTB_0, AR_PHY_KITE_ANT_DIV_ALT_GAINTB);
                regVal |= SM(AR_PHY_KITE_ANT_DIV_GAINTB_1, AR_PHY_KITE_ANT_DIV_MAIN_GAINTB);
            }
            else if (ahp->ah_diversityControl == HAL_ANT_FIXED_B) {
                /* Diversity disabled, RX = LNA2 */
                regVal |= SM(HAL_ANT_DIV_COMB_LNA1, AR_PHY_KITE_ANT_DIV_ALT_LNACONF);
                regVal |= SM(HAL_ANT_DIV_COMB_LNA2, AR_PHY_KITE_ANT_DIV_MAIN_LNACONF);
                regVal |= SM(AR_PHY_KITE_ANT_DIV_GAINTB_1, AR_PHY_KITE_ANT_DIV_ALT_GAINTB);
                regVal |= SM(AR_PHY_KITE_ANT_DIV_GAINTB_0, AR_PHY_KITE_ANT_DIV_MAIN_GAINTB);
            }
        }

        OS_REG_WRITE(ah, AR_PHY_MULTICHAIN_GAIN_CTL, regVal);
        regVal = OS_REG_READ(ah, AR_PHY_MULTICHAIN_GAIN_CTL);
        regVal = OS_REG_READ(ah, AR_PHY_CCK_DETECT);
        regVal &= (~AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV);
        if (ahp->ah_diversityControl == HAL_ANT_VARIABLE)
            regVal |= SM((ant_div_control1 >> 3), AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV);

        OS_REG_WRITE(ah, AR_PHY_CCK_DETECT, regVal);
        regVal = OS_REG_READ(ah, AR_PHY_CCK_DETECT);

        if (pCap->hal_ant_div_comb_support && ahp->ah_diversityControl != HAL_ANT_FIXED_A 
	     &&  ahp->ah_diversityControl != HAL_ANT_FIXED_B) {
            // If support DivComb, set MAIN to LNA1 and ALT to LNA2 at the first beginning
            regVal = OS_REG_READ(ah, AR_PHY_MULTICHAIN_GAIN_CTL);
            regVal &= (~(AR_PHY_KITE_ANT_DIV_MAIN_LNACONF | AR_PHY_KITE_ANT_DIV_ALT_LNACONF));
            regVal |= (HAL_ANT_DIV_COMB_LNA1 << AR_PHY_KITE_ANT_DIV_MAIN_LNACONF_S);
            regVal |= (HAL_ANT_DIV_COMB_LNA2 << AR_PHY_KITE_ANT_DIV_ALT_LNACONF_S); 
            regVal &= (~(AR_PHY_KITE_ANT_DIV_FAST_BIAS));      
            regVal |= (0 << AR_PHY_KITE_ANT_DIV_FAST_BIAS_S);                             
            OS_REG_WRITE(ah, AR_PHY_MULTICHAIN_GAIN_CTL, regVal);
        }
    }
    /* Initialize ob, db1 and db2 values from EEPROM */
    if (pModal->version >= 2) {
        HDPRINTF(ah, HAL_DBG_EEPROM,
                 "EEPROM Model version is set to 2 or greater\n");
        ob[0] = pModal->ob_0;
        ob[1] = pModal->ob_1;
        ob[2] = pModal->ob_2;
        ob[3] = pModal->ob_3;
        ob[4] = pModal->ob_4;

        db1[0] = pModal->db1_0;
        db1[1] = pModal->db1_1;
        db1[2] = pModal->db1_2;
        db1[3] = pModal->db1_3;
        db1[4] = pModal->db1_4;

        db2[0] = pModal->db2_0;
        db2[1] = pModal->db2_1;
        db2[2] = pModal->db2_2;
        db2[3] = pModal->db2_3;
        db2[4] = pModal->db2_4;

    } else if (pModal->version == 1) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "EEPROM Model version is set to 1 \n");
        ob[0] = pModal->ob_0;
        ob[1] = ob[2] = ob[3] = ob[4] = pModal->ob_1;
        db1[0] = pModal->db1_0;
        db1[1] = db1[2] = db1[3] = db1[4] = pModal->db1_1;
        db2[0] = pModal->db2_0;
        db2[1] = db2[2] = db2[3] = db2[4] = pModal->db2_1;
    } else {
        HDPRINTF(ah, HAL_DBG_EEPROM, "EEPROM Model version is set to 0 \n");
        for (i = 0; i < 5; i++) {
            ob[i] = pModal->ob_0;
            db1[i] = pModal->db1_0;
            db2[i] = pModal->db1_0;
        }
    }
    
	if (AR_SREV_K2(ah)) {
	    analogShiftRegRMW(ah, AR9285_AN_RF2G3, AR9271_AN_RF2G3_OB_cck,
                          AR9271_AN_RF2G3_OB_cck_S, ob[0]);    /* ob_0 */
	    analogShiftRegRMW(ah, AR9285_AN_RF2G3, AR9271_AN_RF2G3_OB_psk,
                          AR9271_AN_RF2G3_OB_psk_S, ob[1]);    /* ob_1 */
	    analogShiftRegRMW(ah, AR9285_AN_RF2G3, AR9271_AN_RF2G3_OB_qam,
                          AR9271_AN_RF2G3_OB_qam_S, ob[2]);    /* ob_2 */
	    analogShiftRegRMW(ah, AR9285_AN_RF2G3, AR9271_AN_RF2G3_DB_1,
                          AR9271_AN_RF2G3_DB_1_S, db1[0]);   /* db1_0 */
	    analogShiftRegRMW(ah, AR9285_AN_RF2G4, AR9271_AN_RF2G4_DB_2,
                          AR9271_AN_RF2G4_DB_2_S, db2[0]);   /* db2_0 */
	} else {
        regVal = OS_REG_READ(ah, AR9285_AN_RF2G3);
        regVal &= ~(AR9285_AN_RF2G3_OB_0 | AR9285_AN_RF2G3_OB_1 | AR9285_AN_RF2G3_OB_2 |
                    AR9285_AN_RF2G3_OB_3 | AR9285_AN_RF2G3_OB_4 |
                    AR9285_AN_RF2G3_DB1_0 | AR9285_AN_RF2G3_DB1_1 | AR9285_AN_RF2G3_DB1_2);
        regVal |= (SM(ob[0], AR9285_AN_RF2G3_OB_0) |
                   SM(ob[1], AR9285_AN_RF2G3_OB_1) |
                   SM(ob[2], AR9285_AN_RF2G3_OB_2) |
                   SM(ob[3], AR9285_AN_RF2G3_OB_3) |
                   SM(ob[4], AR9285_AN_RF2G3_OB_4) |
                   SM(db1[0], AR9285_AN_RF2G3_DB1_0) |
                   SM(db1[1], AR9285_AN_RF2G3_DB1_1) |
                   SM(db1[2], AR9285_AN_RF2G3_DB1_2));
        analogShiftRegWrite(ah, AR9285_AN_RF2G3, regVal);

        regVal = OS_REG_READ(ah, AR9285_AN_RF2G4);
        regVal &= ~(AR9285_AN_RF2G4_DB1_3 | AR9285_AN_RF2G4_DB1_4 |
                    AR9285_AN_RF2G4_DB2_0 | AR9285_AN_RF2G4_DB2_1 | AR9285_AN_RF2G4_DB2_2 |
                    AR9285_AN_RF2G4_DB2_3 | AR9285_AN_RF2G4_DB2_4);
        regVal |= (SM(db1[3], AR9285_AN_RF2G4_DB1_3) |
                   SM(db1[4], AR9285_AN_RF2G4_DB1_4) |
                   SM(db2[0], AR9285_AN_RF2G4_DB2_0) |
                   SM(db2[1], AR9285_AN_RF2G4_DB2_1) |
                   SM(db2[2], AR9285_AN_RF2G4_DB2_2) |
                   SM(db2[3], AR9285_AN_RF2G4_DB2_3) |
                   SM(db2[4], AR9285_AN_RF2G4_DB2_4));
        analogShiftRegWrite(ah, AR9285_AN_RF2G4, regVal);
    }

    /* Kite 1.1 WAR for Bug 35666 
     * Decrease the LDO value back to 1.20V */
    if (AR_SREV_KITE_11(ah)) {
        OS_REG_WRITE(ah, AR9285_AN_TOP4, AR9285_AN_TOP4_DEFAULT);
    }

	ar5416Eep4kMinCCAPwrThresApply(ah, chan->channel);

    OS_REG_RMW_FIELD(ah, AR_PHY_SETTLING, AR_PHY_SETTLING_SWITCH,
                     pModal->switchSettling);
    OS_REG_RMW_FIELD(ah, AR_PHY_DESIRED_SZ, AR_PHY_DESIRED_SZ_ADC,
                     pModal->adcDesiredSize);
    OS_REG_WRITE(ah, AR_PHY_RF_CTL4,
        SM(pModal->txEndToXpaOff, AR_PHY_RF_CTL4_TX_END_XPAA_OFF)
        | SM(pModal->txEndToXpaOff, AR_PHY_RF_CTL4_TX_END_XPAB_OFF)
        | SM(pModal->txFrameToXpaOn, AR_PHY_RF_CTL4_FRAME_XPAA_ON)
        | SM(pModal->txFrameToXpaOn, AR_PHY_RF_CTL4_FRAME_XPAB_ON));
    OS_REG_RMW_FIELD(ah, AR_PHY_RF_CTL3, AR_PHY_TX_END_TO_A2_RX_ON,
                     pModal->txEndToRxOn);
    OS_REG_RMW_FIELD(ah, AR_PHY_CCA, AR9280_PHY_CCA_THRESH62,
                     pModal->thresh62);
    OS_REG_RMW_FIELD(ah, AR_PHY_EXT_CCA0, AR_PHY_EXT_CCA0_THRESH62,
                     pModal->thresh62);

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
        if (IS_CHAN_HT40(chan)) {
            /* Overwrite switch settling with HT40 value */
            OS_REG_RMW_FIELD(ah, AR_PHY_SETTLING, AR_PHY_SETTLING_SWITCH,
                             pModal->swSettleHt40);
        }
    }

    /* Green Tx */
    if (ahpriv->ah_opmode == HAL_M_STA && ahpriv->ah_config.ath_hal_sta_update_tx_pwr_enable) {
        for (i = 0; i < 9; i++) {
            regList[i][1] = OS_REG_READ(ah, regList[i][0]);
        }

        /* for PAL off, OB/DB1/DB2 = 1/1/1 */
        AH5416(ah)->txPowerRecord[0][0]=0x1a1a1a1a;    //18/12/9/6 = 8/8/8/8
        AH5416(ah)->txPowerRecord[0][1]=0x0c121a1a;    //54/48/36/24 = 1/4/8/8
        AH5416(ah)->txPowerRecord[0][2]=0x1a1a1a1a;    //CCK = 8
        AH5416(ah)->txPowerRecord[0][3]=0x1a1a1a1a;    //CCK = 8
        AH5416(ah)->txPowerRecord[0][4]=0x1a1a1a1a;    //HT20 MCS3/MCS2/MCS1/MCS0 = 8/8/8/8
        AH5416(ah)->txPowerRecord[0][5]=0x0c12161a;    //HT20 MCS7/MCS6/MCS5/MCS4 = 1/4/6/8
        AH5416(ah)->txPowerRecord[0][6]=0x14141414;    //HT40 MCS3/MCS2/MCS1/MCS0 = 5/5/5/5
        AH5416(ah)->txPowerRecord[0][7]=0x0a101414;    //HT40 MCS7/MCS6/MCS5/MCS4 = 0/3/5/5
        AH5416(ah)->txPowerRecord[0][8]=0x0a0a0a0a;    //duplicate = 0
            
        /* for PAL on, OB/DB1/DB2 = 2/2/2 */
        AH5416(ah)->txPowerRecord[1][0]=0x1e1e1e1e;    //18/12/9/6   = 10/10/10/10
        AH5416(ah)->txPowerRecord[1][1]=0x1e1e1e1e;    //54/48/36/24 = 10/10/10/10
        AH5416(ah)->txPowerRecord[1][2]=0x1e1e1e1e;    //CCK = 10
        AH5416(ah)->txPowerRecord[1][3]=0x1e1e1e1e;    //CCK = 10
        AH5416(ah)->txPowerRecord[1][4]=0x1e1e1e1e;    //HT20 MCS3/MCS2/MCS1/MCS0 = 10/10/10/10
        AH5416(ah)->txPowerRecord[1][5]=0x1e1e1e1e;    //HT20 MCS7/MCS6/MCS5/MCS4 = 10/10/10/10
        AH5416(ah)->txPowerRecord[1][6]=0x18181818;    //HT40 MCS3/MCS2/MCS1/MCS0 =  7/7/7/7
        AH5416(ah)->txPowerRecord[1][7]=0x18181818;    //HT40 MCS7/MCS6/MCS5/MCS4 =  7/7/7/7
        AH5416(ah)->txPowerRecord[1][8]=0x18181818;    //duplicate = 7         
            
        /* original target power for status 3 */
        AH5416(ah)->txPowerRecord[2][0] = regList[0][1];
        AH5416(ah)->txPowerRecord[2][1] = regList[1][1];
        AH5416(ah)->txPowerRecord[2][2] = regList[2][1];
        AH5416(ah)->txPowerRecord[2][3] = regList[3][1];
        AH5416(ah)->txPowerRecord[2][4] = regList[4][1];
        AH5416(ah)->txPowerRecord[2][5] = regList[5][1];
        AH5416(ah)->txPowerRecord[2][6] = regList[6][1];
        AH5416(ah)->txPowerRecord[2][7] = regList[7][1];
        AH5416(ah)->txPowerRecord[2][8] = regList[8][1];

        /* Record PAL */
        AH5416(ah)->PALRecord = OS_REG_READ(ah, AR9285_AN_RF2G7);

        /* Record OB/DB1/DB2 */
        AH_PRIVATE(ah)->ah_ob_db1[0] = OS_REG_READ(ah, AR9285_AN_RF2G3);
        AH_PRIVATE(ah)->ah_db2[0]   = OS_REG_READ(ah, AR9285_AN_RF2G4);

        /* force OB/DB1/DB2 to 1/1/1,2/2/2 */
    	if (AR_SREV_K2(ah)) {
            for (i=1; i<ARRAY_LENGTH(AH_PRIVATE(ah)->ah_ob_db1); i++) {
                AH_PRIVATE(ah)->ah_ob_db1[i] = 
                    ((AH_PRIVATE(ah)->ah_ob_db1[0])&0xff000fff)|
                    (i<<21)|(i<<18)|(i<<15)|(i<<12); 
                AH_PRIVATE(ah)->ah_db2[i] = 
                    ((AH_PRIVATE(ah)->ah_db2[0])&0x1fffffff)|(i<<29);
            }
        } else {
            for (i=1; i<ARRAY_LENGTH(AH_PRIVATE(ah)->ah_ob_db1); i++) {
                AH_PRIVATE(ah)->ah_ob_db1[i] = 
                    ((AH_PRIVATE(ah)->ah_ob_db1[0])&0xff000000)|
                    (i<<21)|(i<<18)|(i<<15)|(i<<12)|
                    (i<<9)|(i<<6)|(i<<3)|(i<<0); 
                AH_PRIVATE(ah)->ah_db2[i] = 
                    ((AH_PRIVATE(ah)->ah_db2[0])&0x00000fff)|(i<<29)|(i<<26)|
                    (i<<23)|(i<<20)|(i<<17)|(i<<14)|(i<<11);
            }
        }
    }

    if (AR_SREV_K2(ah) || AR_SREV_KITE(ah)) {
        u_int8_t bb_desired_scale = (pModal->bb_desired_scale & 0x1f);
        if ((eep->baseEepHeader.txGainType == 0) && (bb_desired_scale != 0)) {
            u_int32_t pwrctrl, pwrctrl_8, pwrctrl_9, pwrctrl_10, pwrctrl_11, pwrctrl_12, pwrctrl_13;

            //AR_PHY_TX_PWRCTRL8: 0xa278.
            pwrctrl_8 = OS_REG_READ(ah, AR_PHY_TX_PWRCTRL8);
            pwrctrl = (u_int32_t)bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            //HDPRINTF(ah, HAL_DBG_EEPROM, "## Set K2 : pwrctrl_8_ori = %2.2x, write-val = %2.2x\n", pwrctrl_8, pwrctrl);
            pwrctrl_8 = (pwrctrl_8 & 0xc0000000) | pwrctrl;
            OS_REG_WRITE(ah, AR_PHY_TX_PWRCTRL8, pwrctrl_8);
            //HDPRINTF(ah, HAL_DBG_EEPROM, "## Set K2 : Read pwrctrl_8 afetr write = %2.2x\n", OS_REG_READ(ah, AR_PHY_TX_PWRCTRL8));

            //AR_PHY_TX_PWRCTRL9: 0xa27c.
            pwrctrl_9 = OS_REG_READ(ah, AR_PHY_TX_PWRCTRL9);
            pwrctrl = (u_int32_t)bb_desired_scale;
            pwrctrl = (pwrctrl << 10) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            //HDPRINTF(ah, HAL_DBG_EEPROM, "## Set K2 : pwrctrl_9_ori = %2.2x, write-val = %2.2x\n", pwrctrl_9, pwrctrl);
            pwrctrl_9 = (pwrctrl_9 & 0xfff07c00) | pwrctrl;
            OS_REG_WRITE(ah, AR_PHY_TX_PWRCTRL9, pwrctrl_9);
            //HDPRINTF(ah, HAL_DBG_EEPROM, "## Set K2 : Read pwrctrl_9 afetr write = %2.2x\n", OS_REG_READ(ah, AR_PHY_TX_PWRCTRL9));

            //AR_PHY_TX_PWRCTRL10: 0xa394. (Not define.)
            pwrctrl_10 = OS_REG_READ(ah, 0xa394);
            pwrctrl = (u_int32_t)bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            //HDPRINTF(ah, HAL_DBG_EEPROM, "## Set K2 : pwrctrl_10_ori = %2.2x, write-val = %2.2x\n", pwrctrl_10, pwrctrl);
            pwrctrl_10 = (pwrctrl_10 & 0xc0000000) | pwrctrl;
            OS_REG_WRITE(ah, 0xa394, pwrctrl_10);
            //HDPRINTF(ah, HAL_DBG_EEPROM, "## Set K2 : Read pwrctrl_10 afetr write = %2.2x\n", OS_REG_READ(ah, 0xa394));

            //AR_PHY_CH0_TX_PWRCTRL11: 0xa398.
            pwrctrl_11 = OS_REG_READ(ah, AR_PHY_CH0_TX_PWRCTRL11);
            pwrctrl = (u_int32_t)bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            //HDPRINTF(ah, HAL_DBG_EEPROM, "## Set K2 : pwrctrl_11_ori = %2.2x, write-val = %2.2x\n", pwrctrl_11, pwrctrl);
            pwrctrl_11 = (pwrctrl_11 & 0xfffffc00) | pwrctrl;
            OS_REG_WRITE(ah, AR_PHY_CH0_TX_PWRCTRL11, pwrctrl_11);
            //HDPRINTF(ah, HAL_DBG_EEPROM, "## Set K2 : Read pwrctrl_11 afetr write = %2.2x\n", OS_REG_READ(ah, AR_PHY_CH0_TX_PWRCTRL11));

            //AR_PHY_CH0_TX_PWRCTRL12: 0xa3dc. (Not define.)
            pwrctrl_12 = OS_REG_READ(ah, 0xa3dc);
            pwrctrl = (u_int32_t)bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            //HDPRINTF(ah, HAL_DBG_EEPROM, "## Set K2 : pwrctrl_12_ori = %2.2x, write-val = %2.2x\n", pwrctrl_12, pwrctrl);
            pwrctrl_12 = (pwrctrl_12 & 0xc0000000) | pwrctrl;
            OS_REG_WRITE(ah, 0xa3dc, pwrctrl_12);
            //HDPRINTF(ah, HAL_DBG_EEPROM, "## Set K2 : Read pwrctrl_12 afetr write = %2.2x\n", OS_REG_READ(ah, 0xa3dc));

            //AR_PHY_CH0_TX_PWRCTRL13: 0xa3e0. (Not define.)
            pwrctrl_13 = OS_REG_READ(ah, 0xa3e0);
            pwrctrl = (u_int32_t)bb_desired_scale;
            pwrctrl = (pwrctrl << 5) | bb_desired_scale;
            //HDPRINTF(ah, HAL_DBG_EEPROM, "## Set K2 : pwrctrl_13_ori = %2.2x, write-val = %2.2x\n", pwrctrl_13, pwrctrl);
            pwrctrl_13 = (pwrctrl_13 & 0xfffffc00) | pwrctrl;
            OS_REG_WRITE(ah, 0xa3e0, pwrctrl_13);
            //HDPRINTF(ah, HAL_DBG_EEPROM, "## Set K2 : Read pwrctrl_13 afetr write = %2.2x\n", OS_REG_READ(ah, 0xa3e0));
        }
    }

    return AH_TRUE;
}


u_int32_t
ar5416Eeprom4kINIFixup(struct ath_hal *ah,ar5416_eeprom_4k_t *pEepData, u_int32_t reg, u_int32_t value)
{
    return (value);
}


/**************************************************************
 * ar5416Eep4kSetPowerPerRateTable
 *
 * Sets the transmit power in the baseband for the given
 * operating channel and mode.
 */
HAL_BOOL
ar5416Eep4kSetPowerPerRateTable(struct ath_hal *ah, ar5416_eeprom_4k_t *pEepData,
    HAL_CHANNEL_INTERNAL *chan,
    int16_t *ratesArray, u_int16_t cfgCtl,
    u_int16_t AntennaReduction,
    u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit,
    u_int8_t chainmask)
{
    /* Local defines to distinguish between extension and control CTL's */
#define EXT_ADDITIVE (0x8000)
#define CTL_11G_EXT (CTL_11G | EXT_ADDITIVE)
#define CTL_11B_EXT (CTL_11B | EXT_ADDITIVE)
    u_int16_t twiceMaxEdgePower = AR5416_MAX_RATE_POWER;
    static const u_int16_t tpScaleReductionTable[5] = { 0, 3, 6, 9, AR5416_MAX_RATE_POWER };

    int i;
    int16_t  twiceLargestAntenna;
    CAL_CTL_DATA_EEP4K *rep;
    CAL_TARGET_POWER_LEG targetPowerOfdm, targetPowerCck = {0, {0, 0, 0, 0}};
    CAL_TARGET_POWER_LEG targetPowerOfdmExt = {0, {0, 0, 0, 0}}, targetPowerCckExt = {0, {0, 0, 0, 0}};
    CAL_TARGET_POWER_HT  targetPowerHt20, targetPowerHt40 = {0, {0, 0, 0, 0}};
    int16_t scaledPower=0, minCtlPower, maxRegAllowedPower;
    #define SUB_NUM_CTL_MODES_AT_2G_40 3    /* excluding HT40, EXT-OFDM, EXT-CCK */
    u_int16_t ctlModesFor11g[] = {CTL_11B, CTL_11G, CTL_2GHT20, CTL_11B_EXT, CTL_11G_EXT, CTL_2GHT40};
    u_int16_t numCtlModes, *pCtlMode, ctlMode, freq;
    CHAN_CENTERS centers;
    int tx_chainmask;
    u_int16_t twiceMinEdgePower;
    struct ath_hal_5416 *ahp = AH5416(ah);

    HALASSERT(IS_CHAN_2GHZ(chan));

    tx_chainmask = chainmask ? chainmask : ahp->ah_txchainmask;

    /* Static analysis check, if tx_chainmask is 0, return */
    if(!owl_get_ntxchains(tx_chainmask)) {
        return AH_FALSE;
    }

    ar5416GetChannelCenters(ah, chan, &centers);

    /* Compute TxPower reduction due to Antenna Gain */
    twiceLargestAntenna = pEepData->modalHeader.antennaGainCh[0];

    twiceLargestAntenna =  (int16_t)AH_MIN((AntennaReduction) - twiceLargestAntenna, 0);

    /* scaledPower is the minimum of the user input power level and the regulatory allowed power level */
    maxRegAllowedPower = twiceMaxRegulatoryPower + twiceLargestAntenna;

    /* Use ah_tp_scale - see bug 30070.*/
    if (AH_PRIVATE(ah)->ah_tp_scale != HAL_TP_SCALE_MAX) { 
        maxRegAllowedPower -= (tpScaleReductionTable[(AH_PRIVATE(ah)->ah_tp_scale)] * 2);
    }

    scaledPower = AH_MIN(powerLimit, maxRegAllowedPower);
    scaledPower = AH_MAX(0, scaledPower);

    /*
    * Get target powers from EEPROM - our baseline for TX Power
    */

    /* Setup for CTL modes */
    numCtlModes = N(ctlModesFor11g) - SUB_NUM_CTL_MODES_AT_2G_40; /* CTL_11B, CTL_11G, CTL_2GHT20 */
    pCtlMode = ctlModesFor11g;

    ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPowerCck,
        AR5416_EEP4K_NUM_2G_CCK_TARGET_POWERS, &targetPowerCck, 4, AH_FALSE);
    ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPower2G,
        AR5416_EEP4K_NUM_2G_20_TARGET_POWERS, &targetPowerOfdm, 4, AH_FALSE);
    ar5416GetTargetPowers(ah, chan, pEepData->calTargetPower2GHT20,
        AR5416_EEP4K_NUM_2G_20_TARGET_POWERS, &targetPowerHt20, 8, AH_FALSE);

    if (IS_CHAN_HT40(chan))
    {
        numCtlModes = N(ctlModesFor11g);    /* All 2G CTL's */
        ar5416GetTargetPowers(ah, chan, pEepData->calTargetPower2GHT40,
            AR5416_EEP4K_NUM_2G_40_TARGET_POWERS, &targetPowerHt40, 8, AH_TRUE);
        ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPowerCck,
            AR5416_EEP4K_NUM_2G_CCK_TARGET_POWERS, &targetPowerCckExt, 4, AH_TRUE);
        ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPower2G,
            AR5416_EEP4K_NUM_2G_20_TARGET_POWERS, &targetPowerOfdmExt, 4, AH_TRUE);
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
           
        if (owl_get_eep4k_ver(ahp) == 14 && owl_get_eep4k_rev(ahp) <= 2) 
          twiceMaxEdgePower = AR5416_MAX_RATE_POWER;
         // TODO: Does 14.3 still have this restriction???

        HDPRINTF(ah, HAL_DBG_POWER_MGMT, "LOOP-Mode ctlMode %d < %d, isHt40CtlMode %d, EXT_ADDITIVE %d\n",
            ctlMode, numCtlModes, isHt40CtlMode, (pCtlMode[ctlMode] & EXT_ADDITIVE));
        /* walk through each CTL index stored in EEPROM */
        for (i = 0; (i < AR5416_EEP4K_NUM_CTLS) && pEepData->ctlIndex[i]; i++)
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
                
                twiceMinEdgePower = ar5416Eep4kGetMaxEdgePower(freq,
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
        case CTL_11G:
            for (i = 0; i < N(targetPowerOfdm.tPow2x); i++)
            {
                targetPowerOfdm.tPow2x[i] = (u_int8_t)AH_MIN(targetPowerOfdm.tPow2x[i], minCtlPower);

#ifdef ATH_BT_COEX
                if ((ahp->ah_btCoexConfigType == HAL_BT_COEX_CFG_3WIRE) &&
                    (ahp->ah_btCoexFlag & HAL_BT_COEX_FLAG_LOWER_TX_PWR) && 
                    (ahp->ah_btWlanIsolation < HAL_BT_COEX_ISOLATION_FOR_NO_COEX))
                {
                    u_int8_t reducePow2x = (HAL_BT_COEX_ISOLATION_FOR_NO_COEX - ahp->ah_btWlanIsolation) << 1;

                    if (reducePow2x <= targetPowerOfdm.tPow2x[i]) {
                        targetPowerOfdm.tPow2x[i] -= reducePow2x;
                    }
                }
#endif
            }
            break;
        case CTL_2GHT20:
            for (i = 0; i < N(targetPowerHt20.tPow2x); i++)
            {
                targetPowerHt20.tPow2x[i] = (u_int8_t)AH_MIN(targetPowerHt20.tPow2x[i], minCtlPower);

#ifdef ATH_BT_COEX
                if ((ahp->ah_btCoexConfigType == HAL_BT_COEX_CFG_3WIRE) &&
                    (ahp->ah_btCoexFlag & HAL_BT_COEX_FLAG_LOWER_TX_PWR) && 
                    (ahp->ah_btWlanIsolation < HAL_BT_COEX_ISOLATION_FOR_NO_COEX))
                {
                    u_int8_t reducePow2x = (HAL_BT_COEX_ISOLATION_FOR_NO_COEX - ahp->ah_btWlanIsolation) << 1;

                    if (reducePow2x <= targetPowerHt20.tPow2x[i]) {
                        targetPowerHt20.tPow2x[i] -= reducePow2x;
                    }
                }
#endif
            }
            break;
        case CTL_11B_EXT:
            targetPowerCckExt.tPow2x[0] = (u_int8_t)AH_MIN(targetPowerCckExt.tPow2x[0], minCtlPower);
            break;
        case CTL_11G_EXT:
            targetPowerOfdmExt.tPow2x[0] = (u_int8_t)AH_MIN(targetPowerOfdmExt.tPow2x[0], minCtlPower);
            break;
        case CTL_2GHT40:
            for (i = 0; i < N(targetPowerHt40.tPow2x); i++)
            {
                targetPowerHt40.tPow2x[i] = (u_int8_t)AH_MIN(targetPowerHt40.tPow2x[i], minCtlPower);

#ifdef ATH_BT_COEX
                if ((ahp->ah_btCoexConfigType == HAL_BT_COEX_CFG_3WIRE) &&
                    (ahp->ah_btCoexFlag & HAL_BT_COEX_FLAG_LOWER_TX_PWR) && 
                    (ahp->ah_btWlanIsolation < HAL_BT_COEX_ISOLATION_FOR_NO_COEX))
                {
                    u_int8_t reducePow2x = (HAL_BT_COEX_ISOLATION_FOR_NO_COEX - ahp->ah_btWlanIsolation) << 1;

                    if (reducePow2x <= targetPowerHt40.tPow2x[i]) {
                        targetPowerHt40.tPow2x[i] -= reducePow2x;
                    }
                }
#endif
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


    ratesArray[rate1l]  = targetPowerCck.tPow2x[0];
    ratesArray[rate2s] = ratesArray[rate2l]  = targetPowerCck.tPow2x[1];
    ratesArray[rate5_5s] = ratesArray[rate5_5l] = targetPowerCck.tPow2x[2];
    ratesArray[rate11s] = ratesArray[rate11l] = targetPowerCck.tPow2x[3];

    if (IS_CHAN_HT40(chan))
    {
        for (i = 0; i < N(targetPowerHt40.tPow2x); i++)
        {
            ratesArray[rateHt40_0 + i] = targetPowerHt40.tPow2x[i];
        }
        ratesArray[rateDupOfdm] = targetPowerHt40.tPow2x[0];
        ratesArray[rateDupCck]  = targetPowerHt40.tPow2x[0];
        ratesArray[rateExtOfdm] = targetPowerOfdmExt.tPow2x[0];
        ratesArray[rateExtCck]  = targetPowerCckExt.tPow2x[0];
    }
    return AH_TRUE;
#undef EXT_ADDITIVE
#undef CTL_11G_EXT
#undef CTL_11B_EXT
}


/**************************************************************
 * ar5416Eep4kGetGainBoundariesAndPdadcs
 *
 * Uses the data points read from EEPROM to reconstruct the pdadc power table
 * Called by ar5416Eep4kSetPowerCalTable only.
 */
static inline HAL_BOOL
ar5416Eep4kGetGainBoundariesAndPdadcs(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan, CAL_DATA_PER_FREQ_EEP4K *pRawDataSet,
    u_int8_t * bChans,  u_int16_t availPiers,
    u_int16_t tPdGainOverlap, int16_t *pMinCalPower, u_int16_t * pPdGainBoundaries,
    u_int8_t * pPDADCValues, u_int16_t numXpdGains)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int       i, j, k;
    int16_t   ss;         /* potentially -ve index for taking care of pdGainOverlap */
    u_int16_t  idxL = 0, idxR = 0, numPiers; /* Pier indexes */

    u_int8_t   *pVpdL, *pVpdR, *pPwrL, *pPwrR;
    u_int8_t   minPwrT4[AR5416_EEP4K_NUM_PD_GAINS];
    u_int8_t   maxPwrT4[AR5416_EEP4K_NUM_PD_GAINS];
    int16_t   vpdStep;
    int16_t   tmpVal;
    u_int16_t  sizeCurrVpdTable, maxIndex, tgtIndex;
    HAL_BOOL    match;
    int16_t  minDelta = 0;
    CHAN_CENTERS centers;
#define PD_GAIN_BOUNDARY_DEFAULT (58)

    /*
     * If numXpdGains is not greater than 0, then the
     * arrays "minPwrT4" and "maxPwrT4" will not be
     * initialized properly.Hence, there is no meaning
     * in moving forward.
     */
    if (numXpdGains <= 0) {
        return AH_FALSE;
    }

    if (numXpdGains > AR5416_EEP4K_NUM_PD_GAINS) {
        return AH_FALSE;
    }
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
                pRawDataSet[idxL].vpdPdg[i], AR5416_EEP4K_PD_GAIN_ICEPTS, ahp->ah_vpdTableI[i]);
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
            maxPwrT4[i] = AH_MIN(pPwrL[AR5416_EEP4K_PD_GAIN_ICEPTS - 1], pPwrR[AR5416_EEP4K_PD_GAIN_ICEPTS - 1]);
            HALASSERT(maxPwrT4[i] > minPwrT4[i]);

            /* Fill pier Vpds */
            ar5416FillVpdTable(minPwrT4[i], maxPwrT4[i], pPwrL, pVpdL, AR5416_EEP4K_PD_GAIN_ICEPTS, ahp->ah_vpdTableL[i]);
            ar5416FillVpdTable(minPwrT4[i], maxPwrT4[i], pPwrR, pVpdR, AR5416_EEP4K_PD_GAIN_ICEPTS, ahp->ah_vpdTableR[i]);

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
    while (i < AR5416_EEP4K_PD_GAINS_IN_MASK)
    {
        pPdGainBoundaries[i] = PD_GAIN_BOUNDARY_DEFAULT;
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
 * ar5416Eep4kSetPowerCalTable
 *
 * Pull the PDADC piers from cal data and interpolate them across the given
 * points as well as from the nearest pier(s) to get a power detector
 * linear voltage to power level table.
 */
static inline HAL_BOOL
ar5416Eep4kSetPowerCalTable(struct ath_hal *ah, ar5416_eeprom_4k_t *pEepData, HAL_CHANNEL_INTERNAL *chan, int16_t *pTxPowerIndexOffset)
{

    CAL_DATA_PER_FREQ_EEP4K *pRawDataset;
    u_int8_t  *pCalBChans = AH_NULL;
    u_int16_t pdGainOverlap_t2;
    u_int8_t  pdadcValues[AR5416_NUM_PDADC_VALUES] = {0};
    u_int16_t gainBoundaries[AR5416_EEP4K_PD_GAINS_IN_MASK] = {0};
    u_int16_t numPiers, i, j;
    int16_t  tMinCalPower;
    u_int16_t numXpdGain, xpdMask;
    u_int16_t xpdGainValues[AR5416_EEP4K_NUM_PD_GAINS] = {0, 0};
    u_int32_t reg32, regOffset;
#ifdef ATH_TX99_DIAG
    struct ath_hal_5416 *ahp;
#endif

#ifdef ATH_TX99_DIAG
    ahp = AH5416(ah);
    AH_PRIVATE(ah)->ah_pwr_offset = (int8_t)ar5416EepromGet(ahp, EEP_PWR_TABLE_OFFSET);
#endif

    HALASSERT(IS_CHAN_2GHZ(chan));

    xpdMask = pEepData->modalHeader.xpdGain;

    if ((pEepData->baseEepHeader.version & AR5416_EEP_VER_MINOR_MASK) >= AR5416_EEP_MINOR_VER_2)
    {
        pdGainOverlap_t2 = pEepData->modalHeader.pdGainOverlap;
    }
    else
    {
        pdGainOverlap_t2 = (u_int16_t)(MS(OS_REG_READ(ah, AR_PHY_TPCRG5), AR_PHY_TPCRG5_PD_GAIN_OVERLAP));
    }


    pCalBChans = pEepData->calFreqPier2G;
    numPiers = AR5416_EEP4K_NUM_2G_CAL_PIERS;
  
    numXpdGain = 0;
    /* Calculate the value of xpdgains from the xpdGain Mask */
    for (i = 1; i <= AR5416_EEP4K_PD_GAINS_IN_MASK; i++)
    {
        if ((xpdMask >> (AR5416_EEP4K_PD_GAINS_IN_MASK - i)) & 1)
        {
            if (numXpdGain >= AR5416_EEP4K_NUM_PD_GAINS)
            {
                HALASSERT(0);
                break;
            }
            xpdGainValues[numXpdGain] = (u_int16_t)(AR5416_EEP4K_PD_GAINS_IN_MASK - i);
            numXpdGain++;
        }
    }

    /* write the detector gain biases and their number */
    OS_REG_RMW_FIELD(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_NUM_PD_GAIN, (numXpdGain - 1) & 0x3);
    OS_REG_RMW_FIELD(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_GAIN_1, xpdGainValues[0]);
    OS_REG_RMW_FIELD(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_GAIN_2, xpdGainValues[1]);
    /* AR5416_EEP4K_NUM_PD_GAINS is only 2, so set AR_PHY_TPCRG1_PD_GAIN_3 to 0 */
    OS_REG_RMW_FIELD(ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_GAIN_3, 0);

    /*
     * NOTE:
     * If AR5416_EEP4K_MAX_CHAINS ever exceeds 1,
     * we need to execute a loop here.
     * i.e Currently the code is always executed for i=0.
     */
    if (pEepData->baseEepHeader.txMask & 1) {

       pRawDataset = pEepData->calPierData2G[0];
       if (!ar5416Eep4kGetGainBoundariesAndPdadcs(ah, chan, pRawDataset,
                pCalBChans, numPiers,
                pdGainOverlap_t2,
                &tMinCalPower, gainBoundaries,
                pdadcValues, numXpdGain)) {
                HDPRINTF(ah, HAL_DBG_CHANNEL,
                         "%s:get pdadc failed due to numXpdGain value %d\n", __func__, numXpdGain);
                return AH_FALSE;
        }

        ENABLE_REG_WRITE_BUFFER

        /*
         * Note the pdadc table may not start at 0 dBm power, could be
         * negative or greater than 0.  Need to offset the power
         * values by the amount of minPower for griffin
         */
        OS_REG_WRITE(ah, AR_PHY_TPCRG5,
            SM(pdGainOverlap_t2, AR_PHY_TPCRG5_PD_GAIN_OVERLAP) |
            SM(gainBoundaries[0], AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_1)  |
            SM(gainBoundaries[1], AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_2)  |
            SM(gainBoundaries[2], AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_3)  |
            SM(gainBoundaries[3], AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_4));

        /*
         * Write the power values into the baseband power table
         */
        regOffset = AR_PHY_BASE + (672 << 2);
        for (j = 0; j < 32; j++) {
                reg32 = ((pdadcValues[4*j + 0] & 0xFF) << 0)  |
                    ((pdadcValues[4*j + 1] & 0xFF) << 8)  |
                    ((pdadcValues[4*j + 2] & 0xFF) << 16) |
                    ((pdadcValues[4*j + 3] & 0xFF) << 24) ;
                OS_REG_WRITE(ah, regOffset, reg32);

                HDPRINTF(ah, HAL_DBG_PHY_IO, "PDADC (%d): %4.4x %8.8x\n",
                    0, regOffset, reg32);
                HDPRINTF(ah, HAL_DBG_PHY_IO,
		            "PDADC: Chain %d | PDADC %3d Value %3d | "
                    "PDADC %3d Value %3d | PDADC %3d Value %3d | PDADC %3d Value %3d |\n",
                    0,
                    4*j, pdadcValues[4*j],
                    4*j+1, pdadcValues[4*j + 1],
                    4*j+2, pdadcValues[4*j + 2],
                    4*j+3, pdadcValues[4*j + 3]);

                regOffset += 4;
         }
         OS_REG_WRITE_FLUSH(ah);

         DISABLE_REG_WRITE_BUFFER
    }

    *pTxPowerIndexOffset = 0;

    return AH_TRUE;
}

HAL_STATUS
ar5416Eeprom4kSetTransmitPower(struct ath_hal *ah,
    ar5416_eeprom_4k_t *pEepData, HAL_CHANNEL_INTERNAL *chan, u_int16_t cfgCtl,
    u_int16_t twiceAntennaReduction, u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit)
{
#define POW_SM(_r, _s)     (((_r) & 0x3f) << (_s))

    MODAL_EEP4K_HEADER *pModal = &pEepData->modalHeader;
    struct ath_hal_5416 *ahp = AH5416(ah);
    int16_t ratesArray[Ar5416RateSize];
    int16_t  txPowerIndexOffset = 0;
    u_int16_t  extra_txpow = 0;
    u_int8_t ht40PowerIncForPdadc = 2;
    int i;

    HALASSERT(owl_get_eep4k_ver(ahp) == AR5416_EEP_VER);

    HALASSERT(IS_CHAN_2GHZ(chan));

    OS_MEMZERO(ratesArray, sizeof(ratesArray));
    if (ahp->ah_emu_eeprom)
		return HAL_OK;

    if ((pEepData->baseEepHeader.version & AR5416_EEP_VER_MINOR_MASK) >= AR5416_EEP_MINOR_VER_2)
    {
        ht40PowerIncForPdadc = pModal->ht40PowerIncForPdadc;
    }

    if (ahp->ah_dynconf) {
        for (i = 0; i < Ar5416RateSize; i++) {
            ratesArray[i] = ahp->ah_ratesArray[i];
        }
    } else {
        if (!ar5416Eep4kSetPowerPerRateTable(ah, pEepData, chan,
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

    if (!ar5416Eep4kSetPowerCalTable(ah, pEepData, chan, &txPowerIndexOffset))
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
    /*
     * Adjust ratesArray to be w.r.t. the base value used by the WLAN chip.
     * For pre-merlin, no adjustment is needed, since the chip's reference
     * value is 0 dBm.  For merlin and beyond, an adjustment is needed,
     * since the chip's reference value is -5 dBm.
     */
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        for (i = 0; i < Ar5416RateSize; i++) {
            ratesArray[i] -= AR5416_PWR_TABLE_OFFSET_DB * 2;
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

	ENABLE_REG_WRITE_BUFFER

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
    /* Write the CCK power per rate set */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE3,
        POW_SM(ratesArray[rate2s], 24)
      | POW_SM(ratesArray[rate2l], 16)
      | POW_SM(ratesArray[rateXr], 8) /* XR target power */
      | POW_SM(ratesArray[rate1l], 0)
    );
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE4,
        POW_SM(ratesArray[rate11s],  24)
      | POW_SM(ratesArray[rate11l],  16)
      | POW_SM(ratesArray[rate5_5s], 8)
      | POW_SM(ratesArray[rate5_5l], 0)
    );
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
                   extra_txpow,  0)
        );
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE8,
            POW_SM(ratesArray[rateHt40_7] + ht40PowerIncForPdadc, 24)
          | POW_SM(ratesArray[rateHt40_6] + ht40PowerIncForPdadc, 16)
          | POW_SM(ratesArray[rateHt40_5] + ht40PowerIncForPdadc, 8)
          | POW_SM(ratesArray[rateHt40_4] + ht40PowerIncForPdadc, 0)
        );
        /* Write the Dup/Ext 40 power per rate set */
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE9,
            POW_SM(ratesArray[rateExtOfdm], 24)
          | POW_SM(ratesArray[rateExtCck],  16)
          | POW_SM(ratesArray[rateDupOfdm], 8)
          | POW_SM(ratesArray[rateDupCck],  0)
        );
    }

#undef POW_SM
    OS_REG_WRITE_FLUSH(ah);

	DISABLE_REG_WRITE_BUFFER

    return HAL_OK;
}

void
ar5416Eeprom4kSetAddac(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
    MODAL_EEP4K_HEADER *pModal;
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_4k_t *eep = &ahp->ah_eeprom.map.map4k;
    u_int8_t biaslevel;

    if (AH_PRIVATE(ah)->ah_mac_version != AR_SREV_VERSION_SOWL)
        return;

    HALASSERT(owl_get_eep4k_ver(ahp) == AR5416_EEP_VER);
    HALASSERT(IS_CHAN_2GHZ(chan));

    /* Xpa bias levels in eeprom are valid from rev 14.7 */
    if (owl_get_eep4k_rev(ahp) < AR5416_EEP_MINOR_VER_7)
        return;

    if (ahp->ah_emu_eeprom) return;

    pModal = &eep->modalHeader;

    if (pModal->xpaBiasLvl != 0xff) {
        biaslevel = pModal->xpaBiasLvl;
        /* Apply bias level to the ADDAC values in the INI array */
        INI_RA(&ahp->ah_iniAddac, 7, 1) =
            (INI_RA(&ahp->ah_iniAddac, 7, 1) & (~0x18)) | biaslevel << 3;
    } 
}

HAL_STATUS
ar5416CheckEeprom4k(struct ath_hal *ah)
{
    u_int32_t sum = 0, el;
    u_int16_t *eepdata;
    int i;
    struct ath_hal_5416 *ahp = AH5416(ah);
    HAL_BOOL need_swap = AH_FALSE;
    ar5416_eeprom_4k_t *eep = (ar5416_eeprom_4k_t *)&ahp->ah_eeprom;

    /* eeprom size is hardcoded to 4k */
#if 0
    if (!owl_eeprom_read(ah, AR_EEPROM_PROTECT, &eeval))
    {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: cannot read EEPROM protection "
            "bits; read locked?\n", __func__);
        return HAL_EEREAD;
    }
    HDPRINTF(ah, HAL_DBG_EEPROM, "EEPROM protect 0x%x\n", eeval);
    ahp->ah_eeprotect = eeval;
    /* XXX check proper access before continuing */
#endif
    if (!ar5416EepDataInFlash(ah))
    {
        u_int16_t magic, magic2;
        int addr;

        if (!ahp->ah_priv.priv.ah_eeprom_read(
                                   ah, AR5416_EEPROM_MAGIC_OFFSET, &magic)) {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Reading Magic # failed\n", __func__);
            return AH_FALSE;
        }
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Read Magic = 0x%04X\n", __func__, magic);

        if (magic != AR5416_EEPROM_MAGIC) {

            magic2 = SWAP16(magic);

            if (magic2 == AR5416_EEPROM_MAGIC) {
                need_swap = AH_TRUE;
                eepdata = (u_int16_t *)(&ahp->ah_eeprom);
                
                for (addr=0; 
                     addr<sizeof(ar5416_eeprom_4k_t)/sizeof(u_int16_t);
                     addr++) {
                     
                    u_int16_t temp;
                    temp = SWAP16(*eepdata);
                    *eepdata = temp;
                    eepdata++;

                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "0x%04X  ", *eepdata);
                    if (((addr+1)%6) == 0)
                        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "\n");
                }
            } else {
                HDPRINTF(ah, HAL_DBG_EEPROM, "Invalid EEPROM Magic. endianness missmatch.\n");
                return HAL_EEBADSUM;
            }
        }
    }
    HDPRINTF(ah, HAL_DBG_EEPROM, "need_swap = %s.\n", need_swap?"True":"False");

    if (need_swap) {
        el = SWAP16(ahp->ah_eeprom.map.map4k.baseEepHeader.length);
    } else {
        el = ahp->ah_eeprom.map.map4k.baseEepHeader.length;
    }

    eepdata = (u_int16_t *)(&ahp->ah_eeprom);
    for (i = 0; i < AH_MIN(el, sizeof(ar5416_eeprom_4k_t))/sizeof(u_int16_t); i++)
        sum ^= *eepdata++;

    if (need_swap) {
        /*
        *  preddy: EEPROM endianness does not match. So change it
        *  8bit values in eeprom data structure does not need to be swapped
        *  Only >8bits (16 & 32) values need to be swapped
        *  If a new 16 or 32 bit field is added to the EEPROM contents,
        *  please make sure to swap the field here
        */
        u_int32_t integer;
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
        integer = SWAP32(eep->modalHeader.antCtrlCommon);
        eep->modalHeader.antCtrlCommon = integer;

        for (i = 0; i < AR5416_EEP4K_MAX_CHAINS; i++) {
            integer = SWAP32(eep->modalHeader.antCtrlChain[i]);
            eep->modalHeader.antCtrlChain[i] = integer;
        }

        for (i = 0; i < AR5416_EEPROM_MODAL_SPURS; i++) {
            word = SWAP16(eep->modalHeader.spurChans[i].spurChan);
            eep->modalHeader.spurChans[i].spurChan = word;
        }
    }

    /* Check CRC - Attach should fail on a bad checksum */
    if (sum != 0xffff || owl_get_eep4k_ver(ahp) != AR5416_EEP_VER ||
        owl_get_eep4k_rev(ahp) < AR5416_EEP_NO_BACK_VER) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "Bad EEPROM checksum 0x%x or revision 0x%04x\n",
            sum, owl_get_eep4k_rev(ahp));
        return HAL_EEBADSUM;
    }

    return HAL_OK;
}

#if AH_PRIVATE_DIAG
void
ar5416FillEmuEeprom4k(struct ath_hal_5416 *ahp)
{
    ar5416_eeprom_t *eep = &ahp->ah_eeprom;

    eep->map.map4k.baseEepHeader.version = AR5416_EEP_VER << 12;
    eep->map.map4k.baseEepHeader.macAddr[0] = 0x00;
    eep->map.map4k.baseEepHeader.macAddr[1] = 0x03;
    eep->map.map4k.baseEepHeader.macAddr[2] = 0x7F;
    eep->map.map4k.baseEepHeader.macAddr[3] = 0xBA;
    eep->map.map4k.baseEepHeader.macAddr[4] = 0xD0;
    eep->map.map4k.baseEepHeader.regDmn[0] = 0;
    eep->map.map4k.baseEepHeader.opCapFlags =
        AR5416_OPFLAGS_11G | AR5416_OPFLAGS_11A;
    eep->map.map4k.baseEepHeader.deviceCap = 0;
    eep->map.map4k.baseEepHeader.rfSilent = 0;
    eep->map.map4k.modalHeader.noiseFloorThreshCh[0] = -1;
}
#endif /* AH_PRIVATE_DIAG */

HAL_BOOL
ar5416FillEeprom4k(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar5416_eeprom_4k_t *eep = &ahp->ah_eeprom.map.map4k;
    u_int16_t *eep_data;
    int addr, eep_start_loc;

    eep_start_loc = 64;  /* for Kite starting location is 128 bytes */

    if (!ar5416EepDataInFlash(ah))
    {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Reading from EEPROM, not flash\n", __func__);
    }
    eep_data = (u_int16_t *)eep;
    for (addr = 0; addr < sizeof(ar5416_eeprom_4k_t) / sizeof(u_int16_t); 
        addr++)
    {
        if (!ahp->ah_priv.priv.ah_eeprom_read(ah, addr + eep_start_loc, 
            eep_data))
        {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Unable to read eeprom region \n", __func__);
            return AH_FALSE;
        }
        eep_data ++;
    }
    return AH_TRUE;
}

u_int8_t
ar5416Eeprom4kGetNumAntConfig(struct ath_hal_5416 *ahp, HAL_FREQ_BAND freq_band)
{
    return 1; /* default antenna configuration */
}

HAL_STATUS
ar5416Eeprom4kGetAntCfg(struct ath_hal_5416 *ahp, HAL_CHANNEL_INTERNAL *chan,
                   u_int8_t index, u_int32_t *config)
{
    MODAL_EEP4K_HEADER *pModal;
    ar5416_eeprom_4k_t  *eep = &ahp->ah_eeprom.map.map4k;
    HALASSERT(IS_CHAN_2GHZ(chan));

    pModal = &eep->modalHeader;

    switch (index) {
        case 0:
            *config = pModal->antCtrlCommon;
            return HAL_OK;
        default:
            break;
    }

    return HAL_EINVAL;
}

#undef N
/*---------------------------------------------*/
#endif /* AH_SUPPORT_EEPROM_4K */
#endif /* AH_SUPPORT_AR5416 */
