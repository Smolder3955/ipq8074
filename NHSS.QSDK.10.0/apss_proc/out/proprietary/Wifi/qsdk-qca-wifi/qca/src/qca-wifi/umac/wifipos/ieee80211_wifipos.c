/*
 *
 * Copyright (c) 2011-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011-2016 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <net/genetlink.h>
#include <linux/netlink.h>
#include <ieee80211_var.h>
//#include <ath_dev.h>
#include <ath_internal.h>
#include <if_athvar.h>
#include <ieee80211_vap_tsf_offset.h>
#define ATH_SUPPORT_WIFIPOS 1
#if ATH_SUPPORT_WIFIPOS
//#include <ieee80211_wpc.h>
//#include <ol_if_athvar.h>
#include <ieee80211_wifipos.h>
#include <ieee80211_wifipos_pvt.h>
#if WIFI_MEM_MANAGER_SUPPORT
#include <mem_manager.h>
#endif

typedef struct {
	A_UINT32 time32; /* upper 32 bits of time stamp */
	A_UINT32 time0;  /* lower 32 bits of time stamp */
} A_TIME64;

// Note:TO enable dynamic debug use mask as 0x20 Eg:iwpriv ath0 dbgLVL_high 0x20
#define WIFIPOS_DPRINTK(format, args...)  printk(KERN_DEBUG "%s:%d " format "\n",__FUNCTION__, __LINE__, ## args)

//TBD: Remove
#ifndef IEEE80211_WIFIPOSDEBUG
#define IEEE80211_WIFIPOSDEBUG 1
#endif

#define tsf_start1 pvt_wifiposdata->cctiming.tsf_start1
#define tsf_start2 pvt_wifiposdata->cctiming.tsf_start2
#define tsf_end1 pvt_wifiposdata->cctiming.tsf_end1
#define tsf_end2 pvt_wifiposdata->cctiming.tsf_end2
#define cnt_cc pvt_wifiposdata->cctiming.cnt_cc
#define wpnt pvt_wifiposdata->cctiming.wpnt
#define wlsct0 pvt_wifiposdata->cctiming.wlsct0
#define wgpt pvt_wifiposdata->cctiming.wgpt
#define wlsct1 pvt_wifiposdata->cctiming.wlsct1
#define wcnt pvt_wifiposdata->cctiming.wcnt
#define wrtxqst pvt_wifiposdata->cctiming.wrtxqst
#define wupnt pvt_wifiposdata->cctiming.wupnt
#define CTStet pvt_wifiposdata->cctiming.CTStet
#define avg_rxclear_pct pvt_wifiposdata->cctiming.avg_rxclear_pct
#define avg_rxframe_pct pvt_wifiposdata->cctiming.avg_rxframe_pct
#define avg_txframe_pct pvt_wifiposdata->cctiming.avg_txframe_pct
#define MEM_ALIGN(x) (((x)<<1)+3) & 0xFFFC


// This is need to change the RX chain mask to num of chains
static u_int8_t ieee80211_mask2numchains[8] = {
     0 /* 000 */,
     1 /* 001 */,
     1 /* 010 */,
     2 /* 011 */,
     1 /* 100 */,
     2 /* 101 */,
     2 /* 110 */,
     3 /* 111 */
};

//TBD: Move these definitions to separate file, extend for other protocol versions
static int packetlength[MAX_PKT_TYPES] =
    { IEEE80211_FRAME_LEN, IEEE80211_QOSFRAME_LEN, 0 };
static int ieee80211_wifipos_databits_per_symbol[] = { 192, 96, 48, 24, 216, 144, 72, 36 };
static int ieee80211_wifipos_databits_per_symbol_11n[] =
    { 26, 52, 78, 104, 156, 208, 234, 260, 52, 104, 156, 208, 312, 416, };
static int ieee80211_wifipos_databits_per_symbol_11b[] = { 0, 0, 0, 0 };

static bool update_bit_pos = false;	//turns on positioning registers on receiving the first measurement request

extern struct ath_softc_net80211 *global_scn[10];
extern int num_global_scn;
extern ol_ath_soc_softc_t *ol_global_soc[GLOBAL_SOC_SIZE];
extern int ol_num_global_soc;

//EXPORT_SYMBOL(ol_global_scn);
//EXPORT_SYMBOL(ol_num_global_scn);

static struct ieee80211vap *ieee80211_wifipos_find_vap(unsigned int vap_idx, struct nsp_mrqst *);


static void ieee80211_wifipos_sendtsfresp(
                            struct ieee80211vap *vap,
                            ieee80211_wifipos_tsfreqdata_t *tsfreqdata);

static ieee80211_wifipos_reqdata_t *ieee80211_wifipos_createreqdata(
                            struct ieee80211vap *vap,
                            struct nsp_mrqst *mrqst);
static ieee80211_wifipos_tsfreqdata_t *ieee80211_wifipos_createtsfrqstdata(
                            struct ieee80211vap *vap,
                            struct nsp_tsf_req *tsfrqst);

int ieee80211_wifipos_syncps_timer(void *arg);

static int ieee80211_wifipos_syncpsenabled(
                            struct ieee80211vap *vap,
                            ieee80211_wifipos_reqdata_t *reqdata);
static int ieee80211_wifipos_schedsyncps(
                            struct ieee80211vap *vap,
                            ieee80211_wifipos_reqdata_t * reqdata);

static void ieee80211_wifipos_resume_hc_traffic(
                            struct ieee80211vap *vap,
                            int hc,
                            struct ieee80211_node *ni_tmp_sta);

void ieee80211_print_mrqst(struct ieee80211vap *vap, struct nsp_mrqst *mrqst);
void ieee80211_print_mresp(struct ieee80211vap *vap, struct nsp_mresp *mresp);
void ieee80211_print_sta_info(struct ieee80211vap *vap, struct nsp_station_info *sta_info);
void ieee80211_print_sresp(struct ieee80211vap *vap, struct nsp_sresp *sresp);
void ieee80211_print_cresp(struct ieee80211vap *vap, struct nsp_cap_resp *cresp);
void ieee80211_print_crqst(struct ieee80211vap *vap, struct nsp_cap_req *creq);
void ieee80211_print_srqst(struct ieee80211vap *vap, struct nsp_sreq *sreq);
void ieee80211_print_slrqst(struct ieee80211vap *vap, struct nsp_sleep_req *sreq);
void ieee80211_print_wrqst(struct ieee80211vap *vap, struct nsp_wakeup_req *wreq);
void ieee80211_print_wresp(struct ieee80211vap *vap, struct nsp_wakeup_resp *wresp);
void ieee80211_print_slresp(struct ieee80211vap *vap, struct nsp_sleep_resp *slresp);
void ieee80211_print_nsphdr(struct ieee80211vap *vap, struct nsp_header *nsp_hdr);
void ieee80211_print_tsfrqst(struct ieee80211vap *vap, struct nsp_tsf_req *tsfrqst);
void ieee80211_print_tsfresp(struct ieee80211vap *vap, struct nsp_tsf_resp *tsfresp);
void ieee80211_print_type1_payld_resp(struct ieee80211vap *vap,
				      struct nsp_type1_resp *type1_payld_resp);


int ieee80211_wifipos_calccctimingstats(ieee80211_wifipos_pvt_data_t *wifiposdata);

static void ieee80211_wifipos_do_framelength_correction_in_tod(
                            ieee80211_wifipos_pvt_data_t *wifiposdata,
                            ieee80211_wifiposdesc_t *wifiposdesc,
                            ieee80211_wifipos_reqdata_t *reqdata);
static void ieee80211_wifipos_do_framelength_correction_in_toa(
                            ieee80211_wifipos_pvt_data_t *wifiposdata,
                            ieee80211_wifiposdesc_t *wifiposdesc);

static u_int64_t ieee80211_wifipos_get_next_tbtt_tsf_64(wlan_if_t vap);
static unsigned int get_dbps_by_rate(int rate);
static void sta_list(void *arg, struct ieee80211_node *ni);

static void ieee80211_wifipos_clear_reqdata(ieee80211_wifipos_reqdata_t *reqdata);
static void ieee80211_wifipos_update_allreqdata_state(
                            ieee80211_wifipos_pvt_data_t *wifiposdata,
                            int state);
static inline void ieee80211_wifipos_dump_reqdata(ieee80211_wifipos_reqdata_t *reqdata);

static uint16_t ieee80211_wifipos_chk_num_req_inprogress(
                            ieee80211_wifipos_pvt_data_t *wifiposdata);
static uint16_t ieee80211_wifipos_chk_num_mrqst_inprogress(
                            ieee80211_wifipos_pvt_data_t *wifiposdata);
static uint16_t ieee80211_wifipos_chk_num_tsfrqst_inprogress(
                            ieee80211_wifipos_pvt_data_t *wifiposdata);

static struct ieee80211vap *ieee80211_wifipos_find_vap(unsigned int vap_idx, struct nsp_mrqst * mrqst)
{
    struct ieee80211com *ic;
    struct ieee80211vap *vap = NULL, *tvap = NULL, *pvap =NULL;
    int i, found = 0;
    struct ath_softc *sc;
    ol_ath_soc_softc_t *soc;
    int soc_idx, scn_idx;
    struct wlan_objmgr_pdev *pdev;

    for (soc_idx = 0; soc_idx < GLOBAL_SOC_SIZE; soc_idx++) {
        soc = ol_global_soc[soc_idx];
        if (soc == NULL)
            continue;
        for (scn_idx = 0; scn_idx < wlan_psoc_get_pdev_count(soc->psoc_obj); scn_idx++) {
            pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, scn_idx, WLAN_MLME_NB_ID);
            if (pdev == NULL) {
                 qdf_print("%s: pdev object (id: %d) is NULL ", __func__, scn_idx);
                 continue;
            }

            ic = wlan_pdev_get_mlme_ext_obj(pdev);
            if (ic == NULL) {
                 wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                 qdf_err("ic (id: %d) is NULL ", scn_idx);
                 continue;
            }

            if (!TAILQ_EMPTY(&ic->ic_vaps)) {
                TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
                    if(vap->iv_wifipos)
                    {
                        if(mrqst == NULL) {
                            if(vap->iv_wifipos)
                            {
                                pvap = vap;
                                found =1;
                                break;
                            }
                            break;
                        }
                        if ((vap_idx == vap->iv_unit) &&
                            (((mrqst->channel < 20) && (ic->ic_curchan->ic_ieee < 20)) || ((mrqst->channel > 20) && (ic->ic_curchan->ic_ieee > 20)))) {
                            pvap = vap;
                            found = 1;
                            break;
                        }
                    }
                }
            }
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
            if (found)
               return pvap;
            }
	}

    for (i = 0; i < num_global_scn; i ++) {
        sc = ATH_DEV_TO_SC(global_scn[i]->sc_dev);
        ic = (struct ieee80211com *)sc->sc_ieee;

        if (!TAILQ_EMPTY(&ic->ic_vaps)) {
            TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
                if(vap->iv_wifipos)
                {
                    pvap = vap;
                    break;
                }
            }
        }
    }
    return pvap;
}


/* Function     : ieee80211_wifipos_update_allreqdata_state
 * Arguments    : state
 * Functionality: updates the state of all the request (tsf, mrqst)
 *                This is currently call to change the state to IDLE.
 * Return       : Void
 */

static void ieee80211_wifipos_update_allreqdata_state(
                            ieee80211_wifipos_pvt_data_t *pvt_wifiposdata,
                            int state)
{
    int count = 0;
    ieee80211_wifipos_tsfreqdata_t *tsfreqdata;
    ieee80211_wifipos_reqdata_t *reqdata;

    for (count = 0; count < ATH_WIFIPOS_MAX_CONCURRENT_TSF_AP; count++) {
        tsfreqdata = &pvt_wifiposdata->tsfreq[count];
#if IEEE80211_WIFIPOSDEBUG
        WIFIPOS_DPRINTK("%s[%d]: ReqId: %d, Rstate: %d, HC: %d, OC: %d, GState: %d, data_index: %d\n",
                        __func__, __LINE__,
                        tsfreqdata->request_id,
                        tsfreqdata->state,
                        tsfreqdata->hc_channel,
                        tsfreqdata->oc_channel,
                        pvt_wifiposdata->state,
                        count);
#endif

        if(tsfreqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC
                || tsfreqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC){
            tsfreqdata->state = state;
        }
    }
    for (count = 0; count < ATH_WIFIPOS_MAX_CONCURRENT_REQUESTS; count++) {
        reqdata = &pvt_wifiposdata->reqdata[count];
        if(reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC ||
                reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC){
#if IEEE80211_WIFIPOSDEBUG
           WIFIPOS_DPRINTK("%s[%d]: ReqId: %d, Rstate: %d, HC: %d, OC: %d, GState: %d, data_index: %d\n",
                        __func__, __LINE__,
                        reqdata->request_id,
                        reqdata->state,
                        reqdata->hc_channel,
                        reqdata->oc_channel,
                        pvt_wifiposdata->state,count );
#endif
            reqdata->state = state;

            if(pvt_wifiposdata->num_mrqst_inprogress > 0){
                pvt_wifiposdata->num_mrqst_inprogress--;
            } else
                printk("%s[%d]: ERROR: Current RQST COUNT MisMatch!!!! \n", __func__, __LINE__);
        }
    }
    if(pvt_wifiposdata->num_mrqst_inprogress > 0){
        printk("%s[%d]: Error: After resetting all the curr req is non zero: %d \n",
                __func__, __LINE__,
                pvt_wifiposdata->num_mrqst_inprogress);
        pvt_wifiposdata->num_mrqst_inprogress = 0;
    }
}

/* Function     : ieee80211_wifipos_chk_num_req_inprogress
 * Arguments    : void
 * Functionality: To check how many request are currently being served.
 * Return       : total number of request in progress
 */

static uint16_t ieee80211_wifipos_chk_num_req_inprogress(
                            ieee80211_wifipos_pvt_data_t *pvt_wifiposdata)
{
    uint16_t total_inprogress = 0;

    total_inprogress += ieee80211_wifipos_chk_num_tsfrqst_inprogress(pvt_wifiposdata);
    total_inprogress += ieee80211_wifipos_chk_num_mrqst_inprogress(pvt_wifiposdata);
#if IEEE80211_WIFIPOSDEBUG
    WIFIPOS_DPRINTK("%s[%d]: Total request in progress: %d \n", __func__, __LINE__, total_inprogress);
#endif
    return total_inprogress;

}

/* Function     : ieee80211_wifipos_chk_num_tsfrqst_inprogress
 * Arguments    : void
 * Functionality: To check how many tsf requests are currently being served.
 * Return       : total number of tsf request in progress
 */
static uint16_t ieee80211_wifipos_chk_num_tsfrqst_inprogress(
                            ieee80211_wifipos_pvt_data_t *pvt_wifiposdata)
{
    int count = 0;
    uint16_t inprogress_cnt = 0;
    ieee80211_wifipos_tsfreqdata_t *reqdata;

    //Search for request entry in request data store and tsf data store
    //Process all requests upto and including the first request which needs a channel change
    //The first channel change request will reschedule this work queue on completion, if there are pending requests
    for (count = 0; count < ATH_WIFIPOS_MAX_CONCURRENT_TSF_AP; count++) {
        reqdata = &pvt_wifiposdata->tsfreq[count];
        if(reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC ||
           reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC) {
            inprogress_cnt++;
        }
    }
#if IEEE80211_WIFIPOSDEBUG
    WIFIPOS_DPRINTK("%s[%d]: Total trequest in progress: %d \n", __func__, __LINE__, inprogress_cnt);
#endif
    return inprogress_cnt;
}

/* Function     : ieee80211_wifipos_chk_num_mrqst_inprogress
 * Arguments    : void
 * Functionality: To check how many measurment requests are currently being served.
 * Return       : total number of measurement requests in progress
 */

static uint16_t ieee80211_wifipos_chk_num_mrqst_inprogress(
                            ieee80211_wifipos_pvt_data_t *pvt_wifiposdata)
{
    int count = 0;
    uint16_t inprogress_cnt = 0;
    ieee80211_wifipos_reqdata_t *reqdata;

    //Search for request entry in request data store and tsf data store
    //Process all requests upto and including the first request which needs a channel change
    //The first channel change request will reschedule this work queue on completion, if there are pending requests
    for (count = 0; count < ATH_WIFIPOS_MAX_CONCURRENT_REQUESTS; count++) {
        reqdata = &pvt_wifiposdata->reqdata[count];
        if(reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC ||
           reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC) {
            inprogress_cnt++;
        }
    }
#if IEEE80211_WIFIPOSDEBUG
    WIFIPOS_DPRINTK("%s[%d]: Total mrequest in progress: %d \n", __func__, __LINE__, inprogress_cnt);
#endif
    return inprogress_cnt;
}
/* Function     : ieee80211_wifipos_dump_reqdata
 * Arguments    : meassurement request
 * Functionality: print all the fields of measurement request
 * Return       : void
 */
static inline void ieee80211_wifipos_dump_reqdata(ieee80211_wifipos_reqdata_t *reqdata)
{
    WIFIPOS_DPRINTK("%x %x %x %x %x \n",
            reqdata->rateset,
            reqdata->retryset,
            reqdata->pkt_type,
            reqdata->txchainmask,
            reqdata->rxchainmask);
    WIFIPOS_DPRINTK("%x %x %x:  %x %x %x\n",
            reqdata->sta_mac_addr[3],
            reqdata->sta_mac_addr[4],
            reqdata->sta_mac_addr[5],
            reqdata->spoof_mac_addr[3],
            reqdata->spoof_mac_addr[4],
            reqdata->spoof_mac_addr[5]);
    WIFIPOS_DPRINTK("%x %x %x %x %x %x %x \n",
            reqdata->mode, reqdata->request_id,
            reqdata->request_cnt,
            reqdata->oc_channel,
            reqdata->hc_channel ,
            reqdata->num_probe_rqst,
            reqdata->state);
}

/* Function     : ieee80211_wifipos_clear_reqdata
 * Arguments    : meassurement request
 * Functionality: re-initialize all the fields of measurement request
 * Return       : void
 */
static inline void ieee80211_wifipos_clear_reqdata(ieee80211_wifipos_reqdata_t *reqdata)
{
#if IEEE80211_WIFIPOSDEBUG
    WIFIPOS_DPRINTK("%s[%d]: ReqId: %x is cleared \n", __func__, __LINE__, reqdata->request_id);
#endif

    reqdata->rateset = 0;
    reqdata->retryset = 0;
    reqdata->pkt_type = 0;
    reqdata->txchainmask = 0;
    reqdata->rxchainmask = 0;
    memset(reqdata->sta_mac_addr, 0, IEEE80211_ADDR_LEN);
    memset(reqdata->spoof_mac_addr, 0, IEEE80211_ADDR_LEN);
    reqdata->mode = 0;
    reqdata->request_id = 0;
    reqdata->request_cnt = 0;
    reqdata->oc_channel = 0; // off - channel
   reqdata->hc_channel = 0; // home channel
    reqdata->req_tstamp = 0;
    reqdata->num_probe_rqst = 0;
    reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_IDLE;
    reqdata->ni_tmp_sta = NULL;
}
/* Function     : ieee80211_wifipos_fsm
 * Arguments    : Event, data
 * Functionality: Finite state machine for wifiposmgr.
 *                Change wifiposmgrstate and reqdata state, based on current state and event
	            //Finite State Machine for wifipos manager state and reqdata state
            	//The following events cause a state change:
            	// (i) Measurement request with channel change
            	// (ii) TSF request with channel change
            	// (ii) Return from channel change (RTC)
            	// (ii) Sync PS timer expired

            	//The FSM function can be called from several different contexts:
            	//(i) User process context: Requests from wpc
            	//(ii) Bottom half: rx/tx completion via update_wifipos_stats)
            	//(iii) Timer: sync ps, channel change
            	//(iv) Work queue: Resuming probes/tsf after channel change
 * Return       : Void
 */
int ieee80211_wifipos_fsm(struct ieee80211vap *vap, int event, void *data, int idle)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_data_t *wifiposdata = vap->iv_wifipos;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
    ieee80211_wifipos_reqdata_t *reqdata;
    ieee80211_wifipos_tsfreqdata_t *tsfreqdata;

    if ( NULL == vap->iv_wifipos ) {
        return ( -1 );
    }
    pvt_wifiposdata=(ieee80211_wifipos_pvt_data_t *)wifiposdata->pvt_wifiposdata;

    spin_lock(&pvt_wifiposdata->lock);


    // TBD: Scheduling Policy doesnot gaurantee fairness. if request comes in IDLE state.
	switch (event) {
	case IEEE80211_WIFIPOS_EVENT_MRQST:
		reqdata = (ieee80211_wifipos_reqdata_t *) data;
#if IEEE80211_WIFIPOSDEBUG
        WIFIPOS_DPRINTK("%s[%d]: ReqId: %x EVENT: %d, GSTATE: %d T: %x \n", __func__, __LINE__,
                reqdata->request_id,
                IEEE80211_WIFIPOS_EVENT_MRQST,
                pvt_wifiposdata->state,
                ic->ic_get_TSF32(ic));
#endif
		switch (pvt_wifiposdata->state) {
		case IEEE80211_WIFIPOSMGR_STATE_IDLE:
			//Set reqdata state based on current wifposmgr state and reqdata info
			//TBD: What happens to probes to associated STAs if sync ps is set: TSFREQ fails, so are they timed?
			//Need to handle this
            if(pvt_wifiposdata->num_mrqst_inprogress > 0){
printk("%s: %d: (pvt_wifiposdata->num_mrqst_inprogress > 0)\n", __FUNCTION__, __LINE__);
                ieee80211_wifipos_update_allreqdata_state(pvt_wifiposdata,
                        IEEE80211_WIFIPOS_RQSTSTATE_IDLE);
            }

			if ((reqdata->mode & SYNCHRONIZE) && ieee80211_wifipos_syncpsenabled(vap, reqdata)) {
				ieee80211_wifipos_schedsyncps(vap, reqdata);
			} else {
				//Check for wifiposmgr statechange
				if (IEEE80211_ADDR_IS_VALID(reqdata->sta_mac_addr)
                        && reqdata->oc_channel != ic->ic_curchan->ic_ieee) {
					if ((pvt_wifiposdata->ccreqdata) || (pvt_wifiposdata->cctsfreqdata))
					{
					    printk("%s : %d: Error: Concurrent channel change attempted\n",
						        __FUNCTION__, __LINE__);
                        spin_unlock(&pvt_wifiposdata->lock);
						return -1;
					}
                    printk("%s: %d: IDLE->OC\n", __FUNCTION__, __LINE__);
                    // NEXT GLOBAL STATE and NEXT REQ STATE
					pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC;
				    reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC;
                    // update CCreq data
					pvt_wifiposdata->ccreqdata = reqdata;
					pvt_wifiposdata->cctsfreqdata = NULL;
				} else {
                    printk("%s: %d: IDLE->HC\n", __FUNCTION__, __LINE__);
                    // NEXT GLOBAL STATE and NEXT REQ STATE
					pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_RUNNING_HC;
				    reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC;
                }
				//Output: Xmit probe
				ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_MREQ, __LINE__,(void *)reqdata);
				wifiposdata->xmitprobe(vap, reqdata);
			}
            break;
		case IEEE80211_WIFIPOSMGR_STATE_RUNNING_HC:
			//Set reqdata state based on current wifposmgr state and reqdata info
			//TBD: What happens to probes to associated STAs if sync ps is set: TSFREQ fails, so are they timed?
			//Need to handle this
			if ((reqdata->mode & SYNCHRONIZE) && ieee80211_wifipos_syncpsenabled(vap, reqdata)) {
                // NEXT REQ STATE
				reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_TIMER;	//wifiposmgr statei will be changed in timerhandler
				ieee80211_wifipos_schedsyncps(vap, reqdata);
			} else {
				//Check for wifiposmgr statechange
				if (reqdata->oc_channel != ic->ic_curchan->ic_ieee) {
				    if ((pvt_wifiposdata->ccreqdata) || (pvt_wifiposdata->cctsfreqdata)) {
						WIFIPOS_DPRINTK("%s : %d: Error: Concurrent channel change attempted\n",
						     __FUNCTION__, __LINE__);
                        spin_unlock(&pvt_wifiposdata->lock);
						return -1;
					}
    				pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_WAIT;
	    		    reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_WAIT;
					pvt_wifiposdata->ccreqdata = reqdata;
					pvt_wifiposdata->cctsfreqdata = NULL;

		            OS_SET_TIMER(&pvt_wifiposdata->wifipos_cleanup_timer, WAIT_EXPIRE_TIME_MRQST);
                    // Output: Update ccreqdata
                    // Output: None
				} else {
                    // NEXT REQ STATE
				    reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC;
				    //Output : Xmit Probe
				    wifiposdata->xmitprobe(vap, reqdata);

                }
			}
            // Output:
		    ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_MREQ, __LINE__, (void *)reqdata);

			break;
		case IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC:
            if (reqdata->oc_channel == ic->ic_curchan->ic_ieee) {
                // NEXT REQ STATE
                reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC;
                //Output : Xmit Probe
                wifiposdata->xmitprobe(vap, reqdata);
                break;
            }
printk("%s: %d: OC->WAIT\n", __FUNCTION__, __LINE__);
        case IEEE80211_WIFIPOSMGR_STATE_WAIT:
printk("%s: %d: WAIT\n", __FUNCTION__, __LINE__);
            if ((reqdata->mode & SYNCHRONIZE)
                    && ieee80211_wifipos_syncpsenabled(vap, reqdata)) {
                ieee80211_wifipos_schedsyncps(vap, reqdata);
            } else {
            // NEXT REQ STATE
			    reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_WAIT;
            }
            // Output: None
		    ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_MREQ, __LINE__, (void *)reqdata);
			break;

		default:
			break;
		}
		break;
	case IEEE80211_WIFIPOS_EVENT_TSFRQST:
		tsfreqdata = (ieee80211_wifipos_tsfreqdata_t *) data;
#if IEEE80211_WIFIPOSDEBUG
        WIFIPOS_DPRINTK("%s[%d]: Tsf_ReqId: %d EVENT: %d, GSTATE: %d \n", __func__, __LINE__,
                tsfreqdata->request_id,
                IEEE80211_WIFIPOS_EVENT_TSFRQST,
                pvt_wifiposdata->state);
#endif
        // Debug data
		switch (pvt_wifiposdata->state) {
		case IEEE80211_WIFIPOSMGR_STATE_IDLE:
			if (tsfreqdata->oc_channel != ic->ic_curchan->ic_ieee) {
				if ((pvt_wifiposdata->ccreqdata) || (pvt_wifiposdata->cctsfreqdata)) {
					WIFIPOS_DPRINTK("%s : %d: Error: Concurrent channel change attempted\n",
					        __FUNCTION__, __LINE__);
                    pvt_wifiposdata->ccreqdata = NULL;
                    pvt_wifiposdata->cctsfreqdata = NULL;
                    spin_unlock(&pvt_wifiposdata->lock);
					return -1;
				}
                // NEXT GLOBAL STATE and NEXT REQ STATE
				pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC;
			    tsfreqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC;
                // Output: cctsfdata upodate
				pvt_wifiposdata->cctsfreqdata = tsfreqdata;
				pvt_wifiposdata->ccreqdata = NULL;
			} else {
                // NEXT GLOBAL STATE and NEXT REQ STATE
				pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_RUNNING_HC;
    			tsfreqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC;
            }
            // Output: Xmit tsfrequest
			wifiposdata->xmittsfrequest(vap, tsfreqdata);
            break;

		case IEEE80211_WIFIPOSMGR_STATE_RUNNING_HC:

			if (tsfreqdata->oc_channel != ic->ic_curchan->ic_ieee) {
				if ((pvt_wifiposdata->ccreqdata) || (pvt_wifiposdata->cctsfreqdata)) {
				    printk("%s : %d: Error: Concurrent channel change attempted\n",
					        __FUNCTION__, __LINE__);
                    spin_unlock(&pvt_wifiposdata->lock);
					return -1;
				}
                // NEXT GLOBAL STATE and NEXT REQ STATE
				pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_WAIT;
			    tsfreqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_WAIT;
		        OS_SET_TIMER(&pvt_wifiposdata->wifipos_cleanup_timer, WAIT_EXPIRE_TIME_TSFRQST);
                // Outpur: Update cctsfreqdata
				pvt_wifiposdata->cctsfreqdata = tsfreqdata;
				pvt_wifiposdata->ccreqdata = NULL;
			} else {
                // NEXT REQ STATE
			    tsfreqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC;
                // Output: Xmit probe
			    wifiposdata->xmittsfrequest(vap, tsfreqdata);

            }
		break;

		case IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC:
        case IEEE80211_WIFIPOSMGR_STATE_WAIT:
            // NEXT REQ STATE
			tsfreqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_WAIT;
			break;
		default:
			break;
		}
		break;
	case IEEE80211_WIFIPOS_EVENT_SYNCPS:
		reqdata = (ieee80211_wifipos_reqdata_t *) data;
#if IEEE80211_WIFIPOSDEBUG
        WIFIPOS_DPRINTK("%s[%d]: ReqId: %d EVENT: %d, GSTATE: %d \n", __func__, __LINE__,
                reqdata->request_id, IEEE80211_WIFIPOS_EVENT_SYNCPS, pvt_wifiposdata->state);
