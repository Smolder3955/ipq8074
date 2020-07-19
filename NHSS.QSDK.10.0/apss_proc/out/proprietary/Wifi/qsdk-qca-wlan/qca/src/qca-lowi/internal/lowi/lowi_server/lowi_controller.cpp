/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Controller

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Controller

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#include <stdio.h>
#include <string.h>
#include <base_util/log.h>
#include <lowi_server/lowi_controller.h>
#include <base_util/vector.h>
#include <common/lowi_utils.h>
#include <lowi_server/lowi_wifidriver_interface.h>
#include <lowi_server/lowi_discovery_scan_result_receiver.h>
#include <lowi_server/lowi_ranging_scan_result_receiver.h>
#include <lowi_server/lowi_scan_request_sender.h>
#include <lowi_server/lowi_version.h>
#include "lowi_time.h"
#include <lowi_server/lowi_scheduler.h>

using namespace qc_loc_fw;

#define DEFAULT_CACHE_SIZE 100
#define DEFAULT_MAX_QUEUE_SIZE 255
#define DEFAULT_THRESHOLD 500

#define BREAK_IF_NULL(r,s) if( NULL == r ) \
      { \
        log_info (TAG, "%s", s); \
        break; \
      }

// Global log level and tag for lowi
int lowi_debug_level;
const char *lowi_debug_tag;

const char * const LOWIController::TAG = "LOWIController" ;

template <class T>
bool createReceiver(T* &scanReceiver, LOWIScanResultReceiverListener* pScanListener, LOWIWifiDriverInterface* pWifiDriver)
{
  bool result = false;
  if (NULL == scanReceiver)
  {
    scanReceiver = new (std::nothrow) T(pScanListener, pWifiDriver);
    if (NULL == scanReceiver)
    {
      log_error ("LOWIController", "Could not create scanReceiver");
    }
    else if (false == scanReceiver->init ())
    {
      log_error ("LOWIController", "Could not init scanReceiver");
      delete scanReceiver;
      scanReceiver = NULL;
    }
    else
    {
      log_debug ("LOWIController", "created scanReceiver successfully");
      result = true;
    }
  }
  return result;
}

LOWIController::LOWIController(const char * const socket_name,
    const char * const config_name) :
    MqClientControllerBase(TAG, SERVER_NAME, socket_name, config_name),
    mEventReceiver (NULL), mCacheManager (NULL), mEventDispatcher (NULL),
    mCurrentRequest (NULL), mDiscoveryScanResultReceiver (NULL),
    mRangingScanResultReceiver(NULL),
    mScanRequestSender(NULL), mNetlinkSocketReceiver(NULL), mWifiDriver (NULL), mPassiveListeningTimerData (NULL),
    mTimerCallback (NULL), mScheduler(NULL),
    mInternalCapabilityRequest (NULL)
{
  mEventReceiver           = new (std::nothrow) LOWIEventReceiver ();
  mEventDispatcher         = new (std::nothrow) LOWIEventDispatcher (this);
  mFreshScanThreshold      = DEFAULT_THRESHOLD;
  mMaxQueueSize            = DEFAULT_MAX_QUEUE_SIZE;
  mWifiStateEnabled        = false;
  mReadDriverCacheDone     = false;
  mNlWifiStatus            = INTF_UNKNOWN;
}

LOWIController::~LOWIController()
{
  _shutdown();
  if (NULL != mEventReceiver)
  {
    mEventReceiver->removeListener(this);
    delete mEventReceiver;
  }
  if (NULL != mCacheManager)
  {
    delete mCacheManager;
  }
  if (NULL != mEventDispatcher)
  {
    delete mEventDispatcher;
  }
  if (NULL != mDiscoveryScanResultReceiver)
  {
    delete mDiscoveryScanResultReceiver;
  }
  if (NULL != mRangingScanResultReceiver)
  {
    delete mRangingScanResultReceiver;
  }
  if (NULL != mScanRequestSender)
  {
    delete mScanRequestSender;
  }
  if (NULL != mWifiDriver)
  {
    delete mWifiDriver;
  }
  if (NULL != mPassiveListeningTimerData)
  {
    removeLocalTimer (mTimerCallback, mPassiveListeningTimerData);
    delete mPassiveListeningTimerData;
  }
  if (NULL != mTimerCallback)
  {
    delete mTimerCallback;
  }
  if (NULL != mNetlinkSocketReceiver)
  {
    delete mNetlinkSocketReceiver;
  }

  // terminate the ranging scan request scheduler
  terminateScheduler();

  if (NULL != mInternalCapabilityRequest)
  {
    delete mInternalCapabilityRequest;
    mInternalCapabilityRequest = NULL;
  }

  lowi_time_close ();
}

int LOWIController::_init()
{
  log_verbose(TAG, "init");
  int result = 0;

  // Read and set the log level from the config file
  int log_level = qc_loc_fw::EL_LOG_ALL;
  m_config->getInt32Default ("LOWI_LOG_LEVEL", log_level,
      log_level);
  qc_loc_fw::log_set_global_level(LOWIUtils::to_logLevel(log_level));
  lowi_debug_level = log_level;
  lowi_debug_tag = qc_loc_fw::LOWI_VERSION;

  // Read the cache size and create the cache manager
  int cache_size = DEFAULT_CACHE_SIZE;
  m_config->getInt32Default ("LOWI_MAX_NUM_CACHE_RECORDS", cache_size,
      cache_size);
  mCacheManager = new (std::nothrow) LOWICacheManager (cache_size);
  if (NULL == mCacheManager)
  {
    log_error (TAG, "Cache could not be created! Operating without cache");
  }

  // Read other config values
  int auxInt = DEFAULT_MAX_QUEUE_SIZE;
  m_config->getInt32Default ("LOWI_MAX_OUTSTANDING_REQUEST", auxInt, auxInt);
  mMaxQueueSize = auxInt;
  auxInt = DEFAULT_THRESHOLD;
  m_config->getInt32Default ("LOWI_FRESH_SCAN_THRESHOLD", auxInt, auxInt);
  mFreshScanThreshold = auxInt;

  log_debug (TAG, "Config parameters : Log level = %d, Cache Records = %d,"
      " max queue size = %d, fresh scan threshold = %d", log_level,
      cache_size, mMaxQueueSize, mFreshScanThreshold);

  // Set-up event listener from EventReceiver
  if (NULL != mEventReceiver)
  {
    mEventReceiver->addListener(this);
  }

  mTimerCallback = new (std::nothrow) TimerCallback ();
  if (NULL == mTimerCallback)
  {
    log_error (TAG, "Unable to create Timer callback");
    result = -1;
  }



  lowi_time_init ();

  // Create the periodic ranging scan request scheduler
  if( false == createScheduler() )
  {
    result = -1;
  }

  // Create the netlink socket receiver
  if( false == createNetLinkSocketReceiver() )
  {
    result = -1;
  }

  // Internal Capability request
  mInternalCapabilityRequest = new (std::nothrow) LOWICapabilityRequest (0);

  //check wifi state from system properties at bootup
  bool wifiState = isWifiEnabled ();
  log_verbose (TAG, "wifiState from system properties %d",wifiState);
  return result;
}

