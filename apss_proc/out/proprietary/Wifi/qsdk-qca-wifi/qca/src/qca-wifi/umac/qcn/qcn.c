/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

/*
* QCN  module
*/
#if QCN_IE
#include "qcn.h"

static void
ieee80211_add_qcn_ie_header_info(struct ieee80211_qcn_ie *qcn_ie)
{
    qcn_ie->header.id = WLAN_ELEMID_VENDOR;
    qcn_ie->oui[0] = (QCA_OUI & 0xff);
    qcn_ie->oui[1] = ((QCA_OUI >> 8) & 0xff);
    qcn_ie->oui[2] = ((QCA_OUI >>16) & 0xff);
    qcn_ie->oui_type = QCN_OUI_TYPE;
}
/*
 * @ieee80211_add_qcn_info_ie() - adds QCN IE
 * @ie:ie pointer
 * @vap :pointer to vap struct
 * @ie_len:pointer to length that will be returned
 * @bstm_reqinfo:pointer to bstm request struct

 * Return:New buffer pointer after the added IE
 */

u_int8_t *
ieee80211_add_qcn_info_ie(u_int8_t *ie,
                             struct ieee80211vap *vap, u_int16_t* ie_len,struct ieee80211_bstm_reqinfo_target *bstm_reqinfo)
{
    u_int8_t len = 0,*frm = NULL,*start = NULL;
    struct ieee80211_qcn_ie *qcn_ie = (struct ieee80211_qcn_ie *)ie;

    if(!qcn_ie)
        return ie;

    ieee80211_add_qcn_ie_header_info(qcn_ie);

    frm   = (u_int8_t *)&qcn_ie->opt_ie[0];
    start = frm;
    len   = ieee80211_setup_qcn_attrs(vap, &frm,
                                        QCN_TRANS_REASON_IE_TYPE, bstm_reqinfo);
    qcn_ie->header.len = len + (sizeof(struct ieee80211_qcn_ie) - sizeof(struct ieee80211_qcn_header));
    /* return the length of this ie, including element ID and length field */
    *ie_len = qcn_ie->header.len +2;
    return (u_int8_t*)(start +len);
}

/*
 * @ieee80211_add_qcn_attr_header_info() - adds QCN attribute header information
 * @frm: pointer to ie
 * @attr_id: Attribute ID
 * @attr_len: Attribute length
 * @len: length of the IE
 */
u_int8_t *
ieee80211_add_qcn_attr_header_info(u_int8_t *frm, qcn_attribute_id attr_id,
                                    u_int8_t attr_len, u_int8_t *ie_len)
{
    struct ieee80211_qcn_feat_attr *qcn_mac_phy_attr =
                                    (struct ieee80211_qcn_feat_attr *) frm;

    qcn_mac_phy_attr->header.id = attr_id;
    qcn_mac_phy_attr->header.len = attr_len;
    frm = (u_int8_t *)&qcn_mac_phy_attr->feature_mask[0];
    *ie_len += (qcn_mac_phy_attr->header.len + sizeof(struct ieee80211_qcn_header));

    return frm;
}
/*
 * @ieee80211_setup_ver_attr_ie() - adds QCN version attribute  IE
 * @frm:tracks location where to attach next sub-attribute
 *
 * Return:length of QCN version attribute
 */

u_int8_t ieee80211_setup_ver_attr_ie(u_int8_t ** frm)
{

    struct ieee80211_qcn_ver_attr *ver_attr = (struct ieee80211_qcn_ver_attr*)(*frm);
    u_int8_t len = 0;

    ver_attr->header.id   = QCN_ATTRIB_VERSION;
    ver_attr->version     = QCN_VER_ATTR_VER;
    ver_attr->sub_version = QCN_VER_ATTR_SUBVERSION;
    ver_attr->header.len  =  sizeof(struct ieee80211_qcn_ver_attr) - sizeof(struct ieee80211_qcn_header);

    *frm = (u_int8_t *)&ver_attr->opt_ie[0];
    len =  sizeof(struct ieee80211_qcn_ver_attr);
    return len;

}

/*
 * @ieee80211_setup_mac_phy_param_attrs() - adds QCN MAC/PHY attributes
 * @vap:pointer to the vap structure
 * @frm:tracks location where to attach next sub-attribute
 *
 * Return:length of all QCN attributes added
 */
