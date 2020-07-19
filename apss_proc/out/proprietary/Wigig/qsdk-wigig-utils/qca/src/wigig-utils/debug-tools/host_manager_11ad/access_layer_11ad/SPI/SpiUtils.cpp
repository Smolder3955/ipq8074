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

#include "SpiUtils.h"
#include "DebugLogger.h"

#include <ostream>

// *************************************************************************************************

#define CASE_TO_TOKEN_RETURN(X)   case (X): return #X;

namespace
{
    const char* SnsQwgRequestTypeToString(uint32_t msgType)
    {
        switch (msgType)
        {
            CASE_TO_TOKEN_RETURN(SNS_QWG_CANCEL_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_VERSION_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_OPEN_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CLOSE_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_KEEP_ALIVE_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_SIMPLE_ITEM_WRITE_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_DEBUG_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CHIP_MEM_WRITE_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CHIP_MEM_DIRECT_WRITE_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CHIP_RESET_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_WMI_SEND_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CHIP_REG_WRITE_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CHIP_REG_READ_REQ_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CHIP_MEM_READ_REQ_V01);
        default: return "<unknown>";
        }
    }

    const char* SnsQwgResponseTypeToString(uint32_t msgType)
    {
        switch (msgType)
        {
            CASE_TO_TOKEN_RETURN(SNS_QWG_CANCEL_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_VERSION_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_OPEN_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CLOSE_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_KEEP_ALIVE_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_SIMPLE_ITEM_WRITE_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_DEBUG_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CHIP_MEM_WRITE_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CHIP_MEM_DIRECT_WRITE_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CHIP_RESET_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_WMI_SEND_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_WMI_EVENT_IND_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_FW_MBOX_READY_IND_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CHIP_REG_WRITE_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CHIP_REG_READ_RESP_V01);
            CASE_TO_TOKEN_RETURN(SNS_QWG_CHIP_MEM_READ_RESP_V01);
        default: return "<unknown>";
        }
    }

