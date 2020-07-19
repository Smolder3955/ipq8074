/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _TARGET_IF_ATF_H_
#define _TARGET_IF_ATF_H_

#include <wlan_atf_utils_defs.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>

#define atf_log(level, args...) \
QDF_TRACE(QDF_MODULE_ID_ATF, level, ## args)

#define atf_logfl(level, format, args...) atf_log(level, FL(format), ## args)

#define atf_fatal(format, args...) \
	atf_logfl(QDF_TRACE_LEVEL_FATAL, format, ## args)
#define atf_err(format, args...) \
	atf_logfl(QDF_TRACE_LEVEL_ERROR, format, ## args)
#define atf_warn(format, args...) \
	atf_logfl(QDF_TRACE_LEVEL_WARN, format, ## args)
#define atf_info(format, args...) \
	atf_logfl(QDF_TRACE_LEVEL_INFO, format, ## args)
#define atf_debug(format, args...) \
	atf_logfl(QDF_TRACE_LEVEL_DEBUG, format, ## args)

void target_if_atf_tx_ops_register(struct wlan_lmac_if_tx_ops *tx_ops);

uint32_t target_if_atf_get_num_msdu_desc(struct wlan_objmgr_psoc *psoc);

uint8_t target_if_atf_is_tx_traffic_blocked(struct wlan_objmgr_vdev *vdev,
					    uint8_t *peer_mac,
					    struct sk_buff *skb);

static inline uint32_t target_if_atf_get_vdev_ac_blk_cnt(
		struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_vdev *vdev)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_vdev_ac_blk_cnt(vdev);
}

static inline uint32_t target_if_atf_get_vdev_blk_txtraffic(
		struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_vdev *vdev)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_vdev_blk_txtraffic(vdev);
}

static inline uint8_t target_if_atf_get_peer_blk_txbitmap(
		struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_peer *peer)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_peer_blk_txbitmap(peer);
}

static inline void target_if_atf_set_obss_scale(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_pdev *pdev, uint32_t scale)
{
	psoc->soc_cb.rx_ops.atf_rx_ops.atf_set_obss_scale(pdev, scale);
}

static inline uint32_t target_if_atf_get_obss_scale(
		struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_pdev *pdev)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_obss_scale(pdev);
}

static inline void target_if_atf_set_sched(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_pdev *pdev, uint8_t value)
{
	psoc->soc_cb.rx_ops.atf_rx_ops.atf_set_sched(pdev, value);
}

static inline uint32_t target_if_atf_get_sched(struct wlan_objmgr_psoc *psoc,
						struct wlan_objmgr_pdev *pdev)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_sched(pdev);
}

static inline uint8_t target_if_atf_get_ssid_group(
		struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_pdev *pdev)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_ssidgroup(pdev);
}

static inline uint8_t target_if_atf_get_mode(struct wlan_objmgr_psoc *psoc)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_mode(psoc);
}

static inline uint32_t target_if_atf_get_msdu_desc(
						struct wlan_objmgr_psoc *psoc)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_msdu_desc(psoc);
}

static inline uint32_t target_if_atf_get_max_vdevs(
						struct wlan_objmgr_psoc *psoc)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_max_vdevs(psoc);
}

static inline uint32_t target_if_atf_get_peers(struct wlan_objmgr_psoc *psoc)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_peers(psoc);
}

static inline uint8_t target_if_atf_get_fmcap(struct wlan_objmgr_psoc *psoc)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_fmcap(psoc);
}

static inline void target_if_atf_set_fmcap(struct wlan_objmgr_psoc *psoc,
						uint8_t value)
{
	psoc->soc_cb.rx_ops.atf_rx_ops.atf_set_fmcap(psoc, value);
}

static inline uint16_t target_if_atf_get_token_allocated(
	struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_peer *peer)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_token_allocated(peer);
}

static inline void target_if_atf_set_token_allocated(
	struct wlan_objmgr_psoc *psoc,
	struct wlan_objmgr_peer *peer, uint16_t value)
{
	psoc->soc_cb.rx_ops.atf_rx_ops.atf_set_token_allocated(peer, value);
}

static inline uint16_t target_if_atf_get_token_utilized(
	struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_peer *peer)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_token_utilized(peer);
}

static inline void target_if_atf_set_token_utilized(
	struct wlan_objmgr_psoc *psoc,
	struct wlan_objmgr_peer *peer, uint16_t value)
{
	psoc->soc_cb.rx_ops.atf_rx_ops.atf_set_token_utilized(peer, value);
}

static inline void target_if_atf_get_peer_stats(
	struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_peer *peer,
	struct atf_stats *stats)
{
	psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_peer_stats(peer, stats);
}

static inline QDF_STATUS target_if_atf_adjust_subgroup_txtokens(
	struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_peer *peer,
	uint8_t ac, uint32_t actual_duration, uint32_t est_duration)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_adjust_subgroup_txtokens(
			peer, ac, actual_duration, est_duration);
}

static inline QDF_STATUS target_if_atf_account_subgroup_txtokens(
	struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_peer *peer,
	uint8_t ac, uint32_t duration)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_account_subgroup_txtokens(
			peer, ac, duration);
}

static inline QDF_STATUS target_if_atf_subgroup_free_buf(
	struct wlan_objmgr_psoc *psoc, uint16_t buf_acc_size, void *bf_atf_sg)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_subgroup_free_buf(
						buf_acc_size, bf_atf_sg);
}

static inline QDF_STATUS target_if_atf_update_subgroup_tidstate(
	struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_peer *peer,
	uint8_t atf_nodepaused)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_update_subgroup_tidstate(
		peer, atf_nodepaused);
}

static inline uint8_t target_if_atf_get_subgroup_airtime(
	struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_peer *peer,
	uint8_t ac)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_subgroup_airtime(
			peer, ac);
}

static inline uint32_t target_if_atf_buf_distribute(
				struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_pdev *pdev,
				struct wlan_objmgr_peer *peer, int8_t ac)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_buf_distribute(pdev, peer, ac);
}

static inline uint32_t target_if_atf_get_shadow_alloted_tx_tokens(
	struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_pdev *pdev)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_shadow_alloted_tx_tokens(pdev);
}

static inline uint32_t target_if_atf_get_txtokens_common(
	struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_pdev *pdev)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_txtokens_common(pdev);
}

static inline uint8_t target_if_atf_get_atf_commit(
	struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_pdev *pdev)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_atf_commit(pdev);
}

static inline uint32_t target_if_atf_get_tx_tokens(
	struct wlan_objmgr_psoc *psoc, struct wlan_objmgr_peer *peer)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_get_tx_tokens(peer);
}

static inline void* target_if_atf_update_buf_held(struct wlan_objmgr_psoc *psoc,
				struct wlan_objmgr_peer *peer, int8_t ac)
{
	return psoc->soc_cb.rx_ops.atf_rx_ops.atf_update_buf_held(peer, ac);
}

#endif /* _TARGET_IF_ATF_H_ */
