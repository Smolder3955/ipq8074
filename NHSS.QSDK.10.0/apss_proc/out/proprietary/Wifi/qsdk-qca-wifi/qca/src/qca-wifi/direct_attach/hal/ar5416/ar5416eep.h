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

#ifndef _ATH_AR5416_EEP_H_
#define _ATH_AR5416_EEP_H_

/*
 * Enum to indentify the eeprom mappings 
 */
typedef enum {
	EEP_MAP_DEFAULT = 0x0,
	EEP_MAP_4KBITS,
	EEP_MAP_AR9287
}HAL_5416_EEP_MAP;
/*
** SWAP Functions
** used to read EEPROM data, which is apparently stored in little
** endian form.  We have included both forms of the swap functions,
** one for big endian and one for little endian.  The indices of the
** array elements are the differences
*/
#if AH_BYTE_ORDER == AH_BIG_ENDIAN

#define AR5416_EEPROM_MAGIC         0x5aa5
#define SWAP16(_x) ( (u_int16_t)( (((const u_int8_t *)(&_x))[0] ) |\
                     ( ( (const u_int8_t *)( &_x ) )[1]<< 8) ) )

#define SWAP32(_x) ((u_int32_t)(                       \
                    (((const u_int8_t *)(&_x))[0]) |        \
                    (((const u_int8_t *)(&_x))[1]<< 8) |    \
                    (((const u_int8_t *)(&_x))[2]<<16) |    \
                    (((const u_int8_t *)(&_x))[3]<<24)))

#else // AH_BYTE_ORDER

#define AR5416_EEPROM_MAGIC         0xa55a
#define    SWAP16(_x) ( (u_int16_t)( (((const u_int8_t *)(&_x))[1] ) |\
                        ( ( (const u_int8_t *)( &_x ) )[0]<< 8) ) )

#define SWAP32(_x) ((u_int32_t)(                       \
                    (((const u_int8_t *)(&_x))[3]) |        \
                    (((const u_int8_t *)(&_x))[2]<< 8) |    \
                    (((const u_int8_t *)(&_x))[1]<<16) |    \
                    (((const u_int8_t *)(&_x))[0]<<24)))

#endif // AH_BYTE_ORDER

#define AR5416_EEPROM_MAGIC_OFFSET  0x0
/* reg_off = 4 * (eep_off) */
#define AR5416_EEPROM_S             2
#define AR5416_EEPROM_OFFSET        0x2000
#ifdef AR9100
#define AR5416_EEPROM_START_ADDR    0x1fff1000
#else
  #ifdef AH_CAL_LOCATION
  #define AR5416_EEPROM_START_ADDR    AH_CAL_LOCATION
  #else
  #define AR5416_EEPROM_START_ADDR    0x503f1200
  #endif
#endif
#define AR5416_EEPROM_MAX           0xae0
#define IS_EEP_MINOR_V3(_ahp) (ar5416EepromGet((_ahp), EEP_MINOR_REV)  >= AR5416_EEP_MINOR_VER_3)

#define owl_get_ntxchains(_txchainmask) \
    (((_txchainmask >> 2) & 1) + ((_txchainmask >> 1) & 1) + (_txchainmask & 1))

/* RF silent fields in \ */
#define EEP_RFSILENT_ENABLED        0x0001  /* bit 0: enabled/disabled */
#define EEP_RFSILENT_ENABLED_S      0       /* bit 0: enabled/disabled */
#define EEP_RFSILENT_POLARITY       0x0002  /* bit 1: polarity */
#define EEP_RFSILENT_POLARITY_S     1       /* bit 1: polarity */
#define EEP_RFSILENT_GPIO_SEL       0x001c  /* bits 2..4: gpio PIN */
#define EEP_RFSILENT_GPIO_SEL_S     2       /* bits 2..4: gpio PIN */

#define AR5416_EEP_NO_BACK_VER       0x1
#define AR5416_EEP_VER               0xE
#define AR5416_EEP_VER_MINOR_MASK    0x0FFF
#define AR5416_EEP_MINOR_VER_2       0x2
#define AR5416_EEP_MINOR_VER_3       0x3
#define AR5416_EEP_MINOR_VER_7       0x7
#define AR5416_EEP_MINOR_VER_9       0x9
#define AR5416_EEP_MINOR_VER_16      0x10
#define AR5416_EEP_MINOR_VER_17      0x11
#define AR5416_EEP_MINOR_VER_19      0x13
#define AR5416_EEP_MINOR_VER_20      0x14
#define AR5416_EEP_MINOR_VER_21      0x15
#define AR5416_EEP_MINOR_VER_22      0x16

