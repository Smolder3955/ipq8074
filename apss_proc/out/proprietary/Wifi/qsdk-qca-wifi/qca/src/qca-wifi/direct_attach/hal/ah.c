/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
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
 * Copyright (c) 2010, Atheros Communications Inc. 
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
 */

#include "opt_ah.h"

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"
#include "ah_eeprom.h"

#if AH_DEBUG || AH_PRINT_FILTER
/* HAL custom print function */
static void hal_print(void *ctxt, const char *fmt, va_list args)
{
    ath_hal_vprintf((struct ath_hal *)ctxt, fmt, args);
}

/* HAL global category bit -> name translation */
static const struct asf_print_bit_spec ath_hal_print_categories[] = {
    {HAL_DBG_RESET, "reset"},
    {HAL_DBG_PHY_IO, "phy IO"},
    {HAL_DBG_REG_IO, "register IO"},
    {HAL_DBG_RF_PARAM, "RF parameters"},
    {HAL_DBG_QUEUE, "queue"},
    {HAL_DBG_EEPROM_DUMP, "EEPROM dump"},
    {HAL_DBG_EEPROM, "EEPROM"},
    {HAL_DBG_NF_CAL, "NF cal"},
    {HAL_DBG_CALIBRATE, "calibrate"},
    {HAL_DBG_CHANNEL, "channel"},
    {HAL_DBG_INTERRUPT, "interrupt"},
    {HAL_DBG_DFS, "DFS"},
    {HAL_DBG_DMA, "DMA"},
    {HAL_DBG_REGULATORY, "regulatory"},
    {HAL_DBG_TX, "TX"},
    {HAL_DBG_TXDESC, "TX desc"},
    {HAL_DBG_RX, "RX"},
    {HAL_DBG_RXDESC, "RX desc"},
    {HAL_DBG_ANI, "ANI"},
    {HAL_DBG_BEACON, "beacon"},
    {HAL_DBG_KEYCACHE, "keycache"},
    {HAL_DBG_POWER_MGMT, "power mgmt"},
    {HAL_DBG_MALLOC, "malloc"},
    {HAL_DBG_FORCE_BIAS, "force bias"},
    {HAL_DBG_POWER_OVERRIDE, "power override"},
    {HAL_DBG_SPUR_MITIGATE, "spur mitigate"},
    {HAL_DBG_PRINT_REG, "print reg"},
    {HAL_DBG_TIMER, "timer"},
    {HAL_DBG_BT_COEX, "BT coex"},
    {HAL_DBG_FCS_RTT,"radio retention"},
};

void HDPRINTF(struct ath_hal *ah, unsigned category, const char *fmt, ...)
{    
    va_list args;

    va_start(args, fmt);
    if (ah) {
        asf_vprint_category(&AH_PRIVATE(ah)->ah_asf_print, category, fmt, args);
    } else {
        qdf_vprint(fmt, args);
    }
    va_end(args);
}
#endif

#ifdef AH_SUPPORT_AR5212
extern  struct ath_hal *ar5212Attach(u_int16_t,
    HAL_ADAPTER_HANDLE, HAL_SOFTC,
    HAL_BUS_TAG, HAL_BUS_HANDLE, HAL_BUS_TYPE, 
    struct hal_reg_parm *, HAL_STATUS*);
#endif
#ifdef AH_SUPPORT_AR5416
extern  struct ath_hal *ar5416Attach(u_int16_t,
    HAL_ADAPTER_HANDLE, HAL_SOFTC,
    HAL_BUS_TAG, HAL_BUS_HANDLE, HAL_BUS_TYPE,
    struct hal_reg_parm *, HAL_STATUS*);
#endif

#ifdef AH_SUPPORT_AR9300
extern  struct ath_hal *ar9300_attach(u_int16_t,
    HAL_ADAPTER_HANDLE, HAL_SOFTC,
    HAL_BUS_TAG, HAL_BUS_HANDLE, HAL_BUS_TYPE,
    struct hal_reg_parm *, HAL_STATUS*);
#endif

#include "version.h"
char ath_hal_version[] = ATH_HAL_VERSION;
const char* ath_hal_buildopts[] = {
#ifdef AH_SUPPORT_AR5210
    "AR5210",
#endif
#ifdef AH_SUPPORT_AR5211
    "AR5211",
#endif
#ifdef AH_SUPPORT_AR5212
    "AR5212",
#endif
#ifdef AH_SUPPORT_AR5312
    "AR5312",
#endif
#ifdef AH_SUPPORT_AR5416
    "AR5416",
#endif
#ifdef AH_SUPPORT_AR9300
    "AR9380",
#endif
#ifdef AH_SUPPORT_5111
    "RF5111",
#endif
#ifdef AH_SUPPORT_5112
    "RF5112",
#endif
#ifdef AH_SUPPORT_2413
    "RF2413",
#endif
#ifdef AH_SUPPORT_5413
    "RF5413",
#endif
#ifdef AH_SUPPORT_2316
    "RF2316",
#endif
#ifdef AH_SUPPORT_2317
    "RF2317",
#endif
#ifdef AH_DEBUG
    "DEBUG",
#endif
#ifdef AH_ASSERT
    "ASSERT",
#endif
#ifdef AH_DEBUG_ALQ
    "DEBUG_ALQ",
#endif
#ifdef AH_REGOPS_FUNC
    "REGOPS_FUNC",
#endif
#ifdef AH_ENABLE_TX_TPC
    "ENABLE_TX_TPC",
#endif
#ifdef AH_PRIVATE_DIAG
    "PRIVATE_DIAG",
#endif
#ifdef AH_SUPPORT_WRITE_EEPROM
    "WRITE_EEPROM",
#endif
#ifdef AH_WRITE_REGDOMAIN
    "WRITE_REGDOMAIN",
#endif
#ifdef AH_NEED_DESC_SWAP
    "TX_DESC_SWAP",
#endif
#ifdef AH_NEED_TX_DATA_SWAP
    "TX_DATA_SWAP",
#endif
#ifdef AH_NEED_RX_DATA_SWAP
    "RX_DATA_SWAP",
#endif
#ifdef AR5416_ISR_READ_CLEAR_SUPPORT
    "AR5416_ISR_READ_CLEAR_SUPPORT",
#endif
#ifdef AH_SUPPORT_DFS
    "DFS",
   #if AH_RADAR_CALIBRATE >= 3
    "RADAR_CALIBRATE (detailed)",
   #elif AH_RADAR_CALIBRATE >= 2
    "RADAR_CALIBRATE (minimal)",
   #elif AH_RADAR_CALIBRATE > 0
    "RADAR_CALIBRATE",
   #endif
   #endif /* AH_SUPPORT_DFS */
#ifdef AH_SUPPORT_WRITE_REG
    "WRITE_REG",
#endif
#ifdef AH_DISABLE_WME
    "DISABLE_WME",
#endif

#ifdef AH_SUPPORT_11D
    "11D",
#endif
    AH_NULL
};

#if !NO_HAL
static const char*
ath_hal_devname(u_int16_t devid)
{
    switch (devid) {
    case AR5210_PROD:
    case AR5210_DEFAULT:
        return "Atheros 5210";

    case AR5211_DEVID:
    case AR5311_DEVID:
    case AR5211_DEFAULT:
        return "Atheros 5211";
    case AR5211_FPGA11B:
        return "Atheros 5211 (FPGA)";

    case AR5212_FPGA:
        return "Atheros 5212 (FPGA)";
    case AR5212_AR5312_REV2:
    case AR5212_AR5312_REV7:
        return "Atheros 5312 WiSoC";
    case AR5212_AR2315_REV6:
    case AR5212_AR2315_REV7:
        return "Atheros 2315 WiSoC";
    case AR5212_AR2317_REV1:
    case AR5212_AR2317_REV2:
        return "Atheros 2317 WiSoC";
    case AR5212_AR2313_REV8:
        return "Atheros 2313 WiSoC";
	case AR5212_AR5424:
	case AR5212_DEVID_FF19:
		return "Atheros 5424";
    case AR5212_DEVID:
    case AR5212_DEVID_IBM:
    case AR5212_AR2413:
    case AR5212_AR5413:
    case AR5212_DEFAULT:
        return "Atheros 5212";
    case AR5212_AR2417:
        return "Atheros 5212/2417";
#ifdef AH_SUPPORT_AR5416
    case AR5416_DEVID_PCI:
    case AR5416_DEVID_PCIE:
        return "Atheros 5416";
    case AR5416_DEVID_AR9160_PCI:
        return "Atheros 9160";
	case AR5416_AR9100_DEVID:
		return "Atheros AR9100 WiSoC";
    case AR5416_DEVID_AR9280_PCI:
    case AR5416_DEVID_AR9280_PCIE:
        return "Atheros 9280";
	case AR5416_DEVID_AR9285_PCIE:
        return "Atheros 9285";
    case AR5416_DEVID_AR9285G_PCIE:
        return "Atheros 9285G";
	case AR5416_DEVID_AR9287_PCI:
	case AR5416_DEVID_AR9287_PCIE:
        return "Atheros 9287";

#endif
	case AR9300_DEVID_AR9380_PCIE:
        return "Atheros 9380";
	case AR9300_DEVID_AR9340:
        return "Atheros 9340";
	case AR9300_DEVID_AR9485_PCIE:
        return "Atheros 9485";
    case AR9300_DEVID_AR9580_PCIE:
        return "Atheros 9580";
    case AR9300_DEVID_AR1111_PCIE:
        return "Atheros 1111";
    case AR9300_DEVID_AR946X_PCIE:
        return "Atheros 946X";
    case AR9300_DEVID_AR956X_PCIE:
        return "Atheros 956X";
    case AR9300_DEVID_AR956X:
        return "Atheros 956X";
    case AR9300_DEVID_EMU_PCIE:
        return "Atheros pcie emulation";
    }
    return AH_NULL;
}

const char* __ahdecl
ath_hal_probe(u_int16_t vendorid, u_int16_t devid)
{
    return (vendorid == ATHEROS_VENDOR_ID ||
        vendorid == ATHEROS_3COM_VENDOR_ID ||
        vendorid == ATHEROS_3COM2_VENDOR_ID ?
            ath_hal_devname(devid) : 0);
}

#endif
/*
 * Attach detects device chip revisions, initializes the hwLayer
 * function list, reads EEPROM information,
 * selects reset vectors, and performs a short self test.
 * Any failures will return an error that should cause a hardware
 * disable.
 */
#if !NO_HAL
struct ath_hal* __ahdecl
ath_hal_attach(u_int16_t devid, HAL_ADAPTER_HANDLE osdev, HAL_SOFTC sc,
    HAL_BUS_TAG st, HAL_BUS_HANDLE sh, HAL_BUS_TYPE bustype,
    struct hal_reg_parm *hal_conf_parm, HAL_STATUS *error)
{
    struct ath_hal *ah=AH_NULL;

