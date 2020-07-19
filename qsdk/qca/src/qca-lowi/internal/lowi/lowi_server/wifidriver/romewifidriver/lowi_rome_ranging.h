#ifndef __LOWI_ROME_RANGING_H__
#define __LOWI_ROME_RANGING_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        WIPS module - Wifi Interface for Positioning System for ranging

GENERAL DESCRIPTION
  This file contains the declaration and some global constants for WIPS
  module.

Copyright (c) 2013-2014, 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.

=============================================================================*/
#include <stdint.h>

#define MAX_BSSIDS_TO_SCAN  10

#define MAX_ELEMENTS_2G_ARR 14
#define MAX_ELEMENTS_5G_ARR 56

/* RTT functional defines start here */
#define RTT_REPORT_PER_FRAME_WITH_CFR 0
#define RTT_REPORT_PER_FRAME_NO_CFR   1
#define RTT_AGGREGATE_REPORT_NON_CFR  2

#define RTT_MEAS_FRAME_NULL 0
#define RTT_MEAS_FRAME_QOSNULL 1
#define RTT_MEAS_FRAME_TMR 2

#define WMI_RTT_BW_20 0
#define WMI_RTT_BW_40 1
#define WMI_RTT_BW_80 2
#define WMI_RTT_BW_160 3

#define WMI_RTT_PREAM_LEGACY 0
#define WMI_RTT_PREAM_HT 2
#define WMI_RTT_PREAM_VHT 3

#define SELECT_TIMEOUT_NORMAL 5
#define SELECT_TIMEOUT_NON_ASAP_TARGET 70
#define SELECT_TIMEOUT_NEVER -1
#include "innavService.h"                           //  structure definitions and such
#include <osal/wifi_scan.h>
#include "wlan_capabilities.h"
#include <inc/lowi_scan_measurement.h>
#include "rttm.h"

/* This is the mask used to acces/clear the Phy Mode field from
 * Channel Structure's "info" field.
 */
#define PHY_MODE_MASK 0xFFFFFF80

/*==================================================================
 * Structure Description:
 * Structure used to convet Target/Destination devuce information
 * to RTT measurement function.
 *
 * mac         : The MAC address of the Target/Destination device
 * rttFrameType: The frame type to be used for RTT indicating
 *               RTTV2 or RTTV3
 ==================================================================*/
typedef struct
{
  tANI_U8 mac[WIFI_MAC_ID_SIZE];
  tANI_U8 rttFrameType;
  tANI_U8 bandwidth;
  tANI_U8 preamble;
  tANI_U8 numFrames;
  tANI_U8 numFrameRetries;

  /*** FTMR related fields */
  tANI_U32 ftmParams;
  tANI_U32 tsfDelta;
  bool tsfValid;
} DestInfo;

typedef enum
{
  /* ROME CLD Messages */
  ROME_REG_RSP_MSG,
  ROME_CHANNEL_INFO_MSG,
  ROME_P2P_PEER_EVENT_MSG,
  ROME_CLD_ERROR_MSG,
  /* ROME FW Messages*/
  ROME_RANGING_CAP_MSG,
  ROME_RANGING_MEAS_MSG,
  ROME_RANGING_ERROR_MSG,
  /* NL/Kernel Messages */
  ROME_NL_ERROR_MSG,
  ROME_MSG_MAX
} RomeNlMsgType;

typedef PACK(struct)
{
  tANI_U32 chId;
  wmi_channel wmiChannelInfo;
} ChannelInfo;

typedef PACK(struct)
{
  tANI_U8 mac[WIFI_MAC_ID_SIZE + 2];
  WMI_RTT_ERROR_INDICATOR errorCode;
} FailedTarget;


