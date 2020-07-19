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

#include "diag.h"

#include "ah.h"
#include "ah_devid.h"
#include "ah_internal.h"

#include "dumpregs.h"

#include <getopt.h>

struct	ath_diag atd;
int	s;
u_int32_t regdata[0xffff / sizeof(u_int32_t)];
#undef OS_REG_READ
#define	OS_REG_READ(ah, off)	regdata[(off) >> 2]
#define isclr(a,i) (((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)
#define setbit(a,i) ((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define isset(a,i) ((a)[(i)/NBBY] & (1<<((i)%NBBY)))

extern	void ar5212SetupRegs(struct ath_diag *, int);
extern	void ar5212DumpRegs(FILE *fd, int what);
extern	void ar5416SetupRegs(struct ath_diag *, int);
extern	void ar5416DumpRegs(FILE *fd, int what);
extern	void ar9300SetupRegs(struct ath_diag *, int);
extern	void ar9300DumpRegs(FILE *fd, int what);

static void
usage(void)
{
	fprintf(stderr, "usage: diag [-I interface] [-abkilpx]\n");
	exit(-1);
}

int
main(int argc, char *argv[])
{
	HAL_REVS revs;
	u_int32_t *data;
	u_int32_t *dp, *ep;
	int what, c;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		err(1, "socket");
	strncpy(atd.ad_name, ATH_DEFAULT, sizeof (atd.ad_name));
	what = 0;
	while ((c = getopt(argc, argv, "I:abdgkilpqx")) != -1)
		switch (c) {
		case 'a':
			what |= DUMP_ALL;
			break;
		case 'b':
			what |= DUMP_BASEBAND;
			break;
		case 'd':
			what |= DUMP_DCU;
			break;
		case 'k':
			what |= DUMP_KEYCACHE;
			break;
		case 'i':
			what |= DUMP_INTERRUPT;
			break;
		case 'I':
			strncpy(atd.ad_name, optarg, sizeof (atd.ad_name));
			break;
		case 'p':
			what |= DUMP_PUBLIC;
			break;
		case 'q':
			what |= DUMP_QCU;
			break;
		case 'x':
			what |= DUMP_XR;
			break;
		case 'l':
			what |= DUMP_LA;
			break;
		case 'g':
			what |= DUMP_DMADBG;
			break;

		default:
			usage();
			/*NOTREACHED*/
		}
	argc -= optind;
	argv += optind;
	if (what == 0)
		what = DUMP_BASIC;

	atd.ad_id = HAL_DIAG_REVS;
	atd.ad_out_data = (caddr_t) &revs;
	atd.ad_out_size = sizeof(revs);
	if (ioctl(s, SIOCGATHDIAG, &atd) < 0)
		err(1, atd.ad_name);

	switch (revs.ah_devid) {
	case AR5212_FPGA:
	case AR5212_DEVID:
	case AR5212_DEVID_IBM:
	case AR5212_DEFAULT:
		ar5212SetupRegs(&atd, what);
		break;
	case AR5416_DEVID_PCI:
	case AR5416_DEVID_PCIE:
	case AR5416_DEVID_AR9160_PCI:
	case AR5416_DEVID_AR9280_PCI:
	case AR5416_DEVID_AR9280_PCIE:
	case AR5416_DEVID_AR9285_PCIE:
		ar5416SetupRegs(&atd, what);
		break;
	case AR9300_DEVID_AR9380_PCIE:
    case AR9300_DEVID_AR9580_PCIE:
    case AR9300_DEVID_AR9340:
		ar9300SetupRegs(&atd, what);
		break;
	}
	atd.ad_out_size = ath_hal_setupdiagregs((HAL_REGRANGE *) atd.ad_in_data,
		atd.ad_in_size / sizeof(HAL_REGRANGE));
	atd.ad_out_data = (caddr_t) malloc(atd.ad_out_size);
	if (atd.ad_out_data == NULL) {
		fprintf(stderr, "Cannot malloc output buffer, size %u\n",
			atd.ad_out_size);
		exit(-1);
	}
	atd.ad_id = HAL_DIAG_REGS | ATH_DIAG_IN | ATH_DIAG_DYN;
	if (ioctl(s, SIOCGATHDIAG, &atd) < 0)
		err(1, atd.ad_name);

	/*
	 * Expand register data into global space that can be
	 * indexed directly by register offset.
	 */
	dp = (u_int32_t *)atd.ad_out_data;
	ep = (u_int32_t *)(atd.ad_out_data + atd.ad_out_size);
	while (dp < ep) {
		u_int r = dp[0] >> 16;		/* start of range */
		u_int e = dp[0] & 0xffff;	/* end of range */
		dp++;
		/* convert offsets to indices */
		r >>= 2; e >>= 2;
		do {
			if (dp >= ep) {
				fprintf(stderr, "Warning, botched return data;"
					"register at offset 0x%x not present\n",
					r << 2);
				break;
			}
			regdata[r++] = *dp++;
		} while (r <= e);
	} 

	switch (revs.ah_devid) {
	case AR5212_FPGA:
	case AR5212_DEVID:
	case AR5212_DEVID_IBM:
	case AR5212_DEFAULT:
		ar5212DumpRegs(stdout, what);
		break;
	case AR5416_DEVID_PCI:
	case AR5416_DEVID_PCIE:
	case AR5416_DEVID_AR9160_PCI:
	case AR5416_DEVID_AR9280_PCI:
	case AR5416_DEVID_AR9280_PCIE:
	case AR5416_DEVID_AR9285_PCIE:
		ar5416DumpRegs(stdout, what);
		break;
	case AR9300_DEVID_AR9380_PCIE:
    case AR9300_DEVID_AR9580_PCIE:
    case AR9300_DEVID_AR9340:
		ar9300DumpRegs(stdout, what);
		break;
	}
	return 0;
}