#endif
		switch (pvt_wifiposdata->state) {
        // Scheduling algorithm prioritizes Sync PS.
		case IEEE80211_WIFIPOSMGR_STATE_IDLE:
		case IEEE80211_WIFIPOSMGR_STATE_RUNNING_HC:
		case IEEE80211_WIFIPOSMGR_STATE_WAIT:

            if (IEEE80211_ADDR_IS_VALID(reqdata->sta_mac_addr)
                    && reqdata->oc_channel != ic->ic_curchan->ic_ieee) {
				if ((pvt_wifiposdata->ccreqdata) || (pvt_wifiposdata->cctsfreqdata)) {
				    printk ("%s : %d: Error: Concurrent channel change attempted\n",
					     __FUNCTION__, __LINE__);
                    spin_unlock(&pvt_wifiposdata->lock);
					return -1;
				}
                // we need to update all the request in HC to be aborted/ finished and
                // then start new request
                ieee80211_wifipos_update_allreqdata_state(pvt_wifiposdata,
                        IEEE80211_WIFIPOS_RQSTSTATE_IDLE);
			    reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC;
				pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC;
				pvt_wifiposdata->ccreqdata = reqdata;
				pvt_wifiposdata->cctsfreqdata = NULL;
			} else {
			    reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC;
				pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_RUNNING_HC;
            }
			ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_SYNCPS, __LINE__,
			        (void *)reqdata);
			wifiposdata->xmitprobe(vap, reqdata);
			break;
		case IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC:
            // Output: Resched sync PS request
            // TBD: Resched syncPS, OR Send empty response
            ieee80211_wifipos_clear_reqdata(reqdata);
            break;
		default:
			break;
		}
		break;
    case IEEE80211_WIFIPOS_EVENT_FINRQST:
        // Note: Caller of this event has to update the reqdata->state = IDLE
        // Debug data _
#if IEEE80211_WIFIPOSDEBUG
        WIFIPOS_DPRINTK("%s[%d]: EVENT: %d, GSTATE: %d \n", __func__, __LINE__,
                IEEE80211_WIFIPOS_EVENT_FINRQST, pvt_wifiposdata->state);
#endif
        switch (pvt_wifiposdata->state) {
		case IEEE80211_WIFIPOSMGR_STATE_IDLE:
			//Invalid state event
			printk("%s : %d: Error: Invalid event %d on state %d\n", __FUNCTION__, __LINE__,
			        event, pvt_wifiposdata->state);
			break;
        case IEEE80211_WIFIPOSMGR_STATE_RUNNING_HC:
            pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_IDLE;
            break;
		case IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC:
            pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_IDLE;

            if (pvt_wifiposdata->cctsfreqdata != NULL ) {
printk("%s : %d: OC->HC due to CC timer for Treq\n",__FUNCTION__, __LINE__);
                // Debug data
                pvt_wifiposdata->cctsfreqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_IDLE;
                ieee80211_wifipos_resume_hc_traffic (vap,
                        pvt_wifiposdata->cctsfreqdata->hc_channel,
                        NULL);
            } else if(pvt_wifiposdata->ccreqdata!= NULL){
printk("%s : %d: OC->HC due to CC timer for Mreq\n",__FUNCTION__, __LINE__);
                // Debug data
                //TBD: tasklet is called in resume_hc(). Check if this is a problem when called in yimer conetxt becuase
                //update_wifipos_stats may be called. Shoudle we move processing of frames
                //to a later time?
                pvt_wifiposdata->num_mrqst_inprogress--;
                if(pvt_wifiposdata->num_mrqst_inprogress > 0)
                    printk("%s[%d] Error: RTC with non 0 req \n", __func__, __LINE__);

                pvt_wifiposdata->ccreqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_IDLE;
                ieee80211_wifipos_resume_hc_traffic (vap,
                        pvt_wifiposdata->ccreqdata->hc_channel,
                        (struct ieee80211_node *) pvt_wifiposdata->ccreqdata->ni_tmp_sta);
            } else {
                printk("%s[%d]: Error: No CCreq data or tsfReqdata \n", __func__, __LINE__);
            }
            // Output and Action
            pvt_wifiposdata->cctsfreqdata = NULL;
            pvt_wifiposdata->ccreqdata = NULL;
            schedule_work(&pvt_wifiposdata->resumeprobes_work); //Resume probes in work queue
            break;
		case IEEE80211_WIFIPOSMGR_STATE_WAIT:
            // This state is called because we were waiting for previous request to
            // finish
            //Output: xmit tsf or xmit probe
            if (pvt_wifiposdata->cctsfreqdata != NULL){
                // Debug data
                pvt_wifiposdata->cctsfreqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC;
                pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC;
                wifiposdata->xmittsfrequest(vap, pvt_wifiposdata->cctsfreqdata);
            } else if (pvt_wifiposdata->ccreqdata != NULL) {
                // Debug data
                pvt_wifiposdata->ccreqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC;
                pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC;
                wifiposdata->xmitprobe(vap, pvt_wifiposdata->ccreqdata);
            } else {
                printk("%s[%d]: EVENT: %d, GSTATE: %d, has NO cctsfreq or ccreq \n",
                        __func__, __LINE__, event,
                        pvt_wifiposdata->state);
            // Next Global STATE
                pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_IDLE;
            }
            break;
        }
        break;
	default:
		break;
	}
	spin_unlock(&pvt_wifiposdata->lock);
	return 0;
}
/* Function     : ieee80211_ol_wifipos_fsm
 * Arguments    : Event, data, idle
 * Functionality: Finite state machine for wifiposmgr.
 *                Change wifiposmgrstate and reqdata state, based on current state and event
	            //Finite State Machine for wifipos manager state and reqdata state
            	//The following events cause a state change:
            	// (i) Measurement request with channel change
            	// (ii) TSF request with channel change
            	// (ii) Return from channel change (RTC)
            	// (ii) Sync PS timer expired

            	//The FSM function can be called from several different contexts:
            	//(i) User process context: Requests from wpc
            	//(ii) Bottom half: rx/tx completion via update_wifipos_stats)
            	//(iii) Timer: sync ps, channel change
            	//(iv) Work queue: Resuming probes/tsf after channel change
 * Return       : Void
 */
int ieee80211_ol_wifipos_fsm(struct ieee80211vap *vap, int event, void *data, int idle)
{

    struct ieee80211com *ic = vap->iv_ic;
    int ret;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata;
    ieee80211_wifipos_reqdata_t *reqdata;

    if ( NULL == vap->iv_wifipos ) {
        return ( -1 );
    }
    pvt_wifiposdata=(ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
    spin_lock(&pvt_wifiposdata->lock);
    // TBD: Scheduling Policy doesnot gaurantee fairness. if request comes in IDLE state.
	switch (event) {
	case IEEE80211_WIFIPOS_EVENT_MRQST:
		reqdata = (ieee80211_wifipos_reqdata_t *) data;

        printk("\n THE reqID: %d. THE DRIVER STATE: %d. THE request STATE: %d.\n",reqdata->request_id, pvt_wifiposdata->state, reqdata->state);
#if IEEE80211_WIFIPOSDEBUG
        WIFIPOS_DPRINTK("%s[%d]: ReqId: %x EVENT: %d, GSTATE: %d T: %x \n", __func__, __LINE__,
                reqdata->request_id,
                IEEE80211_WIFIPOS_EVENT_MRQST,
                pvt_wifiposdata->state,
                ic->ic_get_TSF32(ic));
#endif
        if (reqdata->oc_channel < 20) {
            if (!((ic->ic_curchan->ic_ieee < 20) && (reqdata->hc_channel < 20)))
                printk("\n mrqst on wrong channel\n");
        }
        else if (reqdata->oc_channel >20) {
            if (!((ic->ic_curchan->ic_ieee > 20) && (reqdata->hc_channel > 20)))
                printk("\n mrqst on wrong channel\n");
        }

		switch (pvt_wifiposdata->state) {
		case IEEE80211_WIFIPOSMGR_STATE_IDLE:
			//Set reqdata state based on current wifposmgr state and reqdata info
			//TBD: What happens to probes to associated STAs if sync ps is set: TSFREQ fails, so are they timed?
			//Need to handle this
            if(pvt_wifiposdata->num_mrqst_inprogress > 0){

                printk("\n resetting the state of the reqdata, which can cause problem\n");
                ieee80211_wifipos_update_allreqdata_state(pvt_wifiposdata,
                        IEEE80211_WIFIPOS_RQSTSTATE_IDLE);
            }

            if (IEEE80211_ADDR_IS_VALID(reqdata->sta_mac_addr)
                    && reqdata->oc_channel != ic->ic_curchan->ic_ieee) {
                if ((pvt_wifiposdata->ccreqdata) || (pvt_wifiposdata->cctsfreqdata))
                {
                    printk("%s : %d: Error: Concurrent channel change attempted\n",
                            __FUNCTION__, __LINE__);
                    spin_unlock(&pvt_wifiposdata->lock);
                    return -1;
                }
                // NEXT GLOBAL STATE and NEXT REQ STATE
                pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC;
                reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC;
                // update CCreq data
                } else {
                // NEXT GLOBAL STATE and NEXT REQ STATE
                pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_RUNNING_HC;
                reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC;
                }
                //Output: Xmit probe
                ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_MREQ, __LINE__,(void *)reqdata);
                ret = vap->iv_wifipos->xmitprobe(vap, reqdata);
                if(ret)
                    pvt_wifiposdata->num_mrqst_inprogress++;
            break;
		case IEEE80211_WIFIPOSMGR_STATE_RUNNING_HC:
			//Set reqdata state based on current wifposmgr state and reqdata info
			//TBD: What happens to probes to associated STAs if sync ps is set: TSFREQ fails, so are they timed?
			//Need to handle this
            //Check for wifiposmgr statechange
				if (reqdata->oc_channel != ic->ic_curchan->ic_ieee) {
				    if ((pvt_wifiposdata->ccreqdata) || (pvt_wifiposdata->cctsfreqdata)) {
						WIFIPOS_DPRINTK("%s : %d: Error: Concurrent channel change attempted\n",
						     __FUNCTION__, __LINE__);
                        spin_unlock(&pvt_wifiposdata->lock);
						return -1;
					}
    				pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_WAIT;
	    		    reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_WAIT;
				} else {
                    // NEXT REQ STATE
                    pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_WAIT;
				    reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_WAIT;
				    //Output : Xmit Probe
				    //wifiposdata->xmitprobe(vap, reqdata);
                }
                ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_MREQ, __LINE__, (void *)reqdata);
            break;
		case IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC:
            //Set reqdata state based on current wifposmgr state and reqdata info
            //TBD: What happens to probes to associated STAs if sync ps is set: TSFREQ fails, so are they timed?
            //Need to handle this
            //Check for wifiposmgr statechange
            if (reqdata->oc_channel != ic->ic_curchan->ic_ieee) {
                if ((pvt_wifiposdata->ccreqdata) || (pvt_wifiposdata->cctsfreqdata)) {
                    WIFIPOS_DPRINTK("%s : %d: Error: Concurrent channel change attempted\n",
                                                     __FUNCTION__, __LINE__);
                     spin_unlock(&pvt_wifiposdata->lock);
                     return -1;
                 }
            pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_WAIT;
            reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_WAIT;
            } else {
                // NEXT REQ STATE
                pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_WAIT;
                reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_WAIT;
                //Output : Xmit Probe
                //wifiposdata->xmitprobe(vap, reqdata);

            }
            // Output:
            ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_MREQ, __LINE__, (void *)reqdata);
            break;
        case IEEE80211_WIFIPOSMGR_STATE_WAIT:

            // NEXT REQ STATE
            reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_WAIT;
            // Output: None
		    ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_MREQ, __LINE__, (void *)reqdata);
            if (idle != 0) {
                ret = vap->iv_wifipos->xmitprobe(vap, reqdata);
                if(ret) {
                    pvt_wifiposdata->num_mrqst_inprogress++;
                    }
              }
               //If stuck in WAIT state deadlock, cleanup old requests
                printk("%s:, Clearing request state machine to overcome WAIT stuck \n", __FUNCTION__);
                ieee80211_wifipos_update_allreqdata_state(pvt_wifiposdata, IEEE80211_WIFIPOS_RQSTSTATE_IDLE);
                pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_IDLE;
                vap->iv_wifipos->xmitprobe(vap, reqdata);

            break;
		default:
			break;
    }
    break;
    case IEEE80211_WIFIPOS_EVENT_FINRQST:
        // Note: Caller of this event has to update the reqdata->state = IDLE
        // Debug data _
#if IEEE80211_WIFIPOSDEBUG
        WIFIPOS_DPRINTK("%s[%d]: EVENT: %d, GSTATE: %d \n", __func__, __LINE__,
                IEEE80211_WIFIPOS_EVENT_FINRQST, pvt_wifiposdata->state);
#endif

        switch (pvt_wifiposdata->state) {
		case IEEE80211_WIFIPOSMGR_STATE_IDLE:
			//Invalid state event
			printk("%s : %d: Error: Invalid event %d on state %d\n", __FUNCTION__, __LINE__,
			        event, pvt_wifiposdata->state);
			break;
        case IEEE80211_WIFIPOSMGR_STATE_RUNNING_HC:
            if(pvt_wifiposdata->num_mrqst_inprogress > 0){
                pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_IDLE;
                schedule_work(&pvt_wifiposdata->resumeprobes_work);
            }//Resume probes in work queue
            else{
                pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_IDLE;
            }

            break;
		case IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC:
            pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_IDLE;

                //TBD: tasklet is called in resume_hc(). Check if this is a problem when called in yimer conetxt becuase
                //update_wifipos_stats may be called. Shoudle we move processing of frames
                //to a later time?
                //pvt_wifiposdata->num_mrqst_inprogress--;
            if(pvt_wifiposdata->num_mrqst_inprogress > 0)
                printk("%s[%d] Error: RTC with non 0 req \n", __func__, __LINE__);
            // Output and Action
            // schedule_work(&pvt_wifiposdata->resumeprobes_work); //Resume probes in work queue
            break;
		case IEEE80211_WIFIPOSMGR_STATE_WAIT:
            // This state is called because we were waiting for previous request to
            // finish
            //Output: xmit tsf or xmit probe
            // Output and Action
            if(pvt_wifiposdata->num_mrqst_inprogress > 0){
                printk("\n ******************* scheduling resume-probes********************* \n");
                pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_IDLE;
                schedule_work(&pvt_wifiposdata->resumeprobes_work);

                }//Resume probes in work queue
            else{
                pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_IDLE;
                }
            break;
        }
        break;
	default:
		break;
	}
	spin_unlock(&pvt_wifiposdata->lock);
	return 0;
}


int ieee80211_ftmrr_action_frm (struct ieee80211vap *vap, u_int8_t *data )
{
    struct ieee80211_node *ni;
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    int error = EOK;
    struct neighbor_report_arr *ftmrr = (struct neighbor_report_arr *)data;
    struct ieee80211_ftmrrreq *actionbuf;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "Preparing to send  ftmrr action frame...\n");

    ni = ieee80211_find_txnode(vap, &ftmrr->sta_mac[0]);
    if (ni == NULL) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT, "%s: could not send mgmt found for %s\n",
                          __func__, ether_sprintf(&ftmrr->sta_mac[0]));
         IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION,"Unable to send FTMRR, could not find STA node, not associated??\n");
        return -EINVAL;
    }

    /* Send FTMRR to STA only if it's enabled for RRM and FTMRR, else return */
    if(!(ni->ni_is_rrm_ftmrr)) {
         IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "Not sending FTMRR, STA rrm: %d, ftmrr: %d\n", ieee80211_node_is_rrm(vap,&ftmrr->sta_mac[0]), ni->ni_is_rrm_ftmrr);
        return -EINVAL;
    }
    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
    if (wbuf == NULL) {
        ieee80211_free_node(ni);
        return -ENOMEM;
    }

    actionbuf = (struct ieee80211_ftmrrreq *)frm;
    actionbuf->header.ia_category = IEEE80211_ACTION_CAT_RADIO;
    actionbuf->header.ia_action = IEEE80211_ACTION_MEAS_REQUEST;
    actionbuf->dialogtoken = ftmrr->dialogtoken;
    actionbuf->num_repetitions = htole16(ftmrr->num_repetitions);
    frm = &actionbuf->elem[0];

    OS_MEMCPY(frm, &(ftmrr->id), ftmrr->len+2);
    frm += ftmrr->len+2;
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    /* If Managment Frame protection is enabled (PMF or Cisco CCX), set Privacy bit */
    if ((vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_is_mfp &&
        vap->iv_ccx_evtable->wlan_ccx_is_mfp(vap->iv_ifp)) ||
        ((ieee80211_vap_mfp_test_is_set(vap) ||
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
              (wlan_crypto_is_pmf_enabled(vap->vdev_obj, ni->peer_obj))))) {
#else
              ieee80211_is_pmf_enabled(vap, ni)) && ni->ni_ucastkey.wk_valid)){
#endif
        /* MFP is enabled, so we need to set Privacy bit */
        struct ieee80211_frame *wh;
        wh = (struct ieee80211_frame*)wbuf_header(wbuf);
        wh->i_fc[1] |= IEEE80211_FC1_WEP;
    }
    error = ieee80211_send_mgmt(vap, ni, wbuf,false);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "FTMRR successfully sent\n");

    ieee80211_free_node(ni);    /* reclaim node */
    return error;

}

/* Send Where are you action frame */
void ieee80211_whereareyou_action_frm (struct ieee80211vap *vap, u_int8_t *data )
{
    struct ieee80211_node *ni;
    wbuf_t wbuf = NULL;
    u_int8_t *frm = NULL;
    struct meas_req_lci_request *wru = (struct meas_req_lci_request *)data;
    struct ieee80211_ftmrrreq *actionbuf;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "Preparing Where Are You frame for %s...\n", ether_sprintf(&wru->sta_mac[0]));

    ni = ieee80211_find_txnode(vap, &wru->sta_mac[0]);
    if (ni == NULL) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_OUTPUT, "%s: could not send frame for %s\n",
                          __func__, ether_sprintf(&wru->sta_mac[0]));
         IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION,"Unable to send Where Are You frame, STA not associated.\n");
        return;
    }

    wbuf = ieee80211_getmgtframe(ni, IEEE80211_FC0_SUBTYPE_ACTION, &frm, 0);
    if (wbuf == NULL) {
        ieee80211_free_node(ni);
        return;
    }
    /* Populating Maxage optional subIE, which always goes out with LCI request frame */
    wru->maxage_subie = IEEE80211_MAXAGE_SUBIE;
    wru->maxage_len = IEEE80211_MAXAGE_LEN;
    wru->maxage_val = IEEE80211_MAXAGE_VALUE_DEFAULT; //default value
    wru->len = wru->len + IEEE80211_MAXAGE_IE_SIZE; //total size of IE

    actionbuf = (struct ieee80211_ftmrrreq *)frm;
    actionbuf->header.ia_category = IEEE80211_ACTION_CAT_RADIO;
    actionbuf->header.ia_action = IEEE80211_ACTION_MEAS_REQUEST;
    actionbuf->dialogtoken = wru->dialogtoken;
    actionbuf->num_repetitions = htole16(wru->num_repetitions);
    frm = &actionbuf->elem[0];

    OS_MEMCPY(frm, &(wru->id), wru->len+2);
    frm += wru->len+2;
    wbuf_set_pktlen(wbuf, (frm - (u_int8_t*)wbuf_header(wbuf)));
    /* If Managment Frame protection is enabled (PMF or Cisco CCX), set Privacy bit */
    if ((vap->iv_ccx_evtable && vap->iv_ccx_evtable->wlan_ccx_is_mfp &&
        vap->iv_ccx_evtable->wlan_ccx_is_mfp(vap->iv_ifp)) ||
        ((ieee80211_vap_mfp_test_is_set(vap) ||
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
              (wlan_crypto_is_pmf_enabled(vap->vdev_obj, ni->peer_obj))))) {
#else
              ieee80211_is_pmf_enabled(vap, ni)) && ni->ni_ucastkey.wk_valid)){
#endif
        /* MFP is enabled, so we need to set Privacy bit */
        struct ieee80211_frame *wh;
        wh = (struct ieee80211_frame*)wbuf_header(wbuf);
        wh->i_fc[1] |= IEEE80211_FC1_WEP;
    }
    ieee80211_send_mgmt(vap, ni, wbuf, false);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_ACTION, "Where Are you frame (with MaxAge IE) successfully sent for %s\n", ether_sprintf(&wru->sta_mac[0]));
    ieee80211_free_node(ni);    /* reclaim node */
    return;
}


/* Function     : ieee80211_wifpos_processnspmsg
 * Arguments    : msg from the nl socket
 * Functionality: Process the message received from wpc, and call appropriate functionality
 * Return       : Void
 */
void ieee80211_wifpos_processnspmsg(wbuf_t msg)
{
    struct ieee80211com *ic;
	struct ieee80211vap *vap;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata;
	ieee80211_wifipos_reqdata_t *reqdata;
	ieee80211_wifipos_tsfreqdata_t *tsfdata;
	void *nlh;
	struct nsp_header *nsp_hdr;
	struct nsp_mrqst *mrqst = NULL;
	struct nsp_sreq *srqst;
	struct nsp_cap_req *crqst;
	struct nsp_wakeup_req *wrqst;
	struct nsp_sleep_req *slrqst;
	struct nsp_tsf_req *tsfrqst;
	char *buf;
    unsigned int vap_idx;
	int error = -1;
    void *lcid;


	nlh = wbuf_raw_data(msg);
    nsp_hdr = (struct nsp_header *)OS_NLMSG_DATA(nlh);

    //vap_idx = nsp_hdr->vap_idx;
    //suppose only one vap,
    vap_idx = 0;
    if (nsp_hdr->frame_type == NSP_MRQST) {
        buf = (char *)nsp_hdr;
        mrqst = (struct nsp_mrqst *)(buf + NSP_HDR_LEN);
        vap = ieee80211_wifipos_find_vap(vap_idx, mrqst);
        if (vap == NULL) {
            WIFIPOS_DPRINTK("WifiPositioning:vap_idx(%d) invalid \n", vap_idx);
            printk("%s: %d: vap not found\n", __FUNCTION__, __LINE__);
            return;
        }
    }
    else {
        vap = ieee80211_wifipos_find_vap(vap_idx, mrqst);
        if (vap == NULL) {
            WIFIPOS_DPRINTK("WifiPositioning:vap_idx(%d) invalid \n", vap_idx);
            printk("%s: %d: vap not found\n", __FUNCTION__, __LINE__);
            return;
        }
    }

    if ( NULL == vap->iv_wifipos )
    {
        return;
    }
    ic = vap->iv_ic;
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
  	if (pvt_wifiposdata == NULL) {
		WIFIPOS_DPRINTK("WifiPositioning:vap->iv_wifipos is null\n");
		return;
	}
	pvt_wifiposdata->process_pid = ((struct nlmsghdr *)nlh)->nlmsg_pid;	/*pid of sending process */

	buf = (char *)nsp_hdr;

	ieee80211_print_nsphdr(vap, nsp_hdr);
	if (nsp_hdr->frame_type == NSP_WAKEUPRQST) {
    printk("%s: %d: NSP_WAKEUPRQST\n", __FUNCTION__, __LINE__);
		wrqst = (struct nsp_wakeup_req *)(buf + NSP_HDR_LEN);
		ieee80211_print_wrqst(vap, wrqst);
		if (vap->iv_wifipos->wakeup_request(vap, wrqst) != 0) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			        "WifiPositioning:Error in wakeup_request\n");
		}

	} else if (nsp_hdr->frame_type == NSP_SRQST) {
printk("%s: %d: NSP_SRQST\n", __FUNCTION__, __LINE__);
		srqst = (struct nsp_sreq *)(buf + NSP_HDR_LEN);
		ieee80211_print_srqst(vap, srqst);
		if (vap->iv_wifipos->status_request(vap, srqst) != 0) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
					  "WifiPositioning:Error in status request\n");
		}

	} else if (nsp_hdr->frame_type == NSP_CRQST) {
printk("%s: %d: NSP_CRQST\n", __FUNCTION__, __LINE__);
		crqst = (struct nsp_cap_req *)(buf + NSP_HDR_LEN);
		ieee80211_print_crqst(vap, crqst);
		if (vap->iv_wifipos->cap_request(vap, crqst) != 0) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
					  "WifiPositioning:Error in Capability Request\n");
		}

	} else if (nsp_hdr->frame_type == NSP_TSFRQST) {
printk("%s: %d: NSP_TSFRQST\n", __FUNCTION__, __LINE__);
		tsfrqst = (struct nsp_tsf_req *)(buf + NSP_HDR_LEN);
		ieee80211_print_tsfrqst(vap, tsfrqst);
		tsfdata = ieee80211_wifipos_createtsfrqstdata(vap, tsfrqst);
		if (tsfdata)
			error = vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_TSFRQST, (void *)tsfdata, 0);
		if (error)
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
					  "WifiPositioning:Error in tsf_request\n");

	} else if (nsp_hdr->frame_type == NSP_MRQST) {
printk("%s: %d: NSP_MRQST\n", __FUNCTION__, __LINE__);
		mrqst = (struct nsp_mrqst *)(buf + NSP_HDR_LEN);
		ieee80211_print_mrqst(vap, mrqst);
	    spin_lock(&pvt_wifiposdata->tsf_req_lock);
		reqdata = ieee80211_wifipos_createreqdata(vap, mrqst);
	    spin_unlock(&pvt_wifiposdata->tsf_req_lock);
		if (reqdata)
			error = vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_MRQST, (void *)reqdata, 0);
		if (error)
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			        "WifiPositioning:Error in message request\n");

	} else if (nsp_hdr->frame_type == NSP_STARETURNTOSLEEPREQ) {
printk("%s: %d: NSP_STARETURNTOSLEEPREQ\n", __FUNCTION__, __LINE__);
		slrqst = (struct nsp_sleep_req *)(buf + NSP_HDR_LEN);
		ieee80211_print_slrqst(vap, slrqst);
		if (vap->iv_wifipos->sleep_request(vap, slrqst) != 0) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
					  "WifiPositioning:Error in sleep_request\n");
		}

    } else if (nsp_hdr->frame_type == NSP_FTMRR) {
        struct neighbor_report_arr *nr = (struct neighbor_report_arr *) (buf + NSP_HDR_LEN);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "%s: %d: NSP_FTMRR\n", __FUNCTION__, __LINE__);
        ieee80211_ftmrr_action_frm(vap, (u_int8_t *)nr);
    } else if (nsp_hdr->frame_type == NSP_WHEREAREYOU) {
        struct meas_req_lci_request *wru = (struct meas_req_lci_request *) (buf + NSP_HDR_LEN);
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "%s: %d: NSP_WHEREAREYOU\n", __FUNCTION__, __LINE__);
        ieee80211_whereareyou_action_frm(vap, (u_int8_t *)wru);
    } else if (nsp_hdr->frame_type == NSP_WPC_LCI) {
        printk(KERN_DEBUG "%s: %d: NSP_WPC_LCI\n", __FUNCTION__, __LINE__);
        lcid = (void *)(buf + NSP_HDR_LEN);
        if (vap->iv_wifipos->lci_set && vap->iv_wifipos->lci_set(vap, lcid) != 0) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
                "WifiPositioning:Error in lci set\n");
        }

    } else if (nsp_hdr->frame_type == NSP_WPC_LCR) {
        printk(KERN_DEBUG "%s: %d: NSP_WPC_LCR\n", __FUNCTION__, __LINE__);
        lcid = (void *)(buf + NSP_HDR_LEN);
        if (vap->iv_wifipos->lcr_set && vap->iv_wifipos->lcr_set(vap, lcid) != 0) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
                    "WifiPositioning:Error in lcr set\n");
        }
    }
}

/* Function     : ieee80211_wifipos_nlsend_tsf_update
 * Arguments    : mac address of the AP to which we did tsf probing
 * Functionality: To update the tsf information for requested AP
 * Return       : Void
 */
