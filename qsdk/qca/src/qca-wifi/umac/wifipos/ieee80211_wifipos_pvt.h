/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2011 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

#ifndef _IEEE80211_WIFIPOS_PVT_H
#define _IEEE80211_WIFIPOS_PVT_H
#include <ieee80211_wifipos.h>
#include <ol_if_athvar.h>

//TBD: Extend for other versions of protocol
#define IEEE80211_HDR_SERVICE_BITS_LEN 16
#define IEEE80211_HDR_TAIL_BITS_LEN 6
#define SYMBOL_DURATION_MICRO_SEC 4
#define ACK_PKT_BYTE_SIZE 14

#define WIFIPOS_TRUE 1
#define WIFIPOS_FALSE 0
#define TX_DESC 1
#define RX_DESC 0
#define ATH_WIFIPOS_SINGLE_BAND 0
#define ATH_WIFIPOS_POSITIONING 1
#define NSP_HW_VERSION 0x01

#define NO_OF_CLK_CYCLES_PER_MICROSEC 44
#define L_LTF 8
#define L_STF 8
#define L_SIG 4
#define FC0_ACK_PKT 0xd4
#define ONE_SIFS_DURATION 704	// 16*44
#define TWO_SIFS_DURATION 1408	// (16*44)*2

#define MAX_CTS_TO_SELF_TIME        12	// This is updated to 12 msec as per requested by algorithm team. .
#define MAX_CTS_TO_SELF_TIME_USEC   12000	// This is updated to 12 msec as per requested by algorithm team. .
#define WAIT_EXPIRE_TIME_TSFRQST    5  // TBD: this is currently 5 msec, need rationale for better time. 
#define WAIT_EXPIRE_TIME_MRQST      500 // TBD: this is currently 5 msec, need rationale for better time. 
#define MAX_PROBE_TIME              3
#define MAX_PKT_TYPES               3
#define MAX_TIMEOUT                 15000            // Any further changes in this value needs consent with  NBP server team
#define IEEE80211_FRAME_LEN         (sizeof(struct ieee80211_frame) + IEEE80211_CRC_LEN)
#define IEEE80211_QOSFRAME_LEN      (sizeof(struct ieee80211_frame) + IEEE80211_CRC_LEN )

#define IEEE80211_NODE_SAVEQ_MGMTQ(_ni)     (&_ni->ni_mgmtq)
#define IEEE80211_NODE_SAVEQ_FULL(_nsq)     ((_nsq)->nsq_len >= (_nsq)->nsq_max_len)
#define MIN_TSF_TIMER_TIME  5000	/* 5 msec */
#define     TIME_UNIT_TO_MICROSEC   1024	/* 1 TU equals 1024 microsecs */

//Sizes of circular buffers to store per req/per probe data
//Note that these limit the number of concurrent requests/probes than can be concurrently handled

#define ATH_WIFIPOS_MAX_CONCURRENT_REQUESTS 500
#define ATH_WIFIPOS_MAX_CONCURRENT_PROBES 1000
#define ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA 500
#define ATH_WIFIPOS_MAX_CONCURRENT_TSF_AP 50
#define ATH_WIFIPOS_MAX_CONCURRENT_EMPTY_RESP 200


#define MAX_HDUMP_SIZE 390

struct ieee80211_wifipos_debug_fsm {
    int event;
    int current_gstate;
    int next_gstate;
    int req_id;
    int req_state;
    int next_req_state;
    int req_type; // MREQST: 0, TSFTQST: 1
};

//Events
enum { 
    IEEE80211_WIFIPOS_EVENT_MRQST = 0,
	IEEE80211_WIFIPOS_EVENT_TSFRQST,
	IEEE80211_WIFIPOS_EVENT_SYNCPS,
	IEEE80211_WIFIPOS_EVENT_FINRQST,
};
//Wifipos mgr state
enum {
	IEEE80211_WIFIPOSMGR_STATE_IDLE= 0,
	IEEE80211_WIFIPOSMGR_STATE_RUNNING_HC ,
	IEEE80211_WIFIPOSMGR_STATE_RUNNING_OC,
	IEEE80211_WIFIPOSMGR_STATE_WAIT,
};
//Message request state
enum {
	IEEE80211_WIFIPOS_RQSTSTATE_IDLE = 0,
	IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_HC,
    IEEE80211_WIFIPOS_RQSTSTATE_RUNNING_OC,
	IEEE80211_WIFIPOS_RQSTSTATE_WAIT,
	IEEE80211_WIFIPOS_RQSTSTATE_TIMER
} ;

