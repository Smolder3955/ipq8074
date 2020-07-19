/*
 *  Copyright (c) 2014 Qualcomm Atheros, Inc.  All rights reserved. 
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners. 
 */

#include <ieee80211_var.h>

#ifndef _IEEE80211_RRM__PRIV_H_
#define _IEEE80211_RRM__PRIV_H_

#if UMAC_SUPPORT_RRM

/*
 * Radio Resource Protocol definitions
 */
#define RRM_MIN_RSP_LEN                     3 /*Minimum length of rrm response */

#define RRM_BTABLE_LOCK_INIT(handle)         spin_lock_init(&handle->lock)
#define RRM_BTABLE_LOCK_DESTROY(handle)      spin_lock_destroy(&handle->lock) 
#define RRM_BTABLE_LOCK(handle)              spin_lock(&handle->lock)
#define RRM_BTABLE_UNLOCK(handle)            spin_unlock(&handle->lock)

/* RRM action fields */
#define IEEE80211_ACTION_RM_REQ             0
#define IEEE80211_ACTION_RM_RESP            1
#define IEEE80211_ACTION_LM_REQ             2
#define IEEE80211_ACTION_LM_RESP            3
#define IEEE80211_ACTION_NR_REQ             4
#define IEEE80211_ACTION_NR_RESP            5

/* RRM action frame dialogtokens */
#define IEEE80211_ACTION_RM_TOKEN    1
#define IEEE80211_ACTION_LM_TOKEN    1
#define IEEE80211_ACTION_NR_TOKEN    1

/* Measurement Request Tokens */
#define IEEE80211_MEASREQ_BR_TOKEN              1
#define IEEE80211_MEASREQ_CHLOAD_TOKEN          1
#define IEEE80211_MEASREQ_STASTATS_TOKEN        1
#define IEEE80211_MEASREQ_NHIST_TOKEN           1
#define IEEE80211_MEASREQ_RPIHIST_TOKEN         1
#define IEEE80211_MEASREQ_CCA_TOKEN             1
#define IEEE80211_MEASREQ_TSMREQ_TOKEN          1
#define IEEE80211_MEASREQ_FRAME_TOKEN           1
#define IEEE80211_MEASREQ_LCI_TOKEN             1

/* Measurement Request Type */
#define IEEE80211_MEASREQ_BASIC_REQ            0
#define IEEE80211_MEASREQ_CCA_REQ              1
#define IEEE80211_MEASREQ_RPI_HISTOGRAM_REQ    2
#define IEEE80211_MEASREQ_CHANNEL_LOAD_REQ     3
#define IEEE80211_MEASREQ_NOISE_HISTOGRAM_REQ  4
#define IEEE80211_MEASREQ_BR_TYPE              5
#define IEEE80211_MEASREQ_FRAME_REQ 	       6
#define IEEE80211_MEASREQ_STA_STATS_REQ        7
#define IEEE80211_MEASREQ_LCI_REQ	           8
#define IEEE80211_MEASREQ_TSMREQ_TYPE          9
#define IEEE80211_MEASREQ_LCR_REQ	           11
#define IEEE80211_MEASREQ_FTMRR                16
#define IEEE80211_MEASREQ_OTHER             0xff


#define IEEE80211_MEASRSP_BASIC_REPORT            0
#define IEEE80211_MEASRSP_CCA_REPORT              1
#define IEEE80211_MEASRSP_RPI_HISTOGRAM_REPORT    2
#define IEEE80211_MEASRSP_CHANNEL_LOAD_REPORT     3
#define IEEE80211_MEASRSP_NOISE_HISTOGRAM_REPORT  4
#define IEEE80211_MEASRSP_BEACON_REPORT           5
#define IEEE80211_MEASRSP_FRAME_REPORT            6
#define IEEE80211_MEASRSP_STA_STATS_REPORT        7
#define IEEE80211_MEASRSP_LCI_REPORT              8
#define IEEE80211_MEASRSP_TSM_REPORT              9
#define IEEE80211_MEASRSP_LOC_CIV_REPORT          11

