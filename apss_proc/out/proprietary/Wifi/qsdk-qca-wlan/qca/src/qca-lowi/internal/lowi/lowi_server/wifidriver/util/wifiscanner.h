/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

                       WiFi Scanner API

GENERAL DESCRIPTION
   This file contains the definition of WiFi Scanner API.

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
All Rights Reserved.
Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2010,2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.


=============================================================================*/
#ifdef __cplusplus
extern "C" {
#endif

#ifndef __WIFI_SCANNNER_API_H
#define __WIFI_SCANNNER_API_H

/*--------------------------------------------------------------------------
 * Include Files
 * -----------------------------------------------------------------------*/
#include <osal/wifi_scan.h>
#include "innavService.h"
/*--------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -----------------------------------------------------------------------*/

#define WIPS_MAX_BSSIDS_TO_SCAN MAX_BSSIDS_ALLOWED_FOR_MEASUREMENTS
#define ERR_SELECT_TERMINATED -301
#define ERR_SELECT_TIMEOUT    -10
#define ERR_NOT_READY         -11

/* Maximum number of BSSIDs to be output from passive scan from libwifiscanner module */
#define NUM_MAX_BSSIDS 75

/* This value is to indicate that cell power limit is not found within the beacon */
#define WPOS_CPL_UNAVAILABLE 0x7F

extern WlanFrameStore wlanFrameStore;

/*=============================================================================================
 * Function description:
 * This function sets up the dirver variable states and opens any needed sockets to the WLAN
 * driver.
 *
 * Parameters: NONE
 *
 * Return value: NONE
 *
 =============================================================================================*/
void lowi_scan_drv_open();

/*=============================================================================================
 * Function description:
 * Report RTS CTS measurement.
 *
 * Parameters:
 *    pAppMeasurementRsp
 *    pRslts_out
 *    buf_out
 *
 * Return value:
 *    pointer to parsed scan result. note the memory is allocated with QUIPC_MALLOC
 =============================================================================================*/
const quipc_wlan_meas_s_type * lowi_report_rts_cts_meas(
                               tInNavMeasRspParams* pAppMeasurementRsp,
                               tRttRssiResults* pRslts_out,
                               unsigned char* buf_out);

/*=============================================================================================
 * Function description:
 *   Function with processes RIVA passive scan request with fresh WIFI passive scan measurement.
 *
 * Parameters:
 *   cached   Cache results are needed or fresh
 *   max_age_sec if a cached result is within this maximum age, just return that.
 *   max_meas_age_sec
 *   timeout  Timeout for the request
 *   pRetVal  Return value from the function
 *   pFreq    Pointer to an Array of frequencies to be scanned
 *   num_of_freq  Number of items in the Array of frequencies
 *   frameArrived Indicates that an 80211 frame has arrived and requires processing
 *
 * Return value:
 *    Non-NULL : pointer to parsed scan result. note the memory is allocated with QUIPC_MALLOC
 *    NULL : failure
 =============================================================================================*/
const quipc_wlan_meas_s_type * lowi_proc_iwmm_req_passive_scan_with_live_meas (const int cached,
                                                                               const uint32 max_age_sec,
                                                                               const uint32 max_meas_age_sec,
                                                                               int timeout,
                                                                               int* pRetVal,
                                                                               int* pFreq,
                                                                               int num_of_freq,
                                                                               int* frameArrived);

/*=============================================================================================
 * Function description:
 *   Function to release memory block allocated for scan measurement.
 *   This function simply calls QUIPC_FREE
 *
 * Parameters:
 *   ptr_mem pointer
 *
 * Return value:
 *    none
 =============================================================================================*/
void lowi_free_meas_result (quipc_wlan_meas_s_type * const ptr_mem);

/*=============================================================================================
 * Function description:
 * Send Action Frame To Access Point.
 *
 * Parameters:
 *    frameBody: Frame body to be transmitted
 *    frameLen: Length of Frame body to be transmitted
 *    freq: Frequency on which to transmit the frame
 *    sourceMac: the access point receiving the FTM Range report
 *    selfMac:   The Local STA's MAC address
 *
 * Return value:
 *    Error Code: 0 - Success, all other values indicate failure
 =============================================================================================*/
int lowi_send_action_frame(uint8* frameBody,
                           uint32 frameLen,
                           uint32 freq,
                           uint8 sourceMac[BSSID_SIZE],
                           uint8 selfMac[BSSID_SIZE]);

#endif /* __WIFI_SCANNNER_API_H */

#ifdef __cplusplus
}
#endif
