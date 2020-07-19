/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * Notifications and licenses are retained for attribution purposes only.
 */
/*
 * Copyright (c) 2002-2005 Sam Leffler, Errno Consulting
 * Copyright (c) 2002-2005 Atheros Communications, Inc.
 * Copyright (c) 2008-2010, Atheros Communications Inc. 
 * 
 * Redistribution and use in source and binary forms are permitted
 * provided that the following conditions are met:
 * 1. The materials contained herein are unmodified and are used
 *    unmodified.
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following NO
 *    ''WARRANTY'' disclaimer below (''Disclaimer''), without
 *    modification.
 * 3. Redistributions in binary form must reproduce at minimum a
 *    disclaimer similar to the Disclaimer below and any redistribution
 *    must be conditioned upon including a substantially similar
 *    Disclaimer requirement for further binary redistribution.
 * 4. Neither the names of the above-listed copyright holders nor the
 *    names of any contributors may be used to endorse or promote
 *    product derived from this software without specific prior written
 *    permission.
 * 
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ''AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT,
 * MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE
 * FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGES.
 * 
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5416

/*
 * Make sure the compilation supports either an RF chip
 * or a SoC with integrated RF.
 */
#if !defined(AH_SUPPORT_2133)       && \
    !defined(AH_SUPPORT_2122)       && \
    !defined(AH_SUPPORT_5133)       && \
    !defined(AH_SUPPORT_5122)       && \
    !defined(AH_SUPPORT_MERLIN_ANY) && \
    !defined(AH_SUPPORT_KITE_ANY)
#error "No 5416 RF support defined"
#endif

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"
#include "ah_desc.h"

#include "ar5416/ar5416.h"
#include "ar5416/ar5416desc.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"

/* Add static register initialization vectors */
#define AH_5416_COMMON
#define AH_5416_2133

/* static inits for Merlin (AR928x, AR922x) regs, if Merlin is supported */
#if defined(AH_SUPPORT_MERLIN_10)  /* just revs 1.0 up to (not incl.) 2.0 */ \
 || defined(AH_SUPPORT_MERLIN_ALL) /* all revisions */
    #include "ar5416/ar9280.ini"
#endif
#if defined(AH_SUPPORT_MERLIN_20)  /* just revisions 2.0 and after */ \
 || defined(AH_SUPPORT_MERLIN_ALL) /* all revisions */ \
 || defined(AH_SUPPORT_MERLIN)     /* the lastest revision */
    #include "ar5416/ar9280_merlin2.ini"
#endif

/* static inits for Sowl (AR9160, AR9161) regs, if Sowl is supported */
#if defined(AH_SUPPORT_SOWL)
    #include "ar5416/ar5416_sowl.ini"
#endif

/* static inits for Kiwi (AR9287) registers, if Kiwi is supported */
#if defined(AH_SUPPORT_KIWI_10)  /* just revision 1.0 */ \
 || defined(AH_SUPPORT_KIWI_ALL) /* all revisions */
     #include "ar5416/ar9287.ini"
#endif
#if defined(AH_SUPPORT_KIWI_11)  /* just revisions 1.1 and after */ \
 || defined(AH_SUPPORT_KIWI_ALL) /* all revisions */ \
 || defined(AH_SUPPORT_KIWI)     /* the latest revision */
    #include "ar5416/ar9287_1_1.ini"
#endif

/* static inits for Kite (AR9285) registers, if Kite is supported */
#if defined(AH_SUPPORT_KITE_10)  /* just revisions 1.0 and 1.1 */ \
 || defined(AH_SUPPORT_KITE_ALL) /* all revisions */
    #include "ar5416/ar9285.ini"
#endif
#if defined(AH_SUPPORT_KITE_12)  /* just revisions 1.2 and after */ \
 || defined(AH_SUPPORT_KITE_ALL) /* all revisions */ \
 || defined(AH_SUPPORT_KITE)     /* the latest revision */
    #include "ar5416/ar9285_v1_2.ini"
#endif

/* static inits for K2 (AR9271) registers, if K2 is supported */
#if defined(AH_SUPPORT_K2)
    #include "ar9271.ini"
#endif

/* static inits for Owl / Howl registers, if Owl or Howl is supported */
#if defined(AH_SUPPORT_HOWL)
    #include "ar5416/ar5416_howl.ini"
#elif defined(AH_SUPPORT_OWL)
    #include "ar5416/ar5416.ini"
#endif

static HAL_BOOL ar5416GetChipPowerLimits(struct ath_hal *ah,
        HAL_CHANNEL *chans, u_int32_t nchans);

static inline void ar5416AniSetup(struct ath_hal *ah);
static inline int ar5416GetRadioRev(struct ath_hal *ah);
static inline HAL_STATUS ar5416RfAttach(struct ath_hal *ah);
static inline HAL_STATUS ar5416InitMacAddr(struct ath_hal *ah);
static inline HAL_STATUS ar5416HwAttach(struct ath_hal *ah);
static inline void ar5416HwDetach(struct ath_hal *ah);
static int16_t ar5416GetNfAdjust(struct ath_hal *ah, const HAL_CHANNEL_INTERNAL *c);
static int ar5416GetCalIntervals(struct ath_hal *ah, HAL_CALIBRATION_TIMER **timerp, HAL_CAL_QUERY query);
void ar5416ClearMibCounters(struct ath_hal *ah);

static void
ar5416DisablePciePhy(struct ath_hal *ah);
#ifdef ATH_CCX
static HAL_BOOL
ar5416RecordSerialNumber(struct ath_hal *ah);
#endif

const HAL_PERCAL_DATA iq_cal_multi_sample =
                          {IQ_MISMATCH_CAL,
                          MAX_CAL_SAMPLES,
                          PER_MIN_LOG_COUNT,
                          ar5416IQCalCollect,
                          ar5416IQCalibration};
const HAL_PERCAL_DATA iq_cal_single_sample =
                          {IQ_MISMATCH_CAL,
                          MIN_CAL_SAMPLES,
                          PER_MAX_LOG_COUNT,
                          ar5416IQCalCollect,
                          ar5416IQCalibration};
const HAL_PERCAL_DATA adc_gain_cal_multi_sample =
                                {ADC_GAIN_CAL,
                                MAX_CAL_SAMPLES,
                                PER_MIN_LOG_COUNT,
                                ar5416AdcGainCalCollect,
                                ar5416AdcGainCalibration};
const HAL_PERCAL_DATA adc_gain_cal_single_sample =
                                {ADC_GAIN_CAL,
                                MIN_CAL_SAMPLES,
                                PER_MAX_LOG_COUNT,
                                ar5416AdcGainCalCollect,
                                ar5416AdcGainCalibration};
const HAL_PERCAL_DATA adc_dc_cal_multi_sample =
                              {ADC_DC_CAL,
                              MAX_CAL_SAMPLES,
                              PER_MIN_LOG_COUNT,
                              ar5416AdcDcCalCollect,
                              ar5416AdcDcCalibration};
const HAL_PERCAL_DATA adc_dc_cal_single_sample =
                              {ADC_DC_CAL,
                              MIN_CAL_SAMPLES,
                              PER_MAX_LOG_COUNT,
                              ar5416AdcDcCalCollect,
                              ar5416AdcDcCalibration};
const HAL_PERCAL_DATA adc_init_dc_cal =
                                   {ADC_DC_INIT_CAL,
                                   MIN_CAL_SAMPLES,
                                   INIT_LOG_COUNT,
                                   ar5416AdcDcCalCollect,
                                   ar5416AdcDcCalibration};
static HAL_CALIBRATION_TIMER ar5416_cals =
                                   {IQ_MISMATCH_CAL | ADC_DC_INIT_CAL |
                                    ADC_GAIN_CAL | ADC_DC_CAL,               // Cal type
                                    1200000,                                 // Cal interval
                                    0};                                      // Cal timestamp

