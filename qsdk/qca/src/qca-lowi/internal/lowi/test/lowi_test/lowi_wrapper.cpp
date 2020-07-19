/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Wrapper

GENERAL DESCRIPTION
  This file contains the Wrapper around LOWI Client

Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <inc/lowi_client.h>
#include <lowi_wrapper.h>
#include <osal/wifi_scan.h>

#define LOG_TAG "LOWIWrapper"
#define MEAS_TIME_FILTER 2

using namespace qc_loc_fw;

LOWIClient* client;
LOWIClientListener* listener;
uint32 req_id;
boolean lowi_wrapper_initialized = FALSE;
ptr2LOWIResultFunc ptr2PassiveScanResultsFunc;
ptr2LOWIResultFunc ptr2RtsctsScanResultsFunc;
ptr2LOWIResultFunc ptr2CapabilitiesRespFunc;
ptr2LOWIResultFunc ptr2AsyncDiscoveryScanResultsRespFunc;

/*=============================================================================================
 * Function description:
 * Send message to upper layer for RTS CTS Scan results
 *
 * Parameters:
 *    none
 *
 * Return value:
 *   none
 =============================================================================================*/

class LOWIClientListenerImpl : public LOWIClientListener
{
private:
  static const char *const TAG;
public:

  void printResponse(vector<LOWIScanMeasurement *>& scanMeasurements)
  {
    for (unsigned int ii = 0; ii < scanMeasurements.getNumOfElements(); ++ii)
    {
      LOWIScanMeasurement *scan = scanMeasurements[ii];
      vector<LOWIMeasurementInfo *> measurements = scan->measurementsInfo;
      log_debug(TAG, QUIPC_MACADDR_FMT" Frequency = %d",
                QUIPC_MACADDR(scan->bssid), scan->frequency);
      log_debug(TAG, "Is Secure = %d, Node type = %d, # Meas = %d",
                scan->isSecure, scan->type,
                measurements.getNumOfElements());

      for (unsigned int jj = 0; jj < measurements.getNumOfElements(); ++jj)
      {
        LOWIMeasurementInfo *meas = measurements[jj];
        log_debug(TAG, "#%d, RSSI = %d, RTT = %d, TIMESTAMP = %llu",
                  jj, meas->rssi, meas->rtt_ps, meas->rssi_timestamp);
      }
    }
  }

  void responseReceived(LOWIResponse *response)
  {
    log_info(TAG, "responseReceived Request Id = %d",
             response->getRequestId());
    if (response->getResponseType() == LOWIResponse::DISCOVERY_SCAN ||
        response->getResponseType() == LOWIResponse::ASYNC_DISCOVERY_SCAN_RESULTS)
    {
      LOWIDiscoveryScanResponse *resp = (LOWIDiscoveryScanResponse *)response;
      vector<LOWIScanMeasurement *> scanMeasurements = resp->scanMeasurements;
      printResponse(scanMeasurements);

      int ret_val = -1;
      if (response->getResponseType() == LOWIResponse::DISCOVERY_SCAN)
      {
        log_debug(TAG, "Response for Discovery scan request");
        if (NULL != ptr2PassiveScanResultsFunc)
        {
          ret_val = (*ptr2PassiveScanResultsFunc)(resp);
        }
      }
      else
      {
        log_debug(TAG, "Response for Async Discovery scan result request");
        if (NULL != ptr2AsyncDiscoveryScanResultsRespFunc)
        {
          ret_val = (*ptr2AsyncDiscoveryScanResultsRespFunc)(resp);
        }
      }

      if (0 != ret_val)
      {
        log_error(TAG, "sending msg to client failed");
      }
    }
    else if (response->getResponseType() == LOWIResponse::RANGING_SCAN)
    {
      LOWIRangingScanResponse *resp = (LOWIRangingScanResponse *)response;
      vector<LOWIScanMeasurement *> scanMeasurements = resp->scanMeasurements;
      printResponse(scanMeasurements);

      int ret_val = -1;
      if (NULL != ptr2RtsctsScanResultsFunc)
      {
        ret_val = (*ptr2RtsctsScanResultsFunc)(resp);
      }
      if (0 != ret_val)
      {
        log_error(TAG, "sending msg to upper layer failed");
      }
    }
    else if (response->getResponseType() == LOWIResponse::CAPABILITY)
    {
      log_debug(TAG, "Send response for the capability request");
      LOWICapabilityResponse *cap_resp = (LOWICapabilityResponse *)response;

      log_info(TAG, "discovery_scan_supported = %d", cap_resp->getCapabilities().discoveryScanSupported);
      log_info(TAG, "ranging_scan_supported = %d", cap_resp->getCapabilities().rangingScanSupported);
      log_info(TAG, "active_scan_supported = %d", cap_resp->getCapabilities().activeScanSupported);
      int ret_val = -1;
      if (NULL != ptr2CapabilitiesRespFunc)
      {
        ret_val = (*ptr2CapabilitiesRespFunc)(cap_resp);
        log_debug(TAG, "completed: Send response for the capability request");
      }
      if (0 != ret_val)
      {
        log_error(TAG, "Sending response for capabilities request failed");
      }
    }
    else if (response->getResponseType() == LOWIResponse::LOWI_STATUS)
    {
      log_debug(TAG, "LOWI Status response, ignore");

    }

  }