eRequestStatus LOWIController::requestMeasurements (LOWIRequest* req)
{
  eRequestStatus retVal = INTERNAL_ERROR;

  log_verbose(TAG, "@requestMeasurements() -- request type(%s)",
              (req ? LOWIUtils::to_string(req->getRequestType()) : "NULL"));

 if( false == isWifiEnabled () )
  {
    log_warning (TAG, "requestMeasurements - Wifi is not enabled.");
    // Wifi is not enabled, no need to issue the request
    // For a passive listening request return true to indicate that the
    // request is successfully sent to the driver to avoid issuing the
    // request over and over again.
    retVal = (NULL == req) ? SUCCESS : NO_WIFI;
    return retVal;
  }

  if( NULL == mWifiDriver )
  {
    // Can not continue without the driver to service the request
    log_debug (TAG, "@requestMeasurements() -- Cannot service the request. No driver available");
    return NOT_SUPPORTED;
  }

  // Check the request types
  if( (NULL == req) || (LOWIRequest::NEIGHBOR_REPORT == req->getRequestType()) )
  {
    // Passive listening request. Send it to mDiscoveryScanResultReceiver
    if( NULL != mDiscoveryScanResultReceiver &&
        true == mDiscoveryScanResultReceiver->execute (req) )
    {
      retVal = SUCCESS;
    }
  }
  else if( LOWIRequest::LOWI_INTERNAL_MESSAGE == req->getRequestType() )
  {
    LOWIInternalMessage *r = (LOWIInternalMessage *)req;
    if( LOWIInternalMessage::LOWI_IMSG_FTM_RANGE_RPRT == r->getInternalMessageType() )
    {
      log_debug (TAG, "@requestMeasurements() -- Sending LOWI_IMSG_FTM_RANGE_RPRT to driver");
      // Internal message. Send it to mDiscoveryScanResultReceiver
      if( NULL != mDiscoveryScanResultReceiver &&
          true == mDiscoveryScanResultReceiver->execute (req) )
      {
        retVal = SUCCESS;
      }
    }
  }
  else if (LOWIRequest::DISCOVERY_SCAN == req->getRequestType())
  {
    // Send the discovery request to mDiscoveryScanResultReceiver
    if (NULL != mDiscoveryScanResultReceiver &&
        true == mDiscoveryScanResultReceiver->execute(req))
    {
      retVal = SUCCESS;
    }
  }
  else if (LOWIRequest::RANGING_SCAN == req->getRequestType() ||
           LOWIRequest::SET_LCI_INFORMATION == req->getRequestType() ||
           LOWIRequest::SET_LCR_INFORMATION == req->getRequestType() ||
           LOWIRequest::SEND_LCI_REQUEST == req->getRequestType() ||
           LOWIRequest::FTM_RANGE_REQ == req->getRequestType())
  {
    // Assume the request to be ranging related if we are here; send the request.
    if( NULL != mRangingScanResultReceiver &&
        true == mRangingScanResultReceiver->execute (req) )
    {
      retVal = SUCCESS;
    }
  }

  return retVal;
}

void LOWIController::processRequest (LOWIRequest* request)
{
  log_verbose (TAG, "processRequest");
  // Msg contained a Request

  // Analyze the request and process immediately if possible
  if (0 == processRequestImmediately(request))
  {
    log_info (TAG, "Request was processed immediately");
    delete request;
  }
  else
  {
    // Request could not be processed immediately.
    // Check if current request is for async scan results
    // and put in appropriate request queue.
    int32 ret= addRequestToAsyncScanRequestList(request);
    if (0 == ret)
    {
      // check for asynchronous requests that need to be sent and skip
      log_info (TAG, "Request added to async scan request list");
      return;
    }
    if (-1 == ret)
    {
      log_info (TAG, "Async scan request was not added to the list.");
      delete request;
      return;
    }

    // not an Async request - continue

    // Check if there is any current request executing
    if ( NULL == mCurrentRequest )
    {
      log_info (TAG, "No Request is executing");
      // No current request
      // Make the request a current request and issue the request
      eRequestStatus req_status = requestMeasurements (request);
      if (SUCCESS == req_status)
      {
        mCurrentRequest = request;
        log_info (TAG, "Request(%u) successfully sent to wifi driver", mCurrentRequest->getRequestId());
      }
      else
      {
        log_warning (TAG, "Request could not be sent to wifi driver,"
            " status = %d", req_status);

        LOWIResponse::eScanStatus scan_status =
            LOWIResponse::SCAN_STATUS_INTERNAL_ERROR;
        if (NO_WIFI == req_status)
        {
          scan_status = LOWIResponse::SCAN_STATUS_NO_WIFI;
        }
        else if (NOT_SUPPORTED == req_status)
        {
          scan_status = LOWIResponse::SCAN_STATUS_NOT_SUPPORTED;
        }

        if( (NULL != mScheduler) && mScheduler->isSchedulerRequest(request) )
        {
          // if the scheduler generated the request, then it needs to generate the
          // appropriate error response to the clients.
          mScheduler->manageErrRsp(request, scan_status);
        }
        else
        {
          // handle error response to request not generated by scheduler
          sendErrorResponse (*request, scan_status);
        }
        delete request;
      }
    }
    else
    {
      // Check if the Pending Q is full and respond
      if (mPendingRequestQueue.getSize() >= (uint32)mMaxQueueSize)
      {
        log_info (TAG, "Pending Queue full! Respond as busy!");
        // Respond to the request as LOWI is busy.
        sendErrorResponse (*request, LOWIResponse::SCAN_STATUS_BUSY);

        delete request;
      }
      else
      {
        log_info (TAG, "Queuing the Request");
        // Already a Request is executing
        // Queue the request, just received
        mPendingRequestQueue.push (request);
      }
    }
  }
}

