#ifndef __LOWI_ROME_FSM_H__
#define __LOWI_ROME_FSM_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI ROME Finite State Machine Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Rome Finite State Machine

Copyright (c) 2014-2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================*/

#include <inc/lowi_defines.h>
#include <inc/lowi_request.h>
#include <base_util/vector.h>
#include <lowi_server/lowi_measurement_result.h>
#include <lowi_server/lowi_scan_result_listener.h>
#include <lowi_server/lowi_cache_manager.h>
#include "osal/wifi_scan.h"
#include "wlan_capabilities.h"
#include "lowi_rome_ranging.h"

#define MAX_DIFFERENT_CHANNELS_ALLOWED 65

namespace qc_loc_fw
{

/* Channel spacing defines */
#define CHANNEL_SPACING_10MHZ 10
#define CHANNEL_SPACING_30MHZ 30
typedef enum
{
  ROME_NEW_REQUEST_ARRIVED,
  ROME_TERMINATE_THREAD,
  ROME_MAX_PIPE_EVENTS
} RomePipeEvents;

typedef enum
{
  /* Events from LOWI Controller */
  EVENT_START_THREAD = 0,
  EVENT_RANGING_REQ,
  EVENT_CONFIGURATION_REQ,
  EVENT_INVALID_REQ,
  EVENT_TERMINATE_REQ,
  /* Events from NL Socket */
  EVENT_REGISTRATION_SUCCESS,
  EVENT_RANGING_CAP_INFO,
  EVENT_CHANNEL_INFO,
  EVENT_RANGING_MEAS_RECV,
  EVENT_RANGING_ERROR,
  EVENT_P2P_STATUS_UPDATE,
  EVENT_CLD_ERROR_MESSAGE,
  EVENT_INVALID_NL_MESSAGE,
  /* Internal FSM Events*/
  EVENT_REGISTRATION_FAILURE_OR_LOST,
  EVENT_RANGING_RESPONSE_TO_USER,
  EVENT_CONFIG_RESPONSE_TO_USER,
  EVENT_NOT_READY,
  /* Events from Timer */
  EVENT_TIMEOUT,
  EVENT_MAX
} RangingFSM_Event;

typedef enum
{
  STATE_NOT_REGISTERED = 0,
  STATE_WAITING_FOR_CHANNEL_INFO,
  STATE_WAITING_FOR_RANGING_CAP,
  STATE_READY_AND_IDLE,
  STATE_PROCESSING_RANGING_REQ,
  STATE_PROCESSING_CONFIG_REQ,
  STATE_MAX
} RangingFSM_State;

/** The following struct holds the current
 *  State of the FSM */
typedef struct
{
  /* The current Event being processed by the FSM */
  RangingFSM_Event curEvent;
  /* The current State of the FSM */
  RangingFSM_State curState;
  /* The NL Socket timeout */
  uint64 timeEnd;
  /* This flag is used by the FSM to indicate to the
     LOWI Rome Wi-Fi Driver that a new valid result is waiting */
  bool valid_result;
  /* This flag is used by the FSM to indicate to the
     LOWI Rome Wi-Fi Driver that a terminate request arrived */
  bool terminate_thread;
  /* This flag is used to indicate if the FSM is in the process
     of trying to register with the Wi-Fi Driver*/
  bool notTryingToRegister;
  /* Flag to indicate that there is a pending
   * Internal FSM event to be processed.
   */
  bool internalFsmEventPending;
} RangingFSM_ContextInfo;

typedef struct
{
 /** true if single-sided ranging is supported */
  bool oneSidedSupported;

  /** true if dual-sided ranging per 11v std is supported */
  bool dualSidedSupported11v;

  /** true if dual-sided ranging per 11mc std is supported */
  bool dualSidedSupported11mc;

  /** Highest bandwidth support for rtt requests */
  uint8 bwSupport;

  /** Bit mask representing preambles supported for rtt requests */
  uint8 preambleSupport;

} LOWI_RangingCapabilities;

typedef enum
{
  ROME_NO_RSP_YET = 0,
  ROME_CONFIG_SUCCESS,
  ROME_CONFIG_FAIL
} RomeConfigStatus;
/** The following struct conatains information on request
 *  that FW is currently processing, it is used by the FSM to
 *  decide whether to send the next request to FW or wait for
 *  more responses from FW */
typedef struct
{
  /** The report Type requested from FW */
  unsigned int reportType;
  /** The total number of BSSIDs in teh last request send to FW */
  unsigned int bssidCount;
  /** The toal number of measurements requested per BSSID */
  unsigned int measPerTarget;
  /** Expected number response messages from FW for this
   *  request */
  unsigned int expectedRspFromFw;
  /** Total number of responses so far received from FW for
   *  this request */
  unsigned int totalRspFromFw;
  /** Flag to indicate to the FSM that the last expected
   *  response has arrived from FW */
  bool         lastResponseRecv;
  /** Does the current Req contain an ASAP 0 target */
  bool nonAsapTargetPresent;
  /** Configuration Success/Failed */
  RomeConfigStatus configStatus;
} RomeReqRspInfo;

typedef struct
{
  ChannelInfo chanInfo;
  LOWINodeInfo targetNode;
  uint32 tsfDelta;
  bool tsfValid;
} LOWIRangingNode;

/** The following  structure is used to store the information
 *  from the latest LOWI request plus additional information
 *  request related for the FSM */
typedef struct _RomeRangingRequest
{
  /** vector containing the LOWI Nodes and their associated
   *  Channel structures */
  vector <LOWIRangingNode> vecRangingNodes;
  /** 2D vector containing the LOWI Nodes grouped according to
   *  their Channel structures */
  vector <LOWIRangingNode> arrayVecRangingNodes[MAX_DIFFERENT_CHANNELS_ALLOWED];
  /** vector LOWI Node Arrays, each member of the array
   *  contains a list of BSSIDs for a particulat channel */
  vector <LOWINodeInfo> vecChGroup[MAX_DIFFERENT_CHANNELS_ALLOWED];
  /** The list of channels corresponding to each member in the
   *  above array */
  unsigned int chList[MAX_DIFFERENT_CHANNELS_ALLOWED];
  /** Flag to indicate that this ranging request is still valid
   *  and has not been fully processed by the FSM */
  bool validRangingScan;
  /** The current Channel index indicating the member in the
   *  vecChGroup array that is being serviced by the FSM */
  unsigned int curChIndex;
  /** total number of channels the current request contains */
  unsigned int totChs;
  /** This is an index variable pointing to the current AP in
   *  vecChGroup */
  unsigned int curAp;
  /** Total APs in the current vecChGroup Member */
  unsigned int totAp;

  /** Constructor
    */
  _RomeRangingRequest();
} RomeRangingRequest;

class LOWIRomeFSM
{
private:
  static const char* const TAG;
  tANI_U8                                  mRangingFsmData[2048];
  LOWIRequest*                             mNewReq;
  LOWIRangingScanRequest*                  mNewRangingReq;
  LOWISetLCILocationInformation*           mNewLCIConfigReq;
  LOWISetLCRLocationInformation*           mNewLCRConfigReq;
  LOWISendLCIRequest*                      mNewLCIReq;
  LOWIFTMRangingRequest*                   mNewFTMRangingReq;
  LOWIRequest*                             mCurReq;
  RomeRangingRequest                       mRomeRangingReq;
  RomeReqRspInfo                           mRomeReqRspInfo;
  RomeRttCapabilities                      mRomeRttCapabilities;
  uint16                                   mRtsCtsTag;