/* WIN32 does not support C99 */
static const struct ath_hal_private ar5416hal = {
    {
        ar5416GetRateTable,
        ar5416Detach,

        /* Reset Functions */
        ar5416Reset,
        ar5416PhyDisable,
        ar5416Disable,
        ar5416ConfigPciPowerSave,
        ar5416SetPCUConfig,
        ar5416Calibration,
        ar5416ResetCalValid,
        ar5416SetTxPowerLimit,
        ar5416GetChipMinAndMaxPowers,  /* ah_get_chip_min_and_max_powers */
        ar5416FillHalChansFromRegdb,   /* ath_hal_fill_hal_chans_from_regdb */
		ar5416Setsc,					/* ath_hal_set_sc */
#if ATH_ANT_DIV_COMB
        ar5416_ant_ctrl_set_lna_div_use_bt_ant,       /* ah_ant_ctrl_set_lna_div_use_bt_ant */
#endif /* ATH_ANT_DIV_COMB */

#ifdef ATH_SUPPORT_DFS
        ar5416RadarWait,
        ar5416EnableDfs,
        ar5416GetDfsThresh,
        ar5416_adjust_difs,
        ar5416_dfs_config_fft,
        ar5416_dfs_cac_war,
        NULL, /* ah_cac_tx_quiet */
#endif
        ar5416GetExtensionChannel,
        ar5416IsFastClockEnabled,
        ar5416GetAHDevid,


        /* Transmit functions */
        ar5416UpdateTxTrigLevel,
        ar5416GetTxTrigLevel,
        ar5416SetupTxQueue,
        ar5416SetTxQueueProps,
        ar5416GetTxQueueProps,
        ar5416ReleaseTxQueue,
        ar5416ResetTxQueue,
        ar5416GetTxDP,
        ar5416SetTxDP,
        ar5416NumTxPending,
        ar5416StartTxDma,
        ar5416StopTxDma,
        ar5416StopTxDma, /*ah_stop_tx_dma and ah_stop_tx_dma_indv_que are same for ar5416 & ar5212 but different for ar9300 */
        ar5416AbortTxDma,
        ar5416FillTxDesc,
        ar5416SetDescLink,
        ar5416GetDescLinkPtr,
        ar5416ClearTxDescStatus,
#ifdef ATH_SWRETRY
        ar5416ClearDestMask,
#endif                
        ar5416ProcTxDesc,
        AH_NULL,
        AH_NULL,
        ar5416GetTxIntrQueue,
        ar5416TxReqIntrDesc,
        ar5416CalcTxAirtime,
        AH_NULL,

        /* RX Functions */
        ar5416GetRxDP,
        ar5416SetRxDP,
        ar5416EnableReceive,
        ar5416StopDmaReceive,
        ar5416StartPcuReceive,
        ar5416StopPcuReceive,
        ar5416SetMulticastFilter,
        ar5416GetRxFilter,
        ar5416SetRxFilter,
        ar5416SetRxSelEvm,
        ar5416SetRxAbort,
        ar5416SetupRxDesc,
        ar5416ProcRxDesc,
        ar5416GetRxKeyIdx,
        ar5416ProcRxDescFast,
        ar5416AniArPoll,
        ar5416ProcessMibIntr,

        /* Misc Functions */
        ar5416GetCapability,
        ar5416SetCapability,
        ar5416GetDiagState,
        ar5416GetMacAddress,
        ar5416SetMacAddress,
        ar5416GetBssIdMask,
        ar5416SetBssIdMask,
        ar5416SetRegulatoryDomain,
        ar5416SetLedState,
        ar5416SetPowerLedState,
        ar5416SetNetworkLedState,
        ar5416WriteAssocid,
        ar5416ForceTSFSync,
        ar5416GpioCfgInput,
        ar5416GpioCfgOutput,
        ar5416GpioCfgOutputLEDoff,
        ar5416GpioGet,
        ar5416GpioSet,
        AH_NULL, /* ah_gpio_get_intr */
        ar5416GpioSetIntr,
        AH_NULL, /* ah_gpio_get_polarity */
        AH_NULL, /* ah_gpio_set_polarity */
        AH_NULL, /* ah_gpio_get_mask */
        AH_NULL, /* ah_gpio_set_mask */
        ar5416GetTsf32,
        ar5416GetTsf64,
        ar5416GetTsf2_32,
        ar5416ResetTsf,
        ar5416DetectCardPresent,
        ar5416UpdateMibMacStats,
        ar5416GetMibMacStats,
        ar5416GetRfgain,
        ar5416GetDefAntenna,
        ar5416SetDefAntenna,
        ar5416SetSlotTime,
        ar5416SetAckTimeout,
        ar5416GetAckTimeout,
        ar5416SetCoverageClass,
        ar5416SetQuiet,
        ar5416SetAntennaSwitch,
        ar5416GetDescInfo,
        ar5416SelectAntConfig,
        NULL,
        NULL,                       /* ah_ant_swcom_sel */
        ar5416EnableTPC,
        ar5416OpenLoopPowerControlTempCompensation, /* ah_olpcTempCompensation */
        ar5416DisablePhyRestart,   /* ah_disable_phy_restart */
        ar5416_enable_keysearch_always,
        ar5416InterferenceIsPresent,
        AH_NULL,                  /* ah_disp_tpc_tables */
        AH_NULL,                    /* ah_get_tpc_tables */
        /* Key Cache Functions */
        ar5416GetKeyCacheSize,
        ar5416ResetKeyCacheEntry,
        ar5416IsKeyCacheEntryValid,
        ar5416SetKeyCacheEntry,
        ar5416SetKeyCacheEntryMac,
        AH_NULL,
#if ATH_SUPPORT_KEYPLUMB_WAR
        ar5416CheckKeyCacheEntry,
#endif
        /* Power Management Functions */
        ar5416SetPowerMode,
        ar5416SetSmPowerMode,
#if ATH_WOW        
        ar5416WowApplyPattern,
        ar5416WowEnable,
        ar5416WowWakeUp,
#if ATH_WOW_OFFLOAD
        NULL, /* ah_wow_offload_prep */
        NULL, /* ah_wow_offload_post */
        NULL, /* ah_wow_offload_download_rekey_data */
        NULL, /* ah_wow_offload_retrieve_data */
        NULL, /* ah_wow_offload_download_acer_magic */
        NULL, /* ah_wow_offload_download_acer_swka */
        NULL, /* ah_wow_offload_download_arp_info */
        NULL, /* ah_wow_offload_download_ns_info */
#endif /* ATH_WOW_OFFLOAD */
#endif        

        /* Get Channel Noise */
        ath_hal_get_chan_noise,
        ar5416ChainNoiseFloor,
        NULL,                   /* ah_get_nf_from_reg */
        NULL,			/* ah_get_rx_nf_offset, not supported by 5416 */

        /* Beacon Functions */
        ar5416BeaconInit,
        ar5416SetStaBeaconTimers,

        /* Interrupt Functions */
        ar5416IsInterruptPending,
        ar5416GetPendingInterrupts,
        ar5416GetInterrupts,
        ar5416SetInterrupts,
        ar5416SetIntrMitigationTimer,
        ar5416GetIntrMitigationTimer,
#if ATH_SUPPORT_WIFIPOS
        NULL, /* ah_read_loc_timer_reg */
        NULL, /* ah_get_eeprom_chain_mask */
#endif
        ar5416ForceVCS,
        ar5416SetDfs3StreamFix,
        ar5416Get3StreamSignature,

        /* 11n specific functions (NOT applicable to ar5416) */
        ar5416Set11nTxDesc,
#if ATH_SUPPORT_WIFIPOS
        NULL, /* ah_set_rx_chainmask */
        NULL, /* ah_update_loc_ctl_reg */
#endif 
        /* Start PAPRD functions - supported in ar9300 onwards */
        NULL, /* ah_setPAPRDTxDesc */
        NULL, /* ah_PAPRDInitTable */
        NULL, /* ah_PAPRDSetupGainTable */
        NULL, /* ah_PAPRDCreateCurve */
        NULL, /* ah_PAPRDisDone */
        NULL, /* ah_PAPRDEnable */
        NULL, /* ah_PAPRDPopulateTable */
        NULL, /* ah_isTxDone */
        ar5416_paprd_dec_tx_pwr, /* ah_paprd_dec_tx_pwr */
        NULL, /* ah_paprd_thermal_send */
        /* End PAPRD functions */
        ar5416_factory_defaults, /* ah_factory_defaults */
#ifdef ATH_SUPPORT_TxBF
        /*for TxBF*/
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL, /* ah_get_perrate_txbfflags */
#endif
        ar5416Set11nRateScenario,
        ar5416Set11nAggrFirst,
        ar5416Set11nAggrMiddle,
        ar5416Set11nAggrLast,
        ar5416Clr11nAggr,
        ar5416Set11nRifsBurstMiddle,
        ar5416Set11nRifsBurstLast,
        ar5416Clr11nRifsBurst,
        ar5416Set11nAggrRifsBurst,
        ar5416Set11nRxRifs,
        AH_NULL,
        ar5416DetectBbHang,
        ar5416DetectMacHang,
        ar5416SetImmunity,
        ar5416GetHwHangs,
        ar5416Set11nBurstDuration,
        ar5416Set11nVirtualMoreFrag,
        ar5416Get11nExtBusy,
        ar5416GetChBusyPct,
        ar5416Set11nMac2040,
        ar5416Get11nRxClear,
        ar5416Set11nRxClear,
        ar5416GetMibCycleCountsPct,
        ar5416DmaRegDump,

        /* ForcePPM specific functions */
        ar5416PpmGetRssiDump,
        ar5416PpmArmTrigger,
        ar5416PpmGetTrigger,
        ar5416PpmForce,
        ar5416PpmUnForce,
        ar5416PpmGetForceState,

        ar5416_getSpurInfo,
        ar5416_setSpurInfo,

        ar5416GetMinCCAPwr,

        ar5416GreenApPsOnOff,
        ar5416IsSingleAntPowerSavePossible,

        /* radio measurement specific functions */
        ar5416GetMibCycleCounts,
        ar5416GetVowStats, /* ah_get_vow_stats */
        ar5416ClearMibCounters,
#ifdef ATH_CCX
        ar5416GetCcaThreshold,
        ar5416GetCurRssi,
#endif
#if ATH_GEN_RANDOMNESS
        ar5416GetCurRssi, /* ah_get_rssi_chain0 */
#endif
#ifdef ATH_BT_COEX
        /* Bluetooth Coexistence functions */
        ar5416SetBTCoexInfo,
        ar5416BTCoexConfig,
        ar5416BTCoexSetQcuThresh,
        ar5416BTCoexSetWeights,
        ar5416BTCoexSetupBmissThresh,
        ar5416BTCoexSetParameter,
        ar5416BTCoexDisable,
        ar5416BTCoexEnable,
        ar5416GetBTActiveGpio,
		ar5416GetWlanActiveGpio,
#endif
        /* Generic Timer functions */
        ar5416AllocGenericTimer,
        ar5416FreeGenericTimer,
        ar5416StartGenericTimer,
        ar5416StopGenericTimer,
        ar5416GetGenTimerInterrupts,

        ar5416SetDcsMode,
        ar5416GetDcsMode,

#ifdef ATH_ANT_DIV_COMB
        ar5416AntDivCombGetConfig,  /* ah_get_ant_dvi_comb_conf */
        ar5416AntDivCombSetConfig,  /* ah_set_ant_dvi_comb_conf */
#endif
        AH_NULL, /* ah_print_bb_panic_info */
        AH_NULL, /* ah_handle_radar_bb_panic */
        AH_NULL, /* ah_set_hal_reset_reason */
 
#if ATH_PCIE_ERROR_MONITOR
        AH_NULL,        /* ah_start_pcie_error_monitor */
        AH_NULL,        /* ah_read_pcie_error_monitor*/
        AH_NULL,        /* ah_stop_pcie_error_monitor*/
#endif //ATH_PCIE_ERROR_MONITOR

#if WLAN_SPECTRAL_ENABLE
        /* Spectral scan */
        ar5416ConfigureSpectralScan,
        ar5416GetSpectralParams,
        ar5416StartSpectralScan,
        ar5416StopSpectralScan,
        ar5416IsSpectralEnabled,
        ar5416IsSpectralActive,
        ar5416GetCtlChanNF,
        ar5416GetExtChanNF,
        ar5416GetNominalNF,
#endif  /*  WLAN_SPECTRAL_ENABLE */

#if ATH_SUPPORT_RAW_ADC_CAPTURE
        ar5416EnableTestAddacMode,   /* ah_arEnableTestAddacMode */ 
        ar5416DisableTestAddacMode,  /* ah_arDisableTestAddacMode */ 
        ar5416BeginAdcCapture,       /* ah_arBeginAdcCapture */ 
        ar5416RetrieveCaptureData,   /* ah_arRetrieveCaptureData */ 
        ar5416CalculateADCRefPowers, /* ah_arCalculateADCRefPowers */
        ar5416GetMinAGCGain,         /* ah_arGetMinAGCGain */
#endif

        ar5416PromiscMode,
        ar5416ReadPktlogReg,
        ar5416WritePktlogReg,
    ar5416SetProxySTA,          /* ah_setProxySTA */
        ar5416GetCalIntervals,
#if ATH_SUPPORT_WIRESHARK
        ar5416FillRadiotapHdr,
#endif
#if ATH_TRAFFIC_FAST_RECOVER
        AH_NULL,                /* ah_get_pll3_sqsum_dvc */
#endif

#ifdef ATH_TX99_DIAG
        /* Tx99 functions */
        AH_NULL,
        AH_NULL,
        ar5416_tx99_channel_pwr_update,
        ar5416_tx99_start,
        ar5416_tx99_stop,
        ar5416_tx99_chainmsk_setup,
        ar5416_tx99_set_single_carrier,
#endif
        ar5416ChkRSSIUpdateTxPwr,        
        ar5416_is_skip_paprd_by_greentx,   /* ah_is_skip_paprd_by_greentx */
        AH_NULL,                           /* ah_hwgreentx_set_pal_spare */
#if ATH_SUPPORT_MCI
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
        AH_NULL,
#endif
        AH_NULL,                           /* ah_reset_hw_beacon_proc_crc */
        AH_NULL,                           /* ah_get_hw_beacon_rssi */
        AH_NULL,                           /* ah_set_hw_beacon_rssi_config */
        AH_NULL,                           /* ah_reset_hw_beacon_rssi */
        AH_NULL,                           /* ah_mat_enable */
        AH_NULL,                           /* ah_dump_keycache */
        ar5416IsAniNoiseSpur,              /* ah_is_ani_noise_spur */
#if ATH_SUPPORT_WIFIPOS
        AH_NULL,                            /* ah_lean_channel_change */
        AH_NULL,                            /* ah_enable_hwq */
#endif 
        AH_NULL,                           /* ah_set_hw_beacon_proc */
        AH_NULL,                            /* ah_set_ctl_pwr */ 
        AH_NULL,                            /* ah_set_txchainmaskopt */
		ar5416_reset_nav,					/* ah_reset_nav */
        AH_NULL,                           /* ah_get_smart_ant_tx_info */
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
        ar5416_txbf_loforceon_update,       /* ah_txbf_loforceon_update */
#endif
        AH_NULL,                           /* ah_disable_interrupts_on_exit */
#if ATH_GEN_RANDOMNESS
        AH_NULL,            /* ahrng_data_read*/
#endif
#if ATH_BT_COEX_3WIRE_MODE
        AH_NULL,
        AH_NULL,
#endif
    },

    ar5416GetChannelEdges,
    ar5416GetWirelessModes,
    ar5416EepromRead,
#ifdef AH_SUPPORT_WRITE_EEPROM
    ar5416EepromWrite,
#else
    AH_NULL,
#endif
    ar5416EepromDumpSupport,
    ar5416GetChipPowerLimits,
    ar5416GetNfAdjust,

    /* rest is zero'd by compiler */
};

/*
 * Read MAC version/revision information from Chip registers and initialize
 * local data structures.
 */
void
ar5416ReadRevisions(struct ath_hal *ah)
{
    u_int32_t val;

    val = OS_REG_READ(ah, AR_SREV) & AR_SREV_ID;

    if (val == 0xFF) {
        /* new SREV format for Sowl and later */
        val = OS_REG_READ(ah, AR_SREV);

        /* Include 6-bit Chip Type (masked to 0) to differentiate from pre-Sowl versions */
        AH_PRIVATE(ah)->ah_mac_version = (val & AR_SREV_VERSION2) >> AR_SREV_TYPE2_S;
        
        AH_PRIVATE(ah)->ah_mac_rev = MS(val, AR_SREV_REVISION2);
        AH_PRIVATE(ah)->ah_is_pci_express = (val & AR_SREV_TYPE2_HOST_MODE) ? 0 : 1;

    } else {
        if (!AR_SREV_HOWL(ah)) /* Reads val = 0 for HOWL */
            AH_PRIVATE(ah)->ah_mac_version = MS(val, AR_SREV_VERSION);
        
        AH_PRIVATE(ah)->ah_mac_rev = val & AR_SREV_REVISION;

        if (AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_OWL_PCIE) {
            AH_PRIVATE(ah)->ah_is_pci_express = AH_TRUE;
        }
    }
}

/*
 * Attach for an AR5416 part.
 */
struct ath_hal *
ar5416Attach(u_int16_t devid, HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
    HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype,
    struct hal_reg_parm *hal_conf_parm, HAL_STATUS *status)
{
    struct ath_hal_5416     *ahp;
    struct ath_hal          *ah;
    struct ath_hal_private  *ahpriv;
    HAL_STATUS              ecode;
    u_int32_t               fifo_threshold;