void LOWIController::processMeasurementResults(LOWIMeasurementResult* meas_result)
{
  log_verbose (TAG, "processMeasurementResults");

  bool response_generated = false;
  // Cache the measurements, if there is no current request
  // This means meas results for the Passive listening / Batching
  if (NULL == mCurrentRequest)
  {
    log_info (TAG, "No current Request. Cache the measurements!");
    mCacheManager->putInCache(meas_result->scanMeasurements);

    // Send the response to the Async scan requests
    sendAsyncScanResponse (NULL, meas_result);

    // Get rid of the allocated memory ourselves since
    // no response is being generated
    for (unsigned int ii = 0;
        ii < meas_result->scanMeasurements.getNumOfElements();
        ++ii)
    {
      delete meas_result->scanMeasurements[ii];
    }
  }
  else
  {
    // Check if the measurements correspond to current request
    if(meas_result->request == mCurrentRequest)
    {
      // HACK - We are implementing a temporary cache mechanism using a snapshot
      // of last scan results from discovery / passive listening. Can not yet
      // update the cache with ranging results. The if condition should be removed
      // after the real cache implementation.
      if (meas_result->request->getRequestType() == LOWIRequest::DISCOVERY_SCAN)
      {
        // Cache the measurements and update the frequency table
        // in the cache as well
        mCacheManager->putInCache(meas_result->scanMeasurements);
      }

      // Send the response to the Async scan requests
      sendAsyncScanResponse (mCurrentRequest, meas_result);

      log_info (TAG, "Send response for the current request");
      response_generated = sendResponse (*mCurrentRequest, meas_result);

      // Delete the current request
      delete mCurrentRequest;
      mCurrentRequest = NULL;
    }
    else
    {
      // Measurements are received for probably a
      // passive listening request / batching status issued earlier.
      // Current request is different.

      // Keep waiting for the results for current request

      log_info (TAG, "Results correspond to a different"
                " Request than Current.!");
      // Cache the measurements
      mCacheManager->putInCache(meas_result->scanMeasurements);

      // Send the response to the Async scan requests
      sendAsyncScanResponse (NULL, meas_result);

      // Get rid of the allocated memory ourselves since
      // no response is being generated
      for (unsigned int ii = 0;
          ii < meas_result->scanMeasurements.getNumOfElements();
          ++ii)
      {
        delete meas_result->scanMeasurements[ii];
      }
    }
  }

  processPendingRequests();

  issueRequest ();

  // Delete the measurement results
  delete meas_result;
}

void LOWIController::issueRequest ()
{
  // Issue a new request if there is no current request,
  if (NULL == mCurrentRequest)
  {
    if (0 != mPendingRequestQueue.getSize())
    {
      // Loop through all the pending request with the intention to just issue
      // one request. If the request is unsuccessful, issue an error response
      // and then pick up the next request.
      for (List<LOWIRequest *>::Iterator it = mPendingRequestQueue.begin();
          it != mPendingRequestQueue.end();)
      {
        mCurrentRequest = *it;
        it = mPendingRequestQueue.erase(it);

        log_info (TAG, "Pending request retrieved from the Queue.");
        // Send the request to wifi driver.
        eRequestStatus req_status = requestMeasurements (mCurrentRequest);
        if (SUCCESS == req_status)
        {
          log_info (TAG, "Request successfully sent to wifi driver");
          break;
        }
        else
        {
          log_warning (TAG, "Request could not be sent to wifi driver,"
              " status = %d", req_status);
          LOWIResponse::eScanStatus scan_status =
              LOWIResponse::SCAN_STATUS_INTERNAL_ERROR;
          if (NO_WIFI == req_status)
          {
            scan_status = LOWIResponse::SCAN_STATUS_NO_WIFI;
          }
          else if (NOT_SUPPORTED == req_status)
          {
            scan_status = LOWIResponse::SCAN_STATUS_NOT_SUPPORTED;
          }

          // if the scheduler generated the request, then it needs to generate the
          // appropriate error response to the clients.
          if( NULL != mScheduler )
          {
            if( mScheduler->isSchedulerRequest(mCurrentRequest) )
            {
              mScheduler->manageErrRsp(mCurrentRequest, scan_status);
            }
            else
            {
              // handle error response to request not generated by scheduler
              sendErrorResponse (*mCurrentRequest, scan_status);
            }
          }
          else
          {
            // handle error response to request not generated by scheduler
            sendErrorResponse (*mCurrentRequest, scan_status);
          }

          delete mCurrentRequest;
          mCurrentRequest = NULL;
        }
      }
    }
  }
  else
  {
    log_info (TAG, "Current request still pending for measurements");
  }
}

void LOWIController::_process(InPostcard * const in_msg)
{
// This is useful to stop the process to be able to check memory leaks etc
//#define TEST 1
#ifdef TEST
    uint32 request_id = 0;
#endif
  if(NULL == in_msg) return;

  int result = 1;
  do
  {
    // Parse the Post card to get a Local Msg
    LOWILocalMsg* msg = mEventReceiver->handleEvent (in_msg);

    // The scheduler intercepts the message to manage the following:
    // -- periodic ranging scan requests
    // -- ranging scan requests with NAN wifi nodes in them
    // -- cancel requests
    // The scheduler also manages the responses for these requests.
    // Any other type of request/response will fall through and be processed as usual
    if( (NULL != mScheduler) && mScheduler->manageMsg(msg) )
    {
      log_info (TAG, "Scheduler is managing the msg");
      // delete and set ptr to NULL so that subsequent logic does not handle
      delete msg;
      msg = NULL;
    }

    LOWIRequest* request = NULL;
    LOWIMeasurementResult* meas_result = NULL;

    if (NULL != msg)
    {
      log_info (TAG, "Event received");
      if (true == msg->containsRequest())
      {
        // Contains a Request
        request = msg->getLOWIRequest ();
        if (NULL != request)
        {
          log_info (TAG, "Request received");
          processRequest (request);
        }
        else
        {
          log_error (TAG, "Request pointer NULL");
        }
      }
      else
      {
        // Message contains Measurement Result
        meas_result = msg->getMeasurementResult ();
        if (NULL != meas_result)
        {
          log_info (TAG, "Measurements received");
#ifdef TEST
          if (NULL != meas_result->request)
            request_id = meas_result->request->getRequestId();
#endif
          processMeasurementResults (meas_result);
        }
        else
        {
          log_error (TAG, "Measurements result pointer NULL");
        }
      }
      delete msg;
    }

    result = 0;
  } while (0);

  delete in_msg;
  if(0 != result)
  {
    log_error(TAG, "_process failed %d", result);
  }
#ifdef TEST
  if (9999 == request_id)
  {
    kill ();
  }
#endif
}