  /* The following variable is used to indicate the type of event
     that arrived from LOWI controller */
  RomePipeEvents                           mRomePipeEvents;

  /* The following variable is used to store the pointer to the
     LOWI result object where results for the current request will be
     stored */
  LOWIMeasurementResult*                   mLowiMeasResult;

  /* The following variable is used to indicate that LOWI is running
     CFR capture mode */
  bool                                     mCfrCaptureMode;

  /* The listener through which FSM will report Ranging results */
  LOWIScanResultReceiverListener*          mListener;

  /** The Cache Manager object through which the FSM can access
   *  the BSSID cache.
   */
  LOWICacheManager*                        mCacheManager;

  /** The following array stores the channel info for all
    *  supported channels for the 20MHz Bandwidth */
  static ChannelInfo                       mChannelInfoArray[MAX_CHANNEL_ID];

  /*=============================================================================================
   * Function description:
   *   This function determines if the combination of Channel, BW & Preamble make sense.
   *   This check is done to prevent FW from getting an invalid combo that can potentially
   *   cause a crash.
   *
   * Parameters:
   *   LOWINodeInfo: The node to validate.
   *
   * Return value:
   *    false - NOT Valid & true - Valid
   *
   =============================================================================================*/
  bool validChannelBWPreambleCombo(LOWINodeInfo node);

