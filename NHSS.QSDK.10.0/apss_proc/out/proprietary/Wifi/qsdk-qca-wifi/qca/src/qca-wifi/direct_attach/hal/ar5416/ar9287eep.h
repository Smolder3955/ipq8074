/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * Copyright (c) 2009, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 * - Filename    : ar9287eep.h
 * - Description : eeprom definitions for kiwi chipset 
 ******************************************************************************/
#ifndef _ATH_AR9287_EEP_H_
#define _ATH_AR9287_EEP_H_

/**** Includes ****************************************************************/

/**** Definitions *************************************************************/
#define AR9287_EEP_VER               0xE
#define AR9287_EEP_VER_MINOR_MASK    0xFFF
#define AR9287_EEP_MINOR_VER_1       0x1
#define AR9287_EEP_MINOR_VER_2       0x2
#define AR9287_EEP_MINOR_VER_3       0x3
#define AR9287_EEP_MINOR_VER         AR9287_EEP_MINOR_VER_3
#define AR9287_EEP_MINOR_VER_b       AR9287_EEP_MINOR_VER
#define AR9287_EEP_NO_BACK_VER       AR9287_EEP_MINOR_VER_1

// 16-bit offset location start of calibration struct
#define AR9287_EEP_START_LOC            128 
#define AR9287_NUM_2G_CAL_PIERS         3
#define AR9287_NUM_2G_CCK_TARGET_POWERS 3
#define AR9287_NUM_2G_20_TARGET_POWERS  3
#define AR9287_NUM_2G_40_TARGET_POWERS  3
#define AR9287_NUM_CTLS              12 /* max number of CTL groups to store in the eeprom */
#define AR9287_NUM_BAND_EDGES        4  /* max number of band edges per CTL group */
#define AR9287_CTL_MODE_M				0xF
#define AR9287_NUM_PD_GAINS             4
#define AR9287_PD_GAINS_IN_MASK         4
#define AR9287_PD_GAIN_ICEPTS           1
#define AR9287_EEPROM_MODAL_SPURS       5
#define AR9287_MAX_RATE_POWER           63
#define AR9287_NUM_PDADC_VALUES         128
#define AR9287_NUM_RATES                16
#define AR9287_BCHAN_UNUSED             0xFF
#define AR9287_MAX_PWR_RANGE_IN_HALF_DB 64
#define AR9287_OPFLAGS_11A              0x01
#define AR9287_OPFLAGS_11G              0x02
#define AR9287_OPFLAGS_2G_HT40          0x08
#define AR9287_OPFLAGS_2G_HT20          0x20
#define AR9287_OPFLAGS_5G_HT40          0x04
#define AR9287_OPFLAGS_5G_HT20          0x10
#define AR9287_EEPMISC_BIG_ENDIAN       0x01
#define AR9287_EEPMISC_MINCCAPWR_EN     0x10
#define AR9287_EEPMISC_WOW              0x02
#define AR9287_MAX_CHAINS               2
#define AR9287_ANT_16S                  32
#define AR9287_custdatasize             20

#define AR9287_NUM_ANT_CHAIN_FIELDS     6
#define AR9287_NUM_ANT_COMMON_FIELDS    4
#define AR9287_SIZE_ANT_CHAIN_FIELD     2
#define AR9287_SIZE_ANT_COMMON_FIELD    4
#define AR9287_ANT_CHAIN_MASK           0x3
#define AR9287_ANT_COMMON_MASK          0xf
#define AR9287_CHAIN_0_IDX              0
#define AR9287_CHAIN_1_IDX              1
#define AR9287_DATA_SZ                  32 /* customer data size */

#define AR9287_FLASH_SIZE 16*1024  // byte addressable

//delta from which to start power to pdadc table
#define AR9287_PWR_TABLE_OFFSET_DB  -5

#define AR9287_CHECKSUM_LOCATION (AR9287_EEP_START_LOC + 1)

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif

typedef struct BaseEepAr9287Header {
    u_int16_t  length;
    u_int16_t  checksum;
    u_int16_t  version;
    u_int8_t opCapFlags;
    u_int8_t   eepMisc;	
    u_int16_t  regDmn[2];
    u_int8_t   macAddr[6];
    u_int8_t   rxMask;  
    u_int8_t   txMask;
    u_int16_t  rfSilent; 
    u_int16_t  blueToothOptions;
    u_int16_t  deviceCap;
    u_int32_t  binBuildNumber;
    u_int8_t   deviceType;
    u_int8_t   openLoopPwrCntl; //#bits used: 1, bit0: 1: use open loop power control, 0: use closed loop power control
    int8_t    pwrTableOffset;  // offset in dB to be added to beginning of pdadc table in calibration 
    int8_t     tempSensSlope;  // temperature sensor slope for temp compensation
    int8_t     tempSensSlopePalOn; // ditto, but when PAL is ON
    u_int8_t   futureBase[29];
} __packed BASE_EEPAR9287_HEADER;  /* 64 bytes */

