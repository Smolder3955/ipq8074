/*
 * Copyright (c) 2013-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
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
 * UMAC resmgr specific offload interface functions - for power and performance offload model
 */
#include "ol_if_athvar.h"
#include "ieee80211_sm.h"
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_lmac_if_api.h>
#include <init_deinit_lmac.h>
#include <wlan_mlme_dispatcher.h>
#include <ieee80211_mlme_dfs_dispatcher.h>

#if ATH_PERF_PWR_OFFLOAD
/* Resource Manager structure */
struct ieee80211_resmgr {
    /* the function indirection SHALL be the first fileds */
    struct ieee80211_resmgr_func_table              resmgr_func_table;
    struct ieee80211com                             *ic;               /* Back pointer to ic */
    ieee80211_resmgr_mode                           mode;
    qdf_spinlock_t rm_lock;   /* Lock ensures that only one thread runs res mgr at a time */
    /* Add one lock for notif_handler register/deregister operation, notif_handler will be modified by
       wlan_scan_run() call and cause schedule-while-atomic in split driver. */
    qdf_spinlock_t                                      rm_handler_lock;
    ieee80211_hsm_t                                 hsm_handle;        /* HSM Handle */
    ieee80211_resmgr_notification_handler           notif_handler[IEEE80211_MAX_RESMGR_EVENT_HANDLERS];
    void                                            *notif_handler_arg[IEEE80211_MAX_RESMGR_EVENT_HANDLERS];
};

#define IEEE80211_RESMGR_REQID     0x5555
#define IEEE80211_RESMGR_NOTIFICATION(_resmgr,_notif)  do {                 \
        int i;                                                              \
        for(i = 0;i < IEEE80211_MAX_RESMGR_EVENT_HANDLERS; ++i) {                  \
            if (_resmgr->notif_handler[i]) {                                \
                (* _resmgr->notif_handler[i])                               \
                    (_resmgr, _notif, _resmgr->notif_handler_arg[i]);       \
             }                                                              \
        }                                                                   \
    } while(0)

static void _ieee80211_resmgr_create_complete(ieee80211_resmgr_t resmgr)
{
    return;
}

static void _ieee80211_resmgr_delete(ieee80211_resmgr_t resmgr)
{
    struct ieee80211com *ic;
    if (!resmgr)
        return ;

    ic = resmgr->ic;
    qdf_spinlock_destroy(&resmgr->rm_lock);
    qdf_spinlock_destroy(&resmgr->rm_handler_lock);

    qdf_mem_free(resmgr);

    ic->ic_resmgr = NULL;

    return;
}

static void _ieee80211_resmgr_delete_prepare(ieee80211_resmgr_t resmgr)
{
    return;
}

static int _ieee80211_resmgr_request_offchan(ieee80211_resmgr_t resmgr,
                                     struct ieee80211_ath_channel *chan,
                                     u_int16_t reqid,
                                     u_int16_t max_bss_chan,
                                     u_int32_t max_dwell_time)
{
    return EOK;
}

static int _ieee80211_resmgr_request_bsschan(ieee80211_resmgr_t resmgr,
                                     u_int16_t reqid)
{
    return EOK;
}

static int _ieee80211_resmgr_request_chanswitch(ieee80211_resmgr_t resmgr,
                                        ieee80211_vap_t vap,
                                        struct ieee80211_ath_channel *chan,
                                        u_int16_t reqid)
{
    return EOK;
}

static void _ieee80211_resmgr_vattach(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap)
{
    return;
}

static void _ieee80211_resmgr_vdetach(ieee80211_resmgr_t resmgr, ieee80211_vap_t vap)
{
    return;
}

static const char *_ieee80211_resmgr_get_notification_type_name(ieee80211_resmgr_t resmgr, ieee80211_resmgr_notification_type type)
{
    return "unknown";
}

static int _ieee80211_resmgr_register_noa_event_handler(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap, ieee80211_resmgr_noa_event_handler handler, void *arg)
{

    return EOK;
}

static int _ieee80211_resmgr_unregister_noa_event_handler(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap)
{
    return EOK;
}

static void ieee80211_notify_vap_restart_complete(ieee80211_resmgr_t resmgr,
                             struct ieee80211vap *vap,  ieee80211_resmgr_notification_status status)
{
    ieee80211_resmgr_notification notif;

    /* Intimate start completion to VAP module */
    notif.type = IEEE80211_RESMGR_VAP_RESTART_COMPLETE;
    notif.req_id = IEEE80211_RESMGR_REQID;
    notif.status = status;
    notif.vap = vap;
    vap->iv_rescan = 0;
    IEEE80211_RESMGR_NOTIFICATION(resmgr, &notif);

}

/*
 * Register a resmgr notification handler.
 */
static int
_ieee80211_resmgr_register_notification_handler(ieee80211_resmgr_t resmgr,
                                               ieee80211_resmgr_notification_handler notificationhandler,
                                               void *arg)
{
    int i;

    /* unregister if there exists one already */
    ieee80211_resmgr_unregister_notification_handler(resmgr, notificationhandler,arg);
    qdf_spin_lock(&resmgr->rm_handler_lock);
    for (i=0; i<IEEE80211_MAX_RESMGR_EVENT_HANDLERS; ++i) {
        if (resmgr->notif_handler[i] == NULL) {
            resmgr->notif_handler[i] = notificationhandler;
            resmgr->notif_handler_arg[i] = arg;
            qdf_spin_unlock(&resmgr->rm_handler_lock);
            return 0;
        }
    }
    qdf_spin_unlock(&resmgr->rm_handler_lock);
    return -ENOMEM;
}

