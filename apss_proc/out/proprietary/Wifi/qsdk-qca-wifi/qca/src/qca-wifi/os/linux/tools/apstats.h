/*
 * Copyright (c) 2011, Qualcomm Atheros Inc.
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
 * Description:  Header file for the apstats application.
 *
 *
 * Version    :  0.1
 * Created    :  Thursday, June 30, 2011
 * Revision   :  none
 * Compiler   :  gcc
 *
 */
#ifndef APSTATS_H
#define APSTATS_H

#include <net/ethernet.h>
#include <qcatools_lib.h>
#include <ah_desc.h>
#include <athrs_ctrl.h>
#if ATH_PERF_PWR_OFFLOAD
#include <athtypes_linux.h>
#include <cdp_txrx_stats_struct.h>
#include <ol_txrx_stats.h>
#endif

#include <ieee80211_external.h>

#define ATH_IOCTL_GETPARAM  (SIOCIWFIRSTPRIV + 1)

#define APSTATS_VERSION     "0.1"

#define PATH_PROCNET_DEV    "/proc/net/dev"
#define PATH_SYSNET_DEV     "/sys/class/net/"

/* Rates in kbps */
#define RATE_10K_KBPS       (10000)
#define RATE_100K_KBPS      (100000)
#define RATE_1000K_KBPS     (1000000)

/**
 * Required AP statistics level.
 */
typedef enum
{
    APSTATS_LEVEL_NO_VAL,
    APSTATS_LEVEL_AP,
    APSTATS_LEVEL_RADIO,
    APSTATS_LEVEL_VAP,
    APSTATS_LEVEL_NODE,
    APSTATS_LEVEL_NUM_ENTRIES,
} apstats_level_t;

/**
 * Required recursion characteristics.
 */
typedef enum
{
    APSTATS_RECURSION_NO_VAL,
    APSTATS_RECURSION_FULLY_ENABLED,
    APSTATS_RECURSION_FULLY_DISABLED,
    APSTATS_RECURSION_NUM_ENTRIES,
} apstats_recursion_t;

/**
 * Application configuration - populated based on command line arguments.
 */
typedef struct
{
    int                 is_initialized;
    apstats_level_t     level;
    apstats_recursion_t recursion;
    char                ifname[IFNAMSIZ];
    struct ether_addr   stamacaddr;

} apstats_config_t;

/**
 * Node (STA) level stats.
 */
