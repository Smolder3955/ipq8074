/*
* Copyright (c) 2019 Qualcomm Technologies, Inc.
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

#ifndef _DEBUGLOGGER_H_
#define _DEBUGLOGGER_H_

#include <iostream>
#include <iomanip>
#include <vector>
#include <iterator>

#ifdef _WINDOWS
    #pragma warning(push)
    #pragma warning(disable: 4702) // suppress false unreachable code warning
#endif

// Severity values are expected to raise from zero
enum LogSeverity
{
    LOG_SEV_ERROR   = 0,   // Unexpected input/events that may cause server misbehavior
    LOG_SEV_WARNING = 1,   // Suspicious events
    LOG_SEV_INFO    = 2,   // Events like command/response
    LOG_SEV_DEBUG   = 3,   // Detailed functionality
    LOG_SEV_VERBOSE = 4,   // Excessive debug
};

#define TRACE_WITH_PREFIX(SEV)                                          \
    g_LogConfig.ShouldPrint(SEV) && std::cout << LogMsgPrefix(SEV, __FILE__, __LINE__)


#define LOG_ERROR   TRACE_WITH_PREFIX(LOG_SEV_ERROR)
#define LOG_WARNING TRACE_WITH_PREFIX(LOG_SEV_WARNING)
#define LOG_INFO    TRACE_WITH_PREFIX(LOG_SEV_INFO)
#define LOG_DEBUG   TRACE_WITH_PREFIX(LOG_SEV_DEBUG)
#define LOG_VERBOSE TRACE_WITH_PREFIX(LOG_SEV_VERBOSE)

#define LOG_STATUS                                             \
    g_LogConfig.ShouldPrintStatus()


// Decoupling from system assert allows to print an error message when
// assert is disabled.
#define LOG_ASSERT(CONDITION)                                           \
    do {                                                                \
        if (!(CONDITION)) {                                             \
            LOG_ERROR << "ASSERTION FAILURE: " << #CONDITION            \
                << " at " << __FILE__ << ':' << __LINE__                \
                << std::endl;                                           \
                if (g_LogConfig.ShouldExitOnAssert()) exit(1);          \
        }                                                               \
    } while (false)

// *************************************************************************************************

struct LogConfig
{
public:
    LogConfig(LogSeverity maxSeverity, bool bPrintLocation, bool bPrintTimestamp);
    void SetMaxSeverity(int traceLevel);

    bool ShouldPrint(LogSeverity sev) const { return sev <= m_MaxSeverity; }
    bool ShouldPrintLocation() const { return m_PrintLocation; }
    bool ShouldPrintTimestamp() const { return m_PrintTimestamp; }
    bool ShouldExitOnAssert() const { return m_ExitOnAssert; }

private:

    LogSeverity m_MaxSeverity;
    const bool m_PrintLocation;
    const bool m_PrintTimestamp;
    const bool m_ExitOnAssert;

};

// *************************************************************************************************

extern LogConfig g_LogConfig;

// *************************************************************************************************

class LogMsgPrefix
{
    friend std::ostream& operator<<(std::ostream& os, const LogMsgPrefix& prefix);

public:

    LogMsgPrefix(LogSeverity severity, const char* pFile, int line)
        : Severity(severity), File(pFile), Line(line) {}

private:

    static const char* SeverityToString(LogSeverity sev);

    const LogSeverity Severity;
    const char* const File;
    const int Line;
};


// *************************************************************************************************
// Stream Formatters
// *************************************************************************************************

// Print a boolean value as a string
struct BoolStr
{
    explicit BoolStr(bool value): Value(value) {}
    const bool Value;
};

inline std::ostream& operator<<(std::ostream& os, const BoolStr& boolStr)
{
    return os << std::boolalpha << boolStr.Value << std::noboolalpha;
}

// *************************************************************************************************

// Print a boolean value as a Success/Failure string
struct SuccessStr
{
    explicit SuccessStr(bool value): Value(value) {}
    const bool Value;
};

inline std::ostream& operator<<(std::ostream& os, const SuccessStr& successStr)
{
    return os << (successStr.Value ? "Success" : "Failure");
}

// *************************************************************************************************

// Print a string while displaying newline characters
struct PlainStr
{
    explicit PlainStr(const std::string& value): Value(value) {}
    const std::string& Value;
};

inline std::ostream& operator<<(std::ostream& os, const PlainStr& plainStr)
{
    for (std::string::const_iterator it = plainStr.Value.begin(); it != plainStr.Value.end(); ++it)
    {
        switch (*it)
        {
        case '\r': os << "\\r"; break;
        case '\n': os << "\\n"; break;
        case '\t': os << "\\t"; break;
        default:   os << *it; break;
        }
    }

    return os;
}

// *************************************************************************************************

// Helper class to format hexadecimal numbers
template <int WIDTH = 0> struct Hex
{
    explicit Hex(unsigned long long value): Value(value) {}
    static const unsigned Width = WIDTH;
    const unsigned long long Value;
};

template<int WIDTH>
std::ostream& operator<<(std::ostream& os, const Hex<WIDTH>& hex)
{
    int width = hex.Width;
    std::ios::fmtflags f(os.flags());

    os << "0x";
    if (0 != width) os << std::setw(width) << std::setfill('0');
    os << std::hex << std::uppercase << hex.Value;
    os.flags( f );
    return os;
}

// Prints address as Hex<8>
struct Address: public Hex<8>
{
    explicit Address(unsigned long long value): Hex(value) {}
};


// **************************************************************************************************

// Helper class to print memory content (new line, for large content)
class MemoryDump
{
    friend std::ostream& operator<<(std::ostream& os, const MemoryDump& memoryDump);

public:
    MemoryDump(const unsigned char* pData, size_t length): m_pData(pData), m_Length(length) {}

private:
    const unsigned char* const m_pData;
    const size_t m_Length;
};

inline std::ostream& operator<<(std::ostream& os, const MemoryDump& memoryDump)
{
    std::ios::fmtflags f(os.flags());
    os << std::hex << std::uppercase << std::setfill('0');

    for (size_t i = 0; i < memoryDump.m_Length; ++i)
    {
        if (i % 4 == 0 ) os << "    ";
        if (i % 16 == 0) os << "\n    " << Hex<4>(i) << "    ";
        os << "0x" << std::setw(2) << static_cast<unsigned>(memoryDump.m_pData[i]) << " ";
    }

    os.flags( f );
    return os;
}

// *************************************************************************************************

// Helper class to print memory buffer as a byte array (embedded in the message)
class ByteBuffer
{
    friend std::ostream& operator<<(std::ostream& os, const ByteBuffer& buffer);

public:
    ByteBuffer(const unsigned char* pData, size_t length): m_pData(pData), m_Length(length) {}

private:
    const unsigned char* const m_pData;
    const size_t m_Length;
    const size_t m_MaxPrintedElements = 10;
};

inline std::ostream& operator<<(std::ostream& os, const ByteBuffer& buffer)
{
    std::ios::fmtflags f(os.flags());
    os << std::hex << std::uppercase << std::setfill('0') << '[';

    for (size_t i = 0; i < buffer.m_Length; ++i)
    {
        if (i >= buffer.m_MaxPrintedElements)
        {
            os << "(" << buffer.m_Length - i << " more...)";
            break;
        }

        os << "0x" << std::setw(2) << static_cast<unsigned>(buffer.m_pData[i]);
        if (i != buffer.m_Length - 1) os << ' ';
    }

    os.flags( f );
    return os << ']';
};

// *************************************************************************************************

// Helper class to print memory buffer as a dword array (embedded in the message)
class DwordBuffer
{
    friend std::ostream& operator<<(std::ostream& os, const DwordBuffer& buffer);

public:
    DwordBuffer(const uint32_t* pData, size_t lengthInDwords): m_pData(pData), m_LengthInDwords(lengthInDwords) {}

private:
    const uint32_t* const m_pData;
    const size_t m_LengthInDwords;
};

inline std::ostream& operator<<(std::ostream& os, const DwordBuffer& buffer)
{
    std::ios::fmtflags f(os.flags());
    os << std::hex << std::uppercase << std::setfill('0') << '[';

    for (size_t i = 0; i < buffer.m_LengthInDwords; ++i)
    {
        os << Hex<8>(buffer.m_pData[i]);
        if (i != buffer.m_LengthInDwords - 1) os << ',';
    }

    os.flags(f);
    return os << ']';
};

// *************************************************************************************************

#ifdef _WINDOWS
    #pragma warning(pop)
#endif

#endif // ! _DEBUGLOGGER_H_
