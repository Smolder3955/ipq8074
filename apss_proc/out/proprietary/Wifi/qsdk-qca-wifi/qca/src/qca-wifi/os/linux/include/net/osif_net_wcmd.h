/*
 * Copyright (c) 2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * Copyright (c) 2011,2016,2017 Qualcomm Innovation Center, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011,2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#ifndef __QDF_NET_WCMD_H
#define __QDF_NET_WCMD_H

#include <qdf_types.h>
#include <i_qdf_types.h>
#include "qdf_net_types.h"

/**
 *  * QDF_NET_CMD - These control/get info from the device
 *   */
#define QDF_NET_CMD(_x) \
    QDF_NET_CMD_GET_##_x, \
QDF_NET_CMD_SET_##_x

/**
 *  * qdf_net_cmd_t - Get/Set commands from anet to qdf_drv
 *   */
typedef enum {
    QDF_NET_CMD(LINK_INFO),
    QDF_NET_CMD(POLL_INFO),
    QDF_NET_CMD(CKSUM_INFO),
    QDF_NET_CMD(RING_INFO),
    QDF_NET_CMD(MAC_ADDR),
    QDF_NET_CMD(MTU),
    QDF_NET_CMD_GET_DMA_INFO,
    QDF_NET_CMD_GET_OFFLOAD_CAP,
    QDF_NET_CMD_GET_STATS,
    QDF_NET_CMD_ADD_VID,
    QDF_NET_CMD_DEL_VID,
    QDF_NET_CMD_SET_MCAST,
    QDF_NET_CMD_GET_MCAST_CAP
} qdf_net_cmd_t;

/**
 *  * @brief Command for setting ring paramters.
 *   */
typedef struct {
    uint32_t rx_bufsize;  /*Ro field. For shim's that maintain a pool*/
    uint32_t rx_ndesc;
    uint32_t tx_ndesc;
}qdf_net_cmd_ring_info_t;

/**
 *  * @brief Whether the interface is polled or not. If so, the polling bias (number of
 *   * packets it wants to process per invocation
 *    */
typedef struct {
    uint8_t   polled;
    uint32_t  poll_wt;
}qdf_net_cmd_poll_info_t;

/**
 *  * @brief DMA mask types.
 *   */
typedef enum {
    QDF_DMA_MASK_32BIT,
    QDF_DMA_MASK_64BIT,
}qdf_dma_mask_t;

typedef struct qdf_dma_info {
    qdf_dma_mask_t   dma_mask;
    uint32_t          sg_nsegs; /**< scatter segments */
}qdf_net_cmd_dma_info_t;


/**
 *  * @brief Defines the TX and RX checksumming capabilities/state of the device
 *   * The actual checksum handling happens on an qdf_nbuf
 *    * If offload capability command not supported, all offloads are assumed to be
 *     * none.
 *      */
typedef enum {
    QDF_NET_CKSUM_NONE,           /*Cannot do any checksum*/
    QDF_NET_CKSUM_TCP_UDP_IPv4,   /*tcp/udp on ipv4 with pseudo hdr*/
    QDF_NET_CKSUM_TCP_UDP_IPv6,   /*tcp/udp on ipv6*/
}qdf_net_cksum_type_t;

typedef struct {
    qdf_net_cksum_type_t tx_cksum;
    qdf_net_cksum_type_t rx_cksum;
}qdf_net_cksum_info_t;

typedef qdf_net_cksum_info_t qdf_net_cmd_cksum_info_t;    /*XXX needed?*/

/**
 *  * @brief Command for set/unset vid
 *   */
typedef uint16_t qdf_net_cmd_vid_t ;        /*get/set vlan id*/

/**
 *  * @brief Command for getting offloading capabilities of a device
 *   */
typedef struct {
    qdf_net_cksum_info_t cksum_cap;
    qdf_net_tso_type_t   tso;
    uint8_t         vlan_supported;
}qdf_net_cmd_offload_cap_t;

/**
 *  * @brief Command for getting general stats from a device
 *   */
typedef struct {
    uint32_t tx_packets;  /**< total packets transmitted*/
    uint32_t rx_packets;  /**< total packets recieved*/
    uint32_t tx_bytes;    /**< total bytes transmitted*/
    uint32_t rx_bytes;    /**< total bytes recieved*/
    uint32_t tx_dropped;  /**< total tx dropped because of lack of buffers*/
    uint32_t rx_dropped;  /**< total rx dropped because of lack of buffers*/
    uint32_t rx_errors;   /**< bad packet recieved*/
    uint32_t tx_errors;   /**< transmisison problems*/
}qdf_net_cmd_stats_t;



typedef enum qdf_net_cmd_mcast_cap{
    QDF_NET_MCAST_SUP=0,
    QDF_NET_MCAST_NOTSUP
}qdf_net_cmd_mcast_cap_t;

/**
 *  * @brief For polled devices, adf_drv responds with one of the following status in
 *   * its poll function.
 *    */
typedef enum {
    QDF_NET_POLL_DONE,
    QDF_NET_POLL_NOT_DONE,
    QDF_NET_POLL_OOM,
}qdf_net_poll_resp_t;


/**
 *  * @brief link info capability/parameters for the device
 *   * Note the flags below
 *    */
typedef struct {
    uint32_t  supported;   /*RO Features this if supports*/
    uint32_t  advertized;  /*Features this interface advertizes*/
    int16_t   speed;       /*Force speed 10M, 100M, gigE*/
    int8_t    duplex;      /*duplex full or half*/
    uint8_t   autoneg;     /*Enabled/disable autoneg*/
}qdf_net_cmd_link_info_t;


/**
 *  * qdf_net_devaddr_t - Command for getting general stats from a device
 *   * @num: No. of mcast addresses
 *    * @da_addr: Destination address
 *     */
typedef struct qdf_net_devaddr {
    uint32_t num;
    uint8_t  *da_addr[QDF_NET_MAX_MCAST_ADDR];
} qdf_net_devaddr_t;

typedef qdf_net_devaddr_t       qdf_net_cmd_mcaddr_t;
typedef qdf_net_ethaddr_t       qdf_net_cmd_macaddr_t;

typedef union {
    qdf_net_cmd_link_info_t     link_info;
    qdf_net_cmd_poll_info_t     poll_info;
    qdf_net_cmd_cksum_info_t    cksum_info;
    qdf_net_cmd_ring_info_t     ring_info;
    qdf_net_cmd_dma_info_t      dma_info;
    qdf_net_cmd_vid_t           vid;
    qdf_net_cmd_offload_cap_t   offload_cap;
    qdf_net_cmd_stats_t         stats;
    qdf_net_cmd_mcaddr_t        mcast_info;
    qdf_net_cmd_mcast_cap_t     mcast_cap;
    qdf_net_cmd_macaddr_t       mac_addr;
}qdf_net_cmd_data_t;


/**
 * Defines
 */
#define __QDF_OS_NAME_SIZE              IFNAMSIZ /*hack*/
#define QDF_NET_WCMD_NAME_SIZE          __QDF_OS_NAME_SIZE
#define QDF_NET_WCMD_NICK_NAME          32 /**< Max Device nick name size*/
#define QDF_NET_WCMD_MODE_NAME_LEN      6
#define QDF_NET_WCMD_IE_MAXLEN          256 /** Max Len for IE */

#define QDF_NET_WCMD_MAX_BITRATES       32
#define QDF_NET_WCMD_MAX_ENC_SZ         8
#define QDF_NET_WCMD_MAX_FREQ           32
#define QDF_NET_WCMD_MAX_TXPOWER        8
#define QDF_NET_WCMD_EVENT_CAP          6

/**
 * @brief key set/get info
 */
#define QDF_NET_WCMD_KEYBUF_SIZE        16
#define QDF_NET_WCMD_MICBUF_SIZE        16/**< space for tx+rx keys */
#define QDF_NET_WCMD_KEY_DEFAULT        0x80/**< default xmit key */
#define QDF_NET_WCMD_ADDR_LEN           6
#define QDF_NET_WCMD_KEYDATA_SZ          \
	(QDF_NET_WCMD_KEYBUF_SIZE + QDF_NET_WCMD_MICBUF_SIZE)
/**
 *  @brief key flags
 *  XXX: enum's
 */

#define QDF_NET_WCMD_MAX_SSID           32
#define QDF_NET_WCMD_CHAN_BYTES         32

/**
 *  @brief  Maximum number of address that you may get in the
 *          list of access ponts
 */
#define	QDF_NET_WCMD_MAX_AP		16

#define QDF_NET_WCMD_RATE_MAXSIZE       30
#define QDF_NET_WCMD_NUM_TR_ENTS        128
/**
 * @brief Ethtool specific
 */
#define QDF_NET_WCMD_BUSINFO_LEN        32
#define QDF_NET_WCMD_DRIVSIZ            32
#define QDF_NET_WCMD_VERSIZ             32
#define QDF_NET_WCMD_FIRMSIZ            32

/**
 * @brief flags for encoding (along with the token)
 */
#define IW_ENCODE_INDEX			0x00FF /* Token index */
#define IW_ENCODE_FLAGS			0xFF00 /* Flags deifned below */
#define IW_ENCODE_MODE			0xF000 /* Modes defined below */
#define IW_ENCODE_DISABLED		0x8000 /* Encoding disabled */
#define IW_ENCODE_ENABLED		0x0000 /* Encoding enabled */
#define IW_ENCODE_RESTRICTED		0x4000 /* Refuse nonencoded packets */
#define IW_ENCODE_OPEN			0x2000 /* Accept non encoded packets */
#define IW_ENCODE_NOKEY			0x0800 /* Key is write only,*/
						/* so not preset */
#define IW_ENCODE_TEMP			0x0400 /* Temporary key */

#define QDF_NET_WCMD_DBG_MAXLEN         24

/**
 * @brief Get/Set wireless commands
 */
