/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2011, Atheros Communications
 *
 * This file contains the common utility routines for P2P Protocol module.
 *
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#include "ieee80211_p2p_device_priv.h"
#include "ieee80211_defines.h"
#include "ieee80211_p2p_proto.h"
#include "ieee80211_p2p_ie.h"
#include "ieee80211_p2p_prot_utils.h"
#include "ieee80211_sm.h"
#include "ieee80211_p2p_prot_priv.h"


/*
 * Returns true if this P2P_DISC_EVENT_ON_CHAN_END event is for a previously cancelled scan.
 */
bool
event_scan_end_from_cancel_scan(
    p2p_prot_scan_params        *prot_scan_param,
    IEEE80211_SCAN_ID           req_id
    )
{
    if (prot_scan_param->cancel_scan_requested) {
        if (req_id == prot_scan_param->cancel_scan_request_id) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: Got CHAN_END for previously cancelled scan  id=%d\n",
                __func__, req_id);
            prot_scan_param->cancel_scan_requested = false;
            return true;
        }
    }
    return false;
}

/*
 * Cancel the scan (if any) that was requested earlier by ieee80211_p2p_prot_request_scan()
 */
int
ieee80211_p2p_prot_cancel_scan(
    p2p_prot_scan_params        *prot_scan_param
    )
{
    int     retval = EOK;

    if (prot_scan_param->scan_requested) {
        retval = wlan_scan_cancel(prot_scan_param->vap, 
                         prot_scan_param->my_scan_requestor,
                         prot_scan_param->request_id,
                         IEEE80211_SCAN_CANCEL_ASYNC);
        if (retval != EOK) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: warning: wlan_scan_cancel() returns 0x%x.\n", __func__, retval);
        }
        if (prot_scan_param->cancel_scan_requested) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: warning: cancel_scan_requested is already true for curr request_id=%d, new request_id=%d\n",
                __func__, prot_scan_param->cancel_scan_request_id, prot_scan_param->request_id);
        }
        prot_scan_param->cancel_scan_requested = true;
        prot_scan_param->cancel_scan_request_id = prot_scan_param->request_id;

        prot_scan_param->scan_requested = false;
    }
    return retval;
}

