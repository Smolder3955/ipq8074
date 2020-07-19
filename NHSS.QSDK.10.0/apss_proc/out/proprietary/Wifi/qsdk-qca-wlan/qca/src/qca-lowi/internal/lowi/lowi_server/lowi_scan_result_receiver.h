#ifndef __LOWI_SCAN_RESULT_RECEIVER_H__
#define __LOWI_SCAN_RESULT_RECEIVER_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Scan Result Receiver Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Scan Result Receiver

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/

#include <base_util/sync.h>
#include <inc/lowi_request.h>
#include <inc/lowi_response.h>
#include <lowi_server/lowi_measurement_result.h>
#include <lowi_server/lowi_wifidriver_interface.h>
#include <lowi_server/lowi_scan_result_listener.h>

namespace qc_loc_fw
{
class LOWIWifiDriverInterface;

/**
 * Base class to provide the mechanism to get the scan measurements
 * from Wifi Driver. It is run in a separate thread.
 * It's responsibility is to receive the results and provide the results
 * to it's listener.
 */
class LOWIScanResultReceiver : public Runnable
{
private:
  static const char * const         TAG;

protected:

  LOWIScanResultReceiverListener*   mListener;
  LOWIWifiDriverInterface*          mWifiDriver;
  LOWIRequest*                      mRequest;
  Thread*                           mReceiverThread;
  bool                              mThreadAboutToBlock;
  bool                              mThreadTerminateRequested;
  bool                              mNewRequestArrived;
  Mutex*                            mMutex;
  LOWIWifiDriverInterface::eListenMode mListenMode;

  /**
   * Terminates the Receiver thread.
   *
   * @return true if success, false otherwise
   */
  bool terminate ();

  /**
   * Unblocks the Receiver thread if it is blocked
   * in passive listening mode.
   *
   * @return true if success, false otherwise
   */
  bool unblockThread ();

   /**
   * Main loop of the Receiver
   */
  virtual void run ();

public:

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

  /**
   * Constructor
   * @param LOWIScanResultReceiverListener* listener to be notified
   *                                   with measurement results.
   * @param LOWIWifiDriverInterface* WifiDriverInterface to be used by
   *                                   LOWIScanResultReceiver for listening
   *                                   to the scan measurements.
   */
  LOWIScanResultReceiver(LOWIScanResultReceiverListener* listener,
      LOWIWifiDriverInterface* interface);
  /** Destructor*/
  virtual ~LOWIScanResultReceiver();
protected:
};
} // namespace
#endif //#ifndef __LOWI_SCAN_RESULT_RECEIVER_H__