void LOWIController::_shutdown()
{
  if(CS_DESTROYED != m_state)
  {
    log_info(TAG, "shutdown");

    // flush and kill all modules

    m_state = CS_DESTROYED;
  }
  else
  {
    log_info(TAG, "shutdown: already in DESTROYED state, skip");
  }
}

void LOWIController::scanResultsReceived (LOWIMeasurementResult* result)
{
  log_verbose (TAG, "scanResultsReceived");

  if (NULL == result)
  {
    log_error (TAG, "measurements pointer can not be NULL!");
    return;
  }

  // Pass the pointer to the measurements through the Postcard
  // Measurements should be deleted by the LOWIController once used.
  InPostcard* card = LOWIMeasurementResult::createPostcard(result);
  if (NULL == card)
  {
    log_error (TAG, "Unable to create the card with Scan measurements!");
  }
  else
  {
    // Insert the card to the local msg queue so that the
    // Controller thread can get the InPostcard from the Blocking Queue
    MqMsgWrapper * wrapper = MqMsgWrapper::createInstance(card);
    if (0 != this->m_local_msg_queue->push (wrapper))
    {
      log_error (TAG, "Unable to push the results to the queue."
          " Delete the results");
      delete result;
      delete card;
    }
  }
}

void LOWIController::internalMessageReceived (LOWIInternalMessage* req)
{
  log_verbose (TAG, "internalMessageReceived");

  if (NULL == req)
  {
    log_error (TAG, "internal message pointer can not be NULL!");
    return;
  }

  // Pass the pointer to the internal message through the Postcard
  InPostcard* card = LOWIInternalMessage::createPostcard(req);
  if (NULL == card)
  {
    log_error (TAG, "Unable to create the card with internal message!");
  }
  else
  {
    // Insert the card to the local msg queue so that the
    // Controller thread can get the InPostcard from the Blocking Queue
    MqMsgWrapper * wrapper = MqMsgWrapper::createInstance(card);
    if (0 != this->m_local_msg_queue->push (wrapper))
    {
      log_error (TAG, "Unable to push the internal message to the queue."
          " Delete the request");
      delete req;
      delete card;
    }
  }
}

void LOWIController::wifiStateReceived (eWifiIntfState result)
{

  log_debug (TAG, "wifiStatusReceived: wifi state %d", result);
  OutPostcard* outcard = NULL;
  InPostcard* incard = NULL;
  outcard = OutPostcard::createInstance();
  if (outcard == NULL ||
      outcard->init () != 0 ||
      outcard->addString("INFO", "WIFI-STATUS-UPDATE") ||
      outcard->addInt32 ("IS_WIFI_ON", result) != 0 ||
      outcard->finalize() != 0)
  {
    log_warning (TAG, "wifiStatusReceived, failed to create postcard \n");
  }
  else
  {
    incard = InPostcard::createInstance (outcard);
    MqMsgWrapper * wrapper = MqMsgWrapper::createInstance(incard);
    if (0 != this->m_local_msg_queue->push (wrapper))
    {
      log_error (TAG, "Unable to push the card on the queue.");
      // Insert the card to the local msg queue so that the
      // Controller thread can get the InPostcard from the Blocking Queue
      delete incard;
    }
  }
  delete outcard;
}

bool LOWIController::sendResponse (LOWIRequest& req,
                                   LOWIMeasurementResult* result)
{
  bool retVal = false;
  log_verbose (TAG, "Send Response to the Request!");
  LOWIResponse* resp = generateResponse (req, result);

  if( NULL == resp )
  {
    log_error (TAG, "Unable to generate Response to the request!");
  }
  else
  {
    mEventDispatcher->sendResponse (resp, req.getRequestOriginator());
    delete resp;
    retVal = true;
  }
  return retVal;
}

bool LOWIController::sendErrorResponse (LOWIRequest& req,
    LOWIResponse::eScanStatus status)
{
  bool retVal = false;
  log_verbose (TAG, "Send Error Response to the Request!");
  LOWIMeasurementResult result;
  result.measurementTimestamp = 0;
  result.scanStatus = status;
  result.scanType = LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_UNKNOWN;

  LOWIResponse* resp = generateResponse (req, &result);
  if (NULL != resp)
  {
    mEventDispatcher->sendResponse (resp,
        req.getRequestOriginator());
    retVal = true;
  }
  else
  {
    log_error (TAG, "Unable to send the error response");
  }
  delete resp;
  return retVal;
}

