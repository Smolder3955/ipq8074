/*
 * Copyright (c) 2011-2016,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#include "ieee80211_var.h"
#include "ieee80211_vap_tsf_offset.h"
#include "ieee80211_p2p_proto.h"
#include "ieee80211P2P_api.h"
#include "ieee80211_ie_utils.h"
#include "ieee80211_channel.h"
#include "ieee80211_p2p_ie.h"
#include "ieee80211_p2p_client.h"
#include "ieee80211_p2p_client_power.h"
#include "ieee80211_p2p_go_schedule.h"
#include "ieee80211_p2p_client_priv.h"
#include "ieee80211_p2p_prot_client.h"
#include "ieee80211_p2p_prot_api.h"

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#if UMAC_SUPPORT_P2P

struct ieee80211_rx_status;

static int  ieee80211_p2p_client_input_filter_mgmt(wlan_node_t ni, wbuf_t wbuf,int subtype,struct ieee80211_rx_status *rs);
static int  ieee80211_p2p_client_output_filter(wbuf_t wbuf);
static int   ieee80211_p2p_client_parse_p2p_ie(wlan_p2p_client_t p2p_client_handle, struct p2p_parsed_ie *pie);
static void ieee80211_p2p_client_deliver_event(wlan_p2p_client_t p2p_client_handle, ieee80211_p2p_client_event *evt);
static void ieee80211_p2p_client_update_go_schedule(wlan_p2p_client_t p2p_client_handle);

#define IEEE80211_MAX_P2P_CLIENT_EVENT_HANDLERS 4
#define P2P_CLIENT_LOCK_INIT(handle) spin_lock_init(&handle->p2p_client_lock);
#define P2P_CLIENT_LOCK_DELETE(handle)  spin_lock_destroy(&handle->p2p_client_lock)
#define P2P_CLIENT_LOCK(handle)    spin_lock(&handle->p2p_client_lock)
#define P2P_CLIENT_UNLOCK(handle)  spin_unlock(&handle->p2p_client_lock)

/* the following function should be in os dep layer (asf ? adf ?) */
#define DIFF_NUM(a,b) ((a)>(b) ? ((a)-(b)) :((b)-(a)))
#define MAX_TSF_OFFSET_DRIFT  5*1000 /* in micro seconds */
#define GO_SCHEDULE_UPDATE_TIME  30*60*1000 /* 30 minutes */

#if TEST_VAP_PAUSE
static void ieee80211_vap_test_pause_beacon(wlan_p2p_client_t p2p_client_handle);
static OS_TIMER_FUNC(test_vap_pause_timer_handler);
#endif
static OS_TIMER_FUNC(ieee80211_p2p_client_schedule_update_timer_handler);


/* Get the beacon interval in microseconds */
static inline u_int32_t get_beacon_interval(wlan_if_t vap)
{
    /* The beacon interval is in terms of Time Units */
    return(IEEE80211_TU_TO_USEC(vap->iv_bss->ni_intval));
}

/*
 * VAP state event handler.
 */
static void ieee80211_p2p_client_vap_event_handler (ieee80211_vap_t vap, ieee80211_vap_event *event, void *arg)
{
    wlan_p2p_client_t p2p_client_handle = (wlan_p2p_client_t) arg;

    ieee80211_p2p_client_power_vap_event_handler(p2p_client_handle->p2p_client_power,event);

    switch(event->type)
    {
    case IEEE80211_VAP_FULL_SLEEP:
    case IEEE80211_VAP_DOWN:
        /* vap is down reset all the NOA related data and  stop the go schedule */
         p2p_client_handle->noa_initialized=0;
         OS_MEMZERO(&p2p_client_handle->noa_ie, sizeof(p2p_client_handle->noa_ie));
         ieee80211_p2p_client_update_go_schedule(p2p_client_handle);

         break;
    default:
         break;

    }

}

static void ieee80211_p2p_client_mlme_event_handler (ieee80211_vap_t vap, ieee80211_mlme_event *event, void *arg)
 {
    wlan_p2p_client_t p2p_client_handle = (wlan_p2p_client_t) arg;

    if (event->type == IEEE80211_MLME_EVENT_BEACON_MISS ) {
        /*
         * if we did not receive beacons for some time, then
         * remove NOA , oppPS , our clock might be out of sync.
         * NOA will restart when we receive a beacon  .
         */

        if (p2p_client_handle->noa_initialized) {

            /**
             * if noa schedule has been initialized, then remove them and
             * update go schedule.
             */
            p2p_client_handle->noa_ie.ctwindow = 0;
            p2p_client_handle->noa_ie.num_descriptors = 0;
            p2p_client_handle->noa_initialized = 0;

            IEEE80211_DPRINTF(p2p_client_handle->vap,IEEE80211_MSG_STATE, "%s:Beacon miss stop NOA until we receive a beacon \n", __func__);
            ieee80211_p2p_client_update_go_schedule(p2p_client_handle);
        }
    }

}

/**
 * creates an instance of p2p client object .
 * @param p2p_handle   : handle to the p2p object .
 *                       the information from p2p_handle is used for all the device specific
 *                       information like device id, device category ..etc.
 *
 * @return  on success return an opaque handle to newly created p2p client object.
 *          on failure returns NULL.
 */
