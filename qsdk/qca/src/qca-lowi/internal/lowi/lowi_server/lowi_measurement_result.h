#ifndef __LOWI_MEASUREMENT_RESULT_H__
#define __LOWI_MEASUREMENT_RESULT_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Measurement Result Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI Measurement Result

Copyright (c) 2012 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/

#include <base_util/postcard.h>
#include <inc/lowi_const.h>
#include <inc/lowi_scan_measurement.h>
#include <inc/lowi_response.h>

namespace qc_loc_fw
{

/**
 * This class defines the measurement taken for every scan request.
 * This contains the measurements corresponding the discovery, ranging
 * and background scan requests. However, the fields are valid /
 * invalid based on type of scan as documented below.
 */
class LOWIMeasurementResult
{
private:
  static const char* const TAG;
public:

  /** Type of measurement*/
  LOWIDiscoveryScanResponse::eScanTypeResponse  scanType;
  /** Scan Status*/
  LOWIResponse::eScanStatus        scanStatus;
  /** Time at which the measurements are taken*/
  int64              measurementTimestamp;
  /** Vector containing the scan measurements for each AP */
  vector <LOWIScanMeasurement*> scanMeasurements;
  /** Request to which the results correspond*/
  const LOWIRequest*       request;

  /**
   * Creates an InPostcard and inserts the measurement pointer
   * as a blob to it.
   * @param LOWIMeasurementResult* Scan Measurements
   * @return InPostcard
   */
  static InPostcard* createPostcard

  (LOWIMeasurementResult* result);

  /**
   * Parses the InPostcard and retrieves the Scan Measurement pointer
   * stored as a blob in it
   * @param InPostcard* Card
   * @return Scan Measurement pointer
   */
  static LOWIMeasurementResult* parseScanMeasurement
  (InPostcard* card);

  ~LOWIMeasurementResult ();
  LOWIMeasurementResult ();
};

} // namespace qc_loc_fw

#endif //#ifndef __LOWI_MEASUREMENT_RESULT_H__
