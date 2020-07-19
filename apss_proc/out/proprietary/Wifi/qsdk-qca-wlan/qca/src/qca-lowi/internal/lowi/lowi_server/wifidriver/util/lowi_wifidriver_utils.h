#ifndef __LOWI_WIFI_DRIVER_UTILS_H__
#define __LOWI_WIFI_DRIVER_UTILS_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Wifi Driver Utils Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWIWifiDriverUtils

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

#include "wifiscanner.h"
#include <lowi_server/lowi_measurement_result.h>
#include <inc/lowi_request.h>
#include <inc/lowi_scan_measurement.h>
#include <base_util/config_file.h>
#include "wipsiw.h"
#include "wlan_capabilities.h"

namespace qc_loc_fw
{

/**
 * Utility Class
 */
class LOWIWifiDriverUtils
{
private:
  static const char * const TAG;
  static int ioctlSock;  // Socket for all IOCTL communication with WiFi Driver
  static IwOemDataCap *iwOemDataCap; /* Wi-Fi Driver Capabilities */

public:
  enum eGetWiFiCapabilityError
  {
    CAP_SUCCESS = 0,
    CAP_FAILURE,
    CAP_NO_MEM,
    CAP_IOCTL_FAILURE,
    CAP_SYSTEM_PROP_FAILURE,
    CAP_WLAN_DEV_FAILURE
  };

  /**
   * Parses and injects measurements received from wifidriver
   * into LOWIMeasurementResult
   * @param LOWIMeasurementResult* pointer to the result structure that gets
   *                               the measurements injected to
   * @param vector<const quipc_wlan_meas_s_type*> Measurements received
   *                               from wifi driver
   * @param int32                  RSSI threshold for RTT
   * @return bool true is success, false otherwise
   */
  static bool injectScanMeasurements (LOWIMeasurementResult* result,
      vector<const quipc_wlan_meas_s_type*> & measurements,
      int32 rssi_threshold_for_rtt);

  /**
   * Parses and injects measurement details received from wifidriver
   * for each AP into LOWIMeasurementInfo
   * @param vector <LOWIMeasurementInfo*> Vector that contains
   *                               the measurement info for each AP
   * @param quipc_wlan_ap_meas_s_type* Pointer to the measurement info for AP
   *                               from wifi driver
   * @param int32                  RSSI threshold for RTT
   * @return bool true is success, false otherwise
   */
  static bool injectMeasurementInfo (vector <LOWIMeasurementInfo*>& vec,
      quipc_wlan_ap_meas_s_type* pAp, int32 rssi_threshold_for_rtt);

  /**
   * Gets the supported frequencies for a specified band.
   * @param eBand Band for which supported channels are to be returned
   * @param s_ch_info* pointer to structure that contains all the
   *                   supported channels
   * @param char  Number of channels supported in the retrun value
   * @return int* pointing to chunk of supported frequencies
   *
   * NOTE: Memory returned by int* should be freed by caller
   */
  static int* getSupportedFreqs (LOWIDiscoveryScanRequest::eBand band,
      s_ch_info* p_ch_info,
      unsigned char & num_channels);


  /**
   * Cleans-up the stored Wifi capabilities
   */
  static void cleanupWifiCapability ();

  /**
   * Gets Capability and Identity information from the Wi-Fi Host
   * Driver
   * @param IwOemDataCap* pointing to strucutre where capability
   *                     information will be loaded
   * @param localStaMac location where local STA MAC address will
   *                    be stored.
   * @return eGetWiFiCapabilityError Error code
   */
  static eGetWiFiCapabilityError getWiFiIdentityandCapability (
                                           IwOemDataCap* iwOemDataCap,
                                           LOWIMacAddress &localStaMac);

   /* Sets the scan type (Active / Passive) for the discovery scan.
   * @param eScanType Scan Type (Active / Passive)
   * @return bool true is success, false otherwise
   */
  static bool setDiscoveryScanType (LOWIDiscoveryScanRequest::eScanType type);

  /**
   * Sends the command to the driver
   * @param char* Contains the command that needs to go to driver.
   * @return bool true if success, false otherwise
   */
//  static bool sendDriverCmd (char* buf);

  /**
   * Returns default wifi driver interface name by reading system property if
   * not already read from the system property
   * @return char*: string representing the default wifi driver interface
   */
  static char* get_interface_name (void);

  /**
   * Get the current interface state of Wi-Fi
   * @return eWifiIntfState: WiFi Interface state
   */
  static eWifiIntfState getInterfaceState();
};

} // namespace qc_loc_fw

#endif //#ifndef __LOWI_WIFI_DRIVER_UTILS_H__