wlan_p2p_client_t
wlan_p2p_client_create(wlan_p2p_t p2p_handle, u_int8_t *bssid)
{
    osdev_t oshandle;
    wlan_dev_t devhandle;
    wlan_p2p_client_t p2p_client_handle;
    wlan_if_t vap;
    static char maverick_ie[6];

    /*
     * Maverick IE needs to be present for AirDrop and it is not being
     * set by the OS on P2P client interfaces, so set it here
     */
    maverick_ie[0] = 0xdd;
    maverick_ie[1] = 0x04;
    maverick_ie[2] = 0x00;
    maverick_ie[3] = 0x17;
    maverick_ie[4] = 0xf2;
    maverick_ie[5] = 0x05;

    /*
     * treat p2p_handle as devhandle until the p2p device object is
     * implemented.
     */
    devhandle = (wlan_dev_t) p2p_handle;
    oshandle = ieee80211com_get_oshandle(devhandle);

    /* Check that P2P is supported */
    if (wlan_get_device_param(devhandle, IEEE80211_DEVICE_P2P) == 0) {
        ieee80211com_note(devhandle, IEEE80211_MSG_P2P_PROT,
            "%s: Error: P2P unsupported.\n", __func__);
        return NULL;
    }

    p2p_client_handle = (wlan_p2p_client_t) OS_MALLOC(oshandle, sizeof(struct ieee80211_p2p_client), 0);

    if (p2p_client_handle == NULL) {
        ieee80211com_note(devhandle, IEEE80211_MSG_P2P_PROT,
             "%s: Failed to create p2p_client object \n", __func__);
        return NULL;
    }
    OS_MEMZERO(p2p_client_handle, sizeof(struct ieee80211_p2p_client));

    /*
     * create  station VAP associated with p2p client.
     */
    vap = wlan_vap_create(devhandle,
                          IEEE80211_M_STA,
                          DEF_VAP_SCAN_PRI_MAP_OPMODE_STA_P2P_CLIENT,
                          IEEE80211_P2PCLI_VAP | (bssid ? 0 : IEEE80211_CLONE_BSSID),
                          bssid,
                          NULL,
                          NULL);
    if (vap == NULL) {
        OS_FREE(p2p_client_handle);
        ieee80211com_note(devhandle, IEEE80211_MSG_P2P_PROT,
            "%s: Failed to create vap for p2p_client object \n", __func__);
        return NULL;
    }
    p2p_client_handle->vap = vap;
    p2p_client_handle->devhandle = devhandle;

    /*
     * setup input/output management filter functions..
     */
    (void) ieee80211vap_set_input_mgmt_filter(vap,ieee80211_p2p_client_input_filter_mgmt);
    (void) ieee80211vap_set_output_mgmt_filter_func(vap,ieee80211_p2p_client_output_filter);
    ieee80211vap_set_private_data(vap,(void *) p2p_client_handle);

    /* setup rates for p2p vap */
    ieee80211_p2p_setup_rates(vap);

    /* set the managemet frame rate to 6M including beacon rate */
    wlan_set_param(vap,IEEE80211_MGMT_RATE, IEEE80211_P2P_MIN_RATE);

    /*
     * create p2p go scheduler module.
     * this module keeps track of when go is present and when go is absent based
     * NOA schedule received from GO.
     */
    p2p_client_handle->go_schedule = ieee80211_p2p_go_schedule_create(oshandle,vap);

    if (p2p_client_handle->go_schedule == NULL)
    {
        wlan_vap_delete(p2p_client_handle->vap);
        OS_FREE(p2p_client_handle);
        ieee80211com_note(devhandle, IEEE80211_MSG_P2P_PROT,
            "%s: Failed to create go schedule for p2p_client object \n", __func__);
        return NULL;
    }

    p2p_client_handle->p2p_client_power = ieee80211_p2p_client_power_create(oshandle, p2p_client_handle, vap, p2p_client_handle->go_schedule);
    if (p2p_client_handle->p2p_client_power == NULL)
    {
        ieee80211_p2p_go_schedule_delete(p2p_client_handle->go_schedule);
        wlan_vap_delete(p2p_client_handle->vap);
        OS_FREE(p2p_client_handle);
        ieee80211com_note(devhandle, IEEE80211_MSG_P2P_PROT,
            "%s: Failed to create client power for p2p_client object \n", __func__);
        return NULL;
    }
    if (ieee80211_vap_register_event_handler(vap,ieee80211_p2p_client_vap_event_handler,(void *)p2p_client_handle ) != EOK) {
        ieee80211_p2p_go_schedule_delete(p2p_client_handle->go_schedule);
        wlan_vap_delete(p2p_client_handle->vap);
        OS_FREE(p2p_client_handle);
        ieee80211com_note(devhandle, IEEE80211_MSG_P2P_PROT,
            "%s: Failed to register vap event handler \n", __func__);
        return NULL;
    }
    P2P_CLIENT_LOCK_INIT(p2p_client_handle);

    if (ieee80211_mlme_register_event_handler(vap,ieee80211_p2p_client_mlme_event_handler, p2p_client_handle) != EOK) {
        ieee80211_p2p_go_schedule_delete(p2p_client_handle->go_schedule);
        wlan_vap_delete(p2p_client_handle->vap);
        OS_FREE(p2p_client_handle);
        ieee80211com_note(devhandle, IEEE80211_MSG_P2P_PROT,
            "%s: Failed to register with mlme module \n", __func__);
        return NULL;
    }
#if TEST_VAP_PAUSE
    OS_INIT_TIMER(oshandle, &p2p_client_handle->pause_timer, test_vap_pause_timer_handler, p2p_client_handle , QDF_TIMER_TYPE_WAKE_APPS);
#endif
    OS_INIT_TIMER(oshandle, &p2p_client_handle->schedule_update_timer, ieee80211_p2p_client_schedule_update_timer_handler, p2p_client_handle , QDF_TIMER_TYPE_WAKE_APPS);

    p2p_client_handle->p2p_client_app_ie = wlan_mlme_app_ie_create(vap);
    if (p2p_client_handle->p2p_client_app_ie) {
        wlan_mlme_app_ie_set(p2p_client_handle->p2p_client_app_ie, IEEE80211_FRAME_TYPE_ASSOCREQ, (u_int8_t *)&maverick_ie, sizeof(maverick_ie));
    }

    return p2p_client_handle;
}

/**
 * delete p2p client object.
 * @param p2p_client_handle : handle to the p2p object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
int wlan_p2p_client_delete(wlan_p2p_client_t p2p_client_handle)
{
    if (wlan_p2p_prot_client_is_attach(p2p_client_handle)) {
        IEEE80211_DPRINTF(p2p_client_handle->vap, IEEE80211_MSG_ANY, "%s: Warning: Did not attach Protocol module.\n", __func__);
        wlan_p2p_prot_client_detach(p2p_client_handle);
    }

    ieee80211_vap_unregister_event_handler(p2p_client_handle->vap,ieee80211_p2p_client_vap_event_handler,(void *)p2p_client_handle );
    ieee80211_mlme_unregister_event_handler(p2p_client_handle->vap,ieee80211_p2p_client_mlme_event_handler, p2p_client_handle);
    if (p2p_client_handle->p2p_client_power != NULL)
    {
        ieee80211_p2p_client_power_delete(p2p_client_handle->p2p_client_power);
    }
    ieee80211_p2p_go_schedule_delete(p2p_client_handle->go_schedule);

#if TEST_VAP_PAUSE
    OS_CANCEL_TIMER(&p2p_client_handle->pause_timer);
    OS_FREE_TIMER(&p2p_client_handle->pause_timer);
#endif
    OS_CANCEL_TIMER(&p2p_client_handle->schedule_update_timer);
    OS_FREE_TIMER(&p2p_client_handle->schedule_update_timer);
    P2P_CLIENT_LOCK_DELETE(p2p_client_handle);
    if (p2p_client_handle->p2p_client_app_ie) {
        wlan_mlme_remove_ie_list(p2p_client_handle->p2p_client_app_ie);
    }
    wlan_vap_delete(p2p_client_handle->vap);
    OS_FREE(p2p_client_handle);
    return 0;
}

/**
 * return Station vaphandle associated with underlying p2p client object.
 * @param p2p_client_handle         : handle to the p2p object .
 * @return vaphandle.
 */
wlan_if_t  wlan_p2p_client_get_vap_handle(wlan_p2p_client_t p2p_client_handle)
{
    return p2p_client_handle->vap;
}

