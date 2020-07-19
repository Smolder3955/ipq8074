/*
 * Copyright (c) 2011-2014, 2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#include <scheduler_api.h>
#include <wlan_dfs_ucfg_api.h>
#include <dfs.h>
#include <wlan_dfs_mlme_api.h>
#include <wlan_vdev_mlme_ser_if.h>
#include <wlan_mlme_if.h>
#include <wlan_vdev_mgr_utils_api.h>

#if UMAC_SUPPORT_CFG80211
#include <ieee80211_cfg80211.h>
#endif

#if SM_ENG_HIST_ENABLE
#include <wlan_sm_engine_dbg.h>

#define VDEV_MLME_LINE "-----------------------------------"\
		       "-----------------------------------"\
		       "-----------------------------------"

void wlan_mlme_print_vdev_sm_history(struct wlan_objmgr_psoc *psoc,
					    void *obj,
					    void *arg)
{
	struct vdev_mlme_obj *vdev_mlme = NULL;
	struct wlan_sm *vdev_sm = NULL;
	struct wlan_objmgr_vdev *vdev = NULL;
	uint8_t psoc_index;

	psoc_index = *((uint8_t *)arg);

	vdev = (struct wlan_objmgr_vdev *)obj;
	if (!vdev) {
		mlme_err("VDEV is null");
		goto error;
	}
	vdev_mlme = wlan_objmgr_vdev_get_comp_private_obj(
			vdev,
			WLAN_UMAC_COMP_MLME);
	if (!vdev_mlme) {
		mlme_err("VDEV MLME is null");
		goto error;
	}

	vdev_sm = vdev_mlme->sm_hdl;

	if (!vdev_sm) {
		mlme_err("VDEV SM is null");
		goto error;
	}

	mlme_nofl_err(VDEV_MLME_LINE);
	mlme_nofl_err("SM History: PSOC ID: %d VDEV ID: %d",
		      psoc_index,
		      wlan_vdev_get_id(vdev));
	mlme_nofl_err(VDEV_MLME_LINE);
	wlan_sm_print_history(vdev_sm);

error:
	return;
}

void wlan_mlme_psoc_iterate_handler(
		struct wlan_objmgr_psoc *psoc,
		void *arg, uint8_t index)
{
	uint8_t psoc_index = index;
	wlan_objmgr_iterate_obj_list(psoc, WLAN_VDEV_OP,
				     wlan_mlme_print_vdev_sm_history,
				     &psoc_index, false, WLAN_MLME_NB_ID);

	return;
}

void wlan_mlme_print_all_sm_history(void)
{
	wlan_objmgr_iterate_psoc_list(
			wlan_mlme_psoc_iterate_handler,
			NULL,
			WLAN_MLME_NB_ID);
}
#endif

QDF_STATUS mlme_is_active_cmd_in_sched_ctx(void *umac_cmd)
{
    struct wlan_mlme_ser_data *data = (struct wlan_mlme_ser_data *)umac_cmd;

    if (!data) {
        wlan_mlme_err("Null data");
        return QDF_STATUS_E_INVAL;
    }

    if (data->activation_ctx == MLME_CTX_SCHED)
        return QDF_STATUS_SUCCESS;
    else
        return QDF_STATUS_E_FAILURE;
}

static QDF_STATUS
wlan_mlme_start_ap_vdev(struct wlan_serialization_command *cmd)
{
    wlan_dev_t comhandle;
    wlan_chan_t chan;
    int error;
    struct wlan_objmgr_vdev *vdev = cmd->vdev;
    wlan_if_t vap;
    osif_dev *osifp;
    struct wlan_mlme_ser_data *data = cmd->umac_cmd;
    int forcescan = data->flags;
    enum ieee80211_phymode des_mode;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap) {
        wlan_mlme_err("vap is NULL");
        return QDF_STATUS_E_FAILURE;
    }

    osifp = (osif_dev *)vap->iv_ifp;
    des_mode = wlan_get_desired_phymode(vap);

    if (vap->iv_ic->recovery_in_progress) {
        wlan_mlme_err("recovery_in_progress on vap %d", vap->iv_unit);
        return QDF_STATUS_E_FAILURE;
    }

    if (wlan_vdev_mlme_get_opmode(vdev) == QDF_MONITOR_MODE){
        wlan_mlme_start_monitor(vap);
        return QDF_STATUS_SUCCESS;
    }

    comhandle = wlan_vap_get_devhandle(vap);
    if(comhandle->ic_sta_vap && wlan_is_connected(comhandle->ic_sta_vap)) {
        chan = wlan_get_current_channel(vap, true);
    } else {
        chan = wlan_get_current_channel(vap, false);
    }

#if WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
    if(chan && chan != IEEE80211_CHAN_ANYC &&
            !ieee80211_vap_ext_ifu_acs_is_set(vap) &&
            vap->iv_ic->ic_primary_allowed_enable &&
            !(ieee80211_check_allowed_prim_chanlist(
                    vap->iv_ic,chan->ic_ieee))) {
        wlan_mlme_err("channel %d is not a primary allowed channel",
                      chan->ic_ieee);
        chan = NULL;
    }
#endif

    if ((!chan) || (chan == IEEE80211_CHAN_ANYC)) {
        if (ieee80211_vap_ext_ifu_acs_is_set(vap)) {
            /*
             * If any VAP is active, the channel is already selected
             * so go ahead and init the VAP, the same channel will be used
             */
            if (ieee80211_vaps_active(vap->iv_ic)) {
                /* Set the curent channel to the VAP */
                wlan_chan_t sel_chan;
                int sel_ieee_chan = 0;
                sel_chan = wlan_get_current_channel(vap, true);
                sel_ieee_chan = wlan_channel_ieee(sel_chan);
                error = wlan_set_channel(vap, sel_ieee_chan,
                        sel_chan->ic_vhtop_ch_freq_seg2);
                wlan_mlme_info("wlan_set_channel error code: %d", error);
                if (error !=0) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACS,
                            "%s : failed to set channel with error code %d\n",
                            __func__, error);
                    return QDF_STATUS_E_FAILURE;
                }
            } else {
                /*
                 * No VAPs active. An external entity is responsible for
                 * auto channel selection at VAP init time
                 */
                wlan_mlme_info("No active vaps");
                return QDF_STATUS_E_FAILURE;
            }
        } else {
            /* start ACS module to get channel */
            vap->iv_needs_up_on_acs = 1;
            wlan_autoselect_register_event_handler(vap,
                    &osif_acs_event_handler, (void *)osifp);
            wlan_autoselect_find_infra_bss_channel(vap, NULL);
            wlan_mlme_debug("started wlan_autoselect_find_infra_bss_channel");
            return QDF_STATUS_SUCCESS;
        }
    }

    /*
     * 11AX: Recheck future 802.11ax drafts (>D1.0) on coex rules
     */
    if (((des_mode == IEEE80211_MODE_11NG_HT40PLUS) ||
                (des_mode == IEEE80211_MODE_11NG_HT40MINUS) ||
                (des_mode == IEEE80211_MODE_11NG_HT40) ||
                (des_mode == IEEE80211_MODE_11AXG_HE40PLUS) ||
                (des_mode == IEEE80211_MODE_11AXG_HE40MINUS) ||
                (des_mode == IEEE80211_MODE_11AXG_HE40)) &&
            (wlan_coext_enabled(vap) &&
             (osif_get_num_running_vaps(comhandle) == 0))) {

        wlan_autoselect_register_event_handler(vap,
                &osif_ht40_event_handler, (void *)osifp);
        wlan_attempt_ht40_bss(vap);
        vap->iv_needs_up_on_acs = 1;
        wlan_mlme_debug("started wlan_attempt_ht40_bss");
        return QDF_STATUS_SUCCESS;
    }

    if (forcescan)
        vap->iv_rescan = 1;

    IEEE80211_VAP_LOCK(vap);
    if (wlan_vdev_scan_in_progress(vap->vdev_obj)) {
        /* do not start AP if there is already a pending vap_init */
        IEEE80211_VAP_UNLOCK(vap);
        wlan_mlme_debug("Scan in progress for vdev:%d",
                      wlan_vdev_get_id(vap->vdev_obj));
        return QDF_STATUS_E_FAILURE;
    }
    IEEE80211_VAP_UNLOCK(vap);

    error = wlan_mlme_start_bss(vap, forcescan);
    if (error) {
        /* Radio resource is busy on scanning, try later */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "%s :mlme busy mostly scanning \n", __func__);
        return QDF_STATUS_E_FAILURE;
    } else {
        wlan_mlme_connection_up(vap);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL, "%s :vap up \n", __func__);
        if (wlan_coext_enabled(vap))
            wlan_determine_cw(vap, vap->iv_ic->ic_curchan);
    }

    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
