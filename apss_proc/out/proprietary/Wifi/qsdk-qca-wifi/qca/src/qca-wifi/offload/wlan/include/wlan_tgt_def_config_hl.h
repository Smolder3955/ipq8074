/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef __WLAN_TGT_DEF_CONFIG_H__
#define __WLAN_TGT_DEF_CONFIG_H__

/*
 * TODO: please help to consider if we need a seperate config file from LL case.
 */

/*
 * set of default target config , that can be over written by platform
 */

/*
 * default limit of 8 VAPs per device.
 */
#define CFG_TGT_NUM_VDEV                8
/*
 * We would need 1 AST entry per peer. Scale it by a factor of 2 to minimize hash collisions.
 * TODO: This scaling factor would be taken care inside the WAL in the future.
 */
#define CFG_TGT_NUM_PEER_AST            2

/* # of WDS entries to support.
 */
#define CFG_TGT_WDS_ENTRIES             32

/* MAC DMA burst size. 0: 128B - default, 1: 256B, 2: 64B
 */
#define CFG_TGT_DEFAULT_DMA_BURST_SIZE   0

/* Fixed delimiters to be inserted after every MPDU
 */
#define CFG_TGT_DEFAULT_MAC_AGGR_DELIM   0

/*
 * This value may need to be fine tuned, but a constant value will
 * probably always be appropriate; it is probably not necessary to
 * determine this value dynamically.
 */
#define CFG_TGT_AST_SKID_LIMIT          6
/*
 * total number of peers per device.
 * currently set to 8 to bring up IP3.9 for memory size problem
 */
#define CFG_TGT_NUM_PEERS               8
/*
 * keys per peer node
 */
#define CFG_TGT_NUM_PEER_KEYS           2
/*
 * total number of TX/RX data TIDs 
 */
#define CFG_TGT_NUM_TIDS                (2 * (CFG_TGT_NUM_PEERS + CFG_TGT_NUM_VDEV))
/*
 * number of multicast keys.
 */
#define CFG_TGT_NUM_MCAST_KEYS          8
/*
 * A value of 3 would probably suffice - one for the control stack, one for
 * the data stack, and one for debugging.
 * This value may need to be fine tuned, but a constant value will
 * probably always be appropriate; it is probably not necessary to
 * determine this value dynamically.
 */
#define CFG_TGT_NUM_PDEV_HANDLERS       8
/*
 * A value of 3 would probably suffice - one for the control stack, one for
 * the data stack, and one for debugging.
 * This value may need to be fine tuned, but a constant value will
 * probably always be appropriate; it is probably not necessary to
 * determine this value dynamically.
 */
#define CFG_TGT_NUM_VDEV_HANDLERS       4
/*
 * set this to 8:
 *     one for WAL interals (connection pause)
 *     one for the control stack,
 *     one for the data stack
 *     and one for debugging
 * This value may need to be fine tuned, but a constant value will
 * probably always be appropriate; it is probably not necessary to
 * determine this value dynamically.
 */
#define CFG_TGT_NUM_HANDLERS            14
/*
 * set this to 3: one for the control stack, one for
 * the data stack, and one for debugging.
 * This value may need to be fine tuned, but a constant value will
 * probably always be appropriate; it is probably not necessary to
 * determine this value dynamically.
 */
#define CFG_TGT_NUM_PEER_HANDLERS       32
/*
 * set this to 0x7 (Peregrine = 3 chains).
 * need to be set dynamically based on the HW capability.
 * this is rome
 */
#define CFG_TGT_DEFAULT_TX_CHAIN_MASK   0x3//0x7
/*
 * set this to 0x7 (Peregrine = 3 chains).
 * need to be set dynamically based on the HW capability.
 * this is rome
 */
#define CFG_TGT_DEFAULT_RX_CHAIN_MASK   0x3//0x7
/* 100 ms for video, best-effort, and background */
#define CFG_TGT_RX_TIMEOUT_LO_PRI       100
/* 40 ms for voice*/
#define CFG_TGT_RX_TIMEOUT_HI_PRI       40

/* AR9888 unified is default in ethernet mode */
#define CFG_TGT_RX_DECAP_MODE (0x2)
/* Decap to native Wifi header */
#define CFG_TGT_RX_DECAP_MODE_NWIFI (0x1)

/* maximum number of pending scan requests */
#define CFG_TGT_DEFAULT_SCAN_MAX_REQS   0x4

/* maximum number of scan event handlers */
#define CFG_TGT_DEFAULT_SCAN_MAX_HANDLERS   0x4

/* maximum number of VDEV that could use BMISS offload */
#define CFG_TGT_DEFAULT_BMISS_OFFLOAD_MAX_VDEV   0x2

/* maximum number of VDEV offload Roaming to support */
#define CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_VDEV   0x2

/* maximum number of AP profiles pushed to offload Roaming */
#define CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_PROFILES   0x8

/* default: mcast->ucast disabled */
#if 1
#define CFG_TGT_DEFAULT_NUM_MCAST_GROUPS 0
#define CFG_TGT_DEFAULT_NUM_MCAST_TABLE_ELEMS 0
#define CFG_TGT_DEFAULT_MCAST2UCAST_MODE 0 /* disabled */
#else
/* (for testing) small multicast group membership table enabled */
#define CFG_TGT_DEFAULT_NUM_MCAST_GROUPS 4
#define CFG_TGT_DEFAULT_NUM_MCAST_TABLE_ELEMS 16
#define CFG_TGT_DEFAULT_MCAST2UCAST_MODE 1
#endif

/*
 * Specify how much memory the target should allocate for a debug log of
 * tx PPDU meta-information (how large the PPDU was, when it was sent,
 * whether it was successful, etc.)
 * The size of the log records is configurable, from a minimum of 28 bytes
 * to a maximum of about 300 bytes.  A typical configuration would result
 * in each log record being about 124 bytes.
 * Thus, 1KB of log space can hold about 30 small records, 3 large records,
 * or about 8 typical-sized records.
 */
#define CFG_TGT_DEFAULT_TX_DBG_LOG_SIZE 1024 /* bytes */

/* target based fragment timeout and MPDU duplicate detection */
#define CFG_TGT_DEFAULT_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK 0

/*
 * total number of descriptors to use in the target
 */
#define CFG_TGT_NUM_MSDU_DESC    (8)

/*
 * Maximum number of descriptors to use in the target
 */
#define CFG_TGT_DEFAULT_MAX_PEER_EXT_STATS    16

#endif  /*__WLAN_TGT_DEF_CONFIG_H__ */
