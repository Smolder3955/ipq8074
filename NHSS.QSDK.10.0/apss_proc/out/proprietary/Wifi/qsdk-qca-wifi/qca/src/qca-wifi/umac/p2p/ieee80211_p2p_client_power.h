/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 * P2P client powersave state machine.
 */
#ifndef _IEEE80211_P2P_CLIENT_POWER_H_
#define _IEEE80211_P2P_CLIENT_POWER_H_
#include <ieee80211_var.h>
#include "ieee80211_p2p_go_schedule.h"
#include "ieee80211P2P_api.h"
#include "ieee80211_p2p_proto.h"
#include <ieee80211_options.h>
#include <ieee80211_sm.h>

typedef enum {
   IEEE80211_P2P_CLIENT_EVENT_GO_ABSENT, /* GO is abset */
   IEEE80211_P2P_CLIENT_EVENT_GO_PRESENT, /* GO is available now */
   IEEE80211_P2P_CLIENT_EVENT_GO_PAUSE,
   IEEE80211_P2P_CLIENT_EVENT_GO_UNPAUSE,
} ieee80211_p2p_cient_ps_event;

/*
*  IEEE80211_P2P_CLIENT_EVENT_GO_ABSENT : event will be sent  os SM in each of the following events.
*
*         when CTWINDOW ends inside the absent period. 
*  IEEE80211_P2P_CLIENT_EVENT_GO_PRESENT : event will be sent  os SM in each of the following events.
*          when an Absent period ends.
*         when a tbtt starts and the There is not one shot NOA is present.
*/

/*

- re use the existing STA power save.
- add a mechanism to pause/unpause the vap without entering fake sleep .
  (can be done by node_pause/node_unpause and add node  pause/node unpause to the ATH layer).
- The P2P Client will pause the vap when ever GO is absent and unpasue 
  when the GO is present.
- the vap pause will also pause ath layer traffic.  
- even the null frames will be queued in to the powersave queue when vap is paused.
- vap pause API needs to be modified to pass an extra param to say wether to
  do fake sleep (or) not. Resource Manager will use the fake sleep option and
  the P2P Client will not use fake sleep option when GO becomes absent.
- if fakse sleep option is not used then vap wll be paused immediately and pause
  flag is set . 
- Pause module needs to keep track of pause requests with faks sleep
  option and without fake sleep option and do the right thing accordingly.

- Note  when the VAP is paused becusse of GO absent and while the GO is absent
  then either power save SM (or) Resource Manager issue multiple pause/unpause 
  requests which will result in multiple null frames queued up in the powersave queue.
  One optimization could be to complete the queued null frame if a frame is queued later.

- The node needs to implement pause/unpause API which will be used by
  both the AP powersave as well as the vap pause/unpause. The  node pause/unpause
  should use a refcount (recursive. i.e multple pause/unpause requests can be made
  the last unpause will really un pause the queue).
  the data/mgmt transmit paths will check if node is paused (do not need to check
  2 flags node in powersave and vap is paused). need to add node paused flag.

- The  iv_output_mgmt_add_ie() need to  be changed to iv_output_mgmt_process
  which should return true/false. if true means the frame is cosumed already.
  if flase means the frame can be sent to ath layer.

P2P CLIENT State machine

    EVENT_GO_PRESENT,      
    EVENT_GO_ABSENT,     
    EVENT_GO_ABSENT_WAITING_CTWINDOW,      

events:
    EVENT_GO_PRESENT,     GO is present from NOA scheuler interpreter    
    EVENT_GO_ABSENT,  GO is absent from NOA scheuler interpreter       
    EVENT_GO_TBTT,    GO tbtt start         
    EVENT_GO_CLIENT_SLEEP,  client succesfully sent frame with PS=1              

    The State machine module will also keep track of the 802.11 powersave state that
    the station indicated to the AP (PS bit in the last null frame succesfully sent to AP).
    The module (SM) needs to keep track of 2 flags.
     1. the current PS state of the P2P client indidated to the AP.
     2. current paused state of the VAP.

  GO_PRESENT: GO is present and active and we can transmit frames to GO.
       entry :  if there was a null frame sent out to alter state while in ABSENT state
                resend the null frame. unpause the VAP.(if not unpaused already).
                To resend the nhull frame we need to add ieee80211_vap_pause_retry_fake_sleep().
                to keep the fake sleep state in sync with vap pause.
       exit  : save the current PS state as indicated to AP.       

       EVENT_GO_ABSENT : transition to GO_ABSENT.
       EVENT_GO_CLIENT_SLEEP : if oppPS is enabeld on GO then transition to GO_ABSENT_WAIT_CT_WINDOW.

  
  GO_ABSENT : GO is absent we can not transmit any frames.
 
       entry: pause the vap.

       entry : pause  the vap (with no fake sleep option).
       during this state any null frames sent out will be
       ompleted with failure immediately.the PS bit of the null frame 
       will be saved.   

       EVENT_GO_PRESENT : transition to GO_PRESENT if oppPS==0 or the client is not in sleep wrt p2p GO.
                          transition to GO_ABSENT_WAIT_CT_WINDOW if oppPS==1 and the client is in sleep wrt p2p GO.
   
  GO_ABSENT_WAIT_CT_WINDOW : GO is  present according to NOA schedule but may be absent
                             beacuse of the oppurtunistic powersave. 
                             we can not transmit any frames.
       EVENT_GO_ABSENT  : transition to GO_ABSENT.
       EVENT_GO_TBTT:     transition to GO_PRESENT. 

*/

struct ieee80211_p2p_client_power;
typedef struct ieee80211_p2p_client_power *ieee80211_p2p_client_power_t;

/*
 * create P2P client power save  SM object.
 */
ieee80211_p2p_client_power_t ieee80211_p2p_client_power_create(osdev_t os_handle, wlan_p2p_client_t p2p_client_handle,
                                                              wlan_if_t vap, ieee80211_p2p_go_schedule_t go_schedule);

/*
 * delete P2P client power save  SM object.
 */
void ieee80211_p2p_client_power_delete(ieee80211_p2p_client_power_t p2p_cli_power);

/*
 * update P2P GO opp PS state.
 */
void ieee80211_p2p_client_power_oppPS_update(ieee80211_p2p_client_power_t p2p_cli_power, bool enable);


/*
 * check the null frame going out and filter it out if necessary.
 */
int ieee80211_p2p_client_power_check_and_filter_null(ieee80211_p2p_client_power_t p2p_cli_power, wbuf_t wbuf);

/*
 * vap event handler.
 */
void ieee80211_p2p_client_power_vap_event_handler (ieee80211_p2p_client_power_t p2p_cli_power, ieee80211_vap_event *event);

#endif