/**
 * register p2p event handler with p2p client object.
 * @param p2p_handle        : handle to the p2p client object .
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
wlan_p2p_client_register_event_handlers(wlan_p2p_client_t p2p_client_handle,
                                 void *event_arg, wlan_p2p_event_handler handler)
{
    P2P_CLIENT_LOCK(p2p_client_handle);
    if (handler && p2p_client_handle->p2p_event_handler) {
        if (handler != p2p_client_handle->p2p_event_handler) {
            P2P_CLIENT_UNLOCK(p2p_client_handle);
            return -EINVAL;
        }
    }
    /* needs synchronization */
    p2p_client_handle->p2p_event_handler = handler;
    p2p_client_handle->p2p_event_arg = event_arg;
    p2p_client_handle->p2p_req_id = 0; /* don't need this on client? device only */
    P2P_CLIENT_UNLOCK(p2p_client_handle);
    return 0;
}

static void
ieee80211_p2p_client_deliver_p2p_event(wlan_p2p_client_t p2p_client_handle, wlan_p2p_event *event)
{
    void *event_arg;
    wlan_p2p_event_handler handler;
    wlan_if_t vap = p2p_client_handle->vap;

    P2P_CLIENT_LOCK(p2p_client_handle);
    handler = p2p_client_handle->p2p_event_handler;
    event_arg = p2p_client_handle->p2p_event_arg;
    P2P_CLIENT_UNLOCK(p2p_client_handle);

#if ATH_DEBUG == 0
    /* Fix the compile error when ATH_DEBUG = 0 */
    vap = vap;
#endif

    if (handler) {
        event->req_id = p2p_client_handle->p2p_req_id;
        handler(event_arg, event);
    }

    switch(event->type) {
    case WLAN_P2PDEV_RX_FRAME:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:P2P_EVENT: RX_FRAME SA[" MACSTR "]: frame type[%02x]\n",
                          __func__, MAC2STR(event->u.rx_frame.src_addr), event->u.rx_frame.frame_type);
        break;
    default:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:P2P_EVENT: type %x \n",__func__, event->type);
        break;
    }
}



/**
 * input filter management function for client.
 */
