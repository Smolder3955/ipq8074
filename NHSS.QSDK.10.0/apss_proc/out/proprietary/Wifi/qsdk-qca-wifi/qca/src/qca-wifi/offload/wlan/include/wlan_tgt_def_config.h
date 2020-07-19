/*
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
#ifndef __WLAN_TGT_DEF_CONFIG_H__
#define __WLAN_TGT_DEF_CONFIG_H__

/*
 * set of default target config , that can be over written by platform
 */

/*
 * default limit of 8 VAPs per device.
 */

#define CFG_TGT_MAX_MONITOR_VDEV_QCA6290      	0
#ifdef QCA_LOWMEM_CONFIG
#define CFG_TGT_MAX_MONITOR_VDEV                0
#else
#define CFG_TGT_MAX_MONITOR_VDEV                1
#endif

#ifdef QCA_LOWMEM_PLATFORM
#define CFG_TGT_NUM_VDEV_QCA9888        (4 + CFG_TGT_MAX_MONITOR_VDEV)
#define CFG_TGT_NUM_VDEV_QCA9984        (4 + CFG_TGT_MAX_MONITOR_VDEV)
#define CFG_TGT_NUM_VDEV_AR900B         (4 + CFG_TGT_MAX_MONITOR_VDEV)
#define CFG_TGT_NUM_VDEV_AR988X         (3 + CFG_TGT_MAX_MONITOR_VDEV)
#define CFG_TGT_NUM_VDEV_MESH            4
#else
#define CFG_TGT_NUM_VDEV_QCA9888        (17 + CFG_TGT_MAX_MONITOR_VDEV)
#define CFG_TGT_NUM_VDEV_QCA9984        (16 + CFG_TGT_MAX_MONITOR_VDEV)
#define CFG_TGT_NUM_VDEV_AR900B         (16 + CFG_TGT_MAX_MONITOR_VDEV)
#define CFG_TGT_NUM_VDEV_AR988X         (15 + CFG_TGT_MAX_MONITOR_VDEV)
#define CFG_TGT_NUM_VDEV_MESH            8
#endif

#define CFG_TGT_DEF_MAX_PEER 14

#ifndef QCA_WIFI_QCA8074_VP
#ifdef QCA_LOWMEM_CONFIG
#define CFG_TGT_NUM_VDEV_QCA8074                (5 + CFG_TGT_MAX_MONITOR_VDEV)
#define CFG_TGT_NUM_PEERS_QCA8074_PDEV0         (64 + CFG_TGT_NUM_VDEV_QCA8074)
#define CFG_TGT_NUM_PEERS_QCA8074_PDEV1         (64 + CFG_TGT_NUM_VDEV_QCA8074)
#define CFG_TGT_NUM_PEERS_QCA8074_PDEV2         (64 + CFG_TGT_NUM_VDEV_QCA8074)
#elif defined QCA_512M_CONFIG
#define CFG_TGT_NUM_VDEV_QCA8074                (8 + CFG_TGT_MAX_MONITOR_VDEV)
#define CFG_TGT_NUM_PEERS_QCA8074_PDEV0         (128 + CFG_TGT_NUM_VDEV_QCA8074)
#define CFG_TGT_NUM_PEERS_QCA8074_PDEV1         (128 + CFG_TGT_NUM_VDEV_QCA8074)
#define CFG_TGT_NUM_PEERS_QCA8074_PDEV2         (128 + CFG_TGT_NUM_VDEV_QCA8074)
#else
#define CFG_TGT_NUM_VDEV_QCA8074                (16 + CFG_TGT_MAX_MONITOR_VDEV)
#define CFG_TGT_NUM_PEERS_QCA8074_PDEV0         (512 + CFG_TGT_NUM_VDEV_QCA8074)
#define CFG_TGT_NUM_PEERS_QCA8074_PDEV1         (512 + CFG_TGT_NUM_VDEV_QCA8074)
#define CFG_TGT_NUM_PEERS_QCA8074_PDEV2         (512 + CFG_TGT_NUM_VDEV_QCA8074)
#endif
#else
#define CFG_TGT_NUM_VDEV_QCA8074                4
#define CFG_TGT_NUM_PEERS_QCA8074_PDEV0         (4 + CFG_TGT_NUM_VDEV_QCA8074)
#define CFG_TGT_NUM_PEERS_QCA8074_PDEV1         (4 + CFG_TGT_NUM_VDEV_QCA8074)
#define CFG_TGT_NUM_PEERS_QCA8074_PDEV2         (4 + CFG_TGT_NUM_VDEV_QCA8074)
#endif

