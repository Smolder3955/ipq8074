/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef PMC_SERVICE_H_
#define PMC_SERVICE_H_

#include <map>
#include <memory>
#include "PmcRecordingState.h"
#include "PmcDeviceConfigurations.h"
#include "PmcRegistersAccessor.h"

//This class saves PMC context for a single device - its recording state (if a recording is active or not) and the PMC configurations
class PmcDeviceContext
{
    public:

        PmcDeviceContext(PmcDeviceContext const&) = delete;
        PmcDeviceContext& operator=(PmcDeviceContext const&) = delete;

        PmcDeviceContext()
        {

        }

        PmcRecordingState& GetDeviceRecordingState()
        {
            return m_recordingState;
        }

        PmcDeviceConfigurations& GetDeviceConfigurations()
        {
            return m_deviceConfigurations;
        }

    private:
        PmcRecordingState m_recordingState;
        PmcDeviceConfigurations m_deviceConfigurations;
};

//This class provides access for a context for each device used for the PMC
class PmcService
{
    public:
        static PmcService& GetInstance()
        {
            static PmcService instance;
            return instance;
        }

        PmcService(PmcService const&) = delete;
        PmcService& operator=(PmcService const&) = delete;

        const PmcRegistersAccessor& GetPmcRegistersAccessor() const
        {
            return m_registerAccessor;
        }

        PmcDeviceContext& GetPmcDeviceContext(const std::string& deviceName);

    private:
        //register name => {device name, registerInfo}
        std::map<std::string, std::unique_ptr<PmcDeviceContext>> m_pmcDeviceContextMap;
        const PmcRegistersAccessor m_registerAccessor;
        PmcService() : m_registerAccessor(PmcRegistersAccessor())
        {
        }

};

#endif /* PMC_SERVICE_H_ */
