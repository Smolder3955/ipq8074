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
#include "ah_internal.h"

#include <string.h>
#include <stdlib.h>
#include <err.h>

struct anihandler {
	int	s;
	struct ath_diag atd;
};

static void
anicontrol(struct anihandler *ah, int op, int param)
{
	u_int32_t args[2];

	args[0] = op;
	args[1] = param;
	ah->atd.ad_id = HAL_DIAG_ANI_CMD | ATH_DIAG_IN;
	ah->atd.ad_out_data = NULL;
	ah->atd.ad_out_size = 0;
	ah->atd.ad_in_data = (caddr_t) args;
	ah->atd.ad_in_size = sizeof(args);
	if (ioctl(ah->s, SIOCGATHDIAG, &ah->atd) < 0)
		err(1, ah->atd.ad_name);
}

static void
usage(void)
{
	const char *msg = "\
Usage: ani [-i device] [cmd]\n\
noise X             set Noise Immunity Level to X [0..4]\n\
weak_detect X       set OFDM Weak Signal Detection to X [0,1]\n\
weak_threshold X    set CCK Weak Signal Threshold to X [0,1]\n\
firstep X           set FIR Step Level to X [0..2]\n\
spur X              set Spur Immunity Level to X [0..7]\n";

	fprintf(stderr, "%s", msg);
}

int
main(int argc, char *argv[])
{
#define	streq(a,b)	(strcasecmp(a,b) == 0)
	struct anihandler ani;
	HAL_REVS revs;

	memset(&ani, 0, sizeof(ani));
	ani.s = socket(AF_INET, SOCK_DGRAM, 0);
	if (ani.s < 0)
		err(1, "socket");
	if (argc > 1 && strcmp(argv[1], "-i") == 0) {
		if (argc < 2) {
			fprintf(stderr, "%s: missing interface name for -i\n",
				argv[0]);
			exit(-1);
		}
		strncpy(ani.atd.ad_name, argv[2], sizeof (ani.atd.ad_name));
		argc -= 2, argv += 2;
	} else
                strncpy(ani.atd.ad_name, ATH_DEFAULT, sizeof (ani.atd.ad_name));

	if (argc > 1) {
		if (streq(argv[1], "noise")) {
			anicontrol(&ani, HAL_ANI_NOISE_IMMUNITY_LEVEL,
				atoi(argv[2]));
		} else if (streq(argv[1], "weak_detect")) {
			anicontrol(&ani, HAL_ANI_OFDM_WEAK_SIGNAL_DETECTION,
				atoi(argv[2]));
		} else if (streq(argv[1], "weak_threshold")) {
			anicontrol(&ani, HAL_ANI_CCK_WEAK_SIGNAL_THR,
				atoi(argv[2]));
		} else if (streq(argv[1], "firstep")) {
			anicontrol(&ani, HAL_ANI_FIRSTEP_LEVEL, atoi(argv[2]));
		} else if (streq(argv[1], "spur")) {
			anicontrol(&ani, HAL_ANI_SPUR_IMMUNITY_LEVEL,
				atoi(argv[2]));
		} else {
			usage();
		}
	} else
		usage();

	return 0;
}
