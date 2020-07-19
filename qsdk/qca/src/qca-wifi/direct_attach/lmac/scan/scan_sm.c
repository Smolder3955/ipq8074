/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
#include "scan_sm.h"
#include "scan_sched.h"


extern void ath_scan_enable_txq(ath_dev_t dev);
extern u_int32_t ath_all_txqs_depth(struct ath_softc *sc);

/* Forward declarations */
static bool scanner_leave_bss_channel(ath_scanner_t ss);
static bool scanner_can_leave_home_channel(ath_scanner_t ss);
int ath_scan_get_preemption_data(ath_scanner_t       ss,
                                       wlan_scan_requester  requestor,
                                       wlan_scan_id         scan_id,
                                       scanner_preemption_data_t *preemption_data);

static const char*    scanner_event_name[] = {
    /* SCANNER_EVENT_NONE                    */ "NONE",
    /* SCANNER_EVENT_PROBEDELAY_EXPIRE       */ "PROBEDELAY_EXPIRE",
    /* SCANNER_EVENT_MINDWELL_EXPIRE         */ "MINDWELL_EXPIRE",
    /* SCANNER_EVENT_MAXDWELL_EXPIRE         */ "MAXDWELL_EXPIRE",
    /* SCANNER_EVENT_RESTTIME_EXPIRE         */ "RESTTIME_EXPIRE",
    /* SCANNER_EVENT_EXIT_CRITERIA_EXPIRE    */ "EXIT_CRITERIA_EXPIRE",
    /* SCANNER_EVENT_FAKESLEEP_ENTERED       */ "FAKESLEEP_ENTERED",
    /* SCANNER_EVENT_FAKESLEEP_FAILED        */ "FAKESLEEP_FAILED",
    /* SCANNER_EVENT_RECEIVED_BEACON         */ "RECEIVED_BEACON",
    /* SCANNER_EVENT_SCAN_REQUEST            */ "SCAN_REQUEST",
    /* SCANNER_EVENT_CANCEL_REQUEST          */ "CANCEL_REQUEST",
    /* SCANNER_EVENT_RESTART_REQUEST         */ "RESTART_REQUEST",
    /* SCANNER_EVENT_MAXSCANTIME_EXPIRE      */ "MAXSCANTIME_EXPIRE",
    /* SCANNER_EVENT_FAKESLEEP_EXPIRE        */ "FAKESLEEP_EXPIRE",
    /* SCANNER_EVENT_RADIO_MEASUREMENT_EXPIRE*/ "RADIO_MEASUREMENT_EXPIRE",
    /* SCANNER_EVENT_PROBE_REQUEST_TIMER     */ "PROBE_REQUEST_TIMER",
    /* SCANNER_EVENT_BSSCHAN_SWITCH_COMPLETE */ "BSSCHAN_SWITCH_COMPLETE",
    /* SCANNER_EVENT_OFFCHAN_SWITCH_COMPLETE */ "OFFCHAN_SWITCH_COMPLETE",
    /* SCANNER_EVENT_OFFCHAN_SWITCH_FAILED   */ "OFFCHAN_SWITCH_FAILED",
    /* SCANNER_EVENT_OFFCHAN_SWITCH_ABORTED  */ "OFFCHAN_SWITCH_ABORTED",
    /* SCANNER_EVENT_OFFCHAN_RETRY_EXPIRE    */ "OFFCHAN_RETRY_EXPIRE",
    /* SCANNER_EVENT_CHAN_SWITCH_COMPLETE    */ "CHAN_SWITCH_COMPLETE",
    /* SCANNER_EVENT_SCHEDULE_BSS_COMPLETE   */ "SCHEDULE_BSS_COMPLETE",
    /* SCANNER_EVENT_TERMINATING_TIMEOUT     */ "TERMINATING_TIMEOUT",
    /* SCANNER_EVENT_EXTERNAL_EVENT          */ "EXTERNAL_EVENT",
    /* SCANNER_EVENT_PREEMPT_REQUEST         */ "PREEMPT_REQUEST",
    /* SCANNER_EVENT_FATAL_ERROR             */ "FATAL_ERROR",
};

const char* ath_scan_notification_event_name(enum scan_event_type event)
{
    static const char*    event_name[] = {
        /* SCAN_EVENT_TYPE_STARTED                */ "STARTED",
        /* SCAN_EVENT_TYPE_COMPLETED              */ "COMPLETED",
        /* SCAN_EVENT_TYPE_RADIO_MEASUREMENT_START*/ "RADIO_MEASUREMENT_START",
        /* SCAN_EVENT_TYPE_RADIO_MEASUREMENT_END  */ "RADIO_MEASUREMENT_END",
        /* SCAN_EVENT_TYPE_RESTARTED              */ "RESTARTED",
        /* SCAN_EVENT_TYPE_BSS_CHANNEL            */ "BSS_CHANNEL",
        /* SCAN_EVENT_TYPE_FOREIGN_CHANNEL        */ "FOREIGN_CHANNEL",
        /* SCAN_EVENT_TYPE_BSSID_MATCH            */ "BSSID_MATCH",
        /* SCAN_EVENT_TYPE_FOREIGN_CHANNEL_GET_NF */ "FOREIGN_CHANNEL_GET_NF",
        /* SCAN_EVENT_TYPE_DEQUEUED               */ "DEQUEUED",
        /* SCAN_EVENT_TYPE_PREEMPTED              */ "PREEMPTED"
    };

    ASSERT(SCAN_EVENT_TYPE_MAX == ATH_N(event_name));
    ASSERT(event < ATH_N(event_name));

    return (event_name[event]);
}

const char* ath_scan_notification_reason_name(enum scan_completion_reason reason)
{
    static const char*    reason_name[] = {
        /* IEEE80211_REASON_NONE                 */ "NONE",
        /* IEEE80211_REASON_COMPLETED            */ "COMPLETED",
        /* IEEE80211_REASON_CANCELLED            */ "CANCELLED",
        /* IEEE80211_REASON_TIMEDOUT             */ "TIMEDOUT",
        /* IEEE80211_REASON_TERMINATION_FUNCTION */ "TERMINATION_FUNCTION",
        /* IEEE80211_REASON_MAX_OFFCHAN_RETRIES  */ "MAX_OFFCHAN_RETRIES",
        /* IEEE80211_REASON_PREEMPTED            */ "PREEMPTED",
        /* IEEE80211_REASON_RUN_FAILED           */ "RUN_FAILED",
        /* IEEE80211_REASON_INTERNAL_STOP        */ "INTERNAL_STOP"
    };

    ASSERT(SCAN_REASON_MAX == ATH_N(reason_name));
    ASSERT(reason < ATH_N(reason_name));

    return (reason_name[reason]);
}

bool avp_is_connected(struct ath_vap *avp)
{
    return ((avp->av_up) ? (true) : (false));
}


static void ath_scan_sm_debug_print (void *context, const char *fmt, ...)
{
    ath_scanner_t          ss = context;
    char                   tmp_buf[256];
    va_list                ap;
    u_int32_t              now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

    va_start(ap, fmt);
    vsnprintf(tmp_buf, sizeof(tmp_buf), fmt, ap);
    va_end(ap);
    tmp_buf[sizeof(tmp_buf) - 1] = '\0';

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%d.%03d | %s",
        now / 1000, now % 1000, tmp_buf);
}

static const char* ath_scan_event_name(enum scanner_event event)
{
    ASSERT(event < ATH_N(scanner_event_name));

    return (scanner_event_name[event]);
}

static void*
ath_scan_scheduler_object(ath_scanner_t ss)
{
#if ATH_SUPPORT_MULTIPLE_SCANS
    return ss->ss_scheduler;
#else
    return ss;
#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */
}

static inline bool
is_passive_channel(struct channel_param *channel,
                   u_int16_t                scan_flags)
{
    return ((scan_flags & IEEE80211_SCAN_PASSIVE) ||
             channel->is_chan_passive);
    /*
     * FW relies on WMI_CHAN_FLAG_PASSIVE, why is DA different
     */
#if 0
             IEEE80211_IS_CHAN_CSA(channel));
#endif
}

static void
scanner_post_event(ath_scanner_t                 ss,
                   enum scan_event_type          type,
                   enum scan_completion_reason   reason)
{
    struct scan_event_info *scan_ev;
    struct ath_softc *sc  = ss->ss_sc;
    struct wlan_objmgr_psoc *psoc = wlan_pdev_get_psoc(sc->sc_pdev);

    /*
     * Do not use any fields that may be modified by a simultaneous call to
     * ieee80211_scan_run.
     */
    scan_ev = (struct scan_event_info*) OS_MALLOC(sc->sc_osdev, sizeof(struct scan_event_info), 0);
    OS_MEMZERO(scan_ev, sizeof(struct scan_event_info));
    scan_ev->event.vdev_id       = ss->ss_vap_id;
    scan_ev->event.type          = type;
    scan_ev->event.reason        = reason;
    scan_ev->event.chan_freq     = ss->ss_sc->sc_curchan.channel;
    scan_ev->event.requester     = ss->ss_requestor;
    scan_ev->event.scan_id       = ss->ss_scan_id;

    /*
     * Inform the scheduler that an event notification is about to be sent to
     * registered modules.
     * The scheduler may need to process certain notifications before other
     * modules to ensure any calls to wlan_get_last_scan_info will return
     * consistent data.
     */
    ath_scan_scheduler_preprocess_event(ath_scan_scheduler_object(ss), &scan_ev->event);

    if(sc->sc_rx_ops->scan.scan_ev_handler)
        sc->sc_rx_ops->scan.scan_ev_handler(psoc, scan_ev);

    /*
     * Inform the scheduler that an event notification has been sent to
     * registered modules.
     * The scheduler may need to process certain notifications after other
     * modules to ensure any calls to wlan_get_last_scan_info will return
     * consistent data.
     */
    ath_scan_scheduler_postprocess_event(ath_scan_scheduler_object(ss), &scan_ev->event);
}

int ath_scan_set_channel_list(ath_dev_t dev, void *params)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct scan_chan_list_params *cp = (struct scan_chan_list_params *)params;
    ath_scanner_t ss = NULL;
    int i;

    if(!sc)
        return -EINVAL;

    ss = sc->sc_scanner;
    if(!ss)
        return -EINVAL;

    ss->ss_nallchans = cp->nallchans;
    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: Num channels  %d \n", __func__, ss->ss_nallchans);
    for(i = 0; i < ss->ss_nallchans; i++)
    {
        OS_MEMCPY(&(ss->ss_all_chans[i]), &(cp->ch_param[i]), sizeof(struct channel_param));
    }

    return EOK;
}
#define ATH_2GHZ_FREQUENCY_THRESHOLD    3000
static void ath_construct_chan_list(ath_scanner_t ss, struct scan_req_params *param)
{
    int i,j,k;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: Construct channel list ss->ss_nallchans = %d param->num_chan = %d\n", __func__, ss->ss_nallchans, param->chan_list.num_chan);
    ss->ss_num_chan_indices = 0;
    if(param->chan_list.num_chan == 0)
    {
        k = 0;
        for(j = 0; j < ss->ss_nallchans; j++)
        {
            if(param->scan_f_2ghz && param->scan_f_5ghz)
            {
                ss->ss_scan_chan_indices[k] = j;
                k++;
                if(param->scan_f_half_rate)
                {
                    ss->ss_all_chans[j].half_rate = 1;
                }
                else if(param->scan_f_quarter_rate)
                {
                    ss->ss_all_chans[j].quarter_rate = 1;
                }
            }
			else if(param->scan_f_2ghz)
            {
                if(ss->ss_all_chans[j].mhz < ATH_2GHZ_FREQUENCY_THRESHOLD)
                {
                    ss->ss_scan_chan_indices[k] = j;
                    k++;
                }
                else
                    continue;
            }
            else if(param->scan_f_5ghz)
            {
                if(ss->ss_all_chans[j].mhz < ATH_2GHZ_FREQUENCY_THRESHOLD)
                    continue;
                else
                {
                    ss->ss_scan_chan_indices[k] = j;
                    k++;
                }
            }
            else
            {
                ss->ss_scan_chan_indices[k] = j;
                k++;
                if(param->scan_f_half_rate)
                {
                    ss->ss_all_chans[j].half_rate = 1;
                }
                else if(param->scan_f_quarter_rate)
                {
                    ss->ss_all_chans[j].quarter_rate = 1;
                }
            }
         }
         ss->ss_num_chan_indices = k;
    }
    else
    {
        k = 0;
        for(i = 0 ; i < param->chan_list.num_chan; i++)
        {
            for(j = 0; j < ss->ss_nallchans; j++)
            {
                if((SCAN_CHAN_GET_FREQ(param->chan_list.chan[i].freq) == ss->ss_all_chans[j].mhz) &&
                    (SCAN_CHAN_GET_MODE(param->chan_list.chan[i].freq) == 0 ||
                    (SCAN_CHAN_GET_MODE(param->chan_list.chan[i].freq) == ss->ss_all_chans[j].phy_mode)))
                {
                    ss->ss_scan_chan_indices[k] = j;
                    k++;
                    if(param->scan_f_half_rate)
                    {
                        ss->ss_all_chans[j].half_rate = 1;
                    }
                    else if(param->scan_f_quarter_rate)
                    {
                        ss->ss_all_chans[j].quarter_rate = 1;
                    }
                    break;
                }
            }
        }
        ss->ss_num_chan_indices = k;
    }

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: num chan indices = %d \n", __func__,ss->ss_num_chan_indices);
}

static void scanner_clear_preemption_data(ath_scanner_t ss)
{
    OS_MEMZERO(&(ss->ss_preemption_data), sizeof(ss->ss_preemption_data));

    /* After previous statement, (ss->ss_preemption_data.preempted == false) */
}