//Per probe data store:
//Stores information on every probe sent
//Note: Per request data store structure is in ath_dev.h
typedef struct ieee80211_wifipos_probedata {
	u_int64_t tod;
	u_int64_t toa;
	u_int32_t rate;
	u_int8_t valid;
	u_int8_t rssi[WPC_MAX_CHAINS];
	unsigned char no_of_chains;
	unsigned char no_of_retries;
	int8_t type1_payld[MAX_HDUMP_SIZE];
	u_int8_t sta_mac_addr[ETH_ALEN];
	u_int16_t request_id;
} ieee80211_wifipos_probedata_t;

#define IEEE80211_KA_DONE             0x01
#define IEEE80211_KA_SUCCESS          0x02
#define IEEE80211_WAKEUP_RESP_SENT    0x04
typedef struct {
	unsigned char sta_mac_addr[ETH_ALEN];
	u_int32_t timestamp;
    u_int16_t num_ka_frm;
	u_int8_t wakeup_interval;
    u_int8_t flags; // 0 bit: tx done, 1 bit: tx success
	//TBD: Need request id here?
    u_int16_t request_id;
} ieee80211_wifipos_wakeup_t;

typedef struct {
	unsigned char assoc_ap_mac_addr[ETH_ALEN];
	u_int64_t probe_ap_tsf;
	u_int64_t assoc_ap_tsf;
    u_int32_t fast_time_stamp;
    u_int64_t tsf_time_stamp;
	u_int32_t diff_tsf;
	unsigned char valid;
	unsigned char request_id;
	char ssid[36];
	int hc_channel;
	int oc_channel;
	int state;
} ieee80211_wifipos_tsfreqdata_t;

typedef struct {
	u_int8_t node_count;
	u_int8_t vap_index;
	struct nsp_station_info *stainfo;
} ieee80211_wifipos_stabuff_t;

typedef struct {
	u_int32_t tsf_start1;
	u_int32_t tsf_start2;
	u_int32_t tsf_end1;
	u_int32_t tsf_end2;
	u_int32_t cnt_cc;
	u_int32_t wpnt[1000];	//wlan_node_pause time
	u_int32_t wlsct0[1000];	//wlan_lean_set_channel 0 going off channel
	u_int32_t wgpt[1000];	// time to generate probes
	u_int32_t wlsct1[1000];	// wlan_lean_set_channel 1 coming back to home channel
	u_int32_t wcnt[1000];	// cleanup node time
	u_int32_t wrtxqst[1000];	// time to reap txqs. 
	u_int32_t wupnt[1000];	//wlan_node_unpause time
	u_int32_t CTStet[1000];	// CTS  timer expires. ie. the time we were off channel
	u_int32_t avg_rxclear_pct;
	u_int32_t avg_rxframe_pct;
	u_int32_t avg_txframe_pct;
} ieee80211_wifipos_cctimingstats_t;

enum { IEEE80211_WIFIPOS_DEBUG_VALUE = 0,
	IEEE80211_WIFIPOS_DEBUG_MREQ,
	IEEE80211_WIFIPOS_DEBUG_MRESP,
	IEEE80211_WIFIPOS_DEBUG_KA,
	IEEE80211_WIFIPOS_DEBUG_TSFREQ,
	IEEE80211_WIFIPOS_DEBUG_TSFRESP,
	IEEE80211_WIFIPOS_DEBUG_SYNCPS,
	IEEE80211_WIFIPOS_DEBUG_FSM,
};