    switch (devid) {
#ifdef AH_SUPPORT_AR5212
    case AR5212_DEVID_IBM:
    case AR5212_AR2413:
    case AR5212_AR5413:
    case AR5212_AR5424:
    case AR5212_DEVID_FF19: /* XXX PCI Express extra */
        devid = AR5212_DEVID;
        /* fall thru... */
    case AR5212_AR2417:
    case AR5212_DEVID:
    case AR5212_FPGA:
    case AR5212_DEFAULT:
        ah = ar5212Attach(devid, osdev, sc, st, sh, bustype,
                          hal_conf_parm, error);
        break;
#endif

#ifdef AH_SUPPORT_AR5416
	case AR5416_DEVID_PCI:
	case AR5416_DEVID_PCIE:
	case AR5416_DEVID_AR9160_PCI:
	case AR5416_AR9100_DEVID:
    case AR5416_DEVID_AR9280_PCI:
    case AR5416_DEVID_AR9280_PCIE:
	case AR5416_DEVID_AR9285_PCIE:
    case AR5416_DEVID_AR9285G_PCIE:
	case AR5416_DEVID_AR9287_PCI:
	case AR5416_DEVID_AR9287_PCIE:
        ah = ar5416Attach(devid, osdev, sc, st, sh, bustype,
                          hal_conf_parm, error);
                break;
#endif
#ifdef AH_SUPPORT_AR9300
    case AR9300_DEVID_AR9380_PCIE:
    case AR9300_DEVID_AR9340:
    case AR9300_DEVID_AR9485_PCIE:
    case AR9300_DEVID_AR9580_PCIE:
    case AR9300_DEVID_AR1111_PCIE:
    case AR9300_DEVID_AR946X_PCIE:
#ifdef AH_SUPPORT_SCORPION
    case AR9300_DEVID_AR955X:
#endif
#ifdef AH_SUPPORT_HONEYBEE
    case AR9300_DEVID_AR953X:
#endif
#ifdef AH_SUPPORT_DRAGONFLY
    case AR9300_DEVID_AR956X:
#endif
    case AR9300_DEVID_AR956X_PCIE:
    /* WAR for EV 87973. 
     * Enable support for emulation device assuming that it is AR9300.
     * No need for conditional compile with #ifdef AR9300_EMULATION
     */
    case AR9300_DEVID_EMU_PCIE:
#if ATH_DRIVER_SIM
    case AR9300_DEVID_SIM_PCIE:
#endif
        ah = ar9300_attach(devid, osdev, sc, st, sh, bustype,
                          hal_conf_parm, error);
        break;
#endif
    default:
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "devid=0x%x not supported.\n",devid);
        ah = AH_NULL;
        *error = HAL_ENXIO;
        break;
    }
    if (ah != AH_NULL) {
        /* copy back private state to public area */
#if ICHAN_WAR_SYNCH
        AH_PRIVATE(ah)->ah_ichan_set = false;
        spin_lock_init(&AH_PRIVATE(ah)->ah_ichan_lock);
#endif
        /* initialize HDPRINTF control object */
        ath_hal_hdprintf_init(ah);
    }
    return ah;
}
#endif

 /*
  * Set the vendor ID for the driver.  This is used by the
  * regdomain to make vendor specific changes.
  */
bool __ahdecl
 ath_hal_setvendor(struct ath_hal *ah, u_int32_t vendor)
 {
    AH_PRIVATE(ah)->ah_vendor = vendor;
    return true;
 }


/*
 * Poll the register looking for a specific value.
 * Waits up to timeout (usec).
 */
bool
ath_hal_wait(struct ath_hal *ah, u_int reg, u_int32_t mask, u_int32_t val,
             u_int32_t timeout)
{
/* 
 * WDK documentation states execution should not be stalled for more than 
 * 10us at a time.
 */
#define AH_TIME_QUANTUM        10
    int i;

#if ATH_DRIVER_SIM
    if (1) {
        return true;
    }
#endif

    HALASSERT(timeout >= AH_TIME_QUANTUM);

    for (i = 0; i < (timeout / AH_TIME_QUANTUM); i++) {
        if ((OS_REG_READ(ah, reg) & mask) == val)
            return true;
        OS_DELAY(AH_TIME_QUANTUM);
    }
    HDPRINTF(ah, HAL_DBG_PHY_IO,
             "%s: timeout (%d us) on reg 0x%x: 0x%08x & 0x%08x != 0x%08x\n",
            __func__,
            timeout,
            reg, 
            OS_REG_READ(ah, reg), mask, val);
    return false;
#undef AH_TIME_QUANTUM
}

/*
 * Reverse the bits starting at the low bit for a value of
 * bit_count in size
 */
u_int32_t
ath_hal_reverse_bits(u_int32_t val, u_int32_t n)
{
    u_int32_t retval;
    int i;

    for (i = 0, retval = 0; i < n; i++) {
        retval = (retval << 1) | (val & 1);
        val >>= 1;
    }
    return retval;
}

/*
 * Compute the time to transmit a frame of length frameLen bytes
 * using the specified rate, phy, and short preamble setting.
 */
#if !NO_HAL
u_int16_t __ahdecl
ath_hal_computetxtime(struct ath_hal *ah,
    const HAL_RATE_TABLE *rates, u_int32_t frameLen, u_int16_t rateix,
    bool shortPreamble)
{
    u_int32_t bitsPerSymbol, num_bits, numSymbols, phyTime, txTime;
    u_int32_t kbps;

    kbps = rates->info[rateix].rateKbps;
    /*
     * index can be invalid duting dynamic Turbo transitions. 
     */
    if(kbps == 0) return 0;
    switch (rates->info[rateix].phy) {

    case IEEE80211_T_CCK:
#define CCK_SIFS_TIME        10
#define CCK_PREAMBLE_BITS   144
#define CCK_PLCP_BITS        48
        phyTime     = CCK_PREAMBLE_BITS + CCK_PLCP_BITS;
        if (shortPreamble && rates->info[rateix].shortPreamble)
            phyTime >>= 1;
        num_bits     = frameLen << 3;
        txTime      = phyTime
                + ((num_bits * 1000)/kbps);
        break;
#undef CCK_SIFS_TIME
#undef CCK_PREAMBLE_BITS
#undef CCK_PLCP_BITS

    case IEEE80211_T_OFDM:
/* #define OFDM_SIFS_TIME        16 earlier value see EV# 94662*/
#define OFDM_SIFS_TIME        6
#define OFDM_PREAMBLE_TIME    20
#define OFDM_PLCP_BITS        22
#define OFDM_SYMBOL_TIME       4

#define OFDM_SIFS_TIME_HALF 32
#define OFDM_PREAMBLE_TIME_HALF 40
#define OFDM_PLCP_BITS_HALF 22
#define OFDM_SYMBOL_TIME_HALF   8

#define OFDM_SIFS_TIME_QUARTER      64
#define OFDM_PREAMBLE_TIME_QUARTER  80
#define OFDM_PLCP_BITS_QUARTER      22
#define OFDM_SYMBOL_TIME_QUARTER    16
#define OFDM_DIFF_SIFS_TIME         6
        if (AH_PRIVATE(ah)->ah_curchan && 
            IS_CHAN_QUARTER_RATE(AH_PRIVATE(ah)->ah_curchan)) {
            bitsPerSymbol   = (kbps * OFDM_SYMBOL_TIME_QUARTER) / 1000;
            HALASSERT(bitsPerSymbol != 0);

            num_bits     = OFDM_PLCP_BITS + (frameLen << 3);
            numSymbols  = howmany(num_bits, bitsPerSymbol);
            txTime      = OFDM_SIFS_TIME_QUARTER 
                        + OFDM_PREAMBLE_TIME_QUARTER
                    + (numSymbols * OFDM_SYMBOL_TIME_QUARTER);
        } else if (AH_PRIVATE(ah)->ah_curchan &&
                IS_CHAN_HALF_RATE(AH_PRIVATE(ah)->ah_curchan)) {
            bitsPerSymbol   = (kbps * OFDM_SYMBOL_TIME_HALF) / 1000;
            HALASSERT(bitsPerSymbol != 0);

            num_bits     = OFDM_PLCP_BITS + (frameLen << 3);
            numSymbols  = howmany(num_bits, bitsPerSymbol);
            txTime      = OFDM_SIFS_TIME_HALF + 
                        OFDM_PREAMBLE_TIME_HALF
                    + (numSymbols * OFDM_SYMBOL_TIME_HALF);
        } else { /* full rate channel */
            bitsPerSymbol   = (kbps * OFDM_SYMBOL_TIME) / 1000;
            HALASSERT(bitsPerSymbol != 0);

            num_bits     = OFDM_PLCP_BITS + (frameLen << 3);
            numSymbols  = howmany(num_bits, bitsPerSymbol);
            txTime      = OFDM_SIFS_TIME + OFDM_PREAMBLE_TIME
                    + (numSymbols * OFDM_SYMBOL_TIME);
        }
        break;

#undef OFDM_SIFS_TIME
#undef OFDM_PREAMBLE_TIME
#undef OFDM_PLCP_BITS
#undef OFDM_SYMBOL_TIME
    case IEEE80211_T_TURBO:
#define TURBO_SIFS_TIME         8
#define TURBO_PREAMBLE_TIME    14
#define TURBO_PLCP_BITS        22
#define TURBO_SYMBOL_TIME       4
        /* we still save OFDM rates in kbps - so double them */
        bitsPerSymbol = ((kbps << 1) * TURBO_SYMBOL_TIME) / 1000;
        HALASSERT(bitsPerSymbol != 0);

        num_bits       = TURBO_PLCP_BITS + (frameLen << 3);
        numSymbols    = howmany(num_bits, bitsPerSymbol);
        txTime        = TURBO_SIFS_TIME + TURBO_PREAMBLE_TIME
                  + (numSymbols * TURBO_SYMBOL_TIME);
        break;
#undef TURBO_SIFS_TIME
#undef TURBO_PREAMBLE_TIME
#undef TURBO_PLCP_BITS
#undef TURBO_SYMBOL_TIME

    case ATHEROS_T_XR:
#define XR_SIFS_TIME            16
#define XR_PREAMBLE_TIME(_kpbs) (((_kpbs) < 1000) ? 173 : 76)
#define XR_PLCP_BITS            22
#define XR_SYMBOL_TIME           4
        bitsPerSymbol = (kbps * XR_SYMBOL_TIME) / 1000;
        HALASSERT(bitsPerSymbol != 0);

        num_bits       = XR_PLCP_BITS + (frameLen << 3);
        numSymbols    = howmany(num_bits, bitsPerSymbol);
        txTime        = XR_SIFS_TIME + XR_PREAMBLE_TIME(kbps)
                   + (numSymbols * XR_SYMBOL_TIME);
        break;
#undef XR_SIFS_TIME
#undef XR_PREAMBLE_TIME
#undef XR_PLCP_BITS
#undef XR_SYMBOL_TIME

    default:
        HDPRINTF(ah, HAL_DBG_PHY_IO, "%s: unknown phy %u (rate ix %u)\n",
            __func__, rates->info[rateix].phy, rateix);
        txTime = 0;
        break;
    }
    return txTime;
}
#endif

