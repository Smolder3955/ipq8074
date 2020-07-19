/**************************************************************************
* Copyright © 2000-2002 Atheros Communications, Inc., All Rights Reserved
*
* $Id: //depot/sw/qca_main/components/wlan/qca-wifi-fw/1.2/drivers/host/tools/systemtools/src2/include/oids.h#1 $
*/

#ifndef _OIDS_H_
#define _OIDS_H_


/*
 * NOTE: All vendor-specific OIDs must begin with 0xFF...
 */
#define OID_ATH_XMIT_FILTERED               0xFF000000
#define OID_ATH_XMIT_RETRIES                0xFF000001
#define OID_ATH_XMIT_EXCESSIVE_RETRIES      0xFF000002
#define OID_ATH_XMIT_ACK_RSSI               0xFF000003
#define OID_ATH_XMIT_HW_ACKS_MISSING        0xFF000004
#define OID_ATH_XMIT_RTS_ERRORS             0xFF000005
#define OID_ATH_XMIT_REQUESTS               0xFF000006
#define OID_ATH_XMIT_REQUESTS_DENIED        0xFF000007
#define OID_ATH_XMIT_FIFO_UNDERRUNS         0xFF000008
#define OID_ATH_XMIT_DATA_RATE              0xFF000009
#define OID_ATH_RCV_RSSI                    0xFF00000A
#define OID_ATH_RCV_SW_FCS_ERRORS           0xFF00000B
#define OID_ATH_RCV_HW_FCS_ERRORS           0xFF00000C
#define OID_ATH_RCV_DECRYPT_ERRORS          0xFF00000D
#define OID_ATH_RCV_DUPLICATES              0xFF00000E
#define OID_ATH_RCV_MULTIPLE_DUPLICATES     0xFF00000F
#define OID_ATH_RCV_FIFO_OVERRUNS           0xFF000010
#define OID_ATH_RCV_DATA_RATE               0xFF000011
#define OID_ATH_RCV_MULTICAST_FRAMES        0xFF000012
#define OID_ATH_CURRENT_CHANNEL             0xFF000013
#define OID_ATH_TURBO_MODE                  0xFF000014
#define OID_ATH_STATION_STATE               0xFF000015
#define OID_ATH_POWERSAVE_STATE             0xFF000016
#define OID_ATH_BSSID                       0xFF000017
#define OID_ATH_SSID                        0xFF000018
#define OID_ATH_BSSID_LIST                  0xFF000019
#define OID_ATH_ENCRYPTION_ALG              0xFF00001A
#define OID_ATH_NETWORK_TYPE                0xFF00001B
#define OID_ATH_REGULATORY_DOMAIN           0xFF00001C   // Regulatory Domain from app
#define OID_ATH_DEVICE_INFO                 0xFF00001D   // Device IDs and revs
#define OID_ATH_DIAG_STATISTICS             0xFF00001E   // Network statistics
#define OID_ATH_COUNTRY                     0xFF00001F   // Country code
#define OID_ATH_NOISE_FLOOR                 0xFF000020   // Radio's noise floor
#define OID_ATH_DRIVER_UPTIME               0xFF000021   // Up time in seconds
#define OID_ATH_REGULATORY                  0xFF000022   // EEPROM Regulatory domain
#define OID_ATH_TRANSMIT_POWER              0xFF000023   // Radio's 6mb transmit power
#define OID_ATH_ANTENNA_SWITCH              0xFF000024   // Enable/disable diversity
#define OID_ATH_DEVICE_TYPE                 0xFF000025   // Device Type
#define OID_ATH_AP_POWER_CONTROL            0xFF000026   // AP is controlling power levels
#define OID_ATH_MAX_TX_POWER                0xFF000027   // Max TxPower possible on this channel
#define OID_ATH_SERIAL_NUMBER               0xFF000028   // Return Device Serial number string from EEPROM
#define OID_ATH_SSID_LIST                   0xFF000029
#define OID_ATH_SIGNAL_QUALITY              0xFF00002A
#define OID_ATH_BSS_INFO_LEN                0xFF00002B 
#define OID_ATH_CONN_INFO                   0xFF00002C
#define OID_ATH_QOS_INFO                    0xFF00002D


#define OID_ATH_IS_SCAN_DONE                0xFF000030

