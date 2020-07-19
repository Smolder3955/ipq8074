/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2008-2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */


/****** Includes <> "" ********************************************************/

#include "opt_ah.h"
#include "ah.h"
#include "ah_internal.h"
#include "ah_eeprom.h"
#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"
#include "ar5416/ar5416eep.h"

#ifdef AH_SUPPORT_AR5416
#ifdef AH_SUPPORT_EEPROM_AR9287

#define N(a)            (sizeof (a) / sizeof (a[0]))

static inline HAL_BOOL
ar9287EepromGetGainBoundariesAndPdadcs(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan, CAL_DATA_PER_FREQ_AR9287 *pRawDataSet,
    u_int8_t * bChans,  u_int16_t availPiers,
    u_int16_t tPdGainOverlap, int16_t *pMinCalPower, u_int16_t * pPdGainBoundaries,
    u_int8_t * pPDADCValues, u_int16_t numXpdGains);

 
#ifdef EEPROM_DUMP
static void ar9287EepromPrintPowerPerRate(struct ath_hal *ah, int16_t pRatesPower[]);
static void ar9287EepromDump(struct ath_hal *ah, ar9287_eeprom_t *ar5416Eep);
#endif

#ifdef ART_BUILD
ar9287_eeprom_t ar9287_eeprom_default =
{
    //BASE_EEPAR9287_HEADER  baseEepHeader;         // 32 B
    {
        0x02d7, //u_int16_t  length;
        0xc6f4, //u_int16_t  checksum;
        0xe004, //u_int16_t  version;
        0x16, //u_int8_t opCapFlags;
        0x00, //u_int8_t   eepMisc;	
        {0x0000, 0x001f}, //u_int16_t  regDmn[2];
        {0x0,0x03,0x3f,0x13,0x03,0x28}, //u_int8_t   macAddr[6];
        3, //u_int8_t   rxMask;  
        3, //u_int8_t   txMask;
        0, //u_int16_t  rfSilent; 
        0, //u_int16_t  blueToothOptions;
        0, //u_int16_t  deviceCap;
        0x00091b00, //u_int32_t  binBuildNumber;
        5, //u_int8_t   deviceType;
        1, //u_int8_t   openLoopPwrCntl; //#bits used: 1, bit0: 1: use open loop power control, 0: use closed loop power control
        -5, //int8_t    pwrTableOffset;  // offset in dB to be added to beginning of pdadc table in calibration 
        -6, //int8_t     tempSensSlope;  // temperature sensor slope for temp compensation
        -6, //int8_t     tempSensSlopePalOn; // ditto, but when PAL is ON
        {0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0
        },//u_int8_t   futureBase[29];
    },
    //u_int8_t                 custData[AR9287_DATA_SZ];          // 30 B
    {
	 0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0,
     0, 0, 0, 0, 0, 0, 0, 0   
    },
    //MODAL_EEPAR9287_HEADER modalHeader;        // 99 B           
    {
        {0x10, 0x10}, //u_int32_t  antCtrlChain[AR9287_MAX_CHAINS];       // 8
        0x210, //u_int32_t  antCtrlCommon;                         // 4
        {0, 0}, //int8_t    antennaGainCh[AR9287_MAX_CHAINS];      // 2
        45, //u_int8_t   switchSettling;                        // 1
        {32, 32}, //u_int8_t   txRxAttenCh[AR9287_MAX_CHAINS];        // 2  //xatten1_hyst_margin for merlin (0x9848/0xa848 13:7)
        {0, 0}, //u_int8_t   rxTxMarginCh[AR9287_MAX_CHAINS];       // 2  //xatten2_hyst_margin for merlin (0x9848/0xa848 20:14)
        -30, //int8_t    adcDesiredSize;                        // 1
        0, //u_int8_t   txEndToXpaOff;                         // 1
        2, //u_int8_t   txEndToRxOn;                           // 1
        14, //u_int8_t   txFrameToXpaOn;                        // 1
        28, //u_int8_t   thresh62;                              // 1
        {-1, -1}, //int8_t    noiseFloorThreshCh[AR9287_MAX_CHAINS]; // 2
        0x02, //u_int8_t   xpdGain;                               // 1
        1, //u_int8_t   xpd;                                   // 1
        {0,0}, //int8_t    iqCalICh[AR9287_MAX_CHAINS];           // 2
        {0,0}, //int8_t    iqCalQCh[AR9287_MAX_CHAINS];           // 2
        1.5, //u_int8_t   pdGainOverlap;                         // 1
        0, //u_int8_t   xpaBiasLvl;                            // 1
        14, //u_int8_t   txFrameToDataStart;                    // 1
        14, //u_int8_t   txFrameToPaOn;                         // 1   --> 30 B
        0, //u_int8_t   ht40PowerIncForPdadc;                  // 1
        {0,0}, //u_int8_t   bswAtten[AR9287_MAX_CHAINS];           // 2  //xatten1_db for merlin (0xa20c/b20c 5:0)
        {0,0}, //u_int8_t   bswMargin[AR9287_MAX_CHAINS];          // 2  //xatten1_margin for merlin (0xa20c/b20c 16:12
        45, //u_int8_t   swSettleHt40;                          // 1
	    0, //u_int8_t   version;								 // 1
        3, //u_int8_t   db1;                                   // 1
        3, //u_int8_t   db2;                                   // 1
        3, //u_int8_t   ob_cck;                                // 1
        3, //u_int8_t   ob_psk;                                // 1
        3, //u_int8_t   ob_qam;                                // 1
        3, //u_int8_t   ob_pal_off;                            // 1
        //u_int8_t   futureModal[30];
        {0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   
        },                       // 30
        //SPUR_CHAN spurChans[AR9287_EEPROM_MODAL_SPURS];  // 20 B  
        {{0x8000, 0, 0},{0, 0, 0},{0, 0, 0},{0, 0, 0}},         
    },
    //u_int8_t                 calFreqPier2G[AR9287_NUM_2G_CAL_PIERS];  // 3 B      
    {0, 0, 0},
    //CAL_DATA_PER_FREQ_U_AR9287     calPierData2G[AR9287_MAX_CHAINS][AR9287_NUM_2G_CAL_PIERS]; // 6x40 = 240
    {
        {
            //{
			    0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            //},
            //{
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            //},
            //{
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            //}
        },
        {
            //{
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            //},
            //{
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            //},
            //{
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                0, 0, 0, 0, 0, 0, 0, 0, 0, 0
            //}
        }
    },
    //CAL_TARGET_POWER_LEG    calTargetPowerCck[AR9287_NUM_2G_CCK_TARGET_POWERS];   // 3x5 = 15 B
    {
        {0, {0, 0, 0, 0}},
        {0, {0, 0, 0, 0}},
        {0, {0, 0, 0, 0}}
    },
    //CAL_TARGET_POWER_LEG    calTargetPower2G[AR9287_NUM_2G_20_TARGET_POWERS];     // 3x5 = 15 B
    {
        {0, {0, 0, 0, 0}},
        {0, {0, 0, 0, 0}},
        {0, {0, 0, 0, 0}}
    },
    //CAL_TARGET_POWER_HT     calTargetPower2GHT20[AR9287_NUM_2G_20_TARGET_POWERS]; // 3x9 = 27 B
    {
        {0, {0, 0, 0, 0, 0, 0, 0, 0}},
        {0, {0, 0, 0, 0, 0, 0, 0, 0}},
        {0, {0, 0, 0, 0, 0, 0, 0, 0}}
    },
    //CAL_TARGET_POWER_HT     calTargetPower2GHT40[AR9287_NUM_2G_40_TARGET_POWERS]; // 3x9 = 27 B
    {
        {0, {0, 0, 0, 0, 0, 0, 0, 0}},
        {0, {0, 0, 0, 0, 0, 0, 0, 0}},
        {0, {0, 0, 0, 0, 0, 0, 0, 0}}
    },
    //u_int8_t                 ctlIndex[AR9287_NUM_CTLS]; // 12 B
    //{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0x11, 0x12, 0x15, 0x17, 0x41, 0x42, 0x45, 0x47, 0x31, 0x32, 0x35, 0x37},
    //CAL_CTL_DATA_AR9287     ctlData[AR9287_NUM_CTLS];  // 2x8x12 = 192 B 
    {
        {//1
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        {//2
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        {//3
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        {//4
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        {//5
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        {//6
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        {//7
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        {//8
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        {//9
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        {//10
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        {//11
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
        {//12
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
        },
    },

    //u_int8_t                 padding; //1
    0,
};       

void ar9287EepromLoadDefaults(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ahp->ah_eeprom.map.mapAr9287 = ar9287_eeprom_default;
}
#endif /* ART_BUILD */

u_int32_t
ar9287EepromGet(struct ath_hal_5416 *ahp, EEPROM_PARAM param)
{
    ar9287_eeprom_t *eep = &ahp->ah_eeprom.map.mapAr9287;
    MODAL_EEPAR9287_HEADER *pModal = &eep->modalHeader;
    BASE_EEPAR9287_HEADER  *pBase  = &eep->baseEepHeader;
    u_int16_t ver_minor; 

    ver_minor = pBase->version & AR9287_EEP_VER_MINOR_MASK;

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
    case EEP_MINOR_REV:
        return ver_minor;
    case EEP_TX_MASK:
        return pBase->txMask;
    case EEP_RX_MASK:
        return pBase->rxMask;
    case EEP_PWR_TABLE_OFFSET:
        return AR5416_PWR_TABLE_OFFSET_DB;
    case EEP_DEV_TYPE:
        return pBase->deviceType;
	case EEP_OL_PWRCTRL:
		return pBase->openLoopPwrCntl;
    case EEP_TEMPSENSE_SLOPE:
        if ( ver_minor >= AR9287_EEP_MINOR_VER_2) {
            return pBase->tempSensSlope;
        } else {
            return 0;
        }
    case EEP_TEMPSENSE_SLOPE_PAL_ON:
        if ( ver_minor >= AR9287_EEP_MINOR_VER_3) {
            return pBase->tempSensSlopePalOn;
        } else {
            return 0;
        }
    default:
        HALASSERT(0);
        return 0;
    }
}


static inline void
ar9287EepromUpdateChecksum(struct ath_hal_5416 *ahp)
{
    ar9287_eeprom_t *eep = &ahp->ah_eeprom.map.mapAr9287;
    BASE_EEPAR9287_HEADER  *pBase  = &eep->baseEepHeader;
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
ar9287EepromSetParam(struct ath_hal *ah, EEPROM_PARAM param, u_int32_t value)
{
    HAL_BOOL result = AH_TRUE;
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar9287_eeprom_t *eep = &ahp->ah_eeprom.map.mapAr9287;
    BASE_EEPAR9287_HEADER  *pBase  = &eep->baseEepHeader;
    int offsetRd = 0;
    int offsetChksum = 0;
    u_int16_t checksum;

    offsetRd = AR9287_EEP_START_LOC + (int) (offsetof(struct BaseEepAr9287Header, regDmn[0]) >> 1);
    offsetChksum = AR9287_EEP_START_LOC + (offsetof(struct BaseEepAr9287Header, checksum) >> 1);

    switch (param) {
    case EEP_REG_0:
        pBase->regDmn[0] = (u_int16_t) value;

        result = AH_FALSE;
        if (ar5416EepromWrite(ah, offsetRd, (u_int16_t) value)) {
            ar9287EepromUpdateChecksum(ahp);
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

u_int32_t
ar9287EepromINIFixup(struct ath_hal *ah, ar9287_eeprom_t *pEepData, u_int32_t reg, u_int32_t value)
{
    return (value);
}


HAL_STATUS
ar9287EepromGetAntCfg(struct ath_hal_5416 *ahp, HAL_CHANNEL_INTERNAL *chan,
                   u_int8_t index, u_int32_t *config)
{
    ar9287_eeprom_t  *eep = &ahp->ah_eeprom.map.mapAr9287;
    MODAL_EEPAR9287_HEADER *pModal = &eep->modalHeader;

    switch (index) {
        case 0:
            *config = pModal->antCtrlCommon & 0xFFFF;
            return HAL_OK;
        case 1:
            break;

        default:
            break;
    }

    return HAL_EINVAL;
}

int32_t ar5416EepAr9287MinCCAPwrThresGet(struct ath_hal *ah, int chain, u_int16_t channel)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar9287_eeprom_t *eep = &ahp->ah_eeprom.map.mapAr9287;

    return eep->modalHeader.noiseFloorThreshCh[chain];
}

HAL_BOOL ar5416EepAr9287MinCCAPwrThresApply(struct ath_hal *ah, u_int16_t channel)
{
    int32_t value;
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar9287_eeprom_t *eep = &ahp->ah_eeprom.map.mapAr9287;

    if ((eep->baseEepHeader.eepMisc & AR9287_EEPMISC_MINCCAPWR_EN) == 0) {
        return 0;
    }

    /* Applying individual chain values */
    if (eep->baseEepHeader.rxMask & 0x1) {

        value = ar5416EepAr9287MinCCAPwrThresGet(ah, 0, channel);
        OS_REG_RMW_FIELD(ah, AR_PHY_CCA_CTRL_0, AR_PHY_EXT_CCA0_THRESH62_1, value);

    }
    if (eep->baseEepHeader.rxMask & 0x2) {

        value = ar5416EepAr9287MinCCAPwrThresGet(ah, 1, channel);
        OS_REG_RMW_FIELD(ah, AR_PHY_CCA_CTRL_1, AR_PHY_EXT_CCA0_THRESH62_1, value);

    }

    OS_REG_RMW_FIELD(ah, AR_PHY_CCA_CTRL_0, AR_PHY_EXT_CCA0_THRESH62_ENABLE, 1);

    return 0;
}

/**
 * - Function Name: ar5416EepromAr9287SetBoardValues
 * - Description  : load eeprom header info into the device
 * - Arguments
 *     -      
 * - Returns      : 
 ******************************************************************************/
HAL_BOOL 
ar9287EepromSetBoardValues(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{

	MODAL_EEPAR9287_HEADER *pModal;
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar9287_eeprom_t *eep = &ahp->ah_eeprom.map.mapAr9287;

    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);    
	u_int32_t ant_config;
	u_int32_t regChainOffset;
	u_int8_t    txRxAttenLocal;
    int i;
    u_int32_t regVal;
    u_int32_t greenTxOBPowerOffset = 0x02020202;

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

	HALASSERT(owl_get_eepAr9287_ver(ahp) == AR9287_EEP_VER);

    pModal = &eep->modalHeader;

    if (HAL_OK == ar5416EepromGetAntCfg(ahp, chan, ahpriv->ah_config.ath_hal_defaultAntCfg, &ant_config)) {
        OS_REG_WRITE(ah, AR_PHY_SWITCH_COM, ant_config);
    }


    for (i = 0; i < AR9287_MAX_CHAINS; i++)
    {
 
		regChainOffset = i * 0x1000;
 
        OS_REG_WRITE(ah, AR_PHY_SWITCH_CHAIN_0 + regChainOffset, pModal->antCtrlChain[i]);

        OS_REG_WRITE(ah, AR_PHY_TIMING_CTRL4(0) + regChainOffset, 
            (OS_REG_READ(ah, AR_PHY_TIMING_CTRL4(0) + regChainOffset) &
            ~(AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF | AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF)) |
            SM(pModal->iqCalICh[i], AR_PHY_TIMING_CTRL4_IQCORR_Q_I_COFF) |
            SM(pModal->iqCalQCh[i], AR_PHY_TIMING_CTRL4_IQCORR_Q_Q_COFF));

		/*
        * Large signal upgrade,
        * If 14.3 or later EEPROM, use txRxAttenLocal = pModal->txRxAttenCh[i]
        * else txRxAttenLocal is fixed value above.
        */
        txRxAttenLocal = pModal->txRxAttenCh[i];

        /* 
        * From Merlin(AR9280),
        *     bswAtten[] maps to chain specific xatten1_db(0xa20c/0xb20c 5:0)
        *     bswMargin[] maps to chain specific xatten1_margin (0xa20c/0xb20c 16:12)
        */
        OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + regChainOffset, AR_PHY_GAIN_2GHZ_XATTEN1_MARGIN, pModal->bswMargin[i]);
        OS_REG_RMW_FIELD(ah, AR_PHY_GAIN_2GHZ + regChainOffset, AR_PHY_GAIN_2GHZ_XATTEN1_DB, pModal->bswAtten[i]);

		/* 
        * From Merlin(AR9280),
        *     txRxAttenCh[] maps to chain specific xatten1_hyst_margin(0x9848/0xa848 13:7)
        *     rxTxMarginCh[] maps to chain specific xatten2_hyst_margin (0x9848/0xa848 20:14)
        */
        OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN + regChainOffset, AR9280_PHY_RXGAIN_TXRX_ATTEN, txRxAttenLocal);
        OS_REG_RMW_FIELD(ah, AR_PHY_RXGAIN + regChainOffset, AR9280_PHY_RXGAIN_TXRX_MARGIN, pModal->rxTxMarginCh[i]);
    }

	ar5416EepAr9287MinCCAPwrThresApply(ah, chan->channel);

    if (IS_CHAN_HT40(chan)) {
        OS_REG_RMW_FIELD(ah, AR_PHY_SETTLING, AR_PHY_SETTLING_SWITCH, pModal->swSettleHt40);
    }
    else {
        OS_REG_RMW_FIELD(ah, AR_PHY_SETTLING, AR_PHY_SETTLING_SWITCH, pModal->switchSettling);
    }

    OS_REG_RMW_FIELD(ah, AR_PHY_DESIRED_SZ, AR_PHY_DESIRED_SZ_ADC, pModal->adcDesiredSize);

    OS_REG_WRITE(ah, AR_PHY_RF_CTL4,
        SM(pModal->txEndToXpaOff, AR_PHY_RF_CTL4_TX_END_XPAA_OFF)
        | SM(pModal->txEndToXpaOff, AR_PHY_RF_CTL4_TX_END_XPAB_OFF)
        | SM(pModal->txFrameToXpaOn, AR_PHY_RF_CTL4_FRAME_XPAA_ON)
        | SM(pModal->txFrameToXpaOn, AR_PHY_RF_CTL4_FRAME_XPAB_ON));

    OS_REG_RMW_FIELD(ah, AR_PHY_RF_CTL3, AR_PHY_TX_END_TO_A2_RX_ON, pModal->txEndToRxOn);

	OS_REG_RMW_FIELD(ah, AR_PHY_CCA, AR9280_PHY_CCA_THRESH62, pModal->thresh62);
    OS_REG_RMW_FIELD(ah, AR_PHY_EXT_CCA0, AR_PHY_EXT_CCA0_THRESH62, pModal->thresh62);

    regVal = OS_REG_READ(ah, AR9287_AN_RF2G3_CH0);
    regVal &= ~(AR9287_AN_RF2G3_DB1 | AR9287_AN_RF2G3_DB2 |
                AR9287_AN_RF2G3_OB_CCK | AR9287_AN_RF2G3_OB_PSK |
                AR9287_AN_RF2G3_OB_QAM | AR9287_AN_RF2G3_OB_PAL_OFF);
    regVal |= (SM(pModal->db1, AR9287_AN_RF2G3_DB1) |
               SM(pModal->db2, AR9287_AN_RF2G3_DB2) |
               SM(pModal->ob_cck, AR9287_AN_RF2G3_OB_CCK) |
               SM(pModal->ob_psk, AR9287_AN_RF2G3_OB_PSK) |
               SM(pModal->ob_qam, AR9287_AN_RF2G3_OB_QAM) |
               SM(pModal->ob_pal_off, AR9287_AN_RF2G3_OB_PAL_OFF));
    analogShiftRegWrite(ah, AR9287_AN_RF2G3_CH0, regVal);

    regVal = OS_REG_READ(ah, AR9287_AN_RF2G3_CH1);
    regVal &= ~(AR9287_AN_RF2G3_DB1 | AR9287_AN_RF2G3_DB2 |
               AR9287_AN_RF2G3_OB_CCK | AR9287_AN_RF2G3_OB_PSK |
               AR9287_AN_RF2G3_OB_QAM | AR9287_AN_RF2G3_OB_PAL_OFF);
    regVal |= (SM(pModal->db1, AR9287_AN_RF2G3_DB1) |
               SM(pModal->db2, AR9287_AN_RF2G3_DB2) |
               SM(pModal->ob_cck, AR9287_AN_RF2G3_OB_CCK) |
               SM(pModal->ob_psk, AR9287_AN_RF2G3_OB_PSK) |
               SM(pModal->ob_qam, AR9287_AN_RF2G3_OB_QAM) |
               SM(pModal->ob_pal_off, AR9287_AN_RF2G3_OB_PAL_OFF));
    analogShiftRegWrite(ah, AR9287_AN_RF2G3_CH1, regVal);

    OS_REG_RMW_FIELD(ah, AR_PHY_RF_CTL2,  AR_PHY_TX_END_DATA_START, pModal->txFrameToDataStart);
	OS_REG_RMW_FIELD(ah, AR_PHY_RF_CTL2,  AR_PHY_TX_END_PA_ON, pModal->txFrameToPaOn);

    analogShiftRegRMW(ah, AR9287_AN_TOP2, AR9287_AN_TOP2_XPABIAS_LVL, AR9287_AN_TOP2_XPABIAS_LVL_S, pModal->xpaBiasLvl);


    /* Green Tx */
    if (ahpriv->ah_opmode == HAL_M_STA && ahpriv->ah_config.ath_hal_sta_update_tx_pwr_enable) {
        for (i = 0; i < 9; i++) {
            regList[i][1] = OS_REG_READ(ah, regList[i][0]);
        }

        /* Short : OB/DB1/DB2 = 3/3/1 */
        /* 0x02020202 : Power offset due to OB changing from 3 to 1 */
        AH5416(ah)->txPowerRecord[0][0] = regList[0][1] + greenTxOBPowerOffset;
        AH5416(ah)->txPowerRecord[0][1] = regList[1][1] + greenTxOBPowerOffset;
        AH5416(ah)->txPowerRecord[0][2] = regList[2][1] + greenTxOBPowerOffset;
        AH5416(ah)->txPowerRecord[0][3] = regList[3][1] + greenTxOBPowerOffset;
        AH5416(ah)->txPowerRecord[0][4] = regList[4][1] + greenTxOBPowerOffset;
        AH5416(ah)->txPowerRecord[0][5] = regList[5][1] + greenTxOBPowerOffset;
        AH5416(ah)->txPowerRecord[0][6] = regList[6][1] + greenTxOBPowerOffset;
        AH5416(ah)->txPowerRecord[0][7] = regList[7][1] + greenTxOBPowerOffset;
        AH5416(ah)->txPowerRecord[0][8] = regList[8][1] + greenTxOBPowerOffset;
            
        /* Middle : OB/DB1/DB2 = 3/3/2 */
        AH5416(ah)->txPowerRecord[1][0] = regList[0][1];
        AH5416(ah)->txPowerRecord[1][1] = regList[1][1];
        AH5416(ah)->txPowerRecord[1][2] = regList[2][1];
        AH5416(ah)->txPowerRecord[1][3] = regList[3][1];
        AH5416(ah)->txPowerRecord[1][4] = regList[4][1];
        AH5416(ah)->txPowerRecord[1][5] = regList[5][1];
        AH5416(ah)->txPowerRecord[1][6] = regList[6][1];
        AH5416(ah)->txPowerRecord[1][7] = regList[7][1];
        AH5416(ah)->txPowerRecord[1][8] = regList[8][1];
       
        /* Long : OB/DB1/DB2 = 3/3/3 */
        AH5416(ah)->txPowerRecord[2][0] = regList[0][1];
        AH5416(ah)->txPowerRecord[2][1] = regList[1][1];
        AH5416(ah)->txPowerRecord[2][2] = regList[2][1];
        AH5416(ah)->txPowerRecord[2][3] = regList[3][1];
        AH5416(ah)->txPowerRecord[2][4] = regList[4][1];
        AH5416(ah)->txPowerRecord[2][5] = regList[5][1];
        AH5416(ah)->txPowerRecord[2][6] = regList[6][1];
        AH5416(ah)->txPowerRecord[2][7] = regList[7][1];
        AH5416(ah)->txPowerRecord[2][8] = regList[8][1];

        /* Record OB/DB1/DB2 */
        AH_PRIVATE(ah)->ah_ob_db1[0] = OS_REG_READ(ah, AR9287_AN_RF2G3_CH0);
        AH_PRIVATE(ah)->ah_ob_db1[1] = 0x6DB6400;
        AH_PRIVATE(ah)->ah_ob_db1[2] = 0x6DB6800;
    }

    return AH_TRUE;
}


/**
 * - Function Name: ar9287EepromOpenLoopPowerControlSetPdadcs
 * - Description  : PDADC vs Tx power table on open-loop power control mode
 * - Arguments
 *     -      
 * - Returns      : 
 ******************************************************************************/
static void
ar9287EepromOpenLoopPowerControlSetPdadcs(struct ath_hal *ah, int32_t txPower, u_int16_t chain)
{
    u_int32_t tmpVal;
	u_int32_t a;

	// To enable open-loop power control
	tmpVal = OS_REG_READ(ah, 0xa270); // Chain 0
	tmpVal = tmpVal & 0xFCFFFFFF;
	tmpVal = tmpVal | (0x3 << 24); //bb_error_est_mode
	OS_REG_WRITE(ah, 0xa270, tmpVal);

	tmpVal = OS_REG_READ(ah, 0xb270); // Chain 1
	tmpVal = tmpVal & 0xFCFFFFFF;
	tmpVal = tmpVal | (0x3 << 24); //bb_error_est_mode
	OS_REG_WRITE(ah, 0xb270, tmpVal);

    /* write the olpc ref power for each chain */
    if( chain == 0 ) { // Chain 0
        tmpVal = OS_REG_READ(ah, 0xa398); 
        tmpVal = tmpVal & 0xff00ffff;
        a = (txPower)&0xff; /* removed the signed part */
        tmpVal = tmpVal | (a << 16);
        OS_REG_WRITE(ah, 0xa398, tmpVal);
    }

    if( chain == 1 ) { // Chain 1
        tmpVal = OS_REG_READ(ah, 0xb398); 
        tmpVal = tmpVal & 0xff00ffff;
        a = (txPower)&0xff; /* removed the signed part */
        tmpVal = tmpVal | (a << 16);
        OS_REG_WRITE(ah, 0xb398, tmpVal);
    }
}


/**************************************************************
 * ar9287EepromGetMaxEdgePower
 *
 * Find the maximum conformance test limit for the given channel and CTL info
 */
static inline u_int16_t
ar9287EepromGetMaxEdgePower(u_int16_t freq, CAL_CTL_EDGES *pRdEdgesPower, HAL_BOOL is2GHz)
{
    u_int16_t twiceMaxEdgePower = AR9287_MAX_RATE_POWER;
    int      i;

    /* Get the edge power */
    for (i = 0; (i < AR9287_NUM_BAND_EDGES) && (pRdEdgesPower[i].bChannel != AR9287_BCHAN_UNUSED) ; i++)
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


/**************************************************************
 * ar9298EepromSetPowerPerRateTable
 *
 * Sets the transmit power in the baseband for the given
 * operating channel and mode.
 */
HAL_BOOL
ar9287EepromSetPowerPerRateTable(struct ath_hal *ah, ar9287_eeprom_t *pEepData,
    HAL_CHANNEL_INTERNAL *chan,
    int16_t *ratesArray, u_int16_t cfgCtl,
    u_int16_t AntennaReduction,
    u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit)
{
    /* Local defines to distinguish between extension and control CTL's */
#define EXT_ADDITIVE (0x8000)
#define CTL_11A_EXT (CTL_11A | EXT_ADDITIVE)
#define CTL_11G_EXT (CTL_11G | EXT_ADDITIVE)
#define CTL_11B_EXT (CTL_11B | EXT_ADDITIVE)
#define REDUCE_SCALED_POWER_BY_TWO_CHAIN     6  /* 10*log10(2)*2 */
#define REDUCE_SCALED_POWER_BY_THREE_CHAIN   10 /* 10*log10(3)*2 */
    u_int16_t twiceMaxEdgePower = AR5416_MAX_RATE_POWER;
    static const u_int16_t tpScaleReductionTable[5] = { 0, 3, 6, 9, AR5416_MAX_RATE_POWER };
    int i;
    int16_t  twiceLargestAntenna;
    CAL_CTL_DATA_AR9287 *rep;
    CAL_TARGET_POWER_LEG targetPowerOfdm = {0, {0, 0, 0, 0}}, targetPowerCck = {0, {0, 0, 0, 0}};
    CAL_TARGET_POWER_LEG targetPowerOfdmExt = {0, {0, 0, 0, 0}}, targetPowerCckExt = {0, {0, 0, 0, 0}};
    CAL_TARGET_POWER_HT  targetPowerHt20 = {0}, targetPowerHt40 = {0};
    int16_t scaledPower=0, minCtlPower, maxRegAllowedPower;
    #define SUB_NUM_CTL_MODES_AT_5G_40 2    /* excluding HT40, EXT-OFDM */
    #define SUB_NUM_CTL_MODES_AT_2G_40 3    /* excluding HT40, EXT-OFDM, EXT-CCK */
    u_int16_t ctlModesFor11g[] = {CTL_11B, CTL_11G, CTL_2GHT20, CTL_11B_EXT, CTL_11G_EXT, CTL_2GHT40};
    u_int16_t numCtlModes = 0, *pCtlMode=AH_NULL, ctlMode, freq;
    CHAN_CENTERS centers;
    int tx_chainmask;
    u_int16_t twiceMinEdgePower;
    struct ath_hal_5416 *ahp = AH5416(ah);

    tx_chainmask = ahp->ah_txchainmask;

    ar5416GetChannelCenters(ah, chan, &centers);

    /* Compute TxPower reduction due to Antenna Gain */
    twiceLargestAntenna = AH_MAX(pEepData->modalHeader.antennaGainCh[0],
        pEepData->modalHeader.antennaGainCh[1]);

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
            AR9287_NUM_2G_CCK_TARGET_POWERS, &targetPowerCck, 4, AH_FALSE);
        ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPower2G,
            AR9287_NUM_2G_20_TARGET_POWERS, &targetPowerOfdm, 4, AH_FALSE);
        ar5416GetTargetPowers(ah, chan, pEepData->calTargetPower2GHT20,
            AR9287_NUM_2G_20_TARGET_POWERS, &targetPowerHt20, 8, AH_FALSE);

        if (IS_CHAN_HT40(chan))
        {
            numCtlModes = N(ctlModesFor11g);    /* All 2G CTL's */
            ar5416GetTargetPowers(ah, chan, pEepData->calTargetPower2GHT40,
                AR9287_NUM_2G_40_TARGET_POWERS, &targetPowerHt40, 8, AH_TRUE);
            ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPowerCck,
                AR9287_NUM_2G_CCK_TARGET_POWERS, &targetPowerCckExt, 4, AH_TRUE);
            ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPower2G,
                AR9287_NUM_2G_20_TARGET_POWERS, &targetPowerOfdmExt, 4, AH_TRUE);
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
        for (i = 0; (i < AR9287_NUM_CTLS) && pEepData->ctlIndex[i]; i++)
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
                
                twiceMinEdgePower = ar9287EepromGetMaxEdgePower(freq,
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
 * ar9287EepromGetTxGainIndex
 * - Description  : When the calibrated using the open loop power 
 *                  control scheme, this routine retrieves the Tx gain table
 *                  index for the pcdac that was used to calibrate the board.
 * - Arguments
 *     -      
 * - Returns      : 
 */
static inline void
ar9287EepromGetTxGainIndex(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan, CAL_DATA_PER_FREQ_OP_LOOP_AR9287 *pRawDatasetOpLoop,
    u_int8_t *pCalChans,  u_int16_t availPiers,
    int8_t *pPwr)
{
    u_int16_t  idxL = 0, idxR = 0, numPiers; /* Pier indexes */
    HAL_BOOL match;
    CHAN_CENTERS centers;

    ar5416GetChannelCenters(ah, chan, &centers);

    /* Get the number of populated channel Piers */
    for (numPiers = 0; numPiers < availPiers; numPiers++) {
        if (pCalChans[numPiers] == AR9287_BCHAN_UNUSED) {
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
        *pPwr = (int8_t)pRawDatasetOpLoop[idxL].pwrPdg[0][0];
    } else {
        /* match not found. Get the average pwr of two closest channels */
        *pPwr = ((int8_t)pRawDatasetOpLoop[idxL].pwrPdg[0][0] + (int8_t)pRawDatasetOpLoop[idxR].pwrPdg[0][0])/2;
    }

    return;
}


/**************************************************************
 * ar9287EepromSetPowerCalTable
 *
 * Pull the PDADC piers from cal data and interpolate them across the given
 * points as well as from the nearest pier(s) to get a power detector
 * linear voltage to power level table.
 */
static inline HAL_BOOL
ar9287EepromSetPowerCalTable(struct ath_hal *ah, ar9287_eeprom_t *pEepData, HAL_CHANNEL_INTERNAL *chan, int16_t *pTxPowerIndexOffset)
{
    CAL_DATA_PER_FREQ_AR9287 *pRawDataset;
	CAL_DATA_PER_FREQ_OP_LOOP_AR9287 *pRawDatasetOpenLoop;
    u_int8_t  *pCalBChans = AH_NULL;
    u_int16_t pdGainOverlap_t2;
    u_int8_t  pdadcValues[AR9287_NUM_PDADC_VALUES] = {0};
    u_int16_t gainBoundaries[AR9287_PD_GAINS_IN_MASK] = {0};
    u_int16_t numPiers = 0, i, j;
    int16_t  tMinCalPower;
    u_int16_t numXpdGain, xpdMask;
    u_int16_t xpdGainValues[AR9287_NUM_PD_GAINS] = {0, 0, 0, 0};
    u_int32_t reg32, regOffset, regChainOffset;
    int16_t   diff=0;
    struct ath_hal_5416 *ahp = AH5416(ah);

#ifdef ATH_TX99_DIAG
    AH_PRIVATE(ah)->ah_pwr_offset = (int8_t)ar5416EepromGet(ahp, EEP_PWR_TABLE_OFFSET);
#endif

    xpdMask = pEepData->modalHeader.xpdGain;

    if ((pEepData->baseEepHeader.version & AR9287_EEP_VER_MINOR_MASK) >=
        AR9287_EEP_MINOR_VER_2) {
        pdGainOverlap_t2 = pEepData->modalHeader.pdGainOverlap;
    } else {
        pdGainOverlap_t2 = (u_int16_t)(MS(
            OS_REG_READ(ah, AR_PHY_TPCRG5), AR_PHY_TPCRG5_PD_GAIN_OVERLAP));
    }

    if (IS_CHAN_2GHZ(chan)) {
        pCalBChans = pEepData->calFreqPier2G;
        numPiers = AR9287_NUM_2G_CAL_PIERS;
        if (ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
            pRawDatasetOpenLoop =
                (CAL_DATA_PER_FREQ_OP_LOOP_AR9287*)pEepData->calPierData2G[0];
            ahp->initPDADC = pRawDatasetOpenLoop->vpdPdg[0][0];
        }
    } 
    else {
	return AH_FALSE;
    }

    numXpdGain = 0;
    /* Calculate the value of xpdgains from the xpdGain Mask */
    for (i = 1; i <= AR9287_PD_GAINS_IN_MASK; i++) {
        if ((xpdMask >> (AR9287_PD_GAINS_IN_MASK - i)) & 1) {
            if (numXpdGain >= AR9287_NUM_PD_GAINS) {
                HALASSERT(0);
                break;
            }
            xpdGainValues[numXpdGain] = (u_int16_t)(AR9287_PD_GAINS_IN_MASK-i);
            numXpdGain++;
        }
    }

    /* write the detector gain biases and their number */
    OS_REG_RMW_FIELD(
        ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_NUM_PD_GAIN, (numXpdGain - 1) & 0x3);
    OS_REG_RMW_FIELD(
        ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_GAIN_1, xpdGainValues[0]);
    OS_REG_RMW_FIELD(
        ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_GAIN_2, xpdGainValues[1]);
    OS_REG_RMW_FIELD(
        ah, AR_PHY_TPCRG1, AR_PHY_TPCRG1_PD_GAIN_3, xpdGainValues[2]);

    for (i = 0; i < AR9287_MAX_CHAINS; i++)
    {
        regChainOffset = i * 0x1000;
        if (pEepData->baseEepHeader.txMask & (1 << i))
        {
			pRawDatasetOpenLoop =
                (CAL_DATA_PER_FREQ_OP_LOOP_AR9287 *)pEepData->calPierData2G[i];
            if (ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
                int8_t txPower;
                ar9287EepromGetTxGainIndex(
                    ah, 
                    chan, 
                    pRawDatasetOpenLoop, 
                    pCalBChans, 
                    numPiers,
                    &txPower); 
                ar9287EepromOpenLoopPowerControlSetPdadcs(ah, txPower, i);
            } else {
				pRawDataset =
                    (CAL_DATA_PER_FREQ_AR9287 *)pEepData->calPierData2G[i];
                if (!ar9287EepromGetGainBoundariesAndPdadcs(
                         ah, chan, pRawDataset,
                        pCalBChans, numPiers,
                        pdGainOverlap_t2,
                        &tMinCalPower, gainBoundaries,
                        pdadcValues, numXpdGain)) {
                    HDPRINTF(ah, HAL_DBG_CHANNEL,
                             "%s:get pdadc failed due to numXpdGain value %d\n", __func__, numXpdGain);
                    return AH_FALSE;
                }
            }

            if (i == 0) {
                /*
                * Note the pdadc table may not start at 0 dBm power, could be
                * negative or greater than 0.  Need to offset the power
                * values by the amount of minPower for griffin
                */
                if (!ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
                    OS_REG_WRITE(ah, AR_PHY_TPCRG5 + regChainOffset,
                        SM(pdGainOverlap_t2, AR_PHY_TPCRG5_PD_GAIN_OVERLAP) |
                        SM(gainBoundaries[0],
                            AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_1)  |
                        SM(gainBoundaries[1],
                            AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_2)  |
                        SM(gainBoundaries[2],
                            AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_3)  |
                        SM(gainBoundaries[3],
                            AR_PHY_TPCRG5_PD_GAIN_BOUNDARY_4));
                }
            }

            if ((int32_t)AR9287_PWR_TABLE_OFFSET_DB !=
                pEepData->baseEepHeader.pwrTableOffset ) {
                /* get the difference in dB */
                diff = (u_int16_t)(pEepData->baseEepHeader.pwrTableOffset -
                       (int32_t)AR9287_PWR_TABLE_OFFSET_DB);
                /* get the number of half dB steps */
                diff *= 2; 

                /* shift the table to start at the new offset */
                for( j = 0; j<((u_int16_t)AR9287_NUM_PDADC_VALUES-diff); j++ ) {
                    pdadcValues[j] = pdadcValues[j+diff];
                }

                /* fill the back of the table */
                for (j = (u_int16_t)(AR9287_NUM_PDADC_VALUES-diff);
                     j < AR9287_NUM_PDADC_VALUES; j++ ) {
                    pdadcValues[j] = pdadcValues[AR9287_NUM_PDADC_VALUES-diff];
                }
            }
            if (!ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
				/*
				* Write the power values into the baseband power table
				*/
				regOffset = AR_PHY_BASE + (672 << 2) + regChainOffset;
				for (j = 0; j < 32; j++)
				{
					reg32 = ((pdadcValues[4*j + 0] & 0xFF) << 0)  |
						((pdadcValues[4*j + 1] & 0xFF) << 8)  |
						((pdadcValues[4*j + 2] & 0xFF) << 16) |
						((pdadcValues[4*j + 3] & 0xFF) << 24) ;
					OS_REG_WRITE(ah, regOffset, reg32);

					HDPRINTF(ah, HAL_DBG_PHY_IO,
                        "PDADC (%d,%4x): %4.4x %8.8x\n",
                        i, regChainOffset, regOffset, reg32);
					HDPRINTF(ah, HAL_DBG_PHY_IO,
                        "PDADC: Chain %d | PDADC %3d Value %3d | "
                        "PDADC %3d Value %3d | PDADC %3d Value %3d | "
                        "PDADC %3d Value %3d |\n",
						i,
						4*j, pdadcValues[4*j],
						4*j+1, pdadcValues[4*j + 1],
						4*j+2, pdadcValues[4*j + 2],
						4*j+3, pdadcValues[4*j + 3]);
					regOffset += 4;
				}
			}
        }
    }
    *pTxPowerIndexOffset = 0;

    return AH_TRUE;
}


/**************************************************************
 * ar9287EepromSetTransmitPower
 *
 */


HAL_STATUS
ar9287EepromSetTransmitPower(struct ath_hal *ah,
    ar9287_eeprom_t *pEepData, HAL_CHANNEL_INTERNAL *chan, u_int16_t cfgCtl,
    u_int16_t twiceAntennaReduction, u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit)
{
#define POW_SM(_r, _s)     (((_r) & 0x3f) << (_s))

#define INCREASE_MAXPOW_BY_TWO_CHAIN     6  /* 10*log10(2)*2 */
#define INCREASE_MAXPOW_BY_THREE_CHAIN   10 /* 10*log10(3)*2 */

    MODAL_EEPAR9287_HEADER *pModal = &pEepData->modalHeader;
    struct ath_hal_5416 *ahp = AH5416(ah);
    int16_t ratesArray[Ar5416RateSize];
    int16_t  txPowerIndexOffset = 0;
    u_int8_t ht40PowerIncForPdadc = 2;
    int i;

    HALASSERT(owl_get_eepAr9287_ver(ahp) == AR9287_EEP_VER);

    OS_MEMZERO(ratesArray, sizeof(ratesArray));
    if (ahp->ah_emu_eeprom) return HAL_OK;


    if ((pEepData->baseEepHeader.version & AR9287_EEP_VER_MINOR_MASK) >= AR9287_EEP_MINOR_VER_2)
    {
        ht40PowerIncForPdadc = pModal->ht40PowerIncForPdadc;
    }

	if (!ar9287EepromSetPowerPerRateTable(ah, pEepData, chan,
		&ratesArray[0],cfgCtl,
        twiceAntennaReduction,
        twiceMaxRegulatoryPower, powerLimit))
	{
		HDPRINTF(ah, HAL_DBG_EEPROM,
                "ar5416EepromSetTransmitPower: "
                "unable to set tx power per rate table\n");
        return HAL_EIO;
	}

    if (!ar9287EepromSetPowerCalTable(ah, pEepData, chan, &txPowerIndexOffset))
    {
        HDPRINTF(ah, HAL_DBG_EEPROM, "ar5416EepromAr9287SetTransmitPower: unable to set power table\n");
        return HAL_EIO;
    }

    /*
    * txPowerIndexOffset is set by the SetPowerTable() call -
    *  adjust the rate table (0 offset if rates EEPROM not loaded)
    */
    for (i = 0; i < N(ratesArray); i++)
    {
        ratesArray[i] = (int16_t)(txPowerIndexOffset + ratesArray[i]);
        if (ratesArray[i] > AR9287_MAX_RATE_POWER)
            ratesArray[i] = AR9287_MAX_RATE_POWER;
    }

    /*
     * For Merlin, the transmit powers in the power per rate registers
     * are relative to -5dBm.
     */
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        for (i = 0; i < Ar5416RateSize; i++) {
            ratesArray[i] -= AR9287_PWR_TABLE_OFFSET_DB * 2;
        }
    }

#ifdef EEPROM_DUMP
    ar9287EepromPrintPowerPerRate(ah, ratesArray);
#endif

    /* Write the OFDM power per rate set */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE1,
        POW_SM(ratesArray[rate18mb], 24)
        | POW_SM(ratesArray[rate12mb], 16)
        | POW_SM(ratesArray[rate9mb],  8)
        | POW_SM(ratesArray[rate6mb],  0)
        );

    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE2,
        POW_SM(ratesArray[rate54mb], 24)
        | POW_SM(ratesArray[rate48mb], 16)
        | POW_SM(ratesArray[rate36mb],  8)
        | POW_SM(ratesArray[rate24mb],  0)
        );

    if (IS_CHAN_2GHZ(chan))
    {
        /* Write the CCK power per rate set */
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE3,
            POW_SM(ratesArray[rate2s], 24)
            | POW_SM(ratesArray[rate2l],  16)
            | POW_SM(ratesArray[rateXr],  8) /* XR target power */
            | POW_SM(ratesArray[rate1l],   0)
            );
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE4,
            POW_SM(ratesArray[rate11s], 24)
            | POW_SM(ratesArray[rate11l], 16)
            | POW_SM(ratesArray[rate5_5s],  8)
            | POW_SM(ratesArray[rate5_5l],  0)
            );
    }

    /* Write the HT20 power per rate set */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE5,
        POW_SM(ratesArray[rateHt20_3], 24)
        | POW_SM(ratesArray[rateHt20_2],  16)
        | POW_SM(ratesArray[rateHt20_1],  8)
        | POW_SM(ratesArray[rateHt20_0],   0)
        );

    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE6,
        POW_SM(ratesArray[rateHt20_7], 24)
        | POW_SM(ratesArray[rateHt20_6],  16)
        | POW_SM(ratesArray[rateHt20_5],  8)
        | POW_SM(ratesArray[rateHt20_4],   0)
        );

    if (IS_CHAN_HT40(chan))
    {
        if (ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) {
        /* Write the HT40 power per rate set */
        /* Correct the difference between HT40 and HT20/Legacy */
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE7,
            POW_SM(ratesArray[rateHt40_3], 24)
            | POW_SM(ratesArray[rateHt40_2],  16)
            | POW_SM(ratesArray[rateHt40_1],  8)
            | POW_SM(ratesArray[rateHt40_0],   0)
            );

        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE8,
            POW_SM(ratesArray[rateHt40_7], 24)
            | POW_SM(ratesArray[rateHt40_6],  16)
            | POW_SM(ratesArray[rateHt40_5],  8)
            | POW_SM(ratesArray[rateHt40_4],   0)
            );
		} else {
        /* Correct the difference between HT40 and HT20/Legacy */
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE7,
            POW_SM(ratesArray[rateHt40_3] + ht40PowerIncForPdadc, 24)
            | POW_SM(ratesArray[rateHt40_2] + ht40PowerIncForPdadc,  16)
            | POW_SM(ratesArray[rateHt40_1] + ht40PowerIncForPdadc,  8)
            | POW_SM(ratesArray[rateHt40_0] + ht40PowerIncForPdadc,   0)
            );

        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE8,
            POW_SM(ratesArray[rateHt40_7] + ht40PowerIncForPdadc, 24)
            | POW_SM(ratesArray[rateHt40_6] + ht40PowerIncForPdadc,  16)
            | POW_SM(ratesArray[rateHt40_5] + ht40PowerIncForPdadc,  8)
            | POW_SM(ratesArray[rateHt40_4] + ht40PowerIncForPdadc,   0)
            );

		}

            /* Write the Dup/Ext 40 power per rate set */
            OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE9,
                POW_SM(ratesArray[rateExtOfdm], 24)
                | POW_SM(ratesArray[rateExtCck],  16)
                | POW_SM(ratesArray[rateDupOfdm],  8)
                | POW_SM(ratesArray[rateDupCck],   0)
                );
    }

    /*
     * Return tx power used to iwconfig.
     * Since power is rate dependent, use one of the indices from the
     * AR5416_RATES enum to select an entry from ratesArray[] to report.
     * Pick the lowest possible rate since it will have the highest possible
     * power.
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

    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        AH_PRIVATE(ah)->ah_max_power_level =
            ratesArray[i] + AR9287_PWR_TABLE_OFFSET_DB * 2;
    } else {
        AH_PRIVATE(ah)->ah_max_power_level = ratesArray[i];
    }

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

#ifdef EEPROM_DUMP
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "2xMaxPowerLevel: %d (%s)\n",
        AH_PRIVATE(ah)->ah_max_power_level,
        (i == rateHt40_0) ? "HT40" : (i == rateHt20_0) ? "HT20" : "LEG");
#endif

    return HAL_OK;
}

void
ar9287EepromSetAddac(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
    return;
}

u_int8_t
ar9287EepromGetNumAntConfig(struct ath_hal_5416 *ahp, HAL_FREQ_BAND freq_band)
{
    return 1; /* default antenna configuration */
}

