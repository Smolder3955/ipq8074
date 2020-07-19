/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Scan Measurement

GENERAL DESCRIPTION
  This file contains the implementation of LOWIScanMeasurement

Copyright (c) 2012 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <base_util/log.h>

#include <inc/lowi_scan_measurement.h>

using namespace qc_loc_fw;
const char* const LOWIScanMeasurement::TAG = "LOWIScanMeasurement";

////////////////////////
/// LOWIMeasurementInfo
////////////////////////
LOWIMeasurementInfo::LOWIMeasurementInfo()
{
  meas_age = -1;
  rtt_ps   = 0;
  rtt      = 0;
  rssi     = 0;
  rtt_timestamp  = 0;
  rssi_timestamp = 0;

  tx_bitrate = 0;
  tx_preamble = 0;
  tx_nss = 0;
  tx_bw = 0;
  tx_mcsIdx = 0;

  rx_bitrate = 0;
  rx_preamble = 0;
  rx_nss = 0;
  rx_bw = 0;
  rx_mcsIdx = 0;
}

////////////////////
// LOWIScanMeasurement
////////////////////
LOWIScanMeasurement::LOWIScanMeasurement ()
: msapInfo (NULL), lciInfo(NULL), lcrInfo(NULL)
{
  // Default values
  frequency                  = 0;
  band_center_freq1          = 0;
  band_center_freq2          = 0;
  info                       = 0;
  tsfDelta                   = 0;
  ranging_features_supported = 0;
  rttMeasTimeStamp           = 0;
  associatedToAp             = false;
  isSecure                   = false;
  type                       = NODE_TYPE_UNKNOWN;
  rttType                    = RTT1_RANGING;
  cellPowerLimitdBm          = CELL_POWER_NOT_FOUND;
  indoor_outdoor             =  ' ';
  targetStatus               = LOWI_TARGET_STATUS_FAILURE;
  memset(country_code, 0, LOWI_COUNTRY_CODE_LEN);
  measurementNum                  = 0;
  beaconPeriod                    = 0;
  beaconCaps                      = 0;
  num_frames_attempted            = 0;
  actual_burst_duration           = 0;
  negotiated_num_frames_per_burst = 0;
  retry_after_duration            = 0;
  negotiated_num_frames_per_burst = 0;
  location_features_supported     = 0;

}

LOWIScanMeasurement::~LOWIScanMeasurement ()
{
  for (unsigned int ii = 0; ii < measurementsInfo.getNumOfElements();
      ++ii)
  {
    delete measurementsInfo[ii];
  }

  if (NULL != msapInfo)
  {
    delete msapInfo;
  }
  if (NULL != lciInfo)
  {
    delete lciInfo;
  }
  if (NULL != lcrInfo)
  {
    delete lcrInfo;
  }
}

LOWILocationIE::LOWILocationIE()
{
  id = 0;
  len = 0;
}

LOWILocationIE::LOWILocationIE(const LOWILocationIE &rhs)
: id(rhs.id), len(rhs.len), locData(rhs.locData)
{
}

LOWILocationIE::~LOWILocationIE()
{
}

LOWIScanMeasurement::LOWIScanMeasurement(const LOWIScanMeasurement& rhs)
{
  this->msapInfo = NULL;
  this->lciInfo  = NULL;
  this->lcrInfo  = NULL;
  /* Use the '=' Operator to initialize new object */
  *this = rhs;
}

LOWIScanMeasurement& LOWIScanMeasurement::operator=( const LOWIScanMeasurement& rhs )
{
  if (this != &rhs)
  {
    this->measurementsInfo.flush();
    this->measurementNum = 0;
    bssid = rhs.bssid;
    frequency = rhs.frequency;
    band_center_freq1 = rhs.band_center_freq1;
    band_center_freq2 = rhs.band_center_freq2;
    associatedToAp    = rhs.associatedToAp;
    rttMeasTimeStamp  = rhs.rttMeasTimeStamp;
    info = rhs.info;
    tsfDelta = rhs.tsfDelta;
    ranging_features_supported = rhs.ranging_features_supported;
    isSecure = rhs.isSecure;
    type = rhs.type;
    rttType = rhs.rttType;
    // Do a deep copy for the measurementsInfo
    unsigned int size = rhs.measurementsInfo.getNumOfElements();
    // The vector might be empty
    if (0 != size)
    {
      for (unsigned int ii = 0; ii < size; ++ii)
      {
        LOWIMeasurementInfo* info = new (std::nothrow) LOWIMeasurementInfo();
        if (NULL != info)
        {
          info->meas_age = rhs.measurementsInfo[ii]->meas_age;
          info->rssi = rhs.measurementsInfo[ii]->rssi;
          info->rssi_timestamp = rhs.measurementsInfo[ii]->rssi_timestamp;
          info->rtt_timestamp = rhs.measurementsInfo[ii]->rtt_timestamp;
          measurementsInfo.push_back(info);
        }
        else
        {
          log_error(TAG, "Unexpected - Failed to copy LOWIMeasurementInfo");
        }
      }
    }

    ssid = rhs.ssid;
    delete msapInfo;
    msapInfo = NULL;
    if (NULL != rhs.msapInfo)
    {
      msapInfo = new (std::nothrow) LOWIMsapInfo;
      if (NULL != msapInfo)
      {
        msapInfo->protocolVersion = rhs.msapInfo->protocolVersion;
        msapInfo->serverIdx = rhs.msapInfo->serverIdx;
        msapInfo->venueHash = rhs.msapInfo->venueHash;
      }
    }
    cellPowerLimitdBm = rhs.cellPowerLimitdBm;
    measurementNum    = rhs.measurementNum;
    indoor_outdoor    = rhs.indoor_outdoor;
    targetStatus      = rhs.targetStatus;
    beaconPeriod      = rhs.beaconPeriod;
    beaconCaps        = rhs.beaconCaps;
    ieData            = rhs.ieData;
    num_frames_attempted  = rhs.num_frames_attempted;
    actual_burst_duration = rhs.actual_burst_duration;
    negotiated_num_frames_per_burst = rhs.negotiated_num_frames_per_burst;
    retry_after_duration  = rhs.retry_after_duration;
    negotiated_burst_exp  = rhs.negotiated_burst_exp;
    memcpy(country_code, rhs.country_code, LOWI_COUNTRY_CODE_LEN);
    delete lciInfo;
    lciInfo = NULL;
    if (NULL != rhs.lciInfo)
    {
      lciInfo = new (std::nothrow) LOWILocationIE(*(rhs.lciInfo));
    }
    delete lcrInfo;
    lcrInfo = NULL;
    if (NULL != rhs.lcrInfo)
    {
      lcrInfo = new (std::nothrow) LOWILocationIE(*(rhs.lcrInfo));
    }
    location_features_supported = rhs.location_features_supported;
  }
  return *this;
}


