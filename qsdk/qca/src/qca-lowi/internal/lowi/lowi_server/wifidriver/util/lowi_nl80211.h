/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                LOWI NL80211 Library Header File
GENERAL DESCRIPTION
  This file contains the functions for Parsing and generating IEEE 802.11
  frames.

Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

History:

Date         User      Change
==============================================================================
08/24/2015   subashm   Created
=============================================================================*/

#ifndef LOWI_NL80211_H
#define LOWI_NL80211_H

#include "innavService.h"
#include <base_util/vector.h>

namespace qc_loc_fw
{

/* Global Defines */

#define VENDOR_SPEC_ELEM_LEN 255
#define BSSID_INFO_LEN 4

/* Neighbor Report Element Lengths */
#define NR_TSF_SUB_ELEM_LEN 4
#define NR_ELEM_MIN_LEN 13
#define NR_ELEM_HDR_LEN 2

/* Measurement request element length */
#define MEAS_REQ_ELEM_HDR_LEN 2

/* Element IDs */

#define RM_MEAS_REQ_ELEM_ID 38       /* Measurement Request ID */
#define RM_MEAS_RPT_ELEM_ID 39       /* Measurement Report ID */
#define RM_NEIGHBOR_RPT_ELEM_ID 52   /* Neighbor Report ID */
#define RM_WIDE_BW_CHANNEL_ELEM_ID 6 /* Wide Bandwidth channel ID*/

/* Neighbor Report Sub Element IDs */
#define NR_TSF_INFO_ELEM_ID 1
#define NR_CONDENSED_CTRY_STR_ELEM_ID 2
#define NR_BSS_TRANSITION_CAND_PREF_ELEM_ID 3
#define NR_BSS_TERMINATION_DUR_ELEM_ID 4
#define NR_BEARING_ELEM_ID 5
#define NR_MEAS_REPORT_ELEM_ID 39
#define NR_HT_CAP_ELEM_ID 45
#define NR_HT_OPERATION_ELEM_ID 61
#define NR_SEC_CHAN_OFFSET_ELEM_ID 62
#define NR_MEAS_PILOT_TRANS_ELEM_ID 66
#define NR_RM_ENABLED_CAP_ELEM_ID 70
#define NR_MULTI_BSSID_ELEM_ID 71
#define NR_VHT_CAP_ELEM_ID 191
#define NR_VHT_OPERATION_ELEM_ID 192
#define NR_VENDOR_SPEC_ELEM_ID 221

/* Action Catagories */
#define LOWI_WLAN_ACTION_RADIO_MEAS 5 /* Radio Measurement Action Catagory */

/* Radio Measurement Action Values*/
#define LOWI_RM_ACTION_REQ 0 /* Radio Measurement Request */
#define LOWI_RM_ACTION_RPT 1 /* Radio Measurement Report */
#define LOWI_NR_ACTION_REQ 4 /* Neighbor Report Request */

/* Measurement Request Types*/
#define LOWI_WLAN_LCI_REQ_TYPE 8        /* LCI Request Frame */
#define LOWI_WLAN_LOC_CIVIC_REQ_TYPE 11 /* Location Civic Request Frame */
#define LOWI_WLAN_FTM_RANGE_REQ_TYPE 16 /* Fine Timing Measurement Range Request Frame */

/* Defines for Field values */
#define LOWI_LOC_SUBJECT_REMOTE 1 /* Location Subject - Remote */

/* Frame Structures */

/* Neightbor Report Request Element */
typedef PACK(struct) _NeighborRequestElem
{
  uint8 catagory;
  uint8 radioMeasAction;
  uint8 dialogTok;
  // Optional SSID element goes here - defined seperately
  // Optional LCI Meas Req Element - defined seperately
  // Optional Loc Civic Req Element - defined seperately
} NeighborRequestElem;

/* Measurement Request Element */
typedef PACK(struct) _MeasReqElem
{
  uint8 elementId;
  uint8 len;
  uint8 measTok;
  uint8 measReqMode;
  uint8 measType;
  // Measurement Request - defined seperately
} MeasReqElem;

/* Fine Timing Measurement Range Request element */

typedef PACK (struct) _FtmrReqHead
{
  tANI_U16 randomInterval;
  tANI_U8 minApCount;
} FtmrReqHead;

typedef PACK (struct) _NeighborRprtElem
{
  tANI_U8 bssid[BSSID_SIZE];
  tANI_U8 bssidInfo[BSSID_INFO_LEN];
  tANI_U8 operatingClass;
  tANI_U8 channelNumber;
  tANI_U8 phyType;
} NeighborRprtElem;

typedef PACK (struct) _NR_TsfInfoElem
{
  tANI_U16 tsfOffset;
  tANI_U16 beaconInterval;
} NR_TsfInfoElem;

typedef PACK (struct) _NR_CountryStrElem
{
  tANI_U16 countryString;
} NR_CountryStrElem;

typedef PACK (struct) _MaxAgeElem
{
  tANI_U16 maxAge;
} MaxAgeElem;

typedef PACK (struct) _VendorSpecElem
{
  tANI_U8 vendorSpecContent[VENDOR_SPEC_ELEM_LEN];
} VendorSpecElem;

class FineTimingMeasRangeReq
{
public:
  uint8 dialogTok;
  MeasReqElem measReqElem;
  FtmrReqHead ftmrrReqHead;
  vector <NeighborRprtElem> neighborRprtElem;