static void scanner_save_preemption_data(ath_scanner_t ss)
{
    ss->ss_preemption_data.preempted  = true;
    ss->ss_preemption_data.nchans     = ss->ss_num_chan_indices;
    ss->ss_preemption_data.nallchans  = ss->ss_nallchans;
    ss->ss_preemption_data.next       = ss->ss_next;

    /* Repeat channel being currently scanner upon resumption */
    if (ss->ss_preemption_data.next > 0) {
        ss->ss_preemption_data.next--;
    }

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: next=%d/%d nallchans=%d\n",
        __func__,
        ss->ss_preemption_data.next,
        ss->ss_preemption_data.nchans,
        ss->ss_preemption_data.nallchans);
}



static void ath_schedule_timed_event(ath_scanner_t ss, int msec, enum scanner_event event)
{
    /*
     * ss->ss_pending_event != SCANNER_EVENT_NONE indicates the generic timer
     * has been set but has not expired yet. Trying to start the timer again
     * indicates a flaw in the scanner's behavior.
     */
    if (ss->ss_pending_event != SCANNER_EVENT_NONE) {
        u_int32_t    now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%d.%03d | %s: Timer already set. Event: current=%d new=%d\n",
                              now / 1000, now % 1000, __func__,
                              ss->ss_pending_event, event);
    }

    ss->ss_pending_event = event;
    OS_SET_TIMER(&ss->ss_timer, msec);
}


static int ath_get_all_active_vap_info(struct ath_softc *sc, int exclude_id)
{
    int i,j=0;
    struct ath_vap *avp;

    for (i=0; i < sc->sc_nvaps; i++) {
        avp = sc->sc_vaps[i];
        if (avp) {
            if(avp->av_up && i != exclude_id){
                ++j;
            }
        }
    }
    return j;
}

int ath_scan_run(struct ath_softc *sc, struct scan_request_data *request_data)
{
    int                      i;
    systime_t                current_system_time;
    bool                     bcast_bssid = false;
    ath_scanner_t ss = NULL;
    struct ath_vap *avp = NULL;
    u_int32_t num_active_vaps;
    struct scan_start_request *request = request_data->req;
    struct scan_req_params *params = &request->scan_req;
	struct scan_extra_params_legacy *legacy = &request->legacy_params;

    if(!sc)
        return -EINVAL;

    ss = sc->sc_scanner;
    if(!ss)
        return -EINVAL;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: Scan started by vap %d \n", __func__, params->vdev_id);

    avp = sc->sc_vaps[params->vdev_id];
    if(avp == NULL)
    {
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: Failed to start scan - Invalid vap id\n", __func__);
        return -EINVAL;
    }
    ss->ss_vap_id = params->vdev_id;
    /*
     * sanity check all the params.
     */
    if (params->dwell_time_active > IEEE80211_MAX_MAXDWELL) {
        ATH_SCAN_PRINTF(ss, IEEE80211_MSG_ANY,
            "%s: invalid dwell time active %d (> MAX %d)\n",
            __func__,
            params->dwell_time_active,
            IEEE80211_MAX_MAXDWELL);

        return EINVAL;
    }

    if (params->dwell_time_passive > IEEE80211_MAX_MAXDWELL) {
        ATH_SCAN_PRINTF(ss, IEEE80211_MSG_ANY,
            "%s: invalid dwell time passive %d (> MAX %d)\n",
            __func__,
            params->dwell_time_passive,
            IEEE80211_MAX_MAXDWELL);

        return EINVAL;
    }

    if ((params->probe_delay >= params->dwell_time_active) &&
        (params->dwell_time_active != 0)) {
        ATH_SCAN_PRINTF(ss, IEEE80211_MSG_ANY,
            "%s: invalid probe delay %d (>= active dwell %d)\n",
            __func__,
            params->probe_delay,
            params->dwell_time_active);

        return EINVAL;
    }

/* Deprecated - Used only by P2P  - max_offchannel_time*/

/*
 * offchan_retry_delay and max_offchan_retries does not vary across scan start callers
 * Hard code in DA
 */


    /*
     * Validate time before sending second probe request.
     * Range is 0..min_dwell_time_active.
     * A value of 0 indicates Probe Request should not be repeated.
     * A value of min_dwell_time_active currently means that only 1 Probe Request
     * will be sent, but this will probably change in the future.
     */
    if (params->repeat_probe_time < 0) {
        ATH_SCAN_PRINTF(ss, IEEE80211_MSG_ANY,
            "%s: invalid repeat probe time %d (< 0)\n",
            __func__,
            params->repeat_probe_time);

        return EINVAL;
    }
    if (params->repeat_probe_time > params->dwell_time_active) {
        ATH_SCAN_PRINTF(ss, IEEE80211_MSG_ANY,
            "%s: invalid repeat probe time %d (> param dwell %d)\n",
            __func__,
            params->repeat_probe_time,
            params->dwell_time_active);

        return EINVAL;
    }

    if (params->num_ssids > IEEE80211_SCAN_MAX_SSID) {
        return EINVAL;
    }
    if (params->num_bssid > IEEE80211_SCAN_MAX_BSSID) {
        return EINVAL;
    }
    if (params->chan_list.num_chan > IEEE80211_CHAN_MAX) {
        return EINVAL;
    }


    /*
     * Read scan parameters
     */

    ss->ss_scan_id                    = params->scan_id;
    ss->ss_requestor                  = params->scan_req_id;
    ss->ss_priority                   = params->scan_priority;
    ss->ss_max_dwell_active           = params->dwell_time_active;
    ss->ss_max_dwell_passive          = params->dwell_time_passive;
    ss->ss_cur_max_dwell              = params->dwell_time_passive;
    ss->ss_min_rest_time              = params->min_rest_time;
    ss->ss_max_rest_time              = params->max_rest_time;
    ss->ss_repeat_probe_time          = params->repeat_probe_time;
    ss->ss_idle_time                  = params->idle_time;
    ss->ss_max_scan_time              = params->max_scan_time;
    ss->ss_probe_delay                = params->probe_delay;
    ss->ss_strict_passive_scan        = params->scan_f_strict_passive_pch;


    /* Retain params for DA */
    ss->ss_type                       = legacy->scan_type;
    ss->ss_init_rest_time             = legacy->init_rest_time;
    ss->ss_min_dwell_active           = legacy->min_dwell_active;
    ss->ss_min_dwell_passive          = legacy->min_dwell_passive;

    /* Hardcode for DA */
    ss->ss_offchan_retry_delay        = HW_DEFAULT_OFFCHAN_RETRY_DELAY;
    ss->ss_exit_criteria_intval       = CHECK_EXIT_CRITERIA_INTVAL;
    ss->ss_min_beacon_count           = (avp->av_up) ? HW_DEFAULT_SCAN_MIN_BEACON_COUNT_INFRA : 0;
    ss->ss_beacon_timeout             = HW_DEFAULT_SCAN_BEACON_TIMEOUT;
    num_active_vaps                   = ath_get_all_active_vap_info(sc, ss->ss_vap_id);
    ss->ss_multiple_ports_active      = (num_active_vaps) ? true : false;
    ss->ss_restricted                 = false;
    ss->ss_termination_reason         = SCAN_REASON_NONE;
    ss->ss_max_offchan_retries        = HW_DEFAULT_MAX_OFFCHAN_RETRIES;

/*
 * Replace by new flag IEEE80211_SCAN_CHECK_TERMINATION in ss_flags, used for enh_ind_rpt only
 */
#if 0
    ss->ss_check_termination_function = params->check_termination_function;
    ss->ss_check_termination_context  = params->check_termination_context;
#endif

	ath_construct_chan_list(ss, params);

    spin_lock(&ss->ss_lock);
    if (ss->ss_info.si_scan_in_progress) {
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: Scan already in progress\n", __func__);
        spin_unlock(&ss->ss_lock);
        return EINPROGRESS;
    }

    if (ss->ss_num_chan_indices == 0) {
        /* Got empty list of channels. Cannot let scan starts. */
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: Could not construct channel list \n", __func__);
        spin_unlock(&ss->ss_lock);
        return EINVAL;
    }

    ss->ss_nssid = params->num_ssids;
    for (i = 0; i < params->num_ssids; ++i) {
        ss->ss_ssid[i].length = params->ssid[i].length;
        OS_MEMCPY(ss->ss_ssid[i].ssid, params->ssid[i].ssid, ss->ss_ssid[i].length);
    }

    ss->ss_nbssid = params->num_bssid;
    for (i = 0; i < params->num_bssid; ++i) {
        IEEE80211_ADDR_COPY(ss->ss_bssid[i], params->bssid_list[i].bytes);
        if (IEEE80211_IS_BROADCAST(ss->ss_bssid[i])) {
            bcast_bssid = true;
        }
    }
    if (params->scan_f_bcast_probe) {
        /* if there are no ssid add a null ssid */
        if (ss->ss_nssid == 0) {
            ss->ss_ssid[ss->ss_nssid].length = 0;
            ss->ss_ssid[ss->ss_nssid].ssid[0] = 0;
            ss->ss_nssid++;
        }
        if (!bcast_bssid && (ss->ss_nbssid < IEEE80211_SCAN_MAX_BSSID)) {
            u_int8_t broadcast_addr[IEEE80211_ADDR_LEN] = {0xff,0xff,0xff,0xff,0xff,0xff};
            IEEE80211_ADDR_COPY(ss->ss_bssid[ss->ss_nbssid],broadcast_addr);
            ss->ss_nbssid++;
        }
    }


    /*
     * If scan is being resumed after preemption, verify that number of
     * supported channels has not changed.
     */
    if (request_data->preemption_data.preempted) {
        if (request_data->preemption_data.nallchans != ss->ss_nallchans) {
            /* Total number of channels supported by HW changed. */
            spin_unlock(&ss->ss_lock);
            return EINVAL;
        }

        if (request_data->preemption_data.nchans != ss->ss_num_chan_indices) {
            /*
             * Number of channels in the channel list changed, most likely
             * because of changes in regulatory domain.
             */
            spin_unlock(&ss->ss_lock);
            return EINVAL;
        }

        if (request_data->preemption_data.next >= ss->ss_num_chan_indices) {
            /*
             * Channel from where to restart scan is past the end of the scan
             * list. One of the two previous tests should have failed, so
             * something went wrong.
             */
            spin_unlock(&ss->ss_lock);
            return EINVAL;
        }
    }

    /*
     * Prioritized scans should not wait for data traffic to cease before
     * going off-channel, so always make medium- and high-priority scans forced
     */
    if (ss->ss_priority >= SCAN_PRIORITY_MEDIUM) {
        ss->ss_flags |= SCAN_NOW;
    }


    /* Scan is now in progress */
    ss->ss_info.si_scan_in_progress       = true;
    ss->ss_scan_start_time                = request_data->request_timestamp;

    /*
     * If resuming a preempted scan, start from channel where previous scan
     * stopped.
     */
    if (request_data->preemption_data.preempted) {
        ss->ss_next = request_data->preemption_data.next;

        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: resume preempted scan next=%d/%d nallchans=%d\n",
            __func__,
            request_data->preemption_data.next,
            request_data->preemption_data.nchans,
            request_data->preemption_data.nallchans);
    }
    else {
        ss->ss_next = 0;

        scanner_clear_preemption_data(ss);
    }

    current_system_time = OS_GET_TIMESTAMP();

    ss->ss_athvap                 = avp;
    ss->ss_fakesleep_timeout      = FAKESLEEP_WAIT_TIMEOUT;
    ss->ss_visit_home_channel     = (ss->ss_multiple_ports_active ||
                                     (ss->ss_type == SCAN_TYPE_BACKGROUND) ||
                                     (ss->ss_type == SCAN_TYPE_SPECTRAL));
    ss->ss_offchan_retry_count    = 0;
    ss->ss_currscan_home_entry_time = OS_GET_TIMESTAMP();
    /*
     * Do not reset ss_enter_channel_time so that time spent in the home
     * channel between scans is not lost.
     */
    ss->ss_offchannel_time        = 0;
    ss->ss_run_counter++;

    {
        u_int32_t    now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(current_system_time);

        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%d.%03d | %s: Requestor=%d ID=%d flag=%08X type=%d elapsed=%d/%d multiple_ports=%d preempted=%d runctr=%d\n",
                              now / 1000, now % 1000, __func__,
                              ss->ss_requestor,
                              ss->ss_scan_id,
                              ss->ss_flags,
                              ss->ss_type,
                              (long) CONVERT_SYSTEM_TIME_TO_MS(current_system_time - ss->ss_scan_start_time),
                              ss->ss_max_scan_time,
                              ss->ss_multiple_ports_active,
                              request_data->preemption_data.preempted,
                              ss->ss_run_counter);
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "    nchans=%02d dwell=(%2d,%2d) (%2d,%2d) repeat_probe=%2d max_offchan_time=%d offchan_retry=(%03d,%02d) VAP=%08p\n",
                              ss->ss_num_chan_indices,
                              ss->ss_min_dwell_active,
                              ss->ss_max_dwell_active,
                              ss->ss_min_dwell_passive,
                              ss->ss_max_dwell_passive,
                              ss->ss_repeat_probe_time,
                              ss->ss_max_offchannel_time,
                              ss->ss_offchan_retry_delay,
                              ss->ss_max_offchan_retries,
                              ss->ss_athvap);
    }

    if(sc->sc_rx_ops->mops.wlan_mlme_register_vdev_event_handler)
        sc->sc_rx_ops->mops.wlan_mlme_register_vdev_event_handler(sc->sc_pdev, ss->ss_vap_id);

    if(sc->sc_rx_ops->mops.wlan_mlme_register_pm_event_handler)
        sc->sc_rx_ops->mops.wlan_mlme_register_pm_event_handler(sc->sc_pdev, ss->ss_vap_id);

    ieee80211_sm_dispatch(ss->ss_hsm_handle,
                          request_data->preemption_data.preempted ?
                              SCANNER_EVENT_RESTART_REQUEST : SCANNER_EVENT_SCAN_REQUEST,
                          0,
                          NULL);

    /*
     * Set maximum duration time after dispatching start/restart request event.
     * Even it the timer fires immediately (remaining time is 0), the timeout
     * event will be queued after the start/restart event, as it should.
     */
    if (ss->ss_max_scan_time != 0) {
        u_int32_t    elapsed_scan_time;
        u_int32_t    remaining_time;

        elapsed_scan_time = CONVERT_SYSTEM_TIME_TO_MS(current_system_time - ss->ss_scan_start_time);
        if (ss->ss_max_scan_time > elapsed_scan_time) {
            remaining_time = ss->ss_max_scan_time - elapsed_scan_time;
        }
        else {
            remaining_time = 0;
        }

        OS_SET_TIMER(&ss->ss_maxscan_timer, remaining_time);
    }

    /* Save home channel to switch */
    OS_MEMCPY(&(ss->ss_athvap->av_bsschan), &(ss->ss_sc->sc_curchan), sizeof(HAL_CHANNEL));

    spin_unlock(&ss->ss_lock);
    return EOK;
}