LOWIResponse* LOWIController::generateResponse (LOWIRequest& req,
                                                LOWIMeasurementResult* meas_result)
{
  LOWIResponse* response = NULL;

  switch (req.getRequestType())
  {
  case LOWIRequest::DISCOVERY_SCAN:
  {
    log_verbose (TAG, "generateResponse - DISCOVERY_SCAN");
    if (NULL == meas_result)
    {
      log_error (TAG, "Measurement Result can not be null");
      break;
    }

    LOWIDiscoveryScanResponse* resp = new (std::nothrow)
                LOWIDiscoveryScanResponse (req.getRequestId());
    if (NULL == resp)
    {
      log_error (TAG, "%s, Memory allocation failure!");
      break;
    }

    resp->scanTypeResponse = meas_result->scanType;
    resp->scanStatus = meas_result->scanStatus;
    resp->timestamp = meas_result->measurementTimestamp;

    log_debug (TAG, "generateResponse ScanStatus = %d", resp->scanStatus);

    // The vector might be empty for no measurments
    resp->scanMeasurements = meas_result->scanMeasurements;

    response = resp;
  }
  break;
  case LOWIRequest::RANGING_SCAN:
  case LOWIRequest::PERIODIC_RANGING_SCAN:
  {
    log_verbose (TAG, "generateResponse - RANGING_SCAN");

    if (NULL == meas_result)
    {
      log_error (TAG, "Measurement Result can not be null");
      break;
    }

    LOWIRangingScanResponse* resp = new (std::nothrow)
                LOWIRangingScanResponse (req.getRequestId());
    if (NULL == resp)
    {
      log_error (TAG, "%s, Memory allocation failure!");
      break;
    }

    resp->scanStatus = meas_result->scanStatus;
    log_debug (TAG, "generateResponse ScanStatus = %d", resp->scanStatus);

    // The measurements as such could be null because of
    // no AP's being in range so NULL vector pointer is fine
    resp->scanMeasurements = meas_result->scanMeasurements;

    response = resp;
  }
  break;
  case LOWIRequest::CAPABILITY:
  {
    log_verbose (TAG, "generateResponse - CAPABILITY");

    // Start with default capabilities which are nothing supported.
    LOWICapabilities cap;
    bool status = false;
    if (NULL != mWifiDriver)
    {
      cap = mWifiDriver->getCapabilities();
      status = true;
    }

    LOWICapabilityResponse* resp = new (std::nothrow)
                LOWICapabilityResponse (req.getRequestId(), cap, status);
    if (NULL == resp)
    {
      log_error (TAG, "%s, Memory allocation failure!");
      break;
    }

    response = resp;
  }
  break;
  case LOWIRequest::RESET_CACHE:
  {
    log_verbose (TAG, "generateResponse - RESET_CACHE");
    LOWICacheResetResponse* resp = new (std::nothrow)
                LOWICacheResetResponse (req.getRequestId(),
                    mCacheManager->resetCache());
    if (NULL == resp)
    {
      log_error (TAG, "%s, Memory allocation failure!");
      break;
    }

    response = resp;

  }
  break;

  case LOWIRequest::ASYNC_DISCOVERY_SCAN_RESULTS:
  {
    log_verbose (TAG, "generateResponse - ASYNC_DISCOVERY_SCAN_RESULTS");
    if (NULL == meas_result)
    {
      log_error (TAG, "Measurement Result can not be null");
      break;
    }

    LOWIAsyncDiscoveryScanResultResponse* resp = new (std::nothrow)
      LOWIAsyncDiscoveryScanResultResponse (req.getRequestId());
    if (NULL == resp)
    {
      log_error (TAG, "%s, Memory allocation failure!");
      break;
    }

    resp->scanTypeResponse = meas_result->scanType;
    resp->scanStatus = meas_result->scanStatus;
    resp->timestamp = meas_result->measurementTimestamp;

    log_debug (TAG, "generateResponse ScanStatus = %d", resp->scanStatus);

    // Do a deep copy here instead of a simple vector assignment
    // Reason for doing this expensive operation is
    // The Response when deleted has to completely delete all the
    // scan measurements from the vector as we expect the clients
    // to only delete the response. So if we just copy the vector
    // the delete response with in the lowi_server will delete the
    // all the scan measurements leaving no measurements to be notified
    // to remaining requests
    unsigned int size = meas_result->scanMeasurements.getNumOfElements();
    // The vector might be empty for no measurements
    if (0 == size)
    {
      resp->scanMeasurements = meas_result->scanMeasurements;
    }
    else
    {
      log_verbose(TAG, "Copy all elements from the result vector");
      for (unsigned int ii = 0; ii < size; ++ii)
      {
        LOWIScanMeasurement* meas = new (std::nothrow)
          LOWIScanMeasurement(*meas_result->scanMeasurements[ii]);
        if (NULL != meas)
        {
          resp->scanMeasurements.push_back(meas);
        }
        else
        {
          log_error(TAG, "Unexpected - Failed to copy LOWIScanMeasurement");
        }
      }
    }

    response = resp;
  }
  break;

  default:
    break;
  }
  return response;
}

int32 LOWIController::processRequestImmediately (LOWIRequest* request)
{
  int32 retVal = -1;
  log_verbose (TAG, "%s", __func__);

  // Check if the Request is Reset Cache or Capability check request
  // and respond to them immediately

  if (LOWIRequest::CAPABILITY == request->getRequestType ())
  {
    if (false == isWifiEnabled ())
    {
      log_info (TAG, "Wifi is disabled");
    }
    log_debug (TAG, "Send response for the Capability request");
    sendResponse (*request, NULL);
    retVal = 0;
  }
  else if (LOWIRequest::RESET_CACHE == request->getRequestType ())
  {
    log_debug (TAG, "Send response for the Reset cache request");
    sendResponse (*request, NULL);
    retVal = 0;
  }
  else if (LOWIRequest::DISCOVERY_SCAN == request->getRequestType())
  {
    LOWIDiscoveryScanRequest* req = (LOWIDiscoveryScanRequest*) request;

    int32 err = 0;
    int64 fb_ts = 0;
    int64 cache_ts = 0;
    LOWIMeasurementResult result;
    // Check if it is a cache only request
    if (LOWIDiscoveryScanRequest::CACHE_ONLY == req->getRequestMode())
    {
      // process immediately if there is no current request pending
      // else, check the buffer bit and only process right now if the
      // buffer bit is not set.
      if (false == req->getBufferCacheRequest() || NULL == mCurrentRequest)
      {
        // Cache only request should be served immediately if the buffer
        // bit is not set
        cache_ts =
          LOWIUtils::currentTimeMs()- (req->getMeasAgeFilterSec()*1000);
        fb_ts = 0;
        err = getMeasurementResultsFromCache (cache_ts, fb_ts, req, result);
        // Send the response to the cache only request
        // Not bothered regarding the err code here.
        log_debug (TAG, "Sending response to the CACHE_ONLY request");
        sendResponse (*request, &result);
        retVal = 0;
      }
    }
    else if (LOWIDiscoveryScanRequest::CACHE_FALLBACK
        == req->getRequestMode())
    {
      cache_ts =
        LOWIUtils::currentTimeMs()- (req->getMeasAgeFilterSec()*1000);
      fb_ts =
        LOWIUtils::currentTimeMs()- (req->getFallbackToleranceSec()*1000);
      err = getMeasurementResultsFromCache (cache_ts, fb_ts, req, result);
      if (err == 0)
      {
        // Measurements for all the freq / band are found after the fallback
        // timestamp. We can respond with the results from cache
        log_debug (TAG, "Sending response to the CACHE_FALLBACK request");
        sendResponse (*request, &result);
        retVal = 0;
      }
    }
    else if (LOWIDiscoveryScanRequest::NORMAL
        == req->getRequestMode())
    {
      // In NORMAL Request mode, we should check if all the freq / bands
      // are scanned last, between current time and fresh threshold.
      // In that case, we can service the NORMAL request from Cache itself
      // considering that the measurements in the cache are recent enough.

      // To do this, we query the measurements from the cache with the
      // cache ts = current ts - fresh threshold
      // If all the requested channels were scanned after the cache timestamp,
      // we will get the results as well as err 0, otherwise we will get what
      // ever channels are scanned after cached ts and -1 in error code
      fb_ts = 0;
      cache_ts = LOWIUtils::currentTimeMs() - mFreshScanThreshold;
      if (cache_ts > 0)
      {
        err = getMeasurementResultsFromCache (cache_ts, fb_ts, req, result);
        if (0 == err)
        {
          log_debug (TAG, "Sending response to the NORMAL request");
          sendResponse (*request, &result);
          retVal = 0;
        }
      }
      else
      {
        log_warning (TAG, "Invalid cache ts! Very unlikely."
            " This request will be tried later");
      }
    }
  }

  return retVal;
}

