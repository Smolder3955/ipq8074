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


#define ADDR_READ(addr)      (*((volatile u_int32_t *)(addr)))
#define ADDR_WRITE(addr,b)   (void)((*(volatile u_int32_t *) (addr)) = (b))

#define AR9340_SOC_GPIO_OE      0xB8040000
#define AR9340_SOC_GPIO_OUT     0xB8040008
#define AR9340_SOC_GPIO_FUN4    0xB804003c  /* GPIO_OUT_FUNCTION4 */
#define AR9340_GPIO_WOW         (1 << 16)


void ah_wow_set_gpio_hi(struct ath_hal *ah);
void ah_wow_set_gpio_lo(struct ath_hal *ah);