int ath_scan_stop(ath_scanner_t ss, struct scan_cancel_param *params, scanner_stop_mode stop_mode)
{
    enum scanner_event    event;
    u_int32_t             now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    u_int32_t             initial_run_counter = ss->ss_run_counter;

    spin_lock(&ss->ss_lock);

    initial_run_counter = ss->ss_run_counter;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%d.%03d | %s: module %d ID=%d (curr_id=%d) state=%s in_progress=%d mode=%d runctr=%d\n",
                          now / 1000, now % 1000, __func__,
                          ss->ss_requestor,
                          params->scan_id,
                          ss->ss_scan_id,
                          ieee80211_sm_get_current_state_name(ss->ss_hsm_handle),
                          ss->ss_info.si_scan_in_progress,
                          stop_mode,
                          ss->ss_run_counter);

    if (!ss->ss_info.si_scan_in_progress) {
        spin_unlock(&ss->ss_lock);
        return EPERM;
    }

    /*
     * Verify that scan currently running is the one we want to cancel.
     */
    if ((params->scan_id != 0) && (ss->ss_scan_id != params->scan_id)) {
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: module %d Incorrect scan_id. Requested ID=%d Current ID=%d\n",
                              __func__,
                              params->requester,
                              params->scan_id,
                              ss->ss_scan_id);

#if ATH_SUPPORT_MULTIPLE_SCANS
        spin_unlock(&ss->ss_lock);
        return EINVAL;
#endif
    }

    /* Select event to be sent to the state machine */
    event = (stop_mode == STOP_MODE_PREEMPT) ?
        SCANNER_EVENT_PREEMPT_REQUEST : SCANNER_EVENT_CANCEL_REQUEST;

    spin_unlock(&ss->ss_lock);

    ieee80211_sm_dispatch(ss->ss_hsm_handle,
                              event,
                              0,
                              NULL);

    return EOK;
}

int ath_scan_sm_start(ath_dev_t dev, void *req)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_scanner_t ss = NULL;
    scan_request_data_t    request_data;
    struct scan_start_request *request = (struct scan_start_request*)req;
    struct scan_req_params *params = &request->scan_req;


    if(!sc)
    {
        qdf_print("%s %d sc is NULL ",__func__,__LINE__);
        return -EINVAL;
    }

    ss = sc->sc_scanner;
    if(!ss)
    {
        qdf_print("%s %d ss is NULL ",__func__,__LINE__);
        return -EINVAL;
    }

#if ATH_SUPPORT_MULTIPLE_SCANS
    return ath_scan_scheduler_add(ss->ss_scheduler,
                                        request);
#else

    /* Clear request data. */
    OS_MEMZERO(&request_data, sizeof(request_data));

    request_data.requestor                 = params->scan_req_id;
    request_data.priority                  = params->scan_priority;
    request_data.scan_id                   = params->scan_id;
    request_data.request_timestamp         = OS_GET_TIMESTAMP();
    request_data.req                       = request;
    request_data.preemption_data.preempted = false;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: Starting scan...\n", __func__);

    return ath_scan_run(sc, &request_data);

#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */
}
int ath_scan_cancel(ath_dev_t  dev, void *req)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_scanner_t ss = NULL;
    struct scan_cancel_param *params = (struct scan_cancel_param*)req;

    if(!sc)
        return -EINVAL;

    ss = sc->sc_scanner;
    if(!ss)
        return -EINVAL;

#if ATH_SUPPORT_MULTIPLE_SCANS
    return ath_scan_scheduler_remove(ss->ss_scheduler, params);
#else
    /*
     * Multiple scans not supported.
     * Don't forget to extract the actual scan_id from the mask.
     * Function ieee80211_scan_stop will verify that the scan_id is 0, so
     * leaving higher bytes present in the mask causes the comparison to fail.
     */
    return ath_scan_stop(ss, params, STOP_MODE_CANCEL);
#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */
}

static bool is_radio_measurement_in_home_channel(ath_scanner_t ss)
{
    if (avp_is_connected(ss->ss_athvap)) {
        return (ss->ss_all_chans[ss->ss_scan_chan_indices[ss->ss_next]].mhz == ss->ss_athvap->av_bsschan.channel);
    }

    return false;
}

/*
 * Send probe requests.
 *
 * This function may be called from any IRQL, so do not acquire scan's
 * synchronization object, which is valid only at IRQL <= APC_LEVEL
 * However, we still need to synchronize access to shared data.
 */
static void scanner_send_probereq(ath_scanner_t ss)
{
    int    i, j;

    for (i = 0; i < ss->ss_nssid; i++) {
        for (j = 0; j < ss->ss_nbssid; j++) {
            if(ss->ss_sc->sc_rx_ops->mops.wlan_mlme_send_probe_request)
                ss->ss_sc->sc_rx_ops->mops.wlan_mlme_send_probe_request(ss->ss_sc->sc_pdev,
                                                                ss->ss_vap_id,
                                                                ss->ss_bssid[j],
                                                                ss->ss_bssid[j],
                                                                ss->ss_ssid[i].ssid,
                                                                ss->ss_ssid[i].length,
                                                                ss->ss_ie.ptr,
                                                                ss->ss_ie.len);
        }
    }
}

/*
 * This function must be called only from idle state.
 */
static void scanner_initiate_radio_measurement(ath_scanner_t ss)
{
    if (! avp_is_connected(ss->ss_athvap)) {
        /*
         * Pause request is not expected when we are not connected and so we do not need to process the request.
         * Post the event saying cancelled the pause request.
         */
        scanner_post_event(ss,
                           SCAN_EVENT_TYPE_RADIO_MEASUREMENT_START,
                           SCAN_REASON_CANCELLED);
        return;
    }

    /*
     * If connected we may have to suspend or resume traffic
     * It is guaranteed here that we are in BSSCHANNEL.
     */
    if (is_radio_measurement_in_home_channel(ss)) {
        ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_RADIO_MEASUREMENT);
    }
    else {
        /*
         * We have already verified we're connected, so we need to suspend
         * traffic.
         */
        if (scanner_can_leave_home_channel(ss)) {
            /* Flush any messages from messgae queue and reset HSM to IDLE state. */
            scanner_leave_bss_channel(ss);
        } else {
            /* Not able to process Radio measurement request now, so transition to BSS Channel state to process it later */
            ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_BSS_CHANNEL);
        }
    }
}

static void
scanner_completion_sequence(ath_scanner_t              ss,
                            struct ath_vap             *notifying_vap,
                            enum scan_event_type        event,
                            enum scan_completion_reason reason)
{
    /*
     * Set si_scan_in_progress to false before scan completion notifications
     * are sent. This covers the case in which the notified modules (including
     * the OS) send another scan request as soon as the completion is
     * received, possibly while scanner is still notifying other modules.
     *
     * ss_completion_indications_pending is set to false only after all
     * indications have been sent, and serves to synchronize scan cancel
     * routine.
     */
    if (ss->ss_info.si_scan_in_progress) {
        ss->ss_info.si_scan_in_progress       = false;
        scanner_post_event(ss,
                           event,
                           reason);
        /*
         * The following fields are subject to race conditions.
         * It is possible that clients being notified on scan completion may
         * immediately request another scan, and any values set by
         * ieee80211_scan_run will be overwritten here.
         */
        ss->ss_scan_start_event_posted        = false;
        ss->ss_completion_indications_pending = false;
    }
}

#if ATH_TX_DUTY_CYCLE
int ath_scan_enable_tx_duty_cycle(struct ath_softc *sc)
{
    int status = 0;
    int active_pct = sc->sc_tx_dc_active_pct;

    DPRINTF(scn, ATH_DEBUG_ANY, "%s: pct=%d\n", __func__, active_pct);

    /* use 100% as shorthand to mean tx dc off */
    if (active_pct == 100) {
        return ath_set_tx_duty_cycle(sc, active_pct, false);
    }

    if (active_pct < 20 /*|| active_pct > 80*/) {
        status = -EINVAL;
    } else {
        /* enable tx dc */
        return ath_set_tx_duty_cycle(sc, active_pct, true);
    }

    return status;
}
#endif

static void scanner_terminate(ath_scanner_t ss, enum scan_completion_reason reason)
{
    struct ath_softc *sc  = ss->ss_sc;
    u_int32_t    now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%d.%03d | %s: scan_id=%d reason=%s next_chan=%d/%d nallchans=%d\n",
        now / 1000, now % 1000, __func__,
        ss->ss_scan_id,
        ath_scan_notification_reason_name(reason),
        ss->ss_next,
        ss->ss_num_chan_indices,
        ss->ss_nallchans);

    if (reason == SCAN_REASON_TERMINATION_FUNCTION) {
       ss->ss_info.si_last_term_status =  true;
    } else {
       ss->ss_info.si_last_term_status = false;
    }

    /*
     * Save termination reason if scan is in progress and one has not been set yet.
     */
    if (ss->ss_info.si_scan_in_progress) {
        OS_CANCEL_TIMER(&ss->ss_maxscan_timer);

        if (ss->ss_termination_reason == SCAN_REASON_NONE) {
            ss->ss_termination_reason = reason;

            /*
             * Save timestamp of last scan in which all channels were scanned.
             * Consider scan completed if repeater scan since only channel to be scanned
             */
            if (reason == SCAN_REASON_PREEMPTED) {
                scanner_save_preemption_data(ss);
            }
            else {
                if (ss->ss_next >= ss->ss_num_chan_indices) {
                    ss->ss_last_full_scan_time = ss->ss_scan_start_time;
                }
            }
        }
    }

    OS_CANCEL_TIMER(&ss->ss_timer);

    /*
     * If received an error from the ResourceManager, abort scan and go
     * straight to IDLE state.
     */
    if (reason == SCAN_REASON_INTERNAL_FAILURE) {
        ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_IDLE);
    } else {
    if ((! ss->ss_info.si_in_home_channel) || (ss->ss_suspending_traffic)) {
        int    status;

            /*
             * We should always call set_channel even if ic_curchan == ic_bsschan, which happens when
             * last foreign channel is the home channel, so that beacon timer can be synchronized and
             * beacon can be re-started if we are in ad-hoc mode.
             */

            status = sc->sc_rx_ops->mops.wlan_mlme_resmgr_request_bsschan(sc->sc_pdev);

            switch (status) {
            case EBUSY:
                /*
                 * Resource Manager is responsible for resuming traffic, changing
                 * channel and sending notification once the operation has completed.
                 */
                ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_TERMINATING);

            /* Set watchdog timer in case ResMgr doesn't send back an event */
            ath_schedule_timed_event(ss, RESMGR_WAIT_TIMEOUT, SCANNER_EVENT_TERMINATING_TIMEOUT);
            break;

        case EOK:    /* Resource Manager not active. */
        case EINVAL: /* Resource Manager doesn't have BSS channel information (no VAPs active) */
        default:     /* Resource Manager not active. */
            sc->sc_rx_ops->mops.wlan_mlme_end_scan(sc->sc_pdev);
#if ATH_SUPPORT_VOW_DCS
                if (!sc->sc_rx_ops->mops.wlan_mlme_get_cw_inter_found(sc->sc_pdev))
#endif
                {
                    sc->sc_rx_ops->mops.wlan_mlme_set_home_channel(sc->sc_pdev, ss->ss_vap_id);
                }
#if ATH_VAP_PAUSE_SUPPORT
                if (sc->sc_rx_ops->mops.wlan_mlme_get_enh_rpt_ind(sc->sc_pdev)) {
                    sc->sc_rx_ops->mops.wlan_mlme_unpause(sc->sc_pdev);
                    sc->sc_rx_ops->mops.wlan_mlme_vdev_pause_control(sc->sc_pdev, ss->ss_vap_id);
                }
#endif

#if ATH_SLOW_ANT_DIV
                ath_slow_ant_div_resume(sc);
#endif
                ath_scan_enable_txq(sc);
#if ATH_SUPPORT_VOW_DCS
            if (!sc->sc_rx_ops->mops.wlan_mlme_get_cw_inter_found(sc->sc_pdev))
#endif
            {
                if (avp_is_connected(ss->ss_athvap)) {
                    if (sc->sc_rx_ops->mops.wlan_mlme_sta_power_unpause(sc->sc_pdev, ss->ss_vap_id) != EOK) {
                        /*
                         * No need for a sophisticated error handling here.
                         * If transmission of fake wakeup fails, subsequent traffic sent
                         * with PM bit clear will have the same effect.
                         */
                        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s failed to send fakewakeup\n",
                                    __func__);
                    }
                }
              }

                ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_IDLE);
                break;
            }
        }
        else {
            ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_IDLE);
        }
    }
}