void ieee80211_wifipos_nlsend_tsf_update(
                            struct ieee80211vap *vap,
                            u_int8_t * source_mac_addr)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
    ieee80211_wifipos_tsfreqdata_t *tsfreqdata;
    int i;
    uint16_t sanity_inprogress_cnt = 0;

    if ( NULL == vap->iv_wifipos ) {
    return ;
    }
    pvt_wifiposdata =  (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
	vap = TAILQ_FIRST(&ic->ic_vaps);
	if (vap == NULL) {
		WIFIPOS_DPRINTK("vap is null\n");
		return;
	}

	spin_lock(&pvt_wifiposdata->lock);
	spin_lock(&pvt_wifiposdata->tsf_req_lock);
	for (i = 0; i < ATH_WIFIPOS_MAX_CONCURRENT_TSF_AP; i++) {
        if (IEEE80211_ADDR_EQ(pvt_wifiposdata->tsfreq[i].assoc_ap_mac_addr,
                source_mac_addr)) {
			tsfreqdata = &pvt_wifiposdata->tsfreq[i];
			pvt_wifiposdata->tsfreq[i].probe_ap_tsf = vap->iv_local_tsf_tstamp;
			pvt_wifiposdata->tsfreq[i].assoc_ap_tsf = vap->iv_tsf_sync;
			pvt_wifiposdata->tsfreq[i].valid = 1;
			ieee80211_wifipos_sendtsfresp(vap, tsfreqdata);
            tsfreqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_IDLE;

            sanity_inprogress_cnt = ieee80211_wifipos_chk_num_req_inprogress(pvt_wifiposdata);
            if(0 == sanity_inprogress_cnt
                    && (tsfreqdata->hc_channel == tsfreqdata->oc_channel)){
                OS_CANCEL_TIMER(&pvt_wifiposdata->wifipos_cleanup_timer);
        	    vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL, 0);

            } else if((tsfreqdata->hc_channel != tsfreqdata->oc_channel)
                    && tsfreqdata != IEEE80211_WIFIPOS_RQSTSTATE_IDLE){
                OS_CANCEL_TIMER (&pvt_wifiposdata->wifipos_cctimer);
        	    vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL, 0);
            }
		}
	}
	spin_unlock(&pvt_wifiposdata->tsf_req_lock);
	spin_unlock(&pvt_wifiposdata->lock);
}

/* Function     : ieee80211_wifipos_nlsend_status_resp
 * Arguments    : status request
 * Functionality: Send status message
 * Return       : Void
 */
void ieee80211_wifipos_nlsend_status_resp(
                            struct ieee80211vap *vap,
                            struct nsp_sreq *srqst)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata;
	struct ieee80211vap *nextvap, *tmpvap;
    wbuf_t nlwbuf;
	void *nlh;
	struct nsp_header nsp_hdr;
	struct nsp_sresp sresp;
	struct nsp_vap_info *vapinfo;
	struct nsp_station_info *stainfo;
	char *buf;
	ieee80211_wifipos_stabuff_t stabuf;
	ieee80211_ssid ssidlist[1];
	u_int8_t no_of_vaps = 0, no_of_stas = 0, cnt = 0;
	u_int16_t i = 0;

        printk("%s: %d: \n", __FUNCTION__, __LINE__);
        if ( NULL == vap->iv_wifipos ) {
            return;
        }
        pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
	//wifiposdata->sta_cnt = 0;
	memset(&sresp, 0, sizeof(sresp));
	nsp_hdr.SFD = START_OF_FRAME;
	nsp_hdr.version = NSP_VERSION;
	nsp_hdr.frame_type = NSP_SRESP;

	sresp.request_id = srqst->request_id;
	sresp.req_tstamp = ic->ic_get_TSF32(ic);

	TAILQ_FOREACH(tmpvap, &ic->ic_vaps, iv_next) {
		no_of_vaps++;
		no_of_stas += wlan_iterate_station_list(tmpvap, NULL, NULL);
	}

	nsp_hdr.frame_length =
            SRES_LEN + (no_of_vaps * VAPINFO_LEN) + (no_of_stas * SINFO_LEN);

	sresp.no_of_vaps = no_of_vaps;
	ieee80211_print_nsphdr(vap, &nsp_hdr);
	//TBD
	//wifiposdata->sta_cnt = no_of_stas;

	sresp.no_of_stations = no_of_stas;
	buf = OS_MALLOC(ic->ic_osdev,
	        NSP_HDR_LEN + SRES_LEN + (no_of_vaps * VAPINFO_LEN) + (no_of_stas * SINFO_LEN),
	        GFP_KERNEL);
	if (buf == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:buf allocation failure");
		return;
	}
	memcpy(buf, &nsp_hdr, NSP_HDR_LEN);
	vapinfo = (struct nsp_vap_info *)(((char *)buf) + NSP_HDR_LEN + SRES_LEN);
	stainfo = (struct nsp_station_info *)(
            ((char *)buf) + NSP_HDR_LEN + SRES_LEN + (no_of_vaps * VAPINFO_LEN));

	nextvap = vap;
	cnt = 0;
	i = 0;
	while (nextvap) {
		stabuf.node_count = 0;
		stabuf.stainfo = stainfo + cnt;
		stabuf.vap_index = i;
		cnt += wlan_iterate_station_list(nextvap, sta_list, &stabuf);
		IEEE80211_ADDR_COPY(vapinfo->vap_mac, nextvap->iv_myaddr);
		vapinfo->vap_channel = nextvap->iv_ic->ic_curchan->ic_ieee;
		wlan_get_bss_essid(nextvap, ssidlist);
		OS_MEMCPY(vapinfo->vap_ssid, ssidlist[0].ssid, ssidlist[0].len);
		vapinfo ++;

		nextvap = TAILQ_NEXT(nextvap, iv_next);
		i ++;
	}

	sresp.resp_tstamp = ic->ic_get_TSF32(ic);
	ieee80211_print_sresp(vap, &sresp);
	memcpy(((char *)buf + NSP_HDR_LEN), &sresp, SRES_LEN);
	nlwbuf = wbuf_alloc(ic->ic_osdev,
            WBUF_MAX_TYPE,
            OS_NLMSG_SPACE(NSP_HDR_LEN + SRES_LEN + (no_of_vaps * VAPINFO_LEN) +
			        (no_of_stas * SINFO_LEN)));
	if (nlwbuf == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:wbuf allocation failure");
		return;
	}
	wbuf_append(nlwbuf,
            OS_NLMSG_SPACE(NSP_HDR_LEN + SRES_LEN + (no_of_vaps * VAPINFO_LEN) +
			        (no_of_stas * SINFO_LEN)));

	nlh = wbuf_raw_data(nlwbuf);
	OS_SET_NETLINK_HEADER(nlh,
	        NLMSG_LENGTH(NSP_HDR_LEN + SRES_LEN + (no_of_vaps * VAPINFO_LEN) +
			        (no_of_stas * SINFO_LEN)),
			0,
			0,
			0,
		    pvt_wifiposdata->process_pid);

	wbuf_set_netlink_pid(nlwbuf, 0);
	wbuf_set_netlink_dst_group(nlwbuf, 0);
	memcpy(OS_NLMSG_DATA(nlh),
            buf,
	        OS_NLMSG_SPACE(NSP_HDR_LEN + SRES_LEN + (no_of_vaps * VAPINFO_LEN) +
			        (no_of_stas * SINFO_LEN)));
	OS_NETLINK_UCAST(pvt_wifiposdata->wifipos_sock, nlwbuf, pvt_wifiposdata->process_pid, 0);
	OS_FREE(buf);
}

/* Function     : ieee80211_wifipos_nlsend_cap_resp
 * Arguments    : capabiltiy request
 * Functionality: Send capability message
 * Return       : Void
 */
void ieee80211_wifipos_nlsend_cap_resp(
                            struct ieee80211vap *vap,
                            struct nsp_cap_req *crqst)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata;
	wbuf_t nlwbuf;
	void *nlh;
	struct nsp_header nsp_hdr;
	struct nsp_cap_resp cresp;
	char buf[OS_NLMSG_SPACE(NSP_HDR_LEN + CAPRES_LEN)];

        if ( NULL == vap->iv_wifipos ) {
            return;
        }
        pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
	memset(&cresp, 0, sizeof(cresp));
	memset(&buf, 0, sizeof(buf));
	nsp_hdr.SFD = START_OF_FRAME;
	nsp_hdr.version = NSP_VERSION;
	nsp_hdr.frame_type = NSP_CRESP;
	nsp_hdr.frame_length = CAPRES_LEN;

	ieee80211_print_nsphdr(vap, &nsp_hdr);
	memcpy(buf, &nsp_hdr, NSP_HDR_LEN);

	cresp.request_id = crqst->request_id;
	cresp.band = ATH_WIFIPOS_SINGLE_BAND;
	cresp.positioning = ATH_WIFIPOS_POSITIONING;
    cresp.curr_chain_masks = (ic->ic_tx_chainmask )|(ic->ic_rx_chainmask << 4);
	cresp.sw_version = NSP_AP_SW_VERSION;
	cresp.hw_version = NSP_HW_VERSION;
	cresp.clk_freq = 44000000;	//TBD: Extract from HAL

	ieee80211_print_cresp(vap, &cresp);

	memcpy((buf + NSP_HDR_LEN), &cresp, CAPRES_LEN);
	nlwbuf = wbuf_alloc(ic->ic_osdev,
	        WBUF_MAX_TYPE,
		    OS_NLMSG_SPACE(NSP_HDR_LEN + CAPRES_LEN));
	if (nlwbuf == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:wbuf allocation failure");
		return;
	}
	wbuf_append(nlwbuf, OS_NLMSG_SPACE(NSP_HDR_LEN + CAPRES_LEN));

	nlh = wbuf_raw_data(nlwbuf);
	OS_SET_NETLINK_HEADER(nlh,
            NLMSG_LENGTH(NSP_HDR_LEN + CAPRES_LEN),
			0,
			0,
			0,
			pvt_wifiposdata->process_pid);
	wbuf_set_netlink_pid(nlwbuf, 0);
	wbuf_set_netlink_dst_group(nlwbuf, 0);
	memcpy(OS_NLMSG_DATA(nlh),
            buf,
	        OS_NLMSG_SPACE(NSP_HDR_LEN + CAPRES_LEN));
	OS_NETLINK_UCAST(pvt_wifiposdata->wifipos_sock, nlwbuf, pvt_wifiposdata->process_pid, 0);
}

/* Function     : ieee80211_wifipos_nlsend_tsf_resp
 * Arguments    : tsf request data
 * Functionality: Send tsf response to wpc
 * Return       : Void
 */
void ieee80211_wifipos_nlsend_tsf_resp(struct ieee80211vap *vap, void *data)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
    ieee80211_wifipos_tsfreqdata_t *tsfres = (ieee80211_wifipos_tsfreqdata_t *)data;
	wbuf_t nlwbuf;
	void *nlh;
	struct nsp_header nsp_hdr;
	struct nsp_tsf_resp tsfresp = { 0 };
	char buf[OS_NLMSG_SPACE(NSP_HDR_LEN + TSFRES_LEN)];

        if ( NULL == vap->iv_wifipos ) {
            return ;
        }

        pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
	memset(&buf, 0, sizeof(buf));
	nsp_hdr.SFD = START_OF_FRAME;
	nsp_hdr.version = 1;
	nsp_hdr.frame_type = NSP_TSFRESP;
	nsp_hdr.frame_length = TSFRES_LEN;

	ieee80211_print_nsphdr(vap, &nsp_hdr);
	memcpy(buf, &nsp_hdr, NSP_HDR_LEN);

	tsfresp.request_id = tsfres->request_id;
	memcpy(tsfresp.ap_mac_addr, tsfres->assoc_ap_mac_addr, ETH_ALEN);
	tsfresp.result = 0;
	tsfresp.assoc_tsf = tsfres->assoc_ap_tsf;
	tsfresp.prob_tsf = tsfres->probe_ap_tsf;
	ieee80211_print_tsfresp(vap, &tsfresp);
	ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_TSFRESP, __LINE__,
				   (void *)&tsfresp);

	memcpy((buf + NSP_HDR_LEN), &tsfresp, TSFRES_LEN);
	nlwbuf = wbuf_alloc(ic->ic_osdev,
	        WBUF_MAX_TYPE,
		    OS_NLMSG_SPACE(NSP_HDR_LEN + TSFRES_LEN));
	if (nlwbuf == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:wbuf allocation failure");
		return;
	}
	wbuf_append(nlwbuf, OS_NLMSG_SPACE(NSP_HDR_LEN + TSFRES_LEN));

	nlh = wbuf_raw_data(nlwbuf);
	OS_SET_NETLINK_HEADER(nlh, NLMSG_LENGTH(NSP_HDR_LEN + TSFRES_LEN),
	        0,
	        0,
	        0,
	        pvt_wifiposdata->process_pid);
	wbuf_set_netlink_pid(nlwbuf, 0);
	wbuf_set_netlink_dst_group(nlwbuf, 0);
	memcpy(OS_NLMSG_DATA(nlh), buf, OS_NLMSG_SPACE(NSP_HDR_LEN + TSFRES_LEN));
	OS_NETLINK_UCAST(pvt_wifiposdata->wifipos_sock, nlwbuf, pvt_wifiposdata->process_pid, 0);
}

/* Function     : ieee80211_wifipos_nlsend_sleep_resp
 * Arguments    : sleep request
 * Functionality: Send sleep  response to wpc
 * Return       : Void
 */
#if NSP_AP_SW_VERSION > 0x01
void ieee80211_wifipos_nlsend_sleep_resp(
                            struct ieee80211vap *vap,
                            struct nsp_sleep_req *slrqst,
                            u_int16_t num_ka_frm,
                            unsigned char result)
#else
void ieee80211_wifipos_nlsend_sleep_resp(
                            struct ieee80211vap *vap,
                            struct nsp_sleep_req *slrqst,
						    unsigned char result)
#endif
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	wbuf_t nlwbuf;
	void *nlh;
	struct nsp_header nsp_hdr;
	struct nsp_sleep_resp slresp = { 0 };
	char buf[OS_NLMSG_SPACE(NSP_HDR_LEN + SLEEPRES_LEN)];

printk("%s: %d: \n", __FUNCTION__, __LINE__);
        if ( NULL == vap->iv_wifipos ) {
            return ;
        }
        pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
	memset(&buf, 0, sizeof(buf));
	nsp_hdr.SFD = START_OF_FRAME;
	nsp_hdr.version = NSP_VERSION;
	nsp_hdr.frame_type = NSP_STARETURNTOSLEEPRES;
	nsp_hdr.frame_length = SLEEPRES_LEN;

	ieee80211_print_nsphdr(vap, &nsp_hdr);
	memcpy(buf, &nsp_hdr, NSP_HDR_LEN);

	slresp.request_id = slrqst->request_id;
	memcpy(slresp.sta_mac_addr, slrqst->sta_mac_addr, ETH_ALEN);
#if NSP_AP_SW_VERSION > 0x01
    // NBP 1.1 addition
    slresp.num_ka_frm = num_ka_frm ;
#endif
	slresp.result = result;

	ieee80211_print_slresp(vap, &slresp);

	memcpy((buf + NSP_HDR_LEN), &slresp, SLEEPRES_LEN);
	nlwbuf = wbuf_alloc(ic->ic_osdev,
	        WBUF_MAX_TYPE,
		    OS_NLMSG_SPACE(NSP_HDR_LEN + SLEEPRES_LEN));
	if (nlwbuf == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:wbuf allocation failure");
		return;
	}
	wbuf_append(nlwbuf, OS_NLMSG_SPACE(NSP_HDR_LEN + SLEEPRES_LEN));

	nlh = wbuf_raw_data(nlwbuf);
	OS_SET_NETLINK_HEADER(nlh, NLMSG_LENGTH(NSP_HDR_LEN + SLEEPRES_LEN),
	        0,
	        0,
	        0,
	        pvt_wifiposdata->process_pid);
	wbuf_set_netlink_pid(nlwbuf, 0);
	wbuf_set_netlink_dst_group(nlwbuf, 0);
	memcpy(OS_NLMSG_DATA(nlh), buf, OS_NLMSG_SPACE(NSP_HDR_LEN + SLEEPRES_LEN));
	OS_NETLINK_UCAST(pvt_wifiposdata->wifipos_sock, nlwbuf, pvt_wifiposdata->process_pid, 0);
}

/* Function     : ieee80211_wifipos_nlsend_wakeup_resp
 * Arguments    : wakeup request
 * Functionality: Send wakeup  response to wpc
 * Return       : Void
 */
void ieee80211_wifipos_nlsend_wakeup_resp(
                            struct ieee80211vap *vap,
                            struct nsp_wakeup_req *wrqst,
						    unsigned char result)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	wbuf_t nlwbuf;
	void *nlh;
	struct nsp_header nsp_hdr;
	struct nsp_wakeup_resp wresp = { 0 };
	char buf[OS_NLMSG_SPACE(NSP_HDR_LEN + WAKEUPRES_LEN)];

        printk("%s: %d:\n", __FUNCTION__, __LINE__);
        if ( NULL == vap->iv_wifipos ) {
            return;
        }
        pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
	memset(&buf, 0, sizeof(buf));
	nsp_hdr.SFD = START_OF_FRAME;
	nsp_hdr.version = NSP_VERSION;
	nsp_hdr.frame_type = NSP_WAKEUPRESP;
	nsp_hdr.frame_length = WAKEUPRES_LEN;

	ieee80211_print_nsphdr(vap, &nsp_hdr);
	memcpy(buf, &nsp_hdr, NSP_HDR_LEN);

	wresp.request_id = wrqst->request_id;
	memcpy(wresp.sta_mac_addr, wrqst->sta_mac_addr, ETH_ALEN);
	wresp.result = result;

	ieee80211_print_wresp(vap, &wresp);

	memcpy((buf + NSP_HDR_LEN), &wresp, WAKEUPRES_LEN);
	nlwbuf = wbuf_alloc(ic->ic_osdev,
	        WBUF_MAX_TYPE,
		    OS_NLMSG_SPACE(NSP_HDR_LEN + WAKEUPRES_LEN));
	if (nlwbuf == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:wbuf allocation failure");
		return;
	}
	wbuf_append(nlwbuf, OS_NLMSG_SPACE(NSP_HDR_LEN + WAKEUPRES_LEN));

	nlh = wbuf_raw_data(nlwbuf);
	OS_SET_NETLINK_HEADER(nlh,
            NLMSG_LENGTH(NSP_HDR_LEN + WAKEUPRES_LEN),
			0,
			0,
			0,
			pvt_wifiposdata->process_pid);
	wbuf_set_netlink_pid(nlwbuf, 0);
	wbuf_set_netlink_dst_group(nlwbuf, 0);
	memcpy(OS_NLMSG_DATA(nlh), buf, OS_NLMSG_SPACE(NSP_HDR_LEN + WAKEUPRES_LEN));
	OS_NETLINK_UCAST(pvt_wifiposdata->wifipos_sock, nlwbuf, pvt_wifiposdata->process_pid, 0);
}
/*
    QUIPS-342
    Function        : ieee80211_wifipos_nlsend_empty_resp
    Arguments       : ieee80211_wifiposdesc_t
    Functionality   : Send the empty response to app to track the failure of tx frame
    Return          : Void
*/

void ieee80211_wifipos_nlsend_empty_resp(
                            struct ieee80211vap *vap,
                            ieee80211_wifiposdesc_t *wifiposdesc)
{
#define REQUEST_THROTTLE 2
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
    wbuf_t nlwbuf;
    void *nlh;
    struct nsp_header nsp_hdr;
    struct nsp_mresp mresp = { 0 };
    struct nsp_type1_resp chanest_resp = { 0 };
    char buf[OS_NLMSG_SPACE(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN)];
/*
    unsigned int type1_payld_size;
    struct ieee80211_node *ni;
*/
	int count = 0;
    ieee80211_wifipos_reqdata_t *reqdata = NULL;

//printk("%s: %d: \n", __FUNCTION__, __LINE__);
    if ( NULL == vap->iv_wifipos ) {
        return ;
    }
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
    memset(&buf, 0, sizeof(buf));

    nsp_hdr.SFD = START_OF_FRAME;
    nsp_hdr.version = NSP_VERSION;
    nsp_hdr.frame_type = NSP_TYPE1RESP;
    nsp_hdr.frame_length = MRES_LEN + TYPE1RES_LEN;

    ieee80211_print_nsphdr(vap, &nsp_hdr);
    memcpy(buf, &nsp_hdr, NSP_HDR_LEN);
    mresp.request_id = wifiposdesc->request_id;

	/* Search for request entry in request data store */
	for (count = 0; count < ATH_WIFIPOS_MAX_CONCURRENT_REQUESTS; count++) {
		reqdata = &(pvt_wifiposdata->reqdata[count]);
		if (reqdata->request_id == wifiposdesc->request_id &&
            reqdata->state != IEEE80211_WIFIPOS_RQSTSTATE_IDLE &&
            IEEE80211_ADDR_EQ(reqdata->sta_mac_addr,wifiposdesc->sta_mac_addr))
			break;
	}
	if (reqdata == NULL)
		return;
	if(reqdata->num_probe_rqst > 0)
		reqdata->num_probe_rqst--;

    memcpy(mresp.sta_mac_addr, wifiposdesc->sta_mac_addr, ETH_ALEN);
    mresp.no_of_responses = 1;
    mresp.req_tstamp = reqdata->req_tstamp;
    mresp.resp_tstamp = ic->ic_get_TSF32(ic);
    mresp.result = REQUEST_THROTTLE;

    ieee80211_print_mresp(vap, &mresp);

    chanest_resp.toa = 0;
    chanest_resp.tod = wifiposdesc->tod;
    chanest_resp.send_rate = reqdata->rateset >> 24;
    chanest_resp.receive_rate = 0x0;
    chanest_resp.rssi[0] = 0;
    chanest_resp.rssi[1] = 0;
    chanest_resp.rssi[2] = 0;
    chanest_resp.no_of_chains = 0;;
    chanest_resp.no_of_retries = wifiposdesc->retries;
/*
	type1_payld_size = ATH_DESC_CDUMP_SIZE(numRxChains);
	memcpy(chanest_resp.type1_payld, wifipos_entry->type1_payld,
	       type1_payld_size);
	ieee80211_print_type1_payld_resp(vap, &chanest_resp);
*/
    /* Calling FSM to finish This has to be called where all we are decreasing the probe
       request count */
    if (0 == reqdata->num_probe_rqst){
        if(reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC ) {
              if(pvt_wifiposdata->num_mrqst_inprogress >0){
                  pvt_wifiposdata->num_mrqst_inprogress--;
              } else
                  printk("%s[%d]: Error ReqId: %d!!!!!!\n",
                            __func__, __LINE__, reqdata->request_id);

              reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_IDLE;
              if(0 == pvt_wifiposdata->num_mrqst_inprogress){
    		        OS_CANCEL_TIMER(&pvt_wifiposdata->wifipos_cleanup_timer);
   		            vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL, 0);
              }
        } else if (reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC ) {
            OS_CANCEL_TIMER (&pvt_wifiposdata->wifipos_cctimer);
	        vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL, 0);
        }
    }


	memcpy((buf + NSP_HDR_LEN), &mresp, MRES_LEN);
	memcpy((buf + NSP_HDR_LEN + MRES_LEN), &chanest_resp, TYPE1RES_LEN);

	nlwbuf = wbuf_alloc(ic->ic_osdev,
	        WBUF_MAX_TYPE,
		    OS_NLMSG_SPACE(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN));
	if (nlwbuf == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:wbuf allocation failure");
		return;
	}
	wbuf_append(nlwbuf, OS_NLMSG_SPACE(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN));

	ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_MRESP, __LINE__, (void *)&mresp);

	//Reqdata no longer needed, clear the reqdata structure members
	if (0 == reqdata->num_probe_rqst){
       ieee80211_wifipos_clear_reqdata(reqdata);
    }
	nlh = wbuf_raw_data(nlwbuf);
	OS_SET_NETLINK_HEADER(nlh,
	        NLMSG_LENGTH(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN),
	        0,
	        0,
	        0,
            pvt_wifiposdata->process_pid);
	wbuf_set_netlink_pid(nlwbuf, 0);
	wbuf_set_netlink_dst_group(nlwbuf, 0);
	memcpy(OS_NLMSG_DATA(nlh), buf, OS_NLMSG_SPACE(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN));

//printk("%s: %d: \n", __FUNCTION__, __LINE__);
	OS_NETLINK_UCAST(pvt_wifiposdata->wifipos_sock, nlwbuf, pvt_wifiposdata->process_pid, 0);
	//netlink_unicast(pvt_wifiposdata->wifipos_sock, nlwbuf, pvt_wifiposdata->process_pid, 0);

}


/* Function     : ieee80211_wifipos_nlsend_probe_resp
 * Arguments    : probe data
 * Functionality: Send probe response to wpc
 * Return       : Void
 */
void ieee80211_ol_wifipos_nlsend_probe_resp(
                            struct ieee80211vap *vap,
                            ieee80211_wifipos_probedata_t *wifipos_entry)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;

	wbuf_t nlwbuf;
	void *nlh;
	struct nsp_header nsp_hdr;
	struct nsp_mresp mresp = { 0 };
	struct nsp_type1_resp chanest_resp = { 0 };
	char buf[OS_NLMSG_SPACE(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN)];
	unsigned int type1_payld_size;
	struct ieee80211_node *ni;
	ieee80211_wifipos_reqdata_t *reqdata = NULL;
	int count = 0, ret = -1;
    u_int8_t numRxChains = ieee80211_mask2numchains[
                    (wifipos_entry->no_of_chains & (RX_CHAINMASK_HASH >>8))];

        if ( NULL == vap->iv_wifipos ) {
            return ;
        }

        pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
        if(NULL == pvt_wifiposdata->wifipos_sock)
            return;
	memset(&buf, 0, sizeof(buf));
	nsp_hdr.SFD = START_OF_FRAME;
	nsp_hdr.version = NSP_VERSION;
	nsp_hdr.frame_type = NSP_TYPE1RESP;
	nsp_hdr.frame_length = MRES_LEN + TYPE1RES_LEN;

	ieee80211_print_nsphdr(vap, &nsp_hdr);
	memcpy(buf, &nsp_hdr, NSP_HDR_LEN);

	mresp.request_id = wifipos_entry->request_id;
	/* Search for request entry in request data store */
	for (count = 0; count < ATH_WIFIPOS_MAX_CONCURRENT_REQUESTS; count++) {
		reqdata = &(pvt_wifiposdata->reqdata[count]);
		if (reqdata->request_id == wifipos_entry->request_id &&
            reqdata->state != IEEE80211_WIFIPOS_RQSTSTATE_IDLE &&
            IEEE80211_ADDR_EQ(reqdata->sta_mac_addr,wifipos_entry->sta_mac_addr)) {
			break;
            }
	}
	if (reqdata->request_id != wifipos_entry->request_id){
        printk("%s[%d]:Error  RI: 0x%x, TOD: %Lu, TOA: %Lu, SERVER-reqid: 0x%x count: %d \n", __func__, __LINE__,
                wifipos_entry->request_id, wifipos_entry->tod, wifipos_entry->toa,reqdata->request_id, count);
		return;
    }
    if(reqdata->num_probe_rqst > 0){
        reqdata->num_probe_rqst--;
    }
	memcpy(mresp.sta_mac_addr, wifipos_entry->sta_mac_addr, ETH_ALEN);
	mresp.no_of_responses = 1;
	mresp.req_tstamp = reqdata->req_tstamp;
	mresp.resp_tstamp = ic->ic_get_TSF32(ic);
	ni = ieee80211_find_node(&ic->ic_sta, reqdata->sta_mac_addr);
	if (ni != NULL) {
		ieee80211_free_node(ni);
	}

	ieee80211_print_mresp(vap, &mresp);
	chanest_resp.toa = wifipos_entry->toa;
	chanest_resp.tod = wifipos_entry->tod;
	chanest_resp.send_rate = reqdata->rateset >> 24;
	chanest_resp.receive_rate = wifipos_entry->rate;
	chanest_resp.rssi[0] = wifipos_entry->rssi[0];
	chanest_resp.rssi[1] = wifipos_entry->rssi[1];
	chanest_resp.rssi[2] = wifipos_entry->rssi[2];
	chanest_resp.no_of_chains = wifipos_entry->no_of_chains;
	chanest_resp.no_of_retries = wifipos_entry->no_of_retries;

	type1_payld_size = ATH_DESC_CDUMP_SIZE(numRxChains);
	memcpy(chanest_resp.type1_payld, wifipos_entry->type1_payld, 324);
	ieee80211_print_type1_payld_resp(vap, &chanest_resp);

#if IEEE80211_WIFIPOSDEBUG
    WIFIPOS_DPRINTK(KERN_DEBUG"%s]%d]: [%x] MRQT: %llx, MRPT: %llx , RRT: %llx, TOD: %llx, TOA: %llx\n", __func__, __LINE__,
             reqdata->request_id, mresp.req_tstamp,mresp.resp_tstamp, reqdata->req_tstamp,chanest_resp.tod,chanest_resp.toa);
