/*
 * Copyright (c) 2016-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

/*
 MBO  module
*/

#include "ieee80211_mbo_priv.h"
#include "ieee80211_regdmn.h"
#include "ieee80211_proto.h"
#include <wlan_vdev_mgr_utils_api.h>
#ifdef WLAN_SUPPORT_FILS
#include <wlan_fd_utils_api.h>
#endif /* WLAN_SUPPORT_FILS */

#if ATH_SUPPORT_MBO

/**
 * @brief Verify that the mbo handle is valid
 *
 * @param [in] VAP  the handle to the
 *
 * @return true if handle is valid; otherwise false
 */

static INLINE bool ieee80211_mbo_is_valid(const struct ieee80211vap *vap)
{
    return vap && vap->mbo;
}

/**
 * @brief Determine whether the VAP handle is valid,
 *        has a valid mbo handle,
 *        is operating in a mode where MBO is relevant,
 *        and is not in the process of being deleted.
 *
 * @return true if the VAP is valid; otherwise false
 */

bool ieee80211_mbo_is_vap_valid(const struct ieee80211vap *vap)
{
    /* Unfortunately these functions being used do not take a const pointer
       even though they do not modify the VAP. Thus, we have to cast away
       the const-ness in order to call them.*/

    struct ieee80211vap *nonconst_vap = (struct ieee80211vap *) vap;

    return vap && wlan_vap_get_opmode(nonconst_vap) == IEEE80211_M_HOSTAP &&
           !ieee80211_vap_deleted_is_set(nonconst_vap);
}

/**
 * @brief
 * @param [in]
 * @param [out]
 */

bool ieee80211_vap_mbo_check(struct ieee80211vap *vap)
{
    if((ieee80211_mbo_is_vap_valid(vap)
        && ieee80211_mbo_is_valid(vap))
        && ieee80211_vap_mbo_is_set(vap))
        return true;
    else
        return false;
}

/**
 * @brief
 * @param [in]
 * @param [out]
 */
bool ieee80211_vap_mbo_assoc_status(struct ieee80211vap *vap)
{
    if (vap->mbo->usr_assoc_disallow)
        return true;
    else
        return false;
}
void ieee80211_mbo_help(u_int32_t param)
{
    switch(param) {
        case MBO_ATTRIB_CAP_INDICATION:
            {
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " 0th bit- reserved\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " 1st bit- Non-preferred channel report capability\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " 2nd bit- MBO Cellular capabilities\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " 3rd bit- Association disallowed capability\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " 4th bit- Cellular data link request capability\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " 5th-7th bits- reserved\n");
            }
            break;
        case MBO_ATTRIB_ASSOCIATION_DISALLOWED:
            {
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " value 0- reserved\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " value 1- Unspecified reason\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " value 2- Max no. of associated STAs reached\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " value 3- Air interface overloaded\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " value 4- Authentication server overloaded\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " value 5- Insufficient RSSI\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " value 6-255- reserved\n");
            }
            break;
        case MBO_ATTRIB_TRANSITION_REASON:
            {
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " Code 0- unspecified\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " Code 1- Excessive frame loss rate\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " Code 2- Excessive delay for current traffic stream\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " Code 3- Insufficient bandwidth for current traffic stream\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " Code 4- Load balancing\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " Code 5- Low RSSI\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " Code 6- Received excessive number of retransmissions\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " Code 7- High interference\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " Code 8- Gray zone\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " Code 9- Transitioning to premium AP\n");
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " Code 10-255- reserved\n");
            }
            break;
    }
    return;
}
/**
 * set simple configuration parameter.
 *
 * @param devhandle       : handle to the vap.
 * @param param           : config paramter
 * @param val             : value of the parameter.
 * @return 0  on success and -ve on failure.
 */

int wlan_set_mbo_param(wlan_if_t vap, u_int32_t param, u_int32_t val)
{
    int retv = EOK;
    ieee80211_mbo_t mbo = NULL;

    if(!ieee80211_mbo_is_vap_valid(vap))
        return EINVAL;

    if (!ieee80211_mbo_is_valid(vap))
        return EINVAL;

    mbo = vap->mbo;

    switch(param) {
    case IEEE80211_MBO:
        {
            if(val)
                ieee80211_vap_mbo_set(vap);
            else {
                ieee80211_vap_mbo_clear(vap);
            }
        }
        break;
    case IEEE80211_MBOCAP:
        {
            if (ieee80211_vap_mbo_is_set(vap) ) {
                if(val & MBO_CAP_AP_CELLULAR)
                    mbo->usr_mbocap_cellular_capable =  1;
                else
                    mbo->usr_mbocap_cellular_capable =  0;
            } else {
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "Need to enable MBO Before setting capability \n");
                retv = EINVAL;
            }
        }
        break;
    case IEEE80211_MBO_ASSOC_DISALLOW:
        {
#define MBO_MAX_ASSOC_DISALLOW 0x06
            if (!ieee80211_vap_mbo_is_set(vap)) {
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "MBO not enabled \n");
                retv = EINVAL;
            } else if (val < MBO_MAX_ASSOC_DISALLOW) {
                mbo->usr_assoc_disallow = (u_int8_t)val;
                ieee80211vap_set_flag_ext2(vap, IEEE80211_FEXT2_MBO);
            } else {
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "__Invalid argument __investigate__ \n");
                ieee80211_mbo_help(MBO_ATTRIB_ASSOCIATION_DISALLOWED);
                retv = EINVAL;
            }
        }
#undef MBO_MAX_ASSOC_DISALLOW
        break;
    case IEEE80211_MBO_CELLULAR_PREFERENCE:
        if ((val <= MBO_CELLULAR_PREFERENCE_NOT_USE) ||
            (val == MBO_CELLULAR_PREFERENCE_USE)) /* as per specification */
            mbo->usr_cell_pref = val;
        else
            retv = EINVAL;
        break;
    case IEEE80211_MBO_TRANSITION_REASON:
        if (val <= MBO_TRANSITION_REASON_PREMIUM_AP ) /* as per specification */
           mbo->usr_trans_reason = val;
        else
            retv = EINVAL;
        break;
    case IEEE80211_MBO_ASSOC_RETRY_DELAY:
        mbo->usr_assoc_retry = val;
        break;
    default:
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "Invalid configuration __investigate %s \n",__func__);
        retv = EINVAL;
    }
    return retv;
}


/**
 * get simple configuration parameter.
 *
 * @param devhandle       : handle to the vap.
 * @param param           : paramter.
 * @return value of the parameter.
 */

u_int32_t wlan_get_mbo_param(wlan_if_t vap, u_int32_t param)
{
    u_int32_t retv = 0;
    ieee80211_mbo_t mbo = NULL;

    if(!ieee80211_mbo_is_vap_valid(vap))
        return EINVAL;

    if (!ieee80211_mbo_is_valid(vap))
        return EINVAL;

    mbo = vap->mbo;

    switch(param) {
    case IEEE80211_MBO:
        {
            retv = ieee80211_vap_mbo_is_set(vap);
        }
        break;
    case IEEE80211_MBOCAP:
        {
            if (mbo->usr_mbocap_cellular_capable) {
                retv |= MBO_CAP_AP_CELLULAR;
            }
        }
        break;
    case IEEE80211_MBO_ASSOC_DISALLOW:
        {
            retv = mbo->usr_assoc_disallow;
        }
        break;
    case IEEE80211_MBO_CELLULAR_PREFERENCE:
        return mbo->usr_cell_pref;
    case IEEE80211_MBO_TRANSITION_REASON:
        return mbo->usr_trans_reason;
    case IEEE80211_MBO_ASSOC_RETRY_DELAY:
        return mbo->usr_assoc_retry;
    default:
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "Invalid configuration __investigate %s \n",__func__);
        return EINVAL;;
    }
    return retv;
}

/**
 * @brief Initialize the MBO infrastructure.
 *
 * @param [in] vap  to initialize
 *
 * @return EOK on success; EINPROGRESS if band steering is already initialized
 *         or ENOMEM on a memory allocation failure
 */

int ieee80211_mbo_vattach(struct ieee80211vap *vap)
{

    ieee80211_mbo_t mbo = NULL;

    if(!ieee80211_mbo_is_vap_valid(vap))
        return EINVAL;

    if(vap->mbo)
        return EINPROGRESS;

    mbo = (ieee80211_mbo_t)
        OS_MALLOC(vap->iv_ic->ic_osdev, sizeof(struct ieee80211_mbo), 0);

    if(NULL == mbo) {
        return -ENOMEM;
    }

    OS_MEMZERO(mbo,sizeof(struct ieee80211_mbo));
    mbo->mbo_osdev = vap->iv_ic->ic_osdev;
    mbo->mbo_ic = vap->iv_ic;
    vap->mbo = mbo;

    QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "MBO Initialized");

    return EOK;
}

/**
 * @brief deint the MBO infrastructure.
 *
 * @param [in] vap  to deint
 *
 * @return EOK on success;
 */

int ieee80211_mbo_vdetach(struct ieee80211vap *vap)
{
    if (!ieee80211_mbo_is_valid(vap))
        return EINVAL;

    OS_FREE(vap->mbo);
    vap->mbo = NULL;

    QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "%s: MBO terminated\n", __func__);
    return EOK;
}

/**
 * @brief
 * @param [in]
 * @param [out]
 */

