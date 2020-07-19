/*
 *  Copyright (c) 2014 Qualcomm Atheros, Inc.  All rights reserved. 
 *
 *  Qualcomm is a trademark of Qualcomm Technologies Incorporated, registered in the United
 *  States and other countries.  All Qualcomm Technologies Incorporated trademarks are used with
 *  permission.  Atheros is a trademark of Qualcomm Atheros, Inc., registered in
 *  the United States and other countries.  Other products and brand names may be
 *  trademarks or registered trademarks of their respective owners. 
 */
#ifndef _ALD_NETLINK_H_
#define _ALD_NETLINK_H_

#include <osdep.h>
#include "osif_private.h"
#include "ath_ald_external.h"



struct ald_netlink {
    struct sock             *ald_sock;
    struct sk_buff          *ald_skb;
    struct nlmsghdr         *ald_nlh;
    atomic_t                ald_refcnt;
};

extern struct net init_net;

#if ATH_SUPPORT_HYFI_ENHANCEMENTS

#define QCA_ALD_GENERIC_EVENTS 1

int ald_init_netlink(void);
int ald_destroy_netlink(void);
int ald_assoc_notify( wlan_if_t vap, u_int8_t *macaddr, u_int8_t aflag );
int ald_buffull_notify(wlan_if_t vap);
int ieee80211_ioctl_ald_getStatistics(struct net_device *dev, void *info, void *w, char *extra);
void ieee80211_buffull_handler(struct ieee80211com *ic);

#else /* ATH_SUPPORT_HYFI_ENHANCEMENTS */

#define ald_init_netlink()   do{}while(0)
#define ald_destroy_netlink()    do{}while(0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION (2,6,24)
#define ald_nl_receive(a)    do{}while(0)
#else
#define ald_nl_receive(a, b) do{}while(0)
#endif
#define ald_assoc_notify(a, b, c)    do{}while(0)
#define ald_buffull_notify(a)    do{}while(0)
#define ieee80211_ioctl_ald_getStatistics(a, b, c, d)   do{}while(0)

#endif /* ATH_SUPPORT_HYFI_ENHANCEMENTS */
#endif /* _ALD_NETLINK_H_ */
