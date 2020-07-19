/*
 * Copyright (c) 2011-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <ieee80211_mlme_priv.h>
#include <ieee80211_ucfg.h>
#include <ieee80211_cfg80211.h>
#include <include/wlan_mlme_cmn.h>
#include <include/wlan_pdev_mlme.h>
#include <include/wlan_vdev_mlme.h>
#include <wlan_son_pub.h>
#include "if_athvar.h"
#include <ieee80211_mlme_dfs_dispatcher.h>
#if UNIFIED_SMARTANTENNA
#include <wlan_sa_api_utils_api.h>
#include <wlan_sa_api_utils_defs.h>
#endif
#include <wlan_mlme_dbg.h>
#include <wlan_utility.h>
#include "core/inc/vdev_mlme_sm_actions.h"
#include <wlan_osif_priv.h>
#include <wlan_mlme_if.h>
#include <wlan_mlme_vdev_mgmt_ops.h>
#include <cfg_ucfg_api.h>

QDF_STATUS mlme_register_ops(struct vdev_mlme_obj *vdev_mlme);

uint8_t wlanphymode2ieeephymode[WLAN_PHYMODE_11AXA_HE80_80 + 1] = {
	IEEE80211_MODE_AUTO,                /* WLAN_PHYMODE_AUTO,          */
	IEEE80211_MODE_11A,                 /* WLAN_PHYMODE_11A,           */
	IEEE80211_MODE_11B,                 /* WLAN_PHYMODE_11B,           */
	IEEE80211_MODE_11G,                 /* WLAN_PHYMODE_11G,           */
	IEEE80211_MODE_11NA_HT20,           /* WLAN_PHYMODE_11NA_HT20,     */
	IEEE80211_MODE_11NG_HT20,           /* WLAN_PHYMODE_11NG_HT20,     */
	IEEE80211_MODE_11NA_HT40PLUS,       /* WLAN_PHYMODE_11NA_HT40PLUS, */
	IEEE80211_MODE_11NA_HT40MINUS,      /* WLAN_PHYMODE_11NA_HT40MINUS,*/
	IEEE80211_MODE_11NG_HT40PLUS,       /* WLAN_PHYMODE_11NG_HT40PLUS, */
	IEEE80211_MODE_11NG_HT40MINUS,      /* WLAN_PHYMODE_11NG_HT40MINUS,*/
	IEEE80211_MODE_11NG_HT40,           /* WLAN_PHYMODE_11NG_HT40,     */
	IEEE80211_MODE_11NA_HT40,           /* WLAN_PHYMODE_11NA_HT40,     */
	IEEE80211_MODE_11AC_VHT20,          /* WLAN_PHYMODE_11AC_VHT20,    */
	IEEE80211_MODE_11AC_VHT40PLUS,      /* WLAN_PHYMODE_11AC_VHT40PLUS,*/
	IEEE80211_MODE_11AC_VHT40MINUS,     /* WLAN_PHYMODE_11AC_VHT40MINUS*/
	IEEE80211_MODE_11AC_VHT40,          /* WLAN_PHYMODE_11AC_VHT40,    */
	IEEE80211_MODE_11AC_VHT80,          /* WLAN_PHYMODE_11AC_VHT80,    */
	IEEE80211_MODE_11AC_VHT160,         /* WLAN_PHYMODE_11AC_VHT160,   */
	IEEE80211_MODE_11AC_VHT80_80,       /* WLAN_PHYMODE_11AC_VHT80_80, */
	IEEE80211_MODE_11AXA_HE20,          /* WLAN_PHYMODE_11AXA_HE20,    */
	IEEE80211_MODE_11AXG_HE20,          /* WLAN_PHYMODE_11AXG_HE20,    */
	IEEE80211_MODE_11AXA_HE40PLUS,      /* WLAN_PHYMODE_11AXA_HE40PLUS,*/
	IEEE80211_MODE_11AXA_HE40MINUS,     /* WLAN_PHYMODE_11AXA_HE40MINUS*/
	IEEE80211_MODE_11AXG_HE40PLUS,      /* WLAN_PHYMODE_11AXG_HE40PLUS,*/
	IEEE80211_MODE_11AXG_HE40MINUS,     /* WLAN_PHYMODE_11AXG_HE40MINUS*/
	IEEE80211_MODE_11AXA_HE40,          /* WLAN_PHYMODE_11AXA_HE40,    */
	IEEE80211_MODE_11AXG_HE40,          /* WLAN_PHYMODE_11AXG_HE40,    */
	IEEE80211_MODE_11AXA_HE80,          /* WLAN_PHYMODE_11AXA_HE80,    */
	IEEE80211_MODE_11AXA_HE160,         /* WLAN_PHYMODE_11AXA_HE160,   */
	IEEE80211_MODE_11AXA_HE80_80,       /* WLAN_PHYMODE_11AXA_HE80_80, */
};
EXPORT_SYMBOL(wlanphymode2ieeephymode);

enum mlme_sm_cmd_type {
    MLME_SM_CMD_PDEV_RADAR_DETECT,
};

struct mlme_sm_cmd_sched_data {
    struct wlan_objmgr_pdev *pdev;
    enum mlme_sm_cmd_type cmd_type;
    struct ieee80211_ath_channel *new_chan;
};

/*
 * mlme_sm_cmd_sched_cb(): cb function from scheduler context
 * @msg: scheduler msg data
 *
 * This cb will be called when a msg posted to the scheduler queue
 * has to be flushed
 *
 * Return: Success
 */
QDF_STATUS mlme_sm_cmd_flush_cb(struct scheduler_msg *msg)
{
	struct mlme_sm_cmd_sched_data *req = msg->bodyptr;

	switch (req->cmd_type) {
	case MLME_SM_CMD_PDEV_RADAR_DETECT:
		wlan_pdev_mlme_op_clear(req->pdev,
					WLAN_PDEV_OP_RADAR_DETECT_DEFER);
	break;
	}

	wlan_objmgr_pdev_release_ref(req->pdev, WLAN_SCHEDULER_ID);
	qdf_mem_free(req);
	return QDF_STATUS_SUCCESS;
}

/*
 * mlme_sm_cmd_sched_cb(): cb function from scheduler context
 * @msg: scheduler msg data
 *
 * This is the cb from the scheduler context where MBSSID scenario
 * related vdev cmds are processed
 *
 * Return: Success if vdev cmd is processed else error.
 */
QDF_STATUS mlme_sm_cmd_sched_cb(struct scheduler_msg *msg)
{
	struct mlme_sm_cmd_sched_data *req = msg->bodyptr;
	wlan_dev_t ic = wlan_pdev_get_mlme_ext_obj(req->pdev);

	if (!ic) {
		qdf_err("NULL ic");
		wlan_objmgr_pdev_release_ref(req->pdev, WLAN_SCHEDULER_ID);
		qdf_mem_free(req);
		return QDF_STATUS_SUCCESS;
	}

	switch (req->cmd_type) {
	case MLME_SM_CMD_PDEV_RADAR_DETECT:
		wlan_pdev_mlme_vdev_sm_notify_radar_ind(req->pdev,
							req->new_chan);
	break;
	default:
		qdf_err("Unknown cmd type");
	}

	wlan_objmgr_pdev_release_ref(req->pdev, WLAN_SCHEDULER_ID);
	qdf_mem_free(req);

	return QDF_STATUS_SUCCESS;
}

/*
 * mlme_sm_cmd_schedule_req(): Form a scheduler msg to be posted for
 * vdev operations in MBSSID scenario
 * @vdev: Object manager vdev
 * @cmd_type: Serialization command type
 *
 * Return: Success if the request is posted to the scheduler, else error
 */
QDF_STATUS mlme_sm_cmd_schedule_req(struct wlan_objmgr_pdev *pdev,
				    enum mlme_sm_cmd_type cmd_type,
				    struct ieee80211_ath_channel *chan)
{
	struct scheduler_msg msg = {0};
	struct mlme_sm_cmd_sched_data *req;
	QDF_STATUS ret = QDF_STATUS_E_FAILURE;

	if (!pdev)
		return QDF_STATUS_E_INVAL;

	req = qdf_mem_malloc(sizeof(*req));
	if (!req) {
		qdf_err("req is NULL");
		return ret;
	}

	ret = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_SCHEDULER_ID);
	if (QDF_IS_STATUS_ERROR(ret)) {
		qdf_err("unable to get reference");
		goto sched_error;
	}

	req->pdev = pdev;
	req->cmd_type = cmd_type;
	req->new_chan = chan;

	msg.bodyptr = req;
	msg.callback = mlme_sm_cmd_sched_cb;
	msg.flush_callback = mlme_sm_cmd_flush_cb;

	ret = scheduler_post_message(QDF_MODULE_ID_OS_IF,
				QDF_MODULE_ID_OS_IF, QDF_MODULE_ID_OS_IF, &msg);
	if (QDF_IS_STATUS_ERROR(ret)) {
		wlan_objmgr_pdev_release_ref(pdev, WLAN_SCHEDULER_ID);
		qdf_err("failed to post scheduler_msg");
		goto sched_error;
	}
	return ret;

sched_error:
	qdf_mem_free(req);
	return ret;
}

enum ieee80211_phymode wlan_vdev_get_ieee_phymode(enum wlan_phymode wlan_phymode)
{
	if (wlan_phymode == 0xff)
		return IEEE80211_MODE_AUTO;

	if (wlan_phymode > WLAN_PHYMODE_11AXA_HE80_80)
		return IEEE80211_MODE_AUTO;

