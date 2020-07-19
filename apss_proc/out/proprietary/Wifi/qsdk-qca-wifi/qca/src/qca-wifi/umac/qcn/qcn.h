/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

/*
 QCN  module
*/
#ifndef _QCN_H__
#define _QCN_H__
#include <ieee80211_var.h>
#define QCN_IE_MIN_LEN 8
#define DEFAULT_PPDU_DURATION 100

/*
 * Structure for id(elem id or attribute id) and
 * len fields.Done to avoid redundancy due to
 * repeated usage of these two fields.
 */
struct ieee80211_qcn_header {
    u_int8_t id;
    u_int8_t len;
}qdf_packed;


/*
 * QCN_IE
 *
 */
struct ieee80211_qcn_ie {
    struct ieee80211_qcn_header header;
    u_int8_t oui[3];
    u_int8_t oui_type;
    u_int8_t opt_ie[0];
}qdf_packed; /*packing is required */

/*
 * QCN_IE version attribute
 */
struct ieee80211_qcn_ver_attr {
    struct ieee80211_qcn_header header;
    u_int8_t  version;
    u_int8_t  sub_version;
    u_int8_t  opt_ie[0];
}qdf_packed;

/*
 * QCN_IE transition reason code attribute
 */
struct ieee80211_qcn_transit_reason_code {
    struct ieee80211_qcn_header header;
    u_int8_t reason_code;
    u_int8_t opt_ie[0];
}qdf_packed;

/*
 * QCN_IE trasition reject reason code attribute
 */
struct ieee80211_qcn_transit_reject_reason_code {
    struct ieee80211_qcn_header header;
    u_int8_t reason_code;
    u_int8_t opt_ie[0];
}qdf_packed;

/*
 * QCN_IE feature attribute of variable lenght
 */
struct ieee80211_qcn_feat_attr {
    struct ieee80211_qcn_header header;
    u_int8_t  feature_mask[0];
}qdf_packed;

u_int8_t ieee80211_setup_qcn_attrs(struct ieee80211vap *vap,u_int8_t **frm, qcn_ie_type type, void *reqinfo);
u_int8_t ieee80211_setup_ver_attr_ie(u_int8_t **frm);
u_int8_t ieee80211_setup_qcn_trans_reason_ie(struct ieee80211vap *vap,
        u_int8_t **frm,struct ieee80211_bstm_reqinfo_target *bstm_reqinfo);
#endif /* _QCN_H */
