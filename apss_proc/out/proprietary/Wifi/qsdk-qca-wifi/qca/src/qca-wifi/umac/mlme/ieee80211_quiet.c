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

#include <ieee80211_var.h>

#if UMAC_SUPPORT_QUIET

/*
 * Add quiet information element to a frame.
 */
static u_int8_t *
ieee80211_add_quiet_ie(u_int8_t *frm, struct ieee80211vap *vap)
{
    struct ieee80211_ath_quiet_ie *quiet = (struct ieee80211_ath_quiet_ie *) frm;
    struct ieee80211_quiet_param *vap_quiet = vap->iv_quiet;

    if(!quiet)
        return frm;

    memset(quiet, 0, sizeof(struct ieee80211_ath_quiet_ie));
    quiet->ie = IEEE80211_ELEMID_QUIET;
    quiet->len = sizeof(struct ieee80211_ath_quiet_ie) - 2;
    quiet->tbttcount = vap_quiet->tbttcount;
    quiet->period = vap_quiet->period;
    quiet->duration = htole16(vap_quiet->duration);
    quiet->offset = htole16(vap_quiet->offset);
    return frm + sizeof(struct ieee80211_ath_quiet_ie);
}

/*
 * Update vap quiet parameters
 */
static void
ieee80211_quiet_param_update(struct ieee80211vap *vap, struct ieee80211com *ic)
{
    struct ieee80211_quiet_param *iv_quiet = vap->iv_quiet;
    struct ieee80211_quiet_param *ic_quiet = ic->ic_quiet;
    u_int8_t tbttcount = ic_quiet->tbttcount;
    u_int16_t offset = ic_quiet->beacon_offset + ic_quiet->offset;

    if (offset < iv_quiet->beacon_offset) {
        tbttcount = (tbttcount -1) ? (tbttcount -1) : ic_quiet->period;
        offset += ic->ic_intval - iv_quiet->beacon_offset;
    }
    else {
        if (iv_quiet->beacon_offset < ic_quiet->beacon_offset) {
            tbttcount = (tbttcount -1) ? (tbttcount -1) : ic_quiet->period;
        }
        offset -= iv_quiet->beacon_offset;
    }
    iv_quiet->tbttcount = tbttcount;
    iv_quiet->period = ic_quiet->period;
    iv_quiet->duration = ic_quiet->duration;
    iv_quiet->offset = offset;
}

static void ieee80211_iter_vap_quiet_is_enabled(void *arg, struct ieee80211vap *vap)
{
    u_int16_t *count = (u_int16_t *)arg;
    ASSERT(vap->iv_quiet);
    if (vap->iv_quiet->is_enabled)
        ++(*count);
}


/*
 * Add the quiet ie and update beacon offset
 */
u_int8_t *
ieee80211_quiet_beacon_setup(struct ieee80211vap *vap, struct ieee80211com *ic,
                        struct ieee80211_beacon_offsets *bo,
                        u_int8_t *frm)
{
    ASSERT(ic->ic_quiet);
    ASSERT(vap->iv_quiet);
    if(!vap->iv_bcn_offload_enable) {
        if (ic->ic_quiet->is_enabled &&
                vap->iv_quiet->is_enabled) {
            bo->bo_quiet = frm;
            ieee80211_quiet_param_update(vap, ic);
            frm = ieee80211_add_quiet_ie(frm, vap);
        }
    }
    return frm;
}

/*
 * update the quiet ie
 */
void
ieee80211_quiet_beacon_update(struct ieee80211vap *vap, struct ieee80211com *ic,
                               struct ieee80211_beacon_offsets *bo)
{
    ASSERT(ic->ic_quiet);
    ASSERT(vap->iv_quiet);
    if(!(vap->iv_bcn_offload_enable)) {
        if (ic->ic_quiet->is_enabled &&
                vap->iv_quiet->is_enabled &&
                bo->bo_quiet) {
            ieee80211_quiet_param_update(vap, ic);
            ieee80211_add_quiet_ie(bo->bo_quiet, vap);
        }
    }
}

/*
 * Add quiet ie
 */
u_int8_t *
ieee80211_add_quiet(struct ieee80211vap *vap, struct ieee80211com *ic, u_int8_t *frm)
{
    ASSERT(ic->ic_quiet);
    ASSERT(vap->iv_quiet);
    if (ic->ic_quiet->is_enabled &&
        vap->iv_quiet->is_enabled) {
        frm = ieee80211_add_quiet_ie(frm, vap);
    }
    return frm;
}