wlan_mlme_start_sta_vdev(struct wlan_serialization_command *cmd)
{
    int error = QDF_STATUS_SUCCESS;
    struct wlan_objmgr_vdev *vdev = cmd->vdev;
    wlan_if_t vap = wlan_vdev_get_mlme_ext_obj(vdev);
    int is_ap_cac_timer_running = 0;
    osif_dev *osifp;

    if (!vap) {
        wlan_mlme_err("vap is NULL");
        return QDF_STATUS_E_FAILURE;
    }

    osifp = (osif_dev *)vap->iv_ifp;
    if (osifp->authmode == IEEE80211_AUTH_AUTO) {
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
        /* If the auth mode is set to AUTO, set the auth mode
         * to SHARED for first connection try */
        ieee80211_auth_mode modes[1];
        u_int nmodes=1;
        modes[0] = IEEE80211_AUTH_SHARED;
        wlan_set_authmodes(vap,modes,nmodes);
#else
        wlan_crypto_set_vdev_param(vap->vdev_obj,
                WLAN_CRYPTO_PARAM_AUTH_MODE, 1 << WLAN_CRYPTO_AUTH_SHARED);
#endif
    }

    ucfg_dfs_is_ap_cac_timer_running(vap->iv_ic->ic_pdev_obj,
            &is_ap_cac_timer_running);
    if(is_ap_cac_timer_running) {
        return QDF_STATUS_E_FAILURE;
    }
#if DBDC_REPEATER_SUPPORT
    if ((vap->iv_ic->ic_global_list->same_ssid_support)) {
        if ((vap == vap->iv_ic->ic_sta_vap) &&
                (IS_NULL_ADDR(vap->iv_ic->ic_preferred_bssid))) {
            /*
             * When same_ssid_support is enabled and preferred_bssid is not
             * set, don't start connection. Start connection when connect
             * request comes from supplicant
             */
            return QDF_STATUS_E_FAILURE;
        }
    }
#endif
    error = wlan_connection_sm_start(osifp->sm_handle);

    if (error == -EINPROGRESS) {
        wlan_ser_print_history(vdev, WLAN_SER_CMD_NONSCAN,
                               WLAN_SER_CMD_NONSCAN);
        QDF_ASSERT(0);
    }

    if (error) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_IOCTL,
                "Error %s : all sm start attempt failed\n", __func__);
        return QDF_STATUS_E_FAILURE;
    }
    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