typedef enum qdf_net_wcmd_type {
	/* net80211 */
	QDF_NET_WCMD_GET_RTS_THRES,
	QDF_NET_WCMD_SET_RTS_THRES,
	QDF_NET_WCMD_GET_FRAGMENT,
	QDF_NET_WCMD_SET_FRAGMENT,
	QDF_NET_WCMD_GET_VAPMODE,
	QDF_NET_WCMD_SET_VAPMODE,
	QDF_NET_WCMD_GET_BSSID,
	QDF_NET_WCMD_SET_BSSID,
	QDF_NET_WCMD_GET_NICKNAME,
	QDF_NET_WCMD_SET_NICKNAME,
	QDF_NET_WCMD_GET_FREQUENCY,
	QDF_NET_WCMD_SET_FREQUENCY,
	QDF_NET_WCMD_GET_ESSID,
	QDF_NET_WCMD_SET_ESSID,
	QDF_NET_WCMD_GET_TX_POWER,
	QDF_NET_WCMD_SET_TX_POWER,
	QDF_NET_WCMD_GET_PARAM,
	QDF_NET_WCMD_SET_PARAM,
	QDF_NET_WCMD_GET_OPT_IE,
	QDF_NET_WCMD_SET_OPT_IE,
	QDF_NET_WCMD_GET_APP_IE_BUF,
	QDF_NET_WCMD_SET_APP_IE_BUF,
	QDF_NET_WCMD_GET_ENC,
	QDF_NET_WCMD_SET_ENC,
	QDF_NET_WCMD_GET_KEY,
	QDF_NET_WCMD_SET_KEY,
	QDF_NET_WCMD_GET_SCAN,
	QDF_NET_WCMD_SET_SCAN,
	QDF_NET_WCMD_GET_MODE,
	QDF_NET_WCMD_SET_MODE,
	QDF_NET_WCMD_GET_CHAN_LIST,
	QDF_NET_WCMD_SET_CHAN_LIST,
	QDF_NET_WCMD_GET_WMM_PARAM,
	QDF_NET_WCMD_SET_WMM_PARAM,
	QDF_NET_WCMD_GET_VAPNAME,
	QDF_NET_WCMD_GET_IC_CAPS,
	QDF_NET_WCMD_GET_RETRIES,
	QDF_NET_WCMD_GET_WAP_LIST,
	QDF_NET_WCMD_GET_ADDBQDF_STATUS,
	QDF_NET_WCMD_GET_CHAN_INFO,
	QDF_NET_WCMD_GET_WPA_IE,
	QDF_NET_WCMD_GET_WSC_IE,
	QDF_NET_WCMD_SET_TXPOWER_LIMIT,
	QDF_NET_WCMD_SET_TURBO,
	QDF_NET_WCMD_SET_FILTER,
	QDF_NET_WCMD_SET_ADDBA_RESPONSE,
	QDF_NET_WCMD_SET_MLME,
	QDF_NET_WCMD_SET_SEND_ADDBA,
	QDF_NET_WCMD_SET_SEND_DELBA,
	QDF_NET_WCMD_SET_DELKEY,
	QDF_NET_WCMD_SET_DELMAC,
	QDF_NET_WCMD_SET_ADD_MAC,
	QDF_NET_WCMD_GET_RANGE,
	QDF_NET_WCMD_GET_POWER,
	QDF_NET_WCMD_SET_POWER,
	QDF_NET_WCMD_GET_DEVSTATS,
	QDF_NET_WCMD_SET_MTU,
	QDF_NET_WCMD_SET_SYSCTL,
	QDF_NET_WCMD_GET_STA_STATS,/* stats_sta */
	QDF_NET_WCMD_GET_VAP_STATS, /* stats_vap */
	QDF_NET_WCMD_GET_STATION_LIST, /* station */
	/* Device specific */
	QDF_NET_WCMD_SET_DEV_VAP_CREATE,
	QDF_NET_WCMD_SET_DEV_TX_TIMEOUT,        /* XXX:No data definition */
	QDF_NET_WCMD_SET_DEV_MODE_INIT,         /* XXX:No data definition */
	QDF_NET_WCMD_GET_DEV_STATUS,
	QDF_NET_WCMD_GET_DEV_STATUS_CLR,        /* XXX:No data definition */
	QDF_NET_WCMD_GET_DEV_DIALOG,
	QDF_NET_WCMD_GET_DEV_PHYERR,
	QDF_NET_WCMD_GET_DEV_CWM,
	QDF_NET_WCMD_GET_DEV_ETHTOOL,
	QDF_NET_WCMD_SET_DEV_MAC,
	QDF_NET_WCMD_SET_DEV_CAP,/*ATH_CAP*/
	/* Device write specific */
	QDF_NET_WCMD_SET_DEV_EIFS_MASK,
	QDF_NET_WCMD_SET_DEV_EIFS_DUR,
	QDF_NET_WCMD_SET_DEV_SLOTTIME,
	QDF_NET_WCMD_SET_DEV_ACKTIMEOUT,
	QDF_NET_WCMD_SET_DEV_CTSTIMEOUT,
	QDF_NET_WCMD_SET_DEV_SOFTLED,
	QDF_NET_WCMD_SET_DEV_LEDPIN,
	QDF_NET_WCMD_SET_DEV_DEBUG,
	QDF_NET_WCMD_SET_DEV_TXANTENNA,
	QDF_NET_WCMD_SET_DEV_RXANTENNA,
	QDF_NET_WCMD_SET_DEV_DIVERSITY,
	QDF_NET_WCMD_SET_DEV_TXINTRPERIOD,
	QDF_NET_WCMD_SET_DEV_FFTXQMIN,
	QDF_NET_WCMD_SET_DEV_TKIPMIC,
	QDF_NET_WCMD_SET_DEV_GLOBALTXTIMEOUT,
	QDF_NET_WCMD_SET_DEV_SW_WSC_BUTTON,
	/* Device read specific */
	QDF_NET_WCMD_GET_DEV_EIFS_MASK,
	QDF_NET_WCMD_GET_DEV_EIFS_DUR,
	QDF_NET_WCMD_GET_DEV_SLOTTIME,
	QDF_NET_WCMD_GET_DEV_ACKTIMEOUT,
	QDF_NET_WCMD_GET_DEV_CTSTIMEOUT,
	QDF_NET_WCMD_GET_DEV_SOFTLED,
	QDF_NET_WCMD_GET_DEV_LEDPIN,
	QDF_NET_WCMD_GET_DEV_COUNTRYCODE,
	QDF_NET_WCMD_GET_DEV_REGDOMAIN,
	QDF_NET_WCMD_GET_DEV_DEBUG,
	QDF_NET_WCMD_GET_DEV_TXANTENNA,
	QDF_NET_WCMD_GET_DEV_RXANTENNA,
	QDF_NET_WCMD_GET_DEV_DIVERSITY,
	QDF_NET_WCMD_GET_DEV_TXINTRPERIOD,
	QDF_NET_WCMD_GET_DEV_FFTXQMIN,
	QDF_NET_WCMD_GET_DEV_TKIPMIC,
	QDF_NET_WCMD_GET_DEV_GLOBALTXTIMEOUT,
	QDF_NET_WCMD_GET_DEV_SW_WSC_BUTTON,
	/* For debugging purpose*/
	QDF_NET_WCMD_SET_DBG_INFO,
	QDF_NET_WCMD_GET_DBG_INFO
} qdf_net_wcmd_type_t;
/**
 * @brief Opmodes for the VAP
 */
typedef enum qdf_net_wcmd_opmode {
	QDF_NET_WCMD_OPMODE_IBSS,/**< IBSS (adhoc) station */
	QDF_NET_WCMD_OPMODE_STA,/**< Infrastructure station */
	QDF_NET_WCMD_OPMODE_WDS,/**< WDS link */
	QDF_NET_WCMD_OPMODE_AHDEMO,/**< Old lucent compatible adhoc demo */
	QDF_NET_WCMD_OPMODE_RESERVE0,/**<XXX: future use*/
	QDF_NET_WCMD_OPMODE_RESERVE1,/**<XXX: future use*/
	QDF_NET_WCMD_OPMODE_HOSTAP,/**< Software Access Point*/
	QDF_NET_WCMD_OPMODE_RESERVE2,/**<XXX: future use*/
	QDF_NET_WCMD_OPMODE_MONITOR/**< Monitor mode*/
} qdf_net_wcmd_opmode_t;

/*hack*/
typedef enum qdf_net_wcmd_vapmode{
    QDF_NET_WCMD_VAPMODE_AUTO,   /**< Driver default*/
    QDF_NET_WCMD_VAPMODE_ADHOC,  /**< Single cell*/
    QDF_NET_WCMD_VAPMODE_INFRA,  /**< Multi Cell or Roaming*/
    QDF_NET_WCMD_VAPMODE_MASTER, /**< Access Point*/
    QDF_NET_WCMD_VAPMODE_REPEAT, /**< Wireless Repeater*/
    QDF_NET_WCMD_VAPMODE_SECOND, /**< Secondary master or repeater*/
    QDF_NET_WCMD_VAPMODE_MONITOR /**< Passive Monitor*/
}qdf_net_wcmd_vapmode_t;
/**
 * brief PHY modes for VAP
 */
typedef enum qdf_net_wcmd_phymode {
	QDF_NET_WCMD_PHYMODE_AUTO = 0,/**< autoselect */
	QDF_NET_WCMD_PHYMODE_11A = 1,/**< 5GHz, OFDM */
	QDF_NET_WCMD_PHYMODE_11B = 2,/**< 2GHz, CCK */
	QDF_NET_WCMD_PHYMODE_11G = 3,/**< 2GHz, OFDM */
	QDF_NET_WCMD_PHYMODE_FH = 4,/**< 2GHz, GFSK */
	QDF_NET_WCMD_PHYMODE_TURBO_A = 5,/**< 5GHz, OFDM, 2 x clk dynamic turbo */
	QDF_NET_WCMD_PHYMODE_TURBO_G = 6,/**< 2GHz, OFDM, 2 x clk dynamic turbo*/
	QDF_NET_WCMD_PHYMODE_11NA = 7,/**< 5GHz, OFDM + MIMO*/
	QDF_NET_WCMD_PHYMODE_11NG = 8,/**< 2GHz, OFDM + MIMO*/
	QDF_NET_WCMD_PHYMODE_TURBO_STATIC_A = 9,/**< Turbo Static A*/
} qdf_net_wcmd_phymode_t;
/**
 * @brief param id
 */