int
ieee80211_p2p_prot_request_scan(
    p2p_prot_scan_params        *prot_scan_param,
    p2p_prot_scan_type          curr_scan_type
    )
{
    int     retval = EOK;

    switch(curr_scan_type) {
    case P2P_PROT_SCAN_TYPE_FULL:
    {
        /* Request a Full scan */
        ieee80211_scan_params   *scan_params;
        bool                    active_scan_flag;
        int                     num_ssids;

        scan_params = &(prot_scan_param->request_params);
        num_ssids = 1;  /* One SSID for wildcard */

        wlan_set_default_scan_parameters(prot_scan_param->vap, 
                                         scan_params, 
                                         IEEE80211_M_STA, 
                                         true,              /* active_scan_flag */
                                         true,              /* high_priority_flag */
                                         false,             /* connected_flag */
                                         true,              /* external_scan_flag */
                                         num_ssids,         /* number of SSID in ssid_list */
                                         prot_scan_param->probe_req_ssid,
                                         0);                /* Number of IBSS peer_count */

        /* Copy the IE's (if any) */
        if (prot_scan_param->probe_req_ie_len > 0) {
            /* Note: We did not alloc the buffer for IE but reused the existing buffer inside req_param */
            scan_params->ie_len = prot_scan_param->probe_req_ie_len;
            scan_params->ie_data = prot_scan_param->probe_req_ie_buf;
        }

        /* scan all channels */
        scan_params->num_channels = 0;

        scan_params->type = IEEE80211_SCAN_FOREGROUND;
        scan_params->flags = IEEE80211_SCAN_FORCED | IEEE80211_SCAN_ALLBANDS | IEEE80211_SCAN_ACTIVE;
        if (prot_scan_param->scan_legy_netwk) {
            /* Search for legacy networksduring scan phase */
            scan_params->flags |= IEEE80211_SCAN_ADD_BCAST_PROBE;
        }

        active_scan_flag = prot_scan_param->active_scan;

        scan_params->min_dwell_time_active  = active_scan_flag ? 
                                                P2P_PROT_ACTIVE_SCAN_DWELL_TIME : P2P_PROT_PASSIVE_SCAN_DWELL_TIME;
        scan_params->min_dwell_time_passive = P2P_PROT_PASSIVE_SCAN_DWELL_TIME;

        scan_params->max_dwell_time_active = scan_params->min_dwell_time_active;
        scan_params->max_dwell_time_passive = scan_params->min_dwell_time_passive;

        scan_params->repeat_probe_time = P2P_PROT_SCAN_REPEAT_PROBE_REQUEST_INTVAL;

        /* Adjust the P2P scan parameters so that this scan will not affect other active VAP */

        if (ieee80211_count_active_ports(prot_scan_param->vap->iv_ic, prot_scan_param->vap)) {
            /*
             * If there are other active ports, scan must return to home
             * channel to allow for data traffic or beacons.
             */
            scan_params->multiple_ports_active = true;
            scan_params->min_rest_time         = P2P_PROT_SCAN_MIN_REST_TIME_MULTIPLE_PORTS;
        }

        /*
         * Burst scan: Scan multiple channels before returning to BSS channel.
         */
        scan_params->max_offchannel_time  = P2P_PROT_SCAN_AP_PRESENT_MAX_OFFCHANNEL_TIME;
        scan_params->flags               |= IEEE80211_SCAN_BURST;

        if (prot_scan_param->cancel_scan_requested) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: note: last cancel_scan_requested(id=%d) is not completed yet.\n",
                __func__, prot_scan_param->cancel_scan_request_id);
        }
        if (prot_scan_param->scan_requested) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: should not have pending scan request here.\n", __func__);
        }
        prot_scan_param->scan_requested = false;

        retval = wlan_scan_start(prot_scan_param->vap,
                            scan_params,
                            prot_scan_param->my_scan_requestor,
                            IEEE80211_P2P_DEFAULT_SCAN_PRIORITY,
                            &(prot_scan_param->request_id));
        if (retval != EOK) {

            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s social scan req failed\n", __func__);
            break;
        }
        else {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s P2P_PROT_SCAN_TYPE_FULL: wlan_scan_start OK. returned request_id=%d\n",
                __func__, prot_scan_param->request_id);

            prot_scan_param->scan_requested = true;
        }
        break;
    }
    case P2P_PROT_SCAN_TYPE_PARTIAL:
    {
        /* Request a Partial scan. This is a scan of all channels but broken up into many parts. */
        ieee80211_scan_params   *scan_params;
        int                     i;
        u_int32_t               scan_chan_list[P2P_PROT_PARTIAL_SCAN_NUM_CHAN];
        bool                    active_scan_flag;
        int                     num_ssids;

        num_ssids = 1;  /* One SSID for wildcard */
        scan_params = &(prot_scan_param->request_params);

        wlan_set_default_scan_parameters(prot_scan_param->vap, 
                                         scan_params, 
                                         IEEE80211_M_STA, 
                                         true,              /* active_scan_flag */
                                         true,              /* high_priority_flag */
                                         false,             /* connected_flag */
                                         true,              /* external_scan_flag */
                                         num_ssids,         /* number of SSID in ssid_list */
                                         prot_scan_param->probe_req_ssid,
                                         0);                /* Number of IBSS peer_count */

        /* Copy the IE's (if any) */
        if (prot_scan_param->probe_req_ie_len > 0) {
            /* Note: We did not alloc the buffer for IE but reused the existing buffer inside req_param */
            scan_params->ie_len = prot_scan_param->probe_req_ie_len;
            scan_params->ie_data = prot_scan_param->probe_req_ie_buf;
        }

        /* Figure out what channel to scan */
        if (prot_scan_param->total_num_all_chans >= P2P_PROT_PARTIAL_SCAN_NUM_CHAN) {
            scan_params->num_channels = P2P_PROT_PARTIAL_SCAN_NUM_CHAN;
        }
        else {
            scan_params->num_channels = prot_scan_param->total_num_all_chans;
        }
        if ((prot_scan_param->total_num_all_chans - prot_scan_param->partial_scan_chan_index) < scan_params->num_channels) {
            /* We are at the end of the channel list. Only scan the remainder */
            scan_params->num_channels = prot_scan_param->total_num_all_chans - prot_scan_param->partial_scan_chan_index;
        }

        IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
            "%s: Scan num_chan=%d, start index=%d.\n", 
            __func__, scan_params->num_channels, prot_scan_param->partial_scan_chan_index);

        /* We get the next few entries from the all channel list. */
        for (i = 0; i < scan_params->num_channels; i++) {

            if (prot_scan_param->partial_scan_chan_index > prot_scan_param->total_num_all_chans) {
                prot_scan_param->partial_scan_chan_index = 0;
            }
            ASSERT(i < P2P_PROT_PARTIAL_SCAN_NUM_CHAN);
            scan_chan_list[i] = prot_scan_param->all_chans_list[prot_scan_param->partial_scan_chan_index];

            prot_scan_param->partial_scan_chan_index++;
        }
        scan_params->chan_list = scan_chan_list;

        scan_params->type = IEEE80211_SCAN_FOREGROUND;
        scan_params->flags = IEEE80211_SCAN_FORCED | IEEE80211_SCAN_ALLBANDS | IEEE80211_SCAN_ACTIVE;
        if (prot_scan_param->scan_legy_netwk) {
            /* Search for legacy networksduring scan phase */
            scan_params->flags |= IEEE80211_SCAN_ADD_BCAST_PROBE;
        }

        active_scan_flag = prot_scan_param->active_scan;

        scan_params->min_dwell_time_active  = active_scan_flag ? 
                                                P2P_PROT_ACTIVE_SCAN_DWELL_TIME : P2P_PROT_PASSIVE_SCAN_DWELL_TIME;
        scan_params->min_dwell_time_passive = P2P_PROT_PASSIVE_SCAN_DWELL_TIME;

        scan_params->max_dwell_time_active = scan_params->min_dwell_time_active;
        scan_params->max_dwell_time_passive = scan_params->min_dwell_time_passive;

        scan_params->repeat_probe_time = P2P_PROT_SCAN_REPEAT_PROBE_REQUEST_INTVAL;

        /* Adjust the P2P scan parameters so that this scan will not affect other active VAP */

        if (ieee80211_count_active_ports(prot_scan_param->vap->iv_ic, prot_scan_param->vap)) {
            /*
             * If there are other active ports, scan must return to home
             * channel to allow for data traffic or beacons.
             */
            scan_params->multiple_ports_active = true;
            scan_params->min_rest_time         = P2P_PROT_SCAN_MIN_REST_TIME_MULTIPLE_PORTS;
        }

        /*
         * Burst scan: Scan multiple channels before returning to BSS channel.
         */
        scan_params->max_offchannel_time  = P2P_PROT_SCAN_AP_PRESENT_MAX_OFFCHANNEL_TIME;
        scan_params->flags               |= IEEE80211_SCAN_BURST;

        if (prot_scan_param->cancel_scan_requested) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: note: last cancel_scan_requested(id=%d) is not completed yet.\n",
                __func__, prot_scan_param->cancel_scan_request_id);
        }
        if (prot_scan_param->scan_requested) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: should not have pending scan request here.\n", __func__);
        }
        prot_scan_param->scan_requested = false;

        retval = wlan_scan_start(prot_scan_param->vap,
                            scan_params,
                            prot_scan_param->my_scan_requestor,
                            IEEE80211_P2P_DEFAULT_SCAN_PRIORITY,
                            &(prot_scan_param->request_id));
        if (retval != EOK) {

            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s social scan req failed\n", __func__);
            break;
        }
        else {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s P2P_PROT_SCAN_TYPE_FULL: wlan_scan_start OK. returned request_id=%d\n",
                __func__, prot_scan_param->request_id);

            prot_scan_param->scan_requested = true;
        }
        break;
    }
    case P2P_PROT_SCAN_TYPE_SOCIAL:
    {
        /* Requested a 3 channel scan on the social channels 1,6,11. 
         * We will just send Probe Requests and do not reply with Probe Responses. */
        ieee80211_scan_params   *scan_params;

        scan_params = &(prot_scan_param->request_params);

        wlan_set_default_scan_parameters(prot_scan_param->vap, 
                                         scan_params, 
                                         IEEE80211_M_STA, 
                                         true,              /* active_scan_flag */
                                         true,              /* high_priority_flag */
                                         false,              /* connected_flag */
                                         true,              /* external_scan_flag */
                                         1,                 /* P2P wildcard SSID */
                                         prot_scan_param->probe_req_ssid,
                                         0);                /* peer_count */

        scan_params->num_channels = 3;
        scan_params->chan_list = prot_scan_param->social_chans_list;

        /* Copy the IE's (if any) */
        if (prot_scan_param->probe_req_ie_len > 0) {
            /* Note: We did not alloc the buffer for IE but reused the existing buffer inside req_param */
            scan_params->ie_len = prot_scan_param->probe_req_ie_len;
            scan_params->ie_data = prot_scan_param->probe_req_ie_buf;
        }

        scan_params->type = IEEE80211_SCAN_FOREGROUND;
        scan_params->flags = IEEE80211_SCAN_FORCED | IEEE80211_SCAN_ALLBANDS | IEEE80211_SCAN_ACTIVE;
        scan_params->flags |= IEEE80211_SCAN_ADD_BCAST_PROBE;
        scan_params->min_dwell_time_active = scan_params->max_dwell_time_active = P2P_PROT_SOCIAL_SCAN_DWELL_TIME;
        scan_params->min_dwell_time_passive = scan_params->max_dwell_time_passive = P2P_PROT_SOCIAL_SCAN_DWELL_TIME;
        /* Parameters, min_rest_time and max_rest_time, can be ignored 
         * since this is unused because it is a foreground scan. */
        scan_params->repeat_probe_time = 0;

        /* Parameters for burst scan */
        scan_params->multiple_ports_active = false;
        scan_params->max_offchannel_time   = P2P_PROT_SCAN_AP_PRESENT_MAX_OFFCHANNEL_TIME;
        scan_params->flags |= IEEE80211_SCAN_BURST;

        if (prot_scan_param->cancel_scan_requested) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: note: last cancel_scan_requested(id=%d) is not completed yet.\n",
                __func__, prot_scan_param->cancel_scan_request_id);
        }
        if (prot_scan_param->scan_requested) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: should not have pending scan request here.\n", __func__);
        }
        prot_scan_param->scan_requested = false;

        retval = wlan_scan_start(prot_scan_param->vap,
                            scan_params,
                            prot_scan_param->my_scan_requestor,
                            IEEE80211_P2P_DEFAULT_SCAN_PRIORITY,
                            &(prot_scan_param->request_id));
        if (retval != EOK) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s social scan req failed\n", __func__);
            break;
        }
        else {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s P2P_PROT_SCAN_TYPE_SOCIAL: wlan_scan_start OK. returned request_id=%d\n",
                __func__, prot_scan_param->request_id);

            prot_scan_param->scan_requested = true;
        }
        break;
    }
    case P2P_PROT_SCAN_TYPE_LISTEN:
        ASSERT(false);
        break;
    case P2P_PROT_SCAN_TYPE_FIND_DEV:
    {
        /* Requested a 1 channel scan on the desired device listen channel. 
         * We will just send Probe Requests and do not reply with Probe Responses. */
        ieee80211_scan_params   *scan_params;

        scan_params = &(prot_scan_param->request_params);

        wlan_set_default_scan_parameters(prot_scan_param->vap, 
                                         scan_params, 
                                         IEEE80211_M_STA, 
                                         true,              /* active_scan_flag */
                                         true,              /* high_priority_flag */
                                         false,              /* connected_flag */
                                         true,              /* external_scan_flag */
                                         1,                 /* P2P wildcard SSID */
                                         prot_scan_param->probe_req_ssid,
                                         0);                /* peer_count */

        ASSERT(prot_scan_param->device_freq != 0);

        scan_params->num_channels = 1;
        scan_params->chan_list = &prot_scan_param->device_freq;

        /* Copy the IE's (if any) */
        if (prot_scan_param->probe_req_ie_len > 0) {
            /* Note: We did not alloc the buffer for IE but reused the existing buffer inside req_param */
            scan_params->ie_len = prot_scan_param->probe_req_ie_len;
            scan_params->ie_data = prot_scan_param->probe_req_ie_buf;
        }

        scan_params->type = IEEE80211_SCAN_FOREGROUND;
        scan_params->flags = IEEE80211_SCAN_FORCED | IEEE80211_SCAN_ALLBANDS | IEEE80211_SCAN_ACTIVE;
        scan_params->flags |= IEEE80211_SCAN_ADD_BCAST_PROBE;
        /* Note that we are will stay on channel for multiple social scan dwell times. We will send
         * repeated probe request frames.
         */
        scan_params->min_dwell_time_active = scan_params->max_dwell_time_active = 
            (P2P_PROT_SOCIAL_SCAN_DWELL_TIME * P2P_PROT_DEV_FIND_NUM_REPEATED_PROBE);
        scan_params->min_dwell_time_passive = scan_params->max_dwell_time_passive = P2P_PROT_PASSIVE_SCAN_DWELL_TIME;
        /* Parameters, min_rest_time and max_rest_time, can be ignored 
         * since this is unused because it is a foreground scan. */

        /* We will repeat the probe request */
        scan_params->repeat_probe_time = P2P_PROT_SOCIAL_SCAN_DWELL_TIME;

        /* Parameters for burst scan */
        scan_params->multiple_ports_active = false;
        scan_params->max_offchannel_time   = P2P_PROT_SCAN_AP_PRESENT_MAX_OFFCHANNEL_TIME;
        scan_params->flags |= IEEE80211_SCAN_BURST;

        if (prot_scan_param->cancel_scan_requested) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_NORMAL, IEEE80211_MSG_P2P_PROT,
                "%s: note: last cancel_scan_requested(id=%d) is not completed yet.\n",
                __func__, prot_scan_param->cancel_scan_request_id);
        }
        if (prot_scan_param->scan_requested) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_SERIOUS, IEEE80211_MSG_P2P_PROT,
                "%s: Error: should not have pending scan request here.\n", __func__);
        }
        prot_scan_param->scan_requested = false;

        retval = wlan_scan_start(prot_scan_param->vap,
                            scan_params,
                            prot_scan_param->my_scan_requestor,
                            IEEE80211_P2P_DEFAULT_SCAN_PRIORITY,
                            &(prot_scan_param->request_id));
        if (retval != EOK) {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s find device scan req failed\n", __func__);
            break;
        }
        else {
            IEEE80211_DPRINTF_VB(prot_scan_param->vap, IEEE80211_VERBOSE_LOUD, IEEE80211_MSG_P2P_PROT,
                "%s P2P_PROT_SCAN_TYPE_FIND_DEV: wlan_scan_start OK. returned request_id=%d\n",
                __func__, prot_scan_param->request_id);

            prot_scan_param->scan_requested = true;
        }
        break;
    }
    default:
        ASSERT(false);
        retval = -EPERM;
    }

    return retval;
}