wlan_mlme_stop_ap_vdev(struct wlan_serialization_command *cmd)
{
    struct wlan_objmgr_vdev *vdev = cmd->vdev;
    wlan_if_t vap;
    osif_dev *osifp;
    struct net_device *dev;
    struct ieee80211com *ic;
    struct wlan_mlme_ser_data *data = cmd->umac_cmd;
    enum QDF_OPMODE opmode = wlan_vdev_mlme_get_opmode(vdev);
    int flags = 0;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap) {
        wlan_mlme_err("vap is NULL");
        return QDF_STATUS_E_FAILURE;
    }

    osifp = (osif_dev *)vap->iv_ifp;
    dev = osifp->netdev;
    ic = vap->iv_ic;

    if (opmode == QDF_MONITOR_MODE) {
        flags =  WLAN_MLME_STOP_BSS_F_NO_RESET |
                 WLAN_MLME_STOP_BSS_F_FORCE_STOP_RESET;
        wlan_mlme_stop_bss(vap, flags);
        return QDF_STATUS_SUCCESS;
    }
    /* Cancel CBS */
    wlan_bk_scan_stop_async(ic);
    osif_vap_acs_cancel(dev, 0);
    if (wlan_vdev_scan_in_progress(vap->vdev_obj)) {
        wlan_mlme_debug("Scan in progress.. Cancelling it. vap: 0x%pK", vap);
        wlan_ucfg_scan_cancel(vap, osifp->scan_requestor, 0,
                WLAN_SCAN_CANCEL_VDEV_ALL, false);
#if UMAC_SUPPORT_CFG80211
        if (ic->ic_cfg80211_config)
            wlan_cfg80211_cleanup_scan_queue(ic->ic_pdev_obj, dev);
#endif
        /* we know a scan was interrupted because we're stopping
         * the VAP. This may lead to an invalid channel pointer.
         * Thus, initialise it with the default channel information
         * from the ic to prevent a crash in
         * ieee80211_init_node_rates() during bss reset.
         */
        if (vap->iv_bsschan == IEEE80211_CHAN_ANYC) {
            wlan_mlme_err("Info: overwriting invalid BSS channel"
                    "info by defaults (%pK)", vap->iv_ic->ic_curchan);
            vap->iv_bsschan = vap->iv_ic->ic_curchan;
        }
    }

    flags = WLAN_MLME_STOP_BSS_F_NO_RESET;
    if (data)
        flags |= data->flags;
    wlan_mlme_stop_bss(vap, flags);
    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
wlan_mlme_stop_sta_vdev(struct wlan_serialization_command *cmd)
{
    int waitcnt = 0;
    struct wlan_objmgr_vdev *vdev = cmd->vdev;
    wlan_if_t vap = wlan_vdev_get_mlme_ext_obj(vdev);
    osif_dev *osifp;
    struct net_device *dev;
    u_int32_t flags;
    struct ieee80211com *ic;

    if (!vap) {
        wlan_mlme_err(" vap is NULL");
        return QDF_STATUS_E_INVAL;
    }

    osifp = (osif_dev *)vap->iv_ifp;
    if (!osifp || !osifp->sm_handle) {
        wlan_mlme_err(" osifp or sm_handle is NULL");
        return QDF_STATUS_E_FAILURE;
    }

    ic = vap->iv_ic;
    dev = osifp->netdev;

    /* Cancel all pending scans on this vap synchronously.
     *
     * Its required to ensure that vap state machine is in init
     * state. Otherwise connection state machine may jump to
     * connecting state leaving vap state machine to scanning state.
     * above scenerio will lead to below sequence:
     * a. vap and connection state machines are in init state
     * b. iwlist scan -> vap sate machine moves to scan state
     * c. set essid/bssid -> connection state machine finds ssid in
     *    scan list, creates bss peer and sends vdev start to FW
     * d. scan completes -> vap state machine moves to init state
     * e. vap state machine sends vdev down
     * f. FW crash in HK as vdev down sent without deleting ap peer
     */

    /* Cancel CBS */
    wlan_bk_scan_stop_async(ic);
    osif_vap_acs_cancel(dev, 0);
    wlan_ucfg_scan_cancel(vap, osifp->scan_requestor, INVAL_SCAN_ID,
            WLAN_SCAN_CANCEL_VDEV_ALL, false);

#if UMAC_SUPPORT_CFG80211
    if (ic->ic_cfg80211_config)
        wlan_cfg80211_cleanup_scan_queue(ic->ic_pdev_obj, dev);
#endif

    if (osifp->is_delete_in_progress)
        goto stop_conn_sm;

    if (!osifp->is_restart &&
            wlan_get_param(vap, IEEE80211_FEATURE_WDS)) {
        /* Disable bgscan  for WDS STATION */
        wlan_connection_sm_set_param(osifp->sm_handle,
                WLAN_CONNECTION_PARAM_BGSCAN_POLICY,
                WLAN_CONNECTION_BGSCAN_POLICY_NONE);
        /* Set the connect timeout as infinite for WDS STA*/
        wlan_connection_sm_set_param(osifp->sm_handle,
                WLAN_CONNECTION_PARAM_CONNECT_TIMEOUT,10);
        /* Set the connect timeout as infinite for WDS STA*/
        wlan_connection_sm_set_param(osifp->sm_handle,
                WLAN_CONNECTION_PARAM_RECONNECT_TIMEOUT,10);
        /*
         * Bad AP Timeout value should be specified in ms
         * so converting seconds to ms
         * Set BAD AP timeout to 0 for linux repeater config
         * as AUTH_AUTO mode in WEP needs connection SM to alternate AUTH
         * mode from open to shared continously for reconnection
         */
        wlan_connection_sm_set_param(osifp->sm_handle,
                WLAN_CONNECTION_PARAM_BAD_AP_TIMEOUT, 0);
        wlan_aplist_set_bad_ap_timeout(vap, 0);
    }

    /*If in TXCHANSWITCH then do not send a stop */
    if(wlan_connection_sm_get_param(osifp->sm_handle,
                WLAN_CONNECTION_PARAM_CURRENT_STATE) ==
            WLAN_ASSOC_STATE_TXCHANSWITCH) {
        schedule_timeout_interruptible(OSIF_DISCONNECT_TIMEOUT);
        waitcnt = 0;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                "%s: Tx ChanSwitch is happening\n", __func__);
        while(waitcnt < OSIF_TXCHANSWITCH_LOOPCOUNT) {
            schedule_timeout_interruptible(OSIF_DISCONNECT_TIMEOUT);
            waitcnt++;
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                "%s: Tx ChanSwitch is Completed\n", __func__);
    }