//per-VAP positioning data
typedef struct {
	ieee80211_wifipos_data_t wifiposdata;
	void *wifipos_sock;
	u_int32_t process_pid;
	u_int32_t txcorrection;
	u_int32_t rxcorrection;

    ieee80211_wifipos_wakeup_t wakeup[ATH_WIFIPOS_MAX_CONCURRENT_WAKEUP_STA];
    ieee80211_wifipos_reqdata_t reqdata[ATH_WIFIPOS_MAX_CONCURRENT_REQUESTS];
    ieee80211_wifipos_tsfreqdata_t tsfreq[ATH_WIFIPOS_MAX_CONCURRENT_TSF_AP];
    ieee80211_wifipos_probedata_t probedata[ATH_WIFIPOS_MAX_CONCURRENT_PROBES];
    ieee80211_wifiposdesc_t emptydesc[ATH_WIFIPOS_MAX_CONCURRENT_EMPTY_RESP];
    
    int reqdata_index;
    int tsfreqdata_index;
    
    //Indices into probe data store 
    //Index access needs locking, see above
    int proc_index;
    int tx_index;
    int rx_index;
    int wakeup_sta_count;
    
    //Index into wakeup data store 
    int wakeupindex;

    int emptydesc_widx;
    int emptydesc_ridx;
    //Pointer to request data that triggered the current channel change 
    //(only one request for off channel probes can be runnign at a time).
    //Written during measurement request, read during measurment response/timer.
    //Concurrent access protection not needed
    //Only one of these can be non null at a time
    ieee80211_wifipos_reqdata_t *syncpsreqdata;  
    ieee80211_wifipos_reqdata_t *ccreqdata;
    ieee80211_wifipos_tsfreqdata_t *cctsfreqdata;
    
    //Channel change timing stats collection
    ieee80211_wifipos_cctimingstats_t cctiming;

	//FSM state
	int state;

    // Counter for currently running request 
    // Any request needs channel change needs to check if this 
   // variable is 0, then it can proceed to Off channel requests.
    uint16_t num_mrqst_inprogress;

	//Work queues
	struct work_struct cleanprobedata_work;
	struct work_struct resumeprobes_work;
    struct work_struct emptyresp_work;

	//Locks 
    unsigned long flags;
	spinlock_t lock;	//Global FSM lock
	spinlock_t probedata_lock;
	spinlock_t wakeup_lock;
	spinlock_t tsf_req_lock;
    spinlock_t emptydesc_lock;

	//Timers
	os_timer_t wifipos_cctimer;	//Channel change timer. Only one channel change is possible at a timer, hence need only one timer
	os_timer_t wifipos_wakeup_timer;	//Wakeup timer, all wakeup requests use the same timer
	os_timer_t wifipos_cleanup_timer;	// This timer is used as a check if we are not waiting too long in the WAIT state. 

} ieee80211_wifipos_pvt_data_t;

#if 1
typedef struct ieee80211_ol_wifiposdesc {
     u_int8_t *hdump;
     u_int64_t tod;
     u_int64_t toa;
     u_int32_t rate;
     u_int32_t retries;
     u_int16_t sa;
     u_int8_t rssi0;
     u_int8_t rssi1;
     u_int8_t rssi2;
     u_int8_t rssi3; // TODO: Should use an array here, with max size = number of chains constant
     u_int8_t flags;
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
     u_int8_t req_type;
     u_int64_t t3;
     u_int64_t t4;
} ieee80211_ol_wifiposdesc_t;

#endif 


#define IEEE80211_WIFIPOSDEBUG 0
#if IEEE80211_WIFIPOSDEBUG
//Size of debug log/freq of dump (in events)
#define IEEE80211_WIFIPOS_DEBUGLOGSIZE 5000
#define IEEE80211_WIFIPOS_DEBUGDUMPFREQ 1000 
int ieee80211_wifipos_debugdump(void);
int ieee80211_wifipos_debuglog(int, int, void *);
#else
#define ieee80211_wifipos_debugdump(a)
#define ieee80211_wifipos_debuglog(a, b, c)
#endif

void ieee80211_wifipos_nlsend_tsf_resp(
                            struct ieee80211vap *vap,
					        void *tsfres);
void ieee80211_wifipos_nlsend_wakeup_resp(
                            struct ieee80211vap *vap, 
                            struct nsp_wakeup_req *wrqst,
						    unsigned char result);
void ieee80211_wifipos_nlsend_empty_resp(
                            struct ieee80211vap *vap,
                            ieee80211_wifiposdesc_t *wifiposdesc);
void ieee80211_wifipos_nlsend_probe_resp(
                            struct ieee80211vap *vap,
                            ieee80211_wifipos_probedata_t *wifipos_entry);
void ieee80211_wifipos_nlsend_tsf_update(
                            struct ieee80211vap *vap,
                            u_int8_t * source_mac_addr);

int ieee80211_wifipos_xmittsfrequest(
                            struct ieee80211vap *vap,
                            void *tsfrqst);
int ieee80211_wifipos_xmitprobe(
                            struct ieee80211vap *vap, 
                            ieee80211_wifipos_reqdata_t *reqdata);

int ieee80211_wifipos_fsm(struct ieee80211vap *vap, int event, void *data, int idle);

int ol_ieee80211_wifipos_xmitprobe (struct ieee80211vap *vap,
                                        ieee80211_wifipos_reqdata_t *reqdata);
int ol_ieee80211_wifipos_xmitrtt3 (struct ieee80211vap *vap, u_int8_t *mac_addr,
                                        int extra);
void ol_ath_rtt_keepalive_req(struct ieee80211vap *vap,
                         struct ieee80211_node *ni, bool stop);

int ol_ieee80211_lci_set (struct ieee80211vap *vap, void *reqdata);

int ol_ieee80211_lcr_set (struct ieee80211vap *vap, void *reqdata);

#endif
