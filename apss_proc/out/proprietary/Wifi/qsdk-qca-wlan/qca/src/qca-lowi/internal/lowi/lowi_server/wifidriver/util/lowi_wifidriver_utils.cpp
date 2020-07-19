/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        Wifi Driver Utilities

GENERAL DESCRIPTION
  This file contains the implementation of utilities for Wifi Driver

Copyright (c) 2015-2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

============================================================================*/

#include <string.h>
#include <base_util/log.h>
#include "innavService.h"
#include "lowi_wifidriver_utils.h"
#include <common/lowi_utils.h>

#ifdef ANDROID
#include <common/lowi_diag_log.h>
#include <cutils/properties.h>
#endif

#include <sys/socket.h>
#include <linux/wireless.h>                         // note: NDK's Android'ized wireless.h
#include <errno.h>
#include <string.h>

#define RX_BUFF_SIZE  1024
#define MAX_CAPABILITY_REQ_RETRY_COUNT 3
#define CAPABILITY_RETRY_WAIT_TIME_IN_SEC 1

#ifdef LOWI_ON_ACCESS_POINT
#define PROPERTY_VALUE_MAX  92
#define DEFAULT_WLAN_INTERFACE "ath"
#else
#define DEFAULT_WLAN_INTERFACE "wlan0"
#endif

using namespace qc_loc_fw;

const char * const LOWIWifiDriverUtils::TAG = "LOWIWifiDriverUtils";
static char wlan_ifname[PROPERTY_VALUE_MAX];

IwOemDataCap* LOWIWifiDriverUtils::iwOemDataCap = NULL;

int LOWIWifiDriverUtils::ioctlSock = 0;
#define BUF_LEN 25

typedef struct s_wifi_priv_cmd {
  char *buf;
  int used_len;
  int total_len;
} s_wifi_priv_cmd;

