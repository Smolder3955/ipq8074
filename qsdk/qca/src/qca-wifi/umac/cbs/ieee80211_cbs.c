/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

/* Contineous Background Scan (CBS) */

#include <osdep.h>

#include <ieee80211_var.h>
#include <wlan_scan.h>
#include <ieee80211_channel.h>
#include <ieee80211_acs.h>
#include <ieee80211_acs_internal.h>
#include <ieee80211_cbs.h>
#include <ieee80211_objmgr_priv.h>
#include <ieee80211_ucfg.h>

#define CBS_DWELL_TIME_25MS 25
#define CBS_DWELL_TIME_50MS 50
#define CBS_DWELL_TIME_75MS 75
#define MIN_SCAN_OFFSET_ARRAY_SIZE 0
#define MAX_SCAN_OFFSET_ARRAY_SIZE 7

int ieee80211_cbs_event(ieee80211_cbs_t cbs, enum ieee80211_cbs_event event);

#if UMAC_SUPPORT_CBS

void ieee80211_cbs_post_event (ieee80211_cbs_t cbs, enum ieee80211_cbs_event event)
{
    if (cbs->cbs_event != 0) {
        qdf_print("%s: pending event %d, new event %d can't be processed",
               __FUNCTION__, cbs->cbs_event, event);
        return;
    }

    cbs->cbs_event = event;
    qdf_sched_work(NULL, &cbs->cbs_work);
}

void cbs_work(void *arg)
{
    ieee80211_cbs_t cbs = (ieee80211_cbs_t)arg ;
    enum ieee80211_cbs_event event = cbs->cbs_event;

    cbs->cbs_event = 0;
    ieee80211_cbs_event(cbs, event);
}

static OS_TIMER_FUNC(cbs_timer)
{
    ieee80211_cbs_t cbs ;

    OS_GET_TIMER_ARG(cbs, ieee80211_cbs_t );

    switch (cbs->cbs_state) {
    case IEEE80211_CBS_REST:
        if (cbs->dwell_split_cnt < 0){
            ieee80211_cbs_post_event(cbs, IEEE80211_CBS_SCAN_CONTINUE);
        }
        else {
            ieee80211_cbs_post_event(cbs, IEEE80211_CBS_DWELL_SPLIT);
        }
        break;
    case IEEE80211_CBS_WAIT:
        ieee80211_cbs_post_event(cbs, IEEE80211_CBS_SCAN_START);
        break;
    default:
        break;
    }
}

/*
 * scan handler used for scan events
 */
static void ieee80211_cbs_scan_evhandler(struct wlan_objmgr_vdev *vdev, struct scan_event *event, void *arg)
{
    struct ieee80211vap *originator = wlan_vdev_get_mlme_ext_obj(vdev);
    struct ieee80211com *ic;
    ieee80211_cbs_t cbs;

    if (originator == NULL) {
        eacs_trace(EACS_DBG_LVL0, ("vdev pointer is invalid\n"));
        return;
    }

    ic = originator->iv_ic;
    cbs = (ieee80211_cbs_t) arg;

    IEEE80211_DPRINTF(originator, IEEE80211_MSG_CBS,
                      "scan_id %08X event %d reason %d \n",
                      event->scan_id, event->type, event->reason);

#if ATH_SUPPORT_MULTIPLE_SCANS
    /*
     * Ignore notifications received due to scans requested by other modules
     * and handle new event IEEE80211_SCAN_DEQUEUED.
     */
    ASSERT(0);

    /* Ignore events reported by scans requested by other modules */
    if (cbs->cbs_scan_id != event->scan_id) {
        return;
    }
#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */

    switch (event->type) {
    case SCAN_EVENT_TYPE_FOREIGN_CHANNEL:
    case SCAN_EVENT_TYPE_FOREIGN_CHANNEL_GET_NF: /* notice fallthrough */
        ieee80211_acs_api_update(ic, event->type, event->chan_freq);
        break;
    case SCAN_EVENT_TYPE_COMPLETED:
        if (event->reason != SCAN_REASON_COMPLETED) {
            break;
        }
        if (cbs->dwell_split_cnt < 0) {
            ieee80211_cbs_post_event(cbs, IEEE80211_CBS_SCAN_NEXT);
        }
        else {
            ieee80211_cbs_post_event(cbs, IEEE80211_CBS_DWELL_SPLIT);
        }
        break;
    default:
        break;
    }
}

/*
 * Function to pre-fill the dwell-split & dwell-rest time for different
 * iterations on the same channel. This is used within the scan state machine
 * to scan the complete dwell time by resting between scans
 */
