/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI ROME Wifi Driver

GENERAL DESCRIPTION
  This file contains the implementation of Rome Wifi Driver

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#include <string.h>
#include <base_util/log.h>
#include <lowi_server/lowi_rome_wifidriver.h>
#include "wipsiw.h"
#include "lowi_rome_ranging.h"
#include "wifiscanner.h"
#include <lowi_server/lowi_measurement_result.h>
#include "lowi_wifidriver_utils.h"

using namespace qc_loc_fw;

// Maximum RTT measurements per target for Aggregate report type
#define MAX_RTT_MEAS_PER_DEST 5
// MAximum RTT measurements per target for CFR capture measurements
#define MAX_RTT_MEAS_PER_DEST_CFR_CAP 1
#define DEFAULT_LOWI_RTS_CTS_NUM_MEAS 5
#define DEFAULT_LOWI_RSSI_THRESHOLD_FOR_RTT -140
#define MAX_CACHED_SCAN_AGE_SEC 30
// Structure containing supported channels
static s_ch_info supportedChannels;

/* WLAN frame parameters */
static uint8 gDialogTok = 1;
static uint8 gMeasTok = 1;

const char * const LOWIROMEWifiDriver::TAG = "LOWIROMEWifiDriver";

static vector <LOWINodeInfo> vecChGroup[MAX_DIFFERENT_CHANNELS_ALLOWED];

LOWIROMEWifiDriver::LOWIROMEWifiDriver(ConfigFile *config,
                                       LOWIScanResultReceiverListener *scanResultListener,
                                       LOWIInternalMessageReceiverListener *internalMessageListener,
                                       LOWICacheManager *cacheManager) : LOWIWifiDriverInterface(config)
{

  log_verbose (TAG, "LOWIROMEWifiDriver () - with Internal Message Listener");
  mInternalMessageListener = internalMessageListener;

  int auxInt = 0;
  mReq = NULL;
  mInternalMsgId = 0;

  mCacheManager = cacheManager;
  mConfig->getInt32Default("LOWI_RTS_CTS_NUM_MEAS", auxInt,
                           DEFAULT_LOWI_RTS_CTS_NUM_MEAS);
  mRtsCtsNumMeas = auxInt;
  if (0 == mRtsCtsNumMeas)
  {
    log_warning(TAG, "Unable to read parameter"
                " LOWI_RTS_CTS_NUM_MEAS from config file");
  }

  // Check to see if LOWI is running in CFR capture mode
  auxInt = 0;
  mConfig->getInt32Default("LOWI_RTT_CFR_CAPTURE_MODE", auxInt, 0);
  mCfrCaptureMode = auxInt;

  // Read the RTT threshold value from the config file
  auxInt = 0;
  config->getInt32Default("LOWI_RSSI_THRESHOLD_FOR_RTT", auxInt,
                          DEFAULT_LOWI_RSSI_THRESHOLD_FOR_RTT);
  mRssiThresholdForRtt = auxInt;
  if (0 == mRssiThresholdForRtt)
  {
    log_warning(TAG, "Unable to read parameter"
                "LOWI_RSSI_THRESHOLD_FOR_RTT from config file");
  }
  mRtsCtsTag = 0;
  supportedChannels.num_2g_ch = 0;
  supportedChannels.num_5g_ch = 0;

  // Rome by default has these scan capabilities
  mCapabilities.activeScanSupported = false;
  mCapabilities.discoveryScanSupported = true;

  mCapabilities.supportedCapablities = LOWI_RANGING_SCAN_SUPPORTED;

  mConnectedToDriver = FALSE;

  // initialize the socket used for sending the RTT requests
  RomeWipsOpen();

  // instantiate the FSM
  mLowiRomeFsm = new LOWIRomeFSM(mCfrCaptureMode, scanResultListener, cacheManager);

  if (mLowiRomeFsm == NULL)
  {
    log_warning(TAG, "LOWI Failed to allocate memory for the FSM Object");
  }
}


LOWIROMEWifiDriver::LOWIROMEWifiDriver (ConfigFile* config, LOWIScanResultReceiverListener* scanResultListener,
                                        LOWICacheManager* cacheManager)
: LOWIWifiDriverInterface (config)
{
  log_verbose (TAG, "LOWIROMEWifiDriver ()");

  int auxInt = 0;
  mReq = NULL;
  mInternalMsgId = 0;

  mCacheManager = cacheManager;
  mConfig->getInt32Default ("LOWI_RTS_CTS_NUM_MEAS", auxInt,
      DEFAULT_LOWI_RTS_CTS_NUM_MEAS);
  mRtsCtsNumMeas = auxInt;
  if (0 == mRtsCtsNumMeas)
  {
    log_warning (TAG, "Unable to read parameter"
        " LOWI_RTS_CTS_NUM_MEAS from config file");
  }

  // Check to see if LOWI is running in CFR capture mode
  auxInt = 0;
  mConfig->getInt32Default ("LOWI_RTT_CFR_CAPTURE_MODE", auxInt, 0);
  mCfrCaptureMode = auxInt;

  // Read the RTT threshold value from the config file
  auxInt = 0;
  config->getInt32Default ("LOWI_RSSI_THRESHOLD_FOR_RTT", auxInt,
      DEFAULT_LOWI_RSSI_THRESHOLD_FOR_RTT);
  mRssiThresholdForRtt = auxInt;
  if (0 == mRssiThresholdForRtt)
  {
    log_warning (TAG, "Unable to read parameter"
        "LOWI_RSSI_THRESHOLD_FOR_RTT from config file");
  }
  mRtsCtsTag = 0;
  supportedChannels.num_2g_ch = 0;
  supportedChannels.num_5g_ch = 0;

  // Rome by default has these scan capabilities
  mCapabilities.activeScanSupported = false;
  mCapabilities.discoveryScanSupported = true;

  mCapabilities.supportedCapablities = LOWI_DISCOVERY_SCAN_SUPPORTED | LOWI_RANGING_SCAN_SUPPORTED;

  mConnectedToDriver = FALSE;

  // initialize the socket used for sending the RTT requests
  RomeWipsOpen();

  // instantiate the FSM
  mLowiRomeFsm = new LOWIRomeFSM(mCfrCaptureMode, scanResultListener, cacheManager);

  if (mLowiRomeFsm == NULL)
  {
    log_warning(TAG, "LOWI Failed to allocate memory for the FSM Object");
  }
}

