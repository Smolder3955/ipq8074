/*
 * Copyright (c) 2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2011, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * LMAC offload interface functions for UMAC - for power and performance offload model
 */
#include <osdep.h>
#include "ol_if_athvar.h"
#include "a_debug.h"
#include "wmi_unified_api.h"
#include "ol_ratetable.h"
#include <init_deinit_lmac.h>
#include "fw_dbglog_api.h"
#if defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG)
#include <pktlog_ac_api.h>
#include <pktlog_ac_fmt.h>
#include <pktlog_ac.h>
#if UNIFIED_SMARTANTENNA
#include <target_if_sa_api.h>
#endif
#if UMAC_SUPPORT_ACFG
#include "ieee80211_acfg.h"
#endif /* UMAC_SUPPORT_ACFG */
#endif /* defined(OL_ATH_SMART_LOGGING) && !defined(REMOVE_PKT_LOG) */

#define TPC_TABLE_TYPE_CDD  0
#define TPC_TABLE_TYPE_STBC 1
#define TPC_TABLE_TYPE_TXBF 2

#ifdef OL_ATH_SMART_LOGGING

#ifndef REMOVE_PKT_LOG
/**
 * smart_log_fw_pktlog_stop_reason_t - enum specifying reason for stopping smart
 * log FW initiated packetlog
 *
 * @SMART_LOG_FW_PKTLOG_STOP_NORMAL: Normal stop by firmware
 * @SMART_LOG_FW_PKTLOG_STOP_HOSTREQ: Stop requested by host
 * @SMART_LOG_FW_PKTLOG_STOP_DISABLE: Stop since the feature is being disabled
 */
typedef enum {
    SMART_LOG_FW_PKTLOG_STOP_NORMAL,
    SMART_LOG_FW_PKTLOG_STOP_HOSTREQ,
    SMART_LOG_FW_PKTLOG_STOP_DISABLE,
} smart_log_fw_pktlog_stop_reason_t;

static QDF_STATUS smart_log_fw_pktlog_stop_helper(
        struct ol_ath_softc_net80211 *scn,
        smart_log_fw_pktlog_stop_reason_t reason);
#endif /* REMOVE_PKT_LOG */

/*
 * API to enable/disable the smartlogging feature, if enabled
 * all fw logs, smartlog events are stored in smart_log_file
 */
int32_t
ol_ath_enable_smart_log(struct ol_ath_softc_net80211 *scn, uint32_t cfg)
{
    void *wmi_handle = NULL;
    struct ieee80211com *ic = &(scn->sc_ic);
    struct target_psoc_info *tgt_psoc_info;
    void *dbglog_handle = NULL;

    tgt_psoc_info = (struct target_psoc_info *)wlan_psoc_get_tgt_if_handle(
            scn->soc->psoc_obj);
    dbglog_handle = target_psoc_get_dbglog_hdl(tgt_psoc_info);

    wmi_handle = lmac_get_wmi_hdl(scn->soc->psoc_obj);

    if (!wmi_handle)
        return 0;

    if (!wmi_service_enabled(wmi_handle, wmi_service_smart_logging_support)) {
        qdf_info("Smart Logs not supported");
        return 0;
    }

    if (cfg == 1) {
        if (ic->smart_logging == 1) {
            qdf_info("\nAlready Enabled");
        }
        else {
            if (ol_target_lithium(scn->soc->psoc_obj)) {
                qdf_info("Not supported in lithium platform");
            } else {
                /* Enable fwdbg_smartlog_init */
                if (dbglog_handle &&
                    (fwdbg_smartlog_init(dbglog_handle, ic)) == 0) {
                    if (ic->smart_logging == 1)
                        wmi_unified_send_smart_logging_enable_cmd(wmi_handle,
                                                                  cfg);
                }
            }
        }
    } else if (cfg == 0) {
        if (ic->smart_logging == 0) {
            qdf_info("\nAlready Disabled");
        }
        else {
            if(dbglog_handle) {
                if (ic->smart_logging_p1pingfail_started) {
                   qdf_info("\nP1 ping failure logging in progress. Stopping "
                            "it.");

                   if ((send_fatal_cmd(scn,
                          WMI_HOST_FATAL_CONDITION_CONNECTION_ISSUE,
                          WMI_HOST_FATAL_SUBTYPE_P1_PING_FAILURE_STOP_DEBUG)) !=
                          QDF_STATUS_SUCCESS) {
                        qdf_err("Error: Could not successfully send command to "
                                "stop P1 ping failure logging. Smart logging "
                                "or rest of system may no longer function as "
                                "expected. Investigate!!");
                        qdf_err("Marking P1 ping failure as stopped within "
                                "host. This may not be consistent with FW "
                                "state due to command send failure.");
                   }

                   ic->smart_logging_p1pingfail_started = false;
                }

                /* We do not use the wrapper API exported to other modules to
                 * stop logging because of the need for customized error prints
                 * and also to avoid any inapplicable book-keeping that might be
                 * added to the API.
                 */
                if (ic->smart_logging_connection_fail_started) {
                   qdf_info("\nConnection failure logging in progress. "
                             "Stopping it.");
                   if ((send_fatal_cmd(scn,
                           WMI_HOST_FATAL_CONDITION_CONNECTION_ISSUE,
                           WMI_HOST_FATAL_SUBTYPE_CONNECTION_FAILURE_STOP_DEBUG))
                           != QDF_STATUS_SUCCESS) {
                        qdf_err("Error: Could not successfully send command to "
                                "stop connection failure logging. Smart "
                                "logging or rest of system may no longer "
                                "function as expected. Investigate!!");
                        qdf_err("Marking connection failure logging as stopped "
                                "within host. This may not be consistent with "
                                "FW state due to command send failure.");
                   }
                   ic->smart_logging_connection_fail_started = false;
                }

                wmi_unified_send_smart_logging_enable_cmd(wmi_handle, cfg);
                fwdbg_smartlog_deinit(dbglog_handle, scn->soc);

            }
        }
    }

    return 0;
}
EXPORT_SYMBOL(ol_ath_enable_smart_log);
/*
 * This api is used to send Smartlog event to the FW.
 * It is used when a fatal condition is detected in the host.
 * Once it is sent the FW reseponds by sending required logs
 * to host and sends a FATAL_COMPLETION_EVENT once its done.
 */