typedef enum qdf_net_wcmd_param_id {
	QDF_NET_WCMD_PARAM_TURBO = 1,/**<turbo mode */
	QDF_NET_WCMD_PARAM_MODE,/**< phy mode (11a, 11b, etc.) */
	QDF_NET_WCMD_PARAM_AUTHMODE,/**< authentication mode */
	QDF_NET_WCMD_PARAM_PROTMODE,/**< 802.11g protection */
	QDF_NET_WCMD_PARAM_MCASTCIPHER,/**< multicast/default cipher */
	QDF_NET_WCMD_PARAM_MCASTKEYLEN,/**< multicast key length */
	QDF_NET_WCMD_PARAM_UCASTCIPHERS,/**< unicast cipher suites */
	QDF_NET_WCMD_PARAM_UCASTCIPHER,/**< unicast cipher */
	QDF_NET_WCMD_PARAM_UCASTKEYLEN,/**< unicast key length */
	QDF_NET_WCMD_PARAM_WPA,/**< WPA mode (0,1,2) */
	QDF_NET_WCMD_PARAM_ROAMING,/**< roaming mode */
	QDF_NET_WCMD_PARAM_PRIVACY,/**< privacy invoked */
	QDF_NET_WCMD_PARAM_COUNTERMEASURES,/**< WPA/TKIP countermeasures */
	QDF_NET_WCMD_PARAM_DROPUNENCRYPTED,/**< discard unencrypted frames */
	QDF_NET_WCMD_PARAM_DRIVER_CAPS,/**< driver capabilities */
	QDF_NET_WCMD_PARAM_MACCMD,/**< MAC ACL operation */
	QDF_NET_WCMD_PARAM_WMM,/**< WMM mode (on, off) */
	QDF_NET_WCMD_PARAM_HIDESSID,/**< hide SSID mode (on, off) */
	QDF_NET_WCMD_PARAM_APBRIDGE,/**< AP inter-sta bridging */
	QDF_NET_WCMD_PARAM_KEYMGTALGS,/**< key management algorithms */
	QDF_NET_WCMD_PARAM_RSNCAPS,/**< RSN capabilities */
	QDF_NET_WCMD_PARAM_INACT,/**< station inactivity timeout */
	QDF_NET_WCMD_PARAM_INACT_AUTH,/**< station auth inact timeout */
	QDF_NET_WCMD_PARAM_INACT_INIT,/**< station init inact timeout */
	QDF_NET_WCMD_PARAM_ABOLT,/**< Atheros Adv. Capabilities */
	QDF_NET_WCMD_PARAM_DTIM_PERIOD,/**< DTIM period (beacons) */
	QDF_NET_WCMD_PARAM_BEACON_INTERVAL,/**< beacon interval (ms) */
	QDF_NET_WCMD_PARAM_DOTH,/**< 11.h is on/off */
	QDF_NET_WCMD_PARAM_PWRTARGET,/**< Current Channel Pwr Constraint */
	QDF_NET_WCMD_PARAM_GENREASSOC,/**< Generate a reassociation request */
	QDF_NET_WCMD_PARAM_COMPRESSION,/**< compression */
	QDF_NET_WCMD_PARAM_FF,/**< fast frames support  */
	QDF_NET_WCMD_PARAM_XR,/**< XR support */
	QDF_NET_WCMD_PARAM_BURST,/**< burst mode */
	QDF_NET_WCMD_PARAM_PUREG,/**< pure 11g (no 11b stations) */
	QDF_NET_WCMD_PARAM_AR,/**< AR support */
	QDF_NET_WCMD_PARAM_WDS,/**< Enable 4 address processing */
	QDF_NET_WCMD_PARAM_BGSCAN,/**< bg scanning (on, off) */
	QDF_NET_WCMD_PARAM_BGSCAN_IDLE,/**< bg scan idle threshold */
	QDF_NET_WCMD_PARAM_BGSCAN_INTERVAL,/**< bg scan interval */
	QDF_NET_WCMD_PARAM_MCAST_RATE,/**< Multicast Tx Rate */
	QDF_NET_WCMD_PARAM_COVERAGE_CLASS,/**< coverage class */
	QDF_NET_WCMD_PARAM_COUNTRY_IE,/**< enable country IE */
	QDF_NET_WCMD_PARAM_SCANVALID,/**< scan cache valid threshold */
	QDF_NET_WCMD_PARAM_ROAM_RSSI_11A,/**< rssi threshold in 11a */
	QDF_NET_WCMD_PARAM_ROAM_RSSI_11B,/**< rssi threshold in 11b */
	QDF_NET_WCMD_PARAM_ROAM_RSSI_11G,/**< rssi threshold in 11g */
	QDF_NET_WCMD_PARAM_ROAM_RATE_11A,/**< tx rate threshold in 11a */
	QDF_NET_WCMD_PARAM_ROAM_RATE_11B,/**< tx rate threshold in 11b */
	QDF_NET_WCMD_PARAM_ROAM_RATE_11G,/**< tx rate threshold in 11g */
	QDF_NET_WCMD_PARAM_UAPSDINFO,/**< value for qos info field */
	QDF_NET_WCMD_PARAM_SLEEP,/**< force sleep/wake */
	QDF_NET_WCMD_PARAM_QOSNULL,/**< force sleep/wake */
	QDF_NET_WCMD_PARAM_PSPOLL,/**< force ps-poll generation (sta only) */
	QDF_NET_WCMD_PARAM_EOSPDROP,/**< force uapsd EOSP drop (ap only) */
	QDF_NET_WCMD_PARAM_MARKDFS,/**< mark a dfs interference channel*/
	QDF_NET_WCMD_PARAM_REGCLASS,/**< enable regclass ids in country IE */
	QDF_NET_WCMD_PARAM_CHANBW,/**< set chan bandwidth preference */
	QDF_NET_WCMD_PARAM_WMM_AGGRMODE,/**< set WMM Aggressive Mode */
	QDF_NET_WCMD_PARAM_SHORTPREAMBLE,/**< enable/disable short Preamble */
	QDF_NET_WCMD_PARAM_BLOCKDFSCHAN,/**< enable/disable use of DFS channels */
	QDF_NET_WCMD_PARAM_CWM_MODE,/**< CWM mode */
	QDF_NET_WCMD_PARAM_CWM_EXTOFFSET,/**< Ext. channel offset */
	QDF_NET_WCMD_PARAM_CWM_EXTPROTMODE,/**< Ext. Chan Protection mode */
	QDF_NET_WCMD_PARAM_CWM_EXTPROTSPACING,/**< Ext. chan Protection spacing */
	QDF_NET_WCMD_PARAM_CWM_ENABLE,/**< State machine enabled */
	QDF_NET_WCMD_PARAM_CWM_EXTBUSYTHRESHOLD,/**< Ext. chan busy threshold*/
	QDF_NET_WCMD_PARAM_CWM_CHWIDTH,/**< Current channel width */
	QDF_NET_WCMD_PARAM_SHORT_GI,/**< half GI */
	QDF_NET_WCMD_PARAM_FAST_CC,/**< fast channel change */
	/**
	 * 11n A-MPDU, A-MSDU support
	 */
	QDF_NET_WCMD_PARAM_AMPDU,/**< 11n a-mpdu support */
	QDF_NET_WCMD_PARAM_AMPDU_LIMIT,/**< a-mpdu length limit */
	QDF_NET_WCMD_PARAM_AMPDU_DENSITY,/**< a-mpdu density */
	QDF_NET_WCMD_PARAM_AMPDU_SUBFRAMES,/**< a-mpdu subframe limit */
	QDF_NET_WCMD_PARAM_AMSDU,/**< a-msdu support */
	QDF_NET_WCMD_PARAM_AMSDU_LIMIT,/**< a-msdu length limit */
	QDF_NET_WCMD_PARAM_COUNTRYCODE,/**< Get country code */
	QDF_NET_WCMD_PARAM_TX_CHAINMASK,/**< Tx chain mask */
	QDF_NET_WCMD_PARAM_RX_CHAINMASK,/**< Rx chain mask */
	QDF_NET_WCMD_PARAM_RTSCTS_RATECODE,/**< RTS Rate code */
	QDF_NET_WCMD_PARAM_HT_PROTECTION,/**< Protect traffic in HT mode */
	QDF_NET_WCMD_PARAM_RESET_ONCE,/**< Force a reset */
	QDF_NET_WCMD_PARAM_SETADDBAOPER,/**< Set ADDBA mode */
	QDF_NET_WCMD_PARAM_TX_CHAINMASK_LEGACY,/**< Tx chain mask */
	QDF_NET_WCMD_PARAM_11N_RATE,/**< Set ADDBA mode */
	QDF_NET_WCMD_PARAM_11N_RETRIES,/**< Tx chain mask for legacy clients */
	QDF_NET_WCMD_PARAM_WDS_AUTODETECT,/**< Autodetect/DelBa for WDS mode */
	QDF_NET_WCMD_PARAM_RB,/**< Switch in/out of RB */
	/**
	 * RB Detection knobs.
	 */
	QDF_NET_WCMD_PARAM_RB_DETECT,/**< Do RB detection */
	QDF_NET_WCMD_PARAM_RB_SKIP_THRESHOLD,/**< seqno-skip-by-1s to detect */
	QDF_NET_WCMD_PARAM_RB_TIMEOUT,/**< (in ms) to restore non-RB */
	QDF_NET_WCMD_PARAM_NO_HTIE,/**< Control HT IEs are sent out or parsed */
	QDF_NET_WCMD_PARAM_MAXSTA/**< Config max allowable staions for each VAP */
} qdf_net_wcmd_param_id_t;

/**
 *  @brief APPIEBUF related definations
 */
typedef enum qdf_net_wcmd_appie_frame {
	QDF_NET_WCMD_APPIE_FRAME_BEACON,
	QDF_NET_WCMD_APPIE_FRAME_PROBE_REQ,
	QDF_NET_WCMD_APPIE_FRAME_PROBE_RESP,
	QDF_NET_WCMD_APPIE_FRAME_ASSOC_REQ,
	QDF_NET_WCMD_APPIE_FRAME_ASSOC_RESP,
	QDF_NET_WCMD_APPIE_NUM_OF_FRAME
} qdf_net_wcmd_appie_frame_t;
/**
 * @brief filter pointer info
 */
typedef enum qdf_net_wcmd_filter_type {
	QDF_NET_WCMD_FILTER_TYPE_BEACON = 0x1,
	QDF_NET_WCMD_FILTER_TYPE_PROBE_REQ = 0x2,
	QDF_NET_WCMD_FILTER_TYPE_PROBE_RESP = 0x4,
	QDF_NET_WCMD_FILTER_TYPE_ASSOC_REQ = 0x8,
	QDF_NET_WCMD_FILTER_TYPE_ASSOC_RESP = 0x10,
	QDF_NET_WCMD_FILTER_TYPE_AUTH = 0x20,
	QDF_NET_WCMD_FILTER_TYPE_DEAUTH = 0x40,
	QDF_NET_WCMD_FILTER_TYPE_DISASSOC = 0x80,
	QDF_NET_WCMD_FILTER_TYPE_ALL = 0xFF,
} qdf_net_wcmd_filter_type_t;
/**
 * @brief mlme info pointer
 */
typedef enum qdf_net_wcmd_mlme_op_type {
	QDF_NET_WCMD_MLME_ASSOC,
	QDF_NET_WCMD_MLME_DISASSOC,
	QDF_NET_WCMD_MLME_DEAUTH,
	QDF_NET_WCMD_MLME_AUTHORIZE,
	QDF_NET_WCMD_MLME_UNAUTHORIZE,
} qdf_net_wcmd_mlme_op_type_t;

typedef enum qdf_net_wcmd_wmmparams {
	QDF_NET_WCMD_WMMPARAMS_CWMIN = 1,
	QDF_NET_WCMD_WMMPARAMS_CWMAX,
	QDF_NET_WCMD_WMMPARAMS_AIFS,
	QDF_NET_WCMD_WMMPARAMS_TXOPLIMIT,
	QDF_NET_WCMD_WMMPARAMS_ACM,
	QDF_NET_WCMD_WMMPARAMS_NOACKPOLICY,
} qdf_net_wcmd_wmmparams_t;

/**
 * @brief Power Management Flags
 */
typedef enum qdf_net_wcmd_pmflags {
	QDF_NET_WCMD_POWER_ON  = 0x0,
	QDF_NET_WCMD_POWER_MIN = 0x1,/**< Min */
	QDF_NET_WCMD_POWER_MAX = 0x2,/**< Max */
	QDF_NET_WCMD_POWER_REL = 0x4,/**< Not in seconds/ms/us */
	QDF_NET_WCMD_POWER_MOD = 0xF,/**< Modify a parameter */
	QDF_NET_WCMD_POWER_UCAST_R = 0x100,/**< Ucast messages */
	QDF_NET_WCMD_POWER_MCAST_R = 0x200,/**< Mcast messages */
	QDF_NET_WCMD_POWER_ALL_R = 0x300,/**< All messages though PM */
	QDF_NET_WCMD_POWER_FORCE_S = 0x400,/**< Force PM to unicast */
	QDF_NET_WCMD_POWER_REPEATER = 0x800,/**< Bcast messages in PM*/
	QDF_NET_WCMD_POWER_MODE = 0xF00,/**< Power Management mode */
	QDF_NET_WCMD_POWER_PERIOD = 0x1000,/**< Period/Duration of */
	QDF_NET_WCMD_POWER_TIMEOUT = 0x2000,/**< Timeout (to go asleep) */
	QDF_NET_WCMD_POWER_TYPE = 0xF000/**< Type of parameter */
} qdf_net_wcmd_pmflags_t;
/**
 * @brief Tx Power flags
 */
typedef enum qdf_net_wcmd_txpow_flags {
	QDF_NET_WCMD_TXPOW_DBM = 0,/**< dBm */
	QDF_NET_WCMD_TXPOW_MWATT = 0x1,/**< mW */
	QDF_NET_WCMD_TXPOW_RELATIVE = 0x2,/**< Arbitrary units */
	QDF_NET_WCMD_TXPOW_TYPE = 0xFF,/**< Type of value */
	QDF_NET_WCMD_TXPOW_RANGE = 0x1000/**< Range (min - max) */
} qdf_net_wcmd_txpow_flags_t;
/**
 * @brief Retry flags
 */
