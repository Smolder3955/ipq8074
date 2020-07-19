/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
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

#ifdef AH_SUPPORT_AR5416

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"
#ifdef AH_DEBUG
#include "ah_desc.h"                    /* NB: for HAL_PHYERR* */
#endif

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"

#define AR_GPIOD_MASK                           0x00001FFF      /* GPIO data reg r/w mask */
#define AR_GPIO_BIT(_gpio)                      (1 << (_gpio))

/*
 * Configure GPIO Output Mux control
 */
static void
ar5416GpioCfgOutputMux(struct ath_hal *ah, u_int32_t gpio, u_int32_t type)
{
    int          addr;
    u_int32_t    gpio_shift, tmp;

    // each MUX controls 6 GPIO pins
    if (gpio > 11) {
        addr = AR_GPIO_OUTPUT_MUX3;
    } else if (gpio > 5) {
        addr = AR_GPIO_OUTPUT_MUX2;
    } else {
        addr = AR_GPIO_OUTPUT_MUX1;
    }

    // 5 bits per GPIO pin. Bits 0..4 for 1st pin in that mux, bits 5..9 for 2nd pin, etc.
    gpio_shift = (gpio % 6) * 5;

    /*
     * From Owl to Merlin 1.0, the value read from MUX1 bit 4 to bit 9 are wrong.
     * Here is hardware's coding:
     * PRDATA[4:0] <= gpio_output_mux[0];
     * PRDATA[9:4] <= gpio_output_mux[1]; <==== Bit 4 is used by both gpio_output_mux[0] [1].
     * Currently the max value for gpio_output_mux[] is 6. So bit 4 will never be used.
     * So it should be fine that bit 4 won't be able to recover.
     */

    if (AR_SREV_MERLIN_20_OR_LATER(ah) || (addr != AR_GPIO_OUTPUT_MUX1)) {
        OS_REG_RMW(ah, addr, (type << gpio_shift), (0x1f << gpio_shift));
    }
    else {
        tmp = OS_REG_READ(ah, addr);
        tmp = ((tmp & 0x1F0) << 1) | (tmp & ~0x1F0);
        tmp &= ~(0x1f << gpio_shift);
        tmp |= (type << gpio_shift);
        OS_REG_WRITE(ah, addr, tmp);
    }
}

/*
 * Configure GPIO Output lines
 */
HAL_BOOL
ar5416GpioCfgOutput(struct ath_hal *ah, u_int32_t gpio, HAL_GPIO_OUTPUT_MUX_TYPE halSignalType)
{
#define N(a)    (sizeof(a)/sizeof(a[0]))
    u_int32_t    ah_signal_type;
    u_int32_t    gpio_shift;

    static const u_int32_t    MuxSignalConversionTable[] = {
        /* HAL_GPIO_OUTPUT_MUX_AS_OUTPUT             */ AR_GPIO_OUTPUT_MUX_AS_OUTPUT,
        /* HAL_GPIO_OUTPUT_MUX_AS_PCIE_ATTENTION_LED */ AR_GPIO_OUTPUT_MUX_AS_PCIE_ATTENTION_LED,
        /* HAL_GPIO_OUTPUT_MUX_AS_PCIE_POWER_LED     */ AR_GPIO_OUTPUT_MUX_AS_PCIE_POWER_LED,
        /* HAL_GPIO_OUTPUT_MUX_AS_MAC_NETWORK_LED    */ AR_GPIO_OUTPUT_MUX_AS_MAC_NETWORK_LED,
        /* HAL_GPIO_OUTPUT_MUX_AS_MAC_POWER_LED      */ AR_GPIO_OUTPUT_MUX_AS_MAC_POWER_LED,
        /* HAL_GPIO_OUTPUT_MUX_AS_WLAN_ACTIVE        */ AR_GPIO_OUTPUT_MUX_AS_RX_CLEAR_EXTERNAL,
        /* HAL_GPIO_OUTPUT_MUX_AS_TX_FRAME           */ AR_GPIO_OUTPUT_MUX_AS_TX_FRAME,
    };

    HALASSERT(gpio < AH_PRIVATE(ah)->ah_caps.hal_num_gpio_pins);

    // Convert HAL signal type definitions to hardware-specific values.
    if (halSignalType < N(MuxSignalConversionTable))
    {
        ah_signal_type = MuxSignalConversionTable[halSignalType];
    }
    else
    {
        return AH_FALSE;
    }

    // Configure the MUX
    ar5416GpioCfgOutputMux(ah, gpio, ah_signal_type);

    // 2 bits per output mode
    gpio_shift = 2*gpio;

    OS_REG_RMW(ah, 
               AR_GPIO_OE_OUT, 
               (AR_GPIO_OE_OUT_DRV_ALL << gpio_shift), 
               (AR_GPIO_OE_OUT_DRV << gpio_shift));

    return AH_TRUE;
#undef N
}

/*
 * Configure GPIO Output lines - LED Off
 */
