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

#include "SpiWrite.h"
#include "SpiUtils.h"
#include "DebugLogger.h"

#include <ostream>

// *************************************************************************************************

SpiWrite::SpiWrite(uint32_t address, uint32_t value)
{
    m_Request.addr = address;
    m_Request.value = value;

    LOG_VERBOSE << "Creating SPI Write Request: " << m_Request << std::endl;
}

// *************************************************************************************************

bool SpiWrite::ImportResponseBuffer(const uint8_t* pResponseBuf, uint32_t responseSize)
{
    const sns_qwg_chip_reg_write_resp_msg_v01* pResponse =
        GetTypedMessage<sns_qwg_chip_reg_write_resp_msg_v01>(pResponseBuf, responseSize);
    if (!pResponse) return false;

    LOG_VERBOSE << "Got Write Response: " << *pResponse << std::endl;

    if (pResponse->resp.sns_result_t != 0)
    {
        LOG_ERROR << "SPI Write response returned with error: " << pResponse->resp.sns_err_t << std::endl;
        return false;;
    }

    LOG_DEBUG << "SPI Write import completed" << std::endl;
    return true;
}

// *************************************************************************************************

std::ostream& SpiWrite::Print(std::ostream& os) const
{
    return os << "SPI Write."
              << " Address: " << Address(m_Request.addr)
              << " Value: " << Hex<8>(m_Request.value)
              << " State: " << GetState();
}

#endif   // _WIGIG_ARCH_SPI

