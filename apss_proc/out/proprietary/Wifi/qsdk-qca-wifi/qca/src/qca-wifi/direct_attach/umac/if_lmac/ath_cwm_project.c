/*
 *
 * Copyright (c) 2011-2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */
 
/*
 * CWM (Channel Width Management) 
 *
 */

#include "ath_cwm.h"
#include "ath_cwm_project.h"
#include "ath_cwm_cb.h"
#include <wlan_utility.h>
#if UNIFIED_SMARTANTENNA
#include <wlan_sa_api_utils_api.h>
#include <target_if_sa_api.h>
#endif


/*
 *----------------------------------------------------------------------
 * local function declarations
 *----------------------------------------------------------------------
 */

static void ath_cwm_scan(void *arg, struct ieee80211vap *vap);
static void ath_vap_iter_rate_newstate(void *arg, wlan_if_t vap);
static void ath_vap_iter_send_cwm_action(void *arg, wlan_if_t vap);
static enum cwm_opmode ieee_opmode_to_cwm_opmode(enum ieee80211_opmode iv_opmode);

   
int  ath_cwm_ioctl(struct ath_softc_net80211 *scn, int cmd, caddr_t data)
{
    /* XXX: TODO */
    return 0;
}

int
ath_cwm_get_wlan_scan_in_progress(struct ath_softc_net80211 *scn)
{
  return wlan_pdev_scan_in_progress(scn->sc_pdev);
}

u_int32_t ath_cwm_getextbusy(struct ath_softc_net80211 *scn)
{
    return cwm_getextbusy(scn->sc_cwm);
}


HAL_HT_MACMODE ath_cwm_macmode(ieee80211_handle_t ieee)
{
    struct ieee80211com         *ic  = NET80211_HANDLE(ieee);
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
 
    enum cwm_ht_macmode cw_macmode = cwm_macmode(scn->sc_cwm);

    if(cw_macmode == CWM_HT_MACMODE_20) {
        return HAL_HT_MACMODE_20;
    }
    else {
        return HAL_HT_MACMODE_2040;
    }
}


static enum cwm_opmode ieee_opmode_to_cwm_opmode(enum ieee80211_opmode ieee_opmode)
{
    enum cwm_opmode cw_opmode = CWM_M_OTHER;

    if(ieee_opmode == IEEE80211_M_HOSTAP) {
        cw_opmode = CWM_M_HOSTAP;
    }
    else if(ieee_opmode == IEEE80211_M_STA) {
        cw_opmode = CWM_M_STA;
    }

    return cw_opmode;
}

/* Search for ieee80211_vap_start */
void ath_cwm_up(void *arg, struct ieee80211vap *vap)
{
    cwm_up(vap->iv_ic, ieee_opmode_to_cwm_opmode(vap->iv_opmode));
}


/* Search for ieee80211_vap_down  */
void ath_cwm_down(struct ieee80211vap *vap)
{
    cwm_down(vap->iv_ic, ieee_opmode_to_cwm_opmode(vap->iv_opmode));
}


void ath_cwm_chwidth_change(struct ieee80211_node *ni)
{
	struct ieee80211com         *ic     = ni->ni_ic;
	struct ath_softc_net80211   *scn    = ATH_SOFTC_NET80211(ic);
	
    enum cwm_width cwm_chwidth = (ni->ni_chwidth == IEEE80211_CWM_WIDTH40) ?  
                                                CWM_WIDTH40 : CWM_WIDTH20;
    cwm_chwidth_change(scn->sc_cwm, ni, cwm_chwidth,
            ieee_opmode_to_cwm_opmode(ni->ni_vap->iv_opmode));

}

/*
 * Scan start notification
 *
 * - leaving home chanel
 * - called before channel change 
 */
void
ath_cwm_scan_start(struct ieee80211com *ic)
{
	DPRINTF(ATH_SOFTC_NET80211(ic), ATH_DEBUG_CWM, "%s\n", __func__);

    /* XXX: Why iterate through vap list? */
    wlan_iterate_vap_list(ic,ath_cwm_scan,NULL);
}

