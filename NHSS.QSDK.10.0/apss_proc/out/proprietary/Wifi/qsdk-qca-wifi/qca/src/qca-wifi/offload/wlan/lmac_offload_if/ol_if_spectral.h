/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary . Qualcomm Innovation Center, Inc.
 */
#ifndef	__OL_IF_SPECTRAL_H__
#define	__OL_IF_SPECTRAL_H__

#define PHY_ERROR_SPECTRAL_SCAN         0x26
#define PHY_ERROR_FALSE_RADAR_EXT       0x24

#if WLAN_SPECTRAL_ENABLE
struct ath_spectral; 
extern int ol_if_spectral_setup(struct ieee80211com *ic);
extern int ol_if_spectral_detach(struct ieee80211com *ic);
extern void spectral_send_intf_found_msg(struct ath_spectral* spectral,
                                         u_int16_t cw_int,
                                         u_int32_t dfs_enabled);
#endif

#endif	/* __OL_IF_SPECTRAL_H__ */
