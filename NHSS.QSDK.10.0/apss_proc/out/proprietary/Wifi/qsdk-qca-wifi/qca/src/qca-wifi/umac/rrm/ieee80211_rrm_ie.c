/*
 *  Copyright (c) 2014 Qualcomm Atheros, Inc.  All rights reserved.
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners.
 */

/*
 *  Radio Resource measurements IE parsing/processing routines.
 */

#include <ieee80211_var.h>

#if UMAC_SUPPORT_RRM

#include <ieee80211_rrm.h>
#include "ieee80211_rrm_priv.h"

/**
 * @brief
 *
 * @param frm
 * @param ssid
 * @param len
 *
 * @return
 */
static u_int8_t *ieee80211_add_beaconreq_ssid(u_int8_t *frm, u_int8_t* ssid, u_int len)
{
    *frm++ = IEEE80211_SUBELEMID_BR_SSID;
    *frm++ = len;
    OS_MEMCPY(frm, ssid, len);
    return frm + len;
}
/**
 * @brief
 *
 * @param frm
 * @param binfo
 *
 * @return
 */
static u_int8_t *ieee80211_add_beaconreq_rinfo(u_int8_t *frm, ieee80211_rrm_beaconreq_info_t *binfo)
{
    struct ieee80211_beaconreq_rinfo* rinfo =
                              (struct ieee80211_beaconreq_rinfo *)(frm);
    int rinfo_len = sizeof(struct ieee80211_beaconreq_rinfo);
    rinfo->id = IEEE80211_SUBELEMID_BR_RINFO;
    rinfo->len = rinfo_len - 2;
    rinfo->cond = binfo->rep_cond;
    rinfo->refval = binfo->rep_thresh;
    return (frm + rinfo_len);

}

/**
 * @brief
 *
 * @param frm
 * @param binfo
 *
 * @return
 */
static u_int8_t *ieee80211_add_beaconreq_rdetail(u_int8_t *frm,
                       ieee80211_rrm_beaconreq_info_t *binfo)
{
    struct ieee80211_beaconrep_rdetail* rdetail =
                              (struct ieee80211_beaconrep_rdetail *)(frm);
    int rdetail_len = sizeof(struct ieee80211_beaconrep_rdetail);
    rdetail->id = IEEE80211_SUBELEMID_BR_RDETAIL;
    rdetail->len = rdetail_len - 2;
    rdetail->level = binfo->rep_detail;
    return (frm + rdetail_len);
}

/**
 * @brief
 *
 * @param frm
 *
 * @return
 */
static u_int8_t *ieee80211_add_beaconreq_reqie(struct ieee80211vap *vap, u_int8_t *frm,
                                               ieee80211_rrm_beaconreq_info_t *binfo) {
    if (binfo->req_ielen) {
        *frm++ = IEEE80211_SUBELEMID_BR_IEREQ;
        *frm = binfo->req_ielen;
        memcpy(frm, binfo->req_iebuf, binfo->req_ielen);
        frm += binfo->req_ielen;
    } else {
        if (ieee80211_vap_mbo_check(vap)) {
            *frm++ = IEEE80211_SUBELEMID_BR_IEREQ;
            *frm++ = 3;
            *frm++ = IEEE80211_ELEMID_SSID;
            *frm++ = IEEE80211_ELEMID_MOBILITY_DOMAIN;
            *frm++ = IEEE80211_ELEMID_VENDOR;
        } else {
            *frm++ = IEEE80211_SUBELEMID_BR_IEREQ;
            *frm++ = 8;
            *frm++ = IEEE80211_ELEMID_SSID;
            *frm++ = IEEE80211_ELEMID_RSN;
            *frm++ = IEEE80211_ELEMID_MOBILITY_DOMAIN;
            *frm++ = IEEE80211_ELEMID_RRM;
            *frm++ = IEEE80211_ELEMID_VENDOR;
            *frm++ = IEEE80211_ELEMID_VHTCAP;
            *frm++ = IEEE80211_ELEMID_QBSS_LOAD;
            *frm++ = IEEE80211_ELEMID_HTCAP_ANA;
        }
    }
    return frm;
}

/**
 * @brief
 *
 * @param frm
 * @param chaninfo
 *
 * @return
 */
static u_int8_t *ieee80211_add_beaconreq_chanrep(u_int8_t *frm,
                struct ieee80211_beaconreq_chaninfo *chaninfo)
{
    int i;
    *frm++ = IEEE80211_SUBELEMID_BR_CHANREP;
    *frm++ = chaninfo->numchans + 1 /* for reg class */;
    *frm++ = chaninfo->regclass;
    for (i = 0; i < chaninfo->numchans; i++) {
        *frm++ = chaninfo->channum[i];
    }
    return frm;
}

/**
 * @brief Add Last Beacon Report Indicator IE
 *
 * @param frm Pointer to the frame position
 *
 * @return Updated position in the frame
 */
static u_int8_t *ieee80211_add_beaconreq_lastind(u_int8_t *frm)
{
    *frm++ = IEEE80211_SUBELEMID_BR_LASTIND;
    *frm++ = 1;  /* length */
    *frm++ = 1;  /* flag to request inclusion of Last Beacon Report Indicator */
    return frm;
}

/**
 * @brief
 *
 * @param frm
 * @param tsminfo
 *
 * @return
 */
static u_int8_t* ieee80211_add_tsmreq_trigrep(u_int8_t *frm,
                   ieee80211_rrm_tsmreq_info_t* tsminfo)
{
    struct ieee80211_tsmreq_trigrep* trigrep =
                              (struct ieee80211_tsmreq_trigrep *)(frm);
    int trigrep_len = sizeof(struct ieee80211_tsmreq_trigrep);
    trigrep->id = IEEE80211_SUBELEMID_TSMREQ_TRIGREP;
    trigrep->len = trigrep_len - 2;
    trigrep->tc_avg = (tsminfo->trig_cond & IEEE80211_TSMREQ_TRIGREP_AVG) ? 1 : 0;
    trigrep->tc_cons = (tsminfo->trig_cond & IEEE80211_TSMREQ_TRIGREP_CONS) ? 1 : 0;
    trigrep->tc_delay = (tsminfo->trig_cond & IEEE80211_TSMREQ_TRIGREP_DELAY) ? 1 : 0;
    trigrep->avg_err_thresh = tsminfo->avg_err_thresh;
    trigrep->cons_err_thresh = tsminfo->cons_err_thresh;
    trigrep->delay_thresh = tsminfo->delay_thresh;
    trigrep->meas_count = tsminfo->meas_count;
    trigrep->trig_timeout = tsminfo->trig_timeout;
    return (frm + trigrep_len);
}

/**
 * @brief Add the optional preference subelement ID to a 
 *        neighbor report
 *
 * @param frm Pointer to the frame position for the start of the 
 *            preference subelement ID
 * @param preference Preference value (1 to 255, higher value is 
 *                   higher preference, 0 is reserved)
 *
 * @return Updated position in the frame to contain the next 
 *         element
 */
static u_int8_t *iee80211_add_nr_preference_subie(u_int8_t *frm, uint8_t preference)
{
    struct ieee80211_nr_preference_subie* pref = 
        (struct ieee80211_nr_preference_subie *)(frm);

    pref->id = IEEE80211_SUBELEMID_NEIGHBORREPORT_PREFERENCE;
    /* subtract 2 for the fixed length header (id + length) */
    pref->len = sizeof(struct ieee80211_nr_preference_subie) - 2;
    pref->preference = preference;

    return (frm + sizeof(struct ieee80211_nr_preference_subie));
}

/** Add Wide Bandwidth Channel subelement in neighbor report **/
static u_int8_t *iee80211_add_nr_wbc_subie(u_int8_t *frm, u_int8_t chwidth, u_int8_t cf_s1, u_int8_t cf_s2)
{
    struct ieee80211_nr_wbc_subie* wbc =
        (struct ieee80211_nr_wbc_subie *)(frm);

    wbc->id = IEEE80211_SUBELEMID_NEIGHBORREPORT_WBC;
    /* subtract 2 for the fixed length header (id + length) */
    wbc->len = sizeof(struct ieee80211_nr_wbc_subie) - 2;
    wbc->chwidth = chwidth;
    wbc->cf_s1 = cf_s1;
    wbc->cf_s2 = cf_s2;

    return (frm + sizeof(struct ieee80211_nr_wbc_subie));
}

/**
  * @brief
  *
  * @param ni pointer to the struct node
  * @param type type of the request
  *
  * @return retval
  */

u_int8_t ieee80211_rrm_get_measurement_token(struct ieee80211_node *ni,u_int8_t type)
{
    u_int8_t retval = 0;
    if(!ni)
        return EINVAL;
    switch(type)
    {
        case IEEE80211_MEASREQ_CHANNEL_LOAD_REQ:
            ni->chload_measure_token++;
            retval = ni->chload_measure_token;
            break;
        case IEEE80211_MEASREQ_CCA_REQ:
            ni->cca_measure_token++;
            retval = ni->cca_measure_token;
            break;
        case IEEE80211_MEASREQ_RPI_HISTOGRAM_REQ:
            ni->rpihist_measure_token++;
            retval = ni->nhist_measure_token;
            break;
        case IEEE80211_MEASREQ_NOISE_HISTOGRAM_REQ:
            ni->rpihist_measure_token++;
            retval = ni->rpihist_measure_token;
            break;
        case IEEE80211_MEASREQ_BR_TYPE:
            ni->br_measure_token++;
            retval = ni->br_measure_token;
            break;
        case IEEE80211_MEASREQ_FRAME_REQ:
            ni->frame_measure_token++;
            retval = ni->frame_measure_token;
            break;
        case IEEE80211_MEASREQ_STA_STATS_REQ:
            ni->stastats_measure_token++;
            retval = ni->stastats_measure_token;
            break;
        case IEEE80211_MEASREQ_LCI_REQ:
            ni->lci_measure_token++;
            retval = ni->lci_measure_token;
            break;
        case IEEE80211_MEASREQ_TSMREQ_TYPE:
            ni->tsm_measure_token++;
            retval = ni->tsm_measure_token;
            break;
        default:
            retval = EINVAL;
            break;
    }
    return retval;
}
/* Internal Functions */