typedef enum qdf_net_wcmd_retry_flags {
	QDF_NET_WCMD_RETRY_ON = 0x0,
	QDF_NET_WCMD_RETRY_MIN = 0x1,/**< Value is a minimum */
	QDF_NET_WCMD_RETRY_MAX = 0x2,/**< Maximum */
	QDF_NET_WCMD_RETRY_RELATIVE = 0x4,/**< Not in seconds/ms/us */
	QDF_NET_WCMD_RETRY_SHORT = 0x10,/**< Short packets  */
	QDF_NET_WCMD_RETRY_LONG = 0x20,/**< Long packets */
	QDF_NET_WCMD_RETRY_MODIFIER = 0xFF,/**< Modify a parameter */
	QDF_NET_WCMD_RETRY_LIMIT = 0x1000,/**< Max retries*/
	QDF_NET_WCMD_RETRY_LIFETIME = 0x2000,/**< Max retries us*/
	QDF_NET_WCMD_RETRY_TYPE = 0xF000,/**< Parameter type */
} qdf_net_wcmd_retry_flags_t;

/**
 * @brief CWM Debug mode commands
 */
typedef enum qdf_net_wcmd_cwm_cmd {
	QDF_NET_WCMD_CWM_CMD_EVENT,/**< Send Event */
	QDF_NET_WCMD_CWM_CMD_CTL,/**< Ctrl channel busy */
	QDF_NET_WCMD_CWM_CMD_EXT,/**< Ext chan busy */
	QDF_NET_WCMD_CWM_CMD_VCTL,/**< virt ctrl chan busy*/
	QDF_NET_DBGCWM_CMD_VEXT/**< virt extension channel busy*/
} qdf_net_wcmd_cwm_cmd_t;

/**
 * @brief CWM EVENTS
 */
typedef enum qdf_net_wcmd_cwm_event {
	QDF_NET_WCMD_CWMEVENT_TXTIMEOUT,  /**< tx timeout interrupt */
	QDF_NET_WCMD_CWMEVENT_EXTCHCLEAR, /**< ext channel sensing clear */
	QDF_NET_WCMD_CWMEVENT_EXTCHBUSY,  /**< ext channel sensing busy */
	QDF_NET_WCMD_CWMEVENT_EXTCHSTOP,  /**< ext channel sensing stop */
	QDF_NET_WCMD_CWMEVENT_EXTCHRESUME,/**< ext channel sensing resume */
	QDF_NET_WCMD_CWMEVENT_DESTCW20,   /**< dest channel width changed to 20 */
	QDF_NET_WCMD_CWMEVENT_DESTCW40,   /**< dest channel width changed to 40 */
	QDF_NET_WCMD_CWMEVENT_MAX
} qdf_net_wcmd_cwm_event_t;

/**
 * @brief eth tool info
 */
typedef enum qdf_net_wcmd_ethtool_cmd {
	QDF_NET_WCMD_ETHTOOL_GSET = 0x1,/**< Get settings. */
	QDF_NET_WCMD_ETHTOOL_SSET,/**< Set settings. */
	QDF_NET_WCMD_ETHTOOL_GDRVINFO,/**< Get driver info. */
	QDF_NET_WCMD_ETHTOOL_GREGS,/**< Get NIC registers. */
	QDF_NET_WCMD_ETHTOOL_GWOL,/**< Wake-on-lan options. */
	QDF_NET_WCMD_ETHTOOL_SWOL,/**< Set wake-on-lan options. */
	QDF_NET_WCMD_ETHTOOL_GMSGLVL,/**< Get driver message level */
	QDF_NET_WCMD_ETHTOOL_SMSGLVL,/**< Set driver msg level */
	QDF_NET_WCMD_ETHTOOL_NWAY_RST,/**< Restart autonegotiation. */
	QDF_NET_WCMD_ETHTOOL_GEEPROM,/**< Get EEPROM data */
	QDF_NET_WCMD_ETHTOOL_SEEPROM,/** < Set EEPROM data. */
	QDF_NET_WCMD_ETHTOOL_GCOALESCE,/** < Get coalesce config */
	QDF_NET_WCMD_ETHTOOL_SCOALESCE,/** < Set coalesce config. */
	QDF_NET_WCMD_ETHTOOL_GRINGPARAM,/** < Get ring parameters */
	QDF_NET_WCMD_ETHTOOL_SRINGPARAM,/** < Set ring parameters. */
	QDF_NET_WCMD_ETHTOOL_GPAUSEPARAM,/** < Get pause parameters */
	QDF_NET_WCMD_ETHTOOL_SPAUSEPARAM,/** < Set pause parameters. */
	QDF_NET_WCMD_ETHTOOL_GRXCSUM,/** < Get RX hw csum enable */
	QDF_NET_WCMD_ETHTOOL_SRXCSUM,/** < Set RX hw csum enable */
	QDF_NET_WCMD_ETHTOOL_GTXCSUM,/** < Get TX hw csum enable */
	QDF_NET_WCMD_ETHTOOL_STXCSUM,/** < Set TX hw csum enable */
	QDF_NET_WCMD_ETHTOOL_GSG,/** < Get scatter-gather enable */
	QDF_NET_WCMD_ETHTOOL_SSG,/** < Set scatter-gather enable */
	QDF_NET_WCMD_ETHTOOL_TEST,/** < execute NIC self-test. */
	QDF_NET_WCMD_ETHTOOL_GSTRINGS,/** < get specified string set */
	QDF_NET_WCMD_ETHTOOL_PHYS_ID,/** < identify the NIC */
	QDF_NET_WCMD_ETHTOOL_GSTATS,/** < get NIC-specific statistics */
	QDF_NET_WCMD_ETHTOOL_GTSO,/** < Get TSO enable (ethtool_value) */
	QDF_NET_WCMD_ETHTOOL_STSO,/** < Set TSO enable (ethtool_value) */
	QDF_NET_WCMD_ETHTOOL_GPERMADDR,/** < Get permanent hardware address */
	QDF_NET_WCMD_ETHTOOL_GUFO,/** < Get UFO enable */
	QDF_NET_WCMD_ETHTOOL_SUFO,/** < Set UFO enable */
	QDF_NET_WCMD_ETHTOOL_GGSO,/** < Get GSO enable */
	QDF_NET_WCMD_ETHTOOL_SGSO,/** < Set GSO enable */
} qdf_net_wcmd_ethtool_cmd_t;

typedef enum qdf_net_wcmd_dbg_type {
	QDF_NET_WCMD_DBG_TRIGGER = 1,
	QDF_NET_WCMD_DBG_CAP_PKT = 2,
	QDF_NET_WCMD_DBG_OTHER   = 3,
	QDF_NET_WCMD_DBG_MAX
} qdf_net_wcmd_dbg_type_t;

/*
 * Reason codes
 *
 * Unlisted codes are reserved
 */

enum {
	QDF_NET_WCMD_REASON_UNSPECIFIED         = 1,
	QDF_NET_WCMD_REASON_AUTH_EXPIRE         = 2,
	QDF_NET_WCMD_REASON_AUTH_LEAVE          = 3,
	QDF_NET_WCMD_REASON_ASSOC_EXPIRE        = 4,
	QDF_NET_WCMD_REASON_ASSOC_TOOMANY       = 5,
	QDF_NET_WCMD_REASON_NOT_AUTHED          = 6,
	QDF_NET_WCMD_REASON_NOT_ASSOCED         = 7,
	QDF_NET_WCMD_REASON_ASSOC_LEAVE         = 8,
	QDF_NET_WCMD_REASON_ASSOC_NOT_AUTHED    = 9,
	QDF_NET_WCMD_REASON_RSN_REQUIRED        = 11,
	QDF_NET_WCMD_REASON_RSN_INCONSISTENT    = 12,
	QDF_NET_WCMD_REASON_IE_INVALID          = 13,
	QDF_NET_WCMD_REASON_MIC_FAILURE         = 14,

	QDF_NET_WCMD_STATUS_SUCCESS             = 0,
	QDF_NET_WCMD_STATUS_UNSPECIFIED         = 1,
	QDF_NET_WCMD_STATUS_CAPINFO             = 10,
	QDF_NET_WCMD_STATUS_NOT_ASSOCED         = 11,
	QDF_NET_WCMD_STATUS_OTHER               = 12,
	QDF_NET_WCMD_STATUS_ALG                 = 13,
	QDF_NET_WCMD_STATUS_SEQUENCE            = 14,
	QDF_NET_WCMD_STATUS_CHALLENGE           = 15,
	QDF_NET_WCMD_STATUS_TIMEOUT             = 16,
	QDF_NET_WCMD_STATUS_TOOMANY             = 17,
	QDF_NET_WCMD_STATUS_BASIC_RATE          = 18,
	QDF_NET_WCMD_STATUS_SP_REQUIRED         = 19,
	QDF_NET_WCMD_STATUS_PBCC_REQUIRED       = 20,
	QDF_NET_WCMD_STATUS_CA_REQUIRED         = 21,
	QDF_NET_WCMD_STATUS_TOO_MANY_STATIONS   = 22,
	QDF_NET_WCMD_STATUS_RATES               = 23,
	QDF_NET_WCMD_STATUS_SHORTSLOT_REQUIRED  = 25,
	QDF_NET_WCMD_STATUS_DSSSOFDM_REQUIRED   = 26,
	QDF_NET_WCMD_STATUS_REFUSED             = 37,
	QDF_NET_WCMD_STATUS_INVALID_PARAM       = 38,
};


/**
 * ***************************Structures***********************
 */

/**
 * @brief Information Element
 */
typedef struct qdf_net_ie_info {
	uint16_t         len;
	uint8_t          data[QDF_NET_WCMD_IE_MAXLEN];
} qdf_net_ie_info_t;

/**
 * @brief WCMD info
 */
typedef struct qdf_net_wcmd_vapname {
	uint32_t     len;
	uint8_t      name[QDF_NET_WCMD_NAME_SIZE];
} qdf_net_wcmd_vapname_t;

/**
 * @brief nickname pointer info
 */
typedef struct qdf_net_wcmd_nickname {
	uint16_t      len;
	uint8_t       name[QDF_NET_WCMD_NICK_NAME];
} qdf_net_wcmd_nickname_t;

/**
 * @brief missed frame info
 */
typedef struct  qdf_net_wcmd_miss {
	uint32_t      beacon;/**< Others cases */
} qdf_net_wcmd_miss_t;

/**
 * @brief  discarded frame info
 */
typedef struct  qdf_net_wcmd_discard {
	uint32_t      nwid;/**< Rx : Wrong nwid/essid */
	uint32_t      code; /**< Rx : Unable to code/decode (WEP) */
	uint32_t      fragment;/**< Rx : Can't perform MAC reassembly */
	uint32_t      retries;/**< Tx : Max MAC retries num reached */
	uint32_t      misc;/**< Others cases */
} qdf_net_wcmd_discard_t;

/**
 * @brief Link quality info
 */
typedef struct  qdf_net_wcmd_linkqty {
	uint8_t       qual;/*link quality(retries, SNR, missed beacons)*/
	uint8_t       level;/*Signal level (dBm) */
	uint8_t       noise;/*Noise level (dBm) */
	uint8_t       updated;/*Update flag*/
} qdf_net_wcmd_linkqty_t;

/**
 * @brief frequency info
 */
typedef struct  qdf_net_wcmd_freq {
	int32_t       m;/*Mantissa */
	int16_t       e;/*Exponent */
	uint8_t       i;/*List index (when in range struct) */
	uint8_t       flags;/*Flags (fixed/auto) */

} qdf_net_wcmd_freq_t;
/**
 * @brief VAP parameter range info
 */
