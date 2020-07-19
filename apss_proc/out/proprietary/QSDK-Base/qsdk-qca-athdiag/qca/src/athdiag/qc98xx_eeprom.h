/*
 *  Copyright (c) 2011 Qualcomm Atheros, Inc.
 *  All Rights Reserved.
 *  Qualcomm Atheros Confidential and Proprietary.
 */


/************************************************************************/
/* Header file for EEPROM module -chip QC98xx                           */
/************************************************************************/
#ifndef _QC98XX_EEPROM_H_
#define _QC98XX_EEPROM_H_

#if defined(WIN32) || defined(WIN64)
#pragma pack (push, 1)
#endif /* WIN32 || WIN64 */
#if !defined(ATH_TARGET)
#include "athstartpack.h"
#endif /* !ATH_TARGET */

#if 0
#if defined(FPGA)
#error "Use this header file only for silicon"
#endif /* FPGA */
#endif

#define	AH_LITTLE_ENDIAN    1234
#define	AH_BIG_ENDIAN       4321

#ifndef AH_BYTE_ORDER
#define AH_BYTE_ORDER       AH_BIG_ENDIAN
#endif

#define QC98XX_EEP_VER1             1
#define QC98XX_EEP_VER2             2
#define QC98XX_EEP_VER              QC98XX_EEP_VER2

#define WHAL_NUM_CHAINS							3
#define WHAL_NUM_STREAMS						3	
#define WHAL_NUM_BI_MODAL						2
#define WHAL_NUM_AC_MODAL						5

#define WHAL_NUM_11B_TARGET_POWER_CHANS				2
#define WHAL_NUM_11G_LEG_TARGET_POWER_CHANS			3
#define WHAL_NUM_11G_20_TARGET_POWER_CHANS			3
#define WHAL_NUM_11G_40_TARGET_POWER_CHANS			3
#define WHAL_NUM_11A_LEG_TARGET_POWER_CHANS			6
#define WHAL_NUM_11A_20_TARGET_POWER_CHANS			6
#define WHAL_NUM_11A_40_TARGET_POWER_CHANS			6
#define WHAL_NUM_11A_80_TARGET_POWER_CHANS			6

#define WHAL_NUM_LEGACY_TARGET_POWER_RATES			4
#define WHAL_NUM_11B_TARGET_POWER_RATES				4
#define WHAL_NUM_VHT_TARGET_POWER_RATES				(5*WHAL_NUM_STREAMS+3)	//18 make sure this is not odd number
#define WHAL_NUM_HT_TARGET_POWER_RATES				WHAL_NUM_VHT_TARGET_POWER_RATES			
#define WHAL_NUM_11G_20_TARGET_POWER_RATES			(WHAL_NUM_VHT_TARGET_POWER_RATES)
#define WHAL_NUM_11G_40_TARGET_POWER_RATES			(WHAL_NUM_VHT_TARGET_POWER_RATES)
#define WHAL_NUM_11A_20_TARGET_POWER_RATES			(WHAL_NUM_VHT_TARGET_POWER_RATES)
#define WHAL_NUM_11A_40_TARGET_POWER_RATES			(WHAL_NUM_VHT_TARGET_POWER_RATES)
#define WHAL_NUM_11A_80_TARGET_POWER_RATES			(WHAL_NUM_VHT_TARGET_POWER_RATES)
#define WHAL_NUM_11G_20_TARGET_POWER_RATES_DIV2		(WHAL_NUM_VHT_TARGET_POWER_RATES/2)
#define WHAL_NUM_11G_40_TARGET_POWER_RATES_DIV2		(WHAL_NUM_VHT_TARGET_POWER_RATES/2)
#define WHAL_NUM_11A_20_TARGET_POWER_RATES_DIV2		(WHAL_NUM_VHT_TARGET_POWER_RATES/2)
#define WHAL_NUM_11A_40_TARGET_POWER_RATES_DIV2		(WHAL_NUM_VHT_TARGET_POWER_RATES/2)
#define WHAL_NUM_11A_80_TARGET_POWER_RATES_DIV2		(WHAL_NUM_VHT_TARGET_POWER_RATES/2)

/* Ensure this the MAX target power rates are accurate */
#define WHAL_NUM_MAX_TARGET_POWER_RATES				WHAL_NUM_VHT_TARGET_POWER_RATES

