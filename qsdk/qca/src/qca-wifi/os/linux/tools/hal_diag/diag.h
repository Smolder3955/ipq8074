#ifndef _DIAG_
#define	_DIAG_
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

/**
 * Fix compile issue, due to the tool compliation doesn't include -D__KERNEL__, 
 * cause some missing data type declaration...
 **/
#ifndef __KERNEL__
#define __iomem
#else

#include <linux/types.h>

#endif /* ifndef __KERNEL__ */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

#ifndef __linux__
#include <net80211/ieee80211_radiotap.h>
#endif
#include "if_athioctl.h"

#ifndef ATH_DEFAULT
#define	ATH_DEFAULT	"wifi0"
#endif

struct statshandler {
	u_long	interval;
	void	*total;
	void	*cur;

	void (*getstats)(struct statshandler *, void *);
	void (*update)(struct statshandler *);

	void (*printbanner)(struct statshandler *, FILE *);
	void (*reportdelta)(struct statshandler *, FILE *);
	void (*reporttotal)(struct statshandler *, FILE *);
	void (*reportverbose)(struct statshandler *, FILE *);
};

extern	void reportstats(FILE *fd, struct statshandler *sh);
extern	void runstats(FILE *fd, struct statshandler *sh);
extern	void reportcol(FILE *fd, u_int32_t v, const char *def_fmt,
		u_int32_t max, const char *alt_fmt);
#endif /* _DIAG_ */
