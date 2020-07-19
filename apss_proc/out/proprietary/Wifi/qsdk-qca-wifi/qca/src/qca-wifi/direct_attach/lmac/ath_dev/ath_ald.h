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

/*
 * Public Interface for ALD control module
 */

#ifndef _DEV_ATH_ALD_H
#define _DEV_ATH_ALD_H
#define DEFAULT_BUFF_LEVEL_IN_PERCENT 75
#define MIN_BUFF_LEVEL_IN_PERCENT 25

#include <ath_dev.h>

#include "ath_desc.h"
#include "ah_desc.h"

/* defines */
/* structure in ath_softc to hold statistics */
struct ath_ald_record {
    u_int16_t               sc_ald_free_buf_lvl; /* Buffer Full warning threshold */
    u_int                   sc_ald_buffull_wrn;   
};

#if ATH_SUPPORT_HYFI_ENHANCEMENTS

struct ath_ald_ac_stats{ 
    uint16_t                ac_nobufs;   /* number of tx packets lost due to no descriptor */
    uint16_t                ac_excretries;   /* number of tx packets lost due to excessive retries */
    uint16_t                ac_pktcnt;   /* tx pkt count */
};

/* Collect per link stats */
int ath_ald_collect_ni_data(ath_dev_t dev, ath_node_t an, ath_ald_t ald_data, ath_ni_ald_t ald_ni_data); 
/* collect over all WLAN stats */
int ath_ald_collect_data(ath_dev_t dev, ath_ald_t ald_data);
/* update local stats after each unicast frame transmission */
void ath_ald_update_frame_stats(struct ath_softc *sc, struct ath_buf *bf, struct ath_tx_status *ts);
//void ath_ald_update_rate_stats(struct ath_softc *sc, struct ath_node *an, u_int8_t rix);
#define ath_ald_update_nobuf_stats(_ac)    (sc->sc_aldstatsenabled ? (_ac)->ald_ac_stats.ac_nobufs++ : 0) 
#define ath_ald_update_excretry_stats(_ac)    (sc->sc_aldstatsenabled ? (_ac)->ald_ac_stats.ac_excretries++ : 0)
#define ath_ald_update_pktcnt(_ac)    (sc->sc_aldstatsenabled ? (_ac)->ald_ac_stats.ac_pktcnt++ : 0)
#else

#define ath_ald_collect_ni_data(a, b, c, d) do{}while(0)
#define ath_ald_collect_data(a, b) do{}while(0)
#define ath_ald_update_frame_stats(a, b, c)  do{}while(0)
//#define ath_ald_update_rate_stats(struct ath_softc *sc, struct ath_node *an, u_int8_t rix)
#define ath_ald_update_nobuf_stats(ac) do{}while(0)
#define ath_ald_update_excretry_stats(ac) do{}while(0)
#define ath_ald_update_pktcnt(_ac) do{}while(0)

#endif  /* ATH_SUPPORT_HYFI_ENHANCEMENTS */

#endif  /* _DEV_ATH_ALD_H */
