/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Ranging Request Scheduler

GENERAL DESCRIPTION
  This file contains the implementation for the LOWI ranging request scheduler.
  The scheduler optimizes requests based on periodicity, retries, node type, etc.

Copyright (c) 2014, 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <base_util/log.h>
#include <lowi_utils.h>
#include <base_util/config_file.h>
#include <osal/common_inc.h>
#include "lowi_response.h"
#include "lowi_request.h"
#include "lowi_controller.h"
#include "lowi_scheduler.h"
#include "lowi_internal_message.h"

using namespace qc_loc_fw;


// strings used for debugging purposes
const char* REQUEST_TYPE[7] = {
  "DISCOVERY_SCAN",
  "RANGING_SCAN",
  "CAPABILITY",
  "RESET_CACHE",
  "ASYNC_DISCOVERY_SCAN_RESULTS",
  "PERIODIC_RANGING_SCAN",
  "CANCEL_RANGING_SCAN"
};

// strings used for debugging purposes
const char* SCAN_STATUS[9] =
{
  "SCAN_STATUS_UNKNOWN",
  "SCAN_STATUS_SUCCESS",
  "SCAN_STATUS_BUSY",
  "SCAN_STATUS_DRIVER_ERROR",
  "SCAN_STATUS_DRIVER_TIMEOUT",
  "SCAN_STATUS_INTERNAL_ERROR",
  "SCAN_STATUS_INVALID_REQ",
  "SCAN_STATUS_NOT_SUPPORTED",
  "SCAN_STATUS_NO_WIFI"
};

// msg strings used for printing/debugging PREAMBLE from LOWI
const char* PREAMBLE_STR_[4] = {
  "PREAMBLE_LEGACY",
  "PREAMBLE_CCK",
  "PREAMBLE_HT",
  "PREAMBLE_VHT"
};

// msg strings used for printing/debugging RTT type from LOWI
const char* RTT_STR_[4] = {
  "RTT1",
  "RTT2",
  "RTT3",
  "BEST"
};

// msg strings used for printing/debugging wifi node type from LOWI
const char* NODE_STR_[5] = {
  "UNKNOWN",
  "AP",
  "P2P",
  "NAN",
  "STA"
};


// msg strings used for printing/debugging BW type from LOWI
const char* BW_STR_[BW_160MHZ + 1] =
{
  "BW_20",
  "BW_40",
  "BW_80",
  "BW_160"
};

// any nodes in the database whose time2ReqMsec time is within this timer interval will
// be included in the next ranging request
#define INCLUSION_TIME_INTERVAL_MS 200

// definitions used for processing wifi nodes in a scan response
#define LOWER_PERIODIC_CNTR 1
#define LOWER_RETRY_CNTR    1
#define NO_ACTION           0
#define RESET_RETRY_CNTR    0xff

// initial minimum time used in the search for the actual minimum time
#define INITIAL_MIN_TIME 0xDEADBEEF

// initial minimum period used to search for the actual minimum period among the wifi nodes
// in the data base
#define INITIAL_MIN_PERIOD 0xFFFFFFFF

// maximum time in msec that an wifi node would have to wait before going on a request if
// it arrives while a timer is ongoing
#define ACCEPTABLE_WAIT_TIME_MSEC 200

/* used to convert .1 nsec quantities to usec */
#define LOWI_10K 10000

// Enter/Exit debug macros
#define SCHED_ENTER() log_verbose(TAG, "ENTER: %s", __func__);
#define SCHED_EXIT()  log_verbose(TAG, "EXIT: %s", __func__);

const char * const LOWIScheduler::TAG = "LOWIScheduler";
const char * const LOWIScheduler::LOWI_SCHED_ORIGINATOR_TAG = "LOWIRequestOriginator";

LOWIClientInfo::LOWIClientInfo(LOWIRequest *req)
{
  clientReq = req;
  result    = NULL;
  iReq      = NULL;
  log_verbose("LOWIClientInfo", "LOWIClientInfo() ctor: reqid(%u), reqOriginator(%s)",
              clientReq->getRequestId(), clientReq->getRequestOriginator());
}

void LOWIClientInfo::saveIReq(LOWIInternalMessage *iR)
{
  iReq      = iR;
  log_verbose("LOWIClientInfo", "LOWIClientInfo() ctor: reqid(%u), reqOriginator(%s)",
              clientReq->getRequestId(), clientReq->getRequestOriginator());
}

WiFiNodeInfo::WiFiNodeInfo(LOWIPeriodicNodeInfo *n, LOWIRequest *req)
{
  // store the original request the wifi node came in
  origReq      = req;
  // store the specific node information
  nodeInfo     = *n;

  schReqId     = 0;
  rssi         = 0;
  lastRssi     = 0;
  lastBw       = n->bandwidth;
  periodicCntr = ((int32) n->num_measurements < 0) ? 0 : (int32)n->num_measurements;

  // init the retryCntr
  // set to 0 always as retries have moved to FW
  retryCntr = 0;

  // init the state of the wifi node
  nodeState = WIFI_NODE_READY_FOR_REQ;

  // set the time2ReqMsec time based on periodicity
  time2ReqMsec = nodeInfo.periodic ? nodeInfo.meas_period : 0;

  measResult = NULL;
  meas = NULL;
}

LOWIScheduler::LOWIScheduler(LOWIController *lowiController)
{
  log_verbose (TAG, "Creating LOWIScheduler");
  mController           = lowiController;
  mNumPeriodicNodesInDB = 0;
  mSchedulerTimerData   = NULL;
  mCurrTimeoutMsec          = 0;
  mTimerRunning         = false;
}

LOWIScheduler::~LOWIScheduler()
{
  log_verbose (TAG, "~LOWIScheduler");
  if( NULL != mSchedulerTimerData )
  {
    delete mSchedulerTimerData;
    mSchedulerTimerData = NULL;
  }
}

