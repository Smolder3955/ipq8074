/*
 * Copyright (c) 2012, 2015, 2017-2019 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2012, 2015 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef REMOVE_PKT_LOG
#include "qdf_mem.h"
#include "athdefs.h"
#include "hif.h"
#include "target_type.h"
#include "ath_pci.h"
#include "a_debug.h"
#include <pktlog_ac_i.h>
#include <linux_remote_pktlog.h>
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include "osif_nss_wifiol_if.h"
#endif
#include "cdp_txrx_ctrl.h"
#if UNIFIED_SMARTANTENNA
#include "target_if_sa_api.h"
#endif
#include "init_deinit_lmac.h"

static struct ath_pktlog_info *g_pktlog_info = NULL;
static int g_pktlog_mode = PKTLOG_MODE_SYSTEM;
extern int ol_ath_target_start(ol_ath_soc_softc_t *soc);

void pktlog_init(struct ol_ath_softc_net80211 *scn);
int pktlog_enable(struct ol_ath_softc_net80211 *scn, int32_t log_state);
int pktlog_filter_mac(struct ol_ath_softc_net80211 *scn, char *macaddr);
int pktlog_setsize(struct ol_ath_softc_net80211 *scn, int32_t log_state);
int pktlog_disable(struct ol_ath_softc_net80211 *scn);
void pktlog_deinit(struct ol_ath_softc_net80211 *scn);
int pktlog_text(struct ol_ath_softc_net80211 *scn, char *text);
//extern void pktlog_disable_adapter_logging(void);
//extern void pktlog_disable_adapter_logging(void);
//extern int pktlog_alloc_buf(ol_ath_generic_softc_handle sc,
 //                                struct ath_pktlog_info *pl_info);
//void pktlog_release_buf(struct ath_pktlog_info *pl_info);
//extern void pktlog_release_buf(void *pinfo);

struct ol_pl_arch_dep_funcs ol_pl_funcs = {
    .pktlog_init = pktlog_init,
    .pktlog_enable = pktlog_enable,
    .pktlog_setsize = pktlog_setsize,
    .pktlog_disable = pktlog_disable, //valid for f/w disable
    .pktlog_deinit = pktlog_deinit,
    .pktlog_filter_mac = pktlog_filter_mac,
    .pktlog_text = pktlog_text,
    /*.pktlog_start = pktlog_start,
    .pktlog_readhdr = pktlog_read_hdr,
    .pktlog_readbuf = pktlog_read_buf,*/
};

struct ol_pktlog_dev_t ol_pl_dev = {
    .pl_funcs = &ol_pl_funcs,
};

void ol_pl_sethandle(ol_pktlog_dev_t **pl_handle,
                    struct ol_ath_softc_net80211 *scn)
{
#ifdef OL_ATH_SMART_LOGGING
    ol_ath_soc_softc_t *soc = NULL;
#endif /* OL_ATH_SMART_LOGGING */

    if (scn == NULL) {
        qdf_err("scn is NULL");
        return;
    }

    *pl_handle = (ol_pktlog_dev_t *)qdf_mem_malloc(sizeof(ol_pktlog_dev_t));
    if (*pl_handle == NULL) {
        qdf_err("%s: pktlog initialization failed", __func__);
        return;
    }
    (*pl_handle)->scn = (struct ol_ath_generic_softc_t*)scn;
    (*pl_handle)->sc_osdev = scn->sc_osdev;
    (*pl_handle)->pl_funcs = &ol_pl_funcs;

#ifdef OL_ATH_SMART_LOGGING
    soc = scn->soc;
    if (soc && soc->ol_if_ops) {
        (*pl_handle)->pl_sl_funcs.smart_log_fw_pktlog_stop_and_block =
            soc->ol_if_ops->smart_log_fw_pktlog_stop_and_block;
        (*pl_handle)->pl_sl_funcs.smart_log_fw_pktlog_unblock =
            soc->ol_if_ops->smart_log_fw_pktlog_unblock;
    }
#endif /* OL_ATH_SMART_LOGGING */
}

void ol_pl_freehandle(ol_pktlog_dev_t *pl_handle)
{
    if (pl_handle) {
         qdf_mem_free(pl_handle);
    }
}