  virtual ~LOWIClientListenerImpl()
  {
    log_verbose(TAG, "~LOWIClientListenerImpl");
  }
};

const char *const LOWIClientListenerImpl::TAG = "LOWIWrapper";
/*=============================================================================
 * lowi_proc_iwmm_req_rts_cts_scan_with_live_meas
 *
 * Function description:
 *   This function processes RTS CTS Scan request with live WIFI measurement
 *
 * Parameters:
 *   p_scan_params - Structure pointer containing the scan prescriptions.
 *
 * Return value:
 *   0: success - The request was successfully sent.
 *   Non-zero: failure - request failed.
 ============================================================================*/
int lowi_proc_iwmm_req_rts_cts_scan_with_live_meas(
  vector<qc_loc_fw::LOWIPeriodicNodeInfo> *p_scan_params)
{
  int retVal = 0;

  // This is illegal operation if lowi wrapper has not been initialized
  if (!lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "illegal iwmm_req_rts_cts_scan_with_live_meas"
              " - uninitialized");
    retVal = -1;
    return retVal;
  }

  if (NULL == p_scan_params)
  {
    log_error("LOWIWrapper", "%s, Input parameter NULL", __func__);
    return -1;
  }

  LOWIPeriodicRangingScanRequest *ran = new LOWIPeriodicRangingScanRequest(++req_id, *p_scan_params, 0);
  if (NULL == ran)
  {
    retVal = -1;
  }
  else
  {
    if (LOWIClient::STATUS_OK != client->sendRequest(ran))
    {
      retVal = -2;
    }
    delete ran;
  }

  return retVal;
}

////////////////////////////
// Exposed functions
////////////////////////////

int lowi_queue_rts_cts_scan_req(
  vector<qc_loc_fw::LOWIPeriodicNodeInfo> *p_scan_params)
{
  int ret_val;
  ret_val = lowi_proc_iwmm_req_rts_cts_scan_with_live_meas(p_scan_params);
  return ret_val;
}

int lowi_queue_discovery_scan_req_band(LOWIDiscoveryScanRequest::eBand in_band,
                                       int64 request_timeout,
                                       LOWIDiscoveryScanRequest::eScanType scan_type,
                                       uint32 meas_filter_age)
{
  int retVal = 0;

  // This is illegal operation if lowi wrapper has not been initialized
  if (!lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "illegal discovery_scan_req_band"
              " - uninitialized");
    retVal = -1;
    return retVal;
  }

  if (scan_type > 1)
  {
    // Invalid scan type
    log_error("LOWIWrapper", "lowi_queue_discovery_scan_req_band"
              " invalid parameters");
    retVal = -3;
  }

  LOWIDiscoveryScanRequest *dis = LOWIDiscoveryScanRequest::createFreshScanRequest(
                                  ++req_id, in_band, scan_type, meas_filter_age, request_timeout,
                                  LOWIDiscoveryScanRequest::FORCED_FRESH);
  if (NULL == dis)
  {
    retVal = -2;
  }
  else
  {
    if (LOWIClient::STATUS_OK != client->sendRequest(dis))
    {
      retVal = -1;
    }
  }
  delete dis;
  return retVal;
}

int lowi_queue_discovery_scan_req_ch(vector<LOWIChannelInfo> *p_req,
                                     int64 request_timeout,
                                     LOWIDiscoveryScanRequest::eScanType scan_type,
                                     uint32 meas_filter_age)
{
  int retVal = 0;

  // This is illegal operation if lowi wrapper has not been initialized
  if (!lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "illegal discovery_scan_req_ch"
              " - uninitialized");
    retVal = -1;
    return retVal;
  }

  if (NULL == p_req || 0 == p_req->getNumOfElements() || scan_type > 1)
  {
    log_error("LOWIWrapper", "lowi_queue_discovery_scan_req_ch"
              " invalid parameters");
    return -3;
  }

  LOWIDiscoveryScanRequest *p_disc_req = LOWIDiscoveryScanRequest::createFreshScanRequest(
                                         ++req_id, *p_req, scan_type, meas_filter_age, request_timeout,
                                         LOWIDiscoveryScanRequest::FORCED_FRESH);
  if (NULL == p_disc_req)
  {
    retVal = -2;
  }
  else
  {
    if (LOWIClient::STATUS_OK != client->sendRequest(p_disc_req))
    {
      retVal = -1;
    }
  }
  delete p_disc_req;
  return retVal;

}


