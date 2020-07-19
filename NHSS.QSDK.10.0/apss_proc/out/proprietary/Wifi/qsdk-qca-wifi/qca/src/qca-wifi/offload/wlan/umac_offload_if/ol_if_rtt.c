/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 * Notifications and licenses are retained for attribution purposes only
 *
 * Copyright (c) 2011, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "ol_if_athvar.h"
#include <init_deinit_lmac.h>
#include "qdf_mem.h"
#include <ieee80211_defines.h>
//#define ATH_SUPPORT_WIFIPOS 1
#if ATH_SUPPORT_WIFIPOS
#include <ieee80211_wifipos_pvt.h>
#include <ieee80211_api.h>


//tone number of each bw
//bw = 0, Legacy 20MHz, 53 tones
//bw = 1, HT 20MHz, 57 tones
//bw = 2, HT 40MHz, 117 tones
//bw = 3, VHT 80MHz, 242 tones
#define IS_2GHZ_CH 20
#define RTT3_FRAME_TYPE 3
// [FIXME_MR] Replace 'struct ol_ath_softc_net80211' with 'struct ol_ath_soc_softc' when the two structs get separated
extern ol_ath_soc_softc_t *ol_global_soc[GLOBAL_SOC_SIZE];
extern int ol_num_global_soc;
static atomic_t rtt_nl_users = ATOMIC_INIT(0);
#define RTT_TIMEOUT_MS 180
#define MEM_ALIGN(x) (((x)<<1)+3) & 0xFFFC

#define NUM_ARRAY_ELEMENTS(array_name) ((size_t)(sizeof(array_name) / sizeof(array_name[0])))
#define IS_VALID_INDEX_OF_ARRAY(array_name, array_index) (array_index < NUM_ARRAY_ELEMENTS(array_name))

#define TARGET_OEM_CONFIGURE_LCI        0x0A
#define TARGET_OEM_CONFIGURE_LCR        0x09
#define RTT_LCI_ALTITUDE_MASK		0x3FFFFFFF
#define RTT3_DEFAULT_BURST_DURATION     15
#define IEEE80211_COLOCATED_BSS_MAX_LEN 196


char* measurement_type[] = {
    "NULL Frame",
    "QoS_NULL Frame",
    "TMR/TM Frame"
};

/*
 * Define the sting for each RTT request type
 * for debug print purpose only
 */
char* error_indicator[] = {
    "RTT_COMMAND_HEADER_ERROR",
    "RTT_COMMAND_ERROR",
    "RTT_MODULE_BUSY",
    "RTT_TOO_MANY_STA",
    "RTT_NO_RESOURCE",
    "RTT_VDEV_ERROR",
    "RTT_TRANSIMISSION_ERROR",
    "RTT_TM_TIMER_EXPIRE",
    "RTT_FRAME_TYPE_NOSUPPORT",
    "RTT_TIMER_EXPIRE",
    "RTT_CHAN_SWITCH_ERROR",
    "RTT_TMR_TRANS_ERROR", //TMR trans error, this dest peer will be skipped
    "RTT_NO_REPORT_BAD_CFR_TOKEN", //V3 only. If both CFR and Token mismatch, do not report
    "RTT_NO_REPORT_FIRST_TM_BAD_CFR", //For First TM, if CFR is bad, then do not report
    "RTT_REPORT_TYPE2_MIX", //do not allow report type2 mix with type 0, 1
    "WMI_RTT_REJECT_MAX"
};

#define NUM_ARRAY_ELEMENTS(array_name) ((size_t)(sizeof(array_name) / sizeof(array_name[0])))
#define IS_VALID_INDEX_OF_ARRAY(array_name, array_index) (array_index < NUM_ARRAY_ELEMENTS(array_name))


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


/*
 * print out the rtt measurement response on host
 * for debug purpose
 */
void rtt_print_resp_header(wmi_host_rtt_event_hdr *header,
                           ieee80211_ol_wifiposdesc_t * wifiposdesc)
{
    char* measurement_type_str = NULL;
    uint8_t *dest_mac;

    wifiposdesc->request_id = header->req_id;
    wifiposdesc->rx_pkt_type = header->meas_type;
    measurement_type_str = ((header->meas_type < 3) ?
                          measurement_type[header->meas_type] : "INVALID");
    dest_mac = header->dest_mac;
    /* Only 3 type of frames are used for Measurement type ,
        even though  RTT measurement type is 3 bit header */
    qdf_print("\nRTTREPORT ==================Measurement Report====================");
    qdf_print("RTTREPORT Request ID=%u status=0x%x MeasurementType=%s ReportType=%d \n\
            meas_done=0x%x v3_status=0x%x v3_finish=0x%x v3_tm_status=0x%x num_ap=0x%x ",
            header->req_id, header->status, measurement_type_str, header->report_type,
            header->meas_done, header->v3_status, header->v3_finish, header->v3_tm_start,
            header->num_ap);
    qdf_print("MAC=%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", dest_mac[0], dest_mac[1], dest_mac[2], dest_mac[3], dest_mac[4], dest_mac[5]);
    memcpy(wifiposdesc->sta_mac_addr, dest_mac, 6);
}