/* The following 3 functions have a lot of overlap.
   If possible, they should be combined into a single function.
   DCD 4/2/09
 */

WIRELESS_MODE
ath_hal_chan2wmode(struct ath_hal *ah, const HAL_CHANNEL *chan)
{
    if (IS_CHAN_CCK(chan))
        return WIRELESS_MODE_11b;
    if (IS_CHAN_G(chan))
        return WIRELESS_MODE_11g;
    if (IS_CHAN_108G(chan))
        return WIRELESS_MODE_108g;
    if (IS_CHAN_TURBO(chan))
        return WIRELESS_MODE_TURBO;
    if (IS_CHAN_XR(chan))
        return WIRELESS_MODE_XR;
    return WIRELESS_MODE_11a;
}


/* Note: WIRELESS_MODE defined in ah_internal.h is used here 
         not that enumerated in ath_dev.h */

WIRELESS_MODE
ath_hal_chan2htwmode(struct ath_hal *ah, const HAL_CHANNEL *chan)
{
    if (IS_CHAN_CCK(chan))
        return WIRELESS_MODE_11b;
    /* Tag HT channels with _NG / _NA, instead of falling through to _11g/_11a */
    if (IS_CHAN_HT(chan) && IS_CHAN_G(chan))
        return WIRELESS_MODE_11NG;
    if (IS_CHAN_HT(chan) && IS_CHAN_A(chan))
        return WIRELESS_MODE_11NA;
    if (IS_CHAN_G(chan))
        return WIRELESS_MODE_11g;
    if (IS_CHAN_108G(chan))
        return WIRELESS_MODE_108g;
    if (IS_CHAN_TURBO(chan))
        return WIRELESS_MODE_TURBO;
    if (IS_CHAN_XR(chan))
        return WIRELESS_MODE_XR;
    return WIRELESS_MODE_11a;
}

u_int
ath_hal_get_curmode(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
    /*
     * Pick a default mode at bootup. A channel change is inevitable.
     */
    if (!chan) return HAL_MODE_11NG_HT20;

    /* check for NA_HT before plain A, since IS_CHAN_A includes NA_HT */
    if (IS_CHAN_NA_20(chan))      return HAL_MODE_11NA_HT20;
    if (IS_CHAN_NA_40PLUS(chan))  return HAL_MODE_11NA_HT40PLUS;
    if (IS_CHAN_NA_40MINUS(chan)) return HAL_MODE_11NA_HT40MINUS;
    if (IS_CHAN_A(chan))          return HAL_MODE_11A;

    /* check for NG_HT before plain G, since IS_CHAN_G includes NG_HT */
    if (IS_CHAN_NG_20(chan))      return HAL_MODE_11NG_HT20;
    if (IS_CHAN_NG_40PLUS(chan))  return HAL_MODE_11NG_HT40PLUS;
    if (IS_CHAN_NG_40MINUS(chan)) return HAL_MODE_11NG_HT40MINUS;
    if (IS_CHAN_G(chan))          return HAL_MODE_11G;

    if (IS_CHAN_B(chan))          return HAL_MODE_11B;

    /*
     * TODO: Current callers don't care about SuperX.
     */
    HALASSERT(0);
    return HAL_MODE_11NG_HT20;
}

/*
 * Convert GHz frequency to IEEE channel number.
 */
#if !NO_HAL
u_int __ahdecl
ath_hal_mhz2ieee(struct ath_hal *ah, u_int freq, u_int flags)
{
    if (flags & CHANNEL_2GHZ) { /* 2GHz band */
        if (freq == 2484)
            return 14;
        if (freq < 2484)
            return (freq - 2407) / 5;
        else
            return 15 + ((freq - 2512) / 20);
    } else if (flags & CHANNEL_5GHZ) {/* 5Ghz band */
        if (ath_hal_ispublicsafetysku(ah) && 
            IS_CHAN_IN_PUBLIC_SAFETY_BAND(freq)) {
            return ((freq * 10) + 
                (((freq % 5) == 2) ? 5 : 0) - 49400) / 5;
        } else if ((flags & CHANNEL_A) && (freq <= 5000)) {
            return (freq - 4000) / 5;
        } else {
            return (freq - 5000) / 5;
        }
    } else {            /* either, guess */
        if (freq == 2484)
            return 14;
        if (freq < 2484)
            return (freq - 2407) / 5;
        if (freq < 5000) {
            if (ath_hal_ispublicsafetysku(ah) 
                && IS_CHAN_IN_PUBLIC_SAFETY_BAND(freq)) {
                return ((freq * 10) +   
                    (((freq % 5) == 2) ? 5 : 0) - 49400)/5;
            } else if (freq > 4900) {
                return (freq - 4000) / 5;
            } else {
                return 15 + ((freq - 2512) / 20);
            }
        }
        return (freq - 5000) / 5;
    }
}
#endif

/*
 * Convert between microseconds and core system clocks.
 */
                                     
/* PG: Added support for determining different clock rates
       for 11NGHT20, 11NAHT20, 11NGHT40PLUS/MINUS, 11NAHT40PLUS/MINUS */

                                          /* 11a Turbo  11b  11g  108g  XR  NA  NG */
static const u_int8_t CLOCK_RATE[]  =      { 40,  80,   44,  44,   88,  40, 40, 44 };
static const u_int8_t FAST_CLOCK_RATE[]  = { 44,  80,   44,  44,   88,  40, 44, 44 };

#define ath_hal_is_fast_clock_en(_ah)   ((*(_ah)->ah_is_fast_clock_enabled)((ah)))

u_int
ath_hal_mac_clks(struct ath_hal *ah, u_int usecs)
{
    const HAL_CHANNEL *c = (const HAL_CHANNEL *) AH_PRIVATE(ah)->ah_curchan;

    /* NB: ah_curchan may be null when called attach time */
    if (c != AH_NULL) {
        if (true == ath_hal_is_fast_clock_en(ah)) {
            if (IS_CHAN_HALF_RATE(c))
                return usecs * FAST_CLOCK_RATE[ath_hal_chan2wmode(ah, c)] / 2;
            else if (IS_CHAN_QUARTER_RATE(c))
                return usecs * FAST_CLOCK_RATE[ath_hal_chan2wmode(ah, c)] / 4;
            else
                return usecs * FAST_CLOCK_RATE[ath_hal_chan2wmode(ah, c)];
        } else {
            if (IS_CHAN_HALF_RATE(c))
                return usecs * CLOCK_RATE[ath_hal_chan2wmode(ah, c)] / 2;
            else if (IS_CHAN_QUARTER_RATE(c))
                return usecs * CLOCK_RATE[ath_hal_chan2wmode(ah, c)] / 4;
            else
                return usecs * CLOCK_RATE[ath_hal_chan2wmode(ah, c)];
        }
    } else
        return usecs * CLOCK_RATE[WIRELESS_MODE_11b];
}

u_int
ath_hal_mac_usec(struct ath_hal *ah, u_int clks)
{
    const HAL_CHANNEL *c = (const HAL_CHANNEL *) AH_PRIVATE(ah)->ah_curchan;

    /* NB: ah_curchan may be null when called attach time */
    if (c != AH_NULL) {
        if (true == ath_hal_is_fast_clock_en(ah)) {
            return clks / FAST_CLOCK_RATE[ath_hal_chan2wmode(ah, c)];
        } else {
            return clks / CLOCK_RATE[ath_hal_chan2wmode(ah, c)];
        }
    } else
        return clks / CLOCK_RATE[WIRELESS_MODE_11b];
}

u_int8_t
ath_hal_chan_2_clock_rate_mhz(struct ath_hal *ah)
{
        const HAL_CHANNEL *c = (const HAL_CHANNEL *) AH_PRIVATE(ah)->ah_curchan;
        u_int8_t clockrateMHz;

        if (true == ath_hal_is_fast_clock_en(ah)) {
            clockrateMHz = FAST_CLOCK_RATE[ath_hal_chan2htwmode(ah,c)];
        } else {
            clockrateMHz = CLOCK_RATE[ath_hal_chan2htwmode(ah,c)];
        }

        if (c && IS_CHAN_HT40(c))
            return (clockrateMHz * 2);
        else
            return (clockrateMHz);
}

HAL_STATUS 
ath_hal_set_itf_prefix_name(struct ath_hal *ah, char *prefix, int str_len)
{
    if (str_len > 7) { 
        return HAL_EINVAL;
    }    
#ifdef AH_DEBUG
    strcpy(AH_PRIVATE(ah)->ah_print_itf_name, prefix);
#endif
    return HAL_OK;
}

/*
 * Setup a h/w rate table's reverse lookup table and
 * fill in ack durations.  This routine is called for
 * each rate table returned through the ah_get_rate_table
 * method.  The reverse lookup tables are assumed to be
 * initialized to zero (or at least the first entry).
 * We use this as a key that indicates whether or not
 * we've previously setup the reverse lookup table.
 *
 * XXX not reentrant, but shouldn't matter
 */
void
ath_hal_setupratetable(struct ath_hal *ah, HAL_RATE_TABLE *rt)
{
    int i;

    if (rt->rateCodeToIndex[0] != 0)    /* already setup */
        return;
    for (i = 0; i < 256; i++)
        rt->rateCodeToIndex[i] = (u_int8_t) -1;
    for (i = 0; i < rt->rateCount; i++) {
        u_int8_t code = rt->info[i].rate_code;
        u_int8_t cix = rt->info[i].controlRate;

        rt->rateCodeToIndex[code] = i;
        rt->rateCodeToIndex[code | rt->info[i].shortPreamble] = i;
        /*
         * XXX for 11g the control rate to use for 5.5 and 11 Mb/s
         *     depends on whether they are marked as basic rates;
         *     the static tables are setup with an 11b-compatible
         *     2Mb/s rate which will work but is suboptimal
         */
        rt->info[i].lpAckDuration = ath_hal_computetxtime(ah, rt,
            WLAN_CTRL_FRAME_SIZE, cix, false);
        rt->info[i].spAckDuration = ath_hal_computetxtime(ah, rt,
            WLAN_CTRL_FRAME_SIZE, cix, true);
    }
}

