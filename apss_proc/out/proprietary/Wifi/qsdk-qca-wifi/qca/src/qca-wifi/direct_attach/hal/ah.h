/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting, Atheros
 * Communications, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 *
 */

#ifndef _ATH_AH_H_
#define _ATH_AH_H_
/*
 * Atheros Hardware Access Layer
 *
 * Clients of the HAL call ath_hal_attach to obtain a reference to an ath_hal
 * structure for use with the device.  Hardware-related operations that
 * follow must call back into the HAL through interface, supplying the
 * reference as the first parameter.
 */

#include "wlan_opts.h"
#include "ah_osdep.h"

/*
 * __ahdecl is analogous to _cdecl; it defines the calling
 * convention used within the HAL.  For most systems this
 * can just default to be empty and the compiler will (should)
 * use _cdecl.  For systems where _cdecl is not compatible this
 * must be defined.  See linux/ah_osdep.h for an example.
 */
#ifndef __ahdecl
#define __ahdecl
#endif

#if ATH_SUPPORT_WIRESHARK
#include "ah_radiotap.h" /* ah_rx_radiotap_header */
#endif

/*
 * Status codes that may be returned by the HAL.  Note that
 * interfaces that return a status code set it only when an
 * error occurs--i.e. you cannot check it for success.
 */
typedef enum {
    HAL_OK          = 0,    /* No error */
    HAL_ENXIO       = 1,    /* No hardware present */
    HAL_ENOMEM      = 2,    /* Memory allocation failed */
    HAL_EIO         = 3,    /* Hardware didn't respond as expected */
    HAL_EEMAGIC     = 4,    /* EEPROM magic number invalid */
    HAL_EEVERSION   = 5,    /* EEPROM version invalid */
    HAL_EELOCKED    = 6,    /* EEPROM unreadable */
    HAL_EEBADSUM    = 7,    /* EEPROM checksum invalid */
    HAL_EEREAD      = 8,    /* EEPROM read problem */
    HAL_EEBADMAC    = 9,    /* EEPROM mac address invalid */
    HAL_EESIZE      = 10,   /* EEPROM size not supported */
    HAL_EEWRITE     = 11,   /* Attempt to change write-locked EEPROM */
    HAL_EINVAL      = 12,   /* Invalid parameter to function */
    HAL_ENOTSUPP    = 13,   /* Hardware revision not supported */
    HAL_ESELFTEST   = 14,   /* Hardware self-test failed */
    HAL_EINPROGRESS = 15,   /* Operation incomplete */
    HAL_FULL_RESET  = 16,   /* Full reset done */
    HAL_INV_PMODE   = 17,
    HAL_CAL_TEST    = 18,   /* Calibration test success during reset */
} HAL_STATUS;

/*
 * HAL_BOOL is deprecated, but still currently in use.
 * Provide definitions for HAL_BOOL, AH_FALSE, and AH_TRUE that are
 * compatible with the preferred bool, true, and false.
 * Once all references to HAL_BOOL, AH_FALSE, and AH_TRUE are removed,
 * these deprecation-transition defintions should be removed.
 */
#define HAL_BOOL bool
#define AH_FALSE false
#define AH_TRUE  true

/*
 * Device revision information.
 */
typedef enum {
    HAL_MAC_MAGIC,          /* MAC magic number */
    HAL_MAC_VERSION,        /* MAC version id */
    HAL_MAC_REV,            /* MAC revision */
    HAL_PHY_REV,            /* PHY revision */
    HAL_ANALOG5GHZ_REV,     /* 5GHz radio revision */
    HAL_ANALOG2GHZ_REV,     /* 2GHz radio revision */
} HAL_DEVICE_INFO;

typedef enum {
    HAL_CAP_REG_DMN                       = 0,  /* current regulatory domain */
    HAL_CAP_CIPHER                        = 1,  /* hardware supports cipher */
    HAL_CAP_TKIP_MIC                      = 2,  /* handle TKIP MIC in hardware */
    HAL_CAP_TKIP_SPLIT                    = 3,  /* hardware TKIP uses split keys */
    HAL_CAP_PHYCOUNTERS                   = 4,  /* hardware PHY error counters */
    HAL_CAP_DIVERSITY                     = 5,  /* hardware supports fast diversity */
    HAL_CAP_KEYCACHE_SIZE                 = 6,  /* number of entries in key cache */
    HAL_CAP_NUM_TXQUEUES                  = 7,  /* number of hardware xmit queues */
    HAL_CAP_VEOL                          = 9,  /* hardware supports virtual EOL */
    HAL_CAP_PSPOLL                        = 10, /* hardware has working PS-Poll support */
    HAL_CAP_DIAG                          = 11, /* hardware diagnostic support */
    HAL_CAP_BURST                         = 13, /* hardware supports packet bursting */
    HAL_CAP_FASTFRAME                     = 14, /* hardware supoprts fast frames */
    HAL_CAP_TXPOW                         = 15, /* global tx power limit  */
    HAL_CAP_TPC                           = 16, /* per-packet tx power control  */
    HAL_CAP_PHYDIAG                       = 17, /* hardware phy error diagnostic */
    HAL_CAP_BSSIDMASK                     = 18, /* hardware supports bssid mask */
    HAL_CAP_MCAST_KEYSRCH                 = 19, /* hardware has multicast key search */
    HAL_CAP_TSF_ADJUST                    = 20, /* hardware has beacon tsf adjust */
    HAL_CAP_XR                            = 21, /* hardware has XR support  */
    HAL_CAP_WME_TKIPMIC                   = 22, /* hardware can support TKIP MIC when WMM is turned on */
    HAL_CAP_CHAN_HALFRATE                 = 23, /* hardware can support half rate channels */
    HAL_CAP_CHAN_QUARTERRATE              = 24, /* hardware can support quarter rate channels */
    HAL_CAP_RFSILENT                      = 25, /* hardware has rfsilent support  */
    HAL_CAP_TPC_ACK                       = 26, /* ack txpower with per-packet tpc */
    HAL_CAP_TPC_CTS                       = 27, /* cts txpower with per-packet tpc */
    HAL_CAP_11D                           = 28, /* 11d beacon support for changing cc */
    HAL_CAP_PCIE_PS                       = 29, /* pci express power save */
    HAL_CAP_HT                            = 30, /* hardware can support HT */
    HAL_CAP_GTT                           = 31, /* hardware supports global transmit timeout */
    HAL_CAP_FAST_CC                       = 32, /* hardware supports global transmit timeout */
    HAL_CAP_TX_CHAINMASK                  = 33, /* number of tx chains */
    HAL_CAP_RX_CHAINMASK                  = 34, /* number of rx chains */
    HAL_CAP_TX_TRIG_LEVEL_MAX             = 35, /* maximum Tx trigger level */
    HAL_CAP_NUM_GPIO_PINS                 = 36, /* number of GPIO pins */
    HAL_CAP_WOW                           = 37, /* WOW support */       
    HAL_CAP_CST                           = 38, /* hardware supports carrier sense timeout interrupt */
    HAL_CAP_RIFS_RX                       = 39, /* hardware supports RIFS receive */
    HAL_CAP_RIFS_TX                       = 40, /* hardware supports RIFS transmit */
    HAL_CAP_FORCE_PPM                     = 41, /* Force PPM */
    HAL_CAP_RTS_AGGR_LIMIT                = 42, /* aggregation limit with RTS */
    HAL_CAP_4ADDR_AGGR                    = 43, /* hardware is capable of 4addr aggregation */
    HAL_CAP_DFS_DMN                       = 44, /* DFS domain */
    HAL_CAP_EXT_CHAN_DFS                  = 45, /* DFS support for extension channel */
    HAL_CAP_COMBINED_RADAR_RSSI           = 46, /* Is combined RSSI for radar accurate */
    HAL_CAP_HW_BEACON_PROC_SUPPORT        = 47, /* hardware supports hw beacon processing */
    HAL_CAP_AUTO_SLEEP                    = 48, /* hardware can go to network sleep automatically after waking up to receive TIM */
    HAL_CAP_MBSSID_AGGR_SUPPORT           = 49, /* Support for mBSSID Aggregation */
    HAL_CAP_4KB_SPLIT_TRANS               = 50, /* hardware is capable of splitting PCIe transanctions on 4KB boundaries */
    HAL_CAP_REG_FLAG                      = 51, /* Regulatory domain flags */
    HAL_CAP_BB_RIFS_HANG                  = 52, /* BB may hang due to RIFS */
    HAL_CAP_RIFS_RX_ENABLED               = 53, /* RIFS RX currently enabled */
    HAL_CAP_BB_DFS_HANG                   = 54, /* BB may hang due to DFS */
    HAL_CAP_WOW_MATCH_EXACT               = 55, /* WoW exact match pattern support */
    HAL_CAP_ANT_CFG_2GHZ                  = 56, /* Number of antenna configurations */
    HAL_CAP_ANT_CFG_5GHZ                  = 57, /* Number of antenna configurations */
    HAL_CAP_RX_STBC                       = 58,
    HAL_CAP_TX_STBC                       = 59,
    HAL_CAP_BT_COEX                       = 60, /* hardware is capable of bluetooth coexistence */
    HAL_CAP_DYNAMIC_SMPS                  = 61, /* Dynamic MIMO Power Save hardware support */
    HAL_CAP_WOW_MATCH_DWORD               = 62, /* Wow pattern match first dword */
    HAL_CAP_WPS_PUSH_BUTTON               = 63, /* hardware has WPS push button */
    HAL_CAP_WEP_TKIP_AGGR                 = 64, /* hw is capable of aggregation with WEP/TKIP */
    HAL_CAP_WEP_TKIP_AGGR_TX_DELIM        = 65, /* number of delimiters required by hw when transmitting aggregates with WEP/TKIP */
    HAL_CAP_WEP_TKIP_AGGR_RX_DELIM        = 66, /* number of delimiters required by hw when receiving aggregates with WEP/TKIP */
    HAL_CAP_DS                            = 67, /* hardware support double stream: HB93 1x2 only support single stream */
    HAL_CAP_BB_RX_CLEAR_STUCK_HANG        = 68, /* BB may hang due to Rx Clear Stuck Low */
    HAL_CAP_MAC_HANG                      = 69, /* MAC may hang */
    HAL_CAP_MFP                           = 70, /* Management Frame Proctection in hardware */
    HAL_CAP_DEVICE_TYPE                   = 71, /* Type of device */
    HAL_CAP_TS                            = 72, /* hardware supports three streams */
    HAL_INTR_MITIGATION_SUPPORTED         = 73, /* interrupt mitigation */
    HAL_CAP_MAX_TKIP_WEP_HT_RATE          = 74, /* max HT TKIP/WEP rate HW supports */
    HAL_CAP_ENHANCED_DMA_SUPPORT          = 75, /* hardware supports DMA enhancements */
    HAL_CAP_NUM_TXMAPS                    = 76, /* Number of buffers in a transmit descriptor */
    HAL_CAP_TXDESCLEN                     = 77, /* Length of transmit descriptor */
    HAL_CAP_TXSTATUSLEN                   = 78, /* Length of transmit status descriptor */
    HAL_CAP_RXSTATUSLEN                   = 79, /* Length of transmit status descriptor */
    HAL_CAP_RXFIFODEPTH                   = 80, /* Receive hardware FIFO depth */
    HAL_CAP_RXBUFSIZE                     = 81, /* Receive Buffer Length */
    HAL_CAP_NUM_MR_RETRIES                = 82, /* limit on multirate retries */
    HAL_CAP_GEN_TIMER                     = 83, /* Generic(TSF) timer */
    HAL_CAP_OL_PWRCTRL                    = 84, /* open-loop power control */
    HAL_CAP_MAX_WEP_TKIP_HT20_TX_RATEKBPS = 85,
    HAL_CAP_MAX_WEP_TKIP_HT40_TX_RATEKBPS = 86,
    HAL_CAP_MAX_WEP_TKIP_HT20_RX_RATEKBPS = 87,
    HAL_CAP_MAX_WEP_TKIP_HT40_RX_RATEKBPS = 88,
#if ATH_SUPPORT_WAPI
    HAL_CAP_WAPI_MIC                      = 89, /* handle WAPI MIC in hardware */
#endif
#if WLAN_SPECTRAL_ENABLE
    HAL_CAP_SPECTRAL_SCAN                 = 90,
#endif
    HAL_CAP_CFENDFIX                      = 91,
    HAL_CAP_BB_PANIC_WATCHDOG             = 92,
    HAL_CAP_BT_COEX_ASPM_WAR              = 93,
    HAL_CAP_EXTRADELIMWAR                 = 94,
    HAL_CAP_PROXYSTA                      = 95,
    HAL_CAP_HT20_SGI                      = 96,
#if ATH_SUPPORT_RAW_ADC_CAPTURE
    HAL_CAP_RAW_ADC_CAPTURE               = 97,
#endif
#ifdef  ATH_SUPPORT_TxBF
    HAL_CAP_TXBF                          = 98, /* tx beamforming */
#endif
    HAL_CAP_LDPC                 = 99,
    HAL_CAP_RXDESC_TIMESTAMPBITS = 100,
    HAL_CAP_RXTX_ABORT           = 101,
    HAL_CAP_ANI_POLL_INTERVAL    = 102, /* ANI poll interval in ms */
    HAL_CAP_PAPRD_ENABLED        = 103, /* PA Pre-destortion enabled */
    HAL_CAP_HW_UAPSD_TRIG        = 104, /* hardware UAPSD trigger support */
    HAL_CAP_ANT_DIV_COMB         = 105,   /* Enable antenna diversity/combining */ 
    HAL_CAP_PHYRESTART_CLR_WAR   = 106, /* in some cases, clear phy restart to fix bb hang */
    HAL_CAP_ENTERPRISE_MODE      = 107, /* Enterprise mode features */
    HAL_CAP_LDPCWAR              = 108, /* disable RIFS when both RIFS and LDPC is enabled to fix bb hang */
    HAL_CAP_CHANNEL_SWITCH_TIME_USEC = 109, /* Channel change time  in microseconds; Used by ResMgr Off-Channel Scheduler */
    HAL_CAP_ENABLE_APM           = 110, /* APM enabled */
    HAL_CAP_PCIE_LCR_EXTSYNC_EN  = 111, /* Extended Sycn bit in LCR. */
    HAL_CAP_PCIE_LCR_OFFSET      = 112, /* PCI-E LCR offset*/
#if ATH_SUPPORT_DYN_TX_CHAINMASK
    HAL_CAP_DYN_TX_CHAINMASK     = 113, /* Dynamic Tx Chainmask. */
#endif /* ATH_SUPPORT_DYN_TX_CHAINMASK */
#if ATH_SUPPORT_WAPI
    HAL_CAP_WAPI_MAX_TX_CHAINS   = 114, /* MAX number of tx chains supported by WAPI engine */
    HAL_CAP_WAPI_MAX_RX_CHAINS   = 115, /* MAX number of rx chains supported by WAPI engine */
#endif
    HAL_CAP_OFDM_RTSCTS          = 116, /* The capability to use OFDM rate for RTS/CTS */
#ifdef ATH_SUPPORT_DFS
    HAL_CAP_ENHANCED_DFS_SUPPORT = 117, /* hardware supports DMA enhancements */
#endif
    HAL_CAP_MCI                  = 118, /* MCI coex support */
    HAL_CAP_SMARTANTENNA         = 119, /* Smart Antenna Capabilty */
    HAL_CAP_WOW_GTK_OFFLOAD      = 120, /* GTK rekeying offload support in Wake on Wireless mode */
    HAL_CAP_RADIO_RETENTION      = 121, /* Calibration radio retention */
    HAL_CAP_LMAC_SMART_ANT       = 125, /* Lmac Smart Antenna Capability */
    HAL_CAP_WOW_ARP_OFFLOAD      = 126, /* WOW ARP offload */
    HAL_CAP_WOW_NS_OFFLOAD       = 127, /* WOW NS offload */
    HAL_CAP_WOW_4WAY_HS_WAKEUP   = 128, /* WOW 4WAY handshake wakeup */
    HAL_CAP_WOW_ACER_MAGIC       = 129, /* WOW Acer magic packet wakeup */
    HAL_CAP_WOW_ACER_SWKA        = 130, /* WOW Acer Software keep alive */
#ifdef ATH_TRAFFIC_FAST_RECOVER
    HAL_CAP_TRAFFIC_FAST_RECOVER = 131, 
#endif
    HAL_CAP_TX_DIVERSITY         = 132, /* Dynamic Tx Diversity. */
    HAL_CAP_CRDC                 = 133, /* Chain Rssi Difference Compensation */
#if WLAN_SPECTRAL_ENABLE
    HAL_CAP_ADVNCD_SPECTRAL_SCAN = 134, /* Advanced Spectral Scan capability
                                           (as implemented on 11ac chipsets onwards) */
#endif
#if ATH_SUPPORT_WRAP
    HAL_CAP_PROXYSTARXWAR        = 135, /* WAR for 3SS Rx issue EV131645 */
    HAL_CAP_WRAP_HW_DECRYPT      = 136, /* HW Decryption for WRAP feature*/
    HAL_CAP_WRAP_PROMISC         = 137, /* Promiscuous mode for WRAP feature*/
#endif
    HAL_CAP_PN_CHECK_WAR_SUPPORT = 138, /* PN check WAR support for this chip */
    HAL_CAP_RXDEAF_WAR_NEEDED    = 139, /* If WAR for Rx Deaf issue is needed for this chip */
    HAL_CAP_DCS_SUPPORT          = 140,  /* DCS support for this chip */ 
} HAL_CAPABILITY_TYPE;

typedef enum {
    HAL_CAP_RADAR    = 0,        /* Radar capability */
    HAL_CAP_AR       = 1,        /* AR capability */
    HAL_CAP_SPECTRAL = 2,        /* Spectral scan capability*/
} HAL_PHYDIAG_CAPS;

/*
 * "States" for setting the LED.  These correspond to
 * the possible 802.11 operational states and there may
 * be a many-to-one mapping between these states and the
 * actual hardware states for the LED's (i.e. the hardware
 * may have fewer states).
 *
 * State HAL_LED_SCAN corresponds to external scan, and it's used inside
 * the LED module only.
 */
typedef enum {
    HAL_LED_RESET   = 0,
    HAL_LED_INIT    = 1,
    HAL_LED_READY   = 2,
    HAL_LED_SCAN    = 3,
    HAL_LED_AUTH    = 4,
    HAL_LED_ASSOC   = 5,
    HAL_LED_RUN     = 6
} HAL_LED_STATE;

/*
 * Transmit queue types/numbers.  These are used to tag
 * each transmit queue in the hardware and to identify a set
 * of transmit queues for operations such as start/stop dma.
 */
typedef enum {
    HAL_TX_QUEUE_INACTIVE   = 0,        /* queue is inactive/unused */
    HAL_TX_QUEUE_DATA       = 1,        /* data xmit q's */
    HAL_TX_QUEUE_BEACON     = 2,        /* beacon xmit q */
    HAL_TX_QUEUE_CAB        = 3,        /* "crap after beacon" xmit q */
    HAL_TX_QUEUE_UAPSD      = 4,        /* u-apsd power save xmit q */
    HAL_TX_QUEUE_PSPOLL     = 5,        /* power-save poll xmit q */
    HAL_TX_QUEUE_CFEND      = 6,         /* CF END queue to send cf-end frames*/ 
    HAL_TX_QUEUE_PAPRD      = 7,         /* PAPRD queue to send PAPRD traningframes*/ 
#if ATH_SUPPORT_WIFIPOS     
    HAL_TX_QUEUE_WIFIPOS_OC        = 8,         /* WIFIPOS queue to send probe frames */
    HAL_TX_QUEUE_WIFIPOS_HC     = 9         /* WIFIPOS queue to send HC probe frames */
#endif

} HAL_TX_QUEUE;

#define    HAL_NUM_TX_QUEUES    10        /* max possible # of queues */
#define    HAL_NUM_DATA_QUEUES  4

#ifndef AH_MAX_CHAINS
#define AH_MAX_CHAINS 3
#endif

/*
 * Receive queue types.  These are used to tag
 * each transmit queue in the hardware and to identify a set
 * of transmit queues for operations such as start/stop dma.
 */
typedef enum {
    HAL_RX_QUEUE_HP   = 0,        /* high priority recv queue */
    HAL_RX_QUEUE_LP   = 1,        /* low priority recv queue */
} HAL_RX_QUEUE;

#define    HAL_NUM_RX_QUEUES    2        /* max possible # of queues */

#define    HAL_TXFIFO_DEPTH     8        /* transmit fifo depth */

/*
 * Transmit queue subtype.  These map directly to
 * WME Access Categories (except for UPSD).  Refer
 * to Table 5 of the WME spec.
 */
typedef enum {
    HAL_WME_AC_BK   = 0,            /* background access category */
    HAL_WME_AC_BE   = 1,            /* best effort access category*/
    HAL_WME_AC_VI   = 2,            /* video access category */
    HAL_WME_AC_VO   = 3,            /* voice access category */
    HAL_WME_UPSD    = 4,            /* uplink power save */
    HAL_XR_DATA     = 5,            /* uplink power save */
} HAL_TX_QUEUE_SUBTYPE;

/*
 * Transmit queue flags that control various
 * operational parameters.
 */