QDF_STATUS send_fatal_cmd(struct ol_ath_softc_net80211 *scn, uint32_t type,
        uint32_t subtype)
{
    struct wmi_debug_fatal_events fevent;
    void *wmi_handle = NULL;
    struct ieee80211com *ic = &(scn->sc_ic);

    if (ic->smart_logging == 0) {
        qdf_info("Failed to send cmd, Smart Logs not enabled");
        return QDF_STATUS_E_INVAL;
    }

    wmi_handle = lmac_get_wmi_hdl(scn->soc->psoc_obj);

    if (!wmi_handle)
        return QDF_STATUS_E_FAILURE;

    fevent.num_events = 1;
    fevent.event[0].type = type;
    fevent.event[0].subtype = subtype;
    fevent.event[0].reserved0 = 0;

    return wmi_unified_send_smart_logging_fatal_cmd(wmi_handle, &fevent);
}
EXPORT_SYMBOL(send_fatal_cmd);

/**
 * @brief Start connection failure smart logging
 *
 * @param scn - pointer to struct ol_ath_softc_net80211
 *
 * @return QDF_STATUS_SUCCESS on success or if the API is inapplicable (since
 * smart logging is disabled, or connection failure smart logging has already
 * been started), other QDF_STATUS_* value on error (an error message will be
 * printed).
 */
QDF_STATUS ol_smart_log_connection_fail_start(struct ol_ath_softc_net80211 *scn)
{
    QDF_STATUS ret = 0;
    struct ieee80211com *ic = NULL;

    qdf_assert_always(scn != NULL);

    ic = &(scn->sc_ic);
    qdf_assert_always(ic != NULL);

    if (!ic->smart_logging || ic->smart_logging_connection_fail_started) {
        /* Silently return success */
        return QDF_STATUS_SUCCESS;
    }

    ret = send_fatal_cmd(scn,
            WMI_HOST_FATAL_CONDITION_CONNECTION_ISSUE,
            WMI_HOST_FATAL_SUBTYPE_CONNECTION_FAILURE_START_DEBUG);

    if (ret != QDF_STATUS_SUCCESS) {
        qdf_err("Error %d: Could not successfully send command to start "
                "connection failure logging. Smart logging or rest of system "
                "may no longer function as expected. Investigate!!", ret);
        qdf_err("Preserving connection failure logging status as stopped "
                "within host. This may or may not be consistent with FW state "
                "due to command send failure.");
    } else {
        ic->smart_logging_connection_fail_started = true;
    }

    return ret;
}
EXPORT_SYMBOL(ol_smart_log_connection_fail_start);

/**
 * @brief Stop connection failure smart logging
 *
 * @param scn - pointer to struct ol_ath_softc_net80211
 *
 * @return QDF_STATUS_SUCCESS on success or if the API is inapplicable (since
 * smart logging is disabled, or connection failure smart logging has already
 * been stopped), other QDF_STATUS_* value on error (an error message will be
 * printed).
 */
