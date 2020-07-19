/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

#include "scan_sched.h"
#include "ieee80211_sm.h"


#if ATH_SUPPORT_MULTIPLE_SCANS

/* Absolute scan priorities for VAP opmode IEEE80211_M_IBSS */
static IEEE80211_PRIORITY_MAPPING ibss_scan_priority_mapping[1] =
{
    /* Relative Scan Priority */
    ///* LOW  MEDIUM   HIGH   */
    //{   50,     80,   110 }      /* vap->iv_scan_priority_base == 0 */
    /* VERY_LOW LOW  MEDIUM   HIGH  VERY_HIGH  */
    {   40,     50,     80,   110,    140 }     /* vap->iv_scan_priority_base == 0 */
};

/* Absolute scan priorities for VAP opmode IEEE80211_M_STA */
static IEEE80211_PRIORITY_MAPPING station_scan_priority_mapping[3] =
{
    /* Relative Scan Priority */
    ///* LOW  MEDIUM   HIGH   */
    //{   70,    100,   130 },     /* vap->iv_scan_priority_base == 0 */
    //{   70,    100,   130 },     /* vap->iv_scan_priority_base == 1 */
    //{   70,    100,   130 }      /* vap->iv_scan_priority_base == 2 */
    /* VERY_LOW LOW  MEDIUM   HIGH  VERY_HIGH   */
    {   30,     70,    100,   130,   150 },     /* vap->iv_scan_priority_base == 0 */
    {   30,     70,    100,   130,   150 },     /* vap->iv_scan_priority_base == 1 */
    {   30,     70,    100,   130,   150 }      /* vap->iv_scan_priority_base == 2 */
};

/* Absolute scan priorities for VAP opmode IEEE80211_M_HOSTAP */
static IEEE80211_PRIORITY_MAPPING softap_scan_priority_mapping[3] =
{
    /* Relative Scan Priority */
    ///* LOW  MEDIUM   HIGH   */
    //{   70,    100,   130 },     /* vap->iv_scan_priority_base == 0 */
    //{   70,    100,   130 },     /* vap->iv_scan_priority_base == 1 */
    //{   70,    100,   130 }      /* vap->iv_scan_priority_base == 2 */
    /* VERY_LOW LOW  MEDIUM   HIGH  VERY_HIGH   */
    {   30,     70,    100,   130,   150 },     /* vap->iv_scan_priority_base == 0 */
    {   30,     70,    100,   130,   150 },     /* vap->iv_scan_priority_base == 1 */
    {   30,     70,    100,   130,   150 }      /* vap->iv_scan_priority_base == 2 */
};

static IEEE80211_PRIORITY_MAPPING btamp_scan_priority_mapping[1] =
{
    /* Relative Scan Priority */
    ///* LOW  MEDIUM   HIGH   */
    //{   50,     80,   110 }
    /* VERY_LOW LOW  MEDIUM   HIGH  VERY_HIGH   */
    {   40,     50,     80,   110,    140 }
};

static IEEE80211_PRIORITY_MAPPING_ENTRY default_scan_priority_mapping_table[IEEE80211_OPMODE_MAX+1] =
{
    /* 0: IEEE80211_M_IBSS    */ { ARRAY_LENGTH(ibss_scan_priority_mapping),    ibss_scan_priority_mapping    },
    /* 1: IEEE80211_M_STA     */ { ARRAY_LENGTH(station_scan_priority_mapping), station_scan_priority_mapping },
    /* 2: IEEE80211_M_WDS     */ { 0,                                           NULL                          },
    /* 3: IEEE80211_M_AHDEMO  */ { 0,                                           NULL                          },
    /* 4: N/A                 */ { 0,                                           NULL                          },
    /* 5: N/A                 */ { 0,                                           NULL                          },
    /* 6: IEEE80211_M_HOSTAP  */ { ARRAY_LENGTH(softap_scan_priority_mapping),  softap_scan_priority_mapping  },
    /* 7: N/A                 */ { 0,                                           NULL                          },
    /* 8: IEEE80211_M_MONITOR */ { 0,                                           NULL                          },
    /* 9: IEEE80211_M_BTAMP   */ { ARRAY_LENGTH(btamp_scan_priority_mapping),   btamp_scan_priority_mapping   },
};

#define IEEE80211_SCAN_SCHED_PRINTF(_ssc, _cat, _fmt, ...)      \
    if (ieee80211_msg_ic((_ssc)->ssc_ic, _cat)) {               \
        ieee80211com_note((_ssc)->ssc_ic, _cat,  _fmt, __VA_ARGS__);   \
    }

int ath_scan_get_preemption_data(ath_scanner_t       ss,
                                       wlan_scan_requester  requestor,
                                       wlan_scan_id         scan_id,
                                       scanner_preemption_data_t *preemption_data)
{
    preemption_data->preempted  = ss->ss_preemption_data.preempted;
    preemption_data->nchans     = ss->ss_preemption_data.nchans;
    preemption_data->nallchans  = ss->ss_preemption_data.nallchans;
    preemption_data->next       = ss->ss_preemption_data.next;

    /* Repeat channel being currently scanner upon resumption */
    if (ss->ss_preemption_data.next > 0) {
        ss->ss_preemption_data.next--;
    }

    return EOK;
}

int ath_scan_local_set_priority(ath_scanner_t      ss,
                                wlan_scan_requester requestor,
                                wlan_scan_id        scan_id,
                                scan_priority  scan_priority)
{
    if ((scan_id != 0) && (ss->ss_scan_id != scan_id)) {
        ATH_SCAN_PRINTF(ss, IEEE80211_MSG_ANY, "%s: Invalid scan_id=%d ss_scan_id=%d\n",
                              __func__,
                              scan_id,
                              ss->ss_scan_id);

#if ATH_SUPPORT_MULTIPLE_SCANS
        return EINVAL;
#endif
    }

    if (ss->ss_info.si_scan_in_progress) {
        ss->ss_priority = scan_priority;

        /*
         * Prioritized scans should not wait for data traffic to cease before
         * going off-channel, so always make medium- and high-priority scans forced
         */
        if (scan_priority >= SCAN_PRIORITY_MEDIUM) {
            ss->ss_flags |= IEEE80211_SCAN_NOW;
        }
    }

    return EOK;
}

int ath_scan_local_set_forced_flag(ath_scanner_t      ss,
                                   wlan_scan_requester requestor,
                                   wlan_scan_id        scan_id,
                                   bool                     forced_flag)
{
    if ((scan_id != 0) && (ss->ss_scan_id != scan_id)) {
        ATH_SCAN_PRINTF(ss, IEEE80211_MSG_ANY, "%s: Invalid scan_id=%d ss_scan_id=%d\n",
                              __func__,
                              scan_id,
                              ss->ss_scan_id);

#if ATH_SUPPORT_MULTIPLE_SCANS
        return EINVAL;
#endif
    }

    if (forced_flag) {
        if (ss->ss_info.si_scan_in_progress) {
            ss->ss_flags |= IEEE80211_SCAN_NOW;
        }
    }
    else {
        if (ss->ss_common.ss_info.si_scan_in_progress) {
            ss->ss_flags &= ~IEEE80211_SCAN_NOW;
        }
    }

    return EOK;
}

/*
 * Completion of scans that are still in the scheduler queue.
 * The completion must be handled by the scanner to ensure the notifications
 */
int ath_scan_external_event(ath_scanner_t              ss,
                                  ath_scan_event_type        notification_event,
                                  ath_scan_completion_reason notification_reason,
                                  wlan_scan_id                scan_id,
                                  wlan_scan_requester         requestor)
{
    struct scan_event    scan_ev;

    ATH_SCAN_PRINTF(ss, IEEE80211_MSG_SCAN, "%s: vap=%d scan_id=%d requestor=%08X event=%s reason=%s\n",
        __func__,
        ss->ss_vap_id,
        scan_id,
        requestor,
        ath_scan_notification_event_name(notification_event),
        ath_scan_notification_reason_name(notification_reason));

    OS_MEMZERO(&scan_ev, sizeof(scan_ev));
    scan_ev.vdev_id   = ss->ss_vap_id;
    scan_ev.scan_id   = scan_id;
    scan_ev.requestor = requestor;
    scan_ev.type      = notification_event;
    scan_ev.reason    = notification_reason;

    ieee80211_sm_dispatch(ss->ss_hsm_handle,
                          SCANNER_EVENT_EXTERNAL_EVENT,
                          sizeof(scan_ev),
                          &scan_ev);

    return EOK;
}