stop_conn_sm:
    flags = IEEE80211_CONNECTION_SM_STOP_ASYNC;
    if (osifp->no_stop_disassoc)
        flags |= IEEE80211_CONNECTION_SM_STOP_NO_DISASSOC;

    /*
     * If connection sm is already stopped and in init state, vdev SM is in the
     * vdev bring down path, wait for this stop command to be removed on down
     * completion instead of returning error from here, which will remove the
     * stop cmd and process start cmd, leading to sending of start req even
     * before stop resp is received.
     * Once peer delete resp is recevied, vdev SM will move to INIT state and
     * the cmd in serialization queue will be removed.
     */
    if (wlan_connection_sm_stop(osifp->sm_handle, flags) == EOK &&
           wlan_vdev_is_going_down(vap->vdev_obj) != QDF_STATUS_SUCCESS) {
        wlan_mlme_err("connection stop failed");
        return QDF_STATUS_E_FAILURE;
    }
    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlme_vdev_req_flush_cb(struct scheduler_msg *msg)
{
    struct wlan_serialization_command *cmd = msg->bodyptr;

    if (!cmd || !cmd->vdev) {
        wlan_mlme_err("Null input cmd:%pK", cmd);
        return QDF_STATUS_E_INVAL;
    }

    wlan_objmgr_vdev_release_ref(cmd->vdev, WLAN_SCHEDULER_ID);
    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS
mlme_vdev_start_activation(struct wlan_serialization_command *cmd)
{
    enum QDF_OPMODE opmode;
    struct wlan_objmgr_vdev *vdev;
    struct wlan_mlme_ser_data *data;

    if (!cmd || !cmd->vdev || !cmd->umac_cmd) {
        wlan_mlme_err("Null input cmd:%pK", cmd);
        return QDF_STATUS_SUCCESS;
    }

    vdev = cmd->vdev;
    data = cmd->umac_cmd;
    opmode = wlan_vdev_mlme_get_opmode(vdev);

    data->cmd_in_sched = 0;
    if (opmode == QDF_SAP_MODE || opmode == QDF_MONITOR_MODE)
        return wlan_mlme_start_ap_vdev(cmd);
    else
        return wlan_mlme_start_sta_vdev(cmd);

}

static QDF_STATUS
mlme_vdev_stop_activation(struct wlan_serialization_command *cmd)
{
    enum QDF_OPMODE opmode;
    struct wlan_objmgr_vdev *vdev;
    struct wlan_mlme_ser_data *data;

    if (!cmd || !cmd->vdev || !cmd->umac_cmd) {
        wlan_mlme_err("Null input cmd:%pK", cmd);
        return QDF_STATUS_SUCCESS;
    }

    vdev = cmd->vdev;
    data = cmd->umac_cmd;
    opmode = wlan_vdev_mlme_get_opmode(vdev);

    data->cmd_in_sched = 0;
    if (opmode == QDF_SAP_MODE || opmode == QDF_MONITOR_MODE)
        return wlan_mlme_stop_ap_vdev(cmd);
    else
        return wlan_mlme_stop_sta_vdev(cmd);
}

QDF_STATUS mlme_vdev_start_sched_cb(struct scheduler_msg *msg)
{
    struct wlan_serialization_command *cmd = msg->bodyptr;
    struct wlan_objmgr_vdev *vdev;
    QDF_STATUS ret;

    if (!cmd) {
        wlan_mlme_err("cmd is null");
        return QDF_STATUS_E_INVAL;
    }

    vdev = cmd->vdev;
    if (!vdev) {
        wlan_mlme_err("vdev is null");
        return QDF_STATUS_E_INVAL;
    }

    wlan_mlme_nofl_debug("START_SCHED V:%u", wlan_vdev_get_id(vdev));
    ret = mlme_vdev_start_activation(cmd);
    /*
     * Called from scheduler context hence release the command on error
     */
    if (QDF_IS_STATUS_ERROR(ret))
        wlan_mlme_release_vdev_req(vdev,
                WLAN_SER_CMD_VDEV_START_BSS, EINVAL);

    wlan_objmgr_vdev_release_ref(vdev, WLAN_SCHEDULER_ID);
    return ret;
}

QDF_STATUS mlme_vdev_stop_sched_cb(struct scheduler_msg *msg)
{
    struct wlan_serialization_command *cmd = msg->bodyptr;
    struct wlan_objmgr_vdev *vdev;
    QDF_STATUS ret;

    if (!cmd) {
        wlan_mlme_err("cmd is null");
        return QDF_STATUS_E_INVAL;
    }

    vdev = cmd->vdev;
    if (!vdev) {
        wlan_mlme_err("vdev is null");
        return QDF_STATUS_E_INVAL;
    }

    wlan_mlme_nofl_debug("STOP_SCHED V:%u", wlan_vdev_get_id(vdev));
    ret = mlme_vdev_stop_activation(cmd);
    /*
     * Called from scheduler context hence release the command on error
     */
    if (QDF_IS_STATUS_ERROR(ret))
        wlan_mlme_release_vdev_req(vdev,
                WLAN_SER_CMD_VDEV_STOP_BSS, EINVAL);

    wlan_objmgr_vdev_release_ref(vdev, WLAN_SCHEDULER_ID);
    return ret;
}

QDF_STATUS mlme_ser_process_start_vdev(struct wlan_serialization_command *cmd,
        enum wlan_serialization_cb_reason reason)
{
    struct wlan_objmgr_vdev *vdev = cmd->vdev;
    struct wlan_mlme_ser_data *data = cmd->umac_cmd;
    struct scheduler_msg msg = {0};
    QDF_STATUS ret;

    wlan_mlme_nofl_debug("START V:%u rs:%d act:%d",
                         wlan_vdev_get_id(vdev),
                         reason, cmd->activation_reason);

    switch (reason) {
    case WLAN_SER_CB_ACTIVATE_CMD:
        if (cmd->activation_reason == SER_PENDING_TO_ACTIVE) {
            msg.bodyptr = cmd;
            msg.callback = mlme_vdev_start_sched_cb;
            msg.flush_callback = mlme_vdev_req_flush_cb;

            ret = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_SCHEDULER_ID);
            if (QDF_IS_STATUS_ERROR(ret))
                return ret;

            data->activation_ctx = MLME_CTX_SCHED,
            data->cmd_in_sched = 1;
            ret = scheduler_post_message(QDF_MODULE_ID_MLME,
                    QDF_MODULE_ID_MLME, QDF_MODULE_ID_MLME, &msg);
            if (QDF_IS_STATUS_ERROR(ret)) {
                wlan_mlme_err("failed to post scheduler_msg");
                wlan_objmgr_vdev_release_ref(vdev, WLAN_SCHEDULER_ID);
                return ret;
            }
            wlan_mlme_debug("Cmd act in sched vdev:%d notif:%d",
                            wlan_vdev_get_id(vdev), data->notify_osif);
        } else {
            data->activation_ctx = MLME_CTX_DIRECT;
            return mlme_vdev_start_activation(cmd);
        }
        break;
    case WLAN_SER_CB_CANCEL_CMD:
        break;
    case WLAN_SER_CB_RELEASE_MEM_CMD:
        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SER_IF_ID);
        qdf_mem_free(cmd->umac_cmd);
        cmd->umac_cmd = NULL;
        break;
    case WLAN_SER_CB_ACTIVE_CMD_TIMEOUT:
        wlan_mlme_err("Timeout vdev:%d", wlan_vdev_get_id(vdev));
        break;
    default:
        QDF_ASSERT(0);
        return QDF_STATUS_E_INVAL;
        break;
    }

    return QDF_STATUS_SUCCESS;
}