u_int8_t ieee80211_setup_mac_phy_param_attrs(struct ieee80211vap *vap,
                                                u_int8_t *frm)
{
    struct ieee80211com *ic = vap->iv_ic;
    u_int8_t len = 0;

    /* Add VHT MCS10/11 attribute */
    frm = ieee80211_add_qcn_attr_header_info(frm, QCN_ATTRIB_VHT_MCS10_11_SUPP,
                                            VHT_MCS10_11_SUPP_ATTRIB_LEN, &len);
    *frm++ = (vap->iv_vht_mcs10_11_supp &&
            ic->ic_vhtcap_max_mcs.tx_mcs_set.higher_mcs_supp) ? 1 : 0;

    /* Add HE 400ns SGI support attribute */
    frm = ieee80211_add_qcn_attr_header_info(frm, QCN_ATTRIB_HE_400NS_SGI_SUPP,
                                            HE_400NS_SGI_SUPP_ATTRIB_LEN, &len);
    *frm++ = (ic->ic_he.hecap_info_internal & IEEE80211_HE_0DOT4US_IN_1XLTF_SUPP_BITS);
    *frm++ = (ic->ic_he.hecap_info_internal &
                    IEEE80211_HE_0DOT4US_IN_2XLTF_SUPP_BITS) ? 0x01 : 0;
    /* 400ns SGI with 4XLTF is not supported yet. Set corresponding value to 0*/
    *frm++ = 0;

    /* Add HE 160/80+80MHz 2X LTF support attribute */
    frm = ieee80211_add_qcn_attr_header_info(frm, QCN_ATTRIB_HE_2XLTF_160_80P80_SUPP,
                                        HE_2XLTF_160_80P80_SUPP_ATTRIB_LEN, &len);
    *frm++ = (ic->ic_he.hecap_info_internal &
                    IEEE80211_HE_2XLTF_IN_160_80P80_SUPP_BITS) ? 0x01 : 0;

    if(vap->iv_opmode == IEEE80211_M_STA) {
        /* Add DL OFDMA support attribute */
        frm = ieee80211_add_qcn_attr_header_info
                            (frm, QCN_ATTRIB_HE_DL_OFDMA_SUPP,
                             HE_DL_OFDMA_SUPP_ATTRIB_LEN, &len);
        *frm++ = vap->iv_he_dl_muofdma;

        /* Add DL MUMIMO support attribute */
        frm = ieee80211_add_qcn_attr_header_info
                            (frm, QCN_ATTRIB_HE_DL_MUMIMO_SUPP,
                             HE_DL_MUMIMO_SUPP_ATTRIB_LEN, &len);
        *frm++ = vap->iv_he_mu_bfee;
    }

    return len;
}

/*
 * @ieee80211_setup_qcn_attrs() -   adds QCN  attributes
 * @frm:tracks location where to attach next sub-attribute
 * @type:QCN IE type
 * @reqinfo:points to structure for any additional information
 *
 * Return:length of all QCN attributes added
 */
u_int8_t ieee80211_setup_qcn_attrs(struct ieee80211vap *vap, u_int8_t **frm,
                                                qcn_ie_type type, void *reqinfo)
{
    u_int8_t len = 0;
    len += ieee80211_setup_ver_attr_ie(frm);

    switch(type) {
        case QCN_TRANS_REASON_IE_TYPE:
            /* Send transaction reason code in BSS Transition Mgmt request */
            if(reqinfo) {
                len += ieee80211_setup_qcn_trans_reason_ie(vap, frm,
                        (struct ieee80211_bstm_reqinfo_target *)reqinfo);
            }
            break;

        case QCN_MAC_PHY_PARAM_IE_TYPE:
            len += ieee80211_setup_mac_phy_param_attrs(vap, *frm);
            break;

        default:
            qdf_err("%s:Support for type %d not present", __func__, type);
            break;
    }

    return len;
}
/*
 * @ieee80211_setup_qcn_ie_bstmreq_target() -  adds QCN  IE in BTM request packet
 * @vap:pointer to vap
 * @ie:location where we need to add ie
 * @bstm_reqinfo:points to structure for transition reason code passing
 * @macaddr:pointer to mac address of the node
 *
 * Return:pointer where next item should be populated
 */

