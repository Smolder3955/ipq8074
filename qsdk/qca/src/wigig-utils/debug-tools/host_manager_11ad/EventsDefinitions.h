/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 *       copyright notice, this list of conditions and the following
 *       disclaimer in the documentation and/or other materials provided
 *       with the distribution.
 *     * Neither the name of The Linux Foundation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _11AD_EVENTS_DEFINITIONS_H_
#define _11AD_EVENTS_DEFINITIONS_H_

#include <string>
#include <set>
#include "HostDefinitions.h"

// forward declarations
namespace Json
{
    class Value;
}

//=== Interface for composite types to be used as part of event payload
class JsonSerializable
{
public:
    // Json::Value is lack of move assignment operator, copy assignment operator performs deep copy
    // We cannot build on return value optimization, so the root and the key are passed as arguments.
    // In presence of move assignment operator it can be changed to 'virtual Json::Value ToJson() const = 0;'
    // with corresponding call 'root["keyName"] = m_memberName.ToJson();'

    // Serialization entry point for the general case. Will call internally the virtual ToJsonImpl().
    void ToJson(Json::Value& root, const char* keyName) const;

    // Update the passed Value with serialization of internal structure
    virtual void ToJsonImpl(Json::Value& valueToUpdate) const = 0;

    virtual ~JsonSerializable() {}
};

//=== Base class for all events to be pushed to DMTools
class HostManagerEventBase
{
public:
    explicit HostManagerEventBase(const std::string& deviceName);

    virtual ~HostManagerEventBase() {}

    // Serialization entry point
    std::string ToJson() const;

private:
    std::string m_timestampLocal; // creation time
    std::string m_deviceName;

    // enforce descendants to implement it and so make the class abstract
    virtual const char* GetTypeName(void) const = 0; // contract, shall match the exact class name defined in DmTools
    virtual void ToJsonInternal(Json::Value& root) const = 0;
};

/**************************************** KeepAliveEvent **********************************************************/
class KeepAliveEvent final : public HostManagerEventBase
{
public:
    KeepAliveEvent() : HostManagerEventBase("") {}

private:
    const char* GetTypeName() const override { return "KeepAliveEvent"; }

    void ToJsonInternal(Json::Value& /*root*/) const override {} // all required info is part of base class serialization
};

/*********************************** NewDeviceDiscoveredEvent *****************************************************/
// FW version struct with serialization support
class SerializableFwVersion final : public FwVersion, public JsonSerializable
{
public:
    explicit SerializableFwVersion(const FwVersion& rhs) : FwVersion(rhs) {}

    void ToJsonImpl(Json::Value& valueToUpdate) const override;
};

// FW compilation timestamp with serialization support
class SerializableFwTimestamp final : public FwTimestamp, public JsonSerializable
{
public:
    explicit SerializableFwTimestamp(const FwTimestamp& rhs) : FwTimestamp(rhs) {}

    void ToJsonImpl(Json::Value& valueToUpdate) const override;
};

// Event to be sent upon new device discovery or device FW change
class NewDeviceDiscoveredEvent final : public HostManagerEventBase
{
public:
    NewDeviceDiscoveredEvent(const std::string& deviceName, const FwVersion& fwVersion, const FwTimestamp& fwTimestamp);

private:
    SerializableFwVersion m_fwVersion;
    SerializableFwTimestamp m_fwTimestamp;

    const char* GetTypeName() const override { return "NewDeviceDiscoveredEvent"; }

    void ToJsonInternal(Json::Value& root) const override;
};

/*********************************** ClientConnectedEvent *****************************************************/
class ClientConnectedEvent final : public HostManagerEventBase
{
public:
    /*
    newClientIPSet - a set of already connected users
    newClient - the client that was connected or disconnected
    */
    ClientConnectedEvent(const std::set<std::string> & newClientIPSet, const std::string & newClient);

private:
    const std::set<std::string> m_ExistingClientIPSet;
    const std::string m_newClient;

    const char* GetTypeName() const override { return "ClientConnectedEvent"; }

    void ToJsonInternal(Json::Value& root) const override;
};

class ClientDisconnectedEvent final : public HostManagerEventBase
{
public:
    /*
    newClientIPSet - a set of already connected users
    newClient - the client that was disconnected
    */
    ClientDisconnectedEvent(const std::set<std::string> & newClientIPSet, const std::string & newClient);

private:
    const std::set<std::string> m_ExistingClientIPSet;
    const std::string m_newClient;

    const char* GetTypeName() const override { return "ClientDisconnectedEvent"; }

    void ToJsonInternal(Json::Value& root) const override;
};

/*********************************** Silence Status Changed *****************************************************/
class SilenceStatusChangedEvent final : public HostManagerEventBase
{
public:
    SilenceStatusChangedEvent(const std::string& deviceName, bool isSilent);

private:
    bool m_isSilent;

    const char* GetTypeName() const override { return "SilenceStatusChangedEvent"; }

    void ToJsonInternal(Json::Value& root) const override;
};

/************************************FW uCode status changed *****************************************************/
class FwUcodeStatusChangedEvent final : public HostManagerEventBase
{
public:
    FwUcodeStatusChangedEvent(const std::string& deviceName, FwUcodeState fwUcodeState);

private:
    FwUcodeState m_fwUcodeState;

    const char* GetTypeName() const override { return "FwUcodeStatusChangedEvent"; }

    void ToJsonInternal(Json::Value& root) const override;
};

//********************************** DriverEvent **********************************************//
// Event to be sent upon reception of driver event
class DriverEvent final : public HostManagerEventBase
{
public:
    DriverEvent(const std::string& deviceName, int driverEventType,
                unsigned driverEventId, unsigned listenId,
                unsigned bufferLength, const unsigned char* binaryData);

private:
    const int m_driverEventType;
    const unsigned m_driverEventId;
    const unsigned m_listenId;
    const unsigned m_bufferLength;
    const unsigned char* m_binaryData;

    const char* GetTypeName() const override { return "DriverEvent"; }

    void ToJsonInternal(Json::Value& root) const override;
};

// =================================================================================== //

#endif // _11AD_EVENTS_DEFINITIONS_H_