void rtt_print_meas_resp_body(wmi_host_rtt_meas_event *body,
                              ieee80211_ol_wifiposdesc_t * wifiposdesc)
{
    u_int8_t *p;
    u_int8_t index1;

    //qdf_print("%s:", __func__);
    if (wifiposdesc == NULL){
        printk("\nRTTREPORT WIFIPOS: Desc is NULL *****\n");
        return;
    }
    if(body) {
        qdf_print("RTTREPORT Rx ChainMask=:0x%x BW=%d",
                body->chain_mask, body->bw);

        if (body->t3 || body->t4) {
            qdf_print("RTTREPORT Timestamps %llu %llu %llu %llu",
                    body->tod, body->toa, body->t3, body->t4);
        } else {
            qdf_print("RTTREPORT Timestamps %llu 0 0 %llu",
                    body->tod, body->toa);
        }

        wifiposdesc->rssi0 = body->rssi0;
        wifiposdesc->rssi1 = body->rssi1;
        wifiposdesc->rssi2 = body->rssi2;
        wifiposdesc->rssi3 = body->rssi3;

        wifiposdesc->txrxchain_mask = body->txrxchain_mask;
        p = wifiposdesc->hdump;
        for(index1 = 0; index1 < body->txrxchain_mask; index1++){
            qdf_print("%.4x ", *((u_int16_t *)(p)));
            p+=2;
        }
        qdf_print("RTTREPORT RSSI %.8x %.8x %.8x %.8x", wifiposdesc->rssi0, wifiposdesc->rssi1, wifiposdesc->rssi2, wifiposdesc->rssi3);
    } else {
        qdf_print("Error! body is NULL") ;
    }

    return;
}


/*
 * event handler for  RTT measurement response
 * data  -- rtt measurement response from fw
 */

static int
ol_ath_rtt_meas_report_event_handler(ol_scn_t sc, u_int8_t *data,
                    u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    struct ol_ath_softc_net80211 *scn;
#if !ATH_SUPPORT_LOWI
    struct ieee80211com *ic;
#endif
    ieee80211_ol_wifiposdesc_t *wifiposdesc;
    wmi_host_rtt_event_hdr hdr;
    wmi_host_rtt_meas_event body;
    struct common_wmi_handle *wmi_handle;
    struct wlan_objmgr_pdev *pdev;


    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0, WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_print("%s: pdev object (id: 0) is NULL", __func__);
         return -1;
    }
 #if !ATH_SUPPORT_LOWI
    ic = wlan_pdev_get_mlme_ext_obj(pdev);
#endif
    scn = lmac_get_pdev_feature_ptr(pdev);
    if (scn == NULL) {
         qdf_print("%s: scn (id: 0) is NULL", __func__);
         wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
         return -1;
    }

    wmi_handle = lmac_get_wmi_hdl(soc->psoc_obj);
#define HDUMP_LEN 324

	wifiposdesc = (ieee80211_ol_wifiposdesc_t *)OS_MALLOC(scn->sc_osdev, sizeof(* wifiposdesc), GFP_KERNEL);
    if (wifiposdesc == NULL) {
        qdf_print("\n Unable to allocate memory for WIFIPOS desc ");
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }
    if(wmi_extract_rtt_hdr(wmi_handle, data, &hdr)) {
        qdf_print("\n Unable to extract RTT header ");
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }

    wifiposdesc->hdump = (u_int8_t *)OS_MALLOC(scn->sc_osdev, HDUMP_LEN, GFP_KERNEL);
    if (wifiposdesc->hdump == NULL) {
        OS_FREE(wifiposdesc);
        qdf_print("\n Unable to allocate memory for WIFIPOS desc ");
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }

    if (!data) {
        qdf_print("Get NULL point message from FW");
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }

    if(wmi_extract_rtt_ev(wmi_handle, data, &body,
                wifiposdesc->hdump, HDUMP_LEN)) {
        qdf_print("Failed to extract rtt event");
        OS_FREE(wifiposdesc->hdump);
        OS_FREE(wifiposdesc);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }
    rtt_print_resp_header(&hdr, wifiposdesc);
    rtt_print_meas_resp_body(&body, wifiposdesc);

#if !ATH_SUPPORT_LOWI
    if (hdr.meas_type < RTT3_FRAME_TYPE) {
        ic->ic_update_wifipos_stats(ic, (ieee80211_ol_wifiposdesc_t *)wifiposdesc);
    }
#endif
    OS_FREE(wifiposdesc->hdump);
    OS_FREE(wifiposdesc);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    return 0;
}

/*
 * event handler for TSF measurement response
 * data  -- TSF measurement response from fw
 */
static int
ol_ath_tsf_meas_report_event_handle(ol_scn_t sc, u_int8_t *data,
                    u_int32_t datalen)

{
  //ToDo
  return 0;

}

