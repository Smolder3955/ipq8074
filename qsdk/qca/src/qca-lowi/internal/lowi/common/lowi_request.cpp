/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Request

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Request

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
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

#include <inc/lowi_request.h>
#include <common/lowi_utils.h>
#include <../../osal/os_api.h>

using namespace qc_loc_fw;

const char *const LOWIRequest::TAG = "LOWIRequest";

////////////////////
// LOWIRequest
////////////////////
LOWIRequest::LOWIRequest(uint32 requestId)
  : mRequestOriginator(NULL)
{
  log_debug(TAG, "LOWIRequest - request ID = %d", requestId);
  mRequestId = requestId;
}

LOWIRequest::~LOWIRequest()
{
  log_verbose(TAG, "~LOWIRequest");
  if (NULL != mRequestOriginator)
  {
    delete[] mRequestOriginator;
  }
}

LOWIRequest::LOWIRequest(const LOWIRequest& rhs)
{
  mRequestOriginator = new char[strlen(rhs.mRequestOriginator) + 1];
  strlcpy(mRequestOriginator, rhs.mRequestOriginator,
          sizeof(mRequestOriginator));
  mRequestId = rhs.mRequestId;
}

LOWIRequest& LOWIRequest::operator = (const LOWIRequest& rhs)
{
  char *tempName = new char[strlen(rhs.mRequestOriginator) + 1];
  delete[] mRequestOriginator;
  mRequestOriginator = tempName;
  strlcpy(mRequestOriginator, rhs.mRequestOriginator,
          sizeof(mRequestOriginator));
  mRequestId = rhs.mRequestId;
  return *this;
}

const char* LOWIRequest::getRequestOriginator() const
{
  return this->mRequestOriginator;
}

void LOWIRequest::setRequestOriginator(const char *const request_originator)
{
  int len = strlen(request_originator) + 1;
  this->mRequestOriginator = new(std::nothrow)
                             char[len];
  if (NULL == mRequestOriginator)
  {
    log_error(TAG, "Invalid request as the Originator could not be set");
    return;
  }
  strlcpy(mRequestOriginator, request_originator, len);
}

uint32 LOWIRequest::getRequestId() const
{
  return this->mRequestId;
}

/////////////////////////////
// LOWICapabilityRequest
/////////////////////////////
LOWICapabilityRequest::LOWICapabilityRequest(uint32 requestId)
  : LOWIRequest(requestId)
{
  log_verbose(TAG, "LOWICapabilityRequest");
}
/** Destructor*/
LOWICapabilityRequest::~LOWICapabilityRequest()
{
  log_verbose(TAG, "~LOWICapabilityRequest");
}

LOWIRequest::eRequestType
LOWICapabilityRequest::getRequestType() const
{
  return CAPABILITY;
}

/////////////////////////////
// LOWICacheResetRequest
/////////////////////////////
LOWICacheResetRequest::LOWICacheResetRequest(uint32 requestId)
  : LOWIRequest(requestId)
{
  log_verbose(TAG, "LOWICacheResetRequest");
}

LOWICacheResetRequest::~LOWICacheResetRequest()
{
  log_verbose(TAG, "~LOWICacheResetRequest");
}

LOWIRequest::eRequestType
LOWICacheResetRequest::getRequestType() const
{
  return RESET_CACHE;
}

/////////////////////////////
// DiscoveryScanRequest
/////////////////////////////
LOWIDiscoveryScanRequest::LOWIDiscoveryScanRequest(uint32 requestId)
  : LOWIRequest(requestId)
{
  log_verbose(TAG, "LOWIDiscoveryScanRequest");

  // Default - no timeout
  timeoutTimestamp = 0;
  // Default values
  band = TWO_POINT_FOUR_GHZ;
  bufferCacheRequest = false;
  measAgeFilterSec = 0;
  fallbackToleranceSec = 0;
  requestMode = NORMAL;
  scanType = PASSIVE_SCAN;
}