static int p2p_channel_to_freq_j4(int reg_class, int channel)
{
    /* Table J-4 in P802.11REVmb/D4.0 - Global operating classes */
    /* TODO: more regulatory classes */
    switch (reg_class) {
    case 81:
        /* channels 1..13 */
        if (channel < 1 || channel > 13)
            return -1;
        return 2407 + 5 * channel;
    case 82:
        /* channel 14 */
        if (channel != 14)
            return -1;
        return 2414 + 5 * channel;
    case 83: /* channels 1..9; 40 MHz */
    case 84: /* channels 5..13; 40 MHz */
        if (channel < 1 || channel > 13)
            return -1;
        return 2407 + 5 * channel;
    case 115: /* channels 36,40,44,48; indoor only */
    case 118: /* channels 52,56,60,64; dfs */
        if (channel < 36 || channel > 64)
            return -1;
        return 5000 + 5 * channel;
    case 124: /* channels 149,153,157,161 */
    case 125: /* channels 149,153,157,161,165,169 */
#if P2P_SUPPORT_OP_CLASS_125
        if (channel < 149 || channel > 169)
#else  //P2P_SUPPORT_OP_CLASS_125
        if (channel < 149 || channel > 161)
#endif //P2P_SUPPORT_OP_CLASS_125
            return -1;
        return 5000 + 5 * channel;
    case 116: /* channels 36,44; 40 MHz; indoor only */
    case 117: /* channels 40,48; 40 MHz; indoor only */
    case 119: /* channels 52,60; 40 MHz; dfs */
    case 120: /* channels 56,64; 40 MHz; dfs */
        if (channel < 36 || channel > 64)
            return -1;
        return 5000 + 5 * channel;
    case 126: /* channels 149,157; 40 MHz */
    case 127: /* channels 153,161; 40 MHz */
        if (channel < 149 || channel > 161)
            return -1;
        return 5000 + 5 * channel;
    }
    return -1;
}

