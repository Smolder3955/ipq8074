/*
 * Copyright (c) 2010, Atheros Communications Inc.
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
#ifndef _TX99_WCMD_H
#define _TX99_WCMD_H

/* Uses SIOCGATHDIAG to send these TX99 IOCTL */
#define     ATH_TX99_IOCTL_BEGIN        150
#define     ATH_TX99_IOCTL_START        150
#define     ATH_TX99_IOCTL_STOP         151
#define     ATH_TX99_IOCTL_GET          152
#define     ATH_TX99_IOCTL_SET          153
#define     ATH_TX99_IOCTL_SETCC        154
#define     ATH_TX99_IOCTL_GETCC        155
#define     ATH_TX99_IOCTL_TEST         156
#define     ATH_TX99_IOCTL_END          157

#ifndef     IFNAMSIZ
#define     IFNAMSIZ    16
#endif

typedef enum {
    TX99_WCMD_ENABLE,
    TX99_WCMD_DISABLE,
    TX99_WCMD_SET_FREQ,
    TX99_WCMD_SET_RATE,
    TX99_WCMD_SET_RC,
    TX99_WCMD_SET_POWER,
    TX99_WCMD_SET_TXMODE,
    TX99_WCMD_SET_CHANMASK,
    TX99_WCMD_SET_TYPE,
    TX99_WCMD_SET_TESTMODE,
    TX99_WCMD_GET,
    TX99_WCMD_TEST,
} tx99_wcmd_type_t;

typedef struct tx99_wcmd_data {
    u_int32_t freq;     /* tx frequency (MHz) */
    u_int32_t htmode;   /* tx bandwidth (HT20/HT40) */
    u_int32_t htext;    /* extension channel offset (0(none), 1(plus) and 2(minus)) */
    u_int32_t rate;     /* Kbits/s */
    u_int32_t rc;       /* rate code */
    u_int32_t power;    /* (dBm) */
    u_int32_t txmode;   /* wireless mode, 11NG(8), auto-select(0) */
    u_int32_t chanmask; /* tx chain mask */
    u_int32_t type;
    u_int32_t testmode; /* tx data pattern, recv only */
} tx99_wcmd_data_t;

typedef struct tx99_wcmd {
    char                if_name[IFNAMSIZ];/**< Interface name */
    tx99_wcmd_type_t    type;             /**< Type of wcmd */
    tx99_wcmd_data_t    data;             /**< Data */       
} tx99_wcmd_t;

#endif /* _TX99_WCMD_H */
