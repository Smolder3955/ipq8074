/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2009-2010, Atheros Communications
 * implements P2P protpcol layer.
 * WARNING: synchronization issues are not handled yet.
 */


#include "ieee80211_p2p_device_priv.h"
#include "ieee80211_ie_utils.h"
#include "ieee80211_channel.h"
#include "ieee80211_p2p_proto.h"
#include "ieee80211_p2p_ie.h"
#include "ieee80211_defines.h"
#include "ieee80211_rateset.h"

#ifndef MAC2STR
#define MAC2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5]
#define MACSTR "%02x:%02x:%02x:%02x:%02x:%02x"
#endif

#if UMAC_SUPPORT_P2P

enum {
    IEEE80211_P2P_EVENT_SCAN,
};

/* Assumed that when the Device Type's category ID is zero, then it is invalid */
#define is_dev_type_cat_id_valid(_attrib_dev_type)   ((*(u_int16_t *)_attrib_dev_type) != 0)

/*
 * frame completion event .
 */
typedef struct _ieee80211_p2p_frame_completetion_event {
    u_int8_t src[IEEE80211_ADDR_LEN];
    u_int8_t dst[IEEE80211_ADDR_LEN];
    u_int8_t bssid[IEEE80211_ADDR_LEN];
    void     *arg;
    u_int32_t freq;
    ieee80211_xmit_status ts;
} ieee80211_p2p_frame_completion_event;

typedef struct _ieee80211_p2p_event {
    union {
        ieee80211_scan_event scan_event;
        ieee80211_p2p_frame_completion_event fc_event;
    } u;
} ieee80211_p2p_event;

#define IEEE80211_P2P_MAX_EVENT_LEN sizeof(ieee80211_p2p_event)
#define IEEE80211_P2P_MAX_EVENT_QUEUE_DEPTH 10

/* disable locking for now */
#define P2P_DEV_LOCK(_p2p_handle)    spin_lock(&(_p2p_handle)->p2p_lock);
#define P2P_DEV_UNLOCK(_p2p_handle)  spin_unlock(&(_p2p_handle)->p2p_lock);

static int
ieee80211_p2p_send_proberesp(struct ieee80211_node *ni, u_int8_t *macaddr);

static void ieee80211_action_frame_complete_handler(wlan_if_t vaphandle, wbuf_t wbuf,void *arg,
						u_int8_t *dst_addr, u_int8_t *src_addr, u_int8_t *bssid,
                        ieee80211_xmit_status *ts);

/*
 * Functions/macros that exhibit different behavior depending on whether
 * multiple scan support is enabled.
 */
#if ATH_SUPPORT_MULTIPLE_SCANS

/* 
 * Multiple scan support is enabled, allowing the driver to keep good track of
 * who requested which scan.
 */

/* Safely ignore termination of scans requested by other modules. */
#define ieee80211_p2p_unknown_scan_complete(_p2p_handle)    /* */

/* We can always start a new scan. */
#define ieee80211_p2p_can_start_scan(vap)                   true

#else /* ATH_SUPPORT_MULTIPLE_SCANS */

/*
 * Multiple scan support is disabled, so only one scan request may be present at a time
 */
 
/* Termination of any scan means no scans are currently executing */ 
#define ieee80211_p2p_unknown_scan_complete(_p2p_handle)    (_p2p_handle)->p2p_inprogress_scan_req_type = IEEE80211_P2P_SCAN_REQ_NONE

/* Can only start a new scan if scans are in progress. */
#define ieee80211_p2p_can_start_scan(vap)    (wlan_scan_in_progress(vap) == false)

#endif /* ATH_SUPPORT_MULTIPLE_SCANS */

static int ieee80211_p2p_send_action_scan(wlan_p2p_t p2p_handle);


/**
 * deliver event to other modules that registered an  event handler with this
 * module.
 */
static const char *event_names[] = {
    "CHAN_START",
    "CHAN_END",
    "DEV_FOUND",
    "GO_NEG_RECVD",
    "GO_NEG_COMPLETE",
    "SD_REQUEST_RECVD",
    "SD_RESPONSE_RECVD",
    "RX_FRAME",
    "PROV_DISC_REQ",
    "PROV_DISC_RESP",
    "SCAN_END",
};


static u_int8_t p2p_rates[] = {12 | IEEE80211_RATE_BASIC, 18, 24 | IEEE80211_RATE_BASIC, 36, 48|IEEE80211_RATE_BASIC, 72, 96, 108};  /* OFDM rates 6,9,12,18,24,36,48,54M */


/*
 * Verify whether scan event corresponds to a request made by the p2p port.
 */
static bool 
ieee80211_p2p_match_scan_event(wlan_p2p_t           p2p_handle,
                               ieee80211_scan_event *scan_event)
{
    /*
     * Scan event contains the ID of the module that requested the scan as well
     * as the scan ID. Compare these values against the values stored in the 
     * P2P device to make sure the event corresponds to a request made by the 
     * p2p port
     *
     * If multiple scan support is not enabled, scan_id is always 0, and the 
     * comparison then considers only the requestor. 
     */
    return ((p2p_handle->p2p_scan_requestor == scan_event->requestor) &&
            (p2p_handle->p2p_scan_id == scan_event->scan_id));
}

/* 
 * Check if scan event can be processed immediately or must be deferred to a
 * different thread to avoid deadlocks.
 */
static bool 
ieee80211_p2p_defer_scan_event(wlan_p2p_t           p2p_handle,
                               ieee80211_scan_event *scan_event)
{
    /* 
     * Scan completed events are always processed immediately
     */
    if ((scan_event->type == IEEE80211_SCAN_COMPLETED) || 
        (scan_event->type == IEEE80211_SCAN_DEQUEUED)) {
        return true;
    }

    /* 
     * Foreign channel indication processed only if originated from a scan
     * requested by this module.
     */
    if ((scan_event->type == IEEE80211_SCAN_FOREIGN_CHANNEL) &&
        ieee80211_p2p_match_scan_event(p2p_handle, scan_event)) {
        return true;
    }

    /* Discard other events */
    return false; 
}

/*
 * setup operational rates for p2p vaps.
 * p2p protocol requires not to use CCK rates.
 */
void ieee80211_p2p_setup_rates(wlan_if_t vaphandle) 
{
    int i;
    enum ieee80211_phymode mode;
    for (i = 0; i < IEEE80211_MODE_MAX; i++) {
        mode=(enum ieee80211_phymode) i;
        switch(mode) {
        case IEEE80211_MODE_11A:
        case IEEE80211_MODE_11G:
        case IEEE80211_MODE_11NA_HT20:
        case IEEE80211_MODE_11NG_HT20:
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NA_HT40MINUS:
        case IEEE80211_MODE_11NG_HT40PLUS:
        case IEEE80211_MODE_11NG_HT40MINUS:
            wlan_set_operational_rates(vaphandle, mode,p2p_rates, sizeof(p2p_rates));
            break;
        default:
            break;
        }
    }
}

void ieee80211_p2p_attach(struct ieee80211com *ic)
{
    if ((ic->ic_tsf_timer == NULL) && !(ieee80211com_has_cap_ext(ic, IEEE80211_CEXT_PERF_PWR_OFLD)))
    {
        printk("%s: P2P unavailable since no TSF Timer.\n", __func__);
        return;
    }
    /* Else continue with the attach */

    ic->ic_caps_ext |= IEEE80211_CEXT_P2P;

    ieee80211_ic_p2pDevEnable_set(ic);
}

void ieee80211_p2p_detach(struct ieee80211com *ic)
{
    return;     /* Nothing to do */
}

void
ieee80211_p2p_deliver_event(wlan_p2p_t p2p_handle, wlan_p2p_event *event)
{
    void                      *event_arg;
    wlan_p2p_event_handler    handler;

    P2P_DEV_LOCK(p2p_handle);
    handler = p2p_handle->event_handler;
    event_arg = p2p_handle->event_arg;
    P2P_DEV_UNLOCK(p2p_handle);

    if (handler) {
        handler(event_arg, event);
    }
    switch (event->type) {
    case WLAN_P2PDEV_DEV_FOUND:
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s:P2P_EVENT: %s devname %s addr %s\n",__func__,event_names[event->type],
                          event->u.dev_found.dev_name?event->u.dev_found.dev_name:"NULL",ether_sprintf(event->u.dev_found.dev_addr));
        break;
    case WLAN_P2PDEV_GO_NEGOTIATION_REQ_RECVD:
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s:P2P_EVENT: %s src_addr %s\n",__func__,event_names[event->type],
                          ether_sprintf(event->u.go_neg_req_recvd.src_addr));
        break;
    case WLAN_P2PDEV_GO_NEGOTIATION_COMPLETE:
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s:P2P_EVENT: %s peer_device_addr %s GO %d\n",__func__,event_names[event->type],
                          ether_sprintf(event->u.go_neg_results.peer_device_addr) , event->u.go_neg_results.role_go);
        break;
    case WLAN_P2PDEV_RX_FRAME:
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s:P2P_EVENT: %s frame type %d from %s\n",__func__,event_names[event->type],
                           event->u.rx_frame.frame_type,  ether_sprintf(event->u.rx_frame.src_addr));
        break;
    case WLAN_P2PDEV_CHAN_START:
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s:P2P_EVENT: %s req_id %d freq %d duration %d\n",__func__,
                event_names[event->type],event->req_id, event->u.chan_start.freq, event->u.chan_start.duration) ;
        break;
    case WLAN_P2PDEV_CHAN_END:
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s:P2P_EVENT: %s req_id %d  freq %d duration %d reason %d\n",__func__,
                event_names[event->type],event->req_id, event->u.chan_end.freq, event->u.chan_end.duration, event->u.chan_end.reason) ;
        break;
    default:
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s:P2P_EVENT: %s\n",__func__,event_names[event->type]);
        break;
    }
}

/**
 * check/update the action frame sequence number counter for this src addr
 * If src not present, add to the array (if room, else error)
 * If found, and previous update counter != seq_num, update counter and return true
 * If RETRY && current seq number value == seq_num, return false to indicate duplicate
 */
static bool
ieee80211_check_seq_counter(wlan_p2p_t p2p_handle, struct ieee80211_frame *wh)
{
    int i;
    uint8_t empty[IEEE80211_ADDR_LEN] = {0x00,0x00,0x00,0x00,0x00,0x00};
    ieee80211_p2p_act_frm_seq_ctr *ctr = NULL;
    u_int16_t seq_num = le16toh(*(u_int16_t *)wh->i_seq);
    u_int8_t *src_addr = wh->i_addr2;
    u_int32_t now = (u_int32_t)CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

    /* search the array for this src, if not found try to return an empty slot */
    for (i = 0; i < IEEE80211_P2P_MAX_SEQ_TRACK; i++) {
        ieee80211_p2p_act_frm_seq_ctr *check = &p2p_handle->p2p_seq_tracker[i];

        /* if the array entry is filled, but old, clear it out */
        if (!IEEE80211_ADDR_EQ(check->src, &empty) &&
            ((now - check->last_sec) > IEEE80211_P2P_SEQ_TRACK_MS)) {
            /*IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY,
                            "%s clearing old ACT frame TRACKER SA[" MACSTR "] "
                          "seq[%04x] now %x last_sec %x\n", __func__,
                          MAC2STR(check->src), check->last_seq,
                          now, check->last_sec);*/
            OS_MEMZERO(check, sizeof(ieee80211_p2p_act_frm_seq_ctr));
        }

        if (IEEE80211_ADDR_EQ(check->src, src_addr)) {
            /* found it, and it's within the time tracking interval */
            ctr = check;
            break;
        } else if ((ctr == NULL) && (IEEE80211_ADDR_EQ(check->src, &empty))) {
            /* set ctr to 1st empty array entry */
            ctr = check;
        }
    }
    if (ctr == NULL) {
        /* not found, and no room in the array */
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY,
                          "%s: no room in act frm seq tracker\n", __func__);
        /* should we drop? seems smarter to allow dupes in this case */
        return true;
    } else if (i == IEEE80211_P2P_MAX_SEQ_TRACK) {
        /* we didn't find it, ctr points at empty slot, set it up for this src */
        OS_MEMCPY(ctr->src, src_addr, IEEE80211_ADDR_LEN);
        ctr->last_seq = seq_num - 1; /* can't be a dupe, choose something not == seq_num */
    }

    if ((wh->i_fc[1] & IEEE80211_FC1_RETRY) &&
               (ctr->last_seq == seq_num))
    {
        /* duplicate frame */
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s ACTION DA[" MACSTR "] SA[" MACSTR "] "
                          "seq[%04x] RETRY/DUPLICATE, dropping\n", __func__, MAC2STR(wh->i_addr1),
                          MAC2STR(src_addr), seq_num);
        return false;
    } else {
        /* OK. just update the counter */
        /*IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s ACTION DA[" MACSTR "] SA[" MACSTR "] "
                          "seq[%04x] NEW, prev %04x OK (%d sec)\n", __func__, MAC2STR(wh->i_addr1),
                          MAC2STR(src_addr), seq_num, ctr->last_seq, ctr->last_sec);*/
        ctr->last_seq = seq_num;
        ctr->last_sec = now;
    }
    return true;
}

