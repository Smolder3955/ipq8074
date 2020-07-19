#ifndef __LOWI_RTT_RANGE_PROCESSING_H__
#define __LOWI_RTT_RANGE_PROCESSING_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI RTT Range Processing Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWI RTT Range Processing

Copyright (c) 2012 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#include <osal/commontypes.h>

#define MAX_CORR_VECTOR_LEN 10

/**
 * This class defines the RTTInfo
 */
class LOWIRTTInfo
{
public:
  /* Structure for RTT Logging */
  int32 rtt_adj_ns; /* RTT Adjustment in nano seconds */
  /* Correlation Vector. For floating point value, divide by 65536 */
  int32 norm_corr_vector[MAX_CORR_VECTOR_LEN];
  uint8 corr_len;   /* Length of values in correlation vector */
  uint8 csd_mode;   /* CSD Mode */
  int16 confidence; /* Confidence in CSD Mode*/
};

/**
 * This class provides the CFR (Channel Frequency Response) processing
 * For certain QCA chipsets, the CFR processing may be done within the
 * WLAN SW stack itself, and as a result of that processing, a correction
 * value is received by the clients of the Driver.
 * This class exists to be able to support both types of WLAN driver,
 * WLAN driver that only gives raw CFR data and WLAN driver that gives
 * the output of the CFR processing.
 * The processing of the CFR entails running an iFFT on the raw CFR data.
 * This class should get information necessary from the WLAN driver
 * (such as bandwidth of the packets) in order to be able to perform
 * iFFT correctly.
 */
class LOWIRTTRangeProcessing
{
public:

  /*
   * Handles RTT response channel data
   *
   * Parameters:
   *   @param [in] uint32* Channel Response
   *   @param [in] int32 Length of channel response
   *   @param [in] int32 Response Type. Currently has to be 0
   *   @param [out] RTTInfo* RTT info
   *
   * @return int32  RTT Offset for channel response
   */
  int32 handle_rtt_response_data(const uint32 * const chan_rsp_ptr,
      const int32 len,
      const int32 rsp_type,
      LOWIRTTInfo * const rtt_info_ptr);

  /**
   * Constructor
   */
  LOWIRTTRangeProcessing ();

  /**
   * Destructor
   */
  ~LOWIRTTRangeProcessing ();
};

#endif //#ifndef __LOWI_RTT_RANGE_PROCESSING_H__
