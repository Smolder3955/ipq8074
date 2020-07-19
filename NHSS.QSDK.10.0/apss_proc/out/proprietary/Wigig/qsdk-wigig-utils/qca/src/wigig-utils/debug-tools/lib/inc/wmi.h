/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MARLON_WMI_H_
#define MARLON_WMI_H_

#include "basic_types.h"

#if defined(_MSC_VER)           // Microsoft
#pragma pack (push, marlon_wmi, 1) /* set alignment to 1 byte boundary */
#endif



/**
 * **********************************************************************************************************************************
 *                                                          General
 * **********************************************************************************************************************************
 */

#define WMI_CONTROL_MSG_MAX_LEN         256
#define WMI_MAC_LEN                     6               /* length of mac in bytes */
#define WMI_PROX_RANGE_NUM              3


 enum wmi_phy_capability_e {
    WMI_11A_CAPABILITY   = 1,
    WMI_11G_CAPABILITY   = 2,
    WMI_11AG_CAPABILITY  = 3,
    WMI_11NA_CAPABILITY  = 4,
    WMI_11NG_CAPABILITY  = 5,
    WMI_11NAG_CAPABILITY = 6,
    WMI_11AD_CAPABILITY  = 7,
    // END CAPABILITY
    WMI_11N_CAPABILITY_OFFSET = (WMI_11NA_CAPABILITY - WMI_11A_CAPABILITY),
} ;

/**
 *  Marlon Mailbox interface
 *  used for commands and events
 */
#define WMI_GET_DEVICE_ID(info1) ((info1) & 0xF)
typedef  struct {
    U16         padding;
    U16      commandId;
/*
 * info1 - 16 bits
 * b03:b00 - id
 * b15:b04 - unused
 */
    U16      info1;
    U16      reserved;      /* For alignment */
}  WMI_CMD_HDR;



/**
 * **********************************************************************************************************************************
 *                                                          WMI Commands
 * **********************************************************************************************************************************
 */

/*
 * List of Commands
 */
typedef enum {
    WMI_CONNECT_CMDID                       = 0x0001,
    WMI_DISCONNECT_CMDID                    = 0x0003,
    WMI_START_SCAN_CMDID                    = 0x0007,
    WMI_SET_BSS_FILTER_CMDID                = 0x0009,
    WMI_SET_PROBED_SSID_CMDID               = 0x000a,
    WMI_SET_LISTEN_INT_CMDID                = 0x000b,
    WMI_BCON_CTRL_CMDID                     = 0x000f,
    WMI_ADD_CIPHER_KEY_CMDID                = 0x0016,
    WMI_DELETE_CIPHER_KEY_CMDID             = 0x0017,
    WMI_SET_APPIE_CMDID                     = 0x003f,
    WMI_GET_APPIE_CMDID                     = 0x0040,
    WMI_SET_WSC_STATUS_CMDID                = 0x0041,
    WMI_PXMT_RANGE_CFG_CMDID                = 0x0042,
    WMI_PXMT_SNR2_RANGE_CFG_CMDID           = 0x0043,
    WMI_FAST_MEM_ACC_MODE_CMDID         = 0x0300,
    WMI_MEM_READ_CMDID                = 0x0800,
    WMI_MEM_WR_CMDID                = 0x0801,
    WMI_ECHO_CMDID                = 0x0803,
    WMI_DEEP_ECHO_CMDID                = 0x0804,
    WMI_CONFIG_MAC_CMDID            = 0x0805,
    WMI_CONFIG_PHY_DEBUG_CMDID              = 0x0806,
    WMI_ADD_STATION_CMDID            = 0x0807,
    WMI_ADD_DEBUG_TX_PCKT_CMDID            = 0x0808,
    WMI_PHY_GET_STATISTICS_CMDID            = 0x0809,
    WMI_FS_TUNE_CMDID                       = 0x080A,
    WMI_CORR_MEASURE_CMDID                  = 0x080B,
    WMI_TEMP_SENSE_CMDID                    = 0x080E,
    WMI_DC_CALIB_CMDID                      = 0x080F,
    WMI_SEND_TONE_CMDID                     = 0x0810,
    WMI_IQ_TX_CALIB_CMDID                   = 0x0811,
    WMI_IQ_RX_CALIB_CMDID                   = 0x0812,
    WMI_SET_UCODE_IDLE_CMDID                = 0x0813,
    WMI_SET_WORK_MODE_CMDID                 = 0x0815,
    WMI_LO_LEAKAGE_CALIB_CMDID              = 0x0816,
    WMI_MARLON_R_ACTIVATE_CMDID             = 0x0817,
    WMI_MARLON_R_READ_CMDID                 = 0x0818,
    WMI_MARLON_R_WRITE_CMDID                = 0x0819,
    WMI_MARLON_R_TXRX_SEL_CMDID             = 0x081A,
    MAC_IO_STATIC_PARAMS_CMDID              = 0x081B,
    MAC_IO_DYNAMIC_PARAMS_CMDID             = 0x081C,
    WMI_SILENT_RSSI_CALIB_CMDID             = 0x081D,
    WMI_CFG_RX_CHAIN_CMDID            = 0x0820,
    WMI_VRING_CFG_CMDID                    = 0x0821,
    WMI_RX_ON_CMDID                = 0x0822,
    WMI_VRING_BA_EN_CMDID            = 0x0823,
    WMI_VRING_BA_DIS_CMDID            = 0x0824,
    WMI_RCP_ADDBA_RESP_CMDID            = 0x0825,
    WMI_RCP_DELBA_CMDID                = 0x0826,
    WMI_SET_SSID_CMDID                = 0x0827,
    WMI_GET_SSID_CMDID                = 0x0828,
    WMI_SET_PCP_CHANNEL_CMDID               = 0x0829,
    WMI_GET_PCP_CHANNEL_CMDID               = 0x082a,
    WMI_SW_TX_REQ_CMDID                = 0x082b,
    WMI_READ_MAC_RXQ_CMDID                  = 0x0830,
    WMI_READ_MAC_TXQ_CMDID                  = 0x0831,
    WMI_WRITE_MAC_RXQ_CMDID                 = 0x0832,
    WMI_WRITE_MAC_TXQ_CMDID                 = 0x0833,
    WMI_WRITE_MAC_XQ_FIELD_CMDID            = 0x0834,
    WMI_MLME_PUSH_CMDID                     = 0x0835,
    WMI_BEAMFORMING_MGMT_CMDID              = 0x0836,
    WMI_BF_TXSS_MGMT_CMDID                  = 0x0837,
    WMI_BF_SM_MGMT_CMDID                    = 0x0838,
    WMI_BF_RXSS_MGMT_CMDID                  = 0x0839,
    WMI_SET_SECTORS_CMDID                   = 0x0849,
    WMI_MAINTAIN_PAUSE_CMDID                = 0x0850,
    WMI_MAINTAIN_RESUME_CMDID               = 0x0851,
    WMI_RS_MGMT_CMDID                       = 0x0852,
    WMI_RF_MGMT_CMDID                    = 0x0853,
    /* Performance monitoring commands */
    WMI_BF_CTRL_CMDID                       = 0x0862,
    WMI_NOTIFY_REQ_CMDID                    = 0x0863,
    WMI_GET_STATUS_CMDID                    = 0x0864,
    WMI_UNIT_TEST_CMDID                     = 0x0900,
    WMI_HICCUP_CMDID                        = 0x0901,
    WMI_FLASH_READ_CMDID                    = 0x0902,
    WMI_FLASH_WRITE_CMDID                   = 0x0903,
    WMI_SECURITY_UNIT_TEST_CMDID            = 0x0904,

    WMI_SET_MAC_ADDRESS_CMDID               = 0xF003,
    WMI_ABORT_SCAN_CMDID                    = 0xF007,
    WMI_SET_PMK_CMDID                       = 0xF028,

    WMI_SET_PROMISCUOUS_MODE_CMDID          = 0xF041,
    WMI_GET_PMK_CMDID                       = 0xF048,
    WMI_SET_PASSPHRASE_CMDID                = 0xF049,
    WMI_SEND_ASSOC_RES_CMDID                = 0xF04a,
    WMI_SET_ASSOC_REQ_RELAY_CMDID           = 0xF04b,
    WMI_EAPOL_TX_CMDID                      = 0xF04c,
    WMI_MAC_ADDR_REQ_CMDID                  = 0xF04d,
    WMI_FW_VER_CMDID                     = 0xF04e,
} WMI_COMMAND_ID;