static inline u_int8_t ieee80211_mbo_fill_header(struct ieee80211_mbo_header *header,u_int8_t type,u_int8_t len)
{
    header->ie = type;
    header->len = len;
    return((u_int8_t)sizeof(struct ieee80211_mbo_header));
}

/**
 * @brief
 * @param [in]
 * @param [out]
 */

u_int8_t ieee80211_setup_assoc_disallow_ie(struct ieee80211vap *vap,u_int8_t *frm,u_int8_t reason)
{
    struct ieee80211_assoc_disallow *assoc_ie  = (struct ieee80211_assoc_disallow *)frm;
    u_int8_t len = 0;
    len = ieee80211_mbo_fill_header(&(assoc_ie->header),MBO_ATTRIB_ASSOCIATION_DISALLOWED,0);

    assoc_ie->reason_code = reason;
    assoc_ie->header.len = sizeof(struct ieee80211_assoc_disallow) - len;
    frm = (u_int8_t *)(&assoc_ie->opt_ie[0]);
    return (u_int8_t)sizeof(struct ieee80211_assoc_disallow);
}

/**
 * @brief
 * @param [in]
 * @param [out]
 */

u_int8_t ieee80211_setup_assoc_retry_del_ie(struct ieee80211vap *vap,u_int8_t *frm,
                                          struct ieee80211_bstm_reqinfo *bstm_reqinfo)
{
    struct ieee80211_assoc_retry_delay *assoc_re_del_ie = (struct ieee80211_assoc_retry_delay *)frm;
    ieee80211_mbo_t mbo = NULL;
    u_int8_t len = 0;
    if ( vap ) {
        mbo = vap->mbo;
    }
    if (mbo == NULL){
        return len;
    }

    len = ieee80211_mbo_fill_header(&(assoc_re_del_ie->header),MBO_ATTRIB_ASSOC_RETRY_DELAY,0);
    assoc_re_del_ie->header.len = (sizeof(struct ieee80211_assoc_retry_delay) - len);
    assoc_re_del_ie->re_auth_delay = cpu_to_le16(mbo->usr_assoc_retry);
    return (u_int8_t)(sizeof(struct ieee80211_assoc_retry_delay));
}


/**
 * @brief
 * @param [in]
 * @param [out]
 */

u_int8_t ieee80211_setup_trans_reason_ie(struct ieee80211vap *vap,u_int8_t *frm,struct ieee80211_bstm_reqinfo *bstm_reqinfo)
{
    struct ieee80211_transit_reason_code *trans_reason_code_ie = (struct ieee80211_transit_reason_code *)frm;
    ieee80211_mbo_t mbo = NULL;
    u_int8_t len = 0,len_next_attr = 0;
    if ( vap ) {
        mbo = vap->mbo;
    }
    if (mbo == NULL){
        return len;
    }
    if (mbo->usr_trans_reason <= MBO_TRANSITION_REASON_PREMIUM_AP) {
        len = ieee80211_mbo_fill_header(&(trans_reason_code_ie->header),MBO_ATTRIB_TRANSITION_REASON ,0 );
        trans_reason_code_ie->header.len = (sizeof(struct ieee80211_transit_reason_code) - len);
        trans_reason_code_ie->reason_code = mbo->usr_trans_reason;
        frm = (u_int8_t *)&trans_reason_code_ie->opt_ie[0];
        len = sizeof(struct ieee80211_transit_reason_code);
    }
    if(!bstm_reqinfo->bssterm_inc)
        len_next_attr = ieee80211_setup_assoc_retry_del_ie(vap,frm,bstm_reqinfo);
    return (u_int8_t)(len_next_attr + len);

}

u_int8_t ieee80211_setup_trans_reason_ie_target(struct ieee80211vap *vap,u_int8_t *frm,struct ieee80211_bstm_reqinfo_target *bstm_reqinfo)
{
    struct ieee80211_transit_reason_code *trans_reason_code_ie = (struct ieee80211_transit_reason_code *)frm;
    u_int8_t len = 0;

    if (bstm_reqinfo->trans_reason <= IEEE80211_BSTM_REQ_REASON_INVALID) {
        len = ieee80211_mbo_fill_header(&(trans_reason_code_ie->header),MBO_ATTRIB_TRANSITION_REASON ,0 );
        trans_reason_code_ie->header.len = (sizeof(struct ieee80211_transit_reason_code) - len);
        trans_reason_code_ie->reason_code = bstm_reqinfo->trans_reason;
        frm = (u_int8_t *)&trans_reason_code_ie->reason_code;
        len = sizeof(struct ieee80211_transit_reason_code);
    }

    return len;
}


/**
 * @brief
 * @param [in]
 * @param [out]
 */

u_int8_t ieee80211_setup_cell_pref_ie(struct ieee80211vap *vap,u_int8_t *frm,struct ieee80211_bstm_reqinfo *bstm_reqinfo)
{
    struct ieee80211_cell_data_conn_pref *cell_pref_ie = (struct ieee80211_cell_data_conn_pref *)frm;
    ieee80211_mbo_t mbo = NULL;
    u_int8_t len = 0,len_next_attr = 0;
    /*
     * This ie should be sent only when cellular
     * capabilities are supported by the station.
     */

    if ( vap ) {
        mbo = vap->mbo;
    }
    if(mbo == NULL){
        return len;
    }

    len = ieee80211_mbo_fill_header(&(cell_pref_ie->header),MBO_ATTRIB_CELLULAR_PREFERENCE ,0 );
    frm = (u_int8_t *)&cell_pref_ie->opt_ie[0];
    len_next_attr = ieee80211_setup_trans_reason_ie(vap,frm,bstm_reqinfo);
    cell_pref_ie->header.len = (sizeof(struct ieee80211_cell_data_conn_pref)-len);
    cell_pref_ie->cell_pref  = mbo->usr_cell_pref;
    return (u_int8_t)(len_next_attr + sizeof(struct ieee80211_cell_data_conn_pref));

}
/**
 * @brief
 * @param [in]
 * @param [out]
 */

u_int8_t ieee80211_setup_mbo_cap_ie(u_int8_t *frm, u_int8_t cellular_aware)
{
    struct ieee80211_mbo_cap *cap = NULL;

    cap = (struct ieee80211_mbo_cap *)frm;
    OS_MEMSET(cap,0,sizeof(struct ieee80211_mbo_cap));
    ieee80211_mbo_fill_header(&(cap->header),
                                    MBO_ATTRIB_CAP_INDICATION,0);

    cap->cap_cellular = cellular_aware; /*Broadcasting this value in frame */
    cap->header.len =  sizeof(struct ieee80211_mbo_cap) -
                       sizeof(struct ieee80211_mbo_header);

    return sizeof(struct ieee80211_mbo_cap);
}

/**
 * @brief
 * @param [in]
 * @param [out]
 */

u_int8_t ieee80211_setup_mbo_attrs(struct ieee80211vap *vap,u_int8_t *frm)
{
    ieee80211_mbo_t mbo = NULL;
    u_int8_t *start = frm;

    /* mbo check already done  in caller */

    mbo = vap->mbo;

	frm += ieee80211_setup_mbo_cap_ie(frm, mbo->usr_mbocap_cellular_capable);
    if(mbo->usr_assoc_disallow) {
        frm += ieee80211_setup_assoc_disallow_ie(vap,frm,
                                                 mbo->usr_assoc_disallow);
    }
    return (frm - start);
}

/**
 * @brief
 * @param [in]
 * @param [out]
 */

u_int8_t*
ieee80211_setup_mbo_ie_bstmreq(struct ieee80211vap *vap, u_int8_t *ie,
                               struct ieee80211_bstm_reqinfo *bstm_reqinfo,
                               u_int8_t *macaddr)
{

    u_int8_t len = 0,*frm = NULL;
    struct ieee80211_mbo_ie *mbo_ie = (struct ieee80211_mbo_ie *)ie;
    struct ieee80211_node *ni = NULL;

    if(!mbo_ie)
        return ie;

    ieee80211_mbo_fill_header(&(mbo_ie->header),IEEE80211_ELEMID_VENDOR,0);
    mbo_ie->oui[0] = 0x50;
    mbo_ie->oui[1] = 0x6f;
    mbo_ie->oui[2] = 0x9a;
    mbo_ie->oui_type = MBO_OUI_TYPE;

    frm = (u_int8_t *)&mbo_ie->opt_ie[0];

    /*
     * If cell cap not supported by STA don't send cell_pref_ie
     * To find out if cell cap is supported by this macaddr compare
     * macaddr to macaddr of all STAs connected to vap and when it
     * matches check cell cap description within mbo_attributes structure
     */
     ni = ieee80211_find_node(&vap->iv_ic->ic_sta, macaddr);

     if(!ni)
         return frm;

    if (ni->ni_mbo.cellular_cap == 0x01 )
        len = ieee80211_setup_cell_pref_ie(vap,frm,bstm_reqinfo);
    else
        len = ieee80211_setup_trans_reason_ie ( vap, frm, bstm_reqinfo );

    mbo_ie->header.len = len + (sizeof(struct ieee80211_mbo_ie) - sizeof(struct ieee80211_mbo_header));

    ieee80211_free_node(ni);

    return (frm +len);
}

