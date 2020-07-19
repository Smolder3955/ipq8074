/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include <functional>
#include "PmcSetConfigHandlerSequences.h"
#include "PmcService.h"
#include "PmcSequence.h"
#include "Host.h"
#include "DeviceManager.h"

OperationStatus PmcSetConfigHandlerSequences::ConfigureUCodeEvents(const std::string& deviceName, bool ucodeCoreWriteEnabled)
{
    BasebandType type;
    Host::GetHost().GetDeviceManager().GetDeviceBasebandType(deviceName, type);
    if (type == BasebandType::BASEBAND_TYPE_SPARROW)
    {
        return PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.ucode_event_en", ucodeCoreWriteEnabled ? 1U : 0U);
    }

    // Talyn-M Configuration
    // In Talyn-M the MSB of the core write does not control PMC write anymore as this bit is used to extend an address range.
    // Instead, 2 address ranges may be configured to send core writes in theese ranges to PMC.
    LOG_DEBUG << "Configuring uCode CORE_WRITE events for Talyn-M" << std::endl;

    if (ucodeCoreWriteEnabled)
    {
        auto st = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.ucode_event_en", 1U);
        if (!st)
        {
            return st;
        }
        // Configure PMC uCode Write Range to Range1 = [NOP, NOP], Range2 = N/A
        // Local Write to MAC_RGF.MAC_SXD.LOCAL_REGISTER_IF.LOCAL_WR_REG.sxd_local_wr_data
        // User Defined Command Register
        // +------------------------+-------+------------------------------------------------------------------------------+
        // | Field                  | Bits  |                                                                              |
        // +------------------------+-------+------------------------------------------------------------------------------+
        // | Flash                  | 0     |  When set, flashes the PMC Uder Define FIFO                                  |
        // +------------------------+-------+------------------------------------------------------------------------------+
        // | Last                   | 1     |  When set, the content of the PMC Uder define FIFO is written as a PMC event |
        // +------------------------+-------+------------------------------------------------------------------------------+
        // | Load uCode Cfg         | 2     |  When set, it loads the PMC Allowed Code                                     |
        // +------------------------+-------+------------------------------------------------------------------------------+
        // | Reserved               | 7:3   |                                                                              |
        // +------------------------+-------+------------------------------------------------------------------------------+
        // | Type                   | 15:8  | The User Define Type Value                                                   |
        // +------------------------+-------+------------------------------------------------------------------------------+
        // | UCode Core Write       | 17:16 | Enable PMC Core Write                                                        |
        // | Command                |       | 00 - Disable PMC core Write                                                  |
        // |                        |       | 01 - Enable Region 1 codes                                                   |
        // |                        |       | 10 - Enable Region 2 codes                                                   |
        // |                        |       | 11 - Enable all codes                                                        |
        // +------------------------+-------+------------------------------------------------------------------------------+
        // | Reserved               | 23:18 |                                                                              |
        // +------------------------+-------+------------------------------------------------------------------------------+
        // | Command Code           | 31:24 | The PMC User defined Command Code 0x47                                       |
        // +------------------------+-------+------------------------------------------------------------------------------+

        uint32_t value = (0x1 << 2) | (0x47 << 24) | (0x1 << 16);
        st = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName,
            "MAC_RGF.MAC_SXD.LOCAL_REGISTER_IF.LOCAL_WR_REG.sxd_local_wr_data", value);
        if (!st)
        {
            return st;
        }
        // 2. Configure MSXD ranges in PMC_UCODE_WR_RANGE_0
        st = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName,
                "MAC_EXT_RGF.MAC_SXD_EXT.PMC_UCODE_WR_RANGE.MAC_EXT_MAC_EXT_RGF_MAC_SXD_EXT_PMC_UCODE_WR_RANGE_0.sxd_pmc_start_1", 0U);
        if (!st)
        {
            return st;
        }
        st = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName,
                "MAC_EXT_RGF.MAC_SXD_EXT.PMC_UCODE_WR_RANGE.MAC_EXT_MAC_EXT_RGF_MAC_SXD_EXT_PMC_UCODE_WR_RANGE_0.sxd_pmc_end_1", 0U);
        if (!st)
        {
            return st;
        }
        st = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName,
                "MAC_EXT_RGF.MAC_SXD_EXT.PMC_UCODE_WR_RANGE.MAC_EXT_MAC_EXT_RGF_MAC_SXD_EXT_PMC_UCODE_WR_RANGE_0.sxd_pmc_start_2", 0U);
        if (!st)
        {
            return st;
        }
        st = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName,
                "MAC_EXT_RGF.MAC_SXD_EXT.PMC_UCODE_WR_RANGE.MAC_EXT_MAC_EXT_RGF_MAC_SXD_EXT_PMC_UCODE_WR_RANGE_0.sxd_pmc_end_2", 0U);
        if (!st)
        {
            return st;
        }

        return OperationStatus(true);
    }
    return PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.ucode_event_en", 1U);
}