  FineTimingMeasRangeReq()
  {
    memset(&measReqElem, 0, sizeof(measReqElem));
    memset(&ftmrrReqHead, 0, sizeof(ftmrrReqHead));
    neighborRprtElem.flush();
  }
  ~FineTimingMeasRangeReq()
  {
    memset(&measReqElem, 0, sizeof(measReqElem));
    memset(&ftmrrReqHead, 0, sizeof(ftmrrReqHead));
    neighborRprtElem.flush();
  }
};


/** Information related to successful range measurement with a single AP  */
typedef PACK(struct) _FtmrrRangeEntry
{
  /** Contains the least significant 4 octets of the TSF (synchronized with the
   *  associated AP) at the time (± 32 ?s) at which the initial Fine Timing
   *  Measurement frame was transmitted where the timestamps of both the frame
   *  and response frame were successfully measured.
   */
  uint32 measStartTime;
  /** BSSID of AP whose range is being reported */
  uint8 bssid[BSSID_SIZE];
  /** Estimated range between the requested STA and the AP using the fine timing
   *  measurement procedure, in units of 1/64 m. A value of 216–1 indicates a
   *  range of (216–1)/64 m or higher.
   */
  uint16 range;
  /**
   *  The Max Range Error field contains an upper bound for the error in the
   *  value specified in the Range field, in units of 1/64 m. A value of
   *  zero indicates an unknown error. A value of 216–1 indicates error of
   *  (216-1)/64 m or higher. For instance, a value of 128 in the Max Range
   *  Error field indicates that the value in the Range field has a maximum
   *  error of ± 2 m.
   */
  uint16 maxErrRange;
  /** Reserved field   */
  uint8  reserved;
} FtmrrRangeEntry;

/** Information related to failure range measurement with a single AP */
typedef PACK(struct) _FtmrrErrEntry
{
  /** Contains the least significant 4 octets of the TSF (synchronized with the
   *  associated AP) at the time (± 32 us) at which the Fine Timing Measurement
   *  failure was first detected.
   */
  uint32 measStartTime;
  /** BSSID of AP whose range is being reported */
  uint8 bssid[BSSID_SIZE];
  /** Error report code */
  uint8 errCode;
} FtmrrErrEntry;

typedef PACK(struct) _LciElemCom
{
  uint8 locSubject;
} LciElemCom;

typedef PACK(struct) _LocCivElemCom
{
  uint8 locSubject;
  uint8 civicType;
  uint8 locServiceIntUnits;
  uint16 locServiceInterval;
} LocCivElemCom;
} // namespace qc_loc_fw

#endif // LOWI_NL80211_H
