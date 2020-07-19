/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "SensingEvents.h"
#include <json/json.h>

void SensingStateChangedEvent::ToJsonInternal(Json::Value& root) const
{
    root["SensingState"] = m_sensingState;
}