/*
 * Add measurement request beacon IE
 */

/**
 * @brief
 *
 * @param frm
 * @param ni
 * @param binfo
 *
 * @return
 */
u_int8_t *ieee80211_add_measreq_beacon_ie(u_int8_t *frm, struct ieee80211_node *ni,
                                          ieee80211_rrm_beaconreq_info_t *binfo, u_int8_t *start)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_measreq_ie *measreq = (struct ieee80211_measreq_ie *)frm;
    struct ieee80211_beaconreq* beaconreq;
    u_int8_t token = 0;
    int i, remaining_space;

    OS_MEMZERO(measreq, sizeof(struct ieee80211_measreq_ie));
    measreq->id = IEEE80211_ELEMID_MEASREQ;
    token = ieee80211_rrm_get_measurement_token(ni,IEEE80211_MEASREQ_BR_TYPE);
    if(token != EINVAL) {
        measreq->token = token;
    }else {
        RRM_DEBUG(vap,RRM_DEBUG_VERBOSE,"Invalid token __investigate__ %d %pK \n",token,ni);
        measreq->token = IEEE80211_MEASREQ_BR_TOKEN ;
    }
    measreq->reqmode = binfo->reqmode;
    measreq->reqtype = IEEE80211_MEASREQ_BR_TYPE;
    beaconreq = (struct ieee80211_beaconreq *)(&measreq->req[0]);
    beaconreq->regclass = binfo->regclass;
    beaconreq->channum = binfo->channum;
    beaconreq->random_ivl = htole16(binfo->random_ivl);
    beaconreq->duration = htole16(binfo->duration);
    beaconreq->mode = binfo->mode;
    IEEE80211_ADDR_COPY(beaconreq->bssid, binfo->bssid);

    frm = (u_int8_t *)(&beaconreq->subelm[0]);
    if (binfo->req_ssid == IEEE80211_BCNREQUEST_VALIDSSID_REQUESTED) {
        if (!binfo->ssidlen) {
            frm = ieee80211_add_beaconreq_ssid(frm, vap->iv_bss->ni_essid, vap->iv_bss->ni_esslen);
        } else {
            frm = ieee80211_add_beaconreq_ssid(frm, binfo->ssid, binfo->ssidlen);
        }
    } else if (binfo->req_ssid == IEEE80211_BCNREQUEST_NULLSSID_REQUESTED) {
        /* wildcard ssid */
        frm = ieee80211_add_beaconreq_ssid(frm, frm, 0);
    }
    frm = ieee80211_add_beaconreq_rinfo(frm, binfo);

    frm = ieee80211_add_beaconreq_rdetail(frm, binfo);
    remaining_space = start - frm;
    if (binfo->req_ie) {
        if(binfo->req_ielen > remaining_space) {
            binfo->req_ielen = 0; // should we raise error ?
        }
        frm = ieee80211_add_beaconreq_reqie(vap, frm, binfo);
    }

    for (i = 0; i < binfo->num_chanrep; i++) {
        frm = ieee80211_add_beaconreq_chanrep(frm, &binfo->apchanrep[i]);
    }

    if (binfo->lastind) {
        frm = ieee80211_add_beaconreq_lastind(frm);
    }

    measreq->len = (frm - &(measreq->token));
    return frm;
}

/*
 * Add measurement request tsm IE */

 /**
 * @brief
 *
 * @param frm
 * @param tsminfo
 *
 * @return
 */
u_int8_t *ieee80211_add_measreq_tsm_ie(u_int8_t *frm, ieee80211_rrm_tsmreq_info_t* tsminfo, struct ieee80211_node *ni)
{
    struct ieee80211_measreq_ie *measreq = (struct ieee80211_measreq_ie *)frm;
    struct ieee80211_tsmreq* tsmreq;
    //struct  ieee80211vap *vap = ni->ni_vap;
    u_int8_t token = 0;

    OS_MEMZERO(measreq, sizeof(struct ieee80211_measreq_ie));
    measreq->id = IEEE80211_ELEMID_MEASREQ;
    token = ieee80211_rrm_get_measurement_token(ni, IEEE80211_MEASREQ_TSMREQ_TYPE);
    if(token != EINVAL) {
        measreq->token = token;
    } else {
        measreq->token = IEEE80211_MEASREQ_TSMREQ_TOKEN ;
    }
    measreq->reqmode = BIT_DUR;
    measreq->reqtype = IEEE80211_MEASREQ_TSMREQ_TYPE;
    tsmreq = (struct ieee80211_tsmreq *)(&measreq->req[0]);
    tsmreq->rand_ivl = htole16(tsminfo->rand_ivl);
    tsmreq->meas_dur = htole16(tsminfo->meas_dur);
    IEEE80211_ADDR_COPY(tsmreq->macaddr, tsminfo->macaddr);
    tsmreq->tid = tsminfo->tid;
    tsmreq->bin0_range = tsminfo->bin0_range;
    frm = (u_int8_t *)(&tsmreq->subelm[0]);
    if (tsminfo->trig_cond) {
        frm = ieee80211_add_tsmreq_trigrep(frm, tsminfo);
    }
    measreq->len = (frm - &(measreq->token));
    return frm;
}