void
ath_hal_dumprange(FILE *fd, u_int a, u_int b)
{
	u_int r;

	for (r = a; r+16 <= b; r += 5*4)
		fprintf(fd,
			"%04x %08x  %04x %08x  %04x %08x  %04x %08x  %04x %08x\n"
			, r, OS_REG_READ(ah, r)
			, r+4, OS_REG_READ(ah, r+4)
			, r+8, OS_REG_READ(ah, r+8)
			, r+12, OS_REG_READ(ah, r+12)
			, r+16, OS_REG_READ(ah, r+16)
		);
	switch (b-r) {
	case 16:
		fprintf(fd
			, "%04x %08x  %04x %08x  %04x %08x  %04x %08x\n"
			, r, OS_REG_READ(ah, r)
			, r+4, OS_REG_READ(ah, r+4)
			, r+8, OS_REG_READ(ah, r+8)
			, r+12, OS_REG_READ(ah, r+12)
		);
		break;
	case 12:
		fprintf(fd, "%04x %08x  %04x %08x  %04x %08x\n"
			, r, OS_REG_READ(ah, r)
			, r+4, OS_REG_READ(ah, r+4)
			, r+8, OS_REG_READ(ah, r+8)
		);
		break;
	case 8:
		fprintf(fd, "%04x %08x  %04x %08x\n"
			, r, OS_REG_READ(ah, r)
			, r+4, OS_REG_READ(ah, r+4)
		);
		break;
	case 4:
		fprintf(fd, "%04x %08x\n"
			, r, OS_REG_READ(ah, r)
		);
		break;
	}
}

void
ath_hal_dumpregs(FILE *fd, const HAL_REG regs[], u_int nregs)
{
	int i;

	for (i = 0; i+3 < nregs; i += 4)
		fprintf(fd,
			"%-8s %08x  %-8s %08x  %-8s %08x  %-8s %08x\n"
			, regs[i+0].label, OS_REG_READ(ah, regs[i+0].reg)
			, regs[i+1].label, OS_REG_READ(ah, regs[i+1].reg)
			, regs[i+2].label, OS_REG_READ(ah, regs[i+2].reg)
			, regs[i+3].label, OS_REG_READ(ah, regs[i+3].reg)
		);
	switch (nregs - i) {
	case 3:
		fprintf(fd, "%-8s %08x  %-8s %08x  %-8s %08x\n"
			, regs[i+0].label, OS_REG_READ(ah, regs[i+0].reg)
			, regs[i+1].label, OS_REG_READ(ah, regs[i+1].reg)
			, regs[i+2].label, OS_REG_READ(ah, regs[i+2].reg)
		);
		break;
	case 2:
		fprintf(fd, "%-8s %08x  %-8s %08x\n"
			, regs[i+0].label, OS_REG_READ(ah, regs[i+0].reg)
			, regs[i+1].label, OS_REG_READ(ah, regs[i+1].reg)
		);
		break;
	case 1:
		fprintf(fd, "%-8s %08x\n"
			, regs[i+0].label, OS_REG_READ(ah, regs[i+0].reg)
		);
		break;
	}
}