#define AR5416_EEPROM_MODAL_SPURS    5
#define AR5416_MAX_RATE_POWER        63
#define AR5416_NUM_PDADC_VALUES      128
#define AR5416_NUM_RATES             16
#define AR5416_BCHAN_UNUSED          0xFF
#define AR5416_MAX_PWR_RANGE_IN_HALF_DB 64
#define AR5416_OPFLAGS_11A           0x01
#define AR5416_OPFLAGS_11G           0x02
#define AR5416_OPFLAGS_5G_HT40       0x04
#define AR5416_OPFLAGS_2G_HT40       0x08
#define AR5416_OPFLAGS_5G_HT20       0x10
#define AR5416_OPFLAGS_2G_HT20       0x20
#define AR5416_EEPMISC_BIG_ENDIAN    0x01
#define AR5416_EEPMISC_MINCCAPWR_EN  0x10
#define FREQ2FBIN(x,y) ((y) ? ((x) - 2300) : (((x) - 4800) / 5))
#define AR5416_ANT_16S               25
/* Delta from which to start power to pdadc table */
#define AR5416_PWR_TABLE_OFFSET_DB  -5
#define AR5416_LEGACY_CHAINMASK     1
#define AR5416_1_CHAINMASK          1
#define AR5416_2LOHI_CHAINMASK      5
#define AR5416_2LOMID_CHAINMASK     3
#define AR5416_3_CHAINMASK          7

/* Rx gain type values */
#define AR5416_EEP_RXGAIN_23dB_BACKOFF     0
#define AR5416_EEP_RXGAIN_13dB_BACKOFF     1
#define AR5416_EEP_RXGAIN_ORIG             2

/* Tx gain type values */
#define AR5416_EEP_TXGAIN_ORIGINAL         0
#define AR5416_EEP_TXGAIN_HIGH_POWER       1

#ifdef WIN32
#pragma pack(push, ar5416, 1)
#endif

typedef enum {
    EEP_NFTHRESH_5,
    EEP_NFTHRESH_2,
    EEP_MAC_MSW,
    EEP_MAC_MID,
    EEP_MAC_LSW,
    EEP_REG_0,
    EEP_REG_1,
    EEP_OP_CAP,
    EEP_OP_MODE,
    EEP_RF_SILENT,
    EEP_OB_5,
    EEP_DB_5,
    EEP_OB_2,
    EEP_DB_2,
    EEP_MINOR_REV,
    EEP_TX_MASK,
    EEP_RX_MASK,
    EEP_FSTCLK_5G,
    EEP_RXGAIN_TYPE,
    EEP_OL_PWRCTRL,
    EEP_TXGAIN_TYPE,
    EEP_RC_CHAIN_MASK,
    EEP_DAC_HPWR_5G,
    EEP_FRAC_N_5G,
    EEP_DEV_TYPE,
    EEP_TEMPSENSE_SLOPE,
    EEP_TEMPSENSE_SLOPE_PAL_ON,
    EEP_PWR_TABLE_OFFSET,
    EEP_MODAL_VER,
    EEP_ANT_DIV_CTL1,
    EEP_ANT_DIV_CTL2,
    EEP_SMART_ANTENNA,
} EEPROM_PARAM;

typedef enum Ar5416_Rates {
    rate6mb,  rate9mb,  rate12mb, rate18mb,
    rate24mb, rate36mb, rate48mb, rate54mb,
    rate1l,   rate2l,   rate2s,   rate5_5l,
    rate5_5s, rate11l,  rate11s,  rateXr,
    rateHt20_0, rateHt20_1, rateHt20_2, rateHt20_3,
    rateHt20_4, rateHt20_5, rateHt20_6, rateHt20_7,
    rateHt40_0, rateHt40_1, rateHt40_2, rateHt40_3,
    rateHt40_4, rateHt40_5, rateHt40_6, rateHt40_7,
    rateDupCck, rateDupOfdm, rateExtCck, rateExtOfdm,
    Ar5416RateSize
} AR5416_RATES;

typedef struct spurChanStruct {
    u_int16_t spurChan;
    u_int8_t  spurRangeLow;
    u_int8_t  spurRangeHigh;
} __packed SPUR_CHAN;