#endif
    /* Calling FSM to finish This has to be called where all we are decreasing the probe
       request count */
    if (0 == reqdata->num_probe_rqst){
        pvt_wifiposdata->num_mrqst_inprogress--;
        if(reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC ) {
                reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_IDLE;
                spin_unlock(&pvt_wifiposdata->lock);
                if(0 == pvt_wifiposdata->num_mrqst_inprogress){

   		            vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL, 0);
                }
                spin_lock(&pvt_wifiposdata->lock);
        } else if (reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC ) {
            spin_unlock(&pvt_wifiposdata->lock);
            if (pvt_wifiposdata->num_mrqst_inprogress == 0) {
	        vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL, 0);
            } else {
                printk("\n %s[%d]: ERROR:this can never happen\n", __func__, __LINE__);
	    }
            spin_lock(&pvt_wifiposdata->lock);
        } else if (reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_WAIT) {

                spin_unlock(&pvt_wifiposdata->lock);
                vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL, 0);
                spin_lock(&pvt_wifiposdata->lock);

        }
    }
	memcpy((buf + NSP_HDR_LEN), &mresp, MRES_LEN);
	memcpy((buf + NSP_HDR_LEN + MRES_LEN), &chanest_resp, TYPE1RES_LEN);

	nlwbuf = wbuf_alloc(ic->ic_osdev,
            WBUF_MAX_TYPE,
            OS_NLMSG_SPACE(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN));

	if (nlwbuf == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:wbuf allocation failure");
		return;
	}
	wbuf_append(nlwbuf, OS_NLMSG_SPACE(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN));

	ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_MRESP, __LINE__, (void *)&mresp);

	//Reqdata no longer needed, clear the reqdata structure members
	if (0 == reqdata->num_probe_rqst){
       ieee80211_wifipos_clear_reqdata(reqdata);
    }
	nlh = wbuf_raw_data(nlwbuf);
	OS_SET_NETLINK_HEADER(nlh,
	        NLMSG_LENGTH(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN),
	        0,
	        0,
	        0,
            pvt_wifiposdata->process_pid);
	wbuf_set_netlink_pid(nlwbuf, 0);
	wbuf_set_netlink_dst_group(nlwbuf, 0);
	memcpy(OS_NLMSG_DATA(nlh), buf, OS_NLMSG_SPACE(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN));
	//OS_NETLINK_UCAST(pvt_wifiposdata->wifipos_sock, nlwbuf, pvt_wifiposdata->process_pid, 0);
    ret = netlink_unicast(pvt_wifiposdata->wifipos_sock, nlwbuf, pvt_wifiposdata->process_pid, 0);
}

/* Function     : ieee80211_wifipos_nlsend_probe_resp
 * Arguments    : probe data
 * Functionality: Send probe response to wpc
 * Return       : Void
 */
void ieee80211_wifipos_nlsend_probe_resp(
                            struct ieee80211vap *vap,
                            ieee80211_wifipos_probedata_t *wifipos_entry)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	wbuf_t nlwbuf;
	void *nlh;
	struct nsp_header nsp_hdr;
	struct nsp_mresp mresp = { 0 };
	struct nsp_type1_resp chanest_resp = { 0 };
	char buf[OS_NLMSG_SPACE(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN)];
	unsigned int type1_payld_size;
	struct ieee80211_node *ni;
	ieee80211_wifipos_reqdata_t *reqdata = NULL;
	int count = 0;
    u_int8_t numRxChains = ieee80211_mask2numchains[
                    (wifipos_entry->no_of_chains & (RX_CHAINMASK_HASH >>8))];

    printk("%s: %d: \n", __FUNCTION__, __LINE__);
        if ( NULL == vap->iv_wifipos ) {
            return;
        }
        pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
        if(NULL == pvt_wifiposdata->wifipos_sock)
            return;
	memset(&buf, 0, sizeof(buf));
	nsp_hdr.SFD = START_OF_FRAME;
	nsp_hdr.version = NSP_VERSION;
	nsp_hdr.frame_type = NSP_TYPE1RESP;
	nsp_hdr.frame_length = MRES_LEN + TYPE1RES_LEN;

	ieee80211_print_nsphdr(vap, &nsp_hdr);
	memcpy(buf, &nsp_hdr, NSP_HDR_LEN);

	mresp.request_id = wifipos_entry->request_id;

	/* Search for request entry in request data store */
	for (count = 0; count < ATH_WIFIPOS_MAX_CONCURRENT_REQUESTS; count++) {
		reqdata = &(pvt_wifiposdata->reqdata[count]);
		if (reqdata->request_id == wifipos_entry->request_id &&
            reqdata->state != IEEE80211_WIFIPOS_RQSTSTATE_IDLE &&
            IEEE80211_ADDR_EQ(reqdata->sta_mac_addr,wifipos_entry->sta_mac_addr))
			break;
	}
	if (reqdata->request_id != wifipos_entry->request_id){
        printk(KERN_DEBUG"%s[%d]:Error  RI: 0x%x, TOD: %Lu, TOA: %Lu \n", __func__, __LINE__,
                wifipos_entry->request_id, wifipos_entry->tod, wifipos_entry->toa);
		return;
    }

    if(reqdata->num_probe_rqst > 0)
        reqdata->num_probe_rqst--;

    memcpy(mresp.sta_mac_addr, wifipos_entry->sta_mac_addr, ETH_ALEN);
    mresp.no_of_responses = 1;
    mresp.req_tstamp = reqdata->req_tstamp;
    mresp.resp_tstamp = ic->ic_get_TSF32(ic);
    ni = ieee80211_find_node(&ic->ic_sta, reqdata->sta_mac_addr);
    if (ni != NULL) {
	ieee80211_free_node(ni);
    }

    ieee80211_print_mresp(vap, &mresp);

    chanest_resp.toa = wifipos_entry->toa;
    chanest_resp.tod = wifipos_entry->tod;
    chanest_resp.send_rate = reqdata->rateset >> 24;
    chanest_resp.receive_rate = wifipos_entry->rate;
    chanest_resp.rssi[0] = wifipos_entry->rssi[0];
    chanest_resp.rssi[1] = wifipos_entry->rssi[1];
    chanest_resp.rssi[2] = wifipos_entry->rssi[2];
    chanest_resp.no_of_chains = wifipos_entry->no_of_chains;
    chanest_resp.no_of_retries = wifipos_entry->no_of_retries;

    type1_payld_size = ATH_DESC_CDUMP_SIZE(numRxChains);
    memcpy(chanest_resp.type1_payld, wifipos_entry->type1_payld, type1_payld_size);
    ieee80211_print_type1_payld_resp(vap, &chanest_resp);
#if IEEE80211_WIFIPOSDEBUG
    WIFIPOS_DPRINTK(KERN_DEBUG"%s]%d]: [%x] MRQT: %llx, MRPT: %llx , RRT: %llx, TOD: %llx, TOA: %llx\n", __func__, __LINE__,
             reqdata->request_id, mresp.req_tstamp,mresp.resp_tstamp, reqdata->req_tstamp,chanest_resp.tod,chanest_resp.toa);
#endif
    /* Calling FSM to finish This has to be called where all we are decreasing the probe
       request count */
    if (0 == reqdata->num_probe_rqst){
        if(reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC ) {
            if(pvt_wifiposdata->num_mrqst_inprogress >0){
                pvt_wifiposdata->num_mrqst_inprogress--;
            } else
                printk("%s[%d]: Error ReqId: %d!!!!!!\n", __func__, __LINE__,
                        reqdata->request_id);

            reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_IDLE;
            if (0 == pvt_wifiposdata->num_mrqst_inprogress) {
                OS_CANCEL_TIMER(&pvt_wifiposdata->wifipos_cleanup_timer);
                vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL,0);
            }
        } else if (reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC ) {
            OS_CANCEL_TIMER (&pvt_wifiposdata->wifipos_cctimer);
	        vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL,0);
        }
    }



	memcpy((buf + NSP_HDR_LEN), &mresp, MRES_LEN);
	memcpy((buf + NSP_HDR_LEN + MRES_LEN), &chanest_resp, TYPE1RES_LEN);

	nlwbuf = wbuf_alloc(ic->ic_osdev,
            WBUF_MAX_TYPE,
            OS_NLMSG_SPACE(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN));

	if (nlwbuf == NULL) {
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:wbuf allocation failure");
		return;
	}
	wbuf_append(nlwbuf, OS_NLMSG_SPACE(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN));

	ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_MRESP, __LINE__, (void *)&mresp);

	//Reqdata no longer needed, clear the reqdata structure members
	if (0 == reqdata->num_probe_rqst){
       ieee80211_wifipos_clear_reqdata(reqdata);
    }
	nlh = wbuf_raw_data(nlwbuf);
	OS_SET_NETLINK_HEADER(nlh,
	        NLMSG_LENGTH(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN),
	        0,
	        0,
	        0,
            pvt_wifiposdata->process_pid);
	wbuf_set_netlink_pid(nlwbuf, 0);
	wbuf_set_netlink_dst_group(nlwbuf, 0);
	memcpy(OS_NLMSG_DATA(nlh), buf, OS_NLMSG_SPACE(NSP_HDR_LEN + MRES_LEN + TYPE1RES_LEN));
	OS_NETLINK_UCAST(pvt_wifiposdata->wifipos_sock, nlwbuf, pvt_wifiposdata->process_pid, 0);
}

int ieee80211_wifipos_status_request(
                                struct ieee80211vap *vap,
                                struct nsp_sreq *srqst)
{
    ieee80211_wifipos_data_t *wifiposdata =vap->iv_wifipos;

printk("%s: %d: \n", __FUNCTION__, __LINE__);
        if ( NULL == vap->iv_wifipos ) {
            return ( -1 );
        }
	wifiposdata->nlsend_status_resp(vap, srqst);
	return 0;
}

int ieee80211_wifipos_cap_request(
                            struct ieee80211vap *vap,
                            struct nsp_cap_req *crqst)
{
    ieee80211_wifipos_data_t *wifiposdata = vap->iv_wifipos;

printk("%s: %d: \n", __FUNCTION__, __LINE__);
    if ( NULL == vap->iv_wifipos ) {
        return ( -1 );
    }
	wifiposdata->nlsend_cap_resp(vap, crqst);
	return 0;
}

int ieee80211_ol_wifipos_sleep_request(
                            struct ieee80211vap *vap,
                            struct nsp_sleep_req *slrqst)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	struct ieee80211_node *ni = NULL;
	int i, result = 0;
	unsigned char s[IEEE80211_ADDR_LEN];
	unsigned char sleep_resp_sent = 0;
	memset(s, 0, IEEE80211_ADDR_LEN);

    printk("%s: %d: \n", __FUNCTION__, __LINE__);
        if ( NULL == vap->iv_wifipos ) {
            return ( -1 );
        }
        pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
	//TBD: What is this used for?
	if (IEEE80211_ADDR_EQ(slrqst->sta_mac_addr, s)) {
		sleep_resp_sent = 1;
#if NSP_AP_SW_VERSION > 0x01
		ieee80211_wifipos_nlsend_sleep_resp(vap, slrqst, 0, result);
#else
		ieee80211_wifipos_nlsend_sleep_resp(vap, slrqst, result);
#endif

	} else {
		for (i = 0; i < ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA; i++) {
			spin_lock(&pvt_wifiposdata->wakeup_lock);
			if (IEEE80211_ADDR_EQ(pvt_wifiposdata->wakeup[i].sta_mac_addr,
			        slrqst->sta_mac_addr)) {
			    ni = ieee80211_find_node(&ic->ic_sta,
                        pvt_wifiposdata->wakeup[i].sta_mac_addr);
				if (ni == NULL) {
					result = 0x01;
					spin_unlock(&pvt_wifiposdata->wakeup_lock);
					sleep_resp_sent = 1;
#if NSP_AP_SW_VERSION > 0x01
#if IEEE80211_WIFIPOSDEBUG
                    WIFIPOS_DPRINTK("%s[%d]: STA_DIS[%d]: %s, num_ka_frm: %x \n", __func__, __LINE__,
                                pvt_wifiposdata->wakeup[i].request_id,
                                ether_sprintf(pvt_wifiposdata->wakeup[i].sta_mac_addr),
                                pvt_wifiposdata->wakeup[i].num_ka_frm);
#endif
					vap->iv_wifipos->nlsend_sleep_resp(vap,
					        slrqst,
					        pvt_wifiposdata->wakeup[i].num_ka_frm, result);
#else
					vap->iv_wifipos->nlsend_sleep_resp(vap, slrqst, result);
#endif
                //Reqdata no longer needed, clear the reqdata structure members
				    memset(&pvt_wifiposdata->wakeup[i], 0, sizeof(ieee80211_wifipos_wakeup_t));
				    memset(&pvt_wifiposdata->wakeup[i].sta_mac_addr, 0, IEEE80211_ADDR_LEN);

				    -- pvt_wifiposdata->wakeup_sta_count;

					return -1;
				}
				ieee80211node_clear_flag(ni,IEEE80211_NODE_WAKEUP);

#if NSP_AP_SW_VERSION > 0x01
					vap->iv_wifipos->nlsend_sleep_resp(vap,
					        slrqst,
					        pvt_wifiposdata->wakeup[i].num_ka_frm,
					        result);
#else
					vap->iv_wifipos->nlsend_sleep_resp(vap, slrqst, result);
#endif
		//Reqdata no longer needed, clear the reqdata structure members
				memset(&pvt_wifiposdata->wakeup[i], 0, sizeof(ieee80211_wifipos_wakeup_t));
				memset(&pvt_wifiposdata->wakeup[i].sta_mac_addr, 0, IEEE80211_ADDR_LEN);

				-- pvt_wifiposdata->wakeup_sta_count;
				sleep_resp_sent = 1;

			}
			spin_unlock(&pvt_wifiposdata->wakeup_lock);
		}
		if (!sleep_resp_sent) {
#if NSP_AP_SW_VERSION > 0x01
		vap->iv_wifipos->nlsend_sleep_resp(vap, slrqst, 0, result);
#else
		vap->iv_wifipos->nlsend_sleep_resp(vap, slrqst, 0, result);
#endif
        }
		if (pvt_wifiposdata->wakeup_sta_count == 0) {
			printk("\n The sta count is 0\n");
			vap->iv_wakeup = 0;
			if(ni == NULL)
			printk("\n The node is not present\n");
			else
			vap->iv_wifipos->ol_wakeup_request(vap, ni, TRUE);
		}
        if(ni)
            ieee80211_free_node(ni);
	}
	return 0;
}


int ieee80211_wifipos_sleep_request(
                            struct ieee80211vap *vap,
                            struct nsp_sleep_req *slrqst)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	struct ieee80211_node *ni;
	int i, result = 0;
	unsigned char s[IEEE80211_ADDR_LEN];
	unsigned char sleep_resp_sent = 0;
	memset(s, 0, IEEE80211_ADDR_LEN);

printk("%s: %d: \n", __FUNCTION__, __LINE__);
        if ( NULL == vap->iv_wifipos ) {
            return ( -1 );
        }
        pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
	//TBD: What is this used for?
	if (IEEE80211_ADDR_EQ(slrqst->sta_mac_addr, s)) {
		sleep_resp_sent = 1;
#if NSP_AP_SW_VERSION > 0x01
		ieee80211_wifipos_nlsend_sleep_resp(vap, slrqst, 0, result);
#else
		ieee80211_wifipos_nlsend_sleep_resp(vap, slrqst, result);
#endif

	} else {
		for (i = 0; i < ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA; i++) {
			spin_lock(&pvt_wifiposdata->wakeup_lock);
			if (IEEE80211_ADDR_EQ(pvt_wifiposdata->wakeup[i].sta_mac_addr,
			        slrqst->sta_mac_addr)) {
			    ni = ieee80211_find_node(&ic->ic_sta,
                        pvt_wifiposdata->wakeup[i].sta_mac_addr);
				if (ni == NULL) {
					result = 0x01;
					spin_unlock(&pvt_wifiposdata->wakeup_lock);
					sleep_resp_sent = 1;
#if NSP_AP_SW_VERSION > 0x01
#if IEEE80211_WIFIPOSDEBUG
                    WIFIPOS_DPRINTK("%s[%d]: STA_DIS[%d]: %s, num_ka_frm: %x \n", __func__, __LINE__,
                                pvt_wifiposdata->wakeup[i].request_id,
                                ether_sprintf(pvt_wifiposdata->wakeup[i].sta_mac_addr),
                                pvt_wifiposdata->wakeup[i].num_ka_frm);
#endif
					vap->iv_wifipos->nlsend_sleep_resp(vap,
					        slrqst,
					        pvt_wifiposdata->wakeup[i].num_ka_frm, result);
#else
					vap->iv_wifipos->nlsend_sleep_resp(vap, slrqst, result);
#endif
                //Reqdata no longer needed, clear the reqdata structure members
				    memset(&pvt_wifiposdata->wakeup[i], 0, sizeof(ieee80211_wifipos_wakeup_t));
				    memset(&pvt_wifiposdata->wakeup[i].sta_mac_addr, 0, IEEE80211_ADDR_LEN);

				    -- pvt_wifiposdata->wakeup_sta_count;

					return -1;
				}
				ieee80211node_clear_flag(ni,IEEE80211_NODE_WAKEUP);

#if NSP_AP_SW_VERSION > 0x01
					vap->iv_wifipos->nlsend_sleep_resp(vap,
					        slrqst,
					        pvt_wifiposdata->wakeup[i].num_ka_frm,
					        result);
#else
					vap->iv_wifipos->nlsend_sleep_resp(vap, slrqst, result);
#endif
		//Reqdata no longer needed, clear the reqdata structure members
				memset(&pvt_wifiposdata->wakeup[i], 0, sizeof(ieee80211_wifipos_wakeup_t));
				memset(&pvt_wifiposdata->wakeup[i].sta_mac_addr, 0, IEEE80211_ADDR_LEN);

				-- pvt_wifiposdata->wakeup_sta_count;
				sleep_resp_sent = 1;

                if(ni)
				    ieee80211_free_node(ni);
			}
			spin_unlock(&pvt_wifiposdata->wakeup_lock);
		}
		if (!sleep_resp_sent) {
#if NSP_AP_SW_VERSION > 0x01
		vap->iv_wifipos->nlsend_sleep_resp(vap, slrqst, 0, result);
#else
		vap->iv_wifipos->nlsend_sleep_resp(vap, slrqst, 0, result);
#endif
        }
		if (pvt_wifiposdata->wakeup_sta_count == 0) {
			vap->iv_wakeup = 0;
			OS_CANCEL_TIMER(&pvt_wifiposdata->wifipos_wakeup_timer);
		}
	}
	return 0;
}

int ieee80211_ol_wifipos_wakeup_request(
                            struct ieee80211vap *vap,
                            struct nsp_wakeup_req *wrqst)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_data_t *wifiposdata= vap->iv_wifipos;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	struct ieee80211_node *ni;
	int i, skip = 0;
	int result = 0;

    if ( NULL == vap->iv_wifipos ) {
        return ( -1 );
    }
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
    printk("%s: %d: request_id(%d) mode(%x) wakeup_interval(%d)\n", __FUNCTION__, __LINE__,
        wrqst->request_id,
        wrqst->mode,
        wrqst->wakeup_interval
        );

    printk("%s: %d: sta_mac_addr(%02x:%02x:%02x:%02x:%02x:%02x)\n", __FUNCTION__, __LINE__,
        wrqst->sta_mac_addr[0],
        wrqst->sta_mac_addr[1],
        wrqst->sta_mac_addr[2],
        wrqst->sta_mac_addr[3],
        wrqst->sta_mac_addr[4],
        wrqst->sta_mac_addr[5]
        );

	spin_lock(&pvt_wifiposdata->wakeup_lock);
	for (i = 0; i < ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA; i++) {
		if (IEEE80211_ADDR_EQ(pvt_wifiposdata->wakeup[i].sta_mac_addr,
                wrqst->sta_mac_addr)) {
            printk("%s: %d: wakeup exists\n", __FUNCTION__, __LINE__);
            skip = 1;
            pvt_wifiposdata->wakeup[i].request_id = wrqst->request_id;
			pvt_wifiposdata->wakeup[i].timestamp =
			        CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
			break;
		} else {
			continue;
		}
	}

	ni = ieee80211_find_node(&ic->ic_sta, wrqst->sta_mac_addr);
	if ((ni == NULL)
            || (IEEE80211_AID(ni->ni_associd) == 0)
            || (ni->ni_vap != vap)) {
        printk("%s: %d: ni Not found\n", __FUNCTION__, __LINE__);
		result = 0x01;
		spin_unlock(&pvt_wifiposdata->wakeup_lock);
		vap->iv_wifipos->nlsend_wakeup_resp(vap, wrqst, result);
		return -1;
	}
	// removing it here and placing it at update_ka_done,
    // Once we get the response then only we should be sending the
    // wakeup response. Let server wait until we are confirmed of the
    // waking up of station.
    if(skip) {
        wifiposdata->nlsend_wakeup_resp(vap, wrqst, result);
#if IEEE80211_WIFIPOSDEBUG
        WIFIPOS_DPRINTK("FND_WKRQST[%d]: %x:%x:%x", wrqst->request_id,wrqst->sta_mac_addr[3], wrqst->sta_mac_addr[4],wrqst->sta_mac_addr[5]);
#endif
    }

	vap->iv_wakeup = wrqst->mode;

	if (skip == 0) {
		ieee80211_wifipos_wakeup_t *wakeup =
		    &pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex];

		if (wakeup->wakeup_interval != 0) {
            // Ideally we should never come to this code
			//this reqdata is already being used, buffer must have wrapped around
            // Since the incoming wakeup rqst and sleep rqst are almost serial, we can safetly
            // assume FIFO scenario, we can replace this uncleaned buffer.
			printk("%s: %d: Insufficient space in circular buffer. Reduce incoming request frequency \n",
			     __FUNCTION__, __LINE__);
            // clear this data and replace it will new.
            // TBD: Do we need to do the check for validity of this sta for connectivity and
            // wakeup request?
#if IEEE80211_WIFIPOSDEBUG
            WIFIPOS_DPRINTK("DeletingR[%x],STA: %x:%x:%x  RQT: %x, KA: %x, wkStaCnt: %d \n",
                            pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].request_id,
                            pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].sta_mac_addr[3],
                            pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].sta_mac_addr[4],
                            pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].sta_mac_addr[5],
                            pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].timestamp,
                            pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].num_ka_frm,
                            pvt_wifiposdata->wakeup_sta_count);
#endif
            memset(&pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].sta_mac_addr, 0,IEEE80211_ADDR_LEN);
			pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].timestamp = 0;
			-- pvt_wifiposdata->wakeup_sta_count;
		}
		memcpy(pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].sta_mac_addr,
                wrqst->sta_mac_addr,
                ETH_ALEN);
		pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].wakeup_interval = wrqst->wakeup_interval;
		pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].request_id= wrqst->request_id;
	    pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].timestamp =
		        CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

        pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].flags |= IEEE80211_KA_DONE;
		++ pvt_wifiposdata->wakeup_sta_count;
		pvt_wifiposdata->wakeupindex++;
		if (pvt_wifiposdata->wakeupindex >= ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA) {
			pvt_wifiposdata->wakeupindex = 0;
		}
	}
	spin_unlock(&pvt_wifiposdata->wakeup_lock);

	vap->iv_wifipos->ol_wakeup_request(vap, ni, FALSE);
	//OS_SET_TIMER(&pvt_wifiposdata->wifipos_wakeup_timer, 0);
    if (ni)
        ieee80211_free_node(ni);
    return 0;
}

int ieee80211_wifipos_wakeup_request(
                            struct ieee80211vap *vap,
                            struct nsp_wakeup_req *wrqst)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_data_t *wifiposdata= vap->iv_wifipos;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	struct ieee80211_node *ni;
	int i, skip = 0;
	int result = 0;

    if ( NULL == vap->iv_wifipos ) {
        return ( -1 );
    }
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
printk("%s: %d: request_id(%d) mode(%x) wakeup_interval(%d)\n", __FUNCTION__, __LINE__,
    wrqst->request_id,
    wrqst->mode,
    wrqst->wakeup_interval
    );

printk("%s: %d: sta_mac_addr(%02x:%02x:%02x:%02x:%02x:%02x)\n", __FUNCTION__, __LINE__,
    wrqst->sta_mac_addr[0],
    wrqst->sta_mac_addr[1],
    wrqst->sta_mac_addr[2],
    wrqst->sta_mac_addr[3],
    wrqst->sta_mac_addr[4],
    wrqst->sta_mac_addr[5]
    );

	spin_lock(&pvt_wifiposdata->wakeup_lock);
	for (i = 0; i < ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA; i++) {
		if (IEEE80211_ADDR_EQ(pvt_wifiposdata->wakeup[i].sta_mac_addr,
                wrqst->sta_mac_addr)) {
printk("%s: %d: wakeup exists\n", __FUNCTION__, __LINE__);
            skip = 1;
            pvt_wifiposdata->wakeup[i].request_id = wrqst->request_id;
			pvt_wifiposdata->wakeup[i].timestamp =
			        CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
			break;
		} else {
			continue;
		}
	}

	ni = ieee80211_find_node(&ic->ic_sta, wrqst->sta_mac_addr);
	if ((ni == NULL)
            || (IEEE80211_AID(ni->ni_associd) == 0)
            || (ni->ni_vap != vap)) {
printk("%s: %d: ni Not found\n", __FUNCTION__, __LINE__);
		result = 0x01;
		spin_unlock(&pvt_wifiposdata->wakeup_lock);
		vap->iv_wifipos->nlsend_wakeup_resp(vap, wrqst, result);
		return -1;
	}
	// removing it here and placing it at update_ka_done,
    // Once we get the response then only we should be sending the
    // wakeup response. Let server wait until we are confirmed of the
    // waking up of station.
    if(skip) {
        wifiposdata->nlsend_wakeup_resp(vap, wrqst, result);
#if IEEE80211_WIFIPOSDEBUG
        WIFIPOS_DPRINTK("FND_WKRQST[%d]: %x:%x:%x", wrqst->request_id,wrqst->sta_mac_addr[3], wrqst->sta_mac_addr[4],wrqst->sta_mac_addr[5]);
#endif
    }
    if (ni) {
        ieee80211_free_node(ni);
    }

    vap->iv_wakeup = wrqst->mode;

    if (skip == 0) {
		ieee80211_wifipos_wakeup_t *wakeup =
		    &pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex];

		if (wakeup->wakeup_interval != 0) {
            // Ideally we should never come to this code
			//this reqdata is already being used, buffer must have wrapped around
            // Since the incoming wakeup rqst and sleep rqst are almost serial, we can safetly
            // assume FIFO scenario, we can replace this uncleaned buffer.
			printk("%s: %d: Insufficient space in circular buffer. Reduce incoming request frequency \n",
			     __FUNCTION__, __LINE__);
            // clear this data and replace it will new.
            // TBD: Do we need to do the check for validity of this sta for connectivity and
            // wakeup request?
#if IEEE80211_WIFIPOSDEBUG
            WIFIPOS_DPRINTK("DeletingR[%x],STA: %x:%x:%x  RQT: %x, KA: %x, wkStaCnt: %d \n",
                            pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].request_id,
                            pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].sta_mac_addr[3],
                            pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].sta_mac_addr[4],
                            pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].sta_mac_addr[5],
                            pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].timestamp,
                            pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].num_ka_frm,
                            pvt_wifiposdata->wakeup_sta_count);
#endif
            memset(&pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].sta_mac_addr, 0,IEEE80211_ADDR_LEN);
			pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].timestamp = 0;
			-- pvt_wifiposdata->wakeup_sta_count;
		}
		memcpy(pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].sta_mac_addr,
                wrqst->sta_mac_addr,
                ETH_ALEN);
		pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].wakeup_interval = wrqst->wakeup_interval;
		pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].request_id= wrqst->request_id;
	    pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].timestamp =
		        CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

        pvt_wifiposdata->wakeup[pvt_wifiposdata->wakeupindex].flags |= IEEE80211_KA_DONE;
		++ pvt_wifiposdata->wakeup_sta_count;
		pvt_wifiposdata->wakeupindex++;
		if (pvt_wifiposdata->wakeupindex >= ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA) {
			pvt_wifiposdata->wakeupindex = 0;
		}
	}
	spin_unlock(&pvt_wifiposdata->wakeup_lock);

	OS_SET_TIMER(&pvt_wifiposdata->wifipos_wakeup_timer, 0);
	return 0;
}

/* Function     : ieee80211_wifipos_createtsfrqstdata
 * Arguments    : nsp_tsf_req
 * Functionality: Create tsf request data, and copy it ot circular buffer
 *                for later refernece.
 * Return       : ieee80211_wifipos_tsfreqdata_t
 */
static ieee80211_wifipos_tsfreqdata_t *ieee80211_wifipos_createtsfrqstdata(
                            struct ieee80211vap *vap,
                            struct nsp_tsf_req *tsfrqst)
{
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	ieee80211_wifipos_tsfreqdata_t *tsfreqdata;
    int tsfreqdata_index;
    int new_tsfreq = 1;
    uint16_t tsfCnt = 0;

    if ( NULL == vap->iv_wifipos ) {
        return ( NULL );
    }
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
	spin_lock(&pvt_wifiposdata->tsf_req_lock);

    /* Checking for the previous request for the same AP  */

    for (tsfCnt = 0; tsfCnt < ATH_WIFIPOS_MAX_CONCURRENT_TSF_AP; tsfCnt++) {
        tsfreqdata = &(pvt_wifiposdata->tsfreq[tsfCnt]);
        if(IEEE80211_ADDR_IS_VALID(tsfreqdata->assoc_ap_mac_addr)
                && IEEE80211_ADDR_EQ(tsfreqdata->assoc_ap_mac_addr,tsfrqst->ap_mac_addr)) {
            new_tsfreq = 0;
            printk("%s[%d] Found old tsf req, ReqId: %d \n", __FUNCTION__, __LINE__,
                    tsfreqdata->request_id);
            break;
        }
    }
    if(new_tsfreq) {
        tsfreqdata_index = pvt_wifiposdata->tsfreqdata_index;
        tsfreqdata = &(pvt_wifiposdata->tsfreq[tsfreqdata_index]);
    	pvt_wifiposdata->tsfreqdata_index ++;
    }
    if (pvt_wifiposdata->tsfreqdata_index >= ATH_WIFIPOS_MAX_CONCURRENT_TSF_AP)
        pvt_wifiposdata->tsfreqdata_index = 0;