bool LOWIWifiDriverUtils::injectScanMeasurements
(LOWIMeasurementResult *result,
 vector<const quipc_wlan_meas_s_type*>& measurements, //BATCH const quipc_wlan_meas_s_type * ptr,
 int32 rssi_threshold_for_rtt)
{
  bool retVal = false;
  log_verbose(TAG, "%s", __func__);
  do
  {
    if (NULL == result)
    {
      log_error(TAG, "%s, NULL argument!");
      return retVal;
    }
    if (0 == measurements.getNumOfElements())
    {
      log_error(TAG, "No measurements to insert!");
      return retVal;
    }

    retVal = true;
    result->scanStatus = LOWIResponse::SCAN_STATUS_DRIVER_ERROR;
    for (unsigned int ii = 0; ii < measurements.getNumOfElements(); ++ii)
    {
      const quipc_wlan_meas_s_type *ptr = measurements[ii];

      // Timestamp
      result->measurementTimestamp = ptr->t_timestamp_usec;
      log_debug(TAG, "Timestamp = %llu", ptr->t_timestamp_usec);

      // Scan status - We set the scan status to success for any successful batch.
      if (QUIPC_WLAN_SCAN_STATUS_SUCCEEDED == ptr->e_scan_status)
      {
        result->scanStatus = LOWIResponse::SCAN_STATUS_SUCCESS;
      }

      // Scan type - does not change per batch. So this should be fine.
      // TODO : Change this as the Active scan gets introduced
      result->scanType = LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_PASSIVE;
      if (QUIPC_WLAN_SCAN_TYPE_PASSIVE != ptr->e_scan_type)
      {
        result->scanType = LOWIDiscoveryScanResponse::WLAN_SCAN_TYPE_UNKNOWN;
      }

      bool retVal_internal = true;
      int num_of_ap = ptr->w_num_aps;
      log_debug(TAG, "Total AP's = %d", num_of_ap);
      // For each AP, insert a Measurement
      for (int ii = 0; ii < num_of_ap; ++ii)
      {
        quipc_wlan_ap_meas_s_type *pAp = &ptr->p_ap_meas[ii];
        if (NULL == pAp)
        {
          log_warning(TAG, "%s, AP measurement not found");
          retVal_internal = false;
          break;
        }

        LOWIScanMeasurement *scan = new(std::nothrow) LOWIScanMeasurement();
        if (NULL == scan)
        {
          log_error(TAG, "%s, Memory allocation failure", __func__);
          retVal_internal = false;
          break;
        }

        scan->bssid.setMac(pAp->u_MacId);

        scan->associatedToAp = (pAp->u_AssociatedToAp) ? true : false;
        log_debug(TAG, "%s - associatedToAp: %s",
                  __FUNCTION__, (scan->associatedToAp) ? "TRUE" : "FALSE");

        /* Load Channel Information */
        scan->frequency         = (uint32)pAp->u_Freq;
        scan->band_center_freq1 = (uint32)pAp->u_Band_center_freq1;
        scan->band_center_freq2 = (uint32)pAp->u_Band_center_freq2;
        scan->info = pAp->u_info;
        scan->tsfDelta = pAp->u_tsfDelta;
        log_verbose(TAG, "Channel Info - Primary Freq: %u, Seconday Freq1: %u, Secondary Freq2: %u, PhyMode: %u, tsfDelta: %u",
                    scan->frequency,
                    scan->band_center_freq1,
                    scan->band_center_freq2,
                    scan->info,
                    scan->tsfDelta);

        /* Load 11mc suport info */
        scan->ranging_features_supported = pAp->u_Ranging_features_supported;

        /* Load LCI and LCR suport info */
        scan->location_features_supported = pAp->u_location_features_supported;

        scan->isSecure = false;
        if (TRUE == pAp->is_secure)
        {
          scan->isSecure = true;
        }

        scan->type = ACCESS_POINT;  // defaults to access point since this call is only used by drivers that only support APs

        if (scan->bssid.isLocallyAdministeredMac() == true)
        {
          scan->type = SOFT_AP;
        }

        log_verbose(TAG, "SSID Length = %d", pAp->u_lenSSID);
        if (pAp->u_lenSSID == 0 && pAp->u_SSID[0] == 0)
        {
          log_verbose(TAG, "SSID invalid");
        }
        else
        {
          scan->ssid.setSSID(pAp->u_SSID, pAp->u_lenSSID);
        }

        if (TRUE == pAp->is_msap)
        {
          log_verbose(TAG, "MSAP enabled");
          scan->msapInfo = new(std::nothrow) LOWIMsapInfo;
          if (NULL == scan->msapInfo)
          {
            log_error(TAG, "%s, Memory allocation failure", __func__);
            delete scan;
            retVal_internal = false;
            break;
          }

          scan->msapInfo->protocolVersion = pAp->msap_prot_ver;
          scan->msapInfo->venueHash = pAp->venue_hash;
          scan->msapInfo->serverIdx = pAp->svr_idx;
        }
        else
        {
          scan->msapInfo = NULL;
        }

        scan->cellPowerLimitdBm = pAp->cell_power_limit_dBm;
        scan->indoor_outdoor = pAp->indoor_outdoor;
        memcpy(scan->country_code, pAp->country_code, LOWI_COUNTRY_CODE_LEN);

        if (pAp->u_numMeas > 0)
        {
          log_verbose(TAG, "Num of measurements = %d", pAp->u_numMeas);
          if (false == injectMeasurementInfo
              (scan->measurementsInfo, pAp, rssi_threshold_for_rtt))
          {
            log_warning(TAG, "Inject measurement info failed");
            delete scan;
            retVal_internal = false;
            break;
          }
        }
        result->scanMeasurements.push_back(scan);
      } // for
      if (false == retVal_internal)
      {
        // Any error in the inner loop is a signal that there is some
        // memory allocation issue and we may not continue.
        // Let's break out of the outer loop as well.
        retVal = false;
        break;
      }
    } // outer for
  } while (0);
  if (false == retVal)
  {
    log_error(TAG, "Could not generate the measurement result");
  }
  return retVal;
}

bool LOWIWifiDriverUtils::injectMeasurementInfo
(vector<LOWIMeasurementInfo*>& vec,
 quipc_wlan_ap_meas_s_type *pAp,
 int32 rssi_threshold_for_rtt)
{
  log_verbose(TAG, "injectMeasurementInfo");
  bool retVal = false;
  do
  {
    if (NULL == pAp)
    {
      log_error(TAG, "No measurements");
      break;
    }

    unsigned char num_of_meas = pAp->u_numMeas;
    bool inner_loop_success = true;
    // For each measurement info - insert
    LOWIMeasurementInfo *info = NULL;
    for (unsigned char ii = 0; ii < num_of_meas; ++ii)
    {
      info = new(std::nothrow)
             LOWIMeasurementInfo;
      if (NULL == info)
      {
        inner_loop_success = false;
        break;
      }
      quipc_wlan_ap_meas_info_s_type *pMeas = &pAp->p_meas_info[ii];
      if (NULL == pMeas)
      {
        inner_loop_success = false;
        break;
      }

      info->meas_age = pMeas->l_measAge_msec;
      info->rssi_timestamp = pMeas->t_measTime_usec / 1000;
      log_verbose(TAG, "Measurement timestamp = %llu", info->rssi_timestamp);
      info->rssi = pMeas->x_RSSI_0p5dBm;

      info->rtt = 0;
      info->rtt_ps = 0;
      info->rtt_timestamp = 0;
      if (0 != pMeas->w_RTT_nsec)
      {
        info->rtt_timestamp = pMeas->t_measTime_usec / 1000;
        info->rtt_ps = pMeas->w_RTT_nsec*1000;
        info->rtt = pMeas->w_RTT_nsec;
      }

      vec.push_back(info);
    }

    if (inner_loop_success != false)
    {
      retVal = true;
    }
    else
    {
      delete info;
    }
  } while (0);
  return retVal;
}