QDF_STATUS ol_smart_log_connection_fail_stop(struct ol_ath_softc_net80211 *scn)
{
    QDF_STATUS ret = 0;
    struct ieee80211com *ic = NULL;

    qdf_assert_always(scn != NULL);

    ic = &(scn->sc_ic);
    qdf_assert_always(ic != NULL);

    if (!ic->smart_logging || !ic->smart_logging_connection_fail_started) {
        /* Silently return success.
         * In case the user stops smart logging while connection failure smart
         * logging is in progress, the smart logging de-init routines will take
         * care of automatically stopping connection failure smart logging. Our
         * API need not bother about this.
         */
        return QDF_STATUS_SUCCESS;
    }

    ret = send_fatal_cmd(scn,
            WMI_HOST_FATAL_CONDITION_CONNECTION_ISSUE,
            WMI_HOST_FATAL_SUBTYPE_CONNECTION_FAILURE_STOP_DEBUG);

    if (ret != QDF_STATUS_SUCCESS) {
        qdf_err("Error %d: Could not successfully send command to stop "
                "connection failure logging. Smart logging or rest of system "
                "may no longer function as expected. Investigate!!", ret);
        qdf_err("Marking connection failure logging status as stopped within "
                "host. This may not be consistent with FW state due to command "
                "send failure.");
    }

    ic->smart_logging_connection_fail_started = false;

    return ret;
}
EXPORT_SYMBOL(ol_smart_log_connection_fail_stop);

#ifndef REMOVE_PKT_LOG
/**
 * @brief Internal helper function to stop smart log FW packetlog
 * @details
 *  This is a smart log internal helper function. Do not call this directly. Use
 *  the smart log functions/work queue handlers registered to ic. Smart log
 *  functions/work queue handlers registered to ic must obtain necessary locking
 *  before calling this function.
 *
 * @param scn - pointer to struct ol_ath_softc_net80211
 * @param reason - reason for the stop request
 *
 * @return QDF_STATUS_SUCCESS on success, other QDF_STATUS_* value on error (an
 * error message will be printed).
 */
static QDF_STATUS smart_log_fw_pktlog_stop_helper(
        struct ol_ath_softc_net80211 *scn,
        smart_log_fw_pktlog_stop_reason_t reason)
{
    struct ieee80211com *ic = NULL;
#if UMAC_SUPPORT_ACFG
    acfg_event_data_t *acfg_event = NULL;
#endif /* UMAC_SUPPORT_ACFG */
    QDF_STATUS ret = 0;
    int pktlog_ret = 0;

    qdf_assert_always(scn != NULL);

    ic = &(scn->sc_ic);

    if (!scn->pl_dev) {
        qdf_info("Packetlog pl_dev unavailable. Ignoring FW packetlog stop "
                 "request.");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }

    if (!ic->smart_logging) {
        qdf_info("Smart logging not enabled. Ignoring FW packetlog stop "
                 "request.");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }

    if (!ic->smart_log_fw_pktlog_enabled) {
        qdf_info("Smart log FW initiated packetlog not enabled. Ignoring FW "
                 "packetlog stop request. Investigate!");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }

    if (ic->smart_log_fw_pktlog_blocked) {
        qdf_info("Smart log FW initiated packetlog activity is blocked due to "
                 "host initiated packetlog (or since packetlog functionality "
                 "is not currently available). Ignoring FW packetlog stop "
                 "request.");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }

    if (!ic->smart_log_fw_pktlog_started) {
        qdf_info("FW initiated packet logging not in progress. Ignoring FW "
                 "packetlog stop request.");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }

#if UMAC_SUPPORT_ACFG
        acfg_event = (acfg_event_data_t *)
                        qdf_mem_malloc(sizeof(acfg_event_data_t));
        if (acfg_event == NULL) {
            qdf_err("Could not allocate memory for acfg event.");
            ret = QDF_STATUS_E_NOMEM;
            goto done;
        }
#endif /* UMAC_SUPPORT_ACFG */

    if ((pktlog_ret = scn->pl_dev->pl_funcs->pktlog_enable(scn, 0)) == 0) {
#if UMAC_SUPPORT_ACFG
        switch(reason) {
            case SMART_LOG_FW_PKTLOG_STOP_NORMAL:
                acfg_event->reason = ACFG_SMART_LOG_FW_PKTLOG_STOP_NORMAL;
                break;
            case SMART_LOG_FW_PKTLOG_STOP_HOSTREQ:
                acfg_event->reason = ACFG_SMART_LOG_FW_PKTLOG_STOP_HOSTREQ;
                break;
            case SMART_LOG_FW_PKTLOG_STOP_DISABLE:
                acfg_event->reason = ACFG_SMART_LOG_FW_PKTLOG_STOP_DISABLE;
                break;
            default:
                qdf_err("Unknown smart log FW initiated packetlog stop reason "
                        "%u. Asserting!", reason);
                qdf_assert_always(0);
        }

        acfg_send_event(scn->sc_osdev->netdev, scn->sc_osdev,
                    WL_EVENT_TYPE_SMART_LOG_FW_PKTLOG_STOP, acfg_event);
#endif /* UMAC_SUPPORT_ACFG */

        ret = QDF_STATUS_SUCCESS;
    } else {
        qdf_err("Could not successfully stop FW initiated packetlog (ret=%d). "
                "FW may be down, or there might be an unexpected error.",
                pktlog_ret);
        qdf_err("In case of unexpected error: Smart logging or rest of system "
                "may no longer function as expected. Investigate! Marking FW "
                "initiated packetlog as stopped within host. This may not be "
                "consistent with FW state.");
        ret = QDF_STATUS_E_FAILURE;
    }

    ic->smart_log_fw_pktlog_started = false;

done:
#if UMAC_SUPPORT_ACFG
    if (acfg_event != NULL) {
        qdf_mem_free(acfg_event);
    }
#endif /* UMAC_SUPPORT_ACFG */

    return ret;
}