typedef enum {
    TXQ_FLAG_TXOKINT_ENABLE            = 0x0001, /* enable TXOK interrupt */
    TXQ_FLAG_TXERRINT_ENABLE           = 0x0001, /* enable TXERR interrupt */
    TXQ_FLAG_TXDESCINT_ENABLE          = 0x0002, /* enable TXDESC interrupt */
    TXQ_FLAG_TXEOLINT_ENABLE           = 0x0004, /* enable TXEOL interrupt */
    TXQ_FLAG_TXURNINT_ENABLE           = 0x0008, /* enable TXURN interrupt */
    TXQ_FLAG_BACKOFF_DISABLE           = 0x0010, /* disable Post Backoff  */
    TXQ_FLAG_COMPRESSION_ENABLE        = 0x0020, /* compression enabled */
    TXQ_FLAG_RDYTIME_EXP_POLICY_ENABLE = 0x0040, /* enable ready time
                                                    expiry policy */
    TXQ_FLAG_FRAG_BURST_BACKOFF_ENABLE = 0x0080, /* enable backoff while
                                                    sending fragment burst*/
} HAL_TX_QUEUE_FLAGS;

typedef struct {
    u_int32_t               tqi_ver;        /* hal TXQ version */
    HAL_TX_QUEUE_SUBTYPE    tqi_subtype;    /* subtype if applicable */
    HAL_TX_QUEUE_FLAGS      tqi_qflags;     /* flags (see above) */
    u_int32_t               tqi_priority;   /* (not used) */
    u_int32_t               tqi_aifs;       /* aifs */
    u_int32_t               tqi_cwmin;      /* cw_min */
    u_int32_t               tqi_cwmax;      /* cwMax */
    u_int16_t               tqi_shretry;    /* rts retry limit */
    u_int16_t               tqi_lgretry;    /* long retry limit (not used)*/
    u_int32_t               tqi_cbr_period;
    u_int32_t               tqi_cbr_overflow_limit;
    u_int32_t               tqi_burst_time;
    u_int32_t               tqi_ready_time; /* specified in msecs */
    u_int32_t               tqi_comp_buf;    /* compression buffer phys addr */
} HAL_TXQ_INFO;

#if ATH_WOW
/*
 * Pattern Types.
 */
#define AH_WOW_USER_PATTERN_EN          0x0001
#define AH_WOW_MAGIC_PATTERN_EN         0x0002
#define AH_WOW_LINK_CHANGE              0x0004
#define AH_WOW_BEACON_MISS              0x0008
#define AH_WOW_NLO_DISCOVERY_EN         0x0010  /* SW WOW offload */
#define AH_WOW_AP_ASSOCIATION_LOST_EN   0x0020  /* SW WOW offload */
#define AH_WOW_GTK_HANDSHAKE_ERROR_EN   0x0040  /* SW WOW offload */
#define AH_WOW_4WAY_HANDSHAKE_EN        0x0080  /* SW WOW offload */
#define AH_WOW_GTK_OFFLOAD_EN           0x0100  /* SW WOW offload */
#define AH_WOW_ARP_OFFLOAD_EN           0x0200  /* SW WOW offload */
#define AH_WOW_NS_OFFLOAD_EN            0x0400  /* SW WOW offload */
#define AH_WOW_ACER_MAGIC_EN            0x1000  /* SW WOW offload */
#define AH_WOW_ACER_KEEP_ALIVE_EN       0x2000  /* SW WOW offload */
#define AH_WOW_MAX_EVENTS           8

#endif

#define HAL_TQI_NONVAL 0xffff

/* token to use for aifs, cwmin, cwmax */
#define    HAL_TXQ_USEDEFAULT    ((u_int32_t) -1)

/* compression definitions */
#define HAL_COMP_BUF_MAX_SIZE           9216            /* 9K */
#define HAL_COMP_BUF_ALIGN_SIZE         512

/* ready_time % lo and hi bounds */
#define HAL_READY_TIME_LO_BOUND         15 
#define HAL_READY_TIME_HI_BOUND         96  /* considering the swba and dma 
                                               response times for cabQ */

/*
 * Transmit packet types.  This belongs in ah_desc.h, but
 * is here so we can give a proper type to various parameters
 * (and not require everyone include the file).
 *
 * NB: These values are intentionally assigned for
 *     direct use when setting up h/w descriptors.
 */
typedef enum {
    HAL_PKT_TYPE_NORMAL     = 0,
    HAL_PKT_TYPE_ATIM       = 1,
    HAL_PKT_TYPE_PSPOLL     = 2,
    HAL_PKT_TYPE_BEACON     = 3,
    HAL_PKT_TYPE_PROBE_RESP = 4,
    HAL_PKT_TYPE_CHIRP      = 5,
    HAL_PKT_TYPE_GRP_POLL   = 6,
} HAL_PKT_TYPE;

/* Rx Filter Frame Types */
typedef enum {
    /*
    ** These bits correspond to register 0x803c bits
    */
    HAL_RX_FILTER_UCAST     = 0x00000001,   /* Allow unicast frames */
    HAL_RX_FILTER_MCAST     = 0x00000002,   /* Allow multicast frames */
    HAL_RX_FILTER_BCAST     = 0x00000004,   /* Allow broadcast frames */
    HAL_RX_FILTER_CONTROL   = 0x00000008,   /* Allow control frames */
    HAL_RX_FILTER_BEACON    = 0x00000010,   /* Allow beacon frames */
    HAL_RX_FILTER_PROM      = 0x00000020,   /* Promiscuous mode */
    HAL_RX_FILTER_XRPOLL    = 0x00000040,   /* Allow XR poll frame */
    HAL_RX_FILTER_PROBEREQ  = 0x00000080,   /* Allow probe request frames */
    HAL_RX_FILTER_PHYERR    = 0x00000100,   /* Allow phy errors */
    HAL_RX_FILTER_MYBEACON  = 0x00000200,   /* Filter beacons other than mine */
    HAL_RX_FILTER_PHYRADAR  = 0x00002000,   /* Allow phy radar errors*/
    HAL_RX_FILTER_PSPOLL    = 0x00004000,   /* Allow PSPOLL frames */
    HAL_RX_FILTER_ALL_MCAST_BCAST = 0x00008000,   /* Allow all MCAST,BCAST frames */
    HAL_RX_FILTER_RX_HWBCNPROC_EN = 0x00020000,   /* Allow beacon frames that pass the hw beacon crc */
} HAL_RX_FILTER;

typedef enum {
    HAL_PM_AWAKE            = 0,
    HAL_PM_FULL_SLEEP       = 1,
    HAL_PM_NETWORK_SLEEP    = 2,
    HAL_PM_UNDEFINED        = 3
} HAL_POWER_MODE;

typedef enum {
    HAL_SMPS_DEFAULT = 0,
    HAL_SMPS_SW_CTRL_LOW_PWR,     /* Software control, low power setting */
    HAL_SMPS_SW_CTRL_HIGH_PWR,    /* Software control, high power setting */
    HAL_SMPS_HW_CTRL              /* Hardware Control */
} HAL_SMPS_MODE;

#define AH_ENT_DUAL_BAND_DISABLE        0x00000001
#define AH_ENT_CHAIN2_DISABLE           0x00000002
#define AH_ENT_5MHZ_DISABLE             0x00000004
#define AH_ENT_10MHZ_DISABLE            0x00000008
#define AH_ENT_49GHZ_DISABLE            0x00000010
#define AH_ENT_LOOPBACK_DISABLE         0x00000020
#define AH_ENT_TPC_PERF_DISABLE         0x00000040
#define AH_ENT_MIN_PKT_SIZE_DISABLE     0x00000080
#define AH_ENT_SPECTRAL_PRECISION       0x00000300
#define AH_ENT_SPECTRAL_PRECISION_S     8
#define AH_ENT_RTSCTS_DELIM_WAR         0x00010000
 
#define AH_FIRST_DESC_NDELIMS 60
/*
 * NOTE WELL:
 * These are mapped to take advantage of the common locations for many of
 * the bits on all of the currently supported MAC chips. This is to make
 * the ISR as efficient as possible, while still abstracting HW differences.
 * When new hardware breaks this commonality this enumerated type, as well
 * as the HAL functions using it, must be modified. All values are directly
 * mapped unless commented otherwise.
 */
typedef enum {
    HAL_INT_RX        = 0x00000001,   /* Non-common mapping */
    HAL_INT_RXDESC    = 0x00000002,
    HAL_INT_RXERR     = 0x00000004,
    HAL_INT_RXHP      = 0x00000001,   /* EDMA mapping */
    HAL_INT_RXLP      = 0x00000002,   /* EDMA mapping */
    HAL_INT_RXNOFRM   = 0x00000008,
    HAL_INT_RXEOL     = 0x00000010,
    HAL_INT_RXORN     = 0x00000020,
    HAL_INT_TX        = 0x00000040,   /* Non-common mapping */
    HAL_INT_TXDESC    = 0x00000080,
    HAL_INT_TIM_TIMER = 0x00000100,
    HAL_INT_MCI       = 0x00000200,
    HAL_INT_BBPANIC   = 0x00000400,
    HAL_INT_TXURN     = 0x00000800,
    HAL_INT_MIB       = 0x00001000,
    HAL_INT_RXPHY     = 0x00004000,
    HAL_INT_RXKCM     = 0x00008000,
    HAL_INT_SWBA      = 0x00010000,
    HAL_INT_BRSSI     = 0x00020000,
    HAL_INT_BMISS     = 0x00040000,
    HAL_INT_BNR       = 0x00100000,   /* Non-common mapping */
    HAL_INT_TIM       = 0x00200000,   /* Non-common mapping */
    HAL_INT_DTIM      = 0x00400000,   /* Non-common mapping */
    HAL_INT_DTIMSYNC  = 0x00800000,   /* Non-common mapping */
    HAL_INT_GPIO      = 0x01000000,
    HAL_INT_CABEND    = 0x02000000,   /* Non-common mapping */
    HAL_INT_TSFOOR    = 0x04000000,   /* Non-common mapping */
    HAL_INT_GENTIMER  = 0x08000000,   /* Non-common mapping */
    HAL_INT_CST       = 0x10000000,   /* Non-common mapping */
    HAL_INT_GTT       = 0x20000000,   /* Non-common mapping */
    HAL_INT_FATAL     = 0x40000000,   /* Non-common mapping */
    HAL_INT_GLOBAL    = 0x80000000,   /* Set/clear IER */
    HAL_INT_BMISC     = HAL_INT_TIM
            | HAL_INT_DTIM
            | HAL_INT_DTIMSYNC
            | HAL_INT_TSFOOR
            | HAL_INT_CABEND,

    /* Interrupt bits that map directly to ISR/IMR bits */
    HAL_INT_COMMON    = HAL_INT_RXNOFRM
            | HAL_INT_RXDESC
            | HAL_INT_RXERR
            | HAL_INT_RXEOL
            | HAL_INT_RXORN
            | HAL_INT_TXURN
            | HAL_INT_TXDESC
            | HAL_INT_MIB
            | HAL_INT_RXPHY
            | HAL_INT_RXKCM
            | HAL_INT_SWBA
            | HAL_INT_BRSSI
            | HAL_INT_BMISS,
    HAL_INT_NOCARD   = 0xffffffff    /* To signal the card was removed */
} HAL_INT;

/*
 * MSI vector assignments
 */
typedef enum {
    HAL_MSIVEC_MISC = 0,
    HAL_MSIVEC_TX   = 1,
    HAL_MSIVEC_RXLP = 2,
    HAL_MSIVEC_RXHP = 3,
} HAL_MSIVEC;

typedef enum {
    HAL_INT_LINE = 0,
    HAL_INT_MSI  = 1,
} HAL_INT_TYPE;

/* For interrupt mitigation registers */
typedef enum {
    HAL_INT_RX_FIRSTPKT=0,
    HAL_INT_RX_LASTPKT,
    HAL_INT_TX_FIRSTPKT,
    HAL_INT_TX_LASTPKT,
    HAL_INT_THRESHOLD
} HAL_INT_MITIGATION;

typedef enum {
    HAL_RFGAIN_INACTIVE         = 0,
    HAL_RFGAIN_READ_REQUESTED   = 1,
    HAL_RFGAIN_NEED_CHANGE      = 2
} HAL_RFGAIN;

/*
 * Channels are specified by frequency.
 */
typedef struct {
    u_int16_t   channel;        /* setting in Mhz */
    u_int32_t   channel_flags;   /* see below */
    u_int8_t    priv_flags;
    int8_t      max_reg_tx_power;  /* max regulatory tx power in dBm */
    int8_t      max_tx_power;     /* max true tx power in 0.5 dBm */
    int8_t      min_tx_power;     /* min true tx power in 0.5 dBm */
    int8_t      max_rate_power;     /* max power for all rates in dBm */
    int8_t      min_rate_power;     /* min power for all rates in dBm */
    u_int8_t    regClassId;     /* regulatory class id of this channel */
    u_int8_t    paprd_done:1,               /* 1: PAPRD DONE, 0: PAPRD Cal not done */
                paprd_table_write_done:1;     /* 1: DONE, 0: Cal data write not done */
} HAL_CHANNEL;

#define ARRAY_INFO_SIZE 36

/* channel_flags */
#define CHANNEL_CW_INT    0x00002 /* CW interference detected on channel */
#define CHANNEL_TURBO     0x00010 /* Turbo Channel */
#define CHANNEL_CCK       0x00020 /* CCK channel */
#define CHANNEL_OFDM      0x00040 /* OFDM channel */
#define CHANNEL_2GHZ      0x00080 /* 2 GHz spectrum channel. */
#define CHANNEL_5GHZ      0x00100 /* 5 GHz spectrum channel */
#define CHANNEL_PASSIVE   0x00200 /* Only passive scan allowed in the channel */
#define CHANNEL_DYN       0x00400 /* dynamic CCK-OFDM channel */
#define CHANNEL_XR        0x00800 /* XR channel */
#define CHANNEL_STURBO    0x02000 /* Static turbo, no 11a-only usage */
#define CHANNEL_HALF      0x04000 /* Half rate channel */
#define CHANNEL_QUARTER   0x08000 /* Quarter rate channel */
#define CHANNEL_HT20      0x10000 /* HT20 channel */
#define CHANNEL_HT40PLUS  0x20000 /* HT40 channel with extention channel above */
#define CHANNEL_HT40MINUS 0x40000 /* HT40 channel with extention channel below */

/* priv_flags */
#define CHANNEL_INTERFERENCE    0x01 /* Software use: channel interference 
                                        used for as AR as well as RADAR 
                                        interference detection */
#define CHANNEL_DFS             0x02 /* DFS required on channel */
#define CHANNEL_4MS_LIMIT       0x04 /* 4msec packet limit on this channel */
#define CHANNEL_DFS_CLEAR       0x08 /* if channel has been checked for DFS */
#define CHANNEL_DISALLOW_ADHOC  0x10 /* ad hoc not allowed on this channel */
#define CHANNEL_PER_11D_ADHOC   0x20 /* ad hoc support is per 11d */
#define CHANNEL_EDGE_CH         0x40 /* Edge Channel */
#define CHANNEL_NO_HOSTAP       0x80 /* Non AP Channel */

#define CHANNEL_A           (CHANNEL_5GHZ|CHANNEL_OFDM)
#define CHANNEL_B           (CHANNEL_2GHZ|CHANNEL_CCK)
#define CHANNEL_PUREG       (CHANNEL_2GHZ|CHANNEL_OFDM)
#define CHANNEL_G           (CHANNEL_2GHZ|CHANNEL_OFDM)
#define CHANNEL_T           (CHANNEL_5GHZ|CHANNEL_OFDM|CHANNEL_TURBO)
#define CHANNEL_ST          (CHANNEL_T|CHANNEL_STURBO)
#define CHANNEL_108G        (CHANNEL_2GHZ|CHANNEL_OFDM|CHANNEL_TURBO)
#define CHANNEL_108A        CHANNEL_T
#define CHANNEL_X           (CHANNEL_5GHZ|CHANNEL_OFDM|CHANNEL_XR)
#define CHANNEL_G_HT20      (CHANNEL_2GHZ|CHANNEL_HT20)
#define CHANNEL_A_HT20      (CHANNEL_5GHZ|CHANNEL_HT20)
#define CHANNEL_G_HT40PLUS  (CHANNEL_2GHZ|CHANNEL_HT40PLUS)
#define CHANNEL_G_HT40MINUS (CHANNEL_2GHZ|CHANNEL_HT40MINUS)
#define CHANNEL_A_HT40PLUS  (CHANNEL_5GHZ|CHANNEL_HT40PLUS)
#define CHANNEL_A_HT40MINUS (CHANNEL_5GHZ|CHANNEL_HT40MINUS)
#define CHANNEL_ALL \
        (CHANNEL_OFDM|CHANNEL_CCK| CHANNEL_2GHZ | CHANNEL_5GHZ | CHANNEL_TURBO | CHANNEL_HT20 | CHANNEL_HT40PLUS | CHANNEL_HT40MINUS)
#define CHANNEL_ALL_NOTURBO (CHANNEL_ALL &~ CHANNEL_TURBO)

typedef struct {
    u_int32_t   ackrcv_bad;
    u_int32_t   rts_bad;
    u_int32_t   rts_good;
    u_int32_t   fcs_bad;
    u_int32_t   beacons;
} HAL_MIB_STATS;

typedef u_int16_t HAL_CTRY_CODE;        /* country code */
typedef u_int16_t HAL_REG_DOMAIN;       /* regulatory domain code */

enum {
    CTRY_DEBUG      = 0x1ff,    /* debug country code */
    CTRY_DEFAULT    = 0         /* default country code */
};

typedef enum {
        REG_EXT_FCC_MIDBAND            = 0,
        REG_EXT_JAPAN_MIDBAND          = 1,
        REG_EXT_FCC_DFS_HT40           = 2,
        REG_EXT_JAPAN_NONDFS_HT40      = 3,
        REG_EXT_JAPAN_DFS_HT40         = 4,
        REG_EXT_FCC_CH_144             = 5,
} REG_EXT_BITMAP;       

#define REG_CH144_PRI20_ALLOWED 0x00000001
#define REG_CH144_SEC20_ALLOWED 0x00000002
 
/*
 * Regulatory related information
 */
typedef struct _HAL_COUNTRY_ENTRY{
    u_int16_t   countryCode;  /* HAL private */
    u_int16_t   regDmnEnum;   /* HAL private */
    u_int16_t   regDmn5G;
    u_int16_t   regDmn2G;
    u_int8_t    isMultidomain;
    u_int8_t    iso[3];       /* ISO CC + (I)ndoor/(O)utdoor or both ( ) */
} HAL_COUNTRY_ENTRY;

enum {
    HAL_MODE_11A              = 0x00001,      /* 11a channels */
    HAL_MODE_TURBO            = 0x00002,      /* 11a turbo-only channels */
    HAL_MODE_11B              = 0x00004,      /* 11b channels */
    HAL_MODE_PUREG            = 0x00008,      /* 11g channels (OFDM only) */
    HAL_MODE_11G              = 0x00008,      /* XXX historical */
    HAL_MODE_108G             = 0x00020,      /* 11a+Turbo channels */
    HAL_MODE_108A             = 0x00040,      /* 11g+Turbo channels */
    HAL_MODE_XR               = 0x00100,      /* XR channels */
    HAL_MODE_11A_HALF_RATE    = 0x00200,      /* 11A half rate channels */
    HAL_MODE_11A_QUARTER_RATE = 0x00400,      /* 11A quarter rate channels */
    HAL_MODE_11NG_HT20        = 0x00800,      /* 11N-G HT20 channels */
    HAL_MODE_11NA_HT20        = 0x01000,      /* 11N-A HT20 channels */
    HAL_MODE_11NG_HT40PLUS    = 0x02000,      /* 11N-G HT40 + channels */
    HAL_MODE_11NG_HT40MINUS   = 0x04000,      /* 11N-G HT40 - channels */
    HAL_MODE_11NA_HT40PLUS    = 0x08000,      /* 11N-A HT40 + channels */
    HAL_MODE_11NA_HT40MINUS   = 0x10000,      /* 11N-A HT40 - channels */
    HAL_MODE_ALL              = 0xffffffff
};

#define HAL_MODE_11N_MASK \
  ( HAL_MODE_11NG_HT20 | HAL_MODE_11NA_HT20 | HAL_MODE_11NG_HT40PLUS | \
    HAL_MODE_11NG_HT40MINUS | HAL_MODE_11NA_HT40PLUS | HAL_MODE_11NA_HT40MINUS )

typedef struct {
    int     rateCount;      /* NB: for proper padding */
    u_int8_t    rateCodeToIndex[256];    /* back mapping */
    struct {
        u_int8_t    valid;      /* valid for rate control use */
        u_int8_t    phy;        /* CCK/OFDM/XR */
        u_int32_t   rateKbps;   /* transfer rate in kbs */
        u_int8_t    rate_code;   /* rate for h/w descriptors */
        u_int8_t    shortPreamble;  /* mask for enabling short
                         * preamble in CCK rate code */
        u_int8_t    dot11Rate;  /* value for supported rates
                         * info element of MLME */
        u_int8_t    controlRate;    /* index of next lower basic
                         * rate; used for dur. calcs */
        u_int16_t   lpAckDuration;  /* long preamble ACK duration */
        u_int16_t   spAckDuration;  /* short preamble ACK duration*/
    } info[ARRAY_INFO_SIZE];
} HAL_RATE_TABLE;

typedef struct {
    u_int       rs_count;       /* number of valid entries */
    u_int8_t    rs_rates[36];       /* rates */
} HAL_RATE_SET;

