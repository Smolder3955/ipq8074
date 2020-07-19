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

/*
* 802.11 station connection state machine implementation.
*/

#ifndef  _IEEE80211_CONNECTION_PRIVATE_H_
#define _IEEE80211_CONNECTION_PRIVATE_H_

typedef enum {
    IEEE80211_CONNECTION_STATE_INIT, 
    IEEE80211_CONNECTION_STATE_SCAN, 
    IEEE80211_CONNECTION_STATE_CONNECTING, 
    IEEE80211_CONNECTION_STATE_CONNECTED, 
    IEEE80211_CONNECTION_STATE_BGSCAN, 
    IEEE80211_CONNECTION_STATE_DISCONNECTING, 
    IEEE80211_CONNECTION_STATE_ROAM, 
    IEEE80211_CONNECTION_STATE_SCAN_CANCELLING, 
} ieee80211_connection_state; 

typedef enum {
    IEEE80211_CONNECTION_EVENT_CONNECT_NONE, /* no op event */ 
    IEEE80211_CONNECTION_EVENT_CONNECT_REQUEST, 
    IEEE80211_CONNECTION_EVENT_DISCONNECT_REQUEST, 
    IEEE80211_CONNECTION_EVENT_SCAN_END, 
    IEEE80211_CONNECTION_EVENT_ASSOC_SUCCESS, 
    IEEE80211_CONNECTION_EVENT_ASSOC_FAILED, 
    IEEE80211_CONNECTION_EVENT_ASSOC_REJECT,
    IEEE80211_CONNECTION_EVENT_BGSCAN_START, 
    IEEE80211_CONNECTION_EVENT_DISCONNECTED, 
    IEEE80211_CONNECTION_EVENT_SCAN_CANCELLED, 
    IEEE80211_CONNECTION_EVENT_SCAN_START, 
    IEEE80211_CONNECTION_EVENT_SCAN_RESTART, 
    IEEE80211_CONNECTION_EVENT_ROAM_CHECK,
    IEEE80211_CONNECTION_EVENT_SCAN_TERMCHK,
    IEEE80211_CONNECTION_EVENT_BSS_NODE_FREED,
} ieee80211_connection_event; 

/*
IEEE80211_CONNECTION_STATE_INIT 
-------------------------
Description:  Init state.
entry action: none.
exit action: none.
events handled: 
    IEEE80211_CONNECTION_EVENT_CONNECT_REQUEST: 
                action: none 
                next_state: STATE_SCAN if no candidate AP is found in the scan cache
                                       or scan cahceh is stale. 
                            STATE_CONNECTING if scan cache is hot and there exists at least
                                             one candidate  AP.

IEEE80211_CONNECTION_STATE_SCAN 
-------------------------
Description:  Init state.
entry action: start a scan.
exit action: cancel scan if there is one in progress.
events handled: 

    IEEE80211_CONNECTION_EVENT_SCAN_END: 
                action: none 
                next_state: STATE_CONNECTING if a candidate AP is found in the scan cache
                            STATE_SCAN if no candidate AP found and number of attempts < 
                                      max attepts and has not reached preconfigured maxtime.
                            STATE_INIT if has reched preconfigured max attepts  (or) 
                                       has reached a preconfigured maximum time.                   
    IEEE80211_CONNECTION_EVENT_DISCONNECT_REQUEST:
                action: none 
                next_state : STATE_INIT.

IEEE80211_CONNECTION_STATE_CONNECTING 
-------------------------------
Description:  conneting to an AP.
entry action: start the connection state machine.
exit action: none .
events handled: 

    IEEE80211_CONNECTION_EVENT_CONNECTION_SUSCCESS: 
                action: none 
                next_state : STATE_CONNECTED.

    IEEE80211_CONNECTION_EVENT_CONNECTION_FAILED: 
                action: none 
                next_state : STATE_CONNECTING if more APs in the list of candidate APs. 
			     STATE_SCAN  if no more APs in the list and association
                                         attempts have failed with all the APs. 

    IEEE80211_CONNECTION_EVENT_CONNECTION_SUSCCESS: 
                action: send reconnect notification. 
                next_state : STATE_CONNECTED.

    IEEE80211_CONNECTION_EVENT_DISCONNECT_REQUEST:
                action: none 
                next_state : STATE_DISCONNECTING.

IEEE80211_CONNECTION_STATE_CONNECTED 
------------------------------
Description:  connected state.
entry action: none.
exit action: none.
events handled: 
    IEEE80211_CONNECTION_EVENT_CONNECTION_FAILED:
                action:  none 
			     STATE_CONNECTING. 
    IEEE80211_CONNECTION_EVENT_DISCONNECT_REQUEST:
                action:none 
                next_state : STATE_DISCONNECTING.
    IEEE80211_CONNECTION_EVENT_BGSCAN_START:
                action: none 
                next_state : STATE_BGSCAN.
    IEEE80211_CONNECTION_EVENT_ROAM_CHECK: 
                action: rebuild the aplist 
                next_state : if there is a roam candidate 
                             go to STATE_ROAM. else
                             stay in the CONNECTED state.

IEEE80211_CONNECTION_STATE_BGSCAN 
---------------------------------
Description:  back ground scan. 
entry action: start a bg scan.
exit action: cancel scan if there is one in progress.
events handled: 
    IEEE80211_CONNECTION_EVENT_SCAN_END: 
                action: none
                next_state : STATE_CONNECTED if no roam candidate exists.
                next_state : STATE_ROAMING if a roam candidate exists.
    IEEE80211_CONNECTION_EVENT_CONNECTION_FAILED:
                action: none 
                next_state : STATE_SCAN if the scan cache is stale (or) no AP candidate
                             AP exists. also convert the continue the BG scan as FG scan.
			     STATE_CONNECTING if scan cache is hot and atleast one candidate 
                            AP exists.
    IEEE80211_CONNECTION_EVENT_DISCONNECT_REQUEST:
                action:  cancel bg scan
                next_state : STATE_BGSCAN_CANCELLING.
    IEEE80211_CONNECTION_EVENT_SCAN_CANCEL: 
                usually cancelled by  a different module (user).
                action: none
                next_state : STATE_CONNECTED.

IEEE80211_CONNECTION_STATE_DISCONNECTING 
----------------------------------------
Description:  disconnecting. 
entry action: disconnect from current AP. 
exit action: none.
events handled: 
    IEEE80211_CONNECTION_EVENT_DISCONNECTED: 
                action: none. 
                next_state : STATE_INIT.


IEEE80211_CONNECTION_STATE_ROAM 
-------------------------------
Description:  roaming 
entry action: stop the cuurent association.
exit action: none.
events handled: 
    IEEE80211_CONNECTION_EVENT_DISCONNECTED: 
                action: none. 
                next_state : CONNECTING.

IEEE80211_CONNECTION_STATE_SCAN_CANCELLING 
--------------------------------------------
Description:  waiting for scan to be cancelled
              before disconnecting.  
entry action: none. 
exit action: none.
events handled: 
    IEEE80211_CONNECTION_EVENT_SCAN_END: 
                action: none. 
                next_state : if bg scan STATE_DISCONNECTING.
                             if fg scan STATE_INIT.

*/

#endif
