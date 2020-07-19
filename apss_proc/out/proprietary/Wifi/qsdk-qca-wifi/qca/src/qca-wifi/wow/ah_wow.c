/*
 *  Copyright (c) 2010 Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "ah.h"
#include "ah_internal.h"
#include "ar9300/ar9300reg.h"
#include "ah_wow.h"


void ah_wow_set_gpio_hi(struct ath_hal *ah)
{
    qdf_print("Set GPIO_16 High...");
    
    if (AR_SREV_WASP(ah)) {
        /* set ENABLE_GPIO_16[7:0] to 0x0, disable function pin */
        ADDR_WRITE(AR9340_SOC_GPIO_FUN4, (ADDR_READ(AR9340_SOC_GPIO_FUN4) & ~(0xFF)));
        
        /* 0:enable the output driver. 1:use as input */
        ADDR_WRITE(AR9340_SOC_GPIO_OE, ( ADDR_READ(AR9340_SOC_GPIO_OE) & ~AR9340_GPIO_WOW ));
        ADDR_WRITE(AR9340_SOC_GPIO_OUT, ( ADDR_READ(AR9340_SOC_GPIO_OUT) | AR9340_GPIO_WOW ));
    }
}

void ah_wow_set_gpio_lo(struct ath_hal *ah)
{
    qdf_print("Set GPIO_16 Low...");
    
    if (AR_SREV_WASP(ah)) {
        /* set ENABLE_GPIO_16[7:0] to 0x0, disable function pin */
        ADDR_WRITE(AR9340_SOC_GPIO_FUN4, (ADDR_READ(AR9340_SOC_GPIO_FUN4) & ~(0xFF)));

        /* 0:enable the output driver. 1:use as input */
        ADDR_WRITE(AR9340_SOC_GPIO_OE, ( ADDR_READ(AR9340_SOC_GPIO_OE) & ~AR9340_GPIO_WOW ));
        ADDR_WRITE(AR9340_SOC_GPIO_OUT, ( ADDR_READ(AR9340_SOC_GPIO_OUT) & ~AR9340_GPIO_WOW ));
    }
}