int ieee80211_quiet_attach(struct ieee80211com *ic)
{
    struct ieee80211_quiet_param *ic_quiet;
    if (ic->ic_quiet) {
        ASSERT(ic->ic_quiet == 0);
        return -1;
    }
    ic_quiet = (struct ieee80211_quiet_param *) OS_MALLOC(ic->ic_osdev,
                       (sizeof(struct ieee80211_quiet_param)),0);
    if (ic_quiet == NULL) {
       return -ENOMEM;
    }
    ic->ic_quiet = ic_quiet;
    OS_MEMZERO(ic_quiet, sizeof(struct ieee80211_quiet_param));
    ic_quiet->period = WLAN_QUIET_PERIOD_DEF;
    ic_quiet->tbttcount = WLAN_QUIET_PERIOD_DEF;
    ic_quiet->duration = WLAN_QUIET_DURATION_DEF;
    ic_quiet->offset = WLAN_QUIET_OFFSET_DEF;
    return 0;
}

int ieee80211_quiet_detach(struct ieee80211com *ic)
{
    if (ic->ic_quiet == NULL) {
        ASSERT(ic->ic_quiet);
        return -1; /* already detached */
    }
    OS_FREE(ic->ic_quiet);
    ic->ic_quiet = NULL;
    return 0;
}


int ieee80211_quiet_vattach(struct ieee80211vap *vap)
{
    struct ieee80211com     *ic = vap->iv_ic;
    struct ieee80211_quiet_param *iv_quiet;
    if (vap->iv_quiet) {
        ASSERT(vap->iv_quiet == 0);
        return -1; /* already attached ? */
    }
    iv_quiet = (struct ieee80211_quiet_param *) OS_MALLOC(ic->ic_osdev,
                       (sizeof(struct ieee80211_quiet_param)),0);
    if (iv_quiet == NULL) {
       return -ENOMEM;
    }
    vap->iv_quiet = iv_quiet;
    OS_MEMZERO(iv_quiet, sizeof(struct ieee80211_quiet_param));
    return 0;
}

int ieee80211_quiet_vdetach(struct ieee80211vap *vap)
{
    if (vap->iv_quiet == NULL) {
        ASSERT(vap->iv_quiet);
        return -1; /* already detached */
    }
    OS_FREE(vap->iv_quiet);
    vap->iv_quiet = NULL;
    return 0;
}

int wlan_quiet_set_param(struct ieee80211vap *vap, u_int32_t val) {
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_quiet_param *iv_quiet = vap->iv_quiet;
    struct ieee80211_quiet_param *ic_quiet = ic->ic_quiet;
    struct ieee80211_vap_opmode_count    vap_opmode_count;
    u_int16_t count = 0;
    ASSERT(iv_quiet);
    ASSERT(ic_quiet);
    if (val) {
        /* if the curret VAP is HOSTAP mode and all the
         * other VAPs also in HOSTAP then enable this feature
         */
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            OS_MEMZERO(&(vap_opmode_count), sizeof(vap_opmode_count));
            ieee80211_get_vap_opmode_count(ic, &vap_opmode_count);
            if (vap_opmode_count.total_vaps == vap_opmode_count.ap_count) {
                iv_quiet->is_enabled = val;
                ic_quiet->is_enabled = 1;
                if (vap->iv_bcn_offload_enable) {
                    ic->ic_set_bcn_offload_quiet_mode(vap, ic);
                }
	    }
            else {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                                  "Not all vaps are in HOSTAP mode total vaps %d ap vaps %d \n",
                                  vap_opmode_count.total_vaps,
                                  vap_opmode_count.ap_count);
                return -EINVAL;
            }
        }
        else {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                                  "vap mode is not HOSTAP opmode %x \n",
                                  vap->iv_opmode);
            return -EINVAL;
        }
    }
    else {
        iv_quiet->is_enabled = 0;
        if (vap->iv_bcn_offload_enable) {
            ic->ic_set_bcn_offload_quiet_mode(vap, ic);
        }
	wlan_iterate_vap_list(ic, ieee80211_iter_vap_quiet_is_enabled,
            (void *)&count);
        if (count == 0) {
            /* quiet IE is not enabled for all VAPs
             * disable quiet element support in the ic
             */
            ic_quiet->is_enabled = 0;
        }
    }
    return 0;
}

u_int32_t wlan_quiet_get_param(struct ieee80211vap *vap) {
    ASSERT(vap->iv_quiet);
    return vap->iv_quiet->is_enabled;
}

#endif /* UMAC_SUPPORT_QUIET */