/*
 * Compare the given Request Device Type and our own device type and see if it matches.
 * Also, need to handle the case where the SubCat and OUI are "wildcards".
 */
static bool
ieee80211_p2p_dev_type_matched(u_int8_t *requested_dev_type, u_int8_t *dev_type)
{
    if (OS_MEMCMP(requested_dev_type, dev_type, IEEE80211_P2P_DEVICE_TYPE_LEN) == 0) {
        /* all matched */
        return true;
    }
    /* Else check if only Cat ID matches and the rest are all zeros */
    if ((OS_MEMCMP(requested_dev_type, dev_type, 2) == 0) &&
        (P2PIE_GET_BE32(&requested_dev_type[2]) == 0) &&
        (P2PIE_GET_BE16(&requested_dev_type[6]) == 0))
    {
        /* Category ID matches with wildcard OUI and sub-cat. ID*/
        return true;
    }
    return false;
}

/*
 * Check if the requested device type (if any) matches with
 * our primary or secondary device type list. Returns true if matches or
 * requested device type is not valid.
 */
static bool
p2p_dev_type_list_matched(wlan_p2p_t p2p_handle, u_int8_t *requested_dev_type)
{
    bool    matched = true;
    int     i;

    /* If our primary or secondary device types are defined and there is a
     * desired device type in the WPS IE, then make sure the type matched 
     */
    do {
        if (!is_dev_type_cat_id_valid(requested_dev_type))
        {
            /* Have no requested device type in the WPS IE. */
            break;
        }
        /* Else, make sure it matches with our primary or secondary types. */

        if (!is_dev_type_cat_id_valid(p2p_handle->pri_dev_type))
        {
            /* We did not set our primary device type (which is true for the non-win8 WFD implementations).
             * We assumed that we don't care about device type and always
             * respond. This is not really correct as per spec. But there
             * is no harm in sending extra probe responses. */
            break;
        }

        /* Check the primary device type */
        if (ieee80211_p2p_dev_type_matched(requested_dev_type, p2p_handle->pri_dev_type))
        {
            /* primary device type matches */
            break;
        }

        /* Check the secondary device types */
        for (i = 0; i < p2p_handle->num_sec_dev_types; i++)
        {
            u_int8_t    *sec_dev_type;

            ASSERT(p2p_handle->sec_dev_type != NULL);

            sec_dev_type = &p2p_handle->sec_dev_type[i * IEEE80211_P2P_DEVICE_TYPE_LEN];

            if (ieee80211_p2p_dev_type_matched(requested_dev_type, sec_dev_type))
            {
                /* secondary device type matches */
                break;
            }
        }

        if (i != p2p_handle->num_sec_dev_types) {
            /* Found a match and break from loop */
            break;
        }

        /* Not matched! */
        matched = false;

    } while ( false );

    return matched;
}

/**
 * input filter management function for p2p vap.
 * consume all the management frames.
 * send probe req, probe resp, beacon, action management frames.
 */
static int
ieee80211_p2p_input_filter_mgmt(wlan_node_t ni, wbuf_t wbuf, int subtype, struct ieee80211_rx_status *rs)
{
    wlan_p2p_t p2p_handle;
    wlan_if_t vap;
    u_int8_t *frm, *efrm, *ie_data;
    u_int16_t frame_len,ie_len;
    struct ieee80211_frame *wh;
    u_int32_t freq;
    bool send_event=false, filter_out=true;
    struct p2p_parsed_ie pie;
    wlan_chan_t channel;
    ieee80211_scan_entry_t scan_entry = NULL;

    vap = ieee80211_node_get_vap(ni);
    p2p_handle = ieee80211vap_get_private_data(vap);

    /*
     * if the device vap is not active, only allow action frames,
     * do not allow any other frames.
     */
    if (!ieee80211_vap_active_is_set(vap) && (subtype != IEEE80211_FC0_SUBTYPE_ACTION)) {
        return 0;
    }

    frm = wbuf_header(wbuf);
    efrm = wbuf_header(wbuf) + wbuf_get_pktlen(wbuf);
    frame_len =  wbuf_get_pktlen(wbuf);
    wh = (struct ieee80211_frame *) frm;
    channel = ieee80211_get_current_channel(p2p_handle->p2p_devhandle);
    freq = wlan_channel_frequency(channel);

    if (!IEEE80211_ADDR_EQ(wh->i_addr1, wlan_vap_get_macaddr(vap)) &&
        !IEEE80211_IS_MULTICAST(wh->i_addr1)) {
		/* only accept directed and multicast frames. */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s DA[%s]: Freq[%4u], Frame MGMT "
                          "subtype[%02x] seq[0x%04x], Not for me - DISCARDING !!\n",
                          __func__, ether_sprintf(wh->i_addr1), rs->rs_freq, subtype,
						  le16toh(*(u_int16_t *)wh->i_seq));
        return filter_out;
    }

    ie_data = ieee80211_mgmt_iedata(wbuf,subtype);
    if (ie_data == NULL && subtype != IEEE80211_FC0_SUBTYPE_ACTION) {
        /* Error in finding the IE data. do nothing */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s SA[%s]: Freq[%4u], Frame MGMT "
                          "subtype[%02x] seq[0x%04x], No IE - DISCARDING !!\n",
                          __func__, ether_sprintf(wh->i_addr2), rs->rs_freq, subtype,
                          le16toh(*(u_int16_t *)wh->i_seq));
        return filter_out;
    }
    ie_len = frame_len - (ie_data - frm);
    wh = (struct ieee80211_frame *) frm;

    switch(subtype) {
    case  IEEE80211_FC0_SUBTYPE_PROBE_RESP:
    case  IEEE80211_FC0_SUBTYPE_BEACON:
        scan_entry = ieee80211_update_beacon(ni, wbuf, wh, subtype, rs);
        if (scan_entry) {
            util_scan_free_cache_entry(scan_entry);
        }
        break;
    default:
        break;
    };

    switch(subtype) {
    case  IEEE80211_FC0_SUBTYPE_PROBE_RESP:
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s PROBE_RESP DA[" MACSTR "] SA[" MACSTR "] "
                          "seq[%04x] freq[%4u]\n",__func__, MAC2STR(wh->i_addr1),
                          MAC2STR(wh->i_addr2), le16toh(*(u_int16_t *)wh->i_seq), freq);
        filter_out = false; /* let the mlme/scan recieve the frame  to build scan cache*/
        send_event=true;
        break;
    case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s PROBE_REQ  DA[" MACSTR "] SA[" MACSTR "] "
                          "seq[%04x] freq[%4u]\n", __func__, MAC2STR(wh->i_addr1),
                          MAC2STR(wh->i_addr2), le16toh(*(u_int16_t *)wh->i_seq), freq);
        if (p2p_handle->p2p_inprogress_scan_req_type == IEEE80211_P2P_SCAN_REQ_EXTERNAL_LISTEN) {
            OS_MEMZERO(&pie, sizeof(pie));
            if (ieee80211_p2p_parse_ies(p2p_handle->p2p_oshandle, vap, ie_data, ie_len, &pie) == 0) {

                filter_out = true; /* filter out all management frames */
                send_event = false; /* Init. to no indicate to supplicant */

                do {
                    int     ret;

                    if (!pie.p2p_attributes) {
                        /* No P2P IE included, don't even pass up */
                        break;
                    }

                    /* From this point onwards, we indicate to supplicant */
                    send_event = true;

                    if (pie.rateset.rs_nrates == 0) {
                        /* No rates found */
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, 
                                          "%s: Not able to get rates from probe request to filter 11b devices\n", 
                                          __func__);
                        break;
                    }

                    /* Make sure the SSID is p2p wildcard SSID */
                    if (!pie.is_p2p_wildcard_ssid) {
                        /* Invalid ssid, but still let supplicant know */
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, 
                                          "%s: Probe Req does not have P2P Wildcard SSID\n", 
                                          __func__);
                        break;
                    }
                    
                    if (ieee80211_check_11b_rates(vap, &pie.rateset)) {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, 
                                          "%s: no reply to PROBE_RESP since 11b rates only\n", 
                                          __func__);
                        break;
                    }

                    /* check if the requested device type (if any) matches with
                     * our primary or secondary device type list. */
                    if (!p2p_dev_type_list_matched(p2p_handle, pie.wps_requested_dev_type)) {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_P2P_PROT, 
                                          "%s reject PROBE_REQ since req dev type mismatched. cat=%x %x, subcat=%x %x, oui=%x %x %x %x\n",
                                          __func__, pie.wps_requested_dev_type[0], pie.wps_requested_dev_type[1],
                                          pie.wps_requested_dev_type[6], pie.wps_requested_dev_type[7], 
                                          pie.wps_requested_dev_type[2], pie.wps_requested_dev_type[3], 
                                          pie.wps_requested_dev_type[4], pie.wps_requested_dev_type[5]);
                        break;
                    }

                    if(pie.device_id && 
                       p2p_handle->p2p_device_address_valid &&
                       !IEEE80211_ADDR_EQ(p2p_handle->p2p_device_address, pie.device_id)) {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: Device ID doesn't match\n", __func__);
        			    break;
                    }
                    
                    if(pie.wps_attributes &&
                       pie.wps_attributes_parsed.num_req_dev_type &&
                       p2p_handle->wps_primary_device_type_valid){
                        u_int8_t i;

                        for(i = 0; i < pie.wps_attributes_parsed.num_req_dev_type; i++){
                            if(pie.wps_attributes_parsed.req_dev_type[i] &&
                                !OS_MEMCMP(pie.wps_attributes_parsed.req_dev_type[i], p2p_handle->wps_primary_device_type, WPS_DEV_TYPE_LEN))
                                break;
                        }
                        
                        if(i == pie.wps_attributes_parsed.num_req_dev_type){
                            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: primary_dev_type not match our\n", __func__);
                            break;  
                        }
                    }

                    /* Send the Probe Response */
                    ret = ieee80211_p2p_send_proberesp(ni, wh->i_addr2);
                    if (ret != 0) {
                        printk("%s: sending of PROBE_RESP failed. ret=0x%x\n", __func__, ret);
                    }
                    else {
                        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s Reply to PROBE_REQ with Probe Response.\n",
                                          __func__);
                    }

                } while (false);

                ieee80211_p2p_parse_free(&pie);
            } else {
                send_event = false;
                filter_out = true; /* not a frame we care about, don't even pass up */
            }
        }
        break;
    case IEEE80211_FC0_SUBTYPE_ACTION:
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s ACTION DA[" MACSTR "] SA[" MACSTR "] "
                          "seq[%04x] freq[%4u]\n", __func__, MAC2STR(wh->i_addr1),
                          MAC2STR(wh->i_addr2), le16toh(*(u_int16_t *)wh->i_seq), freq);
        /* check for duplicate frames */
        if (ieee80211_check_seq_counter(p2p_handle, wh) != true) {
            return false;
        }
        filter_out=false;
        send_event=true;
        break;
    default:
        break;
    }
    /*
     * only send the events if the scan req is from external(os shim)
     */
    if (send_event /* && (p2p_handle->p2p_inprogress_scan_req_type == IEEE80211_P2P_SCAN_REQ_EXTERNAL_SCAN ||
                      p2p_handle->p2p_inprogress_scan_req_type== IEEE80211_P2P_SCAN_REQ_EXTERNAL_LISTEN ||
                      p2p_handle->p2p_inprogress_scan_req_type== IEEE80211_P2P_SCAN_REQ_EXTERNAL_SEARCH) */ )
    {
        wlan_p2p_event    p2p_event;

        p2p_event.type                  = WLAN_P2PDEV_RX_FRAME;
        p2p_event.req_id                = p2p_handle->p2p_cur_req_id;
        p2p_event.u.rx_frame.frame_type = subtype;
        p2p_event.u.rx_frame.frame_len  = frame_len;
        p2p_event.u.rx_frame.frame_buf  = frm;
        p2p_event.u.rx_frame.ie_len     = ie_len;
        p2p_event.u.rx_frame.ie_buf     = ie_data;
        p2p_event.u.rx_frame.src_addr   = wh->i_addr2;
        p2p_event.u.rx_frame.frame_rssi = rs->rs_rssi;
        p2p_event.u.rx_frame.freq       = freq;
        p2p_event.u.rx_frame.chan_flags = wlan_channel_flags(channel);
        p2p_event.u.rx_frame.wbuf       = wbuf;
        
        ieee80211_p2p_deliver_event(p2p_handle, &p2p_event);
    }
    /*
     * filter out the frame.
     */
    return filter_out;
}