typedef struct CalTargetPowerLegacy {
    u_int8_t  bChannel;
    u_int8_t  tPow2x[4];
} __packed CAL_TARGET_POWER_LEG;

typedef struct CalTargetPowerHt {
    u_int8_t  bChannel;
    u_int8_t  tPow2x[8];
} __packed CAL_TARGET_POWER_HT;

#if AH_BYTE_ORDER == AH_BIG_ENDIAN
typedef struct CalCtlEdges {
    u_int8_t  bChannel;
    u_int8_t  flag   :2,
             tPower :6;
} __packed CAL_CTL_EDGES;
#else
typedef struct CalCtlEdges {
    u_int8_t  bChannel;
    u_int8_t  tPower :6,
             flag   :2;
} __packed CAL_CTL_EDGES;
#endif

/* EEPROM Default bit map definations */
#define owl_get_eepdef_ver(_ahp)   \
    (((_ahp)->ah_eeprom.map.def.baseEepHeader.version >> 12) & 0xF)
#define owl_get_eepdef_rev(_ahp)   \
    (((_ahp)->ah_eeprom.map.def.baseEepHeader.version) & 0xFFF)

// 16-bit offset location start of calibration struct
#define AR5416_EEPDEF_START_LOC         256
#define AR5416_EEPDEF_NUM_5G_CAL_PIERS      8
#define AR5416_EEPDEF_NUM_2G_CAL_PIERS      4
#define AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS  8
#define AR5416_EEPDEF_NUM_5G_40_TARGET_POWERS  8
#define AR5416_EEPDEF_NUM_2G_CCK_TARGET_POWERS 3
#define AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS  4
#define AR5416_EEPDEF_NUM_2G_40_TARGET_POWERS  4
#define AR5416_EEPDEF_NUM_CTLS              24
#define AR5416_EEPDEF_NUM_BAND_EDGES        8
#define AR5416_EEPDEF_NUM_PD_GAINS          4
#define AR5416_EEPDEF_PD_GAINS_IN_MASK      4
#define AR5416_EEPDEF_PD_GAIN_ICEPTS        5
#define AR5416_EEPDEF_MAX_CHAINS            3

#define AR5416_EEPDEF_NUM_ANT_CHAIN_FIELDS     7
#define AR5416_EEPDEF_NUM_ANT_COMMON_FIELDS    4
#define AR5416_EEPDEF_SIZE_ANT_CHAIN_FIELD     3
#define AR5416_EEPDEF_SIZE_ANT_COMMON_FIELD    4
#define AR5416_EEPDEF_ANT_CHAIN_MASK           0x7
#define AR5416_EEPDEF_ANT_COMMON_MASK          0xf
#define AR5416_EEPDEF_CHAIN_0_IDX              0
#define AR5416_EEPDEF_CHAIN_1_IDX              1
#define AR5416_EEPDEF_CHAIN_2_IDX              2

typedef struct BaseEepDefHeader {
    u_int16_t  length;
    u_int16_t  checksum;
    u_int16_t  version;
    u_int8_t   opCapFlags;
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
    u_int8_t   pwdclkind;
    u_int8_t   fastClk5g;
    u_int8_t   divChain;  
    u_int8_t   rxGainType;
    u_int8_t   dacHiPwrMode_5G; // #bits used: 1, bit0: 1: use the DAC high power mode, 0: don't use the DAC high power mode (1 for MB91 only)
    u_int8_t   openLoopPwrCntl; // #bits used: 1, bit0: 1: use open loop power control, 0: use closed loop power control
    u_int8_t   dacLpMode;       // #bits used: bit0.  
    u_int8_t   txGainType;      // #bits used: all, indicates high power tx gain table support. 0: original 1: high power
    u_int8_t   rcChainMask;     // bits used: bit0, This flag will be "1" if the card is an HB93 1x2, otherwise, it will be "0".
    u_int8_t   desiredScaleCCK; // bits used: bit0-bit4: bb_desired_scale_cck
    u_int8_t   pwrTableOffset;  // offset in dB to be added to begining ot pdadc table in calibration 
    u_int8_t   fracN5g;         // bits used: bit0, indicates that fracN synth mode applies to all 5G channels 
    u_int8_t   futureBase[21];
} __packed BASE_EEPDEF_HEADER; // 64 B

