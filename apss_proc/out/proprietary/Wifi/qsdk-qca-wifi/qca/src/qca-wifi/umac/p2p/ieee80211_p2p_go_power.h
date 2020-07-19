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

#ifndef _IEEE80211_P2P_GO_POWER_H
#define _IEEE80211_P2P_GO_POWER_H

#include "ieee80211P2P_api.h"
#include "ieee80211_p2p_go_schedule.h"

struct ieee80211_p2p_go_ps;
typedef struct ieee80211_p2p_go_ps *ieee80211_p2p_go_ps_t;

#if UMAC_SUPPORT_P2P_GO_POWERSAVE
typedef enum _ieee80211_p2p_go_ps_param_type {
    P2P_GO_PS_CTWIN = 1,        /* CT Window */
    P2P_GO_PS_OPP_PS,           /* Opportunistic Power Save */
} ieee80211_p2p_go_ps_param_type;

ieee80211_p2p_go_ps_t ieee80211_p2p_go_power_vattach(osdev_t os_handle, wlan_p2p_go_t p2p_go_handle);
void ieee80211_p2p_go_power_vdettach(ieee80211_p2p_go_ps_t p2p_go_ps);

int wlan_p2p_GO_ps_set_noa_schedule(
    ieee80211_p2p_go_ps_t           p2p_go_ps,
    u_int8_t                        num_schedules,
    ieee80211_p2p_go_schedule_req   *request);

int ieee80211_p2p_go_power_set_param(
    ieee80211_p2p_go_ps_t           p2p_go_ps,
    ieee80211_p2p_go_ps_param_type  type,
    u_int32_t                       param);

#else

#define ieee80211_p2p_go_power_vattach(os_handle,p2p_go_handle)  NULL
#define ieee80211_p2p_go_power_vdettach(p2p_go_ps)  /**/
#define wlan_p2p_GO_ps_set_noa_schedule(p2p_go_ps, num_schedules, request) -EINVAL
#define ieee80211_p2p_go_power_set_param(p2p_go_ps, type, param)     -EINVAL

#endif

#endif  //_IEEE80211_P2P_GO_POWER_H