typedef struct qdf_net_wcmd_vapparam_range {
	/**
	 * @brief Informative stuff (to choose between different
	 * interface) In theory this value should be the maximum
	 * benchmarked TCP/IP throughput, because with most of these
	 * devices the bit rate is meaningless (overhead an co) to
	 * estimate how fast the connection will go and pick the fastest
	 * one. I suggest people to play with Netperf or any
	 * benchmark...
	 */
	uint32_t           throughput;/**< To give an idea... */

	/** @brief NWID (or domain id) */
	uint32_t           min_nwid;/**< Min NWID to set */
	uint32_t           max_nwid;/**< Max NWID to set */

	/**
	 * @brief Old Frequency (backward compatibility - moved lower )
	 */
	uint16_t           old_num_channels;
	uint8_t            old_num_frequency;
	 /**@brief Wireless event capability bitmasks */
	uint32_t          event_capa[QDF_NET_WCMD_EVENT_CAP];
	 /**@brief Signal level threshold range */
	int32_t           sensitivity;

	/**
	 * @brief Quality of link & SNR stuff Quality range (link,
	 * level, noise) If the quality is absolute, it will be in the
	 * range [0
	 * - max_qual], if the quality is dBm, it will be in the range
	 * [max_qual - 0]. Don't forget that we use 8 bit arithmetics...
	 */
	qdf_net_wcmd_linkqty_t       max_qual;/**< Link Quality*/

	/**
	 * @brief This should contain the average/typical values of the
	 * quality indicator. This should be the threshold between a
	 * "good" and a "bad" link (example : monitor going from green
	 * to orange). Currently, user space apps like quality monitors
	 * don't have any way to calibrate the measurement. With this,
	 * they can split the range between 0 and max_qual in different
	 * quality level (using a geometric subdivision centered on the
	 * average). I expect that people doing the user space apps will
	 * feedback us on which value we need to put in each
	 * driver...
	 */
	qdf_net_wcmd_linkqty_t       avg_qual;

	/**@brief Rates */
	uint8_t           num_bitrates; /**< Number of entries in the list */
	int32_t           bitrate[QDF_NET_WCMD_MAX_BITRATES]; /**< in bps */

	/**@brief RTS threshold */
	int32_t           min_rts; /**< Minimal RTS threshold */
	int32_t           max_rts; /**< Maximal RTS threshold */
	 /**@brief Frag threshold */
	int32_t           min_frag;/**< Minimal frag threshold */
	int32_t           max_frag;/**< Maximal frag threshold */

	/**@brief Power Management duration & timeout */
	int32_t           min_pmp;/**< Minimal PM period */
	int32_t           max_pmp;/**< Maximal PM period */
	int32_t           min_pmt;/**< Minimal PM timeout */
	int32_t           max_pmt;/**< Maximal PM timeout */
	uint16_t          pmp_flags;/**< decode max/min PM period */
	uint16_t          pmt_flags;/**< decode max/min PM timeout */
	uint16_t          pm_capa;/**< PM options supported */

	/**@brief Encoder stuff, Different token sizes */
	uint16_t          enc_sz[QDF_NET_WCMD_MAX_ENC_SZ];
	uint8_t           num_enc_sz; /**< Number of entry in the list */
	uint8_t           max_enc_tk;/**< Max number of tokens */
	/**@brief For drivers that need a "login/passwd" form */
	uint8_t           enc_login_idx;/**< token index for login token */

	/**@brief Transmit power */
	uint16_t          txpower_capa;/**< options supported */
	uint8_t           num_txpower;/**< Number of entries in the list */
	int32_t           txpower[QDF_NET_WCMD_MAX_TXPOWER];/**< in bps */

	/**@brief Wireless Extension version info */
	uint8_t           we_version_compiled;/**< Must be WIRELESS_EXT */
	uint8_t           we_version_source;/**< Last update of source */

	/**@brief Retry limits and lifetime */
	uint16_t          retry_capa;/**< retry options supported */
	uint16_t          retry_flags;/**< decode max/min retry limit*/
	uint16_t          r_time_flags;/**< Decode max/min retry life */
	int32_t           min_retry;/**< Min retries */
	int32_t           max_retry;/**< Max retries */
	int32_t           min_r_time;/**< Min retry lifetime */
	int32_t           max_r_time;/**< Max retry lifetime */

	/**@brief Frequency */
	uint16_t          num_channels;/**< Num channels [0 - (num - 1)] */
	uint8_t           num_frequency;/**< Num entries*/
	/**
	 * Note : this frequency list doesn't need to fit channel
	 * numbers, because each entry contain its channel index
	 */
	qdf_net_wcmd_freq_t    freq[QDF_NET_WCMD_MAX_FREQ];

	uint32_t          enc_capa; /**< IW_ENC_CAPA_* bit field */
} qdf_net_wcmd_vapparam_range_t;

/**
 * @brief key info
 */
typedef struct qdf_net_wcmd_keyinfo {
	uint8_t   ik_type; /**< key/cipher type */
	uint8_t   ik_pad;
	uint16_t  ik_keyix;/**< key index */
	uint8_t   ik_keylen;/**< key length in bytes */
	uint32_t   ik_flags;
	uint8_t   ik_macaddr[QDF_NET_WCMD_ADDR_LEN];
	uint64_t  ik_keyrsc;/**< key receive sequence counter */
	uint64_t  ik_keytsc;/**< key transmit sequence counter */
	uint8_t   ik_keydata[QDF_NET_WCMD_KEYDATA_SZ];
} qdf_net_wcmd_keyinfo_t;

/**
 * @brief bssid pointer info
 */
typedef struct qdf_net_wcmd_bssid {
	uint8_t      bssid[QDF_NET_WCMD_ADDR_LEN];
} qdf_net_wcmd_bssid_t;

/**
 * @brief essid structure info
 */
typedef struct  qdf_net_wcmd_ssid {
	uint8_t     byte[QDF_NET_WCMD_MAX_SSID];
	uint16_t    len;/**< number of fields or size in bytes */
	uint16_t    flags;/**< Optional  */
} qdf_net_wcmd_ssid_t;

typedef struct qdf_net_wcmd_param {
	qdf_net_wcmd_param_id_t   param_id;
	uint32_t                value;
} qdf_net_wcmd_param_t;

/**
 * @brief optional IE pointer info
 */
typedef qdf_net_ie_info_t  qdf_net_wcmd_optie_t;

/**
 * @brief status of VAP interface
 */
typedef struct qdf_net_wcmd_vapstats {
	uint8_t                  status;/**< Status*/
	qdf_net_wcmd_linkqty_t     qual;/**< Quality of the link*/
	qdf_net_wcmd_discard_t     discard;/**< Packet discarded counts */
	qdf_net_wcmd_miss_t        miss;/**< Packet missed counts */
} qdf_net_wcmd_vapstats_t;

/**
 * @brief appie pointer info
 */
typedef struct qdf_net_wcmd_appie {
	qdf_net_wcmd_appie_frame_t     frmtype;
	uint32_t                     len;
	uint8_t                      data[QDF_NET_WCMD_IE_MAXLEN];
} qdf_net_wcmd_appie_t;

/**
 * @brief send ADDBA info pointer
 */
typedef struct qdf_net_wcmd_addba {
	uint16_t  aid;
	uint32_t  tid;
	uint32_t  size;
} qdf_net_wcmd_addba_t;

/**
 * @brief ADDBA status pointer info
 */
typedef struct qdf_net_wcmd_addba_status {
	uint16_t  aid;
	uint32_t  tid;
	uint16_t  status;
} qdf_net_wcmd_addbQDF_STATUS;

/**
 * @brief ADDBA response pointer info
 */
typedef struct qdf_net_wcmd_addba_resp {
	uint16_t  aid;
	uint32_t  tid;
	uint32_t  reason;
} qdf_net_wcmd_addba_resp_t;

/**
 * @brief send DELBA info pointer
 */
typedef struct qdf_net_wcmd_delba {
	uint16_t  aid;
	uint32_t  tid;
	uint32_t  identifier;
	uint32_t  reason;
} qdf_net_wcmd_delba_t;

/**
 * @brief MLME
 */
typedef struct qdf_net_wcmd_mlme {
	qdf_net_wcmd_mlme_op_type_t  op;/**< operation to perform */
	uint8_t                    reason;/**< 802.11 reason code */
	qdf_net_ethaddr_t            mac;
} qdf_net_wcmd_mlme_t;

/**
 * @brief Set the active channel list.  Note this list is
 * intersected with the available channel list in
 * calculating the set of channels actually used in
 * scanning.
 */
typedef struct qdf_net_wcmd_chanlist {
	uint8_t   chanlist[QDF_NET_WCMD_CHAN_BYTES];
} qdf_net_wcmd_chanlist_t;

/**
 * @brief Channels are specified by frequency and attributes.
 */
typedef struct qdf_net_wcmd_chan {
	uint16_t  freq;/**< setting in Mhz */
	uint32_t  flags;/**< see below */
	uint8_t   ieee;/**< IEEE channel number */
	int8_t    maxregpower;/**< maximum regulatory tx power in dBm */
	int8_t    maxpower;/**< maximum tx power in dBm */
	int8_t    minpower;/**< minimum tx power in dBm */
	uint8_t   regclassid;/**< regulatory class id */
} qdf_net_wcmd_chan_t;

/**
 * @brief channel info pointer
 */
typedef struct qdf_net_wcmd_chaninfo {
	uint32_t            nchans;
	qdf_net_wcmd_chan_t   chans;
} qdf_net_wcmd_chaninfo_t;

/**
 * @brief wmm-param info
 */
typedef struct qdf_net_wcmd_wmmparaminfo {
	qdf_net_wcmd_wmmparams_t  cmd;
	uint32_t                ac;
	uint32_t                bss;
	uint32_t                value;
} qdf_net_wcmd_wmmparaminfo_t;

/**
 * @brief wpa ie pointer info
 */
typedef struct qdf_net_wcmd_wpaie {
	qdf_net_ethaddr_t  mac;
	qdf_net_ie_info_t  ie;
} qdf_net_wcmd_wpaie_t;

/**
 * @brief wsc ie pointer info
 */
typedef struct qdf_net_wcmd_wscie {
	qdf_net_ethaddr_t  mac;
	qdf_net_ie_info_t   ie;
} qdf_net_wcmd_wscie_t;

/**
 * @brief rts threshold set/get info
 */
typedef struct qdf_net_wcmd_rts_th {
	uint16_t          threshold;
	uint16_t          disabled;
	uint16_t          fixed;
} qdf_net_wcmd_rts_th_t;

/**
 * @brief fragment threshold set/get info
 */
typedef struct qdf_net_wcmd_frag_th {
	uint16_t     threshold;
	uint16_t     disabled;
	uint16_t     fixed;
} qdf_net_wcmd_frag_th_t;

/**
 * @brief ic_caps set/get/enable/disable info
 */
typedef uint32_t     qdf_net_wcmd_ic_caps_t;

/**
 * @brief retries set/get/enable/disable info
 */
typedef struct qdf_net_wcmd_retries {
	int32_t          value;/**< The value of the parameter itself */
	uint8_t          disabled;/**< Disable the feature */
	uint16_t         flags;/**< Various specifc flags (if any) */
} qdf_net_wcmd_retries_t;

/**
 * @brief power set/get info
 */
typedef struct qdf_net_wcmd_power {
	int32_t               value;/**< The value of the parameter itself */
	uint8_t               disabled;/**< Disable the feature */
	qdf_net_wcmd_pmflags_t  flags;/**< Various specifc flags (if any) */
	int32_t               fixed;/**< fixed */
} qdf_net_wcmd_power_t;

/**
 * @brief txpower set/get/enable/disable info
 */
typedef struct qdf_net_wcmd_txpower {
	uint32_t                     txpower;
	uint8_t                      fixed;
	uint8_t                      disabled;
	qdf_net_wcmd_txpow_flags_t     flags;
} qdf_net_wcmd_txpower_t;

/**
 * @brief tx-power-limit info
 */
typedef uint32_t  qdf_net_wcmd_txpowlimit_t;

/**
 * @brief Scan result data returned
 */