typedef struct ModalEepDefHeader {
    u_int32_t  antCtrlChain[AR5416_EEPDEF_MAX_CHAINS];       // 12
    u_int32_t  antCtrlCommon;                         // 4
    u_int8_t   antennaGainCh[AR5416_EEPDEF_MAX_CHAINS];      // 3
    u_int8_t   switchSettling;                        // 1
    u_int8_t   txRxAttenCh[AR5416_EEPDEF_MAX_CHAINS];        // 3
    u_int8_t   rxTxMarginCh[AR5416_EEPDEF_MAX_CHAINS];       // 3
    u_int8_t   adcDesiredSize;                        // 1
    u_int8_t   pgaDesiredSize;                        // 1
    u_int8_t   xlnaGainCh[AR5416_EEPDEF_MAX_CHAINS];         // 3
    u_int8_t   txEndToXpaOff;                         // 1
    u_int8_t   txEndToRxOn;                           // 1
    u_int8_t   txFrameToXpaOn;                        // 1
    u_int8_t   thresh62;                              // 1
    int8_t     noiseFloorThreshCh[AR5416_EEPDEF_MAX_CHAINS]; // 3
    u_int8_t   xpdGain;                               // 1
    u_int8_t   xpd;                                   // 1
    u_int8_t   iqCalICh[AR5416_EEPDEF_MAX_CHAINS];           // 1
    u_int8_t   iqCalQCh[AR5416_EEPDEF_MAX_CHAINS];           // 1
    u_int8_t   pdGainOverlap;                         // 1
    u_int8_t   ob;                                    // 1 -> For AR9280, this is Chain 0
    u_int8_t   db;                                    // 1 -> For AR9280, this is Chain 0
    u_int8_t   xpaBiasLvl;                            // 1
    u_int8_t   pwrDecreaseFor2Chain;                  // 1
    u_int8_t   pwrDecreaseFor3Chain;                  // 1 -> 48 B
    u_int8_t   txFrameToDataStart;                    // 1
    u_int8_t   txFrameToPaOn;                         // 1
    u_int8_t   ht40PowerIncForPdadc;                  // 1
    u_int8_t   bswAtten[AR5416_EEPDEF_MAX_CHAINS];           // 3
    u_int8_t   bswMargin[AR5416_EEPDEF_MAX_CHAINS];          // 3
    u_int8_t   swSettleHt40;                          // 1
    u_int8_t   xatten2Db[AR5416_EEPDEF_MAX_CHAINS];          // 3 -> New for AR9280 (0xa20c/b20c 11:6)
    u_int8_t   xatten2Margin[AR5416_EEPDEF_MAX_CHAINS];      // 3 -> New for AR9280 (0xa20c/b20c 21:17)
    u_int8_t   ob_ch1;                                // 1 -> ob and db become chain specific from AR9280
    u_int8_t   db_ch1;                                // 1
#if AH_BYTE_ORDER == AH_BIG_ENDIAN
    u_int8_t   useAnt1              : 1,              // 1 Spare bit
               force_xpaon          : 1,              // Force XPA bit for 5G mode
               local_bias           : 1,              // enable local bias
               femBandSelectUsed    : 1,              //
               xlnabufin            : 1,              //
               xlnaisel             : 2,              //
               xlnabufmode          : 1;              //
#else
    u_int8_t   xlnabufmode          : 1,              //
               xlnaisel             : 2,              //
               xlnabufin            : 1,              //
               femBandSelectUsed    : 1,              //
               local_bias           : 1,              // enable local bias
               force_xpaon          : 1,              // Force XPA bit for 5G mode
               useAnt1              : 1;              // 1 Spare bit
#endif
    u_int8_t   miscBits;                              // 5 // bit0, bit1: bb_tx_dac_scale_cck, bit2, bit3, bit4: bb_tx_clip
    u_int16_t  xpaBiasLvlFreq[3];                     // 3
    u_int8_t   futureModal[6];                        // 6

    SPUR_CHAN spurChans[AR5416_EEPROM_MODAL_SPURS];   // 20 B
} __packed MODAL_EEPDEF_HEADER;                          // == 100 B

typedef struct calDataPerFreqOpLoop {
    u_int8_t pwrPdg[2][5]; /* power measurement                  */
    u_int8_t vpdPdg[2][5]; /* pdadc voltage at power measurement */
    u_int8_t pcdac[2][5];  /* pcdac used for power measurement   */
    u_int8_t empty[2][5];  /* future use */
} __packed CAL_DATA_PER_FREQ_OP_LOOP;


