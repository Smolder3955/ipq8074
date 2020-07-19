/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
 * private file for scan module. not to be included
 * outside scan module.
 */
#ifndef _IEEE80211_SCAN_SCHED_PRIV_H
#define _IEEE80211_SCAN_SCHED_PRIV_H

#include "scan_sm.h"


#define ATH_SCAN_SCHEDULER_DEFAULT_MONITORING_INTERVAL           1000
#define MAX_SCAN_SCHEDULER_EVENTS                                   8
#define SUSPEND_SCHEDULER_WATCHDOG_LATENCY                       1000

#define STA_HASHSIZE               32
/* simple hash is enough for variation of macaddr */
#define STA_HASH(addr)                                                  \
    (((const u_int8_t *)(addr))[IEEE80211_ADDR_LEN - 1] % STA_HASHSIZE)

typedef enum _ath_scan_request_status {
    ATH_SCAN_STATUS_QUEUED,
    ATH_SCAN_STATUS_RUNNING,
    ATH_SCAN_STATUS_PREEMPTED,
    ATH_SCAN_STATUS_COMPLETED
} ath_scan_request_status;

typedef struct ath_scan_request {
	TAILQ_ENTRY(ath_scan_request)     list;

    wlan_scan_requester               requestor;
    enum scan_priority                requested_priority;
    u_int32_t                         absolute_priority;
    wlan_scan_id                      scan_id;
    ath_scan_request_status           scheduling_status;
    struct scan_req_params            params;
    scanner_preemption_data_t         preemption_data;
    systime_t                         request_timestamp;
    u_int32_t                         maximum_duration;
} ATH_SCAN_REQUEST, *PATH_SCAN_REQUEST;

typedef struct ath_priority_mapping_entry {
    u_int8_t                      n_rows;
    IEEE80211_PRIORITY_MAPPING         *opmode_priority_mapping_table;
} ATH_PRIORITY_MAPPING_ENTRY;


// Define a queue type that can be used with function arguments
typedef TAILQ_HEAD(, ath_scan_request)       SCHEDULER_QUEUE;

//
// Request objects are returned to the free list after a scan completes.
// Reusing the memory blocks avoids frequent and expensive memory allocations.
//
typedef struct ath_scan_scheduler {
    struct ath_softc                   *ssc_sc;
    ath_scanner_t                       ssc_scanner;

    SCHEDULER_QUEUE                     ssc_request_list; /* list of all requested scans */
    SCHEDULER_QUEUE                     ssc_free_list;    /* list of available requests */
    spinlock_t                          ssc_request_list_lock;
    spinlock_t                          ssc_free_list_lock;
    spinlock_t                          ssc_current_scan_lock;
    spinlock_t                          ssc_priority_table_lock;

    PATH_SCAN_REQUEST                   ssc_current_scan;

    atomic_t                            ssc_id_generator;
    bool                                ssc_enabled;
    bool                                ssc_initialized;
    bool                                ssc_terminating;
    u_int32_t                           ssc_monitoring_interval;
    u_int32_t                           ssc_suspend_time;
    systime_t                           ssc_suspend_timestamp;
    wlan_scan_requester                 ssc_suspend_requestor;  /* The requestor who did the suspend. */

    os_mesg_queue_t                     ssc_mesg_queue;
    os_timer_t                          ssc_monitor_timer;
    os_timer_t                          ssc_suspend_timer;

    ATH_PRIORITY_MAPPING_ENTRY          ssc_priority_mapping_table[IEEE80211_OPMODE_MAX+1];
} ATH_SCAN_SCHEDULER, *PATH_SCAN_SCHEDULER;

enum ath_scan_scheduler_event {
    SCAN_SCHEDULER_EVENT_NONE,
    SCAN_SCHEDULER_EVENT_START_NEXT
};


int ath_scan_external_event(ath_scanner_t              ss,
                                  enum scan_event_type        notification_event,
                                  enum scan_completion_reason notification_reason,
                                  wlan_scan_id                scan_id,
                                  wlan_scan_requester         requestor);

int ath_scan_local_set_priority(ath_scanner_t      ss,
                                wlan_scan_requester requestor,
                                wlan_scan_id        scan_id,
                                enum scan_priority  scan_priority);

int ath_scan_local_set_forced_flag(ath_scanner_t      ss,
                                   wlan_scan_requester requestor,
                                   wlan_scan_id        scan_id,
                                   bool                     forced_flag);

int ath_print_scan_config(ath_scanner_t ss,
                                struct seq_file *m);
#if ATH_SUPPORT_MULTIPLE_SCANS
/*
 * Private interface between Scan Scheduler and Scanner.
 * All other modules, including those in the UMAC, should use
 * wlan_scan_start/wlan_scan_cancel.
 */
int ath_scan_scheduler_attach(ath_scan_scheduler_t *ssc,
                                    ath_scanner_t        ss,
                                    wlan_dev_t                 devhandle,
                                    osdev_t                    osdev);

