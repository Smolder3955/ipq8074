/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#ifndef _FIELD_DESCRIPTION_H_
#define _FIELD_DESCRIPTION_H_

#include <climits>
#include <stdint.h>

class FieldDescription final
{
public:
    FieldDescription(uint32_t address, uint32_t firstBit, uint32_t lastBit) :
        m_address(address), m_firstBit(firstBit), m_mask(CreateBitMask(firstBit, lastBit))
    {}

    uint32_t GetAddress() const { return m_address; }
    uint32_t MaskValue(uint32_t value) const { return (value & m_mask) >> m_firstBit; }

    // make non-copyable
    FieldDescription(const FieldDescription& r) = delete;
    FieldDescription& operator=(const FieldDescription& r) = delete;

private:
    const uint32_t m_address;
    const uint32_t m_firstBit;
    const uint32_t m_mask;

    // Create 32 bits mask, first and last bits are zero-based
    static uint32_t CreateBitMask(uint32_t firstBit, uint32_t lastBit)
    {
        return Bitmask(lastBit - firstBit + 1U) << firstBit;
        // another option:
        // mask = Bitmask(lastBit + 1U) & ~Bitmask(firstBit)
    }

    // create 32 bits mask of 1's till the given ONE-based bitIndex
    static constexpr uint32_t Bitmask(uint32_t numMaskedBits)
    {
        return static_cast<uint32_t>(-(numMaskedBits != 0)) & (static_cast<uint32_t>(-1) >> ((sizeof(uint32_t) * CHAR_BIT) - numMaskedBits));
        // another option:
        // (numMaskedBits == sizeof(uint32_t) * CHAR_BIT) ? static_cast<uint32_t>(-1) : (static_cast<uint32_t>(1) << numMaskedBits) - 1
    }
};

#endif // _FIELD_DESCRIPTION_H_
