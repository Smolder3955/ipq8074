#ifdef __cplusplus
extern "C"
{
#endif
#ifndef __WLAN_CAPABILITIES_H__
#define __WLAN_CAPABILITIES_H__
/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

   WLAN Capabilities Interface Header file

   GENERAL DESCRIPTION
   This component performs Unit testing of Ranging scan interface in LOWI.

Copyright (c) 2014, 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================*/
#include "innavService.h"

/**
 * Wi-Fi IOCTL command for Capabilities
 */
#define SIOCIWFIRSTPRIV            0x8BE0
#define WLAN_PRIV_GET_OEM_DATA_CAP (SIOCIWFIRSTPRIV + 5)
#define WE_GET_OEM_DATA_CAP        13  // IOCTL Sub Command ID

/**
 * Wi-Fi OEM signature and length
 */
#define OEM_TARGET_SIGNATURE_LEN 8
#define OEM_TARGET_SIGNATURE "QUALCOMM"

/**
 * Wi-Fi Driver Version
 */
typedef PACK (struct)
{
  tANI_U8 major;
  tANI_U8 minor;
  tANI_U8 patch;
  tANI_U8 build;
} tDriverVersion;

/**
 * Wi-Fi Chip Identity
 */
#define TARGET_TYPE_ROME_1_1    8    // ROME 1.1
#define TARGET_TYPE_ROME_1_3    10   // ROME 1.3
#define TARGET_TYPE_ROME_2_1    12   // ROME 2.1

/**
 * Wi-Fi OEM Capability Info Structure
 */
typedef PACK (struct)
{
  tANI_U8        oem_target_signature[OEM_TARGET_SIGNATURE_LEN];
  tANI_U32       oem_target_type;
  tANI_U32       oem_fw_version;
  tDriverVersion driver_version;
  tANI_U16       allowed_dwell_time_min;
  tANI_U16       allowed_dwell_time_max;
  tANI_U16       curr_dwell_time_min;
  tANI_U16       curr_dwell_time_max;
  tANI_U16       supported_bands;
  tANI_U16       num_channels;
  tANI_U8        channel_list[WNI_CFG_VALID_CHANNEL_LIST_LEN];
} IwOemDataCap;


/*-----------Rome RTT Capabilities Response Message--------*/
typedef PACK (struct)
{
  /* Supproted Version/Types of RTT
   * bit 0:       Single Sided RTT Enabled/Disabled
   * bit 1:       11mc prototype Double Sided RTT Enabled/Disabled
   * bit 2:       11mc D4.0 Double Sided RTT Enabled/Disabled
   * bit 3:7      reserved
   */
  #define CAP_SINGLE_SIDED_RANGING 0x1
  #define CAP_DOUBLE_SIDED_RANGING 0x2
  #define CAP_11MC_DOUBLE_SIDED_RANGING 0x4
  tANI_U8 rangingTypeMask;

  /* Supported RTT Frames
   * bit 0:       NULL Frame
   * bit 1:       QoS NULL Frame
   * bit 2:       TMR/TM Frame
   * bit 3:       F-TMR/FTM Frame
   * bit 4:       RTS/CTS
   * bit 5:7      Reserved
   */
  #define CAP_NULL_FRAME      0x1
  #define CAP_QOS_NULL_FRAME  0x2
  #define CAP_TMR_TM_FRAMES   0x4
  #define CAP_FTMR_FTM_FRAMES 0x8
  #define CAP_RTS_CTS_FRAMES  0x10
  tANI_U8 supportedFramesMask;

  /* Maximum number of RTT destinations (such as AP, STA, P2P-Client, P2P-Go)
   * per measurement request */
  tANI_U8 maxDestPerReq;

  /* Maximum number of Measurements Frames per destination  */
  tANI_U8 maxMeasPerDest;

  /* In one measurement command, how many channels can destinations reside on */
  tANI_U8 maxChannelsAllowed;

  /*
   * Maximum Allowed Bandwidth:
   * 0 - 20 MHz
   * 1 - 40 MHz
   * 2 - 80 MHz
   * 3 - 160 MHZ
   */
  #define RTT_CAP_BW_20     0x0
  #define RTT_CAP_BW_40     0x1
  #define RTT_CAP_BW_80     0x2
  #define RTT_CAP_BW_160    0x3
  tANI_U8 maxBwAllowed;

  /* Preambles Supported:
   * bit 0   - Legacy
   * bit 1   - HT
   * bit 2   - VHT
   * bit 3:7 - Reserved
   */
  #define CAP_PREAMBLE_LEGACY 0x1
  #define CAP_PREAMBLE_HT     0x2
  #define CAP_PREAMBLE_VHT    0x4
  tANI_U8 preambleSupportedMask;

  /* Report Types Supported:
   * bit 0   - Report per frame with CFR
   * bit 1   - Report per frame without CFR
   * bit 2   - Aggregate Reoprt without CFR
   * bit 3:7 - Reserved
   */
  #define CAP_REPORT_0  0x1
  #define CAP_REPORT_1  0x2
  #define CAP_REPORT_2  0x4
  tANI_U8 reportTypeSupportedMask;

  /* Bit field mask indicating the valid RF chains */
  tANI_U8 maxRfChains;

  /* Supported FAC:
   * bit 0 - No FAC
   * bit 1 - SW IFFT
   * bit 2 - HW IFFT
   * bit 3:7 - Reserved
   */
  #define CAP_NO_FAC  0x1
  #define CAP_SW_IFFT 0x2
  #define CAP_HW_IFFT 0x4
  tANI_U8 facTypeMask;

  /* Bit field mask indicating the valid PHYs */
  tANI_U8 numPhys;
} RomeRttCapabilities;

/* Access Macros for RTT Capabilities - bit fields only */
#define CAP_SINGLE_SIDED_RTT_SUPPORTED(X) (X & CAP_SINGLE_SIDED_RANGING)
#define CAP_DOUBLE_SIDED_RTT_SUPPORTED(X) (X & CAP_DOUBLE_SIDED_RANGING)
#define CAP_11MC_DOUBLE_SIDED_RTT_SUPPORTED(X) (X & CAP_11MC_DOUBLE_SIDED_RANGING)

#define CAP_NULL_FRAME_SUPPORTED(X)       (X & CAP_NULL_FRAME)
#define CAP_QOS_NULL_FRAME_SUPPORTED(X)   (X & CAP_QOS_NULL_FRAME)
#define CAP_TMR_TM_FRAMES_SUPPORTED(X)    (X & CAP_TMR_TM_FRAMES)
#define CAP_FTMR_FTM_FRAMES_SUPPORTED(X)  (X & CAP_FTMR_FTM_FRAMES)
#define CAP_RTS_CTS_FRAMES_SUPPORTED(X)   (X & CAP_RTS_CTS_FRAMES)

#define CAP_PREAMBLE_LEGACY_SUPPORTED(X)  (X & CAP_PREAMBLE_LEGACY)
#define CAP_PREAMBLE_HT_SUPPORTED(X)      (X & CAP_PREAMBLE_HT)
#define CAP_PREAMBLE_VHT_SUPPORTED(X)     (X & CAP_PREAMBLE_VHT)

#define CAP_REPORT_0_SUPPORTED(X)         (X & CAP_REPORT_0)
#define CAP_REPORT_1_SUPPORTED(X)         (X & CAP_REPORT_1)
#define CAP_REPORT_2_SUPPORTED(X)         (X & CAP_REPORT_2)

#define CAP_NO_FAC_SUPPORTED(X)           (X & CAP_NO_FAC)
#define CAP_SW_IFFT_SUPPORTED(X)          (X & CAP_SW_IFFT)
#define CAP_HW_IFFT_SUPPORTED(X)          (X & CAP_HW_IFFT)

#endif //#ifndef __WLAN_CAPABILITIES_H__

#ifdef __cplusplus
}
#endif