u_int8_t*
ieee80211_setup_mbo_ie_bstmreq_target(struct ieee80211vap *vap, u_int8_t *ie,
                               struct ieee80211_bstm_reqinfo_target *bstm_reqinfo,
                               u_int8_t *macaddr)
{

    u_int8_t len = 0,*frm = NULL;
    struct ieee80211_mbo_ie *mbo_ie = (struct ieee80211_mbo_ie *)ie;
    struct ieee80211_node *ni = NULL;

    if(!mbo_ie)
        return ie;

    ieee80211_mbo_fill_header(&(mbo_ie->header),IEEE80211_ELEMID_VENDOR,0);
    mbo_ie->oui[0] = MBO_OUI & 0xFF;
    mbo_ie->oui[1] = (MBO_OUI >> 8) & 0xFF;
    mbo_ie->oui[2] = (MBO_OUI >> 16) & 0xFF;
    mbo_ie->oui_type = MBO_OUI_TYPE;

    frm = (u_int8_t *)&mbo_ie->opt_ie[0];

    /*
     * If cell cap not supported by STA don't send cell_pref_ie
     * To find out if cell cap is supported by this macaddr compare
     * macaddr to macaddr of all STAs connected to vap and when it
     * matches check cell cap description within mbo_attributes structure
     */
     ni = ieee80211_find_node(&vap->iv_ic->ic_sta, macaddr);

     if(!ni)
         return frm;

    len = ieee80211_setup_trans_reason_ie_target( vap, frm, bstm_reqinfo );
    mbo_ie->header.len = len + (sizeof(struct ieee80211_mbo_ie) - sizeof(struct ieee80211_mbo_header));
    return (frm +len);
}


/**
 * @brief
 * @param [in]
 * @param [out]
 */

/*
 * Parse the MBO ie for non-preferred channel report attribute and
 * Cellular capabilities attirbute
 */

int
ieee80211_parse_mboie(u_int8_t *frm, struct ieee80211_node *ni)
{
    u_int len = frm[1];
    u_int non_preferred_channel_attribute_len;
    u_int num_chan = 0, non_pref_attribute = 0;
    u_int updated_len = 0;
    u_int slen;
    u_int  attribute_id;
    u_int  attribute_len;
    u_int oce_sta = 0;
#define MBO_IE_MIN_LEN 4
    if(len < MBO_IE_MIN_LEN){
        IEEE80211_DISCARD_IE(ni->ni_vap,
                IEEE80211_MSG_ELEMID | IEEE80211_MSG_MBO,
                "MBO IE", "too short, len %u", len);
        return -1;
    }

    /* no non-preferred channel report attribute and cellular
       capabilities attribute present*/

    if (len == MBO_IE_MIN_LEN){
        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, " No attribute id present \n");
        goto end;
    }
    slen = len - 4;
    /*
     * Incrementing frm by 6 so that it will point to the mbo attribute id
     * 1 byte for element id
     * 1 byte for length
     * 3 bytes for MBO OUI
     * 1 byte for MBO OUI type
     */
    frm = frm + 6;

    while(slen > updated_len)
    {
        attribute_id = *frm++;
        switch(attribute_id){
            case MBO_ATTRIB_CELLULAR:
                {
                    attribute_len = *frm++;
                    ni->ni_mbo.cellular_cap = *frm++;
                }
                updated_len += attribute_len + 2;
                break;
            case MBO_ATTRIB_NON_PREFERRED_CHANNEL:
                {
                    non_preferred_channel_attribute_len = *frm++;
                    ni->ni_mbo.num_attr = 0;
                    if (non_preferred_channel_attribute_len == 0){
                        QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "No non preferred channel report \n");
                    }
                    else
                    {
                        if(non_pref_attribute < IEEE80211_MBO_NUM_NONPREF_CHAN_ATTR){
                           OS_MEMSET(ni->ni_mbo.channel[non_pref_attribute].channels,0,
                                     sizeof(ni->ni_mbo.channel[non_pref_attribute].channels));
                            ni->ni_mbo.channel[non_pref_attribute].operating_class = *frm++;
                            /* Find out # of channels by excluding op_class, preference, reason_code (3 bytes) */
                            ni->ni_mbo.channel[non_pref_attribute].num_channels =
                                non_preferred_channel_attribute_len - MBO_ATTRIB_NON_CHANNEL_LEN;
                            for(num_chan = 0; num_chan < ni->ni_mbo.channel[non_pref_attribute].num_channels; num_chan++)
                            {
                                ni->ni_mbo.channel[non_pref_attribute].channels[num_chan] = *frm++;
                            }
                            ni->ni_mbo.channel[non_pref_attribute].channels_preference = *frm++;
                            ni->ni_mbo.channel[non_pref_attribute].reason_preference   = *frm++;
                            non_pref_attribute++;
                            ni->ni_mbo.num_attr = non_pref_attribute;
                        }
                        else {
                            frm +=len;
                        }
                    }
                }
                updated_len += non_preferred_channel_attribute_len + 2;
                break;
            case MBO_ATTRIB_TRANSITION_REJECTION:
                attribute_len = *frm++;
                ni->ni_mbo.trans_reject_code = *frm++;
                updated_len += attribute_len + 2;
                break;
            case OCE_ATTRIB_CAP_INDICATION:
                attribute_len = *frm++;
                oce_sta = 1;
                ni->ni_mbo.oce_cap = *frm++;
                updated_len += attribute_len + 2;
                break;
            default:
                break;
        }
    }
    ni->ni_mbo.oce_sta = oce_sta;

end:
#undef MBO_IE_MIN_LEN
    return EOK;
}

/**
 * @brief
 * @param [in]
 * @param [out]
 */

 /*
  * parse the WNM Notification request frame for mbo ie
  */
int ieee80211_parse_wnm_mbo_subelem(u_int8_t *frm, struct ieee80211_node *ni)
{
    u_int len = frm[1];
    u_int num_chan, non_pref_attribute = 0;
    u_int8_t attribute_id;
    u_int8_t *sfrm;
#define MBO_IE_MIN_LEN 4

    if(len < MBO_IE_MIN_LEN){
        IEEE80211_DISCARD_IE(ni->ni_vap,
                IEEE80211_MSG_ELEMID | IEEE80211_MSG_MBO,
                "MBO IE", "too short, len %u", len);
        return -1;
    }
    /*
     * Incrementing frm by 5 will make it point to the mbo attribute subelement
     * 1 byte for subelement id
     * 1 byte for length
     * 3 bytes for MBO OUI
     */
    sfrm = frm;
    while ((sfrm[0] == IEEE80211_ELEMID_VENDOR) &&
           (sfrm[2] == (MBO_OUI >>  0 & 0xFF)) &&
           (sfrm[3] == (MBO_OUI >>  8 & 0xFF)) &&
           (sfrm[4] == (MBO_OUI >> 16 & 0xFF))) {
        frm = sfrm + 5;
        attribute_id = *frm++;
        if (attribute_id == MBO_ATTRIB_NON_PREFERRED_CHANNEL) {
            // Non preferred channel report sub-element
            len = *(sfrm +1);
            if (len == 4) {
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "No non preferred channels list is present \n");
                ni->ni_mbo.num_attr = 0;
            }
            else {
                if(non_pref_attribute < IEEE80211_MBO_NUM_NONPREF_CHAN_ATTR){
                    OS_MEMSET(ni->ni_mbo.channel[non_pref_attribute].channels,0,
                             sizeof(ni->ni_mbo.channel[non_pref_attribute].channels));
                    ni->ni_mbo.channel[non_pref_attribute].operating_class = *frm++;
                    /* Find out # of channels by excluding oui, oui_type, op_class, preference, reason_code (7 bytes) */
                    ni->ni_mbo.channel[non_pref_attribute].num_channels = (len < MBO_SUBELM_NON_CHANNEL_LEN) ?
                        0 : len - MBO_SUBELM_NON_CHANNEL_LEN;
                    for(num_chan = 0; num_chan < ni->ni_mbo.channel[non_pref_attribute].num_channels; num_chan++)
                    {
                        ni->ni_mbo.channel[non_pref_attribute].channels[num_chan] = *frm++;
                    }
                    ni->ni_mbo.channel[non_pref_attribute].channels_preference = *frm++;
                    ni->ni_mbo.channel[non_pref_attribute].reason_preference   = *frm++;
                    non_pref_attribute++;
                    ni->ni_mbo.num_attr = non_pref_attribute;
                }
            }
            sfrm += len + 2;
        } else if (attribute_id == MBO_ATTRIB_CELLULAR) {             // cellular capabilities sub-element
            len = *(sfrm +1);
            if(len < 5 ){
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "No cellular capabilities present \n");
            } else {
                ni->ni_mbo.cellular_cap = *(frm);
            }
            sfrm += len + 2;
        }
    }
#undef MBO_IE_MIN_LEN
    return EOK;
}

/**
 * @brief
 * @param [in]
 * @param [out]
 */

