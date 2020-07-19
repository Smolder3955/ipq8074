#ifndef __LOWI_INTERNAL_MESSAGE_LISTENER_H__
#define __LOWI_INTERNAL_MESSAGE_LISTENER_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*
LOWI Internal Message Listener Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Internal Message Listener

Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#include <lowi_server/lowi_internal_message.h>

namespace qc_loc_fw
{

class LOWIInternalMessageReceiverListener {
public:
  /** Destructor */
  virtual ~LOWIInternalMessageReceiverListener ()
  {

  }

  /**
   * Notifies the listener that a request has been received. Used by internal
   * entities to send a request to the listener
   * @param LOWIRequest*: internal LOWI request, (i.e. not from
   *                     a external client, but from wifi driver of some
   *                     other internal source)
   */
  virtual void internalMessageReceived (LOWIInternalMessage* req) = 0;
};

} // namespace
#endif //#ifndef __LOWI_INTERNAL_MESSAGE_LISTENER_H__
