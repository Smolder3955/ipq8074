/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Discovery Scan Result Receiver

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Discovery Scan Result Receiver

Copyright (c) 2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/
#include <string.h>
#include <base_util/log.h>
#include <lowi_server/lowi_discovery_scan_result_receiver.h>
#include <lowi_server/lowi_measurement_result.h>
#include <lowi_server/lowi_wifidriver_interface.h>
#include <unistd.h>

using namespace qc_loc_fw;

const char * const LOWIDiscoveryScanResultReceiver::TAG =
    "LOWIDiscoveryScanResultReceiver";

LOWIDiscoveryScanResultReceiver::LOWIDiscoveryScanResultReceiver
(
    LOWIScanResultReceiverListener* listener,
    LOWIWifiDriverInterface* interface
)
: LOWIScanResultReceiver (listener, interface)
{
  log_verbose (TAG, "LOWIDiscoveryScanResultReceiver");
  mListenMode = LOWIWifiDriverInterface::DISCOVERY_SCAN;
}

LOWIDiscoveryScanResultReceiver::~LOWIDiscoveryScanResultReceiver ()
{
  log_verbose (TAG, "~LOWIDiscoveryScanResultReceiver");
}