QDF_STATUS mlme_ser_process_stop_vdev(struct wlan_serialization_command *cmd,
        enum wlan_serialization_cb_reason reason)
{
    struct wlan_objmgr_vdev *vdev = cmd->vdev;
    struct wlan_mlme_ser_data *data = cmd->umac_cmd;
    struct scheduler_msg msg = {0};
    QDF_STATUS ret;

    wlan_mlme_nofl_debug("STOP V:%u rs:%d act:%d",
                         wlan_vdev_get_id(vdev),
                         reason, cmd->activation_reason);

    switch (reason) {
    case WLAN_SER_CB_ACTIVATE_CMD:
        if (cmd->activation_reason == SER_PENDING_TO_ACTIVE) {
            msg.bodyptr = cmd;
            msg.callback = mlme_vdev_stop_sched_cb;
            msg.flush_callback = mlme_vdev_req_flush_cb;

            ret = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_SCHEDULER_ID);
            if (QDF_IS_STATUS_ERROR(ret))
                return ret;

            data->cmd_in_sched = 1;
            data->activation_ctx = MLME_CTX_SCHED,
            ret = scheduler_post_message(QDF_MODULE_ID_MLME,
                    QDF_MODULE_ID_MLME, QDF_MODULE_ID_MLME, &msg);
            if (QDF_IS_STATUS_ERROR(ret)) {
                wlan_mlme_err("failed to post scheduler_msg");
                wlan_objmgr_vdev_release_ref(vdev, WLAN_SCHEDULER_ID);
                return ret;
            }
            wlan_mlme_debug("Cmd act in sched vdev:%d notif:%d",
                            wlan_vdev_get_id(vdev), data->notify_osif);
        } else {
            data->activation_ctx = MLME_CTX_DIRECT;
            return mlme_vdev_stop_activation(cmd);
        }
        break;
    case WLAN_SER_CB_CANCEL_CMD:
        break;
    case WLAN_SER_CB_RELEASE_MEM_CMD:
        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SER_IF_ID);
        qdf_mem_free(cmd->umac_cmd);
        cmd->umac_cmd = NULL;
        break;
    case WLAN_SER_CB_ACTIVE_CMD_TIMEOUT:
        wlan_mlme_err("Timeout vdev:%d", wlan_vdev_get_id(vdev));
        break;
    default:
        QDF_ASSERT(0);
        return QDF_STATUS_E_INVAL;
        break;
    }

    return QDF_STATUS_SUCCESS;
}

void wlan_mlme_vdev_cmd_ap_handler(struct wlan_objmgr_vdev *vdev,
        enum wlan_serialization_cmd_type cmd_type)
{
    wlan_chan_t chan;
    wlan_if_t vap = wlan_vdev_get_mlme_ext_obj(vdev);
    uint32_t mgmt_rate;
    struct wlan_vdev_mgr_cfg mlme_cfg;

    if (!vap) {
        wlan_mlme_err("vap is NULL");
        return;
    }