int32 LOWIController::getMeasurementResultsFromCache
(int64 cache_ts, int64 fb_ts, LOWIDiscoveryScanRequest* req,
    LOWIMeasurementResult & result)
{
  int32 err = 0;
  int64 latest_cached_timestamp = 0;
  log_verbose (TAG, "%s", __func__);
  if (0 == req->getChannels().getNumOfElements())
  {
    // Request is for bands
    err = mCacheManager->getFromCache(cache_ts, fb_ts,
        req->getBand(),result.scanMeasurements,
        latest_cached_timestamp);
  }
  else
  {
    // Request is for channels
    err = mCacheManager->getFromCache(cache_ts, fb_ts,
        req->getChannels(),result.scanMeasurements,
        latest_cached_timestamp);
  }
  if (0 == err)
  {
    log_verbose (TAG, "success in getting results from cache");
    result.scanStatus = LOWIResponse::SCAN_STATUS_SUCCESS;
    // TODO: Check if type should be active / passive in this case
    result.scanType =
        LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_UNKNOWN;
    result.measurementTimestamp = latest_cached_timestamp;
  }
  else
  {
    log_debug (TAG, "Unable to get response from cache");
    result.scanStatus = LOWIResponse::SCAN_STATUS_INTERNAL_ERROR;
    // TODO: Check if type should be active / passive in this case
    result.scanType =
        LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_UNKNOWN;
    result.measurementTimestamp = latest_cached_timestamp;
  }
  return err;
}

void LOWIController::updateWifiState (eWifiIntfState wifiState)
{
  log_info (TAG, "updateWifiState: enabled %d mNlWifiStatus %d "
             "mWifiStateEnabled %d", wifiState, mNlWifiStatus, mWifiStateEnabled);
  eWifiIntfState prevState = mNlWifiStatus;
  mNlWifiStatus = wifiState;
  isWifiEnabled ();
}

void LOWIController::timerCallback (eTimerId id)
{
  log_verbose (TAG, "%s", __func__);
  if(RANGING_REQUEST_SCHEDULER_TIMER == id)
  {
    if(NULL != mScheduler)
    {
      log_debug(TAG, "@timerCallback(): ranging request scheduler timer");
      mScheduler->timerCallback();
    }
    else
    {
      log_warning(TAG, "@timerCallback(): mScheduler is NULL");
    }
  }
}

boolean LOWIController::isWifiEnabled ()
{
  boolean prevWiFiState           = mWifiStateEnabled;
  eWifiIntfState prevNlWifiStatus = mNlWifiStatus;

  log_debug (TAG, "@isWifiEnabled(): mNlWifiStatus %d mWifiStateEnabled %d",
             mNlWifiStatus, mWifiStateEnabled);
  // If states match, no need to process
  if (((INTF_DOWN == mNlWifiStatus) && !mWifiStateEnabled) ||
      ((INTF_DOWN < mNlWifiStatus) && mWifiStateEnabled))
  {
    return mWifiStateEnabled;
  }
  if (INTF_UNKNOWN == mNlWifiStatus)
  {
    mNlWifiStatus = LOWIWifiDriverInterface::getInterfaceState();
  }
  mWifiStateEnabled = ((mNlWifiStatus == INTF_UP) ||
                       (mNlWifiStatus == INTF_RUNNING));
  if (false == prevWiFiState && true == mWifiStateEnabled)
  {
    // Wifi enabled now
    log_debug (TAG, "@isWifiEnabled(): Wifi is now enabled");
    // Terminate the previous Scan result receiver
    terminateScanResultReceiver ();

    // Create the wifi driver
    if (true == createWifiDriver ())
    {
      // Read the cached data from the wifi driver.
      readCachedMeasurementsFromDriver ();

      // Create new Scan result receivers
      createScanResultReceiver ();
    }
    else
    {
      log_verbose (TAG, "@isWifiEnabled(): createWifiDriver() failed");
    }
  }
  else if (true == prevWiFiState && false == mWifiStateEnabled)
  {
    log_debug (TAG, "@isWifiEnabled(): Wifi is now disabled");

    // Terminate the scan result receivers
    terminateScanResultReceiver ();

    // Terminate Wifi driver
    terminateWifiDriver ();
    // Wifi is not enabled anymore.
    mNlWifiStatus = INTF_DOWN;
    // There might be a current request that was issued.
    // We should notify an error to the client and cancel all
    // pending requests from the queue as well.
    if (NULL != mCurrentRequest)
    {
      log_verbose (TAG, "Wifi disabled, Send error for current request");
      sendErrorResponse (*mCurrentRequest, LOWIResponse::SCAN_STATUS_NO_WIFI);
      delete mCurrentRequest;
      mCurrentRequest = NULL;
      issueRequest ();
    }
  }
  return mWifiStateEnabled;
}