LOWIROMEWifiDriver::~LOWIROMEWifiDriver ()
{
  log_verbose (TAG, "~LOWIROMEWifiDriver ()");
  RomeWipsClose();
  mConnectedToDriver = FALSE;
}

void LOWIROMEWifiDriver::setNewRequest(const LOWIRequest* r, eListenMode mode)
{
  AutoLock autolock(mMutex);
  if (mode == this->RANGING_SCAN)
  {
    if (mLowiRomeFsm)
    {
      /* Pass in new Request from LOWI Controller to FSM */
      log_verbose(TAG, "%s - Passing in new Request to FSM", __FUNCTION__);
      mLowiRomeFsm->SetLOWIRequest((LOWIRangingScanRequest*)r);
    }
    else
    {
      log_debug(TAG, "Failed to pass in new Ranging request because FSM object is NULL");
    }
  }
}

int LOWIROMEWifiDriver::processPipeEvent(eListenMode mode, RomePipeEvents newEvent)
{
  log_verbose(TAG, "Processing Pipe Event, mode: %s , Event: %u",
              LOWI_TO_STRING(mode, LOWIWifiDriverInterface::modeStr), newEvent);
  AutoLock autolock(mMutex);
  if (mode == this->DISCOVERY_SCAN)
  {
    return Wips_nl_shutdown_communication();
  }
  else
  {
    if (mLowiRomeFsm)
    {
      return mLowiRomeFsm->SetNewPipeEvent(newEvent);
    }
    else
    {
      log_debug(TAG, "Failed to Process Pipe Event because FSM object is NULL");
      return -1;
    }
  }
}

int LOWIROMEWifiDriver::unBlock (eListenMode mode)
{
  return processPipeEvent(mode, ROME_NEW_REQUEST_ARRIVED);
}

int LOWIROMEWifiDriver::terminate (eListenMode mode)
{
  if (mode == REQUEST_SCAN)
  {
    // Unblock the thread
    // Thread could be blocked waiting for status response from the host driver
      return 1;
  }
  return processPipeEvent(mode, ROME_TERMINATE_THREAD);
}

int LOWIROMEWifiDriver::initFileDescriptor (eListenMode mode)
{
  log_verbose (TAG, "initFileDescriptor Mode = %d", mode);
  AutoLock autolock(mMutex);

  if (mode == DISCOVERY_SCAN)
  {
    return Wips_nl_init_pipe();
  }
  else
  {
    return RomeInitRangingPipe();
  }
}

int LOWIROMEWifiDriver::closeFileDescriptor (eListenMode mode)
{
  log_verbose (TAG, "closeFileDescriptor Mode = %d", mode);
  AutoLock autolock(mMutex);

  if (mode == DISCOVERY_SCAN)
  {
    return Wips_nl_close_pipe();
  }
  else
  {
    return RomeCloseRangingPipe();
  }
}

LOWIMeasurementResult* LOWIROMEWifiDriver::getCacheMeasurements ()
{
  log_debug (TAG, "getCacheMeasurements");

  LOWIMeasurementResult* result = NULL;
  const quipc_wlan_meas_s_type * ptr_result = NULL;
  int retVal = -1;
  int frameArrived = 0;

  ptr_result =
      lowi_proc_iwmm_req_passive_scan_with_live_meas
      (1, MAX_CACHED_SCAN_AGE_SEC, 0, SELECT_TIMEOUT_NORMAL,
          &retVal, NULL, 0, &frameArrived);

  // We do not care about the retVal in this case.
  result = new (std::nothrow) LOWIMeasurementResult;
  if (NULL == result)
  {
    log_error (TAG, "Unable to create the cached measurement results");
  }
  else
  {
    result->request = NULL;
    if (NULL != ptr_result)
    {
      log_verbose (TAG, "getCacheMeasurements () - injecting the results"
          " in vector");
      vector <const quipc_wlan_meas_s_type *> result_vector;
      result_vector.push_back(ptr_result);
      LOWIWifiDriverUtils::injectScanMeasurements
      (result, result_vector, mRssiThresholdForRtt);

      log_verbose (TAG, "getCacheMeasurements () - free the measurements");
      lowi_free_meas_result ((quipc_wlan_meas_s_type *)ptr_result);
    }
    else
    {
      // Initialize the result for no measurements case
      log_info (TAG, "getCacheMeasurements () - No measurements found");

      result->measurementTimestamp = LOWIUtils::currentTimeMs();

      result->scanStatus = LOWIResponse::SCAN_STATUS_SUCCESS;

      // It's probably ok to have it as unknown since this is for cache
      result->scanType = LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_UNKNOWN;
    }
  }

  return result;
}

LOWICapabilities LOWIROMEWifiDriver::getCapabilities ()
{
  if (mLowiRomeFsm)
  {
    LOWI_RangingCapabilities lowiRangingCap;
    mCapabilities.rangingScanSupported = false;
    lowiRangingCap = mLowiRomeFsm->GetRangingCap();
    mCapabilities.oneSidedRangingSupported = lowiRangingCap.oneSidedSupported;
    mCapabilities.dualSidedRangingSupported11mc = lowiRangingCap.dualSidedSupported11mc;
    mCapabilities.dualSidedRangingSupported11v = lowiRangingCap.dualSidedSupported11v;
    mCapabilities.bwSupport = lowiRangingCap.bwSupport;
    mCapabilities.preambleSupport = lowiRangingCap.preambleSupport;
    if (mCapabilities.oneSidedRangingSupported ||
        mCapabilities.dualSidedRangingSupported11mc ||
        mCapabilities.dualSidedRangingSupported11v)
    {
      mCapabilities.rangingScanSupported = true;
    }
  }
  return mCapabilities;
}