/**
 * @brief Add the optional measurement element ID to a
 *        neighbor report
 *
 * @param frm Pointer to the frame position for the start of the
 *            preference subelement ID
 * @param neighbor report response data structure
 *
 *
 * @return Updated position in the frame to contain the next
 *         element
 */


extern u_int32_t ap_lcr[IEEE80211_LOC_CIVIC_REPORT_LEN];
extern int num_ap_lci;
extern u_int32_t ap_lci[IEEE80211_LOC_CIVIC_INFO_LEN];
extern int num_ap_lcr;

static u_int8_t *ieee80211_add_nr_meas_subie(u_int8_t *frm, struct ieee80211_nrresp_info* nr_info, u_int8_t meas_count)
{
    u_int8_t *p = (u_int8_t *) ap_lcr; /* Currently only 1 location stored in host cache. TBD: Extend location storage for nbrs */
    int len = 0;
    u_int8_t colocated_bss[IEEE80211_COLOCATED_BSSID_MAX_LEN]={0};

    struct ieee80211_measrsp_ie *measrsp = (struct ieee80211_measrsp_ie *)frm;
    struct ieee80211_nr_lci_subie *nr_lci;
    struct ieee80211_nr_lcr_subie *nr_lcr;
    u_int8_t *cur_lci = NULL;
    measrsp->id = IEEE80211_ELEMID_MEASREP;
    measrsp->token = nr_info->meas_token[meas_count];
    measrsp->rspmode = nr_info->meas_req_mode[meas_count];
    measrsp->rsptype = nr_info->meas_type[meas_count];

    switch(nr_info->meas_type[meas_count]) {
        case IEEE80211_MEASRSP_LCI_REPORT:
            nr_lci = (struct ieee80211_nr_lci_subie *) (&measrsp->rsp[0]);
            nr_lci->id = IEEE80211_SUBELEMID_LCI_RESERVED;
            nr_lci->len = IEEE80211_LOC_CIVIC_INFO_LEN;
            if(num_ap_lci == 0)
            {
                OS_MEMSET(&nr_lci->lci[0], 0, IEEE80211_LOC_CIVIC_INFO_LEN);
            } else {
                OS_MEMCPY(&nr_lci->lci[0], ap_lci, IEEE80211_LOC_CIVIC_INFO_LEN);
            }
            /* TBD: Sub-IE Z and Usage can exist and set even when LCI is 0 */
            nr_lci->z_id = IEEE80211_SUBELEMID_LCI_Z;
            nr_lci->z_len = IEEE80211_SUBELEM_LCI_Z_LEN;
            nr_lci->z_floor_info = IEEE80211_SUBELEM_LCI_Z_FLOOR_DEFAULT;
            nr_lci->z_height_above_floor = IEEE80211_SUBELEM_LCI_Z_HEIGHT_DEFAULT;
            nr_lci->z_uncertainty_height_above_floor = IEEE80211_SUBELEM_LCI_Z_UNCERT_DEFAULT;
            nr_lci->usage_rule_id = IEEE80211_SUBELEMID_LCI_USAGE;
            nr_lci->usage_param = IEEE80211_SUBELEM_LCI_USAGE_PARAM; /* 1 as per WFA */
            // For now "retention expires relative" does not exists
            nr_lci->usage_rule_len = IEEE80211_SUBELEM_LCI_USAGE_RULE_LEN;
            /* Adding all SubIEs. Note each SubIE has additional 2 bytes (subIE Id + Length field) */
            len = nr_lci->usage_rule_len + 2 +  nr_lci->z_len + 2 + nr_lci->len + 2;

            /* Check if colocated_bss contains more than 1 VAP, colocated_bss[1] has num_vaps */
            /* Add Colocated SubIE in Neighbor response only when 2 or more VAPs are present */
            if(nr_info->colocated_bss[1] > 1) {
                /* Convert num_vaps to represent octects: 6*Num_of_vap + 1 (Max BSSID Indicator field) */
                memcpy(&colocated_bss, nr_info->colocated_bss, IEEE80211_COLOCATED_BSSID_MAX_LEN);
                cur_lci = (u_int8_t *) nr_lci;
                cur_lci = cur_lci + len;
                colocated_bss[1] = (colocated_bss[1]*IEEE80211_ADDR_LEN)+1;
                memset(cur_lci, 0, colocated_bss[1]+2);
                memcpy(cur_lci, colocated_bss, colocated_bss[1]+2);
                /* Update length for LCI report */
                len = len + colocated_bss[1]+2;
            }
            break;

        case IEEE80211_MEASRSP_LOC_CIV_REPORT:
            nr_lcr = (struct ieee80211_nr_lcr_subie *)(&measrsp->rsp[0]);
            nr_lcr->civic_loc_type = IEEE80211_SUBELEM_LCR_CIVIC_LOC_TYPE;
            nr_lcr->id = IEEE80211_SUBELEM_LCR_ID_DEFAULT;
            if(num_ap_lcr == 0)
            {
                /* When Civic length is 0, Measurement subelement length is 6 */
                len = IEEE80211_SUBELEM_LOC_CIVIC_DEFAULT_LEN;
                OS_MEMSET(&nr_lcr->lcr[0], 0, len);
            } else {
		p = (u_int8_t *) ap_lcr;
                /* Here p is at start of civic_info[64] defined in rtt.h. Move 3 bytes to get to CivicAddress */
                p = p+3;
                /* Total len = Len of Civic addr (as stored in 3rd byte) + 4 (Country+CAtype+Len) */
                nr_lcr->len = *p + (IEEE80211_LCR_COUNTRY_FIELD_SIZE +
                                  IEEE80211_LCR_CATYPE_FIELD_SIZE + IEEE80211_LCR_LENGTH_FIELD_SIZE);
                len = nr_lcr->len;
                /* civic_info[64] defined in rtt.h goes over the air includes Country, CAtype, Len, Civic Addr */
                OS_MEMCPY(&nr_lcr->lcr[0], ap_lcr, len);
                /* Add octets for civic location type, civic subelement Id and length */
                len = len + (IEEE80211_SUBELEM_LCR_TYPE_FIELD_SIZE +
                          IEEE80211_SUBELEM_LCR_ID_FIELD_SIZE + IEEE80211_SUBELEM_LCR_LENGTH_FIELD_SIZE);
            }
            break;

        default:
            break;
    }
    /* Add size of Measurement report fields (Token, Report Mode, Measurement Type */
    measrsp->len = len + (IEEE80211_MEASREPORT_TOKEN_SIZE +
                         IEEE80211_MEASREPORT_MODE_SIZE + IEEE80211_MEASREPORT_TYPE_SIZE);
    /* Add size for subElement ID field + Length field */
    return (frm + measrsp->len + 2);
}


