/*
* Copyright (c) 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 *  Copyright (c) 2009 Atheros Communications Inc.
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
/********************************************************************/
/* \file ieee80211_hbr.c
 * \brief Headline block removal algorithm, as part of 
 * Atheros iQUE features.
 *
 * This algorithm tries to block the outgoing UDP frames in the VI/VO queues 
 * if the connection of this node is unreliable, thus they will not block 
 * other VI/VO data frames to be transmitted to the nodes with good channel 
 * condition. While transmission being blocked, Qos Null framses will be 
 * sent out periodically as probing frames to check the channel condition, 
 * untill the channel condition is good again, and the normal transmissions
 * are restored thereafter. For more details, please refer to the design document.
 */
#include "osdep.h"
#if ATH_SUPPORT_IQUE

#include <ieee80211_ique.h>
#include <ieee80211_sm.h>
#include "if_athproto.h"
#include "ieee80211_hbr_priv.h"

static wlan_hbr_sm_t ieee80211_hbr_find_byaddr(struct ieee80211vap *vap, u_int8_t *addr);
static void ieee80211_hbr_probe_allnodes(struct ieee80211vap *vap, u_int8_t *addr);
static void ieee80211_hbr_setblock_allnodes(struct ieee80211vap *vap, u_int8_t *addr, u_int8_t block);
static void ieee80211_hbr_iterate(struct ieee80211vap *vap, ieee80211_hbr_iterate_func *f, void *arg1, void *arg2);
static int ieee80211_hbr_step(wlan_hbr_sm_t sm, void *arg1, void *arg2);
static void ieee80211_hbr_state_active_entry(void *ctx);
static void ieee80211_hbr_state_active_exit(void *ctx);
static bool ieee80211_hbr_state_active_event(void *ctx, u_int16_t event, 
                u_int16_t event_data_len, void *event_data);
static void ieee80211_hbr_state_blocking_entry(void *ctx);
static void ieee80211_hbr_state_blocking_exit(void *ctx);
static bool ieee80211_hbr_state_blocking_event(void *ctx, u_int16_t event, 
                u_int16_t event_data_len, void *event_data);
static void ieee80211_hbr_state_probing_entry(void *ctx);
static void ieee80211_hbr_state_probing_exit(void *ctx);
static bool ieee80211_hbr_state_probing_event(void *ctx, u_int16_t event, 
                u_int16_t event_data_len, void *event_data);
static void ieee80211_hbr_addentry(struct ieee80211vap *vap, struct ieee80211_node *ni);
static void ieee80211_hbr_delentry(struct ieee80211vap *vap, struct ieee80211_node *ni);
static void ieee80211_hbr_set_probing(struct ieee80211vap *vap, struct ieee80211_node *ni, wbuf_t wbuf, u_int8_t tid);
static int ieee80211_hbr_dropblocked(struct ieee80211vap *vap, struct ieee80211_node *ni, wbuf_t wbuf);
static void ieee80211_hbr_settrigger_byaddr(struct ieee80211vap *vap, u_int8_t *addr, int signal);
static int ieee80211_hbr_getstate(wlan_hbr_sm_t sm, void *arg1, void *arg2);
static void ieee80211_hbr_getstate_all(struct ieee80211vap * vap);
static void ieee80211_hbr_detach(struct ieee80211vap *vap);
/*
 * Definition of the states for headline block removal state machine
 */

ieee80211_state_info ieee80211_hbr_sm_info[] = {
    { 
        (u_int8_t) IEEE80211_HBR_STATE_ACTIVE, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "ACTIVE",
        ieee80211_hbr_state_active_entry,
        ieee80211_hbr_state_active_exit,
        ieee80211_hbr_state_active_event
    },
    { 
        (u_int8_t) IEEE80211_HBR_STATE_BLOCKING, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "BLOCKING",
        ieee80211_hbr_state_blocking_entry,
        ieee80211_hbr_state_blocking_exit,
        ieee80211_hbr_state_blocking_event
    },
    { 
        (u_int8_t) IEEE80211_HBR_STATE_PROBING, 
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        (u_int8_t) IEEE80211_HSM_STATE_NONE,
        false,
        "PROBING",
        ieee80211_hbr_state_probing_entry,
        ieee80211_hbr_state_probing_exit,
        ieee80211_hbr_state_probing_event
    },
};

static const char*    hbr_event_name[] = {
    /* IEEE80211_HBR_EVENT_BACK    */ "EVENT_BACK",
    /* IEEE80211_HBR_EVENT_FORWARD */ "EVENT_FORWARD",
    /* IEEE80211_HBR_EVENT_STALE   */ "EVENT_STALE"
};

static void ieee80211_hbr_sm_debug_print (void *ctx,const char *fmt,...) 
{
    char tmp_buf[256];
    va_list ap;
     
    va_start(ap, fmt);
    vsnprintf (tmp_buf,256, fmt, ap);
    va_end(ap);
    IEEE80211_DPRINTF(((wlan_hbr_sm_t) ctx)->hbr_vap, IEEE80211_MSG_IQUE,
        "%s", tmp_buf);
}

/* Iterate through the list to find the state machine by the node's address */
static wlan_hbr_sm_t ieee80211_hbr_find_byaddr(struct ieee80211vap *vap, u_int8_t *addr)
{
    wlan_hbr_sm_t sm;
    wlan_hbr_sm_t smt;
    struct ieee80211_hbr_list *ht;
    rwlock_state_t lock_state;

    if (!addr)
        return NULL;	
    sm = NULL;
    ht = vap->iv_hbr_list;
    OS_RWLOCK_WRITE_LOCK(&ht->hbr_lock, &lock_state);
    TAILQ_FOREACH(smt, &ht->hbr_node, hbr_list) {
        if (smt) {
            if(IEEE80211_ADDR_EQ(addr, smt->hbr_addr)) {	
                sm = smt;
                break;
            }
        }	
    }
    OS_RWLOCK_WRITE_UNLOCK(&ht->hbr_lock, &lock_state);
    return sm;
}

/* Iterate through the list to find the state machine by the node's address 
 * - with nolock
 */
static wlan_hbr_sm_t ieee80211_hbr_find_byaddr_nolock(struct ieee80211vap *vap, u_int8_t *addr)
{
    wlan_hbr_sm_t sm;
    wlan_hbr_sm_t smt;
    struct ieee80211_hbr_list *ht;

    if (!addr)
        return NULL;	
    sm = NULL;
    ht = vap->iv_hbr_list;
    TAILQ_FOREACH(smt, &ht->hbr_node, hbr_list) {
        if (smt) {
            if(IEEE80211_ADDR_EQ(addr, smt->hbr_addr)) {	
                sm = smt;
                break;
            }
        }	
    }
    return sm;
}


/* Iterate all associated node to send out probing frames */
static void ieee80211_hbr_probe_allnodes(struct ieee80211vap *vap, u_int8_t *addr)
{
    wlan_hbr_sm_t smt;
    struct ieee80211_hbr_list *ht;
    
    ht = vap->iv_hbr_list;
    TAILQ_FOREACH(smt, &ht->hbr_node, hbr_list) {
        if (smt && smt->hbr_ni) {
            if (IEEE80211_ADDR_EQ(addr, smt->hbr_ni->ni_macaddr) && 
                smt->hbr_ni->ni_associd && smt->hbr_ni != smt->hbr_ni->ni_vap->iv_bss) 
            {
                smt->hbr_ni->ni_ique_flag = 1;
                ieee80211_send_qosnulldata(smt->hbr_ni, WME_AC_VI, 0);
                smt->hbr_ni->ni_ique_flag = 0;
            }
        }
    }    
}

/* Iterate all associated node to set the blocking flag */
static void ieee80211_hbr_setblock_allnodes(struct ieee80211vap *vap, u_int8_t *addr, u_int8_t block)
{
    wlan_hbr_sm_t smt;
    struct ieee80211_hbr_list *ht;
    
    ht = vap->iv_hbr_list;
    TAILQ_FOREACH(smt, &ht->hbr_node, hbr_list) {
        if (smt && smt->hbr_ni) {
            if (IEEE80211_ADDR_EQ(addr, smt->hbr_ni->ni_macaddr) && 
                smt->hbr_ni->ni_associd && smt->hbr_ni != smt->hbr_ni->ni_vap->iv_bss) 
            {
                smt->hbr_ni->ni_hbr_block = block;
                if (!block)
                    smt->hbr_ni->ni_ique_flag = 0;
            }
        }
    }    
}

/* Iterate the state machine list with running the function *f. If *f returns 1, break out of the loop;
 * If *f returns 0, continue to loop 
 */
static void ieee80211_hbr_iterate(struct ieee80211vap *vap, ieee80211_hbr_iterate_func *f, void *arg1, void *arg2)
{
    wlan_hbr_sm_t smt;
    struct ieee80211_hbr_list *ht;
    rwlock_state_t lock_state;
    
    ht = vap->iv_hbr_list;
    OS_RWLOCK_WRITE_LOCK(&ht->hbr_lock, &lock_state);
    TAILQ_FOREACH(smt, &ht->hbr_node, hbr_list) {
        if (smt) {	
            if ((*f)(smt, arg1, arg2))
                break;
            else
                continue;
        }	
    }
    OS_RWLOCK_WRITE_UNLOCK(&ht->hbr_lock, &lock_state);
}

/* Run each state machine */
static int ieee80211_hbr_step(wlan_hbr_sm_t sm, void *arg1, void *arg2)
{
    ieee80211_sm_dispatch(sm->hbr_sm_entry, sm->hbr_trigger, 0, NULL);
    return 0;
}

/* Timer function to run the state machines */
OS_TIMER_FUNC(ieee80211_hbr_timer)
{
    struct ieee80211vap *vap;
    OS_GET_TIMER_ARG(vap, struct ieee80211vap *);
    if ((vap) && (!ieee80211_vap_deleted_is_set(vap))) {
        ieee80211_hbr_iterate(vap, ieee80211_hbr_step, NULL, NULL);	
        OS_SET_TIMER(&vap->iv_hbr_list->hbr_timer, vap->iv_hbr_list->hbr_timeout);
    }
    return;
}

/*
 * entry/exit/event functions for each state of the state machine
 */
/* ACTIVE */
static void ieee80211_hbr_state_active_entry(void *ctx)
{
    wlan_hbr_sm_t sm = (wlan_hbr_sm_t) ctx;
    IEEE80211_DPRINTF(sm->hbr_vap, IEEE80211_MSG_IQUE, 
                    "HBR SM: enter state ACTIVE, with trigger=%d\n", sm->hbr_trigger);
    sm->hbr_block = 0;
    sm->hbr_trigger = IEEE80211_HBR_EVENT_STALE;
    ieee80211_hbr_setblock_allnodes(sm->hbr_vap, sm->hbr_addr, 0);
}

static void ieee80211_hbr_state_active_exit(void *ctx)
{
    /* do nothing on exit */    
    return;
}

/* return true if event handled, otherwise false */
static bool ieee80211_hbr_state_active_event(void *ctx, u_int16_t event, 
                u_int16_t event_data_len, void *event_data)
{
    wlan_hbr_sm_t sm = (wlan_hbr_sm_t) ctx;

    switch(event) {
        case IEEE80211_HBR_EVENT_BACK:
            ieee80211_sm_transition_to(sm->hbr_sm_entry, IEEE80211_HBR_STATE_BLOCKING);    
            return true;
            break;      
        default:
            return true;
    }
    return false;
}

/* BLOCKING */
static void ieee80211_hbr_state_blocking_entry(void *ctx)
{
    wlan_hbr_sm_t sm = (wlan_hbr_sm_t) ctx;
    IEEE80211_DPRINTF(sm->hbr_vap, IEEE80211_MSG_IQUE, 
                     "HBR SM: enter state BLOCKING, with trigger=%d\n", sm->hbr_trigger);
    sm->hbr_block = 1;
    sm->hbr_trigger = IEEE80211_HBR_EVENT_STALE;
    ieee80211_hbr_setblock_allnodes(sm->hbr_vap, sm->hbr_addr, 1);
}

static void ieee80211_hbr_state_blocking_exit(void *ctx)
{
    /* do nothing on exit */    
    return;
}

static bool ieee80211_hbr_state_blocking_event(void *ctx, u_int16_t event, 
                u_int16_t event_data_len, void *event_data)
{
    wlan_hbr_sm_t sm = (wlan_hbr_sm_t) ctx;

    ieee80211_sm_transition_to(sm->hbr_sm_entry, IEEE80211_HBR_STATE_PROBING);    
    return true;
}

/* PROBING */
static void ieee80211_hbr_state_probing_entry(void *ctx)
{
    wlan_hbr_sm_t sm = (wlan_hbr_sm_t) ctx;
    IEEE80211_DPRINTF(sm->hbr_vap, IEEE80211_MSG_IQUE, 
                    "HBR SM: enter state PROBING, with trigger=%d\n", sm->hbr_trigger);
    sm->hbr_block = 1;
    sm->hbr_trigger = IEEE80211_HBR_EVENT_STALE;
//	ieee80211_hbr_setblock_allnodes(sm->hbr_vap, sm->hbr_addr, 1);
    ieee80211_hbr_probe_allnodes(sm->hbr_vap, sm->hbr_addr);
}

static void ieee80211_hbr_state_probing_exit(void *ctx)
{
    /* do nothing on exit */    
    return;
}

static bool ieee80211_hbr_state_probing_event(void *ctx, u_int16_t event, 
                u_int16_t event_data_len, void *event_data)
{
    wlan_hbr_sm_t sm = (wlan_hbr_sm_t) ctx;

    switch(event) {
        case IEEE80211_HBR_EVENT_FORWARD:
            ieee80211_sm_transition_to(sm->hbr_sm_entry, IEEE80211_HBR_STATE_ACTIVE);    
            return true;
            break;      
        case IEEE80211_HBR_EVENT_BACK:
            ieee80211_sm_transition_to(sm->hbr_sm_entry, IEEE80211_HBR_STATE_PROBING);    
            return true;
            break;      
        case IEEE80211_HBR_EVENT_STALE:
            ieee80211_sm_transition_to(sm->hbr_sm_entry, IEEE80211_HBR_STATE_PROBING);    
            return true;
            break;      
        default:
            return false;
    }
    return false;
}

/* Add one entry to the state machine list if no one state machine with 
 * this address is found in the list 
 */
static void ieee80211_hbr_addentry(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
    wlan_hbr_sm_t sm;
    struct ieee80211_hbr_list *ht;
    rwlock_state_t lock_state;
    
    ht = vap->iv_hbr_list;
    if (ni == NULL)
        return;            
    sm = ieee80211_hbr_find_byaddr(vap, ni->ni_macaddr);

    /*remove this entry if it exists previously, reset the state machine for this node*/
    if (sm) {
        ieee80211_hbr_delentry(vap, ni);       
    }
    sm = (wlan_hbr_sm_t)OS_MALLOC(vap-> iv_ic->ic_osdev, sizeof(struct _wlan_hbr_sm), GFP_KERNEL);
	/* Modify for static analysis, prevent sm is NULL */
	if (!sm)
	{
		return;
    }
    sm->hbr_vap = vap;
    sm->hbr_ni = ni;
    sm->hbr_block = 0;
    sm->hbr_trigger = IEEE80211_HBR_EVENT_STALE;
    IEEE80211_ADDR_COPY(sm->hbr_addr, ni->ni_macaddr);
    /* We always reset the state of the state machine to ACTIVE, regardless it is
     * a new entry or one already exists in the list, because this function is
     * called by ieee80211_node_join which imply that we should reset the state
     * even if the entry already exists 
     */
    sm->hbr_sm_entry = ieee80211_sm_create(vap->iv_ic->ic_osdev, 
                                           "hbr",
                                           (void *)sm,
                                           IEEE80211_HBR_STATE_ACTIVE, /*init stats is always ACTIVE*/
                                           ieee80211_hbr_sm_info,
                                           sizeof(ieee80211_hbr_sm_info)/sizeof(ieee80211_state_info),
                                           5,   
                                           0, /*no event data*/ 
                                           MESGQ_PRIORITY_NORMAL,
                                           OS_MESGQ_CAN_SEND_SYNC() ? IEEE80211_HSM_SYNCHRONOUS : IEEE80211_HSM_ASYNCHRONOUS, 
                                           ieee80211_hbr_sm_debug_print,
                                           hbr_event_name,
                                           IEEE80211_N(hbr_event_name));
    if (!sm->hbr_sm_entry) {
        IEEE80211_DPRINTF(vap,IEEE80211_MSG_IQUE,"%s : ieee80211_sm_create failed \n", __func__); 
        OS_FREE(sm);
        return;
    }
    OS_RWLOCK_WRITE_LOCK(&ht->hbr_lock, &lock_state);
    ht->hbr_count ++;
    TAILQ_INSERT_TAIL(&ht->hbr_node, sm, hbr_list);
    OS_RWLOCK_WRITE_UNLOCK(&ht->hbr_lock, &lock_state);
    return;
}


static void ieee80211_hbr_delentry(struct ieee80211vap *vap, struct ieee80211_node *ni)
{
    wlan_hbr_sm_t sm;
    struct ieee80211_hbr_list *ht;
    rwlock_state_t lock_state;
    
    ht = vap->iv_hbr_list;
    if (ni == NULL) {
        return;      
    }

    /* Reset hbr info in ni struct */
    ni->ni_hbr_block = 0;
    ni->ni_ique_flag = 0;

    /* ieee80211_hbr_delentry gets called from both process context
     * and softirq context. so acquire the RWLOCK before finding sm
     * and removing from hbr_node and freeing sm
     * to avoid double freeing of sm
     */
    OS_RWLOCK_WRITE_LOCK(&ht->hbr_lock, &lock_state);
    sm = ieee80211_hbr_find_byaddr_nolock(vap, ni->ni_macaddr);
    if (sm) {
        ht->hbr_count --;
        TAILQ_REMOVE(&ht->hbr_node, sm, hbr_list);
        ieee80211_sm_delete(sm->hbr_sm_entry);
        OS_FREE(sm);
    }
    OS_RWLOCK_WRITE_UNLOCK(&ht->hbr_lock, &lock_state);
    return;
}

static void ieee80211_hbr_set_probing(struct ieee80211vap *vap, struct ieee80211_node *ni, wbuf_t wbuf, u_int8_t tid)
{
    if (ni->ni_ique_flag)
        wbuf_set_probing(wbuf);
    else
        wbuf_clear_probing(wbuf);
    wbuf_set_tid(wbuf, tid);
    wbuf_set_priority(wbuf, TID_TO_WME_AC(tid));
}


static int ieee80211_hbr_dropblocked(struct ieee80211vap *vap, struct ieee80211_node *ni, wbuf_t wbuf)
{
    /*
     *	Headline block removal: if the state machine is in 
     *	BLOCKING or PROBING state, transmision of UDP data frames 
     *	are blocked untill swtiches back to ACTIVE state.
     */
    struct ether_header *eh;
    eh = (struct ether_header *)wbuf_header(wbuf);

    if (!IEEE80211_IS_MULTICAST(eh->ether_dhost)) 
    {  /* Only for unicast frames */ 	
        if (ni->ni_hbr_block) 
        {
            if (TID_TO_WME_AC(wbuf_get_tid(wbuf)) == WME_AC_VI)
            {
            /* only drop VI UDP packets and add a counter */
                if (eh->ether_type == htons(ETHERTYPE_IP)) 
                {
                    const struct ip_header *ip = (struct ip_header *) 
                            (wbuf_header(wbuf) + sizeof (struct ether_header));
                    if (ip->protocol == IP_PROTO_UDP) 
                    {
#ifdef QCA_SUPPORT_CP_STATS
                        WLAN_PEER_CP_STAT(ni, tx_dropblock);
#endif
                        return 1;
                    }
                }
                else if(eh->ether_type == htons(ATH_ETH_TYPE)){
                     /* Case - Multicast packets converted to Unicast packets using Encapsulation method */
                     struct athl2p_tunnel_hdr *tunHdr;
                     tunHdr = (struct athl2p_tunnel_hdr *)((u_int8_t *)(eh) + sizeof(struct ether_header));
                     if (tunHdr->proto == 17)
                     {
                         const struct ip_header *ip = (struct ip_header *)
                                                      ((u_int8_t *)tunHdr + sizeof(struct athl2p_tunnel_hdr) + sizeof (struct ether_header));
                         if (ip->protocol == IP_PROTO_UDP)
                         {
#ifdef QCA_SUPPORT_CP_STATS
                             WLAN_PEER_CP_STAT(ni, tx_dropblock);
#endif
                             return 1;
                         }
                     }
                }
            } 
        }
    } 
    return 0;
}

/* Based on the signal from the rate control module (which is in the ath_dev layer),
 * find out the state machine associated with the address and then setup the tri-state 
 * trigger based upon the signal and current state
 */
static void ieee80211_hbr_settrigger_byaddr(struct ieee80211vap *vap, u_int8_t *addr, int signal)
{
    wlan_hbr_sm_t sm;
    sm = ieee80211_hbr_find_byaddr(vap, addr);
    if (sm) { 
        sm->hbr_trigger = signal;
    }
    /* TODO if sm == NULL, it means there's no entry in the list with this address, 
     * this looks like will never happern. However, shoud we add an entry to the list 
     * IF sm is NULL? Would it be a little bit too aggressive? 
     */
}


static int ieee80211_hbr_getstate(wlan_hbr_sm_t sm, void *arg1, void *arg2)
{
    u_int8_t block;
    int8_t trigger;
    
    trigger = sm->hbr_trigger;
    block = sm->hbr_block;
#ifdef QCA_SUPPORT_CP_STATS
    printk("%d\t %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x\t%s\t%s\t%s\t%llu\n",
        *(int *)arg1, sm->hbr_addr[0], sm->hbr_addr[1], sm->hbr_addr[2], sm->hbr_addr[3],
        sm->hbr_addr[4], sm->hbr_addr[5], 
        ieee80211_sm_get_current_state_name(sm->hbr_sm_entry),
        (trigger==IEEE80211_HBR_EVENT_BACK)?"BACK":((trigger==IEEE80211_HBR_EVENT_STALE)?"STALE":"FORWARD"),
        (block==0)?"No":"Yes",
        peer_cp_stats_tx_dropblock_get(sm->hbr_ni->peer_obj));
#endif
    (*(int *)arg1) ++;
    return 0;
}