static int
ieee80211_p2p_client_input_filter_mgmt(wlan_node_t ni, wbuf_t wbuf,
                                       int subtype, struct ieee80211_rx_status *rs)
{
    int filter=0;
    u_int8_t *bssid, *ie_data, *frm;
    u_int16_t frame_len,ie_len;
    struct ieee80211_frame *wh;
    wlan_p2p_client_t p2p_client_handle;
    osdev_t oshandle;
    wlan_if_t vap = ieee80211_node_get_vap(ni);
    struct p2p_parsed_ie pie;
    wlan_p2p_event p2p_event;
    wlan_chan_t channel;
    u_int32_t freq;
    u_int32_t filter_by_ssid;

    p2p_client_handle = (wlan_p2p_client_t)ieee80211vap_get_private_data(vap);
    filter_by_ssid = p2p_client_handle->p2p_client_event_filter;
    filter_by_ssid = filter_by_ssid & (IEEE80211_P2P_CLIENT_EVENT_BEACON_BY_SSID |
                                        IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP_BY_SSID);

    if (!ieee80211_vap_active_is_set(vap)) {
        /* Normally, we will ignore incoming management frame when our VAP is inactive.
         * The exception is when IEEE80211_P2P_CLIENT_EVENT_BEACON_BY_SSID and
         * IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP_BY_SSID are filtered. For these filters,
         * the frames are looked at even when VAP is inactive. The frames are filtered
         * by SSID instead of BSSID.
         */

        if (filter_by_ssid == 0) {
            /* No need to look any further since VAP is inactive */
            return 0;
        }
        else if ((subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP) &&
                 ((filter_by_ssid & IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP_BY_SSID) != 0))
        {
            /* Else continue to check */
        }
        else if ((subtype == IEEE80211_FC0_SUBTYPE_BEACON) &&
                 ((filter_by_ssid & IEEE80211_P2P_CLIENT_EVENT_BEACON_BY_SSID) != 0))
        {
            /* Else continue to check */
        }
        else {
            /* no need to check further */
            return 0;
        }
    }

    frm = wbuf_header(wbuf);
    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    
    if((OS_MEMCMP(wh->i_addr1, "\xff\xff\xff\xff\xff\xff", IEEE80211_ADDR_LEN) != 0) &&
       (OS_MEMCMP(vap->iv_myaddr,wh->i_addr1,IEEE80211_ADDR_LEN) != 0))
    {
        return 0;
    }

    oshandle = ieee80211com_get_oshandle(p2p_client_handle->devhandle);

    frame_len =  wbuf_get_pktlen(wbuf);
    channel = wlan_node_get_chan(ni);
    freq = wlan_channel_frequency(channel);
    ie_data = ieee80211_mgmt_iedata(wbuf,subtype);
    if (ie_data == NULL && subtype != IEEE80211_FC0_SUBTYPE_ACTION) {
        /* Error in finding the IE data. do nothing */
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s SA[%s]: Freq[%4u], Frame MGMT "
                          "subtype[%02x] seq[0x%04x], No IE - DISCARDING !!\n",
                          __func__, ether_sprintf(wh->i_addr2), rs->rs_freq,
						  subtype, le16toh(*(u_int16_t *)wh->i_seq));
        return filter;
    }
    ie_len = frame_len - (ie_data - frm);

    /*
     * filter out any assoc request and probe request frames without P2P IE.
     */
    switch(subtype) {
    case  IEEE80211_FC0_SUBTYPE_ASSOC_RESP:
        /* reject if no P2P IE is present */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s ASSOC_RESP DA[" MACSTR "] SA[" MACSTR "] "
                          "seq[%04x] freq[%4u]\n", __func__, MAC2STR(wh->i_addr1),
                          MAC2STR(wh->i_addr2), le16toh(*(u_int16_t *)wh->i_seq), freq);
        break;
    case  IEEE80211_FC0_SUBTYPE_PROBE_RESP:
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s PROBE_RESP DA[" MACSTR "] SA[" MACSTR "] "
                              "seq[%04x] freq[%4u]\n", __func__, MAC2STR(wh->i_addr1),
                              MAC2STR(wh->i_addr2), le16toh(*(u_int16_t *)wh->i_seq), freq);
			// Fall through
    case  IEEE80211_FC0_SUBTYPE_BEACON:
    {
        bool    bssid_matched = false;
        bool    ssid_matched = false;

        bssid = wh->i_addr2;
        bssid_matched = IEEE80211_ADDR_EQ(bssid, ieee80211_node_get_bssid(ni));
        if ((filter_by_ssid != 0) || bssid_matched)
        {
            ieee80211_vap_tsf_offset tsf_offset_info;

            OS_MEMZERO(&pie, sizeof(pie));
            if (ieee80211_p2p_parse_ies(oshandle, vap, ie_data, ie_len, &pie) == 0) {
                if(ieee80211_p2p_client_parse_p2p_ie(p2p_client_handle, &pie) != EOK ) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s subtype[%02x] DA[" MACSTR "] SA[" MACSTR "] "
                                      "seq[%04x] freq[%4u], failed IE parse\n", __func__, subtype,
                                      MAC2STR(wh->i_addr1), MAC2STR(wh->i_addr2),
                                      le16toh(*(u_int16_t *)wh->i_seq), freq);
                }

                if (pie.ssid != NULL) {
                    ssid_matched = (pie.ssid[1] == p2p_client_handle->ssid.len) &&
                        (OS_MEMCMP(&(pie.ssid[2]), p2p_client_handle->ssid.ssid, p2p_client_handle->ssid.len) == 0);
                }

                ieee80211_p2p_parse_free(&pie);
            }

            if (bssid_matched)
            {
                if ((p2p_client_handle->p2p_client_event_filter & IEEE80211_P2P_CLIENT_EVENT_BEACON) &&
                    (subtype == IEEE80211_FC0_SUBTYPE_BEACON)) {
                    ieee80211_p2p_client_event evt;
                    evt.type = IEEE80211_P2P_CLIENT_EVENT_BEACON;
                    ieee80211_p2p_client_deliver_event(p2p_client_handle,&evt);
                }
                if ((p2p_client_handle->p2p_client_event_filter & IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP) &&
                    (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP)) {
                    ieee80211_p2p_client_event evt;
                    evt.type = IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP;
                    ieee80211_p2p_client_deliver_event(p2p_client_handle,&evt);
                }
#if TEST_VAP_PAUSE
                ieee80211_vap_test_pause_beacon(p2p_client_handle);
#endif
                if (p2p_client_handle->go_schedule_needs_update) {
                    ieee80211_p2p_client_update_go_schedule(p2p_client_handle);
                }
                /*
                 * check if the tsf of our GO has drifted from our HW tsf by MAX_TSF_OFFSET_DIFF .
                 * this can happen in the case of STA + p2p CLIENT combination
                 * both is co-channel, off-channel case.
                 */
                ieee80211_vap_get_tsf_offset(vap,&tsf_offset_info);
                if (((p2p_client_handle->tsf_offset_info.offset_negative == tsf_offset_info.offset_negative) &&
                     (DIFF_NUM(p2p_client_handle->tsf_offset_info.offset , tsf_offset_info.offset) > MAX_TSF_OFFSET_DRIFT)) ||
                    ((p2p_client_handle->tsf_offset_info.offset_negative != tsf_offset_info.offset_negative) &&
                     ((p2p_client_handle->tsf_offset_info.offset + tsf_offset_info.offset) > MAX_TSF_OFFSET_DRIFT)))
                {
                    /*
                     * our GOs clocks has drifted. ssave the new offset info
                     * and recompute the NOA.
                     */

                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: p2p_client_handle->tsf_offset_info.offset_negative = %d, tsf_offset_info.offset_negative = %d," 
                                      "p2p_client_handle->tsf_offset_info.offset = %d, tsf_offset_info.offset = %d \n",
                                      __func__, p2p_client_handle->tsf_offset_info.offset_negative, tsf_offset_info.offset_negative, p2p_client_handle->tsf_offset_info.offset, tsf_offset_info.offset);


                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s our GO TSF has drifted , reset NOA timers \n",__func__);
                    p2p_client_handle->tsf_offset_info = tsf_offset_info;
                    if (p2p_client_handle->noa_initialized) {
                        ieee80211_p2p_client_update_go_schedule(p2p_client_handle);
                    }
                }
            }

            if (ssid_matched && (filter_by_ssid != 0)) {
                if ((p2p_client_handle->p2p_client_event_filter & IEEE80211_P2P_CLIENT_EVENT_BEACON_BY_SSID) &&
                    (subtype == IEEE80211_FC0_SUBTYPE_BEACON))
                {
                    ieee80211_p2p_client_event evt;
                    evt.type = IEEE80211_P2P_CLIENT_EVENT_BEACON_BY_SSID;
                    evt.u.filter_by_ssid_info.in_group_formation = p2p_client_handle->in_group_formation;
                    evt.u.filter_by_ssid_info.wps_selected_registrar = p2p_client_handle->wps_selected_registrar;
                    IEEE80211_ADDR_COPY(evt.u.filter_by_ssid_info.ta, wh->i_addr2);
                    ieee80211_p2p_client_deliver_event(p2p_client_handle,&evt);
                }
                if ((p2p_client_handle->p2p_client_event_filter & IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP_BY_SSID) &&
                    (subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP))
                {
                    ieee80211_p2p_client_event evt;
                    evt.type = IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP_BY_SSID;
                    evt.u.filter_by_ssid_info.in_group_formation = p2p_client_handle->in_group_formation;
                    evt.u.filter_by_ssid_info.wps_selected_registrar = p2p_client_handle->wps_selected_registrar;
                    IEEE80211_ADDR_COPY(evt.u.filter_by_ssid_info.ta, wh->i_addr2);
                    ieee80211_p2p_client_deliver_event(p2p_client_handle,&evt);
                }
            }
        }
        break;
    }
    case IEEE80211_FC0_SUBTYPE_ACTION:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s ACTION DA[" MACSTR "] SA[" MACSTR "] "
                          "seq[%04x] freq[%4u]\n", __func__, MAC2STR(wh->i_addr1),
                          MAC2STR(wh->i_addr2), le16toh(*(u_int16_t *)wh->i_seq), freq);
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
        ieee80211_p2p_client_deliver_p2p_event(p2p_client_handle, &p2p_event);
        filter = 0; /* is this correct? */
        break;
    default:
        break;
    }
    return filter;
}

static int ieee80211_p2p_client_output_filter( wbuf_t wbuf)
{
    u_int8_t *efrm;
    struct ieee80211_frame *wh;
    int type,subtype;
    struct ieee80211_node *ni = NULL;
    wlan_p2p_client_t p2p_client_handle = NULL;

    wh = (struct ieee80211_frame *) wbuf_header(wbuf);
    type = wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
    subtype = wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

    efrm = wbuf_header(wbuf) + wbuf_get_pktlen(wbuf);
    ni = wlan_wbuf_get_peer_node(wbuf);
    if (!ni) {
        return -EINVAL;
    }
    p2p_client_handle = (wlan_p2p_client_t)
        ieee80211vap_get_private_data(ni->ni_vap);

    if (type != IEEE80211_FC0_TYPE_MGT) {
        if (wbuf_is_pwrsaveframe(wbuf) && (NULL != p2p_client_handle))  {
            return
                ieee80211_p2p_client_power_check_and_filter_null(
                    p2p_client_handle->p2p_client_power, wbuf);
        }
    }

    IEEE80211_DPRINTF(p2p_client_handle->vap, IEEE80211_MSG_ANY,
                      "%s type[%02x] subtype[%02x] DA[" MACSTR
                      "] SA[" MACSTR "] seq[%04x]\n",
                      __func__, type, subtype,
                      MAC2STR(wh->i_addr1), MAC2STR(wh->i_addr2),
                      le16toh(*(u_int16_t *)wh->i_seq));
    return 0;
}