static int
ieee80211_p2p_output_filter(wbuf_t wbuf)
{
    return false;
}

static void
ieee80211_p2p_scan_evhandler(wlan_if_t vaphandle,ieee80211_scan_event *event, void *arg)
{
    wlan_p2p_t p2p_handle = (wlan_p2p_t) arg;
	ieee80211_xmit_status ts;

    /*
     * SCANNER Limitation. you can not restart scan from the scan event handler.
     * calling into p2p module from here will result in scan restart and
     * which will fail. so handle the scan events asynchronously with a message queue.
     */
    if (ieee80211_p2p_defer_scan_event(p2p_handle, event)) {
    if (OS_MESGQ_SEND(&p2p_handle->p2p_mesg_q, IEEE80211_P2P_EVENT_SCAN, sizeof(ieee80211_scan_event), (void *)event) != 0) {
        printk("%s: message queue send failed\n",__func__);
    }
    }
    IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s scan_id %d scan event %s reqid 0x%08X (%s) p2p_scan_id %d p2p_scan_reqid 0x%08X (%s) scan_in_progress type=%d id=%d scan_pending type=%d id=%d\n",
                      __func__,
                      event->scan_id, 
                      wlan_scan_notification_event_name(event->type), 
                      event->requestor, wlan_scan_requestor_name(p2p_handle->p2p_vap, event->requestor),
                      p2p_handle->p2p_scan_id,
                      p2p_handle->p2p_scan_requestor, wlan_scan_requestor_name(p2p_handle->p2p_vap, p2p_handle->p2p_scan_requestor),
                      p2p_handle->p2p_inprogress_scan_req_type,
                      p2p_handle->p2p_cur_req_id,
                      p2p_handle->p2p_pending_scan_req_type,
                      p2p_handle->p2p_req_id);

    /*
     * Ignore notifications received due to scans requested by other modules
     */
    if (! ieee80211_p2p_match_scan_event(p2p_handle, event)) {
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s, scan match failed - p2p_scan_requestor =%x, scan_event->requestor =%x, p2p_scan_id=%x, scan_id=%x\n",
                    __func__, p2p_handle->p2p_scan_requestor, event->requestor, 
                    p2p_handle->p2p_scan_id, event->scan_id);
        return;
    }
    if ( event->type == IEEE80211_SCAN_FOREIGN_CHANNEL) {
        ieee80211_p2p_act_frame *act_frame, *tmpactFrame = NULL;
        u_int32_t   freq = wlan_channel_frequency(ieee80211_get_current_channel(p2p_handle->p2p_devhandle));

        if (p2p_handle->p2p_protection_mode != IEEE80211_PROTECTION_NONE &&  
            IEEE80211_IS_CHAN_2GHZ( ieee80211_get_current_channel(p2p_handle->p2p_devhandle)) ) {
            wlan_set_param(p2p_handle->p2p_vap, IEEE80211_PROTECTION_MODE, p2p_handle->p2p_protection_mode); 
        } else {
            wlan_set_param(p2p_handle->p2p_vap, IEEE80211_PROTECTION_MODE, IEEE80211_PROTECTION_NONE); 
        }
        P2P_DEV_LOCK(p2p_handle);
        TAILQ_FOREACH_SAFE(act_frame, &p2p_handle->p2p_act_frame_head, act_frame_list, tmpactFrame) {
            if (act_frame->freq == freq) {
                TAILQ_REMOVE(&p2p_handle->p2p_act_frame_head, act_frame,act_frame_list);
                TAILQ_INSERT_HEAD(&p2p_handle->p2p_act_frame_sent, act_frame,act_frame_list);

                if (wlan_vap_send_action_frame(p2p_handle->p2p_vap,freq, ieee80211_action_frame_complete_handler,
                                           act_frame,act_frame->dst,act_frame->src,act_frame->bssid,
                                           act_frame->buf, (u_int32_t)act_frame->len) != EOK)
				{
                    // Failed send action frame, then call complete handler to indicate error with transmit status.
                    ts.ts_flags   = IEEE80211_TX_ERROR;
                    ts.ts_retries = 0;

                    ieee80211_action_frame_complete_handler(p2p_handle->p2p_vap, NULL, act_frame, act_frame->dst,
                                                            act_frame->src, act_frame->bssid, &ts);
				}
            }
        }
        P2P_DEV_UNLOCK(p2p_handle);
    }
}

static void
ieee80211_p2p_mesgq_event_handler(void *ctx,u_int16_t event,
                                  u_int16_t event_data_len, void *event_data)
{
    wlan_p2p_t        p2p_handle = (wlan_p2p_t) ctx;
    wlan_if_t         vap = p2p_handle->p2p_vap;
    wlan_p2p_event    p2p_event;

    if (event == IEEE80211_P2P_EVENT_SCAN) {
        ieee80211_scan_event    *scan_event = (ieee80211_scan_event *) event_data;

        P2P_DEV_LOCK(p2p_handle);
        if ((scan_event->type == IEEE80211_SCAN_COMPLETED) || (scan_event->type == IEEE80211_SCAN_DEQUEUED)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s scan completed evtscanid %d p2pscanid %d reqid 0x%08X (%s) p2p_scan_reqid 0x%08X (%s) scan_in_progress type=%d id=%d scan_pending type=%d id=%d\n",
                              __func__, 
                              scan_event->scan_id, 
                              p2p_handle->p2p_scan_id,
                              scan_event->requestor, wlan_scan_requestor_name(p2p_handle->p2p_vap, scan_event->requestor),
                              p2p_handle->p2p_scan_requestor, wlan_scan_requestor_name(p2p_handle->p2p_vap, p2p_handle->p2p_scan_requestor),
                              p2p_handle->p2p_inprogress_scan_req_type,
                              p2p_handle->p2p_cur_req_id,
                              p2p_handle->p2p_pending_scan_req_type,
                              p2p_handle->p2p_req_id);

            /*
             * Only send notifications corresponding to scans started by this port.
             */
            if (ieee80211_p2p_match_scan_event(p2p_handle, scan_event)) {
            switch(p2p_handle->p2p_inprogress_scan_req_type) {
            case IEEE80211_P2P_SCAN_REQ_NONE:
            case IEEE80211_P2P_SCAN_REQ_EXTERNAL_SCAN:
                p2p_handle->p2p_inprogress_scan_req_type = IEEE80211_P2P_SCAN_REQ_NONE;
                break;

            case IEEE80211_P2P_SCAN_REQ_EXTERNAL_LISTEN:
            case IEEE80211_P2P_SCAN_REQ_EXTERNAL_SEARCH:
                {
                    /* external listen is complete */
                    /* deliver CHAN_END event notification */
                    p2p_event.type                = WLAN_P2PDEV_CHAN_END;
                    p2p_event.req_id              = p2p_handle->p2p_cur_req_id;
                    p2p_event.u.chan_end.freq     = p2p_handle->p2p_cur_listen_freq;
                    p2p_event.u.chan_end.duration = p2p_handle->p2p_cur_listen_duration;
                    if (scan_event->reason == IEEE80211_REASON_COMPLETED) {
                        /* duration complete */
                        p2p_event.u.chan_end.reason = WLAN_P2PDEV_CHAN_END_REASON_TIMEOUT;
                        } else if (scan_event->reason == IEEE80211_REASON_CANCELLED) {
                            /* cancelled */
                        p2p_event.u.chan_end.reason = WLAN_P2PDEV_CHAN_END_REASON_CANCEL;
                    } else {
                            /* some other reason */
                        p2p_event.u.chan_end.reason = WLAN_P2PDEV_CHAN_END_REASON_ABORT;
                    }
                    /* 
                     * clear p2p_inprogress_scan_req_type so that another scan
                     * can start while the event is delivered with the lock 
                     * released
                     */
                    p2p_handle->p2p_inprogress_scan_req_type = IEEE80211_P2P_SCAN_REQ_NONE;
                    P2P_DEV_UNLOCK(p2p_handle);

                    ieee80211_p2p_deliver_event(p2p_handle,&p2p_event);

                    P2P_DEV_LOCK(p2p_handle);
                }
                break;

            case IEEE80211_P2P_SCAN_REQ_SEND_ACTION:
                p2p_handle->p2p_inprogress_scan_req_type = IEEE80211_P2P_SCAN_REQ_NONE;
                break;

                default:
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: Unknown request type %d\n", 
                        __func__, p2p_handle->p2p_inprogress_scan_req_type);
                    break;
            }
            }
            else {
                /* Some other module requested the scan that has completed. */
                ieee80211_p2p_unknown_scan_complete(p2p_handle);
            }
            
            /*
             * Start the next request for the P2P device after the previous one
             * has completed.
             */
            if (p2p_handle->p2p_inprogress_scan_req_type == IEEE80211_P2P_SCAN_REQ_NONE) {
            /* no scan request is in progress, check if there are any pending requests, if yes start the pending request */
            if (p2p_handle->p2p_pending_scan_req_type != IEEE80211_P2P_SCAN_REQ_NONE) {
                /* start a scan */
                if (wlan_scan_start(vap,
                                    &p2p_handle->p2p_scan_params,
                                    p2p_handle->p2p_scan_requestor,
                                    IEEE80211_P2P_DEFAULT_SCAN_PRIORITY, 
                                    &(p2p_handle->p2p_scan_id)) != EOK) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s scan req failed type=%d id=%d\n",
                        __func__,
                        p2p_handle->p2p_pending_scan_req_type,
                        p2p_handle->p2p_req_id);

                    P2P_DEV_UNLOCK(p2p_handle);
                    return;
                }
                p2p_handle->p2p_inprogress_scan_req_type = p2p_handle->p2p_pending_scan_req_type;
                p2p_handle->p2p_cur_req_id               = p2p_handle->p2p_req_id;
                p2p_handle->p2p_cur_listen_freq          = p2p_handle->p2p_listen_freq;
                p2p_handle->p2p_cur_listen_duration      = p2p_handle->p2p_listen_duration;
                p2p_handle->p2p_pending_scan_req_type    = IEEE80211_P2P_SCAN_REQ_NONE;
            } else {
                int rc;

                P2P_DEV_UNLOCK(p2p_handle);
                /* no pending scan req, unregister scan handler */
                rc = wlan_scan_unregister_event_handler(vap,ieee80211_p2p_scan_evhandler,(os_if_t)p2p_handle);
                if (rc != EOK) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                                      "%s: wlan_scan_unregister_event_handler "
                                      "failed handler=%08p,%08p rc=%08X\n",
                                      __func__, ieee80211_p2p_scan_evhandler,
                                      p2p_handle, rc);
                }
                P2P_DEV_LOCK(p2p_handle);
            }

            /* start the scan to send action frame if there are any action frames pending  */
            ieee80211_p2p_send_action_scan(p2p_handle);
            }
        } else if (scan_event->type == IEEE80211_SCAN_FOREIGN_CHANNEL) {
            /* check if it is  the scan initiated by me (p2p module ) */
            if (ieee80211_p2p_match_scan_event(p2p_handle, scan_event)) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s scan on foreign channel event: requestor 0x%08X (%s) id=%d p2p_handle: requestor 0x%08X (%s) scan_in_progress type=%d id=%d scan_pending type=%d id=%d\n",
                              __func__, 
                              scan_event->requestor, wlan_scan_requestor_name(p2p_handle->p2p_vap, scan_event->requestor),
                                  scan_event->scan_id,
                              p2p_handle->p2p_scan_requestor, wlan_scan_requestor_name(p2p_handle->p2p_vap, p2p_handle->p2p_scan_requestor),
                              p2p_handle->p2p_inprogress_scan_req_type,
                              p2p_handle->p2p_cur_req_id,
                              p2p_handle->p2p_pending_scan_req_type,
                              p2p_handle->p2p_req_id);

                if ((p2p_handle->p2p_inprogress_scan_req_type == IEEE80211_P2P_SCAN_REQ_EXTERNAL_LISTEN) ||
                     (p2p_handle->p2p_inprogress_scan_req_type == IEEE80211_P2P_SCAN_REQ_EXTERNAL_SEARCH))
                {
                    /* deliver CHAN_START event notification */
                    p2p_event.type                    = WLAN_P2PDEV_CHAN_START;
                    p2p_event.req_id                  = p2p_handle->p2p_cur_req_id;
                    p2p_event.u.chan_start.freq       = p2p_handle->p2p_cur_listen_freq;
                    p2p_event.u.chan_start.chan_flags = wlan_channel_flags(scan_event->chan);
                    p2p_event.u.chan_start.duration   = p2p_handle->p2p_cur_listen_duration;
                    P2P_DEV_UNLOCK(p2p_handle);
                    ieee80211_p2p_deliver_event(p2p_handle,&p2p_event);
                    return;
                }
            }
        }
        P2P_DEV_UNLOCK(p2p_handle);
    }
}