u_int8_t*
ieee80211_setup_mbo_ie(int f_type, struct ieee80211vap *vap, u_int8_t *ie, struct ieee80211_node *ni)
{
    u_int8_t len, *frm;
    struct ieee80211_mbo_ie *mbo_ie = (struct ieee80211_mbo_ie *)ie;

    OS_MEMSET(mbo_ie,0x00,sizeof(struct ieee80211_mbo_ie));
    ieee80211_mbo_fill_header(&(mbo_ie->header),IEEE80211_ELEMID_VENDOR,0);
    mbo_ie->oui[0] = 0x50;
    mbo_ie->oui[1] = 0x6f;
    mbo_ie->oui[2] = 0x9a;
    mbo_ie->oui_type = MBO_OUI_TYPE;

    frm = (uint8_t *)&mbo_ie->opt_ie[0];
    if (ieee80211_vap_mbo_check(vap)) {
        len = ieee80211_setup_mbo_attrs(vap, frm);
        frm += len;
    }
    if (ieee80211_vap_oce_check(vap)) {
        len = ieee80211_setup_oce_attrs(f_type, vap, frm, ni);
        frm += len;
    }

    len = frm - (uint8_t *)&mbo_ie->opt_ie[0];
    mbo_ie->header.len = len + (sizeof(struct ieee80211_mbo_ie) - sizeof(struct ieee80211_mbo_header));
    return frm;
}

/**
 * @brief
 * @param [in]
 * @param [out]
 */

/**
 * Using info parsed in supported operating classes ie
 * to derive channels that are not supported by STA
 */
int ieee80211_mbo_supp_op_cl_to_non_pref_chan_rpt(struct ieee80211_node *ni, u_int16_t countryCode)
{
    int i;
    ni->ni_supp_op_cl.num_chan_supported = 0;
    /**
     * Initializing array to false. The array index serves
     * as channel number. A false value indicates channel number
     * equal to the value of index of the slot is not supported.
     */
    for (i = 0; i < IEEE80211_MBO_CHAN_WORDS; i++)
        ni->ni_supp_op_cl.channels_supported[i] = false;

    /**
     * marking true for those channels that are
     * supported by STA
     */
    for (i = 0; i < (ni->ni_supp_op_cl.num_of_supp_class); i++)
    {
        /**
         * Calling function that marks true for
         * those channels that are supported by the STA
         * as obtained from operating class
         */
        regdmn_get_channel_list_from_op_class(ni->ni_supp_op_cl.supp_class[i], ni);
    }
    return 1;
}

int
ieee80211_parse_op_class_ie(u_int8_t *frm, const struct ieee80211_frame *wh,
                            struct ieee80211_node *ni, u_int16_t countrycode)
{
    int i,opclass_num = 0;

    /**
     * Format of supported operating class IE
     * Elem ID:59( 1 byte )
     * Length: 2-253( 1 byte )
     * Current Operating class( 1 byte )
     * Supported operating classes( Variable )
     */
    opclass_num = ni->ni_supp_op_cl.num_of_supp_class = frm[1];
    ni->ni_supp_op_cl.curr_op_class = frm[2];

    if ( opclass_num ) {
        for (i = 0; i < opclass_num; i++) {
            ni->ni_supp_op_cl.supp_class[i] = frm[i+2];
        }
        ieee80211_mbo_supp_op_cl_to_non_pref_chan_rpt(ni,countrycode);
    }
    return 1;
}

/**
 * @brief Verify that the oce handle is valid
 *
 * @param [in] VAP  the handle to the
 *
 * @return true if handle is valid; otherwise false
 */

static INLINE bool ieee80211_oce_is_valid(const struct ieee80211vap *vap)
{
    return vap && vap->oce;
}

/**
 * @brief Initialize the OCE infrastructure.
 *
 * @param [in] vap  to initialize
 *
 * @return EOK on success; EINPROGRESS if band steering is already initialized
 *         or ENOMEM on a memory allocation failure
 */
int ieee80211_oce_vattach (struct ieee80211vap *vap)
{
	ieee80211_oce_t oce;

	if (!ieee80211_oce_is_vap_valid(vap))
		return EINVAL;

	if (vap->oce)
		return EINPROGRESS;

	oce = (ieee80211_oce_t) OS_MALLOC(vap->iv_ic->ic_osdev, sizeof(*oce), 0);
	if (oce == NULL)
		return -ENOMEM;

	OS_MEMZERO(oce, sizeof(*oce));
	oce->oce_osdev = vap->iv_ic->ic_osdev;
	oce->oce_ic = vap->iv_ic;
	oce->usr_assoc_min_rssi = OCE_ASSOC_MIN_RSSI_DEFAULT;
	oce->usr_assoc_retry_delay = OCE_ASSOC_RETRY_DELAY_DEFAULT;
	vap->oce = oce;

	QDF_TRACE(QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "OCE Initialized");
	return EOK;
}

/**
 * @brief deint the OCE infrastructure.
 *
 * @param [in] vap  to deint
 *
 * @return EOK on success;
 */
int ieee80211_oce_vdetach (struct ieee80211vap *vap)
{
	if (!ieee80211_oce_is_valid(vap))
		return EINVAL;

	if (vap->oce == NULL)
		return EINVAL;

	OS_FREE(vap->oce);
	vap->oce = NULL;

	QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO, "%s: OCE terminated\n", __func__);
	return EOK;
}

/**
 * set simple configuration parameter.
 *
 * @param devhandle       : handle to the vap.
 * @param param           : config paramter
 * @param val             : value of the parameter.
 * @return 0  on success and -ve on failure.
 */
