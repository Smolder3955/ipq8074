/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#ifndef IEEE80211_POWER_PRIV_H
#define IEEE80211_POWER_PRIV_H

#include <ieee80211_var.h>

void ieee80211_protect_set_tim(struct ieee80211vap *vap);
void ieee80211_set_tim(struct ieee80211_node *ni, int set,bool isr_context);
int ieee80211_power_alloc_tim_bitmap(struct ieee80211vap *vap);

#if UMAC_SUPPORT_AP_POWERSAVE 
void    ieee80211_ap_power_vattach(struct ieee80211vap *vap);
void    ieee80211_ap_power_vdetach(struct ieee80211vap *);
#else /* UMAC_SUPPORT_AP_POWERSAVE */
#define ieee80211_ap_power_vattach(vap) /* */
#define ieee80211_ap_power_vdetach(vap) /* */
#endif

#if UMAC_SUPPORT_STA_POWERSAVE 
/*
 * attach sta power  module.
 */
ieee80211_sta_power_t ieee80211_sta_power_vattach(struct ieee80211vap *, int fullsleep_enable, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t,u_int32_t);

int ieee80211_set_powersave(struct ieee80211vap *vap, ieee80211_pwrsave_mode mode);
u_int32_t ieee80211_get_apps_powersave_state(struct ieee80211vap *vap);
int ieee80211_set_powersave_inactive_time(struct ieee80211vap *vap, ieee80211_pwrsave_mode mode, u_int32_t inactive_time);
u_int32_t ieee80211_get_powersave_inactive_time(struct ieee80211vap *vap, ieee80211_pwrsave_mode mode);
int ieee80211_pwrsave_force_sleep(struct ieee80211vap *vap, bool enable);
ieee80211_pwrsave_mode ieee80211_get_powersave(struct ieee80211vap *vap);
int ieee80211_power_set_ips_pause_notif_timeout(struct ieee80211vap *vap, u_int16_t pause_notif_timeout);
u_int16_t ieee80211_power_get_ips_pause_notif_timeout(struct ieee80211vap *vap);

int _ieee80211_sta_power_unregister_event_handler(struct ieee80211vap *vap,
        ieee80211_sta_power_event_handler evhandler, void *arg);
int _ieee80211_sta_power_register_event_handler(struct ieee80211vap *vap,
        ieee80211_sta_power_event_handler evhandler, void *arg);
void _ieee80211_sta_power_event_tim(struct ieee80211vap *vap);
void _ieee80211_sta_power_event_dtim(struct ieee80211vap *vap);
int _ieee80211_sta_power_send_keepalive(struct ieee80211vap *vap);
int _ieee80211_sta_power_pause(struct ieee80211vap *vap, u_int32_t pause_timeout);
int _ieee80211_sta_power_unpause(struct ieee80211vap *vap);
void _ieee80211_sta_power_tx_start(struct ieee80211vap *vap);
void _ieee80211_sta_power_tx_end(struct ieee80211vap *vap);

/*
 * set mode for handloing more data while in pspoll state.
 *  when set to IEEE80211_CONTINUE_PSPOLL_FOR_MORE_DATA, the SM will send pspolls (in response to TIM)
 * to receive  data frames (one pspoll at a time) until all the data
 * frames are received (data frame with more data bit cleared)
 * when set to IEEE80211_WAKEUP_FOR_MORE_DATA, it will only send one pspoll in response to TIM
 * to receive first data frame and if more frames are  queued on the AP
 * (more dat bit set on the received frame) the driver will wake up
 * by sending null frame with PS=0.
 */
int ieee80211_sta_power_set_pspoll_moredata_handling(struct ieee80211vap *vap, ieee80211_pspoll_moredata_handling mode);


/*
* get state of pspoll.
*/
u_int32_t ieee80211_sta_power_get_pspoll(struct ieee80211vap *vap);

int ieee80211_sta_power_set_pspoll(struct ieee80211vap *vap, u_int32_t pspoll);

/*
* get the state of pspoll_moredata.
*/
ieee80211_pspoll_moredata_handling ieee80211_sta_power_get_pspoll_moredata_handling(struct ieee80211vap *vap);

/*
 * called when queueing tx packet  starts. to synchronize the SM with tx thread and make
 * sure that the SM will not change the chip power state while a tx queuing  is in progress.
 */
void _ieee80211_sta_power_tx_start(struct ieee80211vap *vap);


/*
 * called when tx packet queuing ends . 
 */
void _ieee80211_sta_power_tx_end(struct ieee80211vap *vap);

/*
 * detach sta power module.
 */
void ieee80211_sta_power_vdetach(ieee80211_sta_power_t handle);

void ieee80211_sta_power_vap_event_handler(ieee80211_sta_power_t    powersta, 
                                            ieee80211_vap_event *event);

#else
#define ieee80211_sta_power_vattach(vap, fullsleep_enable, a,b,c,d,e,f,g)  NULL 
#define ieee80211_sta_power_vdetach(vap)                                /* */
#define ieee80211_sta_power_vap_event_handler(a,b)                      /* */
#endif

#if UMAC_SUPPORT_STA_SMPS 
/*
 * attach MIMO powersave module.
 */
ieee80211_pwrsave_smps_t ieee80211_pwrsave_smps_attach(struct ieee80211vap *vap, u_int32_t smpsDynamic);


/*
 * detach MIMO powersave module.
 */
void ieee80211_pwrsave_smps_detach(ieee80211_pwrsave_smps_t smps);
#else
#define ieee80211_pwrsave_smps_attach(vap,smpsDynamic) NULL
#define ieee80211_pwrsave_smps_detach(smps)  /**/
#endif

#endif