bool LOWIScheduler::manageMsg(LOWILocalMsg *msg)
{
  SCHED_ENTER()

  bool retVal = false;

  if( (NULL == msg) )
  {
    log_debug(TAG, "@manageMsg(): msg == NULL");
    return retVal;
  }

  LOWIRequest           *req         = NULL;
  LOWIInternalMessage   *iReq        = NULL;
  LOWIMeasurementResult *meas_result = NULL;

  // process the message
  if( true == msg->containsRequest() )
  {
    req = msg->getLOWIRequest();

    if(false == isReqOk(req)) return retVal;

    // check if it's a ranging or stop ranging request, otherwise do nothing
    if( (req->getRequestType() == LOWIRequest::PERIODIC_RANGING_SCAN) ||
        (req->getRequestType() == LOWIRequest::RANGING_SCAN)          ||
        (req->getRequestType() == LOWIRequest::CANCEL_RANGING_SCAN) )
    {
      int32 status = manageRequest(req); // manage the request

      if( (-1 == status) || (0 == status) )
      {
        retVal = true; // indicates that the scheduler is managing the request
      }
    }
    else
    {
      log_debug(TAG, "@manageMsg(): Request into scheduler: %s -- not handled",
                REQUEST_TYPE[req->getRequestType()]);
      retVal = false;
    }
  }
  else if( true == msg->containsInternalMessage() )
  {
    iReq = msg->getLOWIInternalMessage();

    // handle the FTMRR request
    if ( LOWIInternalMessage::LOWI_IMSG_FTM_RANGE_REQ == iReq->getInternalMessageType() )
    {
      // convert the request into a regular LOWIRequest so
      // that it gets processed with existing functions
      LOWIFTMRangeReqMessage *ftmrrMsg = (LOWIFTMRangeReqMessage *)iReq;

      // request id provided by the scheduler
      uint32 reqId = createSchedulerReqId();

      // get the frequency info from the cache
      // BBB??? this code should not be needed. the driver should access the cache
      // only once when the request gets to it with just the BSSID.
      // retrieve channel info from the cache for every BSSID
      vector<LOWIPeriodicNodeInfo> v = ftmrrMsg->getNodes();
      vector<LOWIPeriodicNodeInfo> vNodes;
      for (uint32 ii=0; ii < ftmrrMsg->getNodes().getNumOfElements(); ++ii)
      {
        LOWIScanMeasurement scanMeas;
        LOWIPeriodicNodeInfo n;
        if ( mController->mCacheManager->getFromCache(v[ii].bssid, scanMeas) )
        {
          n.frequency         = scanMeas.frequency;
          n.band_center_freq1 = scanMeas.band_center_freq1;
          n.band_center_freq2 = scanMeas.band_center_freq2;
          n.bandwidth         = BW_20MHZ;
          n.bssid             = v[ii].bssid;
          n.periodic          = 0;
          n.preamble          = RTT_PREAMBLE_VHT;
          n.rttType           = RTT3_RANGING;
          n.num_pkts_per_meas = 5;
          FTM_SET_ASAP(n.ftmRangingParameters); // set ASAP to 1 by default
          vNodes.push_back(n);
          log_debug(TAG, "@manageMsg(): From cache -- freq(%u) bcf1(%u) bcf2(%u)",
                    n.frequency, n.band_center_freq1, n.band_center_freq2);
        }
        else
        {
          n.frequency         = 5240; //todo, clean up the magic numbers.
          n.band_center_freq1 = 5210;
          n.band_center_freq2 = 0;
          n.bandwidth         = BW_20MHZ;
          n.bssid             = v[ii].bssid;
          n.periodic          = 0;
          n.preamble          = RTT_PREAMBLE_VHT;
          n.rttType           = RTT3_RANGING;
          n.num_pkts_per_meas = 5;
          FTM_SET_ASAP(n.ftmRangingParameters); // set ASAP to 1 by default
          vNodes.push_back(n);
          log_debug(TAG, "@manageMsg(): From default -- freq(%u) bcf1(%u) bcf2(%u)",
                    n.frequency, n.band_center_freq1, n.band_center_freq2);
        }
      }

      // create the periodic scan request
      LOWIPeriodicRangingScanRequest *r =
        new (std::nothrow) LOWIPeriodicRangingScanRequest(reqId, vNodes, 0);
      int32 status = -1;
      if( NULL != r )
      {
        r->setRequestOriginator(LOWI_SCHED_ORIGINATOR_TAG);
        r->setTimeoutTimestamp(0);
        r->setReportType(RTT_REPORT_AGGREGATE);
        // manage the request
        status = manageRequest(r);
      }

      if(0 == status)
      {
        // the scheduler is managing the request
        retVal = true;
        // , the results will be placed in the client's list
        // results struct. From there they can easily be processed
        // by looking at the iReq flag of the client in the client list.

        // Find the client in the client list and save the internal message along with
        // the request created by the scheduler to carry out the FTMs. This way when
        // the response comes back, the APs that need to go on an FTMRR Report can be
        // identified.
        saveIReq(r, iReq);
      }
      else if (-1 == status)
      {
        // scheduler was managing the request, but something went wrong.
        // set this flag so the upper layer will discard the local message.
        retVal = true;
      }
    }
  }
  else
  { // gotta response from the driver
    meas_result = msg->getMeasurementResult();

    if( (NULL !=  meas_result) && (NULL != meas_result->request) )
    {
      // check if lowi-scheduler originated the request and if the current request matches
      // the request returned in the results
      if( isSchedulerRequest(meas_result->request) &&
          (meas_result->request == mController->getCurrReqPtr()) )
      {
        log_debug(TAG, "@manageMsg(): Results into scheduler -- handled: scanStatus(%s)",
                  SCAN_STATUS[meas_result->scanStatus]);

        // check the status of the response and handle accordingly
        switch(meas_result->scanStatus)
        {
          case LOWIResponse::SCAN_STATUS_SUCCESS:
            manageRangRsp(meas_result);
            break;
          default:
            manageErrRsp(meas_result);
        }
        delete meas_result;

        // Done with this set of results. Get the current request tracking pointer and
        // set to NULL to indicate to the controller that another request can be sent.
        // As opposed to what controller does, the scheduler doesn't delete the
        // current request when it's done processing the results as there may be periodic
        // wifi nodes still using it.
        if( NULL != mController->getCurrReqPtr() )
        {
          log_debug(TAG, "@manageMsg(): delete current request");
          delete(mController->getCurrReqPtr());
          mController->setCurrReqPtr(NULL);
        }
        // if there are pending requests, this is the time to process them
        mController->processPendingRequests();

        log_verbose(TAG, "@manageMsg(): issueRequest...");
        mController->issueRequest();
        retVal = true;
      }
      else
      {
        log_debug(TAG, "@manageMsg():Results into scheduler -- not handled");
        retVal = false;
      }
    }
    else
    {
      log_debug(TAG, "@manageMsg():NULL results or NULL request in results");
      retVal = false;
    }
  }
  SCHED_EXIT()
  return retVal;
}

int32 LOWIScheduler::manageRequest(LOWIRequest *req)
{
  SCHED_ENTER()
  //**********************************************
  // process a periodic ranging scan request:
  // 1. add wifi nodes to the wifi node data base
  // 2. check for periodic nodes
  // 3. check for NAN nodes and set up NAN request
  // 4. set up a request for all other nodes
  //**********************************************
  if( req->getRequestType() == LOWIRequest::PERIODIC_RANGING_SCAN )
  {
    log_debug(TAG, "@manageRequest(): Received: PERIODIC_RANGING_SCAN");

    // add to requests that are being managed by the scheduler
    LOWIClientInfo *info = new (std::nothrow)LOWIClientInfo(req);
    if( NULL == info )
    {
      return -1;
    }
    mClients.add(info);

    log_verbose(TAG, "@manageRequest(): #clients(%u)", mClients.getSize());
    // put wifi nodes from request into data base from where they will be managed
    addPeriodicNodesToDB(req);

    // Check data base for periodic nodes
    log_debug(TAG, "@manageRequest(): NumPeriodicNodesInDB(%u) DBsize(%u) mTimerRunning(%d)",
              mNumPeriodicNodesInDB,
              mNodeDataBase.getSize(),
              mTimerRunning);
    printDataBase();
    // This case occurs when a request that has periodic nodes arrives and a timer is not
    // already running. That means, either timer has never run or it ran but all the
    // periodic nodes were serviced so the timer was killed
    if( mNumPeriodicNodesInDB && (false == mTimerRunning) )
    {
      // calculate the min period among the periodic nodes
      computeTimerPeriod();

      if( 0 != startTimer() )
      {
        log_error(TAG, "@manageRequest(): unable to set up scheduler timer");
        return -1;
      }
    }
    else if( mNumPeriodicNodesInDB && (true == mTimerRunning) )
    {
      log_verbose(TAG, "@manageRequest(): adjusting the scheduler timer");
      // get the time left to expire on the timer and the
      // time that has elapsed since the timer started
      uint32 timeLeft = (uint32)(mTimerStarted + mCurrTimeoutMsec - LOWIUtils::currentTimeMs());
      uint32 timeElaped = (uint32)(LOWIUtils::currentTimeMs() - mTimerStarted);

      // The wifi node can wait until the timer expires if the time left on the timer is
      // less than some acceptable wait time. This would prevent the timer from being
      // preempted when it is "about" to expire.
      if( timeLeft > ACCEPTABLE_WAIT_TIME_MSEC )
      {
        // get the minimum period from those periodic wifi nodes that came in the request
        uint32 newMinTimerPeriod = mCurrTimeoutMsec;
        findNewMinPeriod(newMinTimerPeriod);

        // There is still a possibility that the timer doesn't need to be adjusted if the
        // minimum period found is greater than the time left on the timer. If this is not
        // the case, adjust the timer period
        if( timeLeft > newMinTimerPeriod )
        {
          if( 0 != adjustTimerPeriod(timeElaped, newMinTimerPeriod) )
          {
            log_error(TAG, "@manageRequest(): unable to adjust the timer period");
            return -1;
          }
        }
      }
    }

    // set up the ranging request(s) with nodes that are ready
    setupRequest();

    //*************************************************************************************
    // Handle NAN nodes
    //*************************************************************************************
    // check data base for NAN types
    if( foundNanNodesInDB() )
    {
      setupNanRequest();
    }
    return 0;
  }

  //**************************************************************************************
  // Handle CANCEL_RANGING_SCAN requests
  //**************************************************************************************
  if( req->getRequestType() == LOWIRequest::CANCEL_RANGING_SCAN )
  {
    log_debug(TAG, "Received: CANCEL_RANGING_SCAN");
    processCancelReq(req);
    return 0;
  }

  //**************************************************************************************
  // Handle RANGING_SCAN request that include NAN nodes
  //**************************************************************************************
  // If this is a ranging scan, we need to see if there are NAN nodes in it. If yes,
  // separate them out and send them to the driver. If no, the request can be processed
  // without adding the wifi nodes to the data base.
  if( req->getRequestType() == LOWIRequest::RANGING_SCAN )
  {
    log_debug(TAG, "Received: RANGING_SCAN");

    // check if NAN nodes are included
    if( false == foundNanNodesInReq() )
    {
      // if this is a regular ranging request without NAN nodes, there is
      // no need to add nodes to the data base, just process as before.
      log_debug (TAG, "No NAN nodes in RANGING_SCAN request");
      return 1;
    }
    else
    {
      log_debug (TAG, "found NAN nodes in RANGING_SCAN request, process them");
    }

    // add to requests that are being managed by the scheduler
    LOWIClientInfo *info = new (std::nothrow)LOWIClientInfo(req);
    if( NULL == info )
    {
      return -1;
    }
    mClients.add(info);

    // there are NAN nodes in this ranging request. Will need to put them in the data
    // base and keep track of them so we can return the appropriate response to the
    // client
    processNanNodes();
  }
  return 0;
}