typedef struct _nodelevel_stats_t
{
    /* Statistics. */

    u_int64_t tx_data_packets;       /**< No. of data frames sent to STA. */
    u_int64_t tx_data_bytes;         /**< No. of data bytes sent to STA. */
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
    u_int64_t tx_data_packets_success;
    u_int64_t tx_data_bytes_success;
#endif
    u_int64_t tx_data_wme[WME_NUM_AC]; /** No. of data frames transmitted
					    per AC */

    u_int64_t rx_data_wme[WME_NUM_AC]; /** No. of data frames received
					    per AC */

    u_int64_t rx_data_packets;       /**< No. of data frames received from STA. */
    u_int64_t rx_data_bytes;         /**< No. of data bytes received from STA. */

#if ATH_SUPPORT_EXT_STAT
    u_int64_t tx_bytes_rate;         /* transmitted bytes for last one second */
    u_int64_t tx_data_rate;          /* transmitted packets for last one second */
    u_int64_t rx_bytes_rate;         /* received bytes for last one second */
    u_int64_t rx_data_rate;          /* received packets for last one second */
#endif

#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
    u_int64_t rx_ucast_data_packets;
    u_int64_t rx_ucast_data_bytes;
    u_int64_t rx_mcast_data_packets;
    u_int64_t rx_mcast_data_bytes;
#endif

    u_int64_t tx_ucast_data_packets; /**< No. of unicast data frames sent to
                                          STA. */
#if UMAC_SUPPORT_STA_STATS_ENHANCEMENT
    u_int64_t tx_ucast_data_bytes;
    u_int64_t tx_mcast_data_packets;
    u_int64_t tx_mcast_data_bytes;
#endif
    u_int64_t last_per;		     /**< last Packet Error Rate */
    u_int32_t tx_rate;               /**< Average link rate used for
                                          transmissions to STA. */
    u_int32_t rx_rate;               /**< Average link rate used by STA to transmit
                                          to us. */

    u_int64_t host_discard;

    u_int64_t rx_micerr;             /**< No. of MIC errors in frames received
                                          from STA. */
    u_int64_t rx_decrypterr;         /**< No. of decryption errors in frames
                                          received from STA. */
    u_int64_t rx_err;                /**< No. of receive errors for this STA. */

    u_int64_t tx_discard;            /**< No. of frames destined to STA whose
                                          transmission failed (Note: Not number
                                          of failed attempts). */
    u_int64_t packets_queued;
    u_int64_t last_tx_rate;
    u_int64_t last_rx_rate;
    u_int64_t last_rx_mgmt_rate;     /** last received rate for mgmt frame */

    u_int64_t num_rx_mpdus;          /**< Number of mpdus received */
    u_int64_t num_rx_ppdus;          /**< Number of ppdus received */
    u_int64_t num_rx_retries;        /**< Number of rx retries */

    u_int8_t  rx_rssi;               /**< Rx RSSI of last frame received from
                                          this STA. */

    u_int8_t  rx_mgmt_rssi;          /**< Rx RSSI of last mgmt frame received from
                                          this STA. */
#if ATH_SUPPORT_EXT_STAT
    u_int8_t  chwidth;               /* communication band width with this STA */
    u_int32_t htcap;                 /* htcap of this STA */
    u_int32_t vhtcap;                /* vhtcap of this STA */
#endif

    u_int64_t tx_mgmt;               /**< No. of mgmt frames transmitted to STA */
    u_int64_t rx_mgmt;               /**< No. of mgmt frames received from STA */


#if ATH_PERF_PWR_OFFLOAD
    u_int32_t ack_rssi[MAX_CHAINS]; /**< Rx RSSI of ack frames received from
                                          this STA. */
#endif

    u_int32_t ppdu_tx_rate;         /* Avg per ppdu tx rate to the STA */
    u_int32_t ppdu_rx_rate;         /* Avg per ppdu rx rate from the STA */
    u_int8_t ol_enable;

    struct ieee80211req_sta_info si;

    /* Miscellaneous info required for application logic. */

    struct ether_addr macaddr;       /**< MAC address of STA. */
    void  *parent;                   /**< Pointer back to vaplevel_stats_t for
                                          VAP under which the node falls. */

    struct _nodelevel_stats_t *next; /**< Next in peer Linked List. */
    struct _nodelevel_stats_t *prev; /**< Previous in peer Linked List. */

    uint64_t excretries[WME_NUM_AC];  /**< excessive retries */
 /* No of packets not trans successfully due to no of retrans attempts exceeding 802.11 retry limit */
    uint32_t failed_retry_count;
    /* No of packets that were successfully transmitted after one or more retransmissions */
    uint32_t retry_count;
    /* No of packets that were successfully transmitted after more than one retransmission */
    uint32_t multiple_retry_count;

} nodelevel_stats_t;

/**
 * VAP level stats.
 */