static inline A_STATUS pktlog_enable_tgt(
        struct ol_ath_softc_net80211 *_scn,
        uint32_t log_state)
{
    uint32_t types = 0, tgt_type;

    struct ath_pktlog_info *pl_info = NULL;
    struct ol_pktlog_dev_t *pl_dev = NULL;

    if ((_scn == NULL) || (_scn->pl_dev == NULL)) {
         return A_ERROR;
    }

    pl_dev = _scn->pl_dev;
    pl_info = pl_dev->pl_info;

    tgt_type = lmac_get_tgt_type(_scn->soc->psoc_obj);

    if (log_state & ATH_PKTLOG_TX) {
        types |= WMI_HOST_PKTLOG_EVENT_TX;
    }
    if (log_state & ATH_PKTLOG_RX) {
        types |= WMI_HOST_PKTLOG_EVENT_RX;
    }
    if (log_state & ATH_PKTLOG_RCFIND) {
        types |= WMI_HOST_PKTLOG_EVENT_RCF;
    }
    if (log_state & ATH_PKTLOG_RCUPDATE) {
        types |= WMI_HOST_PKTLOG_EVENT_RCU;
    }
#if 0
    if(log_state & ATH_PKTLOG_DBG_PRINT) {
        /* Debuglog buffer size is 1500 bytes. This exceeds the 512 bytes size limit when transferred via HTT.
         * Disable this to avoid target assert.
         */
        types |= WMI_HOST_PKTLOG_EVENT_DBG_PRINT;
    }
#endif

    /* Tx Data Capture is supported only for Legacy */
    if (log_state & ATH_PKTLOG_TX_CAPTURE_ENABLE) {
        if ( (tgt_type == TARGET_TYPE_AR9888)  ||
             (tgt_type == TARGET_TYPE_QCA9984) ||
             (tgt_type == TARGET_TYPE_IPQ4019) ||
             (tgt_type == TARGET_TYPE_QCA9888) ||
             (tgt_type == TARGET_TYPE_AR900B) ) {
            types |= WMI_HOST_PKTLOG_EVENT_TX_DATA_CAPTURE;
        }
    }

    if (tgt_type == TARGET_TYPE_AR900B) {
        if (log_state & ATH_PKTLOG_CBF) {
            types |= WMI_HOST_PKTLOG_EVENT_RX;
        }
        if (log_state & ATH_PKTLOG_H_INFO) {
            types |= WMI_HOST_PKTLOG_EVENT_H_INFO;
        }
        if (log_state & ATH_PKTLOG_STEERING) {
            types |= WMI_HOST_PKTLOG_EVENT_STEERING;
        }
    }

    if(pl_info->peer_based_filter == 1) {
        if (pktlog_wmi_send_peer_based_enable (_scn,pl_info->macaddr))
             return A_ERROR;
    }

    if (types != 0) {
        if (pktlog_wmi_send_enable_cmd(_scn, types)) {
            return A_ERROR;
        }
    }

    return A_OK;
}