#define CFG_TGT_NUM_PEERS_QCA8074               (CFG_TGT_NUM_PEERS_QCA8074_PDEV0 + CFG_TGT_NUM_PEERS_QCA8074_PDEV1)
#define CFG_TGT_NUM_PEERS_QCA8074_3M            (CFG_TGT_NUM_PEERS_QCA8074_PDEV0 + CFG_TGT_NUM_PEERS_QCA8074_PDEV1 + CFG_TGT_NUM_PEERS_QCA8074_PDEV2)
#define CFG_TGT_NUM_OFFLOAD_PEERS_QCA8074       4

#define CFG_TGT_NUM_VDEV_QCA6290                1
#define CFG_TGT_NUM_PEERS_QCA6290_PDEV0         8
#define CFG_TGT_NUM_PEERS_QCA6290_PDEV1         8
#define CFG_TGT_NUM_TIDS_QCA6290                (8 * CFG_TGT_NUM_PEERS_QCA6290 + 4 * CFG_TGT_NUM_VDEV_QCA6290 + 8 )
#define CFG_TGT_NUM_PEERS_QCA6290               (CFG_TGT_NUM_PEERS_QCA6290_PDEV0 + CFG_TGT_NUM_PEERS_QCA6290_PDEV1)
#define CFG_TGT_NUM_OFFLOAD_PEERS_QCA6290       4

#ifndef QCA_WIFI_QCA8074_VP
#define CFG_TGT_NUM_TIDS_QCA8074         (8 * CFG_TGT_NUM_PEERS_QCA8074 + 4 * CFG_TGT_NUM_VDEV_QCA8074 + 8 )
#define CFG_TGT_NUM_BCN_OFFLOAD_VDEV     16
#else
#define CFG_TGT_NUM_TIDS_QCA8074         (2 * CFG_TGT_NUM_PEERS_QCA8074 + 4 * CFG_TGT_NUM_VDEV_QCA8074 + 8 )
#define CFG_TGT_NUM_BCN_OFFLOAD_VDEV     4
#endif

#define CFG_TGT_TWT_AP_PDEV_COUNT         2
#define CFG_TGT_TWT_AP_STA_COUNT       1000
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
#define CFG_TGT_AST_SKID_LIMIT          32

#define CFG_TGT_AST_SKID_LIMIT_QCA8074          16
/*
 * Before moving to common host for all targets, peregrine had
 * ast skid length set to 128. Now we are increasing it to 40 from 32
 */
#define CFG_TGT_AST_SKID_LIMIT_AR988X      40

/*
 * default number of peers per device.
 */
#define CFG_TGT_NUM_PEERS               32

/*
 * max number of peers per device.
 */

/*
 * In offload mode target supports features like WOW, chatter and other
 * protocol offloads. In order to support them some functionalities like
 * reorder buffering, PN checking need to be done in target. This determines
 * maximum number of peers suported by target in offload mode
 */
#define CFG_TGT_NUM_OFFLOAD_PEERS       0

/*
 * Number of reorder buffers used in offload mode
 */
#define CFG_TGT_NUM_OFFLOAD_REORDER_BUFFS   0
#define CFG_TGT_NUM_OFFLOAD_REORDER_BUFFS_QCA8074   4

#if defined(QCA_LOWMEM_PLATFORM) || defined(QCA_WIFI_QCA8074_VP)
/*
 * Smart Antenna module require additional memory in firmware,
 * We can supprot 32 stations.
 *
 */
