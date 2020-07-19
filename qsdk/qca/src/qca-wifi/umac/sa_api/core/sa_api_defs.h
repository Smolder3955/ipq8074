/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc..
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _SA_API_DEFS_I_H_
#define _SA_API_DEFS_I_H_

#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <qdf_list.h>
#include <qdf_timer.h>
#include <qdf_util.h>
#include <wlan_sa_api_utils_defs.h>

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

#if UNIFIED_SMARTANTENNA

#define SMART_ANT_STATE_INIT_DONE 0x01
#define SMART_ANT_STATE_DEINIT_DONE 0x20
#define SMART_ANT_STATE_DEFAULT 0x00

#define SMART_ANT_MODE_SERIAL   0
#define SMART_ANT_MODE_PARALLEL 1

#define AR600P_ASSEMBLE_HW_RATECODE(_rate, _nss, _pream) \
    (((_pream) << 6) | ((_nss) << 4) | (_rate))
#define MAX_OFDM_CCK_RATES 12

#define HT_RATE_FLAG 0x80
#define HT_RATE_MASK 0x7f
#define HT_NSS_MASK 070
#define HT_NSS_SHIFT 3
#define HT_MCS_MASK 0x7
#define AR600P_PREAMBLE_HT 2

#define VHT_PREAMBLE_MASK 0xc0
#define VHT_NSS_MASK      0x30
#define VHT_NSS_SHIFT     4
#define VHT_MCS_MASK      0x0f
#define VHT_MCS_SHIFT     6

#define OPMODE_VHT 2
#define OPMODE_HT 1
#define OPMODE_CCK_OFDM 0

#define RATE_INDEX_CCK_OFDM 0
#define RATE_INDEX_BW20 1
#define RATE_INDEX_BW40 2

#define RADIO_ID_DIRECT_ATTACH  0
#define RADIO_ID_OFF_LOAD  1

#define FALL_BACK_RATES_DIRECT_ATTACH 3
#define FALL_BACK_RATES_OFF_LOAD 1

#define SA_PARM_TYPE_MASK 0xffff0000
#define SA_PARM_TYPE_SHIFT 24

#define SMART_ANTENNA_ENABLED(_pdev) \
	((_pdev->enable) & SMART_ANT_ENABLE_MASK)

#define SMART_ANTENNA_TX_FEEDBACK_ENABLED(_pdev) \
	((_pdev->enable) & SMART_ANT_TX_FEEDBACK_MASK)

#define SMART_ANTENNA_RX_FEEDBACK_ENABLED(_pdev) \
	((_pdev->enable) & SMART_ANT_RX_FEEDBACK_MASK)

#include <ieee80211_rateset.h>
#include <qdf_atomic.h>

#define MAX_TX_PPDU_SIZE 32

struct pdev_sa_api;

/**
 * struct sa_api_context - global sa_api object per soc
 * sa_suppoted: sa_suppoted module parameter
 * sa_validate_sw: validate sw module parameter
 * enable: psoc level smart antenna enable
 * psoc_obj: Reference to psoc global object
 *
 * call back functions to control ol/da
 * sa_api_pdev_init: pdev initialization
 * sa_api_pdev_deinit: pdev deinitialization
 * sa_api_enable: sa api enable
 * sa_api_disable: sa api disable
 */
struct sa_api_context {
	uint32_t sa_suppoted;
	uint32_t sa_validate_sw;
	uint32_t enable;
	struct wlan_objmgr_psoc *psoc_obj;

	void (*sa_api_pdev_init)(struct wlan_objmgr_psoc *psoc,
			struct wlan_objmgr_pdev *pdev,
			struct sa_api_context *sc,
			struct pdev_sa_api *pa);
	void (*sa_api_pdev_deinit)(struct wlan_objmgr_pdev *pdev,
			struct sa_api_context *sc,
			struct pdev_sa_api *pa);
	void (*sa_api_enable)(struct wlan_objmgr_psoc *psoc);
	void (*sa_api_disable)(struct wlan_objmgr_psoc *psoc);
};

/**
 * struct pdev_sa_api - radio specific sa_api object
 * state: smart antenna initialization state
 * mode: 0 - serial mode, 1 - parallel mode
 * max_fallback_rates: max fall back rates
 * radio_id: RadioId , 0 - direct attach, 1 - offload
 * interface_id: interface id
 * enable: bit 0: 0 - SA disable, 1- SA enable,
 *         bit 4: 0 - Tx feedback disable, 1 - Tx feedback enable
 *         bit 5: 0 - Rx feedback disable, 1 - Rx feedback enable
 * @pdev_obj: Reference to pdev global object
 */
struct pdev_sa_api {
	uint8_t state;
	uint8_t mode;
	uint8_t max_fallback_rates;
	uint8_t radio_id;
	uint8_t interface_id;
	uint8_t enable;
	struct wlan_objmgr_pdev *pdev_obj;
};

/**
 * struct peer_sa_api - peer specific sa_api object
 * ccp: peer specific external context
 * ch_width: current operating bw of peer
 * peer_obj:  Reference to peer global object
 */
struct peer_sa_api {
	void *ccp;
	uint8_t ch_width;
	struct wlan_objmgr_peer *peer_obj;
};

#endif

#endif /* _SA_API_DEFS_I_H_*/