    IEEE80211_ADDR_COPY(tsfreqdata->assoc_ap_mac_addr, tsfrqst->ap_mac_addr);

	//TBD: Do we need locking?
	if (pvt_wifiposdata->state == IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC) {
        if(pvt_wifiposdata->ccreqdata)
    		tsfreqdata->hc_channel = pvt_wifiposdata->ccreqdata->hc_channel;
        else
    		tsfreqdata->hc_channel = pvt_wifiposdata->cctsfreqdata->hc_channel;

	} else
		tsfreqdata->hc_channel = vap->iv_ic->ic_curchan->ic_ieee;


    tsfreqdata->oc_channel = tsfrqst->channel;
	tsfreqdata->request_id = tsfrqst->request_id;
	memcpy(tsfreqdata->ssid, tsfrqst->ssid, 36);
    tsfreqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_WAIT;
    spin_unlock(&pvt_wifiposdata->tsf_req_lock);

	return tsfreqdata;
}

/* Function     : ieee80211_wifipos_xmittsfrequest
 * Arguments    : ieee80211_wifipos_tsfreqdata_t
 * Functionality: Generates ieee 80211 probe request packet and transmits
 * Return       : Int success for tramit of probe request
 */
int ieee80211_wifipos_xmittsfrequest(struct ieee80211vap *vap, void *data)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	struct ieee80211_node *bss_node;
	struct ath_softc_net80211 *scn = ATH_SOFTC_NET80211(ic);
    ieee80211_wifipos_tsfreqdata_t *tsfrqst = (ieee80211_wifipos_tsfreqdata_t *)data;

	int err = -1;
    int error = -1;

    if ( NULL == vap->iv_wifipos ) {
        return ( -1 );
    }
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;

	bss_node = ieee80211_try_ref_bss_node(vap);
    if (bss_node) {
	ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_TSFREQ, __LINE__,(void *)tsfrqst);

    tsfrqst->tsf_time_stamp = ic->ic_get_TSF64(ic);
    tsfrqst->fast_time_stamp = scn->sc_ops->read_loc_timer_reg(scn->sc_dev);
//TBD: Implement lean channel change
//Will this work: Does not use lean channel change, but uses cc timer
	if (tsfrqst->oc_channel != tsfrqst->hc_channel) {
		ic->ic_scan_start(ic);
		ieee80211_send_cts(vap->iv_bss, IEEE80211_CTS_WIFIPOS);
		OS_SET_TIMER(&pvt_wifiposdata->wifipos_cctimer, MAX_CTS_TO_SELF_TIME);

		error = wlan_set_channel(vap, tsfrqst->oc_channel, 0);
	}

	err = ieee80211_send_probereq(bss_node,
				      vap->iv_myaddr,
				      tsfrqst->assoc_ap_mac_addr,
				      vap->iv_myaddr,
				      tsfrqst->ssid,
				      strlen(tsfrqst->ssid), NULL, 0);

	spin_unlock(&pvt_wifiposdata->tsf_req_lock);
	if (bss_node)
		ieee80211_free_node(bss_node);
    }
	return err;
}

/* Function     : ieee80211_wifipos_createreqdata
 * Arguments    : nsp_mrqst
 * Functionality: Create measurement request and queue in the gloablmeasurement queue
 *                for service.
 * Return       : ieee80211_wifipos_reqdata_t
 */
static ieee80211_wifipos_reqdata_t *ieee80211_wifipos_createreqdata(
                            struct ieee80211vap *vap,
                            struct nsp_mrqst *mrqst)
{
	struct ieee80211com *ic = vap->iv_ic;
    //struct ath_softc_net80211 *scn =ATH_SOFTC_NET80211(ic);
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	ieee80211_wifipos_reqdata_t *reqdata;
    u_int8_t default_chainmask = 0;

    if ( NULL == vap->iv_wifipos ) {
        return ( NULL );
    }
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
    printk("\n 111\n ");

	if (!update_bit_pos) {
		update_bit_pos = 1;//scn->sc_ops->update_loc_ctl_reg(scn->sc_dev, 1);
	}

    default_chainmask = 1;//scn->sc_ops->get_eeprom_chain_mask(scn->sc_dev);

	//Add request entry to the request table.
	reqdata = &(pvt_wifiposdata->reqdata[pvt_wifiposdata->reqdata_index]);

	pvt_wifiposdata->reqdata_index ++;
	if (pvt_wifiposdata->reqdata_index >= ATH_WIFIPOS_MAX_CONCURRENT_REQUESTS)
		pvt_wifiposdata->reqdata_index = 0;
	reqdata->num_probe_rqst = mrqst->no_of_measurements;
	if (mrqst->transmit_rate) {
		reqdata->rateset = (mrqst->transmit_rate
                | mrqst->transmit_rate << 8
                | mrqst->transmit_rate << 16
                | mrqst->transmit_rate << 24);
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:\nRate:%x rateset:%x",
		        mrqst->transmit_rate,
		        reqdata->rateset);
	} else {
		/* if rate is not specified in measurement request
		 * use 6 mbps */
		reqdata->rateset = 0x0b0b0b0b;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:\nUse 6 mbps rateset:%x",
		        reqdata->rateset);
	}
    if(mrqst->transmit_retries) {
        reqdata->retryset = mrqst->transmit_retries;
    } else {
    	reqdata->retryset = 0x01010101;
    }
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:\n Retryset: %x",
            reqdata->rateset);

	memcpy(reqdata->sta_mac_addr, mrqst->sta_mac_addr, ETH_ALEN);
	reqdata->request_id = mrqst->request_id;

    reqdata->bandwidth = mrqst->bandwidth;
	reqdata->req_tstamp = ic->ic_get_TSF32(ic);
	reqdata->pkt_type = (mrqst->mode & FRAME_TYPE_HASH) >> 2;
	IEEE80211_ADDR_COPY(reqdata->spoof_mac_addr, mrqst->spoof_mac_addr);

	//TBD: Do we need locking?
	if (pvt_wifiposdata->state == IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC) {
        if(pvt_wifiposdata->ccreqdata) {
    		reqdata->hc_channel = pvt_wifiposdata->ccreqdata->hc_channel;
        }
        else {
    		//reqdata->hc_channel = pvt_wifiposdata->cctsfreqdata->hc_channel;
        }

	} else
		reqdata->hc_channel = ic->ic_curchan->ic_ieee;
	reqdata->oc_channel = mrqst->channel;

/*
	if (vap->iv_wakeup) {
		for (i = 0; i < ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA; i++) {
			if (IEEE80211_ADDR_EQ
			    (pvt_wifiposdata->wakeup[i].sta_mac_addr,
			     mrqst->sta_mac_addr)) {
				pvt_wifiposdata->wakeup[i].timestamp =
				    CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP
							      ());
				break;
			}
		}
	}
*/
/*
	if ((mrqst->mode & TX_CHAINMASK_HASH) == TX_CHAINMASK_1)
		reqdata->txchainmask = 1;
	if ((mrqst->mode & TX_CHAINMASK_HASH) == TX_CHAINMASK_2)
		reqdata->txchainmask = 3;
	if ((mrqst->mode & TX_CHAINMASK_HASH) == TX_CHAINMASK_3)
		reqdata->txchainmask = 7;
	if ((mrqst->mode & RX_CHAINMASK_HASH) == RX_CHAINMASK_1)
		reqdata->rxchainmask = 1;
	if ((mrqst->mode & RX_CHAINMASK_HASH) == RX_CHAINMASK_2)
		reqdata->rxchainmask = 3;
	if ((mrqst->mode & RX_CHAINMASK_HASH) == RX_CHAINMASK_3)
		reqdata->rxchainmask = 7;
*/

    reqdata->bandwidth = mrqst->bandwidth;
    reqdata->lci_req = mrqst->lci_req;
    reqdata->loc_civ_req = mrqst->loc_civ_req;
    reqdata->req_report_type = mrqst->req_report_type;
    reqdata->req_preamble = mrqst->req_preamble;
    reqdata->asap_mode = mrqst->asap_mode;
    reqdata->burst_exp = mrqst->burst_exp;
    reqdata->burst_period = ntohs(mrqst->burst_period);
    reqdata->burst_dur = ntohs(mrqst->burst_dur);
    reqdata->ftm_per_burst = ntohs(mrqst->ftm_per_burst);

    reqdata->txchainmask = (mrqst->mode & TX_CHAINMASK_HASH) >>5;
    reqdata->rxchainmask = (mrqst->mode & RX_CHAINMASK_HASH) >>8;
   /* Error check for the chainmask to not go beyond what is supported
     * chain masks for TX and RX
     */
	if(!((default_chainmask >>4) & reqdata->txchainmask))
		reqdata->txchainmask = default_chainmask >>4;

	if(!((default_chainmask) & reqdata->rxchainmask))
		reqdata->rxchainmask = (default_chainmask & 0x0f);

    reqdata->mode = mrqst->mode;
    reqdata->request_cnt = mrqst->no_of_measurements;
    reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_WAIT;
    /* Debug prints */
    ieee80211_wifipos_dump_reqdata(reqdata);

    return reqdata;
}

/* Function     : ieee80211_wifipos_syncpsenabled
 * Arguments    : ieee80211_wifipos_reqdata_t
 * Functionality: Check if the TSF information is already there or not
 *                for this particular AP.
 * Return       : 1 for enabled, 0 for disabled.
 */
static int ieee80211_wifipos_syncpsenabled(
                            struct ieee80211vap *vap,
                            ieee80211_wifipos_reqdata_t *reqdata)
{
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	int syncps = 0;
	int index = 0;

    if ( NULL == vap->iv_wifipos ) {
        return ( -1 );
    }
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
	spin_lock(&pvt_wifiposdata->tsf_req_lock);
	for (index = 0; index < ATH_WIFIPOS_MAX_CONCURRENT_TSF_AP; index++) {
		if (IEEE80211_ADDR_EQ(pvt_wifiposdata->tsfreq[index].assoc_ap_mac_addr, reqdata->spoof_mac_addr)) {
			if ((pvt_wifiposdata->tsfreq[index].probe_ap_tsf != 0)
			        && (pvt_wifiposdata->tsfreq[index].assoc_ap_tsf != 0)) {
				syncps = 1;
				break;
			}
		}
	}
    if(IEEE80211_ADDR_EQ(vap->iv_myaddr, reqdata->spoof_mac_addr)) {
        syncps = 1;
    }
	spin_unlock(&pvt_wifiposdata->tsf_req_lock);
	return syncps;
}

/* Function     : ieee80211_wifipos_schedsyncps
 * Arguments    : ieee80211_wifipos_reqdata_t
 * Functionality: Schedule SyncPS reques for later transmission.
 * Return       : 1 for enabled, 0 for disabled.
 */
static int ieee80211_wifipos_schedsyncps(
                            struct ieee80211vap *vap,
                            ieee80211_wifipos_reqdata_t * reqdata)
{

    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	int index = 0;
	u_int32_t diff_tbtt = 0;

    if ( NULL == vap->iv_wifipos ) {
        return ( -1 );
    }
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;

	spin_lock(&pvt_wifiposdata->tsf_req_lock);
	for (index = 0; index < ATH_WIFIPOS_MAX_CONCURRENT_TSF_AP; index++) {
		if (IEEE80211_ADDR_EQ(pvt_wifiposdata->tsfreq[index].assoc_ap_mac_addr, reqdata->spoof_mac_addr)) {
			if (pvt_wifiposdata->tsfreq[index].probe_ap_tsf != 0
                    && pvt_wifiposdata->tsfreq[index].assoc_ap_tsf != 0) {
				diff_tbtt = ic->ic_get_TSF64(ic) -
				    (ieee80211_wifipos_get_next_tbtt_tsf_64(vap) - 102400);
				if (diff_tbtt < pvt_wifiposdata->tsfreq[index].diff_tsf) {
					diff_tbtt = pvt_wifiposdata->tsfreq[index].diff_tsf - diff_tbtt;
				} else {
					diff_tbtt =
					    ieee80211_wifipos_get_next_tbtt_tsf_64(vap)
					    - ic->ic_get_TSF64(ic)
					    + pvt_wifiposdata->tsfreq[index].diff_tsf;
				}
				diff_tbtt = diff_tbtt / 1000;
				//Free the timer if already allocated. Need to free and reallocate, because initialize_timer() allocates a new OS timer
				//initialize_timer() is the only timer api function which allows the timeout and the context to be set
				//free_timer checks internally to verify that it has been allocated and does nothing if not
                if(reqdata->pstimer == NULL) {
                    printk("%s[%d]: pstimer is NULL", __func__, __LINE__);
                    return -1;
                }

                pvt_wifiposdata->syncpsreqdata = reqdata;
                ath_free_timer(reqdata->pstimer);

				ath_initialize_timer(ic->ic_osdev,
				        reqdata->pstimer,
				        diff_tbtt,
					    ieee80211_wifipos_syncps_timer,
					    vap);
				ath_start_timer(reqdata->pstimer);
			}
            break;
		}
	}
    if(IEEE80211_ADDR_EQ(vap->iv_myaddr, reqdata->spoof_mac_addr)){
        diff_tbtt = ieee80211_wifipos_get_next_tbtt_tsf_64(vap) - ic->ic_get_TSF64(ic);
        diff_tbtt = diff_tbtt/1000;

        if(reqdata->pstimer == NULL) {
            printk("%s[%d]: pstimer is NULL", __func__, __LINE__);
            return -1;
        }

        pvt_wifiposdata->syncpsreqdata = reqdata;
        ath_free_timer(reqdata->pstimer);
        ath_initialize_timer(ic->ic_osdev,
                reqdata->pstimer,
                diff_tbtt,
                ieee80211_wifipos_syncps_timer,
                vap);
        ath_start_timer(reqdata->pstimer);
    }
	spin_lock(&pvt_wifiposdata->tsf_req_lock);
	return 0;
}

/* Function     : ieee80211_wifipos_xmitprobe
 * Arguments    : ieee80211_wifipos_reqdata_t
 * Functionality: Generates probe packets and transmits them. If needed, CC is done here.
 * Return       : 1 for enabled, 0 for disabled.
 */
int ieee80211_wifipos_xmitprobe(
                            struct ieee80211vap *vap,
                            ieee80211_wifipos_reqdata_t *reqdata)
{
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node *ni = NULL;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	u_int32_t rxclear_pct = 0, rxframe_pct = 0, txframe_pct = 0;
	u_int8_t pwr_save, cnt;
	int qosresp = 0;
#if UMAC_SUPPORT_WIFIPOS_LEANCC
	u_int32_t tsf1 = 0, tsf2 = 0, tsf3 = 0, tsf4 = 0;
#endif
    int vmf = 1;
    u_int8_t tmp_node = 0;

    if ( NULL == vap->iv_wifipos ) {
        return ( -1 );
    }
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
printk("%s: %d: hc(%d) oc(%d)\n", __FUNCTION__, __LINE__,
    reqdata->hc_channel,
    reqdata->oc_channel);
    if(!IEEE80211_ADDR_IS_VALID(reqdata->sta_mac_addr)){
        printk("%s[%d]: Error: Invalid MAC address in the request\n",
                __func__, __LINE__);
       ieee80211_wifipos_dump_reqdata(reqdata);
       return -1;
    }
#if IEEE80211_WIFIPOSDEBUG
    WIFIPOS_DPRINTK("%s[%d]: ReqId: %x,  xmit_tstamp: %x \n", __func__, __LINE__,
            reqdata->request_id, ic->ic_get_TSF32(ic));
#endif
    if(reqdata->hc_channel == reqdata->oc_channel) {
	    ni = ieee80211_find_node(&ic->ic_sta, reqdata->sta_mac_addr);
    }
	if (ni == NULL) {
		ni = ieee80211_tmp_node(vap, reqdata->sta_mac_addr);
                tmp_node = 1;
        if (ni == NULL)
            return -1;
		if (reqdata->spoof_mac_addr[0] || reqdata->spoof_mac_addr[1]
		    || reqdata->spoof_mac_addr[2] || reqdata->spoof_mac_addr[3]
		    || reqdata->spoof_mac_addr[4] || reqdata->spoof_mac_addr[5]) {
			IEEE80211_ADDR_COPY(ni->ni_bssid, reqdata->spoof_mac_addr);
		}
        printk("%s: %d: create tmp node(%pK)\n", __FUNCTION__, __LINE__, ni);
	}
	reqdata->ni_tmp_sta = (void *)ni;
    if(reqdata->ni_tmp_sta == NULL)
        return -1;

    // incrementing current request count for each call of xmit probe
    pvt_wifiposdata->num_mrqst_inprogress++;
	pwr_save =
	    ieee80211node_has_flag((struct ieee80211_node *)reqdata->ni_tmp_sta,
				   IEEE80211_NODE_PWR_MGT);
    if (reqdata->oc_channel == ic->ic_curchan->ic_ieee) {
        ieee80211_send_cts(vap->iv_bss,IEEE80211_CTS_WIFIPOS);
    }
	if (reqdata->oc_channel != ic->ic_curchan->ic_ieee) {
        printk("%s: %d: Change channel to OC\n", __FUNCTION__, __LINE__);
        ieee80211_send_cts(vap->iv_bss,IEEE80211_CTS_WIFIPOS);
		ic->ic_scan_start(ic);
		OS_SET_TIMER(&pvt_wifiposdata->wifipos_cctimer, MAX_CTS_TO_SELF_TIME);
#if UMAC_SUPPORT_WIFIPOS_LEANCC
		/* Copying this structure to global variable as we need it in resuming
		 * home channel traffic.
		 * incrementing the refernece count for this node. Since the life-span
		 * of this node is till we resume the home channel traffic.
		 */
		// This increment in reference count should be decremented in resuming
		// home channel traffic.
		if (!ieee80211_try_ref_node((struct ieee80211_node *)reqdata->ni_tmp_sta))
			return -1;

		tsf1 = ic->ic_get_TSF32(ic);
		wlan_pause_node(vap, NULL, true);
		tsf2 = ic->ic_get_TSF32(ic);
#endif
		//tsf_start1 = ic->ic_get_TSF32(ic); //TBD
#if UMAC_SUPPORT_WIFIPOS_LEANCC
		wlan_lean_set_channel(vap, reqdata->oc_channel);
		ic->ic_disable_hwq(ic, 0x03df);
#else
		wlan_set_channel(vap, reqdata->oc_channel, 0);
#endif
		//tsf_end1 = ic->ic_get_TSF32(ic); //TBD
		ic->ic_get_channel_busy_info(ic, &rxclear_pct, &rxframe_pct,
					     &txframe_pct);
	}
#if UMAC_SUPPORT_WIFIPOS_LEANCC
	tsf3 = ic->ic_get_TSF32(ic);
#endif

	for (cnt = 0; cnt < reqdata->request_cnt; cnt++) {
        if(cnt == reqdata->request_cnt -1 )
            vmf = 0;
		if ((reqdata->mode & FRAME_TYPE_HASH) == QOS_NULL_FRAME) {
printk("%s: %d: QOS_NULL_FRAME(%d)\n", __FUNCTION__, __LINE__, cnt);
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			        "WifiPositioning:QOS NULL frame:%d", cnt);
			qosresp = ieee80211_send_qosnull_probe(
                    (struct ieee80211_node*)reqdata->ni_tmp_sta,
					WME_AC_VO,
					vmf,
				    (void *)reqdata);

		} else if ((reqdata->mode & FRAME_TYPE_HASH) == NULL_FRAME) {
printk("%s: %d: NULL_FRAME(%d)\n", __FUNCTION__, __LINE__, cnt);
		    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			        "WifiPositioning:NULL frame:%d", cnt);
            if (reqdata->ni_tmp_sta) {
			ieee80211_send_null_probe(
                    (struct ieee80211_node *)reqdata->ni_tmp_sta,
                    vmf,
				    (void *)reqdata);
            }
		}
	}
	// This ref count decrement corresponds to increment in ref count at ieee80211_tmp_node()  in createreqdata
	if (reqdata->ni_tmp_sta) {
            if(tmp_node == 1) {
                wlan_objmgr_delete_node((struct ieee80211_node *)reqdata->ni_tmp_sta);
            }
            else {
                ieee80211_free_node((struct ieee80211_node *)reqdata->ni_tmp_sta);
            }
        }
#if UMAC_SUPPORT_WIFIPOS_LEANCC
	tsf4 = ic->ic_get_TSF32(ic);
	wpnt[cnt_cc] = tsf2 - tsf1;
	wlsct0[cnt_cc] = tsf_end1 - tsf_start1;
	wgpt[cnt_cc] = tsf4 - tsf3;
#endif
	return qosresp;
}

/* Function     : ieee80211_wifipos_sendtsfresp
 * Arguments    : ieee80211_wifipos_tsfreqdata_t
 * Functionality: Send tsf response.
 * Return       : void
 */
static void ieee80211_wifipos_sendtsfresp(
                            struct ieee80211vap *vap,
                            ieee80211_wifipos_tsfreqdata_t *tsfreqdata)
{
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	u_int64_t tsf_sync, local_tsf_tstamp;
	u_int64_t beacon_time, diff;
      u_int32_t difference = 0;

    if ( NULL == vap->iv_wifipos ) {
        return;
    }
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;

	spin_lock(&pvt_wifiposdata->tsf_req_lock);

	vap->iv_wifipos->nlsend_tsf_resp(vap, tsfreqdata);

    if(update_bit_pos){
        difference = abs(vap->iv_local_tsf_tstamp - tsfreqdata->fast_time_stamp);
        vap->iv_local_tsf_tstamp = ((difference/44) + tsfreqdata->tsf_time_stamp);
    }
   	tsf_sync = le64_to_cpu(vap->iv_tsf_sync);
	beacon_time = 102400 - OS_MOD64_TBTT_OFFSET(tsf_sync, 102400);
	tsf_sync = beacon_time + vap->iv_local_tsf_tstamp;
	local_tsf_tstamp = ieee80211_wifipos_get_next_tbtt_tsf_64(vap);
	if (local_tsf_tstamp > tsf_sync) {
		diff = local_tsf_tstamp - tsf_sync;
		diff = 102400 - OS_MOD64_TBTT_OFFSET(diff, 102400);
	} else {
		diff = tsf_sync - local_tsf_tstamp;
		diff = OS_MOD64_TBTT_OFFSET(diff, 102400);
	}
	tsfreqdata->diff_tsf = diff;
	spin_unlock(&pvt_wifiposdata->tsf_req_lock);
#if IEEE80211_WIFIPOSDEBUG
    WIFIPOS_DPRINTK("%s %d: TsfTo: %s, HC: %d OC: %d, SSID: %s, diff: %u \n", __FUNCTION__, __LINE__,
           ether_sprintf(tsfreqdata->assoc_ap_mac_addr),
	       tsfreqdata->hc_channel, tsfreqdata->oc_channel,
           tsfreqdata->ssid,tsfreqdata->diff_tsf );
#endif
}

/* Function     : ieee80211_wifipos_resumeprobes
 * Arguments    : work_struct
 * Functionality: It itrates through all the pending requests in
 *                queue and schedule them for service
 * Return       : void
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
static void ieee80211_wifipos_resumeprobes(void *work_p)
#else
static void ieee80211_wifipos_resumeprobes(struct work_struct *work_p)
#endif
{
	ieee80211_wifipos_pvt_data_t *pvt_wifiposdata =
		container_of(work_p, ieee80211_wifipos_pvt_data_t, resumeprobes_work);
    ieee80211_wifipos_data_t *wifiposdata = &pvt_wifiposdata->wifiposdata;
    struct ieee80211vap *vap = wifiposdata->vap;
	ieee80211_wifipos_reqdata_t *reqdata;
	ieee80211_wifipos_tsfreqdata_t *tsfreqdata;
	int count = 0;
    printk("\n ************************** In resume probes***************************************\n");
	//Search for request entry in request data store and tsf data store
	//Process all requests upto and including the first request which needs a channel change
	//The first channel change request will reschedule this work queue on completion, if there are pending requests
    	for (count = 0; count < ATH_WIFIPOS_MAX_CONCURRENT_REQUESTS; count++) {
	    	reqdata = &pvt_wifiposdata->reqdata[count];
            //printk("\n the mac_addr is %0x:%0x:%0x:%0x:%0x:%0x \n",)
            if(!IEEE80211_ADDR_IS_VALID(reqdata->sta_mac_addr)){
                continue;
            } else {

    		    if (reqdata->state ==
	    	        IEEE80211_WIFIPOS_RQSTSTATE_WAIT) {
#if IEEE80211_WIFIPOSDEBUG
                    WIFIPOS_DPRINTK("%s[%d]: ReqId: %d, Rstate: %d, HC: %d, OC: %d, GState: %d, data_index: %d\n",
                        __func__, __LINE__,
                        reqdata->request_id,
                        reqdata->state,
                        reqdata->hc_channel,
                        reqdata->oc_channel,
                        pvt_wifiposdata->state,count );
#endif
printk("%s : %d:  *****************start WAIT Mreq\n",__FUNCTION__, __LINE__);

    		    	wifiposdata->fsm(vap, IEEE80211_WIFIPOS_EVENT_MRQST,
				    	      (void *)reqdata, 1);
                    if (reqdata->hc_channel != reqdata->oc_channel){
                        return;
		            }
                }
            }

	    }
    	for (count = 0; count < ATH_WIFIPOS_MAX_CONCURRENT_TSF_AP; count++) {
	    	tsfreqdata = &pvt_wifiposdata->tsfreq[count];

		    if (tsfreqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_WAIT) {
#if IEEE80211_WIFIPOSDEBUG
                WIFIPOS_DPRINTK("%s[%d]: ReqId: %d, HC: %d, OC: %d, GState: %d, data_index: %d \n",
                    __func__, __LINE__,
                    tsfreqdata->request_id,
                    tsfreqdata->hc_channel,
                    tsfreqdata->oc_channel,
                    pvt_wifiposdata->state,
                    count);
#endif
                // Always go to RUNNING HC and from there take the new request
    			wifiposdata->fsm(vap,
    			        IEEE80211_WIFIPOS_EVENT_TSFRQST,
    			        (void *)tsfreqdata, 1);
                if (tsfreqdata->hc_channel != tsfreqdata->oc_channel)
                    return;
            }
	   }
}

/* Function     : ieee80211_wifipos_resume_hc_traffic
 * Arguments    : Nothin since we already using wifiposdata
 *                structure. GG
 *                note: This should change later as we need per request
 *                structure to support concurrent probes.
 * Fucntionality: a) To change the channel back to home channel
 *                b) Resume the traffic at home channel
 *                c) cleanup the temporary node.
 * Return       : Void
 */
static void ieee80211_wifipos_resume_hc_traffic(
                            struct ieee80211vap *vap,
                            int hc_channel,
						    struct ieee80211_node *ni_tmp_sta)
{

	struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;

	u_int32_t rxclear_pct = 0, rxframe_pct = 0, txframe_pct = 0;
#if UMAC_SUPPORT_WIFIPOS_LEANCC
	u_int32_t tsf1 = 0, tsf2 = 0, tsf3 = 0, tsf4 = 0;
#endif
    if ( NULL == vap->iv_wifipos ) {
    return;
    }
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;

	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "%s[%d]: Finished probing b4 CTS TIME OUT changing back to old channel: %d current channel: %d",
              __func__,__LINE__,
			  ic->ic_prevchan->ic_ieee,
			  ic->ic_curchan->ic_ieee);

    spin_lock(&pvt_wifiposdata->lock);

	ic->ic_get_channel_busy_info(ic, &rxclear_pct, &rxframe_pct, &txframe_pct);
	tsf_start2 = ic->ic_get_TSF32(ic);
    // TBD: Need lean channel change for tsf request as well.
    if(!pvt_wifiposdata->cctsfreqdata) { // this is for probing, tsf request needs normal channel change.
#if UMAC_SUPPORT_WIFIPOS_LEANCC
    	if (ni_tmp_sta) {
	    	wlan_pause_node(vap, ni_tmp_sta, true);
    	}
	    tsf3 = ic->ic_get_TSF32(ic);
        //wlan_lean_set_channel(vap, ic->ic_prevchan->ic_ieee);
    	wlan_lean_set_channel(vap, hc_channel);
#else
	wlan_set_channel(vap, hc_channel, 0);
#endif
    	tsf_end2 = ic->ic_get_TSF32(ic);
    	ic->ic_scan_end(ic);
     } else {
        wlan_set_channel(vap, hc_channel, 0);
        ic->ic_scan_end(ic);
     }
