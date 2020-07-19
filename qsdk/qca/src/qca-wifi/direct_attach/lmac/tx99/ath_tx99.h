/*
 * Copyright (c) 2013, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
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
/*
 * 2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _ATH_TX99_H
#define _ATH_TX99_H

enum ath_cwm_mode {
    ATH_CWM_MODE20,
    ATH_CWM_MODE2040,
    ATH_CWM_MODE40,
    ATH_CWM_MODEMAX
};

struct ath_tx99 {
	void		(*stop)(struct ath_softc *, int flag);
	int         (*start)(struct ath_softc *);

	u_int32_t	tx99_state;
    u_int32_t	type;			/* single carrier or frame type */
    u_int32_t	txpower;		/* tx power */
    u_int32_t	txrate;			/* tx rate */
    u_int32_t	txrc;			/* tx rate code */
    u_int32_t	txfreq;			/* tx channel frequency (ieee) */
    u_int32_t	txmode;			/* tx channel mode, e.g. 11b */
    u_int32_t   testmode;       /* tx data pattern, recv only */
    u_int32_t   phymode;
    u_int32_t	htmode;			/* channel width mode, e.g. HT20, HT40 */
    u_int32_t	htext;			/* extension channel for HT40 */
    u_int32_t	chanmask;		/* tx chainmask */
    qdf_nbuf_t  skb;            /* packet buffer */

    /**
     * Fields for continuous receive
     */
    int32_t (*prev_hard_start)(wbuf_t wbuf, struct ieee80211com *ic);
    int32_t (*prev_mgt_start)(struct ieee80211com *ic, wbuf_t wbuf);

    u_int32_t	recv;		/* continuous receive modeextension channel for HT40 */
    u_int32_t	imask;		/* interrupt mask copy */
};

int     tx99_attach(struct ath_softc *sc);
void    tx99_detach(struct ath_softc *sc);
int     ath_tx99_tgt_start(struct ath_softc *sc, int chtype);
void    ath_tx99_tgt_stop(struct ath_softc *sc);

u_int8_t tx99_ioctl(ath_dev_t dev, struct ath_softc *sc, int cmd, void *addr);
int		tx99_rate_setup(struct ath_softc *sc, u_int32_t *pRateCode, u_int32_t rateKBPS);
int		tx99_get_rateKBPS(struct ath_softc *sc, u_int32_t RateCode, u_int32_t *rateKBPS);

#define TX99_TESTMODE_TX_PN     0
#define TX99_TESTMODE_TX_ZEROS  1
#define TX99_TESTMODE_TX_ONES   2
#define TX99_TESTMODE_RX_ONLY   3
#define TX99_TESTMODE_SINGLE_CARRIER    4
//#define TX99_TESTMODE_CONT_FRAME_DATA 5

#endif /* _ATH_TX99_H */
