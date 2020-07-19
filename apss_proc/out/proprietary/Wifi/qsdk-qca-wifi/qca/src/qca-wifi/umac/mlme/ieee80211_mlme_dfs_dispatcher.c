/*
 * Copyright (c) 2017, 2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#include <ieee80211_var.h>
#include "ieee80211_mlme_dfs_interface.h"

#ifdef DFS_COMPONENT_ENABLE
#include <wlan_dfs_tgt_api.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_dfs_utils_api.h>

void mlme_dfs_control(struct wlan_objmgr_pdev *pdev,
		u_int id,
		void *indata,
		uint32_t insize,
		void *outdata,
		uint32_t *outsize,
		int *error)
{
	tgt_dfs_control(pdev, id, indata, insize, outdata,
			outsize, error);
}

void mlme_dfs_reset(struct wlan_objmgr_pdev *pdev)
{
	utils_dfs_reset(pdev);
}

bool mlme_dfs_is_freq_in_nol(struct wlan_objmgr_pdev *pdev, uint32_t freq)
{
        return utils_dfs_is_freq_in_nol(pdev, freq);
}

void mlme_dfs_stop(struct wlan_objmgr_pdev *pdev)
{
	tgt_dfs_stop(pdev);
}

void mlme_dfs_cac_valid_reset(struct wlan_objmgr_pdev *pdev,
		uint8_t prevchan_ieee,
		uint32_t prevchan_flags)
{
	utils_dfs_cac_valid_reset(pdev, prevchan_ieee, prevchan_flags);
}
#if ATH_SUPPORT_ZERO_CAC_DFS
void mlme_dfs_reset_precaclists(struct wlan_objmgr_pdev *pdev)
{
	utils_dfs_reset_precaclists(pdev);
}
#endif

#if QCA_SUPPORT_ETSI_PRECAC_DFS
void mlme_dfs_reset_etsiprecaclists(struct wlan_objmgr_pdev *pdev)
{
	utils_dfs_reset_etsi_precaclists(pdev);
}
#endif

void mlme_dfs_cac_stop(struct wlan_objmgr_pdev *pdev)
{
	utils_dfs_cac_stop(pdev);
}

bool mlme_dfs_is_precac_done(struct wlan_objmgr_pdev *pdev, uint16_t ch_freq,
				uint64_t ch_flags, uint16_t ch_flagext,
				uint8_t ch_ieee, uint8_t ch_vhtop_ch_freq_seg1,
				uint8_t ch_vhtop_ch_freq_seg2)
{
	struct wlan_channel channel;

	channel.ch_freq = ch_freq;
	channel.ch_flags = ch_flags;
	channel.ch_flagext = ch_flagext;
	channel.ch_ieee = ch_ieee;
	channel.ch_freq_seg1 = ch_vhtop_ch_freq_seg1;
	channel.ch_freq_seg2 = ch_vhtop_ch_freq_seg2;

	return utils_dfs_is_precac_done(pdev, &channel);
}
void mlme_dfs_clear_cac_started_chan(struct wlan_objmgr_pdev *pdev)
{
	utils_dfs_clear_cac_started_chan(pdev);
}

bool mlme_dfs_check_for_cac_start(struct wlan_objmgr_pdev *pdev,
				  bool *continue_current_cac)
{
	return utils_dfs_check_for_cac_start(pdev, continue_current_cac);
}

void mlme_dfs_start_cac_timer(struct wlan_objmgr_pdev *pdev)
{
	utils_dfs_start_cac_timer(pdev);
}

void mlme_dfs_set_update_nol_flag(struct wlan_objmgr_pdev *pdev, bool val)
{
	utils_dfs_set_update_nol_flag(pdev, val);
}

void mlme_dfs_nol_addchan(struct wlan_objmgr_pdev *pdev,
		struct ieee80211_ath_channel *chan,
		uint32_t dfs_nol_timeout)
{
	utils_dfs_nol_addchan(pdev, chan->ic_freq, dfs_nol_timeout);
}

void mlme_dfs_get_nol_timeout(struct wlan_objmgr_pdev *pdev,
		int *dfs_nol_timeout)
{
	utils_dfs_get_nol_timeout(pdev, dfs_nol_timeout);
}

void mlme_dfs_nol_update(struct wlan_objmgr_pdev *pdev)
{
	utils_dfs_nol_update(pdev);
}

uint16_t mlme_dfs_get_usenol(struct wlan_objmgr_pdev *pdev)
{
	uint16_t usenol = 0;

	utils_dfs_get_usenol(pdev, &usenol);
	return usenol;
}

bool mlme_dfs_get_update_nol_flag(struct wlan_objmgr_pdev *pdev)
{
	bool nol_flag = false;

	utils_dfs_get_update_nol_flag(pdev, &nol_flag);
	return nol_flag;
}

int mlme_dfs_random_channel(struct wlan_objmgr_pdev *pdev,
		struct ch_params *ch_params,
		uint16_t flags)
{
	uint8_t target_chan_ieee = 0;

	utils_dfs_get_random_channel(pdev, flags, ch_params, NULL,
			&target_chan_ieee, NULL);

	return target_chan_ieee;
}

int mlme_dfs_bw_reduced_channel(struct wlan_objmgr_pdev *pdev,
		struct ch_params *ch_params)
{
	uint8_t target_chan_ieee = 0;

	utils_dfs_bw_reduced_channel(pdev, ch_params, NULL,
			&target_chan_ieee);

	return target_chan_ieee;
}

void mlme_dfs_radar_disable(struct wlan_objmgr_pdev *pdev)
{
	utils_dfs_radar_disable(pdev);
}

int mlme_dfs_get_rn_use_nol(struct wlan_objmgr_pdev *pdev)
{
	int rn_use_nol = 0;

	utils_dfs_get_dfs_use_nol(pdev, &rn_use_nol);
	return rn_use_nol;
}

void mlme_dfs_second_segment_radar_disable(struct wlan_objmgr_pdev *pdev)
{
	utils_dfs_second_segment_radar_disable(pdev);
}

int mlme_dfs_is_ap_cac_timer_running(struct wlan_objmgr_pdev *pdev)
{
	int is_ap_cac_timer_running = 0;

	ucfg_dfs_is_ap_cac_timer_running(pdev, &is_ap_cac_timer_running);
	return is_ap_cac_timer_running;
}

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
int mlme_dfs_is_precac_enabled(struct wlan_objmgr_pdev *pdev)
{
	int is_precac_enabled = 0;

	ucfg_dfs_get_precac_enable(pdev, &is_precac_enabled);
	return is_precac_enabled;
}

bool
mlme_dfs_decide_precac_preferred_chan(struct wlan_objmgr_pdev *pdev,
				      uint8_t *ch_ieee,
				      enum wlan_phymode mode)
{
	return utils_dfs_precac_decide_pref_chan(pdev, ch_ieee,
						 mode);
}
#endif

int mlme_dfs_override_cac_timeout(struct wlan_objmgr_pdev *pdev,
		int cac_timeout)
{
	int status = 0;

	ucfg_dfs_override_cac_timeout(pdev, cac_timeout, &status);
	return status;
}

int mlme_dfs_get_override_cac_timeout(struct wlan_objmgr_pdev *pdev,
		int *cac_timeout)
{
	int status = 0;

	ucfg_dfs_get_override_cac_timeout(pdev, cac_timeout, &status);
	return status;
}

void mlme_dfs_get_override_precac_timeout(struct wlan_objmgr_pdev *pdev,
		int *precac_timeout)
{
	ucfg_dfs_get_override_precac_timeout(pdev, precac_timeout);
}

void mlme_dfs_getnol(struct wlan_objmgr_pdev *pdev, void *dfs_nolinfo)
{
	ucfg_dfs_getnol(pdev, dfs_nolinfo);
}

void mlme_dfs_stacac_stop(struct wlan_objmgr_pdev *pdev)
{
	utils_dfs_stacac_stop(pdev);
}

void mlme_dfs_set_cac_timer_running(struct wlan_objmgr_pdev *pdev, int val)
{
	utils_dfs_set_cac_timer_running(pdev, val);
}

void mlme_dfs_get_nol_chfreq_and_chwidth(struct wlan_objmgr_pdev *pdev,
		void *nollist,
		uint32_t *nol_chfreq,
		uint32_t *nol_chwidth,
		int index)
{
	utils_dfs_get_nol_chfreq_and_chwidth(pdev,
			nollist,
			nol_chfreq,
			nol_chwidth,
			index);
}

void mlme_dfs_update_cur_chan_flags(struct wlan_objmgr_pdev *pdev,
		uint64_t flags,
		uint16_t flagext)
{
	utils_dfs_update_cur_chan_flags(pdev, flags, flagext);
}

#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
bool mlme_dfs_is_spoof_check_failed(struct wlan_objmgr_pdev *pdev)
{
	bool is_spoof_check_failed = false;

	utils_dfs_is_spoof_check_failed(pdev, &is_spoof_check_failed);
	return is_spoof_check_failed;
}
#endif /* HOST_DFS_SPOOF_TEST */

