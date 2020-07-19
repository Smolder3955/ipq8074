#ifndef __LOWI_UTILS_H__
#define __LOWI_UTILS_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Utils Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Utils

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.


Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

#include <base_util/postcard.h>
#include <inc/lowi_const.h>
#include <inc/lowi_request.h>
#include <inc/lowi_response.h>
#include <inc/lowi_scan_measurement.h>
#include <string.h>

// Size of an array of objects
#define LOWI_ARR_SIZE(arr) sizeof(arr)/sizeof((arr)[0])
// Check val is less than size of array, before accessing the array
#define LOWI_TO_STRING( val, arr ) LOWIUtils::to_string(val, arr, LOWI_ARR_SIZE(arr))

namespace qc_loc_fw
{

/**
 * Utility Class
 */
class LOWIUtils
{
private:
    /**
     * Parses a periodic ranging request parameters coming on a postcard and
     * generates the inputs to create a periodic ranging request
     *
     * @param card: input postcard
     * @param vec : vector of periodic nodes
     * @param req_id: request id
     * @param timeoutTimestamp: request timeout timestamp
     * @param rttReportType: report type for the response
     *
     * @return bool: true if success, else false
     */
    static bool parsePeriodicRangScanParams(InPostcard *const card,
                                            vector <LOWIPeriodicNodeInfo>& vec,
                                            uint32 &req_id,
                                            INT64 &timeoutTimestamp,
                                            uint8 &rttReportType);

    /**
     * Extracts the mac address from the postcard
     *
     * @param card: input postcard
     *
     * @return LOWIMacAddress : mac address extracted from the postcard
     */
    static LOWIMacAddress extractBssid(InPostcard &card);

    /**
     * parse the ranging scan parameters passed in the card which will be
     * used to populate the ranging scan request
     *
     * @param req_id: ranging scan request identifier
     * @param timeoutTimestamp: time out for the request
     * @param rttReportType: report type used for the response
     * @param card: postcard containing the ranging scan parameters
     * @param vec: vector to be populated with the wifi node information
     *           extracted from the card
     *
     * @return bool: true if success, else: false
     */
    static bool parseRangScanParams(uint32 &req_id,
                                    INT64 &timeoutTimestamp,
                                    uint8 &rttReportType,
                                    InPostcard &card,
                                    vector<LOWINodeInfo> &vec);

    /**
     * parse the discovery scan parameters passed in the card and use them to
     * populate the discovery scan request
     *
     * @param req_id: discovery scan request identifier
     * @param card: postcard containing the discovery scan parameters
     * @param dis: pointer to discovery scan request to populate
     *
     * @return bool: true if success, else: false
     */
    static bool parseDiscScanParams(uint32 &req_id,
                                    InPostcard &card,
                                    LOWIDiscoveryScanRequest *dis);
    /**
     * The following set of functions extract some value from the postcard
     * passed in the argument. The 4th arguments determines the type of the
     * value to be extracted.
     *
     * @param card: input postcard containing the value to be extracted
     * @param n: string with the name of the value to be extracted
     * @param s: string used for debug purposes
     * @param num: value extracted to be placed here
     */
    static void extractUInt8(InPostcard &card, const char* n, const char* s, uint8 &num);
    static void extractUInt16(InPostcard &card, const char* n, const char* s, uint16 &num);
    static void extractUInt32(InPostcard &card, const char* n, const char* s, uint32 &num);
    static void extractInt8(InPostcard &card, const char* n, const char* s, int8 &num);
    static void extractInt16(InPostcard &card, const char* n, const char* s, int16 &num);
    static void extractInt32(InPostcard &card, const char* n, const char* s, int32 &num);
    static void extractInt64(InPostcard &card, const char* n, const char* s, INT64 &num);
    static void extractBool(InPostcard &card, const char* n, const char* s, bool &num);

    /**
    * Parses the LCI parameters coming on the postcard and generates the inputs
    * to create a SET_LCI_INFORMATION request
    *
    * @param card: input postcard
    * @param params: LCI parameters
    * @param req_id: LOWIRequest request id
    */
    static void extractLciInfo(InPostcard *const card,
                               LOWILciInformation &params,
                               uint32 &req_id);

    /**
    * Parses the LCR parameters coming on the postcard and generates the inputs
    * to create a SET_LCR_INFORMATION request
    *
    * @param card: input postcard
    * @param params: LCR parameters
    * @param req_id: LOWIRequest request id
    */
    static void extractLcrInfo(InPostcard *const card,
                               LOWILcrInformation &params,
                               uint32 &req_id);

