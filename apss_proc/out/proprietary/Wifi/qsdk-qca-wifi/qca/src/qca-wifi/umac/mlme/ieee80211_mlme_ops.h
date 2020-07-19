/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
#ifndef _IEEE80211_MLME_OPS_H
#define _IEEE80211_MLME_OPS_H

#define IEEE80211_RESMGR_MAX_LEAVE_BSS_DELAY           20
u_int8_t mlme_unified_stats_status(struct wlan_objmgr_pdev *pdev);
void wlan_register_mlme_ops(struct wlan_objmgr_psoc *psoc);


#endif /* end of _IEEE80211_MLME_PRIV_H */