int wlan_set_oce_param (wlan_if_t vap, u_int32_t param, u_int32_t val)
{
	int retv = EOK;
	ieee80211_oce_t oce = NULL;
	int8_t rssi;
	struct wlan_vdev_mgr_cfg mlme_cfg;

	if (!ieee80211_oce_is_vap_valid(vap))
		return EINVAL;

	oce = vap->oce;

	switch (param) {
	case IEEE80211_OCE:
		if (val) {
			ieee80211_vap_oce_set(vap);
			/* Reset mgmt rate to enable OCE OOB rate */
			mlme_cfg.value = 0;
			wlan_util_vdev_mlme_set_param(vap->vdev_mlme,
					WLAN_MLME_CFG_TX_MGMT_RATE, mlme_cfg);
		} else {
			ieee80211_vap_oce_clear(vap);
		}
		break;
	case IEEE80211_OCE_ASSOC_REJECT:
		if (ieee80211_vap_oce_is_set(vap)) {
			oce->usr_assoc_reject = val;
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_OCE_ASSOC_MIN_RSSI:
		rssi = (int8_t) val;
		if (ieee80211_vap_oce_is_set(vap) &&
			(rssi >= OCE_ASSOC_MIN_RSSI_MIN) &&
			(rssi <= OCE_ASSOC_MIN_RSSI_MAX)) {
			oce->usr_assoc_min_rssi = rssi;
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_OCE_ASSOC_RETRY_DELAY:
		if (ieee80211_vap_oce_is_set(vap)) {
			oce->usr_assoc_retry_delay = (u_int8_t) val;
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_OCE_WAN_METRICS:
		if (ieee80211_vap_oce_is_set(vap)) {
			int ul_cap = OCE_WAN_METRIC(val >> 16);
			int dl_cap = OCE_WAN_METRIC(val & 0xFFFF);

			oce->usr_wan_metrics =
				(ul_cap << OCE_WAN_METRICS_UL_SHIFT) |
				(dl_cap << OCE_WAN_METRICS_DL_SHIFT);
            ieee80211vap_set_flag_ext2(vap, IEEE80211_FEXT2_MBO);
		} else {
			retv = EINVAL;
		}
		break;
	case IEEE80211_OCE_HLP:
		if (ieee80211_vap_oce_is_set(vap)) {
			oce->usr_hlp = val;
            ieee80211vap_set_flag_ext2(vap, IEEE80211_FEXT2_MBO);
		} else {
			retv = EINVAL;
		}
		break;
	default:
		retv = EINVAL;
		break;
	}

	return retv;
}

/**
 * get simple configuration parameter.
 *
 * @param devhandle       : handle to the vap.
 * @param param           : paramter.
 * @return value of the parameter.
 */
u_int32_t wlan_get_oce_param (wlan_if_t vap, u_int32_t param)
{
	u_int32_t retv = 0;
	ieee80211_oce_t oce = NULL;

	if (!ieee80211_oce_is_vap_valid(vap))
		return EINVAL;

	oce = vap->oce;

	switch (param) {
	case IEEE80211_OCE:
		retv = ieee80211_vap_oce_is_set(vap);
		break;
	case IEEE80211_OCE_ASSOC_REJECT:
		retv = oce->usr_assoc_reject;
		break;
	case IEEE80211_OCE_ASSOC_MIN_RSSI:
		retv = oce->usr_assoc_min_rssi;
		break;
	case IEEE80211_OCE_ASSOC_RETRY_DELAY:
		retv = oce->usr_assoc_retry_delay;
		break;
	case IEEE80211_OCE_WAN_METRICS:
		retv = oce->usr_wan_metrics;
		break;
	case IEEE80211_OCE_HLP:
		retv = oce->usr_hlp;
		break;
	default:
		retv = EINVAL;
		break;
	}

	return retv;
}

/**
 * @brief
 * @param [in]
 * @param [out]
 */
u_int8_t ieee80211_setup_oce_attrs (int f_type, struct ieee80211vap *vap, u_int8_t *frm, struct ieee80211_node *ni)
{
	ieee80211_oce_t oce = vap->oce;
	u_int8_t *start = frm;
	struct ieee80211_oce_cap *cap;
	struct ieee80211_oce_assoc_reject *reject;
	struct ieee80211_oce_wan_metrics *metrics;

	struct ieee80211_oce_rnr_completeness *completeness;
	/* Attribute OCE_ATTRIB_CAP_INDICATION */
	cap = (struct ieee80211_oce_cap *) frm;
	cap->header.ie = OCE_ATTRIB_CAP_INDICATION;
	cap->header.len = sizeof (struct ieee80211_oce_cap) - 2;
	cap->ctrl_oce_rel = OCE_CAP_REL_DEFAULT;
	cap->ctrl_sta_cfon = 0;
	cap->ctrl_non_oce_ap_present = oce->non_oce_ap_present;
	cap->ctrl_11b_ap_present = oce->only_11b_ap_present;
	cap->ctrl_hlp_enabled = oce->usr_hlp;
	cap->ctrl_reserved = 0;
	frm += sizeof (struct ieee80211_oce_cap);

	if ((f_type == IEEE80211_FC0_SUBTYPE_ASSOC_RESP) ||
		(f_type == IEEE80211_FC0_SUBTYPE_REASSOC_RESP)) {
		/* Attribute OCE_ATTRIB_RSSI_ASSOC_REJECT */
		if (oce->usr_assoc_reject && ni && (ni->ni_abs_rssi < oce->usr_assoc_min_rssi)) {
			reject = (struct ieee80211_oce_assoc_reject *) frm;

			reject->header.ie = OCE_ATTRIB_RSSI_ASSOC_REJECT;
			reject->header.len = sizeof (struct ieee80211_oce_assoc_reject) - 2;
			reject->delta_rssi = oce->usr_assoc_min_rssi - ni->ni_abs_rssi;
			reject->retry_delay = oce->usr_assoc_retry_delay;
			frm += sizeof (struct ieee80211_oce_assoc_reject);
		}
	} else {
		/* Attribute OCE_ATTRIB_REDUCED_WAN_METRICS */
		if (oce->usr_wan_metrics) {
			metrics = (struct ieee80211_oce_wan_metrics *) frm;

			metrics->header.ie = OCE_ATTRIB_REDUCED_WAN_METRICS;
			metrics->header.len = sizeof (struct ieee80211_oce_wan_metrics) - 2;
			metrics->avail_cap = oce->usr_wan_metrics;
			frm += sizeof (struct ieee80211_oce_wan_metrics);
		}
		if (vap->rnr_enable) {
			completeness = (struct ieee80211_oce_rnr_completeness *) frm;
			if (vap->rnr_ap_info_complete) {
				/* Add wildcard short ssid list */
				completeness->header.ie = OCE_ATTRIB_RNR_COMPLETENESS;
				completeness->header.len = 0;
				frm += sizeof (struct ieee80211_oce_rnr_completeness);
			}
		}
	}

	return (frm - start);
}
#else
/**
 * set configuration parameter.
 *
 * @param devhandle       : handle to the vap.
 * @param param           : simple config paramaeter.
 * @param val             : value of the parameter.
 * @return 0  on success and -ve on failure.
 */
int wlan_set_mbo_param(wlan_dev_t devhandle, ieee80211_device_param param, u_int32_t val)
{
    return EOK;
}

/**
 * get configuration parameter.
 *
 * @param devhandle       : handle to the vap.
 * @param param           : config paramaeter.
 * @return value of the parameter.
 */
u_int32_t wlan_get_mbo_param(wlan_if_t vap, u_int32_t param)
{
    return EOK;
}

/**
 * @brief Initialize the MBO infrastructure.
 *
 * @param [in] vap  to initialize
 *
 * @return EOK on success; EINPROGRESS if band steering is already initialized
 *         or ENOMEM on a memory allocation failure
 */

int ieee80211_mbo_vattach(struct ieee80211vap *vap)
{
    return EOK;
}

/**
 * @brief deint the MBO infrastructure.
 *
 * @param [in] vap  to deint
 *
 * @return EOK on success;
 */

int ieee80211_mbo_vdetach(struct ieee80211vap *vap)
{

    return EOK;
}

/**
 * @brief setup MBO ie in Beacon infrastructure.
 *
 * @param [in] Vap to intialize beacon.
 *
 * @return frame pointer;
 *
 */

u_int8_t* ieee80211_setup_mbo_ie(int f_type, struct ieee80211vap *vap, u_int8_t *ie, struct ieee80211_node *ni)
{
    return ie;
}


/**
 * @brief Initialize the MBO infrastructure.
 *
 * @param [in] vap  to initialize
 *
 * @return EOK on success; EINPROGRESS if band steering is already initialized
 *         or ENOMEM on a memory allocation failure
 */
int ieee80211_oce_vattach (struct ieee80211vap *vap)
{
	return EOK;
}

/**
 * @brief deint the MBO infrastructure.
 *
 * @param [in] vap  to deint
 *
 * @return EOK on success;
 */
int ieee80211_oce_vdetach (struct ieee80211vap *vap)
{
	return EOK;
}

/**
 * set simple configuration parameter.
 *
 * @param devhandle       : handle to the vap.
 * @param param           : config paramter
 * @param val             : value of the parameter.
 * @return 0  on success and -ve on failure.
 */
int wlan_set_oce_param (wlan_if_t vap, u_int32_t param, u_int32_t val)
{
	return EOK;
}

/**
 * get simple configuration parameter.
 *
 * @param devhandle       : handle to the vap.
 * @param param           : paramter.
 * @return value of the parameter.
 */
u_int32_t wlan_get_oce_param (wlan_if_t vap, u_int32_t param)
{
	return EOK;
}
#endif

/**
 * @brief Determine whether the VAP handle is valid,
 *        has a valid oce handle,
 *        is operating in a mode where OCE is relevant,
 *        and is not in the process of being deleted.
 *
 * @return true if the VAP is valid; otherwise false
 */
bool ieee80211_oce_is_vap_valid (struct ieee80211vap *vap)
{
	return (vap && (wlan_vap_get_opmode(vap) == IEEE80211_M_HOSTAP) &&
			!ieee80211_vap_deleted_is_set(vap));
}

/**
 * @brief Determine whether the VAP handle is valid,
 *        and OCE is enabled
 *
 * @return true if the VAP is valid and enabled; otherwise false
 */
bool ieee80211_vap_oce_check (struct ieee80211vap *vap)
{
	if (ieee80211_oce_is_vap_valid(vap) && vap->oce && ieee80211_vap_oce_is_set(vap))
		return true;
	else
		return false;
}

/**
 * @brief Determine whether assoc should be rejected or not,
 *        based on the RSSI of the received (Re)Assoc-Req and
 *        type of STA (OCE or non-OCE)
 *
 * @return true if the assoc is to be rejected; otherwise false
 */
bool ieee80211_vap_oce_assoc_reject (struct ieee80211vap *vap, struct ieee80211_node *ni)
{
	ieee80211_oce_t oce = vap->oce;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "oce->usr_assoc_reject %d, ni->ni_mbo.oce_sta %d, ni->ni_abs_rssi %d, oce->usr_assoc_min_rssi %d\n",
					  oce->usr_assoc_reject, ni->ni_mbo.oce_sta, ni->ni_abs_rssi, oce->usr_assoc_min_rssi);
	if (oce->usr_assoc_reject) {
		if (ni->ni_mbo.oce_sta && (ni->ni_abs_rssi < oce->usr_assoc_min_rssi)) {
			return true;
		} else {
			return false;
		}
	} else {
		return false;
	}
}

/**
 * @brief Determine if OCE capable or not by checking OCE Cap
 *        Indication attribute in the MBO_OCE IE
 *
 * @return true if OCE capable; otherwise false
 */
bool ieee80211_oce_capable(u_int8_t *frm)
{
	u_int attr_id;
	u_int attr_len;
	bool  oce_cap = false;
	int   len = frm[1];

	/* Increment frm to pass MBO OUI (3B) and OUT Type (1B) */
	frm += 6;
	len -= 4;

	while (len > 0)
	{
		attr_id = *frm++;
		attr_len = *frm++;
		if (attr_id == OCE_ATTRIB_CAP_INDICATION) {
			oce_cap = true;
			break;
		}
		frm += attr_len;
		len -= (attr_len + 2);
	}

	return oce_cap;
}

/**
 * @brief Determine if OCE capable STA suppresses this AP
 *        probe response.
 *
 * @return true if STA suppresses the AP's response; false otherwise
 */
bool ieee80211_oce_suppress_ap(u_int8_t *frm, struct ieee80211vap *vap)
{
    struct ieee80211_node *ni = vap->iv_bss;
    u_int attr_id;
    u_int attr_len;
    bool  prb_suppress_bssid = false, prb_suppress_ssid = false;
    int   len = frm[1];
    u_int num_mac = 0, num_ssid = 0;
    u_int8_t *frm_cpy = frm;
    u_int32_t parsed_short_ssid = 0, vap_short_ssid = 0;

    vap_short_ssid = ieee80211_construct_shortssid(ni->ni_essid, ni->ni_esslen);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME, "%s short ssid: 0x%08x\n", __func__, vap_short_ssid);

    /* Increment frm to pass MBO OUI (3B) and OUI Type (1B) */
    frm += 6;
    len -= 4;

    while (len > 0)
    {
        attr_id = *frm++;
        attr_len = *frm++;
        frm_cpy = frm;

        switch (attr_id) {
            case OCE_ATTRIB_PROBE_SUPPRESSION_BSSID:

                /* Validate if length is non-negative and multiple of 6.
                 * The attribute length can be zero if STA does not want to supress.
                 * else ignore this attribute and continue parsing the remaining attributes.
                 */
                if (attr_len < 0 || attr_len % IEEE80211_ADDR_LEN != 0) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                                  "Error parsing probe suppression BSSID attribute length - %u\n", attr_len);
                    break;
                }

                num_mac = attr_len / IEEE80211_ADDR_LEN;
                if (num_mac == 0) {
                    /* Wildcard */
                    prb_suppress_bssid = true;
                } else {
                    /* Loop through 6 bytes of mac address from the list */
                    while (num_mac--) {
                        if (IEEE80211_ADDR_EQ(frm_cpy, vap->iv_myaddr)) {
                            prb_suppress_bssid = true;
                            break;
                        }
                        frm_cpy += IEEE80211_ADDR_LEN;
                    }
                }
                break;
            case OCE_ATTRIB_PROBE_SUPPRESSION_SSID:

                /* Validate if length is atleast 4 and multiple of 4
                 * else ignore this attribute and continue parsing the remaining ones.
                 */
                if (attr_len < IEEE80211_SHORT_SSID_LEN || attr_len % IEEE80211_SHORT_SSID_LEN != 0) {
                    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                                  "Error parsing probe suppression attribute SSID length - %u\n", attr_len);
                    break;
                }

                num_ssid = attr_len / IEEE80211_SHORT_SSID_LEN;

                /* Loop through 4 bytes of short ssid from the list */
                while (num_ssid--) {
                    /* Read 4 byte data from network */
                    parsed_short_ssid = LE_READ_4(frm_cpy);

                    if (parsed_short_ssid == vap_short_ssid) {
                        prb_suppress_ssid = true;
                        break;
                    }
                    frm_cpy += IEEE80211_SHORT_SSID_LEN;
                 }
                 break;

            default:
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
                                 "Obtained attrid %u mismatch\n", attr_id);
        }

        frm += attr_len;
        len -= (attr_len + 2);
    }

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_MLME,
        "prb_suppress_bssid - %d, prb_suppress_ssid - %d\n", prb_suppress_bssid, prb_suppress_ssid);

    /* Supress the response if either of the attributes matches */
    return prb_suppress_bssid || prb_suppress_ssid;
}