/*******************************************************************************************************************
 *                                              Commands data structures
 *******************************************************************************************************************/

/*
 * Frame Types
 */
 enum wmi_mgmt_frame_type_e {
    WMI_FRAME_BEACON        =   0,
    WMI_FRAME_PROBE_REQ,
    WMI_FRAME_PROBE_RESP,
    WMI_FRAME_ASSOC_REQ,
    WMI_FRAME_ASSOC_RESP,
    WMI_NUM_MGMT_FRAME
} ;

/*
 * WMI_CONNECT_CMDID
 */
 enum wmi_network_type_e {
    INFRA_NETWORK       = 0x01,
    ADHOC_NETWORK       = 0x02,
    ADHOC_CREATOR       = 0x04,
    AP_NETWORK          = 0x10,
    P2P_NETWORK         = 0x20,
    WBE_NETWORK         = 0x40,
} ;

 enum wmi_dot11_auth_mode_e {
    OPEN_AUTH           = 0x01,
    SHARED_AUTH         = 0x02,
    LEAP_AUTH           = 0x04,
    WSC_AUTH            = 0x08
} ;

 enum wmi_auth_mode_e {
    NONE_AUTH           = 0x01,
    WPA_AUTH            = 0x02,
    WPA2_AUTH           = 0x04,
    WPA_PSK_AUTH        = 0x08,
    WPA2_PSK_AUTH       = 0x10,
    WPA_AUTH_CCKM       = 0x20,
    WPA2_AUTH_CCKM      = 0x40,
} ;

 enum wmi_crypto_type_e {
    NONE_CRYPT          = 0x01,
    WEP_CRYPT           = 0x02,
    TKIP_CRYPT          = 0x04,
    AES_CRYPT           = 0x08,
    AES_GCMP_CRYPT      = 0x20
} ;


 enum wmi_connect_ctrl_flag_bits_e {
    CONNECT_ASSOC_POLICY_USER           = 0x0001,
    CONNECT_SEND_REASSOC                = 0x0002,
    CONNECT_IGNORE_WPAx_GROUP_CIPHER    = 0x0004,
    CONNECT_PROFILE_MATCH_DONE          = 0x0008,
    CONNECT_IGNORE_AAC_BEACON           = 0x0010,
    CONNECT_CSA_FOLLOW_BSS              = 0x0020,
    CONNECT_DO_WPA_OFFLOAD              = 0x0040,
    CONNECT_DO_NOT_DEAUTH               = 0x0080,
} ;

#define WMI_MAX_SSID_LEN    32

 typedef PREPACK struct  {
    U08 networkType;
    U08 dot11AuthMode;
    U08 authMode;
    U08 pairwiseCryptoType;
    U08 pairwiseCryptoLen;
    U08 groupCryptoType;
    U08 groupCryptoLen;
    U08 ssidLength;
    U08 ssid[WMI_MAX_SSID_LEN];
    U16 channel;
    U08 bssid[WMI_MAC_LEN];
    U32 ctrl_flags;
    U08 destMacAddr[WMI_MAC_LEN];
    U16 reserved;
}  POSTPACK WMI_CONNECT_CMD;


/*
 * WMI_RECONNECT_CMDID
 */
 typedef PREPACK struct  {
    U16 channel;                    /* hint */
    U08 bssid[WMI_MAC_LEN];         /* mandatory if set */
} POSTPACK WMI_RECONNECT_CMD;


