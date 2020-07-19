/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _11AD_ADDRESS_TRANSLATOR_H_
#define _11AD_ADDRESS_TRANSLATOR_H_

#include "HostManagerDefinitions.h"

class AddressTranslator
{
public:
    static bool ToAhbAddress(uint32_t srcAddr, uint32_t& dstAddr, BasebandType basebandType);
};

#endif   //  _11AD_ADDRESS_TRANSLATOR_H_
