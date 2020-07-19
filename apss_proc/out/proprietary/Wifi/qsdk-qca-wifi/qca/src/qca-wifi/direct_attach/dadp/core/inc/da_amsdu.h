/*
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

#ifndef _ATH_AMSDU_H_
#define _ATH_AMSDU_H_
/*
 * AMSDU Frame
 */
#define	ATH_AMSDU_TXQ_LOCK_INIT(_dp_pdev)    spin_lock_init(&(_dp_pdev)->amsdu_txq_lock)
#define	ATH_AMSDU_TXQ_LOCK_DESTROY(_dp_pdev)
#define	ATH_AMSDU_TXQ_LOCK(_dp_pdev)     spin_lock(&(_dp_pdev)->amsdu_txq_lock)
#define	ATH_AMSDU_TXQ_UNLOCK(_dp_pdev)   spin_unlock(&(_dp_pdev)->amsdu_txq_lock)

/*
 * External Definitions
 */
struct ath_amsdu_tx {
	wbuf_t amsdu_tx_buf;	/* amsdu staging area */
	 TAILQ_ENTRY(ath_amsdu_tx) amsdu_qelem;	/* round-robin amsdu tx entry */
	u_int sched;
};

struct ath_amsdu {
	struct ath_amsdu_tx amsdutx[WME_NUM_TID];
};

wbuf_t ath_amsdu_send(wbuf_t wbuf);
void ath_amsdu_complete_wbuf(wbuf_t wbuf);

void wlan_complete_wbuf(wbuf_t wbuf, struct ieee80211_tx_status *ts);
#endif /* _ATH_AMSDU_H_ */