static void ath_cwm_scan(void *arg, struct ieee80211vap *vap)
{
    cwm_scan(vap->iv_ic, ieee_opmode_to_cwm_opmode(vap->iv_opmode));
}

/*
 * Scan end notification
 *
 * - returning to home chanel
 * - called before channel change 
 */
void
ath_cwm_scan_end(struct ieee80211com *ic)
{
	DPRINTF(ATH_SOFTC_NET80211(ic), ATH_DEBUG_CWM, "%s\n", __func__);

    /* XXX: Why iterate through vap list? */
    wlan_iterate_vap_list(ic,ath_cwm_join,NULL);
}

/* XXX: VAP join calls cwm join. When does VAP join happen? Why does it not
 * call cwm_start that starts ext ch sensing? */
void ath_cwm_join(void *arg, struct ieee80211vap *vap)
{
    cwm_join(vap->iv_ic);
}

/*
 * Radio enable notification
 *
 */
void
ath_cwm_radio_enable(struct ath_softc_net80211 *scn)
{
	struct ieee80211com *ic = &scn->sc_ic;

	DPRINTF(scn, ATH_DEBUG_CWM, "%s\n", __func__);

	/* set initial state */
    wlan_iterate_vap_list(ic,ath_cwm_join,NULL);

	/* restart CWM state machine and timer (if needed) */
    wlan_iterate_vap_list(ic,ath_cwm_up,NULL);
}

void ath_cwm_radio_disable(struct ath_softc_net80211 *scn)
{
    cwm_radio_disable(&scn->sc_ic);
}


void ath_cwm_switch_mode_static20(struct ieee80211com *ic)
{
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
    
    /* Cache channel flags so that we can revert to them if needed */
    scn->cached_ic_flags = 0;

    if (ic->ic_curchan->ic_flags & IEEE80211_CHAN_HT40PLUS) {
        scn->cached_ic_flags |= IEEE80211_CHAN_HT40PLUS;
    }

    if (ic->ic_curchan->ic_flags & IEEE80211_CHAN_HT40MINUS) {
        scn->cached_ic_flags |= IEEE80211_CHAN_HT40MINUS;
    }

    cwm_switch_mode_static20(scn->sc_cwm);
}

void ath_cwm_switch_mode_dynamic2040(struct ieee80211com *ic) 
{
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
 
    cwm_switch_mode_dynamic2040(scn->sc_cwm);
}

/*
 *----------------------------------------------------------------------
 * Callback functions
 *----------------------------------------------------------------------
 */
static void ath_vap_iter_send_cwm_action(void *arg, wlan_if_t vap) 
{
    struct ieee80211_node   *ni = NULL;
    struct ieee80211_action_mgt_args actionargs;
    bool   is_aponly = (NULL == arg) ? false : *(bool*)arg;

    if (ieee80211vap_get_opmode(vap) == IEEE80211_M_HOSTAP) {
        /* create temporary node for broadcast */
        if (wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
            ni = ieee80211_tmp_node(vap, IEEE80211_GET_BCAST_ADDR(vap->iv_ic));
        }
    } else if (false == is_aponly) {
        ni = vap->iv_bss;
    }

    /* send channel width action frame */
    if (ni != NULL) {
        actionargs.category	= IEEE80211_ACTION_CAT_HT;
        actionargs.action	= IEEE80211_ACTION_HT_TXCHWIDTH;
        actionargs.arg1		= 0;
        actionargs.arg2		= 0;
        actionargs.arg3		= 0;
        ieee80211_send_action(ni, &actionargs, NULL);

        if (ieee80211vap_get_opmode(vap) == IEEE80211_M_HOSTAP) {
            /* temporary node - decrement reference count so that the node will be 
             * automatically freed upon completion */
            wlan_objmgr_delete_node(ni);
        }
    }
}

/*
 * Send ch width action management frame 
 */
void
ath_cwm_sendactionmgmt(struct ath_softc_net80211 *scn)
{
	struct ieee80211com 	*ic 	= &scn->sc_ic;
	bool   is_aponly = false;

    /* all virtual APs - send ch width action management frame */
    wlan_iterate_vap_list(ic,ath_vap_iter_send_cwm_action,&is_aponly);
}