LOWIDiscoveryScanRequest::eScanType
LOWIDiscoveryScanRequest::getScanType() const
{
  return scanType;
}
LOWIDiscoveryScanRequest::eRequestMode
LOWIDiscoveryScanRequest::getRequestMode() const
{
  return this->requestMode;
}
uint32 LOWIDiscoveryScanRequest::getMeasAgeFilterSec() const
{
  return this->measAgeFilterSec;
}

uint32 LOWIDiscoveryScanRequest::getFallbackToleranceSec() const
{
  return this->fallbackToleranceSec;
}
LOWIDiscoveryScanRequest::eBand
LOWIDiscoveryScanRequest::getBand() const
{
  return this->band;
}

vector<LOWIChannelInfo>& LOWIDiscoveryScanRequest::getChannels()
{
  return this->chanInfo;
}

int64 LOWIDiscoveryScanRequest::getTimeoutTimestamp() const
{
  return this->timeoutTimestamp;
}

bool LOWIDiscoveryScanRequest::getBufferCacheRequest() const
{
  return this->bufferCacheRequest;
}

LOWIDiscoveryScanRequest::~LOWIDiscoveryScanRequest()
{
  log_verbose(TAG, "~LOWIDiscoveryScanRequest");
}

LOWIRequest::eRequestType
LOWIDiscoveryScanRequest::getRequestType() const
{
  return DISCOVERY_SCAN;
}

LOWIDiscoveryScanRequest* LOWIDiscoveryScanRequest::createCacheOnlyRequest
(uint32 requestId,
 eBand band, uint32 measAgeFilter, int64 timeoutTimestamp,
 bool bufferCacheRequest)
{
  LOWIDiscoveryScanRequest *req = NULL;
  do
  {
    req = new(std::nothrow) LOWIDiscoveryScanRequest(requestId);
    if (NULL == req)
    {
      log_error(TAG, "%s, Mem allocation failure!", __func__);
      break;
    }

    req->requestMode = CACHE_ONLY;
    req->band = band;
    req->measAgeFilterSec = measAgeFilter;
    req->timeoutTimestamp = timeoutTimestamp;
    req->bufferCacheRequest = bufferCacheRequest;

    // Following variables are ignored in this request
    req->fallbackToleranceSec = 0;
  } while (0);

  return req;
}

LOWIDiscoveryScanRequest* LOWIDiscoveryScanRequest::createCacheFallbackRequest
(uint32 requestId,
 eBand band, eScanType type, uint32 measAgeFilter,
 uint32 fallbackTolerance,
 int64 timeoutTimestamp, bool bufferCacheRequest)
{
  LOWIDiscoveryScanRequest *req = NULL;
  do
  {
    req = new(std::nothrow) LOWIDiscoveryScanRequest(requestId);
    if (NULL == req)
    {
      log_error(TAG, "%s, Mem allocation failure!", __func__);
      break;
    }

    req->requestMode = CACHE_FALLBACK;
    req->band = band;
    req->scanType = type;
    req->measAgeFilterSec = measAgeFilter;
    req->fallbackToleranceSec = fallbackTolerance;
    req->timeoutTimestamp = timeoutTimestamp;
    req->bufferCacheRequest = bufferCacheRequest;
  } while (0);

  return req;
}

LOWIDiscoveryScanRequest* LOWIDiscoveryScanRequest::createFreshScanRequest
(uint32 requestId,
 eBand band, eScanType type, uint32 measAgeFilter,
 int64 timeoutTimestamp,
 eRequestMode mode)
{
  LOWIDiscoveryScanRequest *req = NULL;
  do
  {
    if (mode != NORMAL && mode != FORCED_FRESH)
    {
      log_error(TAG, "Invalid Mode!");
      break;
    }

    req = new(std::nothrow) LOWIDiscoveryScanRequest(requestId);
    if (NULL == req)
    {
      log_error(TAG, "%s, Mem allocation failure!", __func__);
      break;
    }

    req->requestMode = mode;
    req->band = band;
    req->scanType = type;
    req->measAgeFilterSec = measAgeFilter;
    req->timeoutTimestamp = timeoutTimestamp;

    // Following variables are ignored in this request
    req->bufferCacheRequest = false;
    req->fallbackToleranceSec = 0;
  } while (0);

  return req;
}