    const char* Sensor1ErrorCodeToString(const sensor1_error_e& errorCode)
    {
        switch (errorCode)
        {
        case SENSOR1_SUCCESS:
            return "SENSOR1_SUCCESS";
        case SENSOR1_EBUFFER: /**< Invalid buffer */
            return "SENSOR1_EBUFFER";
        case SENSOR1_ENOMEM: /**< Insufficient memory to process request */
            return "SENSOR1_ENOMEM";
        case SENSOR1_EINVALID_CLIENT: /**< Invalid client handle */
            return "SENSOR1_EINVALID_CLIENT";
        case SENSOR1_EUNKNOWN: /**< Unknown error reason */
            return "SENSOR1_EUNKNOWN";
        case SENSOR1_EFAILED: /**< Generic error */
            return "SENSOR1_EFAILED";
        case SENSOR1_ENOTALLOWED: /**< Operation not allowed at this time */
            return "SENSOR1_ENOTALLOWED";
        case SENSOR1_EBAD_PARAM: /**< Invalid parameter value */
            return "SENSOR1_EBAD_PARAM";
        case SENSOR1_EBAD_PTR: /**< Invalid pointer. May be due to NULL values, or
                                  pointers to memory not allocated with
                                  {@link sensor1_alloc_msg_buf}. */
            return "SENSOR1_EBAD_PTR";
        case SENSOR1_EBAD_MSG_ID: /**< The service specified does not support this message
                                     ID */
            return "SENSOR1_EBAD_MSG_ID";
        case SENSOR1_EBAD_MSG_SZ: /**< The message size does not match the size of the
                                     message determined  by the service/message ID */
            return "SENSOR1_EBAD_MSG_SZ";
        case SENSOR1_EWOULDBLOCK: /**< Function was not successful, and would need to
                                     block to complete successfully. The client should
                                     retry at a later time. */
            return "SENSOR1_EWOULDBLOCK";
        case SENSOR1_EBAD_SVC_ID: /**< Unknown service ID/service number */
            return "SENSOR1_EBAD_SVC_ID";
        default:
            return "<unknown>";
        }
    }
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const wmi_cmd_hdr& wmiNativeMsgHdr)
{
    return os << "{"
              << " mid: "  << static_cast<uint>(wmiNativeMsgHdr.mid)
              << " command_id: "   << wmiNativeMsgHdr.command_id
              << " fw_timestamp: " << Hex<8>(wmiNativeMsgHdr.fw_timestamp)
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const SnsQwgRequestType& reqType)
{
    return os << reqType.m_TypeCode << " (" << SnsQwgRequestTypeToString(reqType.m_TypeCode) << ')';
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const SnsQwgResponseType& respType)
{
    return os << respType.m_TypeCode << " (" << SnsQwgResponseTypeToString(respType.m_TypeCode) << ')';
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sensor1_error_e& errorCode)
{
    return os << static_cast<unsigned>(errorCode) << " (" << Sensor1ErrorCodeToString(errorCode) << ')';
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sensor1_msg_header_s& msgHdr)
{
    return os << "{"
              << " Service Num: "  << msgHdr.service_number
              << " Message ID: "   << SnsQwgRequestType(msgHdr.msg_id)
              << " Message Size: " << msgHdr.msg_size
              << " TXN ID:"        << static_cast<unsigned>(msgHdr.txn_id)
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_common_resp_s_v01& respHdr)
{
    return os << "{"
              << " sns_result_t: " << static_cast<unsigned>(respHdr.sns_result_t)
              << " sns_err_t: " << static_cast<sensor1_error_e>(respHdr.sns_err_t)
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_reg_read_req_msg_v01& readReq)
{
    return os << "{"
              << " addr: " << Address(readReq.addr)
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_reg_read_resp_msg_v01& readResp)
{
    return os << "{"
              << " resp: " << readResp.resp
              << " value: " << Hex<8>(readResp.value)
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_mem_read_req_msg_v01& readBlockReq)
{
    return os << "{"
              << " addr: " << Address(readBlockReq.addr)
              << " size: " << readBlockReq.size
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_mem_read_resp_msg_v01& readBlockResp)
{
    return os << "{"
              << " resp: " << readBlockResp.resp
              << " data_len: " << readBlockResp.data_len
              << " data: " << DwordBuffer(reinterpret_cast<const uint32_t *>(readBlockResp.data), readBlockResp.data_len/4)
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_reg_write_req_msg_v01& writeReq)
{
    return os << "{"
              << " addr: " << Address(writeReq.addr)
              << " value: " << Hex<8>(writeReq.value)
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_reg_write_resp_msg_v01& writeResp)
{
    return os << "{"
              << " resp: " << writeResp.resp
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_mem_write_req_msg_v01& writeBlockReq)
{
return os << "{"
          << " addr: " << Address(writeBlockReq.addr)
          << " data_len: " << writeBlockReq.data_len
          << " data: " << DwordBuffer(reinterpret_cast<const uint32_t *>(writeBlockReq.data), writeBlockReq.data_len/4)
          << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_qwg_chip_mem_write_resp_msg_v01& writeBlockResp)
{
    return os << "{"
              << " resp: " << writeBlockResp.resp
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_common_version_resp_msg_v01& verResp)
{
    return os << "{"
              << " resp: " << verResp.resp
              << " interface_version_number: " << verResp.interface_version_number
              << " max_message_id: " << verResp.max_message_id
              << " }";

}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_qwg_wmi_send_req_msg_v01& wmiReq)
{
    return os << "{"
              << " command_id: " << wmiReq.command_id
              << " mid: " << wmiReq.mid
              << " data_len: " << wmiReq.data_len
              << " data: " << DwordBuffer(reinterpret_cast<const uint32_t *>(wmiReq.data), wmiReq.data_len/4)
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_qwg_wmi_send_resp_msg_v01& wmiResp)
{
    return os << "{"
              << " resp: " << wmiResp.resp
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_qwg_wmi_event_ind_msg_v01& wmiEvent)
{
    return os << "{"
              << " data_len: " << wmiEvent.data_len
              << " data: " << DwordBuffer(reinterpret_cast<const uint32_t *>(wmiEvent.data), wmiEvent.data_len/4)
              << " }";
}

// *************************************************************************************************

std::ostream& operator<<(std::ostream& os, const sns_qwg_fw_mbox_ready_ind_msg_v01& mboxReady)
{
    return os << "{"
              << " ready: " << static_cast<unsigned>(mboxReady.ready)
              << " }";

}

#endif // _WIGIG_ARCH_SPI