typedef struct _vaplevel_stats_t
{
    /* Statistics. */

    u_int64_t tx_data_packets;       /**< No. of data frames transmitted. */
    u_int64_t tx_data_bytes;         /**< No. of data bytes transmitted. */
    u_int64_t tx_datapyld_bytes;     /**< No. of data frame payload bytes
                                          transmitted. */
    u_int64_t tx_data_wme[WME_NUM_AC]; /** No. of data frames transmitted
					    per AC */
    u_int64_t rx_data_wme[WME_NUM_AC]; /** No. of data frames received
					    per AC */
    u_int64_t rx_data_packets;       /**< No. of data frames received. */
    u_int64_t rx_data_bytes;         /**< No. of data bytes received. */
    u_int64_t rx_datapyld_bytes;     /**< No. of data frame payload bytes
                                          received. */

    u_int64_t tx_ucast_data_packets; /**< No. of unicast data frames
                                          transmitted. */
    u_int64_t rx_ucast_data_packets;  /**< No. of unicast data frames
                                           received. */
    u_int64_t tx_mbcast_data_packets;/**< No. of multicast/broadcast data frames
                                          transmitted. */
    u_int64_t rx_mbcast_data_packets;/**< No. of multicast/broadcast data frames
                                       transmitted. */

    u_int64_t tx_bcast_data_packets;
    u_int64_t rx_bcast_data_packets;
    u_int8_t  txrx_rate_available;   /**< Whether Average Tx/Rx rate information
                                          is available for this VAP (it is
                                          gathered from per-node stats, so
                                          at least one STA should be associated
                                          with the VAP for this to be
                                          available). */
    u_int32_t tx_rate;               /**< Average link rate of transmissions. */
    u_int32_t rx_rate;               /**< Average link rate at which frames
                                          were received. */

    u_int8_t  chan_util_enab;        /**< Whether Channel Utilization measurement
                                          is enabled. */
    u_int16_t chan_util;             /**< Channel Utilization (0-255). */

    u_int64_t rx_micerr;             /**< No. of MIC errors in frames received. */
    u_int64_t rx_decrypterr;         /**< No. of decryption errors in frames
                                          received. */
    u_int64_t rx_err;                /**< No. of receive errors. */
    u_int64_t tx_err;                /**< No. of transmitted errors. */
    u_int64_t tx_discard;            /**< No. of discarded packets sent. */
    u_int64_t host_discard;

    u_int64_t rx_discard;            /**< No. of frames whose transmission
                                          failed. (Note: Not number of failed
                                          attempts). */
    u_int64_t u_last_tx_rate;
    u_int64_t m_last_tx_rate;
    u_int64_t u_last_tx_rate_mcs;
    u_int64_t m_last_tx_rate_mcs;
    u_int64_t tx_offer_packets;
    u_int64_t tx_offer_packets_bytes;
    u_int64_t mgmt_tx_fail;          /**< mgmt Tx failure count */

    u_int64_t last_per;              /**< Total cumulative PER of all nodes in this vap */
    /* Miscellaneous info required for application logic. */

    char ifname[IFNAMSIZ];           /**< Interface name for VAP. */
    struct ether_addr macaddr;       /**< MAC address of VAP. */
    void  *parent;                   /**< Pointer back to radiolevel_stats_t
                                          for radio under which the VAP falls. */
    void  *firstchild;               /**< Pointer to first nodelevel_stats_t in
                                          Linked List for STAs under this VAP.*/
    struct _vaplevel_stats_t *next;  /**< Next in peer linked list. */
    struct _vaplevel_stats_t *prev;  /**< Previous in peer linked list. */
    u_int32_t   total_num_offchan_tx_mgmt; /* total number of offchan TX mgmt frames */
    u_int32_t   total_num_offchan_tx_data; /* total number of offchan TX data frames */
    u_int32_t   num_offchan_tx_failed;     /* number of offchan TX frames failed */
    u_int64_t tx_beacon_swba_cnt;    /*Beacon intr SWBA counter*/
    u_int64_t retries;               /**< retries */
    u_int64_t tx_mgmt;               /**< No. of mgmt frames transmitted */
    u_int64_t rx_mgmt;               /**< No. of mgmt frames received */
    uint64_t excretries[WME_NUM_AC];  /**< excessive retries */
    u_int32_t   tx_bcn_succ_cnt;
    u_int32_t   tx_bcn_outage_cnt;

    u_int64_t sta_xceed_rlim;         /* no of connections per vap refused after radio limit */
    u_int64_t sta_xceed_vlim;         /* no of connections per vap refused after vap limit */
    u_int64_t mlme_auth_attempt;      /* no of 802.11 MLME Auth Attempt per vap */
    u_int64_t mlme_auth_success;      /* no of 802.11 MLME Auth success per vap */
    u_int64_t authorize_attempt;      /* no of Authorization attempt per vap */
    u_int64_t authorize_success;      /* no of Authorization success per vap */
    u_int64_t tx_eapol_packets;       /* no of EAPol data frames transmitted */
    u_int64_t peer_delete_req;        /* no of peer delete requests */
    u_int64_t peer_delete_resp;       /* no of peer delete response */
} vaplevel_stats_t;

/**
 * Radio level stats.
 */
