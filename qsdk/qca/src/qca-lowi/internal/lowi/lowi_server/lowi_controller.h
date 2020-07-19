#ifndef __LOWI_CONTROLLER_H__
#define __LOWI_CONTROLLER_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Controller Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Controller

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

#include <base_util/postcard.h>
#include <mq_client/mq_client_controller.h>
#include <base_util/queue.h>
#include <lowi_server/lowi_event_receiver.h>
#include <lowi_server/lowi_scan_result_listener.h>
#include <inc/lowi_request.h>
#include <inc/lowi_response.h>
#include <lowi_server/lowi_event_receiver.h>
#include <lowi_server/lowi_cache_manager.h>
#include <lowi_server/lowi_scan_result_receiver.h>
#include <lowi_server/lowi_event_dispatcher.h>
#include <lowi_server/lowi_measurement_result.h>
#include <lowi_server/lowi_internal_message.h>
#include <lowi_server/lowi_internal_message_listener.h>
#include <lowi_server/lowi_netlink_socket_receiver.h>
namespace qc_loc_fw
{
class LOWIScheduler;
class LOWIWifiDriverInterface;

enum eRequestStatus
{
  SUCCESS,
  NO_WIFI,
  NOT_SUPPORTED,
  INTERNAL_ERROR
};

enum eTimerId
{
  RANGING_REQUEST_SCHEDULER_TIMER = 0
};

class LOWIController;

class TimerData: public qc_loc_fw::TimerDataInterface
{
public:
  eTimerId mTimerId;
  LOWIController* mController;

  TimerData(eTimerId timer_id, LOWIController* controller) :
  mTimerId(timer_id), mController (controller)
  {
  }

  bool operator ==(const qc_loc_fw::TimerDataInterface & rhs) const
  {
    // we'd be dead if this is not really an instance of TimerData
    // but we don't have RTTI...
    const TimerData * const myRhs = (const TimerData * const ) &rhs;
    return(mTimerId == myRhs->mTimerId);
  }
};

class TimerCallback: public qc_loc_fw::MqClientFunctionalModuleBase
{
private:
  static const char * const   TAG;
public:

  TimerCallback() {}
  int init() {return 0;}
  int deinit() {return 0;}
  void timerCallback(const qc_loc_fw::TimerDataInterface * const data);
};

/**
 * This class is the main controller of LOWI
 * Creates EventReceiver, EventDispatcher,
 * CacheManager and WifiDriverInterface classes.
 *
 * This class controls the communication with the IPC Hub.
 * Sends the received Request to EventReceiver and allows to
 * send the Response out as well.
 *
 * It handles the incoming Post cards from the IPC Hub as well
 * as the ScanResultListener thread which sends the measurements
 * using the Post cards.
 *
 * For any request received, it first analyzes the request and then
 * puts the request in the pending request queue if it could not be serviced
 * right away
 */
class LOWIController:
public qc_loc_fw::MqClientControllerBase, LOWIScanResultReceiverListener,
LOWIEventReceiverListener, LOWIInternalMessageReceiverListener
{
private:
  static const char * const TAG;
  /** Handle to EventReceiver*/
  LOWIEventReceiver*             mEventReceiver;
  /** Handle to CacheManager*/
  LOWICacheManager*              mCacheManager;
  /** Handle to EventDispatcher*/
  LOWIEventDispatcher*           mEventDispatcher;
  /** Queue to keep all the Request in*/
  Queue<LOWIRequest*>            mPendingRequestQueue;
  /** List to keep all the Requests for async scan results*/
  List<LOWIRequest*>             mAsyncScanRequestList;
  /** Current Request*/
  LOWIRequest*                   mCurrentRequest;
  /** DiscoveryScanResultReceiver*/
  LOWIScanResultReceiver*        mDiscoveryScanResultReceiver;
  /** RangingScanResultReceiver*/
  LOWIScanResultReceiver*        mRangingScanResultReceiver;
  /** ScanRequestSender   */
  LOWIScanResultReceiver*        mScanRequestSender;
  /** NetLinkSocketReceiver   */
  LOWINetlinkSocketReceiver*     mNetlinkSocketReceiver;
  /** LOWIWifiDriverInterface */
  LOWIWifiDriverInterface*       mWifiDriver;
  /** Timer Data & callback */
  TimerData*                     mPassiveListeningTimerData;
  TimerCallback*                 mTimerCallback;
  /** LOWIScheduler */
  LOWIScheduler*                 mScheduler;
  int32                          mMaxQueueSize;
  int32                          mFreshScanThreshold;
  bool                           mWifiStateEnabled;
  bool                           mReadDriverCacheDone;
  bool                           mUseLowiLp;
  /** Internal capability request */
  LOWICapabilityRequest*         mInternalCapabilityRequest;
  /** wifi state received from RTM message */
  eWifiIntfState                 mNlWifiStatus;

  // private copy constructor and assignment operator so that the
  // the instance can not be copied.
  LOWIController( const LOWIController& rhs );
  LOWIController& operator=( const LOWIController& rhs );

  /**
   * Generates LOWIResponse.
   * @param LOWIRequest& Request for which the response is to be genarated
   * @param LOWIMeasurementResult* Measurement Result
   */
  LOWIResponse* generateResponse (LOWIRequest& req,
      LOWIMeasurementResult* meas_result);

  /**
   * Sends LOWIResponse for error conditions
   * @param LOWIRequest& Request for which the response is to be generated
   * @param LOWIResponse::eScanStatus
   * @return true if able to generate the response, false otherwise
   */
  bool sendErrorResponse (LOWIRequest& req,
      LOWIResponse::eScanStatus status);

  /**
   * Processes the incoming request
   * @param LOWIRequest*
   */
  void processRequest (LOWIRequest* request);

  /**
   * Sends LOWIResponse to the Request originator
   * @param LOWIRequest& Request for which the response is to be generated
   * @param LOWIMeasurementResult* Measurement Result
   * @return true if able to generate the response, false otherwise
   */
  bool sendResponse (LOWIRequest& req, LOWIMeasurementResult* meas_result);

  /**
   * Issues a new request from the pending queue if any
   * If there is no pending request, issues a passive listening request
   */
  void issueRequest ();

  /**
   * Spawns a new ScanResult Listener class instance for every request
   * and starts the thread to get the measurements. Measurements are then
   * received through a callback. The class and the thread are destroyed
   * in the measurement callback.
   * @param LOWIRequest* LOWIRequest to be sent
   * @return request status
   */
  eRequestStatus requestMeasurements (LOWIRequest* r);

  /**
   * Notifies the callback when a timer expires
   * @param eTimerId Id of the Timer that just got expired.
   */
  void timerCallback (eTimerId id);

  /**
   * Checks and issues a passive listening request if
   * no request is currently executing.
   */
  void issuePassiveListeningRequest ();

  /**
   * Processes the measurement results
   * @param LOWIMeasurementResult*
   */
  void processMeasurementResults (LOWIMeasurementResult* meas_result);

  /**
   * Process the request immediately and send the response
   * If the request can not be serviced immediately return error
   * @param LOWIRequest*
   * @return error code (0 - serviced immediately)
   *                    (-1 - can not be serviced immediately)
   */
  int32 processRequestImmediately (LOWIRequest* req);

  /**
   * Add the request to Async Scan request list if the request
   * is Async scan request.
   * @param LOWIRequest*
   * @return  code (0 - Async Scan request)
                   (-1 - Async scan request but can't be added. List full
                   (-2 - Not an Async Scan request)
   */
  int32 addRequestToAsyncScanRequestList (LOWIRequest* req);

  /**
   * Sends the responses to Async scan requests.
   * Updates the list with only valid requests and drops the
   * requests silently that have expired.
   * Notifies to the requests in the list only if the results
   * correspond to passive listening or discovery scan.
   * Does not notify a client if the results were generated
   * due to the client issuing the discovery scan request
   * @param LOWIRequest* Current request that was pending
   *        to receive a response
   * @param LOWIMeasurementResult* Pointer to results received
   * @return bool Returns true if able to generate any response
   *                      false otherwise.
   */
  bool sendAsyncScanResponse (LOWIRequest* req,
      LOWIMeasurementResult* meas_result);

  /**
   * Gets the measurements from the cache based on the request
   * and also returns the appropriate error code
   * @param int64 Cache timestamp
   * @param int64 Fall back timestamp
   * @param LOWIDiscoveryScanRequest* Request
   * @param LOWIMeasurementResult& Output result should be in this
   *
   * @return int32 (0 - success), (-1 - failure)
   */
  int32 getMeasurementResultsFromCache
  (int64 cache_ts, int64 fb_ts, LOWIDiscoveryScanRequest* req,
      LOWIMeasurementResult & result);

  /**
   * Checks is wifi is enabled or not
   * @return true if wifi is enabled or false
   */
  boolean isWifiEnabled ();

  /**
   * Reads the Cached measurements from the underlying wifi driver.
   * Should be trigerred only if wifi is enabled.
   */
  void readCachedMeasurementsFromDriver ();

  /**
   * Creates the wifi driver
   * @return Returns true if success, false otherwise
   */
  bool createWifiDriver ();
  /**
   * Creates the Scan Result Receiver threads
   * @return Returns true if success, false otherwise
   */
  bool createScanResultReceiver ();

  /**
   * Creates the netlink socket receiver thread
   * @return Returns true if success, false otherwise
   */
  bool createNetLinkSocketReceiver();
  /**
   * Terminates the Wifi driver
   * @return Returns true if success, false otherwise
   */
  bool terminateWifiDriver ();

  /**
   * Terminates the Scan Result Receiver threads
   * @return Returns true if success, false otherwise
   */
  bool terminateScanResultReceiver ();

  /**
   * Manage the request before sending it to the wifi driver. This will
   * apply to ranging requests. These requests will be scheduled and their
   * periodicity and retries will be managed here.
   * For any other request, this will be a pass through.
   *
   * @param req: request to be managed
   * @return LOWIRequest*: pointer to new ranging scan request
   */
  LOWIRequest* manageRangingRequest(LOWIRequest* req);

  /**
   * Terminates the ranging scan request scheduler
   */
  void terminateScheduler();

  /**
   * Creates a lowi scheduler for managing ranging and periodic ranging scans
  * @return bool: true if scheduler successfully created, false if not
   */
  bool createScheduler();

  /**
   * Returns the mTimerCallback instance so that others can use the
   * lowi controller timer
   *
   * @return TimerCallback*
   */
  TimerCallback* getTimerCallback();

  /**
   *  Processes requests from the pending request queue. It looks at the various
   *  requests sitting on the pending queue and determines whether they are
   *  still valid. Any request that is no longer valid is removed from the
   *  queue. Those requests that can be processed immediately are processed.
   */
  void processPendingRequests();

  /**
   * Getter function used to retrieve the current request tracking pointer:
   * mCurrentRequest. The scheduler uses this wrapper to retrieve the tracking
   * pointer when it is processing a request/response.
   *
   * @return LOWIRequest*: current request tracking pointer
   */
  LOWIRequest* getCurrReqPtr();

  /**
   * Setter function used to set the current request tracking pointer:
   * mCurrentRequest.
   *
   * @param r: LOWI request to which the current tracking pointer is to point
   */
  void setCurrReqPtr(LOWIRequest* r);

  /**
   * Process location ANQP request through wpa_supplicant. This request is
   * processed immediately and nothing is expected back (for now). The
   * function forms the command to send to the wpa_supplicant.
   *
   * @param request: lowi request to be serviced
   */
  void processNeighborReportRequest(LOWIRequest *request);


  /**
   * Timecallback class will notify us about the callback
   */
  friend class TimerCallback;

  /**
   * scheduler class will handle periodic ranging requests and retries
   */
  friend class LOWIScheduler;

public:
  /**
   * Constructor
   * @param char*:  Name of the server socket to connect to
   * @param char*:  Name with path of the config file
   */
  LOWIController(const char * const socket_name,
      const char * const config_name);
  /** Destructor*/
  virtual ~LOWIController();

  /**
   * Notification callback that the scan measurement results are received.
   * The notification comes in the ScanResultReceiver thread context.Post the
   * results to the BlockingQueue as a blob.
   *
   * The message will then be read by the LOWIController in it's own thread
   * context. It is the responsibility of the LOWIController to free the
   * allocated memory after the results are processed.
   *
   * @param LOWIMeasurementResult* Scan measurement result
   */
  void scanResultsReceived(LOWIMeasurementResult* measurements);
  /**
   * Notification callback that an internal LOWI request has been received.
   * The notification comes in the ScanResultReceiver thread context. Post the
   * results to the BlockingQueue as a blob.
   *
   * The message will then be read by the LOWIController in it's own thread
   * context. It is the responsibility of the LOWIController to free the
   * allocated memory after the request has been processed to completion.
   *
   * @param iRequest: internal LOWI request
   */
  void internalMessageReceived(LOWIInternalMessage *iRequest);

  /**
   * Notifies to listener, the Registration status with OS-Agent
   * @param bool true for successful registration, false otherwise
   * Notification callback that the wifi status changes is received
   * from netlinksocketreceiver thread.
   * The notification comes in the NetLinkSocketReceiver thread context.
   * Post the status to the BlockingQueue.
   *
   * The message will then be read by the LOWIController in it's own thread
   * context.
   *
   * @param eWifiIntfState Wifi Interface state
   */
  void wifiStateReceived(eWifiIntfState state);
  /**
   * Notifies to the listener about the wifi state changes
   * @param eWifiIntfState Wifi Interface state
   */
  virtual void updateWifiState (eWifiIntfState state);

protected:
  /**
   * Called by the base class during initialization.
   * During initialization, creates the ScanResultListener class is a
   * separate thread and passes the protected BlockingMsgQueue to it as a
   * pointer which could be used to receive messages from ScanResultListener
   * @return int  0 for successful initialization
   */
  int _init();
  /**
   * Called by the base class upon receiving a message.
   *
   * Main loop of the Controller
   * Blocks on the msg queue to receive PostCard from either
   * IPC Hub or ScanResultListener.
   * On receiving a post card in the Msg Queue, the post card is handed over
   * to EventReceiver to analyze the type of message.
   * if the message is a measurement
   *  Check if there is any Request in CurrentRequest variable.
   *    if there is no such request
   *      Either we received the passive results
   *      or the request was dropped waiting for a response during timeout.
   *      In either case, keep the measurements in the cache.
   *    else (Request exists)
   *      Check to match the current request with results on best match basis
   *      if request matches results
   *        Update the cache with the results
   *        Respond to the request results
   *      else - request does not match
   *        Update the cache with the reults
   *        No need to send response. Keep waiting for valid results until
   *        request time out
   *      else - can't tell if the request is related to results
   *        Update the cache with the results
   *        Respond to request
   *
   * if the Request is already processed
   *  Cache the ScanMeasurement
   *  Generated the Response for the Request
   *  Process any Cache Request from the Cache buffer queue.
   *  Check the Request queue for any pending Request
   *  if found
   *    Run the similarity check algorithm to determine if the Request
   *    could be serviced from Cache and generate Response for all such Request
   *    For the remaining Request, put the top most Request in the outgoing
   *    Msg queue to be handled by the WLANController.
   *  if not found
   *    Wait for another msg in the incoming msg queue
   * if the Request is new (Not already processed)
   *  if the Request is CACHE_ONLY then check the buffer bit
   *    if bit is set, put the Request in Cache buffer queue
   *    else generate the Response from the measurements in cache
   *  if the Request is FRESH_ONLY then run the similarity check algorithm
   *    if there is a Request being processed by WLANController,
   *      put the Request in Queue
   *    else (similarity check algorithm)
   *      Look at the DB and find out if the last scan time stamp for each
   *      channel requested is still with in the threshold of current time
   *      (Relatively considered fresh)
   *      i.e. current time < last channel scan time + threshold
   *      AND the channel is searched already (might have yielded
   *      no measurements but still have the time stamp of last
   *      time it was searched)
   *      if both the above criteria are satisfied for all channels
   *        requested, then generate the Response from the measurements
   *        in cache.
   *      else put the Request in the Request queue
   *  if the Request is CACHE_FALLBACK then run the CACHE_ONLY algorithm first
   *    and if no results are found then run the FRESH_ONLY algorithm.
   *
   * General steps:
   *
   * Block for a post card on the blocking queue
   * Receive the post card
   * Send post card to Event Receiver to examine and convert to a Msg container
   *  which could be a Request or ScanMeasurement
   * In case of ScanMeasuremets,
   *  Check if the batching was done
   *  if yes
   *    hold on to this set of results in memory and
   *      send remaining request
   *  else
   *    combine all the responses from previous batch scan requests
   *      send for RTT processing if the measurements are RTT related.
   *    Cache the results
   *    respond to the request
   *  Check the pending queue and scan all the requests to find out which
   *  ones could be serviced from the cache. Drop any request which are already
   *  expired per there timeout time stamp
   *  if found
   *    Send responses to such requests from cache
   *  else
   *    Take the top most pending request and keep it in current request
   *    Send a request to WifiDriver using WifiDriverInterface
   *  Block on the Msg Q to receive the response
   *
   * In case of a new Request
   *  Check if there is any current request
   *  if none
   *    Execute the request right away
   *  else
   *    Check if the request could be serviced from cache
   *    if yes
   *      Execute the request and respond to it right away
   *  Check the capacity of the pending queue.
   *  Does it have room
   *    if yes
   *      Put the request in pending queue
   *    else
   *      Generate busy response
   *
   *
   * @param InPostcard Message received
   */
  void _process(qc_loc_fw::InPostcard * const in_msg);
  /**
   * Called by the base class while thread exit
   */
  void _shutdown();

};

} // namespace
#endif //#ifndef __LOWI_CONTROLLER_H__