#define MAX_TSF_32_DIFF  ( ((u_int32_t)1<<31) -1)

static void ieee80211_p2p_client_update_noa_descriptor_offsets(wlan_p2p_client_t p2p_client_handle,
                             ieee80211_p2p_go_schedule_req *go_schedules,
                             u_int32_t start_index, u_int32_t num_descriptors)
{
   ieee80211_vap_tsf_offset tsf_offset_info;
   u_int64_t                curr_tsf64;
   u_int32_t curr_tsf32,i, diff_tsf32;
   struct ieee80211com *ic = p2p_client_handle->vap->iv_ic;

   curr_tsf64 = ic->ic_get_TSF64(ic);

   /* tsf offset from our HW tsf */
   ieee80211_vap_get_tsf_offset(p2p_client_handle->vap,&tsf_offset_info);

   IEEE80211_DPRINTF(p2p_client_handle->vap, IEEE80211_MSG_POWER, "%s: %d 0x%llx \n",
            __func__,tsf_offset_info.offset_negative, tsf_offset_info.offset);

   if (!tsf_offset_info.offset) {
       /* no offset, just return */
       return;
   }
   /*
    * adjust tsf to the clock of the GO.
    * i.e. convert time to tsf clock of GO.
    */
   if (tsf_offset_info.offset_negative) {
       curr_tsf64 -= tsf_offset_info.offset;
   } else {
       curr_tsf64 += tsf_offset_info.offset;
   }
   curr_tsf32 = (u_int32_t) curr_tsf64;

   /* adjust start time with tsf offset  */
   for (i=start_index;i<num_descriptors;++i) {
       /*
        * compute the current tsf32 and the NOA start time.
        * if the differennce is greater than MAX_TSF_32_DIFF
        * then the start time is in the past else the start
        * time is in the future.
        */
       diff_tsf32 = (go_schedules[start_index].noa_descriptor.start_time - curr_tsf32);

       if (diff_tsf32 > MAX_TSF_32_DIFF) {
           curr_tsf64 -= diff_tsf32;
       } else {
           curr_tsf64 += diff_tsf32;
       }

       /*  convert back to the HW tsf */
       if (tsf_offset_info.offset_negative) {
           curr_tsf64 += tsf_offset_info.offset;
       } else {
           curr_tsf64 -= tsf_offset_info.offset;
       }
       go_schedules[start_index].noa_descriptor.start_time = (u_int32_t)curr_tsf64;
   }

}

/*
* when ever there is a change in  noa (or) ct window this function
* will be called to update the go schedule with new updated info.
* sets up 3 kinds of schedule requests with GO.
* 1) priority 0 for one shot absense (from noa descriptors from beacon/probe response).
* 2) priority 2 for periodic absense (from noa descriptors from beacon/probe response).
* 2) priority 1 for periodic presense from tbtt to ctwindow .
*/

static void ieee80211_p2p_client_update_go_schedule(wlan_p2p_client_t p2p_client_handle)
{
    ieee80211_p2p_go_schedule_req go_schedules[IEEE80211_MAX_NOA_DESCRIPTORS];
    struct ieee80211_p2p_sub_element_noa noa_ie;
    u_int32_t nexttbtt;
    u_int32_t cur_index=0, i ,start_adj_index;

    p2p_client_handle->go_schedule_needs_update=0;

    OS_SET_TIMER(&p2p_client_handle->schedule_update_timer,GO_SCHEDULE_UPDATE_TIME);
    noa_ie =  p2p_client_handle->noa_ie;
    IEEE80211_DPRINTF(p2p_client_handle->vap, IEEE80211_MSG_POWER, "%s: noa update index %d oppps %d ctwindow %d \n",
            __func__,noa_ie.index, noa_ie.oppPS, noa_ie.ctwindow);
    /* setup a priority 1 presense schedule for CT WINDOW if ctwindow exisits */
    if (noa_ie.ctwindow) {
       nexttbtt = ieee80211_p2p_client_get_next_tbtt_tsf_32(p2p_client_handle->vap);
       go_schedules[cur_index].type = IEEE80211_P2P_GO_PRESENT_SCHEDULE;
       go_schedules[cur_index].pri = 1;
       go_schedules[cur_index].noa_descriptor.start_time = nexttbtt;
       go_schedules[cur_index].noa_descriptor.type_count = 255;/* continuous */
       go_schedules[cur_index].noa_descriptor.interval = get_beacon_interval(p2p_client_handle->vap);
       go_schedules[cur_index].noa_descriptor.duration = IEEE80211_TU_TO_USEC(noa_ie.ctwindow);
       ++cur_index;
    }

    start_adj_index = cur_index;
    /* setup absent schedules based on the NOA descriptors from GO */
    for (i=0;i<noa_ie.num_descriptors;++i) {
         go_schedules[cur_index].type = IEEE80211_P2P_GO_ABSENT_SCHEDULE;
         go_schedules[cur_index].noa_descriptor = noa_ie.noa_descriptors[i];
         /*
          * according to the spec one shot (type_count == 1) have a highest priority
          * compared presense period (tbtt to ctwindow) and periodic NOA.
          */
          if (go_schedules[cur_index].noa_descriptor.type_count == 1) {
             go_schedules[cur_index].pri = 0; /* one shot NOA */
          } else {
             go_schedules[cur_index].pri = 2; /* periodic NOA if count == 255 then continuous  */
          }
          ++cur_index;
        IEEE80211_DPRINTF(p2p_client_handle->vap, IEEE80211_MSG_POWER, "%s: noa update duration %d type_count %d start_time %x \n",
                          __func__,noa_ie.noa_descriptors[i].duration, noa_ie.noa_descriptors[i].type_count,
                          noa_ie.noa_descriptors[i].start_time);
    }

    ieee80211_p2p_client_update_noa_descriptor_offsets(p2p_client_handle,go_schedules,
              start_adj_index, noa_ie.num_descriptors);

    ieee80211_p2p_go_schedule_setup(p2p_client_handle->go_schedule,cur_index, go_schedules);
}

/*
 * parse the p2p ie and if noa sub element is found in the P2P IE,
 * pass it on to the registered handlers.
 */