/*
 * WMI_SET_PMK_CMDID
 */

#define WMI_MIN_KEY_INDEX   0
#define WMI_MAX_KEY_INDEX   3
#define WMI_MAX_KEY_LEN     32
#define WMI_PASSPHRASE_LEN  64
#define WMI_PMK_LEN         32

typedef PREPACK struct   {
    U08 pmk[WMI_PMK_LEN];
} POSTPACK WMI_SET_PMK_CMD;


/*
 * WMI_SET_PASSPHRASE_CMDID
 */
 typedef PREPACK struct  {
    U08 ssid[WMI_MAX_SSID_LEN];
    U08 passphrase[WMI_PASSPHRASE_LEN];
    U08 ssid_len;
    U08 passphrase_len;
} POSTPACK WMI_SET_PASSPHRASE_CMD;

/*
 * WMI_ADD_CIPHER_KEY_CMDID
 */
enum wmi_connect_key_usage_e {
    PAIRWISE_USAGE      = 0x00,
    GROUP_USAGE         = 0x01,
    TX_USAGE            = 0x02,     /* default Tx Key - Static WEP only */
} ;

 typedef PREPACK struct  {
    U08     keyIndex;
    U08     keyType;
    U08     keyUsage;           /* wmi_connect_key_usage_e */
    U08     keyLength;
    U08     keyRSC[8];          /* key replay sequence counter */
    U08     key[WMI_MAX_KEY_LEN];
    U08     key_op_ctrl;       /* Additional Key Control information */
    U08     key_macaddr[WMI_MAC_LEN];
} POSTPACK WMI_ADD_CIPHER_KEY_CMD;

/*
 * WMI_DELETE_CIPHER_KEY_CMDID
 */
 typedef PREPACK struct  {
    U08     keyIndex;
    U08     key_macaddr[WMI_MAC_LEN];
} POSTPACK WMI_DELETE_CIPHER_KEY_CMD;


/*
 * WMI_START_SCAN_CMD
 */
 enum wmi_scan_type_e {
    WMI_LONG_SCAN  = 0,
    WMI_SHORT_SCAN = 1,
    WMI_PBC_SCAN   = 2
} ;

 typedef PREPACK struct  {
    S32    forceFgScan;
    S32    isLegacy;               /* For Legacy Cisco AP compatibility */
    U32    homeDwellTime;          /* Maximum duration in the home channel(milliseconds) */
    U32    forceScanInterval;      /* Time interval between scans (milliseconds)*/
    U08    scanType;               /* wmi_scan_type_e */
    U08    numChannels;            /* how many channels follow */
    U16    channelList[4];         /* channels in Mhz */
} POSTPACK WMI_START_SCAN_CMD;

/*
 * WMI_SET_PROBED_SSID_CMDID
 */
#define MAX_PROBED_SSID_INDEX   15

 enum wmi_ssid_flag_e {
    DISABLE_SSID_FLAG  = 0,                  /* disables entry */
    SPECIFIC_SSID_FLAG = 0x01,               /* probes specified ssid */
    ANY_SSID_FLAG      = 0x02,               /* probes for any ssid */
} ;

 typedef PREPACK struct  {
    U08     entryIndex;                     /* 0 to MAX_PROBED_SSID_INDEX */
    U08     flag;                           /* wmi_ssid_flag_e */
    U08     ssidLength;
    U08     ssid[WMI_MAX_SSID_LEN];
} POSTPACK WMI_PROBED_SSID_CMD;


/*
 * Add Application specified IE to a management frame
 */
#define WMI_MAX_IE_LEN  255

#ifdef _WINDOWS
#pragma warning( disable : 4200 ) //suppress warning
#endif
typedef PREPACK struct  {
    U08 mgmtFrmType;  /* one of wmi_mgmt_frame_type_e */
    U08 ieLen;    /* Length  of the IE that should be added to the MGMT frame */
    U08 ieInfo[EMPTY_ARRAY_SIZE];
} POSTPACK WMI_SET_APPIE_CMD;


typedef PREPACK struct {
    U08  destMacAddr[WMI_MAC_LEN];
    U16 range;
} POSTPACK WMI_PXMT_RANGE_CFG_CMD;

typedef PREPACK struct {
    S08 snr2range_arr[WMI_PROX_RANGE_NUM-1];
} POSTPACK WMI_PXMT_SNR2_RANGE_CFG_CMD;

/*
 * WMI_RF_MGMT_CMDID
 */
typedef enum wmi_rf_mgmt_type_e {
    WMI_RF_MGMT_W_DISABLE    = 0x0,
    WMI_RF_MGMT_W_ENABLE    = 0x1,
    WMI_RF_MGMT_GET_STATUS    = 0x2
} WMI_RF_MGMT_TYPE;

 typedef PREPACK struct  {
    U32    rf_mgmt_type;
} POSTPACK WMI_RF_MGMT_CMD;


/*
 * WMI_SET_SSID_CMDID
 */
 typedef PREPACK struct  {
    U32    ssid_len;
    U08    ssid[WMI_MAX_SSID_LEN];
} POSTPACK WMI_SET_SSID_CMD;

/*
 * WMI_SET_PCP_CHANNEL_CMDID
 */
 typedef PREPACK struct  {
    U08    channel_index;
    U08    reserved[3];
} POSTPACK WMI_SET_PCP_CHANNEL_CMD;


/*
 * WMI_BCON_CTRL_CMDID
 */
 typedef PREPACK struct  {
    U16    bcon_interval;
    U16    frag_num;
    U32    ss_mask_low;
    U32    ss_mask_high;
    U16    network_type;
    U08    disable_sec_offload;
    U08    disable_sec;
} POSTPACK WMI_BCON_CTRL_CMD;

/*
 * WMI_SW_TX_REQ_CMDID
 */