HAL_STATUS
ar9287CheckEeprom(struct ath_hal *ah)
{
    u_int32_t sum = 0, el;
    u_int16_t *eepdata;
    int i;
    struct ath_hal_5416 *ahp = AH5416(ah);
    HAL_BOOL need_swap = AH_FALSE;
    ar9287_eeprom_t *eep = (ar9287_eeprom_t *)&ahp->ah_eeprom;

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
                     addr<sizeof(ar9287_eeprom_t)/sizeof(u_int16_t);
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
        el = SWAP16(ahp->ah_eeprom.map.mapAr9287.baseEepHeader.length);
    } else {
        el = ahp->ah_eeprom.map.mapAr9287.baseEepHeader.length;
    }

    eepdata = (u_int16_t *)(&ahp->ah_eeprom);
    for (i = 0; i < AH_MIN(el, sizeof(ar9287_eeprom_t))/sizeof(u_int16_t); i++)
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

        for (i = 0; i < AR9287_MAX_CHAINS; i++) {
            integer = SWAP32(eep->modalHeader.antCtrlChain[i]);
            eep->modalHeader.antCtrlChain[i] = integer;
        }

        for (i = 0; i < AR9287_EEPROM_MODAL_SPURS; i++) {
            word = SWAP16(eep->modalHeader.spurChans[i].spurChan);
            eep->modalHeader.spurChans[i].spurChan = word;
        }
    }

    /* Check CRC - Attach should fail on a bad checksum */
    if (sum != 0xffff || owl_get_eepAr9287_ver(ahp) != AR9287_EEP_VER ||
        owl_get_eep4k_rev(ahp) < AR5416_EEP_NO_BACK_VER) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "Bad EEPROM checksum 0x%x or revision 0x%04x\n",
            sum, owl_get_eepAr9287_rev(ahp));
        return HAL_EEBADSUM;
    }

    return HAL_OK;
}