#define TOTAL_DWELL_TIME 200
#define DEFAULT_BEACON_INTERVAL 100
static void ieee80211_cbs_init_dwell_params(ieee80211_cbs_t cbs,
                                  int dwell_split_time, int dwell_rest_time)
{
    int i;

    switch (dwell_split_time) {
    case CBS_DWELL_TIME_25MS:
        cbs->max_arr_size_used = 8;
        cbs->dwell_split_cnt = cbs->max_arr_size_used - 1;
        cbs->max_dwell_split_cnt = cbs->max_arr_size_used - 1;
        if (dwell_rest_time % TOTAL_DWELL_TIME == 0) {
            cbs->scan_dwell_rest[0] = dwell_rest_time;
            cbs->scan_dwell_rest[1] = dwell_rest_time;
            cbs->scan_dwell_rest[2] = dwell_rest_time;
            cbs->scan_dwell_rest[3] = dwell_rest_time;
            cbs->scan_dwell_rest[4] = dwell_rest_time + TOTAL_DWELL_TIME - DEFAULT_BEACON_INTERVAL;
            cbs->scan_dwell_rest[5] = dwell_rest_time + TOTAL_DWELL_TIME - DEFAULT_BEACON_INTERVAL;
            cbs->scan_dwell_rest[6] = dwell_rest_time;
            cbs->scan_dwell_rest[7] = dwell_rest_time;
            cbs->scan_offset[0] = 0;
            cbs->scan_offset[1] = 0;
            cbs->scan_offset[2] = dwell_split_time;
            cbs->scan_offset[3] = dwell_split_time;
            cbs->scan_offset[4] = 2*dwell_split_time;
            cbs->scan_offset[5] = 2*dwell_split_time;
            cbs->scan_offset[6] = 3*dwell_split_time;
            cbs->scan_offset[7] = 3*dwell_split_time;
        }
        else {
            for(i = 0; i < cbs->max_arr_size_used - 1; i++){
                cbs->scan_dwell_rest[i] = dwell_rest_time;
            }
            cbs->scan_offset[0] = 0;
            cbs->scan_offset[1] = dwell_split_time;
            cbs->scan_offset[2] = 2*dwell_split_time;
            cbs->scan_offset[3] = 3*dwell_split_time;
            cbs->scan_offset[4] = 0;
            cbs->scan_offset[5] = dwell_split_time;
            cbs->scan_offset[6] = 2*dwell_split_time;
            cbs->scan_offset[7] = 3*dwell_split_time;
        }
        break;
    case CBS_DWELL_TIME_50MS:
        cbs->max_arr_size_used = 4;
        cbs->dwell_split_cnt = cbs->max_arr_size_used - 1;
        cbs->max_dwell_split_cnt = cbs->max_arr_size_used - 1;
        if (dwell_rest_time % TOTAL_DWELL_TIME == 0) {
            cbs->scan_dwell_rest[0] = dwell_rest_time;
            cbs->scan_dwell_rest[1] = dwell_rest_time;
            cbs->scan_dwell_rest[2] = dwell_rest_time + TOTAL_DWELL_TIME - DEFAULT_BEACON_INTERVAL;
            cbs->scan_dwell_rest[3] = dwell_rest_time + TOTAL_DWELL_TIME - DEFAULT_BEACON_INTERVAL;
            cbs->scan_dwell_rest[4] = 0;
            cbs->scan_dwell_rest[5] = 0;
            cbs->scan_dwell_rest[6] = 0;
            cbs->scan_dwell_rest[7] = 0;
            cbs->scan_offset[0] = 0;
            cbs->scan_offset[1] = 0;
            cbs->scan_offset[2] = dwell_split_time;
            cbs->scan_offset[3] = dwell_split_time;
            cbs->scan_offset[4] = 0;
            cbs->scan_offset[5] = 0;
            cbs->scan_offset[6] = 0;
            cbs->scan_offset[7] = 0;
        }
        else {
            cbs->scan_dwell_rest[0] = dwell_rest_time;
            cbs->scan_dwell_rest[1] = dwell_rest_time;
            cbs->scan_dwell_rest[2] = dwell_rest_time;
            cbs->scan_dwell_rest[3] = dwell_rest_time;
            cbs->scan_dwell_rest[4] = 0;
            cbs->scan_dwell_rest[5] = 0;
            cbs->scan_dwell_rest[6] = 0;
            cbs->scan_dwell_rest[7] = 0;
            cbs->scan_offset[0] = 0;
            cbs->scan_offset[1] = dwell_split_time;
            cbs->scan_offset[2] = 0;
            cbs->scan_offset[3] = dwell_split_time;
            cbs->scan_offset[4] = 0;
            cbs->scan_offset[5] = 0;
            cbs->scan_offset[6] = 0;
            cbs->scan_offset[7] = 0;
        }
        break;
    case CBS_DWELL_TIME_75MS:
        cbs->max_arr_size_used = 4;
        cbs->dwell_split_cnt = cbs->max_arr_size_used - 1;
        cbs->max_dwell_split_cnt = cbs->max_arr_size_used - 1;
        if (dwell_rest_time % TOTAL_DWELL_TIME == 0) {
            cbs->scan_dwell_rest[0] = dwell_rest_time;
            cbs->scan_dwell_rest[1] = dwell_rest_time;
            cbs->scan_dwell_rest[2] = dwell_rest_time + TOTAL_DWELL_TIME - DEFAULT_BEACON_INTERVAL;
            cbs->scan_dwell_rest[3] = dwell_rest_time + TOTAL_DWELL_TIME - DEFAULT_BEACON_INTERVAL;
            cbs->scan_dwell_rest[4] = 0;
            cbs->scan_dwell_rest[5] = 0;
            cbs->scan_dwell_rest[6] = 0;
            cbs->scan_dwell_rest[7] = 0;
            cbs->scan_offset[0] = 0;
            cbs->scan_offset[1] = 0;
            cbs->scan_offset[2] = DEFAULT_BEACON_INTERVAL - dwell_split_time;
            cbs->scan_offset[3] = DEFAULT_BEACON_INTERVAL - dwell_split_time;
            cbs->scan_offset[4] = 0;
            cbs->scan_offset[5] = 0;
            cbs->scan_offset[6] = 0;
            cbs->scan_offset[7] = 0;
        }
        else {
            cbs->scan_dwell_rest[0] = dwell_rest_time;
            cbs->scan_dwell_rest[1] = dwell_rest_time;
            cbs->scan_dwell_rest[2] = dwell_rest_time;
            cbs->scan_dwell_rest[3] = dwell_rest_time;
            cbs->scan_dwell_rest[4] = 0;
            cbs->scan_dwell_rest[5] = 0;
            cbs->scan_dwell_rest[6] = 0;
            cbs->scan_dwell_rest[7] = 0;
            cbs->scan_offset[0] = 0;
            cbs->scan_offset[1] = DEFAULT_BEACON_INTERVAL - dwell_split_time;
            cbs->scan_offset[2] = 0;
            cbs->scan_offset[3] = DEFAULT_BEACON_INTERVAL - dwell_split_time;
            cbs->scan_offset[4] = 0;
            cbs->scan_offset[5] = 0;
            cbs->scan_offset[6] = 0;
            cbs->scan_offset[7] = 0;
        }
        break;
    default:
        cbs_trace("Dwell time not supported\n");
        break;
        return;
    }
}

