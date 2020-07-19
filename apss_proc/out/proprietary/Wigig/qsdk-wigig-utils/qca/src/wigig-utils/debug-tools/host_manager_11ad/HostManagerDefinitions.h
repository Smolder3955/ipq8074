/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _HOST_MANAGER_DEFINITIONS_H
#define _HOST_MANAGER_DEFINITIONS_H
#pragma once

#include <string>
#include <map>

typedef enum BasebandTypeEnum
{
    BASEBAND_TYPE_NONE,
    BASEBAND_TYPE_SPARROW,   // added here to keep backward compatibility. some tools assume MARLON == 2, we don't brake this assumption
    BASEBAND_TYPE_MARLON,
    BASEBAND_TYPE_TALYN,
    BASEBAND_TYPE_LAST
} BasebandType;

typedef enum BasebandRevisionEnum
{
    // In this enum the order does matter. newer revisions should be last!
    BB_REV_UNKNOWN = 0,
    MAR_DB1 = 1,
    MAR_DB2 = 2,
    SPR_A0 = 3,
    SPR_A1 = 4,
    SPR_B0 = 5,
    SPR_C0 = 6,
    SPR_D0 = 7,
    TLN_M_A0 = 8,
    TLN_M_B0 = 9,
    TLN_M_C0 = 10,
    MAX_BB_REVISION = 11
} BasebandRevision;

#endif // !_HOST_MANAGER_DEFINITIONS_H