void LOWIController::readCachedMeasurementsFromDriver ()
{
  if (NULL == mWifiDriver)
  {
    log_debug (TAG, "Unable to read cache from driver, no driver available");
    return;
  }
  // Just read it once only if not read already
  if (false == mReadDriverCacheDone)
  {
    LOWIMeasurementResult* res = mWifiDriver->getCacheMeasurements();
    if (NULL == res)
    {
      log_info (TAG, "No cached measurements found");
    }
    else
    {
      mReadDriverCacheDone = true;
      // Cache the results
      log_info (TAG, "Cache the measurements");
      mCacheManager->putInCache(res->scanMeasurements);

      // Get rid of the allocated memory
      for (unsigned int ii = 0; ii < res->scanMeasurements.getNumOfElements();
            ++ii)
      {
        delete res->scanMeasurements[ii];
      }
      delete res;
    }
  }
}

bool LOWIController::terminateWifiDriver ()
{
  bool result = true;
  log_debug (TAG, "Terminate Wifi driver");

  if (NULL != mWifiDriver)
  {
    delete mWifiDriver;
    mWifiDriver = NULL;
  }
  return result;
}

bool LOWIController::terminateScanResultReceiver ()
{
  bool result = true;
  log_debug (TAG, "Terminate the Scan Result Receivers");

  if (NULL != mDiscoveryScanResultReceiver)
  {
    delete mDiscoveryScanResultReceiver;
    mDiscoveryScanResultReceiver = NULL;
  }

  if (NULL != mRangingScanResultReceiver)
  {
    delete mRangingScanResultReceiver;
    mRangingScanResultReceiver = NULL;
  }

  // HACK - HACK
  if (NULL != mScanRequestSender)
  {
    delete mScanRequestSender;
    mScanRequestSender = NULL;
  }

  return result;
}

void LOWIController::terminateScheduler()
{
  log_verbose(TAG, "Terminate the ranging scan request scheduler");

  if (NULL != mScheduler)
  {
    delete mScheduler;
    mScheduler = NULL;
  }
}

bool LOWIController::createWifiDriver ()
{
  bool result = true;
  log_verbose (TAG, "Create the Wifi driver");

  // Create the wifi driver
  if (NULL == mWifiDriver)
  {
    mWifiDriver = LOWIWifiDriverInterface::createInstance (m_config, this, this, mCacheManager);
    if (NULL == mWifiDriver)
    {
      log_error (TAG, "Unable to create a wifi driver.");
      result = false;
    }
    else
    {
      log_verbose (TAG, "wifi driver instance created.");
    }
  }

  return result;
}

bool LOWIController::createScanResultReceiver ()
{
  bool result = false;

  log_debug (TAG, "%s: getCapabilities discovery(%d), ranging(%d), scanbitmask(0x%x)",
             __FUNCTION__,
             mWifiDriver->getCapabilities().discoveryScanSupported,
             mWifiDriver->getCapabilities().rangingScanSupported,
             mWifiDriver->getCapabilities().supportedCapablities);

  do
  {
    if((mWifiDriver->getCapabilities().supportedCapablities & LOWI_DISCOVERY_SCAN_SUPPORTED) &&
        (mDiscoveryScanResultReceiver == NULL))
    {
      LOWIDiscoveryScanResultReceiver *receiver = NULL;
      result = createReceiver(receiver, this, mWifiDriver);
      mDiscoveryScanResultReceiver = receiver;
      if (!result)
      {
        break;
      }
    }
    if((mWifiDriver->getCapabilities().supportedCapablities & LOWI_RANGING_SCAN_SUPPORTED) &&
        (mRangingScanResultReceiver == NULL))
    {
      LOWIRangingScanResultReceiver *receiver = NULL;
      result = createReceiver(receiver, this, mWifiDriver);
      mRangingScanResultReceiver = receiver;
      if (!result)
      {
        break;
      }
    }
    result = true;
  }
  while (0);
  return result;
}

int32 LOWIController::addRequestToAsyncScanRequestList (LOWIRequest* req)
{
  int32 retVal = -2;

  log_verbose (TAG, "%s", __func__);

  // Check if the Request is Async scan request
  if (LOWIRequest::ASYNC_DISCOVERY_SCAN_RESULTS ==
      req->getRequestType ())
  {
    // Let's use the same max size as for the pending queue
    if (mAsyncScanRequestList.getSize() >= (uint32)mMaxQueueSize)
    {
      log_info (TAG, "Async request list full! Respond as busy!");
      // Respond to the request as LOWI is busy.
      sendErrorResponse (*req, LOWIResponse::SCAN_STATUS_BUSY);
      retVal = -1;
    }
    else if (false == isWifiEnabled ())
    {
      log_info (TAG, "Wifi is disabled");
      sendErrorResponse (*req, LOWIResponse::SCAN_STATUS_NO_WIFI);
      retVal = -1;
    }
    else
    {
      log_debug (TAG, "Iterate over all requests to check validity.");
      // Check all the Async scan Requests for validity
      // Also check if the current request exists in the list
      for (List<LOWIRequest *>::Iterator it = mAsyncScanRequestList.begin();
          it != mAsyncScanRequestList.end();)
      {
        int64 timeout = 0;
        LOWIAsyncDiscoveryScanResultRequest* request =
          (LOWIAsyncDiscoveryScanResultRequest*) *it;
        timeout = request->getTimeoutTimestamp();

        log_debug (TAG, "Request Timeout = %llu", timeout);

        if (timeout < LOWIUtils::currentTimeMs())
        {
          log_info (TAG, "Request timeout! Dropping silently!");
          it = mAsyncScanRequestList.erase(it);
          delete request;
        }
        else
        {
          // Valid request
          log_debug (TAG, "Request is still valid. Check if there is any"
              " other request from the same client in the request list");
          // Check if the request is from the same client
          // as the current request. If it does, remove it from the list, and
          // later, replace it with the new one.
          if (0 == strcmp (req->getRequestOriginator(),
              request->getRequestOriginator()) )
          {
            log_info (TAG, "A request is already in the list from"
              " the same client. Delete it");
            it = mAsyncScanRequestList.erase(it);
            delete request;
          }
          else
          {
            // Increment the Iterator
            ++it;
          }
        }
      }
      log_debug (TAG, "Add the request to the list");
      int64 timeout = 0;
      LOWIAsyncDiscoveryScanResultRequest* new_req =
        (LOWIAsyncDiscoveryScanResultRequest*) req;
      timeout = new_req->getRequestExpiryTime()*1000 +
        LOWIUtils::currentTimeMs();
      new_req->setTimeoutTimestamp(timeout);
      log_verbose (TAG, "Set request timeout = %lld", timeout);
      mAsyncScanRequestList.add(new_req);
      retVal = 0;
    }
  }
  return retVal;
}

