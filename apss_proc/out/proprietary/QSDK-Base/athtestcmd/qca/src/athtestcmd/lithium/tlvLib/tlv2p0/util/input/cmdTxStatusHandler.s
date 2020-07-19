/*
 * Copyright (c) 2017 Qualcomm Technologies, Inc.
 *
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 */

#if !defined(MCHAIN_UTF)
#define MCHAIN_UTF           16
#endif //!defined(MCHAIN_UTF)

# cmd
CMD= TXStatus

# cmd parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:stopTx:1:u:1
UINT8:needReport:1:u:1
PARM_END:

# rsp
RSP= TXStatusRsp

# rsp parm
PARM_START:
UINT8:phyId:1:u:0
UINT8:status:1:u:0
UINT8:numOfReports:1:u:0
UINT8:paCfg:1:u:0

UINT8:gainIdx:1:u:15
INT8:dacGain:1:d:0
UINT8:pad2:2:u:

UINT8:pdadc:MCHAIN_UTF:16:u
INT16:thermCal:MCHAIN_UTF:16:d

UINT32:totalPackets:1:u:0
UINT32:goodPackets:1:u:0
UINT32:underruns:1:u:0
UINT32:otherError:1:u:0
UINT32:excessRetries:1:u:0
UINT32:shortRetry:1:u:0
UINT32:longRetry:1:u:0
UINT32:startTime:1:u:0
UINT32:endTime:1:u:0
UINT32:byteCount:1:u:0
UINT32:dontCount:1:u:0
INT32:rssi:1:d:0
INT32:rssic:MCHAIN_UTF:16:d
INT32:rssie:MCHAIN_UTF:16:d
UINT16:rateBitIndex:1:u:0
PARM_END:

