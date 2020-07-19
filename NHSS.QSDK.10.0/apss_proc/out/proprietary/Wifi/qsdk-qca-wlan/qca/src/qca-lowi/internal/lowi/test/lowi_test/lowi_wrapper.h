/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Wrapper

GENERAL DESCRIPTION
  This file contains the Wrapper around LOWI Client

Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#ifndef __LOWI_WRAPPER_H__
#define __LOWI_WRAPPER_H__

#include <inc/lowi_client.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef int (*ptr2LOWIResultFunc)(qc_loc_fw::LOWIResponse *);

/*=============================================================================================
 * Function description:
 *   This function initializes LOWI client.
 *   Please note that this function shall be called only once before destroy is called
 *
 * Parameters:
 *    ptr2ResultFunc Pointer to function to be called for passive scan results
 *    ptr2ResultFunc Pointer to function to be called for rtscts scan results
 *
 * Return value:
 *    error code: 0: success
 *                non-zero: error
 =============================================================================================*/
extern int lowi_wrapper_init(ptr2LOWIResultFunc passive, ptr2LOWIResultFunc rtscts);

/*=============================================================================================
 * Function description:
 *   This function registers callback function for the capability response.
 *   The callback function will be called when ever there is a response to the
 *   capability request.
 *
 * Parameters:
 *    ptr2CapResponseFunc Pointer to function to be called for capability response
 *
 * Return value:
 *    error code: 0: success
 *                non-zero: error
 =============================================================================================*/
extern int lowi_wrapper_register_capabilities_cb(ptr2LOWIResultFunc cap);

/*=============================================================================================
 * Function description:
 *   This function registers callback function for the
 *   AsyncDiscoveryScanResults response.
 *   The callback function will be called when ever there is a
 *   response to the AsyncDiscoveryScanResults request.
 *
 * Parameters:
 *    ptr2ResultFunc Pointer to function to be called for async response
 *
 * Return value:
 *    error code: 0: success
 *                non-zero: error
 =============================================================================================*/
extern int lowi_wrapper_register_async_resp_cb(ptr2LOWIResultFunc async);

/*=============================================================================================
 * Function description:
 *   This function destroys LOWI client.
 *   Please note that this function shall be called once when the procC daemon starts
 *
 * Parameters:
 *    none
 *
 * Return value:
 *    error code: 0: success
 *                non-zero: error
 =============================================================================================*/
extern int lowi_wrapper_destroy();

/*=============================================================================================
 * lowi_queue_passive_scan_req
 *
 * Description:
 *   This function requests for passive scan on all channels.
 *
 * Parameters: None
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 =============================================================================================*/
extern int lowi_queue_passive_scan_req(void);

/*=============================================================================================
 * lowi_queue_passive_scan_req_xtwifi
 *
 * Description:
 *   This function requests for passive scan on all channels.
 *
 * Parameters: int, uint32, uint32
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 =============================================================================================*/
extern int lowi_queue_passive_scan_req_xtwifi(const int cached,
                                              const uint32 max_scan_age_sec,
                                              const uint32 max_meas_age_sec);

/*=============================================================================================
 * lowi_queue_rts_cts_scan_req
 *
 * Description:
 *   This function requests for RTS/CTS scan for the given APs
 *
 * Parameters:
 *   p_scan_params - Scan parameters: AP MAC ID and channel number.
 *
 * WARNING: LOWIWrapper WILL free the memory pointed by p_scan_params
 *   regardless whether the function
 *   succeeds or not. That means the caller should only allocate prior to calling this function
 *   but never FREE it.
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 =============================================================================================*/
extern int lowi_queue_rts_cts_scan_req(qc_loc_fw::vector<qc_loc_fw::LOWIPeriodicNodeInfo> *p_scan_params);

/*=============================================================================================
 * lowi_queue_discovery_scan_req_band
 *
 * Description:
 *   This function requests for discovery scan on a specified band.
 *
 * Parameters:
 *  Band: A band at which the passive scan is requested.
 *  timeout: A request time out after which the request is dropped.
 *  scan_type: Active - 0 / Passive - 1
 *  meas_filter_type: Filter the measurements based on the age filter
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 =============================================================================================*/
extern int lowi_queue_discovery_scan_req_band(qc_loc_fw::LOWIDiscoveryScanRequest::eBand band,
                                              int64 request_timeout, qc_loc_fw::LOWIDiscoveryScanRequest::eScanType scan_type,
                                              uint32 meas_filter_age);

/*=============================================================================================
 * lowi_queue_discovery_scan_req_ch
 *
 * Description:
 *   This function requests for discovery scan on a specific channels.
 *
 * Parameters:
 *  ptr: A pointer to array of channels.
 *  num_channels: Number of channels pointed by the channel_ptr.
 *  request_timeout: A request time out after which the request is dropped.
 *  scan_type: Active - 0 / Passive - 1
 *  meas_filter_type: Filter the measurements based on the age filter
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 =============================================================================================*/
extern int lowi_queue_discovery_scan_req_ch(qc_loc_fw::vector<qc_loc_fw::LOWIChannelInfo> *ptr,
                                            int64 request_timeout, qc_loc_fw::LOWIDiscoveryScanRequest::eScanType scan_type, uint32 meas_filter_age);

/*=============================================================================================
 * lowi_queue_capabilities_req
 *
 * Description:
 *   This function requests for driver capabilities from lowi
 *
 * Parameters: None
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 =============================================================================================*/
extern int lowi_queue_capabilities_req(void);

/*=============================================================================================
 * lowi_queue_async_discovery_scan_result_req
 *
 * Description:
 *   This function requests for async discovery scan results
 *
 * Parameters: uint32 : Timeout in seconds after which the request can be dropped
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 =============================================================================================*/
extern int lowi_queue_async_discovery_scan_result_req(uint32 timeout);

extern int lowi_queue_nr_request();

/*=============================================================================================
 * lowi_queue_set_lci
 *
 * Description:
 *   This function sets LCI information
 *
 * Parameters: lciInfo : LCI information
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 =============================================================================================*/
extern int lowi_queue_set_lci(qc_loc_fw::LOWILciInformation *lciInfo);

/*=============================================================================================
 * lowi_queue_set_lcr
 *
 * Description:
 *   This function sets LCR information
 *
 * Parameters: lciInfo : LCR information
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 =============================================================================================*/
extern int lowi_queue_set_lcr(qc_loc_fw::LOWILcrInformation *lcrInfo);

/*=============================================================================================
 * lowi_queue_where_are_you
 *
 * Description:
 *   This function requests Where are you for the given AP
 *
 * Parameters:
 *   bssid - Target STA MAC
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 =============================================================================================*/
extern int lowi_queue_where_are_you(qc_loc_fw::LOWIMacAddress bssid);

/*=============================================================================================
 * lowi_queue_ftmrr
 *
 * Description:
 *   This function requests FTM mesurements to given AP
 *
 * Parameters:
 *   bssid - Target STA MAC
 *   randInterval - rand interval
 *   nodes - FTMRR nodes
 *
 * Return value:
 *   0 - success
 *   non-zero: error
 =============================================================================================*/
extern int lowi_queue_ftmrr(qc_loc_fw::LOWIMacAddress bssid,
                            uint16 randInterval, qc_loc_fw::vector<qc_loc_fw::LOWIFTMRRNodeInfo>& nodes);
#ifdef __cplusplus
}
#endif

#endif /* __LOWI_WRAPPER_H__ */