int ath_scan_scheduler_detach(ath_scan_scheduler_t *ssc);

int ath_scan_scheduler_add(ath_scan_scheduler_t ssc,
                                 wlan_if_t                  vaphandle,
                                 ath_scan_params      *params,
                                 wlan_scan_requester   requestor,
                                 scan_priority    priority,
                                 wlan_scan_id          *p_scan_id);

int ath_scan_scheduler_remove(ath_scan_scheduler_t ssc,
                                    wlan_if_t                  vaphandle,
                                    wlan_scan_requester   requestor,
                                    wlan_scan_id          scan_id_mask,
                                    u_int32_t                  flags);

void ath_scan_scheduler_preprocess_event(ath_scan_scheduler_t ssc,
                                               struct scan_event *event);

void ath_scan_scheduler_postprocess_event(ath_scan_scheduler_t ssc,
                                                struct scan_event *event);

int ath_scan_scheduler_set_priority_table(ath_scan_scheduler_t ssc,
                                                enum ieee80211_opmode      opmode,
                                                int                        number_rows,
                                                IEEE80211_PRIORITY_MAPPING *p_mapping_table);

int ath_scan_scheduler_get_priority_table(ath_scan_scheduler_t ssc,
                                                enum ieee80211_opmode      opmode,
                                                int                        *p_number_rows,
                                                IEEE80211_PRIORITY_MAPPING **p_mapping_table);

int ath_scan_scheduler_set_priority(ath_scan_scheduler_t ssc,
                                          wlan_if_t                  vaphandle,
                                          wlan_scan_requester   requestor,
                                          wlan_scan_id          scan_id_mask,
                                          scan_priority    scanPriority);

int ath_scan_scheduler_set_forced_flag(ath_scan_scheduler_t ssc,
                                             wlan_if_t                  vaphandle,
                                             wlan_scan_requester   requestor,
                                             wlan_scan_id          scan_id_mask,
                                             bool                       forced_flag);

int ath_scan_scheduler_get_request_info(ath_scan_scheduler_t ssc,
                                              wlan_scan_id          scan_id,
                                              ath_scan_info        *scan_info);

int
ath_scan_scheduler_get_requests(PATH_SCAN_SCHEDULER ssc,
                                     ath_scan_request_info *scan_request_info,
                                     int                         *total_requests,
                                     int                         *returned_requests);

int ath_scan_get_preemption_data(ath_scanner_t       ss,
                                       wlan_scan_requester  requestor,
                                       wlan_scan_id         scan_id,
                                       scanner_preemption_data_t *preemption_data);

int ieee80211_enable_scan_scheduler(ath_scan_scheduler_t ssc,
                                    wlan_scan_requester   requestor);

int ieee80211_disable_scan_scheduler(ath_scan_scheduler_t ssc,
                                     wlan_scan_requester   requestor,
                                     u_int32_t                  disable_duration);


#else

#define ath_scan_scheduler_attach(_ssc, _ss, _devhandle, _osdev)                                   EOK

#define ath_scan_scheduler_detach(_ssc)                                                            EOK

#define ath_scan_scheduler_add(_ssc, _vaphandle, _params, _requestor, _priority, _p_scan_id)       ENOENT

#define ath_scan_scheduler_remove(_ssc, _vaphandle, _requestor, _scan_id_mask, _flags)             ENOENT

#define ath_scan_scheduler_preprocess_event(_ssc, _event)                                          /* */

#define ath_scan_scheduler_postprocess_event(_ssc, _event)                                         /* */

#define ath_scan_scheduler_set_priority_table(_ssc, _opmode, _number_rows, _p_mapping_table)       EOK

static INLINE int ath_scan_scheduler_get_priority_table(PATH_SCAN_SCHEDULER ssc,
                                                              enum ieee80211_opmode      opmode,
                                                              int                        *p_number_rows,
                                                              IEEE80211_PRIORITY_MAPPING **p_mapping_table)
{
    *p_number_rows   = 0;
    *p_mapping_table = NULL;

    return EOK;
}

#define ath_scan_scheduler_set_priority(_ssc, _vap, _req, _id, _prio)          ath_scan_local_set_priority(_ssc, _req, _id, _prio)

#define ath_scan_scheduler_set_forced_flag(_ssc, _vap, _req, _id, _flag)       ath_scan_local_set_forced_flag(_ssc, _req, _id, _flag)

#define ath_scan_scheduler_get_request_info(_ssc, _scan_id, _scan_info)                            ENOENT

#define ath_scan_scheduler_get_requests(_ssc, _buffer, _total, _returned)                          ENOENT

#define ath_enable_scan_scheduler(_ssc, _req)                                                      EOK

#define ath_disable_scan_scheduler(_ssc, _req, _duration)                                          EOK

#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */

#endif