static inline A_STATUS wdi_pktlog_subscribe(
        struct ol_pktlog_dev_t *pl_dev, int32_t log_state)
{
    struct ol_ath_softc_net80211 *scn =
        (struct ol_ath_softc_net80211 *)(pl_dev->scn);
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);

    if(pl_dev->pl_info->peer_based_filter == 1) {
        if(cdp_enable_peer_based_pktlog(soc_txrx_handle,
                (struct cdp_pdev *)pdev_txrx_handle,
                pl_dev->pl_info->macaddr ,1)) {
            return A_ERROR;
        }
    }
    if ((log_state & ATH_PKTLOG_TX)       ||
        (log_state & ATH_PKTLOG_RCFIND)   ||
        (log_state & ATH_PKTLOG_RCUPDATE) ||
        (log_state & ATH_PKTLOG_RX)       ||
        (log_state & ATH_PKTLOG_TX_CAPTURE_ENABLE)) {
            if (cdp_wdi_event_sub(soc_txrx_handle,
                    (struct cdp_pdev *)pdev_txrx_handle,
                    &(pl_dev->PKTLOG_OFFLOAD_SUBSCRIBER),
                    WDI_EVENT_OFFLOAD_ALL)) {
                return A_ERROR;
            }
    }

    if (log_state & ATH_PKTLOG_RX) {
        if (cdp_wdi_event_sub(soc_txrx_handle,
                    (struct cdp_pdev *)pdev_txrx_handle,
                    &(pl_dev->PKTLOG_RX_REMOTE_SUBSCRIBER),
                    WDI_EVENT_RX_DESC_REMOTE)) {
            return A_ERROR;
        }

        if (lmac_is_target_ar900b(scn->soc->psoc_obj)) {
            if ((log_state & ATH_PKTLOG_CBF)      ||
                (log_state & ATH_PKTLOG_H_INFO)   ||
                (log_state & ATH_PKTLOG_STEERING)) {
                   if (cdp_wdi_event_sub(soc_txrx_handle,
                            (struct cdp_pdev *)pdev_txrx_handle,
                            &(pl_dev->PKTLOG_RX_CBF_SUBSCRIBER),
                            WDI_EVENT_RX_CBF_REMOTE)) {
                          return A_ERROR;
                   }
            }
        }

        if (cdp_wdi_event_sub(soc_txrx_handle,
                    (struct cdp_pdev *)pdev_txrx_handle,
                    &(pl_dev->PKTLOG_RX_DESC_SUBSCRIBER),
                    WDI_EVENT_RX_DESC)) {
            return A_ERROR;
        }

    }

    if (log_state & ATH_PKTLOG_LITE_T2H) {
        if (cdp_wdi_event_sub(soc_txrx_handle,
                (struct cdp_pdev *)pdev_txrx_handle,
                &(pl_dev->PKTLOG_OFFLOAD_SUBSCRIBER),
                WDI_EVENT_LITE_T2H)) {
            return A_ERROR;
        }
    }

    if (log_state & ATH_PKTLOG_LITE_RX) {
        if (cdp_wdi_event_sub(soc_txrx_handle,
                (struct cdp_pdev *)pdev_txrx_handle,
                &(pl_dev->PKTLOG_RX_DESC_SUBSCRIBER),
                WDI_EVENT_LITE_RX)) {
            return A_ERROR;
        }
    }
    return A_OK;
}

int pktlog_text(struct ol_ath_softc_net80211 *scn, char *text)
{
    struct ol_pktlog_dev_t *pl_dev = NULL;

    pl_dev = scn->pl_dev;
    if (process_text_info(pl_dev, text))  {
        return A_ERROR;
    }

    return A_OK;
}

void
pktlog_callback_3_0(void *context, enum WDI_EVENT event, void *log_data,
    u_int16_t peer_id, uint32_t status)
{
    switch(event) {
        case WDI_EVENT_OFFLOAD_ALL:
        {
            /*
             * process all offload pktlog
             */
            if (process_offload_pktlog_3_0(context, log_data)) {
                AR_DEBUG_PRINTF(ATH_DEBUG_TRC, (" Unable to process offload pktlog \n"));
                return;
            }
            break;
        }
        case WDI_EVENT_RX_DESC:
        {
             /*
              * process RX message for remote frames
              */
            if (process_rx_desc_remote(context, log_data)) {
                AR_DEBUG_PRINTF(ATH_DEBUG_TRC, (" Unable to process RX info \n"));
                return;
            }
           break;
        }
        case WDI_EVENT_LITE_T2H:
        {
             /*
              * process HTT lite messges
              */
            if (process_pktlog_lite(context, log_data, PKTLOG_TYPE_LITE_T2H)) {
                AR_DEBUG_PRINTF(ATH_DEBUG_TRC, (" Unable to process lite_t2h\n"));
                return;
            }
            break;
        }
        case WDI_EVENT_LITE_RX:
        {
            /*
             * process RX message for remote frames
             */
            if (process_pktlog_lite(context, log_data, PKTLOG_TYPE_LITE_RX)) {
                AR_DEBUG_PRINTF(ATH_DEBUG_TRC, (" Unable to process lite RX\n"));
                return;
            }
            break;
        }
        default:
            break;
    }
}


