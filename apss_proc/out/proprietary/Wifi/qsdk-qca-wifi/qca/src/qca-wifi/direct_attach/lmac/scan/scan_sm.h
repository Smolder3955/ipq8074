/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
#ifndef _IEEE80211_SCAN_PRIV_H
#define _IEEE80211_SCAN_PRIV_H

#include <ieee80211_channel.h>
#include <_ieee80211.h>
#include <ieee80211.h>
#include <ieee80211_var.h>
#include <ieee80211_resmgr.h>
#include "ath_internal.h"

#include <ieee80211_sm.h>
/*
 * ============== WMI ===============
 */
struct channel_param {
    uint8_t chan_id;
    uint8_t pwr;
    uint32_t mhz;
    uint32_t half_rate:1,
        quarter_rate:1,
        dfs_set:1,
        dfs_set_cfreq2:1,
        is_chan_passive:1,
        allow_ht:1,
        allow_vht:1,
        set_agile:1;
    uint32_t phy_mode;
    uint32_t cfreq1;
    uint32_t cfreq2;
    int8_t   maxpower;
    int8_t   minpower;
    int8_t   maxregpower;
    uint8_t  antennamax;
    uint8_t  reg_class_id;
};

struct scan_chan_list_params {
	uint32_t pdev_id;
    uint16_t nallchans;
    struct channel_param ch_param[1];
};

/*
 * ======================== DA scan sm ============================
 */
#define IEEE80211_SCAN_MAX         IEEE80211_CHAN_MAX
#define IEEE80211_SCAN_MAX_SSID           10
#define IEEE80211_SCAN_MAX_BSSID          10
#define MAX_QUEUED_EVENTS  16

#define HW_DEFAULT_OFFCHAN_RETRY_DELAY                                 100
#define IEEE80211_CHECK_EXIT_CRITERIA_INTVAL                           100
#define HW_DEFAULT_SCAN_MIN_BEACON_COUNT_INFRA                         1
#define HW_DEFAULT_SCAN_BEACON_TIMEOUT                                 1600
#define HW_DEFAULT_MAX_OFFCHAN_RETRIES                                 3

#define ATH_SCAN_PRINTF(_ss, _cat, _fmt, ...)           \
    DPRINTF(_ss->ss_sc, _cat, _fmt, __VA_ARGS__)

#define ATH_N(a)    (sizeof(a)/sizeof(a[0]))

typedef struct ath_scanner          *ath_scanner_t;


enum scanner_state {
    SCANNER_STATE_IDLE = 0,           /* idle */
    SCANNER_STATE_SUSPENDING_TRAFFIC, /* waiting for all ports to suspend traffic */
    SCANNER_STATE_FOREIGN_CHANNEL,    /* wait on channel  */
    SCANNER_STATE_RESUMING_TRAFFIC,   /* waiting for all ports to resume traffic */
    SCANNER_STATE_BSS_CHANNEL,        /* on BSS chan for BG scan */
    SCANNER_STATE_RADIO_MEASUREMENT,  /* scanner's radio measurements state */
    SCANNER_STATE_TERMINATING,        /* waiting for Resource Manager to complete last request */
};

/*
   SCANER_STATE_IDLE                :  the scanner is idle and ready to accept and handle scan requests.
   SCANNER_STATE_SUSPENDING_TRAFFIC :  scanner made an off-channel request to ResourceManager and is
                                       waiting for response. The ResourceManager will pause all VAPs, requesting
                                       the PowerSave module to enter NetworkSleep if necessary.
   SCANNER_STATE_FOREIGN_CHANNEL    :  scanner had changed to a new channel and collecting all the probe
                                       response/beacon frames from APs on the channel.
                                       information about reception of probe responses/beacons on the new
                                       channel and expiration of min DWELL time are kept in flags and used
                                       when determining whether we can transition to the next channel.
   SCANNER_STATE_RESUMING_TRAFFIC   :  scanner issued a request to return to the BSS channel to ResourceManager
                                       and is waiting for response. The ResourceManager will resume operation
                                       in all VAPs.
   SCANNER_STATE_BSS_CHANNEL        :  the scanner is back on to the BSS chan (this state is only valid for BG scan.)
   SCANNER_STATE_RADIO_MEASUREMENT  :  scanner is in radio measurements.
   SCANNER_STATE_REPEATER_CHANNEL   :  Remain on AP VAP channel
   SCANNER_STATE_TERMINATING        :  Scanner requested return to the BSS channel to ResourceManager after a termination
                                       condition has been detected and is waiting for response.
*/