/**
 * @brief Enable smart log FW initiated packetlog
 * @details
 *  This only enables the feature. It does not start a FW initiated packetlog.
 *
 * @param scn - pointer to struct ol_ath_softc_net80211
 *
 * @return QDF_STATUS_SUCCESS on success, other QDF_STATUS_* value on error (an
 * error message will be printed).
 */
QDF_STATUS ol_smart_log_fw_pktlog_enable(struct ol_ath_softc_net80211 *scn)
{
    struct ieee80211com *ic = NULL;
    QDF_STATUS ret = 0;

    qdf_assert_always(scn != NULL);

    ic = &(scn->sc_ic);

    qdf_mutex_acquire(&ic->smart_log_fw_pktlog_mutex);

    if (!ic->smart_logging) {
        qdf_info("Smart logging not enabled. Ignoring FW packetlog enable "
                 "request.");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }

    ic->smart_log_fw_pktlog_enabled = true;
    ret = QDF_STATUS_SUCCESS;

done:
    qdf_mutex_release(&ic->smart_log_fw_pktlog_mutex);
    return ret;
}
EXPORT_SYMBOL(ol_smart_log_fw_pktlog_enable);

/**
 * @brief Disable smart log FW initiated packetlog
 * @details
 *  In case a FW initiated packetlog is in progress, it will be stopped as part
 *  of the disable sequence.
 *
 * @param scn - pointer to struct ol_ath_softc_net80211
 *
 * @return QDF_STATUS_SUCCESS on success, other QDF_STATUS_* value on error (an
 * error message will be printed).
 */
QDF_STATUS ol_smart_log_fw_pktlog_disable(struct ol_ath_softc_net80211 *scn)
{
    struct ieee80211com *ic = NULL;
    QDF_STATUS ret = 0;

    qdf_assert_always(scn != NULL);

    ic = &(scn->sc_ic);

    qdf_mutex_acquire(&ic->smart_log_fw_pktlog_mutex);

    /* We don't check for ic->smart_logging since a disable request could arrive
     * even if ic->smart_logging is not true.
     */

    if (!ic->smart_log_fw_pktlog_enabled) {
        qdf_info("Smart log FW initiated packetlog not enabled. Ignoring FW "
                 "packetlog disable request.");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }

    if (scn->pl_dev && ic->smart_logging &&
            !ic->smart_log_fw_pktlog_blocked &&
            ic->smart_log_fw_pktlog_started &&
           ((ret = smart_log_fw_pktlog_stop_helper(scn,
                        SMART_LOG_FW_PKTLOG_STOP_DISABLE))
                != QDF_STATUS_SUCCESS)) {
        qdf_err("Could not successfully stop FW initiated packet logging. "
                "However, FW initiated packetlog has been marked as disabled "
                "and future usage would be prevented until explicitly "
                "enabled.");
        goto done;
    }

    ret = QDF_STATUS_SUCCESS;

done:
    /* We mark our module as disabled, under all circumstances */
    ic->smart_log_fw_pktlog_enabled = false;
    qdf_mutex_release(&ic->smart_log_fw_pktlog_mutex);
    return ret;
}
EXPORT_SYMBOL(ol_smart_log_fw_pktlog_disable);

/**
 * @brief Start smart log FW initiated packetlog
 * @details
 *  This API is intended to be used only by the WMI event handler processing a
 *  FW request to start packetlog.
 *
 * @param scn - pointer to struct ol_ath_softc_net80211
 * @param fw_pktlog_types - packetlog filters (WMI)
 *
 * @return QDF_STATUS_SUCCESS on success, other QDF_STATUS_* value on error (an
 * error message will be printed).
 */
QDF_STATUS ol_smart_log_fw_pktlog_start(struct ol_ath_softc_net80211 *scn,
        u_int32_t fw_pktlog_types)
{
    struct ieee80211com *ic = NULL;
    struct wlan_objmgr_psoc *psoc = NULL;
    /* host_pktlog_types is int32_t since this is the argument accepted by
     * pktlog_enable()
     */
    int32_t host_pktlog_types = 0;
    QDF_STATUS ret = 0;
    int pktlog_ret = 0;

    qdf_assert_always(scn != NULL);

    ic = &(scn->sc_ic);

    qdf_assert_always(ic->ic_pdev_obj != NULL);

    psoc = wlan_pdev_get_psoc(ic->ic_pdev_obj);
    qdf_assert_always(psoc != NULL);

    qdf_mutex_acquire(&ic->smart_log_fw_pktlog_mutex);

    if (!scn->pl_dev) {
        qdf_info("Packetlog pl_dev unavailable. Ignoring FW packetlog start "
                 "request.");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }

    if (!ic->smart_logging) {
        qdf_info("Smart logging not enabled. Ignoring FW packetlog start "
                 "request.");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }

    if (!ic->smart_log_fw_pktlog_enabled) {
        qdf_info("Smart log FW initiated packetlog not enabled. Ignoring FW "
                 "packetlog start request. Investigate!");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }

    if (ic->smart_log_fw_pktlog_blocked) {
        qdf_info("Smart log FW initiated packetlog is blocked due to host "
                 "initiated packetlog (or since packetlog functionality is not "
                 "currently available). Ignoring FW packetlog start request.");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }

#if UNIFIED_SMARTANTENNA
    if (target_if_sa_api_get_sa_enable(psoc)) {
         qdf_info("Host initiated smart antenna packet logging already in "
                  "progress. Ignoring FW packetlog start request.");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }
#endif

    if (ic->smart_log_fw_pktlog_started) {
        qdf_info("FW initiated packet logging already in progress. Ignoring "
                 "new FW packetlog start request.");
        ret = QDF_STATUS_E_INVAL;
        goto done;
    }

    /* The below correspond to the mapping in pktlog_enable_tgt() */

    if (fw_pktlog_types & WMI_HOST_PKTLOG_EVENT_RX) {
        host_pktlog_types |= ATH_PKTLOG_RX;
    }

    if (fw_pktlog_types & WMI_HOST_PKTLOG_EVENT_TX) {
        host_pktlog_types |= ATH_PKTLOG_TX;
    }

    if (fw_pktlog_types & WMI_HOST_PKTLOG_EVENT_RCF) {
        host_pktlog_types |= ATH_PKTLOG_RCFIND;
    }

    if (fw_pktlog_types & WMI_HOST_PKTLOG_EVENT_RCU) {
        host_pktlog_types |= ATH_PKTLOG_RCUPDATE;
    }

    if(fw_pktlog_types & WMI_HOST_PKTLOG_EVENT_DBG_PRINT) {
        host_pktlog_types |= ATH_PKTLOG_DBG_PRINT;
    }

    if(fw_pktlog_types & WMI_HOST_PKTLOG_EVENT_SMART_ANTENNA) {
        qdf_warn("Unexpected packet log type "
                 "WMI_HOST_PKTLOG_EVENT_SMART_ANTENNA. This should be used "
                 "only along with the Smart Antenna module. Ignoring.");
    }

    if (fw_pktlog_types & WMI_HOST_PKTLOG_EVENT_H_INFO) {
        host_pktlog_types |= ATH_PKTLOG_H_INFO;
    }

    if (fw_pktlog_types & WMI_HOST_PKTLOG_EVENT_STEERING) {
        host_pktlog_types |= ATH_PKTLOG_STEERING;
    }

    /*
     * Note: Apply restrictions for WMI_HOST_PKTLOG_EVENT_TX_DATA_CAPTURE if
     * required in the future. However, for now FW is expected to know what it
     * is doing in this particular area.
     */
    if (fw_pktlog_types & WMI_HOST_PKTLOG_EVENT_TX_DATA_CAPTURE) {
        host_pktlog_types |= ATH_PKTLOG_TX_CAPTURE_ENABLE;
    }

    if ((pktlog_ret =
           scn->pl_dev->pl_funcs->pktlog_enable(scn, host_pktlog_types)) == 0) {
        ic->smart_log_fw_pktlog_started = true;
        ret = QDF_STATUS_SUCCESS;
    } else {
        qdf_err("Could not successfully start FW initiated packetlog (ret=%d). "
                "Smart logging or rest of system may no longer function as "
                "expected. Investigate!", pktlog_ret);
        qdf_err("Maintaining FW initiated packetlog status as stopped within "
                "host. This may or may not be consistent with FW state.");
        ret = QDF_STATUS_E_FAILURE;
    }

done:
    qdf_mutex_release(&ic->smart_log_fw_pktlog_mutex);
    return ret;
}
EXPORT_SYMBOL(ol_smart_log_fw_pktlog_start);

/**
 * @brief Stop smart log FW packetlog
 * @details
 *  This API is intended to be used only by the WMI event handler processing a
 *  FW request to stop packetlog.
 *
 * @param scn - pointer to struct ol_ath_softc_net80211
 *
 * @return QDF_STATUS_SUCCESS on success, other QDF_STATUS_* value on error (an
 * error message will be printed by the helper function used by this function).
 */
QDF_STATUS ol_smart_log_fw_pktlog_stop(struct ol_ath_softc_net80211 *scn)
{
    struct ieee80211com *ic = NULL;
    QDF_STATUS ret = 0;

    qdf_assert_always(scn != NULL);

    ic = &(scn->sc_ic);

    qdf_mutex_acquire(&ic->smart_log_fw_pktlog_mutex);
    ret = smart_log_fw_pktlog_stop_helper(scn, SMART_LOG_FW_PKTLOG_STOP_NORMAL);
    qdf_mutex_release(&ic->smart_log_fw_pktlog_mutex);

    return ret;
}
EXPORT_SYMBOL(ol_smart_log_fw_pktlog_stop);