#define ERR_EEP_READ        -5
#define ERR_MAX_REACHED     -4
#define ERR_NOT_FOUND       -3
#define ERR_VALUE_BAD       -2
#define ERR_RETURN          -1
#define VALUE_OK        0
#define VALUE_NEW       1
#define VALUE_UPDATED   2
#define VALUE_SAME      3
#define VALUE_REMOVED   4
#define SformatOutput snprintf
#define SformatInput sscanf
extern A_UINT8 *pQc9kEepromArea;
extern A_UINT8 *pQc9kEepromBoardArea;

// Target power rates grouping
enum {
	WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20= 0,
    WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22,
    WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24,
    WHAL_VHT_TARGET_POWER_RATES_MCS5,
    WHAL_VHT_TARGET_POWER_RATES_MCS6,
    WHAL_VHT_TARGET_POWER_RATES_MCS7,
    WHAL_VHT_TARGET_POWER_RATES_MCS8,
    WHAL_VHT_TARGET_POWER_RATES_MCS9,
    WHAL_VHT_TARGET_POWER_RATES_MCS15,
    WHAL_VHT_TARGET_POWER_RATES_MCS16,
    WHAL_VHT_TARGET_POWER_RATES_MCS17,
    WHAL_VHT_TARGET_POWER_RATES_MCS18,
    WHAL_VHT_TARGET_POWER_RATES_MCS19,
    WHAL_VHT_TARGET_POWER_RATES_MCS25,
    WHAL_VHT_TARGET_POWER_RATES_MCS26,
    WHAL_VHT_TARGET_POWER_RATES_MCS27,
    WHAL_VHT_TARGET_POWER_RATES_MCS28,
    WHAL_VHT_TARGET_POWER_RATES_MCS29,
};

#define QC98XX_NUM_5G_PER_100M					10 
#define QC98XX_NUM_ALPHATHERM_CHANS_2G			4
#define QC98XX_NUM_ALPHATHERM_CHANS_5G			8
#define QC98XX_NUM_ALPHATHERM_TEMPS				4
#define QC98XX_CONFIG_ENTRIES					24
#define CUSTOMER_DATA_SIZE						20

#define WHAL_NUM_11A_CAL_PIERS					8
#define WHAL_NUM_11G_CAL_PIERS					3
#define WHAL_NUM_CAL_GAINS						2
#define WHAL_NUM_CTLS_5G						18		//6 modes (A, HT20, HT40, VHT20, VHT40, VHT80) * 3 reg dommains
#define WHAL_NUM_CTLS_2G						18		//6 modes (B, G, HT20, HT40, VHT20, VHT40) * 3 reg domains
#define WHAL_NUM_BAND_EDGES_5G					8
#define WHAL_NUM_BAND_EDGES_2G					4
#define QC98XX_EEPROM_MODAL_SPURS				5
#define QC98XX_EEPROM_SPUR_MIT_ENABLE_NF_RSSI	1
#define WHAL_NUM_XLNA							4

#define WHAL_NEG_PWR_OFFSET						(-20)
#define WHAL_FREQ2FBIN(x,y) ((y) ? ((x) - 2300) : (((x) - 4800) / 5))
#define WHAL_FBIN2FREQ(x,y) ((y) ? ((x) + 2300) : (((x) * 5) + 4800))

#define WHAL_BCHAN_UNUSED						0xFF

#define WHAL_CHIP_MAX_TX_POWER                  63

#define MAX_BASE_FUTURE							66
#define MAX_BIMODAL_FUTURE						123
//#define MAX_MODAL_FUTURE						128
//#define MAX_GPIO_CONFIG							57//?????
//#define MAX_GPIO_FUTURE							10
//#define MAX_NEW_FEATURE_DATA_2G					128
//#define MAX_NEW_FEATURE_DATA_5G					88
//#define MAX_PAD_FUTURE							342
//#define MAX_BOARD_GPIO_CFG_EXT					3

#define BIMODAL_5G_INDX							0
#define BIMODAL_2G_INDX							1

#define WHAL_FEATUREENABLE_TEMP_COMP_MASK			0x01
#define WHAL_FEATUREENABLE_TEMP_COMP_SHIFT			0
#define WHAL_FEATUREENABLE_VOLT_COMP_MASK			0x02
#define WHAL_FEATUREENABLE_VOLT_COMP_SHIFT			1
#define WHAL_FEATUREENABLE_DOUBLING_MASK			0x04
#define WHAL_FEATUREENABLE_DOUBLING_SHIFT			2
#define WHAL_FEATUREENABLE_INTERNAL_REGULATOR_MASK	0x08
#define WHAL_FEATUREENABLE_INTERNAL_REGULATOR_SHIFT	3
#define WHAL_FEATUREENABLE_TUNING_CAPS_MASK			0x40
#define WHAL_FEATUREENABLE_TUNING_CAPS_SHIFT		6


#define WHAL_MISCCONFIG_DRIVE_STRENGTH	        0x01 //bit0: drive strength;
#define WHAL_MISCCONFIG_FORCE_THERM_MASK        0x06 //bit1_2: 0=don't force, 1=force to thermometer 0, 2=force to thermometer 1, 3=force to thermometer 2;
#define WHAL_MISCCONFIG_FORCE_THERM_DONT		0x00
#define WHAL_MISCCONFIG_FORCE_THERM_0			0x02
#define WHAL_MISCCONFIG_FORCE_THERM_1			0x04
#define WHAL_MISCCONFIG_FORCE_THERM_2			0x06
#define WHAL_MISCCONFIG_CHAIN_MASK_REDUCE       0x08 //bit3: chain mask reduce;
#define WHAL_MISCCONFIG_QUICK_DROP              0x10 //bit4: quick drop
#define WHAL_MISCCONFIG_EEPMISC_MASK            0x60
#define WHAL_MISCCONFIG_EEPMISC_SHIFT           5
#define WHAL_MISCCONFIG_EEPMISC_BIG_ENDIAN		0x20 //bit5: big Endian
#define WHAL_MISCCONFIG_EEPMISC_WOW             0x40 //bit6: wow
#define WHAL_MISCCONFIG_CTL_N_SAME_CTL_AC       0x80 //bit7: set if ctl_n and ctl_ac using the same table

#define WHAL_ANALOG_PMU_WLAN_WLAN_PMU1_SWREG_SETABLE_MASK  0xff

#define WHAL_BOARD_FLAG1_BIBXOSC0_MASK          0x01
#define WHAL_BOARD_FLAG1_BIBXOSC0_SHIFT         0
#define WHAL_BOARD_FLAG1_SWREG_SET_MASK         0x08 //equivalent to WHAL_BOARD_SWREG_SET in whal_api.h

typedef enum {
    WHAL_OPFLAGS_11A                    = 0x01,
    WHAL_OPFLAGS_11G                    = 0x02,
    WHAL_OPFLAGS_5G_HT40                = 0x04,
    WHAL_OPFLAGS_2G_HT40                = 0x08,
    WHAL_OPFLAGS_5G_HT20                = 0x10,
    WHAL_OPFLAGS_2G_HT20                = 0x20,
} WHAL_OPFLAGS;

typedef enum {
    WHAL_OPFLAGS2_5G_VHT20               = 0x01,
    WHAL_OPFLAGS2_2G_VHT20               = 0x02,
    WHAL_OPFLAGS2_5G_VHT40               = 0x04,
    WHAL_OPFLAGS2_2G_VHT40               = 0x08,
    WHAL_OPFLAGS2_5G_VHT80               = 0x10,
} WHAL_OPFLAGS2;

typedef struct {
    A_UINT8  opFlags;              // see WHAL_OPFLAGS_ defined above
    A_UINT8  featureEnable;        // see WHAL_FEATUREENABLE_ defined above
    A_UINT8  miscConfiguration;	   // see WHAL_MISCCONFIG_ defined above
    A_UINT8  flag1;                // see WHAL_BOARD_FLAG1_BIBXOSC0
    A_UINT32 boardFlags;           // 32-bit see WHAL_BOARD_FLAGS enum defined in target\src\wlan\wal\include\whal_api.h
    A_UINT16 blueToothOptions;
    A_UINT8  opFlags2;
    A_UINT8  flag2;
} __ATTRIB_PACK QC98XX_EEP_FLAGS;  //12B

