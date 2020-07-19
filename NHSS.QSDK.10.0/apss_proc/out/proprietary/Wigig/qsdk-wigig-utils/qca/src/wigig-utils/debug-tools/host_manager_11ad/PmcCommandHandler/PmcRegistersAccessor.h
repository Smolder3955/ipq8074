/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef PMC_REGISTER_SACCESSOR_H_
#define PMC_REGISTER_SACCESSOR_H_

#include <map>
#include <string>
#include "OperationStatus.h"
#include "HostManagerDefinitions.h"

//This class is responsible for reading and writing to and from PMC registers
class PmcRegisterInfo
{
    public:

        // Constructs default invalid instance
        PmcRegisterInfo() :
            m_address(0), m_bitStart(0), m_bitEnd(0)
        {
        }

        PmcRegisterInfo(uint32_t regAddress, uint32_t regBitStart, uint32_t regBitEnd) :
                m_address(regAddress), m_bitStart(regBitStart), m_bitEnd(regBitEnd)
        {
        }

        bool IsValid() const
        {
            return m_address != 0;
        }

        uint32_t GetRegisterAddress() const
        {
            return m_address;
        }

        uint32_t GetBitStart() const
        {
            return m_bitStart;
        }

        uint32_t GetBitEnd() const
        {
            return m_bitEnd;
        }

    private:

        const uint32_t m_address;
        const uint32_t m_bitStart;
        const uint32_t m_bitEnd;
};

class PmcRegistersAccessor
{
    public:

        OperationStatus WritePmcRegister(const std::string& deviceName, const std::string& registerName, uint32_t value) const;
        OperationStatus ReadPmcRegister(const std::string& deviceName, const std::string& registerName, uint32_t& registerValue) const;
        uint32_t WriteToBitMask(uint32_t dataBufferToWriteTo, uint32_t index, uint32_t size, uint32_t valueToWriteInData) const;

    private:
        //register name => {device name, registerInfo}
        static const std::map<std::string, std::map<BasebandRevisionEnum, PmcRegisterInfo>> s_pmcRegisterMap;

        PmcRegisterInfo GetPmcRegisterInfo(const std::string& registerName, const BasebandRevisionEnum deviceType) const;
        OperationStatus WriteRegister(const std::string& deviceName, uint32_t address, uint32_t value) const;
        OperationStatus ReadRegister(const std::string& deviceName, uint32_t address, uint32_t& value) const;
        uint32_t GetBitMask(uint32_t index, uint32_t size) const;
        uint32_t ReadFromBitMask(uint32_t data, uint32_t index, uint32_t size) const;

};

#endif /* PMC_REGISTER_SACCESSOR_H_ */
