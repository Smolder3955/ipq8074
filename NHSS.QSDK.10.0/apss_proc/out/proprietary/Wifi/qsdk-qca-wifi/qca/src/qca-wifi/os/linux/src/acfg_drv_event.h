/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
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
#ifndef __ACFG_DRV_EVENT_H
#define __ACFG_DRV_EVENT_H

#include <acfg_api_types.h>

uint32_t
acfg_net_indicate_event(struct net_device *dev, osdev_t osdev, void *event,
                    u_int32_t event_type, int send_iwevent, u_int32_t pid);

uint32_t
acfg_event_netlink_init(void);

void acfg_event_netlink_delete(void);

enum {
   ACFG_NETLINK_MSG_RESPONSE = 0x1,
   ACFG_NETLINK_SENT_EVENT   = 0x2,
};

#define ACFG_EVENT_LIST_MAX_LEN (1024)
#define ACFG_EVENT_PRINT_LIMIT (250)

/*Per-radio events to user space */
/* Radar detected event */
void osif_radio_evt_radar_detected_ap(void *event_arg, struct ieee80211com *ic);
/* Watch dog event */
void osif_radio_evt_watch_dog(void *event_arg, struct ieee80211com *ic, u_int32_t reason);
/* DFS CAC start*/
void osif_radio_evt_dfs_cac_start(void *event_arg, struct ieee80211com *ic);
/* Up after CAC*/
void osif_radio_evt_dfs_up_after_cac(void *event_arg, struct ieee80211com *ic);

/* channel utilisation percentage*/
void osif_radio_evt_chan_util(struct ieee80211com *ic, u_int8_t self_util, u_int8_t obss_util);

/* per-VAP assoc failure event */
void acfg_assoc_failure_indication_event(struct ieee80211vap *vap, u_int8_t *macaddr, u_int16_t reason);

void acfg_chan_stats_event(struct ieee80211com *ic, u_int8_t self_util, u_int8_t obss_util);

#endif /* __ACFG_DRV_EVENT_H */