/*
 * spin lock already held
 */
static bool scanner_check_endof_scan(ath_scanner_t ss)
{
    if (ss->ss_type == SCAN_TYPE_REPEATER_BACKGROUND) {
        ss->ss_last_full_scan_time = ss->ss_scan_start_time;
        return true;
    }

    if (ss->ss_next >= ss->ss_num_chan_indices) {
        if (ss->ss_flags & SCAN_CONTINUOUS) {
            /* Check whether all channels were scanned */
            if ((ss->ss_next >= ss->ss_nallchans) ||
                (((ss->ss_flags & (SCAN_ALLBANDS)) != SCAN_ALLBANDS))) {
                ss->ss_last_full_scan_time = ss->ss_scan_start_time;
            }

            /*
             * Continuous scanning: indicate that scan restarted, and all
             * channels were scanned.
             */
            scanner_post_event(ss,
                               SCAN_EVENT_TYPE_RESTARTED,
                               SCAN_REASON_NONE);

            ss->ss_next = 0;

            return false;
        } else {
            return true;
        }

    } else {
        return false;
    }
}

static bool scanner_can_leave_foreign_channel(ath_scanner_t ss)
{
    systime_t    current_time = OS_GET_TIMESTAMP();
    u_int32_t    elapsed_dwell_time;

    elapsed_dwell_time = CONVERT_SYSTEM_TIME_TO_MS(current_time - ss->ss_enter_channel_time);

    if (elapsed_dwell_time >= ss->ss_cur_min_dwell) {
        return true;
    } else {
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN,
            "%s: FALSE: elapsed_dwell_time=%d current_time=%lu enter_channel_time=%lu\n",
            __func__,
            elapsed_dwell_time,
            (unsigned long)CONVERT_SYSTEM_TIME_TO_MS(current_time),
            (unsigned long)CONVERT_SYSTEM_TIME_TO_MS(ss->ss_enter_channel_time));

        return false;
    }
}

static bool scanner_can_leave_home_channel(ath_scanner_t ss)
{
    bool         beacon_requirement;
    systime_t    last_data_time;
    systime_t    current_system_time;
    u_int32_t    elapsed_rest_time;
    struct ath_softc *sc  = ss->ss_sc;
    u_int32_t qdepth_sum;

    /* In the case of enh -ind repeater, we need to wait a
     * initial 14 secs in the home channel.
     * handle that - in the case of non enh ind rpt .. the value is
     * defaulted to 0
     */
#if 0
    if (sc->sc_rx_ops->mops.wlan_mlme_get_enh_rpt_ind(sc->sc_pdev) &&
        ( ss->ss_info.si_last_term_status == 0) && (ss->ss_flags & SCAN_TERMINATE_ON_BSSID_MATCH)) {
        if(sc->sc_rx_ops->mops.scanner_check_for_bssid_entry(sc->sc_pdev, ss->ss_vap_id)) {
            return true;
        }
    }
#endif

    /* Retrieve current system time */
    current_system_time = OS_GET_TIMESTAMP();

    /* Calculate current rest time. */
    elapsed_rest_time = CONVERT_SYSTEM_TIME_TO_MS(current_system_time - ss->ss_currscan_home_entry_time);

    if (elapsed_rest_time < ss->ss_init_rest_time) {
        return false;
    }

    /*
     * Check whether connection was lost.
     */
    if (! avp_is_connected(ss->ss_athvap)) {
        ath_scan_connection_lost((ath_dev_t)ss->ss_sc);

        /*
         * Can leave home channel if no connection and not forced to meet
         * minimum rest time.
         */
        if (! ss->ss_visit_home_channel) {
            return true;
        }
    }

    /* Retrieve current system time */
    current_system_time = OS_GET_TIMESTAMP();

    /* Calculate current rest time. */
    elapsed_rest_time = CONVERT_SYSTEM_TIME_TO_MS(current_system_time - ss->ss_enter_channel_time);

    /*
     * Minimum rest time has been met.
     * Can leave home channel if station is not connected and no other ports
     * are active. Otherwise, check remaining conditions.
     */
    if ((! avp_is_connected(ss->ss_athvap)) && (! ss->ss_multiple_ports_active)) {
        return true;
    }

    /*
     * Make sure we stay in the home channel for a minimum amount of time.
     * This includes time spent in the home channel when scan is not running.
     */
    if (elapsed_rest_time < ss->ss_min_rest_time) {
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN,
            "%s: Under min. rest time. Beacon=%lu Dwell=%lu/%lu Done=0\n",
            __func__,
            ss->ss_beacons_received,
            elapsed_rest_time,
            ss->ss_min_rest_time);

        return false;
    }

    /*
     * Minimum rest time already satisfied; can leave home channel if radio measurement
     * has been requested.
     */
    if (ss->ss_type == SCAN_TYPE_RADIO_MEASUREMENTS) {
        ss->ss_info.si_allow_transmit = false;
        return true;
    }

    /*
     * If performing a forced scan, do not exceed the maximum amount of time
     * we can spend on the home channel. This time limit trumps all other
     * conditions when deciding whether to leave the home channel.
     * There's no time limit when performing a non-forced scan.
     */
    if ((ss->ss_flags & (SCAN_FORCED | SCAN_NOW)) &&
        (elapsed_rest_time >= ss->ss_max_rest_time)) {
#if ATH_BT_COEX
        /*
         * If it's a external scan request and BT is busy, we check the other leave home
         * channel condition before return ok.
         */
        if (!((ss->ss_flags & SCAN_EXTERNAL) &&
            ath_get_bt_coex_info(sc, IEEE80211_BT_COEX_INFO_BTBUSY, NULL );
            return true;
        }
#else
        return true;
#endif
    }

    /*
     * Don't leave home channel if there is still data waiting to be
     * transmitted .
     */
    qdepth_sum = ath_all_txqs_depth(sc);
    if (qdepth_sum) {
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s:  Qdepth=%d \n",
            __func__,
            qdepth_sum);

        return false;
    }

    /*
     * Traffic indication timestamp can be modified asynchronously during
     * execution of this routine. Therefore, in order to ensure temporality,
     * we must save a copy of it before querying the current system time.
     */
    last_data_time      = sc->sc_rx_ops->mops.wlan_mlme_get_traffic_indication_timestamp(sc->sc_pdev);
    current_system_time = OS_GET_TIMESTAMP();

    /*
     * Check channel activity; ready to leave channel only if no traffic
     * for a certain amount of time.
     */
    if (CONVERT_SYSTEM_TIME_TO_MS(current_system_time - last_data_time) < ss->ss_idle_time) {
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: Channel has activity. Beacon=%lu Data (now=%lu last=%lu) Done=0\n",
            __func__,
            ss->ss_beacons_received,
            (unsigned long) CONVERT_SYSTEM_TIME_TO_MS(current_system_time),
            (unsigned long) CONVERT_SYSTEM_TIME_TO_MS(last_data_time));

        return false;
    }

    /*
     * Minimum dwell time and network idle requirements met.
     * Verify whether we have received the required number of beacons.
     */
    beacon_requirement = (ss->ss_beacons_received >= ss->ss_min_beacon_count);

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: Rest=%lu Timeout (MinRest=%lu Beacon=%lu) BeaconCnt=%lu Data (now=%lu last=%lu) Qdepth=%d Done=%d\n",
        __func__,
        (unsigned long) CONVERT_SYSTEM_TIME_TO_MS(elapsed_rest_time),
        ss->ss_min_rest_time,
        ss->ss_beacon_timeout,
        ss->ss_beacons_received,
        (unsigned long) CONVERT_SYSTEM_TIME_TO_MS(current_system_time),
        (unsigned long) CONVERT_SYSTEM_TIME_TO_MS(last_data_time),
        qdepth_sum,
        (beacon_requirement || (elapsed_rest_time >= ss->ss_beacon_timeout)));

    /*
     * Leave home channel if satisfied requirements for minimum dwell time,
     * absence of data traffic and beacon statistics. The latter means we
     * either
     *     a) received the required number of beacons, or
     *     b) reached a time limit without receiving the required beacons
     */
    if (beacon_requirement || (elapsed_rest_time >= ss->ss_beacon_timeout)) {
        return true;
    }

    return false;
}

static bool is_common_event(ath_scanner_t ss,
                            enum scanner_event  event,
                            u_int16_t           event_data_len,
                            void                *event_data)
{
    bool                                common_event;
    enum scan_completion_reason              termination_reason;
    bool                                synchronous_termination;

    /* External events reported regardless of the scanner's state */
    if (event == SCANNER_EVENT_EXTERNAL_EVENT) {
        struct scan_event    *scan_event = (struct scan_event *)event_data;

        ASSERT(scan_event);

        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s External Event %s reason %s scan_id=%d requestor=%08X vap_id=%d\n",
            __func__,
            ath_scan_notification_event_name(scan_event->type),
            ath_scan_notification_reason_name(scan_event->reason),
            scan_event->scan_id,
            scan_event->requester,
            scan_event->vdev_id);

        scanner_post_event(ss,
                           scan_event->type,
                           scan_event->reason);

        /* pass-through event; report as already handled */
        return true;
    }

    common_event            = true;
    termination_reason      = SCAN_REASON_NONE;
    synchronous_termination = false;

    /*
     * Check termination conditions
     */
    switch (event) {
    case SCANNER_EVENT_CANCEL_REQUEST:
    case SCANNER_EVENT_PREEMPT_REQUEST:
        {
#if 0
            if (event_data_len > 0) {
                struct ieee80211_scan_cancel_data    *scan_cancel_data = event_data;

            ASSERT(scan_cancel_data);
                synchronous_termination =
                    ((scan_cancel_data->flags & IEEE80211_SCAN_CANCEL_SYNC) != 0);
            }
#endif
            /*
             * Select event to be sent to the state machine and the flag to be set
             * depending on whether we want to cancel or preempt scan.
             */
            termination_reason = (event == SCANNER_EVENT_CANCEL_REQUEST) ?
                SCAN_REASON_CANCELLED : SCAN_REASON_PREEMPTED;
        }
        break;

    case SCANNER_EVENT_MAXSCANTIME_EXPIRE:
        termination_reason = SCAN_REASON_TIMEDOUT;
        break;

    case SCANNER_EVENT_FATAL_ERROR:
        /* Stop scan immediately */
        termination_reason = SCAN_REASON_INTERNAL_FAILURE;
        break;

    default:
        /* not a common event */
        common_event = false;
        break;
    }

    if (common_event) {
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s Common Event Scan state=%s event=%s\n",
            __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

        /*
         * Check if scan must be terminated because of the received event
         */
        if (termination_reason != SCAN_REASON_NONE) {
            bool    event_ignored = true;

            /*
             * Ignore termination conditions if scan not in progress
             */
            if (ss->ss_info.si_scan_in_progress) {
                /*
                 * If scan_start event (IEEE80211_SCAN_STARTED or IEEE80211_SCAN_RESTARTED)
                 * has not been posted yet, only synchronous cancellation
                 * requests are valid after ieee80211_scan_run is called (flag
                 * si_scan_in_progress is * set) but before scan_start event is
                 * posted (flag ss_scan_start_event_posted is false).
                 *
                 * The asynchronous cancellation requests are queued and
                 * handled in the order received. As a result, a cancellation
                 * request received before scan_start event is a leftover from
                 * cancellation of a previous scan and must be ignored
                 */
                if (synchronous_termination || ss->ss_scan_start_event_posted) {
                    scanner_terminate(ss, termination_reason);
                    event_ignored = false;
                }
            }

            if (event_ignored) {
                ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s event=%s IGNORED in_progress=%d start_posted=%d\n",
                    __func__,
                    ath_scan_event_name(event),
                    ss->ss_info.si_scan_in_progress,
                    ss->ss_scan_start_event_posted);
            }
        }
    }

    /*
     * Report whether received event was common.
     * It's valid to indicate a common event was received and processed even
     * if no action was taken (like ignoring termination conditions if no scan
     * is in progress).
     */
    return common_event;
}