typedef PREPACK struct  {
   U08    destMacAddr[WMI_MAC_LEN];
   U16  length;
} POSTPACK WMI_SW_TX_REQ_CMD;

/*
 * WMI_VRING_CFG_CMDID
 */

 typedef PREPACK struct  {
    U32    ring_mem_base_l;
    U16    ring_mem_base_h;
    U16    reserved0;
    U16    ring_size;
    U16    max_mpdu_size;
} POSTPACK wmi_sw_ring_cfg_s;



enum wmi_vring_cfg_schd_params_priority_e {
    REGULAR = 0x0,
    HIGH = 0x1,
};

typedef PREPACK struct  {
    U16    priority;
    U16    timeslot_us;
} POSTPACK wmi_vring_cfg_schd_s;

enum wmi_vring_cfg_encap_trans_type_e {
    ENC_TYPE_802_3 = 0x0,
    ENC_TYPE_NATIVE_WIFI = 0x1
};

enum wmi_vring_cfg_ds_cfg_e {
    DS_PBSS_MODE = 0x0,
    DS_STA_MODE = 0x1,
    DS_AP_MODE = 0x2,
    DS_ADDR4_MODE = 0x3
};

enum wmi_vring_cfg_nwifi_ds_trans_type_e {
    NWIFI_TX_NO_TRANS_MODE = 0x0,
    NWIFI_TX_AP2PBSS_TRANS_MODE = 0x1,
    NWIFI_TX_STA2PBSS_TRANS_MODE = 0x2
};

typedef PREPACK struct  {
    wmi_sw_ring_cfg_s    tx_sw_ring;
    U08    ringid;                         /* 0-23 vrings */

    #define CIDXTID_CID_POS 0
    #define CIDXTID_CID_LEN 4
    #define CIDXTID_CID_MSK 0xF
    #define CIDXTID_TID_POS 4
    #define CIDXTID_TID_LEN 4
    #define CIDXTID_TID_MSK 0xF0
    U08 cidxtid_byte;

    U08    encap_trans_type;
    U08    ds_cfg;                         /* 802.3 DS cfg */
    U08    nwifi_ds_trans_type;

    #define VRING_CFG_MAC_CTRL_LIFETIME_EN_POS 0
    #define VRING_CFG_MAC_CTRL_LIFETIME_EN_LEN 1
    #define VRING_CFG_MAC_CTRL_LIFETIME_EN_MSK 0x1
    #define VRING_CFG_MAC_CTRL_AGGR_EN_POS 1
    #define VRING_CFG_MAC_CTRL_AGGR_EN_LEN 1
    #define VRING_CFG_MAC_CTRL_AGGR_EN_MSK 0x2
    U08    mac_ctrl_byte;

    #define VRING_CFG_TO_RESOLUTION_VALUE_POS 0
    #define VRING_CFG_TO_RESOLUTION_VALUE_LEN 6
    #define VRING_CFG_TO_RESOLUTION_VALUE_MSK 0x3F
    U08    to_resolution_byte;
    U08    agg_max_wsize;
    wmi_vring_cfg_schd_s    schd_params;
} POSTPACK wmi_vring_cfg_s;


enum wmi_vring_cfg_cmd_action_e {
    ADD_VRING = 0x0,
    MODIFY_VRING = 0x1,
    DELETE_VRING = 0x2
};

 typedef PREPACK struct  {
    U32    action;
    wmi_vring_cfg_s    vring_cfg;
} POSTPACK WMI_VRING_CFG_CMD;


/*
 * WMI_VRING_BA_EN_CMDID
 */
 typedef PREPACK struct  {
    U08    ringid;
    U08    agg_max_wsize;
    U16    ba_timeout;
} POSTPACK WMI_VRING_BA_EN_CMD;


/*
 * WMI_VRING_BA_DIS_CMDID
 */
 typedef PREPACK struct  {
    U08    ringid;
    U08    reserved0;
    U16    reason;
} POSTPACK WMI_VRING_BA_DIS_CMD;

/*
 * WMI_START_SCAN_CMDID
 */
enum wmi_scan_ctrl_cmd_scan_mode_e {
    ACTIVE_SCAN = 0x0,
    PASSIV_SCAN = 0x1,
    NO_SCAN = 0x2
};

 typedef PREPACK struct  {
    U16    in_channel_interval;
    U16    num_of_channels;
    U16    channel_list;
    U16    scan_param;
    U32    scan_mode;
} POSTPACK WMI_SCAN_CTRL_CMD;

/*
 * WMI_NOTIFY_REQ_CMDID
 */
 typedef PREPACK struct  {
    U32 cid;
    U32 interval_usec;
} POSTPACK WMI_NOTIFY_REQ_CMD;

/*
 * WMI_CFG_RX_CHAIN_CMDID
 */
enum wmi_sniffer_cfg_mode_e {
    SNIFFER_OFF = 0x0,
    SNIFFER_ON = 0x1
};
enum wmi_sniffer_cfg_phy_info_mode_e {
    SNIFFER_PHY_INFO_DISABLED = 0x0,
    SNIFFER_PHY_INFO_ENABLED = 0x1
};
enum wmi_sniffer_cfg_phy_support_e {
    SNIFFER_CP = 0x0,
    SNIFFER_DP = 0x1,
    SNIFFER_BOTH_PHYS = 0x2
};
 typedef PREPACK struct  {
    U32    mode;
    U32    phy_info_mode;
    U32    phy_support;
    U32    channel_id;
} POSTPACK wmi_sniffer_cfg_s;