uint8_t mlme_dfs_freq_to_chan(uint16_t freq)
{
	return utils_dfs_freq_to_chan(freq);
}

void mlme_dfs_reg_update_nol_ch(struct wlan_objmgr_pdev *pdev,
		uint8_t *ch_list,
		uint8_t num_ch,
		bool nol_ch)
{
	utils_dfs_reg_update_nol_ch(pdev, ch_list, num_ch, nol_ch);
}

QDF_STATUS mlme_dfs_fetch_nol_ie_info(struct wlan_objmgr_pdev *pdev,
		uint8_t *nol_ie_bandwidth,
		uint16_t *nol_ie_startfreq,
		uint8_t *nol_ie_bitmap)
{
	return
	utils_dfs_fetch_nol_ie_info(pdev, nol_ie_bandwidth, nol_ie_startfreq,
			nol_ie_bitmap);
}

QDF_STATUS mlme_dfs_set_rcsa_flags(struct wlan_objmgr_pdev *pdev,
		bool is_rcsa_ie_sent,
		bool is_nol_ie_sent)
{
	return utils_dfs_set_rcsa_flags(pdev, is_rcsa_ie_sent, is_nol_ie_sent);
}

QDF_STATUS mlme_dfs_get_rcsa_flags(struct wlan_objmgr_pdev *pdev,
		bool *is_rcsa_ie_sent,
		bool *is_nol_ie_sent)
{
	return utils_dfs_get_rcsa_flags(pdev, is_rcsa_ie_sent, is_nol_ie_sent);
}

