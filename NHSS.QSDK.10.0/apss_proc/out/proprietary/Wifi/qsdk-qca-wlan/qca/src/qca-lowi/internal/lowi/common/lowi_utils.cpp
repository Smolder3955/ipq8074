/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Utils

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Utils

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#define __STDC_FORMAT_MACROS
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/time.h>
#include <base_util/log.h>

#include <base_util/postcard.h>
#include <inc/lowi_const.h>
#include <common/lowi_utils.h>
#include <inc/lowi_client.h>
#include <inc/lowi_client_receiver.h>
#include <inc/lowi_scan_measurement.h>

using namespace qc_loc_fw;


const int channelArr_2_4_ghz[] =
{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};

const int freqArr_2_4_ghz[] =
{2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447,
  2452, 2457, 2462, 2467, 2472, 2484};

const int channelArr_5_ghz[] =
{36, 40, 44, 48, 52, 56, 60, 64, 100, 104, 108,
  112, 116, 132, 136, 140, 149, 153, 157, 161, 165};

const int freqArr_5_ghz[] =
{5180, 5200, 5220, 5240, 5260, 5280, 5300, 5320, 5500, 5520, 5540,
  5560, 5580, 5660, 5680, 5700, 5745, 5765, 5785, 5805, 5825};

const char *const LOWIUtils::TAG = "LOWIUtils";

// This macro checks if memory allocation for "r" failed.
// If so, log error and break out of for loop.
#define UTILS_BREAK_IF_NULL(r,b,s) if (NULL == r)\
      { \
        log_debug (TAG, "%s", s); \
        b = false; \
        break; \
      }

// Length of location IE data card name
#define LOCATION_IE_DATA_CARD_LEN 32

bool LOWIUtils::parseIEDataInfo
(InPostcard *const card, vector<int8>& measurements)
{
  bool retVal = true;
  log_verbose(TAG, "parseIEDataInfo");

  do
  {
    UTILS_BREAK_IF_NULL(card, retVal, "parseIEDataInfo - Argument NULL!")

    PostcardBase::UINT32 num_of_meas = 0;
    int err = card->getUInt32("NUM_OF_IE", num_of_meas);

    if (0 != err)
    {
      log_error(TAG, "parseIEDataInfo - Unable to extract NUM_OF_IE");
      retVal = false;
      break;
    }

    log_debug(TAG, "parseIEDataInfo - Total IE's = %u", num_of_meas);

    // For each information element, retrieve the corresponding InPostcard
    // and parse the information
    int8 ieData;
    for (uint32 ii = 0; ii < num_of_meas; ++ii)
    {
      InPostcard *inner = 0;
      if (0 == card->getCard("IE_data_card", &inner, ii))
      {
        if (NULL == inner)
        {
          log_debug(TAG, "parseIEDataInfo - No IE_data_card found");
          break;
        }

        extractInt8(*inner, "parseIEDataInfo", "IE_DATA", ieData);
        log_debug(TAG, "parseIEDataInfo - IE_DATA(%d)", ieData);

        // Put the LOWIMeasurementInfo in the vector
        measurements.push_back(ieData);
        delete inner;
      }
    }
  } while (0);
  return retVal;
}

bool LOWIUtils::parseLocationIEDataInfo
(InPostcard *const card, uint8 *info, uint8 len, char const *type)
{
  bool retVal = false;
  log_verbose(TAG, "parseLocationIEDataInfo");

  do
  {
    UTILS_BREAK_IF_NULL(card, retVal, "parseLocationIEDataInfo - Argument NULL!")

    // Create the appropriate card name, and parse the information
      char cardName[LOCATION_IE_DATA_CARD_LEN] = {0};
    snprintf(cardName, sizeof(cardName), "%s%s", "LOCATION_IE_DATA_CARD_", type);
    InPostcard *inner = 0;
    if (0 == card->getCard(cardName, &inner, 0))
    {
      if (NULL == inner)
      {
        log_debug(TAG, "parseLocationIEDataInfo - No LOCATION_IE_DATA_CARD found");
        break;
      }

      // extract the info field
      int length = (int)len;
      if (0 != inner->getArrayUInt8(cardName, &length, info))
      {
        log_debug(TAG, "parseLocationIEDataInfo - Unable to extract location info");
      }
      delete inner;
    }

    retVal = true;
  } while (0);
  return retVal;
}

bool LOWIUtils::parseMeasurementInfo
(InPostcard *const card, vector<LOWIMeasurementInfo*>& measurements)
{
  bool retVal = true;
  log_verbose(TAG, "parseMeasurementInfo");

  do
  {
    if (NULL == card)
    {
      log_error(TAG, "parseMeasurementInfo - Argument NULL!");
      retVal = false;
      break;
    }

    PostcardBase::UINT32 num_of_meas = 0;
    int err = card->getUInt32("NUM_OF_MEAS", num_of_meas);
    if (0 != err)
    {
      log_error(TAG, "parseMeasurementInfo - Unable to extract NUM_OF_MEAS");
      retVal = false;
      break;
    }
    log_debug(TAG, "parseMeasurementInfo - Total measurements = %u", num_of_meas);

    // For each measurement, retrieve the corresponding InPostcard
    // and parse the information
    for (uint32 ii = 0; ii < num_of_meas; ++ii)
    {
      InPostcard *inner = 0;
      if (0 == card->getCard("Measurement_card", &inner, ii))
      {
        if (NULL == inner)
        {
          log_debug(TAG, "parseMeasurementInfo - No Measurement_card found");
          break;
        }
        LOWIMeasurementInfo *info = new(std::nothrow) LOWIMeasurementInfo;
        if (NULL == info)
        {
          log_error(TAG, "parseMeasurementInfo - Mem allocation failure!");
          // Delete the card
          delete inner;
          retVal = false;
          break;
        }

        extractInt64(*inner, "parseMeasurementInfo", "RSSI_TIMESTAMP", info->rssi_timestamp);
        extractInt16(*inner, "parseMeasurementInfo", "RSSI",           info->rssi);
        extractInt32(*inner, "parseMeasurementInfo", "MEAS_AGE",       info->meas_age);
        extractInt64(*inner, "parseMeasurementInfo", "RTT_TIMESTAMP",  info->rtt_timestamp);
        extractInt32(*inner, "parseMeasurementInfo", "RTT_PS",         info->rtt_ps);
        extractUInt8(*inner, "parseMeasurementInfo", "TX_PREAMBLE", info->tx_preamble);
        extractUInt8(*inner, "parseMeasurementInfo", "TX_NSS",      info->tx_nss);
        extractUInt8(*inner, "parseMeasurementInfo", "TX_BW",       info->tx_bw);
        extractUInt8(*inner, "parseMeasurementInfo", "TX_MCS_IDX",  info->tx_mcsIdx);
        extractUInt32(*inner, "parseMeasurementInfo", "TX_BIT_RATE", info->tx_bitrate);
        extractUInt8(*inner, "parseMeasurementInfo", "RX_PREAMBLE", info->rx_preamble);
        extractUInt8(*inner, "parseMeasurementInfo", "RX_NSS",      info->rx_nss);
        extractUInt8(*inner, "parseMeasurementInfo", "RX_BW",       info->rx_bw);
        extractUInt8(*inner, "parseMeasurementInfo", "RX_MCS_IDX",  info->rx_mcsIdx);
        extractUInt32(*inner, "parseMeasurementInfo", "RX_BIT_RATE", info->rx_bitrate);
        info->rtt = info->rtt_ps / 1000;

        log_debug(TAG, "parseMeasurementInfo - RSSI_TIMESTAMP(%lld) RSSI(%d) MEAS_AGE(%d) RTT_TIMESTAMP(%"PRId64") RTT(%d) TX_PREAMBLE(%d) TX_NSS(%d) TX_BW(%d) TX_MCS_IDX(%d) TX_BIT_RATE(%d) RX_PREAMBLE(%d) RX_NSS(%d) RX_BW(%d) RX_MCS_IDX(%d) RX_BIT_RATE(%d)",
                  info->rssi_timestamp, info->rssi, info->meas_age, info->rtt_timestamp, info->rtt, info->tx_preamble, info->tx_nss, info->tx_bw, info->tx_mcsIdx, info->tx_bitrate, info->rx_preamble, info->rx_nss, info->rx_bw, info->rx_mcsIdx, info->rx_bitrate);

        // Put the LOWIMeasurementInfo in the vector
        measurements.push_back(info);
        delete inner;
      }
    }
  } while (0);
  return retVal;
}

bool LOWIUtils::parseScanMeasurements
(InPostcard *const card, vector<LOWIScanMeasurement*>& measurements)
{
  log_verbose(TAG, "parseScanMeasurements");
  bool retVal = true;

  do
  {
    if (NULL == card)
    {
      log_error(TAG, "parseScanMeasurements - Argument NULL!");
      retVal = false;
      break;
    }

    PostcardBase::UINT32 num_of_scans = 0;
    int err = card->getUInt32("NUM_OF_SCANS", num_of_scans);
    if (0 != err)
    {
      log_error(TAG, "parseScanMeasurements - Unable to extract NUM_OF_SCANS");
      retVal = false;
      break;
    }
    log_debug(TAG, "parseScanMeasurements - Total Scan measurements = %u", num_of_scans);

    // For each scan measurement, retrieve the corresponding InPostcard
    // and parse the information
    for (uint32 ii = 0; ii < num_of_scans; ++ii)
    {
      InPostcard *inner = 0;
      if (0 == card->getCard("SCAN_MEAS_CARD", &inner, ii))
      {
        if (NULL == inner)
        {
          log_debug(TAG, "parseScanMeasurements - No SCAN_MEAS_CARD found");
          break;
        }
        bool mem_failure = false;
        LOWIScanMeasurement *meas = NULL;
        do
        {
          meas = new(std::nothrow) LOWIScanMeasurement();
          if (NULL == meas)
          {
            log_error(TAG, "parseScanMeasurements - Mem allocation failure!");
            mem_failure = true;
            break;
          }

          meas->bssid.setMac(LOWIUtils::extractBssid(*inner));

          extractUInt32(*inner, "parseScanMeasurements", "FREQUENCY", meas->frequency);
          extractBool(*inner, "parseScanMeasurements", "IS_SECURE", meas->isSecure);
          uint8 temp;
          extractUInt8(*inner, "parseScanMeasurements", "NODE_TYPE", temp);
          meas->type = LOWIUtils::to_eNodeType(temp);
          extractUInt8(*inner, "parseScanMeasurements", "RTT_TYPE", temp);
          meas->rttType = LOWIUtils::to_eRttType(temp);

          log_debug(TAG, "parseScanMeasurements - FREQUENCY(%d) IS_SECURE(%d) NODE_TYPE(%d) RTT_TYPE(%d)",
                    meas->frequency, (int)meas->isSecure, meas->type, meas->rttType);

          const void *blob = NULL;
          size_t length = 0;
          if (0 == inner->getBlob("SSID", &blob, &length))
          {
            uint8 *ssid = (uint8 *)blob;
            meas->ssid.setSSID(ssid, length);
          }
          else
          {
            log_verbose(TAG, "parseScanMeasurements - Unable to extract"
                        " SSID. It is invalid");
          }

          // Check if the MSAP Info is present
          uint8 msap_info = 0;
          if (0 == inner->getUInt8("MSAP_PROT_VER", msap_info))
          {
            log_verbose(TAG, "parseScanMeasurements - MSAP Info present");
            meas->msapInfo = new(std::nothrow) LOWIMsapInfo;
            if (NULL == meas->msapInfo)
            {
              log_error(TAG, "parseScanMeasurements - Unable to allocate memory.");
              mem_failure = true;
              break;
            }

            extractUInt8(*inner, "parseScanMeasurements", "MSAP_PROT_VER", meas->msapInfo->protocolVersion);
            extractUInt32(*inner, "parseScanMeasurements", "MSAP_VENUE_HASH", meas->msapInfo->venueHash);
            extractUInt8(*inner, "parseScanMeasurements", "MSAP_SERVER_IDX", meas->msapInfo->serverIdx);
            log_debug(TAG, "parseScanMeasurements - MSAP_PROT_VER(%d) MSAP_VENUE_HASH(%d) MSAP_SERVER_IDX(%d)",
                      meas->msapInfo->protocolVersion, meas->msapInfo->venueHash, meas->msapInfo->serverIdx);
          }
          else
          {
            meas->msapInfo = NULL;
          }

          // Get the Cell power
          extractInt8(*inner, "parseScanMeasurements", "CELL_POWER", meas->cellPowerLimitdBm);

          // Get the country code
          int num_elements = LOWI_COUNTRY_CODE_LEN;
          memset(meas->country_code, 0, LOWI_COUNTRY_CODE_LEN);
          if (0 != inner->getArrayUInt8("COUNTRY_CODE", &num_elements, meas->country_code))
          {
            log_warning(TAG, "parseScanMeasurements - Unable to extract COUNTRY_CODE");
          }
          else
          {
            log_debug(TAG, "COUNTRY_CODE is %c%c", (char)meas->country_code[0], (char)meas->country_code[1]);
          }

          // Get Indoor / Outdoor
          extractUInt8(*inner, "parseScanMeasurements", "INDOOR_OUTDOOR", meas->indoor_outdoor);

          // get the measurement number
          extractUInt32(*inner, "parseScanMeasurements", "MEASUREMENT_NUM", meas->measurementNum);

          log_debug(TAG, "parseScanMeasurements - CELL_POWER(%d) INDOOR OUTDOOR(%c), MEASUREMENT_NUM(%u)",
                    meas->cellPowerLimitdBm,
                    (char)meas->indoor_outdoor,
                    meas->measurementNum);

          parseMeasurementInfo(inner, meas->measurementsInfo);

          //get the RTT target status code
          uint32 targetStatus = 0;
          extractUInt32(*inner, "parseScanMeasurements", "RTT_TARGET_STATUS", targetStatus);
          meas->targetStatus = (LOWIScanMeasurement::eTargetStatus)targetStatus;

          // get the beacon period
          extractUInt16(*inner, "parseScanMeasurements", "BEACON_PERIOD", meas->beaconPeriod);

          // get the beacon capabilities
          extractUInt16(*inner, "parseScanMeasurements", "BEACON_CAPS", meas->beaconCaps);

          // get the information element data
          parseIEDataInfo(inner, meas->ieData);

          log_debug(TAG, "parseScanMeasurements - RTT_TARGET_STATUS(%u) BEACON_PERIOD(%u), BEACON_CAPS(%u), IE_LENGTH(%u)",
                    meas->targetStatus,
                    meas->beaconPeriod,
                    meas->beaconCaps,
                    meas->ieData.getNumOfElements());

          // Get the Number of RTT frames attempted.
          extractUInt16(*inner, "parseScanMeasurements", "NUM_RTT_FRAMES_ATTEMPTED", meas->num_frames_attempted);
          // Get the actual time taken to complete rtt measurement.
          extractUInt16(*inner, "parseScanMeasurements", "ACTUAL_BURST_DURATION", meas->actual_burst_duration);
          // Get FTM frames per burst negotiated with target.
          extractUInt8(*inner, "parseScanMeasurements", "NEGOTIATED_NUM_FRAMES_PER_BURST", meas->negotiated_num_frames_per_burst);
          // Get the time after which FTM session can be retried.
          extractUInt8(*inner, "parseScanMeasurements", "RETRY_RTT_AFTER_DURATION", meas->retry_after_duration);
          // Get number of FTM bursts negotiated with the target.
          extractUInt8(*inner, "parseScanMeasurements", "NEGOTIATED_BURST_EXPONENT", meas->negotiated_burst_exp);

          // Check if the lciInfo is present
          uint8 lciInfoID = 0;
          if (0 == inner->getUInt8("LCI_INFO_ID", lciInfoID))
          {
            log_verbose(TAG, "parseScanMeasurements - LCI info present");
            meas->lciInfo = new(std::nothrow) LOWILocationIE();
            if (NULL == meas->lciInfo)
            {
              log_error(TAG, "parseScanMeasurements - Unable to allocate memory.");
              mem_failure = true;
              break;
            }

            meas->lciInfo->id = lciInfoID;
            uint8 len;
            extractUInt8(*inner, "parseScanMeasurements", "LCI_INFO_LEN", len);
            meas->lciInfo->len = len;
            log_debug(TAG, "parseScanMeasurements - LCI_INFO_ID(%d) LCI_INFO_LEN(%d)",
                      meas->lciInfo->id, meas->lciInfo->len);
            // get the location information element data
            if (0 != len)
            {
              meas->lciInfo->locData = (uint8 *)malloc(len);
              if (NULL != meas->lciInfo->locData)
              {
                memset(meas->lciInfo->locData, 0, len);
                parseLocationIEDataInfo(inner, meas->lciInfo->locData, len, "LCI");
              }
            }
          }
          else
          {
            meas->lciInfo = NULL;
          }

          // Check if the lcrInfo is present
          uint8 lcrInfoID = 0;
          if (0 == inner->getUInt8("LCR_INFO_ID", lcrInfoID))
          {
            log_verbose(TAG, "parseScanMeasurements - LCR info present");
            meas->lcrInfo = new(std::nothrow) LOWILocationIE();
            if (NULL == meas->lcrInfo)
            {
              log_error(TAG, "parseScanMeasurements - Unable to allocate memory.");
              mem_failure = true;
              break;
            }

            meas->lcrInfo->id = lcrInfoID;
            uint8 len;
            extractUInt8(*inner, "parseScanMeasurements", "LCR_INFO_LEN", len);
            meas->lcrInfo->len = len;
            log_debug(TAG, "parseScanMeasurements - LCR_INFO_ID(%d) LCR_INFO_LEN(%d)",
                      meas->lcrInfo->id, meas->lcrInfo->len);
            // get the location information element data
            if (0 != len)
            {
              meas->lcrInfo->locData = (uint8 *)malloc(len);
              if (NULL != meas->lcrInfo->locData)
              {
                memset(meas->lcrInfo->locData, 0, len);
                parseLocationIEDataInfo(inner, meas->lcrInfo->locData, len, "LCR");
              }
            }
          }
          else
          {
            meas->lcrInfo = NULL;
          }

          // Put the LOWIMeasurementInfo in the vector
          measurements.push_back(meas);
        } while (0);

        // Delete the card
        delete inner;
        // Delete the measurement pointer if we are here because
        // of mem allocation failure.
        if (true == mem_failure)
        {
          delete meas;
          // No need to continue. break out of the for loop.
          retVal = false;
          break;
        }
      } // if Card
    } // for
  } while (0);

  return retVal;
}

