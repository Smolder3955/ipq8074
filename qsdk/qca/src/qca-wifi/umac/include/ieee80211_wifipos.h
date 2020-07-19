/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _IEEE80211_WIFIPOS_H
#define _IEEE80211_WIFIPOS_H

#include <ieee80211_defines.h>
#include <ieee80211_wpc.h>

typedef struct {
    u_int8_t *hdump;
    u_int32_t tod;
    u_int32_t toa;
    u_int32_t rate;
    u_int32_t retries;
    u_int16_t sa;
    u_int8_t rssi0;
    u_int8_t rssi1;
    u_int8_t rssi2;
    u_int8_t flags;

/* Need to better job here */    
#define ATH_WIFIPOS_TX_UPDATE (1<<0)
#define ATH_WIFIPOS_RX_UPDATE (1<<1)
#define ATH_WIFIPOS_TX_STATUS (1<<2)
    /* From NBP1.1 we have Chain mask instead of number of chains. We were sending 
     * only rx chains earlier as well. Now it has TX(7-4):RX(3-0). Where ever this 
     * feild is used to get size has to convert it to numbero of chains first. 
     */
    u_int8_t txrxchain_mask;
    u_int8_t rx_pkt_type;
    u_int8_t sta_mac_addr[ETH_ALEN];
    u_int32_t request_id;
} ieee80211_wifiposdesc_t;

typedef struct{
    uint32_t rateset;
    uint32_t retryset;
#define NULL_PKT 0x00
#define QOS_NULL_PKT 0x01
    uint32_t pkt_type;
    u_int8_t txchainmask;
    u_int8_t rxchainmask;
    u_int8_t sta_mac_addr[ETH_ALEN];
    u_int8_t spoof_mac_addr[ETH_ALEN];
    u_int32_t mode;
    u_int32_t request_id;
    u_int32_t request_cnt;
    u_int32_t oc_channel; // off - channel 
    u_int32_t hc_channel; // home channel 
    u_int64_t req_tstamp;
    u_int32_t num_probe_rqst;
    u_int32_t state;  
    struct ath_timer *pstimer; //Timer for sync PS
    void *ni_tmp_sta;
    u_int16_t bandwidth;
    u_int8_t lci_req;
    u_int8_t loc_civ_req;
    u_int8_t req_report_type;
    u_int8_t req_preamble;
    u_int8_t asap_mode;
    u_int8_t burst_exp;
    u_int16_t burst_period;
    u_int16_t burst_dur;
    u_int16_t ftm_per_burst;
}ieee80211_wifipos_reqdata_t;

//per-VAP positioning data
struct ieee80211_wifipos_probedata; /* Forward declaration */
typedef struct {
    void *pvt_wifiposdata;
    struct ieee80211vap *vap;
    //Request 
    int (*status_request)(struct ieee80211vap *, struct nsp_sreq *);
    int (*cap_request)(struct ieee80211vap *, struct nsp_cap_req *);
    int (*sleep_request)(struct ieee80211vap *, struct nsp_sleep_req *);
    int (*wakeup_request)(struct ieee80211vap *, struct nsp_wakeup_req *);

    //Response
    void (*nlsend_status_resp)(struct ieee80211vap *, struct nsp_sreq *);
    void (*nlsend_cap_resp)(struct ieee80211vap *, struct nsp_cap_req *);
    void (*nlsend_tsf_resp)(struct ieee80211vap *, void *);
#if NSP_AP_SW_VERSION > 0x01
    void (*nlsend_sleep_resp)(struct ieee80211vap *, 
                                struct nsp_sleep_req *, 
                                u_int16_t num_ka_frm, 
                                unsigned char result);
#else 
    void (*nlsend_sleep_resp)(struct ieee80211vap *,
                                struct nsp_sleep_req *, 
                                unsigned char result);
#endif
    void (*nlsend_wakeup_resp)(struct ieee80211vap *,
                                struct nsp_wakeup_req *, 
                                unsigned char result);
    void (*nlsend_empty_resp)(struct ieee80211vap *, ieee80211_wifiposdesc_t *);
    void (*nlsend_probe_resp)(struct ieee80211vap *, struct ieee80211_wifipos_probedata *);
    void (*nlsend_tsf_update)(struct ieee80211vap *, u_int8_t *smac);

    //FSM
    int (*fsm)(struct ieee80211vap *, int event, void *data, int idle);

    //
    int (*xmittsfrequest)(struct ieee80211vap *vap, void *tsfrqst);
    int (*xmitprobe)(struct ieee80211vap *vap, 
                            ieee80211_wifipos_reqdata_t *reqdata);

    void (*ol_wakeup_request)(struct ieee80211vap *vap,
                             struct ieee80211_node *ni, bool stop);
    int (*xmitrtt3)(struct ieee80211vap *vap, u_int8_t *mac_addr, int extra);
    int (*lci_set) (struct ieee80211vap *vap, void *lci_data);
    int (*lcr_set) (struct ieee80211vap *vap, void *lcr_data);
} ieee80211_wifipos_data_t;

int ieee80211_wifipos_vattach(struct ieee80211vap *vap);
int ieee80211_wifipos_vdetach(struct ieee80211vap *vap);

int ieee80211_wifipos_status_request(
                            struct ieee80211vap *vap,
                            struct nsp_sreq *srqst);
int ieee80211_wifipos_cap_request(
                            struct ieee80211vap *vap, 
                            struct nsp_cap_req *crqst);
int ieee80211_wifipos_sleep_request(
                            struct ieee80211vap *vap, 
                            struct nsp_sleep_req *slrqst);
int ieee80211_wifipos_wakeup_request(
                            struct ieee80211vap *vap, 
                            struct nsp_wakeup_req *wrqst);

void ieee80211_wifipos_nlsend_status_resp(
                            struct ieee80211vap *vap, 
                            struct nsp_sreq *srqst);
void ieee80211_wifipos_nlsend_cap_resp(
                            struct ieee80211vap *vap, 
                            struct nsp_cap_req *crqst);
#if NSP_AP_SW_VERSION > 0x01
void ieee80211_wifipos_nlsend_sleep_resp(
                            struct ieee80211vap *vap, 
                            struct nsp_sleep_req *slrqst,
                            u_int16_t num_ka_frm,
                            unsigned char result);
#else 
void ieee80211_wifipos_nlsend_sleep_resp(
                            struct ieee80211vap *vap, 
                            struct nsp_sleep_req *slrqst,
						    unsigned char result);
#endif
//void ieee80211_wifipos_nlsend_tsf_update(u_int8_t * mac);
int ieee80211_wifipos_set_txcorrection(wlan_if_t vaphandle, unsigned int corr);
unsigned int ieee80211_wifipos_get_txcorrection(wlan_if_t vaphandle);
int ieee80211_wifipos_set_rxcorrection(wlan_if_t vaphandle, unsigned int corr);
unsigned int ieee80211_wifipos_get_rxcorrection(wlan_if_t vaphandle);
void ieee80211_vap_iter_get_colocated_bss(void *arg, wlan_if_t vap);
void ieee80211_cts_done(bool txok);
#endif