  /*=============================================================================================
   * Function description:
   *   This function sets the Phy Mode in the channel structure for the provided node based
   *   on the provided channel, BW and Preamble information.
   *
   * Parameters:
   *   LOWIRangingNode: The ranging node
   *
   * Return value:
   *    NONE
   *
   =============================================================================================*/
  void setPhyMode(LOWIRangingNode &rangingNode);

  /*=============================================================================================
   * Function description:
   *   This function takes the LOWI ranging Request and performs the following operations:
   *   1) Sets up the FSM to start performing Ranging Scans
   *   2) Groups APs/Channel
   *
   * Parameters:
   *   None
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  int PrepareRangingRequest();

  /*=============================================================================================
   * Function description:
   *   This function adds the current node in the Scan Results with the associated error code:
   *
   * Parameters:
   *   node: The target that needs to be added to the scan result
   *   errorCode: the error code for this target
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  int AddTargetToResult(LOWINodeInfo node, LOWIScanMeasurement::eTargetStatus errorCode);

  /*=============================================================================================
   * Function description:
   *   This function validates the incoming LOWI ranging Request from LOWI controller.
   *
   * Parameters:
   *   None
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  int ValidateRangingRequest();

  /*=============================================================================================
   * Function description:
   *   This is a helper function that takesa a vector of APs and generates an array of vectors
   *   with each vector containg Aps on the same channel.
   *
   * Parameters:
   *   origVec: Original vector with all the APs
   *   vec    : Array of vectors with APs in each vector grouped by channel
   *   maxChanels: Max different channels in the original Vector.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  unsigned int groupByChannel(vector <LOWIRangingNode> origVec,
                              vector <LOWIRangingNode> *vec,
                              unsigned int maxChannels);

  /*=============================================================================================
   * Function description:
   *   This function decides whether to send another ranging request to FW or if all the Targets
   *   from the LOWI request have been serviced, it responds to the user with the results.
   *
   * Parameters:
   *   None
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  int SendReqToFwOrRespToUser();

  /*=============================================================================================
   * Function description:
   *   This function takes the Vectors of APs grouped by channel and sends the Ranging request
   *   to the Rome driver layer within LOWI.
   *
   * Parameters:
   *   None
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  int SendRangingReq();

  /*=============================================================================================
   * Function description:
   *   This function Initializes the Measurement result object when a new ranging request
   *   arrives
   *
   * Parameters:
   *   @Param [in] LOWIMeasurementResult*: Handle to the Measurement result to initialize
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  int InitializeMeasResult(LOWIMeasurementResult* lowiMeasResult, LOWIResponse::eScanStatus status = LOWIResponse::SCAN_STATUS_DRIVER_ERROR);

  /*=============================================================================================
   * Function description:
   *   This function is called when New Ranging Measurements arrive from Rome FW. This is where
   *   they are parsed and put into the measurement result object for LOWI.
   *
   * Parameters:
   *   None
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  int ProcessRangingMeas();

  /*=============================================================================================
   * Function description:
   *   This is the state action function called when the State is "Not Registered" and
   *   a start thread event arrives.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object on which the state function should
   *                             work on / make changes.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int SendRegRequest(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function called when A registeration response arrives from the WLAN Host
   *   driver.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandleRegSuccess(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function called when the channel info response arrives from the WLAN Host
   *   driver.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandleChannelInfo(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function called when the ranging capability response arrives
   *   from the WLAN Host driver.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandleRangingCap(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function called a PIPE event ocurs, this usually happens when LOWI
   *   controller has an event waiting for the FSM.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandlePipeEvent(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function called when a Ranging request arrives from LOWI controller
   *   driver.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandleRangingReq(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function called when a Ranging request arrives from LOWI controller
   *   driver.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandleConfigReq(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function called when new Ranging measurements arrive from the Rome FW.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandleRangingMeas(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function is called when a Ranging Error message arrives
   *   from the Rome FW.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandleRangingErrorMsg(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function is called when a Ranging Error message arrives
   *   from the Rome FW and the FSM has just sent an LCI/LCR configuration request
   *   to FW.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandleConfigRspOrErrorMsg(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function called when a P2P Peer info message arrives from the WLAN Host
   *   driver.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandleP2PInfo(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function called when an error message arrives from
   *   the WLAN Host driver.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandleCldErrorMsg(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function called when a registration failure message arrives from
   *   the WLAN Host driver.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandleRegFailureOrLost(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
  * Function description:
  *   This state action function called when a ranging request arrives and the FSM is state
  *   STATE_NOT_REGISTERED.
  *
  * Parameters:
  *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
  *                             work on / make changes to.
  *
  * Return value:
  *    error code: 0 - Success & -1 - Failure
  *
  =============================================================================================*/
  static int HandleRangingReqWhenNotReg(LOWIRomeFSM* pFsmObj);