OutPostcard* LOWIUtils::requestToOutPostcard(LOWIRequest *const request,
                                             const char *const originatorId)
{
  OutPostcard *card = NULL;
  bool success = false;
  log_verbose(TAG, "requestToOutPostcard");
  do
  {
    // Check the parameters
    if (NULL == request || NULL == originatorId)
    {
      log_error(TAG, "requestToOutPostcard - parameter can not be NULL");
      break;
    }

    // Create the OutPostcard
    card = OutPostcard::createInstance();
    if (NULL == card) break;
    card->init();
    card->addString("TO", SERVER_NAME);
    card->addString("FROM", originatorId);

    // Check the type of request and initialize postcard
    LOWIRequest::eRequestType type = request->getRequestType();
    log_verbose(TAG, "requestToOutPostcard - Request type = %s", LOWIUtils::to_string(type));
    switch (type)
    {
    case LOWIRequest::CAPABILITY:
      {
        LOWICapabilityRequest *req = (LOWICapabilityRequest *)request;
        card->addString("REQ", "LOWI_CAPABILITY");
        card->addUInt32("REQ_ID", req->getRequestId());
        // Add TX-ID of type int32 as a standard field in postcard
        card->addInt32("TX-ID", req->getRequestId());
        success = true;
      }
      break;
    case LOWIRequest::DISCOVERY_SCAN:
      {
        LOWIDiscoveryScanRequest *req = (LOWIDiscoveryScanRequest *)request;
        card->addString("REQ", "LOWI_DISCOVERY_SCAN");
        card->addUInt32("REQ_ID", req->getRequestId());
        // Add TX-ID of type int32 as a standard field in postcard
        card->addInt32("TX-ID", req->getRequestId());
        card->addUInt8("BAND", req->getBand());
        card->addBool("BUFFER_CACHE_BIT", req->getBufferCacheRequest());
        card->addUInt32("MEAS_AGE_FILTER", req->getMeasAgeFilterSec());
        card->addUInt32("FALLBACK_TOLERANCE", req->getFallbackToleranceSec());
        card->addUInt8("REQUEST_MODE", req->getRequestMode());
        card->addUInt8("REQUEST_TYPE", req->getRequestType());
        card->addUInt8("SCAN_TYPE", req->getScanType());
        card->addInt64("REQ_TIMEOUT", req->getTimeoutTimestamp());

        vector<LOWIChannelInfo> vec = req->getChannels();
        // Channels are optional. So continue if there are no channels
        // specified in the request.
        uint32 ii = 0;
        for (; ii < vec.getNumOfElements(); ++ii)
        {
          LOWIChannelInfo info = vec[ii];

          // For each Channel Info create a OutPostcard
          OutPostcard *ch_card = OutPostcard::createInstance();
          if (NULL == ch_card)
          {
            // Unable to allocate memory for inner card
            // break out of for loop and log error.
            // The main card returned by the function will
            // contain less information.
            log_error(TAG, "requestToOutPostcard - Mem allocation failure!");
            break;
          }

          ch_card->init();

          ch_card->addUInt32("FREQUENCY", info.getFrequency());

          ch_card->finalize();
          card->addCard("CHANNEL_CARD", ch_card);
          log_debug(TAG, "requestToOutPostcard - Added a channel card to the main card");
          delete ch_card;
        }
        card->addUInt32("NUM_OF_CHANNELS", ii);
        if (ii < vec.getNumOfElements())
        {
          // Not all channels could be allocated. Memory allocation error
          success = false;
          break;
        }
        success = true;
      }
      break;
    case LOWIRequest::RANGING_SCAN:
      {
        LOWIRangingScanRequest *req = (LOWIRangingScanRequest *)request;
        card->addString("REQ", "LOWI_RANGING_SCAN");
        card->addUInt32("REQ_ID", req->getRequestId());
        // Add TX-ID of type int32 as a standard field in postcard
        card->addInt32("TX-ID", req->getRequestId());
        card->addUInt8("REQUEST_TYPE", req->getRequestType());
        card->addInt64("REQ_TIMEOUT", req->getTimeoutTimestamp());
        card->addUInt8("RANGING_SCAN_REPORT_TYPE", req->getReportType());

        vector<LOWINodeInfo> vec = req->getNodes();
        if (0 == vec.getNumOfElements()) break;

        uint32 ii = 0;
        for (; ii < vec.getNumOfElements(); ++ii)
        {
          LOWINodeInfo info = vec[ii];

          // For each Node Info create a OutPostcard
          OutPostcard *node_card = OutPostcard::createInstance();
          if (NULL == node_card)
          {
            // Unable to allocate memory for inner card
            // break out of for loop and log error.
            // The main card returned by the function will
            // contain less information.
            log_error(TAG, "requestToOutPostcard - Mem allocation failure!");
            break;
          }

          node_card->init();

          unsigned int bssid = info.bssid.getLo24();
          node_card->addUInt32("BSSID_LO", bssid);

          bssid = info.bssid.getHi24();
          node_card->addUInt32("BSSID_HI", bssid);

          node_card->addUInt32("FREQUENCY", info.frequency);

          node_card->addUInt32("BAND_CENTER_FREQ1", info.band_center_freq1);

          node_card->addUInt32("BAND_CENTER_FREQ2", info.band_center_freq2);

          node_card->addUInt8("NODE_TYPE", info.nodeType);

          unsigned int spoof_mac_id = info.spoofMacId.getLo24();
          node_card->addUInt32("SPOOF_MAC_ID_LO", spoof_mac_id);

          spoof_mac_id = info.spoofMacId.getHi24();
          node_card->addUInt32("SPOOF_MAC_ID_HI", spoof_mac_id);

          node_card->addUInt8("RTT_TYPE", info.rttType);

          node_card->addUInt8("RANGING_BW", info.bandwidth);

          node_card->addUInt8("RANGING_PREAMBLE", info.preamble);

          node_card->addUInt32("FTM_RANGING_PARAMS", info.ftmRangingParameters);

          node_card->addUInt8("NUM_PKTS_PER_MEAS", info.num_pkts_per_meas);

          node_card->addUInt8("NUM_RETRIES_PER_MEAS", info.num_retries_per_meas);

          node_card->finalize();
          card->addCard("WIFI_NODE_CARD", node_card);
          log_debug(TAG, "requestToOutPostcard - Added a node card to the main card");
          delete node_card;
        }
        card->addUInt32("NUM_OF_NODES", ii);

        if (ii < vec.getNumOfElements())
        {
          // Not all nodes could be allocated. Memory allocation error
          success = false;
          break;
        }

        success = true;
      }
      break;
    case LOWIRequest::PERIODIC_RANGING_SCAN:
      {
        LOWIPeriodicRangingScanRequest *req = (LOWIPeriodicRangingScanRequest *)request;
        card->addString("REQ", "LOWI_PERIODIC_RANGING_SCAN");
        card->addUInt32("REQ_ID", req->getRequestId());
        // Add TX-ID of type int32 as a standard field in postcard
        card->addInt32("TX-ID", req->getRequestId());
        card->addUInt8("REQUEST_TYPE", req->getRequestType());
        card->addInt64("REQ_TIMEOUT", req->getTimeoutTimestamp());
        card->addUInt8("RANGING_SCAN_REPORT_TYPE", req->getReportType());

        vector<LOWIPeriodicNodeInfo> vec = req->getNodes();
        if (0 == vec.getNumOfElements())
        {
          log_warning(TAG, "Request has no nodes");
          break;
        }

        uint32 ii = 0;
        for (; ii < vec.getNumOfElements(); ++ii)
        {
          LOWIPeriodicNodeInfo info = vec[ii];

          // For each Node Info create a OutPostcard
          OutPostcard *node_card = OutPostcard::createInstance();
          if (NULL == node_card)
          {
            // Unable to allocate memory for inner card
            // break out of for loop and log error.
            // The main card returned by the function will
            // contain less information.
            log_error(TAG, "requestToOutPostcard - Mem allocation failure!");
            break;
          }

          node_card->init();

          unsigned int bssid = info.bssid.getLo24();
          node_card->addUInt32("BSSID_LO", bssid);

          bssid = info.bssid.getHi24();
          node_card->addUInt32("BSSID_HI", bssid);

          node_card->addUInt32("FREQUENCY", info.frequency);

          node_card->addUInt32("BAND_CENTER_FREQ1", info.band_center_freq1);

          node_card->addUInt32("BAND_CENTER_FREQ2", info.band_center_freq2);

          node_card->addUInt8("NODE_TYPE", info.nodeType);

          unsigned int spoof_mac_id = info.spoofMacId.getLo24();
          node_card->addUInt32("SPOOF_MAC_ID_LO", spoof_mac_id);

          spoof_mac_id = info.spoofMacId.getHi24();
          node_card->addUInt32("SPOOF_MAC_ID_HI", spoof_mac_id);
          node_card->addUInt8("RTT_TYPE", info.rttType);
          node_card->addUInt8("RANGING_BW", info.bandwidth);
          node_card->addUInt8("RANGING_PREAMBLE", info.preamble);
          node_card->addUInt32("FTM_RANGING_PARAMS", info.ftmRangingParameters);
          node_card->addUInt8("NUM_PKTS_PER_MEAS", info.num_pkts_per_meas);
          node_card->addUInt8("PERIODIC", info.periodic);
          node_card->addUInt32("MEAS_PERIOD", info.meas_period);
          node_card->addUInt32("NUM_MEASUREMENTS", info.num_measurements);
          node_card->addUInt8("NUM_RETRIES_PER_MEAS", info.num_retries_per_meas);
          node_card->finalize();
          card->addCard("WIFI_NODE_CARD", node_card);
          log_debug(TAG, "requestToOutPostcard - Added a node card to the main card");
          delete node_card;
        }
        card->addUInt32("NUM_OF_NODES", ii);

        if (ii < vec.getNumOfElements())
        {
          // Not all nodes could be allocated. Memory allocation error
          success = false;
          break;
        }

        success = true;
      }
      break;
    case LOWIRequest::RESET_CACHE:
      {
        LOWICacheResetRequest *req = (LOWICacheResetRequest *)request;
        card->addString("REQ", "LOWI_RESET_CACHE");
        card->addUInt32("REQ_ID", req->getRequestId());
        // Add TX-ID of type int32 as a standard field in postcard
        card->addInt32("TX-ID", req->getRequestId());
        success = true;
      }
      break;
    case LOWIRequest::ASYNC_DISCOVERY_SCAN_RESULTS:
      {
        LOWIAsyncDiscoveryScanResultRequest *req =
                                                   (LOWIAsyncDiscoveryScanResultRequest *)request;
        card->addString("REQ", "LOWI_ASYNC_DISCOVERY_SCAN_RESULTS");
        card->addUInt32("REQ_ID", req->getRequestId());
        card->addUInt8("REQUEST_TYPE", req->getRequestType());
        card->addUInt32("REQ_TIMEOUT", req->getRequestExpiryTime());

        success = true;
      }
      break;
    case LOWIRequest::CANCEL_RANGING_SCAN:
      {
        LOWICancelRangingScanRequest *req = (LOWICancelRangingScanRequest *)request;
        card->addString("REQ", "CANCEL_RANGING_SCAN");
        card->addUInt32("REQ_ID", req->getRequestId());
        // Add TX-ID of type int32 as a standard field in postcard
        card->addInt32("TX-ID", req->getRequestId());
        card->addUInt8("REQUEST_TYPE", req->getRequestType());

        vector<LOWIMacAddress> vec = req->getBssids();
        if (0 == vec.getNumOfElements()) break;

        uint32 ii = 0;
        for (; ii < vec.getNumOfElements(); ++ii)
        {
          LOWIMacAddress macAddr = vec[ii];

          // For each bssid create a OutPostcard
          OutPostcard *bssid_card = OutPostcard::createInstance();
          if (NULL == bssid_card)
          {
            // Unable to allocate memory for inner card
            // break out of for loop and log error.
            // The main card returned by the function will
            // contain less information.
            log_error(TAG, "requestToOutPostcard - Mem allocation failure!");
            break;
          }

          bssid_card->init();

          unsigned int bssid = macAddr.getLo24();
          bssid_card->addUInt32("BSSID_LO", bssid);

          bssid = macAddr.getHi24();
          bssid_card->addUInt32("BSSID_HI", bssid);

          bssid_card->finalize();
          card->addCard("WIFI_BSSID_CARD", bssid_card);
          log_debug(TAG, "requestToOutPostcard - Added a node card to the main card");
          delete bssid_card;
        }
        card->addUInt32("NUM_OF_BSSIDS", ii);

        if (ii < vec.getNumOfElements())
        {
          // Not all nodes could be allocated. Memory allocation error
          success = false;
          break;
        }

        success = true;
      }
      break;
    case LOWIRequest::SET_LCI_INFORMATION:
      {
        LOWISetLCILocationInformation *req = (LOWISetLCILocationInformation *)request;
        card->addString("REQ", "SET_LCI_INFORMATION");
        card->addUInt32("REQ_ID", req->getRequestId());
        // Add TX-ID of type int32 as a standard field in postcard
        card->addInt32("TX-ID", req->getRequestId());

        LOWILciInformation params = req->getLciParams();
        card->addInt64("LATITUDE", params.latitude);
        card->addInt64("LONGITUDE", params.longitude);
        card->addInt32("ALTITUDE", params.altitude);
        card->addUInt8("LATITUDE_UNC", params.latitude_unc);
        card->addUInt8("LONGITUDE_UNC", params.longitude_unc);
        card->addUInt8("ALTITUDE_UNC", params.altitude_unc);
        card->addUInt8("MOTION_PATTERN", params.motion_pattern);
        card->addInt32("FLOOR", params.floor);
        card->addInt32("HEIGHT_ABOVE_FLOOR", params.height_above_floor);
        card->addInt32("HEIGHT_UNC", params.height_unc);
        card->addUInt32("USAGE_RULES", req->getUsageRules());
        success = true;
      }
      break;
    case LOWIRequest::SET_LCR_INFORMATION:
      {
        LOWISetLCRLocationInformation *req = (LOWISetLCRLocationInformation *)request;
        card->addString("REQ", "SET_LCR_INFORMATION");
        card->addUInt32("REQ_ID", req->getRequestId());
        // Add TX-ID of type int32 as a standard field in postcard
        card->addInt32("TX-ID", req->getRequestId());

        LOWILcrInformation params = req->getLcrParams();
        card->addArrayUInt8("LCR_COUNTRY_CODE", LOWI_COUNTRY_CODE_LEN, params.country_code);
        card->addUInt32("LCR_LENGTH", params.length);
        card->addArrayInt8("LCR_CIVIC_INFO", CIVIC_INFO_LEN, params.civic_info);
        success = true;
      }
      break;
    case LOWIRequest::NEIGHBOR_REPORT:
      {
        LOWINeighborReportRequest *req = (LOWINeighborReportRequest *)request;
        card->addString("REQ", "NEIGHBOR_REPORT");
        card->addUInt32("REQ_ID", req->getRequestId());
        // Add TX-ID of type int32 as a standard field in postcard
        card->addInt32("TX-ID", req->getRequestId());
        success = true;
      }
      break;
    case LOWIRequest::SEND_LCI_REQUEST:
      {
        LOWISendLCIRequest *req = (LOWISendLCIRequest *)request;
        card->addString("REQ", "SEND_LCI_REQUEST");
        card->addUInt32("REQ_ID", req->getRequestId());
        // Add TX-ID of type int32 as a standard field in postcard
        card->addInt32("TX-ID", req->getRequestId());

        addBssidToCard(*card, req->getBssid());
        success = true;
      }
      break;
    case LOWIRequest::FTM_RANGE_REQ:
      {
        LOWIFTMRangingRequest *req = (LOWIFTMRangingRequest *)request;
        card->addString("REQ", "FTM_RANGE_REQ");
        card->addUInt32("REQ_ID", req->getRequestId());
        // Add TX-ID of type int32 as a standard field in postcard
        card->addInt32("TX-ID", req->getRequestId());

        addBssidToCard(*card, req->getBSSID());
        card->addUInt16("RAND_INTER", req->getRandInter());
        vector<LOWIFTMRRNodeInfo> nodes = req->getNodes();
        uint32 ii = 0;
        for (; ii < nodes.getNumOfElements(); ++ii)
        {
          if (!addFTMRRNodeToCard(*card, nodes[ii]))
          {
            break;
          }
        }
        if (ii < nodes.getNumOfElements())
        {
          success = false;
          log_error(TAG, "Failed to add nodes to FTMRR");
          break;
        }
        card->addUInt32("NUM_NODES", ii);
        success = true;
      }
      break;
    default:
      log_warning(TAG, "requestToOutPostcard - Invalid request");
      break;
    }

    if (true == success)
    {
      // Finalize post card
      card->finalize();
      log_verbose(TAG, "requestToOutPostcard - Card finalized");
    }
    else
    {
      log_warning(TAG, "requestToOutPostcard - Unable to create card");
      delete card;
      card = NULL;
    }
  } while (0);
  return card;
}