/**
 * wlan_p2p_channel_to_freq - Convert channel info to frequency
 * @country: Country code
 * @reg_class: Regulatory class
 * @channel: Channel number
 * Returns: Frequency in MHz or -1 if the specified channel is unknown
 */
int wlan_p2p_channel_to_freq(const char *country, int reg_class, int channel)
{
    if (country[2] == 0x04)
        return p2p_channel_to_freq_j4(reg_class, channel);
    
    /* These are mainly for backwards compatibility; to be removed */
    switch (reg_class) {
    case 1: /* US/1, EU/1, JP/1 = 5 GHz, channels 36,40,44,48 */
        if (channel < 36 || channel > 48)
            return -1;
        else return 5000 + 5 * channel;
        break;
    case 3: /* US/3 = 5 GHz, channels 149,153,157,161 */
    case 5: /* US/5 = 5 GHz, channels 149,153,157,161 */
        if (channel < 149 || channel > 161)
            return -1;
        else return 5000 + 5 * channel;
        break;
    case 4: /* EU/4 = 2.407 GHz, channels 1..13 */
    case 12: /* US/12 = 2.407 GHz, channels 1..11 */
    case 30: /* JP/30 = 2.407 GHz, channels 1..13 */
        if (channel < 1 || channel > 13)
            return -1;
        else return 2407 + 5 * channel;
        break;
    case 31: /* JP/31 = 2.414 GHz, channel 14 */
        if (channel != 14)
            return -1;
        else return 2414 + 5 * channel;
        break;
    }
    
    return -1;
}

