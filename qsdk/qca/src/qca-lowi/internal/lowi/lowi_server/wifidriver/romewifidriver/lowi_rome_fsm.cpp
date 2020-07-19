/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI ROME Finite State Machine Header file

GENERAL DESCRIPTION
  This file contains the functions and global data definitions used by the
  LOWI Rome Finite State Machine

Copyright (c) 2014-2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================*/

#include <base_util/log.h>
#include <base_util/time_routines.h>
#include <common/lowi_utils.h>
#include "lowi_rome_fsm.h"
#include "lowi_wifidriver_utils.h"
#include "lowi_p2p_ranging.h"

// Maximum RTT measurements per target for Aggregate report type
#define MAX_RTT_MEAS_PER_DEST 25

// Maximum Scan result length
#define MAX_SCAN_RES_LEN 65534

// Responses from FW for Report Type 2
#define RESP_REPORT_TYPE2 1

// Number of retries the FSM will perform for failure cases
#define ROME_FSM_RETRY_COUNT 3

// Timeout value which will make the FSM block forever
#define ROME_FSM_TIMEOUT_FOREVER 0

/* Macros to check for valid channel spacing */
#define IS_VALID_20MHZ_CHAN_SPACING(prime,second) \
        (second == prime || \
         second == 0)

#define IS_VALID_40MHZ_CHAN_SPACING(prime,second)     \
        (second == (prime + CHANNEL_SPACING_10MHZ) || \
         second == (prime - CHANNEL_SPACING_10MHZ))

#define IS_VALID_80MHZ_CHAN_SPACING(prime,second)     \
        (second == (prime + CHANNEL_SPACING_30MHZ) || \
         second == (prime - CHANNEL_SPACING_30MHZ) || \
         second == (prime + CHANNEL_SPACING_10MHZ) || \
         second == (prime - CHANNEL_SPACING_10MHZ))

using namespace qc_loc_fw;

const char * const LOWIRomeFSM::TAG = "LOWIRomeFSM";

const char* sRangingFSMEvents [EVENT_MAX + 1] =
{
  /* Events from LOWI Controller */
  "EVENT_START_THREAD",
  "EVENT_RANGING_REQ",
  "EVENT_CONFIGURATION_REQ",
  "EVENT_INVALID_REQ",
  "EVENT_TERMINATE_REQ",
  /* Events from NL Socket */
  "EVENT_REGISTRATION_SUCCESS",
  "EVENT_RANGING_CAP_INFO",
  "EVENT_CHANNEL_INFO",
  "EVENT_RANGING_MEAS_RECV",
  "EVENT_RANGING_ERROR",
  "EVENT_P2P_STATUS_UPDATE",
  "EVENT_CLD_ERROR_MESSAGE",
  "EVENT_INVALID_NL_MESSAGE",
  /* Internal FSM Events*/
  "EVENT_REGISTRATION_FAILURE_OR_LOST",
  "EVENT_RANGING_RESPONSE_TO_USER",
  "EVENT_CONFIG_RESPONSE_TO_USER",
  "EVENT_NOT_READY",
  /* Events from Timer */
  "EVENT_TIMEOUT",
  "EVENT_MAX"
};
const char* sRangingFsmStates[STATE_MAX +1] =
{
  "STATE_NOT_REGISTERED",
  "STATE_WAITING_FOR_CHANNEL_INFO",
  "STATE_WAITING_FOR_RANGING_CAP",
  "STATE_READY_AND_IDLE",
  "STATE_PROCESSING_RANGING_REQ",
  "STATE_PROCESSING_CONFIG_REQ",
  "STATE_MAX"
};

const char *sRangingErrorCodes[WMI_RTT_REJECT_MAX + 1] =
{
  "RTT_COMMAND_HEADER_ERROR",
  "RTT_COMMAND_ERROR",
  "RTT_MODULE_BUSY",
  "RTT_TOO_MANY_STA",
  "RTT_NO_RESOURCE",
  "RTT_VDEV_ERROR",
  "RTT_TRANSIMISSION_ERROR",
  "RTT_TM_TIMER_EXPIRE",
  "RTT_FRAME_TYPE_NOSUPPORT",
  "RTT_TIMER_EXPIRE",
  "RTT_CHAN_SWITCH_ERROR",
  "RTT_TMR_TRANS_ERROR",
  "RTT_NO_REPORT_BAD_CFR_TOKEN",
  "RTT_NO_REPORT_FIRST_TM_BAD_CFR",
  "RTT_REPORT_TYPE2_MIX",
  "RTT_LCI_CFG_OK",
  "RTT_LCR_CFG_OK",
  "RTT_CFG_ERROR",
  "WMI_RTT_REJECT_MAX",
  "RTT_LCI_REQ_OK",
  "RTT_FTMRR_OK"
};

ROME_PHY_MODE validPhyModeTable[LOWIDiscoveryScanRequest::BAND_ALL][RTT_PREAMBLE_MAX][BW_MAX] =
{
  /* 2G BAND */
  {
                               /*    BW_20MHZ                 BW_40MHZ                 BW_80MHZ               BW_160MHZ      */
    /* RTT_PREAMBLE_LEGACY */  {ROME_PHY_MODE_11G      , ROME_PHY_MODE_UNKNOWN,   ROME_PHY_MODE_UNKNOWN, ROME_PHY_MODE_UNKNOWN},
    /* RTT_PREAMBLE_HT     */  {ROME_PHY_MODE_11NG_HT20, ROME_PHY_MODE_11NG_HT40, ROME_PHY_MODE_UNKNOWN, ROME_PHY_MODE_UNKNOWN},
    /* RTT_PREAMBLE_VHT    */  {ROME_PHY_MODE_UNKNOWN  , ROME_PHY_MODE_UNKNOWN,   ROME_PHY_MODE_UNKNOWN, ROME_PHY_MODE_UNKNOWN}
  },
  /* 5G BAND */
  {
                               /*    BW_20MHZ                  BW_40MHZ                  BW_80MHZ                  BW_160MHZ      */
    /* RTT_PREAMBLE_LEGACY */  {ROME_PHY_MODE_11A,        ROME_PHY_MODE_UNKNOWN,    ROME_PHY_MODE_UNKNOWN,    ROME_PHY_MODE_UNKNOWN},
    /* RTT_PREAMBLE_HT     */  {ROME_PHY_MODE_11NA_HT20,  ROME_PHY_MODE_11NA_HT40,  ROME_PHY_MODE_UNKNOWN,    ROME_PHY_MODE_UNKNOWN},
    /* RTT_PREAMBLE_VHT    */  {ROME_PHY_MODE_11AC_VHT20, ROME_PHY_MODE_11AC_VHT40, ROME_PHY_MODE_11AC_VHT80, ROME_PHY_MODE_UNKNOWN}
  }
};

RangingFSM_Event RomeCLDToRomeFSMEventMap[ROME_MSG_MAX] =
{
  /** ROME CLD Messages */
  /* ROME_REG_RSP_MSG */          EVENT_REGISTRATION_SUCCESS,
  /* ROME_CHANNEL_INFO_MSG */     EVENT_CHANNEL_INFO,
  /* ROME_P2P_PEER_EVENT_MSG */   EVENT_P2P_STATUS_UPDATE,
  /* ROME_CLD_ERROR_MSG */        EVENT_CLD_ERROR_MESSAGE,
  /** ROME FW Messages*/
  /* ROME_RANGING_CAP_MSG */      EVENT_RANGING_CAP_INFO,
  /* ROME_RANGING_MEAS_MSG */     EVENT_RANGING_MEAS_RECV,
  /* ROME_RANGING_ERROR_MSG */    EVENT_RANGING_ERROR,
  /** NL/Kernel Messages */
  /* ROME_NL_ERROR_MSG */         EVENT_INVALID_NL_MESSAGE
};

/** The following array stores the channel info for all
 *  supported channels for the 20MHz Bandwidth */
ChannelInfo LOWIRomeFSM::mChannelInfoArray[MAX_CHANNEL_ID] = {0, 0, 0, 0, 0, 0, 0};

_RomeRangingRequest::_RomeRangingRequest()
{
  validRangingScan = FALSE;
  curChIndex       = 0;
  totChs           = 0;
  curAp            = 0;
  totAp            = 0;
}

LOWIRomeFSM::LOWIRomeFSM(int cfrCaptureModeState,
                         LOWIScanResultReceiverListener* scanResultListener,
                         LOWICacheManager* cacheManager)
{
  mListener = scanResultListener;
  mCacheManager = cacheManager;
  mRangingFsmContext.curEvent = EVENT_START_THREAD;
  mRangingFsmContext.curState = STATE_NOT_REGISTERED;
  mRangingFsmContext.timeEnd = ROME_FSM_TIMEOUT_FOREVER;
  mRangingFsmContext.terminate_thread = FALSE;
  mRangingFsmContext.notTryingToRegister = TRUE;
  mNewReq = NULL;
  mNewRangingReq = NULL;
  mNewLCIConfigReq = NULL;
  mNewLCRConfigReq = NULL;
  mNewLCIReq = NULL;
  mNewFTMRangingReq = NULL;
  mRomePipeEvents = ROME_MAX_PIPE_EVENTS;
  mLowiMeasResult = NULL;
  mRtsCtsTag = 1;
  if (cfrCaptureModeState > 0)
  {
    log_debug (TAG, "LOWI Rome Wifi Driver running in CFR capture mode");
    mCfrCaptureMode = true;
  }
  else
  {
    log_verbose (TAG, "LOWI not running in CFR capture mode");
    mCfrCaptureMode = false;
  }
  memset(&mRomeReqRspInfo, 0, sizeof(RomeReqRspInfo));
  /* Ready for new request */
  mRomeReqRspInfo.lastResponseRecv = TRUE;
  mRomeReqRspInfo.nonAsapTargetPresent = FALSE;
  mRomeRttCapabilities.rangingTypeMask = 0;
  SetupFsm();
};