static void ieee80211_cbs_cancel(ieee80211_cbs_t cbs)
{
    struct ieee80211com *ic = cbs->cbs_ic;
    struct ieee80211vap *vap = cbs->cbs_vap;
    struct wlan_objmgr_psoc *psoc = NULL;
    struct wlan_objmgr_vdev *vdev = NULL;
    vdev = vap->vdev_obj;
    psoc = wlan_vap_get_psoc(vap);
    /* unregister scan event handler */
    ucfg_scan_unregister_requester(psoc, cbs->cbs_scan_requestor);

    ieee80211_acs_api_complete(ic->ic_acs);
    cbs->cbs_state = IEEE80211_CBS_INIT;

    return;
}

int cbs_scan_start(struct ieee80211vap *vap, ieee80211_cbs_t cbs)
{
    struct scan_start_request *scan_params = NULL;
    QDF_STATUS status = QDF_STATUS_SUCCESS;

    scan_params = (struct scan_start_request*) qdf_mem_malloc(sizeof(*scan_params));
    if (!scan_params) {
        scan_err("unable to allocate scan request");
        return -EINVAL;
    }

    memcpy(scan_params, &cbs->scan_params, sizeof(cbs->scan_params));

    /* Make last split a passive scan */
    if (cbs->dwell_split_cnt < 0) {
        scan_params->scan_req.scan_f_passive = true;
    }

    /* Try to issue a scan */
    if ((status = wlan_ucfg_scan_start(vap,
                  scan_params,
                  cbs->cbs_scan_requestor,
                  SCAN_PRIORITY_HIGH,
                  &(cbs->cbs_scan_id), 0, NULL)) != QDF_STATUS_SUCCESS) {
        eacs_trace(EACS_DBG_LVL0,( " Issue a scan fail."  ));
        scan_err("scan_start failed with status: %d", status);
        ieee80211_cbs_cancel(cbs);
        return -EINVAL;
    }
    return EOK;
}

