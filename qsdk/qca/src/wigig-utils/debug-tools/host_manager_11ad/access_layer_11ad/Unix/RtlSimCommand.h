/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _UNIX_RTL_COMMAND_H_
#define _UNIX_RTL_COMMAND_H_

#include <ostream>
#include <vector>

#include "DebugLogger.h"

// *************************************************************************************************

class IRtlSimCommand
{
public:

    IRtlSimCommand() {}
    virtual ~IRtlSimCommand() {}

    virtual void FlushToFile(std::ostream& os) const = 0;
    virtual std::ostream& WriteToLog(std::ostream& os) const = 0;

    // Non-copyable
    IRtlSimCommand(const IRtlSimCommand&) = delete;
    IRtlSimCommand& operator=(const IRtlSimCommand&) = delete;
};

inline std::ostream& operator<<(std::ostream& os, const IRtlSimCommand& rtlSimCmd)
{
    return rtlSimCmd.WriteToLog(os);
}

// *************************************************************************************************

class RtlSimCommandRead: public IRtlSimCommand
{
public:

    RtlSimCommandRead(uint32_t address)
        : m_Address(address)
    {
    }

    void FlushToFile(std::ostream& os) const override
    {
        os << "RD\n"
           << Hex<8>(m_Address)
           << std::endl;
    }

    std::ostream& WriteToLog(std::ostream& os) const override
    {
        return os << "RD(" << Address(m_Address) << ')';
    }

private:

    const uint32_t m_Address;
};

// *************************************************************************************************

class RtlSimCommandWrite: public IRtlSimCommand
{
public:

    RtlSimCommandWrite(uint32_t address, uint32_t value)
        : m_Address(address)
        , m_Value (value)
    {
    }

    void FlushToFile(std::ostream& os) const override
    {
        os << "WR\n"
           << Hex<8>(m_Address) << '\n'
           << Hex<8>(m_Value)
           << std::endl;
    }

    std::ostream& WriteToLog(std::ostream& os) const override
    {
        return os << "WR(" << Address(m_Address) << ") = " << Hex<8>(m_Value);
    }

private:

    const uint32_t m_Address;
    const uint32_t m_Value;
};

// *************************************************************************************************

class RtlSimCommandReadBlock: public IRtlSimCommand
{
public:

    RtlSimCommandReadBlock(uint32_t address, size_t blockSize)
        : m_Address(address)
        , m_BlockSize(blockSize)
    {
    }

    void FlushToFile(std::ostream& os) const override
    {
        os << "RB\n"
           << Hex<8>(m_Address) << '\n'
           << m_BlockSize
           << std::endl;
    }

    std::ostream& WriteToLog(std::ostream& os) const override
    {
        return os << "RB(" << Address(m_Address) << ", " << m_BlockSize << ')';
    }

private:

    const uint32_t m_Address;
    const size_t m_BlockSize;
};

// *************************************************************************************************

class RtlSimCommandWriteBlock: public IRtlSimCommand
{
public:

    RtlSimCommandWriteBlock(uint32_t address, const std::vector<uint32_t>& values)
        : m_Address(address)
        , m_Values(values)
    {
    }

    void FlushToFile(std::ostream& os) const override
    {
        os << "WB\n"
           << Hex<8>(m_Address) << '\n'
           << m_Values.size();

        for (const auto& value : m_Values)
        {
            os << '\n' << Hex<8>(value);
        }

        os << std::endl;
    }

    std::ostream& WriteToLog(std::ostream& os) const override
    {
        os << "WB(" << Address(m_Address) << ", " << m_Values.size() << ") = [";
        for (const auto& value : m_Values)
        {
            os << Hex<8>(value) << ", ";
        }

        return os << "\b\b]";
    }


private:
    const uint32_t m_Address;
    const std::vector<uint32_t>& m_Values;
};


#endif   // _UNIX_RTL_COMMAND_H_

