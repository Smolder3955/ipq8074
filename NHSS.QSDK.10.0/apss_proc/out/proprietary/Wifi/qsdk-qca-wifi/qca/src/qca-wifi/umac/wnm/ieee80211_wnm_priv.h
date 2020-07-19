/*
* Copyright (c) 2014, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  2014 Qualcomm Atheros, Inc.  All rights reserved.
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners. 
 */

#ifndef _IEEE80211_WNM__PRIV_H_
#define _IEEE80211_WNM__PRIV_H_
#if UMAC_SUPPORT_WNM

#include <ieee80211_var.h>

typedef struct ieee80211_fms_stream_s
{
    TAILQ_ENTRY(ieee80211_fms_stream_s) stream_next;
    u_int8_t num_tclas;
    u_int8_t len;
    u_int8_t counter_id;
    u_int8_t num_sta;
    u_int8_t fmsid;
    u_int8_t del_itvl;
    u_int8_t new_del_itvl;
    u_int8_t del_itvl_changed;
    u_int8_t max_del_itvl;
    u_int8_t new_max_del_itvl;
    u_int8_t max_del_itvl_changed;
    ieee80211_fms_rate_identifier_t rate_id;
    u_int8_t mcast_addr[6];
    TAILQ_HEAD(, tclas_element) tclas_head;
    struct ieee80211_tclas_processing tclasprocess;
}ieee80211_fms_stream_t;

typedef struct ieee80211_fms_counter_s
{
    u_int8_t inuse;
    u_int8_t counter_id;
    u_int8_t current_count;
    u_int8_t del_itvl;
    u_int8_t numfms;
    u_int8_t fms_token;
    TAILQ_HEAD(, ieee80211_fms_stream_s) fms_stream_head;
}ieee80211_fms_counter_t;

struct ieee80211_fms {
       u_int8_t fms_token;
       u_int8_t fmsid;
       ieee80211_fms_counter_t counters[IEEE80211_FMS_MAX_COUNTERS];
};

typedef struct ieee80211_fms    *ieee80211_fms_t;

typedef struct ieee80211_fms_act_stream_ptr {
     TAILQ_ENTRY(ieee80211_fms_act_stream_ptr) strm_ptr_next;
     ieee80211_fms_stream_t *stream;
}ieee80211_fms_act_stream_ptr_t;

int ieee80211_recv_bstm_resp(wlan_if_t vap, wlan_node_t ni,
                                struct ieee80211_action_bstm_resp *bstm_resp, int frm_len);

int ieee80211_recv_bstm_query(wlan_if_t vap, wlan_node_t ni,
                                struct ieee80211_action_bstm_query *bstm_query, int frm_len);

int ieee80211_send_bstm_req(wlan_if_t vap, wlan_node_t ni,
                                        struct ieee80211_bstm_reqinfo* bstm_reqinfo);


int ieee80211_send_bstm_req_target(wlan_if_t vap, wlan_node_t ni,
                                   struct ieee80211_bstm_reqinfo_target* bstm_reqinfo);

int ieee80211_recv_wnm_req(wlan_if_t vap, wlan_node_t ni, u_int8_t *frm, int frm_len);

u_int8_t *fill_nrinfo_from_user(wlan_if_t vap, u_int8_t *frm, wlan_node_t ni);

int ieee80211_recv_fms_req(wlan_if_t vap, wlan_node_t ni,
                                   u_int8_t *frm, int frm_len);
int ieee80211_send_fms_req(wlan_if_t vap, wlan_node_t ni);
int ieee80211_recv_fms_rsp(wlan_if_t vap, wlan_node_t ni,
                           u_int8_t *frm, int frm_len);
int ieee80211_tclass_element_parse(wlan_if_t vap, void *head, ieee80211_tclas_processing *tclasprocess,
                                   struct fmsresp_tclas_subele_status *tclas_status_msg,
                                   u_int8_t **frm, u_int8_t tclass_len);
u_int8_t *ieee80211_add_tfsreq_ie(wlan_if_t vap, wlan_node_t ni, u_int8_t *frm);

u_int8_t *ieee80211_add_tfsrsp_ie(wlan_if_t vap, wlan_node_t ni, u_int8_t *frm);

int ieee80211_send_tfs_req(wlan_if_t vap, wlan_node_t ni);

int ieee80211_recv_tfs_req(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len);

u_int8_t * ieee80211_add_tfs_req(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len);

int ieee80211_add_tfs_resp(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len);

int ieee80211_recv_tfs_req(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len);

int ieee80211_recv_tfs_resp(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len);


int ieee80211_recv_tfs_recv_notify(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len);

int ieee80211_send_tfs_rsp(wlan_if_t vap,wlan_node_t ni);


int ieee80211_tfs_notify(wlan_node_t ni, u_int8_t tfsid_length, u_int8_t *tfsid);

int ieee80211_send_tim_req(wlan_if_t vap, wlan_node_t ni);

int ieee80211_recv_tim_req(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len);

int ieee80211_send_tim_resp(wlan_if_t vap, wlan_node_t ni);

int ieee80211_recv_tim_resp(wlan_if_t vap, wlan_node_t ni,
                                u_int8_t *frm, int frm_len);

int
ieee80211_send_tim_response(wlan_if_t vap, wlan_node_t ni);
int ieee80211_wnm_free_fmsreq(void *head);
int ieee80211_wnm_free_fmsrsp(void *head);
#endif /* UMAC_SUPPORT_WNM */

#endif /* _IEEE80211_WNM__PRIV_H_ */