enum wmi_cfg_rx_chain_cmd_action_e {
    ADD_RX_CHAIN = 0x0,
    DELETE_RX_CHAIN = 0x1
};
enum wmi_cfg_rx_chain_cmd_decap_trans_type_e {
    DECAP_TYPE_802_3 = 0x0,
    DECAP_TYPE_NATIVE_WIFI = 0x1
};
enum wmi_cfg_rx_chain_cmd_nwifi_ds_trans_type_e {
    NWIFI_RX_NO_TRANS_MODE = 0x0,
    NWIFI_RX_PBSS2AP_TRANS_MODE = 0x1,
    NWIFI_RX_PBSS2STA_TRANS_MODE = 0x2
};

 typedef PREPACK struct  {
    U32    action;
    wmi_sw_ring_cfg_s    rx_sw_ring;
    U08    mid;
    U08    decap_trans_type;

    #define CFG_RX_CHAIN_CMD_L2_802_3_OFFLOAD_CTRL_VLAN_TAG_INSERTION_POS 0
    #define CFG_RX_CHAIN_CMD_L2_802_3_OFFLOAD_CTRL_VLAN_TAG_INSERTION_LEN 1
    #define CFG_RX_CHAIN_CMD_L2_802_3_OFFLOAD_CTRL_VLAN_TAG_INSERTION_MSK 0x1
    U08    l2_802_3_offload_ctrl_byte;

    #define CFG_RX_CHAIN_CMD_L2_NWIFI_OFFLOAD_CTRL_REMOVE_QOS_POS 0
    #define CFG_RX_CHAIN_CMD_L2_NWIFI_OFFLOAD_CTRL_REMOVE_QOS_LEN 1
    #define CFG_RX_CHAIN_CMD_L2_NWIFI_OFFLOAD_CTRL_REMOVE_QOS_MSK 0x1
    #define CFG_RX_CHAIN_CMD_L2_NWIFI_OFFLOAD_CTRL_REMOVE_PN_POS 1
    #define CFG_RX_CHAIN_CMD_L2_NWIFI_OFFLOAD_CTRL_REMOVE_PN_LEN 1
    #define CFG_RX_CHAIN_CMD_L2_NWIFI_OFFLOAD_CTRL_REMOVE_PN_MSK 0x2
    U08 l2_nwifi_offload_ctrl_byte;

    U08    vlan_id;
    U08    nwifi_ds_trans_type;

    #define CFG_RX_CHAIN_CMD_L3_L4_CTRL_IPV4_CHECKSUM_EN_POS 0
    #define CFG_RX_CHAIN_CMD_L3_L4_CTRL_IPV4_CHECKSUM_EN_LEN 1
    #define CFG_RX_CHAIN_CMD_L3_L4_CTRL_IPV4_CHECKSUM_EN_MSK 0x1
    #define CFG_RX_CHAIN_CMD_L3_L4_CTRL_TCPIP_CHECKSUM_EN_POS 1
    #define CFG_RX_CHAIN_CMD_L3_L4_CTRL_TCPIP_CHECKSUM_EN_LEN 1
    #define CFG_RX_CHAIN_CMD_L3_L4_CTRL_TCPIP_CHECKSUM_EN_MSK 0x2
    U08    l3_l4_ctrl_byte;

    #define CFG_RX_CHAIN_CMD_RING_CTRL_OVERRIDE_PREFETCH_THRSH_POS 0
    #define CFG_RX_CHAIN_CMD_RING_CTRL_OVERRIDE_PREFETCH_THRSH_LEN 1
    #define CFG_RX_CHAIN_CMD_RING_CTRL_OVERRIDE_PREFETCH_THRSH_MSK 0x1
    #define CFG_RX_CHAIN_CMD_RING_CTRL_OVERRIDE_WB_THRSH_POS 1
    #define CFG_RX_CHAIN_CMD_RING_CTRL_OVERRIDE_WB_THRSH_LEN 1
    #define CFG_RX_CHAIN_CMD_RING_CTRL_OVERRIDE_WB_THRSH_MSK 0x2
    #define CFG_RX_CHAIN_CMD_RING_CTRL_OVERRIDE_ITR_THRSH_POS 2
    #define CFG_RX_CHAIN_CMD_RING_CTRL_OVERRIDE_ITR_THRSH_LEN 1
    #define CFG_RX_CHAIN_CMD_RING_CTRL_OVERRIDE_ITR_THRSH_MSK 0x4
    #define CFG_RX_CHAIN_CMD_RING_CTRL_OVERRIDE_HOST_THRSH_POS 3
    #define CFG_RX_CHAIN_CMD_RING_CTRL_OVERRIDE_HOST_THRSH_LEN 1
    #define CFG_RX_CHAIN_CMD_RING_CTRL_OVERRIDE_HOST_THRSH_MSK 0x8
    U08    ring_ctrl;

    U16    prefetch_thrsh;
    U16    wb_thrsh;
    U32    itr_value;
    U16    host_thrsh;
    U16    reserved;
    wmi_sniffer_cfg_s    sniffer_cfg;
} POSTPACK WMI_CFG_RX_CHAIN_CMD;

/*
 * WMI_RCP_ADDBA_RESP_CMDID
 */
 typedef PREPACK struct  {

    #define CIDXTID_CID_POS 0
    #define CIDXTID_CID_LEN 4
    #define CIDXTID_CID_MSK 0xF
    #define CIDXTID_TID_POS 4
    #define CIDXTID_TID_LEN 4
    #define CIDXTID_TID_MSK 0xF0
    U08 cidxtid_byte;

    U08    dialog_token;
    U16    status_code;
    U16    ba_param_set;                   /* ieee80211_ba_parameterset field to send */
    U16    ba_timeout;
} POSTPACK WMI_RCP_ADDBA_RESP_CMD;

/*
 * WMI_RCP_DELBA_CMDID
 */
 typedef PREPACK struct  {

    #define CIDXTID_CID_POS 0
    #define CIDXTID_CID_LEN 4
    #define CIDXTID_CID_MSK 0xF
    #define CIDXTID_TID_POS 4
    #define CIDXTID_TID_LEN 4
    #define CIDXTID_TID_MSK 0xF0
    U08 cidxtid_byte;

    U08    rsvd;
    U16    reason;
} POSTPACK WMI_RCP_DELBA_CMD;