typedef struct ModalEepAr9287Header {
    u_int32_t  antCtrlChain[AR9287_MAX_CHAINS];       // 8
    u_int32_t  antCtrlCommon;                         // 4
    int8_t    antennaGainCh[AR9287_MAX_CHAINS];      // 2
    u_int8_t   switchSettling;                        // 1
    u_int8_t   txRxAttenCh[AR9287_MAX_CHAINS];        // 2  //xatten1_hyst_margin for merlin (0x9848/0xa848 13:7)
    u_int8_t   rxTxMarginCh[AR9287_MAX_CHAINS];       // 2  //xatten2_hyst_margin for merlin (0x9848/0xa848 20:14)
    int8_t    adcDesiredSize;                        // 1
    u_int8_t   txEndToXpaOff;                         // 1
    u_int8_t   txEndToRxOn;                           // 1
    u_int8_t   txFrameToXpaOn;                        // 1
    u_int8_t   thresh62;                              // 1
    int8_t    noiseFloorThreshCh[AR9287_MAX_CHAINS]; // 2
    u_int8_t   xpdGain;                               // 1
    u_int8_t   xpd;                                   // 1
    int8_t    iqCalICh[AR9287_MAX_CHAINS];           // 2
    int8_t    iqCalQCh[AR9287_MAX_CHAINS];           // 2
    u_int8_t   pdGainOverlap;                         // 1
    u_int8_t   xpaBiasLvl;                            // 1
    u_int8_t   txFrameToDataStart;                    // 1
    u_int8_t   txFrameToPaOn;                         // 1   --> 30 B
    u_int8_t   ht40PowerIncForPdadc;                  // 1
    u_int8_t   bswAtten[AR9287_MAX_CHAINS];           // 2  //xatten1_db for merlin (0xa20c/b20c 5:0)
    u_int8_t   bswMargin[AR9287_MAX_CHAINS];          // 2  //xatten1_margin for merlin (0xa20c/b20c 16:12
    u_int8_t   swSettleHt40;                          // 1
	u_int8_t   version;								 // 1
    u_int8_t   db1;                                   // 1
    u_int8_t   db2;                                   // 1
    u_int8_t   ob_cck;                                // 1
    u_int8_t   ob_psk;                                // 1
    u_int8_t   ob_qam;                                // 1
    u_int8_t   ob_pal_off;                            // 1
    u_int8_t   futureModal[30];                       // 30
    SPUR_CHAN spurChans[AR9287_EEPROM_MODAL_SPURS];  // 20 B       
} __packed MODAL_EEPAR9287_HEADER;             // == 99B


typedef struct calDataPerFreqOpLoopAr9287 {
    u_int8_t pwrPdg[2][5]; /* power measurement                  */
    u_int8_t vpdPdg[2][5]; /* pdadc voltage at power measurement */
    u_int8_t pcdac[2][5];  /* pcdac used for power measurement   */
    u_int8_t empty[2][5];  /* future use */
} __packed CAL_DATA_PER_FREQ_OP_LOOP_AR9287;


typedef struct calDataPerFreqAr9287 {
    u_int8_t pwrPdg[AR9287_NUM_PD_GAINS][AR9287_PD_GAIN_ICEPTS];
    u_int8_t vpdPdg[AR9287_NUM_PD_GAINS][AR9287_PD_GAIN_ICEPTS];
} __packed CAL_DATA_PER_FREQ_AR9287;

typedef union calDataPerFreqAr9287_u {
    struct calDataPerFreqOpLoopAr9287 calDataOpen;
    struct calDataPerFreqAr9287	calDataClose;
} __packed CAL_DATA_PER_FREQ_U_AR9287; 

typedef struct CalCtlDataAr9287 {
    CAL_CTL_EDGES  ctlEdges[AR9287_MAX_CHAINS][AR9287_NUM_BAND_EDGES];
} __packed CAL_CTL_DATA_AR9287;

typedef struct eepMapAr9287 {
    BASE_EEPAR9287_HEADER  baseEepHeader;         // 32 B
    u_int8_t                 custData[AR9287_DATA_SZ];          // 30 B
    MODAL_EEPAR9287_HEADER modalHeader;        // 99 B           
    u_int8_t                 calFreqPier2G[AR9287_NUM_2G_CAL_PIERS];  // 3 B
    CAL_DATA_PER_FREQ_U_AR9287     calPierData2G[AR9287_MAX_CHAINS][AR9287_NUM_2G_CAL_PIERS]; // 6x40 = 240
    CAL_TARGET_POWER_LEG    calTargetPowerCck[AR9287_NUM_2G_CCK_TARGET_POWERS];   // 3x5 = 15 B
    CAL_TARGET_POWER_LEG    calTargetPower2G[AR9287_NUM_2G_20_TARGET_POWERS];     // 3x5 = 15 B
    CAL_TARGET_POWER_HT     calTargetPower2GHT20[AR9287_NUM_2G_20_TARGET_POWERS]; // 3x9 = 27 B
    CAL_TARGET_POWER_HT     calTargetPower2GHT40[AR9287_NUM_2G_40_TARGET_POWERS]; // 3x9 = 27 B
    u_int8_t                 ctlIndex[AR9287_NUM_CTLS]; // 12 B
    CAL_CTL_DATA_AR9287     ctlData[AR9287_NUM_CTLS];  // 2x8x12 = 192 B
    u_int8_t                 padding; //1
} __packed ar9287_eeprom_t; 

/* EEPROM Ar9287 bit map definations */
#define owl_get_eepAr9287_ver(_ahp)   \
    (((_ahp)->ah_eeprom.map.mapAr9287.baseEepHeader.version >> 12) & 0xF)
#define owl_get_eepAr9287_rev(_ahp)   \
    (((_ahp)->ah_eeprom.map.mapAr9287.baseEepHeader.version) & 0xFFF)

#if defined(WIN32) || defined(WIN64)
#pragma pack (pop)
#endif

/**** Macros ******************************************************************/
//#define FREQ2FBIN(x,y) ((y) ? ((x) - 2300) : (((x) - 4800) / 5))

/**** Declarations and externals **********************************************/

/**** Public function prototypes **********************************************/
extern void ar5416_calibration_data_set(struct ath_hal *ah, int32_t source);
extern int32_t ar5416_calibration_data_get(struct ath_hal *ah);
extern void ar5416_calibration_data_address_set(struct ath_hal *ah, int32_t size);
extern int32_t ar5416_calibration_data_address_get(struct ath_hal *ah);

#endif  /* _ATH_AR9287_EEP_H_ */