void LOWIScheduler::addPeriodicNodesToDB(LOWIRequest* req)
{
  SCHED_ENTER()

  LOWIPeriodicRangingScanRequest *perReq = (LOWIPeriodicRangingScanRequest*)req;
  vector<LOWIPeriodicNodeInfo> vec       = perReq->getNodes();

  for(unsigned int ii = 0; ii < vec.getNumOfElements(); ii++)
  {
    LOWIPeriodicNodeInfo *n = (LOWIPeriodicNodeInfo*)&vec[ii];

    WiFiNodeInfo *node = new (std::nothrow)WiFiNodeInfo(n, req);

    if( NULL == node )
    {
      log_error(TAG, "@addPeriodicNodesToDB(): memory allocation failure");
      continue;
    }
    // add node to wifi node data base
    mNodeDataBase.add(node);

    // keep track of the number of periodic nodes being managed
    if( node->nodeInfo.periodic ) mNumPeriodicNodesInDB++;
  }
}

uint32 LOWIScheduler::foundPeriodicNodes()
{
  SCHED_ENTER()

  uint32 cntr = 0;

  for(List<WiFiNodeInfo*>::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); ++it)
  {
    WiFiNodeInfo* info = *it;

    if(info->nodeInfo.periodic)
    {
      ++cntr;
    }
  }
  return cntr;
}

uint32 LOWIScheduler::foundNanNodesInDB()
{
  // todo
  return 0;
}


bool LOWIScheduler::foundNanNodesInReq()
{
  // todo
  return false;
}

void LOWIScheduler::setupNanRequest()
{
  // todo
}

void LOWIScheduler::setupRequest()
{
  SCHED_ENTER()
  vector<LOWINodeInfo> v_rpt;

  // collect those nodes in the database whose state is WIFI_NODE_READY_FOR_REQ
  for( List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); ++it )
  {
    WiFiNodeInfo *info = *it;

    // pick those non-NAN wifi nodes that are ready to be requested,
    // and have the same same report type
    if( NAN_DEVICE != info->nodeInfo.nodeType &&
        WIFI_NODE_READY_FOR_REQ == info->nodeState )
    {
      LOWINodeInfo node;
      node = (LOWINodeInfo)info->nodeInfo;
      v_rpt.push_back(node);
    }
  }

  // compose and send the request
  if( v_rpt.getNumOfElements() > 0 )
  {
    log_debug (TAG, "@setupRequest(): #nodes in req(%d)", v_rpt.getNumOfElements());

    // generate a scheduler request id for this request
    uint32 reqId = createSchedulerReqId();

    // compose the request
    LOWIRangingScanRequest *r = new LOWIRangingScanRequest(reqId, v_rpt, 0);
    if( NULL != r )
    {
      r->setRequestOriginator(LOWI_SCHED_ORIGINATOR_TAG);
      r->setTimeoutTimestamp(0);
      r->setReportType(RTT_REPORT_AGGREGATE);

      // pass the request to the lowi-controller who will either send it right away or
      // put it in the pending queue
      mController->processRequest(r);

      // update node info in the data base for the nodes in this request
      // record the reqId with which these nodes went
      // record a change in state from WIFI_NODE_READY_FOR_REQ --> WIFI_NODE_MEAS_IN_PROGRESS
      updateNodeInfoInDB(r);
      log_debug (TAG, "@setupRequest(): database updated...");

      printDataBase();
    }
    else
    {
      log_error(TAG, "%s: Error: memory alloc failed -- could not create request", __func__);
    }
  }
}

void LOWIScheduler::adjustTimeLeft(uint32 adjustment)
{
  for( List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); ++it )
  {
    WiFiNodeInfo *info = *it;

    if( info->nodeInfo.periodic && (info->nodeState == WIFI_NODE_WAITING_FOR_TIMER) )
    {
      info->time2ReqMsec -= adjustment;
    }
  }
}

void LOWIScheduler::findNewMinPeriod(uint32 &newMinTimerPeriod)
{
  for( List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); ++it )
  {
    WiFiNodeInfo *info = *it;

    if( info->nodeInfo.periodic                    &&    //periodic node
        (info->nodeState == WIFI_NODE_READY_FOR_REQ) &&    // just came in request
        (info->nodeInfo.meas_period < newMinTimerPeriod) ) // it's minimum?
    {
      newMinTimerPeriod = info->nodeInfo.meas_period;
    }
  }
}

int32 LOWIScheduler::adjustTimerPeriod(uint32 timeElaped, uint32 newMinTimerPeriod)
{
  SCHED_ENTER()
  int32 retVal = 0;

  //stop the timer
  mController->removeLocalTimer(mController->getTimerCallback(),
                                mSchedulerTimerData);
  mTimerRunning = false;

  //adjust the periodic nodes in the data base
  adjustTimeLeft(timeElaped);

  //set new timer period
  mCurrTimeoutMsec = newMinTimerPeriod;

  //restart the timer with the new period
  if( 0 != startTimer() )
  {
    log_error(TAG, "@adjustTimerPeriod(): unable to set up scheduler timer");
    retVal = -1;
  }
  return retVal;
}

void LOWIScheduler::computeTimerPeriod( )
{
  SCHED_ENTER()

  // calling this function w/o periodic nodes in the data base is not allowed
  if( 0 == mNumPeriodicNodesInDB ) return;

  uint32 minTime = INITIAL_MIN_TIME;
  uint32 minPeriod = INITIAL_MIN_PERIOD;

  // iterate over the database of nodes, recalculate the time2ReqMsec values
  for( List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); ++it )
  {
    WiFiNodeInfo *info = *it;

    // recalculate the time2ReqMsec values
    if( info->nodeInfo.periodic && (0 != info->periodicCntr) )
    {
      info->time2ReqMsec -= (int32)mCurrTimeoutMsec;
      log_verbose(TAG, "@computeTimerPeriod(): bssid("QUIPC_MACADDR_FMT") stepdown time2ReqMsec(%d)",
                QUIPC_MACADDR(info->nodeInfo.bssid), info->time2ReqMsec);
      if( info->time2ReqMsec <= 0 )
      {
        info->time2ReqMsec = 0;
      }
    }

    // As we iterate over the periodic wifi nodes in the data base, keep track of the
    // minimum time2ReqMsec time, but exclude time2ReqMsec == 0. The minimum time2ReqMsec
    // time will become the next timer period.
    if( 0 != info->time2ReqMsec )
    {
      if( INITIAL_MIN_TIME == minTime )
      {
        minTime = info->time2ReqMsec;
      }
      else
      {
        if( info->time2ReqMsec < (int32)minTime )
        {
          minTime = info->time2ReqMsec;
        }
      }
    }

    // We need to cover the case where we end up with one or more periodic nodes but the
    // time2ReqMsec times are zero; hence, there is not suitable minTime. In that case,
    // keep track of the min period for each node and use that as the next timer period.
    if( info->nodeInfo.meas_period < minPeriod )
    {
      minPeriod = info->nodeInfo.meas_period;
    }
    if( INITIAL_MIN_TIME == minTime )
    {
      minTime = minPeriod;
    }
  }

  // new timeout for scheduler timer
  mCurrTimeoutMsec = minTime;
  log_debug(TAG, "@computeTimerPeriod(): minTime(%d) setting mCurrTimeoutMsec the same(%d)",
            minTime, mCurrTimeoutMsec);
}

void LOWIScheduler::setNodesToReady()
{
  SCHED_ENTER()
  int cnt = 0;
  // calling this function w/o periodic nodes in the data base is not allowed
  if( 0 == mNumPeriodicNodesInDB ) return;

  // iterate over the database of nodes, look at periodic nodes
  for( List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); ++it )
  {
    WiFiNodeInfo *info = *it;

    // check for periodic nodes that have not been fully serviced
    if( info->nodeInfo.periodic &&
       (0 != info->periodicCntr)  &&
       (info->nodeState = WIFI_NODE_WAITING_FOR_TIMER) )
    {
      // any nodes with time2ReqMsec time < the inclusion period are readied for request
      if( info->time2ReqMsec <= (int32)INCLUSION_TIME_INTERVAL_MS )
      {
        info->nodeState = WIFI_NODE_READY_FOR_REQ;
        log_debug(TAG, "@setNodesToReady(): bssid("QUIPC_MACADDR_FMT") time2ReqMsec(%d) state(%s) setback to orig period(%d)",
                  QUIPC_MACADDR(info->nodeInfo.bssid),
                  info->time2ReqMsec,
                  WIFI_NODE_STATE[info->nodeState],
                  info->nodeInfo.meas_period);

        // next time2ReqMsec time for this node is its original period
        info->time2ReqMsec = info->nodeInfo.meas_period;
        ++cnt;
      }
      else
      {
        log_debug(TAG, "@setNodesToReady(): bssid("QUIPC_MACADDR_FMT") time2ReqMsec(%d) state(%s)",
                  QUIPC_MACADDR(info->nodeInfo.bssid),
                  info->time2ReqMsec,
                  WIFI_NODE_STATE[info->nodeState]);
      }
    }
  }
  log_debug(TAG, "@setNodesToReady(): num nodes ready to go(%d)", cnt);
}

int32 LOWIScheduler::startTimer()
{
  SCHED_ENTER()
  int32 result = -1;
  mTimerRunning = false;

  if( NULL == mSchedulerTimerData )
  {
    mSchedulerTimerData =
    new (std::nothrow) TimerData(RANGING_REQUEST_SCHEDULER_TIMER, mController);
  }

  if( NULL != mSchedulerTimerData )
  {
    TimeDiff timeout;
    timeout.reset(true);
    int res = timeout.add_msec(mCurrTimeoutMsec);

    if( !res && (NULL != mController->getTimerCallback()) )
    {
      log_info(TAG, "@startTimer: timer running(%11.0f msec)", timeout.get_total_msec());
      mController->setLocalTimer(timeout,
                                 mController->getTimerCallback(),
                                 mSchedulerTimerData);
      mTimerRunning = true;
      mTimerStarted = LOWIUtils::currentTimeMs();
      result = 0;
    }
    else
    {
      log_error(TAG, "@startTimer: Unable to set the timer(%d)", res);
    }
  }
  else
  {
    log_error(TAG, "@startTimer: Unable to create scheduler timer data");
  }

  return result;
}

void LOWIScheduler::timerCallback()
{
  SCHED_ENTER()

  if( mNumPeriodicNodesInDB )
  {
    // calculate the new timer period and restart the timer
    log_verbose (TAG, "@timerCallback(): renewing the timer");
    computeTimerPeriod();
    setNodesToReady();

    if( 0 != startTimer() )
    {
      log_error(TAG, "@timerCallback(): unable to set up the timer in scheduler");
      return;
    }

    // set up the ranging request(s) with nodes that are ready
    setupRequest();
  }
  else
  {
    log_verbose(TAG, "@timerCallback(): no more periodic nodes left to process");
    if( NULL != mSchedulerTimerData )
    {
      log_debug (TAG, "@timerCallback(): removing local timer -- mNumPeriodicNodesInDB(%u)",
                 mNumPeriodicNodesInDB);
      mController->removeLocalTimer(mController->getTimerCallback(),
                                    mSchedulerTimerData);
      mTimerRunning = false;
      mCurrTimeoutMsec = 0;
    }
  }
}

bool LOWIScheduler::isSchedulerRequest(const LOWIRequest *req)
{
  // Can not handle a request if originator is NULL
  if (NULL == req->getRequestOriginator())
  {
    return false;
  }

  uint32 val = strncmp(LOWI_SCHED_ORIGINATOR_TAG,
                       req->getRequestOriginator(),
                       sizeof(LOWI_SCHED_ORIGINATOR_TAG));
  if( 0 == val )
  {
    return true;
  }
  else
  {
    return false;
  }
}

uint32 LOWIScheduler::createSchedulerReqId()
{
  SCHED_ENTER()
  static uint32 cntr = 0;
  uint32 pid = getpid();

  // take the pid, shift it to the upper 16 bits, and or it with the cntr.
  uint32 reqId = ((pid & 0x0000ffff) << 16) | (++cntr & 0x0000ffff);
  log_debug(TAG, "create id: pid: %u, cntr: %u", pid, cntr);
  printSchReqId(reqId);
  return reqId;
}

void LOWIScheduler::printSchReqId(uint32 reqId)
{
  uint32 pid = (reqId & 0xffff0000) >> 16;
  uint32 cntr = (reqId & 0x0000ffff);
  char buff[32] = {0};
  snprintf(buff, sizeof(buff), "%u-%u", pid, cntr);
  log_debug(TAG, "schReqId: %s", buff);
}

void LOWIScheduler::updateNodeInfoInDB(LOWIRequest *req)
{
  SCHED_ENTER()
  LOWIRangingScanRequest *r = (LOWIRangingScanRequest*)req;
  vector<LOWINodeInfo> v = r->getNodes();

  // find every vector from the request in the wifi node database and update its
  // scheduler request id and the node state
  for(uint32 ii = 0; ii < v.getNumOfElements(); ii++)
  {
    for(List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); ++it)
    {
      WiFiNodeInfo *node = *it;

      if( (WIFI_NODE_READY_FOR_REQ == node->nodeState) &&
          (0 == node->nodeInfo.bssid.compareTo(v[ii].bssid)) )
      {
        node->schReqId = req->getRequestId();
        node->nodeState = WIFI_NODE_MEAS_IN_PROGRESS;
      }
    }
  }
}

void LOWIScheduler::manageRangRsp(LOWIMeasurementResult *measResult)
{
  SCHED_ENTER()
  bool retriesApply;
  bool isPeriodic;
  bool noMeasurements = false;

  log_verbose(TAG, "@manageRangRsp(): # wifi nodes in results(%u)",
              measResult->scanMeasurements.getNumOfElements());
  // iterate over the wifi nodes in this set of results
  for(uint32 ii = 0; ii < measResult->scanMeasurements.getNumOfElements(); ii++)
  {
     // get the wifi node to process
     LOWIScanMeasurement *node = measResult->scanMeasurements[ii];

     // ensure wifi node is on DB, if it is do the following:
     // -- retrieve some info
     // -- save measurement result info
     if( !isWiFiNodeInDB(node->bssid, retriesApply, isPeriodic, measResult) ) continue;

     // check if we got any measurements for this wifi node
     // this is what tells us the status of the node response, if got no measurements,
     // the node is elegible to be retried if retries apply to it.
     noMeasurements = (node->measurementsInfo.getNumOfElements() == 0) ? true : false;

     //////////////////////////////////////////////////////////////////////////////////////
     // the following table shows what needs to be checked on each wifi node and
     // the action that needs to be taken.
     //////////////////////////////////////////////////////////////////////////////////////
     // RETRIES | PERIODIC | STATUS | ACTION
     // no      |  yes     |  good  | lower periodic cntr, ready_for_resp
     // no      |  yes     |  bad   | lower periodic cntr, ready_for_resp
     // no      |  no      |  good  | ready_for_resp
     // no      |  no      |  bad   | ready_for_resp
     // yes     |  yes     |  bad   | lower retry cntr , ready_for_req (will change to rsp if retries ran out)
     // yes     |  no      |  bad   | lower retry cntr , ready_for_req (will change to rsp if retries ran out)
     // yes     |  yes     |  good  | lower periodic cntr, reset retry cntr, ready_for_resp
     // yes     |  no      |  good  |                      reset retry cntr, ready_for_resp
     //////////////////////////////////////////////////////////////////////////////////////

     // disable retries logic in scheduler as it has moved down to FW
     // disable periodicity in scheduler as it is not required for m-release
     retriesApply = false;
     isPeriodic   = false;

     if(!retriesApply)
     {
        log_verbose(TAG, "@manageRangRsp(): noRetries");
        if(isPeriodic)
        {
          log_verbose(TAG, "@manageRangRsp(): periodic node");
          // lower periodic cntr and set new wifi node state
          processNode(node, NO_ACTION, LOWER_PERIODIC_CNTR,
                      WIFI_NODE_READY_FOR_RSP, measResult->scanStatus);
          continue;
        }
        // set new wifi node state
        processNode(node, NO_ACTION, NO_ACTION,
                    WIFI_NODE_READY_FOR_RSP, measResult->scanStatus);
     }
     else
     {
        log_verbose(TAG, "@manageRangRsp(): RetriesApply");
        if(noMeasurements)
        {
          log_verbose(TAG, "@manageRangRsp(): noMeasurements");
          // lower retry cntr, lower periodic cntr and set new wifi node state
          processNode(node, LOWER_RETRY_CNTR, NO_ACTION,
                      WIFI_NODE_READY_FOR_REQ, measResult->scanStatus);
        }
        else
        { // got measurements
          log_verbose(TAG, "@manageRangRsp(): got measurements");
          if(isPeriodic)
          {
            log_verbose(TAG, "@manageRangRsp(): periodic node");
            // lower periodic cntr and set new wifi node state
            processNode(node, RESET_RETRY_CNTR, LOWER_PERIODIC_CNTR,
                        WIFI_NODE_READY_FOR_RSP, measResult->scanStatus);
            continue;
          }
          // reset retry cntr and set wifi node state
          processNode(node, RESET_RETRY_CNTR, NO_ACTION,
                      WIFI_NODE_READY_FOR_RSP, measResult->scanStatus);
        }
     }
  }

  // Check if there are nodes ready for response
  if( nodesForRsp() )
  {
    processRangRsp();
  }

  // clean up database, requests, timer, etc.
  cleanUp();

  // if there are retries, this will pick them up.
  log_verbose(TAG, "@manageRangRsp(): setupRequest() to pick up retries");
  setupRequest();
}