int* LOWIWifiDriverUtils::getSupportedFreqs(
  LOWIDiscoveryScanRequest::eBand band,
  s_ch_info *p_ch_info,
  unsigned char& num_channels)
{
  if (0 == p_ch_info->num_2g_ch && 0 == p_ch_info->num_5g_ch)
  {
    log_debug(TAG, "getSupportedFreqs - supported channel list not found"
              " using the default");
    return LOWIUtils::getChannelsOrFreqs(band,
                                         num_channels, true);
  }
  int *freqs = NULL;
  switch (band)
  {
  case LOWIDiscoveryScanRequest::TWO_POINT_FOUR_GHZ:
    {
      num_channels = p_ch_info->num_2g_ch;
      freqs = new(std::nothrow) int[num_channels];
      if (NULL != freqs)
      {
        for (unsigned char ii = 0; ii < num_channels; ++ii)
        {
          freqs[ii] = p_ch_info->arr_2g_ch[ii];
        }
      }
      break;
    }
  case LOWIDiscoveryScanRequest::FIVE_GHZ:
    {
      num_channels = p_ch_info->num_5g_ch;
      freqs = new(std::nothrow) int[num_channels];
      if (NULL != freqs)
      {
        for (unsigned char ii = 0; ii < num_channels; ++ii)
        {
          freqs[ii] = p_ch_info->arr_5g_ch[ii];
        }
      }
      break;
    }
  default:
    // All
    {
      num_channels = p_ch_info->num_2g_ch + p_ch_info->num_5g_ch;
      freqs = new(std::nothrow) int[num_channels];
      if (NULL != freqs)
      {
        // First copy the 2.4 Ghz freq / channels
        int index = 0;
        for (; index < p_ch_info->num_2g_ch; ++index)
        {
          freqs[index] = p_ch_info->arr_2g_ch[index];
        }
        // Copy the 5 Ghz freq / channels
        for (int ii = 0; ii < p_ch_info->num_5g_ch; ++ii)
        {
          freqs[index + ii] = p_ch_info->arr_5g_ch[ii];
        }
      }
      break;
    }
  }
  return freqs;
}

void LOWIWifiDriverUtils::cleanupWifiCapability()
{
  if (NULL != iwOemDataCap)
  {
    free(iwOemDataCap);
    iwOemDataCap = NULL;
  }
  if (ioctlSock > 0)
  {
    close(ioctlSock);
    ioctlSock = 0;
  }
}

