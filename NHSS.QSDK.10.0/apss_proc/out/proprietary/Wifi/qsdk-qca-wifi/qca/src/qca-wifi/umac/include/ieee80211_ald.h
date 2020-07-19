/*
* Copyright (c) 2013, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2008 Atheros Communications Inc.
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
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved. 
 * Qualcomm Atheros Confidential and Proprietary. 
 */ 

#ifndef _IEEE80211_ALD_H
#define _IEEE80211_ALD_H

typedef void * ald_netlink_t;

#if ATH_SUPPORT_HYFI_ENHANCEMENTS

/*
 * Definitions for IP differentiated services code points (DSCP)
 *
 * Taken from RFC-2597, Section 6 and RFC-2598, Section 2.3.
 */

#define IPTOS_DSCP_MASK         0xfc
#define IPTOS_DSCP_AF11         0x28
#define IPTOS_DSCP_AF12         0x30
#define IPTOS_DSCP_AF13         0x38
#define IPTOS_DSCP_AF21         0x48
#define IPTOS_DSCP_AF22         0x50
#define IPTOS_DSCP_AF23         0x58
#define IPTOS_DSCP_AF31         0x68
#define IPTOS_DSCP_AF32         0x70
#define IPTOS_DSCP_AF33         0x78
#define IPTOS_DSCP_AF41         0x88
#define IPTOS_DSCP_AF42         0x90
#define IPTOS_DSCP_AF43         0x98
#define IPTOS_DSCP_EF           0xb8

/*
 * In RFC 2474, Section 4.2.2.1, the Class Selector Codepoints subsume
 * the old ToS Precedence values.
 */

#define IPTOS_CLASS_CS0                 0x00
#define IPTOS_CLASS_CS1                 0x20
#define IPTOS_CLASS_CS2                 0x40
#define IPTOS_CLASS_CS3                 0x60
#define IPTOS_CLASS_CS4                 0x80
#define IPTOS_CLASS_CS5                 0xa0
#define IPTOS_CLASS_CS6                 0xc0
#define IPTOS_CLASS_CS7                 0xe0

#define IPTOS_LU                        0x74

#define IPTOS_CLASS_DEFAULT             IPTOS_CLASS_CS0


struct ath_ni_linkdiag {          
    int32_t ald_airtime;
    int32_t ald_pktlen;
    int32_t ald_pktnum;
    int32_t ald_count;
    int32_t ald_aggr;
    int32_t ald_phyerr;
    int32_t ald_msdusize;
    int32_t ald_retries;
    u_int32_t ald_capacity;
    u_int32_t ald_lastper;
    u_int32_t ald_txrate;
    u_int32_t ald_max4msframelen;
    u_int32_t ald_txcount;
    u_int32_t ald_avgtxrate;
    u_int32_t ald_avgmax4msaggr;
    u_int32_t ald_txqdepth;
    u_int16_t ald_ac_nobufs[WME_NUM_AC];
    u_int16_t ald_ac_excretries[WME_NUM_AC];
    u_int16_t ald_ac_txpktcnt[WME_NUM_AC];
    spinlock_t ald_lock;
};

struct ath_linkdiag {
    u_int32_t ald_phyerr;
    u_int32_t ald_ostime;
    u_int32_t ald_maxcu;
    u_int32_t msdu_size;
    u_int32_t phyerr_rate;

    int32_t ald_unicast_tx_bytes;
    int32_t ald_unicast_tx_packets;
    
    u_int32_t ald_chan_util;
    u_int32_t ald_chan_capacity;
    u_int32_t ald_dev_load;
    u_int32_t ald_txbuf_used;
    u_int32_t ald_curThroughput;

    struct ald_stat_info *staticp;
    spinlock_t ald_lock;
};

/**
 * for upper layer to call and get WLAN statistics for each link 
 * @param      vaphandle   : vap handle
 * @param      param   : handle for the return value .    
 *
 * @return : 0  on success and -ve on failure.
 *
 */
int wlan_ald_get_statistics(wlan_if_t vaphandle, void *param);
/**
 * attach ath link diag functionalities
 * @param      vap   : vap handle
 */
int ieee80211_ald_vattach(struct ieee80211vap *vap);
/**
 * deattach ath link diag functionalities
 * @param      vap   : vap handle
 */
int ieee80211_ald_vdetach(struct ieee80211vap *vap);
/**
 * collect unicast packet transmission stats and save it locally
 * @param      vap   : vap handle
 */
void ieee80211_ald_record_tx(struct ieee80211vap *vap, wbuf_t wbuf, int datalen);

#else /* ATH_SUPPORT_HYFI_ENHANCEMENTS */

#define wlan_ald_get_statistics(a, b)   do{}while(0)
#define ieee80211_ald_vattach(a) do{}while(0)
#define ieee80211_ald_vdetach(b) do{}while(0)
#define ieee80211_ald_record_tx(a, b, c) do{}while(0)

#endif /* ATH_SUPPORT_HYFI_ENHANCEMENTS */

#endif