static inline A_STATUS
wdi_pktlog_unsubscribe(struct ol_pktlog_dev_t *pl_dev, uint32_t log_state)
{
    ol_txrx_soc_handle soc_txrx_handle;
    ol_txrx_pdev_handle pdev_txrx_handle;
    struct ol_ath_softc_net80211 *scn;
    if (!pl_dev) {
        qdf_err("Invalid pl_dev in %s\n", __FUNCTION__);
        return A_ERROR;
    }

    scn = (struct ol_ath_softc_net80211 *)(pl_dev->scn);

    soc_txrx_handle = wlan_psoc_get_dp_handle(scn->soc->psoc_obj);
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);


    if ((log_state & ATH_PKTLOG_TX)       ||
            (log_state & ATH_PKTLOG_RCFIND)   ||
            (log_state & ATH_PKTLOG_RCUPDATE) ||
            (log_state & ATH_PKTLOG_RX)) {
        if (cdp_wdi_event_unsub(soc_txrx_handle,
                    (struct cdp_pdev *)(pdev_txrx_handle),
                    &(pl_dev->PKTLOG_OFFLOAD_SUBSCRIBER),
                    WDI_EVENT_OFFLOAD_ALL)) {
            return A_ERROR;
        }
    }
    if (log_state & ATH_PKTLOG_RX) {
        if (cdp_wdi_event_unsub(soc_txrx_handle,
                    (struct cdp_pdev *)(pdev_txrx_handle),
                    &(pl_dev->PKTLOG_RX_REMOTE_SUBSCRIBER),
                    WDI_EVENT_RX_DESC_REMOTE)) {
            return A_ERROR;
        }

        if (lmac_is_target_ar900b(scn->soc->psoc_obj)) {
            if ((log_state & ATH_PKTLOG_CBF)      ||
                (log_state & ATH_PKTLOG_H_INFO)   ||
                        (log_state & ATH_PKTLOG_STEERING)) {
                   if (cdp_wdi_event_unsub(soc_txrx_handle,
                                (struct cdp_pdev *)(pdev_txrx_handle),
                                &(pl_dev->PKTLOG_RX_CBF_SUBSCRIBER),
                           WDI_EVENT_RX_CBF_REMOTE)) {
                       return A_ERROR;
                   }
            }
        }
        if (cdp_wdi_event_unsub(soc_txrx_handle,
                    (struct cdp_pdev *)pdev_txrx_handle,
                    &(pl_dev->PKTLOG_RX_DESC_SUBSCRIBER),
                    WDI_EVENT_RX_DESC)) {
            return A_ERROR;
        }
    }
    if (log_state & ATH_PKTLOG_LITE_RX) {
        if (cdp_wdi_event_unsub(soc_txrx_handle,
                    (struct cdp_pdev *)(pdev_txrx_handle),
                    &(pl_dev->PKTLOG_RX_DESC_SUBSCRIBER),
                    WDI_EVENT_LITE_RX)) {
            return A_ERROR;
        }
    }
    if (log_state & ATH_PKTLOG_LITE_T2H) {
        if (cdp_wdi_event_unsub(soc_txrx_handle,
                    (struct cdp_pdev *)(pdev_txrx_handle),
                    &(pl_dev->PKTLOG_OFFLOAD_SUBSCRIBER),
                    WDI_EVENT_LITE_T2H)) {
            return A_ERROR;
        }
    }

    if(pl_dev->pl_info->peer_based_filter == 1) {
        pl_dev->pl_info->peer_based_filter = 0;
        if(cdp_enable_peer_based_pktlog(soc_txrx_handle,
                (struct cdp_pdev *)pdev_txrx_handle,
                pl_dev->pl_info->macaddr, 0)) {
            return A_ERROR;
        }
    }

    return A_OK;
}

int
pktlog_wmi_send_peer_based_enable(struct ol_ath_softc_net80211 *scn,
        char *macaddr)
{
    void *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if(!wmi_unified_peer_based_pktlog_send(pdev_wmi_handle, macaddr,
        lmac_get_pdev_idx(scn->sc_pdev), 1)) {
        return 0;
    }
    return -1;
}


