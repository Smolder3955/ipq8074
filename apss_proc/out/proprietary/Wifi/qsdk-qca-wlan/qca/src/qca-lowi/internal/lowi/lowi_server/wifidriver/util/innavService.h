/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                Innav Service Header File
GENERAL DESCRIPTION
  This file contains the functions for testing IWSS

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
All Rights Reserved.
Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2010-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.


History:

Date         User      Change
==============================================================================
11/20/2010   ns        Created
=============================================================================*/

#ifndef INNAV_SERVICE_H
#define INNAV_SERVICE_H

#ifdef __cplusplus
extern "C"
{
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <sched.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>

//#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/if_ether.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/wireless.h>
#include <osal/commontypes.h>

#define MAX_BSSIDS_ALLOWED_FOR_MEASUREMENTS 16
#define BSSID_SIZE 6

#define ifr_name    ifr_ifrn.ifrn_name    /* interface name     */

/* Common type definitions */
typedef uint8_t      tANI_U8;
typedef int8_t       tANI_S8;
typedef uint16_t     tANI_U16;
typedef int16_t      tANI_S16;
typedef uint32_t     tANI_U32;
typedef int32_t      tANI_S32;
typedef uint64_t     tANI_U64;
typedef int64_t      tANI_S64;
typedef tANI_U8   tSirMacAddr[6];

/*********************************************************
 IOCTL INTERFACE DEFINITIONS
 *********************************************************/
//Number to be added to reported RSSI for converting RSSI to dBm
#define LOWI_RSSI_ADJ_FACTOR -100
#define LOWI_RSSI_05DBM_UNITS(rssi) ((((int16) rssi) + LOWI_RSSI_ADJ_FACTOR) * 2)

#define WLAN_PRIV_SET_INNAV_MEASUREMENTS 0x8BF1
#define WLAN_PRIV_GET_INNAV_MEASUREMENTS 0x8BF3

/* The highest valid channel ID supported by Wi-Fi driver */
#define MAX_CHANNEL_ID                   165
/* The Maximum number of valid channels supported by Wi-Fi driver */
#define WNI_CFG_VALID_CHANNEL_LIST_LEN   256

#define BAND_2G_BEGIN 1
#define BAND_2G_END   14
#define BAND_5G_BEGIN 34
#define BAND_5G_END   165

#define IS_2G_CHANNEL(x) ((x >= BAND_2G_BEGIN) && (x <= BAND_2G_END))

#define LOWI_MAX_WLAN_FRAME_SIZE 2048
#define LOWI_WLAN_MAX_FRAMES 10

/* Frame header */

typedef PACK (struct) _Wlan80211FrameHeader
{
  tANI_U16 frameControl;
  tANI_U16 durationId;
  tANI_U8  addr1[BSSID_SIZE];
  tANI_U8  addr2[BSSID_SIZE];
  tANI_U8  addr3[BSSID_SIZE];
  tANI_U16 seqCtrl;
} Wlan80211FrameHeader;

typedef PACK(struct)
{
  tANI_U8 sourceMac[BSSID_SIZE + 2];
  tANI_U32 frameLen;
  tANI_U32 freq;
  tANI_U8 frameBody[LOWI_MAX_WLAN_FRAME_SIZE];
} WlanFrame;

typedef PACK (struct)
{
  tANI_U8 numFrames;
  WlanFrame wlanFrames[LOWI_WLAN_MAX_FRAMES];
} WlanFrameStore;

/*-------------------------------------------------------------------------
  WLAN_HAL_START_INNAV_MEAS_REQ
--------------------------------------------------------------------------*/
typedef enum
{
  eRTS_CTS_BASED = 1,
  eFRAME_BASED,
} tInNavMeasurementMode;

typedef PACK (struct)
{
  tSirMacAddr   bssid;
  tANI_U16      channel;
} tBSSIDChannelInfo;

typedef PACK (struct)
{
  /* Number of BSSIDs */
  tANI_U8                  numBSSIDs;
  /* Number of Measurements required */
  tANI_U8                  numInNavMeasurements;
  /*.Type of measurements (RTS-CTS or FRAME-BASED) */
  tANI_U16                 measurementMode;
  tANI_U16                 rtsctsTag; //reserved; /* rts/cts measurement tag */

  /* bssid channel info for doing the measurements */
  tBSSIDChannelInfo        bssidChannelInfo[MAX_BSSIDS_ALLOWED_FOR_MEASUREMENTS];

} tInNavMeasReqParams;

/*-------------------------------------------------------------------------
  WLAN_HAL_START_INNAV_MEAS_RSP
--------------------------------------------------------------------------*/
typedef PACK (struct) // 16 bytes
{
  tANI_U32     rssi;
  tANI_U16     rtt;
  tANI_U16     snr;
  tANI_U32     measurementTime;
  tANI_U32     measurementTimeHi;
} tRttRssiTimeData;

typedef PACK (struct) // 24 bytes
{
  tSirMacAddr         bssid;
  tANI_U8             numSuccessfulMeasurements;
  tANI_U8             channel;
  tRttRssiTimeData    rttRssiTimeData[1];
} tRttRssiResults;

typedef PACK (struct) // 36 bytes
{
  tANI_U16         numBSSIDs;
  tANI_U16         rspLen;
  tANI_U32         status;
  tANI_U32         rtsctsTag;
  tRttRssiResults  rttRssiResults[1];
} tInNavMeasRspParams, *tpInNavRspParams;

/*********************************************************
 INNAV - SPECIFIC - REQUEST STRUCTURES
 *********************************************************/

typedef enum
{
    RTS_CTS_MODE = 1,
    FRAME_BASED,
} eMeasurementMode;

struct BSSIDChannelInfo
{
    unsigned char            bssid[BSSID_SIZE]; //BSSID of the AP to be measured
    unsigned short           channelNumber; //Channel number of the AP to be measured
};

struct wips_innav_measurement_request
{
    unsigned char            numBSSIDs; //number of BSSIDs in the measurement set
    unsigned char            numInNavMeasurements; //number of rtt/rssi measurements per BSSID
    unsigned short           numSetRepetitions; //Number of times to measure the given BSSID set
    unsigned int             measurementTimeInterval; //Time interval between the measurement sets
    eMeasurementMode         measurementMode; //Indicates whether to use RTS/CTS or frame based measurements
    struct BSSIDChannelInfo  bssidChannelInfo[MAX_BSSIDS_ALLOWED_FOR_MEASUREMENTS]; //array of BSSID and channel info
};

/*********************************************************
 INNAV - SPECIFIC - RESPONSE STRUCTURES
 *********************************************************/

//BYTE Packed
struct wips_innav_rtt_rssi_snr_data
{
#ifdef ANI_BIG_BYTE_ENDIAN
    unsigned int      rssi                 :  7;
    unsigned int      rtt                  : 10;
    unsigned int      snr                  :  8;
    unsigned int      reserved             :  7;
#else
    unsigned int      reserved             :  7;
    unsigned int      snr                  :  8;
    unsigned int      rtt                  : 10;
    unsigned int      rssi                 :  7;
#endif
    unsigned int      measurementTimeLo        ;
    unsigned int      measurementTimeHi        ;
};

struct wips_innav_results
{
    unsigned char                       bssid[6];
    unsigned char                       numSuccessfulMeasurements;
    struct wips_innav_rtt_rssi_snr_data rttRssiSnrTimeData[1];
};

struct wips_innav_measurement_response
{
    unsigned char               numBSSIDs; //Number of BSSIDs for which the measurement was performed
    struct wips_innav_results   perBssidResultData[1]; //Pointer to the array of result data for each BSSID
};

/** Operating Phy Modes:
 *  The following enumeration lists out the various PHY
 *  Modes that the FW uses to setup the HW for Frame
 *  transmission. This mode differs depending on the Target's
 *  operating Channel/Bandwidth.
 */
typedef enum
{
  ROME_PHY_MODE_11A           = 0,  /* 11a Mode */
  ROME_PHY_MODE_11G           = 1,  /* 11b/g Mode */
  ROME_PHY_MODE_11B           = 2,  /* 11b Mode */
  ROME_PHY_MODE_11GONLY       = 3,  /* 11g only Mode */
  ROME_PHY_MODE_11NA_HT20     = 4,  /* 11na HT20 mode */
  ROME_PHY_MODE_11NG_HT20     = 5,  /* 11ng HT20 mode */
  ROME_PHY_MODE_11NA_HT40     = 6,  /* 11na HT40 mode */
  ROME_PHY_MODE_11NG_HT40     = 7,  /* 11ng HT40 mode */
  ROME_PHY_MODE_11AC_VHT20    = 8,  /* 5G 11ac VHT20 mode */
  ROME_PHY_MODE_11AC_VHT40    = 9,  /* 5G 11ac VHT40 mode */
  ROME_PHY_MODE_11AC_VHT80    = 10, /* 5G 11ac VHT80 mode */
  ROME_PHY_MODE_11AC_VHT20_2G = 11, /* 2G 11ac VHT20 mode */
  ROME_PHY_MODE_11AC_VHT40_2G = 12, /* 2G 11ac VHT40 mode */
  ROME_PHY_MODE_11AC_VHT80_2G = 13, /* 2G 11ac VHT80 mode */
  ROME_PHY_MODE_UNKNOWN       = 14,
  ROME_PHY_MODE_MAX           = 14
} ROME_PHY_MODE;

#ifdef __cplusplus
}
#endif
#endif