/**
 * issue a scan request (called for listen,scan operations issued by
 * by external module).
 */
static int
ieee80211_p2p_start_scan(wlan_p2p_t p2p_handle, ieee80211_p2p_scan_request type)
{
    wlan_if_t                   vap = p2p_handle->p2p_vap;

    wlan_scan_register_event_handler(vap,ieee80211_p2p_scan_evhandler,(os_if_t)p2p_handle);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s num_channels %d chan_1 %d scan_in_progress type=%d id=%d scan_pending type=%d id=%d scan_in_progress %d pri=%d\n", 
        __func__,
        p2p_handle->p2p_scan_params.num_channels, 
        (p2p_handle->p2p_scan_params.num_channels == 0) ? 0 : *p2p_handle->p2p_scan_params.chan_list,
        p2p_handle->p2p_inprogress_scan_req_type,
        p2p_handle->p2p_cur_req_id,
        p2p_handle->p2p_pending_scan_req_type,
        p2p_handle->p2p_req_id,
        wlan_scan_in_progress(vap),
        p2p_handle->p2p_scan_params.p2p_scan_priority);

    /*
     * P2P Device can handle only one scan request at a time, so make sure no
     * requests are pending.
     */
    if ((!ieee80211_p2p_can_start_scan(vap)) || 
        (p2p_handle->p2p_inprogress_scan_req_type != IEEE80211_P2P_SCAN_REQ_NONE)) {
        /*
         * scan is in progress currently (was issued either by us or by somebody else).
         * queue the request.
         */

        p2p_handle->p2p_pending_scan_req_type = type;

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s queue the scan request scan_in_progress\n",
                          __func__);
        return 0;
    }

    /* start a scan */
    p2p_handle->p2p_inprogress_scan_req_type = type;
    if (wlan_scan_start(vap,
                        &p2p_handle->p2p_scan_params,
                        p2p_handle->p2p_scan_requestor,
                        p2p_handle->p2p_scan_params.p2p_scan_priority,
                        &(p2p_handle->p2p_scan_id)) != EOK) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s scan req failed type=%d id=%d\n",
            __func__,
            p2p_handle->p2p_pending_scan_req_type,
            p2p_handle->p2p_req_id);

        /* 
         * clear p2p_inprogress_scan_req_type so that another scan
         * can start while the event is delivered with the lock 
         * released
         */
        p2p_handle->p2p_inprogress_scan_req_type = IEEE80211_P2P_SCAN_REQ_NONE;
        
        return EBUSY;
    }
    p2p_handle->p2p_cur_req_id          = p2p_handle->p2p_req_id;
    p2p_handle->p2p_cur_listen_freq     = p2p_handle->p2p_listen_freq;
    p2p_handle->p2p_cur_listen_duration = p2p_handle->p2p_listen_duration;

    return 0;
}


/* p2p_scan - Request a P2P scan/search
 * @ctx: Callback context from cb_ctx
 * @full_scan: Whether this is a full scan of all channels
 * @freq: Specific frequency (MHz) to scan or 0 for no restriction
 * Returns: 0 on success, -1 on failure
 *
 * This callback function is used to request a P2P scan or search
 * operation to be completed. If full_scan is requested (non-zero),
 * this is request for the initial scan to find group owners from all
 * channels. If full_scan is not requested (zero), this is a request
 * for search phase scan of the social channels. In that case, non-zero
 * freq value can be used to indicate only a single social channel to
 * scan for (the known listen channel of the intended peer).
 *
 * The scan results are returned after this call by calling
 * p2p_scan_res_handler() for each scan result that has a P2P IE and
 * then calling p2p_scan_res_handled() to indicate that all scan
 * results have been indicated.
 */
int
ieee80211_p2p_scan(wlan_p2p_t p2p_handle, int full_scan, int freq, ieee80211_p2p_scan_request type)
{
    u_int32_t social_channels[IEEE80211_P2P_NUM_SOCIAL_CHANNELS] = {IEEE80211_P2P_SOCIAL_CHANNELS};
    wlan_if_t vap = p2p_handle->p2p_vap;
    int retval;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s\n",__func__);
    P2P_DEV_LOCK(p2p_handle);
    if (p2p_handle->p2p_pending_scan_req_type != IEEE80211_P2P_SCAN_REQ_NONE) {
        /*
         * received a scan request while one is pending.
         */
        P2P_DEV_UNLOCK(p2p_handle);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s unexpected: received scan request while one of type %d is pending\n",
         __func__,p2p_handle->p2p_pending_scan_req_type);
         return -1;
    }

    p2p_handle->p2p_scan_params.type = IEEE80211_SCAN_FOREGROUND;
    p2p_handle->p2p_scan_params.flags = IEEE80211_SCAN_FORCED | IEEE80211_SCAN_ALLBANDS | IEEE80211_SCAN_ACTIVE;
    p2p_handle->p2p_scan_params.flags |= IEEE80211_SCAN_ADD_BCAST_PROBE;
    p2p_handle->p2p_scan_params.min_dwell_time_active = p2p_handle->p2p_scan_params.max_dwell_time_active = P2P_DWELL_TIME_ACTIVE;
    p2p_handle->p2p_scan_params.min_dwell_time_passive = p2p_handle->p2p_scan_params.max_dwell_time_passive = P2P_DWELL_TIME_PASSIVE;
    p2p_handle->p2p_scan_params.min_rest_time = p2p_handle->p2p_scan_params.max_rest_time = P2P_REST_TIME;
    p2p_handle->p2p_scan_params.repeat_probe_time = 0;
    p2p_handle->p2p_scan_params.num_ssid = 1;
    OS_MEMCPY(p2p_handle->p2p_scan_params.ssid_list[0].ssid, IEEE80211_P2P_WILDCARD_SSID, IEEE80211_P2P_WILDCARD_SSID_LEN);
    p2p_handle->p2p_scan_params.ssid_list[0].len =  IEEE80211_P2P_WILDCARD_SSID_LEN;

    if (full_scan) {
        /* scan all channels */
        p2p_handle->p2p_scan_params.num_channels = 0;
    } if (freq == 0) {
        int i;
        /* scan p2p social channels */
        p2p_handle->p2p_scan_params.num_channels = IEEE80211_P2P_NUM_SOCIAL_CHANNELS;
        for (i=0;i<IEEE80211_P2P_NUM_SOCIAL_CHANNELS; ++i) {
             p2p_handle->p2p_scan_channels[i] = social_channels[i];
        }
        p2p_handle->p2p_scan_params.chan_list = p2p_handle->p2p_scan_channels;
    } else {
        /* scan only the specified frequency */
        p2p_handle->p2p_scan_params.num_channels = 1;
        p2p_handle->p2p_scan_channels[0] =  wlan_mhz2ieee(p2p_handle->p2p_devhandle, freq, 0);
        p2p_handle->p2p_scan_params.chan_list = p2p_handle->p2p_scan_channels;
    }

    p2p_handle->p2p_scan_params.chan_list = p2p_handle->p2p_scan_channels;

    wlan_scan_register_event_handler(vap,ieee80211_p2p_scan_evhandler,(os_if_t)p2p_handle);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s num_channels %d chan_1 %d\n", __func__,
                      p2p_handle->p2p_scan_params.num_channels, p2p_handle->p2p_scan_params.num_channels==0?0:*p2p_handle->p2p_scan_params.chan_list);

    retval = ieee80211_p2p_start_scan(p2p_handle,type);
    P2P_DEV_UNLOCK(p2p_handle);
    return retval;
}

int
ieee80211_p2p_start_listen(wlan_p2p_t p2p_handle, unsigned int freq, u_int32_t req_id,
                           unsigned int duration, ieee80211_p2p_scan_request type,
                           IEEE80211_SCAN_PRIORITY scan_priority)
{
    wlan_if_t vap = p2p_handle->p2p_vap;
    int retval;

    KASSERT(vap != NULL, ("vap not set"));
    P2P_DEV_LOCK(p2p_handle);

    /*
     * use scan operation to implement listen.
     * in the listen state the radio needs to be switched to a channel and
     * listen for probe requests and respond to them without sending any probe
     * requests.
     * setup a passive scan so that no probe request will be sent out.
     */
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "LISTEN: %s type=%d, pri=%d, freq=%d, dur=%d, req_id=%d, \n",
        __func__, type, scan_priority, freq, duration, req_id);

    if (p2p_handle->p2p_pending_scan_req_type != IEEE80211_P2P_SCAN_REQ_NONE) {
        /*
         * received a scan request while one is pending.
         */
       if (p2p_handle->p2p_pending_scan_req_type != type) {
           P2P_DEV_UNLOCK(p2p_handle);
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s unexpected: received scan request while one of type %d is pending\n",
            __func__,p2p_handle->p2p_pending_scan_req_type);
           return -1;
       } else {
           IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s overwriting previous pending scan req reqid %d freq %d duration %d with               reqid %d freq %d duration %d\n",
             __func__, p2p_handle->p2p_req_id , p2p_handle->p2p_listen_freq, p2p_handle->p2p_listen_duration,
             req_id, freq, duration); 
       }
    }
    wlan_set_default_scan_parameters(vap,&p2p_handle->p2p_scan_params,IEEE80211_M_STA,true,true,true,true,0,NULL,0);
    p2p_handle->p2p_scan_params.p2p_scan_priority = scan_priority;
    p2p_handle->p2p_scan_params.type = IEEE80211_SCAN_FOREGROUND;
    p2p_handle->p2p_scan_params.flags = IEEE80211_SCAN_FORCED | IEEE80211_SCAN_ALLBANDS | IEEE80211_SCAN_PASSIVE;
    p2p_handle->p2p_scan_params.flags |= IEEE80211_SCAN_ADD_BCAST_PROBE;
    p2p_handle->p2p_scan_params.min_dwell_time_active = p2p_handle->p2p_scan_params.max_dwell_time_active = duration;
    p2p_handle->p2p_scan_params.min_dwell_time_passive = p2p_handle->p2p_scan_params.max_dwell_time_passive = duration;
    p2p_handle->p2p_scan_params.min_rest_time = p2p_handle->p2p_scan_params.max_rest_time = P2P_REST_TIME;
    p2p_handle->p2p_scan_params.repeat_probe_time = 0;
    p2p_handle->p2p_scan_params.num_channels = 1;
    p2p_handle->p2p_scan_channels[0] =  wlan_mhz2ieee(p2p_handle->p2p_devhandle, freq, 0);

    p2p_handle->p2p_scan_params.chan_list = p2p_handle->p2p_scan_channels;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s num_channels %d chan_1 %d\n", __func__,
                      p2p_handle->p2p_scan_params.num_channels,
                      p2p_handle->p2p_scan_params.num_channels == 0 ? 0 :
                                                            *p2p_handle->p2p_scan_params.chan_list);

    p2p_handle->p2p_req_id = req_id;
    p2p_handle->p2p_listen_freq = freq;
    p2p_handle->p2p_listen_duration = duration;

    retval = ieee80211_p2p_start_scan(p2p_handle,type);
    P2P_DEV_UNLOCK(p2p_handle);

    return retval;
}

/**
 * Stop Listen scans.
 */