#ifdef AH_PRIVATE_DIAG
void
ar9287FillEmuEeprom(struct ath_hal_5416 *ahp)
{
    ar5416_eeprom_t *eep = &ahp->ah_eeprom;

    eep->map.mapAr9287.baseEepHeader.version = AR9287_EEP_VER << 12;
    eep->map.mapAr9287.baseEepHeader.macAddr[0] = 0x00;
    eep->map.mapAr9287.baseEepHeader.macAddr[1] = 0x03;
    eep->map.mapAr9287.baseEepHeader.macAddr[2] = 0x7F;
    eep->map.mapAr9287.baseEepHeader.macAddr[3] = 0xBA;
    eep->map.mapAr9287.baseEepHeader.macAddr[4] = 0xD0;
    eep->map.mapAr9287.baseEepHeader.regDmn[0] = 0;
    eep->map.mapAr9287.baseEepHeader.opCapFlags = AR9287_OPFLAGS_11G;
    eep->map.mapAr9287.baseEepHeader.deviceCap = 0;
    eep->map.mapAr9287.baseEepHeader.rfSilent = 0;
    eep->map.mapAr9287.modalHeader.noiseFloorThreshCh[0] = -1;
}
#endif /* AH_PRIVATE_DIAG */

#ifdef EEPROM_DUMP
/**************************************************************
 * ar9287EepromDump
 *
 * Produce a formatted dump of the EEPROM structure
 */
