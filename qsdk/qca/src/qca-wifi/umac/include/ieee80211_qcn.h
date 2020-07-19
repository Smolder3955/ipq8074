/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#if QCN_IE
#ifndef _UMAC_IEEE80211_QCN__
#define _UMAC_IEEE80211_QCN__

#include <ieee80211_var.h>
#include <ieee80211_node.h>

typedef enum {
    QCN_TRANSITION_REJECTION_UNSPECIFIED,
    QCN_TRANSITION_REJECTION_FRAME_LOSS,
    QCN_TRANSITION_REJECTION_EXCESSIVE_DELAY,
    QCN_TRANSITION_REJECTION_INSUFFICIENT_QOS,
    QCN_TRANSITION_REJECTION_LOW_RSSI,
    QCN_TRANSITION_REJECTION_INTERFERENCE,
    QCN_TRANSITION_REJECTION_GRAY_ZONE,
    QCN_TRANSITION_REJECTION_SERVICE_UNAVAIL,

/*Reserved values form 8-255 */
    QCN_TRANSITION_REJECTION_MAX = 255
}qcn_transition_rej_code;


typedef enum {
    QCN_TRANSITION_REASON_UNSPECIFIED,
    QCN_TRANSITION_REASON_FRAME_LOSS,
    QCN_TRANSITION_REASON_EXCESSIVE_DELAY,
    QCN_TRANSITION_REASON_INSUFFICIENT_BANDWIDTH,
    QCN_TRANSITION_REASON_LOAD_BALANCE,
    QCN_TRANSITION_REASON_LOW_RSSI,
    QCN_TRANSITION_REASON_RETRANSMISSION,
    QCN_TRANSITION_REASON_INTERFERENCE,
    QCN_TRANSITION_REASON_GRAY_ZONE,
    QCN_TRANSITION_REASON_PREMIUM_AP,

/*Reserved values form 10-255 */
    QCN_TRANSITION_REASON_MAX = 255
}qcn_transition_reason_code;

/* QCN IE attribute types */
typedef enum {
    QCN_ATTRIB_VERSION                  = 0x01,
    QCN_ATTRIB_VHT_MCS10_11_SUPP        = 0X02,
    QCN_ATTRIB_HE_400NS_SGI_SUPP        = 0X03,
    QCN_ATTRIB_HE_2XLTF_160_80P80_SUPP  = 0X04,
    QCN_ATTRIB_HE_DL_OFDMA_SUPP         = 0X05,
    QCN_ATTRIB_TRANSITION_REASON        = 0x06,
    QCN_ATTRIB_TRANSITION_REJECTION     = 0x07,
    QCN_ATTRIB_HE_DL_MUMIMO_SUPP        = 0X08
}qcn_attribute_id;

/* QCN type to identify attributes to be added to QCN IE
 */
typedef enum {
    QCN_TRANS_REASON_IE_TYPE        = 1,
    QCN_MAC_PHY_PARAM_IE_TYPE       = 2
}qcn_ie_type;
#define QCN_VER_ATTR_VER              0x01
#define QCN_VER_ATTR_SUBVERSION       0x00

#define VHT_MCS10_11_SUPP_ATTRIB_LEN            1
#define HE_400NS_SGI_SUPP_ATTRIB_LEN            3
#define HE_2XLTF_160_80P80_SUPP_ATTRIB_LEN      1
#define HE_DL_OFDMA_SUPP_ATTRIB_LEN             1
#define HE_DL_MUMIMO_SUPP_ATTRIB_LEN            1

/*
 * @ieee80211_setup_qcn_ie_bstmreq_target() -  adds QCN  IE in BTM request packet
 * @vap:pointer to vap
 * @ie:location where we need to add ie
 * @bstm_reqinfo:points to structure for transition reason code passing
 * @macaddr:pointer to mac address of the node
 *
 * Return:pointer where next item should be populated
 */
u_int8_t* ieee80211_setup_qcn_ie_bstmreq_target(struct ieee80211vap *vap, u_int8_t *ie,
                                         struct ieee80211_bstm_reqinfo_target *bstm_reqinfo,u_int8_t *macaddr);

/*
 * @ieee80211_parse_qcnie() -  parse QCN  trans_reason sub-attribute
 * @frm:tracks location where to attach next sub-attribute
 * @wh:frame pointer
 * @ni:node pointer
 * @version: pointer for sending the version info to the caller

 * Return:0 on success , -1 on failure
 */
int ieee80211_parse_qcnie(u_int8_t *frm, const struct ieee80211_frame *wh,
                    struct ieee80211_node *ni,u_int8_t * data);


/*
 * @ieee80211_add_qcn_info_ie() - adds QCN IE
 * @ie:ie pointer
 * @vap :pointer to vap struct
 * @ie_len:pointer to length that will be returned
 * @bstm_reqinfo:pointer to bstm request struct

 * Return:New buffer pointer after the added IE
 */
u_int8_t *ieee80211_add_qcn_info_ie(u_int8_t *frm,
        struct ieee80211vap *vap, u_int16_t *ie_len,struct ieee80211_bstm_reqinfo_target *bstm_reqinfo);

u_int8_t *ieee80211_add_qcn_ie_mac_phy_params(u_int8_t *ie, struct ieee80211vap *vap);
#if QCN_ESP_IE
u_int8_t *ieee80211_add_esp_info_ie(u_int8_t *frm,
        struct ieee80211com *ic, u_int16_t *ie_len);

u_int8_t ieee80211_est_field_attribute(struct ieee80211com *ic, u_int8_t *frm);
#endif

#endif
#endif //QCN_IE