uint8* LOWIROMEWifiDriver::parseNeighborReport(uint8 &elemLen, uint8* frameBody, FineTimingMeasRangeReq &rangeReq)
{
  log_verbose(TAG, "%s", __FUNCTION__);
  if (elemLen < NR_ELEM_MIN_LEN ||
      frameBody == NULL)
  {
    log_debug(TAG, "%s - Aborting Neighbor report parsing because Element length less than %u",
              __FUNCTION__, NR_ELEM_MIN_LEN);
    return NULL;
  }

  NeighborRprtElem neighborRprtElem;
  uint8 elemId = *frameBody++;
  uint8 len    = *frameBody++;

  log_debug(TAG, "%s - parentElemLen %u, elemId: %u, len: %u", __FUNCTION__, elemLen, elemId, len);

  /** Account for the length of this element. This is done so that
   *  the caller can keep track of how much data is yet to be
   *  parsed */
  if (elemLen >= (len + NR_ELEM_HDR_LEN))
  {
    elemLen -= (len + NR_ELEM_HDR_LEN);
    if (elemId == RM_NEIGHBOR_RPT_ELEM_ID)
    {
      log_verbose(TAG, "%s - Going to parse NR Element", __FUNCTION__);
    }
    else
    {
      log_debug(TAG, "%s - %u is not a NR Element - Not Parsing", __FUNCTION__, elemId);
      frameBody += len;
      return frameBody;
    }
  }
  else
  {
    log_debug(TAG, "%s - Bad Element Length: %u - Not Going to Parse Element", __FUNCTION__, elemLen);
    elemLen = 0;
    return NULL;
  }

  memcpy(neighborRprtElem.bssid, frameBody, BSSID_SIZE);
  frameBody   += BSSID_SIZE;
  memcpy(neighborRprtElem.bssidInfo, frameBody, BSSID_INFO_LEN);
  frameBody   += BSSID_INFO_LEN;
  neighborRprtElem.operatingClass = *frameBody++;
  neighborRprtElem.channelNumber  = *frameBody++;
  neighborRprtElem.phyType        = *frameBody++;

  log_debug(TAG, "%s - Neighbor Report Element - elemId: %u, len: %u, BSSID: " QUIPC_MACADDR_FMT
              ", Bssid-Info: 0x%02x%02x%02x%02x, operatingClass: %u, channelNumber: %u, phyType: %u",
              __FUNCTION__,
              elemId,
              len,
              QUIPC_MACADDR(neighborRprtElem.bssid),
              neighborRprtElem.bssidInfo[3],
              neighborRprtElem.bssidInfo[2],
              neighborRprtElem.bssidInfo[1],
              neighborRprtElem.bssidInfo[0],
              neighborRprtElem.operatingClass,
              neighborRprtElem.channelNumber,
              neighborRprtElem.phyType);

  /** The basic Neighbor report Elemnt is NR_ELEM_MIN_LEN(13)
   *  bytes long. These bytes have been parsed above. All
   *  additional bytes will ignored for the moment.
   */
  if (len > NR_ELEM_MIN_LEN)
  {
    log_debug(TAG, "%s - Ignore additional bytes in Neighbor Report Element: %u",
              __FUNCTION__,
              (len - NR_ELEM_MIN_LEN));
    frameBody += (len - NR_ELEM_MIN_LEN);
  }

  rangeReq.neighborRprtElem.push_back(neighborRprtElem);

  return frameBody;
}
uint8* LOWIROMEWifiDriver::parseMeasReqElem(uint32 &currentFrameLen,
                                            uint8* frameBody,
                                            uint8 dialogTok,
                                            uint8 sourceMac[BSSID_SIZE],
                                            uint8 staMac[BSSID_SIZE],
                                            uint32 freq)
{

  log_verbose(TAG, "%s", __FUNCTION__);
  if (currentFrameLen > 1)
  {
    uint8 elemId  = *frameBody++;
    uint8 elemLen = *frameBody++;
    currentFrameLen -=2;
    uint8* measReqElemBody = frameBody;

    if (elemLen > currentFrameLen ||
        elemLen < 3)
    {
      log_debug(TAG, "%s - recieved a bad elementh length - aborting parsing RM frame: elem Len: %u, frame Len: %u",
               __FUNCTION__,
               elemLen,
               currentFrameLen);
      return NULL;
    }
    else
    {
      /* Move Frame Body pointer to the next element */
      frameBody += elemLen;
      currentFrameLen -= elemLen;
    }

    if (elemId != RM_MEAS_REQ_ELEM_ID)
    {
      log_info(TAG, "%s - Skipping element that is not a Measurement Request Element: E-ID: %u",
               __FUNCTION__, elemId);
      return frameBody;
    }

    uint8 measTok     = *measReqElemBody++;
    uint8 measReqMode = *measReqElemBody++;
    uint8 measType    = *measReqElemBody++;
    elemLen          -= 3;

    log_verbose(TAG, "%s - Measurement Request Header - ElemId: %u, MeasToken: %u, MeasReqMode: %u, MeasType: %u",
                __FUNCTION__,
                elemId,
                measTok,
                measReqMode,
                measType);

    if (elemLen == 0 ||
        measType != LOWI_WLAN_FTM_RANGE_REQ_TYPE)
    {
      /* reached end of Measurement Request Element */
      log_debug(TAG, "%s - Reached end of element prematurely! - aborting", __FUNCTION__);
      return frameBody;
    }

    FineTimingMeasRangeReq rangeReq;
    rangeReq.dialogTok = dialogTok;
    rangeReq.measReqElem.measTok = measTok;
    rangeReq.measReqElem.measReqMode = measReqMode;
    rangeReq.measReqElem.measType = measType;

    /* Start Parsing Fine Timing Measurement Range Request Element */
    FtmrReqHead fmtrReqHead;
    fmtrReqHead.randomInterval = ((measReqElemBody[0] << 8) | (measReqElemBody[1]));
    measReqElemBody += MEAS_REQ_ELEM_HDR_LEN;
    fmtrReqHead.minApCount = *measReqElemBody++;
    elemLen         -= 3;

    rangeReq.ftmrrReqHead = fmtrReqHead;

    log_verbose(TAG, "%s - Fine Timing Measurement Range Request Header - randomInterval: %u, minApCount: %u",
                __FUNCTION__,
                fmtrReqHead.randomInterval,
                fmtrReqHead.minApCount);

    if (!elemLen)
    {
      /* Bad Element - no neighbor report elements */
      log_debug(TAG, " %s - Received FTM Range request with no elements, aborting!",
                __FUNCTION__);
      return NULL;
    }

    while (elemLen)
    {
      measReqElemBody = parseNeighborReport(elemLen, measReqElemBody, rangeReq);
      if (measReqElemBody == NULL)
      {
        break;
      }
    }
    if (rangeReq.neighborRprtElem.getNumOfElements())
    {
      /* Create LOWI Request and send out to LOWI controller */
      vector <LOWIPeriodicNodeInfo> nodes;
      log_verbose(TAG, "%s - Number of APs to Range with: %u Ranging shall be performed with the following APs:",
                  __FUNCTION__,
                  rangeReq.neighborRprtElem.getNumOfElements());
      for (unsigned int i = 0 ; i < rangeReq.neighborRprtElem.getNumOfElements() ; i++)
      {
        LOWIPeriodicNodeInfo node;
        NeighborRprtElem nbrElem = rangeReq.neighborRprtElem[i];
        node.bssid = LOWIMacAddress(nbrElem.bssid);
        node.bssid.print();
        nodes.push_back(node);
      }

      /* Using dummy message Id for now... since we are not expecting a response for this message
         The FTMR report is another request from the contrller to send out FTMR Report*/
      mInternalMsgId++;
      log_verbose(TAG, "%s - Sending Internal FTMRR to LOWI controller with %u Aps and msg ID: %u",
                  __FUNCTION__,
                  rangeReq.neighborRprtElem.getNumOfElements(),
                  mInternalMsgId);
      LOWIMacAddress mac = LOWIMacAddress(sourceMac);
      LOWIMacAddress selfMac = LOWIMacAddress(staMac);
      LOWIFTMRangeReqMessage *lowiFtmrMessage = new LOWIFTMRangeReqMessage(mInternalMsgId, nodes, mac, selfMac, freq, rangeReq.dialogTok, rangeReq.measReqElem.measTok);
      mInternalMessageListener->internalMessageReceived(lowiFtmrMessage);
    }
    else
    {
      log_verbose(TAG, "%s - No Ranging shall be performed because there are no APs in the FTMRR",
                  __FUNCTION__);
    }
  }
  else
  {
    return NULL;
  }

  return frameBody;
}