typedef enum {
    HAL_ANT_VARIABLE = 0,           /* variable by programming */
    HAL_ANT_FIXED_A  = 1,           /* fixed to 11a frequencies */
    HAL_ANT_FIXED_B  = 2,           /* fixed to 11b frequencies */
    HAL_ANT_MAX_MODE = 3,
} HAL_ANT_SETTING;

typedef enum {
    HAL_M_IBSS              = 0,    /* IBSS (adhoc) station */
    HAL_M_STA               = 1,    /* infrastructure station */
    HAL_M_HOSTAP            = 6,    /* Software Access Point */
    HAL_M_MONITOR           = 8,    /* Monitor mode */
    HAL_M_RAW_ADC_CAPTURE   = 10    /* Raw ADC capture mode */
} HAL_OPMODE;

#if ATH_DEBUG
static inline const char *ath_hal_opmode_name(HAL_OPMODE opmode)
{
    switch (opmode) {
    case HAL_M_IBSS:
        return "IBSS";
    case HAL_M_STA:
        return "STA";
    case HAL_M_HOSTAP:
        return "HOSTAP";
    case HAL_M_MONITOR:
        return "MONITOR";
    case HAL_M_RAW_ADC_CAPTURE:
        return "RAW_ADC";
    default:
        return "UNKNOWN";
    }
}
#else
#define ath_hal_opmode_name(opmode) ((const char *) 0x0)
#endif /* ATH_DEBUG */

typedef struct {
    u_int8_t    kv_type;        /* one of HAL_CIPHER */
    u_int8_t    kv_apsd;        /* mask for APSD enabled ACs */
    u_int16_t   kv_len;         /* length in bits */
    u_int8_t    kv_val[16];     /* enough for 128-bit keys */
    u_int8_t    kv_mic[8];      /* TKIP Rx MIC key */
    u_int8_t    kv_txmic[8];    /* TKIP Tx MIC key */
} HAL_KEYVAL;

#define AH_KEYTYPE_MASK     0x0F
typedef enum {
    HAL_KEY_TYPE_CLEAR,
    HAL_KEY_TYPE_WEP,
    HAL_KEY_TYPE_AES,
    HAL_KEY_TYPE_TKIP,
#if ATH_SUPPORT_WAPI
    HAL_KEY_TYPE_WAPI,
#endif
    HAL_KEY_PROXY_STA_MASK = 0x10
} HAL_KEY_TYPE;

typedef enum {
    HAL_CIPHER_WEP      = 0,
    HAL_CIPHER_AES_OCB  = 1,
    HAL_CIPHER_AES_CCM  = 2,
    HAL_CIPHER_CKIP     = 3,
    HAL_CIPHER_TKIP     = 4,
    HAL_CIPHER_CLR      = 5,        /* no encryption */
#if ATH_SUPPORT_WAPI
    HAL_CIPHER_WAPI     = 6,        /* WAPI encryption */
#endif
    HAL_CIPHER_MIC      = 127,      /* TKIP-MIC, not a cipher */
    HAL_CIPHER_UNUSED   = 0xff 
} HAL_CIPHER;

enum {
    HAL_SLOT_TIME_6  = 6,
    HAL_SLOT_TIME_9  = 9,
    HAL_SLOT_TIME_13 = 13,
    HAL_SLOT_TIME_20 = 20,
    HAL_SLOT_TIME_21 = 21,
};

/* 11n */
typedef enum {
        HAL_HT_MACMODE_20       = 0,            /* 20 MHz operation */
        HAL_HT_MACMODE_2040     = 1,            /* 20/40 MHz operation */
} HAL_HT_MACMODE;

typedef enum {
    HAL_HT_EXTPROTSPACING_20    = 0,            /* 20 MHZ spacing */
    HAL_HT_EXTPROTSPACING_25    = 1,            /* 25 MHZ spacing */
} HAL_HT_EXTPROTSPACING;

typedef struct {
    HAL_HT_MACMODE              ht_macmode;             /* MAC - 20/40 mode */
    HAL_HT_EXTPROTSPACING       ht_extprotspacing;      /* ext channel protection spacing */
} HAL_HT_CWM;

typedef enum {
    HAL_RX_CLEAR_CTL_LOW        = 0x1,  /* force control channel to appear busy */
    HAL_RX_CLEAR_EXT_LOW        = 0x2,  /* force extension channel to appear busy */
} HAL_HT_RXCLEAR;

typedef enum {
    HAL_FREQ_BAND_5GHZ          = 0,
    HAL_FREQ_BAND_2GHZ          = 1,
} HAL_FREQ_BAND;

#define HAL_RATESERIES_RTS_CTS  0x0001  /* use rts/cts w/this series */
#define HAL_RATESERIES_2040     0x0002  /* use ext channel for series */
#define HAL_RATESERIES_HALFGI   0x0004  /* use half-gi for series */
#define HAL_RATESERIES_STBC     0x0008  /* use STBC for series */
#ifdef  ATH_SUPPORT_TxBF
#define HAL_RATESERIES_TXBF     0x0010  /* use tx_bf for series */
#endif

typedef struct {
    u_int   Tries;
    u_int   Rate;
    u_int   PktDuration;
    u_int   ch_sel;
    u_int   RateFlags;
    u_int   rate_index;
    u_int   tx_power_cap;     /* in 1/2 dBm units */
} HAL_11N_RATE_SERIES;

enum {
    HAL_TRUE_CHIP = 1,
    HAL_MAC_TO_MAC_EMU,
    HAL_MAC_BB_EMU
};

/*
 * Per-station beacon timer state.  Note that the specified
 * beacon interval (given in TU's) can also include flags
 * to force a TSF reset and to enable the beacon xmit logic.
 * If bs_cfpmaxduration is non-zero the hardware is setup to
 * coexist with a PCF-capable AP.
 */
typedef struct {
    u_int32_t   bs_nexttbtt;        /* next beacon in TU */
    u_int32_t   bs_nextdtim;        /* next DTIM in TU */
    u_int32_t   bs_intval;          /* beacon interval+flags */
#define HAL_BEACON_PERIOD       0x0000ffff  /* beacon interval mask for TU */
#define HAL_BEACON_PERIOD_TU8   0x0007ffff  /* beacon interval mask for TU/8 */
#define HAL_BEACON_ENA          0x00800000  /* beacon xmit enable */
#define HAL_BEACON_RESET_TSF    0x01000000  /* clear TSF */
#define HAL_TSFOOR_THRESHOLD    0x00004240 /* TSF OOR threshold (16k us) */ 
    u_int32_t   bs_dtimperiod;
    u_int16_t   bs_cfpperiod;       /* CFP period in TU */
    u_int16_t   bs_cfpmaxduration;  /* max CFP duration in TU */
    u_int32_t   bs_cfpnext;         /* next CFP in TU */
    u_int16_t   bs_timoffset;       /* byte offset to TIM bitmap */
    u_int16_t   bs_bmissthreshold;  /* beacon miss threshold */
    u_int32_t   bs_sleepduration;   /* max sleep duration */
    u_int32_t   bs_tsfoor_threshold;/* TSF out of range threshold */
} HAL_BEACON_STATE;

/*
 * Per-node statistics maintained by the driver for use in
 * optimizing signal quality and other operational aspects.
 */
typedef struct {
    u_int32_t   ns_avgbrssi;    /* average beacon rssi */
    u_int32_t   ns_avgrssi;     /* average rssi of all received frames */
    u_int32_t   ns_avgtxrssi;   /* average tx rssi */
    u_int32_t   ns_avgtxrate;   /* average IEEE tx rate (in 500kbps units) */
#ifdef ATH_SWRETRY    
    u_int32_t    ns_swretryfailcount; /*number of sw retries which got failed*/
    u_int32_t    ns_swretryframecount; /*total number of frames that are sw retried*/
#endif        
} HAL_NODE_STATS;

/* for VOW_DCS */
typedef struct {
    u_int32_t   cyclecnt_diff;    /* delta cycle count */
    u_int32_t   rxclr_cnt;        /* rx clear count */
    u_int32_t   txframecnt_diff;  /* delta tx frame count */
    u_int32_t   rxframecnt_diff;  /* delta rx frame count */    
    u_int32_t   listen_time;      /* listen time in msec - time for which ch is free */
    u_int32_t   ofdmphyerr_cnt;   /* OFDM err count since last reset */
    u_int32_t   cckphyerr_cnt;    /* CCK err count since last reset */
    u_int32_t   ofdmphyerrcnt_diff; /* delta OFDM Phy Error Count */
    HAL_BOOL    valid;             /* if the stats are valid*/        
} HAL_ANISTATS;

typedef struct {
    u_int8_t txctl_offset;
    u_int8_t txctl_numwords;
    u_int8_t txstatus_offset;
    u_int8_t txstatus_numwords;

    u_int8_t rxctl_offset;
    u_int8_t rxctl_numwords;
    u_int8_t rxstatus_offset;
    u_int8_t rxstatus_numwords;

    u_int8_t macRevision;
} HAL_DESC_INFO;

#define HAL_RSSI_EP_MULTIPLIER  (1<<7)  /* pow2 to optimize out * and / */
#define HAL_RATE_EP_MULTIPLIER  (1<<7)  /* pow2 to optimize out * and / */
#define HAL_IS_MULTIPLE_CHAIN(x)    (((x) & ((x) - 1)) != 0)
#define HAL_IS_SINGLE_CHAIN(x)      (((x) & ((x) - 1)) == 0)


/*
 * GPIO Output mux can select from a number of different signals as input.
 * The current implementation uses 5 of these input signals:
 *     - An output value specified by the caller;
 *     - The Attention LED signal provided by the PCIE chip;
 *     - The Power     LED signal provided by the PCIE chip;
 *     - The Network LED pin controlled by the chip's MAC;
 *     - The Power   LED pin controlled by the chip's MAC.
 */
typedef enum {
    HAL_GPIO_OUTPUT_MUX_AS_OUTPUT,
    HAL_GPIO_OUTPUT_MUX_AS_PCIE_ATTENTION_LED,
    HAL_GPIO_OUTPUT_MUX_AS_PCIE_POWER_LED,
    HAL_GPIO_OUTPUT_MUX_AS_MAC_NETWORK_LED,
    HAL_GPIO_OUTPUT_MUX_AS_MAC_POWER_LED,
    HAL_GPIO_OUTPUT_MUX_AS_WLAN_ACTIVE,
    HAL_GPIO_OUTPUT_MUX_AS_TX_FRAME,
    HAL_GPIO_OUTPUT_MUX_AS_MCI_WLAN_DATA,
    HAL_GPIO_OUTPUT_MUX_AS_MCI_WLAN_CLK,
    HAL_GPIO_OUTPUT_MUX_AS_MCI_BT_DATA,
    HAL_GPIO_OUTPUT_MUX_AS_MCI_BT_CLK,
    HAL_GPIO_OUTPUT_MUX_AS_WL_IN_TX,
    HAL_GPIO_OUTPUT_MUX_AS_WL_IN_RX,
    HAL_GPIO_OUTPUT_MUX_AS_BT_IN_TX,
    HAL_GPIO_OUTPUT_MUX_AS_BT_IN_RX,
    HAL_GPIO_OUTPUT_MUX_AS_SMART_ANT_SERIAL_STROBE,
    HAL_GPIO_OUTPUT_MUX_AS_SMART_ANT_SERIAL_DATA,
    HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_CTRL0,
    HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_CTRL1,
    HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_CTRL2,
    HAL_GPIO_OUTPUT_MUX_AS_SMARTANT_SWCOM3,
    HAL_GPIO_OUTPUT_MUX_NUM_ENTRIES    // always keep this type last; it must map to the number of entries in this enumeration
} HAL_GPIO_OUTPUT_MUX_TYPE;

typedef enum {
    HAL_GPIO_INTR_LOW = 0,
    HAL_GPIO_INTR_HIGH,
    HAL_GPIO_INTR_DISABLE
} HAL_GPIO_INTR_TYPE;

/*
** Definition of HAL operating parameters
**
** This ENUM provides a mechanism for external objects to update HAL operating
** parameters through a common ioctl-like interface.  The parameter IDs are
** used to identify the specific parameter in question to the get/set interface.
*/

typedef enum {
    HAL_CONFIG_DMA_BEACON_RESPONSE_TIME = 0,
    HAL_CONFIG_SW_BEACON_RESPONSE_TIME,
    HAL_CONFIG_ADDITIONAL_SWBA_BACKOFF,
    HAL_CONFIG_6MB_ACK,                
    HAL_CONFIG_CWMIGNOREEXTCCA,
    HAL_CONFIG_CCA_DETECTION_LEVEL,
    HAL_CONFIG_CCA_DETECTION_MARGIN,
    HAL_CONFIG_FORCEBIAS,              
    HAL_CONFIG_FORCEBIASAUTO,
    HAL_CONFIG_PCIEPOWERSAVEENABLE,    
    HAL_CONFIG_PCIEL1SKPENABLE,        
    HAL_CONFIG_PCIECLOCKREQ,           
    HAL_CONFIG_PCIEWAEN,               
    HAL_CONFIG_PCIEDETACH,
    HAL_CONFIG_PCIEPOWERRESET,         
    HAL_CONFIG_PCIERESTORE,  
    HAL_CONFIG_ENABLEMSI,
    HAL_CONFIG_HTENABLE,               
    HAL_CONFIG_DISABLETURBOG,          
    HAL_CONFIG_OFDMTRIGLOW,            
    HAL_CONFIG_OFDMTRIGHIGH,           
    HAL_CONFIG_CCKTRIGHIGH,            
    HAL_CONFIG_CCKTRIGLOW,             
    HAL_CONFIG_ENABLEANI,
    HAL_CONFIG_ENABLEADAPTIVECCATHRES,
    HAL_CONFIG_NOISEIMMUNITYLVL,       
    HAL_CONFIG_OFDMWEAKSIGDET,         
    HAL_CONFIG_CCKWEAKSIGTHR,          
    HAL_CONFIG_SPURIMMUNITYLVL,        
    HAL_CONFIG_FIRSTEPLVL,             
    HAL_CONFIG_RSSITHRHIGH,            
    HAL_CONFIG_RSSITHRLOW,             
    HAL_CONFIG_DIVERSITYCONTROL,       
    HAL_CONFIG_ANTENNASWITCHSWAP,
    HAL_CONFIG_FULLRESETWARENABLE,
    HAL_CONFIG_DISABLEPERIODICPACAL,
    HAL_CONFIG_DEBUG,
    HAL_CONFIG_KEEPALIVETIMEOUT,
    HAL_CONFIG_DEFAULTANTENNASET,
    HAL_CONFIG_REGREAD_BASE,
#ifdef ATH_SUPPORT_TxBF
    HAL_CONFIG_TxBF_CTL,
#endif

    HAL_CONFIG_KEYCACHE_PRINT,

    HAL_CONFIG_SHOW_BB_PANIC,
    HAL_CONFIG_INTR_MITIGATION_RX,
    HAL_CONFIG_CH144,
#ifdef HOST_OFFLOAD
    HAL_CONFIG_LOWER_INT_MITIGATION_RX,
#endif
    HAL_CONFIG_OPS_PARAM_MAX
} HAL_CONFIG_OPS_PARAMS_TYPE;

/*
 * Diagnostic interface.  This is an open-ended interface that
 * is opaque to applications.  Diagnostic programs use this to
 * retrieve internal data structures, etc.  There is no guarantee
 * that calling conventions for calls other than HAL_DIAG_REVS
 * are stable between HAL releases; a diagnostic application must
 * use the HAL revision information to deal with ABI/API differences.
 */
enum {
    HAL_DIAG_REVS           = 0,    /* MAC/PHY/Radio revs */
    HAL_DIAG_EEPROM         = 1,    /* EEPROM contents */
    HAL_DIAG_EEPROM_EXP_11A = 2,    /* EEPROM 5112 power exp for 11a */
    HAL_DIAG_EEPROM_EXP_11B = 3,    /* EEPROM 5112 power exp for 11b */
    HAL_DIAG_EEPROM_EXP_11G = 4,    /* EEPROM 5112 power exp for 11g */
    HAL_DIAG_ANI_CURRENT    = 5,    /* ANI current channel state */
    HAL_DIAG_ANI_OFDM       = 6,    /* ANI OFDM timing error stats */
    HAL_DIAG_ANI_CCK        = 7,    /* ANI CCK timing error stats */
    HAL_DIAG_ANI_STATS      = 8,    /* ANI statistics */
    HAL_DIAG_RFGAIN         = 9,    /* RfGain GAIN_VALUES */
    HAL_DIAG_RFGAIN_CURSTEP = 10,   /* RfGain GAIN_OPTIMIZATION_STEP */
    HAL_DIAG_PCDAC          = 11,   /* PCDAC table */
    HAL_DIAG_TXRATES        = 12,   /* Transmit rate table */
    HAL_DIAG_REGS           = 13,   /* Registers */
    HAL_DIAG_ANI_CMD        = 14,   /* ANI issue command */
    HAL_DIAG_SETKEY         = 15,   /* Set keycache backdoor */
    HAL_DIAG_RESETKEY       = 16,   /* Reset keycache backdoor */
    HAL_DIAG_EEREAD         = 17,   /* Read EEPROM word */
    HAL_DIAG_EEWRITE        = 18,   /* Write EEPROM word */
    HAL_DIAG_TXCONT         = 19,   /* TX99 settings */
    HAL_DIAG_SET_RADAR      = 20,   /* Set Radar thresholds */
    HAL_DIAG_GET_RADAR      = 21,   /* Get Radar thresholds */
    HAL_DIAG_USENOL         = 22,   /* Turn on/off the use of the radar NOL */
    HAL_DIAG_GET_USENOL     = 23,   /* Get status of the use of the radar NOL */
    HAL_DIAG_REGREAD        = 24,   /* Reg reads */
    HAL_DIAG_REGWRITE       = 25,   /* Reg writes */
    HAL_DIAG_GET_REGBASE    = 26,   /* Get register base */
    HAL_DIAG_PRINT_REG      = 27,
    HAL_DIAG_RDWRITE        = 28,   /* Write regulatory domain */
    HAL_DIAG_RDREAD         = 29,   /* Get regulatory domain */
    HAL_DIAG_CHANNELS       = 30,   /* Get channel list */
};

/* For register print */
#define HAL_DIAG_PRINT_REG_COUNTER  0x00000001 /* print tf, rf, rc, cc counters */
#define HAL_DIAG_PRINT_REG_ALL      0x80000000 /* print all registers */


typedef struct halCounters {
    u_int32_t   tx_frame_count;
    u_int32_t   rx_frame_count;
    u_int32_t   rx_clear_count;
    u_int32_t   cycle_count;
    u_int8_t    is_rx_active;     // true (1) or false (0)
    u_int8_t    is_tx_active;     // true (1) or false (0)
} HAL_COUNTERS;

#ifdef ATH_CCX
typedef struct {
    u_int8_t    clockRate;
    u_int32_t   noiseFloor;
    u_int8_t    ccaThreshold;
} HAL_CHANNEL_DATA;

#endif

/* DFS defines */

typedef struct {
        int32_t         pe_firpwr;      /* FIR pwr out threshold */
        int32_t         pe_rrssi;       /* Radar rssi thresh */
        int32_t         pe_height;      /* Pulse height thresh */
        int32_t         pe_prssi;       /* Pulse rssi thresh */
        int32_t         pe_inband;      /* Inband thresh */
        /* The following params are only for AR5413 and later */
        u_int32_t       pe_relpwr;      /* Relative power threshold in 0.5dB steps */
        u_int32_t       pe_relstep;     /* Pulse Relative step threshold in 0.5dB steps */
        u_int32_t       pe_maxlen;      /* Max length of radar sign in 0.8us units */
        bool        pe_usefir128;   /* Use the average in-band power measured over 128 cycles */         
        bool        pe_blockradar;  /* Enable to block radar check if pkt detect is done via OFDM
                                           weak signal detect or pkt is detected immediately after tx
                                           to rx transition */
        bool        pe_enmaxrssi;   /* Enable to use the max rssi instead of the last rssi during
                                           fine gain changes for radar detection */
} HAL_PHYERR_PARAM;

#define HAL_PHYERR_PARAM_NOVAL  65535
#define HAL_PHYERR_PARAM_ENABLE 0x8000  /* Enable/Disable if applicable */

/* DFS defines end */

#ifdef ATH_BT_COEX
/*
 * BT Co-existence definitions
 */
typedef enum {
    HAL_BT_MODULE_CSR_BC4       = 0,    /* CSR BlueCore v4 */
    HAL_BT_MODULE_JANUS         = 1,    /* Kite + Valkyrie combo */
    HAL_BT_MODULE_HELIUS        = 2,    /* Kiwi + Valkyrie combo */
    HAL_MAX_BT_MODULES
} HAL_BT_MODULE;

typedef struct {
    HAL_BT_MODULE   bt_module;
    u_int8_t        bt_coex_config;
    u_int8_t        bt_gpio_bt_active;
    u_int8_t        bt_gpio_bt_priority;
    u_int8_t        bt_gpio_wlan_active;
    u_int8_t        bt_active_polarity;
    bool            bt_single_ant;
    u_int8_t        bt_dutyCycle;
    u_int8_t        bt_isolation;
    u_int8_t        bt_period;
} HAL_BT_COEX_INFO;

