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
 * Private defines for the P2P GO Power Save Module.
 * The public defines should be placed at ieee80211_p2p_go_power.h
 */

/*
 * P2P GO (Group Owner/AP) PS (power save) state machine.
 */

typedef enum {
    P2P_GO_PS_STATE_INIT,       /* GO Power Save not started */
    P2P_GO_PS_STATE_TSF_RESYNC, /* TSF has changed and waiting for all sta's to re-sync */
    P2P_GO_PS_STATE_ACTIVE,     /* GO active and awake */
    P2P_GO_PS_STATE_DOZE,       /* GO asleep */
    P2P_GO_PS_STATE_OPP_PS,     /* GO in Opportunistic Power State (awake) */
    P2P_GO_PS_STATE_TX_BCN,     /* GO waiting to send beacon during TBTT */
    P2P_GO_PS_STATE_CTWIN,      /* GO in CT Window (awake) */
} ieee80211_connection_state; 

/* NOTE: please update p2p_go_ps_sm_event_name and function event_name() when this enum is changed */ 
typedef enum {
    P2P_GO_PS_EVENT_NULL = 0,           /* Invalid event */
    P2P_GO_PS_EVENT_STARTED,            /* Power Save Started */
    P2P_GO_PS_EVENT_STOPPED,            /* Power Save Stopped */
    P2P_GO_PS_EVENT_TBTT,               // TODO???: Replace this event P2P_GO_PS_EVENT_SWBA, 
    P2P_GO_PS_EVENT_TX_BCN,
    P2P_GO_PS_EVENT_TSF_CHANGED,
    P2P_GO_PS_EVENT_OPP_PS_OK,          /* During opportunistic PS and all stas asleep */

    /* Timer generated events */
    P2P_GO_PS_EVENT_TIMEOUT_TX_BCN,     /* Beacon Tx timeout timer */
    P2P_GO_PS_EVENT_CTWIN_EXPIRE,       /* CT Window period end */
    P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP,  /* Start of 1-shot NOA sleep */
    P2P_GO_PS_EVENT_PERIOD_NOA_SLEEP,   /* Start of periodic NOA sleep */
    P2P_GO_PS_EVENT_NOA_WAKE,           /* Wakeup call for periodic/1-shot NOA sleep */

} ieee80211_connection_event; 

