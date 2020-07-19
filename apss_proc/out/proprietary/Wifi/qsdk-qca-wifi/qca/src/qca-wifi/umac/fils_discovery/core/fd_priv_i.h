/*
 * Copyright (c) 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 */

#ifndef _FD_PRIV_I_H_
#define _FD_PRIV_I_H_

#include "fd_defs_i.h"

void fd_ctx_init_da(struct fd_context *fd_ctx);

void fd_ctx_init_ol(struct fd_context *fd_ctx);

void fd_vdev_config_fils(struct wlan_objmgr_vdev* vdev, uint32_t fd_period);

void fd_ctx_deinit(struct fd_context *fd_ctx);

void fd_free_list(struct wlan_objmgr_psoc *psoc, qdf_list_t *fd_deferred_list);
#endif /* _FD_PRIV_I_H_ */