HAL_STATUS
ath_hal_getcapability(struct ath_hal *ah, HAL_CAPABILITY_TYPE type,
    u_int32_t capability, u_int32_t *result)
{
    const HAL_CAPABILITIES *p_cap = &AH_PRIVATE(ah)->ah_caps;

    switch (type) {
    case HAL_CAP_REG_DMN:       /* regulatory domain */
        *result = AH_PRIVATE(ah)->ah_current_rd;
        return HAL_OK;
    case HAL_CAP_DFS_DMN:
        *result = AH_PRIVATE(ah)->ah_dfsDomain;
        return HAL_OK;
    case HAL_CAP_COMBINED_RADAR_RSSI:
         return (p_cap->hal_use_combined_radar_rssi ? HAL_OK : HAL_ENOTSUPP);
#if ATH_SUPPORT_WRAP
    case HAL_CAP_WRAP_HW_DECRYPT:
        if (ath_hal_Honeybee11_later(ah))
        {
                                /*Honeybee 1.1 and later revisions can support HW decryption in WRAP mode*/
            return HAL_OK;
        } else {
            return HAL_ENOTSUPP;
        }
    case HAL_CAP_WRAP_PROMISC:
        if (!ath_hal_Honeybee20(ah))
        {
            return HAL_OK;
        } else {
                                /*Promiscuous mode was disabled for Honeybee 2.0*/
            return HAL_ENOTSUPP;
        }
#endif
    case HAL_CAP_EXT_CHAN_DFS:
         return (p_cap->hal_ext_chan_dfs_support ? HAL_OK : HAL_ENOTSUPP);
    case HAL_CAP_CIPHER:        /* cipher handled in hardware */
    case HAL_CAP_TKIP_MIC:      /* handle TKIP MIC in hardware */
        return HAL_ENOTSUPP;
    case HAL_CAP_TKIP_SPLIT:    /* hardware TKIP uses split keys */
        return HAL_ENOTSUPP;
    case HAL_CAP_WME_TKIPMIC:   /* hardware can do TKIP MIC when WMM is turned on */
        return HAL_ENOTSUPP;
    case HAL_CAP_DIVERSITY:     /* hardware supports fast diversity */
        return HAL_ENOTSUPP;
    case HAL_CAP_KEYCACHE_SIZE: /* hardware key cache size */
        *result =  p_cap->hal_key_cache_size;
        return HAL_OK;
    case HAL_CAP_NUM_TXQUEUES:  /* number of hardware tx queues */
        *result = p_cap->hal_total_queues;
        return HAL_OK;
    case HAL_CAP_VEOL:      /* hardware supports virtual EOL */
        return p_cap->hal_veol_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_PSPOLL:        /* hardware PS-Poll support works */
        return p_cap->hal_ps_poll_broken ? HAL_ENOTSUPP : HAL_OK;
    case HAL_CAP_BURST:
        return p_cap->hal_burst_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_FASTFRAME:
        return p_cap->hal_fast_frames_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_DIAG:      /* hardware diagnostic support */
        *result = AH_PRIVATE(ah)->ah_diagreg;
        return HAL_OK;
    case HAL_CAP_TXPOW:     /* global tx power limit  */
        switch (capability) {
        case 0:         /* facility is supported */
            return HAL_OK;
        case 1:         /* current limit */
            *result = AH_PRIVATE(ah)->ah_power_limit;
            return HAL_OK;
        case 2:         /* current max tx power */
            *result = AH_PRIVATE(ah)->ah_max_power_level;
            return HAL_OK;
        case 3:         /* scale factor */
            *result = AH_PRIVATE(ah)->ah_tp_scale;
            return HAL_OK;
        case 4:         /* extra power for certain rates */
            *result = AH_PRIVATE(ah)->ah_extra_txpow;
            return HAL_OK;
        case 5:        /* power reduction */
            *result = AH_PRIVATE(ah)->ah_power_scale;
            return HAL_OK;
        }
        return HAL_ENOTSUPP;
    case HAL_CAP_BSSIDMASK:     /* hardware supports bssid mask */
        return p_cap->hal_bss_id_mask_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_MCAST_KEYSRCH: /* multicast frame keycache search */
        return p_cap->hal_mcast_key_srch_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_TSF_ADJUST:    /* hardware has beacon tsf adjust */
        return HAL_ENOTSUPP;
    case HAL_CAP_XR:
        return p_cap->hal_xr_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_CHAN_HALFRATE:
        return p_cap->hal_chan_half_rate ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_CHAN_QUARTERRATE:
        return p_cap->hal_chan_quarter_rate ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_RFSILENT:      /* rfsilent support  */
        switch (capability) {
        case 0:         /* facility is supported */
            return p_cap->hal_rf_silent_support ? HAL_OK : HAL_ENOTSUPP;
        case 1:         /* current setting */
            return AH_PRIVATE(ah)->ah_rfkillEnabled ?
                HAL_OK : HAL_ENOTSUPP;
        case 2:         /* rfsilent config */
            /*
             * lower 16 bits for GPIO pin,
             * and upper 16 bits for GPIO polarity
             */
            *result = MS(AH_PRIVATE(ah)->ah_rfsilent, AR_EEPROM_RFSILENT_POLARITY);
            *result <<= 16;
            *result |= MS(AH_PRIVATE(ah)->ah_rfsilent, AR_EEPROM_RFSILENT_GPIO_SEL);
            return HAL_OK;
        case 3:         /* use GPIO INT to detect rfkill switch */
        default:
            return HAL_ENOTSUPP;
        }
    case HAL_CAP_11D:
#ifdef AH_SUPPORT_11D
        return HAL_OK;
#else
        return HAL_ENOTSUPP;
#endif

    case HAL_CAP_PCIE_PS:
        switch (capability) {
        case 0:
            return AH_PRIVATE(ah)->ah_is_pci_express ? HAL_OK : HAL_ENOTSUPP;
        case 1:
            return (AH_PRIVATE(ah)->ah_config.ath_hal_pcie_power_save_enable == 1) ? HAL_OK : HAL_ENOTSUPP;
        }
        return HAL_ENOTSUPP;

    case HAL_CAP_HT:
        return p_cap->hal_ht_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_HT20_SGI:
        return p_cap->hal_ht20_sgi_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_GTT:
        return p_cap->hal_gtt_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_FAST_CC:
        return p_cap->hal_fast_cc_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_TX_CHAINMASK:
        *result = p_cap->hal_tx_chain_mask;
        return HAL_OK;
    case HAL_CAP_RX_CHAINMASK:
        *result = p_cap->hal_rx_chain_mask;
        return HAL_OK;
    case HAL_CAP_TX_TRIG_LEVEL_MAX:
        *result = p_cap->hal_tx_trig_level_max;
        return HAL_OK;
    case HAL_CAP_NUM_GPIO_PINS:
        *result = p_cap->hal_num_gpio_pins;
        return HAL_OK;
#if ATH_WOW                
    case HAL_CAP_WOW:
        return p_cap->hal_wow_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_WOW_MATCH_EXACT:
        return p_cap->hal_wow_match_pattern_exact ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_WOW_MATCH_DWORD:
        return p_cap->hal_wow_pattern_match_dword ? HAL_OK : HAL_ENOTSUPP;
#endif                
    case HAL_CAP_CST:
        return p_cap->hal_cst_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_RIFS_RX:
        return p_cap->hal_rifs_rx_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_RIFS_TX:
        return p_cap->hal_rifs_tx_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_FORCE_PPM:
        return p_cap->halforce_ppm_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_RTS_AGGR_LIMIT:
        *result = p_cap->hal_rts_aggr_limit;
        return p_cap->hal_rts_aggr_limit ? HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_HW_BEACON_PROC_SUPPORT:
		return p_cap->hal_hw_beacon_proc_support ? HAL_OK : HAL_ENOTSUPP;
	case HAL_CAP_AUTO_SLEEP:
        return p_cap->hal_auto_sleep_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_MBSSID_AGGR_SUPPORT:   
        return p_cap->hal_mbssid_aggr_support ? HAL_OK : HAL_ENOTSUPP;
 	case HAL_CAP_4KB_SPLIT_TRANS:
        return p_cap->hal4kb_split_trans_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_REG_FLAG:
        *result = p_cap->hal_reg_cap;
        return HAL_OK;
    case HAL_CAP_ANT_CFG_5GHZ:
    case HAL_CAP_ANT_CFG_2GHZ:
        *result = 1; /* Default only one configuration */
        return HAL_OK;
    case HAL_CAP_MFP:  /* MFP option configured */
        *result = p_cap->hal_mfp_support;
        return HAL_OK;
    case HAL_CAP_RX_STBC:
        return HAL_ENOTSUPP;
    case HAL_CAP_TX_STBC:
        return HAL_ENOTSUPP;
    case HAL_CAP_LDPC:
        return HAL_ENOTSUPP;
#ifdef ATH_BT_COEX
    case HAL_CAP_BT_COEX:
        return p_cap->hal_bt_coex_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_BT_COEX_ASPM_WAR:
        return p_cap->hal_bt_coex_aspm_war ? HAL_OK : HAL_ENOTSUPP;
#endif
    case HAL_CAP_WPS_PUSH_BUTTON:
        return p_cap->hal_wps_push_button ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_ENHANCED_DMA_SUPPORT:
        return p_cap->hal_enhanced_dma_support ? HAL_OK : HAL_ENOTSUPP;
#ifdef ATH_SUPPORT_DFS
    case HAL_CAP_ENHANCED_DFS_SUPPORT:
        return p_cap->hal_enhanced_dfs_support ? HAL_OK : HAL_ENOTSUPP;
#endif
    case HAL_CAP_PROXYSTA:
        return p_cap->hal_proxy_sta_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_NUM_TXMAPS:
        *result = p_cap->hal_num_tx_maps;
        return p_cap->hal_num_tx_maps ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_TXDESCLEN:
        *result = p_cap->hal_tx_desc_len;
        return p_cap->hal_tx_desc_len ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_TXSTATUSLEN:
        *result = p_cap->hal_tx_status_len;
        return p_cap->hal_tx_status_len ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_RXSTATUSLEN:
        *result = p_cap->hal_rx_status_len;
        return p_cap->hal_rx_status_len ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_RXFIFODEPTH:
        switch (capability) {
        case HAL_RX_QUEUE_HP:
            *result = p_cap->hal_rx_hp_depth;
            break;
        case HAL_RX_QUEUE_LP:
            *result = p_cap->hal_rx_lp_depth;
            break;
        default:
            return HAL_ENOTSUPP;
        }
        return HAL_OK;
    case HAL_CAP_WEP_TKIP_AGGR:
        return p_cap->hal_wep_tkip_aggr_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_WEP_TKIP_AGGR_TX_DELIM:
        *result = p_cap->hal_wep_tkip_aggr_num_tx_delim;
        return HAL_OK;
    case HAL_CAP_WEP_TKIP_AGGR_RX_DELIM:
        *result = p_cap->hal_wep_tkip_aggr_num_rx_delim;
        return HAL_OK;
    case HAL_CAP_DEVICE_TYPE:
        *result = (u_int32_t)AH_PRIVATE(ah)->ah_devType;
        return HAL_OK;
    case HAL_INTR_MITIGATION_SUPPORTED:
        *result = p_cap->halintr_mitigation;
        return HAL_OK;
    case HAL_CAP_MAX_TKIP_WEP_HT_RATE:
        *result = p_cap->hal_wep_tkip_max_ht_rate;
        return HAL_OK;
    case HAL_CAP_NUM_MR_RETRIES:
        *result = p_cap->hal_num_mr_retries;
        return HAL_OK;
    case HAL_CAP_GEN_TIMER:
        return p_cap->hal_gen_timer_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_MAX_WEP_TKIP_HT20_TX_RATEKBPS:
    case HAL_CAP_MAX_WEP_TKIP_HT40_TX_RATEKBPS:
    case HAL_CAP_MAX_WEP_TKIP_HT20_RX_RATEKBPS:
    case HAL_CAP_MAX_WEP_TKIP_HT40_RX_RATEKBPS:
        *result = (u_int32_t)(-1);
        return HAL_OK;
    case HAL_CAP_CFENDFIX:
        *result = p_cap->hal_cfend_fix_support;
        return HAL_OK;
#if WLAN_SPECTRAL_ENABLE
    case HAL_CAP_SPECTRAL_SCAN:
        return p_cap->hal_spectral_scan ? HAL_OK : HAL_ENOTSUPP;
    /* No chipsets serviced by the direct attach HAL
       offer Advanced Spectral Scan functionality (available
       in 11ac chipsets onwards). Hence we do not waste
       space in HAL_CAPABILITIES to indicate such support.
       XXX If in the future the functionality is made available
           on chipsets serviced by HAL, then make additions
           accordingly */
    case HAL_CAP_ADVNCD_SPECTRAL_SCAN:
        return HAL_ENOTSUPP;
#endif
#if ATH_SUPPORT_RAW_ADC_CAPTURE
    case HAL_CAP_RAW_ADC_CAPTURE:
        return p_cap->hal_raw_adc_capture ? HAL_OK : HAL_ENOTSUPP;
#endif
    case HAL_CAP_EXTRADELIMWAR:
        *result = p_cap->hal_aggr_extra_delim_war;
        return HAL_OK;
    case HAL_CAP_RXDESC_TIMESTAMPBITS:
        *result = p_cap->hal_rx_desc_timestamp_bits;
        return HAL_OK;        
    case HAL_CAP_RXTX_ABORT:
        return p_cap->hal_rx_tx_abort_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_ANI_POLL_INTERVAL:
        *result = p_cap->hal_ani_poll_interval;
        return HAL_OK;
    case HAL_CAP_PAPRD_ENABLED:
        *result = p_cap->hal_paprd_enabled;
        return p_cap->hal_paprd_enabled ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_HW_UAPSD_TRIG:
        return p_cap->hal_hw_uapsd_trig ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_ANT_DIV_COMB:
        return p_cap->hal_ant_div_comb_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_CHANNEL_SWITCH_TIME_USEC:
        *result = p_cap->hal_channel_switch_time_usec;
        return HAL_OK;
    case HAL_CAP_LMAC_SMART_ANT:
        *result = p_cap->hal_lmac_smart_ant;
        return HAL_OK;
#if ATH_SUPPORT_WRAP
    case HAL_CAP_PROXYSTARXWAR:
        *result = p_cap->hal_proxy_sta_rx_war;
        return HAL_OK;
#endif
#if ATH_SUPPORT_WAPI
    case HAL_CAP_WAPI_MAX_TX_CHAINS:
        *result = p_cap->hal_wapi_max_tx_chains;
        return HAL_OK;
    case HAL_CAP_WAPI_MAX_RX_CHAINS:
        *result = p_cap->hal_wapi_max_rx_chains;
        return HAL_OK;
#endif
    case HAL_CAP_MCI:
        return p_cap->hal_mci_support ? HAL_OK : HAL_ENOTSUPP;
#if ATH_WOW_OFFLOAD
    case HAL_CAP_WOW_GTK_OFFLOAD:
        if (p_cap->hal_wow_gtk_offload_support &&
            HAL_WOW_CTRL(ah, HAL_WOW_OFFLOAD_SW_WOW_ENABLE) &&
            !HAL_WOW_CTRL(ah, HAL_WOW_OFFLOAD_GTK_DISABLE))
        {
            return HAL_OK;
        }
        else {
            return HAL_ENOTSUPP;
        }
    case HAL_CAP_WOW_ARP_OFFLOAD:
        if (p_cap->hal_wow_arp_offload_support &&
            HAL_WOW_CTRL(ah, HAL_WOW_OFFLOAD_SW_WOW_ENABLE) &&
            !HAL_WOW_CTRL(ah, HAL_WOW_OFFLOAD_ARP_DISABLE))
        {
            return HAL_OK;
        }
        else {
            return HAL_ENOTSUPP;
        }
    case HAL_CAP_WOW_NS_OFFLOAD:
        if (p_cap->hal_wow_ns_offload_support &&
            HAL_WOW_CTRL(ah, HAL_WOW_OFFLOAD_SW_WOW_ENABLE) &&
            !HAL_WOW_CTRL(ah, HAL_WOW_OFFLOAD_NS_DISABLE))
        {
            return HAL_OK;
        }
        else {
            return HAL_ENOTSUPP;
        }
    case HAL_CAP_WOW_4WAY_HS_WAKEUP:
        if (p_cap->hal_wow_4way_hs_wakeup_support &&
            HAL_WOW_CTRL(ah, HAL_WOW_OFFLOAD_SW_WOW_ENABLE) &&
            !HAL_WOW_CTRL(ah, HAL_WOW_OFFLOAD_4WAY_HS_DISABLE))
        {
            return HAL_OK;
        }
        else {
            return HAL_ENOTSUPP;
        }
    case HAL_CAP_WOW_ACER_MAGIC:
        if (p_cap->hal_wow_acer_magic_support &&
            HAL_WOW_CTRL(ah, HAL_WOW_OFFLOAD_SW_WOW_ENABLE) &&
            !HAL_WOW_CTRL(ah, HAL_WOW_OFFLOAD_ACER_MAGIC_DISABLE))
        {
            return HAL_OK;
        }
        else {
            return HAL_ENOTSUPP;
        }
    case HAL_CAP_WOW_ACER_SWKA:
        if (p_cap->hal_wow_acer_swka_support &&
            HAL_WOW_CTRL(ah, HAL_WOW_OFFLOAD_SW_WOW_ENABLE) &&
            !HAL_WOW_CTRL(ah, HAL_WOW_OFFLOAD_ACER_SWKA_DISABLE))
        {
            return HAL_OK;
        }
        else {
            return HAL_ENOTSUPP;
        }
#endif /* ATH_WOW_OFFLOAD */
    case HAL_CAP_RADIO_RETENTION:
        return p_cap->hal_radio_retention_support ? HAL_OK : HAL_ENOTSUPP;
    case HAL_CAP_DCS_SUPPORT:
        return HAL_ENOTSUPP;
    default:
        return HAL_EINVAL;
    }
}