/*
P2P_GO_PS_STATE_INIT
--------------------

Description:  Init state (VAP is running). But Power Save is not running (inactive).
entry action: Stop all timers, remove NOE IE, deregistered tx beacon notify, 
              Pause NOA scheduler.
exit action: none.
events handled: 
    P2P_GO_PS_EVENT_START:
          action : register tx beacon notify. Unpause NOA scheduler.
          next state : P2P_GO_PS_STATE_TSF_RESYNC  

P2P_GO_PS_STATE_TSF_RESYNC
--------------------------

Description:  Waiting for the next beacon to resync TSF.
entry action: Start the TBTT timer. If already started, then restart with new value.
exit action: none.
events handled: 
    P2P_GO_PS_STATE_EVENT_TBTT:
          action : none.
          next state : P2P_GO_PS_STATE_TX_BCN

    P2P_GO_PS_EVENT_STOP:
          action : none
          next state : P2P_GO_PS_STATE_INIT  

P2P_GO_PS_STATE_TX_BCN
----------------------

Description: 
    Waiting for the transmission of next beacon.
Note: We assumed that the hardware cannot sleep while waiting
for this beacon to be tx-ed.
entry action: 
    Update beacon IE's and prepare the DMA for next beacon. Set Beacon Tx timeout timer.
exit action: stop P2P_GO_PS_EVENT_TIMEOUT_TX_BCN timer.
events handled: 
    P2P_GO_PS_EVENT_TX_BCN:
          action : none
          next state :
            if (have CTWin) then next_state=P2P_GO_PS_STATE_CTWIN
            else
                if (NOA started) then next_state=P2P_GO_PS_STATE_DOZE
                else
                    if (Do Opportunistic PS) then next_state=P2P_GO_PS_STATE_OPP_PS
                    else next_state=P2P_GO_PS_STATE_ACTIVE
                    endif
                endif
            endif
    
    P2P_GO_PS_EVENT_STOP:
          action : none.
          next state : P2P_GO_PS_STATE_INIT  

    P2P_GO_PS_EVENT_TIMEOUT_TX_BCN:
    P2P_GO_PS_EVENT_TSF_CHANGED:
          action : none.
          next state : P2P_GO_PS_STATE_TSF_RESYNC
    
    P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP:
          action : set oneshot_noa = true.
          next state : P2P_GO_PS_STATE_DOZE

P2P_GO_PS_STATE_CTWIN
---------------------

Description: CT Windows phase. GO is active.
entry action: start CTWin timer.
exit action: Stop the CTWin timer.
events handled: 
    P2P_GO_PS_EVENT_CTWIN_EXPIRE:
          action : none
          next state : 
            if (NOA started) then next_state=P2P_GO_PS_STATE_DOZE
            else
                if (Do Opportunistic PS) then next_state=P2P_GO_PS_STATE_OPP_PS
                else next_state=P2P_GO_PS_STATE_ACTIVE
                endif
            endif

    P2P_GO_PS_EVENT_STOP:
          action : 
          next state : P2P_GO_PS_STATE_INIT  

    P2P_GO_PS_EVENT_TSF_CHANGED:
          action : none.
          next state : P2P_GO_PS_STATE_TSF_RESYNC
    
    P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP:
          action : set oneshot_noa = true.
          next state : P2P_GO_PS_STATE_DOZE

    P2P_GO_PS_EVENT_PERIOD_NOA_SLEEP:
          ignored

P2P_GO_PS_STATE_OPP_PS
----------------------

Description: GO Opportunistic Power Save. Can sleep when all sta clients are asleep.
entry action: none.
exit action: none.
events handled: 
    P2P_GO_PS_EVENT_CHK_OPP_PS:
          action : check the sleep status of all associated sta clients.
          next state : if all asleep, then next_state=P2P_GO_PS_STATE_DOZE

    P2P_GO_PS_EVENT_TBTT:
          action : none.
          next state : P2P_GO_PS_STATE_TX_BCN

    P2P_GO_PS_EVENT_STOP:
          action : 
          next state : P2P_GO_PS_STATE_INIT  

    P2P_GO_PS_EVENT_TSF_CHANGED:
          action : none.
          next state : P2P_GO_PS_STATE_TSF_RESYNC

    P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP:
          action : set oneshot_noa = true, set pend_opp_ps = true.
          next state : P2P_GO_PS_STATE_DOZE

    P2P_GO_PS_EVENT_PERIOD_NOA_SLEEP:
          action : set pend_opp_ps = true.
          next state : P2P_GO_PS_STATE_DOZE

P2P_GO_PS_STATE_ACTIVE
----------------------

Description: GO is active and awake.
entry action: none.
exit action: none.
events handled: 
    P2P_GO_PS_EVENT_ONESHOT_NOA_SLEEP:
    P2P_GO_PS_EVENT_PERIOD_NOA_SLEEP:
          action : none
          next state : P2P_GO_PS_STATE_DOZE

    P2P_GO_PS_EVENT_TBTT:
          action : none.
          next state : P2P_GO_PS_STATE_TX_BCN

    P2P_GO_PS_EVENT_STOP:
          action : 
          next state : P2P_GO_PS_STATE_INIT  

    P2P_GO_PS_EVENT_TSF_CHANGED:
          action : none.
          next state : P2P_GO_PS_STATE_TSF_RESYNC

P2P_GO_PS_STATE_DOZE
--------------------

Description: Asleep
entry action: 
    if (oneshot_noa) then Stop TBTT Timer
    Tell the hw that GO is asleep.
exit action: 
    if (oneshot_noa) then Restart TBTT Timer, set oneshot_noa = false.
    Tell the hw that GO is awake.
    Set pend_opp_ps = false.
events handled: 
    P2P_GO_PS_EVENT_TBTT:
          action : none.
          next state : P2P_GO_PS_STATE_TX_BCN
          
    P2P_GO_PS_EVENT_NOA_WAKE:
          action : none
          next state : 
                if (oneshot_noa) then
                    calculate current TBTT
                    if (have CTWin) AND (current time is inside CTWin)
                        then next_state=P2P_GO_PS_STATE_CTWIN
                    else if (have Opp PS)
                        if (all STA asleep) 
                            then Restart TBTT Timer, set oneshot_noa = false.
                        else 
                            next_state=P2P_GO_PS_STATE_OPP_PS
                        endif
                    else 
                        next_state=P2P_GO_PS_STATE_ACTIVE
                    endif
                else  //Periodic NOA
                    else if (have Opp PS)
                        if (NOT all STA asleep) then
                            next_state=P2P_GO_PS_STATE_OPP_PS
                        endif
                    else 
                        next_state=P2P_GO_PS_STATE_ACTIVE
                    endif
                endif
    
    P2P_GO_PS_EVENT_STOP:
          action : 
          next state : P2P_GO_PS_STATE_INIT  

    P2P_GO_PS_EVENT_TSF_CHANGED:
          action : none.
          next state : P2P_GO_PS_STATE_TSF_RESYNC
    

*/