static int
ieee80211_p2p_client_parse_p2p_ie(wlan_p2p_client_t p2p_client_handle, struct p2p_parsed_ie *pie)
{
    const u_int8_t *noa = pie->noa;
    size_t noa_num_descriptors = pie->noa_num_descriptors;
    struct ieee80211_p2p_sub_element_noa noa_ie;

    /* IE is valid  parse the different sub elements */
    if (noa) {
        if (ieee80211_p2p_parse_noa(noa, (u_int8_t)noa_num_descriptors, &noa_ie) == EOK) {
            if (!p2p_client_handle->noa_initialized || p2p_client_handle->noa_ie.index != noa_ie.index) {
                /*
                 * new noa update from GO. save and propagate to NOA schedule to
                 * GO scheduler
                 */
                if (p2p_client_handle->noa_ie.oppPS != noa_ie.oppPS &&
                    p2p_client_handle->p2p_client_event_filter & IEEE80211_P2P_CLIENT_EVENT_UPDATE_OPPPS ) {
                    ieee80211_p2p_client_event evt;
                    evt.type = IEEE80211_P2P_CLIENT_EVENT_UPDATE_OPPPS;
                    evt.u.oppPS = noa_ie.oppPS;
                    IEEE80211_DPRINTF(p2p_client_handle->vap,IEEE80211_MSG_STATE, "%s: oppps %s \n", __func__, evt.u.oppPS ? "ON":"OFF");
                    ieee80211_p2p_client_deliver_event(p2p_client_handle,&evt);
                }
                if (p2p_client_handle->noa_ie.ctwindow != noa_ie.ctwindow &&
                    p2p_client_handle->p2p_client_event_filter & IEEE80211_P2P_CLIENT_EVENT_UPDATE_CTWINDOW ) {
                    ieee80211_p2p_client_event evt;
                    evt.type = IEEE80211_P2P_CLIENT_EVENT_UPDATE_CTWINDOW;
                    evt.u.ctwindow = noa_ie.ctwindow;
                    IEEE80211_DPRINTF(p2p_client_handle->vap,IEEE80211_MSG_STATE, "%s: ctwindow %d \n", __func__, noa_ie.ctwindow);
                    ieee80211_p2p_client_deliver_event(p2p_client_handle,&evt);
                }
                p2p_client_handle->noa_ie = noa_ie;
                p2p_client_handle->noa_initialized=1;
                ieee80211_p2p_client_update_go_schedule(p2p_client_handle);
            }
        }
    } else {
        /**
         * no NOA attribute , so GO is avialable all the time.
         * if noa schedule has never been initialized (or) if there was a ctwindow
         * (or) if there was a noa before the remove them and update go schedule.
         */

        if (p2p_client_handle->noa_ie.oppPS) {
            p2p_client_handle->noa_ie.oppPS=0; /* no oppPS if noa is absent from GO */
            if (p2p_client_handle->p2p_client_event_filter & IEEE80211_P2P_CLIENT_EVENT_UPDATE_OPPPS ) {
                ieee80211_p2p_client_event evt;
                evt.type = IEEE80211_P2P_CLIENT_EVENT_UPDATE_OPPPS;
                evt.u.oppPS = 0;
                IEEE80211_DPRINTF(p2p_client_handle->vap,IEEE80211_MSG_STATE, "%s: oppps %s \n", __func__, evt.u.oppPS ? "ON":"OFF");
                ieee80211_p2p_client_deliver_event(p2p_client_handle,&evt);
            }
        }

        /**
         * if noa schedule has been initialized, then remove them and
         * update go schedule.
         */

        if (p2p_client_handle->noa_initialized) {

            p2p_client_handle->noa_ie.ctwindow = 0;
            p2p_client_handle->noa_ie.num_descriptors = 0;
            p2p_client_handle->noa_initialized = 0;

            IEEE80211_DPRINTF(p2p_client_handle->vap,IEEE80211_MSG_STATE, "%s: No NOA IE.\n", __func__);
            ieee80211_p2p_client_update_go_schedule(p2p_client_handle);
        }
    }

    if (p2p_client_handle->p2p_client_event_filter &
        (IEEE80211_P2P_CLIENT_EVENT_PROBE_RESP | IEEE80211_P2P_CLIENT_EVENT_BEACON)) {

        ieee80211_p2p_client_ie_bit_state   in_group_formation, wps_selected_registrar;

        /* check that state of Group Formation Bit in the P2P Capability attribute */
        if (pie->capability != NULL) {
            /* Parse the Capability attribute */
            u_int8_t    dev_capab;
            u_int8_t    group_capab;
            int         retval;

            retval = ieee80211_p2p_parse_capability(pie->capability, &dev_capab, &group_capab);
            ASSERT(retval == EOK);
            if ((group_capab & P2P_GROUP_CAPAB_GROUP_FORMATION) != 0) {
                in_group_formation = IEEE80211_P2P_CLIENT_IE_BIT_SET;
            }
            else {
                in_group_formation = IEEE80211_P2P_CLIENT_IE_BIT_CLR;
            }
        }
        else {
            in_group_formation = IEEE80211_P2P_CLIENT_IE_BIT_NOT_PRESENT;
        }
        p2p_client_handle->in_group_formation = in_group_formation;

        /* Update the state of Selected Registrar WPS attribute in the WPS IE */
        if (pie->wps_selected_registrar_present) {
            if (pie->wps_selected_registrar == 1) {
                wps_selected_registrar = IEEE80211_P2P_CLIENT_IE_BIT_SET;
            }
            else {
                ASSERT(pie->wps_selected_registrar == 0);
                wps_selected_registrar = IEEE80211_P2P_CLIENT_IE_BIT_CLR;
            }
        }
        else {
            wps_selected_registrar = IEEE80211_P2P_CLIENT_IE_BIT_NOT_PRESENT;
        }
        p2p_client_handle->wps_selected_registrar = wps_selected_registrar;
    }

    return EOK;
}

/*
 * deliver event to registered handlers.
 */
static void ieee80211_p2p_client_deliver_event(wlan_p2p_client_t p2p_client_handle, ieee80211_p2p_client_event *evt)
{
    int i;
    P2P_CLIENT_LOCK(p2p_client_handle);
    for (i=0;i<IEEE80211_MAX_P2P_CLIENT_EVENT_HANDLERS; ++i) {
        ieee80211_p2p_client_event_handler evhandler = p2p_client_handle->p2p_client_event_handlers[i];
        void *arg = p2p_client_handle->p2p_client_event_handler_args[i];
        if( p2p_client_handle->p2p_client_event_filters[i] & evt->type ) {
           P2P_CLIENT_UNLOCK(p2p_client_handle);
           (* evhandler)(p2p_client_handle, evt, arg);
           P2P_CLIENT_LOCK(p2p_client_handle);
        }
    }
    P2P_CLIENT_UNLOCK(p2p_client_handle);

}

/*
 * unregister event handler.
 */