static void
ath_scan_scheduler_queue(ath_scan_scheduler_t ssc,
                               PIEEE80211_SCAN_REQUEST    new_request);
static int
dup_scan_parameters(osdev_t               ic_osdev,
                    ath_scan_params *output_params,
                    ath_scan_params *input_params)
{
    int    rc = EOK;

    *output_params = *input_params;

    output_params->chan_list = NULL;
    output_params->ie_data   = NULL;

    do {
        if ((input_params->num_channels > 0) && (input_params->chan_list != NULL)) {
            int    length = input_params->num_channels * sizeof(input_params->chan_list[0]);

            output_params->chan_list =
                (u_int32_t *) OS_MALLOC(ic_osdev, length, GFP_KERNEL);

            if (output_params->chan_list != NULL) {
                OS_MEMCPY(output_params->chan_list, input_params->chan_list, length);
            }
            else {
                rc = ENOMEM;
                break;
            }
        }

        if ((input_params->ie_len > 0) && (input_params->ie_data != NULL)) {
            output_params->ie_data =
                (u_int8_t *) OS_MALLOC(ic_osdev, input_params->ie_len, GFP_KERNEL);

            if (output_params->ie_data != NULL) {
                OS_MEMCPY(output_params->ie_data, input_params->ie_data, input_params->ie_len);
            }
            else {
                rc = ENOMEM;
                break;
            }
        }
    } while (false);

    if (rc != EOK) {
        if (output_params->chan_list != NULL) {
            OS_FREE(output_params->chan_list);
            output_params->chan_list = NULL;
        }

        if (output_params->ie_data != NULL) {
            OS_FREE(output_params->ie_data);
            output_params->ie_data = NULL;
        }
    }

    return rc;
}

static void
free_scan_parameters(ath_scan_params *scan_params)
{
    if (scan_params->chan_list != NULL) {
        OS_FREE(scan_params->chan_list);
        scan_params->chan_list = NULL;
    }

    if (scan_params->ie_data != NULL) {
        OS_FREE(scan_params->ie_data);
        scan_params->ie_data = NULL;
    }
}

static void
free_scan_request(ath_scan_scheduler_t ssc,
                  PIEEE80211_SCAN_REQUEST    *scan_request)
{
    (*scan_request)->scheduling_status = IEEE80211_SCAN_STATUS_COMPLETED;

    free_scan_parameters(&((*scan_request)->params));

    spin_lock(&(ssc->ssc_free_list_lock));

    TAILQ_INSERT_TAIL(&(ssc->ssc_free_list), *scan_request, list);
    (*scan_request) = NULL;

    spin_unlock(&(ssc->ssc_free_list_lock));
}

static PIEEE80211_SCAN_REQUEST
get_scan_request(ath_scan_scheduler_t ssc)
{
    PIEEE80211_SCAN_REQUEST    next_request = NULL;

    spin_lock(&(ssc->ssc_free_list_lock));

    next_request = TAILQ_FIRST(&(ssc->ssc_free_list));

    if (next_request != NULL) {
        ASSERT(next_request->scheduling_status == IEEE80211_SCAN_STATUS_COMPLETED);

        TAILQ_REMOVE(&(ssc->ssc_free_list), next_request, list);
    }
    else {
        next_request = (PIEEE80211_SCAN_REQUEST)
            OS_MALLOC(ssc->ssc_ic->ic_osdev, sizeof(*next_request), GFP_KERNEL);
    }

    spin_unlock(&(ssc->ssc_free_list_lock));

    return next_request;
}

static int
ath_scan_scheduler_start_next_scan(IEEE80211_SCAN_SCHEDULER *ssc)
{
    PIEEE80211_SCAN_REQUEST    next_request = NULL;
    int                        rc = EOK;
    bool                       b_scan_in_progress = false;

    spin_lock(&(ssc->ssc_request_list_lock));
    spin_lock(&(ssc->ssc_current_scan_lock));

    if (ssc->ssc_current_scan != NULL) {
        IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: E_BUSY: current_scan_id #%d priority=%08X status=%d p=%08p\n",
            __func__,
            ssc->ssc_current_scan->scan_id,
            ssc->ssc_current_scan->absolute_priority,
            ssc->ssc_current_scan->scheduling_status,
            ssc->ssc_current_scan);

       spin_unlock(&(ssc->ssc_current_scan_lock));
       spin_unlock(&(ssc->ssc_request_list_lock));

       return EBUSY;
    }

    while (! b_scan_in_progress) {
        if (! TAILQ_EMPTY(&(ssc->ssc_request_list))) {

            next_request = TAILQ_FIRST(&(ssc->ssc_request_list));

            /* Start executing next scan */
            TAILQ_REMOVE(&(ssc->ssc_request_list), next_request, list);
            IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Start scan_id=%d priority=%08X status=%d p=%08p\n",
                __func__,
                next_request->scan_id,
                next_request->absolute_priority,
                next_request->scheduling_status,
                next_request);
        }

        if (next_request != NULL) {
            scan_request_data_t    request_data;

            ASSERT((next_request->scheduling_status == IEEE80211_SCAN_STATUS_QUEUED) ||
                   (next_request->scheduling_status == IEEE80211_SCAN_STATUS_PREEMPTED));

            ssc->ssc_current_scan = next_request;

            OS_MEMZERO(&request_data, sizeof(request_data));

            request_data.requestor         = next_request->requestor;
            request_data.priority          = next_request->requested_priority;
            request_data.scan_id           = next_request->scan_id;
            request_data.request_timestamp = next_request->request_timestamp;
            request_data.params            = &(next_request->params);
            request_data.preemption_data   = next_request->preemption_data;

            rc = ath_scan_run(ssc->ssc_scanner,
                                    next_request->vaphandle,
                                    &request_data);

            if (rc == EOK) {
                b_scan_in_progress = true;
            }
            else {
                IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Failed to start vap=%08p scan_id=%d priority=%08X status=%d p=%08p rc=%08X\n",
                    __func__,
                    next_request->vaphandle,
                    next_request->scan_id,
                    next_request->absolute_priority,
                    next_request->scheduling_status,
                    next_request,
                    rc);

                ath_scan_external_event(ssc->ssc_scanner,
                                              next_request->vaphandle,
                                              (next_request->scheduling_status == IEEE80211_SCAN_STATUS_QUEUED) ?
                                                  SCAN_EVENT_TYPE_DEQUEUED : SCAN_EVENT_TYPE_COMPLETED,
                                              SCAN_REASON_RUN_FAILED,
                                              next_request->scan_id,
                                              next_request->requestor);

                free_scan_request(ssc, &next_request);
                ssc->ssc_current_scan = NULL;
            }
        }
        else {
            break;
        }
    }

    spin_unlock(&(ssc->ssc_current_scan_lock));
    spin_unlock(&(ssc->ssc_request_list_lock));

    return rc;
}

static int
ath_scan_scheduler_dispatch_start_event(IEEE80211_SCAN_SCHEDULER *ssc)
{
    int    rc = EOK;

    if (ssc->ssc_enabled && !ssc->ssc_terminating) {
        rc = OS_MESGQ_SEND(&ssc->ssc_mesg_queue, SCAN_SCHEDULER_EVENT_START_NEXT, 0, NULL);
        if (rc != EOK) {
            IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: FAILED to deliver event SCAN_SCHEDULER_EVENT_START_NEXT rc=%08X!!!\n",
                __func__,
                rc);
        }
    }
    else {
        IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: NOT starting next scan: enabled=%d initialized=%d terminating=%d\n",
            __func__,
            ssc->ssc_enabled,
            ssc->ssc_initialized,
            ssc->ssc_terminating);
    }

    return rc;
}