typedef struct qdf_net_wcmd_scan_result {
	uint16_t  isr_len;        /**< length (mult of 4) */
	uint16_t  isr_freq;       /**< MHz */
	uint32_t  isr_flags;     /**< channel flags */
	uint8_t   isr_noise;
	uint8_t   isr_rssi;
	uint8_t   isr_intval;     /**< beacon interval */
	uint16_t  isr_capinfo;    /**< capabilities */
	uint8_t   isr_erp;        /**< ERP element */
	uint8_t   isr_bssid[QDF_NET_WCMD_ADDR_LEN];
	uint8_t   isr_nrates;
	uint8_t   isr_rates[QDF_NET_WCMD_RATE_MAXSIZE];
	uint8_t   isr_ssid_len;   /**< SSID length */
	uint8_t   isr_ie_len;     /**< IE length */
	uint8_t   isr_pad[5];
	/* variable length SSID followed by IE data */
	uint8_t   isr_ssid[QDF_NET_WCMD_MAX_SSID];
	uint8_t   isr_wpa_ie[QDF_NET_WCMD_IE_MAXLEN];
	uint8_t   isr_wme_ie[QDF_NET_WCMD_IE_MAXLEN];
	uint8_t   isr_ath_ie[QDF_NET_WCMD_IE_MAXLEN];
} qdf_net_wcmd_scan_result_t;

/**
 * @brief scan request info
 */
typedef struct qdf_net_wcmd_scan {
	qdf_net_wcmd_scan_result_t  result[QDF_NET_WCMD_MAX_AP];
	uint32_t                  len;/*Valid entries*/
} qdf_net_wcmd_scan_t;

/**
 * waplist data structure
 */
typedef struct qdf_net_vaplist {
	qdf_net_ethaddr_t       mac;
	qdf_net_wcmd_opmode_t   mode;
	qdf_net_wcmd_linkqty_t  qual;
} qdf_net_vaplist_t;

/**
 * @brief waplist request info
 */
typedef struct qdf_net_wcmd_vaplist {
	qdf_net_vaplist_t       list[QDF_NET_WCMD_MAX_AP];
	uint32_t              len;
} qdf_net_wcmd_vaplist_t;

/**
 * @brief Station information block; the mac address is used
 * to retrieve other data like stats, unicast key, etc.
 */
typedef struct qdf_net_wcmd_sta {
	uint16_t  isi_len;/**< length (mult of 4) */
	uint16_t  isi_freq;/**< MHz */
	uint64_t  isi_flags;/**< channel flags */
	uint16_t  isi_state;/**< state flags */
	uint8_t   isi_authmode;/**< authentication algorithm */
	int8_t    isi_rssi;
	uint16_t  isi_capinfo;/**< capabilities */
	uint8_t   isi_athflags;/**< Atheros capabilities */
	uint8_t   isi_erp;/**< ERP element */
	uint8_t   isi_macaddr[QDF_NET_WCMD_ADDR_LEN];
	uint8_t   isi_nrates;/**< negotiated rates */
	uint8_t   isi_rates[QDF_NET_WCMD_RATE_MAXSIZE];
	uint8_t   isi_txrate;/**< index to isi_rates[] */
	uint32_t  isi_txrateKbps;/**< rate in Kbps , for 11n */
	uint16_t  isi_ie_len;/**< IE length */
	uint16_t  isi_associd;/**< assoc response */
	uint16_t  isi_txpower;/**< current tx power */
	uint16_t  isi_vlan;/**< vlan tag */
	uint16_t  isi_txseqs[17];/**< seq to be transmitted */
	uint16_t  isi_rxseqs[17];/**< seq previous for qos frames*/
	uint16_t  isi_inact;/**< inactivity timer */
	uint8_t   isi_uapsd;/**< UAPSD queues */
	uint8_t   isi_opmode;/**< sta operating mode */
	uint8_t   isi_cipher;
	uint32_t  isi_assoc_time;/**< sta association time */
	uint16_t  isi_htcap;/**< HT capabilities */
	/* XXX frag state? */
	/* variable length IE data */
	uint8_t   isi_wpa_ie[QDF_NET_WCMD_IE_MAXLEN];
	uint8_t   isi_wme_ie[QDF_NET_WCMD_IE_MAXLEN];
	uint8_t   isi_ath_ie[QDF_NET_WCMD_IE_MAXLEN];
} qdf_net_wcmd_sta_t;

/**
 * @brief list of stations
 */
typedef struct qdf_net_wcmd_stainfo {
	qdf_net_wcmd_sta_t   list[QDF_NET_WCMD_MAX_AP];
	uint32_t           len;
} qdf_net_wcmd_stainfo_t;

/**
 * @brief ath caps info
 */
typedef struct qdf_net_wcmd_devcap {
	int32_t   cap;
	int32_t   setting;
} qdf_net_wcmd_devcap_t;

/**
 * @brief station stats
 */
typedef struct qdf_net_wcmd_stastats {
	qdf_net_ethaddr_t  mac;/**< MAC of the station*/
	struct ns_data {
		uint32_t  ns_rx_data;/**< rx data frames */
		uint32_t  ns_rx_mgmt;/**< rx management frames */
		uint32_t  ns_rx_ctrl;/**< rx control frames */
		uint32_t  ns_rx_ucast;/**< rx unicast frames */
		uint32_t  ns_rx_mcast;/**< rx multi/broadcast frames */
		uint64_t  ns_rx_bytes;/**< rx data count (bytes) */
		uint64_t  ns_rx_beacons;/**< rx beacon frames */
		uint32_t  ns_rx_proberesp;/**< rx probe response frames */

		uint32_t  ns_rx_dup;/**< rx discard 'cuz dup */
		uint32_t  ns_rx_noprivacy;/**< rx w/ wep but privacy off */
		uint32_t  ns_rx_wepfail;/**< rx wep processing failed */
		uint32_t  ns_rx_demicfail;/**< rx demic failed */
		uint32_t  ns_rx_decap;/**< rx decapsulation failed */
		uint32_t  ns_rx_defrag;/**< rx defragmentation failed */
		uint32_t  ns_rx_disassoc;/**< rx disassociation */
		uint32_t  ns_rx_deauth;/**< rx deauthentication */
		uint32_t  ns_rx_action;/**< rx action */
		uint32_t  ns_rx_decryptcrc;/**< rx decrypt failed on crc */
		uint32_t  ns_rx_unauth;/**< rx on unauthorized port */
		uint32_t  ns_rx_unencrypted;/**< rx unecrypted w/ privacy */
		uint32_t  ns_tx_data;/**< tx data frames */
		uint32_t  ns_tx_mgmt;/**< tx management frames */
		uint32_t  ns_tx_ucast;/**< tx unicast frames */
		uint32_t  ns_tx_mcast;/**< tx multi/broadcast frames */
		uint64_t  ns_tx_bytes;/**< tx data count (bytes) */
		uint32_t  ns_tx_probereq;/**< tx probe request frames */
		uint32_t  ns_tx_uapsd;/**< tx on uapsd queue */

		uint32_t  ns_tx_novlantag;/**< tx discard 'cuz no tag */
		uint32_t  ns_tx_vlanmismatch;/**< tx discard 'cuz bad tag */
		uint32_t  ns_tx_eosplost;/**< uapsd EOSP retried out */
		uint32_t  ns_ps_discard;/**< ps discard 'cuz of age */
		uint32_t  ns_uapsd_triggers;/**< uapsd triggers */
	/* MIB-related state */
		uint32_t  ns_tx_assoc;/**< [re]associations */
		uint32_t  ns_tx_assoc_fail;/**< [re]association failures */
		uint32_t  ns_tx_auth;/**< [re]authentications */
		uint32_t  ns_tx_auth_fail;/**< [re]authentication failures*/
		uint32_t  ns_tx_deauth;/**< deauthentications */
		uint32_t  ns_tx_deauth_code;/**< last deauth reason */
		uint32_t  ns_tx_disassoc;/**< disassociations */
		uint32_t  ns_tx_disassoc_code;/**< last disassociation reason */
		uint32_t  ns_psq_drops;/**< power save queue drops */
	} data;
} qdf_net_wcmd_stastats_t;
/**
 * @brief 11n tx/rx stats
 */
typedef struct qdf_net_wcmd_11n_stats {
	uint32_t   tx_pkts;/**< total tx data packets */
	uint32_t   tx_checks;/**< tx drops in wrong state */
	uint32_t   tx_drops;/**< tx drops due to qdepth limit */
	uint32_t   tx_minqdepth;/**< tx when h/w queue depth is low */
	uint32_t   tx_queue;/**< tx pkts when h/w queue is busy */
	uint32_t   tx_comps;/**< tx completions */
	uint32_t   tx_stopfiltered;/**< tx pkts filtered for requeueing */
	uint32_t   tx_qnull;/**< txq empty occurences */
	uint32_t   tx_noskbs;/**< tx no skbs for encapsulations */
	uint32_t   tx_nobufs;/**< tx no descriptors */
	uint32_t   tx_badsetups;/**< tx key setup failures */
	uint32_t   tx_normnobufs;/**< tx no desc for legacy packets */
	uint32_t   tx_schednone;/**< tx schedule pkt queue empty */
	uint32_t   tx_bars;/**< tx bars sent */
	uint32_t   txbar_xretry;/**< tx bars excessively retried */
	uint32_t   txbar_compretries;/**< tx bars retried */
	uint32_t   txbar_errlast;/**< tx bars last frame failed */
	uint32_t   tx_compunaggr;/**< tx unaggregated frame completions */
	uint32_t   txunaggr_xretry;/**< tx unaggregated excessive retries */
	uint32_t   tx_compaggr;/**< tx aggregated completions */
	uint32_t   tx_bawadv;/**< tx block ack window advanced */
	uint32_t   tx_bawretries;/**< tx block ack window retries */
	uint32_t   tx_bawnorm;/**< tx block ack window additions */
	uint32_t   tx_bawupdates;/**< tx block ack window updates */
	uint32_t   tx_bawupdtadv;/**< tx block ack window advances */
	uint32_t   tx_retries;/**< tx retries of sub frames */
	uint32_t   tx_xretries;/**< tx excessive retries of aggregates */
	uint32_t   txaggr_noskbs;/**< tx no skbs for aggr encapsualtion */
	uint32_t   txaggr_nobufs;/**< tx no desc for aggr */
	uint32_t   txaggr_badkeys;/**< tx enc key setup failures */
	uint32_t   txaggr_schedwindow;/**< tx no frame scheduled: baw limited */
	uint32_t   txaggr_single;/**< tx frames not aggregated */
	uint32_t   txaggr_compgood;/**< tx aggr good completions */
	uint32_t   txaggr_compxretry;/**< tx aggr excessive retries */
	uint32_t   txaggr_compretries;/**< tx aggr unacked subframes */
	uint32_t   txunaggr_compretries;/**< tx non-aggr unacked subframes */
	uint32_t   txaggr_prepends;/**< tx aggr old frames requeued */
	uint32_t   txaggr_filtered;/**< filtered aggr packet */
	uint32_t   txaggr_fifo;/**< fifo underrun of aggregate */
	uint32_t   txaggr_xtxop;/**< txop exceeded for an aggregate */
	uint32_t   txaggr_desc_cfgerr;/**< aggregate descriptor config error */
	uint32_t   txaggr_data_urun;/**< data underrun for an aggregate */
	uint32_t   txaggr_delim_urun;/**< delimiter underrun for an aggregate */
	uint32_t   txaggr_errlast;/**< tx aggr: last sub-frame failed */
	uint32_t   txunaggr_errlast;/**< tx non-aggr: last frame failed */
	uint32_t   txaggr_longretries;/**< tx aggr h/w long retries */
	uint32_t   txaggr_shortretries;/**< tx aggr h/w short retries */
	uint32_t   txaggr_timer_exp;/**< tx aggr : tx timer expired */
	uint32_t   txaggr_babug;/**< tx aggr : BA bug */
	uint32_t   txaggr_badtid; /**< tx aggr: Bad TID */
	uint32_t   rx_pkts;/**< rx pkts */
	uint32_t   rx_aggr;/**< rx aggregated packets */
	uint32_t   rx_aggrbadver;/**< rx pkts with bad version */
	uint32_t   rx_bars;/**< rx bars */
	uint32_t   rx_nonqos;/**< rx non qos-data frames */
	uint32_t   rx_seqreset;/**< rx sequence resets */
	uint32_t   rx_oldseq;/**< rx old packets */
	uint32_t   rx_bareset;/**< rx block ack window reset */
	uint32_t   rx_baresetpkts;/**< rx pts indicated due to baw resets */
	uint32_t   rx_dup;/**< rx duplicate pkts */
	uint32_t   rx_baadvance;/**< rx block ack window advanced */
	uint32_t   rx_recvcomp;/**< rx pkt completions */
	uint32_t   rx_bardiscard;/**< rx bar discarded */
	uint32_t   rx_barcomps;/**< rx pkts unblocked on bar reception */
	uint32_t   rx_barrecvs;/**< rx pkt completions on bar reception */
	uint32_t   rx_skipped;/**< rx pkt sequences skipped on timeout */
	uint32_t   rx_comp_to;/**< rx indications due to timeout */
	uint32_t   wd_tx_active;/**< watchdog: tx is active */
	uint32_t   wd_tx_inactive;/**< watchdog: tx is not active */
	uint32_t   wd_tx_hung;/**< watchdog: tx is hung */
	uint32_t   wd_spurious;/**< watchdog: spurious tx hang */
	uint32_t   tx_requeue;/**< filter & requeue on 20/40 transitions */
	uint32_t   tx_drain_txq;/**< draining tx queue on error */
	uint32_t   tx_drain_tid;/**< draining tid buf queue on error */
	uint32_t   tx_drain_bufs;/**< buffers drained from pending tid queue */
	uint32_t   tx_tidpaused;/**< pausing tx on tid */
	uint32_t   tx_tidresumed;/**< resuming tx on tid */
	uint32_t   tx_unaggr_filtered;/**< unaggregated tx pkts filtered */
	uint32_t   tx_aggr_filtered;/**< aggregated tx pkts filtered */
	uint32_t   tx_filtered;/**< total sub-frames filtered */
	uint32_t   rx_rb_on;/**< total rb on-s */
	uint32_t   rx_rb_off;/**< total rb off-s */
	uint32_t   tx_deducted_tokens;/**< ATF txtokens deducted */
	uint32_t   tx_unusable_tokens;/**< ATF txtokens unusable */
} qdf_net_wcmd_11n_stats_t;


