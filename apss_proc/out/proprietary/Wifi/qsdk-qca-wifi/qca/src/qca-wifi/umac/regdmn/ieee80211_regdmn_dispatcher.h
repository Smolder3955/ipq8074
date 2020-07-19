/*
 * Copyright (c) 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#ifndef _IEEE80211_REGDMN_DISPATCHER_H_
#define _IEEE80211_REGDMN_DISPATCHER_H_

/**
 * ieee80211_regdmn_program_cc() - Program user country code or regdomain
 * @pdev: The physical dev to program country code or regdomain
 * @rd: User country code or regdomain
 *
 * Return: QDF_STATUS
 */
int ieee80211_regdmn_program_cc(struct wlan_objmgr_pdev *pdev,
		struct cc_regdmn_s *rd);

/**
 * ieee80211_regdmn_get_chip_mode() - Get supported chip mode
 * @pdev: pdev pointer
 * @chip_mode: chip mode
 *
 * Return: QDF STATUS
 */
QDF_STATUS ieee80211_regdmn_get_chip_mode(struct wlan_objmgr_pdev *pdev,
		uint32_t *chip_mode);

/**
 * ieee80211_regdmn_get_freq_range() - Get 2GHz and 5GHz frequency range
 * @pdev: pdev pointer
 * @low_2g: low 2GHz frequency range
 * @high_2g: high 2GHz frequency range
 * @low_5g: low 5GHz frequency range
 * @high_5g: high 5GHz frequency range
 *
 * Return: QDF status
 */
QDF_STATUS ieee80211_regdmn_get_freq_range(struct wlan_objmgr_pdev *pdev,
		uint32_t *low_2g,
		uint32_t *high_2g,
		uint32_t *low_5g,
		uint32_t *high_5g);

/**
 * ieee80211_regdmn_get_current_chan_list () - get current channel list
 * @pdev: pdev ptr
 * @chan_list: channel list
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ieee80211_regdmn_get_current_chan_list(struct wlan_objmgr_pdev *pdev,
		struct regulatory_channel *chan_list);

/**
 * ieee80211_regdmn_get_current_cc() - get current country code or regdomain
 * @pdev: The physical dev to program country code or regdomain
 * @rd: Pointer to country code or regdomain
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ieee80211_regdmn_get_current_cc(struct wlan_objmgr_pdev *pdev,
		struct cc_regdmn_s *rd);

/**
 * ieee80211_regdmn_get_5g_bonded_channel_state() - Get 5G bonded channel state
 * @pdev: The physical dev to program country code or regdomain
 * @ch: channel number.
 * @bw: channel band width
 *
 * Return: channel state
 */
enum channel_state ieee80211_regdmn_get_5g_bonded_channel_state(
		struct wlan_objmgr_pdev *pdev, uint8_t ch,
		enum phy_ch_width bw);

/**
 * ieee80211_regdmn_get_2g_bonded_channel_state() - Get 2G bonded channel state
 * @pdev: The physical dev to program country code or regdomain
 * @ch: channel number.
 * @sec_ch: Secondary channel.
 * @bw: channel band width
 *
 * Return: channel state
 */
enum channel_state ieee80211_regdmn_get_2g_bonded_channel_state(
		struct wlan_objmgr_pdev *pdev, uint8_t ch,
		uint8_t sec_ch, enum phy_ch_width bw);

/**
 * ieee80211_regdmn_freq_to_chan () - convert channel freq to channel number
 * @pdev: The physical dev to set current country for
 * @freq: frequency
 *
 * Return: true or false
 */
uint32_t ieee80211_regdmn_freq_to_chan(struct wlan_objmgr_pdev *pdev,
		uint32_t freq);

/**
 * ieee80211_regdmn_set_channel_params () - Sets channel parameteres for given bandwidth
 * @pdev: The physical dev to program country code or regdomain
 * @ch: channel number.
 * @sec_ch_2g: Secondary channel.
 * @ch_params: pointer to the channel parameters.
 *
 * Return: None
 */
void ieee80211_regdmn_set_channel_params(struct wlan_objmgr_pdev *pdev, uint8_t ch,
		uint8_t sec_ch_2g,
		struct ch_params *ch_params);

/**
 * ieee80211_regdmn_get_dfs_region() - Get the DFS region.
 * @pdev: The physical dev to program country code or regdomain
 * @dfs_region: pointer to dfs_reg.
 *
 * Return: QDF_STATUS
 */
int ieee80211_regdmn_get_dfs_region(struct wlan_objmgr_pdev *pdev,
		enum dfs_reg *dfs_region);
#endif