int
pktlog_wmi_send_peer_based_disable(struct ol_ath_softc_net80211 *scn,
        char *macaddr)
{
    void *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if(!wmi_unified_peer_based_pktlog_send(pdev_wmi_handle, macaddr,
        lmac_get_pdev_idx(scn->sc_pdev), 0)) {
        return 0;
    }
    return -1;
}


int
pktlog_wmi_send_enable_cmd(struct ol_ath_softc_net80211 *scn,
        uint32_t types)
{
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (!wmi_unified_packet_log_enable_send(pdev_wmi_handle, types,
        lmac_get_pdev_idx(scn->sc_pdev))) {
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (scn->sc_ic.nss_radio_ops) {
            scn->sc_ic.nss_radio_ops->ic_nss_ol_pktlog_cfg(scn, 1);
        }
#endif
        return 0;
    }
    return -1;
}

int
pktlog_wmi_send_disable_cmd(struct ol_ath_softc_net80211 *scn)
{
    struct common_wmi_handle *pdev_wmi_handle;
#if UNIFIED_SMARTANTENNA
#if !defined(CONFIG_AR900B_SUPPORT)
    /* Smart Antenna uses packet log frame work, So when smart antenna is enabled disabling packet log
       in firmware is not allowed */
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    QDF_STATUS status;

    pdev = scn->sc_pdev;

    status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_SA_API_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_err("%s, %d unable to get reference", __func__, __LINE__);
        return -1;
    }

    wlan_pdev_obj_lock(pdev);
    psoc = wlan_pdev_get_psoc(pdev);
    wlan_pdev_obj_unlock(pdev);

    if (target_if_sa_api_is_tx_feedback_enabled(psoc, pdev)) {
        qdf_info("%s: As Smart Antenna is enabled, Packet log is not disabled ", __FUNCTION__);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
        return -1;
    }
    wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
#endif
#endif

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    if (!wmi_unified_packet_log_disable_send(pdev_wmi_handle,
                          lmac_get_pdev_idx(scn->sc_pdev))) {
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        if (scn->sc_ic.nss_radio_ops) {
            scn->sc_ic.nss_radio_ops->ic_nss_ol_pktlog_cfg(scn, 0);
        }
#endif
        return 0;
    }
    return -1;
}

int
pktlog_disable(struct ol_ath_softc_net80211 *scn)
{
    ol_pktlog_dev_t *pl_dev = NULL;
    struct ath_pktlog_info *pl_info = NULL;

#if UNIFIED_SMARTANTENNA
#if !defined(CONFIG_AR900B_SUPPORT)
    /* Smart Antenna uses packet log frame work, So when smart antenna is enabled disabling packet log
       in firmware is not allowed */
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    QDF_STATUS status;
#endif
#endif

    if ((scn == NULL) || (scn->pl_dev == NULL)) {
         return A_ERROR;
    }

    pl_dev = scn->pl_dev;
    pl_info = pl_dev->pl_info;

#if UNIFIED_SMARTANTENNA
#if !defined(CONFIG_AR900B_SUPPORT)
    /* Smart Antenna uses packet log frame work, So when smart antenna is enabled disabling packet log
       in firmware is not allowed */
    pdev = scn->sc_pdev;

    status = wlan_objmgr_pdev_try_get_ref(pdev, WLAN_SA_API_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        qdf_err("%s, %d unable to get reference", __func__, __LINE__);
        return -1;
    }

    wlan_pdev_obj_lock(pdev);
    psoc = wlan_pdev_get_psoc(pdev);
    wlan_pdev_obj_unlock(pdev);


    if (target_if_sa_api_is_tx_feedback_enabled(psoc, pdev)) {
        qdf_info("%s: As Smart Antenna is enabled, Packet log is not disabled ", __FUNCTION__);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
        return -1;
    }
    wlan_objmgr_pdev_release_ref(pdev, WLAN_SA_API_ID);
#endif
#endif

    if (PKTLOG_DISABLE(scn)) {
	qdf_err("%s, %d unable to send pktlog disable command", __func__, __LINE__);
        return A_ERROR;
    }

    if(pl_info->peer_based_filter == 1) {
        if (pktlog_wmi_send_peer_based_disable (scn,pl_info->macaddr)) {
              qdf_err("%s, %d unable to send disable pktlog filter WMI", __func__, __LINE__);
              return A_ERROR;
        }
    }
    return A_OK;
}

