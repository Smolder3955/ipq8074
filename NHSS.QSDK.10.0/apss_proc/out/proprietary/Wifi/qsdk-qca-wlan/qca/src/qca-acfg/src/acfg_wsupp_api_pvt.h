/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
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

#ifndef __ACFG_WSUPP_API_PVT_H
#define __ACFG_WSUPP_API_PVT_H

/* defaults*/
#define WSUPP_DEFAULT_IFNAME "ath0"

#if 1
#define dbgprintf(lvl, fmt, args...) do { \
    if (lvl >= dbg_lvl) { \
        printf("%s(%d): " fmt "\n", __func__, __LINE__, ## args); \
    } \
} while (0)
#else
#define dbgprintf(args...) do { } while (0)
#endif

/* constants */
enum { MSG_MSGDUMP, MSG_DEBUG, MSG_INFO, MSG_WARNING, MSG_ERROR };

/* structures */
typedef struct acfg_wsupp {
    uint8_t unique[ACFG_WSUPP_UNIQUE_LEN];
    uint8_t ifname[ACFG_MAX_IFNAME];
} acfg_wsupp_t;

#endif
