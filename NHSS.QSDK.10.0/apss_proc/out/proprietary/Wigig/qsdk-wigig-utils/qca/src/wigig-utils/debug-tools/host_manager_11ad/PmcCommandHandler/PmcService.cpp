/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcService.h"

PmcDeviceContext& PmcService::GetPmcDeviceContext(const std::string& deviceName)
{
    auto deviceNameIter = m_pmcDeviceContextMap.find(deviceName);
    if (deviceNameIter == m_pmcDeviceContextMap.end())
    {
        std::unique_ptr<PmcDeviceContext> deviceContextPtr(new PmcDeviceContext());
        m_pmcDeviceContextMap.insert(std::make_pair(deviceName, std::move(deviceContextPtr)));
        return *(m_pmcDeviceContextMap[deviceName].get());
    }
    return *(deviceNameIter->second.get());
}