   /*=============================================================================================
   * Function description:
   *   This state action function called when a ranging request arrives and the FSM is not in
   *   state to handle or accept new requests.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int IgnoreRangingReq(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function is called a timeout occures while waiting on the NL Socket
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int HandleNlTimeout(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This state action function called when an event that has no effect on the current state
   *   arrives.
   *
   * Parameters:
   *   @param [in] LOWIRomeFSM*: Pointer to the FSM object which the state function should
   *                             work on / make changes to.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  static int DoNothing(LOWIRomeFSM* pFsmObj);

  /*=============================================================================================
   * Function description:
   *   This function sets the an FSM flag to indicate that there is an internal FSM event
   *   pending.
   *
   * Parameters:
   *   None
   *
   * Return value:
   *   None
   *
   =============================================================================================*/
  void setFsmInternalEvent();

  /*=============================================================================================
   * Function description:
   *   This function clears the FSM flag that indicates that there is an internal FSM event
   *   pending.
   *
   * Parameters:
   *   None
   *
   * Return value:
   *   None
   *
   =============================================================================================*/
  void clearFsmInternalEvent();

  /*=============================================================================================
   * Function description:
   *   This function is called to setup the FSM's state function table
   *
   * Parameters:
   *   None
   *
   * Return value:
   *   None
   *
   =============================================================================================*/
  void SetupFsm();

  /*=============================================================================================
   * Function description:
   *   This function logs the newly arrived Ranging Request to Diag Log
   *
   * Parameters:
   *   vector <LOWINodeInfo> - a Vector containing the Targets for this Ranging Request.
   *
   * Return value:
   *   None
   *
   =============================================================================================*/
  void LogRangingRequest(vector <LOWINodeInfo> &v);

  /*=============================================================================================
   * Function description:
   *   Calculates the number of bytes that will be needed by FW to provide the Measurements,
   *   LCI & LCR information for a target. This will be used to make sure that LOWI to ensure
   *   that it limits the number of targets per request so that the MAX WMI message size is not
   *   exceeded.
   *
   * Parameters:
   *   numMeas: Num Measurements for this target.
   *   ftmRangingParams: Fine timing measurement ranging parameters.
   *
   * Return value:
   *   The bytes needed for this target.
   *
   =============================================================================================*/
  unsigned int ExpectedMeasSizeForTarget(unsigned int numMeas, uint32 ftmRangingParams);

  /*=============================================================================================
   * Function description:
   *   Add Dummy Measurements for previous RTT Measurement Request
   *   This should be called when the FW does not send a response
   *
   * Parameters:
   *   None
   *
   * Return value:
   *   None
   *
   =============================================================================================*/
  void AddDummyMeas();

  typedef int (*stateFunction) (LOWIRomeFSM*);