    HAL_USE_INTERSPERSED_READS;

    /* NB: memory is returned zero'd */
    ahp = ar5416NewState(
        devid, osdev, sc, st, sh, bustype, hal_conf_parm, status);
    if (ahp == AH_NULL) return AH_NULL;
    ah = &ahp->ah_priv.priv.h;
    ahpriv = AH_PRIVATE(ah);
#if AH_REGREAD_DEBUG
/* clean up the buffer to record the register reads */
    OS_MEMZERO(ahpriv->ah_regaccess, 8192*sizeof(u_int32_t)); 
    ahpriv->ah_regaccessbase = 0;
    ahpriv->ah_regtsf_start = 0;
#endif

    /* interrupt mitigation hardware is only available on OWL and after */
    if (ahpriv->ah_config.ath_hal_intr_mitigation_rx != 0) {
        ahp->ah_intrMitigationRx = AH_TRUE;
    }
    if (ahpriv->ah_config.ath_hal_intr_mitigation_tx != 0) {
        ahp->ah_intrMitigationTx = AH_TRUE;
    }

    if (!ar5416SetResetReg(ah, HAL_RESET_POWER_ON)) {    /* reset chip */
        HDPRINTF(ah, HAL_DBG_RESET, "%s: couldn't reset chip\n", __func__);
        ecode = HAL_EIO;
        goto bad;
    }

    ar5416InitPLL(ah, AH_NULL);

    if (!ar5416SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: couldn't wakeup chip\n", __func__);
        ecode = HAL_EIO;
        goto bad;
    }

    /* Serialization of Register Accesses:
     *
     * Owl (ar5416) has some issues with platforms that do not correctly
     * serialize PCI register accesses. We also see this issue on Merlin
     * PCI-based cards (see ExtraView bug # 52527).
     *
     * Enable serialization of register accesses when the following are true:
     *      - device requires serialization (e.g. Owl 2.2 and PCI/miniPCI)
     *      - Multiprocessor/HT system
     *
     * See bug 21930 and 32666 for more information.
     * Note: register accesses are serialized using the interrupt spinlock.
     *       This means that all register accesses will not be serialized until
     *       interrupts are registered.  This should be fine as only one driver thread 
     *       should be running during intialization.
     */
    if (ahpriv->ah_config.ath_hal_serialize_reg_mode == SER_REG_MODE_AUTO) {
        if ((ahpriv->ah_mac_version == AR_SREV_VERSION_OWL_PCI) || 
            (AR_SREV_MERLIN(ah) && (ahpriv->ah_is_pci_express == AH_FALSE))) {
            /* Enabled this feature when the hardware needs this workaround. */
            ahpriv->ah_config.ath_hal_serialize_reg_mode = SER_REG_MODE_ON;
        } else {
            ahpriv->ah_config.ath_hal_serialize_reg_mode = SER_REG_MODE_OFF;
        }
    }
    HDPRINTF(ah, HAL_DBG_RESET, "%s: ath_hal_serialize_reg_mode is %d\n",
             __func__, ahpriv->ah_config.ath_hal_serialize_reg_mode);

    /* add mac revision check when needed */
    if (ahpriv->ah_mac_version != AR_SREV_VERSION_OWL_PCI &&
        ahpriv->ah_mac_version != AR_SREV_VERSION_OWL_PCIE && 
        ahpriv->ah_mac_version != AR_SREV_VERSION_SOWL &&
        (!AR_SREV_HOWL(ah)) && (!AR_SREV_MERLIN(ah)) &&
        (!AR_SREV_KITE(ah)) && (!AR_SREV_K2(ah)) && (!AR_SREV_KIWI(ah))) {
        HDPRINTF(ah, HAL_DBG_RESET,
            "%s: Mac Chip Rev 0x%02x.%x is not supported by this driver\n",
            __func__, ahpriv->ah_mac_version, ahpriv->ah_mac_rev);
        ecode = HAL_ENOTSUPP;
        goto bad;
    }

    if (AR_SREV_HOWL(ah)) {
        /* Setup supported calibrations */
        ahp->ah_iqCalData.calData = &iq_cal_multi_sample;
        ahp->ah_suppCals = IQ_MISMATCH_CAL;
        ahpriv->ah_is_pci_express = AH_FALSE;
    }
    if (AR_SREV_K2(ah)) {
        ahpriv->ah_is_pci_express = AH_FALSE;
    }
    ahpriv->ah_phy_rev = OS_REG_READ(ah, AR_PHY_CHIP_ID);

    if (AR_SREV_SOWL_10_OR_LATER(ah)) {
        /* Setup supported calibrations */
        if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
            ahp->ah_iqCalData.calData = &iq_cal_single_sample;
            ahp->ah_adcGainCalData.calData = &adc_gain_cal_single_sample;
            ahp->ah_adcDcCalData.calData = &adc_dc_cal_single_sample;
            ahp->ah_adcDcCalInitData.calData = &adc_init_dc_cal;
        } else {
            ahp->ah_iqCalData.calData = &iq_cal_multi_sample;
            ahp->ah_adcGainCalData.calData = &adc_gain_cal_multi_sample;
            ahp->ah_adcDcCalData.calData = &adc_dc_cal_multi_sample;
            ahp->ah_adcDcCalInitData.calData = &adc_init_dc_cal;
        }
        ahp->ah_suppCals = ADC_GAIN_CAL | ADC_DC_CAL | IQ_MISMATCH_CAL;

        /* Per EV74509, better performance by removing ADC_GAIN_CAL */
        if (AR_SREV_KIWI(ah))
            ahp->ah_suppCals &= ~(ADC_GAIN_CAL);
    }

    if (AR_SREV_SOWL(ah) || AR_SREV_HOWL(ah)) {
        ahp->ah_rifs_enabled = AH_TRUE;
    }

    ahp->ah_ani_function = HAL_ANI_ALL;
    if (AR_SREV_HOWL(ah)) {
        ahp->ah_ani_function = 0;
    } else if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        /* For reliable RIFS rx, disable ANI noise immunity */
        ahp->ah_ani_function &= ~HAL_ANI_NOISE_IMMUNITY_LEVEL;
    }

    HDPRINTF(ah, HAL_DBG_RESET, "%s: This Mac Chip Rev 0x%02x.%x is\n",
        __func__, ahpriv->ah_mac_version, ahpriv->ah_mac_rev);

    /* Support for ar5416 multiple INIs */
    if (AR_SREV_K2(ah)) {
        HDPRINTF(ah, HAL_DBG_RESET, "Ar5416Attach : K2\n");
#if defined(AH_SUPPORT_K2)
	    INIT_INI_ARRAY(&ahp->ah_iniModes, ar9271Modes_K2, 6);
	    INIT_INI_ARRAY(&ahp->ah_iniCommon, ar9271Common_K2, 2);
	    INIT_INI_ARRAY(&ahp->ah_iniCommon_normal_cck_fir_coeff_K2, ar9271Common_normal_cck_fir_coeff_K2, 2);
	    INIT_INI_ARRAY(&ahp->ah_iniCommon_japan_2484_cck_fir_coeff_K2, ar9271Common_japan_2484_cck_fir_coeff_K2, 2);
	    INIT_INI_ARRAY(&ahp->ah_iniModes_K2_1_0_only, ar9271Modes_K2_1_0_only, 6);
	    INIT_INI_ARRAY(&ahp->ah_iniModes_K2_ANI_reg, ar9271Modes_K2_ANI_reg, 6);
	    INIT_INI_ARRAY(&ahp->ah_iniModes_high_power_tx_gain_K2, ar9271Modes_high_power_tx_gain_K2, 6);
	    INIT_INI_ARRAY(&ahp->ah_iniModes_normal_power_tx_gain_K2, ar9271Modes_normal_power_tx_gain_K2, 6);
#endif /* AH_SUPPORT_K2 */
    } else if (AR_SREV_KIWI_11_OR_LATER(ah)) {
#if defined(AH_SUPPORT_KIWI)     /* latest rev */ || \
    defined(AH_SUPPORT_KIWI_ALL) /* all revs */   || \
    defined(AH_SUPPORT_KIWI_11)  /* rev 1.1+ */
        /* use the register initializations from ar9287_1_1.ini */
        INIT_INI_ARRAY(&ahp->ah_iniModes, ar9287Modes_kiwi1_1, 6);
        INIT_INI_ARRAY(&ahp->ah_iniCommon, ar9287Common_kiwi1_1, 2);

        /*
         * PCIE SERDES settings from INI,
         * initialize according to registry setting
         */
        if (ahpriv->ah_config.ath_hal_pcie_clock_req) {
            /* Shut off CLKREQ active in L1 */
            INIT_INI_ARRAY(
                &ahp->ah_iniPcieSerdes,
                ar9287PciePhy_clkreq_off_L1_kiwi1_1, 2);
        } else {
            INIT_INI_ARRAY(
                &ahp->ah_iniPcieSerdes,
                ar9287PciePhy_clkreq_always_on_L1_kiwi1_1, 2);
        }
#if ATH_WOW
        /* SerDes values during WOW sleep */
        INIT_INI_ARRAY(
            &ahp->ah_iniPcieSerdesWow, ar9287PciePhy_AWOW_kiwi1_1, 2);
#endif
#endif /* AH_SUPPORT_KIWI_11 */
    } else if (AR_SREV_KIWI_10_OR_LATER(ah)) {
#if defined(AH_SUPPORT_KIWI_ALL) /* all revs */ || \
    defined(AH_SUPPORT_KIWI_10)  /* rev 1.0) */
        /* use the register initializations from ar9287.ini */
        INIT_INI_ARRAY(&ahp->ah_iniModes, ar9287Modes_kiwi1_0, 6);
        INIT_INI_ARRAY(&ahp->ah_iniCommon, ar9287Common_kiwi1_0, 2);

        /*
         * PCIE SERDES settings from INI,
         * initialize according to registry setting
         */
        if (ahpriv->ah_config.ath_hal_pcie_clock_req) {
            /* Shut off CLKREQ active in L1 */
            INIT_INI_ARRAY(
                &ahp->ah_iniPcieSerdes,
                ar9287PciePhy_clkreq_off_L1_kiwi1_0, 2);
        } else {
            INIT_INI_ARRAY(
                &ahp->ah_iniPcieSerdes,
                ar9287PciePhy_clkreq_always_on_L1_kiwi1_0, 2);
        }
#if ATH_WOW
        /* SerDes values during WOW sleep */
        INIT_INI_ARRAY(
            &ahp->ah_iniPcieSerdesWow, ar9287PciePhy_AWOW_kiwi1_0, 2);
#endif
#endif /* AH_SUPPORT_KIWI_10 */
    } else if (AR_SREV_KITE_12_OR_LATER(ah)) {
#if defined(AH_SUPPORT_KITE)     /* latest rev */ || \
    defined(AH_SUPPORT_KITE_ALL) /* all revs */   || \
    defined(AH_SUPPORT_KITE_12)  /* rev 1.2+ */
        u_int32_t txgainType = ar5416EepromGet(ahp, EEP_TXGAIN_TYPE);
        /* use the register initializations from ar9285_v1_2.ini */
        INIT_INI_ARRAY(&ahp->ah_iniModes, ar9285Modes_kite1_2, 6);
        INIT_INI_ARRAY(&ahp->ah_iniCommon, ar9285Common_kite1_2, 2);

        /*
         * PCIE SERDES settings from INI,
         * initialize according to registry setting
         */
        if (ahpriv->ah_config.ath_hal_pcie_clock_req) {
            /* Shut off CLKREQ active in L1 */
            INIT_INI_ARRAY(
                &ahp->ah_iniPcieSerdes,
                ar9285PciePhy_clkreq_off_L1_kite1_2, 2);
        } else {
            INIT_INI_ARRAY(
                &ahp->ah_iniPcieSerdes,
                ar9285PciePhy_clkreq_always_on_L1_kite1_2, 2);
        }
#if ATH_WOW
        /* SerDes values during WOW sleep */
        INIT_INI_ARRAY(
            &ahp->ah_iniPcieSerdesWow, ar9285PciePhy_AWOW_kite1_2, 2);
#endif
        /* txgain table */
        if (txgainType == AR5416_EEP_TXGAIN_HIGH_POWER) {
            if (AR_SREV_ELIJAH_20(ah)) {
                INIT_INI_ARRAY(
                        &ahp->ah_iniModesTxgain,
                        ar9285Modes_Elijah2_0_high_power, 6);
            }
            else {            
                INIT_INI_ARRAY(
                        &ahp->ah_iniModesTxgain,
                        ar9285Modes_high_power_tx_gain_kite1_2, 6);
            }
        }
        else {
            if (AR_SREV_ELIJAH_20(ah)) {
                INIT_INI_ARRAY(
                        &ahp->ah_iniModesTxgain,
                        ar9285Modes_Elijah2_0_normal_power, 6);
            }
            else {
                INIT_INI_ARRAY(
                        &ahp->ah_iniModesTxgain,
                        ar9285Modes_original_tx_gain_kite1_2, 6);
            }
        }
#endif /* AH_SUPPORT_KITE_20 */
    } else if (AR_SREV_KITE_10_OR_LATER(ah)) {
#if defined(AH_SUPPORT_KITE_ALL) /* all revs */ || \
    defined(AH_SUPPORT_KITE_10)  /* rev range [1.0-1.1) */
        /* use the register initializations from ar9285.ini */
        INIT_INI_ARRAY(&ahp->ah_iniModes, ar9285Modes_kite, 6);
        INIT_INI_ARRAY(&ahp->ah_iniCommon, ar9285Common_kite, 2);

        /*
         * PCIE SERDES settings from INI,
         * initialize according to registry setting
         */
        if (ahpriv->ah_config.ath_hal_pcie_clock_req) {
            /* Shut off CLKREQ active in L1 */
            INIT_INI_ARRAY(
                &ahp->ah_iniPcieSerdes, ar9285PciePhy_clkreq_off_L1_kite, 2);
        } else {
            INIT_INI_ARRAY(
                &ahp->ah_iniPcieSerdes,
                ar9285PciePhy_clkreq_always_on_L1_kite, 2);
        }
#endif /* AH_SUPPORT_KITE_10 */
    } else if (AR_SREV_MERLIN_20_OR_LATER(ah)) {
#if defined(AH_SUPPORT_MERLIN)     /* latest rev */ || \
    defined(AH_SUPPORT_MERLIN_ALL) /* all revs */   || \
    defined(AH_SUPPORT_MERLIN_20)  /* rev 2.0+ */
        /* use the register initializations from ar9280_merlin2.ini */
        INIT_INI_ARRAY(&ahp->ah_iniModes, ar9280Modes_merlin2, 6);
        INIT_INI_ARRAY(&ahp->ah_iniCommon, ar9280Common_merlin2, 2);

        /*
         * PCIE SERDES settings from INI,
         * initialize according to registry setting
         */
        if (ahpriv->ah_config.ath_hal_pcie_clock_req) {
            /* Shut off CLKREQ active in L1 */
            INIT_INI_ARRAY(
                &ahp->ah_iniPcieSerdes, ar9280PciePhy_clkreq_off_L1_merlin, 2);
        } else {
            INIT_INI_ARRAY(
                &ahp->ah_iniPcieSerdes,
                ar9280PciePhy_clkreq_always_on_L1_merlin, 2);
        }
        /*
         * ath_hal_pcie_power_save_enable should be 2 for OWL/Condor
         * and 0 for merlin
         */
        ahpriv->ah_config.ath_hal_pcie_power_save_enable = 0;

#if ATH_WOW
        /* SerDes values during WOW sleep */
        INIT_INI_ARRAY(&ahp->ah_iniPcieSerdesWow, ar9280PciePhy_AWOW_merlin, 2);
#endif
        
        /* Fast clock modal settings */
        INIT_INI_ARRAY(
            &ahp->ah_iniModesAdditional, ar9280Modes_fast_clock_merlin2, 3);
#endif /* AH_SUPPORT_MERLIN_20 */
    } else if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
#if defined(AH_SUPPORT_MERLIN_ALL) /* any rev */ || \
    defined(AH_SUPPORT_MERLIN_10)  /* rev range [1.0-2.0) */
        /* use the register initializations from ar9280.ini */
        INIT_INI_ARRAY(&ahp->ah_iniModes, ar9280Modes_merlin, 6);
        INIT_INI_ARRAY(&ahp->ah_iniCommon, ar9280Common_merlin, 2);
#endif /* AH_SUPPORT_MERLIN_10 */
    } else if (AR_SREV_SOWL_10_OR_LATER(ah)) {
#ifdef AH_SUPPORT_SOWL
        /* use the register initializations from ar5416_sowl.ini */
        INIT_INI_ARRAY(&ahp->ah_iniModes, ar5416Modes_sowl, 6);
        INIT_INI_ARRAY(&ahp->ah_iniCommon, ar5416Common_sowl, 2);
        INIT_INI_ARRAY(&ahp->ah_iniBank0, ar5416Bank0_sowl, 2);
        INIT_INI_ARRAY(&ahp->ah_iniBB_RfGain, ar5416BB_RfGain_sowl, 3);
        INIT_INI_ARRAY(&ahp->ah_iniBank1, ar5416Bank1_sowl, 2);
        INIT_INI_ARRAY(&ahp->ah_iniBank2, ar5416Bank2_sowl, 2);
        INIT_INI_ARRAY(&ahp->ah_iniBank3, ar5416Bank3_sowl, 3);
        INIT_INI_ARRAY(&ahp->ah_iniBank6, ar5416Bank6_sowl, 3);
        INIT_INI_ARRAY(&ahp->ah_iniBank6TPC, ar5416Bank6TPC_sowl, 3);
        INIT_INI_ARRAY(&ahp->ah_iniBank7, ar5416Bank7_sowl, 2);
        if (AR_SREV_SOWL_11(ah))
            {INIT_INI_ARRAY(&ahp->ah_iniAddac, ar5416Addac_sowl1_1, 2);}
        else
            {INIT_INI_ARRAY(&ahp->ah_iniAddac, ar5416Addac_sowl, 2);}
#endif /* AH_SUPPORT_SOWL */
    } else {
#if defined(AH_SUPPORT_OWL) || defined(AH_SUPPORT_HOWL)
        INIT_INI_ARRAY(&ahp->ah_iniModes, ar5416Modes, 6);
        INIT_INI_ARRAY(&ahp->ah_iniCommon, ar5416Common, 2);
        INIT_INI_ARRAY(&ahp->ah_iniBank0, ar5416Bank0, 2);
        INIT_INI_ARRAY(&ahp->ah_iniBB_RfGain, ar5416BB_RfGain, 3);
        INIT_INI_ARRAY(&ahp->ah_iniBank1, ar5416Bank1, 2);
        INIT_INI_ARRAY(&ahp->ah_iniBank2, ar5416Bank2, 2);
        INIT_INI_ARRAY(&ahp->ah_iniBank3, ar5416Bank3, 3);
        INIT_INI_ARRAY(&ahp->ah_iniBank6, ar5416Bank6, 3);
        INIT_INI_ARRAY(&ahp->ah_iniBank6TPC, ar5416Bank6TPC, 3);
        INIT_INI_ARRAY(&ahp->ah_iniBank7, ar5416Bank7, 2);
        INIT_INI_ARRAY(&ahp->ah_iniAddac, ar5416Addac, 2);
#endif /* AH_SUPPORT_OWL || AH_SUPPORT_HOWL */
    }

    /* Configire PCIE after Ini init. SERDES values now come from ini file */
    if (ahpriv->ah_is_pci_express) {
        ar5416ConfigPciPowerSave(ah, 0, 0);
    } else {
        ar5416DisablePciePhy(ah);
    }

    /* Support for Japan ch.14 (2484) spread */
    if (AR_SREV_KIWI_11_OR_LATER(ah)) {
#if defined(AH_SUPPORT_KIWI)     /* latest rev */ || \
    defined(AH_SUPPORT_KIWI_ALL) /* all revs */   || \
    defined(AH_SUPPORT_KIWI_11)  /* rev 1.1+ */
        /* use the register initializations from ar9287_1_1.ini */
        INIT_INI_ARRAY(&ahp->ah_iniCckfirNormal, ar9287Common_normal_cck_fir_coeff_kiwi1_1, 2);
        INIT_INI_ARRAY(&ahp->ah_iniCckfirJapan2484, ar9287Common_japan_2484_cck_fir_coeff_kiwi1_1, 2);
#endif /* AH_SUPPORT_KIWI_11 */
    }

    ecode = ar5416HwAttach(ah);
    if (ecode != HAL_OK)
        goto bad;
        
    /* rxgain table */
    if (AR_SREV_KIWI_11_OR_LATER(ah)) {
#if defined(AH_SUPPORT_KIWI)     /* latest rev */ || \
    defined(AH_SUPPORT_KIWI_ALL) /* all revs */   || \
    defined(AH_SUPPORT_KIWI_11)  /* rev 1.1+ */
        /* use the register initializations from ar9287_1_1.ini */
        INIT_INI_ARRAY(
            &ahp->ah_iniModesRxgain,
            ar9287Modes_rx_gain_kiwi1_1, 6);
#endif /* AH_SUPPORT_KIWI_11 */
    }
    else if (AR_SREV_KIWI_10(ah)) {
#if defined(AH_SUPPORT_KIWI_ALL) /* all revs */ || \
    defined(AH_SUPPORT_KIWI_10)  /* rev 1.0) */
        /* use the register initializations from ar9287.ini */
        INIT_INI_ARRAY(
            &ahp->ah_iniModesRxgain,
            ar9287Modes_rx_gain_kiwi1_0, 6);
#endif /* AH_SUPPORT_KIWI_10 */
    }
    else if (AR_SREV_MERLIN_20(ah)) {
#if defined(AH_SUPPORT_MERLIN)     /* latest rev */ || \
    defined(AH_SUPPORT_MERLIN_ALL) /* all revs */   || \
    defined(AH_SUPPORT_MERLIN_20)  /* rev 2.0+ */
        /* use the register initializations from ar9280_merlin2.ini */
        if (ar5416EepromGet(ahp, EEP_MINOR_REV) >= AR5416_EEP_MINOR_VER_17) {
            u_int32_t rxgainType = ar5416EepromGet(ahp, EEP_RXGAIN_TYPE);

            if (rxgainType == AR5416_EEP_RXGAIN_13dB_BACKOFF) {
                INIT_INI_ARRAY(
                    &ahp->ah_iniModesRxgain,
                    ar9280Modes_backoff_13db_rxgain_merlin2, 6);
            } else if (rxgainType == AR5416_EEP_RXGAIN_23dB_BACKOFF){
                INIT_INI_ARRAY(
                    &ahp->ah_iniModesRxgain,
                    ar9280Modes_backoff_23db_rxgain_merlin2, 6);
            } else {
                INIT_INI_ARRAY(
                    &ahp->ah_iniModesRxgain,
                    ar9280Modes_original_rxgain_merlin2, 6);
            }
        } else {
            INIT_INI_ARRAY(
                &ahp->ah_iniModesRxgain,
                ar9280Modes_original_rxgain_merlin2, 6);
        }
#endif /* AH_SUPPORT_MERLIN_20 */
    }
      
    /* txgain table */
    if (AR_SREV_KIWI_11_OR_LATER(ah)) {
#if defined(AH_SUPPORT_KIWI)     /* latest rev */ || \
    defined(AH_SUPPORT_KIWI_ALL) /* all revs */   || \
    defined(AH_SUPPORT_KIWI_11)  /* rev 1.1+ */
        /* use the register initializations from ar9287.ini */
        INIT_INI_ARRAY(
            &ahp->ah_iniModesTxgain,
            ar9287Modes_tx_gain_kiwi1_1, 6);
#endif /* AH_SUPPORT_KIWI_11 */
    }
    else if (AR_SREV_KIWI_10(ah)) {
#if defined(AH_SUPPORT_KIWI_ALL) /* all revs */ || \
    defined(AH_SUPPORT_KIWI_10)  /* rev 1.0) */
        /* use the register initializations from ar9287.ini */
        INIT_INI_ARRAY(
            &ahp->ah_iniModesTxgain,
            ar9287Modes_tx_gain_kiwi1_0, 6);
#endif /* AH_SUPPORT_KIWI_10 */
    }
    else if (AR_SREV_MERLIN_20(ah)) {
#if defined(AH_SUPPORT_MERLIN)     /* latest rev */ || \
    defined(AH_SUPPORT_MERLIN_ALL) /* all revs */   || \
    defined(AH_SUPPORT_MERLIN_20)  /* rev 2.0+ */
        /* use the register initializations from ar9280_merlin2.ini */
        if (ar5416EepromGet(ahp, EEP_MINOR_REV) >= AR5416_EEP_MINOR_VER_19) {
            u_int32_t txgainType = ar5416EepromGet(ahp, EEP_TXGAIN_TYPE);

            if (txgainType == AR5416_EEP_TXGAIN_HIGH_POWER) {
                INIT_INI_ARRAY(
                    &ahp->ah_iniModesTxgain,
                    ar9280Modes_high_power_tx_gain_merlin2, 6);
            } else {
                INIT_INI_ARRAY(
                    &ahp->ah_iniModesTxgain,
                    ar9280Modes_original_tx_gain_merlin2, 6);
            }
        } else {
            INIT_INI_ARRAY(
                &ahp->ah_iniModesTxgain,
                ar9280Modes_original_tx_gain_merlin2, 6);
        }
#endif /* AH_SUPPORT_MERLIN_20 */
    }

    /* 
     * Set the curTrigLevel to a value that works all modes - 11a/b/g or 11n 
     * with aggregation enabled or disabled. 
     */
    if (AR_SREV_KITE(ah) || AR_SREV_K2(ah)) {
        /* Number of FIFOs are reduced to half in Kite. So need to adjust
         * txTrigLevel accordingly to aviod tx stall
         * See Bug #33104
         */
        AH_PRIVATE(ah)->ah_tx_trig_level = (AR_FTRIG_256B >> AR_FTRIG_S);

        fifo_threshold = MAX_TX_FIFO_THRESHOLD_KITE;
    }
    else {
        AH_PRIVATE(ah)->ah_tx_trig_level = (AR_FTRIG_512B >> AR_FTRIG_S);

        fifo_threshold = MAX_TX_FIFO_THRESHOLD_DEFAULT;
    }

    /* Set the max trigger level */
    if (ahpriv->ah_config.ath_hal_txTrigLevelMax == 0) {
        ahpriv->ah_config.ath_hal_txTrigLevelMax = fifo_threshold;
    }
    
    if (!ar5416FillCapabilityInfo(ah)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s:failed ar5416FillCapabilityInfo\n", __func__);
        ecode = HAL_EEREAD;
        goto bad;
    }

    /*
    ** XXX
    ** The above may need to be done for other arrays in the future.  At this
    ** point, it's limited to the modal arrays.
    **
    ** Got everything we need now to setup the capabilities.
    */
    
    ecode = ar5416InitMacAddr(ah);
    if (ecode != HAL_OK) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: failed initializing mac address\n", __func__);
        goto bad;
    }