#define IEEE80211_LOC_CIVIC_INFO_LEN		16
#define IEEE80211_LOC_CIVIC_REPORT_LEN		64
#define IEEE80211_SUBELEM_LCI_Z_LEN		6
#define IEEE80211_SUBELEM_LCI_Z_FLOOR_DEFAULT	0
#define IEEE80211_SUBELEM_LCI_Z_HEIGHT_DEFAULT	0
#define IEEE80211_SUBELEM_LCI_Z_UNCERT_DEFAULT	0
#define IEEE80211_SUBELEM_LCI_USAGE_RULE_LEN	1
#define IEEE80211_SUBELEM_LCI_USAGE_PARAM	1
#define IEEE80211_SUBELEM_LOC_CIVIC_DEFAULT_LEN 6
#define IEEE80211_SUBELEM_LCR_CIVIC_LOC_TYPE	0
#define IEEE80211_SUBELEM_LCR_ID_DEFAULT	0
#define IEEE80211_SUBELEM_LCR_TYPE_FIELD_SIZE	1
#define IEEE80211_SUBELEM_LCR_ID_FIELD_SIZE	1
#define IEEE80211_SUBELEM_LCR_LENGTH_FIELD_SIZE 1
#define IEEE80211_LCR_COUNTRY_FIELD_SIZE	2
#define IEEE80211_LCR_CATYPE_FIELD_SIZE	1
#define IEEE80211_LCR_LENGTH_FIELD_SIZE	1
#define IEEE80211_MEASREPORT_TOKEN_SIZE	1
#define IEEE80211_MEASREPORT_MODE_SIZE		1
#define IEEE80211_MEASREPORT_TYPE_SIZE	 	1
#define IEEE80211_NRREQ_MEASREQ_FIELDS_LEN	9

