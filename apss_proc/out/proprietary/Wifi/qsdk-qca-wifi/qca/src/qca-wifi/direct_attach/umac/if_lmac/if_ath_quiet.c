/*
* Copyright (c) 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

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

#if UMAC_SUPPORT_QUIET

#include "if_athvar.h"

/* Function to update ic quiet parameters and 
 * set and reset HW quiet period accordingly
 */
void
ath_quiet_update(struct ieee80211com *ic, struct ieee80211vap *vap)
{
    struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    struct ieee80211_quiet_param *iv_quiet = vap->iv_quiet;
    struct ieee80211_quiet_param *ic_quiet = ic->ic_quiet;

    ASSERT(ic_quiet);
    ASSERT(iv_quiet);

    /* Update ic quiet params for the vap with beacon offset 0 */
    if(ic_quiet->is_enabled) {
        u_int32_t tsf_adj, param2;
        if (scn->sc_ops->ath_vap_info_get(scn->sc_dev, vap->iv_unit,
                                          ATH_VAP_INFOTYPE_SLOT_OFFSET, &tsf_adj, &param2) != EOK) {
            DPRINTF(scn,ATH_DEBUG_ANY,"%s: failed to get vap info \n", __func__);
        }
        /* convert tsf adjust in to TU */
        tsf_adj = tsf_adj >> 10;

        /* Compute the beacon_offset from slot 0 */
        if (tsf_adj) {
            iv_quiet->beacon_offset = ic->ic_intval - tsf_adj;
        }
        else {
            iv_quiet->beacon_offset = 0;
        }

        if (vap->iv_unit == 0) {
            ic_quiet->beacon_offset = iv_quiet->beacon_offset;
            if (ic_quiet->tbttcount == 1) {
                /* Reset the quiet count */
                ic_quiet->tbttcount = ic_quiet->period;
            }
            else {
                ic_quiet->tbttcount--;
            }
            if (ic_quiet->tbttcount == 1) {
                /* enable the quiet time in the HW */
                scn->sc_ops->set_quiet(scn->sc_dev,
                    ic_quiet->period * ic->ic_intval, /* in TU */
                    ic_quiet->duration, /* in TU */
                    ic_quiet->offset + ic_quiet->tbttcount*ic->ic_intval,
                    (HAL_QUIET_ENABLE | HAL_QUIET_ADD_CURRENT_TSF | 
                     HAL_QUIET_ADD_SWBA_RESP_TIME));
            }
            else {
                if (ic_quiet->tbttcount == ic_quiet->period - 1) {
                    /* disable quiet time in the HW */
                    scn->sc_ops->set_quiet(scn->sc_dev,
                        ic_quiet->period * ic->ic_intval, /* in TU */
                        ic_quiet->duration, /* in TU */
                        ic_quiet->offset + ic_quiet->tbttcount*ic->ic_intval,
                        HAL_QUIET_DISABLE);
                }
            }
        }
    } else if (ic_quiet->tbttcount != ic_quiet->period) {
        /* quiet support is disabled
         * the hw quiet period was set before
         * so just disable the hw quiet period and 
         * reset the tbttcount
         */
         ic_quiet->tbttcount = ic_quiet->period;
         /* disable quiet time in the HW */
         scn->sc_ops->set_quiet(scn->sc_dev,
             ic_quiet->period * ic->ic_intval, /* in TU */
             ic_quiet->duration, /* in TU */
             ic_quiet->offset + ic_quiet->tbttcount*ic->ic_intval,
             HAL_QUIET_DISABLE);
    }
}

#endif  /* UMAC_SUPPORT_QUIET */
