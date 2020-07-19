/*
 * Copyright (c) 2011, 2017-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#ifndef _FD_DEFS_I_H_
#define _FD_DEFS_I_H_

#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <qdf_list.h>
#include <qdf_util.h>
#include <ieee80211.h>

#define fd_log(level, args...) \
QDF_TRACE(QDF_MODULE_ID_FD, level, ## args)

#define fd_logfl(level, format, args...) fd_log(level, FL(format), ## args)

#define fd_fatal(format, args...) \
	fd_logfl(QDF_TRACE_LEVEL_FATAL, format, ## args)
#define fd_err(format, args...) \
	fd_logfl(QDF_TRACE_LEVEL_ERROR, format, ## args)
#define fd_warn(format, args...) \
	fd_logfl(QDF_TRACE_LEVEL_WARN, format, ## args)
#define fd_info(format, args...) \
	fd_logfl(QDF_TRACE_LEVEL_INFO, format, ## args)
#define fd_debug(format, args...) \
	fd_logfl(QDF_TRACE_LEVEL_DEBUG, format, ## args)

/* FILS Discovery MIN interval */
#define WLAN_FD_INTERVAL_MIN               20
/* Deferred FD frame MAX size */
#define WLAN_FD_DEFERRED_MAX_SIZE          128
/* FILS Discovery Public Action */
#define WLAN_ACTION_FILS_DISCOVERY         34
/* FILS Enable bit in CFG param */
#define WLAN_FILS_ENABLE_BIT               31
/* FD period mask */
#define WLAN_FD_PERIOD_MASK                0xFFFF

#define WLAN_FD_FRAMECNTL_CAP              0x0020
#define WLAN_FD_FRAMECNTL_SHORTSSID        0x0040
#define WLAN_FD_FRAMECNTL_APCSN            0x0080
#define WLAN_FD_FRAMECNTL_ANO              0x0100
#define WLAN_FD_FRAMECNTL_CH_CENTERFREQ    0x0200
#define WLAN_FD_FRAMECNTL_PRIMARY_CH       0x0400
#define WLAN_FD_FRAMECNTL_RSN_INFO         0x0800
#define WLAN_FD_FRAMECNTL_LEN_PRES         0x1000
#define WLAN_FD_FRAMECNTL_MD_PRES          0x2000
#define WLAN_FD_FRAMECNTL_11B_PRESENT      0x4000
#define WLAN_FD_FRAMECNTL_NON_OCE_PRESENT  0x8000

#define WLAN_FD_IS_CAP_PRESENT(_v)             (_v & WLAN_FD_FRAMECNTL_CAP)
#define WLAN_FD_IS_SHORTSSID_PRESENT(_v)       (_v & WLAN_FD_FRAMECNTL_SHORTSSID)
#define WLAN_FD_IS_APCSN_PRESENT(_v)           (_v & WLAN_FD_FRAMECNTL_APCSN)
#define WLAN_FD_IS_ANO_PRESENT(_v)             (_v & WLAN_FD_FRAMECNTL_ANO)
#define WLAN_FD_IS_LEN_PRESENT(_v)             (_v & WLAN_FD_FRAMECNTL_LEN_PRES)
#define WLAN_FD_IS_FRAMECNTL_CH_CENTERFREQ(_v) (_v & WLAN_FD_FRAMECNTL_CH_CENTERFREQ)
#define WLAN_FD_IS_FRAMECNTL_PRIMARY_CH(_v)    (_v & WLAN_FD_FRAMECNTL_PRIMARY_CH)

/* FD Frame */
/**
 * struct fd_action_header - FILS Discovery Action frame header
 * @action_header: IEEE80211 Action frame header
 * @fd_frame_cntl: FILS Disovery Frame Control
 * @timestamp:     Time stamp
 * @bcn_interval:  Beacon Interval
 * @elem:          variable len sub element fileds
 */
struct fd_action_header {
	struct ieee80211_action  action_header;
	uint16_t                 fd_frame_cntl;
	uint8_t                  timestamp[8];
	uint16_t                 bcn_interval;
	uint8_t                  elem[1];
};

struct fd_buf_entry {
	bool                        is_dma_mapped;
	qdf_list_node_t             fd_deferred_list_elem;
	qdf_nbuf_t                  fd_buf;
};

struct fd_vdev {
	uint8_t                     fils_enable;
	uint32_t                    fd_update;
        uint32_t                    fd_period;
	bool                        is_fd_dma_mapped;
	qdf_list_t                  fd_deferred_list;
	qdf_nbuf_t                  fd_wbuf;
	qdf_spinlock_t              fd_lock;
	struct wlan_objmgr_vdev     *vdev_obj;
};

struct fd_context {
	bool    is_fd_capable;
	struct wlan_objmgr_psoc *psoc_obj;

	QDF_STATUS (*fd_enable)(struct wlan_objmgr_psoc *psoc);
	QDF_STATUS (*fd_disable)(struct wlan_objmgr_psoc *psoc);
};
#endif /* _FD_DEFS_I_H_ */