	return wlanphymode2ieeephymode[wlan_phymode];
}

QDF_STATUS mlme_vdev_reset_proto_params_cb(
                        struct vdev_mlme_obj *vdev_mlme,
			uint16_t event_data_len, void *event_data)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_channel *bss_chan;

	if (!vdev_mlme) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = vdev_mlme->vdev;

	if (vdev == NULL) {
		mlme_err("VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	bss_chan = wlan_vdev_mlme_get_bss_chan(vdev);
	if (bss_chan)
		ieee80211_update_vdev_chan(bss_chan, IEEE80211_CHAN_ANYC);
	else
		mlme_err("(vdev-id:%d) BSS chan is not allocated",
						    wlan_vdev_get_id(vdev));

	if (vdev_mlme->mgmt.generic.ssid_len) {
		qdf_mem_zero(vdev_mlme->mgmt.generic.ssid, WLAN_SSID_MAX_LEN+1);
		vdev_mlme->mgmt.generic.ssid_len = 0;
	}

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlme_sta_cac_start(struct ieee80211com *ic,
                                        struct ieee80211vap *vap)
{
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
	int cac_timeout;
	if (mlme_is_stacac_needed(vap)) {
		qdf_info(
			"STACAC_start chan %d timeout %d sec, curr time:%d sec",
			ic->ic_curchan->ic_freq,
			ieee80211_dfs_get_cac_timeout(ic, ic->ic_curchan),
			(qdf_system_ticks_to_msecs(qdf_system_ticks()) / 1000));
		mlme_set_stacac_running(vap,1);
		/* start CAC timer */
		ieee80211_dfs_cac_start(vap);
		mlme_reset_mlme_req(vap);
		cac_timeout = ieee80211_dfs_get_cac_timeout(ic, ic->ic_curchan);
		wlan_cfg80211_dfs_cac_start(vap, cac_timeout);
		IEEE80211_DELIVER_EVENT_CAC_STARTED(vap,
					ic->ic_curchan->ic_freq, cac_timeout);
		return QDF_STATUS_SUCCESS;
	}
#endif

	return QDF_STATUS_E_FAILURE;
}

static int
ieee80211_vap_wme_param_update(struct ieee80211com *ic, wlan_if_t vaphandle)
{
    enum ieee80211_phymode mode;
    int i;
    struct ieee80211_wme_state *wme;

    wme = &vaphandle->iv_wmestate;
    mode = ieee80211_chan2mode(vaphandle->iv_bsschan);
    for(i = 0;i< WME_NUM_AC;i++){
        ic->phyParamForAC[i][mode].logcwmin = wme->wme_wmeChanParams.cap_wmeParams[i].wmep_logcwmin;
        ic->phyParamForAC[i][mode].logcwmax = wme->wme_wmeChanParams.cap_wmeParams[i].wmep_logcwmax;
        ic->phyParamForAC[i][mode].aifsn = wme->wme_wmeChanParams.cap_wmeParams[i].wmep_aifsn;
        ic->phyParamForAC[i][mode].txopLimit = wme->wme_wmeChanParams.cap_wmeParams[i].wmep_txopLimit;
        ic->bssPhyParamForAC[i][mode].logcwmin = wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_logcwmin;
        ic->bssPhyParamForAC[i][mode].logcwmax = wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_logcwmax;
        ic->bssPhyParamForAC[i][mode].aifsn = wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_aifsn;
        ic->bssPhyParamForAC[i][mode].txopLimit = wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_txopLimit;
        ic->bssPhyParamForAC[i][mode].acm = wme->wme_wmeBssChanParams.cap_wmeParams[i].wmep_acm;
    }
    return 0;
}

static inline void ieee80211_update_chan_history(struct ieee80211com  *ic)
{
    ic->ic_chanhist[ic->ic_chanidx].chanid = (ic->ic_curchan)->ic_ieee;
    ic->ic_chanhist[ic->ic_chanidx].chanjiffies = OS_GET_TIMESTAMP();
    ic->ic_chanidx == (IEEE80211_CHAN_MAXHIST - 1) ? ic->ic_chanidx = 0 : ++(ic->ic_chanidx);
}

static inline void ieee80211_update_peer_cw(struct ieee80211com *ic,
					    struct ieee80211vap *vap)
{
	struct node_chan_width_switch_params pi = {0};

	/* Update new channel and width for associated STA's */
	if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
		pi.max_peers = wlan_vdev_get_peer_count(vap->vdev_obj);
		pi.chan_width_peer_list = qdf_mem_malloc(sizeof(struct node_chan_width_switch_info *) *
							 pi.max_peers);

		if (!pi.chan_width_peer_list) {
		qdf_err("Error in allocating peer chwidth list");

		/* Sanity check is done within ieee80211_node_update_chan_and_phymode
		 * This is to ensure, in the case of failure to update target info
		 * downgrades will still be supported (as before) */
		}

		wlan_iterate_station_list(vap, ieee80211_node_update_chan_and_phymode, &pi);

		if (pi.chan_width_peer_list) {
			/* Peer information will be sent to the target only if
			 * the necessary space was allocated */
			if (pi.num_peers > 0) {
				ic->ic_node_chan_width_switch(&pi, vap);
			}

			qdf_mem_free(pi.chan_width_peer_list);
		}
	}
}

QDF_STATUS mlme_vdev_start_continue_cb(struct vdev_mlme_obj *vdev_mlme,
			uint16_t event_data_len, void *event_data)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	struct ieee80211com *ic;
	struct ieee80211_node *ni;
	struct ieee80211vap *vap;
	struct ieee80211_ath_channel *chan;
	struct wlan_channel *des_chan;
	enum QDF_OPMODE opmode;
	uint8_t numvaps_up = 0;
	struct ieee80211_wme_state wme_zero = {0};
	uint8_t restart = 0;
	int8_t error;

	if (vdev_mlme == NULL) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = vdev_mlme->vdev;
	if (vdev == NULL) {
		mlme_err("VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev == NULL) {
		mlme_err("(vdev-id:%d) PDEV is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if (!ic) {
		mlme_err("(vdev-id:%d) ic is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	if (event_data_len == 1)
		restart = *((uint8_t *)event_data);

	des_chan = wlan_vdev_mlme_get_des_chan(vdev);
	opmode = wlan_vdev_mlme_get_opmode(vdev);

	chan =  ieee80211_find_dot11_channel(ic,des_chan->ch_ieee,
			des_chan->ch_freq_seg2,
			wlan_vdev_get_ieee_phymode(des_chan->ch_phymode));
	if (!chan) {
		mlme_err("(vdev-id:%d) chan is NULL (ieee: %d)",
			 wlan_vdev_get_id(vdev), des_chan->ch_ieee);
		return QDF_STATUS_E_FAILURE;
	}

	switch (opmode) {

	case QDF_MONITOR_MODE:
		/* Handle same as HOSTAP */
	case QDF_SAP_MODE:
		numvaps_up = ieee80211_get_num_vaps_up(ic);

		/*
		 * if there is a vap already running.
		 * ignore the desired channel and use the
		 * operating channel of the other vap.
		 */
		if (numvaps_up == 0) {
			 /* 20/40 Mhz coexistence  handler */
			ic->ic_prevchan = ic->ic_curchan;
			ic->ic_curchan = chan;
			/* update max channel power to max regpower of current channel */
			ieee80211com_set_curchanmaxpwr(ic, chan->ic_maxregpower);

			/* ieee80211 Layer - Default Configuration */
			vap->iv_bsschan = ic->ic_curchan;

			/* This function was being called before "vap->iv_bsschan = ic->ic_curchan" and now moved here.
			 * This fixes a bug where the wme parameters are set to FW before the current channel gets
			 * updated("vap->iv_bsschan"). The "ic_flags" in "iv_bsschan" is used to get the corresponding
			 * phy mode with respect to the channel (ieee80211_chan2mode), and this phy mode is used to set the
			 * wme parameters to FW. IR-088422.
			 */
			ieee80211_wme_initparams(vap);
                        if(memcmp(&wme_zero,&vap->iv_wmestate,sizeof(wme_zero)) != 0){
                              ieee80211_vap_wme_param_update(ic, vap);
                        }
			/* reset erp state */
			ieee80211_reset_erp(ic, ic->ic_curmode, vap->iv_opmode);
		} else {
			/* Copy the wme params from ic to vap structure if vaps are up for the first time */
			if(qdf_mem_cmp(&wme_zero, &vap->iv_wmestate,
				 	sizeof(wme_zero)) == 0)
				qdf_mem_copy(&vap->iv_wmestate,
					     &ic->ic_wme, sizeof(ic->ic_wme));

			/* ieee80211 Layer - Default Configuration */
			vap->iv_bsschan = ic->ic_curchan;
			ieee80211_wme_initparams(vap);
		}

		if (ieee80211_get_num_ap_vaps_up(ic) == 0) {
			/*update channel history*/
			ieee80211_update_chan_history(ic);
		}

		if (vdev->vdev_mlme.bss_chan) {
			mlme_nofl_info("vdev[%d] ieee chan:%d",
				       wlan_vdev_get_id(vdev),
				       vap->iv_bsschan->ic_ieee);
			ieee80211_update_vdev_chan(vdev->vdev_mlme.bss_chan,
						   vap->iv_bsschan);
		}

                ic->ic_opmode = ieee80211_new_opmode(vap, true);

		vap->iv_enable_radar_table(ic, vap, 1, 1);

		if(IEEE80211_IS_CHAN_DFS(ic->ic_curchan))
		{
			/*
			 * When there is radar detect in Repeater, repeater sends RCSAs, CSAs and
			 * switches to new next channel, in ind rpt case repeater AP could start
			 * beaconing before Root comes up, next channel needs to be changed
			 */
			if(!ic->ic_tx_next_ch ||
			   ic->ic_curchan == ic->ic_tx_next_ch)
				ieee80211_update_dfs_next_channel(ic);
		}
		else {
			ic->ic_tx_next_ch = NULL;
		}
		// Tell the vap that the channel change has happened.
		/* For Band steering enabled:- it will be sent from ieee80211_state_event*/
		IEEE80211_DELIVER_EVENT_CHANNEL_CHANGE(vap, ic->ic_curchan);
		ieee80211com_clear_flags(ic, IEEE80211_F_DFS_CHANSWITCH_PENDING);
		vap->channel_switch_state = 0;

		if (restart) {
			ieee80211_update_peer_cw(ic, vap);
		}

		if (opmode == QDF_SAP_MODE)
			error = ieee80211_mlme_create_infra_continue(vap);
		else
			error = wlan_vdev_mlme_sm_deliver_evt_sync(vdev,
				WLAN_VDEV_SM_EV_START_SUCCESS, 0, NULL);
		if (!error)
		    IEEE80211_DELIVER_EVENT_MLME_JOIN_COMPLETE_INFRA(vap,
			    IEEE80211_STATUS_SUCCESS);
		break;
	case QDF_STA_MODE:
                ni = vap->iv_bss;

                chan = ni->ni_chan;

                vap->iv_bsschan = chan;
		ieee80211_update_vdev_chan(vdev->vdev_mlme.bss_chan, chan);

		ic->ic_prevchan = ic->ic_curchan;
		ic->ic_curchan = chan;
		/* update max channel power to max regpower of current channel */
		ieee80211com_set_curchanmaxpwr(ic, chan->ic_maxregpower);

		/* ieee80211 Layer - Default Configuration */
		vap->iv_bsschan = ic->ic_curchan;

		/* XXX reset erp state */
		ieee80211_reset_erp(ic, ic->ic_curmode, vap->iv_opmode);
		ieee80211_wme_initparams(vap);

		vap->iv_enable_radar_table(ic, vap, 0, 0);
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
		if (mlme_sta_cac_start(ic, vap) == QDF_STATUS_SUCCESS) {
			wlan_mlme_inc_act_cmd_timeout(vdev,
					WLAN_SER_CMD_VDEV_START_BSS);
			wlan_vdev_mlme_sm_deliver_evt_sync(vdev,
				WLAN_VDEV_SM_EV_DFS_CAC_WAIT, 0, NULL);
		}
		else
#endif
		{
			ieee80211_mlme_join_infra_continue(vap,EOK);
		}

		if (!vap->iv_quick_reconnect && restart) {
			wlan_vdev_mlme_sm_deliver_evt_sync(vdev,
				WLAN_VDEV_SM_EV_START_SUCCESS, 0, NULL);
		}
		// Tell the vap that the channel change has happened.
		/* For Band steering enabled:- it will be sent from ieee80211_state_event*/
		IEEE80211_DELIVER_EVENT_CHANNEL_CHANGE(vap, ic->ic_curchan);
		ieee80211com_clear_flags(ic, IEEE80211_F_DFS_CHANSWITCH_PENDING);
		vap->channel_switch_state = 0;

		break;
	default:
		break;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_sta_conn_start_cb(struct vdev_mlme_obj *vdev_mlme,
			uint16_t event_data_len, void *event_data)
{
	struct ieee80211vap *vap;
	struct ieee80211_mlme_priv *mlme_priv;
	struct wlan_objmgr_vdev *vdev;

	if (vdev_mlme == NULL) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = vdev_mlme->vdev;
	if (vdev == NULL) {
		mlme_err("VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = vap->iv_mlme_priv;
	if (!mlme_priv) {
		mlme_err("(vdev-id:%d) mlme_priv  is NULL",
			 wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
	mlme_set_stacac_valid(vap,1);
#endif
	mlme_priv->im_request_type = MLME_REQ_JOIN_INFRA;
	ieee80211_mlme_join_infra_continue(vap,EOK);
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
	mlme_priv->im_is_stacac_running = 0;
#endif
	qdf_info("STACAC expired");

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_start_req_failed_cb(struct vdev_mlme_obj *vdev_mlme,
			uint16_t event_data_len, void *event_data)
{
	struct ieee80211vap *vap;
	struct wlan_objmgr_vdev *vdev;

	if (vdev_mlme == NULL) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = vdev_mlme->vdev;
	if (vdev == NULL) {
		mlme_err("VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	if (vap->iv_opmode == IEEE80211_M_STA)
		ieee80211_mlme_join_infra_continue(vap, EINVAL);
	else if (vap->iv_opmode == IEEE80211_M_HOSTAP)
		ieee80211_mlme_create_infra_continue_async(vap, EINVAL);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_notify_up_complete_cb(
                        struct vdev_mlme_obj *vdev_mlme,
			uint16_t event_data_len, void *event_data)
{
	struct wlan_objmgr_vdev *vdev;
	struct ieee80211vap *vap;
	ieee80211_vap_event evt;
	struct wlan_channel *iter_des_chan = NULL;
	struct ieee80211vap *tmpvap;
	struct ieee80211com *ic;
	uint8_t restart_progress = 0;
	uint8_t issue_evt = 0;
	enum wlan_vdev_sm_evt event;

	if (vdev_mlme == NULL) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = vdev_mlme->vdev;
	if (vdev == NULL) {
		mlme_err("VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	evt.type = IEEE80211_VAP_UP;
	ieee80211_vap_deliver_event(vap, &evt);
	wlan_mlme_release_vdev_req(vdev_mlme->vdev,
				   WLAN_SER_CMD_VDEV_START_BSS, EOK);

	ic = vap->iv_ic;
	/* If any VDEV has moved to RESTART_PROGRESS substate, but this VDEV
	 * missed the event, then move to RESTART_PROGRESS sub state
	 */
	if ((wlan_pdev_mlme_op_get(ic->ic_pdev_obj,
				   WLAN_PDEV_OP_RESTART_INPROGRESS)) &&
	    (!wlan_pdev_mlme_op_get(ic->ic_pdev_obj,
                                    WLAN_PDEV_OP_RADAR_DETECT_DEFER))) {
		TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
			if (wlan_vdev_is_restart_progress(tmpvap->vdev_obj) ==
						QDF_STATUS_SUCCESS) {
				iter_des_chan = wlan_vdev_mlme_get_des_chan(
							tmpvap->vdev_obj);
				restart_progress = 1;
				event = WLAN_VDEV_SM_EV_FW_VDEV_RESTART;
				mlme_err("vdev (%d)Issuing vdev SM_EV_FW_VDEV_RESTART",
					 wlan_vdev_get_id(vdev));
				issue_evt = 1;
				break;
			}
		}
	}

	/* If any VDEV has moved to CSA_RESTART sub state, but this non-sta VDEV
	 * missed event, then move to CSA_RESTART sub state
	 */
	if ((!restart_progress) &&
	    (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE)) {
		TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
			if (wlan_vdev_mlme_is_csa_restart(tmpvap->vdev_obj) ==
						QDF_STATUS_SUCCESS) {
				iter_des_chan = wlan_vdev_mlme_get_des_chan(
							tmpvap->vdev_obj);
				event = WLAN_VDEV_SM_EV_CSA_RESTART;
				mlme_err("vdev (%d)Issuing vdev SM_EV_CSA_RESTART",
				 wlan_vdev_get_id(vdev));
				issue_evt = 1;
				break;
			}
		}
	}
	if (iter_des_chan)
		wlan_chan_copy(vdev->vdev_mlme.des_chan, iter_des_chan);

	if (issue_evt)
		wlan_vdev_mlme_sm_deliver_evt_sync(vdev, event, 0, NULL);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_update_beacon_cb(struct vdev_mlme_obj *vdev_mlme,
                        enum beacon_update_op op,
			uint16_t event_data_len, void *event_data)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	struct ieee80211com *ic;
	struct ieee80211vap *vap;

	if (vdev_mlme == NULL) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = vdev_mlme->vdev;

	if (vdev == NULL) {
		mlme_err("VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev == NULL) {
		mlme_err("(vdev-id:%d) PDEV is NULL",
				wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if (!ic) {
		mlme_err("(vdev-id:%d) ic is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	vap->beacon_status = op;

	switch (op) {
	case BEACON_INIT:
		/* handled as part of vap->iv_up() */
		break;
	case BEACON_UPDATE:
		break;
	case BEACON_CSA:
                if (vap->iv_opmode == IEEE80211_M_MONITOR) {
			/* No CSA for Monitor mode, move to RESTART state */
			wlan_vdev_mlme_sm_deliver_evt_sync(vdev,
				WLAN_VDEV_SM_EV_CSA_COMPLETE, 0, NULL);
		}
		else {
		    ic->ic_flags |= IEEE80211_F_CHANSWITCH;
		    wlan_pdev_beacon_update(ic);
                }
		break;
	case BEACON_FREE:
		ic->ic_beacon_free(ic, wlan_vdev_get_id(vdev));
		break;
	default:
		break;
	}

	return QDF_STATUS_SUCCESS;
}

static
void mlme_vdev_peer_timeout_rel_handler(struct ieee80211vap *vap)
{
	qdf_list_t *logical_del_peerlist;
	struct wlan_objmgr_peer *peer;
	qdf_list_node_t *peerlist;
	struct wlan_logically_del_peer *temp_peer = NULL;
	struct ieee80211_node *ni;

	if (!vap || !vap->iv_peer_rel_ref) {
		mlme_err("Null vap, vap: 0x%pK", vap);
		return;
	}

	logical_del_peerlist =
		wlan_objmgr_vdev_get_log_del_peer_list(vap->vdev_obj,
						       WLAN_MLME_SB_ID);
	if (!logical_del_peerlist)
		return;

	while (QDF_IS_STATUS_SUCCESS(qdf_list_remove_front(logical_del_peerlist,
				     &peerlist))) {
		temp_peer = qdf_container_of(peerlist,
					     struct wlan_logically_del_peer,
					     list);
        peer = temp_peer->peer;
        ni = wlan_peer_get_mlme_ext_obj(peer);

		mlme_err("peer mac: %s", ether_sprintf(peer->macaddr));

		if (vap->iv_peer_rel_ref(vap, ni, peer->macaddr))
			mlme_err("Failed to handle peer del failure");
		/*
		 * Release the ref taken during wlan_objmgr_vdev_get_log_del_peer_list
		 */
		wlan_objmgr_peer_release_ref(peer, WLAN_MLME_SB_ID);
		qdf_mem_free(temp_peer);
	}
	qdf_list_destroy(logical_del_peerlist);
	qdf_mem_free(logical_del_peerlist);
}

static os_timer_func(mlme_vdev_peer_del_timeout_handler)
{
	struct ieee80211vap *vap;

	OS_GET_TIMER_ARG(vap, struct ieee80211vap *);
	if (wlan_vdev_get_peer_count(vap->vdev_obj) <= 1)
		return;

	mlme_err("Peer del wait timed out for vdev:%u",
		 wlan_vdev_get_id(vap->vdev_obj));
#if VDEV_ASSERT_MANAGEMENT
	/*
	 * Release references held for pending peer delete response
	 */
	mlme_vdev_peer_timeout_rel_handler(vap);
#else
	/*
	 * Print any leaked peer refs and do FW assert if there are any missing
	 * peer delete responses or mgmt completions for STA peers
	 */
	if (vap->iv_ic->ic_print_peer_refs)
		vap->iv_ic->ic_print_peer_refs(vap, vap->iv_ic, true);
#endif

	return;
}

static void mlme_vdev_cleanup_peers(struct ieee80211vap *vap)
{
	if (wlan_vdev_get_peer_count(vap->vdev_obj) == 1)
		return;

	/* Flush any pending mgmt frame in host queue for this vap */
	ieee80211_flush_vap_mgmt_queue(vap, false);
	/* cleanup all associated sta peers on this vap */
	wlan_iterate_station_list(vap, cleanup_sta_peer, NULL);
	/* cleanup all unassociated sta peers on this vap */
	wlan_iterate_unassoc_sta_list(vap, cleanup_sta_peer, NULL);
	/* flush bss peer tids */
	mlme_ext_vap_flush_bss_peer_tids(vap);

	return;
}

static os_timer_func(mlme_vdev_force_cleanup_peers)
{
	struct ieee80211vap *vap;

	OS_GET_TIMER_ARG(vap, struct ieee80211vap *);

	mlme_vdev_cleanup_peers(vap);
}

static QDF_STATUS mlme_vdev_send_deauth(struct ieee80211vap *vap)
{
	bool send_bcast_deauth = false;
	extern void sta_deauth (void *arg, struct ieee80211_node *ni);

	send_bcast_deauth = g_unicast_deauth_on_stop ? false : true;
	if (g_unicast_deauth_on_stop || wlan_vap_is_pmf_enabled(vap)) {
		mlme_nofl_info("Sending ucast %s to %s associated stas",
                               vap->iv_send_deauth ? "deauth" : "disassoc",
                               wlan_vap_is_pmf_enabled(vap) ? "PMF" : "ALL");

		if (vap->iv_send_deauth)
			wlan_iterate_station_list(vap, sta_deauth, NULL);
		else
			wlan_iterate_station_list(vap, sta_disassoc, NULL);

		wlan_iterate_unassoc_sta_list(vap, sta_deauth, NULL);

                send_bcast_deauth = false;
	}

	if (send_bcast_deauth) {
		/* Send broadcast deauth frames twice */
		mlme_nofl_info("Sending bcast deauth to stas");
		ieee80211_send_deauth(vap->iv_bss, IEEE80211_REASON_AUTH_LEAVE);
		ieee80211_send_deauth(vap->iv_bss, IEEE80211_REASON_AUTH_LEAVE);
		/* cleanup all associated sta peers on this vap */
		wlan_iterate_station_list(vap, cleanup_sta_peer, NULL);
		/* cleanup all unassociated sta peers on this vap */
		wlan_iterate_unassoc_sta_list(vap, cleanup_sta_peer, NULL);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_disconnect_peers_cb(
                        struct vdev_mlme_obj *vdev_mlme,
			uint16_t event_data_len, void *event_data)
{
	struct wlan_objmgr_vdev *vdev;
	struct ieee80211_mlme_priv *mlme_priv;
	struct ieee80211vap *vap;
	struct ol_ath_softc_net80211 *scn;
	ol_ath_soc_softc_t *soc;

	if (vdev_mlme == NULL) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	vdev = vdev_mlme->vdev;
	if (vdev == NULL) {
		mlme_err("VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL",
			wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	mlme_priv = vap->iv_mlme_priv;
	switch(vap->iv_opmode) {
#if UMAC_SUPPORT_IBSS
	case IEEE80211_M_IBSS:
		mlme_stop_adhoc_bss(vap, flags);
		break;
#endif
	case IEEE80211_M_HOSTAP:
	case IEEE80211_M_BTAMP:
		qdf_atomic_set(&vap->kickout_in_progress, 1);
		/* Reset connection state, so that no new connections occur */
		mlme_priv->im_connection_up = 0;
		/* disassoc/deauth all stations */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME|IEEE80211_MSG_AUTH,
			"%s: disassoc/deauth all stations. iv_node_count: %d, peer_cnt: %d\n",
			__func__, vap->iv_node_count, wlan_vdev_get_peer_count(vdev));

		/* flush mgmt frames for this vap to make down faster */
		ieee80211_flush_vap_mgmt_queue(vap, false);

		if ((wlan_vdev_get_peer_count(vdev) == 1) ||
		    (vap->iv_node_count == 1)) {
			mlme_vdev_sm_notify_peers_disconnected(vap);
			qdf_atomic_set(&vap->kickout_in_progress, 0);
			return QDF_STATUS_SUCCESS;
		}
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME|IEEE80211_MSG_AUTH,
                                  "%s: sending %s deauth to all stations.\n",
                                  __func__, g_unicast_deauth_on_stop ? "UNICAST" : "BROADCAST");

		if (vap->force_cleanup_peers)
			mlme_vdev_cleanup_peers(vap);
		else
			mlme_vdev_send_deauth(vap);

		vap->force_cleanup_peers = 0;

		qdf_atomic_set(&vap->kickout_in_progress, 0);
		qdf_timer_mod(&vap->peer_cleanup_timer, 1000);

		if (vap->iv_ic->ic_print_peer_refs) {
			/* Applicable only for lithium platforms */
			scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);
			soc = scn->soc;
			qdf_timer_mod(&vap->peer_del_wait_timer,
				      soc->peer_del_wait_time);
		}
		break;

	case IEEE80211_M_STA:
		/* There should be no mlme requests pending */
		ASSERT(vap->iv_mlme_priv->im_request_type == MLME_REQ_NONE);

		/* Reset state variables */
		mlme_priv->im_connection_up = 0;
		mlme_sta_swbmiss_timer_stop(vap);
		ucfg_son_set_root_dist(vap->vdev_obj,
						SON_INVALID_ROOT_AP_DISTANCE);

		 /* Station BSS peer is deleted after sending STOP request, here
		  * connection sm gives event to move to STOP state*/
		break;

	case IEEE80211_M_MONITOR:
		wlan_vdev_mlme_sm_deliver_evt_sync(vdev,
			WLAN_VDEV_SM_EV_DISCONNECT_COMPLETE, 0, NULL);
		break;
	default:
		break;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_dfs_cac_timer_stop_cb(
                        struct vdev_mlme_obj *vdev_mlme,
			uint16_t event_data_len, void *event_data)
{
	struct wlan_objmgr_pdev *pdev;
        struct ieee80211vap *vap;
	struct ieee80211com *ic;
        uint8_t force = 0;

        vap = wlan_vdev_get_mlme_ext_obj(vdev_mlme->vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL",
			 wlan_vdev_get_id(vdev_mlme->vdev));
		return QDF_STATUS_E_FAILURE;
	}
        ic = vap->iv_ic;

	pdev = ic->ic_pdev_obj;
	if(pdev == NULL) {
		mlme_err("pdev is null");
		return QDF_STATUS_E_FAILURE;
	}

	if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_DFS_ID) !=
			QDF_STATUS_SUCCESS) {
		return QDF_STATUS_E_FAILURE;
	}
	if(!force && (!mlme_dfs_is_ap_cac_timer_running(pdev) ||
				ieee80211_dfs_vaps_in_dfs_wait(ic, vap))) {
		wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);
		return QDF_STATUS_E_FAILURE;
	}
	mlme_dfs_cac_stop(pdev);
	wlan_objmgr_pdev_release_ref(pdev, WLAN_DFS_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_stop_continue_cb(struct vdev_mlme_obj *vdev_mlme,
			uint16_t event_data_len, void *event_data)
{
	struct ieee80211vap *vap;
	struct ieee80211com *ic;
	struct wlan_objmgr_vdev *vdev;

	if (!vdev_mlme) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	vdev = vdev_mlme->vdev;
	if (!vdev) {
		mlme_err("VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL",
			wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

        ic = vap->iv_ic;

        if (IEEE80211_VAP_IS_MBSS_NON_TRANSMIT_ENABLED(vap)) {
            if (ic->ic_mbss.transmit_vap != NULL) {
                /* Delete VAP profile from MBSS IE */
                ieee80211_mbssid_del_profile(vap);
            } else {
                QDF_TRACE(QDF_MODULE_ID_MBSSIE, QDF_TRACE_LEVEL_INFO,
                          "Transmitting VAP has been deleted!\n");
            }
        } else {
            mlme_vdev_update_beacon(vdev_mlme, BEACON_FREE, event_data_len,
                                    event_data);
        }

	son_send_vap_stop_event(vdev_mlme->vdev);

	vap->iv_cleanup(vap);

	/*
	 * In case of AP, peer bss will be reset only at the time of vdev delete,
	 * But in case of STA, BSS peer delete request will be sent to FW for
	 * every instance of vdev up/down.
	 */
	if ((vap->iv_opmode == IEEE80211_M_STA)) {
		qdf_atomic_set(&vap->stop_action_progress, 1);
		ieee80211_reset_bss(vap);
		qdf_atomic_set(&vap->stop_action_progress, 0);
	} else {
		ieee80211_node_reset(vap->iv_bss);
	}

       /* BSS peer node will not be reset during up/down flow in hostap mode,
        * issue bss node freed event only when single node is left which is
	* BSS peer
        */
	if ((vap->iv_opmode == IEEE80211_M_HOSTAP && vap->iv_node_count == 1) ||
	    ((vap->iv_opmode != IEEE80211_M_HOSTAP) &&   /* If !AP and !STA */
	     (vap->iv_opmode != IEEE80211_M_STA))) {
		wlan_vdev_mlme_sm_deliver_evt_sync(vap->vdev_obj,
					WLAN_VDEV_SM_EV_MLME_DOWN_REQ, 0, NULL);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_notify_down_complete_cb(
				struct vdev_mlme_obj *vdev_mlme,
			uint16_t event_data_len, void *event_data)
{

	wlan_mlme_release_vdev_req(vdev_mlme->vdev,
				   WLAN_SER_CMD_VDEV_STOP_BSS, EOK);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_vdev_notify_down_complete(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_mlme_obj *vdev_mlme;

	vdev_mlme = wlan_vdev_mlme_get_cmpt_obj(vdev);
	if (!vdev_mlme)
		return QDF_STATUS_E_FAILURE;

	mlme_vdev_notify_down_complete_cb(vdev_mlme, 0, NULL);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_notify_start_state_exit_cb(struct vdev_mlme_obj *vdev_mlme)
{
	struct wlan_objmgr_pdev *pdev;
        struct ieee80211vap *vap;
	struct ieee80211com *ic;

        vap = wlan_vdev_get_mlme_ext_obj(vdev_mlme->vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL",
			 wlan_vdev_get_id(vdev_mlme->vdev));
		return QDF_STATUS_E_FAILURE;
	}
        ic = vap->iv_ic;

	pdev = ic->ic_pdev_obj;
	if(pdev == NULL) {
		mlme_err("pdev is null");
		return QDF_STATUS_E_FAILURE;
	}

	if (!wlan_pdev_mlme_op_get(pdev, WLAN_PDEV_OP_RADAR_DETECT_DEFER))
		return QDF_STATUS_SUCCESS;

	/* Invoke RADAR detect defer with last VDEV which moves out of Start
         * state
         */
	if (wlan_util_is_pdev_restart_progress(pdev, WLAN_MLME_SB_ID) ==
					QDF_STATUS_SUCCESS)
		return QDF_STATUS_SUCCESS;

	mlme_sm_cmd_schedule_req(pdev, MLME_SM_CMD_PDEV_RADAR_DETECT,
				 ic->ic_radar_defer_chan);

	return QDF_STATUS_SUCCESS;
}


QDF_STATUS mlme_vdev_is_newchan_no_cac_cb(struct vdev_mlme_obj *vdev_mlme)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_channel *des_chan;
	struct ieee80211_ath_channel *curchan;
	struct ieee80211vap *vap;
	struct ieee80211com *ic;
	struct wlan_objmgr_pdev *pdev;

	if (!vdev_mlme) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = vdev_mlme->vdev;
	if (!vdev) {
		mlme_err("VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (!pdev) {
		mlme_err("(vdev-id:%d) PDEV is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if (!ic) {
		mlme_err("(vdev-id:%d) ic is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	des_chan = wlan_vdev_mlme_get_des_chan(vdev);

	curchan = ieee80211_find_dot11_channel(ic, des_chan->ch_ieee,
			des_chan->ch_freq_seg2,
			wlan_vdev_get_ieee_phymode(des_chan->ch_phymode));
	if (!curchan) {
		mlme_err("(vdev-id:%d) des chan(%d) is NULL",
			  wlan_vdev_get_id(vdev), des_chan->ch_ieee);
		return QDF_STATUS_E_FAILURE;
	}

	if (!ieee80211_is_instant_chan_change_possible(vap, curchan)) {
		mlme_err("(vdev-id:%d) des chan(%d) needs CAC",
			  wlan_vdev_get_id(vdev), des_chan->ch_ieee);

                vap->force_cleanup_peers = 1;
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/* Translation table for conversion from QDF mode to IEEE80211 opmode */
uint8_t qdf_opmode2ieee80211_opmode[QDF_MAX_NO_OF_MODE+1] = {
	IEEE80211_M_STA,         /* QDF_STA_MODE,        */
	IEEE80211_M_HOSTAP,      /* QDF_SAP_MODE,        */
	IEEE80211_M_P2P_CLIENT,  /* QDF_P2P_CLIENT_MODE, */
	IEEE80211_M_P2P_GO,      /* QDF_P2P_GO_MODE,     */
	IEEE80211_M_ANY,         /* QDF_FTM_MODE,        */
	IEEE80211_M_IBSS,        /* QDF_IBSS_MODE,       */
	IEEE80211_M_MONITOR,     /* QDF_MONITOR_MODE,    */
	IEEE80211_M_P2P_DEVICE,  /* QDF_P2P_DEVICE_MODE, */
	IEEE80211_M_ANY,         /* QDF_OCB_MODE,        */
	IEEE80211_M_ANY,         /* QDF_EPPING_MODE,     */
	IEEE80211_M_ANY,         /* QDF_QVIT_MODE,       */
	IEEE80211_M_ANY,         /* QDF_NDI_MODE,        */
	IEEE80211_M_WDS,         /* QDF_WDS_MODE,        */
	IEEE80211_M_BTAMP,       /* QDF_BTAMP_MODE,      */
	IEEE80211_M_AHDEMO,      /* QDF_AHDEMO_MODE      */
	IEEE80211_M_ANY,         /* QDF_MAX_NO_OF_MODE   */
};

struct ieee80211com *wlan_sc_get_ic(struct ath_softc_net80211 *scn)
{
    return &scn->sc_ic;
}

QDF_STATUS mlme_pdev_ext_obj_create(struct pdev_mlme_obj *pdev_mlme)
{
	struct ieee80211com *ic;
	struct pdev_osif_priv *osif_priv;
	struct wlan_objmgr_pdev *pdev;

	if (!pdev_mlme) {
		mlme_err("PDEV MLME is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pdev = pdev_mlme->pdev;
	if (!pdev) {
		mlme_err("PDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	* From scn, get ic pointer.
	* since ic is static member of scn, so to have minimal changes
	* using ic from scn instead of allocating
	*/
	osif_priv = wlan_pdev_get_ospriv(pdev);

	ic = wlan_sc_get_ic((struct ath_softc_net80211 *)
				osif_priv->legacy_osif_priv);
	/* store back pointer in pdev */
	ic->ic_pdev_obj = pdev;
	pdev_mlme->ext_pdev_ptr = ic;
	pdev_mlme->mlme_register_ops = mlme_register_ops;

	mlme_restart_timer_init(pdev_mlme);


	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_pdev_ext_obj_destroy(struct pdev_mlme_obj *pdev_mlme)
{
	struct ieee80211com *ic;
	struct wlan_objmgr_pdev *pdev;

	if (!pdev_mlme) {
		mlme_err("PDEV MLME is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pdev = pdev_mlme->pdev;
	if (!pdev) {
		mlme_err("PDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if(ic != NULL) {
		mlme_restart_timer_delete(pdev_mlme);
		ic->ic_pdev_obj = NULL;
	}
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_ext_obj_create(struct vdev_mlme_obj *vdev_mlme)
{
	enum ieee80211_opmode opmode;
	uint8_t *bssid;
	uint8_t *mataddr;
	uint32_t flags;
	struct wlan_objmgr_pdev *pdev;
	wlan_dev_t devhandle;
	wlan_if_t vap;
	struct wlan_objmgr_vdev *vdev;

	mlme_err(" VDEV MLME obj is creation");
	if (vdev_mlme == NULL) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = vdev_mlme->vdev;

	if (vdev == NULL) {
		mlme_err("VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	/* get MAC address from VDEV */
	bssid = wlan_vdev_mlme_get_macaddr(vdev);
	/* get opmode from VDEV */
	opmode = qdf_opmode2ieee80211_opmode[wlan_vdev_mlme_get_opmode(vdev)];
	if (opmode == IEEE80211_M_ANY) {
		mlme_err("qdf opmode %d is not supported",
			 wlan_vdev_mlme_get_opmode(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	mataddr = wlan_vdev_mlme_get_mataddr(vdev);
	flags = vdev->vdev_objmgr.c_flags;
	pdev = wlan_vdev_get_pdev(vdev);
	if(pdev == NULL) {
		mlme_err("PDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	devhandle = (wlan_dev_t )wlan_pdev_get_mlme_ext_obj(pdev);
	if(devhandle == NULL) {
		mlme_err("ic is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vap = wlan_vap_create(devhandle, opmode, 0, flags, bssid, mataddr, 
			(void *)vdev_mlme);
	if(vap == NULL) {
		mlme_err(" VDEV MLME legacy obj creation failed");
		return QDF_STATUS_E_FAILURE;
	}
	vdev_mlme->ext_vdev_ptr = vap;

	return QDF_STATUS_SUCCESS;
}


QDF_STATUS mlme_vdev_ext_obj_post_create(struct vdev_mlme_obj *vdev_mlme)
{
	wlan_if_t vap;
	enum QDF_OPMODE opmode;

	if (vdev_mlme == NULL) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vap = vdev_mlme->ext_vdev_ptr;
	if(vap == NULL) {
		mlme_err("Legacy VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

#if ATH_PERF_PWR_OFFLOAD
	opmode = wlan_vdev_mlme_get_opmode(vap->vdev_obj);
        /* If lp_iot_mode vap then skip turning on BF */
        if ((opmode == QDF_SAP_MODE) &&
                (vap->iv_ic->ic_implicitbf) &&
                !(vap->iv_create_flags & IEEE80211_LP_IOT_VAP)) {
             wlan_set_param(vap, IEEE80211_SUPPORT_IMPLICITBF, 1);
        }
#endif
	ieee80211_vap_attach(vap);
	qdf_timer_init(NULL, &vap->peer_cleanup_timer,
		       mlme_vdev_force_cleanup_peers, (void *)(vap),
		       QDF_TIMER_TYPE_WAKE_APPS);

	qdf_timer_init(NULL, &vap->peer_del_wait_timer,
		       mlme_vdev_peer_del_timeout_handler, (void *)(vap),
		       QDF_TIMER_TYPE_WAKE_APPS);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_ext_obj_destroy(struct vdev_mlme_obj *vdev_mlme)
{
	wlan_if_t vap;

	if (vdev_mlme == NULL) {
		mlme_err("VDEV MLME is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vap = vdev_mlme->ext_vdev_ptr;
	if (vap != NULL) {
		qdf_timer_stop(&vap->peer_cleanup_timer);
		qdf_timer_stop(&vap->peer_del_wait_timer);
		ieee80211_vap_free(vap);
		vdev_mlme->ext_vdev_ptr = NULL;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_start_fw_send_cb(struct wlan_objmgr_vdev *vdev,
					  uint8_t restart)
{
	struct wlan_objmgr_pdev *pdev;
	struct ieee80211com *ic;
	struct wlan_channel *des_chan;
	struct ieee80211_ath_channel *curchan;
	struct ieee80211vap *vap;
	int error;

	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev == NULL) {
		mlme_err("(vdev-id:%d) PDEV is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if (!ic) {
		mlme_err("(vdev-id:%d) ic is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	des_chan = wlan_vdev_mlme_get_des_chan(vdev);

	curchan =  ieee80211_find_dot11_channel(ic, des_chan->ch_ieee,
			des_chan->ch_freq_seg2,
			wlan_vdev_get_ieee_phymode(des_chan->ch_phymode));
	if (!curchan) {
		mlme_err("(vdev-id:%d) des chan(%d) is NULL",
			  wlan_vdev_get_id(vdev), des_chan->ch_ieee);
		return QDF_STATUS_E_FAILURE;
	}
	if (restart)
		vap->restart_txn = 1;
	else
		vap->restart_txn = 0;

	error = mlme_ext_vap_start(vap, curchan, MLME_REQ_ID, 0, restart);
	if (error == EBUSY) {
		error = QDF_STATUS_SUCCESS;
		if (restart)
			ieee80211_send_chanswitch_complete_event(ic);
		}
	else {
		error = QDF_STATUS_E_FAILURE;
	}

	return error;
}

static QDF_STATUS mlme_vdev_up_send_cb(struct vdev_mlme_obj *vdev_mlme,
			uint16_t event_data_len, void *event_data)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_objmgr_pdev *pdev;
	struct ieee80211com *ic;
	struct ieee80211vap *vap;
	bool restart = false;

	if (vdev_mlme == NULL) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev = vdev_mlme->vdev;

	if (vdev == NULL) {
		mlme_err("VDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev == NULL) {
		mlme_err("(vdev-id:%d) PDEV is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if (!ic) {
		mlme_err("(vdev-id:%d) ic is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	if (vap->restart_txn)
		restart = true;

	mlme_ext_vap_up(vap, restart);

        vap->force_cleanup_peers = 0;
	vap->restart_txn = 0;

        /* Send peer authorize command to FW after channel switch announcement */
        if(restart && vap->iv_opmode == IEEE80211_M_STA) {
               if(vap->iv_root_authorize(vap, 1)) {
                      qdf_err("Unable to authorize peer");
               }
               if (ic->ic_repeater_move.state == REPEATER_MOVE_START) {
                   IEEE80211_DELIVER_EVENT_BEACON_MISS(vap);
                   ic->ic_repeater_move.state = REPEATER_MOVE_IN_PROGRESS;
               }
        }

	/* Send the CAC Complete Event to the stavap if it is waiting
	* in state REPEATER_CAC
	*/
	{
		struct ieee80211vap *stavap = NULL;
		STA_VAP_DOWNUP_LOCK(ic);
		stavap = ic->ic_sta_vap;
		if(stavap) {
			int val=0;
			wlan_mlme_sm_get_curstate
				(stavap,
				 IEEE80211_PARAM_CONNECTION_SM_STATE,&val);
			if(val == WLAN_ASSOC_STATE_REPEATER_CAC) {
				IEEE80211_DELIVER_EVENT_MLME_REPEATER_CAC_COMPLETE(stavap,0);
			}
		}
		STA_VAP_DOWNUP_UNLOCK(ic);
	}
#if UNIFIED_SMARTANTENNA
	if ((vap->iv_opmode == IEEE80211_M_HOSTAP) && (ieee80211_get_num_active_vaps(ic) == 1)) {
		QDF_STATUS status;
		status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_SA_API_ID);
		if (QDF_IS_STATUS_ERROR(status)) {
			mlme_err("unable to get vdev reference (smartantenna");
		} else {
			wlan_sa_api_stop(pdev, vdev, SMART_ANT_RECONFIGURE);
			wlan_sa_api_start(pdev, vdev, SMART_ANT_RECONFIGURE);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_SA_API_ID);
		}
	}
#endif

	/* Set default mcast rate */
	ieee80211_set_mcast_rate(vap);

	return QDF_STATUS_SUCCESS;
}


QDF_STATUS mlme_vdev_stop_fw_send_cb(struct wlan_objmgr_vdev *vdev)
{
	struct ieee80211vap *vap;
	ieee80211_vap_event evt;
	struct wlan_objmgr_pdev *pdev;
	unsigned long bringdown_pend_vdev_arr[2];

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	pdev = wlan_vdev_get_pdev(vdev);
	if (pdev == NULL) {
		mlme_err("(vdev-id:%d) PDEV is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	qdf_timer_stop(&vap->peer_cleanup_timer);
	qdf_timer_stop(&vap->peer_del_wait_timer);

	mlme_ext_vap_stop(vap);

	evt.type = IEEE80211_VAP_STOPPING;
	ieee80211_vap_deliver_event(vap, &evt);

	if (wlan_pdev_mlme_op_get(pdev, WLAN_PDEV_OP_RADAR_DETECT_DEFER)) {
		/* Reset the bitmap */
		bringdown_pend_vdev_arr[0] = 0;
		bringdown_pend_vdev_arr[1] = 0;

		wlan_pdev_chan_change_pending_vdevs(pdev,
			bringdown_pend_vdev_arr,
			WLAN_MLME_SB_ID);

		/* If all the pending vdevs goes down, then clear RADAR detect
		 * defer flag
		 */
		if (!bringdown_pend_vdev_arr[0] && !bringdown_pend_vdev_arr[1])
			wlan_pdev_mlme_op_clear
					(pdev,
					 WLAN_PDEV_OP_RADAR_DETECT_DEFER);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_down_fw_send_cb(struct wlan_objmgr_vdev *vdev)
{
	struct ieee80211vap *vap;

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return QDF_STATUS_E_FAILURE;
	}

	mlme_ext_vap_down(vap);

	return QDF_STATUS_SUCCESS;
}

static void wlan_vdev_set_restart_flag(struct wlan_objmgr_pdev *pdev,
				       void *object,
				       void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	unsigned long *send_array = (unsigned long *)arg;
	struct ieee80211vap *vap;

	if (wlan_util_map_index_is_set(send_array, wlan_vdev_get_id(vdev)) ==
					false)
		return;

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return;
	}

	vap->restart_txn = 1;
}

QDF_STATUS mlme_ext_restart_fail_sched_cb(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev = msg->bodyptr;

	wlan_vdev_mlme_sm_deliver_evt(vdev,
				      WLAN_VDEV_SM_EV_RESTART_REQ_FAIL,
				      0, NULL);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_SCHEDULER_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_ext_restart_fail_flush_cb(struct scheduler_msg *msg)
{
	struct wlan_objmgr_vdev *vdev = msg->bodyptr;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_SCHEDULER_ID);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_vdev_multivdev_restart_fw_send_cb(struct wlan_objmgr_pdev *pdev)
{
	struct ieee80211com *ic;
	struct wlan_channel *des_chan;
	struct ieee80211_ath_channel *curchan;
	struct wlan_objmgr_vdev *vdev, *tmp_vdev;
	uint32_t vdev_ids[WLAN_UMAC_PDEV_MAX_VDEVS];
	uint32_t num_vdevs = 0, i;
	struct pdev_mlme_obj *pdev_mlme;
	uint32_t max_vdevs = 0;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};

	if (pdev == NULL) {
		mlme_err("PDEV is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	pdev_mlme = wlan_pdev_mlme_get_cmpt_obj(pdev);
	if (!pdev_mlme) {
		mlme_err(" PDEV MLME is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	ic = wlan_pdev_get_mlme_ext_obj(pdev);
	if (!ic) {
		mlme_err("ic is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	max_vdevs = wlan_psoc_get_max_vdev_count(wlan_pdev_get_psoc(pdev));
	for (i = 0; i < max_vdevs; i++) {
		if (wlan_util_map_index_is_set(
			pdev_mlme->restart_send_vdev_bmap, i)
					== false)
			continue;

		vdev_ids[num_vdevs] = i;
		num_vdevs++;
	}

	if (num_vdevs == 0)
		return QDF_STATUS_E_FAILURE;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_ids[0],
						    WLAN_MLME_NB_ID);
	if (!vdev)
		return QDF_STATUS_E_FAILURE;

	des_chan = wlan_vdev_mlme_get_des_chan(vdev);

	curchan =  ieee80211_find_dot11_channel(ic, des_chan->ch_ieee,
			des_chan->ch_freq_seg2,
			wlan_vdev_get_ieee_phymode(des_chan->ch_phymode));

	mlme_err("(vdev-id:%d) des chan(%d)",
			  wlan_vdev_get_id(vdev), des_chan->ch_ieee);
	if (!curchan) {
		mlme_err("(vdev-id:%d) des chan(%d) is NULL",
			  wlan_vdev_get_id(vdev), des_chan->ch_ieee);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	ic->ic_prevchan = ic->ic_curchan;
	ic->ic_curchan = curchan;
	wlan_objmgr_pdev_iterate_obj_list(pdev,
			WLAN_VDEV_OP,
			wlan_vdev_set_restart_flag,
			pdev_mlme->restart_send_vdev_bmap, 0,
			WLAN_MLME_NB_ID);

	status = mlme_ext_multi_vdev_restart(ic, vdev_ids, num_vdevs);
	if (QDF_IS_STATUS_ERROR(status)) {
		for (i = 0; i < num_vdevs; i++) {
			tmp_vdev = wlan_objmgr_get_vdev_by_id_from_pdev(pdev,
					vdev_ids[i], WLAN_SCHEDULER_ID);
			if (tmp_vdev == NULL)
				continue;
			mlme_info("Multivdev restart failure vdev:%u",
				  wlan_vdev_get_id(tmp_vdev));
			msg.bodyptr = tmp_vdev;
			msg.callback = mlme_ext_restart_fail_sched_cb;
			msg.flush_callback = mlme_ext_restart_fail_flush_cb;
			scheduler_post_message(QDF_MODULE_ID_MLME,
					QDF_MODULE_ID_MLME,
					QDF_MODULE_ID_MLME, &msg);
		}
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_NB_ID);
	ieee80211_send_chanswitch_complete_event(ic);

	return QDF_STATUS_SUCCESS;

}

static QDF_STATUS mlme_vdev_ext_delete_response_cb(
				     struct vdev_mlme_obj *vdev_mlme,
                                     struct vdev_delete_response *rsp)
{
   return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlme_vdev_ext_stop_response_cb(
				     struct vdev_mlme_obj *vdev_mlme,
                                     struct vdev_stop_response *rsp)
{
   struct ieee80211vap *vap = vdev_mlme->ext_vdev_ptr;

   wlan_vdev_mlme_sm_deliver_evt(vap->vdev_obj, WLAN_VDEV_SM_EV_STOP_RESP,
		   		 0, NULL);

   return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlme_vdev_ext_start_response_cb(
				     struct vdev_mlme_obj *vdev_mlme,
                                     struct vdev_start_response *rsp)
{
   return mlme_ext_vap_start_response_event_handler(rsp, vdev_mlme);
}


static struct vdev_mlme_ops mlme_ops = {
	.mlme_vdev_reset_proto_params = mlme_vdev_reset_proto_params_cb,
	.mlme_vdev_start_continue = mlme_vdev_start_continue_cb,
	.mlme_vdev_sta_conn_start = mlme_vdev_sta_conn_start_cb,
	.mlme_vdev_start_req_failed = mlme_vdev_start_req_failed_cb,
	.mlme_vdev_up_send = mlme_vdev_up_send_cb,
	.mlme_vdev_notify_up_complete = mlme_vdev_notify_up_complete_cb,
	.mlme_vdev_update_beacon = mlme_vdev_update_beacon_cb,
	.mlme_vdev_disconnect_peers = mlme_vdev_disconnect_peers_cb,
	.mlme_vdev_dfs_cac_timer_stop = mlme_vdev_dfs_cac_timer_stop_cb,
	.mlme_vdev_stop_continue = mlme_vdev_stop_continue_cb,
	.mlme_vdev_notify_down_complete = mlme_vdev_notify_down_complete_cb,
	.mlme_vdev_ext_delete_rsp = mlme_vdev_ext_delete_response_cb,
	.mlme_vdev_ext_stop_rsp = mlme_vdev_ext_stop_response_cb,
	.mlme_vdev_ext_start_rsp = mlme_vdev_ext_start_response_cb,
	.mlme_vdev_notify_start_state_exit =
					mlme_vdev_notify_start_state_exit_cb,
	.mlme_vdev_is_newchan_no_cac = mlme_vdev_is_newchan_no_cac_cb,
};

struct mlme_ext_ops glbl_ops_ext = {
	.mlme_pdev_ext_hdl_create = mlme_pdev_ext_obj_create,
	.mlme_pdev_ext_hdl_destroy = mlme_pdev_ext_obj_destroy,
	.mlme_vdev_ext_hdl_create = mlme_vdev_ext_obj_create,
	.mlme_vdev_ext_hdl_post_create = mlme_vdev_ext_obj_post_create,
	.mlme_vdev_ext_hdl_destroy = mlme_vdev_ext_obj_destroy,
	.mlme_vdev_start_fw_send = mlme_vdev_start_fw_send_cb,
	.mlme_vdev_stop_fw_send = mlme_vdev_stop_fw_send_cb,
	.mlme_vdev_down_fw_send = mlme_vdev_down_fw_send_cb,
	.mlme_multivdev_restart_fw_send =
				mlme_vdev_multivdev_restart_fw_send_cb,
	.mlme_vdev_enqueue_exp_cmd = NULL,
};

QDF_STATUS mlme_register_ops(struct vdev_mlme_obj *vdev_mlme)
{
	if (!vdev_mlme) {
		mlme_err("VDEV MLME obj is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	vdev_mlme->ops = &mlme_ops;

	mlme_register_cmn_ops(vdev_mlme);

	return QDF_STATUS_SUCCESS;
}

struct mlme_ext_ops *mlme_get_global_ops(void)
{
	return &glbl_ops_ext;
}

void mlme_vdev_sm_notify_peers_disconnected(struct ieee80211vap *vap)
{
	struct wlan_objmgr_vdev *vdev = vap->vdev_obj;
	u_int32_t vap_node_count = 0;

	qdf_spin_lock_bh(&vap->peer_free_notif_lock);

	if (vap->iv_opmode == IEEE80211_M_STA) {
		if (qdf_atomic_read(&(vap->stop_action_progress)))
			wlan_vdev_mlme_sm_deliver_evt_sync(vdev,
				WLAN_VDEV_SM_EV_MLME_DOWN_REQ, 0, NULL);
		else
			wlan_vdev_mlme_sm_deliver_evt(vdev,
				WLAN_VDEV_SM_EV_MLME_DOWN_REQ, 0, NULL);
	}
	else {
		IEEE80211_VAP_LOCK(vap);
		vap_node_count = vap->iv_node_count;
		IEEE80211_VAP_UNLOCK(vap);
		if (vap_node_count == 1) {
			qdf_timer_stop(&vap->peer_cleanup_timer);
			qdf_timer_stop(&vap->peer_del_wait_timer);
			if (qdf_atomic_read(&(vap->kickout_in_progress)))
				wlan_vdev_mlme_sm_deliver_evt_sync(vdev,
					WLAN_VDEV_SM_EV_DISCONNECT_COMPLETE,
					 0, NULL);
			else
				wlan_vdev_mlme_sm_deliver_evt(vdev,
					WLAN_VDEV_SM_EV_DISCONNECT_COMPLETE,
					0, NULL);
		}
	}
	qdf_spin_unlock_bh(&vap->peer_free_notif_lock);
}

void mlme_vdev_sm_notify_conn_sm_init_state(struct ieee80211vap *vap)
{
	struct wlan_objmgr_vdev *vdev = vap->vdev_obj;
	enum wlan_vdev_state state;
	enum wlan_vdev_state substate;
	enum wlan_vdev_sm_evt event;

	if (vap->iv_opmode != IEEE80211_M_STA)
		return;

	state = wlan_vdev_mlme_get_state(vdev);
	substate = wlan_vdev_mlme_get_substate(vdev);

	if (state == WLAN_VDEV_S_INIT) {
		event = WLAN_VDEV_SM_EV_DOWN;
	}
        else {
		if (state == WLAN_VDEV_S_START) {
		    if (substate == WLAN_VDEV_SS_START_DISCONN_PROGRESS)
			event = WLAN_VDEV_SM_EV_CONNECTION_FAIL;
		    else if (substate == WLAN_VDEV_SS_START_RESTART_PROGRESS ||
		             substate == WLAN_VDEV_SS_START_START_PROGRESS)
		        event = WLAN_VDEV_SM_EV_DOWN;
                    else
			event = WLAN_VDEV_SM_EV_DISCONNECT_COMPLETE;
                }
		else {
			event = WLAN_VDEV_SM_EV_DISCONNECT_COMPLETE;
                }
	}

	wlan_vdev_mlme_sm_deliver_evt(vdev, event, 0, NULL);
}

static void mlme_vdev_sm_radar_notify(struct wlan_objmgr_pdev *pdev,
				      void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct ieee80211_ath_channel *new_chan = arg;

	if (wlan_vdev_mlme_get_opmode(vdev) != QDF_STA_MODE) {
		if (vdev->vdev_mlme.des_chan)
			ieee80211_update_vdev_chan(vdev->vdev_mlme.des_chan,
						   new_chan);

		wlan_vdev_mlme_sm_deliver_evt(vdev,
					      WLAN_VDEV_SM_EV_RADAR_DETECTED,
					      0, NULL);
	}
}

void
wlan_pdev_mlme_vdev_sm_notify_radar_ind(struct wlan_objmgr_pdev *pdev,
					struct ieee80211_ath_channel *new_chan)
{
	struct ieee80211com *ic;

	if (!pdev)
		return;

	if (wlan_util_is_pdev_restart_progress(pdev, WLAN_MLME_SB_ID) ==
					QDF_STATUS_SUCCESS) {
		if (wlan_pdev_mlme_op_get(pdev,
					  WLAN_PDEV_OP_RESTART_INPROGRESS)) {
			mlme_info("Channel change is in progress, ignore radar detect event");
			return;
		}
		ic = wlan_pdev_get_mlme_ext_obj(pdev);
		if (!ic) {
			mlme_err("ic is NULL");
			return;
		}
		ic->ic_radar_defer_chan = new_chan;

		/* Defer RADAR detection as RESTART is in progress */
		wlan_pdev_mlme_op_set(pdev, WLAN_PDEV_OP_RADAR_DETECT_DEFER);
	} else {
		wlan_pdev_mlme_op_set(pdev, WLAN_PDEV_OP_MBSSID_RESTART);

		wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  mlme_vdev_sm_radar_notify,
					  new_chan, 0, WLAN_MLME_SB_ID);
		/* clear defer RADAR detection as RESTART is triggered */
		wlan_pdev_mlme_op_clear(pdev, WLAN_PDEV_OP_RADAR_DETECT_DEFER);
	}
}


static void mlme_vdev_sm_nol_chan_change(struct wlan_objmgr_pdev *pdev,
				      void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct ieee80211_ath_channel *new_chan = arg;
	struct ieee80211vap *vap;

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return;
	}

#if defined(WLAN_DFS_FULL_OFFLOAD) && defined(QCA_DFS_NOL_OFFLOAD)
        vap->vap_start_failure_action_taken = true;
#endif
	if (vdev->vdev_mlme.des_chan)
		ieee80211_update_vdev_chan(vdev->vdev_mlme.des_chan,
					   new_chan);

	/* If VDEV is in RESTART_PROGRESS substate, do not notify radar */
	if (wlan_vdev_is_restart_progress(vdev) == QDF_STATUS_SUCCESS)
		return;

	/* For STA, it should do beacon miss */
	if ((vap->iv_opmode == IEEE80211_M_STA) &&
	    (wlan_vdev_is_up(vdev) == QDF_STATUS_SUCCESS)) {
		wlan_scan_update_channel_list(vap->iv_ic);
		IEEE80211_DELIVER_EVENT_BEACON_MISS(vap);
	} else {
		wlan_vdev_mlme_sm_deliver_evt(vdev,
					      WLAN_VDEV_SM_EV_RADAR_DETECTED,
					      0, NULL);
	}
}

void
wlan_pdev_mlme_vdev_sm_notify_chan_failure(struct wlan_objmgr_pdev *pdev,
					struct ieee80211_ath_channel *new_chan)
{
	if (!pdev)
		return;

	wlan_pdev_mlme_op_set(pdev, WLAN_PDEV_OP_MBSSID_RESTART);
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  mlme_vdev_sm_nol_chan_change,
					  new_chan, 0, WLAN_MLME_SB_ID);
}

static void mlme_vdev_sm_csa_restart(struct wlan_objmgr_pdev *pdev,
				      void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct ieee80211_ath_channel *new_chan = arg;
	struct ieee80211vap *vap;

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return;
	}

	if(vap->iv_opmode == IEEE80211_M_HOSTAP ||
	   vap->iv_opmode == IEEE80211_M_MONITOR) {
		ieee80211vap_set_flag(vap, IEEE80211_F_CHANSWITCH);
		if (vdev->vdev_mlme.des_chan)
			ieee80211_update_vdev_chan(vdev->vdev_mlme.des_chan,
						   new_chan);
		wlan_vdev_mlme_sm_deliver_evt(vdev, WLAN_VDEV_SM_EV_CSA_RESTART,
					      0, NULL);
	}
}

void
wlan_pdev_mlme_vdev_sm_csa_restart(struct wlan_objmgr_pdev *pdev,
				   struct ieee80211_ath_channel *new_chan)
{
	if (!pdev)
		return;

	wlan_pdev_mlme_op_set(pdev, WLAN_PDEV_OP_MBSSID_RESTART);
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  mlme_vdev_sm_csa_restart,
					  new_chan, 0, WLAN_MLME_SB_ID);
}

static void mlme_vdev_sm_chan_change(struct wlan_objmgr_pdev *pdev,
				      void *object, void *arg)
{
	struct wlan_objmgr_vdev *vdev = (struct wlan_objmgr_vdev *)object;
	struct ieee80211_ath_channel *new_chan = arg;
	struct ieee80211vap *vap;
	enum wlan_vdev_sm_evt event;

	vap = wlan_vdev_get_mlme_ext_obj(vdev);
	if (!vap) {
		mlme_err("(vdev-id:%d) vap  is NULL", wlan_vdev_get_id(vdev));
		return;
	}

	if(vap->iv_opmode == IEEE80211_M_HOSTAP ||
	   vap->iv_opmode == IEEE80211_M_MONITOR) {
		if (vdev->vdev_mlme.des_chan)
			ieee80211_update_vdev_chan(vdev->vdev_mlme.des_chan,
						   new_chan);

		if (wlan_vdev_is_up(vdev) == QDF_STATUS_SUCCESS)
			event = WLAN_VDEV_SM_EV_SUSPEND_RESTART;
		else
			event = WLAN_VDEV_SM_EV_RADAR_DETECTED;

		wlan_vdev_mlme_sm_deliver_evt(vdev, event, 0, NULL);
	} else if (vap->iv_opmode == IEEE80211_M_STA) {
		wlan_scan_update_channel_list(vap->iv_ic);
		IEEE80211_DELIVER_EVENT_BEACON_MISS(vap);
	}
}

void
wlan_pdev_mlme_vdev_sm_chan_change(struct wlan_objmgr_pdev *pdev,
				   struct ieee80211_ath_channel *new_chan)
{
	if (!pdev)
		return;

	wlan_pdev_mlme_op_set(pdev, WLAN_PDEV_OP_MBSSID_RESTART);
	wlan_objmgr_pdev_iterate_obj_list(pdev, WLAN_VDEV_OP,
					  mlme_vdev_sm_chan_change,
					  new_chan, 0, WLAN_MLME_SB_ID);
}

static void mlme_vdev_send_fw_vdev_restart(struct wlan_objmgr_vdev *vdev,
				  struct ieee80211_ath_channel *new_chan)
{
	if (vdev->vdev_mlme.des_chan)
		ieee80211_update_vdev_chan(vdev->vdev_mlme.des_chan, new_chan);

	wlan_vdev_mlme_sm_deliver_evt(vdev, WLAN_VDEV_SM_EV_FW_VDEV_RESTART, 0,
				      NULL);
}

void wlan_pdev_mlme_vdev_sm_seamless_chan_change
				(struct wlan_objmgr_pdev *pdev,
				 struct wlan_objmgr_vdev *vdev,
				 struct ieee80211_ath_channel *new_chan)
{
	if (!vdev)
		return;

	wlan_pdev_mlme_op_set(pdev, WLAN_PDEV_OP_MBSSID_RESTART);
	mlme_vdev_send_fw_vdev_restart(vdev, new_chan);
}