void
ieee80211_p2p_stop_listen(wlan_p2p_t p2p_handle, u_int32_t flags)
{
    wlan_if_t vap = p2p_handle->p2p_vap;
    bool remove_pending=false;
    KASSERT(vap != NULL, ("vap not set"));

    /* cancel both pending and in progress scan requests */
    P2P_DEV_LOCK(p2p_handle);

    if ((p2p_handle->p2p_pending_scan_req_type == IEEE80211_P2P_SCAN_REQ_EXTERNAL_LISTEN) ||
        (p2p_handle->p2p_pending_scan_req_type == IEEE80211_P2P_SCAN_REQ_EXTERNAL_SEARCH))
    {
        p2p_handle->p2p_pending_scan_req_type = IEEE80211_P2P_SCAN_REQ_NONE;
        remove_pending=true;
    }

    if ((p2p_handle->p2p_inprogress_scan_req_type == IEEE80211_P2P_SCAN_REQ_EXTERNAL_LISTEN) ||
        (p2p_handle->p2p_inprogress_scan_req_type == IEEE80211_P2P_SCAN_REQ_EXTERNAL_SEARCH))
    {
        P2P_DEV_UNLOCK(p2p_handle);

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s cancel requested for in_progress request req_id %d freq %d duration %d\n",__func__,
                          p2p_handle->p2p_cur_req_id,p2p_handle->p2p_cur_listen_freq,p2p_handle->p2p_cur_listen_duration);
        if (wlan_scan_cancel(vap,
                             p2p_handle->p2p_scan_requestor,
                             IEEE80211_VAP_SCAN,
                             flags) != EOK) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s scan cancel failed\n",__func__);
        }
    } else {
        P2P_DEV_UNLOCK(p2p_handle);
    }
    if (remove_pending) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s removed pending req req_id %d freq %d duration %d\n",__func__,
                          p2p_handle->p2p_req_id,p2p_handle->p2p_listen_freq,p2p_handle->p2p_listen_duration);
    }
}

/**
 * Stop any pending send_action scans (both pending and inprogress).
 */
void
ieee80211_p2p_stop_send_action_scan(wlan_p2p_t p2p_handle)
{
    wlan_if_t vap = p2p_handle->p2p_vap;
    ieee80211_p2p_act_frame *act_frame, *tmp = NULL;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d\n",__func__, __LINE__);

    P2P_DEV_LOCK(p2p_handle);
    /* move any pending action frames into the free pool from sent list as well as pending list */
    TAILQ_FOREACH_SAFE(act_frame, &p2p_handle->p2p_act_frame_head, act_frame_list, tmp) {
        TAILQ_REMOVE(&p2p_handle->p2p_act_frame_head, act_frame, act_frame_list);
        TAILQ_INSERT_HEAD(&p2p_handle->p2p_act_frame_pool, act_frame, act_frame_list);
    }

    TAILQ_FOREACH_SAFE(act_frame, &p2p_handle->p2p_act_frame_sent, act_frame_list, tmp) {
        TAILQ_REMOVE(&p2p_handle->p2p_act_frame_sent, act_frame, act_frame_list);
        TAILQ_INSERT_HEAD(&p2p_handle->p2p_act_frame_pool, act_frame, act_frame_list);
    }
    /* cancel any pending listen requests first */
    if (p2p_handle->p2p_pending_scan_req_type == IEEE80211_P2P_SCAN_REQ_SEND_ACTION) {
        int rc;

        p2p_handle->p2p_pending_scan_req_type = IEEE80211_P2P_SCAN_REQ_NONE;
        P2P_DEV_UNLOCK(p2p_handle);
        rc = wlan_scan_unregister_event_handler(vap,ieee80211_p2p_scan_evhandler,(os_if_t)p2p_handle);
        if (rc != EOK) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                              "%s: wlan_scan_unregister_event_handler "
                              "failed handler=%08p,%08p rc=%08X\n",
                              __func__, ieee80211_p2p_scan_evhandler,
                              p2p_handle, rc);
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s clear pending send_action_scan req\n",__func__);
        return;
    }

    /* if no pending listen requests then cancel inprogress listen request */
    if (p2p_handle->p2p_inprogress_scan_req_type == IEEE80211_P2P_SCAN_REQ_SEND_ACTION) {
        /*
         * CHECK if SCAN_CANCEL_SYNC works on all platforms/OS.
         */
        if (wlan_scan_cancel(vap,
                             p2p_handle->p2p_scan_requestor,
                             IEEE80211_VAP_SCAN,
                             IEEE80211_SCAN_CANCEL_SYNC) != EOK) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s scan cancel failed\n",__func__);
        }
        /*
         * clear the pending flags.
         */
        p2p_handle->p2p_inprogress_scan_req_type = IEEE80211_P2P_SCAN_REQ_NONE;

        /* drain any pending messages */
        OS_MESGQ_DRAIN(&p2p_handle->p2p_mesg_q,NULL);
        P2P_DEV_UNLOCK(p2p_handle);
        return;
    }
    P2P_DEV_UNLOCK(p2p_handle);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s no pending send action scan req\n",__func__);
}



/**
 * stop - Stop any pending p2p operation.
 */
void
ieee80211_p2p_stop(wlan_p2p_t p2p_handle, u_int32_t flags)
{
    wlan_if_t vap = p2p_handle->p2p_vap;
	u_int32_t    scan_flags = IEEE80211_SCAN_CANCEL_ASYNC;
    int          rc;
    ieee80211_p2p_act_frame *act_frame = NULL, *tmptmp = NULL;
    ieee80211_xmit_status ts;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d flags=%08X\n",__func__, __LINE__, flags);
    rc = wlan_scan_unregister_event_handler(vap,ieee80211_p2p_scan_evhandler,(os_if_t)p2p_handle);
    if (rc != EOK) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                          "%s: wlan_scan_unregister_event_handler "
                          "failed handler=%08p,%08p rc=%08X\n",
                          __func__, ieee80211_p2p_scan_evhandler,
                          p2p_handle, rc);
    }

    /*
     * Only mutually exclusive flags WLAN_P2P_STOP_SYNC and WLAN_P2P_STOP_WAIT
     * currently support. 
     * Modify this code if different parameter flags can be OR'ed together.
     */
    switch (flags)
    {
    case WLAN_P2P_STOP_SYNC:
        scan_flags = IEEE80211_SCAN_CANCEL_SYNC;
        break;
        
    case WLAN_P2P_STOP_WAIT:
        scan_flags = IEEE80211_SCAN_CANCEL_ASYNC | IEEE80211_SCAN_CANCEL_WAIT;
        break;
        
    default:        
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s: %d Invalid flags=%08X\n",__func__, __LINE__, flags);
        ASSERT(0);
        break;    
    }

    /*
     * CHECK if SCAN_CANCEL_SYNC works on all platforms/OS.
     */
    p2p_handle->p2p_pending_scan_req_type = IEEE80211_P2P_SCAN_REQ_NONE;
    if (p2p_handle->p2p_inprogress_scan_req_type != IEEE80211_P2P_SCAN_REQ_NONE ) {
        if (wlan_scan_cancel(vap,
                         p2p_handle->p2p_scan_requestor,
                         IEEE80211_VAP_SCAN,
                         scan_flags) != EOK) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s scan cancel failed\n",__func__);
    	}
	}


    P2P_DEV_LOCK(p2p_handle);
    /* clear all pending flags */
    p2p_handle->p2p_inprogress_scan_req_type = IEEE80211_P2P_SCAN_REQ_NONE;

    /* drain any pending messages */
    OS_MESGQ_DRAIN(&p2p_handle->p2p_mesg_q,NULL);

    ts.ts_flags = IEEE80211_TX_XRETRY;
    ts.ts_retries = 10; 
    /* fail all the sent frames and waiting frames */
    TAILQ_FOREACH_SAFE(act_frame, &p2p_handle->p2p_act_frame_sent, act_frame_list, tmptmp) {
       TAILQ_REMOVE(&p2p_handle->p2p_act_frame_sent, act_frame, act_frame_list);
       P2P_DEV_UNLOCK(p2p_handle);
       if (act_frame->frame_complete_handler) {
              act_frame->frame_complete_handler(vap, NULL,
                                                  act_frame->arg, act_frame->dst,
                                                  act_frame->src, act_frame->bssid, &ts);
        }
        P2P_DEV_LOCK(p2p_handle);
        TAILQ_INSERT_HEAD(&p2p_handle->p2p_act_frame_pool, act_frame, act_frame_list);
    }
    TAILQ_FOREACH_SAFE(act_frame, &p2p_handle->p2p_act_frame_head, act_frame_list, tmptmp) {
       TAILQ_REMOVE(&p2p_handle->p2p_act_frame_head, act_frame, act_frame_list);
       P2P_DEV_UNLOCK(p2p_handle);
       if (act_frame->frame_complete_handler) {
              act_frame->frame_complete_handler(vap, NULL,
                                                  act_frame->arg, act_frame->dst,
                                                  act_frame->src, act_frame->bssid, &ts);
        }
        P2P_DEV_LOCK(p2p_handle);
        TAILQ_INSERT_HEAD(&p2p_handle->p2p_act_frame_pool, act_frame, act_frame_list);
    }
    P2P_DEV_UNLOCK(p2p_handle);
}


/**
 * common action frame complete handler.
 * both for internal, external action frames.
 */
static void ieee80211_action_frame_complete_handler(wlan_if_t vaphandle, wbuf_t wbuf, void *arg,
                                                    u_int8_t *dst_addr, u_int8_t *src_addr, u_int8_t *bssid,
                                                   ieee80211_xmit_status *ts)
{
    ieee80211_p2p_act_frame *act_frame = (ieee80211_p2p_act_frame *) arg;
    ieee80211_p2p_act_frame *tmp_act_frame, *tmptmp = NULL;
    wlan_p2p_t p2p_handle;

    p2p_handle = ieee80211vap_get_private_data(vaphandle);

    IEEE80211_DPRINTF(vaphandle, IEEE80211_MSG_ANY, "%s:%d  the action frame with seq # %d completed with success %d\n",
                      __func__, __LINE__, act_frame->p2p_act_frame_seq, (ts->ts_flags == 0));
    /* check if the frame is still in the pending list */
    P2P_DEV_LOCK(p2p_handle);
    TAILQ_FOREACH_SAFE(tmp_act_frame, &p2p_handle->p2p_act_frame_sent, act_frame_list, tmptmp) {
        if (act_frame == tmp_act_frame) {
            TAILQ_REMOVE(&p2p_handle->p2p_act_frame_sent, act_frame, act_frame_list);
            P2P_DEV_UNLOCK(p2p_handle);
            if (act_frame->frame_complete_handler)
                act_frame->frame_complete_handler(vaphandle, wbuf,
                                                  act_frame->arg, dst_addr,
                                                  src_addr, bssid, ts);
            P2P_DEV_LOCK(p2p_handle);
            TAILQ_INSERT_HEAD(&p2p_handle->p2p_act_frame_pool, act_frame, act_frame_list);
            P2P_DEV_UNLOCK(p2p_handle);
            return;
        }
    }
    P2P_DEV_UNLOCK(p2p_handle);
}

/**
 * scan (change channel) to send action frame.
 * called with p2plock held.
 */
static int
ieee80211_p2p_send_action_scan(wlan_p2p_t p2p_handle)
{
    wlan_if_t vap = p2p_handle->p2p_vap;
    ieee80211_p2p_act_frame *act_frame;
    u_int32_t freq;

    act_frame = TAILQ_FIRST(&p2p_handle->p2p_act_frame_head);

    if (act_frame == NULL) {
        /* no action frames to send and no need to scan */
        return 0;
    }

    freq = act_frame->freq;
    /*
     * use scan operation to implement listen.
     * in the listen state the radio needs to be switched to a channel and
     * listen for probe requests and respond to them without sending any probe
     * requests.
     * setup a passive scan so that no probe request will be sent out.
     */
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "LISTEN: %s:%d\n",__func__, __LINE__);
    if (p2p_handle->p2p_pending_scan_req_type != IEEE80211_P2P_SCAN_REQ_NONE) {
        /*
         * received a scan request while one is pending.
         */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s unexpected: received scan request while one of type %d is pending\n",
         __func__,p2p_handle->p2p_pending_scan_req_type);
         return -1;
    }
    wlan_set_default_scan_parameters(vap,&p2p_handle->p2p_scan_params,IEEE80211_M_STA,true,true,true,true,0,NULL,0);

    p2p_handle->p2p_scan_params.type = IEEE80211_SCAN_FOREGROUND;
    p2p_handle->p2p_scan_params.flags = IEEE80211_SCAN_FORCED | IEEE80211_SCAN_ALLBANDS | IEEE80211_SCAN_PASSIVE;
    p2p_handle->p2p_scan_params.flags |= IEEE80211_SCAN_ADD_BCAST_PROBE;
    p2p_handle->p2p_scan_params.min_dwell_time_active = p2p_handle->p2p_scan_params.max_dwell_time_active = act_frame->dwell_time;
    p2p_handle->p2p_scan_params.min_dwell_time_passive = p2p_handle->p2p_scan_params.max_dwell_time_passive = act_frame->dwell_time;
    p2p_handle->p2p_scan_params.min_rest_time = p2p_handle->p2p_scan_params.max_rest_time = P2P_REST_TIME;
    p2p_handle->p2p_scan_params.repeat_probe_time = 0;
    p2p_handle->p2p_scan_params.num_channels = 1;
    p2p_handle->p2p_scan_channels[0] =  wlan_mhz2ieee(p2p_handle->p2p_devhandle, freq, 0);

    p2p_handle->p2p_scan_params.chan_list = p2p_handle->p2p_scan_channels;

    wlan_scan_register_event_handler(vap,ieee80211_p2p_scan_evhandler,(os_if_t)p2p_handle);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s num_channels %d chan_1 %d\n", __func__,
                      p2p_handle->p2p_scan_params.num_channels, 
                      p2p_handle->p2p_scan_params.num_channels == 0 ? 0 : *p2p_handle->p2p_scan_params.chan_list);

    return ieee80211_p2p_start_scan(p2p_handle,IEEE80211_P2P_SCAN_REQ_SEND_ACTION);
}

