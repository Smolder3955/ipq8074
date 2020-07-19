/*
 * Copyright (c) 2011,2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

/*
 * public API for VAP PAUSE module.
 */
#ifndef _IEEE80211_VAP_PAUSE_H_
#define _IEEE80211_VAP_PAUSE_H_

#include <ieee80211_vap.h>

#if UMAC_SUPPORT_VAP_PAUSE

typedef struct _ieee80211_vap_pause_info {
    u_int8_t                          iv_pause:1,   /* fake sleep entered for STA vap */
                                      iv_force_pause:1; /* force sleep */
    u_int8_t                          iv_pause_count; /* how many times the fake_sleep requested */
    u_int8_t                          iv_force_pause_count; /* how many times the force pause requested */
    atomic_t                          iv_pause_beacon_count; /* number of beacons sent/received after unpause */
    u_int16_t                         iv_min_pause_beacon_count; /*minumum  number of beacons to tx/rx to allow pause  */
    u_int16_t                         iv_idle_timeout; /* in msec.vap is idle if no actitivity for the time */
	u_int32_t                         iv_tx_data_frame_len;      /* total data in bytes transmitted on the vap  */
	u_int32_t                         iv_rx_data_frame_len;      /* total data in bytes received on the vap  */
	u_int32_t                         iv_tx_data_frame_count;    /* total number of data frmes transmitted on the vap  */
	u_int32_t                         iv_rx_data_frame_count;    /* total number of data frmes received on the vap  */
} ieee80211_vap_pause_info;


/**
 * @ can pause query.
 * ARGS :
 *  vap    : handle to vap.
 * RETURNS:
 *  returns  true if the vap can pause else returns false.
 */
bool ieee80211_vap_can_pause(ieee80211_vap_t vap);

/**
 * pause request.
 * @param vap    : handle to vap.
 * @param req_id : id of the requestor.
 * @return  EOK if it accepted the request and processes the request asynchronously and
 *    at the end of processing the request the VAP posts the  
 *    IEEE80211_VAP_PAUSED event to all the registred event handlers.
 *  returns EEXIST if the VAP is in FULL SLEEP state (or) if vap is already in pause  state.
 */
int ieee80211_vap_pause(ieee80211_vap_t vap, u_int16_t reqid);
int  ieee80211_ic_pause(struct ieee80211com *ic);
void  ieee80211_ic_unpause(struct ieee80211com *ic);

/**
 * @ standby bss request.
 * @param vap    : handle to vap.
 * @return returns  EOK if it accepted the request and processes the request asynchronously and
 *    at the end of processing the request the VAP posts the  
 *    IEEE80211_VAP_STANDBY event to all the registred event handlers.
 *  returns EINVAL if vap is already in pause  state.
 */
int ieee80211_vap_standby_bss(ieee80211_vap_t vap);

/**
 * unpause request.
 * @param vap    : handle to vap.
 * @param req_id : id of the requestor.
 * @return  returns EOK if it accepted the request and processes the request asynchronously and
 *    at the end of processing the request the VAP posts the  
 *    IEEE80211_VAP_UNPAUSED event to all the registred event handlers.
 *    returns EINVAL if the VAP is in FULL SLEEP state (or) if vap is not  in pause (or)
 *    if no pause request is pending currently .
 *    returns EBUSY if the pause count not 0 (other modules still wat to keep it paused) 
 */
int ieee80211_vap_unpause(ieee80211_vap_t vap, u_int16_t reqid );

/**
 * @ Reset vap pause state/counters .
 * ARGS :
 *  vap    : handle to vap.
 * RETURNS:
 */
void ieee80211_vap_pause_reset(ieee80211_vap_t vap);

/**
 * resume bss request.
 * @param vap    : handle to vap.
 * @param req_id : id of the requestor.
 * @return EOK if it accepted the request and processes the request asynchronously and
 *    at the end of processing the request the VAP posts the  
 *    IEEE80211_VAP_UNPAUSED event to all the registred event handlers.
 *  returns EEXIST if the VAP is in FULL SLEEP state (or) if vap is not in pause state. 
 */
int ieee80211_vap_resume_bss(ieee80211_vap_t vap);

/**
 * force pause request.synchronously pause the active traffic in the ath  
 * and umac layers. no fake sleep is performed.
 * @param vap    : handle to vap.
 * @param req_id : id of the requestor.
 * @return  EOK if request is processed. 
 *  returns EEXIST if the if vap is already in forced pause state.
 */