#define OID_ATH_DIAG_HUNG_COUNT             0xFF100000
#define OID_ATH_DIAG_NDIS_RESETS            0xFF100001
#define OID_ATH_DIAG_SELF_RESETS            0xFF100002
#define OID_ATH_DIAG_RESET                  0xFF100003
#define OID_ATH_DIAG_EVERYTHING             0xFF100004
#define OID_ATH_VER_USERNAME                0xFF100005
#define OID_ATH_VER_COMPUTERNAME            0xFF100006
#define OID_ATH_VER_PROJDIR                 0xFF100007
#define OID_ATH_VER_CONFIG                  0xFF100008
#define OID_ATH_DIAG_TX_PENDED              0xFF100009
#define OID_ATH_DIAG_RESOURCE_AVAIL         0xFF10000a
#define OID_ATH_DIAG_STAT1                  0xFF10000b
#define OID_ATH_MIC_ALG                     0xFF10000c
#define OID_ATH_DOT11_AUTH_ALG              0xFF10000d

#define OID_ATH_SLEEP_TRACK                 0xFF100100
#define OID_ATH_POWER_STATE                 0xFF100101

/* Compression Performance Counters / Stats */
#define OID_ATH_COMP_CPC0                   0xFF100200
#define OID_ATH_COMP_CPC1                   0xFF100201
#define OID_ATH_COMP_CPC2                   0xFF100202
#define OID_ATH_COMP_CPC3                   0xFF100203
#define OID_ATH_COMP_SUCCESSES              0xFF100204

#define OID_ATH_AP_IPADDR                   0xFF100205


/*
 * the following are NDIS OIDs that are duplicated as Atheros vendor-specific
 * OIDs to circumvent the problem in 98SE and ME where NDIS isn't exporting
 * its MSNdis_xxx WMI classes
 */
#define OID_ATH_GEN_DRIVER_VERSION          0xFF200000
#define OID_ATH_GEN_VENDOR_DRIVER_VERSION   0xFF200001
#define OID_ATH_802_3_CURRENT_ADDRESS       0xFF200002
#define OID_ATH_802_3_PERMANENT_ADDRESS     0xFF200003

/*
 * the following are NDIS 5.1 802_11 OIDs that are duplicated as 
 * Atheros vendor-specific OIDs to be used in NDIS 5.0 through WMI
 */
#define OID_ATH_802_11_BSSID                    0xFF210000
#define OID_ATH_802_11_SSID                     0xFF210001
#define OID_ATH_802_11_NETWORK_TYPES_SUPPORTED  0xFF210002
#define OID_ATH_802_11_NETWORK_TYPE_IN_USE      0xFF210003
#define OID_ATH_802_11_RSSI                     0xFF210004
#define OID_ATH_802_11_INFRASTRUCTURE_MODE      0xFF210005
#define OID_ATH_802_11_SUPPORTED_RATES          0xFF210006
#define OID_ATH_802_11_CONFIGURATION            0xFF210007
#define OID_ATH_802_11_ADD_WEP                  0xFF210008
#define OID_ATH_802_11_REMOVE_WEP               0xFF210009
#define OID_ATH_802_11_DISASSOCIATE             0xFF21000A
#define OID_ATH_802_11_BSSID_LIST               0xFF21000B
#define OID_ATH_802_11_AUTHENTICATION_MODE      0xFF21000C
#define OID_ATH_802_11_PRIVACY_FILTER           0xFF21000D
#define OID_ATH_802_11_BSSID_LIST_SCAN          0xFF21000E
#define OID_ATH_802_11_WEP_STATUS               0xFF21000F
#define OID_ATH_802_11_RELOAD_DEFAULTS          0xFF210010

#define OID_ATH_NT4_NDIS_REQUEST                0xFF220000

#ifdef DIAN
#define OID_GEN_TCOV_STATUS	                0xFF220001  // For Dian
#define OID_GEN_TCOV_NEGOTIATE                  0xFF220002  // For Dian
#define OID_GEN_TCOV_CONSOLE_DATA               0xFF220003  // For Dian
#endif

typedef enum athNdisRequestType
{
    AthNdisRequestQuery,
    AthNdisRequestSet,
} ATH_NDIS_REQUEST_TYPE;
 
/*
 * ATH_NDIS_REQUEST
 * Information Buffer must appear directly following this structure.
 */
typedef struct athNdisRequest {
    ATH_NDIS_REQUEST_TYPE   requestType;
    ULONG                   oid;
    PVOID                   pInfoBuffer;        // Information Buffer must appear directly following this structure
    UINT                    infoBufferLength;
    UINT                    bytesReadOrWritten; // Filled in by driver. Written for query.  Read for Set.
    UINT                    bytesNeeded;        // Filled in by driver
} ATH_NDIS_REQUEST;