typedef struct {
    A_UINT16  length;								//  2B
    A_UINT16  checksum;								//  2B
    A_UINT8   eeprom_version;						//  1B
    A_UINT8   template_version;						//  1B
    A_UINT8   macAddr[6];							//  6B
    A_UINT16  regDmn[2];							//  4B
    QC98XX_EEP_FLAGS  opCapBrdFlags;				// 12B
    A_UINT16  binBuildNumber;						//  2B   major(4):minor(5):build(7)
    A_UINT8   txrxMask;								//  1B 4 bits tx and 4 bits rx
    A_UINT8   rfSilent;                             //  1B
    A_UINT8   wlanLedGpio;                          //  1B
    A_UINT8   spurBaseA;							//  1B
    A_UINT8   spurBaseB;							//  1B
    A_UINT8   spurRssiThresh;						//  1B
    A_UINT8   spurRssiThreshCck;					//  1B
    A_UINT8   spurMitFlag;							//  1B
    A_UINT8   swreg;								//  1B SW controlled internal regulator fields
    A_UINT8   txrxgain;								//  1B
    A_INT8    pwrTableOffset;                       //  1B
    A_UINT8   param_for_tuning_caps;				//  1B
    A_INT8    deltaCck20_t10;						// 1B, "desired_scale_cck"
    A_INT8    delta4020_t10;						// 1B
    A_INT8    delta8020_t10;						// 1B
    A_UINT8   custData[CUSTOMER_DATA_SIZE];			// 20B
    A_UINT8   param_for_tuning_caps_1;              // 1B
    A_UINT8   baseFuture[MAX_BASE_FUTURE];			// 66B
} __ATTRIB_PACK BASE_EEP_HEADER;					// = 132B

typedef struct {
    A_UINT8  spurChan;
    //A_UINT8  spurABChoose;    
    A_UINT8  spurA_PrimSecChoose;
    A_UINT8  spurB_PrimSecChoose;
} __ATTRIB_PACK SPUR_CHAN;							// = 3B

//Note the order of the fields in this structure has been optimized to put all fields likely to change together
typedef struct {
    A_INT8    voltSlope[WHAL_NUM_CHAINS];			// 3B
    SPUR_CHAN spurChans[QC98XX_EEPROM_MODAL_SPURS];	// 15B  spur channels in usual fbin coding format
    A_UINT8   xpaBiasLvl;							// 1B "xpabiaslvl" 4bits
    A_INT8    antennaGainCh;						// 1B affect antenna gain calculation
    A_UINT32  antCtrlCommon;                        // 4B "switch_table_com1" idle, t1, t2, b (4 bits per setting = 16bits)
    A_UINT32  antCtrlCommon2;                       // 4B "switch_table_com2" ra1l1, ra2l1, ra1l2, ra2l2, ra12
    A_UINT16  antCtrlChain[WHAL_NUM_CHAINS];        // 6B "switch_table_chn_b0" idle, t, r, rx1, rx12, b (2 bits each)
    A_UINT8   rxFilterCap;							// 1B
    A_UINT8   rxGainCap;							// 1B
    A_UINT8   txrxgain;								//  1B
    A_INT8    noiseFlrThr;                          // 1B
    A_INT8    minCcaPwr[WHAL_NUM_CHAINS];           // 3B
    A_UINT8   futureBiModal[MAX_BIMODAL_FUTURE];    // 123B  
} __ATTRIB_PACK BIMODAL_EEP_HEADER;					// = 164B

typedef struct
{
    A_UINT8   value2G;
    A_UINT8   value5GLow;		// freqs: 5180-5500
    A_UINT8   value5GMid;		// freqs: 5500-5785
    A_UINT8   value5GHigh;		// freqs: 5785-
} __ATTRIB_PACK FREQ_MODAL_PIERS;       //4B

typedef struct
{
    FREQ_MODAL_PIERS xatten1DB[WHAL_NUM_CHAINS];           // 12B  "xatten1_db" 6bits
    FREQ_MODAL_PIERS xatten1Margin[WHAL_NUM_CHAINS];       // 12B  "xatten1_margin" 5bits
    A_UINT8 reserved [sizeof(FREQ_MODAL_PIERS)*WHAL_NUM_CHAINS*5];// 60B
} __ATTRIB_PACK FREQ_MODAL_EEP_HEADER;		//84B

typedef struct {
    A_UINT8 txgainIdx[WHAL_NUM_CAL_GAINS];
    A_UINT16 power_t8[WHAL_NUM_CAL_GAINS];
} __ATTRIB_PACK CAL_DATA_PER_POINT_OLPC;		// 6B

typedef struct {
    CAL_DATA_PER_POINT_OLPC calPerPoint[WHAL_NUM_CHAINS];		// 18B
    A_INT8   dacGain[WHAL_NUM_CAL_GAINS];						// 2B
    A_UINT8  thermCalVal;										// 1B, "therm_cal_value"
    A_UINT8  voltCalVal;										// 1B, "volt_cal_value"
} __ATTRIB_PACK CAL_DATA_PER_FREQ_OLPC;             // = 22B