LOWIResponse* LOWIUtils::inPostcardToResponse(InPostcard *const card)
{
  log_verbose(TAG, "inPostcardToResponse");
  LOWIResponse *response = NULL;
  bool success = false;

  do
  {
    // Check the postcard
    if (NULL == card)
    {
      log_error(TAG, "inPostcardToResponse - Input Parameter can not be NULL!");
      break;
    }

    const char *print_from = NOT_AVAILABLE;
    const char *from = 0;
    if (0 != card->getString("FROM", &from))
    {
      log_warning(TAG, "inPostcardToResponse - Unable to extract FROM");
    }
    else
    {
      print_from = from;
    }

    const char *print_to = NOT_AVAILABLE;
    const char *to = 0;
    if (0 != card->getString("TO", &to))
    {
      log_warning(TAG, "inPostcardToResponse - Unable to extract TO");
    }
    else
    {
      print_to = to;
    }

    const char *print_resp_type = NOT_AVAILABLE;
    const char *resp_type = 0;
    if (0 != card->getString("RESP", &resp_type))
    {
      log_warning(TAG, "inPostcardToResponse - Unable to extract RESP");
    }
    else
    {
      print_resp_type = resp_type;
    }

    log_info(TAG, "inPostcardToResponse - FROM: %s, TO:   %s, RESP:  %s",
             print_from, print_to, print_resp_type);

    if (resp_type == NULL)
    {
      log_error(TAG, "inPostcardToResponse - NULL resp, break");
      break;
    }

    // extract the Request ID which all responses have
    uint32 req_id = 0;
    extractUInt32(*card, "inPostcardToResponse", "REQ_ID", req_id);

    // Create the response
    if ((0 == strcmp(resp_type, "LOWI_DISCOVERY_SCAN")) ||
        (0 == strcmp(resp_type, "LOWI_ASYNC_DISCOVERY_SCAN_RESULTS")))
    {
      // Scan Status
      uint8 scan_status = 0;
      extractUInt8(*card, "inPostcardToResponse", "SCAN_STATUS", scan_status);

      // Scan Type
      uint8 scan_type = 0;
      extractUInt8(*card, "inPostcardToResponse", "SCAN_TYPE", scan_type);

      // Packet Timestamp
      INT64 packet_timestamp = 0;
      extractInt64(*card, "inPostcardToResponse", "PACKET_TIMESTAMP", packet_timestamp);

      log_debug(TAG, "inPostcardToResponse - Request id(%d) Scan Status(%d) Scan Type(%d) Packet time stamp(%"PRId64")",
                req_id, scan_status, scan_type, packet_timestamp);

      if (0 == strcmp(resp_type, "LOWI_DISCOVERY_SCAN"))
      {
        log_debug(TAG, "inPostcardToResponse - DiscoveryScanResponse");
        LOWIDiscoveryScanResponse *resp =
                                          new(std::nothrow) LOWIDiscoveryScanResponse(req_id);
        if (NULL == resp)
        {
          log_error(TAG, "inPostcardToResponse - Memory allocation failure!");
          break;
        }

        parseScanMeasurements(card, resp->scanMeasurements);

        resp->scanStatus = LOWIUtils::to_eScanStatus(scan_status);
        resp->scanTypeResponse = LOWIUtils::to_eScanTypeResponse(scan_type);
        resp->timestamp = packet_timestamp;
        response = resp;
      }
      else
      {
        log_debug(TAG, "inPostcardToResponse -"
                  " AsyncDiscoveryScanResultResponse");
        LOWIAsyncDiscoveryScanResultResponse *resp =
                                                     new(std::nothrow) LOWIAsyncDiscoveryScanResultResponse(req_id);
        if (NULL == resp)
        {
          log_error(TAG, "inPostcardToResponse - Memory allocation failure!");
          break;
        }

        parseScanMeasurements(card, resp->scanMeasurements);

        resp->scanStatus = LOWIUtils::to_eScanStatus(scan_status);
        resp->scanTypeResponse = LOWIUtils::to_eScanTypeResponse(scan_type);
        resp->timestamp = packet_timestamp;
        response = resp;
      }

      success = true;
    }
    else if (0 == strcmp(resp_type, "LOWI_RANGING_SCAN"))
    {
      // Scan Status
      uint8 scan_status = 0;
      extractUInt8(*card, "inPostcardToResponse", "SCAN_STATUS", scan_status);

      log_debug(TAG, "inPostcardToResponse - Request id(%d) Scan Status(%d)",
                req_id, scan_status);

      LOWIRangingScanResponse *resp =
                                      new(std::nothrow) LOWIRangingScanResponse(req_id);
      if (NULL == resp)
      {
        log_error(TAG, "inPostcardToResponse - Memory allocation failure!");
        break;
      }

      parseScanMeasurements(card, resp->scanMeasurements);

      resp->scanStatus = LOWIUtils::to_eScanStatus(scan_status);

      response = resp;
      success = true;
    }
    else if (0 == strcmp(resp_type, "LOWI_CAPABILITY"))
    {
      // Ranging Supported
      bool ranging_supported;
      extractBool(*card, "inPostcardToResponse", "RANGING_SCAN_SUPPORTED", ranging_supported);

      // Discovery Supported
      bool discovery_supported;
      extractBool(*card, "inPostcardToResponse", "DISCOVERY_SCAN_SUPPORTED", discovery_supported);

      // Active Scan Supported
      bool active_supported;
      extractBool(*card, "inPostcardToResponse", "ACTIVE_SCAN_SUPPORTED", active_supported);

      // scans Supported
      uint32 supported_capability;
      extractUInt32(*card, "inPostcardToResponse", "SUPPORTED_CAPABILITY", supported_capability);

      log_debug(TAG, "inPost cap Response - Request id(%d) Ranging scan(%d)"
                "Discovery scan(%d) Active scan(%d) capability bitmask(0x%x)",
                req_id, (int)ranging_supported, (int)discovery_supported, (int)active_supported, supported_capability);

      // Single-sided ranging scan supported
      bool single_sided_supported;
      extractBool(*card, "inPostcardToResponse", "SINGLE_SIDED_RANGING_SCAN_SUPPORTED", single_sided_supported);

      // Dual-sided ranging scan supported (11v)
      bool dual_sided_supported_11v;
      extractBool(*card, "inPostcardToResponse", "DUAL_SIDED_RANGING_SCAN_SUPPORTED_11V", dual_sided_supported_11v);

      // Dual-sided ranging scan supported (11mc)
      bool dual_sided_supported_11mc;
      extractBool(*card, "inPostcardToResponse", "DUAL_SIDED_RANGING_SCAN_SUPPORTED_11MC", dual_sided_supported_11mc);

      log_debug(TAG, "inPostcardToResponse - Single-sided rang scan supported(%d)"
                " Ranging 11v(%d) Ranging 11mc(%d)",
                (int)single_sided_supported, (int)dual_sided_supported_11v,
                (int)dual_sided_supported_11mc);

      // bw support level
      uint8 bw_support;
      extractUInt8(*card, "inPostcardToResponse", "BW_SUPPORT", bw_support);

      // preamble support mask
      uint8 preamble_support;
      extractUInt8(*card, "inPostcardToResponse", "PREAMBLE_SUPPORT", preamble_support);

      // Request status
      bool cap_status;
      extractBool(*card, "inPostcardToResponse", "CAPABILITY_STATUS", cap_status);

      log_debug(TAG, "inPostcardToResponse - Capability scan status = %d",
                (int)cap_status);

      LOWICapabilities cap;
      cap.discoveryScanSupported        = discovery_supported;
      cap.rangingScanSupported          = ranging_supported;
      cap.activeScanSupported           = active_supported;
      cap.oneSidedRangingSupported      = single_sided_supported;
      cap.dualSidedRangingSupported11v  = dual_sided_supported_11v;
      cap.dualSidedRangingSupported11mc = dual_sided_supported_11mc;
      cap.bwSupport                     = bw_support;
      cap.preambleSupport               = preamble_support;
      cap.supportedCapablities          = supported_capability;

      LOWICapabilityResponse *resp =
                                     new(std::nothrow) LOWICapabilityResponse(req_id, cap, cap_status);
      if (NULL == resp)
      {
        log_error(TAG, "inPostcardToResponse - Memory allocation failure!");
        break;
      }

      response = resp;
      success = true;
    }
    else if (0 == strcmp(resp_type, "LOWI_RESET_CACHE"))
    {
      // status
      bool status;
      extractBool(*card, "inPostcardToResponse", "CACHE_STATUS", status);
      log_debug(TAG, "inPostcardToResponse - Request id(%d) Cache status(%d)",
                req_id, (int)status);

      LOWICacheResetResponse *resp =
                                     new(std::nothrow) LOWICacheResetResponse(req_id, status);
      if (NULL == resp)
      {
        log_error(TAG, "inPostcardToResponse - Memory allocation failure!");
        break;
      }

      response = resp;
      success = true;
    }
    else if (0 == strcmp(resp_type, "LOWI_STATUS"))
    {
      uint8 status;
      uint8 reqType;
      extractUInt8(*card, "inPostcardToResponse", "LOWI_STATUS", status);
      extractUInt8(*card, "inPostcardToResponse", "REQ_TYPE", reqType);
      log_debug(TAG, "inPostcardToResponse - Request id(%d) RspStatus(%u), ReqType(%u)",
                req_id, status, reqType);

      LOWIStatusResponse *resp = new(std::nothrow) LOWIStatusResponse(req_id);
      UTILS_BREAK_IF_NULL(resp, success, "inPostcardToResponse - Memory allocation failure!")

      resp->scanStatus = LOWIUtils::to_eLOWIDriverStatus(status);

      response = resp;
      success = true;
    }
  }while (0);

  return response;
}

