/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _SENSING_EVENT_DEFINITIONS_H_
#define _SENSING_EVENT_DEFINITIONS_H_

#include "EventsDefinitions.h"

// forward declaration to prevent unnecessary include
namespace Json
{
    class Value;
}

// Event to be sent upon reception of sensing state change notification from WiGig Sensing QMI Lib
class SensingStateChangedEvent final : public HostManagerEventBase
{
public:
    explicit SensingStateChangedEvent(std::string state) :
        HostManagerEventBase(""),
        m_sensingState(std::move(state))
    {}

private:
    std::string m_sensingState;

    const char* GetTypeName() const override { return "SensingStateChangedEvent"; }

    void ToJsonInternal(Json::Value& root) const override;
};

#endif // _SENSING_EVENT_DEFINITIONS_H_