//TBD: Remove?
    	spin_lock(&pvt_wifiposdata->probedata_lock);
    	while (pvt_wifiposdata->tx_index != pvt_wifiposdata->rx_index) {
    		if (pvt_wifiposdata->rx_index > pvt_wifiposdata->tx_index) {
    			IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
				        "WifiPositioning:Remove toa:%x rx_index:%d",
				        pvt_wifiposdata->probedata[pvt_wifiposdata->rx_index].toa,
				        pvt_wifiposdata->rx_index);
    			pvt_wifiposdata->probedata[pvt_wifiposdata->rx_index].toa = 0;
    			pvt_wifiposdata->rx_index--;
    		} else {
    			IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
				        "WifiPositioning:Remove tod:%x tx_index:%d",
				        pvt_wifiposdata->probedata[pvt_wifiposdata->tx_index].tod,
				        pvt_wifiposdata->tx_index);
    			pvt_wifiposdata->probedata[pvt_wifiposdata->tx_index].tod = 0;
    			pvt_wifiposdata->tx_index--;
    		}
    	}
    	spin_unlock(&pvt_wifiposdata->probedata_lock);

#if UMAC_SUPPORT_WIFIPOS_LEANCC
    if(!pvt_wifiposdata->cctsfreqdata) { // this is for probing only, tsf request needs normal channel change.
    	tsf1 = ic->ic_get_TSF32(ic);
    	ic->ic_vap_reap_txqs(ic, vap);
    	tsf2 = ic->ic_get_TSF32(ic);
    	wlan_pause_node(vap, NULL, false);
    	if (ni_tmp_sta) {
    		wlan_pause_node(vap, ni_tmp_sta, false);
    	}
    	// Decrementing the nod ref cnt corresponding to the increment
    	// in ref cnt while updating this variable.
    	if (ni_tmp_sta) {
    		ieee80211_free_node(ni_tmp_sta);
    	}

    	tsf4 = ic->ic_get_TSF32(ic);

    	wlsct1[cnt_cc] = tsf_end2 - tsf3;
    	wcnt[cnt_cc] = tsf3 - tsf_start2;
    	wrtxqst[cnt_cc] = tsf2 - tsf1;
    	wupnt[cnt_cc] = tsf4 - tsf2;
    	CTStet[cnt_cc] = tsf_end2 - tsf_start1;
    	avg_rxclear_pct += rxclear_pct;
    	avg_rxframe_pct += rxframe_pct;
    	avg_txframe_pct += txframe_pct;
    /*  DEBUG prints for validation.
        printk(KERN_DEBUG"%s[%d]:START: %u, START_TIMER_FUNC: %u\n", __func__, __LINE__,
                tsf_start1, tsf_start2);
        printk(KERN_DEBUG"%s[%d]: CCT1: %d, CCT2: %d, CTS_TIME: %d \n",
                    __func__,
                    cnt_cc,
                    tsf_end1 - tsf_start1,
                    tsf_end2 - tsf_start2,
                    tsf_end2 - tsf_start1 );
    */
    /*
        printk(KERN_DEBUG"%s[%d]: rxclear_pct: %u, rxframe_pct: %u, txframe_pct: %u\n",
                        __func__,
                        cnt_cc,
                        rxclear_pct,
                        rxframe_pct,
                        txframe_pct);
    */
    	cnt_cc++;
    	ieee80211_wifipos_calccctimingstats(pvt_wifiposdata);
    }

#endif
    spin_unlock(&pvt_wifiposdata->lock);

}

/* Function     : ieee80211_wifipos_cleanprobedata
 * Arguments    : work_struct
 * Functionality: It itrates through all the recieved CD and matches them
 *                 and throws out stale data
 * Return       : void
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
static void ieee80211_wifipos_cleanprobedata(void *work_p)
#else
static void ieee80211_wifipos_cleanprobedata(struct work_struct *work_p)
#endif
{
	ieee80211_wifipos_pvt_data_t *pvt_wifiposdata =
		container_of(work_p, ieee80211_wifipos_pvt_data_t, cleanprobedata_work);
    ieee80211_wifipos_data_t *wifiposdata = &pvt_wifiposdata->wifiposdata;
    struct ieee80211vap *vap = wifiposdata->vap;
	static int index = 0;
	int temp = index;

	temp = temp ? temp - 1 : ATH_WIFIPOS_MAX_CONCURRENT_PROBES - 1;
	spin_lock(&pvt_wifiposdata->lock);
	spin_lock(&pvt_wifiposdata->probedata_lock);
	while (!pvt_wifiposdata->probedata[index].valid
            && index != temp
	        && index != pvt_wifiposdata->proc_index) {
		pvt_wifiposdata->probedata[index].toa = pvt_wifiposdata->probedata[index].tod = 0;
		pvt_wifiposdata->probedata[index].valid = WIFIPOS_FALSE;
		index++;
		if (index >= ATH_WIFIPOS_MAX_CONCURRENT_PROBES) {
			index = 0;
		}
	}
	while (pvt_wifiposdata->probedata[index].valid) {
		wifiposdata->nlsend_probe_resp(vap, &pvt_wifiposdata->probedata[index]);
		pvt_wifiposdata->probedata[index].toa = pvt_wifiposdata->probedata[index].tod = 0;
		pvt_wifiposdata->probedata[index].valid = WIFIPOS_FALSE;
		index++;
		if (index >= ATH_WIFIPOS_MAX_CONCURRENT_PROBES) {
			index = 0;
		}
	}
	spin_unlock(&pvt_wifiposdata->probedata_lock);
	spin_unlock(&pvt_wifiposdata->lock);
}

#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
static void ieee80211_wifipos_emptyresp(void *work_p)
#else
static void ieee80211_wifipos_emptyresp(struct work_struct *work_p)
#endif
{
	ieee80211_wifipos_pvt_data_t *pvt_wifiposdata =
		container_of(work_p, ieee80211_wifipos_pvt_data_t, emptyresp_work);
    ieee80211_wifipos_data_t *wifiposdata = &pvt_wifiposdata->wifiposdata;
    struct ieee80211vap *vap = wifiposdata->vap;
    ieee80211_wifiposdesc_t *wifiposdesc;

    spin_lock(&pvt_wifiposdata->emptydesc_lock);
    while(pvt_wifiposdata->emptydesc_ridx != pvt_wifiposdata->emptydesc_widx) {
        wifiposdesc = &pvt_wifiposdata->emptydesc[pvt_wifiposdata->emptydesc_ridx];

        spin_unlock(&pvt_wifiposdata->emptydesc_lock);
        vap->iv_wifipos->nlsend_empty_resp(vap, wifiposdesc);
        spin_lock(&pvt_wifiposdata->emptydesc_lock);

        if ( ++ pvt_wifiposdata->emptydesc_ridx >= ATH_WIFIPOS_MAX_CONCURRENT_EMPTY_RESP)
            pvt_wifiposdata->emptydesc_ridx = 0;
    }
    spin_unlock(&pvt_wifiposdata->emptydesc_lock);

}

/* Function     : find_matching_node
 * Arguments    : ieee80211_wifiposdesc_t
 * Functionality: Itrated thought the array of received TOD/TOA and look for the
 *                matching node for any new recieved TOA/TOD
 * Return       : 0: Success, -1: failure
 */
static int find_matching_node(
                            ieee80211_wifipos_pvt_data_t *pvt_wifiposdata,
                            ieee80211_wifiposdesc_t * wifiposdesc,
                            int desc_type)
{
	int i = 0;
	int match_node = -1;

	if (desc_type == TX_DESC) {

		i = pvt_wifiposdata->proc_index;
		while (i != pvt_wifiposdata->rx_index) {
			if (pvt_wifiposdata->probedata[i].toa
			    && (pvt_wifiposdata->probedata[i].toa - wifiposdesc->tod) > ONE_SIFS_DURATION
			    && (pvt_wifiposdata->probedata[i].toa - wifiposdesc->tod) < TWO_SIFS_DURATION) {
				match_node = i;
				break;
			}
			i++;
			if (i >= ATH_WIFIPOS_MAX_CONCURRENT_PROBES)
				i = 0;

		}
	} else {
		i = pvt_wifiposdata->proc_index;
		while (i != pvt_wifiposdata->tx_index) {
			if (pvt_wifiposdata->probedata[i].tod
                    && (wifiposdesc->toa - pvt_wifiposdata->probedata[i].tod) > ONE_SIFS_DURATION
			        && (wifiposdesc->toa - pvt_wifiposdata->probedata[i].tod) < TWO_SIFS_DURATION) {
				match_node = i;
				break;
			}
			i++;
			if (i >= ATH_WIFIPOS_MAX_CONCURRENT_PROBES)
				i = 0;
		}
	}
	return match_node;
}

int ieee80211_isthere_wakeup_request(struct ieee80211_node *ni)
{
	if (ni == NULL)
		return 0;

	return (ni->ni_flags & IEEE80211_NODE_WAKEUP);
}

int32_t ieee80211com_init_netlink(struct ieee80211com *ic)
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
            if (pdev == NULL) {
                 qdf_print("%s: pdev object (id: %d) is NULL ", __func__, scn_idx);
                 continue;
            }

            ic_tmp = wlan_pdev_get_mlme_ext_obj(pdev);

            if (ic_tmp && ic_tmp->rtt_sock != NULL) {
                ic->rtt_sock = ic_tmp->rtt_sock;
                printk("%s: Socket already created %pK\n", __func__, ic->rtt_sock);
                wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                return 0;
            }
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
        }
    }

    soc = ((struct ol_ath_softc_net80211 *) ic)->soc;
    for (scn_idx = 0; scn_idx < wlan_psoc_get_pdev_count(soc->psoc_obj); scn_idx++) {
        pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, scn_idx, WLAN_MLME_NB_ID);
        if (pdev == NULL) {
             qdf_print("%s: pdev object (id: %d) is NULL ", __func__, scn_idx);
             continue;
        }

        ic_tmp = wlan_pdev_get_mlme_ext_obj(pdev);

        if (ic_tmp && ic_tmp->rtt_sock != NULL) {
            ic->rtt_sock = ic_tmp->rtt_sock;
            printk("%s: Socket already created %pK\n", __func__, ic->rtt_sock);
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
            return 0;
        }
        wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
    }

    ic->rtt_sock = OS_NETLINK_CREATE(
                        NETLINK_WIFIPOS,
                        1,
                        (void *)ieee80211_wifpos_processnspmsg,
                        THIS_MODULE);

    if (ic->rtt_sock == NULL) {
        printk("\n the 1st socket creation has failed\n");
        ic->rtt_sock = OS_NETLINK_CREATE(
                            NETLINK_WIFIPOS + 1,
                            1,
                            (void *)ieee80211_wifpos_processnspmsg,
                            THIS_MODULE);
        if (ic->rtt_sock == NULL) {
            printk("%s: %d: Wifipos Netlink socket failed\n",
                __FUNCTION__, __LINE__);
            return -ENODEV;
        } else
            printk("%s: %d: Wifipos 2nd Netlink socket created:%pK\n",
                __FUNCTION__, __LINE__, ic->rtt_sock);
    } else{
        printk(KERN_INFO"%s: %d: Wifipos 1st Netlink socket created:%pK\n",
                __FUNCTION__, __LINE__, ic->rtt_sock);
    }

    return 0;
}
/* Function     : ieee80211_update_ka_done
 * Arguments    : *sta_mac_addr, tx_status
 * Functionality: Iterate through all the STA in KA list and update their responses.
 * Return       : 0: Success, -1: failure
 */
int32_t ieee80211_ol_update_ka_done(u_int8_t *sta_mac_addr, u_int8_t tx_status)
{
    struct ieee80211com *ic;
    struct ieee80211vap *vap = NULL, *tvap = NULL;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
    int i = 0;
    int retVal = -1;
    struct nsp_wakeup_req wrqst;
    ol_ath_soc_softc_t *soc;
    int soc_idx, scn_idx;
    struct wlan_objmgr_pdev *pdev;

    for (soc_idx = 0; soc_idx < GLOBAL_SOC_SIZE; soc_idx++) {
        soc = ol_global_soc[soc_idx];
        if (soc == NULL)
            continue;
        for (scn_idx = 0; scn_idx < wlan_psoc_get_pdev_count(soc->psoc_obj); scn_idx++) {
            pdev = wlan_objmgr_get_pdev_by_id(soc->psoc_obj, scn_idx, WLAN_MLME_NB_ID);
            if (pdev == NULL) {
                 qdf_print("%s: pdev object (id: %d) is NULL ", __func__, scn_idx);
                 continue;
            }

            ic = wlan_pdev_get_mlme_ext_obj(pdev);
            if (ic == NULL) {
                 wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
                 qdf_err("ic (id: %d) is NULL ", scn_idx);
                 continue;
            }
            printk("%s: %d: \n", __FUNCTION__, __LINE__);

            if (!TAILQ_EMPTY(&ic->ic_vaps)) {
                TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
                    if (vap->iv_wifipos) {
                        printk("vap addr(%02x:%02x:%02x:%02x:%02x:%02x) idx(%d)\n",
                               vap->iv_myaddr[0],
                               vap->iv_myaddr[1],
                               vap->iv_myaddr[2],
                               vap->iv_myaddr[3],
                               vap->iv_myaddr[4],
                               vap->iv_myaddr[5],
                               vap->iv_unit);
                        pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
                        break;
                    }
                }
            }
            wlan_objmgr_pdev_release_ref(pdev, WLAN_MLME_NB_ID);
        }
    }

    for (i = 0; i < ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA; i++) {
        if ((pvt_wifiposdata == NULL) || (vap == NULL))
            return retVal;
        else {
            spin_lock(&pvt_wifiposdata->wakeup_lock);
            if (IEEE80211_ADDR_EQ (pvt_wifiposdata->wakeup[i].sta_mac_addr, sta_mac_addr)){
                pvt_wifiposdata->wakeup[i].flags |= (IEEE80211_KA_DONE | (tx_status << 1));
                if( !(pvt_wifiposdata->wakeup[i].flags & IEEE80211_WAKEUP_RESP_SENT)
                        && (pvt_wifiposdata->wakeup[i].flags & IEEE80211_KA_SUCCESS)) {
                    // if successful KA,  send the wakeup response
                    printk("%s: %d: KA success\n", __FUNCTION__, __LINE__);
                    pvt_wifiposdata->wakeup[i].flags |= IEEE80211_WAKEUP_RESP_SENT;
                    wrqst.request_id = pvt_wifiposdata->wakeup[i].request_id;
                    memcpy( wrqst.sta_mac_addr, pvt_wifiposdata->wakeup[i].sta_mac_addr, ETH_ALEN);
                    vap->iv_wifipos->nlsend_wakeup_resp(vap, &wrqst, 0);
                }
                spin_unlock(&pvt_wifiposdata->wakeup_lock);
                retVal = 0;
                break;
            }
            spin_unlock(&pvt_wifiposdata->wakeup_lock);
        }
    }
    return retVal;
}
/* Function     : ieee80211_update_ka_done
 * Arguments    : *sta_mac_addr, tx_status
 * Functionality: Iterate through all the STA in KA list and update their responses.
 * Return       : 0: Success, -1: failure
 */
int ieee80211_update_ka_done(u_int8_t *sta_mac_addr, u_int8_t tx_status)
{
    struct ieee80211com *ic;
    struct ieee80211vap *vap = NULL, *tvap = NULL;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
    int i = 0;
    int retVal = -1;
    struct nsp_wakeup_req wrqst;

    printk("%s: %d: \n", __FUNCTION__, __LINE__);

    //TBD
    for (i = 0; i < num_global_scn; i ++) {
        ic = &global_scn[i]->sc_ic;

        if (!TAILQ_EMPTY(&ic->ic_vaps)) {
            TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
                if (vap->iv_wifipos) {
                    printk("vap addr(%02x:%02x:%02x:%02x:%02x:%02x) idx(%d)\n",
                        vap->iv_myaddr[0],
                        vap->iv_myaddr[1],
                        vap->iv_myaddr[2],
                        vap->iv_myaddr[3],
                        vap->iv_myaddr[4],
                        vap->iv_myaddr[5],
                        vap->iv_unit);
                    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
                    break;
                }
            }
        }
    }

    for (i = 0; i < ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA; i++) {
        if ((pvt_wifiposdata == NULL) || (vap == NULL))
                    return retVal;
        else {
            spin_lock(&pvt_wifiposdata->wakeup_lock);
            if (IEEE80211_ADDR_EQ (pvt_wifiposdata->wakeup[i].sta_mac_addr, sta_mac_addr)){
                pvt_wifiposdata->wakeup[i].flags |= (IEEE80211_KA_DONE | (tx_status << 1));
                if( !(pvt_wifiposdata->wakeup[i].flags & IEEE80211_WAKEUP_RESP_SENT)
                        && (pvt_wifiposdata->wakeup[i].flags & IEEE80211_KA_SUCCESS)) {
                    // if successful KA,  send the wakeup response
                    printk("%s: %d: KA success\n", __FUNCTION__, __LINE__);
                    pvt_wifiposdata->wakeup[i].flags |= IEEE80211_WAKEUP_RESP_SENT;
                    wrqst.request_id = pvt_wifiposdata->wakeup[i].request_id;
                    memcpy( wrqst.sta_mac_addr, pvt_wifiposdata->wakeup[i].sta_mac_addr, ETH_ALEN);
                    vap->iv_wifipos->nlsend_wakeup_resp(vap, &wrqst, 0);
                }
                retVal = 0;
                spin_unlock(&pvt_wifiposdata->wakeup_lock);
                break;
            }
            spin_unlock(&pvt_wifiposdata->wakeup_lock);
        }
    }
    return retVal;
}

/* Function     : ieee80211_update_wifipos_stats
 * Arguments    : ieee80211_wifiposdesc_t
 * Functionality: Itrated thought the array of received TOD/TOA and look for the
 *                empty place to update new recieved TOA/TOD
 * Return       : 0: Success, -1: failure
 */
int32_t ieee80211_ol_update_wifipos_stats(struct ieee80211com *ic, ieee80211_ol_wifiposdesc_t *wifiposdesc)
{
    struct ieee80211vap *vap = NULL, *tvap = NULL;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	u_int8_t expected_rx_pkt = 0x00;
	int index = 0;
    int size = 0;
    u_int64_t t1, t2;

    if (!wifiposenable)
        return -1;

    if (!TAILQ_EMPTY(&ic->ic_vaps)) {
        TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
            if (vap->iv_wifipos && vap->iv_wifipos->pvt_wifiposdata) {
                pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
                break;
            }
        }
    }
    if ((pvt_wifiposdata == NULL) || (vap == NULL))
        return -1;
	spin_lock(&pvt_wifiposdata->lock);
	/*Search for request entry in request data store */
    // We are using only NULL/QOS NULL data.
    expected_rx_pkt = FC0_ACK_PKT;
	spin_lock(&pvt_wifiposdata->probedata_lock);
    index = pvt_wifiposdata->proc_index;


    if ((index >= 0) && (wifiposdesc != NULL)) {
    	memcpy(pvt_wifiposdata->probedata[index].sta_mac_addr,
                    wifiposdesc->sta_mac_addr,
                                ETH_ALEN);
        pvt_wifiposdata->probedata[index].request_id = wifiposdesc->request_id;

        pvt_wifiposdata->probedata[index].tod = wifiposdesc->tod;
        pvt_wifiposdata->probedata[index].toa = wifiposdesc->toa;
#define RTT_MEAS_FRAME_TMR 2
	if(wifiposdesc->req_type == RTT_MEAS_FRAME_TMR) {
            t1 = wifiposdesc->tod;
            t2 = wifiposdesc->toa;
	    pvt_wifiposdata->probedata[index].toa =wifiposdesc->t4 -  t1; //toa = t4-t1
            pvt_wifiposdata->probedata[index].tod = wifiposdesc->t3 - t2; //tod = t3-t2
            printk("%s:TMR Frame, t1:%llu, t2:%llu, t3:%llu, t4:%llu tod:%llu\n",  __FUNCTION__, t1, t2, wifiposdesc->t3, wifiposdesc->t4, pvt_wifiposdata->probedata[index].tod);
        }
        pvt_wifiposdata->probedata[index].valid = WIFIPOS_TRUE;
        pvt_wifiposdata->probedata[index].rssi[0] = wifiposdesc->rssi0;
        pvt_wifiposdata->probedata[index].rssi[1] = wifiposdesc->rssi1;
        pvt_wifiposdata->probedata[index].rssi[2] = wifiposdesc->rssi2;
        //ifiposdata->tx_index = index + 1;
        pvt_wifiposdata->proc_index = index;
        size = MEM_ALIGN(wifiposdesc->txrxchain_mask);
        memcpy(pvt_wifiposdata->probedata[index].type1_payld, wifiposdesc->hdump,
                                       (324));
    }
    else {
        printk("NEED TODO:Please stop, need to change state by sending empty response \n ");
        spin_unlock(&pvt_wifiposdata->probedata_lock);
        spin_unlock(&pvt_wifiposdata->lock);
        vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL, 0);
        return 0;
    }

	if (pvt_wifiposdata->probedata[pvt_wifiposdata->proc_index].valid) {
		pvt_wifiposdata->proc_index++;
        schedule_work(&pvt_wifiposdata->cleanprobedata_work);
		if (pvt_wifiposdata->proc_index >= ATH_WIFIPOS_MAX_CONCURRENT_PROBES) {
			pvt_wifiposdata->proc_index = 0;
		}
	}
	spin_unlock(&pvt_wifiposdata->probedata_lock);
	spin_unlock(&pvt_wifiposdata->lock);
	return 0;
}
/* Function     : ieee80211_update_wifipos_stats
 * Arguments    : ieee80211_wifiposdesc_t
 * Functionality: Itrated thought the array of received TOD/TOA and look for the
 *                empty place to update new recieved TOA/TOD
 * Return       : 0: Success, -1: failure
 */
int ieee80211_update_wifipos_stats(ieee80211_wifiposdesc_t * wifiposdesc)
{
    struct ieee80211com *ic = NULL;
    struct ieee80211vap *vap = NULL, *tvap = NULL;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	ieee80211_wifipos_reqdata_t *reqdata = NULL;
	u_int8_t expected_rx_pkt = 0x00;
	int index = 0;
	int count = 0, found_req = 0;

    int i;

    if (!wifiposenable)
        return -1;

    printk("%s: %d: wifiposdesc->flags(0x%x)\n", __FUNCTION__, __LINE__, wifiposdesc->flags);

    //TBD, get the VAP
    for (i = 0; i < num_global_scn; i ++) {
        ic = &global_scn[i]->sc_ic;

        if (!TAILQ_EMPTY(&ic->ic_vaps)) {
            TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
                if (vap->iv_wifipos && vap->iv_wifipos->pvt_wifiposdata) {
                    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;
                    break;
                }
            }
        }
    }
    if ((pvt_wifiposdata != NULL) && (vap != NULL) && (ic != NULL)) {
	spin_lock(&pvt_wifiposdata->lock);
	/*Search for request entry in request data store */
    // We are using only NULL/QOS NULL data.
    expected_rx_pkt = FC0_ACK_PKT;

	spin_lock(&pvt_wifiposdata->probedata_lock);
	if ((wifiposdesc->flags & ATH_WIFIPOS_TX_UPDATE)) {
    	for (count = 0; count < ATH_WIFIPOS_MAX_CONCURRENT_REQUESTS; count++) {
	    	reqdata = &pvt_wifiposdata->reqdata[count];
            if (reqdata == NULL)
                continue;
		    if (reqdata->request_id == wifiposdesc->request_id
                    && reqdata->state != IEEE80211_WIFIPOS_RQSTSTATE_IDLE
                    && IEEE80211_ADDR_EQ(reqdata->sta_mac_addr,
                            wifiposdesc->sta_mac_addr)){
                found_req = 1;
			    break;
            }
	    }
    	if (found_req  == 0) {
printk("%s: %d: reqdata Not Found\n", __FUNCTION__, __LINE__);
            spin_unlock(&pvt_wifiposdata->probedata_lock);
	        spin_unlock(&pvt_wifiposdata->lock);
		    return -1;
        }

		if (wifiposdesc->flags & ATH_WIFIPOS_TX_STATUS) {
			ieee80211_wifipos_do_framelength_correction_in_tod(pvt_wifiposdata, wifiposdesc, reqdata);
			index = find_matching_node(pvt_wifiposdata, wifiposdesc, TX_DESC);
			memcpy(pvt_wifiposdata->probedata[pvt_wifiposdata->tx_index].sta_mac_addr,
                    wifiposdesc->sta_mac_addr,
			        ETH_ALEN);
			pvt_wifiposdata->probedata[pvt_wifiposdata->tx_index].request_id =
                    wifiposdesc->request_id;
			pvt_wifiposdata->probedata[pvt_wifiposdata->tx_index].no_of_retries =
                    wifiposdesc->retries;
			if (index >= 0) {
printk("%s: %d: TX_STATUS matched, idx(%d) tod(%u)\n", __FUNCTION__, __LINE__,
        index, wifiposdesc->tod);
				pvt_wifiposdata->probedata[index].tod = wifiposdesc->tod;
				pvt_wifiposdata->probedata[index].valid = WIFIPOS_TRUE;
				pvt_wifiposdata->tx_index = index + 1;
				pvt_wifiposdata->proc_index = index;
			} else {
printk("%s: %d: TX_STATUS Not matched, idx(%d) tod(%u)\n", __FUNCTION__, __LINE__,
        pvt_wifiposdata->tx_index, wifiposdesc->tod);
				pvt_wifiposdata->probedata[pvt_wifiposdata->tx_index].tod =
                        wifiposdesc->tod;
				pvt_wifiposdata->tx_index = pvt_wifiposdata->tx_index + 1;
			}
			if (pvt_wifiposdata->tx_index >= ATH_WIFIPOS_MAX_CONCURRENT_PROBES) {
				pvt_wifiposdata->tx_index = 0;
			}
            WIFIPOS_DPRINTK("ReqId: %x,  T: %x, TOD: %x \n",
                    reqdata->request_id,
                    ic->ic_get_TSF32(ic),
                    wifiposdesc->tod);
            if (0 == reqdata->num_probe_rqst){
                if(reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC) {
                    if(pvt_wifiposdata->num_mrqst_inprogress > 0){
                        pvt_wifiposdata->num_mrqst_inprogress--;
                    } else
                        printk("%s[%d]: Error ReqId: %d!!!!!!\n",
                                __func__, __LINE__,
                                reqdata->request_id);

                    reqdata->state = IEEE80211_WIFIPOS_RQSTSTATE_IDLE;
                    if(0 == pvt_wifiposdata->num_mrqst_inprogress){
    			        OS_CANCEL_TIMER(&pvt_wifiposdata->wifipos_cleanup_timer);
   		        	    vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL,0);
                    }
                } else if (reqdata->state == IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC) {
                    OS_CANCEL_TIMER (&pvt_wifiposdata->wifipos_cctimer);
			        vap->iv_wifipos->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL,0);
                }
            }

        } else {
            int emptydesc_cnt = 0;

			WIFIPOS_DPRINTK("WifiPositioning:Tx Status flag not set\n");
            /* QUIPS-342 changes
            if(reqdata->num_probe_rqst > 0)
    			reqdata->num_probe_rqst--;
            */
	        spin_unlock(&pvt_wifiposdata->lock);
            WIFIPOS_DPRINTK("%s[%d]: ReqId: %x,  xmit_tstamp: %x \n",
                    __func__, __LINE__,
                    reqdata->request_id,
                    ic->ic_get_TSF32(ic));

            spin_lock(&pvt_wifiposdata->emptydesc_lock);
            if (pvt_wifiposdata->emptydesc_ridx <= pvt_wifiposdata->emptydesc_widx) {
                emptydesc_cnt = (ATH_WIFIPOS_MAX_CONCURRENT_EMPTY_RESP - 1) -
                    (pvt_wifiposdata->emptydesc_widx - pvt_wifiposdata->emptydesc_ridx);
            } else {
                emptydesc_cnt =
                    (pvt_wifiposdata->emptydesc_ridx - pvt_wifiposdata->emptydesc_widx) - 1;
            }

            if (emptydesc_cnt > 0) {
                printk("%s: %d: sched emptyresp_work\n", __FUNCTION__, __LINE__);
                memcpy((void *)(&pvt_wifiposdata->emptydesc[pvt_wifiposdata->emptydesc_widx]),
                        (void *)wifiposdesc,
                        sizeof(ieee80211_wifiposdesc_t));

                if ( ++ pvt_wifiposdata->emptydesc_widx >= ATH_WIFIPOS_MAX_CONCURRENT_EMPTY_RESP)
                    pvt_wifiposdata->emptydesc_widx = 0;

                schedule_work(&pvt_wifiposdata->emptyresp_work);
            } else {
                printk("%s: %d: emptydesc queue is full\n", __FUNCTION__, __LINE__);
            }
            spin_unlock(&pvt_wifiposdata->emptydesc_lock);
            spin_unlock(&pvt_wifiposdata->probedata_lock);
            return -1;
		}
	} else {
		if (wifiposdesc->rx_pkt_type == expected_rx_pkt) {
            u_int8_t numRxChains =
                    ieee80211_mask2numchains[(wifiposdesc->txrxchain_mask & (RX_CHAINMASK_HASH >>8))];
			ieee80211_wifipos_do_framelength_correction_in_toa(pvt_wifiposdata, wifiposdesc);
			index = find_matching_node(pvt_wifiposdata, wifiposdesc, RX_DESC);
            /* With NBP 1.1 wifiposdata->txrxchain_mask now contains MASK for TX and RX
            * TX:RX. This has to be changed to num of chains to get the size of
            * channel dump. The infrastructure is taken from exiting definition
            * in ath_main.c
            */
			if (index >= 0) {
printk("%s: %d: RX_STATUS matched, idx(%d) toa(%u)\n", __FUNCTION__, __LINE__,
        index, wifiposdesc->toa);
				pvt_wifiposdata->probedata[index].toa = wifiposdesc->toa;
				pvt_wifiposdata->probedata[index].no_of_chains = wifiposdesc->txrxchain_mask;
				pvt_wifiposdata->probedata[index].rate = wifiposdesc->rate;

				pvt_wifiposdata->probedata[index].rssi[0] = wifiposdesc->rssi0;
				pvt_wifiposdata->probedata[index].rssi[1] = wifiposdesc->rssi1;
				pvt_wifiposdata->probedata[index].rssi[2] = wifiposdesc->rssi2;
				pvt_wifiposdata->probedata[index].valid = WIFIPOS_TRUE;

				memcpy(pvt_wifiposdata->probedata[index].type1_payld, wifiposdesc->hdump,
				       ATH_DESC_CDUMP_SIZE(numRxChains));
				pvt_wifiposdata->rx_index = index + 1;
				pvt_wifiposdata->proc_index = index;

			} else {
printk("%s: %d: RX_STATUS Not matched, idx(%d) toa(%u)\n", __FUNCTION__, __LINE__,
        pvt_wifiposdata->rx_index, wifiposdesc->toa);

				pvt_wifiposdata->probedata[pvt_wifiposdata->rx_index].toa = wifiposdesc->toa;
				pvt_wifiposdata->probedata[pvt_wifiposdata->rx_index].no_of_chains = wifiposdesc->txrxchain_mask;
				pvt_wifiposdata->probedata[pvt_wifiposdata->rx_index].rate = wifiposdesc->rate;

				pvt_wifiposdata->probedata[pvt_wifiposdata->rx_index].rssi[0] = wifiposdesc->rssi0;
				pvt_wifiposdata->probedata[pvt_wifiposdata->rx_index].rssi[1] = wifiposdesc->rssi1;
				pvt_wifiposdata->probedata[pvt_wifiposdata->rx_index].rssi[2] = wifiposdesc->rssi2;
				memcpy(pvt_wifiposdata->probedata[pvt_wifiposdata->rx_index].type1_payld,
				       wifiposdesc->hdump,
				       ATH_DESC_CDUMP_SIZE(numRxChains));

				pvt_wifiposdata->rx_index = pvt_wifiposdata->rx_index + 1;
			}
            WIFIPOS_DPRINTK(" T: %x, TOA: %x \n", ic->ic_get_TSF32(ic), wifiposdesc->toa);
			if (pvt_wifiposdata->rx_index >= ATH_WIFIPOS_MAX_CONCURRENT_PROBES) {
				pvt_wifiposdata->rx_index = 0;
			}
		} else {
			WIFIPOS_DPRINTK("WifiPositioning:Rx for invalid packet\n");
		}
	}
	if (pvt_wifiposdata->probedata[pvt_wifiposdata->proc_index].valid
	        && pvt_wifiposdata->probedata[pvt_wifiposdata->proc_index].toa
	        && pvt_wifiposdata->probedata[pvt_wifiposdata->proc_index].tod) {
		pvt_wifiposdata->proc_index++;
		schedule_work(&pvt_wifiposdata->cleanprobedata_work);
		if (pvt_wifiposdata->proc_index >= ATH_WIFIPOS_MAX_CONCURRENT_PROBES) {
			pvt_wifiposdata->proc_index = 0;
		}
	}
	spin_unlock(&pvt_wifiposdata->probedata_lock);
	spin_unlock(&pvt_wifiposdata->lock);
    }
	return 0;
}



