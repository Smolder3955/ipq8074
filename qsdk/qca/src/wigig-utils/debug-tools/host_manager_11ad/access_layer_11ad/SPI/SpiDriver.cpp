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

#ifdef _WIGIG_ARCH_SPI

#include "SpiDriver.h"
#include "SpiUtils.h"
#include "SpiRequest.h"
#include "SpiRead.h"
#include "SpiReadBlock.h"
#include "SpiWrite.h"
#include "SpiWriteBlock.h"
#include "SpiVersionRequest.h"
#include "SpiWmiCommand.h"
#include "SpiWmiEventHandler.h"

#include <sensor1.h>
#include <sns_qwg_v01.h>

#include <string.h>
#include <sstream>

// *************************************************************************************************

namespace
{
    // Asyncronous callback registered with the Sensor1 service to handle service events.
    // This is a global function that is called from a service working thread.
    // Data races should be considered!

    void Sensor1ServiceCallback(
        intptr_t pContext, sensor1_msg_header_s *pMsgHdr, sensor1_msg_type_e msgType, void *pMsg)
    {
        if (0 == pContext)
        {
            LOG_ERROR << "SPI Sevice Callback: no context provided" << std::endl;
            return;
        }

        SpiDriver* pDriver = reinterpret_cast<SpiDriver *>(pContext);
        pDriver->OnServiceEvent(pMsgHdr, msgType, pMsg);
    }
}

// *************************************************************************************************

SpiDriver::SpiDriver(const std::string& interfaceName)
    : DriverAPI(interfaceName)
    , m_pServiceHandle(nullptr)
    , m_pPendingSpiRequest(nullptr)
{
    std::stringstream ss;
    ss << Utils::SPI << DEVICE_NAME_DELIMITER << interfaceName;
    m_DeviceName = ss.str();

    LOG_DEBUG << "Created SPI driver."
              << " Device Name: " << m_DeviceName
              << " Supported block size: " << SNS_QWG_DATA_BLOCK_MAX_SIZE_V01 << " B"
              << std::endl;
}

// *************************************************************************************************

SpiDriver::~SpiDriver()
{
    Close();
}

// *************************************************************************************************

bool SpiDriver::Open()
{
    LOG_DEBUG << "Opening an SPI driver" << std::endl;

    sensor1_error_e error = sensor1_init();
    if (SENSOR1_SUCCESS != error)
    {
        LOG_ERROR << "Cannot open SPI driver: sensor1_init returned " << error << std::endl;
        return false;
    }

    error = sensor1_open(&m_pServiceHandle, Sensor1ServiceCallback, reinterpret_cast<intptr_t>(this));
    if (SENSOR1_SUCCESS != error)
    {
        LOG_ERROR << "Cannot open SPI driver: sensor1_open returned " <<  error << std::endl;
        return false;
    }

    // Version request triggers indication notifications for the service channel.
    // Without this command indications will be omitted.
    SpiVersionRequest spiVerRequest;
    if (!ExecuteBlockingRequest(spiVerRequest))
    {
        LOG_ERROR << "Cannot send SPI version request" << std::endl;
        return false;
    }

    return true;
}

// *************************************************************************************************

bool SpiDriver::ReOpen()
{
    LOG_DEBUG << "Trying to re-open an SPI driver" << std::endl;

    Close();
    return Open();
}

// *************************************************************************************************

void SpiDriver::Close()
{
    LOG_DEBUG << "Closing an SPI driver" << std::endl;

    if (!m_pServiceHandle)
    {
        LOG_DEBUG << " Skipping SPI driver close - no service handle exist" << std::endl;
        return;
    }

    sensor1_error_e error = sensor1_close(m_pServiceHandle);
    if (SENSOR1_SUCCESS != error)
    {
        LOG_ERROR << "Cannot close SPI driver: sensor1_close returned " <<  error << std::endl;
    }

    m_pServiceHandle = nullptr;   // Make sure to close only once.
}

// *************************************************************************************************

bool SpiDriver::Read(DWORD address, DWORD& value)
{
    SpiRead spiRead(address, value);
    if (!ExecuteBlockingRequest(spiRead))
    {
        LOG_ERROR << "Cannot send SPI Read request" << std::endl;
        return false;
    }

    return spiRead.IsCompletedSuccessfully();
}

// *************************************************************************************************

bool SpiDriver::ReadBlock(DWORD address, DWORD blockSize, std::vector<DWORD>& values)
{
    SpiReadBlock spiReadBlock(address, blockSize, values);
    if (!ExecuteBlockingRequest(spiReadBlock))
    {
        LOG_ERROR << "Cannot send SPI ReadBlock request" << std::endl;
        return false;
    }

    return spiReadBlock.IsCompletedSuccessfully();
}

