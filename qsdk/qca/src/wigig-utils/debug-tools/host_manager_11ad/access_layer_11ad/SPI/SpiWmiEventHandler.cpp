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

#include "SpiWmiEventHandler.h"
#include "Host.h"
#include "SpiUtils.h"
#include "DebugLogger.h"

#include <sns_qwg_v01.h>

// *************************************************************************************************

void SpiWmiEventHandler::HandleWmiEvent(const char* pInterfaceName, void *pMsg, uint32_t msgSize)
{
        const sns_qwg_wmi_event_ind_msg_v01* pInd =
        GetTypedMessage<sns_qwg_wmi_event_ind_msg_v01>(pMsg, msgSize, SNS_QWG_WMI_EVENT_MAX_SIZE_V01);
    if (nullptr == pInd)
    {
        LOG_ERROR << "No SPI WMI event provided in a service event" << std::endl;
        return;
    }

    LOG_DEBUG << "Got SPI WMI event: " << *pInd << std::endl;

    uint32_t headerSize = sizeof(struct wil6210_mbox_hdr) + sizeof(struct wmi_cmd_hdr);
    if (pInd->data_len < headerSize)
    {
        LOG_ERROR << "WMI Event is too short"
                  << " Minimal Length: " << headerSize
                  << " Actual Length: " << pInd->data_len
                  << std::endl;
        return;
    }

    // Raw data as received from the service - requires protocol translation

    const wmi_event* pRawWmiEvent = reinterpret_cast<const wmi_event*>(pInd->data);
    const wmi_cmd_hdr* pRawWmiHdr = &pRawWmiEvent->wmi;
    const uint8_t* pRawWmiData = pRawWmiEvent->data;
    uint32_t wmiDataLength = pInd->data_len - headerSize;

    LOG_VERBOSE << "WMI Event Sanity."
                << " Buffer: " << msgSize
                << " Message:" << pInd->data_len
                << " MB Header: " << sizeof(wil6210_mbox_hdr)
                << " WMI Header: " << sizeof(struct wmi_cmd_hdr)
                << " WMI Data: " << wmiDataLength
                << " Length in MB Header: " << reinterpret_cast<const wil6210_mbox_hdr *>(pInd->data)->len
                << " MB Sequence: " << reinterpret_cast<const wil6210_mbox_hdr *>(pInd->data)->seq
                << std::endl;

    LOG_DEBUG << "Got WMI Event: " << *pRawWmiHdr << std::endl;
    LOG_VERBOSE << "WMI Event data payload: "
                << MemoryDump(reinterpret_cast<const unsigned char *>(pRawWmiData), wmiDataLength) << std::endl;

    // Translate SPI WMI Event to Driver Control Event (additional memory copy needed)

    uint32_t driverEventLength = sizeof(DriverControlHeader) + wmiDataLength;
    std::vector<uint8_t> driverEventPayload(driverEventLength);

    DriverControlHeader *pDriverControlHdr = reinterpret_cast<DriverControlHeader *>(driverEventPayload.data());
    pDriverControlHdr->CommandId = pRawWmiHdr->command_id;
    pDriverControlHdr->Context = 0;
    pDriverControlHdr->DeviceId = pRawWmiHdr->mid;
    pDriverControlHdr->DataLength = wmiDataLength;

    uint8_t *pDstWmiData = driverEventPayload.data() + sizeof(DriverControlHeader);
    memcpy(pDstWmiData, pRawWmiData, wmiDataLength);

    Host::GetHost().PushEvent(DriverEvent(pInterfaceName, 2, 0, 0, driverEventLength, driverEventPayload.data()));
}

#endif   // _WIGIG_ARCH_SPI