typedef enum {
    HAL_BT_COEX_MODE_LEGACY     = 0,    /* legacy rx_clear mode */
    HAL_BT_COEX_MODE_UNSLOTTED  = 1,    /* untimed/unslotted mode */
    HAL_BT_COEX_MODE_SLOTTED    = 2,    /* slotted mode */
    HAL_BT_COEX_MODE_DISALBED   = 3,    /* coexistence disabled */
} HAL_BT_COEX_MODE;

typedef enum {
    HAL_BT_COEX_CFG_NONE,               /* No bt coex enabled */
    HAL_BT_COEX_CFG_2WIRE_2CH,          /* 2-wire with 2 chains */
    HAL_BT_COEX_CFG_2WIRE_CH1,          /* 2-wire with ch1 */
    HAL_BT_COEX_CFG_2WIRE_CH0,          /* 2-wire with ch0 */
    HAL_BT_COEX_CFG_3WIRE,              /* 3-wire */
    HAL_BT_COEX_CFG_MCI                 /* MCI */
} HAL_BT_COEX_CFG;

typedef enum {
    HAL_BT_COEX_SET_ACK_PWR     = 0,    /* Change ACK power setting */
    HAL_BT_COEX_LOWER_TX_PWR,           /* Change transmit power */
    HAL_BT_COEX_ANTENNA_DIVERSITY,      /* Enable RX diversity for Kite */
    HAL_BT_COEX_MCI_MAX_TX_PWR,         /* Set max tx power for concurrent tx */
    HAL_BT_COEX_MCI_FTP_STOMP_RX,       /* Use a different weight for stomp low */
} HAL_BT_COEX_SET_PARAMETER;

#define HAL_BT_COEX_FLAG_LOW_ACK_PWR        0x00000001
#define HAL_BT_COEX_FLAG_LOWER_TX_PWR       0x00000002
#define HAL_BT_COEX_FLAG_ANT_DIV_ALLOW      0x00000004    /* Check Rx Diversity is allowed */
#define HAL_BT_COEX_FLAG_ANT_DIV_ENABLE     0x00000008    /* Check Diversity is on or off */
#define HAL_BT_COEX_FLAG_MCI_MAX_TX_PWR     0x00000010
#define HAL_BT_COEX_FLAG_MCI_FTP_STOMP_RX   0x00000020

#define HAL_BT_COEX_ANTDIV_CONTROL1_ENABLE  0x0b
#define HAL_BT_COEX_ANTDIV_CONTROL2_ENABLE  0x09          /* main: LNA1, alt: LNA2 */
#define HAL_BT_COEX_ANTDIV_CONTROL1_FIXED_A 0x04
#define HAL_BT_COEX_ANTDIV_CONTROL2_FIXED_A 0x09
#define HAL_BT_COEX_ANTDIV_CONTROL1_FIXED_B 0x02
#define HAL_BT_COEX_ANTDIV_CONTROL2_FIXED_B 0x06

#define HAL_BT_COEX_ISOLATION_FOR_NO_COEX   30

#define HAL_BT_COEX_ANT_DIV_SWITCH_COM      0x66666666

#define HAL_BT_COEX_HELIUS_CHAINMASK        0x02

#define HAL_BT_COEX_LOW_ACK_POWER           0x0
#define HAL_BT_COEX_HIGH_ACK_POWER          0x3f3f3f

#define HAL_BT_COEX_REDUCE_LEGACY_RATE_PWR  20

typedef enum {
    HAL_BT_COEX_NO_STOMP = 0,
    HAL_BT_COEX_STOMP_ALL,
    HAL_BT_COEX_STOMP_LOW,
    HAL_BT_COEX_STOMP_NONE,
    HAL_BT_COEX_STOMP_ALL_FORCE,
    HAL_BT_COEX_STOMP_LOW_FORCE,
} HAL_BT_COEX_STOMP_TYPE;

/*
 * Weight table configurations.
 */
#define AR5416_BT_WGHT                     0xff55
#define AR5416_STOMP_ALL_WLAN_WGHT         0xfcfc
#define AR5416_STOMP_LOW_WLAN_WGHT         0xa8a8
#define AR5416_STOMP_NONE_WLAN_WGHT        0x0000
#define AR5416_STOMP_ALL_FORCE_WLAN_WGHT   0xffff       // Stomp BT even when WLAN is idle
#define AR5416_STOMP_LOW_FORCE_WLAN_WGHT   0xaaaa       // Stomp BT even when WLAN is idle

#define AR9300_BT_WGHT                     0xcccc4444
#define AR9300_STOMP_ALL_WLAN_WGHT0        0xfffffff0
#define AR9300_STOMP_ALL_WLAN_WGHT1        0xfffffff0
#define AR9300_STOMP_LOW_WLAN_WGHT0        0x88888880
#define AR9300_STOMP_LOW_WLAN_WGHT1        0x88888880
#define AR9300_STOMP_NONE_WLAN_WGHT0       0x00000000
#define AR9300_STOMP_NONE_WLAN_WGHT1       0x00000000
#define AR9300_STOMP_ALL_FORCE_WLAN_WGHT0  0xffffffff   // Stomp BT even when WLAN is idle
#define AR9300_STOMP_ALL_FORCE_WLAN_WGHT1  0xffffffff
#define AR9300_STOMP_LOW_FORCE_WLAN_WGHT0  0x88888888   // Stomp BT even when WLAN is idle
#define AR9300_STOMP_LOW_FORCE_WLAN_WGHT1  0x88888888

#define JUPITER_STOMP_ALL_WLAN_WGHT0       0x01017d01
#define JUPITER_STOMP_ALL_WLAN_WGHT1       0x41414101
#define JUPITER_STOMP_ALL_WLAN_WGHT2       0x41414101
#define JUPITER_STOMP_ALL_WLAN_WGHT3       0x41414141
#define JUPITER_STOMP_LOW_WLAN_WGHT0       0x01017d01
#define JUPITER_STOMP_LOW_WLAN_WGHT1       0x3b3b3b01
#define JUPITER_STOMP_LOW_WLAN_WGHT2       0x3b3b3b01
#define JUPITER_STOMP_LOW_WLAN_WGHT3       0x3b3b3b3b
#define JUPITER_STOMP_LOW_FTP_WLAN_WGHT0   0x01017d01
#define JUPITER_STOMP_LOW_FTP_WLAN_WGHT1   0x013b0101
#define JUPITER_STOMP_LOW_FTP_WLAN_WGHT2   0x3b3b0101
#define JUPITER_STOMP_LOW_FTP_WLAN_WGHT3   0x3b3b013b
#define JUPITER_STOMP_NONE_WLAN_WGHT0      0x01017d01
#define JUPITER_STOMP_NONE_WLAN_WGHT1      0x01010101
#define JUPITER_STOMP_NONE_WLAN_WGHT2      0x01010101
#define JUPITER_STOMP_NONE_WLAN_WGHT3      0x01010101
#define JUPITER_STOMP_ALL_FORCE_WLAN_WGHT0 0x01017d7d
#define JUPITER_STOMP_ALL_FORCE_WLAN_WGHT1 0x7d7d7d01
#define JUPITER_STOMP_ALL_FORCE_WLAN_WGHT2 0x7d7d7d7d
#define JUPITER_STOMP_ALL_FORCE_WLAN_WGHT3 0x7d7d7d7d
#define JUPITER_STOMP_LOW_FORCE_WLAN_WGHT0 0x01013b3b
#define JUPITER_STOMP_LOW_FORCE_WLAN_WGHT1 0x3b3b3b01
#define JUPITER_STOMP_LOW_FORCE_WLAN_WGHT2 0x3b3b3b3b
#define JUPITER_STOMP_LOW_FORCE_WLAN_WGHT3 0x3b3b3b3b

#define MCI_CONCUR_TX_WLAN_WGHT1_MASK      0xff000000
#define MCI_CONCUR_TX_WLAN_WGHT1_MASK_S    24
#define MCI_CONCUR_TX_WLAN_WGHT2_MASK      0x00ff0000
#define MCI_CONCUR_TX_WLAN_WGHT2_MASK_S    16
#define MCI_CONCUR_TX_WLAN_WGHT3_MASK      0x000000ff
#define MCI_CONCUR_TX_WLAN_WGHT3_MASK_S    0
#define MCI_CONCUR_TX_WLAN_WGHT3_MASK2     0x00ff0000
#define MCI_CONCUR_TX_WLAN_WGHT3_MASK2_S   16

typedef struct {
    u_int8_t          bt_time_extend;      /* extend rx_clear after tx/rx to protect
                                            * the burst (in usec). */
    bool              bt_txstate_extend;   /* extend rx_clear as long as txsm is
                                            * transmitting or waiting for ack. */
    bool              bt_txframe_extend;   /* extend rx_clear so that when tx_frame
                                            * is asserted, rx_clear will drop. */
    HAL_BT_COEX_MODE  bt_mode;             /* coexistence mode */
    bool              bt_quiet_collision;  /* treat BT high priority traffic as
                                            * a quiet collision */
    bool              bt_rxclear_polarity; /* invert rx_clear as WLAN_ACTIVE */
    u_int8_t          bt_priority_time;    /* slotted mode only. indicate the time in usec
                                            * from the rising edge of BT_ACTIVE to the time
                                            * BT_PRIORITY can be sampled to indicate priority. */
    u_int8_t          bt_first_slot_time;  /* slotted mode only. indicate the time in usec
                                            * from the rising edge of BT_ACTIVE to the time
                                            * BT_PRIORITY can be sampled to indicate tx/rx and
                                            * BT_FREQ is sampled. */
    bool              bt_hold_rxclear;     /* slotted mode only. rx_clear and bt_ant decision
                                            * will be held the entire time that BT_ACTIVE is asserted,
                                            * otherwise the decision is made before every slot boundry. */
} HAL_BT_COEX_CONFIG;
#endif

#if ATH_SUPPORT_MCI

#define MCI_QUERY_BT_VERSION_VERBOSE            0
#define MCI_LINKID_INDEX_MGMT_PENDING           1

#define HAL_MCI_FLAG_DISABLE_TIMESTAMP      0x00000001      /* Disable time stamp */

typedef enum mci_message_header {
    MCI_LNA_CTRL     = 0x10,        /* len = 0 */
    MCI_CONT_NACK    = 0x20,        /* len = 0 */
    MCI_CONT_INFO    = 0x30,        /* len = 4 */
    MCI_CONT_RST     = 0x40,        /* len = 0 */
    MCI_SCHD_INFO    = 0x50,        /* len = 16 */
    MCI_CPU_INT      = 0x60,        /* len = 4 */
    MCI_SYS_WAKING   = 0x70,        /* len = 0 */
    MCI_GPM          = 0x80,        /* len = 16 */
    MCI_LNA_INFO     = 0x90,        /* len = 1 */
    MCI_LNA_STATE    = 0x94,
    MCI_LNA_TAKE     = 0x98,
    MCI_LNA_TRANS    = 0x9c,
    MCI_SYS_SLEEPING = 0xa0,        /* len = 0 */
    MCI_REQ_WAKE     = 0xc0,        /* len = 0 */
    MCI_DEBUG_16     = 0xfe,        /* len = 2 */
    MCI_REMOTE_RESET = 0xff         /* len = 16 */
} MCI_MESSAGE_HEADER;

/* Default remote BT device MCI COEX version */
#define MCI_GPM_COEX_MAJOR_VERSION_DEFAULT  3
#define MCI_GPM_COEX_MINOR_VERSION_DEFAULT  0
/* Local WLAN MCI COEX version */
#define MCI_GPM_COEX_MAJOR_VERSION_WLAN     3
#define MCI_GPM_COEX_MINOR_VERSION_WLAN     0

typedef enum mci_gpm_subtype {
    MCI_GPM_BT_CAL_REQ      = 0,
    MCI_GPM_BT_CAL_GRANT    = 1,
    MCI_GPM_BT_CAL_DONE     = 2,
    MCI_GPM_WLAN_CAL_REQ    = 3,
    MCI_GPM_WLAN_CAL_GRANT  = 4,
    MCI_GPM_WLAN_CAL_DONE   = 5,
    MCI_GPM_COEX_AGENT      = 0x0C,
    MCI_GPM_RSVD_PATTERN    = 0xFE,
    MCI_GPM_RSVD_PATTERN32  = 0xFEFEFEFE,
    MCI_GPM_BT_DEBUG        = 0xFF
} MCI_GPM_SUBTYPE_T;

typedef enum mci_gpm_coex_opcode {
    MCI_GPM_COEX_VERSION_QUERY      = 0,
    MCI_GPM_COEX_VERSION_RESPONSE   = 1,
    MCI_GPM_COEX_STATUS_QUERY       = 2,
    MCI_GPM_COEX_HALT_BT_GPM        = 3,
    MCI_GPM_COEX_WLAN_CHANNELS      = 4,
    MCI_GPM_COEX_BT_PROFILE_INFO    = 5,
    MCI_GPM_COEX_BT_STATUS_UPDATE   = 6,
    MCI_GPM_COEX_BT_UPDATE_FLAGS    = 7
} MCI_GPM_COEX_OPCODE_T;

typedef enum mci_gpm_coex_query_type {
    /* WLAN information */
    MCI_GPM_COEX_QUERY_WLAN_ALL_INFO    = 0x01,
    /* BT information */
    MCI_GPM_COEX_QUERY_BT_ALL_INFO      = 0x01,
    MCI_GPM_COEX_QUERY_BT_TOPOLOGY      = 0x02,
    MCI_GPM_COEX_QUERY_BT_DEBUG         = 0x04
} MCI_GPM_COEX_QUERY_TYPE_T;

typedef enum mci_gpm_coex_halt_bt_gpm {
    MCI_GPM_COEX_BT_GPM_UNHALT      = 0,
    MCI_GPM_COEX_BT_GPM_HALT        = 1
} MCI_GPM_COEX_HALT_BT_GPM_T;

typedef enum mci_gpm_coex_profile_type {
    MCI_GPM_COEX_PROFILE_UNKNOWN    = 0,
    MCI_GPM_COEX_PROFILE_RFCOMM     = 1,
    MCI_GPM_COEX_PROFILE_A2DP       = 2,
    MCI_GPM_COEX_PROFILE_HID        = 3,
    MCI_GPM_COEX_PROFILE_BNEP       = 4,
    MCI_GPM_COEX_PROFILE_VOICE      = 5,
    MCI_GPM_COEX_PROFILE_MAX
} MCI_GPM_COEX_PROFILE_TYPE_T;

typedef enum mci_gpm_coex_profile_state {
    MCI_GPM_COEX_PROFILE_STATE_END      = 0,
    MCI_GPM_COEX_PROFILE_STATE_START    = 1
} MCI_GPM_COEX_PROFILE_STATE_T;

typedef enum mci_gpm_coex_profile_role {
    MCI_GPM_COEX_PROFILE_SLAVE      = 0,
    MCI_GPM_COEX_PROFILE_MASTER     = 1
} MCI_GPM_COEX_PROFILE_ROLE_T;

typedef enum mci_gpm_coex_bt_status_type {
    MCI_GPM_COEX_BT_NONLINK_STATUS  = 0,
    MCI_GPM_COEX_BT_LINK_STATUS     = 1
} MCI_GPM_COEX_BT_STATUS_TYPE_T;

typedef enum mci_gpm_coex_bt_status_state {
    MCI_GPM_COEX_BT_NORMAL_STATUS   = 0,
    MCI_GPM_COEX_BT_CRITICAL_STATUS = 1
} MCI_GPM_COEX_BT_STATUS_STATE_T;

#define MCI_GPM_INVALID_PROFILE_HANDLE  0xff

typedef enum mci_gpm_coex_bt_updata_flags_op {
    MCI_GPM_COEX_BT_FLAGS_READ          = 0x00,
    MCI_GPM_COEX_BT_FLAGS_SET           = 0x01,
    MCI_GPM_COEX_BT_FLAGS_CLEAR         = 0x02
} MCI_GPM_COEX_BT_FLAGS_OP_T;

/* MCI GPM/Coex opcode/type definitions */
enum {
    MCI_GPM_COEX_W_GPM_PAYLOAD      = 1,
    MCI_GPM_COEX_B_GPM_TYPE         = 4,
    MCI_GPM_COEX_B_GPM_OPCODE       = 5,
    /* MCI_GPM_WLAN_CAL_REQ, MCI_GPM_WLAN_CAL_DONE */
    MCI_GPM_WLAN_CAL_W_SEQUENCE     = 2,
    /* MCI_GPM_COEX_VERSION_QUERY */
    /* MCI_GPM_COEX_VERSION_RESPONSE */
    MCI_GPM_COEX_B_MAJOR_VERSION    = 6,
    MCI_GPM_COEX_B_MINOR_VERSION    = 7,
    /* MCI_GPM_COEX_STATUS_QUERY */
    MCI_GPM_COEX_B_BT_BITMAP        = 6,
    MCI_GPM_COEX_B_WLAN_BITMAP      = 7,
    /* MCI_GPM_COEX_HALT_BT_GPM */
    MCI_GPM_COEX_B_HALT_STATE       = 6,
    /* MCI_GPM_COEX_WLAN_CHANNELS */
    MCI_GPM_COEX_B_CHANNEL_MAP      = 6,
    /* MCI_GPM_COEX_BT_PROFILE_INFO */
    MCI_GPM_COEX_B_PROFILE_TYPE     = 6,
    MCI_GPM_COEX_B_PROFILE_LINKID   = 7,
    MCI_GPM_COEX_B_PROFILE_STATE    = 8,
    MCI_GPM_COEX_B_PROFILE_ROLE     = 9,
    MCI_GPM_COEX_B_PROFILE_RATE     = 10,
    MCI_GPM_COEX_B_PROFILE_VOTYPE   = 11,
    MCI_GPM_COEX_H_PROFILE_T        = 12,
    MCI_GPM_COEX_B_PROFILE_W        = 14,
    MCI_GPM_COEX_B_PROFILE_A        = 15,
    /* MCI_GPM_COEX_BT_STATUS_UPDATE */
    MCI_GPM_COEX_B_STATUS_TYPE      = 6,
    MCI_GPM_COEX_B_STATUS_LINKID    = 7,
    MCI_GPM_COEX_B_STATUS_STATE     = 8,
    /* MCI_GPM_COEX_BT_UPDATE_FLAGS */
    MCI_GPM_COEX_B_BT_FLAGS_OP      = 10,
    MCI_GPM_COEX_W_BT_FLAGS         = 6
};

#define MCI_GPM_RECYCLE(_p_gpm) \
    {                           \
        *(((u_int32_t *)(_p_gpm)) + MCI_GPM_COEX_W_GPM_PAYLOAD) = MCI_GPM_RSVD_PATTERN32; \
    }
#define MCI_GPM_TYPE(_p_gpm)    \
    (*(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_GPM_TYPE) & 0xff)
#define MCI_GPM_OPCODE(_p_gpm)  \
    (*(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_GPM_OPCODE) & 0xff)

#define MCI_GPM_SET_CAL_TYPE(_p_gpm, _cal_type)             \
    {                                                       \
        *(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_GPM_TYPE) = (_cal_type) & 0xff; \
    }
#define MCI_GPM_SET_TYPE_OPCODE(_p_gpm, _type, _opcode)     \
    {                                                       \
        *(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_GPM_TYPE) = (_type) & 0xff;     \
        *(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_GPM_OPCODE) = (_opcode) & 0xff;   \
    }
#define MCI_GPM_IS_CAL_TYPE(_type) ((_type) <= MCI_GPM_WLAN_CAL_DONE)

#define MCI_NUM_BT_CHANNELS     79

#define MCI_GPM_SET_CHANNEL_BIT(_p_gpm, _bt_chan)                   \
    {                                                               \
        if (_bt_chan < MCI_NUM_BT_CHANNELS) {                       \
            *(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_CHANNEL_MAP + \
                (_bt_chan / 8)) |= 1 << (_bt_chan & 7);             \
        }                                                           \
    }

#define MCI_GPM_CLR_CHANNEL_BIT(_p_gpm, _bt_chan)                   \
    {                                                               \
        if (_bt_chan < MCI_NUM_BT_CHANNELS) {                       \
            *(((u_int8_t *)(_p_gpm)) + MCI_GPM_COEX_B_CHANNEL_MAP + \
                (_bt_chan / 8)) &= ~(1 << (_bt_chan & 7));          \
        }                                                           \
    }

#define HAL_MCI_INTERRUPT_SW_MSG_DONE            0x00000001
#define HAL_MCI_INTERRUPT_CPU_INT_MSG            0x00000002
#define HAL_MCI_INTERRUPT_RX_CHKSUM_FAIL         0x00000004
#define HAL_MCI_INTERRUPT_RX_INVALID_HDR         0x00000008
#define HAL_MCI_INTERRUPT_RX_HW_MSG_FAIL         0x00000010
#define HAL_MCI_INTERRUPT_RX_SW_MSG_FAIL         0x00000020
#define HAL_MCI_INTERRUPT_TX_HW_MSG_FAIL         0x00000080
#define HAL_MCI_INTERRUPT_TX_SW_MSG_FAIL         0x00000100
#define HAL_MCI_INTERRUPT_RX_MSG                 0x00000200
#define HAL_MCI_INTERRUPT_REMOTE_SLEEP_UPDATE    0x00000400
#define HAL_MCI_INTERRUPT_CONT_INFO_TIMEOUT      0x80000000
#define HAL_MCI_INTERRUPT_MSG_FAIL_MASK ( HAL_MCI_INTERRUPT_RX_HW_MSG_FAIL | \
                                          HAL_MCI_INTERRUPT_RX_SW_MSG_FAIL | \
                                          HAL_MCI_INTERRUPT_TX_HW_MSG_FAIL | \
                                          HAL_MCI_INTERRUPT_TX_SW_MSG_FAIL )

