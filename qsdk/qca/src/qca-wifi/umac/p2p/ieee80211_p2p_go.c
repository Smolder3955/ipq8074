/*
 * Copyright (c) 2011-2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2009-2010, Atheros Communications
 */


#include "ieee80211_var.h"
#include "ieee80211_p2p_proto.h"
#include "ieee80211P2P_api.h"
#include "ieee80211_ie_utils.h"
#include "ieee80211_channel.h"
#include "ieee80211_p2p_ie.h"
#include "ieee80211_p2p_go_power.h"
#include "ieee80211_p2p_go.h"
#include "ieee80211_p2p_go_priv.h"
#include "ieee80211_p2p_prot_go.h"
#include "ieee80211_p2p_prot_api.h"

#include "ieee80211_rateset.h"

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#if UMAC_SUPPORT_P2P

/* disable locking for now */
#define P2P_GO_LOCK(_p2p_handle)  /* */
#define P2P_GO_UNLOCK(_p2p_handle)  /* */

struct ieee80211_rx_status;

static int
ieee80211_p2p_go_input_filter_mgmt(wlan_node_t ni, wbuf_t wbuf, int subtype,
                                   struct ieee80211_rx_status *rs);

static int
ieee80211_p2p_go_output_filter(wbuf_t wbuf);

static void
go_mlme_event_handler(ieee80211_vap_t vap, ieee80211_mlme_event *event, void *arg);

static void ieee80211_p2p_go_update_beacon_p2p_ie(
    wlan_p2p_go_t       p2p_go_handle);

#if TEST_VAP_PAUSE
static void test_pause_tx_bcn_notify( ieee80211_tx_bcn_notify_t h_notify,
                                      int beacon_id, u_int32_t tx_status, void *arg);
static OS_TIMER_FUNC(test_vap_pause_timer_handler);
#endif

#define P2P_GO_HANDLE_MARKER    0xDE0198FA


#define NOA_SM_SCHEDULER_DELAY 500 /* time for NOA SM to run before and after scheduler takes channel away */

/*
 * This is a callback from Scheduler/ResManager to update the NOA schedule.
*/
static void
p2p_go_resmgr_noa_event_cb(
    void *arg,
    ieee80211_resmgr_noa_schedule_t noa_schedules,
    u_int8_t num_schedules)
{
    int                             ret = 0;
    int                             i;
    ieee80211_p2p_go_schedule_req   go_schedules[IEEE80211_MAX_NOA_DESCRIPTORS];
    wlan_p2p_go_t                   p2p_go_handle = NULL;

    ASSERT(arg != 0);
    p2p_go_handle = (wlan_p2p_go_t)arg;

    if (num_schedules > IEEE80211_MAX_NOA_DESCRIPTORS) {
        /* Check the max. number of schedules */
        IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY, 
            "%s: Error: too many schedules. curr=%d, max=%d\n", __func__,
            num_schedules, IEEE80211_MAX_NOA_DESCRIPTORS);
        return;
    }

    for (i = 0; i < num_schedules; ++i) {

        go_schedules[i].type = IEEE80211_P2P_GO_ABSENT_SCHEDULE;
        go_schedules[i].pri = noa_schedules[i].priority;    /* Lower is higher priority */
        go_schedules[i].noa_descriptor.type_count = noa_schedules[i].type_count;
        go_schedules[i].noa_descriptor.interval = noa_schedules[i].interval;
        go_schedules[i].noa_descriptor.duration = noa_schedules[i].duration + NOA_SM_SCHEDULER_DELAY*2;
        go_schedules[i].noa_descriptor.start_time = noa_schedules[i].start_tsf - NOA_SM_SCHEDULER_DELAY;
        IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_POWER,
                          "%s: noa update %d: pri=%d duration=0x%x type=%d start_time=0x%x, interval=0x%x\n",
                          __func__, i, go_schedules[i].pri, go_schedules[i].noa_descriptor.duration,
                          go_schedules[i].noa_descriptor.type_count,
                          go_schedules[i].noa_descriptor.start_time,
                          go_schedules[i].noa_descriptor.interval);
    }

    ret = wlan_p2p_GO_ps_set_noa_schedule(p2p_go_handle->p2p_go_ps, num_schedules, go_schedules);
    if (ret != 0) {
        IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY, 
            "%s: Error=%d from wlan_p2p_GO_ps_set_noa_schedule\n", __func__, ret);
    }
}

wlan_p2p_go_t
wlan_p2p_GO_create(wlan_p2p_t p2p_handle, u_int8_t *bssid)
{
    osdev_t                 oshandle;
    wlan_dev_t              devhandle;
    wlan_p2p_go_t           p2p_go_handle = NULL;
    wlan_if_t               vap = NULL;
    bool                    success = true;
    struct wlan_mlme_app_ie *h_app_ie = NULL;

    /*
     * treat p2p_handle as devhandle until the p2p device object is
     * implemented.
     */
    devhandle = (wlan_dev_t) p2p_handle;
    oshandle = ieee80211com_get_oshandle(devhandle);

    /* Check that P2P is supported */
    if (wlan_get_device_param(devhandle, IEEE80211_DEVICE_P2P) == 0) {
        ieee80211com_note(devhandle, IEEE80211_MSG_P2P_PROT, "%s: Error: P2P unsupported.\n", __func__);
        return NULL;
    }

    do {

        p2p_go_handle = (wlan_p2p_go_t) OS_MALLOC(oshandle, sizeof(struct ieee80211_p2p_go), 0);

        if (p2p_go_handle == NULL) {
            success = false;
            break;
        }
        OS_MEMZERO(p2p_go_handle, sizeof(struct ieee80211_p2p_go));

        spin_lock_init(&p2p_go_handle->lock);
        spin_lock_init(&p2p_go_handle->proberesp_p2p_ie.lock);

        vap = wlan_vap_create(devhandle,
                              IEEE80211_M_HOSTAP,
                              DEF_VAP_SCAN_PRI_MAP_OPMODE_AP_P2P_GO,
                              IEEE80211_P2PGO_VAP | (bssid ? 0 : IEEE80211_CLONE_BSSID),
                              bssid, NULL, NULL);
        if (vap == NULL) {
            success = false;
            break;
        }
        p2p_go_handle->specialmarker = P2P_GO_HANDLE_MARKER;
        p2p_go_handle->vap = vap;
        p2p_go_handle->devhandle = devhandle;
        /* by default use scheduler for scan */
        p2p_go_handle->use_scheduler_for_scan =  true;

        h_app_ie = wlan_mlme_app_ie_create(p2p_go_handle->vap);
        if (h_app_ie == NULL) {
            IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY, 
                "%s: Error in allocating Application IE\n", __func__);
            success = false;
            break;
        }
        p2p_go_handle->app_ie_handle = h_app_ie;

        ieee80211vap_set_private_data(vap,(void *) p2p_go_handle);

        /* TODO: need to unregister these filters when this function fails or in wlan_p2p_GO_delete */
        (void) ieee80211vap_set_input_mgmt_filter(vap, ieee80211_p2p_go_input_filter_mgmt);
        (void) ieee80211vap_set_output_mgmt_filter_func(vap, ieee80211_p2p_go_output_filter);

        /* setup rates for p2p vap */
        ieee80211_p2p_setup_rates(vap); 

        /* Make sure that GO does not resume on a DFS channel */
        wlan_set_param(vap, IEEE80211_AP_REJECT_DFS_CHAN, true);

        /* set the managemet frame rate to 6M including beacon rate */
        wlan_set_param(vap,IEEE80211_MGMT_RATE, IEEE80211_P2P_MIN_RATE);

        /* set the multicast frame rate to 6M (convert ieee rate to Kbps, the param takes kbps) */
        wlan_set_param(vap,IEEE80211_MCAST_RATE, (IEEE80211_P2P_MIN_RATE * 1000)/2);

        /*
         * turn on the cansleep feature/flag to allow the GO to sleep.
         */
        ieee80211_vap_cansleep_set(vap);


        p2p_go_handle->p2p_go_ps = ieee80211_p2p_go_power_vattach(oshandle, p2p_go_handle);
        if (p2p_go_handle->p2p_go_ps == NULL) {
            IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY, 
                "%s: Error: unable to attach to P2P GO Power Management. Disable CanSleep.\n", __func__);
            ieee80211_vap_cansleep_clear(vap);
        }
        else {
            /* Register with Resource Manager for NOA schedule changes */
            int retval;
            retval = ieee80211_resmgr_register_noa_event_handler(vap->iv_ic->ic_resmgr, 
                          p2p_go_handle->vap, p2p_go_resmgr_noa_event_cb, p2p_go_handle);
            if (retval != EOK) {
                IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY, 
                    "%s: Error=%d from ieee80211_resmgr_register_noa_event_handler()\n", __func__, retval);
            }
        }

       /* registerm with mlme module */
        if ( ieee80211_mlme_register_event_handler(vap,go_mlme_event_handler, p2p_go_handle) != EOK) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, 
                    "%s: ERROR, ieee80211_mlme_register_evet_handler failed \n",
                       __func__);
            success = false;
        }
        /* Update the BEACON IE's */
        ieee80211_p2p_go_update_beacon_p2p_ie(p2p_go_handle);

    } while ( false );

    /* by default use scheduler for scan */
    if (p2p_go_handle) {
        p2p_go_handle->use_scheduler_for_scan =  true;
        
        /* By deafult allow unlimited clients to be associated with the GO */
        p2p_go_handle->p2p_go_max_clients = 0xffff;
    }
    /*
     * Clear the "no_multichannel" bit to allow multi-channel operation
     * for the case where the STA vap is on a DFS channel which is not
     * allowed for a GO
     */
    ieee80211_vap_no_multichannel_clear(vap);
