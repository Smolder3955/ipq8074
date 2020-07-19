/*
 * Copyright (c) 2017-2019 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * Copyright (c) 2010, Atheros Communications Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/*
 * Defintions for the Atheros Wireless LAN controller driver.
 */
#ifndef _DEV_OL_ATH_ATHVAR_H
#define _DEV_OL_ATH_ATHVAR_H

#include <osdep.h>
#include <a_types.h>
#include <a_osapi.h>
#include "ol_defines.h"
#include "ieee80211_channel.h"
#include "ieee80211_proto.h"
#include "ieee80211_rateset.h"
#include "ieee80211_regdmn.h"
#include "ieee80211_wds.h"
#include "ieee80211_ique.h"
#include "ieee80211_acs.h"
#include "qdf_types.h"
#include "qdf_lock.h"
#include "qdf_lock.h"
#include "wmi_unified_api.h"
#include "htc_api.h"
#include "ar_ops.h"
#include "cdp_txrx_cmn_struct.h"
#include "cdp_txrx_stats_struct.h"
#include "cdp_txrx_ctrl_def.h"
#include "cdp_txrx_cmn.h"
#include "cdp_txrx_raw.h"
#include "cdp_txrx_me.h"
#include "cdp_txrx_mon.h"
#include "cdp_txrx_pflow.h"
#include "cdp_txrx_host_stats.h"
#include "cdp_txrx_wds.h"
#include <pktlog_ac_api.h>
#include "epping_test.h"
#include "wdi_event_api.h"
#include "ol_helper.h"
#include "ol_if_thermal.h"
#include "ol_if_txrx_handles.h"
#include "qca_ol_if.h"
#if PERF_FIND_WDS_NODE
#include "wds_addr_api.h"
#endif
#if FW_CODE_SIGN
#include <misc/fw_auth.h>
#endif  /* FW_CODE_SIGN */
#if OL_ATH_SUPPORT_LED
#include <linux/gpio.h>
#endif
#include <ieee80211_objmgr_priv.h>
#include <osif_private.h>
#ifndef BUILD_X86
#include <linux/qcom-pcie.h>
#endif
#include "qdf_vfs.h"
#include "qdf_dev.h"

#include <wlan_vdev_mgr_tgt_if_tx_defs.h>

#if ATH_PERF_PWR_OFFLOAD
#include <ieee80211_sm.h>
#endif

/* WRAP SKB marks used by the hooks to optimize */
#define WRAP_ATH_MARK              0x8000

#define WRAP_FLOOD                 0x0001  /*don't change see NF_BR_FLOOD netfilter_bridge.h*/
#define WRAP_DROP                  0x0002  /*mark used to drop short circuited pkt*/
#define WRAP_REFLECT               0x0004  /*mark used to identify reflected multicast*/
#define WRAP_ROUTE                 0x0008  /*mark used allow local deliver to the interface*/

#define WRAP_MARK_FLOOD            (WRAP_ATH_MARK | WRAP_FLOOD)
#define WRAP_MARK_DROP             (WRAP_ATH_MARK | WRAP_DROP)
#define WRAP_MARK_REFLECT          (WRAP_ATH_MARK | WRAP_REFLECT)
#define WRAP_MARK_ROUTE            (WRAP_ATH_MARK | WRAP_ROUTE)

#define WRAP_MARK_IS_FLOOD(_mark)  ((_mark & WRAP_ATH_MARK)?((_mark & WRAP_FLOOD)?1:0):0)
#define WRAP_MARK_IS_DROP(_mark)   ((_mark & WRAP_ATH_MARK)?((_mark & WRAP_DROP)?1:0):0)
#define WRAP_MARK_IS_REFLECT(_mark) ((_mark & WRAP_ATH_MARK)?((_mark & WRAP_REFLECT)?1:0):0)
#define WRAP_MARK_IS_ROUTE(_mark)  ((_mark & WRAP_ATH_MARK)?((_mark & WRAP_ROUTE)?1:0):0)

#define EXT_TID_NONPAUSE    19

#define RTT_LOC_CIVIC_INFO_LEN      16
#define RTT_LOC_CIVIC_REPORT_LEN    64

/* Requestor ID for multiple vdev restart */
#define MULTIPLE_VDEV_RESTART_REQ_ID 0x1234

/* DFS defines */
#define DFS_RESET_TIME_S 7
#define DFS_WAIT (60 + DFS_RESET_TIME_S) /* 60 seconds */
#define DFS_WAIT_MS ((DFS_WAIT) * 1000) /*in MS*/

#define DFS_WEATHER_CHANNEL_WAIT_MIN 10 /*10 minutes*/
#define DFS_WEATHER_CHANNEL_WAIT_S (DFS_WEATHER_CHANNEL_WAIT_MIN * 60)
#define DFS_WEATHER_CHANNEL_WAIT_MS ((DFS_WEATHER_CHANNEL_WAIT_S) * 1000)       /*in MS*/


#define AGGR_BURST_AC_OFFSET 24
#define AGGR_BURST_AC_MASK 0x0f
#define AGGR_BURST_DURATION_MASK 0x00ffffff
#define AGGR_PPDU_DURATION  2000
#define AGGR_BURST_DURATION 8000

/* Lithium ratecode macros to extract mcs, nss, preamble from the ratecode
 * table in host
 */
#define RATECODE_V1_RC_SIZE     16
#define RATECODE_V1_RC_MASK     0xffff
/* This macro is used to extract preamble from the rc in the tables defined in
 * host. In these tables we have kept 4 bits for rate in the rc since 4 bits can
 * accomodate all the valid rates till HE(MCS 11). If the tables get updated in
 * the future to accomodate 5 bit rate in rc then we will have to use
 * PREAMBLE_OFFSET_IN_V1_RC in place of this macro.
 */
#define RATECODE_V1_PREAMBLE_OFFSET (4+3)
#define RATECODE_V1_PREAMBLE_MASK   0x7
/* This macro is used to extract nss from the rc in the tables defined in
 * host. In these tables we have kept 4 bits for rate in the rc since 4 bits can
 * accomodate all the valid rates till HE(MCS 11). If the tables get updated in
 * the future to accomodate 5 bit rate in rc then we will have to use
 * NSS_OFFSET_IN_V1_RC in place of this macro.
 */
#define RATECODE_V1_NSS_OFFSET  0x4
#define RATECODE_V1_NSS_MASK    0x7
/* This macro is used to extract rate from the rc in the tables defined in
 * host. In these tables we have kept 4 bits for rate in the rc since 4 bits can
 * accomodate all the valid rates till HE(MCS 11). If the tables get updated in
 * the future to accomodate 5 bit rate in rc then we will have to redefine
 * the macro to 0x1f to parse 5 bits.
 */
#define RATECODE_V1_RIX_MASK    0xf
/* Lithium ratecode macros to assemble the rate, mcs and preamble in the V1
 * coding format to send to target
 */
#define VERSION_OFFSET_IN_V1_RC  28
#define PREAMBLE_OFFSET_IN_V1_RC 8
#define NSS_OFFSET_IN_V1_RC      5
/* Following macro assembles '_rate' in V1 format
 * where '_rate' is of length 16 bits in the format
 * _rate = (((_pream) << 8) | ((_nss) << 5) | (rate))
 */
#define ASSEMBLE_RATECODE_V1(_rate, _nss, _pream) \
    ((((1) << VERSION_OFFSET_IN_V1_RC) |          \
     ((_pream) << PREAMBLE_OFFSET_IN_V1_RC)) |    \
     ((_nss) << NSS_OFFSET_IN_V1_RC) | (_rate))
#define V1_RATECODE_FROM_RATE(_rate) \
       (((1) << VERSION_OFFSET_IN_V1_RC) | _rate)

/* Legacy ratecode macros to extract rate, mcs and preamble from the ratecode
 * table in host
 */
#define RATECODE_LEGACY_RC_SIZE IEEE80211_RATE_SIZE
#define RATECODE_LEGACY_RC_MASK         0xff
#define RATECODE_LEGACY_NSS_OFFSET      0x4
#define RATECODE_LEGACY_NSS_MASK        0x7
#define RATECODE_LEGACY_RIX_MASK        0xf
#define RATECODE_LEGACY_PREAMBLE_OFFSET 7
#define RATECODE_LEGACY_PREAMBLE_MASK   0x3
/* Legacy ratecode macros to assemble the rate, mcs and preamble in the legacy
 * ratecode format to send to target
 */
#define PREAMBLE_OFFSET_IN_LEGACY_RC    6
#define NSS_MASK_IN_LEGACY_RC           0x3
/* Following macro assembles '_rate' in legacy format
 * where '_rate' is of length 8 bits in the format
 * _rate = (((_pream) << 6) | ((_nss) << 4) | (rate))
 */
#define ASSEMBLE_RATECODE_LEGACY(_rate, _nss, _pream) \
    (((_pream) << PREAMBLE_OFFSET_IN_LEGACY_RC) |     \
    ((_nss) << RATECODE_LEGACY_NSS_OFFSET) | (_rate))

#define DP_TRACE_CONFIG_DEFAULT_LIVE_MODE 0
#define DP_TRACE_CONFIG_DEFAULT_THRESH 4
#define DP_TRACE_CONFIG_DEFAULT_THRESH_TIME_LIMIT 10
#define DP_TRACE_CONFIG_DEFAULT_VERBOSTY QDF_DP_TRACE_VERBOSITY_LOW
#define DP_TRACE_CONFIG_DEFAULT_BITMAP \
        (QDF_NBUF_PKT_TRAC_TYPE_EAPOL |\
        QDF_NBUF_PKT_TRAC_TYPE_DHCP |\
        QDF_NBUF_PKT_TRAC_TYPE_MGMT_ACTION |\
        QDF_NBUF_PKT_TRAC_TYPE_ARP |\
        QDF_NBUF_PKT_TRAC_TYPE_ICMP)

#define RATE_DROPDOWN_LIMIT 7 /* Maximum Value for Rate Drop Down Logic */

#define SNIFFER_DISABLE 0
#define SNIFFER_TX_CAPTURE_MODE 1
#define SNIFFER_M_COPY_MODE 2
#define SNIFFER_TX_MONITOR_MODE 3

#define BEACON_TX_MODE_BURST 1

#define PPDU_DESC_ENHANCED_STATS 1
#define PPDU_DESC_DEBUG_SNIFFER 2

#define MAX_TIM_BITMAP_LENGTH 68 /* Max allowed TIM bitmap for 512 client is 64 + 4  (Guard length) */

typedef void * hif_handle_t;
typedef void * hif_softc_t;

#if ATH_SUPPORT_DSCP_OVERRIDE
extern A_UINT32 dscp_tid_map[WMI_HOST_DSCP_MAP_MAX];
#endif

#ifdef QCA_OL_DMS_WAR
/**
 * struct dms_meta_hdr - Meta data structure used for DMS
 * @dms_id: DMS ID corresponding to the peer, should be non-zero
 * @unicast_addr: Address of the DMS subscribed peer
 */
struct dms_meta_hdr {
    uint8_t dms_id;
    uint8_t unicast_addr[IEEE80211_ADDR_LEN];
};
#endif

struct ath_version {
    u_int32_t    host_ver;
    u_int32_t    target_ver;
    u_int32_t    wlan_ver;
    u_int32_t    wlan_ver_1;
    u_int32_t    abi_ver;
};

typedef enum _ATH_BIN_FILE {
    ATH_OTP_FILE,
    ATH_FIRMWARE_FILE,
    ATH_PATCH_FILE,
    ATH_BOARD_DATA_FILE,
    ATH_FLASH_FILE,
    ATH_TARGET_EEPROM_FILE,
    ATH_UTF_FIRMWARE_FILE,
} ATH_BIN_FILE;

typedef enum _OTP_PARAM {
    PARAM_GET_EEPROM_ALL            = 0,
    PARAM_SKIP_MAC_ADDR             = 1,
    PARAM_SKIP_REG_DOMAIN           = 2,
    PARAM_SKIP_OTPSTREAM_ID_CAL_5G  = 4,
    PARAM_SKIP_OTPSTREAM_ID_CAL_2G  = 8,
    PARAM_GET_CHIPVER_BID           = 0x10,
    PARAM_USE_GOLDENTEMPLATE        = 0x20,
    PARAM_USE_OTP                   = 0x40,
    PARAM_SKIP_EEPROM               = 0x80,
    PARAM_EEPROM_SECTION_MAC        = 0x100,
    PARAM_EEPROM_SECTION_REGDMN     = 0x200,
    PARAM_EEPROM_SECTION_CAL        = 0x400,
    PARAM_OTP_SECTION_MAC           = 0x800,
    PARAM_OTP_SECTION_REGDMN        = 0x1000,
    PARAM_OTP_SECTION_CAL           = 0x2000,
    PARAM_SKIP_OTP                  = 0x4000,
    PARAM_GET_BID_FROM_FLASH        = 0x8000,
    PARAM_FLASH_SECTION_ALL         = 0x10000,
    PARAM_FLASH_ALL                 = 0x20000,
    PARAM_DUAL_BAND_2G              = 0x40000,
    PARAM_DUAL_BAND_5G              = 0x80000
}OTP_PARAM;


typedef enum _ol_target_status  {
     OL_TRGET_STATUS_CONNECTED = 0,    /* target connected */
     OL_TRGET_STATUS_RESET,        /* target got reset */
     OL_TRGET_STATUS_EJECT,        /* target got ejected */
} ol_target_status;

