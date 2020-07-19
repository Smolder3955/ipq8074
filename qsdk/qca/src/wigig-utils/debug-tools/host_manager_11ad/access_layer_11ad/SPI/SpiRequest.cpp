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

#include "SpiRequest.h"
#include "DebugLogger.h"

#include <sns_common_v01.h>
#include <ostream>

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const SpiRequestState& spiReqState)
{
    switch(spiReqState)
    {
    case SPI_REQ_CREATED:   return os << "SPI_REQ_CREATED";
    case SPI_REQ_SENT:      return os << "SPI_REQ_SENT";
    case SPI_REQ_COMPLETED: return os << "SPI_REQ_COMPLETED";
    case SPI_REQ_FAILED:    return os << "SPI_REQ_FAILED";
    default:                return os << "<unknown: " << spiReqState << ">";
    }
}

// *************************************************************************************************

bool SpiRequest::CreateRequestHeader(sensor1_msg_header_s& msgHdr)
{
    if (!IsValidRequest())
    {
        LOG_ERROR << "Invalid request: " << *this << std::endl;
        return false;
    }

    msgHdr.service_number = SNS_QWG_SVC_ID_V01;
    msgHdr.msg_id = GetMessageId();
    msgHdr.msg_size = GetRequestLength();

    return true;
}

// *************************************************************************************************

void SpiRequest::Complete(const uint8_t* pResponseBuf, uint32_t responseSize)
{
    if (nullptr == pResponseBuf || 0 == responseSize)
    {
        LOG_ERROR << "Got invalid SPI response" << std::endl;
        m_State = SPI_REQ_FAILED;
        return;
    }

    if (!ImportResponseBuffer(pResponseBuf, responseSize))
    {
        LOG_ERROR << "Failure importing reposnse buffer" << std::endl;
        m_State = SPI_REQ_FAILED;
        return;
    }

    m_State = SPI_REQ_COMPLETED;
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const SpiRequest& spiRequest)
{
    return spiRequest.Print(os);
}


#endif   // _WIGIG_ARCH_SPI
