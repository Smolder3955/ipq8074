#ifndef __LOWI_SCAN_REQUEST_SENDER_H__
#define __LOWI_SCAN_REQUEST_SENDER_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Scan Request Sender Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Scan Request Sender

Copyright (c) 2015 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

#include <base_util/sync.h>
#include <inc/lowi_request.h>
#include <inc/lowi_response.h>
#include <lowi_server/lowi_measurement_result.h>
#include <lowi_server/lowi_scan_result_receiver.h>

namespace qc_loc_fw
{

/**
 * This class provides the mechanism to get scan requests to the WiFi
 * Driver. Currently, it is only used to send background scan
 * requests.  It is run in a separate thread. It's responsibility is
 * to receive requests from the lowi-controller, queue them up, and
 * send them to the WiFi Driver.
 */
class LOWIScanRequestSender : public LOWIScanResultReceiver
{
private:
  static const char * const TAG;
  BlockingQueue*            mMsgQueue;

protected:
  /**
   * Main loop of the Receiver
   */
  virtual void run ();

  /**
   * Terminates the Receiver thread.
   *
   * @return true if success, false otherwise
   */
  bool terminate ();

public:
  /**
   * Constructor
   * @param LOWIScanResultReceiverListener* listener to be notified
   *                                   with measurement results.
   * @param LOWIWifiDriverInterface* WifiDriverInterface to be used by
   *                                   LOWIScanResultReceiver for listening
   *                                   to the scan measurements.
   */
    LOWIScanRequestSender(LOWIScanResultReceiverListener* listener,
                          LOWIWifiDriverInterface* interface);

  /** Destructor*/
  virtual ~LOWIScanRequestSender();

  /**
   * Initializes the Receiver and starts execution of the thread.
   * @return bool true is success, false otherwise
   */
  virtual bool init ();

  /**
   * Executes LOWIRequest.
   * @param LOWIRequest*               LOWIRequest for which the
   *                                   scan is to be performed
   * @return bool true is success, false otherwise
   */
  virtual bool execute (LOWIRequest* request);
};
} // namespace
#endif //#ifndef __LOWI_SCAN_REQUEST_SENDER_H__