typedef struct {
    A_INT16     thermAdcScaledGain;   // 2B, "therm_adc_scaled_gain"
    A_INT8      thermAdcOffset;       // 1B, "therm_adc_offset"
    A_UINT8     rbias;			      // 1B											//
} __ATTRIB_PACK CAL_DATA_PER_CHIP;    // = 4B  

typedef struct {
    A_INT8     NF_Power_dBr[WHAL_NUM_CHAINS];   // 3B, "nf_power_dBr"
    A_INT8     NF_Power_dBm[WHAL_NUM_CHAINS];   // 3B, "nf_power_dBm"
	A_UINT8    NF_thermCalVal;                  // 1B, "therm_cal_value"
	A_UINT8    NF_thermCalSlope;                // 1B, "therm_cal_slope"
} __ATTRIB_PACK NF_CAL_DATA_PER_CHIP;    // = 8B  

typedef struct {
    A_UINT8  tPow2x[WHAL_NUM_LEGACY_TARGET_POWER_RATES];
} __ATTRIB_PACK CAL_TARGET_POWER_LEG;                       // 4B
#define TARGET_POWER_LEG  CAL_TARGET_POWER_LEG

typedef struct {
    A_UINT8  tPow2x[WHAL_NUM_11B_TARGET_POWER_RATES];
} __ATTRIB_PACK CAL_TARGET_POWER_11B;                       // 4B
#define TARGET_POWER_11B  CAL_TARGET_POWER_11B

// For VHT rates, the target powers for each pier are stored as follow:
// Target power are stored as .5 dB step (24 = 12 dB)
// There are 3 base target powers for 1-stream, 2-stream and 3-stream VHT rates
// 18 values (4-bit each) represent the target power delta against their bases for the following data rate groups:
// MCS0_10_20, MCS1_2_11_12_21_22, MCS3_4_13_14_23_24, 
// MCS5, MCS6, MCS7, MCS8, MCS9, 
// MCS15, MCS16, MCS17, MCS18, MCS19, 
// MCS25, MCS26, MCS27, MCS28, MCS29,
//	Delta of MCS0_10_20 is stored in tPow2xDelta[3:0], delta of MCS1_2_11_12_21_22 is stored in tPow2xDelta[7:4], and so on
//		tPow2x of Xstream MCS0 = tPow2xBase[X-1] + (tPow2xDelta[0] & 0xf)
//		tPow2x of Xstream MCS1_2 = tPow2xBase[X-1] + ((tPow2xDelta[0] >> 4) & 0xf)
//		tPow2x of 1stream MCS5 = tPow2xBase[0] + ((tPow2xDelta[1] >> 4) & 0xf)
//		tPow2x of 2stream MCS5 (MCS15) = tPow2xBase[2] + (tPow2xDelta[4] & 0xf)

// 11/19/2012 - The delta is extended to 5 bits. The extended bits are defined in extTPow2xDelta2G and extTPow2xDelta5G
// The calculation to get the 5th bit of a given [bandwith, pier, rateIndex] is:
// bitthInArray = ( (MAX_PIERS * bandwith) + pier) * MAX_RATE_INDEX + rateIndex;
// fifthBit = (extTPow2xDelta[bitthInArray >> 3] >> (bitthInArray % 8)) & 1;
//
// where: MAX_PIERS = 3 for 2GHz and 6 for 5GHz; MAX_RATE_INDEX = 18
//               bandwith: 0 for VHT20, 1 for VHT40, 2 for VHT80
//               pier: 0..2 for 2GHz; 0..5 for 5GHz.

#define CAL_TARGET_POWER_VHT20_INDEX    0
#define CAL_TARGET_POWER_VHT40_INDEX    1
#define CAL_TARGET_POWER_VHT80_INDEX    2
#define QC98XX_EXT_TARGET_POWER_SIZE_2G  16
#define QC98XX_EXT_TARGET_POWER_SIZE_5G  44

typedef struct {
    A_UINT8  tPow2xBase[WHAL_NUM_STREAMS];
    A_UINT8  tPow2xDelta[WHAL_NUM_11G_20_TARGET_POWER_RATES_DIV2];
} __ATTRIB_PACK CAL_TARGET_POWER_11G_20;                    // 12B

typedef struct {
    A_UINT8  tPow2xBase[WHAL_NUM_STREAMS];
    A_UINT8  tPow2xDelta[WHAL_NUM_11G_40_TARGET_POWER_RATES_DIV2];
} __ATTRIB_PACK CAL_TARGET_POWER_11G_40;					// 12B