/*
 * WMI_RCP_ADDBA_REQ_CMDID
 */
 typedef PREPACK struct  {

    #define CIDXTID_CID_POS 0
    #define CIDXTID_CID_LEN 4
    #define CIDXTID_CID_MSK 0xF
    #define CIDXTID_TID_POS 4
    #define CIDXTID_TID_LEN 4
    #define CIDXTID_TID_MSK 0xF0
    U08 cidxtid_byte;

    U08    dialog_token;
    U16    ba_param_set;                   /* ieee80211_ba_parameterset field as it received */
    U16    ba_timeout;
    U16    ba_seq_ctrl;                    /* ieee80211_ba_seqstrl field as it received */
} POSTPACK WMI_RCP_ADDBA_REQ_CMD;

/**
 *  WMI_SET_MAC_ADDRESS_CMDID
 */
typedef PREPACK struct  {
    U08 macaddr[WMI_MAC_LEN];
    U16 reserved;
} POSTPACK WMI_SET_MAC_ADDRESS_CMD;


/*
* WMI_EAPOL_TX_CMDID
*/
 typedef PREPACK struct  {
    U08 dst_mac[WMI_MAC_LEN];
    U16 eapol_len;
    U08 eapol[EMPTY_ARRAY_SIZE];
} POSTPACK WMI_EAPOL_TX_CMD;

/*
* WMI_ECHO_CMDID
*/
typedef PREPACK struct {
    U32    value;
} POSTPACK WMI_ECHO_CMD;

/**
 * **********************************************************************************************************************************
 *                                                          WMI Events
 * **********************************************************************************************************************************
 */

/*
 * List of Events (target to host)
 */
enum WMI_EVENT_ID {
    WMI_IMM_RSP_EVENTID            = 0x0000,
    WMI_READY_EVENTID                   = 0x1001,
    WMI_CONNECT_EVENTID                 = 0x1002,
    WMI_DISCONNECT_EVENTID              = 0x1003,
    WMI_SCAN_COMPLETE_EVENTID           = 0x100a,
    WMI_REPORT_STATISTICS_EVENTID       = 0x100b,
    WMI_RD_MEM_RSP_EVENTID         = 0x1800,
    WMI_FW_READY_EVENTID                = 0x1801,
    WMI_EXIT_FAST_MEM_ACC_MODE_EVENTID     = 0x0200,
    WMI_ECHO_RSP_EVENTID        = 0x1803,
    WMI_CONFIG_MAC_DONE_EVENTID         = 0x1805,
    WMI_CONFIG_PHY_DEBUG_DONE_EVENTID   = 0x1806,
    WMI_ADD_STATION_DONE_EVENTID    = 0x1807,
    WMI_ADD_DEBUG_TX_PCKT_DONE_EVENTID  = 0x1808,
    WMI_PHY_GET_STATISTICS_EVENTID      = 0x1809,
    WMI_FS_TUNE_DONE_EVENTID            = 0x180A,
    WMI_CORR_MEASURE_DONE_EVENTID       = 0x180B,
    WMI_TEMP_SENSE_DONE_EVENTID         = 0x180E,
    WMI_DC_CALIB_DONE_EVENTID           = 0x180F,
    WMI_IQ_TX_CALIB_DONE_EVENTID        = 0x1811,
    WMI_IQ_RX_CALIB_DONE_EVENTID        = 0x1812,
    WMI_SET_WORK_MODE_DONE_EVENTID      = 0x1815,
    WMI_LO_LEAKAGE_CALIB_DONE_EVENTID   = 0x1816,
    WMI_MARLON_R_ACTIVATE_DONE_EVENTID  = 0x1817,
    WMI_MARLON_R_READ_DONE_EVENTID      = 0x1818,
    WMI_MARLON_R_WRITE_DONE_EVENTID     = 0x1819,
    WMI_MARLON_R_TXRX_SEL_DONE_EVENTID  = 0x181A,
    WMI_SILENT_RSSI_CALIB_DONE_EVENTID  = 0x181D,

    WMI_CFG_RX_CHAIN_DONE_EVENTID    = 0x1820,
    WMI_VRING_CFG_DONE_EVENTID        = 0x1821,
    WMI_RX_ON_EVENTID            = 0x1822,
    WMI_BA_STATUS_EVENTID        = 0x1823,
    WMI_RCP_ADDBA_REQ_EVENTID        = 0x1824,
    WMI_ADDBA_RESP_SENT_EVENTID        = 0x1825,
    WMI_DELBA_EVENTID            = 0x1826,
    WMI_GET_SSID_EVENTID        = 0x1828,
    WMI_GET_PCP_CHANNEL_EVENTID            = 0x182a,
    WMI_SW_TX_COMPLETE_EVENTID        = 0x182b,

    WMI_READ_MAC_RXQ_EVENTID        = 0x1830,
    WMI_READ_MAC_TXQ_EVENTID        = 0x1831,
    WMI_WRITE_MAC_RXQ_EVENTID        = 0x1832,
    WMI_WRITE_MAC_TXQ_EVENTID        = 0x1833,
    WMI_WRITE_MAC_XQ_FIELD_EVENTID    = 0x1834,

    WMI_BEAFORMING_MGMT_DONE_EVENTID    = 0x1836,
    WMI_BF_TXSS_MGMT_DONE_EVENTID       = 0x1837,
    WMI_BF_RXSS_MGMT_DONE_EVENTID       = 0x1839,
    WMI_RS_MGMT_DONE_EVENTID            = 0x1852,
    WMI_RF_MGMT_STATUS_EVENTID            = 0x1853,
    WMI_BF_SM_MGMT_DONE_EVENTID         = 0x1838,
    WMI_RX_MGMT_PACKET_EVENTID    = 0x1840,

    /* Performance monitoring events */
    WMI_DATA_PORT_OPEN_EVENTID        = 0x1860,
    WMI_WBE_LINKDOWN_EVENTID        = 0x1861,