enum scanner_event {
    SCANNER_EVENT_NONE,                    /* no event */
    SCANNER_EVENT_PROBEDELAY_EXPIRE,       /* probe delay timer expired */
    SCANNER_EVENT_MINDWELL_EXPIRE,         /* min_dwell timer expired */
    SCANNER_EVENT_MAXDWELL_EXPIRE,         /* max_dwell timer expired */
    SCANNER_EVENT_RESTTIME_EXPIRE,         /* rest_time timer expired */
    SCANNER_EVENT_EXIT_CRITERIA_EXPIRE,    /* re-check conditions to leave channel */
    SCANNER_EVENT_FAKESLEEP_ENTERED,       /* fake sleep entered */
    SCANNER_EVENT_FAKESLEEP_FAILED,        /* fake sleep enter failed */
    SCANNER_EVENT_RECEIVED_BEACON,         /* received beacon/probe response on the channel */
    SCANNER_EVENT_SCAN_REQUEST,            /* request to  scan */
    SCANNER_EVENT_CANCEL_REQUEST,          /* request to cancel scan */
    SCANNER_EVENT_RESTART_REQUEST,         /* request to restart scan */
    SCANNER_EVENT_MAXSCANTIME_EXPIRE,      /* max scan time expired */
    SCANNER_EVENT_FAKESLEEP_EXPIRE,        /* fakesleep timeout */
    SCANNER_EVENT_RADIO_MEASUREMENT_EXPIRE,/* radio measurements timeout */
    SCANNER_EVENT_PROBE_REQUEST_TIMER,     /* probe request timer expired */
    SCANNER_EVENT_BSSCHAN_SWITCH_COMPLETE, /* RESMGR completed switch to BSS channel */
    SCANNER_EVENT_OFFCHAN_SWITCH_COMPLETE, /* RESMGR completed switch to foreign channel */
    SCANNER_EVENT_OFFCHAN_SWITCH_FAILED,   /* RESMGR could not complete switch to foreign channel */
    SCANNER_EVENT_OFFCHAN_SWITCH_ABORTED,  /* RESMGR aborted switch to foreign channel - possibly because of VAP UP event */
    SCANNER_EVENT_OFFCHAN_RETRY_EXPIRE,    /* completed delay before retrying a switch to a foreign channel */
    SCANNER_EVENT_CHAN_SWITCH_COMPLETE,    /* RESMGR completed switch to new channel */
    SCANNER_EVENT_SCHEDULE_BSS_COMPLETE,   /* RESMGR completed scheduling new BSS */
    SCANNER_EVENT_TERMINATING_TIMEOUT,     /* timeout waiting for RESMGR event */
    SCANNER_EVENT_EXTERNAL_EVENT,          /* event generated externally (usually by the Scan Scheduler) */
    SCANNER_EVENT_PREEMPT_REQUEST,         /* preemption request */
    SCANNER_EVENT_FATAL_ERROR,             /* fatal error - abort scan */
    SCANNER_EVENT_CAN_PAUSE_WAIT_EXPIRE ,  /* can pause wait timer expiry */
};