bool LOWIUtils::parsePeriodicRangScanParams(InPostcard *const card,
                                            vector<LOWIPeriodicNodeInfo>& vec,
                                            uint32& req_id,
                                            INT64& timeoutTimestamp,
                                            uint8& rttReportType)
{
  // Time out timestamp
  extractInt64(*card, "parsePeriodicRangScanParams", "REQ_TIMEOUT", timeoutTimestamp);
  // Ranging Scan Report Type
  extractUInt8(*card, "parsePeriodicRangScanParams", "RANGING_SCAN_REPORT_TYPE", rttReportType);
  // Wifi Nodes
  uint32 num_of_nodes = 0;
  extractUInt32(*card, "parsePeriodicRangScanParams", "NUM_OF_NODES", num_of_nodes);

  log_debug(TAG, "parsePeriodicRangScanParams - Request id(%d) REQ_TIMEOUT(%lld) RANGING_SCAN_REPORT_TYPE(%u) NUM_OF_NODES(%u)",
            req_id, timeoutTimestamp, rttReportType, num_of_nodes);

  for (uint32 ii = 0; ii < num_of_nodes; ++ii)
  {
    // For each Node extract the card
    InPostcard *inner = 0;
    int err = card->getCard("WIFI_NODE_CARD", &inner, ii);
    if (0 != err || NULL == inner)
    {
      // Unable to get card. break out of for loop.
      log_error(TAG, "parsePeriodicRangScanParams - Unable to extract WIFI_NODE_CARD");
      return false;
    }

    LOWIPeriodicNodeInfo info;

    info.bssid.setMac(LOWIUtils::extractBssid(*inner));

    extractUInt32(*inner, "parsePeriodicRangScanParams", "FREQUENCY", info.frequency);
    extractUInt32(*inner, "parsePeriodicRangScanParams", "BAND_CENTER_FREQ1", info.band_center_freq1);
    extractUInt32(*inner, "parsePeriodicRangScanParams", "BAND_CENTER_FREQ2", info.band_center_freq2);

    unsigned char type;
    extractUInt8(*inner, "parsePeriodicRangScanParams", "NODE_TYPE", type);
    info.nodeType = LOWIUtils::to_eNodeType(type);

    uint32 spoof_lo;
    extractUInt32(*inner, "parsePeriodicRangScanParams", "SPOOF_MAC_ID_LO", spoof_lo);
    uint32 spoof_hi;
    extractUInt32(*inner, "parsePeriodicRangScanParams", "SPOOF_MAC_ID_HI", spoof_hi);
    info.spoofMacId.setMac(spoof_hi, spoof_lo);

    uint8 rttType;
    extractUInt8(*inner, "parsePeriodicRangScanParams", "RTT_TYPE", rttType);
    info.rttType = LOWIUtils::to_eRttType(rttType);

    uint8 ranging_bw;
    extractUInt8(*inner, "parsePeriodicRangScanParams", "RANGING_BW", ranging_bw);
    info.bandwidth = LOWIUtils::to_eRangingBandwidth(ranging_bw);

    uint8 ranging_preamble = 0;
    extractUInt8(*inner, "parsePeriodicRangScanParams", "RANGING_PREAMBLE", ranging_preamble);
    info.preamble = LOWIUtils::to_eRangingPreamble(ranging_preamble);

    uint32 ftmRangingParameters = 0;
    extractUInt32(*inner, "parsePeriodicRangScanParams", "FTM_RANGING_PARAMS", ftmRangingParameters);
    info.ftmRangingParameters = ftmRangingParameters;

    log_debug(TAG, "parsePeriodicRangScanParams - FREQUENCY(%d) BAND_CENTER_FREQ1(%d) BAND_CENTER_FREQ2(%d) NODE_TYPE(%d) RTT Type(%u) Ranging BW(%u) RANGING_PREAMBLE(%u)",
              info.frequency, info.band_center_freq1, info.band_center_freq2, info.nodeType, info.rttType, info.bandwidth, info.preamble);

    extractUInt8(*inner, "parsePeriodicRangScanParams", "PERIODIC", info.periodic);
    extractUInt32(*inner, "parsePeriodicRangScanParams", "MEAS_PERIOD", info.meas_period);
    extractUInt32(*inner, "parsePeriodicRangScanParams", "NUM_MEASUREMENTS", info.num_measurements);
    extractUInt8(*inner, "parsePeriodicRangScanParams", "NUM_PKTS_PER_MEAS", info.num_pkts_per_meas);
    extractUInt8(*inner, "parsePeriodicRangScanParams", "NUM_RETRIES_PER_MEAS", info.num_retries_per_meas);

    log_debug(TAG, "parsePeriodicRangScanParams -  Periodic(%u) MEAS_PERIOD(%d) NUM_MEASUREMENTS(%d)",
              info.periodic, info.meas_period, info.num_measurements);
    log_debug(TAG, "parsePeriodicRangScanParams -  NUM_PKTS_PER_MEAS(%d) NUM_RETRIES_PER_MEAS(%u)",
              info.num_pkts_per_meas, info.num_retries_per_meas);

    vec.push_back(info);

    delete inner;
  }
  return true;
}

void LOWIUtils::extractUInt8(InPostcard& inner, const char *n, const char *s, uint8& num)
{
  num = 0;
  if (0 != inner.getUInt8(s, num))
  {
    log_warning(TAG, "%s%s%s", n, " - Unable to extract ", s);
  }
}

void LOWIUtils::extractUInt16(InPostcard& card, const char *n, const char *s, uint16& num)
{
  num = 0;
  if (0 != card.getUInt16(s, num))
  {
    log_warning(TAG, "%s%s%s", n, " - Unable to extract ", s);
  }
}

void LOWIUtils::extractUInt32(InPostcard& card, const char *n, const char *s, uint32& num)
{
  PostcardBase::UINT32 num32 = 0;
  if (0 != card.getUInt32(s, num32))
  {
    log_warning(TAG, "%s%s%s", n, " - Unable to extract ", s);
  }
  num = num32;
}

void LOWIUtils::extractInt8(InPostcard& inner, const char *n, const char *s, int8& num)
{
  uint32 val = strncmp(s, "CELL_POWER", sizeof("CELL_POWER"));
  num = (0 == val) ? CELL_POWER_NOT_FOUND : 0;

  if (0 != inner.getInt8(s, (INT8&)num))
  {
    log_warning(TAG, "%s%s%s", n, " - Unable to extract ", s);
  }
}

void LOWIUtils::extractInt16(InPostcard& inner, const char *n, const char *s, INT16& num)
{
  num = 0;
  if (0 != inner.getInt16(s, num))
  {
    log_warning(TAG, "%s%s%s", n, " - Unable to extract ", s);
  }
}

void LOWIUtils::extractInt32(InPostcard& inner, const char *n, const char *s, int32& num)
{
  PostcardBase::INT32 num32 = 0;
  if (0 != inner.getInt32(s, num32))
  {
    log_warning(TAG, "%s%s%s", n, " - Unable to extract ", s);
  }
  num = num32;
}

void LOWIUtils::extractInt64(InPostcard& inner, const char *n, const char *s, INT64& num)
{
  num = 0;
  if (0 != inner.getInt64(s, num))
  {
    log_warning(TAG, "%s%s%s", n, " - Unable to extract ", s);
  }
}

void LOWIUtils::extractBool(InPostcard& inner, const char *n, const char *s, bool& num)
{
  num = false;
  if (0 != inner.getBool(s, num))
  {
    log_warning(TAG, "%s%s%s", n, " - Unable to extract ", s);
  }
}