/*
 * event handler for RTT Error report
 * data  -- rtt measurement response from fw
 */
static int
ol_ath_error_report_event_handle(ol_scn_t sc, u_int8_t *data,
                    u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    wmi_host_rtt_error_report_event ev;
    struct ol_ath_softc_net80211 *scn;
#if !ATH_SUPPORT_LOWI
    struct ieee80211com *ic;
#endif
    ieee80211_ol_wifiposdesc_t * wifiposdesc;
    struct common_wmi_handle *pdev_wmi_handle;
    struct wlan_objmgr_pdev *pdev;

    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0, WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_print("%s: pdev object (id: 0) is NULL", __func__);
         return -1;
    }
    scn = lmac_get_pdev_feature_ptr(pdev);
    if (scn == NULL) {
         qdf_print("%s: scn (id: 0) is NULL", __func__);
         wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
         return -1;
    }

 #if !ATH_SUPPORT_LOWI
    ic = wlan_pdev_get_mlme_ext_obj(pdev);
#endif

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(pdev);
    qdf_print("%s: data=%pK, datalen=%u", __func__, data, datalen);
    if (!data) {
        qdf_print("Get NULL point message from FW");
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }

    if(wmi_extract_rtt_error_report_ev(pdev_wmi_handle, data, &ev)) {
        qdf_print("%s:\n Unable to extract RTT header ", __func__);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }

	wifiposdesc = (ieee80211_ol_wifiposdesc_t *)OS_MALLOC(scn->sc_osdev, sizeof(* wifiposdesc), GFP_KERNEL);
    if (wifiposdesc == NULL) {
        qdf_print("\n Unable to allocate memory for WIFIPOS desc ");
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }
    wifiposdesc->hdump = OS_MALLOC(scn->sc_osdev, 324, GFP_KERNEL);
    if (wifiposdesc->hdump == NULL) {
        OS_FREE(wifiposdesc);
        qdf_print("\n Unable to allocate memory for WIFIPOS desc ");
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }

    rtt_print_resp_header(&ev.hdr, wifiposdesc);
    printk("\n The meas type is %d\n",ev.hdr.meas_type);
    if (IS_VALID_INDEX_OF_ARRAY(error_indicator, ev.reject_reason)) {
        qdf_print("%s",error_indicator[ev.reject_reason] );
    }
    else {
        qdf_print("Unknown error index (%d)", ev.reject_reason );
    }
    //KW# 6147 qdf_print("%s",error_indicator[*((WMI_RTT_ERROR_INDICATOR *)data)<10 ? *((WMI_RTT_ERROR_INDICATOR *)data) : 1 ] ) ;

    //If the error code < 10, we can use it, otherwise use some valid error-code between 0-9. I used 1 here to signify error.

    //The developer may change add another code or use a completely different logic to handle this

    /* Only 3 type of frames are used for Measurement type ,
       even though  RTT measurement type is 3 bit header */
    qdf_print("Measurement Type is %s ", (ev.hdr.meas_type < 3)? measurement_type[ev.hdr.meas_type]:"Invalid frame type");
    qdf_print("Report Type is %d", ev.hdr.report_type);
    // if the frame < 2, then only call the update_stats function(RTT2).
#if !ATH_SUPPORT_LOWI
    if (ev.hdr.meas_type < 2) {
        ic->ic_update_wifipos_stats(ic, NULL);
    }
#endif
    OS_FREE(wifiposdesc->hdump);
    OS_FREE(wifiposdesc);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);

    return 1;
}

/*
 * event handler for keepalive notification
 */
static int
ol_ath_rtt_keepalive_event_handle(ol_scn_t sc, u_int8_t *data,
                    u_int32_t datalen)
{
    ol_ath_soc_softc_t *soc = (ol_ath_soc_softc_t *) sc;
    wmi_host_rtt_event_hdr header;
    struct ieee80211com *ic;
    uint8_t *dest_mac;
    struct common_wmi_handle *pdev_wmi_handle;
    struct wlan_objmgr_pdev *pdev;

    pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, 0, WLAN_MLME_SB_ID);
    if (pdev == NULL) {
         qdf_print("%s: pdev object (id: 0) is NULL", __func__);
         return -1;
    }

    ic = wlan_pdev_get_mlme_ext_obj(pdev);
    if (ic == NULL) {
         wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
         qdf_err("ic (id: 0) is NULL");
         return -1;
    }

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(pdev);
    if(wmi_extract_rtt_hdr(pdev_wmi_handle, data, &header)) {
        qdf_print("%s:Unable to extract rtt header", __func__);
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
        return -1;
    }
    qdf_print("\n\n==================Keepalive Event====================");
    qdf_print("Request ID is:0x%x", header.req_id);
    qdf_print("Request result is:%d", header.result);

    dest_mac = header.dest_mac;
    qdf_print("Request dest_mac is:%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", dest_mac[0],dest_mac[1],
                 dest_mac[2],dest_mac[3],dest_mac[4],dest_mac[5]);
    ic->ic_update_ka_done(dest_mac, 1);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_SB_ID);
    return 0;
}
void
ol_ath_rtt_netlink_attach(struct ieee80211com *ic)
{
    int ret = -1;
    if (ic->ic_rtt_init_netlink) {
        ret = ic->ic_rtt_init_netlink(ic);
        if (ret == 0) {
            atomic_inc(&rtt_nl_users);
        }
    }
}

