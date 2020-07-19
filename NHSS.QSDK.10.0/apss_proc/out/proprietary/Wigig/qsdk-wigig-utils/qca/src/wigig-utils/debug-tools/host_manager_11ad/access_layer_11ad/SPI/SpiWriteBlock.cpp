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

#include "SpiWriteBlock.h"
#include "SpiUtils.h"
#include "DebugLogger.h"

#include <string.h>
#include <vector>
#include <ostream>

// *************************************************************************************************

SpiWriteBlock::SpiWriteBlock(uint32_t address, std::vector<uint32_t> values)
{
    m_Request.addr = address;
    m_Request.data_len = values.size() * 4;

    // Must be called before data copy to prevent buffer overrun for invlaid request.
    if (!IsValidRequest())
    {
        // Erro is already printed, nothing more to do in this flow.
        return;
    }

    memcpy(m_Request.data, values.data(), m_Request.data_len);
    LOG_VERBOSE << "Creating SPI WriteBlock Request: " << m_Request << std::endl;
}

// *************************************************************************************************

bool SpiWriteBlock::IsValidRequest() const
{
    if (m_Request.data_len > SNS_QWG_DATA_BLOCK_MAX_SIZE_V01)
    {
        LOG_ERROR << "Invalid SPI WriteBlock Request: " << *this
                  << " Max Block Size: " << SNS_QWG_DATA_BLOCK_MAX_SIZE_V01
                  << " Requested Block Size: " << m_Request.data_len
                  << std::endl;

        return false;
    }

    return true;
}

// *************************************************************************************************

bool SpiWriteBlock::ImportResponseBuffer(const uint8_t* pResponseBuf, uint32_t responseSize)
{
    const sns_qwg_chip_mem_write_resp_msg_v01* pResponse =
        GetTypedMessage<sns_qwg_chip_mem_write_resp_msg_v01>(pResponseBuf, responseSize);
    if (nullptr == pResponse) return false;

    LOG_VERBOSE << "Got WriteBlock Response: " << *pResponse << std::endl;

    if (pResponse->resp.sns_result_t != 0)
    {
        LOG_ERROR << "SPI WriteBlock response returned with error: " << pResponse->resp.sns_err_t << std::endl;
        return false;;
    }

    LOG_DEBUG << "SPI WriteBlock import completed" << std::endl;
    return true;
}

// *************************************************************************************************

std::ostream& SpiWriteBlock::Print(std::ostream& os) const
{
    return os << "SPI WriteBlock."
              << " Address: " << Address(m_Request.addr)
              << " Values: " << m_Request.data_len
              << " State: " << GetState();
}

#endif   // _WIGIG_ARCH_SPI