LOWIDiscoveryScanRequest* LOWIDiscoveryScanRequest::createCacheOnlyRequest
(uint32 requestId,
 vector<LOWIChannelInfo>& chanInfo,
 uint32 measAgeFilter, int64 timeoutTimestamp,
 bool bufferCacheRequest)
{
  LOWIDiscoveryScanRequest *req = NULL;
  do
  {
    if (0 == chanInfo.getNumOfElements())
    {
      log_error(TAG, "Channels to be scanned can not be 0!");
      break;
    }

    req = new(std::nothrow) LOWIDiscoveryScanRequest(requestId);
    if (NULL == req)
    {
      log_error(TAG, "%s, Mem allocation failure!", __func__);
      break;
    }

    req->requestMode = CACHE_ONLY;
    req->chanInfo = chanInfo;
    req->measAgeFilterSec = measAgeFilter;
    req->timeoutTimestamp = timeoutTimestamp;
    req->bufferCacheRequest = bufferCacheRequest;

    // Following variables are ignored in this request
    req->fallbackToleranceSec = 0;
    req->band = BAND_ALL;
  } while (0);

  return req;
}

LOWIDiscoveryScanRequest* LOWIDiscoveryScanRequest::createCacheFallbackRequest
(uint32 requestId,
 vector<LOWIChannelInfo>& chanInfo,
 eScanType type, uint32 measAgeFilter,
 uint32 fallbackTolerance,
 int64 timeoutTimestamp, bool bufferCacheRequest)
{
  LOWIDiscoveryScanRequest *req = NULL;
  do
  {
    if (0 == chanInfo.getNumOfElements())
    {
      log_error(TAG, "Channels to be scanned can not be 0!");
      break;
    }
    req = new(std::nothrow) LOWIDiscoveryScanRequest(requestId);
    if (NULL == req)
    {
      log_error(TAG, "%s, Mem allocation failure!", __func__);
      break;
    }

    req->requestMode = CACHE_FALLBACK;
    req->chanInfo = chanInfo;
    req->scanType = type;
    req->measAgeFilterSec = measAgeFilter;
    req->fallbackToleranceSec = fallbackTolerance;
    req->timeoutTimestamp = timeoutTimestamp;
    req->bufferCacheRequest = bufferCacheRequest;

    // Following variables are ignored in this request
    req->band = BAND_ALL;
  } while (0);

  return req;
}

LOWIDiscoveryScanRequest* LOWIDiscoveryScanRequest::createFreshScanRequest
(uint32 requestId,
 vector<LOWIChannelInfo>& chanInfo,
 eScanType type, uint32 measAgeFilter,
 int64 timeoutTimestamp, eRequestMode mode)
{
  LOWIDiscoveryScanRequest *req = NULL;
  do
  {
    if (mode != NORMAL && mode != FORCED_FRESH)
    {
      log_error(TAG, "Invalid Mode!");
      break;
    }

    if (0 == chanInfo.getNumOfElements())
    {
      log_error(TAG, "Channels to be scanned can not be 0!");
      break;
    }

    req = new(std::nothrow) LOWIDiscoveryScanRequest(requestId);
    if (NULL == req)
    {
      log_error(TAG, "%s, Mem allocation failure!", __func__);
      break;
    }

    req->requestMode = mode;
    req->chanInfo = chanInfo;
    req->scanType = type;
    req->measAgeFilterSec = measAgeFilter;
    req->timeoutTimestamp = timeoutTimestamp;

    // Following variables are ignored in this request
    req->bufferCacheRequest = false;
    req->fallbackToleranceSec = 0;
    req->band = BAND_ALL;
  } while (0);

  return req;
}

/////////////////////////////
// LOWIRangingScanRequest
/////////////////////////////
vector<LOWINodeInfo>& LOWIRangingScanRequest::getNodes()
{
  return this->nodeInfo;
}

void LOWIRangingScanRequest::setTimeoutTimestamp(int64 timestamp)
{
  this->timeoutTimestamp = timestamp;
}

int64 LOWIRangingScanRequest::getTimeoutTimestamp() const
{
  return this->timeoutTimestamp;
}

void LOWIRangingScanRequest::setReportType(eRttReportType report_type)
{
  this->rttReportType = report_type;
}