#if TEST_VAP_PAUSE
    p2p_go_handle->h_tx_bcn_notify=
    ieee80211_reg_notify_tx_bcn(devhandle->ic_notify_tx_bcn_mgr, test_pause_tx_bcn_notify, 0, p2p_go_handle);
    OS_INIT_TIMER(oshandle, &p2p_go_handle->pause_timer, test_vap_pause_timer_handler, p2p_go_handle , QDF_TIMER_TYPE_WAKE_APPS);
#endif

    if (!success) {
        /* Failure. Do the cleanup */

        if ((p2p_go_handle != NULL) && (p2p_go_handle->p2p_go_ps != NULL)) {
            if ((p2p_go_handle->vap) && vap) {
                ieee80211_resmgr_unregister_noa_event_handler(vap->iv_ic->ic_resmgr, p2p_go_handle->vap);
            }
            ieee80211_p2p_go_power_vdettach(p2p_go_handle->p2p_go_ps);
            p2p_go_handle->p2p_go_ps = NULL;
        }

        if (h_app_ie != NULL) {
            wlan_mlme_remove_ie_list(h_app_ie);
            h_app_ie = NULL;
        }

        if (vap != NULL) {
            wlan_vap_delete(vap);
            vap = NULL;
        }

        if (p2p_go_handle != NULL) {
            spin_lock_destroy(&p2p_go_handle->proberesp_p2p_ie.lock);
            spin_lock_destroy(&p2p_go_handle->lock);
            OS_FREE(p2p_go_handle);
            p2p_go_handle = NULL;
        }
    }

    return p2p_go_handle;
}

int wlan_p2p_GO_delete(wlan_p2p_go_t p2p_go_handle)
{
    ASSERT(p2p_go_handle != NULL);

    if (wlan_p2p_prot_go_is_attach(p2p_go_handle)) {
        IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY,
                          "%s: Warning: Did not attach Protocol module.\n", __func__);
        wlan_p2p_prot_go_detach(p2p_go_handle);
    }

    wlan_p2p_GO_set_suppress_beacon(p2p_go_handle, false);

    ieee80211_mlme_unregister_event_handler(p2p_go_handle->vap,go_mlme_event_handler, p2p_go_handle);

    if (p2p_go_handle->p2p_go_ps) {

        ieee80211_resmgr_unregister_noa_event_handler(p2p_go_handle->vap->iv_ic->ic_resmgr, p2p_go_handle->vap);

        ieee80211_p2p_go_power_vdettach(p2p_go_handle->p2p_go_ps);
        p2p_go_handle->p2p_go_ps = NULL;
    }

    wlan_mlme_remove_ie_list(p2p_go_handle->app_ie_handle);

    spin_lock_destroy(&p2p_go_handle->lock);    // assumes that lock is always init.

#if TEST_VAP_PAUSE
    OS_CANCEL_TIMER(&p2p_go_handle->pause_timer);
    OS_FREE_TIMER(&p2p_go_handle->pause_timer);
    ieee80211_dereg_notify_tx_bcn(p2p_go_handle->h_tx_bcn_notify);
#endif

    wlan_vap_delete(p2p_go_handle->vap);

    if (p2p_go_handle->proberesp_p2p_ie.curr_ie) {
        /* Free the current ProbeResp P2P IE */
        OS_FREE(p2p_go_handle->proberesp_p2p_ie.curr_ie);
    }

    if (p2p_go_handle->proberesp_p2p_ie.new_ie) {
        /* Free the new ProbeResp P2P IE */
        OS_FREE(p2p_go_handle->proberesp_p2p_ie.new_ie);
    }
    spin_lock_destroy(&p2p_go_handle->proberesp_p2p_ie.lock);

    if (p2p_go_handle->beacon_sub_ie) {
        /* Free the old IE */
        OS_FREE(p2p_go_handle->beacon_sub_ie);
    }

    OS_FREE(p2p_go_handle);
    return 0;
}


wlan_if_t wlan_p2p_GO_get_vap_handle(wlan_p2p_go_t p2p_go_handle)
{
    return p2p_go_handle->vap;
}


/**
 * register p2p event handler with p2p go object.
 * @param p2p_handle        : handle to the p2p GO object .
 * @param event_arg         : handle opaque to the implementor.
 * @handler                 : handler function to receive p2p related events.
 *                            if handler is NULL it unregisters the previously registered
 *                            handler. returns an error if handler is registered
 *                            already.
 *
 * @return on success return 0.
 *         on failure returns -ve value.
 */
int
wlan_p2p_GO_register_event_handlers(wlan_p2p_go_t p2p_go_handle,
                                 void *event_arg,
                                 wlan_p2p_event_handler handler)
{
    P2P_GO_LOCK(p2p_go_handle);
    if (handler && p2p_go_handle->event_handler) {
        if (handler != p2p_go_handle->event_handler) {
            P2P_GO_UNLOCK(p2p_go_handle);
            return -EINVAL;
        }
    }
    /* needs synchronization */
    p2p_go_handle->event_handler = handler;
    p2p_go_handle->event_arg = event_arg;
    p2p_go_handle->p2p_req_id = 0; /* don't need this on GO? device only? */
    P2P_GO_UNLOCK(p2p_go_handle);
    return 0;
}



static void
ieee80211_p2p_go_deliver_p2p_event(wlan_p2p_go_t p2p_go_handle, wlan_p2p_event *event)
{
    void *event_arg;
    wlan_p2p_event_handler handler;
    wlan_if_t vap = p2p_go_handle->vap;

    P2P_GO_LOCK(p2p_go_handle);
    handler = p2p_go_handle->event_handler;
    event_arg = p2p_go_handle->event_arg;
    P2P_GO_UNLOCK(p2p_go_handle);

#if ATH_DEBUG == 0
    /* Fix the compile error when ATH_DEBUG = 0 */
    vap = vap;
#endif

    if (handler) {
        event->req_id = p2p_go_handle->p2p_req_id;
        handler(event_arg, event);
    }
    switch(event->type) {
    case WLAN_P2PDEV_RX_FRAME:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:P2P_EVENT: RX_FRAME SA[" MACSTR "]: "
                          "frame type[%02x]\n", __func__, MAC2STR(event->u.rx_frame.src_addr),
                          event->u.rx_frame.frame_type );
        break;
    default:
        IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY,
                          "%s:P2P_EVENT: type %x \n",__func__, event->type);
        break;
    }
}

