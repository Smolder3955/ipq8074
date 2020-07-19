/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _SHARED_VERSION_INFO_H_
#define _SHARED_VERSION_INFO_H_

#include <string>
#include <sstream>
#include <string.h>

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

// The only place for version definition/editing
#define TOOL_VERSION_MAJOR  1
#define TOOL_VERSION_MINOR  5
#define TOOL_VERSION_MAINT  0
#define TOOL_VERSION_INTERM 11

#define TOOL_VERSION            TOOL_VERSION_MAJOR,TOOL_VERSION_MINOR,TOOL_VERSION_MAINT,TOOL_VERSION_INTERM
#define TOOL_VERSION_STR        STR(TOOL_VERSION_MAJOR.TOOL_VERSION_MINOR.TOOL_VERSION_MAINT.TOOL_VERSION_INTERM)

inline std::string GetToolsVersion()
{
    return TOOL_VERSION_STR;
}

inline std::string GetToolsBuildInfo()
{
    std::stringstream ss;

    ss << "802.11ad Tools Build Info" << "\n"
       << "Version:          " << TOOL_VERSION_STR << "\n";
#ifdef BUILD_ID
    if (strlen(BUILD_ID) > 0)
    {
        ss << "Build ID:         " << BUILD_ID << "\n";
    }
#endif
#ifdef BUILD_DATE
    ss << "Build Date:       " << BUILD_DATE << "\n";
#endif
#ifdef LAST_CHANGE_ID
    if (strlen(LAST_CHANGE_ID) > 0)
    {
        ss << "Last Change ID:   " << LAST_CHANGE_ID << "\n";
    }
#endif
#ifdef LAST_CHANGE_DATE
    if (strlen(LAST_CHANGE_DATE) > 0)
    {
        ss << "Last Change Date: " << LAST_CHANGE_DATE;
    }
#endif

    return ss.str();
}

#endif // _SHARED_VERSION_INFO_H_
