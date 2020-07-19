/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc..
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */



#ifndef WPC_MGR_H
#define WPC_MGR_H


#define WPC_MAX_PROBESPERREQ        100
#define WPC_MAX_CONCURRENTREQ       500   
#define WPC_TIMERRESOLUTION         10000   //in us i.e 10 ms
#define WPC_MREQTIMEOUT             1000000  //in us i.e 1s  TBD: Change this
#define WPC_INPUT_THROTTLE_MASK     0x0002



typedef struct {
	u_int32_t toa;
	u_int32_t tod;
	u_int32_t txrate;
	u_int8_t rssi[WPC_MAX_CHAINS];
    int16_t toa_corr[WPC_MAX_CHAINS];
    int16_t toa_sp_corr[WPC_MAX_CHAINS];
    double rtt[WPC_MAX_CHAINS]; // rtt from different chains used in outliers.
    double frtt;    // final rtt
	u_int8_t cd[TYPE1PAYLDLEN];
}wpc_probe_data;

typedef struct {
	u_int16_t fftsz;
	u_int8_t  bw40;
	u_int8_t  ntx_chains;
	u_int8_t  nrx_chains;
	u_int8_t  pathMinPwrThresh;
	u_int8_t  toaSearchRange; 
}wpc_pkt_corr_param;

struct nsp_mrequest_state {
	u_int16_t valid;
	u_int16_t state;
	u_int16_t type; 

	u_int32_t timeout; //In ms
	struct timeval starttime;

	/* Information from the request */
        union {
        	struct nsp_mrqst mrqst;
                struct nsp_tsf_req tsfrqst;
        };
	u_int16_t mrqst_index;
	
	/* Data from the probe responses */
	wpc_probe_data probedata[WPC_MAX_PROBESPERREQ];
	u_int16_t proberespcount;
	wpc_pkt_corr_param pkt_corr_info;
   
	 /* Statistics for this request */
	double rtt_mean;
	double rtt_stddev;
	double rssi_mean;
    double  rssi_stddev;
	u_int16_t rtt_samples;
	u_int16_t rssi_samples;
};

enum {
	WPC_MGRSTATE_INIT=0,
	WPC_MGRSTATE_REQUESTINPROGRESS
};

void wpc_rtte_calculate_ftoa(wpc_probe_data *ppd, u_int16_t nRxChains); 
/* Function     :   wpc_init_rtte
 * Arguments    :   NA 
 * Functionality:   To initialize variables to enable rrte engine.
 * Return       :   Void
 */
void wpc_init_rtte();
/* TBD
 *  Function     : wpc_init
 * Arguments    : NA
 * Functionality: To initialize all the static variables
 * Return       : Void
 */

/*
    Processes driver messages 
*/
int wpc_mgr_procdrivermsg();
/*
    Processes server messages 
*/
int wpc_mgr_procservermsg();
#endif