/* Returns 0 if the ssid is equal to mine */
static int
cmp_my_ssid(wlan_if_t vap, const u8 *ssid)
{
    wlan_node_t     ni = vap->iv_bss;

    ASSERT(ssid != NULL);
    ASSERT(ni != NULL);

    if ((ssid[1] != ni->ni_esslen) ||
        (OS_MEMCMP(ssid + 2, ni->ni_essid, ssid[1]) != 0))
    {
        return -EINVAL;
    }
    return 0;
}

/*
 * This is called to update the ProbeResp P2P IE with the new one.
 */
static void
update_proberesp_p2p_ie(wlan_p2p_go_t p2p_go_handle)
{
    spin_lock(&p2p_go_handle->proberesp_p2p_ie.lock);

    ASSERT(p2p_go_handle->proberesp_p2p_ie.new_ie);

    if (p2p_go_handle->proberesp_p2p_ie.curr_ie) {
        /* Free the current IE */
        OS_FREE(p2p_go_handle->proberesp_p2p_ie.curr_ie);
    }

    p2p_go_handle->proberesp_p2p_ie.curr_ie = p2p_go_handle->proberesp_p2p_ie.new_ie;
    p2p_go_handle->proberesp_p2p_ie.curr_ie_len = p2p_go_handle->proberesp_p2p_ie.new_ie_len;

    p2p_go_handle->proberesp_p2p_ie.new_ie = NULL;
    p2p_go_handle->proberesp_p2p_ie.new_ie_len = 0;

    spin_unlock(&p2p_go_handle->proberesp_p2p_ie.lock);
}

static int
ieee80211_p2p_go_input_filter_mgmt(wlan_node_t ni, wbuf_t wbuf, int subtype,
                                   struct ieee80211_rx_status *rs)
{
    int filter=0;
    u_int8_t *frm, *ie_data;
    u_int16_t frame_len,ie_len;
    struct ieee80211_frame *wh;
    struct p2p_parsed_ie pie;
    wlan_if_t vap = ieee80211_node_get_vap(ni);
    wlan_p2p_go_t           p2p_go_handle;
    osdev_t                 oshandle;
    wlan_p2p_event p2p_event;
    wlan_chan_t channel;
    u_int32_t freq;

    if (!ieee80211_vap_active_is_set(vap)) {
      return 0;
    }

    frm = wbuf_header(wbuf);
    wh = (struct ieee80211_frame *) frm;
    
    if( OS_MEMCMP(wh->i_addr1, "\xff\xff\xff\xff\xff\xff", IEEE80211_ADDR_LEN)!=0 && OS_MEMCMP(vap->iv_myaddr,wh->i_addr1,IEEE80211_ADDR_LEN)!=0 ) {
        return 0;
    }


    p2p_go_handle = (wlan_p2p_go_t) ieee80211vap_get_private_data(vap);
    oshandle = ieee80211com_get_oshandle(p2p_go_handle->devhandle);

    frame_len =  wbuf_get_pktlen(wbuf);
    channel = wlan_node_get_chan(ni);
    freq = wlan_channel_frequency(channel);
    ie_data = ieee80211_mgmt_iedata(wbuf,subtype);
    if (ie_data == NULL && subtype != IEEE80211_FC0_SUBTYPE_ACTION) {
        /* Error in finding the IE data. do nothing */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s SA[%s]: Freq[%4u], Frame MGMT subtype[%02x] "
                          "seq[0x%04x], No IE - DISCARDING !!\n", __func__, ether_sprintf(wh->i_addr2),
                          rs->rs_freq, subtype, le16toh(*(u_int16_t *)wh->i_seq));
        return filter;
    }
    ie_len = frame_len - (ie_data - frm);

    /*
     * filter out any assoc request and probe request frames without P2P IE.
     */
    switch(subtype) {
    case IEEE80211_FC0_SUBTYPE_ASSOC_REQ:
    case IEEE80211_FC0_SUBTYPE_REASSOC_REQ:
    case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
        /* reject if no P2P IE is present */
	    OS_MEMZERO(&pie, sizeof(pie));
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s MGMT subtype[%02x] DA[" MACSTR "] SA[" MACSTR "] "
                          "seq[%04x] freq[%4u]\n", __func__, subtype, MAC2STR(wh->i_addr1),
                          MAC2STR(wh->i_addr2), le16toh(*(u_int16_t *)wh->i_seq), freq);

        if (ieee80211_p2p_parse_ies(oshandle, vap, ie_data, ie_len, &pie) == 0) {
            /* *************** DEBUG NOTE ***************
               NOTE: Disabled the filter for now. Once the P2P client can send P2P IE, we
               can re-enable this filter. */
            filter=0;
            //filter=1;

            if ((subtype == IEEE80211_FC0_SUBTYPE_ASSOC_REQ) || 
                (subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ)) {
                if (pie.p2p_attributes) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: filter out Assoc Req frame SA[" MACSTR "]\n",
                                  __func__, MAC2STR(wh->i_addr2));
                    /* indicate peer side support p2p IE */
                    ni->ni_p2p_awared = 1;
                } else {
                    ni->ni_p2p_awared = 0;
                    if (p2p_go_handle->p2p_go_allow_p2p_clients_only) {
                        filter = 1;
                    }
                }
                
                /* Apply the max client limit, when a new P2P Client is trying to associate with the GO */
                if(!ieee80211_node_get_associd(ni) && ieee80211_mlme_get_num_assoc_sta(vap) >= p2p_go_handle->p2p_go_max_clients){
                    ieee80211_send_assocresp(ni,(subtype == IEEE80211_FC0_SUBTYPE_REASSOC_REQ),IEEE80211_STATUS_TOOMANY,NULL);
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: Assoc Req from SA[" MACSTR "] denied. GO allows at most 1 associated Client\n",
                                      __func__, MAC2STR(wh->i_addr2));
                    filter = 1;
                }
            } else if (subtype == IEEE80211_FC0_SUBTYPE_PROBE_REQ) {
                /*
                 * If is a p2p probe request with P2P wild card SSID,
                 * the mlme ap code will reject the frame and therefore, handle it here.
                 */

                p2p_event.type = WLAN_P2PDEV_RX_FRAME;
                p2p_event.u.rx_frame.frame_type = subtype;
                p2p_event.u.rx_frame.frame_len = frame_len;
                p2p_event.u.rx_frame.frame_buf = frm;
                p2p_event.u.rx_frame.ie_len = ie_len;
                p2p_event.u.rx_frame.ie_buf = ie_data;
                p2p_event.u.rx_frame.src_addr = wh->i_addr2;
                p2p_event.u.rx_frame.frame_rssi = rs->rs_rssi;
                p2p_event.u.rx_frame.freq = freq;
                p2p_event.u.rx_frame.chan_flags = wlan_channel_flags(channel);
                p2p_event.u.rx_frame.wbuf = wbuf;
                ieee80211_p2p_go_deliver_p2p_event(p2p_go_handle, &p2p_event);

                do {
                    /* Do not respond to wildcard ssid probes on hidden p2p devices */
                    if ((pie.ssid == NULL) || ((IEEE80211_VAP_IS_HIDESSID_ENABLED(vap)) && (pie.ssid[1] == 0))) {
                        /* No SSID or wildcard SSID found. Do not handle this here. */
                        break;
                    }
                    
                    /* 
                     * The GO will response to the probe request if the SSID is
                     * p2p wildcard, or wildcard, or equal to my ssid.
                     */
                    if (!pie.is_p2p_wildcard_ssid &&
                        (pie.ssid[1] != 0) &&
                        (cmp_my_ssid(vap, pie.ssid) != 0))
                    {
                        /* No acceptable SSID found. Do not handle this here. */
                        break;
                    }

                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, 
                                      "%s: probereq with valid SSID (p2p wildcard=%d, len=%d, cmp_my_ssid=%d, p2p_attrib=%d)\n",
                                      __func__, pie.is_p2p_wildcard_ssid, (int)pie.ssid[1], 
                                      (cmp_my_ssid(vap, pie.ssid)!=0), (pie.p2p_attributes!=0));

                    if (pie.rateset.rs_nrates == 0) {
                        /* No rates found. Do not handle this here. */
                        break;
                    }

                    filter=1;   /* From this point onwards, this frame will be filtered */

                    /* 
                     * Spec 2.4.1 P2P Devices shall not respond to Probe Request frames that 
                     * indicate support for 11b rates only
                     */
                    if (ieee80211_check_11b_rates(vap, &pie.rateset)) {
                        /* Reject this probe req */
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, 
                                          "%s: probereq with 11b rates only rejected.\n",
                                          __func__);
                        break;
                    }

                    if(pie.device_id &&
                       p2p_go_handle->p2p_device_address_valid &&
                       !IEEE80211_ADDR_EQ(p2p_go_handle->p2p_device_address, pie.device_id)) {
                        if(p2p_go_handle->p2p_client_device_addr_num){
                            u_int8_t i;

                            for(i = 0; i < p2p_go_handle->p2p_client_device_addr_num; i++){
                                if(IEEE80211_ADDR_EQ(p2p_go_handle->p2p_client_device_addr[i], pie.device_id))
                                    break;
                            }
                            
                            if(i == p2p_go_handle->p2p_client_device_addr_num){
                                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: Device ID doesn't match\n", __func__);
                                break;
                            }
                        }
                    }
                    if(pie.wps_attributes&&
                       pie.wps_attributes_parsed.num_req_dev_type &&
                       p2p_go_handle->wps_primary_device_type_valid){
                        u_int8_t i;

                        for(i = 0; i < pie.wps_attributes_parsed.num_req_dev_type; i++){
                            if(pie.wps_attributes_parsed.req_dev_type[i] &&
                               !OS_MEMCMP(pie.wps_attributes_parsed.req_dev_type[i], p2p_go_handle->wps_primary_device_type, WPS_DEV_TYPE_LEN))
                                break;
                        }
                        if(i == pie.wps_attributes_parsed.num_req_dev_type){
                            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: primary_dev_type not match our\n", __func__);
                            break;  
                        }
                    }

                    if (pie.p2p_attributes) {
                        int     ret;

                        /* Need to check if the P2P IE needs to be updated */
                        if (p2p_go_handle->proberesp_p2p_ie.new_ie != NULL) {
                            update_proberesp_p2p_ie(p2p_go_handle);
                        }

                        ret = ieee80211_send_proberesp(ni, wh->i_addr2, 
                                                       p2p_go_handle->proberesp_p2p_ie.curr_ie, 
                                                       p2p_go_handle->proberesp_p2p_ie.curr_ie_len, NULL);
                        if (ret != 0) {
                            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: sending of PROBE_RESP failed. ret=0x%x\n",
                                              __func__, ret);
                        }
                    }
                    else {
                        /* No P2P IE. We will still response but without p2p IE */
                        int     ret;
                        ret = ieee80211_send_proberesp(ni, wh->i_addr2, NULL, 0, NULL);
                        if (ret != 0) {
                            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: sending of PROBE_RESP(no p2p IE) failed. ret=0x%x\n",
                                              __func__, ret);
                        }
                    }

                } while ( false );
            }
            ieee80211_p2p_parse_free(&pie);
        }
        break;
    case IEEE80211_FC0_SUBTYPE_ACTION:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s ACTION DA[" MACSTR "] SA[" MACSTR "] "
                          "seq[%04x] freq[%4u]\n", __func__, MAC2STR(wh->i_addr1), MAC2STR(wh->i_addr2),
                          le16toh(*(u_int16_t *)wh->i_seq), freq);
        p2p_event.type = WLAN_P2PDEV_RX_FRAME;
        p2p_event.u.rx_frame.frame_type = subtype;
        p2p_event.u.rx_frame.frame_len = frame_len;
        p2p_event.u.rx_frame.frame_buf = frm;
        p2p_event.u.rx_frame.ie_len = ie_len;
        p2p_event.u.rx_frame.ie_buf = ie_data;
        p2p_event.u.rx_frame.src_addr = wh->i_addr2;
        p2p_event.u.rx_frame.frame_rssi = rs->rs_rssi;
        p2p_event.u.rx_frame.freq = freq;
        p2p_event.u.rx_frame.chan_flags = wlan_channel_flags(channel);
        p2p_event.u.rx_frame.wbuf = wbuf;
        ieee80211_p2p_go_deliver_p2p_event(p2p_go_handle, &p2p_event);
        filter = 0; /* should this get filtered? */
        break;
    }
    return filter;
}