void
ath_scan_scheduler_preprocess_event(IEEE80211_SCAN_SCHEDULER *ssc,
                                          struct scan_event *event)
{
    if ((event->type == SCAN_EVENT_TYPE_STARTED) ||
        (event->type == SCAN_EVENT_TYPE_RESTARTED)) {
        if ((ssc->ssc_current_scan != NULL) &&
            (event->scan_id == ssc->ssc_current_scan->scan_id)) {
            ssc->ssc_current_scan->scheduling_status = IEEE80211_SCAN_STATUS_RUNNING;
        }
    }
}

void
ath_scan_scheduler_postprocess_event(IEEE80211_SCAN_SCHEDULER *ssc,
                                           struct scan_event *event)
{
    bool    start_next_scan = false;

    spin_lock(&(ssc->ssc_current_scan_lock));

    /*
     * Verify the scan_id before clearing the current scan information.
     * It's possible a queued scan was completed due to cancel or time out,
     * leaving the currently active scan untouched.
     */
    if ((ssc->ssc_current_scan == NULL) ||
        (ssc->ssc_current_scan->scan_id != event->scan_id)) {
        spin_unlock(&(ssc->ssc_current_scan_lock));

        return;
    }

    ASSERT(ssc->ssc_current_scan->scheduling_status == IEEE80211_SCAN_STATUS_RUNNING);

    if (event->type == SCAN_EVENT_TYPE_COMPLETED) {
        free_scan_request(ssc, &(ssc->ssc_current_scan));

        start_next_scan = true;
    }

    if (event->type == SCAN_EVENT_TYPE_PREEMPTED) {
        int    rc;

        rc = ath_scan_get_preemption_data(ssc->ssc_scanner,
                                                ssc->ssc_current_scan->requestor,
                                                ssc->ssc_current_scan->scan_id,
                                                &(ssc->ssc_current_scan->preemption_data));

        if (rc != EOK) {
            IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Failed to preempt scan_id=%d requestor=%s p=%08p rc=%08X\n",
                __func__,
                ssc->ssc_current_scan->scan_id,
                ath_scan_get_requestor_name(ssc->ssc_scanner, ssc->ssc_current_scan->requestor),
                ssc->ssc_current_scan,
                rc);
        }

        ssc->ssc_current_scan->scheduling_status = IEEE80211_SCAN_STATUS_PREEMPTED;

        ath_scan_scheduler_queue(ssc, ssc->ssc_current_scan);

        ssc->ssc_current_scan = NULL;

        start_next_scan = true;
    }

    if (start_next_scan) {
        /*
         * Try to start next scan request if the currently active scan completed.
         * Make sure scheduler is initialized and not unloading
         */
        if ((ssc->ssc_current_scan == NULL) &&
            (ssc->ssc_enabled)              &&
            (ssc->ssc_initialized)          &&
            (! ssc->ssc_terminating)) {
            ath_scan_scheduler_dispatch_start_event(ssc);
        }
        else {
            IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: NOT starting next scan: ssc_current_scan=%08p enabled=%d initialized=%d terminating=%d\n",
                __func__,
                ssc->ssc_current_scan,
                ssc->ssc_enabled,
                ssc->ssc_initialized,
                ssc->ssc_terminating);
        }
    }

    spin_unlock(&(ssc->ssc_current_scan_lock));
}

static void
ath_scan_scheduler_mesg_handler(void      *ctx,
                                      u_int16_t event,
                                      u_int16_t event_data_len,
                                      void      *event_data)
{
    PIEEE80211_SCAN_SCHEDULER    ssc = (PIEEE80211_SCAN_SCHEDULER) ctx;

    switch (event) {
    case SCAN_SCHEDULER_EVENT_START_NEXT:
        ath_scan_scheduler_start_next_scan(ssc);
        break;

    default:
        IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Invalid event %d\n",
            __func__,
            event);
        break;
    }
}

static
OS_TIMER_FUNC(ssc_monitor_timer_handler)
{
    PIEEE80211_SCAN_SCHEDULER    ssc;
    PIEEE80211_SCAN_REQUEST      current_request, tmp;
    systime_t                    current_time;

    OS_GET_TIMER_ARG(ssc, PIEEE80211_SCAN_SCHEDULER);

    spin_lock(&(ssc->ssc_request_list_lock));

    if (ssc->ssc_terminating) {
        spin_unlock(&(ssc->ssc_request_list_lock));
        return;
    }

    current_time = OS_GET_TIMESTAMP();

    /*
     * Examines list of queued scans and remove those who exceeded the
     * specified maximum duration.
     */
    TAILQ_FOREACH_SAFE(current_request, &(ssc->ssc_request_list), list, tmp) {
        if (CONVERT_SYSTEM_TIME_TO_MS(current_time - current_request->request_timestamp) >= current_request->maximum_duration) {
            /* Remove current element from request list */
            TAILQ_REMOVE(&(ssc->ssc_request_list), current_request, list);

            IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Unqueueing request vap=%08p scan_id=%d priority=%08X status=%d p=%08p\n",
                __func__,
                current_request->vaphandle,
                current_request->scan_id,
                current_request->absolute_priority,
                current_request->scheduling_status,
                current_request);

            ath_scan_external_event(ssc->ssc_scanner,
                                          current_request->vaphandle,
                                          SCAN_EVENT_TYPE_COMPLETED,
                                          SCAN_REASON_TIMEDOUT,
                                          current_request->scan_id,
                                          current_request->requestor);

            /* send request to free queue */
            free_scan_request(ssc, &current_request);
        }
    }

    /*
     * Sanity check: if scan scheduler was suspended for a specified amount of
     * time, make sure it eventually resumes.
     */
    if (ssc->ssc_suspend_time != 0) {
        ASSERT(CONVERT_SYSTEM_TIME_TO_MS(current_time - ssc->ssc_suspend_timestamp) <=
            (ssc->ssc_suspend_time + SUSPEND_SCHEDULER_WATCHDOG_LATENCY));
    }

    spin_unlock(&(ssc->ssc_request_list_lock));

    if (! ssc->ssc_terminating) {
        OS_SET_TIMER(&(ssc->ssc_monitor_timer), ssc->ssc_monitoring_interval);
    }
}

static
OS_TIMER_FUNC(ssc_suspend_timer_handler)
{
    PIEEE80211_SCAN_SCHEDULER    ssc;

    OS_GET_TIMER_ARG(ssc, PIEEE80211_SCAN_SCHEDULER);

    IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Timer expired; restarting scans, requestor=0x%x\n",
        __func__, ssc->ssc_suspend_requestor);

    spin_lock(&(ssc->ssc_request_list_lock));

    ssc->ssc_suspend_requestor = 0;

    if (ssc->ssc_terminating) {
        spin_unlock(&(ssc->ssc_request_list_lock));
        return;
    }

    ssc->ssc_suspend_time      = 0;
    ssc->ssc_suspend_timestamp = 0;
    ssc->ssc_enabled           = true;

    ath_scan_scheduler_dispatch_start_event(ssc);

    spin_unlock(&(ssc->ssc_request_list_lock));
}

int
ath_scan_scheduler_set_priority_table(ath_scan_scheduler_t ssc,
                                            enum ieee80211_opmode      opmode,
                                            int                        number_rows,
                                            IEEE80211_PRIORITY_MAPPING *mapping_table)
{
    ASSERT((opmode >= 0) && (opmode <= IEEE80211_OPMODE_MAX));

    spin_lock(&(ssc->ssc_priority_table_lock));

    /*
     * Store number of rows and keep a pointer to the specified mapping table.
     */
    if ((opmode >= 0) && (opmode <= IEEE80211_OPMODE_MAX)) {
        ssc->ssc_priority_mapping_table[opmode].n_rows                        = number_rows;
        ssc->ssc_priority_mapping_table[opmode].opmode_priority_mapping_table = mapping_table;

        spin_unlock(&(ssc->ssc_priority_table_lock));

        return EOK;
    }

    spin_unlock(&(ssc->ssc_priority_table_lock));

    return EINVAL;
}

