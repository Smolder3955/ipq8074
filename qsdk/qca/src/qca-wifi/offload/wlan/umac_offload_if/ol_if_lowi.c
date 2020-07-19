/*
 * Copyright (c) 2016-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include "ol_if_athvar.h"
#include <ieee80211_defines.h>
#include <init_deinit_lmac.h>
#include <wlan_utility.h>

#if ATH_SUPPORT_LOWI
#include <ieee80211_api.h>

#define RTT_LCI_ALTITUDE_MASK           0x3FFFFFFF
#define LOWI_MESSAGE_LCR_CIVIC_INFO_POSITION      12
#define LOWI_MESSAGE_LOC_CIVIC_PARAMS_POSITION    8
#define IEEE80211_COLOCATED_BSS_MAX_LEN           196
#define IEEE80211_SUBIE_COLOCATED_BSSID           0x7
#define IEEE80211_MAXBSSID_INDICATOR_DEFAULT      0
#define ANI_MSG_OEM_DATA_RSP            0x04
#define TARGET_OEM_CAPABILITY_RSP       0x02
#define TARGET_OEM_MEASUREMENT_RSP      0x04
#define TARGET_OEM_ERROR_REPORT_RSP     0x05
#define TARGET_OEM_CONFIGURE_LCR        0x09
#define TARGET_OEM_CONFIGURE_LCI        0x0A

/* For storing AP's LCI and LCR */
extern u_int32_t ap_lcr[RTT_LOC_CIVIC_REPORT_LEN];
extern int num_ap_lcr;
extern u_int32_t ap_lci[RTT_LOC_CIVIC_INFO_LEN];
extern int num_ap_lci;

/* function to send FW response to LOWI */
extern void ath_lowi_if_nlsend_response(struct ieee80211com *ic, u_int8_t *data, u_int16_t datalen, u_int8_t msgtype, u_int32_t msg_subtype, u_int8_t error_code);

static void ol_ath_cache_lci(void *req, struct lci_set_params *plci_param);
static void ol_ath_cache_lcr(void *req);
static void ieee80211_vap_iter_for_colocated_bss(void *arg, wlan_if_t vap);
int ol_ath_lowi_data_req_to_fw(struct ieee80211com *ic, int msg_len, void *req, int msgsubType);

/* Capability response event handler */
static int
wmi_lowi_oem_cap_resp_event_handler(ol_scn_t sc, u_int8_t *data,
                    u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct ieee80211com *ic;
    struct wlan_objmgr_pdev *pdev;

    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0, WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_print("%s: pdev object (id: 0) is NULL ", __func__);
         return -1;
    }

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
         qdf_err("ic is NULL");
         return -1;
    }

    ath_lowi_if_nlsend_response(ic, data, datalen, ANI_MSG_OEM_DATA_RSP, TARGET_OEM_CAPABILITY_RSP, 0);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    return 0;
}

/* Measurement report event handler */
static int
wmi_lowi_oem_meas_report_event_handler(ol_scn_t sc, u_int8_t *data,
                    u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct ieee80211com *ic;
    struct wlan_objmgr_pdev *pdev;

    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0, WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_print("%s: pdev object (id: 0) is NULL", __func__);
         return -1;
    }

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
         qdf_err("ic is NULL");
         return -1;
    }

    ath_lowi_if_nlsend_response(ic, data, datalen, ANI_MSG_OEM_DATA_RSP, TARGET_OEM_MEASUREMENT_RSP, 0);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    return 0;
}

/* Error report event handler */
static int
wmi_lowi_oem_err_report_event_handler(ol_scn_t sc, u_int8_t *data,
                    u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct ieee80211com *ic;
    wmi_host_rtt_error_report_event ev;
    struct common_wmi_handle *wmi_handle;
    struct wlan_objmgr_pdev *pdev;

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
    /* Extract error report for LOWI */
    if(wmi_extract_rtt_error_report_ev(wmi_handle, data, &ev)) {
	    qdf_print("%s:\n Unable to extract RTT header ", __func__);
	    return -1;
    }
    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0, WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_print("%s: pdev object (id: 0) is NULL", __func__);
         return -1;
    }

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (!ic) {
         qdf_err("ic is NULL");
         return -1;
    }

    ath_lowi_if_nlsend_response(ic, data, datalen, ANI_MSG_OEM_DATA_RSP, TARGET_OEM_ERROR_REPORT_RSP,
				ev.reject_reason);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    return 0;
}