/**
 * p2p_freq_to_channel - Convert frequency into channel info
 * @country: Country code
 * @reg_class: Buffer for returning regulatory class
 * @channel: Buffer for returning channel number
 * Returns: 0 on success, -1 if the specified frequency is unknown
 * 
 * Note: the returned reg_class and channel are for the HT20
 * modes and not HT40 modes.
 * The information for this function is extracted from the Table
 * E-4-Global Operating classes.
 * 
 */
int
wlan_p2p_freq_to_channel(const char *country, unsigned int freq, u8 *reg_class, u8 *channel)
{
    if ((freq >= 2412) && (freq <= 2472)) {
        if (((freq - 2407) % 5) != 0) {
            /* error: freq not exact */
            return -1;
        }
        *reg_class = 81; /* 2.407 GHz, channels 1..13 */
        *channel = (freq - 2407) / 5;
        return 0;
    }
    
    if (freq == 2484) {
        *reg_class = 82; /* channel 14 */
        *channel = 14;
        return 0;
    }

#define FREQ_5G_CH(_chan_num)   (5000 + (5 * _chan_num))

#define CASE_5G_FREQ(_chan_num)         \
    case FREQ_5G_CH(_chan_num):         \
        *channel = _chan_num;           \
        break;

    if ((freq >= FREQ_5G_CH(36)) && (freq <= FREQ_5G_CH(48))) {
        switch(freq) {
        CASE_5G_FREQ(36);
        CASE_5G_FREQ(40);
        CASE_5G_FREQ(44);
        CASE_5G_FREQ(48);
        default:
            /* No valid frequency in this range */
            return -1;
        }
        *reg_class = 115; /* 5 GHz, channels 36..48 */
        return 0;
    }
    
    if ((freq >= FREQ_5G_CH(149)) && (freq <= FREQ_5G_CH(161))) {
        switch(freq) {
        CASE_5G_FREQ(149);
        CASE_5G_FREQ(153);
        CASE_5G_FREQ(157);
        CASE_5G_FREQ(161);
        default:
            /* No valid frequency in this range */
            return -1;
        }
        *reg_class = 124; /* 5 GHz, channels 149..161 */
        return 0;
    }

    if ((freq >= FREQ_5G_CH(8)) && (freq <= FREQ_5G_CH(16))) {
        switch(freq) {
        CASE_5G_FREQ(8);
        CASE_5G_FREQ(12);
        CASE_5G_FREQ(16);
        default:
            /* No valid frequency in this range */
            return -1;
        }
        *reg_class = 112; /* 5 GHz, channels 8, 12, 16 */
        return 0;
    }

    if ((freq >= FREQ_5G_CH(52)) && (freq <= FREQ_5G_CH(64))) {
        switch(freq) {
        CASE_5G_FREQ(52);
        CASE_5G_FREQ(56);
        CASE_5G_FREQ(60);
        CASE_5G_FREQ(64);
        default:
            /* No valid frequency in this range */
            return -1;
        }
        *reg_class = 118; /* 5 GHz, channels 52, 56, 60, 64 */
        return 0;
    }

    if ((freq >= FREQ_5G_CH(100)) && (freq <= FREQ_5G_CH(140))) {
        switch(freq) {
        CASE_5G_FREQ(100);
        CASE_5G_FREQ(104);
        CASE_5G_FREQ(108);
        CASE_5G_FREQ(112);
        CASE_5G_FREQ(116);
        CASE_5G_FREQ(120);
        CASE_5G_FREQ(124);
        CASE_5G_FREQ(128);
        CASE_5G_FREQ(132);
        CASE_5G_FREQ(136);
        CASE_5G_FREQ(140);
        default:
            /* No valid frequency in this range */
            return -1;
        }
        *reg_class = 121; /* 5 GHz, channels 100, 104, 108, 112,
                           * 116, 120, 124, 128, 132, 136, 140 */
        return 0;
    }

#if P2P_SUPPORT_OP_CLASS_125
/* Note: the wpa_supplicant from Jouni did not support this class. Therefore, to be consistent,
 * this driver does not support it. */
    if ((freq >= FREQ_5G_CH(165)) && (freq <= FREQ_5G_CH(169))) {
        switch(freq) {
        CASE_5G_FREQ(165);
        CASE_5G_FREQ(169);
        default:
            /* No valid frequency in this range */
            return -1;
        }
        *reg_class = 125; /* 5 GHz, license exempt behavior, channels 165, 169 */
        return 0;
    }
#endif //P2P_SUPPORT_OP_CLASS_125

    return -1;

#undef CASE_5G_FREQ
#undef FREQ_5G_CH
}

