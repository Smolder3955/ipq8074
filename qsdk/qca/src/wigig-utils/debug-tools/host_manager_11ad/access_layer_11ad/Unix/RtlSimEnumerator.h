/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _RTL_SIM_ENUMERATOR_H_
#define _RTL_SIM_ENUMERATOR_H_

#include <string>
#include <set>

class RtlSimEnumerator
{
public:

    static std::string GetRtlDirectory();
    static std::set<std::string> Enumerate();

private:

    static std::set<std::string> GetRtlInterfaceNames();
};

#endif   // _RTL_SIM_ENUMERATOR_H_