int
ath_scan_scheduler_get_priority_table(ath_scan_scheduler_t ssc,
                                            enum ieee80211_opmode      opmode,
                                            int                        *p_number_rows,
                                            IEEE80211_PRIORITY_MAPPING **p_mapping_table)
{
    ASSERT((opmode >= 0) && (opmode <= IEEE80211_OPMODE_MAX));

    spin_lock(&(ssc->ssc_priority_table_lock));

    if ((opmode >= 0) && (opmode <= IEEE80211_OPMODE_MAX)) {
        *p_number_rows   = ssc->ssc_priority_mapping_table[opmode].n_rows;
        *p_mapping_table = ssc->ssc_priority_mapping_table[opmode].opmode_priority_mapping_table;

        spin_unlock(&(ssc->ssc_priority_table_lock));

        return EOK;
    }

    spin_unlock(&(ssc->ssc_priority_table_lock));

    return EINVAL;
}

static int
ieee80211_preempt_scan(ath_scan_scheduler_t ssc,
                       PIEEE80211_SCAN_REQUEST    request,
                       wlan_scan_requester   requestor,
                       wlan_scan_id          scan_id)
{
    int    rc;

    /*
     * Do not access fields in structure request since it may be NULL.
     */
    rc = ath_scan_stop(ssc->ssc_scanner,
                             requestor,
                             scan_id,
                             STOP_MODE_PREEMPT,
                             IEEE80211_SCAN_CANCEL_ASYNC);
    if (rc != EOK) {
        IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Failed to preempt scan_id=%d requestor=%s p=%08p rc=%08X\n",
            __func__,
            scan_id,
            ath_scan_get_requestor_name(ssc->ssc_scanner, requestor),
            request,
            rc);
    }

    return rc;
}

static void
determine_highest_priority_scan(ath_scan_scheduler_t ssc)
{
    PIEEE80211_SCAN_REQUEST     first_request;
    wlan_scan_id           current_scan_id = 0;
    wlan_scan_requester    requestor = 0;

    /* Acquire lock to request list */
    spin_lock(&(ssc->ssc_request_list_lock));

    /* Acquire lock to current scan */
    spin_lock(&(ssc->ssc_current_scan_lock));

    first_request = TAILQ_FIRST(&(ssc->ssc_request_list));

    /* Verify whether new scan has higher priority than current scan */
    if ((ssc->ssc_current_scan != NULL) &&
        (first_request != NULL) &&
        (first_request->absolute_priority > ssc->ssc_current_scan->absolute_priority)) {
        current_scan_id = ssc->ssc_current_scan->scan_id;
        requestor       = ssc->ssc_current_scan->requestor;

        IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Preempt scan_id=%d priority=%08X status=%d p=%08p because of scan_id=%d priority=%08X status=%d p=%08p\n",
            __func__,
            ssc->ssc_current_scan->scan_id,
            ssc->ssc_current_scan->absolute_priority,
            ssc->ssc_current_scan->scheduling_status,
            ssc->ssc_current_scan,
            first_request->scan_id,
            first_request->absolute_priority,
            first_request->scheduling_status,
            first_request);
    }

    /* Release lock to current scan */
    spin_unlock(&(ssc->ssc_current_scan_lock));


    /* Release lock to request list */
    spin_unlock(&(ssc->ssc_request_list_lock));

    /*
     * If new scan has higher priority than current scan, cancel current scan.
     * New scan will start running as soon as the current one completes.
     * Current scan ID is passed as an argument
     */
    if (current_scan_id != 0) {
        ieee80211_preempt_scan(ssc, ssc->ssc_current_scan, requestor, current_scan_id);
    }

    /* Schedule next scan if necessary */
    if (ssc->ssc_enabled && !ssc->ssc_terminating) {
        if (ssc->ssc_current_scan == NULL) {
            ath_scan_scheduler_dispatch_start_event(ssc);
        }
    }
    else {
        IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: NOT starting next scan: suspend_time=%d current_time=%d suspend_timestamp=%d enabled=%d initialized=%d terminating=%d\n",
            __func__,
            ssc->ssc_suspend_time,
            (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP()),
            (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(ssc->ssc_suspend_timestamp),
            ssc->ssc_enabled,
            ssc->ssc_initialized,
            ssc->ssc_terminating);
    }
}

static u_int32_t
calculate_absolute_priority(ath_scan_scheduler_t ssc,
                            ieee80211_vap_t            vaphandle,
                            scan_priority    requested_priority)
{
    IEEE80211_PRIORITY_MAPPING_ENTRY    *p_mapping_entry;
    u_int32_t                           absolute_priority = 0;

    ASSERT(requested_priority < SCAN_PRIORITY_COUNT);

    spin_lock(&(ssc->ssc_priority_table_lock));

    p_mapping_entry = &(ssc->ssc_priority_mapping_table[vaphandle->iv_opmode]);
    if (p_mapping_entry->n_rows > 0) {
        if (vaphandle->iv_scan_priority_base < p_mapping_entry->n_rows) {
            absolute_priority = p_mapping_entry->opmode_priority_mapping_table[vaphandle->iv_scan_priority_base][requested_priority];
        }
        else {
            absolute_priority = p_mapping_entry->opmode_priority_mapping_table[p_mapping_entry->n_rows - 1][requested_priority];
        }
    }

    spin_unlock(&(ssc->ssc_priority_table_lock));

    return absolute_priority;
}

/*
 * Do not acquire any locks inside these function.
 * Synchronization is handled by the caller.
 */
static void
ath_scan_scheduler_queue(ath_scan_scheduler_t ssc,
                               PIEEE80211_SCAN_REQUEST    new_request)
{
    if (TAILQ_EMPTY(&(ssc->ssc_request_list))) {
        TAILQ_INSERT_HEAD(&(ssc->ssc_request_list), new_request, list);
        IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Added to head of queue scan_id=%d priority=%08X status=%d active=%d p=%08p\n",
            __func__,
            new_request->scan_id,
            new_request->absolute_priority,
            new_request->scheduling_status,
            ssc->ssc_enabled,
            new_request);
    }
    else {
        PIEEE80211_SCAN_REQUEST    current_request;

        TAILQ_FOREACH(current_request, &(ssc->ssc_request_list), list) {
            if (new_request->absolute_priority > current_request->absolute_priority) {
                TAILQ_INSERT_BEFORE(current_request, new_request, list);
                IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: scan_id=%d priority=%08X status=%d p=%08p inserted before scan_id=%d priority=%08X status=%d p=%08p active=%d\n",
                    __func__,
                    new_request->scan_id,
                    new_request->absolute_priority,
                    new_request->scheduling_status,
                    new_request,
                    current_request->scan_id,
                    current_request->absolute_priority,
                    current_request->scheduling_status,
                    current_request,
                    ssc->ssc_enabled);
                break;
            }
        }

        /*
         * Add new request to the end of the list if not inserted yet.
         */
        if (current_request == NULL) {
            TAILQ_INSERT_TAIL(&(ssc->ssc_request_list), new_request, list);
            IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: inserted at tail scan_id=%d priority=%08X status=%d active=%d p=%08p\n",
                __func__,
                new_request->scan_id,
                new_request->absolute_priority,
                new_request->scheduling_status,
                ssc->ssc_enabled,
                new_request);
        }
    }
}