/*
 * Add the P2P IE to the frame.
 * NOTE: assumed that the lock is held.
 */
static u_int8_t *p2p_go_add_p2p_ie(
    wlan_p2p_go_t           p2p_go_handle,
    ieee80211_frame_type    ftype, 
    u_int8_t                *efrm)
{
    struct ieee80211_p2p_ie *p2pie;
    u_int8_t                *start_efrm = efrm;

    /* if there is nothing to add, just return */

    if ( (p2p_go_handle->p2p_module_sub_ie_len + p2p_go_handle->noa_sub_ie_len) == 0) {
        return efrm;
    }

    p2pie = (struct ieee80211_p2p_ie *) efrm;

    efrm = ieee80211_p2p_add_p2p_ie(efrm);

    if (ftype == IEEE80211_FRAME_TYPE_BEACON) {
        /* Add the beacon frame specific blob */
        OS_MEMCPY(efrm, p2p_go_handle->beacon_sub_ie, p2p_go_handle->beacon_sub_ie_len);
        efrm += p2p_go_handle->beacon_sub_ie_len;
    }

    /* Assumed that the order of sub-attributes is not important */
    /* Add the blob from P2P module */
    OS_MEMCPY(efrm, p2p_go_handle->p2p_module_sub_ie, p2p_go_handle->p2p_module_sub_ie_len);
    efrm += p2p_go_handle->p2p_module_sub_ie_len;

    /* Add the blob from NOA sub-attributes */
    OS_MEMCPY(efrm, p2p_go_handle->noa_sub_ie, p2p_go_handle->noa_sub_ie_len);
    efrm += p2p_go_handle->noa_sub_ie_len;

    /* TODO: we don't support fragmentation of P2P IE for now */
    ASSERT(((efrm - start_efrm) - 2) <= (int)0x0FF);

    /* update the IE len. Minus 2 for Element ID and Len */
    p2pie->p2p_len = (efrm - start_efrm) - 2;

    return efrm;
}

/*
 * Function to update the P2P IE for beacon frame.
 */
static void ieee80211_p2p_go_update_beacon_p2p_ie(
    wlan_p2p_go_t       p2p_go_handle)
{
    int                     ie_len;
    int                     alloc_ie_len;
    u_int8_t                *efrm, *ie_buf;
    osdev_t                 oshandle;

    spin_lock(&(p2p_go_handle->lock));

    /* TODO: the IE subattributes from P2P module is not enabled */
    ie_len = p2p_go_handle->p2p_module_sub_ie_len +
             p2p_go_handle->beacon_sub_ie_len +
             p2p_go_handle->noa_sub_ie_len;

    if (ie_len == 0 ) {
        /* there is nothing to add, remove existoing one if any */
        if (wlan_mlme_app_ie_set(p2p_go_handle->app_ie_handle, IEEE80211_FRAME_TYPE_BEACON,
                            NULL, 0) != 0) {
            IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY,
                         "%s wlan_mlme_app_ie_set 0 len failed \n",__func__);
        }  
        spin_unlock(&(p2p_go_handle->lock));
        return;
    }

    ie_len += 4;                /* 4 octets for P2P IE OUI */
    alloc_ie_len = ie_len + 2;  /* 2 octets for Element ID and len */

    /* TODO: need to handle fragmentation */
    ASSERT(ie_len <= (int)0x0FF);

    oshandle = ieee80211com_get_oshandle(p2p_go_handle->devhandle);
    ie_buf = (u_int8_t *)OS_MALLOC(oshandle, alloc_ie_len, 0);
    if (ie_buf == NULL) {
        IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY,
                          "%s: unable to alloc memory. size=%d\n",
                          __func__, alloc_ie_len);
        spin_unlock(&(p2p_go_handle->lock));
        return;
    }

    efrm = ie_buf;
    efrm = p2p_go_add_p2p_ie(p2p_go_handle, IEEE80211_FRAME_TYPE_BEACON, efrm);

    ASSERT((efrm - ie_buf) == alloc_ie_len);

    spin_unlock(&(p2p_go_handle->lock));

    if (wlan_mlme_app_ie_set(p2p_go_handle->app_ie_handle, IEEE80211_FRAME_TYPE_BEACON,
                            ie_buf, alloc_ie_len) != 0) {
        IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY,
                          "%s wlan_mlme_app_ie_set failed \n",__func__);
    }

    OS_FREE(ie_buf);
}