typedef struct {
    A_UINT8  tPow2xBase[WHAL_NUM_STREAMS];
    A_UINT8  tPow2xDelta[WHAL_NUM_11A_20_TARGET_POWER_RATES_DIV2];
} __ATTRIB_PACK CAL_TARGET_POWER_11A_20;                    // 12B

typedef struct {
    A_UINT8  tPow2xBase[WHAL_NUM_STREAMS];
    A_UINT8  tPow2xDelta[WHAL_NUM_11A_40_TARGET_POWER_RATES_DIV2];
} __ATTRIB_PACK CAL_TARGET_POWER_11A_40;                    // 12B

typedef struct {
    A_UINT8  tPow2xBase[WHAL_NUM_STREAMS];
    A_UINT8  tPow2xDelta[WHAL_NUM_11A_80_TARGET_POWER_RATES_DIV2];
} __ATTRIB_PACK CAL_TARGET_POWER_11A_80;                    // 12B
#define CAL_TARGET_POWER_HT_VHT   CAL_TARGET_POWER_11A_80

typedef union {
    A_UINT8  tPow2x[WHAL_NUM_MAX_TARGET_POWER_RATES];
    struct {
        A_UINT8  tPow2x[WHAL_NUM_LEGACY_TARGET_POWER_RATES];
    } __ATTRIB_PACK pwrLeg;
    struct {
        A_UINT8  tPow2x[WHAL_NUM_11B_TARGET_POWER_RATES];
    } __ATTRIB_PACK pwr11B;                       
    struct {
        A_UINT8  tPow2x[WHAL_NUM_11G_20_TARGET_POWER_RATES];
    } __ATTRIB_PACK pwr11G20;			
    struct {
        A_UINT8  tPow2x[WHAL_NUM_11G_40_TARGET_POWER_RATES];
    } __ATTRIB_PACK pwr11G40;			
    struct {
        A_UINT8  tPow2x[WHAL_NUM_11A_20_TARGET_POWER_RATES];
    } __ATTRIB_PACK pwr11A20;			
    struct {
        A_UINT8  tPow2x[WHAL_NUM_11A_40_TARGET_POWER_RATES];
    } __ATTRIB_PACK pwr11A40;			
    struct {
        A_UINT8  tPow2x[WHAL_NUM_11A_80_TARGET_POWER_RATES];
    } __ATTRIB_PACK pwr11A80;			
} __ATTRIB_PACK CAL_TARGET_POWER;							

#if (AH_BYTE_ORDER == AH_BIG_ENDIAN)
typedef union {
    A_UINT8   ctlEdgePwr;
    struct {
        A_UINT8 flag  :2,
                tPower :6;
    } u;
} __ATTRIB_PACK CAL_CTL_EDGE_PWR;                           // 1B
#else
typedef union {
    A_UINT8   ctlEdgePwr;
    struct {
        A_UINT8   tPower :6,
                  flag   :2;
    } u;
} __ATTRIB_PACK CAL_CTL_EDGE_PWR;                           // 1B
#endif

typedef struct {
    CAL_CTL_EDGE_PWR  ctl_edges[WHAL_NUM_BAND_EDGES_2G];
} __ATTRIB_PACK CAL_CTL_DATA_2G;                            // 4B

typedef struct {
    CAL_CTL_EDGE_PWR  ctl_edges[WHAL_NUM_BAND_EDGES_5G];
} __ATTRIB_PACK CAL_CTL_DATA_5G;                            // 8B

typedef struct {
	// frequencies used are 2412, 2437, 2472
    A_UINT8   alphaThermTbl[WHAL_NUM_CHAINS][QC98XX_NUM_ALPHATHERM_CHANS_2G][QC98XX_NUM_ALPHATHERM_TEMPS]; // 48B      
} __ATTRIB_PACK TEMP_COMP_TBL_2G;

typedef struct {
	// frequencies used are 5180, 5240, 5360, 5320, 5500, 5600, 5700, 5805
    A_UINT8   alphaThermTbl[WHAL_NUM_CHAINS][QC98XX_NUM_ALPHATHERM_CHANS_5G][QC98XX_NUM_ALPHATHERM_TEMPS]; // 96B      
} __ATTRIB_PACK TEMP_COMP_TBL_5G;

typedef struct{
    struct {
        A_UINT16  config    :4,
                  strengh   :2,
                  output    :1,
                  pull      :2,
                  applied   :1,
                  available :1,
                  swfun     :5;
    } cfg;
} __ATTRIB_PACK GPIO_CONFIG;					// = 2B