typedef struct CalCtlDataEepDef {
    CAL_CTL_EDGES  ctlEdges[AR5416_EEPDEF_MAX_CHAINS][AR5416_EEPDEF_NUM_BAND_EDGES];
} __packed CAL_CTL_DATA_EEPDEF;

typedef struct calDataPerFreqEepDef {
    u_int8_t pwrPdg[AR5416_EEPDEF_NUM_PD_GAINS][AR5416_EEPDEF_PD_GAIN_ICEPTS];
    u_int8_t vpdPdg[AR5416_EEPDEF_NUM_PD_GAINS][AR5416_EEPDEF_PD_GAIN_ICEPTS];
} __packed CAL_DATA_PER_FREQ_EEPDEF;


typedef struct eepMapDef {
    BASE_EEPDEF_HEADER      baseEepHeader;         // 64 B
    u_int8_t             custData[64];                   // 64 B
    MODAL_EEPDEF_HEADER     modalHeader[2];        // 200 B
    u_int8_t             calFreqPier5G[AR5416_EEPDEF_NUM_5G_CAL_PIERS];
    u_int8_t             calFreqPier2G[AR5416_EEPDEF_NUM_2G_CAL_PIERS];
    CAL_DATA_PER_FREQ_EEPDEF    calPierData5G[AR5416_EEPDEF_MAX_CHAINS][AR5416_EEPDEF_NUM_5G_CAL_PIERS];
    CAL_DATA_PER_FREQ_EEPDEF    calPierData2G[AR5416_EEPDEF_MAX_CHAINS][AR5416_EEPDEF_NUM_2G_CAL_PIERS];
    CAL_TARGET_POWER_LEG calTargetPower5G[AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS];
    CAL_TARGET_POWER_HT  calTargetPower5GHT20[AR5416_EEPDEF_NUM_5G_20_TARGET_POWERS];
    CAL_TARGET_POWER_HT  calTargetPower5GHT40[AR5416_EEPDEF_NUM_5G_40_TARGET_POWERS];
    CAL_TARGET_POWER_LEG calTargetPowerCck[AR5416_EEPDEF_NUM_2G_CCK_TARGET_POWERS];
    CAL_TARGET_POWER_LEG calTargetPower2G[AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS];
    CAL_TARGET_POWER_HT  calTargetPower2GHT20[AR5416_EEPDEF_NUM_2G_20_TARGET_POWERS];
    CAL_TARGET_POWER_HT  calTargetPower2GHT40[AR5416_EEPDEF_NUM_2G_40_TARGET_POWERS];
    u_int8_t             ctlIndex[AR5416_EEPDEF_NUM_CTLS];
    CAL_CTL_DATA_EEPDEF         ctlData[AR5416_EEPDEF_NUM_CTLS];
    u_int8_t             padding;
} __packed ar5416_eeprom_def_t;  // EEPDEF_MAP


/* EEPROM 4K bit map definations */
#define owl_get_eep4k_ver(_ahp)   \
    (((_ahp)->ah_eeprom.map.map4k.baseEepHeader.version >> 12) & 0xF)
#define owl_get_eep4k_rev(_ahp)   \
    (((_ahp)->ah_eeprom.map.map4k.baseEepHeader.version) & 0xFFF)

// 16-bit offset location start of calibration struct
#define AR5416_EEP4K_START_LOC         64
#define AR5416_EEP4K_NUM_2G_CAL_PIERS      3
#define AR5416_EEP4K_NUM_2G_CCK_TARGET_POWERS 3
#define AR5416_EEP4K_NUM_2G_20_TARGET_POWERS  3
#define AR5416_EEP4K_NUM_2G_40_TARGET_POWERS  3
#define AR5416_EEP4K_NUM_CTLS              12
#define AR5416_EEP4K_NUM_BAND_EDGES        4
#define AR5416_EEP4K_NUM_PD_GAINS          2
#define AR5416_EEP4K_PD_GAINS_IN_MASK      4
#define AR5416_EEP4K_PD_GAIN_ICEPTS        5
#define AR5416_EEP4K_MAX_CHAINS            1

typedef struct BaseEep4kHeader {
    u_int16_t  length;
    u_int16_t  checksum;
    u_int16_t  version;
    u_int8_t   opCapFlags;
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
    u_int8_t   txGainType;      // #bits used: all, indicates high power tx gain table support. 0: original 1: high power
} __packed BASE_EEP4K_HEADER; // 32 B