/*
 * Send ch width action management frame 
 */
void
ath_cwm_sendactionmgmt_aponly(struct ath_softc_net80211 *scn)
{
    struct ieee80211com  *ic = &scn->sc_ic;
    bool   is_aponly = true;
    /* all virtual APs - send ch width action management frame for AP Vaps only */
    wlan_iterate_vap_list(ic,ath_vap_iter_send_cwm_action,&is_aponly);
}


/*
 * Update node's rate table 
 */
void
ath_cwm_rate_updatenode(void *arg) 
{
    struct ieee80211_node *ni = arg;
    struct ieee80211com         *ic = ni->ni_ic;
    struct ieee80211vap         *vap = ni->ni_vap;
    struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    struct ieee80211_key        *k = NULL;
#endif
    u_int32_t                   capflag = 0;
    enum ieee80211_cwm_width ic_cw_width = cwm_get_width(ic);

    if ((ni->ni_chwidth == IEEE80211_CWM_WIDTH40) && 
            (ic_cw_width == IEEE80211_CWM_WIDTH40))
    {
        capflag |=  ATH_RC_CW40_FLAG;
    }

    if (((ni->ni_chwidth == IEEE80211_CWM_WIDTH20) && (ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI20)) ||
            ((ni->ni_chwidth == IEEE80211_CWM_WIDTH40) && (ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI20) && (ni->ni_htcap & IEEE80211_HTCAP_C_SHORTGI40))) {
        capflag |= ATH_RC_SGI_FLAG;
    }
    if (ni->ni_flags & IEEE80211_NODE_HT) {
        capflag |= ATH_RC_HT_FLAG;
        capflag |= ath_set_ratecap(scn, ni, vap);
    }
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
    if (ni->ni_ucastkey.wk_cipher != &ieee80211_cipher_none) {
        k = &ni->ni_ucastkey;
    } else if (vap->iv_def_txkey != IEEE80211_KEYIX_NONE) {
        k = &vap->iv_nw_keys[vap->iv_def_txkey];
    }
    if (k && (k->wk_cipher->ic_cipher == IEEE80211_CIPHER_WEP || 
                k->wk_cipher->ic_cipher == IEEE80211_CIPHER_TKIP)) {
#else
    if (wlan_crypto_vdev_has_ucastcipher(vap->vdev_obj, ((1 << WLAN_CRYPTO_CIPHER_WEP) | (1 << WLAN_CRYPTO_CIPHER_TKIP)))) {
#endif
        capflag |= ATH_RC_WEP_TKIP_FLAG;
        if (!ieee80211_has_weptkipaggr(ni)) {
            /* Atheros proprietary wep/tkip aggregation mode is not supported */
            ieee80211node_set_flag(ni, IEEE80211_NODE_NOAMPDU);
        }
    }

    /* Rx STBC is a 2-bit mask. Needs to convert from ieee definition to ath definition. */ 
    capflag |= (((ni->ni_htcap & IEEE80211_HTCAP_C_RXSTBC) >> IEEE80211_HTCAP_C_RXSTBC_S)
            << ATH_RC_RX_STBC_FLAG_S);

    if (ni->ni_flags & IEEE80211_NODE_UAPSD) {
        capflag |= ATH_RC_UAPSD_FLAG;
    }

#ifdef  ATH_SUPPORT_TxBF
    capflag |= (((ni->ni_txbf.channel_estimation_cap) << ATH_RC_CEC_FLAG_S) & ATH_RC_CEC_FLAG);
#endif
    scn->sc_ops->ath_rate_newassoc(scn->sc_dev, ATH_NODE_NET80211(ni)->an_sta, 1, capflag,
            &ni->ni_rates, &ni->ni_htrates);
}

/*
 * Update all associated nodes and VAPs
 *
 * Called when local channel width changed.  e.g. if AP mode,
 * update all associated STAs when the AP's channel width changes.
 */
void
ath_cwm_rate_updateallnodes(struct ath_softc_net80211 *scn)
{
    struct ieee80211com 	*ic 	= &scn->sc_ic;
    /* all virtual APs - update destination nodes rate */
    wlan_iterate_vap_list(ic,ath_vap_iter_rate_newstate,(void *)scn);
}

static void ath_vap_iter_rate_newstate(void *arg, wlan_if_t vap)
{
    struct ath_softc_net80211 *scn = (struct ath_softc_net80211 *)arg;
    struct ath_vap_net80211 *avn = ATH_VAP_NET80211(vap);

    if (wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
        scn->sc_ops->ath_rate_newstate(scn->sc_dev,avn->av_if_id, 1);
    }
    else {
        scn->sc_ops->ath_rate_newstate(scn->sc_dev,avn->av_if_id, 0);
    }
}

/*
 * HT40 allowed?
 * - check device
 * - check regulatory (channel flags)
 */
int
ath_cwm_ht40allowed(struct ath_softc_net80211 *scn) 
{
	struct ieee80211com 	*ic 	= &scn->sc_ic;
        struct ieee80211_ath_channel *chan  = ic->ic_curchan;

	DPRINTF(scn, ATH_DEBUG_CWM, "%s: IC channel: chfreq %d, chflags 0x%llX\n",
        __func__, chan->ic_freq, chan->ic_flags);

	if ((ic->ic_htcap & IEEE80211_HTCAP_C_CHWIDTH40) &&
	    (IEEE80211_IS_CHAN_11N_HT40(chan)) && (scn->sc_cwm->cw_ht40allowed)) {
		return 1;
	} else {
		return 0;
	}   
}

void ath_cwm_change_channel(struct ath_softc_net80211 *scn)
{
	struct ieee80211com 	*ic 	= &scn->sc_ic;
 
    ath_net80211_change_channel(ic, ic->ic_curchan);
}

void ath_cwm_set_macmode(struct ath_softc_net80211 *scn, enum cwm_ht_macmode cw_macmode)
{
    HAL_HT_MACMODE hal_macmode = (cw_macmode == CWM_HT_MACMODE_20) ? HAL_HT_MACMODE_20 : HAL_HT_MACMODE_2040;
	scn->sc_ops->set_macmode(scn->sc_dev, hal_macmode);
}



void ath_cwm_set_curchannelflags_ht20(struct ath_softc_net80211 *scn)
{
    struct ieee80211com 	*ic 	= &scn->sc_ic;

    if (ic->ic_curchan->ic_flags & IEEE80211_CHAN_HT40PLUS) {
        ic->ic_curchan->ic_flags &= ~IEEE80211_CHAN_HT40PLUS;
    }

    if (ic->ic_curchan->ic_flags & IEEE80211_CHAN_HT40MINUS) {
        ic->ic_curchan->ic_flags &= ~IEEE80211_CHAN_HT40MINUS;
    }

    ic->ic_curchan->ic_flags |= IEEE80211_CHAN_HT20;
}

void ath_cwm_set_curchannelflags_ht40(struct ath_softc_net80211 *scn)
{ 
    struct ieee80211com 	*ic 	= &scn->sc_ic;

    if (ic->ic_curchan->ic_flags & IEEE80211_CHAN_HT20) {
        ic->ic_curchan->ic_flags &= ~IEEE80211_CHAN_HT20;
    }

    if (scn->cached_ic_flags & IEEE80211_CHAN_HT40PLUS) {
        ic->ic_curchan->ic_flags |= IEEE80211_CHAN_HT40PLUS;
    }

    if (scn->cached_ic_flags & IEEE80211_CHAN_HT40MINUS) {
        ic->ic_curchan->ic_flags |= IEEE80211_CHAN_HT40MINUS;
    }
}

enum cwm_opmode ath_cwm_get_icopmode(struct ath_softc_net80211 *scn)
{ 
    struct ieee80211com 	*ic 	= &scn->sc_ic;

    return ieee_opmode_to_cwm_opmode(ic->ic_opmode);
 
}

int ath_cwm_get_extchbusyper(struct ath_softc_net80211 *scn)
{
    return scn->sc_ops->get_extbusyper(scn->sc_dev);
}

enum cwm_phymode ath_cwm_get_curchannelmode(struct ath_softc_net80211 *scn)
{ 
    struct ieee80211com 	*ic 	= &scn->sc_ic;
    enum ieee80211_phymode ieee_phymode = ieee80211_chan2mode(ic->ic_curchan);

    /* 11AX TODO - Add 11ax processing if required in the future. Currently, 11ac
     * too is not processed.
     */

    switch(ieee_phymode){
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NG_HT40PLUS:
            return CWM_PHYMODE_HT40PLUS;
        case IEEE80211_MODE_11NA_HT40MINUS:
        case IEEE80211_MODE_11NG_HT40MINUS:
            return CWM_PHYMODE_HT40MINUS;
        default:
            return CWM_PHYMODE_OTHER;
    }
}


#ifndef ATH_CWM_MAC_DISABLE_REQUEUE
/*
 * Fast Channel Change Allowed?
 */
static int cwm_queryfcc(struct ath_softc_net80211 *scn)
{
    struct ieee80211com 	*ic 	= &scn->sc_ic;

    if (ic->ic_flags_ext & IEEE80211_FAST_CC) {
        return 1;
    } else {
        return 0;
    }
}

/*
 * Stop tx DMA (fast).  Does not reset or channel change
 */
void
ath_cwm_stoptxdma(struct ath_softc_net80211 *scn)
{
    struct ath_hal 		*ah 	= scn->sc_ah;

    /* disable interrupts */
    ath_hal_intrset(ah, 0);     	

    if (cwm_queryfcc(scn)) {
        /* fast tx abort */
        if (!ath_hal_aborttxdma(ah)) {
            printk("%s: unable to abort tx dma\n", __func__);
        }
    } else {
        int i;
        for (i = 0; i < HAL_NUM_TX_QUEUES; i++)
            if (ATH_TXQ_SETUP(sc, i)) {
                ath_hal_stoptxdma(ah, sc->sc_txq[i].axq_qnum);
            }
    }
}

/*
 * Resume tx DMA 
 */
void
ath_cwm_resumetxdma(struct ath_softc_net80211 *scn)
{
    struct ath_hal 		*ah 	= sc->sc_ah;

    /* Re-enable interrupts */
    ath_hal_intrset(ah, sc->sc_imask);
}


void ath_cwm_requeue(void *arg)
{
    struct ieee80211_node *ni = arg;
	struct ieee80211com         *ic = ni->ni_ic;
	struct ath_softc_net80211   *scn = ATH_SOFTC_NET80211(ic);
 	int 		      	ac;
	struct ath_txq      	*txq;
	int			requeue[WME_AC_VO + 1];


	KASSERT(ni != NULL, ("%s: error node is null\n", __func__));

	/* requeueing only needed if destination node has frames
	 * on _any_ hardware queue 
	 */
	if (ATH_NODE_HWQ_ACTIVE(ni)) {
		/* stop hw tx */
        cwm_stoptxdma((void *)scn);

		/*
		 * first complete all packets in h/w queue
		 * mark incomplete packets as sw filtered
		 */
		for (ac = WME_AC_BE; ac <= WME_AC_VO; ac++) {
			/* check if destination node has frames on _this_ hardware queue */
			if (ATH_NODE_AC_ACTIVE(ni, ac)) {
				txq = scn->sc_ac2q[ac];
				owl_tx_processq(scn, txq, OWL_TXQ_FILTERED);
				requeue[ac] = 1;
			} else {
				requeue[ac] = 0;
			}
		}

		/* update node's rate table */
        cwm_rate_updatenode((void *)ni);

		/* requeue frames */
		for (ac = WME_AC_BE; ac <= WME_AC_VO; ac++) {
			/* check if destination node has frames on _this_ hardware queue */
			if (requeue[ac]) {
				txq = scn->sc_ac2q[ac];
				owl_tx_requeue(scn, txq);
			}
		}
		/* restart hw tx */
        cwm_resumetxdma((void *)scn);
	} else {
		/* update node's rate table */
        cwm_rate_updatenode((void *)ni);
	}

}


void ath_cwm_process_tx_pkts(struct ath_softc_net80211 *scn)
{
	int			ac;
	struct ath_txq      	*txq;
 
	/*
	 * first complete all packets in h/w queue
	 * mark incomplete packets as sw filtered
	 */
	for (ac = WME_AC_BE; ac <= WME_AC_VO; ac++) {
		txq = scn->sc_ac2q[ac];
		owl_tx_processq(scn, txq, OWL_TXQ_FILTERED);
	}
}


void ath_cwm_requeue_tx_pkts(struct ath_softc_net80211 *scn)
{ 
	int			ac;
	struct ath_txq      	*txq;
 
    for (ac = WME_AC_BE; ac <= WME_AC_VO; ac++) {
		txq = snc->sc_ac2q[ac];
		owl_tx_requeue(scn, txq);
	}

}

#endif /* #ifndef ATH_CWM_MAC_DISABLE_REQUEUE */


void ath_cwm_switch_to40(struct ieee80211com *ic)
{
    if(ic->ic_curchan->ic_flags & IEEE80211_CHAN_HT20) {
        return;
    }

    cwm_switch_to40(ic);

}


#if UNIFIED_SMARTANTENNA
/* Smart Antenna handling for CWM action */
int ath_smart_ant_cwm_action(struct ath_softc_net80211 *scn)
{
    struct wlan_objmgr_psoc *psoc = NULL;
    struct wlan_objmgr_pdev *pdev = NULL;
    int ret = -1;
    QDF_STATUS status;

    pdev = scn->sc_pdev;

    status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_SA_API_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_print("%s, %d unable to get reference", __func__, __LINE__);
        return ret;
    }

    psoc = wlan_pdev_get_psoc(pdev);

    ret = target_if_sa_api_cwm_action(psoc, pdev);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
    return ret;
}
#else
int ath_smart_ant_cwm_action(struct ath_softc_net80211 *scn)
{
    return 0;
}
#endif