void
ol_if_rtt_detach(struct ieee80211com *ic)
{
    struct ieee80211com *ic_tmp;
    ol_ath_soc_softc_t *soc;
    int soc_idx, scn_idx;
    struct wlan_objmgr_pdev *pdev;

    for (soc_idx = 0; soc_idx < GLOBAL_SOC_SIZE; soc_idx++) {
        soc = ol_global_soc[soc_idx];
        if (soc == NULL)
            continue;
        for (scn_idx = 0; scn_idx < wlan_psoc_get_pdev_count(soc->psoc_obj); scn_idx++) {
            pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, scn_idx, WLAN_MLME_NB_ID);
            if(pdev == NULL) {
                 qdf_print("%s: pdev object (id: %d) is NULL ", __func__, scn_idx);
                 continue;
            }

            ic_tmp = wlan_pdev_get_mlme_ext_obj(pdev);
            if (ic_tmp == NULL) {
                 wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                 qdf_err("ic (id: %d) is NULL", scn_idx);
                 continue;
            }

            if (ic_tmp->rtt_sock == NULL) {
                //ic->rtt_sock = ic_tmp->rtt_sock;
                printk("%s: Socket already released %pK\n", __func__, ic->rtt_sock);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                return;
            }
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
        }
    }

    if (atomic_dec_and_test(&rtt_nl_users) && ic->rtt_sock) {
        OS_SOCKET_RELEASE(ic->rtt_sock);
        ic->rtt_sock = NULL;
        printk(KERN_INFO"\n releasing the socket %pK and val of ic is %pK\n", ic->rtt_sock, ic);
    }
}
/*
 * RTT measurement response handler attach functions for offload solutions
 */
void
ol_ath_rtt_meas_report_attach(ol_ath_soc_softc_t *soc)
{
    wmi_unified_t wmi_handle;

    wmi_handle = lmac_get_wmi_unified_hdl(soc->psoc_obj);

    /* Register WMI event handlers */
    wmi_unified_register_event_handler(wmi_handle,
        wmi_rtt_meas_report_event_id,
        ol_ath_rtt_meas_report_event_handler,
        WMI_RX_UMAC_CTX);

    wmi_unified_register_event_handler(wmi_handle,
        wmi_tsf_meas_report_event_id,
        ol_ath_tsf_meas_report_event_handle,
        WMI_RX_UMAC_CTX);

    wmi_unified_register_event_handler(wmi_handle,
        wmi_rtt_error_report_event_id,
        ol_ath_error_report_event_handle,
        WMI_RX_UMAC_CTX);

    wmi_unified_register_event_handler(wmi_handle,
        wmi_rtt_keepalive_event_id,
        ol_ath_rtt_keepalive_event_handle,
        WMI_RX_UMAC_CTX);

    return;

}

typedef struct {
    struct channel_param *channel;
    struct ieee80211com *ic;
    bool is_2gvht_en;
} channel_search;

/*
 * Find the channel information according to the scan entry
 */