u_int
ath_hal_setupdiagregs(const HAL_REGRANGE regs[], u_int nr)
{
	u_int space;
	int i;

	space = 0;
	for (i = 0; i < nr; i++) {
		u_int n = 2 * sizeof(u_int32_t);	/* reg range + first */
		if (regs[i].end) {
			if (regs[i].end < regs[i].start) {
				fprintf(stderr, "%s: bad register range, "
					"end 0x%x < start 0x%x\n",
					__func__, regs[i].end, regs[i].end);
				exit(-1);
			}
			n += regs[i].end - regs[i].start;
		}
		space += n;
	}
	return space;
}

/*
 * Format an Ethernet MAC for printing.
 */
const char*
ether_sprintf(const u_int8_t *mac)
{
	static char etherbuf[18];
	snprintf(etherbuf, sizeof(etherbuf), "%02x:%02x:%02x:%02x:%02x:%02x",
		mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	return etherbuf;
}

/* XXX cheat, 5212 has a superset of the key table defs */
#include "ar5212/ar5212reg.h"

void
ath_hal_dumpkeycache(FILE *fd, int nkeys, int micEnabled)
{
	static const char *keytypenames[] = {
		"WEP-40", 	/* AR_KEYTABLE_TYPE_40 */
		"WEP-104",	/* AR_KEYTABLE_TYPE_104 */
		"#2",
		"WEP-128",	/* AR_KEYTABLE_TYPE_128 */
		"TKIP",		/* AR_KEYTABLE_TYPE_TKIP */
		"AES-OCB",	/* AR_KEYTABLE_TYPE_AES */
		"AES-CCM",	/* AR_KEYTABLE_TYPE_CCM */
		"CLR",		/* AR_KEYTABLE_TYPE_CLR */
	};
	u_int8_t mac[IEEE80211_ADDR_LEN];
	u_int8_t ismic[128/NBBY];
	int entry;
	int first = 1;

	memset(ismic, 0, sizeof(ismic));
	for (entry = 0; entry < nkeys; entry++) {
		u_int32_t macLo, macHi, type;
		u_int32_t key0, key1, key2, key3, key4;

        type = OS_REG_READ(ah, AR_KEYTABLE_TYPE(entry));
		macHi = OS_REG_READ(ah, AR_KEYTABLE_MAC1(entry));
		if ((macHi & AR_KEYTABLE_VALID) == 0 && isclr(ismic, entry) && (type & 0x7))
			continue;
		macLo = OS_REG_READ(ah, AR_KEYTABLE_MAC0(entry));
		macHi <<= 1;
		if (macLo & (1<<31))
			macHi |= 1;
		macLo <<= 1;
		mac[4] = macHi & 0xff;
		mac[5] = macHi >> 8;
		mac[0] = macLo & 0xff;
		mac[1] = macLo >> 8;
		mac[2] = macLo >> 16;
		mac[3] = macLo >> 24;
		type = OS_REG_READ(ah, AR_KEYTABLE_TYPE(entry));
		if ((type & 0x7) == AR_KEYTABLE_TYPE_TKIP && micEnabled)
			setbit(ismic, entry+64);
		key0 = OS_REG_READ(ah, AR_KEYTABLE_KEY0(entry));
		key1 = OS_REG_READ(ah, AR_KEYTABLE_KEY1(entry));
		key2 = OS_REG_READ(ah, AR_KEYTABLE_KEY2(entry));
		key3 = OS_REG_READ(ah, AR_KEYTABLE_KEY3(entry));
		key4 = OS_REG_READ(ah, AR_KEYTABLE_KEY4(entry));
		if (first) {
			fprintf(fd, "\n");
			first = 0;
		}
		fprintf(fd, "KEY[%03u] MAC %s %-7s %02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x\n"
			, entry
			, ether_sprintf(mac)
			, isset(ismic, entry) ? "MIC" : keytypenames[type & 7]
			, (key0 >>  0) & 0xff
			, (key0 >>  8) & 0xff
			, (key0 >> 16) & 0xff
			, (key0 >> 24) & 0xff
			, (key1 >>  0) & 0xff
			, (key1 >>  8) & 0xff
			, (key2 >>  0) & 0xff
			, (key2 >>  8) & 0xff
			, (key2 >> 16) & 0xff
			, (key2 >> 24) & 0xff
			, (key3 >>  0) & 0xff
			, (key3 >>  8) & 0xff
			, (key4 >>  0) & 0xff
			, (key4 >>  8) & 0xff
			, (key4 >> 16) & 0xff
			, (key4 >> 24) & 0xff
		);
	}
}