int
ath_scan_scheduler_add(ath_scan_scheduler_t ssc,
                             ieee80211_vap_t            vaphandle,
                             ath_scan_params      *params,
                             wlan_scan_requester   requestor,
                             scan_priority    priority,
                             wlan_scan_id          *scan_id)
{
    PIEEE80211_SCAN_REQUEST    new_request;
    wlan_scan_id          current_scan_id = 0;
    int                        rc;

    new_request = get_scan_request(ssc);
    if (new_request == NULL) {
        return ENOMEM;
    }

    ASSERT(priority < SCAN_PRIORITY_COUNT);
    if (priority >= SCAN_PRIORITY_COUNT) {
        IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Invalid priority %d\n",
            __func__,
            priority);

        return EINVAL;
    }

    /* Clears request data, including preemption data */
    OS_MEMZERO(new_request, sizeof(*new_request));

    new_request->vaphandle          = vaphandle;
    new_request->requestor          = requestor;
    new_request->requested_priority = priority;
    new_request->absolute_priority  =
        calculate_absolute_priority(ssc, vaphandle, priority);
    new_request->scan_id            = atomic_inc(&(ssc->ssc_id_generator)) & ~IEEE80211_SCAN_CLASS_MASK;
    new_request->request_timestamp  = OS_GET_TIMESTAMP();
    new_request->maximum_duration   = params->max_scan_time;
    new_request->scheduling_status  = IEEE80211_SCAN_STATUS_QUEUED;

    /*
     * Copy scan parameters, including memory blocks dereferenced by those parameters
     */
    rc = dup_scan_parameters(vaphandle->iv_ic->ic_osdev, &(new_request->params), params);
    if (rc != EOK) {
        free_scan_request(ssc, &new_request);

        return rc;
    }

    *scan_id = new_request->scan_id;

    /* Acquire lock to request list */
    spin_lock(&(ssc->ssc_request_list_lock));

    /* Queue new request */
    ath_scan_scheduler_queue(ssc, new_request);

    /* Release lock to request list */
    spin_unlock(&(ssc->ssc_request_list_lock));

    /*
     * Determine highest-priority scan and start it, cancelling current scan if
     * necessary.
     */
    determine_highest_priority_scan(ssc);

    return EOK;
}

static int
ath_scan_scheduler_match_request(ieee80211_vap_t         vaphandle,
                                       wlan_scan_id       match_mask,
                                       PIEEE80211_SCAN_REQUEST current_request)
{
    if ((match_mask & IEEE80211_SCAN_CLASS_MASK) == IEEE80211_ALL_SCANS) {
        /* Cancelling all scans - always match scan id */
        return true;
    }

    if ((match_mask & IEEE80211_SCAN_CLASS_MASK) == IEEE80211_VAP_SCAN) {
        /*
         * Cancelling VAP scans - report a match if scan was requested by the
         * same VAP trying to cancel it.
         */
        return (vaphandle == current_request->vaphandle);
    }

    if ((match_mask & IEEE80211_SCAN_CLASS_MASK) == IEEE80211_SPECIFIC_SCAN) {
        /*
         * Cancelling specific scan - report a match if specified scan id
         * matches the request's scan id.
         */
        return ((match_mask & ~IEEE80211_SCAN_CLASS_MASK) == current_request->scan_id);
    }

    return false;
}

int ath_scan_scheduler_set_priority(ath_scan_scheduler_t ssc,
                                          ieee80211_vap_t            vaphandle,
                                          wlan_scan_requester   requestor,
                                          wlan_scan_id          scan_id_mask,
                                          scan_priority    new_scan_priority)
{
    PIEEE80211_SCAN_REQUEST    current_request, tmp;
    wlan_scan_id          current_scan_id = 0;
    u_int32_t                  new_absolute_priority;
    int                        rc = EOK;
    bool                       b_requeue;

    ASSERT(new_scan_priority < SCAN_PRIORITY_COUNT);
    if (new_scan_priority >= SCAN_PRIORITY_COUNT) {
        IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Invalid priority %d\n",
            __func__,
            new_scan_priority);

        return EINVAL;
    }

    /* Acquire lock to list of requests. */
    spin_lock(&(ssc->ssc_request_list_lock));

    new_absolute_priority =
        calculate_absolute_priority(ssc, vaphandle, new_scan_priority);

    TAILQ_FOREACH_SAFE(current_request, &(ssc->ssc_request_list), list, tmp) {
        /* Find element in the list */
        if (ath_scan_scheduler_match_request(vaphandle, scan_id_mask, current_request)) {
            if (new_scan_priority != current_request->requested_priority) {
                b_requeue = (new_absolute_priority != current_request->absolute_priority);

                IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Adjusting priority scan_id=%d requestor=%s req_priority=%d=>%d abs_priority=%08X=>%08X requeue=%d status=%d p=%08p\n",
                    __func__,
                    current_request->scan_id,
                    ath_scan_get_requestor_name(ssc->ssc_scanner, requestor),
                    current_request->requested_priority, new_scan_priority,
                    current_request->absolute_priority, new_absolute_priority,
                    b_requeue,
                    current_request->scheduling_status,
                    current_request);

                current_request->requested_priority = new_scan_priority;
                current_request->absolute_priority  = new_absolute_priority;

                if (b_requeue) {
                    /* Remove current element from request list */
                    TAILQ_REMOVE(&(ssc->ssc_request_list), current_request, list);

                    ath_scan_scheduler_queue(ssc, current_request);
                }
            }
        }
    }

    /* Now handle possible change of the current scan. */
    /* Acquire lock to current scan. */
    spin_lock(&(ssc->ssc_current_scan_lock));

    if ((ssc->ssc_current_scan != NULL) &&
        ath_scan_scheduler_match_request(vaphandle, scan_id_mask, ssc->ssc_current_scan)) {
        IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Adjusting current scan priority scan_id=%d requestor=%s req_priority=%d=>%d abs_priority=%08X=>%08X status=%d p=%08p\n",
            __func__,
            ssc->ssc_current_scan->scan_id,
            ath_scan_get_requestor_name(ssc->ssc_scanner, requestor),
            ssc->ssc_current_scan->requested_priority, new_scan_priority,
            ssc->ssc_current_scan->absolute_priority, new_absolute_priority,
            ssc->ssc_current_scan->scheduling_status,
            current_request);

        ssc->ssc_current_scan->requested_priority = new_scan_priority;
        ssc->ssc_current_scan->absolute_priority  = new_absolute_priority;

        rc = ath_scan_local_set_priority(ssc->ssc_scanner,
                                         requestor,
                                         ssc->ssc_current_scan->scan_id,
                                         new_scan_priority);

        if (rc != EOK) {
            IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Failed to set forced flag scan_id=%d requestor=%s priority=%08X status=%d p=%08p rc=%08X\n",
                __func__,
                ssc->ssc_current_scan->scan_id,
                ath_scan_get_requestor_name(ssc->ssc_scanner, requestor),
                ssc->ssc_current_scan->absolute_priority,
                ssc->ssc_current_scan->scheduling_status,
                ssc->ssc_current_scan,
                rc);
        }
    }

    /* Release lock to current scan */
    spin_unlock(&(ssc->ssc_current_scan_lock));

    /* Release lock to request list */
    spin_unlock(&(ssc->ssc_request_list_lock));

    /*
     * Determine highest-priority scan and start it, cancelling current scan if
     * necessary.
     */
    determine_highest_priority_scan(ssc);

    return rc;
}

