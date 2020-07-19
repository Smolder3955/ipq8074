/* AUTO-GENERATED FILE - DO NOT EDIT DIRECTLY */
/*
 * Copyright (c) 2012 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/**
 *   @addtogroup WDIAPI
 *@{
 */
/**
 * @file wdi_types.h
 * @brief Data type definitions used within the WDI API
 * @details
 *  The data type definitions shown below are purely for documentation
 *  reference.
 *  The actual data type definitions are obtained by including the individual
 *  API header files that define them.
 */
#ifndef _WDI_TYPES__H_
#define _WDI_TYPES__H_

#ifdef WDI_TYPES_DIRECT_DEFS

struct ol_pdev_t;
typedef struct ol_pdev_t* ol_pdev_handle;

struct ol_vdev_t;
typedef struct ol_vdev_t* ol_vdev_handle;

struct ol_peer_t;
typedef struct ol_peer_t* ol_peer_handle;

/**
 * @typedef ol_osif_vdev_handle
 * @brief opaque handle for OS shim virtual device object
 */
struct ol_osif_vdev_t;
typedef struct ol_osif_vdev_t* ol_osif_vdev_handle;

#else

/* obtain data type defs from individual API header files */

#include <ol_ctrl_api.h>
#include <ol_osif_api.h>
#include <ol_txrx_api.h>

#endif /* WDI_TYPES_DIRECT_DEFS */

#endif /* _WDI_TYPES__H_ */

/**@}*/