/*
 * Convert the given channel to its corresponding P2P regulatory class and channel number.
 * Return 0 if a conversion is found. Return -1 if not.
 */
int
wlan_p2p_chan_convert_to_p2p_channel(wlan_chan_t chan, const char *country, u8 *reg_class, u8 *channel)
{
    u_int32_t freq = wlan_channel_frequency(chan);
    //u_int32_t flags = wlan_channel_flags(chan);
    u_int32_t ieee = wlan_channel_ieee(chan);

    if (IEEE80211_IS_CHAN_11N_HT40(chan)) {
        /* HT40 mode */
        if (IEEE80211_IS_CHAN_5GHZ(chan)) {
            /* HT40 mode in 5 Ghz band */
            if (IEEE80211_IS_CHAN_11NA_HT40PLUS(chan)) {
                /* 5G HT40 PLUS */
                switch(ieee) {
                case 36:
                case 44:
                    *reg_class = 116;
                    *channel = ieee;
                    break;
                case 52:
                case 60:
                    *reg_class = 119;
                    *channel = ieee;
                    break;
                case 100:
                case 108:
                case 116:
                case 124:
                case 132:
                    *reg_class = 122;
                    *channel = ieee;
                    break;
                case 149:
                case 157:
                    *reg_class = 126;
                    *channel = ieee;
                    break;
                default:
                    /* Unsupported */
                    return -1;
                }
                return 0;
            }
            else {
                /* Must be 5 Ghz HT40 Minus */
                ASSERT(IEEE80211_IS_CHAN_11NA_HT40MINUS(chan));

                switch(ieee) {
                case 40:
                case 48:
                    *reg_class = 117;
                    *channel = ieee;
                    break;
                case 56:
                case 64:
                    *reg_class = 120;
                    *channel = ieee;
                    break;
                case 104:
                case 112:
                case 120:
                case 128:
                case 136:
                    *reg_class = 123;
                    *channel = ieee;
                    break;
                case 153:
                case 161:
                    *reg_class = 127;
                    *channel = ieee;
                    break;
                default:
                    /* Unsupported */
                    return -1;
                }
                return 0;
            }
        }
        else {
            if (IEEE80211_IS_CHAN_11NG_HT40PLUS(chan)) {
                /* 2.4 GHz HT40 PLUS */
                if ((ieee >= 1) && (ieee <= 9)) {
                    *reg_class = 83; /* 2.407 GHz, HT40Plus, channels 1..9 */
                    *channel = ieee;
                    return 0;
                }
            }
            else {
                /* Must be 2.4 Ghz HT40 Minus */
                ASSERT(IEEE80211_IS_CHAN_11NG_HT40MINUS(chan));

                if ((ieee >= 5) && (ieee <= 13)) {
                    *reg_class = 84; /* 2.407 GHz, HT40Minus, channels 5..13 */
                    *channel = ieee;
                    return 0;
                }
            }
            return -1;
        }
    }
    else {
        /* Non HT40 modes */
        if (wlan_p2p_freq_to_channel(country, freq, reg_class, channel) == 0) {
            /* Found an appropriate P2P channel */
            return 0;
        }
        else {
            return -1;   /* not found */
        }
    }
}

