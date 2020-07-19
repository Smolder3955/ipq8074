/*
 * Copyright (c) 2017 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef _QC98XX_EEPROM_STRUCT_H_
#define _QC98XX_EEPROM_STRUCT_H_

typedef enum targetPowerLegacyRates {
    LEGACY_TARGET_RATE_6_24,
    LEGACY_TARGET_RATE_36,
    LEGACY_TARGET_RATE_48,
    LEGACY_TARGET_RATE_54
}TARGET_POWER_LEGACY_RATES;

typedef enum targetPowerCckRates {
    LEGACY_TARGET_RATE_1L_5L,
    LEGACY_TARGET_RATE_5S,
    LEGACY_TARGET_RATE_11L,
    LEGACY_TARGET_RATE_11S
}TARGET_POWER_CCK_RATES;

typedef enum targetPowerVHTRates {
    VHT_TARGET_RATE_0 = 0,
    VHT_TARGET_RATE_1_2,
    VHT_TARGET_RATE_3_4,
    VHT_TARGET_RATE_5,
    VHT_TARGET_RATE_6,
    VHT_TARGET_RATE_7,
    VHT_TARGET_RATE_8,
    VHT_TARGET_RATE_9,
    VHT_TARGET_RATE_10,
    VHT_TARGET_RATE_11_12,
    VHT_TARGET_RATE_13_14,
    VHT_TARGET_RATE_15,
    VHT_TARGET_RATE_16,
    VHT_TARGET_RATE_17,
    VHT_TARGET_RATE_18,
    VHT_TARGET_RATE_19,
    VHT_TARGET_RATE_20,
    VHT_TARGET_RATE_21_22,
    VHT_TARGET_RATE_23_24,
    VHT_TARGET_RATE_25,
    VHT_TARGET_RATE_26,
    VHT_TARGET_RATE_27,
    VHT_TARGET_RATE_28,
    VHT_TARGET_RATE_29,
	VHT_TARGET_RATE_LAST,
}TARGET_POWER_VHT_RATES;

#define IS_1STREAM_TARGET_POWER_VHT_RATES(x) (((x) >= VHT_TARGET_RATE_0) && ((x) <= VHT_TARGET_RATE_9))
#define IS_2STREAM_TARGET_POWER_VHT_RATES(x) (((x) >= VHT_TARGET_RATE_10 && (x) <= VHT_TARGET_RATE_19)) 
#define IS_3STREAM_TARGET_POWER_VHT_RATES(x) (((x) >= VHT_TARGET_RATE_20 && (x) <= VHT_TARGET_RATE_29) )

#define QC98XX_CTL_MODE_M            0xF
#define QC98XX_BCHAN_UNUSED          0xFF

#define RATE_PRINT_HT_SIZE			24
#define RATE_PRINT_VHT_SIZE			30

extern const char *sRatePrintHT[30];
extern const char *sRatePrintVHT[30];
extern const char *sRatePrintLegacy[4];
extern const char *sRatePrintCck[4];
extern const char *sDeviceType[];
extern const char *sCtlType[];
extern const int mapRate2Index[24];
	
#endif //_QC98XX_EEPROM_STRUCT_H_
