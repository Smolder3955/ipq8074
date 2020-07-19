/*
* Copyright (c) 2017 Qualcomm Technologies, Inc.
*
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _TLV2_INC_H_
#define _TLV2_INC_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#if !defined(_HOST_SIM_TESTING)
#include "osapi.h"
#endif
#include "wlantype.h"
#include "tsCommon.h"
#include "cmdRspParmsInternal.h"
#include "cmdRspParmsDict.h"
#include "tlvCmdEncoder.h"
#include "tlv2Api.h"
#include "tlv2SysApi.h"

// #define UTF_INFOH_ENALBE
// #define UTF_INFOL_ENALBE

#ifdef UTF_INFOH_ENALBE
#define UTF_INFOH(format, ...) wdiag_msg(WLAN_MODULE_WAL, DBGLOG_INFO, format, ##__VA_ARGS__)
#else
#define UTF_INFOH(format, ...)
#endif

#ifdef UTF_INFOL_ENALBE
#define UTF_INFOL(format, ...) wdiag_msg(WLAN_MODULE_WAL, DBGLOG_VERBOSE, format, ##__VA_ARGS__)
#else
#define UTF_INFOL(format, ...)
#endif


#endif //_TLV2_INC_H_/