/* main CBS state machine */
int ieee80211_cbs_event(ieee80211_cbs_t cbs, enum ieee80211_cbs_event event)
{
    struct ieee80211com *ic = NULL;
    int rc = EOK;
    int scan_offset;
    int offset_array_idx;
    struct ieee80211vap *vap;
    enum ieee80211_cbs_state cur_state;
    struct scan_start_request *scan_params = NULL;
    struct wlan_objmgr_vdev *vdev = NULL;
    struct wlan_objmgr_psoc *psoc = NULL;
    struct wlan_objmgr_pdev *pdev = NULL;
    QDF_STATUS status = QDF_STATUS_SUCCESS;

    ic = cbs->cbs_ic;
    vap = cbs->cbs_vap;

    vdev = vap->vdev_obj;
    status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_OSIF_SCAN_ID);

    if (QDF_IS_STATUS_ERROR(status)) {
        scan_info("unable to get reference");
        return EBUSY;
    }
    cur_state = cbs->cbs_state;
    psoc = wlan_vap_get_psoc(vap);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_CBS,
                      "%s: CBS state %d Event %d\n", __func__, cur_state, event);

    scan_params = &cbs->scan_params;

    switch (event) {
    case IEEE80211_CBS_SCAN_START:
        switch (cur_state) {
        case IEEE80211_CBS_INIT:
        case IEEE80211_CBS_WAIT: /* notice fallthrough */
            if(wlan_autoselect_in_progress(vap)) {
                cbs_trace("ACS in progress, try later\n");
                rc = -EINPROGRESS;
                goto release_ref;
            }
            /* Prepare ACS */
            rc = ieee80211_acs_api_prepare(vap, ic->ic_acs, IEEE80211_MODE_NONE, cbs->chan_list, &cbs->nchans);
            if (rc != EOK)
                goto release_ref;

            cbs->chan_list_idx = 0;
            /* register scan event handler */
            cbs->cbs_scan_requestor = ucfg_scan_register_requester(psoc, (uint8_t*)"cbs",
                ieee80211_cbs_scan_evhandler, (void *)cbs);

            if (!cbs->cbs_scan_requestor) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                    "%s: ucfg_scan_register_requestor() failed handler=%08p,%08p status=%08X\n",
                    __func__, ieee80211_cbs_scan_evhandler, cbs, status);
                scan_err("unable to allocate requester: status: %d", status);
                ucfg_scan_unregister_requester(psoc, cbs->cbs_scan_requestor);
                rc = -EINVAL;
                goto release_ref;
            }

            /* Fill scan parameter */
            status = wlan_update_scan_params(vap,scan_params,IEEE80211_M_HOSTAP,true,true,true,true,0,NULL,0);
            if (status) {
                scan_err("scan param init failed with status: %d", status);
                ucfg_scan_unregister_requester(psoc, cbs->cbs_scan_requestor);
                rc = -EINVAL;
                goto release_ref;
            }

            scan_params->scan_req.scan_flags = 0;
            scan_params->legacy_params.scan_type = SCAN_TYPE_BACKGROUND;
            scan_params->legacy_params.min_dwell_passive = CBS_DEFAULT_DWELL_TIME;
            scan_params->scan_req.dwell_time_passive = CBS_DEFAULT_DWELL_TIME + 5;
            scan_params->scan_req.min_rest_time = CBS_DEFAULT_MIN_REST_TIME;
            scan_params->scan_req.max_rest_time = CBS_DEFAULT_DWELL_REST_TIME;
            scan_params->scan_req.scan_f_passive = false;
            scan_params->scan_req.scan_f_2ghz = true;
            scan_params->scan_req.scan_f_5ghz = true;
            scan_params->scan_req.scan_f_offchan_mgmt_tx = true;
            scan_params->scan_req.scan_f_offchan_data_tx = true;
            scan_params->scan_req.scan_f_chan_stat_evnt = true;

            /*TODO Add min_dwell_rest_time in scan parameter and initinalize it here*/
            /*Setting offsets and dwell rest times*/
            if (cbs->min_dwell_rest_time % DEFAULT_BEACON_INTERVAL) {
                cbs->min_dwell_rest_time =
                    (cbs->min_dwell_rest_time / (2*DEFAULT_BEACON_INTERVAL))
                    *2*DEFAULT_BEACON_INTERVAL +
                    ((cbs->min_dwell_rest_time % 200 < 100) ? 100 : 200);
            }

            /* Pre-fill dwell-split, dwell-rest times for differnet interations */
            ieee80211_cbs_init_dwell_params(cbs, cbs->dwell_split_time, cbs->min_dwell_rest_time);

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CBS,
                              "Current Channel for scan is %d \n",
                              cbs->chan_list[cbs->chan_list_idx]);

            /* Array bound check */
            offset_array_idx = cbs->max_arr_size_used - cbs->dwell_split_cnt - 1;
            if ((offset_array_idx < MIN_SCAN_OFFSET_ARRAY_SIZE)
                   || (offset_array_idx > MAX_SCAN_OFFSET_ARRAY_SIZE)){
                rc = -EINVAL;
                goto release_ref;
            }

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CBS,
                              "Offset for scan is %d \n",
                              cbs->scan_offset[offset_array_idx]);

            scan_offset = cbs->scan_offset[offset_array_idx];

            /* channel to scan */
            status = ucfg_scan_init_chanlist_params(scan_params, 1, &cbs->chan_list[cbs->chan_list_idx++], NULL);
            if (status) {
                ucfg_scan_unregister_requester(psoc, cbs->cbs_scan_requestor);
                rc = -EINVAL;
                goto release_ref;
            }

            scan_params->scan_req.scan_offset_time = (scan_offset +26) * 1000;

            pdev = wlan_vdev_get_pdev(vdev);
            /*Flush scan table before starting scan */
            ucfg_scan_flush_results(pdev, NULL);

            cbs->cbs_state = IEEE80211_CBS_SCAN;
            cbs->dwell_split_cnt--;

            /* Enable ACS Ranking */
            ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_RANK , 1);

	    if ((status = cbs_scan_start(vap, cbs) != EOK)) {
                rc = -EINVAL;
                goto release_ref;
            }
            break;
        default:
            cbs_trace("Can't start scan in current state %d.\n", cur_state);
            rc = -EINVAL;
            goto release_ref;
        }
        break;
    case IEEE80211_CBS_SCAN_NEXT:
        switch (cur_state) {
        case IEEE80211_CBS_SCAN:
            if (cbs->chan_list_idx < cbs->nchans) {
                cbs->cbs_state = IEEE80211_CBS_REST;
                OS_SET_TIMER(&cbs->cbs_timer, cbs->rest_time);
            } else
                ieee80211_cbs_post_event(cbs, IEEE80211_CBS_SCAN_COMPLETE);
            break;
        default:
            break;
        }
        break;
    case IEEE80211_CBS_DWELL_SPLIT:
        switch (cur_state) {
        case IEEE80211_CBS_SCAN:
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CBS,
                              " dwell reset %d\n", cbs->scan_dwell_rest[cbs->dwell_split_cnt]);

            /* Array bound check */
            offset_array_idx = cbs->max_arr_size_used - cbs->dwell_split_cnt - 1;
            if ((offset_array_idx < MIN_SCAN_OFFSET_ARRAY_SIZE)
                   || (offset_array_idx > MAX_SCAN_OFFSET_ARRAY_SIZE)){
                rc = -EINVAL;
                goto release_ref;
            }
            if (cbs->scan_dwell_rest[offset_array_idx] == 0) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_CBS,
                                  "Current offset for scan is %d \n",
                                  cbs->scan_offset[offset_array_idx]);

                scan_offset = cbs->scan_offset[offset_array_idx];

                scan_params->scan_req.scan_offset_time = (scan_offset +26) * 1000;
                cbs->dwell_split_cnt--;

		if ((status = cbs_scan_start(vap, cbs) != EOK)) {
                    rc = -EINVAL;
                    goto release_ref;
                }
            }
            else {
                cbs->cbs_state = IEEE80211_CBS_REST;
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_CBS,
                              " dwell rest timer %d\n",
                              cbs->scan_dwell_rest[offset_array_idx]);
                /* Dwell rest time array also takes same idx as that of offset*/
                OS_SET_TIMER(&cbs->cbs_timer,
                             cbs->scan_dwell_rest[offset_array_idx]);
            }
            break;
        case IEEE80211_CBS_REST:
            cbs->cbs_state = IEEE80211_CBS_SCAN;

            /* Array bound check */
            offset_array_idx = cbs->max_arr_size_used - cbs->dwell_split_cnt - 1;
            if ((offset_array_idx < MIN_SCAN_OFFSET_ARRAY_SIZE)
                   || (offset_array_idx > MAX_SCAN_OFFSET_ARRAY_SIZE)){
                rc = -EINVAL;
                goto release_ref;
            }

            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CBS,
                              "Current offset for scan is %d \n",
                              cbs->scan_offset[offset_array_idx]);

            scan_offset = cbs->scan_offset[offset_array_idx];

            scan_params->scan_req.scan_offset_time = (scan_offset +26) * 1000;
            cbs->dwell_split_cnt--;

	    if ((status = cbs_scan_start(vap, cbs) != EOK)) {
                rc = -EINVAL;
                goto release_ref;
            }
            break;
        default:
            break;
        }
        break;
    case IEEE80211_CBS_SCAN_CONTINUE:
        switch (cur_state) {
        case IEEE80211_CBS_REST:
             IEEE80211_DPRINTF(vap, IEEE80211_MSG_CBS,
                              "Current Channel for scan is %d \n", cbs->chan_list[cbs->chan_list_idx]);

            cbs->dwell_split_cnt = cbs->max_dwell_split_cnt;
            /* channel to scan */
            status = ucfg_scan_init_chanlist_params(scan_params, 1, &cbs->chan_list[cbs->chan_list_idx++], NULL);
            if (status) {
                ucfg_scan_unregister_requester(psoc, cbs->cbs_scan_requestor);
                rc = -EINVAL;
                goto release_ref;
            }

            /* Array bound check */
            offset_array_idx = cbs->max_arr_size_used - cbs->dwell_split_cnt - 1;
            if ((offset_array_idx < MIN_SCAN_OFFSET_ARRAY_SIZE)
                   || (offset_array_idx > MAX_SCAN_OFFSET_ARRAY_SIZE)){
                rc = -EINVAL;
                goto release_ref;
            }

            cbs->cbs_state = IEEE80211_CBS_SCAN;
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_CBS,
                              "Current offset for scan is %d \n",
                              cbs->scan_offset[offset_array_idx]);

            scan_offset = cbs->scan_offset[offset_array_idx];
            scan_params->scan_req.scan_offset_time = (scan_offset +26) * 1000;
            cbs->dwell_split_cnt--;

	    if ((status = cbs_scan_start(vap, cbs) != EOK)) {
                rc = -EINVAL;
                goto release_ref;
            }
            break;
        default:
            break;
        }
        break;
    case IEEE80211_CBS_SCAN_CANCEL_SYNC:
        switch (cur_state) {
        case IEEE80211_CBS_INIT:
            /* do nothing */
            break;
        default:
            qdf_timer_sync_cancel(&cbs->cbs_timer);

            if (wlan_scan_in_progress(cbs->cbs_vap)) {
                wlan_ucfg_scan_cancel(cbs->cbs_vap, cbs->cbs_scan_requestor,
                                      0, WLAN_SCAN_CANCEL_VDEV_ALL, true);
            }
            ieee80211_cbs_cancel(cbs);
            break;
        }
        break;
     case IEEE80211_CBS_SCAN_CANCEL_ASYNC:
        switch (cur_state) {
        case IEEE80211_CBS_INIT:
            /* do nothing */
            break;
        default:
            qdf_timer_sync_cancel(&cbs->cbs_timer);

            if (wlan_scan_in_progress(cbs->cbs_vap)) {
                wlan_ucfg_scan_cancel(cbs->cbs_vap, cbs->cbs_scan_requestor,
                                      0, WLAN_SCAN_CANCEL_VDEV_ALL, false);
            }
            ieee80211_cbs_cancel(cbs);
            break;
        }
        break;
    case IEEE80211_CBS_SCAN_COMPLETE:
        switch (cur_state) {
        default:
            /* unregister scan event handler */
            ucfg_scan_unregister_requester(psoc, cbs->cbs_scan_requestor);
            cbs->cbs_state = IEEE80211_CBS_RANK;
            ieee80211_cbs_post_event(cbs, IEEE80211_CBS_RANK_START);
            break;
        }
        break;
    case IEEE80211_CBS_RANK_START:
        switch (cur_state) {
        case IEEE80211_CBS_RANK:
            /* Rank all channels */
            ieee80211_acs_api_rank(ic->ic_acs, cbs->nchans);
            ieee80211_cbs_post_event(cbs, IEEE80211_CBS_RANK_COMPLETE);
        default:
            break;
        }
        break;
    case IEEE80211_CBS_RANK_COMPLETE:
        switch (cur_state) {
        case IEEE80211_CBS_RANK:
            ieee80211_acs_api_complete(ic->ic_acs);
            if (cbs->wait_time) {
                cbs->cbs_state = IEEE80211_CBS_WAIT;
                OS_SET_TIMER(&cbs->cbs_timer, cbs->wait_time);
            } else {
                cbs->cbs_state = IEEE80211_CBS_INIT;
            }
        default:
            break;
        }
        break;
    case IEEE80211_CBS_DCS_INTERFERENCE:
        switch (cur_state) {
        default:
            break;
        }
        break;
    case IEEE80211_CBS_STATS_COLLECT:
        switch (cur_state) {
        default:
            break;
        }
        break;
    case IEEE80211_CBS_STATS_COMPLETE:
        switch (cur_state) {
        default:
            break;
        }
        break;
    default:
        break;
    }

