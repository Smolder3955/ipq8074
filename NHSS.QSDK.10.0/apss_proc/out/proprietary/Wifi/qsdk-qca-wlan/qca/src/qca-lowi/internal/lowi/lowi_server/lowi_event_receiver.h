#ifndef __LOWI_EVENT_RECEIVER__
#define __LOWI_EVENT_RECEIVER__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Event Receiver Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Event Receiver

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/

#include <base_util/postcard.h>
#include <inc/lowi_request.h>
#include <inc/lowi_scan_measurement.h>
#include <lowi_server/lowi_local_msg.h>

namespace qc_loc_fw
{

class LOWIEventReceiverListener {
public:
  /** Destructor */
  virtual ~LOWIEventReceiverListener ()
  {

  }


  /**
   * Notifies to the listener about the wifi state changes
   * @param eWifiIntfState Wifi Interface state
   */
  virtual void updateWifiState (eWifiIntfState state) = 0;
};

/**
 * This class handles the incoming events / postcard from the LOWIController.
 * Analyzes the postcard to determine if it contains a new request or scan
 * measurements. Converts them to a LOWILocalMsg container that might have a
 * Request or scan measurements.
 *
 * It works as a shim layer to convert the InPostCard to a structure
 */
class LOWIEventReceiver
{
private:
  static const char * const TAG;
  LOWIEventReceiverListener* mListener;
public:
  /**
   * Handles Event sent by LOWIController
   * Converts the Event received to a LocalMsg
   * @param InPostCard Event received in the PostCard format
   * @return LOWILocalMsg container which may have a request or scan measurements
   */
  LOWILocalMsg* handleEvent (InPostcard * const in_msg);

  /**
   * Adds a listener to the LOWIEventReceiver
   * @param LOWIEventReceiverListener* listener to be added
   */
  void addListener (LOWIEventReceiverListener* pListener);

  /**
   * Removes a listener from the LOWIEventReceiver
   * @param LOWIEventReceiverListener* listener to be removed
   */
  void removeListener (LOWIEventReceiverListener* pListener);

  /**
   * Constructor
   */
  LOWIEventReceiver();
  /** Destructor*/
  ~LOWIEventReceiver();
protected:
};
} // namespace
#endif //#ifndef __LOWI_EVENT_RECEIVER__