/*
 * Add Neigbor Report IE
 */

/**
 * @brief
 *
 * @param frm
 * @param nr_info
 *
 * @return
 */
u_int8_t *ieee80211_add_nr_ie(u_int8_t *frm, struct ieee80211_nrresp_info* nr_info)
{
    struct ieee80211_nr_ie *nr = (struct ieee80211_nr_ie *)frm;
    u_int8_t meas_count = 0;
    OS_MEMZERO(nr, sizeof(struct ieee80211_nr_ie));
    nr->id = IEEE80211_ELEMID_NEIGHBOR_REPORT;
    IEEE80211_ADDR_COPY(nr->bssid, nr_info->bssid);
    nr->regclass = nr_info->regclass;
    nr->channum = nr_info->channum;
    nr->phytype = nr_info->phytype;

    /* TBD - Need to check actual RSN cap of the node */
    nr->bsinfo0_security = 1;

    if (nr_info->capinfo & IEEE80211_CAPINFO_SPECTRUM_MGMT) {
        nr->bsinfo0_specmgmt = 1;
    }
    if (nr_info->capinfo & IEEE80211_CAPINFO_QOS) {
        nr->bsinfo0_qos = 1;
    }
    if (nr_info->capinfo & IEEE80211_CAPINFO_APSD) {
        nr->bsinfo0_apsd = 1;
    }
    if (nr_info->capinfo & IEEE80211_CAPINFO_RADIOMEAS) {
        nr->bsinfo0_rrm = 1;
    }
    nr->bsinfo1_ht = (nr_info->is_ht) ? 1 : 0;
    nr->bsinfo1_vht = (nr_info->is_vht) ? 1 : 0;
    nr->bsinfo1_ftm = (nr_info->is_ftm) ? 1 : 0;
    nr->bsinfo1_he = (nr_info->is_he) ? 1 : 0;
    nr->bsinfo1_he_er = (nr_info->is_he_er_su) ? 1 : 0;

    /* TBD - Should be based RSSI strength */
    nr->bsinfo0_ap_reach = 3;

    frm = (u_int8_t *)(&nr->subelm[0]);
    /* Add sub elements */
    /* Add Wide Bandwidth Channel optional subelement */
    frm = iee80211_add_nr_wbc_subie(frm, nr_info->chwidth, nr_info->cf_s1, nr_info->cf_s2);

    frm = iee80211_add_nr_preference_subie(frm, nr_info->preference);

    // LCI and/or LCR measurement request may come any order
    while(nr_info->meas_count)
    {
        frm = ieee80211_add_nr_meas_subie(frm, nr_info, meas_count);
        meas_count++;
        nr_info->meas_count--;
    }

    nr->len = (frm - &nr->bssid[0]); 
    return frm;
}