static u_int32_t ath_phymode2flags(struct channel_param *ch)
{
    u_int32_t flags;
    static const u_int32_t modeflags[] = {
        IEEE80211_CHAN_A,	        /*WMI_HOST_MODE_11A	*/
        IEEE80211_CHAN_G,	        /*WMI_HOST_MODE_11G	*/
        IEEE80211_CHAN_B,	        /*WMI_HOST_MODE_11B	*/
        IEEE80211_CHAN_PUREG,	    /*WMI_HOST_MODE_11GONLY*/
        IEEE80211_CHAN_11NA_HT20,	/*WMI_HOST_MODE_11NA_HT20*/
        IEEE80211_CHAN_11NG_HT20,	/*WMI_HOST_MODE_11NG_HT20*/
        0,                          /*WMI_HOST_MODE_11NA_HT40 */
        0,                          /*WMI_HOST_MODE_11NG_HT40 */
        0,                          /*WMI_HOST_MODE_11AC_VHT20*/
        0,                          /*WMI_HOST_MODE_11AC_VHT40*/
        IEEE80211_CHAN_11AC_VHT80,	/*WMI_HOST_MODE_11AC_VHT80 */
        0,                          /*WMI_HOST_MODE_11AC_VHT20_2G*/
        0,                          /*WMI_HOST_MODE_11AC_VHT40_2G*/
        0,                          /*WMI_HOST_MODE_11AC_VHT80_2G*/
        0,                          /*WMI_HOST_MODE_11AC_VHT80_80*/
        0,                          /*WMI_HOST_MODE_11AC_VHT160*/
    };

    flags = modeflags[ch->phy_mode];

    if (ch->half_rate) {
        flags |= IEEE80211_CHAN_HALF;
    } else if (ch->quarter_rate) {
        flags |= IEEE80211_CHAN_QUARTER;
    }

    return flags;
}
static bool scanner_leave_bss_channel(ath_scanner_t ss)
{
    int          status;
    u_int32_t    estimated_offchannel_time;
    struct ath_softc *sc = ss->ss_sc;

    /*
     * Estimate time to be spent off-channel.
     * If burst scan is enabled, this estimated time is the maximum between
     * the next dwell time and ss_offchannel_time.
     */
    if (is_passive_channel(&ss->ss_all_chans[ss->ss_scan_chan_indices[ss->ss_next]], ss->ss_flags)) {
        estimated_offchannel_time = ss->ss_max_dwell_passive;
    }
    else {
        estimated_offchannel_time = ss->ss_max_dwell_active;
    }

    /* P2P - deprecated */
#if 0
    if ((ss->ss_flags & SCAN_BURST) &&
        (ss->ss_max_offchannel_time > 0)) {

        if (ss->ss_offchannel_time > estimated_offchannel_time) {
            estimated_offchannel_time = ss->ss_offchannel_time;
        }
    }
#endif

    /*
     * Conditions necessary to leave BSS channel have already been met, so
     * there's no need for the ResourceManager for the same conditions again.
     */
    status = sc->sc_rx_ops->mops.wlan_mlme_resmgr_request_offchan(sc->sc_pdev,
                                                     ss->ss_all_chans[ss->ss_scan_chan_indices[ss->ss_next]].mhz,
                                                     ath_phymode2flags(&ss->ss_all_chans[ss->ss_scan_chan_indices[ss->ss_next]]),
                                                     estimated_offchannel_time);

    switch (status) {
    case EOK:
        /*
         * Resource Manager not active.
         * Scanner needs to suspend traffic if client is connected.
         */
        if (avp_is_connected(ss->ss_athvap)) {
            ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_SUSPENDING_TRAFFIC);
        }
        else {
            ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_FOREIGN_CHANNEL);
        }
        break;

    case EBUSY:
        /*
         * Resource Manager is responsible for suspending traffic, changing
         * channel and sending notification once the operation has completed.
         */
        ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_SUSPENDING_TRAFFIC);
        break;

    default:
        /*
         * Error.
         * Scanner needs to suspend traffic if client is connected.
         */
        if (avp_is_connected(ss->ss_athvap)) {
            ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_SUSPENDING_TRAFFIC);
        }
        else {
            ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_FOREIGN_CHANNEL);
        }
        break;
    }

    return true;
}

static void scanner_state_idle_entry(void *context)
{
    ath_scanner_t                 ss = context;
    u_int32_t                     now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    enum scan_event_type          notification_event;
    enum scan_completion_reason   notification_reason;
    struct ath_vap                *notifying_vap;
    bool                          scan_preempted;
    struct ath_softc              *sc = ss->ss_sc;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%d.%03d | %s all_channels_scanned=%d\n",
        now / 1000, now % 1000, __func__,
        scanner_check_endof_scan(ss));

    if (ss->ss_athvap != NULL) {
        sc->sc_rx_ops->mops.wlan_mlme_unregister_vdev_event_handler(sc->sc_pdev, ss->ss_vap_id);

        if (! sc->sc_rx_ops->mops.wlan_mlme_resmgr_active(sc->sc_pdev)) {
            sc->sc_rx_ops->mops.wlan_mlme_unregister_pm_event_handler(sc->sc_pdev, ss->ss_vap_id);
        }
    }

    /*
     * Save the current VAP *before* sending notifications to avoid a race
     * condition in which another module requests a scan during the
     * notification. The resulting call to ieee80211_scan_run overwrites
     * ss->ss_athvap while it is still being used (error #1). If ss_athvap is then
     * cleared after all notifications, it overwrites the new value specified
     * in the call to ieee80211_scan_run (error #2).
     */
    notifying_vap  = ss->ss_athvap;
    scan_preempted = (ss->ss_termination_reason == SCAN_REASON_PREEMPTED);

    OS_CANCEL_TIMER(&ss->ss_timer);
    OS_CANCEL_TIMER(&ss->ss_maxscan_timer);

    ss->ss_pending_event           = SCANNER_EVENT_NONE;
    ss->ss_suspending_traffic      = false;
    ss->ss_athvap                     = NULL;
    atomic_set(&(ss->ss_wait_for_beacon),     false);
    atomic_set(&(ss->ss_beacon_event_queued), false);

    ss->ss_info.si_allow_transmit  = true;
    ss->ss_info.si_in_home_channel = true;

    /*
     * Execute completion sequence (send notifications and clear flags).
     * Send special notification event if scan has been preempted (i.e. no
     * longer active, but will continue later because there's still work to do).
     */
    if (scan_preempted) {
        notification_event  = SCAN_EVENT_TYPE_PREEMPTED;
        notification_reason = SCAN_REASON_PREEMPTED;
    }
    else {
        /*
         * Scanner returning to IDLE state before scan_start event is posted
         * indicates a synchronous cancellation.
         *
         * If ATH_SUPPORT_MULTIPLE_SCANS is enabled, post
         * IEEE80211_SCAN_DEQUEUED since other modules never received
         * IEEE80211_SCAN_STARTED or IEEE80211_SCAN_RESTARTED.
         *
         * If ATH_SUPPORT_MULTIPLE_SCANS is disabled, post
         * IEEE80211_SCAN_COMPLETED since the shim layers for platforms that
         * do not support multiple scans have not been modified to handle
         * IEEE80211_SCAN_DEQUEUED, IEEE80211_SCAN_PREEMPTED, etc.
         */
#if ATH_SUPPORT_MULTIPLE_SCANS
        notification_event = ss->ss_scan_start_event_posted ?
		SCAN_EVENT_TYPE_COMPLETED: SCAN_EVENT_TYPE_PREEMPTED;
#else
        notification_event = SCAN_EVENT_TYPE_COMPLETED;
#endif
        notification_reason = ss->ss_termination_reason;
    }

    scanner_completion_sequence(ss, notifying_vap, notification_event, notification_reason);
}

static bool scanner_state_idle_event(void      *context,
                                     u_int16_t event,
                                     u_int16_t event_data_len,
                                     void      *event_data)
{
    ath_scanner_t                 ss = context;
    enum scan_event_type           notification_event;
    enum scan_completion_reason    notification_reason;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s\n",
        __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

    if (is_common_event(ss, event, event_data_len, event_data)) {
        return true;
    }

    switch (event) {
    case SCANNER_EVENT_SCAN_REQUEST:
    case SCANNER_EVENT_RESTART_REQUEST:
        if (! ss->ss_info.si_scan_in_progress) {
            ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s nothing to do\n",
                __func__,
                ieee80211_sm_get_current_state_name(ss->ss_hsm_handle),
                ath_scan_event_name(event));

            return true;
        }

        if (event == SCANNER_EVENT_SCAN_REQUEST) {
            notification_event  = SCAN_EVENT_TYPE_STARTED;
            notification_reason = SCAN_REASON_NONE;
        }
        else {
            notification_event  = SCAN_EVENT_TYPE_RESTARTED;
            notification_reason = SCAN_REASON_PREEMPTED;
        }
        scanner_post_event(ss,
                           notification_event,
                           notification_reason);
        /*
         * Scan started and termination indications have not been sent yet.
         */
        ss->ss_scan_start_event_posted        = true;
        ss->ss_completion_indications_pending = true;

        if(ss->ss_type == SCAN_TYPE_RADIO_MEASUREMENTS){
            scanner_initiate_radio_measurement(ss);
        }
        else {
            if (ss->ss_visit_home_channel) {
                if (scanner_can_leave_home_channel(ss)) {
                    scanner_leave_bss_channel(ss);
                }
                else {
                    ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_BSS_CHANNEL);
                }
            }
            else {
                scanner_leave_bss_channel(ss);
            }
        }
        break;

    default:
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s in_progress=%d not handled\n",
            __func__,
            ieee80211_sm_get_current_state_name(ss->ss_hsm_handle),
            ath_scan_event_name(event),
            ss->ss_info.si_scan_in_progress);

        return false;
    }

    return true;
}

static void scanner_state_suspending_traffic_entry(void *context)
{
    ath_scanner_t          ss = context;
    int                    status;
    u_int32_t              now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    struct ath_softc       *sc = ss->ss_sc;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%d.%03d | %s\n",
        now / 1000, now % 1000, __func__);

    ss->ss_info.si_allow_transmit = false;

    /*
     * If resources manager is performing traffic management, wait for it to
     * send us a  notification that we have transitioned off channel.
     */

    if (! sc->sc_rx_ops->mops.wlan_mlme_resmgr_active(sc->sc_pdev)) {
        sc->sc_rx_ops->mops.wlan_mlme_set_vdev_sleep(sc->sc_pdev, ss->ss_vap_id);

        status = sc->sc_rx_ops->mops.wlan_mlme_sta_power_pause(sc->sc_pdev, ss->ss_vap_id,  ss->ss_fakesleep_timeout);
        if (status != EOK)
        {
            ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s fakesleep ignored \n", __func__);
            /* switch to foreign channel immediately */
            ath_schedule_timed_event(ss, 0, SCANNER_EVENT_FAKESLEEP_EXPIRE);
        }
#if UMAC_SUPPORT_VAP_PAUSE
        if (sc->sc_rx_ops->mops.wlan_mlme_get_enh_rpt_ind(sc->sc_pdev)) {
          /* call vap-pause -- stop data and beacons if needed */
        if (sc->sc_rx_ops->mops.wlan_mlme_pause(sc->sc_pdev) == -1)
            ath_schedule_timed_event(ss, IEEE80211_CAN_PAUSE_WAIT_DELAY , SCANNER_EVENT_CAN_PAUSE_WAIT_EXPIRE);
        }
#endif
    }

    /*
     * Remember that we're entering suspending traffic state.
     * This allows the termination routine to resume VAP operation if scan was
     * cancelled while suspending state (or while in a foreign channel.)
     */
    ss->ss_suspending_traffic = true;
}

static bool scanner_state_suspending_traffic_event(void      *context,
                                                   u_int16_t event,
                                                   u_int16_t event_data_len,
                                                   void      *event_data)
{
    ath_scanner_t    ss = context;
    struct ath_softc *sc = ss->ss_sc;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s\n",
        __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

    if (is_common_event(ss, event, event_data_len, event_data)) {
        return true;
    }

    switch (event) {
    case SCANNER_EVENT_FAKESLEEP_EXPIRE:
    case SCANNER_EVENT_FAKESLEEP_FAILED:
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s error sending fakesleep event=%s\n",
            __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

        ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_FOREIGN_CHANNEL);
        break;

    case SCANNER_EVENT_FAKESLEEP_ENTERED:
        ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_FOREIGN_CHANNEL);
        break;

    case SCANNER_EVENT_OFFCHAN_SWITCH_COMPLETE:
        if (ss->ss_type == SCAN_TYPE_RADIO_MEASUREMENTS) {
            ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_RADIO_MEASUREMENT);
        } else {
            ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_FOREIGN_CHANNEL);
        }
        break;

    case SCANNER_EVENT_OFFCHAN_SWITCH_FAILED:
    case SCANNER_EVENT_OFFCHAN_SWITCH_ABORTED:
        /*
         * Off-channel request failed.
         * Cancel scan if we have already exceeded the maximum number of
         * retries. Otherwise wait some time and retry.
         */
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s retries=%02d/%02d\n",
            __func__, ss->ss_offchan_retry_count, ss->ss_max_offchan_retries);

        ss->ss_suspending_traffic = false;
        if (ss->ss_offchan_retry_count < ss->ss_max_offchan_retries) {
            ss->ss_offchan_retry_count++;
            ath_schedule_timed_event(ss, ss->ss_offchan_retry_delay, SCANNER_EVENT_OFFCHAN_RETRY_EXPIRE);
        }
        else {
            scanner_terminate(ss, SCAN_REASON_MAX_OFFCHAN_RETRIES);
        }
        break;

    case SCANNER_EVENT_OFFCHAN_RETRY_EXPIRE:
        /*
         * Retry off-channel switch
         */
        scanner_leave_bss_channel(ss);
        break;
        #if UMAC_SUPPORT_VAP_PAUSE
        case SCANNER_EVENT_CAN_PAUSE_WAIT_EXPIRE :
        if (sc->sc_rx_ops->mops.wlan_mlme_get_enh_rpt_ind(sc->sc_pdev)) {
        if (sc->sc_rx_ops->mops.wlan_mlme_pause(sc->sc_pdev) == -1)  {
                 /* check if we can pause AGAIN */
            ath_schedule_timed_event(ss, IEEE80211_CAN_PAUSE_WAIT_DELAY , SCANNER_EVENT_CAN_PAUSE_WAIT_EXPIRE);
            }
            else {
            ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_FOREIGN_CHANNEL);
            }
            }
          break;
      #endif

    default:
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s not handled\n",
            __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

        return false;
    }

    return true;
}