LOWIRequest* LOWIUtils::inPostcardToRequest(InPostcard *const card)
{
  log_verbose(TAG, "inPostcardToRequest");
  LOWIRequest *request = NULL;
  bool success = true;
  const char *from = 0;

  do
  {
    // Check the postcard
    if (NULL == card)
    {
      log_error(TAG, "inPostcardToRequest - Card can not be null!");
      break;
    }

    const char *print_from = NOT_AVAILABLE;
    if (0 != card->getString("FROM", &from))
    {
      log_warning(TAG, "inPostcardToRequest - Unable to extract FROM");
    }
    else
    {
      print_from = from;
    }

    const char *print_to = NOT_AVAILABLE;
    const char *to = 0;
    if (0 != card->getString("TO", &to))
    {
      log_warning(TAG, "inPostcardToRequest - Unable to extract TO");
    }
    else
    {
      print_to = to;
    }

    const char *print_req = NOT_AVAILABLE;
    const char *req = 0;
    if (0 != card->getString("REQ", &req))
    {
      log_warning(TAG, "inPostcardToRequest - Unable to extract REQ");
    }
    else
    {
      print_req = req;
    }

    log_info(TAG, "inPostcardToRequest - FROM: %s, TO:   %s, REQ:  %s",
             print_from, print_to, print_req);

    // Request ID
    uint32 req_id = 0;
    extractUInt32(*card, "inPostcardToRequest", "REQ_ID", req_id);

    // Create the request
    if (0 == strcmp(req, "LOWI_DISCOVERY_SCAN"))
    {
      // Create the Request
      LOWIDiscoveryScanRequest *disc = new(std::nothrow) LOWIDiscoveryScanRequest(req_id);
      UTILS_BREAK_IF_NULL(disc, success, "inPostcardToRequest - Memory allocation failure!")

      if (false == parseDiscScanParams(req_id, *card, disc))
      {
        success = false;
        break;
      }
      request = disc;
    }
    else if (0 == strcmp(req, "LOWI_RANGING_SCAN"))
    {
      INT64 timeoutTimestamp = 0;
      uint8 rttReportType = 0;
      vector<LOWINodeInfo> vec;
      success = parseRangScanParams(req_id,
                                    timeoutTimestamp,
                                    rttReportType,
                                    *card,
                                    vec);

      // Create the Ranging scan request
      LOWIRangingScanRequest *rang = new(std::nothrow)
                                     LOWIRangingScanRequest(req_id, vec, timeoutTimestamp);
      UTILS_BREAK_IF_NULL(rang, success, "inPostcardToRequest - Memory allocation failure!")
      request = rang;
      rang->setReportType(LOWIUtils::to_eRttReportType(rttReportType));
    }
    else if (0 == strcmp(req, "LOWI_CAPABILITY"))
    {
      request = new(std::nothrow) LOWICapabilityRequest(req_id);
      UTILS_BREAK_IF_NULL(request, success, "inPostcardToRequest - Memory allocation failure!")
    }
    else if (0 == strcmp(req, "LOWI_RESET_CACHE"))
    {
      request = new(std::nothrow) LOWICacheResetRequest(req_id);
      UTILS_BREAK_IF_NULL(request, success, "inPostcardToRequest - Memory allocation failure!")
    }
    else if (0 == strcmp(req, "LOWI_ASYNC_DISCOVERY_SCAN_RESULTS"))
    {
      // Request timeout
      uint32 timeout = 0;
      extractUInt32(*card, "inPostcardToRequest", "REQ_TIMEOUT", timeout);

      log_debug(TAG, "inPostcardToRequest - Request id(%d) REQ_TIMEOUT(%d)",
                req_id, timeout);

      request = new(std::nothrow) LOWIAsyncDiscoveryScanResultRequest(req_id, timeout);
      UTILS_BREAK_IF_NULL(request, success, "inPostcardToRequest - Memory allocation failure!")
    }
    else if (0 == strcmp(req, "LOWI_PERIODIC_RANGING_SCAN"))
    {
      INT64 timeoutTimestamp = 0;
      uint8 rttReportType = 0;
      vector<LOWIPeriodicNodeInfo> vec;
      success = parsePeriodicRangScanParams(card,
                                            vec,
                                            req_id,
                                            timeoutTimestamp,
                                            rttReportType);

      // Create the Ranging scan request
      LOWIPeriodicRangingScanRequest *rang =
                                             new(std::nothrow) LOWIPeriodicRangingScanRequest(req_id, vec, timeoutTimestamp);

      UTILS_BREAK_IF_NULL(rang, success, "inPostcardToRequest - Memory allocation failure!")
      request = rang;
      rang->setReportType(LOWIUtils::to_eRttReportType(rttReportType));
    }
    else if (0 == strcmp(req, "CANCEL_RANGING_SCAN"))
    {
      // WiFi BSSIDs
      uint32 num_of_bssids = 0;
      extractUInt32(*card, "inPostcardToRequest", "NUM_OF_BSSIDS", num_of_bssids);

      log_debug(TAG, "inPostcardToRequest - Request id(%d) NUM_OF_BSSIDS(%u)",
                req_id, num_of_bssids);

      vector<LOWIMacAddress> vec;
      for (uint32 ii = 0; ii < num_of_bssids; ++ii)
      {
        // For each Node extract the card
        InPostcard *inner = 0;
        int err = card->getCard("WIFI_BSSID_CARD", &inner, ii);
        if (0 != err || NULL == inner)
        {
          // Unable to get card. break out of for loop.
          // vector will have less entries, which is probably fine
          log_error(TAG, "inPostcardToRequest - Unable to extract WIFI_BSSID_CARD");
          success = false;
          break;
        }

        vec.push_back(LOWIUtils::extractBssid(*inner));

        delete inner;
      }

      // Create the Ranging scan request
      LOWICancelRangingScanRequest *r = new(std::nothrow)
                                        LOWICancelRangingScanRequest(req_id, vec);
      UTILS_BREAK_IF_NULL(r, success, "inPostcardToRequest - Memory allocation failure!")
      request = r;
    }
    else if (0 == strcmp(req, "SET_LCI_INFORMATION"))
    {
      LOWILciInformation params;
      extractLciInfo(card, params, req_id);
      uint32 usageRules;
      extractUInt32(*card, "inPostcardToRequest", "USAGE_RULES", usageRules);

      LOWISetLCILocationInformation *r =
                                         new(std::nothrow) LOWISetLCILocationInformation(req_id, params, usageRules);
      UTILS_BREAK_IF_NULL(r, success, "inPostcardToRequest - Memory allocation failure!")
      request = r;
    }
    else if (0 == strcmp(req, "SET_LCR_INFORMATION"))
    {
      LOWILcrInformation params;
      extractLcrInfo(card, params, req_id);

      LOWISetLCRLocationInformation *r =
                                         new(std::nothrow) LOWISetLCRLocationInformation(req_id, params);
      UTILS_BREAK_IF_NULL(r, success, "inPostcardToRequest - Memory allocation failure!")
      request = r;
    }
    else if (0 == strcmp(req, "NEIGHBOR_REPORT"))
    {
      request = new(std::nothrow) LOWINeighborReportRequest(req_id);
      UTILS_BREAK_IF_NULL(request, success, "inPostcardToRequest - Memory allocation failure!")
    }
    else if (0 == strcmp(req, "SEND_LCI_REQUEST"))
    {
      LOWIMacAddress bssid = LOWIUtils::extractBssid(*card);
      request = new(std::nothrow) LOWISendLCIRequest(req_id, bssid);
      UTILS_BREAK_IF_NULL(request, success, "inPostcardToRequest - Memory allocation failure!")
    }
    else if (0 == strcmp(req, "FTM_RANGE_REQ"))
    {
      LOWIMacAddress bssid;
      uint16 randInterval = 0;
      vector<LOWIFTMRRNodeInfo> vec;
      extractFTMRRInfo(card, vec, bssid, randInterval);
      if (0 == vec.getNumOfElements())
      {
        success = false;
        log_error(TAG, "inPostcardToRequest - failed to extract FTMRR Info");
        break;
      }
      request = new(std::nothrow) LOWIFTMRangingRequest(req_id, bssid, randInterval, vec);
      UTILS_BREAK_IF_NULL(request, success, "inPostcardToRequest - Memory allocation failure!")
    }
  } while (0);

  if (false == success)
  {
    log_error(TAG, "inPostcardToRequest - Unable to create the Request from the Postcard");
    delete request;
    request = NULL;
  }
  else
  {
    // Set originator of the request
    if (NULL != request)
    {
      request->setRequestOriginator(from);
    }
  }
  return request;
}

bool LOWIUtils::parseDiscScanParams(uint32& req_id,
                                    InPostcard& card,
                                    LOWIDiscoveryScanRequest *disc)
{
  // Scan Type
  uint8 scan_type = 0;
  extractUInt8(card, "parseDiscScanParams", "SCAN_TYPE", scan_type);

  // Request Mode
  uint8 req_mode = 0;
  extractUInt8(card, "parseDiscScanParams", "REQUEST_MODE", req_mode);

  uint8 band = 0;
  extractUInt8(card, "parseDiscScanParams", "BAND", band);

  bool buffer_cache_bit = false;
  extractBool(card, "parseDiscScanParams", "BUFFER_CACHE_BIT", buffer_cache_bit);

  uint32 measAgeFilter = 0;
  extractUInt32(card, "parseDiscScanParams", "MEAS_AGE_FILTER", measAgeFilter);

  log_debug(TAG, "parseDiscScanParams - Request id = %d Scan Type(%d) REQUEST_MODE(%d) BAND(%d) BUFFER_CACHE_BIT(%d) MEAS_AGE_FILTER(%d)",
            req_id, scan_type, req_mode, band, buffer_cache_bit, measAgeFilter);

  uint32 num_of_channels = 0;
  extractUInt32(card, "parseDiscScanParams", "NUM_OF_CHANNELS", num_of_channels);

  vector<LOWIChannelInfo> vec;

  for (uint32 ii = 0; ii < num_of_channels; ++ii)
  {
    // For each Channel extract the card
    InPostcard *inner = 0;
    card.getCard("CHANNEL_CARD", &inner, ii);
    if (NULL == inner)
    {
      // Unable to allocate memory. break out of for loop.
      // vector will have less entries, which is probably fine
      log_error(TAG, "parseDiscScanParams - Memory allocation failure");
      return false;
    }

    uint32 freq = 0;
    extractUInt32(*inner, "parseDiscScanParams", "FREQUENCY", freq);
    log_debug(TAG, "parseDiscScanParams - FREQUENCY = %d", freq);

    LOWIChannelInfo ch(freq);
    vec.push_back(ch);

    delete inner;
  } // for

  // Fallback tolerance
  uint32 fbTolerance = 0;
  extractUInt32(card, "parseDiscScanParams", "FALLBACK_TOLERANCE", fbTolerance);

  // Request type
  uint8 requestType = 0;
  extractUInt8(card, "parseDiscScanParams", "REQUEST_TYPE", requestType);

  // Time out timestamp
  INT64 timeoutTimestamp = 0;
  extractInt64(card, "parseDiscScanParams", "REQ_TIMEOUT", timeoutTimestamp);

  log_debug(TAG, "parseDiscScanParams - FALLBACK_TOLERANCE(%d) REQUEST_TYPE(%d) REQ_TIMEOUT(%"PRId64")",
            fbTolerance, requestType, timeoutTimestamp);

  // populate the request
  disc->band = LOWIUtils::to_eBand(band);
  disc->bufferCacheRequest = buffer_cache_bit;
  disc->measAgeFilterSec = measAgeFilter;
  disc->fallbackToleranceSec = fbTolerance;
  disc->scanType = LOWIUtils::to_eScanType(scan_type);
  disc->requestMode = LOWIUtils::to_eRequestMode(req_mode);
  disc->chanInfo = vec;
  disc->timeoutTimestamp = timeoutTimestamp;

  return true;
}

bool LOWIUtils::parseRangScanParams(uint32& req_id,
                                    INT64& timeoutTimestamp,
                                    uint8& rttReportType,
                                    InPostcard& card,
                                    vector<LOWINodeInfo>& vec)
{
  // Timeout timestamp
  extractInt64(card, "parseRangScanParams", "REQ_TIMEOUT", timeoutTimestamp);
  // Ranging Scan Report Type
  extractUInt8(card, "parseRangScanParams", "RANGING_SCAN_REPORT_TYPE", rttReportType);

  // Wifi Nodes
  uint32 num_of_nodes = 0;
  extractUInt32(card, "parseRangScanParams", "NUM_OF_NODES", num_of_nodes);

  log_debug(TAG, "parseRangScanParams - Request id(%d) REQ_TIMEOUT(%"PRId64") RANGING_SCAN_REPORT_TYPE(%u), NUM_OF_NODES(%u)",
            req_id, timeoutTimestamp, rttReportType, num_of_nodes);

  for (uint32 ii = 0; ii < num_of_nodes; ++ii)
  {
    // For each Node extract the card
    InPostcard *inner = 0;
    int err = card.getCard("WIFI_NODE_CARD", &inner, ii);
    if (0 != err || NULL == inner)
    {
      // Unable to get card. break out of for loop.
      // vector will have less entries, which is probably fine
      log_error(TAG, "parseRangScanParams - Unable to extract WIFI_NODE_CARD");
      return false;
    }

    LOWINodeInfo info;

    info.bssid.setMac(LOWIUtils::extractBssid(*inner));

    extractUInt32(*inner, "parseRangScanParams", "FREQUENCY", info.frequency);

    extractUInt32(*inner, "parseRangScanParams", "BAND_CENTER_FREQ1", info.band_center_freq1);

    extractUInt32(*inner, "parseRangScanParams", "BAND_CENTER_FREQ2", info.band_center_freq2);

    unsigned char type = 0;
    extractUInt8(*inner, "parseRangScanParams", "NODE_TYPE", type);
    info.nodeType = LOWIUtils::to_eNodeType(type);

    uint32 spoof_lo = 0;
    extractUInt32(*inner, "parseRangScanParams", "SPOOF_MAC_ID_LO", spoof_lo);
    uint32 spoof_hi = 0;
    extractUInt32(*inner, "parseRangScanParams", "SPOOF_MAC_ID_HI", spoof_hi);
    info.spoofMacId.setMac(spoof_hi, spoof_lo);

    uint8 rttType = 0;
    extractUInt8(*inner, "parseRangScanParams", "RTT_TYPE", rttType);
    info.rttType = LOWIUtils::to_eRttType(rttType);

    uint8 ranging_bw = 0;
    extractUInt8(*inner, "parseRangScanParams", "RANGING_BW", ranging_bw);
    info.bandwidth = LOWIUtils::to_eRangingBandwidth(ranging_bw);

    uint8 ranging_preamble = 0;
    extractUInt8(*inner, "parseRangScanParams", "RANGING_PREAMBLE", ranging_preamble);
    info.preamble = LOWIUtils::to_eRangingPreamble(ranging_preamble);

    uint32 ftmRangingParameters = 0;
    extractUInt32(*inner, "parseRangScanParams", "FTM_RANGING_PARAMS", ftmRangingParameters);
    info.ftmRangingParameters = ftmRangingParameters;

    extractUInt8(*inner, "parseRangScanParams", "NUM_PKTS_PER_MEAS", info.num_pkts_per_meas);

    extractUInt8(*inner, "parsePeriodicRangScanParams", "NUM_RETRIES_PER_MEAS", info.num_retries_per_meas);

    log_debug(TAG, "parseRangScanParams - FREQUENCY(%d) BAND_CENTER_FREQ1(%d) BAND_CENTER_FREQ2(%d) NODE_TYPE(%d) RTT_TYPE(%u) RANGING_BW(%u) RANGING_PREAMBLE(%u) NUM_PKTS_PER_MEAS(%d) NUM_RETRIES_PER_MEAS(%d)",
              info.frequency, info.band_center_freq1, info.band_center_freq2, info.rttType, info.bandwidth, info.preamble, info.num_pkts_per_meas, info.num_retries_per_meas);

    vec.push_back(info);

    delete inner;
  }

  return true;
}

LOWIMacAddress LOWIUtils::extractBssid(InPostcard& inner)
{
  LOWIMacAddress bssid;

  unsigned int bssid_lo = 0;
  if (0 != inner.getUInt32("BSSID_LO", bssid_lo))
  {
    log_warning(TAG, "inPostcardToRequest - Unable to extract BSSID_LO");
  }
  unsigned int bssid_hi = 0;
  if (0 != inner.getUInt32("BSSID_HI", bssid_hi))
  {
    log_warning(TAG, "inPostcardToRequest - Unable to extract BSSID_HIGH");
  }

  bssid.setMac(bssid_hi, bssid_lo);

  return bssid;
}

void LOWIUtils::addBssidToCard(OutPostcard& card, const LOWIMacAddress& bssid)
{
  unsigned int mac = bssid.getLo24();
  card.addUInt32("BSSID_LO", mac);
  mac = bssid.getHi24();
  card.addUInt32("BSSID_HI", mac);
}