LOWIWifiDriverUtils::eGetWiFiCapabilityError
LOWIWifiDriverUtils::getWiFiIdentityandCapability(IwOemDataCap* pIwOemDataCap, LOWIMacAddress &localStaMac)
{
  eGetWiFiCapabilityError retVal = CAP_FAILURE;
  struct iwreq req;
  struct iwreq req_staMac;
  char   gInterface[PROPERTY_VALUE_MAX];
  int    attempted_copy_length = 0;
  char rxBuff[RX_BUFF_SIZE];

  if(iwOemDataCap != NULL) /* The Local Copy of Capabilities is valid */
  {
    memcpy(pIwOemDataCap, iwOemDataCap, sizeof(IwOemDataCap));
    return CAP_SUCCESS;
  }

  memset(&req, 0, sizeof(req));
  memset(&req_staMac, 0, sizeof(req_staMac));
  if(ioctlSock <= 0) /* Open IOCTL socket if socket not already open or failed to open the last time */
  {
    ioctlSock = socket(AF_INET, SOCK_DGRAM, 0);
    if (ioctlSock < 0)
    {
      log_error(TAG, "getWiFiIdentityandCapability - Failed to open IOCTL socket");
      return CAP_IOCTL_FAILURE;
    }
    else
    {
      log_verbose(TAG, "getWiFiIdentityandCapability - Successfully Openned IOCTL socket");
    }
  }

  if (ioctlSock > 0) /* Get Capability information if Socket open and valid */
  {
    /* Get the Wi-Fi Capabilities and chip identity */
    int ret = -1;
#ifndef LOWI_ON_ACCESS_POINT
    ret = property_get("wifi.interface", gInterface, 0);
#endif
    if(ret < 0)
    {
      log_error(TAG, "getWiFiIdentityandCapability - property_get returned: %d, %s\n", ret, gInterface);
      return CAP_SYSTEM_PROP_FAILURE;
    }
    log_verbose(TAG, "getWiFiIdentityandCapability - property_get returned: %s\n", gInterface);
    memset(rxBuff, 0xa, RX_BUFF_SIZE);
    attempted_copy_length = strlcpy(req.ifr_name, gInterface, IFNAMSIZ);
    attempted_copy_length = strlcpy(req_staMac.ifr_name, gInterface, IFNAMSIZ);
    if(attempted_copy_length >= IFNAMSIZ)
    {
      log_warning(TAG, "getWiFiIdentityandCapability - Interface name was truncated!!");
    }
    req.u.data.flags = WE_GET_OEM_DATA_CAP;
    req.u.data.pointer = rxBuff;
    req.u.data.length = RX_BUFF_SIZE;

    //retry for few times every one second for ioctl socket
    //as could be possible WIFI driver is not up at this time.
    for (int i = 0; i <= MAX_CAPABILITY_REQ_RETRY_COUNT; i++)
    {
      ret = ioctl(ioctlSock, WLAN_PRIV_GET_OEM_DATA_CAP, &req);
      if ((ret < 0) && (errno == ENODEV))
      {
        log_error(TAG, "getWiFiIdentityandCapability - ioctl(GET_CAP) returned: %d, "
                  "retrying... errno: %s(%d)\n", ret, strerror(errno), errno);
        retVal = CAP_WLAN_DEV_FAILURE;
      }
      else
      {
        log_debug(TAG, "getWiFiIdentityandCapability - ioctl(GET_CAP) returned: %d, "
                  "Error: %s", ret, (ret < 0 ? strerror(errno): "None"));
        {
          /* Ask for STA MAC Address */
          errno = 0;
          ret = ioctl(ioctlSock, SIOCGIFHWADDR, &req_staMac);
          if (ret < 0)
          {
            log_debug(TAG, "%s - ioctl(GET_CAP) returned: %d, Errno: %s(%d)\n", __FUNCTION__, ret, strerror(errno), errno);
            retVal = CAP_IOCTL_FAILURE;
          }
          else
          {
            log_debug(TAG, "%s - ioctl(GET_MAC_ADDR) returned: %d, Error: None", __FUNCTION__, ret);
            log_debug(TAG, "%s - Local STA's MAC address : " QUIPC_MACADDR_FMT, __FUNCTION__,
                           QUIPC_MACADDR(req_staMac.u.addr.sa_data));
            localStaMac = LOWIMacAddress((const unsigned char*) req_staMac.u.addr.sa_data);
            localStaMac.print();
            retVal = CAP_SUCCESS;
          }
        }
        break;
      }
      if ( i < MAX_CAPABILITY_REQ_RETRY_COUNT)
      {
        sleep(CAPABILITY_RETRY_WAIT_TIME_IN_SEC);
      }
    }

    // Local Copy of Capabilities is not valid, allocaing memory and
    // collecting Capabilities from Host Driver
    iwOemDataCap = (IwOemDataCap*)calloc(1, sizeof(IwOemDataCap));
    if(iwOemDataCap == NULL)
    {
      log_error(TAG, "getWiFiIdentityandCapability: Failed to allocate %u bytes", sizeof(IwOemDataCap));
      close (ioctlSock);
      ioctlSock = 0;
      return CAP_NO_MEM;
    }

    memcpy(pIwOemDataCap, rxBuff, sizeof(IwOemDataCap));
    /* Store capabilities, making capabilities available for lower layers */
    memcpy(iwOemDataCap, rxBuff, sizeof(IwOemDataCap));

  }

  return retVal;
}

