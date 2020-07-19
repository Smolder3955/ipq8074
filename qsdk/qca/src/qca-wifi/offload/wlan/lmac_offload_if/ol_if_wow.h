/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2011, Atheros Communications Inc.
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

#ifndef _OL_IF_WOW
#define _OL_IF_WOW 

/*
 * Structure and defines for WoW
 */

#if ATH_WOW

#define MAX_PATTERN_SIZE 256
#define MAX_NUM_PATTERN                   8
#define MAX_NUM_USER_PATTERN              6  /* Deducting the disassociate/deauthenticate packets */

/*
 * TODO This may not be the right place is the event definitions are shared
 * with firmware.
 */
#define ATH_WAKE_UP_PATTERN_MATCH       0x00000001
#define ATH_WAKE_UP_MAGIC_PACKET        0x00000002
#define ATH_WAKE_UP_LINK_CHANGE         0x00000004
#define ATH_WAKE_UP_BEACON_MISS         0x00000008
#define ATH_WOW_NLO_DISCOVERY           0x00000010  /* SW WOW offload */
#define ATH_WOW_AP_ASSOCIATION_LOST     0x00000020  /* SW WOW offload */
#define ATH_WOW_GTK_HANDSHAKE_ERROR     0x00000040  /* SW WOW offload */
#define ATH_WAKE_UP_4WAY_HANDSHAKE      0x00000080  /* SW WOW offload */
#define ATH_WOW_GTK_OFFLOAD             0x00000100  /* SW WOW offload */
#define ATH_WOW_ARP_OFFLOAD             0x00000200  /* SW WOW offload */
#define ATH_WOW_NS_OFFLOAD              0x00000400  /* SW WOW offload */
#define ATH_WAKE_UP_ACER_MAGIC_PACKET   0x00001000  /* SW WOW offload */
#define ATH_WAKE_UP_ACER_KEEP_ALIVE     0x00002000  /* SW WOW offload */

#define ATH_WAKE_UP_PATTERN_DISASSOC        0x10
#define ATH_WAKE_UP_PATTERN_DEAUTHED        0x20


typedef struct ol_wowPattern {
    u_int32_t valid;
    u_int8_t  patternBytes[MAX_PATTERN_SIZE];
    u_int8_t  maskBytes[MAX_PATTERN_SIZE];
    u_int32_t patternId;
    u_int32_t patternLen;
} OL_WOW_PATTERN;

struct ol_wow_info {
    u_int32_t   wakeUpEvents; //Values passed in OID_PNP_ENABLE_WAKE_UP
    u_int32_t   numOfPatterns;
    u_int32_t   wowWakeupReason;
    u_int32_t   intrMaskBeforeSleep;
    OL_WOW_PATTERN patterns[MAX_NUM_PATTERN];
    u_int8_t    chipPatternBytes[MAX_PATTERN_SIZE];
    u_int16_t   wowDuration;    
    u_int32_t   timeoutinsecs;
};

int ol_ath_wow_soc_attach(ol_ath_soc_softc_t *soc);
void ol_ath_wow_soc_detach(ol_ath_soc_softc_t *soc);
int ol_ath_wow_attach(struct ieee80211com *ic);
int ol_ath_wow_detach(struct ieee80211com *ic);

#else  /* ATH_WOW */

#define ol_ath_wow_soc_attach(soc) /**/
#define ol_ath_wow_soc_detach(soc) /**/
#define ol_ath_wow_attach(_ic) /**/
#define ol_ath_wow_detach(_ic) /**/
#endif /* ATH_WOW */

#endif /* _OL_IF_WOW */