    switch (cmd_type) {
    case WLAN_SER_CMD_VDEV_START_BSS:
        wlan_mlme_debug("START complete notification");
        wlan_util_vdev_mlme_get_param(vap->vdev_mlme,
                WLAN_MLME_CFG_TX_MGMT_RATE, &mgmt_rate);

        if (mgmt_rate == 0) {
            chan = wlan_get_des_channel(vap);
            if ((chan == NULL) || (chan == IEEE80211_CHAN_ANYC)) {
                wlan_mlme_err("vap %d ic_curchan %pK  ic_ieee:%u  freq:%u ",
                        vap->iv_unit, vap->iv_ic->ic_curchan,
                        vap->iv_ic->ic_curchan->ic_ieee,
                        vap->iv_ic->ic_curchan->ic_freq);
                chan = vap->iv_ic->ic_curchan;
            }
            /*Set default mgmt rate*/
            if (IEEE80211_IS_CHAN_5GHZ(chan)) {
                mlme_cfg.value = DEFAULT_LOWEST_RATE_5G;
            } else if (IEEE80211_IS_CHAN_2GHZ(chan)) {
                mlme_cfg.value = DEFAULT_LOWEST_RATE_2G;
            }
            wlan_util_vdev_mlme_set_param(vap->vdev_mlme,
                    WLAN_MLME_CFG_TX_MGMT_RATE, mlme_cfg);
        }
        wlan_restore_vap_params(vap);
        vap->iv_needs_up_on_acs = 1;
        break;
    default:
        break;
    }
}

void wlan_mlme_vdev_cmd_sta_handler(struct wlan_objmgr_vdev *vdev,
        enum wlan_serialization_cmd_type cmd_type)
{
    /*No-op for now*/
}

void wlan_mlme_vdev_cmd_handler(struct wlan_objmgr_vdev *vdev,
        enum wlan_serialization_cmd_type cmd_type)
{
    enum QDF_OPMODE opmode = wlan_vdev_mlme_get_opmode(vdev);

    if (opmode == QDF_SAP_MODE)
        wlan_mlme_vdev_cmd_ap_handler(vdev, cmd_type);
    else
        wlan_mlme_vdev_cmd_sta_handler(vdev, cmd_type);
}

QDF_STATUS wlan_mlme_release_vdev_req(struct wlan_objmgr_vdev *vdev,
                                   enum wlan_serialization_cmd_type cmd_type,
                                   int32_t notify_status)
{
    wlan_if_t vap;
    uint8_t vdev_id;
    struct wlan_objmgr_psoc *psoc;
    enum wlan_serialization_cmd_type active_cmd_type;
    struct wlan_mlme_ser_data *data;

    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    vdev_id = wlan_vdev_get_id(vdev);
    psoc = wlan_vdev_get_psoc(vdev);
    if (!vap || !psoc) {
        wlan_mlme_err("%s is null", vap ? "psoc" : "vap");
        return QDF_STATUS_E_INVAL;
    }

    active_cmd_type = wlan_serialization_get_vdev_active_cmd_type(vdev);
    /* If requested release command is STOP, remove command though active
     * command type is not STOP, otherwise release only if active command and
     * requested command types match
     */
    if ((cmd_type != active_cmd_type) &&
        (active_cmd_type == WLAN_SER_CMD_VDEV_STOP_BSS)) {
        wlan_mlme_nofl_err("cmd mismatch, v:%u ct:%d rt:%d", vdev_id,
                           cmd_type, active_cmd_type);
        return QDF_STATUS_E_FAILURE;
    }

    /*
     * If the cmd is activated in scheduler context, then wait until cmd
     * completes its execution i.e do not free the cmd now.
     */
    data = wlan_serialization_get_active_cmd(psoc, vdev_id, active_cmd_type);
    if (data && data->cmd_in_sched) {
        wlan_mlme_nofl_err("IF_REL V:%u exec in sched type:%d",
                           vdev_id, active_cmd_type);
        return QDF_STATUS_E_PENDING;
    }

    /*
     * If the requested cmd type and the active cmd type is same,
     * only then call mlme and osif callback.
     */
    if (cmd_type == active_cmd_type && notify_status == EOK) {
        data = wlan_serialization_get_active_cmd(psoc, vdev_id, cmd_type);
        if (data && (data->notify_osif == WLAN_MLME_NOTIFY_MLME ||
                data->notify_osif == WLAN_MLME_NOTIFY_OSIF)) {
            /* MLME Post processing handler */
            wlan_mlme_vdev_cmd_handler(vdev, cmd_type);

            /* OSIF Post processing handler */
            if (data->notify_osif == WLAN_MLME_NOTIFY_OSIF)
                osif_mlme_notify_handler(vap, cmd_type);
        }
    }
    wlan_mlme_nofl_debug("IF_REL V:%u notif:%d ct:%d rt:%d", vdev_id,
                         notify_status, cmd_type, active_cmd_type);
    if (active_cmd_type < WLAN_SER_CMD_MAX)
        wlan_vdev_mlme_ser_remove_request(vdev, vdev_id, active_cmd_type);

    return QDF_STATUS_SUCCESS;
}

static void mlme_cmd_abort_flush_cb(struct scheduler_msg *msg)
{
    struct wlan_objmgr_vdev *vdev = msg->bodyptr;
    wlan_objmgr_vdev_release_ref(vdev, WLAN_SCHEDULER_ID);
}