#define HAL_MCI_INTERRUPT_RX_MSG_REMOTE_RESET    0x00000001
#define HAL_MCI_INTERRUPT_RX_MSG_LNA_CONTROL     0x00000002
#define HAL_MCI_INTERRUPT_RX_MSG_CONT_NACK       0x00000004
#define HAL_MCI_INTERRUPT_RX_MSG_CONT_INFO       0x00000008
#define HAL_MCI_INTERRUPT_RX_MSG_CONT_RST        0x00000010
#define HAL_MCI_INTERRUPT_RX_MSG_SCHD_INFO       0x00000020
#define HAL_MCI_INTERRUPT_RX_MSG_CPU_INT         0x00000040
#define HAL_MCI_INTERRUPT_RX_MSG_GPM             0x00000100
#define HAL_MCI_INTERRUPT_RX_MSG_LNA_INFO        0x00000200
#define HAL_MCI_INTERRUPT_RX_MSG_SYS_SLEEPING    0x00000400
#define HAL_MCI_INTERRUPT_RX_MSG_SYS_WAKING      0x00000800
#define HAL_MCI_INTERRUPT_RX_MSG_REQ_WAKE        0x00001000
#define HAL_MCI_INTERRUPT_RX_MSG_MONITOR         (HAL_MCI_INTERRUPT_RX_MSG_LNA_CONTROL | \
                                                  HAL_MCI_INTERRUPT_RX_MSG_LNA_INFO    | \
                                                  HAL_MCI_INTERRUPT_RX_MSG_CONT_NACK   | \
                                                  HAL_MCI_INTERRUPT_RX_MSG_CONT_INFO   | \
                                                  HAL_MCI_INTERRUPT_RX_MSG_CONT_RST)

typedef enum mci_bt_state {
    MCI_BT_SLEEP,
    MCI_BT_AWAKE,
    MCI_BT_CAL_START,
    MCI_BT_CAL
} MCI_BT_STATE_T;

/* Type of state query */
typedef enum mci_state_type {
    HAL_MCI_STATE_ENABLE,
    HAL_MCI_STATE_INIT_GPM_OFFSET,
    HAL_MCI_STATE_NEXT_GPM_OFFSET,
    HAL_MCI_STATE_LAST_GPM_OFFSET,
    HAL_MCI_STATE_BT,
    HAL_MCI_STATE_SET_BT_SLEEP,
    HAL_MCI_STATE_SET_BT_AWAKE,
    HAL_MCI_STATE_SET_BT_CAL_START,
    HAL_MCI_STATE_SET_BT_CAL,
    HAL_MCI_STATE_LAST_SCHD_MSG_OFFSET,
    HAL_MCI_STATE_REMOTE_SLEEP,
    HAL_MCI_STATE_CONT_RSSI_POWER,
    HAL_MCI_STATE_CONT_PRIORITY,
    HAL_MCI_STATE_CONT_TXRX,
    HAL_MCI_STATE_RESET_REQ_WAKE,
    HAL_MCI_STATE_SEND_WLAN_COEX_VERSION,
    HAL_MCI_STATE_SET_BT_COEX_VERSION,
    HAL_MCI_STATE_SEND_WLAN_CHANNELS,
    HAL_MCI_STATE_SEND_VERSION_QUERY,
    HAL_MCI_STATE_SEND_STATUS_QUERY,
    HAL_MCI_STATE_NEED_FLUSH_BT_INFO,
    HAL_MCI_STATE_SET_CONCUR_TX_PRI,
    HAL_MCI_STATE_RECOVER_RX,
    HAL_MCI_STATE_NEED_FTP_STOMP,
    HAL_MCI_STATE_NEED_TUNING,
    HAL_MCI_STATE_SHARED_CHAIN_CONCUR_TX,
    HAL_MCI_STATE_DEBUG,
    HAL_MCI_STATE_MAX
} HAL_MCI_STATE_TYPE;

#define HAL_MCI_STATE_DEBUG_REQ_BT_DEBUG    1

#define HAL_MCI_BT_MCI_FLAGS_UPDATE_CORR          0x00000002
#define HAL_MCI_BT_MCI_FLAGS_UPDATE_HDR           0x00000004
#define HAL_MCI_BT_MCI_FLAGS_UPDATE_PLD           0x00000008
#define HAL_MCI_BT_MCI_FLAGS_LNA_CTRL             0x00000010
#define HAL_MCI_BT_MCI_FLAGS_DEBUG                0x00000020
#define HAL_MCI_BT_MCI_FLAGS_SCHED_MSG            0x00000040
#define HAL_MCI_BT_MCI_FLAGS_CONT_MSG             0x00000080
#define HAL_MCI_BT_MCI_FLAGS_COEX_GPM             0x00000100
#define HAL_MCI_BT_MCI_FLAGS_CPU_INT_MSG          0x00000200
#define HAL_MCI_BT_MCI_FLAGS_MCI_MODE             0x00000400
#define HAL_MCI_BT_MCI_FLAGS_EGRET_MODE           0x00000800
#define HAL_MCI_BT_MCI_FLAGS_JUPITER_MODE         0x00001000
#define HAL_MCI_BT_MCI_FLAGS_OTHER                0x00010000

#define HAL_MCI_DEFAULT_BT_MCI_FLAGS        0x00011dde
/*
    HAL_MCI_BT_MCI_FLAGS_UPDATE_CORR  = 1
    HAL_MCI_BT_MCI_FLAGS_UPDATE_HDR   = 1
    HAL_MCI_BT_MCI_FLAGS_UPDATE_PLD   = 1
    HAL_MCI_BT_MCI_FLAGS_LNA_CTRL     = 1
    HAL_MCI_BT_MCI_FLAGS_DEBUG        = 0
    HAL_MCI_BT_MCI_FLAGS_SCHED_MSG    = 1
    HAL_MCI_BT_MCI_FLAGS_CONT_MSG     = 1
    HAL_MCI_BT_MCI_FLAGS_COEX_GPM     = 1
    HAL_MCI_BT_MCI_FLAGS_CPU_INT_MSG  = 0
    HAL_MCI_BT_MCI_FLAGS_MCI_MODE     = 1
    HAL_MCI_BT_MCI_FLAGS_EGRET_MODE   = 1
    HAL_MCI_BT_MCI_FLAGS_JUPITER_MODE = 1
    HAL_MCI_BT_MCI_FLAGS_OTHER        = 1
*/

#define HAL_MCI_TOGGLE_BT_MCI_FLAGS \
    (   HAL_MCI_BT_MCI_FLAGS_UPDATE_CORR    |   \
        HAL_MCI_BT_MCI_FLAGS_UPDATE_HDR     |   \
        HAL_MCI_BT_MCI_FLAGS_UPDATE_PLD     |   \
        HAL_MCI_BT_MCI_FLAGS_MCI_MODE   )

#define HAL_MCI_2G_FLAGS_CLEAR_MASK         0x00000000
#define HAL_MCI_2G_FLAGS_SET_MASK           HAL_MCI_TOGGLE_BT_MCI_FLAGS
#define HAL_MCI_2G_FLAGS                    HAL_MCI_DEFAULT_BT_MCI_FLAGS

#define HAL_MCI_5G_FLAGS_CLEAR_MASK         HAL_MCI_TOGGLE_BT_MCI_FLAGS
#define HAL_MCI_5G_FLAGS_SET_MASK           0x00000000
#define HAL_MCI_5G_FLAGS                    (HAL_MCI_DEFAULT_BT_MCI_FLAGS & \
                                            ~HAL_MCI_TOGGLE_BT_MCI_FLAGS)
    
#define HAL_MCI_GPM_NOMORE  0
#define HAL_MCI_GPM_MORE    1
#define HAL_MCI_GPM_INVALID 0xffffffff    

#define ATH_AIC_MAX_BT_CHANNEL          79

/*
 * Default value for Jupiter   is 0x00002201
 * Default value for Aphrodite is 0x00002282
 */
#define ATH_MCI_CONFIG_CONCUR_TX            0x00000003
#define ATH_MCI_CONFIG_MCI_OBS_MCI          0x00000004
#define ATH_MCI_CONFIG_MCI_OBS_TXRX         0x00000008
#define ATH_MCI_CONFIG_MCI_OBS_BT           0x00000010
#define ATH_MCI_CONFIG_DISABLE_MCI_CAL      0x00000020
#define ATH_MCI_CONFIG_DISABLE_OSLA         0x00000040
#define ATH_MCI_CONFIG_DISABLE_FTP_STOMP    0x00000080
#define ATH_MCI_CONFIG_AGGR_THRESH          0x00000700
#define ATH_MCI_CONFIG_AGGR_THRESH_S        8
#define ATH_MCI_CONFIG_DISABLE_AGGR_THRESH  0x00000800
#define ATH_MCI_CONFIG_CLK_DIV              0x00003000
#define ATH_MCI_CONFIG_CLK_DIV_S            12
#define ATH_MCI_CONFIG_DISABLE_TUNING       0x00004000
#define ATH_MCI_CONFIG_MCI_WEIGHT_DBG       0x40000000
#define ATH_MCI_CONFIG_DISABLE_MCI          0x80000000

#define ATH_MCI_CONFIG_MCI_OBS_MASK     ( ATH_MCI_CONFIG_MCI_OBS_MCI | \
                                          ATH_MCI_CONFIG_MCI_OBS_TXRX | \
                                          ATH_MCI_CONFIG_MCI_OBS_BT )
#define ATH_MCI_CONFIG_MCI_OBS_GPIO     0x0000002F

#define ATH_MCI_CONCUR_TX_SHARED_CHN    0x01
#define ATH_MCI_CONCUR_TX_UNSHARED_CHN  0x02
#define ATH_MCI_CONCUR_TX_DEBUG         0x03

/* 
 * The values below come from the system team test result.
 * For Jupiter, BT tx power level is from 0(-20dBm) to 6(4dBm).
 * Lowest WLAN tx power would be in bit[23:16] of dword 1.
 */
static const u_int32_t mci_concur_tx_max_pwr[4][8] =
    { /* No limit */
      {0x7f7f7f7f, 0x7f7f7f7f, 0x7f7f7f7f, 0x7f7f7f7f,
       0x7f7f7f7f, 0x7f7f7f7f, 0x7f7f7f7f, 0x7f7f7f7f},
      /* 11G */
      {0x16161616, 0x12121516, 0x12121212, 0x12121212,
       0x12121212, 0x12121212, 0x12121212, 0x7f121212},
      /* HT20 */
      {0x15151515, 0x14141515, 0x14141414, 0x14141414,
       0x14141414, 0x14141414, 0x14141414, 0x7f141414},
      /* HT40 */
      {0x10101010, 0x10101010, 0x10101010, 0x10101010,
       0x10101010, 0x10101010, 0x10101010, 0x7f101010}};
#define ATH_MCI_CONCUR_TX_LOWEST_PWR_MASK     0x00ff0000
#define ATH_MCI_CONCUR_TX_LOWEST_PWR_MASK_S   16
#endif

/* SPECTRAL SCAN defines begin */
typedef struct {
    u_int16_t   ss_fft_period;               /* Skip interval for FFT reports */
    u_int16_t   ss_period;                   /* Spectral scan period */
    u_int16_t   ss_count;                    /* # of reports to return from
                                                ss_active */
    u_int16_t   ss_short_report;             /* Set to report ony 1 set of FFT
                                                results */
    u_int8_t    radar_bin_thresh_sel;
    u_int16_t   ss_spectral_pri;             /* Priority, and are we doing a
                                                noise power cal ? */
    u_int16_t   ss_fft_size;                 /* Defines the number of FFT data
                                                points to compute, defined
                                                as a log index:
                                                num_fft_pts = 2^ss_fft_size */
    u_int16_t   ss_gc_ena;                   /* Set, to enable targeted gain
                                                change before starting the
                                                spectral scan FFT */
    u_int16_t   ss_restart_ena;              /* Set, to enable abort of receive
                                                frames when in high priority
                                                and a spectral scan is queued */
    u_int16_t   ss_noise_floor_ref;          /* Noise floor reference number 
                                                signed) for the calculation
                                                of bin power (dBm).
                                                Though stored as an unsigned
                                                value, this should be treated
                                                as a signed 8-bit int. */
    u_int16_t   ss_init_delay;               /* Disallow spectral scan triggers
                                                after tx/rx packets by setting
                                                this delay value to roughly
                                                SIFS time period or greater.
                                                Delay timer counts in units of
                                                0.25us */
    u_int16_t   ss_nb_tone_thr;              /* Number of strong bins
                                                (inclusive) per sub-channel,
                                                below which a signal is declared
                                                a narrowband tone */
    u_int16_t   ss_str_bin_thr;              /* Bin/max_bin ratio threshold over
                                                which a bin is declared strong
                                                (for spectral scan bandwidth
                                                analysis). */
    u_int16_t   ss_wb_rpt_mode;              /* Set this bit to report spectral
                                                scans as EXT_BLOCKER
                                                (phy_error=36), if none of the
                                                sub-channels are deemed
                                                narrowband. */
    u_int16_t   ss_rssi_rpt_mode;            /* Set this bit to report spectral
                                                scans as EXT_BLOCKER
                                                (phy_error=36), if the ADC RSSI
                                                is below the threshold
                                                ss_rssi_thr */
    u_int16_t   ss_rssi_thr;                 /* ADC RSSI must be greater than or
                                                equal to this threshold
                                                (signed Db) to ensure spectral
                                                scan reporting with normal phy 
                                                error codes (please see
                                                ss_rssi_rpt_mode above).
                                                Though stored as an unsigned
                                                value, this should be treated
                                                as a signed 8-bit int. */
    u_int16_t   ss_pwr_format;               /* Format of frequency bin
                                                magnitude for spectral scan
                                                triggered FFTs:
                                                0: linear magnitude
                                                1: log magnitude
                                                   (20*log10(lin_mag),
                                                    1/2 dB step size) */
    u_int16_t   ss_rpt_mode;                 /* Format of per-FFT reports to
                                                software for spectral scan
                                                triggered FFTs.
                                                0: No FFT report
                                                   (only pulse end summary)
                                                1: 2-dword summary of metrics
                                                   for each completed FFT
                                                2: 2-dword summary +
                                                   1x-oversampled bins(in-band)
                                                   per FFT
                                                3: 2-dword summary +
                                                   2x-oversampled bins (all)
                                                   per FFT */
    u_int16_t   ss_bin_scale;                /* Number of LSBs to shift out to 
                                                scale the FFT bins for spectral
                                                scan triggered FFTs. */
    u_int16_t   ss_dbm_adj;                  /* Set (with ss_pwr_format=1), to 
                                                report bin magnitudes converted
                                                to dBm power using the
                                                noisefloor calibration
                                                results. */
    u_int16_t   ss_chn_mask;                 /* Per chain enable mask to select
                                                input ADC for search FFT. */
    int8_t      ss_nf_cal[AH_MAX_CHAINS*2];  /* nf calibrated values for
                                                ctl+ext */
    int8_t      ss_nf_pwr[AH_MAX_CHAINS*2];  /* nf pwr values for ctl+ext */
    int32_t     ss_nf_temp_data;             /* temperature data taken during
                                                nf scan */ 
} HAL_SPECTRAL_PARAM;
#define HAL_SPECTRAL_PARAM_NOVAL  0xFFFF
#define HAL_SPECTRAL_PARAM_ENABLE 0x8000  /* Enable/Disable if applicable */

/* 
 * Noise power data definitions   
 * units are: 4 x dBm - NOISE_PWR_DATA_OFFSET (e.g. -25 = (-25/4 - 90) = -96.25 dBm)
 * range (for 6 signed bits) is (-32 to 31) + offset => -122dBm to -59dBm  
 * resolution (2 bits) is 0.25dBm 
 */
#define NOISE_PWR_DATA_OFFSET           -90 /* dbm - all pwr report data is represented offset by this */
#define INT_2_NOISE_PWR_DBM(_p)         (((_p) - NOISE_PWR_DATA_OFFSET) << 2)
#define NOISE_PWR_DBM_2_INT(_p)         ((((_p) + 3) >> 2) + NOISE_PWR_DATA_OFFSET)
#define NOISE_PWR_DBM_2_DEC(_p)         (((-(_p)) & 3) * 25)
#define N2DBM(_x,_y)                    ((((_x) - NOISE_PWR_DATA_OFFSET) << 2) - (_y)/25)
/* SPECTRAL SCAN defines end */

typedef struct hal_calibration_timer {
    const u_int32_t    cal_timer_group;
    const u_int32_t    cal_timer_interval;
    u_int64_t          cal_timer_ts;
} HAL_CALIBRATION_TIMER;

typedef enum hal_cal_query {
    HAL_QUERY_CALS,
    HAL_QUERY_RERUN_CALS
} HAL_CAL_QUERY;

typedef struct mac_dbg_regs {
    u_int32_t dma_dbg_0;
    u_int32_t dma_dbg_1;
    u_int32_t dma_dbg_2;
    u_int32_t dma_dbg_3;
    u_int32_t dma_dbg_4;
    u_int32_t dma_dbg_5;
    u_int32_t dma_dbg_6;
    u_int32_t dma_dbg_7;
} mac_dbg_regs_t;

typedef enum hal_mac_hangs {
    dcu_chain_state = 0x1,
    dcu_complete_state = 0x2,
    qcu_state = 0x4,
    qcu_fsp_ok = 0x8,
    qcu_fsp_state = 0x10,
    qcu_stitch_state = 0x20,
    qcu_fetch_state = 0x40,
    qcu_complete_state = 0x80
} hal_mac_hangs_t;

typedef struct hal_mac_hang_check {
    u_int8_t dcu_chain_state;
    u_int8_t dcu_complete_state;
    u_int8_t qcu_state;
    u_int8_t qcu_fsp_ok;
    u_int8_t qcu_fsp_state;
    u_int8_t qcu_stitch_state;
    u_int8_t qcu_fetch_state;
    u_int8_t qcu_complete_state;
} hal_mac_hang_check_t;

typedef struct hal_bb_hang_check {
    u_int16_t hang_reg_offset;
    u_int32_t hang_val;
    u_int32_t hang_mask;
    u_int32_t hang_offset;
} hal_hw_hang_check_t; 

typedef enum hal_hw_hangs {
    HAL_DFS_BB_HANG_WAR          = 0x1,
    HAL_RIFS_BB_HANG_WAR         = 0x2,
    HAL_RX_STUCK_LOW_BB_HANG_WAR = 0x4,
    HAL_MAC_HANG_WAR             = 0x8,
    HAL_PHYRESTART_CLR_WAR       = 0x10,
    HAL_MAC_HANG_DETECTED        = 0x40000000,
    HAL_BB_HANG_DETECTED         = 0x80000000
} hal_hw_hangs_t;

#ifndef REMOVE_PKT_LOG
/* Hal packetlog defines */
typedef struct hal_log_data_ani {
    u_int8_t phy_stats_disable;
    u_int8_t noise_immun_lvl;
    u_int8_t spur_immun_lvl;
    u_int8_t ofdm_weak_det;
    u_int8_t cck_weak_thr;
    u_int16_t fir_lvl;
    u_int16_t listen_time;
    u_int32_t cycle_count;
    u_int32_t ofdm_phy_err_count;
    u_int32_t cck_phy_err_count;
    int8_t rssi;
    int32_t misc[8];            /* Can be used for HT specific or other misc info */
    /* TBD: Add other required log information */
} HAL_LOG_DATA_ANI;
/* Hal packetlog defines end*/
#endif

/*
 * MFP decryption options for initializing the MAC.
 */

typedef enum {
    HAL_MFP_QOSDATA = 0,        /* Decrypt MFP frames like QoS data frames. All chips before Merlin. */
    HAL_MFP_PASSTHRU,       /* Don't decrypt MFP frames at all. Passthrough */            
    HAL_MFP_HW_CRYPTO       /* hardware decryption enabled. Merlin can do it. */
} HAL_MFP_OPT_t;

/* LNA config supported */
typedef enum {
    HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2 = 0,
    HAL_ANT_DIV_COMB_LNA2            = 1,
    HAL_ANT_DIV_COMB_LNA1            = 2,
    HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2  = 3,
    HAL_ANT_DIV_COMB_LNA_MASK        = 4,
} HAL_ANT_DIV_COMB_LNA_CONF;

typedef enum {
    HAL_SWITCH_COM_ANTX = 0x0,    /* switch com antx */
    HAL_SWITCH_COM_ANT1 = 0x1,    /* switch com ant1 */
    HAL_SWITCH_COM_ANT2 = 0x2,    /* switch com ant2 */
    HAL_SWITCH_COM_ANT3 = 0x4,    /* switch com ant3 */
    HAL_SWITCH_COM_MASK = 0xf,    /* switch com mask */
} HAL_ANT_DIV_SWITCH;

#ifdef ATH_ANT_DIV_COMB
typedef struct {
    u_int8_t main_lna_conf;
    u_int8_t alt_lna_conf;
    u_int8_t fast_div_bias;
    u_int8_t main_gaintb;
    u_int8_t alt_gaintb;
    u_int8_t antdiv_configgroup;
    int8_t   lna1_lna2_delta;
    u_int8_t fast_div_disable;
    u_int8_t switch_com_r;
    u_int8_t switch_com_t;
} HAL_ANT_COMB_CONFIG;

