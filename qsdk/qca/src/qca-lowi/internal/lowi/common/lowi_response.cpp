/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Response

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Response

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
#include <inc/lowi_response.h>

using namespace qc_loc_fw;

const char* const LOWIResponse::TAG = "LOWIResponse";

LOWICapabilities::LOWICapabilities ()
{
  discoveryScanSupported        = false;
  rangingScanSupported          = false;
  activeScanSupported           = false;
  oneSidedRangingSupported      = false;
  dualSidedRangingSupported11v  = false;
  dualSidedRangingSupported11mc = false;
  bwSupport                     = 0;
  preambleSupport               = 0;
  supportedCapablities          = LOWI_NO_SCAN_SUPPORTED;
}

////////////////////
// LOWIResponse
////////////////////
LOWIResponse::LOWIResponse (uint32 requestId)
{
  this->requestId = requestId;
}

LOWIResponse::~LOWIResponse()
{
}

uint32 LOWIResponse::getRequestId ()
{
  return this->requestId;
}

/////////////////////////////
// LOWICapabilityResponse
/////////////////////////////
LOWICapabilityResponse::LOWICapabilityResponse (uint32 requestId,
    LOWICapabilities capabilities, bool status)
: LOWIResponse (requestId)
{
  log_verbose (TAG, "LOWICapabilityResponse");
  mCapabilities = capabilities;
  mStatus = status;
}

LOWICapabilityResponse::~LOWICapabilityResponse ()
{
  log_verbose (TAG, "~LOWICapabilityResponse");
}

LOWIResponse::eResponseType
LOWICapabilityResponse::getResponseType ()
{
  return CAPABILITY;
}

LOWICapabilities LOWICapabilityResponse::getCapabilities ()
{
  return mCapabilities;
}

bool LOWICapabilityResponse::getStatus ()
{
  return mStatus;
}

/////////////////////////////
// LOWICacheResetResponse
/////////////////////////////
LOWICacheResetResponse::LOWICacheResetResponse
(uint32 requestId, bool status)
: LOWIResponse (requestId)
{
  log_verbose (TAG, "LOWICacheResetResponse");
  mCacheResetStatus = status;
}

LOWICacheResetResponse::~LOWICacheResetResponse ()
{
  log_verbose (TAG, "~LOWICacheResetResponse");
}

LOWIResponse::eResponseType
LOWICacheResetResponse::getResponseType ()
{
  return RESET_CACHE;
}

bool LOWICacheResetResponse::getStatus ()
{
  return mCacheResetStatus;
}
/////////////////////////////
// LOWIDiscoveryScanResponse
/////////////////////////////
LOWIDiscoveryScanResponse::LOWIDiscoveryScanResponse (uint32 requestId)
: LOWIResponse (requestId)
{
  log_verbose (TAG, "LOWIDiscoveryScanResponse");
  // Default values
  scanTypeResponse = WLAN_SCAN_TYPE_UNKNOWN;
  scanStatus = SCAN_STATUS_UNKNOWN;
  timestamp = 0;
}

LOWIDiscoveryScanResponse::~LOWIDiscoveryScanResponse ()
{
  log_verbose (TAG, "~LOWIDiscoveryScanResponse");

  // free up the memory held by the measurements
  for (unsigned int ii = 0; ii < scanMeasurements.getNumOfElements(); ++ii)
  {
    LOWIScanMeasurement* meas = scanMeasurements[ii];
    if (NULL != meas)
    {
      delete meas;
    }
  }
}

LOWIResponse::eResponseType
LOWIDiscoveryScanResponse::getResponseType ()
{
  return DISCOVERY_SCAN;
}

/////////////////////////////
// LOWIRangingScanResponse
/////////////////////////////
LOWIRangingScanResponse::LOWIRangingScanResponse
(uint32 requestId)
: LOWIResponse (requestId)
{
  log_verbose (TAG, "LOWIRangingScanResponse");
  // Default values
  scanStatus = SCAN_STATUS_UNKNOWN;
}

LOWIRangingScanResponse::~LOWIRangingScanResponse ()
{
  // Free the allocated memory here
  log_verbose (TAG, "~LOWIRangingScanResponse");

  // free up the memory held by the measurements
  for (unsigned int ii = 0; ii < scanMeasurements.getNumOfElements(); ++ii)
  {
    LOWIScanMeasurement* meas = scanMeasurements[ii];
    if (NULL != meas)
    {
      delete meas;
    }
  }

}

LOWIResponse::eResponseType
LOWIRangingScanResponse::getResponseType ()
{
  return RANGING_SCAN;
}

////////////////////////////////////////
// LOWIAsyncDiscoveryScanResultResponse
////////////////////////////////////////
LOWIAsyncDiscoveryScanResultResponse::
LOWIAsyncDiscoveryScanResultResponse (uint32 requestId)
: LOWIDiscoveryScanResponse (requestId)
{
  log_verbose (TAG, "LOWIAsyncDiscoveryScanResultResponse");
  // Default values
  scanTypeResponse = WLAN_SCAN_TYPE_UNKNOWN;
  scanStatus = SCAN_STATUS_UNKNOWN;
  timestamp = 0;
}

LOWIAsyncDiscoveryScanResultResponse::
~LOWIAsyncDiscoveryScanResultResponse ()
{
  // Scan measurements will be deleted by the parent destructor itself.
}

LOWIResponse::eResponseType
LOWIAsyncDiscoveryScanResultResponse::getResponseType ()
{
  return ASYNC_DISCOVERY_SCAN_RESULTS;
}

////////////////////////////////////////
// LOWIStatusResponse
////////////////////////////////////////
LOWIStatusResponse::LOWIStatusResponse(uint32 requestId)
: LOWIResponse(requestId)
{
  log_verbose (TAG, "LOWIStatusResponse");
}

LOWIStatusResponse::~LOWIStatusResponse()
{
  log_verbose (TAG, "~LOWIStatusResponse");
}

LOWIResponse::eResponseType
LOWIStatusResponse::getResponseType()
{
  return LOWI_STATUS;
}

