/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "HostManagerDefinitions.h"
#include <ostream>

std::ostream& operator<<(std::ostream& os, const BasebandTypeEnum& basebandType)
{
    switch(basebandType)
    {
    case BASEBAND_TYPE_NONE:   return os << "BASEBAND_TYPE_NONE";
    case BASEBAND_TYPE_SPARROW: return os << "BASEBAND_TYPE_SPARROW";
    case BASEBAND_TYPE_MARLON:  return os << "BASEBAND_TYPE_MARLON";
    case BASEBAND_TYPE_TALYN:   return os << "BASEBAND_TYPE_TALYN";
    default:                    return os << "<unknown>";
    }
}