static void p2p_reg_class_intersect(const struct p2p_reg_class *a,
                                    const struct p2p_reg_class *b,
                                    struct p2p_reg_class *res)
{
    size_t i, j;
    
    res->reg_class = a->reg_class;
    
    for (i = 0; i < a->channels; i++) {
        for (j = 0; j < b->channels; j++) {
            if (a->channel[i] != b->channel[j])
                continue;
            res->channel[res->channels] = a->channel[i];
            res->channels++;
            if (res->channels == P2P_MAX_REG_CLASS_CHANNELS)
                return;
        }
    }
}

/**
 * p2p_channels_intersect - Intersection of supported channel lists
 * @a: First set of supported channels
 * @b: Second set of supported channels
 * @res: Data structure for returning the intersection of support channels
 *
 * This function can be used to find a common set of supported channels. Both
 * input channels sets are assumed to use the same country code. If different
 * country codes are used, the regulatory class numbers may not be matched
 * correctly and results are undefined.
 */
void
wlan_p2p_channels_intersect(const struct p2p_channels *a,
                            const struct p2p_channels *b,
                            struct p2p_channels *res)
{
    size_t i, j;
    
    OS_MEMSET(res, 0, sizeof(*res));
    
    for (i = 0; i < a->reg_classes; i++) {
        const struct p2p_reg_class *a_reg = &a->reg_class[i];
        for (j = 0; j < b->reg_classes; j++) {
            const struct p2p_reg_class *b_reg = &b->reg_class[j];
            if (a_reg->reg_class != b_reg->reg_class)
                continue;
            p2p_reg_class_intersect(a_reg, b_reg, &res->reg_class[res->reg_classes]);
            if (res->reg_class[res->reg_classes].channels) {
                res->reg_classes++;
                if (res->reg_classes == P2P_MAX_REG_CLASSES)
                    return;
            }
        }
    }
}