release_ref:
    wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
    return rc;
}

int ieee80211_cbs_init(ieee80211_cbs_t *cbs, wlan_dev_t devhandle, osdev_t osdev)
{
    OS_MEMZERO(*cbs, sizeof(struct ieee80211_cbs));

    (*cbs)->cbs_ic     = devhandle;
    (*cbs)->cbs_osdev  = osdev;
    (*cbs)->rest_time  = CBS_DEFAULT_RESTTIME;
    (*cbs)->dwell_time = CBS_DEFAULT_DWELL_TIME;
    (*cbs)->wait_time  = CBS_DEFAULT_WAIT_TIME;
    (*cbs)->dwell_split_time = CBS_DEFAULT_DWELL_SPLIT_TIME;
    (*cbs)->min_dwell_rest_time = CBS_DEFAULT_DWELL_REST_TIME;

    spin_lock_init(&((*cbs)->cbs_lock));

    OS_INIT_TIMER(osdev, & (*cbs)->cbs_timer, cbs_timer, (void * )(*cbs), QDF_TIMER_TYPE_WAKE_APPS);

    (*cbs)->cbs_state = IEEE80211_CBS_INIT;

    qdf_create_work(osdev, &((*cbs)->cbs_work), cbs_work, (void *)(*cbs));

    cbs_trace("CBS Inited\n");

    return EOK;
}