bool mlme_dfs_process_nol_ie_bitmap(struct wlan_objmgr_pdev *pdev,
		uint8_t nol_ie_bandwidth,
		uint16_t nol_ie_startfreq,
		uint8_t nol_ie_bitmap)
{
	return
	utils_dfs_process_nol_ie_bitmap(pdev, nol_ie_bandwidth,
			nol_ie_startfreq, nol_ie_bitmap);
}

#else

void mlme_dfs_control(struct wlan_objmgr_pdev *pdev,
		u_int id,
		void *indata,
		uint32_t insize,
		void *outdata,
		uint32_t *outsize,
		int *error)
{
}

void mlme_dfs_reset(struct wlan_objmgr_pdev *pdev)
{
}

bool mlme_dfs_is_freq_in_nol(struct wlan_objmgr_pdev *pdev, uint32_t freq)
{
	return false;
}

void mlme_dfs_stop(struct wlan_objmgr_pdev *pdev)
{
}

void mlme_dfs_cac_valid_reset(struct wlan_objmgr_pdev *pdev,
		uint8_t prevchan_ieee,
		uint32_t prevchan_flags)
{
}
#if ATH_SUPPORT_ZERO_CAC_DFS
void mlme_dfs_reset_precaclists(struct wlan_objmgr_pdev *pdev)
{
}
#endif

#if QCA_SUPPORT_ETSI_PRECAC_DFS
void mlme_dfs_reset_etsiprecaclists(struct wlan_objmgr_pdev *pdev)
{
}
#endif

void mlme_dfs_cac_stop(struct wlan_objmgr_pdev *pdev)
{
}

bool mlme_dfs_if_precac_done(struct wlan_objmgr_pdev *pdev, uint16_t ch_freq,
				uint64_t ch_flags, uint16_t ch_flagext,
				uint8_t ch_ieee, uint8_t ch_vhtop_ch_freq_seg1,
				uint8_t ch_vhtop_ch_freq_seg2)
{
	return false;
}
void mlme_dfs_clear_cac_started_chan(struct wlan_objmgr_pdev *pdev)
{
}

bool mlme_dfs_check_for_cac_start(struct wlan_objmgr_pdev *pdev,
				  bool *continue_current_cac)
{
	return 0;
}

void mlme_dfs_start_cac_timer(struct wlan_objmgr_pdev *pdev)
{
	mlme_dfs_proc_cac(pdev);
}

void mlme_dfs_set_update_nol_flag(struct wlan_objmgr_pdev *pdev, bool val)
{
}

void mlme_dfs_nol_addchan(struct wlan_objmgr_pdev *pdev,
		struct ieee80211_ath_channel *chan,
		uint32_t dfs_nol_timeout)
{
}

void mlme_dfs_get_nol_timeout(struct wlan_objmgr_pdev *pdev,
		int *dfs_nol_timeout)
{
}

void mlme_dfs_nol_update(struct wlan_objmgr_pdev *pdev)
{
}