#define DEFAULT_ANTDIV_CONFIG_GROUP      0x00
#define HAL_ANTDIV_CONFIG_GROUP_1        0x01   /* Hornet 1.1 */
#define HAL_ANTDIV_CONFIG_GROUP_2        0x02   /* Poseidon 1.1 */
#define HAL_ANTDIV_CONFIG_GROUP_3        0x03   /* not yet used */
#endif

/*
 * Flag for setting QUIET period
 */
typedef enum {
    HAL_QUIET_DISABLE             = 0x0,
    HAL_QUIET_ENABLE              = 0x1,
    HAL_QUIET_ADD_CURRENT_TSF     = 0x2, /* add current TSF to next_start offset */
    HAL_QUIET_ADD_SWBA_RESP_TIME  = 0x4, /* add beacon response time to next_start offset */
} HAL_QUIET_FLAG;

/*
** Transmit Beamforming Capabilities
*/  
#ifdef  ATH_SUPPORT_TxBF    
typedef struct hal_txbf_caps {
    u_int8_t channel_estimation_cap;
    u_int8_t csi_max_rows_bfer;
    u_int8_t comp_bfer_antennas;
    u_int8_t noncomp_bfer_antennas;
    u_int8_t csi_bfer_antennas;
    u_int8_t minimal_grouping;
    u_int8_t explicit_comp_bf;
    u_int8_t explicit_noncomp_bf;
    u_int8_t explicit_csi_feedback;
    u_int8_t explicit_comp_steering;
    u_int8_t explicit_noncomp_steering;
    u_int8_t explicit_csi_txbf_capable;
    u_int8_t calibration;
    u_int8_t implicit_txbf_capable;
    u_int8_t tx_ndp_capable;
    u_int8_t rx_ndp_capable;
    u_int8_t tx_staggered_sounding;
    u_int8_t rx_staggered_sounding;
    u_int8_t implicit_rx_capable;
} HAL_TXBF_CAPS;
#endif

/*
 * Generic Timer domain
 */
typedef enum {
    HAL_GEN_TIMER_TSF = 0,
    HAL_GEN_TIMER_TSF2,
    HAL_GEN_TIMER_TSF_ANY
} HAL_GEN_TIMER_DOMAIN;

/* ah_reset_reason */
enum{
    HAL_RESET_NONE = 0x0,
    HAL_RESET_BBPANIC = 0x1,
};

/*
 * Green Tx, Based on different RSSI of Received Beacon thresholds, 
 * using different tx power by modified register tx power related values.
 * The thresholds are decided by system team.
 */
#define GreenTX_thres1  56   /* in dB */
#define GreenTX_thres2  36   /* in dB */

typedef enum {
    HAL_RSSI_TX_POWER_NONE      = 0,
    HAL_RSSI_TX_POWER_SHORT     = 1,    /* short range, reduce OB/DB bias current and disable PAL */
    HAL_RSSI_TX_POWER_MIDDLE    = 2,    /* middle range, reduce OB/DB bias current and PAL is enabled */
    HAL_RSSI_TX_POWER_LONG      = 3,    /* long range, orig. OB/DB bias current and PAL is enabled */
} HAL_RSSI_TX_POWER;

/*
 * LDPC Support
 */
typedef enum {
    HAL_LDPC_NONE             = 0x0, /* Not support LDPC */
    HAL_LDPC_RX	              = 0x1, /* support LDPC decode */
    HAL_LDPC_TX               = 0x2, /* support LDPC encode */
    HAL_LDPC_TXRX             = 0x3, /* support LDPC encode and decode */
} HAL_LDPC_FLAG;

/*
** Forward Reference
*/      

struct ath_desc;
struct ath_rx_status;
struct ath_tx_status;

#if ATH_DRIVER_SIM

struct ath_hal_sim {
    u_int32_t __ahdecl(*ahsim_access_sim_reg)(struct ath_hal *ah, u_int32_t reg, u_int32_t val, bool read_access);

    u_int32_t __ahdecl(*ahsim_get_active_tx_queue)(struct ath_hal *ah);
    u_int32_t __ahdecl(*ahsim_distribute_transmit_packet)(struct ath_hal *src_ah, u_int8_t* alt_src, struct ath_hal *dst_ah, u_int32_t q, u_int64_t* ba_mask, bool *is_unicast_for_me);
    void __ahdecl(*ahsim_done_transmit)(struct ath_hal *ah, u_int32_t q, u_int32_t num_agg_pkts, u_int64_t ba_mask);
    bool __ahdecl(*ahsim_record_tx_c)(struct ath_hal* ah, u_int32_t q, bool intersim);
    void __ahdecl(*ahsim_rx_dummy_pkt)(struct ath_hal* ah, u_int8_t* dummy_pkt, u_int32_t dummy_pkt_bytes);
    u_int8_t* __ahdecl(*ahsim_get_tx_buffer)(struct ath_hal* ah, u_int32_t q, u_int32_t* frag_length);

    u_int16_t sim_index;
    
    spinlock_t tx_timer_lock;   
    os_timer_t tx_timer;
    bool tx_timer_active;

    spinlock_t reg_lock;

    bool is_monitor;
    u_int32_t output_pkt_snippet_length;
};
#endif

typedef struct halvowstats {
    u_int32_t   tx_frame_count;
    u_int32_t   rx_frame_count;
    u_int32_t   rx_clear_count;
    u_int32_t   cycle_count;
    u_int32_t   ext_cycle_count;
} HAL_VOWSTATS;

struct hal_bb_panic_info {
    u_int32_t status;
    u_int32_t tsf;
    u_int32_t phy_panic_wd_ctl1;
    u_int32_t phy_panic_wd_ctl2;
    u_int32_t phy_gen_ctrl;
    u_int32_t rxc_pcnt;
    u_int32_t rxf_pcnt;
    u_int32_t txf_pcnt;
    u_int32_t cycles;
    u_int32_t wd;
    u_int32_t det;
    u_int32_t rdar;
    u_int32_t r_odfm;
    u_int32_t r_cck;
    u_int32_t t_odfm;
    u_int32_t t_cck;
    u_int32_t agc;
    u_int32_t src;
};

#ifdef AH_ANALOG_SHADOW_READ
#define RF_BEGIN 0x7800
#define RF_END 0x7900
#endif

#if ATH_WOW_OFFLOAD
enum {
    WOW_PARAM_REPLAY_CNTR,
    WOW_PARAM_KEY_TSC,
    WOW_PARAM_TX_SEQNUM,
};
#endif /* ATH_WOW_OFFLOAD */

enum {
    AR_REG_TX_FRM_CNT    = 0x1,
    AR_REG_RX_FRM_CNT    = 0x2,
    AR_REG_RX_CLR_CNT    = 0x4,
    AR_REG_CYCLE_CNT     = 0x8,
    AR_REG_EXT_CYCLE_CNT = 0x10,
    AR_REG_VOW_ALL       = 0xff,
};

#ifdef AH_ANALOG_SHADOW_READ
#define RF_BEGIN 0x7800
#define RF_END 0x7900
#endif

#define HAL_MAX_SANTENNA 4
#define ATH_GPIOPIN_ANT_SERIAL_STROBE 0
#define ATH_GPIOFUNC_ANT_SERIAL_STROBE 71
#define ATH_GPIOPIN_ANT_SERIAL_DATA 1
#define ATH_GPIOFUNC_ANT_SERIAL_DATA 72
#define ATH_GPIOPIN_ANTCHAIN0 1
#define ATH_GPIOPIN_ANTCHAIN1 2
#define ATH_GPIOPIN_ANTCHAIN2 3
#define ATH_GPIOFUNC_ANTCHAIN0 0x47
#define ATH_GPIOFUNC_ANTCHAIN1 0x48
#define ATH_GPIOFUNC_ANTCHAIN2 0x28

struct smart_ant_gpio_conf {
    u_int32_t gpio_pin[HAL_MAX_SANTENNA];
    u_int32_t gpio_func[HAL_MAX_SANTENNA];
    u_int32_t gpio_pin_swcom3;
    u_int32_t gpio_func_swcom3;
    u_int32_t gpio_pin_strobe;
    u_int32_t gpio_func_strobe;
    u_int32_t gpio_pin_data;
    u_int32_t gpio_func_data;
};

/*
 * Hardware Access Layer (HAL) API.
 *
 * Clients of the HAL call ath_hal_attach to obtain a reference to an
 * ath_hal structure for use with the device.  Hardware-related operations
 * that follow must call back into the HAL through interface, supplying
 * the reference as the first parameter.  Note that before using the
 * reference returned by ath_hal_attach the caller should verify the
 * ABI version number.
 */
struct ath_hal {
    const HAL_RATE_TABLE *__ahdecl(*ah_get_rate_table)(struct ath_hal *,
                u_int mode);
    void      __ahdecl(*ah_detach)(struct ath_hal*);

    /* Reset functions */
    bool  __ahdecl(*ah_reset)(struct ath_hal *, HAL_OPMODE,
                HAL_CHANNEL *, HAL_HT_MACMODE, u_int8_t,
                                u_int8_t, HAL_HT_EXTPROTSPACING,
                                bool b_channel_change, HAL_STATUS *status,
                                int is_scan);

    bool  __ahdecl(*ah_phy_disable)(struct ath_hal *);
    bool  __ahdecl(*ah_disable)(struct ath_hal *);
        void  __ahdecl(*ah_config_pci_power_save)(struct ath_hal *, int, int);
    void      __ahdecl(*ah_set_pcu_config)(struct ath_hal *);
    bool  __ahdecl(*ah_per_calibration)(struct ath_hal*, HAL_CHANNEL *, 
                u_int8_t, bool, bool *, int, u_int32_t *);
    void      __ahdecl(*ah_reset_cal_valid)(struct ath_hal*, HAL_CHANNEL *,
                       bool *, u_int32_t);
    bool  __ahdecl(*ah_set_tx_power_limit)(struct ath_hal *, u_int32_t,
                u_int16_t, u_int16_t);
    void  __ahdecl(*ah_get_chip_min_and_max_powers)(struct ath_hal *, int8_t *, int8_t *);
    void  __ahdecl(*ah_fill_hal_chans_from_regdb)(struct ath_hal *, uint16_t,
                                                  int8_t, int8_t, int8_t,
                                                  uint32_t, int);
    void __ahdecl(*ah_hal_set_sc)(struct ath_hal *ah, HAL_SOFTC sc);
#if ATH_ANT_DIV_COMB
    HAL_BOOL  __ahdecl(*ah_ant_ctrl_set_lna_div_use_bt_ant)(struct ath_hal *, HAL_BOOL, HAL_CHANNEL *);
#endif /* ATH_ANT_DIV_COMB */

#ifdef ATH_SUPPORT_DFS
    bool  __ahdecl(*ah_radar_wait)(struct ath_hal *, HAL_CHANNEL *);

    void       __ahdecl(*ah_ar_enable_dfs)(struct ath_hal *,
                 HAL_PHYERR_PARAM *pe);
    void       __ahdecl(*ah_ar_get_dfs_thresh)(struct ath_hal *,
                 HAL_PHYERR_PARAM *pe);
    void      __ahdecl(*ah_adjust_difs)(struct ath_hal *, u_int32_t);
    u_int32_t __ahdecl(*ah_dfs_config_fft)(struct ath_hal*, bool);
    void      __ahdecl(*ah_dfs_cac_war)(struct ath_hal *, u_int32_t);
    void      __ahdecl(*ah_cac_tx_quiet)(struct ath_hal *, bool);
#endif

    HAL_CHANNEL* __ahdecl(*ah_get_extension_channel)(struct ath_hal*ah);
    bool __ahdecl(*ah_is_fast_clock_enabled)(struct ath_hal*ah);
    uint16_t __ahdecl(*ah_hal_get_ah_devid)(struct ath_hal *ah);

    /* Transmit functions */
    bool  __ahdecl(*ah_update_tx_trig_level)(struct ath_hal*,
                bool incTrigLevel);
    u_int16_t __ahdecl(*ah_get_tx_trig_level)(struct ath_hal *);
    int   __ahdecl(*ah_setup_tx_queue)(struct ath_hal *, HAL_TX_QUEUE,
                const HAL_TXQ_INFO *q_info);
    bool  __ahdecl(*ah_set_tx_queue_props)(struct ath_hal *, int q,
                const HAL_TXQ_INFO *q_info);
    bool  __ahdecl(*ah_get_tx_queue_props)(struct ath_hal *, int q,
                HAL_TXQ_INFO *q_info);
    bool  __ahdecl(*ah_release_tx_queue)(struct ath_hal *ah, u_int q);
    bool  __ahdecl(*ah_reset_tx_queue)(struct ath_hal *ah, u_int q);
    u_int32_t __ahdecl(*ah_get_tx_dp)(struct ath_hal*, u_int);
    bool  __ahdecl(*ah_set_tx_dp)(struct ath_hal*, u_int, u_int32_t txdp);
    u_int32_t __ahdecl(*ah_num_tx_pending)(struct ath_hal *, u_int q);
    bool  __ahdecl(*ah_start_tx_dma)(struct ath_hal*, u_int);
    bool  __ahdecl(*ah_stop_tx_dma)(struct ath_hal*, u_int, u_int timeout);
    bool  __ahdecl(*ah_stop_tx_dma_indv_que)(struct ath_hal*, u_int, u_int timeout);
    bool  __ahdecl(*ah_abort_tx_dma)(struct ath_hal *);
    bool  __ahdecl(*ah_fill_tx_desc)(struct ath_hal *ah, void *ds, dma_addr_t *buf_addr,
                u_int32_t *seg_len, u_int desc_id, u_int qcu, HAL_KEY_TYPE key_type, bool first_seg,
                bool last_seg, const void *ds0);
    void      __ahdecl(*ah_set_desc_link)(struct ath_hal *, void *ds, u_int32_t link);
    void      __ahdecl(*ah_get_desc_link_ptr)(struct ath_hal *, void *ds, u_int32_t **link);
    void       __ahdecl(*ah_clear_tx_desc_status)(struct ath_hal *, void *);
#ifdef ATH_SWRETRY
    void       __ahdecl(*ah_clear_dest_mask)(struct ath_hal *, void *);
#endif
    HAL_STATUS __ahdecl(*ah_proc_tx_desc)(struct ath_hal *, void *);
    void       __ahdecl(*ah_get_raw_tx_desc)(struct ath_hal *, u_int32_t *);
    void       __ahdecl(*ah_get_tx_rate_code)(struct ath_hal *, void*, struct ath_tx_status *);
    void       __ahdecl(*ah_get_tx_intr_queue)(struct ath_hal *, u_int32_t *);
    void       __ahdecl(*ah_req_tx_intr_desc)(struct ath_hal *, void*);
    u_int32_t  __ahdecl(*ah_calc_tx_airtime)(struct ath_hal *, void*, struct ath_tx_status *, 
           HAL_BOOL comp_wastedt, u_int8_t nbad, u_int8_t nframes );
    void       __ahdecl(*ah_setup_tx_status_ring)(struct ath_hal *, void *, u_int32_t , u_int16_t);


    /* Receive Functions */
    u_int32_t __ahdecl(*ah_get_rx_dp)(struct ath_hal*, HAL_RX_QUEUE);
    void      __ahdecl(*ah_set_rx_dp)(struct ath_hal*, u_int32_t rxdp, HAL_RX_QUEUE qtype);
    void      __ahdecl(*ah_enable_receive)(struct ath_hal*);
    bool  __ahdecl(*ah_stop_dma_receive)(struct ath_hal*, u_int timeout);
    void      __ahdecl(*ah_start_pcu_receive)(struct ath_hal*, bool is_scanning);
    void      __ahdecl(*ah_stop_pcu_receive)(struct ath_hal*);
    void      __ahdecl(*ah_set_multicast_filter)(struct ath_hal*,
                u_int32_t filter0, u_int32_t filter1);
    u_int32_t __ahdecl(*ah_get_rx_filter)(struct ath_hal*);
    void      __ahdecl(*ah_set_rx_filter)(struct ath_hal*, u_int32_t);
    bool  __ahdecl(*ah_set_rx_sel_evm)(struct ath_hal*, bool, bool);
    bool  __ahdecl(*ah_set_rx_abort)(struct ath_hal*, bool);
    bool  __ahdecl(*ah_setup_rx_desc)(struct ath_hal *, struct ath_desc *,
                u_int32_t size, u_int flags);
    HAL_STATUS __ahdecl(*ah_proc_rx_desc)(struct ath_hal *, struct ath_desc *,
                u_int32_t phyAddr, struct ath_desc *next,
                u_int64_t tsf, struct ath_rx_status *rxs);
    HAL_STATUS __ahdecl(*ah_get_rx_key_idx)(struct ath_hal *, struct ath_desc *,
                u_int8_t *keyix, u_int8_t *status);
    HAL_STATUS __ahdecl(*ah_proc_rx_desc_fast)(struct ath_hal *ah,
                struct ath_desc *ds, u_int32_t pa, struct ath_desc *nds,
                struct ath_rx_status *rx_stats, void *buf_addr);
    void      __ahdecl(*ah_rx_monitor)(struct ath_hal *,
                const HAL_NODE_STATS *, HAL_CHANNEL *, HAL_ANISTATS *);
    void      __ahdecl(*ah_proc_mib_event)(struct ath_hal *,
                const HAL_NODE_STATS *);

    /* Misc Functions */
    HAL_STATUS __ahdecl(*ah_get_capability)(struct ath_hal *,
                HAL_CAPABILITY_TYPE, u_int32_t capability,
                u_int32_t *result);
    bool   __ahdecl(*ah_set_capability)(struct ath_hal *,
                HAL_CAPABILITY_TYPE, u_int32_t capability,
                u_int32_t setting, HAL_STATUS *);
    bool   __ahdecl(*ah_get_diag_state)(struct ath_hal *, int request,
                const void *args, u_int32_t argsize,
                void **result, u_int32_t *resultsize);
    void      __ahdecl(*ah_get_mac_address)(struct ath_hal *, u_int8_t *);
    bool  __ahdecl(*ah_set_mac_address)(struct ath_hal *, const u_int8_t*);
    void      __ahdecl(*ah_get_bss_id_mask)(struct ath_hal *, u_int8_t *);
    bool  __ahdecl(*ah_set_bss_id_mask)(struct ath_hal *, const u_int8_t*);
    bool  __ahdecl(*ah_set_regulatory_domain)(struct ath_hal*,
                u_int16_t, HAL_STATUS *);
    void      __ahdecl(*ah_set_led_state)(struct ath_hal*, HAL_LED_STATE);
    void      __ahdecl(*ah_setpowerledstate)(struct ath_hal *, u_int8_t);
    void      __ahdecl(*ah_setnetworkledstate)(struct ath_hal *, u_int8_t);
    void      __ahdecl(*ah_write_associd)(struct ath_hal*,
                const u_int8_t *bssid, u_int16_t assoc_id);
    void      __ahdecl(*ah_force_tsf_sync)(struct ath_hal*,
                const u_int8_t *bssid, u_int16_t assoc_id);
    bool  __ahdecl(*ah_gpio_cfg_input)(struct ath_hal *, u_int32_t gpio);
    bool  __ahdecl(*ah_gpio_cfg_output)(struct ath_hal *,
                u_int32_t gpio, HAL_GPIO_OUTPUT_MUX_TYPE signalType);
    bool  __ahdecl(*ah_gpio_cfg_output_led_off)(struct ath_hal *,
                u_int32_t gpio, HAL_GPIO_OUTPUT_MUX_TYPE signalType);
    u_int32_t __ahdecl(*ah_gpio_get)(struct ath_hal *, u_int32_t gpio);
    bool  __ahdecl(*ah_gpio_set)(struct ath_hal *,
                u_int32_t gpio, u_int32_t val);
    u_int32_t __ahdecl(*ah_gpio_get_intr)(struct ath_hal*);
    void      __ahdecl(*ah_gpio_set_intr)(struct ath_hal*, u_int, u_int32_t);
    u_int32_t __ahdecl(*ah_gpio_get_polarity)(struct ath_hal*);
    void      __ahdecl(*ah_gpio_set_polarity)(struct ath_hal*, u_int32_t, u_int32_t);
    u_int32_t __ahdecl(*ah_gpio_get_mask)(struct ath_hal*);
    int __ahdecl(*ah_gpio_set_mask)(struct ath_hal*, u_int32_t, u_int32_t);
    u_int32_t __ahdecl(*ah_get_tsf32)(struct ath_hal*);
    u_int64_t __ahdecl(*ah_get_tsf64)(struct ath_hal*);
    u_int32_t __ahdecl(*ah_get_tsf2_32)(struct ath_hal*);
    void      __ahdecl(*ah_reset_tsf)(struct ath_hal*);
    bool  __ahdecl(*ah_detect_card_present)(struct ath_hal*);
    void      __ahdecl(*ah_update_mib_mac_stats)(struct ath_hal*);
    void      __ahdecl(*ah_get_mib_mac_stats)(struct ath_hal*, HAL_MIB_STATS*);
    HAL_RFGAIN __ahdecl(*ah_get_rf_gain)(struct ath_hal*);
    u_int     __ahdecl(*ah_get_def_antenna)(struct ath_hal*);
    void      __ahdecl(*ah_set_def_antenna)(struct ath_hal*, u_int);
    bool  __ahdecl(*ah_set_slot_time)(struct ath_hal*, u_int);
    bool  __ahdecl(*ah_set_ack_timeout)(struct ath_hal*, u_int);
    u_int     __ahdecl(*ah_get_ack_timeout)(struct ath_hal*);
    void      __ahdecl(*ah_set_coverage_class)(struct ath_hal*, u_int8_t, int);
    HAL_STATUS __ahdecl(*ah_set_quiet)(struct ath_hal *,
                u_int32_t, u_int32_t, u_int32_t, HAL_QUIET_FLAG);
    bool  __ahdecl(*ah_set_antenna_switch)(struct ath_hal *,
                HAL_ANT_SETTING, HAL_CHANNEL *,
                u_int8_t *, u_int8_t *, u_int8_t *);
    void      __ahdecl(*ah_get_desc_info)(struct ath_hal*, HAL_DESC_INFO *);
    HAL_STATUS  __ahdecl(*ah_select_ant_config)(struct ath_hal *ah,
                u_int32_t cfg);
    u_int32_t  __ahdecl(*ah_ant_ctrl_common_get)(struct ath_hal *ah, HAL_BOOL is_2ghz);
    bool      __ahdecl(*ah_ant_swcom_sel)(struct ath_hal *ah, u_int8_t ops,
                        u_int32_t *common_tbl1, u_int32_t *common_tbl2);
    void      __ahdecl (*ah_enable_tpc)(struct ath_hal *);
    void      __ahdecl (*ah_olpc_temp_compensation)(struct ath_hal *);
    void      __ahdecl (*ah_disable_phy_restart)(struct ath_hal *, int disable_phy_restart);
    void      __ahdecl (*ah_enable_keysearch_always)(struct ath_hal *, int enable);
    bool  __ahdecl(*ah_interference_is_present)(struct ath_hal*);
    void      __ahdecl(*ah_disp_tpc_tables)(struct ath_hal *ah);
    u_int8_t *__ahdecl(*ah_get_tpc_tables)(struct ath_hal *ah, u_int8_t *ratecount);
    /* Key Cache Functions */
    u_int32_t __ahdecl(*ah_get_key_cache_size)(struct ath_hal*);
    bool  __ahdecl(*ah_reset_key_cache_entry)(struct ath_hal*, u_int16_t);
    bool  __ahdecl(*ah_is_key_cache_entry_valid)(struct ath_hal *,
                u_int16_t);
    bool  __ahdecl(*ah_set_key_cache_entry)(struct ath_hal*,
                u_int16_t, const HAL_KEYVAL *,
                const u_int8_t *, int);
    bool  __ahdecl(*ah_set_key_cache_entry_mac)(struct ath_hal*,
                u_int16_t, const u_int8_t *);
    bool  __ahdecl(*ah_print_key_cache)(struct ath_hal*);
#if ATH_SUPPORT_KEYPLUMB_WAR
    bool  __ahdecl(*ah_check_key_cache_entry)(struct ath_hal*, u_int16_t, const HAL_KEYVAL *, int);
#endif