bool LOWIController::sendAsyncScanResponse (LOWIRequest* req,
    LOWIMeasurementResult* meas_result)
{
  bool retVal = false;
  log_verbose (TAG, "Send Async scan Response!");
  if (0 == mAsyncScanRequestList.getSize())
  {
    log_verbose (TAG, "No request in the Async request list");
    return retVal;
  }

  if (NULL != meas_result->request &&
    meas_result->request->getRequestType() != LOWIRequest::DISCOVERY_SCAN)
  {
    log_verbose (TAG, "Not a discovery scan or passive listening"
        " result. No need to notify Async scan requests");
    return retVal;
  }

  // Iterate over the list and generate responses for all the requests
  // As long as
  // 1. They are still valid
  // 2. The request in the list does not match the incoming request param
  // Also drop the the invalid requests from the list
  for (List<LOWIRequest *>::Iterator it = mAsyncScanRequestList.begin();
      it != mAsyncScanRequestList.end();)
  {
    int64 timeout = 0;
    LOWIAsyncDiscoveryScanResultRequest* request =
      (LOWIAsyncDiscoveryScanResultRequest*) *it;
    timeout = request->getTimeoutTimestamp();

    log_debug (TAG, "Request Timeout = %lld", timeout);

    if (timeout < LOWIUtils::currentTimeMs())
    {
      log_info (TAG, "Request timeout! Dropping silently!");
      it = mAsyncScanRequestList.erase(it);
      delete request;
    }
    else
    {
      // Valid request
      log_debug (TAG, "Request is still valid. Check if this request"
          " is from the same client as the current request");
      // Check if the request is from the same client
      // as the current request
      if ( NULL != req && (0 == strcmp (req->getRequestOriginator(),
           request->getRequestOriginator())) )
      {
        log_info (TAG, "Request is from the same client."
          " No need to send a response to this client");
      }
      else
      {
        log_verbose (TAG, "Request is from a different client.");

        // Generate response
        LOWIResponse* resp = generateResponse (*request, meas_result);
        if (NULL == resp)
        {
          log_error (TAG, "Unable to generate Response!");
        }
        else
        {
          mEventDispatcher->sendResponse (resp, request->getRequestOriginator());
          delete resp;
          retVal = true;
        }
      }
      // Increment the Iterator
      ++it;
    }
  }
  return retVal;
}

bool LOWIController::createScheduler()
{
  bool result = true;

  log_debug (TAG, "Create the ranging request scheduler");
  if (NULL == mScheduler)
  {
    mScheduler = new (std::nothrow)LOWIScheduler(this);
    if (NULL == mScheduler)
    {
      log_error (TAG, "Could not create the ranging request scheduler");
      result = false;
    }
  }

  return result;
}

bool LOWIController::createNetLinkSocketReceiver()
{
  bool result = false;

  log_debug (TAG, "Create the Netlinksocketreceiver");
  // create the netlink socket receiver to receive
  // netlink socket packets.
  if (NULL == mNetlinkSocketReceiver)
  {
    log_verbose (TAG, "create mNetlinkSocketReceiver");
    mNetlinkSocketReceiver =
          new (std::nothrow) LOWINetlinkSocketReceiver(this);
    if (NULL == mNetlinkSocketReceiver)
    {
      log_warning (TAG, "Could not create mNetlinkSocketReceiver");
      result = false;
    }
    else
    {
      log_verbose (TAG, "init mNetlinkSocketReceiver");
      if (false == mNetlinkSocketReceiver->init())
      {
        log_warning (TAG, "Could not init mNetlinkSocketReceiver");
        result = false;
      }
      else
      {
        log_verbose (TAG, "created mNetlinkSocketReceiver successfully");
        result = true;
      }
    }
  }
  return result;
}

LOWIRequest* LOWIController::getCurrReqPtr()
{
  return mCurrentRequest;
}

void LOWIController::setCurrReqPtr(LOWIRequest* r)
{
  mCurrentRequest = r;
}

void LOWIController::processPendingRequests()
{
  // Check all the pending Requests for validity and respond
  // to Requests that can be responded through the data in cache
  for( List<LOWIRequest *>::Iterator it = mPendingRequestQueue.begin();
     it != mPendingRequestQueue.end(); )
  {
    LOWIRequest* const req = *it;
    int64 timeout = 0;
    // Only requests that can be in the pending request queue can be either
    // discovery scan or ranging scan
    if( LOWIRequest::DISCOVERY_SCAN == req->getRequestType () )
    {
      LOWIDiscoveryScanRequest* r = (LOWIDiscoveryScanRequest*) req;
      timeout = r->getTimeoutTimestamp();
    }
    else if( LOWIRequest::RANGING_SCAN == req->getRequestType () )
    {
      LOWIRangingScanRequest* r = (LOWIRangingScanRequest*) req;
      timeout = r->getTimeoutTimestamp();
    }
    log_debug (TAG, "Request Timeout = %llu", timeout);

    if( 0 != timeout && timeout < LOWIUtils::currentTimeMs() )
    {
      log_info (TAG, "Request timeout! Dropping silently!");
      it = mPendingRequestQueue.erase(it);
      delete req;
    }
    else
    {
      // Valid request
      log_debug (TAG, "Request is still valid. Check if it can be"
                 " serviced now");
      // Check if the request can be serviced from the cache
      if( 0 == processRequestImmediately(req) )
      {
        log_info (TAG, "Request was processed from the cache!");

        // Remove the request from the queue and delete it.
        it = mPendingRequestQueue.erase(it);
        delete req;
      }
      else
      {
        // Increment the Iterator
        ++it;
      }
    }
  }
}

TimerCallback* LOWIController::getTimerCallback()
{
  return mTimerCallback;
}

void
TimerCallback::timerCallback(const qc_loc_fw::TimerDataInterface * const data)
{
  // Timer callback
  log_verbose ("TimerCallback", "%s", __func__);
  const TimerData * const myData = (const TimerData * const ) data;
  if (NULL != myData)
  {
    log_debug("TimerCallback", "timer id: %d", myData->mTimerId);
    if (NULL != myData->mController)
    {
      // Send the callback to the controller
      myData->mController->timerCallback (myData->mTimerId);
    }
  }
}
