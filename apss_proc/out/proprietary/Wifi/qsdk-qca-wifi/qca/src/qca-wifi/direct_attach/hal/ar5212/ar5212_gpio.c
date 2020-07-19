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

#include "ar5212/ar5212.h"
#include "ar5212/ar5212reg.h"
#include "ar5212/ar5212phy.h"
#ifdef AH_SUPPORT_AR5311
#include "ar5212/ar5311reg.h"
#endif

/*
 * Configure GPIO Output lines
 */
HAL_BOOL
ar5212GpioCfgOutput(struct ath_hal *ah, u_int32_t gpio, HAL_GPIO_OUTPUT_MUX_TYPE halSignalType)
{
	HALASSERT(gpio < AR_NUM_GPIO);

	// AR5212 does not have a multiplexer, so it cannot use any signal
    // other than the regular GPIO output
	if (halSignalType != HAL_GPIO_OUTPUT_MUX_AS_OUTPUT)
	{
		return AH_FALSE;
	}

	// since both bits corresponding to the specified pin will always be 
	// set to 1, there's no need to clear those bits beforehand.
	OS_REG_WRITE(ah, AR_GPIOCR, 
		  OS_REG_READ(ah, AR_GPIOCR) | AR_GPIOCR_CR_A(gpio));

	return AH_TRUE;
}

/*
 * Configure GPIO Output lines-LED off
 */
HAL_BOOL
ar5212GpioCfgOutputLEDoff(struct ath_hal *ah, u_int32_t gpio, HAL_GPIO_OUTPUT_MUX_TYPE halSignalType)
{
	HALASSERT(gpio < AR_NUM_GPIO);

	// AR5212 does not have a multiplexer, so it cannot use any signal
    // other than the regular GPIO output
	if (halSignalType != HAL_GPIO_OUTPUT_MUX_AS_OUTPUT)
	{
		return AH_FALSE;
	}

	// since both bits corresponding to the specified pin will always be 
	// set to 1, there's no need to clear those bits beforehand.
	OS_REG_WRITE(ah, AR_GPIOCR, 
		  OS_REG_READ(ah, AR_GPIOCR) | AR_GPIOCR_CR_A(gpio));

	return AH_TRUE;
}

/*
 * Configure GPIO Input lines
 */
HAL_BOOL
ar5212GpioCfgInput(struct ath_hal *ah, u_int32_t gpio)
{
	HALASSERT(gpio < AR_NUM_GPIO);

	// since both bits corresponding to the specified pin will always be 
	// set to 0, there's no need to OR those bits with AR_GPIOCR_CR_N (which is 0).
	OS_REG_WRITE(ah, AR_GPIOCR, 
		  OS_REG_READ(ah, AR_GPIOCR) &~ AR_GPIOCR_CR_A(gpio));

	return AH_TRUE;
}

/*
 * Once configured for I/O - set output lines
 */
HAL_BOOL
ar5212GpioSet(struct ath_hal *ah, u_int32_t gpio, u_int32_t val)
{
	u_int32_t reg;

	HALASSERT(gpio < AR_NUM_GPIO);

	reg =  OS_REG_READ(ah, AR_GPIODO);
	reg &= ~(1 << gpio);
	reg |= (val&1) << gpio;

	OS_REG_WRITE(ah, AR_GPIODO, reg);
	return AH_TRUE;
}

/*
 * Once configured for I/O - get input lines
 */
u_int32_t
ar5212GpioGet(struct ath_hal *ah, u_int32_t gpio)
{
	if (gpio < AR_NUM_GPIO) {
		u_int32_t val = OS_REG_READ(ah, AR_GPIODI);
		val = ((val & AR_GPIOD_MASK) >> gpio) & 0x1;
		return val;
	} else  {
		return 0xffffffff;
	}
}

/*
 * Set the GPIO Interrupt
 */
void
ar5212GpioSetIntr(struct ath_hal *ah, u_int gpio, u_int32_t ilevel)
{
	u_int32_t val;

	/* XXX bounds check gpio */
	val = OS_REG_READ(ah, AR_GPIOCR);
	val &= ~(AR_GPIOCR_CR_A(gpio) |
		 AR_GPIOCR_INT_MASK | AR_GPIOCR_INT_ENA | AR_GPIOCR_INT_SEL);
	val |= AR_GPIOCR_CR_N(gpio) | AR_GPIOCR_INT(gpio) | AR_GPIOCR_INT_ENA;
	if (ilevel)
		val |= AR_GPIOCR_INT_SELH;	/* interrupt on pin high */
	else
		val |= AR_GPIOCR_INT_SELL;	/* interrupt on pin low */

	/* Don't need to change anything for low level interrupt. */
	OS_REG_WRITE(ah, AR_GPIOCR, val);

	/* Change the interrupt mask. */
	(void) ar5212SetInterrupts(ah, AH5212(ah)->ah_maskReg | HAL_INT_GPIO, 0);
}

#endif  /* AH_SUPPORT_AR5212 */