void LOWIROMEWifiDriver::processWlanFrame()
{
  char tempBuff[5000];
  log_verbose(TAG, "%s", __FUNCTION__);
  for (int i = 0; i < wlanFrameStore.numFrames; ++i)
  {
    uint8 sourceMac[BSSID_SIZE];
    uint8 staMac[BSSID_SIZE];
    uint32 freq = wlanFrameStore.wlanFrames[i].freq;
    uint8 frameLen = wlanFrameStore.wlanFrames[i].frameLen;
    uint8 *frame = wlanFrameStore.wlanFrames[i].frameBody;
    Wlan80211FrameHeader *wlanFrameHeader = (Wlan80211FrameHeader*) frame;
    uint8 *frameBody = frame + sizeof(Wlan80211FrameHeader);

    int l = 0;
    for (unsigned int i = 0; i < frameLen; i++)
    {
      l+=snprintf(tempBuff+l, 10, "0x%02x ", frameBody[i]);
    }

    uint8 actCatagory = *frameBody++;
    uint8 frameBodyLen = frameLen - sizeof(Wlan80211FrameHeader) - 1;

    log_verbose(TAG, "%s - Frame Header - frameControl:0x%x durationId: 0x%x addr1:" QUIPC_MACADDR_FMT " addr2:" QUIPC_MACADDR_FMT " addr3:" QUIPC_MACADDR_FMT " SeqControl: 0x%x",
                __FUNCTION__,
                wlanFrameHeader->frameControl,
                wlanFrameHeader->durationId,
                QUIPC_MACADDR(wlanFrameHeader->addr1),
                QUIPC_MACADDR(wlanFrameHeader->addr2),
                QUIPC_MACADDR(wlanFrameHeader->addr3),
                wlanFrameHeader->seqCtrl);

    memcpy(sourceMac, wlanFrameHeader->addr2, BSSID_SIZE);
    memcpy(staMac, wlanFrameHeader->addr1, BSSID_SIZE);

    log_verbose(TAG, "%s - Received Action Frame: Source Addr: " QUIPC_MACADDR_FMT " Catagory: %u Data Len: %u",
                __FUNCTION__,
                QUIPC_MACADDR(wlanFrameHeader->addr2),
                actCatagory,
                frameBodyLen);

    log_verbose(TAG, "%s - Frame: %s", __FUNCTION__, tempBuff);

    uint8 rmAction  = *frameBody++;
    uint8 dialogTok = *frameBody++;
    uint16 numRep   = ((frameBody[0] << 8) | (frameBody[1]));
    frameBody += 2;
    uint8 *measReqElem = frameBody;
    uint32 currentFrameLen = frameBodyLen - 4;


    if (actCatagory != LOWI_WLAN_ACTION_RADIO_MEAS ||
        rmAction    != LOWI_RM_ACTION_REQ)
    {
      log_debug(TAG, "%s - Received a Non Radio Measurement Request Frame - skipping this frame", __FUNCTION__);
      continue;
    }
    else
    {
      log_debug(TAG, "%s - Received a Radio Measurement Request Frame: Action-Cat: %u, Action Type: %u, dialogTok: %u, numRep: %u",
                __FUNCTION__,
                actCatagory,
                rmAction,
                dialogTok,
                numRep);
    }

    while(currentFrameLen)
    {
      frameBody = parseMeasReqElem(currentFrameLen, frameBody, dialogTok, sourceMac, staMac, freq);
      if (frameBody == NULL)
      {
        break;
      }
    }
  }
}

