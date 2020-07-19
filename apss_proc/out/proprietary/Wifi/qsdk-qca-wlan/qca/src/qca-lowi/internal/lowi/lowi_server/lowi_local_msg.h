#ifndef __LOWI_LOCAL_MSG_H__
#define __LOWI_LOCAL_MSG_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Local Msg Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Local Msg

Copyright (c) 2012 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

=============================================================================*/

#include <inc/lowi_request.h>
#include <lowi_server/lowi_measurement_result.h>
#include <lowi_server/lowi_internal_message.h>

namespace qc_loc_fw
{
/**
 * This class defines a LOWILocalMsg handled by the LOWIController
 *
 * The class is just a container class which could contain a valid
 * new Request or Scan measurements received from the recent scan.
 *
 * This class is just a container and does not delete the contents.
 */
class LOWILocalMsg
{
private:
  static const char * const TAG;
  bool mContainsRequest;
  bool mContainsInternalMessage;

  /**
   * Pointer to the Request. It will be NULL if container
   * does not hold a valid Request.
   */
  LOWIRequest*          mRequest;
  /**
   * Scan Measurements
   * It will be NULL if container does not hold scan measurements.
   */
  LOWIMeasurementResult * mMeasurementResults;

  /**
   * Pointer to an Internal Message. It will be NULL if container does not hold
   * a valid internal message
   */
  LOWIInternalMessage *mInternalMessage;

public:
  /**
   * Constructor
   * @param LOWIRequest* LOWIRequest
   */
  LOWILocalMsg (LOWIRequest* req);

  /**
   * Constructor
   * @param LOWIMeasurementResult* scan measurements
   */
  LOWILocalMsg (LOWIMeasurementResult * meas);

  /**
   * Constructor
   * @param LOWIInternalMessage* LOWIInternalMessage
   */
  LOWILocalMsg (LOWIInternalMessage *req);

  /** Destructor*/
  ~LOWILocalMsg ();

  /**
   * Returns the LOWIRequest
   * @return LOWIRequest
   */
  LOWIRequest* getLOWIRequest ();

  /**
   * Checks if the Msg contains a Request.
   * @return true if yes, false otherwise
   */
  bool containsRequest ();

  /**
   * Returns the measurement result
   * @return Measurement result
   */
  LOWIMeasurementResult * getMeasurementResult ();

  /**
   * Returns the LOWIInternalMessage
   * @return LOWIInternalMessage
   */
  LOWIInternalMessage* getLOWIInternalMessage ();

  /**
   * Checks if the Msg contains an internal message.
   * @return true if yes, false otherwise
   */
  bool containsInternalMessage ();

};
}  // namespace
#endif //#ifndef __LOWI_LOCAL_MSG_H__