eRttReportType LOWIRangingScanRequest::getReportType() const
{
  return this->rttReportType;
}

LOWIRangingScanRequest::LOWIRangingScanRequest
(uint32 requestId,
 vector<LOWINodeInfo>& node_info,
 int64 timestamp)
  : LOWIRequest(requestId), timeoutTimestamp(timestamp)
{
  log_verbose(TAG, "LOWIRangingScanRequest");
  nodeInfo = node_info;

  // Validate all nodes
  for (unsigned int ii = 0; ii < nodeInfo.getNumOfElements();
       ++ii)
  {
    nodeInfo[ii].validate();
  }

  // Default - no timeout
  timestamp = 0;
  // Default Report type => aggregate
  rttReportType = RTT_REPORT_AGGREGATE;
}

LOWIRangingScanRequest::~LOWIRangingScanRequest()
{
  log_verbose(TAG, "~LOWIRangingScanRequest");
}

LOWIRequest::eRequestType
LOWIRangingScanRequest::getRequestType() const
{
  return RANGING_SCAN;
}

//////////////////////
// LOWIChannelInfo
//////////////////////
LOWIChannelInfo::LOWIChannelInfo()
{
  frequency = 0;
}
LOWIChannelInfo::LOWIChannelInfo(uint32 freq)
{
  frequency = freq;
}

LOWIChannelInfo::LOWIChannelInfo(uint32 channel,
                                 LOWIDiscoveryScanRequest::eBand band)
{
  frequency = LOWIUtils::channelBandToFreq(channel, band);
}

LOWIChannelInfo::~LOWIChannelInfo()
{
}

LOWIDiscoveryScanRequest::eBand
LOWIChannelInfo::getBand() const
{
  return LOWIUtils::freqToBand(frequency);
}

uint32 LOWIChannelInfo::getFrequency() const
{
  return frequency;
}

uint32 LOWIChannelInfo::getChannel() const
{
  return LOWIUtils::freqToChannel(frequency);
}

//////////////////////
// LOWINodeInfo
//////////////////////
LOWINodeInfo::LOWINodeInfo()
{
  // Initialize with defaults
  nodeType  = ACCESS_POINT;
  rttType   = RTT2_RANGING;
  bandwidth = BW_20MHZ;

  // FTM parameter defaults
  FTM_SET_ASAP(ftmRangingParameters);
  FTM_SET_BURST_DUR(ftmRangingParameters, 11);   // 128msec, enough for 5 bursts
  FTM_SET_BURSTS_EXP(ftmRangingParameters, 0);   // 1 burst
  FTM_SET_BURST_PERIOD(ftmRangingParameters, 0); // no preference
  FTM_CLEAR_LCI_REQ(ftmRangingParameters);       // no LCI request
  FTM_CLEAR_LOC_CIVIC_REQ(ftmRangingParameters); // no LCR request
}

void LOWINodeInfo::validate()
{
  // Check that the BW specified for 2.4Ghz is not 80 Mhz or more
  // Change to 20 Mhz if that's the case.
  LOWIChannelInfo ch_info = LOWIChannelInfo(frequency);
  if (ch_info.getBand() == LOWIDiscoveryScanRequest::TWO_POINT_FOUR_GHZ)
  {
    if (bandwidth >= BW_80MHZ)
    {
      bandwidth = BW_20MHZ;
    }
  }

  // Allow only 20 Mhz bandwidth for RTT1 Ranging
  if (RTT1_RANGING == rttType)
  {
    bandwidth = BW_20MHZ;
  }
}

//////////////////////////
// LOWIPeriodicNodeInfo
//////////////////////////
LOWIPeriodicNodeInfo::LOWIPeriodicNodeInfo()
  : LOWINodeInfo()
{
}

void LOWIPeriodicNodeInfo::validateParams()
{
  validate();

  /* check periodic request params */
  if (periodic)
  {
    if (meas_period <= LOWI_MIN_MEAS_PERIOD || num_measurements <= 0)
    {
      // params are incorrect, convert to non-periodic;
      periodic = 0;
    }
  }

  if (num_retries_per_meas > MAX_RETRIES_PER_MEAS)
  {
    num_retries_per_meas = MAX_RETRIES_PER_MEAS;
  }
}

