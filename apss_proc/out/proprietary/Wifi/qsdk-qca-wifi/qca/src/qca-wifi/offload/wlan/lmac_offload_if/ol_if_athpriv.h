/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
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

/*
 * Defintions for the Atheros Wireless LAN controller driver.
 */
#ifndef _DEV_OL_ATH_PRIV_H
#define _DEV_OL_ATH_PRIV_H

#include <osdep.h>
#include <a_types.h>
#include <a_osapi.h>
#include "ieee80211_channel.h"
#include "ieee80211_proto.h"
#include "ieee80211_rateset.h"
#include "ieee80211_regdmn.h"
#include "ieee80211_wds.h"
#include "ieee80211_node.h"
#include "ieee80211_objmgr_priv.h"

void ol_ath_setup_rates(struct ieee80211com *ic);
void ol_ath_vht_rate_setup(struct ieee80211com *ic, u_int32_t mcs_map,
                             u_int16_t max_datarate, u_int16_t basic_mcs);

int
ol_ath_node_set_param(struct ol_ath_softc_net80211 *scn, u_int8_t *peer_addr,u_int32_t param_id,
        u_int32_t param_val,u_int32_t vdev_id);

void
ol_ath_node_update(struct ieee80211_node *ni);

static inline void
ol_bytestream_endian_fix(u_int32_t *addr, int num_words)
{
    int i;
    u_int8_t *p;

    /*
     * In a big-endian host, the bytes within each u_int32_t word will
     * be automatically swapped as a WMI command is downloaded to the
     * target, or as a WMI event is uploaded from the target.
     * This fixes endianness problems for u_int32_t mesage fields.
     * However, any bytestream data within the message also gets
     * byte-swapped, which make it out of order on the receiving side.
     * This function "pre-distorts" an array of bytes by swapping them,
     * to cancel out the additional automatic byteswap during download
     * or upload.
     */
    p = (u_int8_t *)addr;
    for (i = 0; i < num_words; i++) {
        u_int8_t tmp;
        tmp = p[0];
        p[0] = p[3];
        p[3] = tmp;

        tmp = p[1];
        p[1] = p[2];
        p[2] = tmp;

        p += 4;
    }
}


#endif