    /* Power Management Functions */
    bool  __ahdecl(*ah_set_power_mode)(struct ath_hal*,
                HAL_POWER_MODE mode, int set_chip);
    void      __ahdecl(*ah_set_sm_ps_mode)(struct ath_hal*, HAL_SMPS_MODE mode);
#if ATH_WOW        
    void      __ahdecl(*ah_wow_apply_pattern)(struct ath_hal *ah,
                u_int8_t *p_ath_pattern, u_int8_t *p_ath_mask,
                int32_t pattern_count, u_int32_t ath_pattern_len);
    bool  __ahdecl(*ah_wow_enable)(struct ath_hal *ah,
                u_int32_t pattern_enable, u_int32_t timoutInSeconds, int clearbssid, bool offloadEnable);
//    u_int32_t __ahdecl(*ah_wow_wake_up)(struct ath_hal *ah, u_int8_t  *chipPatternBytes);
    u_int32_t __ahdecl(*ah_wow_wake_up)(struct ath_hal *ah, bool offloadEnabled);
#if ATH_WOW_OFFLOAD
    void      __ahdecl(*ah_wow_offload_prep)(struct ath_hal *ah);
    void      __ahdecl(*ah_wow_offload_post)(struct ath_hal *ah);
    u_int32_t __ahdecl(*ah_wow_offload_download_rekey_data)(struct ath_hal *ah, u_int32_t *data, u_int32_t size);
    void      __ahdecl(*ah_wow_offload_retrieve_data)(struct ath_hal *ah, void *buf, u_int32_t param);
    void      __ahdecl(*ah_wow_offload_download_acer_magic)(struct ath_hal *ah,
                        bool valid, u_int8_t *datap, u_int32_t bytes);
    void      __ahdecl(*ah_wow_offload_download_acer_swka)(struct ath_hal *ah,
                        u_int32_t id, bool valid, u_int32_t period, u_int32_t size, u_int32_t *datap);
    void      __ahdecl(*ah_wow_offload_download_arp_info)(struct ath_hal *ah, u_int32_t id, u_int32_t *data);
    void      __ahdecl(*ah_wow_offload_download_ns_info)(struct ath_hal *ah, u_int32_t id, u_int32_t *data);
#endif /* ATH_WOW_OFFLOAD */
#endif        

    int16_t   __ahdecl(*ah_get_chan_noise)(struct ath_hal *, HAL_CHANNEL *);
    void      __ahdecl(*ah_get_chain_noise_floor)(struct ath_hal *, int16_t *, HAL_CHANNEL *, int);
    int16_t   __ahdecl(*ah_get_nf_from_reg)(struct ath_hal *, HAL_CHANNEL *, int);
    int       __ahdecl(*ah_get_rx_nf_offset)(struct ath_hal *, HAL_CHANNEL *, int8_t *, int8_t *);

    /* Beacon Management Functions */
    void      __ahdecl(*ah_beacon_init)(struct ath_hal *,
                u_int32_t nexttbtt, u_int32_t intval, u_int32_t intval_frac, HAL_OPMODE opmode);
    void      __ahdecl(*ah_set_station_beacon_timers)(struct ath_hal*,
                const HAL_BEACON_STATE *);

    /* Interrupt functions */
    bool  __ahdecl(*ah_is_interrupt_pending)(struct ath_hal*);
    bool  __ahdecl(*ah_get_pending_interrupts)(struct ath_hal*, HAL_INT*, HAL_INT_TYPE, u_int8_t, bool);
    HAL_INT   __ahdecl(*ah_get_interrupts)(struct ath_hal*);
    HAL_INT   __ahdecl(*ah_set_interrupts)(struct ath_hal*, HAL_INT, bool);
    void      __ahdecl(*ah_set_intr_mitigation_timer)(struct ath_hal* ah,
                HAL_INT_MITIGATION reg, u_int32_t value);
    u_int32_t __ahdecl(*ah_get_intr_mitigation_timer)(struct ath_hal* ah,
                HAL_INT_MITIGATION reg);
#if ATH_SUPPORT_WIFIPOS
    u_int32_t __ahdecl(*ah_read_loc_timer_reg)(struct ath_hal* ah);
    u_int8_t __ahdecl(*ah_get_eeprom_chain_mask)(struct ath_hal* ah);
#endif
    /*Force Virtual Carrier Sense*/
    HAL_BOOL  __ahdecl(*ah_forceVCS)(struct ath_hal*);
    HAL_BOOL  __ahdecl(*ah_setDfs3StreamFix)(struct ath_hal*, u_int32_t);
    HAL_BOOL  __ahdecl(*ah_get3StreamSignature)(struct ath_hal*);
    /* 11n Functions */
    void      __ahdecl(*ah_set_11n_tx_desc)(struct ath_hal *ah,
                void *ds, u_int pkt_len, HAL_PKT_TYPE type,
                u_int tx_power, u_int key_ix, HAL_KEY_TYPE key_type,
                u_int flags);
#if ATH_SUPPORT_WIFIPOS
    void      __ahdecl(*ah_set_rx_chainmask)(struct ath_hal *ah,
                int chainmask);
    u_int32_t      __ahdecl(*ah_update_loc_ctl_reg)(struct ath_hal *ah,
                int pos_bit);
#endif 
/* PAPRD Functions */
    void      __ahdecl(*ah_set_paprd_tx_desc)(struct ath_hal *ah,
                void *ds, int chain_num);
    int      __ahdecl(*ah_paprd_init_table)(struct ath_hal *ah, HAL_CHANNEL *chan);
    HAL_STATUS  __ahdecl(*ah_paprd_setup_gain_table)(struct ath_hal *ah,
                int chain_num);
    HAL_STATUS  __ahdecl(*ah_paprd_create_curve)(struct ath_hal *ah,
                HAL_CHANNEL *chan, int chain_num);
    int         __ahdecl(*ah_paprd_is_done)(struct ath_hal *ah);
    void        __ahdecl(*ah_PAPRDEnable)(struct ath_hal *ah,
                bool enable_flag, HAL_CHANNEL *chan);
    void        __ahdecl(*ah_paprd_populate_table)(struct ath_hal *ah,
                HAL_CHANNEL *chan, int chain_num);
    HAL_STATUS  __ahdecl(*ah_is_tx_done)(struct ath_hal *ah);
    void        __ahdecl(*ah_paprd_dec_tx_pwr)(struct ath_hal *ah);
    int         __ahdecl(*ah_paprd_thermal_send)(struct ath_hal *ah);
/* End PAPRD Functions */    
    void        __ahdecl(*ah_factory_defaults)(struct ath_hal *ah);
#ifdef ATH_SUPPORT_TxBF
    /* tx_bf functions */
    void      __ahdecl(*ah_set_11n_tx_bf_sounding)(struct ath_hal *ah, 
                void *ds,HAL_11N_RATE_SERIES series[],u_int8_t CEC,u_int8_t opt);
    HAL_TXBF_CAPS*  __ahdecl(*ah_get_tx_bf_capability)(struct ath_hal *ah);
    bool  __ahdecl(*ah_read_key_cache_mac)(struct ath_hal *ah, u_int16_t entry,
                u_int8_t *mac);
    void      __ahdecl(*ah_tx_bf_set_key)(struct ath_hal *ah, u_int16_t entry, u_int8_t rx_staggered_sounding,
                u_int8_t channel_estimation_cap, u_int8_t MMSS);
    void      __ahdecl((*ah_setHwCvTimeout) (struct ath_hal *ah,bool opt));
    void      __ahdecl(*ah_fill_tx_bf_cap)(struct ath_hal *ah);
    void      __ahdecl(*ah_tx_bf_get_cv_cache_nr)(struct ath_hal *ah, u_int16_t keyidx,
                u_int8_t *nr);
    void      __ahdecl(*ah_reconfig_h_xfer_timeout)(struct ath_hal *ah, bool is_reset);
    void      __ahdecl(*ah_reset_lowest_txrate)(struct ath_hal *ah);
    void     __ahdecl(*ah_get_perrate_txbfflag)(struct ath_hal *ah, u_int8_t tx_disable_flag[][AH_MAX_CHAINS]);
#endif
#if UNIFIED_SMARTANTENNA    
    void      __ahdecl(*ah_set_11n_rate_scenario)(struct ath_hal *ah,
                void *ds, void *lastds, u_int dur_update_en, u_int rts_cts_rate, u_int rts_cts_duration,
                HAL_11N_RATE_SERIES series[], u_int nseries, u_int flags, u_int32_t smart_ant_status, u_int32_t antenna_array[]);

#else    
    void      __ahdecl(*ah_set_11n_rate_scenario)(struct ath_hal *ah,
                void *ds, void *lastds, u_int dur_update_en, u_int rts_cts_rate, u_int rts_cts_duration,
                HAL_11N_RATE_SERIES series[], u_int nseries, u_int flags, u_int32_t smartAntenna);
#endif    
    void      __ahdecl(*ah_set_11n_aggr_first)(struct ath_hal *ah,
                void *ds, u_int aggr_len);
    void      __ahdecl(*ah_set_11n_aggr_middle)(struct ath_hal *ah,
                void *ds, u_int num_delims);
    void      __ahdecl(*ah_set_11n_aggr_last)(struct ath_hal *ah,
                void *ds);
    void      __ahdecl(*ah_clr_11n_aggr)(struct ath_hal *ah,
                void *ds);
    void      __ahdecl(*ah_set_11n_rifs_burst_middle)(struct ath_hal *ah,
                void *ds);
    void      __ahdecl(*ah_set_11n_rifs_burst_last)(struct ath_hal *ah,
                void *ds);
    void      __ahdecl(*ah_clr_11n_rifs_burst)(struct ath_hal *ah,
                void *ds);
    void      __ahdecl(*ah_set_11n_aggr_rifs_burst)(struct ath_hal *ah,
                void *ds);
    bool  __ahdecl(*ah_set_11n_rx_rifs)(struct ath_hal *ah, bool enable);
    HAL_BOOL  __ahdecl(*ah_setSmartAntenna)(struct ath_hal *ah, HAL_BOOL enable, int mode);    
    bool  __ahdecl(*ah_detect_bb_hang)(struct ath_hal *ah);
    bool  __ahdecl(*ah_detect_mac_hang)(struct ath_hal *ah);
    void      __ahdecl(*ah_immunity)(struct ath_hal *ah, bool enable);
    void      __ahdecl(*ah_get_hang_types)(struct ath_hal *, hal_hw_hangs_t *);
    void      __ahdecl(*ah_set_11n_burst_duration)(struct ath_hal *ah,
                void *ds, u_int burst_duration);
    void      __ahdecl(*ah_set_11n_virtual_more_frag)(struct ath_hal *ah,
                void *ds, u_int vmf);
    int8_t    __ahdecl(*ah_get_11n_ext_busy)(struct ath_hal *ah);
    u_int32_t __ahdecl(*ah_get_ch_busy_pct)(struct ath_hal *ah);
    void      __ahdecl(*ah_set_11n_mac2040)(struct ath_hal *ah, HAL_HT_MACMODE);
    HAL_HT_RXCLEAR __ahdecl(*ah_get_11n_rx_clear)(struct ath_hal *ah);
    void      __ahdecl(*ah_set_11n_rx_clear)(struct ath_hal *ah, HAL_HT_RXCLEAR rxclear);
    u_int32_t __ahdecl(*ah_get_mib_cycle_counts_pct)(struct ath_hal *ah, u_int32_t*, u_int32_t*, u_int32_t*);
    void      __ahdecl(*ah_dma_reg_dump)(struct ath_hal *);
    u_int32_t __ahdecl(*ah_ppm_get_rssi_dump)(struct ath_hal *);
    u_int32_t __ahdecl(*ah_ppm_arm_trigger)(struct ath_hal *);
    int       __ahdecl(*ah_ppm_get_trigger)(struct ath_hal *);
    u_int32_t __ahdecl(*ah_ppm_force)(struct ath_hal *);
    void      __ahdecl(*ah_ppm_un_force)(struct ath_hal *);
    u_int32_t __ahdecl(*ah_ppm_get_force_state)(struct ath_hal *);
    int       __ahdecl(*ah_get_spur_info)(struct ath_hal *, int *, int, u_int16_t *);
    int       __ahdecl(*ah_setSpurInfo)(struct ath_hal *, int, int, u_int16_t *);

    /* Noise floor functions */
    int16_t   __ahdecl(*ah_ar_get_noise_floor_val)(struct ath_hal *ah);

    void      __ahdecl(*ah_set_rx_green_ap_ps_on_off)(struct ath_hal *ah,
                u_int16_t rxMask);
    u_int16_t __ahdecl(*ah_is_single_ant_power_save_possible)(struct ath_hal *ah);

    /* Radio Measurement Specific Functions */
    void      __ahdecl(*ah_get_mib_cycle_counts)(struct ath_hal *ah, HAL_COUNTERS* p_cnts);
    void      __ahdecl(*ah_get_vow_stats)(struct ath_hal *ah, 
                                          HAL_VOWSTATS* p_stats, u_int8_t);
    /* CCX Radio Measurement Specific Functions */
    void      __ahdecl(*ah_clear_mib_counters)(struct ath_hal *);
#ifdef ATH_CCX
    u_int8_t  __ahdecl(*ah_get_cca_threshold)(struct ath_hal *);
    u_int32_t __ahdecl(*ah_get_cur_rssi)(struct ath_hal *);
#endif
#if ATH_GEN_RANDOMNESS
    u_int32_t __ahdecl(*ah_get_rssi_chain0)(struct ath_hal *);
#endif
    
#ifdef ATH_BT_COEX
    /* Bluetooth Coexistence Functions */
    void      __ahdecl(*ah_set_bt_coex_info)(struct ath_hal *, HAL_BT_COEX_INFO *);
    void      __ahdecl(*ah_bt_coex_config)(struct ath_hal *, HAL_BT_COEX_CONFIG *);
    void      __ahdecl(*ah_bt_coex_set_qcu_thresh)(struct ath_hal *, int);
    void      __ahdecl(*ah_bt_coex_set_weights)(struct ath_hal *, u_int32_t);
    void      __ahdecl(*ah_bt_coex_set_bmiss_thresh)(struct ath_hal *, u_int32_t);
    void      __ahdecl(*ah_bt_coex_set_parameter)(struct ath_hal *, u_int32_t, u_int32_t);
    void      __ahdecl(*ah_bt_coex_disable)(struct ath_hal *);
    int       __ahdecl(*ah_bt_coex_enable)(struct ath_hal *);
    u_int32_t __ahdecl(*ah_bt_coex_info)(struct ath_hal *, u_int32_t reg);
    u_int32_t __ahdecl(*ah_coex_wlan_info)(struct ath_hal *, u_int32_t reg,u_int32_t bOn);
#endif
    /* Generic Timer Functions */
    int       __ahdecl(*ah_gentimer_alloc)(struct ath_hal *, HAL_GEN_TIMER_DOMAIN);
    void      __ahdecl(*ah_gentimer_free)(struct ath_hal *, int);
    void      __ahdecl(*ah_gentimer_start)(struct ath_hal *, int, u_int32_t, u_int32_t);
    void      __ahdecl(*ah_gentimer_stop)(struct ath_hal *, int);
    void      __ahdecl(*ah_gentimer_get_intr)(struct ath_hal *, u_int32_t *, u_int32_t *);

    /* DCS - dynamic (transmit) chainmask selection */
    void      __ahdecl(*ah_set_dcs_mode)(struct ath_hal *, u_int32_t);
    u_int32_t __ahdecl(*ah_get_dcs_mode)(struct ath_hal *);

#if ATH_ANT_DIV_COMB
    void     __ahdecl(*ah_get_ant_dvi_comb_conf)(struct ath_hal *, HAL_ANT_COMB_CONFIG *);
    void     __ahdecl(*ah_set_ant_dvi_comb_conf)(struct ath_hal *, HAL_ANT_COMB_CONFIG *);
#endif

    /* Baseband panic functions */
    int     __ahdecl(*ah_get_bb_panic_info)(struct ath_hal *ah, struct hal_bb_panic_info *bb_panic);
    bool    __ahdecl(*ah_handle_radar_bb_panic)(struct ath_hal *ah);
    void    __ahdecl(*ah_set_hal_reset_reason)(struct ath_hal *ah, u_int8_t);

#if ATH_PCIE_ERROR_MONITOR
    int       __ahdecl(*ah_start_pcie_error_monitor)(struct ath_hal *ah,int b_auto_stop);
    int       __ahdecl(*ah_read_pcie_error_monitor)(struct ath_hal *ah, void* p_read_counters);
    int       __ahdecl(*ah_stop_pcie_error_monitor)(struct ath_hal *ah);
#endif //ATH_PCIE_ERROR_MONITOR

#if WLAN_SPECTRAL_ENABLE
    /* Spectral Scan functions */
    void     __ahdecl(*ah_ar_configure_spectral)(struct ath_hal *ah, HAL_SPECTRAL_PARAM *sp);
    void     __ahdecl(*ah_ar_get_spectral_config)(struct ath_hal *ah, HAL_SPECTRAL_PARAM *sp);
    void     __ahdecl(*ah_ar_start_spectral_scan)(struct ath_hal *);
    void     __ahdecl(*ah_ar_stop_spectral_scan)(struct ath_hal *);
    bool __ahdecl(*ah_ar_is_spectral_enabled)(struct ath_hal *);
    bool __ahdecl(*ah_ar_is_spectral_active)(struct ath_hal *);
    int16_t   __ahdecl(*ah_ar_get_ctl_nf)(struct ath_hal *ah);
    int16_t   __ahdecl(*ah_ar_get_ext_nf)(struct ath_hal *ah);
    int16_t   __ahdecl(*ah_ar_get_nominal_nf)(struct ath_hal *ah, HAL_FREQ_BAND band);
#endif  /* WLAN_SPECTRAL_ENABLE */

#if ATH_SUPPORT_RAW_ADC_CAPTURE
    void       __ahdecl(*ah_ar_enable_test_addac_mode)(struct ath_hal *ah);
    void       __ahdecl(*ah_ar_disable_test_addac_mode)(struct ath_hal *ah);
    void       __ahdecl(*ah_ar_begin_adc_capture)(struct ath_hal *ah, int auto_agc_gain);
    HAL_STATUS __ahdecl(*ah_ar_retrieve_capture_data)(struct ath_hal *ah, u_int16_t chain_mask, int disable_dc_filter, void *data_buf, u_int32_t *data_buf_len);
    HAL_STATUS __ahdecl(*ah_ar_calculate_adc_ref_powers)(struct ath_hal *ah, int freq_mhz, int16_t *sample_min, int16_t *sample_max, int32_t *chain_ref_pwr, int num_chain_ref_pwr);
    HAL_STATUS __ahdecl(*ah_ar_get_min_agc_gain)(struct ath_hal *ah, int freq_mhz, int32_t *chain_gain, int num_chain_gain);
#endif