OperationStatus PmcSetConfigHandlerSequences::ConfigCollectIdleSm(const std::string& deviceName, bool configurationValue)
{
    auto configRegisterSet = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.idle_sm_en", configurationValue ? 1U : 0U);
    if (!configRegisterSet.IsSuccess())
    {
        return configRegisterSet;
    }

    PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceConfigurations().SetCollectIdleSmEvents(configurationValue);
    if (configurationValue)
    {
        auto st = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.IDLE_SM.MAC_MAC_RGF_PMC_IDLE_SM_0.slot_en", 1U);
        if (!st.IsSuccess())
        {
            return st;
        }
    }
    return OperationStatus(true);
}

OperationStatus PmcSetConfigHandlerSequences::ConfigCollectRxPpdu(const std::string& deviceName, bool configurationValue)
{
    auto configRegisterSet = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.rx_en", configurationValue ? 1U : 0U);
    if (!configRegisterSet.IsSuccess())
    {
        return configRegisterSet;
    }
    PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceConfigurations().SetCollectRxPpduEvents(configurationValue);
    return OperationStatus(true);
}

OperationStatus PmcSetConfigHandlerSequences::ConfigCollectTxPpdu(const std::string& deviceName, bool configurationValue)
{
    auto configRegisterSet = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.tx_en", configurationValue ? 1U : 0U);
    if (!configRegisterSet.IsSuccess())
    {
        return configRegisterSet;
    }
    PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceConfigurations().SetCollectTxPpduEvents(configurationValue);
    return OperationStatus(true);
}

OperationStatus PmcSetConfigHandlerSequences::ConfigCollectUcode(const std::string& deviceName, bool configurationValue)
{
    auto collectUcodeRes = ConfigureUCodeEvents(deviceName, configurationValue);
    if (!collectUcodeRes.IsSuccess())
    {
        return collectUcodeRes;
    }
    PmcService::GetInstance().GetPmcDeviceContext(deviceName).GetDeviceConfigurations().SetCollectUcodeEvents(configurationValue);
    return OperationStatus(true);
}

OperationStatus PmcSetConfigHandlerSequences::ConfigRxMpdu(const std::string& deviceName)
{
    // RX MPDU Data
    auto st = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_1.qos_data_en", 1U);
    if (!st.IsSuccess())
    {
        return st;
    }
    auto st2 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_1.management_en", 1U);
    if (!st2.IsSuccess())
    {
        return st2;
    }
    auto st3 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_1.control_en", 1U);
    if (!st3.IsSuccess())
    {
        return st3;
    }
    auto st4 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_1.beacon_en", 1U);
    if (!st4.IsSuccess())
    {
        return st4;
    }
    auto st5 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_1.error_en", 1U);
    if (!st5.IsSuccess())
    {
        return st5;
    }
    auto st6 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_1.phy_info_en", 1U);
    if (!st6.IsSuccess())
    {
        return st6;
    }
    auto st7 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_2.qos_data_max_data", 0x8);
    if (!st7.IsSuccess())
    {
        return st7;
    }
    auto st8 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_2.management_max_data", 0x8);
    if (!st8.IsSuccess())
    {
        return st8;
    }
    auto st9 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_2.control_max_data", 0x8);
    if (!st9.IsSuccess())
    {
        return st9;
    }
    auto st10 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_2.beacon_max_data", 0x8);
    if (!st10.IsSuccess())
    {
        return st10;
    }
    auto st11 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_3.error_max_data", 0x8);
    if (!st11.IsSuccess())
    {
        return st11;
    }
    auto st12 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_3.phy_info_max_data", 0x8);
    if (!st12.IsSuccess())
    {
        return st12;
    }
    return OperationStatus(true);
}

OperationStatus PmcSetConfigHandlerSequences::ConfigTxMpdu(const std::string& deviceName)
{
    // TX MPDU Data
    auto st = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_1.qos_data_en", 1U);
    if (!st.IsSuccess())
    {
        return st;
    }
    auto st2 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_1.management_en", 1U);
    if (!st2.IsSuccess())
    {
        return st2;
    }
    auto st3 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_1.control_en", 1U);
    if (!st3.IsSuccess())
    {
        return st3;
    }
    auto st4 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_1.beacon_en", 1U);
    if (!st4.IsSuccess())
    {
        return st4;
    }
    auto st5 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_1.direct_pkt_en", 1U);
    if (!st5.IsSuccess())
    {
        return st5;
    }

    auto st6 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_2.qos_data_max_data", 0x8);
    if (!st6.IsSuccess())
    {
        return st6;
    }
    auto st7 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_2.management_max_data", 0x8);
    if (!st7.IsSuccess())
    {
        return st7;
    }
    auto st8 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_2.control_max_data", 0x8);
    if (!st8.IsSuccess())
    {
        return st8;
    }
    auto st9 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_2.beacon_max_data", 0x8);
    if (!st9.IsSuccess())
    {
        return st9;
    }
    auto st10 = PmcService::GetInstance().GetPmcRegistersAccessor().WritePmcRegister(deviceName, "MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_3.direct_pkt_max_data", 0x8);
    if (!st10.IsSuccess())
    {
        return st10;
    }
    return OperationStatus(true);
}