/*
 * the following NDIS OIDs are used as commands i.e. for performing
 * operations on the driver
 */
#define OID_ATH_RADIO_ENABLE                    0xFF300000
#define OID_ATH_WLAN_CONFIG                     0xFF300001

/*
 * Version of config structure. Must be incremented when the 
 * config structure is changed.
 */
typedef enum athWlanConfigVersion {
        ATH_WLAN_CONFIG_VERSION_1 = 1,
        ATH_WLAN_CONFIG_VERSION_2 = 2,
        ATH_WLAN_CONFIG_VERSION_3 = 3,
        ATH_WLAN_CONFIG_VERSION_4 = 4,
        ATH_WLAN_CONFIG_VERSION_5 = 5,
        ATH_WLAN_CONFIG_VERSION_6 = 6,
        ATH_WLAN_CONFIG_VERSION_7 = 7
} ATH_WLAN_CONFIG_VERSION_TYPE;

#define ATH_WLAN_CONFIG_VERSION_CURRENT   ATH_WLAN_CONFIG_VERSION_7

/*
 * Version of Atheros5000_BssInfo Structure. Increment it when the structure
 * changes. There was no Version 1 , therefore version 1 is derived from the 
 * size passed
 */
typedef enum athBssInfoVersion {
    ATH_BSS_INFO_VERSION_1 = 1,
    ATH_BSS_INFO_VERSION_2 = 2
} ATH_BSS_INFO_VERSION_TYPE;

#define ATH_BSS_INFO_VERSION_CURRENT   ATH_BSS_INFO_VERSION_2
/*
 * This is the maximum number of BSSs for which the apps allocate space. Ideally it should not
 * be here. Since there was no versioning of the Atheros5000_BssInfo structure, there is no 
 * way to know the previous version. This number is used to compare the sizes of the 
 * Atheros5000_BssInfo structure to arrive at the pre-version-2 version.
 */
#define NUM_MAX_BSS_REQ 50

typedef enum athConnInfoVersion {
    ATH_CONN_INFO_VERSION_1 = 1
} ATH_CONN_INFO_VERSION_TYPE;

#define ATH_CONN_INFO_VERSION_CURRENT ATH_CONN_INFO_VERSION_1

typedef enum athQosInfoVersion {
    ATH_QOS_INFO_VERSION_1 = 1
} ATH_QOS_INFO_VERSION_TYPE;

#define ATH_QOS_INFO_VERSION_CURRENT   ATH_QOS_INFO_VERSION_1

/*
 * The various Connection Modes
 */
#define CONN_SUPER_MODE_BIT_POS     0
#define CONN_XR_MODE_BIT_POS        1

#ifdef RGTEST
#define OID_ATH_RGTEST_PROTOCOL_CONNECTED       0xFF400000
#define OID_ATH_RGTEST_PREPEND_CHAN_INFO        0xFF400001
#define OID_ATH_RGTEST_PHY_INFO                 0xFF400002
#define OID_ATH_RGTEST_ANTENNA                  0xFF400003
#define OID_ATH_RGTEST_SET_RATE                 0xFF400004
#endif /* #ifdef RGTEST */

#define OID_ATH_RGTEST_GET_RATE_LOG_HEADER      0xFF400007
#define OID_ATH_RGTEST_GET_RATE_LOG             0xFF400008

#define OID_ATH_RGTEST_GEN_TSPEC                0xFF400009
#define OID_ATH_RGTEST_ACK_POLICY               0xFF40000A

/*
 * Meeting House OID Definitions
 */
#define OID_MH_802_1X_SUPPORTED                 0xFFEDC100
#define OID_MH_SUPP_AUTH_STATUS                 0xFFEDC101
#define OID_MH_CCX_LEAP                         0xFFEDC102
#define OID_MH_OID_CCX_MIXED_CELLS              0xFFEDC103 /* Not used */
#define OID_MH_CCX_8021X                        0xFFEDC105
#define OID_MH_CCX_8021X_RESULT                 0xFFEDC106
#define OID_MH_CCX_ADD_KRK                      0xFFEDC107
#define OID_MH_CCX_REMOVE_KRK                   0xFFEDC108

#define OID_ATH_SUPL_AUTH_STATUS                0xFFEDC200

#endif /* #ifndef _OIDS_H_ */