static void ieee80211_hbr_getstate_all(struct ieee80211vap * vap)
{
    int i;
    i = 0;
    printk("HBR list dump\nNode\tAddress\t\t\tState\tTrigger\tBlock\tDropped VI frames\n");
    ieee80211_hbr_iterate(vap, ieee80211_hbr_getstate, (void *)(&i), NULL);
}

static void ieee80211_hbr_settimeout(struct ieee80211vap *vap, u_int32_t timeout)
{
    if (vap->iv_hbr_list)
        vap->iv_hbr_list->hbr_timeout = timeout;            
}

static void ieee80211_hbr_detach(struct ieee80211vap *vap)
{
    wlan_hbr_sm_t sm;
    wlan_hbr_sm_t smt;
    struct ieee80211_hbr_list *ht;
    
    ht = vap->iv_hbr_list;
    sm = smt = NULL;
    OS_CANCEL_TIMER(&vap->iv_hbr_list->hbr_timer);
    OS_FREE_TIMER(&vap->iv_hbr_list->hbr_timer);
    TAILQ_FOREACH_SAFE(sm,  &ht->hbr_node, hbr_list, smt) {
        if (sm) {
            ieee80211_hbr_delentry(vap, sm->hbr_ni);
        }
    }
    OS_FREE(vap->iv_hbr_list);
    vap->iv_hbr_list = NULL;
    return;
}

int ieee80211_hbr_attach(struct ieee80211vap *vap)
{
    struct ieee80211_hbr_list *hl;
    hl = (struct ieee80211_hbr_list *)OS_MALLOC(
                    vap-> iv_ic->ic_osdev,       
                    sizeof(struct ieee80211_hbr_list), GFP_KERNEL);
    if (hl == NULL) {
        vap->iv_ique_ops.hbr_detach = NULL;
        vap->iv_ique_ops.hbr_nodejoin = NULL;
        vap->iv_ique_ops.hbr_nodeleave = NULL;
        vap->iv_ique_ops.hbr_dropblocked = NULL;
        vap->iv_ique_ops.hbr_set_probing = NULL;
        vap->iv_ique_ops.hbr_sendevent = NULL;
        vap->iv_ique_ops.hbr_dump = NULL;
        vap->iv_ique_ops.hbr_settimer = NULL;
        return -ENOMEM;   
    }        
    vap->iv_hbr_list = hl;
    vap->iv_hbr_list->hbrlist_vap = vap;	
    vap->iv_hbr_list->hbr_count = 0;
    vap->iv_hbr_list->hbr_timeout = DEF_HBR_TIMEOUT;
    OS_RWLOCK_INIT(&vap->iv_hbr_list->hbr_lock);
    TAILQ_INIT(&vap->iv_hbr_list->hbr_node);
    /* attach functions */
    vap->iv_ique_ops.hbr_detach = ieee80211_hbr_detach;
    vap->iv_ique_ops.hbr_nodejoin = ieee80211_hbr_addentry;
    vap->iv_ique_ops.hbr_nodeleave = ieee80211_hbr_delentry;
    vap->iv_ique_ops.hbr_dropblocked = ieee80211_hbr_dropblocked;
    vap->iv_ique_ops.hbr_set_probing = ieee80211_hbr_set_probing;
    vap->iv_ique_ops.hbr_sendevent = ieee80211_hbr_settrigger_byaddr;
    vap->iv_ique_ops.hbr_dump = ieee80211_hbr_getstate_all;
    vap->iv_ique_ops.hbr_settimer = ieee80211_hbr_settimeout;
    /* init and start timer for sm */
    OS_INIT_TIMER(vap->iv_ic->ic_osdev, &vap->iv_hbr_list->hbr_timer, ieee80211_hbr_timer, vap, QDF_TIMER_TYPE_WAKE_APPS);
    OS_SET_TIMER(&vap->iv_hbr_list->hbr_timer, vap->iv_hbr_list->hbr_timeout);
    return 0;
}

#endif /* ATH_SUPPORT_IQUE */