#define CFG_TGT_NUM_SMART_ANT_PEERS_MAX 32
/*
 * RTT module require additional memory in firmware
 */
#define CFG_TGT_NUM_RTT_PEERS_MAX  32

#define CFG_TGT_NUM_PEERS_MAX  32
#else
/*
 * Smart Antenna module require additional memory in firmware,
 * We can supprot 115 stations.
 *
 */
#define CFG_TGT_NUM_SMART_ANT_PEERS_MAX 115
/*
 * RTT module require additional memory in firmware
 */
#define CFG_TGT_NUM_RTT_PEERS_MAX  128

#define CFG_TGT_NUM_PEERS_MAX  128
#endif

#if PEER_CACHEING_HOST_ENABLE
#ifdef QCA_LOWMEM_PLATFORM

#define CFG_TGT_QCACHE_ACTIVE_PEERS   20

#define CFG_TGT_QCACHE_MIN_ACTIVE_PEERS    16

#define CFG_TGT_NUM_QCACHE_PEERS_MAX_LOW_MEM  33
#define CFG_TGT_NUM_QCACHE_PEERS_MAX  33
/* For VoW */
#define CFG_TGT_QCACHE_NUM_PEERS_VOW  33

#else
#define CFG_TGT_NUM_QCACHE_PEERS_MAX_LOW_MEM  129
#define CFG_TGT_NUM_QCACHE_PEERS_MAX  513

#if PEER_FLOW_CONTROL
#define CFG_TGT_QCACHE_ACTIVE_PEERS    35
#else
#define CFG_TGT_QCACHE_ACTIVE_PEERS    50
#endif

#define CFG_TGT_QCACHE_MIN_ACTIVE_PEERS    26
/* For VoW */
#define CFG_TGT_QCACHE_NUM_PEERS_VOW  128
#endif
#endif

#define CFG_TGT_MAX_AST_TABLE_SIZE  4096
/*
 * keys per peer node
 */
#define CFG_TGT_NUM_PEER_KEYS           2
/*
 * total number of data TX and RX TIDs
 */
#define CFG_TGT_NUM_TIDS                (2 * CFG_TGT_NUM_PEERS)
/*
 * max number of Tx TIDS
 */
#define CFG_TGT_NUM_TIDS_MAX            (2 * CFG_TGT_NUM_PEERS_MAX)
/*
 * set this to 0x7 (Peregrine = 3 chains).
 * need to be set dynamically based on the HW capability.
 */
#define CFG_TGT_DEFAULT_TX_CHAIN_MASK_4SS 0xf
#define CFG_TGT_DEFAULT_RX_CHAIN_MASK_4SS 0xf
#define CFG_TGT_DEFAULT_TX_CHAIN_MASK_3SS 0x7
#define CFG_TGT_DEFAULT_RX_CHAIN_MASK_3SS 0x7
#define CFG_TGT_DEFAULT_TX_CHAIN_MASK_2SS 0x3
#define CFG_TGT_DEFAULT_RX_CHAIN_MASK_2SS 0x3

/* 100 ms for video, best-effort, and background */
#define CFG_TGT_RX_TIMEOUT_LO_PRI       100
/* 40 ms for voice*/
#define CFG_TGT_RX_TIMEOUT_HI_PRI       40

/* AR9888 unified is default in ethernet mode */
#define CFG_TGT_RX_DECAP_MODE (0x2)
/* Decap to native Wifi header */
#define CFG_TGT_RX_DECAP_MODE_NWIFI (0x1)
/* Decap to raw mode header */
#define CFG_TGT_RX_DECAP_MODE_RAW   (0x0)


/* maximum number of pending scan requests */
#define CFG_TGT_DEFAULT_SCAN_MAX_REQS   0x4

/* maximum number of VDEV that could use BMISS offload */
#define CFG_TGT_DEFAULT_BMISS_OFFLOAD_MAX_VDEV   0x3

/* maximum number of VDEV offload Roaming to support */
#define CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_VDEV   0x3

