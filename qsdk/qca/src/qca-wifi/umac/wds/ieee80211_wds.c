/*
 * Copyright (c) 2013-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2013-2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
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

#include <osdep.h>


#include <ieee80211_var.h>
#include <ieee80211_node.h>
#include <wbuf.h>
#include <ieee80211_rateset.h>
#include <mlme/ieee80211_mlme_priv.h>
#include <linux/crypto.h>

#include "ol_if_athvar.h"
#include <wlan_mlme_dp_dispatcher.h>
#ifdef WLAN_CONV_CRYPTO_SUPPORTED
#include "wlan_crypto_global_def.h"
#include "wlan_crypto_global_api.h"
#endif
#ifdef QCA_SUPPORT_CP_STATS
#include <wlan_cp_stats_ic_utils_api.h>
#endif
#include <wlan_vdev_mlme.h>

/* FIXME : Commented calls to lock_bh */

/* Add wds address to the node table */
static inline int
_ieee80211_add_wds_addr(struct ieee80211vap *vaphandle,
		       struct ieee80211_node *ni, const u_int8_t *macaddr,
		       u_int32_t flags)
{
    int hash;
    struct ieee80211_wds_addr *wds;
    struct ieee80211_node_table *nt = &vaphandle->iv_ic->ic_sta;
    struct ieee80211com *ic = nt->nt_ic;

    rwlock_state_t lock_state;

    wds = (struct ieee80211_wds_addr *) OS_MALLOC(ni->ni_ic->ic_osdev,
            sizeof(struct ieee80211_wds_addr), GFP_KERNEL );

    if (wds == NULL) {
	    /* XXX msg */
	    return 1;
	}

    if (!(ic->ic_is_mode_offload(nt->nt_ic))) {
        if (flags & IEEE80211_NODE_F_WDS_BEHIND) {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_WDS,
                "%s is behind me\n", ether_sprintf(macaddr));
        } else {
            IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_WDS,
                              " it is reachable through: %s, refcnt:%d\n",
                              ether_sprintf(ni->ni_macaddr),
                              ieee80211_node_refcnt(ni));
        }
    }

    wds->wds_agingcount = WDS_AGING_COUNT;
    wds->wds_staging_age = 2 * WDS_AGING_COUNT;
    hash = IEEE80211_NODE_HASH(macaddr);
    IEEE80211_ADDR_COPY(wds->wds_macaddr, macaddr);
    if (!ieee80211_try_ref_node(ni)) {             /* Reference node */
        OS_FREE(wds);
        return -1;
    }

    wds->flags = flags;
    wds->wds_ni = ni;
    wds->wds_last_pkt_time = OS_GET_TIMESTAMP();

    OS_RWLOCK_WRITE_LOCK(&nt->nt_wds_nodelock, &lock_state);
    LIST_INSERT_HEAD(&nt->nt_wds_hash[hash], wds, wds_hash);
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);

    return ic->ic_node_add_wds_entry((void *)(vaphandle->iv_ifp), macaddr, ni->ni_macaddr, flags);
}

int
ieee80211_add_wds_addr(struct ieee80211vap *vaphandle,
                      struct ieee80211_node_table *nt,
		      struct ieee80211_node *ni, const u_int8_t *macaddr,
		      u_int32_t flags)
{
    return _ieee80211_add_wds_addr(vaphandle, ni, macaddr, flags);
}

/* remove wds address from the wds hash table */
void
ieee80211_remove_wds_addr(wlan_if_t vaphandle,
                          struct ieee80211_node_table *nt,
			  const u_int8_t *macaddr, u_int32_t flags)
{
    int hash;
    struct ieee80211_wds_addr *wds;
    struct ieee80211com *ic = nt->nt_ic;
    u_int8_t *destmac = (u_int8_t *) macaddr;
    bool hmwds_flag_set;
    bool is_node_delete = 0;
    rwlock_state_t lock_state;

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    hmwds_flag_set = (flags & IEEE80211_NODE_F_WDS_HM)?1:0;
#else
    hmwds_flag_set = 0;
#endif
    OS_RWLOCK_WRITE_LOCK(&nt->nt_wds_nodelock, &lock_state);

    hash = IEEE80211_NODE_HASH(macaddr);
    LIST_FOREACH(wds, &nt->nt_wds_hash[hash], wds_hash) {
        if (IEEE80211_ADDR_EQ(wds->wds_macaddr, macaddr)) {
            if ((hmwds_flag_set || (!(wds->flags & IEEE80211_NODE_F_WDS_STAGE)))
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
                    && ((wds->flags & IEEE80211_NODE_F_WDS_HM) == (flags & IEEE80211_NODE_F_WDS_HM))
#endif
                    && (wds->flags & flags)
                    ) {
                ieee80211_free_node(wds->wds_ni);  /* Decrement ref count */
                LIST_REMOVE(wds, wds_hash);
                IEEE80211_DPRINTF_IC_CATEGORY(ic, IEEE80211_MSG_WDS,  "%s: deleting mac(%s)"
                    " from host wds table wds_flags:0x%x flags:0x%x\n", __func__,
                    ether_sprintf(macaddr), wds->flags, flags);
                OS_FREE(wds);
                is_node_delete = 1;
            } else {
                IEEE80211_DPRINTF_IC_CATEGORY(ic, IEEE80211_MSG_WDS, "%s: Not delete Mac(%s)"
                    " from host wds table wds_flags:0x%x flags:0x%x\n", __func__,
                    ether_sprintf(macaddr), wds->flags,flags);
            }
            break;
        }
    }
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);
    if (is_node_delete)
    {
        IEEE80211_DPRINTF_IC_CATEGORY(ic, IEEE80211_MSG_WDS, "%s: Call FW to delete mac(%s)"
            " from FW table flags:0x%x\n", __func__, ether_sprintf(macaddr), flags);
        ic->ic_node_del_wds_entry((void *)(vaphandle->iv_ifp), destmac);
    }
}

/* Remove node references from wds table */
void
ieee80211_del_wds_node(struct ieee80211_node_table *nt,
		       struct ieee80211_node *ni)
{
    int hash;
    struct ieee80211_wds_addr *wds;
    struct ieee80211_wds_addr *wds_next = NULL;

    for (hash=0; hash<IEEE80211_NODE_HASHSIZE; hash++) {
        for ((wds) = LIST_FIRST((&nt->nt_wds_hash[hash])); (wds);) {
            if (wds->wds_ni == ni) {
                /* Instead of freeing the node, make sure that the wds entry is
                 * flagged as stage wds_addr, and force the node pointer to be
                 * NULL. When actual staging timer expires, free the wds entry.
                 */
                wds_next = LIST_NEXT(wds, wds_hash);
                wds->flags |= IEEE80211_NODE_F_WDS_STAGE;
                wds->wds_ni = NULL;
                /* cache the address of the node */
                OS_MEMCPY(wds->wds_ni_macaddr, ni->ni_macaddr, IEEE80211_ADDR_LEN);
                ieee80211_free_node(ni);
                wds = wds_next;
            } else {
                wds = LIST_NEXT(wds, wds_hash);
            }
        }
	}
}

static OS_TIMER_FUNC(ieee80211_node_wds_ageout)
{
    struct ieee80211_node_table *nt;
    struct ieee80211_wds_addr *wds = NULL;
    struct ieee80211_wds_addr *wds_next = NULL;
    int hash;
    rwlock_state_t lock_state;

	OS_GET_TIMER_ARG( nt,struct ieee80211_node_table *);

    OS_RWLOCK_WRITE_LOCK(&nt->nt_wds_nodelock, &lock_state);

    for (hash=0; hash<IEEE80211_NODE_HASHSIZE; hash++) {
        for ((wds) = LIST_FIRST((&nt->nt_wds_hash[hash])); (wds);) {
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
            if (wds->flags & IEEE80211_NODE_F_WDS_HM) {
                wds = LIST_NEXT(wds, wds_hash);
                continue;
            }
#endif

            if (wds->flags & IEEE80211_NODE_F_WDS_STAGE) {
                /* use different count for this */
                wds->wds_staging_age--;
                if (!wds->wds_staging_age) {
                    wds_next = LIST_NEXT(wds, wds_hash);
                    LIST_REMOVE(wds, wds_hash);
                    OS_FREE(wds);
                    wds = wds_next;
                    if (NULL == wds) {
                        break;
                    }
                }
                /* current entry already marked as 'on Stage', but this is not
                 * right time to free the entry, process the next
                 */
                wds = LIST_NEXT(wds, wds_hash);
                continue;
            }

            if (wds->wds_ni && (wds->wds_ni->ni_flags & IEEE80211_NODE_NAWDS) &&
            IEEE80211_ADDR_EQ(wds->wds_macaddr, wds->wds_ni->ni_macaddr))
            {
                wds = LIST_NEXT(wds, wds_hash);
                continue;
            }

            if (!wds->wds_agingcount) {
                wds_next = LIST_NEXT(wds, wds_hash);
                ieee80211_free_node(wds->wds_ni);  /* Decrement ref count */
                LIST_REMOVE(wds, wds_hash);
                OS_FREE(wds);
                wds = wds_next;
            } else {
                wds->wds_agingcount--;
                wds = LIST_NEXT(wds, wds_hash);
            }
        }
	}
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);
    OS_SET_TIMER(&nt->nt_wds_aging_timer,WDS_AGING_TIMER_VAL);

}

void
ieee80211_wds_attach(struct ieee80211_node_table *nt)
{

	OS_INIT_TIMER(nt->nt_ic->ic_osdev,
				  &nt->nt_wds_aging_timer,
	              ieee80211_node_wds_ageout,
	              nt, QDF_TIMER_TYPE_WAKE_APPS);
    OS_RWLOCK_INIT(&nt->nt_wds_nodelock);
    OS_SET_TIMER(&nt->nt_wds_aging_timer,WDS_AGING_TIMER_VAL);
}

void
ieee80211_wds_detach(struct ieee80211_node_table *nt)
{
    OS_FREE_TIMER(&nt->nt_wds_aging_timer);
    OS_RWLOCK_DESTROY(&nt->nt_wds_nodelock);
}


static struct ieee80211_node *
_ieee80211_find_wds_node(
        struct ieee80211_node_table *nt,
        const u_int8_t *macaddr,
        struct ieee80211_wds_addr **wds_stag,
        u_int8_t *stage)
{
    struct ieee80211_node *ni;
    struct ieee80211_wds_addr *wds;
    int hash;

    hash = IEEE80211_NODE_HASH(macaddr);
    LIST_FOREACH(wds, &nt->nt_wds_hash[hash], wds_hash) {
        if (IEEE80211_ADDR_EQ(wds->wds_macaddr, macaddr)) {

            /* if the node is flagged as STAGE, it means, node has gone
             * probably and quickly came back. We should be checking that
             * and add clear the flag, if necessary.
             *
             * If the wireless station behind the AP is moved/roamed onto
             * other AP, l2UF would have cleared the path. But we do not
             * expect the wired stations to go quickly from one wds station
             * to other. In any case, if there any frame coming from one
             * station should clear the path.
             */
            if (!(nt->nt_ic->ic_is_mode_offload(nt->nt_ic)) &&
                (wds->flags & IEEE80211_NODE_F_WDS_STAGE)) {
                    *wds_stag = wds;
                    *stage = 1;
                    return NULL;
            } else {
                ni = wds->wds_ni;
                wds->wds_agingcount = WDS_AGING_COUNT; /* reset the aging count */
                wds->wds_staging_age = 2 * WDS_AGING_COUNT;

                if (ni) {
                    if (!ieee80211_try_ref_node(ni))
                        return NULL;
                }
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
                *wds_stag = wds;
#endif
                return ni;
	        }
        }
    }
    return NULL;
}

void
ieee80211_set_wds_node_time(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
    struct ieee80211_wds_addr *wds;
    int hash;

    hash = IEEE80211_NODE_HASH(macaddr);
    LIST_FOREACH(wds, &nt->nt_wds_hash[hash], wds_hash) {
        if (IEEE80211_ADDR_EQ(wds->wds_macaddr, macaddr)) {
            wds->wds_last_pkt_time = OS_GET_TIMESTAMP();
        }
    }
}
static systime_t
_ieee80211_get_wds_node_time(
        struct ieee80211_node_table *nt,
        const u_int8_t *macaddr)
{
    struct ieee80211_wds_addr *wds;
    int hash;

    hash = IEEE80211_NODE_HASH(macaddr);
    LIST_FOREACH(wds, &nt->nt_wds_hash[hash], wds_hash) {
        if (IEEE80211_ADDR_EQ(wds->wds_macaddr, macaddr)) {
            return wds->wds_last_pkt_time;
        }
    }
    return 0;
}

/* Remove all the wds entries associated with the AP when the AP to
 * which STA is associated goes down
 */
int ieee80211_node_removeall_wds (struct ieee80211_node_table *nt,struct ieee80211_node *ni)
{
    unsigned int hash;
    struct ieee80211_wds_addr *wds;
    struct ieee80211_wds_addr *wds_next;
    rwlock_state_t lock_state;
    OS_RWLOCK_WRITE_LOCK(&nt->nt_wds_nodelock, &lock_state);
    for (hash=0 ;hash < IEEE80211_NODE_HASHSIZE;hash++) {
        for ((wds) = LIST_FIRST((&nt->nt_wds_hash[hash])); (wds);) {
            if (wds->wds_ni == ni) {
                wds_next = LIST_NEXT(wds,wds_hash);
                ieee80211_free_node(wds->wds_ni);
                LIST_REMOVE(wds, wds_hash);
                OS_FREE(wds);
                wds = wds_next;
            } else {
                wds = LIST_NEXT(wds, wds_hash);
            }
        }
	}
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);
    return 0;
}

struct ieee80211_node *
__ieee80211_find_wds_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr, struct ieee80211_wds_addr **wds)
{
    struct ieee80211_node *ni;
    u_int8_t stag=0;


    rwlock_state_t lock_state;
    OS_RWLOCK_WRITE_LOCK(&nt->nt_wds_nodelock, &lock_state);
    ni = _ieee80211_find_wds_node(nt, macaddr, wds, &stag);
    if (!(nt->nt_ic->ic_is_mode_offload(nt->nt_ic))) {
        /* find wds node should return the pointer the ni, for some reasons,
         * wds entry would have been found on staging. At that instance
         * probably we do not have the node pointer. It, means, there is no
         * real association between wds and wds_ni. Now because it is found
         * on staging, try to establish the relationship between these two in
         * a special way.
         *
         * the 'wds' argument should contain the pointer to the wds that is in
         * staging. If node is found, that would be NULL, and NI pointer would
         * be returned properly.
         */
        if (ni == NULL  && stag == 1) {
            if (*wds) {
                ni = _ieee80211_find_node(nt, (*wds)->wds_ni_macaddr);
            } else {
                OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);
                return NULL;
            }
            if (ni && *wds) {
                (*wds)->wds_ni = ni;
                (*wds)->wds_agingcount = WDS_AGING_COUNT;
                (*wds)->wds_staging_age = 2 * WDS_AGING_COUNT;
                (*wds)->flags &= ~IEEE80211_NODE_F_WDS_STAGE;

                IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_WDS,
                        "%s attaching the node macaddr %s wds mac %s\n",
                      __func__, ni->ni_macaddr, macaddr);

                if (!ieee80211_try_ref_node(ni)) {          /* Reference node */
                    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);
                    return NULL;
                }
            } else {
                OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);
                return NULL;
            }
        }
    }
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);
    return ni;
}

struct ieee80211_node *
ieee80211_find_wds_node(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
    struct ieee80211_wds_addr *wds = NULL;

    return __ieee80211_find_wds_node(nt, macaddr, &wds);
}

u_int32_t
ieee80211_find_wds_node_age(struct ieee80211_node_table *nt, const u_int8_t *macaddr)
{
    systime_t wds_time;
    u_int32_t wds_age = 0;

    rwlock_state_t lock_state;
    systime_t time=OS_GET_TIMESTAMP();
    OS_RWLOCK_WRITE_LOCK(&nt->nt_wds_nodelock, &lock_state);
    wds_time = _ieee80211_get_wds_node_time(nt, macaddr);
    wds_age = CONVERT_SYSTEM_TIME_TO_MS((time - wds_time));
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);
    return wds_age;
}

void
wds_clear_wds_table(struct ieee80211_node * ni, struct ieee80211_node_table *nt, wbuf_t wbuf )
{
    struct ieee80211_frame *wh;
    rwlock_state_t lock_state;
    wh = (struct ieee80211_frame *) wbuf_header(wbuf);

    if (ni != ni->ni_vap->iv_bss) {
        struct ieee80211_node *ni_wds=NULL;
        ni_wds = ieee80211_find_wds_node(nt,wh->i_addr2);

        if (ni_wds) {
            OS_BEACON_DECLARE_AND_RESET_VAR(flags);
            OS_BEACON_WRITE_LOCK(&nt->nt_nodelock, &lock_state, flags);
            (void) ieee80211_remove_wds_addr(ni->ni_vap, nt,wh->i_addr2,IEEE80211_NODE_F_WDS_BEHIND | IEEE80211_NODE_F_WDS_REMOTE);
            OS_BEACON_WRITE_UNLOCK(&nt->nt_nodelock, &lock_state, flags);
            ieee80211_free_node(ni_wds);
        }
    }
}
#ifdef ATH_HTC_MII_RXIN_TASKLET
void
ieee80211_nawds_learn(struct ieee80211vap *vap, u_int8_t *mac);

void
ieee80211_nawds_learn_deferwork(void *arg)

{
    struct ieee80211com *ic = (struct ieee80211com *)arg;
    nawds_dentry_t * nawds_entry = NULL ;

    do {

        OS_NAWDSDEFER_LOCKBH(&ic->ic_nawdsdefer_lock);
        nawds_entry = TAILQ_FIRST(&ic->ic_nawdslearnlist);
        if(nawds_entry)
            TAILQ_REMOVE(&ic->ic_nawdslearnlist,nawds_entry,nawds_dlist);
        OS_NAWDSDEFER_UNLOCKBH(&ic->ic_nawdsdefer_lock);
        if(!nawds_entry)
            break;
        ieee80211_nawds_learn(nawds_entry->vap, &nawds_entry->mac[0]);
        OS_FREE(nawds_entry);

    }while(1);
    atomic_set(&ic->ic_nawds_deferflags, DEFER_DONE);
}



void
ieee80211_nawds_learn_defer(struct ieee80211vap *vap, u_int8_t *mac)
{

    struct ieee80211com *ic = vap->iv_ic;

    nawds_dentry_t * nawds_entry ;

    nawds_entry = ( nawds_dentry_t * )  OS_MALLOC(ic->ic_osdev, sizeof(nawds_dentry_t), GFP_KERNEL);
    nawds_entry->vap = vap;

    OS_MEMCPY(&nawds_entry->mac[0],mac,IEEE80211_ADDR_LEN);

    TAILQ_INSERT_TAIL(&ic->ic_nawdslearnlist, nawds_entry, nawds_dlist);


    if(atomic_read(&ic->ic_nawds_deferflags) != DEFER_PENDING){
        {
            atomic_set(&ic->ic_nawds_deferflags, DEFER_PENDING);
            OS_PUT_DEFER_ITEM(ic->ic_osdev,
                    ieee80211_nawds_learn_deferwork,
                    WORK_ITEM_SINGLE_ARG_DEFERED,
                    ic, NULL, NULL);

        }
    }

}

#endif
void
wds_update_rootwds_table(wlan_if_t vaphandle ,struct ieee80211_node * ni, struct ieee80211_node_table *nt, wbuf_t wbuf )
{
    struct ieee80211_frame_addr4 *wh4;
    struct ieee80211_node *ni_wds=NULL;
    struct ieee80211_node *ni_wds_dest=NULL;
    struct ieee80211_node *temp_node=NULL;
    rwlock_state_t lock_state;
#if 0 /* check already perfomed in ieee80211_input.c */
    if (!IEEE80211_VAP_IS_WDS_ENABLED(vap)) {
        IEEE80211_DISCARD(vap, IEEE80211_MSG_INPUT,
                  wh, "data", "%s", "4 addr not allowed");
        goto err;
    }
#endif
    wh4 = (struct ieee80211_frame_addr4 *) wbuf_header(wbuf);
    ni_wds = ieee80211_find_wds_node(nt, wh4->i_addr4);
    /* Last call increments ref count if !NULL */
    if ((ni_wds != NULL) && (ni_wds != ni)) {
        /* node with source address (addr4) moved to another WDS capable
           station. remove the reference to  the previous statation add
           reference to the new one */
        (void) ieee80211_remove_wds_addr(vaphandle, nt,wh4->i_addr4, IEEE80211_NODE_F_WDS_REMOTE);
        ni_wds_dest = ieee80211_find_wds_node(nt, wh4->i_addr3);
        /* node with source address (addr4) and node with address(addr3)
           destination both are reachable through ni. But since ni hands this
           packet to us, we delete the oldest entry of the two(destination) */
        if((ni_wds_dest != NULL) && (ni_wds_dest == ni))
        {
            /* node with source address (addr4) and node with address(addr3)
             * destination both are reachable through ni. But since ni hands this
             * packet to us, we delete the oldest entry of the two(destination) */
            (void) ieee80211_remove_wds_addr(vaphandle, nt,wh4->i_addr3, IEEE80211_NODE_F_WDS_REMOTE);
        }
        if(ni_wds_dest != NULL)
            ieee80211_free_node(ni_wds_dest); /* Decr ref count */
        ieee80211_add_wds_addr(vaphandle, nt, ni, wh4->i_addr4,
            IEEE80211_NODE_F_WDS_REMOTE);
    }
    if (ni_wds == NULL) {
        temp_node = ieee80211_find_node(nt,wh4->i_addr4);
        if (temp_node) {
            if ((temp_node == ni->ni_vap->iv_bss) || (temp_node == ni)) {
                /* Received a frame with wrong SA (it is ourself). Do not update the wds table */
                ieee80211_free_node(temp_node);
                return;
            }

            if(temp_node->ni_vap->iv_opmode == IEEE80211_M_STA) {
                ieee80211_sta_leave(temp_node);
            } else {
                IEEE80211_NODE_LEAVE(temp_node);
            }

            ieee80211_free_node(temp_node);
        }
        ni_wds_dest = ieee80211_find_wds_node(nt, wh4->i_addr3);
        if((ni_wds_dest != NULL) && (ni_wds_dest == ni))
        {
            (void) ieee80211_remove_wds_addr(vaphandle, nt,wh4->i_addr3, IEEE80211_NODE_F_WDS_REMOTE);
        }
        if(ni_wds_dest != NULL)
            ieee80211_free_node(ni_wds_dest); /* Decr ref count */
        ieee80211_add_wds_addr(vaphandle, nt, ni, wh4->i_addr4,
            IEEE80211_NODE_F_WDS_REMOTE);
    }
    else
    {
        OS_RWLOCK_WRITE_LOCK(&nt->nt_wds_nodelock, &lock_state);
        ieee80211_set_wds_node_time(nt, wh4->i_addr4);
        OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);
        ni_wds_dest = ieee80211_find_wds_node(nt, wh4->i_addr3);
        if((ni_wds_dest != NULL) && (ni_wds_dest == ni_wds))
        {
            (void) ieee80211_remove_wds_addr(vaphandle, nt,wh4->i_addr3, IEEE80211_NODE_F_WDS_REMOTE);
        }
        if(ni_wds_dest != NULL)
            ieee80211_free_node(ni_wds_dest); /* Decr ref count */
        ieee80211_free_node(ni_wds); /* Decr ref count */
    }
}

int
wds_sta_chkmcecho(wlan_if_t vaphandle, const u_int8_t *sender )
{
    struct ieee80211_node_table *nt = &(vaphandle->iv_ic->ic_sta);
    struct ieee80211_node *ni_wds=NULL;
    int mcastecho = 0;
    ni_wds = ieee80211_find_wds_node(nt,sender);

    if (ni_wds ) {
        if (ieee80211_find_wds_node_age(nt, sender) < 1000)
        {
            mcastecho = 1;
        }
        else
        {
            ieee80211_remove_wds_addr(vaphandle, nt,sender, IEEE80211_NODE_F_WDS_BEHIND);
        }
        ieee80211_free_node(ni_wds); /* Decr ref count */
    }

    return mcastecho;
}
/* Due to OWL specific HW bug: Deny aggregation if
 * we're Owl in WDS mode and
 * the remote node hasn't sent us an IE indicating they're Atheros Owl or later
 * and we're a WDS client or we're WDS AP and the remote node is discovered
 * to be a WDS client.
 * Disablement of this is controlled by toggling IEEE80211_C_WDS_AUTODETECT
 * via "iwpriv athN wdsdetect 0".
 */
int ieee80211_node_wdswar_isaggrdeny(struct ieee80211_node *ni)
{
    return (((ni->ni_vap->iv_flags_ext & IEEE80211_FEXT_WDS) &&
	     (ni->ni_vap->iv_flags_ext & IEEE80211_C_WDS_AUTODETECT) &&
	     (ni->ni_vap->iv_ic->ic_ath_extcap & IEEE80211_ATHEC_OWLWDSWAR) &&
	     !(ni->ni_flags & IEEE80211_NODE_ATH) &&
	     ((ni->ni_vap->iv_opmode & IEEE80211_M_STA) ||
	      ((ni->ni_vap->iv_opmode & IEEE80211_M_HOSTAP) &&
	       (ni->ni_flags & IEEE80211_NODE_WDS)))) != 0 );

}

/* Due to OWL specific HW bug, send a DELBA to remote node when we detect
 * that they're a WDS link potentially sending aggregates to us.
 * We do this if we're Owl in WDS mode and
 * the remote node hasn't sent us an IE indicating they're Atheros Owl or later
 * and we're a WDS AP and the remote node is discovered to be a WDS client.
 * Disablement of this is controlled by toggling IEEE80211_C_WDS_AUTODETECT
 * via "iwpriv athN wdsdetect 0".
 */
int ieee80211_node_wdswar_issenddelba(struct ieee80211_node *ni)
{
    return (((ni->ni_vap->iv_flags_ext & IEEE80211_FEXT_WDS) &&
	     (ni->ni_vap->iv_flags_ext & IEEE80211_C_WDS_AUTODETECT) &&
	     (ni->ni_ic->ic_ath_extcap & IEEE80211_ATHEC_OWLWDSWAR) &&
	     !(ni->ni_flags & IEEE80211_NODE_ATH) &&
	     (ni->ni_vap->iv_opmode & IEEE80211_M_HOSTAP)) != 0 );
}

#if UMAC_SUPPORT_NAWDS

#ifndef UMAC_MAX_NAWDS_REPEATER
#error NAWDS feature is enabled but UMAC_MAX_NAWDS_REPEATER is not defined
#endif

/* codes for Non-Associated WDS - NAWDS */
/* common functions for UMAC and MLME support */
static int
is_nawds_valid_mac(char *addr)
{
    char nullmac[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    if (IEEE80211_IS_MULTICAST(addr) ||
        IEEE80211_ADDR_EQ(addr, nullmac))
        return 0;

    return 1;
}

static bool
is_nawds_mac_bss_mac(struct ieee80211com *ic, char *mac)
{
    struct ieee80211_node *ni = NULL;

    ni = ieee80211_find_node(&ic->ic_sta, mac);
    if (ni) {
        if (ni->ni_vap->iv_bss == ni) {
            ieee80211_free_node(ni);
            return true;
        }
        ieee80211_free_node(ni);
    }

    return false;
}

static int
is_nawds_valid_caps(struct ieee80211com *ic, u_int32_t caps)
{
    u_int32_t ht_flags, vht_flags, he_flags, he_axa_flags, he_axg_flags;
    bool is_160_or_80p80_supported = false;

    if (ic->ic_modecaps &
                ((1 << IEEE80211_MODE_11AC_VHT160) |
                 (1 << IEEE80211_MODE_11AC_VHT80_80) |
                 (1 << IEEE80211_MODE_11AXA_HE160) |
                 (1ULL << IEEE80211_MODE_11AXA_HE80_80))) {
        is_160_or_80p80_supported = true;
    }

    if(caps & NAWDS_INVALID_CAP_MODE)
        return 0;

    if(!is_160_or_80p80_supported && (caps & (NAWDS_REPEATER_CAP_11ACVHT80_80 |
                                              NAWDS_REPEATER_CAP_11ACVHT160 |
                                              NAWDS_REPEATER_CAP_11AXAHE80_80 |
                                              NAWDS_REPEATER_CAP_11AXAHE160)))
        return 0;

    ht_flags  = (NAWDS_REPEATER_CAP_HT20 | NAWDS_REPEATER_CAP_HT2040);
    vht_flags = (NAWDS_REPEATER_CAP_11ACVHT20 | NAWDS_REPEATER_CAP_11ACVHT40 | NAWDS_REPEATER_CAP_11ACVHT80);
    he_axa_flags = (NAWDS_REPEATER_CAP_11AXAHE20 | NAWDS_REPEATER_CAP_11AXAHE40 | NAWDS_REPEATER_CAP_11AXAHE80);
    he_axg_flags = (NAWDS_REPEATER_CAP_11AXGHE20 | NAWDS_REPEATER_CAP_11AXGHE40);
    he_flags = he_axa_flags | he_axg_flags;

    if(is_160_or_80p80_supported) {
        vht_flags |= (NAWDS_REPEATER_CAP_11ACVHT80_80 | NAWDS_REPEATER_CAP_11ACVHT160);
        he_flags |= (NAWDS_REPEATER_CAP_11AXAHE80_80 |  NAWDS_REPEATER_CAP_11AXAHE160);
    }
    if (caps & (NAWDS_MULTI_STREAMS)) {
        if (!((caps & he_flags) || (caps & vht_flags) || (caps & ht_flags))){
            return 0;
        }
    }

    if (caps & NAWDS_5TO8_STREAMS) {
        if (!((caps & he_axa_flags) || (caps & vht_flags)))
            return 0;
    }

    // check if radio nss caps match
    if ((caps & NAWDS_REPEATER_CAP_8S) && (ic->ic_spatialstreams < 8)) {
        return 0;
    }
    else if ((caps & NAWDS_REPEATER_CAP_7S) && (ic->ic_spatialstreams < 7)) {
        return 0;
    }
    else if ((caps & NAWDS_REPEATER_CAP_6S) && (ic->ic_spatialstreams < 6)) {
        return 0;
    }
    else if ((caps & NAWDS_REPEATER_CAP_5S) && (ic->ic_spatialstreams < 5)) {
        return 0;
    }
    else if ((caps & NAWDS_REPEATER_CAP_4S) && (ic->ic_spatialstreams < 4)) {
        return 0;
    }
    else if ((caps & NAWDS_REPEATER_CAP_TS) && (ic->ic_spatialstreams < 3)) {
        return 0;
    }
    else if ((caps & NAWDS_REPEATER_CAP_DS) && (ic->ic_spatialstreams < 2)) {
        return 0;
    }
    return 1;
}

/* UMAC Support Functions */
void
ieee80211_nawds_attach(struct ieee80211vap *vap)
{
    OS_MEMZERO(&vap->iv_nawds, sizeof(struct ieee80211_nawds));
    NAWDS_LOCK_INIT(&vap->iv_nawds.lock);
}

int ieee80211_nawds_send_wbuf(struct ieee80211vap *vap, wbuf_t wbuf)
{
    int i, count = 0;
    struct ieee80211_node *rep_ni, *src_ni, *wbuf_ni = NULL;
    struct ieee80211_nawds *nawds = &vap->iv_nawds;
    struct ieee80211com *ic = vap->iv_ic;
    struct ether_header *eh;
    wbuf_t wbuf1 = NULL;

    /* Do not send nawds packet when mode is off or node is inactive */
    wbuf_ni = wlan_wbuf_get_peer_node(wbuf);
    if ((wbuf_ni != NULL) &&
        (wbuf_ni->ni_flags & IEEE80211_NODE_NAWDS)) {
        if ((nawds->mode == IEEE80211_NAWDS_DISABLED) ||
            (wbuf_ni->ni_inact <= 1)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS,
                "%s: nawds mode off or node inactive\n", __func__);
            ieee80211_free_node(wbuf_ni);
            wbuf_complete(wbuf);
            return -1;
        }
    }

    if (nawds->mode == IEEE80211_NAWDS_DISABLED)
        return 0;

    eh = (struct ether_header *)wbuf_header(wbuf);

    if (!((vap->iv_flags_ext & IEEE80211_FEXT_WDS) &&
         (IEEE80211_IS_MULTICAST(eh->ether_dhost))))
        return 0;

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS, "nawds: %s: ",
            ether_sprintf(eh->ether_dhost));

    for (i = 0; i < UMAC_MAX_NAWDS_REPEATER; i++) {
        if (!is_nawds_valid_mac(nawds->repeater[i].mac))
            continue;
        if ((rep_ni = ieee80211_find_node(&ic->ic_sta, nawds->repeater[i].mac)) == NULL) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS, "missing node: %s\n",
                    ether_sprintf(nawds->repeater[i].mac));
            continue;
        }
        if (!(rep_ni->ni_flags & IEEE80211_NODE_NAWDS)) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS, "node without flags: %s\n",
                    ether_sprintf(nawds->repeater[i].mac));
            ieee80211_free_node(rep_ni);
            continue;
        }
        if (rep_ni->ni_inact <= 1) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS, "node inactive: %s\n",
                    ether_sprintf(nawds->repeater[i].mac));
            ieee80211_free_node(rep_ni);
            continue;
        }
        /* To avoid a bcast storm we need to check if the src is reachable
         * over this repeater; if it is, skip this process for this
         * repeater alone and contine to send on other repeaters
         */
        src_ni = ieee80211_find_txnode(vap, eh->ether_shost);
        if (rep_ni == src_ni) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS, "drop: %s\n",
                    ether_sprintf(nawds->repeater[i].mac));
            ieee80211_free_node(rep_ni);
            ieee80211_free_node(src_ni);
            continue;
        }
        if (src_ni)
            ieee80211_free_node(src_ni);

        /* copy buf and send it out */
        wbuf1 = wbuf_copy(wbuf);
        if(wbuf1 == NULL) {
           /* wbuf_copy() failed */
           ieee80211_free_node(rep_ni);
           return 0;
        }
        wlan_wbuf_set_peer_node(wbuf1, rep_ni);
        wbuf_clear_flags(wbuf1);
        vap->iv_evtable->wlan_dev_xmit_queue(vap->iv_ifp, wbuf1);

        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS, "send: %s ref: %d\n",
                ether_sprintf(nawds->repeater[i].mac),
                ieee80211_node_refcnt(rep_ni));
        count++;
    }

    return count;
}

int
ieee80211_nawds_disable_beacon(struct ieee80211vap *vap)
{
    if ((vap->iv_nawds.mode == IEEE80211_NAWDS_STATIC_BRIDGE) ||
        (vap->iv_nawds.mode == IEEE80211_NAWDS_LEARNING_BRIDGE))
        return 1;
    return 0;
}

int
ieee80211_nawds_enable_learning(struct ieee80211vap *vap)
{
    if ((vap->iv_nawds.mode == IEEE80211_NAWDS_LEARNING_REPEATER) ||
        (vap->iv_nawds.mode == IEEE80211_NAWDS_LEARNING_BRIDGE))
        return 1;
    return 0;
}

void
ieee80211_nawds_learn(struct ieee80211vap *vap, u_int8_t *mac)
{
    wlan_nawds_config_mac(vap, mac, vap->iv_nawds.defcaps);
	IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS, "NAWDS repeater learned %s: %d\n",
            ether_sprintf(mac),vap->iv_nawds.defcaps);
    wlan_nawds_config_key(vap, mac, vap->iv_nawds.psk);
}


/* IEEE80211 MLME support functions */
static void
ieee80211_nawds_node_leave(wlan_if_t vaphandle, u_int8_t *addr)
{
    int i;
    struct ieee80211_node* ni;
	struct ieee80211vap *vap = vaphandle;
    struct ieee80211_nawds *nawds = &vap->iv_nawds;
    struct ieee80211com *ic = vap->iv_ic;

    for(i = 0; i < UMAC_MAX_NAWDS_REPEATER; i++) {
        if (IEEE80211_ADDR_EQ(nawds->repeater[i].mac, addr))
            break;
    }

    if (i == UMAC_MAX_NAWDS_REPEATER)
        return;

    /* reclaim the node */
    ni = ieee80211_find_node(&ic->ic_sta, addr);
    if (ni) {
        IEEE80211_NODE_LEAVE(ni);
        ieee80211_free_node(ni);
    }

    /* clear NAWDS node table for the mac */
    nawds->repeater[i].caps = 0;
    OS_MEMZERO(nawds->repeater[i].mac, IEEE80211_ADDR_LEN);
}

static void
ieee80211_nawds_node_leave_all(wlan_if_t vaphandle)
{
    int i;
	struct ieee80211vap *vap = vaphandle;
    struct ieee80211_nawds *nawds = &vap->iv_nawds;

    for(i = 0; i < UMAC_MAX_NAWDS_REPEATER; i++) {
        if (is_nawds_valid_mac(nawds->repeater[i].mac)) {
            ieee80211_nawds_node_leave(vap, nawds->repeater[i].mac);
        }
    }
}

/* This function is called to configure a repeater node as a HT node.
 * To avoid too much configuration, defaults are assumed as follows
 * MAX A-MPDU factor (valid range 0-3) default 2
 * MAX mpdudensity (valid range 0-7) default 7
 * SHORTGI not supprted as HT40 is not supported
 */
#define ATH_WDS_SINGLE_STREAM_REP_MAXAMPDUFACTOR 2
#define ATH_WDS_SINGLE_STREAM_REP_MPDUDENSITY    7
#define ATH_WDS_DOUBLE_STREAM_REP_MAXAMPDUFACTOR 3
#define ATH_WDS_DOUBLE_STREAM_REP_MPDUDENSITY    0
#define ATH_WDS_TRIPLE_STREAM_REP_MAXAMPDUFACTOR 3
#define ATH_WDS_TRIPLE_STREAM_REP_MPDUDENSITY    6
#define ATH_WDS_4_STREAM_REP_MAXAMPDUFACTOR 3
#define ATH_WDS_4_STREAM_REP_MPDUDENSITY    6
static int
ieee80211_nawds_config_ht(struct ieee80211_node *ni, u_int32_t caps)
{
    struct ieee80211_node * vapnode;
    struct ieee80211com   *ic = ni->ni_ic;
    struct ieee80211vap   *vap = ni->ni_vap;
    u_int32_t  mpdudensity = 0;
    u_int16_t  maxampdufactor = 0;
    struct vdev_mlme_obj *vdev_mlme = vap->vdev_mlme;

    /* Check the HT20/40 ratesets */
    if (caps & NAWDS_REPEATER_CAP_HT2040)
        ni->ni_chwidth = IEEE80211_CWM_WIDTH40;
    else
        ni->ni_chwidth = IEEE80211_CWM_WIDTH20;

    /* Check if DS rates could be used */
    ni->ni_htcap &= (~IEEE80211_HTCAP_C_SM_MASK);
    ni->ni_htcap &= ~(IEEE80211_HTCAP_C_SHORTGI40);

    if (caps & (NAWDS_REPEATER_CAP_DS |
                NAWDS_REPEATER_CAP_TS |
                NAWDS_REPEATER_CAP_4S |
                NAWDS_REPEATER_CAP_5S |
                NAWDS_REPEATER_CAP_6S |
                NAWDS_REPEATER_CAP_7S |
                NAWDS_REPEATER_CAP_8S)){
        ni->ni_htcap |= IEEE80211_HTCAP_C_TXSTBC;
        ni->ni_htcap |= (IEEE80211_HTCAP_C_RXSTBC & ( 1 << IEEE80211_HTCAP_C_RXSTBC_S));
        ni->ni_htcap |= IEEE80211_HTCAP_C_ADVCODING;
        ni->ni_htcap |= IEEE80211_HTCAP_C_SMPOWERSAVE_DISABLED;
        ni->ni_htcap |= IEEE80211_HTCAP_C_SHORTGI20;

        ni->ni_htcap &= (((vdev_mlme->proto.generic.ldpc & IEEE80211_HTCAP_C_LDPC_RX) &&
              (ieee80211com_get_ldpccap(ic) & IEEE80211_HTCAP_C_LDPC_RX)) ?
              ni->ni_htcap : ~IEEE80211_HTCAP_C_ADVCODING);

        if (caps & NAWDS_REPEATER_CAP_HT2040) {
            ni->ni_htcap |= IEEE80211_HTCAP_C_SHORTGI40;
            ni->ni_htcap |= IEEE80211_HTCAP_C_CHWIDTH40;
        }

        if (IEEE80211_IS_CHAN_11NA(vap->iv_bsschan) ||
            IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
            IEEE80211_IS_CHAN_11AXA(vap->iv_bsschan))
            ni->ni_htcap &= ~IEEE80211_HTCAP_C_DSSSCCK40;
        else
            ni->ni_htcap |= IEEE80211_HTCAP_C_DSSSCCK40;

        ni->ni_updaterates = IEEE80211_NODE_SM_EN;

        if (caps & NAWDS_5TO8_STREAMS) {
            /*
             * ni_streams will later be increased to the required value in the
             * VHT specific configuration routine.
             */
            ni->ni_streams = 4;

            mpdudensity = ATH_WDS_4_STREAM_REP_MPDUDENSITY;
            maxampdufactor = ATH_WDS_4_STREAM_REP_MAXAMPDUFACTOR;
        } else if (caps & NAWDS_REPEATER_CAP_4S) {
            ni->ni_streams = 4;
            mpdudensity = ATH_WDS_4_STREAM_REP_MPDUDENSITY;
            maxampdufactor = ATH_WDS_4_STREAM_REP_MAXAMPDUFACTOR;
        } else if (caps & NAWDS_REPEATER_CAP_TS) {
            ni->ni_streams = 3;
            mpdudensity = ATH_WDS_TRIPLE_STREAM_REP_MPDUDENSITY;
            maxampdufactor = ATH_WDS_TRIPLE_STREAM_REP_MAXAMPDUFACTOR;
        } else {
            ni->ni_streams = 2;
            mpdudensity = ATH_WDS_DOUBLE_STREAM_REP_MPDUDENSITY;
            maxampdufactor = ATH_WDS_DOUBLE_STREAM_REP_MAXAMPDUFACTOR;
        }

    } else {
        ni->ni_htcap |= IEEE80211_HTCAP_C_SMPOWERSAVE_STATIC;
        ni->ni_updaterates = IEEE80211_NODE_SM_PWRSAV_STAT;
        ni->ni_streams = 1;
        mpdudensity = ATH_WDS_SINGLE_STREAM_REP_MPDUDENSITY;
        maxampdufactor =  ATH_WDS_SINGLE_STREAM_REP_MAXAMPDUFACTOR;
    }

    if (ni->ni_streams > vdev_mlme->proto.generic.nss) {
        ni->ni_streams = vdev_mlme->proto.generic.nss;
    }

#ifdef ATH_SUPPORT_TxBF
    if (caps &  NAWDS_REPEATER_CAP_TXBF){
        ni->ni_txbf.value = ic->ic_txbf.value; /* force node's txbf setting as local setting*/
        ieee80211_match_txbfcapability(ic, ni);
    }
#endif

    ni->ni_htcap &= (((vap->iv_tx_stbc) && (ni->ni_streams > 1)) ? \
            ni->ni_htcap : ~IEEE80211_HTCAP_C_TXSTBC);
    ni->ni_htcap &= (((vap->iv_rx_stbc) && (ni->ni_streams > 0)) ? \
            ni->ni_htcap : ~IEEE80211_HTCAP_C_RXSTBC);

    /* mark the node as HT-capable */
    ieee80211node_set_flag(ni, IEEE80211_NODE_HT);

    /*
     * The Maximum Rx A-MPDU defined by this field is equal to
     *   (2^^(13 + Maximum Rx A-MPDU Factor)) - 1
     * octets.  Maximum Rx A-MPDU Factor is an integer in the
     * range 0 to 3.
     */
    ni->ni_maxampdu = ((1u << (IEEE80211_HTCAP_MAXRXAMPDU_FACTOR + maxampdufactor)) - 1);
    ni->ni_mpdudensity = ieee80211_parse_mpdudensity(mpdudensity);

    /* copy the VAP HT Rates */
    vapnode = ni->ni_vap->iv_bss;
    memcpy(&(ni->ni_htrates), &(vapnode->ni_htrates), sizeof(struct ieee80211_rateset));

    if (ic->ic_set_ampduparams) {
        ic->ic_set_ampduparams(ni);
    }

    return 0;
}

static int
ieee80211_nawds_config_base(struct ieee80211_node *ni, int caps, uint8_t ht_disable)
{

    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic ;
    struct ieee80211_node * vapni;

    /* sanity check */
    if ((vap == NULL) || ((ic = vap->iv_ic) == NULL)) {
        return -EINVAL;
    }

    vapni = vap->iv_bss;

    /* configure the capabilties */
    ni->ni_capinfo = vapni->ni_capinfo;
    ieee80211node_set_flag(ni, vapni->ni_flags);

    /* for bkward compat I assume bg unless configured otherwise */
    ieee80211node_clear_flag(ni, IEEE80211_NODE_HT);
    ieee80211node_clear_flag(ni, IEEE80211_NODE_WDS);
    ieee80211node_set_flag(ni, IEEE80211_NODE_QOS);
    ieee80211node_set_flag(ni, IEEE80211_NODE_ERP);

    /* 11AX TODO (Phase III) - Check for 11ax changes required here */

    if ((caps &  NAWDS_REPEATER_CAP_DS) ||
            (caps & NAWDS_REPEATER_CAP_TS) ||
            (caps & NAWDS_REPEATER_CAP_4S) ||
            (caps & NAWDS_REPEATER_CAP_5S) ||
            (caps & NAWDS_REPEATER_CAP_6S) ||
            (caps & NAWDS_REPEATER_CAP_7S) ||
            (caps & NAWDS_REPEATER_CAP_8S) ||
            ht_disable) {
        if (ht_disable || IEEE80211_IS_CHAN_11NG(vap->iv_bsschan)) {
            ni->ni_capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
            ni->ni_capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
            IEEE80211_DISABLE_PROTECTION(ic);
            ic->ic_protmode = IEEE80211_PROT_NONE;
            ieee80211_set_protmode(ic);
        }
    }

    /* copy the rates and xrates */
    memcpy(&(ni->ni_rates), &(vapni->ni_rates), sizeof(struct ieee80211_rateset));

    if ((ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_PREAMBLE) == 0) {
        /* set preamble to no short preamble */
        ieee80211com_clear_flags(ic, IEEE80211_F_SHPREAMBLE);
        ieee80211com_set_flags(ic, IEEE80211_F_USEBARKER);
    }
    else {
        /* set preamble to short preamble */
        ieee80211com_set_flags(ic, IEEE80211_F_SHPREAMBLE);
        ieee80211com_clear_flags(ic, IEEE80211_F_USEBARKER);
    }

    ieee80211_set_shortslottime(ic,
            IEEE80211_IS_CHAN_A(ic->ic_curchan) ||
            IEEE80211_IS_CHAN_11NA(ic->ic_curchan) ||
            (ni->ni_capinfo & IEEE80211_CAPINFO_SHORT_SLOTTIME));

    wlan_pdev_beacon_update(ic);

    return 0;
}

#if (MESH_MODE_SUPPORT||ATH_SUPPORT_NAC)
static int
ieee80211_localpeer_config_base(struct ieee80211_node *ni, int caps, uint8_t ht_disable)
{

    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211com *ic ;
    struct ieee80211_node * vapni;
    struct ieee80211_ath_channel    *chan;

    /* sanity check */
    if ((vap == NULL) || ((ic = vap->iv_ic) == NULL)) {
        return -EINVAL;
    }

    vapni = vap->iv_bss;

    /* configure the capabilties */
    ni->ni_capinfo = vapni->ni_capinfo;
    ieee80211node_set_flag(ni, vapni->ni_flags);

    /* for bkward compat I assume bg unless configured otherwise */
    ieee80211node_clear_flag(ni, IEEE80211_NODE_HT);
    ieee80211node_clear_flag(ni, IEEE80211_NODE_WDS);
    ieee80211node_set_flag(ni, IEEE80211_NODE_QOS);
    ieee80211node_set_flag(ni, IEEE80211_NODE_ERP);

    /* 11AX TODO (Phase II) - Check for 11ax changes required here */

    chan = vap->iv_bsschan;

    if (ic->ic_num_ap_vaps == 1) {
        /* Only change/update the global ic_protmode if there's only a mesh vap.
           Otherwise, follow the other VAP's setting */
        if (ht_disable || IEEE80211_IS_CHAN_11NG(chan) || IEEE80211_IS_CHAN_ANYG(chan)) {
            IEEE80211_ENABLE_PROTECTION(ic);
            ic->ic_protmode = IEEE80211_PROT_RTSCTS;
            ieee80211_set_protmode(ic);
        }
    }

    if (ic->ic_flags & IEEE80211_F_SHSLOT) {
        ni->ni_capinfo |= IEEE80211_CAPINFO_SHORT_SLOTTIME;
    }

    if (ic->ic_flags & IEEE80211_F_SHPREAMBLE) {
        ni->ni_capinfo |= IEEE80211_CAPINFO_SHORT_PREAMBLE;
    }

    /* copy the rates and xrates */
    memcpy(&(ni->ni_rates), &(vapni->ni_rates), sizeof(struct ieee80211_rateset));

    wlan_pdev_beacon_update(ic);

    return 0;
}

#endif
/*
 * VHT Configuration :
 * 1. vhtcap
 * 2. rx_max_rate;
 *    rx_vhtrates;
 *    tx_max_rate;
 *    ni_tx_vhtrates;
 *    IEEE80211_NODE_VHT
 */
static int
ieee80211_nawds_config_vht(struct ieee80211_node *ni, u_int32_t caps, bool target_update)
{
    struct ieee80211com   *ic = ni->ni_ic;
    struct ieee80211vap   *vap = ni->ni_vap;
    u_int32_t vhtcap_info, ampdu_len = 0;
    u_int8_t  chwidth = 0;
    bool is_160_or_80p80_supported = false;
    struct vdev_mlme_obj *vdev_mlme = vap->vdev_mlme;

    if (ic->ic_modecaps &
                ((1 << IEEE80211_MODE_11AC_VHT160) |
                 (1 << IEEE80211_MODE_11AC_VHT80_80))) {
        is_160_or_80p80_supported = true;
    }

    /* Fill in the VHT capabilities info */
    vhtcap_info    = ic->ic_vhtcap;
    vhtcap_info   &= ((vap->iv_sgi) ? ic->ic_vhtcap : ~IEEE80211_VHTCAP_SHORTGI_80);
    if(is_160_or_80p80_supported && ((caps & NAWDS_REPEATER_CAP_11ACVHT80_80)
                                     || (caps & NAWDS_REPEATER_CAP_11ACVHT160)))
    vhtcap_info   &= ((vap->iv_sgi) ? ic->ic_vhtcap : ~IEEE80211_VHTCAP_SHORTGI_160);/* Short GI for 160 and 80+80 MHz */
    vhtcap_info   &= ((vdev_mlme->proto.generic.ldpc & IEEE80211_HTCAP_C_LDPC_RX) ?  ic->ic_vhtcap  : ~IEEE80211_VHTCAP_RX_LDPC);
    vhtcap_info   &= ((vap->iv_tx_stbc) ?  ic->ic_vhtcap : ~IEEE80211_VHTCAP_TX_STBC);
    vhtcap_info   &= ((vap->iv_rx_stbc) ?  ic->ic_vhtcap : ~IEEE80211_VHTCAP_RX_STBC);

    /* Disable beam-forming capability in NAWDS mode */
    vhtcap_info &= ~(IEEE80211_VHTCAP_SU_BFORMER | IEEE80211_VHTCAP_SU_BFORMEE |
            IEEE80211_VHTCAP_MU_BFORMER | IEEE80211_VHTCAP_MU_BFORMEE);

    ni->ni_vhtcap  = htole32(vhtcap_info);

    /* Set Chwidth depending on defcaps */
    if(is_160_or_80p80_supported && ((caps & NAWDS_REPEATER_CAP_11ACVHT80_80)
           || (caps & NAWDS_REPEATER_CAP_11ACVHT160)))
        chwidth = IEEE80211_CWM_WIDTH160;
    else if(caps & NAWDS_REPEATER_CAP_11ACVHT80)
        chwidth = IEEE80211_CWM_WIDTH80;
    else if(caps & NAWDS_REPEATER_CAP_11ACVHT40)
        chwidth = IEEE80211_CWM_WIDTH40;
    else if(caps & NAWDS_REPEATER_CAP_11ACVHT20)
        chwidth = IEEE80211_CWM_WIDTH20;

    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
        switch(chwidth) {
            case IEEE80211_CWM_WIDTH20:
                ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
            break;

            case IEEE80211_CWM_WIDTH40:
                    ni->ni_chwidth = IEEE80211_CWM_WIDTH40;
            break;

            case IEEE80211_CWM_WIDTH80:
                    ni->ni_chwidth = IEEE80211_CWM_WIDTH80;
            break;

            case IEEE80211_CWM_WIDTH160:
                    ni->ni_chwidth = IEEE80211_CWM_WIDTH160;
            break;

            default:
                /* Do nothing */
            break;
        }
    }

    /* Fill in the VHT MCS info */
    ieee80211_set_vht_rates(ic,vap);

    /*
     * The Maximum Rx A-MPDU defined by this field is equal to
     *   (2^^(13 + Maximum Rx A-MPDU Factor)) - 1
     * octets.  Maximum Rx A-MPDU Factor is an integer in the
     * range 0 to 7.
     */
    ampdu_len = (le32toh(ni->ni_vhtcap) & IEEE80211_VHTCAP_MAX_AMPDU_LEN_EXP) >> IEEE80211_VHTCAP_MAX_AMPDU_LEN_EXP_S;
    ni->ni_maxampdu = (1u << (IEEE80211_VHTCAP_MAX_AMPDU_LEN_FACTOR + ampdu_len)) -1;
    ni->ni_tx_vhtrates = vap->iv_vhtcap_max_mcs.tx_mcs_set.mcs_map;
    ni->ni_tx_max_rate = vap->iv_vhtcap_max_mcs.tx_mcs_set.data_rate;
    ni->ni_rx_vhtrates = vap->iv_vhtcap_max_mcs.rx_mcs_set.mcs_map;
    ni->ni_rx_max_rate = vap->iv_vhtcap_max_mcs.rx_mcs_set.data_rate;

    ieee80211node_set_flag(ni, IEEE80211_NODE_VHT);

    /* Streams decision based on TS/DS or higher flags */

    if (caps & NAWDS_REPEATER_CAP_8S) {
        ni->ni_streams = 8;
    }
    else if (caps & NAWDS_REPEATER_CAP_7S) {
        ni->ni_streams = 7;
    }
    else if (caps & NAWDS_REPEATER_CAP_6S) {
        ni->ni_streams = 6;
    }
    else if (caps & NAWDS_REPEATER_CAP_5S) {
        ni->ni_streams = 5;
    }
    else if (caps & NAWDS_REPEATER_CAP_4S) {
        ni->ni_streams = 4;
    }
    else if (caps & NAWDS_REPEATER_CAP_TS) {
        ni->ni_streams = 3;
    }
    else if (caps & NAWDS_REPEATER_CAP_DS) {
        ni->ni_streams = 2;
    } else {
        ni->ni_streams = 1;
    }
    if (ni->ni_streams > vdev_mlme->proto.generic.nss) {
        ni->ni_streams = vdev_mlme->proto.generic.nss;
    }
    /*
     * Update NSS and CHWIDTH params on target
     */
    if (target_update) {
        if (ic->ic_nss_change != NULL)
            ic->ic_nss_change(ni);
    }

    return 0;
}

static void ieee80211_copy_hecap_rates(struct ieee80211com *ic,
                                       struct ieee80211vap  *vap,
                                       struct ieee80211_node *ni)
{
    struct ieee80211_he_handle *ic_he  = &ic->ic_he;
    struct ieee80211_he_handle *ni_he  = &ni->ni_he;

    if(ni->ni_phymode > IEEE80211_MODE_11AXA_HE80_80) {
        ieee80211_note(vap, IEEE80211_MSG_WDS,
                   "%s WARNING!!! Unsupported ni_phymode=%x\n",
                    __func__, ni->ni_phymode);
        return;
    }

    switch(ni->ni_phymode) {
        case IEEE80211_MODE_11AXA_HE80_80:
            ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] =
                ic_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80];
            ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80] =
                ic_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80_80];
            /* fall through */

        case IEEE80211_MODE_11AXA_HE160:
            ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160] =
              ic_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160];
            ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160] =
              ic_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_160];
            /* fall through */

        default:
            ni_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80] =
              ic_he->hecap_rxmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80];
            ni_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80] =
              ic_he->hecap_txmcsnssmap[HECAP_TXRX_MCS_NSS_IDX_80];
            break;
    }
}

static void
ieee80211_nawds_config_he(struct ieee80211_node *ni, u_int32_t caps)
{
    struct ieee80211com *ic = ni->ni_ic;
    struct ieee80211vap *vap = ni->ni_vap;
    struct ieee80211_he_handle *ni_he, *ic_he;
    u_int32_t *ic_hecap_phyinfo, val, *ni_hecap_phyinfo;
    u_int8_t rx_streams, tx_streams;
    bool is_160_or_80p80_supported = false;
    struct vdev_mlme_obj *vdev_mlme = vap->vdev_mlme;

    if (ic->ic_modecaps &
                ((1 << IEEE80211_MODE_11AXA_HE160) |
                 (1ULL << IEEE80211_MODE_11AXA_HE80_80))) {
        is_160_or_80p80_supported = true;
    }

     ni_he = &ni->ni_he;
     ic_he = &ic->ic_he;

     qdf_mem_copy(ni_he, ic_he, sizeof(*ni_he));

     ni_hecap_phyinfo = &ni_he->hecap_phyinfo[HECAP_PHYBYTE_IDX0];
     ic_hecap_phyinfo = &ic_he->hecap_phyinfo[HECAP_PHYBYTE_IDX0];

     val = HECAP_PHY_LDPC_GET_FROM_IC(ic_hecap_phyinfo);
     if (!(val && vdev_mlme->proto.generic.ldpc)) {
       val = 0;
     }
     HECAP_PHY_LDPC_SET_TO_IC(ni_hecap_phyinfo, val);

     IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS,
                       "%s:ldpc:%d \n", __func__, val);

     rx_streams = ieee80211_get_rxstreams(ic, vap);
     tx_streams = ieee80211_get_txstreams(ic, vap);

     val = HECAP_PHY_TXSTBC_GET_FROM_IC(ic_hecap_phyinfo);
     if (!(val && vap->iv_tx_stbc && (tx_streams > 1))) {
       val = 0;
     }
     HECAP_PHY_TXSTBC_SET_TO_IC(ni_hecap_phyinfo, val);

     IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS,
                       "%s:tx_stbc:%d \n", __func__, val);

     val = HECAP_PHY_RXSTBC_GET_FROM_IC(ic_hecap_phyinfo);
     if (!(val && vap->iv_rx_stbc && (rx_streams > 1))) {
       val = 0;
     }
     HECAP_PHY_RXSTBC_SET_TO_IC(ni_hecap_phyinfo, val);

     IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS,
                       "%s:rx_stbc:%d \n", __func__, val);

     /* Disable beam-forming capability in NAWDS mode */
     HECAP_PHY_SUBFMR_SET_TO_IC(ni_hecap_phyinfo, 0);
     HECAP_PHY_SUBFME_SET_TO_IC(ni_hecap_phyinfo, 0);
     HECAP_PHY_MUBFMR_SET_TO_IC(ni_hecap_phyinfo, 0);

    /* Set Chwidth depending on defcaps */
    if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
        if(is_160_or_80p80_supported && (caps &
                       (NAWDS_REPEATER_CAP_11AXAHE80_80 |
                        NAWDS_REPEATER_CAP_11AXAHE160)))
            ni->ni_chwidth = IEEE80211_CWM_WIDTH160;
        else if(caps & NAWDS_REPEATER_CAP_11AXAHE80)
            ni->ni_chwidth = IEEE80211_CWM_WIDTH80;
        else if(caps & (NAWDS_REPEATER_CAP_11AXAHE40 |
                        NAWDS_REPEATER_CAP_11AXGHE40))
            ni->ni_chwidth = IEEE80211_CWM_WIDTH40;
        else if(caps & (NAWDS_REPEATER_CAP_11AXAHE20 |
                        NAWDS_REPEATER_CAP_11AXGHE20))
            ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
    }

    ieee80211node_set_extflag(ni, IEEE80211_NODE_HE);

    /* Set mpdu density to zero for HE capable APs */
    ni->ni_mpdudensity = ieee80211_parse_mpdudensity(IEEE80211_HTCAP_MPDUDENSITY_NA);

    /* Streams decision based on TS/DS or higher flags */
    if (caps & NAWDS_REPEATER_CAP_8S) {
        ni->ni_streams = 8;
    }
    else if (caps & NAWDS_REPEATER_CAP_7S) {
        ni->ni_streams = 7;
    }
    else if (caps & NAWDS_REPEATER_CAP_6S) {
        ni->ni_streams = 6;
    }
    else if (caps & NAWDS_REPEATER_CAP_5S) {
        ni->ni_streams = 5;
    }
    else if (caps & NAWDS_REPEATER_CAP_4S) {
        ni->ni_streams = 4;
    }
    else if (caps & NAWDS_REPEATER_CAP_TS) {
        ni->ni_streams = 3;
    }
    else if (caps & NAWDS_REPEATER_CAP_DS) {
        ni->ni_streams = 2;
    } else {
        ni->ni_streams = 1;
    }
    if (ni->ni_streams > vdev_mlme->proto.generic.nss) {
        ni->ni_streams = vdev_mlme->proto.generic.nss;
    }

    return;
}

#if (MESH_MODE_SUPPORT||ATH_SUPPORT_NAC)
static struct
ieee80211_node *wlan_localpeer_config(wlan_if_t vaphandle, char *macaddr, int caps, int *prev_added)
{
    struct ieee80211_node *ni = NULL;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    u_int32_t ht_flags, vht_flags, he_flags;
    uint8_t ht_disable=0;
    bool is_160_or_80p80_supported = false;

    if (ic->ic_modecaps &
                ((1 << IEEE80211_MODE_11AC_VHT160) |
                 (1 << IEEE80211_MODE_11AC_VHT80_80) |
                 (1 << IEEE80211_MODE_11AXA_HE160) |
                 (1ULL << IEEE80211_MODE_11AXA_HE80_80)))
    {
        is_160_or_80p80_supported = true;
    }

    /* if previously configured node found, start the node over */
    ni = ieee80211_find_node(&ic->ic_sta, macaddr);
    if (!ni)
        ni = find_logically_deleted_node_on_soc(
                wlan_vdev_get_psoc(vap->vdev_obj), macaddr, NULL);

    if (ni) {
        *prev_added = 1;
        ieee80211_free_node(ni);
        return NULL;
    }

    ni = ieee80211_dup_bss(vap, macaddr);
    if (ni == NULL)
        return NULL;

    /* another check for NG setup  was earlier 0 */
    /* we do not want very low RSSI as we may start at very poor rates, so setting RSSI to a normal value*/
    ni->ni_rssi = 35;

    /* base configuration */
    ieee80211_localpeer_config_base(ni, caps, ht_disable);

    /* HT capability */
    ht_flags  = (NAWDS_REPEATER_CAP_HT20 | NAWDS_REPEATER_CAP_HT2040);
    vht_flags = (NAWDS_REPEATER_CAP_11ACVHT20 | NAWDS_REPEATER_CAP_11ACVHT40 | NAWDS_REPEATER_CAP_11ACVHT80);
    he_flags = (NAWDS_REPEATER_CAP_11AXAHE20 | NAWDS_REPEATER_CAP_11AXGHE20 | NAWDS_REPEATER_CAP_11AXAHE40 |
                NAWDS_REPEATER_CAP_11AXGHE40 | NAWDS_REPEATER_CAP_11AXAHE80);
    if(is_160_or_80p80_supported) {
        vht_flags |= (NAWDS_REPEATER_CAP_11ACVHT80_80 | NAWDS_REPEATER_CAP_11ACVHT160);
        he_flags |= (NAWDS_REPEATER_CAP_11AXAHE80_80 |  NAWDS_REPEATER_CAP_11AXAHE160);
    }

    if (caps & (ht_flags | vht_flags | he_flags)) {
            ieee80211_nawds_config_ht(ni, caps);
            /* VHT Capability */
            if ((caps & (vht_flags | he_flags)) &&
                    /* Include 2.4Ghz also if 256QAM is enabled */
                    (IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
                     IEEE80211_IS_CHAN_11NG(vap->iv_bsschan) ||
                     IEEE80211_IS_CHAN_HE(vap->iv_bsschan)) &&
                    ieee80211vap_vhtallowed(vap)) {

                ieee80211_nawds_config_vht(ni, caps, FALSE);
                /* HE capability */
                if (ieee80211vap_heallowed(vap) && (caps & he_flags))
                    ieee80211_nawds_config_he(ni, caps);

           /* 11AX TODO (Phase II) - Revisit later */
        }
    } else {
        /* non-ht/vht i.e legacy case */
        ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
        OS_MEMSET(&ni->ni_htrates, 0 , sizeof(struct ieee80211_rateset));
    }

    /* Update the PHY mode of the node */
    if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan)) {
        ni->ni_phymode = IEEE80211_MODE_11A;
    } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11G)) {
        ni->ni_phymode = IEEE80211_MODE_11G;
    } else {
        ni->ni_phymode = IEEE80211_MODE_11B;
    }
    ieee80211_update_ht_vht_he_phymode(ic, ni);

    /* Rates are copied here for HE because it requires
     * ni's phymode which is set above
     */
    if (ieee80211vap_heallowed(vap)) {
        ieee80211_copy_hecap_rates(ic, vap, ni);
    }

    ieee80211_node_join(ni);
#ifdef QCA_SUPPORT_CP_STATS
    vdev_cp_stats_authorize_attempt_inc(vap->vdev_obj, 1);
#endif

    wlan_node_set_peer_state(ni, WLAN_WAITKEY_STATE);
    /* Mark this node entry as local mesh peer */
    ieee80211node_set_extflag(ni,IEEE80211_LOCAL_MESH_PEER);
    if (!(ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)) {
        ieee80211_node_authorize(ni);
    } else {
        printk (" privacy \n");
    }
    if (ic->ic_newassoc != NULL)
        ic->ic_newassoc(ni, 1);

    /* free the extra refcount got from dup_bss() */
    ieee80211_free_node(ni);

    IEEE80211_DPRINTF(vap, IEEE80211_MSG_NAC | IEEE80211_MSG_MESH,"%s: Addr bytes[0][5]:%2x:%2x \n",
                           __func__,macaddr[0],macaddr[5]);
    return ni;
}

int wlan_authorise_local_peer(wlan_if_t vaphandle, char *macaddr)
{
    struct ieee80211_node *ni = NULL;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;

    /* if previously configured node found, start the node over */
    ni = ieee80211_find_node(&ic->ic_sta, macaddr);
    if (ni) {
        ieee80211_free_node(ni);

        if (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY) {
            ieee80211_node_authorize(ni);
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_NAC | IEEE80211_MSG_MESH,"%s: Authorize Addr bytes[0][5]:%2x:%2x \n",
                            __func__,macaddr[0],macaddr[5]);
        }
        return 0;
    } else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_NAC | IEEE80211_MSG_NAC,"%s: Failed Addr bytes[0][5]:%2x:%2x \n",
                            __func__,macaddr[0],macaddr[5]);
        return EINVAL;
    }
    return 0;
}

int
wlan_del_localpeer(wlan_if_t vaphandle, char *macaddr)
{
    struct ieee80211_node* ni;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;

    /* reclaim the node */
    ni = ieee80211_find_node(&ic->ic_sta, macaddr);

    if (ni) {
        if (ni->ni_ext_flags&IEEE80211_LOCAL_MESH_PEER) {
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_NAC | IEEE80211_MSG_MESH,"%s: delete mesh peer mac bytes[0][5]:%2x:%2x \n",
                               __func__,macaddr[0],macaddr[5]);
            IEEE80211_NODE_LEAVE(ni);
        }
        ieee80211_free_node(ni);
        return 0;
    } else {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_NAC,"%s: Failed Addr bytes[0][5]:%2x:%2x \n",
                            __func__,macaddr[0],macaddr[5]);
        return -1;
    }
    return 0;
}
#endif

static struct
ieee80211_node *wlan_nawds_config_repeater(wlan_if_t vaphandle, char *macaddr, int caps)
{
    struct ieee80211_node *ni = NULL;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_node       *ni_wds = NULL;
    struct ieee80211_node_table  *nt = &ic->ic_sta;
    u_int32_t ht_flags, vht_flags, he_flags;
    uint8_t ht_disable=0;
    bool is_160_or_80p80_supported = false;

    if (ic->ic_modecaps &
                ((1 << IEEE80211_MODE_11AC_VHT160) |
                 (1 << IEEE80211_MODE_11AC_VHT80_80) |
                 (1 << IEEE80211_MODE_11AXA_HE160) |
                 (1ULL << IEEE80211_MODE_11AXA_HE80_80)))
    {
        is_160_or_80p80_supported = true;
    }

    /* if previously configured node found, start the node over */
    ni = ieee80211_find_node(&ic->ic_sta, macaddr);
    if (ni) {
        ieee80211_free_node(ni);
        goto reconfigure;
    }

    ni = ieee80211_dup_bss(vap, macaddr);
    if (ni == NULL)
        return NULL;

    /* free the extra refcount got from dup_bss() */
    ieee80211_free_node(ni);

reconfigure:
    /* another check for NG setup  was earlier 0 */
    /* we do not want very low RSSI as we may start at very poor rates, so setting RSSI to a normal value*/
    ni->ni_rssi = 35;

    /*
     * Set the default nss to 1,
     * the config ht/vht funcitons will take care of the value configured in caps.
     */
    ni->ni_streams = 1;

    if((IEEE80211_VAP_IS_PRIVACY_ENABLED(vap) || (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)) &&
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
            (RSN_CIPHER_IS_WEP(&vap->iv_rsn))){
#else
            wlan_crypto_vdev_has_ucastcipher(vap->vdev_obj, (1 << WLAN_CRYPTO_CIPHER_WEP))) {
#endif
       ht_disable = 1;
    }
    /* base configuration */
    ieee80211_nawds_config_base(ni, caps, ht_disable);

    /* HT capability */
    ht_flags  = (NAWDS_REPEATER_CAP_HT20 | NAWDS_REPEATER_CAP_HT2040);
    vht_flags = (NAWDS_REPEATER_CAP_11ACVHT20 | NAWDS_REPEATER_CAP_11ACVHT40 | NAWDS_REPEATER_CAP_11ACVHT80);
    he_flags = (NAWDS_REPEATER_CAP_11AXAHE20 | NAWDS_REPEATER_CAP_11AXGHE20 | NAWDS_REPEATER_CAP_11AXAHE40 |
                NAWDS_REPEATER_CAP_11AXGHE40 | NAWDS_REPEATER_CAP_11AXAHE80);
    if(is_160_or_80p80_supported) {
        vht_flags |= (NAWDS_REPEATER_CAP_11ACVHT80_80 | NAWDS_REPEATER_CAP_11ACVHT160);
        he_flags |= (NAWDS_REPEATER_CAP_11AXAHE80_80 |  NAWDS_REPEATER_CAP_11AXAHE160);
    }

    if (caps & (ht_flags | vht_flags | he_flags) || ht_disable) {
        if((IEEE80211_VAP_IS_PRIVACY_ENABLED(vap) || (ni->ni_capinfo & IEEE80211_CAPINFO_PRIVACY)) &&
#ifndef WLAN_CONV_CRYPTO_SUPPORTED
                (RSN_CIPHER_IS_WEP(&vap->iv_rsn))){
#else
            wlan_crypto_vdev_has_ucastcipher(vap->vdev_obj, (1 << WLAN_CRYPTO_CIPHER_WEP))) {
#endif
            IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS, "%s: WEP Mode, so not configuring HT Cap\n", __FUNCTION__);
            ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
            ni->ni_streams = 1;
            OS_MEMSET(&ni->ni_htrates, 0 , sizeof(struct ieee80211_rateset));

        }
        else {
            ieee80211_nawds_config_ht(ni, caps);
            /* VHT Capability */
            if ((caps & (vht_flags | he_flags)) &&
                    /* Include 2.4Ghz also if 256QAM is enabled */
                    (IEEE80211_IS_CHAN_11AC(vap->iv_bsschan) ||
                     IEEE80211_IS_CHAN_11NG(vap->iv_bsschan) ||
                     IEEE80211_IS_CHAN_HE(vap->iv_bsschan)) &&
                    ieee80211vap_vhtallowed(vap)) {

                ieee80211_nawds_config_vht(ni, caps, FALSE);
                /* HE capability */
                if (ieee80211vap_heallowed(vap) && (caps & he_flags))
                    ieee80211_nawds_config_he(ni, caps);
            }
        }
    } else {
        /* non-ht/vht i.e legacy case */
        ni->ni_chwidth = IEEE80211_CWM_WIDTH20;
        OS_MEMSET(&ni->ni_htrates, 0 , sizeof(struct ieee80211_rateset));
    }

    /* Update the PHY mode of the node */
    if (IEEE80211_IS_CHAN_5GHZ(ic->ic_curchan)) {
        ni->ni_phymode = IEEE80211_MODE_11A;
    } else if (IEEE80211_SUPPORT_PHY_MODE(ic, IEEE80211_MODE_11G)) {
        ni->ni_phymode = IEEE80211_MODE_11G;
    } else {
        ni->ni_phymode = IEEE80211_MODE_11B;
    }
    ieee80211_update_ht_vht_he_phymode(ic, ni);

    /* Rates are copied here for HE because it requires
     * ni's phymode which is set above
     */
    if (ieee80211vap_heallowed(vap)) {
        ieee80211_copy_hecap_rates(ic, vap, ni);
    }

    /* the node was created as a repeater to avoid kickout */
    ieee80211node_set_flag(ni, IEEE80211_NODE_WDS);
    ieee80211node_set_flag(ni, IEEE80211_NODE_NAWDS);
    wlan_peer_set_nawds(ni->peer_obj);

    ieee80211_node_join(ni);
    if (ic->ic_newassoc != NULL)
        ic->ic_newassoc(ni, 1);
    ieee80211_node_authorize(ni);

    ic->ic_nss_change(ni);
    ic->ic_chwidth_change(ni);

    ni_wds = ieee80211_find_wds_node(nt, macaddr);
    if (ni_wds == NULL) {
        ieee80211_add_wds_addr(vaphandle, nt, ni, macaddr,
            IEEE80211_NODE_F_WDS_REMOTE);
    } else {
        ieee80211_free_node(ni_wds);
    }

    return ni;
}

int wlan_nawds_config_mac(wlan_if_t vaphandle, char *macaddr, u_int32_t caps)
{
    int i, slot_free = -1, max_xretries = 0, slot_max_retries = 0, ret = 0;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_nawds *nawds = &vap->iv_nawds;
    struct ieee80211com *ic = vap->iv_ic;
    nawds_rwlock_state_t(lock_state);

    ret = OS_MEMCMP(vaphandle->iv_myaddr, macaddr, IEEE80211_ADDR_LEN);
    if(!ret)
        return -EINVAL;

    /*do not add the Mac address in table if nawds is disable*/
    if(IEEE80211_NAWDS_DISABLED == nawds->mode){
       IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS,
                "%s: nawds mode off\n", __func__);
       return -EINVAL;
    }

    /* If HE is enabled, enable HT and VHT
       If VHT is enabled, enable HT */
    if(caps & NAWDS_REPEATER_CAP_11ACVHT20){
        nawds->defcaps |= NAWDS_REPEATER_CAP_HT20;
        caps           |= NAWDS_REPEATER_CAP_HT20;
    } else if(caps & NAWDS_REPEATER_CAP_11ACVHT40){
        nawds->defcaps |= NAWDS_REPEATER_CAP_HT2040;
        caps           |= NAWDS_REPEATER_CAP_HT2040;
    } else if(caps & (NAWDS_REPEATER_CAP_11AXAHE20 | NAWDS_REPEATER_CAP_11AXGHE20)) {
        nawds->defcaps |= (NAWDS_REPEATER_CAP_11ACVHT20 | NAWDS_REPEATER_CAP_HT20);
        caps           |= (NAWDS_REPEATER_CAP_11ACVHT20 | NAWDS_REPEATER_CAP_HT20);
    } else if(caps & (NAWDS_REPEATER_CAP_11AXAHE40 |  NAWDS_REPEATER_CAP_11AXGHE40)) {
        nawds->defcaps |= (NAWDS_REPEATER_CAP_11ACVHT40 | NAWDS_REPEATER_CAP_HT2040);
        caps           |= (NAWDS_REPEATER_CAP_11ACVHT40 | NAWDS_REPEATER_CAP_HT2040);
    } else if(caps & (NAWDS_REPEATER_CAP_11AXAHE80 | NAWDS_REPEATER_CAP_11ACVHT80)) {
        nawds->defcaps |= NAWDS_REPEATER_CAP_11ACVHT80;
        caps           |= NAWDS_REPEATER_CAP_11ACVHT80;
    } else if(caps & (NAWDS_REPEATER_CAP_11AXAHE80_80 | NAWDS_REPEATER_CAP_11ACVHT80_80)) {
        nawds->defcaps |= NAWDS_REPEATER_CAP_11ACVHT80_80;
        caps           |= NAWDS_REPEATER_CAP_11ACVHT80_80;
    } else if(caps & (NAWDS_REPEATER_CAP_11AXAHE160 | NAWDS_REPEATER_CAP_11ACVHT160)) {
        nawds->defcaps |= NAWDS_REPEATER_CAP_11ACVHT160;
        caps           |= NAWDS_REPEATER_CAP_11ACVHT160;
    }

    /* sanity check */
    if (!is_nawds_valid_mac(macaddr) ||
        !is_nawds_valid_caps(ic,caps)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS,
                "%s: Invalid capabilities or MAC addr\n", __func__);
        return -EINVAL;
    }

    if (is_nawds_mac_bss_mac(ic, macaddr)) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_WDS,
                "%s: Error: Attempt to add BSS MAC as repeater\n", __func__);
        return -EINVAL;
    }

    NAWDS_WRITE_LOCK(&nawds->lock, &lock_state);

    /* try to find repeater with the mac and update the caps */
    for(i = 0; i < UMAC_MAX_NAWDS_REPEATER; i++) {
        if (IEEE80211_ADDR_EQ(nawds->repeater[i].mac, macaddr)) {
            nawds->repeater[i].caps = caps;
            wlan_nawds_config_repeater(vaphandle, macaddr, caps);
            OS_RWLOCK_WRITE_UNLOCK(&nawds->lock, &lock_state);
            return 0;
        }
        if (!is_nawds_valid_mac(nawds->repeater[i].mac)) {
            if (slot_free == -1)
                slot_free = i;
        } else if (nawds->override) {
            /* the entry has a valid mac */
            struct ieee80211_node *ni = NULL;
            ni = ieee80211_find_node(&ic->ic_sta, nawds->repeater[i].mac);
            if (!ni)
                continue;
            ieee80211_free_node(ni);
            if (ni->ni_consecutive_xretries >= max_xretries) {
                max_xretries = ni->ni_consecutive_xretries;
                slot_max_retries = i;
            }
        }
    }

    /* can't find the repeater for the given mac address */
    if (slot_free == -1) {
        if (!nawds->override) {
            NAWDS_WRITE_UNLOCK(&nawds->lock, &lock_state);
            return -ENOSPC;
        } else {
            ieee80211_nawds_node_leave(vaphandle,
                    nawds->repeater[slot_max_retries].mac);
            slot_free = slot_max_retries;
        }
    }

    /* clear the NAWDS repeater with the largest tx errors */

    /* configure the NAWDS repeater node */
    IEEE80211_ADDR_COPY(nawds->repeater[slot_free].mac, macaddr);
    nawds->repeater[slot_free].caps = caps;
    wlan_nawds_config_repeater(vaphandle, macaddr, caps);

    NAWDS_WRITE_UNLOCK(&nawds->lock, &lock_state);

    return 0;
}

int wlan_nawds_config_key(wlan_if_t vaphandle, char *macaddr, char *psk)
{
    int ret = -1;
    struct crypto_hash *tfm;
    struct hash_desc desc;
    struct scatterlist sg;
    int out_size;
    void *out;
    struct wlan_crypto_req_key req_key;
    int i = 0;
    char data[32];

    if (!is_nawds_valid_mac(macaddr)) {
        return -EINVAL;
    }

    qdf_mem_zero(&req_key, sizeof(struct wlan_crypto_req_key));
    qdf_mem_copy(req_key.macaddr, macaddr, IEEE80211_ADDR_LEN);

    for(i=0; i<6; i++)
        macaddr[i] ^= vaphandle->iv_myaddr[i];

    memset(data, '\0', sizeof(data));
    qdf_mem_copy(data, psk, strlen(psk));
    for (i = 0; i < strlen(psk) + 1; i++) {
        data[i] ^= macaddr[i%IEEE80211_ADDR_LEN];
        data[i+1] ^= macaddr[(i+1)%IEEE80211_ADDR_LEN];
        i++;
    }


    tfm = crypto_alloc_hash("sha256", 0, CRYPTO_ALG_TYPE_SHASH);
    if (IS_ERR(tfm)) {
        qdf_err("Failed to allocate transformation for %s: %ld",
                      "sha256", PTR_ERR(tfm));
        return -EINVAL;
    }
    out_size = crypto_hash_digestsize(tfm);

    out = kmalloc(out_size, GFP_KERNEL);
    if (!out)
        goto free_tfm;
    memset(out, 0, out_size);

    desc.tfm   = tfm;
    desc.flags = CRYPTO_ALG_TYPE_SHASH;

    ret = crypto_hash_init(&desc);
    if (ret < 0) {
        qdf_info("%s: could not init hash", __func__);
        goto free_out;
    }

    sg_init_one(&sg, data, strlen(psk));
    ret = crypto_hash_update(&desc, &sg, strlen(psk));
    if (ret < 0) {
        qdf_info("%s: could not update hash", __func__);
        goto free_out;
    }
    ret = crypto_hash_final(&desc, out);
    if (ret < 0) {
        qdf_info("%s: could not finalize hash", __func__);
        goto free_out;
    }

    if (((char *)out)[0] == 0)
        ((char *)out)[0] = 1;

    req_key.type   = WLAN_CRYPTO_CIPHER_AES_CCM;
    req_key.keylen = strlen(psk);
    req_key.keyix  = 0;

    qdf_mem_copy(req_key.keydata, out, strlen(psk));
    wlan_crypto_setkey(vaphandle->vdev_obj, &req_key);

free_out:
    kfree(out);
free_tfm:
    crypto_free_hash(tfm);
    return ret;
}

#if (MESH_MODE_SUPPORT||ATH_SUPPORT_NAC)

#define MESH_CAPS_VER1 0x8000
#define MESH_CAPS_BW_OFFSET 0
#define MESH_CAPS_NSS_OFFSET 4
#define MESH_CAPS_MODE_OFFSET 8
#define MESH_CAPS_NIBBLE_MASK 0xF
#define MESH_CAPS_MAX_NSS 8
typedef enum {
    MESH_PREAMBLE_OFDM,
    MESH_PREAMBLE_CCK,
    MESH_PREAMBLE_HT ,
    MESH_PREAMBLE_VHT,
    MESH_PREAMBLE_HE,
    MESH_PREAMBLE_MAX,
} localpeer_preamble_type_t;

typedef enum {
    MESH_BW_20,
    MESH_BW_40,
    MESH_BW_80,
    MESH_BW_80_80,
    MESH_BW_160,
    MESH_BW_MAX,
} localpeer_bw_type_t;

int wlan_add_localpeer(wlan_if_t vaphandle, char *macaddr, u_int32_t caps)
{
    int prev_added = 0;
    uint8_t bw;
    uint8_t nss;
    uint8_t mode;

    if (caps & MESH_CAPS_VER1) {
        bw   = (caps >> MESH_CAPS_BW_OFFSET) & MESH_CAPS_NIBBLE_MASK;
        nss  = (caps >> MESH_CAPS_NSS_OFFSET) & MESH_CAPS_NIBBLE_MASK;
        mode = (caps >> MESH_CAPS_MODE_OFFSET) & MESH_CAPS_NIBBLE_MASK;

        if ((nss >= MESH_CAPS_MAX_NSS) || (mode >= MESH_PREAMBLE_MAX)
                                            || (bw >= MESH_PREAMBLE_MAX)) {
            qdf_print("%s: invalid nss %d bw %d mode %d !!!", __func__, nss, bw, mode);
            return QDF_STATUS_E_INVAL;
        }

        caps = nss ? (1 << (nss - 1)) : 0;

        if (mode == MESH_PREAMBLE_HT) {
            if (bw == MESH_BW_40) {
                caps |= NAWDS_REPEATER_CAP_HT2040;
            } else {
                caps |= NAWDS_REPEATER_CAP_HT20;
            }
        } else if (mode == MESH_PREAMBLE_VHT) {
            switch (bw) {
                case MESH_BW_160:   caps |= NAWDS_REPEATER_CAP_11ACVHT160; break;
                case MESH_BW_80_80: caps |= NAWDS_REPEATER_CAP_11ACVHT80_80; break;
                case MESH_BW_80:    caps |= NAWDS_REPEATER_CAP_11ACVHT80; break;
                case MESH_BW_40:    caps |= NAWDS_REPEATER_CAP_11ACVHT40; break;
                default:            caps |= NAWDS_REPEATER_CAP_11ACVHT20; break;
            }
        } else if (mode == MESH_PREAMBLE_HE) {
            switch (bw) {
                case MESH_BW_160:   caps |= NAWDS_REPEATER_CAP_11AXAHE160; break;
                case MESH_BW_80_80: caps |= NAWDS_REPEATER_CAP_11AXAHE80_80; break;
                case MESH_BW_80:    caps |= NAWDS_REPEATER_CAP_11AXAHE80; break;
                case MESH_BW_40:    caps |= NAWDS_REPEATER_CAP_11AXAHE40; break;
                default:            caps |= NAWDS_REPEATER_CAP_11AXAHE20; break;
            }
        }
        qdf_print("%s: derived ver1 caps %x", __func__, caps);
    }

    if (wlan_localpeer_config(vaphandle, macaddr, caps, &prev_added) == NULL) {
        if (prev_added) {
            return EINVAL;
        } else {
            return ENOMEM;
        }
    }

    return 0;
}
#endif

int wlan_nawds_delete_mac(wlan_if_t vaphandle, char *macaddr)
{
    int i;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_nawds *nawds = &vap->iv_nawds;
    nawds_rwlock_state_t(lock_state);

    NAWDS_WRITE_LOCK(&nawds->lock, &lock_state);

    /* try to find repeater with the mac and update the caps */
    for(i = 0; i < UMAC_MAX_NAWDS_REPEATER; i++) {
        if (IEEE80211_ADDR_EQ(nawds->repeater[i].mac, macaddr)) {
            ieee80211_nawds_node_leave(vaphandle, macaddr);
            NAWDS_WRITE_UNLOCK(&nawds->lock, &lock_state);
            return 0;
        }
    }

    /* can't find the repeater for the given mac address */
    NAWDS_WRITE_UNLOCK(&nawds->lock, &lock_state);
    return -ENXIO;
}

int wlan_nawds_get_mac(wlan_if_t vaphandle, int num, char *macaddr, int *caps)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_nawds *nawds = &vap->iv_nawds;
    nawds_rwlock_state_t(lock_state);

    /* sanity check */
    if (num < 0 || num >= UMAC_MAX_NAWDS_REPEATER) {
        return -EINVAL;
    }

    NAWDS_WRITE_LOCK(&nawds->lock, &lock_state);

    OS_MEMCPY(macaddr, nawds->repeater[num].mac, IEEE80211_ADDR_LEN);
    *caps = nawds->repeater[num].caps;

    NAWDS_WRITE_UNLOCK(&nawds->lock, &lock_state);

    return 0;
}

/* Send beacon control command to enable or disable beacon TX in beacon offload enabled mode */
static void wlan_nawds_beacon_control(wlan_if_t vaphandle, enum ieee80211_nawds_mode mode)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    int ret = 0;

    switch(mode)
    {
        case IEEE80211_NAWDS_DISABLED:
        case IEEE80211_NAWDS_STATIC_REPEATER:
        case IEEE80211_NAWDS_LEARNING_REPEATER:
            /* Enable beaconing */
            if (ic->ic_beacon_offload_control) {
                ret = ic->ic_beacon_offload_control(vap, IEEE80211_BCN_OFFLD_TX_ENABLE);
            }
            break;
        case IEEE80211_NAWDS_STATIC_BRIDGE:
        case IEEE80211_NAWDS_LEARNING_BRIDGE:
            /*Disable beaconing */
            if (ic->ic_beacon_offload_control) {
                ret = ic->ic_beacon_offload_control(vap, IEEE80211_BCN_OFFLD_TX_DISABLE);
            }
            break;
    }
    if (ret) {
        qdf_print("Failed to send beacon offload control message");
    }
}

int wlan_nawds_set_param(wlan_if_t vaphandle, enum ieee80211_nawds_param param, void *val)
{
    int ret = 0;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_nawds *nawds = &vap->iv_nawds;
    struct ieee80211com *ic = vap->iv_ic;
    nawds_rwlock_state_t(lock_state);
    int mode, defcaps, override;

    NAWDS_WRITE_LOCK(&nawds->lock, &lock_state);

    switch(param) {
        case IEEE80211_NAWDS_PARAM_MODE:
            mode = *((u_int8_t *) val);
            /* sanity check */
            if (mode < IEEE80211_NAWDS_DISABLED ||
                mode > IEEE80211_NAWDS_LEARNING_BRIDGE) {
                ret = -EINVAL;
                goto out;
            }
            if (mode == nawds->mode)
                goto out;
            /* clear all nawds repeaters */
            ieee80211_nawds_node_leave_all(vap);
            /* change mode */
            nawds->mode = mode;
            wlan_nawds_beacon_control(vap, nawds->mode);
            break;
        case IEEE80211_NAWDS_PARAM_DEFCAPS:
            defcaps = *((u_int32_t *) val);
            if (!is_nawds_valid_caps(ic,defcaps)) {
                ret = -EINVAL;
                goto out;
            }
            nawds->defcaps = defcaps;
            break;
        case IEEE80211_NAWDS_PARAM_OVERRIDE:
            override = *((u_int8_t *) val);
            nawds->override = override;
            break;
        default:
            ret = -EINVAL;
            goto out;
    }

out:
    NAWDS_WRITE_UNLOCK(&nawds->lock, &lock_state);

    return ret;
}

int wlan_nawds_get_param(wlan_if_t vaphandle, enum ieee80211_nawds_param param, void *val)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_nawds *nawds = &vap->iv_nawds;
    nawds_rwlock_state_t(lock_state);

    NAWDS_WRITE_LOCK(&nawds->lock, &lock_state);

    switch(param) {
        case IEEE80211_NAWDS_PARAM_MODE:
            *((u_int8_t *) val) = nawds->mode;
            break;
        case IEEE80211_NAWDS_PARAM_DEFCAPS:
            *((u_int32_t *) val) = nawds->defcaps;
            break;
        case IEEE80211_NAWDS_PARAM_OVERRIDE:
            *((u_int8_t *) val) = nawds->override;
            break;
        case IEEE80211_NAWDS_PARAM_NUM:
            *((u_int8_t *) val) = UMAC_MAX_NAWDS_REPEATER;
            break;
        default:
            NAWDS_WRITE_UNLOCK(&nawds->lock, &lock_state);
            return -EINVAL;
    }

    NAWDS_WRITE_UNLOCK(&nawds->lock, &lock_state);

    return 0;
}

#endif /* UMAC_SUPPORT_NAWDS */

int wlan_wds_add_entry(wlan_if_t vaphandle, char *destmac, char *peermac)
{
    struct ieee80211com *ic;
    if(vaphandle) {
	ic = vaphandle->iv_ic;
        ic->ic_node_add_wds_entry((void *)(vaphandle->iv_ifp), destmac, peermac, 0);
        return 0;
    } else {
        return -1;
    }
}

int wlan_wds_del_entry(wlan_if_t vaphandle, char *destmac)
{

    struct ieee80211com *ic;
    if(vaphandle) {
	ic = vaphandle->iv_ic;
        ic->ic_node_del_wds_entry((void *)(vaphandle->iv_ifp), destmac);
        return 0;
    } else {
        return -1;
    }
}

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
static inline int _wlan_hmwds_add_addr(wlan_if_t vaphandle, struct ieee80211_node_table *nt, u_int8_t *wds_ni_macaddr, u_int8_t *wds_macaddr)
{
    int retval = 0;
    struct ieee80211_node *wds_ni, *is_direct_ni;
    struct ieee80211_wds_addr *wds = NULL;

	is_direct_ni = ieee80211_find_node(nt, wds_macaddr);
	if(is_direct_ni)
	{
		printk("Should not add a directly associated node as a wds  node \n");
        ieee80211_free_node(is_direct_ni);
		return retval;
	}

    if ((wds_ni = __ieee80211_find_wds_node(nt, wds_macaddr, &wds)) &&
            IEEE80211_ADDR_EQ(wds_ni->ni_macaddr, wds_ni_macaddr)) {
        wds->flags |= IEEE80211_NODE_F_WDS_HM;
        nt->nt_ic->ic_node_add_wds_entry((void *)(vaphandle->iv_ifp), wds_macaddr, wds_ni_macaddr, IEEE80211_NODE_F_WDS_HM);
        ieee80211_free_node(wds_ni);
    } else {
        if (wds) {
            wds->flags &= ~IEEE80211_NODE_F_WDS_HM;
            if (wds_ni) {
                /* Remove the WDS addres if it was found behind some other node
                 * to avoid duplicate entries.
                 */
                ieee80211_remove_wds_addr(vaphandle, nt, wds->wds_macaddr, IEEE80211_NODE_F_WDS_REMOTE);
                wds = NULL;
                /* Free the reference from earlier find above
                 */
                ieee80211_free_node(wds_ni);
            }
        }

        if (!(wds_ni = ieee80211_find_node(nt, wds_ni_macaddr)))
            return -EINVAL;

        retval = _ieee80211_add_wds_addr(vaphandle, wds_ni, wds_macaddr,
                IEEE80211_NODE_F_WDS_REMOTE | IEEE80211_NODE_F_WDS_HM);
        ieee80211_free_node(wds_ni);
    }

    return retval;
}

int wlan_hmwds_add_addr(wlan_if_t vaphandle, u_int8_t *wds_ni_macaddr, u_int8_t *wds_macaddr, u_int16_t wds_macaddr_cnt)
{
    int i, retval;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_node_table *nt = NULL;
    struct ieee80211_node *ni;

    if (!vap || !wds_ni_macaddr || !wds_macaddr || !wds_macaddr_cnt)
        return -EINVAL;

    nt = &vap->iv_ic->ic_sta;

    /* If more than one address is being added, the new set of addresses
     * should replace the existing hmwds entries - reset all wds entries
     * and then add new entries
     */
    if (wds_macaddr_cnt > 1) {
        wlan_hmwds_reset_addr(vaphandle, wds_ni_macaddr);
    }

    ni = ieee80211_find_node(nt, wds_ni_macaddr);
    if (ni) {
        retval = nt->nt_ic->ic_node_use_4addr(ni);
        IEEE80211_DPRINTF(ni->ni_vap, IEEE80211_MSG_WDS, "%s: use_4addr peer %02x:%02x:%02x"
                          ":%02x:%02x:%02x, returns %d\n", __func__, ni->ni_macaddr[0], ni->ni_macaddr[1],
                          ni->ni_macaddr[2], ni->ni_macaddr[3], ni->ni_macaddr[4], ni->ni_macaddr[5], retval);
        ieee80211_free_node(ni);
    }

    retval = 0;
    for (i = 0; i < wds_macaddr_cnt; i++) {
        ieee80211_remove_wds_addr(vaphandle, nt, wds_macaddr + i * IEEE80211_ADDR_LEN,
                                  IEEE80211_NODE_F_WDS_REMOTE | IEEE80211_NODE_F_WDS_BEHIND | IEEE80211_NODE_F_WDS_HM);
        retval |= _wlan_hmwds_add_addr(vap, nt, wds_ni_macaddr, wds_macaddr + i * IEEE80211_ADDR_LEN);
    }

    return retval;
}

int wlan_hmwds_remove_addr(wlan_if_t vaphandle, u_int8_t *macaddr)
{
    struct ieee80211vap *vap = vaphandle;
    rwlock_state_t lock_state;
    struct ieee80211_node_table *nt = NULL;

    if (!vap || !macaddr)
        return -EINVAL;
    nt = &vap->iv_ic->ic_sta;

    ieee80211_remove_wds_addr(vaphandle, nt, macaddr, IEEE80211_NODE_F_WDS_REMOTE | IEEE80211_NODE_F_WDS_BEHIND | IEEE80211_NODE_F_WDS_HM);

    return 0;
}

int wlan_hmwds_reset_addr(wlan_if_t vaphandle, u_int8_t *macaddr)
{
    unsigned int hash;
    u_int8_t stag = 0;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_node *wds_ni;
    struct ieee80211_wds_addr *wds = NULL;
    rwlock_state_t lock_state;
    struct ieee80211_node_table *nt = NULL;
    int retval = 0;
    struct ieee80211_wds_addr *wds_next = NULL;

    if (!vap || !macaddr)
        return -EINVAL;

    nt = &vap->iv_ic->ic_sta;

    if ((wds_ni = ieee80211_find_node(nt, macaddr))) {
        OS_RWLOCK_WRITE_LOCK(&nt->nt_wds_nodelock, &lock_state);
        for (hash = 0; hash < IEEE80211_NODE_HASHSIZE; hash++) {
            for (wds = LIST_FIRST(&nt->nt_wds_hash[hash]); wds;) {
                wds_next = LIST_NEXT(wds, wds_hash);
                if (wds->flags & IEEE80211_NODE_F_WDS_HM &&
                        wds->wds_ni == wds_ni) {
                    ieee80211_free_node(wds->wds_ni);  /* Decrement ref count */
                    LIST_REMOVE(wds, wds_hash);
                    IEEE80211_DPRINTF_IC_CATEGORY(vap->iv_ic, IEEE80211_MSG_WDS,  "%s: deleting mac(%s)"
                            " from host wds table wds_flags:0x%x \n", __func__,
                            ether_sprintf(wds->wds_macaddr), wds->flags);
                    OS_FREE(wds);
                }
                wds = wds_next;
            }
	    }
        wds = NULL;
        OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);
        retval = vap->iv_ic->ic_node_update_wds_entry((void *)(vap->iv_ifp), NULL, wds_ni->ni_macaddr, 0);
    } else {
        wds_ni = _ieee80211_find_wds_node(nt, macaddr, &wds, &stag);
    }

    if (wds_ni) {
        ieee80211_free_node(wds_ni);
    }

    if (wds && wds->flags & IEEE80211_NODE_F_WDS_HM) {
        ieee80211_free_node(wds->wds_ni);  /* Decrement ref count */
        LIST_REMOVE(wds, wds_hash);
        IEEE80211_DPRINTF_IC_CATEGORY(vap->iv_ic, IEEE80211_MSG_WDS,  "%s: deleting mac(%s)"
                " from host wds table wds_flags:0x%x \n", __func__,
                ether_sprintf(wds->wds_macaddr), wds->flags);
        OS_FREE(wds);
        retval = vap->iv_ic->ic_node_update_wds_entry((void *)(vap->iv_ifp), macaddr, NULL, 0);
    }

    return retval;
}

int wlan_hmwds_reset_table(wlan_if_t vaphandle)
{
    unsigned int hash;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_node_table *nt = NULL;
    struct ieee80211_wds_addr *wds;
    rwlock_state_t lock_state;
    struct ieee80211_wds_addr *wds_next;

    if (!vap)
        return -EINVAL;

    nt = &vap->iv_ic->ic_sta;

    OS_RWLOCK_WRITE_LOCK(&nt->nt_wds_nodelock, &lock_state);
    for (hash = 0; hash < IEEE80211_NODE_HASHSIZE; hash++) {
        for (wds = LIST_FIRST(&nt->nt_wds_hash[hash]); wds;) {
            wds_next = LIST_NEXT(wds, wds_hash);
            if (wds->flags & IEEE80211_NODE_F_WDS_HM) {
                ieee80211_free_node(wds->wds_ni);  /* Decrement ref count */
                LIST_REMOVE(wds, wds_hash);
                IEEE80211_DPRINTF_IC_CATEGORY(vap->iv_ic, IEEE80211_MSG_WDS,  "%s: deleting mac(%s)"
                        " from host wds table wds_flags:0x%x \n", __func__,
                        ether_sprintf(wds->wds_macaddr), wds->flags);
                OS_FREE(wds);
            }
            wds = wds_next;
        }
	}
    OS_RWLOCK_WRITE_UNLOCK(&nt->nt_wds_nodelock, &lock_state);

    vap->iv_ic->ic_node_update_wds_entry((void *)(vap->iv_ifp), NULL, NULL, 0);

    return 0;
}

int wlan_wds_dump_wds_addr(wlan_if_t vaphandle)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = NULL;

    if (!vap)
        return -EINVAL;

    ic = vap->iv_ic;
    if(ic->ic_node_dump_wds_table)
    {
        ic->ic_node_dump_wds_table(ic);
    }
    return 0;
}

int wlan_hmwds_read_addr(wlan_if_t vaphandle, u_int8_t *wds_ni_macaddr, u_int8_t *buf, u_int16_t *buflen)
{
    unsigned int hash;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_node_table *nt = NULL;
    struct ieee80211_wds_addr *wds;
    struct ieee80211_node *wds_ni;
    rwlock_state_t lock_state;
    u_int32_t buf_max_len, entry_count = 0;

    if (!vap || !wds_ni_macaddr || !buf || !buflen || !(*buflen))
        return -EINVAL;

    nt = &vap->iv_ic->ic_sta;

    if (!(wds_ni = ieee80211_find_node(nt, wds_ni_macaddr)))
        return -EINVAL;

    buf_max_len = *buflen;
    *buflen = 0;

    OS_RWLOCK_READ_LOCK(&nt->nt_wds_nodelock, &lock_state);
    for (hash = 0; hash < IEEE80211_NODE_HASHSIZE; hash++) {
        for (wds = LIST_FIRST(&nt->nt_wds_hash[hash]);
                wds && (*buflen + IEEE80211_ADDR_LEN < buf_max_len);) {
            if (wds->flags & IEEE80211_NODE_F_WDS_HM &&
                        wds->wds_ni == wds_ni) {
                OS_MEMCPY(buf + *buflen, wds->wds_macaddr, IEEE80211_ADDR_LEN);
                *buflen += IEEE80211_ADDR_LEN;
                entry_count++;
            }
            wds = LIST_NEXT(wds, wds_hash);
        }
        if (*buflen + IEEE80211_ADDR_LEN >= buf_max_len)
            break;
	}
    OS_RWLOCK_READ_UNLOCK(&nt->nt_wds_nodelock, &lock_state);

    ieee80211_free_node(wds_ni);
    *buflen = entry_count;

    return 0;
}

int wlan_wds_read_table(wlan_if_t vaphandle, struct ieee80211_wlanconfig_wds_table *wds_table)
{
    unsigned int hash;
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211_node_table *nt;
    struct ieee80211_wds_addr *wds;
    rwlock_state_t lock_state;
    u_int32_t buf_max_len, buflen = 0;

    if (!vap || !wds_table || !wds_table->wds_entry_cnt)
        return -EINVAL;

    nt = &vap->iv_ic->ic_sta;
    buf_max_len = wds_table->wds_entry_cnt;
    wds_table->wds_entry_cnt = 0;

    OS_RWLOCK_READ_LOCK(&nt->nt_wds_nodelock, &lock_state);
    for (hash = 0; hash < IEEE80211_NODE_HASHSIZE; hash++) {
        for (wds = LIST_FIRST(&nt->nt_wds_hash[hash]);
                wds && (buflen + sizeof(struct ieee80211_wlanconfig_wds) < buf_max_len);) {
            struct ieee80211_wlanconfig_wds *wlanconfig_wds =
                &wds_table->wds_entries[wds_table->wds_entry_cnt];
            OS_MEMCPY(&wlanconfig_wds->destmac, wds->wds_macaddr, IEEE80211_ADDR_LEN);
            if (wds->wds_ni) {
                OS_MEMCPY(&wlanconfig_wds->peermac, wds->wds_ni->ni_macaddr,
                          IEEE80211_ADDR_LEN);
            } else {
                OS_MEMCPY(&wlanconfig_wds->peermac, wds->wds_ni_macaddr,
                          IEEE80211_ADDR_LEN);
            }
            wlanconfig_wds->flags = wds->flags;

            buflen += sizeof(struct ieee80211_wlanconfig_wds);
            wds_table->wds_entry_cnt++;

            wds = LIST_NEXT(wds, wds_hash);
        }
        if (buflen + sizeof(struct ieee80211_wlanconfig_wds) >= buf_max_len)
            break;
	}
    OS_RWLOCK_READ_UNLOCK(&nt->nt_wds_nodelock, &lock_state);

    return 0;
}

int wlan_hmwds_set_bridge_mac_addr(wlan_if_t vaphandle, u_int8_t *bridgemacaddr)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = NULL;

    if (!vap || !bridgemacaddr)
        return -EINVAL;

    ic = vap->iv_ic;
    if(ic->ic_node_set_bridge_mac_addr)
    {
        ic->ic_node_set_bridge_mac_addr(ic, bridgemacaddr);
    }

    return 0;
}

#endif /* ATH_SUPPORT_HYFI_ENHANCEMENTS */