/**
 * @brief ampdu info
 */
typedef struct qdf_net_wcmd_ampdu_trc {
	uint32_t   tr_head;
	uint32_t   tr_tail;
	struct trc_entry {
		uint16_t   tre_seqst;/**< starting sequence of aggr */
		uint16_t   tre_baseqst;/**< starting sequence of ba */
		uint32_t   tre_npkts;/**< packets in aggregate */
		uint32_t   tre_aggrlen;/**< aggregation length */
		uint32_t   tre_bamap0;/**< block ack bitmap word 0 */
		uint32_t   tre_bamap1;/**< block ack bitmap word 1 */
	} tr_ents[QDF_NET_WCMD_NUM_TR_ENTS];
} qdf_net_wcmd_ampdu_trc_t;

/**
 * @brief phy stats info
 */
typedef struct qdf_net_wcmd_phystats {
	uint32_t   ast_watchdog;/**< device reset by watchdog */
	uint32_t   ast_hardware;/**< fatal hardware error interrupts */
	uint32_t   ast_bmiss;/**< beacon miss interrupts */
	uint32_t   ast_rxorn;/**< rx overrun interrupts */
	uint32_t   ast_rxeol;/**< rx eol interrupts */
	uint32_t   ast_txurn;/**< tx underrun interrupts */
	uint32_t   ast_txto;/**< tx timeout interrupts */
	uint32_t   ast_cst;/**< carrier sense timeout interrupts */
	uint32_t   ast_mib;/**< mib interrupts */
	uint32_t   ast_tx_packets;/**< packet sent on the interface */
	uint32_t   ast_tx_mgmt;/**< management frames transmitted */
	uint32_t   ast_tx_discard;/**< frames discarded prior to assoc */
	uint32_t   ast_tx_invalid;/**< frames discarded 'cuz device gone */
	uint32_t   ast_tx_qstop;/**< tx queue stopped 'cuz full */
	uint32_t   ast_tx_encap;/**< tx encapsulation failed */
	uint32_t   ast_tx_nonode;/**< no node*/
	uint32_t   ast_tx_nobuf;/**< no buf */
	uint32_t   ast_tx_stop; /**< number of times netif stopped */
	uint32_t   ast_tx_resume; /**< number of times netif resumed */
	uint32_t   ast_tx_nobufmgt;/**< no buffer (mgmt)*/
	uint32_t   ast_tx_xretries;/**< too many retries */
	uint32_t   ast_tx_fifoerr;/**< FIFO underrun */
	uint32_t   ast_tx_filtered;/**< xmit filtered */
	uint32_t   ast_tx_timer_exp;/**< tx timer expired */
	uint32_t   ast_tx_shortretry;/**< on-chip retries (short) */
	uint32_t   ast_tx_longretry;/**< tx on-chip retries (long) */
	uint32_t   ast_tx_badrate;/**< tx failed 'cuz bogus xmit rate */
	uint32_t   ast_tx_noack;/**< tx frames with no ack marked */
	uint32_t   ast_tx_rts;/**< tx frames with rts enabled */
	uint32_t   ast_tx_cts;/**< tx frames with cts enabled */
	uint32_t   ast_tx_shortpre;/**< tx frames with short preamble */
	uint32_t   ast_tx_altrate;/**< tx frames with alternate rate */
	uint32_t   ast_tx_protect;/**< tx frames with protection */
	uint32_t   ast_rx_orn;/**< rx failed 'cuz of desc overrun */
	uint32_t   ast_rx_crcerr;/**< rx failed 'cuz of bad CRC */
	uint32_t   ast_rx_fifoerr;/**< rx failed 'cuz of FIFO overrun */
	uint32_t   ast_rx_badcrypt;/**< rx failed 'cuz decryption */
	uint32_t   ast_rx_badmic;/**< rx failed 'cuz MIC failure */
	uint32_t   ast_rx_phyerr;/**< rx PHY error summary count */
	uint32_t   ast_rx_phy[64];/**< rx PHY error per-code counts */
	uint32_t   ast_rx_tooshort;/**< rx discarded 'cuz frame too short */
	uint32_t   ast_rx_toobig;/**< rx discarded 'cuz frame too large */
	uint32_t   ast_rx_nobuf;/**< rx setup failed 'cuz no skbuff */
	uint32_t   ast_rx_packets;/**< packet recv on the interface */
	uint32_t   ast_rx_mgt;/**< management frames received */
	uint32_t   ast_rx_ctl;/**< control frames received */
	int8_t     ast_tx_rssi_combined;/**< tx rssi of last ack [combined] */
	int8_t     ast_tx_rssi_ctl0;/**< tx rssi of last ack [ctl, chain 0] */
	int8_t     ast_tx_rssi_ctl1;/**< tx rssi of last ack [ctl, chain 1] */
	int8_t     ast_tx_rssi_ctl2;/**< tx rssi of last ack [ctl, chain 2] */
	int8_t     ast_tx_rssi_ext0;/**< tx rssi of last ack [ext, chain 0] */
	int8_t     ast_tx_rssi_ext1;/**< tx rssi of last ack [ext, chain 1] */
	int8_t     ast_tx_rssi_ext2;/**< tx rssi of last ack [ext, chain 2] */
	int8_t     ast_rx_rssi_combined;/**< rx rssi from histogram [combined]*/
	int8_t     ast_rx_rssi_ctl0;/**< rx rssi from histogram [ctl, chain 0] */
	int8_t     ast_rx_rssi_ctl1;/**< rx rssi from histogram [ctl, chain 1] */
	int8_t     ast_rx_rssi_ctl2;/**< rx rssi from histogram [ctl, chain 2] */
	int8_t     ast_rx_rssi_ext0;/**< rx rssi from histogram [ext, chain 0] */
	int8_t     ast_rx_rssi_ext1;/**< rx rssi from histogram [ext, chain 1] */
	int8_t     ast_rx_rssi_ext2;/**< rx rssi from histogram [ext, chain 2] */
	uint32_t   ast_be_xmit;/**< beacons transmitted */
	uint32_t   ast_be_nobuf;/**< no skbuff available for beacon */
	uint32_t   ast_per_cal;/**< periodic calibration calls */
	uint32_t   ast_per_calfail;/**< periodic calibration failed */
	uint32_t   ast_per_rfgain;/**< periodic calibration rfgain reset */
	uint32_t   ast_rate_calls;/**< rate control checks */
	uint32_t   ast_rate_raise;/**< rate control raised xmit rate */
	uint32_t   ast_rate_drop;/**< rate control dropped xmit rate */
	uint32_t   ast_ant_defswitch;/**< rx/default antenna switches */
	uint32_t   ast_ant_txswitch;/**< tx antenna switches */
	uint32_t   ast_ant_rx[8];/**< rx frames with antenna */
	uint32_t   ast_ant_tx[8];/**< tx frames with antenna */
	uint32_t   ast_suspend;/**< driver suspend calls */
	uint32_t   ast_resume;/**< driver resume calls  */
	uint32_t   ast_shutdown;/**< driver shutdown calls  */
	uint32_t   ast_init;/**< driver init calls  */
	uint32_t   ast_stop;/**< driver stop calls  */
	uint32_t   ast_reset;/**< driver resets      */
	uint32_t   ast_nodealloc;/**< nodes allocated    */
	uint32_t   ast_nodefree;/**< nodes deleted      */
	uint32_t   ast_keyalloc;/**< keys allocated     */
	uint32_t   ast_keydelete;/**< keys deleted       */
	uint32_t   ast_bstuck;/**< beacon stuck       */
	uint32_t   ast_draintxq;/**< drain tx queue     */
	uint32_t   ast_stopdma;/**< stop tx queue dma  */
	uint32_t   ast_stoprecv;/**< stop recv          */
	uint32_t   ast_startrecv;/**< start recv         */
	uint32_t   ast_flushrecv;/**< flush recv         */
	uint32_t   ast_chanchange;/**< channel changes    */
	uint32_t   ast_fastcc;/**< Number of fast channel changes */
	uint32_t   ast_fastcc_errs;/**< Number of failed fast channel changes */
	uint32_t   ast_chanset;/**< channel sets       */
	uint32_t   ast_cwm_mac;/**< CWM - mac mode switch */
	uint32_t   ast_cwm_phy;/**< CWM - phy mode switch */
	uint32_t   ast_cwm_requeue;/**< CWM - requeue dest node packets */
	uint32_t   ast_rx_delim_pre_crcerr;/**< pre-delimit crc errors */
	uint32_t   ast_rx_delim_post_crcerr;/**< post-delimit crc errors */
	uint32_t   ast_rx_decrypt_busyerr;/**< decrypt busy errors */

	qdf_net_wcmd_11n_stats_t  ast_11n;/**< 11n statistics */
	qdf_net_wcmd_ampdu_trc_t  ast_trc;/**< ampdu trc */
} qdf_net_wcmd_phystats_t;

/**
 * @brief diag info
 */