void LOWIRomeFSM::SetupFsm()
{

  /** Initialize all action functions to "DoNothing". */

  for (unsigned int state = STATE_NOT_REGISTERED; state < STATE_MAX; state++)
  {
    for (unsigned int fsmEvent = EVENT_START_THREAD; fsmEvent < EVENT_MAX; fsmEvent++)
    {
      stateTable[state][fsmEvent] = DoNothing;
    }
  }

  /***************** State Functions for State: STATE_NOT_REGISTERED *************/
  /** -- Events from LOWI Controller -- */
             /** Current State */    /** Trigger Event */                     /** Action Function */
  stateTable[STATE_NOT_REGISTERED]   [EVENT_START_THREAD]                         = SendRegRequest;
  stateTable[STATE_NOT_REGISTERED]   [EVENT_RANGING_REQ]                          = HandleRangingReqWhenNotReg;
  /** -- Events from NL Socket -- */
  stateTable[STATE_NOT_REGISTERED]   [EVENT_REGISTRATION_SUCCESS]                 = HandleRegSuccess;
  /** -- Events from Timer -- */
  stateTable[STATE_NOT_REGISTERED]   [EVENT_TIMEOUT]                              = HandleRegFailureOrLost;

  /***************** State Functions for State: STATE_WAITING_FOR_CHANNEL_INFO *************/
  /** -- Events from LOWI Controller -- */
             /** Current State */              /** Trigger Event */                /** Action Function */
  stateTable[STATE_WAITING_FOR_CHANNEL_INFO]   [EVENT_RANGING_REQ]                   = HandleRangingReqWhenNotReg;
  /** -- Events from NL Socket -- */
  stateTable[STATE_WAITING_FOR_CHANNEL_INFO]   [EVENT_CHANNEL_INFO]                  = HandleChannelInfo;
  /** -- Internal FSM Events  --  */
  stateTable[STATE_WAITING_FOR_CHANNEL_INFO]   [EVENT_REGISTRATION_FAILURE_OR_LOST]  = HandleRegFailureOrLost;
  /** -- Events from Timer -- */
  stateTable[STATE_WAITING_FOR_CHANNEL_INFO]   [EVENT_TIMEOUT]                       = HandleRegFailureOrLost;

  /***************** State Functions for State: STATE_WAITING_FOR_RANGING_CAP *************/
  /** -- Events from LOWI Controller -- */
             /** Current State */             /** Trigger Event */                /** Action Function */
  stateTable[STATE_WAITING_FOR_RANGING_CAP]   [EVENT_RANGING_REQ]                   = HandleRangingReqWhenNotReg;
  /** -- Events from NL Socket -- */
  stateTable[STATE_WAITING_FOR_RANGING_CAP]   [EVENT_RANGING_CAP_INFO]              = HandleRangingCap;
  /** -- Internal FSM Events  --  */
  stateTable[STATE_WAITING_FOR_RANGING_CAP]   [EVENT_REGISTRATION_FAILURE_OR_LOST]  = HandleRegFailureOrLost;
  /** -- Events from Timer -- */
  stateTable[STATE_WAITING_FOR_RANGING_CAP]   [EVENT_TIMEOUT]                       = HandleRegFailureOrLost;

  /***************** State Functions for State: STATE_READY_AND_IDLE *************/
  /** -- Events from LOWI Controller -- */
             /** Current State */    /** Trigger Event */                /** Action Function */
  stateTable[STATE_READY_AND_IDLE]   [EVENT_RANGING_REQ]                   = HandleRangingReq;
  stateTable[STATE_READY_AND_IDLE]   [EVENT_CONFIGURATION_REQ]             = HandleConfigReq;
  stateTable[STATE_READY_AND_IDLE]   [EVENT_INVALID_REQ]                   = IgnoreRangingReq;
  /** -- Events from NL Socket -- */
  stateTable[STATE_READY_AND_IDLE]   [EVENT_P2P_STATUS_UPDATE]             = HandleP2PInfo;
  /** -- Internal FSM Events  --  */
  stateTable[STATE_READY_AND_IDLE]   [EVENT_REGISTRATION_FAILURE_OR_LOST]  = HandleRegFailureOrLost;

  /***************** State Functions for State: STATE_PROCESSING_RANGING_REQ *************/
  /** -- Events from LOWI Controller -- */
             /** Current State */           /** Trigger Event */                 /** Action Function */
  stateTable[STATE_PROCESSING_RANGING_REQ]   [EVENT_RANGING_REQ]                   = IgnoreRangingReq;
  /** -- Events from NL Socket -- */
  stateTable[STATE_PROCESSING_RANGING_REQ]   [EVENT_RANGING_ERROR]                 = HandleRangingErrorMsg;
  stateTable[STATE_PROCESSING_RANGING_REQ]   [EVENT_RANGING_MEAS_RECV]             = HandleRangingMeas;
  stateTable[STATE_PROCESSING_RANGING_REQ]   [EVENT_P2P_STATUS_UPDATE]             = HandleP2PInfo;
  stateTable[STATE_PROCESSING_RANGING_REQ]   [EVENT_CLD_ERROR_MESSAGE]             = HandleCldErrorMsg;
  /** -- Internal FSM Events  --  */
  stateTable[STATE_PROCESSING_RANGING_REQ]   [EVENT_REGISTRATION_FAILURE_OR_LOST]  = HandleRegFailureOrLost;
  /** -- Events from Timer -- */
             /** Current State */           /** Trigger Event */                 /** Action Function */
  stateTable[STATE_PROCESSING_RANGING_REQ]   [EVENT_TIMEOUT]                       = HandleNlTimeout;

  /***************** State Functions for State: STATE_PROCESSING_CONFIG_REQ *************/
  /** -- Events from LOWI Controller -- */
             /** Current State */           /** Trigger Event */                 /** Action Function */
  stateTable[STATE_PROCESSING_CONFIG_REQ]   [EVENT_RANGING_REQ]                   = IgnoreRangingReq;
  /** -- Events from NL Socket -- */
  stateTable[STATE_PROCESSING_CONFIG_REQ]   [EVENT_RANGING_ERROR]                 = HandleConfigRspOrErrorMsg;
  stateTable[STATE_PROCESSING_CONFIG_REQ]   [EVENT_P2P_STATUS_UPDATE]             = HandleP2PInfo;
  stateTable[STATE_PROCESSING_CONFIG_REQ]   [EVENT_CLD_ERROR_MESSAGE]             = HandleCldErrorMsg;
  /** -- Internal FSM Events  --  */
  stateTable[STATE_PROCESSING_CONFIG_REQ]   [EVENT_REGISTRATION_FAILURE_OR_LOST]  = HandleRegFailureOrLost;
  /** -- Events from Timer -- */
             /** Current State */           /** Trigger Event */                 /** Action Function */
  stateTable[STATE_PROCESSING_CONFIG_REQ]   [EVENT_TIMEOUT]                       = HandleNlTimeout;

}

int LOWIRomeFSM::HandleRegFailureOrLost(LOWIRomeFSM* pFsmObj)
{
  int retVal = 0;
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }

  if (pFsmObj->ProcessingRangingRequest())
  {
    /* Stop Processing Request and return Result to User */
    pFsmObj->SendCurrentResultToUser(LOWIResponse::SCAN_STATUS_DRIVER_ERROR);
  }
  if (pFsmObj->NewRequestPending())
  {
    /* Respond to User with Failure */
    IgnoreRangingReq(pFsmObj);
  }

  /* Reset to STATE_NOT_REGISTERED and wait indefinitely */
  log_warning(TAG, "HandleRegFailureOrLost(): Registration Failed or lost registration with Host Driver");
  pFsmObj->mRangingFsmContext.curState = STATE_NOT_REGISTERED;
  pFsmObj->mRangingFsmContext.timeEnd = ROME_FSM_TIMEOUT_FOREVER;
  pFsmObj->mRangingFsmContext.notTryingToRegister = TRUE;

  return retVal;
}

int LOWIRomeFSM::HandleCldErrorMsg(LOWIRomeFSM* pFsmObj)
{
  int retVal = 0;
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }

  tAniMsgHdr * pMsgHdr = (tAniMsgHdr *)pFsmObj->mRangingFsmData;
  if ((ANI_MSG_OEM_ERROR != pMsgHdr->type) ||
      (0 == pMsgHdr->length))
  {
    log_warning(TAG, "%s: Invalid ANI Msg Type %u, len = %d", __FUNCTION__,
                pMsgHdr->type, pMsgHdr->length);
    return -1;
  }
  eOemErrorCode errCode = *((eOemErrorCode *)(pFsmObj->mRangingFsmData +
                                              sizeof(tAniMsgHdr)));
  log_debug(TAG, "%s: CLD Error Msg Type %u, len = %d, code %d", __FUNCTION__,
            pMsgHdr->type, pMsgHdr->length, errCode);
  switch (errCode)
  {
    case OEM_ERR_APP_NOT_REGISTERED:   /* OEM App is not registered */
      /* Reset to STATE_NOT_REGISTERED */
      pFsmObj->mRangingFsmContext.curState = STATE_NOT_REGISTERED;
      pFsmObj->mRangingFsmContext.timeEnd = ROME_FSM_TIMEOUT_FOREVER;
      pFsmObj->mRangingFsmContext.notTryingToRegister = TRUE;

      //Copy the appropriate request to New Req
      if (pFsmObj->mNewRangingReq)
      {
        pFsmObj->mNewReq = pFsmObj->mNewRangingReq;
        pFsmObj->mNewRangingReq = NULL;
        // Clean up the state for Previous Ranging Request.
        pFsmObj->mRomeReqRspInfo.lastResponseRecv = TRUE;
        pFsmObj->mRomeReqRspInfo.expectedRspFromFw = 0;
      }
      else if (pFsmObj->mCurReq)
      {
        pFsmObj->mNewReq = pFsmObj->mCurReq;
        pFsmObj->mCurReq = NULL;
      }
      // Clean up the stored measurement result.
      // Make sure the measurement result vector is empty
      if (pFsmObj->mLowiMeasResult)
      {
        for (vector <LOWIScanMeasurement*>::Iterator it = pFsmObj->mLowiMeasResult->scanMeasurements.begin();
             it != pFsmObj->mLowiMeasResult->scanMeasurements.end(); it++)
        {
          delete (*it);
        }
        pFsmObj->mLowiMeasResult->scanMeasurements.flush();
        delete pFsmObj->mLowiMeasResult;
        pFsmObj->mLowiMeasResult = NULL;
      }
      //start Registration
      SendRegRequest(pFsmObj);
      break;


    case OEM_ERR_NULL_CONTEXT:          /* Error null context */
    case OEM_ERR_INVALID_SIGNATURE:     /* Invalid signature */
    case OEM_ERR_NULL_MESSAGE_HEADER:   /* Invalid message type */
    case OEM_ERR_INVALID_MESSAGE_TYPE:  /* Invalid message type */
    case OEM_ERR_INVALID_MESSAGE_LENGTH:/* Invalid length in message body */
    default:
      log_warning(TAG, "%s:Unexpected error code %d", __FUNCTION__, errCode);
      break;
  }

  return retVal;
}

