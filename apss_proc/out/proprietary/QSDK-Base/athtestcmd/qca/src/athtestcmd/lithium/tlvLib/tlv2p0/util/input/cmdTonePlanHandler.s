/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#ifndef TLV2_MAX_USERS
#define TLV2_MAX_USERS 9
#endif

#ifndef TLV2_MAX_20MHZ_BANDWIDTHS
#define TLV2_MAX_20MHZ_BANDWIDTHS 8
#endif

#ifndef TLV2_MAX_BANDWIDTHS_USERS
#define TLV2_MAX_BANDWIDTHS_USERS 72
#endif

# cmd
CMD= TonePlan

# cmd parm
PARM_START:

UINT8:phyId:1:u:0
UINT8:bandwidth:1:u:0
UINT8:version:1:u:0
UINT8:pad1:1:u:0

UINT32:userEntries:1:u:0
UINT32:dataRates:TLV2_MAX_BANDWIDTHS_USERS:72:u
UINT32:powerdBm:TLV2_MAX_BANDWIDTHS_USERS:72:u
UINT32:fec:TLV2_MAX_BANDWIDTHS_USERS:72:u
UINT32:dcm:TLV2_MAX_BANDWIDTHS_USERS:72:u

PARM_END:
