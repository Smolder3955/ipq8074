/*
* Copyright (c) 2018-2019 Qualcomm Technologies, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Technologies, Inc.
*/

#include "PmcRegistersAccessor.h"
#include "DeviceManager.h"
#include "Host.h"
#include "PmcSequence.h"
#include "DebugLogger.h"

const std::map<std::string, std::map<BasebandRevisionEnum, PmcRegisterInfo>> PmcRegistersAccessor::s_pmcRegisterMap
{
     {"MAC_RGF.MAC_SXD.TIMING_INDIRECT.TIMING_INDIRECT_REG_5.msrb_capture_ts_low", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886eb8 ,0 , 31}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88607c ,0 , 31}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.delimiter", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88607c ,0 , 15}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.rec_en_set",{{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88607c ,16 , 16}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.rec_en_clr", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88607c ,17 , 17}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.rec_active", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88607c ,18 , 18}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.rx_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88607c ,24 , 24}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.tx_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88607c ,25 , 25}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.ucode_event_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88607c ,26 , 26}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.idle_sm_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88607c ,27 , 27}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.fw_udef_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88607c ,28 , 28}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_0.ucpu_udef_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88607c ,29, 29}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_1", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886080 ,0, 31}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_1.intf_type", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886080 ,0, 0}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_1.dma_if_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886080 ,2, 2}}}},
     {"MAC_RGF.PMC.GENERAL.MAC_MAC_RGF_PMC_GENERAL_1.pkt_treshhold", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886080 ,16, 27}}}},
     {"MAC_RGF.PMC.IDLE_SM.MAC_MAC_RGF_PMC_IDLE_SM_0.slot_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860d0 ,0, 0}}}},
     {"MAC_RGF.PMC.RX_TX.MAC_MAC_RGF_PMC_RX_TX_0", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88608c ,0, 31}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_0.qid_mask", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886094 ,0, 31}}}},
     {"MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_0.qid_mask", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860ac ,0, 31}}}},
     {"MAC_RGF.PRS.CTRL.MAC_PRS_CTRL_0.pmc_post_dec_mode", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886200 ,16, 16}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_1.qos_data_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886098 ,0, 0}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_1.management_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886098 ,1, 1}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_1.control_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886098 ,2, 2}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_1.beacon_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886098 ,3, 3}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_1.error_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886098 ,4, 4}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_1.phy_info_en", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886098 ,5, 5}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_2.qos_data_max_data", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88609c ,0, 7}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_2.management_max_data", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88609c ,8, 15}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_2.control_max_data", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88609c ,16, 23}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_2.beacon_max_data", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88609c ,24, 31}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_3.error_max_data", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860a0 ,0, 7}}}},
     {"MAC_RGF.PMC.RX_TX.RX.MAC_MAC_RGF_PMC_RX_3.phy_info_max_data", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860a0 ,8, 15}}}},
     {"MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_1.qos_data_en", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860b0 ,0, 0}}}},
     {"MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_1.management_en", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860b0 ,1, 1}}}},
     {"MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_1.control_en", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860b0 ,2, 2}}}},
     {"MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_1.beacon_en", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860b0 ,3, 3}}}},
     {"MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_1.direct_pkt_en", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860b0 ,4, 4}}}},
     {"MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_2.qos_data_max_data", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860b4 ,0, 7}}}},
     {"MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_2.management_max_data", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860b4 ,8, 15}}}},
     {"MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_2.control_max_data", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860b4 ,16, 23}}}},
     {"MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_2.beacon_max_data", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860b4 ,24, 31}}}},
     {"MAC_RGF.PMC.RX_TX.TX.MAC_MAC_RGF_PMC_RX_TX_TX_3.direct_pkt_max_data", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8860b8 ,0, 7}}}},
     {"MAC_RGF.MAC_SXD.LOCAL_REGISTER_IF.LOCAL_WR_REG.sxd_local_wr_data", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x886dc8 ,0, 31}}}},
     {"USER.CLK_CTL_EXTENTION.USER_USER_CLKS_CTL_EXT_SW_BYPASS_HW_CG_0.mac_pmc_clk_sw_bypass_hw", {{BasebandRevisionEnum::SPR_B0, PmcRegisterInfo{0x880c20 ,9, 9}}, {BasebandRevisionEnum::SPR_D0, PmcRegisterInfo{0x880c20 ,9, 9}}}},
     {"USER.CLKS_CTL.SW.RST.USER_USER_CLKS_CTL_SW_RST_VEC_0", { {BasebandRevisionEnum::TLN_M_B0, PmcRegisterInfo{0x880b04 ,0, 31}}, {BasebandRevisionEnum::TLN_M_C0, PmcRegisterInfo{ 0x880b04 ,0, 31}} }},
     {"USER.EXTENTION.USER_USER_EXTENTION_3.mac_pmc_clk_sw_bypass_hw", { {BasebandRevisionEnum::TLN_M_B0, PmcRegisterInfo{0x880C20 ,9, 9}}, {BasebandRevisionEnum::TLN_M_C0, PmcRegisterInfo{ 0x880C20 ,9, 9}} }},
     {"USER.EXTENTION.USER_USER_EXTENTION_3.mac_prp_ahb_rgf_hclk_sw_bypass_hw", {{BasebandRevisionEnum::TLN_M_B0, PmcRegisterInfo{0x880C20 ,16, 16}}, {BasebandRevisionEnum::TLN_M_C0, PmcRegisterInfo{0x880C20 ,16, 16}} }},
     {"DMA_RGF.DESCQ.<0>.IS.IS.is_pmc_dma", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8812dc ,6, 6}}}},
     {"DMA_RGF.DESCQ.<1>.IS.IS.is_pmc_dma", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88134c ,6, 6}}}},
     {"DMA_RGF.DESCQ.<2>.IS.IS.is_pmc_dma", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8813bc ,6, 6}}}},
     {"DMA_RGF.DESCQ.<3>.IS.IS.is_pmc_dma", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88142c ,6, 6}}}},
     {"DMA_RGF.DESCQ.<4>.IS.IS.is_pmc_dma", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88149c ,6, 6}}}},
     {"DMA_RGF.DESCQ.<5>.IS.IS.is_pmc_dma", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88150c ,6, 6}}}},
     {"DMA_RGF.DESCQ.<6>.IS.IS.is_pmc_dma", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88157c ,6, 6}}}},
     {"DMA_RGF.DESCQ.<7>.IS.IS.is_pmc_dma", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8815ec ,6, 6}}}},
     {"DMA_RGF.DESCQ.<8>.IS.IS.is_pmc_dma", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88165c ,6, 6}}}},
     {"DMA_RGF.DESCQ.<9>.IS.IS.is_pmc_dma", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8816cc ,6, 6}}}},
     {"DMA_RGF.DESCQ.<10>.IS.IS.is_pmc_dma", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x88173c ,6, 6}}}},
     {"DMA_RGF.DESCQ.<11>.IS.IS.is_pmc_dma", {{ BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8817ac ,6, 6}}}},
     {"DMA_RGF.DESCQ.<0>.RX_MAX_SIZE.MAX_RX_PL_PER_DESCRIPTOR.val", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881344 ,0, 15}}}},
     {"DMA_RGF.DESCQ.<1>.RX_MAX_SIZE.MAX_RX_PL_PER_DESCRIPTOR.val", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8813b4 ,0, 15}}}},
     {"DMA_RGF.DESCQ.<2>.RX_MAX_SIZE.MAX_RX_PL_PER_DESCRIPTOR.val", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881424 ,0, 15}}}},
     {"DMA_RGF.DESCQ.<3>.RX_MAX_SIZE.MAX_RX_PL_PER_DESCRIPTOR.val", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881494 ,0, 15}}}},
     {"DMA_RGF.DESCQ.<4>.RX_MAX_SIZE.MAX_RX_PL_PER_DESCRIPTOR.val", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881504 ,0, 15}}}},
     {"DMA_RGF.DESCQ.<5>.RX_MAX_SIZE.MAX_RX_PL_PER_DESCRIPTOR.val", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881574 ,0, 15}}}},
     {"DMA_RGF.DESCQ.<6>.RX_MAX_SIZE.MAX_RX_PL_PER_DESCRIPTOR.val", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8815e4 ,0, 15}}}},
     {"DMA_RGF.DESCQ.<7>.RX_MAX_SIZE.MAX_RX_PL_PER_DESCRIPTOR.val", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881654 ,0, 15}}}},
     {"DMA_RGF.DESCQ.<8>.RX_MAX_SIZE.MAX_RX_PL_PER_DESCRIPTOR.val", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8816c4 ,0, 15}}}},
     {"DMA_RGF.DESCQ.<9>.RX_MAX_SIZE.MAX_RX_PL_PER_DESCRIPTOR.val", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881734 ,0, 15}}}},
     {"DMA_RGF.DESCQ.<10>.RX_MAX_SIZE.MAX_RX_PL_PER_DESCRIPTOR.val", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8817a4 ,0, 15}}}},
     {"DMA_RGF.DESCQ.<11>.RX_MAX_SIZE.MAX_RX_PL_PER_DESCRIPTOR.val", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881814 ,0, 15}}}},
     {"DMA_RGF.DESCQ.<0>.SW_HEAD.CRNT", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881310 ,0, 31}}}},
     {"DMA_RGF.DESCQ.<1>.SW_HEAD.CRNT", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881380 ,0, 31}}}},
     {"DMA_RGF.DESCQ.<2>.SW_HEAD.CRNT", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8813f0 ,0, 31}}}},
     {"DMA_RGF.DESCQ.<3>.SW_HEAD.CRNT", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881460 ,0, 31}}}},
     {"DMA_RGF.DESCQ.<4>.SW_HEAD.CRNT", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8814d0 ,0, 31}}}},
     {"DMA_RGF.DESCQ.<5>.SW_HEAD.CRNT", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881540 ,0, 31}}}},
     {"DMA_RGF.DESCQ.<6>.SW_HEAD.CRNT", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8815b0 ,0, 31}}}},
     {"DMA_RGF.DESCQ.<7>.SW_HEAD.CRNT", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881620 ,0, 31}}}},
     {"DMA_RGF.DESCQ.<8>.SW_HEAD.CRNT", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881690 ,0, 31}}}},
     {"DMA_RGF.DESCQ.<9>.SW_HEAD.CRNT", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881700 ,0, 31}}}},
     {"DMA_RGF.DESCQ.<10>.SW_HEAD.CRNT", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x881770 ,0, 31}}}},
     {"DMA_RGF.DESCQ.<11>.SW_HEAD.CRNT", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x8817e0 ,0, 31}}}},
     {"MAC_EXT_RGF.MAC_SXD_EXT.PMC_UCODE_WR_RANGE.MAC_EXT_MAC_EXT_RGF_MAC_SXD_EXT_PMC_UCODE_WR_RANGE_0.sxd_pmc_start_1", { {BasebandRevisionEnum::TLN_M_B0, PmcRegisterInfo{0x88C1BC ,0, 7}}, {BasebandRevisionEnum::TLN_M_C0, PmcRegisterInfo{0x88C1BC ,0, 7}} }},
     {"MAC_EXT_RGF.MAC_SXD_EXT.PMC_UCODE_WR_RANGE.MAC_EXT_MAC_EXT_RGF_MAC_SXD_EXT_PMC_UCODE_WR_RANGE_0.sxd_pmc_end_1", { {BasebandRevisionEnum::TLN_M_B0, PmcRegisterInfo{0x88C1BC ,8, 15}}, {BasebandRevisionEnum::TLN_M_C0, PmcRegisterInfo{0x88C1BC ,8, 15}} }},
     {"MAC_EXT_RGF.MAC_SXD_EXT.PMC_UCODE_WR_RANGE.MAC_EXT_MAC_EXT_RGF_MAC_SXD_EXT_PMC_UCODE_WR_RANGE_0.sxd_pmc_start_2", { {BasebandRevisionEnum::TLN_M_B0, PmcRegisterInfo{0x88C1BC ,16, 23}}, {BasebandRevisionEnum::TLN_M_C0, PmcRegisterInfo{0x88C1BC ,16, 23}} }},
     {"MAC_EXT_RGF.MAC_SXD_EXT.PMC_UCODE_WR_RANGE.MAC_EXT_MAC_EXT_RGF_MAC_SXD_EXT_PMC_UCODE_WR_RANGE_0.sxd_pmc_end_2", { {BasebandRevisionEnum::TLN_M_B0, PmcRegisterInfo{0x88C1BC ,24, 31}}, {BasebandRevisionEnum::TLN_M_C0, PmcRegisterInfo{ 0x88C1BC ,24, 31}} }},
     {"MSXD_CMD.EXT_CONTROL.NID_UCODE_ACTIVE.MSXD_CMD_MSXD_CMD_EXT_CONTROL_NID_UCODE_ACTIVE_0.msrl_ucode_active_nid", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x180 ,0, 1}}}},
     {"MSXD_CMD.BI_TSF_CONTROL.BI_CONTROL.msrl_en_tsf_timer", {{BasebandRevisionEnum::BB_REV_UNKNOWN, PmcRegisterInfo{0x34 ,1, 1}}}},
 };

//registerName => { revision => register info }
PmcRegisterInfo PmcRegistersAccessor::GetPmcRegisterInfo(const std::string& registerName, const BasebandRevisionEnum deviceType) const
{
    auto registerNameIterator = s_pmcRegisterMap.find(registerName);
    if(registerNameIterator == s_pmcRegisterMap.end())
    {
        return PmcRegisterInfo();
    }
    auto revisionToRegInfoMap = registerNameIterator->second;
    auto revisionIterator = revisionToRegInfoMap.find(deviceType);
    if(!(revisionIterator == revisionToRegInfoMap.end()))
    {
        return revisionIterator->second;
    }
    auto unknownRevisionIterator = revisionToRegInfoMap.find(BasebandRevisionEnum::BB_REV_UNKNOWN);
    if(!(unknownRevisionIterator == revisionToRegInfoMap.end()))
    {
        return unknownRevisionIterator->second;
    }
    return PmcRegisterInfo();
}

OperationStatus PmcRegistersAccessor::ReadRegister(const std::string& deviceName, uint32_t address, uint32_t& value) const
{
    DWORD result = 0;
    DeviceManagerOperationStatus status = Host::GetHost().GetDeviceManager().Read(deviceName, address, result);
    value = result;

    switch (status)
    {
       case dmosSuccess:
       {
           return OperationStatus(true);
       }
       case dmosSilentDevice:
       {
           return OperationStatus(false, "Cannot read from a silent device");
       }
       case dmosNoSuchConnectedDevice:
       {
           return OperationStatus(false, "No device found");
       }
       default:
       {
           std::stringstream ss;
           ss << "Cannot read from the device when trying to read the address " << Address(address);
           return OperationStatus(false, ss.str());
       }
    }
}