int LOWIRomeFSM::DoNothing(LOWIRomeFSM* pFsmObj)
{
  int retVal = 0;
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }
  log_debug(TAG, "DoNothing(): FSM Recieved Event: %s while in State: %s",
              sRangingFSMEvents[pFsmObj->mRangingFsmContext.curEvent],
              sRangingFsmStates[pFsmObj->mRangingFsmContext.curState]);
  return retVal;
}

void LOWIRomeFSM::setFsmInternalEvent()
{
  mRangingFsmContext.internalFsmEventPending = true;
}

void LOWIRomeFSM::clearFsmInternalEvent()
{
  mRangingFsmContext.internalFsmEventPending = false;
}

int LOWIRomeFSM::HandleRangingReqWhenNotReg(LOWIRomeFSM* pFsmObj)
{
  log_verbose(TAG, "HandleRangingReqWhenNotReg(): Received Ranging Request when Not registered");
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }

  if (pFsmObj->mRangingFsmContext.notTryingToRegister)
  {
    if(SendRegRequest(pFsmObj) != 0)
    {
      log_debug(TAG, "HandleRangingReqWhenNotReg: Failed to Start Registration Process");
      return -1;
    }
  }
  return 0;
}
int LOWIRomeFSM::IgnoreRangingReq(LOWIRomeFSM* pFsmObj)
{
  int retVal = 0;
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }
  log_debug(TAG, "IgnoreRangingReq(): Ignoring newly arrived ranging request");
  LOWIMeasurementResult* result = new (std::nothrow) LOWIMeasurementResult;
  if (result == NULL)
  {
    log_debug(TAG, "IgnoreRangingReq() - Warning: failed to allocate memory for Ranging Measurment Results");
    return -1;
  }
  if (pFsmObj->mRangingFsmContext.curEvent == EVENT_INVALID_REQ)
  {
    retVal = pFsmObj->InitializeMeasResult(result, LOWIResponse::SCAN_STATUS_INVALID_REQ);
  }
  else
  {
    retVal = pFsmObj->InitializeMeasResult(result);
  }
  if (retVal != 0)
  {
    log_info(TAG, "IgnoreRangingReq(): Something went wrong when trying to initialize LOWI Meas Results");
  }
  else
  {
    pFsmObj->mRangingFsmContext.curEvent = EVENT_RANGING_RESPONSE_TO_USER;
    /* Send Result to User */
    pFsmObj->SendResultToClient(result);
  }
  return retVal;
}

int LOWIRomeFSM::SendRegRequest(LOWIRomeFSM* pFsmObj)
{
  int retVal = 0;
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }
  retVal = RomeSendRegReq();
  if (retVal != 0)
  {
    /* Failed to send Registration request */
    log_warning(TAG, "SendRegRequest(): Failed to Send Registration request to Host Driver");
    pFsmObj->mRangingFsmContext.curEvent = EVENT_REGISTRATION_FAILURE_OR_LOST;
    HandleRegFailureOrLost(pFsmObj);
  }
  else
  {
    pFsmObj->mRangingFsmContext.notTryingToRegister = FALSE;
    pFsmObj->mRangingFsmContext.timeEnd = get_time_rtc_ms() + (SELECT_TIMEOUT_NORMAL * 1000);
  }
  return retVal;
}

int LOWIRomeFSM::HandleRegSuccess(LOWIRomeFSM *pFsmObj)
{
  IwOemDataCap iwOemDataCap;
  int retVal = -1;
  LOWIMacAddress localStaMac;
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }
  do
  {
    if (RomeExtractRegRsp(pFsmObj->mRangingFsmData) != 0)
    {
      log_warning(TAG, "HandleRegSuccess - Failed to Register with Wi-Fi Host Driver");
      HandleRegFailureOrLost(pFsmObj);
      break;
    }
    if (LOWIWifiDriverUtils::getWiFiIdentityandCapability(&iwOemDataCap, localStaMac) < 0)
    {
      log_warning(TAG, "HandleRegSuccess - Failed to Get Wifi Capabilities");
      HandleRegFailureOrLost(pFsmObj);
      break;
    }
    if (RomeSendChannelInfoReq(iwOemDataCap) != 0)
    {
      log_warning(TAG, "HandleRegSuccess - Failed to request Channel Information");
      HandleRegFailureOrLost(pFsmObj);
      break;
    }
    pFsmObj->mRangingFsmContext.timeEnd = get_time_rtc_ms() + (SELECT_TIMEOUT_NORMAL * 1000);
    pFsmObj->mRangingFsmContext.curState = STATE_WAITING_FOR_CHANNEL_INFO;
    retVal = 0;
  } while (0);

  return retVal;
}

int LOWIRomeFSM::HandleChannelInfo(LOWIRomeFSM *pFsmObj)
{
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }

  memset(mChannelInfoArray, 0, sizeof(mChannelInfoArray));
  if (RomeExtractChannelInfo(pFsmObj->mRangingFsmData, mChannelInfoArray) != 0)
  {
    log_warning(TAG, "HandleChannelInfo - Failed to get Channel Information");
    HandleRegFailureOrLost(pFsmObj);
    return -1;
  }
  if (RomeSendRangingCapReq() != 0)
  {
    log_warning(TAG, "HandleChannelInfo - Failed to request Ranging Capabilities from FW");
    HandleRegFailureOrLost(pFsmObj);
    return -1;
  }
  pFsmObj->mRangingFsmContext.timeEnd = get_time_rtc_ms() + (SELECT_TIMEOUT_NORMAL * 1000);
  pFsmObj->mRangingFsmContext.curState = STATE_WAITING_FOR_RANGING_CAP;
  return 0;
}

int LOWIRomeFSM::HandleRangingCap(LOWIRomeFSM *pFsmObj)
{
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }

  if (RomeExtractRangingCap(pFsmObj->mRangingFsmData, &(pFsmObj->mRomeRttCapabilities)) != 0)
  {
    log_warning(TAG, "HandleRangingCap - Failed to get Ranging Capabilities from FW");
    HandleRegFailureOrLost(pFsmObj);
    return -1;
  }
  pFsmObj->mRangingFsmContext.curState = STATE_READY_AND_IDLE;

  if (pFsmObj->NewRequestPending()) /* Check if there is a pending new request, if so process it immediately */
  {
    pFsmObj->setFsmInternalEvent();
    pFsmObj->LowiReqToEvent(pFsmObj->mNewReq);
  }
  else /* Other wise sit and wait for new request */
  {
    pFsmObj->mRangingFsmContext.timeEnd = ROME_FSM_TIMEOUT_FOREVER;
  }
  return 0;
}

int LOWIRomeFSM::InitializeMeasResult(LOWIMeasurementResult *lowiMeasResult, LOWIResponse::eScanStatus status)
{
  log_verbose(TAG, "InitializeMeasResult()");
  if (lowiMeasResult == NULL)
  {
    log_warning(TAG, "InitializeMeasResult(): Received an invalid Meas Results Pointer!!!");
    return -1;
  }

  lowiMeasResult->scanType = LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_UNKNOWN;
  lowiMeasResult->scanStatus = status;
  lowiMeasResult->request = mNewRangingReq;

  return 0;
}

int LOWIRomeFSM::HandleConfigReq(LOWIRomeFSM *pFsmObj)
{
  int retVal = 0;
  log_verbose(TAG, "%s", __FUNCTION__);
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }

  pFsmObj->mRtsCtsTag++;
  if (pFsmObj->mNewLCIConfigReq)
  {
    log_verbose(TAG, "%s: Sending New LCI Configuration Request", __FUNCTION__);
    pFsmObj->mCurReq = pFsmObj->mNewLCIConfigReq;
    retVal = RomeSendLCIConfiguration(pFsmObj->mRtsCtsTag, pFsmObj->mNewLCIConfigReq);
    pFsmObj->mNewLCIConfigReq = NULL;
  }
  else if (pFsmObj->mNewLCRConfigReq)
  {
    log_verbose(TAG, "%s: Sending New LCR Configuration Request", __FUNCTION__);
    pFsmObj->mCurReq = pFsmObj->mNewLCRConfigReq;
    retVal = RomeSendLCRConfiguration(pFsmObj->mRtsCtsTag, pFsmObj->mNewLCRConfigReq);
    pFsmObj->mNewLCRConfigReq = NULL;
  }
  else if (pFsmObj->mNewLCIReq)
  {
    log_verbose(TAG, "%s: Sending New LCI Request", __FUNCTION__);
    pFsmObj->mCurReq = pFsmObj->mNewLCIReq;
    retVal = RomeSendLCIRequest(pFsmObj->mRtsCtsTag, pFsmObj->mNewLCIReq);
    pFsmObj->mNewLCIReq = NULL;
  }
  else if (pFsmObj->mNewFTMRangingReq)
  {
    log_verbose(TAG, "%s: Sending FTMRR", __FUNCTION__);
    pFsmObj->mCurReq = pFsmObj->mNewFTMRangingReq;
    retVal = RomeSendFTMRR(pFsmObj->mRtsCtsTag, pFsmObj->mNewFTMRangingReq);
    pFsmObj->mNewFTMRangingReq = NULL;
  }
  else
  {
    log_verbose(TAG, "%s: No valid Configuration Request", __FUNCTION__);
    retVal = -1;
  }

  if (retVal != -1) // Succesfully sent Request to FW
  {
    pFsmObj->mRangingFsmContext.curState = STATE_PROCESSING_CONFIG_REQ;

    log_verbose(TAG, "%s: Setting timeout %u secs in the future",
                __FUNCTION__,
                SELECT_TIMEOUT_NORMAL);
    pFsmObj->mRangingFsmContext.timeEnd = get_time_rtc_ms() + (SELECT_TIMEOUT_NORMAL * 1000);
  }

  return retVal;
}

