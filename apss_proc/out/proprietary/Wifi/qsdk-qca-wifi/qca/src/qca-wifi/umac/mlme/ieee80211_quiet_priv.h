/*
* Copyright (c) 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2011 Atheros Communications Inc.
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

#ifndef _IEEE80211_QUIET_PRIV_H
#define _IEEE80211_QUIET_PRIV_H

#if UMAC_SUPPORT_QUIET

/*
 * External Definitions
 */
u_int8_t *
ieee80211_quiet_beacon_setup(struct ieee80211vap *vap, struct ieee80211com *ic,
                        struct ieee80211_beacon_offsets *bo,
                        u_int8_t *frm);

void
ieee80211_quiet_beacon_update(struct ieee80211vap *vap, struct ieee80211com *ic,
                               struct ieee80211_beacon_offsets *bo);


u_int8_t *
ieee80211_add_quiet(struct ieee80211vap *vap, struct ieee80211com *ic, u_int8_t *frm);
#else

#define ieee80211_quiet_beacon_setup(vap, ic, bo, frm)      (frm)
#define ieee80211_quiet_beacon_update(vap, ic, bo)          /**/
#define ieee80211_add_quiet(vap, ic, frm)                   (frm)

#endif /* UMAC_SUPPORT_QUIET */

#endif /* _IEEE80211_QUIET_PRIV_H */
