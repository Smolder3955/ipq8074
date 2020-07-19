/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "Utils.h"
#include <algorithm>

std::string Utils::ToLower(const std::string& st)
{
    std::string lowerSt(st);
    transform(st.begin(), st.end(), lowerSt.begin(), ::tolower);
    return lowerSt;
}