int LOWIRomeFSM::HandleRangingReq(LOWIRomeFSM *pFsmObj)
{
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }

  if (pFsmObj->ValidateRangingRequest() != 0)
  {
    log_debug(TAG, "%s: Ranging Request Not Valid, Aborting request", __FUNCTION__);
    pFsmObj->RejectNewRangingReq();
    return -1;
  }
  if (pFsmObj->PrepareRangingRequest() != 0)
  {
    log_debug(TAG, "HandleRangingReq(): Failed to Prepare Ranging Request");
    IgnoreRangingReq(pFsmObj);
    return -1;
  }
  if (pFsmObj->SendRangingReq() != 0)
  {
    log_debug(TAG, "HandleRangingReq(): Failed to Send Ranging Request to FW");
    IgnoreRangingReq(pFsmObj);
    return -1;
  }
  pFsmObj->mRangingFsmContext.curState = STATE_PROCESSING_RANGING_REQ;

  uint64 timeOut = (pFsmObj->mRomeReqRspInfo.nonAsapTargetPresent ?
                    SELECT_TIMEOUT_NON_ASAP_TARGET :
                    SELECT_TIMEOUT_NORMAL);
  log_verbose(TAG, "%s: Setting timeout %u secs in the future", __FUNCTION__, timeOut);
  pFsmObj->mRangingFsmContext.timeEnd = get_time_rtc_ms() + (timeOut * 1000);
  return 0;
}

int LOWIRomeFSM::HandleRangingMeas(LOWIRomeFSM* pFsmObj)
{

  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }

  /* Increment the total number of Measurement responses recieved from FW */
  pFsmObj->mRomeReqRspInfo.totalRspFromFw++;

  if(pFsmObj->ProcessRangingMeas() != 0)
  {
    log_info(TAG, "HandleRangingMeas(): Failed to Process Ranging Measurements");
  }

  pFsmObj->SendReqToFwOrRespToUser();

  return 0;
}

int LOWIRomeFSM::RejectNewRangingReq()
{
  int retVal = 0;
  log_debug(TAG, "%s: Rejecting newly arrived ranging request", __FUNCTION__);
  LOWIMeasurementResult* result = new (std::nothrow) LOWIMeasurementResult;
  if (result == NULL)
  {
    log_debug(TAG, "%s - Warning: failed to allocate memory for Ranging Measurment Results", __FUNCTION__);
    return -1;
  }
  retVal = InitializeMeasResult(result, LOWIResponse::SCAN_STATUS_INVALID_REQ);
  if (retVal != 0)
  {
    log_info(TAG, "%s: Something went wrong when trying to initialize LOWI Meas Results", __FUNCTION__);
  }
  else
  {
    mRangingFsmContext.curEvent = EVENT_RANGING_RESPONSE_TO_USER;
    /* Set New request to NULL */
    SetLOWIRequest(NULL);
    /* Send Result to User */
    SendResultToClient(result);
  }
  return retVal;
}

int LOWIRomeFSM::SendCurrentResultToUser(LOWIResponse::eScanStatus status)
{
  /* Done with the Ranging Request - Respond to User */
  mRangingFsmContext.timeEnd = ROME_FSM_TIMEOUT_FOREVER;
  mRangingFsmContext.curEvent = EVENT_RANGING_RESPONSE_TO_USER;
  mRangingFsmContext.curState = STATE_READY_AND_IDLE;
  /* Send Result to User */
  if (mLowiMeasResult)
  {
    mLowiMeasResult->scanStatus = status;
    SendResultToClient(mLowiMeasResult);
  }
  else
  {
    IgnoreRangingReq(this);
  }

  return 0;
}

int LOWIRomeFSM::SendReqToFwOrRespToUser()
{
  /* check to see if all expected Responses from FW have arrived */
  mRomeReqRspInfo.lastResponseRecv = (mRomeReqRspInfo.totalRspFromFw == mRomeReqRspInfo.expectedRspFromFw)? TRUE : FALSE;

  /* All Expected responses have arrived from FW */
  if (mRomeReqRspInfo.lastResponseRecv)
  {
    /* Send another Ranging Request to FW if the Ranging request is still valid */
    if (mRomeRangingReq.validRangingScan)
    {
      if(SendRangingReq() != 0)
      {
        log_warning(TAG, ": Failed to Send Ranging Request, Aborting current LOWI request");
        /* Done with the Ranging Request - Respond to User */
        (mLowiMeasResult)->scanStatus = LOWIResponse::SCAN_STATUS_SUCCESS;
        mRangingFsmContext.timeEnd = ROME_FSM_TIMEOUT_FOREVER;
        mRangingFsmContext.curEvent = EVENT_RANGING_RESPONSE_TO_USER;
        mRangingFsmContext.curState = STATE_READY_AND_IDLE;
        /* Send Result to User */
        SendResultToClient(mLowiMeasResult);
        return -1;
      }
      uint64 timeOut = (mRomeReqRspInfo.nonAsapTargetPresent ?
                        SELECT_TIMEOUT_NON_ASAP_TARGET :
                        SELECT_TIMEOUT_NORMAL);
      log_verbose(TAG, "%s: Setting timeout %u secs in the future",
                  __FUNCTION__,
                  timeOut);
      mRangingFsmContext.timeEnd = get_time_rtc_ms() + (timeOut * 1000);
    }
    else /* Other wise send the result to User */
    {
      /* Done with the Ranging Request - Respond to User */
      mRangingFsmContext.timeEnd = ROME_FSM_TIMEOUT_FOREVER;
      mRangingFsmContext.curEvent = EVENT_RANGING_RESPONSE_TO_USER;
      mRangingFsmContext.curState = STATE_READY_AND_IDLE;
      if (NULL != mLowiMeasResult)
      {
        (mLowiMeasResult)->scanStatus = LOWIResponse::SCAN_STATUS_SUCCESS;

        /* Send Result to User */
        SendResultToClient(mLowiMeasResult);
      }
    }
  }
  else /* Still expecting responses from FW */
  {
    // Do Nothing
  }

  return 0;
}

int LOWIRomeFSM::HandleConfigRspOrErrorMsg(LOWIRomeFSM* pFsmObj)
{
  log_verbose(TAG, "%s", __FUNCTION__);
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }

  int retVal = HandleRangingErrorMsg(pFsmObj);

  if (pFsmObj->mRomeReqRspInfo.configStatus == ROME_CONFIG_SUCCESS ||
      pFsmObj->mRomeReqRspInfo.configStatus == ROME_CONFIG_FAIL)
  {
    log_verbose(TAG, "%s Received response for Configuration request", __FUNCTION__);
    pFsmObj->mLowiMeasResult = new (std::nothrow) LOWIMeasurementResult;
    if (pFsmObj->mLowiMeasResult == NULL)
    {
      log_debug(TAG, "HandleRangingReq () - Warning: failed to allocate memory for Ranging Measurment Results");
      return -1;
    }
    pFsmObj->mLowiMeasResult->scanType = LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_UNKNOWN;
    if (pFsmObj->mRomeReqRspInfo.configStatus == ROME_CONFIG_SUCCESS)
    {
      log_verbose(TAG, "%s Received response for Configuration request: SUCCESS", __FUNCTION__);
      pFsmObj->mLowiMeasResult->scanStatus = LOWIResponse::SCAN_STATUS_SUCCESS;
    }
    else
    {
      log_verbose(TAG, "%s Received response for Configuration request: FAILURE", __FUNCTION__);
      pFsmObj->mLowiMeasResult->scanStatus = LOWIResponse::SCAN_STATUS_DRIVER_ERROR;
    }
    pFsmObj->mLowiMeasResult->request = pFsmObj->mCurReq;

    pFsmObj->SendResultToClient(pFsmObj->mLowiMeasResult);

    pFsmObj->mCurReq = NULL;

    pFsmObj->mRangingFsmContext.timeEnd  = ROME_FSM_TIMEOUT_FOREVER;
    pFsmObj->mRangingFsmContext.curEvent = EVENT_CONFIG_RESPONSE_TO_USER;
    pFsmObj->mRangingFsmContext.curState = STATE_READY_AND_IDLE;
  }
  return retVal;
}