void ieee80211_cbs_deinit(ieee80211_cbs_t *cbs)
{
    OS_FREE_TIMER(&(*cbs)->cbs_timer);
    spin_lock_destroy(&((*cbs)->cbs_lock));
}

int ieee80211_cbs_attach(ieee80211_cbs_t *cbs,
        wlan_dev_t          devhandle,
        osdev_t             osdev)
{
    if (*cbs)
        return -EINPROGRESS;

    *cbs = (ieee80211_cbs_t) OS_MALLOC(osdev, sizeof(struct ieee80211_cbs), 0);

    if (*cbs == NULL) {
        return -ENOMEM;
    }

    ieee80211_cbs_init(&(*cbs), devhandle, osdev);
    return EOK;
}

int ieee80211_cbs_detach(ieee80211_cbs_t *cbs)
{
    if (*cbs == NULL)
        return EINPROGRESS;

    qdf_destroy_work(NULL, &(*cbs)->cbs_work);
    ieee80211_cbs_deinit(&(*cbs));
    OS_FREE(*cbs);

    *cbs = NULL;

    return EOK;
}

int ieee80211_cbs_scan(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_cbs_t cbs = ic->ic_cbs;

    if (ic->ic_acs == NULL) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CBS,
                "%s: CBS Needs ACS to be enabled\n", __func__);
        return -EINVAL;
    }

    cbs->cbs_vap = vap;

    ieee80211_cbs_post_event(cbs, IEEE80211_CBS_SCAN_START);

    return 0;
}