static void 
ar9287EepromDump(struct ath_hal *ah, ar9287_eeprom_t *eep)
{
    u_int16_t           i, j, k, c, r;
    struct ath_hal_5416     *ahp = AH5416(ah);
    BASE_EEPAR9287_HEADER     *pBase = &(eep->baseEepHeader);
    MODAL_EEPAR9287_HEADER        *pModal =AH_NULL;
    CAL_TARGET_POWER_HT     *pPowerInfo = AH_NULL;
    CAL_TARGET_POWER_LEG    *pPowerInfoLeg = AH_NULL;
    HAL_BOOL            legacyPowers = AH_FALSE;
    CAL_DATA_PER_FREQ_U_AR9287       *pDataPerChannel;
    HAL_BOOL            is2Ghz = 0;
    u_int8_t            noMoreSpurs;
    u_int8_t            *pChannel;
    u_int16_t           *pSpurData;
    u_int16_t           channelCount, channelRowCnt, vpdCount, rowEndsAtChan;
    u_int16_t           xpdGainMapping[] = {0, 1, 2, 4};
    u_int16_t           xpdGainValues[AR9287_NUM_PD_GAINS], numXpdGain = 0, xpdMask;
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

    const static char *sTargetPowerMode[4] = {
      "2G CCK  ", "2G OFDM ", "2G HT20 ", "2G HT40 ",
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

    if (!eep) {
        return;
    }

    /* Print Header info */
    pBase = &(eep->baseEepHeader);
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
             (pBase->opCapFlags & AR9287_OPFLAGS_11A) || 0, (pBase->opCapFlags & AR9287_OPFLAGS_11G) || 0,
         (pBase->opCapFlags & AR5416_OPFLAGS_N_5G_HT40) || 0, (pBase->opCapFlags & AR5416_OPFLAGS_N_2G_HT40) || 0);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  OpFlags: Disable 5HT20 %d, Disable 2HT20 %d                  |\n",
             (pBase->opCapFlags & AR5416_OPFLAGS_N_5G_HT20) || 0, (pBase->opCapFlags & AR5416_OPFLAGS_N_2G_HT20) || 0);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Misc: Big Endian %d  | Wake On Wireless %d\n",      
             (pBase->eepMisc & AR9287_EEPMISC_BIG_ENDIAN) || 0, (pBase->eepMisc & AR9287_EEPMISC_WOW) || 0);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Cal Bin Maj Ver %3d Cal Bin Min Ver %3d Cal Bin Build %3d  |\n",
             (pBase->binBuildNumber >> 24) & 0xFF,
             (pBase->binBuildNumber >> 16) & 0xFF,
             (pBase->binBuildNumber >> 8) & 0xFF);
    if (IS_EEP_MINOR_V3(ahp)) {
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Device Type: %s                                  |\n",
                 sDeviceType[(pBase->deviceType & 0x7)]);
    }

    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Customer Data in hex                                       |\n");
    for (i = 0; i < AR9287_DATA_SZ; i++) {
        if ((i % 16) == 0) {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |= ");
        }
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "%02X ", eep->custData[i]);
        if ((i % 16) == 15) {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "          =|\n");
        }
    }

    /* Print Modal Header info */
	pModal = &eep->modalHeader;

	HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |======================2GHz Modal Header======================|\n");
	HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Ant Chain 0     0x%08X  |  Ant Chain 1     0x%08X  |\n",
			 pModal->antCtrlChain[0], pModal->antCtrlChain[1]);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Antenna Common  0x%08X  |  Antenna Gain Chain 0   %3d  |\n",
             pModal->antCtrlCommon, pModal->antennaGainCh[0]);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Antenna Gain Chain 1   %3d  |  Switch Settling        %3d  |\n",
             pModal->antennaGainCh[1], pModal->switchSettling);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  TxRxAttenuation Ch 0   %3d  |  TxRxAttenuation Ch 1   %3d  |\n",
             pModal->txRxAttenCh[0], pModal->txRxAttenCh[1]);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  RxTxMargin Chain 0     %3d  |  RxTxMargin Chain 1     %3d  |\n",
             pModal->rxTxMarginCh[0], pModal->rxTxMarginCh[1]);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  adc desired size       %3d  |  tx end to rx on        %3d  |\n",
             pModal->adcDesiredSize, pModal->txEndToRxOn);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  tx end to xpa off      %3d  |\n", pModal->txEndToXpaOff);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  tx frame to xpa on     %3d  |  thresh62               %3d  |\n",
             pModal->txFrameToXpaOn, pModal->thresh62);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  noise floor thres 0    %3d  |  noise floor thres 1    %3d  |\n",
             pModal->noiseFloorThreshCh[0], pModal->noiseFloorThreshCh[1]);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Xpd Gain Mask 0x%X          |                     Xpd %2d  |\n",
             pModal->xpdGain, pModal->xpd);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  IQ Cal I Chain 0       %3d  |  IQ Cal Q Chain 0       %3d  |\n",
             pModal->iqCalICh[0], pModal->iqCalQCh[0]);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  IQ Cal I Chain 1       %3d  |  IQ Cal Q Chain 1       %3d  |\n",
             pModal->iqCalICh[1], pModal->iqCalQCh[1]);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  pdGain Overlap     %2d.%d dB  |  Xpa bias level         %3d  |\n",
             pModal->pdGainOverlap / 2, (pModal->pdGainOverlap % 2) * 5, pModal->xpaBiasLvl);
	HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  txFrameToDataStart     %3d  |  txFrameToPaOn          %3d  |\n",
            pModal->txFrameToDataStart, pModal->txFrameToPaOn);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  ht40PowerIncForPdadc   %3d  \n", pModal->ht40PowerIncForPdadc);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  bswAtten Chain 0       %3d  |  bswAtten Chain 1       %3d  |\n",
			 pModal->bswAtten[0], pModal->bswAtten[1]);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  bswMargin Chain 0      %3d  |  bswMargin Chain 1      %3d  |\n",
			 pModal->bswMargin[0], pModal->bswMargin[1]);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Anlg Outv Bias (ob_cck)%3d  |  Anlg Out Bias (ob_psk)  %3d  |\n",
             pModal->ob_cck, pModal->ob_psk);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Anlg Outv Bias (ob_qam)%3d  |  Anlg Out Bias (pal_off)  %3d  |\n",
             pModal->ob_qam, pModal->ob_pal_off);
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |  Anlg Drv Bias (db1)    %3d  |  Anlg Drv Bias (db2)     %3d  |\n",
             pModal->db1, pModal->db2);


    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, " |=============================================================|\n");

