#ifndef __LOWI_DISCOVERY_SCAN_RESULT_RECEIVER_H__
#define __LOWI_DISCOVERY_SCAN_RESULT_RECEIVER_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Discovery Scan Result Receiver Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Scan Result Receiver

Copyright (c) 2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/

#include <base_util/sync.h>
#include <inc/lowi_request.h>
#include <inc/lowi_response.h>
#include <lowi_server/lowi_measurement_result.h>
#include <lowi_server/lowi_scan_result_receiver.h>

namespace qc_loc_fw
{

/**
 * This class provides the mechanism to get the discovery scan
 * measurements from Wifi Driver. It is run in a separate thread.
 * It's responsibility is to receive the results and provide the results
 * to it's listener. It may receive the scan measurements even when there
 * was no request made by LOWI to the wifi driver (Passive listening).
 * It will create the memory to store the scan measurements on heap
 * and then pass the pointer to the memory to the listener.
 *
 * In case of any socket error, it will re-establish the connection
 * with the socket. This may happen if the socket this thread is
 * listening on is closed by the LOWIController thread to trigger reset.
 */
class LOWIDiscoveryScanResultReceiver : public LOWIScanResultReceiver
{
private:
  static const char * const         TAG;

protected:

public:
  /**
   * Constructor
   * @param LOWIScanResultReceiverListener* listener to be notified
   *                                   with measurement results.
   * @param LOWIWifiDriverInterface* WifiDriverInterface to be used by
   *                                   LOWIScanResultReceiver for listening
   *                                   to the scan measurements.
   */
  LOWIDiscoveryScanResultReceiver(LOWIScanResultReceiverListener* listener,
      LOWIWifiDriverInterface* interface);

  /** Destructor*/
  virtual ~LOWIDiscoveryScanResultReceiver();

};
} // namespace
#endif //#ifndef __LOWI_DISCOVERY_SCAN_RESULT_RECEIVER_H__