//////////////////////////////////////
// LOWIAsyncDiscoveryScanResultRequest
//////////////////////////////////////
LOWIAsyncDiscoveryScanResultRequest::
LOWIAsyncDiscoveryScanResultRequest(uint32 requestId,
                                    uint32 request_timer_sec)
  : LOWIRequest(requestId), requestExpirySec(request_timer_sec),
    timeoutTimestamp(0)
{
  if (request_timer_sec > ASYNC_DISCOVERY_REQ_VALIDITY_PERIOD_SEC_MAX)
  {
    log_verbose(TAG, "LOWIAsyncDiscoveryScanResultRequest, requested"
                "expiry more than max. Setting to max");
    requestExpirySec = ASYNC_DISCOVERY_REQ_VALIDITY_PERIOD_SEC_MAX;
  }
}

uint32 LOWIAsyncDiscoveryScanResultRequest::getRequestExpiryTime()
{
  return requestExpirySec;
}

LOWIRequest::eRequestType
LOWIAsyncDiscoveryScanResultRequest::getRequestType() const
{
  return ASYNC_DISCOVERY_SCAN_RESULTS;
}

int64 LOWIAsyncDiscoveryScanResultRequest::getTimeoutTimestamp() const
{
  return timeoutTimestamp;
}

void
LOWIAsyncDiscoveryScanResultRequest::setTimeoutTimestamp(int64 timeout)
{
  timeoutTimestamp = timeout;
}


//////////////////////////////////
// LOWIPeriodicRangingScanRequest
//////////////////////////////////
vector<LOWIPeriodicNodeInfo>& LOWIPeriodicRangingScanRequest::getNodes()
{
  return nodeInfo;
}

vector<LOWINodeInfo> LOWIPeriodicRangingScanRequest::emptyNodeInfo;
LOWIPeriodicRangingScanRequest::LOWIPeriodicRangingScanRequest(
  uint32 requestId,
  vector<LOWIPeriodicNodeInfo>& node_info,
  int64 timestamp) : LOWIRangingScanRequest(requestId, emptyNodeInfo, timestamp)
{
  log_verbose(TAG, "LOWIPeriodicRangingScanRequest");
  nodeInfo = node_info;

  // Validate all nodes
  for (unsigned int ii = 0; ii < nodeInfo.getNumOfElements();
       ++ii)
  {
    nodeInfo[ii].validateParams();
  }
}

LOWIPeriodicRangingScanRequest::~LOWIPeriodicRangingScanRequest()
{
  log_verbose(TAG, "~LOWIPeriodicRangingScanRequest");
}

LOWIRequest::eRequestType
LOWIPeriodicRangingScanRequest::getRequestType() const
{
  return PERIODIC_RANGING_SCAN;
}


//////////////////////////////////
// LOWICancelRangingScanRequest
//////////////////////////////////
LOWICancelRangingScanRequest::LOWICancelRangingScanRequest(
  uint32 requestId,
  vector<LOWIMacAddress>& bssid_info)
  : LOWIRequest(requestId)
{
  log_verbose(TAG, "LOWICancelRangingScanRequest");
  bssidInfo = bssid_info;
}

LOWICancelRangingScanRequest::~LOWICancelRangingScanRequest()
{
  log_verbose(TAG, "~LOWICancelRangingScanRequest");
}

vector<LOWIMacAddress>& LOWICancelRangingScanRequest::getBssids()
{
  return this->bssidInfo;
}

LOWIRequest::eRequestType
LOWICancelRangingScanRequest::getRequestType() const
{
  return CANCEL_RANGING_SCAN;
}

LOWILciInformation::LOWILciInformation()
{
  latitude           = 0;
  longitude          = 0;
  altitude           = 0;
  latitude_unc       = 0;
  longitude_unc      = 0;
  altitude_unc       = 0;
  motion_pattern     = LOWI_MOTION_UNKNOWN;
  floor              = 0;
  height_above_floor = 0;
  height_unc         = 0;
}

