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

#ifndef _SPI_DRIVER_H_
#define _SPI_DRIVER_H_

#ifdef _WIGIG_ARCH_SPI

#include "DriverAPI.h"

#include <sensor1.h>

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <condition_variable>

class SpiRequest;

// *************************************************************************************************

// SPI driver provides access to a 802.11ad device connected through the SPI bus.
// The access is provided through the Sensor1 library. SPI driver is aware of the Sensor1 service
// protocol. Specific request/response construction and handling is decoupled to classes derived
// from SpiRequest. Each such class implements a specific command and its response.

class SpiDriver final: public DriverAPI
{
public:

    explicit SpiDriver(const std::string& interfaceName);
    ~SpiDriver() override;

    bool Read(DWORD address, DWORD& value) override;
    bool ReadBlock(DWORD address, DWORD blockSize, std::vector<DWORD>& values) override;
    bool Write(DWORD address, DWORD value) override;
    bool WriteBlock(DWORD addr, std::vector<DWORD> values) override;

    bool Open() override;
    bool ReOpen() override;
    void Close() override;

    bool RegisterDriverControlEvents() override;
    bool DriverControl(uint32_t id, const void* inBuf, uint32_t inBufSize,
                       void* outBuf, uint32_t outBufSize, DWORD* pLastError) override;

    void OnServiceEvent(sensor1_msg_header_s *pMsgHdr, sensor1_msg_type_e msgType, void *pMsg);

private:

    std::string m_DeviceName;
    sensor1_handle_s *m_pServiceHandle;

    std::mutex m_Mutex;
    SpiRequest* m_pPendingSpiRequest;
    std::condition_variable m_BlockingRequestCondVar;

    const int RESPONSE_TIMEOUT_SEC = 10;

    bool ExecuteBlockingRequest(SpiRequest& spiRequest);
    void HandleServiceEvent(sensor1_msg_header_s *pMsgHdr, sensor1_msg_type_e msgType, void *pMsg);
    void HandleResponse(void *pMsg, uint32_t msgSize);
    void HandleIndication(uint32_t msgId, void *pMsg, uint32_t msgSize);
    void HandleWmiEvent(void *pMsg, uint32_t msgSize);
};


#endif   // _WIGIG_ARCH_SPI
#endif   // _SPI_DRIVER_H_