void
pktlog_deinit(struct ol_ath_softc_net80211 *scn)
{
#if REMOTE_PKTLOG_SUPPORT
    ol_pktlog_dev_t *pl_dev = NULL;
    struct ath_pktlog_info *pl_info = NULL;
    struct pktlog_remote_service *service = NULL;

    pl_dev = scn->pl_dev;

    if (!pl_dev) {
        return;
    }

    pl_info = pl_dev->pl_info;
    if (pl_info){
        qdf_spinlock_destroy(&pl_info->log_lock);
        qdf_mutex_destroy(&pl_info->pktlog_mutex);
    }

    service = &pl_dev->rpktlog_svc;
    if (!service) {
        return;
    }

    qdf_destroy_work(scn->soc->qdf_dev, &service->connection_service);
    qdf_destroy_work(scn->soc->qdf_dev, &service->accept_service);
    qdf_destroy_work(scn->soc->qdf_dev, &service->send_service);
#endif
}

void
pktlog_init_3_0(struct ol_ath_softc_net80211 *scn)
{
    struct ol_pktlog_dev_t *pl_dev = NULL;

    if (scn == NULL) {
        return;
    }

    pl_dev = scn->pl_dev;
    if (pl_dev == NULL) {
        return;
    }

    pl_dev->PKTLOG_RX_DESC_SUBSCRIBER.callback = pktlog_callback_3_0;
    pl_dev->PKTLOG_RX_DESC_SUBSCRIBER.context = pl_dev;
    pl_dev->PKTLOG_OFFLOAD_SUBSCRIBER.callback = pktlog_callback_3_0;
    pl_dev->PKTLOG_OFFLOAD_SUBSCRIBER.context = pl_dev;
    pl_dev->pktlog_hdr_size = sizeof(struct ath_pktlog_hdr_ar900b);

}
qdf_export_symbol(pktlog_init_3_0);

void
pktlog_init(struct ol_ath_softc_net80211 *scn)
{
    ol_ath_soc_softc_t *soc;
    struct ath_pktlog_info *pl_info = NULL;
    struct ol_pktlog_dev_t *pl_dev = NULL;
    uint32_t target_type;
#if REMOTE_PKTLOG_SUPPORT
    struct pktlog_remote_service *service = NULL;
#endif
    ol_txrx_pdev_handle pdev_txrx_handle;

    if ((scn == NULL) || (scn->pl_dev == NULL)) {
        return;
    }

    soc = scn->soc;
    pdev_txrx_handle = wlan_pdev_get_dp_handle(scn->sc_pdev);
    pl_dev = scn->pl_dev;
    pl_info = pl_dev->pl_info;
    target_type = lmac_get_tgt_type(scn->soc->psoc_obj);

    if (pl_info) {
#if REMOTE_PKTLOG_SUPPORT
        service = &pl_dev->rpktlog_svc;
#endif
        OS_MEMZERO(pl_info, sizeof(*pl_info));
        qdf_spinlock_create(&pl_info->log_lock);
        qdf_mutex_create(&pl_info->pktlog_mutex);

        pl_info->buf_size = PKTLOG_DEFAULT_BUFSIZE;
        pl_info->buf = NULL;
        pl_info->log_state = 0;
        pl_info->sack_thr = PKTLOG_DEFAULT_SACK_THR;
        pl_info->tail_length = PKTLOG_DEFAULT_TAIL_LENGTH;
        pl_info->thruput_thresh = PKTLOG_DEFAULT_THRUPUT_THRESH;
        pl_info->per_thresh = PKTLOG_DEFAULT_PER_THRESH;
        pl_info->phyerr_thresh = PKTLOG_DEFAULT_PHYERR_THRESH;
        pl_info->trigger_interval = PKTLOG_DEFAULT_TRIGGER_INTERVAL;
        pl_info->pktlen = 0;
        pl_info->start_time_thruput = 0;
        pl_info->start_time_per = 0;
        pl_info->scn = (ol_ath_generic_softc_handle) scn;
        pl_info->remote_port = 0;
        pl_info->tx_capture_enabled = 0;

        if (pl_dev) {
            pl_dev->target_type = target_type;
            if (soc->ol_if_ops->pktlog_init)
                soc->ol_if_ops->pktlog_init(scn);
            else {
               qdf_warn("%s: WARNING: Pktlog enabled for offload target type %d\n", __func__, target_type);
            }
            pl_info->pktlog_hdr_size = pl_dev->pktlog_hdr_size;
        } else {
            qdf_assert_always(0);
        }

#if REMOTE_PKTLOG_SUPPORT
        service->port = DEFAULT_REMOTE_PKTLOG_PORT;
        qdf_create_work(scn->soc->qdf_dev, &service->connection_service, pktlog_start_remote_service, scn);
        qdf_create_work(scn->soc->qdf_dev, &service->accept_service, pktlog_start_accept_service, scn);
        qdf_create_work(scn->soc->qdf_dev, &service->send_service, pktlog_run_send_service, scn);
#endif
        /*
         * Add the WDI subscribe command
         * Might be moved to enable function because,
         * it is not consuming the WDI unless we really need it.
         */
    }
}