/*
 * Send a probe response frame.
 * NB: for probe response, the node may not represent the peer STA.
 * We could use BSS node to reduce the memory usage from temporary node.
 */
static int
ieee80211_p2p_send_proberesp(struct ieee80211_node *ni, u_int8_t *macaddr)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic = ni->ni_ic;
    wbuf_t wbuf;
    struct ieee80211_frame *wh;
    u_int8_t *frm;
    u_int16_t capinfo;

    wbuf = wbuf_alloc(ic->ic_osdev, WBUF_TX_MGMT, MAX_TX_RX_PACKET_SIZE);
    if (wbuf == NULL) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_P2P_PROT,
                          "%s: Error: unable to alloc wbuf of type WBUF_TX_MGMT.\n",
                           __FUNCTION__);
        return -ENOMEM;
    }

    if (!IEEE80211_ADDR_EQ(vap->iv_bss->ni_bssid, vap->iv_myaddr)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_P2P_PROT,
                          "%s: Warning: p2p device BSSID is not my address.\n",
                           __FUNCTION__);
    }

    /* setup the wireless header */
    wh = (struct ieee80211_frame *)wbuf_header(wbuf);
    ieee80211_send_setup(vap, ni, wh,
                         IEEE80211_FC0_TYPE_MGT | IEEE80211_FC0_SUBTYPE_PROBE_RESP,
                         vap->iv_myaddr, macaddr,
                         ieee80211_node_get_bssid(ni));
    frm = (u_int8_t *)&wh[1];

    /*
     * probe response frame format
     *  [8] time stamp
     *  [2] beacon interval
     *  [2] cabability information
     *  [tlv] ssid
     *  [tlv] supported rates
     *  [tlv] parameter set (FH/DS)
     *  [tlv] parameter set (IBSS)
     *  [tlv] extended rate phy (ERP)
     *  [tlv] extended supported rates
     *  [tlv] country (if present)
     *  [3] power constraint
     *  [tlv] WPA
     *  [tlv] WME
     *  [tlv] HT Capabilities
     *  [tlv] HT Information
     *      [tlv] Atheros Advanced Capabilities
     */
    OS_MEMZERO(frm, 8);  /* timestamp should be filled later */
    frm += 8;
    *(u_int16_t *)frm = htole16(vap->iv_bss->ni_intval);
    frm += 2;

    capinfo = 0; /* no IBSS, ESS capability bits according to the P2P spec */
  
    if (IEEE80211_VAP_IS_PRIVACY_ENABLED(vap))
        capinfo |= IEEE80211_CAPINFO_PRIVACY;
    if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) &&
        IEEE80211_IS_CHAN_2GHZ(ic->ic_curchan))
        capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
    if (ic->ic_flags & IEEE80211_F_SHSLOT)
        capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
    if (ieee80211_ic_doth_is_set(ic) && ieee80211_vap_doth_is_set(vap))
        capinfo |= IEEE80211_CAPINFO_SPECTRUM_MGMT;
    *(u_int16_t *)frm = htole16(capinfo);
    frm += 2;
    
    if ((vap->iv_bss->ni_esslen != IEEE80211_P2P_WILDCARD_SSID_LEN) ||
        (OS_MEMCMP(vap->iv_bss->ni_essid, IEEE80211_P2P_WILDCARD_SSID, IEEE80211_P2P_WILDCARD_SSID_LEN) != 0))
    {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_P2P_PROT,
                          "%s: Warning: p2p device SSID is not p2p wildcard. len=%d.\n",
                           __FUNCTION__, vap->iv_bss->ni_esslen);
    }
    frm = ieee80211_add_ssid(frm, vap->iv_bss->ni_essid,
                             vap->iv_bss->ni_esslen);

    frm = ieee80211_add_rates(frm, &vap->iv_op_rates[IEEE80211_MODE_11G]);

    if (!IEEE80211_IS_CHAN_FHSS(ic->ic_curchan)) {
        *frm++ = IEEE80211_ELEMID_DSPARMS;
        *frm++ = 1;
        *frm++ = ieee80211_chan2ieee(ic, ic->ic_curchan);
    }
    
    if (IEEE80211_IS_COUNTRYIE_ENABLED(ic) && ieee80211_vap_country_ie_is_set(vap)) {
        frm = ieee80211_add_country(frm, vap);
    }

    if (IEEE80211_IS_CHAN_ANYG(ic->ic_curchan) || 
        IEEE80211_IS_CHAN_11NG(ic->ic_curchan) ||
        IEEE80211_IS_CHAN_11AXG(ic->ic_curchan) ) {
        frm = ieee80211_add_erp(frm, ic);
    }

    frm = ieee80211_add_xrates(frm,  &vap->iv_op_rates[IEEE80211_MODE_11G]);

    IEEE80211_VAP_LOCK(vap);
    if (vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBERESP].length) {
        OS_MEMCPY(frm, vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBERESP].ie,
                  vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBERESP].length);
        frm += vap->iv_app_ie[IEEE80211_FRAME_TYPE_PROBERESP].length;
    }
    /* Add the Application IE's */
    frm = ieee80211_mlme_app_ie_append(vap, IEEE80211_FRAME_TYPE_PROBERESP, frm);
    IEEE80211_VAP_UNLOCK(vap);
    
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t *)wbuf_header(wbuf)));

    return ieee80211_send_mgmt(vap,ni, wbuf,true);
}

/**
 * creates an instance of p2p object .
 * @param dev_handle    : handle to the radio .
 * @param bssid         : MAC address that was previously allocated.
 *
 * @return  on success return an opaque handle to newly created p2p object.
 *          on failure returns NULL.
 */
wlan_p2p_t
wlan_p2p_create(wlan_dev_t dev_handle, wlan_p2p_params *p2p_params)
{
    wlan_p2p_t p2p_handle;
    osdev_t oshandle;
    u_int32_t i;
    ieee80211_p2p_act_frame *act_frame;
    ieee80211_ssid ssid;
    int error;
    u_int8_t module_name[]="P2P_DEVICE_UMAC";
    u_int8_t *prealloc_mac= NULL;
    int flags = (IEEE80211_CLONE_BSSID | IEEE80211_P2PDEV_VAP);

    /* Check that P2P is supported */
    if (wlan_get_device_param(dev_handle, IEEE80211_DEVICE_P2P) == 0) {
        ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
            "%s: Error: P2P unsupported.\n", __func__);
        return NULL;
    }

    oshandle = ieee80211com_get_oshandle(dev_handle);
    p2p_handle = (wlan_p2p_t) OS_MALLOC(oshandle,
                  sizeof(struct ieee80211p2p) + 4 /*padding */
                  + sizeof(ieee80211_p2p_act_frame)*IEEE80211_P2P_MAX_ACTION_FRAME, 0);
    if (p2p_handle == NULL) {
        ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
            "%s p2p device allocation failed\n", __func__);
        return NULL;
    }
    OS_MEMZERO(p2p_handle, sizeof(struct ieee80211p2p) + 4 + sizeof(ieee80211_p2p_act_frame)*IEEE80211_P2P_MAX_ACTION_FRAME);

    if (p2p_params && IEEE80211_ADDR_IS_VALID(p2p_params->dev_id)) {
        /* Use device id mentioned */
        prealloc_mac = p2p_params->dev_id;
        flags &= ~IEEE80211_CLONE_BSSID;
    }

    p2p_handle->p2p_vap = wlan_vap_create(dev_handle,
                                          IEEE80211_M_HOSTAP,
                                          DEF_VAP_SCAN_PRI_MAP_OPMODE_AP_P2P_DEVICE,
                                          flags,
                                          prealloc_mac, NULL, NULL);
    if (p2p_handle->p2p_vap == NULL) {
        ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
            "%s p2p vap creation failed\n", __func__);
        OS_FREE(p2p_handle);
        return NULL;
    }

    p2p_handle->p2p_devhandle = dev_handle;
    p2p_handle->p2p_oshandle = oshandle;

    ssid.len = IEEE80211_P2P_WILDCARD_SSID_LEN;
    memcpy(ssid.ssid, IEEE80211_P2P_WILDCARD_SSID, IEEE80211_P2P_WILDCARD_SSID_LEN);
    if (wlan_set_desired_ssidlist(p2p_handle->p2p_vap, 1, &ssid)) {
        ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
            "%s setting SSID failed\n", __func__);
        return NULL;
    }

    wlan_set_desired_phymode(p2p_handle->p2p_vap, IEEE80211_MODE_11G);

     ieee80211_p2p_setup_rates(p2p_handle->p2p_vap); 

    error = ieee80211_create_infra_bss(p2p_handle->p2p_vap,(const u_int8_t *)IEEE80211_P2P_WILDCARD_SSID, IEEE80211_P2P_WILDCARD_SSID_LEN);

    if (error) {
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s create infra bss failed\n", __func__);
        wlan_vap_delete(p2p_handle->p2p_vap);
        OS_FREE(p2p_handle);
        return NULL;
    }

    /* set the managemet frame rate to 6M */
    wlan_set_param(p2p_handle->p2p_vap,IEEE80211_MGMT_RATE, IEEE80211_P2P_MIN_RATE);

    if (p2p_params && (p2p_params->dev_name[0] != '\0')) {
        OS_MEMCPY(p2p_handle->p2p_dev_name, p2p_params->dev_name, IEEE80211_MAX_P2P_DEV_NAME_LEN);
        /* ensure that p2p_dev_name is null-terminated */
        p2p_handle->p2p_dev_name[IEEE80211_MAX_P2P_DEV_NAME_LEN-1] = '\0';
    } else {
        char *tmp = p2p_handle->p2p_dev_name;
        char *tmp_buf = tmp;
        u_int8_t *mac = wlan_vap_get_macaddr(p2p_handle->p2p_vap);

        tmp = tmp + snprintf(tmp,IEEE80211_MAX_P2P_DEV_NAME_LEN, "p2p-");
        tmp = tmp + snprintf(tmp,(IEEE80211_MAX_P2P_DEV_NAME_LEN - (tmp-tmp_buf)), "%s", ether_sprintf(mac));
    }

    if (OS_MESGQ_INIT(oshandle, &p2p_handle->p2p_mesg_q, IEEE80211_P2P_MAX_EVENT_LEN,IEEE80211_P2P_MAX_EVENT_QUEUE_DEPTH,
              ieee80211_p2p_mesgq_event_handler, p2p_handle, MESGQ_PRIORITY_NORMAL, MESGQ_ASYNCHRONOUS_EVENT_DELIVERY) != 0) {
        printk("HSM: %s : OS_MESGQ_INIT failed\n",__func__);

        wlan_vap_delete(p2p_handle->p2p_vap);
        OS_FREE(p2p_handle);

        return NULL;
    }

    /* Set the primary and secondary device types (if any) */
    if (p2p_params) {
        if (is_dev_type_cat_id_valid(p2p_params->pri_dev_type)) {
            OS_MEMCPY(p2p_handle->pri_dev_type, p2p_params->pri_dev_type, IEEE80211_P2P_DEVICE_TYPE_LEN);

            ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
                "%s: Primary Device Type: cat=%x %x, subcat=%x %x, oui=%x %x %x %x\n",
                __func__,p2p_handle->pri_dev_type[0], p2p_handle->pri_dev_type[1],
                p2p_handle->pri_dev_type[6], p2p_handle->pri_dev_type[7], 
                p2p_handle->pri_dev_type[2], p2p_handle->pri_dev_type[3], 
                p2p_handle->pri_dev_type[4], p2p_handle->pri_dev_type[5]);
        }

        /* check if there is secondary device types */
        if (is_dev_type_cat_id_valid(p2p_params->sec_dev_type[0])) {
            /* Allocate the memory. For now, we alloc the max. */
            u_int8_t    *sec_dev_type_list;

            sec_dev_type_list = (u_int8_t *) OS_MALLOC(oshandle, 
                                                       (IEEE80211_P2P_DEVICE_TYPE_LEN * IEEE80211_P2P_SEC_DEVICE_TYPES),
                                                       0);
            if (sec_dev_type_list == NULL) {
                ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
                                  "%s p2p secondary device type list allocation"
                                  " failed. size=%d\n", __func__,
                                  (IEEE80211_P2P_DEVICE_TYPE_LEN * IEEE80211_P2P_SEC_DEVICE_TYPES));

                wlan_vap_delete(p2p_handle->p2p_vap);
                OS_FREE(p2p_handle);
                return NULL;
            }
            OS_MEMZERO(sec_dev_type_list, (IEEE80211_P2P_DEVICE_TYPE_LEN * IEEE80211_P2P_SEC_DEVICE_TYPES));

            p2p_handle->sec_dev_type = sec_dev_type_list;
            p2p_handle->num_sec_dev_types = 0;
            while (is_dev_type_cat_id_valid(p2p_params->sec_dev_type[p2p_handle->num_sec_dev_types]))
            {
                OS_MEMCPY(sec_dev_type_list,
                          p2p_params->sec_dev_type[p2p_handle->num_sec_dev_types],
                          IEEE80211_P2P_DEVICE_TYPE_LEN);

                ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
                    "%s: Secondary Device Type[%d]: cat=%x %x, subcat=%x %x,"
                    "oui=%x %x %x %x\n",
                    __func__, p2p_handle->num_sec_dev_types,
                    sec_dev_type_list[0], sec_dev_type_list[1],
                    sec_dev_type_list[6], sec_dev_type_list[7], 
                    sec_dev_type_list[2], sec_dev_type_list[3], 
                    sec_dev_type_list[4], sec_dev_type_list[5]);

                p2p_handle->num_sec_dev_types++;
                sec_dev_type_list += IEEE80211_P2P_DEVICE_TYPE_LEN;
            }
        }
    }

    spin_lock_init(&p2p_handle->p2p_lock);
    ieee80211vap_set_private_data(p2p_handle->p2p_vap,(void *) p2p_handle);

    (void) ieee80211vap_set_input_mgmt_filter(p2p_handle->p2p_vap,ieee80211_p2p_input_filter_mgmt);
    (void) ieee80211vap_set_output_mgmt_filter_func(p2p_handle->p2p_vap,ieee80211_p2p_output_filter);

    if (wlan_scan_get_requestor_id(p2p_handle->p2p_vap, module_name,
                                    &p2p_handle->p2p_scan_requestor) != EOK) {
        printk("HSM: %s : wlan_scan_get_requestor_id failed\n",__func__);
        if (p2p_handle->sec_dev_type) {
            OS_FREE(p2p_handle->sec_dev_type);
        }
        wlan_vap_delete(p2p_handle->p2p_vap);
        OS_FREE(p2p_handle);
        return NULL;
    }


    TAILQ_INIT(&p2p_handle->p2p_act_frame_pool);
    TAILQ_INIT(&p2p_handle->p2p_act_frame_head);
    TAILQ_INIT(&p2p_handle->p2p_act_frame_sent);
    /* construct action frame pool */
    act_frame = (ieee80211_p2p_act_frame *)&p2p_handle[1];
    for (i=0;i<IEEE80211_P2P_MAX_ACTION_FRAME;++i) {
        TAILQ_INSERT_TAIL(&p2p_handle->p2p_act_frame_pool, act_frame, act_frame_list);
        act_frame++;
    }

    OS_MEMZERO(&p2p_handle->p2p_seq_tracker, sizeof(p2p_handle->p2p_seq_tracker));
    return p2p_handle;
}

