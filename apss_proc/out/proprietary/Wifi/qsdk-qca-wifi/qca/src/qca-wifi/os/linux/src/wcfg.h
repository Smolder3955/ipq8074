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
/*
 * Copyright (c) 2016,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#ifndef _WCFG_H_
#define _WCFG_H_

/*
 * This file consists of definitions
 * common to both host and target and used
 * while servicing requests issued by wlanconfig.
*/

#include<acfg_api_types.h>

#define LINUX_PVT_DEV_START         (SIOCDEVPRIVATE + 0)
#define LINUX_PVT_DEV_END           (SIOCDEVPRIVATE + 15)
#define LINUX_PVT_DEV_SZ            16 /*0xf*/

/**
 * @brief WIFI specific IOCTL numbers
 */
#define LINUX_PVT_GETATH_STATS      (SIOCDEVPRIVATE + 0)
#define LINUX_PVT_CLRATH_STATS      (SIOCDEVPRIVATE + 4)
#define LINUX_PVT_PHYERR            (SIOCDEVPRIVATE + 5)
#define LINUX_PVT_VAP_CREATE        (SIOCDEVPRIVATE + 7)
#define LINUX_PVT_TX99TOOL          (SIOCDEVPRIVATE + 13)


/**
 * @brief VAP specific IOCTL numbers
 */
#define LINUX_PVT_SET_VENDORPARAM   (SIOCDEVPRIVATE + 0)
#define LINUX_PVT_GET_VENDORPARAM   (SIOCDEVPRIVATE + 1)
#define LINUX_PVT_GETKEY            (SIOCDEVPRIVATE + 3)
#define LINUX_PVT_GETWPAIE          (SIOCDEVPRIVATE + 4)
#define LINUX_PVT_GETSTA_STATS      (SIOCDEVPRIVATE + 5)
#define LINUX_PVT_GETSTA_INFO       (SIOCDEVPRIVATE + 6)
#define LINUX_PVT_VAP_DELETE        (SIOCDEVPRIVATE + 8)
#define LINUX_PVT_GETSCAN_RESULTS   (SIOCDEVPRIVATE + 9)
#define LINUX_PVT_CONFIG_NAWDS      (SIOCDEVPRIVATE + 12)
#define LINUX_PVT_P2P_BIG_PARAM     (SIOCDEVPRIVATE + 14)

#define LINUX_PVT_GETCHAN_INFO      (SIOCIWFIRSTPRIV + 7)
#define LINUX_PVT_GETCHAN_LIST      (SIOCIWFIRSTPRIV + 13)
/*
 * Buffer size to store
 * station info.
 */
#define WCFG_STA_INFO_SPACE     (24*1024)
#define WCFG_CHAN_INFO_BUFLEN     (1022)
#define WCFG_CHAN_LIST_BUFLEN     (32)

#define WCFG_RATE_MAXSIZE       36
#define WCFG_RATE_VAL           0x7F

/*
 * Station information block; the mac address is used
 * to retrieve other data like stats, unicast key, etc.
*/
struct wcfg_sta_info {
    u_int16_t   isi_len;        /* length (mult of 4) */
    u_int16_t   isi_freq;       /* MHz */
    u_int64_t   isi_flags;      /* channel flags */
    u_int16_t   isi_state;      /* state flags */
    u_int8_t    isi_authmode;       /* authentication algorithm */
    int8_t          isi_rssi;
    u_int16_t   isi_capinfo;        /* capabilities */
    u_int8_t    isi_athflags;       /* Atheros capabilities */
    u_int8_t    isi_erp;        /* ERP element */
    u_int8_t    isi_macaddr[ACFG_MACADDR_LEN];
    u_int8_t    isi_nrates;
                                /* negotiated rates */
    u_int8_t    isi_rates[WCFG_RATE_MAXSIZE];
    u_int8_t    isi_txrate;     /* index to isi_rates[] */
    u_int32_t   isi_txratekbps; /* rate in Kbps, for 11n */
    u_int16_t   isi_ie_len;     /* IE length */
    u_int16_t   isi_associd;        /* assoc response */
    u_int16_t   isi_txpower;        /* current tx power */
    u_int16_t   isi_vlan;       /* vlan tag */
    u_int16_t   isi_txseqs[17];     /* seq to be transmitted */
    u_int16_t   isi_rxseqs[17];     /* seq previous for qos frames*/
    u_int16_t   isi_inact;      /* inactivity timer */
    u_int8_t    isi_uapsd;      /* UAPSD queues */
    u_int8_t    isi_opmode;     /* sta operating mode */
    u_int8_t    isi_cipher;
    u_int32_t       isi_assoc_time;         /* sta association time */
    u_int16_t   isi_htcap;      /* HT capabilities */
    u_int32_t   isi_rxratekbps; /* rx rate in Kbps */
                                /* We use this as a common variable for legacy rates
                                   and lln. We do not attempt to make it symmetrical
                                   to isi_txratekbps and isi_txrate, which seem to be
                                   separate due to legacy code. */
    u_int8_t    isi_maxrate_per_client; /* Max rate per client */
    /* XXX frag state? */
    /* variable length IE data */
};



#endif // _WCFG_H_