#define SPUR_TO_KHZ(is2GHz, spur)    ( ((spur) + ((is2GHz) ? 23000 : 49000)) * 100 )
#define NO_SPUR                      ( 0x8000 )

    /* Print spur data */
    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "=======================Spur Information======================\n");

	HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| 11G Spurs in MHz                                          |\n");

    pSpurData = (u_int16_t *)eep->modalHeader.spurChans;
    noMoreSpurs = 0;
    for (j = 0; j < AR9287_EEPROM_MODAL_SPURS; j++) {
        if ((pModal->spurChans[j].spurChan == NO_SPUR) || noMoreSpurs) {
            noMoreSpurs = 1;
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|  NO SPUR  ");
        } else {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|   %4d.%1d  ", SPUR_TO_KHZ(i,pModal->spurChans[j].spurChan )/1000,
                     ((SPUR_TO_KHZ(i, pModal->spurChans[j].spurChan))/100) % 10);
        }
    }
	for (j = 0; j < AR9287_EEPROM_MODAL_SPURS; j++) {
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

    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|===========================================================|\n");

#undef SPUR_TO_KHZ
#undef NO_SPUR

    /* Print calibration info */
    for (c = 0; c < AR9287_MAX_CHAINS; c++) {
        if (!((pBase->txMask >> c) & 1)) {
            continue;
        }
        pDataPerChannel = (CAL_DATA_PER_FREQ_U_AR9287 *)eep->calPierData2G[c];
        numChannels = AR9287_NUM_2G_CAL_PIERS;
        pChannel = eep->calFreqPier2G;

        xpdMask = eep->modalHeader.xpdGain;

        numXpdGain = 0;
        /* Calculate the value of xpdgains from the xpdGain Mask */
        for (j = 1; j <= AR9287_PD_GAINS_IN_MASK; j++) {
            if ((xpdMask >> (AR9287_PD_GAINS_IN_MASK - j)) & 1) {
                if (numXpdGain >= AR9287_NUM_PD_GAINS) {
                    HALASSERT(0);
                    break;
                }
                xpdGainValues[numXpdGain++] = (u_int16_t)(AR9287_PD_GAINS_IN_MASK - j);
            }
        }

        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "===============Power Calibration Information Chain %d =======================\n", c);
        for (channelRowCnt = 0; channelRowCnt < numChannels; channelRowCnt += 5) {
            temp = channelRowCnt + 5;
            rowEndsAtChan = (u_int16_t)AH_MIN(numChannels, temp);
            for (channelCount = channelRowCnt; (channelCount < rowEndsAtChan)  && (pChannel[channelCount] != 0xff); channelCount++) {
                HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|     %04d     ", fbin2freq(pChannel[channelCount], AH_TRUE));
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

                for (vpdCount = 0; vpdCount < AR9287_PD_GAIN_ICEPTS; vpdCount++) {
                    for (channelCount = channelRowCnt; (channelCount < rowEndsAtChan)  && (pChannel[channelCount] != 0xff); channelCount++) {
                        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|  %d   %d  ", 
								pDataPerChannel[channelCount].calDataOpen.vpdPdg[k][vpdCount],
                                pDataPerChannel[channelCount].calDataOpen.pwrPdg[k][vpdCount]);
                    }
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
                }
            }
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|              |              |              |              |              |\n");
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|==============|==============|==============|==============|==============|\n");
        }
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
    }

    /* Print Target Powers */
    for (i = 0; i < 4; i++) {
        switch(i) {
        case 0:
            pPowerInfo = (CAL_TARGET_POWER_HT *)eep->calTargetPowerCck;
            pPowerInfoLeg = eep->calTargetPowerCck;
            numPowers = AR9287_NUM_2G_CCK_TARGET_POWERS;
            ratePrint = 1;
            numRates = 4;
            is2Ghz = AH_TRUE;
            legacyPowers = AH_TRUE;
            break;
        case 1:
            pPowerInfo = (CAL_TARGET_POWER_HT *)eep->calTargetPower2G;
            pPowerInfoLeg = eep->calTargetPower2G;
            numPowers = AR9287_NUM_2G_20_TARGET_POWERS;
            ratePrint = 0;
            numRates = 4;
            is2Ghz = AH_TRUE;
            legacyPowers = AH_TRUE;
           break;
        case 2:
            pPowerInfo = eep->calTargetPower2GHT20;
            numPowers = AR9287_NUM_2G_20_TARGET_POWERS;
            ratePrint = 2;
            numRates = 8;
            is2Ghz = AH_TRUE;
            legacyPowers = AH_FALSE;
            break;
        case 3:
            pPowerInfo = eep->calTargetPower2GHT40;
            numPowers = AR9287_NUM_2G_40_TARGET_POWERS;
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
    for (i = 0; (eep->ctlIndex[i] != 0) && (i < AR9287_NUM_CTLS); i++) {
        if (((eep->ctlIndex[i] & CTL_MODE_M) == CTL_11A) || 
            ((eep->ctlIndex[i] & CTL_MODE_M) ==  CTL_5GHT20) || 
            ((eep->ctlIndex[i] & CTL_MODE_M) ==  CTL_5GHT40)) { 
            is2Ghz = AH_FALSE;
        } else {
            is2Ghz = AH_TRUE;
        }
        
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|                                                                       |\n");
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| CTL: 0x%02x %s                                           |\n",
               eep->ctlIndex[i], 
         (eep->ctlIndex[i] & CTL_MODE_M) < (sizeof(sCtlType)/sizeof(char *)) ? sCtlType[eep->ctlIndex[i] & CTL_MODE_M]:"unknown mode");
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

        for (c = 0; c < numTxChains; c++) {
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "======================== Edges for Chain %d ==============================\n", c);
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| edge  ");
            for (j = 0; j < AR9287_NUM_BAND_EDGES; j++) {
                if (eep->ctlData[i].ctlEdges[c][j].bChannel == AR9287_BCHAN_UNUSED) {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|  --   ");
                } else {
                    tempFreq = fbin2freq(eep->ctlData[i].ctlEdges[c][j].bChannel, is2Ghz);
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| %04d  ", tempFreq);
                }
            }
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| power ");
            for (j = 0; j < AR9287_NUM_BAND_EDGES; j++) {
                if (eep->ctlData[i].ctlEdges[c][j].bChannel == AR9287_BCHAN_UNUSED) {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|  --   ");
                } else {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| %2d.%d  ", eep->ctlData[i].ctlEdges[c][j].tPower / 2,
                        (eep->ctlData[i].ctlEdges[c][j].tPower % 2) * 5);
                }
            }
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|=======|=======|=======|=======|=======|=======|=======|=======|=======|\n");

            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "| flag  ");
            for (j = 0; j < AR9287_NUM_BAND_EDGES; j++) {
                if (eep->ctlData[i].ctlEdges[c][j].bChannel == AR9287_BCHAN_UNUSED) {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|  --   ");
                } else {
                    HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|   %1d   ", eep->ctlData[i].ctlEdges[c][j].flag);
                }
            }
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "|\n");
            HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "=========================================================================\n");
        }
    }

}
#endif // EEPROM_DUMP