int LOWIRomeFSM::HandleRangingErrorMsg(LOWIRomeFSM* pFsmObj)
{
  int retVal = 0;
  tANI_U32 errorCode;
  tANI_U8 bssid[WIFI_MAC_ID_SIZE + 2];

  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }

  if (RomeExtractRangingError(pFsmObj->mRangingFsmData, &errorCode, bssid) != 0)
  {
    log_info(TAG, "HandleRangingErrorMsg: Failed to Extract Error Code from Ranging Error Message");
    return -1;
  }

  if (errorCode > WMI_RTT_REJECT_MAX)
  {
    log_debug(TAG, "HandleRangingErrorMsg - Received Invalid Error Code from FW: %u", errorCode);
    return -1;
  }
  log_debug(TAG, "HandleRangingErrorMsg - Received Error: %s", sRangingErrorCodes[errorCode]);
  switch (errorCode)
  {
    case RTT_COMMAND_HEADER_ERROR:       //rtt cmd header parsing error --terminate
    case RTT_MODULE_BUSY:                //rtt no resource -- terminate
    case RTT_NO_RESOURCE:                //Any Resource allocate failure
    case RTT_CHAN_SWITCH_ERROR:          //channel swicth failed
    case RTT_REPORT_TYPE2_MIX:           //do not allow report type2 mix with type 0, 1
    case RTT_COMMAND_ERROR:              //rtt body parsing error -- skip current STA REQ
    {
      // Entire request Rejected
      pFsmObj->mRomeReqRspInfo.totalRspFromFw = 0;
      pFsmObj->mRomeReqRspInfo.expectedRspFromFw = 0;
      break;
    }
    case RTT_TOO_MANY_STA:               //STA exceed the support limit -- only serve the first n STA
    {
      // Do Nothing
      if (pFsmObj->mRomeReqRspInfo.reportType == RTT_REPORT_PER_FRAME_WITH_CFR ||
          pFsmObj->mRomeReqRspInfo.reportType == RTT_REPORT_PER_FRAME_NO_CFR)
      {
        /* For Report Type 0/1 Reduce the expected number of measurements
           to MAX supported */
        pFsmObj->mRomeReqRspInfo.expectedRspFromFw = (MAX_BSSIDS_TO_SCAN * pFsmObj->mRomeReqRspInfo.measPerTarget);
      }
      break;
    }
    case RTT_VDEV_ERROR:                 //can not find vdev with vdev ID -- skip current STA REQ
    case RTT_FRAME_TYPE_NOSUPPORT:       //We do not support RTT measurement with this type of frame
    case RTT_TMR_TRANS_ERROR:            //TMR trans error, this dest peer will be skipped
    case RTT_TM_TIMER_EXPIRE:            //wait for first TM timer expire -- terminate current STA measurement
    case WMI_RTT_REJECT_MAX:             // Temporarily this will be used to indicate FTMR rejected by peer
    {
      // For Report Type 0/1 - No measurement reports for the specifed BSSID
      // For report Type 2   - No measurements for specified BSSID in Aggregated Report
      log_info(TAG, " Due to error: %s, bssid: " QUIPC_MACADDR_FMT " will be skipped",
               sRangingErrorCodes[errorCode],
               QUIPC_MACADDR(bssid));

      if (pFsmObj->mRomeReqRspInfo.reportType == RTT_REPORT_PER_FRAME_WITH_CFR ||
          pFsmObj->mRomeReqRspInfo.reportType == RTT_REPORT_PER_FRAME_NO_CFR)
      {
        /* For Report Type 0/1 Reduce the expected number of measurements
           by subtracting number of measurements/Target */
        pFsmObj->mRomeReqRspInfo.expectedRspFromFw -= pFsmObj->mRomeReqRspInfo.measPerTarget;
      }
      break;
    }
    case RTT_TRANSIMISSION_ERROR:        //Tx failure -- continiue and measure number-- Applicable only for RTT V2
    case RTT_NO_REPORT_BAD_CFR_TOKEN:    //V3 only. If both CFR and Token mismatch, do not report
    case RTT_NO_REPORT_FIRST_TM_BAD_CFR: //For First TM, if CFR is bad, then do not report
    {
      // For Report Type 0/1 - No measurement report for current Frame for specifed BSSID
      // For report Type 2   - No measurement for 1 frame in aggregated report for specifed BSSID
      if (pFsmObj->mRomeReqRspInfo.reportType == RTT_REPORT_PER_FRAME_WITH_CFR ||
          pFsmObj->mRomeReqRspInfo.reportType == RTT_REPORT_PER_FRAME_NO_CFR)
      {
        /* For Report Type 0/1 Reduce the expected number of measurements by 1 */
        pFsmObj->mRomeReqRspInfo.expectedRspFromFw--;
      }
      break;
    }
    case RTT_TIMER_EXPIRE:               //Whole RTT measurement timer expire -- terminate current STA measurement
    {
      // For Report Type 0/1 - No MORE measurement reports for the specifed BSSID
      // For report Type 2   - Less than requested measurements for specified BSSID in Aggregated Report
      if (pFsmObj->mRomeReqRspInfo.reportType == RTT_REPORT_PER_FRAME_WITH_CFR ||
          pFsmObj->mRomeReqRspInfo.reportType == RTT_REPORT_PER_FRAME_NO_CFR)
      {
        /* For Report Type 0/1 Reduce the expected number of measurements
           by subtracting number of measurements remaining for the current BSSID */
        pFsmObj->mRomeReqRspInfo.expectedRspFromFw -= (pFsmObj->mRomeReqRspInfo.measPerTarget - (pFsmObj->mRomeReqRspInfo.totalRspFromFw % pFsmObj->mRomeReqRspInfo.measPerTarget));
      }
      break;
    }
    case RTT_LCI_CFG_OK:                 //LCI Configuration OK
    case RTT_LCR_CFG_OK:                 //LCR Configuration OK
    case RTT_LCI_REQ_OK:                 //Where are you request OK
    case RTT_FTMRR_OK:                   // FTMRR OK
    {
      log_verbose(TAG, "%s Receveid Error Code: %s", __FUNCTION__, sRangingErrorCodes[errorCode]);
      if (pFsmObj->mRangingFsmContext.curState == STATE_PROCESSING_CONFIG_REQ)
      {
        pFsmObj->mRomeReqRspInfo.configStatus = ROME_CONFIG_SUCCESS;
      }
      else
      {
        // Do Nothing
        log_info(TAG, "%s Receveid Error Code: %s when not expecting it",
                 __FUNCTION__, sRangingErrorCodes[errorCode]);
      }
      break;
    }
    case RTT_CFG_ERROR:                  //Bad LCI or LCR Configuration Request
    {
      log_verbose(TAG, "%s Receveid Error Code: RTT_CFG_ERROR", __FUNCTION__);
      if (pFsmObj->mRangingFsmContext.curState == STATE_PROCESSING_CONFIG_REQ)
      {
        pFsmObj->mRomeReqRspInfo.configStatus = ROME_CONFIG_FAIL;
      }
      else
      {
        // Do Nothing
        log_info(TAG,"%s Receveid Error Code: RTT_CFG_ERROR when not expecting it",
                 __FUNCTION__);
      }
      break;
    }
    default:
    {
      log_debug(TAG, "%s - HandleRangingErrorMsg: Something bad happened, an invalid error arrived!", __FUNCTION__);
      break;
    }
  }

  if (errorCode != RTT_LCI_CFG_OK &&
      errorCode != RTT_LCR_CFG_OK &&
      errorCode != RTT_CFG_ERROR  &&
      errorCode != RTT_LCI_REQ_OK &&
      errorCode != RTT_FTMRR_OK)
  {
    /* Add Skipped Target to List */
    RomeAddSkippedTargetToList(errorCode, bssid);
    retVal = pFsmObj->SendReqToFwOrRespToUser();
  }

  return retVal;
}

int LOWIRomeFSM::HandleP2PInfo(LOWIRomeFSM* pFsmObj)
{
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }

  if(RomeExtractP2PInfo(pFsmObj->mRangingFsmData) != 0)
  {
    log_debug(TAG, ": Failed to Extract information from P2P Peer info message");
    return -1;
  }
  return 0;
}

int LOWIRomeFSM::HandleNlTimeout(LOWIRomeFSM* pFsmObj)
{
  if (NULL == pFsmObj)
  {
    log_warning(TAG, "%s: Invalid FSM Object", __FUNCTION__);
    return -1;
  }

  log_verbose(TAG, "HandleNlTimeout()");

  /* We will get no more Responses from FW */
  pFsmObj->mRomeReqRspInfo.totalRspFromFw = 0;
  pFsmObj->mRomeReqRspInfo.expectedRspFromFw = 0;
  pFsmObj->AddDummyMeas();

  if (pFsmObj->SendReqToFwOrRespToUser() != 0)
  {
    log_debug(TAG, "HandleNlTimeout(): Failed to Respond to User!!");
    return -1;
  }
  return 0;
}

int LOWIRomeFSM::SetLOWIRequest(LOWIRequest* pReq)
{
  log_verbose(TAG, "%s - Received new request from Client", __FUNCTION__);
  mNewReq = pReq;
  return 0;
}

