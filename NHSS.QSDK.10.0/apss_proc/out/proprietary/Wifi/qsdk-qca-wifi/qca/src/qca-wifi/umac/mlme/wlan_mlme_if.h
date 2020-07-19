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

#ifndef _WLAN_MLME_IF_H
#define _WLAN_MLME_IF_H

#include <qdf_types.h>
#include <qdf_trace.h>
#include <ieee80211_mlme_priv.h>
#include <ieee80211_objmgr_priv.h>
#include <wlan_serialization_api.h>

#define wlan_mlme_err(format, args...) \
    QDF_TRACE_ERROR(QDF_MODULE_ID_MLME, format, ## args)
#define wlan_mlme_info(format, args...) \
    QDF_TRACE_INFO(QDF_MODULE_ID_MLME, format, ## args)
#define wlan_mlme_debug(format, args...) \
    QDF_TRACE_DEBUG(QDF_MODULE_ID_MLME, format, ## args)

#define wlan_mlme_nofl_err(format, args...) \
    QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_MLME, format, ## args)
#define wlan_mlme_nofl_info(format, args...) \
    QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_MLME, format, ## args)
#define wlan_mlme_nofl_debug(format, args...) \
    QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_MLME, format, ## args)

/* Serialization command timeout in milliseconds */
#define WLAN_MLME_SER_CMD_TIMEOUT_MS 65000

#define WLAN_SERIALZATION_CANCEL_WAIT_ITERATIONS 1000
#define WLAN_SERIALZATION_CANCEL_WAIT_TIME 100

/*
 * enum wlan_mlme_notify_mod - Call the notification cb
 * @WLAN_MLME_NOTIFY_NONE - No handlers
 * @WLAN_MLME_NOTIFY_MLME - Notify mlme layer
 * @WLAN_MLME_NOTIFY_OSIF - Notify osif layer
 */
enum wlan_mlme_notify_mod {
    WLAN_MLME_NOTIFY_NONE,
    WLAN_MLME_NOTIFY_MLME,
    WLAN_MLME_NOTIFY_OSIF,
};

/*
 * enum mlme_cmd_activation_ctx - Activation context of cmd
 * @MLME_CTX_DIRECT - Activated from caller context
 * @MLME_CTX_SCHED - Activated in scheduler context
 */
enum mlme_cmd_activation_ctx {
    MLME_CTX_DIRECT,
    MLME_CTX_SCHED,
};

/*
 * struct wlan_mlme_ser_data - Data in serialization command
 * @vdev: VDEV object associated to the command
 * @flags: Store restart or start status flag
 * @notify_osif: On completion of cmd execution indicate whether
 *  the post processing handlers to be called
 * @cmd_in_sched: Mark if a cmd is activated in scheduler context
 * @activation_ctx: Denote the context in which the cmd was activated
 */
struct wlan_mlme_ser_data {
    struct wlan_objmgr_vdev *vdev;
    int32_t flags;
    uint8_t notify_osif;
    uint8_t cmd_in_sched;
    uint8_t activation_ctx;
};

/*
 * struct wlan_mlme_sched_data - Scheduler context execution
 * @vdev: Objmgr vdev object
 * @cmd_type: Command type to be processed
 * @notify_status: Whether to notify MLME and OSIF
 */
struct wlan_mlme_sched_data {
    struct wlan_objmgr_vdev *vdev;
    enum wlan_serialization_cmd_type cmd_type;
    uint8_t notify_status;
};

/*
 * osif_get_num_active_vaps(): Get number of active vaps
 * @comhandle: legacy ic handle
 *
 * Return: number of active vaps
 */
u_int32_t osif_get_num_active_vaps(wlan_dev_t comhandle);

/*
 * osif_get_num_running_vaps(): Get number of running vaps
 * @comhandle: legacy ic handle
 *
 * Running vaps include vaps with tx_rx capability
 * Return: number of active vaps
 */
u_int16_t osif_get_num_running_vaps(wlan_dev_t comhandle);

/*
 * osif_ht40_event_handler(): Event handler
 * @channel: Channel configuration from the event notifier
 *
 * Return: void
 */
void osif_ht40_event_handler(void *arg, wlan_chan_t channel);

/*
 * osif_acs_event_handler(): ACS Event handler
 * @channel: Channel configuration from the event notifier
 *
 * Return: void
 */
void osif_acs_event_handler(void *arg, wlan_chan_t channel);

/*
 * osif_mlme_notify_handler(): Cmd post processing in OSIF layer
 * @vap: legacy vap handle
 * @cmd_type: Serialization cmd type for which the handler is called
 *
 * Return: void
 */
void osif_mlme_notify_handler(wlan_if_t vap,
        enum wlan_serialization_cmd_type cmd_type);

/*
 * wlan_mlme_vdev_cmd_handler(): Cmd post processing handler in mlme layer
 * @vdev: Objmgr vdev information
 * @cmd_type: Serialization cmd type for which the handler is called
 *
 * Return: void
 */
void
wlan_mlme_vdev_cmd_handler(struct wlan_objmgr_vdev *vdev,
                           enum wlan_serialization_cmd_type cmd_type);

/*
 * mlme_ser_process_start_vdev(): Callback called for command activation from
 * serialization
 * @cmd: serialization command information
 * @reason: Callback reason on which the cb function should operate
 *
 * Return: status of command activation after processing
 */
QDF_STATUS
mlme_ser_process_start_vdev(struct wlan_serialization_command *cmd,
                            enum wlan_serialization_cb_reason reason);

/*
 * mlme_ser_process_stop_vdev(): Callback called for command activation from
 * serialization
 * @cmd: serialization command information
 * @reason: Callback reason on which the cb function should operate
 *
 * Return: status of command activation after processing
 */
QDF_STATUS
mlme_ser_process_stop_vdev(struct wlan_serialization_command *cmd,
                           enum wlan_serialization_cb_reason reason);

/*
 * wlan_mlme_release_vdev_req() - Release a cmd from the serialization queue
 * @vdev: vdev object component
 * @cmd_type: serialization command type
 * @status: return value from the command execution
 *
 * Return: Success on command removal, else error value
 */
QDF_STATUS
wlan_mlme_release_vdev_req(struct wlan_objmgr_vdev *vdev,
                           enum wlan_serialization_cmd_type cmd_type,
                           int32_t status);

/*
 * wlan_mlme_start_vdev() - Send start request to a vdev
 * @vdev: vdev object component
 * @f_scan: forcescan flag indicate is rescan is required
 * @notify_osif: post processing handler to be called after cmd execution
 *
 * Return: Success on cmd addition to the serialization queue, else error value
 */
QDF_STATUS
wlan_mlme_start_vdev(struct wlan_objmgr_vdev *vdev,
                     uint32_t f_scan, uint8_t notify_osif);

/*
 * wlan_mlme_stop_vdev() - Send stop request to a vdev
 * @vdev: vdev object component
 * @flags: stop request flags to be sent to mlme
 * @notify_osif: post processing handler to be called after cmd execution
 *
 * Return: Success on cmd addition to the serialization queue, else error value
 */
QDF_STATUS
wlan_mlme_stop_vdev(struct wlan_objmgr_vdev *vdev,
                    uint32_t flags, uint8_t notify_osif);

/*
 * wlan_mlme_restart_vdev() - Send restart request to a vdev
 * @vdev: vdev object component
 * @f_scan: forcescan flag indicate is rescan is required
 * @notify_osif: post processing handler to be called after cmd execution
 *
 * Return: Success on cmd addition to the serialization queue, else error value
 */
QDF_STATUS wlan_mlme_restart_vdev(struct wlan_objmgr_vdev *vdev,
                                  uint32_t f_scan, uint8_t notify_osif);

/*
 * wlan_mlme_inc_act_cmd_timeout() - increase serialization active cmd timeout
 * @vdev: vdev object component
 * @cmd_type: serialization command type
 *
 * Return: void
 */
void
wlan_mlme_inc_act_cmd_timeout(struct wlan_objmgr_vdev *vdev,
                              enum wlan_serialization_cmd_type cmd_type);

/*
 * wlan_mlme_wait_for_cmd_completion() - wait for vdev active cmds completion
 * @vdev: vdev object component
 *
 * Return: void
 */
void wlan_mlme_wait_for_cmd_completion(struct wlan_objmgr_vdev *vdev);

#if WLAN_SER_DEBUG
extern void wlan_ser_print_history(struct wlan_objmgr_vdev *vdev, u_int8_t,
                                   u_int32_t);
#else
#define wlan_ser_print_history(params...)
#endif

#if SM_ENG_HIST_ENABLE
void wlan_mlme_print_all_sm_history(void);
#else
static inline void wlan_mlme_print_all_sm_history(void) {}
#endif

#endif /* _WLAN_MLME_IF_H  */