typedef struct ModalEep4kHeader {
    u_int32_t  antCtrlChain[AR5416_EEP4K_MAX_CHAINS];       // 4
    u_int32_t  antCtrlCommon;                         // 4
    u_int8_t   antennaGainCh[AR5416_EEP4K_MAX_CHAINS];      // 1
    u_int8_t   switchSettling;                        // 1
    u_int8_t   txRxAttenCh[AR5416_EEP4K_MAX_CHAINS];        // 1
    u_int8_t   rxTxMarginCh[AR5416_EEP4K_MAX_CHAINS];       // 1
    u_int8_t   adcDesiredSize;                        // 1
    u_int8_t   pgaDesiredSize;                        // 1
    u_int8_t   xlnaGainCh[AR5416_EEP4K_MAX_CHAINS];         // 1
    u_int8_t   txEndToXpaOff;                         // 1
    u_int8_t   txEndToRxOn;                           // 1
    u_int8_t   txFrameToXpaOn;                        // 1
    u_int8_t   thresh62;                              // 1
    int8_t     noiseFloorThreshCh[AR5416_EEP4K_MAX_CHAINS]; // 1
    u_int8_t   xpdGain;                               // 1
    u_int8_t   xpd;                                   // 1
    u_int8_t   iqCalICh[AR5416_EEP4K_MAX_CHAINS];           // 1
    u_int8_t   iqCalQCh[AR5416_EEP4K_MAX_CHAINS];           // 1
    u_int8_t   pdGainOverlap;                         // 1
#if AH_BYTE_ORDER == AH_BIG_ENDIAN
    u_int8_t   ob_1:4, // 1 -> For AR9285, ob_0:bit0-3, ob_1:bit4-7
               ob_0:4;
    u_int8_t   db1_1:4, // 1 -> For AR9285, db1_0:bit0-3, db1_1:bit4-7
               db1_0:4;
#else
    u_int8_t   ob_0:4,
               ob_1:4;
    u_int8_t   db1_0:4,
               db1_1:4;
#endif
    u_int8_t   xpaBiasLvl;                            // 1
    u_int8_t   txFrameToDataStart;                    // 1
    u_int8_t   txFrameToPaOn;                         // 1
    u_int8_t   ht40PowerIncForPdadc;                  // 1
    u_int8_t   bswAtten[AR5416_EEP4K_MAX_CHAINS];     // 1
    u_int8_t   bswMargin[AR5416_EEP4K_MAX_CHAINS];    // 1
    u_int8_t   swSettleHt40;                          // 1
    u_int8_t   xatten2Db[AR5416_EEP4K_MAX_CHAINS];          // 1
    u_int8_t   xatten2Margin[AR5416_EEP4K_MAX_CHAINS];      // 1
#if AH_BYTE_ORDER == AH_BIG_ENDIAN
    u_int8_t   db2_1:4,
               db2_0:4; // 1 -> For AR9285, db2_0:bit0-3, db2_1:bit4-7
#else
    u_int8_t   db2_0:4,
               db2_1:4; // 1 -> For AR9285, db2_0:bit0-3, db2_1:bit4-7
#endif
    u_int8_t version; // 1 -> Modal heaber version
#if AH_BYTE_ORDER == AH_BIG_ENDIAN
    u_int8_t   ob_3:4,
               ob_2:4;
    u_int8_t   antdiv_ctl1:4,
               ob_4:4; // 2 -> For AR9285, ob_2:bit0-3, ob_3:bit4-7, ob_4:bit8-11, antdiv_ctl1:bit12-15
    u_int8_t   db1_3:4,
               db1_2:4;
    u_int8_t   antdiv_ctl2:4,
               db1_4:4; // 2 -> For AR9285, db1_2:bit0-3, db1_3:bit4-7, db1_4:bit8-11, antdiv_ctl2:bit12-15
    u_int8_t   db2_2:4,
               db2_3:4;
    u_int8_t   reserved:4,
               db2_4:4; // 2 -> For AR9285, db2_2:bit0-3, db2_3:bit4-7, db2_4:bit8-11
#else
    u_int8_t   ob_2:4,
               ob_3:4;
    u_int8_t   ob_4:4,
               antdiv_ctl1:4; // 2 -> For AR9285, ob_2:bit0-3, ob_3:bit4-7, ob_4:bit8-11, antdiv_ctl1:bit12-15
    u_int8_t   db1_2:4,
               db1_3:4;
    u_int8_t   db1_4:4,
               antdiv_ctl2:4; // 2 -> For AR9285, db1_2:bit0-3, db1_3:bit4-7, db1_4:bit8-11, antdiv_ctl2:bit12-15
    u_int8_t   db2_2:4,
               db2_3:4;
    u_int8_t   db2_4:4,
               reserved:4; // 2 -> For AR9285, db2_2:bit0-3, db2_3:bit4-7, db2_4:bit8-11
#endif
    u_int8_t   tx_diversity;
    u_int8_t   flc_pwr_thresh;
    u_int8_t   bb_desired_scale:5,
               smart_antenna:3;
    u_int8_t   futureModal[1];

    SPUR_CHAN spurChans[AR5416_EEPROM_MODAL_SPURS];   // 20 B
} __packed MODAL_EEP4K_HEADER;                          // == 68 B