void LOWIROMEWifiDriver::sendNeighborRprtReq()
{
  uint8 frameBody[2048];
  char frameChar[2048];
  int l = 0;
  uint32 i = 0, frameBodyLen = 0;
  uint8 destMac[BSSID_SIZE];
  uint8 staMac[BSSID_SIZE];
  uint32 freq;
  LOWIScanMeasurement associatedApMeas;
  LOWIMacAddress localStaMac;

  if (mCacheManager && mCacheManager->getAssociatedAP(associatedApMeas) == false)
  {
    log_debug(TAG, "%s - Not associated to any AP so cannot request Neighbor Report - Aborting",
              __FUNCTION__);
    return;
  }
  else
  {
    log_debug(TAG, "%s - Associated to AP: " QUIPC_MACADDR_FMT " requesting Neighbor Report",
                __FUNCTION__, QUIPC_MACADDR(associatedApMeas.bssid));
  }

  freq = associatedApMeas.frequency;

  for (i = 0; i < BSSID_SIZE; i++)
  {
    destMac[i] = associatedApMeas.bssid[i];
  }
  log_debug(TAG, "%s - The Destination MAC address for the NR request is: " QUIPC_MACADDR_FMT,
            __FUNCTION__, QUIPC_MACADDR(destMac));

  if (mCacheManager &&
      mCacheManager->getStaMacInCache(localStaMac) == false)
  {
    log_debug(TAG, "%s - Failed to request Local STA MAC so cannot request Neighbor Report - Aborting",
              __FUNCTION__);
    return;
  }
  else
  {
    log_debug(TAG, "%s - Local STA MAC from cache: " QUIPC_MACADDR_FMT,
                __FUNCTION__, QUIPC_MACADDR(localStaMac));
    for (i = 0; i < BSSID_SIZE; i++)
    {
      staMac[i] = localStaMac[i];
    }
    log_error(TAG, "%s - The Local STA MAC address for the NR request is: " QUIPC_MACADDR_FMT,
              __FUNCTION__, QUIPC_MACADDR(staMac));
  }

  NeighborRequestElem nrReqElem;
  MeasReqElem measReqElemlci;
  MeasReqElem measReqElemlcr;
  LciElemCom lciElemCom;
  LocCivElemCom locCivElemCom;

  nrReqElem.catagory = LOWI_WLAN_ACTION_RADIO_MEAS;
  nrReqElem.radioMeasAction = LOWI_NR_ACTION_REQ;
  nrReqElem.dialogTok = gDialogTok++;
  if (gDialogTok == 0)
  {
    /* Dialog Token shall always be a non zero number so increment again*/
    gDialogTok++;
  }

  measReqElemlci.elementId = RM_MEAS_REQ_ELEM_ID;
  measReqElemlci.len = 3 + sizeof(lciElemCom);
  measReqElemlci.measTok = gMeasTok++;
  if (gMeasTok == 0)
  {
    /* Measurement Token shall always be a non zero number so increment again*/
    gMeasTok++;
  }
  measReqElemlci.measReqMode = 0; /* Reserved */
  measReqElemlci.measType = LOWI_WLAN_LCI_REQ_TYPE;

  measReqElemlcr.elementId = RM_MEAS_REQ_ELEM_ID;
  measReqElemlcr.len = 3 + sizeof(locCivElemCom);
  measReqElemlcr.measTok = gMeasTok++;
  if (gMeasTok == 0)
  {
    /* Measurement Token shall always be a non zero number so increment again*/
    gMeasTok++;
  }
  measReqElemlcr.measReqMode = 0; /* Reserved */
  measReqElemlcr.measType = LOWI_WLAN_LOC_CIVIC_REQ_TYPE;

  lciElemCom.locSubject = LOWI_LOC_SUBJECT_REMOTE;

  locCivElemCom.locSubject = LOWI_LOC_SUBJECT_REMOTE;
  locCivElemCom.civicType = 0; /*IETF RFC format*/
  locCivElemCom.locServiceIntUnits = 0; /* Seconds units */
  locCivElemCom.locServiceInterval = 0; /* 0 Seconds */

  memset(frameBody, 0, sizeof(frameBody));

  /* Construct the Neighbor Request Frame header */
  memcpy(frameBody, &nrReqElem, sizeof(nrReqElem));
  frameBodyLen += sizeof(nrReqElem);

  /* Construct the Measurement Elements */
  /* Construct LCI Measurement Element */
  memcpy((frameBody + frameBodyLen), &measReqElemlci, sizeof(measReqElemlci));
  frameBodyLen += sizeof(measReqElemlci);
  /* LCI Element */
  memcpy((frameBody + frameBodyLen), &lciElemCom, sizeof(lciElemCom));
  frameBodyLen += sizeof(lciElemCom);
  /* Construct Location Civic Measurement Element */
  memcpy((frameBody + frameBodyLen), &measReqElemlcr, sizeof(measReqElemlcr));
  frameBodyLen += sizeof(measReqElemlcr);
  /* Location Civic Element */
  memcpy((frameBody + frameBodyLen), &locCivElemCom, sizeof(locCivElemCom));
  frameBodyLen += sizeof(locCivElemCom);

  for (i = 0; i < frameBodyLen; i++)
  {
    l+=snprintf(frameChar+l, 10, "0x%02x ", frameBody[i]);
  }

  log_info(TAG, "%s - FrameBody: %s", __FUNCTION__, frameChar);

  lowi_send_action_frame(frameBody, frameBodyLen, freq, destMac, staMac);
}