// *************************************************************************************************

bool SpiDriver::Write(DWORD address, DWORD value)
{
    SpiWrite spiWrite(address, value);
    if (!ExecuteBlockingRequest(spiWrite))
    {
        LOG_ERROR << "Cannot send SPI Write request" << std::endl;
        return false;
    }

    return spiWrite.IsCompletedSuccessfully();
}

// *************************************************************************************************

bool SpiDriver::WriteBlock(DWORD addr, std::vector<DWORD> values)
{
    SpiWriteBlock spiWriteBlock(addr, values);
    if (!ExecuteBlockingRequest(spiWriteBlock))
    {
        LOG_ERROR << "Cannot send SPI WriteBlock request" << std::endl;
        return false;
    }

    return spiWriteBlock.IsCompletedSuccessfully();

}

// *************************************************************************************************

bool SpiDriver::RegisterDriverControlEvents()
{
    return true;
}

// *************************************************************************************************

bool SpiDriver::DriverControl(uint32_t id, const void* inBuf, uint32_t inBufSize,
                              void* outBuf, uint32_t outBufSize, DWORD* pLastError)
{
    (void)outBuf;
    (void)outBufSize;
    (void)pLastError;

    LOG_DEBUG << "Sending SPI driver command: " << id << std::endl;
    if (id != 0)
    {
        // SPI driver simulates driver control by re-composing a driver event according to the
        // Sensor1 service contract. Only WMI driver control is supported for this purpose.
        LOG_ERROR << "Unsupported command: " << id << std::endl;
        return false;
    }

    SpiWmiCommand spiWmiCommand(inBuf, inBufSize);
    if (!ExecuteBlockingRequest(spiWmiCommand))
    {
        LOG_ERROR << "Cannot send SPI WMI command request" << std::endl;
        return false;
    }

    return spiWmiCommand.IsCompletedSuccessfully();
}

// *************************************************************************************************

bool SpiDriver::ExecuteBlockingRequest(SpiRequest& spiRequest)
{
    std::unique_lock<std::mutex> scopedLock(m_Mutex);

    if (m_pPendingSpiRequest)
    {
        LOG_ERROR << "Rejecting SPI I/O request as there is another pending request."
                  << " Pending Request: " << *m_pPendingSpiRequest
                  << " Rejected Request: " << spiRequest
                  << std::endl;
        return false;
    }

    LOG_VERBOSE << "Starting SPI Request: " << spiRequest << std::endl;

    sensor1_msg_header_s msgHdr;
    if (!spiRequest.CreateRequestHeader(msgHdr))
    {
        LOG_ERROR << "Cannnot create SPI request header for: " << spiRequest << std::endl;
        return false;
    }

    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    void* pServiceBuffer = nullptr;
    sensor1_error_e error = sensor1_alloc_msg_buf(m_pServiceHandle, spiRequest.GetRequestLength(), &pServiceBuffer);
    if (SENSOR1_SUCCESS != error)
    {
        LOG_ERROR << "Cannot allocate sensor1 buffer."
                  << " Error: " << error
                  << " Message length: " << spiRequest.GetRequestLength()
                  << " Request: " << spiRequest
                  << std::endl;
        return false;
    }

    // Establish a context between the request start and completion point.
    m_pPendingSpiRequest = &spiRequest;

    // WARNING! Starting from this point any error flow requires service buffer deallocation and state reset.

    if (spiRequest.GetRequestBuffer() != nullptr && spiRequest.GetRequestLength() > 0)
    {
        // There are requests that do not require a buffer, so this operation is optional.
        memcpy(pServiceBuffer, spiRequest.GetRequestBuffer(), spiRequest.GetRequestLength());
    }

    LOG_VERBOSE << "Sending sensor1 message. Header: " << msgHdr << std::endl;
    LOG_VERBOSE << "Request buffer: "
                << MemoryDump(reinterpret_cast<unsigned char *>(pServiceBuffer), spiRequest.GetRequestLength())
                << std::endl;

    // Send the request

    // Service API is expected to release the provided buffer.
    error = sensor1_write(m_pServiceHandle, &msgHdr, pServiceBuffer);
    if (SENSOR1_SUCCESS != error)
    {
        LOG_ERROR << "Cannot send service request. sensor1_write returned "
                  << " Error: " << error
                  << " Msg Header: " << msgHdr
                  << " Request: " << spiRequest
                  << std::endl;

        //sensor1_free_msg_buf(m_pServiceHandle, pServiceBuffer);
        m_pPendingSpiRequest = nullptr;
        return false;
    }

    LOG_VERBOSE << "Waiting for SPI response" << std::endl;

    // Wait for the response

    bool bConditionMet = m_BlockingRequestCondVar.wait_for(
        scopedLock,
        std::chrono::seconds(RESPONSE_TIMEOUT_SEC),
        [this]{return (m_pPendingSpiRequest == nullptr);});

    if (!bConditionMet)
    {
        // Exit on timeout, the service is not responsive. Try to re-open as a best effort.
        // In any case, the service should be closed to prevent pending events from being
        // received in the current context.
        LOG_ERROR << "Blocking request timed out: " << spiRequest << std::endl;
        if (ReOpen())
        {
            // If re-open succeeded, a subsequent operation will hopefully succeed.
            // However, this flow is reported as error as current operation has been failed.
            LOG_ERROR << "The service has been re-opened after request timeout" << std::endl;
        }
        else
        {
            // Next enumeration will delete the device from the repository.
            LOG_ERROR << "Cannot re-open the service after request timeout" << std::endl;
        }

        return false;
    }

    std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
    int executionTimeMsec = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    LOG_DEBUG << "Blocking SPI request completed: " << spiRequest
              << " Execution time: " << executionTimeMsec << " msec"
              << std::endl;

    return true;
}