static bool ieee80211_check_for_p2p_ie(u_int8_t *buf, u_int16_t buflen)
{
	const u8 *end, *pos;
    const u_int8_t  wfa_oui[3] = IEEE80211_P2P_WFA_OUI;

    pos = buf;
    end = buf +  buflen;

    while (pos + 1 < end) {
        if (pos + 2 + pos[1] > end) {
            return false;
        }
        if (pos[0] == IEEE80211_P2P_SUB_ELEMENT_VENDOR &&
            pos[1] >= 4 &&
            pos[2] == wfa_oui[0] &&
            pos[3] == wfa_oui[1] &&
            pos[4] == wfa_oui[2] &&
            pos[5] == IEEE80211_P2P_WFA_VER) {

            return true;
        }
        pos += 2 + pos[1];
    }
     
    return false;
}

static int ieee80211_p2p_go_output_filter(wbuf_t wbuf)
{
    u_int8_t *efrm;
    struct ieee80211_frame *wh;
    int subtype;
    struct ieee80211_node   *ni = NULL;
    wlan_p2p_go_t           p2p_go_handle = NULL;
    wlan_if_t vap = NULL;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

    efrm = wbuf_header(wbuf) + wbuf_get_pktlen(wbuf);
    ni = wlan_wbuf_get_peer_node(wbuf);
    p2p_go_handle = (wlan_p2p_go_t) ieee80211vap_get_private_data(ni->ni_vap);
    vap = ieee80211_node_get_vap(ni);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s MGMT subtype[%02x] DA[" MACSTR "] SA[" MACSTR "] "
                      "seq[%04x]\n", __func__, subtype, MAC2STR(wh->i_addr1), MAC2STR(wh->i_addr2),
                      le16toh(*(u_int16_t *)wh->i_seq));
    switch(subtype) {
    case  IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
    case  IEEE80211_FC0_SUBTYPE_REASSOC_RESP:
        if (ni->ni_p2p_awared) {
            u_int16_t status;
            u_int8_t *frm;
            enum p2p_status_code p2p_status;

            frm = (u_int8_t *)&wh[1];
            frm += 2;
            status = le16toh(*(u_int16_t *)frm);
            switch (status) {
                case IEEE80211_STATUS_SUCCESS:
                    p2p_status = P2P_SC_SUCCESS;
                    break;
                case IEEE80211_REASON_ASSOC_TOOMANY:
                    p2p_status = P2P_SC_FAIL_LIMIT_REACHED;
                    break;
                default:
                    p2p_status = P2P_SC_FAIL_INVALID_PARAMS;
                    break;
            }

            if (ni->ni_p2p_assocstatus) {
                p2p_status = ni->ni_p2p_assocstatus;
            }

            frm = efrm;
            efrm = ieee80211_p2p_add_p2p_ie(efrm);
            if (p2p_status != P2P_SC_SUCCESS) {
                struct ieee80211_p2p_ie *p2p_ie = (struct ieee80211_p2p_ie *)frm;

                /*
                 * [1] p2p Sub element id
                 * [2] attr len
                 * [1] status code
                 */
                efrm[0] = IEEE80211_P2P_SUB_ELEMENT_STATUS;
                efrm++;
                P2PIE_PUT_LE16(efrm, 1);
                efrm++;
                efrm++;
                efrm[0] = (u_int8_t)p2p_status;
                efrm++;
                p2p_ie->p2p_len += 4;
            }
            /* STNG TODO: consider adding the "Extended Listen Timing" sub-attribute */

            wbuf_set_pktlen(wbuf, (efrm - (u_int8_t *)wbuf_header(wbuf)) ) ;
        }
        break;
    case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
    {
        u_int16_t frame_len,ie_len;
        u_int8_t *frm, *ie_data;

        ie_data = ieee80211_mgmt_iedata(wbuf,subtype);
        frm = wbuf_header(wbuf);
        frame_len =  wbuf_get_pktlen(wbuf);
        ie_len = frame_len - (ie_data - frm);

        /* XXXX quick HACK to not include NOA IE for legacy probe responses.
         * check if probe response has p2p ie in it already, if so  then add  noa  
         *  if probe response does not have p2p ie then it is being send to legacy device, so do not
         * add noa attribute.
         */
        if (ieee80211_check_for_p2p_ie(ie_data, ie_len)) {

            ASSERT(p2p_go_handle->specialmarker == P2P_GO_HANDLE_MARKER);

            spin_lock(&(p2p_go_handle->lock));

            efrm = p2p_go_add_p2p_ie(p2p_go_handle, IEEE80211_FRAME_TYPE_PROBERESP, efrm);

            spin_unlock(&(p2p_go_handle->lock));

            wbuf_set_pktlen(wbuf, (efrm - (u_int8_t *)wbuf_header(wbuf)) ) ;

        }
        break;
    }
    default:
        break;
    }
    return 0;
}

/*
 * Inform the P2P GO that there is a new P2P sub-attributes (excluding the NOA subattribute).
 * p2p_sub_ie will contains the new P2P sub-attributes.
 */
int wlan_p2p_go_update_p2p_ie(
    wlan_p2p_go_t   p2p_go_handle,
    u_int8_t        *p2p_sub_ie,
    u_int16_t       p2p_sub_ie_len)
{
    spin_lock(&(p2p_go_handle->lock));

    if (p2p_go_handle->beacon_sub_ie) {
        /* Free the old IE */
        OS_FREE(p2p_go_handle->beacon_sub_ie);
        p2p_go_handle->beacon_sub_ie = NULL;
        p2p_go_handle->beacon_sub_ie_len = 0;
    }

    p2p_go_handle->beacon_sub_ie = p2p_sub_ie;
    p2p_go_handle->beacon_sub_ie_len = p2p_sub_ie_len;

    spin_unlock(&(p2p_go_handle->lock));

    /* Update the BEACON IE's */
    ieee80211_p2p_go_update_beacon_p2p_ie(p2p_go_handle);

    return EOK;
}

/*
 * Inform the P2P GO that there is a new NOA IE.
 * new_noa_ie will contains the new NOA IE structure.
 * If new_noa_ie is NULL, then disable the append of NOA IE.
 */
void wlan_p2p_go_update_noa_ie(
    wlan_p2p_go_t                           p2p_go_handle,
    struct ieee80211_p2p_sub_element_noa    *new_noa_ie,
    u_int32_t                               tsf_offset)
{
    spin_lock(&(p2p_go_handle->lock));

    if (new_noa_ie) {

        if ((new_noa_ie->ctwindow == 0) && (new_noa_ie->oppPS == 0) &&
            (new_noa_ie->num_descriptors == 0)) {
            /* No need to add NOA IE */
            p2p_go_handle->noa_sub_ie_len = 0;  /* no NOA IE sub-attributes */
        }
        else {
            /* Create the binary blob containing NOA sub-IE */
            u_int8_t    *efrm;
            u_int8_t    *start_efrm;
            u_int16_t   len;

            start_efrm = &p2p_go_handle->noa_sub_ie[0];
            efrm = ieee80211_p2p_add_noa(start_efrm, new_noa_ie, tsf_offset);
            len = efrm - start_efrm;
            ASSERT(len <= sizeof(p2p_go_handle->noa_sub_ie));
            p2p_go_handle->noa_sub_ie_len = len;
        }
    }
    else {
        p2p_go_handle->noa_sub_ie_len = 0;  /* no NOA IE sub-attributes */
    }

    spin_unlock(&(p2p_go_handle->lock));

    /* Update the BEACON IE's */
    ieee80211_p2p_go_update_beacon_p2p_ie(p2p_go_handle);
}