/* State transition table


##For Background Scan
event       curstate -->      IDLE           SUSPENDING        FOREIGN_CHANNEL      BSSCHAN         RESUMING      TERMINATING     RADIO_MEASUREMENT
                                              TRAFFIC                                               TRAFFIC
--------------------------------------------------------------------------------------------------------------------------------------------

NONE                           --              --                   --                --              --             --            --
PROBEDELAY_EXPIRE              --              --             FOREIGN_CHANNEL         --              --             --            --
MINDWELL_EXPIRE                --              --             FOREIGN_CHANNEL         --              --             --            --
MAXDWELL_EXPIRE                --              --                 RESUMING            --              --             --            --
RESTTIME_EXPIRE                --              --                   --            SUSPENDING          --             --            --
EXIT_CRITERIA_EXPIRE           --              --                   --            SUSPENDING          --             --            --
SCAN_REQUEST               SUSPENDING          --                   --                --              --             --            --
FAKESLEEP_ENTERED              --         FOREIGN_CHANNEL           --                --              --             --            --
FAKESLEEP_FAILED               --         FOREIGN_CHANNEL           --                --              --             --            --
RECEIVED_BEACON,               --              --             FOREIGN_CHANNEL         --              --             --            --
CANCEL_REQUEST                 --           TERMINATING         TERMINATING       TERMINATING     TERMINATING        --       TERMINATING
RADIO_MEAS_REQUEST      RADIO_MEAS/SUSPENDING  --                   --                --              --             --            --
RESTART_REQUEST                --              --             FOREIGN_CHANNEL         --              --             --        see Note below
MAXSCANTIME_EXPIRE             --           SUSPENDING          TERMINATING       TERMINATING     TERMINATING        --          RADIO_MEAS
FAKESLEEP_EXPIRE               --         FOREIGN_CHANNEL           --                --              --             --            --
PROBE_REQUEST_TIMER            --              --             FOREIGN_CHANNEL         --              --             --            --
BSSCHAN_SWITCH_COMPLETE        --              --                   --                --   IDLE/BSSCHAN/RADIO_MEAS  IDLE           --
OFFCHAN_SWITCH_COMPLETE        --         FOREIGN_CHANNEL           --                --              --             --            --
OFFCHAN_SWITCH_FAILED          --           SUSPENDING              --                --              --             --            --
OFFCHAN_RETRY_EXPIRE           --           SUSPENDING          TERMINATING       TERMINATING     TERMINATING        --       TERMINATING
SCHEDULE_BSS_COMPLETE          --              --                   --             RESUMING           --             --            --
TERMINATING_TIMEOUT            --              --                   --                --              --             --            --
PREEMPT_REQUEST                --           TERMINATING         TERMINATING       TERMINATING     TERMINATING        --       TERMINATING

BSSCHAN_SWITCH_COMPLETE will take to different states based on whether pause requested/resume requested.
          if Radio Meas and resume requested, then Next state would be BSSCHAN/IDLE based on scan already in progress or not.
          if Radio Meas requested and Pause Request has not yet handled, then Next state would be RADIO_MEAS.
          if Radio Meas not requested, then Next state would be BSSCHAN.

RESUME_REQUEST will resume to a different states based on when it went in to RADIO_MEASUREMENT state.
          if RADIO_MEAS at FAKESLEEP_WAIT then the resume state would be the same .
          if RADIO_MEAS at FOREIGN_CHANNEL then the resume state would be the same .
          if RADIO_MEAS at BSSCHAN or BSSCHAN_RESTTIME or BSSCHAN_CONTINUE then the resume state would be BSSCHAN .
          if RADIO_MEAS at IDLE or TERMINATING then the resume state would be IDLE if no scan request comes in the middle
          else it would be BSSCHAN .

--------------------------------------------------------------------------------------------------------------


##For Foreground Scan
event       curstate -->      IDLE          SUSPENDING         FOREIGN_CHANNEL       TERMINATING        RADIO_MEASUREMENT
                                             TRAFFIC
---------------------------------------------------------------------------------------------------------------------------

NONE                           --               --                  --                   --               --
PROBEDELAY_EXPIRE              --               --             FOREIGN_CHANNEL           --               --
MINDWELL_EXPIRE                --               --             FOREIGN_CHANNEL           --               --
MAXDWELL_EXPIRE                --               --             FOREIGN_CHANNEL           --               --
SCAN_REQUEST               SUSPENDING           --                  --                   --               --
CANCEL_REQUEST                 --               --               TERMINATING         TERMINATING      TERMINATING
RADIO_MEAS_REQUEST             --               --               RADIO_MEAS          RADIO_MEAS           --
RESTART_REQUEST             SUSPENDING          --             FOREIGN_CHANNEL           --              --
MAXSCANTIME_EXPIRE             --               --               TERMINATING         TERMINATING       RADIO_MEAS
BSSCHAN_SWITCH_COMPLETE        --               --                  --                  IDLE             --
OFFCHAN_SWITCH_COMPLETE        --         FOREIGN_CHANNEL           --                   --              --
OFFCHAN_SWITCH_FAILED          --           SUSPENDING              --                   --              --
OFFCHAN_RETRY_EXPIRE           --           TERMINATING          TERMINATING         TERMINATING     TERMINATING
SCHEDULE_BSS_COMPLETE          --              --                   --                RESUMING           --
TERMINATING_TIMEOUT            --              --                   --                   --              --
PREEMPT_REQUEST                --              --                TERMINATING         TERMINATING      TERMINATING

RESUME_REQUEST will resume to a different states based on when it went in to RADIO_MEAS state.
          if RADIO_MEAS at FOREIGN_CHANNEL then the resume state would be the same .

**** Rest of the events are not relevant to Foreground scan.
-----------------------------------------------------------------------------------------------------------------------------------------


##For Repeater Scan
event       curstate -->      IDLE          REPEATER         TERMINATING        RADIO_MEASUREMENT
                                             SCAN
---------------------------------------------------------------------------------------------------

NONE                           --                --                --               --
PROBEDELAY_EXPIRE              --             TERMINATING          --               --
MINDWELL_EXPIRE                --             TERMINATING          --               --
MAXDWELL_EXPIRE                --             TERMINATING          --               --
SCAN_REQUEST               REPEATER_SCAN         --                --               --
CANCEL_REQUEST                 --             TERMINATING      TERMINATING      TERMINATING
RADIO_MEAS_REQUEST             --             RADIO_MEAS       RADIO_MEAS           --
RESTART_REQUEST            REPEATER SCAN      TERMINATING          --               --
MAXSCANTIME_EXPIRE             --             TERMINATING      TERMINATING       RADIO_MEAS
BSSCHAN_SWITCH_COMPLETE        --                --                IDLE             --
OFFCHAN_SWITCH_COMPLETE        --                --                --               --
OFFCHAN_SWITCH_FAILED          --                --                --               --
OFFCHAN_RETRY_EXPIRE           --             TERMINATING      TERMINATING     TERMINATING
SCHEDULE_BSS_COMPLETE          --                --               RESUMING          --
TERMINATING_TIMEOUT            --                --                --               --
PREEMPT_REQUEST                --             TERMINATING      TERMINATING      TERMINATING


**** Rest of the events are not relevant to Foreground scan.
-----------------------------------------------------------------------------------------------------------------------------------------

*/