  stateFunction stateTable[STATE_MAX][EVENT_MAX];

public:
  RangingFSM_ContextInfo mRangingFsmContext;

  /**
   * Passes on the Request sent from LOWI Controller to LOWI
   * Ranging FSM module
   *
   * @param  LOWIRequest* mReq - The Ranging Request
   * @return 0 - success, -1 failure
   */
  int SetLOWIRequest(LOWIRequest* pReq);

  /*=============================================================================================
   * Function description:
   *   This function checks to see if there is a new Ranging request pending.
   *
   * Parameters:
   *   None
   *
   * Return value: true or false
   *
   =============================================================================================*/
  bool NewRequestPending();

  /*=============================================================================================
   * Function description:
   *   This function checks to see if FSM is currently processing a request.
   *
   * Parameters:
   *   None
   *
   * Return value: true or false
   *
   =============================================================================================*/
  bool ProcessingRangingRequest();

  /*=============================================================================================
   * Function description:
   *   This function rejects the new request sent by the caller.
   *
   * Parameters:
   *   NONE
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  int RejectNewRangingReq();

  /*=============================================================================================
   * Function description:
   *   This function sends the current Ranging result to user with the status provided by
   *   the caller.
   *
   * Parameters:
   *   eScanStatus: The Status of the Ranging scan Results.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  int SendCurrentResultToUser(LOWIResponse::eScanStatus status);

  /*=============================================================================================
   * Function description:
   *   This Main FSM function called by the any object that has an instance of the FSM class.
   *   This function when called inturn calls the appropriate action function from the FSM
   *   action function table based in the current FSM state and the current Event.
   *
   * Parameters:
   *   None
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  int FSM();

  /*=============================================================================================
   * Function description:
   *   This function is called by the LOWI rome wifi Driver object to indicate to the FSM
   *   that a new PIPE event has arrived from LOWI controller.
   *
   * Parameters:
   *   Pipe Event
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  int SetNewPipeEvent(RomePipeEvents newEvent);

  /*=============================================================================================
   * Function description:
   *   This function is called to send results to the user via the Listenener Object.
   *
   * Parameters:
   *   LOWIMeasurementResult*: Measurement results.
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  int SendResultToClient(LOWIMeasurementResult* result);

  /*=============================================================================================
   * Function description:
   *   This function is called by the LOWI rome wifi Driver object to check if the FSM is ready
   *   for accepting ranging requests.
   *
   * Parameters:
   *   None
   *
   * Return value:
   *   FSM state: 0 - Not ready & 1 - Ready & Idle
   *
   =============================================================================================*/
  bool IsReady();

  /*=============================================================================================
   * Function description:
   *   This function is called by the LOWI rome wifi Driver object to get the Ranging
   *   capabilities from the FSM.
   *
   * Parameters:
   *   None
   *
   * Return value:
   *    error code: 0 - Success & -1 - Failure
   *
   =============================================================================================*/
  LOWI_RangingCapabilities GetRangingCap();

  /*=============================================================================================
   * Function description:
   *   Waits on the Private Netlink Socket and the LOWI controller pipe for timeout if timeout >0
   *   If Messages/Data arrive, collects and processes them accordingly.
   *   If LOWI controller requests a shutdown then exits and picks up the new pequest to process
   *   If Timeout occurs exits without processing any data.
   *
   * Parameters:
   *   None
   *
   * Return value:
   *    error code
   *
   =============================================================================================*/
  int ListenForEvents();

  /*=============================================================================================
   * Function description:
   *   Map LOWI Request to internal event.
   *
   * Parameters:
   *   LOWIRequest* mReq - The Request
   *
   * Return value:
   *   0 if request is valid., -1 otherwise.
   *
   =============================================================================================*/
  int LowiReqToEvent(const LOWIRequest * const pReq);

  LOWIRomeFSM(int mCfrCaptureModeState,
              LOWIScanResultReceiverListener* scanResultListener,
              LOWICacheManager* cacheManager);
  ~LOWIRomeFSM();
};

} // namepsace

#endif //#ifndef __LOWI_ROME_FSM_H__