OperationStatus PmcRegistersAccessor::WriteRegister(const std::string& deviceName, uint32_t address, uint32_t value) const
{
    DeviceManagerOperationStatus status = Host::GetHost().GetDeviceManager().Write(deviceName, address, value);
    switch (status)
    {
       case dmosSuccess:
       {
           return OperationStatus(true);
       }
       case dmosSilentDevice:
       {
           return OperationStatus(false, "Cannot write to a silent device");
       }
       case dmosNoSuchConnectedDevice:
       {
           return OperationStatus(false, "No device found");
       }
       default:
       {
           std::stringstream ss;
           ss << "Cannot write to the device when trying to write to address " << Address(address);
           return OperationStatus(false, ss.str());
       }
    }
}

OperationStatus PmcRegistersAccessor::WritePmcRegister(const std::string& deviceName, const std::string& registerName, uint32_t value) const
{
    BasebandRevisionEnum revision = PmcSequence::GetDeviceRevision(deviceName);
    PmcRegisterInfo registerInfo = GetPmcRegisterInfo(registerName, revision);
    if(!registerInfo.IsValid())
    {
        return OperationStatus(false, "Failed retrieving register info for register " + registerName);
    }
    uint32_t regValue = 0;
    OperationStatus statusRead = ReadRegister(deviceName, registerInfo.GetRegisterAddress(), regValue);
    if(!statusRead.IsSuccess())
    {
        LOG_ERROR << statusRead;
        return statusRead;
    }
    uint32_t dataToWrite = WriteToBitMask(regValue, registerInfo.GetBitStart(), registerInfo.GetBitEnd() - registerInfo.GetBitStart() + 1, value);
    OperationStatus statusWrite = WriteRegister(deviceName, registerInfo.GetRegisterAddress(), dataToWrite);
    if(!statusWrite.IsSuccess())
    {
        LOG_ERROR << statusWrite;
        return statusWrite;
    }
    return OperationStatus(true);
}