// *************************************************************************************************

// Driver callback for service events.
// this function is decoupled from HandleServiceEvent() that does the real handling to make sure the
// service buffer is released in any flow.

void SpiDriver::OnServiceEvent(sensor1_msg_header_s *pMsgHdr, sensor1_msg_type_e msgType, void *pMsg)
{
    HandleServiceEvent(pMsgHdr, msgType, pMsg);

    if (nullptr == m_pServiceHandle)
    {
        LOG_ERROR << "Cannot return event buffer to the service - no handle available" << std::endl;
        return;
    }

    if(nullptr != pMsg)
    {
        sensor1_free_msg_buf(m_pServiceHandle, pMsg );
    }
}

// *************************************************************************************************

void SpiDriver::HandleServiceEvent(sensor1_msg_header_s *pMsgHdr, sensor1_msg_type_e msgType, void *pMsg)
{
    if (nullptr == pMsgHdr)
    {
        LOG_DEBUG << "Callback: No message header provided" << std::endl;
        return;
    }

    LOG_VERBOSE << "Received Message from service: " << *pMsgHdr << std::endl;

    if (pMsgHdr->service_number != SNS_QWG_SVC_ID_V01)
    {
        LOG_ERROR << "Unknown service: " << pMsgHdr->service_number << std::endl;
        return;
    }

    if (nullptr == pMsg)
    {
        // TODO: Should we return here?
        LOG_ERROR << "No message received from service" << std::endl;
        return;
    }

    // The below dump produces very long output event for verbose debug.
    // It should only be un-commented for dedicated debug cases.
    // LOG_VERBOSE << "Received buffer: "
    //             << MemoryDump(reinterpret_cast<unsigned char *>(pMsg), pMsgHdr->msg_size) << std::endl;

    switch (msgType)
    {
    case SENSOR1_MSG_TYPE_RESP:
        HandleResponse(pMsg, pMsgHdr->msg_size);
        break;

    case SENSOR1_MSG_TYPE_IND:
        HandleIndication(pMsgHdr->msg_id, pMsg, pMsgHdr->msg_size);
        break;

    default:
        LOG_ERROR << "Got unexpected message type: " << msgType << std::endl;
        break;
    }
}

// *************************************************************************************************

void SpiDriver::HandleResponse(void *pMsg, uint32_t msgSize)
{
    std::lock_guard<std::mutex> scopedLock(m_Mutex);
    if (nullptr == m_pPendingSpiRequest)
    {
        LOG_ERROR << "Unexpected SPI response - no pending request found" << std::endl;
        return;
    }

    m_pPendingSpiRequest->Complete(reinterpret_cast<uint8_t *>(pMsg), msgSize);
    m_pPendingSpiRequest = nullptr;

    m_BlockingRequestCondVar.notify_one();
}

// *************************************************************************************************

void SpiDriver::HandleIndication(uint32_t msgId, void *pMsg, uint32_t msgSize)
{
    switch(msgId)
    {
    case SNS_QWG_WMI_EVENT_IND_V01:
        SpiWmiEventHandler::HandleWmiEvent(m_DeviceName.c_str(), pMsg, msgSize);
        break;

    case SNS_QWG_FW_MBOX_READY_IND_V01:
        LOG_DEBUG << "FW Mailbox Ready indication is deprecated - ignored" << std::endl;
        break;

    default:
        LOG_ERROR << "Unsupported SPI indication msg id: " << msgId << std::endl;
    }
}


#endif   // _WIGIG_ARCH_SPI
