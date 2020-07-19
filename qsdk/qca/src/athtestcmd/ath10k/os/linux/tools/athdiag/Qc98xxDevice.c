/*
 *  Copyright (c) 2014 Qualcomm Atheros, Inc.  All rights reserved. 
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners. 
 */


#include <errno.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <athtypes_linux.h>
#include "qc98xx_eeprom.h"



#include "Qc98xxEepromStructGet.h"

#include "rate_constants.h"
#include "vrate_constants.h"
#include "Qc98xxEepromStruct.h"

#include "sw_version.h"

#ifdef QDART_BUILD
#include "qmslCmd.h"
#define TEST_WITHOUT_CHIP 1
#endif

#define MDCU 10			// should we only set the first 8??
#define MQCU 10
        
#define BW_AUTOMATIC (0)
#define BW_HT40_PLUS (40)
#define BW_HT40_MINUS (-40)
#define BW_HT20 (20)
#define BW_OFDM (19)
#define BW_HALF (10)
#define BW_QUARTER (5)

#define HAL_HT_MACMODE_20       0           /* 20 MHz operation */
#define HAL_HT_MACMODE_2040     1           /* 20/40 MHz operation */

#define QC98XX_DEFAULT_NAME		"QCA ART2 for Peregrine(AR9888)"

A_UINT8 *pQc9kEepromArea = NULL;
A_UINT8 *pQc9kEepromBoardArea = NULL;

A_UINT8 Qc98xxEepromArea[sizeof(QC98XX_EEPROM)];
A_UINT8 Qc98xxEepromBoardArea[sizeof(QC98XX_EEPROM)];



#if AP_BUILD
extern void Qc98xx_swap_eeprom(QC98XX_EEPROM *eep);
#endif

#define MCHANNEL 2000