u_int8_t*
ieee80211_setup_qcn_ie_bstmreq_target(struct ieee80211vap *vap, u_int8_t *ie,
                               struct ieee80211_bstm_reqinfo_target *bstm_reqinfo,
                               u_int8_t *macaddr)
{

    u_int8_t  *frm = NULL;
    u_int16_t ie_len;
    frm =  ieee80211_add_qcn_info_ie(ie,vap,&ie_len,bstm_reqinfo);
    return frm;
}


/*
 * @ieee80211_setup_qcn_trans_reason_ie() -  adds QCN  trans_reason sub-attribute
 * @vap:pointer to vap
 * @frm:tracks location where to attach next sub-attribute
 * @bstm_reqinfo:points to structure for transition reason code passing
 *
 * Return:length of attribute added
 */


u_int8_t
ieee80211_setup_qcn_trans_reason_ie(struct ieee80211vap *vap,u_int8_t **frm,struct ieee80211_bstm_reqinfo_target *bstm_reqinfo)
{
    struct ieee80211_qcn_transit_reason_code *trans_reason_code_ie = (struct ieee80211_qcn_transit_reason_code *)(*frm);
    u_int8_t len = 0;
    qcn_transition_reason_code trans_reason;
    trans_reason = (qcn_transition_reason_code)bstm_reqinfo->qcn_trans_reason;

    if (trans_reason <= QCN_TRANSITION_REASON_PREMIUM_AP) {
        trans_reason_code_ie->header.id = QCN_ATTRIB_TRANSITION_REASON;
        trans_reason_code_ie->header.len  = (sizeof(struct ieee80211_qcn_transit_reason_code) - sizeof(struct ieee80211_qcn_header));
        trans_reason_code_ie->reason_code = trans_reason;
        *frm = (u_int8_t *)&trans_reason_code_ie->opt_ie[0];
        len = sizeof(struct ieee80211_qcn_transit_reason_code);
    }
    return (u_int8_t)(len);

}

/*
 * @ieee80211_peer_vht_mcs_10_11_supp() - parses VHT MCS10/11 cap from peer caps
 * @ni:Node structure for the connected peer
 * @value:Support for VHT MCS10/11
 *
 */
static void
ieee80211_peer_vht_mcs_10_11_supp(struct ieee80211_node *ni, uint8_t value)
{
    struct ieee80211com *ic = ni->ni_ic;
    uint8_t self_supp = ic->ic_vhtcap_max_mcs.tx_mcs_set.higher_mcs_supp;
    uint16_t peer_mcsnssmap = ni->ni_tx_vhtrates;

    if(value && ni->ni_vap->iv_vht_mcs10_11_supp &&
            ic->ic_vhtcap_max_mcs.tx_mcs_set.higher_mcs_supp) {
        VHT_INTRSCTD_SS_MCS10_11_CAP(self_supp, peer_mcsnssmap,
                                        ni->ni_higher_vhtmcs_supp);
    } else {
        ni->ni_higher_vhtmcs_supp = 0;
    }
}

/*
 * @ieee80211_add_qcn_ie_mac_phy_params() - adds the QCN IE for MAC/PHY params
 * @ie:pointer to track where to attach next IE
 * @vap:pointer to the vap structure
 *
 * Return - pointer to updated frame
 */
u_int8_t *
ieee80211_add_qcn_ie_mac_phy_params(u_int8_t *ie, struct ieee80211vap *vap)
{
    struct ieee80211_qcn_ie *qcn_mac_phy_ie = (struct ieee80211_qcn_ie *)ie;
    u_int8_t len = 0, *frm = NULL, *start = NULL;

    ieee80211_add_qcn_ie_header_info(qcn_mac_phy_ie);

    frm = (u_int8_t *)&qcn_mac_phy_ie->opt_ie[0];
    start = frm;
    len = ieee80211_setup_qcn_attrs(vap, &frm, QCN_MAC_PHY_PARAM_IE_TYPE, NULL);
    qcn_mac_phy_ie->header.len = len + (sizeof(struct ieee80211_qcn_ie) -
            sizeof(struct ieee80211_qcn_header));
    return (u_int8_t *)(start + len);
}

/*
 * @ieee80211_parse_qcnie() -  parse QCN  trans_reason sub-attribute
 * @frm:tracks location where to attach next sub-attribute
 * @wh:frame pointer
 * @ni:node pointer
 * @data: pointer for sending the version info to the caller

 * Return:0 on success , -1 on failure
 */