uint16_t mlme_dfs_get_usenol(struct wlan_objmgr_pdev *pdev)
{
	uint16_t usenol = 0;
	return usenol;
}

bool mlme_dfs_get_update_nol_flag(struct wlan_objmgr_pdev *pdev)
{
	bool nol_flag = false;

	return nol_flag;
}

int mlme_dfs_random_channel(struct wlan_objmgr_pdev *pdev,
		uint8_t is_select_nondfs,
		uint8_t skip_curchan)
{
	int target_channel = 0;

	return target_channel;
}

void mlme_dfs_radar_disable(struct wlan_objmgr_pdev *pdev)
{
}

int mlme_dfs_get_rn_use_nol(struct wlan_objmgr_pdev *pdev)
{
	int rn_use_nol = 0;

	return rn_use_nol;
}

void mlme_dfs_second_segment_radar_disable(struct wlan_objmgr_pdev *pdev)
{
}

int mlme_dfs_is_ap_cac_timer_running(struct wlan_objmgr_pdev *pdev)
{
	int is_ap_cac_timer_running = 0;

	return is_ap_cac_timer_running;
}

#ifdef WLAN_DFS_PRECAC_AUTO_CHAN_SUPPORT
int mlme_dfs_is_precac_enabled(struct wlan_objmgr_pdev *pdev)
{
	int is_precac_enabled = 0;
	return is_precac_enabled;
}
#endif

int mlme_dfs_override_cac_timeout(struct wlan_objmgr_pdev *pdev,
		int cac_timeout)
{
	int status = 0;

	return status;
}

int mlme_dfs_get_override_cac_timeout(struct wlan_objmgr_pdev *pdev,
		int *cac_timeout)
{
	int status = 0;

	return status;
}

int mlme_dfs_get_override_precac_timeout(struct wlan_objmgr_pdev *pdev,
		int *precac_timeout)
{
	int status = 0;

	return status;
}

void mlme_dfs_getnol(struct wlan_objmgr_pdev *pdev, void *dfs_nolinfo)
{
}

void mlme_dfs_stacac_stop(struct wlan_objmgr_pdev *pdev)
{
}

bool mlme_dfs_is_ignore_dfs(struct wlan_objmgr_pdev *pdev)
{
	bool ignore_dfs = false;

	return ignore_dfs;
}

bool mlme_dfs_is_cac_valid(struct wlan_objmgr_pdev *pdev)
{
	bool is_cac_valid = false;

	return is_cac_valid;
}

bool mlme_dfs_is_ignore_cac(struct wlan_objmgr_pdev *pdev)
{
	bool ignore_cac = false;

	return ignore_cac;
}

void mlme_dfs_set_cac_timer_running(struct wlan_objmgr_pdev *pdev, int val)
{
}

void mlme_dfs_get_nol_chfreq_and_chwidth(struct wlan_objmgr_pdev *pdev,
		void *nollist,
		uint32_t *nol_chfreq,
		uint32_t *nol_chwidth,
		int index)
{
}

void mlme_dfs_update_cur_chan_flags(struct wlan_objmgr_pdev *pdev,
		uint64_t flags,
		uint16_t flagext)
{
}

#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
bool mlme_dfs_is_spoof_check_failed(struct wlan_objmgr_pdev *pdev)
{
	bool is_spoof_check_failed = false;

	return is_spoof_check_failed;
}
#endif /* HOST_DFS_SPOOF_TEST */
uint8_t mlme_dfs_freq_to_chan(uint16_t freq)
{
    return 0;
}

void mlme_dfs_reg_update_nol_ch(struct wlan_objmgr_pdev *pdev,
				uint8_t *ch_list,
				uint8_t num_ch,
				bool nol_ch)
{
}

QDF_STATUS mlme_dfs_fetch_nol_ie_info(struct wlan_objmgr_pdev *pdev,
		uint8_t *nol_ie_bandwidth,
		uint16_t *nol_ie_startfreq,
		uint8_t *nol_ie_bitmap)
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS mlme_dfs_set_rcsa_flags(struct wlan_objmgr_pdev *pdev,
		bool is_rcsa_ie_sent,
		bool is_nol_ie_sent)
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS mlme_dfs_set_rcsa_flags(struct wlan_objmgr_pdev *pdev,
		bool *is_rcsa_ie_sent,
		bool *is_nol_ie_sent)
{
	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS mlme_dfs_process_nol_ie_bitmap(struct wlan_objmgr_pdev *pdev,
		uint8_t nol_ie_bandwidth,
		uint16_t nol_ie_startfreq,
		uint8_t nol_ie_bitmap)
{
	return false;
}
#endif