typedef struct _radiolevel_stats_t
{
    /* Statistics. */

    u_int64_t tx_data_packets;       /**< No. of data frames transmitted. */
    u_int64_t tx_data_bytes;         /**< No. of data bytes transmitted. */
    u_int64_t rx_data_packets;       /**< No. of data frames received. */
    u_int64_t rx_data_bytes;         /**< No. of data bytes received. */

    u_int64_t tx_ucast_data_packets; /**< No. of unicast data frames
                                          transmitted. */
    u_int64_t tx_mbcast_data_packets;/**< No. of multicast/broadcast data frames
                                          transmitted. */
    u_int64_t tx_data_wme[WME_NUM_AC]; /**< No. of data frames transmitted per AC */
    u_int64_t rx_data_wme[WME_NUM_AC]; /**< No. of data frames received per AC */
    u_int64_t tx_beacon_frames;      /**< No. of beacon frames transmitted. */
    u_int64_t tx_mgmt_frames;        /**< No. of management frames transmitted
                                          (including beacon frames). */
    u_int64_t rx_mgmt_frames;        /**< No. of management frames received. */

    u_int64_t rx_mgmt_frames_rssi_drop;  /**< No. of management frames dropped because of low RSSI */

    u_int8_t  txrx_rate_available;   /**< Whether Average Tx/Rx rate information
                                          is available for this radio (depends
                                          on whether the info is available
                                          for at least one of the VAPs). */
    u_int32_t tx_rate;               /**< Average link rate of transmissions. */
    u_int32_t rx_rate;               /**< Average link rate at which frames
                                          were received. */

    u_int8_t  rx_rssi;               /**< Rx RSSI of last frame received. */

    u_int8_t  chan_util_enab;        /**< Whether Channel Utilization measurement
                                          is enabled (depends on whether it is
                                          enabled for at least one of the
                                          VAPs). */
    u_int16_t chan_util;             /**< Channel Utilization (0-255). This is the
                                          average of per-VAP values, considering
                                          those VAPs with the measurement
                                          enabled. */

    u_int64_t rx_phyerr;             /**< No. of PHY errors in frames received. */
    u_int64_t rx_crcerr;             /**< No. of CRC errors in frames received. */
    u_int64_t rx_micerr;             /**< No. of MIC errors in frames received. */
    u_int64_t rx_decrypterr;         /**< No. of decryption errors in frames
                                          received. */
    u_int64_t rx_err;                /**< No. of receive errors. */

    u_int64_t tx_discard;            /**< No. of frames whose transmission
                                          failed. (Note: Not number of failed
                                          attempts). */
    u_int64_t tx_err;                /**< No. of tx error  */
    u_int8_t  thrput_enab;           /**< Whether in-driver throughput measurement
                                          is enabled. */
    u_int32_t thrput;                /**< Throughput in kbps */

    u_int8_t  total_per;             /**< PER measured from start of operation. */
    u_int8_t  prdic_per_enab;        /**< Whether periodic PER measurement is
                                          enabled. */
    u_int8_t  prdic_per;             /**< Periodic PER measured in configured
                                          time window. */

    u_int64_t rx_ctl_frames;            /**< Rxed control frames */
    u_int64_t tx_ctl_frames;            /**< Txed control frames */

    u_int64_t sta_xceed_rlim;         /* no of connections per radio refused after radio limit */
    u_int64_t sta_xceed_vlim;         /* no of connections per radio refused after vap limit */
    u_int64_t mlme_auth_attempt;      /* no of 802.11 MLME Auth Attempt per radio  */
    u_int64_t mlme_auth_success;      /* no of 802.11 MLME Auth success per radio  */
    u_int64_t authorize_attempt;      /* no of Authorization attempt per radio  */
    u_int64_t authorize_success;      /* no of Authorization success per radio  */
    u_int8_t  self_bss_util;            /* self bss channel utilization per radio */
    u_int8_t  obss_util;               /* other bss and non wifi channel utilization per radio */

    /* Miscellaneous info required for application logic. */

    char ifname[IFNAMSIZ];           /**< Interface name for radio. */
    struct ether_addr macaddr;       /**< MAC address for radio. */
    void  *parent;                   /**< Pointer back to aplevel_stats_t for
                                          AP. */
    void  *firstchild;               /**< Pointer to first vaplevel_stats_t in
                                          Linked List for VAPs under this radio.*/
    struct _radiolevel_stats_t *next;/**< Next in peer linked list. */
    struct _radiolevel_stats_t *prev;/**< Previous in peer linked list. */
    /*
    Required to determine which wlan interface.
    */
    u_int8_t ol_enable;

    int32_t chan_nf;
    u_int32_t tx_frame_count;
    u_int32_t rx_frame_count;
    u_int32_t rx_clear_count;
    u_int32_t cycle_count;
    u_int32_t phy_err_count;
    u_int32_t chan_tx_pwr;
} radiolevel_stats_t;