static int
__pktlog_enable(struct ol_ath_softc_net80211 *scn, int32_t log_state)
{
    ol_pktlog_dev_t *pl_dev = NULL;
    struct ath_pktlog_info *pl_info = NULL;
    int error;

    if (scn == NULL) {
        return -EINVAL;
    }

    if (ol_ath_target_start(scn->soc)) {
        qdf_err("failed to start the target");
        return -1;
    }

    pl_dev = scn->pl_dev;
    pl_info = pl_dev->pl_info;

    pl_dev->sc_osdev = scn->sc_osdev;

    if (!pl_info) {
        return -EINVAL;
    }
    if (log_state != 0 && log_state != pl_info->log_state) {

        if (!scn) {
            if (g_pktlog_mode == PKTLOG_MODE_ADAPTER) {
                pktlog_disable_adapter_logging();
                g_pktlog_mode = PKTLOG_MODE_SYSTEM;
            }
        }
        else {
            if (g_pktlog_mode == PKTLOG_MODE_SYSTEM) {
                g_pktlog_mode = PKTLOG_MODE_ADAPTER;
            }
        }

        if (pl_info->buf == NULL) {
            error = pktlog_alloc_buf(scn);
            if (error != 0)
                return error;

            qdf_spin_lock(&pl_info->log_lock);
            pl_info->buf->bufhdr.version = CUR_PKTLOG_VER;
            pl_info->buf->bufhdr.magic_num = PKTLOG_MAGIC_NUM;
            pl_info->buf->wr_offset = 0;
            pl_info->buf->rd_offset = -1;
            qdf_spin_unlock(&pl_info->log_lock);
        }
        pl_info->start_time_thruput = OS_GET_TIMESTAMP();
        pl_info->start_time_per = pl_info->start_time_thruput;

        /* WDI subscribe */
        if (wdi_pktlog_subscribe(pl_dev, log_state)) {
            qdf_err("Unable to subscribe to the WDI %s\n",
                    __FUNCTION__);
            return -1;
        }

        /* WMI command to enable pktlog on the firmware */
        if (pktlog_enable_tgt(scn, log_state)) {
            qdf_err("Device cannot be enabled, %s\n", __FUNCTION__);
            return -1;
        } else {
            pl_dev->tgt_pktlog_enabled = true;
#if REMOTE_PKTLOG_SUPPORT
            if (log_state & ATH_PKTLOG_REMOTE_LOGGING_ENABLE) {
                /* enable remote packet logging */
                pktlog_remote_enable(scn, 1);
            }
#endif
    pl_info->log_state |= log_state;
        }
    } else if (!log_state && pl_dev->tgt_pktlog_enabled) {
        pl_dev->pl_funcs->pktlog_disable(scn);

        pl_dev->tgt_pktlog_enabled = false;
#if REMOTE_PKTLOG_SUPPORT
        /* disable remote packet logging */
        pktlog_remote_enable(scn, 0);
#endif
        if (wdi_pktlog_unsubscribe(pl_dev, pl_info->log_state)) {
            qdf_err("Cannot unsubscribe pktlog from the WDI\n");
            return -1;
        }
    pl_info->log_state = log_state;
    }

    return 0;
}