bool
ath_hal_setcapability(struct ath_hal *ah, HAL_CAPABILITY_TYPE type,
    u_int32_t capability, u_int32_t setting, HAL_STATUS *status)
{

    switch (type) {
    case HAL_CAP_TXPOW:
        switch (capability) {
        case 3:
            /* Make sure we do not go beyond the MIN value */
            if (setting > HAL_TP_SCALE_MIN) 
                setting = HAL_TP_SCALE_MIN;
            AH_PRIVATE(ah)->ah_tp_scale = setting;
            return true;
        case 5:
            /* Make sure we do not go beyond the MIN value */
            if (setting > HAL_TP_SCALE_MIN)
                setting = HAL_TP_SCALE_MIN;
            AH_PRIVATE(ah)->ah_power_scale = setting;
            return true;
        }
        break;
    case HAL_CAP_RFSILENT:      /* rfsilent support  */
        /*
         * NB: allow even if hal_rf_silent_support is false
         *     in case the EEPROM is misprogrammed.
         */
        switch (capability) {
        case 1:         /* current setting */
            if (!AH_PRIVATE(ah)->ah_rfkillEnabled && (setting != 0)) {
                /* 
                 * When ah_rfkillEnabled is set from 0 to 1, clear ah_rfkillINTInit to 0.
                 */
                AH_PRIVATE(ah)->ah_rfkillINTInit = false;
            }
            AH_PRIVATE(ah)->ah_rfkillEnabled = (setting != 0);
            return true;
        case 2:         /* rfsilent config */
            return false;
        }
        break;
#if ATH_SUPPORT_IBSS
    case HAL_CAP_MCAST_KEYSRCH:
        (*(ah->ah_set_capability))(ah, type, capability, setting, status);
        break;
#endif
    case HAL_CAP_REG_DMN:           /* regulatory domain */
        AH_PRIVATE(ah)->ah_current_rd = setting;
        return true;
    default:
        break;
    }
    if (status)
        *status = HAL_EINVAL;
    return false;
}

/* 
 * Common support for getDiagState method.
 */

static u_int
ath_hal_getregdump(struct ath_hal *ah, const HAL_REGRANGE *regs,
    void *dstbuf, int space)
{
    u_int32_t *dp = dstbuf;
    int i;

    for (i = 0; space >= 2*sizeof(u_int32_t); i++) {
        u_int r = regs[i].start;
        u_int e = regs[i].end;
        *dp++ = (r<<16) | e;
        space -= sizeof(u_int32_t);
        do {
            *dp++ = OS_REG_READ(ah, r);
            r += sizeof(u_int32_t);
            space -= sizeof(u_int32_t);
        } while (r <= e && space >= sizeof(u_int32_t));
    }
    return (char *) dp - (char *) dstbuf;
}