/* maximum number of AP profiles pushed to offload Roaming */
#define CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_PROFILES   0x8

/* maximum number of VDEV offload GTK to support */
#define CFG_TGT_DEFAULT_GTK_OFFLOAD_MAX_VDEV   0x3
/* default: mcast->ucast disabled if ATH_SUPPORT_MCAST2UCAST not defined */
#ifndef ATH_SUPPORT_MCAST2UCAST
#define CFG_TGT_DEFAULT_NUM_MCAST_GROUPS 0
#define CFG_TGT_DEFAULT_NUM_MCAST_TABLE_ELEMS 0
#define CFG_TGT_DEFAULT_MCAST2UCAST_MODE 0 /* disabled */
#else
/* (for testing) small multicast group membership table enabled */
#define CFG_TGT_DEFAULT_NUM_MCAST_GROUPS 12
#define CFG_TGT_DEFAULT_NUM_MCAST_TABLE_ELEMS 64
#define CFG_TGT_DEFAULT_MCAST2UCAST_MODE 2
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
#define CFG_TGT_DEFAULT_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK 1

/* Configuration for VoW
 * VoW requires dedicated resources reserved for VI links. As resources
 * are limited in the target, it requires to relook into the other resources
 * and optimize the allocation of them in the system. Below configuration
 * tries to adjust the system resources allocation based on the current
 * customer reqirements for Video soultions (Carrier requirements)
 */

/*  Default VoW configuration (No of VI Nodes, Descs per VI Node)
 */
#define CFG_TGT_DEFAULT_VOW_CONFIG   0

#if defined(CONFIG_AR900B_SUPPORT)
#ifdef QCA_LOWMEM_PLATFORM
/*
 * No of VAPs per devices
 */
#define CFG_TGT_NUM_VDEV_VOW        4

/* No of WDS entries to support
    */
#define CFG_TGT_WDS_ENTRIES_VOW     4

/*
 * total number of peers per device
 */
#define CFG_TGT_NUM_PEERS_VOW         4

/*
 * total number active of peers
 */
#define CFG_TGT_NUM_ACTIVE_PEERS_VOW  4
#else
/*
 * No of VAPs per devices
 */
#define CFG_TGT_NUM_VDEV_VOW        16

/* No of WDS entries to support
    */
#define CFG_TGT_WDS_ENTRIES_VOW     16

/*
 * total number of peers per device
 */
#define CFG_TGT_NUM_PEERS_VOW         16

/*
 * total number active of peers
 */
#define CFG_TGT_NUM_ACTIVE_PEERS_VOW  16
#endif
#else
/*
 * No of VAPs per devices
 */
#define CFG_TGT_NUM_VDEV_VOW        16

/* No of WDS entries to support
    */
#define CFG_TGT_WDS_ENTRIES_VOW     16

/*
 * total number of peers per device
 */
#define CFG_TGT_NUM_PEERS_VOW       16

/*
 * total number active of peers
 */
#define CFG_TGT_NUM_ACTIVE_PEERS_VOW  4
#endif
/*
 * total number of data TX and RX TIDs
 */
#define CFG_TGT_NUM_TIDS_VOW        (2 * (CFG_TGT_NUM_PEERS_VOW + CFG_TGT_NUM_VDEV_VOW))


/*
 * total number of descriptors to use in the target
 */
#if PEER_FLOW_CONTROL
#if QCA_PARTNER_DIRECTLINK_TX
#define CFG_TGT_NUM_MSDU_DESC_AR900B    (2500)
#else
#define CFG_TGT_NUM_MSDU_DESC_AR900B    (2500)
#endif /* QCA_PARTNER_DIRECTLINK_TX */
#else
#define CFG_TGT_NUM_MSDU_DESC_AR900B    (1024 + 400)
#endif

#define CFG_TGT_NUM_MSDU_DESC_AR988X    (1024 + 400)