/**
 * @brief Stop and block smart log FW packetlog
 * @details
 *  This API should be called by any host module which wants to start/stop
 *  packetlog. The API will check if FW initiated packetlog is in progress, and
 *  if so print an appropriate message and stop the FW initiated packetlog. It
 *  will also block future FW initiated packetlog activity based on the caller's
 *  instruction on whether to block under all circumstances or block only if FW
 *  initiated packetlog was in progress. Even if a host module intends to stop
 *  packetlog, it should call this API. The subsequent call made by the host
 *  module to stop packetlog will be a no-op in this case, but this helps to
 *  achieve a uniform flow and ensure appropriate information is printed for the
 *  benefit of the user.
 *
 * @param scn - pointer to struct ol_ath_softc_net80211
 * @param host_pktlog_types - host packetlog filter
 * @param block_only_if_started - block only if FW initiated packetlog had been
 * started (will not take effect if already blocked)
 *
 * @return QDF_STATUS_SUCCESS (this will be returned in case this API is
 * inapplicable, as well), other QDF_STATUS_* value on error (an error message
 * will be printed).
 */
QDF_STATUS ol_smart_log_fw_pktlog_stop_and_block(
        struct ol_ath_softc_net80211 *scn,
        int32_t host_pktlog_types, bool block_only_if_started)
{
    struct ieee80211com *ic = NULL;
    bool fw_pktlog_was_started = false;
    QDF_STATUS ret = 0;

    qdf_assert_always(scn != NULL);

    ic = &(scn->sc_ic);

    qdf_mutex_acquire(&ic->smart_log_fw_pktlog_mutex);

    if (scn->pl_dev && ic->smart_logging &&
            ic->smart_log_fw_pktlog_enabled &&
            !ic->smart_log_fw_pktlog_blocked &&
            ic->smart_log_fw_pktlog_started) {
        if (!host_pktlog_types) {
            qdf_info("Stopping FW initiated packetlog due to host initiated "
                     "stop request.");
        } else {
            qdf_info("Stopping FW initiated packetlog due to host initiated "
                     "start request.");
        }

        fw_pktlog_was_started = true;

        if ((ret = smart_log_fw_pktlog_stop_helper(scn,
                        SMART_LOG_FW_PKTLOG_STOP_HOSTREQ))
                != QDF_STATUS_SUCCESS) {
            qdf_err("Could not successfully stop FW initiated packet logging. "
                    "Investigate!");
            qdf_err("However, FW initiated packetlog has been marked as "
                    "blocked and future usage would be prevented until "
                    "explicitly unblocked.");
            ret = QDF_STATUS_E_FAILURE;
            goto done;
        }
    }

    ret = QDF_STATUS_SUCCESS;

done:
    if (!block_only_if_started) {
        ic->smart_log_fw_pktlog_blocked = true;
    } else if (fw_pktlog_was_started) {
        ic->smart_log_fw_pktlog_blocked = true;
    }
    qdf_mutex_release(&ic->smart_log_fw_pktlog_mutex);
    return ret;
}
EXPORT_SYMBOL(ol_smart_log_fw_pktlog_stop_and_block);

/**
 * @brief Unblock smart log FW packetlog
 *
 * @param scn - pointer to struct ol_ath_softc_net80211
 */
void ol_smart_log_fw_pktlog_unblock(struct ol_ath_softc_net80211 *scn)
{
    struct ieee80211com *ic = NULL;

    qdf_assert_always(scn != NULL);

    ic = &(scn->sc_ic);

    qdf_mutex_acquire(&ic->smart_log_fw_pktlog_mutex);

     /* Currently, we silently accept an unblock request under all
      * circumstances.
      */
    ic->smart_log_fw_pktlog_blocked = false;
    qdf_mutex_release(&ic->smart_log_fw_pktlog_mutex);
}
EXPORT_SYMBOL(ol_smart_log_fw_pktlog_unblock);
#endif /* REMOVE_PKT_LOG */
#endif /* OL_ATH_SMART_LOGGING */

