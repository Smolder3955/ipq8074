/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Wifi Driver Interface

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Wifi Driver Interface

Copyright (c) 2012-2014 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#include <string.h>
#include <base_util/log.h>
#include <lowi_server/lowi_wifidriver_interface.h>
#include <lowi_server/lowi_rome_wifidriver.h>
#include "lowi_wifidriver_utils.h"
#include "wipsiw.h"

using namespace qc_loc_fw;

const char * const LOWIWifiDriverInterface::TAG = "LOWIWifiDriverInterface";

const char *LOWIWifiDriverInterface::modeStr[REQUEST_SCAN+1] =
{
  "DISCOVERY_SCAN",
  "RANGING_SCAN",
  "REQUEST_SCAN"
};

LOWIWifiDriverInterface::LOWIWifiDriverInterface (ConfigFile* config)
: mConfig (config), mMsgQueue (NULL), mMutex (NULL)
{
  log_verbose (TAG, "LOWIWifiDriverInterface ()");
  if (NULL == mConfig)
  {
    log_error (TAG, "Handle to Config file is null");
  }

  mMutex = Mutex::createInstance("LOWIWifiDriverInterface",false);
  if(0 == mMutex)
  {
    log_error(TAG, "Cannot allocate mutex for LOWIWifiDriverInterface");
  }

  // Create the blocking Queue
  mMsgQueue = BlockingQueue::createInstance ("LOWIWifiDriverInterfaceQ");
  if (NULL == mMsgQueue)
  {
    log_error (TAG, "Unable to create Message Queue");
  }

  // initialize the Discovery Scan Driver Layer
  lowi_scan_drv_open();

  //set the default values as for external and fake wifi driver
  mCapabilities.supportedCapablities = LOWI_DISCOVERY_SCAN_SUPPORTED;
}

LOWIWifiDriverInterface::~LOWIWifiDriverInterface ()
{
  log_verbose (TAG, "~LOWIWifiDriverInterface ()");
  LOWIWifiDriverUtils::cleanupWifiCapability ();
  mMsgQueue->close();
  delete mMsgQueue;
  delete mMutex;
}

LOWIWifiDriverInterface* LOWIWifiDriverInterface::createInstance
(ConfigFile* config,
 LOWIScanResultReceiverListener* scanResultListener,
 LOWIInternalMessageReceiverListener* internalMessageListener,
 LOWICacheManager* cacheManager)
{
  log_verbose (TAG, "createInstance ()");
#ifndef LOWI_ON_ACCESS_POINT
  int retValCap = 0;
  IwOemDataCap iwOemDataCap;
  LOWIMacAddress staMac;
  memset(&iwOemDataCap, 0, sizeof(IwOemDataCap));
  retValCap = LOWIWifiDriverUtils::getWiFiIdentityandCapability(&iwOemDataCap, staMac);

  switch (retValCap)
  {
  case LOWIWifiDriverUtils::CAP_NO_MEM:
  case LOWIWifiDriverUtils::CAP_WLAN_DEV_FAILURE:
  case LOWIWifiDriverUtils::CAP_SYSTEM_PROP_FAILURE:
    {
      log_error(TAG, "createInstance - WLAN Device not ready/ Mem failure: %d", retValCap);
      return NULL;
    }
    break;
  case LOWIWifiDriverUtils::CAP_IOCTL_FAILURE:
    // Ioctl failure, this may happen on an external driver or QC driver
    // that does not support this ioctl
    log_debug(TAG, "createInstance - Ioctl failure");
    break;
  case LOWIWifiDriverUtils::CAP_SUCCESS:
    {
      if ((TARGET_TYPE_ROME_1_1 == iwOemDataCap.oem_target_type) ||
          (TARGET_TYPE_ROME_1_3 == iwOemDataCap.oem_target_type) ||
          (TARGET_TYPE_ROME_2_1 == iwOemDataCap.oem_target_type))
      {
        log_verbose(TAG, "%s - Local STA MAC: " QUIPC_MACADDR_FMT,
                    __FUNCTION__, QUIPC_MACADDR(staMac));
        /* Store local STA's MAC address in cache. This will be used later when
           sending out frames to Associated Access Points */
        if (cacheManager)
        {
          cacheManager->storeStaMacInCache(staMac);
        }
        log_verbose(TAG, "createInstance - Got Capability information: %d\n", retValCap);
        log_verbose(TAG, "createInstance - oem_target_signature: %s\n", iwOemDataCap.oem_target_signature);
        log_verbose(TAG, "createInstance - oem_target_type: %u, oem_fw_version: 0x%x, driver_version: %u %u %u %u",
                    iwOemDataCap.oem_target_type, iwOemDataCap.oem_fw_version,
                    iwOemDataCap.driver_version.major, iwOemDataCap.driver_version.minor,
                    iwOemDataCap.driver_version.patch, iwOemDataCap.driver_version.build);
        log_verbose(TAG, "createInstance - allowed_dwell_time min: %d, max: %d\n",
                    iwOemDataCap.allowed_dwell_time_min, iwOemDataCap.allowed_dwell_time_max);
        log_verbose(TAG, "createInstance - curr_dwell_time min: %d, max: %d\n",
                    iwOemDataCap.curr_dwell_time_min, iwOemDataCap.curr_dwell_time_max);
        log_verbose(TAG, "createInstance - supported_bands: %u &  num_channels: %u\n",
                    iwOemDataCap.supported_bands, iwOemDataCap.num_channels);
        for (int i = 0; i < iwOemDataCap.num_channels; i++)
        {
          log_verbose(TAG, "createInstance - Channel ID: %u Supported\n", iwOemDataCap.channel_list[i]);
        }
#endif // #ifndef LOWI_ON_ACCESS_POINT
        log_verbose(TAG, "Creating Rome wifi Driver");
        LOWIWifiDriverInterface *ptr = new(std::nothrow) LOWIROMEWifiDriver(config, scanResultListener, internalMessageListener, cacheManager);
        if (NULL == ptr)
        {
          log_error(TAG, "Unable to create the rome target Driver!");
        }
        else
        {
          LOWICapabilities configCap;

          // initialize to do-not-care for now
          configCap.activeScanSupported = true;
          configCap.discoveryScanSupported = true;
          configCap.rangingScanSupported = true;
          ptr->configCapabilities(configCap);
        }

        return ptr;
#ifndef LOWI_ON_ACCESS_POINT
      }
    }
    break;
  default:
    log_debug(TAG, "createInstance - Default. Cap error = %d", retValCap);
    break;
  }

  bool wifi_vendor_is_ath = false;
  bool qcom_chipset = true;

  // Change the default for premium mode to be true.
  // Right now lowi only caters to clients in premium mode only but
  // this may change in the future.
  bool izat_premium_enabled = true;

  LOWIWifiDriverInterface *ptr1 = NULL;
  LOWICapabilities configCap;

  // initialize to do-not-care for now
  configCap.activeScanSupported = true;
  configCap.discoveryScanSupported = true;
  configCap.rangingScanSupported = true;

  if (false != qcom_chipset)
  {
    // for QC wifi chipset, if basic, ranging scan needs to be configured to
    // be disabled
    if (false == izat_premium_enabled)
    {
      configCap.rangingScanSupported = false;
    }

  }

  if (NULL == ptr1)
  {
    log_error(TAG, "Unable to create the target Driver!");
  }
  else
  {
    ptr1->configCapabilities(configCap);
  }
  return ptr1;
#endif //#ifndef LOWI_ON_ACCESS_POINT
}