int ieee80211_cbs_set_param(ieee80211_cbs_t cbs, int param , int val)
{
    struct ieee80211com *ic = cbs->cbs_ic;
    int ret = EOK;

    switch (param) {
        case IEEE80211_CBS_ENABLE:
            cbs->cbs_enable = val;
            if (val == 0) {
                wlan_bk_scan_stop(ic);
                ret = EOK;
                break;
            }
            ret = wlan_bk_scan(ic);
            break;
        case  IEEE80211_CBS_DWELL_SPLIT_TIME:
            if (val != CBS_DWELL_TIME_25MS && val != CBS_DWELL_TIME_50MS &&
                val != CBS_DWELL_TIME_75MS) {
                qdf_print("Dwell time not supported ");
                ret = -EINVAL;
                break;
            }
            wlan_bk_scan_stop(ic);
            cbs->dwell_split_time = val;
            if (cbs->cbs_enable)
                ret = wlan_bk_scan(ic);
            break;
        case IEEE80211_CBS_DWELL_REST_TIME:
            if (val < DEFAULT_BEACON_INTERVAL){
                qdf_print("Invalid rest time. Rest time should be non-negative ");
                ret = -EINVAL;
                break;
            }
            wlan_bk_scan_stop(ic);
            if (val % DEFAULT_BEACON_INTERVAL != 0) {
                val = (val / (2*DEFAULT_BEACON_INTERVAL))
                    * (2*DEFAULT_BEACON_INTERVAL) +
                (((val % (2*DEFAULT_BEACON_INTERVAL)) < DEFAULT_BEACON_INTERVAL)
                      ? DEFAULT_BEACON_INTERVAL : 2*DEFAULT_BEACON_INTERVAL);
            }
            cbs->min_dwell_rest_time = val;
            if (cbs->cbs_enable)
                ret = wlan_bk_scan(ic);
            break;
        case IEEE80211_CBS_WAIT_TIME:
            if (val < 0){
                qdf_print("Wait time cannot be negative ");
                ret = -EINVAL;
                break;
            }
            wlan_bk_scan_stop(ic);
            if (val % DEFAULT_BEACON_INTERVAL != 0){
                val = (val / (2*DEFAULT_BEACON_INTERVAL))
                    * (2*DEFAULT_BEACON_INTERVAL) +
                (((val % (2*DEFAULT_BEACON_INTERVAL)) < DEFAULT_BEACON_INTERVAL)
                  ? DEFAULT_BEACON_INTERVAL : 2*DEFAULT_BEACON_INTERVAL);
               }
            cbs->wait_time = val;
            if (cbs->cbs_enable)
                ret = wlan_bk_scan(ic);
            break;
        case IEEE80211_CBS_REST_TIME:
            if (val < 0){
                qdf_print("Rest time cannot be negative ");
                ret = -EINVAL;
                break;
            }
            wlan_bk_scan_stop(ic);
            cbs->rest_time = val;
            if (cbs->cbs_enable)
                ret = wlan_bk_scan(ic);
            break;
       case IEEE80211_CBS_CSA_ENABLE:
           wlan_bk_scan_stop(ic);
           if (val != 0) {
               cbs->cbs_csa = 1;
           } else {
               cbs->cbs_csa = 0;
           }
           if (cbs->cbs_enable)
               ret = wlan_bk_scan(ic);
           break;
    default:
        break;
    }
    return ret;
}

