/*
 * Copyright (c) 2012, 2017, 2019 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 *
 * 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef _PKTLOG_AC_H_
#define _PKTLOG_AC_H_
#ifndef REMOVE_PKT_LOG

#include "ol_if_athvar.h"
#include <pktlog_ac_api.h>
/* Include pktlog_ac_fmt.h below ol_if_athvar.h to ensure MacOS driver compiles.
 * TODO: Fix __ATTRIB_PACK defination in this file, to remove above requirement.
 */
#include <pktlog_ac_fmt.h>
#include "osdep.h"
#if 0
#include <wmi_unified.h>
#endif
#include <wmi_unified_api.h>
#include <wdi_event_api.h>
#include <linux_remote_pktlog.h>

#define NO_REG_FUNCS 4

/* Locking interface for pktlog */
#define PKTLOG_LOCK_INIT(_pl_info)    spin_lock_init(&(_pl_info)->log_lock)
#define	PKTLOG_LOCK_DESTROY(_pl_info)
#define PKTLOG_LOCK(_pl_info)         spin_lock(&(_pl_info)->log_lock)
#define PKTLOG_UNLOCK(_pl_info)       spin_unlock(&(_pl_info)->log_lock)

#define PKTLOG_MODE_SYSTEM   1
#define PKTLOG_MODE_ADAPTER  2

/* This macro has defined temporary purpose*/
#define PKTLOG_HW2SW_MACID(id) ((id) > 0 ? ((id) - 1) : 0)
#define PKTLOG_SW2HW_MACID(id) ((id) + 1)

extern void pktlog_disable_adapter_logging(void);
extern int pktlog_alloc_buf(struct ol_ath_softc_net80211 *scn);
extern void pktlog_release_buf(struct ol_ath_softc_net80211 *scn);
/*extern void pktlog_disable_adapter_logging(void);
extern int pktlog_alloc_buf(ol_ath_generic_softc_handle sc,
                                struct ath_pktlog_info *pl_info);
extern void pktlog_release_buf(struct ath_pktlog_info *pl_info);*/

struct ol_pl_arch_dep_funcs {
    void (*pktlog_init)(struct ol_ath_softc_net80211 *scn);
    int (*pktlog_enable)(struct ol_ath_softc_net80211 *scn,
            int32_t log_state);
    int (*pktlog_setsize)(struct ol_ath_softc_net80211 *scn,
            int32_t log_state);
    int (*pktlog_disable)(struct ol_ath_softc_net80211 *scn);
    void (*pktlog_deinit)(struct ol_ath_softc_net80211 *scn);
    int (*pktlog_filter_mac)(struct ol_ath_softc_net80211 *scn,
            char *macaddr);
    int (*pktlog_text)(struct ol_ath_softc_net80211 *scn, char *text);
};

struct ol_pl_os_dep_funcs{
    int (*pktlog_attach) (struct ol_ath_softc_net80211 *scn);
    void (*pktlog_detach) (struct ol_ath_softc_net80211 *scn);
};


#ifdef OL_ATH_SMART_LOGGING
struct ol_pl_smartlog_handler_funcs {
    /*
     * See documentation for the functions that get registered to the below
     * function pointers.
     */
    QDF_STATUS (*smart_log_fw_pktlog_stop_and_block)(
            struct ol_ath_softc_net80211 *scn,
            int32_t host_pktlog_types, bool block_only_if_started);
    void (*smart_log_fw_pktlog_unblock)(struct ol_ath_softc_net80211 *scn);
};
#endif /* OL_ATH_SMART_LOGGING */

extern struct ol_pl_arch_dep_funcs ol_pl_funcs;
extern struct ol_pl_os_dep_funcs *g_ol_pl_os_dep_funcs;

/* Pktlog handler to save the state of the pktlogs */
struct ol_pktlog_dev_t
{
    struct ol_pl_arch_dep_funcs *pl_funcs;
    struct ath_pktlog_info *pl_info;
    ol_ath_generic_softc_handle scn;
    char *name;
    bool tgt_pktlog_enabled;
#ifdef OL_ATH_SMART_LOGGING
    struct ol_pl_smartlog_handler_funcs pl_sl_funcs;
#endif /* OL_ATH_SMART_LOGGING */
    osdev_t sc_osdev;
    uint32_t target_type;
    size_t pktlog_hdr_size;
    uint32_t msdu_id_offset;
    size_t msdu_info_priv_size;
    A_STATUS (*process_tx_msdu_info) (ol_txrx_pdev_handle txrx_pdev,
                                      void *data, ath_pktlog_hdr_t *pl_hdr);
    wdi_event_subscribe PKTLOG_RX_REMOTE_SUBSCRIBER;
    wdi_event_subscribe PKTLOG_OFFLOAD_SUBSCRIBER;
    wdi_event_subscribe PKTLOG_RX_CBF_SUBSCRIBER;
    wdi_event_subscribe PKTLOG_RX_DESC_SUBSCRIBER;
#if REMOTE_PKTLOG_SUPPORT
    struct pktlog_remote_service rpktlog_svc;  /* remote packet log service */
#endif
};

extern struct ol_pktlog_dev_t ol_pl_dev;
extern unsigned int enable_pktlog_support;
/*
 * pktlog WMI command
 * Selective modules enable TBD
 * TX/ RX/ RCF/ RCU
 */

int pktlog_wmi_send_peer_based_enable(struct ol_ath_softc_net80211 *scn,
                                   char *macaddr);
int pktlog_wmi_send_peer_based_disable(struct ol_ath_softc_net80211 *scn,
                                   char *macaddr);
int pktlog_wmi_send_enable_cmd(struct ol_ath_softc_net80211 *scn, uint32_t types);
int pktlog_wmi_send_disable_cmd(struct ol_ath_softc_net80211 *scn);

#define PKTLOG_DISABLE(_scn) \
    pktlog_wmi_send_disable_cmd(_scn)

/*
 * WDI related data and functions
 * Callback function to the WDI events
 */
void pktlog_callback(void *pdev, enum WDI_EVENT event, void *log_data, u_int16_t peer_id, uint32_t status);

#define ol_pktlog_attach(_scn)                          \
    do {                                                    \
        if (g_ol_pl_os_dep_funcs) {                             \
            g_ol_pl_os_dep_funcs->pktlog_attach(_scn);              \
        }                                                       \
    } while(0)

#define ol_pktlog_detach(_scn)                          \
    do {                                                    \
        if (g_ol_pl_os_dep_funcs) {                             \
            g_ol_pl_os_dep_funcs->pktlog_detach(_scn);              \
        }                                                       \
    } while(0)

#else /* REMOVE_PKT_LOG */
#define ol_pktlog_attach(_scn)    /* */
#define ol_pktlog_detach(_scn)    /* */
#endif /* REMOVE_PKT_LOG */
#endif /* _PKTLOG_AC_H_ */