namespace qc_loc_fw
{

/*===========================================================================
 * Function description:
 *   This function parses the ranging measurements that arrive from FW
 *
 * Parameters:
 *   measRes: The ranging measurements from FW.
 *   scanMEasurements: The destination where parsed scan measurements
 *                     will be stored.
 *
 * Return value:
 *   error Code: 0 - Success, -1 - Failure
 ===========================================================================*/
int RomeParseRangingMeas(char *measRes, vector<LOWIScanMeasurement*> *scanMeasurements);

/*===========================================================================
 * Function description:
 *   This function constructs the LCI configuration message and
 *   sends it to FW.
 *
 * Parameters:
 *   reqId: The request Id for this request.
 *   request: The LCI configuration request and parameters.
 *
 * Return value:
 *   error Code: 0 - Success, -1 - Failure
 ===========================================================================*/
int RomeSendLCIConfiguration(tANI_U16 reqId, LOWISetLCILocationInformation *request);

/*===========================================================================
 * Function description:
 *   This function constructs the LCR configuration message and
 *   sends it to FW.
 *
 * Parameters:
 *   reqId: The request Id for this request.
 *   request: The LCR configuration request and parameters.
 *
 * Return value:
 *   error Code: 0 - Success, -1 - Failure
 ===========================================================================*/
int RomeSendLCRConfiguration(tANI_U16 reqId, LOWISetLCRLocationInformation *request);

/*===========================================================================
 * Function description:
 *   This function constructs the LCI request message and
 *   sends it to host.
 *
 * Parameters:
 *   reqId: The request Id for this request.
 *   request: The Where are you request and parameters.
 *
 * Return value:
 *   error Code: 0 - Success, -1 - Failure
 ===========================================================================*/
int RomeSendLCIRequest(tANI_U16 reqId, LOWISendLCIRequest *request);

/*===========================================================================
 * Function description:
 *   This function constructs the FTM ranging request message and
 *   sends it to host.
 *
 * Parameters:
 *   reqId: The request Id for this request.
 *   request: The Where are you request and parameters.
 *
 * Return value:
 *   error Code: 0 - Success, -1 - Failure
 ===========================================================================*/
int RomeSendFTMRR(tANI_U16 reqId, LOWIFTMRangingRequest *request);

} // namespace qc_loc_fw