int lowi_queue_passive_scan_req(void)
{
  int retVal = 0;

  // This is illegal operation if lowi wrapper has not been initialized
  if (!lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "illegal passive_scan_req"
              " - uninitialized");
    retVal = -1;
    return retVal;
  }

  LOWIDiscoveryScanRequest *dis = LOWIDiscoveryScanRequest::createFreshScanRequest(
                                  ++req_id, LOWIDiscoveryScanRequest::TWO_POINT_FOUR_GHZ,
                                  LOWIDiscoveryScanRequest::PASSIVE_SCAN, MEAS_TIME_FILTER, 0,
                                  LOWIDiscoveryScanRequest::FORCED_FRESH);
  if (NULL == dis)
  {
    retVal = -2;
  }
  else
  {
    if (LOWIClient::STATUS_OK != client->sendRequest(dis))
    {
      retVal = -1;
    }
  }
  delete dis;
  return retVal;
}

int lowi_queue_capabilities_req(void)
{
  int retVal = 0;

  // This is illegal operation if lowi wrapper has not been initialized
  if (!lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "illegal capabilities_req"
              " - uninitialized");
    retVal = -1;
    return retVal;
  }

  LOWICapabilityRequest *cap = new LOWICapabilityRequest(++req_id);

  if (NULL == cap)
  {
    retVal = -2;
  }
  else
  {
    if (LOWIClient::STATUS_OK != client->sendRequest(cap))
    {
      retVal = -1;
    }
  }
  delete cap;
  return retVal;
}

int lowi_queue_async_discovery_scan_result_req(uint32 timeout)
{
  int retVal = 0;

  // This is illegal operation if lowi wrapper has not been initialized
  if (!lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "illegal async_discovery_scan_result_req"
              " - uninitialized");
    retVal = -1;
    return retVal;
  }

  LOWIAsyncDiscoveryScanResultRequest *async = new LOWIAsyncDiscoveryScanResultRequest(++req_id, timeout);

  if (LOWIClient::STATUS_OK != client->sendRequest(async))
  {
    retVal = -1;
  }

  delete async;
  return retVal;
}

int lowi_queue_nr_request()
{
  int retVal = -1;

  // This is illegal operation if lowi wrapper has not been initialized
  if (!lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "illegal lowi_queue_nr_request"
              " - uninitialized");
    return retVal;
  }

  LOWINeighborReportRequest *nrReq = new LOWINeighborReportRequest(++req_id);

  if (LOWIClient::STATUS_OK != client->sendRequest(nrReq))
  {
    log_debug("LOWIWrapper", "Failed to send Neighbor Report Request to LOWI");
  }
  else
  {
    retVal = 0;
  }

  delete nrReq;

  return retVal;
}

int lowi_queue_set_lci(LOWILciInformation *lciInfo)
{
  int retVal = -1;

  // This is illegal operation if lowi wrapper has not been initialized
  if (!lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "illegal lowi_queue_set_lci"
              " - uninitialized");
    return retVal;
  }

  if (NULL == lciInfo)
  {
    log_error("LOWIWrapper", "lowi_queue_set_lci: null input");
    return retVal;
  }

  uint32 usageRules = 0x01; //set to 1 by default.

  LOWISetLCILocationInformation *lciReq = new LOWISetLCILocationInformation(++req_id, *lciInfo, usageRules);
  if (NULL == lciReq)
  {
    log_error("LOWIWrapper", "lowi_queue_set_lci"
              " - out of memory");
    return retVal;
  }


  if (LOWIClient::STATUS_OK != client->sendRequest(lciReq))
  {
    log_debug("LOWIWrapper", "Failed to set LCI information");
  }
  else
  {
    retVal = 0;
  }

  delete lciReq;

  return retVal;
}

int lowi_queue_set_lcr(LOWILcrInformation *lcrInfo)
{
  int retVal = -1;

  // This is illegal operation if lowi wrapper has not been initialized
  if (!lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "illegal lowi_queue_set_lcr"
              " - uninitialized");
    return retVal;
  }

  if (NULL == lcrInfo)
  {
    log_error("LOWIWrapper", "lowi_queue_set_lcr: null input");
    return retVal;
  }

  LOWISetLCRLocationInformation *lcrReq = new LOWISetLCRLocationInformation(++req_id, *lcrInfo);
  if (NULL == lcrReq)
  {
    log_error("LOWIWrapper", "lowi_queue_set_lcr"
              " - out of memory");
    return retVal;
  }


  if (LOWIClient::STATUS_OK != client->sendRequest(lcrReq))
  {
    log_debug("LOWIWrapper", "Failed to set LCR information");
  }
  else
  {
    retVal = 0;
  }

  delete lcrReq;

  return retVal;
}