int rtt_find_channel_info (void *arg, wlan_scan_entry_t scan_entry)
{
    struct channel_param *wmi_chan;
    u_int32_t chan_mode;
    struct ieee80211com *ic;
    struct ieee80211_ath_channel *se_chan;
    struct ol_ath_softc_net80211 *scn = NULL;
    bool is_2gvht_en;

    qdf_print("%s:", __func__);

    if (!(arg && scan_entry)) {
        return -1; //critical error
    }

    wmi_chan = ((channel_search *)arg)->channel;
    ic = ((channel_search *)arg)->ic;

    if(!(wmi_chan && ic)) {
        return -1; //critical error
    }

    se_chan = wlan_util_scan_entry_channel(scan_entry);

    if(!se_chan) {
        return -1; //critical error
    }

    wmi_chan->mhz = ieee80211_chan2freq(ic,se_chan);
    chan_mode = ieee80211_chan2mode(se_chan);

    scn = OL_ATH_SOFTC_NET80211(ic);
    if(!scn) {
        return -1; //critical error
    }

    is_2gvht_en = ((channel_search *)arg)->is_2gvht_en;
    wmi_chan->phy_mode = ol_get_phymode_info(scn, chan_mode, is_2gvht_en);

    if(chan_mode == IEEE80211_MODE_11AC_VHT80) {
        if (se_chan->ic_ieee < IS_2GHZ_CH) {
            wmi_chan->cfreq1 = ieee80211_ieee2mhz(
                                             ic,
                                             se_chan->ic_vhtop_ch_freq_seg1,
                                             IEEE80211_CHAN_2GHZ);
        } else {
            wmi_chan->cfreq1 = ieee80211_ieee2mhz(
                                            ic,
                                            se_chan->ic_vhtop_ch_freq_seg1,
                                            IEEE80211_CHAN_5GHZ);
        }
    } else if((chan_mode == IEEE80211_MODE_11NA_HT40PLUS) ||
              (chan_mode == IEEE80211_MODE_11NG_HT40PLUS) ||
              (chan_mode == IEEE80211_MODE_11AC_VHT40PLUS)) {
        wmi_chan->cfreq1 = wmi_chan->mhz + 10;
    } else if((chan_mode == IEEE80211_MODE_11NA_HT40MINUS) ||
              (chan_mode == IEEE80211_MODE_11NG_HT40MINUS) ||
              (chan_mode == IEEE80211_MODE_11AC_VHT40MINUS)) {
        wmi_chan->cfreq1 = wmi_chan->mhz - 10;
    } else {
        wmi_chan->cfreq1 = wmi_chan->mhz;
    }

    /* we do not support HT80PLUS80 yet */
    wmi_chan->cfreq2=0;
    wmi_chan->minpower = se_chan->ic_minpower;
    wmi_chan->maxpower = se_chan->ic_maxpower;
    wmi_chan->maxregpower = se_chan->ic_maxregpower;
    wmi_chan->antennamax = se_chan->ic_antennamax;
    wmi_chan->reg_class_id = se_chan->ic_regClassId;

    if (IEEE80211_IS_CHAN_DFS(se_chan))
        wmi_chan->dfs_set = TRUE;

    qdf_print("WMI channel freq=%d, mode=%x band_center_freq1=%d",
        wmi_chan->mhz, wmi_chan->phy_mode, wmi_chan->cfreq1);

    return 1; //seccessful!
}

#define RTT_TEST 1

#if RTT_TEST

#define RTT_REQ_FRAME_TYPE_LSB    (0)
#define RTT_REQ_FRAME_TYPE_MASK   (0x3 << RTT_REQ_FRAME_TYPE_LSB)
#define RTT_REQ_BW_LSB            (2)
#define RTT_REQ_BW_MASK           (0x3 << RTT_REQ_BW_LSB)
#define RTT_REQ_PREAMBLE_LSB      (4)
#define RTT_REQ_PREAMBLE_MASK     (0x3 << RTT_REQ_PREAMBLE_LSB)
#define RTT_REQ_NUM_REQ_LSB       (6)
#define RTT_REQ_NUM_REQ_MASK      (0xf << RTT_REQ_NUM_REQ_LSB)
#define RTT_REQ_REPORT_TYPE_LSB   (10)
#define RTT_REQ_REPORT_TYPE_MASK  (0x3 << RTT_REQ_REPORT_TYPE_LSB)
#define RTT_REQ_NUM_MEASUREMENTS_LSB   (12)
#define RTT_REQ_NUM_MEASUREMENTS_MASK  (0x1f << RTT_REQ_NUM_MEASUREMENTS_LSB)
#define RTT_REQ_ASAP_MODE_LSB   (20)
#define RTT_REQ_ASAP_MODE_MASK  (0x1 << RTT_REQ_ASAP_MODE_LSB)
#define RTT_REQ_LCI_REQUESTED_LSB   (21)
#define RTT_REQ_LCI_REQUESTED_MASK  (0x1 << RTT_REQ_LCI_REQUESTED_LSB)
#define RTT_REQ_LOC_CIV_REQUESTED_LSB   (22)
#define RTT_REQ_LOC_CIV_REQUESTED_MASK  (0x1 << RTT_REQ_LOC_CIV_REQUESTED_LSB)