int ieee80211_cbs_get_param(ieee80211_cbs_t cbs, int param)
{
    int val = 0;

    switch (param) {
        case  IEEE80211_CBS_ENABLE:
            val = cbs->cbs_enable;
            break;
        case  IEEE80211_CBS_DWELL_SPLIT_TIME:
            val = cbs->dwell_split_time;
            break;
        case  IEEE80211_CBS_DWELL_REST_TIME:
            val = cbs->min_dwell_rest_time;
            break;
        case IEEE80211_CBS_REST_TIME:
            val = cbs->rest_time;
            break;
        case  IEEE80211_CBS_WAIT_TIME:
            val = cbs->wait_time;
            break;
        case  IEEE80211_CBS_CSA_ENABLE:
            val = cbs->cbs_csa;
            break;
    }
    return val;
}

/* List of function APIs used by other modules within the driver */

int ieee80211_cbs_api_change_home_channel(ieee80211_cbs_t cbs)
{
    struct ieee80211com *ic = cbs->cbs_ic;
    struct ieee80211vap *vap;
    int chan, found = 0, i = 1, ch_switch_tbtt = 3;
    /* ch_switch_tbtt is the countdown tbtt number sent
       by AP to all stations during a channel switch
    */
    struct ieee80211_ath_channel* channel;

    /* Call async in interrupt context */
    wlan_bk_scan_stop_async(ic);

    /* Loop through and figure the first VAP on this radio */
    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            found = 1;
            break;
        }
    }
    if (!found) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_CBS,
                "%s: No VAPs exist\n", __func__);
        return -EINVAL;
    }
    /* Get the 1st ranked channel from ACS and check for a valid non-dfs channel.
       If not available, fall back to dfs channel. */
    do{
        chan = ieee80211_acs_api_get_ranked_chan(cbs->cbs_ic->ic_acs, i++);
        channel = ieee80211_doth_findchan(vap, chan);

        /* When all channels are checked and there is no non-dfs channel
           other than current channel, continue with the first best channel */
        if (!chan){
            chan = ieee80211_acs_api_get_ranked_chan(cbs->cbs_ic->ic_acs, 1);
            channel = ieee80211_doth_findchan(vap, chan);
            /* First chan value being null implies empty channel list. So return in that case */
            if (!chan) {
                cbs_trace("Returning due to empty channel list\n");
                return -EINVAL;
            } else if ((ic->ic_curchan->ic_ieee == chan) || (channel == NULL) ) {
            /* If first chan is same as current chan or channel obtained is not valid for current mode, get next best chan */
                chan = ieee80211_acs_api_get_ranked_chan(cbs->cbs_ic->ic_acs, 2);
                channel = ieee80211_doth_findchan(vap, chan);
                /* Check if channel structure is non-null */
                if (!channel) {
                    cbs_trace("Returning due to null channel structure.\n");
                    return -EINVAL;
                }
                break;
            /* This is the case where we get a non-zero chan, non-NULL channel which is not same as current channel */
            } else {
                break;
            }
        }
      /* Loop through again if channel is NULL, or if channel is DFS, or if channel is same as current channel */
    } while (((channel == NULL) ? 1 : IEEE80211_IS_CHAN_DFS(channel)) || (ic->ic_curchan->ic_ieee == chan));

    cbs_trace("New home channel %d\n", chan);
    /* switch ASAP, set the CSA TBTT count to 1 */
    ieee80211_ucfg_set_chanswitch(vap, chan, ch_switch_tbtt, 0);

    return EOK;
}

int wlan_bk_scan(struct ieee80211com *ic)
{
    struct ieee80211vap * vap;
    int found = 0;

    qdf_print("Starting CBS");

    /* Loop through and figure the first VAP on this radio */
    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            found = 1;
            break;
        }
    }
    if (found) {
        ieee80211_cbs_scan(vap);
    } else {
        printk(" failed to start CBS. no vap found\n");
        return -EINVAL;
    }
    return EOK;
}

void wlan_bk_scan_stop(struct ieee80211com *ic)
{
    ieee80211_cbs_t cbs = ic->ic_cbs;

    if (cbs->cbs_state != IEEE80211_CBS_INIT ) {
        ieee80211_cbs_event(cbs, IEEE80211_CBS_SCAN_CANCEL_SYNC);
    }
    qdf_cancel_work(&cbs->cbs_work);
}

void wlan_bk_scan_stop_async(struct ieee80211com *ic)
{
    ieee80211_cbs_t cbs = ic->ic_cbs;

    if (cbs->cbs_state != IEEE80211_CBS_INIT ) {
        ieee80211_cbs_post_event(cbs, IEEE80211_CBS_SCAN_CANCEL_ASYNC);
    }
}

#else /* UMAC_SUPPORT_CBS */
int wlan_bk_scan(struct ieee80211com *ic)
{
    /* nothing to do */
    return EOK;
}
void wlan_bk_scan_stop(struct ieee80211com *ic)
{
    /* nothing to do */
}
void wlan_bk_scan_stop_async(struct ieee80211com *ic)
{
    /* nothing to do */
}
#endif /* UMAC_SUPPORT_CBS */