bool LOWIWifiDriverUtils::setDiscoveryScanType (LOWIDiscoveryScanRequest::eScanType type)
{
  bool ret = false;
  char buf[BUF_LEN];
  log_debug (TAG, "setDiscoveryScanType to %d", type);

  if (type == LOWIDiscoveryScanRequest::PASSIVE_SCAN)
  {
    strlcpy(buf, "SCAN-PASSIVE", BUF_LEN-1);
  }
  else if (type == LOWIDiscoveryScanRequest::ACTIVE_SCAN)
  {
    strlcpy(buf, "SCAN-ACTIVE", BUF_LEN-1);
  }
  else
  {
    log_debug (TAG, "Scan type other that active / passive. return");
    return ret;
  }
  //return sendDriverCmd (buf);
  return ret;
}
/*
bool LOWIWifiDriverUtils::sendDriverCmd (char* buf)
{
  bool ret = false;
  int      retVal = -EINVAL;
  char     *gInterface;     // Interface name (from Android property) on which we issue ioctls
  gInterface = lowi_wifidriver_utils_get_default_ifname();
  if (gInterface == 0)
  {
    log_debug(TAG, "property_get returned: NULL\n");
    return ret;
  }

  log_debug(TAG, "wifi.interface is %s\n", gInterface );

  int      ioctlSock = 0;
  ioctlSock = socket( AF_INET, SOCK_DGRAM, 0 );
  if ( ioctlSock < 0 )
  {
      log_debug(TAG, "sendDriverCmd: socket(ioctlSock) returned: %d\n", errno);
      return ret;
  }

  struct ifreq        wrq;
  int                 attempted_copy_length;
  s_wifi_priv_cmd priv_cmd;
  memset( &wrq, 0, sizeof(wrq) );
  memset(&priv_cmd, 0, sizeof(priv_cmd));
  attempted_copy_length = strlcpy( wrq.ifr_name, gInterface, IFNAMSIZ );
  if (attempted_copy_length >= IFNAMSIZ )
  {
    log_debug(TAG, "%s]%d %s was truncated!!", __func__, __LINE__, gInterface);
  }

  priv_cmd.buf = buf;
  priv_cmd.used_len = strlen(buf);
  priv_cmd.total_len = strlen(buf);
  wrq.ifr_data = &priv_cmd;

  retVal = ioctl( ioctlSock, SIOCDEVPRIVATE + 1, &wrq );
  if ( retVal < 0 )
  {
      log_debug(TAG, "ioctl(SIOCDEVPRIVATE + 1) returned %d\n", errno);
  }
  log_debug(TAG, "Buffer = %s", buf);
  if ( ioctlSock > 0 )
  {
      close( ioctlSock );
      ioctlSock = 0;
  }

  ret = true;
  return ret;
}
*/
char * LOWIWifiDriverUtils::get_interface_name()
{
  if (0 == wlan_ifname[0])
  {
#ifdef __ANDROID__
    if (property_get("wifi.interface", wlan_ifname, DEFAULT_WLAN_INTERFACE) != 0)
    {
      log_debug("LOWIWpaInterface", "Using interface '%s'\n", wlan_ifname);
      return wlan_ifname;
    }
#else
    strlcpy(wlan_ifname, DEFAULT_WLAN_INTERFACE, sizeof(wlan_ifname));
#endif
  }
  return wlan_ifname;
}

eWifiIntfState LOWIWifiDriverUtils::getInterfaceState()
{
  eWifiIntfState state = INTF_UNKNOWN;

#ifdef LOWI_ON_ACCESS_POINT
  const char* ifr_num[] = { "0", "1", "2" };
#else
  const char* ifr_num[] = { "" };
#endif
  int ifr_count = 0;

  do
  {
    if (ioctlSock <= 0) /* Open IOCTL socket if socket not already open or failed to open the last time */
    {
      ioctlSock = socket(AF_INET, SOCK_DGRAM, 0);
      if (ioctlSock < 0)
      {
        log_error(TAG, "%s - Failed to open IOCTL socket", __FUNCTION__);
        break;
      }
    }
    struct ifreq req;
    memset(&req, 0, sizeof(req));
    strlcpy(req.ifr_name, get_interface_name(), IFNAMSIZ);
    strlcat(req.ifr_name, ifr_num[ifr_count], IFNAMSIZ);
    errno = 0;
    int ret = ioctl(ioctlSock, SIOCGIFFLAGS, &req);
    if (ret < 0)
    {
      log_debug(TAG, "%s - ioctl(GET_IFFLAGS) returned: %d, Errno: %s(%d)",
                __FUNCTION__, ret, strerror(errno), errno);
    }
    else
    {
      log_debug(TAG, "%s: ifname %s, flags 0x%x (%s%s)", __FUNCTION__,
                req.ifr_name, req.ifr_flags,
                (req.ifr_flags & IFF_UP) ? "[UP]" : "",
                (req.ifr_flags & IFF_RUNNING) ? "[RUNNING]" : "" );
      state = (req.ifr_flags & IFF_RUNNING ? INTF_RUNNING :
               (req.ifr_flags & IFF_UP     ? INTF_UP      : INTF_DOWN));
      if ((state == INTF_UP) || (state == INTF_RUNNING))
      {
        break;
      }
    }
  }
  while ((++ifr_count) < sizeof(ifr_num)/sizeof(const char*));
  return state;
}