static void scanner_state_foreign_channel_entry(void *context)
{
    ath_scanner_t    ss = context;
    u_int32_t        now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    struct ath_softc *sc = ss->ss_sc;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%d.%03d | %s: entering channel=%d passive=%d probe_delay=%d\n",
        now / 1000, now % 1000, __func__,
        ss->ss_all_chans[ss->ss_scan_chan_indices[ss->ss_next]].mhz,
        is_passive_channel(&ss->ss_all_chans[ss->ss_scan_chan_indices[ss->ss_next]], ss->ss_flags),
        ss->ss_probe_delay);

    ss->ss_info.si_allow_transmit  = false;
    ss->ss_info.si_in_home_channel = false;

    ss->ss_offchan_retry_count     = 0;
    ss->ss_suspending_traffic      = false;

    /*
     * Need to set the hw to scan mode only if previous state was not
     * a foreign channel
     */
    if (ieee80211_sm_get_curstate(ss->ss_hsm_handle) != SCANNER_STATE_FOREIGN_CHANNEL) {
        if (!sc->sc_rx_ops->mops.wlan_mlme_resmgr_active(sc->sc_pdev)) {
#if ATH_SLOW_ANT_DIV
            ath_slow_ant_div_suspend(sc);
#endif
            sc->sc_rx_ops->mops.wlan_mlme_scan_start(sc->sc_pdev);
        }
    } else {
        /* Finishing scan in one channel. Post the get NF event */
        scanner_post_event(ss,
                           SCAN_EVENT_TYPE_FOREIGN_CHANNEL_GET_NF,
                           SCAN_REASON_COMPLETED);
    }

    if (!sc->sc_rx_ops->mops.wlan_mlme_resmgr_active(sc->sc_pdev) ||
        (ieee80211_sm_get_curstate(ss->ss_hsm_handle) == SCANNER_STATE_FOREIGN_CHANNEL)) {
        sc->sc_rx_ops->mops.wlan_mlme_set_channel(sc->sc_pdev,
                                          ss->ss_all_chans[ss->ss_scan_chan_indices[ss->ss_next]].mhz,
                                          ath_phymode2flags(&ss->ss_all_chans[ss->ss_scan_chan_indices[ss->ss_next]]));
#if WLAN_SPECTRAL_ENABLE
        sc->sc_rx_ops->mops.wlan_mlme_start_record_stats(sc->sc_pdev);
#endif
    }

    scanner_post_event(ss,
                       SCAN_EVENT_TYPE_FOREIGN_CHANNEL,
                       SCAN_REASON_NONE);

    if (is_passive_channel(&ss->ss_all_chans[ss->ss_scan_chan_indices[ss->ss_next]], ss->ss_flags)) {
        /*
         * Monitor beacons received only if we're performing an active scan.
         * If scan is passive, we'll never send a probe request, so there's no
         * need to monitor traffic on the scanned channel.
         */
        if (ss->ss_strict_passive_scan) {
            atomic_set(&(ss->ss_wait_for_beacon), false);
        } else {
            atomic_set(&(ss->ss_wait_for_beacon), !(ss->ss_flags & IEEE80211_SCAN_PASSIVE));
        }
        atomic_set(&(ss->ss_beacon_event_queued), false);

        ss->ss_cur_min_dwell = ss->ss_min_dwell_passive;
        ss->ss_cur_max_dwell = ss->ss_max_dwell_passive;

        /*
         * In passive channels, start counting dwell time once we enter
         * the channel.
         * If we receive a beacon we then send a probe request and
         * reset the dwell time.
         */
        ss->ss_enter_channel_time = OS_GET_TIMESTAMP();

        /*
         * Schedule timed event after saving current time.
         * This provides more accurate time calculations if there is a
         * significant latency before the timer is armed.
         */
        if (ss->ss_cur_min_dwell == ss->ss_cur_max_dwell) {
            ath_schedule_timed_event(ss, ss->ss_cur_min_dwell, SCANNER_EVENT_MAXDWELL_EXPIRE);
        } else {
            ath_schedule_timed_event(ss, ss->ss_cur_min_dwell, SCANNER_EVENT_MINDWELL_EXPIRE);
        }
    }
    else {
        atomic_set(&(ss->ss_wait_for_beacon), false);
        ss->ss_cur_min_dwell = ss->ss_min_dwell_active;
        ss->ss_cur_max_dwell = ss->ss_max_dwell_active;

        if (ss->ss_probe_delay) {
            /*
             * Do not start counting dwell time until the probe delay
             * elapses and we send a probe request.
             */
            ath_schedule_timed_event(ss, ss->ss_probe_delay, SCANNER_EVENT_PROBEDELAY_EXPIRE);
        }
        else {
            /* Send first probe request */
            scanner_send_probereq(ss);

            /*
             * Start counting dwell time once we send a probe request.
             */
            ss->ss_enter_channel_time = OS_GET_TIMESTAMP();

            /*
             * Schedule timed event after saving current time.
             * This provides more accurate time calculations if there is a
             * significant latency before the timer is armed.
             */
            if (ss->ss_repeat_probe_time > 0) {
                ath_schedule_timed_event(ss, ss->ss_repeat_probe_time, SCANNER_EVENT_PROBE_REQUEST_TIMER);
            }
            else {
                if (ss->ss_cur_min_dwell == ss->ss_cur_max_dwell) {
                    ath_schedule_timed_event(ss, ss->ss_cur_min_dwell, SCANNER_EVENT_MAXDWELL_EXPIRE);
                } else {
                    ath_schedule_timed_event(ss, ss->ss_cur_min_dwell, SCANNER_EVENT_MINDWELL_EXPIRE);
                }
            }
        }
    }

    /*
     * Field ss_enter_channel_time had already been set above, and holds the
     * timestamp corresponding to a channel change.
     *
     * ss_enter_channel_time is updated on *every* channel change (either to
     * a BSS channel or to a Foreign Channel), while ss_offchannel_time is
     * updated only we scanner first enters a foreign channel after leaving
     * the BSS channel. On subsequent changes to other foreign channels,
     * without returning to the BSS channel, ss_offchannel_time is not updated.
     */
    if (ieee80211_sm_get_curstate(ss->ss_hsm_handle) != SCANNER_STATE_FOREIGN_CHANNEL) {
        ss->ss_offchannel_time = ss->ss_enter_channel_time;
    }

    ss->ss_next++;
}

static void scanner_state_foreign_channel_exit(void *context)
{
    ath_scanner_t    ss = context;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: wait_for_beacon=%d\n",
        __func__,
        ss->ss_wait_for_beacon);

    /* Stop monitoring beacons */
    atomic_set(&(ss->ss_wait_for_beacon), false);
}

static bool scanner_leave_foreign_channel(ath_scanner_t ss)
{
    int    status;
    struct ath_softc *sc = ss->ss_sc;

    if ((ss->ss_flags & IEEE80211_SCAN_BURST) &&
        (ss->ss_max_offchannel_time > 0)      &&
        (ss->ss_visit_home_channel)) {
        u_int32_t    min_dwell;
        u_int32_t    future_offchannel_time;
        systime_t    current_time = OS_GET_TIMESTAMP();

        /*
         * Estimate the total amount of time spent off-channel if we scan one
         * more channel before returning to the BSS channel. If this total time
         * is still less than max_offchannel_time, then we can visit one more
         * channel.
         */
        min_dwell =
            is_passive_channel(&ss->ss_all_chans[ss->ss_scan_chan_indices[ss->ss_next]], ss->ss_flags) ?
                ss->ss_min_dwell_passive : ss->ss_min_dwell_active;

        /*
         * future_offchannel_time is current amount of time spent off-channel
         * plus the dwell time for the next channel.
         */
        future_offchannel_time =
            ((u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(current_time - ss->ss_offchannel_time)) +
            min_dwell;

        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s: current_time=%d ss_offchannel_time=%d min_dwell=%d future_offchannel_time=%d scan_another_channel=%d\n",
            __func__,
            (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(current_time),
            (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(ss->ss_offchannel_time),
            min_dwell,
            future_offchannel_time,
            (future_offchannel_time < ss->ss_max_offchannel_time));

        if (future_offchannel_time < ss->ss_max_offchannel_time) {
            ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_FOREIGN_CHANNEL);
            return true;
        }
    }

#if WLAN_SPECTRAL_ENABLE
    sc->sc_rx_ops->mops.wlan_mlme_end_record_stats(sc->sc_pdev);
#endif


    status = sc->sc_rx_ops->mops.wlan_mlme_resmgr_request_bsschan(sc->sc_pdev);

    switch (status) {
    case EOK:
        /*
         * Resource Manager not active.
         * Scanner needs to visit home channel if connected or if softAP is
         * running. It should not visit home channel in case of auto channel scan.
         */
        if (ss->ss_visit_home_channel && !(sc->sc_rx_ops->mops.wlan_mlme_get_acs_in_progress(sc->sc_pdev, ss->ss_vap_id)) ) {
            ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_BSS_CHANNEL);
        } else {
            ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_FOREIGN_CHANNEL);
        }
        break;

    case EBUSY:
        /*
         * Resource Manager is responsible for resuming traffic, changing
         * channel and sending notification once the operation has completed.
         */
        ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_RESUMING_TRAFFIC);
        break;

    case EINVAL:
        /*
         * No BSS channel (client not connected and no softAP running).
         * Move on to next foreign channel.
         */
        ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_FOREIGN_CHANNEL);
        break;

    default:
        /*
         * Error.
         * Move on to next foreign channel.
         */
        ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_FOREIGN_CHANNEL);
        break;
    }

    return true;
}

static bool scanner_state_foreign_channel_event(void      *context,
                                                u_int16_t event,
                                                u_int16_t event_data_len,
                                                void      *event_data)
{
    ath_scanner_t    ss = context;
#if WLAN_SPECTRAL_ENABLE
    struct ath_softc *sc = ss->ss_sc;
#endif

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s\n",
        __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

    if (is_common_event(ss, event, event_data_len, event_data)) {
        return true;
    }

    switch (event) {
    case SCANNER_EVENT_PROBEDELAY_EXPIRE:
        /* Send first probe request */
        scanner_send_probereq(ss);

        /*
         * Start counting dwell time once we send a probe request.
         */
        ss->ss_enter_channel_time = OS_GET_TIMESTAMP();

        /*
         * Schedule timed event after saving current time.
         * This provides more accurate time calculations if there is a
         * significant latency before the timer is armed.
         */
        if (ss->ss_repeat_probe_time > 0) {
            ath_schedule_timed_event(ss, ss->ss_repeat_probe_time, SCANNER_EVENT_PROBE_REQUEST_TIMER);
        }
        else {
            if (ss->ss_cur_min_dwell == ss->ss_cur_max_dwell) {
                ath_schedule_timed_event(ss, ss->ss_cur_min_dwell, SCANNER_EVENT_MAXDWELL_EXPIRE);
            } else {
                ath_schedule_timed_event(ss, ss->ss_cur_min_dwell, SCANNER_EVENT_MINDWELL_EXPIRE);
            }
        }

        /* Stay in the same state */
        break;

    case SCANNER_EVENT_PROBE_REQUEST_TIMER:
        /*
         * Whenever we have completed the dwell time, give the client
         * a chance to indicate scan must terminate.
         * This allows for the scan to run only for the time necessary to
         * meet a desired condition - for example, find a candidate AP.
         */

#if 0
	if ((ss->ss_info.si_last_term_status == 0) &&(ss->ss_check_termination_function != NULL) &&
            (ss->ss_check_termination_function(ss->ss_check_termination_context) == true)) {
            qdf_print("%s %d Scan termination called ",__func__,__LINE__);
            scanner_terminate(ss, SCAN_REASON_TERMINATION_FUNCTION);
        }
        else {
#endif
            if (scanner_can_leave_foreign_channel(ss)) {
                if (scanner_check_endof_scan(ss)) {
                    scanner_terminate(ss, SCAN_REASON_COMPLETED);
                }
                else {
                    scanner_leave_foreign_channel(ss);
                }
            }
            else {
                /* Send second Probe Request */
                scanner_send_probereq(ss);

                if (ss->ss_cur_min_dwell == ss->ss_cur_max_dwell) {
                    ath_schedule_timed_event(ss, ss->ss_cur_min_dwell - ss->ss_repeat_probe_time, SCANNER_EVENT_MAXDWELL_EXPIRE);
                } else {
                    ath_schedule_timed_event(ss, ss->ss_cur_min_dwell - ss->ss_repeat_probe_time, SCANNER_EVENT_MINDWELL_EXPIRE);
                }
            }
#if 0
        }
#endif
        break;

    case SCANNER_EVENT_MINDWELL_EXPIRE:
        /*
         * Whenever we have completed the dwell time, give the client
         * a chance to indicate scan must terminate.
         * This allows for the scan to run only for the time necessary to
         * meet a desired condition - for example, find a candidate AP.
         */
#if 0
        if ((ss->ss_info.si_last_term_status == 0) && (ss->ss_check_termination_function != NULL) &&
            (ss->ss_check_termination_function(ss->ss_check_termination_context) == true)) {
            qdf_print("%s %d Scan termination called ",__func__,__LINE__);
            scanner_terminate(ss, SCAN_REASON_TERMINATION_FUNCTION);
        }
        else {
#endif
            if (scanner_can_leave_foreign_channel(ss)) {
                if (scanner_check_endof_scan(ss)) {
                     /* get noise floor for the last channel scanned */
                     scanner_post_event(ss,
                                       SCAN_EVENT_TYPE_FOREIGN_CHANNEL_GET_NF,
                                       SCAN_REASON_COMPLETED);

#if WLAN_SPECTRAL_ENABLE
                    sc->sc_rx_ops->mops.wlan_mlme_end_record_stats(sc->sc_pdev);
#endif

                     scanner_terminate(ss, SCAN_REASON_COMPLETED);
                }
                else {
                    scanner_leave_foreign_channel(ss);
                }
            }
            else {
                systime_t    enter_channel_time = ss->ss_enter_channel_time;
                u_int32_t    remaining_dwell_time;
                u_int32_t    new_timer_interval;

                /*
                 * Maximum remaining dwell time = cur_max_dwell_time - elapsed_time
                 * Read starting time before current time to avoid
                 * inconsistent timestamps due to preemption.
                 */
                remaining_dwell_time = ss->ss_cur_max_dwell -
                    CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP() - enter_channel_time);

                /*
                 * Set new timer to minimum of min_dwell and maximum
                 * remaining dwell time.
                 */
                new_timer_interval = min(ss->ss_cur_min_dwell, remaining_dwell_time);

                if (ss->ss_cur_min_dwell == ss->ss_cur_max_dwell) {
                    ath_schedule_timed_event(ss, new_timer_interval, SCANNER_EVENT_MAXDWELL_EXPIRE);
                } else {
                    ath_schedule_timed_event(ss, new_timer_interval, SCANNER_EVENT_MINDWELL_EXPIRE);
                }
            }
#if 0
        }
#endif
        break;

    case SCANNER_EVENT_RECEIVED_BEACON:
        /*
         * Should NOT be monitoring beacons if performing passive scan.
         */
        ASSERT(! (ss->ss_flags & IEEE80211_SCAN_PASSIVE));

        /*
         * Received beacon in passive channel. Flag ss_wait_for_beacon is set
         * only if scan is active.
         */
        if (OS_ATOMIC_CMPXCHG(&ss->ss_wait_for_beacon, true, false) == true) {
            /*
             * Received beacon in passive channel, so it's OK to send probe
             * requests now. Cancel max_passive_dwell timer and switch scanning
             * in this channel from passive to active.
             */
            OS_CANCEL_TIMER(&ss->ss_timer);

            ss->ss_cur_min_dwell = ss->ss_min_dwell_active;
            ss->ss_cur_max_dwell = ss->ss_max_dwell_active;

            /* Send first probe request */
            scanner_send_probereq(ss);

            /*
             * Start counting dwell time once we send a probe request.
             */
            ss->ss_enter_channel_time = OS_GET_TIMESTAMP();

            /*
             * Schedule timed event after saving current time.
             * This provides more accurate time calculations if there is a
             * significant latency before the timer is armed.
             */
            if (ss->ss_repeat_probe_time > 0) {
                ath_schedule_timed_event(ss, ss->ss_repeat_probe_time, SCANNER_EVENT_PROBE_REQUEST_TIMER);
            }
            else {
                if (ss->ss_cur_min_dwell == ss->ss_cur_max_dwell) {
                    ath_schedule_timed_event(ss, ss->ss_cur_min_dwell, SCANNER_EVENT_MAXDWELL_EXPIRE);
                } else {
                    ath_schedule_timed_event(ss, ss->ss_cur_min_dwell, SCANNER_EVENT_MINDWELL_EXPIRE);
                }
            }
        }
        break;

    case SCANNER_EVENT_MAXDWELL_EXPIRE:
        /*
         * Whenever we have completed the dwell time, give the client
         * a chance to indicate scan must terminate.
         * This allows for the scan to run only for the time necessary to
         * meet a desired condition - for example, find a candidate AP.
         */
#if 0
        if ((ss->ss_info.si_last_term_status == 0) && (ss->ss_check_termination_function != NULL) &&
            (ss->ss_check_termination_function(ss->ss_check_termination_context) == true)) {
            qdf_print("%s %d Scan termination called ",__func__,__LINE__);
            scanner_terminate(ss, SCAN_REASON_TERMINATION_FUNCTION);
        }
        else {
#endif
            if (scanner_check_endof_scan(ss)) {
                    /* Defer scan completion to retrieve chan info from target */
                scanner_terminate(ss, SCAN_REASON_COMPLETED);
            }
            else {
                scanner_leave_foreign_channel(ss);
            }
#if 0
        }
#endif
        break;
    default:
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s not handled\n",
            __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

        return false;
    }

    return true;
}