/*
 * total number of max descriptors user can configure.
 * this is derived from the below calculation.
 * 1 active peer consumes 780 bytes
 * 1 descriptor consumes 16 bytes
 * for every one less Active peer in target we get 48 descriptors
 * to have some cushion we set this to 46 descriptors
 * At most we can let go 16 Active peers, this allows us to
 * have 16 * 46 = 736 descriptors in target.
 */
#define CFG_TGT_NUM_MAX_MSDU_DESC_AR900B (CFG_TGT_NUM_MSDU_DESC_AR900B + \
                                          ((CFG_TGT_QCACHE_ACTIVE_PEERS - CFG_TGT_QCACHE_MIN_ACTIVE_PEERS) \
                                          * CFG_TGT_NUM_MSDU_DESC_PER_ACTIVE_PEER))

/*
 * number of extra descriptors that can be added to target
 * by removing one active peer from target.
 */
#define CFG_TGT_NUM_MSDU_DESC_PER_ACTIVE_PEER	46

/*
 * Maximum number of descriptors to use in the target
 */
#define CFG_TGT_DEFAULT_MAX_PEER_EXT_STATS    16

#if QCA_AIRTIME_FAIRNESS
/*
 * No of VAPs per devices
 */
/*#define CFG_TGT_NUM_VDEV_ATF        8*/
#ifdef QCA_LOWMEM_PLATFORM
/*
 * total number of peers per device
 */
#define CFG_TGT_NUM_PEERS_ATF      5
#else
/*
 * total number of peers per device
 */
#define CFG_TGT_NUM_PEERS_ATF      50
#endif
/*
 *No of WDS entries to support
 */
/*#define CFG_TGT_WDS_ENTRIES_ATF     16*/

/*
 * total number of data TX and RX TIDs
 */
/*#define CFG_TGT_NUM_TIDS_ATF     (2 * (CFG_TGT_NUM_PEERS_ATF + CFG_TGT_NUM_VDEV_ATF))*/

#define CFG_TGT_NUM_MSDU_DESC_ATF   2496

#endif

#if ATH_SUPPORT_WRAP

#define CFG_TGT_NUM_WRAP_VDEV_AR988X            16
#define CFG_TGT_NUM_WRAP_PEERS_MAX_AR988X       14

#if PEER_FLOW_CONTROL
/*
 * Due to firmware memory optimization issue, we decrease max VDEV supported to 24 for QWRAP.
 * Once memory optimization issue is resolved, we will revisit these numbers
 */
#define CFG_TGT_NUM_WRAP_VDEV_AR900B            24  /* Max 28 proxy sta vaps + 1 main sta vap + 1 ap vap */
#define CFG_TGT_NUM_WRAP_PEERS_MAX_AR900B       22  /* Max 28 wireless clients */
#define CFG_TGT_NUM_WRAP_VDEV_QCA9984           30  /* Max 28 proxy sta vaps + 1 main sta vap + 1 ap vap */
#define CFG_TGT_NUM_WRAP_PEERS_MAX_QCA9984      28  /* Max 28 wireless clients */
#define CFG_TGT_NUM_WRAP_VDEV_IPQ4019           30  /* Max 28 proxy sta vaps + 1 main sta vap + 1 ap vap */
#define CFG_TGT_NUM_WRAP_VDEV_IPQ8074           30  /* Max 28 proxy sta vaps + 1 main sta vap + 1 ap vap */
#define CFG_TGT_NUM_WRAP_PEERS_MAX_IPQ4019      28  /* Max 28 wireless clients */
#define CFG_TGT_NUM_WRAP_PEERS_MAX_IPQ8074      28  /* Max 28 wireless clients */
#else
#define CFG_TGT_NUM_WRAP_VDEV_AR900B            19
#define CFG_TGT_NUM_WRAP_PEERS_MAX_AR900B       17  /* Max 17 wireless clients */
#define CFG_TGT_NUM_WRAP_VDEV_QCA9984           19
#define CFG_TGT_NUM_WRAP_PEERS_MAX_QCA9984      17  /* Max 17 wireless clients */
#endif

#endif


#endif  /*__WLAN_TGT_DEF_CONFIG_H__ */