#if ATH_WOW
    /*
     * Needs to be removed once we stop using XB92 XXX
     * FIXME: Check with latest boards too - SriniK
     */
    ar5416WowSetGpioResetLow(ah);

    /*
     * Clear the Wow Status.
     */
    OS_REG_WRITE(ah, AR_PCIE_PM_CTRL, OS_REG_READ(ah, AR_PCIE_PM_CTRL) | 
        AR_PMCTRL_WOW_PME_CLR);
    OS_REG_WRITE(ah, AR_WOW_PATTERN_REG,
        AR_WOW_CLEAR_EVENTS(OS_REG_READ(ah, AR_WOW_PATTERN_REG)));
#endif

    /*
     * Based on the chip type, set up the default noise floor values
     * and limits for both the 2 GHz and 5 GHz bands.
     */
    ahpriv->nf_2GHz.nominal = AR_PHY_CCA_NOM_VAL_OWL_2GHZ;
    ahpriv->nf_2GHz.max     = AR_PHY_CCA_MAX_GOOD_VAL_OWL_2GHZ;
    ahpriv->nf_2GHz.min     = AR_PHY_CCA_MIN_GOOD_VAL_OWL_2GHZ;
    ahpriv->nf_5GHz.nominal = AR_PHY_CCA_NOM_VAL_OWL_5GHZ;
    ahpriv->nf_5GHz.max     = AR_PHY_CCA_MAX_GOOD_VAL_OWL_5GHZ;
    ahpriv->nf_5GHz.min     = AR_PHY_CCA_MIN_GOOD_VAL_OWL_5GHZ;
    if (AR_SREV_MERLIN(ah)) {
        ahpriv->nf_2GHz.nominal = AR_PHY_CCA_NOM_VAL_MERLIN_2GHZ;
        ahpriv->nf_2GHz.max     = AR_PHY_CCA_MAX_GOOD_VAL_MERLIN_2GHZ;
        ahpriv->nf_2GHz.min     = AR_PHY_CCA_MIN_GOOD_VAL_MERLIN_2GHZ;
        ahpriv->nf_5GHz.nominal = AR_PHY_CCA_NOM_VAL_MERLIN_5GHZ;
        ahpriv->nf_5GHz.max     = AR_PHY_CCA_MAX_GOOD_VAL_MERLIN_5GHZ;
        ahpriv->nf_5GHz.min     = AR_PHY_CCA_MIN_GOOD_VAL_MERLIN_5GHZ;
    } else if (AR_SREV_KITE(ah)) {
        ahpriv->nf_2GHz.nominal = AR_PHY_CCA_NOM_VAL_KITE_2GHZ;
        ahpriv->nf_2GHz.max     = AR_PHY_CCA_MAX_GOOD_VAL_KITE_2GHZ;
        ahpriv->nf_2GHz.min     = AR_PHY_CCA_MIN_GOOD_VAL_KITE_2GHZ;
        /* Kite only supports the 2 GHz band */
    } else if (AR_SREV_K2(ah)) {
        ahpriv->nf_2GHz.nominal = AR_PHY_CCA_NOM_VAL_K2_2GHZ;
        ahpriv->nf_2GHz.max     = AR_PHY_CCA_MAX_GOOD_VAL_K2_2GHZ;
        ahpriv->nf_2GHz.min     = AR_PHY_CCA_MIN_GOOD_VAL_K2_2GHZ;
        /* K2 only supports the 2 GHz band */
    } else if (AR_SREV_KIWI(ah)) {
        ahpriv->nf_2GHz.nominal = AR_PHY_CCA_NOM_VAL_KIWI_2GHZ;
        ahpriv->nf_2GHz.max     = AR_PHY_CCA_MAX_GOOD_VAL_KIWI_2GHZ;
        ahpriv->nf_2GHz.min     = AR_PHY_CCA_MIN_GOOD_VAL_KIWI_2GHZ;
        /* Kiwi only supports the 2 GHz band */
    }
    ahpriv->nf_cw_int_delta = AR_PHY_CCA_CW_INT_DELTA;


    return ah;

bad:
    if (ahp)
        ar5416Detach((struct ath_hal *) ahp);
    if (status)
        *status = ecode;
    return AH_NULL;
}

void
ar5416Detach(struct ath_hal *ah)
{
    HALASSERT(ah != AH_NULL);
    HALASSERT(AH_PRIVATE(ah)->ah_magic == AR5416_MAGIC);

    /* Make sure that chip is awake before writing to it */
    /* Don't write if wake up fails */
    if (!ar5416SetPowerMode(ah, HAL_PM_AWAKE, AH_TRUE)) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE,
                 "%s: failed to wake up chip\n",
                 __func__);
	    goto ar_detach;
    }

    /* fix for EV 59534. If the AR_WA_D3_L1_DISABLE is set, pcie can not enter L1 when driver is unloaded */
    if ((AH_PRIVATE(ah)->ah_config.ath_hal_pcie_waen & AR_WA_D3_L1_DISABLE) &&
        AH_PRIVATE(ah)->ah_config.ath_hal_pcieDetach) {
        OS_REG_WRITE(ah, AR_WA, OS_REG_READ(ah, AR_WA) & ~AR_WA_D3_L1_DISABLE);
    }

ar_detach:
    ar5416HwDetach(ah);
    ar5416SetPowerMode(ah, HAL_PM_FULL_SLEEP, AH_TRUE);
    ath_hal_hdprintf_deregister(ah);
    ath_hal_free(ah, ah);
}

struct ath_hal_5416 *
ar5416NewState(u_int16_t devid, HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
    HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype,
    struct hal_reg_parm *hal_conf_parm, HAL_STATUS *status)
{
    static const u_int8_t defbssidmask[IEEE80211_ADDR_LEN] =
        { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
    struct ath_hal_5416 *ahp;
    struct ath_hal *ah;

    /* NB: memory is returned zero'd */
    ahp = qdf_mem_malloc(sizeof(struct ath_hal_5416));
    if (ahp == AH_NULL) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
                 "%s: cannot allocate memory for state block\n",
                 __func__);
        *status = HAL_ENOMEM;
        return AH_NULL;
    }

    ah = &ahp->ah_priv.priv.h;
    /* set initial values */

    /* Attach Owl 1.0 structure as default hal structure */
    OS_MEMZERO(&ahp->ah_priv, sizeof(ahp->ah_priv));
    OS_MEMCPY(&ahp->ah_priv.priv, &ar5416hal, sizeof(ahp->ah_priv.priv));

    AH_PRIVATE(ah)->ah_osdev = osdev;
    AH_PRIVATE(ah)->ah_st = st;
    AH_PRIVATE(ah)->ah_sc = sc;
    AH_PRIVATE(ah)->ah_sh = sh;

    AH_PRIVATE(ah)->ah_magic = AR5416_MAGIC;
    AH_PRIVATE(ah)->ah_devid = devid;

    AH_PRIVATE(ah)->ah_flags = 0;
     
    

    /*
    ** Initialize factory defaults in the private space
    */
    ath_hal_factory_defaults(AH_PRIVATE(ah), hal_conf_parm);

    if ((devid == AR5416_AR9100_DEVID)) /* Howl */
        AH_PRIVATE(ah)->ah_mac_version = AR_SREV_VERSION_HOWL;

         
    if (!hal_conf_parm->calInFlash)
        if(!AR_SREV_HOWL(ah))
            AH_PRIVATE(ah)->ah_flags |= AH_USE_EEPROM;

#ifndef WIN32
    if (ar5416EepDataInFlash(ah)) {
        ahp->ah_priv.priv.ah_eeprom_read  = ar5416FlashRead;
        ahp->ah_priv.priv.ah_eeprom_dump  = AH_NULL;
#ifdef AH_SUPPORT_WRITE_EEPROM
        ahp->ah_priv.priv.ah_eeprom_write = ar5416FlashWrite;
#endif
    } else {
        ahp->ah_priv.priv.ah_eeprom_read  = ar5416EepromRead;
#ifdef AH_SUPPORT_WRITE_EEPROM
        ahp->ah_priv.priv.ah_eeprom_write = ar5416EepromWrite;
#endif
    }
#else /* WIN32 */

    ahp->ah_priv.priv.ah_eeprom_read  = ar5416EepromRead;
#ifdef AH_SUPPORT_WRITE_EEPROM
    ahp->ah_priv.priv.ah_eeprom_write = ar5416EepromWrite;
#endif

#endif /* WIN32 */

    AH_PRIVATE(ah)->ah_power_limit = MAX_RATE_POWER;
    AH_PRIVATE(ah)->ah_tp_scale = HAL_TP_SCALE_MAX;  /* no scaling */

    ahp->ah_atimWindow = 0;         /* [0..1000] */
    ahp->ah_diversityControl = AH_PRIVATE(ah)->ah_config.ath_hal_diversity_control;
    ahp->ah_antennaSwitchSwap = AH_PRIVATE(ah)->ah_config.ath_hal_antenna_switch_swap;

    /*
     * Enable MIC handling.
     */
    ahp->ah_staId1Defaults = AR_STA_ID1_CRPT_MIC_ENABLE;
    ahp->ah_enable32kHzClock = DONT_USE_32KHZ;/* XXX */
    ahp->ah_slottime = (u_int) -1;
    ahp->ah_acktimeout = (u_int) -1;
    OS_MEMCPY(&ahp->ah_bssidmask, defbssidmask, IEEE80211_ADDR_LEN);

    /*
     * 11g-specific stuff
     */
    ahp->ah_gBeaconRate = 0;        /* adhoc beacon fixed rate */

    /* SM power mode: Attach time, disable any setting */
    ahp->ah_smPowerMode = HAL_SMPS_DEFAULT;

    return ahp;
}

#ifdef AH_PRIVATE_DIAG
HAL_BOOL
ar5416ChipTest(struct ath_hal *ah)
{
    u_int32_t regAddr[2] = { AR_STA_ID0, AR_PHY_BASE+(8 << 2) };
    u_int32_t regHold[2];
    u_int32_t patternData[4] =
        { 0x55555555, 0xaaaaaaaa, 0x66666666, 0x99999999 };
    int i, j;

    /* Test PHY & MAC registers */
    for (i = 0; i < 2; i++) {
        u_int32_t addr = regAddr[i];
        u_int32_t wrData, rdData;

        regHold[i] = OS_REG_READ(ah, addr);
        for (j = 0; j < 0x100; j++) {
            wrData = (j << 16) | j;
            OS_REG_WRITE(ah, addr, wrData);
            rdData = OS_REG_READ(ah, addr);
            if (rdData != wrData) {
                HDPRINTF(ah, HAL_DBG_REG_IO, 
                    "%s: address test failed addr: 0x%08x - wr:0x%08x != rd:0x%08x\n",
                    __func__, addr, wrData, rdData);
                return AH_FALSE;
            }
        }
        for (j = 0; j < 4; j++) {
            wrData = patternData[j];
            OS_REG_WRITE(ah, addr, wrData);
            rdData = OS_REG_READ(ah, addr);
            if (wrData != rdData) {
                HDPRINTF(ah, HAL_DBG_REG_IO,
                    "%s: address test failed addr: 0x%08x - wr:0x%08x != rd:0x%08x\n",
                    __func__, addr, wrData, rdData);
                return AH_FALSE;
            }
        }
        OS_REG_WRITE(ah, regAddr[i], regHold[i]);
    }
    OS_DELAY(100);
    return AH_TRUE;
}
#endif

/*
 * Store the channel edges for the requested operational mode
 */
HAL_BOOL
ar5416GetChannelEdges(struct ath_hal *ah,
    u_int16_t flags, u_int16_t *low, u_int16_t *high)
{
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CAPABILITIES *pCap = &ahpriv->ah_caps;

    if (flags & CHANNEL_5GHZ) {
        *low = pCap->hal_low_5ghz_chan;
        *high = pCap->hal_high_5ghz_chan;
        return AH_TRUE;
    }
    if ((flags & CHANNEL_2GHZ)) {
        *low = pCap->hal_low_2ghz_chan;
        *high = pCap->hal_high_2ghz_chan;

        return AH_TRUE;
    }
    return AH_FALSE;
}

/*
 * Fill all software cached or static hardware state information.
 * Return failure if capabilities are to come from EEPROM and
 * cannot be read.
 */
HAL_BOOL
ar5416FillCapabilityInfo(struct ath_hal *ah)
{
#define AR_KEYTABLE_SIZE    128
    struct ath_hal_5416 *ahp = AH5416(ah);
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CAPABILITIES *pCap = &ahpriv->ah_caps;
    u_int16_t capField = 0, eeval;

    ahpriv->ah_devType = (u_int16_t)ar5416EepromGet(ahp, EEP_DEV_TYPE);
    eeval = ar5416EepromGet(ahp, EEP_REG_0);

    /* XXX record serial number */
    AH_PRIVATE(ah)->ah_current_rd = eeval;
      
    pCap->halintr_mitigation = AH_TRUE;
    eeval = ar5416EepromGet(ahp, EEP_REG_1);
    if (AR_SREV_KITE_10_OR_LATER(ah)) {
        /* 
         * For Kite and later chipsets, the following bits are not programmed in EEPROM
         * and so are set as enabled always.
         * Bit 0: en_fcc_mid,  Bit 1: en_jap_mid,      Bit 2: en_fcc_dfs_ht40
         * Bit 3: en_jap_ht40, Bit 4: en_jap_dfs_ht40
         */
        eeval |= AR9285_RDEXT_DEFAULT;
    }
    AH_PRIVATE(ah)->ah_current_rd_ext = eeval;

    /* Read the capability EEPROM location */
    capField = ar5416EepromGet(ahp, EEP_OP_CAP);

    /* Construct wireless mode from EEPROM */
    pCap->hal_wireless_modes = 0;
    eeval = ar5416EepromGet(ahp, EEP_OP_MODE);

    if (eeval & AR5416_OPFLAGS_11A) {
        pCap->hal_wireless_modes |= HAL_MODE_11A |
         ((!ahpriv->ah_config.ath_hal_ht_enable || (eeval & AR5416_OPFLAGS_N_5G_HT20)) ?
           0 : (HAL_MODE_11NA_HT20 | ((eeval & AR5416_OPFLAGS_N_5G_HT40) ? 
           0 : (HAL_MODE_11NA_HT40PLUS | HAL_MODE_11NA_HT40MINUS))));
    }
    if (eeval & AR5416_OPFLAGS_11G) {
        pCap->hal_wireless_modes |= HAL_MODE_11B | HAL_MODE_11G | 
        ((!ahpriv->ah_config.ath_hal_ht_enable || (eeval & AR5416_OPFLAGS_N_2G_HT20)) ?
          0 : (HAL_MODE_11NG_HT20 | ((eeval & AR5416_OPFLAGS_N_2G_HT40) ? 
          0 : (HAL_MODE_11NG_HT40PLUS | HAL_MODE_11NG_HT40MINUS))));
                  
    }
    pCap->hal_tx_chain_mask = ar5416EepromGet(ahp, EEP_TX_MASK);
#ifdef __LINUX_ARM_ARCH__ /* AP71 */
    pCap->hal_rx_chain_mask = ar5416EepromGet(ahp, EEP_RX_MASK);
#else /* CB71/72 or XB72 */
    /* 
     * TODO : Tempary force rx chain mask by eeprom for K2, should fix it by DeviceId assign.   
     */
    if ((AH_PRIVATE(ah)->ah_devid == AR5416_DEVID_PCI) && !(eeval & AR5416_OPFLAGS_11A) && !AR_SREV_K2(ah)) {
        /* CB71: GPIO 0 is pulled down to indicate 3 rx chains */
        pCap->hal_rx_chain_mask = (ar5416GpioGet(ah, 0)) ? 0x5 : 0x7;
    } else {
        /* Use RxChainMask from EEPROM. */
        pCap->hal_rx_chain_mask = ar5416EepromGet(ahp, EEP_RX_MASK);
    }
#endif

    /*
     * This being a newer chip supports TKIP non-splitmic mode.
     *
     * TKIP non-splitmic mode doesn't work for Merlin 1.0, disable it.
     * To Do - Remove check when Merlin 2.0 is released
     */
    if (!(AR_SREV_MERLIN(ah) && (AH_PRIVATE(ah)->ah_mac_rev == 0))) {
        ahp->ah_miscMode |= AR_PCU_MIC_NEW_LOC_ENA;
    }

    pCap->hal_low_2ghz_chan = 2312;
    pCap->hal_high_2ghz_chan = 2732;

    pCap->hal_low_5ghz_chan = 4920;
    pCap->hal_high_5ghz_chan = 6100;

    pCap->hal_cipher_ckip_support = AH_FALSE;
    pCap->hal_cipher_tkip_support = AH_TRUE;
    pCap->hal_cipher_aes_ccm_support = AH_TRUE;

    pCap->hal_mic_ckip_support    = AH_FALSE;
    pCap->hal_mic_tkip_support    = AH_TRUE;
    pCap->hal_mic_aes_ccm_support  = AH_TRUE;

    pCap->hal_chan_spread_support = AH_TRUE;
    pCap->hal_sleep_after_beacon_broken = AH_TRUE;

    pCap->hal_burst_support = AH_TRUE;
    pCap->hal_chap_tuning_support = AH_TRUE;
    pCap->hal_turbo_prime_support = AH_TRUE;
    pCap->hal_fast_frames_support = AH_FALSE;

    pCap->hal_turbo_g_support = pCap->hal_wireless_modes & HAL_MODE_108G;

    pCap->hal_xr_support = AH_FALSE;

    pCap->hal_ht_support = ahpriv->ah_config.ath_hal_ht_enable ? AH_TRUE : AH_FALSE;
    pCap->hal_gtt_support = AH_TRUE;
    pCap->hal_ps_poll_broken = AH_TRUE;    /* XXX fixed in later revs? */

    if (AR_SREV_KIWI(ah) || AR_SREV_K2(ah)) 
        pCap->hal_ht20_sgi_support = AH_TRUE;
	else
        pCap->hal_ht20_sgi_support = AH_FALSE;

    /*USB doesn't support VEOL, such as K2, MAGPIE plus any radio chip,like KIWI, Merlin...*/
#ifdef ATH_USB
    pCap->hal_veol_support = AH_FALSE;
#else
    pCap->hal_veol_support = AH_TRUE;
#endif

    pCap->hal_bss_id_mask_support = AH_TRUE;
    pCap->hal_mcast_key_srch_support = AH_FALSE;    /* Bug 26802, fixed in later revs? */
    pCap->hal_tsf_add_support = AH_TRUE;

    if (capField & AR_EEPROM_EEPCAP_MAXQCU)
        pCap->hal_total_queues = MS(capField, AR_EEPROM_EEPCAP_MAXQCU);
    else
        pCap->hal_total_queues = HAL_NUM_TX_QUEUES;

    if (capField & AR_EEPROM_EEPCAP_KC_ENTRIES)
        pCap->hal_key_cache_size =
            1 << MS(capField, AR_EEPROM_EEPCAP_KC_ENTRIES);
    else
        pCap->hal_key_cache_size = AR_KEYTABLE_SIZE;

    pCap->hal_fast_cc_support = AH_TRUE;
    pCap->hal_num_mr_retries   = 4;
    pCap->hal_tx_trig_level_max = ahpriv->ah_config.ath_hal_txTrigLevelMax;
    pCap->hal_proxy_sta_support = AH_FALSE;

    if (AR_SREV_K2(ah)) {
        pCap->hal_num_gpio_pins = AR9271_NUM_GPIO;
    } else if (AR_SREV_KITE_10_OR_LATER(ah)) {
        pCap->hal_num_gpio_pins = AR9285_NUM_GPIO;
    } else if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        pCap->hal_num_gpio_pins = AR928X_NUM_GPIO;
    } else {
        pCap->hal_num_gpio_pins = AR_NUM_GPIO;
    }

    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        pCap->hal_wow_support = AH_TRUE;
        pCap->hal_wow_match_pattern_exact = AH_TRUE;
        if (AR_SREV_MERLIN(ah)) {
            pCap->hal_wow_pattern_match_dword = AH_TRUE;
        }
    } else {
        pCap->hal_wow_support = AH_FALSE;
        pCap->hal_wow_match_pattern_exact = AH_FALSE;
    }

    if (AR_SREV_SOWL_10_OR_LATER(ah) || AR_SREV_HOWL(ah)) {
        pCap->hal_cst_support = AH_TRUE;
        pCap->hal_rifs_rx_support = AH_TRUE;
        pCap->hal_rifs_tx_support = AH_TRUE;
        /* SOWL has no aggregation limit with RTS, so allow max ampdu */
        pCap->hal_rts_aggr_limit = IEEE80211_AMPDU_LIMIT_MAX;
        /* Merlin MFP is set in registry. Disabled for now */
        pCap->hal_mfp_support = ahpriv->ah_config.ath_hal_mfp_support;
    } else {
        /* OWL 2.0 has aggregation limit of 8K with RTS */
        pCap->hal_rts_aggr_limit = (8*1024);

        /* Disable hardware MFP in the earlier chips. 
         * They will automatically decrypt like data frames
         */
        pCap->hal_mfp_support = HAL_MFP_QOSDATA;
    }

    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        pCap->halforce_ppm_support = AH_FALSE;
    } else {
        pCap->halforce_ppm_support = AH_TRUE;
    }

    pCap->hal_hw_beacon_proc_support = AH_FALSE;
    pCap->hal_num_tx_maps = 1;
    pCap->hal_tx_desc_len = sizeof(struct ath_desc);
    pCap->hal_tx_status_len = 0;
    pCap->hal_rx_status_len = 0;

    /* Enable extension channel DFS support for SOWL and HOWL*/
    if (AR_SREV_SOWL_10_OR_LATER(ah) || AR_SREV_HOWL(ah)) {
        pCap->hal_use_combined_radar_rssi = AH_TRUE;
        pCap->hal_ext_chan_dfs_support = AH_TRUE;
    } else {
        pCap->hal_use_combined_radar_rssi = AH_FALSE;
        pCap->hal_ext_chan_dfs_support = AH_FALSE;
    }

    ahpriv->ah_rfsilent = ar5416EepromGet(ahp, EEP_RF_SILENT);
    if (ahpriv->ah_rfsilent & EEP_RFSILENT_ENABLED) {
        ahp->ah_gpioSelect = MS(ahpriv->ah_rfsilent, EEP_RFSILENT_GPIO_SEL);
        ahp->ah_polarity   = MS(ahpriv->ah_rfsilent, EEP_RFSILENT_POLARITY);

        ath_hal_enable_rfkill(ah, AH_TRUE);
        pCap->hal_rf_silent_support = AH_TRUE;
    }

    if (ahpriv->ah_devid == AR5416_DEVID_AR9280_PCI) {
        pCap->hal_wps_push_button = AH_TRUE;
    } else {
        pCap->hal_wps_push_button = AH_FALSE;
    }
#if WLAN_SPECTRAL_ENABLE
    /* Merlin and later are capable of spectral scan */
    if (AR_SREV_MERLIN(ah) || AR_SREV_KIWI(ah)) {
       pCap->hal_spectral_scan = AH_TRUE;
    } else {
       pCap->hal_spectral_scan = AH_FALSE;
    }
#endif

#ifdef ATH_BT_COEX
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        pCap->hal_bt_coex_support = AH_TRUE;
        /* BT coex ASPM WAR is needed only for Merlin and Kite */
        pCap->hal_bt_coex_aspm_war = AR_SREV_KIWI(ah) ? AH_FALSE : AH_TRUE;
    } else {
        pCap->hal_bt_coex_support = AH_FALSE;
    }
#endif

    pCap->hal_mci_support = AH_FALSE;