HAL_BOOL
ar5416GpioCfgOutputLEDoff(struct ath_hal *ah, u_int32_t gpio, HAL_GPIO_OUTPUT_MUX_TYPE halSignalType)
{
#define N(a)    (sizeof(a)/sizeof(a[0]))
    u_int32_t    ah_signal_type;
    u_int32_t    gpio_shift;

    static const u_int32_t    MuxSignalConversionTable[] = {
        /* HAL_GPIO_OUTPUT_MUX_AS_OUTPUT             */ AR_GPIO_OUTPUT_MUX_AS_OUTPUT,
        /* HAL_GPIO_OUTPUT_MUX_AS_PCIE_ATTENTION_LED */ AR_GPIO_OUTPUT_MUX_AS_PCIE_ATTENTION_LED,
        /* HAL_GPIO_OUTPUT_MUX_AS_PCIE_POWER_LED     */ AR_GPIO_OUTPUT_MUX_AS_PCIE_POWER_LED,
        /* HAL_GPIO_OUTPUT_MUX_AS_MAC_NETWORK_LED    */ AR_GPIO_OUTPUT_MUX_AS_MAC_NETWORK_LED,
        /* HAL_GPIO_OUTPUT_MUX_AS_MAC_POWER_LED      */ AR_GPIO_OUTPUT_MUX_AS_MAC_POWER_LED,
        /* HAL_GPIO_OUTPUT_MUX_AS_WLAN_ACTIVE        */ AR_GPIO_OUTPUT_MUX_AS_RX_CLEAR_EXTERNAL,
        /* HAL_GPIO_OUTPUT_MUX_AS_TX_FRAME           */ AR_GPIO_OUTPUT_MUX_AS_TX_FRAME,
    };

    HALASSERT(gpio < AH_PRIVATE(ah)->ah_caps.hal_num_gpio_pins);

    // Convert HAL signal type definitions to hardware-specific values.
    if ((halSignalType >= 0) && (halSignalType < N(MuxSignalConversionTable)))
    {
        ah_signal_type = MuxSignalConversionTable[halSignalType];
    }
    else
    {
        return AH_FALSE;
    }

    // Configure the MUX
    ar5416GpioCfgOutputMux(ah, gpio, ah_signal_type);

    // 2 bits per output mode
    gpio_shift = 2*gpio;

    OS_REG_RMW(ah, 
               AR_GPIO_OE_OUT, 
               (AR_GPIO_OE_OUT_DRV_NO << gpio_shift), 
               (AR_GPIO_OE_OUT_DRV << gpio_shift));

    return AH_TRUE;
#undef N
}

/*
 * Configure GPIO Input lines
 */
HAL_BOOL
ar5416GpioCfgInput(struct ath_hal *ah, u_int32_t gpio)
{
    u_int32_t    gpio_shift;

    HALASSERT(gpio < AH_PRIVATE(ah)->ah_caps.hal_num_gpio_pins);

    /* TODO: configure input mux for AR5416 */
    /* If configured as input, set output to tristate */
    gpio_shift = 2*gpio;

    OS_REG_RMW(ah, 
               AR_GPIO_OE_OUT, 
               (AR_GPIO_OE_OUT_DRV_NO << gpio_shift), 
               (AR_GPIO_OE_OUT_DRV << gpio_shift));

    return AH_TRUE;
}

/*
 * Once configured for I/O - set output lines
 */
HAL_BOOL
ar5416GpioSet(struct ath_hal *ah, u_int32_t gpio, u_int32_t val)
{
    HALASSERT(gpio < AH_PRIVATE(ah)->ah_caps.hal_num_gpio_pins);

    OS_REG_RMW(ah, AR_GPIO_IN_OUT, ((val&1) << gpio), AR_GPIO_BIT(gpio));

    return AH_TRUE;
}

/*
 * Once configured for I/O - get input lines
 */
u_int32_t
ar5416GpioGet(struct ath_hal *ah, u_int32_t gpio)
{
    HALASSERT(gpio < AH_PRIVATE(ah)->ah_caps.hal_num_gpio_pins);

    if (gpio >= AH_PRIVATE(ah)->ah_caps.hal_num_gpio_pins) {
        return 0xffffffff;
    }

    // Read output value for all gpio's, shift it left, and verify whether a 
    // specific gpio bit is set.
    if(AR_SREV_K2(ah)) {
        u_int32_t val;
         
        val = OS_REG_READ(ah, AR_GPIO_IN_OUT);
        if (val == (u_int32_t)(-1)) {
            return 0;
        } else {
            return (MS(val, AR9271_GPIO_IN_VAL) & AR_GPIO_BIT(gpio)) != 0;
        }
    }
    else if (AR_SREV_KIWI_10_OR_LATER(ah)) {
        return (MS(OS_REG_READ(ah, AR_GPIO_IN_OUT), AR9287_GPIO_IN_VAL) & AR_GPIO_BIT(gpio)) != 0;
    } else if (AR_SREV_KITE_10_OR_LATER(ah)) {
        return (MS(OS_REG_READ(ah, AR_GPIO_IN_OUT), AR9285_GPIO_IN_VAL) & AR_GPIO_BIT(gpio)) != 0;
    } else if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        return (MS(OS_REG_READ(ah, AR_GPIO_IN_OUT), AR928X_GPIO_IN_VAL) & AR_GPIO_BIT(gpio)) != 0;
    } else {
        return (MS(OS_REG_READ(ah, AR_GPIO_IN_OUT), AR_GPIO_IN_VAL) & AR_GPIO_BIT(gpio)) != 0;
    }
}

