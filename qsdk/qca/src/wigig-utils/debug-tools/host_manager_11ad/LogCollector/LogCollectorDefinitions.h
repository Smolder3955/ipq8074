/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/
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

#ifndef _LOG_COLLECTOR_DEFINITIONS
#define _LOG_COLLECTOR_DEFINITIONS

#pragma once

#include <string>
#include <map>

#include "HostManagerDefinitions.h"

namespace log_collector
{
    // types
    enum MODULES
    {
        SYSTEM,
        DRIVERS,
        MAC_MON,
        HOST_CMD,
        PHY_MON,
        INFRA,
        CALIBS,
        TXRX,
        RAD_MGR,
        SCAN,
        MLME,
        L2_MGR,
        DISC,
        MGMT_SRV,
        SEC_PSM,
        WBE_MNGR,
        NUM_MODULES,
    };
    enum RecordingType
    {
        RECORDING_TYPE_RAW,
        RECORDING_TYPE_TXT,
        RECORDING_TYPE_BOTH
    };

    std::ostream &operator<<(std::ostream &output, const RecordingType &rt);

    // TODO: should be moved here from HostManagerDefinitions.
    enum CpuType
    {
        CPU_TYPE_FW,
        CPU_TYPE_UCODE,
        CPU_TYPE_BOTH
    };

    std::ostream &operator<<(std::ostream &output, const CpuType &rt);

    static std::map<CpuType, std::string> CPU_TYPE_TO_STRING = { { CPU_TYPE_FW, "fw" },{ CPU_TYPE_UCODE, "ucode" },{ CPU_TYPE_BOTH, "both" } };


#ifdef _WINDOWS
    /*
        module_level_info 8 bit representation:
        LSB                                 MSB
        +-----+-------+-----+-----+-----+-----+
        | 3:R | 1:TPM | 1:V | 1:I | 1:W | 1:E |
        +-----+-------+-----+-----+-----+-----+

    */

    struct module_level_info { /* Little Endian */
        unsigned char error_level_enable : 1;   // E
        unsigned char warn_level_enable : 1;    // W
        unsigned char info_level_enable : 1;    // I
        unsigned char verbose_level_enable : 1; // V
        unsigned char third_party_mode : 1;     // TPM - This bit is set if we are on Third party mode and need to deal with logs with \n
        unsigned char reserved0 : 3;            // R

        module_level_info() :
            error_level_enable(0),
            warn_level_enable(0),
            info_level_enable(0),
            verbose_level_enable(0),
            third_party_mode(0),
            reserved0(0) {}

        module_level_info& operator=(const module_level_info& other)
        {
            if (this == &other)
            {
                return *this;
            }
            error_level_enable = other.error_level_enable;
            warn_level_enable = other.warn_level_enable;
            info_level_enable = other.info_level_enable;
            verbose_level_enable = other.verbose_level_enable;
            //third_party_mode = other.third_party_mode; // we do not want to override the third party bit.
            reserved0 = other.reserved0;
            return *this;
        }

        bool operator==(const module_level_info& other)
        {
            if (this == &other)
            {
                return true;
            }
            return (error_level_enable == other.error_level_enable &&
                warn_level_enable == other.warn_level_enable &&
                info_level_enable == other.info_level_enable &&
                verbose_level_enable == other.verbose_level_enable &&
                // third_party_mode == other.third_party_mode && // we do not want to compare the third party bit.
                reserved0 == other.reserved0);
        }

        bool operator!=(const module_level_info& other) { return !(*this == other); }

    };

    /*
    log_trace_header 32 bit representation:
    LSB                                         MSB
    +------+-------+-----+-----+-------+------+-------+
    |2:DNM | 18:SO | 4:M | 2:V | 2:DNL | 1:IS | 3:SIG |
    +------+-------+-----+-----+-------+------+-------+

    */