#if ATH_WOW_OFFLOAD
    pCap->hal_wow_gtk_offload_support   = AH_FALSE;
    pCap->hal_wow_arp_offload_support   = AH_FALSE;
    pCap->hal_wow_ns_offload_support    = AH_FALSE;
    pCap->hal_wow_4way_hs_wakeup_support= AH_FALSE;
    pCap->hal_wow_acer_magic_support    = AH_FALSE;
    pCap->hal_wow_acer_swka_support     = AH_FALSE;
#endif /* ATH_WOW_OFFLOAD */
    pCap->hal_radio_retention_support = AH_FALSE;
    pCap->hal_gen_timer_support = AH_TRUE;
    ahp->ah_availGenTimers = ~((1 << AR_FIRST_NDP_TIMER) - 1);
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        /* For Merlin and later, there are 16 generic timers. #7-#15 are available to use. */
        ahp->ah_availGenTimers &= (1 << AR_NUM_GEN_TIMERS_POST_MERLIN) - 1;
    } else {
        /* For Owl, there are 8 generic timers. Only #7 is available to use. */
        ahp->ah_availGenTimers &= (1 << AR_NUM_GEN_TIMERS_PRE_MERLIN) - 1;
    }

    /* 
     * Owl/Howl cannot return automatically to network sleep mode after 
     * waking up to receive TIM.
     */
    if ((AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_OWL_PCI)  ||
        (AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_OWL_PCIE) || 
        (AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_SOWL)     ||
        (AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_HOWL)     ||
        (AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_MERLIN)) {
        pCap->hal_auto_sleep_support = AH_FALSE;
    }
    else {
        pCap->hal_auto_sleep_support = AH_TRUE;
    }


    /**
     * Bug 33245, 32737, 29824 reported a bug regarding aggregation on VAP 1~3.
     * It has been fixed in Howl 1.4, SOWL, andMerlin or later chipset.
     */
    pCap->hal_mbssid_aggr_support = AH_TRUE;

#ifdef AR9100
    if (AR_SREV_HOWL(ah)) {
        /* Check for Howl */
        u_int32_t ver=0;
        ath_hal_get_chip_revisionid(&ver);
        HDPRINTF(ah, HAL_DBG_RESET, "Howl Revision ID 0x%x\n",ver);
        if ( (ver & ATH_SREV_REV_HOWL_MASK) == ATH_SREV_REV_HOWL_NO_MBSSID_AGGR) {
             /* Howl 1.1,1.2,1.3 not supporting MBSSID aggregation */
            HDPRINTF(ah, HAL_DBG_RESET, "No MBSSID aggregation support\n");
            pCap->hal_mbssid_aggr_support = AH_FALSE;
        }
    } else {
#endif
        if ( !AR_SREV_SOWL_10_OR_LATER(ah) ) {
            HDPRINTF(ah, HAL_DBG_RESET, "No MBSSID aggregation support");
            pCap->hal_mbssid_aggr_support = AH_FALSE;
        }
#ifdef AR9100
    }
#endif

    /* Merlin/Kite have issue with splitting transanctions on 4KB boundary */
    if (AR_SREV_MERLIN(ah) || AR_SREV_KITE(ah)) {
        pCap->hal4kb_split_trans_support = AH_FALSE;
    } else {
        pCap->hal4kb_split_trans_support = AH_TRUE;
    }

    /* Read regulatory domain flag */
    if (AH_PRIVATE(ah)->ah_current_rd_ext & (1 << REG_EXT_JAPAN_MIDBAND)) {
        /* 
         * For AR5416 and above, if REG_EXT_JAPAN_MIDBAND is set,
         * turn on U1 EVEN, U2, and MIDBAND. 
         */
        pCap->hal_reg_cap =
            AR_EEPROM_EEREGCAP_EN_KK_NEW_11A |
            AR_EEPROM_EEREGCAP_EN_KK_U1_EVEN | 
            AR_EEPROM_EEREGCAP_EN_KK_U2 |
            AR_EEPROM_EEREGCAP_EN_KK_MIDBAND;
    } else {
        pCap->hal_reg_cap =
            AR_EEPROM_EEREGCAP_EN_KK_NEW_11A |
            AR_EEPROM_EEREGCAP_EN_KK_U1_EVEN;
    }

    /* Advertise FCC midband for Owl devices with FCC midband set in eeprom */
    if ((AH_PRIVATE(ah)->ah_current_rd_ext & (1 << REG_EXT_FCC_MIDBAND)) &&
        ((AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_OWL_PCI) ||
         (AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_OWL_PCIE)))
        pCap->hal_reg_cap |= AR_EEPROM_EEREGCAP_EN_FCC_MIDBAND;

    pCap->hal_num_ant_cfg_5ghz =
        ar5416EepromGetNumAntConfig(ahp, HAL_FREQ_BAND_5GHZ);
    pCap->hal_num_ant_cfg_2ghz =
        ar5416EepromGetNumAntConfig(ahp, HAL_FREQ_BAND_2GHZ);

    if (AR_SREV_MERLIN(ah) || AR_SREV_KIWI(ah)) {
        pCap->hal_rx_stbc_support = 1;  /* number of streams for STBC receive. */
        pCap->hal_tx_stbc_support = 1;
    } else if (AR_SREV_KITE(ah) || AR_SREV_K2(ah)) {
        /* Only RX stbc supported */
        pCap->hal_rx_stbc_support = 1;
        pCap->hal_tx_stbc_support = 0;
    } else {
        pCap->hal_rx_stbc_support = 0;
        pCap->hal_tx_stbc_support = 0;
    }

    pCap->hal_enhanced_dma_support = AH_FALSE;
#ifdef ATH_SUPPORT_DFS
    if (AR_SREV_KIWI(ah) || AR_SREV_MERLIN(ah)) {
        pCap->hal_enhanced_dfs_support = AH_TRUE;
    } else {
        pCap->hal_enhanced_dfs_support = AH_FALSE;
    }
#endif
    pCap->hal_hw_uapsd_trig        = AH_FALSE;

    /* EV61133 (missing interrupts due to AR_ISR_RAC) */
#if AR5416_ISR_READ_CLEAR_SUPPORT
    pCap->hal_isr_rac_support = AH_TRUE;
#else
    pCap->hal_isr_rac_support = AH_FALSE;
#endif

    pCap->hal_wep_tkip_aggr_support =
        AR_SREV_MERLIN_10_OR_LATER(ah) ? AH_TRUE : AH_FALSE;
    pCap->hal_wep_tkip_aggr_num_tx_delim = 64;
    pCap->hal_wep_tkip_aggr_num_rx_delim = 64;
    /* Ev 50370: WAR for WEP/TKIP TxUnderRun for some chips */
    pCap->hal_wep_tkip_max_ht_rate = 13;
    pCap->hal_cfend_fix_support = AH_TRUE;
    pCap->hal_aggr_extra_delim_war = AH_TRUE;
    pCap->hal_rx_desc_timestamp_bits = 32;
    pCap->hal_rx_tx_abort_support = AH_FALSE;
    pCap->hal_ani_poll_interval = AR5416_ANI_POLLINTERVAL;
    pCap->hal49Ghz = AH_TRUE;
#if ATH_SUPPORT_WRAP
    pCap->hal_proxy_sta_rx_war = AH_FALSE;
#endif
    pCap->hal_channel_switch_time_usec = AR5416_CHANNEL_SWITCH_TIME_USEC;
    AH_PRIVATE(ah)->ah_config.ath_hal_desc_tpc = 1;
#if ATH_SUPPORT_RAW_ADC_CAPTURE
    pCap->hal_raw_adc_capture = AR_SREV_MERLIN_20_OR_LATER(ah) ? AH_TRUE : AH_FALSE;
#endif
    if (AR_SREV_KITE(ah)) {
        /* Set the Ant diversity and combining support 
         * if the eeprom modal versoion is > 3 and 
         * ant fast diversity and lna diversity enabled
         */
        if ((ar5416EepromGet(ahp, EEP_MODAL_VER) >= 3) && 
            (ahp->ah_diversityControl == HAL_ANT_VARIABLE)) {
            u_int8_t ant_div_control1 = ar5416EepromGet(ahp, EEP_ANT_DIV_CTL1);
            if ((ant_div_control1 & 0x1) && ((ant_div_control1 >> 3) & 0x1)) {
                pCap->hal_ant_div_comb_support = AH_TRUE;
            }
        }
    }

    pCap->hal_lmac_smart_ant = ar5416EepromGet(ahp, EEP_SMART_ANTENNA);
#if ATH_SUPPORT_WAPI
    /*
     * WAPI engine support 2 stream rates at most currently
     */
    pCap->hal_wapi_max_tx_chains = 2;
    pCap->hal_wapi_max_rx_chains = 2;
#endif

    return AH_TRUE;
#undef AR_KEYTABLE_SIZE
}

HAL_STATUS
ar5416RadioAttach(struct ath_hal *ah)
{
    u_int32_t val;

    /*
     * Set correct Baseband to analog shift
     * setting to access analog chips.
     */
    OS_REG_WRITE(ah, AR_PHY(0), 0x00000007);

    val = ar5416GetRadioRev(ah);
    switch (val & AR_RADIO_SREV_MAJOR) {
        case 0:
            /* TODO:
             * WAR for bug 10062.  When RF_Silent is used, the
             * analog chip is reset.  So when the system boots
             * up with the radio switch off we cannot determine
             * the RF chip rev.  To workaround this check the mac/
             * phy revs and set radio rev.
             */
            val = AR_RAD5133_SREV_MAJOR;
            break;
        case AR_RAD5133_SREV_MAJOR:
        case AR_RAD5122_SREV_MAJOR:
        case AR_RAD2133_SREV_MAJOR:
        case AR_RAD2122_SREV_MAJOR:
            break;
        default:
#ifdef AH_DEBUG
            HDPRINTF(ah, HAL_DBG_CHANNEL,
                "%s: 5G Radio Chip Rev 0x%02X is not supported by this driver\n",
                __func__,AH_PRIVATE(ah)->ah_analog_5ghz_rev);
#endif
            return HAL_ENOTSUPP;
    }

    AH_PRIVATE(ah)->ah_analog_5ghz_rev = val;

    return HAL_OK;
}

static inline void
ar5416AniSetup(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    int i;

    const int totalSizeDesired[] = { -55, -55, -55, -55, -62 };
    const int coarseHigh[]       = { -14, -14, -14, -14, -12 };
    const int coarseLow[]        = { -64, -64, -64, -64, -70 };
    const int firpwr[]           = { -78, -78, -78, -78, -80 };

    for (i = 0; i < 5; i++) {
        ahp->ah_totalSizeDesired[i] = totalSizeDesired[i];
        ahp->ah_coarseHigh[i] = coarseHigh[i];
        ahp->ah_coarseLow[i] = coarseLow[i];
        ahp->ah_firpwr[i] = firpwr[i];
    }
}

static HAL_BOOL
ar5416GetChipPowerLimits(struct ath_hal *ah, HAL_CHANNEL *chans, u_int32_t nchans)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    return ahp->ah_rfHal.getChipPowerLim(ah, chans, nchans);
}

/*
 * Disable PLL when in L0s as well as receiver clock when in L1.
 * This power saving option must be enabled through the Serdes.
 *
 * Programming the Serdes must go through the same 288 bit serial shift
 * register as the other analog registers.  Hence the 9 writes.
 *
 * XXX Clean up the magic numbers.
 */
