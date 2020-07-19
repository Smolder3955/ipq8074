/*
* Copyright (c) 2016, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2016 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _IEEE80211_RTT_H_
#define _IEEE80211_RTT_H_

/* Used for Where are you: LCI measurement request */
struct wru_lci_request {
    u_int8_t sta_mac[IEEE80211_ADDR_LEN];
    u_int8_t dialogtoken;
    u_int16_t num_repetitions;
    u_int8_t id;
    u_int8_t len;
    u_int8_t meas_token;
    u_int8_t meas_req_mode;
    u_int8_t meas_type;
    u_int8_t loc_subject;
}__attribute__((packed));


#define MAX_NEIGHBOR_NUM 15
#define MAX_NEIGHBOR_LEN 50
struct ftmrr_request {
    u_int8_t sta_mac[IEEE80211_ADDR_LEN];
    u_int8_t dialogtoken;
    u_int16_t num_repetitions;
    u_int8_t id;
    u_int8_t len;
    u_int8_t meas_token;
    u_int8_t meas_req_mode;
    u_int8_t meas_type;
    u_int16_t rand_inter;
    u_int8_t min_ap_count;
    u_int8_t elem[MAX_NEIGHBOR_NUM*MAX_NEIGHBOR_LEN];
}__attribute__((packed));

#endif