bool LOWIUtils::addFTMRRNodeToCard(OutPostcard& card, const LOWIFTMRRNodeInfo& node)
{
  bool result = false;
  do
  {
    // For each bssid create a OutPostcard
    OutPostcard *ftm_node_card = OutPostcard::createInstance();
    if (NULL == ftm_node_card)
    {
      // Unable to allocate memory for inner card
      // break out of for loop
      log_error(TAG, "addFTMRRNodeToCard - Mem allocation failure!");
      break;
    }
    ftm_node_card->init();
    addBssidToCard(*ftm_node_card, node.bssid);

    ftm_node_card->addUInt32("BSSID_INFO", node.bssidInfo);
    ftm_node_card->addUInt8("OPERATING_CLASS", node.operatingClass);
    ftm_node_card->addUInt8("BANDWIDTH", node.bandwidth);
    ftm_node_card->addUInt8("CENTER_CHANEL1", node.center_Ch1);
    ftm_node_card->addUInt8("CENTER_CHANEL2", node.center_Ch2);
    ftm_node_card->addUInt8("CHANEL", node.ch);
    ftm_node_card->addUInt8("PHY_TYPE", node.phyType);

    ftm_node_card->finalize();
    card.addCard("FTMRR_NODE_CARD", ftm_node_card);
    log_debug(TAG, "addFTMRRNodeToCard - Added a node card to the main card");
    delete ftm_node_card;
    result = true;
  } while (0);
  return result;
}

OutPostcard* LOWIUtils::responseToOutPostcard(LOWIResponse *resp,
                                              const char *to)
{
  OutPostcard *card = NULL;
  do
  {
    if (NULL == resp)
    {
      log_error(TAG, "responseToOutPostcard - Invalid argument!");
      break;
    }

    switch (resp->getResponseType())
    {
    case LOWIResponse::DISCOVERY_SCAN:
      {
        log_verbose(TAG, "responseToOutPostcard - DISCOVERY_SCAN");
        card = OutPostcard::createInstance();
        if (NULL == card) break;

        card->init();
        card->addString("RESP", "LOWI_DISCOVERY_SCAN");
        LOWIDiscoveryScanResponse *response =
                                              (LOWIDiscoveryScanResponse *)resp;

        card->addUInt8("SCAN_STATUS", response->scanStatus);
        card->addUInt8("SCAN_TYPE", response->scanTypeResponse);
        card->addInt64("PACKET_TIMESTAMP", response->timestamp);
        // Scan measurements available
        injectScanMeasurements(*card, response->scanMeasurements);
      }
      break;
    case LOWIResponse::RANGING_SCAN:
      {
        log_verbose(TAG, "responseToOutPostcard - RANGING_SCAN");
        card = OutPostcard::createInstance();
        if (NULL == card) break;

        card->init();
        card->addString("RESP", "LOWI_RANGING_SCAN");
        LOWIRangingScanResponse *response =
                                            (LOWIRangingScanResponse *)resp;

        card->addUInt8("SCAN_STATUS", response->scanStatus);

        // Scan measurements available
        injectScanMeasurements(*card, response->scanMeasurements);
      }
      break;
    case LOWIResponse::CAPABILITY:
      {
        log_verbose(TAG, "responseToOutPostcard - CAPABILITY");
        card = OutPostcard::createInstance();
        if (NULL == card) break;

        card->init();
        card->addString("RESP", "LOWI_CAPABILITY");
        LOWICapabilityResponse *response = (LOWICapabilityResponse *)resp;
        LOWICapabilities cap = response->getCapabilities();

        card->addBool("DISCOVERY_SCAN_SUPPORTED", cap.discoveryScanSupported);
        card->addBool("RANGING_SCAN_SUPPORTED", cap.rangingScanSupported);
        card->addBool("ACTIVE_SCAN_SUPPORTED", cap.activeScanSupported);
        card->addBool("SINGLE_SIDED_RANGING_SCAN_SUPPORTED", cap.oneSidedRangingSupported);
        card->addBool("DUAL_SIDED_RANGING_SCAN_SUPPORTED_11V", cap.dualSidedRangingSupported11v);
        card->addBool("DUAL_SIDED_RANGING_SCAN_SUPPORTED_11MC", cap.dualSidedRangingSupported11mc);
        card->addUInt8("BW_SUPPORT", cap.bwSupport);
        card->addUInt8("PREAMBLE_SUPPORT", cap.preambleSupport);
        card->addBool("CAPABILITY_STATUS", response->getStatus());
        card->addUInt32("SUPPORTED_CAPABILITY", cap.supportedCapablities);
      }
      break;
    case LOWIResponse::RESET_CACHE:
      {
        log_verbose(TAG, "responseToOutPostcard - RESET_CACHE");
        card = OutPostcard::createInstance();
        if (NULL == card) break;

        card->init();
        card->addString("RESP", "LOWI_RESET_CACHE");
        LOWICacheResetResponse *response = (LOWICacheResetResponse *)resp;
        card->addBool("CACHE_STATUS", response->getStatus());
      }
      break;
    case LOWIResponse::ASYNC_DISCOVERY_SCAN_RESULTS:
      {
        log_verbose(TAG, "responseToOutPostcard -"
                    " ASYNC_DISCOVERY_SCAN_RESULTS");
        card = OutPostcard::createInstance();
        if (NULL == card) break;

        card->init();
        card->addString("RESP", "LOWI_ASYNC_DISCOVERY_SCAN_RESULTS");
        LOWIAsyncDiscoveryScanResultResponse *response =
                                                         (LOWIAsyncDiscoveryScanResultResponse *)resp;

        card->addUInt8("SCAN_STATUS", response->scanStatus);
        card->addUInt8("SCAN_TYPE", response->scanTypeResponse);
        card->addInt64("PACKET_TIMESTAMP", response->timestamp);
        // Scan measurements available
        injectScanMeasurements(*card, response->scanMeasurements);
      }
      break;
    case LOWIResponse::LOWI_STATUS:
      {
        log_verbose(TAG, "responseToOutPostcard -"
                    " LOWI_STATUS");
        card = OutPostcard::createInstance();
        if (NULL == card) break;

        card->init();
        card->addString("RESP", "LOWI_STATUS");
        LOWIStatusResponse *response = (LOWIStatusResponse *)resp;

        card->addUInt8("LOWI_STATUS", response->scanStatus);
        card->addUInt8("REQ_TYPE", response->mRequestType);
      }
      break;
    default:
      log_error(TAG, "responseToOutPostcard - Unknown Type!");
      break;
    }
  } while (0);

  if (NULL == card)
  {
    log_error(TAG, "responseToOutPostcard - Unable to create the post card");
  }
  else
  {
    // Common to all type of requests
    card->addString("TO", to);
    card->addString("FROM", SERVER_NAME);
    card->addUInt32("REQ_ID", resp->getRequestId());
    // Add TX-ID of type int32 as a standard field in postcard
    card->addInt32("TX-ID", resp->getRequestId());
    card->finalize();
  }

  return card;
}

bool LOWIUtils::injectScanMeasurements(OutPostcard& card,
                                       vector<LOWIScanMeasurement*>& meas)
{
  log_verbose(TAG, "injectScanMeasurements");
  bool retVal = false;
  do
  {
    uint32 num_of_scans = meas.getNumOfElements();

    log_debug(TAG, "injectScanMeasurements - num of scans = %d",
              num_of_scans);
    card.addUInt32("NUM_OF_SCANS", num_of_scans);

    // For each scan measurement - insert a Post card
    for (uint32 ii = 0; ii < num_of_scans; ++ii)
    {
      OutPostcard *scan_card = OutPostcard::createInstance();
      if (NULL == scan_card)
      {
        log_error(TAG, "injectScanMeasurements - Memory allocation failure!");
        break;
      }

      scan_card->init();

      if (NULL == meas[ii])
      {
        log_debug(TAG, "injectScanMeasurements - NULL(%u)", ii);
        break;
      }
      meas[ii]->bssid.print();

      unsigned int bssid = meas[ii]->bssid.getLo24();
      scan_card->addUInt32("BSSID_LO", bssid);

      bssid = meas[ii]->bssid.getHi24();
      scan_card->addUInt32("BSSID_HI", bssid);

      unsigned int freq = meas[ii]->frequency;
      scan_card->addUInt32("FREQUENCY", freq);

      scan_card->addBool("IS_SECURE", meas[ii]->isSecure);

      unsigned char temp = meas[ii]->type;
      scan_card->addUInt8("NODE_TYPE", temp);

      unsigned char rttType = meas[ii]->rttType;
      scan_card->addUInt8("RTT_TYPE", rttType);

      unsigned char ssid[SSID_LEN];
      int ssid_len = 0;
      meas[ii]->ssid.getSSID(ssid, &ssid_len);
      const void *blob = (const void *)(ssid);
      size_t length = (ssid_len >= 0) ? ssid_len : 0;
      // Insert the SSID as a blob only if the SSID is valid or has the
      // length > 0. Else prints out an error message in Post card implementation.
      if ((meas[ii]->ssid.isSSIDValid() == true) && (0 < length))
      {
        scan_card->addBlob("SSID", blob, length);
      }
      else
      {
        log_verbose(TAG, "injectScanMeasurements - Skip injecting SSID"
                    " to the postcard");
      }

      if (NULL != meas[ii]->msapInfo)
      {
        scan_card->addUInt8("MSAP_PROT_VER",
                            meas[ii]->msapInfo->protocolVersion);
        scan_card->addUInt32("MSAP_VENUE_HASH",
                             meas[ii]->msapInfo->venueHash);
        scan_card->addUInt8("MSAP_SERVER_IDX",
                            meas[ii]->msapInfo->serverIdx);
      }

      // Inject cell power
      scan_card->addInt8("CELL_POWER", meas[ii]->cellPowerLimitdBm);

      // Inject Country code
      scan_card->addArrayUInt8("COUNTRY_CODE", LOWI_COUNTRY_CODE_LEN, meas[ii]->country_code);

      // Inject Indoor / Outdoor
      scan_card->addUInt8("INDOOR_OUTDOOR", meas[ii]->indoor_outdoor);

      // Inject the measurement number
      scan_card->addUInt32("MEASUREMENT_NUM", meas[ii]->measurementNum);

      //inject the RTT target status code
      log_verbose(TAG, "RTT_TARGET_STATUS being injected(%u)", (uint32)meas[ii]->targetStatus);
      scan_card->addUInt32("RTT_TARGET_STATUS", (uint32)meas[ii]->targetStatus);

      // Inject the beacon period
      scan_card->addUInt16("BEACON_PERIOD", meas[ii]->beaconPeriod);

      // Inject the beacon capabilities
      scan_card->addUInt16("BEACON_CAPS", meas[ii]->beaconCaps);

      // Inject the ie data
      injectIeData(*scan_card, meas[ii]->ieData);

      // Inject the Number of RTT frames attempted.
      scan_card->addUInt16("NUM_RTT_FRAMES_ATTEMPTED", meas[ii]->num_frames_attempted);
      // Inject the actual time taken to complete rtt measurement.
      scan_card->addUInt16("ACTUAL_BURST_DURATION", meas[ii]->actual_burst_duration);
      // Inject FTM frames per burst negotiated with target.
      scan_card->addUInt8("NEGOTIATED_NUM_FRAMES_PER_BURST", meas[ii]->negotiated_num_frames_per_burst);
      // Inject the time after which FTM session can be retried.
      scan_card->addUInt8("RETRY_RTT_AFTER_DURATION", meas[ii]->retry_after_duration);
      // Inject number of FTM bursts negotiated with the target.
      scan_card->addUInt8("NEGOTIATED_BURST_EXPONENT", meas[ii]->negotiated_burst_exp);

      if (NULL != meas[ii]->lciInfo)
      {
        scan_card->addUInt8("LCI_INFO_ID", meas[ii]->lciInfo->id);
        scan_card->addUInt8("LCI_INFO_LEN", meas[ii]->lciInfo->len);
        injectLocationIeData(*scan_card, meas[ii]->lciInfo->locData,
                             meas[ii]->lciInfo->len, "LCI");
      }

      if (NULL != meas[ii]->lcrInfo)
      {
        scan_card->addUInt8("LCR_INFO_ID", meas[ii]->lcrInfo->id);
        scan_card->addUInt8("LCR_INFO_LEN", meas[ii]->lcrInfo->len);
        injectLocationIeData(*scan_card, meas[ii]->lcrInfo->locData,
                             meas[ii]->lcrInfo->len, "LCR");
      }

      // Inject measurement info
      injectMeasurementInfo(*scan_card, meas[ii]->measurementsInfo);

      scan_card->finalize();

      card.addCard("SCAN_MEAS_CARD", scan_card);
      delete scan_card;
      retVal = true;
    }
  } while (0);

  return retVal;
}

bool LOWIUtils::injectMeasurementInfo(OutPostcard& card,
                                      vector<LOWIMeasurementInfo*>& info)
{
  bool retVal = false;
  do
  {
    uint32 num_of_meas = info.getNumOfElements();
    log_verbose(TAG, "injectMeasurementInfo");

    card.addUInt32("NUM_OF_MEAS", num_of_meas);
    log_verbose(TAG, "injectScanMeasurements - NUM_OF_MEAS(%u)", num_of_meas);

    // For each measurement info - insert a Post card
    for (uint32 ii = 0; ii < num_of_meas; ++ii)
    {
      OutPostcard *meas_card = OutPostcard::createInstance();
      if (NULL == meas_card)
      {
        log_error(TAG, "injectMeasurementInfo - Memory allocation failure!");
        break;
      }

      meas_card->init();

      meas_card->addInt64("RSSI_TIMESTAMP", info[ii]->rssi_timestamp);
      meas_card->addInt16("RSSI", info[ii]->rssi);
      meas_card->addInt32("MEAS_AGE", info[ii]->meas_age);
      meas_card->addInt64("RTT_TIMESTAMP", info[ii]->rtt_timestamp);
      meas_card->addInt32("RTT_PS", info[ii]->rtt_ps);
      meas_card->addUInt8("TX_PREAMBLE", info[ii]->tx_preamble);
      meas_card->addUInt8("TX_NSS", info[ii]->tx_nss);
      meas_card->addUInt8("TX_BW", info[ii]->tx_bw);
      meas_card->addUInt8("TX_MCS_IDX", info[ii]->tx_mcsIdx);
      meas_card->addUInt32("TX_BIT_RATE", info[ii]->tx_bitrate);
      meas_card->addUInt8("RX_PREAMBLE", info[ii]->rx_preamble);
      meas_card->addUInt8("RX_NSS", info[ii]->rx_nss);
      meas_card->addUInt8("RX_BW", info[ii]->rx_bw);
      meas_card->addUInt8("RX_MCS_IDX", info[ii]->rx_mcsIdx);
      meas_card->addUInt32("RX_BIT_RATE", info[ii]->rx_bitrate);

      meas_card->finalize();

      card.addCard("Measurement_card", meas_card);
      delete meas_card;
      retVal = true;
    }
  } while (0);
  return retVal;
}


