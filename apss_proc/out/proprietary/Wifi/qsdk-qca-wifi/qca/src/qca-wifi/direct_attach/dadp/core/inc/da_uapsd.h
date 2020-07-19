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

#ifndef _ATH_UAPSD_H_
#define _ATH_UAPSD_H_

#ifdef ATH_SUPPORT_UAPSD
/*
 * External Definitions
 */
wbuf_t ath_net80211_uapsd_allocqosnullframe(struct wlan_objmgr_pdev *pdev);
wbuf_t ath_net80211_uapsd_getqosnullframe(struct wlan_objmgr_peer *peer,
					  wbuf_t wbuf, int ac);
void ath_net80211_uapsd_retqosnullframe(struct wlan_objmgr_peer *peer,
					wbuf_t wbuf);
void ath_net80211_uapsd_eospindicate(struct wlan_objmgr_peer *peer, wbuf_t wbuf,
				     int txok, int force_eosp);
bool ath_net80211_check_uapsdtrigger(struct wlan_objmgr_pdev *pdev,
				     struct ieee80211_qosframe *qwh,
				     u_int16_t keyix, bool isr_context);
void ath_net80211_uapsd_deliverdata(ieee80211_handle_t ieee,
				    struct ieee80211_qosframe *qwh,
				    u_int16_t keyix, u_int8_t is_trig,
				    bool isr_context);

static INLINE void
ath_uapsd_txq_update(struct ath_softc_net80211 *scn, HAL_TXQ_INFO * qi, int ac)
{
	/*
	 * set VO parameters in UAPSD queue
	 */
	if (ac == WME_AC_VO)
		scn->sc_ops->txq_update(scn->sc_dev, scn->sc_uapsd_qnum, qi);
}

static INLINE void
ath_uapsd_txctl_update(struct ath_softc_net80211 *scn, wbuf_t wbuf,
		       ieee80211_tx_control_t * txctl)
{
	txctl->isuapsd = wbuf_is_uapsd(wbuf);
	/*
	 * UAPSD frames go to a dedicated hardware queue.
	 */
	if (txctl->isuapsd) {
		txctl->qnum = scn->sc_uapsd_qnum;
	}
}

static INLINE void ath_uapsd_attach(struct ath_softc_net80211 *scn)
{
	struct ieee80211com *ic = &scn->sc_ic;
	scn->sc_uapsd_qnum =
	    scn->sc_ops->tx_get_qnum(scn->sc_dev, HAL_TX_QUEUE_UAPSD, 0);
	/*
	 * UAPSD capable
	 */
	if (scn->sc_ops->have_capability(scn->sc_dev, ATH_CAP_UAPSD)) {
		ieee80211com_set_cap(ic, IEEE80211_C_UAPSD);
		IEEE80211_UAPSD_ENABLE(ic);
	}
}

static INLINE void
ath_uapsd_pwrsave_check(wbuf_t wbuf, struct ieee80211_node *ni)
{
	wlan_if_t vap = ni->ni_vap;
	if (WME_UAPSD_AC_ISDELIVERYENABLED(wbuf_get_priority(wbuf), ni)) {
		/* U-APSD power save queue for delivery enabled AC */
		wbuf_set_uapsd(wbuf);
		wbuf_set_moredata(wbuf);
		/* DA change required as part of stats convergence */
		/* IEEE80211_NODE_STAT(ni, tx_uapsd); */

		if ((vap->iv_set_tim != NULL)
		    && IEEE80211_NODE_UAPSD_USETIM(ni)) {
			vap->iv_set_tim(ni, 1, false);
		}
	}
}

void
ath_net80211_uapsd_pause_control(struct wlan_objmgr_peer *peer, bool pause);

void ath_net80211_uapsd_process_uapsd_trigger(ieee80211_handle_t ieee,
					      struct ieee80211_node *ni,
					      bool enforce_max_sp,
					      bool * sent_eosp);

#else

#define ath_uapsd_txq_update(_scn, _qi, _ac)
#define ath_uapsd_txctl_update(_scn, _wbuf, _txctl)
#define ath_uapsd_attach(_scn)
#define ath_uapsd_pwrsave_check(_wbuf, _ni)
#define ath_net80211_uapsd_pause_control(ni,pause)
#define ath_net80211_uapsd_process_uapsd_trigger(ieee, ni, enforce_max_sp, sent_eosp)

#endif /* ATH_SUPPORT_UAPSD */

#endif /* _ATH_UAPSD_H_ */