static QDF_STATUS mlme_cmd_abort_start_sched_cb(struct scheduler_msg *msg)
{
    struct wlan_objmgr_vdev *vdev = msg->bodyptr;

    wlan_vdev_mlme_ser_remove_request(vdev, wlan_vdev_get_id(vdev),
                                      WLAN_SER_CMD_VDEV_START_BSS);
    wlan_objmgr_vdev_release_ref(vdev, WLAN_SCHEDULER_ID);
    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlme_cmd_abort_restart_sched_cb(struct scheduler_msg *msg)
{
    struct wlan_objmgr_vdev *vdev = msg->bodyptr;

    wlan_vdev_mlme_ser_remove_request(vdev, wlan_vdev_get_id(vdev),
                                      WLAN_SER_CMD_VDEV_RESTART);
    wlan_objmgr_vdev_release_ref(vdev, WLAN_SCHEDULER_ID);
    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlme_cmd_abort(struct wlan_objmgr_vdev *vdev)
{
    enum wlan_serialization_cmd_type active_cmd;
    struct scheduler_msg msg = {0};
    QDF_STATUS ret;

    active_cmd = wlan_serialization_get_vdev_active_cmd_type(vdev);
    if (active_cmd == WLAN_SER_CMD_VDEV_STOP_BSS) {
        wlan_mlme_err("Invalid active cmd type %u", active_cmd);
        return QDF_STATUS_E_INVAL;
    }

    ret = wlan_ser_validate_umac_cmd(vdev,
                                     active_cmd,
                                     mlme_is_active_cmd_in_sched_ctx);

    if (ret == QDF_STATUS_SUCCESS) {
        if (active_cmd == WLAN_SER_CMD_VDEV_START_BSS) {
            msg.callback = mlme_cmd_abort_start_sched_cb;
        } else if (active_cmd == WLAN_SER_CMD_VDEV_RESTART) {
            msg.callback = mlme_cmd_abort_restart_sched_cb;
        } else {
            /*
             * When a new command is added to be part of abort sequence then add
             * a corresponding scheduler callback here
             */
            wlan_mlme_err("Unknown active_cmd_type %u", active_cmd);
            return QDF_STATUS_E_FAILURE;
        }

        ret = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_SCHEDULER_ID);
        if (QDF_IS_STATUS_ERROR(ret))
            return ret;

        msg.bodyptr = vdev;
        msg.flush_callback = mlme_cmd_abort_flush_cb;
        ret = scheduler_post_message(QDF_MODULE_ID_MLME, QDF_MODULE_ID_MLME,
                                     QDF_MODULE_ID_MLME, &msg);
        if (QDF_IS_STATUS_ERROR(ret)) {
            wlan_objmgr_vdev_release_ref(vdev, WLAN_SCHEDULER_ID);
            wlan_mlme_err("failed to post scheduler_msg active cmd:%u",
                          active_cmd);
            return ret;
        }
    } else if (ret == QDF_STATUS_E_FAILURE) {
        wlan_vdev_mlme_ser_remove_request(vdev, wlan_vdev_get_id(vdev),
                                          active_cmd);
    } else {
        wlan_mlme_err("Active cmd not found type:%u", active_cmd);
        return ret;
    }

    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlme_start_vdev(struct wlan_objmgr_vdev *vdev,
        uint32_t f_scan, uint8_t notify_osif)
{
    struct wlan_serialization_command cmd = {0};
    struct wlan_mlme_ser_data *data;
    QDF_STATUS status;
    enum wlan_serialization_status ret;
    uint8_t vdev_id = wlan_vdev_get_id(vdev);

    wlan_mlme_debug("vdev:%d notify:%d", vdev_id, notify_osif);
    data = qdf_mem_malloc(sizeof(*data));
    if (!data)
        return QDF_STATUS_E_NOMEM;

    status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_MLME_SER_IF_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_mem_free(data);
        return QDF_STATUS_E_BUSY;
    }

    data->vdev = vdev;
    data->flags = f_scan;
    data->notify_osif = notify_osif;

    cmd.source = WLAN_UMAC_COMP_MLME;
    cmd.vdev = vdev;
    cmd.cmd_type = WLAN_SER_CMD_VDEV_START_BSS;
    cmd.cmd_cb = mlme_ser_process_start_vdev;
    cmd.cmd_id = vdev_id;
    cmd.umac_cmd = data;
    cmd.cmd_timeout_duration = WLAN_MLME_SER_CMD_TIMEOUT_MS;

    ret = wlan_vdev_mlme_ser_start_bss(&cmd);
    if (ret != WLAN_SER_CMD_ACTIVE && ret != WLAN_SER_CMD_PENDING) {
        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SER_IF_ID);
        qdf_mem_free(data);
        wlan_mlme_nofl_err("IF_START V:%u err:%d", vdev_id, ret);
        return QDF_STATUS_E_FAILURE;
    }

    wlan_mlme_nofl_debug("IF_START V:%u notif:%d ret:%d %s",
                         vdev_id, notify_osif, ret, ret ? "AQ" : "PQ");
    return QDF_STATUS_SUCCESS;
}

static QDF_STATUS mlme_stop_vdev(struct wlan_objmgr_vdev *vdev,
        uint32_t flags, uint8_t notify_osif)
{
    struct wlan_mlme_ser_data *data;
    QDF_STATUS status;
    enum wlan_serialization_status ret;
    struct wlan_serialization_command cmd = {0};
    wlan_if_t vap;
    osif_dev *osifp;
    uint8_t vdev_id = wlan_vdev_get_id(vdev);

    wlan_mlme_debug("vdev:%d notify:%d", vdev_id, notify_osif);
    vap = wlan_vdev_get_mlme_ext_obj(vdev);
    if (!vap) {
        wlan_mlme_err("vap is NULL");
        return QDF_STATUS_E_FAILURE;
    }
    osifp = (osif_dev *)vap->iv_ifp;

    data = qdf_mem_malloc(sizeof(*data));
    if (!data)
        return QDF_STATUS_E_NOMEM;

    status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_MLME_SER_IF_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_mem_free(data);
        return QDF_STATUS_E_BUSY;
    }

    data->vdev = vdev;
    data->flags = flags;
    data->notify_osif = notify_osif;

    cmd.source = WLAN_UMAC_COMP_MLME;
    cmd.vdev = vdev;
    cmd.cmd_type = WLAN_SER_CMD_VDEV_STOP_BSS;
    cmd.cmd_cb = mlme_ser_process_stop_vdev;
    cmd.cmd_id = vdev_id;
    cmd.umac_cmd = data;
    cmd.cmd_timeout_duration = WLAN_MLME_SER_CMD_TIMEOUT_MS;

    /* Mark to disable queue if vdev delete has triggered this stop cmd */
    if (osifp && osifp->is_delete_in_progress) {
        cmd.queue_disable = 1;
        wlan_mlme_nofl_err("IF_STOP: V:%u Dis SER queue", vdev_id);
    }

    ret = wlan_vdev_mlme_ser_stop_bss(&cmd);
    if (ret != WLAN_SER_CMD_ACTIVE && ret != WLAN_SER_CMD_PENDING &&
        ret != WLAN_SER_CMD_ALREADY_EXISTS) {
        wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SER_IF_ID);
        qdf_mem_free(data);
        wlan_mlme_nofl_debug("IF_STOP V:%u err_ret:%d", vdev_id, ret);
        return QDF_STATUS_E_FAILURE;
    }

    wlan_mlme_nofl_debug("IF_STOP V:%u notif:%d ret:%d %s",
                         vdev_id, notify_osif, ret, ret ? "AQ" : "PQ");
    if (ret == WLAN_SER_CMD_PENDING) {
        /*
         * To avoid the stop cmd waiting in pending queue, we are
         * triggering active cmd abort, where remove request for the cmd in the
         * active queue is sent.
         */
        wlan_mlme_nofl_debug("IF_STOP V:%u act cmd abort", vdev_id);
        mlme_cmd_abort(vdev);
    }
    return QDF_STATUS_SUCCESS;
}