int
ieee80211_parse_qcnie(u_int8_t *frm, const struct ieee80211_frame *wh,
        struct ieee80211_node *ni,u_int8_t * data)
{

    struct ieee80211com *ic = ni->ni_ic;
    u_int len = frm[1];
    u_int updated_len = 0;
    u_int slen,valid_id =1;
    u_int attribute_id;
    u_int attribute_len;
    bool dlofdma_supp = false, dlmumimo_supp = false;
    if(len < QCN_IE_MIN_LEN) {
        IEEE80211_DISCARD_IE(ni->ni_vap,
                IEEE80211_MSG_ELEMID ,
                "QCN_IE", "too short, len %u", len);
        return -1;
    }


    if (len == QCN_IE_MIN_LEN) {
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_MLME," QCN_IE only one attribute present \n");
    }
    slen = len - 4;

    /*
     * Incrementing frm by 6 so that it will point to the qcn attribute id
     * 1 byte for element id
     * 1 byte for length
     * 3 bytes for QCN OUI
     * 1 byte for QCN OUI type
     */

    frm = frm + 6;
    /* if we get a call from probe request ,self node, populate the data and return */
    if(data) {
        attribute_id = *frm++;
        if(attribute_id == QCN_ATTRIB_VERSION) {
            attribute_len = *frm++;
            data[0] = *frm++; // version
            data[1] = *frm++; // sub-versoin
        }
    }
    else {
        while(slen > updated_len && valid_id)
        {
            attribute_id = *frm++;
            switch(attribute_id){
                case QCN_ATTRIB_VERSION:
                    attribute_len = *frm++;
                    ni->ni_qcn_version_flag = *frm++;
                    ni->ni_qcn_subver_flag = *frm++;
                    updated_len += (attribute_len +
                                sizeof(struct ieee80211_qcn_header));
                    break;
                case QCN_ATTRIB_TRANSITION_REJECTION:
                    attribute_len = *frm++;
                    ni->ni_qcn_tran_rej_code = *frm++;
                    updated_len += (attribute_len +
                                sizeof(struct ieee80211_qcn_header));
                    break;
                case QCN_ATTRIB_VHT_MCS10_11_SUPP:
                    attribute_len = *frm++;
                    ieee80211_peer_vht_mcs_10_11_supp(ni, *frm);
                    frm = frm + 1;
                    updated_len += (attribute_len +
                                sizeof(struct ieee80211_qcn_header));
                    break;
                case QCN_ATTRIB_HE_400NS_SGI_SUPP:
                    attribute_len = *frm++;
                    ni->ni_he.hecap_info_internal |=
                            ((*frm++ << IEEE80211_HE_0DOT4US_IN_1XLTF_SUPP_S) &
                                            ic->ic_he.hecap_info_internal);
                    ni->ni_he.hecap_info_internal |=
                            ((*frm++ << IEEE80211_HE_0DOT4US_IN_2XLTF_SUPP_S) &
                                            ic->ic_he.hecap_info_internal);
                    /* Support for 400ns SGI with 4XLTF is not yet supported*/
                    frm = frm + 1;
                    updated_len += (attribute_len +
                                sizeof(struct ieee80211_qcn_header));
                    break;
                case QCN_ATTRIB_HE_2XLTF_160_80P80_SUPP:
                    attribute_len = *frm++;
                    ni->ni_he.hecap_info_internal |=
                            ((*frm++ << IEEE80211_HE_2XLTF_IN_160_80P80_SUPP_S) &
                                            ic->ic_he.hecap_info_internal);
                    updated_len += (attribute_len +
                                sizeof(struct ieee80211_qcn_header));
                    break;
                case QCN_ATTRIB_HE_DL_OFDMA_SUPP:
                    attribute_len = *frm++;
                    dlofdma_supp = *frm++;
                    if(dlofdma_supp) {
                        ni->ni_he.hecap_info_internal |=
                                (IEEE80211_HE_DL_MU_SUPPORT_ENABLE <<
                                 IEEE80211_HE_DL_OFDMA_SUPP_S);
                    } else {
                        ni->ni_he.hecap_info_internal |=
                                (IEEE80211_HE_DL_MU_SUPPORT_DISABLE <<
                                 IEEE80211_HE_DL_OFDMA_SUPP_S);
                    }
                    updated_len += (attribute_len +
                                sizeof(struct ieee80211_qcn_header));
                    break;
                case QCN_ATTRIB_HE_DL_MUMIMO_SUPP:
                    attribute_len = *frm++;
                    dlmumimo_supp = *frm++;
                    if(dlmumimo_supp) {
                        ni->ni_he.hecap_info_internal |=
                                (IEEE80211_HE_DL_MU_SUPPORT_ENABLE <<
                                 IEEE80211_HE_DL_MUMIMO_SUPP_S);
                    } else {
                        ni->ni_he.hecap_info_internal |=
                                (IEEE80211_HE_DL_MU_SUPPORT_DISABLE <<
                                 IEEE80211_HE_DL_MUMIMO_SUPP_S);
                    }
                    updated_len += (attribute_len +
                                sizeof(struct ieee80211_qcn_header));
                    break;
                default:
                    qdf_err("Warning:-QCN_IE attribute %d invalid or not defined yet \n",attribute_id);
                    valid_id = 0;
                    break;
            }
        }
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_MLME,
                "QCN_IE Version|0x%x|0x%x|, reject code |0x%x| \n"
                "Q2Q Feature Capability info: VHT MCS10//11 supp: 0x%x, "
                "HE 400ns SGI support(B0-B1) | HE 2X LTF supp for 160/80+80(B2) | "
                "HE DLOFDMA support(B3) | HE DL MUMIMO support(B4): 0x%x \n",
                ni->ni_qcn_version_flag, ni->ni_qcn_subver_flag,
                ni->ni_qcn_tran_rej_code, ni->ni_higher_vhtmcs_supp,
                ni->ni_he.hecap_info_internal);
    }