#define CHECK_EXIT_CRITERIA_INTVAL           100

#define IEEE80211_MIN_MINDWELL                          0 /* msec */
#define IEEE80211_MAX_MAXDWELL                       5000 /* msec */
#define FAKESLEEP_WAIT_TIMEOUT                       1000 /* msec */
#define IEEE80211_VAP_SUSPEND_DELAY                   100 /* msec */
#define IEEE80211_MIN_OFFCHAN_RETRY_DELAY              50 /* msec */
#define IEEE80211_MAX_OFFCHAN_RETRIES                  20 /* no units */
#define IEEE80211_MAX_OFFCHAN_RETRY_TIME            10000 /* no units */
#define RESMGR_WAIT_TIMEOUT                1500 /* msec */
#define IEEE80211_CANCEL_RETRY_DELAY                   20 /* msec */
#define IEEE80211_CANCEL_TIMEOUT                     5000 /* msec */
#define IEEE80211_CAN_PAUSE_WAIT_DELAY                 10 /* msec */

struct ath_scanner_info {
    u_int32_t    si_allow_transmit   : 1,
                 si_in_home_channel  : 1,
                 si_scan_in_progress : 1,
		 /* Flag to indicate last scan done is full scan or
			terminated by terminator function */
                 si_last_term_status : 1;
};

typedef struct scanner_preemption_data {
    bool         preempted;     /* indicate scan was really preempted */
    u_int16_t    nchans;        /* # chan's to scan */
    u_int16_t    nallchans;     /* # all chans's to scan */
    u_int16_t    next;          /* index of next chan to scan */
} scanner_preemption_data_t;

typedef struct scan_request_data {
    wlan_scan_requester     requestor;
    enum scan_priority      priority;
    wlan_scan_id            scan_id;
    systime_t                    request_timestamp;
    struct scan_start_request *req;
    scanner_preemption_data_t    preemption_data;
 } scan_request_data_t;

typedef enum _scanner_stop_mode {
    STOP_MODE_CANCEL,
    STOP_MODE_PREEMPT
} scanner_stop_mode;

struct ath_scanner {

    struct ath_scanner_info ss_info;

    /* Driver-wide data structures */
    struct ath_softc                    *ss_sc;
    struct ath_vap                      *ss_athvap;
    int                                 ss_vap_id;
    osdev_t                             ss_osdev;
    void*                               ss_resmgr;
    void*                               ss_scheduler;