bool
ath_hal_getdiagstate(struct ath_hal *ah, int request,
    const void *args, u_int32_t argsize,
    void **result, u_int32_t *resultsize)
{
    switch (request) {
    case HAL_DIAG_REVS:
        *result = &AH_PRIVATE(ah)->ah_devid;
        *resultsize = sizeof(HAL_REVS);
        return true;
    case HAL_DIAG_REGS:
        *resultsize = ath_hal_getregdump(ah, args, *result,*resultsize);
        return true;
    case HAL_DIAG_REGREAD: {
            if (argsize != sizeof(u_int))
           return false;
        **(u_int32_t **)result = OS_REG_READ(ah, *(const u_int *)args);
        *resultsize = sizeof(u_int32_t);
        return true;    
    }
    case HAL_DIAG_REGWRITE: {
            const HAL_DIAG_REGVAL *rv;
        if (argsize != sizeof(HAL_DIAG_REGVAL))
            return false;
        rv = (const HAL_DIAG_REGVAL *)args;
        OS_REG_WRITE(ah, rv->offset, rv->val);
        return true;   
    }
    case HAL_DIAG_GET_REGBASE: {
        /* Should be HAL_BUS_HANDLE but compiler warns and hal build set to
           treat warnings as errors. */
        **(uintptr_t **)result = (uintptr_t) AH_PRIVATE(ah)->ah_sh;
        *resultsize = sizeof(HAL_BUS_HANDLE);
        return true;
    }
#ifdef AH_PRIVATE_DIAG
    case HAL_DIAG_SETKEY: {
        const HAL_DIAG_KEYVAL *dk;

        if (argsize != sizeof(HAL_DIAG_KEYVAL))
            return false;
        dk = (const HAL_DIAG_KEYVAL *)args;
        return ah->ah_set_key_cache_entry(ah, dk->dk_keyix,
            &dk->dk_keyval, dk->dk_mac, dk->dk_xor);
    }
    case HAL_DIAG_RESETKEY:
        if (argsize != sizeof(u_int16_t))
            return false;
        return ah->ah_reset_key_cache_entry(ah, *(const u_int16_t *)args);
#endif /* AH_PRIVATE_DIAG */
    case HAL_DIAG_EEREAD:
        if (argsize != sizeof(u_int16_t))
            return false;
        if (!ath_hal_eepromRead(ah, *(const u_int16_t *)args, *result))
            return false;
        *resultsize = sizeof(u_int16_t);
        return true;
    case HAL_DIAG_EEPROM:
        {
            void *dstbuf;
            *resultsize = ath_hal_eepromDump(ah, &dstbuf);
            *result = dstbuf;
        }
        return true;
#ifdef AH_SUPPORT_WRITE_EEPROM
    case HAL_DIAG_EEWRITE: {
        const HAL_DIAG_EEVAL *ee;

        if (argsize != sizeof(HAL_DIAG_EEVAL))
            return false;
        ee = (const HAL_DIAG_EEVAL *)args;
        return ath_hal_eepromWrite(ah, ee->ee_off, ee->ee_data);
    }
#endif /* AH_SUPPORT_WRITE_EEPROM */
    case HAL_DIAG_RDWRITE: {
        HAL_STATUS status;
        const HAL_REG_DOMAIN *rd;
    
        if (argsize != sizeof(HAL_REG_DOMAIN))
            return false;
        rd = (const HAL_REG_DOMAIN *) args;
        if (!ath_hal_setcapability(ah, HAL_CAP_REG_DMN, 0, *rd, &status))
            return false;
        return true;
    }   
    case HAL_DIAG_RDREAD: {
        u_int32_t rd;
        if (ath_hal_getcapability(ah, HAL_CAP_REG_DMN, 0, &rd) != HAL_OK){
            return false;
        }
        *resultsize = sizeof(u_int16_t);
        *((HAL_REG_DOMAIN *) (*result)) = (u_int16_t) rd;
        return true;
    }
 
    }
    return false;
}

/*
 * Set the properties of the tx queue with the parameters
 * from q_info.
 */
bool
ath_hal_set_tx_q_props(struct ath_hal *ah,
    HAL_TX_QUEUE_INFO *qi, const HAL_TXQ_INFO *q_info)
{
    u_int32_t cw;

    if (qi->tqi_type == HAL_TX_QUEUE_INACTIVE) {
        HDPRINTF(ah, HAL_DBG_QUEUE, "%s: inactive queue\n", __func__);
        return false;
    }

    HDPRINTF(ah, HAL_DBG_QUEUE, "%s: queue %pK\n", __func__, qi);

    /* XXX validate parameters */
    qi->tqi_ver = q_info->tqi_ver;
    qi->tqi_subtype = q_info->tqi_subtype;
    qi->tqi_qflags = q_info->tqi_qflags;
    qi->tqi_priority = q_info->tqi_priority;
    if (q_info->tqi_aifs != HAL_TXQ_USEDEFAULT)
        qi->tqi_aifs = AH_MIN(q_info->tqi_aifs, 255);
    else
        qi->tqi_aifs = INIT_AIFS;
    if (q_info->tqi_cwmin != HAL_TXQ_USEDEFAULT) {
        cw = AH_MIN(q_info->tqi_cwmin, 1024);
        /* make sure that the CWmin is of the form (2^n - 1) */
        qi->tqi_cwmin = 1;
        while (qi->tqi_cwmin < cw)
            qi->tqi_cwmin = (qi->tqi_cwmin << 1) | 1;
    } else
        qi->tqi_cwmin = q_info->tqi_cwmin;
    if (q_info->tqi_cwmax != HAL_TXQ_USEDEFAULT) {
        cw = AH_MIN(q_info->tqi_cwmax, 1024);
        /* make sure that the CWmax is of the form (2^n - 1) */
        qi->tqi_cwmax = 1;
        while (qi->tqi_cwmax < cw)
            qi->tqi_cwmax = (qi->tqi_cwmax << 1) | 1;
    } else
        qi->tqi_cwmax = INIT_CWMAX;
    /* Set retry limit values */
    if (q_info->tqi_shretry != 0)
        qi->tqi_shretry = AH_MIN(q_info->tqi_shretry, 15);
    else
        qi->tqi_shretry = INIT_SH_RETRY;
    if (q_info->tqi_lgretry != 0)
        qi->tqi_lgretry = AH_MIN(q_info->tqi_lgretry, 15);
    else
        qi->tqi_lgretry = INIT_LG_RETRY;
    qi->tqi_cbr_period = q_info->tqi_cbr_period;
    qi->tqi_cbr_overflow_limit = q_info->tqi_cbr_overflow_limit;
    qi->tqi_burst_time = q_info->tqi_burst_time;
    qi->tqi_ready_time = q_info->tqi_ready_time;

    switch (q_info->tqi_subtype) {
    case HAL_WME_UPSD:
        if (qi->tqi_type == HAL_TX_QUEUE_DATA)
            qi->tqi_int_flags = HAL_TXQ_USE_LOCKOUT_BKOFF_DIS;
        break;
    default:
        break;      /* NB: silence compiler */
    }
    return true;
}

bool
ath_hal_get_tx_q_props(struct ath_hal *ah,
    HAL_TXQ_INFO *q_info, const HAL_TX_QUEUE_INFO *qi)
{
    if (qi->tqi_type == HAL_TX_QUEUE_INACTIVE) {
        HDPRINTF(ah, HAL_DBG_QUEUE, "%s: inactive queue\n", __func__);
        return false;
    }

    q_info->tqi_qflags = qi->tqi_qflags;
    q_info->tqi_ver = qi->tqi_ver;
    q_info->tqi_subtype = qi->tqi_subtype;
    q_info->tqi_qflags = qi->tqi_qflags;
    q_info->tqi_priority = qi->tqi_priority;
    q_info->tqi_aifs = qi->tqi_aifs;
    q_info->tqi_cwmin = qi->tqi_cwmin;
    q_info->tqi_cwmax = qi->tqi_cwmax;
    q_info->tqi_shretry = qi->tqi_shretry;
    q_info->tqi_lgretry = qi->tqi_lgretry;
    q_info->tqi_cbr_period = qi->tqi_cbr_period;
    q_info->tqi_cbr_overflow_limit = qi->tqi_cbr_overflow_limit;
    q_info->tqi_burst_time = qi->tqi_burst_time;
    q_info->tqi_ready_time = qi->tqi_ready_time;
    return true;
}

                                     /* 11a Turbo  11b  11g  108g  XR */
static const int16_t NOISE_FLOOR[] = { -96, -93,  -98, -96,  -93, -96 };

/*
 * Read the current channel noise floor and return.
 * If nf cal hasn't finished, channel noise floor should be 0
 * and we return a nominal value based on band and frequency.
 *
 * NB: This is a private routine used by per-chip code to
 *     implement the ah_get_chan_noise method.
 */
int16_t
ath_hal_get_chan_noise(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    HAL_CHANNEL_INTERNAL *ichan;

    ichan = ath_hal_checkchannel(ah, chan);
    if (ichan == AH_NULL) {
        HDPRINTF(ah, HAL_DBG_NF_CAL, "%s: invalid channel %u/0x%x; no mapping\n",
            __func__, chan->channel, chan->channel_flags);
        return 0;
    }
    if (ichan->raw_noise_floor == 0) {
        WIRELESS_MODE mode = ath_hal_chan2wmode(ah, chan);

        HALASSERT(mode < WIRELESS_MODE_MAX);
        return NOISE_FLOOR[mode] + ath_hal_getNfAdjust(ah, ichan);
    } else
        return ichan->raw_noise_floor;
}

/*
 * Enable single Write Key cache
 */
void __ahdecl
ath_hal_set_singleWriteKC(struct ath_hal *ah, u_int8_t singleWriteKC)
{
    AH_PRIVATE(ah)->ah_singleWriteKC = singleWriteKC;
}

#if !NO_HAL
bool __ahdecl ath_hal_enabledANI(struct ath_hal *ah)
{
    return AH_PRIVATE(ah)->ah_config.ath_hal_enable_ani;
}
#endif

#if !NO_HAL
u_int32_t __ahdecl ath_hal_get_device_info(struct ath_hal *ah,HAL_DEVICE_INFO type)
{
    struct ath_hal_private  *ap = AH_PRIVATE(ah);
    u_int32_t result = 0;

    switch (type) {
    case HAL_MAC_MAGIC:
        result = ap->ah_magic;   /* MAC magic number */
        break;
    case HAL_MAC_VERSION:        /* MAC version id */
        result = ap->ah_mac_version;
        break;
    case HAL_MAC_REV:            /* MAC revision */
        result = ap->ah_mac_rev;
        break;
    case HAL_PHY_REV:            /* PHY revision */
        result = ap->ah_phy_rev;
        break;
    case HAL_ANALOG5GHZ_REV:     /* 5GHz radio revision */
        result = ap->ah_analog_5ghz_rev;
        break;
    case HAL_ANALOG2GHZ_REV:     /* 2GHz radio revision */
        result = ap->ah_analog2GhzRev;
        break;
    default:
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: Unknown type %d\n",__func__,type);
        HALASSERT(0);
    }
    return result;
}
#endif