int ath_scan_scheduler_set_forced_flag(ath_scan_scheduler_t ssc,
                                             struct ath_vap        *vaphandle,
                                             wlan_scan_requester   requestor,
                                             wlan_scan_id          scan_id_mask,
                                             bool                       forced_flag)
{
    PIEEE80211_SCAN_REQUEST    current_request, tmp;
    int                        rc = EOK;

    /* Acquire lock to list of requests. */
    spin_lock(&(ssc->ssc_request_list_lock));

    TAILQ_FOREACH_SAFE(current_request, &(ssc->ssc_request_list), list, tmp) {
        /* Find element in the list */
        if (ath_scan_scheduler_match_request(vaphandle, scan_id_mask, current_request)) {

            IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Adjusting forced flag scan_id=%d requestor=%s priority=%08X status=%d p=%08p\n",
                __func__,
                current_request->scan_id,
                ath_scan_get_requestor_name(ssc->ssc_scanner, requestor),
                current_request->absolute_priority,
                current_request->scheduling_status,
                current_request);

            if (forced_flag) {
                current_request->params.flags |= IEEE80211_SCAN_FORCED;
            }
            else {
                current_request->params.flags &= ~IEEE80211_SCAN_FORCED;
            }
        }
    }

    /* Now handle possible change of the current scan. */
    /* Acquire lock to current scan. */
    spin_lock(&(ssc->ssc_current_scan_lock));

    if ((ssc->ssc_current_scan != NULL) &&
        ath_scan_scheduler_match_request(vaphandle, scan_id_mask, ssc->ssc_current_scan)) {
        rc = ath_scan_set_forced_flag(ssc->ssc_scanner,
                                            ssc->ssc_current_scan->requestor,
                                            ssc->ssc_current_scan->scan_id,
                                            forced_flag);

        if (rc != EOK) {
            IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Failed to set forced flag scan_id=%d requestor=%s priority=%08X status=%d p=%08p rc=%08X\n",
                __func__,
                ssc->ssc_current_scan->scan_id,
                ath_scan_get_requestor_name(ssc->ssc_scanner, requestor),
                ssc->ssc_current_scan->absolute_priority,
                ssc->ssc_current_scan->scheduling_status,
                ssc->ssc_current_scan,
                rc);
        }
    }

    /* Release lock to current scan. */
    spin_unlock(&(ssc->ssc_current_scan_lock));

    /* Release lock to list of requests. */
    spin_unlock(&(ssc->ssc_request_list_lock));

    return rc;
}

int
ath_scan_scheduler_remove(ath_scan_scheduler_t ssc,
                                ieee80211_vap_t            vaphandle,
                                wlan_scan_requester   requestor,
                                wlan_scan_id          scan_id_mask,
                                u_int32_t                  flags)
{
    PIEEE80211_SCAN_REQUEST    current_request, tmp;
    int                  rc = EOK;

    /* Acquire lock to list of requests. */
    spin_lock(&(ssc->ssc_request_list_lock));

    TAILQ_FOREACH_SAFE(current_request, &(ssc->ssc_request_list), list, tmp) {
        /* Remove current element from list */
        if (ath_scan_scheduler_match_request(vaphandle, scan_id_mask, current_request)) {
            TAILQ_REMOVE(&(ssc->ssc_request_list), current_request, list);

            IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Unqueueing request vap=%08p scan_id=%d priority=%08X status=%d p=%08p\n",
                __func__,
                current_request->vaphandle,
                current_request->scan_id,
                current_request->absolute_priority,
                current_request->scheduling_status,
                current_request);

            ath_scan_external_event(ssc->ssc_scanner,
                                          current_request->vaphandle,
                                          SCAN_EVENT_TYPE_DEQUEUED,
                                          SCAN_REASON_CANCELLED,
                                          current_request->scan_id,
                                          current_request->requestor);

            /* send request to free queue */
            free_scan_request(ssc, &current_request);
        }
    }

    /* Release lock to list of requests. */
    spin_unlock(&(ssc->ssc_request_list_lock));

    /* Now handle possible cancellation of the current scan. */
    {
        wlan_scan_id    current_scan_id = 0;

        /* Acquire lock to current scan. */
        spin_lock(&(ssc->ssc_current_scan_lock));

        if ((ssc->ssc_current_scan != NULL) &&
            ath_scan_scheduler_match_request(vaphandle, scan_id_mask, ssc->ssc_current_scan)) {
            current_scan_id = ssc->ssc_current_scan->scan_id;
        }

        /* Release lock to current scan. */
        spin_unlock(&(ssc->ssc_current_scan_lock));

        if (current_scan_id != 0) {
            /*
             * Call to scanner module made without any locks acquired to avoid
             * possible deadlock if IEEE80211_SCAN_CANCEL_SYNC flag is used.
             * In a SYNC stop, the scan will terminate synchronously, and the
             * call to ath_scan_scheduler_post_processing causes
             * ssc_current_scan_lock to be acquired again.
             */
            rc = ath_scan_stop(ssc->ssc_scanner,
                                     requestor,
                                     current_scan_id,
                                     STOP_MODE_CANCEL,
                                     flags);
            if (rc != EOK) {
                IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Failed to stop scan_id=%d requestor=%s p=%08p rc=%08X\n",
                    __func__,
                    current_scan_id,
                    ath_scan_get_requestor_name(ssc->ssc_scanner, requestor),
                    ssc->ssc_current_scan,
                    rc);
            }
        }
    }

    return rc;
}

static void
ieee80211_populate_scan_info(ath_scan_info     *info,
                             PIEEE80211_SCAN_REQUEST current_request)
{
    info->type                   = current_request->params.type;
    info->requestor              = current_request->requestor;
    info->scan_id                = current_request->scan_id;
    info->priority               = current_request->requested_priority;
    info->scheduling_status      = current_request->scheduling_status;
    info->min_dwell_time_active  = current_request->params.min_dwell_time_active;
    info->max_dwell_time_active  = current_request->params.max_dwell_time_active;
    info->min_dwell_time_passive = current_request->params.min_dwell_time_passive;
    info->max_dwell_time_passive = current_request->params.max_dwell_time_passive;
    info->min_rest_time          = current_request->params.min_rest_time;
    info->max_rest_time          = current_request->params.max_rest_time;
    info->max_offchannel_time    = current_request->params.max_offchannel_time;
    info->repeat_probe_time      = current_request->params.repeat_probe_time;
    info->min_beacon_count       = current_request->params.min_beacon_count;
    info->flags                  = current_request->params.flags;
    info->scan_start_time        = 0;
    info->cancelled              = false;
    info->in_progress            = false;
}

int ath_scan_scheduler_get_request_info(ath_scan_scheduler_t ssc,
                                              wlan_scan_id          scan_id,
                                              ath_scan_info        *info)
{
    PIEEE80211_SCAN_REQUEST    current_request;

    /* Acquire lock to list of requests. */
    spin_lock(&(ssc->ssc_request_list_lock));

    /* First check whether specified scan_id is the one currently running. */
    /* Acquire lock to current scan. */
    spin_lock(&(ssc->ssc_current_scan_lock));

    if ((ssc->ssc_current_scan != NULL) &&
        ath_scan_scheduler_match_request(NULL, scan_id, ssc->ssc_current_scan)) {
        ieee80211_get_last_scan_info(ssc->ssc_scanner, info);

        /* Release lock to current scan. */
        spin_unlock(&(ssc->ssc_current_scan_lock));

        /* Release lock to current scan. */
        spin_unlock(&(ssc->ssc_request_list_lock));

        return EOK;
    }

    /* Release lock to current scan. */
    spin_unlock(&(ssc->ssc_current_scan_lock));

    TAILQ_FOREACH(current_request, &(ssc->ssc_request_list), list) {
        /* Search for specified scan_id. */
        if (ath_scan_scheduler_match_request(NULL, scan_id, current_request)) {
            break;
        }
    }

    /* If found scan_id. */
    if (current_request != NULL) {
        ieee80211_populate_scan_info(info, current_request);

        spin_unlock(&(ssc->ssc_request_list_lock));

        return EOK;
    }

    /* Release lock to list of requests. */
    spin_unlock(&(ssc->ssc_request_list_lock));

    OS_MEMZERO(info, sizeof(*info));

    return ENOENT;
}

static void
ieee80211_populate_request_info(ath_scan_request_info *request_info,
                                PIEEE80211_SCAN_REQUEST     current_request)
{
    request_info->vaphandle          = current_request->vaphandle;
    request_info->requestor          = current_request->requestor;
    request_info->requested_priority = current_request->requested_priority;
    request_info->absolute_priority  = current_request->absolute_priority;
    request_info->scan_id            = current_request->scan_id;
    request_info->params             = current_request->params;
    request_info->request_timestamp  = current_request->request_timestamp;
    request_info->maximum_duration   = current_request->maximum_duration;
    request_info->scheduling_status  = current_request->scheduling_status;
}