bool LOWIRomeFSM::NewRequestPending()
{
  if (mNewReq != NULL)
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool LOWIRomeFSM::ProcessingRangingRequest()
{
  return mRomeRangingReq.validRangingScan;
}

int LOWIRomeFSM::SendResultToClient(LOWIMeasurementResult* result)
{
  int retVal = 0;
  /* Send Result through listener */
  if (mListener != NULL)
  {
    log_verbose(TAG, "SendResultToClient(): Total Number of APs in result: %u",
                result->scanMeasurements.getNumOfElements());
    mListener->scanResultsReceived(result);
  }
  else
  {
    log_debug(TAG, "SendResultToClient(): Invalid Listener Handle, failed to send back Result");
    delete result;
    retVal = -1;
  }

  /* Reset the request pointer */
  mNewRangingReq = NULL;

  return retVal;
}

bool LOWIRomeFSM::IsReady()
{
  if (mRangingFsmContext.curState == STATE_READY_AND_IDLE)
    return TRUE;
  else
    return FALSE;
}

LOWI_RangingCapabilities LOWIRomeFSM::GetRangingCap()
{
  LOWI_RangingCapabilities lowiRangingCap;
  memset(&lowiRangingCap, 0, sizeof(LOWI_RangingCapabilities));

  lowiRangingCap.oneSidedSupported = true;
  lowiRangingCap.dualSidedSupported11mc = true;
  lowiRangingCap.bwSupport = RTT_CAP_BW_80;
  lowiRangingCap.preambleSupport = (CAP_PREAMBLE_LEGACY |
                                    CAP_PREAMBLE_HT     |
                                    CAP_PREAMBLE_VHT);
  if (mRangingFsmContext.curState == STATE_READY_AND_IDLE)
  {
    lowiRangingCap.oneSidedSupported = (CAP_SINGLE_SIDED_RTT_SUPPORTED(mRomeRttCapabilities.rangingTypeMask)) ? TRUE : FALSE;
    lowiRangingCap.dualSidedSupported11mc = (CAP_11MC_DOUBLE_SIDED_RTT_SUPPORTED(mRomeRttCapabilities.rangingTypeMask)) ? TRUE : FALSE;
    lowiRangingCap.bwSupport = mRomeRttCapabilities.maxBwAllowed;
    lowiRangingCap.preambleSupport = mRomeRttCapabilities.preambleSupportedMask;
  }

  return lowiRangingCap;
}

void LOWIRomeFSM::LogRangingRequest(vector <LOWINodeInfo> &v)
{
  mRtsCtsTag++;
}

unsigned int LOWIRomeFSM::ExpectedMeasSizeForTarget(unsigned int numMeas, uint32 ftmRangingParams)
{
  unsigned int maxLen = 0;
  bool lci = false, lcr = false;

  lci = FTM_GET_LCI_REQ(ftmRangingParams);
  lcr = FTM_GET_LOC_CIVIC_REQ(ftmRangingParams);

  // The size of Subtype Field +
  // RTT Message Header +
  // Per Target Header
  maxLen += sizeof(OemMsgSubType) +
            sizeof(RomeRTTReportHeaderIE) +
            sizeof(RomeRttPerPeerReportHdr);
  // Measurement Size
  maxLen += numMeas * (sizeof(RomeRttPerFrame_IE_RTTV3));
  // Size for LCI information
  maxLen += lci ? LCI_FTM_LCI_TOT_MAX_LEN : 0;
  // Size for LCR information
  maxLen += lcr ? LOC_CIVIC_ELE_MAX_LEN : 0;

  return maxLen;
}

void LOWIRomeFSM::AddDummyMeas()
{
  // Find the index of the channel and the first BSSID in the previous request
  unsigned int chIdx = mRomeRangingReq.curChIndex;
  unsigned int apIdx = mRomeRangingReq.curAp;

  if (!mRomeRangingReq.validRangingScan || (apIdx == 0))
  {
    // If on 1st AP that means previous channel was used.
    // Go to last AP on this channel.
    chIdx--;
    apIdx = mRomeRangingReq.arrayVecRangingNodes[chIdx].getNumOfElements()-1;
  }
  else
  {
    apIdx--;
  }
  vector <LOWIRangingNode>& vNodes = mRomeRangingReq.arrayVecRangingNodes[chIdx];
  for (unsigned int bssIdx = 0; (bssIdx < mRomeReqRspInfo.bssidCount) &&
                                (bssIdx <= apIdx); bssIdx++)
  {
    AddTargetToResult(vNodes[apIdx-bssIdx].targetNode,
                      LOWIScanMeasurement::LOWI_TARGET_STATUS_FAILURE);
  }
}

bool LOWIRomeFSM::validChannelBWPreambleCombo(LOWINodeInfo node)
{

  bool retVal = false;

  do
  {
    /* Check to ensure the primary and center frequency provided by User are valid */
    if (LOWIUtils::freqToChannel(node.frequency) == 0)
    {
      log_debug (TAG, "%s, Invalid primary channel", __FUNCTION__);
      break;
    }

    /* Ensure Preamble requested by user is within the supported range */
    if (node.preamble < RTT_PREAMBLE_LEGACY ||
        node.preamble > RTT_PREAMBLE_VHT)
    {
      log_debug (TAG, "%s, Invalid Preamble requested", __FUNCTION__);
      break;
    }

    /* Ensure that the Channel spacing requested is allowed for requested BW */
    switch (node.bandwidth)
    {
      case BW_20MHZ:
      {
        /* For 20MHZ BW, only allow Center Frequency of 0 or same as Primary */
        if (IS_VALID_20MHZ_CHAN_SPACING(node.frequency, node.band_center_freq1))
        {
          /* Valid channel spacing */
          retVal = true;
        }
        break;
      }
      case BW_40MHZ:
      {
        /* For 40MHZ BW, only allow Center Frequency of Primary +/- 10 MHz */
        if (IS_VALID_40MHZ_CHAN_SPACING(node.frequency, node.band_center_freq1))
        {
          /* Valid channel spacing */
          retVal = true;
        }
        break;
      }
      case BW_80MHZ:
      {
        /* For 80MHZ BW, allow Center Frequency of Primary +/- 10 MHz OR Primary +/- 30 MHz */
        if (IS_VALID_80MHZ_CHAN_SPACING(node.frequency, node.band_center_freq1))
        {
          /* Valid channel spacing */
          retVal = true;
        }
        break;
      }
      default:
      {
        /* Invalid Bandwidth - Do Nothing */
        break;
      }
    }

    /* Ensure that the requested Bandwidth, Channel and preamble are an allowed combination */
    /* Using the valid Phy mode table to check the combination */
    uint32 band = IS_2G_CHANNEL(LOWIUtils::freqToChannel(node.frequency)) ?
                  LOWIDiscoveryScanRequest::TWO_POINT_FOUR_GHZ : LOWIDiscoveryScanRequest::FIVE_GHZ;
    if (retVal &&
        validPhyModeTable[band][node.preamble][node.bandwidth] == ROME_PHY_MODE_UNKNOWN)
    {
      /* Set to false, just in case the case the switch statement above had set it to true */
      retVal = false;
    }
    else
    {
      log_debug (TAG, "%s, Valid BW(%u), Channel(%u:%u) and preamble(%u) for AP:" QUIPC_MACADDR_FMT,
                 __FUNCTION__, node.bandwidth,
                 node.frequency, node.band_center_freq1,
                 node.preamble, QUIPC_MACADDR(node.bssid));
    }
  } while(0);

  if (retVal == false)
  {
    log_info (TAG, "%s, Invalid BW(%u), Channel and preamble(%u), Aborting! AP:" QUIPC_MACADDR_FMT
              " frequency: %u, center_freq1: %u",
              __FUNCTION__, node.bandwidth, node.preamble,
              QUIPC_MACADDR(node.bssid),
              node.frequency, node.band_center_freq1);
  }

  return retVal;
}

int LOWIRomeFSM::ValidateRangingRequest()
{
  if (mNewRangingReq == NULL)
  {
    log_debug(TAG, "%s: No New request to handle, Aborting call", __FUNCTION__);
    return -1;
  }

  vector <LOWINodeInfo> v = mNewRangingReq->getNodes();
  unsigned int numNonAsapTargets = 0;

  if (0 == v.getNumOfElements())
  {
    log_warning (TAG, "Ranging scan requested with no AP's");
    return -1;
  }
  else
  {
    for(unsigned int n = 0; n < v.getNumOfElements(); n++)
    {
      if (v[n].rttType < RTT1_RANGING ||
          v[n].rttType > RTT3_RANGING)
      {
        log_debug (TAG, "%s, Invalid Ranging type requested in Ranging scan, Aborting, AP# %u: , RTT Type: %u",
                     __FUNCTION__,
                     n+1,
                     v[n].rttType);
        return -1;
      }

      if (v[n].num_pkts_per_meas > MAX_RTT_MEAS_PER_DEST)
      {
        log_debug (TAG, "%s, Invalid number fo FTM frames requested in Ranging scan, Aborting, AP# %u: , FTMs: %u",
                     __FUNCTION__,
                     n+1,
                     v[n].num_pkts_per_meas);
        return -1;
      }

      if (!validChannelBWPreambleCombo(v[n]))
      {
        log_debug (TAG, "%s, Invalid Combination of Channel/BW/Preamble",
                     __FUNCTION__);
        return -1;

      }

      /* Check if it is an ASAP 0 request */
      if (FTM_GET_ASAP(v[n].ftmRangingParameters) == 0 &&
          v[n].rttType == RTT3_RANGING)
      {
        numNonAsapTargets++;
      }
    }

    /* Only 1 ASAP 0 target alloowed per request*/
    if (numNonAsapTargets > 1)
    {
      log_debug (TAG, "%s, More than 1 target requested with ASAP 0 in Ranging scan, Aborting, numNonAsapTargets: %u",
                 __FUNCTION__,
                 numNonAsapTargets);
      return -1;
    }
  }

  return 0;
}

void LOWIRomeFSM::setPhyMode(LOWIRangingNode &rangingNode)
{
  uint8 phyMode = ROME_PHY_MODE_11G;

  if ((LOWIUtils::freqToChannel(rangingNode.chanInfo.wmiChannelInfo.mhz) == 0) ||
      (rangingNode.targetNode.bandwidth < BW_20MHZ || rangingNode.targetNode.bandwidth >= BW_MAX) ||
      (rangingNode.targetNode.preamble < RTT_PREAMBLE_LEGACY || rangingNode.targetNode.preamble >= RTT_PREAMBLE_MAX))
  {
    log_debug(TAG, "%s, Not a valid preamble/BW/Channel: channel: %u, preamble: %u, Bw: %u",
              __FUNCTION__,
              rangingNode.chanInfo.wmiChannelInfo.mhz,
              rangingNode.targetNode.preamble,
              rangingNode.targetNode.bandwidth);
  }
  else
  {
    uint32 band = IS_2G_CHANNEL(LOWIUtils::freqToChannel(rangingNode.chanInfo.wmiChannelInfo.mhz)) ?
                  LOWIDiscoveryScanRequest::TWO_POINT_FOUR_GHZ : LOWIDiscoveryScanRequest::FIVE_GHZ;
    phyMode = validPhyModeTable[band][rangingNode.targetNode.preamble][rangingNode.targetNode.bandwidth];
    /* Default to ROME_PHY_MODE_11G if invalid preamble, BW & band combination */
    phyMode = (phyMode == ROME_PHY_MODE_UNKNOWN) ? ((uint8)ROME_PHY_MODE_11G) : phyMode;
  }

  rangingNode.chanInfo.wmiChannelInfo.info &= PHY_MODE_MASK;
  rangingNode.chanInfo.wmiChannelInfo.info |= phyMode;
}

int LOWIRomeFSM::AddTargetToResult(LOWINodeInfo node, LOWIScanMeasurement::eTargetStatus errorCode)
{
  log_debug(TAG, "%s: Add " QUIPC_MACADDR_FMT " errorCode %d",
            __FUNCTION__, QUIPC_MACADDR(node.bssid), errorCode);
  vector <LOWIScanMeasurement*> *scanMeasurements = &mLowiMeasResult->scanMeasurements;
  LOWIScanMeasurement* rangingMeasurement = new (std::nothrow) LOWIScanMeasurement;
  if (rangingMeasurement == NULL)
  {
    log_warning(TAG, "%s: Failed to allocate memory for rangingMeasurement", __FUNCTION__);
    return -1;
  }
  rangingMeasurement->bssid.setMac(node.bssid);
  rangingMeasurement->frequency = node.frequency;
  rangingMeasurement->isSecure = false;
  rangingMeasurement->msapInfo = NULL;
  rangingMeasurement->cellPowerLimitdBm = 0;

  rangingMeasurement->type = ACCESS_POINT;
  rangingMeasurement->rttType = node.rttType;
  rangingMeasurement->num_frames_attempted = 0;
  rangingMeasurement->actual_burst_duration = 0;
  rangingMeasurement->negotiated_num_frames_per_burst = 0;
  rangingMeasurement->retry_after_duration = 0;
  rangingMeasurement->negotiated_burst_exp = 0;

  rangingMeasurement->targetStatus = errorCode;

  scanMeasurements->push_back(rangingMeasurement);

  return 0;

}

int LOWIRomeFSM::PrepareRangingRequest()
{
  log_verbose(TAG, "%s", __FUNCTION__);
  if (mNewRangingReq != NULL)
  {
    if (mRomeRttCapabilities.rangingTypeMask == 0)
    {
      // No support for Ranging
      log_debug(TAG, "Ranging Not supported: %d",
                mRomeRttCapabilities.rangingTypeMask);
      return -1;
    }
    vector <LOWINodeInfo> v = mNewRangingReq->getNodes();
    LogRangingRequest(v);
    if (0 == v.getNumOfElements())
    {
      log_warning (TAG, "Ranging scan requested with no AP's");
      return -1;
    }
    else
    {
      mLowiMeasResult = new (std::nothrow) LOWIMeasurementResult;
      if (mLowiMeasResult == NULL)
      {
        log_debug(TAG, "HandleRangingReq () - Warning: failed to allocate memory for Ranging Measurment Results");
        return -1;
      }
      if(InitializeMeasResult(mLowiMeasResult) != 0)
      {
        log_info(TAG, "HandleRangingReq(): Something went wrong when trying to initialize LOWI Meas Results");
        return -1;
      }
      log_verbose (TAG, "Ranging scan requested with : %u APs", v.getNumOfElements());

      mRomeRangingReq.vecRangingNodes.flush();
      for(unsigned int oof = 0; oof < v.getNumOfElements(); oof++)
      {
        LOWIScanMeasurement scanMeasurement;
        LOWIRangingNode rangingNode;
        rangingNode.targetNode = v[oof];
        rangingNode.tsfDelta = 0;
        rangingNode.tsfValid = false;

        uint32 chan = LOWIUtils::freqToChannel(v[oof].frequency);
        if (chan  == 0)
        {
          return -1;
        }

        rangingNode.chanInfo.wmiChannelInfo.mhz = v[oof].frequency;
        rangingNode.chanInfo.wmiChannelInfo.band_center_freq1 = v[oof].band_center_freq1;
        rangingNode.chanInfo.wmiChannelInfo.band_center_freq2 = 0;
        rangingNode.chanInfo.wmiChannelInfo.info &= PHY_MODE_MASK;
        rangingNode.chanInfo.wmiChannelInfo.info |= mChannelInfoArray[chan -1].wmiChannelInfo.info;
        setPhyMode(rangingNode);
        rangingNode.chanInfo.wmiChannelInfo.reg_info_1 = mChannelInfoArray[chan -1].wmiChannelInfo.reg_info_1;
        rangingNode.chanInfo.wmiChannelInfo.reg_info_2 = mChannelInfoArray[chan -1].wmiChannelInfo.reg_info_2;

        log_verbose (TAG, "%s: AP: " QUIPC_MACADDR_FMT " frequency: %u, phymode %d",
                     __FUNCTION__, QUIPC_MACADDR(v[oof].bssid),v[oof].frequency,
                     rangingNode.chanInfo.wmiChannelInfo.info & ~PHY_MODE_MASK);

#if 0 /* If 0'ed out because we are not using cache for now */
        if (mCacheManager &&
            mCacheManager->getFromCache(v[oof].bssid, scanMeasurement) == true)
        {
          rangingNode.chanInfo.wmiChannelInfo.mhz = scanMeasurement.frequency;
          rangingNode.chanInfo.wmiChannelInfo.band_center_freq1 = scanMeasurement.band_center_freq1;
          rangingNode.chanInfo.wmiChannelInfo.band_center_freq2 = scanMeasurement.band_center_freq2;
          rangingNode.chanInfo.wmiChannelInfo.info &= PHY_MODE_MASK;
          rangingNode.chanInfo.wmiChannelInfo.info |= scanMeasurement.info;
          rangingNode.tsfDelta = scanMeasurement.tsfDelta;
          rangingNode.tsfValid = true;
          log_verbose(TAG, "%s: Valid TSF: %u", __FUNCTION__, rangingNode.tsfDelta);
        }
        else
        {
          log_verbose(TAG, "%s: InValid TSF: %u", __FUNCTION__, rangingNode.tsfDelta);
        }
#else /* Getting just TSF Delta from Cache for now */
        if (mCacheManager &&
            mCacheManager->getFromCache(v[oof].bssid, scanMeasurement) == true)
        {
          rangingNode.tsfDelta = scanMeasurement.tsfDelta;
          rangingNode.tsfValid = true;
        }
#endif
        mRomeRangingReq.vecRangingNodes.push_back(rangingNode);
      }
    }

    /** Group APs by Channel Structure */
    for(unsigned int i = 0; i < MAX_DIFFERENT_CHANNELS_ALLOWED; i++)
    {
      mRomeRangingReq.arrayVecRangingNodes[i].flush();
    }
    mRomeRangingReq.totChs = groupByChannel(mRomeRangingReq.vecRangingNodes,
                                            &mRomeRangingReq.arrayVecRangingNodes[0],
                                            MAX_DIFFERENT_CHANNELS_ALLOWED);
    mRomeRangingReq.curChIndex = 0;
    mRomeRangingReq.totAp = mRomeRangingReq.arrayVecRangingNodes[mRomeRangingReq.curChIndex].getNumOfElements();
    mRomeRangingReq.curAp = 0;
    mRomeRangingReq.validRangingScan = TRUE;
    for(unsigned int foo = 0; foo < mRomeRangingReq.totChs; ++foo)
    {
      unsigned int apCount = mRomeRangingReq.vecChGroup[foo].getNumOfElements();
      for(unsigned int bar = 0 ;bar <apCount; bar++)
      {
        log_verbose (TAG, "BSSID: " QUIPC_MACADDR_FMT "frequency: %u, Center Frequency 1: %u",
                     QUIPC_MACADDR(mRomeRangingReq.arrayVecRangingNodes[foo][bar].targetNode.bssid),
                     mRomeRangingReq.arrayVecRangingNodes[foo][bar].chanInfo.wmiChannelInfo.mhz,
                     mRomeRangingReq.arrayVecRangingNodes[foo][bar].chanInfo.wmiChannelInfo.band_center_freq1);
      }
    }
  }
  else
  {
    log_debug(TAG, "No New request");
    return -1;
  }

  return 0;
}

unsigned int LOWIRomeFSM::groupByChannel(vector <LOWIRangingNode> origVec,
                                         vector <LOWIRangingNode> *vec,
                                         unsigned int maxChannels)
{
  unsigned int chCount = 0;

  log_verbose (TAG, "groupByChannel - Num Aps: %u", origVec.getNumOfElements());
  for(unsigned int i = 0; i < origVec.getNumOfElements(); ++i)
  {
    ChannelInfo chanInfo = origVec[i].chanInfo;
    log_verbose (TAG, "groupByChannel - Current AP's Frequency: %u, BSSID: " QUIPC_MACADDR_FMT,
                 chanInfo.wmiChannelInfo.mhz, QUIPC_MACADDR(origVec[i].targetNode.bssid));

    if(chanInfo.wmiChannelInfo.mhz == 0)
    {
      /** Note:  This should never happen, but simply,
        *  continuing to gracefully handle this case
        */
        log_debug(TAG, "AP with Channel Frequency of 0 was sent for Ranging!");
        continue;
    }

    boolean found = false;
    /* Search already gathered channels (last discovered channel first) */
    if (chCount != 0)
    {
      for(int j = (chCount - 1); j >= 0; j--)
      {
        vector <LOWIRangingNode>::Iterator it = vec[j].begin();
        LOWIRangingNode topNode = *it;

        if (memcmp(&(topNode.chanInfo), &chanInfo, sizeof(chanInfo)) == 0)
        {
          /* Match Found vector already exists so add to vector */
          log_verbose (TAG, "groupByChannel - Adding to existing vector [%u]",  j);
          vec[j].push_back(origVec[i]);
          found = true;
          break;
        }
      }
    }
    if(found == false) /* No matching freq was found, create a new vector for this frequency */
    {
      chCount++;
      if(chCount < maxChannels)
      {
        vec[chCount - 1].push_back(origVec[i]);
        log_verbose (TAG, "groupByChannel - Creating new vector for new channel");
      }
      else
      {
        log_debug (TAG, "Reached MAX number of different Channels per RTT request!");
        return chCount;
      }
    }
  }

  log_verbose(TAG, "%s - Tot Channels %u", __FUNCTION__, chCount);
  return chCount;
}

int LOWIRomeFSM::SendRangingReq()
{
  int retVal = 0;

  DestInfo   bssidsToScan[MAX_BSSIDS_TO_SCAN];
  vector <LOWIRangingNode>::Iterator it = mRomeRangingReq.arrayVecRangingNodes[mRomeRangingReq.curChIndex].begin();
  ChannelInfo chanInfo = (*it).chanInfo;
  unsigned int bssidIdx = 0;

  log_verbose(TAG, "SendRangingReq");

  if (!mRomeReqRspInfo.lastResponseRecv)
  {
    /* Don't send another Request because FW is still handling previous request */
    log_debug(TAG, "FW Still handling previous request");
    return 0;
  }

  log_verbose(TAG, "mRomeRangingReq.curChIndex: %u, mRomeRangingReq.totChs: %u, mRomeRangingReq.curAp: %u, mRomeRangingReq.totAp: %u",
              mRomeRangingReq.curChIndex,
              mRomeRangingReq.totChs,
              mRomeRangingReq.curAp,
              mRomeRangingReq.totAp);

  mRomeReqRspInfo.nonAsapTargetPresent = FALSE;

  if (mRomeRangingReq.curChIndex < mRomeRangingReq.totChs &&
      mRomeRangingReq.curAp < mRomeRangingReq.totAp)
  {
    vector <LOWIRangingNode>& vNodes = mRomeRangingReq.arrayVecRangingNodes[mRomeRangingReq.curChIndex];
    unsigned int spaceLeftInRsp = MAX_WMI_MESSAGE_SIZE;
    while (mRomeRangingReq.curAp < mRomeRangingReq.totAp)
    {
      LOWIRangingNode & rangingNode = vNodes[mRomeRangingReq.curAp];
      unsigned int spaceNeeded = ExpectedMeasSizeForTarget(MIN(rangingNode.targetNode.num_pkts_per_meas, MAX_RTT_MEAS_PER_DEST),
                                                           rangingNode.targetNode.ftmRangingParameters);
      if (spaceNeeded < spaceLeftInRsp)
      {
        spaceLeftInRsp -= spaceNeeded;
        log_verbose(TAG, "%s: Adding AP to this request - spaceUsed: %u spaceLeftInRsp: %u", __FUNCTION__, spaceNeeded, spaceLeftInRsp);
      }
      else
      {
        log_verbose(TAG, "%s: Done with this request - reached max response size, spaceUsed: %u, spaceLeftInRsp: %u", __FUNCTION__, spaceNeeded, spaceLeftInRsp);
        break;
      }
      switch (rangingNode.targetNode.rttType)
      {
        case RTT3_RANGING:
        {
          bssidsToScan[bssidIdx].rttFrameType  = RTT_MEAS_FRAME_TMR;
          break;
        }
        case RTT1_RANGING:
        case RTT2_RANGING:
        case BEST_EFFORT_RANGING: /* For now Best effor will default to RTT V2 */
        default: /* This should never happen, but just in case RTTV2 is the default */
        {
          bssidsToScan[bssidIdx].rttFrameType  = RTT_MEAS_FRAME_QOSNULL;
          break;
        }
      }
      for (int i = 0; i < WIFI_MAC_ID_SIZE; ++i)
      {
        bssidsToScan[bssidIdx].mac[i]  = rangingNode.targetNode.bssid[i];
      }
      bssidsToScan[bssidIdx].bandwidth = rangingNode.targetNode.bandwidth;
      bssidsToScan[bssidIdx].preamble  = rangingNode.targetNode.preamble;
      bssidsToScan[bssidIdx].numFrames = MIN(rangingNode.targetNode.num_pkts_per_meas, MAX_RTT_MEAS_PER_DEST);
      bssidsToScan[bssidIdx].numFrameRetries = rangingNode.targetNode.num_retries_per_meas;

      /* FTM Parameters*/
      bssidsToScan[bssidIdx].ftmParams = rangingNode.targetNode.ftmRangingParameters;
      bssidsToScan[bssidIdx].tsfDelta  = rangingNode.tsfDelta;
      bssidsToScan[bssidIdx].tsfValid  = rangingNode.tsfValid;

      /** Check if target is an ASAP 0 target */
      if ((rangingNode.targetNode.rttType == RTT3_RANGING) &&
          (FTM_GET_ASAP(rangingNode.targetNode.ftmRangingParameters) == 0))
      {
        mRomeReqRspInfo.nonAsapTargetPresent = TRUE;
      }

      mRomeRangingReq.curAp++;
      bssidIdx++;
    }

    log_verbose(TAG, "%s: Sending %u APs to FW for Ranging, ASAP %d",
                __FUNCTION__, bssidIdx, mRomeReqRspInfo.nonAsapTargetPresent);

    mRomeReqRspInfo.bssidCount = bssidIdx;
    mRomeReqRspInfo.measPerTarget = MAX_RTT_MEAS_PER_DEST;
    mRomeReqRspInfo.lastResponseRecv = FALSE;
    mRomeReqRspInfo.totalRspFromFw = 0;
    if(mCfrCaptureMode)
    {
      retVal = RomeSendRttReq(mRtsCtsTag,
                              chanInfo,
                              bssidIdx,
                              bssidsToScan,
                              bssidsToScan,
                              RTT_REPORT_PER_FRAME_WITH_CFR);

      mRomeReqRspInfo.reportType = RTT_REPORT_PER_FRAME_WITH_CFR;
      mRomeReqRspInfo.expectedRspFromFw = (bssidIdx * MAX_RTT_MEAS_PER_DEST);
    }
    else
    {
      retVal = RomeSendRttReq(mRtsCtsTag,
                              chanInfo,
                              bssidIdx,
                              bssidsToScan,
                              bssidsToScan,
                              RTT_AGGREGATE_REPORT_NON_CFR);

      mRomeReqRspInfo.reportType = RTT_AGGREGATE_REPORT_NON_CFR;
      mRomeReqRspInfo.expectedRspFromFw = RESP_REPORT_TYPE2;
    }

    /* If we still have APs left in the current channel */
    if (mRomeRangingReq.curAp < mRomeRangingReq.totAp)
    {
      log_verbose(TAG,"Stayng on current Channel Channel");
    }
    else /* Move to APs on the next channel if we are done with all APs in this channel*/
    {
      mRomeRangingReq.curChIndex++;
      /* if we have Channels left, change AP count and iterator to the list of APs in the new Channel */
      if (mRomeRangingReq.curChIndex < mRomeRangingReq.totChs)
      {
        log_verbose(TAG,"Moving to Next Channel");
        mRomeRangingReq.curAp = 0;
        mRomeRangingReq.totAp = mRomeRangingReq.arrayVecRangingNodes[mRomeRangingReq.curChIndex].getNumOfElements();
      }
      else /* We are done with all channels, mark this request as invalid */
      {
        log_verbose(TAG,"Done with All channels and APs");
        mRomeRangingReq.validRangingScan = FALSE;
      }
    }
  }
  else
  {
    mRomeRangingReq.validRangingScan = FALSE;
    log_verbose(TAG," %s : This should not have hapenned", __FUNCTION__);
  }

  return retVal;
}

int LOWIRomeFSM::ProcessRangingMeas()
{
  log_verbose(TAG, "ProcessRangingMeas");

  if(mCfrCaptureMode)
  {
    /* In CFR capture we do not parse the results - simple log them over Diag Log */
    return 0;
  }

  if (RomeParseRangingMeas((char*)mRangingFsmData, &mLowiMeasResult->scanMeasurements) != 0)
  {
    log_info (TAG, "%s, Failed to Process LOWI Ranging Meas from FW!", __FUNCTION__);
    return -1;
  }

  return 0;
}
/*=============================================================================================
 * Function description:
 *   Waits on the Private Netlink Socket and the LOWI controller pipe for Timeout if timeout> 0
 *   If Messages/Data arrive, collects and processes them accordingly.
 *   If LOWI controller requests a shutdown then exits and picks up the new pequest to process
 *   If Timeout occurs exits without processing any data.
 *
 * Parameters:
 *   timeout_val     : int number indicating timeout in seconds
 *
 * Return value:
 *    error code
 *
 =============================================================================================*/
int LOWIRomeFSM::ListenForEvents()
{
  int retVal = -1;

  int timeout_val = SELECT_TIMEOUT_NEVER;

  if (mRangingFsmContext.timeEnd)
  {
    uint64 timeNow = get_time_rtc_ms();
    timeout_val = (mRangingFsmContext.timeEnd - timeNow) / 1000;
  }

  retVal = RomeWaitOnActivityOnSocketOrPipe(timeout_val);

  if(retVal > 0)                             /* Some Valid Message Arrived on Socket */
  {
    RomeNlMsgType msgType;
    log_verbose(TAG, "ListenForEvents: Received Message on Private Netlink Socket \n");
    RomeNLRecvMessage(&msgType, mRangingFsmData, sizeof(mRangingFsmData));
    mRangingFsmContext.curEvent = RomeCLDToRomeFSMEventMap[msgType];
  }
  else if(retVal == ERR_SELECT_TIMEOUT)      /* Timeout Occured */
  {
    log_debug(TAG, "ListenForEvents: Timeout waiting on Private Netlink Socket \n");
    mRangingFsmContext.curEvent = EVENT_TIMEOUT;
  }
  else if(retVal == ERR_SELECT_TERMINATED)   /* Shutdown Through Pipe */
  {
    if (mRomePipeEvents == ROME_TERMINATE_THREAD)
    {
      mRangingFsmContext.curEvent = EVENT_TERMINATE_REQ;
    }
    else if (mRomePipeEvents == ROME_NEW_REQUEST_ARRIVED)
    {
      LowiReqToEvent(mNewReq);
    }
    log_verbose(TAG, "ListenForEvents: Event received over PIPE from LOWI controller: %s",
                sRangingFSMEvents[mRangingFsmContext.curEvent]);
  }
  else                                       /* Some Error Occured */
  {
    log_warning(TAG, "ListenForEvents: %s %d",
                retVal == ERR_NOT_READY ? "Driver not ready" :
                "Error when waiting for event", retVal);
    mRangingFsmContext.curEvent = EVENT_NOT_READY;
  }

  return retVal;
}

int LOWIRomeFSM::LowiReqToEvent(const LOWIRequest * const pReq)
{
  int ret_val = -1;
  if (NULL == pReq)
  {
    mRangingFsmContext.curEvent = EVENT_INVALID_REQ;
  }
  else if (mRangingFsmContext.curState >= STATE_READY_AND_IDLE)
  {
    if (pReq->getRequestType() == LOWIRequest::RANGING_SCAN)
    {
      mRangingFsmContext.curEvent = EVENT_RANGING_REQ;
      mNewRangingReq = (LOWIRangingScanRequest*) mNewReq;
      ret_val = 0;
    }
    else if (pReq->getRequestType() == LOWIRequest::SET_LCI_INFORMATION)
    {
      mRangingFsmContext.curEvent = EVENT_CONFIGURATION_REQ;
      mNewLCIConfigReq = (LOWISetLCILocationInformation*) mNewReq;
      ret_val = 0;
    }
    else if (pReq->getRequestType() == LOWIRequest::SET_LCR_INFORMATION)
    {
      mRangingFsmContext.curEvent = EVENT_CONFIGURATION_REQ;
      mNewLCRConfigReq = (LOWISetLCRLocationInformation*) mNewReq;
      ret_val = 0;
    }
    else if (pReq->getRequestType() == LOWIRequest::SEND_LCI_REQUEST)
    {
      mRangingFsmContext.curEvent = EVENT_CONFIGURATION_REQ;
      mNewLCIReq = (LOWISendLCIRequest *)mNewReq;
      ret_val = 0;
    }
    else if (pReq->getRequestType() == LOWIRequest::FTM_RANGE_REQ)
    {
      mRangingFsmContext.curEvent = EVENT_CONFIGURATION_REQ;
      mNewFTMRangingReq = (LOWIFTMRangingRequest *)mNewReq;
      ret_val = 0;
    }
    else
    {
      mRangingFsmContext.curEvent = EVENT_INVALID_REQ;
      mNewRangingReq = (LOWIRangingScanRequest*) mNewReq;
    }
    SetLOWIRequest(NULL);
  }
  else
  {
    mRangingFsmContext.curEvent = EVENT_RANGING_REQ;
  }
  return ret_val;
}

int LOWIRomeFSM::FSM()
{
  int retVal = -1;
  do
  {
    log_info(TAG, "%s: Received Event %s, Current state: %s", __FUNCTION__,
             sRangingFSMEvents[mRangingFsmContext.curEvent],
             sRangingFsmStates[mRangingFsmContext.curState]);
    if (mRangingFsmContext.curState < STATE_MAX &&
        mRangingFsmContext.curEvent < EVENT_MAX)
    {
      retVal = stateTable[mRangingFsmContext.curState][mRangingFsmContext.curEvent](this);
      log_info(TAG, "%s: New FSM state: %s", __FUNCTION__,
               sRangingFsmStates[mRangingFsmContext.curState]);
    }
    else
    {
      log_warning(TAG, "LOWI Ranging FSM received a bad State: %u or Event: %u",
                  mRangingFsmContext.curState, mRangingFsmContext.curEvent);
    }

    /* Check to see if there any any pending internal Events */
    if (mRangingFsmContext.internalFsmEventPending)
    {
      /* clear the flag and allow the event to be procesed by FSM */
      clearFsmInternalEvent();
    }
    else
    {
      /** After FSM has processed current event, wait listening for
       *  new events from Timer, NL socket & LOWI Controller */
      ListenForEvents();
    }

    /** If a terminate request arrived then stop FSM and return
     *  to caller */
    if (mRangingFsmContext.curEvent == EVENT_TERMINATE_REQ)
    {
      log_debug(TAG, "%s: Received Terminate Thread request from LOWI Controller",
                __FUNCTION__);
      break;
    }

  } while(1);
  return retVal;
}

int LOWIRomeFSM::SetNewPipeEvent(RomePipeEvents newEvent)
{
  mRomePipeEvents = newEvent;
  return RomeUnblockRangingThread();
}