QDF_STATUS wlan_mlme_start_vdev(struct wlan_objmgr_vdev *vdev,
        uint32_t f_scan, uint8_t notify_osif)
{
	QDF_STATUS status;
	QDF_STATUS ret_val;

	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_MLME_SER_IF_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_mlme_info("unable to get reference");
		return QDF_STATUS_E_BUSY;
	}

	wlan_vdev_mlme_cmd_lock(vdev);
	ret_val = mlme_start_vdev(vdev, f_scan, notify_osif);
	wlan_vdev_mlme_cmd_unlock(vdev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SER_IF_ID);

	return ret_val;
}

QDF_STATUS wlan_mlme_stop_vdev(struct wlan_objmgr_vdev *vdev,
        uint32_t flags, uint8_t notify_osif)
{
	QDF_STATUS status;
	QDF_STATUS ret_val;

	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_MLME_SER_IF_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_mlme_info("unable to get reference");
		return QDF_STATUS_E_BUSY;
	}

	wlan_vdev_mlme_cmd_lock(vdev);
	ret_val = mlme_stop_vdev(vdev, flags, notify_osif);
	wlan_vdev_mlme_cmd_unlock(vdev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SER_IF_ID);

	return ret_val;

}

QDF_STATUS wlan_mlme_restart_vdev(struct wlan_objmgr_vdev *vdev,
        uint32_t flags, uint8_t notify_osif)
{
	QDF_STATUS status;
	QDF_STATUS ret_val;

	status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_MLME_SER_IF_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		wlan_mlme_info("unable to get reference");
		return QDF_STATUS_E_BUSY;
	}

	wlan_vdev_mlme_cmd_lock(vdev);
	ret_val = mlme_stop_vdev(vdev, flags, notify_osif);
	ret_val = mlme_start_vdev(vdev, flags, notify_osif);

	wlan_vdev_mlme_cmd_unlock(vdev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_MLME_SER_IF_ID);

	return ret_val;

}

void wlan_mlme_inc_act_cmd_timeout(struct wlan_objmgr_vdev *vdev,
        enum wlan_serialization_cmd_type cmd_type)
{
    struct wlan_serialization_command cmd = {0};
    uint32_t dfs_cac_timeout;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_channel *chan;

    pdev = wlan_vdev_get_pdev(vdev);
    if (!pdev) {
        wlan_mlme_err("pdev is NULL");
        return;
    }
    chan = wlan_vdev_mlme_get_des_chan(vdev);
    dfs_cac_timeout = dfs_mlme_get_cac_timeout(pdev,
                                    chan->ch_freq,
                                    chan->ch_freq_seg2,
                                    chan->ch_flags);

    /* Seconds to milliseconds with added delta of ten seconds*/
    dfs_cac_timeout = (dfs_cac_timeout + 10) * 1000;

    wlan_mlme_err("DFS timeout:%u", dfs_cac_timeout);
    cmd.vdev = vdev;
    cmd.cmd_id = wlan_vdev_get_id(vdev);
    cmd.cmd_timeout_duration = dfs_cac_timeout;
    cmd.cmd_type = cmd_type;
    cmd.source = WLAN_UMAC_COMP_MLME;
    mlme_ser_inc_act_cmd_timeout(&cmd);
}

void wlan_mlme_wait_for_cmd_completion(struct wlan_objmgr_vdev *vdev)
{
    qdf_event_t vdev_wait_for_active_cmds;
    uint32_t max_wait_iterations =
                   WLAN_SERIALZATION_CANCEL_WAIT_ITERATIONS;
    uint8_t vdev_id = wlan_vdev_get_id(vdev);

    memset(&vdev_wait_for_active_cmds, 0, sizeof(vdev_wait_for_active_cmds));

    qdf_event_create(&vdev_wait_for_active_cmds);
    qdf_event_reset(&vdev_wait_for_active_cmds);

    while(wlan_serialization_get_vdev_active_cmd_type(vdev)
                    != WLAN_SER_CMD_MAX && max_wait_iterations) {
        qdf_wait_single_event(&vdev_wait_for_active_cmds,
                              WLAN_SERIALZATION_CANCEL_WAIT_TIME);
        max_wait_iterations--;
    }

    if (!max_wait_iterations)
            wlan_mlme_err("Waiting for active cmds removal timed out. vdev id: %d\n",
                     vdev_id);

    qdf_event_destroy(&vdev_wait_for_active_cmds);
}

