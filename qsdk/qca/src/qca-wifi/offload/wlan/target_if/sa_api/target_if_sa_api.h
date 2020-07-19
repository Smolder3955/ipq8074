/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _TARGET_IF_SA_API_H_
#define _TARGET_IF_SA_API_H_

#include <wlan_sa_api_utils_defs.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>

#define sa_api_log(level, args...) \
QDF_TRACE(QDF_MODULE_ID_SA_API, level, ## args)

#define sa_api_logfl(level, format, args...) sa_api_log(level, FL(format), ## args)

#define sa_api_fatal(format, args...) \
	sa_api_logfl(QDF_TRACE_LEVEL_FATAL, format, ## args)
#define sa_api_err(format, args...) \
	sa_api_logfl(QDF_TRACE_LEVEL_ERROR, format, ## args)
#define sa_api_warn(format, args...) \
	sa_api_logfl(QDF_TRACE_LEVEL_WARN, format, ## args)
#define sa_api_info(format, args...) \
	sa_api_logfl(QDF_TRACE_LEVEL_INFO, format, ## args)
#define sa_api_debug(format, args...) \
	sa_api_logfl(QDF_TRACE_LEVEL_DEBUG, format, ## args)

/*
 * Chip specific GPIO pin and function values.
 * configuration depends on Smart Antenna board design.
 */
#define OL_SMART_ANTENNA_PIN0  2
#define OL_SMART_ANTENNA_FUNC0 5

#define OL_SMART_ANTENNA_PIN1  3
#define OL_SMART_ANTENNA_FUNC1 5

#define OL_SMART_ANTENNA_PIN2  4
#define OL_SMART_ANTENNA_FUNC2 5

#define OL_SMART_ANTENNA_PIN3  5
#define OL_SMART_ANTENNA_FUNC3 5

/* IPQ4019's smart antenna related GPIOs */
#define IPQ4019_SMARTANTENNA_BASE_GPIO 0x102c000 /* gpio 44 base address */
#define IPQ4019_SMARTANTENNA_GPIOS_REG_SIZE 0x4000
#define IPQ4019_SMARTANTENNA_GPIO45_OFFSET 0x1000
#define IPQ4019_SMARTANTENNA_GPIO46_OFFSET 0x2000
#define IPQ4019_SMARTANTENNA_GPIO47_OFFSET 0x3000

void target_if_sa_api_tx_ops_register(struct wlan_lmac_if_tx_ops *tx_ops);

static inline
uint32_t target_if_sa_api_get_sa_supported(struct wlan_objmgr_psoc *psoc)
{
	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_get_sa_supported(psoc);
}

static inline
uint32_t target_if_sa_api_get_validate_sw(struct wlan_objmgr_psoc *psoc)
{
	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_get_validate_sw(psoc);
}

static inline
void target_if_sa_api_set_enable_sa(struct wlan_objmgr_psoc *psoc,
					uint8_t value)
{
	psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_enable_sa(psoc, value);
}

static inline
uint32_t target_if_sa_api_get_sa_enable(struct wlan_objmgr_psoc *psoc)
{
	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_get_sa_enable(psoc);
}

static inline
int32_t target_if_sa_api_ucfg_set_param(struct wlan_objmgr_pdev *pdev,
					 char *value)
{
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(pdev);

	if (psoc == NULL) {
		return -1;
	}

	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_ucfg_set_param(pdev, value);
}

static inline
int32_t target_if_sa_api_ucfg_get_param(struct wlan_objmgr_pdev *pdev,
					 char *value)
{
	struct wlan_objmgr_psoc *psoc;

	psoc = wlan_pdev_get_psoc(pdev);

	if (psoc == NULL) {
		return -1;
	}

	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_ucfg_get_param(pdev, value);
}

static inline
uint32_t target_if_sa_api_is_tx_feedback_enabled(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_pdev *pdev)
{
	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_is_tx_feedback_enabled(pdev);
}

static inline
uint32_t target_if_sa_api_is_rx_feedback_enabled(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_pdev *pdev)
{
	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_is_rx_feedback_enabled(pdev);
}

static inline
uint32_t target_if_sa_api_update_tx_feedback(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, struct sa_tx_feedback *feedback)
{
	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_update_tx_feedback(pdev, peer, feedback);
}

static inline
uint32_t target_if_sa_api_update_rx_feedback(struct wlan_objmgr_psoc *psoc,
		struct wlan_objmgr_pdev *pdev, struct wlan_objmgr_peer *peer, struct sa_rx_feedback *feedback)
{
	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_update_rx_feedback(pdev, peer, feedback);
}

static inline
void target_if_sa_api_peer_assoc_hanldler(struct wlan_objmgr_psoc *psoc,
						struct wlan_objmgr_pdev *pdev,
						struct wlan_objmgr_peer *peer,
						struct sa_rate_cap *rate_cap)
{
	psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_peer_assoc_hanldler(pdev, peer, rate_cap);
}

static inline
uint32_t target_if_sa_api_convert_rate_2g(struct wlan_objmgr_psoc *psoc,
					uint32_t rate)
{
	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_convert_rate_2g(rate);
}

static inline uint32_t
target_if_sa_api_convert_rate_5g(struct wlan_objmgr_psoc *psoc,
					uint32_t rate)
{
	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_convert_rate_5g(rate);
}

static inline
uint32_t target_if_sa_api_get_sa_mode(struct wlan_objmgr_psoc *psoc,
					struct wlan_objmgr_pdev *pdev)
{
	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_get_sa_mode(pdev);
}

static inline
uint32_t target_if_sa_api_get_beacon_txantenna(struct wlan_objmgr_psoc *psoc,
					struct wlan_objmgr_pdev *pdev)
{
	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_get_beacon_txantenna(pdev);
}

static inline
uint32_t target_if_sa_api_cwm_action(struct wlan_objmgr_psoc *psoc,
					struct wlan_objmgr_pdev *pdev)
{
	return psoc->soc_cb.rx_ops.sa_api_rx_ops.sa_api_cwm_action(pdev);
}

#endif /* _TARGET_IF_SA_API_H_ */