/*
 * RTT measurement response handler attach functions for offload solutions
 */
void
ol_ath_lowi_wmi_event_attach(ol_ath_soc_softc_t *soc)
{
    wmi_unified_t wmi_handle;

    wmi_handle = lmac_get_wmi_unified_hdl(soc->psoc_obj);

    /* Register WMI event handlers */
    wmi_unified_register_event_handler(wmi_handle,
        wmi_oem_cap_event_id,
        wmi_lowi_oem_cap_resp_event_handler,
        WMI_RX_UMAC_CTX);

    wmi_unified_register_event_handler(wmi_handle,
	wmi_oem_meas_report_event_id,
        wmi_lowi_oem_meas_report_event_handler,
        WMI_RX_UMAC_CTX);

    wmi_unified_register_event_handler(wmi_handle,
        wmi_oem_report_event_id,
        wmi_lowi_oem_err_report_event_handler,
        WMI_RX_UMAC_CTX);

    return;

}

/** Function to send ANI_MSG_OEM_DATA_REQ received from LOWI to FW **/
int ol_ath_lowi_data_req_to_fw(struct ieee80211com *ic,
                                  int msg_len, void *req, int msgsubType)
{
    u_int8_t colocated_bss[IEEE80211_COLOCATED_BSS_MAX_LEN]={0};
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct lci_set_params lci_param;
    struct lcr_set_params lcr_param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&lci_param, sizeof(lci_param), 0);
    qdf_mem_set(&lcr_param, sizeof(lcr_param), 0);

    /* If msgsubType is LCI or LCR, cache it for driver */
    if (msgsubType == TARGET_OEM_CONFIGURE_LCR) {
        ol_ath_cache_lcr(req);
	lcr_param.msg_len = msg_len;
	lcr_param.lcr_data = req;
	if (wmi_unified_lcr_set_cmd_send(pdev_wmi_handle, &lcr_param)) {
        	qdf_print("%s: ERROR: Host unable to send LCR LOWI request to FW ", __func__);
        	return -1;
    	}
	return 0;
    }

    if (msgsubType == TARGET_OEM_CONFIGURE_LCI) {
	/* Get colocated bss and populate colocated_bssid_info field in wmi_rtt_lci_cfg_head */
	wlan_iterate_vap_list(&scn->sc_ic, ieee80211_vap_iter_for_colocated_bss,(void *) &colocated_bss);
	lci_param.lci_data = req;
	lci_param.colocated_bss = colocated_bss;
	lci_param.msg_len = msg_len;
	if (wmi_unified_lci_set_cmd_send(pdev_wmi_handle, &lci_param)) {
        	qdf_print("%s: ERROR: Host unable to send LCI LOWI request to FW ", __func__);
        	return -1;
    	}
        ol_ath_cache_lci(req+4, &lci_param); //move msgsubType (4 octets)
	return 0;
    }

    /* As per rtt_interface.h, we have to just send req to FW, msgSubType is already there */
    /* Control enters here only if subtype is neither of LCI or LCR */

    if (wmi_unified_start_oem_data_cmd(pdev_wmi_handle, msg_len, req)) {
	    qdf_print("%s: ERROR: Host unable to send OEM request to FW ", __func__);
	    return -1;
    }

    return 0;
}

/* Caches LCR (Civic Location) data in HOST for later use */
static void ol_ath_cache_lcr(void *req)
{
    char *lcr_data = (char *) req;
    int  loc_civic_len;
    /* Save LCR data in host buffer */
    /* LOWI LCR msg: msgsubType(4), req_id(4), loc_civic_params(4),civic_info(256) */
    lcr_data = lcr_data + LOWI_MESSAGE_LCR_CIVIC_INFO_POSITION;
    /* Get LCR length to copy, first 8 bits of loc_civic_params are length as per rtt_interface.c */
    loc_civic_len = (*(int* )(req + LOWI_MESSAGE_LOC_CIVIC_PARAMS_POSITION)) & 0xff;
    OL_IF_MSG_COPY_CHAR_ARRAY(ap_lcr, lcr_data, loc_civic_len); /* Max size for LCR is 256 */
    num_ap_lcr = 1;
    qdf_print("Copied LCR data to driver cache (%d octets)", loc_civic_len);
}