#if AH_REGREAD_DEBUG
inline void
ath_hal_regread_dump(struct ath_hal *ah)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);
    int i;
    u_int64_t tsfnow;
    tsfnow = ah->ah_get_tsf64(ah);
    printk("REG base = 0x%x, total time is %lld x10^-6 sec\n", 
           ahp->ah_regaccessbase<<13, 
           tsfnow - ahp->ah_regtsf_start);
/* In order to save memory, the register address space are
 * divided by 8 segments, with each the size is 8192 (0x2000),
 * and the ah_regaccessbase is from 0 to 7.
 */
    for (i = 0; i < 8192; i ++) {
        if (ahp->ah_regaccess[i] > 0) {
            printk("REG #%x, counts %d\n", 
                   i+ahp->ah_regaccessbase*8192, ahp->ah_regaccess[i]);
        }
    }
}
#endif

/******************************************************************************/
/*!
**  \brief Set HAL Private parameter
**
**  This function will set the specific parameter ID to the value passed in
**  the buffer.  The caller will have to ensure the proper type is used.
**
**  \param ah Pointer to HAL object (this)
**  \param p Parameter ID
**  \param b Parameter value in binary format.
**
**  \return 0 On success
**  \return -1 on invalid parameter
*/

#if !NO_HAL
u_int32_t __ahdecl 
ath_hal_set_config_param(struct ath_hal *ah,
                         HAL_CONFIG_OPS_PARAMS_TYPE p,
                         void *valBuff)
{
    struct ath_hal_private  *ap = AH_PRIVATE(ah);
    int                     retval = 0;
    
    /*
    ** Code Begins
    ** Switch on parameter
    */
    
    switch(p)
    {
        case HAL_CONFIG_DMA_BEACON_RESPONSE_TIME:
            ap->ah_config.ath_hal_dma_beacon_response_time = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_SW_BEACON_RESPONSE_TIME:
            ap->ah_config.ath_hal_sw_beacon_response_time = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_ADDITIONAL_SWBA_BACKOFF:
            ap->ah_config.ath_hal_additional_swba_backoff = *(int *)valBuff;
        break;                                         
                                                   
        case HAL_CONFIG_6MB_ACK:                        
            ap->ah_config.ath_hal_6mb_ack = *(int *)valBuff;
        break;                                         
                                                   
        case HAL_CONFIG_CWMIGNOREEXTCCA:                
            ap->ah_config.ath_hal_cwm_ignore_ext_cca = *(int *)valBuff;
        break;                                         

        case HAL_CONFIG_CCA_DETECTION_LEVEL:
            ap->ah_config.ath_hal_cca_detection_level = *(int *)valBuff;
        break;

        case HAL_CONFIG_CCA_DETECTION_MARGIN:
            ap->ah_config.ath_hal_cca_detection_margin = *(int *)valBuff;
        break;
  
#ifdef ATH_FORCE_BIAS
        case HAL_CONFIG_FORCEBIAS:              
            ap->ah_config.ath_hal_forceBias = *(int *)valBuff;
        break;

        case HAL_CONFIG_FORCEBIASAUTO:
            ap->ah_config.ath_hal_forceBiasAuto = *(int *)valBuff;
        break;
#endif
        
        case HAL_CONFIG_PCIEPOWERSAVEENABLE:    
            ap->ah_config.ath_hal_pcie_power_save_enable = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_PCIEL1SKPENABLE:        
            ap->ah_config.ath_hal_pcieL1SKPEnable = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_PCIECLOCKREQ:           
            ap->ah_config.ath_hal_pcie_clock_req = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_PCIEWAEN:               
            ap->ah_config.ath_hal_pcie_waen = *(int *)valBuff;
        break;

        case HAL_CONFIG_PCIEDETACH:               
            ap->ah_config.ath_hal_pcieDetach = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_PCIEPOWERRESET:         
            ap->ah_config.ath_hal_pciePowerReset = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_PCIERESTORE:         
            ap->ah_config.ath_hal_pcieRestore = *(int *)valBuff;
        break;

        case HAL_CONFIG_HTENABLE:               
            ap->ah_config.ath_hal_ht_enable = *(int *)valBuff;
        break;
        
#ifdef ATH_SUPERG_DYNTURBO
        case HAL_CONFIG_DISABLETURBOG:          
            ap->ah_config.ath_hal_disableTurboG = *(int *)valBuff;
        break;
#endif
        case HAL_CONFIG_OFDMTRIGLOW:            
            ap->ah_config.ath_hal_ofdmTrigLow = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_OFDMTRIGHIGH:           
            ap->ah_config.ath_hal_ofdmTrigHigh = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_CCKTRIGHIGH:            
            ap->ah_config.ath_hal_cckTrigHigh = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_CCKTRIGLOW:             
            ap->ah_config.ath_hal_cckTrigLow = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_ENABLEANI:              
            ap->ah_config.ath_hal_enable_ani = *(int *)valBuff;
        break;

        case HAL_CONFIG_ENABLEADAPTIVECCATHRES:
            ap->ah_config.ath_hal_enable_adaptiveCCAThres = *(int *)valBuff;
        break;
 
        case HAL_CONFIG_NOISEIMMUNITYLVL:       
            ap->ah_config.ath_hal_noiseImmunityLvl = *(int *)valBuff;
        break;

        case HAL_CONFIG_OFDMWEAKSIGDET:         
            ap->ah_config.ath_hal_ofdmWeakSigDet = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_CCKWEAKSIGTHR:          
            ap->ah_config.ath_hal_cckWeakSigThr = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_SPURIMMUNITYLVL:        
            ap->ah_config.ath_hal_spurImmunityLvl = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_FIRSTEPLVL:             
            ap->ah_config.ath_hal_firStepLvl = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_RSSITHRHIGH:            
            ap->ah_config.ath_hal_rssiThrHigh = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_RSSITHRLOW:             
            ap->ah_config.ath_hal_rssiThrLow = *(int *)valBuff;
        break;
        case HAL_CONFIG_DIVERSITYCONTROL:       
            ap->ah_config.ath_hal_diversity_control = *(int *)valBuff;
        break;
        
        case HAL_CONFIG_ANTENNASWITCHSWAP:
            ap->ah_config.ath_hal_antenna_switch_swap = *(int *)valBuff;
        break;

        case HAL_CONFIG_FULLRESETWARENABLE:
            ap->ah_config.ath_hal_fullResetWarEnable = *(int *)valBuff;
        break;
        case HAL_CONFIG_DISABLEPERIODICPACAL:
            ap->ah_config.ath_hal_disPeriodicPACal = *(int *)valBuff;
        break;

        case HAL_CONFIG_KEYCACHE_PRINT:
            if (ah->ah_print_key_cache)
                (*ah->ah_print_key_cache)(ah);
        break;

#if AH_DEBUG || AH_PRINT_FILTER
        case HAL_CONFIG_DEBUG:
        {
            int category, mask = *(int *)valBuff;
            for (category = 0; category < HAL_DBG_NUM_CATEGORIES; category++) {
                int category_mask = (1 << category) & mask;
                asf_print_mask_set(&ap->ah_asf_print, category, category_mask);
            }
        }
        break;
#endif
        case HAL_CONFIG_DEFAULTANTENNASET:
            ap->ah_config.ath_hal_defaultAntCfg = *(int *)valBuff;
        break;

#if AH_REGREAD_DEBUG
        case HAL_CONFIG_REGREAD_BASE:
            ap->ah_regaccessbase = *(int *)valBuff;
            OS_MEMZERO(ap->ah_regaccess, 8192*sizeof(u_int32_t));
            ap->ah_regtsf_start = ah->ah_get_tsf64(ah);
        break;
#endif
#ifdef ATH_SUPPORT_TxBF
        case HAL_CONFIG_TxBF_CTL:
            ap->ah_config.ath_hal_txbf_ctl = *(int *)valBuff;
            if (ah->ah_fill_tx_bf_cap!=NULL)
                ah->ah_fill_tx_bf_cap(ah);         // update txbf cap by new settings
        break;
#endif
        case HAL_CONFIG_SHOW_BB_PANIC:
            ap->ah_config.ath_hal_show_bb_panic = !!*(int *)valBuff;
        break;

        case HAL_CONFIG_INTR_MITIGATION_RX:
            ap->ah_config.ath_hal_intr_mitigation_rx = *(int *)valBuff;
        break;

        case HAL_CONFIG_CH144:
            ap->ah_config.ath_hal_ch144 = *(int *)valBuff;
			break;

#ifdef HOST_OFFLOAD
        case HAL_CONFIG_LOWER_INT_MITIGATION_RX:
            ap->ah_config.ath_hal_lower_rx_mitigation = *(int *)valBuff;
        break;
#endif
        default:
            retval = -1;
    }
    
    return (retval);
}
#endif

/******************************************************************************/
/*!
**  \brief Get HAL Private Parameter
**
**  Gets the value of the parameter and returns it in the provided buffer. It
**  is assumed that the value is ALWAYS returned as an integre value.
**
**  \param ah      Pointer to HAL object (this)
**  \param p       Parameter ID
**  \param valBuff Parameter value in binary format.
**
**  \return number of bytes put into the buffer
**  \return 0 on invalid parameter ID
*/

