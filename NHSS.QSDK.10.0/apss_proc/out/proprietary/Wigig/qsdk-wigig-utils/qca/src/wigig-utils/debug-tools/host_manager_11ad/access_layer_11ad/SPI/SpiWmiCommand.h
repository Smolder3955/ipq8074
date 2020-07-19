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

#ifndef _SPI_WMI_COMMAND_H_
#define _SPI_WMI_COMMAND_H_

#ifdef _WIGIG_ARCH_SPI

#include "SpiRequest.h"

#include <sns_qwg_v01.h>
#include <iosfwd>

// *************************************************************************************************

class SpiWmiCommand final: public SpiRequest
{
public:

    SpiWmiCommand(const void* inBuf, uint32_t inBufSize);

    const uint8_t* GetRequestBuffer() const override { return reinterpret_cast<const uint8_t *>(&m_Request); }
    uint32_t GetRequestLength() const override { return sizeof (m_Request); }

private:

    sns_qwg_wmi_send_req_msg_v01 m_Request;

    bool IsValidRequest() const override;
    uint32_t GetMessageId() const override { return SNS_QWG_WMI_SEND_REQ_V01; }
    bool ImportResponseBuffer(const uint8_t* pResponseBuf, uint32_t responseSize) override;
    std::ostream& Print(std::ostream& os) const override;
};

#endif   // _WIGIG_ARCH_SPI
#endif   // _SPI_WMI_COMMAND_H_
