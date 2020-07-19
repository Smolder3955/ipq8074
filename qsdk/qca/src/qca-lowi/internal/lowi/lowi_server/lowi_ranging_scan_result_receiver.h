#ifndef __LOWI_RANGING_SCAN_RESULT_RECEIVER_H__
#define __LOWI_RANGING_SCAN_RESULT_RECEIVER_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Ranging Scan Result Receiver Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Ranging Scan Result Receiver

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
 * This class provides the mechanism to get the ranging scan measurements
 * from Wifi Driver. It is run in a separate thread.
 * It's responsibility is to receive the results and provide the results
 * to it's listener. After that it blocks on a blocking queue waiting for
 * a ranging scan request. Upon receiving a new ranging request, the
 * thread unblocks and issues the request to WLAN drive.
 */
class LOWIRangingScanResultReceiver : public LOWIScanResultReceiver
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
  LOWIRangingScanResultReceiver(LOWIScanResultReceiverListener* listener,
                                LOWIWifiDriverInterface* interface);

  /** Destructor*/
  virtual ~LOWIRangingScanResultReceiver();

};
} // namespace
#endif //#ifndef __LOWI_RANGING_SCAN_RESULT_RECEIVER_H__