/* External Functions */

/*
 * Add RRM capability IE
 */

/**
 * @brief
 *
 * @param frm
 * @param ni
 *
 * @return
 */
u_int8_t *ieee80211_add_rrm_cap_ie(u_int8_t *frm, struct ieee80211_node *ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_rrm_cap_ie *rrmcap = (struct ieee80211_rrm_cap_ie *)frm;
    int rrmcap_len = sizeof(struct ieee80211_rrm_cap_ie);
    if (ieee80211_vap_rrm_is_set(vap)) {
        OS_MEMZERO(rrmcap, sizeof(struct ieee80211_rrm_cap_ie));
        rrmcap->id = IEEE80211_ELEMID_RRM;
        rrmcap->len = rrmcap_len - 2;
        rrmcap->lnk_meas = 1;
        rrmcap->neig_rpt = 1;
        rrmcap->bcn_passive = 1;
        rrmcap->bcn_active = 1;
        rrmcap->bcn_table = 1;
        rrmcap->tsm_meas = 1;
        rrmcap->trig_tsm_meas = 1;
        rrmcap->lci_meas = 1;
        rrmcap->civ_loc_meas = 1;
        rrmcap->ftm_range_report = 1;

        return (frm + rrmcap_len);
    }
    else {
        return frm;
    }
}

/**
 * @brief
 *
 * @param frm
 * @param type
 * @param token
 *
 * @return
 */
static u_int8_t *ieee80211_add_rrm_ie(u_int8_t *frm,u_int8_t type,u_int8_t token)
{

    struct ieee80211_measreq_ie *measreq = (struct ieee80211_measreq_ie *)frm;
    measreq->id = IEEE80211_ELEMID_MEASREQ;
    measreq->token = token;
    measreq->reqmode = BIT_ENABLE | BIT_DUR;
    measreq->reqtype = type;
    frm =(u_int8_t *)(&measreq->reqtype);
    frm++;/* Moving to next byte */
    return frm;
}

/**
 * @brief
 *
 * @param frm
 * @param ni
 * @param params
 *
 * @return
 */
u_int8_t *ieee80211_add_measreq_stastats_ie(u_int8_t *frm, struct ieee80211_node *ni,
                       ieee80211_rrm_stastats_info_t *params)
{
    struct ieee80211_measreq_ie *measreq = (struct ieee80211_measreq_ie *)frm;
    struct ieee80211_stastatsreq *statsreq;
    u_int8_t token = 0;

    token = ieee80211_rrm_get_measurement_token(ni, IEEE80211_MEASREQ_STA_STATS_REQ);
    if(token != EINVAL) {
        measreq->token = token;
    }else {
        measreq->token = IEEE80211_MEASREQ_STASTATS_TOKEN ;
    }
	frm = ieee80211_add_rrm_ie(frm,IEEE80211_MEASREQ_STA_STATS_REQ, measreq->token);
    statsreq = (struct ieee80211_stastatsreq *)(&measreq->req[0]);
    IEEE80211_ADDR_COPY(statsreq->dstmac, params->dstmac);
    statsreq->rintvl = htole16(params->r_invl);
    statsreq->mduration = htole16(params->m_dur);
    statsreq->gid = params->gid;
    frm = (u_int8_t *)(&statsreq->req[0]);
    measreq->len = (frm - &(measreq->token));
    return frm;
}


/**
 * @brief
 *
 * @param frm
 * @param nhist
 *
 * @return
 */
u_int8_t *ieee80211_add_nhist_opt_ie(u_int8_t *frm,ieee80211_rrm_nhist_info_t *nhist)
{
    *frm++ = IEEE80211_SUBELEMID_NHIST_CONDITION;
    *frm++ = nhist->cond;
    *frm++ = nhist->c_val;
    return frm;
}

/**
 * @brief
 *
 * @param frm
 * @param action
 * @param n_rpt
 *
 * @return
 */
u_int8_t *ieee80211_add_rrm_action_ie(u_int8_t *frm,u_int8_t action, u_int16_t n_rpt,
        struct ieee80211_node *ni)
{
    struct ieee80211_action_rm_req *req = (struct ieee80211_action_rm_req*)(frm);
    req->header.ia_category = IEEE80211_ACTION_CAT_RM;
    req->header.ia_action = action;