    spinlock_t                          ss_lock;

    u_int32_t                           ss_run_counter; /* number of scans requested */

    /*
     * Scan state machine.
     */
    ieee80211_hsm_t                     ss_hsm_handle;

    /*
     * 2 timers:
     *     -Generic timer controlling the amount of time spent in each state;
     *     -Maximum duration the scan can run.
     *
     * One auxiliary variable to map expiration of the generic timer into
     * different events.
     */
    os_timer_t                          ss_timer;
    os_timer_t                          ss_maxscan_timer;

#if !ATH_SUPPORT_MULTIPLE_SCANS && defined USE_WORKQUEUE_FOR_MESGQ_AND_SCAN
    qdf_work_t                       ss_timer_defered;         /* work queue for scan_timer defered */
    qdf_work_t                       ss_maxscan_timer_defered; /* work queue for max scan timer defered */
#endif

    enum scanner_event                  ss_pending_event;

    /*
     * General scan information - scan type (foreground/background), flags,
     * request id (ID of the client which requested the scan) and start time.
     */
    enum scan_type                      ss_type;
    u_int16_t                           ss_flags;
    wlan_scan_requester                 ss_requestor;
    wlan_scan_id                        ss_scan_id;
    enum scan_priority                  ss_priority;
    systime_t                           ss_scan_start_time;
    systime_t                           ss_last_full_scan_time;

    /*
     * Number of off-channel retries and maximum allowed.
     */
    u_int16_t                           ss_offchan_retry_count;
    u_int16_t                           ss_max_offchan_retries;

    /*
     * List of SSIDs and BSSIDs:
     * Probe requests are sent to each BSSID in the list (usually the wildcard
     * BSSID), one request for each SSID in the list.
     * The specified IE elements are added to the probe requests.
     */
    u_int8_t                            ss_nssid;                /* # ssid's to probe/match */
    struct wlan_ssid                    ss_ssid[IEEE80211_SCAN_MAX_SSID];
    u_int8_t                            ss_nbssid;               /* # bssid's to probe */
    u_int8_t                            ss_bssid[IEEE80211_SCAN_MAX_BSSID][IEEE80211_ADDR_LEN];
    struct element_info                 ss_ie;

    /* List of channels to be scanned for the current scan */
    u_int8_t                            ss_scan_chan_indices[IEEE80211_SCAN_MAX];
    u_int8_t                            ss_num_chan_indices;
    u_int16_t                           ss_next;                           /* index of next chan to scan */

    /* List of channels supported by current reg domain - Set by set_channel_list from UMAC */
    struct channel_param                ss_all_chans[IEEE80211_SCAN_MAX];
    u_int16_t                           ss_nallchans;                         /* # all chan's to scan */

    /* Timing values for each phase of the scan procedure - all in ms */
    u_int32_t                           ss_cur_min_dwell;        /* min dwell time on the current foreign channel */
    u_int32_t                           ss_cur_max_dwell;        /* max dwell time on the current foreign channel */
    u_int32_t                           ss_min_dwell_active;     /* min dwell time on an active channel */
    u_int32_t                           ss_max_dwell_active;     /* max dwell time on an active channel */
    u_int32_t                           ss_min_dwell_passive;    /* min dwell time on a passive channel */
    u_int32_t                           ss_max_dwell_passive;    /* max dwell time on a passive channel */
    u_int32_t                           ss_min_rest_time;        /* minimum rest time on channel */
    u_int32_t                           ss_init_rest_time;       /* initial rest time on channel */
    u_int32_t                           ss_max_rest_time;        /* maximum rest time on channel */
    u_int32_t                           ss_probe_delay;          /* time delay before sending first probe request */
    u_int32_t                           ss_repeat_probe_time;    /* time delay before sending subsequent probe requests */
    u_int32_t                           ss_offchan_retry_delay;  /* delay to wait before retrying off channel switch */
    u_int32_t                           ss_idle_time;            /* idle time on bss channel */
    u_int32_t                           ss_exit_criteria_intval; /* time interval before rechecking conditions to leave channel */
    u_int32_t                           ss_max_scan_time;        /* maximum time to finish scanning */
    u_int32_t                           ss_min_beacon_count;     /* number of home AP beacons to receive before leaving the home channel */
    u_int32_t                           ss_beacons_received;     /* number of beacons received from the home AP */
    u_int32_t                           ss_beacon_timeout;       /* maximum time to wait for beacons */
    u_int32_t                           ss_fakesleep_timeout;    /* maximum time to wait for a notification of transmission of a Null Frame */
    u_int32_t                           ss_max_offchannel_time;  /* maximum time to spend off-channel (cumulative for several consecutive foreign channels) */
    systime_t                           ss_offchannel_time;      /* time scanner first went off-channel - reset on every return to BSS channel */