    /**
    * Parses the FTMRR parameters coming on the postcard and
    * generates the inputs to create a FTM ranging request
    *
    * @param card: input postcard
    * @param params: FTMRR parameters
    * @param params: randomization interval
    * @param req_id: LOWIRequest request id
    */
    static void extractFTMRRInfo(InPostcard *const card,
                                 vector<LOWIFTMRRNodeInfo> &params,
                                 LOWIMacAddress &bssid,
                                 uint16 &interval);

    /**
     * Adds the mac address to the postcard
     * @param card: input postcard
     * @param bssid: mac address to be added to the card
     */
    static void addBssidToCard(OutPostcard &card, const LOWIMacAddress &bssid);

    /**
     * Adds the FTMRR node to the postcard
     * @param card: input postcard
     * @param node: FTMRR node to be added to the card
     *
     * @return bool: true if success, else: false
     */
    static bool addFTMRRNodeToCard(OutPostcard &card, const LOWIFTMRRNodeInfo &node);

public:
  /**
   * Log TAG
   */
  static const char * const TAG;

  /**
   * Parses the Scan Measurements from the InPostCard
   * @param InPostcard* InPostCard from which the scan measurements
   *                    to be parsed
   * @param  [in] vector<LOWIScanMeasurement*>& Measurement vector for all
   *                                       measurements.
   * @return true - success, false - otherwise
   */
  static bool parseScanMeasurements
  (InPostcard* const postcard, vector <LOWIScanMeasurement*> & scan);

  /**
   * Parses the Measurements Info from the InPostCard
   * @param InPostcard* InPostcard from which the measurement info to be parsed
   * @param  [in] vector<LOWIMeasurementInfo*>& Measurement info
   *                                            for all measurements.
   * @return true - success, false - otherwise
   */
  static bool parseMeasurementInfo
  (InPostcard* const card, vector <LOWIMeasurementInfo*>& meas_info);

  /**
   * Parses the information element data
   *
   * @param card: InPostCard from which measurements are to be parsed
   * @param meas_info: vector where measurement information is to be placed.
   *
   * @return bool: true if success, false if failure
   */
  static bool parseIEDataInfo
  (InPostcard* const card, vector <int8> &meas_info);

  /**
   * Parses the location information element data
   *
   * @param card: InPostCard from which measurements are parsed
   * @param info: location where information is to be placed
   * @param type: string indicatign the type of info: LCI or LCR
   *
   * @return bool: true if success, false if failure
   */
  static bool parseLocationIEDataInfo
  (InPostcard* const card, uint8 *info, uint8 len, char const *type);

  /**
   * Composes an OutPostCard from the Request created by the client.
   *
   * This API is intended for the clients that have there own socket based IPC
   * implementation and are registered with the IPC hub. Such clients just need
   * to call this API to convert the Request to an OutPostCard which could then
   * be sent to the IPC Hub by the client. The recipient field of the Postcard
   * is populated by this API.
   *
   * Note: Memory should be deallocated by the client
   *
   * @param LOWIRequest* Request to be converted to an OutPostCard
   * @param char* ID of the originator which will be added to the OutPostCard
   * @return OutPostCard
   */
  static OutPostcard* requestToOutPostcard (LOWIRequest* const request,
      const char* const originatorId);

  /**
   * Parses an InPostCard and generates the Response needed by the client.
   *
   * This API is intended for the clients that have there own socket based IPC
   * implementation and are registered with the IPC hub. Such clients communicate
   * with the uWifiPsoAPI process through IPC hub to send and receive Postcards
   * on there own and need this API to parse the InPostCard into a Response.
   *
   * Note: Memory should be deallocated by the client
   *
   * @param InPostcard* InPostcard to be parsed
   * @return LOWIResponse
   */
  static LOWIResponse* inPostcardToResponse (InPostcard* const postcard);

  /**
   * Creates a Request from a InPostcard
   * Used by the LOWI server to parse the InPostcard and create a Request
   * @param InPostcard* Postcard
   * @return LOWIRequest
   */
  static LOWIRequest* inPostcardToRequest (InPostcard* const card);

  /**
   * Creates a OutPostcard from the response.
   * Used by the LOWI server to create a OutPostcard to be sent to the Hub.
   * @param LOWIResponse* Response for which the Postcard is to be created
   * @param char* ID of the receiver of this postcard
   */
  static OutPostcard* responseToOutPostcard (LOWIResponse* resp,
      const char* to);

  /**
   * Injects the MeasurementInfo into the Post card
   * @param OutPostcard Card to be filled with Measurement Info
   * @param vector <LOWIMeasurementInfo*> Measurements container from where
   *        the measurement info is to be extracted
   * @return true - success, false otherwise
   */
  static bool injectMeasurementInfo (OutPostcard & card,
      vector <LOWIMeasurementInfo*> & info);