int ieee80211_wifipos_syncps_timer(void *arg)
{

    struct ieee80211vap *vap = (struct ieee80211vap *)(arg);
    ieee80211_wifipos_data_t *wifiposdata = vap->iv_wifipos;

	wifiposdata->fsm(vap, IEEE80211_WIFIPOS_EVENT_SYNCPS, (void *)(((ieee80211_wifipos_pvt_data_t *)(wifiposdata->pvt_wifiposdata))->syncpsreqdata),0);
	return 1;		//Do not rearm timer
}

static OS_TIMER_FUNC(ieee80211_wifipos_cleanup_timer)
{

    struct ieee80211vap *vap;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata;
    ieee80211_wifipos_data_t *wifiposdata;

    OS_GET_TIMER_ARG(vap, struct ieee80211vap *);

    wifiposdata = vap->iv_wifipos;
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)wifiposdata->pvt_wifiposdata;

    // If this timer is called this implies that the we have not
    // recieved the completion for either TSF RQST OR MRQST
    ieee80211_wifipos_update_allreqdata_state(pvt_wifiposdata, IEEE80211_WIFIPOS_RQSTSTATE_IDLE);
	wifiposdata->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL, 0);
}

static OS_TIMER_FUNC(ieee80211_wifipos_wakeup_timer)
{
    struct ieee80211vap *vap;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata;
    ieee80211_wifipos_data_t *wifiposdata;
    struct ieee80211_node *ni = NULL;
    struct node_powersave_queue *psq;
	int index;
	int ac = WME_AC_VO;
	int i = 0;
    u_int8_t need_resched = 0, wakeup_interval = 0;

    OS_GET_TIMER_ARG(vap, struct ieee80211vap *);

    wifiposdata = vap->iv_wifipos;
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)wifiposdata->pvt_wifiposdata;

	spin_lock(&pvt_wifiposdata->lock);

	if (pvt_wifiposdata->state == IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC
        || pvt_wifiposdata->state == IEEE80211_WIFIPOSMGR_STATE_WAIT) {

printk("%s: %d: \n", __FUNCTION__, __LINE__);

        OS_SET_TIMER(&pvt_wifiposdata->wifipos_wakeup_timer,MAX_CTS_TO_SELF_TIME );
    } else {

	    for (index = 0; index < ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA; index++) {
		    spin_lock(&pvt_wifiposdata->wakeup_lock);
    		if (IEEE80211_ADDR_IS_VALID(pvt_wifiposdata->wakeup[index].sta_mac_addr)) {
                need_resched = 1;
                wakeup_interval = pvt_wifiposdata->wakeup[index].wakeup_interval;
    			ni = ieee80211_find_node(&vap->iv_ic->ic_sta, pvt_wifiposdata->wakeup[index].sta_mac_addr);
                if (ni != NULL) {
	    		if ((IEEE80211_AID(ni->ni_associd) != 0)
                        || (ni->ni_vap == vap)) {

printk("\n%s: %d: timestamp(0x%08x) num_ka_frm(%d) wakeup_interval(%d) flags(%x)\n",
        __FUNCTION__, __LINE__,
        pvt_wifiposdata->wakeup[index].timestamp,
        pvt_wifiposdata->wakeup[index].num_ka_frm,
        pvt_wifiposdata->wakeup[index].wakeup_interval,
        pvt_wifiposdata->wakeup[index].flags);

		    		if ((pvt_wifiposdata->wakeup[index].timestamp != 0)
				            &&((CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP()) -
                                    pvt_wifiposdata->wakeup[index].timestamp)> MAX_TIMEOUT)) {
			    		printk(" current time %x, %x\n", CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP()),
					            pvt_wifiposdata->wakeup[index].timestamp);
	    				memset(&pvt_wifiposdata->wakeup[index].sta_mac_addr, 0, IEEE80211_ADDR_LEN);
				    	pvt_wifiposdata->wakeup[index].timestamp = 0;
                        --pvt_wifiposdata->wakeup_sta_count;
		    		}
			    	if (pvt_wifiposdata->wakeup_sta_count == 0) {
printk("%s: %d: \n", __FUNCTION__, __LINE__);
				    	vap->iv_wakeup = 0;
                        need_resched = 0;
    					OS_CANCEL_TIMER(&pvt_wifiposdata->wifipos_wakeup_timer);
			    	} else {
				    	ieee80211_wifipos_debuglog(IEEE80211_WIFIPOS_DEBUG_KA,__LINE__, (void *)ni);
	    				psq = IEEE80211_NODE_SAVEQ_MGMTQ(ni);
		    			ieee80211node_set_flag(ni,IEEE80211_NODE_WAKEUP);
                        if(pvt_wifiposdata->wakeup[index].flags & IEEE80211_KA_DONE) {
                            pvt_wifiposdata->wakeup[index].flags &= ~(IEEE80211_KA_DONE | IEEE80211_KA_SUCCESS);

					        if (!IEEE80211_NODE_SAVEQ_FULL(psq)) {
						        if (ni->ni_flags & IEEE80211_NODE_UAPSD) {
    	    					    for (i = WME_NUM_AC - 1; i >= 0; i --) {
		    	    					if (ni->ni_uapsd_ac_trigena[i]) {
					    	    			ac = i;
						    	    		break;
							    	    }
    							    }
printk("%s: %d: send QoS NULL as KA, ac(%d)\n", __FUNCTION__, __LINE__, ac);
    	    						ieee80211_send_qosnulldata_keepalive(ni, ac, 0, true);
		    	    			} else {
printk("%s: %d: send NULL Data as KA\n", __FUNCTION__, __LINE__);
			    	    			ieee80211_send_nulldata_keepalive(ni, 0, true);

                                }
                                // keeping track of the number of KA frames generated
                                pvt_wifiposdata->wakeup[index].num_ka_frm ++;
                            }
                        } else {
                            // Just writing it here. That we are skipping the KA frame for this
                            // time.
                        }
                    }
                    if(ni)
                        ieee80211_free_node(ni);
                } else {
                    // This is the case where we have a node in wakeup array and somehow
                    // this sta is kicked out and we still have this stale sta which needs
                    // to be deleted
	    		    memset(&pvt_wifiposdata->wakeup[index].sta_mac_addr, 0, IEEE80211_ADDR_LEN);
				    pvt_wifiposdata->wakeup[index].timestamp = 0;
			        -- pvt_wifiposdata->wakeup_sta_count;
                }
            }
            }

            spin_unlock(&pvt_wifiposdata->wakeup_lock);
        }

        if(need_resched && wakeup_interval)
            OS_SET_TIMER(&pvt_wifiposdata->wifipos_wakeup_timer, wakeup_interval);

    }
    spin_unlock(&pvt_wifiposdata->lock);
}

static OS_TIMER_FUNC(ieee80211_wifipos_cctimer)
{
	/* All the channel change and the other activities
	 * done in this function is taken to the below called
	 * function.
	 * Note: hc => home channel
	 */
    struct ieee80211vap *vap;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata;
    ieee80211_wifipos_data_t *wifiposdata;

    OS_GET_TIMER_ARG(vap, struct ieee80211vap *);
    wifiposdata = vap->iv_wifipos;
    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)wifiposdata->pvt_wifiposdata;

    if(pvt_wifiposdata->ccreqdata){
	    wifiposdata->fsm(vap, IEEE80211_WIFIPOS_EVENT_FINRQST, NULL, 0);
        }
}

int ieee80211_wifipos_vattach(struct ieee80211vap *vap)
{
    int index = 0;
	struct ieee80211com *ic = vap->iv_ic;
    ieee80211_wifipos_data_t *wifiposdate;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata = NULL;
	ieee80211_wifipos_reqdata_t *reqdata;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "%s: %d: Wifipos intialization starting\n", __FUNCTION__,
	       __LINE__);
    ic->ic_update_wifipos_stats = ieee80211_ol_update_wifipos_stats;
    ic->ic_update_ka_done = ieee80211_ol_update_ka_done;
    if (vap->iv_wifipos == NULL) {
#if WIFI_MEM_MANAGER_SUPPORT
        struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(ic);
        pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *) (wifi_kmem_allocation(scn->soc->soc_idx, 0, KM_WIFIPOS, sizeof(ieee80211_wifipos_pvt_data_t), GFP_KERNEL));
#else
        pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *) (OS_MALLOC(
                    ic->ic_osdev,
                    sizeof(ieee80211_wifipos_pvt_data_t),
                    GFP_KERNEL));
#endif
        if (!pvt_wifiposdata) {
            printk("%s: Unable to allocate memory \n", __func__);
            vap->iv_wifipos = NULL;
            return -ENOMEM;
        }

        OS_MEMZERO(pvt_wifiposdata, sizeof(ieee80211_wifipos_pvt_data_t));
        wifiposdate = &pvt_wifiposdata->wifiposdata;
        wifiposdate->pvt_wifiposdata = pvt_wifiposdata;
        wifiposdate->vap = vap;

        vap->iv_wifipos = wifiposdate;

        spin_lock_init(&pvt_wifiposdata->lock);
        spin_lock_init(&pvt_wifiposdata->probedata_lock);
        spin_lock_init(&pvt_wifiposdata->wakeup_lock);
        spin_lock_init(&pvt_wifiposdata->tsf_req_lock);
        spin_lock_init(&pvt_wifiposdata->emptydesc_lock);
        if (ic->rtt_sock != NULL)
        pvt_wifiposdata->wifipos_sock = ic->rtt_sock;
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "BRINGUP:pointer of pvt_wifiposdata is %pK and socket val is %pK and socket id is %d\n", pvt_wifiposdata, pvt_wifiposdata->wifipos_sock, pvt_wifiposdata->process_pid);
		pvt_wifiposdata->state = IEEE80211_WIFIPOSMGR_STATE_IDLE;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "Registered WIFIPOS netlink family");
#if (LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20))
		INIT_WORK(&pvt_wifiposdata->cleanprobedata_work, ieee80211_wifipos_cleanprobedata, NULL);
		INIT_WORK(&pvt_wifiposdata->resumeprobes_work, ieee80211_wifipos_resumeprobes, NULL);
        INIT_WORK(&pvt_wifiposdata->emptyresp_work, ieee80211_wifipos_emptyresp, NULL);
#else
		INIT_WORK(&pvt_wifiposdata->cleanprobedata_work, ieee80211_wifipos_cleanprobedata);
		INIT_WORK(&pvt_wifiposdata->resumeprobes_work, ieee80211_wifipos_resumeprobes);
        INIT_WORK(&pvt_wifiposdata->emptyresp_work, ieee80211_wifipos_emptyresp);
#endif
		OS_INIT_TIMER(ic->ic_osdev,
		        &pvt_wifiposdata->wifipos_cctimer,
		        ieee80211_wifipos_cctimer,
		        vap, QDF_TIMER_TYPE_WAKE_APPS);
		OS_INIT_TIMER(ic->ic_osdev,
                &pvt_wifiposdata->wifipos_wakeup_timer,
                ieee80211_wifipos_wakeup_timer,
                vap, QDF_TIMER_TYPE_WAKE_APPS);
		OS_INIT_TIMER(ic->ic_osdev,
                &pvt_wifiposdata->wifipos_cleanup_timer,
                ieee80211_wifipos_cleanup_timer,
                vap, QDF_TIMER_TYPE_WAKE_APPS);
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "%s: %d: Wifipos intialization work q & timer alloc complete\n",
		       __FUNCTION__, __LINE__);

		for (index = 0; index < ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA; index++) {
			memset(&pvt_wifiposdata->wakeup[index].sta_mac_addr, 0, IEEE80211_ADDR_LEN);
			pvt_wifiposdata->wakeup[index].timestamp = 0;
		}
		for (index = 0; index < ATH_WIFIPOS_MAX_CONCURRENT_TSF_AP; index++) {
			memset(&pvt_wifiposdata->tsfreq[index].assoc_ap_mac_addr, 0, IEEE80211_ADDR_LEN);
			pvt_wifiposdata->tsfreq[index].probe_ap_tsf = 0;
			pvt_wifiposdata->tsfreq[index].assoc_ap_tsf = 0;
			pvt_wifiposdata->tsfreq[index].diff_tsf = 0;
			pvt_wifiposdata->tsfreq[index].valid = 0;
		}
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "%s: %d: Wifipos intialization data store 1 alloc complete\n", __FUNCTION__, __LINE__);

	    vap->iv_wifipos->status_request =       ieee80211_wifipos_status_request;
            vap->iv_wifipos->cap_request =          ieee80211_wifipos_cap_request;

            vap->iv_wifipos->nlsend_status_resp =   ieee80211_wifipos_nlsend_status_resp;
            vap->iv_wifipos->nlsend_cap_resp =      ieee80211_wifipos_nlsend_cap_resp;
            vap->iv_wifipos->nlsend_tsf_resp =      ieee80211_wifipos_nlsend_tsf_resp;
            vap->iv_wifipos->nlsend_sleep_resp =    ieee80211_wifipos_nlsend_sleep_resp;
            vap->iv_wifipos->nlsend_wakeup_resp =   ieee80211_wifipos_nlsend_wakeup_resp;
            vap->iv_wifipos->nlsend_empty_resp  =   ieee80211_wifipos_nlsend_empty_resp;
            vap->iv_wifipos->nlsend_tsf_update =    ieee80211_wifipos_nlsend_tsf_update;

		if(ic->ic_is_mode_offload(ic)) {
            vap->iv_wifipos->fsm =                  ieee80211_ol_wifipos_fsm;
            vap->iv_wifipos->nlsend_probe_resp =    ieee80211_ol_wifipos_nlsend_probe_resp;
            vap->iv_wifipos->wakeup_request =       ieee80211_ol_wifipos_wakeup_request;
            vap->iv_wifipos->sleep_request =        ieee80211_ol_wifipos_sleep_request;
		}
		else {
			vap->iv_wifipos->fsm =                  ieee80211_wifipos_fsm;
			vap->iv_wifipos->nlsend_probe_resp =    ieee80211_wifipos_nlsend_probe_resp;
			vap->iv_wifipos->xmitprobe = 		ieee80211_wifipos_xmitprobe;
			vap->iv_wifipos->wakeup_request =       ieee80211_wifipos_wakeup_request;
			vap->iv_wifipos->sleep_request =        ieee80211_wifipos_sleep_request;
                        vap->iv_wifipos->xmitrtt3       =       NULL;
                        vap->iv_wifipos->ol_wakeup_request =    NULL;
                        vap->iv_wifipos->lci_set =              NULL;
                        vap->iv_wifipos->lcr_set =              NULL;
		}
            vap->iv_wifipos->xmittsfrequest =       ieee80211_wifipos_xmittsfrequest;
	}

    if(pvt_wifiposdata != NULL) {
        for (index = 0; index < ATH_WIFIPOS_MAX_CONCURRENT_PROBES; index++) {
            pvt_wifiposdata->probedata[index].tod = 0;
            pvt_wifiposdata->probedata[index].toa = 0;
            pvt_wifiposdata->probedata[index].valid = WIFIPOS_FALSE;
        }
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "%s: %d: Wifipos intialization data store 2 alloc complete\n", __FUNCTION__, __LINE__);

        for (index = 0; index < ATH_WIFIPOS_MAX_CONCURRENT_REQUESTS; index++) {
            reqdata = &(pvt_wifiposdata->reqdata[index]);
            reqdata->pstimer = OS_MALLOC(ic->ic_osdev, sizeof(struct ath_timer), GFP_KERNEL);

            if (NULL == reqdata->pstimer)
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "%s[%d]: pstimer out of memory \n", __func__, __LINE__);

	    reqdata->hc_channel = ic->ic_curchan->ic_ieee;
	    reqdata->ni_tmp_sta = NULL;
        }
    }
	//TBD: Initialize timing stats structure
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "%s: %d: Wifipos intialization complete with ic:%pK\n", __FUNCTION__, __LINE__,ic);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "%s: %d: vap addr(%02x:%02x:%02x:%02x:%02x:%02x) idx(%d)\n", __FUNCTION__, __LINE__,
        vap->iv_myaddr[0],
        vap->iv_myaddr[1],
        vap->iv_myaddr[2],
        vap->iv_myaddr[3],
        vap->iv_myaddr[4],
        vap->iv_myaddr[5],
        vap->iv_unit);

	return 0;
}

int ieee80211_wifipos_vdetach(struct ieee80211vap *vap)
{
    ieee80211_wifipos_data_t *wifiposdata = vap->iv_wifipos;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata;
	ieee80211_wifipos_reqdata_t *reqdata;
	int count = 0;
#if WIFI_MEM_MANAGER_SUPPORT
	struct ol_ath_softc_net80211 *scn = OL_ATH_SOFTC_NET80211(vap->iv_ic);
#endif
	update_bit_pos = false;

	if (wifiposdata) {
        pvt_wifiposdata = wifiposdata->pvt_wifiposdata;
		if (pvt_wifiposdata->wifipos_sock) {
			IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "Release the socket");
			pvt_wifiposdata->wifipos_sock = NULL;
		}
		OS_FREE_TIMER(&pvt_wifiposdata->wifipos_cctimer);
		OS_FREE_TIMER(&pvt_wifiposdata->wifipos_wakeup_timer);
		OS_FREE_TIMER(&pvt_wifiposdata->wifipos_cleanup_timer);

		for (count = 0; count < ATH_WIFIPOS_MAX_CONCURRENT_REQUESTS; count++) {
			reqdata = &(pvt_wifiposdata->reqdata[count]);
			ath_free_timer(reqdata->pstimer);	//Will not dealloc if not initialized
			OS_FREE(reqdata->pstimer);
		}

#if WIFI_MEM_MANAGER_SUPPORT
        wifi_kmem_free(scn->soc->soc_idx, 0, KM_WIFIPOS,(void *)pvt_wifiposdata);
#else
		OS_FREE(pvt_wifiposdata);
#endif
		vap->iv_wifipos = NULL;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "UnRegistered WIFI positioning netlink family");
	}
	return 0;
}

int ieee80211_wifipos_set_txcorrection(wlan_if_t vaphandle, unsigned int corr)
{
	struct ieee80211vap *vap = vaphandle;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata;

    if ((!wifiposenable) || (NULL == vap->iv_wifipos))
    {
        printk("wifipos not supported\n");
        return 0;
    }

    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;

	if (pvt_wifiposdata) {
		pvt_wifiposdata->txcorrection = corr;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
				  "WifiPositioning:Setting txcorrection = %d\n",
				  pvt_wifiposdata->txcorrection);
	}
	return 0;
}

int ieee80211_wifipos_set_rxcorrection(wlan_if_t vaphandle, unsigned int corr)
{
	struct ieee80211vap *vap = vaphandle;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata;

    if ((!wifiposenable) || (NULL == vap->iv_wifipos))
    {
        printk("wifipos not supported\n");
        return 0;
    }

    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;

	if (pvt_wifiposdata) {
		pvt_wifiposdata->rxcorrection = corr;
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
				  "WifiPositioning:Setting rxcorrection = %d\n",
				  pvt_wifiposdata->rxcorrection);
	}
	return 0;
}

unsigned int ieee80211_wifipos_get_rxcorrection(wlan_if_t vaphandle)
{
	struct ieee80211vap *vap = vaphandle;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata;

    if ((!wifiposenable) || (NULL == vap->iv_wifipos))
    {
        printk("wifipos not supported\n");
        return 0;
    }

    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;

	if (pvt_wifiposdata)
		return pvt_wifiposdata->rxcorrection;
	return 0;
}

unsigned int ieee80211_wifipos_get_txcorrection(wlan_if_t vaphandle)
{
	struct ieee80211vap *vap = vaphandle;
    ieee80211_wifipos_pvt_data_t *pvt_wifiposdata;

    if ((!wifiposenable) || (NULL == vap->iv_wifipos))
    {
        printk("wifipos not supported\n");
        return 0;
    }

    pvt_wifiposdata = (ieee80211_wifipos_pvt_data_t *)vap->iv_wifipos->pvt_wifiposdata;

	if (pvt_wifiposdata)
		return pvt_wifiposdata->txcorrection;
	return 0;
}

static void ieee80211_wifipos_do_framelength_correction_in_tod(
                            ieee80211_wifipos_pvt_data_t *pvt_wifiposdata,
                            ieee80211_wifiposdesc_t *wifiposdesc,
                            ieee80211_wifipos_reqdata_t *reqdata)
{
	unsigned int txframe_bitlen = 0, txframe_cycles = 0;
	unsigned int dbps = 0;
	unsigned int padbits = 0;
	txframe_bitlen =
	    IEEE80211_HDR_SERVICE_BITS_LEN +
	    packetlength[reqdata->pkt_type] * 8 + IEEE80211_HDR_TAIL_BITS_LEN;
	dbps = get_dbps_by_rate(reqdata->rateset >> 24);
	if (txframe_bitlen % dbps) {
		padbits = dbps - (txframe_bitlen % dbps);
		txframe_bitlen += padbits;
	}
	txframe_cycles =
	    ((txframe_bitlen / dbps) * SYMBOL_DURATION_MICRO_SEC + L_LTF +
	     L_STF + L_SIG) * NO_OF_CLK_CYCLES_PER_MICROSEC;

	wifiposdesc->tod =
	    wifiposdesc->tod + txframe_cycles +
	    pvt_wifiposdata->txcorrection;
}

static void ieee80211_wifipos_do_framelength_correction_in_toa(
                            ieee80211_wifipos_pvt_data_t *pvt_wifiposdata,
                            ieee80211_wifiposdesc_t *wifiposdesc)
{
	unsigned int rxframe_bitlen = 0, rxframe_cycles = 0;
	unsigned int dbps = 0;
	unsigned int padbits = 0;

	dbps = get_dbps_by_rate(wifiposdesc->rate);
	rxframe_bitlen =
	    IEEE80211_HDR_SERVICE_BITS_LEN + ACK_PKT_BYTE_SIZE * 8 +
	    IEEE80211_HDR_TAIL_BITS_LEN;
	padbits = 0;
	if (rxframe_bitlen % dbps) {
		padbits = dbps - (rxframe_bitlen % dbps);
		rxframe_bitlen += padbits;
	}
	rxframe_cycles =
	    ((rxframe_bitlen / dbps) * SYMBOL_DURATION_MICRO_SEC + L_LTF +
	     L_STF + L_SIG) * NO_OF_CLK_CYCLES_PER_MICROSEC;

	wifiposdesc->toa = wifiposdesc->toa - rxframe_cycles - pvt_wifiposdata->rxcorrection;
}

/* Get the beacon interval in microseconds */
static INLINE u_int32_t get_beacon_interval(wlan_if_t vap)
{
	/* The beacon interval is in terms of Time Units */
	return (vap->iv_bss->ni_intval * TIME_UNIT_TO_MICROSEC);
}

/*
* get the nextbtt for the given vap.
* for the second STA vap the tsf offset is used for calculating tbtt.
*/
static u_int64_t ieee80211_wifipos_get_next_tbtt_tsf_64(wlan_if_t vap)
{
	struct ieee80211com *ic = vap->iv_ic;
	u_int64_t curr_tsf64, nexttbtt_tsf64;
	u_int32_t bintval;	/* beacon interval in micro seconds */
	ieee80211_vap_tsf_offset tsf_offset_info;

	curr_tsf64 = ic->ic_get_TSF64(ic);
	/* calculate the next tbtt */

	/* tsf offset from our HW tsf */
	ieee80211_vap_get_tsf_offset(vap, &tsf_offset_info);
	/* adjust tsf to the clock of the GO */
	if (tsf_offset_info.offset_negative) {
		curr_tsf64 -= tsf_offset_info.offset;
	} else {
		curr_tsf64 += tsf_offset_info.offset;
	}

	bintval = get_beacon_interval(vap);

	nexttbtt_tsf64 = curr_tsf64 + bintval;

	nexttbtt_tsf64 = nexttbtt_tsf64 - OS_MOD64_TBTT_OFFSET(nexttbtt_tsf64, bintval);

	if ((nexttbtt_tsf64 - curr_tsf64) < MIN_TSF_TIMER_TIME) {	/* if the immediate next tbtt is too close go to next one */
		nexttbtt_tsf64 += bintval;
	}

	/* adjust tsf back to the clock of our HW */
	if (tsf_offset_info.offset_negative) {
		nexttbtt_tsf64 += tsf_offset_info.offset;
	} else {
		nexttbtt_tsf64 -= tsf_offset_info.offset;
	}

	return nexttbtt_tsf64;
}
static void sta_list(void *arg, struct ieee80211_node *ni)
{
	struct nsp_station_info sta_info;
	wlan_rssi_info rssi_info;
	ieee80211_wifipos_stabuff_t *stabuf =
	    (ieee80211_wifipos_stabuff_t *) arg;
	if(IEEE80211_AID(ni->ni_associd) == 0)
		return;

    sta_info.vap_index = stabuf->vap_index;
    IEEE80211_ADDR_COPY(sta_info.sta_mac_addr, ni->ni_macaddr);
    if (wlan_node_getrssi(ni, &rssi_info, WLAN_RSSI_RX) == 0) {
        sta_info.rssi = rssi_info.avg_rssi;
    }
    sta_info.info = 0x1;
    sta_info.tstamp = ni->last_rx_desc_tsf;
    ieee80211_print_sta_info(ni->ni_vap, &sta_info);
    memcpy(stabuf->stainfo + stabuf->node_count, &sta_info, SINFO_LEN);
    stabuf->node_count += 1;
}