    switch(action)
    {
        case IEEE80211_ACTION_RM_REQ:
            req->dialogtoken = ni->rrm_dialog_token;
            ni->rrm_dialog_token ++;
            break;
        case IEEE80211_ACTION_LM_REQ:
            req->dialogtoken = ni->lm_dialog_token;
            ni->lm_dialog_token ++;
            break;
        case IEEE80211_ACTION_NR_REQ:
            req->dialogtoken = ni->nr_dialog_token;
            ni->nr_dialog_token ++;
            break;
        default:
            req->dialogtoken = ni->rrm_dialog_token;
            ni->rrm_dialog_token ++;
            break;
    }
    req->num_rpts = htole16(n_rpt);
    frm += (sizeof(struct ieee80211_action_rm_req) - 1);
    return frm;
}

/**
 * @brief
 *
 * @param frm
 * @param ni
 * @param fr_info
 *
 * @return
 */
u_int8_t *ieee80211_add_measreq_frame_req_ie(u_int8_t *frm, struct ieee80211_node *ni,
                       ieee80211_rrm_frame_req_info_t  *fr_info)
{

    struct ieee80211_measreq_ie *measreq = (struct ieee80211_measreq_ie *)frm;
	struct ieee80211_frame_req *frame_req;
    u_int8_t token = 0;

    token = ieee80211_rrm_get_measurement_token(ni, IEEE80211_MEASREQ_FRAME_REQ);
    if(token != EINVAL) {
        measreq->token = token;
    } else {
        measreq->token = IEEE80211_MEASREQ_FRAME_TOKEN ;
    }
    frm = ieee80211_add_rrm_ie(frm,IEEE80211_MEASREQ_FRAME_REQ,measreq->token);
	frame_req = (struct ieee80211_frame_req *)frm;
	frame_req->regclass = fr_info->regclass;
	frame_req->chnum = fr_info->chnum;
	frame_req->rintvl = htole16(fr_info->r_invl);
	frame_req->mduration = htole16(fr_info->m_dur);
	frame_req->ftype = fr_info->ftype;
	frm = (u_int8_t *)(frame_req->req);
	measreq->len = (frm - &(measreq->token));
	return frm;
}
u_int8_t *ieee80211_add_lcireq_opt_ie(u_int8_t *frm, ieee80211_rrm_lcireq_info_t *lcireq_info)
{
    *frm++ = IEEE80211_SUBELEMID_LC_AZIMUTH_CONDITION;
    *frm++ = 1; /* it is extensible will change it later */
    *frm++ = (lcireq_info->azi_res & 0x0f) | (lcireq_info->azi_type >> 4);

    return frm;
}

u_int8_t *ieee80211_add_measreq_lci_ie(u_int8_t *frm, struct ieee80211_node *ni,
                       ieee80211_rrm_lcireq_info_t *lcireq_info)
{
    struct ieee80211_measreq_ie *measreq = (struct ieee80211_measreq_ie *)frm;
    struct ieee80211_lcireq *lcireq;
    u_int8_t token = 0;

    token = ieee80211_rrm_get_measurement_token(ni, IEEE80211_MEASREQ_LCI_REQ);
    if(token != EINVAL) {
        measreq->token = token;
    } else {
        measreq->token = IEEE80211_MEASREQ_LCI_TOKEN;
    }
    frm = ieee80211_add_rrm_ie(frm,IEEE80211_MEASREQ_LCI_REQ, measreq->token);
    lcireq = (struct ieee80211_lcireq *)frm;
    lcireq->location = lcireq_info->location;
    lcireq->lat_res = lcireq_info->lat_res;
    lcireq->long_res =lcireq_info->long_res;
    lcireq->alt_res = lcireq_info->alt_res;
    frm = (u_int8_t *)(&lcireq->req[0]);

    if(lcireq_info->azi_res)
        frm = ieee80211_add_lcireq_opt_ie(frm,lcireq_info);

    measreq->len = (frm - &(measreq->token));
    return frm;
}

/**
 * @brief
 *
 * @param frm
 * @param ni
 * @param cca_info
 *
 * @return
 */
u_int8_t *ieee80211_add_measreq_cca_ie(u_int8_t *frm, struct ieee80211_node *ni,
                       ieee80211_rrm_cca_info_t *cca_info)
{
    struct ieee80211_measreq_ie *measreq = (struct ieee80211_measreq_ie *)frm;

    struct ieee80211_ccareq *ccareq;
    u_int8_t token = 0;

    token = ieee80211_rrm_get_measurement_token(ni, IEEE80211_MEASREQ_CCA_REQ);
    if(token != EINVAL) {
        measreq->token = token;
    }else {
        measreq->token = IEEE80211_MEASREQ_CCA_TOKEN ;
    }
    frm = ieee80211_add_rrm_ie(frm,IEEE80211_MEASREQ_CCA_REQ, measreq->token);
    ccareq = (struct ieee80211_ccareq *)frm;
    ccareq->chnum = cca_info->chnum;
    ccareq->tsf = htole64(cca_info->tsf);
    ccareq->mduration = htole16(cca_info->m_dur);

    frm = (u_int8_t *)(&ccareq->req[0]);
    measreq->len = (frm - &(measreq->token));

    return frm;
}