OperationStatus PmcRegistersAccessor::ReadPmcRegister(const std::string& deviceName, const std::string& registerName, uint32_t& registerValue) const
{
    BasebandRevisionEnum revision = PmcSequence::GetDeviceRevision(deviceName);
    PmcRegisterInfo registerInfo = GetPmcRegisterInfo(registerName, revision);
    if(!registerInfo.IsValid())
    {
        return OperationStatus(false, "Failed retrieving register info for register " + registerName);
    }
    uint32_t temp;
    OperationStatus res = ReadRegister(deviceName, registerInfo.GetRegisterAddress(), temp);
    if(!res.IsSuccess())
    {
        LOG_ERROR << res;
        return res;
    }
    registerValue = ReadFromBitMask(temp, registerInfo.GetBitStart(), registerInfo.GetBitEnd() - registerInfo.GetBitStart() + 1);
    return OperationStatus(true);
}

uint32_t PmcRegistersAccessor::WriteToBitMask(uint32_t dataBufferToWriteTo, uint32_t index, uint32_t size, uint32_t valueToWriteInData) const
{
    return (dataBufferToWriteTo & (~GetBitMask(index, size))) | (valueToWriteInData << index);
}

uint32_t PmcRegistersAccessor::GetBitMask(uint32_t index, uint32_t size) const
{
    if(size == 32)
    {
        return 0xFFFFFFFF << index;
    }
    return ((1 << (size)) - 1) << index;
}

uint32_t PmcRegistersAccessor::ReadFromBitMask(uint32_t data, uint32_t index, uint32_t size) const
{
    return (data & GetBitMask(index, size)) >> index;
}