void LOWIScheduler::manageErrRsp(LOWIMeasurementResult *measResult)
{
  SCHED_ENTER()
  bool retriesApply;
  bool isPeriodic;

  uint32 numNodes = measResult->scanMeasurements.getNumOfElements();

  log_verbose(TAG, "@manageErrRsp(): # wifi nodes in results: %u", numNodes);

  // This is the case where the FSM rejected the request.
  // Check the nodes in the request and retry those for which retries apply.
  // Those nodes for which retries do not apply are sent back to the client in a response.
  // Periodic nodes, not yet fully serviced, remain on the database.
  if( !numNodes )
  {
    log_verbose(TAG, "@manageErrRsp(): bad rsp and no nodes");
    // get nodes from request
    LOWIRangingScanRequest *r = (LOWIRangingScanRequest *)mController->getCurrReqPtr();
    vector<LOWINodeInfo> vec = r->getNodes();

    for( uint32 ii = 0; ii < vec.getNumOfElements(); ii++ )
    {
      LOWINodeInfo n = vec[ii];

      log_debug(TAG, "@manageErrRsp(): node from request: "QUIPC_MACADDR_FMT"",
                QUIPC_MACADDR(n.bssid));

      if( !isWiFiNodeInDB(n.bssid , retriesApply, isPeriodic, measResult) ) continue;

      // STATUS | RETRIES | PERIODIC | ACTION
      //  bad   | no      |  yes     | lower periodic cntr, ready_for_resp
      //  bad   | no      |  no      | ready_for_resp
      //  bad   | yes     |  dontcare| lower retry cntr , ready_for_req (will change to rsp if retries ran out)
      //  bad   | yes     |  dontcare| lower retry cntr , ready_for_req (will change to rsp if retries ran out)

      // disable retries logic in scheduler as it has moved down to FW
      // disable periodicity in scheduler as it is not required for m-release
      retriesApply = false;
      isPeriodic   = false;

      if( !retriesApply )
      {
        log_verbose(TAG, "@manageErrRsp(): noRetries");
        if( isPeriodic )
        {
          log_verbose(TAG, "@manageErrRsp(): periodic node");
          // lower periodic cntr and set new wifi node state
          processNode(n.bssid, NO_ACTION, LOWER_PERIODIC_CNTR,
                      WIFI_NODE_READY_FOR_RSP, measResult->scanStatus);
          continue;
        }
        // set new wifi node state
        processNode(n.bssid, NO_ACTION, NO_ACTION,
                    WIFI_NODE_READY_FOR_RSP, measResult->scanStatus);
      }
      else
      {
        log_verbose(TAG, "@manageErrRsp(): RetriesApply");
        // lower retry cntr, lower periodic cntr and set new wifi node state
        processNode(n.bssid, LOWER_RETRY_CNTR, NO_ACTION,
                    WIFI_NODE_READY_FOR_REQ, measResult->scanStatus);
      }
    }

    // Check if there are nodes ready for response
    if( nodesForRsp() )
    {
      processRangRsp();
    }

    // clean up database, requests, timer, etc.
    cleanUp();

    // if there are retries, this will pick them up.
    log_verbose(TAG, "@manageErrRsp(): setupRequest() to pick up retries");
    setupRequest();
  }
  else
  {
    log_error(TAG, "@manageErrRsp(): Bad rsp with nodes, this should not happen???");
  }
}

void LOWIScheduler::manageErrRsp(LOWIRequest *req, LOWIResponse::eScanStatus scan_status)
{
  SCHED_ENTER()
  bool retriesApply;
  bool isPeriodic;
  vector<uint32> v; // keep track of the reqId for those nodes that are ready for response
  vector<LOWINodeInfo> vec;

  // update the node status prior to processing the error
  updateNodeInfoInDB(req);

  LOWIRangingScanRequest *r = (LOWIRangingScanRequest *)(req);
  vec = r->getNodes();
  log_verbose(TAG, "@manageErrRsp(req): request not sent to wifi driver...#nodes(%u)",
              vec.getNumOfElements());

  // This is the case where the FSM rejected the request.
  // Check the nodes in the request and retry those for which retries apply.
  // Those nodes for which retries do not apply are sent back to the client in a response.
  // Periodic nodes, not yet fully serviced, remain on the database.
  if( vec.getNumOfElements() )
  {
    for( uint32 ii = 0; ii < vec.getNumOfElements(); ii++ )
    {
      LOWINodeInfo n = vec[ii];

      log_debug(TAG, "@manageErrRsp(): node from request: "QUIPC_MACADDR_FMT"",
                QUIPC_MACADDR(n.bssid));

      if( !isWiFiNodeInDB(n.bssid , retriesApply, isPeriodic, req) ) continue;

      // STATUS | RETRIES | PERIODIC | ACTION
      //  bad   | no      |  yes     | lower periodic cntr, ready_for_resp
      //  bad   | no      |  no      | ready_for_resp
      //  bad   | yes     |  dontcare| lower retry cntr , ready_for_req (will change to rsp if retries ran out)
      //  bad   | yes     |  dontcare| lower retry cntr , ready_for_req (will change to rsp if retries ran out)
      if( !retriesApply )
      {
        log_verbose(TAG, "@manageErrRsp(): noRetries");
        if( isPeriodic )
        {
          log_verbose(TAG, "@manageErrRsp(): periodic node");
          // lower periodic cntr and set new wifi node state
          processNode(n.bssid, NO_ACTION, LOWER_PERIODIC_CNTR, WIFI_NODE_READY_FOR_RSP, scan_status);
          continue;
        }
        // set new wifi node state
        processNode(n.bssid, NO_ACTION, NO_ACTION, WIFI_NODE_READY_FOR_RSP, scan_status);
      }
      else
      {
        log_verbose(TAG, "@manageErrRsp(): RetriesApply");
        // lower retry cntr, lower periodic cntr and set new wifi node state
        processNode(n.bssid, LOWER_RETRY_CNTR, NO_ACTION, WIFI_NODE_READY_FOR_REQ, scan_status);
      }
    }

    // Check if there are nodes ready for response
    if( nodesForRsp() )
    {
      processRangRsp();
    }

    // clean up database, requests, timer, etc.
    cleanUp();

    // if there are retries, this will pick them up.
    log_verbose(TAG, "@manageErrRsp(): setupRequest() to pick up retries");
    setupRequest();
  }
  else
  {
    log_error(TAG, "@manageErrRsp(): Req failed but no nodes, this should not happen???");
  }
}

// find the node as many times as it is in the database...
void LOWIScheduler::processNode(LOWIScanMeasurement *inNode,
                                int32 retry_step,
                                int32 periodic_step,
                                eWiFiNodeState nextState,
                                LOWIResponse::eScanStatus scan_status)
{
  SCHED_ENTER()
  log_verbose(TAG, "@processNode(): retry_step(%u) periodic_step(%u)", retry_step, periodic_step);

  for(List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); ++it)
  {
    WiFiNodeInfo *info = *it;

    // if bssid matches and request id matches, we got a winner.
    // there is a chance that the same node could be on the data base more than once. If
    // that is the case, the state will distinguish them. If they happen to have the same
    // state also, then a response will be sent out for both and the tracking metrics
    // adjusted for both.
    if( (NULL != info->measResult)                           &&
        (0 == info->nodeInfo.bssid.compareTo(inNode->bssid)) &&
        (WIFI_NODE_MEAS_IN_PROGRESS == info->nodeState)      &&
        info->schReqId == info->measResult->request->getRequestId() )
    {
      log_debug(TAG, "@processNode(): "QUIPC_MACADDR_FMT"",
               QUIPC_MACADDR(info->nodeInfo.bssid));

      // handle node state
      info->nodeState = nextState;

      // handle the retry cntr
      if( LOWIResponse::SCAN_STATUS_NO_WIFI == scan_status )
      {
        log_verbose(TAG, "@processNode(): SCAN_STATUS_NO_WIFI no more retries...");
        // we're done retrying this wifi node, respond to client
        info->nodeState = WIFI_NODE_READY_FOR_RSP;
        // reset the retry counter, do not retry
        info->retryCntr = 0;
      }
      else if(retry_step > (int)MAX_RETRIES_PER_MEAS)
      {
        log_verbose(TAG, "@processNode(): reset retries to MAX_RETRIES_PER_MEAS");
        // results were good, reset retry cntr to the top
        info->retryCntr = MAX_RETRIES_PER_MEAS;
      }
      else
      {
        // adjust the retry cntr
        info->retryCntr -= retry_step;
        log_verbose(TAG, "@processNode(): retries left = %d", info->retryCntr);

        // retries exhausted
        if( 0 == info->retryCntr )
        {
          log_verbose(TAG, "@processNode(): no more retries...WIFI_NODE_READY_FOR_RSP");
          // we're done retrying this wifi node, respond to client
          info->nodeState = WIFI_NODE_READY_FOR_RSP;
          if( !info->nodeInfo.periodic )
          {
          // reset the retry counter
            info->retryCntr = info->nodeInfo.num_retries_per_meas;
          }
        }
      }

      // handle the periodic cntr
      if( info->nodeInfo.periodic )
      {
        info->periodicCntr -= periodic_step; // adjust the periodic cntr

        // for those periodic nodes which have retries and for which the retry
        // counter has gone done to zero, their periodic cntr needs to be adjusted
        if( (info->retryCntr == 0) && (info->nodeInfo.num_retries_per_meas > 0) )
        {
          info->periodicCntr -= 1;
          // reset the retry counter
          info->retryCntr = info->nodeInfo.num_retries_per_meas;
        }

        log_verbose(TAG, "@processNode(): periodicCntr left = %d", info->periodicCntr);
      }

      // if the state is WIFI_NODE_READY_FOR_RSP, we gather the request id of the clients
      // so we can put together all the WIFI_NODE_READY_FOR_RSP nodes that go to the same client
      if( WIFI_NODE_READY_FOR_RSP == info->nodeState )
      {
        // if periodic, add measurement number
        if( info->nodeInfo.periodic )
        {
          inNode->measurementNum = info->nodeInfo.num_measurements - info->periodicCntr;
        }

        // store the measurement information for this node for use when it's time
        // to put together a response to the client
        info->meas = inNode;
        addToClientList(info, scan_status);
      }
    } // if
  } // for loop
  SCHED_EXIT()
}

void LOWIScheduler::processNode(LOWIMacAddress bssid,
                                int32 retry_step,
                                int32 periodic_step,
                                eWiFiNodeState nextState,
                                LOWIResponse::eScanStatus scan_status)
{
  SCHED_ENTER()

  for(List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); ++it)
  {
    WiFiNodeInfo *info = *it;

    // There is a chance that the same node could be on the data base more than once. If
    // that is the case, the state will distinguish them. If they happen to have the same
    // state also, then a response will be sent out for both and the tracking metrics
    // adjusted for both.
    if( ( 0 == info->nodeInfo.bssid.compareTo(bssid)) &&
        (WIFI_NODE_MEAS_IN_PROGRESS == info->nodeState))
    {
      log_debug(TAG, "@processNode(): "QUIPC_MACADDR_FMT"",
                QUIPC_MACADDR(info->nodeInfo.bssid));

      // handle node state
      info->nodeState = nextState;

      // handle the retry cntr
      if( LOWIResponse::SCAN_STATUS_NO_WIFI == scan_status )
      {
        log_verbose(TAG, "@processNode(): SCAN_STATUS_NO_WIFI no more retries...");
        // we're done retrying this wifi node, respond to client
        info->nodeState = WIFI_NODE_READY_FOR_RSP;
        // reset the retry counter, do not retry
        info->retryCntr = 0;
      }
      else if(retry_step > (int)MAX_RETRIES_PER_MEAS)
      {
        log_verbose(TAG, "@processNode(): reset retries to MAX_RETRIES_PER_MEAS");
        // results were good, reset retry cntr to the top
        info->retryCntr = MAX_RETRIES_PER_MEAS;
      }
      else
      {
        // adjust the retry cntr
        info->retryCntr -= retry_step;
        log_verbose(TAG, "@processNode(): retries left = %d", info->retryCntr);

        if(0 == info->retryCntr)
        {
          log_verbose(TAG, "@processNode(): no more retries...WIFI_NODE_READY_FOR_RSP");
          // we're done retrying this wifi node, respond to client
          info->nodeState = WIFI_NODE_READY_FOR_RSP;
        }
      }

      // handle the periodic cntr
      if( info->nodeInfo.periodic )
      {
        info->periodicCntr -= periodic_step; // adjust the periodic cntr
        // for those periodic nodes which have retries and for which the retry
        // counter has gone done to zero, their periodic cntr needs to be adjusted
        if( (info->retryCntr == 0) && (info->nodeInfo.num_retries_per_meas > 0) )
        {
          info->periodicCntr -= 1;
          // reset the retry counter
          info->retryCntr = MAX_RETRIES_PER_MEAS;
        }
        log_verbose(TAG, "@processNode(): periodicCntr left = %d", info->periodicCntr);
      }

      // if the state is WIFI_NODE_READY_FOR_RSP, and since we're handling an error rsp,
      // we update the meas field for this node in its context structure
      if( WIFI_NODE_READY_FOR_RSP == info->nodeState )
      {
        // Since we're managing an error condition, there are no real rtt measurements for
        // this node. Therefore; fill up the measurement info with the request info but
        // without rtt or rssi measurements in the results. This, along with the scan
        // status will tell the client that this particular wifi node was unsuccessful.
        // In addition, this passes other relevant information about the node to the
        // client, and ensures that LOWI will process the response properly. (i.e. LOWI
        // doesn't process "bad" responses with wifi nodes in them, until now)
        LOWIScanMeasurement *r = new (std::nothrow)LOWIScanMeasurement();
        if( NULL != r )
        {
          r->bssid.setMac(info->nodeInfo.bssid);
          r->frequency = info->nodeInfo.frequency;
          r->isSecure  = false;
          r->type      = info->nodeInfo.nodeType;
          r->rttType   = info->nodeInfo.rttType;
          r->msapInfo  = NULL;
          r->cellPowerLimitdBm = 0;
          info->meas = r; // add it to the database

      // if periodic, add measurement number
          if( info->nodeInfo.periodic )
          {
            r->measurementNum = info->nodeInfo.num_measurements - info->periodicCntr;
          }
        }
        else
        {
            log_error(TAG, "@processNode(): memory allocation failure");
        }

        // store the measurement information for this node for use when it's time
        // to put together a response to the client
        log_verbose(TAG, "@processNode(): adding node to client list", info->periodicCntr);
        addToClientList(info, scan_status);
      }
    } // if
  } // for loop
  SCHED_EXIT()
}

bool LOWIScheduler::isWiFiNodeInDB(LOWIMacAddress bssid,
                                   bool &retries,
                                   bool &periodic,
                                   LOWIMeasurementResult *measResult)
{
  SCHED_ENTER()
  for(List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); ++it)
  {
    WiFiNodeInfo *info = *it;

    log_verbose(TAG, "@isWiFiNodeInDB: in_bssid("QUIPC_MACADDR_FMT") in_reqId(%u) db_bssid("QUIPC_MACADDR_FMT") db_reqId(%u)",
                QUIPC_MACADDR(bssid), measResult->request->getRequestId(),
                QUIPC_MACADDR(info->nodeInfo.bssid), info->schReqId);

    if( (0 == info->nodeInfo.bssid.compareTo(bssid)) &&
        (measResult->request->getRequestId() == info->schReqId) )
    {
      log_verbose (TAG, "@isWiFiNodeInDB: Found a match: bssid("QUIPC_MACADDR_FMT") reqId(%u)",
                   QUIPC_MACADDR(bssid), info->schReqId);
      retries = (info->nodeInfo.num_retries_per_meas > 0);
      periodic = info->nodeInfo.periodic;
      // save the meas_result that this node came in
      info->measResult = measResult;
      return true;
    }
  }
  return false;
}

bool LOWIScheduler::isWiFiNodeInDB(LOWIMacAddress bssid,
                                   bool &retries,
                                   bool &periodic,
                                   LOWIRequest *req)
{
  SCHED_ENTER()
  for(List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); ++it)
  {
    WiFiNodeInfo *info = *it;

    log_verbose(TAG, "@isWiFiNodeInDB: in_bssid("QUIPC_MACADDR_FMT") in_reqId(%u) db_bssid("QUIPC_MACADDR_FMT") db_reqId(%u)",
                QUIPC_MACADDR(bssid), req->getRequestId(),
                QUIPC_MACADDR(info->nodeInfo.bssid), info->schReqId);

    if( (0 == info->nodeInfo.bssid.compareTo(bssid)) &&
        (req->getRequestId() == info->schReqId) )
    {
      log_verbose (TAG, "@isWiFiNodeInDB: Found a match: bssid("QUIPC_MACADDR_FMT") reqId(%u)",
                   QUIPC_MACADDR(bssid), info->schReqId);
      retries = (info->nodeInfo.num_retries_per_meas > 0);
      periodic = info->nodeInfo.periodic;
      // save the meas_result that this node came in
      info->measResult = NULL;
      return true;
    }
  }
  return false;
}

