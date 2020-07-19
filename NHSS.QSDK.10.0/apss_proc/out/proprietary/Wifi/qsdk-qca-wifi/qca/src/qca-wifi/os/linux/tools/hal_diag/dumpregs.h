#ifndef _DUMPREGS_
#define	_DUMPREGS_
/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
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

/* 
 * Copyright (c) 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved. 
 * Qualcomm Atheros Confidential and Proprietary. 
 * Notifications and licenses are retained for attribution purposes only.
 */ 

extern	u_int32_t regdata[0xffff / sizeof(u_int32_t)];
#undef OS_REG_READ
#define	OS_REG_READ(ah, off)	regdata[(off) >> 2]

typedef struct {
	const char*	label;
	u_int		reg;
} HAL_REG;

enum {
	DUMP_BASIC	= 0x0001,	/* basic/default registers */
	DUMP_KEYCACHE	= 0x0002,	/* key cache */
	DUMP_BASEBAND	= 0x0004,	/* baseband */
	DUMP_INTERRUPT	= 0x0008,	/* interrupt state */
	DUMP_XR		= 0x0010,	/* XR state */
	DUMP_QCU	= 0x0020,	/* QCU state */
	DUMP_DCU	= 0x0040,	/* DCU state */
	DUMP_LA		= 0x0080,	/* MAC/PCU logic analyzer register space */
	DUMP_DMADBG	= 0x0100,	/* DMA debug registers */
	DUMP_PUBLIC	= 0x0061,	/* public = BASIC+QCU+DCU */
	DUMP_ALL	= 0xffff
};

extern	u_int ath_hal_setupdiagregs(const HAL_REGRANGE regs[], u_int nr);
extern	void ath_hal_dumprange(FILE *fd, u_int a, u_int b);
extern	void ath_hal_dumpregs(FILE *fd, const HAL_REG regs[], u_int nregs);
extern	void ath_hal_dumpkeycache(FILE *fd, int nkeys, int micEnabled);
#endif /* _DUMPREGS_ */