HAL_BOOL
ar9287FillEeprom(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar9287_eeprom_t *eep = &ahp->ah_eeprom.map.mapAr9287;
    u_int16_t *eep_data;
    int addr, owl_eep_start_loc = AR9287_EEP_START_LOC;
#ifdef EEPROM_DUMP
    u_int16_t val;
#endif

    eep_data = (u_int16_t *)eep;

    for (addr = 0; addr < sizeof(ar9287_eeprom_t) / sizeof(u_int16_t); 
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
    for (addr = 0; addr < 128; addr++)
    {
        if (!ahp->ah_priv.ah_eeprom_read(ah, addr, &val))
        {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Unable to read eeprom region \n", __func__);
            return AH_FALSE;
        }
        HDPRINTF(ah, HAL_DBG_EEPROM_DUMP, "Offset 0x%x : 0x%x", addr, val);
    }

    ar9287EepromDump(ah, eep);
#endif
    return AH_TRUE;
}



/**************************************************************
 * ar9287EepromGetGainBoundariesAndPdadcs
 *
 * Uses the data points read from EEPROM to reconstruct the pdadc power table
 * Called by ar5416SetPowerCalTable only.
 */
static inline HAL_BOOL
ar9287EepromGetGainBoundariesAndPdadcs(struct ath_hal *ah,
    HAL_CHANNEL_INTERNAL *chan, CAL_DATA_PER_FREQ_AR9287 *pRawDataSet,
    u_int8_t * bChans,  u_int16_t availPiers,
    u_int16_t tPdGainOverlap, int16_t *pMinCalPower, u_int16_t * pPdGainBoundaries,
    u_int8_t * pPDADCValues, u_int16_t numXpdGains)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int       i, j, k;
    int16_t   ss;         /* potentially -ve index for taking care of pdGainOverlap */
    u_int16_t  idxL = 0, idxR = 0, numPiers; /* Pier indexes */
    u_int8_t   *pVpdL, *pVpdR, *pPwrL, *pPwrR;
    u_int8_t   minPwrT4[AR9287_NUM_PD_GAINS];
    u_int8_t   maxPwrT4[AR9287_NUM_PD_GAINS];
    int16_t   vpdStep;
    int16_t   tmpVal;
    u_int16_t  sizeCurrVpdTable, maxIndex, tgtIndex;
    HAL_BOOL    match;
    int16_t  minDelta = 0;
    CHAN_CENTERS centers;

    /*
     * If numXpdGains is not greater than 0, then the
     * arrays "minPwrT4" and "maxPwrT4" will not be
     * initialized properly.Hence, there is no meaning
     * in moving forward.
     */
    if (numXpdGains <= 0) {
        return AH_FALSE;
    }

    ar5416GetChannelCenters(ah, chan, &centers);

    /* Trim numPiers for the number of populated channel Piers */
    for (numPiers = 0; numPiers < availPiers; numPiers++)
    {
        if (bChans[numPiers] == AR9287_BCHAN_UNUSED)
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
			/* Modify for static analysis, sizeof pRawDataSet[idxL].pwrPdg[i][4] is only 1 */
            maxPwrT4[i] = pRawDataSet[idxL].pwrPdg[i][0];
            ar5416FillVpdTable(minPwrT4[i], maxPwrT4[i], pRawDataSet[idxL].pwrPdg[i],
                pRawDataSet[idxL].vpdPdg[i], AR9287_PD_GAIN_ICEPTS, ahp->ah_vpdTableI[i]);
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
            maxPwrT4[i] = AH_MIN(pPwrL[AR9287_PD_GAIN_ICEPTS - 1], pPwrR[AR9287_PD_GAIN_ICEPTS - 1]);
            HALASSERT(maxPwrT4[i] > minPwrT4[i]);

            /* Fill pier Vpds */
            ar5416FillVpdTable(minPwrT4[i], maxPwrT4[i], pPwrL, pVpdL, AR9287_PD_GAIN_ICEPTS, ahp->ah_vpdTableL[i]);
            ar5416FillVpdTable(minPwrT4[i], maxPwrT4[i], pPwrR, pVpdR, AR9287_PD_GAIN_ICEPTS, ahp->ah_vpdTableR[i]);

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
        while ((ss < 0) && (k < (AR9287_NUM_PDADC_VALUES - 1)))
        {
            tmpVal = (int16_t)(ahp->ah_vpdTableI[i][0] + ss * vpdStep);
            pPDADCValues[k++] = (u_int8_t)((tmpVal < 0) ? 0 : tmpVal);
            ss++;
        }

        sizeCurrVpdTable = (u_int8_t)((maxPwrT4[i] - minPwrT4[i]) / 2 + 1);
        tgtIndex = (u_int8_t)(pPdGainBoundaries[i] + tPdGainOverlap - (minPwrT4[i] / 2));
        maxIndex = (tgtIndex < sizeCurrVpdTable) ? tgtIndex : sizeCurrVpdTable;

        while ((ss < maxIndex) && (k < (AR9287_NUM_PDADC_VALUES - 1)))
        {
            pPDADCValues[k++] = ahp->ah_vpdTableI[i][ss++];
        }

        vpdStep = (int16_t)(ahp->ah_vpdTableI[i][sizeCurrVpdTable - 1] - ahp->ah_vpdTableI[i][sizeCurrVpdTable - 2]);
        vpdStep = (int16_t)((vpdStep < 1) ? 1 : vpdStep);
        /*
        * for last gain, pdGainBoundary == Pmax_t2, so will
        * have to extrapolate
        */
        if (tgtIndex > maxIndex)
        {  /* need to extrapolate above */
            while ((ss <= tgtIndex) && (k < (AR9287_NUM_PDADC_VALUES - 1)))
            {
                tmpVal = (int16_t)((ahp->ah_vpdTableI[i][sizeCurrVpdTable - 1] +
                    (ss - maxIndex + 1) * vpdStep));
                pPDADCValues[k++] = (u_int8_t)((tmpVal > 255) ? 255 : tmpVal);
                ss++;
            }
        }               /* extrapolated above */
    }                   /* for all pdGainUsed */

    /* Fill out pdGainBoundaries - only up to 2 allowed here, but hardware allows up to 4 */
    while (i < AR9287_PD_GAINS_IN_MASK)
    {
        pPdGainBoundaries[i] = pPdGainBoundaries[i-1];
        i++;
    }

    while (k < AR9287_NUM_PDADC_VALUES)
    {
        pPDADCValues[k] = pPDADCValues[k-1];
        k++;
    }

    return AH_TRUE;
}


