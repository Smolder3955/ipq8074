/*
 * Copyright (c) 2016 , 2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 * 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */

/**
 * @file ol_txrx_handles.h
 * @brief typedefs of ol_txrx handles.
 */

#ifndef _OL_IF_TXRX_HANDLES_H
#define _OL_IF_TXRX_HANDLES_H


/**
 * ol_txrx_pdev_handle - opaque handle for txrx physical device
 * object
 */
struct ol_if_txrx_pdev_t;
typedef struct ol_if_txrx_pdev_t *ol_txrx_pdev_handle;

/**
 * ol_txrx_vdev_handle - opaque handle for txrx virtual device
 * object
 */
struct ol_if_txrx_vdev_t;
typedef struct ol_if_txrx_vdev_t *ol_txrx_vdev_handle;

/**
 * ol_pdev_handle - opaque handle for the configuration
 * associated with the physical device
 */
struct ol_if_pdev_t;
typedef struct ol_if_pdev_t *ol_pdev_handle;

/**
 * ol_soc_handle - opaque handle for the configuration
 * associated with the soc
 */
struct ol_if_soc_t;
typedef struct ol_if_soc_t *ol_soc_handle;

/**
 * ol_txrx_peer_handle - opaque handle for txrx peer object
 */
struct ol_if_txrx_peer_t;
typedef struct ol_if_txrx_peer_t *ol_txrx_peer_handle;

#endif