typedef struct CalCtlDataEep4k {
    CAL_CTL_EDGES  ctlEdges[AR5416_EEP4K_MAX_CHAINS][AR5416_EEP4K_NUM_BAND_EDGES];
} __packed CAL_CTL_DATA_EEP4K;

typedef struct calDataPerFreqEep4k {
    u_int8_t pwrPdg[AR5416_EEP4K_NUM_PD_GAINS][AR5416_EEP4K_PD_GAIN_ICEPTS];
    u_int8_t vpdPdg[AR5416_EEP4K_NUM_PD_GAINS][AR5416_EEP4K_PD_GAIN_ICEPTS];
} __packed CAL_DATA_PER_FREQ_EEP4K;

typedef struct eepMap4k {
    BASE_EEP4K_HEADER    baseEepHeader;         // 32 B
	u_int8_t             custData[20];          // 20 B 
	MODAL_EEP4K_HEADER   modalHeader;        // 68 B
    u_int8_t             calFreqPier2G[AR5416_EEP4K_NUM_2G_CAL_PIERS];
    CAL_DATA_PER_FREQ_EEP4K    calPierData2G[AR5416_EEP4K_MAX_CHAINS][AR5416_EEP4K_NUM_2G_CAL_PIERS];
    CAL_TARGET_POWER_LEG calTargetPowerCck[AR5416_EEP4K_NUM_2G_CCK_TARGET_POWERS];
    CAL_TARGET_POWER_LEG calTargetPower2G[AR5416_EEP4K_NUM_2G_20_TARGET_POWERS];
    CAL_TARGET_POWER_HT  calTargetPower2GHT20[AR5416_EEP4K_NUM_2G_20_TARGET_POWERS];
    CAL_TARGET_POWER_HT  calTargetPower2GHT40[AR5416_EEP4K_NUM_2G_40_TARGET_POWERS];
    u_int8_t             ctlIndex[AR5416_EEP4K_NUM_CTLS];
    CAL_CTL_DATA_EEP4K   ctlData[AR5416_EEP4K_NUM_CTLS];
    u_int8_t             padding;
} __packed ar5416_eeprom_4k_t;  // EEP4K_MAP


#include "ar5416/ar9287eep.h"

typedef struct eepMap {
	union {
		ar5416_eeprom_def_t def;
		ar5416_eeprom_4k_t map4k;
		ar9287_eeprom_t mapAr9287;
	}map;
} __packed ar5416_eeprom_t;  // EEP_MAP

#ifdef WIN32
#pragma pack(pop, ar5416)
#endif

//#define EEPROM_DUMP 1   // TODO: Remove by FCS

/*
 *  Control inclusion of Target Power Override (<= 14.4)
 *  define any of the following 7 lines to enable the respective mode's override.
 */
#if AH_AR5416_OVRD_TGT_PWR_5G == 1 || \
    AH_AR5416_OVRD_TGT_PWR_5GHT20 == 1 || \
    AH_AR5416_OVRD_TGT_PWR_5GHT40 == 1 || \
    AH_AR5416_OVRD_TGT_PWR_CCK == 1 || \
    AH_AR5416_OVRD_TGT_PWR_2G == 1 || \
    AH_AR5416_OVRD_TGT_PWR_2GHT20 == 1 || \
    AH_AR5416_OVRD_TGT_PWR_2GHT40 == 1
#define AH_AR5416_OVRD_TGT_PWR  /* Control general feature to override target powers */
#endif



/**************************************************************************
 * interpolate
 *
 * Returns signed interpolated or the scaled up interpolated value
 */