int ieee80211_p2p_client_unregister_event_handler(wlan_p2p_client_t p2p_client_handle,ieee80211_p2p_client_event_handler evhandler, void *arg)
{
    int i,j;
    if (!p2p_client_handle) return EINVAL; /* can happen if called from an ap vap */
    P2P_CLIENT_LOCK(p2p_client_handle);
    for (i=0;i<IEEE80211_MAX_P2P_CLIENT_EVENT_HANDLERS; ++i) {
        if ( p2p_client_handle->p2p_client_event_handlers[i] == evhandler && p2p_client_handle->p2p_client_event_handler_args[i] == arg) {
            p2p_client_handle->p2p_client_event_handlers[i] = NULL;
            p2p_client_handle->p2p_client_event_handler_args[i] = NULL;
            /* recompute event filters */
            p2p_client_handle->p2p_client_event_filters[i] = 0;
            for (j=0;j<IEEE80211_MAX_P2P_CLIENT_EVENT_HANDLERS; ++j) {
              if ( p2p_client_handle->p2p_client_event_handlers[j]) {
                p2p_client_handle->p2p_client_event_filter |= p2p_client_handle->p2p_client_event_filters[j] ;
              }
            }
            P2P_CLIENT_UNLOCK(p2p_client_handle);
            return 0;
        }
    }
    P2P_CLIENT_UNLOCK(p2p_client_handle);
    return EEXIST;
}

/*
 * register event handler.
 */
int ieee80211_p2p_client_register_event_handler(wlan_p2p_client_t p2p_client_handle,ieee80211_p2p_client_event_handler evhandler, void *arg, u_int32_t event_filter)
{
    int i;
    /* unregister if there exists one already */
    if (!p2p_client_handle) return EINVAL; /* can happen if called from an ap vap */
    ieee80211_p2p_client_unregister_event_handler(p2p_client_handle,evhandler,arg);

    P2P_CLIENT_LOCK(p2p_client_handle);
    for (i=0;i<IEEE80211_MAX_P2P_CLIENT_EVENT_HANDLERS; ++i) {
        if ( p2p_client_handle->p2p_client_event_handlers[i] == NULL) {
            p2p_client_handle->p2p_client_event_handlers[i] = evhandler;
            p2p_client_handle->p2p_client_event_handler_args[i] = arg;
            p2p_client_handle->p2p_client_event_filters[i] = event_filter;
            p2p_client_handle->p2p_client_event_filter |= event_filter;
            P2P_CLIENT_UNLOCK(p2p_client_handle);
            return EOK;
        }
    }
    P2P_CLIENT_UNLOCK(p2p_client_handle);
    return -ENOMEM;
}


#define MIN_TSF_TIMER_TIME  5000 /* 5 msec */

u_int32_t ieee80211_p2p_client_get_next_tbtt_tsf_32(wlan_if_t vap)
{
   struct ieee80211com *ic = vap->iv_ic;
   u_int64_t           curr_tsf64, nexttbtt_tsf64;
   u_int32_t           bintval; /* beacon interval in micro seconds */
   ieee80211_vap_tsf_offset tsf_offset_info;

   curr_tsf64 = ic->ic_get_TSF64(ic);
   /* calculate the next tbtt */

   /* tsf offset from our HW tsf */
   ieee80211_vap_get_tsf_offset(vap,&tsf_offset_info);

   /* adjust tsf to the clock of the GO */
   if (tsf_offset_info.offset_negative) {
       curr_tsf64 -= tsf_offset_info.offset;
   } else {
       curr_tsf64 += tsf_offset_info.offset;
   }

   bintval = get_beacon_interval(vap);

   nexttbtt_tsf64 =  curr_tsf64 + bintval;

   nexttbtt_tsf64  = nexttbtt_tsf64 - OS_MOD64_TBTT_OFFSET(nexttbtt_tsf64, bintval);

   if ((nexttbtt_tsf64 - curr_tsf64) < MIN_TSF_TIMER_TIME ) {  /* if the immediate next tbtt is too close go to next one */
       nexttbtt_tsf64 += bintval;
   }

   /* adjust tsf back to the clock of our HW */
   if (tsf_offset_info.offset_negative) {
       nexttbtt_tsf64 += tsf_offset_info.offset;
   } else {
       nexttbtt_tsf64 -= tsf_offset_info.offset;
   }

   return (u_int32_t) nexttbtt_tsf64;
}

int wlan_p2p_client_set_noa_schedule(
    wlan_p2p_client_t                   p2p_client_handle,
    u_int8_t                            num_noa_schedules,
    wlan_p2p_go_noa_req                 *request)
{
    int i;
    struct ieee80211_p2p_sub_element_noa *pnoa_ie;
    u_int32_t  nexttbtt;

    pnoa_ie =  &p2p_client_handle->noa_ie;
    pnoa_ie->num_descriptors = num_noa_schedules;
    nexttbtt = ieee80211_p2p_client_get_next_tbtt_tsf_32(p2p_client_handle->vap);

    for (i=0;i<num_noa_schedules; ++i) {
        pnoa_ie->noa_descriptors[i].type_count = request[i].num_iterations; /* count */
        pnoa_ie->noa_descriptors[i].interval = get_beacon_interval(p2p_client_handle->vap);
        /* Convert from msec to microseconds */
        pnoa_ie->noa_descriptors[i].duration = request[i].duration * 1000;
        pnoa_ie->noa_descriptors[i].start_time = nexttbtt + (request[i].offset_next_tbtt * 1000);
    }
    ieee80211_p2p_client_update_go_schedule(p2p_client_handle);

    return 0;
}

int wlan_p2p_client_set_param(wlan_p2p_client_t p2p_client_handle, wlan_p2p_client_param param, u_int32_t val)
{
    ieee80211_p2p_client_event  evt;
    int                         ret = 0;

    P2P_CLIENT_LOCK(p2p_client_handle);
    switch(param) {
    case WLAN_P2P_CLIENT_OPPPS:
        evt.type = IEEE80211_P2P_CLIENT_EVENT_UPDATE_OPPPS;
        evt.u.oppPS = val;
        IEEE80211_DPRINTF(p2p_client_handle->vap,IEEE80211_MSG_STATE, "%s: oppps %s \n", __func__, evt.u.oppPS ? "ON":"OFF");
        ieee80211_p2p_client_deliver_event(p2p_client_handle,&evt);
        break;

    case WLAN_P2P_CLIENT_CTWINDOW:
        evt.type = IEEE80211_P2P_CLIENT_EVENT_UPDATE_CTWINDOW;
        evt.u.ctwindow = val;
        p2p_client_handle->noa_ie.ctwindow = val;
        IEEE80211_DPRINTF(p2p_client_handle->vap,IEEE80211_MSG_STATE, "%s: ctwindow %d \n", __func__, val);
        ieee80211_p2p_client_deliver_event(p2p_client_handle,&evt);
	    ieee80211_p2p_client_update_go_schedule(p2p_client_handle);
        break;

    default:
        if (wlan_p2p_prot_client_is_attach(p2p_client_handle)) {
            ret = wlan_p2p_prot_client_set_param(p2p_client_handle, param, val);
        }
        else
            ret = EINVAL;
        break;
    }
    P2P_CLIENT_UNLOCK(p2p_client_handle);
    return ret;
}

