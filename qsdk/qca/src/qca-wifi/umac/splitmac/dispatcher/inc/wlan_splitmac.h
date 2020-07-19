/*
 * Copyright (c) 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */

#if WLAN_SUPPORT_SPLITMAC
#ifndef WLAN_SPLITMAC_H
#define WLAN_SPLITMAC_H
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <qdf_list.h>
#include <qdf_timer.h>
#include <qdf_util.h>

struct splitmac_vdev_priv_obj {
	struct wlan_objmgr_vdev *vdev;
	int splitmac_enabled;
};

struct splitmac_peer_priv_obj {
	struct wlan_objmgr_peer *peer;
	int splitmac_state;
/* splitmac state definitions */
#define SPLITMAC_NODE_INIT		   0x1
#define SPLITMAC_IN_ASSOC_REQ	   0x2
#define SPLITMAC_ADDCLIENT_START   0x3
#define SPLITMAC_ADDCLIENT_END     0x4
};

QDF_STATUS wlan_splitmac_init(void);
QDF_STATUS wlan_splitmac_deinit(void);
QDF_STATUS splitmac_vdev_obj_create(struct wlan_objmgr_vdev *vdev, void *arg);
QDF_STATUS splitmac_vdev_obj_destroy(struct wlan_objmgr_vdev *vdev, void *arg);
int splitmac_is_enabled(struct wlan_objmgr_vdev *vdev);
void splitmac_set_enabled_flag(struct wlan_objmgr_vdev *vdev, int enabled);
int splitmac_get_enabled_flag(struct wlan_objmgr_vdev *vdev);
int splitmac_api_add_client(struct wlan_objmgr_vdev *vdev, u_int8_t *stamac,
				u_int16_t associd, u_int8_t qos, void *lrates,
				void *htrates, u_int16_t vhtrates, u_int16_t herates);
int splitmac_api_del_client(struct wlan_objmgr_vdev *vdev,
			u_int8_t *stamac);
int splitmac_api_authorize_client(struct wlan_objmgr_vdev *vdev,
			u_int8_t *stamac, u_int32_t authorize);
int splitmac_api_set_key(struct wlan_objmgr_vdev *vdev, u_int8_t *macaddr,
			u_int8_t cipher, u_int16_t keyix, u_int32_t keylen,
			u_int8_t *keydata);
int splitmac_api_del_key(struct wlan_objmgr_vdev *vdev, u_int8_t *macaddr,
			u_int16_t keyix);
void splitmac_api_set_state(struct wlan_objmgr_peer *peer, int state);
int splitmac_api_get_state(struct wlan_objmgr_peer *peer);

#define splitmac_log(level, args...) \
QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_SPLITMAC, level, ## args)

#define splitmac_logfl(level, format, args...) \
			splitmac_log(level, FL(format), ## args)

#define splitmac_fatal(format, args...) \
	splitmac_logfl(QDF_TRACE_LEVEL_FATAL, format, ## args)
#define splitmac_err(format, args...) \
	splitmac_logfl(QDF_TRACE_LEVEL_ERROR, format, ## args)
#define splitmac_warn(format, args...) \
	splitmac_logfl(QDF_TRACE_LEVEL_WARN, format, ## args)
#define splitmac_info(format, args...) \
	splitmac_logfl(QDF_TRACE_LEVEL_INFO, format, ## args)
#define splitmac_debug(format, args...) \
	splitmac_logfl(QDF_TRACE_LEVEL_DEBUG, format, ## args)

#endif	/* WLAN_SPLITMAC_H */
#endif	/* WLAN_SUPPORT_SPLITMAC */

