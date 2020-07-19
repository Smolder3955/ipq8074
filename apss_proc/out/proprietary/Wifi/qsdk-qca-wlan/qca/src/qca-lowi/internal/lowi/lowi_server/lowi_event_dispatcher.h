#ifndef __LOWI_EVENT_DISPATCHER__
#define __LOWI_EVENT_DISPATCHER__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Event Dispatcher Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Event Dispatcher

Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/

#include <base_util/postcard.h>

#include <inc/lowi_response.h>
namespace qc_loc_fw
{
class LOWIController;

/**
 * This class handles the outgoing events / Responses.
 * Converts the responses to the events & sends them to the
 * LOWIController
 */
class LOWIEventDispatcher
{
private:
  static const char * const TAG;
  LOWIController* const mController;

public:
  /**
   * Constructor
   * @param LOWIEventController* Pointer to LOWIController class
   *                         to send the IPC messages
   */
  LOWIEventDispatcher(LOWIController* const controller);
  /** Destructor*/
  ~LOWIEventDispatcher();

  /**
   * Sends the response to LOWIController
   * @param LOWIResponse* Response to be sent
   * @param Char*         Request Originator, to which the response will go
   */
  void sendResponse (LOWIResponse* response,
      const char* const request_originator);

protected:
};
} // namespace
#endif //#ifndef __LOWI_EVENT_DISPATCHER__
