/*
* Copyright (c) 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#if ATH_SUPPORT_IQUE

#ifndef IEEE80211_HBR_PRIV_H
#define IEEE80211_HBR_PRIV_H

#include <osdep.h>
#include <ieee80211_var.h>
#include <ieee80211_sm.h>
#include <ieee80211_ique.h>


/*
 * Description for the HBR state machine
 */
/*
IEEE80211_HBR_STATE_ACTIVE
----------------------------------------
Description:  Active/Init state
entry action: clear the block flag
exit action:  none
event handled:
    IEEE80211_HBR_EVENT_BACK:
        action:   none
        next state: BLOCKING
    IEEE80211_HBR_EVENT_FORWARD:
        action:   none
        next state: ACTIVE
    IEEE80211_HBR_EVENT_STALE:
        action:   none
        next state: ACTIVE

IEEE80211_HBR_STATE_BLOCKING
----------------------------------------
Description:  Active/Init state
entry action: set the block flag
exit action:  none
event handled:
    IEEE80211_HBR_EVENT_BACK:
        action:   none
        next state: PROBING
    IEEE80211_HBR_EVENT_FORWARD:
        action:   none
        next state: ACTIVE
    IEEE80211_HBR_EVENT_STALE:
        action:   none
        next state: PROBING

IEEE80211_HBR_STATE_PROBING
----------------------------------------
Description:  Block the frames to the node with the specified tid, and send out
            probing QoSNull frames to detect the connectivity
entry action: block outgoing frames, send out QoSNull probing frames with
            N_PROBING flag set.
exit action:  none
event handled:
    IEEE80211_HBR_EVENT_BACK:
        action:   none
        next state: PROBING
    IEEE80211_HBR_EVENT_FORWARD:
        action:   none
        next state: ACTIVE
    IEEE80211_HBR_EVENT_STALE:
        action:   none
        next state: PROBING
*/

/*
 * Data structures for headline block removal (hbr)
 */

/* Entry for each node and state machine (per MAC address) */

struct _wlan_hbr_sm {
    struct ieee80211vap *hbr_vap;   /* back pointer to the vap */
    struct ieee80211_node *hbr_ni;
	u_int8_t	hbr_addr[IEEE80211_ADDR_LEN];	/* node's mac addr */
    ieee80211_hbr_event    hbr_trigger;    /* event to trigger the transition of the state machine */
	u_int8_t	hbr_block;		/* block outgoing traffic ? */
    ieee80211_hsm_t hbr_sm_entry;   /* state machine entry for hbr */
    TAILQ_ENTRY(_wlan_hbr_sm)   hbr_list;    
};

/* A list for all nodes (and state machines) */
struct ieee80211_hbr_list {
	struct ieee80211vap *hbrlist_vap;	/* backpointer to vap */	
	u_int16_t	hbr_count;
	rwlock_t	hbr_lock;
    os_timer_t  hbr_timer;
    u_int32_t   hbr_timeout; 
	TAILQ_HEAD(HBR_HEAD_TYPE, _wlan_hbr_sm) hbr_node;
};

typedef struct _wlan_hbr_sm *wlan_hbr_sm_t;


/* this type of function will be called while iterating the state machine list for each entry,
 * with two arguments, and the return value indicate to break (if return 1) or continue (if return 0)
 * iterating 
 */
typedef int ieee80211_hbr_iterate_func(wlan_hbr_sm_t, void *, void *);

#define DEF_HBR_TIMEOUT 2000

#endif /* IEEE80211_HBR_PRIV_H */

#endif /* ATH_SUPPORT_IQUE */

