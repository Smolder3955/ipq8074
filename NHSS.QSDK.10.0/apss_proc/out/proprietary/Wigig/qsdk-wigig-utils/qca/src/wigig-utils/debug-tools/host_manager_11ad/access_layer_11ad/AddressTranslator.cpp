/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "AddressTranslator.h"
#include "DebugLogger.h"

#include <ostream>
#include <stdint.h>

namespace
{
    // Defines a Linker-AHB region mapping. this struct mimics the data
    // structure used by the Linux driver to map addresses. All translation
    // tables are copied from the driver code.
    // Linker address region: [LinkerRegionBegin, LinkerRegionEnd)
    // AHB Region: [AhbRegionBegin, AhbRegionBegin + LinkerRegionEnd - LinkerRegionBegin)
    // Notes:
    // 1. FW and uCode may have linker regions with the same addresses.
    //    These addresses cannot be accessed through linker address, only AHB address is supported.
    // 2. Other regions may be accessed by either linker or AHB address.

    struct fw_map
    {
        uint32_t LinkerRegionBegin;   // Defines linker region begin point
        uint32_t LinkerRegionEnd;     // Defines linker region exclusive end point
        uint32_t AhbRegionBegin;      // Defines corresponding AHB region begin point
        const char* RegionName;       // Region name
        bool IsFwRegion;              // FW/uCode region
    };

    std::ostream& operator<<(std::ostream& os, const fw_map& region)
    {
        return os << "{"
                  << Address(region.LinkerRegionBegin) << ", "
                  << Address(region.LinkerRegionEnd)  << ", "
                  << Address(region.AhbRegionBegin) << ", "
                  << region.RegionName << ", "
                  << BoolStr(region.IsFwRegion)
                  << "}";
    }

// Comment out unused definitions to prevent compilation warnings
// Keep for future use
    //const struct fw_map sparrow_fw_mapping[] = {
    //    /* FW code RAM 256k */
    //    {0x000000, 0x040000, 0x8c0000, "fw_code", true},
    //    /* FW data RAM 32k */
    //    {0x800000, 0x808000, 0x900000, "fw_data", true},
    //    /* periph data 128k */
    //    {0x840000, 0x860000, 0x908000, "fw_peri", true},
    //    /* various RGF 40k */
    //    {0x880000, 0x88a000, 0x880000, "rgf", true},
    //    /* AGC table   4k */
    //    {0x88a000, 0x88b000, 0x88a000, "AGC_tbl", true},
    //    /* Pcie_ext_rgf 4k */
    //    {0x88b000, 0x88c000, 0x88b000, "rgf_ext", true},
    //    /* mac_ext_rgf 512b */
    //    {0x88c000, 0x88c200, 0x88c000, "mac_rgf_ext", true},
    //    /* upper area 548k */
    //    {0x8c0000, 0x949000, 0x8c0000, "upper", true},
    //    /* UCODE areas - accessible by debugfs blobs but not by
    //     * wmi_addr_remap. UCODE areas MUST be added AFTER FW areas!
    //     */
    //    /* ucode code RAM 128k */
    //    {0x000000, 0x020000, 0x920000, "uc_code", false},
    //    /* ucode data RAM 16k */
    //    {0x800000, 0x804000, 0x940000, "uc_data", false},
    //};

///**
// * @sparrow_d0_mac_rgf_ext - mac_rgf_ext section for Sparrow D0
// * it is a bit larger to support extra features
// */
//    const struct fw_map sparrow_d0_mac_rgf_ext = {
//        0x88c000, 0x88c500, 0x88c000, "mac_rgf_ext", true
//    };

/**
 * @talyn_fw_mapping provides memory remapping table for Talyn
 *
 * array size should be in sync with the declaration in the wil6210.h
 *
 * Talyn memory mapping:
 * Linker address         PCI/Host address
 *                        0x880000 .. 0xc80000  4Mb BAR0
 * 0x800000 .. 0x820000   0xa00000 .. 0xa20000  128k DCCM
 * 0x840000 .. 0x858000   0xa20000 .. 0xa38000  96k PERIPH
 */
    const struct fw_map talyn_fw_mapping[] = {
        /* FW code RAM 1M */
        {0x000000, 0x100000, 0x900000, "fw_code", true},
        /* FW data RAM 128k */
        {0x800000, 0x820000, 0xa00000, "fw_data", true},
        /* periph. data RAM 96k */
        {0x840000, 0x858000, 0xa20000, "fw_peri", true},
        /* various RGF 40k */
        {0x880000, 0x88a000, 0x880000, "rgf", true},
        /* AGC table 4k */
        {0x88a000, 0x88b000, 0x88a000, "AGC_tbl", true},
        /* Pcie_ext_rgf 4k */
        {0x88b000, 0x88c000, 0x88b000, "rgf_ext", true},
        /* mac_ext_rgf 1344b */
        {0x88c000, 0x88c540, 0x88c000, "mac_rgf_ext", true},
        /* ext USER RGF 4k */
        {0x88d000, 0x88e000, 0x88d000, "ext_user_rgf", true},
        /* OTP 4k */
        {0x8a0000, 0x8a1000, 0x8a0000, "otp", true},
        /* DMA EXT RGF 64k */
        {0x8b0000, 0x8c0000, 0x8b0000, "dma_ext_rgf", true},
        /* upper area 1536k */
        {0x900000, 0xa80000, 0x900000, "upper", true},
        /* UCODE areas - accessible by debugfs blobs but not by
         * wmi_addr_remap. UCODE areas MUST be added AFTER FW areas!
         */
        /* ucode code RAM 256k */
        {0x000000, 0x040000, 0xa38000, "uc_code", false},
        /* ucode data RAM 32k */
        {0x800000, 0x808000, 0xa78000, "uc_data", false},
    };



}

bool AddressTranslator::ToAhbAddress(uint32_t srcAddr, uint32_t& dstAddr, BasebandType basebandType)
{
    dstAddr = srcAddr;

    if (basebandType != BASEBAND_TYPE_TALYN)
    {
        // TODO - Support all basebands
        LOG_ERROR << "Address translator only supports TALYN map" << std::endl;
        return false;
    }

    for(const fw_map& regionMap: talyn_fw_mapping)
    {
        LOG_VERBOSE << "Checking " << Hex<8>(srcAddr) << " against " << regionMap << std::endl;

        if (srcAddr >= regionMap.LinkerRegionBegin &&
            srcAddr < regionMap.LinkerRegionEnd)
        {
            // Bingo!
            uint32_t offsetInRegion = srcAddr - regionMap.LinkerRegionBegin;
            dstAddr = regionMap.AhbRegionBegin + offsetInRegion;
            LOG_VERBOSE << "Successful address translation (" << basebandType << "): "
                        << Address(srcAddr) << " --> "  << Address(dstAddr) << std::endl;
            return true;
        }
    }

    return false;
}