#define     TIME_UNIT_TO_MICROSEC   1024    /* 1 TU equals 1024 microsecs */

/* Get the beacon interval in microseconds */
static inline u_int32_t get_beacon_interval(wlan_if_t vap) 
{
    /* The beacon interval is in terms of Time Units */
    return(vap->iv_bss->ni_intval * TIME_UNIT_TO_MICROSEC);
}

#define MIN_TSF_TIMER_TIME  5000 /* 5 msec */

/* Calculate the next TBTT time in terms of microseconds */
static u_int32_t get_next_tbtt_tsf_32(wlan_if_t vap) 
{
   struct ieee80211com *ic = vap->iv_ic;
   u_int64_t           curr_tsf64, nexttbtt_tsf64;
   u_int32_t           bintval; /* beacon interval in micro seconds */
   
   curr_tsf64 = ic->ic_get_TSF64(ic);
   /* calculate the next tbtt */ 

   bintval = get_beacon_interval(vap);
    
   nexttbtt_tsf64 =  curr_tsf64 + bintval;
   nexttbtt_tsf64  = nexttbtt_tsf64 - OS_MOD64_TBTT_OFFSET(nexttbtt_tsf64, bintval);
   if ((nexttbtt_tsf64 - curr_tsf64) < MIN_TSF_TIMER_TIME ) {  /* if the immediate next tbtt is too close go to next one */
       nexttbtt_tsf64 += bintval;
   }
   return (u_int32_t) nexttbtt_tsf64;
}

/*
 * Routine to set a new NOA schedule into the GO vap.
 */
int wlan_p2p_GO_set_noa_schedule(
    wlan_p2p_go_t                   p2p_go_handle,
    u_int8_t                        num_noa_schedules,
    wlan_p2p_go_noa_req             *request)
{
    int     ret = 0;
    int i, j;
    ieee80211_p2p_go_schedule_req go_schedules[IEEE80211_MAX_NOA_DESCRIPTORS];
    u_int32_t  nexttbtt;

    nexttbtt = get_next_tbtt_tsf_32(p2p_go_handle->vap);

    for (i = 0, j = 0; i < num_noa_schedules; ++i) {

        /* If the duration is zero, then skip it. */
        if (request[i].duration == 0) {
            IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_POWER, 
                              "%s: Entry=%d, duration is zero. Skip it.\n",
                              __func__, i);
            continue;
        }

        go_schedules[j].type = IEEE80211_P2P_GO_ABSENT_SCHEDULE;
        go_schedules[j].pri = i + 1;    /* Lower is higher priority */
        go_schedules[j].noa_descriptor.type_count = request[i].num_iterations;
        go_schedules[j].noa_descriptor.interval = get_beacon_interval(p2p_go_handle->vap);
        /* Convert from msec to microseconds */
        go_schedules[j].noa_descriptor.duration = request[i].duration * 1000;
        go_schedules[j].noa_descriptor.start_time = nexttbtt + (request[i].offset_next_tbtt * 1000);

        IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_POWER, "%s: noa update duration %d num_iterations %d start_time %d \n",
                          __func__,request[i].duration, request[i].num_iterations,request[i].offset_next_tbtt);

        j++;
    }

    ret = wlan_p2p_GO_ps_set_noa_schedule(p2p_go_handle->p2p_go_ps, j, go_schedules);

    return ret;

}

/* 
 * Check to see if we need to change the Beacon suppression state.
 * NOTE: assumed that the lock is held.
 */
static void
p2p_GO_ps_check_suppress_beacon(
    wlan_p2p_go_t p2p_go_handle,
    bool have_sta_associated)
{
    int retval;

    if (p2p_go_handle->en_suppress_beacon) {
        if (have_sta_associated && p2p_go_handle->beacon_is_suppressed) {
            /* Unsuspend the beacon since there are associated stations */
            retval = ieee80211_mlme_set_beacon_suspend_state(p2p_go_handle->vap, false);
            if (retval != 0) {
                IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_MLME, 
                    "%s: called. Error=%d from ieee80211_mlme_ap_set_beacon_suspend_state()\n", __func__, retval);
            }
            p2p_go_handle->beacon_is_suppressed = false;
        }
        else if (!have_sta_associated && !p2p_go_handle->beacon_is_suppressed) {
            /* Suspend the beacon since there are no associated stations */
            retval = ieee80211_mlme_set_beacon_suspend_state(p2p_go_handle->vap, true);
            if (retval != 0) {
                IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_MLME, 
                    "%s: called. Error=%d from ieee80211_mlme_ap_set_beacon_suspend_state()\n", __func__, retval);
            }
            p2p_go_handle->beacon_is_suppressed = true;
        }
    }
    else {
        /* No beacon suppression */
        if (p2p_go_handle->beacon_is_suppressed) {

            retval = ieee80211_mlme_set_beacon_suspend_state(p2p_go_handle->vap, false);
            if (retval != 0) {
                IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_MLME, 
                    "%s: called. Error=%d from ieee80211_mlme_ap_set_beacon_suspend_state()\n", __func__, retval);
            }
            p2p_go_handle->beacon_is_suppressed = false;
        }
    }
}

static void
go_mlme_event_handler(ieee80211_vap_t vap, ieee80211_mlme_event *event, void *arg)
{
    wlan_p2p_go_t   p2p_go_handle = (wlan_p2p_go_t) arg;
    bool            have_sta_associated;

    spin_lock(&(p2p_go_handle->lock));

    /* Check to see if we need to change the Beacon suppression state. */
    have_sta_associated = (ieee80211_mlme_get_num_assoc_sta(p2p_go_handle->vap) != 0);
    p2p_GO_ps_check_suppress_beacon(p2p_go_handle, have_sta_associated);
    
    /*
     * turn on the offchannel scheduler to schedule the time for scanner 
     * offchannel requests only when a STA is associated. if no sta is 
     * then do not turn off scheduler. 
     * this is to reduce the total scan time when no stations are associated.
     * with the scheduler on , the scheduker schedules each offchannel request
     * with a delay of 2 beacon intervals and also update the GO with the NOA
     * info to go off channel. this is to allow 2 beacons to go out with the NOA
     * info to allow p2p clients to receive the NOA. this delays the scan 
     * operation significantly (300msec ~3 beacon intervals for each channel ).
     */   
    if(have_sta_associated && p2p_go_handle->use_scheduler_for_scan) {
        ieee80211_vap_needs_scheduler_set(vap);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, 
                    "%s: non zero station associated , use scheduler for scan \n", __func__);
    } else {
        ieee80211_vap_needs_scheduler_clear(vap);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, 
                    "%s: no station associated , do not use scheduler for scan \n", __func__);
    }
    spin_unlock(&(p2p_go_handle->lock));
}

/*
 * Routine to set the Beacon Suppression when no station associated.
 */
int wlan_p2p_GO_set_suppress_beacon(
    wlan_p2p_go_t   p2p_go_handle,
    bool            enable_suppression)
{
    int     retval = 0;
    bool    have_sta_associated;

    spin_lock(&(p2p_go_handle->lock));

    do {

        if (p2p_go_handle->en_suppress_beacon == enable_suppression) {
            /* No change in state. Ignored. */
            IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_MLME, 
                              "%s: No change in state. suppress_beacon=%d\n",
                              __func__, enable_suppression);
            break;
        }

        IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_MLME, 
                          "%s: New state of suppress_beacon=%d\n",
                          __func__, enable_suppression);

        p2p_go_handle->en_suppress_beacon = enable_suppression;

    } while ( false );

    if (retval == 0) {
        /* Check to see if we need to change the Beacon suppression state. */
        have_sta_associated = (ieee80211_mlme_get_num_assoc_sta(p2p_go_handle->vap) != 0);
        p2p_GO_ps_check_suppress_beacon(p2p_go_handle, have_sta_associated);
    }

    spin_unlock(&(p2p_go_handle->lock));

    return retval;
}