void
ar5416ConfigPciPowerSave(struct ath_hal *ah, int restore, int powerOff)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t val;
    u_int8_t i;

    if (AH_PRIVATE(ah)->ah_is_pci_express != AH_TRUE) {
        return;
    }

    if (AR_SREV_ELIJAH_20(ah)) {
        val = AH_PRIVATE(ah)->ah_config.ath_hal_war70c;
        if ( val)
        {
            val &= 0xffff00ff;
            val |= 0x6f00;
            OS_REG_WRITE(ah, 0x570c, val);
        }
    }

    /* Do not touch SERDES registers */
    if (AH_PRIVATE(ah)->ah_config.ath_hal_pcie_power_save_enable == 2) {
        return;
    }

    /* Nothing to do on restore for 11N */
    if (!restore) {
        if (AR_SREV_MERLIN_20_OR_LATER(ah)) {
            /* Merlin 2.0 or later chips use SERDES values from Ini file */
            for (i = 0; i < ahp->ah_iniPcieSerdes.ia_rows; i++) {
                OS_REG_WRITE(ah, INI_RA(&ahp->ah_iniPcieSerdes, i, 0), INI_RA(&ahp->ah_iniPcieSerdes, i, 1));
            }

        } else if (AR_SREV_MERLIN(ah) && (AH_PRIVATE(ah)->ah_mac_rev == AR_SREV_REVISION_MERLIN_10)) {
            
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x9248fd00);
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x24924924);

            /* RX shut off when elecidle is asserted */
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0xa8000019);
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x13160820);
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0xe5980560);

            /* Shut off CLKREQ active in L1 */
            if (AH_PRIVATE(ah)->ah_config.ath_hal_pcie_clock_req) {
                OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x401deffc);
            } else {
                OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x401deffd);
            }

            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x1aaabe40);
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0xbe105554);
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x00043007);

            /* Load the new settings */
            OS_REG_WRITE(ah, AR_PCIE_SERDES2, 0x00000000);

        } else {
            ENABLE_REG_WRITE_BUFFER

            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x9248fc00);
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x24924924);

            /* RX shut off when elecidle is asserted */
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x28000039);
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x53160824);
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0xe5980579);

            /* Ignore ath_hal_pcie_clock_req setting for pre-Merlin 11n */
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x001defff);

            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x1aaabe40);
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0xbe105554);
            OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x000e3007);

            /* Load the new settings */
            OS_REG_WRITE(ah, AR_PCIE_SERDES2, 0x00000000);

            OS_REG_WRITE_FLUSH(ah);

            DISABLE_REG_WRITE_BUFFER
        }

        OS_DELAY(1000);

    }

    if (powerOff) {
        /* clear bit 19 to disable L1 */
        OS_REG_CLR_BIT(ah, AR_PCIE_PM_CTRL, AR_PCIE_PM_CTRL_ENA);

        val = OS_REG_READ(ah, AR_WA);
        /* 
         * Set PCIe workaround bits
         * In Merlin and Kite, bit 14 in WA register (disable L1) should only 
         * be set when device enters D3 and be cleared when device comes back to D0.
         */
        if (AH_PRIVATE(ah)->ah_config.ath_hal_pcie_waen) {
            if (AH_PRIVATE(ah)->ah_config.ath_hal_pcie_waen & AR_WA_D3_L1_DISABLE)
                val |= AR_WA_D3_L1_DISABLE;
        } else {
            if (((AR_SREV_KITE(ah) || AR_SREV_K2(ah) || AR_SREV_KIWI(ah)) &&
                 (AR9285_WA_DEFAULT & AR_WA_D3_L1_DISABLE)) ||
                (AR_SREV_MERLIN(ah) && (AR9280_WA_DEFAULT & AR_WA_D3_L1_DISABLE))) {
                val |= AR_WA_D3_L1_DISABLE;
            }
        }

        if (AR_SREV_MERLIN(ah) || AR_SREV_KITE(ah) || AR_SREV_KIWI(ah)) {
            /*
             * Disable bit 6 and 7 before entering D3 to prevent system hang.
             * Please see EV 64306.
             */
            val &= ~(AR_WA_BIT6 | AR_WA_BIT7);
        }
       
        if (AR_SREV_MERLIN(ah)) {
            val |= AR_WA_BIT22;
        }

        if (AR_SREV_ELIJAH_20(ah)) {
            /*
            Enable bit23 for Elijah
            */
            val |= AR_WA_BIT23;
        }

        OS_REG_WRITE(ah, AR_WA, val);
    } else {
        /* 
         * Set PCIe workaround bits
         * In Merlin and Kite, bit 14 in WA register (disable L1) should only 
         * be set when device enters D3 and be cleared when device comes back to D0.
         */
        if (AH_PRIVATE(ah)->ah_config.ath_hal_pcie_waen) {
            val = AH_PRIVATE(ah)->ah_config.ath_hal_pcie_waen;
            if (!powerOff)
                val &= (~AR_WA_D3_L1_DISABLE);
        } else {
            if (AR_SREV_KITE(ah) || AR_SREV_K2(ah) || AR_SREV_KIWI(ah)) {
                val = AR9285_WA_DEFAULT;
                if (!powerOff)
                    val &= (~AR_WA_D3_L1_DISABLE);
            }
            else if (AR_SREV_MERLIN(ah)) {
                /*
                 * For Merlin chips, bit 22 of 0x4004 needs to be set to work around
                 * a card disappearance issue. See bug# 32141.
                 */
                val = AR9280_WA_DEFAULT;
                if (!powerOff)
                    val &= (~AR_WA_D3_L1_DISABLE);
            } else {
                val = AR_WA_DEFAULT;
            }
        }

        /* Software workaroud for ASPM system hang. Please see EV 64306. */
        if (AR_SREV_KITE(ah) || AR_SREV_KIWI(ah)) {
            val |= (AR_WA_BIT6 | AR_WA_BIT7);
        }
        if (AR_SREV_ELIJAH_20(ah)) {
            /*
            Enable bit23 for Elijah
            */
            val |= AR_WA_BIT23;
        }

        OS_REG_WRITE(ah, AR_WA, val);

        /* set bit 19 to allow forcing of pcie core into L1 state */
        OS_REG_SET_BIT(ah, AR_PCIE_PM_CTRL, AR_PCIE_PM_CTRL_ENA);
    }
}

/*
 * Recipe from charles to turn off PCIe PHY in PCI mode for power savings
 */
void
ar5416DisablePciePhy(struct ath_hal *ah)
{
#ifndef AH_SERDES_WRITE_SKIP
    if ( AR_SREV_OWL(ah) && AR_SREV_OWL_20_OR_LATER(ah) ) {
        ENABLE_REG_WRITE_BUFFER
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x9248fc00);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x24924924);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x28000029);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x57160824);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x25980579);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x00000000);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x1aaabe40);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0xbe105554);
        OS_REG_WRITE(ah, AR_PCIE_SERDES, 0x000e1007);

        /* Load the new settings */
        OS_REG_WRITE(ah, AR_PCIE_SERDES2, 0x00000000);
        OS_REG_WRITE_FLUSH(ah);
        DISABLE_REG_WRITE_BUFFER
    }
#endif /* AH_SERDES_WRITE_SKIP */
}

static inline int
ar5416GetRadioRev(struct ath_hal *ah)
{
    u_int32_t val;
    int i;

    /* Read Radio Chip Rev Extract */
    ENABLE_REG_WRITE_BUFFER
    OS_REG_WRITE(ah, AR_PHY(0x36), 0x00007058);
    for (i = 0; i < 8; i++)
        OS_REG_WRITE(ah, AR_PHY(0x20), 0x00010000);
    
    OS_REG_WRITE_FLUSH(ah);
    DISABLE_REG_WRITE_BUFFER

    val = (OS_REG_READ(ah, AR_PHY(256)) >> 24) & 0xff;
    val = ((val & 0xf0) >> 4) | ((val & 0x0f) << 4);
    return ath_hal_reverse_bits(val, 8);
}

static inline HAL_STATUS
ar5416RfAttach(struct ath_hal *ah)
{
    HAL_BOOL rfStatus = AH_FALSE;
    HAL_STATUS ecode = HAL_OK;

    rfStatus = ar2133RfAttach(ah, &ecode);
    if (!rfStatus) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: RF setup failed, status %u\n",
            __func__, ecode);
        return ecode;
    }

    return HAL_OK;
}

static inline HAL_STATUS
ar5416InitMacAddr(struct ath_hal *ah)
{
    u_int32_t sum;
    int i;
    u_int16_t eeval;
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t EEP_MAC [] = { EEP_MAC_LSW, EEP_MAC_MID, EEP_MAC_MSW };

    sum = 0;
    for (i = 0; i < 3; i++) {
        eeval = ar5416EepromGet(ahp, EEP_MAC[i]);
        sum += eeval;
        ahp->ah_macaddr[2*i] = eeval >> 8;
        ahp->ah_macaddr[2*i + 1] = eeval & 0xff;
    }
    if (sum == 0 || sum == 0xffff*3) {
        HDPRINTF(ah, HAL_DBG_EEPROM, "%s: mac address read failed: %s\n",
            __func__, ath_hal_ether_sprintf(ahp->ah_macaddr));
        return HAL_EEBADMAC;
    }

    return HAL_OK;
}

/*
 * Code for the "real" chip i.e. non-emulation. Review and revisit
 * when actual hardware is at hand.
 */
static inline HAL_STATUS
ar5416HwAttach(struct ath_hal *ah)
{
    HAL_STATUS ecode;

/* This register R/W test will cost about 0.8 seconds on USB. */
/* And target firmware will do similar test. */
/* So skip register R/W test to accelerate split driver initialization. */
    if (!ar5416ChipTest(ah)) {
        HDPRINTF(ah, HAL_DBG_REG_IO, "%s: hardware self-test failed\n", __func__);
        return HAL_ESELFTEST;
    }

    ecode = ar5416RadioAttach(ah);
    if (ecode != HAL_OK)
        return ecode;

    ecode = ar5416EepromAttach(ah);
    if (ecode != HAL_OK)
        return ecode;
#ifdef ATH_CCX
    ar5416RecordSerialNumber(ah);
#endif
    ecode = ar5416RfAttach(ah);
    if (ecode != HAL_OK)
        return ecode;
    ar5416AniSetup(ah); /* setup 5416-specific ANI tables */
    ar5416AniAttach(ah);
    return HAL_OK;
}

static inline void
ar5416HwDetach(struct ath_hal *ah)
{
    /* XXX EEPROM allocated state */
    ar5416AniDetach(ah);
    ar5416RfDetach(ah);
}

static int16_t
ar5416GetNfAdjust(struct ath_hal *ah, const HAL_CHANNEL_INTERNAL *c)
{
    return 0;
}

#ifdef ATH_CCX
static HAL_BOOL
ar5416RecordSerialNumber(struct ath_hal *ah)
{
    int      i;
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int8_t    *sn = (u_int8_t*)ahp->ah_priv.priv.ser_no;
    u_int8_t    *data = ar5416EepromGetCustData(ahp);
    for (i = 0; i < AR_EEPROM_SERIAL_NUM_SIZE; i++) {
        sn[i] = data[i];
    }

    sn[AR_EEPROM_SERIAL_NUM_SIZE] = '\0';

    return AH_TRUE;
}
#endif

void
ar5416SetImmunity(struct ath_hal *ah, HAL_BOOL enable)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t m1ThreshLow = enable ? 127 : ahp->ah_immunity_vals[0],
              m2ThreshLow = enable ? 127 : ahp->ah_immunity_vals[1],
              m1Thresh = enable ? 127 : ahp->ah_immunity_vals[2],
              m2Thresh = enable ? 127 : ahp->ah_immunity_vals[3],
              m2CountThr = enable ? 31 : ahp->ah_immunity_vals[4],
              m2CountThrLow = enable ? 63 : ahp->ah_immunity_vals[5];

    if (ahp->ah_immunity_on == enable) {
        return;
    }

    ahp->ah_immunity_on = enable;

    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_LOW,
                     AR_PHY_SFCORR_LOW_M1_THRESH_LOW, m1ThreshLow);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_LOW,
                     AR_PHY_SFCORR_LOW_M2_THRESH_LOW, m2ThreshLow);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR,
                     AR_PHY_SFCORR_M1_THRESH, m1Thresh);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR,
                     AR_PHY_SFCORR_M2_THRESH, m2Thresh);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR,
                     AR_PHY_SFCORR_M2COUNT_THR, m2CountThr);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_LOW,
                     AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW, m2CountThrLow);
                                               
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT,
                     AR_PHY_SFCORR_EXT_M1_THRESH_LOW, m1ThreshLow);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT,
                     AR_PHY_SFCORR_EXT_M2_THRESH_LOW, m2ThreshLow);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT,
                     AR_PHY_SFCORR_EXT_M1_THRESH, m1Thresh);
    OS_REG_RMW_FIELD(ah, AR_PHY_SFCORR_EXT,
                     AR_PHY_SFCORR_EXT_M2_THRESH, m2Thresh);

    if (!enable) {
        OS_REG_SET_BIT(ah, AR_PHY_SFCORR_LOW,
                       AR_PHY_SFCORR_LOW_USE_SELF_CORR_LOW);
    } else {
        OS_REG_CLR_BIT(ah, AR_PHY_SFCORR_LOW,
                       AR_PHY_SFCORR_LOW_USE_SELF_CORR_LOW);
    }
}

static int
ar5416GetCalIntervals(struct ath_hal *ah, HAL_CALIBRATION_TIMER **timerp, HAL_CAL_QUERY query)
{
    *timerp = &ar5416_cals;

	switch (query) {
    case HAL_QUERY_CALS:
	    return AR5416_NUM_CAL_TYPES;
    case HAL_QUERY_RERUN_CALS:
	    return 0;
    default:
	    HALASSERT(0);
	}

    return 0;
}

HAL_BOOL
ar5416RegulatoryDomainOverride(struct ath_hal *ah, u_int16_t regdmn)
{
    AH_PRIVATE(ah)->ah_current_rd = regdmn;
    return AH_TRUE;
}

#endif /* AH_SUPPORT_AR5416 */