int
pktlog_enable(struct ol_ath_softc_net80211 *scn, int32_t log_state)
{
    ol_pktlog_dev_t *pl_dev = NULL;
    struct ath_pktlog_info *pl_info = NULL;
    int status;

    if (scn == NULL) {
        return -EINVAL;
    }

    if (ol_ath_target_start(scn->soc)) {
        qdf_err("failed to start the target");
        return -1;
    }

    pl_dev = scn->pl_dev;

    if (!pl_dev) {
        return -EINVAL;
    }
    pl_info = pl_dev->pl_info;

    if (!pl_info) {
        return -EINVAL;
    }

    qdf_mutex_acquire(&pl_info->pktlog_mutex);
    status = __pktlog_enable(scn, log_state);
    qdf_mutex_release(&pl_info->pktlog_mutex);
    return status;

}


static int
__pktlog_filter_mac(struct ol_ath_softc_net80211 *scn, char *macaddr)
{
    ol_pktlog_dev_t *pl_dev = NULL;
    struct ath_pktlog_info *pl_info = NULL;

    if(scn == NULL) {
        return -EINVAL;
    }

    pl_dev = scn->pl_dev;
    pl_info = pl_dev->pl_info;
    pl_info->peer_based_filter = 1;

    qdf_mem_copy(&pl_info->macaddr[0],macaddr,6);
    return 0;
}

int
pktlog_filter_mac(struct ol_ath_softc_net80211 *scn, char *macaddr)
{
    ol_pktlog_dev_t *pl_dev = NULL;
    struct ath_pktlog_info *pl_info = NULL;
    int status;

    if(scn == NULL) {
        return -EINVAL;
    }

    if (ol_ath_target_start(scn->soc)) {
        qdf_err("failed to start the target\n");
        return -1;
    }

    pl_dev = scn->pl_dev;

    if(!pl_dev) {
        return -EINVAL;
    }
    pl_info = pl_dev->pl_info;

    if (!pl_info) {
        return -EINVAL;
    }
    qdf_mutex_acquire(&pl_info->pktlog_mutex);
    status = __pktlog_filter_mac(scn, macaddr);
    qdf_mutex_release(&pl_info->pktlog_mutex);
    return status;

}
static int
__pktlog_setsize(struct ol_ath_softc_net80211 *scn, int32_t size)
{
    ol_pktlog_dev_t *pl_dev = NULL;
    struct ath_pktlog_info *pl_info = NULL;

    pl_dev  = scn->pl_dev;
    pl_info = (pl_dev) ? pl_dev->pl_info : g_pktlog_info;

    qdf_info("I am setting the size of the pktlog buffer:%s\n", __FUNCTION__);
    if (size < 0)
        return -EINVAL;

    if (size == pl_info->buf_size)
        return 0;

    if (pl_info->log_state) {
        qdf_warn("Logging should be disabled before changing bufer size\n");
        return -EINVAL;
    }

    qdf_spin_lock(&pl_info->log_lock);
    if (pl_info->buf != NULL)
        pktlog_release_buf(scn);

    if (size != 0)
        pl_info->buf_size = size;
    qdf_spin_lock(&pl_info->log_lock);

    return 0;
}


int
pktlog_setsize(struct ol_ath_softc_net80211 *scn, int32_t size)
{
    ol_pktlog_dev_t *pl_dev = NULL;
    struct ath_pktlog_info *pl_info = NULL;
    int status;

    if ((scn == NULL)) {
        return -EINVAL;
    }

    pl_dev  = scn->pl_dev;
    pl_info = (pl_dev) ? pl_dev->pl_info : g_pktlog_info;

    if (pl_info == NULL) {
        return -EINVAL;
    }

    qdf_mutex_acquire(&pl_info->pktlog_mutex);
    status = __pktlog_setsize(scn, size);
    qdf_mutex_release(&pl_info->pktlog_mutex);
    return status;
}

#endif /* REMOVE_PKT_LOG */