void LOWIScheduler::processRangRsp()
{
  SCHED_ENTER()

  // Iterate over the list of client requests and send responses to those clients that
  // have nodes ready for response
  for( List<LOWIClientInfo*>::Iterator it = mClients.begin(); it != mClients.end(); ++it )
  {
    LOWIClientInfo *client = *it;

    if( (client->scanMeasVec.getNumOfElements() > 0) &&
        (NULL != client->clientReq)                  &&
        (NULL != client->result) )
    {
      // check for internal messages
      if (NULL != client->iReq)
      {
        log_verbose(TAG, "@processRangRsp: internal message detected");
        processInternalMsg(client);
      }
      else
      {
        LOWIMeasurementResult *result = new(std::nothrow) LOWIMeasurementResult;
        if ( NULL != result )
        {
          result->measurementTimestamp = client->result->measurementTimestamp;
          result->scanStatus           = client->result->scanStatus;
          result->scanType             = client->result->scanType;
          result->request              = client->clientReq;
          result->scanMeasurements     = client->scanMeasVec;

          log_verbose(TAG, "@processRangRsp: Creating response: status(%s) numBssids(%u) reqId(%u) originator(%s) reqType(%u)",
                      SCAN_STATUS[result->scanStatus],
                      result->scanMeasurements.getNumOfElements(),
                      result->request->getRequestId(),
                      result->request->getRequestOriginator(),
                      result->request->getRequestType());

          // send the response to the client
          LOWIRequest *origReq = client->clientReq;
          mController->sendResponse(*origReq, result);
          log_verbose(TAG, "@processRangRsp: Response sent");
          // clean up any result information
          client->scanMeasVec.flush();
          client->result = NULL;
          delete result;
        }
      }
    }
  }
}

void LOWIScheduler::addToClientList(WiFiNodeInfo *pNode,
                                    LOWIResponse::eScanStatus scan_status)
{
  SCHED_ENTER()
  for( List<LOWIClientInfo*>::Iterator it = mClients.begin(); it != mClients.end(); ++it)
  {
    LOWIClientInfo *client = *it;
    if( pNode->origReq->getRequestId() == client->clientReq->getRequestId() )
    {
      // store the scan measurement for this node which will go to this client in the rsp
      client->scanMeasVec.push_back(pNode->meas);
      // store measurement information to be used in the rsp to the client
      client->result = pNode->measResult;

      // This case arises when a request fails to be sent to the wifi driver and the
      // client needs to be notified. Since the measurement result can't be null, we
      // create a fake one here.
      if( NULL == client->result )
      {
        client->result = new (std::nothrow) LOWIMeasurementResult;
        if( NULL != client->result )
        {
          client->result->measurementTimestamp = LOWIUtils::currentTimeMs();
          client->result->scanStatus           = scan_status;
          client->result->scanType             = LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_UNKNOWN;
          client->result->request              = client->clientReq;
        }
        else
        {
          log_error(TAG, "@addToClientList(): memory allocation failure");
        }
      }
    }
  }
}

void LOWIScheduler::dataBaseCleanup()
{
  SCHED_ENTER()

  if(mNodeDataBase.getSize() == 0)
  {
    log_info(TAG, "Database is empty...nothing to clean up");
    return;
  }

  log_info(TAG, "Database status before cleanup...");
  printDataBase();

  for( List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); )
  {
    WiFiNodeInfo *info = *it;

    LOWIPeriodicRangingScanRequest* r = (LOWIPeriodicRangingScanRequest*)info->origReq;
    int64 timeout = r->getTimeoutTimestamp();

    // remove nodes that have "expired" per their own timeout timestamp
    if( 0 != timeout && (timeout < LOWIUtils::currentTimeMs()) )
    {
      delete info;
      it = mNodeDataBase.erase(it);
      continue;
    }
    else
    {
      // for periodic nodes, check to see if the periodic counter has been exhausted
      if( info->nodeInfo.periodic )
      {
        if( 0 == info->periodicCntr )
        {
          log_info(TAG, "Periodic node fully serviced: "QUIPC_MACADDR_FMT"",
                   QUIPC_MACADDR(info->nodeInfo.bssid));

          delete info;
          it = mNodeDataBase.erase(it); // remove the wifi node from database
          mNumPeriodicNodesInDB--;     // adjust the periodic node count
        }
        else
        {
          // periodic node still valid, reset state if it's not on a retry mode;
          // A node that is  going to be retried, would have state
          // WIFI_NODE_READY_FOR_REQ when it gets here.  That node would not wait
          // for teh timer but would be requested right away
          if( info->nodeState != WIFI_NODE_READY_FOR_REQ )
          {
          info->nodeState = WIFI_NODE_WAITING_FOR_TIMER;
          }
          ++it;
        }
      }
      else
      { // This is a one-shot node
        // At this point, a one-shot node that is WIFI_NODE_READY_FOR_RSP means that the
        // response already went out to the client. The wifi node can be removed.
        // If the node is to be retried, the state would be WIFI_NODE_READY_FOR_REQ
        // instead.
        if( WIFI_NODE_READY_FOR_RSP == info->nodeState )
        {
          log_info(TAG, "One-shot node fully serviced: "QUIPC_MACADDR_FMT"",
                   QUIPC_MACADDR(info->nodeInfo.bssid));

          delete info;
          it = mNodeDataBase.erase(it);
        }
        else
        {
          ++it;
        }
      }
    }
  }

  log_info(TAG, "Database status after cleanup...");
  printDataBase();
}

void LOWIScheduler::deleteClientInfo()
{
  SCHED_ENTER()
  bool reqStillUsed = false;

  // iterate over the client information list
  for( List<LOWIClientInfo*>::Iterator it = mClients.begin(); it != mClients.end(); )
  {
    LOWIClientInfo *clInfo = *it;
    reqStillUsed = false;

    // for each client request, iterate over the node database checking if any node
    // still is using the request
    for( List<WiFiNodeInfo* >::Iterator iter = mNodeDataBase.begin(); iter != mNodeDataBase.end(); )
    {
      WiFiNodeInfo *node = *iter;
      if( clInfo->clientReq->getRequestId() == node->origReq->getRequestId() )
      {
        reqStillUsed = true;
        break;
      }
      else
      {
        ++iter;
      }
    }

    // no WiFiNode in the data base matched this request which means that all nodes
    // that came in this request have been serviced, hence, free the memory
    if( false == reqStillUsed )
    {
      log_debug(TAG, "@deleteClientInfo: deleting reqId(%u) originator(%s), reqType(%s)",
               clInfo->clientReq->getRequestId(),
               clInfo->clientReq->getRequestOriginator(),
               REQUEST_TYPE[clInfo->clientReq->getRequestType()]);

      if ( NULL != clInfo->iReq )
      {
        delete clInfo->iReq;
      }

      delete clInfo;  // remove the client information (request, etc).
      it = mClients.erase(it); // remove it from the client info list
    }
    else
    {
        ++it;
    }
  } //for
}

void LOWIScheduler::processCancelReq(LOWIRequest *req)
{
  SCHED_ENTER()

  if( mNodeDataBase.getSize() == 0 )
  {
    log_debug(TAG, "@processCancelReq(): Data base is empty...nothing to cancel");
    return;
  }

  LOWICancelRangingScanRequest *r = (LOWICancelRangingScanRequest*)req;
  vector<LOWIMacAddress> v        = r->getBssids();
  char *reqOriginator             = (char*)r->getRequestOriginator();

  // check if this wifinode to cancel is in the data base
  for( uint32 ii = 0; ii < v.getNumOfElements(); ii++ )
  {
    bool found = false;

  for( List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); )
  {
    WiFiNodeInfo *info = *it;

    // compare request originators
    uint32 val = strncmp(reqOriginator,
                         info->origReq->getRequestOriginator(),
                         sizeof(reqOriginator));

      if( info->nodeInfo.periodic && (0 == info->nodeInfo.bssid.compareTo(v[ii])) && !val )
      {
        // cancelling a periodic node
        log_debug(TAG, "@processCancelReq(): Periodic node cancelled: "QUIPC_MACADDR_FMT"",
                  QUIPC_MACADDR(info->nodeInfo.bssid));
        mNumPeriodicNodesInDB--; // adjust the periodic node count
        delete info;
        it = mNodeDataBase.erase(it);
        found = true;
        break;
      }
      else if( 0 == info->nodeInfo.bssid.compareTo(v[ii]) && !val )
      {
        // cancelling a one-shot node
        log_debug(TAG, "@processCancelReq(): One-shot node cancelled: "QUIPC_MACADDR_FMT"",
                  QUIPC_MACADDR(info->nodeInfo.bssid));
        delete info;
        it = mNodeDataBase.erase(it);
        found = true;
        break;
      }
      else
      {
        ++it; // next node in data base
      }
    }

    if( false == found )
    {
      log_verbose(TAG, "@processCancelReq(): Node not found: "QUIPC_MACADDR_FMT"",
                  QUIPC_MACADDR(v[ii]));
    }
  }
}

