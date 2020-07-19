/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 *  Copyright (c) 2008 Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */
#ifndef _ATH_HTC_H_
#define _ATH_HTC_H_

/*
 * External Definitions
 */

u_int8_t ath_htc_find_tgt_node_index(wbuf_t);
u_int8_t ath_net80211_find_tgt_node_index(ieee80211_handle_t ieee, ieee80211_node_t node);
u_int8_t ath_net80211_find_tgt_vap_index(ieee80211_handle_t ieee, ieee80211_node_t node);

void ath_add_vap_target(struct ieee80211com *ic, void *vtar, int size);
void ath_add_node_target(struct ieee80211com *ic, void *ntar, int size);
void ath_delete_vap_target(struct ieee80211com *ic, void *vtar, int size);
void ath_delete_node_target(struct ieee80211com *ic, void *ntar, int size);
void ath_update_node_target(struct ieee80211com *ic, void *ntar, int size);
#if ENCAP_OFFLOAD
void ath_update_vap_target(struct ieee80211com *ic, void *vtgt, int size);
#else
#define ath_update_vap_target(ic, vtgt, size)   (0)
#endif
void ath_htc_ic_update_target(struct ieee80211com *ic, void *target, int size);
void ath_get_config_chainmask(struct ieee80211com *ic, void *vtar);

void ath_htc_wmm_update_params(struct ieee80211com *ic, struct ieee80211vap *vap);
int ath_htc_wmm_update_enable(struct ieee80211com *ic);
void ath_htc_wmm_update(ieee80211_handle_t ieee);
void ath_net80211_uapsd_creditupdate(ieee80211_handle_t ieee);
void athnet80211_rxcleanup(ieee80211_handle_t ieee);

#define ath_node_kick_event(scn, nodeindex)
#define ath_nodekick_event_defer(data)
#define ATH_HTC_RXPAUSE(_ic)  
#define ATH_HTC_RXUNPAUSE(_ic) 



#define IEEE80211_HTC_SET_NODE_INDEX(txctl, wbuf)
#define HTC_WBUF_TX_DELCARE
#define HTC_WBUF_TX_MGT_PREPARE(ic, scn, txctl)
#define HTC_WBUF_TX_MGT_COMPLETE_STATUS(ic)
#define HTC_WBUF_TX_MGT_ERROR_STATUS(ic)
#define HTC_WBUF_TX_DATA_PREPARE(ic, scn)
#define HTC_WBUF_TX_DATA_COMPLETE_STATUS(ic)
#define HTC_WBUF_TX_MGT_ACTION_FRAME_NODE_FREE(ni)
#define IEEE80211_HTC_SET_IC_CALLBACK(ic)
#define HTC_SET_NET80211_OPS_FUNC(net80211_ops, find_tgt_idx_func, usb_wmm_update_func, find_tgt_vapidx_func, uapsd_creditupdate_func, _athnet80211_rxcleanup)
#define ATH_CLEAR_KEYCACHE(_scn)    ath_clear_keycache(_scn)
#define ATH_RESTORE_KEYCACHE(_scn)  ath_restore_keycache(_scn)

#ifdef ATH_USB
#if 0
#define ATH_USB_UPDATE_CWIN_FOR_BE(qi)                                          \
    if ((IEEE80211_IS_CHAN_11N(ic->ic_bsschan)) && (ac == WME_AC_BE)) {         \
        if (OS_CHIP_IS_MAGPIE(ic) == FALSE) {                                   \
            qi.tqi_cwmin = qi.tqi_cwmin/2;                                      \
            printk("%s: reduce cwmin to half in BE queue, queueNum = %d, cwmin = %d\n", __FUNCTION__, scn->sc_ac2q[ac], qi.tqi_cwmin); \
        }                                                                                     \
    }

#define ATH_USB_RESTORE_CWIN_FOR_BE(qi)                                         \
    if ((IEEE80211_IS_CHAN_11N(ic->ic_bsschan)) && (ac == WME_AC_BE)) {         \
        if (OS_CHIP_IS_MAGPIE(ic) == FALSE) {                                   \
            qi.tqi_cwmin = qi.tqi_cwmin*2;                                      \
            printk("%s: restore cwmin to half in BE queue, queueNum = %d, cwmin = %d\n", __FUNCTION__, scn->sc_ac2q[ac], qi.tqi_cwmin); \
        }                                                                                     \
    }
#else
#define ATH_USB_UPDATE_CWIN_FOR_BE(qi)
#define ATH_USB_RESTORE_CWIN_FOR_BE(qi)
#endif
#else
#define ATH_USB_UPDATE_CWIN_FOR_BE(qi)
#define ATH_USB_RESTORE_CWIN_FOR_BE(qi)
#endif /* #ifdef ATH_USB */

#endif /* _ATH_HTC_H_ */