int ath_cwm_attach(struct ath_softc_net80211 *scn, struct ath_reg_parm *ath_conf_parm)
{
	struct ieee80211com 	*ic 	= &scn->sc_ic;
    struct ath_cwm_conf cwm_conf_parm;

    /* Install CWM APIs */
    ic->ic_cwm_get_extprotmode = cwm_get_extprotmode;
    ic->ic_cwm_get_extoffset = cwm_get_extoffset;
    ic->ic_cwm_get_extprotspacing = cwm_get_extprotspacing;
    ic->ic_cwm_get_enable = cwm_get_enable;
    ic->ic_cwm_get_extbusythreshold = cwm_get_extbusythreshold;
    ic->ic_cwm_get_mode = cwm_get_mode;
    ic->ic_cwm_get_width = cwm_get_width;
    ic->ic_cwm_set_extprotmode = cwm_set_extprotmode;
    ic->ic_cwm_set_extprotspacing = cwm_set_extprotspacing;
    ic->ic_cwm_set_enable = cwm_set_enable;
    ic->ic_cwm_set_extbusythreshold = cwm_set_extbusythreshold;
    ic->ic_cwm_set_mode = cwm_set_mode;
    ic->ic_cwm_set_width = cwm_set_width;
    ic->ic_cwm_set_extoffset = cwm_set_extoffset;
    ic->ic_bss_to40 = ath_cwm_switch_to40;
    ic->ic_bss_to20 = cwm_switch_to20;

    cwm_conf_parm.cw_enable = ath_conf_parm->cwmEnable; 

    scn->sc_cwm = cwm_attach(scn, scn->sc_osdev, &cwm_conf_parm);

    if(scn->sc_cwm == NULL) {
        return ENOMEM;
    }

    return 0;
}

void ath_cwm_detach(struct ath_softc_net80211 *scn)
{
    if (scn) {
        struct ieee80211com *ic = &scn->sc_ic;
        cwm_detach(ic);
        scn->sc_cwm = NULL;
    }
}