u_int8_t
tpc_config_get_rate_tpc(wmi_host_pdev_tpc_config_event *ev, u_int32_t rate_idx, u_int32_t num_chains, u_int8_t rate_code, u_int8_t type)
{
    u_int8_t tpc = ev->ratesArray[rate_idx];
    u_int8_t num_streams;
    u_int8_t preamble;
    u_int8_t chain_idx;

    num_streams = 1 + AR600P_GET_HW_RATECODE_NSS(rate_code);
    preamble = AR600P_GET_HW_RATECODE_PREAM(rate_code);
    chain_idx = num_chains - 1;

    /*
     * find TPC based on the target power for that rate and the maximum
     * allowed regulatory power based on the number of tx chains.
     */
    if (chain_idx < WMI_HOST_TPC_TX_NUM_CHAIN)
        tpc = A_MIN(ev->ratesArray[rate_idx], ev->maxRegAllowedPower[chain_idx]);

    if (ev->numTxChain > 1) {
        /*
         * Apply array gain factor for non-cck frames and when
         * num of chains used is more than the number of streams
         */
        if (preamble != AR600P_HW_RATECODE_PREAM_CCK) {
            u_int8_t stream_idx;

            stream_idx = num_streams - 1;
            if (type == TPC_TABLE_TYPE_STBC) {
                if (num_chains > num_streams) {
                    tpc = A_MIN(tpc, ev->maxRegAllowedPowerAGSTBC[chain_idx - 1][stream_idx]);
                }
            } else if (type == TPC_TABLE_TYPE_TXBF) {
                if (num_chains > num_streams) {
                    tpc = A_MIN(tpc, ev->maxRegAllowedPowerAGTXBF[chain_idx - 1][stream_idx]);
                }
            } else {
                if (num_chains > num_streams) {
                    tpc = A_MIN(tpc, ev->maxRegAllowedPowerAGCDD[chain_idx - 1][stream_idx]);
                }
            }
        }
    }

    return tpc;
}

void
tpc_config_disp_tables(wmi_host_pdev_tpc_config_event *ev, u_int8_t *rate_code, u_int16_t *pream_table, u_int8_t type)
{
    u_int32_t i, j;
    u_char table_str[3][5] =  {
        "CDD ",
        "STBC",
        "TXBF"
    };
    u_char pream_str[8][6] = {
        "CCK  ",
        "OFDM ",
        "HT20 ",
        "HT40 ",
        "VHT20",
        "VHT40",
        "VHT80",
        "HTDUP"
    };
    u_int32_t pream_idx;
    u_int8_t tpc[IEEE80211_MAX_TX_CHAINS];
    switch (type) {
        case TPC_TABLE_TYPE_CDD:
            if (!(ev->flags & WMI_HOST_TPC_CONFIG_EVENT_FLAG_TABLE_CDD)) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("%s : CDD not supported \n",__func__));
                return;
            }
            break;
        case TPC_TABLE_TYPE_STBC:
            if (!(ev->flags & WMI_HOST_TPC_CONFIG_EVENT_FLAG_TABLE_STBC)) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("%s : STBC not supported \n",__func__));
                return;
            }
            break;
        case TPC_TABLE_TYPE_TXBF:
            if (!(ev->flags & WMI_HOST_TPC_CONFIG_EVENT_FLAG_TABLE_TXBF)) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("%s : TXBF not supported \n",__func__));
                return;
            }
            break;
        default:
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("%s : Invalid type %d \n",__func__, type));
            return;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("**************** %s POWER TABLE ****************\n",table_str[type])) ;
    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("**************************************************\n"));

    pream_idx = 0;
    for(i = 0;i < ev->rateMax; i++) {
	    char tpc_values[IEEE80211_MAX_TX_CHAINS * 20] = "";
	    char buff[20] = "";

	    if (i == pream_table[pream_idx]) {
		    pream_idx++;
	    }

	    for(j = 0; j < IEEE80211_MAX_TX_CHAINS; j++) {

		    if(j < ev->numTxChain) {
			    tpc[j] = tpc_config_get_rate_tpc(ev, i, j + 1, rate_code[i], type);
			    snprintf(buff, sizeof(buff), "%8d ", tpc[j]);
			    strlcat(tpc_values, buff, sizeof(tpc_values));
		    }
		    else {
			    break;
		    }

	    }
	    AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("%8d %s 0x%2x %s\n", i, pream_str[pream_idx], rate_code[i], tpc_values));
    }
    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("**************************************************\n"));

    return;
}

void tpc_config_event_handler(ol_scn_t sc, u_int8_t *data, u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    wmi_host_pdev_tpc_config_event event, *ev = &event;
    u_int32_t i, j;
    u_int8_t rate_code[200];
    u_int16_t pream_table[10];
    u_int8_t rate_idx;
    u_int32_t pream_idx;
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);

    if (wmi_extract_pdev_tpc_config_ev_param(wmi_handle, data, ev)) {
        return;
    }

#ifdef BIG_ENDIAN_HOST
    {
        /*
         * Target is in little endian, copy engine interface will
         * swap at the dword boundary. Re-swap the byte stream
         * arrays
         */
        u_int32_t *destp, *srcp;
        u_int32_t len;

        srcp = (u_int32_t *)&ev->maxRegAllowedPower[0];
        destp = (u_int32_t *)&ev->maxRegAllowedPower[0];
        len = sizeof(wmi_host_pdev_tpc_config_event) - offsetof(wmi_host_pdev_tpc_config_event, maxRegAllowedPower);
        for(i=0; i < (roundup(len, sizeof(u_int32_t))/4); i++) {
            *destp = le32_to_cpu(*srcp);
            destp++; srcp++;
        }
    }