int wlan_p2p_client_get_param(wlan_p2p_client_t p2p_client_handle, wlan_p2p_client_param param)
{
   int val=0;
    switch(param) {
    case WLAN_P2P_CLIENT_OPPPS:
        val=p2p_client_handle->noa_ie.oppPS;
        break;
    case WLAN_P2P_CLIENT_CTWINDOW:
        val=p2p_client_handle->noa_ie.ctwindow;
        break;
    default:
        val = 0;
        break;
    }
    return val;
}

/**
 * To set a parameter array in the P2P Client module.
 *  @param p2p_client_handle  : handle to the p2p Client object.
 *  @param param              : config paramaeter.
 *  @param byte_arr           : byte array .
 *  @param len                : length of byte array.
 *  @return 0  on success and -ve on failure.
 */
int
wlan_p2p_client_set_param_array(wlan_p2p_client_t p2p_client_handle, wlan_p2p_client_param param,
                                u_int8_t *byte_arr, u_int32_t len)
{
    int         ret = EOK;

    P2P_CLIENT_LOCK(p2p_client_handle);
    switch (param) {
    /* No non-protocol parameter array defined yet. */
    default:
        if (wlan_p2p_prot_client_is_attach(p2p_client_handle)) {
            ret = wlan_p2p_prot_client_set_param_array(p2p_client_handle, param, byte_arr, len);
        }
        else
            ret = EINVAL;
    }
    P2P_CLIENT_UNLOCK(p2p_client_handle);

    return ret;
}

/**
 * To set the desired SSIDList in the P2P client module.
 *  @param p2p_client_handle  : handle to the p2p client object.
 *  @param ssidlist           : new SSID.
 *  @return 0  on success and -ve on failure.
 */
int
wlan_p2p_client_set_desired_ssid(wlan_p2p_client_t p2p_client_handle,
                                 ieee80211_ssid *ssid)
{
    int     retval = EOK;

    retval = wlan_set_desired_ssidlist(p2p_client_handle->vap, 1, ssid);
    if (retval != EOK)
    {
        IEEE80211_DPRINTF(p2p_client_handle->vap, IEEE80211_MSG_STATE,
                          "%s: Error: wlan_set_desired_ssidlist ret=0x%x.\n",
                          __func__, retval);
        return -EINVAL;
    }
    else {
#if DBG
        char    ssid_str[IEEE80211_NWID_LEN + 1];

        ASSERT(ssid->len <= IEEE80211_NWID_LEN);
        OS_MEMCPY(ssid_str, ssid->ssid, ssid->len);
        ssid_str[ssid->len] = 0;

        IEEE80211_DPRINTF(p2p_client_handle->vap, IEEE80211_MSG_STATE,
                             "%s: ssid=%s\n",
                             __func__, ssid_str);
#endif

        /* Make a copy of this SSID */
        OS_MEMCPY(&p2p_client_handle->ssid, ssid, sizeof(ieee80211_ssid));
    }

    return retval;
}

int wlan_p2p_client_get_noa_info(wlan_p2p_client_t  p2p_client_handle, wlan_p2p_noa_info *noa_info)
{
    int i;
    struct ieee80211com *ic = p2p_client_handle->vap->iv_ic;

    noa_info->ctwindow = p2p_client_handle->noa_ie.ctwindow;
    noa_info->num_descriptors = p2p_client_handle->noa_ie.num_descriptors;
    noa_info->oppPS = p2p_client_handle->noa_ie.oppPS;
    noa_info->index = p2p_client_handle->noa_ie.index ;
    for (i=0;i<noa_info->num_descriptors;++i) {
        noa_info->noa_descriptors[i].type_count = p2p_client_handle->noa_ie.noa_descriptors[i].type_count;
        noa_info->noa_descriptors[i].duration = p2p_client_handle->noa_ie.noa_descriptors[i].duration;
        noa_info->noa_descriptors[i].start_time = p2p_client_handle->noa_ie.noa_descriptors[i].start_time;
        noa_info->noa_descriptors[i].interval = p2p_client_handle->noa_ie.noa_descriptors[i].interval;
    }
   noa_info->cur_tsf32 = ic->ic_get_TSF32(ic);

   return EOK;
}

static OS_TIMER_FUNC(ieee80211_p2p_client_schedule_update_timer_handler)
{
    wlan_p2p_client_t p2p_client_handle;

    OS_GET_TIMER_ARG(p2p_client_handle, wlan_p2p_client_t);
    IEEE80211_DPRINTF(p2p_client_handle->vap,IEEE80211_MSG_STATE, "%s: go_schedule needs update \n", __func__);
    p2p_client_handle->go_schedule_needs_update=1;
}

#if TEST_VAP_PAUSE

extern int ath_vap_pause_time; /* exposed from osdep via sysctl */

static OS_TIMER_FUNC(test_vap_pause_timer_handler)
{
    wlan_p2p_client_t p2p_client_handle;
    int pause_time = ath_vap_pause_time;

    /* pause time range is 20 to 80msec,assuming 100msec beacon interval */
    if (pause_time < 20 ) pause_time=20;
    if (pause_time > 80 ) pause_time=80;

    OS_GET_TIMER_ARG(p2p_client_handle, wlan_p2p_client_t);

    if (!p2p_client_handle->pause_state) {
        //p2p_client_handle->vap->iv_ic->ic_vap_pause_control(p2p_client_handle->vap->iv_ic,p2p_client_handle->vap,true);
        ieee80211_vap_force_pause(p2p_client_handle->vap, 0);
        p2p_client_handle->pause_state=1;
        OS_SET_TIMER(&p2p_client_handle->pause_timer, pause_time);
    } else {
        //p2p_client_handle->vap->iv_ic->ic_vap_pause_control(p2p_client_handle->vap->iv_ic,p2p_client_handle->vap,false);
        ieee80211_vap_force_unpause(p2p_client_handle->vap, 0);
        p2p_client_handle->pause_state=0;
    }
}

static void ieee80211_vap_test_pause_beacon(wlan_p2p_client_t p2p_client_handle)
{
    if (p2p_client_handle->pause_state) {
        //p2p_client_handle->vap->iv_ic->ic_vap_pause_control(p2p_client_handle->vap->iv_ic,p2p_client_handle->vap,false);
        ieee80211_vap_force_unpause(p2p_client_handle->vap, 0);
        p2p_client_handle->pause_state=0;
        OS_CANCEL_TIMER(&p2p_client_handle->pause_timer);
    }
    if (!ath_vap_pause_time) {
        return;
    }

    /* start   10 msec timer */
    OS_SET_TIMER(&p2p_client_handle->pause_timer, 10);
}

#endif
#endif

