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

#ifndef _SPI_UTILS_H_
#define _SPI_UTILS_H_

#ifdef _WIGIG_ARCH_SPI

#include "DebugLogger.h"

#include <sensor1.h>
#include <sns_qwg_v01.h>
#include <iosfwd>

// *************************************************************************************************

// WMI event data structure as arrives from FW. This structure is passed by the service as-is,
// without any change. This binary structure shall be parsed and converted to a format compatible
// with other architectures.

struct wil6210_mbox_hdr
{
    uint16_t seq;
    uint16_t len; /* payload, bytes after this header */
    uint16_t type;
    uint8_t flags;
    uint8_t reserved;
} __packed;

struct wmi_cmd_hdr {
    uint8_t mid;
    uint8_t reserved;
    uint16_t command_id;
    uint32_t fw_timestamp;
} __packed;

struct wmi_event {
    struct wil6210_mbox_hdr hdr;
    struct wmi_cmd_hdr wmi;
    uint8_t data[0];
} __packed;

std::ostream& operator<<(std::ostream& os, const wmi_cmd_hdr& wmiNativeMsgHdr);

// *************************************************************************************************

// Simulates driver event binary structure as obtained from the linux driver and expected by the
// WMI event consumer. WMI events should be converted to this format.
struct DriverControlHeader
{
    uint32_t CommandId;     // WMI command ID, offset 0
    uint16_t Context;       // unused, offset 4
    uint8_t DeviceId;       // MID, offset 6
    uint16_t DataLength;    // Data length in bytes, offset 7
    uint8_t Data[];         // Placeholder for data start, max length 2048
} __attribute__((__packed__));

// *************************************************************************************************

// Sanity check for a message size to be compatible with the expected message type
template<typename T>
const T* GetTypedMessage(const void* pMsg, uint32_t msgSize, uint32_t margin = 0)
{
    if (msgSize < (sizeof(T) - margin))
    {
        LOG_ERROR << "Insufficient response size for #T."
                  << " Minimal Expected: " << (sizeof(T) - margin)
                  << " Actual: " << msgSize
                  << std::endl;
        return nullptr;
    }

    return reinterpret_cast<const T *>(pMsg);
}

// *************************************************************************************************

class SnsQwgRequestType
{
    friend std::ostream& operator<<(std::ostream& os, const SnsQwgRequestType& reqType);
public:
    explicit SnsQwgRequestType(uint32_t msgType): m_TypeCode(msgType) {}
private:
    const uint32_t m_TypeCode;
};

// *************************************************************************************************

class SnsQwgResponseType
{
    friend std::ostream& operator<<(std::ostream& os, const SnsQwgResponseType& respType);
public:
    explicit SnsQwgResponseType(uint32_t msgType): m_TypeCode(msgType) {}
private:
    const uint32_t m_TypeCode;
};

// *************************************************************************************************

// Output operators for service types

std::ostream& operator<<(std::ostream& os, const sensor1_error_e& errorCode);
std::ostream& operator<<(std::ostream& os, const sensor1_msg_header_s& msgHdr);
std::ostream& operator<<(std::ostream& os, const sns_common_resp_s_v01& respHdr);

std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_reg_read_req_msg_v01& readReq);
std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_reg_read_resp_msg_v01& readResp);

std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_mem_read_req_msg_v01& readBlockReq);
std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_mem_read_resp_msg_v01& readBlockResp);

std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_reg_write_req_msg_v01& writeReq);
std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_reg_write_resp_msg_v01& writeResp);

std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_mem_write_req_msg_v01& writeBlockReq);
std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_mem_write_resp_msg_v01& writeBlockResp);

std::ostream& operator<<(std::ostream& os, const sns_common_version_resp_msg_v01& verResp);

std::ostream& operator<<(std::ostream& os, const sns_qwg_wmi_send_req_msg_v01& wmiReq);
std::ostream& operator<<(std::ostream& os, const sns_qwg_wmi_send_resp_msg_v01& wmiResp);

std::ostream& operator<<(std::ostream& os, const sns_qwg_wmi_event_ind_msg_v01& wmiEvent);

std::ostream& operator<<(std::ostream& os, const sns_qwg_fw_mbox_ready_ind_msg_v01& wmiEvent);


#endif   // _WIGIG_ARCH_SPI
#endif   // _SPI_UTILS_H_