static bool scanner_state_resuming_traffic_event(void      *context,
                                                 u_int16_t event,
                                                 u_int16_t event_data_len,
                                                 void      *event_data)
{
    ath_scanner_t    ss = context;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s\n",
        __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

    if (is_common_event(ss, event, event_data_len, event_data)) {
        return true;
    }

    switch (event) {
    case SCANNER_EVENT_BSSCHAN_SWITCH_COMPLETE:
        /* Go back to BSS channel state part of normal scan */
        ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_BSS_CHANNEL);
        break;

    default:
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s not handled\n",
            __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

        return false;
    }

    return true;
}

static void scanner_state_bss_channel_entry(void *context)
{
    ath_scanner_t    ss = context;
    u_int32_t              now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    struct ath_softc       *sc = ss->ss_sc;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%d.%03d | %s: \n", now / 1000, now % 1000, __func__);

    /*
     * Need to take the hardware out of "scan" mode only if transitioning
     * from a foreign channel.
     */
    if ((ieee80211_sm_get_curstate(ss->ss_hsm_handle) != SCANNER_STATE_BSS_CHANNEL) &&
        (ieee80211_sm_get_curstate(ss->ss_hsm_handle) != SCANNER_STATE_IDLE) &&
        (ieee80211_sm_get_curstate(ss->ss_hsm_handle) != SCANNER_STATE_SUSPENDING_TRAFFIC)) {
        if (! sc->sc_rx_ops->mops.wlan_mlme_resmgr_active(sc->sc_pdev)) {
            sc->sc_rx_ops->mops.wlan_mlme_end_scan(sc->sc_pdev);
            sc->sc_rx_ops->mops.wlan_mlme_set_home_channel(sc->sc_pdev, ss->ss_vap_id);
#if ATH_SLOW_ANT_DIV
            ath_slow_ant_div_resume(sc);
#endif
        }
         ath_scan_enable_txq(sc);
        if (! sc->sc_rx_ops->mops.wlan_mlme_resmgr_active(sc->sc_pdev)) {
            sc->sc_rx_ops->mops.wlan_mlme_set_vdev_wakeup(sc->sc_pdev, ss->ss_vap_id);
            if (avp_is_connected(ss->ss_athvap)) {
                if (sc->sc_rx_ops->mops.wlan_mlme_sta_power_unpause(sc->sc_pdev, ss->ss_vap_id) != EOK)
                {
                    /*
                     * No need for a sophisticated error handling here.
                     * If transmission of fake wakeup fails, subsequent traffic sent
                     * with PM bit clear will have the same effect.
                     */
                    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s failed to send fakewakeup\n",
                        __func__);
                    // status = EIO;
                }
#if ATH_VAP_PAUSE_SUPPORT
                if (sc->sc_rx_ops->mops.wlan_mlme_get_enh_rpt_ind(sc->sc_pdev)) {
                    /* call vap-unpause */
                    sc->sc_rx_ops->mops.wlan_mlme_unpause(sc->sc_pdev);
                }
#endif

            }
#if UMAC_SUPPORT_VAP_PAUSE
            /* call vap-unpause -- resume data and beacons if needed */
            if (sc->sc_rx_ops->mops.wlan_mlme_get_enh_rpt_ind(sc->sc_pdev)) {
                sc->sc_rx_ops->mops.wlan_mlme_unpause(sc->sc_pdev);
            }
#endif
        }

        ss->ss_enter_channel_time = OS_GET_TIMESTAMP();
        ss->ss_beacons_received   = 0;
    }

    ath_schedule_timed_event(ss, ss->ss_min_rest_time, SCANNER_EVENT_RESTTIME_EXPIRE);

    atomic_set(&(ss->ss_wait_for_beacon), false);

    ss->ss_info.si_allow_transmit  = true;
    ss->ss_info.si_in_home_channel = true;

    /*
     * Notify upper layers that we have returned to BSS channel.
     * Notification must be sent in the transition from SCANNER_STATE_SUSPENDING_TRAFFIC
     * to SCANNER_STATE_BSS_CHANNEL since frames may have been queued at upper layers
     * while we attempted to suspend traffic.
     */
    if ((ieee80211_sm_get_curstate(ss->ss_hsm_handle) != SCANNER_STATE_BSS_CHANNEL) &&
        (ieee80211_sm_get_curstate(ss->ss_hsm_handle) != SCANNER_STATE_IDLE)) {
        scanner_post_event(ss,
                           SCAN_EVENT_TYPE_BSS_CHANNEL,
                           SCAN_REASON_NONE);
    }
}

static bool scanner_state_bss_channel_event(void      *context,
                                            u_int16_t event,
                                            u_int16_t event_data_len,
                                            void      *event_data)
{
    ath_scanner_t    ss = context;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s\n",
        __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

    ASSERT(event_data);

    if (is_common_event(ss, event, event_data_len, event_data)) {
        return true;
    }

    switch (event) {
    case SCANNER_EVENT_RESTTIME_EXPIRE:
    case SCANNER_EVENT_EXIT_CRITERIA_EXPIRE:
        if (scanner_can_leave_home_channel(ss)) {
            scanner_leave_bss_channel(ss);
        }
        else {
            ath_schedule_timed_event(ss, ss->ss_exit_criteria_intval, SCANNER_EVENT_EXIT_CRITERIA_EXPIRE);
        }
        break;

    default:
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s not handled\n",
            __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

        return false;
    }

    return true;
}

static void scanner_state_radio_measurement_entry(void *context)
{
    ath_scanner_t    ss = context;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s Prev state %s\n",
        __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle));

    /* cancel timer */
    OS_CANCEL_TIMER(&ss->ss_timer);

    /*
     * Start a timer to ensure the state machine doesn't dealock in case
     * we never receive a "resume" request. The timer value is the nominal
     * radio measurement duration
     */
    ath_schedule_timed_event(ss,
                                   ss->ss_min_dwell_active,
                                   SCANNER_EVENT_RADIO_MEASUREMENT_EXPIRE);
    scanner_post_event(ss,
                       SCAN_EVENT_TYPE_RADIO_MEASUREMENT_START,
                       SCAN_REASON_NONE);
}

static bool scanner_state_radio_measurement_event(void      *context,
                                                  u_int16_t event,
                                                  u_int16_t event_data_len,
                                                  void      *event_data)
{
    ath_scanner_t    ss = context;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s\n",
        __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

    if (is_common_event(ss, event, event_data_len, event_data)) {
        return true;
    }

    switch (event) {

    case SCANNER_EVENT_RADIO_MEASUREMENT_EXPIRE:
        /*
         * End of Radio Measurements
         * Leave Radio measurements state so that scan can be used by others.
         */
        scanner_post_event(ss,
                           SCAN_EVENT_TYPE_RADIO_MEASUREMENT_END,
                           SCAN_REASON_NONE);

        scanner_terminate(ss, SCAN_REASON_COMPLETED);
        break;

    case SCANNER_EVENT_SCAN_REQUEST:
        if (! ss->ss_info.si_scan_in_progress) {
            ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s nothing to do\n",
                __func__,
                ieee80211_sm_get_current_state_name(ss->ss_hsm_handle),
                ath_scan_event_name(event));

            return true;
        }
        break;

    default:
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s in_progress=%d not handled\n",
            __func__,
            ieee80211_sm_get_current_state_name(ss->ss_hsm_handle),
            ath_scan_event_name(event),
            ss->ss_info.si_scan_in_progress);

        return false;
    }

    return true;
}

static void scanner_state_terminating_entry(void *context)
{
    ath_scanner_t    ss = context;
    u_int32_t              now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%d.%03d | %s\n",
        now / 1000, now % 1000, __func__);
}

static bool scanner_state_terminating_event(void      *context,
                                            u_int16_t event,
                                            u_int16_t event_data_len,
                                            void      *event_data)
{
    ath_scanner_t    ss = context;

    ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s\n",
        __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

    switch (event) {
    case SCANNER_EVENT_BSSCHAN_SWITCH_COMPLETE:
    case SCANNER_EVENT_OFFCHAN_SWITCH_FAILED:
    case SCANNER_EVENT_OFFCHAN_SWITCH_ABORTED:
        ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_IDLE);
        break;

    case SCANNER_EVENT_TERMINATING_TIMEOUT:
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s No response from ResMgr\n",
            __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));

        ASSERT(event != SCANNER_EVENT_TERMINATING_TIMEOUT);
        ieee80211_sm_transition_to(ss->ss_hsm_handle, SCANNER_STATE_IDLE);
        break;

    default:
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s event=%s not handled\n",
            __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(event));
        return false;
    }

    return true;
}

static OS_TIMER_FUNC(scanner_timer_handler)
{
    ath_scanner_t    ss;

    OS_GET_TIMER_ARG(ss, ath_scanner_t);

    if (ss->ss_pending_event == SCANNER_EVENT_TERMINATING_TIMEOUT) {
        ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%s state=%s posting event=%s\n",
            __func__, ieee80211_sm_get_current_state_name(ss->ss_hsm_handle), ath_scan_event_name(ss->ss_pending_event));
    }

#if !ATH_SUPPORT_MULTIPLE_SCANS && defined USE_WORKQUEUE_FOR_MESGQ_AND_SCAN
    qdf_sched_work(NULL, &ss->ss_timer_defered);
#else
    ieee80211_sm_dispatch(ss->ss_hsm_handle,
                          ss->ss_pending_event,
                          0,
                          NULL);

    /* Clear pending event */
    ss->ss_pending_event = SCANNER_EVENT_NONE;
#endif
}

static OS_TIMER_FUNC(scanner_timer_maxtime_handler)
{
    ath_scanner_t    ss;

    OS_GET_TIMER_ARG(ss, ath_scanner_t);

#if !ATH_SUPPORT_MULTIPLE_SCANS && defined USE_WORKQUEUE_FOR_MESGQ_AND_SCAN
   qdf_sched_work(NULL, &ss->ss_maxscan_timer_defered);
#else
    ieee80211_sm_dispatch(ss->ss_hsm_handle,
                          SCANNER_EVENT_MAXSCANTIME_EXPIRE,
                          0,
                          NULL);
#endif
}