static inline int16_t
interpolate(u_int16_t target, u_int16_t srcLeft, u_int16_t srcRight,
    int16_t targetLeft, int16_t targetRight)
{
    int16_t rv;

    if (srcRight == srcLeft)
    {
        rv = targetLeft;
    }
    else
    {
        rv = (int16_t)( ((target - srcLeft) * targetRight +
            (srcRight - target) * targetLeft) / (srcRight - srcLeft) );
    }
    return rv;
}

/**************************************************************************
 * fbin2freq
 *
 * Get channel value from binary representation held in eeprom
 * RETURNS: the frequency in MHz
 */
static inline u_int16_t
fbin2freq(u_int8_t fbin, HAL_BOOL is2GHz)
{
    /*
    * Reserved value 0xFF provides an empty definition both as
    * an fbin and as a frequency - do not convert
    */
    if (fbin == AR5416_BCHAN_UNUSED)
    {
        return fbin;
    }

    return (u_int16_t)((is2GHz) ? (2300 + fbin) : (4800 + 5 * fbin));
}

/**************************************************************
 * getLowerUppderIndex
 *
 * Return indices surrounding the value in sorted integer lists.
 * Requirement: the input list must be monotonically increasing
 *     and populated up to the list size
 * Returns: match is set if an index in the array matches exactly
 *     or a the target is before or after the range of the array.
 */
static inline HAL_BOOL
getLowerUpperIndex(u_int8_t target, u_int8_t *pList, u_int16_t listSize,
    u_int16_t *indexL, u_int16_t *indexR)
{
    u_int16_t i;

    /*
    * Check first and last elements for beyond ordered array cases.
    */
    if (target <= pList[0])
    {
        *indexL = *indexR = 0;
        return AH_TRUE;
    }
    if (target >= pList[listSize-1])
    {
        *indexL = *indexR = (u_int16_t)(listSize - 1);
        return AH_TRUE;
    }

    /* look for value being near or between 2 values in list */
    for (i = 0; i < listSize - 1; i++)
    {
        /*
        * If value is close to the current value of the list
        * then target is not between values, it is one of the values
        */
        if (pList[i] == target)
        {
            *indexL = *indexR = i;
            return AH_TRUE;
        }
        /*
        * Look for value being between current value and next value
        * if so return these 2 values
        */
        if (target < pList[i + 1])
        {
            *indexL = i;
            *indexR = (u_int16_t)(i + 1);
            return AH_FALSE;
        }
    }
    HALASSERT(0);
    return AH_FALSE;
}

/**************************************************************
 * ar5416FillVpdTable
 *
 * Fill the Vpdlist for indices Pmax-Pmin
 * Note: pwrMin, pwrMax and Vpdlist are all in dBm * 4
 */
static inline HAL_BOOL
ar5416FillVpdTable(u_int8_t pwrMin, u_int8_t pwrMax, u_int8_t *pPwrList,
    u_int8_t *pVpdList, u_int16_t numIntercepts, u_int8_t *pRetVpdList)
{
    u_int16_t  i, k;
    u_int8_t   currPwr = pwrMin;
    u_int16_t  idxL = 0, idxR = 0;

    HALASSERT(pwrMax > pwrMin);
    for (i = 0; i <= (pwrMax - pwrMin) / 2; i++)
    {
        /* Code status canalysis fix */
        if(!(i < AR5416_MAX_PWR_RANGE_IN_HALF_DB))
        {
            HALASSERT(0);
            break;
        }
        getLowerUpperIndex(currPwr, pPwrList, numIntercepts,
            &(idxL), &(idxR));
        if (idxR < 1)
            idxR = 1;           /* extrapolate below */
        if (idxL == numIntercepts - 1)
            idxL = (u_int16_t)(numIntercepts - 2);   /* extrapolate above */
        if (pPwrList[idxL] == pPwrList[idxR])
            k = pVpdList[idxL];
        else
            k = (u_int16_t)( ((currPwr - pPwrList[idxL]) * pVpdList[idxR] + (pPwrList[idxR] - currPwr) * pVpdList[idxL]) /
                (pPwrList[idxR] - pPwrList[idxL]) );
        HALASSERT(k < 256);
        pRetVpdList[i] = (u_int8_t)k;
        currPwr += 2;               /* half dB steps */
    }

    return AH_TRUE;
}

#endif  /* _ATH_AR5416_EEP_H_ */