typedef struct qdf_net_wcmd_diag {
	int8_t     ad_name[QDF_NET_WCMD_NAME_SIZE];/**< if name*/
	uint16_t   ad_id;
	uint16_t   ad_in_size;/**< pack to fit, yech */
	uint8_t    *ad_in_data;
	uint8_t    *ad_out_data;
	uint32_t   ad_out_size;
} qdf_net_wcmd_diag_t;

/*
 * Device phyerr ioctl info
 */
typedef struct qdf_net_wcmd_phyerr {
/**< if name, e.g. "ath0" */
	int8_t     ad_name[QDF_NET_WCMD_NAME_SIZE];
	uint16_t   ad_id;
	uint16_t   ad_in_size;                /**< pack to fit, yech */
	uint8_t    *ad_in_data;
	uint8_t    *ad_out_data;
	uint32_t   ad_out_size;
} qdf_net_wcmd_phyerr_t;

/**
 * @brief cwm-info
 */
typedef struct qdf_net_wcmd_cwminfo {
	uint32_t  ci_chwidth; /**< channel width */
	uint32_t  ci_macmode; /**< MAC mode */
	uint32_t  ci_phymode; /**< Phy mode */
	uint32_t  ci_extbusyper; /**< extension busy (percent) */
} qdf_net_wcmd_cwminfo_t;

/**
 * @brief cwm-dbg
 */
typedef struct qdf_net_wcmd_cwmdbg {
	qdf_net_wcmd_cwm_cmd_t    dc_cmd;/**< dbg commands*/
	qdf_net_wcmd_cwm_event_t  dc_arg;/**< events*/
} qdf_net_wcmd_cwmdbg_t;

/**
 * @brief device cwm info
 */
typedef struct qdf_net_wcmd_cwm {
	uint32_t      type;
	union {
		qdf_net_wcmd_cwmdbg_t   dbg;
		qdf_net_wcmd_cwminfo_t  info;
	} cwm;
} qdf_net_wcmd_cwm_t;

/**
 * @brief Helpers to access the CWM structures
 */
#define cwm_dbg         cwm.dbg
#define cwm_info        cwm.info

/**
 * @brief eth tool info
 */
typedef struct qdf_net_wcmd_ethtool {
	uint32_t  cmd;/*XXX:???*/
	int8_t    driver[QDF_NET_WCMD_DRIVSIZ];/**< driver short name */
	int8_t    version[QDF_NET_WCMD_VERSIZ];/**< driver ver string */
	int8_t    fw_version[QDF_NET_WCMD_FIRMSIZ];/**< firmware ver string*/
	int8_t    bus_info[QDF_NET_WCMD_BUSINFO_LEN];/**< Bus info */
	int8_t    reserved1[32];
	int8_t    reserved2[16];
	uint32_t  n_stats;/**< number of u64's from ETHTOOL_GSTATS */
	uint32_t  testinfo_len;
	uint32_t  eedump_len;/**< Size of data from EEPROM(bytes) */
	uint32_t  regdump_len;/**< Size of data from REG(bytes) */
} qdf_net_wcmd_ethtool_t;

typedef struct qdf_net_wcmd_ethtool_info {
	qdf_net_wcmd_ethtool_cmd_t     cmd;/*XXX:???*/
	qdf_net_wcmd_ethtool_t         drv;
} qdf_net_wcmd_ethtool_info_t;

/**
 * @brief vap create flag info
 */
typedef enum qdf_net_wcmd_vapcreate_flags {
	QDF_NET_WCMD_CLONE_BSSID = 0x1,/**< allocate unique mac/bssid */
	QDF_NET_WCMD_NO_STABEACONS/**< Do not setup the sta beacon timers*/
} qdf_net_wcmd_vapcreate_flags_t;

/**
 * @brief VAP info structure used during VAPCREATE
 */
typedef struct qdf_net_wcmd_vapinfo {
	uint8_t                       icp_name[QDF_NET_WCMD_NAME_SIZE];
	qdf_net_wcmd_opmode_t           icp_opmode;/**< operating mode */
	qdf_net_wcmd_vapcreate_flags_t  icp_flags;
} qdf_net_wcmd_vapinfo_t;


/**
 * @brief ath stats info
 */
typedef struct qdf_net_wcmd_devstats {
	uint64_t   rx_packets;/**< total packets received       */
	uint64_t   tx_packets;/**< total packets transmitted    */
	uint64_t   rx_bytes;/**< total bytes received         */
	uint64_t   tx_bytes;/**< total bytes transmitted      */
	uint64_t   rx_errors;/**< bad packets received         */
	uint64_t   tx_errors;/**< packet transmit problems     */
	uint64_t   rx_dropped;/**< no space in linux buffers    */
	uint64_t   tx_dropped;/**< no space available in linux  */
	uint64_t   multicast;/**< multicast packets received   */
	uint64_t   collisions;

	/* detailed rx_errors: */
	uint64_t   rx_length_errors;
	uint64_t   rx_over_errors;/**< receiver ring buff overflow  */
	uint64_t   rx_crc_errors;/**< recved pkt with crc error    */
	uint64_t   rx_frame_errors;/**< recv'd frame alignment error */
	uint64_t   rx_fifo_errors;/**< recv'r fifo overrun          */
	uint64_t   rx_missed_errors;/**< receiver missed packet       */

	/* detailed tx_errors */
	uint64_t   tx_aborted_errors;
	uint64_t   tx_carrier_errors;
	uint64_t   tx_fifo_errors;
	uint64_t   tx_heartbeat_errors;
	uint64_t   tx_window_errors;

	/* for cslip etc */
	uint64_t   rx_compressed;
	uint64_t   tx_compressed;
} qdf_net_wcmd_devstats_t;

/**
 * @brief mtu set/get/enable/disable info
 */
typedef  uint32_t     qdf_net_wcmd_mtu_t;
/**
 * @brief turbo
 */
typedef  uint32_t     qdf_net_wcmd_turbo_t;
/**
 * @brief Used for user specific debugging info
 *  exchange from the driver
 */
typedef struct qdf_net_wcmd_dbg_info {
	qdf_net_wcmd_dbg_type_t     type;
	uint8_t                   data[QDF_NET_WCMD_DBG_MAXLEN];
} qdf_net_wcmd_dbg_info_t;


typedef union qdf_net_wcmd_data {
	qdf_net_wcmd_vapname_t          vapname;/*XXX: ???*/
	qdf_net_wcmd_bssid_t            bssid;
	qdf_net_wcmd_nickname_t         nickname;
	qdf_net_wcmd_ssid_t             essid;
	qdf_net_wcmd_rts_th_t           rts;/*GET_RTS_THRES & SET_RTS_THRES*/
	qdf_net_wcmd_frag_th_t          frag;/*GET_FRAG & SET_FRAG*/
	qdf_net_wcmd_ic_caps_t          ic_caps;
	qdf_net_wcmd_freq_t             freq;
	qdf_net_wcmd_retries_t          retries;
	qdf_net_wcmd_txpower_t          txpower;
	qdf_net_wcmd_txpowlimit_t       txpowlimit;
	qdf_net_wcmd_vaplist_t          vaplist;
	qdf_net_wcmd_phymode_t          phymode;
	qdf_net_wcmd_vapmode_t          vapmode;/*GET_OPMODE & SET_OPMODE*/
	qdf_net_wcmd_devcap_t           devcap;
	qdf_net_wcmd_turbo_t            turbo;
	qdf_net_wcmd_param_t            param;
	qdf_net_wcmd_optie_t            optie;
	qdf_net_wcmd_appie_t            appie;
	qdf_net_wcmd_filter_type_t      filter;
	qdf_net_wcmd_addba_t            addba;
	qdf_net_wcmd_delba_t            delba;
	qdf_net_wcmd_addbQDF_STATUS     addba_status;
	qdf_net_wcmd_addba_resp_t       addba_resp;
	qdf_net_wcmd_keyinfo_t          key;
	qdf_net_wcmd_mlme_t             mlme;
	qdf_net_wcmd_chanlist_t         chanlist;
	qdf_net_wcmd_chaninfo_t         chaninfo;
	qdf_net_wcmd_wmmparaminfo_t     wmmparam;
	qdf_net_wcmd_wpaie_t            wpaie;
	qdf_net_wcmd_wscie_t            wscie;
	qdf_net_wcmd_power_t            power;
	qdf_net_wcmd_diag_t             dev_diag;
	qdf_net_wcmd_phyerr_t           phyerr;
	qdf_net_wcmd_cwm_t              cwm;
	qdf_net_wcmd_ethtool_info_t     ethtool;
	qdf_net_wcmd_vapinfo_t          vap_info;/**< during vapcreate*/

	qdf_net_wcmd_mtu_t              mtu;
	qdf_net_ethaddr_t               mac;/*MAC addr of VAP or Dev */
	qdf_net_wcmd_stainfo_t          *station;
	qdf_net_wcmd_scan_t             *scan;
	qdf_net_wcmd_vapparam_range_t   *range;
	qdf_net_wcmd_stastats_t         *stats_sta;
	qdf_net_wcmd_vapstats_t         *stats_vap;/*XXX: name*/
	qdf_net_wcmd_phystats_t         *stats_phy;
	qdf_net_wcmd_devstats_t         *stats_dev;

	uint32_t                      datum;/*for sysctl*/

	qdf_net_wcmd_dbg_info_t         dbg_info;/* Debugging purpose*/
} qdf_net_wcmd_data_t;

/**
 * @brief ioctl structure to configure the wireless interface.
 */
typedef struct qdf_net_wcmd {
/**< Iface name*/
	char                     if_name[QDF_NET_WCMD_NAME_SIZE];
	qdf_net_wcmd_type_t      type;             /**< Type of wcmd */
	qdf_net_wcmd_data_t      data;             /**< Data */
} qdf_net_wcmd_t;

/**
 * @brief helper macros
 */
#define d_vapname               data.vapname
#define d_bssid                 data.bssid
#define d_nickname              data.nickname
#define d_essid                 data.essid
#define d_rts                   data.rts
#define d_frag                  data.frag
#define d_iccaps                data.ic_caps
#define d_freq                  data.freq
#define d_retries               data.retries
#define d_txpower               data.txpower
#define d_txpowlimit            data.txpowlimit
#define d_vaplist               data.vaplist
#define d_scan                  data.scan
#define d_phymode               data.phymode
#define d_opmode                data.opmode
#define d_devcap                data.devcap
#define d_turbo                 data.turbo
#define d_param                 data.param
#define d_optie                 data.optie
#define d_appie                 data.appie
#define d_filter                data.filter
#define d_addba                 data.addba
#define d_delba                 data.delba
#define d_addba_status          data.addba_status
#define d_addba_resp            data.addba_resp
#define d_key                   data.key
#define d_mlme                  data.mlme
#define d_chanlist              data.chanlist
#define d_chaninfo              data.chaninfo
#define d_wmmparam              data.wmmparam
#define d_wpaie                 data.wpaie
#define d_wscie                 data.wscie
#define d_power                 data.power
#define d_station               data.station
#define d_range                 data.range
#define d_stastats              data.stats_sta
#define d_vapstats              data.stats_vap
#define d_devstats              data.stats_dev
#define d_phystats              data.stats_phy
#define d_daig                  data.dev_diag
#define d_phyerr                data.phyerr
#define d_cwm                   data.cwm
#define d_ethtool               data.ethtool
#define d_vapinfo               data.vap_info
#define d_mtu                   data.mtu
#define d_mac                   data.mac
#define d_datum                 data.datum
#define d_dbginfo               data.dbg_info

typedef struct qdf_net_wcmd_chansw {
	uint8_t    chan;
	uint8_t    ttbt;
} qdf_net_wcmd_chansw_t;
/**
 * ***************************Unresoloved*******************
 */
#endif
