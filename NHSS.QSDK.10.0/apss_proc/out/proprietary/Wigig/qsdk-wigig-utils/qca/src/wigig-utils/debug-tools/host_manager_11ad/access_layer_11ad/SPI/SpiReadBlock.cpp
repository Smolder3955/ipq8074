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

#include "SpiReadBlock.h"
#include "SpiUtils.h"
#include "DebugLogger.h"

#include <ostream>
#include <vector>

// *************************************************************************************************

SpiReadBlock::SpiReadBlock(uint32_t address, uint32_t sizeInDwords, std::vector<uint32_t>& refResult)
    : m_RefResult(refResult)
{
    m_Request.addr = address;
    m_Request.size = sizeInDwords * 4;

    LOG_VERBOSE << "Creating SPI ReadBlock Request: " << m_Request << std::endl;
}

// *************************************************************************************************

bool SpiReadBlock::IsValidRequest() const
{
    if (m_Request.size > SNS_QWG_DATA_BLOCK_MAX_SIZE_V01)
    {
        LOG_ERROR << "Invalid SPI ReadBlock Request: " << *this
                  << " Max Block Size: " << SNS_QWG_DATA_BLOCK_MAX_SIZE_V01
                  << " Requested Block Size: " << m_Request.size
                  << std::endl;

        return false;
    }

    return true;
}

// *************************************************************************************************

bool SpiReadBlock::ImportResponseBuffer(const uint8_t* pResponseBuf, uint32_t responseSize)
{
    const sns_qwg_chip_mem_read_resp_msg_v01* pResponse =
        GetTypedMessage<sns_qwg_chip_mem_read_resp_msg_v01>(pResponseBuf, responseSize, SNS_QWG_SIMPLE_ITEM_MAX_SIZE_V01);
    if (nullptr == pResponse) return false;

    LOG_VERBOSE << "Got ReadBlock Response: " << *pResponse << std::endl;

    if (pResponse->resp.sns_result_t != 0)
    {
        LOG_ERROR << "SPI ReadBlock response returned with error: " << pResponse->resp.sns_err_t << std::endl;
        return false;;
    }

    if (pResponse->data_len != m_Request.size)
    {
        LOG_ERROR << "Unexpected size of RB response."
                  << " Expected: " << m_Request.size
                  << " Actual: " << pResponse->data_len
                  << std::endl;
        return false;
    }

    // std::vector support move constructor, so the following construct is efficient.
    m_RefResult = std::vector<uint32_t>(reinterpret_cast<const uint32_t *>(pResponse->data),
                                        reinterpret_cast<const uint32_t *>(pResponse->data) + pResponse->data_len/4);

    LOG_DEBUG << "SPI ReadBlock import completed. DWORDs: " << m_RefResult.size() << std::endl;
    return true;
}

// *************************************************************************************************

std::ostream& SpiReadBlock::Print(std::ostream& os) const
{
    return os << "SPI Read."
              << " Address: " << Address(m_Request.addr)
              << " DWORDs: " << m_Request.size / 4
              << " State: " << GetState();
}

#endif   // _WIGIG_ARCH_SPI