#define CONFIG_ADDR_ADDRESS_MASK    0x000FFFFF
#define CONFIG_ADDR_ADDRESS_SHIFT   0
#define CONFIG_ADDR_MODE_MASK       0x00300000
#define CONFIG_ADDR_MODE_SHIFT      20
#define CONFIG_ADDR_NUMVALUES_MASK  0x00c00000
#define CONFIG_ADDR_NUMVALUES_SHIFT 22
#define CONFIG_ADDR_CTRL_MASK       0xff000000
#define CONFIG_ADDR_PREPOST_MASK    0x01000000
#define CONFIG_ADDR_CTRL_SHIFT      24

#define OTP_CONFIG_MODE_UNKNOWN     0
#define OTP_CONFIG_MODE_COMMON      1
#define OTP_CONFIG_MODE_2MODAL      2      // 2G,5G
#define OTP_CONFIG_MODE_5MODAL      3      // 2G_VHT20, 2G_VHT40, 5G_VHT20, 5G_VHT40, 5G_VHT80

#define GET_LENGTH_CONFIG_ENTRY_32B(mode) (((mode) == OTP_CONFIG_MODE_5MODAL) ? 6 : (((mode) == OTP_CONFIG_MODE_2MODAL) ? 3 : 2))
#define GET_LENGTH_CONFIG_ENTRY_8B(mode) (sizeof(A_UINT32) * GET_LENGTH_CONFIG_ENTRY_32B(mode))

typedef struct {
    A_UINT32 address;

    // This value field can be varied in size depended on the mode.
    // Common mode, 1 value
    // 2-value Modal mode, 2 values (value2G, value5G)
    // 5-value Modal mode, 5 values (value2GHt20, value2GHt40, value5GHt20, value5GHt40, value5GHt80)
    A_UINT32 value[5]; 

}__ATTRIB_PACK CONFIG_ADDR;                                             // = 24B 