int Qc98xxTargetPowerGet(int frequency, int rate, double *power)
{
    A_BOOL  is2GHz = 0;
    A_UINT8 powerT2 = 0;

    if(frequency < 4000) {
        is2GHz = 1;
    }
    //call the relevant get target power file based on rateIndex.  
    //Rate index will be used to find the appropriate target power index
    switch (rate) {
        //Legacy
        case RATE_INDEX_6:
        case RATE_INDEX_9:
        case RATE_INDEX_12:
        case RATE_INDEX_18:
        case RATE_INDEX_24:
            powerT2 = Qc98xxEepromGetLegacyTrgtPwr(LEGACY_TARGET_RATE_6_24, frequency, is2GHz);            
            break;
        case RATE_INDEX_36:
            powerT2 = Qc98xxEepromGetLegacyTrgtPwr(LEGACY_TARGET_RATE_36, frequency, is2GHz);            
            break;
        case RATE_INDEX_48:
            powerT2 = Qc98xxEepromGetLegacyTrgtPwr(LEGACY_TARGET_RATE_48, frequency, is2GHz);            
            break;
        case RATE_INDEX_54:
            powerT2 = Qc98xxEepromGetLegacyTrgtPwr(LEGACY_TARGET_RATE_54, frequency, is2GHz);            
            break;

        //Legacy CCK
        case RATE_INDEX_1L:
        case RATE_INDEX_2L:
        case RATE_INDEX_2S:
        case RATE_INDEX_5L:
            powerT2 = Qc98xxEepromGetCckTrgtPwr(LEGACY_TARGET_RATE_1L_5L, frequency);
            break;
        case RATE_INDEX_5S:
            powerT2 = Qc98xxEepromGetCckTrgtPwr(LEGACY_TARGET_RATE_5S, frequency);
            break;
        case RATE_INDEX_11L:
            powerT2 = Qc98xxEepromGetCckTrgtPwr(LEGACY_TARGET_RATE_11L, frequency);
            break;
        case RATE_INDEX_11S:
            powerT2 = Qc98xxEepromGetCckTrgtPwr(LEGACY_TARGET_RATE_11S, frequency);
            break;
        
        //HT20
        case RATE_INDEX_HT20_MCS0:
        case RATE_INDEX_HT20_MCS8:
        case RATE_INDEX_HT20_MCS16:
		case vRATE_INDEX_HT20_MCS0:
		case vRATE_INDEX_HT20_MCS10:
		case vRATE_INDEX_HT20_MCS20:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20, frequency, is2GHz);
            break;
        case RATE_INDEX_HT20_MCS1:
        case RATE_INDEX_HT20_MCS2:
        case RATE_INDEX_HT20_MCS9:
        case RATE_INDEX_HT20_MCS10:
        case RATE_INDEX_HT20_MCS17:
        case RATE_INDEX_HT20_MCS18:
		case vRATE_INDEX_HT20_MCS1:
		case vRATE_INDEX_HT20_MCS2:
		case vRATE_INDEX_HT20_MCS11:
		case vRATE_INDEX_HT20_MCS12:
		case vRATE_INDEX_HT20_MCS21:
		case vRATE_INDEX_HT20_MCS22:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22, frequency, is2GHz);     
            break;
        case RATE_INDEX_HT20_MCS3:
        case RATE_INDEX_HT20_MCS4:
        case RATE_INDEX_HT20_MCS11:
        case RATE_INDEX_HT20_MCS12:
        case RATE_INDEX_HT20_MCS19:
        case RATE_INDEX_HT20_MCS20:
		case vRATE_INDEX_HT20_MCS3:
		case vRATE_INDEX_HT20_MCS4:
		case vRATE_INDEX_HT20_MCS13:
		case vRATE_INDEX_HT20_MCS14:
		case vRATE_INDEX_HT20_MCS23:
		case vRATE_INDEX_HT20_MCS24:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24, frequency, is2GHz);
            break;
        case RATE_INDEX_HT20_MCS5:
		case vRATE_INDEX_HT20_MCS5:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS5, frequency, is2GHz);
            break;
        case RATE_INDEX_HT20_MCS6:
		case vRATE_INDEX_HT20_MCS6:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS6, frequency, is2GHz);
            break;
        case RATE_INDEX_HT20_MCS7:
		case vRATE_INDEX_HT20_MCS7:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS7, frequency, is2GHz);
            break;
        case vRATE_INDEX_HT20_MCS8:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS8, frequency, is2GHz);
            break;
        case vRATE_INDEX_HT20_MCS9:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS9, frequency, is2GHz);
            break;
        case RATE_INDEX_HT20_MCS13:
		case vRATE_INDEX_HT20_MCS15:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS15, frequency, is2GHz);
            break;
        case RATE_INDEX_HT20_MCS14:
 		case vRATE_INDEX_HT20_MCS16:
           powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS16, frequency, is2GHz);
            break;
        case RATE_INDEX_HT20_MCS15:
		case vRATE_INDEX_HT20_MCS17:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS17, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT20_MCS18:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS18, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT20_MCS19:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS19, frequency, is2GHz);
            break;
        case RATE_INDEX_HT20_MCS21:
		case vRATE_INDEX_HT20_MCS25:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS25, frequency, is2GHz);
            break;
        case RATE_INDEX_HT20_MCS22:
 		case vRATE_INDEX_HT20_MCS26:
           powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS26, frequency, is2GHz);
            break;
        case RATE_INDEX_HT20_MCS23:
		case vRATE_INDEX_HT20_MCS27:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS27, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT20_MCS28:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS28, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT20_MCS29:
            powerT2 = Qc98xxEepromGetHT20TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS29, frequency, is2GHz);
            break;
        
        //HT40
        case RATE_INDEX_HT40_MCS0:
        case RATE_INDEX_HT40_MCS8:
        case RATE_INDEX_HT40_MCS16:
		case vRATE_INDEX_HT40_MCS0:
		case vRATE_INDEX_HT40_MCS10:
		case vRATE_INDEX_HT40_MCS20:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20, frequency, is2GHz);
            break;
        case RATE_INDEX_HT40_MCS1:
        case RATE_INDEX_HT40_MCS2:
        case RATE_INDEX_HT40_MCS9:
        case RATE_INDEX_HT40_MCS10:
        case RATE_INDEX_HT40_MCS17:
        case RATE_INDEX_HT40_MCS18:
		case vRATE_INDEX_HT40_MCS1:
		case vRATE_INDEX_HT40_MCS2:
		case vRATE_INDEX_HT40_MCS11:
		case vRATE_INDEX_HT40_MCS12:
		case vRATE_INDEX_HT40_MCS21:
		case vRATE_INDEX_HT40_MCS22:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22, frequency, is2GHz);     
            break;
        case RATE_INDEX_HT40_MCS3:
        case RATE_INDEX_HT40_MCS4:
        case RATE_INDEX_HT40_MCS11:
        case RATE_INDEX_HT40_MCS12:
        case RATE_INDEX_HT40_MCS19:
        case RATE_INDEX_HT40_MCS20:
		case vRATE_INDEX_HT40_MCS3:
		case vRATE_INDEX_HT40_MCS4:
		case vRATE_INDEX_HT40_MCS13:
		case vRATE_INDEX_HT40_MCS14:
		case vRATE_INDEX_HT40_MCS23:
		case vRATE_INDEX_HT40_MCS24:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24, frequency, is2GHz);
            break;
        case RATE_INDEX_HT40_MCS5:
		case vRATE_INDEX_HT40_MCS5:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS5, frequency, is2GHz);
            break;
        case RATE_INDEX_HT40_MCS6:
		case vRATE_INDEX_HT40_MCS6:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS6, frequency, is2GHz);
            break;
        case RATE_INDEX_HT40_MCS7:
		case vRATE_INDEX_HT40_MCS7:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS7, frequency, is2GHz);
            break;
        case vRATE_INDEX_HT40_MCS8:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS8, frequency, is2GHz);
            break;
        case vRATE_INDEX_HT40_MCS9:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS9, frequency, is2GHz);
            break;
        case RATE_INDEX_HT40_MCS13:
		case vRATE_INDEX_HT40_MCS15:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS15, frequency, is2GHz);
            break;
        case RATE_INDEX_HT40_MCS14:
 		case vRATE_INDEX_HT40_MCS16:
           powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS16, frequency, is2GHz);
            break;
        case RATE_INDEX_HT40_MCS15:
		case vRATE_INDEX_HT40_MCS17:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS17, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT40_MCS18:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS18, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT40_MCS19:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS19, frequency, is2GHz);
            break;
        case RATE_INDEX_HT40_MCS21:
		case vRATE_INDEX_HT40_MCS25:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS25, frequency, is2GHz);
            break;
        case RATE_INDEX_HT40_MCS22:
 		case vRATE_INDEX_HT40_MCS26:
           powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS26, frequency, is2GHz);
            break;
        case RATE_INDEX_HT40_MCS23:
		case vRATE_INDEX_HT40_MCS27:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS27, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT40_MCS28:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS28, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT40_MCS29:
            powerT2 = Qc98xxEepromGetHT40TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS29, frequency, is2GHz);
            break;

		//HT80
		case vRATE_INDEX_HT80_MCS0:
		case vRATE_INDEX_HT80_MCS10:
		case vRATE_INDEX_HT80_MCS20:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS0_10_20, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT80_MCS1:
		case vRATE_INDEX_HT80_MCS2:
		case vRATE_INDEX_HT80_MCS11:
		case vRATE_INDEX_HT80_MCS12:
		case vRATE_INDEX_HT80_MCS21:
		case vRATE_INDEX_HT80_MCS22:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS1_2_11_12_21_22, frequency, is2GHz);     
            break;
		case vRATE_INDEX_HT80_MCS3:
		case vRATE_INDEX_HT80_MCS4:
		case vRATE_INDEX_HT80_MCS13:
		case vRATE_INDEX_HT80_MCS14:
		case vRATE_INDEX_HT80_MCS23:
		case vRATE_INDEX_HT80_MCS24:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS3_4_13_14_23_24, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT80_MCS5:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS5, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT80_MCS6:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS6, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT80_MCS7:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS7, frequency, is2GHz);
            break;
        case vRATE_INDEX_HT80_MCS8:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS8, frequency, is2GHz);
            break;
        case vRATE_INDEX_HT80_MCS9:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS9, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT80_MCS15:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS15, frequency, is2GHz);
            break;
 		case vRATE_INDEX_HT80_MCS16:
           powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS16, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT80_MCS17:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS17, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT80_MCS18:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS18, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT80_MCS19:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS19, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT80_MCS25:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS25, frequency, is2GHz);
            break;
 		case vRATE_INDEX_HT80_MCS26:
           powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS26, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT80_MCS27:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS27, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT80_MCS28:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS28, frequency, is2GHz);
            break;
		case vRATE_INDEX_HT80_MCS29:
            powerT2 = Qc98xxEepromGetHT80TrgtPwr(rate, WHAL_VHT_TARGET_POWER_RATES_MCS29, frequency, is2GHz);
            break;
		
		default:
			printf("Qc98xxTargetPowerGet - Invalid rate index %d\n", rate);
			return -1;
    }
    *power = ((double)powerT2)/2;
    return 0;
}

//int Qc98xxTargetPowerSetFunction(/* arguments????*/)

int Qc98xxIs2GHz (void)
{
	return ((Qc98xxEepromStructGet()->baseEepHeader.opCapBrdFlags.opFlags & WHAL_OPFLAGS_11G) > 0 ? 1 : 0);
}



//=======================================================================



#define LinkDllName "LinkQc9K"

//
// clear all device control function pointers and set to default behavior
//
//
// clear all device control function pointers and set to default behavior
//