    struct log_trace_header { /* Little Endian */
        //LSB
        unsigned dword_num_msb : 2;     /* DNM */
        unsigned string_offset : 18;   /* SO */
        unsigned module : 4;            /* M - module that outputs the trace */
        unsigned level : 2;             /* V - verbosity level [Error, Warning, Info, Verbose]*/
        unsigned dword_num_lsb : 2;     /* DNL - to make the dw_num. If Old Mode just take this 2 bits*/
        unsigned is_string : 1; /* IS - (obsolete) indicate if the printf uses %s*/
        unsigned signature : 3; /* SIG - should be 5 (2'101) in a valid header */
        //MSB
    };
#else
    struct module_level_info { /* Little Endian */
        unsigned error_level_enable : 1;
        unsigned warn_level_enable : 1;
        unsigned info_level_enable : 1;
        unsigned verbose_level_enable : 1;
        unsigned third_party_mode : 1; // This bit is set if we are on Third party mode and need to deal with logs with \n
        unsigned reserved0 : 3;

        module_level_info() :
            error_level_enable(0),
            warn_level_enable(0),
            info_level_enable(0),
            verbose_level_enable(0),
            third_party_mode(0),
            reserved0(0) {}

        module_level_info& operator=(const module_level_info& other)
        {
            if (this == &other)
            {
                return *this;
            }
            error_level_enable = other.error_level_enable;
            warn_level_enable = other.warn_level_enable;
            info_level_enable = other.info_level_enable;
            verbose_level_enable = other.verbose_level_enable;
            //third_party_mode = other.third_party_mode; // we do not want to override the third party bit.
            reserved0 = other.reserved0;
            return *this;
        }

        bool operator==(const module_level_info& other)
        {
            if (this == &other)
            {
                return true;
            }
            return (error_level_enable == other.error_level_enable &&
                warn_level_enable == other.warn_level_enable &&
                info_level_enable == other.info_level_enable &&
                verbose_level_enable == other.verbose_level_enable &&
                // third_party_mode == other.third_party_mode &&// we do not want to compare the third party bit.
                reserved0 == other.reserved0);
        }

        bool operator!=(const module_level_info& other) { return !(*this == other); }

    } __attribute__((packed));

    struct log_trace_header { /* Little Endian */
                              /* the offset of the trace string in the strings sections */
        unsigned dword_num_msb : 2;
        unsigned string_offset : 18;
        //unsigned string_offset : 20;
        unsigned module : 4; /* module that outputs the trace */
        unsigned level : 2;  /*verbosity level [Error, Warning, Info, verbose]*/
        unsigned dword_num_lsb : 2; /* to make the dw_num */
        unsigned is_string : 1; /* indicate if the printf uses %s */
        unsigned signature : 3; /* should be 5 (2'101) in valid header */
    } __attribute__((packed));
#endif

    std::ostream& operator<<(std::ostream& os, const module_level_info& moduleLevelInfo);

    union log_event {
        struct log_trace_header hdr;
        int param ;
    };

    typedef struct log_buffer_t {
        unsigned write_ptr; /* incremented by trace producer every write */
        struct module_level_info module_level_info_array[NUM_MODULES];
        union log_event evt[0];
    }log_buffer;

    // consts
    extern const std::map<CpuType, size_t> logTracerTypeToLogBufferSizeInDwords;
    extern const std::string module_names[NUM_MODULES];
    extern const char *const levels[];
    extern const std::map<BasebandType, unsigned> baseband_to_peripheral_memory_start_address_linker;
    extern const std::map<BasebandType, unsigned> baseband_to_peripheral_memory_start_address_ahb;
    extern const std::map<BasebandType, unsigned> baseband_to_ucode_dccm_start_address_linker;
    extern const std::map<BasebandType, unsigned> baseband_to_ucode_dccm_start_address_ahb;
    extern const std::map<CpuType, int> logTracerTypeToLogOffsetAddress;

    // configuration parameters
    extern const std::string POLLING_INTERVAL_MS;
    extern const std::string OUTPUT_FILE_SUFFIX;
    extern const std::string MODULES_VERBOSITY_LEVELS;
    extern const std::string MAX_SINGLE_FILE_SIZE_MB;
    extern const std::string MAX_NUM_OF_LOG_FILES;
    extern const std::string CONVERSION_FILE_PATH;
    extern const std::string LOG_FILES_DIRECTORY;
}

#endif // !_LOG_COLLECTOR_DEFINITIONS

