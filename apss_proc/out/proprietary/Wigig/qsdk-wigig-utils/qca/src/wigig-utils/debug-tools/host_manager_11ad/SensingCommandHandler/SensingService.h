/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _SENSING_SERVICE_H_
#define _SENSING_SERVICE_H_

#include <vector>
#include <string>
#include "OperationStatus.h"

// forward declaration
struct wigig_sensing_lib_client_struct;

// Singleton Service providing APIs to operate Sensing control and data flows through the WiGig Sensing QMI Library
class SensingService
{
public:
    // Getter for the Sensing Service Singleton
    static SensingService& GetInstance();
    ~SensingService();

    OperationStatus OpenSensing(bool autoRecovery, bool sendNotification);
    OperationStatus CloseSensing();

    OperationStatus GetParameters(std::vector<unsigned char>& parameters) const;
    OperationStatus ChangeMode(const std::string& modeStr, uint32_t preferredChannel, uint32_t& burstSizeBytes) const;
    OperationStatus GetMode(std::string& modeStr) const;
    OperationStatus ReadData(uint32_t maxSizeBytes, std::vector<unsigned char>& sensingData, std::vector<uint64_t>& driTsfVec,
                             uint32_t& dropCntFromLastRead, uint32_t& numRemainingBursts) const;
    OperationStatus GetNumRemainingBursts(uint32_t& numRemainingBursts) const;

    void OnSensingStateChanged(wigig_sensing_lib_client_struct* sensingHandle, const std::string& stateStr) const;

    SensingService(const SensingService&) = delete;
    SensingService& operator=(const SensingService&) = delete;

private:
    SensingService();
    bool IsRegistered() const;
    OperationStatus ValidateAvailability(const std::string& requestStr) const;

    wigig_sensing_lib_client_struct* m_sensingHandle; // registration handle assigned by the WiGig Sensing QMI Library
};

#endif // _SENSING_SERVICE_H_