/**
 * @brief
 *
 * @param frm
 * @param ni
 * @param rpihist_info
 *
 * @return
 */
u_int8_t *ieee80211_add_measreq_rpihist_ie(u_int8_t *frm, struct ieee80211_node *ni,
                       ieee80211_rrm_rpihist_info_t *rpihist_info)
{
    struct ieee80211_measreq_ie *measreq = (struct ieee80211_measreq_ie *)frm;

    struct ieee80211_rpihistreq *rpihistreq;
    u_int8_t token = 0;

    token = ieee80211_rrm_get_measurement_token(ni, IEEE80211_MEASREQ_RPI_HISTOGRAM_REQ);
    if(token != EINVAL) {
        measreq->token = token;
    }else {
        measreq->token = IEEE80211_MEASREQ_RPIHIST_TOKEN ;
    }
    frm = ieee80211_add_rrm_ie(frm,IEEE80211_MEASREQ_RPI_HISTOGRAM_REQ, measreq->token);
    rpihistreq = (struct ieee80211_rpihistreq *)frm;
    rpihistreq->chnum = rpihist_info->chnum;
    rpihistreq->tsf = htole64(rpihist_info->tsf);
    rpihistreq->mduration = htole16(rpihist_info->m_dur);

    frm = (u_int8_t *)(&rpihistreq->req[0]);
    measreq->len = (frm - &(measreq->token));

    return frm;
}

/**
 * @brief
 *
 * @param frm
 * @param ni
 * @param nhist_info
 *
 * @return
 */
u_int8_t *ieee80211_add_measreq_nhist_ie(u_int8_t *frm, struct ieee80211_node *ni,
                       ieee80211_rrm_nhist_info_t *nhist_info)
{
    struct ieee80211_measreq_ie *measreq = (struct ieee80211_measreq_ie *)frm;

    struct ieee80211_nhistreq *nhistreq;
    u_int8_t token = 0;

    token = ieee80211_rrm_get_measurement_token(ni, IEEE80211_MEASREQ_NOISE_HISTOGRAM_REQ);
    if(token != EINVAL) {
        measreq->token = token;
    }else {
        measreq->token = IEEE80211_MEASREQ_NHIST_TOKEN ;
    }
    frm = ieee80211_add_rrm_ie(frm,IEEE80211_MEASREQ_NOISE_HISTOGRAM_REQ, measreq->token);
	nhistreq = (struct ieee80211_nhistreq *)frm;
	nhistreq->regclass = nhist_info->regclass;
	nhistreq->chnum = nhist_info->chnum;
	nhistreq->rintvl = htole16(nhist_info->r_invl);
	nhistreq->mduration = htole16(nhist_info->m_dur);

    frm = (u_int8_t *)(&nhistreq->req[0]);

    if(nhist_info->cond)
        frm = ieee80211_add_nhist_opt_ie(frm,nhist_info);

    measreq->len = (frm - &(measreq->token));

    return frm;
}

/**
 * @brief
 *
 * @param frm
 * @param chinfo
 *
 * @return
 */
u_int8_t *ieee80211_add_chload_opt_ie(u_int8_t *frm,ieee80211_rrm_chloadreq_info_t *chinfo)
{
    *frm++ = IEEE80211_SUBELEMID_CHLOAD_CONDITION;
    *frm++ = chinfo->cond;
    *frm++ = chinfo->c_val;
    return frm;
}

/**
 * @brief
 *
 * @param frm
 * @param ni
 * @param chinfo
 *
 * @return
 */
u_int8_t *ieee80211_add_measreq_chload_ie(u_int8_t *frm, struct ieee80211_node *ni,
        ieee80211_rrm_chloadreq_info_t *chinfo)
{
    struct ieee80211_measreq_ie *measreq = (struct ieee80211_measreq_ie *)frm;

    struct ieee80211_chloadreq * chloadreq;
    u_int8_t token = 0;

    token = ieee80211_rrm_get_measurement_token(ni,IEEE80211_MEASREQ_CHANNEL_LOAD_REQ);
    if(token != EINVAL) {
        measreq->token = token;
    } else {
        measreq->token = IEEE80211_MEASREQ_CHLOAD_TOKEN ;
    }
    frm = ieee80211_add_rrm_ie(frm,IEEE80211_MEASREQ_CHANNEL_LOAD_REQ, measreq->token);

    chloadreq = (struct ieee80211_chloadreq *)(frm);

    chloadreq->regclass = chinfo->regclass;

    chloadreq->chnum = chinfo->chnum;

    chloadreq->rintvl = htole16(chinfo->r_invl);

    chloadreq->mduration = htole16(chinfo->m_dur);

    frm = (u_int8_t *)(&chloadreq->req[0]);

    if(chinfo->cond)
        frm = ieee80211_add_chload_opt_ie(frm,chinfo);

    measreq->len = (frm - &(measreq->token));

    return frm;
}
#endif /* UMAC_SUPPORT_RRM */