void ol_ath_rtt_meas_req_test(struct ieee80211com *ic, u_int8_t *mac_addr, int extra)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    int ret;
    u_int8_t peer[6];  //ap
    struct ieee80211vap *vap;
    static u_int8_t req_id = 1;
    channel_search channel_search_info;
    struct rtt_meas_req_test_params param;
    bool is_2gvht_en;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    //for test purpose, assume there is only one vap
    vap = TAILQ_FIRST(&(ic)->ic_vaps) ;
    is_2gvht_en = ieee80211_vap_256qam_is_set(vap);

    param.req_num_req = MS(extra, RTT_REQ_NUM_REQ) + 1;   /* 1 more, so that 0 -> 1 */
    param.req_frame_type  = MS(extra, RTT_REQ_FRAME_TYPE);
    param.req_bw          = MS(extra, RTT_REQ_BW);
    param.req_preamble    = MS(extra, RTT_REQ_PREAMBLE);
    param.req_report_type = MS(extra, RTT_REQ_REPORT_TYPE);
    param.num_measurements = MS(extra, RTT_REQ_NUM_MEASUREMENTS);
    param.asap_mode = MS(extra, RTT_REQ_ASAP_MODE);
    param.loc_civ_requested = MS(extra, RTT_REQ_LOC_CIV_REQUESTED);
    param.lci_requested = MS(extra, RTT_REQ_LCI_REQUESTED);
    param.req_id = req_id;

    if(param.req_report_type < WMI_HOST_RTT_AGGREGATE_REPORT_NON_CFR) {
	param.req_report_type ^= 1;   /* In command line, 0 - FAC, 1 - CFR, need to revert here */
    }

    if (param.num_measurements == 0)
	param.num_measurements = 25;

    OS_MEMCPY(peer, mac_addr, 6);

    qdf_print("The mac_addr is"
                 " %.2x:%.2x:%.2x:%.2x:%.2x:%.2x extra=%d", peer[0],peer[1], peer[2],
                                               peer[3], peer[4],peer[5], extra);

    //start from here, embed the first req in each RTT measurement Command
    /*peer[5] = 0x12;
    peer[4] = 0x90;
    peer[3] = 0x78;
    peer[2] = 0x56;
    peer[1] = 0x34;
    peer[0] = 0x12;
	*/
    //find channel from the peer mac of first request
    //channel_search_info.channel = (wmi_channel *)&head->channel;
    channel_search_info.channel = &param.channel;
    channel_search_info.ic = ic;
    channel_search_info.is_2gvht_en = is_2gvht_en;
    ret = util_wlan_scan_db_iterate_macaddr(vap, &peer[0],
        (scan_iterator_func)rtt_find_channel_info, &channel_search_info);
    OS_MEMCPY(param.peer, peer, 6);
    if (!ret) {
        qdf_print("Can not find corresponding channel info %d of "
                     "%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", ret, peer[0],peer[1], peer[2],
                     peer[3], peer[4],peer[5]);
        req_id++;
        return;
    } else if (ret == -1){
        req_id++;
        qdf_print("critical error %d", ret);
        return;
    }

    ret = wmi_unified_rtt_meas_req_test_cmd_send(pdev_wmi_handle, &param);
    req_id++;
}
#endif
/*
 * Send RTT measurement Command to FW (for test purpose only) here we encode two
 * STA request in each measurement comments If you need change the embedded
 * value, please search the corresponding macro in wmi_unified.h and choose the
 * macro you want to use.
 */