/**
 * p2p_channels_includes - Check whether a channel is included in the list
 * @channels: List of supported channels
 * @reg_class: Regulatory class of the channel to search
 * @channel: Channel number of the channel to search
 * Returns: 1 if channel was found or 0 if not
 */
int 
wlan_p2p_channels_includes(const struct p2p_channels *channels, u8 reg_class,
                           u8 channel)
{
    size_t i, j;
    for (i = 0; i < channels->reg_classes; i++) {
        const struct p2p_reg_class *reg = &channels->reg_class[i];
        if (reg->reg_class != reg_class)
            continue;
        for (j = 0; j < reg->channels; j++) {
            if (reg->channel[j] == channel)
                return 1;
        }
    }
    return 0;
}

/*
 * This function will check if the channel frequency can be supported in the given
 * channel list. If supported, then the reg class and channel number are returned.
 * Return 1 if the given frequency is supported by the given channel list.
 * Return 0 if unsupported.
 */
int
wlan_p2p_supported_chan_on_freq(const char          *country,
                                struct p2p_channels *chan_list, 
                                u_int32_t           supported_freq,
                                u8                  *ret_chan_reg_class, 
                                u8                  *ret_chan_channel)
{
    if (wlan_p2p_freq_to_channel(country, supported_freq, ret_chan_reg_class, ret_chan_channel) < 0)
        return 0;
    return wlan_p2p_channels_includes(chan_list, *ret_chan_reg_class, *ret_chan_channel);
}