/*
 * Unregister a resmgr event handler.
 */
static int _ieee80211_resmgr_unregister_notification_handler(ieee80211_resmgr_t resmgr,
                                                     ieee80211_resmgr_notification_handler handler,
                                                     void  *arg)
{
    int i;
    qdf_spin_lock(&resmgr->rm_handler_lock);
    for (i=0; i<IEEE80211_MAX_RESMGR_EVENT_HANDLERS; ++i) {
        if (resmgr->notif_handler[i] == handler && resmgr->notif_handler_arg[i] == arg ) {
            resmgr->notif_handler[i] = NULL;
            resmgr->notif_handler_arg[i] = NULL;
            qdf_spin_unlock(&resmgr->rm_handler_lock);
            return 0;
        }
    }
    qdf_spin_unlock(&resmgr->rm_handler_lock);
    return -EEXIST;
}


static int _ieee80211_resmgr_off_chan_sched_set_air_time_limit(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap,
    u_int32_t scheduled_air_time_limit
    )
{
    return EINVAL;
}

static u_int32_t _ieee80211_resmgr_off_chan_sched_get_air_time_limit(ieee80211_resmgr_t resmgr,
    ieee80211_vap_t vap
    )
{
    return 100;
}

ieee80211_resmgr_t ol_resmgr_create(struct ieee80211com *ic, ieee80211_resmgr_mode mode)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ieee80211_resmgr_t    resmgr;

    qdf_info("OL Resmgr Init-ed");

    if (ic->ic_resmgr) {
        printk("%s : ResMgr already exists \n", __func__);
        return NULL;
    }

    /* Allocate ResMgr data structures */
    resmgr = (ieee80211_resmgr_t) qdf_mem_malloc(sizeof(struct ieee80211_resmgr));
    if (resmgr == NULL) {
        printk("%s : ResMgr memory alloction failed\n", __func__);
        return NULL;
    }

    OS_MEMZERO(resmgr, sizeof(struct ieee80211_resmgr));
    resmgr->ic = ic;
    resmgr->mode = mode;
    /* Indicate the device is capable of multi-chan operation*/
    ieee80211com_set_cap_ext(&scn->sc_ic, IEEE80211_CEXT_MULTICHAN);

    /* initialize function pointer table */
    resmgr->resmgr_func_table.resmgr_create_complete = _ieee80211_resmgr_create_complete;
    resmgr->resmgr_func_table.resmgr_delete = _ieee80211_resmgr_delete;
    resmgr->resmgr_func_table.resmgr_delete_prepare = _ieee80211_resmgr_delete_prepare;

    resmgr->resmgr_func_table.resmgr_register_notification_handler = _ieee80211_resmgr_register_notification_handler;
    resmgr->resmgr_func_table.resmgr_unregister_notification_handler = _ieee80211_resmgr_unregister_notification_handler;

    resmgr->resmgr_func_table.resmgr_request_offchan = _ieee80211_resmgr_request_offchan;
    resmgr->resmgr_func_table.resmgr_request_bsschan = _ieee80211_resmgr_request_bsschan;
    resmgr->resmgr_func_table.resmgr_request_chanswitch = _ieee80211_resmgr_request_chanswitch;


    resmgr->resmgr_func_table.resmgr_vattach = _ieee80211_resmgr_vattach;
    resmgr->resmgr_func_table.resmgr_vdetach = _ieee80211_resmgr_vdetach;
    resmgr->resmgr_func_table.resmgr_get_notification_type_name = _ieee80211_resmgr_get_notification_type_name;
    resmgr->resmgr_func_table.resmgr_register_noa_event_handler = _ieee80211_resmgr_register_noa_event_handler;
    resmgr->resmgr_func_table.resmgr_unregister_noa_event_handler = _ieee80211_resmgr_unregister_noa_event_handler;
    resmgr->resmgr_func_table.resmgr_off_chan_sched_set_air_time_limit = _ieee80211_resmgr_off_chan_sched_set_air_time_limit;
    resmgr->resmgr_func_table.resmgr_off_chan_sched_get_air_time_limit = _ieee80211_resmgr_off_chan_sched_get_air_time_limit;

    qdf_spinlock_create(&resmgr->rm_lock);
    qdf_spinlock_create(&resmgr->rm_handler_lock);


    return resmgr;
}

void ol_ath_resmgr_soc_attach(ol_ath_soc_softc_t *soc)
{
    struct common_wmi_handle *wmi_handle;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    /* If Beacon offload service enabled */
    if (ol_ath_is_beacon_offload_enabled(soc)) {
        wmi_unified_register_event_handler((void *)wmi_handle,
                                           wmi_pdev_csa_switch_count_status_event_id,
                                           ol_ath_pdev_csa_status_event_handler,
                                           WMI_RX_UMAC_CTX);
    }
}

void
ol_ath_resmgr_attach(struct ieee80211com *ic)
{
    ic->ic_resmgr_create = ol_resmgr_create;
}

#endif /* ATH_PERF_PWR_OFFLOAD */