QDF_STATUS
ieee80211_check_non_oce_ap(void *iarg, wlan_scan_entry_t se)
{
    ieee80211_oce_scan_cbinfo_t *cb_info = (ieee80211_oce_scan_cbinfo_t *) iarg;
    struct ieee80211_ath_channel *chan = wlan_util_scan_entry_channel(se);

    bool oce_cap = false;
    u_int8_t *mboie;

    if (chan->ic_ieee != cb_info->channel)
        return QDF_STATUS_SUCCESS;

    if (util_scan_entry_age(se) > (cb_info->age_timeout * 1000))
        return QDF_STATUS_SUCCESS;

    mboie = util_scan_entry_mbo_oce(se);
    if (mboie) {
        oce_cap = ieee80211_oce_capable(mboie);
    }

    if (oce_cap == false) {
        cb_info->non_oce_ap_present = true;
        return QDF_STATUS_E_ALREADY;
    } else {
        return QDF_STATUS_SUCCESS;
    }
}

/*
 * Check if there are any active non-OCE APs in the AP's operating channel.
 */
QDF_STATUS
ieee80211_update_non_oce_ap_presence (struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_oce_t oce = vap->oce;
    struct ieee80211vap *nbr_vap, *tvap;
    ieee80211_oce_scan_cbinfo_t cb_info;
    bool prev_value = oce->non_oce_ap_present;
    QDF_STATUS status = QDF_STATUS_SUCCESS;

    /* Go through vap list of AP's own radio */
    TAILQ_FOREACH_SAFE(nbr_vap, &ic->ic_vaps, iv_next, tvap) {
        if (!OS_MEMCMP(nbr_vap->iv_myaddr, vap->iv_myaddr, IEEE80211_ADDR_LEN))
            continue;

        if (!ieee80211_vap_oce_check(nbr_vap)) {
            oce->non_oce_ap_present = true;
            return status;
        }
    }

    OS_MEMSET(&cb_info, 0, sizeof(cb_info));
    cb_info.channel = vap->iv_bsschan->ic_ieee;
    cb_info.age_timeout = vap->nbr_scan_period;

    /* Go through scan list */
    status = ucfg_scan_db_iterate(wlan_vap_get_pdev(vap), ieee80211_check_non_oce_ap, (void *)&cb_info);
    oce->non_oce_ap_present = cb_info.non_oce_ap_present;

    if (oce->non_oce_ap_present != prev_value) {
        ieee80211vap_set_flag_ext2(vap, IEEE80211_FEXT2_MBO);
#ifdef WLAN_SUPPORT_FILS
        wlan_fd_update_trigger(vap->vdev_obj);
#endif
    }

    return status;
}

bool ieee80211_non_oce_ap_present (struct ieee80211vap *vap)
{
    if (vap->oce && vap->oce->non_oce_ap_present)
        return true;
    else
        return false;
}

QDF_STATUS
ieee80211_check_11b_ap(void *iarg, wlan_scan_entry_t se)
{
    ieee80211_oce_scan_cbinfo_t *cb_info = (ieee80211_oce_scan_cbinfo_t *) iarg;
    struct ieee80211_ath_channel *chan = wlan_util_scan_entry_channel(se);
    enum wlan_phymode phymode;

    if (chan->ic_ieee != cb_info->channel)
        return QDF_STATUS_SUCCESS;

    if (util_scan_entry_age(se) > (cb_info->age_timeout * 1000))
        return QDF_STATUS_SUCCESS;

    phymode = util_scan_entry_phymode(se);
    if (phymode == WLAN_PHYMODE_11B) {
        cb_info->only_11b_ap_present = true;
        return QDF_STATUS_E_ALREADY;
    } else {
        return QDF_STATUS_SUCCESS;
    }
	return QDF_STATUS_SUCCESS;
}


/*
 * Check if there are any active 802.11B APs in the AP's operating channel.
 */
QDF_STATUS
ieee80211_update_11b_ap_presence (struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_oce_t oce = vap->oce;
    struct ieee80211vap *nbr_vap, *tvap;
    ieee80211_oce_scan_cbinfo_t cb_info;
    bool prev_value = oce->only_11b_ap_present;
    QDF_STATUS status = QDF_STATUS_SUCCESS;

    /* Go through vap list of AP's own radio */
    TAILQ_FOREACH_SAFE(nbr_vap, &ic->ic_vaps, iv_next, tvap) {
        if (!OS_MEMCMP(nbr_vap->iv_myaddr, vap->iv_myaddr, IEEE80211_ADDR_LEN))
            continue;

        if (nbr_vap->iv_des_mode == IEEE80211_MODE_11B) {
            oce->only_11b_ap_present = true;
            return status;
        }
    }

    OS_MEMSET(&cb_info, 0, sizeof(cb_info));
    cb_info.channel = vap->iv_bsschan->ic_ieee;
    cb_info.age_timeout = vap->nbr_scan_period;

    /* Go through scan list */
    status = ucfg_scan_db_iterate(wlan_vap_get_pdev(vap), ieee80211_check_11b_ap, (void *)&cb_info);
    oce->only_11b_ap_present = cb_info.only_11b_ap_present;

    if (oce->only_11b_ap_present != prev_value) {
        ieee80211vap_set_flag_ext2(vap, IEEE80211_FEXT2_MBO);
#ifdef WLAN_SUPPORT_FILS
        wlan_fd_update_trigger(vap->vdev_obj);
#endif
    }

    return status;
}

bool ieee80211_11b_ap_present (struct ieee80211vap *vap)
{
    if (vap->oce && vap->oce->only_11b_ap_present)
        return true;
    else
        return false;
}