#endif

    /* Create the rate code table based on the chains supported */
    rate_idx = 0;
    pream_idx = 0;

    /* Fill CCK rate code */
    for (i=0;i<4;i++) {
        rate_code[rate_idx] = AR600P_ASSEMBLE_HW_RATECODE(i, 0, AR600P_HW_RATECODE_PREAM_CCK);
        rate_idx++;
    }
    pream_table[pream_idx] = rate_idx;
    pream_idx++;

    /* Fill OFDM rate code */
    for (i=0;i<8;i++) {
        rate_code[rate_idx] = AR600P_ASSEMBLE_HW_RATECODE(i, 0, AR600P_HW_RATECODE_PREAM_OFDM);
        rate_idx++;
    }
    pream_table[pream_idx] = rate_idx;
    pream_idx++;

    /* Fill HT20 rate code */
    for (i=0;i<ev->numTxChain;i++) {
        for (j=0;j<8;j++) {
            rate_code[rate_idx] = AR600P_ASSEMBLE_HW_RATECODE(j, i, AR600P_HW_RATECODE_PREAM_HT);
            rate_idx++;
        }
    }
    pream_table[pream_idx] = rate_idx;
    pream_idx++;

    /* Fill HT40 rate code */
    for (i=0;i<ev->numTxChain;i++) {
        for (j=0;j<8;j++) {
            rate_code[rate_idx] = AR600P_ASSEMBLE_HW_RATECODE(j, i, AR600P_HW_RATECODE_PREAM_HT);
            rate_idx++;
        }
    }
    pream_table[pream_idx] = rate_idx;
    pream_idx++;

    /* Fill VHT20 rate code */
    for (i=0;i<ev->numTxChain;i++) {
        for (j=0;j<10;j++) {
            rate_code[rate_idx] = AR600P_ASSEMBLE_HW_RATECODE(j, i, AR600P_HW_RATECODE_PREAM_VHT);
            rate_idx++;
        }
    }
    pream_table[pream_idx] = rate_idx;
    pream_idx++;

    /* Fill VHT40 rate code */
    for (i=0;i<ev->numTxChain;i++) {
        for (j=0;j<10;j++) {
            rate_code[rate_idx] = AR600P_ASSEMBLE_HW_RATECODE(j, i, AR600P_HW_RATECODE_PREAM_VHT);
            rate_idx++;
        }
    }
    pream_table[pream_idx] = rate_idx;
    pream_idx++;

    /* Fill VHT80 rate code */
    for (i=0;i<ev->numTxChain;i++) {
        for (j=0;j<10;j++) {
            rate_code[rate_idx] = AR600P_ASSEMBLE_HW_RATECODE(j, i, AR600P_HW_RATECODE_PREAM_VHT);
            rate_idx++;
        }
    }
    pream_table[pream_idx] = rate_idx;
    pream_idx++;

    rate_code[rate_idx++] = AR600P_ASSEMBLE_HW_RATECODE(0, 0, AR600P_HW_RATECODE_PREAM_CCK);
    rate_code[rate_idx++] = AR600P_ASSEMBLE_HW_RATECODE(0, 0, AR600P_HW_RATECODE_PREAM_OFDM);
    rate_code[rate_idx++] = AR600P_ASSEMBLE_HW_RATECODE(0, 0, AR600P_HW_RATECODE_PREAM_CCK);
    rate_code[rate_idx++] = AR600P_ASSEMBLE_HW_RATECODE(0, 0, AR600P_HW_RATECODE_PREAM_OFDM);
    rate_code[rate_idx++] = AR600P_ASSEMBLE_HW_RATECODE(0, 0, AR600P_HW_RATECODE_PREAM_OFDM);

    /* use 0xFFFF to indicate end of table */
    pream_table[pream_idx] = 0xFFFF;

    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("**************************************************\n"));
    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("TPC Config for channel %4d mode %2d \n", ev->chanFreq, ev->phyMode));
    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("**************************************************\n"));

    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("CTL           = 0x%2x   Reg. Domain           = %2d \n", ev->ctl, ev->regDomain));
    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("Antenna Gain  = %2d     Reg. Max Antenna Gain = %2d \n", ev->twiceAntennaGain, ev->twiceAntennaReduction));
    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("Power Limit   = %2d     Reg. Max Power        = %2d \n", ev->powerLimit, ev->twiceMaxRDPower));
    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("Num tx chains = %2d    Num  Supported Rates  = %2d \n", ev->numTxChain, ev->rateMax));
    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("**************************************************\n"));

    tpc_config_disp_tables(ev, rate_code, pream_table, TPC_TABLE_TYPE_CDD);
    tpc_config_disp_tables(ev, rate_code, pream_table, TPC_TABLE_TYPE_STBC);
    tpc_config_disp_tables(ev, rate_code, pream_table, TPC_TABLE_TYPE_TXBF);

}
qdf_export_symbol(tpc_config_event_handler);
