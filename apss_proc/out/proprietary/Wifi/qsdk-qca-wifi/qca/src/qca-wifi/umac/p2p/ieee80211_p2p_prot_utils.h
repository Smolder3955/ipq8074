/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2011, Atheros Communications
 *
 * This file contains the defines for the common utility routines for P2P Protocol module.
 *
 */
/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */


#ifndef _IEEE80211_P2P_PROT_UTILS_H
#define _IEEE80211_P2P_PROT_UTILS_H

#include "ieee80211_p2p_device_priv.h"

/* Type of scan to request */
typedef enum {
    P2P_PROT_SCAN_TYPE_NONE = 0,    /* No scan pending */
    P2P_PROT_SCAN_TYPE_FULL,        /* All channels full scan */
    P2P_PROT_SCAN_TYPE_PARTIAL,     /* All channels scan but scan a partial list everytime */
    P2P_PROT_SCAN_TYPE_SOCIAL,      /* 3 channels (ch 1, 6, 11) scan and in search mode */
    P2P_PROT_SCAN_TYPE_LISTEN,      /* Single channel scan but in listen mode */
    P2P_PROT_SCAN_TYPE_FIND_DEV,    /* Single channel scan to find a device. Listen Channel is known */
} p2p_prot_scan_type;

/* Structure to store the current scan parameters and information */
typedef struct _p2p_prot_scan_params {
    wlan_if_t                           vap;

    IEEE80211_SCAN_ID                   request_id;             /* Scan ID of the current requested scan */
    bool                                scan_requested;         /* a scan is requested and pending */

    bool                                cancel_scan_requested;  /* Is this scan cancelled and waiting for confirmation from scanner? */
    IEEE80211_SCAN_ID                   cancel_scan_request_id; /*  If cancel_scan_requested is true, then this field is the ID. */
    int                                 cancel_scan_recheck_cnt;/* Number of times to check if the cancelled scan is confirmed */

    int                                 set_chan_req_id;        /* last ID used when calling wlan_p2p_set_channel() */
    bool                                set_chan_requested;     /* A set_channel is requested and pending */
    u_int16_t                           set_chan_freq;          /* requested set chan frequency setting in Mhz */
    bool                                set_chan_started;       /* Has the ON_CHAN scan started? */
    systime_t                           set_chan_startime;      /* Time when the ON_CHAN scan started */
    u_int32_t                           set_chan_duration;      /* requested ON_CHAN duration in millisec */
    bool                                set_chan_cancelled;     /* Is last set_channel cancelled? */
    int                                 set_chan_cancel_req_id; /* last ID of cancelled set channel request. */

    ieee80211_scan_params               request_params;         /* Current scan request parameters */
    u_int32_t                           social_chans_list[IEEE80211_P2P_NUM_SOCIAL_CHANNELS];
    u_int32_t                           total_num_all_chans;
    u_int32_t                           all_chans_list[IEEE80211_CHAN_MAX];

    IEEE80211_SCAN_REQUESTOR            my_scan_requestor;      /* scan requester ID. */
    int                                 partial_scan_chan_index;/* Used for Partial Scan and indicates the next channel index to scan. */
    u_int32_t                           device_freq;            /* Listen or Operational Freq of desired device. Only valid when this peer is in scan table */

    ieee80211_ssid                      *probe_req_ssid;        /* SSID used for Probe Requests */

    u_int32_t                           probe_req_ie_len;       /* Length of Additional IE for Probe Request frames */
    u_int8_t                            *probe_req_ie_buf;      /* Additional IE for Probe Request frames */

    bool                                scan_legy_netwk;        /* scan legacy network */
    bool                                active_scan;            /* Whether to use an active scan */

} p2p_prot_scan_params;

int
ieee80211_p2p_prot_request_scan(
    p2p_prot_scan_params        *prot_scan_param,
    p2p_prot_scan_type          curr_scan_type
    );

int
ieee80211_p2p_prot_cancel_scan(
    p2p_prot_scan_params        *prot_scan_param
    );

/*
 * Returns true if this P2P_DISC_EVENT_ON_CHAN_END event is for a previously cancelled scan.
 */
bool
event_scan_end_from_cancel_scan(
    p2p_prot_scan_params        *prot_scan_param,
    IEEE80211_SCAN_ID           req_id
    );

int wlan_p2p_channel_to_freq(const char *country, int reg_class, int channel);

int wlan_p2p_freq_to_channel(const char *country, unsigned int freq, u8 *reg_class, u8 *channel);

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
                                u8                  *ret_chan_channel);

int 
wlan_p2p_channels_includes(const struct p2p_channels *channels, u8 reg_class,
                           u8 channel);

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
                            struct p2p_channels *res);

/*
 * Convert the given channel to its corresponding P2P regulatory class and channel number.
 * Return 0 if a conversion is found. Return -1 if not.
 */
int
wlan_p2p_chan_convert_to_p2p_channel(wlan_chan_t chan, const char *country, u8 *reg_class, u8 *channel);

#endif  //_IEEE80211_P2P_PROT_UTILS_H