    void       __ahdecl(*ah_promisc_mode )(struct ath_hal *ah, bool);
    void       __ahdecl(*ah_read_pktlog_reg)(struct ath_hal *ah, u_int32_t *, u_int32_t *, u_int32_t *, u_int32_t *);
    void       __ahdecl(*ah_write_pktlog_reg)(struct ath_hal *ah, bool, u_int32_t, u_int32_t, u_int32_t, u_int32_t );
    HAL_STATUS    __ahdecl(*ah_set_proxy_sta)(struct ath_hal *ah, bool);
    int      __ahdecl(*ah_get_cal_intervals)(struct ath_hal *, HAL_CALIBRATION_TIMER **, HAL_CAL_QUERY);
#if ATH_SUPPORT_WIRESHARK
    void     __ahdecl(*ah_fill_radiotap_hdr)(struct ath_hal *,
               struct ah_rx_radiotap_header *, struct ah_ppi_data *, struct ath_desc *, void *);
#endif
#if ATH_TRAFFIC_FAST_RECOVER
    unsigned long __ahdecl(*ah_get_pll3_sqsum_dvc)(struct ath_hal *ah);
#endif

#ifdef ATH_TX99_DIAG
    /* Tx99 functions */
    void       __ahdecl(*ah_tx99txstopdma)(struct ath_hal *, u_int32_t);
    void       __ahdecl(*ah_tx99drainalltxq)(struct ath_hal *);
    void       __ahdecl(*ah_tx99channelpwrupdate)(struct ath_hal *, HAL_CHANNEL *, u_int32_t);
    void       __ahdecl(*ah_tx99start)(struct ath_hal *, u_int8_t *);
    void       __ahdecl(*ah_tx99stop)(struct ath_hal *);
    void       __ahdecl(*ah_tx99_chainmsk_setup)(struct ath_hal *, int);
    void       __ahdecl(*ah_tx99_set_single_carrier)(struct ath_hal *, int, int);
#endif

    void      __ahdecl(*ah_ChkRssiUpdateTxPwr)(struct ath_hal *, int rssi);
    HAL_BOOL    __ahdecl(*ah_is_skip_paprd_by_greentx)(struct ath_hal *);
    void      __ahdecl(*ah_hwgreentx_set_pal_spare)(struct ath_hal *ah, int value);

#if ATH_SUPPORT_MCI
    /* MCI Coexistence Functions */
    void      __ahdecl(*ah_mci_setup)(struct ath_hal *, u_int32_t, void *, u_int16_t, u_int32_t);
    bool      __ahdecl(*ah_mci_send_message)(struct ath_hal *, u_int8_t, u_int32_t, u_int32_t *, u_int8_t, bool, bool);
    u_int32_t __ahdecl(*ah_mci_get_interrupt)(struct ath_hal *, u_int32_t *, u_int32_t *);
    u_int32_t __ahdecl(*ah_mci_state) (struct ath_hal *, u_int32_t, u_int32_t *);
    void      __ahdecl(*ah_mci_detach) (struct ath_hal *); 
#endif

    void      __ahdecl(*ah_reset_hw_beacon_proc_crc)(struct ath_hal *ah);
    int32_t   __ahdecl(*ah_get_hw_beacon_rssi)(struct ath_hal *ah);
    void      __ahdecl(*ah_set_hw_beacon_rssi_threshold)(struct ath_hal *ah,
                                        u_int32_t rssi_threshold);
    void      __ahdecl(*ah_reset_hw_beacon_rssi)(struct ath_hal *ah);
    void      __ahdecl(*ah_mat_enable)(struct ath_hal *, int);
    void      __ahdecl(*ah_dump_keycache)(struct ath_hal *ah, int n, u_int32_t *entry);
    bool      __ahdecl(*ah_is_ani_noise_spur)(struct ath_hal *ah);
#if ATH_SUPPORT_WIFIPOS
    bool      __ahdecl(*ah_lean_channel_change)(struct ath_hal *ah, HAL_OPMODE opmode, HAL_CHANNEL *chan,
                        HAL_HT_MACMODE macmode, u_int8_t txchainmask, u_int8_t rxchainmask);
    bool     __ahdecl(*ah_disable_hwq)(struct ath_hal *ah, u_int16_t mask);
#endif

    void      __ahdecl(*ah_set_hw_beacon_proc)(struct ath_hal *ah, bool on);

    /* Import the 3rd party ctl power tables */
    bool    __ahdecl(*ah_set_ctl_pwr)(struct ath_hal *ah, u_int8_t *ctl_array);
    /* Set the optional tx chainmask which differs with primary txchainmask */
    void    __ahdecl(*ah_set_txchainmaskopt)(struct ath_hal *ah, u_int8_t mask);
	void	__ahdecl(*ah_reset_nav)(struct ath_hal *ah);
    u_int32_t __ahdecl(*ah_get_smart_ant_tx_info)(struct ath_hal *ah, void *ds, u_int32_t rate_array[], u_int32_t antenna_array[]);
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
    HAL_BOOL  __ahdecl(*ah_txbf_loforceon_update)(struct ath_hal *ah, HAL_BOOL lostate);
#endif
    void      __ahdecl(*ah_disable_interrupts_on_exit)(struct ath_hal *ah);
#if ATH_GEN_RANDOMNESS
    int       __ahdecl(*ah_adc_data_read)(struct ath_hal *ah,int *buf, int size, int *scrnglast);
#endif
#if ATH_BT_COEX_3WIRE_MODE
    u_int8_t __ahdecl(*ah_enable_basic_3wire_btcoex)(struct ath_hal *);
    u_int8_t __ahdecl(*ah_disable_basic_3wire_btcoex)(struct ath_hal *);
#endif
#ifdef ATH_SUPPORT_DFS
    u_int32_t   ah_use_cac_prssi;  /* we may use different PRSSI value during CAC */
#endif
#ifdef AH_ANALOG_SHADOW_READ
    u_int32_t   ah_rfshadow[(RF_END-RF_BEGIN)/4+1];
#endif

    u_int32_t   ah_fccaifs;  /* fcc aifs is set  */
#if ATH_BT_COEX_3WIRE_MODE
    u_int32_t   ah_3wire_bt_coex_enable;
    unsigned long   ah_bt_coex_bt_weight;
    unsigned long   ah_bt_coex_wl_weight;
#endif
    struct smart_ant_gpio_conf sa_gpio_conf;
};

/* 
 * Check the PCI vendor ID and device ID against Atheros' values
 * and return a printable description for any Atheros hardware.
 * AH_NULL is returned if the ID's do not describe Atheros hardware.
 */
#if NO_HAL
static inline  const char *__ahdecl ath_hal_probe(u_int16_t vendorid, u_int16_t devid)
{
	  return NULL;
}
#else
extern  const char *__ahdecl ath_hal_probe(u_int16_t vendorid, u_int16_t devid);
#endif

/*
 * Attach the HAL for use with the specified device.  The device is
 * defined by the PCI device ID.  The caller provides an opaque pointer
 * to an upper-layer data structure (HAL_SOFTC) that is stored in the
 * HAL state block for later use.  Hardware register accesses are done
 * using the specified bus tag and handle.  On successful return a
 * reference to a state block is returned that must be supplied in all
 * subsequent HAL calls.  Storage associated with this reference is
 * dynamically allocated and must be freed by calling the ah_detach
 * method when the client is done.  If the attach operation fails a
 * null (AH_NULL) reference will be returned and a status code will
 * be returned if the status parameter is non-zero.
 */
#if NO_HAL
static inline struct ath_hal * __ahdecl ath_hal_attach(u_int16_t devid, HAL_ADAPTER_HANDLE osdev,
	HAL_SOFTC softc, HAL_BUS_TAG tag, HAL_BUS_HANDLE hnd, HAL_BUS_TYPE type,
	struct hal_reg_parm *hal_conf_parm, HAL_STATUS* status)
{
	  return NULL;
}
#else
extern  struct ath_hal * __ahdecl ath_hal_attach(u_int16_t devid, HAL_ADAPTER_HANDLE osdev, 
        HAL_SOFTC, HAL_BUS_TAG, HAL_BUS_HANDLE, HAL_BUS_TYPE,
        struct hal_reg_parm *hal_conf_parm, HAL_STATUS* status);
#endif

 /*
  * Set the Vendor ID for Vendor SKU's which can modify the
  * channel properties returned by ath_hal_init_channels.
  * Return true if set succeeds
  */
 
extern  bool __ahdecl ath_hal_setvendor(struct ath_hal *, u_int32_t );

/*
 * Return a list of channels available for use with the hardware.
 * The list is based on what the hardware is capable of, the specified
 * country code, the modeSelect mask, and whether or not outdoor
 * channels are to be permitted.
 *
 * The channel list is returned in the supplied array.  maxchans
 * defines the maximum size of this array.  nchans contains the actual
 * number of channels returned.  If a problem occurred or there were
 * no channels that met the criteria then false is returned.
 */
#if NO_HAL
static inline bool __ahdecl ath_hal_init_channels(struct ath_hal *strptr,
	HAL_CHANNEL *chans, u_int maxchans, u_int *nchans,
	u_int8_t *regclassids, u_int maxregids, u_int *nregids,
	HAL_CTRY_CODE cc, u_int32_t modeSelect,
	bool enableOutdoor, bool enableExtendedChannels,
	bool block_dfs_enable){return 0;}

static inline u_int __ahdecl ath_hal_getwirelessmodes(struct ath_hal *strptr, HAL_CTRY_CODE cc){return 0;}

static inline u_int16_t getEepromRD(struct ath_hal *ah);
#else
extern  bool __ahdecl ath_hal_init_channels(struct ath_hal *,
        HAL_CHANNEL *chans, u_int maxchans, u_int *nchans,
        u_int8_t *regclassids, u_int maxregids, u_int *nregids,
        HAL_CTRY_CODE cc, u_int32_t modeSelect,
        bool enableOutdoor, bool enableExtendedChannels,
        bool block_dfs_enable);

/*
 * Return bit mask of wireless modes supported by the hardware.
 */
extern  u_int __ahdecl ath_hal_getwirelessmodes(struct ath_hal*, HAL_CTRY_CODE);
extern  u_int16_t getEepromRD(struct ath_hal *ah);
#endif

void __ahdecl ath_hal_init_NF_buffer_for_regdb_chans(struct ath_hal *ah, u_int cc);
uint32_t __ahdecl ath_hal_get_chip_mode(struct ath_hal *ah);

void __ahdecl ath_hal_get_freq_range(struct ath_hal *ah,
                                     uint32_t flags,
                                     uint32_t *low_freq,
                                     uint32_t *high_freq);

/*
 * Return rate table for specified mode (11a, 11b, 11g, etc).
 */
extern  const HAL_RATE_TABLE * __ahdecl ath_hal_getratetable(struct ath_hal *,
        u_int mode);

#if NO_HAL
static inline  u_int16_t __ahdecl ath_hal_computetxtime(struct ath_hal *strptr,
	const HAL_RATE_TABLE *rates, u_int32_t frameLen,
	u_int16_t rateix, bool shortPreamble){
	return 0;
}
#else
/*
 * Calculate the transmit duration of a frame.
 */
extern u_int16_t __ahdecl ath_hal_computetxtime(struct ath_hal *,
        const HAL_RATE_TABLE *rates, u_int32_t frameLen,
        u_int16_t rateix, bool shortPreamble);
#endif

/*
 * Return if device is public safety.
 */
extern bool __ahdecl ath_hal_ispublicsafetysku(struct ath_hal *);

/*
 * Convert between IEEE channel number and channel frequency
 * using the specified channel flags; e.g. CHANNEL_2GHZ.
 */
#if NO_HAL
static inline u_int __ahdecl ath_hal_mhz2ieee(struct ath_hal *strptr, u_int mhz, u_int flags){return 0;}
#else
extern  u_int __ahdecl ath_hal_mhz2ieee(struct ath_hal *, u_int mhz, u_int flags);
#endif

/*
 * Find the HAL country code from its ISO name.
 */
#if NO_HAL
static inline HAL_CTRY_CODE __ahdecl findCountryCode(u_int8_t *countryString){return 0;}
static inline uint8_t  __ahdecl findCTLByCountryCode(HAL_CTRY_CODE countrycode, bool is2G)
{return 0;}
#else
extern HAL_CTRY_CODE __ahdecl findCountryCode(u_int8_t *countryString);
extern u_int8_t  __ahdecl findCTLByCountryCode(HAL_CTRY_CODE countrycode, bool is2G);
#endif

/*
 * Find the HAL country code from its domain enum.
 */
extern HAL_CTRY_CODE __ahdecl findCountryCodeByRegDomain(HAL_REG_DOMAIN regdmn);

/*
 * Return the current regulatory domain information
 */
extern void __ahdecl ath_hal_getCurrentCountry(void *ah, HAL_COUNTRY_ENTRY* ctry);

/*
 * Return the 802.11D supported country table
 */
extern u_int __ahdecl ath_hal_get11DCountryTable(void *ah, HAL_COUNTRY_ENTRY* ctries, u_int nCtryBufCnt);

extern void __ahdecl ath_hal_set_singleWriteKC(struct ath_hal *ah, u_int8_t singleWriteKC);

#if NO_HAL
static inline bool __ahdecl ath_hal_enabledANI(struct ath_hal *ah){return 0;}
#else
extern bool __ahdecl ath_hal_enabledANI(struct ath_hal *ah);
#endif

/* 
 * Device info function 
 */
#if NO_HAL
static inline u_int32_t __ahdecl ath_hal_get_device_info(struct ath_hal *ah,HAL_DEVICE_INFO type)
{
	return 0;
}

/*
** Prototypes for the HAL layer configuration get and set functions
*/

static inline u_int32_t __ahdecl ath_hal_set_config_param(struct ath_hal *ah,
                                   HAL_CONFIG_OPS_PARAMS_TYPE p,
                                   void *b)
{
	return 0;
}

static inline u_int32_t __ahdecl ath_hal_get_config_param(struct ath_hal *ah,
                                   HAL_CONFIG_OPS_PARAMS_TYPE p,
                                   void *b)
{
	return 0;
}
static inline void __ahdecl ath_hal_display_tpctables(struct ath_hal *ah)
{
}
#else
u_int32_t __ahdecl ath_hal_get_device_info(struct ath_hal *ah,HAL_DEVICE_INFO type);

/*
** Prototypes for the HAL layer configuration get and set functions
*/

u_int32_t __ahdecl ath_hal_set_config_param(struct ath_hal *ah,
                                   HAL_CONFIG_OPS_PARAMS_TYPE p,
                                   void *b);

u_int32_t __ahdecl ath_hal_get_config_param(struct ath_hal *ah,
                                   HAL_CONFIG_OPS_PARAMS_TYPE p,
                                   void *b);
/*
 * Display the TPC tables for the current channel and country
 */
void __ahdecl ath_hal_display_tpctables(struct ath_hal *ah);
#endif

/*
 * Return a version string for the HAL release.
 */
extern  char ath_hal_version[];
/*
 * Return a NULL-terminated array of build/configuration options.
 */
extern  const char* ath_hal_buildopts[];
#ifdef ATH_CCX
extern int __ahdecl ath_hal_get_sernum(struct ath_hal *ah, u_int8_t *pSn, int limit);
extern bool __ahdecl  ath_hal_get_chandata(struct ath_hal * ah, HAL_CHANNEL * chan, HAL_CHANNEL_DATA* pData);
#endif

#ifdef DBG
#if NO_HAL
static inline u_int32_t __ahdecl ath_hal_readRegister(struct ath_hal *ah, u_int32_t offset){return 0;}
#else
extern u_int32_t __ahdecl ath_hal_readRegister(struct ath_hal *ah, u_int32_t offset);
#endif
extern void __ahdecl ath_hal_writeRegister(struct ath_hal *ah, u_int32_t offset, u_int32_t value);
#else
#if NO_HAL
static inline u_int32_t __ahdecl ath_hal_readRegister(struct ath_hal *ah, u_int32_t offset){return 0;}
#else
extern u_int32_t __ahdecl ath_hal_readRegister(struct ath_hal *ah, u_int32_t offset);
#endif
#endif

/*
 * Return country code from ah_private structure
 */
extern HAL_CTRY_CODE __ahdecl ath_hal_get_countryCode(struct ath_hal *ah);

/*
 * Return common mode power in 5Ghz
 */
#if NO_HAL
static inline u_int8_t __ahdecl getCommonPower(u_int16_t freq)
{
	  return 0;
}
#else
extern u_int8_t __ahdecl getCommonPower(u_int16_t freq);
#endif
/*
 * Copy the prefix print string for HDPRINTF
 */
extern HAL_STATUS 
ath_hal_set_itf_prefix_name(struct ath_hal *ah, char *prefix, int str_len);

#if ATH_SUPPORT_CFEND
/*
 * Get the current aggregation and reset the aggregation count
 */
extern u_int8_t __ahdecl ath_hal_get_rx_cur_aggr_n_reset(struct ath_hal *ah);
#endif

/* define used in tx_bf rate control */
//before osprey 2.2 , it has a bug that will cause sounding invalid problem at three stream
// (sounding bug when NESS=0), so it should swap to  2 stream rate at three stream.
#define AR_SREV_VER_OSPREY 0x1C0
#define AR_SREV_REV_OSPREY_22            3      

#define ath_hal_support_ts_sounding(_ah) \
    ((ath_hal_get_device_info(_ah, HAL_MAC_VERSION) > AR_SREV_VER_OSPREY) ||   \
     ((ath_hal_get_device_info(_ah, HAL_MAC_VERSION) == AR_SREV_VER_OSPREY) && \
      (ath_hal_get_device_info(_ah, HAL_MAC_REV) >= AR_SREV_REV_OSPREY_22)))

#define AR_SREV_VERSION_JUPITER 0x280
#define AR_SREV_VERSION_DRAGONFLY 0x600 
#define ath_hal_Jupiter(_ah)    \
    (ath_hal_get_device_info(_ah, HAL_MAC_VERSION) == AR_SREV_VERSION_JUPITER)
#define ath_hal_Dragonfly(_ah) \
    (ath_hal_get_device_info(_ah, HAL_MAC_VERSION) == AR_SREV_VERSION_DRAGONFLY)
/* Tx hang check to be done only for osprey and later series */
#define ath_hal_support_tx_hang_check(_ah) \
    (ath_hal_get_device_info(_ah, HAL_MAC_VERSION) >= AR_SREV_VER_OSPREY)
#define ath_hal_support_wifipos(_ah) \
    (ath_hal_get_device_info(_ah, HAL_MAC_VERSION) >= AR_SREV_VER_OSPREY)

#define AR_SREV_VERSION_HONEYBEE 0x500
#define AR_SREV_REVISION_HONEYBEE_10          0x40      /* Honeybee 1.0 */
#define AR_SREV_REVISION_HONEYBEE_11          0x41      /* Honeybee 1.1 */
#define AR_SREV_REVISION_HONEYBEE_20          0x60      /* Honeybee 2.0 */

#define ath_hal_Honeybee11(_ah) \
    ((ath_hal_get_device_info(_ah, HAL_MAC_VERSION) == AR_SREV_VERSION_HONEYBEE) && \
      (ath_hal_get_device_info(_ah, HAL_MAC_REV) == AR_SREV_REVISION_HONEYBEE_11))

#define ath_hal_Honeybee11_later(_ah) \
    ((ath_hal_get_device_info(_ah, HAL_MAC_VERSION) == AR_SREV_VERSION_HONEYBEE) && \
      (ath_hal_get_device_info(_ah, HAL_MAC_REV) >= AR_SREV_REVISION_HONEYBEE_11))

#define ath_hal_Honeybee20(_ah) \
    ((ath_hal_get_device_info(_ah, HAL_MAC_VERSION) == AR_SREV_VERSION_HONEYBEE) && \
      (ath_hal_get_device_info(_ah, HAL_MAC_REV) == AR_SREV_REVISION_HONEYBEE_20))

extern void __ahdecl ath_hal_phyrestart_clr_war_enable(struct ath_hal *ah, int war_enable);
#if NO_HAL
static inline void __ahdecl ath_hal_keysearch_always_war_enable(struct ath_hal *ah, int war_enable){return;}

static inline u_int32_t __ahdecl ath_hal_get_mfp_qos(struct ath_hal *ah){return 0;}
static inline u_int32_t __ahdecl ath_hal_set_mfp_qos(struct ath_hal *ah, u_int32_t dot11w){return 0;}
static inline void __ahdecl ath_hal_set_antenna_gain(struct ath_hal *ah, u_int32_t antgain, bool is2g){ return; }
static inline u_int32_t __ahdecl ath_hal_get_antenna_gain(struct ath_hal *ah, bool is2g) { return 0; }
#else
extern void __ahdecl ath_hal_keysearch_always_war_enable(struct ath_hal *ah, int war_enable);
extern u_int32_t __ahdecl ath_hal_get_mfp_qos(struct ath_hal *ah);
extern u_int32_t __ahdecl ath_hal_set_mfp_qos(struct ath_hal *ah, u_int32_t dot11w);

/* Set user defined antenna gain value */
extern void __ahdecl ath_hal_set_antenna_gain(struct ath_hal *ah, u_int32_t antgain, bool is2g);
#endif
extern u_int32_t __ahdecl ath_hal_get_antenna_gain(struct ath_hal *ah, bool is2g);
#endif /* _ATH_AH_H_ */