/*
 * Set the GPIO Interrupt
 * Sync and Async interrupts are both set/cleared. Async GPIO interrupts may not be raised
 * when the chip is put to sleep.
 */
void
ar5416GpioSetIntr(struct ath_hal *ah, u_int gpio, u_int32_t ilevel)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t val, mask;


    HALASSERT(gpio < AH_PRIVATE(ah)->ah_caps.hal_num_gpio_pins);


	if (ilevel == HAL_GPIO_INTR_DISABLE) {
        val = MS(OS_REG_READ(ah, AR_INTR_ASYNC_ENABLE), AR_INTR_ASYNC_ENABLE_GPIO);
        val &= ~AR_GPIO_BIT(gpio);
        OS_REG_RMW_FIELD(ah, AR_INTR_ASYNC_ENABLE, AR_INTR_ASYNC_ENABLE_GPIO, val);

        mask = MS(OS_REG_READ(ah, AR_INTR_ASYNC_MASK), AR_INTR_ASYNC_MASK_GPIO);
        mask &= ~AR_GPIO_BIT(gpio);
        OS_REG_RMW_FIELD(ah, AR_INTR_ASYNC_MASK, AR_INTR_ASYNC_MASK_GPIO, mask);

        /* Clear synchronous GPIO interrupt registers and pending interrupt flag */
        val = MS(OS_REG_READ(ah, AR_INTR_SYNC_ENABLE), AR_INTR_SYNC_ENABLE_GPIO);
        val &= ~AR_GPIO_BIT(gpio);
        OS_REG_RMW_FIELD(ah, AR_INTR_SYNC_ENABLE, AR_INTR_SYNC_ENABLE_GPIO, val);

        mask = MS(OS_REG_READ(ah, AR_INTR_SYNC_MASK), AR_INTR_SYNC_MASK_GPIO);
        mask &= ~AR_GPIO_BIT(gpio);
        OS_REG_RMW_FIELD(ah, AR_INTR_SYNC_MASK, AR_INTR_SYNC_MASK_GPIO, mask);

        val = MS(OS_REG_READ(ah, AR_INTR_SYNC_CAUSE), AR_INTR_SYNC_ENABLE_GPIO);
        val |= AR_GPIO_BIT(gpio);
        OS_REG_RMW_FIELD(ah, AR_INTR_SYNC_CAUSE, AR_INTR_SYNC_ENABLE_GPIO, val);

    } else {
		val = MS(OS_REG_READ(ah, AR_GPIO_INTR_POL), AR_GPIO_INTR_POL_VAL);
		if (ilevel == HAL_GPIO_INTR_HIGH) {
			/* 0 == interrupt on pin high */
			val &= ~AR_GPIO_BIT(gpio);
		} else if (ilevel == HAL_GPIO_INTR_LOW) {
			/* 1 == interrupt on pin low */
			val |= AR_GPIO_BIT(gpio);
		}
		OS_REG_RMW_FIELD(ah, AR_GPIO_INTR_POL, AR_GPIO_INTR_POL_VAL, val);

		/* Change the interrupt mask. */
		val = MS(OS_REG_READ(ah, AR_INTR_ASYNC_ENABLE), AR_INTR_ASYNC_ENABLE_GPIO);
		val |= AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_ASYNC_ENABLE, AR_INTR_ASYNC_ENABLE_GPIO, val);

		mask = MS(OS_REG_READ(ah, AR_INTR_ASYNC_MASK), AR_INTR_ASYNC_MASK_GPIO);
		mask |= AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_ASYNC_MASK, AR_INTR_ASYNC_MASK_GPIO, mask);

        /* Set synchronous GPIO interrupt registers as well */
		val = MS(OS_REG_READ(ah, AR_INTR_SYNC_ENABLE), AR_INTR_SYNC_ENABLE_GPIO);
		val |= AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_SYNC_ENABLE, AR_INTR_SYNC_ENABLE_GPIO, val);

		mask = MS(OS_REG_READ(ah, AR_INTR_SYNC_MASK), AR_INTR_SYNC_MASK_GPIO);
		mask |= AR_GPIO_BIT(gpio);
		OS_REG_RMW_FIELD(ah, AR_INTR_SYNC_MASK, AR_INTR_SYNC_MASK_GPIO, mask);
	}

	/* Save GPIO interrupt mask */
    HALASSERT(mask == val);
    ahp->ah_gpioMask = mask;
}

#endif  /* AH_SUPPORT_AR5416 */