/**
 * To set a parameter in the P2P GO module.
 * @param p2p_go_handle         : handle to the p2p GO object.
 * @param param_type            : type of parameter to set.
 * @param param                 : new parameter.
 * @return Error Code. Equal 0 if success.
 */
int wlan_p2p_go_set_param(wlan_p2p_go_t p2p_go_handle, 
                          wlan_p2p_go_param_type param_type, u_int32_t param)
{
    int     ret = 0;

    P2P_GO_LOCK(p2p_go_handle);

    switch(param_type) {
    case WLAN_P2PGO_CTWIN:
        /* CT Window */
        if (p2p_go_handle->p2p_go_ps == NULL) {
            IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY, 
                              "%s: Error: GO PS not attached.\n", __func__);  
            ret = EINVAL;
            break;
        }
        ret = ieee80211_p2p_go_power_set_param(p2p_go_handle->p2p_go_ps, P2P_GO_PS_CTWIN, param);
        break;

    case WLAN_P2PGO_OPP_PS:
        /* Opportunistic Power Save */
        if (p2p_go_handle->p2p_go_ps == NULL) {
            IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY, 
                              "%s: Error: GO PS not attached.\n", __func__);  
            ret = EINVAL;
            break;
        }
        ret = ieee80211_p2p_go_power_set_param(p2p_go_handle->p2p_go_ps, P2P_GO_PS_OPP_PS, param);
        break;

    case WLAN_P2PGO_USE_SCHEDULER_FOR_SCAN:
        /* 
         * if set , turns on scheduler for scan operations and with scheduer on
         *  the scan operations are scheduled inbetween becaons and also NOA will 
         *  announced in the beacons to indicate to connected clients when the scanner
         *  goes off channel.
         */

         p2p_go_handle->use_scheduler_for_scan =  ( param != 0);
         if (p2p_go_handle->use_scheduler_for_scan) {
            ieee80211_vap_needs_scheduler_set(p2p_go_handle->vap);
         } else {
            ieee80211_vap_needs_scheduler_clear(p2p_go_handle->vap);
         }
        break; /* ret = 0 (success) */ 
    case WLAN_P2PGO_ALLOW_P2P_CLIENTS_ONLY:
        /* non-zero param disables association for non-P2P aware clients */
        p2p_go_handle->p2p_go_allow_p2p_clients_only = (param != 0);
        break;
    case WLAN_P2PGO_MAX_CLIENTS:
        p2p_go_handle->p2p_go_max_clients = (u_int16_t)param;
        break;
    default:
        if (wlan_p2p_prot_go_is_attach(p2p_go_handle)) {
            ret = wlan_p2p_prot_go_set_param(p2p_go_handle, param_type, param);
        }
        else
            ret = EINVAL;
    }

    P2P_GO_UNLOCK(p2p_go_handle);
    return ret;
}

/**
 * To set a parameter array in the P2P GO module.
 *  @param p2p_go_handle : handle to the p2p GO object.
 *  @param param         : config paramaeter.
 *  @param byte_arr      : byte array .
 *  @param len           : length of byte array.
 *  @return 0  on success and -ve on failure.
 */
int
wlan_p2p_go_set_param_array(wlan_p2p_go_t p2p_go_handle, wlan_p2p_go_param_type param,
                            u_int8_t *byte_arr, u_int32_t len)
{
    int     ret = EOK;

    P2P_GO_LOCK(p2p_go_handle);
    switch (param) {
    /* For now, there is no Non-Protocol parameter array defined */
    default:
    if (wlan_p2p_prot_go_is_attach(p2p_go_handle)) {
        ret = wlan_p2p_prot_go_set_param_array(p2p_go_handle, param, byte_arr, len);
    }
    else
        ret = EINVAL;
    }
    P2P_GO_UNLOCK(p2p_go_handle);

    return ret;
}

int wlan_p2p_GO_get_noa_info(wlan_p2p_go_t p2p_go_handle, wlan_p2p_noa_info *noa_info)
{
    if (p2p_go_handle->noa_sub_ie_len) {
        int i;
        struct ieee80211com *ic = p2p_go_handle->vap->iv_ic;
        struct ieee80211_p2p_sub_element_noa noa;
        int noa_num_descriptors =
                (p2p_go_handle->noa_sub_ie_len - 2)/IEEE80211_P2P_NOA_DESCRIPTOR_LEN;
        OS_MEMZERO(&noa, sizeof(struct ieee80211_p2p_sub_element_noa));
        /* skip p2p subelement ID and length in noa_sub_ie buffer */
        ieee80211_p2p_parse_noa(&p2p_go_handle->noa_sub_ie[3], noa_num_descriptors, &noa);
        noa_info->index = noa.index;
        noa_info->oppPS = noa.oppPS;
        noa_info->ctwindow = noa.ctwindow;
        noa_info->num_descriptors = noa.num_descriptors;
        for (i=0; i < noa.num_descriptors; ++i) {
            noa_info->noa_descriptors[i].type_count = noa.noa_descriptors[i].type_count;
            noa_info->noa_descriptors[i].duration = noa.noa_descriptors[i].duration;
            noa_info->noa_descriptors[i].start_time = noa.noa_descriptors[i].start_time;
            noa_info->noa_descriptors[i].interval = noa.noa_descriptors[i].interval;
        }
       noa_info->cur_tsf32 = ic->ic_get_TSF32(ic);
   } else {
        OS_MEMZERO(noa_info, sizeof(wlan_p2p_noa_info));
        return EINVAL;
   }
   return EOK;
}

/*
 * This routine will store the new ProbeResp P2P IE.
 */
void
ieee80211_p2p_go_store_proberesp_p2p_ie(wlan_p2p_go_t p2p_go_handle, u8 *p2p_ie_ptr, u_int16_t p2p_ie_len)
{
    spin_lock(&p2p_go_handle->proberesp_p2p_ie.lock);

    if (p2p_go_handle->proberesp_p2p_ie.new_ie) {
        /* Free the new IE */
        IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY,
                          "%s: Already have new p2p ie len %d. Overwriting it.\n",
                          __func__, p2p_go_handle->proberesp_p2p_ie.new_ie_len);
        OS_FREE(p2p_go_handle->proberesp_p2p_ie.new_ie);
    }

    p2p_go_handle->proberesp_p2p_ie.new_ie = p2p_ie_ptr;
    p2p_go_handle->proberesp_p2p_ie.new_ie_len = p2p_ie_len;

    spin_unlock(&p2p_go_handle->proberesp_p2p_ie.lock);
}

/**
 * set application specific ie buffer for a given frame. This
 * routine will extract the P2P IE and store in
 * p2p_go_handle->p2p_ie. Only the non P2P IE are plumbed down.
 *
 * 
 * @param vaphandle     : handle to the vap.
 * @param ftype         : frame type.   
 * @param buf           : appie buffer.
 * @param buflen        : length of the buffer.
 * @return  0  on success and -ve on failure.
 * once the buffer is set for a perticular frame the MLME keep setting up the ie in to the frame 
 * every time the frame is sent out. passing 0 value to buflen would remove the appie biffer.
 */