#ifdef __cplusplus
extern "C"
{
#endif

/*=============================================================================================
 * Function description:
 * Open Rome RTT Interface Module
 *
 * Parameters:
 *    cfrCaptureModeState: CFR Capture Mode
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeWipsOpen();

/*=============================================================================================
 * Function description:
 * Close Rome RTT Interface Module
 *
 * Parameters:
 *    None
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeWipsClose();

/*=============================================================================================
 * Function description:
 * Send Channel Info request to Rome CLD
 *
 * Parameters:
 *    iwOemDataCap: The WLAn capabilites information
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeSendChannelInfoReq(IwOemDataCap iwOemDataCap);

/*=============================================================================================
 * Function description:
 * Extract information from Channein info message.
 *
 * Parameters:
 *    data   : The message body
 *    pChannelInfoArray: Pointer to Channel info Array
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeExtractChannelInfo(void* data, ChannelInfo *pChannelInfoArray);
/*=============================================================================================
 * Function description:
 * Send Registration request to Rome CLD
 *
 * Parameters:
 *    None
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeSendRegReq();

/*=============================================================================================
 * Function description:
 * Close Rome RTT Interface Module
 *
 * Parameters:
 *    msgType: The Type of Message received
 *    data   : The message body
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeNLRecvMessage(RomeNlMsgType* msgType, void* data, tANI_U32 maxDataLen);

/*=============================================================================================
 * Function description:
 * Extract information from registration message
 *
 * Parameters:
 *    data   : The message body
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeExtractRegRsp(void* data);

/*=============================================================================================
 * Function description:
 * Send Ranging Capabilities request to Rome FW
 *
 * Parameters:
 *    None
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeSendRangingCapReq();

/*=============================================================================================
 * Function description:
 * Extract information from Ranging Capability message
 *
 * Parameters:
 *    data   : The message body
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeExtractRangingCap(void* data, RomeRttCapabilities* pRomeRttCapabilities);

/*=============================================================================================
 * Function description:
 * Extract Error Code from Ranging Error Message
 *
 * Parameters:
 *    data   : The message body
 *    errorCode: Thelocation to store the extracted error code
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeExtractRangingError(void* data, tANI_U32* errorCode, tANI_U8* bssid);

/*=============================================================================================
 * Function description:
 * Based on the Error Code if the bssid Will be skipped, then the bssid will be added
 * to the "Skipped Targets" List. This will allow the driver to send back appropriate
 * error Codes to the Client.
 *
 * Parameters:
 *    errorCode   : The Error code recieved from FW
 *    bssid       : the bssid associated with the error code
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeAddSkippedTargetToList(tANI_U32 errorCode, tANI_U8 mac[WIFI_MAC_ID_SIZE + 2]);

/*=============================================================================================
 * Function description:
 * Extract information from P2P Peer info message
 *
 * Parameters:
 *    data   : The message body
 *
 * Return value:
 *    SUCCESS/FAILURE
 =============================================================================================*/
int RomeExtractP2PInfo(void* data);

/*=============================================================================================
 * Function description:
 *   Called by external entity to create the pipe to Ranging thread
 *
 * Parameters:
 *   None
 *
 * Return value:
 *    0 Success, other values otherwise
 =============================================================================================*/
int RomeInitRangingPipe();

/*=============================================================================================
 * Function description:
 *   Called by external entity to create the pipe to Raning Thread
 *
 * Parameters:
 *   None
 *
 * Return value:
 *    0 Success, other values otherwise
 =============================================================================================*/
int RomeCloseRangingPipe();

/*=============================================================================================
 * Function description:
 *   Called by external entity to terminate the blocking of the Ranging thread on select call
 *   by means of writing to a pipe. Thread is blocked on socket and a pipe in select call
 *
 * Parameters:
 *   None
 *
 * Return value:
 *    num of bytes written, -1 for error, 0 for no bytes written
 =============================================================================================*/
int RomeUnblockRangingThread();

/*=============================================================================================
 * Function description:
 *   Waits on the Private Netlink Socket and the LOWI controller pipe
 *   If Messages/Data arrive, collects and processes them accordingly.
 *   If LOWI controller requests a shutdown then exits and picks up the new pequest to process
 *
 * Parameters:
 *   timeout_val     : int number indicating timeout in seconds
 *
 * Return value:
 *    error code
 *
 =============================================================================================*/
int RomeBlockListeningForP2PEvents(void);

/*===========================================================================
 * Function description:
 *   Waits on a private Netlink socket for one of the following events
 *       1) Data becomes available on socket
 *       2) Activity on unBlock Pipe
 *       3) A timeout happens.
 *
 * Parameters:
 *   nl_sock_fd is the file descriptor of the Private Netlink Socket
 *   int timeout_val is the timout value specified by caller
 *
 * Return value:
 *   TRUE, if some data is available on socket. FALSE, if timed out or error
 ===========================================================================*/
int RomeWaitOnActivityOnSocketOrPipe(int timeout_val);

/*=====================================================================================
 * Function description:
 *   This function sends the Ranging Request to the FW
 *
 * Parameters:
 *   ChannelInfo: The object containing the channel on which ranging will be performed.
 *   unsigned int numBSSIDs: Number of BSSIDs in the request.
 *   DestInfo bssidsToScan: BSSIDs to Range with.
 *   DestInfo spoofBssids: Spoof Addresses of the BSSIDs to range with.
 *   unsigned int reportType: Report Type for Masurements from FW
 *
 * Return value:
 *   error Code: 0 - Success, -1 - Failure
 ======================================================================================*/
int RomeSendRttReq(uint16 reqId,
                   ChannelInfo chanInfo,
                   unsigned int numBSSIDs,
                   DestInfo bssidsToScan[MAX_BSSIDS_TO_SCAN],
                   DestInfo spoofBssids[MAX_BSSIDS_TO_SCAN],
                   unsigned int reportType);

#ifdef __cplusplus
}
#endif

#endif // __LOWI_ROME_RANGING_H__

