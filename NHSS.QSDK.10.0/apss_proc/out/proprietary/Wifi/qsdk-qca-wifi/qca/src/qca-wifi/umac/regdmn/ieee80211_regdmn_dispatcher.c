/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_reg_ucfg_api.h>
#include <wlan_reg_services_api.h>

int ieee80211_regdmn_program_cc(struct wlan_objmgr_pdev *pdev,
		struct cc_regdmn_s *rd)
{
	return ucfg_reg_program_cc(pdev, rd);
}

QDF_STATUS ieee80211_regdmn_get_chip_mode(struct wlan_objmgr_pdev *pdev,
		uint32_t *chip_mode)
{
	return wlan_reg_get_chip_mode(pdev, chip_mode);
}

QDF_STATUS ieee80211_regdmn_get_freq_range(struct wlan_objmgr_pdev *pdev,
		uint32_t *low_2g,
		uint32_t *high_2g,
		uint32_t *low_5g,
		uint32_t *high_5g)
{
	return wlan_reg_get_freq_range(pdev, low_2g, high_2g, low_5g, high_5g);
}

QDF_STATUS ieee80211_regdmn_get_current_chan_list(struct wlan_objmgr_pdev *pdev,
		struct regulatory_channel *chan_list)
{
	return ucfg_reg_get_current_chan_list(pdev, chan_list);
}

QDF_STATUS ieee80211_regdmn_get_current_cc(struct wlan_objmgr_pdev *pdev,
		struct cc_regdmn_s *rd)
{
	return ucfg_reg_get_current_cc(pdev, rd);
}

enum channel_state ieee80211_regdmn_get_5g_bonded_channel_state(
		struct wlan_objmgr_pdev *pdev, uint8_t ch,
		enum phy_ch_width bw)
{
	return wlan_reg_get_5g_bonded_channel_state(
			pdev, ch, bw);
}

enum channel_state ieee80211_regdmn_get_2g_bonded_channel_state(
		struct wlan_objmgr_pdev *pdev, uint8_t ch,
		uint8_t sec_ch, enum phy_ch_width bw)
{
	return wlan_reg_get_2g_bonded_channel_state(pdev, ch, sec_ch, bw);
}

uint32_t ieee80211_regdmn_freq_to_chan(struct wlan_objmgr_pdev *pdev,
		uint32_t freq)
{
	return wlan_reg_freq_to_chan(pdev, freq);
}

void ieee80211_regdmn_set_channel_params(struct wlan_objmgr_pdev *pdev, uint8_t ch,
		uint8_t sec_ch_2g,
		struct ch_params *ch_params)
{
	return wlan_reg_set_channel_params(pdev, ch,
			sec_ch_2g, ch_params);
}

int ieee80211_regdmn_get_dfs_region(struct wlan_objmgr_pdev *pdev,
		enum dfs_reg *dfs_region)
{
	return wlan_reg_get_dfs_region(pdev, dfs_region);
}