struct ieee80211_lci_subelement_info{
    A_UINT8     latitude_unc:6,
                latitude_0_1:2;     //bits 0 to 1 of latitude
    A_UINT32    latitude_2_33;     //bits 2 to 33 of latitude
    A_UINT8     longitude_unc:6,
                longitude_0_1:2;    //bits 0 to 1 of longitude
    A_UINT32    longitude_2_33;    //bits 2 to 33 of longitude
    A_UINT8     altitude_type:4,
                altitude_unc_0_3:4;
    A_UINT32    altitude_unc_4_5:2,
                altitude:30;
    A_UINT8     datum:3,
                reg_loc_agmt:1,
                reg_loc_dse:1,
                dep_sta:1,
                version:2;
} __ATTRIB_PACK;

/* Caches LCI (Location Civic Information) in HOST buffer */
static void ol_ath_cache_lci(void *req, struct lci_set_params *plci_param)
{
    struct ieee80211_lci_subelement_info lci_sub;

    /* Save LCI data in host buffer */
    {
        lci_sub.latitude_unc = plci_param->latitude_unc;
        lci_sub.latitude_0_1 = plci_param->latitude_0_1;
        lci_sub.latitude_2_33 = plci_param->latitude_2_33;
        lci_sub.longitude_unc = plci_param->longitude_unc;
        lci_sub.longitude_0_1 = plci_param->longitude_0_1;
        lci_sub.longitude_2_33 = plci_param->longitude_2_33;
        lci_sub.altitude_type = plci_param->altitude_type;
        lci_sub.altitude_unc_0_3 = plci_param->altitude_unc_0_3;
        lci_sub.altitude_unc_4_5 = plci_param->altitude_unc_4_5;
        lci_sub.altitude = plci_param->altitude;
        lci_sub.datum = plci_param->datum;
        lci_sub.reg_loc_agmt = plci_param->reg_loc_agmt;
        lci_sub.reg_loc_dse = plci_param->reg_loc_dse;
        lci_sub.dep_sta = plci_param->dep_sta;
        lci_sub.version = plci_param->version;

        /* Max size of LCI is 16 octets */
        OL_IF_MSG_COPY_CHAR_ARRAY(ap_lci, &lci_sub, sizeof(struct ieee80211_lci_subelement_info));
        num_ap_lci = 1;
        qdf_print("Copied LCI data to driver cache");
    }
}

/* Function called by VAP iterator that adds active vaps as colocated bssids */
static void ieee80211_vap_iter_for_colocated_bss(void *arg, wlan_if_t vap)
{
    /* This function is called iteratively for each active VAP and populate params array */
    /* params[]: subelementId (1octet), num_vaps(1 octet), maxbssidInd (1octect), BSSIDs...(each 6 octets) */
    u_int8_t *params, num_vaps;
    params = (u_int8_t *) arg;
    params[0]=IEEE80211_SUBIE_COLOCATED_BSSID; //Colocated BSSID Subelement ID
    params[2]=IEEE80211_MAXBSSID_INDICATOR_DEFAULT; //MaxBSSID Indicator: Default: 0
    /* params[1] contains num_vaps added so far */
    num_vaps = params[1];

    /* If an active vap, add it to correct location in params */
    if (wlan_vdev_is_up(vap->vdev_obj) == QDF_STATUS_SUCCESS) {
        /* Position to store this vap: First 3 octets + 6*(Num of Vaps already added) */
        /* Position is always w.r.t params as there could already be non-zero num_vaps(bssids) */
        /* stored in the params array previously or coming into the function */
        memcpy(params+((num_vaps*IEEE80211_ADDR_LEN)+3), vap->iv_myaddr, IEEE80211_ADDR_LEN);
        params[1]++;
    }
}

#endif /* ATH_SUPPORT_LOWI */