void
ol_ath_rtt_meas_req(struct ieee80211com *ic,
                    struct ieee80211vap *vap, ieee80211_wifipos_reqdata_t *reqdata)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    int ret;
    u_int32_t chan_mode;
    //struct ieee80211vap *vap;
    u_int16_t req_id = reqdata->request_id;
    struct ieee80211_ath_channel      *chan;
    u_int16_t       freq;
    struct rtt_meas_req_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    qdf_print("%s: The request ID is: %d", __func__, req_id);
    if(!pdev_wmi_handle) {
        qdf_print("WMI ERROR:Invalid wmi_handle  ");
        req_id++;
        return;
    }

    param.req_id = req_id;
    //for test purpose, assume there is only one vap
    //vap = TAILQ_FIRST(&(ic)->ic_vaps) ;

    /* Temporarily, hardcoding peer mac address for test purpose will be removed
     * once RTT host has been developed for even req_id, loke 0, 2, 4, there is
     * no channel_swicth for odd req_id, like 1, 3 ,5, there is channel switch
     * currently, for both cases, we have 3 req in each command please change
     * here if you only have one (or just let it be). Even == HC, odd == OC.
     */

    param.vdev_id = (OL_ATH_VAP_NET80211(vap))->av_if_id;
    param.sta_mac_addr = reqdata->sta_mac_addr;

    if(reqdata->oc_channel != ic->ic_curchan->ic_ieee) {
        if (reqdata->oc_channel < IS_2GHZ_CH) {
            param.channel.mhz = ieee80211_ieee2mhz(ic,reqdata->oc_channel,IEEE80211_CHAN_2GHZ);
        }
        else {
            param.channel.mhz = ieee80211_ieee2mhz(ic,reqdata->oc_channel,IEEE80211_CHAN_5GHZ);
        }
        chan = ol_ath_find_full_channel(ic, param.channel.mhz);
        if (chan) {
        chan_mode = ieee80211_chan2mode(chan);
        freq = chan->ic_freq;
        if(chan_mode == IEEE80211_MODE_11AC_VHT80) {
        if (chan->ic_ieee < IS_2GHZ_CH)
            param.channel.cfreq1 = ieee80211_ieee2mhz(ic,
                                        chan->ic_vhtop_ch_freq_seg1, IEEE80211_CHAN_2GHZ);
        else
            param.channel.cfreq1 = ieee80211_ieee2mhz(ic,
                                        chan->ic_vhtop_ch_freq_seg1, IEEE80211_CHAN_5GHZ);
        } else if((chan_mode == IEEE80211_MODE_11NA_HT40PLUS) || (chan_mode == IEEE80211_MODE_11NG_HT40PLUS) ||
                   (chan_mode == IEEE80211_MODE_11AC_VHT40PLUS)) {
            param.channel.cfreq1 = freq + 10;
        } else if((chan_mode == IEEE80211_MODE_11NA_HT40MINUS) || (chan_mode == IEEE80211_MODE_11NG_HT40MINUS) ||
                   (chan_mode == IEEE80211_MODE_11AC_VHT40MINUS)) {
                   param.channel.cfreq1 = freq - 10;
                   } else {
                   param.channel.cfreq1 = freq;
                   }
                   /* we do not support HT80PLUS80 yet */
        param.channel.cfreq2=0;
        param.channel.minpower = chan->ic_minpower;
        param.channel.maxpower = chan->ic_maxpower;
        param.channel.maxregpower = chan->ic_maxregpower;
        param.channel.antennamax = chan->ic_antennamax;
        param.channel.reg_class_id = chan->ic_regClassId;

        if (chan->ic_ieee < IS_2GHZ_CH) {
            param.is_mode_na = TRUE;
        }
        else {
            param.is_mode_ac = TRUE;
        }
        if (IEEE80211_IS_CHAN_DFS(ic->ic_curchan))
            param.channel.dfs_set = TRUE;

        param.channel.cfreq2 = 0;
    }
    }
    else {
        if (reqdata->hc_channel < IS_2GHZ_CH) {
            param.channel.mhz = ieee80211_ieee2mhz(ic,reqdata->hc_channel,IEEE80211_CHAN_2GHZ);
        }
        else {
            param.channel.mhz = ieee80211_ieee2mhz(ic,reqdata->hc_channel,IEEE80211_CHAN_5GHZ);
        }
        chan = ol_ath_find_full_channel(ic, param.channel.mhz);
        if (chan) {
        chan_mode = ieee80211_chan2mode(chan);
        freq = chan->ic_freq;
        if(chan_mode == IEEE80211_MODE_11AC_VHT80) {
        if (chan->ic_ieee < IS_2GHZ_CH)
            param.channel.cfreq1 = ieee80211_ieee2mhz(ic,
                                        chan->ic_vhtop_ch_freq_seg1, IEEE80211_CHAN_2GHZ);
        else
            param.channel.cfreq1 = ieee80211_ieee2mhz(ic,
                                        chan->ic_vhtop_ch_freq_seg1, IEEE80211_CHAN_5GHZ);
        } else if((chan_mode == IEEE80211_MODE_11NA_HT40PLUS) || (chan_mode == IEEE80211_MODE_11NG_HT40PLUS) ||
                   (chan_mode == IEEE80211_MODE_11AC_VHT40PLUS)) {
            param.channel.cfreq1 = freq + 10;
        } else if((chan_mode == IEEE80211_MODE_11NA_HT40MINUS) || (chan_mode == IEEE80211_MODE_11NG_HT40MINUS) ||
                   (chan_mode == IEEE80211_MODE_11AC_VHT40MINUS)) {
                   param.channel.cfreq1 = freq - 10;
                   } else {
                   param.channel.cfreq2 = freq;
                   }
                   /* we do not support HT80PLUS80 yet */
        param.channel.cfreq2=0;

        param.channel.minpower = chan->ic_minpower;
        param.channel.maxpower = chan->ic_maxpower;
        param.channel.maxregpower = chan->ic_maxregpower;
        param.channel.antennamax = chan->ic_antennamax;
        param.channel.reg_class_id = chan->ic_regClassId;

        if (chan->ic_ieee < IS_2GHZ_CH) {
            param.is_mode_na = TRUE;
        }
        else {
            param.is_mode_ac = TRUE;
        }
        if (IEEE80211_IS_CHAN_DFS(ic->ic_curchan))
            param.channel.dfs_set = TRUE;

    }
    }

    param.spoof_mac_addr = reqdata->spoof_mac_addr;

    //embedded varing part of each request
    //set Preamble, BW, measurement times
    if ((reqdata->bandwidth == 0) || (reqdata->bandwidth == 1))
        param.is_bw_20 = TRUE;
    else if (reqdata->bandwidth == 2)
        param.is_bw_40 = TRUE;
    else if (reqdata->bandwidth == 3)
        param.is_bw_80 = TRUE;

    param.num_probe_rqst = reqdata->num_probe_rqst;

    ret = wmi_unified_rtt_meas_req_cmd_send(pdev_wmi_handle, &param);
}

/*
 * Send Keepalive command to FW to probe a single associated station.
 * Make sure the client is awake before sending RTT measurement command.
 */
void
ol_ath_rtt_keepalive_req(struct ieee80211vap *vap,
                         struct ieee80211_node *ni, bool stop)
{
    static u_int16_t req_id = 1;
    struct ieee80211com *ic;
    struct ol_ath_softc_net80211 *scn;
    struct rtt_keepalive_req_params param;
    int ret;
    struct common_wmi_handle *pdev_wmi_handle;

    qdf_mem_set(&param, sizeof(param), 0);
    if (ni == NULL) {
        return;
    }

    ic = ni->ni_ic;
    scn = OL_ATH_SOFTC_NET80211(ic);

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    stop = 0;
    param.stop = stop;
    if ((!vap || !ni) || (vap != ni->ni_vap) || (!pdev_wmi_handle)) {
        qdf_print("%s: Invalid parameter", __func__);
        req_id++;
        return;
    }