static unsigned int get_dbps_by_rate(int rate)
{
	int mode = (rate & 0x000000F0) >> 4;
	int index = (rate & 0x0000000F);
	unsigned int dbps = 0;

	switch (mode) {
		//11 G
	case 0:
		if (index >= 0x08 && index <= 0x0f)
			dbps = ieee80211_wifipos_databits_per_symbol[index - 0x08];
		else
			WIFIPOS_DPRINTK
			    ("WifiPositioning:Invalid get_dbps_by_rate() rate %d\n",
			     rate);
		break;
		//11 B TODO: Identify proper values and fill in the structure
	case 1:
		if (index == 0x08 || index == 0x09 || index == 0x0a
		    || index == 0x0b)
			dbps = ieee80211_wifipos_databits_per_symbol_11b[index - 0x08];
		else
			WIFIPOS_DPRINTK
			    ("WifiPositioning:Invalid get_dbps_by_rate() rate %d\n",
			     rate);
		break;
		//11 N
	case 8:
		if (index >= 0x0 && index < 0x0e)
			dbps = ieee80211_wifipos_databits_per_symbol_11n[index];
		else
			WIFIPOS_DPRINTK
			    ("WifiPositioning:Invalid get_dbps_by_rate() rate %d\n",
			     rate);
		break;

	default:
		WIFIPOS_DPRINTK
		    ("WifiPositioning:Invalid get_dbps_by_rate() mode %d rate %d\n",
		     mode, rate);
		break;
	}
	return dbps;
}

int ieee80211_wifipos_calccctimingstats(ieee80211_wifipos_pvt_data_t *pvt_wifiposdata)
{

	//Values are stored in global structure pvt_wifiposdata->cctiming
	//Variable names here are redefined. TBD: Add structure names

	/* Calculating the average time over 1000 channel changes */
	u_int32_t wpnt_avg = 0, wlsct0_avg = 0, wgpt_avg = 0, wlsct1_avg = 0;
	u_int32_t wcnt_avg = 0, wrtxqst_avg = 0, wupnt_avg = 0, CTStet_avg = 0;

	int i = 0;
	if (cnt_cc == 1000) {
		for (i = 0; i < 1000; i++) {
			wpnt_avg += wpnt[i];
			wlsct0_avg += wlsct0[i];
			wgpt_avg += wgpt[i];
			wlsct1_avg += wlsct1[i];
			wcnt_avg += wcnt[i];
			wrtxqst_avg += wrtxqst[i];
			wupnt_avg += wupnt[i];
			CTStet_avg += CTStet[i];
		}
		wpnt_avg = wpnt_avg / 1000;
		wlsct0_avg = wlsct0_avg / 1000;
		wgpt_avg = wgpt_avg / 1000;
		wlsct1_avg = wlsct1_avg / 1000;
		wcnt_avg = wcnt_avg / 1000;
		wrtxqst_avg = wrtxqst_avg / 1000;
		wupnt_avg = wupnt_avg / 1000;
		CTStet_avg = CTStet_avg / 1000;
		avg_txframe_pct = avg_txframe_pct / 1000;
		avg_rxframe_pct = avg_rxframe_pct / 1000;
		avg_rxclear_pct = avg_rxclear_pct / 1000;
		printk(KERN_DEBUG
		       "%s:\nAvg_node_pause_time: %d, Avg_node_unpasue_time: %d \nAvg_cc_to_oc_time: %d, Avg_cc_to_hc_time: %d \nAvg_probe_gen_time: %d\nAvg_node_cleanup_time: %d, Avg_reap_txqs_time: %d\nAvg_CTS_expire_time: %d\n, AvgRxClearpct: %u, AvgRxFramePct: %u, AvgTxFramepct: %u \n",
		       __func__, wpnt_avg, wupnt_avg, wlsct0_avg, wlsct1_avg,
		       wgpt_avg, wcnt_avg, wrtxqst_avg, CTStet_avg,
		       avg_rxclear_pct, avg_rxframe_pct, avg_txframe_pct);

		cnt_cc = 0;
		avg_txframe_pct = 0;
		avg_rxframe_pct = 0;
		avg_rxclear_pct = 0;
	}
	return 0;
}

//TBD: Remove and use debugging infrastructure
//TBD: Remove the prints, switch to the wifipositioing debug infrastucture
void ieee80211_print_tsfrqst(struct ieee80211vap *vap, struct nsp_tsf_req *tsfrqst)
{
	unsigned char *s;
	s = (char *)tsfrqst->ap_mac_addr;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:--------------TSF_REQUEST-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request ID            :%x\n",
			  tsfrqst->request_id);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:AP MAC addr          :%02x:%02x:%02x:%02x:%02x:%02x\n",
			  s[0], s[1], s[2], s[3], s[4], s[5]);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Channel               :%x\n",
			  tsfrqst->channel);
}

void ieee80211_print_tsfresp(struct ieee80211vap *vap, struct nsp_tsf_resp *tsfresp)
{
	unsigned char *s;
	s = (unsigned char *)tsfresp->ap_mac_addr;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:---------------TSF_RESPONSE-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request ID            :%x\n",
			  tsfresp->request_id);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:MAC addr              :%x:%x:%x:%x:%x:%x\n",
			  s[0], s[1], s[2], s[3], s[4], s[5]);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Associated AP's tsf   :%llx\n",
			  tsfresp->assoc_tsf);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Probing AP's tsf      :%llx\n",
			  tsfresp->prob_tsf);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Result                :%x\n",
			  tsfresp->result);
}

void ieee80211_print_mrqst(struct ieee80211vap *vap, struct nsp_mrqst *mrqst)
{
	unsigned char *s = (char *)mrqst->sta_mac_addr;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:----------------TYPE1_MEASUREMENT_REQUEST-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request ID            :%x\n",
			  mrqst->request_id);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Mode                  :%x\n",
			  mrqst->mode);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:STA MAC addr          :%02x:%02x:%02x:%02x:%02x:%02x\n",
			  s[0], s[1], s[2], s[3], s[4], s[5]);
	s = (unsigned char *)mrqst->spoof_mac_addr;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Spoof MAC addr        :%02x:%02x:%02x:%02x:%02x:%02x\n",
			  s[0], s[1], s[2], s[3], s[4], s[5]);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:STA Info              :%x\n",
			  mrqst->sta_info);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Channel               :%x\n",
			  mrqst->channel);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Number of measurements:%x\n",
			  mrqst->no_of_measurements);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Transmit Rate         :%x\n",
			  mrqst->transmit_rate);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Timeout               :%x\n",
			  mrqst->timeout);
}

void ieee80211_print_type1_payld_resp(struct ieee80211vap *vap,
                            struct nsp_type1_resp *type1_payld_resp)
{
#ifdef IEEE80211_DEBUG
	unsigned int i, tmp;
#endif
    u_int8_t numRxChains = ieee80211_mask2numchains[
                    (type1_payld_resp->no_of_chains & (RX_CHAINMASK_HASH >>8))];

	unsigned int type1_payld_size, chain_cnt;
	type1_payld_size = ATH_DESC_CDUMP_SIZE(numRxChains);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:TOA                   :%llx\n",
			  type1_payld_resp->toa);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:TOD                   :%llx\n",
			  type1_payld_resp->tod);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:RTT                   :%llx\n",
			  type1_payld_resp->toa - type1_payld_resp->tod);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Send rate             :%x\n",
			  type1_payld_resp->send_rate);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Receive rate          :%x\n",
			  type1_payld_resp->receive_rate);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:No of chains          :%x\n",
			  numRxChains);
	for (chain_cnt = 0; chain_cnt < numRxChains ;
	     chain_cnt++)
		IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
				  "WifiPositioning:RSSI for CHAIN %d      :%x\n",
				  chain_cnt + 1,
				  type1_payld_resp->rssi[chain_cnt]);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:TYPE1PAYLOAD          :%d\n",
			  type1_payld_size);
#ifdef IEEE80211_DEBUG
	for (i = 0; i <= type1_payld_size / 4; i++) {
		if (i % 7 == 0)
			WIFIPOS_DPRINTK("\n");
		tmp = *(((int *)type1_payld_resp->type1_payld) + i);
		/* convert the channel dump to little endian format */
		tmp =
		    ((tmp << 24) | ((tmp << 8) & 0x00ff0000) |
		     ((tmp >> 8) & 0x0000ff00) | (tmp >> 24));
		WIFIPOS_DPRINTK("%x ", tmp);
	}
#endif
}
void ieee80211_print_slresp(struct ieee80211vap *vap, struct nsp_sleep_resp *slresp)
{
	unsigned char *s;
	s = (char *)slresp->sta_mac_addr;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:---------------SLEEP_RESPONSE-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request ID            :%x\n",
			  slresp->request_id);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:MAC addr              :%x:%x:%x:%x:%x:%x\n",
			  s[0], s[1], s[2], s[3], s[4], s[5]);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Result                :%x\n",
			  slresp->result);
}

void ieee80211_print_wresp(struct ieee80211vap *vap, struct nsp_wakeup_resp *wresp)
{
	unsigned char *s;
	s = (char *)wresp->sta_mac_addr;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:---------------WAKEUP_RESPONSE-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request ID            :%x\n",
			  wresp->request_id);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:MAC addr              :%x:%x:%x:%x:%x:%x\n",
			  s[0], s[1], s[2], s[3], s[4], s[5]);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Result                :%x\n",
			  wresp->result);
}
void ieee80211_print_wrqst(struct ieee80211vap *vap, struct nsp_wakeup_req *wreq)
{
	unsigned char *s;
	s = (char *)wreq->sta_mac_addr;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:---------------WAKEUP_REQUEST-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request ID            :%x\n",
			  wreq->request_id);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:MAC addr              :%x:%x:%x:%x:%x:%x\n",
			  s[0], s[1], s[2], s[3], s[4], s[5]);
}
void ieee80211_print_slrqst(struct ieee80211vap *vap, struct nsp_sleep_req *sreq)
{
	unsigned char *s;
	s = (char *)sreq->sta_mac_addr;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:---------------SLEEP_REQUEST-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request ID            :%x\n",
			  sreq->request_id);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:MAC addr              :%x:%x:%x:%x:%x:%x\n",
			  s[0], s[1], s[2], s[3], s[4], s[5]);
}

void ieee80211_print_srqst(struct ieee80211vap *vap, struct nsp_sreq *sreq)
{
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:---------------STATUS_REQUEST-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request ID            :%x\n",
			  sreq->request_id);
}

void ieee80211_print_crqst(struct ieee80211vap *vap, struct nsp_cap_req *creq)
{
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:---------------CAPABILITY_REQUEST-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request ID            :%x\n",
			  creq->request_id);
}

void ieee80211_print_cresp(struct ieee80211vap *vap, struct nsp_cap_resp *cresp)
{
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:---------------CAPABILITY_RESPONSE-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request ID            :%x\n",
			  cresp->request_id);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Band                  :%x\n",
			  cresp->band);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Positioning           :%x\n",
			  cresp->positioning);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:ChainMask (Tx:Rx)     :%x\n",
			  cresp->curr_chain_masks);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:SW Version            :%x\n",
			  cresp->sw_version);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:HW Version            :%x\n",
			  cresp->hw_version);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Clock Frequency in HZ :%d\n",
			  cresp->clk_freq);
}

void ieee80211_print_sresp(struct ieee80211vap *vap, struct nsp_sresp *sresp)
{
//    unsigned char *s;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:---------------STATUS_RESPONSE-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request ID            :%x\n",
			  sresp->request_id);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:No of stations        :%x\n",
			  sresp->no_of_stations);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request timestamp     :%llxx\n",
			  sresp->req_tstamp);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Response timestamp    :%llxx\n",
			  sresp->resp_tstamp);
/*    s = (char *)sresp->ap1_mac;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:AP1 Mac address    :%x %x %x %x %x %x\n",s[0],s[1],s[2],s[3],s[4],s[5]);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:AP1 channel        :%x\n",sresp->ap1_channel);
    s = (char *)sresp->ap2_mac;
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:AP2 Mac address    :%x %x %x %x %x %x\n",s[0],s[1],s[2],s[3],s[4],s[5]);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:AP2 channel        :%x\n",sresp->ap2_hannel);
    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS, "WifiPositioning:SSID               :%s\n",sresp->ssid); */
}

void ieee80211_print_sta_info(struct ieee80211vap *vap,
                            struct nsp_station_info *sta_info)
{
	unsigned char *s;
	s = (char *)sta_info->sta_mac_addr;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:---------------STATUS_INFO-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:STA Mac address    :%x %x %x %x %x %x\n",
			  s[0], s[1], s[2], s[3], s[4], s[5]);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:VAP Index          :%x\n",
			  sta_info->vap_index);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:STA last timestamp :%llx\n",
			  sta_info->tstamp);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Station info       :%x\n",
			  sta_info->info);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Station RSSI       :%x\n",
			  sta_info->rssi);

}

void ieee80211_print_mresp(struct ieee80211vap *vap, struct nsp_mresp *mresp)
{
	unsigned char *s;
	s = (char *)mresp->sta_mac_addr;
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:---------------TYPE1 MEASUREMENT_RESPONSE-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request ID            :%x\n",
			  mresp->request_id);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:MAC addr              :%x:%x:%x:%x:%x:%x\n",
			  s[0], s[1], s[2], s[3], s[4], s[5]);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:No of responses       :%x\n",
			  mresp->no_of_responses);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Request timestamp     :%llx\n",
			  mresp->req_tstamp);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Response timestamp    :%llx\n",
			  mresp->resp_tstamp);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Result                :%x\n",
			  mresp->result);
}

void ieee80211_print_nsphdr(struct ieee80211vap *vap, struct nsp_header *nsp_hdr)
{
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:---------------NSP_HEADER-----------------\n");
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Start of Frame Delimiter :%x\n",
			  nsp_hdr->SFD);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Version                  :%x\n",
			  nsp_hdr->version);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Frame Type               :%x\n",
			  nsp_hdr->frame_type);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
			  "WifiPositioning:Frame Length             :%x\n",
			  nsp_hdr->frame_length);
}

//TBD: Move to seperate debug log file
#if  0 //IEEE80211_WIFIPOSDEBUG

struct ieee80211_wifipos_debug {
	u_int32_t ts;
	int32_t index;
	u_int32_t event;
	void *eventdata;
} ieee80211_wifipos_debug_g[IEEE80211_WIFIPOS_DEBUGLOGSIZE];
static int32_t ieee80211_wifipos_debug_index = 0;

struct ieee80211_wifipos_debug_value {
	u_int32_t location;
	int32_t value;
} ieee80211_wifipos_debug_value_g[IEEE80211_WIFIPOS_DEBUGLOGSIZE];

struct ieee80211_wifipos_debug_mreq {
	ieee80211_wifipos_reqdata_t reqdata;
} ieee80211_wifipos_debug_mreq_g[IEEE80211_WIFIPOS_DEBUGLOGSIZE];

struct ieee80211_wifipos_debug_mresp {
	struct nsp_mresp mresp;
} ieee80211_wifipos_debug_mresp_g[IEEE80211_WIFIPOS_DEBUGLOGSIZE];

struct ieee80211_wifipos_debug_ka {
	u_int8_t ni_macaddr[IEEE80211_ADDR_LEN];	/* MAC address */
} ieee80211_wifipos_debug_ka_g[IEEE80211_WIFIPOS_DEBUGLOGSIZE];

struct ieee80211_wifipos_debug_tsfreq {
	ieee80211_wifipos_tsfreqdata_t tsfreq;
} ieee80211_wifipos_debug_tsfreq_g[IEEE80211_WIFIPOS_DEBUGLOGSIZE];

struct ieee80211_wifipos_debug_tsfresf {
	struct nsp_tsf_resp tsfresp;
} ieee80211_wifipos_debug_tsfresp_g[IEEE80211_WIFIPOS_DEBUGLOGSIZE];

struct ieee80211_wifipos_debug_syncps {
	ieee80211_wifipos_reqdata_t reqdata;
} ieee80211_wifipos_debug_syncps_g[IEEE80211_WIFIPOS_DEBUGLOGSIZE];
struct ieee80211_wifipos_debug_fsm ieee80211_wifipos_debug_fsm_g[IEEE80211_WIFIPOS_DEBUGLOGSIZE];

int ieee80211_wifipos_debuglog(int event, int location, void *data)
{
	struct ieee80211vap *vap;
	struct ieee80211com *ic;
	vap = TAILQ_FIRST(&ic->ic_vaps);
	ic = vap->iv_ic;
//TBD: Remove:
	if (event == IEEE80211_WIFIPOS_DEBUG_KA)
		return 0;
	ieee80211_wifipos_debug_g[ieee80211_wifipos_debug_index].ts =
	    ic->ic_get_TSF32(ic);
//Cross check to make sure no events are lost, because of wraparound
	ieee80211_wifipos_debug_g[ieee80211_wifipos_debug_index].index = ieee80211_wifipos_debug_index;
	ieee80211_wifipos_debug_g[ieee80211_wifipos_debug_index].event = event;
	switch (event) {
	case IEEE80211_WIFIPOS_DEBUG_VALUE:
		ieee80211_wifipos_debug_value_g[ieee80211_wifipos_debug_index].
		    location = location;
		ieee80211_wifipos_debug_value_g[ieee80211_wifipos_debug_index].
		    value = ((int32_t) data);
		break;
	case IEEE80211_WIFIPOS_DEBUG_MREQ:
		memcpy(&ieee80211_wifipos_debug_mreq_g
		       [ieee80211_wifipos_debug_index].reqdata,
		       (ieee80211_wifipos_reqdata_t *) data,
		       sizeof(ieee80211_wifipos_reqdata_t));
		break;
	case IEEE80211_WIFIPOS_DEBUG_MRESP:
		memcpy(&ieee80211_wifipos_debug_mresp_g
		       [ieee80211_wifipos_debug_index].mresp,
		       (struct nsp_mresp *)data, sizeof(struct nsp_mresp));
		break;
	case IEEE80211_WIFIPOS_DEBUG_KA:
		memcpy(&ieee80211_wifipos_debug_ka_g
		       [ieee80211_wifipos_debug_index].ni_macaddr,
		       ((struct ieee80211_node *)data)->ni_macaddr, ETH_ALEN);
		break;
	case IEEE80211_WIFIPOS_DEBUG_TSFREQ:
		memcpy(&ieee80211_wifipos_debug_tsfreq_g
		       [ieee80211_wifipos_debug_index].tsfreq,
		       (ieee80211_wifipos_tsfreqdata_t *) data,
		       sizeof(ieee80211_wifipos_tsfreqdata_t));
		break;
	case IEEE80211_WIFIPOS_DEBUG_TSFRESP:
		memcpy(&ieee80211_wifipos_debug_tsfresp_g
		       [ieee80211_wifipos_debug_index].tsfresp,
		       (struct nsp_tsf_resp *)data,
		       sizeof(struct nsp_tsf_resp));
		break;
	case IEEE80211_WIFIPOS_DEBUG_SYNCPS:
		memcpy(&ieee80211_wifipos_debug_syncps_g
		       [ieee80211_wifipos_debug_index].reqdata,
		       (ieee80211_wifipos_reqdata_t *) data,
		       sizeof(ieee80211_wifipos_reqdata_t));
		break;
    case IEEE80211_WIFIPOS_DEBUG_FSM:
         memcpy(&ieee80211_wifipos_debug_fsm_g[ieee80211_wifipos_debug_index],
                (ieee80211_wifipos_debug_fsm *) data,
                sizeof(ieee80211_wifipos_debug_fsm));

        break;
	default:
		printk("%s: %d: Error: Unknown event logged\n", __FUNCTION__,
		       __LINE__);

	}

	ieee80211_wifipos_debug_index++;
	if (ieee80211_wifipos_debug_index > IEEE80211_WIFIPOS_DEBUGLOGSIZE)
		ieee80211_wifipos_debug_index = 0;

	if ((ieee80211_wifipos_debug_index % IEEE80211_WIFIPOS_DEBUGDUMPFREQ) ==
	    0)
		ieee80211_wifipos_debugdump();

	return 0;

}

int ieee80211_wifipos_debugdump()
{
	//Dump last few events
	ieee80211_wifipos_reqdata_t *reqdata;
	struct nsp_mresp *mresp;
	int index, index2, count = 0;
	u_int32_t ts;
	int value, line;

	for (count = IEEE80211_WIFIPOS_DEBUGDUMPFREQ; count > 0; count--) {
		//Dump the last DUMPFREQ events
		index = ieee80211_wifipos_debug_index - count;
		if (index < 0)
			index += IEEE80211_WIFIPOS_DEBUGLOGSIZE;

		index2 = ieee80211_wifipos_debug_g[index].index;
		ts = ieee80211_wifipos_debug_g[index].ts;
		switch (ieee80211_wifipos_debug_g[index].event) {
		case IEEE80211_WIFIPOS_DEBUG_VALUE:
			value = ieee80211_wifipos_debug_value_g[index].value;
			line = ieee80211_wifipos_debug_value_g[index].location;
			printk("%d: VALUE: Index: %d Line %d: Value %d: \n", ts,
			       index2, line, value);
			break;
		case IEEE80211_WIFIPOS_DEBUG_MREQ:
			reqdata =
			    &ieee80211_wifipos_debug_mreq_g[index].reqdata;
			printk("%d: MREQ (probe): %d: %d: MAC: ", ts, index2,
			       ieee80211_wifipos_debug_index);
			printk("%s", ether_sprintf(reqdata->sta_mac_addr));
			printk("\n");
			break;
		case IEEE80211_WIFIPOS_DEBUG_MRESP:
			mresp = &ieee80211_wifipos_debug_mresp_g[index].mresp;
			printk("%d: MRESP (probe): %d: %d: MAC: ", ts, index2,
			       ieee80211_wifipos_debug_index);
			printk("%s", ether_sprintf(mresp->sta_mac_addr));
			printk("\n");
			break;
		case IEEE80211_WIFIPOS_DEBUG_KA:
			printk("%d: KEEPALIVE: %d: %d: MAC: ", ts, index2,
			       ieee80211_wifipos_debug_index);
			printk("%s",
			       ether_sprintf(ieee80211_wifipos_debug_ka_g
					     [index].ni_macaddr));
			printk("\n");
			break;
		case IEEE80211_WIFIPOS_DEBUG_TSFREQ:
			printk("%d: TSFREQ: %d: %d: MAC: ", ts, index2,
			       ieee80211_wifipos_debug_index);
			printk("%s",
			       ether_sprintf(ieee80211_wifipos_debug_tsfreq_g
					     [index].tsfreq.assoc_ap_mac_addr));
			printk("\n");
			break;
		case IEEE80211_WIFIPOS_DEBUG_TSFRESP:
			printk("%d: TSFRESP: %d: %d: MAC: ", ts, index2,
			       ieee80211_wifipos_debug_index);
			printk("%s",
			       ether_sprintf(ieee80211_wifipos_debug_tsfresp_g
					     [index].tsfresp.ap_mac_addr));
			printk("\n");
			break;
		case IEEE80211_WIFIPOS_DEBUG_SYNCPS:
			reqdata =
			    &ieee80211_wifipos_debug_syncps_g[index].reqdata;
			printk("%d: SYNCPS (probe): %d: %d: MAC: ", ts, index2,
			       ieee80211_wifipos_debug_index);
			printk("%s", ether_sprintf(reqdata->sta_mac_addr));
			printk("\n");
			break;
		default:
			printk("%s: %d: Unknown event\n ", __FUNCTION__,
			       __LINE__);

		}
	}

	return 0;
}
#endif

#if 0
/* Function     : ieee80211_wifipos_cc_xmitprobe
 * Arguments    : ieee80211_wifipos_reqdata_t
 * Functionality: To transmit the request which needs channel change.
 * Return       : 1 for enabled, 0 for disabled
 */
static int ieee80211_wifipos_cc_xmitprobe(ieee80211_wifipos_reqdata_t * reqdata)
{
    u_int8_t pwr_save, cnt;
    int qosresp = 0;

    struct ieee80211_node *ni = NULL;
    struct ieee80211vap *vap;
    struct ath_softc_net80211 *scn;
    struct ieee80211com *ic;
    int vmf = 1;
    vap = TAILQ_FIRST(&ieee80211_wifiposdata_g->ic->ic_vaps);

    if (vap == NULL) {
        WIFIPOS_DPRINTK("WifiPositioning:Vap is null\n");
        return -1;
    }
    if(!IEEE80211_ADDR_IS_VALID(reqdata->sta_mac_addr)){
        printk("%s[%d]: Error: Invalid MAC address in the request\n",
        __func__, __LINE__);
       ieee80211_wifipos_dump_reqdata(reqdata);
       return -1;
    }
    scn = ATH_SOFTC_NET80211(vap->iv_ic);
    ic = vap->iv_ic;
#if IEEE80211_WIFIPOSDEBUG
    WIFIPOS_DPRINTK("%s[%d]: ReqId: %x,  xmit_tstamp: %x \n", __func__, __LINE__,
                     reqdata->request_id, ic->ic_get_TSF32(ic));
#endif
    if(reqdata->hc_channel == reqdata->oc_channel) {
        ni = ieee80211_find_node(&ieee80211_wifiposdata_g->ic->ic_sta,
                 reqdata->sta_mac_addr );
    }
    if (ni == NULL) {
        ni = ieee80211_tmp_node(vap, reqdata->sta_mac_addr);

        if (reqdata->spoof_mac_addr[0] || reqdata->spoof_mac_addr[1]
            || reqdata->spoof_mac_addr[2] || reqdata->spoof_mac_addr[3]
            || reqdata->spoof_mac_addr[4] || reqdata->spoof_mac_addr[5]) {
            IEEE80211_ADDR_COPY(ni->ni_bssid,
                        reqdata->spoof_mac_addr);
        }
    }
    reqdata->ni_tmp_sta = (void *)ni;
// incrementing current request count for each call of xmit probe
    ieee80211_wifiposdata_g->num_mrqst_inprogress++;
    pwr_save =
        ieee80211node_has_flag((struct ieee80211_node *)reqdata->ni_tmp_sta,
                   IEEE80211_NODE_PWR_MGT);
    if (reqdata->oc_channel != ieee80211_wifiposdata_g->ic->ic_curchan->ic_ieee) {
//        ieee80211_send_cts(vap->iv_bss,IEEE80211_CTS_WIFIPOS);

        ic->ic_scan_start(ic);
        OS_SET_TIMER(&ieee80211_wifiposdata_g->wifipos_cctimer,
                 MAX_CTS_TO_SELF_TIME);
#if UMAC_SUPPORT_WIFIPOS_LEANCC
        /* Copying this structure to global variable as we need it in resuming
         * home channel traffic.
         * incrementing the refernece count for this node. Since the life-span
         * of this node is till we resume the home channel traffic.
         */
        // This increment in reference count should be decremented in resuming
        // home channel traffic.
        ieee80211_ref_node((struct ieee80211_node *)reqdata->
                   ni_tmp_sta);
        wlan_pause_node(vap, NULL, true);
        wlan_lean_set_channel(vap, reqdata->oc_channel);
        ic->ic_disable_hwq(ic, 0x03df);
#else
        wlan_set_channel(vap, reqdata->oc_channel, 0);
#endif
        ic->ic_isoffchan = 1;

    }
    for (cnt = 0; cnt < reqdata->request_cnt; cnt++) {
        if(cnt == reqdata->request_cnt -1 )
            vmf = 0;
        if ((reqdata->mode & FRAME_TYPE_HASH) == QOS_NULL_FRAME) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
                      "WifiPositioning:QOS NULL frame:%d",
                      cnt);
            qosresp =
                ieee80211_send_qosnull_probe((struct ieee80211_node
                              *)reqdata->ni_tmp_sta,
                             WME_AC_VO, vmf,
                             (void *)reqdata);

        } else if ((reqdata->mode & FRAME_TYPE_HASH) == NULL_FRAME) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WIFIPOS,
                      "WifiPositioning:NULL frame:%d", cnt);
            ieee80211_send_null_probe((struct ieee80211_node *)
                          reqdata->ni_tmp_sta, vmf,
                          (void *)reqdata);
        }
    }
    // This ref count decrement corresponds to increment in ref count at ieee80211_tmp_node()  in createreqdata
    if (reqdata->ni_tmp_sta)
        ieee80211_free_node((struct ieee80211_node *)reqdata->
                    ni_tmp_sta);

    return qosresp;

}

/* Function     : ieee80211_cts_done
 * Arguments    : Void since this is just and indicator of cts done
 * Functionality: This is CTS done indicator, which would trigger the
                  channel change for measurement request. Assumption is
                  that the ccreqdata or cctsfreqdata is not cleared in
                  xmitprobe. This allows to reuse the resume probe work
                  by just testing if cc request data is available. We
                  dont want to call whole routine here becasue this is a
                  call from tx handler and we dont want to block the call.
 * Return       : 0 Success, -1: failure. Irrespective of CTS success we
                  need to do channel change and do measurement.
 */
#endif
void ieee80211_cts_done(bool txok) {
    WIFIPOS_DPRINTK("CTS DONE txok=%d\n", txok);
    if (!txok) {
        WIFIPOS_DPRINTK("CTS failure!!!\n");
    }
//    schedule_work(&pvt_wifiposdata->resumeprobes_work); //Resume probes in work queue
}
#endif