/**
 * Called at the start of a reset
 * @param p2p_handle : handle to the p2p object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
void
wlan_p2p_reset_start(wlan_p2p_t p2p_handle)
{
}

/**
 * Called at the end of a reset
 * @param p2p_handle : handle to the p2p object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
void
wlan_p2p_reset_end(wlan_p2p_t p2p_handle)
{
    int error = 0;

    /* Note: during reset, the vap->iv_bss is re-initialized and
     * certain settings are lost. We need to restore them. */

    error = ieee80211_create_infra_bss(p2p_handle->p2p_vap,(const u_int8_t *)IEEE80211_P2P_WILDCARD_SSID, IEEE80211_P2P_WILDCARD_SSID_LEN);

    if (error) {
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY, "%s: ERROR: create infra bss failed.\n", __func__);
    }
}

/**
 * delete p2p object.
 * @param p2p_handle : handle to the p2p object .
 *
 * @return on success returns 0.
 *         on failure returns a negative value.
 */
int
wlan_p2p_delete(wlan_p2p_t p2p_handle)
{
    int rc;

    rc = wlan_scan_unregister_event_handler(p2p_handle->p2p_vap,ieee80211_p2p_scan_evhandler,(os_if_t)p2p_handle);
    if (rc != EOK) {
        IEEE80211_DPRINTF(p2p_handle->p2p_vap, IEEE80211_MSG_ANY,
                          "%s: wlan_scan_unregister_event_handler "
                          "failed handler=%08p,%08p rc=%08X\n",
                          __func__, ieee80211_p2p_scan_evhandler,
                          p2p_handle, rc);
    }
    OS_MESGQ_DESTROY(&p2p_handle->p2p_mesg_q);

    wlan_scan_clear_requestor_id(p2p_handle->p2p_vap,p2p_handle->p2p_scan_requestor);


    spin_lock_destroy(&p2p_handle->p2p_lock);   

    wlan_vap_delete(p2p_handle->p2p_vap);

    if (p2p_handle->sec_dev_type) {
        OS_FREE(p2p_handle->sec_dev_type);
    }

    OS_FREE(p2p_handle);
    return 0;
}


/**
 * set simple p2p configuration parameter.
 *
 *  @param p2p_handle     : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @param val           : value of the parameter.
 *  @return 0  on success and -ve on failure.
 */
int
wlan_p2p_set_param(wlan_p2p_t p2p_handle, wlan_p2p_param param, u_int32_t val)
{
   switch(param) {
      case WLAN_P2P_PROTECTION_MODE:
          p2p_handle->p2p_protection_mode = val;
          break;
      default:
         break;
   }
    return 0;
}

/**
 * get simple p2p configuration parameter.
 *
 *  @param p2p_handle     : handle to the vap.
 *  @param param         : simple config paramaeter.
 *  @return value of the parameter.
 */
u_int32_t
wlan_p2p_get_param(wlan_p2p_t p2p_handle, wlan_p2p_param param)
{
    return 0;
}


/**
 * register  event handler with p2p object.
 * @param p2p_handle        : handle to the p2p object .
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
wlan_p2p_register_event_handlers(wlan_p2p_t p2p_handle,
                                 void *event_arg,
                                 wlan_p2p_event_handler handler)
{
    P2P_DEV_LOCK(p2p_handle);
    if (handler && p2p_handle->event_handler) {
       if (handler != p2p_handle->event_handler) {
        P2P_DEV_UNLOCK(p2p_handle);
           return -EINVAL;
       }
    }
    /* needs synchronization */
    p2p_handle->event_handler = handler;
    p2p_handle->event_arg = event_arg;
    P2P_DEV_UNLOCK(p2p_handle);
    return 0;
}


/**
 * return vaphandle associated with underlying p2p device.
 * @param p2p_handle         : handle to the p2p object .
 * @return vaphandle.
 */
wlan_if_t
wlan_p2p_get_vap_handle(wlan_p2p_t p2p_handle)
{
    return (p2p_handle->p2p_vap);
}


/**
 * request to switch to a chnnel and remain on channel for certain duration.
 * @param p2p_handle     : handle to the p2p device object.
 * @param in_listen_state : whether in listen state for this set channel.
 * @param freq     :  channel to remain on in mhz
 * @param  req_id       : id of the request  (passed back to the event handler to match request/response )
 * @param  channel_time :  time to remain on channel in msec.
 *  The driver asynchronously switches to the requested channel at an appropriate time
 *  and sends event WLAN_P2PDEV_CHAN_START. the driver keeps the radio on the channel
 *  for channel_time (using a timer) , then switches away from the channel and sends
 *  WLAN_P2PDEV_CHAN_END event with reason WLAN_P2PDEV_CHAN_END_REASON_TIMEOUT,
 * @return EOK if successful .
 *
 */
int
wlan_p2p_set_channel(wlan_p2p_t  p2p_handle, bool in_listen_state, u_int32_t freq,
                     u_int32_t req_id, u_int32_t channel_time,
                     IEEE80211_SCAN_PRIORITY scan_priority)
{
    if (in_listen_state) {
        return ieee80211_p2p_start_listen(p2p_handle, freq, req_id,
                                          channel_time, IEEE80211_P2P_SCAN_REQ_EXTERNAL_LISTEN, scan_priority);
    }
    else {
        return ieee80211_p2p_start_listen(p2p_handle, freq, req_id,
                                          channel_time, IEEE80211_P2P_SCAN_REQ_EXTERNAL_SEARCH, scan_priority);
    }
}

/**
 * cancel current remain on channel .
 * the driver switches away the channel and sends
 * WLAN_P2PDEV_CHAN_END event with reason WLAN_P2PDEV_CHAN_END_REASON_CANCEL.
 */
int
wlan_p2p_cancel_channel(wlan_p2p_t  p2p_handle, u_int32_t flags)
{
    ieee80211_p2p_stop_listen(p2p_handle, flags);
    return 0;
}

/**
 * start a p2p scan . 
 *  @param p2p_handle             : handle to the p2p device object.
 *  @param params                : scan params.
 *
 * @return:
 *   internally uses the SCAN module to start the scan request. if the scan module is busy
 *  the p2p module will queue the request and issue it when the scan module is ready.
 *  on success returns 0. 
 *  on failure returns a negative value.
 *  if succesful
 *  at the end of scan WLAN_P2PDEV_SCAN_END event is delivered via the event handler. 
 */
int wlan_p2p_scan(wlan_p2p_t             p2p_handle,
                  ieee80211_scan_params *params) 
{
    wlan_if_t vap = p2p_handle->p2p_vap;
    int retval;

    P2P_DEV_LOCK(p2p_handle);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s\n",__func__);
    if (p2p_handle->p2p_pending_scan_req_type != IEEE80211_P2P_SCAN_REQ_NONE) {
        /*
         * received a scan request while one is pending.
         */
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s unexpected: received scan request while one of type %d is pending\n",
                          __func__,p2p_handle->p2p_pending_scan_req_type);
        P2P_DEV_UNLOCK(p2p_handle);
        return -1;
    }

    
    OS_MEMCPY(&p2p_handle->p2p_scan_params,params, sizeof(ieee80211_scan_params));

    wlan_scan_register_event_handler(vap,ieee80211_p2p_scan_evhandler,(os_if_t)p2p_handle);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s num_channels %d chan_1 %d\n", __func__,
                      p2p_handle->p2p_scan_params.num_channels, p2p_handle->p2p_scan_params.num_channels==0?0:*p2p_handle->p2p_scan_params.chan_list);

    retval = ieee80211_p2p_start_scan(p2p_handle,IEEE80211_P2P_SCAN_REQ_EXTERNAL_SCAN);
    P2P_DEV_UNLOCK(p2p_handle);
    return retval;
}