return EOK;
}

#if QCN_ESP_IE
uint8_t ieee80211_est_field_attribute(struct ieee80211com *ic, uint8_t *frm)
{
    uint8_t len;
    int32_t temp;
    struct ieee80211_esp_info_field *esp_field = (struct ieee80211_esp_info_field*)(frm);

    /**
     * Bit 0-1 - Access Category - BE
     * Bit 2   - Reserved
     * Bit 3-4 - Data Format - AMPDU and AMSDU enabled
     * Bit 5-7 - BA Window Size of 16
     */
    esp_field->esp_info_field_head = 0x1;
    esp_field->esp_info_field_head |= (0x3 << 3);

    if (ic->ic_esp_ba_window) {
        /* BA Window Size as entered by user */
        esp_field->esp_info_field_head |= (ic->ic_esp_ba_window << 5);
    } else {
        /* BA Window Size of 16 */
        esp_field->esp_info_field_head |= (0x5 << 5);
    }

    if (ic->ic_esp_air_time_fraction) {
        esp_field->air_time_fraction = ic->ic_esp_air_time_fraction;
    } else {
        /* AC_BE : Bit[0-7] */
        temp = (ic->ic_fw_esp_air_time) & (0xFF);
        temp = (temp * 255) / 100;
        /* Airtime as an integer value */
        esp_field->air_time_fraction = temp;
    }

    if (ic->ic_esp_ppdu_duration) {
        esp_field->ppdu_duration_target = ic->ic_esp_ppdu_duration;
    } else {
        /* Default : 250us PPDU Duration */
        esp_field->ppdu_duration_target = DEFAULT_PPDU_DURATION;
    }
    len = sizeof(struct ieee80211_esp_info_field);

    return len;
}

uint8_t* ieee80211_add_esp_info_ie(uint8_t *ie, struct ieee80211com *ic, uint16_t* ie_len)
{
    uint8_t len = 0, *frm = NULL, *start = NULL;
    struct ieee80211_est_ie *est_ie = (struct ieee80211_est_ie *)ie;

    if (!est_ie)
        return ie;

    est_ie->element_id = IEEE80211_ELEMID_EXTN;
    est_ie->element_id_extension = IEEE80211_ELEMID_EXT_ESP_ELEMID_EXTENSION;
    frm = (uint8_t *)&est_ie->esp_information_list[0];
    start = frm;
    len = ieee80211_est_field_attribute(ic, frm);
    est_ie->len = len + sizeof(struct ieee80211_est_ie) - 2;
    *ie_len = est_ie->len + 2;

    return (uint8_t*)(start + len);
}
#endif /* QCN_ESP_IE */
#endif /* QCN_IE */