bool LOWIUtils::injectIeData(OutPostcard& card,
                             vector<int8>& info)
{
  bool retVal = false;
  do
  {
    uint32 num_of_meas = info.getNumOfElements();
    log_verbose(TAG, "injectIeData");

    card.addUInt32("NUM_OF_IE", num_of_meas);
    log_debug(TAG, "@injectIeData() -- NUM_OF_IE(%u)", num_of_meas);

    // For each Information Element - insert a Post card
    for (uint32 ii = 0; ii < num_of_meas; ++ii)
    {
      OutPostcard *ie_data_card = OutPostcard::createInstance();
      if (NULL == ie_data_card)
      {
        log_error(TAG, "injectIeData - Memory allocation failure!");
        break;
      }

      ie_data_card->init();

      ie_data_card->addInt8("IE_DATA", info[ii]);

      ie_data_card->finalize();

      card.addCard("IE_data_card", ie_data_card);
      delete ie_data_card;
      retVal = true;
    }
  } while (0);
  return retVal;
}

bool LOWIUtils::injectLocationIeData(OutPostcard& card, uint8 *info, uint8 len, char const *type)
{
  bool retVal = false;
  do
  {
    log_verbose(TAG, "injectLocationIeData");

    // Create a card name
    char cardName[LOCATION_IE_DATA_CARD_LEN] = {0};
    snprintf(cardName, sizeof(cardName), "%s%s", "LOCATION_IE_DATA_CARD_", type);

    OutPostcard *location_ie_data_card = OutPostcard::createInstance();
    if (NULL == location_ie_data_card)
    {
      log_error(TAG, "injectLocationIeData - Memory allocation failure!");
      break;
    }

    location_ie_data_card->init();
    location_ie_data_card->addArrayUInt8(cardName, len, info);
    location_ie_data_card->finalize();

    card.addCard(cardName, location_ie_data_card);
    delete location_ie_data_card;
  } while (0);
  return retVal;
}

////////////////////////////////////////////////////////
// Other util functions
////////////////////////////////////////////////////////
LOWIResponse::eResponseType
LOWIUtils::to_eResponseType(int a)
{
  switch (a)
  {
  case 1:
    return LOWIResponse::DISCOVERY_SCAN;
  case 2:
    return LOWIResponse::RANGING_SCAN;
  case 3:
    return LOWIResponse::CAPABILITY;
  case 4:
    return LOWIResponse::RESET_CACHE;
  case 5:
    return LOWIResponse::ASYNC_DISCOVERY_SCAN_RESULTS;
  default:
    log_warning(TAG, "to_eResponseType - default case");
    return LOWIResponse::RESPONSE_TYPE_UNKNOWN;
  }
}

LOWIResponse::eScanStatus
LOWIUtils::to_eScanStatus(int a)
{
  switch (a)
  {
  case 1:
    return LOWIResponse::SCAN_STATUS_SUCCESS;
  case 2:
    return LOWIResponse::SCAN_STATUS_BUSY;
  case 3:
    return LOWIResponse::SCAN_STATUS_DRIVER_ERROR;
  case 4:
    return LOWIResponse::SCAN_STATUS_DRIVER_TIMEOUT;
  case 5:
    return LOWIResponse::SCAN_STATUS_INTERNAL_ERROR;
  case 6:
    return LOWIResponse::SCAN_STATUS_INVALID_REQ;
  case 7:
    return LOWIResponse::SCAN_STATUS_NOT_SUPPORTED;
  case 8:
    return LOWIResponse::SCAN_STATUS_NO_WIFI;
  default:
    log_warning(TAG, "to_eScanStatus - default case");
    return LOWIResponse::SCAN_STATUS_UNKNOWN;
  }
}

LOWIDiscoveryScanResponse::eScanTypeResponse
LOWIUtils::to_eScanTypeResponse(int a)
{
  switch (a)
  {
  case 1:
    return LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_PASSIVE;
  case 2:
    return LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_ACTIVE;
  default:
    log_warning(TAG, "to_eScanTypeResponse - default case");
    return LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_UNKNOWN;
  }
}

eNodeType LOWIUtils::to_eNodeType(int a)
{
  switch (a)
  {
  case 1:
    return ACCESS_POINT;
  case 2:
    return PEER_DEVICE;
  case 3:
    return NAN_DEVICE;
  case 4:
    return STA_DEVICE;
  case 5:
    return SOFT_AP;
  default:
    log_verbose(TAG, "to_eNodeType - default case");
    return NODE_TYPE_UNKNOWN;
  }
}

eRttType LOWIUtils::to_eRttType(uint8 a)
{
  switch (a)
  {
  case 0:
    return RTT1_RANGING;
  case 1:
    return RTT2_RANGING;
  case 2:
    return RTT3_RANGING;
  default:
    log_verbose(TAG, "to_eRttType - default case - RTT2_RANGING");
    return RTT2_RANGING;
  }
}

eRttReportType LOWIUtils::to_eRttReportType(uint8 a)
{
  switch (a)
  {
  case 0:
    return RTT_REPORT_1_FRAME_CFR;
  case 1:
    return RTT_REPORT_1_FRAME_NO_CFR;
  case 2:
    return RTT_REPORT_AGGREGATE;
  default:
    log_verbose(TAG, "to_eRttReportType - default case - RTT_REPORT_AGGREGATE");
    return RTT_REPORT_AGGREGATE;
  }
}

eRangingBandwidth LOWIUtils::to_eRangingBandwidth(uint8 a)
{
  switch (a)
  {
  case 0:
    return BW_20MHZ;
  case 1:
    return BW_40MHZ;
  case 2:
    return BW_80MHZ;
  case 3:
    return BW_160MHZ;
  default:
    log_verbose(TAG, "to_eRangingBandwidth - default case");
    return BW_20MHZ;
  }
}

eRangingPreamble LOWIUtils::to_eRangingPreamble(uint8 a)
{
  switch (a)
  {
  case 0:
    return RTT_PREAMBLE_LEGACY;
  case 1:
    return RTT_PREAMBLE_HT;
  case 2:
    return RTT_PREAMBLE_VHT;
  default:
    log_verbose(TAG, "to_eRangingPreamble - default case");
    return RTT_PREAMBLE_LEGACY;
  }
}

LOWIDiscoveryScanRequest::eBand LOWIUtils::to_eBand(int a)
{
  switch (a)
  {
  case 0:
    return LOWIDiscoveryScanRequest::TWO_POINT_FOUR_GHZ;
  case 1:
    return LOWIDiscoveryScanRequest::FIVE_GHZ;
  default:
    return LOWIDiscoveryScanRequest::BAND_ALL;
  }
}

LOWIDiscoveryScanRequest::eScanType LOWIUtils::to_eScanType(int a)
{
  switch (a)
  {
  case 1:
    return LOWIDiscoveryScanRequest::ACTIVE_SCAN;
  default:
    return LOWIDiscoveryScanRequest::PASSIVE_SCAN;
  }
}

LOWIDiscoveryScanRequest::eRequestMode LOWIUtils::to_eRequestMode(int a)
{
  switch (a)
  {
  case 1:
    return LOWIDiscoveryScanRequest::NORMAL;
  case 2:
    return LOWIDiscoveryScanRequest::CACHE_ONLY;
  case 3:
    return LOWIDiscoveryScanRequest::CACHE_FALLBACK;
  default:
    return LOWIDiscoveryScanRequest::FORCED_FRESH;
  }
}

qc_loc_fw::ERROR_LEVEL LOWIUtils::to_logLevel(int a)
{
  switch (a)
  {
  case 0:
    return EL_LOG_OFF;
  case 1:
    return EL_ERROR;
  case 2:
    return EL_WARNING;
  case 3:
    return EL_INFO;
  case 4:
    return EL_DEBUG;
  case 5:
    return EL_VERBOSE;
  case 100:
    // Fallback
  default:
    return EL_LOG_ALL;
  }
}

eLowiMotionPattern LOWIUtils::to_eLOWIMotionPattern(uint8 motion)
{
  switch (motion)
  {
  case 0:
    return qc_loc_fw::LOWI_MOTION_NOT_EXPECTED;
  case 1:
    return qc_loc_fw::LOWI_MOTION_EXPECTED;
  case 2:
    return qc_loc_fw::LOWI_MOTION_UNKNOWN;
  default:
    return qc_loc_fw::LOWI_MOTION_UNKNOWN;
  }
}

char const* LOWIUtils::to_string(LOWIRequest::eRequestType a)
{
  switch (a)
  {
  case LOWIRequest::DISCOVERY_SCAN:
    return "DISCOVERY_SCAN";
  case LOWIRequest::RANGING_SCAN:
    return "RANGING_SCAN";
  case LOWIRequest::CAPABILITY:
    return "CAPABILITY";
  case LOWIRequest::RESET_CACHE:
    return "RESET_CACHE";
  case LOWIRequest::ASYNC_DISCOVERY_SCAN_RESULTS:
    return "ASYNC_DISCOVERY_SCAN_RESULTS";
  case LOWIRequest::PERIODIC_RANGING_SCAN:
    return "PERIODIC_RANGING_SCAN";
  case LOWIRequest::CANCEL_RANGING_SCAN:
    return "CANCEL_RANGING_SCAN";
  case LOWIRequest::SET_LCI_INFORMATION:
    return "SET_LCI_INFORMATION";
  case LOWIRequest::NEIGHBOR_REPORT:
    return "NEIGHBOR_REPORT";
  case LOWIRequest::SET_LCR_INFORMATION:
    return "SET_LCR_INFORMATION";
  case LOWIRequest::SEND_LCI_REQUEST:
    return "SEND_LCI_REQUEST";
  case LOWIRequest::FTM_RANGE_REQ:
    return "FTM_RANGE_REQ";
  case LOWIRequest::LOWI_INTERNAL_MESSAGE:
    return "LOWI_INTERNAL_MESSAGE";
  default:
    return "Unknown request";
  }
}

char const* LOWIUtils::to_string(LOWIResponse::eResponseType a)
{
  switch (a)
  {
  case LOWIResponse::RESPONSE_TYPE_UNKNOWN:
    return "RESPONSE_TYPE_UNKNOWN";
  case LOWIResponse::DISCOVERY_SCAN:
    return "DISCOVERY_SCAN";
  case LOWIResponse::RANGING_SCAN:
    return "RANGING_SCAN";
  case LOWIResponse::CAPABILITY:
    return "CAPABILITY";
  case LOWIResponse::RESET_CACHE:
    return "RESET_CACHE";
  case LOWIResponse::ASYNC_DISCOVERY_SCAN_RESULTS:
    return "ASYNC_DISCOVERY_SCAN_RESULTS";
  case LOWIResponse::LOWI_STATUS:
    return "LOWI_STATUS";
  default:
    return "Unknown response";
  }
}

char const* LOWIUtils::to_string(LOWIResponse::eScanStatus a)
{
  switch (a)
  {
  case LOWIResponse::SCAN_STATUS_SUCCESS:
    return "SCAN_STATUS_SUCCESS";
  case LOWIResponse::SCAN_STATUS_BUSY:
    return "SCAN_STATUS_BUSY";
  case LOWIResponse::SCAN_STATUS_DRIVER_ERROR:
    return "SCAN_STATUS_DRIVER_ERROR";
  case LOWIResponse::SCAN_STATUS_DRIVER_TIMEOUT:
    return "SCAN_STATUS_DRIVER_TIMEOUT";
  case LOWIResponse::SCAN_STATUS_INTERNAL_ERROR:
    return "SCAN_STATUS_INTERNAL_ERROR";
  case LOWIResponse::SCAN_STATUS_INVALID_REQ:
    return "SCAN_STATUS_INVALID_REQ";
  case LOWIResponse::SCAN_STATUS_NOT_SUPPORTED:
    return "SCAN_STATUS_NOT_SUPPORTED";
  case LOWIResponse::SCAN_STATUS_NO_WIFI:
    return "SCAN_STATUS_NO_WIFI";
  case LOWIResponse::SCAN_STATUS_TOO_MANY_REQUESTS:
    return "SCAN_STATUS_TOO_MANY_REQUESTS";
  case LOWIResponse::SCAN_STATUS_OUT_OF_MEMORY:
    return "SCAN_STATUS_OUT_OF_MEMORY";
  default:
    return "SCAN_STATUS_UNKNOWN";
  }
}

char const* LOWIUtils::to_string(size_t val, const char *arr[], size_t arr_size)
{
  static const char LOWI_UNKNOWN_STR[] = "Unknown";
  return (val < arr_size ? arr[val] : LOWI_UNKNOWN_STR);
}

