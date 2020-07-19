/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                       WiFi Scan Interface Header file

GENERAL DESCRIPTION
  This file contains the structure declarations and function prototypes for
  Wifi scanner API

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
All Rights Reserved.
Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2010-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================*/
#ifdef __cplusplus
extern "C" {
#endif

#ifndef __WIFI_SCAN_H__
#define __WIFI_SCAN_H__

/*--------------------------------------------------------------------------
 * Include Files
 * -----------------------------------------------------------------------*/
#include <osal/common_inc.h>

/*--------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -----------------------------------------------------------------------*/

/* MAC ID size in bytes. */
#define WIFI_MAC_ID_SIZE 6
/* SSID size */
#define WIFI_SSID_SIZE 32
/* Country Code size in bytes */
#define COUNTRY_CODE_SIZE 2

/*--------------------------------------------------------------------------
 * Type Declarations
 * -----------------------------------------------------------------------*/
//MSG_TYPE_IMC_IWMM_REQUEST_IMMEDIATE_PASSIVE payload
//No Payload needed for this message

typedef enum
{
  TWO_POINT_FOUR_GHZ,
  FIVE_GHZ,
  BAND_ALL
} e_wlan_scan_band;

//Per AP Measurement data for Ranging Request
/** Defines supported RTT types for Ranging scan request */
typedef enum
{
  RTTV1_RANGING = 0,        /* Ranging with no Multipath correction           */
  RTTV2_RANGING,            /* Ranging with Multipath correction - Default    */
  RTTV3_RANGING,            /* Two-sided Ranging as defined in 802.11mc spec  */
  RTT_BEST_EFFORT_RANGING   /* One of the above, which ever the peer supports */
} eRttTypeOsal;

/** Defines the Bandwidth for Ranging scan request */
typedef enum
{
  RANGING_BW_20MHZ = 0,
  RANGING_BW_40MHZ,
  RANGING_BW_80MHZ,
  RANGING_BW_160MHZ
} e_wlan_bandwidth_ranging;


/** Defines the Preamble for Ranging scan request */
typedef enum
{
  RANGING_PREAMBLE_LEGACY = 0,
  RANGING_PREAMBLE_HT,
  RANGING_PREAMBLE_VHT
} e_wlan_preamble_ranging;

typedef struct
{
  uint8  u_bssid[6]; //BSSID of the AP to be measured
  uint16 w_channelNumber; //Channel number of the AP to be measured
  uint16 w_band_center_freq1; // Band center freq for 40/80/160 MHz BW
  uint16 w_band_center_freq2; // Band center freq of second lobe in 80 + 80 MHz BW mode
  e_wlan_scan_band e_band;  // Band that the channel belongs to
  eRttTypeOsal rttType;
  e_wlan_bandwidth_ranging  e_bandwidth;       // Bandwidth to be used to range with the AP
  e_wlan_preamble_ranging e_preamble; // Preamble to be used to range with the AP

  /*****************************************************************************
   * FTMR Related fields
   * Bit 0: ASAP = 0/1
   * Bit 1: LCI Req = True/False
   * Bit 2: Location Civic Req = True/False
   * Bits 3-7: Reserved
   * Bits 8-11: Number of Bursts Exponent
   * Bits 12-15: Burst Duration
   * Bits 16-31: Burst Period (time between Bursts)
   ****************************************************************************/
  #define FTM_ASAP_BIT 0
  #define FTM_LCI_BIT 1
  #define FTM_LOC_CIVIC_BIT 2
  #define FTM_BURSTS_EXP_START_BIT 8
  #define FTM_BURSTS_EXP_MASK 0xF
  #define FTM_BURST_DUR_START_BIT 12
  #define FTM_BURST_DUR_MASK 0xF
  #define FTM_BURST_PERIOD_START_BIT 16
  #define FTM_BURST_PERIOD_MASK 0xFFFF

  #define FTM_SET_ASAP(x) (x = (x | (1 << FTM_ASAP_BIT)))
  #define FTM_CLEAR_ASAP(x) (x = (x & ~(1 << FTM_ASAP_BIT)))
  #define FTM_GET_ASAP(x) ((x & (1 << FTM_ASAP_BIT)) >> FTM_ASAP_BIT)

  #define FTM_SET_LCI_REQ(x) (x = (x | (1 << FTM_LCI_BIT)))
  #define FTM_CLEAR_LCI_REQ(x) (x = (x & ~(1 << FTM_LCI_BIT)))
  #define FTM_GET_LCI_REQ(x) ((x & (1 << FTM_LCI_BIT)) >> FTM_LCI_BIT)

  #define FTM_SET_LOC_CIVIC_REQ(x) (x = (x | (1 << FTM_LOC_CIVIC_BIT)))
  #define FTM_CLEAR_LOC_CIVIC_REQ(x) (x = (x & ~(1 << FTM_LOC_CIVIC_BIT)))
  #define FTM_GET_LOC_CIVIC_REQ(x) ((x & (1 << FTM_LOC_CIVIC_BIT)) >> FTM_LOC_CIVIC_BIT)

  #define FTM_SET_BURSTS_EXP(x,y) (x = ((x & ~(FTM_BURSTS_EXP_MASK << FTM_BURSTS_EXP_START_BIT)) | (y << FTM_BURSTS_EXP_START_BIT)))
  #define FTM_GET_BURSTS_EXP(x) ((x & (FTM_BURSTS_EXP_MASK << FTM_BURSTS_EXP_START_BIT)) >> FTM_BURSTS_EXP_START_BIT)

  #define FTM_SET_BURST_DUR(x,y) (x = ((x & ~(FTM_BURST_DUR_MASK << FTM_BURST_DUR_START_BIT)) | (y << FTM_BURST_DUR_START_BIT)))
  #define FTM_GET_BURST_DUR(x) ((x & (FTM_BURST_DUR_MASK << FTM_BURST_DUR_START_BIT)) >> FTM_BURST_DUR_START_BIT)

  #define FTM_SET_BURST_PERIOD(x,y) (x = ((x & ~(FTM_BURST_PERIOD_MASK << FTM_BURST_PERIOD_START_BIT)) | (y << FTM_BURST_PERIOD_START_BIT)))
  #define FTM_GET_BURST_PERIOD(x) ((x & (FTM_BURST_PERIOD_MASK << FTM_BURST_PERIOD_START_BIT)) >> FTM_BURST_PERIOD_START_BIT)
  uint32 ftmRangingParams;
  uint32 numFramesPerBurst;
}t_iwmm_req_ap_info_type;

//Message payload for Ranging request
typedef struct
{
  uint16 rtsctsTag;  // unique RTS/CTS request tag for tracking
  uint8  w_num_aps;//number of BSSIDs in the measurement set
  t_iwmm_req_ap_info_type ap_info[1];
}t_iwmm_iwss_req_rts_cts_scan_type;


typedef enum
{
   // first message starts at index 0
   QUIPC_WLAN_SCAN_STATUS_FAILED= 0,
   QUIPC_WLAN_SCAN_STATUS_SUCCEEDED = 1,
} quipc_wlan_scan_status_e_type;

typedef enum
{
   QUIPC_WLAN_SCAN_TYPE_NONE    = 0,
   QUIPC_WLAN_SCAN_TYPE_PASSIVE = 1,
   QUIPC_WLAN_SCAN_TYPE_RTS_CTS = 2,
   QUIPC_WLAN_SCAN_TYPE_PASSIVE_AND_RTS_CTS = 3,
} quipc_wlan_scan_type_e_type;

//Per AP, Per measurement data for single measurement
#define AP_AGE_UNSPECIFIED_IN_MSECS -1

typedef struct
{
   int16 x_RSSI_0p5dBm;         /* In unit of 0.5 dBm */

   int32 l_measAge_msec;        /* In units of 1 milli-second, -1 means info not available */
   uint64 t_measTime_usec;      /* In micro seconds since January 1, 1970 */

   uint16 w_RTT_nsec;           /* RTT value in nanosecs, it will be deprecated soon
                                   everyone should use the below RTT value which is in psec*/
   uint32 q_RTT_psec;           /* RTT value in picosecs */

   /* TX Parameters */
   /** bitrate */
   uint32 tx_bitrate; /* in units of 100Kbps */
   /** preamble */
   uint8 tx_preamble;
   /** Number of spatial streams */
   uint8 tx_nss;
   /** bandwidth */
   uint8 tx_bw;
   /** Modulation Coding Scheme (MCS) index defines the
    * following:
    * 1) The Number of Streams used for TX & RX
    * 2) the Type of mudulation used
    * 3) the Coding Rate
    *
    * Note: Does not apply to legacy frames (frames using schemes
    * prior to 802.11n)
    * */
   uint8 tx_mcsIdx; /* does not apply to OFDM preambles */

   /* RX Parameters */
   /** bitrate */
   uint32 rx_bitrate; /* in units of 100Kbps */
   /** preamble */
   uint8 rx_preamble;
   /** Number of spatial streams */
   uint8 rx_nss;
   /** bandwidth */
   uint8 rx_bw;
   /** Modulation Coding Scheme (MCS) index defines the
   * following:
   * 1) The Number of Streams used for TX & RX
   * 2) the Type of mudulation used
   * 3) the Coding Rate
   *
   * Note: Does not apply to legacy frames (frames using schemes
   * prior to 802.11n)
   * */
   uint8 rx_mcsIdx; /* does not apply to OFDM preambles */
} quipc_wlan_ap_meas_info_s_type;

//  Per AP measurement block for single RTS CTS Scan (multiple measurements),
// or passive scan (single measurement)
typedef struct
{
  uint8                     u_MacId[6];

  uint8                     u_AssociatedToAp;

  /** BSSID's Channel Information */
  uint8                     u_Chan;              /* Channel ID */
  uint16                    u_Freq;              /* Channel Primary Frequency */
  uint16                    u_Band_center_freq1; /* Channel Center Frequency for whole BW (used for 40, 80 & 160MHz BW) */
  uint16                    u_Band_center_freq2; /* Channel Center frequency of second 80 MHz lobe for 80 + 80 BW */
  /** The following info field is a bitfield tht contains PHY mode
    * and flags for this bssid.
    * Bits 0-6 contian the PHY mode
    * Bits 7-13 contain the flags
    */
  uint32                    u_info;

  uint32                    u_tsfDelta;  /* Delta between local TSF and Target's TSF*/

  uint32                    u_Ranging_features_supported;/* Indicates if the BSSID supports 11mc - dual sided RTT */

  /** definitions used in location_features_supported  */
  #define WIPS_LCI_SUPPORTED_MASK       0x00000001
  #define WIPS_LOC_CIVIC_SUPPORTED_MASK 0x00000002
  uint32                    u_location_features_supported; /* Indicates if Target can provide LCI and LCR information */

  uint8                     u_numMeas;
  quipc_wlan_ap_meas_info_s_type* p_meas_info;

  boolean                   is_msap;
  boolean                   is_secure;
  uint8                     msap_prot_ver;
  uint8                     svr_idx;
  uint32                    venue_hash;

  /** Total RTT measurement Frames attempted */
  uint16 u_num_frames_attempted;

  /** Actual time taken by FW to finish one burst of
   * measurements (unit: ms) */
  uint16 u_actual_burst_duration;

  /** Number of "FTM frames per burst" negotiated with
   * peer/target. */
  uint8 u_negotiated_num_frames_per_burst;

  /** If Target/peer fails to accept an FTM session. Peer will
   * provide when it to retry FTM session. this field has the
   * time after which FTM session can be retried.
   * uint: seconds */
  uint8 u_retry_after_duration;

  /** Number of "FTM bursts" negotiated with peer/target.
   * This is indicated in the form of an exponent.
   * The number of bursts = 2^negotiated_burst_exp */
  uint8 u_negotiated_burst_exp;

  uint8                     u_lenSSID;
  uint8                     u_SSID[WIFI_SSID_SIZE+1];
  int8                      cell_power_limit_dBm;
  uint8                     country_code[COUNTRY_CODE_SIZE]; // Applications should not rely on this. Internal use only
  uint8                     indoor_outdoor; // Applications should not rely on this. Internal use only
  uint64                    tsf;  // Timestamp at which the beacon from the AP was last received. Applications should not rely on this at all as this will not be populated.
}quipc_wlan_ap_meas_s_type;

#define QUIPC_WLAN_MEAS_VALID_TIMESTAMP_USEC 0x00000001
#define QUIPC_WLAN_MEAS_VALID_SCAN_STATUS    0x00000002
#define QUIPC_WLAN_MEAS_VALID_SCAN_TYPE      0x00000004
#define QUIPC_WLAN_MEAS_VALID_NUM_APS        0x00000008
#define QUIPC_WLAN_MEAS_VALID_AP_MEAS        0x00000010

// In QUIPC, we decide that the memory needed by
// measurement will be continuous. So, the memory will
// be like the following
// first quipc_wlan_meas_s_type
// followed by w_num_aps of quipc_wlan_ap_meas_s_type
// followed by measurements for first ap (quipc_wlan_ap_meas_info_s_type)
// followed by measurements for second ap (quipc_wlan_ap_meas_info_s_type)
// ...
// followed by measurements for last ap (quipc_wlan_ap_meas_info_s_type)
typedef struct
{
   uint16                        w_valid_mask;
   /* Timestamp: heartbeat time */
   uint64                        t_timestamp_usec;
   /* scan status: this field will be invalid if IMC sends heartbeat to IPF
      without measurement report from IWMM */
   quipc_wlan_scan_status_e_type e_scan_status;

   /* scan type: this field will be invalid if IMC sends heartbeat to IPF
      without measurement report from IWMM */
   quipc_wlan_scan_type_e_type   e_scan_type;

   /* number of aps: this field will set to 0 if IMC sends heartbeat to IPF
      without measurement report from IWMM */
   uint16                        w_num_aps;
   /* Number of access points whose measurement is
      included here.Zero indicates that there are no
      new set of measurements in this period. If there
      are multiple measurment, p_ap_meas points to
      the first measurement, and the other measurement
      can be accessed via p_meas_info_ptr + i, where i
      is in the range of [0, w_num_aps-1]*/
   quipc_wlan_ap_meas_s_type*    p_ap_meas;
   uint32                        rtsctsTag;
} quipc_wlan_meas_s_type;

// These wlan capabilities are provided by LOWI.
typedef struct
{
  /** TRUE if discovery scan is supported */
  boolean discovery_scan_supported;
  /** TRUE if ranging scan is supported */
  boolean ranging_scan_supported;
  /** TRUE if active scan is supported, FALSE for passive scan */
  boolean active_scan_supported;
  /** TRUE is the capabilities are known, FALSE otherwise */
  boolean capabilities_known;
}osal_wlan_capabilities;

#endif /* __WIFI_SCAN_H__ */

#ifdef __cplusplus
}
#endif