#if UMAC_SUPPORT_RRM_DEBUG
#define RRM_DEBUG_DEFAULT  0x0
#define RRM_DEBUG_VERBOSE  0x1
#define RRM_DEBUG_PKT_DUMP 0x2
#define RRM_DEBUG_INFO     0x4
#define RRM_DEBUG_API      0x10
#define RRM_DEBUG(vap,rrm_db_lvl,format,...) 	\
        do						\
        {						\
	    if (vap->rrm->rrm_dbg_lvl & rrm_db_lvl) 	\
            {						\
              IEEE80211_DPRINTF(vap, IEEE80211_MSG_RRM,format,##__VA_ARGS__);		\
            }						\
        } while(0)
#define RRM_FUNCTION_ENTER  RRM_DEBUG(vap,RRM_DEBUG_API,"**** Entering Function %s ****\n",__func__)
#define RRM_FUNCTION_EXIT   RRM_DEBUG(vap,RRM_DEBUG_API,"**** Exiting Function %s  ****\n",__func__)  
#else
#define RRM_DEBUG(vap, rrm_db_lvl, format, ...)
#define RRM_FUNCTION_ENTER 
#define RRM_FUNCTION_EXIT 
#endif



enum {
    IEEE80211_STASTATS_GID0=0,
    IEEE80211_STASTATS_GID1,
    IEEE80211_STASTATS_GID2,
    IEEE80211_STASTATS_GID3,
    IEEE80211_STASTATS_GID4,
    IEEE80211_STASTATS_GID5,
    IEEE80211_STASTATS_GID6,
    IEEE80211_STASTATS_GID7,
    IEEE80211_STASTATS_GID8,
    IEEE80211_STASTATS_GID9,
    IEEE80211_STASTATS_GID10,
};
enum {
    IEEE80211_SUBELEMID_BR_SSID = 0,
    IEEE80211_SUBELEMID_BR_RINFO = 1,
    IEEE80211_SUBELEMID_BR_RDETAIL = 2,
    IEEE80211_SUBELEMID_BR_IEREQ = 10,
    IEEE80211_SUBELEMID_BR_CHANREP = 51,
    IEEE80211_SUBELEMID_BR_LASTIND = 164,
};

enum {
    IEEE80211_SUBELEMID_CHLOAD_RESERVED = 0,
    IEEE80211_SUBELEMID_CHLOAD_CONDITION = 1,
    IEEE80211_SUBELEMID_CHLOAD_VENDOR = 221,
};
enum {
    IEEE80211_SUBELEMID_LC_RESERVED = 0,
    IEEE80211_SUBELEMID_LC_AZIMUTH_CONDITION = 1,
    IEEE80211_SUBELEMID_LC_VENDOR = 221,
};


enum {
    IEEE80211_SUBELEMID_NHIST_RESERVED = 0,
    IEEE80211_SUBELEMID_NHIST_CONDITION = 1,
    IEEE80211_SUBELEMID_NHIST_VENDOR = 221,
};

enum {
    IEEE80211_SUBELEMID_TSMREQ_TRIGREP = 1,
};

enum {
    IEEE80211_SUBELEMID_BREPORT_RESERVED = 0,
    IEEE80211_SUBELEMID_BREPORT_FRAME_BODY = 1,
};

enum {
    IEEE80211_SUBELEMID_FR_REPORT_RESERVED = 0,
    IEEE80211_SUBELEMID_FR_REPORT_COUNT = 1,
};
enum {
    IEEE80211_SUBELEMID_LCI_RESERVED = 0,
    IEEE80211_SUBELEMID_LCI_AZIMUTH_REPORT = 1,
    IEEE80211_SUBELEMID_LCI_Z = 4,
    IEEE80211_SUBELEMID_LCI_USAGE = 6,
    IEEE80211_SUBELEMID_LCI_VENDOR = 221,
};
enum {
    IEEE80211_SUBELEMID_NEIGHBORREPORT_RESERVED = 0,
    IEEE80211_SUBELEMID_NEIGHBORREPORT_PREFERENCE = 3,
    IEEE80211_SUBELEMID_NEIGHBORREPORT_WBC = 6,
};
enum {
    IEEE80211_RRM_LCI_ID=0,
    IEEE80211_RRM_LCI_LEN=1,
    IEEE80211_RRM_LCI_LAT_RES = 2,
    IEEE80211_RRM_LCI_LAT_FRAC = 3,
    IEEE80211_RRM_LCI_LAT_INT = 4,
    IEEE80211_RRM_LCI_LONG_RES = 5,
    IEEE80211_RRM_LCI_LONG_FRAC = 6,
    IEEE80211_RRM_LCI_LONG_INT = 7,
    IEEE80211_RRM_LCI_ALT_TYPE = 8,
    IEEE80211_RRM_LCI_ALT_RES = 9,
    IEEE80211_RRM_LCI_ALT_FRAC = 10,
    IEEE80211_RRM_LCI_ALT_INT = 11,
    IEEE80211_RRM_LCI_DATUM = 12,
    IEEE80211_RRM_LCI_LAST = 13,
};

struct u_nhist_resp
{

    u_int8_t ipi[10];
    u_int8_t anpi;
    u_int8_t antid;

};

struct ieee80211_subelm_header {
    u_int8_t    subelm_id;
    u_int8_t    len;
} __packed;

/* Measurement request information element */
struct ieee80211_measreq_ie {
    u_int8_t    id;         /* IEEE80211_ELEMID_MEASREQ */
    u_int8_t    len;         
    u_int8_t    token;
    u_int8_t    reqmode;
    u_int8_t    reqtype;
    u_int8_t    req[1];     /* varialbe len measurement requet */
} __packed; 

/* Beacon report request */
struct ieee80211_beaconreq {
    u_int8_t    regclass;         
    u_int8_t    channum;
    u_int16_t   random_ivl;
    u_int16_t   duration;
    u_int8_t    mode;
    u_int8_t    bssid[6];
    u_int8_t    subelm[1];     /* varialbe len sub element fileds */
} __packed;

/* Beacon report request - ssid information element */
struct ieee80211_beaconreq_ssid {
    u_int8_t id;
    u_int8_t len;
    u_int8_t ssid[1];  /* Variable len ssid string */
} __packed;

/* Beacon report request - report information element */
struct ieee80211_beaconreq_rinfo {
    u_int8_t id;
    u_int8_t len;
    u_int8_t cond;
    u_int8_t refval;
} __packed;

/* Beacon report request - report detail element */
struct ieee80211_beaconrep_rdetail {
    u_int8_t id;
    u_int8_t len;
    u_int8_t level;
} __packed;

/* Beacon report request - request IE element */
struct ieee80211_beaconrep_iereq {
    u_int8_t id;
    u_int8_t len;
    u_int8_t iereq[1];  /* Variable len of requested ies */
} __packed;

/* Beacon report request - channel report element */
struct ieee80211_beaconrep_chanrep {
    u_int8_t id;
    u_int8_t len;
    u_int8_t regclass;
    u_int8_t chans[1];  /* Variable len of requested channels */
} __packed;

#define IEEE80211_TSMREQ_TRIGREP_AVG (0x01)
#define IEEE80211_TSMREQ_TRIGREP_CONS (0x02)
#define IEEE80211_TSMREQ_TRIGREP_DELAY (0x04)


/* Traffic stream measurement Report req IE */
struct ieee80211_tsmreq {
    u_int16_t   rand_ivl;
    u_int16_t   meas_dur;
    u_int8_t    macaddr[6];
#if _BYTE_ORDER == _BIG_ENDIAN
    u_int8_t    tid:4;             /* B4 - B7 */
    u_int8_t    tid_reserved:4;    /* B0 - B4 */
#else
    u_int8_t    tid_reserved:4;    /* B0 - B4 */
    u_int8_t    tid:4;             /* B4 - B7 */
#endif
    u_int8_t    bin0_range;
    u_int8_t    subelm[1];     /* varialbe len sub element fileds */
} __packed;

/*  tsm request - trigger report element */
struct ieee80211_tsmreq_trigrep {
    u_int8_t id;
    u_int8_t len;
#if _BYTE_ORDER == _BIG_ENDIAN
    u_int8_t tc_reserved :5;  /* B3 - B7 */
    u_int8_t tc_delay :1;     /* B2 */
    u_int8_t tc_cons :1;      /* B1 */
    u_int8_t tc_avg :1;       /* B0 */
#else
    u_int8_t tc_avg :1;       /* B0 */
    u_int8_t tc_cons :1;      /* B1 */
    u_int8_t tc_delay :1;     /* B2 */
    u_int8_t tc_reserved :5;  /* B3 - B7 */
#endif
    u_int8_t avg_err_thresh;
    u_int8_t cons_err_thresh;
    u_int8_t delay_thresh;
    u_int8_t meas_count;
    u_int8_t trig_timeout;
    
} __packed;

/*  Neigbor report information element */
struct ieee80211_nr_ie {
    u_int8_t    id;         /* IEEE80211_ELEMID_NEIGHBORREPORT */
    u_int8_t    len;
    u_int8_t    bssid[6];
#if _BYTE_ORDER == _BIG_ENDIAN
    u_int8_t    bsinfo0_rrm:1;          /* BSINFO Byte0 Radio measurement B7 */
    u_int8_t    bsinfo0_apsd:1;         /* BSINFO Byte0 APSD B6 */
    u_int8_t    bsinfo0_qos:1;          /* BSINFO Byte0 qos B5 */
    u_int8_t    bsinfo0_specmgmt:1;     /* BSINFO Byte0 spectral mgmt B4 */
    u_int8_t    bsinfo0_keyscope:1;     /* BSINFO Byte0 Key scope B3 */
    u_int8_t    bsinfo0_security:1;     /* BSINFO Byte0 Security B2 */
    u_int8_t    bsinfo0_ap_reach:2;     /* BSINFO Byte0 AP reachability B0-1 */

    u_int8_t    bsinfo1_he_er:1;        /* BSINFO Byte1 HE ER BSS B7 */
    u_int8_t    bsinfo1_he:1;           /* BSINFO Byte1 HE B6 */
    u_int8_t    bsinfo1_ftm:1;          /* BSINFO Byte1 FTM B5 */
    u_int8_t    bsinfo1_vht:1;          /* BSINFO Byte1 VHT Throughput B4 */
    u_int8_t    bsinfo1_ht:1;           /* BSINFO Byte1 HT Throughput B3 */
    u_int8_t    bsinfo1_mdomain:1;      /* BSSINFO Byte1 Mobility Domain B2 */
    u_int8_t    bsinfo1_iba:1;          /* BSINFO Byte1 Immediate BA B1 */
    u_int8_t    bsinfo1_dba:1;          /* BSINFO Byte1 Delayed BA B0 */

    u_int8_t    bsinfo2_reserved:6;     /* BSINFO Byte2 Reserved B2-7 */
    u_int8_t    bsinfo2_colocated_ap:1; /* BSINFO Byte2 Co-Located AP B1 */
    u_int8_t    bsinfo2_20tuprobresp:1; /* BSINFO Byte2 20TU Probe Response B0 */
#else
    u_int8_t    bsinfo0_ap_reach:2;     /* BSINFO Byte0 AP reachability B0-1 */
    u_int8_t    bsinfo0_security:1;     /* BSINFO Byte0 Security B2 */
    u_int8_t    bsinfo0_keyscope:1;     /* BSINFO Byte0 Key scope B3 */
    u_int8_t    bsinfo0_specmgmt:1;     /* BSINFO Byte0 spectral mgmt B4 */
    u_int8_t    bsinfo0_qos:1;          /* BSINFO Byte0 qos B5 */
    u_int8_t    bsinfo0_apsd:1;         /* BSINFO Byte0 APSD B6 */
    u_int8_t    bsinfo0_rrm:1;          /* BSINFO Byte0 Radio measurement B7 */

    u_int8_t    bsinfo1_dba:1;          /* BSINFO Byte1 Delayed BA B0 */
    u_int8_t    bsinfo1_iba:1;          /* BSINFO Byte1 Immediate BA B1 */
    u_int8_t    bsinfo1_mdomain:1;      /* BSSINFO Byte1 Mobility Domain B2 */
    u_int8_t    bsinfo1_ht:1;           /* BSINFO Byte1 HT Throughput B3 */
    u_int8_t    bsinfo1_vht:1;          /* BSINFO Byte1 VHT Throughput B4 */
    u_int8_t    bsinfo1_ftm:1;          /* BSINFO Byte1 FTM B5 */
    u_int8_t    bsinfo1_he:1;           /* BSINFO Byte1 HE B6 */
    u_int8_t    bsinfo1_he_er:1;        /* BSINFO Byte1 HE ER BSS B7 */

    u_int8_t    bsinfo2_20tuprobresp:1; /* BSINFO Byte2 20TU Probe Response B0 */
    u_int8_t    bsinfo2_colocated_ap:1; /* BSINFO Byte2 Co-Located AP B1 */
    u_int8_t    bsinfo2_reserved:6;     /* BSINFO Byte2 Reserved B2-7 */
#endif
    u_int8_t    bsinfo3_reserved;
    u_int8_t    regclass;
    u_int8_t    channum;
    u_int8_t    phytype;
    u_int8_t    subelm[1];     /* varialbe len optional sub element */
} __packed;

struct ieee80211_nr_preference_subie {
    u_int8_t    id;         /* IEEE80211_SUBELEMID_NEIGHBORREPORT_PREFERENCE */
    u_int8_t    len;        /* Fixed length: 1 */
    u_int8_t    preference;
} __packed;

/* Wide Bandwidth Channel sub-element */
struct ieee80211_nr_wbc_subie {
    u_int8_t    id;         /* IEEE80211_SUBELEMID_NEIGHBORREPORT_WBC */
    u_int8_t    len;        /* Fixed length: 1 */
    u_int8_t    chwidth;    /* Channel Width: 1 */
    u_int8_t    cf_s1;      /* Center Channel Seg1: 1 */
    u_int8_t    cf_s2;      /* Center Channel Seg2: 1 */
} __packed;

struct ieee80211_nr_meas_subie {
    u_int8_t    id;         /* IEEE80211_ELEMID_MEASREQ  */
    u_int8_t    len;
    u_int8_t    subelm[1]; /* Followed by LCI or LOC civic info */
} __packed;

struct ieee80211_nr_lci_subie {
    u_int8_t    id;         /*  */
    u_int8_t    len;
    u_int8_t    lci[16]; /* LCI payload */
    u_int8_t    z_id;
    u_int8_t    z_len;
    u_int16_t   z_floor_info;
#if _BYTE_ORDER == _BIG_ENDIAN
    u_int32_t z_uncertainty_height_above_floor:8;  /* Bits 24-31 */
    u_int32_t z_height_above_floor:24;             /* Bits 0-23 */
#else
    u_int32_t z_height_above_floor:24;             /* Bits 0-23 */
    u_int32_t z_uncertainty_height_above_floor:8;  /* Bits 24-31 */
#endif
    u_int8_t    usage_rule_id;
    u_int8_t    usage_rule_len;
    u_int8_t    usage_param;
    //u_int16_t   retention_expires_relative;
} __packed;

struct ieee80211_nr_lcr_subie {
    u_int8_t    civic_loc_type;
    u_int8_t    id;         /*  */
    u_int8_t    len;
    u_int32_t   lcr[64]; /* ?? LCR payload */
}__packed;

struct mbo_user_bssidpref {
    u_int8_t pref_val;     /* Instead of adding pref_val nr ie we add it separately for compatibility with legacy structure*/
    u_int8_t opt_ie[1];
}__packed;

/*  RRM capability information element */
struct ieee80211_rrm_cap_ie {
    u_int8_t    id;         /* IEEE80211_ELEMID_RRM */
    u_int8_t    len;
#if _BYTE_ORDER == _BIG_ENDIAN  /* Byte-1 */
    u_int8_t    bcn_meas_rpt:1;
    u_int8_t    bcn_table:1;
    u_int8_t    bcn_active:1;
    u_int8_t    bcn_passive:1;
    u_int8_t    rep_meas:1;
    u_int8_t    para_meas:1;
    u_int8_t    neig_rpt:1;
    u_int8_t    lnk_meas:1;
#else
    u_int8_t    lnk_meas:1;
    u_int8_t    neig_rpt:1;
    u_int8_t    para_meas:1;
    u_int8_t    rep_meas:1;
    u_int8_t    bcn_passive:1;
    u_int8_t    bcn_active:1;
    u_int8_t    bcn_table:1;
    u_int8_t    bcn_meas_rpt:1;
#endif
#if _BYTE_ORDER == _BIG_ENDIAN  /* Byte-2 */
    u_int8_t    trig_tsm_meas:1;
    u_int8_t    tsm_meas:1;
    u_int8_t    lci_azimuth:1;
    u_int8_t    lci_meas:1;
    u_int8_t    stat_meas:1;
    u_int8_t    noise_meas:1;
    u_int8_t    chan_load_meas:1;
    u_int8_t    frm_meas:1;
#else
    u_int8_t    frm_meas:1;
    u_int8_t    chan_load_meas:1;
    u_int8_t    noise_meas:1;
    u_int8_t    stat_meas:1;
    u_int8_t    lci_meas:1;
    u_int8_t    lci_azimuth:1;
    u_int8_t    tsm_meas:1;
    u_int8_t    trig_tsm_meas:1;
#endif
#if _BYTE_ORDER == _BIG_ENDIAN  /* Byte-3 */
    u_int8_t    non_op_chan_max:3;
    u_int8_t    op_chan_max:3;
    u_int8_t    rrm_mib:1;
    u_int8_t    ap_chan_rpt:1;
#else
    u_int8_t    ap_chan_rpt:1;
    u_int8_t    rrm_mib:1;
    u_int8_t    op_chan_max:3;
    u_int8_t    non_op_chan_max:3;
#endif
#if _BYTE_ORDER == _BIG_ENDIAN  /* Byte-4 */
    u_int8_t    bss_avg_acc:1;
    u_int8_t    rsni_meas:1;
    u_int8_t    rcpi_meas:1;
    u_int8_t    neig_rpt_tsf:1;
    u_int8_t    meas_pilot_tsinfo:1;
    u_int8_t    meas_pilot:3;
#else
    u_int8_t    meas_pilot:3;
    u_int8_t    meas_pilot_tsinfo:1;
    u_int8_t    neig_rpt_tsf:1;
    u_int8_t    rcpi_meas:1;
    u_int8_t    rsni_meas:1;
    u_int8_t    bss_avg_acc:1;
#endif
#if _BYTE_ORDER == _BIG_ENDIAN  /* Byte-5 */
    u_int8_t    reserved:4;
    u_int8_t    civ_loc_meas:1;
    u_int8_t    ftm_range_report:1;
    u_int8_t    ant_info:1;
    u_int8_t    bss_adms_cap:1;
#else
    u_int8_t    bss_adms_cap:1;
    u_int8_t    ant_info:1;
    u_int8_t    ftm_range_report:1;
    u_int8_t    civ_loc_meas:1;
    u_int8_t    reserved:4;
#endif
} __packed;

/* radio measurement request struct */
struct ieee80211_action_rm_req {
    struct ieee80211_action             header;
    u_int8_t                            dialogtoken;
    u_int16_t                           num_rpts;
    u_int8_t                            req_ies[1];
} __packed;

struct ieee80211_action_rm_rsp {
    struct ieee80211_action             header;
    u_int8_t                            dialogtoken;
    u_int8_t                            rsp_ies[1];
} __packed;

/* neighbor report request struct */
struct ieee80211_action_nr_req {
    struct ieee80211_action             header;
    u_int8_t                            dialogtoken;
    u_int8_t                            req_ies[1];
} __packed;

/* neighbor report response struct */
struct ieee80211_action_nr_resp {
    struct ieee80211_action             header;
    u_int8_t                            dialogtoken;
    u_int8_t                            resp_ies[1];
} __packed;

/* link measurement request struct */
struct ieee80211_action_lm_req {
    struct ieee80211_action             header;
    u_int8_t                            dialogtoken;
    u_int8_t                            txpwr_used;
    u_int8_t                            max_txpwr;
    u_int8_t                            opt_ies[1];
} __packed;

struct ieee80211_rrm_tpc_ie
{
    u_int8_t id;
    u_int8_t len;
    u_int8_t tx_pow;
    u_int8_t lmargin;
}__packed;

/* link measurement response */
struct ieee80211_action_lm_rsp {
    struct ieee80211_action             header;
    u_int8_t                            dialogtoken;
    struct ieee80211_rrm_tpc_ie         tpc;
    u_int8_t                            rxant;
    u_int8_t                            txant;
    u_int8_t                            rcpi;
    u_int8_t                            rsni;
    u_int8_t                            rsp[1];
} __packed;

struct ieee80211_frame_req{
    u_int8_t regclass;
    u_int8_t chnum;
    u_int16_t rintvl;
    u_int16_t mduration;
    u_int8_t ftype;
    u_int8_t mac[6];
    u_int8_t req[1];
}__packed;

struct ieee80211_lcireq{
		u_int8_t location;
		u_int8_t lat_res;
		u_int8_t long_res;
		u_int8_t alt_res;
		u_int8_t req[1];
}__packed;

struct ieee80211_ccareq{
    u_int8_t chnum;
    u_int64_t tsf;
    u_int16_t mduration;
    u_int8_t req[1];
}__packed;

struct ieee80211_rpihistreq{
    u_int8_t chnum;
    u_int64_t tsf;
    u_int16_t mduration;
    u_int8_t req[1];
}__packed;

struct ieee80211_nhistreq{
    u_int8_t regclass;
    u_int8_t chnum;
    u_int16_t rintvl;
    u_int16_t mduration;
    u_int8_t req[1];
}__packed;

struct ieee80211_chloadreq{
    u_int8_t regclass;
    u_int8_t chnum;
    u_int16_t rintvl;
    u_int16_t mduration;
    u_int8_t req[1];
}__packed;

struct ieee80211_stastatsreq{
    u_int8_t dstmac[6];
    u_int16_t rintvl;
    u_int16_t mduration;
    u_int8_t gid;
    u_int8_t req[1];
}__packed;

struct ieee80211_tsm_rsp {
    u_int8_t mstime[8];
    u_int16_t m_intvl;
    u_int8_t mac[6];
    u_int8_t tid;
    u_int32_t tx_cnt;
    u_int32_t discnt;
    u_int32_t multirtycnt;
    u_int32_t cfpoll;
    u_int32_t qdelay;
    u_int32_t txdelay;
    u_int8_t brange;
    u_int32_t bin[5];
    u_int8_t rsp[1];
}__packed;

struct ieee80211_frmcnt
{
    u_int8_t ta[6];
    u_int8_t bssid[6];
    u_int8_t phytype;
    u_int8_t arcpi;
    u_int8_t lrsni;
    u_int8_t lrcpi;
    u_int8_t antid;
    u_int16_t frmcnt;
}__packed;

/**
 * @brief 
 */
struct ieee80211_frm_rsp {
    u_int8_t regclass;
    u_int8_t chnum;
    u_int8_t tsf[8];
    u_int16_t mduration;
    u_int8_t rsp[1];
}__packed; 

struct ieee80211_stastatsrsp {
    u_int16_t m_intvl;
    u_int8_t gid;
    union {
        ieee80211_rrm_statsgid0_t gid0;
        ieee80211_rrm_statsgid1_t gid1;
        ieee80211_rrm_statsgidupx_t upstats[8]; /* from 0 to seven */
        ieee80211_rrm_statsgid10_t gid10;
    }stats;
    u_int8_t rsp[1];
}__packed;

struct ieee80211_ccarsp
{
    u_int8_t chnum;
    u_int64_t tsf;
    u_int16_t mduration;
    u_int8_t cca_busy;
    u_int8_t rsp[1];
}__packed;

struct ieee80211_rpihistrsp
{
    u_int8_t chnum;
    u_int64_t tsf;
    u_int16_t mduration;
    u_int8_t rpi[IEEE80211_RRM_RPI_SIZE];
    u_int8_t rsp[1];
}__packed;

struct ieee80211_nhistrsp
{
    u_int8_t regclass;
    u_int8_t chnum;
    u_int8_t tsf[8];
    u_int16_t mduration;
    u_int8_t antid;
    u_int8_t anpi;
    u_int8_t ipi[11];
    u_int8_t rsp[1];
}__packed;

struct ieee80211_chloadrsp{
    u_int8_t regclass;
    u_int8_t chnum;
    u_int8_t tsf[8];
    u_int16_t mduration;
    u_int8_t chload;
    u_int8_t rsp[1];
}__packed;

struct ieee80211_lcirsp {
    u_int8_t lci_data[18];
    u_int8_t rsp[1];
}__packed;

struct ieee80211_measrsp_ie {
    u_int8_t    id;         /* IEEE80211_ELEMID_MEASRSP */
    u_int8_t    len;
    u_int8_t    token;
    u_int8_t    rspmode;
    u_int8_t    rsptype;
    u_int8_t    rsp[1];     /* varialbe len measurement requet */
}__packed;

#define BIT_PARALLEL    0x01
#define BIT_ENABLE      0x02
#define BIT_REQUEST     0x04
#define BIT_REPORT      0x08
#define BIT_DUR         0x10

u_int8_t *ieee80211_add_measreq_beacon_ie(u_int8_t *frm, struct ieee80211_node *ni,
                                          ieee80211_rrm_beaconreq_info_t *binfo, u_int8_t *wbufhead);

u_int8_t *ieee80211_add_measreq_tsm_ie(u_int8_t *frm, 
                       ieee80211_rrm_tsmreq_info_t *tsm_info, struct ieee80211_node *ni);

int ieee80211_recv_neighbor_req(wlan_if_t vap, wlan_node_t ni, 
                                struct ieee80211_action_nr_req *nr_req, int frm_len);

int ieee80211_send_neighbor_resp(wlan_if_t vap, wlan_node_t ni,
                                         ieee80211_rrm_nrreq_info_t *nrreq_info);

int ieee80211_recv_radio_measurement_req(wlan_if_t vap, wlan_node_t ni,
                struct ieee80211_action_rm_req *frm , int  frm_len);

int ieee80211_recv_radio_measurement_rsp(wlan_if_t vap, wlan_node_t ni,
        u_int8_t *frm, u_int32_t frm_len);
int ieee80211_recv_lm_rsp(wlan_if_t vap, wlan_node_t ni,
        u_int8_t *frm, u_int32_t frm_len);
int ieee80211_recv_link_measurement_req(wlan_if_t vap, wlan_node_t ni,
                struct ieee80211_action_nr_req *frm, u_int8_t frm_len);
int ieee80211_recv_link_measurement_rsp(wlan_if_t vap,wlan_node_t  ni,
                struct ieee80211_action_nr_req *frm, u_int8_t frm_len);
int ieee80211_recv_neighbor_req(wlan_if_t vap, wlan_node_t ni,
               struct ieee80211_action_nr_req *frm,int frm_len);
int ieee80211_recv_neighbor_rsp(wlan_if_t vap, wlan_node_t ni,
                struct ieee80211_action_nr_req *frm,int frm_len);
int ieee80211_radio_measurement_rsp(wlan_if_t vap, wlan_node_t ni,
                struct ieee80211_measreq_ie *measrsp, int frm_len);

int ieee80211_rrm_beacon_measurement_report(wlan_if_t vap,
        u_int8_t *frm,u_int32_t frm_len);

int ieee80211_rrm_nhist_report(wlan_if_t vap, u_int8_t *,u_int32_t frm_len);

int ieee80211_rrm_cca_report(wlan_if_t vap, u_int8_t *,u_int32_t frm_len);

int ieee80211_rrm_rpihist_report(wlan_if_t vap, u_int8_t *,u_int32_t frm_len);

int ieee80211_rrm_stastats_report(wlan_if_t vap, u_int8_t *,u_int32_t frm_len);

int ieee80211_rrm_tsm_report(wlan_if_t vap, u_int8_t *,u_int32_t frm_len);

int ieee80211_rrm_frm_report(wlan_if_t vap, u_int8_t *,u_int32_t frm_len);

int ieee80211_rrm_chload_report(wlan_if_t vap, u_int8_t *,u_int32_t frm_len);

int ieee80211_rrm_lci_report(wlan_if_t vap, u_int8_t *,u_int32_t frm_len);

int ieee80211_rrm_recv_stastats_req(wlan_if_t vap, wlan_node_t ni, 
                             struct ieee80211_measreq_ie *req,u_int8_t rm_token);
int ieee80211_rrm_recv_chload_req(wlan_if_t vap, wlan_node_t ni, 
                              struct ieee80211_measreq_ie *req,u_int8_t rm_token);
int ieee80211_rrm_recv_nhist_req(wlan_if_t vap, wlan_node_t ni, 
                              struct ieee80211_measreq_ie *req,u_int8_t rm_token);
int ieee80211_rrm_recv_beacon_req(wlan_if_t vap, wlan_node_t ni, 
                              struct ieee80211_measreq_ie *req,u_int8_t rm_token);
u_int8_t *ieee80211_rrm_send_bcnreq_resp(ieee80211_rrm_t rrm,u_int8_t *frm);
u_int8_t *ieee80211_rrm_send_stastats_resp(ieee80211_rrm_t rrm,u_int8_t *frm);
u_int8_t *ieee80211_rrm_send_chload_resp(ieee80211_rrm_t rrm,u_int8_t *frm);

u_int8_t *ieee80211_add_rrm_action_ie(u_int8_t *frm,u_int8_t action, u_int16_t n_rpt,
        struct ieee80211_node *ni);

u_int8_t *ieee80211_add_measreq_lci_ie(u_int8_t *frm, struct ieee80211_node *ni, 
                       ieee80211_rrm_lcireq_info_t *lcireq_info);
int ieee80211_rrm_scan_start(wlan_if_t vap, bool active_scan);
int ieee80211_rrm_set_report_pending(wlan_if_t vap,u_int8_t type, void *cbinfo);
int ieee80211_rrm_free_report(ieee80211_rrm_t);
void ieee80211_send_report(ieee80211_rrm_t rrm);
int  ieee80211_set_nr_share_radio_flag(wlan_if_t vap, int value);

u_int8_t *ieee80211_add_measreq_chload_ie(u_int8_t *frm, struct ieee80211_node *ni,
                       ieee80211_rrm_chloadreq_info_t*chinfo);

u_int8_t *ieee80211_add_measreq_nhist_ie(u_int8_t *frm, struct ieee80211_node *ni,
                       ieee80211_rrm_nhist_info_t *nhist);
u_int8_t *ieee80211_add_measreq_cca_ie(u_int8_t *frm, struct ieee80211_node *ni,
                       ieee80211_rrm_cca_info_t *cca);

u_int8_t *ieee80211_add_measreq_rpihist_ie(u_int8_t *frm, struct ieee80211_node *ni,
                       ieee80211_rrm_rpihist_info_t *rpihist);

u_int8_t *ieee80211_add_measreq_frame_req_ie(u_int8_t *frm, struct ieee80211_node *ni, 
                       ieee80211_rrm_frame_req_info_t  *fr_info);

u_int8_t *ieee80211_add_measreq_stastats_ie(u_int8_t *frm, struct ieee80211_node *ni, 
                       ieee80211_rrm_stastats_info_t *stastats);

int rrm_attach_misc(ieee80211_rrm_t rrm);

int rrm_detach_misc(ieee80211_rrm_t rrm);



void  set_vo_window(ieee80211_rrm_t rrm,u_int8_t new_vo);

void  set_stcnt_window(ieee80211_rrm_t rrm,u_int8_t new_stcnt);

void  set_be_window(ieee80211_rrm_t rrm,u_int8_t new_be);

void  set_anpi_window(ieee80211_rrm_t rrm,int8_t new_anpi);

void  set_per_window(ieee80211_rrm_t rrm);

void  set_frmcnt_window(ieee80211_rrm_t rrm,u_int32_t new_frmcnt);

void  set_ackfail_window(ieee80211_rrm_t rrm,u_int32_t new_ackfail);

void  set_chload_window(ieee80211_rrm_t rrm,u_int8_t new_chload);

#else

#define ieee80211_rrm_set_rrmstats(vap_handle,val)

#define ieee80211_rrm_set_slwindow(vap_handle,val)

#define ieee80211_rrm_get_rrmstats(vap_handle) 

#define ieee80211_rrm_get_slwindow(vap_handle)

#endif /* UMAC_SUPPORT_RRM */

#endif /* _IEEE80211_RRM__PRIV_H_ */

