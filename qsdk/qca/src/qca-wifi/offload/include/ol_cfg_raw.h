/*
 * Copyright (c) 2011-2014 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef _OL_CFG_RAW__H_
#define _OL_CFG_RAW__H_

#include <qdf_types.h>

/* Branching optimization compile time configuration for Raw Mode */

/**
 * @brief QCA_RAWMODE_OPTIMIZATION_CONFIG_NORAW - Optimize for non Raw case
 * @details
 *    Set QCA_RAWMODE_OPTIMIZATION_CONFIG to this value to optimize branching
 *    with assumption that Raw mode will not be run time enabled in production.
 */
#define QCA_RAWMODE_OPTIMIZATION_CONFIG_NORAW        0

/**
 * @brief QCA_RAWMODE_OPTIMIZATION_CONFIG_ONLYRAW - Optimize for Raw case
 * @details
 *    Set QCA_RAWMODE_OPTIMIZATION_CONFIG to this value to optimize branching
 *    with assumption that only Raw mode will be run time enabled in production.
 */
#define QCA_RAWMODE_OPTIMIZATION_CONFIG_ONLYRAW      1

/**
 * @brief QCA_RAWMODE_OPTIMIZATION_CONFIG_ONLYRAW - Optimize for mixed case
 * @details
 *    Set QCA_RAWMODE_OPTIMIZATION_CONFIG to this value to optimize branching
 *    with assumption that mixed mode (Raw+Ethernet/Native Wi-Fi) will be
 *    run time enabled in production. This also handles cases where all VAPs
 *    could be Raw, or all VAPs could be Ethernet/Native Wi-Fi, depending on
 *    end admin preference.
 */
#define QCA_RAWMODE_OPTIMIZATION_CONFIG_RAWMIXED     2

/* Note: These macros are not for use in those sections of Mixed VAP
 * implementations which handle final decapsulation.
 * They are for use only in the core data path prior to such decapsulation.
 *
 * Different macros are provided for Tx and Rx directions to provide future
 * flexibility (e.g. if some products mandate that at least one VAP will be in
 * Raw Mode, but others could be in Ethernet/Native-WiFi mode, which will lead
 * to asymmetric likelihoods in Tx and Rx directions for Peregrine).
 */
#if QCA_OL_SUPPORT_RAWMODE_TXRX

#if QCA_RAWMODE_OPTIMIZATION_CONFIG == QCA_RAWMODE_OPTIMIZATION_CONFIG_NORAW
#define OL_CFG_RAW_TX_LIKELINESS(_cond)    qdf_unlikely((_cond))
#define OL_CFG_RAW_RX_LIKELINESS(_cond)    qdf_unlikely((_cond))
#define OL_CFG_NONRAW_TX_LIKELINESS(_cond) qdf_likely((_cond))
#define OL_CFG_NONRAW_RX_LIKELINESS(_cond) qdf_likely((_cond))
#elif QCA_RAWMODE_OPTIMIZATION_CONFIG == QCA_RAWMODE_OPTIMIZATION_CONFIG_ONLYRAW
#define OL_CFG_RAW_TX_LIKELINESS(_cond)    qdf_likely((_cond))
#define OL_CFG_RAW_RX_LIKELINESS(_cond)    qdf_likely((_cond))
#define OL_CFG_NONRAW_TX_LIKELINESS(_cond) qdf_unlikely((_cond))
#define OL_CFG_NONRAW_RX_LIKELINESS(_cond) qdf_unlikely((_cond))
#elif QCA_RAWMODE_OPTIMIZATION_CONFIG == QCA_RAWMODE_OPTIMIZATION_CONFIG_RAWMIXED
#define OL_CFG_RAW_TX_LIKELINESS(_cond)    (_cond)
#define OL_CFG_RAW_RX_LIKELINESS(_cond)    (_cond)
#define OL_CFG_NONRAW_TX_LIKELINESS(_cond) (_cond)
#define OL_CFG_NONRAW_RX_LIKELINESS(_cond) (_cond)
#else
#error "Invalid value for QCA_RAWMODE_OPTIMIZATION_CONFIG"
#endif /* QCA_RAWMODE_OPTIMIZATION_CONFIG */

#else /* QCA_OL_SUPPORT_RAWMODE_TXRX */

#define OL_CFG_RAW_TX_LIKELINESS(_cond)    (0)
#define OL_CFG_RAW_RX_LIKELINESS(_cond)    (0)
#define OL_CFG_NONRAW_TX_LIKELINESS(_cond) (1)
#define OL_CFG_NONRAW_RX_LIKELINESS(_cond) (1)

#endif /* QCA_OL_SUPPORT_RAWMODE_TXRX */

#endif /* _OL_CFG_RAW__H_ */