int
ath_scan_scheduler_get_requests(ath_scan_scheduler_t ssc,
                                     ath_scan_request_info *scan_request_info,
                                     int                         *total_requests,
                                     int                         *returned_requests)
{
    PIEEE80211_SCAN_REQUEST    current_request;
    int                        n_output_buffer_entries;

    /* check input parameters */
    ASSERT(scan_request_info != NULL);
    ASSERT(total_requests    != NULL);
    ASSERT(returned_requests != NULL);

    /* save the maximum number of entries the output buffer can hold */
    n_output_buffer_entries = *total_requests;

    /* clear output variables */
    OS_MEMZERO(scan_request_info, n_output_buffer_entries * sizeof(scan_request_info[0]));
    *total_requests = 0;
    *returned_requests = 0;

    /* Acquire lock to list of requests. */
    spin_lock(&(ssc->ssc_request_list_lock));

    /* Acquire lock to current scan. */
    spin_lock(&(ssc->ssc_current_scan_lock));

    /* Place currently active scan request at the head of the list */
    if (ssc->ssc_current_scan != NULL) {
        /*
         * If there's still space in the output buffer, copy current scan
         * information and increment counters and pointers;
         */
        if (*returned_requests < n_output_buffer_entries) {
            ieee80211_populate_request_info(scan_request_info, ssc->ssc_current_scan);
            scan_request_info++;
            (*returned_requests)++;
        }

        /* increment total number of requests */
        (*total_requests)++;
    }

    /* Release lock to current scan. */
    spin_unlock(&(ssc->ssc_current_scan_lock));

    /* copy each queued scan request to the output buffer */
    TAILQ_FOREACH(current_request, &(ssc->ssc_request_list), list) {
        /*
         * If there's still space in the output buffer, copy current scan
         * information and increment counters and pointers;
         */
        if (*returned_requests < n_output_buffer_entries) {
            ieee80211_populate_request_info(scan_request_info, current_request);
            scan_request_info++;
            (*returned_requests)++;
        }

        /* increment total number of requests */
        (*total_requests)++;
    }

    /* Release lock to list of requests. */
    spin_unlock(&(ssc->ssc_request_list_lock));

    /*
     * Return EOK if all current scan requests were copied to the output buffer;
     * return ENOMEM if the output buffer was too small.
     */
    if (*returned_requests == *total_requests) {
        return EOK;
    }
    else {
        return ENOMEM;
    }
}

bool ath_scan_scheduler_scan_in_progress(ath_scan_scheduler_t ssc)
{
    bool    b_scan_in_progress = false;

    /* Acquire lock to request list */
    spin_lock(&(ssc->ssc_request_list_lock));

    /* Acquire lock to current scan */
    spin_lock(&(ssc->ssc_current_scan_lock));

    b_scan_in_progress = (ssc->ssc_current_scan != NULL) ||
                         (TAILQ_FIRST(&(ssc->ssc_request_list)) != NULL);


    /* Release lock to current scan */
    spin_unlock(&(ssc->ssc_current_scan_lock));

    /* Release lock to request list */
    spin_unlock(&(ssc->ssc_request_list_lock));

    return b_scan_in_progress;
}

/*
 * This function will enable the scan scheduler that was previously suspended.
 * Warning: This code assumes only one requestor at one time. If there are more
 * than one requestors, then the last request will overwrite the previous one.
 */
int
ieee80211_enable_scan_scheduler(ath_scan_scheduler_t ssc,
                                wlan_scan_requester   requestor)
{
    IEEE80211_SCAN_SCHED_PRINTF(
        ssc, IEEE80211_MSG_SCAN,
        "%s: module=%s enabled=%d disable_duration=%d current_time=%d suspend_timestamp=%d requestor=0x%x\n",
        __func__,
        ath_scan_get_requestor_name(ssc->ssc_scanner, requestor),
        ssc->ssc_enabled,
        ssc->ssc_suspend_time,
        (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP()),
        (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(ssc->ssc_suspend_timestamp), requestor);

    /* Acquire lock to request list */
    spin_lock(&(ssc->ssc_request_list_lock));

    /* Acquire lock to current scan */
    spin_lock(&(ssc->ssc_current_scan_lock));

    if (ssc->ssc_suspend_requestor != requestor) {

          /* Release lock to current scan */
          spin_unlock(&(ssc->ssc_current_scan_lock));

          /* Release lock to request list */
          spin_unlock(&(ssc->ssc_request_list_lock));

        IEEE80211_SCAN_SCHED_PRINTF(
            ssc, IEEE80211_MSG_SCAN,
            "%s: Warning: scan scheduler is started by different requestor=0x%x (orig=0x%x). Ignore.\n",
            __func__, requestor, ssc->ssc_suspend_requestor);
        return EOK;
    }
    ssc->ssc_suspend_requestor = 0;

    if (ssc->ssc_terminating) {
        /* Release lock to current scan */
        spin_unlock(&(ssc->ssc_current_scan_lock));

        /* Release lock to request list */
        spin_unlock(&(ssc->ssc_request_list_lock));

        return EAGAIN;
    }

    /* Cancel timer set to reenable scans */
    if (ssc->ssc_suspend_time != 0) {
        OS_CANCEL_TIMER(&(ssc->ssc_suspend_timer));

        ssc->ssc_suspend_time      = 0;
        ssc->ssc_suspend_timestamp = 0;
    }

    ssc->ssc_enabled = true;

    ath_scan_scheduler_dispatch_start_event(ssc);

    /* Release lock to current scan */
    spin_unlock(&(ssc->ssc_current_scan_lock));

    /* Release lock to request list */
    spin_unlock(&(ssc->ssc_request_list_lock));

    return EOK;
}

/*
 * If parameter disable_duration is set to 0, scanning is disabled until a
 * subsequent call to ieee80211_enable_scan_scheduler.
 * If parameter disable_duration is non-0, scanning is disabled for the
 * spedified amount of time.
 *
 * Warning: This code assumes only one requestor at one time. If there are more
 * than one requestors, then the last request will overwrite the previous one.
 */
int
ieee80211_disable_scan_scheduler(ath_scan_scheduler_t ssc,
                                 wlan_scan_requester   requestor,
                                 u_int32_t                  disable_duration)
{
    wlan_scan_id           current_scan_id = 0;
    wlan_scan_requester    current_scan_requestor = 0;
    systime_t                   current_time = OS_GET_TIMESTAMP();

    IEEE80211_SCAN_SCHED_PRINTF(
        ssc, IEEE80211_MSG_SCAN,
        "%s: module=%s disable_duration=%d enabled=%d suspend_timestamp=%d requestor=0x%x\n",
        __func__,
        ath_scan_get_requestor_name(ssc->ssc_scanner, requestor),
        disable_duration,
        ssc->ssc_enabled,
        (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(current_time), requestor);

    /* Acquire lock to request list */
    spin_lock(&(ssc->ssc_request_list_lock));

    /* Acquire lock to current scan */
    spin_lock(&(ssc->ssc_current_scan_lock));

    ssc->ssc_enabled = false;

    if (ssc->ssc_terminating) {
        /* Release lock to current scan */
        spin_unlock(&(ssc->ssc_current_scan_lock));

        /* Release lock to request list */
        spin_unlock(&(ssc->ssc_request_list_lock));

        return EAGAIN;
    }

    /* Verify if there is a scan in progress */
    if (ssc->ssc_current_scan != NULL) {
        current_scan_id        = ssc->ssc_current_scan->scan_id;
        current_scan_requestor = ssc->ssc_current_scan->requestor;

        IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Preempt scan_id=%d priority=%08X status=%d p=%08p\n",
            __func__,
            ssc->ssc_current_scan->scan_id,
            ssc->ssc_current_scan->absolute_priority,
            ssc->ssc_current_scan->scheduling_status,
            ssc->ssc_current_scan);
    }

    ssc->ssc_suspend_time = disable_duration;
    /* TODO: we should implement a queue of requestors so that we don't have to overwrite the last one */
    if (ssc->ssc_suspend_requestor != 0) {
        IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN,
            "%s: Warning: already an pending suspend operation. requestor=0x%x\n",
            __func__, ssc->ssc_suspend_requestor);
    }
    ASSERT(requestor != 0);
    ssc->ssc_suspend_requestor = requestor;

    /*
     * If disable_duration is specified, set timer so that scans will be
     * reenabled later.
     */
    if (disable_duration != 0) {
        ssc->ssc_suspend_timestamp = current_time;
        OS_SET_TIMER(&(ssc->ssc_suspend_timer), disable_duration);
    }
    else {
        ssc->ssc_suspend_timestamp = 0;
    }

    /* Release lock to current scan */
    spin_unlock(&(ssc->ssc_current_scan_lock));

    /* Release lock to request list */
    spin_unlock(&(ssc->ssc_request_list_lock));

    /*
     * Stop current scan
     */
    if (current_scan_id != 0) {
        ieee80211_preempt_scan(ssc,
                               ssc->ssc_current_scan,
                               current_scan_requestor,
                               current_scan_id);
    }

    return EOK;
}