int ieee80211_vap_force_pause(ieee80211_vap_t vap, u_int16_t reqid);

/**
 * force unpause request.synchronously unpause the active traffic in the ath  
 * and umac layers. no fake wakeup is performed.
 * @param vap    : handle to vap.
 * @param req_id : id of the requestor.
 * @return  EOK if request is processed. 
 *    returns EBUSY if the force pause count not 0 (other modules still wat to keep it force paused) 
 *    returns EINVAL if it is not in force paused state. 
 */
int ieee80211_vap_force_unpause(ieee80211_vap_t vap, u_int16_t reqid);

/**
 * request CSA on AP VAP.
 * @param vap    : handle to vap.
 * @param req_id : ID of the requesting module.
 * @return EOK if it accepted the request and processes the request asynchronously and
 *    at the end of processing the request the VAP posts the  
 *    IEEE80211_VAP_CSA_COMPLETE event.
 *    The VAP will wait for configured number of beacons (configured via IEEE80211_CSA_BEACON_COUNT) 
 *    to go out with CSA before sending the
 *    IEEE80211_VAP_CSA_COMPLETE event.
 *  returns EINVAL if the VAP is in FULL SLEEP state (or) if vap is in pauseed state.
 */
int ieee80211_vap_request_csa(ieee80211_vap_t vap, wlan_chan_t chan );

#define IEEE80211_CLEAR_ACTIVITY_STATS 0x1

/* change the following functions to macros ? */ 

/**
*  get activity info 
*  @param vap      : handle to vap.
*  @param activity : info about the data transfer activity of a vap.
*  @param flags    : if IEEE80211_CLEAR_ACTITIVY_STATS is set it will clear the activity stats.
*  
*/  
void ieee80211_vap_get_activity(ieee80211_vap_t vap, ieee80211_vap_activity *activity,u_int8_t flags);


/*
 * get a parameter specific to vap pause module.
 */
u_int32_t ieee80211_vap_pause_get_param(wlan_if_t vaphandle, ieee80211_param param);


/*
 * set a parameter specific to vap pause module.
 */
u_int32_t ieee80211_vap_pause_set_param(wlan_if_t vaphandle, ieee80211_param param,u_int32_t val);

#define ieee80211_vap_is_paused(vap)  (vap->iv_pause_info.iv_pause)
#define ieee80211_vap_is_force_paused(vap)  (vap->iv_pause_info.iv_force_pause)

void ieee80211_vap_pause_complete(ieee80211_vap_t vap, IEEE80211_STATUS status); 
void ieee80211_vap_unpause_complete(ieee80211_vap_t vap);

/*
 * module attach function.
 */
void ieee80211_vap_pause_vattach(struct ieee80211com *ic,ieee80211_vap_t vap);

/*
 * module late attach function.
 */
void ieee80211_vap_pause_late_vattach(struct ieee80211com *ic,ieee80211_vap_t vap); 

/*
 * module detach function.
 */
void ieee80211_vap_pause_vdetach(struct ieee80211com *ic,ieee80211_vap_t vap);

#else
typedef struct _ieee80211_vap_pause_info {
    u_int8_t                          iv_dummy;
} ieee80211_vap_pause_info;

#define IEEE80211_DELIVER_VAP_EVENT( _vap,_evt)  /* */ 
#define ieee80211_vap_is_paused(vap) (false) 
#define ieee80211_vap_is_force_paused(vap) (false) 
#define ieee80211_vap_pause(vap,req_id) (EINVAL)
#define ieee80211_vap_standby_bss(vap) (EINVAL)
#define ieee80211_vap_unpause(vap,req_id) (EINVAL) 
#define ieee80211_vap_can_pause(vap,req_id) true 
#define ieee80211_vap_request_csa(vap,chan) (EINVAL)
#define ieee80211_vap_get_activity(vap,activity) (EINVAL)
#define ieee80211_vap_pause_set_param(vap,param,value) /* */ 
#define ieee80211_vap_pause_get_param(vap,param)  0 
#define ieee80211_vap_pause_vattach(ic,vap)      /* */ 
#define ieee80211_vap_pause_vdetach(ic,vap)      /* */ 
#define ieee80211_vap_pause_late_vattach(ic,vap) /* */ 
#define ieee80211_vap_pause_reset(vap)           /* */
#endif

#endif
