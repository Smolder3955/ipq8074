#ifndef __LOWI_CONST_H__
#define __LOWI_CONST_H__

/*====*====*====*====*====*====*====*====*====*====*====*====*====*====*====*

        LOWI Const Interface Header file

GENERAL DESCRIPTION
  This file contains the structure definitions and function prototypes for
  LOWIConst

Copyright (c) 2012 Qualcomm Atheros, Inc.
  All Rights Reserved.
  Qualcomm Atheros Confidential and Proprietary.

Copyright (c) 2016 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Confidential and Proprietary - Qualcomm Technologies, Inc.
=============================================================================*/
#include <ctype.h>
#include <inttypes.h>
#include <time.h>
#include <new>
#include <stdint.h>
#include <base_util/vector.h>
#include <inc/lowi_defines.h>

namespace qc_loc_fw
{

#define BSSID_LEN 6
#define SSID_LEN 32
#define MAX_CHAN_INFO_SIZE 16
#define CLIENT_NAME_LEN 128
#define CELL_POWER_NOT_FOUND 0x7F
#define LOWI_COUNTRY_CODE_LEN 2
#define CIVIC_INFO_LEN 256 // civic information size in bytes

const char* const LOWI_CLIENT_SUFFIX = "-LOWIClient";
const char* const SERVER_NAME = "LOWI-SERVER";
const char* const NOT_AVAILABLE = "NA";
const int ASYNC_DISCOVERY_REQ_VALIDITY_PERIOD_SEC_MAX = 3600;

// minimum measurement period (in msec) for a wifi node in an ranging periodic request
uint32 const LOWI_MIN_MEAS_PERIOD = 500;
// maximum number of measurement retries attempted by the lowi scheduler on a wifi node
uint8  const MAX_RETRIES_PER_MEAS = 3;
// ssid length: 32 bytes plus one byte for null char
uint32 const LOWI_SSID_LENGTH_WITH_NULL = SSID_LEN + 1;
// speed of light in centimeters per nanosecond
float const LOWI_LIGHT_SPEED_CM_PER_NSEC = 30.0;
// divide by two constant
float const LOWI_DIVIDE_BY_TWO = 2.0;
// tenths of nanoseconds constant
float const TENTHS_PER_NSEC = 10.0;
// includes all constants needed to yield distance in cm such that:
// distance(cm) = rtt * RTT_DIST_CONST;
// where rtt is in 10ths of nsec
float const RTT_DIST_CONST = (LOWI_LIGHT_SPEED_CM_PER_NSEC /
                              TENTHS_PER_NSEC /
                              LOWI_DIVIDE_BY_TWO);
// FTMRR Report Range units: 1/64 m in cms
float const FTMRR_RANGE_UNITS = 1.5625;
// WPA Supplicant's string command size
uint32 const LOWI_WPA_CMD_SIZE = 256;
// AP Geospatial Location ANQP Info ID
uint32 const LOWI_GEOSPATIAL_LOCATION_ANQP_INFO_ID = 265;
// AP Civic Location ANQP Info ID
uint32 const LOWI_CIVIC_LOCATION_ANQP_INFO_ID = 266;

} // namespace qc_loc_fw
#endif //#ifndef __LOWI_CONST_H__