LOWIResponse::eScanStatus LOWIUtils::to_eLOWIDriverStatus(uint8 err)
{
  switch (err)
  {
  case 0:
    return LOWIResponse::SCAN_STATUS_UNKNOWN;
  case 1:
    return LOWIResponse::SCAN_STATUS_SUCCESS;
  case 2:
    return LOWIResponse::SCAN_STATUS_BUSY;
  case 3:
    return LOWIResponse::SCAN_STATUS_DRIVER_ERROR;
  case 4:
    return LOWIResponse::SCAN_STATUS_DRIVER_TIMEOUT;
  case 5:
    return LOWIResponse::SCAN_STATUS_INTERNAL_ERROR;
  case 6:
    return LOWIResponse::SCAN_STATUS_INVALID_REQ;
  case 7:
    return LOWIResponse::SCAN_STATUS_NOT_SUPPORTED;
  case 8:
    return LOWIResponse::SCAN_STATUS_NO_WIFI;
  case 12:
    return LOWIResponse::SCAN_STATUS_TOO_MANY_REQUESTS;
  case 13:
    return LOWIResponse::SCAN_STATUS_OUT_OF_MEMORY;
  default:
    return LOWIResponse::SCAN_STATUS_UNKNOWN;
  }
}

int64 LOWIUtils::currentTimeMs()
{
  struct timeval      present_time;
  int64              current_time_msec = 0;

  // present time: seconds, and microseconds
  if(0 == gettimeofday(&present_time, NULL))
  {

    // Calculate absolute expire time (to avoid data overflow)
    current_time_msec = present_time.tv_sec;
    current_time_msec *= 1000;  // convert to milli-seconds

    // covert the micro-seconds portion to milliseconds
    current_time_msec += (present_time.tv_usec + 500) / 1000;
  }
  return current_time_msec;
}

uint32 LOWIUtils::freqToChannel(uint32 freq)
{
  switch (freq)
  {
    /*2.4 GHz Band*/
  case 2412:
    return 1;
  case 2417:
    return 2;
  case 2422:
    return 3;
  case 2427:
    return 4;
  case 2432:
    return 5;
  case 2437:
    return 6;
  case 2442:
    return 7;
  case 2447:
    return 8;
  case 2452:
    return 9;
  case 2457:
    return 10;
  case 2462:
    return 11;
  case 2467:
    return 12;
  case 2472:
    return 13;
  case 2484:
    return 14;
    /*5 GHz Band*/
  case 5170:
    return 34;
  case 5180:
    return 36;
  case 5190:
    return 38;
  case 5200:
    return 40;
  case 5210:
    return 42;
  case 5220:
    return 44;
  case 5230:
    return 46;
  case 5240:
    return 48;
  case 5260:
    return 52;
  case 5280:
    return 56;
  case 5300:
    return 60;
  case 5320:
    return 64;
  case 5500:
    return 100;
  case 5520:
    return 104;
  case 5540:
    return 108;
  case 5560:
    return 112;
  case 5580:
    return 116;
  case 5600:
    return 120;
  case 5620:
    return 124;
  case 5640:
    return 128;
  case 5660:
    return 132;
  case 5680:
    return 136;
  case 5700:
    return 140;
  case 5745:
    return 149;
  case 5765:
    return 153;
  case 5785:
    return 157;
  case 5805:
    return 161;
  case 5825:
    return 165;
    /** Other than 2.4 / 5 Ghz band */
  default:
    return 0;
  }
}

LOWIDiscoveryScanRequest::eBand LOWIUtils::freqToBand(uint32 freq)
{
  if (2412 <= freq && 2484 >= freq)
  {
    return LOWIDiscoveryScanRequest::TWO_POINT_FOUR_GHZ;
  }
  else if (5170 <= freq && 5825 >= freq)
  {
    return LOWIDiscoveryScanRequest::FIVE_GHZ;
  }
  return LOWIDiscoveryScanRequest::BAND_ALL;
}

uint32 LOWIUtils::channelBandToFreq(uint32 channel,
                                    LOWIDiscoveryScanRequest::eBand band)
{
  // Ignoring the band here as there is no known instance of the
  // collision between the band and channel as of now.
  log_verbose(TAG, "channelBandToFreq - Channel = %d", channel);
  switch (channel)
  {
    /*2.4 GHz Band*/
  case 1:
    return 2412;
  case 2:
    return 2417;
  case 3:
    return 2422;
  case 4:
    return 2427;
  case 5:
    return 2432;
  case 6:
    return 2437;
  case 7:
    return 2442;
  case 8:
    return 2447;
  case 9:
    return 2452;
  case 10:
    return 2457;
  case 11:
    return 2462;
  case 12:
    return 2467;
  case 13:
    return 2472;
  case 14:
    return 2484;
    /*5 GHz Band*/
  case 34:
    return 5170;
  case 36:
    return 5180;
  case 38:
    return 5190;
  case 40:
    return 5200;
  case 42:
    return 5210;
  case 44:
    return 5220;
  case 46:
    return 5230;
  case 48:
    return 5240;
  case 52:
    return 5260;
  case 56:
    return 5280;
  case 60:
    return 5300;
  case 64:
    return 5320;
  case 100:
    return 5500;
  case 104:
    return 5520;
  case 108:
    return 5540;
  case 112:
    return 5560;
  case 116:
    return 5580;
  case 120:
    return 5600;
  case 124:
    return 5620;
  case 128:
    return 5640;
  case 132:
    return 5660;
  case 136:
    return 5680;
  case 140:
    return 5700;
  case 149:
    return 5745;
  case 153:
    return 5765;
  case 157:
    return 5785;
  case 161:
    return 5805;
  case 165:
    return 5825;
    /** Other than 2.4 / 5 Ghz band */
  default:
    {
      log_error(TAG, "channelBandToFreq - Invalid channel = %d", channel);
      return 0;
    }
  }
}

int* LOWIUtils::getChannelsOrFreqs(LOWIDiscoveryScanRequest::eBand band,
                                   unsigned char& num_channels, bool freq)
{
  int *chan_freq = NULL;
  switch (band)
  {
  case LOWIDiscoveryScanRequest::TWO_POINT_FOUR_GHZ:
    {
      num_channels = sizeof(channelArr_2_4_ghz) / sizeof(channelArr_2_4_ghz[0]);
      chan_freq = new(std::nothrow) int[num_channels];
      if (NULL != chan_freq)
      {
        for (unsigned char ii = 0; ii < num_channels; ++ii)
        {
          if (true == freq)
          {
            chan_freq[ii] = freqArr_2_4_ghz[ii];
          }
          else
          {
            chan_freq[ii] = channelArr_2_4_ghz[ii];
          }
        }
      }
      break;
    }
  case LOWIDiscoveryScanRequest::FIVE_GHZ:
    {
      num_channels = sizeof(channelArr_5_ghz) / sizeof(channelArr_5_ghz[0]);
      chan_freq = new(std::nothrow) int[num_channels];
      if (NULL != chan_freq)
      {
        for (unsigned char ii = 0; ii < num_channels; ++ii)
        {
          if (true == freq)
          {
            chan_freq[ii] = freqArr_5_ghz[ii];
          }
          else
          {
            chan_freq[ii] = channelArr_5_ghz[ii];
          }
        }
      }
      break;
    }
  default:
    // All
    {
      num_channels = sizeof(channelArr_2_4_ghz) / sizeof(channelArr_2_4_ghz[0]);
      num_channels += sizeof(channelArr_5_ghz) / sizeof(channelArr_5_ghz[0]);
      chan_freq = new(std::nothrow) int[num_channels];
      if (NULL != chan_freq)
      {
        // First copy the 2.4 Ghz freq / channels
        unsigned int index = 0;
        for (; index < (sizeof(channelArr_2_4_ghz) / sizeof(channelArr_2_4_ghz[0]));
             ++index)
        {
          if (true == freq)
          {
            chan_freq[index] = freqArr_2_4_ghz[index];
          }
          else
          {
            chan_freq[index] = channelArr_2_4_ghz[index];
          }
        }
        // Copy the 5 Ghz freq / channels
        for (unsigned int ii = 0;
             ii < (sizeof(channelArr_5_ghz) / sizeof(channelArr_5_ghz[0])); ++ii)
        {
          if (true == freq)
          {
            chan_freq[index + ii] = freqArr_5_ghz[ii];
          }
          else
          {
            chan_freq[index + ii] = channelArr_5_ghz[ii];
          }
        }
      }
      break;
    }
  }
  return chan_freq;
}

int* LOWIUtils::getChannelsOrFreqs(vector<LOWIChannelInfo>& v,
                                   unsigned char& num_channels, bool freq)
{
  int *chan_freq = NULL;
  num_channels = v.getNumOfElements();
  chan_freq = new(std::nothrow) int[num_channels];
  if (NULL != chan_freq)
  {
    for (int ii = 0; ii < num_channels; ++ii)
    {
      if (true == freq)
      {
        chan_freq[ii] = v[ii].getFrequency();
      }
      else
      {
        chan_freq[ii] = v[ii].getChannel();
      }
    }
  }
  return chan_freq;
}

void LOWIUtils::extractLciInfo(InPostcard *const card,
                               LOWILciInformation& params,
                               uint32& req_id)
{
  extractInt64(*card, "inPostcardToRequest", "LATITUDE", (INT64&)params.latitude);
  extractInt64(*card, "inPostcardToRequest", "LONGITUDE", (INT64&)params.longitude);
  extractInt32(*card, "inPostcardToRequest", "ALTITUDE", params.altitude);
  extractUInt8(*card, "inPostcardToRequest", "LATITUDE_UNC", params.latitude_unc);
  extractUInt8(*card, "inPostcardToRequest", "LONGITUDE_UNC", params.longitude_unc);
  extractUInt8(*card, "inPostcardToRequest", "ALTITUDE_UNC", params.altitude_unc);
  uint8 motion;
  extractUInt8(*card, "inPostcardToRequest", "FLUSH", motion);
  params.motion_pattern = LOWIUtils::to_eLOWIMotionPattern(motion);
  extractInt32(*card, "inPostcardToRequest", "FLOOR", params.floor);
  extractInt32(*card, "inPostcardToRequest", "HEIGHT_ABOVE_FLOOR", params.height_above_floor);
  extractInt32(*card, "inPostcardToRequest", "HEIGHT_UNC", params.height_unc);
  log_debug(TAG, "inPostcardToRequest - Request id(%d) LATITUDE(%"PRId64") LONGITUDE(%"PRId64"), ALTITUDE(%d), LATITUDE_UNC(%u) LONGITUDE_UNC(%u), ALTITUDE_UNC(%u), MOTION(%u), FLOOR(%d), HEIGHT_ABOVE(%d), HEIGHT_UNC(%d)",
            req_id, params.latitude, params.longitude, params.altitude, params.latitude_unc, params.longitude_unc, params.altitude_unc, params.motion_pattern, params.floor, params.height_above_floor, params.height_unc);
}

void LOWIUtils::extractLcrInfo(InPostcard *const card,
                               LOWILcrInformation& params,
                               uint32& req_id)
{
  // extract the country code
  int num_elements = LOWI_COUNTRY_CODE_LEN;
  memset(&params.country_code, 0, LOWI_COUNTRY_CODE_LEN);

  if (0 != card->getArrayUInt8("LCR_COUNTRY_CODE", &num_elements, params.country_code))
  {
    log_warning(TAG, "inPostcardToRequest - Unable to extract COUNTRY_CODE");
  }
  else
  {
    log_debug(TAG, "LCR_COUNTRY_CODE is %c%c", (char)params.country_code[0], (char)params.country_code[1]);
  }

  // extract the length of the info field
  uint32 length;
  extractUInt32(*card, "inPostcardToRequest", "LCR_LENGTH", params.length);
  log_debug(TAG, "inPostcardToRequest - Request id(%d) LCR_LENGTH(%u)",
            req_id, params.length);

  // extract the civic information
  num_elements = CIVIC_INFO_LEN;
  memset(&params.civic_info, 0, CIVIC_INFO_LEN);

  if (0 != card->getArrayInt8("LCR_CIVIC_INFO", &num_elements, params.civic_info))
  {
    log_warning(TAG, "inPostcardToRequest - Unable to extract LCR_CIVIC_INFO");
  }
  else
  {
    for (uint32 ii = 0; ii < params.length; ++ii)
    {
      log_debug(TAG, "LCR_CIVIC_INFO[%u](%c)", ii, (char)params.civic_info[ii]);
    }
  }
}

void LOWIUtils::extractFTMRRInfo(InPostcard *const card,
                                 vector<LOWIFTMRRNodeInfo>& params,
                                 LOWIMacAddress& bssid,
                                 uint16& interval)
{
  bssid.setMac(extractBssid(*card));
  uint32 num_elm = 0;
  extractUInt32(*card, "extractFTMRRInfo", "NUM_NODES",  num_elm);
  extractUInt16(*card, "extractFTMRRInfo", "RAND_INTER", interval);
  for (uint32 idx = 0; idx < num_elm; idx++)
  {
    // For each Node extract the card
    InPostcard *inner = 0;
    int err = card->getCard("FTMRR_NODE_CARD", &inner, idx);
    if (0 != err || NULL == inner)
    {
      // Unable to get card. break out of for loop.
      log_error(TAG, "extractFTMRRInfo - Unable to extract FTMRR_NODE_CARD");
      return;
    }

    LOWIFTMRRNodeInfo node;
    node.bssid.setMac(extractBssid(*inner));
    extractUInt32(*inner, "extractFTMRRInfo", "BSSID_INFO", node.bssidInfo);
    extractUInt8(*inner, "extractFTMRRInfo", "OPERATING_CLASS", node.operatingClass);
    uint8 bw = BW_20MHZ;
    extractUInt8(*inner, "extractFTMRRInfo", "BANDWIDTH", bw);
    node.bandwidth = to_eRangingBandwidth(bw);
    extractUInt8(*inner, "extractFTMRRInfo", "CENTER_CHANEL1", node.center_Ch1);
    extractUInt8(*inner, "extractFTMRRInfo", "CENTER_CHANEL2", node.center_Ch2);
    extractUInt8(*inner, "extractFTMRRInfo", "CHANEL", node.ch);
    extractUInt8(*inner, "extractFTMRRInfo", "PHY_TYPE", node.phyType);
    params.push_back(node);
    delete inner;
  }
}



