/*
* Copyright (c) 2011, 2017-2018 Qualcomm Innovation Center, Inc.
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

/*
* 802.11 station association state machine implementation.
*/

#ifndef _IEEE80211_ASSOC_PRIVATE_H_
#define _IEEE80211_ASSOC_PRIVATE_H_

typedef enum {
    IEEE80211_ASSOC_STATE_INIT,
    IEEE80211_ASSOC_STATE_TXCHANSWITCH,
    IEEE80211_ASSOC_STATE_REPEATER_CAC,
    IEEE80211_ASSOC_STATE_JOIN,
    IEEE80211_ASSOC_STATE_JOIN_SETTING_COUNTRY,
    IEEE80211_ASSOC_STATE_JOIN_COMPLETED,
    IEEE80211_ASSOC_STATE_AUTH,
    IEEE80211_ASSOC_STATE_ASSOC,
    IEEE80211_ASSOC_STATE_RUN,
    IEEE80211_ASSOC_STATE_DISASSOC,
    IEEE80211_ASSOC_STATE_MLME_WAIT, /* wait for mlme operation to complete */
} ieee80211_connection_state;

typedef enum {
    IEEE80211_ASSOC_EVENT_CONNECT_REQUEST,
    IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST,
    IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST,
    IEEE80211_ASSOC_EVENT_REASSOC_REQUEST,
    IEEE80211_ASSOC_EVENT_JOIN_COMPLETE,
    IEEE80211_ASSOC_EVENT_JOIN_SUCCESS, 
    IEEE80211_ASSOC_EVENT_JOIN_SUCCESS_SET_COUNTRY,
    IEEE80211_ASSOC_EVENT_JOIN_FAIL, 
    IEEE80211_ASSOC_EVENT_AUTH_SUCCESS, 
    IEEE80211_ASSOC_EVENT_AUTH_FAIL, 
    IEEE80211_ASSOC_EVENT_ASSOC_FAIL, 
    IEEE80211_ASSOC_EVENT_ASSOC_SUCCESS, 
    IEEE80211_ASSOC_EVENT_BEACON_WAIT_TIMEOUT, 
    IEEE80211_ASSOC_EVENT_BEACON_MISS, 
    IEEE80211_ASSOC_EVENT_DISASSOC, 
    IEEE80211_ASSOC_EVENT_DEAUTH, 
    IEEE80211_ASSOC_EVENT_DISASSOC_SENT, 
    IEEE80211_ASSOC_EVENT_TIMEOUT, 
    IEEE80211_ASSOC_EVENT_RECONNECT_REQUEST,
    IEEE80211_ASSOC_EVENT_TXCHANSWITCH_SUCCESS,
    IEEE80211_ASSOC_EVENT_TXCHANSWITCH_FAIL,
    IEEE80211_ASSOC_EVENT_REPEATER_CAC_SUCCESS,
    IEEE80211_ASSOC_EVENT_REPEATER_CAC_FAIL,
    IEEE80211_ASSOC_EVENT_WAIT_FOR_FTIE_UPDATE,
} ieee80211_connection_event; 

/*
IEEE80211_ASSOC_STATE_INIT
--------------------------------
Description:  Init state.
entry action: none.
exit action: none.
events handled: 
    IEEE80211_ASSOC_EVENT_CONNECT_REQUEST:
          action : none.
          next state : STATE_JOIN
    IEEE80211_ASSOC_EVENT_REASSOC_REQUEST,
		  action:  set reassoc req flag.
          next state : STATE_JOIN

IEEE80211_ASSOC_STATE_JOIN
------------------------------------
Description:  waiting for a beacon from AP for the HW to sync tsf.
entry action: start beacon wait timer.
exit action: none.
    IEEE80211_ASSOC_EVENT_BEACON_MISS:
    IEEE80211_ASSOC_EVENT_BEACON_WAIT_TIMEOUT:
	      action: none
          next_state : STATE_INIT.
    IEEE80211_ASSOC_EVENT_BEACON_RECVD:
	      action: none
          next_state : if assoc request STATE_AUTH.
                       if reassoc request STATE_ASSOC
    IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
	      action: none
          next_state : STATE_INIT.
      

IEEE80211_ASSOC_STATE_AUTH
--------------------------------
Description:  802.11 auth frame sent and waiting for response. 
entry action: setup channel,  bssid , rx filters and send auth frame.
exit action: none.
events handled:
    IEEE80211_ASSOC_EVENT_AUTH_SUCCESS:
	      action: none
          next state : STATE_ASSOC
    IEEE80211_ASSOC_EVENT_AUTH_FAIL:
    IEEE80211_ASSOC_EVENT_TIMEOUT:
          action:  send auth frame if number of attempts is less than
                   the configured max.
          next state : STATE_AUTH if the number of attempts
                       is less than the configured max.
                       STATE_INIT if the number of attempts
                       reaches the configured max.
    IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
    IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST:
	      action: none
          next_state : STATE_INIT.

IEEE80211_ASSOC_STATE_ASSOC
---------------------------------
Description:  802.11 asooc frame sent and waiting for response.
entry action: if assoc req send assoc frame,
              if reassoc req send reassoc frame.
exit action: none.
events handled:
    IEEE80211_ASSOC_EVENT_ASSOC_SUCCESS:
	      action: none
          next state : STATE_RUN.
    IEEE80211_ASSOC_EVENT_ASSOC_FAIL:
    IEEE80211_ASSOC_EVENT_TIMEOUT:
		   action:  send assoc frame if number of attempts is less than
                    the configured max.
           next state : STATE_ASSOC if the number of attempts is less
                        than the configured max. STATE_AUTH if the number
                        of attempts reaches the configured max.
    IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
    IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST:
	       action: none
           next_state : STATE_INIT.
    IEEE80211_ASSOC_EVENT_DEAUTH:
	      action: clear reassoc flag.
          next_state : STATE_AUTH.


IEEE80211_ASSOC_STATE_RUN
-------------------------------
Description:  connected to AP.
entry action: none.
exit action:
    IEEE80211_ASSOC_EVENT_DISCONNECT_REQUEST:
	      action: none
          next_state : STATE_INIT.
    IEEE80211_ASSOC_EVENT_DISASSOC_REQUEST:
	      action: none
          next_state : STATE_DISASSOC.
    IEEE80211_ASSOC_EVENT_BEACON_MISS:
	      action: none
          next_state : STATE_INIT.
    IEEE80211_ASSOC_EVENT_DISASSOC:
	      action: none
          next_state : STATE_ASSOC.
    IEEE80211_ASSOC_EVENT_DEAUTH:
	      action: none
          next_state : STATE_AUTH.

IEEE80211_ASSOC_STATE_DISASSOC
------------------------------------
Description:  disconecting from AP.
entry action: send disassoc.
exit action: none.
    IEEE80211_ASSOC_EVENT_DISASSOC_SENT:
	      action: none
          next_state : STATE_INIT.
*/

#endif