int lowi_queue_where_are_you(LOWIMacAddress bssid)
{
  int retVal = 0;

  // This is illegal operation if lowi wrapper has not been initialized
  if (!lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "illegal where are you request"
              " - uninitialized");
    retVal = -1;
    return retVal;
  }

  LOWISendLCIRequest *wru = new LOWISendLCIRequest(++req_id, bssid);
  if (NULL == wru)
  {
    retVal = -2;
  }
  else
  {
    if (LOWIClient::STATUS_OK != client->sendRequest(wru))
    {
      retVal = -1;
    }
  }
  delete wru;
  return retVal;
}

int lowi_queue_ftmrr(qc_loc_fw::LOWIMacAddress bssid,
                     uint16 randInterval, vector<qc_loc_fw::LOWIFTMRRNodeInfo>& nodes)
{
  int retVal = 0;

  // This is illegal operation if lowi wrapper has not been initialized
  if (!lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "illegal FTMRR"
              " - uninitialized");
    retVal = -1;
    return retVal;
  }

  LOWIFTMRangingRequest *ftmrr = new LOWIFTMRangingRequest(++req_id, bssid,
                                                         randInterval, nodes);
  if (NULL == ftmrr)
  {
    retVal = -2;
  }
  else
  {
    if (LOWIClient::STATUS_OK != client->sendRequest(ftmrr))
    {
      retVal = -1;
    }
  }
  delete ftmrr;
  return retVal;
}

int lowi_queue_passive_scan_req_xtwifi(const int cached,
                                       const uint32 max_scan_age_sec,
                                       const uint32 max_meas_age_sec)
{
  int retVal = 0;

  // This is illegal operation if lowi wrapper has not been initialized
  if (!lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "illegal passive_scan_req_xtwifi"
              " - uninitialized");
    retVal = -1;
    return retVal;
  }

  LOWIDiscoveryScanRequest *dis = LOWIDiscoveryScanRequest::createCacheFallbackRequest(
    ++req_id, LOWIDiscoveryScanRequest::TWO_POINT_FOUR_GHZ,
    LOWIDiscoveryScanRequest::PASSIVE_SCAN, max_meas_age_sec,
    max_scan_age_sec, 0, false);

  if (LOWIClient::STATUS_OK != client->sendRequest(dis))
  {
    retVal = -1;
  }

  delete dis;
  return retVal;
}

int lowi_wrapper_init(ptr2LOWIResultFunc passive, ptr2LOWIResultFunc rtscts)
{
  int retVal = 0;
  listener = NULL;
  client = NULL;

  // Set the log level to Warning
  log_set_local_level_for_tag("LOWIWrapper", EL_WARNING);

  if (lowi_wrapper_initialized)
  {
    log_error("LOWIWrapper", "init - LOWI wrapper already initialized!");
    return retVal;
  }

  listener = new LOWIClientListenerImpl();
  if (NULL == listener)
  {
    log_error("LOWIWrapper", "Could not create the LOWIClientListener");
    retVal = -1;
    return retVal;
  }

  client = LOWIClient::createInstance(listener, true, LOWIClient::LL_INFO);
  req_id = 0;

  if (NULL == client)
  {
    log_error("LOWIWrapper", "Could not create the LOWIClient");
    retVal = -1;
    return retVal;
  }

  ptr2PassiveScanResultsFunc = passive;
  ptr2RtsctsScanResultsFunc = rtscts;
  lowi_wrapper_initialized = TRUE;

  return retVal;
}

int lowi_wrapper_destroy()
{
  int retVal = 0;

  if (NULL != client)
  {
    delete client;
    client = NULL;
  }

  if (NULL != listener)
  {
    delete listener;
    listener = NULL;
  }

  ptr2PassiveScanResultsFunc = NULL;
  ptr2RtsctsScanResultsFunc = NULL;
  lowi_wrapper_initialized = FALSE;

  return retVal;
}

int lowi_wrapper_register_capabilities_cb(ptr2LOWIResultFunc cap)
{
  // Register/overwrite the callback function for capabilities.
  ptr2CapabilitiesRespFunc = cap;
  return 0;
}

int lowi_wrapper_register_async_resp_cb(ptr2LOWIResultFunc async)
{
  // Register/overwrite the callback function for aync scan response
  ptr2AsyncDiscoveryScanResultsRespFunc = async;
  return 0;
}