void LOWIROMEWifiDriver::SendFTMRRep(LOWIFTMRangeRprtMessage* req)
{
  uint8 frameBody[2048];
  char frameChar[2048];
  int l = 0;
  uint8 sourceMac[BSSID_SIZE];
  uint8 staMac[BSSID_SIZE];
  uint32 freq;
  LOWIMacAddress mac = req->getRequesterBssid();
  LOWIMacAddress selfMac = req->getSelfBssid();

  LOWIScanMeasurement scanMeasurement;
  uint32 i = 0, frameBodyLen = 0;
  if (req == NULL)
  {
    log_info(TAG, "%s - Received an null pointer for the request - Aborting", __FUNCTION__);
    return;
  }

  freq = req->getFrequency();
  if (!freq)
  {
    if (mCacheManager &&
        mCacheManager->getFromCache(mac, scanMeasurement) == true)
    {
      freq = scanMeasurement.frequency;
      log_verbose(TAG, "%s: Found BSSID in cache and retreived frequency: %u", __FUNCTION__, freq);
    }
    else
    {
      log_verbose(TAG, "%s: Didnt Find Target BSSID in cache - aborting sending Action frame out",
                  __FUNCTION__);
      return;
    }
  }

  vector <LOWIRangeEntry> rangeEntries = req->getSuccessNodes();
  vector <LOWIErrEntry>   errEntries   = req->getErrNodes();

  uint8 rangeEntryCount = rangeEntries.getNumOfElements();
  uint8 errEntryCount   = errEntries.getNumOfElements();

  memset(frameBody, 0, sizeof(frameBody));

  /* Construct the Radio Measurement Frame header */
  /* Add Catagory to the frame */
  frameBody[frameBodyLen++] = LOWI_WLAN_ACTION_RADIO_MEAS;
  /* Radio Measurement Action Type*/
  frameBody[frameBodyLen++] = LOWI_RM_ACTION_RPT;
  /* Dialog Token */
  frameBody[frameBodyLen++] = (uint8) req->getDiagToken();

  /* Construct the Measurement Elements */
  /* Add Element ID*/
  frameBody[frameBodyLen++] = RM_MEAS_RPT_ELEM_ID;
  /* Get Ptr to the Length of the Report Element, it shall be filled in later */
  uint8 *measRptLenField = &frameBody[frameBodyLen++];
  uint8 measRptLen = 0;
  /* Add Measurement Token */
  frameBody[frameBodyLen++] = (uint8) req->getMeasToken();
  /* Add Measurement Report Mode - all zeros */
  frameBody[frameBodyLen++] = 0;
  /* Add Measurement Type */
  frameBody[frameBodyLen++] = LOWI_WLAN_FTM_RANGE_REQ_TYPE;

  /* Keep track of the length of the Measurement Report Element */
  measRptLen += 3;

  /* Construct the Fine Timing Range Report Body */
  /* Add Range Entry Count */
  frameBody[frameBodyLen++] = rangeEntryCount;
  /* Add Range Entries */
  if (rangeEntryCount)
  {
    FtmrrRangeEntry* rangeEntElems = (FtmrrRangeEntry*) &frameBody[frameBodyLen];
    for (uint32 i = 0; i < rangeEntryCount; i++)
    {
      FtmrrRangeEntry rangeEntry;

      memset(&rangeEntry, 0, sizeof(rangeEntry));
      /* Measurement Start Time */
      rangeEntry.measStartTime = rangeEntries[i].measStartTime;
      /* BSSID */
      for (int j = 0; j < BSSID_SIZE; j++)
      {
        rangeEntry.bssid[j] = rangeEntries[i].bssid[j];
      }
      /* Range */
      rangeEntry.range = rangeEntries[i].range;
      /* Max Range Error */
      rangeEntry.maxErrRange = rangeEntries[i].maxErrRange;

      log_verbose(TAG, "%s: Range Entry[%d] BSSID: " QUIPC_MACADDR_FMT
                  " Starttime:%u range:%u maxErr:%u", __FUNCTION__, i,
                  QUIPC_MACADDR(rangeEntries[i].bssid), rangeEntries[i].measStartTime,
                  rangeEntries[i].range, rangeEntries[i].maxErrRange);
      memcpy(rangeEntElems, &rangeEntry, sizeof(FtmrrRangeEntry));
      rangeEntElems++;
      frameBodyLen += sizeof(FtmrrRangeEntry);
    }
  }
  /* Add Range Error Count */
  frameBody[frameBodyLen++] = errEntryCount;
  /* Add Error Entries */
  if (errEntryCount)
  {
    FtmrrErrEntry* errEntElems = (FtmrrErrEntry*) &frameBody[frameBodyLen];
    for (uint32 i = 0; i < errEntryCount; i++)
    {
      FtmrrErrEntry errEntry;
      /* Measurement Start Time */
      log_verbose(TAG, "%s - errEntries[%d].measStartTime: %u",
                  __FUNCTION__, i, errEntries[i].measStartTime);
      errEntry.measStartTime = errEntries[i].measStartTime;
      /* BSSID */
      log_verbose(TAG, "%s -  Error Entry[%d] - BSSID: " QUIPC_MACADDR_FMT,
                  __FUNCTION__, i, QUIPC_MACADDR(errEntries[i].bssid));
      for (int j = 0; j < BSSID_SIZE; j++)
      {
        errEntry.bssid[j] = errEntries[i].bssid[j];
      }
      /* Error Code */
      log_verbose(TAG, "%s - errEntries[%d].errCode: %u",
                  __FUNCTION__, i, errEntries[i].errCode);
      errEntry.errCode = errEntries[i].errCode;

      memcpy(errEntElems, &errEntry, sizeof(FtmrrErrEntry));
      errEntElems++;
      frameBodyLen += sizeof(FtmrrErrEntry);
    }
  }
  /* Keep track of the length of the Measurement Report Element */
  measRptLen += 1 + (sizeof(FtmrrRangeEntry) * rangeEntryCount) + 1 + (sizeof(FtmrrErrEntry) * errEntryCount);
  /* Update Measurement Report Length Field */
  *measRptLenField = measRptLen;

  log_error(TAG, "%s - The Destination MAC address is: " QUIPC_MACADDR_FMT,
            __FUNCTION__, QUIPC_MACADDR(mac));
  for (int i = 0; i < BSSID_SIZE; i++)
  {
    sourceMac[i] = mac[i];
  }

  log_error(TAG, "%s - The Self MAC address is: " QUIPC_MACADDR_FMT,
            __FUNCTION__, QUIPC_MACADDR(selfMac));
  for (int i = 0; i < BSSID_SIZE; i++)
  {
    staMac[i] = selfMac[i];
  }

  for (i = 0; i < frameBodyLen; i++)
  {
    l+=snprintf(frameChar+l, 10, "0x%02x ", frameBody[i]);
  }

  log_info(TAG, "%s - FrameBody: %s", __FUNCTION__, frameChar);

  lowi_send_action_frame(frameBody, frameBodyLen, freq, sourceMac, staMac);
}