  /**
   * Injects the ScanMeasurements into the Postcard.
   * @param OutPostcard Card to be filled with Scan measurements
   * @param vector <ScanMeasurement*> Scan Measurements
   * @return true - success, false otherwise
   */
  static bool injectScanMeasurements (OutPostcard & card,
      vector <LOWIScanMeasurement*> & meas);

  /**
   * Injects the Infomation Element (IE) data into a postcard
   *
   * @param card: OutPostcard to be filled with IE data
   * @param info: IE data
   * @return bool: true if success, false otherwise
   */
  static bool injectIeData (OutPostcard & card, vector <int8> & info);

  /**
   * Injects the Infomation Element (IE) data into a postcard
   *
   * @param card: OutPostcard to be filled with location IE data
   * @param info: location IE data
   * @param len : length of IE data
   * @param type: string describing the type of data: LCI or LCR
   *
   * @return bool: true if success, false otherwise
   */
  static bool injectLocationIeData (OutPostcard & card, uint8 *info, uint8 len,
                                    char const *type);

  /** Various functions for type conversion, printing, etc */
  static LOWIResponse::eResponseType to_eResponseType (int a);
  static LOWIResponse::eScanStatus to_eScanStatus (int a);
  static LOWIDiscoveryScanResponse::eScanTypeResponse to_eScanTypeResponse(int a);
  static eNodeType to_eNodeType (int a);
  static eRttType to_eRttType (unsigned char a);
  static eRttReportType to_eRttReportType (unsigned char a);
  static eRangingBandwidth to_eRangingBandwidth (uint8 a);
  static eRangingPreamble to_eRangingPreamble (uint8 a);
  static LOWIDiscoveryScanRequest::eBand to_eBand (int a);
  static LOWIDiscoveryScanRequest::eScanType to_eScanType (int a);
  static LOWIDiscoveryScanRequest::eRequestMode to_eRequestMode (int a);
  static qc_loc_fw::ERROR_LEVEL to_logLevel (int a);
  static eLowiMotionPattern to_eLOWIMotionPattern(uint8 a);
  static LOWIResponse::eScanStatus to_eLOWIDriverStatus(uint8 a);
  static char const* to_string(LOWIResponse::eScanStatus a);
  static char const* to_string(LOWIRequest::eRequestType a);
  static char const* to_string(LOWIResponse::eResponseType a);
  static char const* to_string(size_t val, const char * arr[], size_t arr_size);

  /**
   * Function description:
   *    This function will return current time in number of milli-seconds
   *    since Epoch (00:00:00 UTC, January 1, 1970Jan 1st, 1970).
   *
   * Parameters:
   *    none
   *
   * Return value:
   *    number of milli-seconds since epoch.
   */
  static int64 currentTimeMs ();

  /**
   * Returns Channel corresponding the frequency
   * @param uint32 Frequency which can be in 2.4 GHz or 5 Ghz band
   * @return 0 for frequency out of 2.4 / 5 Ghz band
   *         Valid channel number otherwise
   */
  static uint32 freqToChannel (uint32 freq);

  /**
   * Returns the band for the frequency passed.
   * @param uint32 Frequency in 2.4 / 5 Ghz band
   * @return associated band
   */
  static LOWIDiscoveryScanRequest::eBand freqToBand( uint32 freq );

  /**
   * Returns the frequency for the band and channel passed.
   * @param uint32 Channel number
   * @param LOWIDiscoveryScanRequest::eBand Band of the channel
   * @return Frequency
   */
  static uint32 channelBandToFreq (uint32 channel,
      LOWIDiscoveryScanRequest::eBand band);

  /**
   * Get channels or frequency's corresponding to the band
   * @param LOWIDiscoveryScanRequest::eBand Band
   * @param unsigned char Num of channels
   * @param bool flag to indicate if freqency's or channels needed.
   * @return Pointer to the channels array
   */
  static int * getChannelsOrFreqs (LOWIDiscoveryScanRequest::eBand,
      unsigned char & channels, bool freq);

  /**
   * Get channels or freqency's corresponding to the ChannelInfo
   * @param vector<LOWIChannelInfo> & Channels
   * @param unsigned char Num of channels
   * @param bool flag to indicate if freqency's or channels needed.
   * @return Pointer to the array of channels
   */
  static int * getChannelsOrFreqs (vector<LOWIChannelInfo> & v,
      unsigned char & channels, bool freq);

};

} // namespace qc_loc_fw

#endif //#ifndef __LOWI_UTILS_H__