typedef struct qc98xxEeprom {
    BASE_EEP_HEADER               baseEepHeader;                        // 132B   

    // biModalHeader[0] for 5G and biModalHeader[1] for 2G
    BIMODAL_EEP_HEADER            biModalHeader[WHAL_NUM_BI_MODAL];     // 328B 

    FREQ_MODAL_EEP_HEADER         freqModalHeader;                      //  84B
	
    // modalHeader[5]: 0 for 5GHT80, 1 for 5GHT40, 2 for 5GHT20, 3 for 2GHT40 and 4 for 2GHT20 
    //MODAL_EEP_HEADER			  modalHeader[WHAL_NUM_AC_MODAL]; 
    CAL_DATA_PER_CHIP             chipCalData;                          //   4B 
                                                                        // = 548B
	
    // 2G
    A_UINT8                       calFreqPier2G[WHAL_NUM_11G_CAL_PIERS];                        //   3B
    A_UINT8                       pad0[1];                                                      //   1B
    CAL_DATA_PER_FREQ_OLPC        calPierData2G[WHAL_NUM_11G_CAL_PIERS];                        //  66B   
    A_UINT8                       pad1[46];                                                     //  46B 
    
    A_UINT8                       extTPow2xDelta2G[QC98XX_EXT_TARGET_POWER_SIZE_2G];     		//  16B
    A_UINT8                       targetFreqbinCck[WHAL_NUM_11B_TARGET_POWER_CHANS];            //   2B
    A_UINT8                       targetFreqbin2G[WHAL_NUM_11G_LEG_TARGET_POWER_CHANS];         //   3B
    A_UINT8                       pad11[1];                                                     //   1B
    A_UINT8                       targetFreqbin2GVHT20[WHAL_NUM_11G_20_TARGET_POWER_CHANS];     //   3B
    A_UINT8                       pad12[1];                                                     //   1B
    A_UINT8                       targetFreqbin2GVHT40[WHAL_NUM_11G_40_TARGET_POWER_CHANS];     //   3B
    A_UINT8                       pad2[1];                                                      //   1B

    CAL_TARGET_POWER_11B          targetPowerCck[WHAL_NUM_11B_TARGET_POWER_CHANS];              //   8B
    CAL_TARGET_POWER_LEG          targetPower2G[WHAL_NUM_11G_LEG_TARGET_POWER_CHANS];           //  12B
    CAL_TARGET_POWER_11G_20       targetPower2GVHT20[WHAL_NUM_11G_20_TARGET_POWER_CHANS];       //  36B
    CAL_TARGET_POWER_11G_40       targetPower2GVHT40[WHAL_NUM_11G_40_TARGET_POWER_CHANS];       //   36B

    A_UINT8                       ctlIndex2G[WHAL_NUM_CTLS_2G];                                 //  18B
	A_UINT8						  ctl2GFuture[2];												//   2B
    A_UINT8                       ctlFreqbin2G[WHAL_NUM_CTLS_2G][WHAL_NUM_BAND_EDGES_2G];       //  72B
    CAL_CTL_DATA_2G               ctlData2G[WHAL_NUM_CTLS_2G];                                  //  72B  
    NF_CAL_DATA_PER_CHIP		  nfCalData2G [ WHAL_NUM_11G_CAL_PIERS ];                       //  24B
	A_UINT8						  ctl2GFuture1[16];												//  16B

                                                                                               
    TEMP_COMP_TBL_2G              tempComp2G;                                                   //  48B   
    A_UINT8                       pad21[2];                                                     //   2B
                                                                                                // = 492B
    // 5G
    A_UINT8                       calFreqPier5G[WHAL_NUM_11A_CAL_PIERS];                        //   8B
    CAL_DATA_PER_FREQ_OLPC        calPierData5G[WHAL_NUM_11A_CAL_PIERS];                        // 176B
    A_UINT8                       pad3[20];														//   20B

    A_UINT8                       extTPow2xDelta5G[QC98XX_EXT_TARGET_POWER_SIZE_5G];	    	//   44B
    A_UINT8                       targetFreqbin5G[WHAL_NUM_11A_LEG_TARGET_POWER_CHANS];         //   6B
    A_UINT8                       targetFreqbin5GVHT20[WHAL_NUM_11A_20_TARGET_POWER_CHANS];     //   6B
    A_UINT8                       targetFreqbin5GVHT40[WHAL_NUM_11A_40_TARGET_POWER_CHANS];     //   6B
    A_UINT8                       targetFreqbin5GVHT80[WHAL_NUM_11A_80_TARGET_POWER_CHANS];     //   6B
    
    CAL_TARGET_POWER_LEG          targetPower5G[WHAL_NUM_11A_LEG_TARGET_POWER_CHANS];           //  24B
    CAL_TARGET_POWER_11A_20       targetPower5GVHT20[WHAL_NUM_11A_20_TARGET_POWER_CHANS];       //  72B
    CAL_TARGET_POWER_11A_40       targetPower5GVHT40[WHAL_NUM_11A_40_TARGET_POWER_CHANS];       //  72B
    CAL_TARGET_POWER_11A_80       targetPower5GVHT80[WHAL_NUM_11A_80_TARGET_POWER_CHANS];       //  72B

    A_UINT8                       ctlIndex5G[WHAL_NUM_CTLS_5G];                                 //   18B
    A_UINT8                       ctl5GFuture[2];												//   2B  
    A_UINT8                       ctlFreqbin5G[WHAL_NUM_CTLS_5G][WHAL_NUM_BAND_EDGES_5G];		//  144B
    CAL_CTL_DATA_5G               ctlData5G[WHAL_NUM_CTLS_5G];									//  144B
    NF_CAL_DATA_PER_CHIP		  nfCalData5G [ WHAL_NUM_11A_CAL_PIERS ];                       //  64B


    TEMP_COMP_TBL_5G		      tempComp5G;												    //  96B 
                                                                                                // = 980B
    // Config section (sticky writes)
    A_UINT32                      configAddr[QC98XX_CONFIG_ENTRIES];                            // 96B
    																							
} __ATTRIB_PACK QC98XX_EEPROM;    // 548 + 492 + 980 + 96 = 2116B

/* since AR6000_EEPROM may be used as a generic eeprom struct in other portions of s/w, for now keep it that way */
#define AR6000_EEPROM QC98XX_EEPROM
#define QC98XX_EEPROM_SIZE_LARGEST		(sizeof(QC98XX_EEPROM))
extern A_UINT8 Qc98xxEepromArea[QC98XX_EEPROM_SIZE_LARGEST];
extern A_UINT8 Qc98xxEepromBoardArea[QC98XX_EEPROM_SIZE_LARGEST];
//#define QC98XX_EEPROM_SIZE_NONRAM		3072                                          /* size of non-RAM section */
                                                                            
#if !defined(ATH_TARGET)
#include "athendpack.h"
#endif /* !ATH_TARGET */
#if defined(WIN32) || defined(WIN64)
#pragma pack(pop)
#endif /* WIN32 || WIN64 */

#endif /* _QC98XX_EEPROM_H_ */