void LOWIScheduler::printDataBase()
{
  char header[256];
  char line[256];

  log_debug(TAG,"****************************** WiFiNode Database *****************************************************************************************");
  log_debug(TAG,"Num of remaining nodes: %u", mNodeDataBase.getSize());
  snprintf(header, sizeof(header), "%-17s %-4s %-5s %-5s %-6s %-4s %-4s %-12s %-7s %-7s %-7s %-7s %-10s %-10s %-27s\n", "BSSID", "Freq", "nodeT", "rttT", "BW", "pkts", "cont", "period", "numMeas", "perCntr", "retries", "retCntr", "ReqId", "schId", "nodeState");
  log_debug(TAG,"%s", header);
  for( List<WiFiNodeInfo* >::Iterator it = mNodeDataBase.begin(); it != mNodeDataBase.end(); )
  {
    WiFiNodeInfo *node = *it;
    snprintf(line, sizeof(line), ""QUIPC_MACADDR_FMT" %-4u %-5s %-5s %-6s %-4u %-4u %-12u %-7u %-7d %-7u %-7d %-10u %-10u %-27s\n",
              QUIPC_MACADDR(node->nodeInfo.bssid),
              node->nodeInfo.frequency,
              NODE_STR_[node->nodeInfo.nodeType],
              RTT_STR_[node->nodeInfo.rttType],
              BW_STR_[node->lastBw],
              node->nodeInfo.num_pkts_per_meas,
              node->nodeInfo.periodic,
              node->nodeInfo.meas_period,
              node->nodeInfo.num_measurements,
              node->periodicCntr,
              node->nodeInfo.num_retries_per_meas,
              node->retryCntr,
              node->origReq->getRequestId(),
              node->schReqId,
              WIFI_NODE_STATE[node->nodeState]);
    log_debug(TAG,"%s", line);

    ++it;
  }
  log_debug(TAG,"******************************************************************************************************************************************");
}

bool LOWIScheduler::isReqOk(LOWIRequest* req)
{
  if(NULL == req)
  {
    log_debug(TAG, "Request is NULL");
    return false;
  }
  else if( (req->getRequestType() > LOWIRequest::CANCEL_RANGING_SCAN) ||
           (req->getRequestType() < LOWIRequest::DISCOVERY_SCAN) )
  {
    log_debug(TAG, "Unrecognizable request type");
    return false;
  }
  else
  {
    log_debug(TAG, "Request into scheduler: %s",
              REQUEST_TYPE[req->getRequestType()]);
    return true;
  }
}

void LOWIScheduler::cleanUp()
{
  SCHED_ENTER()
  // Now that responses have been sent, remove the nodes that
  // have been fully serviced or have expired
  dataBaseCleanup();
  log_debug (TAG, "@cleanUp(): NumPeriodicNodesInDB left: %u", mNumPeriodicNodesInDB);

  // "kill" the timer if there aren't any more periodic nodes to service
  if( (0 == mNumPeriodicNodesInDB) && mTimerRunning )
  {
    if( NULL != mSchedulerTimerData )
    {
      log_debug (TAG, "@cleanUp(): removing local timer");
      mController->removeLocalTimer(mController->getTimerCallback(),
                                    mSchedulerTimerData);
      mTimerRunning = false;
      mCurrTimeoutMsec = 0;
    }
  }

  // remove any client requests that are no longer valid
  deleteClientInfo();
}

bool LOWIScheduler::nodesForRsp()
{
  SCHED_ENTER()
  for( List<LOWIClientInfo*>::Iterator it = mClients.begin(); it != mClients.end(); ++it)
  {
    LOWIClientInfo *client = *it;
    if( client->scanMeasVec.getNumOfElements() > 0 )
    {
      log_debug(TAG, "@nodesForRsp(): # nodes for this client(%u)",
                  client->scanMeasVec.getNumOfElements());
      return true;
    }
  }
  log_verbose(TAG, "@nodesForRsp(): no nodes ready for rsp");
  return false;
}

void LOWIScheduler::saveIReq(LOWIRequest *req, LOWIInternalMessage *iReq)
{
  // Iterate over the list of client requests and find the matching request
  for( List<LOWIClientInfo*>::Iterator it = mClients.begin(); it != mClients.end(); ++it )
  {
    LOWIClientInfo *client = *it;

    // requests are equal if they have the same request id
    if ( client->clientReq->getRequestId() == req->getRequestId() )
    {
      // this request is an internal message
      client->saveIReq(iReq);
    }
  }
}

uint32 LOWIScheduler::calculateRangeForFTMRR(vector <LOWIMeasurementInfo *> &measInfo)
{
  uint32 range  = 0;
  int32 rttSum  = 0;
  int32 rttMean = 0;

  if ( measInfo.getNumOfElements() > 0 )
  {
    for (uint32 ii = 0; ii < measInfo.getNumOfElements(); ++ii)
    {
      rttSum += measInfo[ii]->rtt_ps;
    }

    rttMean = rttSum/measInfo.getNumOfElements();

    range = uint32((float)rttMean * RTT_DIST_CONST / FTMRR_RANGE_UNITS);
  }
  return range;
}

void LOWIScheduler::processInternalMsg(LOWIClientInfo *client)
{
  if ( NULL == client )
  {
    return;
  }

  // find what type of internal message it is and process it.
  if ( LOWIInternalMessage::LOWI_IMSG_FTM_RANGE_REQ == client->iReq->getInternalMessageType() )
  {
    log_verbose(TAG, "@processRangRsp: FTMRR message detected");
    vector<LOWIRangeEntry> vSuccess;
    vector<LOWIErrEntry> vErr;

    // process the node info
    // go through all the APs in the client list
    for (uint32 ii = 0; ii < client->scanMeasVec.getNumOfElements(); ii++)
    {
      if ( client->scanMeasVec[ii]->targetStatus ==
           LOWIScanMeasurement::LOWI_TARGET_STATUS_SUCCESS )
      {
        LOWIRangeEntry entry;
        uint64 ts = client->scanMeasVec[ii]->rttMeasTimeStamp;
        entry.measStartTime = (uint32)(ts/LOWI_10K);
        entry.bssid = client->scanMeasVec[ii]->bssid;
        entry.range = calculateRangeForFTMRR(client->scanMeasVec[ii]->measurementsInfo);
        log_verbose(TAG, "@processRangRsp: bssid("QUIPC_MACADDR_FMT") range(%u) measStartTime(%u)",
                    QUIPC_MACADDR(entry.bssid), entry.range, entry.measStartTime);
        vSuccess.push_back(entry);
      }
      else
      {
        LOWIErrEntry entry;
        switch ( client->scanMeasVec[ii]->targetStatus )
        {
          case LOWIScanMeasurement::LOWI_TARGET_STATUS_RTT_FAIL_TARGET_BUSY_TRY_LATER:
            {
              entry.errCode = REQ_FAILED_AT_AP;
            }
            break;
          case LOWIScanMeasurement::LOWI_TARGET_STATUS_RTT_FAIL_TARGET_NOT_CAPABLE:
            {
              entry.errCode = REQ_INCAPABLE_AP;
            }
            break;
          default:
            {
              entry.errCode = TX_FAIL;
            }
            break;
        }
        uint64 ts = client->scanMeasVec[ii]->rttMeasTimeStamp;
        entry.measStartTime = (uint32)(ts/LOWI_10K);
        entry.bssid = client->scanMeasVec[ii]->bssid;
        entry.errCode = qc_loc_fw::TX_FAIL;
        log_verbose(TAG, "@processRangRsp: bssid(" QUIPC_MACADDR_FMT") errCode(%d) measStartTime(%u)",
                    QUIPC_MACADDR(entry.bssid), entry.errCode, entry.measStartTime);
        vErr.push_back(entry);
      }
    }
    // create the request that will send the FTMRR report to the driver
    LOWIFTMRangeReqMessage *re = (LOWIFTMRangeReqMessage *)client->iReq;
    LOWIFTMRangeRprtMessage *r =
    new (std::nothrow) LOWIFTMRangeRprtMessage(createSchedulerReqId(),
                                               re->getRequesterBssid(),
                                               re->getSelfBssid(),
                                               re->getFrequency(),
                                               re->getDiagToken(),
                                               re->getMeasToken(),
                                               vSuccess,
                                               vErr);
    if ( NULL != r )
    {
      // pass the request to the lowi-controller who will either send it right away or
      // put it in the pending queue
      log_verbose(TAG, "@processRangRsp: calling requestMeasurements()");
      mController->requestMeasurements(r);
    }
  }
}

void LOWIScheduler::processNanNodes()
{
  // todo
}
