/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Ranging Scan Result Receiver

GENERAL DESCRIPTION
  This file contains the implementation of LOWI Scan Result Receiver

Copyright (c) 2013-2014 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/
#include <string.h>
#include <base_util/log.h>
#include <lowi_server/lowi_ranging_scan_result_receiver.h>
#include <lowi_server/lowi_measurement_result.h>
#include <lowi_server/lowi_wifidriver_interface.h>
#include <unistd.h>

using namespace qc_loc_fw;

const char * const LOWIRangingScanResultReceiver::TAG =
    "LOWIRangingScanResultReceiver";

LOWIRangingScanResultReceiver::LOWIRangingScanResultReceiver
(
    LOWIScanResultReceiverListener* listener,
    LOWIWifiDriverInterface* interface
)
: LOWIScanResultReceiver(listener, interface)
{
  log_verbose (TAG, "LOWIRangingScanResultReceiver");
  mListenMode = LOWIWifiDriverInterface::RANGING_SCAN;
}

LOWIRangingScanResultReceiver::~LOWIRangingScanResultReceiver ()
{
  log_verbose (TAG, "~LOWIRangingScanResultReceiver");
}