    WMI_BF_CTRL_DONE_EVENTID            = 0x1862,
    WMI_NOTIFY_REQ_DONE_EVENTID         = 0x1863,
    WMI_GET_STATUS_DONE_EVENTID         = 0x1864,

    WMI_UNIT_TEST_EVENTID               = 0x1900,
    WMI_FLASH_READ_DONE_EVENTID         = 0x1902,
    WMI_FLASH_WRITE_DONE_EVENTID        = 0x1903,


    WMI_SET_CHANNEL_EVENTID           = 0x9000,
    WMI_ASSOC_REQ_EVENTID              = 0x9001,
    WMI_EAPOL_RX_EVENTID             = 0x9002,
    WMI_MAC_ADDR_RESP_EVENTID         = 0x9003,
    WMI_FW_VER_EVENTID          = 0x9004,
} ;



/*******************************************************************************************************************
 *                                              Events data structures
 *******************************************************************************************************************/

/*
 * WMI_RF_MGMT_STATUS_EVENTID
 */
enum wmi_rf_status_e {
    WMI_RF_ENABLED            = 0x0,
    WMI_RF_DISABLED_HW      = 0x1,
    WMI_RF_DISABLED_SW      = 0x2,
    WMI_RF_DISABLED_HW_SW    = 0x3
};

 typedef PREPACK struct  {
    U32    rf_status;
} POSTPACK WMI_RF_MGMT_STATUS_EVENT;

/*
 * WMI_GET_STATUS_DONE_EVENTID
 */
 typedef PREPACK struct  {
    U32    is_associated;
    U32    cid;
    U16 bssid[3];
    U16    channel;
    U32    network_type;
    U32    ssid_len;
    U08    ssid[WMI_MAX_SSID_LEN];
    U32    rf_status;
    U32 is_secured;
} POSTPACK WMI_GET_STATUS_DONE_EVENT;



/**
 * WMI_FW_VER_EVENTID
 */
 typedef PREPACK struct  {
    U08 major;
    U08 minor;
    U16 subminor;
    U16 build;
} POSTPACK WMI_FW_VER_EVENT;



/*
* WMI_MAC_ADDR_RESP_EVENTID
*/
 typedef PREPACK struct  {
    U08 mac_addr[WMI_MAC_LEN];
    U08 auth_mode;
    U08 crypt_mode;
    U32 offload_mode; // 0/1
} POSTPACK WMI_MAC_ADDR_RESP_EVENT;


/*
* WMI_EAPOL_RX_EVENTID
*/
 typedef PREPACK struct  {
    U08 src_mac[WMI_MAC_LEN];
    U16 eapol_len;
    U08 eapol[EMPTY_ARRAY_SIZE];
} POSTPACK WMI_EAPOL_RX_EVENT;


/*
* WMI_READY_EVENTID
*/
 typedef PREPACK struct  {
    U32    sw_version;
    U32    abi_version;
    U08    macaddr[WMI_MAC_LEN];
    U08    phyCapability;              /* wmi_phy_capability_e */
    U08 reserved;
    U32 extend_mem_addr;
    U32 extend_mem_size;
} POSTPACK WMI_READY_EVENT;


/*
 * WMI_NOTIFY_REQ_DONE_EVENTID
 */
 typedef PREPACK struct  {
    U32 status;
    U32 tsf_low;
    U32 tsf_high;
    U32 snr_val;
    U32 tx_tpt;
    U32 tx_goodput;
    U32 rx_goodput;
    U16 bf_mcs;
    U16 my_rx_sector;
    U16 my_tx_sector;
    U16 other_rx_sector;
    U16 other_tx_sector;
    U16 range;
} POSTPACK WMI_NOTIFY_REQ_DONE_EVENT;

/*
 * WMI_CONNECT_EVENTID
 */
 typedef PREPACK struct  {
    U16    channel;
    U08    bssid[WMI_MAC_LEN];
    U16    listenInterval;
    U16    beaconInterval;
    U32    networkType;
    U08    beaconIeLen;
    U08    assocReqLen;
    U08    assocRespLen;
    U08    cid;
    U08    reserved0;
    U16    reserved1;
    U08    assocInfo[EMPTY_ARRAY_SIZE];
} POSTPACK WMI_CONNECT_EVENT;



/*
 * WMI_DISCONNECT_EVENTID
 */
enum wmi_disconnect_reason_e {
    NO_NETWORK_AVAIL        = 0x01,
    LOST_LINK               = 0x02,     /* bmiss */
    DISCONNECT_CMD          = 0x03,
    BSS_DISCONNECTED        = 0x04,
    AUTH_FAILED             = 0x05,
    ASSOC_FAILED            = 0x06,
    NO_RESOURCES_AVAIL      = 0x07,
    CSERV_DISCONNECT        = 0x08,
    INVALID_PROFILE         = 0x0a,
    DOT11H_CHANNEL_SWITCH   = 0x0b,
    PROFILE_MISMATCH        = 0x0c,
    CONNECTION_EVICTED      = 0x0d,
    IBSS_MERGE              = 0xe
};

 typedef PREPACK struct  {
    U16    protocolReasonStatus;  /* reason code, see 802.11 spec. */
    U08    bssid[WMI_MAC_LEN];    /* set if known */
    U08    disconnectReason ;      /* see wmi_disconnect_reason_e */
    U08    assocRespLen;
    U08    assocInfo[EMPTY_ARRAY_SIZE];
} POSTPACK WMI_DISCONNECT_EVENT;


/*
 * WMI_SCAN_COMPLETE_EVENTID
 */
 typedef PREPACK struct  {
    U32 status;
} POSTPACK WMI_SCAN_COMPLETE_EVENT;

/*
 * WMI_BA_STATUS_EVENTID
 */