//////////////////////////////////
// LOWISetLCILocationInformation
//////////////////////////////////
LOWISetLCILocationInformation::LOWISetLCILocationInformation(uint32 requestId,
                                                             LOWILciInformation& params,
                                                             uint32& usageRules)
  : LOWIRequest(requestId)
{
  log_verbose(TAG, "LOWISetLCILocationInformation");
  mLciInfo   = params;
  mUsageRules = usageRules;
}

LOWISetLCILocationInformation::~LOWISetLCILocationInformation()
{
  log_verbose(TAG, "~LOWISetLCILocationInformation");
}

const LOWILciInformation& LOWISetLCILocationInformation::getLciParams() const
{
  return mLciInfo;
}

uint32 LOWISetLCILocationInformation::getUsageRules() const
{
  return mUsageRules;
}

LOWIRequest::eRequestType
LOWISetLCILocationInformation::getRequestType() const
{
  return SET_LCI_INFORMATION;
}


LOWILcrInformation::LOWILcrInformation()
{
  memset(country_code, 0, LOWI_COUNTRY_CODE_LEN);
  length = 0;
  memset(civic_info, 0, CIVIC_INFO_LEN);
}

//////////////////////////////////
// LOWISetLCRLocationInformation
//////////////////////////////////
LOWISetLCRLocationInformation::LOWISetLCRLocationInformation(uint32 requestId, LOWILcrInformation& params)
  : LOWIRequest(requestId)
{
  log_verbose(TAG, "LOWISetLCRLocationInformation");
  mLcrInfo = params;
}

LOWISetLCRLocationInformation::~LOWISetLCRLocationInformation()
{
  log_verbose(TAG, "~LOWISetLCRLocationInformation");
}

const LOWILcrInformation& LOWISetLCRLocationInformation::getLcrParams() const
{
  return mLcrInfo;
}

LOWIRequest::eRequestType
LOWISetLCRLocationInformation::getRequestType() const
{
  return SET_LCR_INFORMATION;
}


///////////////////////////
// Neighbor Report Request
///////////////////////////
LOWINeighborReportRequest::LOWINeighborReportRequest(uint32 requestId)
  : LOWIRequest(requestId)
{
  log_verbose(TAG, "LOWINeighborReportRequest");
}

LOWINeighborReportRequest::~LOWINeighborReportRequest()
{
  log_verbose(TAG, "~LOWINeighborReportRequest");
}

LOWIRequest::eRequestType
LOWINeighborReportRequest::getRequestType() const
{
  return NEIGHBOR_REPORT;
}

///////////////////////////
// LCI Request
///////////////////////////
LOWISendLCIRequest::LOWISendLCIRequest(uint32 requestId, LOWIMacAddress bssid)
  : LOWIRequest(requestId)
{
  mBssid = bssid;
  log_debug(TAG, "LOWISendLCIRequest: macAddr(" QUIPC_MACADDR_FMT ")",
            QUIPC_MACADDR(mBssid));
}

///////////////////////////
// FTM Range Request
///////////////////////////
LOWIFTMRRNodeInfo::LOWIFTMRRNodeInfo(LOWIMacAddress mac, uint32 bssid_info, uint8 operating_class,
                  uint8 phy_type, uint8 chan,  uint8 center_ch1, uint8 center_ch2,
                  eRangingBandwidth bw) :
  bssid(mac), bssidInfo(bssid_info), operatingClass(operating_class),
  phyType(phy_type), ch(chan), center_Ch1(center_ch1),
  center_Ch2(center_ch2), bandwidth(bw)
{
  center_Ch1 = (0 == center_Ch1) ? ch : center_Ch1;
}
LOWIFTMRangingRequest::LOWIFTMRangingRequest(uint32 requestId, LOWIMacAddress bssid,
                                             uint16 randInterval, vector<LOWIFTMRRNodeInfo>& nodes)
  : LOWIRequest(requestId),
    mDestBssid(bssid),
    mRandInterval(randInterval),
    mNodes(nodes)
{
  log_debug(TAG, "LOWIFTMRRNodeInfo: macAddr(" QUIPC_MACADDR_FMT ")",
            QUIPC_MACADDR(mDestBssid));
}