QDF_STATUS
ieee80211_fill_rnr_entry(void *iarg, wlan_scan_entry_t se)
{
    ieee80211_rnr_cbinfo_t *cb_info = (ieee80211_rnr_cbinfo_t *) iarg;
    struct ieee80211vap *vap = cb_info->vap;
    ieee80211_rnr_ap_info_t *ap_info = (ieee80211_rnr_ap_info_t *) cb_info->ap_info;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211vap *nbr_vap, *tvap;
    struct ieee80211com *nbr_ic;
    struct ieee80211_country_ie *country_ie;
    char country_iso[4];
    struct ieee80211_ath_channel *chan;
    u_int8_t *scan_ssid;
    u_int8_t scan_ssid_len;
    int i;

    if (cb_info->ap_info_cnt >= cb_info->ap_info_cnt_max)
        return QDF_STATUS_E_NOMEM;

    /* Check if it's from neighboring BSSs or not */
    for (i = 0; i < MAX_RADIO_CNT; i++) {
        GLOBAL_IC_LOCK_BH(vap->iv_ic->ic_global_list);
        nbr_ic = ic->ic_global_list->global_ic[i];
        GLOBAL_IC_UNLOCK_BH(vap->iv_ic->ic_global_list);

        if (!nbr_ic || TAILQ_EMPTY(&nbr_ic->ic_vaps))
            continue;

        TAILQ_FOREACH_SAFE(nbr_vap, &nbr_ic->ic_vaps, iv_next, tvap) {
            if (!OS_MEMCMP(nbr_vap->iv_myaddr, util_scan_entry_bssid(se), IEEE80211_ADDR_LEN))
                return QDF_STATUS_SUCCESS;
        }
    }

    scan_ssid = util_scan_entry_ssid(se)->ssid;
    scan_ssid_len = util_scan_entry_ssid(se)->length;
    country_ie = (struct ieee80211_country_ie *) util_scan_entry_country(se);
    chan = wlan_util_scan_entry_channel(se);
    if (country_ie) {
        ap_info->op_class = regdmn_get_opclass (country_ie->cc, chan);
    } else {
        ieee80211_getCurrentCountryISO(vap->iv_ic, country_iso);
        ap_info->op_class = regdmn_get_opclass (country_iso, chan);
    }
    ap_info->channel = chan->ic_ieee;

    ap_info->hdr_field_type = 0;
    ap_info->hdr_filtered_nbr_ap = 0;
    ap_info->hdr_info_cnt = 0;
    ap_info->hdr_info_len = sizeof (ieee80211_rnr_tbtt_info_t);
    ap_info->tbtt_info[0].tbtt_offset = 255;
    IEEE80211_ADDR_COPY (ap_info->tbtt_info[0].bssid, util_scan_entry_bssid(se));
    ap_info->tbtt_info[0].short_ssid = htole32(ieee80211_construct_shortssid(scan_ssid, scan_ssid_len));

    cb_info->ap_info = (uint8_t *) (ap_info) + sizeof (ieee80211_rnr_ap_info_t) + sizeof (ieee80211_rnr_tbtt_info_t);
    ++cb_info->ap_info_cnt;

    return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ieee80211_update_rnr (struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    u_int32_t ap_info_cnt = 0;
    u_int32_t tbtt_info_cnt;
    ieee80211_rnr_ap_info_t *ap_info;
    ieee80211_rnr_tbtt_info_t *tbtt_info;
    struct ieee80211vap *nbr_vap, *tvap;
    struct ieee80211com *nbr_ic;
    ieee80211_rnr_cbinfo_t cb_info;
    char country_iso[4];
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    u_int8_t *frm;
    u_int8_t *org_frm;
    int i;

    frm = vap->rnr_ap_info;
    org_frm = frm;

    /* Go through vap list of AP's own radios */
    for (i = 0; i < MAX_RADIO_CNT; i++) {
        GLOBAL_IC_LOCK_BH(vap->iv_ic->ic_global_list);
        nbr_ic = ic->ic_global_list->global_ic[i];
        GLOBAL_IC_UNLOCK_BH(vap->iv_ic->ic_global_list);

        if (!nbr_ic || TAILQ_EMPTY(&nbr_ic->ic_vaps))
            continue;

        if (ap_info_cnt >= RNR_MAX_ENTRIES)
            break;

        ap_info = (ieee80211_rnr_ap_info_t *) frm;
        tbtt_info = ap_info->tbtt_info;
        tbtt_info_cnt = 0;
        TAILQ_FOREACH_SAFE(nbr_vap, &nbr_ic->ic_vaps, iv_next, tvap) {
            if (!OS_MEMCMP(nbr_vap->iv_myaddr, vap->iv_myaddr, IEEE80211_ADDR_LEN))
                continue;

            if ((ap_info_cnt + tbtt_info_cnt) >= RNR_MAX_ENTRIES)
                break;

            if (vap->rnr_enable_tbtt && (ic->ic_intval == nbr_ic->ic_intval)) {
                u_int32_t tbtt_offset;

                /* TBTT Offset indicates the offset in TUs, rounded down to nearest TU, to the next
                 * TBTT of an AP from the immediately prior TBTT of the AP that transmits this element.
                 */
                if (nbr_vap->iv_estimate_tbtt > vap->iv_estimate_tbtt) {
                    tbtt_offset = ((nbr_vap->iv_estimate_tbtt - vap->iv_estimate_tbtt) * 1000) / 1024;
                    tbtt_offset = tbtt_offset % ic->ic_intval;
                } else {
                    tbtt_offset = ((vap->iv_estimate_tbtt - nbr_vap->iv_estimate_tbtt) * 1000 + 1023) / 1024;
                    tbtt_offset = (ic->ic_intval - (tbtt_offset % ic->ic_intval)) % ic->ic_intval;
                }

                /* The value 254 indicates an offset of 254 TUs or higher */
                if (tbtt_offset > 254)
                    tbtt_offset = 254;

                tbtt_info->tbtt_offset = tbtt_offset;
            } else {
                /* The value 255 indicates an unknown offset value */
                tbtt_info->tbtt_offset = 255;
            }

            IEEE80211_ADDR_COPY(tbtt_info->bssid, nbr_vap->iv_myaddr);      // or vap->iv_my_hwaddr
            tbtt_info->short_ssid = htole32(ieee80211_construct_shortssid(nbr_vap->iv_bss->ni_essid, nbr_vap->iv_bss->ni_esslen));

            tbtt_info++;
            tbtt_info_cnt++;
        }

        if (tbtt_info_cnt) {
            nbr_vap = TAILQ_FIRST(&nbr_ic->ic_vaps);
			ieee80211_getCurrentCountryISO(nbr_vap->iv_ic, country_iso);
            ap_info->op_class = regdmn_get_opclass (country_iso, nbr_vap->iv_ic->ic_curchan);
            ap_info->channel = nbr_vap->iv_bsschan->ic_ieee;
            ap_info->hdr_field_type = 0;
            ap_info->hdr_filtered_nbr_ap = 0;
            ap_info->hdr_info_cnt = tbtt_info_cnt - 1;
            ap_info->hdr_info_len = sizeof (ieee80211_rnr_tbtt_info_t);
            ap_info_cnt += tbtt_info_cnt;
            frm += sizeof (ieee80211_rnr_ap_info_t) + sizeof (ieee80211_rnr_tbtt_info_t) * tbtt_info_cnt;
        }
    }

    /* Go through scan list of all radios */
    for (i = 0; i < MAX_RADIO_CNT; i++) {
        GLOBAL_IC_LOCK_BH(vap->iv_ic->ic_global_list);
        nbr_ic = ic->ic_global_list->global_ic[i];
        GLOBAL_IC_UNLOCK_BH(vap->iv_ic->ic_global_list);

        if (!nbr_ic || TAILQ_EMPTY(&nbr_ic->ic_vaps))
            continue;

        OS_MEMSET(&cb_info, 0, sizeof(cb_info));
        cb_info.vap = vap;
        cb_info.ap_info = frm;
        cb_info.ap_info_cnt = ap_info_cnt;
        cb_info.ap_info_cnt_max = RNR_MAX_ENTRIES;
        status = ucfg_scan_db_iterate(nbr_ic->ic_pdev_obj, ieee80211_fill_rnr_entry, &cb_info);
        frm = cb_info.ap_info;
        ap_info_cnt = cb_info.ap_info_cnt;
        if (status != QDF_STATUS_SUCCESS)
            break;
    }

    vap->rnr_ap_info_len = (int) (frm - org_frm);
    if (ieee80211_bg_scan_enabled(vap) && (status == QDF_STATUS_SUCCESS))
        vap->rnr_ap_info_complete = true;
    else
        vap->rnr_ap_info_complete = false;

    ieee80211vap_set_flag_ext2(vap, IEEE80211_FEXT2_MBO);
    if (vap->rnr_enable_fd) {
#ifdef WLAN_SUPPORT_FILS
        wlan_fd_update_trigger(vap->vdev_obj);
#endif
    }

    return status;
}

/*
 * Add Reduced Neighbor Report (RNR) information element to frame.
 */
u_int8_t *ieee80211_add_rnr_ie (u_int8_t *frm, struct ieee80211vap *vap, u_int8_t *ssid, u_int8_t ssid_len)
{
    ieee80211_rnr_ap_info_t *ap_info;
    ieee80211_rnr_tbtt_info_t *tbtt_info;
    u_int32_t tbtt_info_cnt;
    u_int32_t tbtt_info_len;
    u_int8_t *start;
    u_int8_t *end;
    u_int32_t short_ssid = htole32(ieee80211_construct_shortssid(ssid, ssid_len));
    int i;

    if (!vap->rnr_ap_info_len)
        return frm;

    *frm++ = IEEE80211_ELEMID_REDUCED_NBR_RPT;
    *frm++ = vap->rnr_ap_info_len;
    OS_MEMCPY(frm, vap->rnr_ap_info, vap->rnr_ap_info_len);

    /* Walk through the list and update 'Filtered Neighbor AP' subfield after ssid matching */
    if (ssid_len) {
        start = frm;
        end = start + vap->rnr_ap_info_len;

        while (start < end) {
            ap_info = (ieee80211_rnr_ap_info_t *) start;
            tbtt_info_cnt = ap_info->hdr_info_cnt + 1;
            tbtt_info_len = ap_info->hdr_info_len;
            tbtt_info = ap_info->tbtt_info;

            for (i = 0; i < tbtt_info_cnt; i++) {
                tbtt_info = &ap_info->tbtt_info[i];

                if (tbtt_info->short_ssid == short_ssid) {
                    ap_info->hdr_filtered_nbr_ap = 1;
                    break;
                }
            }

            start += sizeof (ieee80211_rnr_ap_info_t) + sizeof (ieee80211_rnr_tbtt_info_t) * tbtt_info_cnt;
        }
    }

    frm += vap->rnr_ap_info_len;

    return frm;
}

QDF_STATUS
ieee80211_add_ap_chan_rpt_entry(struct ieee80211vap *vap, int op_class, int channel)
{
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    int i;
    int j;

    for (i = 0; i < vap->ap_chan_rpt_cnt; i++) {
        if (vap->ap_chan_rpt[i].op_class == op_class)
            break;
    }

    if (i < vap->ap_chan_rpt_cnt) {
        for (j = 0; j < vap->ap_chan_rpt[i].chan_cnt; j++) {
            if (vap->ap_chan_rpt[i].chan[j] == (u_int8_t) channel)
                break;
        }

        if (j == vap->ap_chan_rpt[i].chan_cnt) {

            if (vap->ap_chan_rpt[i].chan_cnt < ACR_MAX_CHANNELS) {
                vap->ap_chan_rpt[i].chan[vap->ap_chan_rpt[i].chan_cnt] = channel;
                vap->ap_chan_rpt[i].chan_cnt++;
            } else {
                status = QDF_STATUS_E_NOMEM;
            }
        }
    } else {
        if (vap->ap_chan_rpt_cnt < ACR_MAX_ENTRIES) {
            vap->ap_chan_rpt[vap->ap_chan_rpt_cnt].op_class = op_class;
            vap->ap_chan_rpt[vap->ap_chan_rpt_cnt].chan[0] = channel;
            vap->ap_chan_rpt[vap->ap_chan_rpt_cnt].chan_cnt = 1;
            vap->ap_chan_rpt_cnt++;
        } else {
            status = QDF_STATUS_E_NOENT;
        }
    }

    return status;
}


QDF_STATUS
ieee80211_fill_ap_chan_rpt_entry(void *iarg, wlan_scan_entry_t se)
{
    ieee80211_acr_cbinfo_t *cb_info = (ieee80211_acr_cbinfo_t *) iarg;
    struct ieee80211vap *vap = cb_info->vap;
    ieee80211_ap_chan_info_t *ap_chan_info = cb_info->ap_chan_info;
    u_int32_t ap_chan_info_cnt = cb_info->ap_chan_info_cnt;
    u_int32_t ap_chan_info_cnt_max = cb_info->ap_chan_info_cnt_max;
    struct ieee80211_country_ie *country_ie;
    char country_iso[4];
    struct ieee80211_ath_channel *chan;
    u_int8_t op_class;

    if (ap_chan_info_cnt >= ap_chan_info_cnt_max)
        return -ENOMEM;

    country_ie = (struct ieee80211_country_ie *) util_scan_entry_country(se);
    chan = wlan_util_scan_entry_channel(se);
    if (country_ie) {
        op_class = regdmn_get_opclass (country_ie->cc, chan);
    } else {
        ieee80211_getCurrentCountryISO(vap->iv_ic, country_iso);
        op_class = regdmn_get_opclass (country_iso, chan);
    }

    ap_chan_info[ap_chan_info_cnt].op_class = op_class;
    ap_chan_info[ap_chan_info_cnt].channel = chan->ic_ieee;
    ap_chan_info_cnt++;
    cb_info->ap_chan_info_cnt = ap_chan_info_cnt;

    return QDF_STATUS_SUCCESS;
}


QDF_STATUS
ieee80211_update_ap_chan_rpt (struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211com *nbr_ic;
    struct ieee80211vap *nbr_vap, *tvap;
    ieee80211_ap_chan_info_t ap_chan_info[ACR_MAX_ENTRIES * ACR_MAX_CHANNELS];
    u_int32_t ap_chan_info_cnt = 0;
    ieee80211_acr_cbinfo_t cb_info;
    char country_iso[4];
    int i, j, k;
    int temp;
    int status = QDF_STATUS_SUCCESS;

    OS_MEMZERO(vap->ap_chan_rpt, sizeof (vap->ap_chan_rpt));
    vap->ap_chan_rpt_cnt = 0;

    /* Go through vap list of AP's own radios */
    for (i = 0; i < MAX_RADIO_CNT; i++) {
        GLOBAL_IC_LOCK_BH(vap->iv_ic->ic_global_list);
        nbr_ic = ic->ic_global_list->global_ic[i];
        GLOBAL_IC_UNLOCK_BH(vap->iv_ic->ic_global_list);

        if (!nbr_ic || TAILQ_EMPTY(&nbr_ic->ic_vaps))
            continue;

        TAILQ_FOREACH_SAFE(nbr_vap, &nbr_ic->ic_vaps, iv_next, tvap) {
            if (!OS_MEMCMP(nbr_vap->iv_myaddr, vap->iv_myaddr, IEEE80211_ADDR_LEN))
                continue;

            if (ap_chan_info_cnt >= ACR_MAX_ENTRIES * ACR_MAX_CHANNELS)
                break;

            ieee80211_getCurrentCountryISO(nbr_vap->iv_ic, country_iso);
            ap_chan_info[ap_chan_info_cnt].op_class = regdmn_get_opclass (country_iso, nbr_vap->iv_ic->ic_curchan);
            ap_chan_info[ap_chan_info_cnt].channel = nbr_vap->iv_bsschan->ic_ieee;
            ++ap_chan_info_cnt;
        }
    }

    /* Go through scan list of all radios */
    for (i = 0; i < MAX_RADIO_CNT; i++) {
        GLOBAL_IC_LOCK_BH(vap->iv_ic->ic_global_list);
        nbr_ic = ic->ic_global_list->global_ic[i];
        GLOBAL_IC_UNLOCK_BH(vap->iv_ic->ic_global_list);

        if (!nbr_ic || TAILQ_EMPTY(&nbr_ic->ic_vaps))
            continue;

        OS_MEMSET(&cb_info, 0, sizeof(cb_info));
        cb_info.vap = vap;
        cb_info.ap_chan_info = ap_chan_info;
        cb_info.ap_chan_info_cnt = ap_chan_info_cnt;
        cb_info.ap_chan_info_cnt_max = ACR_MAX_ENTRIES * ACR_MAX_CHANNELS;
        status = ucfg_scan_db_iterate(nbr_ic->ic_pdev_obj, ieee80211_fill_ap_chan_rpt_entry, &cb_info);
        ap_chan_info_cnt = cb_info.ap_chan_info_cnt;
        if (status != QDF_STATUS_SUCCESS)
            break;
    }

    /* Classify channel list per op_class */
    for (i = 0; i < ap_chan_info_cnt; i++) {
        status = ieee80211_add_ap_chan_rpt_entry(vap, ap_chan_info[i].op_class, ap_chan_info[i].channel);
        if (status != QDF_STATUS_SUCCESS)
            break;
    }

    /* Sort channel list for each op_class */
    for (i = 0; i < vap->ap_chan_rpt_cnt; i++) {
        for (j = 0; j < vap->ap_chan_rpt[i].chan_cnt - 1; j++) {
            for (k = j + 1; k < vap->ap_chan_rpt[i].chan_cnt; k++) {
                if (vap->ap_chan_rpt[i].chan[j] > vap->ap_chan_rpt[i].chan[k]) {
                    temp = vap->ap_chan_rpt[i].chan[j];
                    vap->ap_chan_rpt[i].chan[j] = vap->ap_chan_rpt[i].chan[k];
                    vap->ap_chan_rpt[i].chan[k] = temp;
                }
            }
        }
    }

    ieee80211vap_set_flag_ext2(vap, IEEE80211_FEXT2_MBO);

    return status;
}

/*
 * Add AP Channel Report information element to frame.
 */
u_int8_t *ieee80211_add_ap_chan_rpt_ie (u_int8_t *frm, struct ieee80211vap *vap)
{
    char country_iso[4];
    int i;

    if (vap->ap_chan_rpt_cnt) {
        for (i = 0; i < vap->ap_chan_rpt_cnt; i++) {
            *frm++ = IEEE80211_ELEMID_AP_CHAN_RPT;
            *frm++ = vap->ap_chan_rpt[i].chan_cnt + 1;
            *frm++ = vap->ap_chan_rpt[i].op_class;
            OS_MEMCPY (frm, vap->ap_chan_rpt[i].chan, vap->ap_chan_rpt[i].chan_cnt);
            frm += vap->ap_chan_rpt[i].chan_cnt;
        }
    } else {
        *frm++ = IEEE80211_ELEMID_AP_CHAN_RPT;
        *frm++ = 1;
        ieee80211_getCurrentCountryISO(vap->iv_ic, country_iso);
        *frm++ = regdmn_get_opclass (country_iso, vap->iv_ic->ic_curchan);
    }

    return frm;
}


bool ieee80211_bg_scan_enabled (struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211com *nbr_ic;
    int i;

    for (i = 0; i < MAX_RADIO_CNT; i++) {
        GLOBAL_IC_LOCK_BH(vap->iv_ic->ic_global_list);
        nbr_ic = ic->ic_global_list->global_ic[i];
        GLOBAL_IC_UNLOCK_BH(vap->iv_ic->ic_global_list);

        if (!nbr_ic || TAILQ_EMPTY(&nbr_ic->ic_vaps))
            continue;

        if (!ieee80211_acs_get_param(nbr_ic->ic_acs, IEEE80211_ACS_ENABLE_BK_SCANTIMER) ||
            !(ieee80211_acs_get_param(nbr_ic->ic_acs, IEEE80211_ACS_CTRLFLAG) & ACS_PEIODIC_BG_SCAN))
            break;
    }

    if (i == MAX_RADIO_CNT)
        return true;
    else
        return false;
}