enum ol_ath_tx_ecodes  {
    TX_IN_PKT_INCR=0,
    TX_OUT_HDR_COMPL,
    TX_OUT_PKT_COMPL,
    PKT_ENCAP_FAIL,
    TX_PKT_BAD,
    RX_RCV_MSG_RX_IND,
    RX_RCV_MSG_PEER_MAP,
    RX_RCV_MSG_TYPE_TEST
} ;

enum ol_recovery_option {
    RECOVERY_DISABLE = 0,
    RECOVERY_ENABLE_AUTO,       /* Automatically recover after FW assert */
    RECOVERY_ENABLE_WAIT        /* only do FW RAM dump and wait for user */
} ;

typedef struct {
    int8_t          rssi;       /* RSSI (noise floor ajusted) */
    u_int16_t       channel;    /* Channel */
    int             rateKbps;   /* data rate received (Kbps) */
    uint32_t        phy_mode;   /* phy mode */
    u_int8_t        status;     /* rx descriptor status */
} ol_ath_ieee80211_rx_status_t;

#ifndef ATH_CAP_DCS_CWIM
#define ATH_CAP_DCS_CWIM 0x1
#define ATH_CAP_DCS_WLANIM 0x2
#endif
#define STATS_MAX_RX_CES     12
#define STATS_MAX_RX_CES_PEREGRINE 8

/*
 * structure to hold the packet error count for CE and hif layer
*/
struct ol_ath_stats {
    int hif_pipe_no_resrc_count;
    int ce_ring_delta_fail_count;
    int sw_index[STATS_MAX_RX_CES];
    int write_index[STATS_MAX_RX_CES];
};

struct ol_ath_target_cap {
    target_resource_config     wlan_resource_config; /* default resource config,the os shim can overwrite it */
    /* any other future capabilities of the target go here */

};
/* callback to be called by durin target initialization sequence
 * to pass the target
 * capabilities and target default resource config to os shim.
 * the os shim can change the default resource config (or) the
 * service bit map to enable/disable the services. The change will
 * pushed down to target.
 */
typedef void   (* ol_ath_update_fw_config_cb)\
    (ol_ath_soc_softc_t *soc, struct ol_ath_target_cap *tgt_cap);

/*
 * memory chunck allocated by Host to be managed by FW
 * used only for low latency interfaces like pcie
 */
struct ol_ath_mem_chunk {
    u_int32_t *vaddr;
    u_int32_t paddr;
    qdf_dma_mem_context(memctx);
    u_int32_t len;
    u_int32_t req_id;
};
/** dcs wlan wireless lan interfernce mitigation stats */

/*
 * wlan_dcs_im_tgt_stats_t is defined in wmi_unified.h
 * The below stats are sent from target to host every one second.
 * prev_dcs_im_stats - The previous statistics at last known time
 * im_intr_count, number of times the interfernce is seen continuously
 * sample_count - int_intr_count of sample_count, the interference is seen
 */
typedef struct _wlan_dcs_im_host_stats {
	wmi_host_dcs_im_tgt_stats_t prev_dcs_im_stats;
    A_UINT8   im_intr_cnt;              		/* Interefernce detection counter */
    A_UINT8   im_samp_cnt;              		/* sample counter */
} wlan_dcs_im_host_stats_t;

typedef enum {
    DCS_DEBUG_DISABLE=0,
    DCS_DEBUG_CRITICAL=1,
    DCS_DEBUG_VERBOSE=2,
} wlan_dcs_debug_t;

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
struct ol_ath_ald_record {
    u_int32_t               free_descs;
    u_int32_t               pool_size;
    u_int16_t               ald_free_buf_lvl; /* Buffer Full warning threshold */
    u_int                   ald_buffull_wrn;
};
#endif

#define DCS_PHYERR_PENALTY      500
#define DCS_PHYERR_THRESHOLD    300
#define DCS_RADARERR_THRESHOLD 1000
#define DCS_COCH_INTR_THRESHOLD  30 /* 30 % excessive channel utilization */
#define DCS_TXERR_THRESHOLD      30
#define DCS_USER_MAX_CU          50 /* tx channel utilization due to our tx and rx */
#define DCS_TX_MAX_CU            30
#define DCS_INTR_DETECTION_THR    6
#define DCS_SAMPLE_SIZE          10

typedef struct ieee80211_mib_cycle_cnts periodic_chan_stats_t;

typedef struct _wlan_host_dcs_params {
    u_int8_t                dcs_enable; 			/* if dcs enabled or not, along with running state*/
    wlan_dcs_debug_t        dcs_debug;              /* 0-disable, 1-critical, 2-all */
    u_int32_t			    phy_err_penalty;     	/* phy error penalty*/
    u_int32_t			    phy_err_threshold;
    u_int32_t				radar_err_threshold;
    u_int32_t				coch_intr_thresh ;
    u_int32_t				user_max_cu; 			/* tx_cu + rx_cu */
    u_int32_t 				intr_detection_threshold;
    u_int32_t 				intr_detection_window;
    u_int32_t				tx_err_thresh;
    wlan_dcs_im_host_stats_t scn_dcs_im_stats;
    periodic_chan_stats_t   chan_stats;
} wlan_host_dcs_params_t;

#if OL_ATH_SUPPORT_LED
#define PEREGRINE_LED_GPIO    1
#define BEELINER_LED_GPIO    17
#define CASCADE_LED_GPIO     17
#define BESRA_LED_GPIO       17
#define IPQ4019_LED_GPIO     58

#define LED_POLL_TIMER       500

enum IPQ4019_LED_TYPE {
    IPQ4019_LED_SOURCE = 1,        /* Wifi LED source select */
    IPQ4019_LED_GPIO_PIN = 2,      /* Wifi LED GPIO */
};

typedef struct ipq4019_wifi_leds {
    uint32_t wifi0_led_gpio;      /* gpio of wifi0 led */
    uint32_t wifi1_led_gpio;      /* gpio of wifi1 led */
} ipq4019_wifi_leds_t;

typedef enum _OL_BLINK_STATE {
    OL_BLINK_DONE = 0,
    OL_BLINK_OFF_START = 1,
    OL_BLINK_ON_START = 2,
    OL_BLINK_STOP = 3,
    OL_NUMER_BLINK_STATE,
} OL_BLINK_STATE;

typedef enum {
    OL_LED_OFF = 0,
    OL_LED_ON
} OL_LED_STATUS;

typedef enum _OL_LED_EVENT {
    OL_ATH_LED_TX = 0,
    OL_ATH_LED_RX = 1,
    OL_ATH_LED_POLL = 2,
    OL_NUMER_LED_EVENT,
} OL_LED_EVENT;

typedef struct {
    u_int32_t    timeOn;      // LED ON time in ms
    u_int32_t    timeOff;     // LED OFF time in ms
} OL_LED_BLINK_RATES;

#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
struct alloc_task_pvt_data {
    wmi_buf_t evt_buf;
    void *scn_handle;
};
#endif


#ifndef AR900B_REV_1
#define AR900B_REV_1 0x0
#endif

#ifndef AR900B_REV_2
#define AR900B_REV_2 0x1
#endif

#ifndef QCA_WIFI_QCA8074_VP
#define DEFAULT_WMI_TIMEOUT 10
#else
#define DEFAULT_WMI_TIMEOUT 60
#endif
#define DEFAULT_WMI_TIMEOUT_EMU 60
#define DEFAULT_WMI_TIMEOUT_UNINTR 2
#ifdef QCA_LOWMEM_CONFIG
#define MAX_WMI_CMDS 1024
#else
#define MAX_WMI_CMDS 2048
#endif

#define DEFAULT_ANI_ENABLE_STATUS false

/*
 *  Error types that needs to be muted as per rate limit
 *  Currently added for pn errors and sequnce errors only.
 *  In future this structure can be exapanded for other errors.
 */
#define DEFAULT_PRINT_RATE_LIMIT_VALUE 100

/*
 * Default TX ACK time out value (micro second)
 */
#define DEFAULT_TX_ACK_TIMEOUT 0x40

/*
 * Max TX ACK time out value (micro second)
 */
#define MAX_TX_ACK_TIMEOUT 0xFF

#define MAX_AUTH_REQUEST   32
#define DEFAULT_AUTH_CLEAR_TIMER  1000

struct mute_error_types {
    u_int32_t  pn_errors;
    u_int32_t  seq_num_errors;
};

struct debug_config {
    int print_rate_limit; /* rate limit value */
    struct mute_error_types err_types;
};

#define WMI_MGMT_DESC_POOL_MAX 50
struct wmi_mgmt_desc_t {
	struct ieee80211_cb cb;
	qdf_nbuf_t   nbuf;
	uint32_t     desc_id;
};
union wmi_mgmt_desc_elem_t {
	union wmi_mgmt_desc_elem_t *next;
	struct wmi_mgmt_desc_t wmi_mgmt_desc;
};

/*
 * In parallel mode gpio_pin/func[0-4]- is chain[0-4] configuration
 * In serial mode gpio_pin/func[0]- Configuration for data signal
 *                gpio_pin/func[1]- Configuration for strobe signal
 */
#define SMART_ANT_MAX_SA_CHAINS 4
struct ol_smart_ant_gpio_conf {
    u_int32_t gpio_pin[WMI_HAL_MAX_SANTENNA]; /* GPIO pin configuration for each chain */
    u_int32_t gpio_func[WMI_HAL_MAX_SANTENNA];/* GPIO function configuration for each chain */
};

/* Max number of radios supported in SOC chip */
#define MAX_RADIOS  3

struct soc_spectral_stats {
    uint64_t  phydata_rx_errors;
};

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#define OSIF_NSS_WIFILI_MAX_NUMBER_OF_PAGE 96
#define OSIF_NSS_WIFI_MAX_PEER_ID 4096

struct nss_wifi_peer_mem {
    bool in_use[OSIF_NSS_WIFI_MAX_PEER_ID];    /* array to track peer id allocated. */
    uint32_t paddr[OSIF_NSS_WIFI_MAX_PEER_ID];    /* Array to store the peer_phy mem address allocated. */
    uintptr_t vaddr[OSIF_NSS_WIFI_MAX_PEER_ID];    /* Array to store the peer_virtual mem address allocated. */
    int peer_id_to_alloc_idx_map[OSIF_NSS_WIFI_MAX_PEER_ID]; /* peer_id to alloc index map */
    uint16_t available_idx;
    spinlock_t queue_lock;
};

struct ol_ath_softc_nss_soc {
    int nss_wifiol_id;			/* device id as the device gets probed*/
    int nss_wifiol_ce_enabled;		/* where ce is completed , wifi3.0 may not require*/
    struct {
        struct completion complete;     /* completion structure */
        int response;               /* Response from FW */
    } osif_nss_ol_wifi_cfg_complete;
    uint32_t nss_sidx;		/* assigned NSS id number */
    int nss_sifnum;			/* device interface number */
    void *nss_sctx;		/* device nss context */
    struct nss_wifi_soc_ops *ops;
    uint32_t nss_scfg;
    uint32_t nss_nxthop;        /* nss next hop config */
    uint32_t desc_pmemaddr[OSIF_NSS_WIFILI_MAX_NUMBER_OF_PAGE];
    uintptr_t desc_vmemaddr[OSIF_NSS_WIFILI_MAX_NUMBER_OF_PAGE];
    uint32_t desc_memsize[OSIF_NSS_WIFILI_MAX_NUMBER_OF_PAGE];
    struct nss_wifi_peer_mem nwpmem;
    struct device *dmadev;
};

struct ol_ath_softc_nss_radio {
    uint32_t nss_idx;			/* assigned NSS id number for radio*/
    int nss_rifnum;			/* NSS interface number for the radio */
    void *nss_rctx;		        /* nss context for the radio */
    uint8_t nss_nxthop;                 /* nss next hop config */
};
#endif

typedef struct ol_ath_soc_softc {
    int                     recovery_enable;	 /* enable/disable target recovery feature */
    void		    (*pci_reconnect)(struct ol_ath_soc_softc *);
    qdf_work_t 	    pci_reconnect_work;

#if defined(EPPING_TEST) && !defined(HIF_USB)
    /* for mboxping */
    HTC_ENDPOINT_ID         EppingEndpoint[4];
    qdf_spinlock_t       data_lock;
    struct sk_buff_head     epping_nodrop_queue;
    qdf_timer_t             epping_timer;
    bool                    epping_timer_running;
 #endif
    osdev_t                 sc_osdev;

    uint16_t                soc_attached;
    qdf_semaphore_t         stats_sem;

    /*
     * handle for code that uses adf version of OS
     * abstraction primitives
     */
    qdf_device_t   qdf_dev;

    /**
     * call back set by the os shim
     */
    ol_ath_update_fw_config_cb cfg_cb;

    u_int32_t board_id;
    u_int32_t chip_id;
    u_int16_t device_id;
    void      *platform_devid;
    struct targetdef_s *targetdef;

    struct ath_version      version;
    /* Is this chip a derivative of beeliner - Used for common checks that apply to derivates of ar900b */
    ol_target_status  target_status; /* target status */
    bool             is_sim;   /* is this a simulator */
    u_int8_t         is_target_paused;
    u_int32_t        fwsuspendfailed;
    u_int32_t        sc_dump_opts;       /* fw dump collection options*/

    /* Packet statistics */
    struct ol_ath_stats     pkt_stats;

    bool                dbg_log_init;

    bool                    enableuartprint;    /* enable uart/serial prints from target */

    u_int32_t               vow_config;
    bool                    low_mem_system;

    u_int32_t               max_desc;
    u_int32_t               max_active_peers;	/* max active peers derived from max_descs */
    u_int32_t               peer_del_wait_time; /* duration to wait for peer del completion */
    u_int32_t               max_clients;
    u_int32_t               max_vaps;
    wdi_event_subscribe     scn_rx_peer_invalid_subscriber;
    wdi_event_subscribe     scn_rx_lite_monitor_mpdu_subscriber;

    u_int32_t               sa_validate_sw; /* validate Smart Antenna Software */
    u_int32_t               enable_smart_antenna; /* enable smart antenna */
    bool                    cce_disable; /* disable hw CCE block */

    struct debug_config      dbg;   /* debug support */

   struct swap_seg_info     *target_otp_codeswap_seginfo ;     /* otp codeswap seginfo */
   struct swap_seg_info     *target_otp_dataswap_seginfo ;     /* otp dataswap seginfo */
   struct swap_seg_info     *target_bin_codeswap_seginfo ;     /* target bin codeswap seginfo */
   struct swap_seg_info     *target_bin_dataswap_seginfo ;     /* target bin dataswap seginfo */
   struct swap_seg_info     *target_bin_utf_codeswap_seginfo ; /* target utf bin codeswap seginfo */
   struct swap_seg_info     *target_bin_utf_dataswap_seginfo ; /* target utf bin dataswap seginfo */
   u_int64_t                *target_otp_codeswap_cpuaddr;      /* otp codeswap cpu addr */
   u_int64_t                *target_otp_dataswap_cpuaddr;      /* otp dataswap cpu addr */
   u_int64_t                *target_bin_codeswap_cpuaddr;      /* target bin codeswap cpu addr */
   u_int64_t                *target_bin_dataswap_cpuaddr;      /* target bin dataswap cpu addr */
   u_int64_t                *target_bin_utf_codeswap_cpuaddr;  /* target utf bin codeswap cpu addr */
   u_int64_t                *target_bin_utf_dataswap_cpuaddr;  /* target utf bin dataswap cpu addr */

    bool                down_complete;
    atomic_t            reset_in_progress;

    u_int8_t cal_in_flash; /* calibration data is stored in flash */
    void *cal_mem; /* virtual address for the calibration data on the flash */

    u_int8_t cal_in_file; /* calibration data is stored in file on file system */
#ifdef AH_CAL_IN_FLASH_PCI
    u_int8_t cal_idx; /* index of this radio in the CalAddr array */
#endif
    /* BMI info */
    struct bmi_info       *bmi_handle;

    void            *diag_ol_priv; /* OS-dependent private info for DIAG access */

    qdf_mempool_t mempool_ol_ath_node; /* Memory pool for nodes */
    qdf_mempool_t mempool_ol_ath_vap;  /* Memory pool for vaps */
    qdf_mempool_t mempool_ol_ath_peer; /* Memory pool for peer entry */
    qdf_mempool_t mempool_ol_rx_reorder_buf; /*  Memory pool for reorder buffers */

#if WMI_RECORDING
   struct proc_dir_entry *wmi_proc_entry;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
/* Workqueue to alloc dma memory in process context */
    qdf_workqueue_t *alloc_task_wqueue;
    qdf_work_t alloc_task;
#endif

#ifdef WLAN_FEATURE_FASTPATH
    void		    *htt_handle;
#endif /* WLAN_FEATURE_FASTPATH */


   u_int32_t                soc_idx;
   uint32_t                 tgt_sched_params;  /* target scheduler params */
   struct wlan_objmgr_psoc *psoc_obj;
   struct net_device       *netdev;
   struct ol_if_offload_ops *ol_if_ops;

   void *nbuf;
   void *nbuf1;

   struct ol_ath_radiostats     soc_stats;

    /* UMAC callback functions */
    void                    (*net80211_node_cleanup)(struct ieee80211_node *);
    void                    (*net80211_node_free)(struct ieee80211_node *);

    bool                    nss_nwifi_offload; /* decap of NWIFI to 802.3 is offloaded to NSS */
    u_int32_t               sc_in_delete:1;   /* don't add any more VAPs */

#if OL_ATH_SUPPORT_LED
    const OL_LED_BLINK_RATES  *led_blink_rate_table;   /* blinking rate table to be used */
#endif

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    struct ol_ath_softc_nss_soc nss_soc;
#endif

   bool                    invalid_vht160_info; /* is set when wireless modes and VHT cap's mismatch */

   int                      btcoex_enable;        /* btcoex enable/disable */
   int                      btcoex_wl_priority;   /* btcoex WL priority */
   int                      btcoex_duration;      /* wlan duration for btcoex */
   int                      btcoex_period;        /* sum of wlan and bt duration */
   int                      btcoex_gpio;          /* btcoex gpio pin # for WL priority */
   int                      btcoex_duty_cycle;    /* FW capability for btcoex duty cycle */

   qdf_spinlock_t           soc_lock;

#if WLAN_SPECTRAL_ENABLE
   struct soc_spectral_stats spectral_stats;
#endif

    struct ol_ath_cookie    cookie;

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    struct ol_ath_ald_record  buff_thresh;
#endif
   int wmi_diag_version;
   u_int8_t                 recovery_in_progress;
   uint8_t                    soc_lp_iot_vaps_mask;
#ifndef BUILD_X86
   struct qcom_pcie_register_event pcie_event;
   struct qcom_pcie_notify pcie_notify;
#endif
#ifdef WLAN_SUPPORT_TWT
   bool twt_enable;
   struct wmi_twt_enable_param twt;
#endif
   uint32_t                 tgt_iram_bkp_paddr;    /* tgt iram content available here */
   bool                     wlanstats_enabled;
} ol_ath_soc_softc_t;

#if  ATH_DATA_TX_INFO_EN
struct ol_ath_txrx_ppdu_info_ctx {
    /* Pointer to parent scn handle */
    struct ol_ath_softc_net80211 *scn;

    /* lock to protect ppdu_info buffer queue */
    qdf_spinlock_t lock;

    /* work queue to process ppdu_info stats */
    qdf_work_t work;

    /* ppdu_info buffer queue */
    qdf_nbuf_queue_t nbufq;
};
#endif

struct ol_mgmt_softc_ctx {
    qdf_atomic_t         mgmt_pending_completions;
    qdf_nbuf_queue_t     mgmt_backlog_queue;
    qdf_spinlock_t       mgmt_backlog_queue_lock;
    u_int16_t            mgmt_pending_max;       /* Max size of mgmt descriptor table */
    u_int16_t            mgmt_pending_probe_resp_threshold; /* Threshold size of mgmt descriptor table for probe responses */
};

struct ol_ath_softc_net80211 {
    struct ieee80211com     sc_ic;      /* NB: base class, must be first */
    ol_pktlog_dev_t 		*pl_dev;    /* Must be second- pktlog handle */
    u_int32_t               sc_prealloc_idmask;   /* preallocated vap id bitmap: can only support 32 vaps */
    u_int32_t               macreq_enabled;     /* user mac request feature enable/disable */

#if ATH_DEBUG
    unsigned long rtsctsenable;
#endif

    osdev_t    sc_osdev;

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
    struct ol_ath_softc_nss_radio nss_radio;
#endif

    uint8_t                 ps_report;

    struct ol_regdmn *ol_regdmn_handle;
    u_int8_t                bcn_mode;
    u_int8_t                burst_enable;
    u_int8_t                cca_threshold;
#if ATH_SUPPORT_WRAP
    u_int8_t                mcast_bcast_echo;
    bool                    qwrap_enable;    /* enable/disable qwrap target config  */
#endif
    u_int8_t                dyngroup;
    u_int16_t               burst_dur;
    u_int8_t                arp_override;
    u_int8_t                igmpmld_override;
    u_int8_t                igmpmld_tid;
    struct ieee80211_mib_cycle_cnts  mib_cycle_cnts;  /* used for channel utilization for ol model */
    /*
     * Includes host side stack level stats +
     * radio level athstats
     */
    wmi_host_dbg_stats   ath_stats;

    int                     tx_rx_time_info_flag;

    /* This structure is used to update the radio level stats, the stats
        are directly fetched from the descriptors
    */
    struct ieee80211_chan_stats chan_stats;     /* Used for channel radio-level stats */
    qdf_semaphore_t         scn_stats_sem;
    int16_t                 chan_nf;            /* noise_floor */
    int16_t                 cur_hw_nf;        /* noise_floor used to calculate Signal level*/
    u_int32_t               min_tx_power;
    u_int32_t               max_tx_power;
    u_int32_t               txpowlimit2G;
    u_int32_t               txpowlimit5G;
    u_int32_t               txpower_scale;
    u_int32_t               powerscale; /* reduce the final tx power */
    u_int32_t               chan_tx_pwr;
    u_int32_t               special_ap_vap; /*ap_monitor mode*/
    u_int32_t               smart_ap_monitor; /*smart ap monitor mode*/
    u_int32_t               vdev_count;
    u_int32_t               mon_vdev_count;
    qdf_spinlock_t       scn_lock;

    /** DCS configuration and running state */
    wlan_host_dcs_params_t   scn_dcs;

    u_int32_t               dtcs; /* Dynamic Tx Chainmask Selection enabled/disabled */
#if PERF_FIND_WDS_NODE
    struct wds_table        scn_wds_table;
#endif

    bool                    scn_cwmenable;    /*CWM enable/disable state*/
    bool                    is_ani_enable;    /*ANI enable/diable state*/
#if ATH_RX_LOOPLIMIT_TIMER
    qdf_timer_t          rx_looplimit_timer;
    u_int32_t               rx_looplimit_timeout;        /* timeout intval */
    bool                    rx_looplimit_valid;
    bool                    rx_looplimit;
#endif
    u_int32_t               peer_ext_stats_count;
    qdf_atomic_t            peer_count;
#if ATH_SUPPORT_WRAP
    int                     sc_nwrapvaps; /* # of WRAP vaps */
    int                     sc_npstavaps; /* # of ProxySTA vaps */
    int                     sc_nscanpsta; /* # of scan-able non-Main ProxySTA vaps */
    qdf_spinlock_t       sc_mpsta_vap_lock; /* mpsta vap lock */
    struct ieee80211vap     *sc_mcast_recv_vap; /* the ProxySTA vap to receive multicast frames */
#endif
    int                     sc_chan_freq;           /* channel change freq in mhz */
    int                     sc_chan_band_center_f1; /* channel change band center freq in mhz */
    int                     sc_chan_band_center_f2; /* channel change band center freq in mhz */
    int                     sc_chan_phy_mode;    /* channel change PHY mode */

#if OL_ATH_SUPPORT_LED
    os_timer_t                      scn_led_blink_timer;     /* led blinking timer */
    os_timer_t                      scn_led_poll_timer;     /* led polling timer */
    OL_BLINK_STATE                scn_blinking; /* LED blink operation active */
    u_int32_t               scn_led_time_on;  /* LED ON time for current blink in ms */
    u_int32_t               scn_led_byte_cnt;
    u_int32_t               scn_led_total_byte_cnt;
    u_int32_t               scn_led_last_time;
    u_int32_t               scn_led_max_blink_rate_idx;
    u_int8_t                scn_led_gpio;
#endif
    u_int32_t               enable_smart_antenna; /* enable smart antenna */
    u_int32_t               ol_rts_cts_rate;

    int                     sc_nstavaps;
    bool                    sc_is_blockdfs_set;
    u_int32_t               scn_last_peer_invalid_time;  /*Time interval since last invalid was sent */
    u_int32_t               scn_peer_invalid_cnt;      /* number of permissible dauth in interval */
    u_int32_t               scn_user_peer_invalid_cnt; /* configurable by user  */
    u_int32_t               aggr_burst_dur[WME_AC_VO+1]; /* maximum VO */
    bool                    scn_qboost_enable;    /*Qboost enable/disable state*/
    u_int32_t               scn_sifs_frmtype;    /*SIFS RESP enable/disable state*/
    u_int32_t               scn_sifs_uapsd;    /*SIFS RESP UAPSD enable/disable state*/
    bool                    scn_block_interbss;  /* block interbss traffic */
    u_int8_t                fw_disable_reset;
    u_int16_t               txbf_sound_period;
    bool                    scn_promisc;            /* Set or clear promisc mode */
    atomic_t                sc_dev_enabled;    /* dev is enabled */
    struct thermal_param    thermal_param;
#if UNIFIED_SMARTANTENNA
    wdi_event_subscribe sa_event_sub;
#define MAX_TX_PPDU_SIZE 32
    uint32_t tx_ppdu_end[MAX_TX_PPDU_SIZE]; /* ppdu status for tx completion */
#endif
#if QCA_AIRTIME_FAIRNESS
    uint8_t     atf_strict_sched;
#endif
    u_int32_t               sc_dump_opts;       /* fw dump collection options*/

    u_int8_t                dpdenable;
    int8_t                  sc_noise_floor_th; /* signal noise floor in dBm used in ch hoping */
    u_int16_t               sc_noise_floor_report_iter; /* #of iteration noise is higher then threshold */
    u_int16_t               sc_noise_floor_total_iter;/* total # of iteration */
    u_int8_t                sc_enable_noise_detection; /* Enable/Disable noise detection due to channel hopping in acs */
    u_int8_t                scn_mgmt_retry_limit; /*Management retry limit*/
    u_int16_t               scn_amsdu_mask;
    u_int16_t               scn_ampdu_mask;

#if ATH_PROXY_NOACK_WAR
   bool sc_proxy_noack_war;
#endif
   u_int32_t                sc_arp_dbg_srcaddr;  /* ip address to monitor ARP */
   u_int32_t                sc_arp_dbg_dstaddr;  /* ip address to monitor ARP */
   u_int32_t                sc_arp_dbg_conf;   /* arp debug conf */
   u_int32_t                sc_tx_arp_req_count; /* tx arp request counters */
   u_int32_t                sc_rx_arp_req_count; /* rx arp request counters  */
   u_int32_t                sc_tx_arp_resp_count; /* tx arp response counters  */
   u_int32_t                sc_rx_arp_resp_count; /* rx arp response counters  */

   bool      periodic_chan_stats;
#if ATH_DATA_TX_INFO_EN
    u_int32_t               enable_perpkt_txstats;
#endif
   int                      enable_statsv2;
   struct ol_smart_ant_gpio_conf sa_gpio_conf; /* GPIO configuration */
   int16_t                  chan_nf_sec80;            /* noise_floor secondary 80 */
   uint32_t                 user_config_val;

   uint32_t wifi_num;

   u_int32_t radio_id;
   ol_ath_soc_softc_t      *soc;
   struct net_device       *netdev;
   struct wlan_objmgr_pdev *sc_pdev;

#if WLAN_SPECTRAL_ENABLE
   QDF_STATUS (*wmi_spectral_configure_cmd_send)(void *wmi_hdl,
                struct vdev_spectral_configure_params *param);
   QDF_STATUS (*wmi_spectral_enable_cmd_send)(void *wmi_hdl,
                struct vdev_spectral_enable_params *param);
#endif

    /* UTF event information */
    struct {
        u_int8_t            *data;
        u_int32_t           length;
        u_int16_t           offset;
        u_int8_t            currentSeq;
        u_int8_t            expectedSeq;
    } utf_event_info;

    u_int32_t               max_clients;
    u_int32_t               max_vaps;

    u_int32_t               set_ht_vht_ies:1, /* true if vht ies are set on target */
                            set_he_ies:1;     /* true if HE ies are set on target */
#if ATH_DATA_TX_INFO_EN
    struct ieee80211_tx_status   *tx_status_buf;  /*per-msdu tx status info*/
#endif
    u_int8_t                vow_extstats;
    u_int8_t                retry_stats;
    qdf_timer_t              scn_stats_timer;     /* stats  timer */
    u_int8_t                is_scn_stats_timer_init;
    u_int32_t               pdev_stats_timer;
    uint8_t                 tx_ack_timeout; /* TX ack timeout value in microsec */
    wdi_event_subscribe stats_tx_data_subscriber;
    wdi_event_subscribe stats_rx_data_subscriber;
    wdi_event_subscribe stats_nondata_subscriber;
    wdi_event_subscribe stats_rx_nondata_subscriber;
#if QCN_IE
    wdi_event_subscribe stats_bpr_subscriber;
#endif
    uint32_t                soft_chain;
    wdi_event_subscribe stats_rx_subscriber;
    wdi_event_subscribe stats_tx_subscriber;
    atomic_t tx_metadata_ref;
    atomic_t rx_metadata_ref;
#if  ATH_DATA_TX_INFO_EN
    /* TxRx ppdu stats processing context */
    struct ol_ath_txrx_ppdu_info_ctx *tx_ppdu_stats_ctx;
    struct ol_ath_txrx_ppdu_info_ctx *rx_ppdu_stats_ctx;
#endif
    wdi_event_subscribe     htt_stats_subscriber;
    struct ol_mgmt_softc_ctx mgmt_ctx;
    wdi_event_subscribe     peer_stats_subscriber;
    wdi_event_subscribe     dp_stats_subscriber;
    wdi_event_subscribe     csa_phy_update_subscriber;
    wdi_event_subscribe     sojourn_stats_subscriber;
    wdi_event_subscribe     dp_rate_tx_stats_subscriber;
    wdi_event_subscribe     dp_rate_rx_stats_subscriber;
    wdi_event_subscribe     peer_create_subscriber;
    wdi_event_subscribe     peer_destroy_subscriber;
    wdi_event_subscribe     peer_flush_rate_stats_sub;
    wdi_event_subscribe     flush_rate_stats_req_sub;
    qdf_timer_t             auth_timer; /* auth timer */
    qdf_atomic_t            auth_cnt;   /* number of auth received DEFAULT_AUTH_CLEAR_TIMER */
    uint8_t                 max_auth;   /* maximum auth to receive in DEFAULT_AUTH_CLEAR_TIMER */
    uint8_t                 sc_bsta_fixed_idmask; /* Mask value to set fixed mac address for backhaul STA */
};

#define OL_ATH_PPDU_STATS_BACKLOG_MAX 16

#define PDEV_ID(scn) scn->pdev_id
#define PDEV_UNIT(pdev_id) (pdev_id)

#define OL_ATH_DCS_ENABLE(__arg1, val) ((__arg1) |= (val))
#define OL_ATH_DCS_DISABLE(__arg1, val) ((__arg1) &= ~(val))
#define OL_ATH_DCS_SET_RUNSTATE(__arg1) ((__arg1) |= 0x10)
#define OL_ATH_DCS_CLR_RUNSTATE(__arg1) ((__arg1) &= ~0x10)
#define OL_IS_DCS_ENABLED(__arg1) ((__arg1) & 0x0f)
#define OL_IS_DCS_RUNNING(__arg1) ((__arg1) & 0x10)
#define OL_ATH_CAP_DCS_CWIM     0x1
#define OL_ATH_CAP_DCS_WLANIM   0x2
#define OL_ATH_CAP_DCS_MASK  (OL_ATH_CAP_DCS_CWIM | OL_ATH_CAP_DCS_WLANIM)

#define ol_scn_host_80211_enable_get(_ol_pdev_hdl) \
    (wlan_psoc_nif_feat_cap_get(\
                  ((struct ol_ath_softc_net80211 *)(_ol_pdev_hdl))->soc->psoc_obj,\
                                          WLAN_SOC_F_HOST_80211_ENABLE))

#define ol_scn_nss_nwifi_offload_get(_ol_pdev_hdl) \
    ((struct ol_ath_softc_net80211 *)(_ol_pdev_hdl))->soc->nss_nwifi_offload

#define ol_scn_target_revision(_ol_pdev_hdl) \
    (((struct ol_ath_softc_net80211 *)(_ol_pdev_hdl))->soc->target_revision)

#define OL_ATH_SOFTC_NET80211(_ic)     ((struct ol_ath_softc_net80211 *)(_ic))

struct bcn_buf_entry {
    A_BOOL                        is_dma_mapped;
    wbuf_t                        bcn_buf;
    TAILQ_ENTRY(bcn_buf_entry)    deferred_bcn_list_elem;
};

struct ol_ath_vap_net80211 {
    struct ieee80211vap             av_vap;     /* NB: base class, must be first */
    struct ol_ath_softc_net80211    *av_sc;     /* back pointer to softc */
    int                             av_if_id;   /* interface id */
    u_int64_t                       av_tsfadjust;       /* Adjusted TSF, host endian */
    bool                            av_bcn_offload;  	/* Handle beacons in FW */
    wbuf_t                          av_wbuf;            /* Beacon buffer */
    A_BOOL                          is_dma_mapped;
    struct ieee80211_bcn_prb_info   av_bcn_prb_templ;   /* Beacon probe template */
    struct ieee80211_beacon_offsets av_beacon_offsets;  /* beacon fields offsets */
    bool                            av_ol_resmgr_wait;  /* UMAC waits for target */
                                                        /*   event to bringup vap*/
    qdf_spinlock_t               avn_lock;
    TAILQ_HEAD(, bcn_buf_entry)     deferred_bcn_list;  /* List of deferred bcn buffers */
    struct ieee80211_ath_channel        *av_ol_resmgr_chan;  /* Channel ptr configured in the target*/
#if ATH_SUPPORT_WRAP
    u_int32_t                   av_is_psta:1,   /* is ProxySTA VAP */
                                av_is_mpsta:1,  /* is Main ProxySTA VAP */
                                av_is_wrap:1,   /* is WRAP VAP */
                                av_use_mat:1;   /* use MAT for this VAP */
    u_int8_t                    av_mat_addr[IEEE80211_ADDR_LEN];    /* MAT addr */
#endif
    bool                        av_restart_in_progress; /* vdev restart in progress */
    wmi_host_vdev_stats  vdev_stats;
    wmi_host_vdev_extd_stats  vdev_extd_stats;
    uint32_t                  vdev_param_capabilities; /*vdev param capabilities state*/
};
#define OL_ATH_VAP_NET80211(_vap)      ((struct ol_ath_vap_net80211 *)(_vap))

struct ol_ath_node_net80211 {
    struct ieee80211_node       an_node;     /* NB: base class, must be first */
    u_int32_t                   an_ni_rx_rate;
    u_int32_t                   an_ni_tx_rate;
    u_int32_t                   an_ni_avg_rx_rate;
    u_int32_t                   an_ni_avg_tx_rate;
    u_int8_t                    an_ni_tx_ratecode;
    u_int8_t                    an_ni_tx_flags;
    u_int8_t                    an_ni_tx_power;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
	u_int32_t					an_tx_rates_used;
	u_int32_t					an_tx_cnt;
	u_int32_t					an_phy_err_cnt;
	u_int32_t					an_tx_bytes;
	u_int32_t					an_tx_ratecount;
#endif
};

#define OL_ATH_NODE_NET80211(_ni)      ((struct ol_ath_node_net80211 *)(_ni))

void ol_target_failure(void *instance, QDF_STATUS status);

int ol_ath_soc_attach(ol_ath_soc_softc_t *soc, IEEE80211_REG_PARAMETERS *ieee80211_conf_parm, ol_ath_update_fw_config_cb cb);

void qboost_config(struct ieee80211vap *vap, struct ieee80211_node *ni, bool qboost_cfg);

int ol_ath_pdev_attach(struct ol_ath_softc_net80211 *scn, IEEE80211_REG_PARAMETERS *ieee80211_conf_parm, uint8_t phy_id);

int ol_asf_adf_attach(ol_ath_soc_softc_t *soc);

int ol_asf_adf_detach(ol_ath_soc_softc_t *soc);

void ieee80211_mucap_vattach(struct ieee80211vap *vap);
void ieee80211_mucap_vdetach(struct ieee80211vap *vap);


void ol_ath_target_status_update(ol_ath_soc_softc_t *soc, ol_target_status status);

int ol_ath_soc_detach(ol_ath_soc_softc_t *soc, int force);
int ol_ath_pdev_detach(struct ol_ath_softc_net80211 *scn, int force);

#ifdef QVIT
void ol_ath_qvit_detach(struct ol_ath_softc_net80211 *scn);
void ol_ath_qvit_attach(struct ol_ath_softc_net80211 *scn);
void ol_ath_pdev_qvit_detach(struct ol_ath_softc_net80211 *scn);
void ol_ath_pdev_qvit_attach(struct ol_ath_softc_net80211 *scn);
#endif

#if ATH_SUPPORT_WIFIPOS
extern unsigned int wifiposenable;

int ol_ieee80211_wifipos_xmitprobe (struct ieee80211vap *vap,
                                        ieee80211_wifipos_reqdata_t *reqdata);
int ol_ieee80211_wifipos_xmitrtt3 (struct ieee80211vap *vap, u_int8_t *mac_addr,
                                        int extra);
void ol_ath_rtt_keepalive_req(struct ieee80211vap *vap,
                         struct ieee80211_node *ni, bool stop);

int ol_ieee80211_lci_set (struct ieee80211vap *vap, void *reqdata);
int ol_ieee80211_lcr_set (struct ieee80211vap *vap, void *reqdata);

#endif
void ol_ath_suspend_resume_attach(struct ol_ath_softc_net80211 *scn);

int ol_ath_resume(struct ol_ath_softc_net80211 *scn);
int ol_ath_suspend(struct ol_ath_softc_net80211 *scn);

void ol_ath_vap_attach(struct ieee80211com *ic);
void ol_ath_vap_soc_attach(ol_ath_soc_softc_t *soc);

int ol_ath_cwm_attach(struct ol_ath_softc_net80211 *scn);

struct ieee80211vap *ol_ath_vap_get(struct ol_ath_softc_net80211 *scn, u_int8_t vdev_id);
struct ieee80211vap *ol_ath_getvap(osif_dev *osdev);
u_int8_t ol_ath_vap_get_myaddr(struct ol_ath_softc_net80211 *scn, u_int8_t vdev_id,
                                 u_int8_t *macaddr);
struct ieee80211vap *ol_ath_pdev_vap_get(struct wlan_objmgr_pdev *pdev, u_int8_t vdev_id);
void ol_ath_release_vap(struct ieee80211vap *vap);

void ol_ath_beacon_attach(struct ieee80211com *ic);
void ol_ath_beacon_soc_attach(ol_ath_soc_softc_t *soc);

int ol_ath_node_attach(struct ol_ath_softc_net80211 *scn, struct ieee80211com *ic);
int ol_ath_node_soc_attach(ol_ath_soc_softc_t *soc);

void ol_ath_resmgr_attach(struct ieee80211com *ic);
void ol_ath_resmgr_soc_attach(ol_ath_soc_softc_t *soc);

#if ATH_SUPPORT_WIFIPOS
void ol_ath_rtt_meas_report_attach(ol_ath_soc_softc_t *soc);
void ol_ath_rtt_netlink_attach(struct ieee80211com *ic);
void ol_if_rtt_detach(struct ieee80211com *ic);
#endif
#if ATH_SUPPORT_LOWI
void ol_ath_lowi_wmi_event_attach(ol_ath_soc_softc_t *soc);
#endif

#if QCA_AIRTIME_FAIRNESS
int ol_ath_set_atf(struct ieee80211com *ic);
int ol_ath_send_atf_peer_request(struct ieee80211com *ic);
int ol_ath_set_atf_grouping(struct ieee80211com *ic);
int ol_ath_set_bwf(struct ieee80211com *ic);
#endif

void ol_chan_stats_event (struct ieee80211com *ic,
         periodic_chan_stats_t *pstats, periodic_chan_stats_t *nstats);

int ol_ath_invalidate_channel_stats(struct ieee80211com *ic);

int ol_ath_periodic_chan_stats_config(struct ol_ath_softc_net80211 *scn,
        bool enable, u_int32_t stats_period);

void ol_ath_power_attach(struct ieee80211com *ic);

struct ieee80211_ath_channel *
ol_ath_find_full_channel(struct ieee80211com *ic, u_int32_t freq);

int ol_ath_vap_send_data(struct ieee80211vap *vap, wbuf_t wbuf);

void ol_ath_vap_send_hdr_complete(void *ctx, HTC_PACKET_QUEUE *htc_pkt_list);


void ol_rx_indicate(void *ctx, wbuf_t wbuf);

void ol_rx_handler(void *ctx, HTC_PACKET *htc_packet);

enum ol_rx_err_type {
    OL_RX_ERR_DEFRAG_MIC,
    OL_RX_ERR_PN,
    OL_RX_ERR_UNKNOWN_PEER,
    OL_RX_ERR_MALFORMED,
    OL_RX_ERR_TKIP_MIC,
};

/**
 * @brief Provide notification of failure during host rx processing
 * @details
 *  Indicate an error during host rx data processing, including what
 *  kind of error happened, when it happened, which peer and TID the
 *  erroneous rx frame is from, and what the erroneous rx frame itself
 *  is.
 *
 * @param vdev_id - ID of the virtual device received the erroneous rx frame
 * @param peer_mac_addr - MAC address of the peer that sent the erroneous
 *      rx frame
 * @param tid - which TID within the peer sent the erroneous rx frame
 * @param tsf32  - the timstamp in TSF units of the erroneous rx frame, or
 *      one of the fragments that when reassembled, constitute the rx frame
 * @param err_type - what kind of error occurred
 * @param rx_frame - the rx frame that had an error
 */
void
ol_rx_err(
    ol_pdev_handle pdev,
    u_int8_t vdev_id,
    u_int8_t *peer_mac_addr,
    int tid,
    u_int32_t tsf32,
    enum ol_rx_err_type err_type,
    qdf_nbuf_t rx_frame);


enum ol_rx_notify_type {
    OL_RX_NOTIFY_IPV4_IGMP,
};

/*
 * The enum values are alligned with the
 * values used by 3.0. This ensures compatibility
 * across both the versions.
 */
enum {
    OL_TIDMAP_PRTY_SVLAN = 3,
    OL_TIDMAP_PRTY_CVLAN = 4,
};

/**
 * @brief Provide notification of reception of data of special interest.
 * @details
 *  Indicate when "special" data has been received.  The nature of the
 *  data that results in it being considered special is specified in the
 *  notify_type argument.
 *  This function is currently used by the data-path SW to notify the
 *  control path SW when the following types of rx data are received:
 *    + IPv4 IGMP frames
 *      The control SW can use these to learn about multicast group
 *      membership, if it so chooses.
 *
 * @param pdev - handle to the ctrl SW's physical device object
 * @param vdev_id - ID of the virtual device received the special data
 * @param peer_mac_addr - MAC address of the peer that sent the special data
 * @param tid - which TID within the peer sent the special data
 * @param tsf32  - the timstamp in TSF units of the special data
 * @param notify_type - what kind of special data was received
 * @param rx_frame - the rx frame containing the special data
 */
int
ol_rx_notify(
    ol_pdev_handle pdev,
    u_int8_t vdev_id,
    u_int8_t *peer_mac_addr,
    int tid,
    u_int32_t tsf32,
    enum ol_rx_notify_type notify_type,
    qdf_nbuf_t rx_frame);

struct firmware_priv {
	size_t size;
	const u8 *data;
	void **pages;
	/* firmware loader private fields */
	void *priv;
};


void
ol_ath_mgmt_soc_attach(ol_ath_soc_softc_t *soc);
void ol_ath_mgmt_attach(struct ieee80211com *ic);
void ol_ath_mgmt_detach(struct ieee80211com *ic);
void ol_ath_mgmt_tx_complete(void *ctxt, wbuf_t wbuf, int err);
void ol_ath_mgmt_register_offload_beacon_tx_status_event(
        struct ieee80211com *ic, bool unregister);
void ol_ath_mgmt_register_bsscolor_collision_det_config_event(
        struct ieee80211com *ic);


void ol_ath_beacon_alloc(struct ieee80211com *ic, int if_id);

void ol_ath_beacon_stop(struct ol_ath_softc_net80211 *scn,
                   struct ol_ath_vap_net80211 *avn);

void ol_ath_beacon_free(struct ieee80211com *ic, int if_id);

int ol_ath_bcn_tmpl_send(struct ol_ath_softc_net80211 *scn,
				int vdev_id, struct ieee80211vap *vap);

void ol_ath_net80211_newassoc(struct ieee80211_node *ni, int isnew);

void ol_ath_phyerr_attach(ol_ath_soc_softc_t *soc);
void ol_ath_phyerr_detach(ol_ath_soc_softc_t *soc);
void ol_ath_phyerr_enable(struct ieee80211com *ic);
void ol_ath_phyerr_disable(struct ieee80211com *ic);

void ol_ath_vap_tx_lock(osif_dev *osifp);
void ol_ath_vap_tx_unlock(osif_dev *osifp);

int
ol_transfer_target_eeprom_caldata(ol_ath_soc_softc_t *soc, u_int32_t address, bool compressed);

int
ol_transfer_bin_file(ol_ath_soc_softc_t *soc, ATH_BIN_FILE file,
                    u_int32_t address, bool compressed);

int
ol_ath_request_firmware(struct firmware_priv **fw_entry, const char *file,
		                        struct device *dev, int dev_id);
void
ol_ath_release_firmware(struct firmware_priv *fw_entry);

int
__ol_ath_check_wmi_ready(ol_ath_soc_softc_t *soc);


void __ol_target_paused_event(ol_ath_soc_softc_t *soc);

u_int32_t host_interest_item_address(u_int32_t target_type, u_int32_t item_offset);

int
ol_ath_set_config_param(struct ol_ath_softc_net80211 *scn,
        enum _ol_ath_param_t param, void *buff, bool *restart_vaps);

int
ol_ath_get_config_param(struct ol_ath_softc_net80211 *scn, enum _ol_ath_param_t param, void *buff);

int
ol_hal_set_config_param(struct ol_ath_softc_net80211 *scn, enum _ol_hal_param_t param, void *buff);

int
ol_hal_get_config_param(struct ol_ath_softc_net80211 *scn, enum _ol_hal_param_t param, void *buff);

int
ol_net80211_set_mu_whtlist(wlan_if_t vap, u_int8_t *macaddr, u_int16_t tidmask);

void ol_ath_config_bsscolor_offload(wlan_if_t vap, bool disable);

uint32_t ol_get_phymode_info(struct ol_ath_softc_net80211 *scn,
        uint32_t chan_mode, bool is_2gvht_en);

/**
 * ol_get_phymode_info_from_wlan_phymode() - Converts wlan_phymode to WMI_HOST_WLAN_PHY_MODE
 * @scn: scn pointer
 * @chan_mode: Input wlan_phymode.
 * @is_2gvht_en: VHT modes enabled for 2G.
 *
 * Return: uint32_t WMI_HOST_WLAN_PHY_MODE
 */

uint32_t
ol_get_phymode_info_from_wlan_phymode(struct ol_ath_softc_net80211 *scn,
        uint32_t chan_mode, bool is_2gvht_en);

unsigned int ol_ath_bmi_user_agent_init(ol_ath_soc_softc_t *soc);
int ol_ath_wait_for_bmi_user_agent(ol_ath_soc_softc_t *soc);
void ol_ath_signal_bmi_user_agent_done(ol_ath_soc_softc_t *soc);

void ol_ath_diag_user_agent_init(ol_ath_soc_softc_t *soc);
void ol_ath_diag_user_agent_fini(ol_ath_soc_softc_t *soc);
void ol_ath_host_config_update(ol_ath_soc_softc_t *soc);

void ol_ath_suspend_resume_attach(struct ol_ath_softc_net80211 *scn);
int ol_ath_suspend_target(ol_ath_soc_softc_t *soc, int disable_target_intr);
int ol_ath_resume_target(ol_ath_soc_softc_t *soc);
u_int ol_ath_mhz2ieee(struct ieee80211com *ic, u_int freq, u_int64_t flags);
void ol_ath_set_ht_vht_ies(struct ieee80211_node *ni);

int ol_print_scan_config(wlan_if_t vaphandle, struct seq_file *m);

int wmi_unified_set_ap_ps_param(struct ol_ath_vap_net80211 *avn,
        struct ol_ath_node_net80211 *anode, A_UINT32 param, A_UINT32 value);
int wmi_unified_set_sta_ps_param(struct ol_ath_vap_net80211 *avn,
        A_UINT32 param, A_UINT32 value);

int ol_ath_wmi_send_vdev_param(struct ol_ath_softc_net80211 *scn, uint8_t if_id,
        uint32_t param_id, uint32_t param_value);
int ol_ath_wmi_send_sifs_trigger(struct ol_ath_softc_net80211 *scn, uint8_t if_id,
        uint32_t param_value);
int ol_ath_pdev_set_param(struct ol_ath_softc_net80211 *scn,
                    uint32_t param_id, uint32_t param_value);
int
ol_ath_send_peer_assoc(struct ol_ath_softc_net80211 *scn, struct ieee80211com *ic,
                            struct ieee80211_node *ni, int isnew );

#ifdef OBSS_PD
int
ol_ath_send_cfg_obss_spatial_reuse_param(struct ol_ath_softc_net80211 *scn,
					 uint8_t if_id);
int ol_ath_set_obss_thresh(struct ieee80211com *ic, int enable, struct ol_ath_softc_net80211 *scn);
#endif

#if 0
int wmi_unified_pdev_get_tpc_config(wmi_unified_t wmi_handle, u_int32_t param);
int wmi_unified_pdev_caldata_version_check_cmd(wmi_unified_t wmi_handle, u_int32_t param);
#endif
int ol_ath_set_fw_hang(struct ol_ath_softc_net80211 *scn, u_int32_t delay_time_ms);
int peer_sta_kickout(struct ol_ath_softc_net80211 *scn, A_UINT8 *peer_macaddr);
int ol_ath_set_beacon_filter(wlan_if_t vap, u_int32_t *ie);
int ol_ath_remove_beacon_filter(wlan_if_t vap);
int
ol_get_tx_free_desc(struct ol_ath_softc_net80211 *scn);
void ol_get_radio_stats(struct ol_ath_softc_net80211 *scn, struct ol_ath_radiostats *stats);
#if 0
int
wmi_send_node_rate_sched(struct ol_ath_softc_net80211 *scn,
        wmi_peer_rate_retry_sched_cmd *cmd_buf);

int
wmi_unified_peer_flush_tids_send(wmi_unified_t wmi_handle,
                                 u_int8_t peer_addr[IEEE80211_ADDR_LEN],
                                 u_int32_t peer_tid_bitmap,
                                 u_int32_t vdev_id);
#endif
int
ol_ath_node_set_param(struct ol_ath_softc_net80211 *scn, u_int8_t *peer_addr,u_int32_t param_id,
        u_int32_t param_val,u_int32_t vdev_id);
#if UNIFIED_SMARTANTENNA
int ol_ath_smart_ant_rxfeedback(ol_txrx_pdev_handle pdev, ol_txrx_peer_handle peer, struct sa_rx_feedback *rx_feedback);
int ol_smart_ant_enabled(struct ol_ath_softc_net80211 *scn);
#endif /* UNIFIED_SMARTANTENNA */
#if QCA_LTEU_SUPPORT
void ol_ath_nl_attach(struct ieee80211com *ic);
void ol_ath_nl_detach(struct ieee80211com *ic);
#endif

int
ol_gpio_config(struct ol_ath_softc_net80211 *scn, u_int32_t gpio_num, u_int32_t input,
                        u_int32_t pull_type, u_int32_t intr_mode);
int
ol_ath_gpio_output(struct ol_ath_softc_net80211 *scn, u_int32_t gpio_num, u_int32_t set);

#define DEFAULT_PERIOD  100         /* msec */
#define DEFAULT_WLAN_DURATION   80  /* msec */
int
ol_ath_btcoex_duty_cycle(ol_ath_soc_softc_t *soc,u_int32_t period, u_int32_t duration);
int
ol_ath_btcoex_wlan_priority(ol_ath_soc_softc_t *soc, u_int32_t val);

int ol_ath_packet_power_info_get(struct ol_ath_softc_net80211 *scn, struct packet_power_info_params *param);
#if 0
int ol_ath_nf_dbr_dbm_info_get(struct ol_ath_softc_net80211 *scn);

int
wmi_unified_pdev_fips(struct ol_ath_softc_net80211 *scn,
                      u_int8_t *key,
                      u_int32_t key_len,
                      u_int8_t *data,
                      u_int32_t data_len,
        		      u_int32_t mode,
                      u_int32_t op);
#endif
int
ieee80211_extended_ioctl_chan_switch (struct net_device *dev,
                     struct ieee80211com *ic, caddr_t param);
int
ieee80211_extended_ioctl_chan_scan (struct net_device *dev,
                struct ieee80211com *ic, caddr_t param);

int
ieee80211_extended_ioctl_rep_move (struct net_device *dev,
                struct ieee80211com *ic, caddr_t param);

#if ATH_PROXY_NOACK_WAR
int32_t ol_ioctl_get_proxy_noack_war(struct ol_ath_softc_net80211 *scn, caddr_t param);
int32_t ol_ioctl_reserve_proxy_macaddr (struct ol_ath_softc_net80211 *scn, caddr_t *param);
int ol_ath_pdev_proxy_ast_reserve_event_handler (ol_scn_t sc, u_int8_t *data, u_int32_t datalen);
#endif
#if ATH_SUPPORT_WRAP && DBDC_REPEATER_SUPPORT
void ol_ioctl_disassoc_clients(struct ol_ath_softc_net80211 *scn);
int32_t ol_ioctl_get_primary_radio(struct ol_ath_softc_net80211 *scn, caddr_t param);
int32_t ol_ioctl_get_mpsta_mac_addr(struct ol_ath_softc_net80211 *scn, caddr_t param);
int32_t ol_ioctl_get_force_client_mcast(struct ol_ath_softc_net80211 *scn, caddr_t param);
int32_t ol_ioctl_get_max_priority_radio(struct ol_ath_softc_net80211 *scn, caddr_t param);
#endif
#if DBDC_REPEATER_SUPPORT
u_int16_t ol_ioctl_get_disconnection_timeout(struct ol_ath_softc_net80211 *scn, caddr_t param);
void ol_ioctl_iface_mgr_status(struct ol_ath_softc_net80211 *scn, caddr_t param);
#endif
u_int8_t ol_ioctl_get_stavap_connection(struct ol_ath_softc_net80211 *scn, caddr_t param);
int32_t ol_ioctl_get_preferred_uplink(struct ol_ath_softc_net80211 *scn, caddr_t param);
int32_t ol_ioctl_get_chan_vendorsurvey_info(struct ol_ath_softc_net80211 *scn,
        caddr_t param);
#if OL_ATH_SUPPORT_LED
extern void ol_ath_led_event(struct ol_ath_softc_net80211 *scn, OL_LED_EVENT event);
extern bool ipq4019_led_initialized;
extern void ipq4019_wifi_led(struct ol_ath_softc_net80211 *scn, int on_or_off);
extern void ipq4019_wifi_led_init(struct ol_ath_softc_net80211 *scn);
extern void ipq4019_wifi_led_deinit(struct ol_ath_softc_net80211 *scn);
#endif
int
ol_get_board_id(ol_ath_soc_softc_t *soc, char *boarddata_file );

int ol_ath_set_tx_capture (struct ol_ath_softc_net80211 *scn, int val);
int ol_ath_set_debug_sniffer(struct ol_ath_softc_net80211 *scn, int val);
void process_rx_mpdu(void *pdev, enum WDI_EVENT event, void *data, u_int16_t peer_id, enum htt_cmn_rx_status status);

int ol_ath_set_capture_latency(struct ol_ath_softc_net80211 *scn, int val);
#if QCN_IE
int ol_ath_set_bpr_wifi3(struct ol_ath_softc_net80211 *scn, int val);
#endif

void ol_ath_get_min_and_max_power(struct ieee80211com *ic,
                                  int8_t *max_tx_power,
                                  int8_t *min_tx_power);
bool ol_ath_is_regulatory_offloaded(struct ieee80211com *ic);
uint32_t ol_ath_get_modeSelect(struct ieee80211com *ic);
uint32_t ol_ath_get_chip_mode(struct ieee80211com *ic);
void ol_ath_fill_hal_chans_from_reg_db(struct ieee80211com *ic);

QDF_STATUS ol_ath_fill_umac_legacy_chanlist(struct wlan_objmgr_pdev *pdev,
        struct regulatory_channel *curr_chan_list);

/**
 * ol_ath_fill_umac_radio_band_info(): Fills the radio band information
 * based on the channel list supported by regdmn and chip capability.
 * @pdev: Pointer to the pdev object.
 */
uint8_t ol_ath_fill_umac_radio_band_info(struct wlan_objmgr_pdev *pdev);

QDF_STATUS ol_ath_set_country_failed(struct wlan_objmgr_pdev *pdev);

QDF_STATUS ol_scan_set_chan_list(struct wlan_objmgr_pdev *pdev, void *arg);

uint32_t ol_ath_get_interface_id(struct wlan_objmgr_pdev *pdev);

/**
 * ol_ath_init_and_enable_radar_table() - Initialize and enable the radar table.
 * @ scn: Pointer to ol_ath_softc_net80211 structure.
 */
void ol_ath_init_and_enable_radar_table(struct ol_ath_softc_net80211 *scn);

/**
 * ol_ath_num_mcast_tbl_elements() - get number of mcast table elements
 * @ic: ieee80211com object
 *
 * To get number of mcast table elements
 *
 * Return: number of mcast table elements
 */
uint32_t ol_ath_num_mcast_tbl_elements(struct ieee80211com *ic);

/**
 * ol_ath_num_mcast_grps() - get number of mcast groups
 * @ic: ieee80211com object
 *
 * To get number of mcast groups
 *
 * Return: number of mcast grps
 */
uint32_t ol_ath_num_mcast_grps(struct ieee80211com *ic);

/**
 * ol_ath_is_target_ar900b() - check  target type
 * @ic: ieee80211com object
 *
 * To check target type
 *
 * Return: True if the target type is ar900b else False
 */
bool ol_ath_is_target_ar900b(struct ieee80211com *ic);

/**
 * ol_ath__get_pdev_idx() - get pdev_id of Firmware
 * @ic: ieee80211com object
 *
 * To get pdev_id of FW
 *
 * Return: pdev_id
 */
int32_t ol_ath_get_pdev_idx(struct ieee80211com *ic);

/**
 * ol_ath_get_tgt_type() - get target type
 * @ic: ieee80211com object
 *
 * To get target type
 *
 * Return: target type
 */
uint32_t ol_ath_get_tgt_type(struct ieee80211com *ic);

#ifdef OL_ATH_SMART_LOGGING
int32_t
ol_ath_enable_smart_log(struct ol_ath_softc_net80211 *scn, uint32_t cfg);

QDF_STATUS
send_fatal_cmd(struct ol_ath_softc_net80211 *scn, uint32_t type,
        uint32_t subtype);

QDF_STATUS
ol_smart_log_connection_fail_start(struct ol_ath_softc_net80211 *scn);

QDF_STATUS
ol_smart_log_connection_fail_stop(struct ol_ath_softc_net80211 *scn);

#ifndef REMOVE_PKT_LOG
QDF_STATUS
ol_smart_log_fw_pktlog_enable(struct ol_ath_softc_net80211 *scn);

QDF_STATUS
ol_smart_log_fw_pktlog_disable(struct ol_ath_softc_net80211 *scn);

QDF_STATUS
ol_smart_log_fw_pktlog_start(struct ol_ath_softc_net80211 *scn,
        u_int32_t fw_pktlog_types);

QDF_STATUS
ol_smart_log_fw_pktlog_stop(struct ol_ath_softc_net80211 *scn);

QDF_STATUS
ol_smart_log_fw_pktlog_stop_and_block(struct ol_ath_softc_net80211 *scn,
        int32_t host_pktlog_types, bool block_only_if_started);

void
ol_smart_log_fw_pktlog_unblock(struct ol_ath_softc_net80211 *scn);
#endif /* REMOVE_PKT_LOG */
#endif /* OL_ATH_SMART_LOGGING */

#ifdef BIG_ENDIAN_HOST
     /* This API is used in copying in elements to WMI message,
        since WMI message uses multilpes of 4 bytes, This API
        converts length into multiples of 4 bytes, and performs copy
     */
#define OL_IF_MSG_COPY_CHAR_ARRAY(destp, srcp, len)  do { \
      int j; \
      u_int32_t *src, *dest; \
      src = (u_int32_t *)srcp; \
      dest = (u_int32_t *)destp; \
      for(j=0; j < roundup(len, sizeof(u_int32_t))/4; j++) { \
          *(dest+j) = qdf_le32_to_cpu(*(src+j)); \
      } \
   } while(0)

/* This macro will not work for anything other than a multiple of 4 bytes */
#define OL_IF_SWAPBO(x, len)  do { \
      int numWords; \
      int i; \
      void *pv = &(x); \
      u_int32_t *wordPtr; \
      numWords = (len)/sizeof(u_int32_t); \
      wordPtr = (u_int32_t *)pv; \
      for (i = 0; i < numWords; i++) { \
          *(wordPtr + i) = __cpu_to_le32(*(wordPtr + i)); \
      } \
   } while(0)

#else

#define OL_IF_MSG_COPY_CHAR_ARRAY(destp, srcp, len)  do { \
    OS_MEMCPY(destp, srcp, len); \
   } while(0)

#endif
#define AR9887_DEVICE_ID    (0x0050)
#define AR9888_DEVICE_ID    (0x003c)

/*
    *  * options for firmware dump generation
    *      - 0x1 - Dump to file
    *      - 0x2 - Dump to crash scope
    *      - 0x4 - Do not crash the host after dump
    *      - 0x8 - host/target recovery without dump
    *
*/
#define FW_DUMP_TO_FILE                 0x1u
#define FW_DUMP_TO_CRASH_SCOPE          0x2u
#define FW_DUMP_NO_HOST_CRASH           0x4u
#define FW_DUMP_RECOVER_WITHOUT_CORE    0x8u
#define FW_DUMP_ADD_SIGNATURE           0x10u

#define VHT_MCS_SET_FOR_NSS(x, ss) ( ((x) & (3 << ((ss)<<1))) >> ((ss)<<1) )
#define VHT_MAXRATE_IDX_SHIFT   4

/*
 * Band Width Types
 */
typedef enum {
	BW_20MHZ,
	BW_40MHZ,
	BW_80MHZ,
	BW_160MHZ,
	BW_CNT,
	BW_IDLE = 0xFF,//default BW state after WLAN ON.
} BW_TYPES;


/*Monitor filter types*/
typedef enum _monitor_filter_type {
    MON_FILTER_ALL_DISABLE          = 0x0,   //disable all filters
    MON_FILTER_ALL_EN               = 0x01,  //enable all filters
    MON_FILTER_TYPE_OSIF_MAC        = 0x02,  //enable osif MAC addr based filter
    MON_FILTER_TYPE_UCAST_DATA      = 0x04,  //enable htt unicast data filter
    MON_FILTER_TYPE_MCAST_DATA      = 0x08,  //enable htt multicast cast data filter
    MON_FILTER_TYPE_NON_DATA        = 0x10,  //enable htt non-data filter

    MON_FILTER_TYPE_LAST            = 0x1F,  //last
} monitor_filter_type;

#define RX_MON_FILTER_PASS          0x0001
#define RX_MON_FILTER_OTHER         0x0002

#define FILTER_MGMT_EN              0xFFFF
#define FILTER_CTRL_EN              0xFFFF

#define FILTER_DATA_UCAST_EN        0x8000
#define FILTER_DATA_MCAST_EN        0x4000

#define FILTER_TYPE_ALL_EN          0x0001
#define FILTER_TYPE_UCAST_DATA      0x0004
#define FILTER_TYPE_MCAST_DATA      0x0008
#define FILTER_TYPE_NON_DATA        0x0010

#define FILTER_TYPE_UCAST_DATA_EN   0x0005
#define FILTER_TYPE_MCAST_DATA_EN   0x0009
#define FILTER_TYPE_NON_DATA_EN     0x0011

#define FILTER_TYPE_BOTH            0xFFFF
#define FILTER_TYPE_FP              0x00FF
#define FILTER_TYPE_MO              0xFF00

#define SET_MON_RX_FILTER_MASK      0x00FF

#define SET_MON_FILTER_MODE(val)    \
        ((val & SET_MON_RX_FILTER_MASK) ?\
        RX_MON_FILTER_PASS | RX_MON_FILTER_OTHER: 0)

#define SET_MON_FILTER_MGMT(val)    \
        ((val & FILTER_TYPE_NON_DATA_EN) ? 0 : FILTER_MGMT_EN)

#define SET_MON_FILTER_CTRL(val)    \
        ((val & FILTER_TYPE_NON_DATA_EN) ? 0 : FILTER_CTRL_EN)

#define SET_MON_FILTER_DATA(val)    \
        (((val & FILTER_TYPE_UCAST_DATA_EN) ? 0 : FILTER_DATA_UCAST_EN) |\
        ((val & FILTER_TYPE_MCAST_DATA_EN) ? 0 : FILTER_DATA_MCAST_EN))

#if FW_CODE_SIGN
/* ideally these are supposed to go into a different file, in interest of time
 * where this involves LOST approvals etc, for all new files, adding it in the
 * current file, and would require reorg og the code, post the check-in. This
 * entire block would need to go into a file, fw_sign.h
 */
/* known product magic numbers */
#define FW_IMG_MAGIC_BEELINER       0x424c4e52U                      /* BLNR */
#define FW_IMG_MAGIC_CASCADE        0x43534345U                      /* CSCE */
#define FW_IMG_MAGIC_SWIFT          0x53574654U                      /* SWFT */
#define FW_IMG_MAGIC_PEREGRINE      0x5052474eU                      /* PRGN */
#define FW_IMG_MAGIC_DAKOTA         0x44414b54U                      /* DAKT */
#define FW_IMG_MAGIC_UNKNOWN        0x00000000U                      /* zero*/
/* chip idenfication numbers
 * Most of the times chip identifier is pcie device id. In few cases that could
 * be a non-pci device id if the device is not pci device. It is assumed at
 * at build time, it is known to the tools for which the firmware is getting
 * built.
 */
#define RSA_PSS1_SHA256 1

#ifndef NELEMENTS
#define NELEMENTS(__array) sizeof((__array))/sizeof ((__array)[0])
#endif

typedef struct _fw_device_id{
    u_int32_t      dev_id;                  /* this pcieid or internal device id */
    char       *dev_name;                    /* short form of the device name */
    u_int32_t      img_magic;                    /* image magic for this product */
} fw_device_id;

struct cert {
    const unsigned int cert_len;
    const unsigned char *cert;
};
enum {
    FW_SIGN_ERR_INTERNAL,
    FW_SIGN_ERR_INV_VER_STRING,
    FW_SIGN_ERR_INV_DEV_ID,
    FW_SIGN_ERR_INPUT,
    FW_SIGN_ERR_INPUT_FILE,
    FW_SIGN_ERR_FILE_FORMAT,
    FW_SIGN_ERR_FILE_ACCESS,
    FW_SIGN_ERR_CREATE_OUTPUT_FILE,
    FW_SIGN_ERR_UNSUPP_CHIPSET,
    FW_SIGN_ERR_FILE_WRITE,
    FW_SIGN_ERR_INVALID_FILE_MAGIC,
    FW_SIGN_ERR_INFILE_READ,
    FW_SIGN_ERR_IMAGE_VER,
    FW_SIGN_ERR_SIGN_ALGO,
    FW_SIGN_ERR_MAX
};

typedef uint fw_img_magic_t;
/* current version of the firmware signing , major 1, minor 0*/
#define THIS_FW_IMAGE_VERSION ((1<<15) | 0)

/* fw_img_file_magic
 * In current form of firmware download process, there are three different
 * kinds of files, that get downloaded. All of these can be in different
 * formats.
 *
 * 1. downloadable, executable, target resident (athwlan.bin)
 * 2. downloadable, executable, target non-resident otp (otp.bin)
 * 3. downloadable, non-executable, target file (fakeBoard)
 * 4. no-download, no-execute, host-resident code swap file.
 * 5. Add another for future
 * Each of these can be signed or unsigned, each of these can signed
 * with single key or can be signed with multiple keys, provisioning
 * all kinds in this list.
 * This list contains only filenames that are generic. Each board might
 * name their boards differently, but they continue to use same types.
 */
#define FW_IMG_FILE_MAGIC_TARGET_WLAN       0x5457414eu  /* target WLAN - TWLAN*/
#define FW_IMG_FILE_MAGIC_TARGET_OTP        0x544f5450u    /* target OTP TOTP */
#define FW_IMG_FILE_MAGIC_TARGET_BOARD_DATA 0x54424446u         /* target TBDF*/
#define FW_IMG_FILE_MAGIC_TARGET_CODE_SWAP  0x4857414eu         /* host HWLAN */
#define FW_IMG_FILE_MAGIC_INVALID           0

typedef uint fw_img_file_magic_t;

/* fw_img_sign_ver
 *
 * This supports for multiple revisions of this module to support different
 * features in future. This is defined in three numbers, major and minor
 * major - In general this would not change, unless, there is new header types
 *         are added or major crypto algorithms changed
 * minor - Minor changes like, adding new devices, new files etc.
 * There is no sub-release version for this, probably change another minor
 * version if really needs a change
 * All these are listed  in design document, probably in .c files
 */

/* fw_ver_rel_type
 * FW version release could be either test, of production, zero is not allowed
 */
#define FW_IMG_VER_REL_TEST         0x01
#define FW_IMG_VER_REL_PROD         0x02
#define FW_IMG_VER_REL_UNDEFIND     0x00

/*
 * fw_ver_maj, fw_ver_minor
 * major and minor versions are planned below.
 * u_int32_t  fw_ver_maj;                        * 32bits, MMMM.MMMM.SCEE.1111 *
 * u_int32_t  fw_ver_minor;                      * 32bits, mmmm.mmmm.rrrr.rrrr *
 */
/*
 * extrat major version number from fw_ver_maj field
 * Higher 16 bits of 32 bit quantity
 * FW_VER_GET_MAJOR - Extract Major version
 * FW_VER_SET_MAJOR - Clear the major version bits and set the bits
 */
#define FW_VER_MAJOR_MASK 0xffff0000u
#define FW_VER_MAJOR_SHIFT 16
#define FW_VER_GET_MAJOR(__mj)  (((__mj)->fw_ver_maj & FW_VER_MAJOR_MASK) >> FW_VER_MAJOR_SHIFT)
#define FW_VER_SET_MAJOR(__mj, __val)  (__mj)->fw_ver_maj =\
                        ((((__val) & 0x0000ffff) << FW_VER_MAJOR_SHIFT) |\
                        ((__mj)->fw_ver_maj  & ~FW_VER_MAJOR_MASK))

/*
 * Extract build variants. The following variants are defined at this moement.
 * This leaves out scope for future types.
 * This is just a number, so this can contain upto 255 values, 0 is undefined
 */
#define FW_VER_IMG_TYPE_S_RETAIL        0x1U
#define FW_VER_IMG_TYPE_E_ENTERPRISE    0x2U
#define FW_VER_IMG_TYPE_C_CARRIER       0x3U
#define FW_VER_IMG_TYPE_X_UNDEF         0x0U

#define FW_VER_IMG_TYPE_MASK            0x0000ff00
#define FW_VER_IMG_TYPE_SHIFT           8
#define FW_VER_GET_IMG_TYPE(__t) (((__t)->fw_ver_maj & FW_VER_IMG_TYPE_MASK) >>\
                                            FW_VER_IMG_TYPE_SHIFT)
#define FW_VER_SET_IMG_TYPE(__t, __val) \
        (__t)->fw_ver_maj  = \
            ((__t)->fw_ver_maj &~FW_VER_IMG_TYPE_MASK) | \
                ((((u_int32_t)(__val)) & 0xff) << FW_VER_IMG_TYPE_SHIFT)

#define FW_VER_IMG_TYPE_VER_MASK            0x000000ff
#define FW_VER_IMG_TYPE_VER_SHIFT           0

#define FW_VER_GET_IMG_TYPE_VER(__t) (((__t)->fw_ver_maj & \
                     FW_VER_IMG_TYPE_VER_MASK) >> FW_VER_IMG_TYPE_VER_SHIFT)

#define FW_VER_SET_IMG_TYPE_VER(__t, __val) (__t)->fw_ver_maj = \
                         ((__t)->fw_ver_maj&~FW_VER_IMG_TYPE_VER_MASK) |\
                         ((((u_int32_t)(__val)) & 0xff) << FW_VER_IMG_TYPE_VER_SHIFT)

#define FW_VER_IMG_MINOR_VER_MASK           0xffff0000
#define FW_VER_IMG_MINOR_VER_SHIFT          16
#define FW_VER_IMG_MINOR_SUBVER_MASK        0x0000ffff
#define FW_VER_IMG_MINOR_SUBVER_SHIFT       0

#define FW_VER_IMG_MINOR_RELNBR_MASK        0x0000ffff
#define FW_VER_IMG_MINOR_RELNBR_SHIFT       0

#define FW_VER_IMG_GET_MINOR_VER(__m) (((__m)->fw_ver_minor &\
                                        FW_VER_IMG_MINOR_VER_MASK) >>\
                                            FW_VER_IMG_MINOR_VER_SHIFT)

#define FW_VER_IMG_SET_MINOR_VER(__t, __val) (__t)->fw_ver_minor = \
                     ((__t)->fw_ver_minor &~FW_VER_IMG_MINOR_VER_MASK) |\
                     ((((u_int32_t)(__val)) & 0xffff) << FW_VER_IMG_MINOR_VER_SHIFT)

#define FW_VER_IMG_GET_MINOR_SUBVER(__m) (((__m)->fw_ver_minor & \
                     FW_VER_IMG_MINOR_SUBVER_MASK) >> FW_VER_IMG_MINOR_SUBVER_SHIFT)

#define FW_VER_IMG_SET_MINOR_SUBVER(__t, __val) (__t)->fw_ver_minor = \
                     ((__t)->fw_ver_minor&~FW_VER_IMG_MINOR_SUBVER_MASK) |\
                     ((((u_int32_t)(__val)) & 0xffff) << FW_VER_IMG_MINOR_SUBVER_SHIFT)

#define FW_VER_IMG_GET_MINOR_RELNBR(__m) (((__m)->fw_ver_bld_id &\
                     FW_VER_IMG_MINOR_RELNBR_MASK) >> FW_VER_IMG_MINOR_RELNBR_SHIFT)

#define FW_VER_IMG_SET_MINOR_RELNBR(__t, __val) (__t)->fw_ver_bld_id = \
                     ((__t)->fw_ver_bld_id &~FW_VER_IMG_MINOR_RELNBR_MASK) |\
                     ((((u_int32_t)(__val)) & 0xffff) << FW_VER_IMG_MINOR_RELNBR_SHIFT)

/* signed/unsigned - bit 0 of fw_hdr_flags */
#define FW_IMG_FLAGS_SIGNED                 0x00000001U
#define FW_IMG_FLAGS_UNSIGNED               0x00000000U
#define FW_IMG_IS_SIGNED(__phdr) ((__phdr)->fw_hdr_flags & FW_IMG_FLAGS_SIGNED)

/* file format type - bits 1,2,3*/
#define FW_IMG_FLAGS_FILE_FORMAT_MASK       0x0EU
#define FW_IMG_FLAGS_FILE_FORMAT_SHIFT      0x1U
#define FW_IMG_FLAGS_FILE_FORMAT_TEXT       0x1U
#define FW_IMG_FLAGS_FILE_FORMAT_BIN        0x2U
#define FW_IMG_FLAGS_FILE_FORMAT_UNKNOWN    0x0U
#define FW_IMG_FLAGS_FILE_FORMAT_GET(__flags) \
                    (((__flags) & FW_IMG_FLAGS_FILE_FORMAT_MASK) >> \
                    FW_IMG_FLAGS_FILE_FORMAT_SHIFT)

#define FW_IMG_FLAGS_FILE_COMPRES_MASK       0x10U
#define FW_IMG_FLAGS_FILE_COMPRES_SHIFT      0x4U
#define FW_IMG_FLAGS_COMPRESSED             0x1U
#define FW_IMG_FLAGS_UNCOMPRESSED           0x0U
#define FW_IMG_IS_COMPRESSED(__flags) ((__flags)&FW_IMG_FLAGS_FILE_COMPRES_MASK)
#define FW_IMG_FLAGS_SET_COMRESSED(__flags) \
                            ((__flags) |= 1 << FW_IMG_FLAGS_FILE_COMPRES_SHIFT)

#define FW_IMG_FLAGS_FILE_ENCRYPT_MASK       0x20U
#define FW_IMG_FLAGS_FILE_ENCRYPT_SHIFT      0x5U
#define FW_IMG_FLAGS_ENCRYPTED               0x1U
#define FW_IMG_FLAGS_UNENCRYPTED             0x0U
#define FW_IMG_IS_ENCRYPTED(__flags) ((__flags)&FW_IMG_FLAGS_FILE_ENCRYPT_MASK)
#define FW_IMG_FLAGS_SET_ENCRYPT(__flags) \
                            ((__flags) |= 1 << FW_IMG_FLAGS_FILE_ENCRYPT_SHIFT)

/* any file that is dowloaded is marked target resident
 * any file that is not downloaded but loaded at host
 * would be marked NOT TARGET RESIDENT file, or host resident file
 */
#define FW_IMG_FLAGS_FILE_TARGRES_MASK       0x40U
#define FW_IMG_FLAGS_FILE_TARGRES_SHIFT      0x6U
#define FW_IMG_FLAGS_TARGRES                 0x1U
#define FW_IMG_FLAGS_HOSTRES                 0x0U

#define FW_IMG_IS_TARGRES(__flags) ((__flags)&FW_IMG_FLAGS_FILE_TARGRES_MASK)

#define FW_IMG_FLAGS_SET_TARGRES(__flags) \
                   ((__flags) |= (1 <<FW_IMG_FLAGS_FILE_TARGRES_SHIFT))

#define FW_IMG_FLAGS_FILE_EXEC_MASK       0x80U
#define FW_IMG_FLAGS_FILE_EXEC_SHIFT      0x7U
#define FW_IMG_FLAGS_EXEC                 0x1U
#define FW_IMG_FLAGS_NONEXEC                 0x0U
#define FW_IMG_IS_EXEC(__flags) ((__flags)&FW_IMG_FLAGS_FILE_EXEC_MASK)
#define FW_IMG_FLAGS_SET_EXEC (__flags) \
                            ((__flags) |= 1 << FW_IMG_FLAGS_FILE_EXEC_SHIFT)

/* signing algorithms, only rsa-pss1 with sha256 is supported*/
enum {
    FW_IMG_SIGN_ALGO_UNSUPPORTED = 0,
    FW_IMG_SIGN_ALGO_RSAPSS1_SHA256  = 1
};
/* header of the firmware file, also contains the pointers to the file itself
 * and the signature at end of the file
 */
struct firmware_head {
    fw_img_magic_t      fw_img_magic_number;       /* product magic, eg, BLNR */
    u_int32_t              fw_chip_identification;/*firmware chip identification */

    /* boarddata, otp, swap, athwlan.bin etc */
    fw_img_file_magic_t fw_img_file_magic;
    u_int16_t              fw_img_sign_ver;        /* signing method version */

    u_int16_t              fw_img_spare1;                      /* undefined. */
    u_int32_t              fw_img_spare2;                       /* undefined */

    /* Versioning and release types */
    u_int32_t              fw_ver_rel_type;              /* production, test */
    u_int32_t              fw_ver_maj;        /* 32bits, MMMM.MMMM.SSEE.1111 */
    u_int32_t              fw_ver_minor;      /* 32bits, mmmm.mmmm.ssss.ssss */
    u_int32_t              fw_ver_bld_id;                 /* actual build id */
    /* image versioning is little tricky to handle. We assume there are three
     * different versions that we can encode.
     * MAJOR - 16 bits, lower 16 bits of this is spare, chip version encoded
               in lower 16 bits
     * minor - 16 bits, 0-65535,
     * sub-release - 16 bits, usually this would be zero. if required we
     * can use this. For eg. BL.2_1.400.2, is, Beeliner, 2.0, first major
     * release, build 400, and sub revision of 2 in the same build
     */
    u_int32_t             fw_ver_spare3;

     /* header identificaiton */
    u_int32_t              fw_hdr_length;                   /* header length */
    u_int32_t              fw_hdr_flags;                /* extra image flags */
    /* image flags are different single bit flags of different characterstics
    // At this point of time these flags include below, would extend as
    // required
    //                      signed/unsigned,
    //                      bin/text,
    //                      compressed/uncompressed,
    //                      unencrypted,
    //                      target_resident/host_resident,
    //                      executable/non-execuatable
    */
    u_int32_t              fw_hdr_spare4;                      /* future use */

    u_int32_t              fw_img_size;                       /* image size; */
    u_int32_t              fw_img_length;            /* image length in byes */
    /* there is no real requirement for keeping the size, the size is
     * fw_img_size = fw_img_length - ( header_size + signature)
     * in otherwords fw_img_size is actual image size that would be
     * downloaded to target board.
     */
    u_int32_t              fw_spare5;
    u_int32_t              fw_spare6;

    /* security details follows here after */
    u_int16_t              fw_sig_len;     /* index into known signature lengths */
    u_int8_t               fw_sig_algo;                   /* signature algorithm */
    u_int8_t               fw_oem_id;            /* oem ID, to access otp or etc */

#if 0
    /* actual image body    */
    u_int8_t              *fw_img_body;                     /* fw_img_size bytes */
    u_int8_t              *fw_img_padding;          /*if_any_upto_4byte_boundary */
    u_int8_t              *fw_signature;                  /* pointer to checksum */
#endif
};
#endif /* FW_CODE_SIGN */

enum pdev_oper {
       PDEV_ITER_POWERUP,
       PDEV_ITER_TARGET_ASSERT,
       PDEV_ITER_PCIE_ASSERT,
       PDEV_ITER_PDEV_ENTRY_ADD,
       PDEV_ITER_PDEV_ENTRY_DEL,
       PDEV_ITER_RECOVERY_AHB_REMOVE,
       PDEV_ITER_RECOVERY_REMOVE,
       PDEV_ITER_RECOVERY_WAIT,
       PDEV_ITER_RECOVERY_STOP,
       PDEV_ITER_RECOVERY_PROBE,
       PDEV_ITER_LED_GPIO_STATUS,
       PDEV_ITER_PDEV_NETDEV_STOP,
       PDEV_ITER_PDEV_NETDEV_OPEN,
       PDEV_ITER_TARGET_FWDUMP,
       PDEV_ITER_SEND_SUSPEND,
       PDEV_ITER_SEND_RESUME,
       PDEV_ITER_PDEV_DEINIT_BEFORE_SUSPEND,
       PDEV_ITER_PDEV_DETACH_OP,
       PDEV_ITER_PDEV_DEINIT_OP,
       PDEV_ITER_PDEV_RESET_PARAMS,
       PDEV_ITER_FATAL_SHUTDOWN,
};

struct pdev_op_args {
    enum pdev_oper type;
    void *pointer;
    int8_t ret_val;
};

enum status_code {
    PDEV_ITER_STATUS_INIT,
    PDEV_ITER_STATUS_OK,
    PDEV_ITER_STATUS_FAIL,
};

void wlan_pdev_operation(struct wlan_objmgr_psoc *psoc,
                             void *obj, void *args);
struct wlan_lmac_if_tx_ops;

extern QDF_STATUS wlan_global_lmac_if_set_txops_registration_cb(WLAN_DEV_TYPE dev_type,
                        QDF_STATUS (*handler)(struct wlan_lmac_if_tx_ops *));
extern QDF_STATUS wlan_lmac_if_set_umac_txops_registration_cb
                        (QDF_STATUS (*handler)(struct wlan_lmac_if_tx_ops *));

QDF_STATUS olif_register_umac_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops);

bool ol_ath_is_beacon_offload_enabled(ol_ath_soc_softc_t *soc);

uint32_t num_chain_from_chain_mask(uint32_t mask);

QDF_STATUS
ol_ath_mgmt_beacon_send(struct wlan_objmgr_vdev *vdev,
                        wbuf_t wbuf);
QDF_STATUS
ol_if_mgmt_send (struct wlan_objmgr_vdev *vdev,
                 qdf_nbuf_t nbuf, u_int32_t desc_id,
                 void *mgmt_tx_params);
QDF_STATUS register_legacy_wmi_service_ready_callback(void);

#ifdef WLAN_CONV_CRYPTO_SUPPORTED
int ol_ath_vdev_install_key_send(struct ieee80211vap *vap, struct ol_ath_softc_net80211 *scn, u_int8_t if_id,
                             struct wlan_crypto_key *key,
                             u_int8_t *macaddr,
                             u_int8_t def_keyid, u_int8_t force_none, uint32_t keytype);
#else
int ol_ath_vdev_install_key_send(struct ieee80211vap *vap, struct ol_ath_softc_net80211 *scn, u_int8_t if_id,
                             struct ieee80211_key *key,
                             u_int8_t *macaddr,
                             u_int8_t def_keyid, u_int8_t force_none, uint32_t keytype);
#endif

#if QCA_11AX_STUB_SUPPORT
/**
 * @brief Determine whether 802.11ax stubbing is enabled or not
 *
 * @param soc - ol_ath_soc_softc_t structure for the soc
 * @return Integer status value.
 *      -1 : Failure
 *       0 : Disabled
 *       1 : Enabled
 */
extern int ol_ath_is_11ax_stub_enabled(ol_ath_soc_softc_t *soc);

#define OL_ATH_IS_11AX_STUB_ENABLED(_soc) ol_ath_is_11ax_stub_enabled((_soc))

#else
#define OL_ATH_IS_11AX_STUB_ENABLED(_soc) (0)
#endif /* QCA_11AX_STUB_SUPPORT */

QDF_STATUS target_if_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops);
int ol_ath_offload_bcn_tx_status_event_handler(ol_scn_t sc, uint8_t *data, uint32_t datalen);
int ol_ath_pdev_csa_status_event_handler(ol_scn_t sc, u_int8_t *data, u_int32_t datalen);

bool ol_target_lithium(struct wlan_objmgr_psoc *psoc);
void ol_ath_resmgr_soc_attach(ol_ath_soc_softc_t *soc);
void rx_dp_peer_invalid(void *scn_handle, enum WDI_EVENT event, void *data, uint16_t peer_id);

QDF_STATUS
ol_if_dfs_enable(struct wlan_objmgr_pdev *pdev, int *is_fastclk,
                 struct wlan_dfs_phyerr_param *param,
                 uint32_t dfsdomain);

QDF_STATUS
ol_if_get_tsf64(struct wlan_objmgr_pdev *pdev, uint64_t *tsf64);

QDF_STATUS
ol_dfs_get_caps(struct wlan_objmgr_pdev *pdev,
                struct wlan_dfs_caps *dfs_caps);

QDF_STATUS ol_if_dfs_get_thresholds(struct wlan_objmgr_pdev *pdev,
                                    struct wlan_dfs_phyerr_param *param);

QDF_STATUS
ol_if_dfs_disable(struct wlan_objmgr_pdev *pdev, int no_cac);

QDF_STATUS
ol_if_dfs_get_ext_busy(struct wlan_objmgr_pdev *pdev, int *ext_chan_busy);

QDF_STATUS
ol_ath_get_target_type(struct wlan_objmgr_pdev *pdev, uint32_t *target_type);

QDF_STATUS
ol_is_mode_offload(struct wlan_objmgr_pdev *pdev, bool *is_offload);

QDF_STATUS ol_ath_get_ah_devid(struct wlan_objmgr_pdev *pdev, uint16_t *devid);

QDF_STATUS ol_ath_get_phymode_info(struct wlan_objmgr_pdev *pdev,
                                   uint32_t chan_mode,
                                   uint32_t *mode_info,
                                   bool is_2gvht_en);

u_int32_t ol_if_peer_get_rate(struct wlan_objmgr_peer *peer , u_int8_t type);

int
ol_ath_find_logical_del_peer_and_release_ref(struct ieee80211vap *vap, uint8_t *peer_mac_addr);
int ol_ath_rel_ref_for_logical_del_peer(struct ieee80211vap *vap,
        struct ieee80211_node *ni, uint8_t *peer_mac_addr);
#define VALIDATE_TX_CHAINMASK 1
#define VALIDATE_RX_CHAINMASK 2
bool ol_ath_validate_chainmask(struct ol_ath_softc_net80211 *scn,
        uint32_t chainmask, int direction, int phymode);

#if ATH_SUPPORT_NAC_RSSI
int
ol_ath_config_fw_for_nac_rssi(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id,  enum cdp_nac_param_cmd cmd, char *bssid, char *client_macaddr,
                              uint8_t chan_num);
int
ol_ath_config_bssid_in_fw_for_nac_rssi(struct wlan_objmgr_pdev *pdev, uint8_t vdev_id, enum cdp_nac_param_cmd cmd, char *bssid);
#endif

void
ol_ath_process_ppdu_stats(void *pdev_hdl, enum WDI_EVENT event,
                             void *data, uint16_t peer_id, uint32_t status);
void ol_ath_process_tx_metadata(struct ieee80211com *ic, void *data);
void ol_ath_process_rx_metadata(struct ieee80211com *ic, void *data);
void ol_ath_subscribe_ppdu_desc_info(struct ol_ath_softc_net80211 *scn, uint8_t context);
void ol_ath_unsubscribe_ppdu_desc_info(struct ol_ath_softc_net80211 *scn, uint8_t context);

#ifdef ATH_SUPPORT_DFS
#if defined(WLAN_DFS_PARTIAL_OFFLOAD) && defined(HOST_DFS_SPOOF_TEST)
QDF_STATUS
ol_if_is_host_dfs_check_support_enabled(struct wlan_objmgr_pdev *pdev,
        bool *enabled);
void
ol_vdev_add_dfs_violated_chan_to_nol(struct ieee80211com *ic,
                                     struct ieee80211_ath_channel *chan);
void ol_vdev_pick_random_chan_and_restart(wlan_if_t vap);
#endif /* HOST_DFS_SPOOF_TEST */
#endif
enum ieee80211_opmode ieee80211_new_opmode(struct ieee80211vap *vap, bool vap_active);

int ol_ath_send_ft_roam_start_stop(struct ieee80211vap *vap, uint32_t start);

enum {
    OL_TXRX_VDEV_STATS_TX_INGRESS = 1,
};

/* The following definitions are used by SON application currently.
 * The converged SON does not have apis exposed to set / get pdev param
 * currently. Hence these are not put into son specific files as retreiving
 * these definitions would be tough. Another copy of these definitions are
 * made in band_steering_api.h file to make one-to-one mapping easier at
 * application level.
 */
#define LOW_BAND_MIN_FIVEG_FREQ 5180 /*FIVEG_LOW_BAND_MIN_FREQ */
#define LOW_BAND_MAX_FIVEG_FREQ 5320 /*FIVEG_LOW_BAND_MAX_FREQ */
#define MAX_FREQ_IN_TWOG 2484 /*TWOG_MAX_FREQ */
/**
 * @brief Get whether the Radio is tuned for low, high, full band or 2g.
 */
enum {
    NO_BAND_INFORMATION_AVAILABLE = 0, /* unable to retrieve band info due to some error */
    HIGH_BAND_RADIO, /*RADIO_IN_HIGH_BAND*/
    FULL_BAND_RADIO,/* RADIO_IN_FULL_BAND */
    LOW_BAND_RADIO, /* RADIO_IN_LOW_BAND */
    NON_5G_RADIO, /* RADIO_IS_NON_5G */
};
QDF_STATUS
ol_ath_set_pcp_tid_map(ol_txrx_vdev_handle vdev, uint32_t mapid);
QDF_STATUS
ol_ath_set_tidmap_prty(ol_txrx_vdev_handle vdev, uint32_t prec_val);
int
ol_ath_set_default_pcp_tid_map(struct ieee80211com *ic,
                              uint32_t pcp, uint32_t tid);
int
ol_ath_set_default_tidmap_prty(struct ieee80211com *ic, uint32_t val);
#endif /* _DEV_OL_ATH_ATHVAR_H  */