#ifdef EEPROM_DUMP
static void
ar9287EepromPrintPowerPerRate(struct ath_hal *ah,int16_t pRatesPower[])
{
        const u_int8_t rateString[][16] = { " 6M OFDM", " 9M OFDM", "12M OFDM", "18M OFDM",
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
                HDPRINTF(ah,  HAL_DBG_POWER_OVERRIDE, " %s %2d.%1d dBm | %s %2d.%1d dBm | %s %2d.%1d dBm | %s %2d.%1d dBm\n",
                        rateString[i], pRatesPower[i] / 2, (pRatesPower[i] % 2) * 5,
                        rateString[i + 1], pRatesPower[i + 1] / 2, (pRatesPower[i + 1] % 2) * 5,
                        rateString[i + 2], pRatesPower[i + 2] / 2, (pRatesPower[i + 2] % 2) * 5,
                        rateString[i + 3], pRatesPower[i + 3] / 2, (pRatesPower[i + 3] % 2) * 5);
        }
}
#endif //EEPROM_DUMP

u_int16_t *ar9287RegulatoryDomainGet(struct ath_hal *ah)
{
    ar9287_eeprom_t *eep = &AH5416(ah)->ah_eeprom.map.mapAr9287;
	return eep->baseEepHeader.regDmn;
}

#ifdef ART_BUILD
/*added by eguo*/