/**
 * AP level stats.
 */
typedef struct
{
    /* WLAN statistics. */

    u_int64_t tx_data_packets;      /**< No. of data frames transmitted. */
    u_int64_t tx_data_bytes;        /**< No. of data bytes transmitted. */
    u_int64_t rx_data_packets;      /**< No. of data frames received. */
    u_int64_t rx_data_bytes;        /**< No. of data bytes received. */

    u_int64_t tx_ucast_data_packets; /**< No. of unicast data frames
                                          transmitted. */
    u_int64_t tx_mbcast_data_packets;/**< No. of multicast/broadcast data frames
                                          transmitted. */

    u_int8_t  txrx_rate_available;  /**< Whether Average Tx/Rx rate information
                                         is available for the AP (depends on
                                         whether the information is available
                                         for at least one of the radios). */
    u_int32_t tx_rate;              /**< Average link rate of transmissions. */
    u_int32_t rx_rate;              /**< Average link rate at which frames
                                         were received. */

    u_int8_t  res_util_enab;        /**< Whether Resource Utilization measurement
                                         is available (depends on whether Channel
                                         Utilization is enabled for all
                                         radios). */
    u_int16_t res_util;             /**< Resource Utilization (0-255). This is
                                         the average of Channel Utilization
                                         figures across radios. */

    u_int64_t rx_phyerr;            /**< No. of PHY errors in frames received. */
    u_int64_t rx_crcerr;            /**< No. of CRC errors in frames received. */
    u_int64_t rx_micerr;            /**< No. of MIC errors in frames received. */
    u_int64_t rx_decrypterr;        /**< No. of decryption errors in frames
                                         received. */
    u_int64_t rx_err;               /**< No. of receive errors. */

    u_int64_t tx_discard;           /**< No. of frames whose transmission
                                         failed. (Note: Not number of failed
                                         attempts). */
    u_int64_t tx_err;               /**< No. of tx errors */

    u_int8_t  thrput_enab;          /**< Whether in-driver throughput measurement
                                         is available (depends on whether in-driver
                                         throughput measurement is enabled for at
                                         least one of the radios). */
    u_int32_t thrput;               /**< Throughput in kbps. This is the sum of
                                         of throughput values across radios. */

    u_int8_t total_per;             /**< PER measured from start of operation.
                                         This is the average of total PER values
                                         across radios. */
    u_int8_t prdic_per_enab;        /**< Whether periodic PER measurement is
                                         available (depends on whether periodic
                                         PER measurement is enabled for
                                         all radios). */
    u_int8_t prdic_per;             /**< Average of periodic PER values across
                                         all radios. */

    /* Miscellaneous info required for application logic. */

    void   *firstwlanchild;         /**< Pointer to first radiolevel_stats_t in
                                         Linked List for radios under this AP */
}  aplevel_stats_t;

/* Structure to hold radio and VAP names read from /proc */
typedef struct
{
    char **ifnames;
    int max_size;
    int curr_size;
} sys_ifnames;

extern int sys_ifnames_init(sys_ifnames *ifs, int base_max_size);
extern int sys_ifnames_extend(sys_ifnames *ifs, int additional_size);
extern void sys_ifnames_deinit(sys_ifnames *ifs);
int sys_ifnames_add(sys_ifnames *ifs, char *ifname);

extern int apstats_config_init(apstats_config_t *config);
int apstats_config_populate(apstats_config_t *config,
                            const apstats_level_t level,
                            const apstats_recursion_t recursion,
                            const char *ifname,
                            const char *stamacaddr);

#endif /* APSTATS_H */