int
ath_scan_scheduler_attach(ath_scan_scheduler_t *scan_scheduler,
                                ath_scanner_t        ss,
                                struct ath_softc           *devhandle,
                                osdev_t                    osdev)
{
    int    rc;

    if (*scan_scheduler) {
        return EINPROGRESS; /* already attached ? */
    }

    *scan_scheduler = (ath_scan_scheduler_t) OS_MALLOC(osdev, sizeof(struct ath_scan_scheduler), 0);
    if (*scan_scheduler) {
        OS_MEMZERO(*scan_scheduler, sizeof(struct ath_scan_scheduler));

        spin_lock_init(&((*scan_scheduler)->ssc_request_list_lock));
        spin_lock_init(&((*scan_scheduler)->ssc_free_list_lock));
        spin_lock_init(&((*scan_scheduler)->ssc_current_scan_lock));
        spin_lock_init(&((*scan_scheduler)->ssc_priority_table_lock));

        TAILQ_INIT(&((*scan_scheduler)->ssc_request_list));
        TAILQ_INIT(&((*scan_scheduler)->ssc_free_list));

        OS_MEMCPY((*scan_scheduler)->ssc_priority_mapping_table,
                  default_scan_priority_mapping_table,
                  sizeof(default_scan_priority_mapping_table));

        (*scan_scheduler)->ssc_sc                  = devhandle;
        (*scan_scheduler)->ssc_scanner             = ss;
        (*scan_scheduler)->ssc_enabled             = true;
        (*scan_scheduler)->ssc_initialized         = true;
        (*scan_scheduler)->ssc_terminating         = false;
        (*scan_scheduler)->ssc_id_generator        = 0;
        (*scan_scheduler)->ssc_monitoring_interval = IEEE80211_SCAN_SCHEDULER_DEFAULT_MONITORING_INTERVAL;
        (*scan_scheduler)->ssc_suspend_time        = 0;
        (*scan_scheduler)->ssc_suspend_timestamp   = 0;

        OS_INIT_TIMER(osdev, &((*scan_scheduler)->ssc_monitor_timer), ssc_monitor_timer_handler, (void *) (*scan_scheduler), QDF_TIMER_TYPE_WAKE_APPS);
        OS_INIT_TIMER(osdev, &((*scan_scheduler)->ssc_suspend_timer), ssc_suspend_timer_handler, (void *) (*scan_scheduler), QDF_TIMER_TYPE_WAKE_APPS);

        rc = OS_MESGQ_INIT(osdev,
                           &((*scan_scheduler)->ssc_mesg_queue),
                           0,
                           MAX_SCAN_SCHEDULER_EVENTS,
                           ath_scan_scheduler_mesg_handler,
                           (*scan_scheduler),
                           MESGQ_PRIORITY_HIGH,
                           MESGQ_ASYNCHRONOUS_EVENT_DELIVERY);

        return EOK;
    }

    return ENOMEM;
}

static void
ieee80211_free_queue(ath_scan_scheduler_t ssc,
                     SCHEDULER_QUEUE            *queue)
{
    PIEEE80211_SCAN_REQUEST    current_request;

    while (true) {
        current_request = TAILQ_FIRST(queue);

        if (current_request != NULL) {
            TAILQ_REMOVE(queue, current_request, list);

            IEEE80211_SCAN_SCHED_PRINTF(ssc, IEEE80211_MSG_SCAN, "%s: Freeing request memory scan_id=%d priority=%08X status=%d p=%08p\n",
                __func__,
                current_request->scan_id,
                current_request->absolute_priority,
                current_request->scheduling_status,
                current_request);

            free_scan_parameters(&(current_request->params));
            OS_FREE(current_request);
        }
        else {
            break;
        }
    }
}

int
ath_scan_scheduler_detach(ath_scan_scheduler_t *ssc)
{
    PIEEE80211_SCAN_REQUEST    current_request;
    int                        rc;

    if (*ssc == NULL) {
        return EINPROGRESS; /* already detached ? */
    }

    if (! (*ssc)->ssc_initialized) {
        IEEE80211_SCAN_SCHED_PRINTF(*ssc, IEEE80211_MSG_SCAN, "%s: Scan Scheduler not initialized!\n", __func__);

        return EINVAL;
    }

    spin_lock(&((*ssc)->ssc_request_list_lock));

    (*ssc)->ssc_enabled     = false;
    (*ssc)->ssc_terminating = true;

    spin_unlock(&((*ssc)->ssc_request_list_lock));

    if (! OS_CANCEL_TIMER(&((*ssc)->ssc_monitor_timer))) {
        IEEE80211_SCAN_SCHED_PRINTF(*ssc, IEEE80211_MSG_SCAN, "%s: Monitor timer not cancelled!\n", __func__);
    }
    if (! OS_CANCEL_TIMER(&((*ssc)->ssc_suspend_timer))) {
        if ((*ssc)->ssc_suspend_timestamp != 0) {
            IEEE80211_SCAN_SCHED_PRINTF(*ssc, IEEE80211_MSG_SCAN, "%s: Suspend timer not cancelled!\n", __func__);
        }
    }

    spin_lock(&((*ssc)->ssc_request_list_lock));

    TAILQ_FOREACH(current_request, &((*ssc)->ssc_request_list), list) {
        IEEE80211_SCAN_SCHED_PRINTF(*ssc, IEEE80211_MSG_SCAN, "%s: Found queued scan_id=%d priority=%08X status=%d p=%08p\n",
            __func__,
            current_request->scan_id,
            current_request->absolute_priority,
            current_request->scheduling_status,
            current_request);
    }

    spin_unlock(&((*ssc)->ssc_request_list_lock));

    spin_lock(&((*ssc)->ssc_request_list_lock));
    ieee80211_free_queue(*ssc, &((*ssc)->ssc_request_list));
    spin_unlock(&((*ssc)->ssc_request_list_lock));

    spin_lock(&((*ssc)->ssc_free_list_lock));
    ieee80211_free_queue(*ssc, &((*ssc)->ssc_free_list));
    spin_unlock(&((*ssc)->ssc_free_list_lock));

    spin_lock(&((*ssc)->ssc_current_scan_lock));
    ASSERT((*ssc)->ssc_current_scan == NULL);

    if ((*ssc)->ssc_current_scan != NULL) {
        free_scan_parameters(&((*ssc)->ssc_current_scan->params));
        OS_FREE((*ssc)->ssc_current_scan);
    }
    spin_unlock(&((*ssc)->ssc_current_scan_lock));

    OS_FREE_TIMER(&((*ssc)->ssc_monitor_timer));
    OS_FREE_TIMER(&((*ssc)->ssc_suspend_timer));
    OS_MESGQ_DESTROY(&((*ssc)->ssc_mesg_queue));

    spin_lock_destroy(&((*ssc)->ssc_request_list_lock));
    spin_lock_destroy(&((*ssc)->ssc_free_list_lock));
    spin_lock_destroy(&((*ssc)->ssc_current_scan_lock));
    spin_lock_destroy(&((*ssc)->ssc_priority_table_lock));

    (*ssc)->ssc_initialized = false;

    OS_FREE(*ssc);
    *ssc = NULL;

    return EOK;
}

#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */
