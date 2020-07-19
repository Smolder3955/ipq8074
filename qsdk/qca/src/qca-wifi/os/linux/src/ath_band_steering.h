/*
 * Copyright (c) 2014,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#include <osdep.h>
#include "osif_private.h"

#ifndef _ATH_SUPPORT_SON__
#define _ATH_SUPPORT_SON__

#if QCA_SUPPORT_SON

#define QCA_BAND_STEERING_GENERIC_EVENTS 1

struct band_steering_netlink {
    struct sock      *bsteer_sock;
    atomic_t         bsteer_refcnt;
};

int ath_band_steering_netlink_init(void);
int ath_band_steering_netlink_delete(void);
void ath_band_steering_netlink_send(ath_netlink_bsteering_event_t *event, u_int32_t pid, bool bcast, size_t event_size);
#else
#define ath_band_steering_netlink_init() do {} while (0)
#define ath_band_steering_netlink_delete() do {} while (0)
#define ath_band_steering_netlink_send(ev,pid,bcast,ev_size) do {} while (0)
#endif

#endif /* QCA_SUPPORT_SON */