#if !NO_HAL
u_int32_t __ahdecl
ath_hal_get_config_param(struct ath_hal *ah,
                         HAL_CONFIG_OPS_PARAMS_TYPE p, 
                         void *valBuff)
{
    struct ath_hal_private  *ap = AH_PRIVATE(ah);
    int                     retval = 0;
    
    /*
    ** Code Begins
    ** Switch on parameter
    */
    
    switch (p)
    {
        case HAL_CONFIG_DMA_BEACON_RESPONSE_TIME:
            *(int *)valBuff = ap->ah_config.ath_hal_dma_beacon_response_time;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_SW_BEACON_RESPONSE_TIME:
            *(int *)valBuff = ap->ah_config.ath_hal_sw_beacon_response_time;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_ADDITIONAL_SWBA_BACKOFF:
            *(int *)valBuff = ap->ah_config.ath_hal_additional_swba_backoff;
            retval = sizeof(int);
        break;                                         
                                                   
        case HAL_CONFIG_6MB_ACK:                        
            *(int *)valBuff = ap->ah_config.ath_hal_6mb_ack;
            retval = sizeof(int);
        break;                                         
                                                   
        case HAL_CONFIG_CWMIGNOREEXTCCA:                
            *(int *)valBuff = ap->ah_config.ath_hal_cwm_ignore_ext_cca;
            retval = sizeof(int);
        break;                                         

        case HAL_CONFIG_CCA_DETECTION_LEVEL:
            *(int *)valBuff = ap->ah_config.ath_hal_cca_detection_level;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_CCA_DETECTION_MARGIN:
            *(int *)valBuff = ap->ah_config.ath_hal_cca_detection_margin;
            retval = sizeof(int);
        break;

#ifdef ATH_FORCE_BIAS
        case HAL_CONFIG_FORCEBIAS:              
            *(int *)valBuff = ap->ah_config.ath_hal_forceBias;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_FORCEBIASAUTO:
            *(int *)valBuff = ap->ah_config.ath_hal_forceBiasAuto;
            retval = sizeof(int);
        break;
#endif
        
        case HAL_CONFIG_PCIEPOWERSAVEENABLE:    
            *(int *)valBuff = ap->ah_config.ath_hal_pcie_power_save_enable;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_PCIEL1SKPENABLE:        
            *(int *)valBuff = ap->ah_config.ath_hal_pcieL1SKPEnable;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_PCIECLOCKREQ:           
            *(int *)valBuff = ap->ah_config.ath_hal_pcie_clock_req;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_PCIEWAEN:               
            *(int *)valBuff = ap->ah_config.ath_hal_pcie_waen;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_PCIEDETACH:               
            *(int *)valBuff = ap->ah_config.ath_hal_pcieDetach;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_PCIEPOWERRESET:         
            *(int *)valBuff = ap->ah_config.ath_hal_pciePowerReset;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_PCIERESTORE:         
            *(int *)valBuff = ap->ah_config.ath_hal_pcieRestore;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_ENABLEMSI:         
            *(int *)valBuff = ap->ah_config.ath_hal_enable_msi;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_HTENABLE:               
            *(int *)valBuff = ap->ah_config.ath_hal_ht_enable;
            retval = sizeof(int);
        break;
        
#ifdef ATH_SUPERG_DYNTURBO
        case HAL_CONFIG_DISABLETURBOG:          
            *(int *)valBuff = ap->ah_config.ath_hal_disableTurboG;
            retval = sizeof(int);
        break;
#endif
        case HAL_CONFIG_OFDMTRIGLOW:            
            *(int *)valBuff = ap->ah_config.ath_hal_ofdmTrigLow;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_OFDMTRIGHIGH:           
            *(int *)valBuff = ap->ah_config.ath_hal_ofdmTrigHigh;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_CCKTRIGHIGH:            
            *(int *)valBuff = ap->ah_config.ath_hal_cckTrigHigh;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_CCKTRIGLOW:             
            *(int *)valBuff = ap->ah_config.ath_hal_cckTrigLow;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_ENABLEANI:              
            *(int *)valBuff = ap->ah_config.ath_hal_enable_ani;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_ENABLEADAPTIVECCATHRES:
            *(int *)valBuff = ap->ah_config.ath_hal_enable_adaptiveCCAThres;
            retval = sizeof(int);
        break;
 
        case HAL_CONFIG_NOISEIMMUNITYLVL:       
            *(int *)valBuff = ap->ah_config.ath_hal_noiseImmunityLvl;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_OFDMWEAKSIGDET:         
            *(int *)valBuff = ap->ah_config.ath_hal_ofdmWeakSigDet;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_CCKWEAKSIGTHR:          
            *(int *)valBuff = ap->ah_config.ath_hal_cckWeakSigThr;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_SPURIMMUNITYLVL:        
            *(int *)valBuff = ap->ah_config.ath_hal_spurImmunityLvl;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_FIRSTEPLVL:             
            *(int *)valBuff = ap->ah_config.ath_hal_firStepLvl;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_RSSITHRHIGH:            
            *(int *)valBuff = ap->ah_config.ath_hal_rssiThrHigh;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_RSSITHRLOW:             
            *(int *)valBuff = ap->ah_config.ath_hal_rssiThrLow;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_DIVERSITYCONTROL:       
            *(int *)valBuff = ap->ah_config.ath_hal_diversity_control;
            retval = sizeof(int);
        break;
        
        case HAL_CONFIG_ANTENNASWITCHSWAP:
            *(int *)valBuff = ap->ah_config.ath_hal_antenna_switch_swap;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_FULLRESETWARENABLE:
            *(int *)valBuff = ap->ah_config.ath_hal_fullResetWarEnable;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_DISABLEPERIODICPACAL:
            *(int *)valBuff = ap->ah_config.ath_hal_disPeriodicPACal;
            retval = sizeof(int);
        break;

        case HAL_CONFIG_KEYCACHE_PRINT:
            if (ah->ah_print_key_cache)
                (*ah->ah_print_key_cache)(ah);
            retval = sizeof(int);
        break;

#if AH_DEBUG || AH_PRINT_FILTER
        case HAL_CONFIG_DEBUG:
        {
            /*
             * asf_print: the asf print mask is (probably) 64 bits long, but 
             * we are expected to return 32 bits. For now, just return the 32
             * LSBs from the print mask. Currently, all used HAL print 
             * categories fit within 32 bits anyway.
             */
            u_int8_t *ah_mask_ptr = &ap->ah_asf_print.category_mask[0];
            *(int *)valBuff = 
                ah_mask_ptr[0] |
                (ah_mask_ptr[1] << 8) |
                (ah_mask_ptr[2] << 16) |
                (ah_mask_ptr[3] << 24);
        }
        retval = sizeof(int);
        break;
#endif
        
        case HAL_CONFIG_DEFAULTANTENNASET:
            *(int *)valBuff = ap->ah_config.ath_hal_defaultAntCfg;
        break;

#if AH_REGREAD_DEBUG
        case HAL_CONFIG_REGREAD_BASE:
            ath_hal_regread_dump(ah);           
            retval = sizeof(int);
        break;    
#endif

#ifdef ATH_SUPPORT_TxBF
        case HAL_CONFIG_TxBF_CTL:        
            *(int *)valBuff = ap->ah_config.ath_hal_txbf_ctl;
            retval = sizeof(int);           
        break;
#endif
        case HAL_CONFIG_SHOW_BB_PANIC:
            *(int *)valBuff = ap->ah_config.ath_hal_show_bb_panic;
            retval = sizeof(int);           
        break;

        case HAL_CONFIG_CH144:
            *(int *)valBuff = ap->ah_config.ath_hal_ch144;
            retval = sizeof(int);
        break;
        
        default:
            retval = 0;
    }
    
    return (retval);
}
#endif
#ifdef ATH_CCX
/*
 * ath_hal_get_sernum
 * Copy the HAL's serial number buffer into a buffer provided by the caller.
 * The caller can optionally specify a limit on the number of characters
 * to copy into the pSn buffer.
 * This limit is only used if it is a positive number.
 * The number of characters truncated during the copy is returned.
 */
int __ahdecl
ath_hal_get_sernum(struct ath_hal *ah, u_int8_t *pSn, int limit)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);
    u_int8_t    i;
    int bytes_truncated = 0;

    if (limit <= 0 || limit >= sizeof(ahp->ser_no)) {
        limit = sizeof(ahp->ser_no);
    } else {
        bytes_truncated = sizeof(ahp->ser_no) - limit;
    }

    for (i = 0; i < limit; i++) {
        pSn[i] = ahp->ser_no[i];
    }
    return bytes_truncated;
}    

bool __ahdecl
ath_hal_get_chandata(struct ath_hal *ah, HAL_CHANNEL *chan, HAL_CHANNEL_DATA* pChanData)
{
    WIRELESS_MODE       mode = ath_hal_chan2wmode(ah, chan);

    if (mode > WIRELESS_MODE_XR) {
        return false;
    }

    if (true == ath_hal_is_fast_clock_en(ah)) {
        pChanData->clockRate    = FAST_CLOCK_RATE[mode];
    } else {
        pChanData->clockRate    = CLOCK_RATE[mode];
    }
    pChanData->noiseFloor   = NOISE_FLOOR[mode];
    pChanData->ccaThreshold = ah->ah_get_cca_threshold(ah);

    return true;
}
#endif

HAL_CTRY_CODE __ahdecl
ath_hal_get_countryCode(struct ath_hal *ah)
{
    return AH_PRIVATE(ah)->ah_countryCode;
}


#if ATH_SUPPORT_CFEND
u_int8_t  __ahdecl ath_hal_get_rx_cur_aggr_n_reset(struct ath_hal *ah)
{
    u_int8_t res = AH_PRIVATE(ah)->ah_rx_num_cur_aggr_good;
    AH_PRIVATE(ah)->ah_rx_num_cur_aggr_good = 0;
    return res;
}
#endif

#ifdef DBG
#if !NO_HAL
u_int32_t __ahdecl
ath_hal_readRegister(struct ath_hal *ah, u_int32_t offset)
{
    return OS_REG_READ(ah, offset);
}

#endif
void __ahdecl
ath_hal_writeRegister(struct ath_hal *ah, u_int32_t offset, u_int32_t value)
{
    OS_REG_WRITE(ah, offset, value);
}
#else
#if !NO_HAL
u_int32_t __ahdecl
ath_hal_readRegister(struct ath_hal *ah, u_int32_t offset)
{
    return OS_REG_READ(ah, offset);
}
#endif
#endif

#if !NO_HAL
u_int32_t __ahdecl
ath_hal_set_mfp_qos(struct ath_hal *ah, u_int32_t dot11w)
{
    if (dot11w > 1) {
        return HAL_EINVAL;
    }
    AH_PRIVATE(ah)->ah_mfp_qos = dot11w;
    return HAL_OK;
}

u_int32_t __ahdecl
ath_hal_get_mfp_qos(struct ath_hal *ah)
{
    return AH_PRIVATE(ah)->ah_mfp_qos;
}

void __ahdecl
ath_hal_display_tpctables(struct ath_hal *ah)
{
    if (ah->ah_disp_tpc_tables != AH_NULL) {
        ah->ah_disp_tpc_tables(ah);   
    }
}

void __ahdecl
ath_hal_set_antenna_gain(struct ath_hal *ah, u_int32_t antgain, bool is2g)
{
    if (is2g)
        AH_PRIVATE(ah)->ah_antenna_gain_2g = 2 * antgain;
    else
        AH_PRIVATE(ah)->ah_antenna_gain_5g = 2 * antgain;
}

u_int32_t __ahdecl
ath_hal_get_antenna_gain(struct ath_hal *ah, bool is2g)
{
    if (is2g)
        return AH_PRIVATE(ah)->ah_antenna_gain_2g >> 1;
    else
        return AH_PRIVATE(ah)->ah_antenna_gain_5g >> 1;
}
#endif
