/*
 *
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2009 Atheros Communications Inc.
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
 *
 * \file ieee80211_me.c
 * \brief Atheros multicast enhancement algorithm, as part of
 * Atheros iQUE feature set.
 *
 * This file contains the main implementation of the Atheros multicast
 * enhancement functionality.
 *
 * The main purpose of this module is to convert (by translating or
 * tunneling) the multicast stream into duplicated unicast streams for
 * performance enhancement of home wireless applications. For more
 * details, please refer to the design documentation.
 *
 */

#include <ieee80211_ique.h>
#include <linux/inet.h>

#if ATH_PERF_PWR_OFFLOAD
#include "ol_if_athvar.h"
#endif

#if ATH_SUPPORT_IQUE
#include "osdep.h"
#include "ieee80211_me_priv.h"
#include "if_athproto.h"
#include <if_athvar.h>

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <nss_api_if.h>
#include <osif_private.h>
#include <osif_nss_wifiol_vdev_if.h>
#endif
#include <wlan_mlme_dp_dispatcher.h>
#include <wlan_utility.h>


#define IGMP_QUERY      0x11
#define IGMPv1_REPORT   0x12
#define IGMPv2_REPORT   0x16
#define IGMP_LEAVE      0x17
#define IGMPv3_REPORT   0x22
#define MLD_QUERY       0x82
#define MLD_REPORT      0x83
#define MLD_LEAVE       0x84
#define MLDv2_REPORT    0x8f

#define MAX_BLOCKED_MCAST_VOW_DESC_PER_NODE 300
#define IEEE80211_ADD_HMMC_CHECK_IP(ip) (((ip) & htobe32(0xf0000000)) != htobe32(0xe0000000))

static int ieee80211_me_SnoopInspecting(struct ieee80211vap *vap, struct ieee80211_node *ni, wbuf_t wbuf);
static int ieee80211_me_igmp_mld_inspect(struct ieee80211vap *vap, wbuf_t wbuf,
        u_int8_t *type, void *group, u_int8_t *hmmc_found);
static void ieee80211_me_detach(struct ieee80211vap *vap);
#if ATH_SUPPORT_ME_SNOOP_TABLE
static void ieee80211_me_remove_expired_group(struct ieee80211vap *vap);
static void ieee80211_me_remove_empty_group(struct ieee80211vap *vap);
static void ieee80211_remove_node_grp(struct MC_GRP_MEMBER_LIST *grp_member_list, struct ieee80211vap *vap);
static int ieee80211_me_ipv6_SnooplsDenied(struct ieee80211vap *vap, u_int8_t *ether_dhost,
                                           u_int8_t *ipv6);
static void ol_ieee80211_remove_node_grp(struct MC_GRP_MEMBER_LIST *grp_member_list,
                                      struct ieee80211vap *vap);
static struct MC_GROUP_LIST_ENTRY* ieee80211_me_create_grp_list(struct ieee80211vap* vap,
                                                                u_int8_t* grp_addr, u_int8_t *grp_ipaddr,
                                                                u_int32_t type, int cmd);
static void ieee80211_me_add_member_list(struct MC_GROUP_LIST_ENTRY* grp_list,
                                         struct MC_LIST_UPDATE* list_entry, u_int32_t ether_type);
static struct MC_GRP_MEMBER_LIST* ieee80211_me_find_member_src(struct MC_GROUP_LIST_ENTRY* grp_list,
                                                               u_int8_t* grp_member_addr,
                                                               u_int32_t src_ip_addr);
static struct MC_GRP_MEMBER_LIST* ieee80211_me_find_member(struct MC_GROUP_LIST_ENTRY* grp_list,
                                                           u_int8_t* grp_member_addr);
static void ieee80211_me_remove_all_member_grp(struct MC_GROUP_LIST_ENTRY* grp_list,
                                               u_int8_t* grp_member_addr);
static struct MC_GROUP_LIST_ENTRY*
ieee80211_me_find_group_list(struct ieee80211vap* vap, uint8_t *grp_addr, u_int8_t *grp_ipaddr, u_int32_t ether_type);
static void ieee80211_me_SnoopListUpdate(struct MC_LIST_UPDATE* list_entry, u_int32_t ether_type);
static int ieee80211_me_SnoopIsDenied(struct ieee80211vap *vap, u_int32_t grpaddr);
static void ieee80211_me_remove_expired_member(struct MC_GROUP_LIST_ENTRY* grp_list,
                                               struct ieee80211vap* vap,
                                               u_int32_t nowtimestamp);
void ieee80211_me_SnoopListUpdate_timestamp(struct MC_LIST_UPDATE* list_entry, u_int32_t ether_type);
static u_int8_t ieee80211_me_count_member_anysrclist(struct MC_GROUP_LIST_ENTRY* grp_list,
                                                     u_int8_t* table,
                                                     int table_len,
                                                     u_int32_t timestamp);
static u_int8_t ieee80211_me_count_member_src_list(struct MC_GROUP_LIST_ENTRY* grp_list,
                                                   u_int32_t src_ip_addr,
                                                   u_int8_t* table, int table_len,
                                                   u_int32_t timestamp);
static uint8_t ieee80211_me_SnoopListGetMember(struct ieee80211vap* vap, uint8_t* grp_addr, u_int8_t *grp_ipaddr,
                                               u_int32_t src_ip_addr, uint8_t* table, int table_len, u_int32_t ether_type);
static void ieee80211_me_remove_node_grp(struct MC_GROUP_LIST_ENTRY* grp_list,
                                         struct ieee80211_node* ni);
static void ieee80211_me_clean_snp_list(struct ieee80211vap* vap);
static void ieee80211_me_SnoopListInit(struct ieee80211vap *vap);
static int ieee80211_me_SnoopConvert(struct ieee80211vap *vap, wbuf_t wbuf);
static void ieee80211_me_SnoopWDSNodeCleanup(struct ieee80211_node *ni);
static void ieee80211_me_SnoopListDump(struct ieee80211vap *vap);
static void ieee80211_me_SnoopShowDenyTable(struct ieee80211vap *vap);
static void ieee80211_me_SnoopClearDenyTable(struct ieee80211vap *vap);
static void ieee80211_me_SnoopDeleteGrp(struct ieee80211vap* vap, uint8_t *grp_addr, uint8_t *grp_ipaddr, u_int32_t ether_type);
static int  ieee80211_me_SnoopAddDenyEntry(struct ieee80211vap *vap, int *grpaddr);
int ieee80211_me_attach(struct ieee80211vap * vap);
static void ol_ieee80211_remove_all_node_grp(struct MC_GRP_MEMBER_LIST *grp_member_list,
                              struct ieee80211vap *vap);
#endif

#if ATH_SUPPORT_ME_SNOOP_TABLE
static void ieee80211_remove_node_grp(struct MC_GRP_MEMBER_LIST *grp_member_list,struct ieee80211vap *vap)
{
    struct MC_SNOOP_LIST *snp_list;

    if (vap->iv_ic->ic_is_mode_offload(vap->iv_ic)) {
        ol_ieee80211_remove_node_grp(grp_member_list, vap);
    }
    else{
        snp_list = &vap->iv_me->ieee80211_me_snooplist;
        atomic_inc(&snp_list->msl_group_member_limit);
   }


}

/* Add a member to the list */
static void
ieee80211_me_add_member_list(struct MC_GROUP_LIST_ENTRY* grp_list,
                             struct MC_LIST_UPDATE* list_entry, u_int32_t ether_type)
{
    struct ieee80211vap *vap = list_entry->vap;
    struct MC_GRP_MEMBER_LIST* grp_member_list;
    struct MC_SNOOP_LIST *snp_list = &list_entry->vap->iv_me->ieee80211_me_snooplist;
    u_int8_t *src, *dest, length;

    if(atomic_dec_and_test(&snp_list->msl_group_member_limit))
    {
        atomic_inc(&snp_list->msl_group_member_limit);
        ieee80211_me_remove_expired_group(vap);

        if(atomic_dec_and_test(&snp_list->msl_group_member_limit))
        {
                /*
                 * There is no space for more members,
                 * so undo the allocation done by the "dec"
                 * of the above atomic_dec_and_test.
                 */
                atomic_inc(&snp_list->msl_group_member_limit);
                ieee80211_me_remove_empty_group(vap);
                return;
        }
    }

    grp_member_list = (struct MC_GRP_MEMBER_LIST*) OS_MALLOC(
                       list_entry->vap->iv_ic->ic_osdev,
                       sizeof(struct MC_GRP_MEMBER_LIST),
                       GFP_KERNEL);

    if (ether_type == ETHERTYPE_IP) {
        length = IGMP_IP_ADDR_LENGTH;
    } else {
        length = MLD_IP_ADDR_LENGTH;
    }

    if(grp_member_list != NULL){
        TAILQ_INSERT_TAIL(&grp_list->src_list, grp_member_list, member_list);
        grp_member_list->src_ip_addr = list_entry->src_ip_addr;
        grp_member_list->niptr       = list_entry->ni;
        grp_member_list->vap         = list_entry->vap;
        grp_member_list->mode        = list_entry->cmd;
        grp_member_list->timestamp   = list_entry->timestamp;
        grp_member_list->type        = ether_type;
        IEEE80211_ADDR_COPY(grp_member_list->grp_member_addr,list_entry->grp_member);
        dest = (int8_t *)(&grp_member_list->u);
        src = (int8_t *)(&list_entry->u);
        OS_MEMCPY(dest, src, length);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        {
            struct net_device *devp = OSIF_TO_NETDEV(list_entry->vap->iv_ifp);
            osif_dev  *osifp = ath_netdev_priv(devp);
            struct ieee80211vap *vap = osifp->os_if;
            struct ieee80211com *ic = vap->iv_ic;
            if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                ic->nss_vops->ic_osif_nss_vdev_me_add_member_list(osifp, list_entry, ether_type, length);
            }
        }
#endif

    } else {
        atomic_inc(&snp_list->msl_group_member_limit);
    }
}

/*
 *
 * Function to remove empty group
 *
 */
static void ieee80211_me_remove_empty_group(struct ieee80211vap *vap)
{
        struct MC_SNOOP_LIST *snp_list;
        struct MC_GROUP_LIST_ENTRY* curr_grp_list;
        struct MC_GROUP_LIST_ENTRY* next_grp_list;

        if ((!vap) || (ieee80211_vap_deleted_is_set(vap))) {
                return;
        }

        snp_list = (struct MC_SNOOP_LIST *)&(vap->iv_me->ieee80211_me_snooplist);
        curr_grp_list = TAILQ_FIRST(&snp_list->msl_node);
        while(curr_grp_list != NULL){
                next_grp_list = TAILQ_NEXT(curr_grp_list,grp_list);
                if(!TAILQ_FIRST(&curr_grp_list->src_list)){
                        TAILQ_REMOVE(&snp_list->msl_node,curr_grp_list,grp_list);
                        OS_FREE(curr_grp_list);
                        atomic_inc(&snp_list->msl_group_list_limit);
                }
                curr_grp_list = next_grp_list;
        }
}

/*
 *  Function to remove expired group from the snoop table
 *
 */
static void ieee80211_me_remove_expired_group(struct ieee80211vap *vap)
{
        struct MC_SNOOP_LIST *snp_list;
        struct MC_GROUP_LIST_ENTRY* curr_grp_list;
        struct MC_GROUP_LIST_ENTRY* next_grp_list;
        int ret = 0;

        systime_t timestamp  = OS_GET_TIMESTAMP();
        u_int32_t now        = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp);

        if ((!vap) || (ieee80211_vap_deleted_is_set(vap))) {
                return;
        }

        snp_list = (struct MC_SNOOP_LIST *)&(vap->iv_me->ieee80211_me_snooplist);
        curr_grp_list = TAILQ_FIRST(&snp_list->msl_node);
        while(curr_grp_list != NULL){
                next_grp_list = TAILQ_NEXT(curr_grp_list,grp_list);
                ret = ieee80211_me_remove_expired_member(curr_grp_list,vap,now);
                if(!TAILQ_FIRST(&curr_grp_list->src_list) && ret){
                        TAILQ_REMOVE(&snp_list->msl_node,curr_grp_list,grp_list);
                        OS_FREE(curr_grp_list);
                        atomic_inc(&snp_list->msl_group_list_limit);
                }
                curr_grp_list = next_grp_list;
        }
}

/* Remove group members whose timer are expired */
static int
ieee80211_me_remove_expired_member(struct MC_GROUP_LIST_ENTRY* grp_list,
                                   struct ieee80211vap* vap,
                                   u_int32_t nowtimestamp)
{
    struct MC_GRP_MEMBER_LIST* curr_grp_member_list;
    struct MC_GRP_MEMBER_LIST* next_grp_member_list;
    int ret = 0;

    curr_grp_member_list = TAILQ_FIRST(&grp_list->src_list);

    while(curr_grp_member_list != NULL){
       next_grp_member_list = TAILQ_NEXT(curr_grp_member_list, member_list);
       /* if timeout remove member*/
       if((nowtimestamp - curr_grp_member_list->timestamp) > vap->iv_me->me_timeout){
           /* remove the member from list*/
           TAILQ_REMOVE(&grp_list->src_list, curr_grp_member_list, member_list);
           ieee80211_remove_node_grp(curr_grp_member_list, vap);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
           {
               struct net_device* devp = OSIF_TO_NETDEV(curr_grp_member_list->vap->iv_ifp);
               if(vap->iv_ic->ic_is_mode_offload(vap->iv_ic)) {
                   osif_dev* osifp = ath_netdev_priv(devp);
                   if(osifp->nss_wifiol_ctx && vap->iv_ic->nss_vops) {
                           vap->iv_ic->nss_vops->ic_osif_nss_vdev_me_remove_member_list(osifp,
                               (u_int8_t *)&grp_list->u, grp_list->group_addr,
                               curr_grp_member_list->grp_member_addr, curr_grp_member_list->type);
                   }
               }
           }
#endif
           OS_FREE(curr_grp_member_list);
           ret = 1;
       }
       curr_grp_member_list = next_grp_member_list;
    }
    return ret;
}

/* Remove all entry of a given member address from a group list*/
static void
ieee80211_me_remove_all_member_grp(struct MC_GROUP_LIST_ENTRY* grp_list,
                                   u_int8_t* grp_member_addr)
{
    struct MC_GRP_MEMBER_LIST* curr_grp_member_list;
    struct MC_GRP_MEMBER_LIST* next_grp_member_list;

    curr_grp_member_list = TAILQ_FIRST(&grp_list->src_list);

    while(curr_grp_member_list != NULL){
       next_grp_member_list = TAILQ_NEXT(curr_grp_member_list, member_list);
       if(IEEE80211_ADDR_EQ(curr_grp_member_list->grp_member_addr,grp_member_addr)){
           /* remove the member from list*/
           TAILQ_REMOVE(&grp_list->src_list, curr_grp_member_list, member_list);
           ieee80211_remove_node_grp(curr_grp_member_list, curr_grp_member_list->vap);

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
           {
               struct net_device* devp = OSIF_TO_NETDEV(curr_grp_member_list->vap->iv_ifp);
               osif_dev* osifp = ath_netdev_priv(devp);
               struct ieee80211vap *vap = osifp->os_if;
               struct ieee80211com *ic = vap->iv_ic;
               if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                   ic->nss_vops->ic_osif_nss_vdev_me_remove_member_list(osifp, (u_int8_t *)&grp_list->u,
                                    grp_list->group_addr, curr_grp_member_list->grp_member_addr, curr_grp_member_list->type);
               }
           }
#endif

           OS_FREE(curr_grp_member_list);
       }
       curr_grp_member_list = next_grp_member_list;
    }
}

/* find a member based on member mac addr and source ip address */
static struct MC_GRP_MEMBER_LIST*
ieee80211_me_find_member_src(struct MC_GROUP_LIST_ENTRY* grp_list,
                             u_int8_t* grp_member_addr,
                             u_int32_t src_ip_addr)
{
    struct MC_GRP_MEMBER_LIST* grp_member_list;
    TAILQ_FOREACH(grp_member_list, &grp_list->src_list,member_list){
        if((IEEE80211_ADDR_EQ(grp_member_list->grp_member_addr,grp_member_addr))
            &&  (grp_member_list->src_ip_addr == src_ip_addr))
        {
            return(grp_member_list);
        }
    }
    return(NULL);
}

/* find a member based on group member mac addr */
static struct MC_GRP_MEMBER_LIST*
ieee80211_me_find_member(struct MC_GROUP_LIST_ENTRY* grp_list,
                         u_int8_t* grp_member_addr)
{
    struct MC_GRP_MEMBER_LIST* grp_member_list;

    TAILQ_FOREACH(grp_member_list, &grp_list->src_list,member_list){
       if(IEEE80211_ADDR_EQ(grp_member_list->grp_member_addr,grp_member_addr)){
           return(grp_member_list);
       }
    }
    return(NULL);
}

/* find a group based on group address */
static struct MC_GROUP_LIST_ENTRY*
ieee80211_me_find_group_list(struct ieee80211vap* vap, uint8_t *grp_addr, u_int8_t *grp_ipaddr, u_int32_t ether_type)
{
    struct MC_SNOOP_LIST       *snp_list;
    struct MC_GROUP_LIST_ENTRY *grp_list;
    char *grpaddr;
    int length;

    if (ether_type == ETHERTYPE_IP) {
        length = IGMP_IP_ADDR_LENGTH;
    } else {
        length = MLD_IP_ADDR_LENGTH;
    }

    snp_list = &(vap->iv_me->ieee80211_me_snooplist);
    TAILQ_FOREACH(grp_list,&snp_list->msl_node,grp_list){
       grpaddr = (int8_t *)(&grp_list->u);
       if (IEEE80211_ADDR_EQ(grp_addr, grp_list->group_addr) &&
                     (OS_MEMCMP(grp_ipaddr, grpaddr, length) == 0)) {
           return(grp_list);
       }
    }
    return(NULL);
}

/* create a group list
 * find is group list is already present,
 * if no group list found, then create a new list
 * if group list is alreay existing return that group list */
static struct MC_GROUP_LIST_ENTRY*
ieee80211_me_create_grp_list (struct ieee80211vap* vap, u_int8_t* grp_addr,
                          u_int8_t *grp_ipaddr, u_int32_t ether_type, int cmd)
{
    struct MC_SNOOP_LIST       *snp_list;
    struct MC_GROUP_LIST_ENTRY *grp_list;
    rwlock_state_t lock_state;
    u_int8_t length,*dest;

    snp_list = &vap->iv_me->ieee80211_me_snooplist;
    grp_list = ieee80211_me_find_group_list(vap,grp_addr, grp_ipaddr, ether_type);
    if((grp_list == NULL) && (cmd==IGMP_SNOOP_CMD_ADD_INC_LIST)){
            if(atomic_dec_and_test(&snp_list->msl_group_list_limit))
            {
                    atomic_inc(&snp_list->msl_group_list_limit);
                    ieee80211_me_remove_expired_group(vap);
                    if(atomic_dec_and_test(&snp_list->msl_group_list_limit))
                    {

                            /*
                             * There is no space for more groups,
                             * so undo the allocation done by the "dec"
                             * of the above atomic_dec_and_test.
                             */
                            atomic_inc(&snp_list->msl_group_list_limit);
                            return NULL;
                    }
            }

        grp_list = (struct MC_GROUP_LIST_ENTRY *)OS_MALLOC(
                   vap-> iv_ic->ic_osdev,
                   sizeof(struct MC_GROUP_LIST_ENTRY),
                   GFP_KERNEL);

        if(grp_list != NULL){
            IEEE80211_SNOOP_LOCK(snp_list,&lock_state);
            TAILQ_INSERT_TAIL(&snp_list->msl_node,grp_list,grp_list);
            IEEE80211_ADDR_COPY(grp_list->group_addr,grp_addr);

            if (ether_type == ETHERTYPE_IP) {
                length = IGMP_IP_ADDR_LENGTH;
            } else {
                length = MLD_IP_ADDR_LENGTH;
            }

            dest = (int8_t *)(&grp_list->u);
            OS_MEMCPY(dest,grp_ipaddr,length);
            TAILQ_INIT(&grp_list->src_list);
            IEEE80211_SNOOP_UNLOCK(snp_list, &lock_state);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            {
                struct net_device* devp = OSIF_TO_NETDEV(vap->iv_ifp);
                osif_dev* osifp = ath_netdev_priv(devp);
                struct ieee80211vap *vap = osifp->os_if;
                struct ieee80211com *ic = vap->iv_ic;
                if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                    ic->nss_vops->ic_osif_nss_vdev_me_create_grp_list(osifp, grp_addr, grp_ipaddr, ether_type, length);
                }
            }
#endif
        } else {
            atomic_inc(&snp_list->msl_group_list_limit);
        }
    }
    return(grp_list);
}

/**************************************************************************
 * !
 * \brief Show the Group Deny Table
 *
 * \param vap
 *
 * \return N/A
 */
static void ieee80211_me_SnoopShowDenyTable(struct ieee80211vap *vap)
{
    int idx;
    struct MC_SNOOP_LIST *ps;
    rwlock_state_t lock_state;

    ps = &(vap->iv_me->ieee80211_me_snooplist);
    IEEE80211_SNOOP_LOCK(ps, &lock_state);
    if (ps->msl_deny_count == 0) {
        IEEE80211_SNOOP_UNLOCK(ps, &lock_state);
        return;
    }
    for (idx = 0 ; idx < ps->msl_deny_count; idx ++) {
        printk("%d - addr : %d.%d.%d.%d, mask : %d.%d.%d.%d\n", idx + 1,
               (ps->msl_deny_group[idx] >> 24) & 0xff,
               (ps->msl_deny_group[idx] >> 16) & 0xff,
               (ps->msl_deny_group[idx] >> 8) & 0xff,
               (ps->msl_deny_group[idx] & 0xff),
               (ps->msl_deny_mask[idx] >> 24) & 0xff,
               (ps->msl_deny_mask[idx] >> 16) & 0xff,
               (ps->msl_deny_mask[idx] >> 8) & 0xff,
               (ps->msl_deny_mask[idx] & 0xff));
    }
    IEEE80211_SNOOP_UNLOCK(ps, &lock_state);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    {
        struct net_device* devp = OSIF_TO_NETDEV(vap->iv_ifp);
        osif_dev* osifp = ath_netdev_priv(devp);
        struct ieee80211vap *vap = osifp->os_if;
        struct ieee80211com *ic = vap->iv_ic;
        if (osifp->nss_wifiol_ctx && ic->nss_vops) {
            ic->nss_vops->ic_osif_nss_vdev_me_dump_denylist(osifp);
        }
    }
#endif
}

static int ieee80211_me_ipv6_SnooplsDenied(struct ieee80211vap *vap,u_int8_t *ether_dhost, u_int8_t *ipv6)
{
    struct MC_SNOOP_LIST *ps;
    int i = 0;
    struct ipv6_mcast_deny *p;
    ps = &(vap->iv_me->ieee80211_me_snooplist);
    p = &(ps->msl_ipv6_deny_group[0]);

    for (i=0; i < ps->msl_ipv6_deny_list_length; p++, i++) {
        if ((OS_MEMCMP(ether_dhost, &p->mac, IEEE80211_ADDR_LEN) == 0) &&
             (OS_MEMCMP(ipv6, (u_int8_t *)&p->dst.in6_u,
                           sizeof(struct in6_addr)) == 0)) {
            return 1;
        }
    }
    return 0;

}

/*
 * Check if the address is in deny list
 */
static int
ieee80211_me_SnoopIsDenied(struct ieee80211vap *vap, u_int32_t grpaddr)
{
    int idx;
    struct MC_SNOOP_LIST *ps;
    ps = &(vap->iv_me->ieee80211_me_snooplist);

    if (ps->msl_deny_count == 0) {
        return 0;
    }
    for (idx = 0 ; idx < ps->msl_deny_count; idx ++) {
        if (grpaddr != ps->msl_deny_group[idx])
        {
             continue;
        }
        return 1;
    }
    return 0;
}

/*
 * Clear the Snoop Deny Table
 */
static void
ieee80211_me_SnoopClearDenyTable(struct ieee80211vap *vap)
{
    struct MC_SNOOP_LIST *ps;

    ps = &(vap->iv_me->ieee80211_me_snooplist);
    ps->msl_deny_count = 0;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    {
        struct net_device* devp = OSIF_TO_NETDEV(vap->iv_ifp);
        osif_dev* osifp = ath_netdev_priv(devp);
        struct ieee80211com *ic = vap->iv_ic;
        if (osifp->nss_wifiol_ctx && ic->nss_vops) {
            ic->nss_vops->ic_osif_nss_vdev_me_delete_deny_list(osifp);
        }
    }
#endif
    return;
}

/*
 * Add the Snoop Deny Table entry
 */
static int
ieee80211_me_SnoopAddDenyEntry(struct ieee80211vap *vap, int *grpaddr)
{
    u_int8_t idx;
    struct MC_SNOOP_LIST *ps;
    rwlock_state_t lock_state;
    ps = &(vap->iv_me->ieee80211_me_snooplist);
    idx = ps->msl_deny_count;
    if(ps->msl_deny_count >= GRPADDR_FILTEROUT_N) {
        printk("\n Deny table entries exceeded\n");
        return -1;
    }
    IEEE80211_SNOOP_LOCK(ps, &lock_state);
#define SNOOP_TABLE_DENY_MASK 0xffffff00UL
    ps->msl_deny_count ++;
    ps->msl_deny_group[idx] = *(u_int32_t*)grpaddr;
    ps->msl_deny_mask[idx]  = SNOOP_TABLE_DENY_MASK;
#undef SNOOP_TABLE_DENY_MASK
    IEEE80211_SNOOP_UNLOCK(ps, &lock_state);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    {
        struct net_device* devp = OSIF_TO_NETDEV(vap->iv_ifp);
        osif_dev* osifp = ath_netdev_priv(devp);
        struct ieee80211com *ic = vap->iv_ic;
        if (osifp->nss_wifiol_ctx && ic->nss_vops) {
            ic->nss_vops->ic_osif_nss_vdev_me_add_deny_member(osifp, *(u_int32_t*)grpaddr);
        }
    }
#endif
    return 0;

}

/* print all the group entries */
static void
ieee80211_me_SnoopListDump(struct ieee80211vap* vap)
{
    struct MC_SNOOP_LIST *snp_list = &(vap->iv_me->ieee80211_me_snooplist);
    struct MC_GROUP_LIST_ENTRY* grp_list;
    struct MC_GRP_MEMBER_LIST* grp_member_list;
    rwlock_state_t lock_state;

    if(vap->iv_opmode == IEEE80211_M_HOSTAP &&
        vap->iv_me->mc_snoop_enable &&
        snp_list != NULL)
    {
        IEEE80211_SNOOP_LOCK(snp_list,&lock_state);
        TAILQ_FOREACH(grp_list,&snp_list->msl_node,grp_list){
            printk("group addr %x:%x:%x:%x:%x:%x \n",grp_list->group_addr[0],
                                                     grp_list->group_addr[1],
                                                     grp_list->group_addr[2],
                                                     grp_list->group_addr[3],
                                                     grp_list->group_addr[4],
                                                     grp_list->group_addr[5]);

            TAILQ_FOREACH(grp_member_list, &grp_list->src_list,member_list){
                printk("    src_ip_addr %d:%d:%d:%d \n",((grp_member_list->src_ip_addr >> 24) & 0xff),
                                                        ((grp_member_list->src_ip_addr >> 16) & 0xff),
                                                        ((grp_member_list->src_ip_addr >>  8) & 0xff),
                                                        ((grp_member_list->src_ip_addr) & 0xff));
                printk("    member addr %x:%x:%x:%x:%x:%x\n",grp_member_list->grp_member_addr[0],
                                                             grp_member_list->grp_member_addr[1],
                                                             grp_member_list->grp_member_addr[2],
                                                             grp_member_list->grp_member_addr[3],
                                                             grp_member_list->grp_member_addr[4],
                                                             grp_member_list->grp_member_addr[5]);
                printk("    Type 0x%x \n",grp_member_list->type);
                if (grp_member_list->type == ETHERTYPE_IP) {
                    printk("   ipv4 addr=0x%x\r\n",grp_member_list->u.grpaddr_ip4);
                } else {
                    int i;
                    printk("   ipv6 addr=");
                    for (i = 0; i < MLD_IP_ADDR_LENGTH; i++) {
                        printk("%x ",grp_member_list->u.grpaddr_ip6[i]);
                    }
                    printk("\r\n-------------------------------\r\n");
                }
                printk("    Mode %d \n",grp_member_list->mode);
            }
        }
        IEEE80211_SNOOP_UNLOCK(snp_list, &lock_state);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
	{
	    struct net_device* devp = OSIF_TO_NETDEV(vap->iv_ifp);
	    osif_dev* osifp = ath_netdev_priv(devp);
            struct ieee80211com *ic = vap->iv_ic;
	    if (osifp->nss_wifiol_ctx && ic->nss_vops) {
	        ic->nss_vops->ic_osif_nss_vdev_me_dump_snooplist(osifp);
	    }
	}
#endif
    }
}

/* To delete the group & all its members */
static void ieee80211_me_SnoopDeleteGrp(struct ieee80211vap* vap, uint8_t *grp_addr, u_int8_t *grp_ipaddr, u_int32_t ether_type)
{
    struct MC_GROUP_LIST_ENTRY    *grp_list;
    struct MC_SNOOP_LIST          *snp_list;

    snp_list = &(vap->iv_me->ieee80211_me_snooplist);
    grp_list = ieee80211_me_find_group_list(vap, grp_addr, grp_ipaddr, ether_type);
    if(grp_list){
        /* Delete all the group members
           target's group count is reduced in node_grp function,
           so not required to increment the count here. */
        ieee80211_me_remove_node_grp(grp_list, NULL);
        if(!TAILQ_FIRST(&grp_list->src_list)){
            TAILQ_REMOVE(&snp_list->msl_node,grp_list, grp_list);
            if (vap->iv_ic->ic_is_mode_offload(vap->iv_ic)) {
                atomic_inc(&snp_list->msl_group_list_limit);
            }
            OS_FREE(grp_list);
        }
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
        {
            struct net_device* devp = OSIF_TO_NETDEV(vap->iv_ifp);
            osif_dev* osifp = ath_netdev_priv(devp);
            struct ieee80211com *ic = vap->iv_ic;
            if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                ic->nss_vops->ic_osif_nss_vdev_me_delete_grp_list(osifp, grp_addr, (u_int8_t *)&grp_list->u, ether_type);
            }
        }
#endif
    }
}
#endif

/* update the snoop table based on the received entries*/
void
ieee80211_me_SnoopListUpdate_timestamp(struct MC_LIST_UPDATE* list_entry, u_int32_t ether_type)
{
#if ATH_SUPPORT_ME_SNOOP_TABLE
    struct MC_SNOOP_LIST* snp_list;
    struct MC_GROUP_LIST_ENTRY* grp_list;
    struct MC_GRP_MEMBER_LIST* grp_member_list;
    systime_t timestamp;
    rwlock_state_t lock_state;
    struct ieee80211vap* vap = NULL;
    u_int8_t *grp_addr, *grp_ipaddr;


    vap = list_entry->vap;
    grp_addr = list_entry->grp_addr;
    grp_ipaddr = (int8_t *)(&list_entry->u);

    timestamp  = OS_GET_TIMESTAMP();
    list_entry->timestamp = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp);
    snp_list = &(list_entry->vap->iv_me->ieee80211_me_snooplist);
    grp_list = ieee80211_me_find_group_list(vap, grp_addr, grp_ipaddr, ether_type);

    if(grp_list != NULL){
        IEEE80211_SNOOP_LOCK(snp_list,&lock_state);
        /* if entry is for particular source then,
         * find is member is already list in the table
         * if yes update the entry with new values
         * if no create a new entry and add member details*/
        if(list_entry->src_ip_addr){
            /*Not any source */
           grp_member_list = ieee80211_me_find_member_src(grp_list,list_entry->grp_member,list_entry->src_ip_addr);
            if(grp_member_list != NULL){
                grp_member_list->timestamp = list_entry->timestamp;

            }
        } else {
            /*Any source
              find the grp_member, if src address is non zero, there is possiblity of few more members list
              if src_ip_address is zero then check the cmd, any source no action, exclude_list remove the entry
              Remove all grp member entries
              if cmd is any source, then include in the list, src addr 0 mode include
              if cmd is exclude_list, no need to add the list*/
            grp_member_list = ieee80211_me_find_member(grp_list,list_entry->grp_member);
            if(grp_member_list != NULL){
		    grp_member_list->timestamp = list_entry->timestamp;
	    }

        }
        IEEE80211_SNOOP_UNLOCK(snp_list, &lock_state);
    }
#endif
}
EXPORT_SYMBOL(ieee80211_me_SnoopListUpdate_timestamp);

#if ATH_SUPPORT_ME_SNOOP_TABLE
/* update the snoop table based on the received entries*/
void
ieee80211_me_SnoopListUpdate(struct MC_LIST_UPDATE* list_entry, u_int32_t ether_type)
{
    struct MC_SNOOP_LIST* snp_list;
    struct MC_GROUP_LIST_ENTRY* grp_list;
    struct MC_GRP_MEMBER_LIST* grp_member_list;
    systime_t timestamp;
    rwlock_state_t lock_state;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    struct net_device* devp = OSIF_TO_NETDEV(list_entry->vap->iv_ifp);
    osif_dev* osifp = ath_netdev_priv(devp);
    struct ieee80211vap *vap = osifp->os_if;
    struct ieee80211com *ic = vap->iv_ic;
#endif

    timestamp  = OS_GET_TIMESTAMP();
    list_entry->timestamp = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp);
    snp_list = &(list_entry->vap->iv_me->ieee80211_me_snooplist);

    grp_list = ieee80211_me_create_grp_list(list_entry->vap,list_entry->grp_addr,  (int8_t *)(&list_entry->u), ether_type, list_entry->cmd);

    if(grp_list != NULL){
        IEEE80211_SNOOP_LOCK(snp_list,&lock_state);
        /* if entry is for particular source then,
         * find is member is already list in the table
         * if yes update the entry with new values
         * if no create a new entry and add member details*/
        if(list_entry->src_ip_addr){
            /*Not any source */
           grp_member_list = ieee80211_me_find_member_src(grp_list,list_entry->grp_member,list_entry->src_ip_addr);
            if(grp_member_list != NULL){
                grp_member_list->mode      = list_entry->cmd;
                grp_member_list->timestamp = list_entry->timestamp;
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                    ic->nss_vops->ic_osif_nss_vdev_me_update_member_list(osifp, list_entry, ether_type);
                }
#endif
            } else {
                ieee80211_me_add_member_list(grp_list, list_entry, ether_type);
            }
        } else {
            /*Any source
              find the grp_member, if src address is non zero, there is possiblity of few more members list
              if src_ip_address is zero then check the cmd, any source no action, exclude_list remove the entry
              Remove all grp member entries
              if cmd is any source, then include in the list, src addr 0 mode include
              if cmd is exclude_list, no need to add the list*/
            grp_member_list = ieee80211_me_find_member(grp_list,list_entry->grp_member);
            if(grp_member_list != NULL){
                if(grp_member_list->src_ip_addr) {
                    ieee80211_me_remove_all_member_grp(grp_list,list_entry->grp_member);
                    if(list_entry->cmd == IGMP_SNOOP_CMD_ADD_INC_LIST) {
                        /* alloc memory
                           add to the list*/
                       ieee80211_me_add_member_list(grp_list, list_entry, ether_type);
                    } else {
                        /* when no member is added, then delete the grp_list if the grp_list is empty
                         */
                        if(!TAILQ_FIRST(&grp_list->src_list)){
                            TAILQ_REMOVE(&snp_list->msl_node,grp_list,grp_list);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                            if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                                ic->nss_vops->ic_osif_nss_vdev_me_delete_grp_list(osifp,
                                        grp_list->group_addr, (u_int8_t *)&grp_list->u, ether_type);
                            }
#endif
                            OS_FREE(grp_list);
                            atomic_inc(&snp_list->msl_group_list_limit);
                        }
                    }
                } else if(list_entry->cmd == IGMP_SNOOP_CMD_ADD_EXC_LIST) {
                    /* remove the member from list*/
                   TAILQ_REMOVE(&grp_list->src_list, grp_member_list, member_list);
                   ieee80211_remove_node_grp(grp_member_list, list_entry->vap);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                   if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                       ic->nss_vops->ic_osif_nss_vdev_me_remove_member_list(osifp, (u_int8_t *)&grp_list->u,
                               grp_list->group_addr, grp_member_list->grp_member_addr, ether_type);
                   }
#endif
                   OS_FREE(grp_member_list);
                   if(!TAILQ_FIRST(&grp_list->src_list)){
                       TAILQ_REMOVE(&snp_list->msl_node,grp_list,grp_list);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                       if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                           ic->nss_vops->ic_osif_nss_vdev_me_delete_grp_list(osifp,
                                   grp_list->group_addr, (u_int8_t *)&grp_list->u, ether_type);
                       }
#endif
                       OS_FREE(grp_list);
                       atomic_inc(&snp_list->msl_group_list_limit);
                   }
                }
            } else {
                if(list_entry->cmd != IGMP_SNOOP_CMD_ADD_EXC_LIST) {
                    /* Alloc memory and add to the list*/
                    ieee80211_me_add_member_list(grp_list, list_entry, ether_type);
                }
            }
        }
        IEEE80211_SNOOP_UNLOCK(snp_list, &lock_state);
    }
}
#endif

struct mld2_grec {
    __u8 grec_type;
    __u8 grec_auxwords;
    __be16 grec_nsrcs;
    struct in6_addr grec_mca;
    struct in6_addr grec_src[0];
};

struct mld_msg {
    struct icmp6hdr mld_hdr;
    struct in6_addr mld_mca;
};

/*************************************************************************
 * !
 * \brief Build up the snoop list by monitoring the IGMP packets
 *
 * \param ni Pointer to the node
 *           skb Pointer to the skb buffer
 *
 * \return 0: Not IGMP/MLD 1: IGMP/MLD, but not Query. 2: Query
 */
static int
ieee80211_me_SnoopInspecting(struct ieee80211vap *vap, struct ieee80211_node *ni, wbuf_t wbuf)
{
    int retval = 0;
    u_int8_t type = 0;
    struct ieee80211com *ic = vap->iv_ic;
    u_int8_t deep_igmp_inspect =  ic->ic_dropstaquery | ic->ic_blkreportflood ;
#if ATH_SUPPORT_ME_SNOOP_TABLE
    struct ether_header *eh = (struct ether_header *) wbuf_header(wbuf);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    struct net_device* devp = OSIF_TO_NETDEV(vap->iv_ifp);
    osif_dev* osifp = ath_netdev_priv(devp);
#endif
    char *grp_ipaddr;
#endif

    if (vap->iv_me->me_hifi_enable && vap->iv_opmode == IEEE80211_M_HOSTAP && deep_igmp_inspect &&
            ieee80211_me_igmp_mld_inspect(vap, wbuf, &type, NULL, NULL)) {
        switch (type) {
            case IGMP_QUERY:
            case MLD_QUERY:
                retval = IEEE80211_QUERY_FROM_STA;
                IEEE80211_NOTE(vap, IEEE80211_MSG_IQUE, ni,
                        "%s: Query from STA.", __func__);
                break;
            case IGMPv1_REPORT:
            case IGMPv2_REPORT:
            case IGMP_LEAVE:
            case IGMPv3_REPORT:
            case MLD_REPORT:
            case MLD_LEAVE:
            case MLDv2_REPORT:
                retval = IEEE80211_REPORT_FROM_STA;
                IEEE80211_NOTE(vap, IEEE80211_MSG_IQUE, ni,
                        "%s: Report from STA.\n", __func__);
                break;
        }
    }
#if ATH_SUPPORT_ME_SNOOP_TABLE
    if (!vap->iv_me->mc_snoop_enable){
        return retval;
    }

    if (vap->iv_opmode == IEEE80211_M_HOSTAP ) {
        if (IEEE80211_IS_MULTICAST(eh->ether_dhost) &&
            !IEEE80211_IS_BROADCAST(eh->ether_dhost)) {
            if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
                const struct ip_header *ip = (struct ip_header *)
                    (wbuf_header(wbuf) + sizeof (struct ether_header));
                if (ip->protocol == IPPROTO_IGMP) {
                    int                         ip_headerlen;       /*ip header len based on endiness*/
                    const struct igmp_header    *igmp;              /* igmp header for v1 and v2*/
                    const struct igmp_v3_report *igmpr3;            /* igmp header for v3*/
                    const struct igmp_v3_grec   *grec;              /* igmp group record*/
                    u_int32_t                     groupAddr = 0;     /* to hold group address from group record*/
                    u_int8_t                     *srcAddr = eh->ether_shost; /* source address which send the report and it is the member*/
                    u_int8_t                     groupAddrL2[IEEE80211_ADDR_LEN]; /*group multicast mac address*/
                    struct MC_SNOOP_LIST*        snp_list;
                    struct MC_LIST_UPDATE        list_entry;        /* list entry where all member details will be updated and passed on updating the snoop list*/
                    struct MC_GROUP_LIST_ENTRY*  grp_list;
                    struct MC_GRP_MEMBER_LIST*   grp_member_list;
                    rwlock_state_t lock_state;

                    /*
                     * Zero out MC_LIST_UPDATE structure
                     */
                    qdf_mem_set((uint8_t *)&list_entry, sizeof(struct MC_LIST_UPDATE), 0);

                    /* Init multicast group address conversion */
                    groupAddrL2[0] = 0x01;
                    groupAddrL2[1] = 0x00;
                    groupAddrL2[2] = 0x5e;

                    /*pre fill the list_entry members which are going to be const in this function*/
                    IEEE80211_ADDR_COPY(list_entry.grp_member,srcAddr);
                    list_entry.vap        = vap;
                    list_entry.ni         = ni;

                    ip_headerlen = ip->version_ihl & 0x0F;

                    /* v1 & v2 */
                    igmp = (struct igmp_header *)
                        (wbuf_header(wbuf) + sizeof (struct ether_header) + (4 * ip_headerlen));
                    /* ver 3*/
                    igmpr3 = (struct igmp_v3_report *) igmp;

                    /* If packet is not IGMP report or IGMP leave don't handle it*/
                    if(!IS_IGMP_REPORT_LEAVE_PACKET(igmp->type)){
                        if ((igmp->type == IGMP_QUERY) ||
                            (igmp->type == MLD_QUERY)) {
                            retval = IEEE80211_QUERY_FROM_STA;
                        }
                        return retval;
                    }
                    if(igmp->type != IGMPV3_REPORT_TYPE){
                        groupAddr = ntohl(igmp->group);
                        /* Check is group addres is in deny list */
                        if(ieee80211_me_SnoopIsDenied(vap,groupAddr)){
                            return retval;
                        }
                        if(igmp->type != IGMPV2_LEAVE_TYPE){
                            list_entry.cmd = IGMP_SNOOP_CMD_ADD_INC_LIST;
                        } else {
                            list_entry.cmd = IGMP_SNOOP_CMD_ADD_EXC_LIST;
                        }
                        groupAddrL2[3] = (groupAddr >> 16) & 0x7f;
                        groupAddrL2[4] = (groupAddr >>  8) & 0xff;
                        groupAddrL2[5] = (groupAddr >>  0) & 0xff;
                        list_entry.u.grpaddr_ip4 = groupAddr;
                        IEEE80211_ADDR_COPY(list_entry.grp_addr, groupAddrL2);
                        list_entry.src_ip_addr = 0;
                        ieee80211_me_SnoopListUpdate(&list_entry, ETHERTYPE_IP);
                    } else {
                        u_int16_t no_grec; /* no of group records  */
                        u_int16_t no_srec; /* no of source records */

                        /*  V3 report handling */
                        no_grec = ntohs(igmpr3->ngrec);
                        snp_list = &(vap->iv_me->ieee80211_me_snooplist);
                        grec = (struct igmp_v3_grec*)((u_int8_t*)igmpr3 + 8);
                        /* loop to handle all group records */
                        while(no_grec){
                            u_int32_t *src_addr;
                            /* Group recod handling */
                            /* no of srcs record    */
                            list_entry.cmd = grec->grec_type;
                            groupAddr = ntohl(grec->grec_mca);
                            /* Check is group addres is in deny list */
                            if(ieee80211_me_SnoopIsDenied(vap,groupAddr)){
                                /* move the grec to next group record*/
                                grec = (struct igmp_v3_grec*)((u_int8_t*)grec + IGMPV3_GRP_REC_LEN(grec));
                                no_grec--;
                                continue;
                            }
                            no_srec = ntohs(grec->grec_nsrcs);
                            src_addr = (u_int32_t*)((u_int8_t*)grec + sizeof(struct igmp_v3_grec));
                            groupAddrL2[3] = (groupAddr >> 16) & 0x7f;
                            groupAddrL2[4] = (groupAddr >>  8) & 0xff;
                            groupAddrL2[5] = (groupAddr >>  0) & 0xff;
                            IEEE80211_ADDR_COPY(list_entry.grp_addr,groupAddrL2);
                            list_entry.u.grpaddr_ip4 = groupAddr;
                            if (grec->grec_type == IGMPV3_CHANGE_TO_EXCLUDE ||
                                grec->grec_type == IGMPV3_MODE_IS_EXCLUDE)
                            {
                                list_entry.cmd = IGMP_SNOOP_CMD_ADD_EXC_LIST;
                                /* remove old member entries as new member entries are received */
                                grp_ipaddr = (int8_t *)(&list_entry.u);
                                grp_list = ieee80211_me_find_group_list(vap, list_entry.grp_addr, grp_ipaddr, ETHERTYPE_IP);
                                if(grp_list != NULL){
                                    IEEE80211_SNOOP_LOCK(snp_list,&lock_state);
                                    grp_member_list = ieee80211_me_find_member(grp_list,
                                                               list_entry.grp_member);
                                    ieee80211_me_remove_all_member_grp(grp_list,list_entry.grp_member);
                                    IEEE80211_SNOOP_UNLOCK(snp_list, &lock_state);
                                }
                                 /* if no source record is there then it is include for all source */
                                if(!no_srec){
                                    list_entry.u.grpaddr_ip4 = groupAddr;
                                    list_entry.src_ip_addr = 0;
                                    list_entry.cmd = IGMP_SNOOP_CMD_ADD_INC_LIST;
                                    ieee80211_me_SnoopListUpdate(&list_entry, ETHERTYPE_IP);
                                } else {
                                    IEEE80211_SNOOP_LOCK(snp_list,&lock_state);
                                    if((grp_list !=NULL) && !TAILQ_FIRST(&grp_list->src_list)){
                                        TAILQ_REMOVE(&snp_list->msl_node,grp_list,grp_list);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                                        if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                                            ic->nss_vops->ic_osif_nss_vdev_me_delete_grp_list(osifp,
                                                    grp_list->group_addr, (u_int8_t *)&grp_list->u, ntohs(eh->ether_type));
                                        }
#endif
                                        OS_FREE(grp_list);
                                        atomic_inc(&snp_list->msl_group_list_limit);
                                    }
                                    IEEE80211_SNOOP_UNLOCK(snp_list, &lock_state);
                                }
                            } else if (grec->grec_type == IGMPV3_CHANGE_TO_INCLUDE ||
                                       grec->grec_type == IGMPV3_MODE_IS_INCLUDE)
                            {
                                list_entry.cmd = IGMP_SNOOP_CMD_ADD_INC_LIST;
                                grp_ipaddr = (int8_t *)(&list_entry.u);
                                grp_list = ieee80211_me_find_group_list(vap,list_entry.grp_addr, grp_ipaddr,ETHERTYPE_IP);
                                if(grp_list != NULL){
                                    IEEE80211_SNOOP_LOCK(snp_list,&lock_state);
                                    grp_member_list = ieee80211_me_find_member(grp_list,
                                                               list_entry.grp_member);
                                    ieee80211_me_remove_all_member_grp(grp_list,list_entry.grp_member);
                                    if(!TAILQ_FIRST(&grp_list->src_list)){
                                        TAILQ_REMOVE(&snp_list->msl_node,grp_list,grp_list);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                                        if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                                            ic->nss_vops->ic_osif_nss_vdev_me_delete_grp_list(osifp,
                                                    grp_list->group_addr, (u_int8_t *)&grp_list->u, ntohs(eh->ether_type));
                                        }
#endif
                                        OS_FREE(grp_list);
                                        atomic_inc(&snp_list->msl_group_list_limit);
                                    }
                                    IEEE80211_SNOOP_UNLOCK(snp_list, &lock_state);
                                    /* if no of srec is zero and mode is include, it is exclude for all source
                                 * no need to update as member is already removed in above lines */
                                }
                            } else if  (grec->grec_type == IGMPV3_ALLOW_NEW_SOURCES) {
                                list_entry.cmd = IGMP_SNOOP_CMD_ADD_INC_LIST;
                            }

                            while(no_srec){
                                list_entry.u.grpaddr_ip4 = groupAddr;
                                list_entry.src_ip_addr = ntohl(*src_addr);
                                if(grec->grec_type != IGMPV3_BLOCK_OLD_SOURCES){
                                    /* Source record handling*/
                                    ieee80211_me_SnoopListUpdate(&list_entry, ETHERTYPE_IP);
                                } else {
                                    grp_ipaddr = (int8_t *)(&list_entry.u);
                                    grp_list = ieee80211_me_find_group_list(vap,list_entry.grp_addr, grp_ipaddr,ETHERTYPE_IP);
                                    if(grp_list != NULL){
                                        grp_member_list = ieee80211_me_find_member_src(grp_list,
                                                               list_entry.grp_member,list_entry.src_ip_addr);
                                        if(grp_member_list != NULL){
                                            IEEE80211_SNOOP_LOCK(snp_list,&lock_state);
                                            TAILQ_REMOVE(&grp_list->src_list, grp_member_list, member_list);

                                            ieee80211_remove_node_grp(grp_member_list, list_entry.vap);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                                            if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                                                ic->nss_vops->ic_osif_nss_vdev_me_remove_member_list(osifp,
                                                        grp_ipaddr, (uint8_t *)grp_list->group_addr,
                                                        (uint8_t *)grp_member_list->grp_member_addr, ETHERTYPE_IP);
                                            }
#endif
                                            OS_FREE(grp_member_list);
                                            if(!TAILQ_FIRST(&grp_list->src_list)){
                                                TAILQ_REMOVE(&snp_list->msl_node,grp_list,grp_list);
#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
                                                if (osifp->nss_wifiol_ctx && ic->nss_vops) {
                                                    ic->nss_vops->ic_osif_nss_vdev_me_delete_grp_list(osifp,
                                                            grp_list->group_addr, (u_int8_t *)&grp_list->u, ntohs(eh->ether_type));
                                                }
#endif
                                                OS_FREE(grp_list);
                                                atomic_inc(&snp_list->msl_group_list_limit);
                                            }
                                            IEEE80211_SNOOP_UNLOCK(snp_list, &lock_state);
                                        }
                                    }
                                }
                                src_addr++;
                                no_srec--;
                            } /* while of no_srec*/
                            /* move the grec to next group record*/
                            grec = (struct igmp_v3_grec*)((u_int8_t*)grec + IGMPV3_GRP_REC_LEN(grec));
                            no_grec--;
                        } /*while of no_grec*/
                    } /* else of IGMPV3_REPORT_TYPE*/
                }
            } else  if (ntohs(eh->ether_type) == ETHERTYPE_IPV6) {
                const qdf_net_ipv6hdr_t *ipv6;
                const qdf_net_icmpv6hdr_t *icmpv6;
                struct in6_addr *mld_mca = NULL;
                int cmd = IGMP_SNOOP_CMD_OTHER;
                u_int8_t     groupAddrL2[IEEE80211_ADDR_LEN]; /*group multicast mac address*/
                struct MC_LIST_UPDATE 			 list_entry;
                u_int8_t *srcAddr = eh->ether_shost; /* source address which send the report and it is the member*/
                u_int8_t *src,*dest;

                ipv6 = (qdf_net_ipv6hdr_t *) (wbuf_header(wbuf) + sizeof(struct ether_header));
                if (ieee80211_me_ipv6_SnooplsDenied(vap,eh->ether_dhost,
                            (u_int8_t *)&ipv6->ipv6_daddr.in6_u)) {
                    IEEE80211_NOTE(vap, IEEE80211_MSG_IQUE, ni,
                                    "%s: IPV6 Denied\r\n.", __func__);
                    return retval;
                }

                if (ipv6->ipv6_nexthdr == 0) {
                    struct ipv6_hopopt_hdr *ipv6_ho;
                    ipv6_ho = (void *) (unsigned char *) ipv6 + sizeof(qdf_net_ipv6hdr_t);

                    if (ipv6_ho->nexthdr != QDF_NEXTHDR_ICMP) {
                        return retval;
                    }

                    /* hopbyhop payload len = (hdrlen +  1) * 8 */
                    icmpv6 = (void *)(unsigned char *) ipv6_ho + ((ipv6_ho->hdrlen + 1) << 3);
                } else if (ipv6->ipv6_nexthdr != QDF_NEXTHDR_ICMP) {
                    icmpv6 = (void *)(unsigned char *) ipv6 + sizeof(qdf_net_ipv6hdr_t);
                } else {
                    return retval;
                }

                /* Get ICMPv6 paylod now we are in process mldv1,v2) */
                switch (icmpv6->icmp6_type) {
                    case ICMPV6_MGM_QUERY: 	/* Listener Query */
                        break;
                    case ICMPV6_MGM_REPORT: 	/* Listener Report */
                    case ICMPV6_MGM_REDUCTION: 	/* Listener Done */
                        {
                            struct mld_msg *mld1 = (void *) icmpv6;
                            cmd = (icmpv6->icmp6_type ==
                                    ICMPV6_MGM_REPORT ? IGMP_SNOOP_CMD_ADD_INC_LIST :
                                    IGMP_SNOOP_CMD_ADD_EXC_LIST);
                            mld_mca = &mld1->mld_mca;
                        }
                        break;
                    case ICMPV6_MLD2_REPORT:
                        {
                            struct mld2_grec *pgr = (void *) (unsigned char *) icmpv6 + 8; //4
                            if (pgr->grec_type == MLD2_ALLOW_NEW_SOURCES) {
                                cmd = IGMP_SNOOP_CMD_ADD_INC_LIST;
                            }
                            else if (pgr->grec_type == MLD2_BLOCK_OLD_SOURCES) {
                                cmd = IGMP_SNOOP_CMD_ADD_EXC_LIST;
                            }
                            else if ((pgr->grec_type == MLD2_CHANGE_TO_INCLUDE) ||
                                    (pgr->grec_type == MLD2_MODE_IS_INCLUDE)) {
                                cmd = IGMP_SNOOP_CMD_ADD_EXC_LIST;
                            }
                            else if ((pgr->grec_type == MLD2_CHANGE_TO_EXCLUDE) ||	//4
                                    (pgr->grec_type == MLD2_MODE_IS_EXCLUDE)) {
                                cmd = IGMP_SNOOP_CMD_ADD_INC_LIST;
                            }
                            else {
                                return retval;
                            }
                            mld_mca = &pgr->grec_mca;
                        }
                        break;
                    default:
                        break;
                }

                if (cmd == IGMP_SNOOP_CMD_OTHER) {
                    return retval;
                }

                IEEE80211_ADDR_COPY(list_entry.grp_member, srcAddr);
                list_entry.vap				= vap;
                list_entry.ni 				= ni;
                list_entry.cmd = cmd;

                groupAddrL2[0] = 0x33;
                groupAddrL2[1] = 0x33;
                groupAddrL2[2] = ((unsigned char *) mld_mca)[12];
                groupAddrL2[3] = ((unsigned char *) mld_mca)[13];
                groupAddrL2[4] = ((unsigned char *) mld_mca)[14];
                groupAddrL2[5] = ((unsigned char *) mld_mca)[15];
                IEEE80211_ADDR_COPY(list_entry.grp_addr, groupAddrL2);

                dest = (int8_t *)(&list_entry.u);
                src = (int8_t *)(mld_mca);
                OS_MEMCPY(dest, src, MLD_IP_ADDR_LENGTH);

                if (ieee80211_me_ipv6_SnooplsDenied(vap, &groupAddrL2[0], src)) {
                    IEEE80211_NOTE(vap, IEEE80211_MSG_IQUE, ni,
                                    "%s: IPV6 Denied\r\n.", __func__);
                    return retval;
                }

                list_entry.src_ip_addr = 0x0;
                ieee80211_me_SnoopListUpdate(&list_entry, ETHERTYPE_IPV6);

            }
        }
    }

    /* multicast tunnel egress */
    if (ntohs(eh->ether_type) == ATH_ETH_TYPE &&
        vap->iv_opmode == IEEE80211_M_STA &&
        vap->iv_me->mc_snoop_enable)
    {
        struct athl2p_tunnel_hdr *tunHdr;

        tunHdr = (struct athl2p_tunnel_hdr *) wbuf_pull(wbuf, sizeof(struct ether_header));
        /* check protocol subtype */
        eh = (struct ether_header *) wbuf_pull(wbuf, sizeof(struct athl2p_tunnel_hdr));
    }
#endif
    return retval;
}

#if ATH_SUPPORT_ME_SNOOP_TABLE
/* count no of members with anysrc, add the member mac address in the table */
static u_int8_t
ieee80211_me_count_member_anysrclist(struct MC_GROUP_LIST_ENTRY* grp_list,
        u_int8_t* table, int table_len, u_int32_t timestamp)
{
    struct MC_GRP_MEMBER_LIST* grp_member_list;
    u_int8_t count = 0;

    TAILQ_FOREACH(grp_member_list, &grp_list->src_list,member_list){
        if(grp_member_list->src_ip_addr == 0){
            /* Do not add more than table length.
             * But count will still be updated to give full count,
             * which can be used by caller to make further decision
             * */
            if( count < table_len ) {
                IEEE80211_ADDR_COPY(&table[count * IEEE80211_ADDR_LEN],
                        grp_member_list->grp_member_addr);
            }
            grp_member_list->timestamp = timestamp;
            count++;
        }
    }
    return(count);
}

/* count no of members with matching src ip, add the member mac address in the table */
static u_int8_t
ieee80211_me_count_member_src_list(struct MC_GROUP_LIST_ENTRY* grp_list,
                                   u_int32_t src_ip_addr,
                                   u_int8_t* table, int table_len,
                                   u_int32_t timestamp)
{
    struct MC_GRP_MEMBER_LIST* grp_member_list;
    u_int8_t count = 0;
    TAILQ_FOREACH(grp_member_list, &grp_list->src_list,member_list){

        /*  if src addres matches and mode is include then add the list in table*/
        if(grp_member_list->src_ip_addr == src_ip_addr ){
            if(grp_member_list->mode == IGMP_SNOOP_CMD_ADD_INC_LIST){
                /* Do not add more than table length.
                 * But count will still be updated to give full count,
                 * which can be used by caller to make further decision
                 * */
                if( count < table_len ) {
                    IEEE80211_ADDR_COPY(&table[count * IEEE80211_ADDR_LEN],
                            grp_member_list->grp_member_addr);
                }
                grp_member_list->timestamp = timestamp;
		count++;
            }
        } else {
            /* if src address not match and mode is exclude then include in table*/
            if(grp_member_list->mode == IGMP_SNOOP_CMD_ADD_EXC_LIST){
                /* Do not add more than table length.
                 * But count will still be updated to give full count,
                 * which can be used by caller to make further decision
                 * */
                if( count < table_len ) {
                    IEEE80211_ADDR_COPY(&table[count * IEEE80211_ADDR_LEN],
                            grp_member_list->grp_member_addr);
                }
                grp_member_list->timestamp = timestamp;
		count++;
            }
        }
    }
    return(count);
}

/* Getmembers list from snoop table for current data */
static uint8_t
ieee80211_me_SnoopListGetMember(struct ieee80211vap* vap, uint8_t* grp_addr, u_int8_t *grp_ipaddr,
                                u_int32_t src_ip_addr,uint8_t* table, int table_len, u_int32_t ether_type)
{
    struct MC_SNOOP_LIST* snp_list;
    struct MC_GROUP_LIST_ENTRY* grp_list;
    uint8_t count, offset = 0;
    rwlock_state_t lock_state;

    systime_t timestamp  = OS_GET_TIMESTAMP();
    u_int32_t now        = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(timestamp);

    count = 0;
    snp_list = &(vap->iv_me->ieee80211_me_snooplist);
    IEEE80211_SNOOP_LOCK(snp_list,&lock_state);
    grp_list = ieee80211_me_find_group_list(vap,grp_addr, grp_ipaddr, ether_type);

    if(grp_list != NULL){
        count  = ieee80211_me_count_member_anysrclist(grp_list,
                                                  &table[0],table_len,now);
        table_len -= count;
        offset = count;
        if( table_len <= 0 ) {
            /* No space in table. just pass same buffer with zero as len
             * so that complete count is calculated
             */
            table_len = 0;
            offset = 0;
        }
        if (src_ip_addr != 0) {
            count += ieee80211_me_count_member_src_list(grp_list,src_ip_addr,
                                                  &table[offset],table_len,now);
        }
    }
    IEEE80211_SNOOP_UNLOCK(snp_list, &lock_state);
    return(count);

}

static void
ol_ieee80211_remove_node_grp(struct MC_GRP_MEMBER_LIST *grp_member_list,
                          struct ieee80211vap *vap)
{
    struct MC_SNOOP_LIST *snp_list;

    snp_list = &vap->iv_me->ieee80211_me_snooplist;
    atomic_inc(&snp_list->msl_group_member_limit);
}

/* this function is called only for cleaning up the snoop table */
static void
ol_ieee80211_remove_all_node_grp(struct MC_GRP_MEMBER_LIST *grp_member_list,
                              struct ieee80211vap *vap)
{
    struct MC_SNOOP_LIST *snp_list = &vap->iv_me->ieee80211_me_snooplist;

    if (!(vap->iv_ic->ic_is_mode_offload(vap->iv_ic))) {
        return;
    }
    atomic_inc(&snp_list->msl_group_list_limit);
}

/* remove particular node from the group list, used mainly for node cleanup
 * if node pointer is NULL, then remove all the node in the group, used mainly for clean group list */
static void
ieee80211_me_remove_node_grp(struct MC_GROUP_LIST_ENTRY* grp_list,struct ieee80211_node* ni)
{
    struct MC_GRP_MEMBER_LIST* curr_grp_member_list;
    struct MC_GRP_MEMBER_LIST* next_grp_member_list;

    curr_grp_member_list = TAILQ_FIRST(&grp_list->src_list);
    if ((curr_grp_member_list != NULL) && (ni == NULL)) {
        ol_ieee80211_remove_all_node_grp(curr_grp_member_list,curr_grp_member_list->vap);
    }
    while(curr_grp_member_list != NULL){
       next_grp_member_list = TAILQ_NEXT(curr_grp_member_list, member_list);
       if((curr_grp_member_list->niptr == ni) || (ni == NULL)){
           /* remove the member from list*/
           TAILQ_REMOVE(&grp_list->src_list, curr_grp_member_list, member_list);
           if (ni != NULL) {
               ol_ieee80211_remove_node_grp(curr_grp_member_list, curr_grp_member_list->vap);
           }
           OS_FREE(curr_grp_member_list);
       }
       curr_grp_member_list = next_grp_member_list;
    }
}

/* clean the snoop list, remove all group and member entries */
static void
ieee80211_me_clean_snp_list(struct ieee80211vap* vap)
{
    struct MC_SNOOP_LIST *snp_list = &(vap->iv_me->ieee80211_me_snooplist);
    struct MC_GROUP_LIST_ENTRY* curr_grp_list;
    struct MC_GROUP_LIST_ENTRY* next_grp_list;
    rwlock_state_t lock_state;

    IEEE80211_SNOOP_LOCK(snp_list,&lock_state);
    curr_grp_list = TAILQ_FIRST(&snp_list->msl_node);
    while(curr_grp_list != NULL){
        next_grp_list = TAILQ_NEXT(curr_grp_list,grp_list);
        ieee80211_me_remove_node_grp(curr_grp_list,NULL);
        TAILQ_REMOVE(&snp_list->msl_node,curr_grp_list,grp_list);
        if (vap->iv_ic->ic_is_mode_offload(vap->iv_ic)){
           atomic_inc(&snp_list->msl_group_list_limit);
        }
        OS_FREE(curr_grp_list);
        curr_grp_list = next_grp_list;
    }
    IEEE80211_SNOOP_UNLOCK(snp_list, &lock_state);
}

/* remove the node from the snoop list*/
static void
ieee80211_me_SnoopWDSNodeCleanup(struct ieee80211_node* ni)
{
    struct ieee80211vap *vap = ni->ni_vap;
    struct MC_SNOOP_LIST *snp_list = &(vap->iv_me->ieee80211_me_snooplist);
    struct MC_GROUP_LIST_ENTRY* curr_grp_list;
    struct MC_GROUP_LIST_ENTRY* next_grp_list;
    rwlock_state_t lock_state;

    if(vap->iv_opmode == IEEE80211_M_HOSTAP &&
        vap->iv_me->mc_snoop_enable &&
        snp_list != NULL)
    {
        IEEE80211_SNOOP_LOCK(snp_list,&lock_state);
        curr_grp_list = TAILQ_FIRST(&snp_list->msl_node);
        while(curr_grp_list != NULL){
            next_grp_list = TAILQ_NEXT(curr_grp_list,grp_list);
            ieee80211_me_remove_node_grp(curr_grp_list,ni);

            if(!TAILQ_FIRST(&curr_grp_list->src_list)){
                TAILQ_REMOVE(&snp_list->msl_node,curr_grp_list,grp_list);
                OS_FREE(curr_grp_list);
                atomic_inc(&snp_list->msl_group_list_limit);
            }
            curr_grp_list = next_grp_list;
        }
        IEEE80211_SNOOP_UNLOCK(snp_list, &lock_state);
    }
}
#endif

static inline bool igmp_query_check (uint8_t igmp_pkt)
{

    switch (igmp_pkt){
        case IGMP_QUERY :
        case MLD_QUERY :
        case IGMPv1_REPORT:
        case IGMPv2_REPORT:
        case IGMP_LEAVE :
        case IGMPv3_REPORT :
        case MLD_REPORT :
        case MLD_LEAVE:
        case MLDv2_REPORT :
            return true ;
        default:
            return false;
    }

}

/*******************************************************************
 * !
 * \brief Mcast enhancement option 1: Tunneling, or option 2: Translate
 *
 * Add an IEEE802.3 header to the mcast packet using node's MAC address
 * as the destination address
 *
 * \param
 *             vap Pointer to the virtual AP
 *             wbuf Pointer to the wbuf
 *
 * \return number of packets converted and transmitted, or 0 if failed
 */
#if ATH_SUPPORT_ME_SNOOP_TABLE
static int
ieee80211_me_SnoopConvert(struct ieee80211vap *vap, wbuf_t wbuf)
{
    struct ether_header *eh;
    u_int32_t src_ip_addr;
    u_int32_t grp_addr = 0;
    u_int8_t grp_ipv6_addr[MLD_IP_ADDR_LENGTH];
    u_int8_t grpmac[IEEE80211_ADDR_LEN];    /* copy of original frame group addr */
                                            /* table of tunnel group dest mac addrs */
    u_int8_t newmac[MAX_SNOOP_ENTRIES][IEEE80211_ADDR_LEN];
    uint8_t newmaccnt = 0;                        /* count of entries in newmac */
    u_int32_t type = ETHERTYPE_IP;
    uint8_t igmp_pkt = 0;

    if ( (vap->iv_me->mc_snoop_enable == 0 ) || ( wbuf == NULL ) ) {
        /*
         * if snoop is disabled return -1 to indicate
         * that the frame's not been transmitted and continue the
         * regular xmit path in wlan_vap_send
         */
        return -1;
    }

    eh = (struct ether_header *)wbuf_header(wbuf);

    src_ip_addr = 0;
    /*  Extract the source ip address from the list*/
    if (vap->iv_opmode == IEEE80211_M_HOSTAP ) {
        if (IEEE80211_IS_MULTICAST(eh->ether_dhost) &&
            !IEEE80211_IS_BROADCAST(eh->ether_dhost)){
            if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
                const struct ip_header *ip = (struct ip_header *)
                    (wbuf_header(wbuf) + sizeof (struct ether_header));
                src_ip_addr = ntohl(ip->saddr);
                grp_addr = ntohl(ip->daddr);
                type = ETHERTYPE_IP;
                /* if grp address is in denied list then don't send process it here*/
                if (ieee80211_me_SnoopIsDenied(vap, grp_addr)) {
                    return -1;
                }
            } else if (ntohs(eh->ether_type) == ETHERTYPE_IPV6) {
                const qdf_net_ipv6hdr_t *ipv6;
                ipv6 = (qdf_net_ipv6hdr_t *) (wbuf_header(wbuf) +
                        sizeof(struct ether_header));
                if (ieee80211_me_ipv6_SnooplsDenied(vap, eh->ether_dhost,
                            (u_int8_t *)&ipv6->ipv6_daddr.in6_u)) {
                    return -1;
                }
                OS_MEMCPY(grp_ipv6_addr, (int8_t *)(&ipv6->ipv6_daddr.in6_u),
                                                           MLD_IP_ADDR_LENGTH);
                type = ETHERTYPE_IPV6;
                /* do not drop any ICMP packet. from ETH */
                if (ipv6->ipv6_nexthdr == 0) {
                    struct ipv6_hopopt_hdr *ipv6_ho;
                    ipv6_ho = (void *) (unsigned char *) ipv6 + sizeof(qdf_net_ipv6hdr_t);
                    if (ipv6_ho->nexthdr == QDF_NEXTHDR_ICMP) {
                        return -1;
                    }
                } else if (ipv6->ipv6_nexthdr == QDF_NEXTHDR_ICMP) {
                    return -1;
                }
            } else {
                /*ether_type is not equal to 0x0800 OR 0x086dd,
                  instead of dropping packets sending as multicast*/
                return -1;
            }
        }
    } else {
        return -1;
    }

    /* Get members of this group */
    /* save original frame's src addr once */
    IEEE80211_ADDR_COPY(grpmac, eh->ether_dhost);

        if (type == ETHERTYPE_IP) {
            newmaccnt = ieee80211_me_SnoopListGetMember(vap, grpmac, (u_int8_t *)&grp_addr, src_ip_addr,
                    newmac[0], MAX_SNOOP_ENTRIES, type);
        } else if (type == ETHERTYPE_IPV6) {
            newmaccnt = ieee80211_me_SnoopListGetMember(vap, grpmac, &grp_ipv6_addr[0], src_ip_addr,
                    newmac[0], MAX_SNOOP_ENTRIES, type);
        }
    /* save original frame's src addr once */
    /*
     * If newmaccnt is 0: no member intrested in this group
     *                 1: mc in table, only one dest, skb cloning not needed
     *                >1: multiple mc in table, skb cloned for each entry past
     *                    first one.
     */

    /* Maitain the original wbuf avoid being modified.
     */
    if (newmaccnt > 0 && vap->iv_me->mc_mcast_enable == 0)
    {
        /* We have members intrested in this group and no enhancement is required, send true multicast */
        return -1;
    } else if(newmaccnt == 0) {
        if ( ieee80211_me_igmp_mld_inspect(vap, wbuf, &igmp_pkt, NULL, NULL))
            if(igmp_query_check(igmp_pkt)){
                return -1;
            }
        /* no members is intrested, no need to send, just drop it */
        wbuf_complete(wbuf);
        return 1;
    } else if(newmaccnt > MAX_SNOOP_ENTRIES) {
        /* Number of entries is more than supported, send true multicast */
        return -1;
    }
    if (!(vap->iv_ic->ic_is_mode_offload(vap->iv_ic))) {
		return wlan_me_Convert(vap->vdev_obj, wbuf, newmac, newmaccnt);
    }

    return vap->iv_ic->ic_me_convert(vap, wbuf, newmac, newmaccnt);
}

/**************************************************************************
 * !
 * \brief Initialize the mc snooping and encapsulation feature.
 *
 * \param vap
 *
 * \return N/A
 */
static void ieee80211_me_SnoopListInit(struct ieee80211vap *vap)
{
    atomic_set(&vap->iv_me->ieee80211_me_snooplist.msl_group_list_limit,
                vap->iv_me->ieee80211_me_snooplist.msl_list_max_length+1);
    atomic_set(&vap->iv_me->ieee80211_me_snooplist.msl_group_member_limit,
                vap->iv_me->ieee80211_me_snooplist.msl_max_length+1);

    vap->iv_me->ieee80211_me_snooplist.msl_group_list_count = 0;
    vap->iv_me->ieee80211_me_snooplist.msl_misc = 0;
    IEEE80211_SNOOP_LOCK_INIT(&vap->iv_me->ieee80211_me_snooplist, "Snoop Table");
    TAILQ_INIT(&vap->iv_me->ieee80211_me_snooplist.msl_node);
}

#endif
/********************************************************************
 * !
 * \brief Detach the resources for multicast enhancement
 *
 * \param sc Pointer to ATH object (this)
 *
 * \return N/A
 */

static void
ieee80211_me_detach(struct ieee80211vap *vap)
{
    if(vap->iv_me) {
#if ATH_SUPPORT_ME_SNOOP_TABLE
        ieee80211_me_clean_snp_list(vap);
        IEEE80211_ME_LOCK_DESTROY(vap);
#endif
        OS_FREE(vap->iv_me);
        vap->iv_me = NULL;
    }
}

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
static struct ieee80211_me_hifi_entry *ieee80211_me_hifi_find_entry(
        struct ieee80211_me_hifi_group *group,
        int *group_ix,
        struct ieee80211_me_hifi_table *table)
{
    int cnt;

    if (!group || !table)
        return NULL;

    for (cnt = 0; cnt < table->entry_cnt; cnt++) {
        if (!OS_MEMCMP(&table->entry[cnt].group, group, sizeof(*group)))
            break;
    }
    if (cnt == table->entry_cnt)
        return NULL;

    *group_ix = cnt;
    return &table->entry[cnt];
}

int ieee80211_me_hifi_filter(struct ieee80211_me_hifi_node *node, const void *ip_header, u_int16_t pro)
{
    int i;

    if (!node->filter_mode || (node->filter_mode == IEEE80211_ME_HIFI_EXCLUDE && !node->nsrcs))
        return 0;

    if (node->filter_mode == IEEE80211_ME_HIFI_INCLUDE && !node->nsrcs)
        return 1;

    if (pro == htobe16(ETHERTYPE_IP)) {
        u_int32_t ip4 = ((struct ip_header *)ip_header)->saddr;
        const u_int32_t *srcs = (u_int32_t *)node->srcs;
        for (i = 0; i < node->nsrcs; i++) {
            if (srcs[i] == ip4)
                break;
        }
    } else if (pro == htobe16(ETHERTYPE_IPV6)) {
        qdf_net_ipv6_addr_t *ip6 = &((qdf_net_ipv6hdr_t *)ip_header)->ipv6_saddr;
        qdf_net_ipv6_addr_t *srcs = (qdf_net_ipv6_addr_t *)node->srcs;
        for (i = 0; i < node->nsrcs; i++) {
            if (!OS_MEMCMP(&srcs[i], ip6, sizeof(*srcs)))
                break;
        }
    } else {
        return 0;
    }

    return ((node->filter_mode == IEEE80211_ME_HIFI_INCLUDE && i == node->nsrcs) ||
        (node->filter_mode == IEEE80211_ME_HIFI_EXCLUDE && i != node->nsrcs));
}


static int ieee80211_me_hifi_hmmc_convert(struct ieee80211vap *vap, wbuf_t wbuf)
{
#define ME_HIFI_NI_CNT_MAX MAX_SNOOP_ENTRIES
    struct ieee80211_node_table *nt = &vap->iv_ic->ic_sta;
    rwlock_state_t lock_state;
    OS_BEACON_DECLARE_AND_RESET_VAR(flags);
    struct ieee80211_node *ni, *prev = NULL;
    u_int32_t i, ni_cnt = 0;
    struct ieee80211_node *ni_buf[ME_HIFI_NI_CNT_MAX];
    struct ether_header *eh = (struct ether_header *) wbuf_header(wbuf);
    u_int8_t newmac[MAX_SNOOP_ENTRIES][IEEE80211_ADDR_LEN];
    int new_mac_cnt = 0;                        /* count of entries in newmac */

    if ((ni = ieee80211_find_txnode(vap, eh->ether_shost)) &&
            ni->ni_associd && ni != vap->iv_bss) {
        ieee80211_free_node(ni);
        return -1;
    }

    if (ni)
        ieee80211_free_node(ni);

    /* Convert the packet to uniast to all STA in node table. */
    OS_BEACON_READ_LOCK(&nt->nt_nodelock, &lock_state, flags);
    TAILQ_FOREACH(ni, &nt->nt_node, ni_list) {
        if (!ni || ni->ni_vap != vap ||
                !ni->ni_associd || ni == vap->iv_bss)
            continue;

        if (vap->iv_ic->ic_is_mode_offload(vap->iv_ic)) {
            if (new_mac_cnt < MAX_SNOOP_ENTRIES) {
                IEEE80211_ADDR_COPY(newmac[new_mac_cnt], ni->ni_macaddr);
                new_mac_cnt++;
            }
        } else {
            if (!ieee80211_try_ref_node(ni))	/* mark referenced */
                ni_buf[ni_cnt++] = ni;
            if (ni_cnt >= ME_HIFI_NI_CNT_MAX) {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IQUE,
                                  "WARNING: too many nodes in %s:%d\n", __func__, __LINE__);
                break;
            }
        }
    }
    OS_BEACON_READ_UNLOCK(&nt->nt_nodelock, &lock_state, flags);

    if (!(vap->iv_ic->ic_is_mode_offload(vap->iv_ic))) {
        for (i = 0; i < ni_cnt; i++) {
            if (prev != NULL) {
                wbuf_t wbuf2 = wbuf_copy(wbuf);

                if (!wbuf2) {
                    /* Release reference */
                    while (i < ni_cnt)
                        ieee80211_free_node(ni_buf[i++]);
                    break;
                }

                wbuf_clear_flags(wbuf2);

                wlan_me_hifi_forward(vap->vdev_obj, wbuf2, prev->peer_obj);

            }
            prev = ni_buf[i];
        }
        if (prev) {
            wbuf_clear_flags(wbuf);

            wlan_me_hifi_forward(vap->vdev_obj, wbuf, prev->peer_obj);
        } else {
            wbuf_complete(wbuf);
        }
    } else {
        return vap->iv_ic->ic_me_convert(vap, wbuf, newmac, new_mac_cnt);
    }
    return 0;
}

int ieee80211_me_hifi_convert(struct ieee80211vap *vap, wbuf_t wbuf)
{
    int n;
    u_int8_t hmmc_found = 0;
    rwlock_state_t lock_state;
    struct ether_header *eh = NULL;
    u_int8_t zero_mac[IEEE80211_ADDR_LEN];
    struct ieee80211_node *ni = NULL, *prev = NULL;
    struct ieee80211_me_hifi_group group;
    struct ieee80211_me_hifi_entry *entry = NULL;
    struct ieee80211_me_hifi_table *table;
    u_int8_t newmac[MAX_SNOOP_ENTRIES][IEEE80211_ADDR_LEN];
    int new_mac_cnt = 0;
    uint8_t igmp_pkt = 0;
    int group_ix = 0; /* Multicast group index */

    if ((wlan_vdev_is_up(vap->vdev_obj) != QDF_STATUS_SUCCESS) ||
        !vap->iv_me->me_hifi_enable)
        return -1;

    if (ieee80211_me_igmp_mld_inspect(vap, wbuf, NULL, &group, &hmmc_found))
        return -1;

    if (hmmc_found) {
        return ieee80211_me_hifi_hmmc_convert(vap, wbuf);
    }

    OS_RWLOCK_READ_LOCK(&vap->iv_me->me_hifi_lock, &lock_state);
    table = &vap->iv_me->me_hifi_table;
    if (!table->entry_cnt ||
            !(entry = ieee80211_me_hifi_find_entry(&group, &group_ix, table)) ||
            !entry->node_cnt ) {
        OS_RWLOCK_READ_UNLOCK(&vap->iv_me->me_hifi_lock, &lock_state);
        /*If there are no one in group. drop the frame*/
        wbuf_complete(wbuf);
        return 1;
    }

    eh = (struct ether_header *) wbuf_header(wbuf);
    switch (ntohs(eh->ether_type)) {
        case ETHERTYPE_IP:
            {
            struct ip_header *iph = (struct ip_header *)(eh + 1);

            if (iph->protocol == IPPROTO_IGMP)
                return -1;

            OS_MEMSET(&group, 0, sizeof group);
            group.u.ip4 = ntohl(iph->daddr);
            group.pro = ETHERTYPE_IP;
            }
            break;
        case ETHERTYPE_IPV6:
            {
            qdf_net_ipv6hdr_t *ip6h = (qdf_net_ipv6hdr_t *)(eh + 1);
            u_int8_t *nexthdr = (u_int8_t *)(ip6h + 1);

            if (ip6h->ipv6_nexthdr == IPPROTO_ICMPV6 ||
                    (ip6h->ipv6_nexthdr == IPPROTO_HOPOPTS &&
                     *nexthdr == IPPROTO_ICMPV6))
                return -1;

            OS_MEMSET(&group, 0, sizeof group);
            OS_MEMCPY(group.u.ip6,
                    ip6h->ipv6_daddr.s6_addr,
                    sizeof(qdf_net_ipv6_addr_t));
            group.pro = ETHERTYPE_IPV6;
            }
            break;
        default:
            return -1;
    }


    OS_MEMSET(zero_mac, 0, IEEE80211_ADDR_LEN);
    for (n = 0; n < entry->node_cnt; n++) {
        if (IEEE80211_ADDR_EQ(eh->ether_shost, entry->nodes[n].mac) ||
                IEEE80211_ADDR_EQ(zero_mac, entry->nodes[n].mac) ||
                ieee80211_me_hifi_filter(&entry->nodes[n], eh + 1, ntohs(eh->ether_type))) {
            continue;
        }

        if(vap->iv_ic->ic_is_mode_offload(vap->iv_ic)) {
            if(new_mac_cnt < MAX_SNOOP_ENTRIES) {
#ifdef QCA_OL_DMS_WAR
                if (!vap->iv_me_amsdu)
                    IEEE80211_ADDR_COPY(newmac[new_mac_cnt], entry->nodes[n].mac);
                else
#endif
                {
                    if (!vap->iv_me->me_ra[group_ix][n].dup) {
                        IEEE80211_ADDR_COPY(newmac[new_mac_cnt], vap->iv_me->me_ra[group_ix][n].mac);
                    } else {
                        /* Peer is dropped since frame is being sent to a common next-hop node */
                        continue;
                    }
                }
                new_mac_cnt++;
            } else {
                IEEE80211_DPRINTF(vap, IEEE80211_MSG_IQUE,
                        "WARNING: too many nodes in %s:%d\n", __func__, __LINE__);
                break;
            }
        } else {
            if(!(ni = ieee80211_find_txnode(vap, entry->nodes[n].mac))) {
                continue;
            }
            if (!ni->ni_associd || ni == vap->iv_bss) {
                ieee80211_free_node(ni);
                continue;
            }

            if (prev != NULL) {
                wbuf_t wbuf2 = wbuf_copy(wbuf);

                if (!wbuf2) break;

                wbuf_clear_flags(wbuf2);
                wlan_me_hifi_forward(vap->vdev_obj, wbuf2, prev->peer_obj);
            }
            prev = ni;
        }
    }
    OS_RWLOCK_READ_UNLOCK(&vap->iv_me->me_hifi_lock, &lock_state);

    if(vap->iv_ic->ic_is_mode_offload(vap->iv_ic)) {
        if(new_mac_cnt) {
            return vap->iv_ic->ic_me_convert(vap, wbuf, newmac, new_mac_cnt);
        } else if(new_mac_cnt == 0){
            if ( ieee80211_me_igmp_mld_inspect(vap, wbuf, &igmp_pkt, NULL, NULL))
                if(igmp_query_check(igmp_pkt)){
                    return -1;
                }
            /*If there are no one in group. drop the frame*/
            wbuf_complete(wbuf);
            return 1;
        }
    } else {
        if (prev) {
            wbuf_clear_flags(wbuf);
            wlan_me_hifi_forward(vap->vdev_obj, wbuf, prev->peer_obj);
            return 0;
        }
    }

    return -1;
}
#endif

static inline int _ieee80211_me_hmmc_find(struct ieee80211vap *vap, u_int32_t dip)
{
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    int i;
    struct ieee80211com *ic = vap->iv_ic;

    for (i = 0; i < ic->ic_hmmc_cnt; i++) {
        if (ic->ic_hmmcs[i].ip == (dip & ic->ic_hmmcs[i].mask))
            return 1;
    }
#endif
    return 0;
}

int ieee80211_me_hmmc_find(struct ieee80211vap *vap, u_int32_t dip)
{
    return _ieee80211_me_hmmc_find(vap, dip);
}

/*******************************************************************
 * !
 * \brief Inspect IGMP and MLD packets to get type and multicast group.
 *
 * \input param  vap
 *               wbuf
 * \output param type - the type of IGMP or MLD
 *               group - multicast group
 *
 * \return non-zero indicator IGMP or MLD packets.
 */
static int ieee80211_me_igmp_mld_inspect(struct ieee80211vap *vap, wbuf_t wbuf,
        u_int8_t *type, void *group, u_int8_t *hmmc_found)
{
    u_int16_t pro;
    u_int32_t ip4 = 0;
    u_int8_t *ip6 = NULL;
    struct ether_header *eh = (struct ether_header *) wbuf_header(wbuf);
    int is_multicast = IEEE80211_IS_MULTICAST(eh->ether_dhost) &&
        !IEEE80211_IS_BROADCAST(eh->ether_dhost);
    int ret = 0;

    switch (eh->ether_type) {
        case htobe16(ETHERTYPE_IP):
            {
            struct ip_header *iph = (struct ip_header *)(eh + 1);
            int ip_headerlen;
            const struct igmp_header *igmp;

            ip4 = iph->saddr;
            pro = ETHERTYPE_IP;

            if (iph->protocol == IPPROTO_IGMP) {
                if (!type) return 1;

                ip_headerlen = iph->version_ihl & 0x0F;

                igmp = (struct igmp_header *)(wbuf_header(wbuf) +
                        sizeof (struct ether_header) + (4 * ip_headerlen));
                *type = igmp->type;
                ret = 1;
                break;
            }
            if (is_multicast && group) {
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
                struct ieee80211_me_hifi_group *grp = (struct ieee80211_me_hifi_group *)group;
                OS_MEMSET(grp, 0, sizeof(*grp));
                grp->u.ip4 = iph->daddr;
                grp->pro = htobe16(ETHERTYPE_IP);
                if (hmmc_found)
                    *hmmc_found = _ieee80211_me_hmmc_find(vap, iph->daddr);
#endif
            }
            }
            break;
        case htobe16(ETHERTYPE_IPV6):
            {
            qdf_net_ipv6hdr_t *ip6h = (qdf_net_ipv6hdr_t *)(eh + 1);
            u_int8_t *nexthdr = (u_int8_t *)(ip6h + 1);
            qdf_net_icmpv6hdr_t *mld;

            ip6 = ip6h->ipv6_saddr.s6_addr;
            pro = ETHERTYPE_IPV6;

            if (ip6h->ipv6_nexthdr == IPPROTO_ICMPV6 ||
                    (ip6h->ipv6_nexthdr == IPPROTO_HOPOPTS &&
                     *nexthdr == IPPROTO_ICMPV6)) {
                if (!type) return 1;

                if (ip6h->ipv6_nexthdr == IPPROTO_ICMPV6)
                    mld = (qdf_net_icmpv6hdr_t *)nexthdr;
                else
                    mld = (qdf_net_icmpv6hdr_t *)(nexthdr + 8);
                *type = mld->icmp6_type;
                ret = 1;
                break;
            }
            if (is_multicast && group) {
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
                struct ieee80211_me_hifi_group *grp = (struct ieee80211_me_hifi_group *)group;
                OS_MEMSET(grp, 0, sizeof(*grp));
                OS_MEMCPY(grp->u.ip6,
                        ip6h->ipv6_daddr.s6_addr,
                        sizeof(qdf_net_ipv6_addr_t));
                grp->pro = htobe16(ETHERTYPE_IPV6);
#endif
            }
            }
            break;
        default:
            return 1;
    }
    return ret;
}

/*******************************************************************
 * !
 * \brief Attach the ath_me module for multicast enhancement.
 *
 * This function allocates the ath_me structure and attachs function
 * entry points to the function table of ath_softc.
 *
 * \param  vap
 *
 * \return pointer to the mcastenhance op table (vap->iv_me_ops).
 */
int
ieee80211_me_attach(struct ieee80211vap * vap)
{
#if ATH_SUPPORT_ME_SNOOP_TABLE
    struct ipv6_mcast_deny *ipv6_mcast_deny_list;
    char *deny[] = {
                    "FF02:0000:0000:0000:0000:0001:FF00:0000",	//V6_SOLICITED_NODE_ADDRESS
                    "FF02:0000:0000:0000:0000:0002:FF00:0000",	//V6_NODE_INFORMATION_QUERIES
                    "FF02:0000:0000:0000:0000:0000:0000:0001", 	//V6_ALL_NODE_ADDRESS
                    "FF02:0000:0000:0000:0000:0000:0000:0002", 	//V6_ALL_ROUTERS_ADDRESS
                    "FF02:0000:0000:0000:0000:0000:0000:0009", 	//V6_RIP_ROUTERS
                    "FF02:0000:0000:0000:0000:0000:0000:000C", 	//V6_SSDP_LINK_LOCAL
                    "FF05:0000:0000:0000:0000:0000:0000:000C", 	//V6_SSDP_SITE_LOCAL
                    "FF02:0000:0000:0000:0000:0000:0000:00FB", 	//V6_MDNS
                    "FF02:0000:0000:0000:0000:0000:0001:0002", 	//V6_ALL_DHCP_AGENTS
                    "FF02:0000:0000:0000:0000:0000:0001:0003", 	//V6_LLMNR
    //              "FF02:0000:0000:0000:0000:0000:0000:0016", 	//V6_ALL_MLDV2_CAPABLE_ROUTERS

                    0 /*Should be NULL terminated */
                    }, **p = deny;
    unsigned char *m;
    int i = 0;
#endif
    struct ieee80211_ique_me *ame;

    ame = (struct ieee80211_ique_me *)OS_MALLOC(
                    vap-> iv_ic->ic_osdev,
                    sizeof(struct ieee80211_ique_me), GFP_KERNEL);

    if (ame == NULL)
            return -ENOMEM;

    OS_MEMZERO(ame, sizeof(struct ieee80211_ique_me));

    /*Attach function entry points*/
    vap->iv_me = ame;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    vap->iv_ique_ops.me_hifi_convert = ieee80211_me_hifi_convert;
#endif
    vap->iv_ique_ops.me_hmmc_find = ieee80211_me_hmmc_find;
    vap->iv_ique_ops.me_inspect = ieee80211_me_SnoopInspecting;
    vap->iv_ique_ops.me_detach = ieee80211_me_detach;
#if ATH_SUPPORT_ME_SNOOP_TABLE
    vap->iv_ique_ops.me_convert = ieee80211_me_SnoopConvert;
    vap->iv_ique_ops.me_dump = ieee80211_me_SnoopListDump;
    vap->iv_ique_ops.me_clean = ieee80211_me_SnoopWDSNodeCleanup;
    vap->iv_ique_ops.me_showdeny = ieee80211_me_SnoopShowDenyTable;
    vap->iv_ique_ops.me_adddeny = ieee80211_me_SnoopAddDenyEntry;
    vap->iv_ique_ops.me_cleardeny = ieee80211_me_SnoopClearDenyTable;
    vap->iv_ique_ops.me_deletegrp = ieee80211_me_SnoopDeleteGrp;

    ame->me_iv = vap;
    ame->ieee80211_me_snooplist.msl_max_length = MAX_SNOOP_ENTRIES;
    ame->ieee80211_me_snooplist.msl_list_max_length = DEF_SNOOP_ENTRIES;

    ame->me_timeout = DEF_ME_TIMEOUT;
    ame->mc_discard_mcast = 1;
    ame->ieee80211_me_snooplist.msl_deny_count = 4;

    ame->ieee80211_me_snooplist.msl_deny_group[0] = 3758096385UL; /*224.0.0.1 */
    ame->ieee80211_me_snooplist.msl_deny_mask[0] = 4294967040UL;   /* 255.255.255.0*/

    ame->ieee80211_me_snooplist.msl_deny_group[1] = 4026531585UL;     /*239.255.255.1*/
    ame->ieee80211_me_snooplist.msl_deny_mask[1] = 4294967040UL;   /* 255.255.255.0*/

    /* IP address 224.0.0.22 should not filtered as IGMPv3*/
    ame->ieee80211_me_snooplist.msl_deny_group[2] = 3758096406UL;  /*224.0.0.22*/
    ame->ieee80211_me_snooplist.msl_deny_mask[2] = 4294967040UL;   /* 255.255.255.0*/

    /* Do not filter 224.0.0.251 (MDNS) */
    ame->ieee80211_me_snooplist.msl_deny_group[3] = 3758096635UL;  /*224.0.0.251*/
    ame->ieee80211_me_snooplist.msl_deny_mask[3] = 4294967040UL;   /* 255.255.255.0*/
    ipv6_mcast_deny_list = &(ame->ieee80211_me_snooplist.msl_ipv6_deny_group[0]);

    for (;*p != NULL; p++, i++) {
        //inet_pton(AF_INET6, *p, &ipv6_mcast_deny_list[i].dst);
        in6_pton(*p, strlen(*p), (void *)&ipv6_mcast_deny_list[i].dst, '\\', NULL);
        m = ipv6_mcast_deny_list[i].mac;
        m[0] = 0x33;
        m[1] = 0x33;
        m[2] = ((unsigned char *)&ipv6_mcast_deny_list[i].dst)[12];
        m[3] = ((unsigned char *)&ipv6_mcast_deny_list[i].dst)[13];
        m[4] = ((unsigned char *)&ipv6_mcast_deny_list[i].dst)[14];
        m[5] = ((unsigned char *)&ipv6_mcast_deny_list[i].dst)[15];
    }
    ame->ieee80211_me_snooplist.msl_ipv6_deny_list_length = i;
    IEEE80211_ME_LOCK_INIT(vap);
    ieee80211_me_SnoopListInit(vap);
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    ame->me_hifi_enable = 0;
    OS_RWLOCK_INIT(&ame->me_hifi_lock);
#endif
    return 0;
}

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
int ieee80211_add_hmmc(struct ieee80211vap *vap, u_int32_t ip, u_int32_t mask)
{
    int i;
    struct ieee80211com *ic = vap->iv_ic;
    int action = IGMP_ACTION_ADD_MEMBER, wildcard = IGMP_WILDCARD_SINGLE;

    if (!ic || !ip || !mask || IEEE80211_ADD_HMMC_CHECK_IP(ip))
        return -EINVAL;

    for (i = 0; i < ic->ic_hmmc_cnt; i++) {
        if (ic->ic_hmmcs[i].ip == ip)
            break;
    }
    if (i != ic->ic_hmmc_cnt) {
        ic->ic_hmmcs[i].ip = ip;
        ic->ic_hmmcs[i].mask = mask;
        return 0;
    }
    if (ic->ic_hmmc_cnt < ATH_HMMC_CNT_MAX) {
        ic->ic_hmmcs[ic->ic_hmmc_cnt].ip = ip;
        ic->ic_hmmcs[ic->ic_hmmc_cnt].mask = mask;
        if(ic->ic_is_mode_offload(ic))
            ic->ic_mcast_group_update(ic, action, wildcard, (u_int8_t *)&ic->ic_hmmcs[ic->ic_hmmc_cnt].ip,
                                      IGMP_IP_ADDR_LENGTH, NULL, 0, 0, NULL, (u_int8_t *)&ic->ic_hmmcs[ic->ic_hmmc_cnt].mask, vap->iv_unit);
        ic->ic_hmmc_cnt++;
        return 0;
    }
    return -1;
}

int ieee80211_del_hmmc(struct ieee80211vap *vap, u_int32_t ip, u_int32_t mask)
{
    struct ieee80211com *ic = vap->iv_ic;
    int i, hmmc_size = sizeof(ic->ic_hmmcs) / ATH_HMMC_CNT_MAX;
    int action = IGMP_ACTION_DELETE_MEMBER, wildcard = IGMP_WILDCARD_ALL;

    if (!ic || !ip || !mask)
        return -EINVAL;

    if (!ic->ic_hmmc_cnt)
        return 0;

    for (i = 0; i < ic->ic_hmmc_cnt; i++) {
        if (ic->ic_hmmcs[i].ip == ip &&
                ic->ic_hmmcs[i].mask == mask)
            break;
    }

    if (i == ic->ic_hmmc_cnt)
        return -EINVAL;

    if(ic->ic_is_mode_offload(ic))
        ic->ic_mcast_group_update(ic, action, wildcard, (u_int8_t *)&ip,
                                      IGMP_IP_ADDR_LENGTH, NULL, 0, 0, NULL, NULL, vap->iv_unit);
    OS_MEMCPY(&ic->ic_hmmcs[i], &ic->ic_hmmcs[i+1],
            (ic->ic_hmmc_cnt - i - 1) * hmmc_size );
    ic->ic_hmmc_cnt--;

    return 0;
}
#endif

#endif /* ATH_SUPPORT_IQUE */