enum wmi_vring_ba_status_e {
    AGREED = 0x0,
    NON_AGREED = 0x1,
} ;

 typedef PREPACK struct  {
    U16    status;
    U16    reserved0;
    U08    ringid;
    U08    agg_wsize;
    U16    ba_timeout;
} POSTPACK WMI_VRING_BA_STATUS_EVENT;


/*
 * WMI_DELBA_EVENTID
 */
 typedef PREPACK struct  {

    #define CIDXTID_CID_POS 0
    #define CIDXTID_CID_LEN 4
    #define CIDXTID_CID_MSK 0xF
    #define CIDXTID_TID_POS 4
    #define CIDXTID_TID_LEN 4
    #define CIDXTID_TID_MSK 0xF0
    U08 cidxtid_byte;

    U08    from_initiator;
    U16    reason;
} POSTPACK WMI_DELBA_EVENT;

/*
 * WMI_VRING_CFG_DONE_EVENTID
 */
enum wmi_vring_cfg_status {
    VRING_CFG_SUCCESS = 0x0,
    VRING_CFG_FAILURE = 0x1
};

 typedef PREPACK struct  {
    U08    ringid;
    U08    status;
    U16    reserved;
    U32    tx_vring_tail_ptr;
} POSTPACK WMI_VRING_CFG_DONE_EVENT;


/*
 * WMI_ADDBA_RESP_SENT_EVENTID
 */
enum wmi_rcp_addba_resp_sent_event_status_e {
    SUCCESS = 0x0,
    FAIL = 0x1
};

 typedef PREPACK struct  {

    #define CIDXTID_CID_POS 0
    #define CIDXTID_CID_LEN 4
    #define CIDXTID_CID_MSK 0xF
    #define CIDXTID_TID_POS 4
    #define CIDXTID_TID_LEN 4
    #define CIDXTID_TID_MSK 0xF0
    U08 cidxtid_byte;

    U08    rsvd;
    U16    status;
} POSTPACK WMI_RCP_ADDBA_RESP_SENT_EVENT;

/*
 * WMI_RCP_ADDBA_REQ_EVENTID
 */
 typedef PREPACK struct  {

    #define CIDXTID_CID_POS 0
    #define CIDXTID_CID_LEN 4
    #define CIDXTID_CID_MSK 0xF
    #define CIDXTID_TID_POS 4
    #define CIDXTID_TID_LEN 4
    #define CIDXTID_TID_MSK 0xF0
    U08 cidxtid_byte;

    U08    dialog_token;
    U16    ba_param_set;                   /* ieee80211_ba_parameterset field as it received */
    U16    ba_timeout;
    U16    ba_seq_ctrl;                    /* ieee80211_ba_seqstrl field as it received */
} POSTPACK WMI_RCP_ADDBA_REQ_EVENT;

/*
 * WMI_CFG_RX_CHAIN_DONE_EVENTID
 */
enum wmi_cfg_rx_chain_done_event_status_e {
    CFG_RX_CHAIN_SUCCESS = 0x1,
};

 typedef PREPACK struct  {
    U32    rx_ring_tail_ptr;               /* Rx V-Ring Tail pointer */
    U32    status;
} POSTPACK WMI_CFG_RX_CHAIN_DONE_EVENT;


/*
 * WMI_WBE_LINKDOWN_EVENTID
 */
enum wmi_wbe_link_down_event_reason_e {
    USER_REQUEST = 0x0,
    RX_DISASSOC = 0x1,
    BAD_PHY_LINK = 0x2,
};

 typedef PREPACK struct  {
    U08    cid;
    U08    reserved[3];
    U32    reason;
} POSTPACK WMI_WBE_LINK_DOWN_EVENT;

/*
 * WMI_DATA_PORT_OPEN_EVENTID
 */
 typedef PREPACK struct  {
    U08    cid;
    U08    reserved0[3];
} POSTPACK WMI_DATA_PORT_OPEN_EVENT;


/*
 * WMI_GET_PCP_CHANNEL_EVENTID
 */
 typedef PREPACK struct  {
    U08    channel_index;
    U08    reserved[3];
} POSTPACK WMI_GET_PCP_CHANNEL_EVENT;

/*
 * WMI_SW_TX_COMPLETE_EVENTID
 */
enum wmi_sw_tx_status_e {
    TX_SW_STATUS_SUCCESS                = 0x0,
    TX_SW_STATUS_FAILED_NO_RESOURCES    = 0x1,
    TX_SW_STATUS_FAILED_TX                = 0x2
};

typedef PREPACK struct  {
   U08    status;
   U08    reserved[3];
} POSTPACK WMI_SW_TX_COMPLETE_EVENT;

/*
 * WMI_GET_SSID_EVENTID
 */
 typedef PREPACK struct  {
    U32    ssid_len;
    U08    ssid[WMI_MAX_SSID_LEN];
} POSTPACK WMI_GET_SSID_EVENT;

/*
 * WMI_RX_MGMT_PACKET_EVENTID
 */
 typedef PREPACK struct  {
    U08    mcs;
    S08 snr;
    U16 range;
    U16    stype;
    U16    status;
    U32    length;
    U08    qid;                            /* Not resolved when == 0xFFFFFFFF  ==> Broadcast to all MIDS */
    U08    mid;                            /* Not resolved when == 0xFFFFFFFF  ==> Broadcast to all MIDS */
    U08    cid;
    U08    channel;                        /* From Radio MNGR */
} POSTPACK wmi_rx_mgmt_info_s;


typedef PREPACK struct  {
    wmi_rx_mgmt_info_s    info;
    U32    payload;//[EMPTY_ARRAY_SIZE];
} POSTPACK WMI_RX_MGMT_PACKET_EVENT;

/*
 * WMI_ECHO_RSP_EVENTID
 */
typedef PREPACK struct {
    U32    echoed_value;
} POSTPACK WMI_ECHO_EVENT;


#if defined(_MSC_VER)           // Microsoft
#pragma pack (pop, marlon_wmi)  /* restore original alignment from stack */
#endif


#endif /* MARLON_WMI_H_ */
