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
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5212

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"
#ifdef AH_DEBUG
#include "ah_desc.h"			/* NB: for HAL_PHYERR* */
#endif
#include "ah_eeprom.h"

#include "ar5212/ar5212.h"
#include "ar5212/ar5212reg.h"
#include "ar5212/ar5212phy.h"
#ifdef AH_SUPPORT_AR5311
#include "ar5212/ar5311reg.h"
#endif


#define AR5212_EEPROM_S
#define AR5212_EEPROM_OFFSET        0x2000
#ifdef AR9100
#define AR5212_EEPROM_START_ADDR    0x1fff1000
#else
#define AR5212_EEPROM_START_ADDR    0xb07a1000
#endif
#define AR5212_EEPROM_MAX           0xae0


/*
 * Read 16 bits of data from offset into *data
 */
HAL_BOOL
ar5212EepromRead(struct ath_hal *ah, u_int off, u_int16_t *data)
{
	OS_REG_WRITE(ah, AR_EEPROM_ADDR, off);
	OS_REG_WRITE(ah, AR_EEPROM_CMD, AR_EEPROM_CMD_READ);

	if (!ath_hal_wait(ah, AR_EEPROM_STS,
	    AR_EEPROM_STS_READ_COMPLETE | AR_EEPROM_STS_READ_ERROR,
	    AR_EEPROM_STS_READ_COMPLETE, AH_WAIT_TIMEOUT)) {
		HDPRINTF(ah, HAL_DBG_EEPROM, "%s: read failed for entry 0x%x\n", __func__, off);
		return AH_FALSE;
	}
	*data = OS_REG_READ(ah, AR_EEPROM_DATA) & 0xffff;
	return AH_TRUE;
}

#ifdef AH_SUPPORT_WRITE_EEPROM
/*
 * Write 16 bits of data from data to the specified EEPROM offset.
 */
HAL_BOOL
ar5212EepromWrite(struct ath_hal *ah, u_int off, u_int16_t data)
{
    u_int32_t status;
    int to = 15000;     /* 15ms timeout */
    /* Add Nala's Mac Version (2006/10/30) */
    u_int32_t reg = 0;

    if (AH_PRIVATE(ah)->ah_is_pci_express) {
        /* Set GPIO 3 to output and then to 0 (ACTIVE_LOW) */
        if(IS_2425(ah)) {
            reg = OS_REG_READ(ah, AR_GPIOCR);
            OS_REG_WRITE(ah, AR_GPIOCR, 0xFFFF);
        } else {
            ar5212GpioCfgOutput(ah, 3, HAL_GPIO_OUTPUT_MUX_AS_OUTPUT);
            ar5212GpioSet(ah, 3, 0);
        }
        
    } else if (IS_2413(ah) || IS_5413(ah)) {
        /* Set GPIO 4 to output and then to 0 (ACTIVE_LOW) */
        ar5212GpioCfgOutput(ah, 4, HAL_GPIO_OUTPUT_MUX_AS_OUTPUT);
        ar5212GpioSet(ah, 4, 0);
    } else if (IS_2417(ah)){
        /* Add for Nala but not sure (2006/10/30) */
        reg = OS_REG_READ(ah, AR_GPIOCR);
        OS_REG_WRITE(ah, AR_GPIOCR, 0xFFFF);    
    }

    /* Send write data */
    OS_REG_WRITE(ah, AR_EEPROM_ADDR, off);
    OS_REG_WRITE(ah, AR_EEPROM_DATA, data);
    OS_REG_WRITE(ah, AR_EEPROM_CMD, AR_EEPROM_CMD_WRITE);

    while (to > 0) {
        OS_DELAY(1);
        status = OS_REG_READ(ah, AR_EEPROM_STS);
        if (status & AR_EEPROM_STS_WRITE_COMPLETE) {
            if (AH_PRIVATE(ah)->ah_is_pci_express) {
                if(IS_2425(ah)) {
                    OS_REG_WRITE(ah, AR_GPIOCR, reg);
                } else {
                    ar5212GpioSet(ah, 3, 1);
                }
            } else if (IS_2413(ah) || IS_5413(ah)) {
                ar5212GpioSet(ah, 4, 1);
            } else if (IS_2417(ah)){
                /* Add for Nala but not sure (2006/10/30) */
                OS_REG_WRITE(ah, AR_GPIOCR, reg);
            }
            return AH_TRUE;
        }

        if (status & AR_EEPROM_STS_WRITE_ERROR)    {
		    HDPRINTF(ah, HAL_DBG_EEPROM, "%s: write failed for entry 0x%x, data 0x%x\n",
			    __func__, off, data);
            return AH_FALSE;;
        }
        to--;
    }

	HDPRINTF(ah, HAL_DBG_EEPROM, "%s: write timeout for entry 0x%x, data 0x%x\n",
		__func__, off, data);

    if (AH_PRIVATE(ah)->ah_is_pci_express) {
        if(IS_2425(ah)) {
            OS_REG_WRITE(ah, AR_GPIOCR, reg);
        } else {
            ar5212GpioSet(ah, 3, 1);
        }
    } else if (IS_2413(ah) || IS_5413(ah)) {
        ar5212GpioSet(ah, 4, 1);
    } else if (IS_2417(ah)){
        /* Add for Nala but not sure (2006/10/30) */
        OS_REG_WRITE(ah, AR_GPIOCR, reg);
    }

    return AH_FALSE;
}
#endif /* AH_SUPPORT_WRITE_EEPROM */

#ifndef WIN32
/*************************
 * Flash APIs for AP only
 *************************/

HAL_STATUS
ar5212FlashMap(struct ath_hal *ah)
{
    struct ath_hal_5212 *ahp = AH5212(ah);

    #ifdef AR9100
        ahp->ah_cal_mem = OS_REMAP(
            ah, AR5212_EEPROM_START_ADDR, AR5212_EEPROM_MAX);
    #else
        ahp->ah_cal_mem = OS_REMAP(
            (uintptr_t) AR5212_EEPROM_START_ADDR, AR5212_EEPROM_MAX);
    #endif

    if (!ahp->ah_cal_mem) {
        HDPRINTF(ah, HAL_DBG_EEPROM,
            "%s: cannot remap eeprom region \n", __func__);
        return HAL_EIO;
    }

    return HAL_OK;
}

HAL_BOOL
ar5212FlashRead(struct ath_hal *ah, u_int off, u_int16_t *data)
{
    struct ath_hal_5212 *ahp = AH5212(ah);

    *data = ((u_int16_t *)ahp->ah_cal_mem)[off];
    return AH_TRUE;

}

HAL_BOOL
ar5212FlashWrite(struct ath_hal *ah, u_int off, u_int16_t data)
{
    struct ath_hal_5212 *ahp = AH5212(ah);

    ((u_int16_t *)ahp->ah_cal_mem)[off] = data;
    return AH_TRUE;
}
#endif /* WIN32 */

#endif /* AH_SUPPORT_AR5212 */