/**************************************************************
 * ar9298EepromSetPowerPerRateTable
 *
 * Sets the transmit power in the baseband for the given
 * operating channel and mode.
 */
HAL_BOOL
ar9287EepromSetPowerPerRateTableNoCtl(struct ath_hal *ah, ar9287_eeprom_t *pEepData,
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
#define REDUCE_SCALED_POWER_BY_THREE_CHAIN   10 /* 10*log10(3)*2 */
    u_int16_t twiceMaxEdgePower = AR5416_MAX_RATE_POWER;
    static const u_int16_t tpScaleReductionTable[5] = { 0, 3, 6, 9, AR5416_MAX_RATE_POWER };
    int i;
    int16_t  twiceLargestAntenna;
    CAL_CTL_DATA_AR9287 *rep;
    CAL_TARGET_POWER_LEG targetPowerOfdm = {0, {0, 0, 0, 0}}, targetPowerCck = {0, {0, 0, 0, 0}};
    CAL_TARGET_POWER_LEG targetPowerOfdmExt = {0, {0, 0, 0, 0}}, targetPowerCckExt = {0, {0, 0, 0, 0}};
    CAL_TARGET_POWER_HT  targetPowerHt20, targetPowerHt40 = {0, {0, 0, 0, 0}};
    int16_t scaledPower=0, minCtlPower, maxRegAllowedPower;
    #define SUB_NUM_CTL_MODES_AT_5G_40 2    /* excluding HT40, EXT-OFDM */
    #define SUB_NUM_CTL_MODES_AT_2G_40 3    /* excluding HT40, EXT-OFDM, EXT-CCK */
    u_int16_t ctlModesFor11g[] = {CTL_11B, CTL_11G, CTL_2GHT20, CTL_11B_EXT, CTL_11G_EXT, CTL_2GHT40};
    u_int16_t numCtlModes = 0, *pCtlMode=AH_NULL, ctlMode, freq;
    CHAN_CENTERS centers;
    int tx_chainmask;
    u_int16_t twiceMinEdgePower;
    struct ath_hal_5416 *ahp = AH5416(ah);

    tx_chainmask = chainmask ? chainmask : ahp->ah_txchainmask;

    ar5416GetChannelCenters(ah, chan, &centers);

    /* Compute TxPower reduction due to Antenna Gain */
    twiceLargestAntenna = AH_MAX(pEepData->modalHeader.antennaGainCh[0],
        pEepData->modalHeader.antennaGainCh[1]);

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
            AR9287_NUM_2G_CCK_TARGET_POWERS, &targetPowerCck, 4, AH_FALSE);
        ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPower2G,
            AR9287_NUM_2G_20_TARGET_POWERS, &targetPowerOfdm, 4, AH_FALSE);
        ar5416GetTargetPowers(ah, chan, pEepData->calTargetPower2GHT20,
            AR9287_NUM_2G_20_TARGET_POWERS, &targetPowerHt20, 8, AH_FALSE);

        if (IS_CHAN_HT40(chan))
        {
            numCtlModes = N(ctlModesFor11g);    /* All 2G CTL's */
            ar5416GetTargetPowers(ah, chan, pEepData->calTargetPower2GHT40,
                AR9287_NUM_2G_40_TARGET_POWERS, &targetPowerHt40, 8, AH_TRUE);
            ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPowerCck,
                AR9287_NUM_2G_CCK_TARGET_POWERS, &targetPowerCckExt, 4, AH_TRUE);
            ar5416GetTargetPowersLeg(ah, chan, pEepData->calTargetPower2G,
                AR9287_NUM_2G_20_TARGET_POWERS, &targetPowerOfdmExt, 4, AH_TRUE);
        }
    }

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
 * ar9287EepromSetTransmitPower
 *
 */

HAL_STATUS
ar9287SetTargetPowerFromEeprom(struct ath_hal *ah,
    ar9287_eeprom_t *pEepData, HAL_CHANNEL_INTERNAL *chan, u_int16_t cfgCtl,
    u_int16_t twiceAntennaReduction, u_int16_t twiceMaxRegulatoryPower,
    u_int16_t powerLimit)
{
#define POW_SM(_r, _s)     (((_r) & 0x3f) << (_s))

#define INCREASE_MAXPOW_BY_TWO_CHAIN     6  /* 10*log10(2)*2 */
#define INCREASE_MAXPOW_BY_THREE_CHAIN   10 /* 10*log10(3)*2 */

    MODAL_EEPAR9287_HEADER *pModal = &pEepData->modalHeader;
    struct ath_hal_5416 *ahp = AH5416(ah);
    int16_t ratesArray[Ar5416RateSize];
    int16_t  txPowerIndexOffset = 0;
    u_int8_t ht40PowerIncForPdadc = 2;
    int i;

    HALASSERT(owl_get_eepAr9287_ver(ahp) == AR9287_EEP_VER);

    OS_MEMZERO(ratesArray, sizeof(ratesArray));


	if (!ar9287EepromSetPowerPerRateTableNoCtl(ah, pEepData, chan,
		&ratesArray[0],cfgCtl,
        twiceAntennaReduction,
        twiceMaxRegulatoryPower, powerLimit, 0))
	{
		HDPRINTF(ah, HAL_DBG_EEPROM,
                "ar5416EepromSetTransmitPower: "
                "unable to set tx power per rate table\n");
                return HAL_EIO;
        }

    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        for (i = 0; i < Ar5416RateSize; i++) {
            ratesArray[i] -= AR9287_PWR_TABLE_OFFSET_DB * 2;
        }
    }

    /* Write the OFDM power per rate set */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE1,
        POW_SM(ratesArray[rate18mb], 24)
        | POW_SM(ratesArray[rate12mb], 16)
        | POW_SM(ratesArray[rate9mb],  8)
        | POW_SM(ratesArray[rate6mb],  0)
        );

    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE2,
        POW_SM(ratesArray[rate54mb], 24)
        | POW_SM(ratesArray[rate48mb], 16)
        | POW_SM(ratesArray[rate36mb],  8)
        | POW_SM(ratesArray[rate24mb],  0)
        );

        /* Write the CCK power per rate set */
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE3,
            POW_SM(ratesArray[rate2s], 24)
            | POW_SM(ratesArray[rate2l],  16)
            | POW_SM(ratesArray[rateXr],  8) /* XR target power */
            | POW_SM(ratesArray[rate1l],   0)
            );
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE4,
            POW_SM(ratesArray[rate11s], 24)
            | POW_SM(ratesArray[rate11l], 16)
            | POW_SM(ratesArray[rate5_5s],  8)
            | POW_SM(ratesArray[rate5_5l],  0)
            );

    /* Write the HT20 power per rate set */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE5,
        POW_SM(ratesArray[rateHt20_3], 24)
        | POW_SM(ratesArray[rateHt20_2],  16)
        | POW_SM(ratesArray[rateHt20_1],  8)
        | POW_SM(ratesArray[rateHt20_0],   0)
        );

    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE6,
        POW_SM(ratesArray[rateHt20_7], 24)
        | POW_SM(ratesArray[rateHt20_6],  16)
        | POW_SM(ratesArray[rateHt20_5],  8)
        | POW_SM(ratesArray[rateHt20_4],   0)
        );

        /* Correct the difference between HT40 and HT20/Legacy */
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE7,
            POW_SM(ratesArray[rateHt40_3] + ht40PowerIncForPdadc, 24)
            | POW_SM(ratesArray[rateHt40_2] + ht40PowerIncForPdadc,  16)
            | POW_SM(ratesArray[rateHt40_1] + ht40PowerIncForPdadc,  8)
            | POW_SM(ratesArray[rateHt40_0] + ht40PowerIncForPdadc,   0)
            );

        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE8,
            POW_SM(ratesArray[rateHt40_7] + ht40PowerIncForPdadc, 24)
            | POW_SM(ratesArray[rateHt40_6] + ht40PowerIncForPdadc,  16)
            | POW_SM(ratesArray[rateHt40_5] + ht40PowerIncForPdadc,  8)
            | POW_SM(ratesArray[rateHt40_4] + ht40PowerIncForPdadc,   0)
            );


            /* Write the Dup/Ext 40 power per rate set */
            OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE9,
                POW_SM(ratesArray[rateExtOfdm], 24)
                | POW_SM(ratesArray[rateExtCck],  16)
                | POW_SM(ratesArray[rateDupOfdm],  8)
                | POW_SM(ratesArray[rateDupCck],   0)
                );

    return HAL_OK;
}


HAL_BOOL
ar9287WriteEeprom(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ar9287_eeprom_t *eep = &ahp->ah_eeprom.map.mapAr9287;
    u_int16_t *eep_data;
    int addr, owl_eep_start_loc = AR9287_EEP_START_LOC;	//0x80
	int i;
#if 0
    unsigned int eepAddr;
    unsigned int byteAddr;
    unsigned short svalue, word;
    int j;
#endif
    eep_data = (u_int16_t *)eep;

    for (addr = 0; addr < sizeof(ar9287_eeprom_t) / sizeof(u_int16_t); 
        addr++)
    {
#if 0
        eepAddr = (unsigned short)(owl_eep_start_loc+addr)/2;
       	byteAddr = (unsigned short) (owl_eep_start_loc+addr)%2;
        word = buffer[i]<<(byteAddr*8);
        svalue = svalue & ~(0xff<<(byteAddr*8));
        svalue = svalue | word;
		if(!ar9300EepromWrite(AH, eepAddr,  svalue ))// return 0 if eprom write is correct; ar9300EepromWrite returns 1 for success
			return 1; // bad write
#endif
		
			if (!ahp->ah_priv.priv.ah_eeprom_write(ah, addr + owl_eep_start_loc, 
				eep_data[addr]))
        {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Unable to read eeprom region \n", __func__);
            return AH_FALSE;
        }
    }

    return AH_TRUE;
}

HAL_BOOL
ar9287WriteCfgEeprom(struct ath_hal *ah,u_int16_t *data,u_int16_t size)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int addr;

    for (addr = 0; addr < size/2;addr++)
    {
        if (!ahp->ah_priv.priv.ah_eeprom_write(ah, addr,data[addr]))
        {
            HDPRINTF(ah, HAL_DBG_EEPROM, "%s: Unable to read eeprom region \n", __func__);
            return AH_FALSE;
        }
    }

    return AH_TRUE;
}
#endif /* ART_BUILD */

#undef N
#undef POW_SM

#endif /* AH_SUPPORT_EEPROM_AR9287 */
#endif /* AH_SUPPORT_AR5416 */