LOWIMeasurementResult* LOWIROMEWifiDriver::getMeasurements
(LOWIRequest* r, eListenMode mode)
{
  LOWIMeasurementResult* result = NULL;
  log_verbose (TAG, "getMeasurements");

  vector <const quipc_wlan_meas_s_type *> result_vector;
  const quipc_wlan_meas_s_type * ptr_result = NULL;
  int retVal = -1001;
  int frameArrived = 0;

  if (LOWIWifiDriverInterface::DISCOVERY_SCAN == mode)
  {
    // Check the type of request and issue the request to the wifi driver accordingly
    if (NULL == r)
    {
      /////////////////////////////
      // Passive listening request
      /////////////////////////////
      log_info (TAG, "getMeasurements - Passive listening mode");

      // Perform passive scan by making the request to the NL layer
      ptr_result = lowi_proc_iwmm_req_passive_scan_with_live_meas(
                   0, 0, 0, SELECT_TIMEOUT_NEVER, &retVal, NULL, 0, &frameArrived);
      if (NULL != ptr_result)
      {
        log_debug(TAG, "Added result, retVal = %d", retVal);
        result_vector.push_back(ptr_result);
      }
    }
    else if(LOWIRequest::DISCOVERY_SCAN == r->getRequestType())
    {
      // Issue passive / active scan request to the wifi driver
      log_info (TAG, "getMeasurements - Discovery scan request");

      if (false == mCapabilities.discoveryScanSupported)
      {
        log_error (TAG, "getMeasurements - Discovery scan request. Not supported");
        retVal = -1002;
      }
      else
      {
        LOWIDiscoveryScanRequest* req = (LOWIDiscoveryScanRequest*) r;

        bool scan_type_changed = false;

        // Check the scan type and request the driver
        if (LOWIDiscoveryScanRequest::PASSIVE_SCAN == req->getScanType())
        {
          log_debug (TAG, "True Passive scan requested."
                          " Send request to driver");
          scan_type_changed = LOWIWifiDriverUtils::setDiscoveryScanType(
                              LOWIDiscoveryScanRequest::PASSIVE_SCAN);
        }

        // Check the Channels on which the scan is to be performed
        vector <LOWIChannelInfo> chanVector = req->getChannels();

        int * channelsToScan = NULL;
        unsigned char numChannels = 0;
        if (0 == chanVector.getNumOfElements())
        {
          // Request is to perform a scan on a band
          // Step 1 : Check if we have a supported channel list from kernel
          if (0 == supportedChannels.num_2g_ch &&
              0 == supportedChannels.num_5g_ch)
          {
            // We do not have the supported channel list yet.
            int err = WipsGetSupportedChannels (&supportedChannels);
            if (0 != err)
            {
              log_error (TAG, "getMeasurements - Unable to get the"
                  " supported channel list");
            }
          }

          // Step 2 : Get the final list of frequencies based on what's supported
          // by the kernel if it is available.
          // Use the default set of frequencies if the supported list is not
          // available
          channelsToScan = LOWIWifiDriverUtils::getSupportedFreqs(req->getBand(),
                                                                  &supportedChannels,
                                                                  numChannels);
        }
        else
        {
          channelsToScan = LOWIUtils::getChannelsOrFreqs (chanVector, numChannels, true);
        }

        uint32 max_scan_age_sec = (uint32) req->getFallbackToleranceSec();
        uint32 max_meas_age_sec = (uint32) req->getMeasAgeFilterSec();
        int cached = 0;
        if (req->getRequestMode() == LOWIDiscoveryScanRequest::CACHE_FALLBACK)
        {
          // If it is a cache fallback mode, get the cached measurements
          // from the wifi driver
          cached = 1;
        }

        // make the request to the NL layer
        ptr_result = lowi_proc_iwmm_req_passive_scan_with_live_meas (
                     cached, max_scan_age_sec, max_meas_age_sec,
                     SELECT_TIMEOUT_NORMAL, &retVal, channelsToScan, numChannels, &frameArrived);
        if (NULL != ptr_result)
        {
          result_vector.push_back(ptr_result);
        }
        delete [] channelsToScan;

        // Check if the request was considered invalid
        if (-EINVAL == retVal)
        {
          log_error (TAG, "getMeasurements - Discovery scan request was invalid");
          if (0 == chanVector.getNumOfElements())
          {
            // It was a request to perform a scan on a band
            if (0 == supportedChannels.num_2g_ch &&
                0 == supportedChannels.num_5g_ch)
            {
              // The supported channel list was not available
              // We might have done the scan on default channels
              // The EINVAL is expected in this case.
              // We will continue to try to get the supported channel list
              // in subsequent requests.
              log_debug (TAG, "getMeasurements - Discovery scan request"
                  " the supported channel list was never found");
            }
            else
            {
              // We had the supported channel list but still the request was
              // considered invalid. It's time to fetch the supported channel
              // list again. This might be because of the regulatory domain
              // change. The supported channel list will be fetched in subsequent
              // request for band scan. We will just make the list dirty now.
              supportedChannels.num_2g_ch = 0;
              supportedChannels.num_5g_ch = 0;
            }
          }
          // Restore the scan type if it was changed
          if (true == scan_type_changed)
          {
            bool status = LOWIWifiDriverUtils::setDiscoveryScanType(
                                               LOWIDiscoveryScanRequest::ACTIVE_SCAN);
            log_debug (TAG, "Restored the scan type = %d", status);
            scan_type_changed = false;
          }
        }
      }
    } // else if(DISCOVERY_SCAN == r->getRequestType())
    else if (LOWIRequest::LOWI_INTERNAL_MESSAGE == r->getRequestType())
    {
      LOWIInternalMessage *intreq = (LOWIInternalMessage*)r;
      if (LOWIInternalMessage::LOWI_IMSG_FTM_RANGE_RPRT == intreq->getInternalMessageType())
      {
        log_verbose(TAG, "%s - Sending out Fine Timing Measurement Range Report",__FUNCTION__);
        LOWIFTMRangeRprtMessage* req = (LOWIFTMRangeRprtMessage*) r;

        SendFTMRRep(req);
      }
    }
    else if (LOWIRequest::NEIGHBOR_REPORT == r->getRequestType())
    {
      log_verbose(TAG, "%s - Sending out Neighbor Report Request", __FUNCTION__);
      sendNeighborRprtReq();
    }

    /** Process any Action Frames that have arrived over the
     *  Generic Netlink socket */
    if (frameArrived)
    {
      log_verbose(TAG, "%s - Action Frames arrived over Genl Socket - Processing them",
                  __FUNCTION__);
      processWlanFrame();
    }
    else
    {
      log_verbose(TAG, "%s - No Action Frames arrived over Genl Socket", __FUNCTION__);
    }
  }

  // REQUEST_SCAN thread handles scan requests of certain types
  if(LOWIWifiDriverInterface::REQUEST_SCAN == mode)
  {

  }

  /** Ranging FSM */
  if (LOWIWifiDriverInterface::RANGING_SCAN == mode)
  {
    /** This is a blocking Call to the FSM. The FSM will return
     *  when there is a terminate Thread request */
    if(mLowiRomeFsm->FSM() != 0)
    {
      log_debug(TAG, "FSM returned an non zero value indicating something bad happened");
    }
    /* Reset the Request and Result Vector Pointer in the FSM */
    mLowiRomeFsm->SetLOWIRequest(NULL);
  }
  /*************/

  if (LOWIWifiDriverInterface::RANGING_SCAN != mode)
  {

    log_verbose (TAG, "getMeasurements () - Request type: %u analyzing the results - %d", (r) ? r->getRequestType() : 2000, retVal);
    result = NULL;

    if (r != NULL &&
        LOWIRequest::LOWI_INTERNAL_MESSAGE == r->getRequestType())
    {
      LOWIInternalMessage *intreq = (LOWIInternalMessage*)r;
      if (LOWIInternalMessage::LOWI_IMSG_FTM_RANGE_RPRT == intreq->getInternalMessageType())
      {
        log_verbose(TAG,"%s - Finished processing LOWI Internal Message returning to Passive Listennig mode", __FUNCTION__);
        result = NULL;
      }
    }
    else if (r != NULL &&
             LOWIRequest::NEIGHBOR_REPORT == r->getRequestType())
    {
      log_verbose(TAG,"%s - Finished processing Neighbvor Report Request returning to Passive Listennig mode", __FUNCTION__);
      result = new (std::nothrow) LOWIMeasurementResult;
      if (NULL == result)
      {
        log_error (TAG, "Unable to create the measurement result");
      }
      else
      {
        result->request = r;
        result->scanStatus = LOWIResponse::SCAN_STATUS_SUCCESS;
      }
    }
    else
    {
      if (ERR_SELECT_TERMINATED == retVal)
      {
        // No need to provide any results as the scan was terminated
        log_info (TAG, "Scan terminated");

        for (uint32 ii = 0; ii < result_vector.getNumOfElements(); ++ii)
        {
          lowi_free_meas_result ((quipc_wlan_meas_s_type *)result_vector [ii]);
        }

      }
      else
      {
        // As long as the scan was not terminated let's provide
        // the results even if we may have found NULL results
        log_verbose (TAG, "getMeasurements () - parsing the results");
        // Parse the result and update the vector
        result = new (std::nothrow) LOWIMeasurementResult;
        if (NULL == result)
        {
          log_error (TAG, "Unable to create the measurement result");
        }
        else
        {
          result->request = r;
          if (0 != result_vector.getNumOfElements())
          {
          log_debug (TAG, "getMeasurements () - injecting %d results"
                     " in vector", result_vector.getNumOfElements());
            LOWIWifiDriverUtils::injectScanMeasurements
            (result, result_vector, mRssiThresholdForRtt);

            log_verbose (TAG, "getMeasurements () - free the measurements");
            for (unsigned int ii = 0; ii < result_vector.getNumOfElements(); ++ii)
            {
              lowi_free_meas_result ((quipc_wlan_meas_s_type *)result_vector [ii]);
            }
          }
          else
          {
            // Initialize the result for no measurements case
            log_debug (TAG, "getMeasurements () - No measurements found");
            result->measurementTimestamp = LOWIUtils::currentTimeMs();
            result->scanStatus = LOWIResponse::SCAN_STATUS_SUCCESS;
            if (-EINVAL == retVal)
            {
              log_debug (TAG, "getMeasurements () - No measurements found"
                              " - Request invalid");
              result->scanStatus = LOWIResponse::SCAN_STATUS_INVALID_REQ;
            }
            else if (ERR_SELECT_TIMEOUT == retVal)
            {
              // Driver timed out
              result->scanStatus = LOWIResponse::SCAN_STATUS_DRIVER_TIMEOUT;
            }
            else if (-1002 == retVal)
            {
              log_debug (TAG, "getMeasurements () - Request Not supported");
              result->scanStatus = LOWIResponse::SCAN_STATUS_NOT_SUPPORTED;
            }
            else if (-1003 == retVal)
            {
              log_debug (TAG, "getMeasurements () - Internal Error");
              result->scanStatus = LOWIResponse::SCAN_STATUS_INTERNAL_ERROR;
            }
            else if (0 != retVal)
            {
              log_debug (TAG, "getMeasurements () - No measurements found"
                  " Err = %d", retVal);
              result->scanStatus = LOWIResponse::SCAN_STATUS_DRIVER_ERROR;
            }

            // This drive supports only passive scan as of now
            result->scanType = LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_PASSIVE;
            if ( (NULL != ptr_result) &&
                 (QUIPC_WLAN_SCAN_TYPE_PASSIVE != ptr_result->e_scan_type) )
            {
              result->scanType =
                  LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_UNKNOWN;
            }
          }
        }
      }
    }
  }

  return result;
}