/**
* send action management frame.
 *@param p2p_handle: handle to the p2p device object.
* @param freq     : channel to send on (only to validate/match with current channel)
* @param arg      : arg (will be used in the mlme_action_send_complete)
* @param dst_addr : destination mac address
* @param src_addr : source address.(most of the cases vap mac address).
* @param bssid    : BSSID or %NULL to use default
* @param data     : includes total payload of the action management frame.
* @param data_len : data len.
* @returns 0 if succesful and -ve if failed.
* if the radio is not on the passedf in freq then it will return an error.
* if returns 0 then mlme_action_send_complete will be called with the status of
* the frame transmission.
*/
int
wlan_p2p_send_action_frame(wlan_p2p_t p2p_handle, u_int32_t freq, u_int32_t dwell_time,
                               wlan_action_frame_complete_handler handler, void *arg,
                               const u_int8_t *dst_addr, const u_int8_t *src_addr,
                               const u_int8_t *bssid, const u_int8_t *data,
                               u_int32_t data_len)
{
    wlan_if_t vap = p2p_handle->p2p_vap;
    bool pending_action_frame=false;
    ieee80211_p2p_act_frame *act_frame, *tmp_act_frame, *tmptmp = NULL;
    bool found=false, queue_frame=true;
    ieee80211_scan_info info;
    int minTime     = 8;  // Fudge factor
    int hasTimeLeft = TRUE;
    int scanTimeLeft= 0;
 
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d\n",__func__, __LINE__);
    if (data_len >= IEEE80211_P2P_MAX_BUF_LEN) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d  buffer is too big\n",__func__, __LINE__);
        return -1;
    }
    P2P_DEV_LOCK(p2p_handle);
    p2p_handle->p2p_act_frame_seq++;
    act_frame = TAILQ_FIRST(&p2p_handle->p2p_act_frame_pool);
    if (act_frame) {
        TAILQ_REMOVE(&p2p_handle->p2p_act_frame_pool, act_frame, act_frame_list);
        act_frame->freq = freq;
	    /* sanity check the dwell time*/
#if P2P_MIN_DWELL_TIME == 0
        if ( (dwell_time == 0) || (dwell_time > P2P_MAX_DWELL_TIME)) {
#else
        if ((dwell_time == 0) || (dwell_time < P2P_MIN_DWELL_TIME) || (dwell_time > P2P_MAX_DWELL_TIME)) {
#endif
            dwell_time = P2P_SEND_ACTION_DWELL_TIME;
        } 

        act_frame->dwell_time = dwell_time;
        OS_MEMCPY(act_frame->dst, dst_addr, IEEE80211_ADDR_LEN);
        OS_MEMCPY(act_frame->src, src_addr, IEEE80211_ADDR_LEN);
        OS_MEMCPY(act_frame->bssid, bssid, IEEE80211_ADDR_LEN);
        OS_MEMCPY(act_frame->buf, data, data_len);
        act_frame->len = data_len;
        act_frame->frame_complete_handler = handler;
        act_frame->arg = arg;
        act_frame->p2p_act_frame_seq = p2p_handle->p2p_act_frame_seq;
    } else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d no more actframe buffers\n",__func__, __LINE__);
        P2P_DEV_UNLOCK(p2p_handle);
        return -1;
    }

    TAILQ_INSERT_HEAD(&p2p_handle->p2p_act_frame_sent, act_frame,act_frame_list);

    if (wlan_get_last_scan_info(vap, &info) == EOK) {
        scanTimeLeft = info.min_dwell_time_active - (CONVERT_SYSTEM_TIME_TO_MS( OS_GET_TIMESTAMP() - info.scan_start_time));
        hasTimeLeft = /* How much scan time left */ ((scanTimeLeft - dwell_time) < minTime) ? TRUE : FALSE;
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d send_action seq #%d chan %d dst %s dwell time %d, "
                      "hasTimeLeft[%d] scanTimeLeft[%u] min dt[%u] sst[%u]\n", __func__, __LINE__,
                      act_frame->p2p_act_frame_seq, wlan_mhz2ieee(p2p_handle->p2p_devhandle, freq, 0),
                      ether_sprintf(dst_addr), act_frame->dwell_time, hasTimeLeft, scanTimeLeft,
                      info.min_dwell_time_active, info.scan_start_time);
   /* 
    * if a scan in progress and if still has time left then send the action frame 
    * if no scan in progress and no scheduler and atleast one vap is active then 
    * try to send the frame immediately.
    */
#if 0
    if ( (info.in_progress && hasTimeLeft) || 
          ( !ieee80211_resmgr_oc_scheduler_is_active(vap->iv_ic->ic_resmgr) &&  ieee80211_vaps_active(vap->iv_ic) ) ) {
            
        if ( wlan_vap_send_action_frame(vap, freq, ieee80211_action_frame_complete_handler,
                                   act_frame, dst_addr, src_addr, bssid, data, data_len) == EOK) {
           queue_frame=false;
        } 
    }
#endif

    if (queue_frame) {

        /* check if it already moved to free pool. this can happen if the vap_send_action_frame
           completes the frame with error synchronously */
        TAILQ_FOREACH_SAFE(tmp_act_frame, &p2p_handle->p2p_act_frame_sent, act_frame_list, tmptmp) {
            if (act_frame == tmp_act_frame) {
                TAILQ_REMOVE(&p2p_handle->p2p_act_frame_sent, act_frame,act_frame_list);
                found=true;
            }
        }

        /* if not found, then  it is in the free list */
        if (!found) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d WARNING send_action failed synchronously\n",__func__, __LINE__);
            TAILQ_REMOVE(&p2p_handle->p2p_act_frame_pool, act_frame,act_frame_list);
        }
        /* check if there is a pending action frame */
        if (TAILQ_FIRST(&p2p_handle->p2p_act_frame_head) != NULL) {
            pending_action_frame=true;
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d queue the action frame with seq # %d\n",
              __func__, __LINE__, act_frame->p2p_act_frame_seq);
        TAILQ_INSERT_HEAD(&p2p_handle->p2p_act_frame_head, act_frame,act_frame_list);
        /* start the scan to send action frame if this is the first action frame */
        if (!pending_action_frame) {
            ieee80211_p2p_send_action_scan(p2p_handle);
        }
    } else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY, "%s:%d the action frame with seq # %d sent\n",
              __func__, __LINE__, act_frame->p2p_act_frame_seq);
    }
    P2P_DEV_UNLOCK(p2p_handle);

    return 0;

}
void wlan_p2p_stop(wlan_p2p_t p2p_handle, u_int32_t flags) 
{
    /* stop the p2p device */
    ieee80211_p2p_stop( p2p_handle, flags);
}

/*
 * Set the primary device type
 */
int
wlan_p2p_set_primary_dev_type(wlan_p2p_t p2p_handle, u_int8_t *pri_dev_type)
{
    wlan_dev_t dev_handle = p2p_handle->p2p_devhandle;

    if (is_dev_type_cat_id_valid(pri_dev_type)) {
        P2P_DEV_LOCK(p2p_handle);
        OS_MEMCPY(p2p_handle->pri_dev_type, pri_dev_type, IEEE80211_P2P_DEVICE_TYPE_LEN);
        P2P_DEV_UNLOCK(p2p_handle);

        ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
            "%s: Primary Device Type: cat=%x %x, subcat=%x %x, oui=%x %x %x %x\n",
            __func__,p2p_handle->pri_dev_type[0], p2p_handle->pri_dev_type[1],
            p2p_handle->pri_dev_type[6], p2p_handle->pri_dev_type[7], 
            p2p_handle->pri_dev_type[2], p2p_handle->pri_dev_type[3], 
            p2p_handle->pri_dev_type[4], p2p_handle->pri_dev_type[5]);

        return EOK;
    }
    else {
        ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT, "%s: ERROR: invalid"
            "primary Device Type: cat=%x %x, subcat=%x %x, oui=%x %x %x %x\n",
            __func__, pri_dev_type[0], pri_dev_type[1],
            pri_dev_type[6], pri_dev_type[7], 
            pri_dev_type[2], pri_dev_type[3], 
            pri_dev_type[4], pri_dev_type[5]);

        return -EINVAL;
    }
}

/*
 * Set the secondary device type(s)
 */
int
wlan_p2p_set_secondary_dev_type(wlan_p2p_t p2p_handle, int num_dev_types, u_int8_t *sec_dev_type)
{
    wlan_dev_t dev_handle = p2p_handle->p2p_devhandle;

    /* check if there is a valid secondary device types */
    if (is_dev_type_cat_id_valid(sec_dev_type)) {
        /* Allocate the memory. */
        u_int8_t    *sec_dev_type_list;
        int         i;
 
        sec_dev_type_list = (u_int8_t *) OS_MALLOC(p2p_handle->p2p_oshandle,
                                                   (IEEE80211_P2P_DEVICE_TYPE_LEN * num_dev_types),
                                                   0);
        if (sec_dev_type_list == NULL) {
            ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
                 "%s p2p secondary device type list allocation failed. size=%d\n",
                  __func__, (IEEE80211_P2P_DEVICE_TYPE_LEN * num_dev_types));

            return -ENOMEM;
        }
        OS_MEMZERO(sec_dev_type_list, (IEEE80211_P2P_DEVICE_TYPE_LEN * num_dev_types));

        P2P_DEV_LOCK(p2p_handle);
        if (p2p_handle->sec_dev_type) {
            /* Free the old list */
            OS_FREE(p2p_handle->sec_dev_type);
        }
        p2p_handle->sec_dev_type = sec_dev_type_list;
        for (i = 0; i < num_dev_types; i++)
        {
            OS_MEMCPY(sec_dev_type_list,
                      sec_dev_type,
                      IEEE80211_P2P_DEVICE_TYPE_LEN);

            ASSERT(is_dev_type_cat_id_valid(sec_dev_type_list));

            ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
                "%s: Secondary Device Type[%d]: cat=%x %x, subcat=%x %x,"
                " oui=%x %x %x %x\n", __func__, i,
                sec_dev_type_list[0], sec_dev_type_list[1],
                sec_dev_type_list[6], sec_dev_type_list[7], 
                sec_dev_type_list[2], sec_dev_type_list[3], 
                sec_dev_type_list[4], sec_dev_type_list[5]);


            sec_dev_type += IEEE80211_P2P_DEVICE_TYPE_LEN;
            sec_dev_type_list += IEEE80211_P2P_DEVICE_TYPE_LEN;
        }
        p2p_handle->num_sec_dev_types = num_dev_types;

        P2P_DEV_UNLOCK(p2p_handle);

        return EOK;
    }
    else {
        ieee80211com_note(dev_handle, IEEE80211_MSG_P2P_PROT,
            "%s: ERROR: invalid first secondary Device Type: "
            "cat=%x %x, subcat=%x %x, oui=%x %x %x %x\n",
            __func__, sec_dev_type[0], sec_dev_type[1],
            sec_dev_type[6], sec_dev_type[7], 
            sec_dev_type[2], sec_dev_type[3], 
            sec_dev_type[4], sec_dev_type[5]);

        return -EINVAL;
    }
}

/**
 *parse wps ie for a given ie buffer..
 * 
 * @param vaphandle     : handle to the vap.
 * @param ftype         : frame type.   
 * @param buf           : appie buffer.
 * @param buflen        : length of the buffer.
 * @return  0  on success and -ve on failure.
 */
int wlan_p2p_parse_appie(wlan_p2p_t p2p_handle, ieee80211_frame_type ftype, u_int8_t *buf, u_int16_t buflen)
{
    int rc=0;
    struct p2p_parsed_ie pie;
    
    if (ftype != IEEE80211_FRAME_TYPE_PROBERESP) {
        return EINVAL;
    }

    if (buflen == 0) {
        p2p_handle->p2p_device_address_valid = 0;
        p2p_handle->wps_primary_device_type_valid = 0;

        return rc;
    }

    OS_MEMZERO(&pie, sizeof(pie));
    rc = ieee80211_p2p_parse_ies(p2p_handle->p2p_oshandle, p2p_handle->p2p_vap, buf, buflen, &pie);

    /* save device address for error checking */
    if(!rc && pie.p2p_device_addr){
        OS_MEMCPY(p2p_handle->p2p_device_address, pie.p2p_device_addr, IEEE80211_ADDR_LEN);
        p2p_handle->p2p_device_address_valid = 1;
    }else
        p2p_handle->p2p_device_address_valid = 0;

    /* save primary_dev_type for error checking */
    if(!rc && pie.wps_attributes){
        OS_MEMCPY(p2p_handle->wps_primary_device_type, pie.wps_attributes_parsed.primary_dev_type, WPS_DEV_TYPE_LEN);
        p2p_handle->wps_primary_device_type_valid = 1;
    }else
        p2p_handle->wps_primary_device_type_valid = 0;
    ieee80211_p2p_parse_free(&pie);

    return rc;

}
#endif