    /* flags controlling scan's execution */
    u_int32_t                           ss_multiple_ports_active          : 1,
                                        ss_visit_home_channel             : 1,
                                        ss_suspending_traffic             : 1,
                                        ss_completion_indications_pending : 1,
                                        ss_restricted                     : 1, /* perform restricted scan */
                                        ss_scan_start_event_posted        : 1, /* Due to scan pause, scan start will be handled from scan_radio_meas state
                                                                                  This flag tells us whether scan request came after pause request or
                                                                                   before pause request. This flag will be true if scan came first then
                                                                                   pause else it will be false */
                                        ss_strict_passive_scan            :1;

    atomic_t                            ss_wait_for_beacon;
    atomic_t                            ss_beacon_event_queued;

    systime_t                           ss_enter_channel_time;
    systime_t                           ss_currscan_home_entry_time;
    /*
     * State information - current state, suspend state (if a scan pause is
     * requested).
     */
    enum scanner_state                  ss_suspend_state;

    /* termination reason */
    enum scan_completion_reason    ss_termination_reason;

    /* state in which scan was preempted */
    scanner_preemption_data_t           ss_preemption_data;

};



/*
 *  * lower 16 bits (0 - 15): frequency.
 *   * upper 8  bits (16-24) :  phy mode.
 *    */
#define SCAN_CHAN_FREQ_SHIFT         0
#define SCAN_CHAN_FREQ_MASK          0xffff
#define SCAN_CHAN_MODE_SHIFT    16
#define SCAN_CHAN_MODE_MASK     0xff

#define SCAN_CHAN_GET_MODE(_c) \
    ( ((_c) >> SCAN_CHAN_MODE_SHIFT ) & SCAN_CHAN_MODE_MASK )

#define SCAN_CHAN_GET_FREQ(_c) \
    ( ((_c) >> SCAN_CHAN_FREQ_SHIFT ) & SCAN_CHAN_FREQ_MASK )

#define SCAN_PASSIVE            0x0001 /* passively scan all the channels */
#define SCAN_ACTIVE             0x0002 /* actively  scan all the channels (regdomain rules still apply) */
#define SCAN_2GHZ               0x0004 /* scan 2GHz band */
#define SCAN_5GHZ               0x0008 /* scan 5GHz band */
#define SCAN_ALLBANDS           (SCAN_5GHZ | SCAN_2GHZ)
#define SCAN_CONTINUOUS         0x0010 /* keep scanning until maxscantime expires */
#define SCAN_FORCED             0x0020 /* forced scan (OS request) - should proceed even in the presence of data traffic */
#define SCAN_NOW                0x0040 /* scan now (User request)  - should proceed even in the presence of data traffic */
#define SCAN_TERMINATE_ON_BSSID_MATCH 0x0080

#define SCAN_CANCEL_ASYNC 0x0 /* asynchronouly wait for scan SM to complete cancel */
#define SCAN_CANCEL_WAIT  0x1 /* wait for scan SM to complete cancel */
#define SCAN_CANCEL_SYNC  0x2 /* synchronously execute cancel scan */

int ath_scan_attach(ath_dev_t dev);
int ath_scan_detach(ath_dev_t dev);
int ath_scan_sm_start(ath_dev_t dev, void *req);
int ath_scan_cancel(ath_dev_t  dev, void *req);
void ath_scan_connection_lost(ath_dev_t dev);
void ath_scan_sta_power_events(ath_dev_t dev, int event_type, int event_status);
void ath_scan_resmgr_events(ath_dev_t dev, int event_type, int event_status);
bool ath_scan_get_sc_isscan(ath_dev_t dev);
int ath_scan_set_channel_list(ath_dev_t dev, void *params);
bool ath_scan_in_home_channel(ath_dev_t dev);


#endif
