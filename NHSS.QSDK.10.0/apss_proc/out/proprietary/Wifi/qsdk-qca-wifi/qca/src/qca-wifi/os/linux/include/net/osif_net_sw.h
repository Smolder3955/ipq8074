/*
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * Copyright (c) 2011,2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

/**
 * @ingroup qdf_net_public
 * @file osif_net_sw.h
 * This file defines the device and virtual device switch tables.
 */

#ifndef __QDF_NET_SW_H
#define __QDF_NET_SW_H

#include "osif_net_wcmd.h"

/**
 * @brief per device switch structure
 */
typedef struct _qdf_dev_sw {
	/**
	 * @brief Handler for device open - mandatory interface
	 */
	uint32_t (*drv_open)(qdf_drv_handle_t hdl);
	/**
	 * @brief Handler for device close - mandatory interface
	 */
	void (*drv_close)(qdf_drv_handle_t hdl);
	/**
	 * @brief Handler for transmit - mandatory interface
	 */
	uint32_t (*drv_tx)(qdf_drv_handle_t hdl, /*qdf_nbuf_t HACK*/void* pkt);
	/**
	 * @brief Handler for configuration command - mandatory interface
	 */
	uint32_t (*drv_cmd)(qdf_drv_handle_t hdl, qdf_net_cmd_t cmd,
		qdf_net_cmd_data_t *data);
	/**
	 * @brief Handler for ioctl - mandatory interface
	 */
	uint32_t (*drv_ioctl)(qdf_drv_handle_t hdl, int num,
		void *data);
	/**
	 * @brief Handler for transmission timeout - mandatory interface
	 */
	uint32_t (*drv_tx_timeout)(qdf_drv_handle_t hdl);
	/**
	 * @brief Handler for wireless configuration - optional interface
	 */
	uint32_t (*drv_wcmd)(qdf_drv_handle_t hdl, qdf_net_wcmd_type_t cmd,
		qdf_net_wcmd_data_t *data);
	/**
	 * @brief Handler for polling if polling/deferred processing required -
	 * optional interface
	 */
	qdf_net_poll_resp_t (*drv_poll)(qdf_drv_handle_t hdl, int quota,
		int *work_done);
	/**
	 * @brief Handler for per cpu deffered callback (e.g. for RSS) - optional
	 * interface
	 */
	qdf_net_poll_resp_t (*drv_poll_cpu)(qdf_drv_handle_t hdl, int quota,
		int *work_done, void *arg);
	/**
	 * @brief Handler for disabling receive interrupts for polling.
	 * qdf_drv should do proper locking -
	 * these are not called in atomic context
	 */
	void (*drv_poll_int_disable)(qdf_drv_handle_t hdl);
	/**
	 * @brief Handler for enabling receive interrupts for polling.
	 * qdf_drv should do proper locking -
	 * these are not called in atomic context
	 */
	void (*drv_poll_int_enable)(qdf_drv_handle_t hdl);
} qdf_dev_sw_t;
#endif