int wlan_p2p_GO_set_appie(wlan_p2p_go_t p2p_go_handle, ieee80211_frame_type ftype, u_int8_t *buf, u_int16_t buflen, u_int8_t identifier)
{
    int rc;
    u8 *ie_buf, *p2p_ie_buf;
    osdev_t  oshandle;
    const u_int8_t  wfa_oui[3] = IEEE80211_P2P_WFA_OUI;
	const u8 *end, *pos;
    u8 *p2p_ie_ptr,*ie_ptr;
    u_int16_t ie_len=0, p2p_ie_len=0;
    struct ieee80211vap *vap_handle;

    if (ftype != IEEE80211_FRAME_TYPE_PROBERESP) {
        return EINVAL;
    }
    /* 
     * parse and divide the ie into 2 parts p2p ie and non p2p ie.
     * save p2p ie into local area and setup non p2p ie into the
     * mlme.
     */  
    oshandle = ieee80211com_get_oshandle(p2p_go_handle->devhandle);
    ie_buf = (u_int8_t *)OS_MALLOC(oshandle,buflen , 0);
    if (!ie_buf) {
        return ENOMEM;
    }
    ie_ptr = ie_buf;

    /* Allocate another buffer for P2P IE */
    p2p_ie_buf = (u_int8_t *)OS_MALLOC(oshandle, buflen , 0);
    if (!p2p_ie_buf) {
        OS_FREE(ie_buf);
        return ENOMEM;
    }
    p2p_ie_ptr = p2p_ie_buf;

    pos = buf;
    end = buf +  buflen;

    while (pos + 1 < end) {
        if (pos + 2 + pos[1] > end) {
            printk("%s: invalid ie setup by os shim\n", __func__);
            OS_FREE(ie_buf);
            return EINVAL;
        }
        if (pos[0] == IEEE80211_P2P_SUB_ELEMENT_VENDOR &&
            pos[1] >= 4 &&
            pos[2] == wfa_oui[0] &&
            pos[3] == wfa_oui[1] &&
            pos[4] == wfa_oui[2] &&
            pos[5] == IEEE80211_P2P_WFA_VER) {
            /* p2p ie , copy to p2p buffer */
            OS_MEMCPY(p2p_ie_ptr, pos, 2 + pos[1]);
            p2p_ie_len += (2+pos[1]);
            p2p_ie_ptr += (2+pos[1]);
        } else {
            /* non p2p ie , copy to non p2p buffer */
            OS_MEMCPY(ie_ptr, pos, 2 + pos[1]);
            ie_len += (2+pos[1]);
            ie_ptr += (2+pos[1]);
        }
        pos += 2 + pos[1];
    }
    /* setup non p2p ie with appie buf */
    vap_handle = p2p_go_handle->vap;
    rc = wlan_mlme_app_ie_set_check(vap_handle, ftype, ie_buf, ie_len, identifier)
    IEEE80211_DPRINTF(p2p_go_handle->vap, IEEE80211_MSG_ANY, 
        "%s: p2p ie len %d non p2p ie len %d \n", __func__, p2p_ie_len,ie_len);
    OS_FREE(ie_buf);

    /* Store the P2P IE */
    ieee80211_p2p_go_store_proberesp_p2p_ie(p2p_go_handle, p2p_ie_buf, p2p_ie_len);

    return rc;
}

/**
 *parse p2p ie for a given ie buffer..
 * 
 * @param vaphandle     : handle to the vap.
 * @param ftype         : frame type.   
 * @param buf           : appie buffer.
 * @param buflen        : length of the buffer.
 * @return  0  on success and -ve on failure.
 */
int wlan_p2p_GO_parse_appie(wlan_p2p_go_t p2p_go_handle, ieee80211_frame_type ftype, u_int8_t *buf, u_int16_t buflen)
{
    int i, rc = 0;
    osdev_t oshandle;
    struct p2p_parsed_ie pie;

    if (ftype != IEEE80211_FRAME_TYPE_PROBERESP) {
        return EINVAL;
    }

    if (buflen == 0){
        p2p_go_handle->p2p_device_address_valid = 0;
        p2p_go_handle->wps_primary_device_type_valid = 0;
        p2p_go_handle->p2p_client_device_addr_num = 0;  
        
        return rc;
    }

    oshandle = ieee80211com_get_oshandle(p2p_go_handle->devhandle);

    OS_MEMZERO(&pie, sizeof(pie));
    rc = ieee80211_p2p_parse_ies(oshandle, p2p_go_handle->vap, buf, buflen, &pie);
    if(!rc){
        /* save device address for error checking */
        if(pie.p2p_device_addr){
            OS_MEMCPY(p2p_go_handle->p2p_device_address, pie.p2p_device_addr, IEEE80211_ADDR_LEN);
            p2p_go_handle->p2p_device_address_valid = 1;
        } else {
            p2p_go_handle->p2p_device_address_valid = 0;
        }

        /* save client device address for error checking */
        if(pie.group_info && pie.group_info_len){
            struct p2p_group_info group_info;
            
            if(!ieee80211_p2p_parse_group_info(pie.group_info, pie.group_info_len, &group_info)){
                for(i = 0; i < group_info.num_clients; i++)
                    OS_MEMCPY(p2p_go_handle->p2p_client_device_addr[i], group_info.client[i].p2p_device_addr, IEEE80211_ADDR_LEN);
                p2p_go_handle->p2p_client_device_addr_num = group_info.num_clients;
            } else {
                p2p_go_handle->p2p_client_device_addr_num = 0;
            }
        }

        /* save primary_dev_type for error checking */
        if(pie.wps_attributes){
            OS_MEMCPY(p2p_go_handle->wps_primary_device_type, pie.wps_attributes_parsed.primary_dev_type, WPS_DEV_TYPE_LEN);
            p2p_go_handle->wps_primary_device_type_valid = 1;
        } else {
            p2p_go_handle->wps_primary_device_type_valid = 0;
        }
    }
    ieee80211_p2p_parse_free(&pie);

    return rc;
}

#if P2P_TEST_CODE
/************** Private P2P GO Power Save TEST CODE ******************/
int p2p_private_noa_go_ps_test_start(
    wlan_p2p_go_t p2p_go_handle, ieee80211_p2p_go_ps_t p2p_go_ps, int testnum);

int
p2p_private_noa_go_schedule_test_start(wlan_p2p_go_t p2p_go_handle, int testnum)
{
    if (p2p_go_handle->p2p_go_ps == NULL) {
        printk("%s: not starting test since p2p_go_handle->p2p_go_ps is NULL\n", __func__);
        return 0;
    }
    return(p2p_private_noa_go_ps_test_start(p2p_go_handle, p2p_go_handle->p2p_go_ps, testnum));
}

#endif  //P2P_TEST_CODE

#if TEST_VAP_PAUSE

extern int ath_vap_pause_time; /* exposed from osdep via sysctl */

static OS_TIMER_FUNC(test_vap_pause_timer_handler)
{
    wlan_p2p_go_t p2p_go_handle;
    int pause_time = ath_vap_pause_time;

    /* pause time range is 20 to 80msec,assuming 100msec beacon interval */
    if (pause_time < 20 ) pause_time=20;
    if (pause_time > 80 ) pause_time=80;

    OS_GET_TIMER_ARG(p2p_go_handle, wlan_p2p_go_t);

    if (!p2p_go_handle->pause_state) {
        ieee80211_vap_force_pause(p2p_go_handle->vap, 0);
        p2p_go_handle->pause_state=1;
        OS_SET_TIMER(&p2p_go_handle->pause_timer, pause_time);
    } else {
        ieee80211_vap_force_unpause(p2p_go_handle->vap, 0);
        p2p_go_handle->pause_state=0;
    }
}

static void test_pause_tx_bcn_notify( ieee80211_tx_bcn_notify_t h_notify,
    int beacon_id, u_int32_t tx_status, void *arg)
{
    wlan_p2p_go_t p2p_go_handle = (wlan_p2p_go_t) arg;
    if (p2p_go_handle->pause_state) {
        ieee80211_vap_force_unpause(p2p_go_handle->vap, 0);
        p2p_go_handle->pause_state=0;
        OS_CANCEL_TIMER(&p2p_go_handle->pause_timer);
    }
    if (!ath_vap_pause_time) {
        return;
    }

    /* start   10 msec timer */
    OS_SET_TIMER(&p2p_go_handle->pause_timer, 10);
}

#endif

#endif  //UMAC_SUPPORT_P2P