ieee80211_state_info ath_scan_sm_info[] = {
    {
        (u_int8_t) SCANNER_STATE_IDLE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "IDLE",
        scanner_state_idle_entry,
        NULL,
        scanner_state_idle_event
    },
    {
        (u_int8_t) SCANNER_STATE_SUSPENDING_TRAFFIC,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "SUSPENDING_TRAFFIC",
        scanner_state_suspending_traffic_entry,
        NULL,
        scanner_state_suspending_traffic_event
    },
    {
        (u_int8_t) SCANNER_STATE_FOREIGN_CHANNEL,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "FOREIGN_CHANNEL",
        scanner_state_foreign_channel_entry,
        scanner_state_foreign_channel_exit,
        scanner_state_foreign_channel_event
    },
    {
        (u_int8_t) SCANNER_STATE_RESUMING_TRAFFIC,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "RESUMING_TRAFFIC",
        NULL,
        NULL,
        scanner_state_resuming_traffic_event
    },
    {
        (u_int8_t) SCANNER_STATE_BSS_CHANNEL,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "BSS_CHANNEL",
        scanner_state_bss_channel_entry,
        NULL,
        scanner_state_bss_channel_event
    },
    {
        (u_int8_t) SCANNER_STATE_RADIO_MEASUREMENT,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "RADIO_MEASUREMENT",
        scanner_state_radio_measurement_entry,
        NULL,
        scanner_state_radio_measurement_event
    },
    {
        (u_int8_t) SCANNER_STATE_TERMINATING,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "TERMINATING",
        scanner_state_terminating_entry,
        NULL,
        scanner_state_terminating_event
    },
};


int ath_scan_attach(ath_dev_t dev)
{
    int    status;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_scanner_t ss = NULL;

    qdf_print("%s: Attaching scan", __func__);
    if (ss) {
        qdf_print("%s: ss already allocated", __func__);
        return EINPROGRESS; /* already attached ? */
    }

    ss = (ath_scanner_t) OS_MALLOC(sc->sc_osdev, sizeof(struct ath_scanner), 0);
    if (ss) {
        OS_MEMZERO(ss, sizeof(struct ath_scanner));

        /*
         * Save handles to required objects.
         * Resource manager may not be initialized at this time.
         */
        ss->ss_sc        = sc;
        ss->ss_osdev     = sc->sc_osdev;
        ss->ss_resmgr    = NULL;

        /*
         * TO-DO: For now treat both the si_allow_transmit and si_in_home_channel flags as the same.
         *        and als do not use atomic operation on these
         *        flags until understand VISTA scanner implementation.
         */

        ss->ss_info.si_allow_transmit     = true;
        ss->ss_info.si_in_home_channel    = true;
        ss->ss_info.si_last_term_status   = false;

        ss->ss_hsm_handle = ieee80211_sm_create(sc->sc_osdev,
                                               "scan",
                                               (void *) ss,
                                               SCANNER_STATE_IDLE,
                                               ath_scan_sm_info,
                                               sizeof(ath_scan_sm_info)/sizeof(ieee80211_state_info),
                                               MAX_QUEUED_EVENTS,
                                               sizeof(struct scan_event),
                                               MESGQ_PRIORITY_LOW,
                                               IEEE80211_HSM_ASYNCHRONOUS, /* run the SM asynchronously */
                                               ath_scan_sm_debug_print,
                                               scanner_event_name,
                                               ATH_N(scanner_event_name));
        if (! ss->ss_hsm_handle) {
            OS_FREE((ss));
            qdf_print("%s: ieee80211_sm_create failed", __func__);
            return ENOMEM;
        }
        /*
         * initilize 2 timers.
         * first timer to run the scanner state machine.
         * the second one not to let scanner go beyond max scanner time specified
         */
        OS_INIT_TIMER(sc->sc_osdev, &(ss->ss_timer), scanner_timer_handler, (void *) (ss), QDF_TIMER_TYPE_WAKE_APPS);
        OS_INIT_TIMER(sc->sc_osdev, &(ss->ss_maxscan_timer), scanner_timer_maxtime_handler, (void *) (ss), QDF_TIMER_TYPE_WAKE_APPS);

#if !ATH_SUPPORT_MULTIPLE_SCANS && defined USE_WORKQUEUE_FOR_MESGQ_AND_SCAN
        qdf_create_work(sc->sc_osdev, &((ss)->ss_timer_defered), scan_timer_defered, (void *)(ss));
        qdf_create_work(sc->sc_osdev, &((ss)->ss_maxscan_timer_defered), scan_max_timer_defered, (void *)(ss));
#endif

        spin_lock_init(&(ss->ss_lock));
        ss->ss_enter_channel_time = OS_GET_TIMESTAMP();

        /*
         * Initialize scan scheduler
         */
        status = ath_scan_scheduler_attach(&(ss->ss_scheduler), ss, sc, sc->sc_osdev);

        sc->sc_scanner = (void*)ss;

        return status;
    }

    return ENOMEM;
}

int ath_scan_detach(ath_dev_t dev)
{
    int    status;
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_scanner_t ss = sc->sc_scanner;

    if (ss == NULL) {
        return EINPROGRESS; /* already detached ? */
    }

    /*
     * Terminate scan scheduler
     */
    status = ath_scan_scheduler_detach(&(ss->ss_scheduler));


    if (ss->ss_hsm_handle) {
        ieee80211_sm_delete(ss->ss_hsm_handle);
    }

    /*
     * Free timer objects
     */
    OS_FREE_TIMER(&(ss->ss_timer));
    OS_FREE_TIMER(&(ss->ss_maxscan_timer));

    /*
     * Free synchronization objects
     */
    spin_lock_destroy(&(ss->ss_lock));

    OS_FREE(ss);

    sc->sc_scanner = NULL;


    return EOK;
}

static int ath_scan_set_priority(ath_scanner_t       ss,
                           wlan_scan_requester requestor,
                           wlan_scan_id        scan_id_mask,
                           enum scan_priority  scanPriority)
{
    return ath_scan_scheduler_set_priority(ath_scan_scheduler_object(ss),
                                                 vaphandle,
                                                 requestor,
                                                 scan_id_mask,
                                                 scanPriority);
}

static int ath_scan_set_forced_flag(ath_scanner_t       ss,
                              wlan_scan_requester requestor,
                              wlan_scan_id        scan_id_mask,
                              bool                     forced_flag)
{
    return ath_scan_scheduler_set_forced_flag(ath_scan_scheduler_object(ss),
                                                    ss->ss_athvap,
                                                    requestor,
                                                    scan_id_mask,
                                                    forced_flag);
}

static int
ath_scan_get_priority_table(ath_scanner_t       ss,
                             enum ieee80211_opmode      opmode,
                             int                        *p_number_rows,
                             IEEE80211_PRIORITY_MAPPING **p_mapping_table)
{
    /*
     * Typecast parameter devhandle to ic.
     * Defining a variable causes compilation errors (unreferenced variable)
     * in some platforms.
     */
    return ath_scan_scheduler_get_priority_table( ss->ss_scheduler,
        opmode,
        p_number_rows,
        p_mapping_table);
}

static int
ath_scan_set_priority_table(ath_scanner_t       ss,
                             enum ieee80211_opmode      opmode,
                             int                        number_rows,
                             IEEE80211_PRIORITY_MAPPING *p_mapping_table)
{
    /*
     * Typecast parameter devhandle to ic.
     * Defining a variable causes compilation errors (unreferenced variable)
     * in some platforms.
     */
    return ath_scan_scheduler_set_priority_table( ss->ss_scheduler,
        opmode,
        number_rows,
        p_mapping_table);
}

static int _ath_scan_scheduler_get_requests(ath_scanner_t       ss,
                                     PATH_SCAN_REQUEST scan_request_info,
                                     int                         *total_requests,
                                     int                         *returned_requests)
{
    return  ath_scan_scheduler_get_requests(ss->ss_scheduler,
                                                 scan_request_info,
                                                 total_requests,
                                                 returned_requests);
}

static int ath_scan_enable_scan_scheduler(ath_scanner_t       ss,
                               wlan_scan_requester requestor)
{
    /*
     * Typecast parameter devhandle to ic.
     * Defining a variable causes compilation errors (unreferenced variable)
     * in some platforms.
     */
    return ath_enable_scan_scheduler( ss->ss_scheduler,
        requestor);
}

static int ath_scan_disable_scan_scheduler(ath_scanner_t       ss,
                                wlan_scan_requester requestor,
                                u_int32_t                max_suspend_time)
{
    /*
     * Typecast parameter devhandle to ic.
     * Defining a variable causes compilation errors (unreferenced variable)
     * in some platforms.
     */
    return ath_disable_scan_scheduler( ss->ss_scheduler,
        requestor,
        max_suspend_time);
}

#if !ATH_SUPPORT_MULTIPLE_SCANS && defined USE_WORKQUEUE_FOR_MESGQ_AND_SCAN
void scan_timer_defered(void *timer_arg)
{
    ath_scanner_t      ss;

    OS_GET_TIMER_ARG(ss, ath_scanner_t);

    ieee80211_sm_dispatch(ss->ss_hsm_handle,
                          ss->ss_pending_event,
                          0,
                          NULL);

    /* Clear pending event */
    ss->ss_pending_event = SCANNER_EVENT_NONE;
}

void scan_max_timer_defered(void *timer_arg)
{
    ath_scanner_t      ss;

    OS_GET_TIMER_ARG(ss, ath_scanner_t);

    ieee80211_sm_dispatch(ss->ss_hsm_handle,
                          SCANNER_EVENT_MAXSCANTIME_EXPIRE,
                          0,
                          NULL);
}
#endif

void ath_scan_connection_lost(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_scanner_t ss = sc->sc_scanner;

    if (ss->ss_type == SCAN_TYPE_BACKGROUND) {
        if (ss->ss_info.si_scan_in_progress) {
            u_int32_t    now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

            ATH_SCAN_PRINTF(ss, ATH_DEBUG_SCAN, "%d.%03d | %s: Connection lost; converting to foreground scan\n",
                now / 1000, now % 1000, __func__);
        }

        ss->ss_type = IEEE80211_SCAN_FOREGROUND;

        /*
         * We may still need to visit the home channel if there are other ports active.
         */
        ss->ss_visit_home_channel = ss->ss_multiple_ports_active;
    }
}


void ath_scan_reset_flags(struct ath_softc *sc)
{
    ath_scanner_t ss = sc->sc_scanner;

    ss->ss_info.si_scan_in_progress = FALSE;
}

void ath_scan_sta_power_events(ath_dev_t dev, int event_type, int event_status)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_scanner_t ss = sc->sc_scanner;
    enum scanner_event ev = SCANNER_EVENT_NONE;

    if (event_type == IEEE80211_POWER_STA_PAUSE_COMPLETE) {
        ev = (event_status == IEEE80211_POWER_STA_STATUS_SUCCESS ?
                      SCANNER_EVENT_FAKESLEEP_ENTERED : SCANNER_EVENT_FAKESLEEP_FAILED);
    }

    ieee80211_sm_dispatch(ss->ss_hsm_handle,
                              ev,
                              0,
                              NULL);
}

void ath_scan_resmgr_events(ath_dev_t dev, int event_type, int event_status)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_scanner_t ss = sc->sc_scanner;
    enum scanner_event ev = SCANNER_EVENT_NONE;

    /*
     * SM in umac should provide API to check if scan is in progress, invoke that API once SM code is merged
     */

    /* Skip any ResMgr events if scan not running */
    if (! ss->ss_info.si_scan_in_progress) {
        return;
    }

    switch (event_type) {
    case IEEE80211_RESMGR_BSSCHAN_SWITCH_COMPLETE:
        ev = SCANNER_EVENT_BSSCHAN_SWITCH_COMPLETE;
        break;

    case IEEE80211_RESMGR_OFFCHAN_SWITCH_COMPLETE:
        if (event_status == EOK) {
            ev = SCANNER_EVENT_OFFCHAN_SWITCH_COMPLETE;
        }
        else {
            ev = SCANNER_EVENT_OFFCHAN_SWITCH_FAILED;
        }
        break;

    case IEEE80211_RESMGR_VAP_START_COMPLETE:
        break;

    case IEEE80211_RESMGR_CHAN_SWITCH_COMPLETE:
        ev = SCANNER_EVENT_CHAN_SWITCH_COMPLETE;
        break;

    case IEEE80211_RESMGR_SCHEDULE_BSS_COMPLETE:
        ev = SCANNER_EVENT_SCHEDULE_BSS_COMPLETE;
        break;

    case IEEE80211_RESMGR_OFFCHAN_ABORTED:
        /*
         * Off-channel operation aborted, probably because of VAP UP.
         * Stop scan immediately.
         */
        ev = SCANNER_EVENT_OFFCHAN_SWITCH_ABORTED;
        break;

    default:
#if 0
        printk("%s: ignored ResMgr event %s\n", __func__,
                 ieee80211_resmgr_get_notification_type_name(ss->ss_resmgr,event_type));
#endif
        break;
    }

    ieee80211_sm_dispatch(ss->ss_hsm_handle,
                              ev,
                              0,
                              NULL);
}

bool ath_scan_get_sc_isscan(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);

    return (sc->sc_isscan) ? (true) : (false) ;
}

bool ath_scan_in_home_channel(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    ath_scanner_t ss = sc->sc_scanner;

    return ((ss->ss_info.si_in_home_channel) ? (true) : (false));
}