LOWIRTTInfo*
LOWIWifiDriverInterface::processRTT (vector <LOWIScanMeasurement*> & v)
{
  // TODO - Implement the function later
  return NULL;
}

void LOWIWifiDriverInterface::configCapabilities ( LOWICapabilities& configCap )
{
  mCapabilities.rangingScanSupported = (mCapabilities.rangingScanSupported && configCap.rangingScanSupported);
  mCapabilities.discoveryScanSupported = (mCapabilities.discoveryScanSupported && configCap.discoveryScanSupported);
  mCapabilities.activeScanSupported = (mCapabilities.activeScanSupported && configCap.activeScanSupported);
}

void LOWIWifiDriverInterface::setNewRequest(const LOWIRequest* r, eListenMode mode)
{
  AutoLock autolock(mMutex);
  mReq = r;
}

int LOWIWifiDriverInterface::unBlock (eListenMode mode)
{
  log_debug (TAG, "unBlock Mode = %s", LOWI_TO_STRING( mode, modeStr ));
  AutoLock autolock(mMutex);
  if (mode == this->DISCOVERY_SCAN)
  {
    return Wips_nl_shutdown_communication();
  }
  else
  {
    long event = 1;
    mMsgQueue->push ((void*) event);
    return 1; // Indicate that 1 byte is written (operation success)
  }
}

int LOWIWifiDriverInterface::terminate (eListenMode mode)
{
  return unBlock(mode);
}

int LOWIWifiDriverInterface::initFileDescriptor (eListenMode mode)
{
  log_debug (TAG, "initFileDescriptor Mode = %s", LOWI_TO_STRING( mode, modeStr ));
  AutoLock autolock(mMutex);

  if (mode == DISCOVERY_SCAN)
  {
    return Wips_nl_init_pipe();
  }
  else
  {
    log_verbose (TAG, "initFileDescriptor not supported in RANGING mode");
    return 0;
  }
}

int LOWIWifiDriverInterface::closeFileDescriptor (eListenMode mode)
{
  log_debug (TAG, "closeFileDescriptor Mode = %s", LOWI_TO_STRING( mode, modeStr ));
  AutoLock autolock(mMutex);

  if (mode == DISCOVERY_SCAN)
  {
    return Wips_nl_close_pipe();
  }
  else
  {
    log_verbose (TAG, "closeFileDescriptor not supported in RANGING mode");
    return 0;
  }
}

LOWIMeasurementResult* LOWIWifiDriverInterface::block
(LOWIRequest* request, eListenMode mode)
{
  LOWIMeasurementResult* result = NULL;
  result = getMeasurements (request, mode);
  return result;
}

void LOWIWifiDriverInterface::processWlanFrame() const
{
}
eWifiIntfState LOWIWifiDriverInterface::getInterfaceState()
{
  return LOWIWifiDriverUtils::getInterfaceState();
}