    param.req_id = req_id;
    param.vdev_id = (OL_ATH_VAP_NET80211(vap))->av_if_id;
    IEEE80211_ADDR_COPY(param.macaddr, ni->ni_macaddr);

    ret = wmi_unified_rtt_keepalive_req_cmd_send(pdev_wmi_handle, &param);
    req_id = param.req_id;
    return;
}

extern u_int32_t ap_lcr[RTT_LOC_CIVIC_REPORT_LEN];
extern int num_ap_lci;
extern u_int32_t ap_lci[RTT_LOC_CIVIC_INFO_LEN];
extern int num_ap_lcr;

int ol_ath_lci_set(struct ieee80211com *ic,
                    void *lci_data)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct ieee80211_lci_subelement_info lci_sub;
    u_int8_t colocated_bss[IEEE80211_COLOCATED_BSS_MAX_LEN]={0};
    struct lci_set_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);

    /* Get colocated bss and populate colocated_bssid_info field in wmi_rtt_lci_cfg_head */
    wlan_iterate_vap_list(ic, ieee80211_vap_iter_get_colocated_bss,(void *) &colocated_bss);

    param.lci_data = lci_data;
    param.colocated_bss = colocated_bss;

    if(wmi_unified_lci_set_cmd_send(pdev_wmi_handle, &param)) {
        printk("%s:Unable to set LCI data\n", __func__);
        return -1;
    }

    /* Save LCI data in host buffer */
    {

        lci_sub.latitude_unc = param.latitude_unc;
        lci_sub.latitude_0_1 = param.latitude_0_1;
        lci_sub.latitude_2_33 = param.latitude_2_33;
        lci_sub.longitude_unc = param.longitude_unc;
        lci_sub.longitude_0_1 = param.longitude_0_1;
        lci_sub.longitude_2_33 = param.longitude_2_33;
        lci_sub.altitude_type = param.altitude_type;
        lci_sub.altitude_unc_0_3 = param.altitude_unc_0_3;
        lci_sub.altitude_unc_4_5 = param.altitude_unc_4_5;
        lci_sub.altitude = param.altitude;
        lci_sub.datum = param.datum;
        lci_sub.reg_loc_agmt = param.reg_loc_agmt;
        lci_sub.reg_loc_dse = param.reg_loc_dse;
        lci_sub.dep_sta = param.dep_sta;
        lci_sub.version = param.version;

	/* Max size of LCI is 16 octets */
        OL_IF_MSG_COPY_CHAR_ARRAY(ap_lci, &lci_sub, sizeof(struct ieee80211_lci_subelement_info));
        num_ap_lci = 1;
        qdf_print("Copied LCI num %d to driver cache", num_ap_lci);
    }
    return 0;
}

int ol_ath_lcr_set(struct ieee80211com *ic,
                    void *lcr_data)
{
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    struct lcr_set_params param;
    struct common_wmi_handle *pdev_wmi_handle;

    pdev_wmi_handle = lmac_get_pdev_wmi_handle(scn->sc_pdev);
    qdf_mem_set(&param, sizeof(param), 0);
    param.lcr_data = lcr_data;

    if(wmi_unified_lcr_set_cmd_send(pdev_wmi_handle, &param)) {
        printk("%s:Unable to set LCR data\n", __func__);
        return -1;
    }

    /* Save LCR data in host buffer */
    /* lcr_data is pointing to wmi_rtt_lcr_cfg_head defined in wpc.c, need to move 8 bytes to get to civic_info[64] */
    lcr_data = lcr_data + 8;
    OL_IF_MSG_COPY_CHAR_ARRAY(ap_lcr, lcr_data, RTT_LOC_CIVIC_REPORT_LEN*4); /* Max size for LCR is 256 */

    num_ap_lcr = 1;
    qdf_print("Copy LCR num %d to driver cache", num_ap_lcr);

    return 0;
}

//#if ATH_SUPPORT_WIFIPOS
int ol_ieee80211_wifipos_xmitprobe (struct ieee80211vap *vap,
                                        ieee80211_wifipos_reqdata_t *reqdata)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_ath_rtt_meas_req(&scn->sc_ic, vap, reqdata);
    return 1;

}

int ol_ieee80211_wifipos_xmitrtt3 (struct ieee80211vap *vap, u_int8_t *mac_addr,
                                        int extra)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_ath_rtt_meas_req_test(&scn->sc_ic, mac_addr, extra);
    return 0;

}

int ol_ieee80211_lci_set (struct ieee80211vap *vap,
                                        void *reqdata)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_ath_lci_set(&scn->sc_ic, reqdata);
    return 0;

}

int ol_ieee80211_lcr_set (struct ieee80211vap *vap,
                                        void *reqdata)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
    ol_ath_lcr_set(&scn->sc_ic, reqdata);
    return 0;

}

#endif

