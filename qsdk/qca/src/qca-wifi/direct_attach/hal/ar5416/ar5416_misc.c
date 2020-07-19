/*
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 */
/*
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

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"
#ifdef AH_DEBUG
#include "ah_desc.h"                    /* NB: for HAL_PHYERR* */
#endif

#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416phy.h"
#include "ar5416/ar5416desc.h" /* AR5416_CONTTXMODE */

static u_int
ar5416MacToUsec(struct ath_hal *ah, u_int clks)
{
    HAL_CHANNEL_INTERNAL *chan = AH_PRIVATE(ah)->ah_curchan;

    if (chan && IS_CHAN_HT40(chan))
        return (ath_hal_mac_usec(ah, clks) / 2);
    else
        return (ath_hal_mac_usec(ah, clks));
}

static u_int
ar5416MacToClks(struct ath_hal *ah, u_int usecs)
{
    HAL_CHANNEL_INTERNAL *chan = AH_PRIVATE(ah)->ah_curchan;

    if (chan && IS_CHAN_HT40(chan))
        return (ath_hal_mac_clks(ah, usecs) * 2);
    else
        return (ath_hal_mac_clks(ah, usecs));
}

void
ar5416GetMacAddress(struct ath_hal *ah, u_int8_t *mac)
{
        struct ath_hal_5416 *ahp = AH5416(ah);

        OS_MEMCPY(mac, ahp->ah_macaddr, IEEE80211_ADDR_LEN);
}

HAL_BOOL
ar5416SetMacAddress(struct ath_hal *ah, const u_int8_t *mac)
{
        struct ath_hal_5416 *ahp = AH5416(ah);

        OS_MEMCPY(ahp->ah_macaddr, mac, IEEE80211_ADDR_LEN);
        return AH_TRUE;
}

void
ar5416GetBssIdMask(struct ath_hal *ah, u_int8_t *mask)
{
        struct ath_hal_5416 *ahp = AH5416(ah);

        OS_MEMCPY(mask, ahp->ah_bssidmask, IEEE80211_ADDR_LEN);
}

HAL_BOOL
ar5416SetBssIdMask(struct ath_hal *ah, const u_int8_t *mask)
{
        struct ath_hal_5416 *ahp = AH5416(ah);

        /* save it since it must be rewritten on reset */
        OS_MEMCPY(ahp->ah_bssidmask, mask, IEEE80211_ADDR_LEN);

		ENABLE_REG_WRITE_BUFFER
        OS_REG_WRITE(ah, AR_BSSMSKL, LE_READ_4(ahp->ah_bssidmask));
        OS_REG_WRITE(ah, AR_BSSMSKU, LE_READ_2(ahp->ah_bssidmask + 4));
        OS_REG_WRITE_FLUSH(ah);
		DISABLE_REG_WRITE_BUFFER
        return AH_TRUE;
}

/*
 * Attempt to change the cards operating regulatory domain to the given value
 * Returns: A_EINVAL for an unsupported regulatory domain.
 *          A_HARDWARE for an unwritable EEPROM or bad EEPROM version
 */
HAL_BOOL
ar5416SetRegulatoryDomain(struct ath_hal *ah,
        u_int16_t regDomain, HAL_STATUS *status)
{
#ifdef AH_SUPPORT_WRITE_REGDOMAIN
        struct ath_hal_5416 *ahp = AH5416(ah);
#endif
        HAL_STATUS ecode;

#ifdef AH_SUPPORT_WRITE_REGDOMAIN
        if (AH_PRIVATE(ah)->ah_current_rd == regDomain) {
   		    return AH_TRUE;
        }


        if (ar5416EepromSetParam(ah, EEP_REG_0, regDomain)) {
                HDPRINTF(ah, HAL_DBG_REGULATORY, "%s: set regulatory domain to %u (0x%x)\n",
                        __func__, regDomain, regDomain);
                AH_PRIVATE(ah)->ah_current_rd = regDomain;
                return AH_TRUE;
        }
#else
        if(AH_PRIVATE(ah)->ah_current_rd == 0)
        {
    	    AH_PRIVATE(ah)->ah_current_rd = regDomain;
            return AH_TRUE;
        }
#endif
        ecode = HAL_EIO;

#ifdef AH_SUPPORT_WRITE_REGDOMAIN
bad:
#endif 
        if (status)
                *status = ecode;
        return AH_FALSE;
}

/*
 * Return the wireless modes (a,b,g,t) supported by hardware.
 *
 * This value is what is actually supported by the hardware
 * and is unaffected by regulatory/country code settings.
 *
 */
u_int
ar5416GetWirelessModes(struct ath_hal *ah)
{
    return AH_PRIVATE(ah)->ah_caps.hal_wireless_modes;
}

/*
 * Set the interrupt and GPIO values so the ISR can disable RF
 * on a switch signal.  Assumes GPIO port and interrupt polarity
 * are set prior to call.
 */
void
ar5416EnableRfKill(struct ath_hal *ah)
{
    /* TODO - can this really be above the hal on the GPIO interface for
     * TODO - the client only?
     */
    struct ath_hal_5416    *ahp = AH5416(ah);

    /* Connect rfsilent_bb_l to baseband */
    OS_REG_SET_BIT(ah, AR_GPIO_INPUT_EN_VAL, AR_GPIO_INPUT_EN_VAL_RFSILENT_BB);
    
    /* Set input mux for rfsilent_bb_l to GPIO #0 */
    OS_REG_CLR_BIT(ah, AR_GPIO_INPUT_MUX2, AR_GPIO_INPUT_MUX2_RFSILENT);

    /* Configure the desired GPIO port for input and enable baseband rf silence */
    ath_hal_gpio_cfg_input(ah, ahp->ah_gpioSelect);
    OS_REG_SET_BIT(ah, AR_PHY_TEST, RFSILENT_BB);

    /*
     * If radio disable switch connection to GPIO bit x is enabled
     * program GPIO interrupt.
     * If rfkill bit on eeprom is 1, setupeeprommap routine has already
     * verified that it is a later version of eeprom, it has a place for
     * rfkill bit and it is set to 1, indicating that GPIO bit x hardware
     * connection is present.
     */
	 /* RFKill uses polling not interrupt, disable interrupt to avoid Eee PC 2.6.21.4 hang up issue */
    if (ath_hal_hasrfkill_int(ah))
    {
        if (ahp->ah_gpioBit == ar5416GpioGet(ah, ahp->ah_gpioSelect)) {
            /* switch already closed, set to interrupt upon open */
            ar5416GpioSetIntr(ah, ahp->ah_gpioSelect, !ahp->ah_gpioBit);
        } else {
            ar5416GpioSetIntr(ah, ahp->ah_gpioSelect, ahp->ah_gpioBit);
        }
    }
}       

/*
 * Change the LED blinking pattern to correspond to the connectivity
 */
void
ar5416SetLedState(struct ath_hal *ah, HAL_LED_STATE state)
{
    static const u_int32_t ledbits[8] = {
        AR_CFG_LED_ASSOC_NONE,     /* HAL_LED_RESET */
        AR_CFG_LED_ASSOC_PENDING,  /* HAL_LED_INIT  */
        AR_CFG_LED_ASSOC_PENDING,  /* HAL_LED_READY */
        AR_CFG_LED_ASSOC_PENDING,  /* HAL_LED_SCAN  */
        AR_CFG_LED_ASSOC_PENDING,  /* HAL_LED_AUTH  */
        AR_CFG_LED_ASSOC_ACTIVE,   /* HAL_LED_ASSOC */
        AR_CFG_LED_ASSOC_ACTIVE,   /* HAL_LED_RUN   */
        AR_CFG_LED_ASSOC_NONE,  
    };
    
    OS_REG_RMW_FIELD(ah, AR_CFG_LED, AR_CFG_LED_ASSOC_CTL, ledbits[state]);
}

/*
 * Sets the Power LED on the cardbus without affecting the Network LED.
 */
void
ar5416SetPowerLedState(struct ath_hal *ah, u_int8_t enabled)
{
    u_int32_t    val;

    val = enabled ? AR_CFG_LED_MODE_POWER_ON : AR_CFG_LED_MODE_POWER_OFF;

    OS_REG_RMW_FIELD(ah, AR_CFG_LED, AR_CFG_LED_POWER, val);
}

/*
 * Sets the Network LED on the cardbus without affecting the Power LED.
 */
void
ar5416SetNetworkLedState(struct ath_hal *ah, u_int8_t enabled)
{
    u_int32_t    val;

    val = enabled ? AR_CFG_LED_MODE_NETWORK_ON : AR_CFG_LED_MODE_NETWORK_OFF;

    OS_REG_RMW_FIELD(ah, AR_CFG_LED, AR_CFG_LED_NETWORK, val);
}

/*
 * Change association related fields programmed into the hardware.
 * Writing a valid BSSID to the hardware effectively enables the hardware
 * to synchronize its TSF to the correct beacons and receive frames coming
 * from that BSSID. It is called by the SME JOIN operation.
 */
void
ar5416WriteAssocid(struct ath_hal *ah, const u_int8_t *bssid, u_int16_t assocId)
{
        struct ath_hal_5416 *ahp = AH5416(ah);

        /* save bssid and assocId for restore on reset */
        OS_MEMCPY(ahp->ah_bssid, bssid, IEEE80211_ADDR_LEN);
        ahp->ah_assocId = assocId;

        OS_REG_WRITE(ah, AR_BSS_ID0, LE_READ_4(ahp->ah_bssid));
        OS_REG_WRITE(ah, AR_BSS_ID1, LE_READ_2(ahp->ah_bssid+4) |
                                     ((assocId & 0x3fff)<<AR_BSS_ID1_AID_S));
}

/*
 * Functions to get/set DCS mode
 */
void
ar5416SetDcsMode(struct ath_hal *ah, u_int32_t mode)
{
    AH_PRIVATE(ah)->ah_dcs_enable = mode;
}

u_int32_t
ar5416GetDcsMode(struct ath_hal *ah)
{
    return AH_PRIVATE(ah)->ah_dcs_enable;
}

/*
 * Get the current hardware tsf for stamlme
 */
u_int64_t
ar5416GetTsf64(struct ath_hal *ah)
{
    u_int64_t tsf;

    /* XXX sync multi-word read? */
    tsf = OS_REG_READ(ah, AR_TSF_U32);
    tsf = (tsf << 32) | OS_REG_READ(ah, AR_TSF_L32);
    return tsf;
}

void
ar5416SetTsf64(struct ath_hal *ah, u_int64_t tsf)
{
    OS_REG_WRITE(ah, AR_TSF_L32, (tsf & 0xffffffff));
    OS_REG_WRITE(ah, AR_TSF_U32, ((tsf >> 32) & 0xffffffff));
}

/*
 * Get the current hardware tsf for stamlme
 */
u_int32_t
ar5416GetTsf32(struct ath_hal *ah)
{
    return OS_REG_READ(ah, AR_TSF_L32);
}

u_int32_t
ar5416GetTsf2_32(struct ath_hal *ah)
{
    if (AR_SREV_KIWI(ah)) {
        return OS_REG_READ(ah, AR_TSF2_L32);
    }
    else {
        return -1;
    }
}

/*
 * Reset the current hardware tsf for stamlme.
 */
void
ar5416ResetTsf(struct ath_hal *ah)
{
    int count;

    count = 0;
    while (OS_REG_READ(ah, AR_SLP32_MODE) & AR_SLP32_TSF_WRITE_STATUS) {
        count++;
        if (count > 10) {
            HDPRINTF(ah, HAL_DBG_RESET, "%s: AR_SLP32_TSF_WRITE_STATUS limit exceeded\n", __func__);
            break;
        }
        OS_DELAY(10);
    }
    OS_REG_WRITE(ah, AR_RESET_TSF, AR_RESET_TSF_ONCE);
}

/*
 * Set or clear hardware basic rate bit
 * Set hardware basic rate set if basic rate is found
 * and basic rate is equal or less than 2Mbps
 */
void
ar5416SetBasicRate(struct ath_hal *ah, HAL_RATE_SET *rs)
{
        HAL_CHANNEL_INTERNAL *chan = AH_PRIVATE(ah)->ah_curchan;
        u_int32_t reg;
        u_int8_t xset;
        int i;

        if (chan == AH_NULL || !IS_CHAN_CCK(chan))
                return;
        xset = 0;
        for (i = 0; i < rs->rs_count; i++) {
                u_int8_t rset = rs->rs_rates[i];
                /* Basic rate defined? */
                if ((rset & 0x80) && (rset &= 0x7f) >= xset)
                        xset = rset;
        }
        /*
         * Set the h/w bit to reflect whether or not the basic
         * rate is found to be equal or less than 2Mbps.
         */
        reg = OS_REG_READ(ah, AR_STA_ID1);
		if (xset && xset/2 <= 2) {
                OS_REG_WRITE(ah, AR_STA_ID1, reg | AR_STA_ID1_BASE_RATE_11B);
		}
		else {
                OS_REG_WRITE(ah, AR_STA_ID1, reg &~ AR_STA_ID1_BASE_RATE_11B);
		}
}

/*
 * Grab a semi-random value from hardware registers - may not
 * change often
 */
u_int32_t
ar5416GetRandomSeed(struct ath_hal *ah)
{
        u_int32_t nf;

        nf = (OS_REG_READ(ah, AR_PHY(25)) >> 19) & 0x1ff;
        if (nf & 0x100)
                nf = 0 - ((nf ^ 0x1ff) + 1);
        return (OS_REG_READ(ah, AR_TSF_U32) ^
                OS_REG_READ(ah, AR_TSF_L32) ^ nf);
}

/*
 * Detect if our card is present
 */
HAL_BOOL
ar5416DetectCardPresent(struct ath_hal *ah)
{
        u_int16_t macVersion, macRev;
        u_int32_t v;

	if(AR_SREV_HOWL(ah))
		return AH_TRUE;
        /*
         * Read the Silicon Revision register and compare that
         * to what we read at attach time.  If the same, we say
         * a card/device is present.
         */
        v = OS_REG_READ(ah, AR_SREV) & AR_SREV_ID;
        if (v == 0xFF) {
	    /* new SREV format */
            v = OS_REG_READ(ah, AR_SREV);
            /* Include 6-bit Chip Type (masked to 0) to differentiate from pre-Sowl versions */
            macVersion= (v & AR_SREV_VERSION2) >> AR_SREV_TYPE2_S;
            macRev = MS(v, AR_SREV_REVISION2);
        } else {
            macVersion = MS(v, AR_SREV_VERSION);
            macRev = v & AR_SREV_REVISION;
        }
        return (AH_PRIVATE(ah)->ah_mac_version == macVersion &&
                AH_PRIVATE(ah)->ah_mac_rev == macRev);
}

/*
 * Update MIB Counters
 */
void
ar5416UpdateMibMacStats(struct ath_hal *ah)
{
        struct ath_hal_5416 *ahp = AH5416(ah);
        HAL_MIB_STATS* stats = &ahp->ah_stats.ast_mibstats;

        stats->ackrcv_bad += OS_REG_READ(ah, AR_ACK_FAIL);
        stats->rts_bad    += OS_REG_READ(ah, AR_RTS_FAIL);
        stats->fcs_bad    += OS_REG_READ(ah, AR_FCS_FAIL);
        stats->rts_good   += OS_REG_READ(ah, AR_RTS_OK);
        stats->beacons    += OS_REG_READ(ah, AR_BEACON_CNT);
}

void
ar5416GetMibMacStats(struct ath_hal *ah, HAL_MIB_STATS* stats)
{
        struct ath_hal_5416 *ahp = AH5416(ah);
        HAL_MIB_STATS* istats = &ahp->ah_stats.ast_mibstats;

        stats->ackrcv_bad = istats->ackrcv_bad;
        stats->rts_bad    = istats->rts_bad;    
        stats->fcs_bad    = istats->fcs_bad;    
        stats->rts_good   = istats->rts_good;  
        stats->beacons    = istats->beacons;   
}

/*
 * Detect if the HW supports spreading a CCK signal on channel 14
 */
HAL_BOOL
ar5416IsJapanChannelSpreadSupported(struct ath_hal *ah)
{
        return AH_TRUE;
}

/*
 * Get the rssi of frame curently being received.
 */
u_int32_t
ar5416GetCurRssi(struct ath_hal *ah)
{
    if (AR_SREV_MERLIN_10_OR_LATER(ah))
        return (OS_REG_READ(ah, AR9280_PHY_CURRENT_RSSI) & 0xff);
    else
        return (OS_REG_READ(ah, AR_PHY_CURRENT_RSSI) & 0xff);
}

u_int
ar5416GetDefAntenna(struct ath_hal *ah)
{
        return (OS_REG_READ(ah, AR_DEF_ANTENNA) & 0x7);
}

/* Setup coverage class */
void
ar5416SetCoverageClass(struct ath_hal *ah, u_int8_t coverageclass, int now)
{
}

void
ar5416SetDefAntenna(struct ath_hal *ah, u_int antenna)
{
        OS_REG_WRITE(ah, AR_DEF_ANTENNA, (antenna & 0x7));
}

HAL_BOOL
ar5416SetAntennaSwitch(struct ath_hal *ah,
	HAL_ANT_SETTING settings, HAL_CHANNEL *chan, u_int8_t *tx_chainmask,
    u_int8_t *rx_chainmask, u_int8_t *antenna_cfgd)
{
#define ANTENNA0_CHAINMASK    0x1
#define ANTENNA1_CHAINMASK    0x2
    struct ath_hal_5416 *ahp = AH5416(ah);
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);

    if (AR_SREV_MERLIN(ah) || AR_SREV_KIWI(ah)) {
        /* Antenna selection in Merlin is done by setting the tx/rx chainmasks approp. */
        if (!ahp->ah_tx_chainmask_cfg) {
            /* Store configured chainmask settings */
            ahp->ah_tx_chainmask_cfg = *tx_chainmask;
            ahp->ah_rx_chainmask_cfg = *rx_chainmask;
        }

        switch (settings) {
        case HAL_ANT_FIXED_A:
            /* Enable first antenna only */
            *tx_chainmask = ANTENNA0_CHAINMASK;
            *rx_chainmask = ANTENNA0_CHAINMASK;
            *antenna_cfgd = AH_TRUE;
            break;
        case HAL_ANT_FIXED_B:
            /* Enable second antenna only, after checking capability */
            if (ahpriv->ah_caps.hal_tx_chain_mask > ANTENNA1_CHAINMASK) {
                *tx_chainmask = ANTENNA1_CHAINMASK;
            }
            *rx_chainmask = ANTENNA1_CHAINMASK;
            *antenna_cfgd = AH_TRUE;
            break;
        case HAL_ANT_VARIABLE:
            /* Restore original chainmask settings */
            *tx_chainmask = ahp->ah_tx_chainmask_cfg;
            *rx_chainmask = ahp->ah_rx_chainmask_cfg;
            *antenna_cfgd = AH_TRUE;
            break;
        default:
            break;
        }
    } else {

        /*
         * Owl does not support diversity or changing antennas.
         *
         * Instead this API and function are defined differently for AR5416.
         * To support Tablet PC's, this interface allows the system
         * to dramatically reduce the TX power on a particular chain.
         * 
         * Based on the value of (redefined) diversityControl, the 
         * reset code will decrease power on chain 0 or chain 1/2.
         *
         * Based on the value of bit 0 of antennaSwitchSwap, the mapping between OID call
         * and chain is defined as:
         *  0:  map A -> 0, B -> 1;
         *  1:  map A -> 1, B -> 0;
         *
         * NOTE:
         *   The devices that use this OID should use a txChainMask and
         *   txChainSelectLegacy setting of 5 or 3 if ANTENNA_FIXED_B is
         *   used in order to ensure an active transmit antenna.  This
         *   API will allow the host to turn off the only transmitting
         *   antenna to ensure the antenna closest to the user's body is
         *   powered-down.
         */

        /* Set antenna control for use during reset sequence by ar5416DecreaseChainPower() */

        /* 
         * Kite is a 1X1 chip, but it still use 2 antennas. 
         * Chip will have the best performance when hardware set to use antenna diversity.
         * Antenna diversity can be disbaled by reconfigure the hardware. Please look at 
         * ar5416Eeprom4kSetBoardValues(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
         * in ar5416_eeprom_4k.c for detail. This subroutine is called during reset sequence.
         * Overthere, hardware bits related to antenna diversity will be configured 
         * correspondingly by checking ahp->ah_diversityControl.
         */
        ahp->ah_diversityControl = settings;
    }

    return AH_TRUE;
#undef ANTENNA0_CHAINMASK
#undef ANTENNA1_CHAINMASK
}

HAL_BOOL
ar5416IsSleepAfterBeaconBroken(struct ath_hal *ah)
{
        return AH_TRUE;
}

HAL_BOOL
ar5416SetSlotTime(struct ath_hal *ah, u_int us)
{
        struct ath_hal_5416 *ahp = AH5416(ah);

        if (us < HAL_SLOT_TIME_9 || us > ar5416MacToUsec(ah, 0xffff)) {
                HDPRINTF(ah, HAL_DBG_RESET, "%s: bad slot time %u\n", __func__, us);
                ahp->ah_slottime = (u_int) -1;  /* restore default handling */
                return AH_FALSE;
        } else {
                /* convert to system clocks */
                OS_REG_WRITE(ah, AR_D_GBL_IFS_SLOT, ar5416MacToClks(ah, us));
                ahp->ah_slottime = us;
                return AH_TRUE;
        }
}

HAL_BOOL
ar5416SetAckTimeout(struct ath_hal *ah, u_int us)
{
        struct ath_hal_5416 *ahp = AH5416(ah);

        if (us > ar5416MacToUsec(ah, MS(0xffffffff, AR_TIME_OUT_ACK))) {
                HDPRINTF(ah, HAL_DBG_RESET, "%s: bad ack timeout %u\n", __func__, us);
                ahp->ah_acktimeout = (u_int) -1; /* restore default handling */
                return AH_FALSE;
        } else {
                /* convert to system clocks */
                OS_REG_RMW_FIELD(ah, AR_TIME_OUT,
                        AR_TIME_OUT_ACK, ar5416MacToClks(ah, us));
                ahp->ah_acktimeout = us;
                return AH_TRUE;
        }
}

u_int
ar5416GetAckTimeout(struct ath_hal *ah)
{
        u_int clks = MS(OS_REG_READ(ah, AR_TIME_OUT), AR_TIME_OUT_ACK);
        return ar5416MacToUsec(ah, clks);      /* convert from system clocks */
}

HAL_STATUS
ar5416SetQuiet(struct ath_hal *ah, u_int32_t period, u_int32_t duration, u_int32_t nextStart, HAL_QUIET_FLAG flag)
{
    struct ath_hal_private  *ap  = AH_PRIVATE(ah);
    u_int32_t period_us = TU_TO_USEC(period); /* covert to us unit */
    u_int32_t nextStart_us = TU_TO_USEC(nextStart); /* convert to us unit */
    if (flag & HAL_QUIET_ENABLE) {
        if ((!nextStart) || (flag & HAL_QUIET_ADD_CURRENT_TSF)) {
            /* Add the nextStart offset to the current TSF */
            nextStart_us += OS_REG_READ(ah, AR_TSF_L32);
        }
        if (flag & HAL_QUIET_ADD_SWBA_RESP_TIME) {
                nextStart_us += ap->ah_config.ath_hal_sw_beacon_response_time;
        }
        OS_REG_RMW_FIELD(ah, AR_QUIET1, AR_QUIET1_QUIET_ACK_CTS_ENABLE, 1);
        OS_REG_WRITE(ah, AR_QUIET2, SM(duration, AR_QUIET2_QUIET_DUR));
        OS_REG_WRITE(ah, AR_QUIET_PERIOD, period_us);
        OS_REG_WRITE(ah, AR_NEXT_QUIET_TIMER, nextStart_us);
        OS_REG_SET_BIT(ah, AR_TIMER_MODE, AR_QUIET_TIMER_EN);
    }
    else {
        OS_REG_CLR_BIT(ah, AR_TIMER_MODE, AR_QUIET_TIMER_EN);
    }
    return HAL_OK;
}

void
ar5416SetPCUConfig(struct ath_hal *ah)
{
        ar5416SetOperatingMode(ah, AH_PRIVATE(ah)->ah_opmode);
}

HAL_STATUS
ar5416GetCapability(struct ath_hal *ah, HAL_CAPABILITY_TYPE type,
        u_int32_t capability, u_int32_t *result)
{
        struct ath_hal_5416 *ahp = AH5416(ah);
        const HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;

        switch (type) {
        case HAL_CAP_CIPHER:            /* cipher handled in hardware */
                switch (capability) {
                case HAL_CIPHER_AES_CCM:
                case HAL_CIPHER_AES_OCB:
                case HAL_CIPHER_TKIP:
                case HAL_CIPHER_WEP:
                case HAL_CIPHER_MIC:
                case HAL_CIPHER_CLR:
                        return HAL_OK;
#if ATH_SUPPORT_WAPI
                case HAL_CIPHER_WAPI:
                    if (AR_SREV_MERLIN_10_OR_LATER(ah))
                        return HAL_OK;
                    else
                        return HAL_ENOTSUPP;
#endif
                default:
                        return HAL_ENOTSUPP;
                }
#if ATH_SUPPORT_WAPI
		case HAL_CAP_WAPI_MIC:
			 return HAL_OK;
#endif
        case HAL_CAP_TKIP_MIC:          /* handle TKIP MIC in hardware */
                switch (capability) {
                case 0:                 /* hardware capability */
                        return HAL_OK;
                case 1:
                        return (ahp->ah_staId1Defaults &
                            AR_STA_ID1_CRPT_MIC_ENABLE) ?  HAL_OK : HAL_ENXIO;
                }
                return HAL_EINVAL;
        case HAL_CAP_TKIP_SPLIT:        /* hardware TKIP uses split keys */
                /* XXX check rev when new parts are available */
                return (ahp->ah_miscMode & AR_PCU_MIC_NEW_LOC_ENA) ?
                HAL_ENXIO : HAL_OK;
        case HAL_CAP_WME_TKIPMIC:   /* hardware can do TKIP MIC when WMM is turned on */
                return HAL_OK;
        case HAL_CAP_PHYCOUNTERS:       /* hardware PHY error counters */
                return ahp->ah_hasHwPhyCounters ? HAL_OK : HAL_ENXIO;
        case HAL_CAP_DIVERSITY:         /* hardware supports fast diversity */
                switch (capability) {
                case 0:                 /* hardware capability */
                        return HAL_OK;
                case 1:                 /* current setting */
                        return (OS_REG_READ(ah, AR_PHY_CCK_DETECT) &
                                AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV) ?
                                HAL_OK : HAL_ENXIO;
                }
                return HAL_EINVAL;
        case HAL_CAP_TPC:
                switch (capability) {
                case 0:                 /* hardware capability */
                        return HAL_OK;
                case 1:
                        return AH_PRIVATE(ah)->ah_config.ath_hal_desc_tpc ?
                               HAL_OK : HAL_ENXIO;
                }
                return HAL_OK;
        case HAL_CAP_PHYDIAG:           /* radar pulse detection capability */
                return HAL_OK;
        case HAL_CAP_MCAST_KEYSRCH:     /* multicast frame keycache search */
                switch (capability) {
                case 0:                 /* hardware capability */
                    return HAL_OK;
                case 1:
                        return (ahp->ah_staId1Defaults &
                            AR_STA_ID1_MCAST_KSRCH) ? HAL_OK : HAL_ENXIO;
                }
                return HAL_EINVAL;
        case HAL_CAP_TSF_ADJUST:        /* hardware has beacon tsf adjust */
                switch (capability) {
                case 0:                 /* hardware capability */
                        return pCap->hal_tsf_add_support ? HAL_OK : HAL_ENOTSUPP;
                case 1:
                        return (ahp->ah_miscMode & AR_PCU_TX_ADD_TSF) ?
                                HAL_OK : HAL_ENXIO;
                }
                return HAL_EINVAL;
        case HAL_CAP_RFSILENT:		/* rfsilent support  */
                if (capability == 3) {  /* rfkill interrupt */
                    /*
                     * XXX: Interrupt-based notification of RF Kill state 
                     *      changes not working yet. Report that this feature
                     *      is not supported so that polling is used instead.
                     */
                    return (HAL_ENOTSUPP);
                }
                return ath_hal_getcapability(ah, type, capability, result);
        case HAL_CAP_4ADDR_AGGR:
                /* Following two macVersion have a bug in processing 
                 * 4addr aggregation 
                 */
                return ((AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_OWL_PCI) ||
                        (AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_OWL_PCIE)) ?
                        HAL_ENOTSUPP : HAL_OK;
        case HAL_CAP_BB_RIFS_HANG:
                /* Track chips that are known to have BB hangs related
                 * to RIFS.
                 */
                return (AR_SREV_SOWL(ah) ||
                        AR_SREV_HOWL(ah)) ?
                        HAL_OK : HAL_ENOTSUPP;
        case HAL_CAP_BB_DFS_HANG:
                /* Track chips that are known to have BB hangs related
                 * to DFS.
                 */
                return (AR_SREV_SOWL(ah) ||
                        AR_SREV_HOWL(ah)) ?
                        HAL_OK : HAL_ENOTSUPP;
        case HAL_CAP_BB_RX_CLEAR_STUCK_HANG:
                /* Track chips that are known to have BB hangs related
                 * to rx_clear stuck low.
                 */
                return AR_SREV_MERLIN(ah) ? HAL_OK : HAL_ENOTSUPP;
        case HAL_CAP_MAC_HANG:
                /* Track chips that are known to have MAC hangs.
                 */
                return ((AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_OWL_PCI) ||
                        (AH_PRIVATE(ah)->ah_mac_version == AR_SREV_VERSION_OWL_PCIE) ||
                        (AR_SREV_SOWL(ah) || AR_SREV_HOWL(ah))) ?
                        HAL_OK : HAL_ENOTSUPP;
       case HAL_CAP_RIFS_RX_ENABLED:
                /* Is RIFS RX currently enabled */
                return (ahp->ah_rifs_enabled == AH_TRUE) ?
                        HAL_OK : HAL_ENOTSUPP;
        case HAL_CAP_ANT_CFG_2GHZ:
                *result = pCap->hal_num_ant_cfg_2ghz;
                return HAL_OK;
        case HAL_CAP_ANT_CFG_5GHZ:
                *result = pCap->hal_num_ant_cfg_5ghz;
                return HAL_OK;
        case HAL_CAP_RX_STBC:
                *result = pCap->hal_rx_stbc_support;
                return HAL_OK;
        case HAL_CAP_TX_STBC:
                *result = pCap->hal_tx_stbc_support;
                return HAL_OK;
        case HAL_CAP_DYNAMIC_SMPS:
                return (AR_SREV_MERLIN_20(ah) ||
                         AR_SREV_KIWI(ah)) ? HAL_OK : HAL_ENOTSUPP;
        case HAL_CAP_DS:
                return (AR_SREV_MERLIN_20_OR_LATER(ah) &&
                        (ar5416EepromGet(ahp, EEP_RC_CHAIN_MASK) == 1)) ?  
                       HAL_ENOTSUPP : HAL_OK;
        case HAL_CAP_TS:
                return ((pCap->hal_tx_chain_mask & 0x7) != 0x7 ||
                        (pCap->hal_tx_chain_mask & 0x7) != 0x7) ? 
                        HAL_ENOTSUPP : HAL_OK;
        case HAL_CAP_OL_PWRCTRL:
                return ((AR_SREV_MERLIN_20(ah) ||
                         AR_SREV_KIWI_10_OR_LATER(ah)) &&
                        ar5416EepromGet(ahp, EEP_OL_PWRCTRL)) ?
                       HAL_OK : HAL_ENOTSUPP;
        case HAL_CAP_CRDC:
                return HAL_ENOTSUPP;
        case HAL_CAP_MAX_WEP_TKIP_HT20_TX_RATEKBPS:
                /*
                 * Non-kiwi chips can only support up to MCS49 for WEP/TKIP
                 * Kiwi can support all HT20 rates for WEP/TKIP
                 */
                if (!AR_SREV_KIWI_10_OR_LATER(ah))
                    *result = 117000;
                else
                    *result = (u_int32_t)(-1);
                return HAL_OK;
        case HAL_CAP_MAX_WEP_TKIP_HT40_TX_RATEKBPS:
                /*
                 * Non-kiwi chips can support all HT40 rates for WEP/TKIP
                 * Kiwi can only support up to MCS12 for WEP/TKIP
                 */
                if (AR_SREV_KIWI(ah))
                    *result = 162000;
                else
                    *result = (u_int32_t)(-1);
                return HAL_OK;
         case HAL_CAP_GEN_TIMER:
                return pCap->hal_gen_timer_support ? HAL_OK : HAL_ENOTSUPP;
         case HAL_CAP_OFDM_RTSCTS:
                return (AR_SREV_KITE(ah) || AR_SREV_K2(ah))? HAL_OK : HAL_ENOTSUPP;
         default:
                return ath_hal_getcapability(ah, type, capability, result);
        }
}

void
ar5416GetHwHangs(struct ath_hal *ah, hal_hw_hangs_t *hangs)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    *hangs = 0;

    if (ar5416GetCapability(ah, HAL_CAP_BB_RIFS_HANG, 0, AH_NULL) == HAL_OK)
        *hangs |= HAL_RIFS_BB_HANG_WAR;
    if (ar5416GetCapability(ah, HAL_CAP_BB_DFS_HANG, 0, AH_NULL) == HAL_OK)
        *hangs |= HAL_DFS_BB_HANG_WAR;
    if (ar5416GetCapability(ah, HAL_CAP_BB_RX_CLEAR_STUCK_HANG, 0, AH_NULL)
        == HAL_OK)
        *hangs |= HAL_RX_STUCK_LOW_BB_HANG_WAR;
    if (ar5416GetCapability(ah, HAL_CAP_MAC_HANG, 0, AH_NULL) == HAL_OK)
        *hangs |= HAL_MAC_HANG_WAR;

    ahp->ah_hang_wars = *hangs;
}

HAL_BOOL
ar5416SetCapability(struct ath_hal *ah, HAL_CAPABILITY_TYPE type,
        u_int32_t capability, u_int32_t setting, HAL_STATUS *status)
{
        struct ath_hal_5416 *ahp = AH5416(ah);
        const HAL_CAPABILITIES *pCap = &AH_PRIVATE(ah)->ah_caps;
        u_int32_t v;

        switch (type) {
#if ATH_SUPPORT_WAPI
		case HAL_CAP_WAPI_MIC:
			ahp->ah_staId1Defaults |= AR_STA_ID1_CRPT_MIC_ENABLE;
			return AH_TRUE;
#endif
        case HAL_CAP_TKIP_MIC:          /* handle TKIP MIC in hardware */
                if (setting)
                        ahp->ah_staId1Defaults |= AR_STA_ID1_CRPT_MIC_ENABLE;
                else
                        ahp->ah_staId1Defaults &= ~AR_STA_ID1_CRPT_MIC_ENABLE;
                return AH_TRUE;
        case HAL_CAP_DIVERSITY:
                v = OS_REG_READ(ah, AR_PHY_CCK_DETECT);
                if (setting)
                        v |= AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV;
                else
                        v &= ~AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV;
                OS_REG_WRITE(ah, AR_PHY_CCK_DETECT, v);
                return AH_TRUE;
        case HAL_CAP_DIAG:              /* hardware diagnostic support */
                /*
                 * NB: could split this up into virtual capabilities,
                 *     (e.g. 1 => ACK, 2 => CTS, etc.) but it hardly
                 *     seems worth the additional complexity.
                 */
#ifdef AH_DEBUG
                AH_PRIVATE(ah)->ah_diagreg = setting;
#else
                AH_PRIVATE(ah)->ah_diagreg = setting & 0x6;     /* ACK+CTS */
#endif
                OS_REG_WRITE(ah, AR_DIAG_SW, AH_PRIVATE(ah)->ah_diagreg);
                return AH_TRUE;
        case HAL_CAP_TPC:
                AH_PRIVATE(ah)->ah_config.ath_hal_desc_tpc = (setting != 0);
                return AH_TRUE;
        case HAL_CAP_MCAST_KEYSRCH:     /* multicast frame keycache search */
                if (setting)
                        ahp->ah_staId1Defaults |= AR_STA_ID1_MCAST_KSRCH;
                else
                        ahp->ah_staId1Defaults &= ~AR_STA_ID1_MCAST_KSRCH;
                return AH_TRUE;
        case HAL_CAP_TSF_ADJUST:        /* hardware has beacon tsf adjust */
                if (pCap->hal_tsf_add_support) {
                        if (setting)
                                ahp->ah_miscMode |= AR_PCU_TX_ADD_TSF;
                        else
                                ahp->ah_miscMode &= ~AR_PCU_TX_ADD_TSF;
                        return AH_TRUE;
                }
                /* fall thru... */
        default:
                return ath_hal_setcapability(ah, type, capability,
                                setting, status);
        }
}

#ifdef AH_DEBUG
static void
ar5416PrintReg(struct ath_hal *ah, u_int32_t args)
{
    u_int32_t i=0;

    /* Read 0x80d0 to trigger pcie analyzer */
    HDPRINTF(ah, HAL_DBG_PRINT_REG,"0x%04x 0x%08x\n", 0x80d0, OS_REG_READ(ah, 0x80d0));

    if (args & HAL_DIAG_PRINT_REG_COUNTER) {
        struct ath_hal_5416 *ahp = AH5416(ah);
        u_int32_t tf, rf, rc, cc;

        tf = OS_REG_READ(ah, AR_TFCNT);
        rf = OS_REG_READ(ah, AR_RFCNT);
        rc = OS_REG_READ(ah, AR_RCCNT);
        cc = OS_REG_READ(ah, AR_CCCNT);
        
        HDPRINTF(ah, HAL_DBG_PRINT_REG, "AR_TFCNT Diff= 0x%x\n", tf - ahp->lasttf);
        HDPRINTF(ah, HAL_DBG_PRINT_REG, "AR_RFCNT Diff= 0x%x\n", rf - ahp->lastrf);
        HDPRINTF(ah, HAL_DBG_PRINT_REG, "AR_RCCNT Diff= 0x%x\n", rc - ahp->lastrc);
        HDPRINTF(ah, HAL_DBG_PRINT_REG, "AR_CCCNT Diff= 0x%x\n", cc - ahp->lastcc);

        ahp->lasttf = tf;
        ahp->lastrf = rf;
        ahp->lastrc = rc;
        ahp->lastcc = cc;

        HDPRINTF(ah, HAL_DBG_PRINT_REG, "DMADBG0 = 0x%x\n", OS_REG_READ(ah, AR_DMADBG_0));
        HDPRINTF(ah, HAL_DBG_PRINT_REG, "DMADBG1 = 0x%x\n", OS_REG_READ(ah, AR_DMADBG_1));
        HDPRINTF(ah, HAL_DBG_PRINT_REG, "DMADBG2 = 0x%x\n", OS_REG_READ(ah, AR_DMADBG_2));
        HDPRINTF(ah, HAL_DBG_PRINT_REG, "DMADBG3 = 0x%x\n", OS_REG_READ(ah, AR_DMADBG_3));
        HDPRINTF(ah, HAL_DBG_PRINT_REG, "DMADBG4 = 0x%x\n", OS_REG_READ(ah, AR_DMADBG_4));
        HDPRINTF(ah, HAL_DBG_PRINT_REG, "DMADBG5 = 0x%x\n", OS_REG_READ(ah, AR_DMADBG_5));
        HDPRINTF(ah, HAL_DBG_PRINT_REG, "DMADBG6 = 0x%x\n", OS_REG_READ(ah, AR_DMADBG_6));
        HDPRINTF(ah, HAL_DBG_PRINT_REG, "DMADBG7 = 0x%x\n", OS_REG_READ(ah, AR_DMADBG_7));
    }

    if (args & HAL_DIAG_PRINT_REG_ALL) {
        for(i=0x8; i<=0xB8; i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        for(i=0x800; i<=(0x800 + (10 << 2)); i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", 0x840, OS_REG_READ(ah, 0x840));

        HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", 0x880, OS_REG_READ(ah, 0x880));

        for(i=0x8C0; i<=(0x8C0 + (10 << 2)); i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        for(i=0xa00; i<=(0xa00 + (10 << 2)); i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        for(i=0x1F00; i<=0x1F04; i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        for(i=0x4000; i<=0x408C; i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        for(i=0x5000; i<=0x503C; i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        for(i=0x7040; i<=0x7058; i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        for(i=0x8000; i<=0x8098; i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        for(i=0x80D4; i<=0x8200; i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        for(i=0x8240; i<=0x97FC; i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        for(i=0x9800; i<=0x99f0; i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        for(i=0x9c10; i<=0x9CFC; i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }

        for(i=0xA200; i<=0xA26C; i += sizeof(u_int32_t))
        {
            HDPRINTF(ah, HAL_DBG_PRINT_REG, "0x%04x 0x%08x\n", i, OS_REG_READ(ah, i));
        }
    }
}
#endif

HAL_BOOL
ar5416GetDiagState(struct ath_hal *ah, int request,
        const void *args, u_int32_t argsize,
        void **result, u_int32_t *resultsize)
{
        struct ath_hal_5416 *ahp = AH5416(ah);

        (void) ahp;
        if (ath_hal_getdiagstate(ah, request, args, argsize, result, resultsize))
            return AH_TRUE;

        switch (request) {
#ifdef AH_PRIVATE_DIAG
        unsigned long tmp;
#if 0   // XXX - TODO
        const EEPROM_POWER_EXPN_2133 *pe;
#endif
        case HAL_DIAG_EEPROM:
                *result = &ahp->ah_eeprom;
                *resultsize = sizeof(HAL_EEPROM);
                return AH_TRUE;

#if 0   // XXX - TODO
        case HAL_DIAG_EEPROM_EXP_11A:
        case HAL_DIAG_EEPROM_EXP_11B:
        case HAL_DIAG_EEPROM_EXP_11G:
                pe = &ahp->ah_modePowerArray2133[
                        request - HAL_DIAG_EEPROM_EXP_11A];
                *result = pe->p_channels;
                *resultsize = (*result == AH_NULL) ? 0 :
                        roundup(sizeof(u_int16_t) * pe->num_channels,
                                sizeof(u_int32_t)) +
                        sizeof(EXPN_DATA_PER_CHANNEL_2133) * pe->num_channels;
                return AH_TRUE;
#endif
        case HAL_DIAG_RFGAIN:
                *result = &ahp->ah_gainValues;
                *resultsize = sizeof(GAIN_VALUES);
                return AH_TRUE;
        case HAL_DIAG_RFGAIN_CURSTEP:
                tmp = (unsigned long ) ahp->ah_gainValues.currStep;
                *result = (void *) tmp;
                *resultsize = (*result == AH_NULL) ?
                        0 : sizeof(GAIN_OPTIMIZATION_STEP);
                return AH_TRUE;
#if 0   // XXX - TODO
        case HAL_DIAG_PCDAC:
                *result = ahp->ah_pcdacTable;
                *resultsize = ahp->ah_pcdacTableSize;
                return AH_TRUE;
#endif
        case HAL_DIAG_TXRATES:
                *result = &ahp->ah_ratesArray[0];
                *resultsize = sizeof(ahp->ah_ratesArray);
                return AH_TRUE;
        case HAL_DIAG_ANI_CURRENT:
                *result = ar5416AniGetCurrentState(ah);
                *resultsize = (*result == AH_NULL) ?
                        0 : sizeof(struct ar5416AniState);
                return AH_TRUE;
        case HAL_DIAG_ANI_STATS:
                *result = ar5416AniGetCurrentStats(ah);
                *resultsize = (*result == AH_NULL) ?
                        0 : sizeof(struct ar5416Stats);
                return AH_TRUE;
        case HAL_DIAG_ANI_CMD:
                if (argsize != 2*sizeof(u_int32_t))
                        return AH_FALSE;
                ar5416AniControl(ah, ((const u_int32_t *)args)[0],
                        ((const u_int32_t *)args)[1], AH_FALSE);
                return AH_TRUE;
        case HAL_DIAG_TXCONT:
                tmp = (unsigned long)args;
                AR5416_CONTTXMODE(ah, (struct ath_desc *)tmp, argsize );
                return AH_TRUE;
#endif /* AH_PRIVATE_DIAG */
#ifdef AH_DEBUG
        case HAL_DIAG_PRINT_REG:
            ar5416PrintReg(ah, *((const u_int32_t *)args));
            return AH_TRUE;
#endif
        default:
            break;
        }

        return AH_FALSE;
}

void
ar5416DmaRegDump(struct ath_hal *ah)
{
#define NUM_DMA_DEBUG_REGS  8
#define NUM_QUEUES          10

    u_int32_t val[NUM_DMA_DEBUG_REGS];
    int       qcuOffset = 0, dcuOffset = 0;
    u_int32_t *qcuBase  = &val[0], *dcuBase = &val[4], reg;
    int       i, j, k;
    int16_t nfarray[NUM_NF_READINGS];
    HAL_NFCAL_HIST_FULL *h =
#ifdef ATH_NF_PER_CHAN
        &AH_PRIVATE(ah)->ah_curchan->nf_cal_hist;
#else
        &AH_PRIVATE(ah)->nf_cal_hist;
#endif

     /* selecting DMA OBS 8 */
    OS_REG_WRITE(ah,AR_MACMISC, 
        ((AR_MACMISC_DMA_OBS_LINE_8 << AR_MACMISC_DMA_OBS_S) | 
         (AR_MACMISC_MISC_OBS_BUS_1 << AR_MACMISC_MISC_OBS_BUS_MSB_S)));
 
    ath_hal_printf(ah, "Raw DMA Debug values:\n");
    for (i = 0; i < NUM_DMA_DEBUG_REGS; i++) {
        if (i % 4 == 0) {
            ath_hal_printf(ah, "\n");
        }

        val[i] = OS_REG_READ(ah, AR_DMADBG_0 + (i * sizeof(u_int32_t)));
        ath_hal_printf(ah, "%d: %08x ", i, val[i]);
    }

    ath_hal_printf(ah, "\n\n");
    ath_hal_printf(ah, "Num QCU: chain_st fsp_ok fsp_st DCU: chain_st\n");

    for (i = 0; i < NUM_QUEUES; i++, qcuOffset += 4, dcuOffset += 5) {
        if (i == 8) {
            /* only 8 QCU entries in val[0] */
            qcuOffset = 0;
            qcuBase++;
        }

        if (i == 6) {
            /* only 6 DCU entries in val[4] */
            dcuOffset = 0;
            dcuBase++;
        }

        ath_hal_printf(ah,  "%2d          %2x      %1x     %2x           %2x\n",
                  i,
                  (*qcuBase & (0x7 << qcuOffset)) >> qcuOffset,
                  (*qcuBase & (0x8 << qcuOffset)) >> (qcuOffset + 3),
                  val[2] & (0x7 << (i * 3)) >> (i * 3),
                  (*dcuBase & (0x1f << dcuOffset)) >> dcuOffset);
    }

    ath_hal_printf(ah, "\n");
    ath_hal_printf(ah, "qcu_stitch state:   %2x    qcu_fetch state:        %2x\n",
              (val[3] & 0x003c0000) >> 18, (val[3] & 0x03c00000) >> 22);
    ath_hal_printf(ah,  "qcu_complete state: %2x    dcu_complete state:     %2x\n",
              (val[3] & 0x1c000000) >> 26, (val[6] & 0x3));
    ath_hal_printf(ah,  "dcu_arb state:      %2x    dcu_fp state:           %2x\n",
             (val[5] & 0x06000000) >> 25, (val[5] & 0x38000000) >> 27);
    ath_hal_printf(ah,  "chan_idle_dur:     %3d    chan_idle_dur_valid:     %1d\n",
              (val[6] & 0x000003fc) >> 2, (val[6] & 0x00000400) >> 10);
    ath_hal_printf(ah,  "txfifo_valid_0:      %1d    txfifo_valid_1:          %1d\n",
              (val[6] & 0x00000800) >> 11, (val[6] & 0x00001000) >> 12);
    ath_hal_printf(ah,  "txfifo_dcu_num_0:   %2d    txfifo_dcu_num_1:       %2d\n",
              (val[6] & 0x0001e000) >> 13, (val[6] & 0x001e0000) >> 17);

    ath_hal_printf(ah, "pcu observe 0x%x \n", OS_REG_READ(ah, AR_OBS_BUS_1)); 
    ath_hal_printf(ah, "AR_CR 0x%x \n", OS_REG_READ(ah,AR_CR));

    ar5416UploadNoiseFloor(ah, nfarray);
    ath_hal_printf(ah, "\n");
    ath_hal_printf(ah, "Min CCA Out:\n");
    ath_hal_printf(ah, "\t\tChain 0\t\tChain 1\t\tChain 2\n");
    ath_hal_printf(ah, "Control:\t%8d\t%8d\t%8d\n",
                   nfarray[0], nfarray[1], nfarray[2]);
    ath_hal_printf(ah, "Extension:\t%8d\t%8d\t%8d\n\n",
                   nfarray[3], nfarray[4], nfarray[5]);

    for (i = 0; i < NUM_NF_READINGS; i++) {
        ath_hal_printf(ah, "%s Chain %d NF History:\n",
                       ((i < 3) ? "Control " : "Extension "), i%3);
        for (j=0, k = h->base.curr_index;
             j < HAL_NF_CAL_HIST_LEN_FULL;
             j++, k++) {
            ath_hal_printf(ah, "Element %d: %d\n",
                           j, h->nf_cal_buffer[k%HAL_NF_CAL_HIST_LEN_FULL][i]);
        }
        ath_hal_printf(ah, "Last Programmed NF: %d\n\n", h->base.priv_nf[i]);
    }

    reg = OS_REG_READ(ah, AR_PHY_FIND_SIGLOW);
    ath_hal_printf(ah, "FIRStep Low = 0x%x (%d)\n",
                   MS(reg, AR_PHY_FIND_SIGLOW_FIRSTEP_LOW),
                   MS(reg, AR_PHY_FIND_SIGLOW_FIRSTEP_LOW));
    reg = OS_REG_READ(ah, AR_PHY_DESIRED_SZ);
    ath_hal_printf(ah, "Total Desired = 0x%x (%d)\n",
                   MS(reg, AR_PHY_DESIRED_SZ_TOT_DES),
                   MS(reg, AR_PHY_DESIRED_SZ_TOT_DES));
    ath_hal_printf(ah, "ADC Desired = 0x%x (%d)\n",
                   MS(reg, AR_PHY_DESIRED_SZ_ADC),
                   MS(reg, AR_PHY_DESIRED_SZ_ADC));
    reg = OS_REG_READ(ah, AR_PHY_FIND_SIG);
    ath_hal_printf(ah, "FIRStep = 0x%x (%d)\n",
                   MS(reg, AR_PHY_FIND_SIG_FIRSTEP),
                   MS(reg, AR_PHY_FIND_SIG_FIRSTEP));
    reg = OS_REG_READ(ah, AR_PHY_AGC_CTL1);
    ath_hal_printf(ah, "Coarse High = 0x%x (%d)\n",
                   MS(reg, AR_PHY_AGC_CTL1_COARSE_HIGH),
                   MS(reg, AR_PHY_AGC_CTL1_COARSE_HIGH));
    ath_hal_printf(ah, "Coarse Low = 0x%x (%d)\n",
                   MS(reg, AR_PHY_AGC_CTL1_COARSE_LOW),
                   MS(reg, AR_PHY_AGC_CTL1_COARSE_LOW));
    ath_hal_printf(ah, "Coarse Power Constant = 0x%x (%d)\n",
                   MS(reg, AR_PHY_AGC_CTL1_COARSE_PWR_CONST),
                   MS(reg, AR_PHY_AGC_CTL1_COARSE_PWR_CONST));
    reg = OS_REG_READ(ah, AR_PHY_TIMING5);
    ath_hal_printf(ah, "Enable Cyclic Power Thresh = %d\n",
                   MS(reg, AR_PHY_TIMING5_CYCPWR_THR1_ENABLE));
    ath_hal_printf(ah, "Cyclic Power Thresh = 0x%x (%d)\n",
                   MS(reg, AR_PHY_TIMING5_CYCPWR_THR1),
                   MS(reg, AR_PHY_TIMING5_CYCPWR_THR1));
    ath_hal_printf(ah, "Cyclic Power Thresh 1A= 0x%x (%d)\n",
                   MS(reg, AR_PHY_TIMING5_CYCPWR_THR1A),
                   MS(reg, AR_PHY_TIMING5_CYCPWR_THR1A));
    reg = OS_REG_READ(ah, AR_PHY_DAG_CTRLCCK);
    ath_hal_printf(ah, "Barker RSSI Thresh Enable = %d\n",
                   MS(reg, AR_PHY_DAG_CTRLCCK_EN_RSSI_THR));
    ath_hal_printf(ah, "Barker RSSI Thresh = 0x%x (%d)\n",
                   MS(reg, AR_PHY_DAG_CTRLCCK_RSSI_THR),
                   MS(reg, AR_PHY_DAG_CTRLCCK_RSSI_THR));
}

/*
 * Return the busy for rx_frame, rx_clear, and tx_frame
 */
u_int32_t
ar5416GetMibCycleCountsPct(struct ath_hal *ah, u_int32_t *rxc_pcnt, u_int32_t *rxf_pcnt, u_int32_t *txf_pcnt)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t good = 1;

    u_int32_t rc = OS_REG_READ(ah, AR_RCCNT);
    u_int32_t rf = OS_REG_READ(ah, AR_RFCNT);
    u_int32_t tf = OS_REG_READ(ah, AR_TFCNT);
    u_int32_t cc = OS_REG_READ(ah, AR_CCCNT); /* read cycles last */

    if (ahp->ah_cycles == 0 || ahp->ah_cycles > cc) {
        /*
         * Cycle counter wrap (or initial call); it's not possible
         * to accurately calculate a value because the registers
         * right shift rather than wrap--so punt and return 0.
         */
        HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: cycle counter wrap. ExtBusy = 0\n", __func__);
        good = 0;
    } else {
        u_int32_t cc_d = cc - ahp->ah_cycles;
        u_int32_t rc_d = rc - ahp->ah_rx_clear;
        u_int32_t rf_d = rf - ahp->ah_rx_frame;
        u_int32_t tf_d = tf - ahp->ah_tx_frame;

        if (cc_d != 0) {
            *rxc_pcnt = rc_d * 100 / cc_d;
            *rxf_pcnt = rf_d * 100 / cc_d;
            *txf_pcnt = tf_d * 100 / cc_d;
        } else {
            good = 0;
        }
    }

    ahp->ah_cycles = cc;
    ahp->ah_rx_frame = rf;
    ahp->ah_rx_clear = rc;
    ahp->ah_tx_frame = tf;
    
    return good;
}

/*
 * Return approximation of extension channel busy over an time interval
 * 0% (clear) -> 100% (busy)
 * -1 for invalid estimate
 */
int8_t
ar5416Get11nExtBusy(struct ath_hal *ah)
{
/* Overflow condition to check before multiplying to get % (x * 100 > 0xFFFFFFFF ) => (x > 0x28F5C28) */
#define OVERFLOW_LIMIT  0x28F5C28
#define ERROR_CODE      -1

    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t busy = 0; /* percentage */
    int8_t busyper = 0;
    u_int32_t cycleCount, ctlBusy, extBusy;

    /* cycleCount will always be the first to wrap; therefore, read it last
     * This sequence of reads is not atomic, and MIB counter wrap
     * could happen during it ?
     */
    ctlBusy = OS_REG_READ(ah, AR_RCCNT);
    extBusy = OS_REG_READ(ah, AR_EXTRCCNT);
    cycleCount = OS_REG_READ(ah, AR_CCCNT);

    if ((ahp->ah_cycleCount == 0) || (ahp->ah_cycleCount > cycleCount) ||
	(ahp->ah_ctlBusy > ctlBusy) ||(ahp->ah_extBusy > extBusy)) {
        /*
         * Cycle counter wrap (or initial call); it's not possible
         * to accurately calculate a value because the registers
         * right shift rather than wrap--so punt and return 0.
         */
        busyper = ERROR_CODE;
        HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: cycle counter wrap. ExtBusy = 0\n", __func__);

    } else {
        u_int32_t cycleDelta = cycleCount - ahp->ah_cycleCount;
        u_int32_t ctlBusyDelta = ctlBusy - ahp->ah_ctlBusy;
        u_int32_t extBusyDelta = extBusy - ahp->ah_extBusy;
        u_int32_t ctlClearDelta = 0;

        if (AR_SREV_MERLIN_10_OR_LATER(ah)) {

            /* Compute extension channel busy percentage
             * Overflow condition: 0xFFFFFFFF < extBusyDelta * 100      
             * Underflow condition/Divide-by-zero: check that cycleDelta >> 7 != 0
             * Will never happen, since (extBusyDelta < cycleDelta) always, and shift necessitated by
             * large extBusyDelta.
             * Due to timing difference to read the registers and counter overflow, it may still happen that
             * cycleDelta >> 7 = 0.
             *
             */
            if (cycleDelta) {
                if (extBusyDelta > OVERFLOW_LIMIT) {
                    if (cycleDelta >> 7) {
                        busy = ((extBusyDelta >> 7) * 100) / (cycleDelta  >> 7);
                    } else {
                        busyper = ERROR_CODE;
                    }
                } else {
                    busy = (extBusyDelta * 100) / cycleDelta;
                }
            } else {
                busyper = ERROR_CODE;
            }
        
        } else {
            /* Compute control channel rxclear.
             * The cycle delta may be less than the control channel delta.
             * This could be solved by freezing the timers (or an atomic read,
             * if one was available). Checking for the condition should be
             * sufficient.
             */

  
            /* TOASK: i.e. with the above check in place, and knowing that cycleCount 
             * will be the first to wrap
             * (in which case we return busy=0), no second check is needed */
            if (cycleDelta > ctlBusyDelta) {
                ctlClearDelta = cycleDelta - ctlBusyDelta;
            }

            /* Compute ratio of extension channel busy to control channel clear
             * as an approximation to extension channel cleanliness.
             *
             * According to the hardware folks, ext rxclear is undefined
             * if the ctrl rxclear is de-asserted (i.e. busy)
             * BUT, the MIB counter being read here is MAC_PCU_RX_CLEAR_DIFF_CNT, 
             * which is only incremented when the rx_clear_ext is L, and 
             * rx_clear_ctl is H i.e. this a valid count
             */
            if (ctlClearDelta) {
                 /* Warning : ensure no overflow/underflow 
                  *
                  *  Overflow condition: 0xFFFFFFFF < extBusyDelta * 100      
                  *                       => check before multiplying/dividing to get percentage 
                  *
                  *  Underflow condition/Divide-by-zero: check that ctlClearDelta >> 7 != 0
                  *  Will never happen, since (extBusyDelta < ctlClearDelta) always, and shift necessitated by
                  *  large extBusyDelta.
                  *
                  *  Note : Above Fix results in loss of 7 LSBs of extBusyDelta & ctlClearDelta
                  *         If they are both higher than the overflow threshold, 
                  *         and close enough to differ only in these 7 LSBs, 'busy' will be reported 100% 
                  */
				if (extBusyDelta > OVERFLOW_LIMIT) {
                    if (ctlClearDelta >> 7) {
                        busy =  ((extBusyDelta >> 7) * 100) / (ctlClearDelta >> 7);
                    } else {
                        busyper = ERROR_CODE;
                    }
                } else {
                    busy = (extBusyDelta * 100) / ctlClearDelta;
                }
            } else {
                busyper = ERROR_CODE;
            }
        }
        if (busy > 100) {
            busy = 100;
        }
        if ( busyper != ERROR_CODE ) {
            busyper = busy;
        }

    }

    ahp->ah_cycleCount = cycleCount;
    ahp->ah_ctlBusy = ctlBusy;
    ahp->ah_extBusy = extBusy;

    return busyper;
#undef ERROR_CODE
#undef OVERFLOW_LIMIT
}

u_int32_t
ar5416GetChBusyPct(struct ath_hal *ah)
{
    /*
     * Overflow condition to check before multiplying to get %
     * (x * 100 > 0xFFFFFFFF ) => (x > 0x28F5C28)
     */
#define OVERFLOW_LIMIT  0x28F5C28
#define ERROR_CODE      -1    

    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t busy = 0, extbusy = 0, rf = 0, tf = 0; /* percentage */
    int8_t busyper = 0, extbusyper = 0, rfper = 0, tfper = 0;
    u_int32_t cycle_count, ctl_busy, ext_busy, ctl_rf, ctl_tf;

    /* cycle_count will always be the first to wrap; therefore, read it last
     * This sequence of reads is not atomic, and MIB counter wrap
     * could happen during it ?
     */
    ctl_busy = OS_REG_READ(ah, AR_RCCNT);
    ext_busy = OS_REG_READ(ah, AR_EXTRCCNT);
    ctl_rf = OS_REG_READ(ah, AR_RFCNT);
    ctl_tf = OS_REG_READ(ah, AR_TFCNT);
    cycle_count = OS_REG_READ(ah, AR_CCCNT);

    if ((ahp->ah_cycleCount == 0) || (ahp->ah_cycleCount > cycle_count) ||
        (ahp->ah_ctlBusy > ctl_busy) || (ahp->ah_extBusy > ext_busy) ||
        (ahp->ah_Rf > ctl_rf) || (ahp->ah_Tf > ctl_tf))
        {
            /*
             * Cycle counter wrap (or initial call); it's not possible
             * to accurately calculate a value because the registers
             * right shift rather than wrap--so punt and return 0.
             */
            busyper = ERROR_CODE;
            HDPRINTF(ah, HAL_DBG_CHANNEL,
                     "%s: cycle counter wrap. ExtBusy = 0\n", __func__);
        } else {
        u_int32_t cycle_delta = cycle_count - ahp->ah_cycleCount;
        u_int32_t ctl_busy_delta = ctl_busy - ahp->ah_ctlBusy;
        u_int32_t ext_busy_delta = ext_busy - ahp->ah_extBusy;
        u_int32_t ctl_rf_delta = ctl_rf - ahp->ah_Rf;
        u_int32_t ctl_tf_delta = ctl_tf - ahp->ah_Tf;

        /* ext_busy_delta can't be more than cycle_delta */
        if ((cycle_delta) && (ext_busy_delta <= cycle_delta )) {
            /*
             * Compute extension channel busy percentage
             * Overflow condition: 0xFFFFFFFF < ext_busy_delta * 100
             * Underflow condition/Divide-by-zero: check that cycle_delta >> 7 != 0
             * Will never happen, since (ext_busy_delta < cycle_delta) always,
             * and shift necessitated by large ext_busy_delta.
             * Due to timing difference to read the registers and counter overflow,
             * it may still happen that cycle_delta >> 7 = 0.
             *
             */
            if (ext_busy_delta > OVERFLOW_LIMIT) {
                if (cycle_delta >> 7) {
                    extbusy = ((ext_busy_delta >> 7) * 100) / (cycle_delta  >> 7);
                } else {
                    extbusyper = ERROR_CODE;
                }
            } else {
                extbusy = (ext_busy_delta * 100) / cycle_delta;
            }
        } else {
            extbusyper = ERROR_CODE;
        }

        if (extbusy > 100) {
            extbusy = 100;
        }
        if ( extbusyper != ERROR_CODE ) {
            extbusyper = extbusy;
        }


        /* ctl_busy_delta can't be more than cycle_delta */
        if ((cycle_delta) && (ctl_busy_delta <= cycle_delta )) {
            /*
             * Compute control channel busy percentage
             * Overflow condition: 0xFFFFFFFF < ctl_busy_delta * 100
             * Underflow condition/Divide-by-zero: check that cycle_delta >> 7 != 0
             * Will never happen, since (ctl_busy_delta < cycle_delta) always,
             * and shift necessitated by large ctl_busy_delta.
             * Due to timing difference to read the registers and counter overflow,
             * it may still happen that cycle_delta >> 7 = 0.
             *
             */
            if (ctl_busy_delta > OVERFLOW_LIMIT) {
                if (cycle_delta >> 7) {
                    busy = ((ctl_busy_delta >> 7) * 100) / (cycle_delta  >> 7);
                } else {
                    busyper = ERROR_CODE;
                }
            } else {
                busy = (ctl_busy_delta * 100) / cycle_delta;
            }
        } else {
            busyper = ERROR_CODE;
        }

        if (busy > 100) {
            busy = 100;
        }
        if ( busyper != ERROR_CODE ) {
            busyper = busy;
        }


        if ((cycle_delta) && (ctl_rf_delta <= cycle_delta )) {
            if (ctl_rf_delta > OVERFLOW_LIMIT) {
                if (cycle_delta >> 7) {
                    rf = ((ctl_rf_delta >> 7) * 100) / (cycle_delta  >> 7);
                } else {
                    rfper = ERROR_CODE;
                }
            } else {
                rf = (ctl_rf_delta * 100) / cycle_delta;
            }
        } else {
            rfper = ERROR_CODE;
        }

        if (rf > 100) {
            rf = 100;
        }
        if ( rfper != ERROR_CODE ) {
            rfper = rf;
        }


        if ((cycle_delta) && (ctl_tf_delta <= cycle_delta )) {
            if (ctl_tf_delta > OVERFLOW_LIMIT) {
                if (cycle_delta >> 7) {
                    tf = ((ctl_tf_delta >> 7) * 100) / (cycle_delta  >> 7);
                } else {
                    tfper = ERROR_CODE;
                }
            } else {
                tf = (ctl_tf_delta * 100) / cycle_delta;
            }
        } else {
            tfper = ERROR_CODE;
        }

        if (tf > 100) {
            tf = 100;
        }
        if ( tfper != ERROR_CODE ) {
            tfper = tf;
        }

    }

    ahp->ah_cycleCount = cycle_count;
    ahp->ah_ctlBusy = ctl_busy;
    ahp->ah_extBusy = ext_busy;
    ahp->ah_Rf = ctl_rf;
    ahp->ah_Tf = ctl_tf;

    return (((tfper & 0xff) << 24) | ((rfper & 0xff ) << 16) |
            ((extbusyper & 0xff) << 8) | ((busyper & 0xff) << 0));
#undef OVERFLOW_LIMIT
#undef ERROR_CODE    
}

/*
 * Configure 20/40 operation
 *
 * 20/40 = joint rx clear (control and extension)
 * 20    = rx clear (control)
 *
 * - NOTE: must stop MAC (tx) and requeue 40 MHz packets as 20 MHz when changing
 *         from 20/40 => 20 only
 */
void
ar5416Set11nMac2040(struct ath_hal *ah, HAL_HT_MACMODE mode)
{
    u_int32_t macmode;

    /* Configure MAC for 20/40 operation */
    if (mode == HAL_HT_MACMODE_2040 && 
        !AH_PRIVATE(ah)->ah_config.ath_hal_cwm_ignore_ext_cca) {
        macmode = AR_2040_JOINED_RX_CLEAR;
    } else {
        macmode = 0;
    }
    OS_REG_WRITE(ah, AR_2040_MODE, macmode);
}

/*
 * Get Rx clear (control/extension channel)
 *
 * Returns active low (busy) for ctrl/ext channel
 * Owl 2.0
 */
HAL_HT_RXCLEAR
ar5416Get11nRxClear(struct ath_hal *ah)
{
    HAL_HT_RXCLEAR rxclear = 0;
    u_int32_t val;

    val = OS_REG_READ(ah, AR_DIAG_SW);

    /* control channel */
    if (val & AR_DIAG_RX_CLEAR_CTL_LOW) {
        rxclear |= HAL_RX_CLEAR_CTL_LOW;
    }
    /* extension channel */
    if (val & AR_DIAG_RX_CLEAR_EXT_LOW) {
        rxclear |= HAL_RX_CLEAR_EXT_LOW;
    }
    return rxclear;
}

/*
 * Set Rx clear (control/extension channel)
 *
 * Useful for forcing the channel to appear busy for
 * debugging/diagnostics
 * Owl 2.0
 */
void
ar5416Set11nRxClear(struct ath_hal *ah, HAL_HT_RXCLEAR rxclear)
{
    /* control channel */
    if (rxclear & HAL_RX_CLEAR_CTL_LOW) {
        OS_REG_SET_BIT(ah, AR_DIAG_SW, AR_DIAG_RX_CLEAR_CTL_LOW);
    } else {
        OS_REG_CLR_BIT(ah, AR_DIAG_SW, AR_DIAG_RX_CLEAR_CTL_LOW);
    }
    /* extension channel */
    if (rxclear & HAL_RX_CLEAR_EXT_LOW) {
        OS_REG_SET_BIT(ah, AR_DIAG_SW, AR_DIAG_RX_CLEAR_EXT_LOW);
    } else {
        OS_REG_CLR_BIT(ah, AR_DIAG_SW, AR_DIAG_RX_CLEAR_EXT_LOW);
    }
}

#ifdef AR5416_EMULATION
/* MAC tracing (emulation only)
 * - stop tracing (ar5416StopMacTrace)
 *   and then use "dumpregs -l" to display MAC trace buffer
 */
void
ar5416InitMacTrace(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_MAC_PCU_LOGIC_ANALYZER_32L, 0xFFFFFFFF);
    OS_REG_WRITE(ah, AR_MAC_PCU_LOGIC_ANALYZER_16U, 0xFFFF);
}

void
ar5416StopMacTrace(struct ath_hal *ah)
{
    OS_REG_CLR_BIT(ah, AR_MAC_PCU_LOGIC_ANALYZER, 
		   (AR_MAC_PCU_LOGIC_ANALYZER_CTL | AR_MAC_PCU_LOGIC_ANALYZER_QCU_SEL));
    OS_REG_SET_BIT(ah, AR_MAC_PCU_LOGIC_ANALYZER, AR_MAC_PCU_LOGIC_ANALYZER_ENABLE | AR_MAC_PCU_LOGIC_ANALYZER_HOLD |
                        (1 << AR_MAC_PCU_LOGIC_ANALYZER_QCU_SEL_S));
}
#endif //AR5416_EMULATION

/*
 * HAL support code for force ppm tracking workaround.
 */

u_int32_t
ar5416PpmGetRssiDump(struct ath_hal *ah)
{
    u_int32_t retval;
    u_int32_t off1;
    u_int32_t off2;

    if (OS_REG_READ(ah, AR_PHY_ANALOG_SWAP) & AR_PHY_SWAP_ALT_CHAIN) {
        off1 = 0x2000;
        off2 = 0x1000;
    } else {
        off1 = 0x1000;
        off2 = 0x2000;
    }

    retval = ((0xff & OS_REG_READ(ah, AR_PHY_CHAN_INFO_GAIN       )) << 0) |
             ((0xff & OS_REG_READ(ah, AR_PHY_CHAN_INFO_GAIN + off1)) << 8) |
             ((0xff & OS_REG_READ(ah, AR_PHY_CHAN_INFO_GAIN + off2)) << 16);

    return retval;
}

u_int32_t
ar5416PpmForce(struct ath_hal *ah)
{
    u_int32_t data_fine;
    u_int32_t data4;
    HAL_BOOL signed_val = AH_FALSE;

    data_fine = AR_PHY_CHAN_INFO_GAIN_DIFF_PPM_MASK & OS_REG_READ(ah, AR_PHY_CHAN_INFO_GAIN_DIFF);
	
    /* 
     * bit [11-0] is new ppm value. bit 11 is the signed bit.
     * So check value from bit[10:0].
     * Now get the abs val of the ppm value read in bit[0:11]. 
     * After that do bound check on abs value.
     * if value is off limit, CAP the value and and restore signed bit.
     */
    if (data_fine & AR_PHY_CHAN_INFO_GAIN_DIFF_PPM_SIGNED_BIT)
    {
         /* get the positive value */
         data_fine = (~data_fine + 1) & AR_PHY_CHAN_INFO_GAIN_DIFF_PPM_MASK;
         signed_val = AH_TRUE;
    }
    if (data_fine > AR_PHY_CHAN_INFO_GAIN_DIFF_UPPER_LIMIT)
    {
        HDPRINTF(ah, HAL_DBG_REG_IO, "%s Correcting ppm out of range %x\n", __func__, (data_fine & 0x7ff));
        data_fine = AR_PHY_CHAN_INFO_GAIN_DIFF_UPPER_LIMIT;
    }
	/* 
     * Restore signed value if changed above. 
     * Use typecast to avoid compilation errors 
     */
	if (signed_val)
		data_fine = (-(int32_t)data_fine) & AR_PHY_CHAN_INFO_GAIN_DIFF_PPM_MASK; 
	
    /* write value */
    data4 = OS_REG_READ(ah, AR_PHY_TIMING2) & ~(AR_PHY_TIMING2_USE_FORCE_PPM | AR_PHY_TIMING2_FORCE_PPM_VAL);
    OS_REG_WRITE(ah, AR_PHY_TIMING2, data4 | data_fine | AR_PHY_TIMING2_USE_FORCE_PPM);

    return data_fine;
 }

void
ar5416PpmUnForce(struct ath_hal *ah)
{
    u_int32_t data4;

    data4 = OS_REG_READ(ah, AR_PHY_TIMING2) & ~AR_PHY_TIMING2_USE_FORCE_PPM;
    OS_REG_WRITE(ah, AR_PHY_TIMING2, data4);
}

u_int32_t 
ar5416PpmArmTrigger(struct ath_hal *ah)
{
    u_int32_t val;
    u_int32_t ret;

    val = OS_REG_READ(ah, AR_PHY_CHAN_INFO_MEMORY);
    ret = OS_REG_READ(ah, AR_TSF_L32);
    OS_REG_WRITE(ah, AR_PHY_CHAN_INFO_MEMORY, val | AR_PHY_CHAN_INFO_MEMORY_CAPTURE_MASK);

    /* return low word of TSF at arm time */
    return ret;
}

int
ar5416PpmGetTrigger(struct ath_hal *ah)
{
    if (OS_REG_READ(ah, AR_PHY_CHAN_INFO_MEMORY) & AR_PHY_CHAN_INFO_MEMORY_CAPTURE_MASK) {
        /* has not triggered yet, return false */
        return 0;
    }

    /* else triggered, return true */
    return 1;
}

/* Sowl HT40 emits some noise in the extension channel when it hits the TX hang. Used as part of Sowl TX hang WAR, mark register 0x981C inactive when TX hang is detected This limits the noise emitted when the hang is hit. */

void
ar5416MarkPhyInactive(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_DIS); 
}

// DEBUG
u_int32_t
ar5416PpmGetForceState(struct ath_hal *ah)
{
    return (OS_REG_READ(ah, AR_PHY_TIMING2) & (AR_PHY_TIMING2_USE_FORCE_PPM | AR_PHY_TIMING2_FORCE_PPM_VAL));
}

/*
 * Return the Cycle counts for rx_frame, rx_clear, and tx_frame
 */
void
ar5416GetMibCycleCounts(struct ath_hal *ah, HAL_COUNTERS* pCnts)
{

    pCnts->tx_frame_count = OS_REG_READ(ah, AR_TFCNT);
    pCnts->rx_frame_count = OS_REG_READ(ah, AR_RFCNT);
    pCnts->rx_clear_count = OS_REG_READ(ah, AR_RCCNT);
    pCnts->cycle_count   = OS_REG_READ(ah, AR_CCCNT);
    pCnts->is_tx_active   = (OS_REG_READ(ah, AR_TFCNT) ==
                           pCnts->tx_frame_count) ? AH_FALSE : AH_TRUE;
    pCnts->is_rx_active   = (OS_REG_READ(ah, AR_RFCNT) ==
                           pCnts->rx_frame_count) ? AH_FALSE : AH_TRUE;
}

void
ar5416ClearMibCounters(struct ath_hal *ah)
{
    u_int32_t regVal;

    regVal = OS_REG_READ(ah, AR_MIBC);
    OS_REG_WRITE(ah, AR_MIBC, regVal | AR_MIBC_CMC);
    OS_REG_WRITE(ah, AR_MIBC, regVal & ~AR_MIBC_CMC);
}

#ifdef ATH_CCX
u_int8_t
ar5416GetCcaThreshold(struct ath_hal *ah)
{
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        return (u_int8_t)MS(OS_REG_READ(ah, AR_PHY_CCA), AR9280_PHY_CCA_THRESH62);
    } else {
        return (u_int8_t)MS(OS_REG_READ(ah, AR_PHY_CCA), AR_PHY_CCA_THRESH62);
    }
}
#endif

/* Enable or Disable RIFS Rx capability as part of SW WAR for Bug 31602 */
HAL_BOOL
ar5416SetRifsDelay(struct ath_hal *ah, HAL_BOOL enable)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    HAL_BOOL is_chan_2g = IS_CHAN_2GHZ(AH_PRIVATE(ah)->ah_curchan);

	ENABLE_REG_WRITE_BUFFER

    if (enable) {
        if (ahp->ah_rifs_enabled == AH_TRUE)
            return AH_TRUE;

        OS_REG_WRITE(ah, AR_PHY_SEARCH_START_DELAY, ahp->ah_rifs_reg[0]);
        OS_REG_WRITE(ah, AR_PHY_HEAVY_CLIP_FACTOR_RIFS,
                     ahp->ah_rifs_reg[1]);

        ahp->ah_rifs_enabled = AH_TRUE;
        OS_MEMZERO(ahp->ah_rifs_reg, sizeof(ahp->ah_rifs_reg));
    } else {
        if (ahp->ah_rifs_enabled == AH_TRUE) {
            ahp->ah_rifs_reg[0] = OS_REG_READ(ah,
                                              AR_PHY_SEARCH_START_DELAY);
            ahp->ah_rifs_reg[1] = OS_REG_READ(ah,
                                              AR_PHY_HEAVY_CLIP_FACTOR_RIFS);
        }
        // Change rifs init delay to 0
        OS_REG_WRITE(ah, AR_PHY_HEAVY_CLIP_FACTOR_RIFS,
                     (ahp->ah_rifs_reg[1] & ~(AR_PHY_RIFS_INIT_DELAY)));
        if (is_chan_2g) {
			if (IS_CHAN_HT40(AH_PRIVATE(ah)->ah_curchan)) {
                OS_REG_WRITE(ah, AR_PHY_SEARCH_START_DELAY, 0x268);
			}
			else { // Sowl 2G HT-20 default is 0x134 for search start delay
                OS_REG_WRITE(ah, AR_PHY_SEARCH_START_DELAY, 0x134);
			}
        } else {
			if (IS_CHAN_HT40(AH_PRIVATE(ah)->ah_curchan)) {
                OS_REG_WRITE(ah, AR_PHY_SEARCH_START_DELAY, 0x370);
			}
			else { // Sowl 5G HT-20 default is 0x1b8 for search start delay
                OS_REG_WRITE(ah, AR_PHY_SEARCH_START_DELAY, 0x1b8);
			}
        }

        ahp->ah_rifs_enabled = AH_FALSE;
    }
    OS_REG_WRITE_FLUSH(ah);

	DISABLE_REG_WRITE_BUFFER

    return AH_TRUE;

} /* ar5416SetRifsDelay () */

/* Set the current RIFS Rx setting */
HAL_BOOL
ar5416Set11nRxRifs(struct ath_hal *ah, HAL_BOOL enable)
{
    HAL_BOOL is_chan_2g = IS_CHAN_2GHZ(AH_PRIVATE(ah)->ah_curchan);
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t tmp_val;

    /* Non-Owl 11n chips */
    if ((ath_hal_getcapability(ah, HAL_CAP_RIFS_RX, 0, AH_NULL) == HAL_OK)) {
        if (ar5416GetCapability(ah, HAL_CAP_BB_RIFS_HANG, 0, AH_NULL) == HAL_OK)
            {return ar5416SetRifsDelay(ah, enable);}
        return AH_FALSE;
    }

    /* Owl based chips */
    if (enable) {
        if (ahp->ah_rifs_enabled == AH_FALSE) {
            ahp->ah_rifs_reg[0] = OS_REG_READ(ah, AR_PHY_SETTLING);
            ahp->ah_rifs_reg[1] = OS_REG_READ(ah, AR_PHY_DESIRED_SZ);
            ahp->ah_rifs_reg[2] = OS_REG_READ(ah, AR_PHY_FIND_SIG);
            ahp->ah_rifs_reg[3] = OS_REG_READ(ah, AR_PHY_AGC_CONTROL);
            ahp->ah_rifs_reg[4] = OS_REG_READ(ah, AR_PHY_SEARCH_START_DELAY);
            ahp->ah_rifs_reg[5] = OS_REG_READ(ah, AR_PHY_TIMING5);
            ahp->ah_rifs_reg[6] = OS_REG_READ(ah, AR_PHY_FRAME_CTL);
            ahp->ah_rifs_reg[7] = OS_REG_READ(ah, AR_PHY_MULTICHAIN_GAIN_CTL);
            ahp->ah_rifs_reg[8] = OS_REG_READ(ah, AR_PHY_EXT_CCA);
            ahp->ah_rifs_reg[9] = OS_REG_READ(ah, AR_PHY_DAG_CTRLCCK);
            ahp->ah_rifs_reg[10] = OS_REG_READ(ah, 0xa388);
        }

        /*
         * enable rb
         */
        // Total desired set to -44dBFS
        OS_REG_WRITE(ah, AR_PHY_DESIRED_SZ, ((ahp->ah_rifs_reg[1] & ~(0xff00000))
                                   | (0xde << 20)));
        // First step set to 0
        OS_REG_WRITE(ah, AR_PHY_FIND_SIG, (ahp->ah_rifs_reg[2] & ~(0x3f000)));
        // min num gc
        OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL, ((ahp->ah_rifs_reg[3] & ~(0x38)) | 0x18));
        // Enable cyclic power threshold for OFDM weak signal detection
        OS_REG_WRITE(ah, AR_PHY_TIMING5, ((ahp->ah_rifs_reg[5] & ~(0xff)) | 0x1d));
        // Avoid chip hang, disable baseband service error check
        OS_REG_WRITE(ah, AR_PHY_FRAME_CTL, (ahp->ah_rifs_reg[6] & ~(1 << 28)));
        // Enable RIFS detection
        OS_REG_WRITE(ah, 0xa388, (ahp->ah_rifs_reg[10] | (1 << 26)));
        // Quickdrop low: improve RSSI imbalance support
        OS_REG_WRITE(ah, AR_PHY_MULTICHAIN_GAIN_CTL, (ahp->ah_rifs_reg[7] & ~(0xff)));
        // Extension channel cyclic power threshold
        OS_REG_WRITE(ah, AR_PHY_EXT_CCA, ((ahp->ah_rifs_reg[8] & ~(0xfe00)) | (0xe << 9)));

        if (is_chan_2g) {
            // AGC settling
            tmp_val = ((ahp->ah_rifs_reg[0] & ~(0x7f)) | 0x20);
            tmp_val = ((tmp_val & ~(0x3f80)) | (0x2d << 7));
            OS_REG_WRITE(ah, AR_PHY_SETTLING, tmp_val);
            // Enable barker
            OS_REG_WRITE(ah, AR_PHY_DAG_CTRLCCK,
            ((ahp->ah_rifs_reg[9] & ~(0x1fe00)) | (0x8 << 10) | (0x1 << 9)));
            // Search start delay
			if(IS_CHAN_HT40(AH_PRIVATE(ah)->ah_curchan)) {
                OS_REG_WRITE(ah, AR_PHY_SEARCH_START_DELAY, 0x84);
			}
			else {
                OS_REG_WRITE(ah, AR_PHY_SEARCH_START_DELAY, 0x42);
			}
        } else { 
            // AGC settling
            tmp_val = ((ahp->ah_rifs_reg[0] & ~(0x7f)) | 0x1e);
            tmp_val = ((tmp_val & ~(0x3f80)) | (0x2c << 7));
            OS_REG_WRITE(ah, AR_PHY_SETTLING, tmp_val);
            // Enable barker
            OS_REG_WRITE(ah, AR_PHY_DAG_CTRLCCK, ahp->ah_rifs_reg[9] & ~(0x1fe00));
            // Search start delay
			if(IS_CHAN_HT40(AH_PRIVATE(ah)->ah_curchan)) {
                OS_REG_WRITE(ah, AR_PHY_SEARCH_START_DELAY, 0x78);
			}
			else {
                OS_REG_WRITE(ah, AR_PHY_SEARCH_START_DELAY, 0x3c);
			}
        }

        // Flush writes
        (void) OS_REG_READ(ah, AR_PHY_MULTICHAIN_GAIN_CTL);

        ahp->ah_rifs_enabled = AH_TRUE;

    } else {
        if (ahp->ah_rifs_enabled == AH_FALSE)
            return AH_TRUE;

        /*
         * disable rb
         */
        OS_REG_WRITE(ah, AR_PHY_SETTLING, ahp->ah_rifs_reg[0]);
        OS_REG_WRITE(ah, AR_PHY_DESIRED_SZ, ahp->ah_rifs_reg[1]);
        OS_REG_WRITE(ah, AR_PHY_FIND_SIG, ahp->ah_rifs_reg[2]);
        OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL, ahp->ah_rifs_reg[3]);
        OS_REG_WRITE(ah, AR_PHY_SEARCH_START_DELAY, ahp->ah_rifs_reg[4]);
        OS_REG_WRITE(ah, AR_PHY_TIMING5, ahp->ah_rifs_reg[5]);
        OS_REG_WRITE(ah, AR_PHY_FRAME_CTL, ahp->ah_rifs_reg[6]);
        OS_REG_WRITE(ah, AR_PHY_MULTICHAIN_GAIN_CTL, ahp->ah_rifs_reg[7]);
        OS_REG_WRITE(ah, AR_PHY_EXT_CCA, ahp->ah_rifs_reg[8]);
        OS_REG_WRITE(ah, AR_PHY_DAG_CTRLCCK, ahp->ah_rifs_reg[9]);
        OS_REG_WRITE(ah, 0xa388, ahp->ah_rifs_reg[10]);

        // Flush writes
        (void) OS_REG_READ(ah, AR_PHY_MULTICHAIN_GAIN_CTL);

        OS_MEMZERO(ahp->ah_rifs_reg, sizeof(ahp->ah_rifs_reg));

        ahp->ah_rifs_enabled = AH_FALSE;
    }

    return AH_TRUE;

} /* ar5416Set11nRxRifs () */

static hal_mac_hangs_t
ar5416CompareDbgHang(struct ath_hal *ah, mac_dbg_regs_t mac_dbg,
                     hal_mac_hang_check_t hang_check, hal_mac_hangs_t hangs)
{
    int i = 0;
    hal_mac_hangs_t found_hangs = 0;

    if (hangs & dcu_chain_state) { 
        for (i = 0; i < 6; i++) {
            if (((mac_dbg.dma_dbg_4 >> (5*i)) & 0x1f) ==
                  hang_check.dcu_chain_state)
                found_hangs |= dcu_chain_state;
        }
        for (i = 0; i < 4; i++) {
            if (((mac_dbg.dma_dbg_5 >> (5*i)) & 0x1f) ==
                  hang_check.dcu_chain_state)
                found_hangs |= dcu_chain_state;
        }
    }

    if (hangs & dcu_complete_state) { 
        if ((mac_dbg.dma_dbg_6 & 0x3) == hang_check.dcu_complete_state)
            found_hangs |= dcu_complete_state;
    }

    if (hangs & qcu_stitch_state) { 
        if (((mac_dbg.dma_dbg_3 >> 18) & 0xf) == hang_check.qcu_stitch_state)
            found_hangs |= qcu_stitch_state;
    }

    if (hangs & qcu_fetch_state) { 
        if (((mac_dbg.dma_dbg_3 >> 22) & 0xf) == hang_check.qcu_fetch_state)
            found_hangs |= qcu_fetch_state;
    }

    if (hangs & qcu_complete_state) { 
        if (((mac_dbg.dma_dbg_3 >> 26) & 0x7) == hang_check.qcu_complete_state)
            found_hangs |= qcu_complete_state;
    }

    return found_hangs;

} /* end - ar5416CompareDbgHang */

#define NUM_STATUS_READS 50
HAL_BOOL
ar5416DetectMacHang(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    mac_dbg_regs_t mac_dbg;
    hal_mac_hang_check_t hang_sig1_val = {0x6, 0x1, 0, 0, 0, 0, 0, 0},
                         hang_sig2_val = {0, 0, 0, 0, 0, 0x9, 0x8, 0x4};
    hal_mac_hangs_t      hang_sig1 = (dcu_chain_state | dcu_complete_state),
                         hang_sig2 = (qcu_stitch_state |
                                      qcu_fetch_state |
                                      qcu_complete_state);
    int i = 0;

    if (!(ahp->ah_hang_wars & HAL_MAC_HANG_WAR)) {
        return AH_FALSE;
    }

    OS_MEMZERO(&mac_dbg, sizeof(mac_dbg));

    mac_dbg.dma_dbg_3 = OS_REG_READ(ah, AR_DMADBG_3);
    mac_dbg.dma_dbg_4 = OS_REG_READ(ah, AR_DMADBG_4);
    mac_dbg.dma_dbg_5 = OS_REG_READ(ah, AR_DMADBG_5);
    mac_dbg.dma_dbg_6 = OS_REG_READ(ah, AR_DMADBG_6);

    for (i=1; i <= NUM_STATUS_READS; i++) {
        if (mac_dbg.dma_dbg_3 != OS_REG_READ(ah, AR_DMADBG_3) ||
            mac_dbg.dma_dbg_4 != OS_REG_READ(ah, AR_DMADBG_4) ||
            mac_dbg.dma_dbg_5 != OS_REG_READ(ah, AR_DMADBG_5) ||
            mac_dbg.dma_dbg_6 != OS_REG_READ(ah, AR_DMADBG_6))
            {
             return AH_FALSE;
            }
    }

    if (hang_sig1==ar5416CompareDbgHang(ah, mac_dbg, hang_sig1_val, hang_sig1)){
        HDPRINTF(ah, HAL_DBG_DFS, "%s sig5count=%d sig6count=%d ", __func__,
                 ahp->ah_hang[MAC_HANG_SIG1], ahp->ah_hang[MAC_HANG_SIG2]);
        ahp->ah_hang[MAC_HANG_SIG1]++;
        return AH_TRUE;
    }
    if (hang_sig2==ar5416CompareDbgHang(ah, mac_dbg, hang_sig2_val, hang_sig2)){
        HDPRINTF(ah, HAL_DBG_DFS, "%s sig5count=%d sig6count=%d ", __func__,
                 ahp->ah_hang[MAC_HANG_SIG1], ahp->ah_hang[MAC_HANG_SIG2]);
        ahp->ah_hang[MAC_HANG_SIG2]++;
        return AH_TRUE;
    }

    HDPRINTF(ah, HAL_DBG_DFS, "%s Found an unknown MAC hang signature "
                              "DMA Debug3=0x%x DMA Debug4=0x%x "
                              "DMA Debug5=0x%x DMA Debug6=0x%x\n", __func__,
             mac_dbg.dma_dbg_3, mac_dbg.dma_dbg_4, mac_dbg.dma_dbg_5,
             mac_dbg.dma_dbg_6);

    return AH_FALSE;

} /* end - ar5416DetectMacHang */


/* Determine if the baseband is hung by reading the Observation Bus Register */
HAL_BOOL
ar5416DetectBbHang(struct ath_hal *ah)
{
#define N(a) (sizeof(a)/sizeof(a[0]))
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t hang_sig = 0;
    int i=0;
    /* Check the PCU Observation Bus 1 register (0x806c) NUM_STATUS_READS times
     *
     * 4 known BB hang signatures -
     * [1] bits 8,9,11 are 0. State machine state (bits 25-31) is 0x1E
     * [2] bits 8,9 are 1, bit 11 is 0. State machine state (bits 25-31) is 0x52
     * [3] bits 8,9 are 1, bit 11 is 0. State machine state (bits 25-31) is 0x18
     * [4] bit 10 is 1, bit 11 is 0. WEP state (bits 12-17) is 0x2,
     *     Rx State (bits 20-24) is 0x7.
     */
    hal_hw_hang_check_t hang_list [] =
    {
     /* Offset        Reg Value   Reg Mask    Hang Offset */
       {AR_OBS_BUS_1, 0x1E000000, 0x7E000B00, BB_HANG_SIG1},
       {AR_OBS_BUS_1, 0x52000B00, 0x7E000B00, BB_HANG_SIG2},
       {AR_OBS_BUS_1, 0x18000B00, 0x7E000B00, BB_HANG_SIG3},
       {AR_OBS_BUS_1, 0x00702400, 0x7E7FFFEF, BB_HANG_SIG4}
    };

    if (!(ahp->ah_hang_wars & (HAL_RIFS_BB_HANG_WAR |
                               HAL_DFS_BB_HANG_WAR |
                               HAL_RX_STUCK_LOW_BB_HANG_WAR))) {
         return AH_FALSE;
    }

    hang_sig = OS_REG_READ(ah, AR_OBS_BUS_1);
    for (i=1; i <= NUM_STATUS_READS; i++) {
        if (hang_sig != OS_REG_READ(ah, AR_OBS_BUS_1)) {
             return AH_FALSE;
        }
    }

    for (i=0; i < N(hang_list); i++) {
        if ((hang_sig & hang_list[i].hang_mask) == hang_list[i].hang_val) {
            ahp->ah_hang[hang_list[i].hang_offset]++;
            HDPRINTF(ah, HAL_DBG_DFS, "%s sig1count=%d sig2count=%d "
                     "sig3count=%d sig4count=%d\n", __func__,
                     ahp->ah_hang[BB_HANG_SIG1], ahp->ah_hang[BB_HANG_SIG2],
                     ahp->ah_hang[BB_HANG_SIG3], ahp->ah_hang[BB_HANG_SIG4]);
            return AH_TRUE;
        }
    }

    HDPRINTF(ah, HAL_DBG_DFS, "%s Found an unknown BB hang signature! "
                              "<0x806c>=0x%x\n", __func__, hang_sig);

    return AH_FALSE;

#undef N
} /* end - ar5416DetectBbHang () */

#undef NUM_STATUS_READS

HAL_STATUS
ar5416SelectAntConfig(struct ath_hal *ah, u_int32_t cfg)
{
    struct ath_hal_5416     *ahp = AH5416(ah);
    HAL_CHANNEL_INTERNAL    *chan = AH_PRIVATE(ah)->ah_curchan;
    const HAL_CAPABILITIES  *pCap = &AH_PRIVATE(ah)->ah_caps;
    u_int32_t               ant_config;
    u_int32_t               halNumAntConfig;

    halNumAntConfig = IS_CHAN_2GHZ(chan)? pCap->hal_num_ant_cfg_2ghz : pCap->hal_num_ant_cfg_5ghz;

    if (cfg < halNumAntConfig) {
        if (HAL_OK == ar5416EepromGetAntCfg(ahp, chan, cfg, &ant_config)) {
            OS_REG_WRITE(ah, AR_PHY_SWITCH_COM, ant_config);
            return HAL_OK;
        }
    }

    return HAL_EINVAL;
}

#ifdef ATH_BT_COEX
void
ar5416SetBTCoexInfo(struct ath_hal *ah, HAL_BT_COEX_INFO *btinfo)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    ahp->ah_btModule = btinfo->bt_module;
    ahp->ah_btCoexConfigType = btinfo->bt_coex_config;
    ahp->ah_btActiveGpioSelect = btinfo->bt_gpio_bt_active;
    ahp->ah_btPriorityGpioSelect = btinfo->bt_gpio_bt_priority;
    ahp->ah_wlanActiveGpioSelect = btinfo->bt_gpio_wlan_active;
    ahp->ah_btActivePolarity = btinfo->bt_active_polarity;
    ahp->ah_btCoexSingleAnt = btinfo->bt_single_ant;
    ahp->ah_btWlanIsolation = btinfo->bt_isolation;
}

void
ar5416BTCoexConfig(struct ath_hal *ah, HAL_BT_COEX_CONFIG *btconf)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    HAL_BOOL rxClearPolarity = btconf->bt_rxclear_polarity;

    /*
     * For Kiwi and Osprey, the polarity of rx_clear is active high.
     * The bt_rxclear_polarity flag from ath_dev needs to be inverted.
     */
    if (AR_SREV_KIWI(ah)) {
        rxClearPolarity = !btconf->bt_rxclear_polarity;
    }

    ahp->ah_btCoexMode = (ahp->ah_btCoexMode & AR_BT_QCU_THRESH) |
        SM(btconf->bt_time_extend, AR_BT_TIME_EXTEND) |
        SM(btconf->bt_txstate_extend, AR_BT_TXSTATE_EXTEND) |
        SM(btconf->bt_txframe_extend, AR_BT_TX_FRAME_EXTEND) |
        SM(btconf->bt_mode, AR_BT_MODE) |
        SM(btconf->bt_quiet_collision, AR_BT_QUIET) |
        SM(rxClearPolarity, AR_BT_RX_CLEAR_POLARITY) |
        SM(btconf->bt_priority_time, AR_BT_PRIORITY_TIME) |
        SM(btconf->bt_first_slot_time, AR_BT_FIRST_SLOT_TIME);

    ahp->ah_btCoexMode2 |= SM(btconf->bt_hold_rxclear, AR_BT_HOLD_RX_CLEAR);

    if (ahp->ah_btCoexSingleAnt == AH_FALSE) {
        /* Enable ACK to go out even though BT has higher priority. */
        ahp->ah_btCoexMode2 |= AR_BT_DISABLE_BT_ANT;
    }
}

void
ar5416BTCoexSetQcuThresh(struct ath_hal *ah, int qnum)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ahp->ah_btCoexMode |= SM(qnum, AR_BT_QCU_THRESH);
}

void
ar5416BTCoexSetWeights(struct ath_hal *ah, u_int32_t stompType)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    if (AR_SREV_KIWI_10_OR_LATER(ah)) {
        /* TODO: TX RX seperate is not enabled. */
        switch(stompType) {
        case HAL_BT_COEX_STOMP_ALL:
            ahp->ah_btCoexBTWeight = AR5416_BT_WGHT;
            ahp->ah_btCoexWLANWeight = AR5416_STOMP_ALL_WLAN_WGHT;
            break;
        case HAL_BT_COEX_STOMP_LOW:
            ahp->ah_btCoexBTWeight = AR5416_BT_WGHT;
            ahp->ah_btCoexWLANWeight = AR5416_STOMP_LOW_WLAN_WGHT;
            break;
        case HAL_BT_COEX_STOMP_ALL_FORCE:
            ahp->ah_btCoexBTWeight = AR5416_BT_WGHT;
            ahp->ah_btCoexWLANWeight = AR5416_STOMP_ALL_FORCE_WLAN_WGHT;
            break;
        case HAL_BT_COEX_STOMP_LOW_FORCE:
            ahp->ah_btCoexBTWeight = AR5416_BT_WGHT;
            ahp->ah_btCoexWLANWeight = AR5416_STOMP_LOW_FORCE_WLAN_WGHT;
            break;
        case HAL_BT_COEX_STOMP_NONE:
        case HAL_BT_COEX_NO_STOMP:
            ahp->ah_btCoexBTWeight = AR5416_BT_WGHT;
            ahp->ah_btCoexWLANWeight = AR5416_STOMP_NONE_WLAN_WGHT;
            break;
        default:
            /* There is a forceWeight from registry */
            ahp->ah_btCoexBTWeight = stompType & 0xffff;
            ahp->ah_btCoexWLANWeight = stompType >> 16;
            break;
        }
    }
    else {
        switch(stompType) {
        case HAL_BT_COEX_STOMP_ALL:
            ahp->ah_btCoexBTWeight = AR5416_BT_WGHT;
            ahp->ah_btCoexWLANWeight = AR5416_STOMP_ALL_WLAN_WGHT;
            break;
        case HAL_BT_COEX_STOMP_LOW:
            ahp->ah_btCoexBTWeight = AR5416_BT_WGHT;
            ahp->ah_btCoexWLANWeight = AR5416_STOMP_LOW_WLAN_WGHT;
            break;
        case HAL_BT_COEX_STOMP_ALL_FORCE:
            ahp->ah_btCoexBTWeight = AR5416_BT_WGHT;
            ahp->ah_btCoexWLANWeight = AR5416_STOMP_ALL_FORCE_WLAN_WGHT;
            break;
        case HAL_BT_COEX_STOMP_LOW_FORCE:
            ahp->ah_btCoexBTWeight = AR5416_BT_WGHT;
            ahp->ah_btCoexWLANWeight = AR5416_STOMP_LOW_FORCE_WLAN_WGHT;
            break;
        case HAL_BT_COEX_STOMP_NONE:
        case HAL_BT_COEX_NO_STOMP:
            ahp->ah_btCoexBTWeight = AR5416_BT_WGHT;
            ahp->ah_btCoexWLANWeight = AR5416_STOMP_NONE_WLAN_WGHT;
            break;
        default:
            /* There is a forceWeight from registry */
            ahp->ah_btCoexBTWeight = stompType & 0xffff;
            ahp->ah_btCoexWLANWeight = stompType >> 16;
            break;
        }
    }
}

void
ar5416BTCoexSetupBmissThresh(struct ath_hal *ah, u_int32_t thresh)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    ahp->ah_btCoexMode2 |= SM(thresh, AR_BT_BCN_MISS_THRESH);
}

static void
ar5416BTCoexAntennaDiversity(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t regVal;
    u_int8_t ant_div_control1, ant_div_control2;

    if ((ahp->ah_btCoexFlag & HAL_BT_COEX_FLAG_ANT_DIV_ALLOW) || 
        (ahp->ah_diversityControl != HAL_ANT_VARIABLE))
    {
        if ((ahp->ah_btCoexFlag & HAL_BT_COEX_FLAG_ANT_DIV_ENABLE) &&
             (ahp->ah_diversityControl == HAL_ANT_VARIABLE))
        {
            /* Enable antenna diversity */
            ant_div_control1 = HAL_BT_COEX_ANTDIV_CONTROL1_ENABLE;
            ant_div_control2 = HAL_BT_COEX_ANTDIV_CONTROL2_ENABLE;

            /* Don't disable BT ant to allow BB to control SWCOM */
            ahp->ah_btCoexMode2 &= (~(AR_BT_DISABLE_BT_ANT));
            OS_REG_WRITE(ah, AR_BT_COEX_MODE2, ahp->ah_btCoexMode2);

            /* Program the correct SWCOM table */
            OS_REG_WRITE(ah, AR_PHY_SWITCH_COM, HAL_BT_COEX_ANT_DIV_SWITCH_COM);
            OS_REG_RMW(ah, AR_PHY_SWITCH_CHAIN_0, 0, 0xf0000000);
        }
        else if (ahp->ah_diversityControl == HAL_ANT_FIXED_B) {
            /* Disable antenna diversity. Use antenna B(LNA2) only. */
            ant_div_control1 = HAL_BT_COEX_ANTDIV_CONTROL1_FIXED_B;
            ant_div_control2 = HAL_BT_COEX_ANTDIV_CONTROL2_FIXED_B;

            /* Disable BT ant to allow concurrent BT and WLAN receive */
            ahp->ah_btCoexMode2 |= AR_BT_DISABLE_BT_ANT;
            OS_REG_WRITE(ah, AR_BT_COEX_MODE2, ahp->ah_btCoexMode2);

            /* Program SWCOM talbe to make sure RF switch always parks at WLAN side */
            OS_REG_WRITE(ah, AR_PHY_SWITCH_COM, HAL_BT_COEX_ANT_DIV_SWITCH_COM);
            OS_REG_RMW(ah, AR_PHY_SWITCH_CHAIN_0, 0x60000000, 0xf0000000);
        }
        else
        {
            /* Disable antenna diversity. Use antenna A(LNA1) only */
            ant_div_control1 = HAL_BT_COEX_ANTDIV_CONTROL1_FIXED_A;
            ant_div_control2 = HAL_BT_COEX_ANTDIV_CONTROL2_FIXED_A;

            /* Disable BT ant to allow concurrent BT and WLAN receive */
            ahp->ah_btCoexMode2 |= AR_BT_DISABLE_BT_ANT;
            OS_REG_WRITE(ah, AR_BT_COEX_MODE2, ahp->ah_btCoexMode2);

            /* Program SWCOM talbe to make sure RF switch always parks at BT side */
            OS_REG_WRITE(ah, AR_PHY_SWITCH_COM, 0);
            OS_REG_RMW(ah, AR_PHY_SWITCH_CHAIN_0, 0, 0xf0000000);
        }

        regVal = OS_REG_READ(ah, AR_PHY_MULTICHAIN_GAIN_CTL);
        regVal &= (~(AR_PHY_KITE_ANT_DIV_CTL_ALL));
        /* Clear ant_fast_div_bias [14:9] since for Janus the main LNA is always LNA1. */
        regVal &= (~(AR_PHY_KITE_ANT_DIV_FAST_BIAS));

        regVal |= SM(ant_div_control1, AR_PHY_KITE_ANT_DIV_CTL);
        regVal |= SM(ant_div_control2, AR_PHY_KITE_ANT_DIV_ALT_LNACONF);
        regVal |= SM((ant_div_control2 >> 2), AR_PHY_KITE_ANT_DIV_MAIN_LNACONF);
        regVal |= SM((ant_div_control1 >> 1), AR_PHY_KITE_ANT_DIV_ALT_GAINTB);
        regVal |= SM((ant_div_control1 >> 2), AR_PHY_KITE_ANT_DIV_MAIN_GAINTB);
        OS_REG_WRITE(ah, AR_PHY_MULTICHAIN_GAIN_CTL, regVal);

        regVal = OS_REG_READ(ah, AR_PHY_CCK_DETECT);
        regVal &= (~AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV);
        regVal |= SM((ant_div_control1 >> 3), AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV);
        OS_REG_WRITE(ah, AR_PHY_CCK_DETECT, regVal);
    }
}

void
ar5416BTCoexSetParameter(struct ath_hal *ah, u_int32_t type, u_int32_t value)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    switch (type) {
        case HAL_BT_COEX_SET_ACK_PWR:
            if (value) {
                ahp->ah_btCoexFlag |= HAL_BT_COEX_FLAG_LOW_ACK_PWR;
                OS_REG_WRITE(ah, AR_TPC, HAL_BT_COEX_LOW_ACK_POWER);
            }
            else {
                ahp->ah_btCoexFlag &= ~HAL_BT_COEX_FLAG_LOW_ACK_PWR;
                OS_REG_WRITE(ah, AR_TPC, HAL_BT_COEX_HIGH_ACK_POWER);
            }
            break;
        case HAL_BT_COEX_ANTENNA_DIVERSITY:
            if (AR_SREV_KITE(ah)) {
                ahp->ah_btCoexFlag |= HAL_BT_COEX_FLAG_ANT_DIV_ALLOW;
                if (value) {
                    ahp->ah_btCoexFlag |= HAL_BT_COEX_FLAG_ANT_DIV_ENABLE;
                }
             else {
                 ahp->ah_btCoexFlag &= ~HAL_BT_COEX_FLAG_ANT_DIV_ENABLE;
                }
                ar5416BTCoexAntennaDiversity(ah);
            }
            break;
        case HAL_BT_COEX_LOWER_TX_PWR:
            if (value) {
                if ((ahp->ah_btCoexFlag & HAL_BT_COEX_FLAG_LOWER_TX_PWR) == 0) {
                    ahp->ah_btCoexFlag |= HAL_BT_COEX_FLAG_LOWER_TX_PWR;
		    AH_PRIVATE(ah)->ah_config.ath_hal_desc_tpc = 1;
                    ar5416SetTxPowerLimit(ah, AH_PRIVATE(ah)->ah_power_limit, AH_PRIVATE(ah)->ah_extra_txpow, 0);
                }
            }
            else {
                if (ahp->ah_btCoexFlag & HAL_BT_COEX_FLAG_LOWER_TX_PWR) {
                    ahp->ah_btCoexFlag &= ~HAL_BT_COEX_FLAG_LOWER_TX_PWR;
		    AH_PRIVATE(ah)->ah_config.ath_hal_desc_tpc = 0;
                    ar5416SetTxPowerLimit(ah, AH_PRIVATE(ah)->ah_power_limit, AH_PRIVATE(ah)->ah_extra_txpow, 0);
                }
            }
            break;

        default:
            break;
    }
}

void
ar5416BTCoexDisable(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    /* Always drive rx_clear_external output as 0 */
    ath_hal_gpioSet(ah, ahp->ah_wlanActiveGpioSelect, 0);
    ath_hal_gpio_cfg_output(ah, ahp->ah_wlanActiveGpioSelect, HAL_GPIO_OUTPUT_MUX_AS_OUTPUT);

    if (AR_SREV_K2(ah)) {
        /* Set wlanActiveGpio to input when disabling BT-COEX to reduce power consumption */
        ath_hal_gpio_cfg_input(ah, ahp->ah_wlanActiveGpioSelect);
    }

    if (ahp->ah_btCoexSingleAnt == AH_TRUE) {
        OS_REG_RMW_FIELD(ah, AR_QUIET1, AR_QUIET1_QUIET_ACK_CTS_ENABLE, 1); 
        OS_REG_RMW_FIELD(ah, AR_PCU_MISC, AR_PCU_BT_ANT_PREVENT_RX, 0);
    }

    OS_REG_WRITE(ah, AR_BT_COEX_MODE, AR_BT_QUIET | AR_BT_MODE);
    OS_REG_WRITE(ah, AR_BT_COEX_WEIGHT, 0);
    if (AR_SREV_KIWI_10_OR_LATER(ah)) {
        OS_REG_WRITE(ah, AR_BT_COEX_WEIGHT2, 0);
    }
    OS_REG_WRITE(ah, AR_BT_COEX_MODE2, 0);

    ahp->ah_btCoexEnabled = AH_FALSE;
}

int
ar5416BTCoexEnable(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);
    u_int32_t  val;
   
    /* Program coex mode and weight registers to actually enable coex */
    OS_REG_WRITE(ah, AR_BT_COEX_MODE, ahp->ah_btCoexMode);
    OS_REG_WRITE(ah, AR_BT_COEX_WEIGHT, 
                 SM(ahp->ah_btCoexWLANWeight & 0xFFFF, AR_BT_WL_WGHT) |
                 SM(ahp->ah_btCoexBTWeight & 0xFFFF, AR_BT_BT_WGHT));
    if (AR_SREV_KIWI_10_OR_LATER(ah)) {
        OS_REG_WRITE(ah, AR_BT_COEX_WEIGHT2, 
                     SM(ahp->ah_btCoexWLANWeight >> 16, AR_BT_WL_WGHT));
    }
    OS_REG_WRITE(ah, AR_BT_COEX_MODE2, ahp->ah_btCoexMode2);
    
    /* Added Select GPIO5~8 instaed SPI */
    if (AR_SREV_K2(ah)) {
        val = OS_REG_READ(ah, AR9271_CLOCK_CONTROL);
        val &= 0xFFFFFEFF;
        OS_REG_WRITE(ah, AR9271_CLOCK_CONTROL, val);
    }

    if (ahp->ah_btCoexFlag & HAL_BT_COEX_FLAG_LOW_ACK_PWR) {
        OS_REG_WRITE(ah, AR_TPC, HAL_BT_COEX_LOW_ACK_POWER);
    }
    else {
        OS_REG_WRITE(ah, AR_TPC, HAL_BT_COEX_HIGH_ACK_POWER);
    }

    if (ahp->ah_btCoexSingleAnt == AH_TRUE) {
        OS_REG_RMW_FIELD(ah, AR_QUIET1, AR_QUIET1_QUIET_ACK_CTS_ENABLE, 1); 
        OS_REG_RMW_FIELD(ah, AR_PCU_MISC, AR_PCU_BT_ANT_PREVENT_RX, 1);
    }
    else {
        OS_REG_RMW_FIELD(ah, AR_QUIET1, AR_QUIET1_QUIET_ACK_CTS_ENABLE, 1); 
        OS_REG_RMW_FIELD(ah, AR_PCU_MISC, AR_PCU_BT_ANT_PREVENT_RX, 0);
    }

    if (ahp->ah_btCoexConfigType == HAL_BT_COEX_CFG_3WIRE) {
        /* For 3-wire, configure the desired GPIO port for rx_clear */
        ath_hal_gpio_cfg_output(ah, ahp->ah_wlanActiveGpioSelect, HAL_GPIO_OUTPUT_MUX_AS_WLAN_ACTIVE);
    }
    else {
        /* For 2-wire, configure the desired GPIO port for TX_FRAME output */
        ath_hal_gpio_cfg_output(ah, ahp->ah_wlanActiveGpioSelect, HAL_GPIO_OUTPUT_MUX_AS_TX_FRAME);
    }

    /* enable a weak pull down on BT_ACTIVE. When BT device is disabled, BT_ACTIVE might be floating. */
    OS_REG_RMW(ah, AR_GPIO_PDPU, (0x2 << (ahp->ah_btActiveGpioSelect * 2)), (0x3 << (ahp->ah_btActiveGpioSelect * 2)));

    ahp->ah_btCoexEnabled = AH_TRUE;

    return 0;
}

u_int32_t
ar5416GetBTActiveGpio(struct ath_hal *ah, u_int32_t reg)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	u_int32_t regval;
	ath_hal_gpio_cfg_input(ah, ahp->ah_btActiveGpioSelect);
	ath_hal_gpio_cfg_output(ah, ahp->ah_wlanActiveGpioSelect, HAL_GPIO_OUTPUT_MUX_AS_OUTPUT);
    regval = OS_REG_READ(ah, AR_GPIO_PDPU) & (0x1<<ahp->ah_btActiveGpioSelect);

	return regval;
}

u_int32_t
ar5416GetWlanActiveGpio(struct ath_hal *ah, u_int32_t reg,u_int32_t bOn)
{
	struct ath_hal_5416 *ahp = AH5416(ah);
	u_int32_t regval;
	ath_hal_gpio_cfg_input(ah, ahp->ah_btActiveGpioSelect);
	ath_hal_gpio_cfg_output(ah, ahp->ah_wlanActiveGpioSelect, HAL_GPIO_OUTPUT_MUX_AS_OUTPUT);
    OS_REG_WRITE(ah, AR_GPIO_PDPU, bOn<<ahp->ah_wlanActiveGpioSelect);
    regval = OS_REG_READ(ah, AR_GPIO_PDPU) & (0x1<<ahp->ah_wlanActiveGpioSelect);
	return regval;
}

void
ar5416InitBTCoex(struct ath_hal *ah)
{
    struct ath_hal_5416 *ahp = AH5416(ah);

    if (ahp->ah_btCoexConfigType == HAL_BT_COEX_CFG_3WIRE) {
        OS_REG_SET_BIT(ah, AR_GPIO_INPUT_EN_VAL,
                   (AR_GPIO_INPUT_EN_VAL_BT_PRIORITY_BB |
                    AR_GPIO_INPUT_EN_VAL_BT_ACTIVE_BB));

        /* Set input mux for bt_prority_async and bt_active_async to GPIO pins */
        OS_REG_RMW_FIELD(ah, AR_GPIO_INPUT_MUX1, AR_GPIO_INPUT_MUX1_BT_ACTIVE,
                     ahp->ah_btActiveGpioSelect);
        OS_REG_RMW_FIELD(ah, AR_GPIO_INPUT_MUX1, AR_GPIO_INPUT_MUX1_BT_PRIORITY,
                         ahp->ah_btPriorityGpioSelect);

        /* Configure the desired GPIO ports for input */
        ath_hal_gpio_cfg_input(ah, ahp->ah_btActiveGpioSelect);
        ath_hal_gpio_cfg_input(ah, ahp->ah_btPriorityGpioSelect); 

        if (AR_SREV_KITE(ah)) {
            ar5416BTCoexAntennaDiversity(ah);
        }
        
        if (ahp->ah_btCoexEnabled) {
            ar5416BTCoexEnable(ah);
        }else {
            ar5416BTCoexDisable(ah);
        }
    }
    else if (ahp->ah_btCoexConfigType != HAL_BT_COEX_CFG_NONE) {
        /* 2-wire */
        if (ahp->ah_btCoexEnabled) {
            /* Connect bt_active_async to baseband */
            OS_REG_CLR_BIT(ah, AR_GPIO_INPUT_EN_VAL, 
                            (AR_GPIO_INPUT_EN_VAL_BT_PRIORITY_DEF | AR_GPIO_INPUT_EN_VAL_BT_FREQUENCY_DEF));
            OS_REG_SET_BIT(ah, AR_GPIO_INPUT_EN_VAL,
                            AR_GPIO_INPUT_EN_VAL_BT_ACTIVE_BB);

            /* Set input mux for bt_prority_async and bt_active_async to GPIO pins */
            OS_REG_RMW_FIELD(ah, AR_GPIO_INPUT_MUX1, AR_GPIO_INPUT_MUX1_BT_ACTIVE,
                             ahp->ah_btActiveGpioSelect);

            /* Configure the desired GPIO ports for input */
            ath_hal_gpio_cfg_input(ah, ahp->ah_btActiveGpioSelect);

            /* Enable coexistence on initialization */
            ar5416BTCoexEnable(ah);
        }
    }
}

#endif /* ATH_BT_COEX */

#if ATH_ANT_DIV_COMB
void
ar5416AntDivCombGetConfig(struct ath_hal *ah, HAL_ANT_COMB_CONFIG* divCombConf) {
    u_int32_t regVal = OS_REG_READ(ah, AR_PHY_MULTICHAIN_GAIN_CTL);
    divCombConf->main_lna_conf = ((regVal & AR_PHY_KITE_ANT_DIV_MAIN_LNACONF) >> 
                                   AR_PHY_KITE_ANT_DIV_MAIN_LNACONF_S);
    divCombConf->alt_lna_conf = ((regVal & AR_PHY_KITE_ANT_DIV_ALT_LNACONF) >> 
                                   AR_PHY_KITE_ANT_DIV_ALT_LNACONF_S);
    divCombConf->fast_div_bias = ((regVal & AR_PHY_KITE_ANT_DIV_FAST_BIAS) >> 
                                   AR_PHY_KITE_ANT_DIV_FAST_BIAS_S);
    divCombConf->antdiv_configgroup = DEFAULT_ANTDIV_CONFIG_GROUP;
    regVal = OS_REG_READ(ah, AR_PHY_SWITCH_COM);                                   
    divCombConf->switch_com_r = ((regVal & AR_PHY_KITE_SWITCH_COM_RA1L1) >>
                                   AR_PHY_KITE_SWITCH_COM_RA1L1_S);
    divCombConf->switch_com_t = ((regVal & AR_PHY_KITE_SWITCH_COM_T1) >> 
                                   AR_PHY_KITE_SWITCH_COM_T1_S);
}

void
ar5416AntDivCombSetConfig(struct ath_hal *ah, HAL_ANT_COMB_CONFIG* divCombConf) {
    u_int32_t regVal;
    
    if (divCombConf->main_lna_conf != HAL_ANT_DIV_COMB_LNA_MASK) {
        /*
         * Configure Rx related antenna setting
         */
        regVal = OS_REG_READ(ah, AR_PHY_MULTICHAIN_GAIN_CTL);
        regVal &= ~(AR_PHY_KITE_ANT_DIV_MAIN_LNACONF | AR_PHY_KITE_ANT_DIV_ALT_LNACONF | 
                    AR_PHY_KITE_ANT_DIV_FAST_BIAS);
        if (divCombConf->fast_div_disable) {
        	/* disable LNA diversity */
            regVal &= ~AR_PHY_KITE_ANT_DIV_CTL ;        
        }
        regVal |= ((divCombConf->main_lna_conf << AR_PHY_KITE_ANT_DIV_MAIN_LNACONF_S) &
                   AR_PHY_KITE_ANT_DIV_MAIN_LNACONF);
        regVal |= ((divCombConf->alt_lna_conf << AR_PHY_KITE_ANT_DIV_ALT_LNACONF_S) &
                   AR_PHY_KITE_ANT_DIV_ALT_LNACONF);
        regVal |= ((divCombConf->fast_div_bias << AR_PHY_KITE_ANT_DIV_FAST_BIAS_S) &
                   AR_PHY_KITE_ANT_DIV_FAST_BIAS);
        
        OS_REG_WRITE(ah, AR_PHY_MULTICHAIN_GAIN_CTL, regVal);
    }

    if ((divCombConf->switch_com_r != HAL_SWITCH_COM_MASK) ||
        (divCombConf->switch_com_t != HAL_SWITCH_COM_MASK)) {
        /* config rx/tx related antennas */
        regVal = OS_REG_READ(ah, AR_PHY_SWITCH_COM);

        if (divCombConf->switch_com_r != HAL_SWITCH_COM_MASK) {
            /*
             * Configure Rx antenna r1l1 and r2l1 the same configuration.
             */
            regVal &= ~(AR_PHY_KITE_SWITCH_COM_RA1L1 | AR_PHY_KITE_SWITCH_COM_RA2L1);
            regVal |= ((divCombConf->switch_com_r << AR_PHY_KITE_SWITCH_COM_RA1L1_S) & AR_PHY_KITE_SWITCH_COM_RA1L1);          
            regVal |= ((divCombConf->switch_com_r << AR_PHY_KITE_SWITCH_COM_RA2L1_S) & AR_PHY_KITE_SWITCH_COM_RA2L1);          
        }
        
        if (divCombConf->switch_com_t != HAL_SWITCH_COM_MASK) {
            /*
             * Configure Tx related antenna setting
             *
             * Configure Tx antenna t1 and t2 with the same configuration.
             * Avoid the 0x8058 default setting(switch in rx path) 
             *  and Tx description antenna setting.
             * Let Tx force switch_com_t as Tx antenna.
             */
            regVal &= ~(AR_PHY_KITE_SWITCH_COM_T1 | AR_PHY_KITE_SWITCH_COM_T2 );
            regVal |= ((divCombConf->switch_com_t << AR_PHY_KITE_SWITCH_COM_T1_S) & AR_PHY_KITE_SWITCH_COM_T1);
            regVal |= ((divCombConf->switch_com_t << AR_PHY_KITE_SWITCH_COM_T2_S) & AR_PHY_KITE_SWITCH_COM_T2);
        }
        OS_REG_WRITE(ah, AR_PHY_SWITCH_COM, regVal);
    }  
}

#endif

/*proxySTA not supported on OWL/Merlin */
HAL_STATUS ar5416SetProxySTA(struct ath_hal *ah, HAL_BOOL enable)
{
    HAL_STATUS retval = HAL_OK;
    if (enable) {
         OS_REG_WRITE(ah, AR_AZIMUTH_MODE, 
		OS_REG_READ(ah, AR_AZIMUTH_MODE) | 
		AR_AZIMUTH_KEY_SEARCH_AD1 | 
		AR_AZIMUTH_CTS_MATCH_TX_AD2 | 
		AR_AZIMUTH_BA_USES_AD1);
     } else {
         OS_REG_WRITE(ah, AR_AZIMUTH_MODE, 
		OS_REG_READ(ah, AR_AZIMUTH_MODE) & 
			~( AR_AZIMUTH_KEY_SEARCH_AD1 | 
			AR_AZIMUTH_CTS_MATCH_TX_AD2 | 
			AR_AZIMUTH_BA_USES_AD1));
     }
    /* ath_hal_printf(ah, "%s[%d] reg 0x%04x = 0x%08x\n", __func__, __LINE__, AR_AZIMUTH_MODE, OS_REG_READ(ah, AR_AZIMUTH_MODE)); */
    return retval;
}

void ar5416EnableTPC(struct ath_hal *ah)
{
    AH_PRIVATE(ah)->ah_config.ath_hal_desc_tpc = 1;
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE_MAX, MAX_RATE_POWER |AR_PHY_POWER_TX_RATE_MAX_TPC_ENABLE);
}


/*
 * ar5416ForceTSFSync 
 *      This function forces the TSF sync to the given bssid, this is implemented
 *      as a temp hack to get the AoW demo, and is primarily used in the WDS client
 *      mode of operation, where we sync the TSF to RootAP TSF values
 */
void
ar5416ForceTSFSync(struct ath_hal *ah, const u_int8_t *bssid, u_int16_t assocId)
{
    ar5416SetOperatingMode(ah, HAL_M_STA);
    ar5416WriteAssocid(ah, bssid, assocId);
}


/* GreenTX */
void ar5416ChkRSSIUpdateTxPwr(struct ath_hal *ah, int rssi)
{
    u_int32_t index, PALset = 0, Pwctrl8 = 0, Pwctrl9 = 0;
    HAL_RSSI_TX_POWER old_green_tx_status;

    if((AH_PRIVATE(ah)->ah_opmode != HAL_M_STA) || 
        !AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable) {
        return;
    }
    
    old_green_tx_status = AH_PRIVATE(ah)->green_tx_status;

    if ((AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable_S1) && (rssi > GreenTX_thres1)) {
        if(AH_PRIVATE(ah)->green_tx_status != HAL_RSSI_TX_POWER_SHORT) {
            AH_PRIVATE(ah)->green_tx_status = HAL_RSSI_TX_POWER_SHORT;
        }
    } else if(AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable_S2 && (rssi > GreenTX_thres2)) {
        if(AH_PRIVATE(ah)->green_tx_status != HAL_RSSI_TX_POWER_MIDDLE) {
            AH_PRIVATE(ah)->green_tx_status = HAL_RSSI_TX_POWER_MIDDLE;
        }
    } else if(AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable_S3) {
        if(AH_PRIVATE(ah)->green_tx_status != HAL_RSSI_TX_POWER_LONG) {
            AH_PRIVATE(ah)->green_tx_status = HAL_RSSI_TX_POWER_LONG;
        }
    }

    /* If status not change, don't do anything */
    if (old_green_tx_status == AH_PRIVATE(ah)->green_tx_status){
        return;
    }
    
    /* for UB94,K2 and Kite which ath_hal_sta_update_tx_pwr_enable is enabled */
    if ((AH_PRIVATE(ah)->green_tx_status != HAL_RSSI_TX_POWER_NONE) &&
        (AR_SREV_K2(ah) || AR_SREV_KITE(ah) || AR_SREV_MERLIN(ah) || AR_SREV_KIWI(ah))) {
        index = AH_PRIVATE(ah)->green_tx_status - 1;
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE1, AH5416(ah)->txPowerRecord[index][0]);
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE2, AH5416(ah)->txPowerRecord[index][1]);
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE3, AH5416(ah)->txPowerRecord[index][2]);
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE4, AH5416(ah)->txPowerRecord[index][3]);
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE5, AH5416(ah)->txPowerRecord[index][4]);
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE6, AH5416(ah)->txPowerRecord[index][5]);
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE7, AH5416(ah)->txPowerRecord[index][6]);
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE8, AH5416(ah)->txPowerRecord[index][7]);
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE9, AH5416(ah)->txPowerRecord[index][8]);
        
        if (AR_SREV_K2(ah) || AR_SREV_KITE(ah)){
            switch (AH_PRIVATE(ah)->green_tx_status) {
                case HAL_RSSI_TX_POWER_SHORT:
                    /* disable PAL */
                    PALset = AH5416(ah)->PALRecord | AR9285_AN_RF2G7_PWDDB;
                    /* Set OB/DB1/DB2 to 1/1/1 */
                    index = 1;
                    if (AR_SREV_KITE(ah)) {
                        Pwctrl8 = 0x2d6b5ad6;
                        Pwctrl9 = 0x050b02d6;
                    }
                    break;
                case HAL_RSSI_TX_POWER_MIDDLE:
                    /* Enable PAL, set 0x7838[1] to 0 */
                    PALset = AH5416(ah)->PALRecord;
                    /* Set OB/DB1/DB2 to 2/2/2 */
                    index = 2;
                    if (AR_SREV_KITE(ah)) {
                        Pwctrl8 = 0x35ad6b5a;
                        Pwctrl9 = 0x050d035a;
                    } 
                    break;
                case HAL_RSSI_TX_POWER_LONG:
                    /* enable PAL */
                    PALset = AH5416(ah)->PALRecord;
                    /* restore OB/DB1/DB2 */
                    index = 0;
                    if (AR_SREV_KITE(ah)) {
                        Pwctrl8 = 0x39ce739c;
                        Pwctrl9 = 0x050e039c;
                    }
                    break;
                default:
                    break;
            }
            
            OS_REG_WRITE(ah, AR9285_AN_RF2G7, PALset);

            OS_REG_WRITE(ah, AR9285_AN_RF2G3, AH_PRIVATE(ah)->ah_ob_db1[index]);
            OS_REG_WRITE(ah, AR9285_AN_RF2G4, AH_PRIVATE(ah)->ah_db2[index]);
            
            if (AR_SREV_KITE(ah)) {
                OS_REG_WRITE(ah, AR_PHY_TX_PWRCTRL8,Pwctrl8);
                OS_REG_WRITE(ah, AR_PHY_TX_PWRCTRL9,Pwctrl9);
            }
        }

		if (AR_SREV_KIWI(ah)){
            switch (AH_PRIVATE(ah)->green_tx_status) {
               case HAL_RSSI_TX_POWER_SHORT:                    
			   	    index = 1;
                    Pwctrl9 = 0; 
                   break;
               case HAL_RSSI_TX_POWER_MIDDLE:
			   		index = 2;
                    Pwctrl9 = 0;                   
                   break;
               case HAL_RSSI_TX_POWER_LONG: 
			   	    index = 0;
                    Pwctrl9 = 0;
                   break;
               default:
                   break;
           }
            
           OS_REG_WRITE(ah, AR9287_AN_RF2G3_CH0, AH_PRIVATE(ah)->ah_ob_db1[index]);
           OS_REG_WRITE(ah, AR9287_AN_RF2G3_CH1, AH_PRIVATE(ah)->ah_ob_db1[index]);
		   
           OS_REG_WRITE(ah, AR_PHY_TX_PWRCTRL9, 
                            (OS_REG_READ(ah, AR_PHY_TX_PWRCTRL9) &
                            (~AR_PHY_TX_DESIRED_SCALE_CCK)) |
                            (Pwctrl9 & AR_PHY_TX_DESIRED_SCALE_CCK));
        }
    }
}

/*
 * ar5416_is_skip_paprd_by_greentx
 *
 * This function check if we need to skip PAPRD tuning 
 * when GreenTx in specific state.
 */
HAL_BOOL 
ar5416_is_skip_paprd_by_greentx(struct ath_hal *ah)
{
    return AH_FALSE;
}

/*
 * ar5416_paprd_dec_tx_pwr
 *
 * This function check if we need to decrease tx power  
 * if Paprd is off or train fail.
 */
void 
ar5416_paprd_dec_tx_pwr(struct ath_hal *ah)
{
    return;
}

/*
 * ar5416DisablePhyRestart
 *
 * This function is applicable for latest ar9300, 
 * avoid null-function-pointer by having empty function
 */
void ar5416DisablePhyRestart(struct ath_hal *ah, int disable_phy_restart)
{
}
void ar5416GetVowStats(
    struct ath_hal *ah, HAL_VOWSTATS* p_stats, u_int8_t vow_reg_flags)
{
    if (vow_reg_flags & AR_REG_TX_FRM_CNT) {
        p_stats->tx_frame_count = OS_REG_READ(ah, AR_TFCNT);
    }
    if (vow_reg_flags & AR_REG_RX_FRM_CNT) {
        p_stats->rx_frame_count = OS_REG_READ(ah, AR_RFCNT);
    }
    if (vow_reg_flags & AR_REG_RX_CLR_CNT) {
        p_stats->rx_clear_count = OS_REG_READ(ah, AR_RCCNT);
    }
    if (vow_reg_flags & AR_REG_CYCLE_CNT) {
        p_stats->cycle_count   = OS_REG_READ(ah, AR_CCCNT);
    }
    if (vow_reg_flags & AR_REG_EXT_CYCLE_CNT) {
        p_stats->ext_cycle_count   = OS_REG_READ(ah, AR_EXTRCCNT);
    }
}


#ifdef ATH_TX99_DIAG
void ar5416_tx99_chainmsk_setup(struct ath_hal *ah, int tx_chainmask)
{
    switch (tx_chainmask) {
        case 0x5:
            OS_REG_WRITE(ah, AR_PHY_ANALOG_SWAP, 
                OS_REG_READ(ah, AR_PHY_ANALOG_SWAP) | AR_PHY_SWAP_ALT_CHAIN);
            /*
             * fall through !
             */
        case 0x3:
            {
                /*
                 * workaround for OWL 1.0 cal failure, always cal 3 chains for
                 * multi chain -- then after cal set true mask value
                 */
                if (AR_SREV_OWL_10(ah)) {
                    OS_REG_WRITE(ah, AR_PHY_RX_CHAINMASK, 0x7);
                    OS_REG_WRITE(ah, AR_PHY_CAL_CHAINMASK, 0x7);
                } else {
                    OS_REG_WRITE(ah, AR_PHY_RX_CHAINMASK, tx_chainmask);
                    OS_REG_WRITE(ah, AR_PHY_CAL_CHAINMASK, tx_chainmask);
                }
                break;
            }
            /*
             * fall through !
             */
        case 0x1:
        case 0x7:
            OS_REG_WRITE(ah, AR_PHY_RX_CHAINMASK, tx_chainmask);
            OS_REG_WRITE(ah, AR_PHY_CAL_CHAINMASK, tx_chainmask);
            break;
        default:
            break;
    }
    
    OS_REG_WRITE(ah, AR_SELFGEN_MASK, tx_chainmask);
    if (tx_chainmask == 0x5) {
        OS_REG_WRITE(ah, AR_PHY_ANALOG_SWAP, 
            OS_REG_READ(ah, AR_PHY_ANALOG_SWAP) | AR_PHY_SWAP_ALT_CHAIN);
    }
}

void
ar928x_tx99_set_single_carrier(struct ath_hal *ah, int tx_chain_mask, int chtype)
{
    u_int32_t temp_reg, val;

    OS_REG_WRITE(ah, 0x983c, ((0x7ff<<11) | 0x7ff));
    OS_REG_WRITE(ah, 0x9808, ((1<<7) | (1<<1)));
    OS_REG_WRITE(ah, 0x982c, 0x80008000);
    
    /* 11G mode */
    if (!chtype) {
        if (AR_SREV_KITE(ah)) {
            OS_REG_WRITE(ah, 0x7868, (u_int32_t)((0x1 << 29) | (0x1 << 27) |
                (0x1 << 17) | (0x1 << 16) |
                (0x1 << 14) | (0x1 << 12) |
                (0x1 << 11)  | (0x1 << 7)  |
                (0x1 << 5)));

            OS_DELAY(10000);

            temp_reg = OS_REG_READ(ah, 0x7864) & 0xc007ffff;
            val = OS_REG_READ(ah, 0xa274);
            temp_reg |= ((val & 0x0000000e) >> 1) << 27;
            temp_reg |= ((val & 0x00000030) >> 4) << 25;
            temp_reg |= ((val & 0x000001c0) >> 6) << 22;
            temp_reg |= ((val & 0x00001c00) >> 10) << 19;
            OS_REG_WRITE(ah, 0x7864, temp_reg);

        } else if (AR_SREV_KIWI(ah)) { 
            if((tx_chain_mask & 0x01) == 0x01) {
                OS_REG_WRITE(ah, 0x78ac, (u_int32_t)((0x1 << 29) | (0x1 << 20) |
                    (0x1 << 18) | (0x1 << 16) |
                    (0x1 << 12) | (0x1 << 10) |
                    (0x1 << 7)  | (0x1 << 5)  |
                    (0x1 << 3)));
            }
            /* chain one */
            if ((tx_chain_mask & 0x02) == 0x02 ) {
                OS_REG_WRITE(ah, 0x78ac, (u_int32_t)((0x1 << 29) | (0x1 << 20) |
                    (0x3 << 18) | (0x1 << 16) |
                    (0x3 << 12) | (0x3 << 10) |
                    (0x1 << 7)  | (0x1 << 5)  |
                    (0x1 << 3)));
            }
        } else {
            if((tx_chain_mask & 0x01) == 0x01) {
                OS_REG_WRITE(ah, 0x788c, (u_int32_t)((0x1 << 29) | (0x1 << 20) |
                    (0x1 << 18) | (0x1 << 16) |
                    (0x1 << 12) | (0x1 << 10) |
                    (0x1 << 7)  | (0x1 << 4)  |
                    (0x1 << 2)));
            }
            /* chain one */
            if ((tx_chain_mask & 0x02) == 0x02 ) {
                OS_REG_WRITE(ah, 0x788c, (u_int32_t)((0x1 << 29) | (0x1 << 20) |
                    (0x3 << 18) | (0x1 << 16) |
                    (0x3 << 12) | (0x3 << 10) |
                    (0x1 << 7)  | (0x1 << 4)  |
                    (0x1 << 2)));
            }
        }
    } else {
        if (AH_PRIVATE(ah)->ah_devid == AR5416_DEVID_AR9280_PCI) {
            OS_REG_WRITE(ah, AR_PHY_SWITCH_COM, 0x00000dd0);
        }
        /* chain zero */
        if( (tx_chain_mask & 0x01) == 0x01 ) {
            OS_REG_WRITE(ah, 0x788c, (u_int32_t)((0x1 << 29) | (0x0 << 20) |
                 (0x0 << 18) | (0x1 << 16) |
                 (0x1 << 12) | (0x1 << 10) |
                 (0x1 << 7)  | (0x1 << 4)  |
                 (0x1 << 2)));
        }
        /* chain one */
        if ((tx_chain_mask & 0x02) == 0x02 ) {
            OS_REG_WRITE(ah, 0x788c, (u_int32_t)((0x1 << 29) | (0x0 << 20) |
                 (0x0 << 18) | (0x1 << 16) |
                 (0x3 << 12) | (0x3 << 10) |
                 (0x1 << 7)  | (0x1 << 4)  |
                 (0x1 << 2)));
        }
    }

    /* force xpa on */
    if( (tx_chain_mask & 0x01) == 0x01 ) {
        OS_REG_WRITE(ah, 0xa3d8, ((0x1<<1) | 1)); /* for chain zero */
    }
    if( (tx_chain_mask & 0x02) == 0x02 ) {
        OS_REG_WRITE(ah, 0xa3d8, ((0x3<<1) | 1)); /* for chain one */
    }
}

void 
ar5416_tx99_set_single_carrier(struct ath_hal *ah, int tx_chain_mask, int chtype)
{
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        ar928x_tx99_set_single_carrier(ah, tx_chain_mask, chtype);
    } else {
        /* not support pre-merlin chips yet */
    }
}
void ar5416_tx99_channel_pwr_update(struct ath_hal *ah, HAL_CHANNEL *c, 
    u_int32_t txpower)
{
#define PWR_MAS(_r, _s)     (((_r) & 0x3f) << (_s))

    int16_t pwr_offset = 0;
    static int16_t ratesArray[Ar5416RateSize] = { 0 };
    int32_t i;
    int32_t cck_ofdm_delta = 0;
    u_int8_t ht40PowerIncForPdadc = 2;
    
    pwr_offset = AH_PRIVATE(ah)->ah_pwr_offset;

    /* The max power is limited to 63 */
    if ((txpower - pwr_offset*2) <= AR5416_MAX_RATE_POWER) {
        for (i = 0; i < Ar5416RateSize; i++) {
            ratesArray[i] = txpower - pwr_offset*2;
        }
    } else {
        for (i = 0; i < Ar5416RateSize; i++) {
            ratesArray[i] = AR5416_MAX_RATE_POWER;
        }
    }

    /* Write the OFDM power per rate set */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE1,
        PWR_MAS(ratesArray[rate18mb], 24)
          | PWR_MAS(ratesArray[rate12mb], 16)
          | PWR_MAS(ratesArray[rate9mb],  8)
          | PWR_MAS(ratesArray[rate6mb],  0)
    );

    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE2,
        PWR_MAS(ratesArray[rate54mb], 24)
          | PWR_MAS(ratesArray[rate48mb], 16)
          | PWR_MAS(ratesArray[rate36mb],  8)
          | PWR_MAS(ratesArray[rate24mb],  0)
    );

    if (IS_CHAN_2GHZ(c)) {
        /* Write the CCK power per rate set */
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE3,
            PWR_MAS(ratesArray[rate2s]-cck_ofdm_delta, 24)
              | PWR_MAS(ratesArray[rate2l]-cck_ofdm_delta,  16)
              | PWR_MAS(ratesArray[rateXr],  8) /* XR target power */
              | PWR_MAS(ratesArray[rate1l]-cck_ofdm_delta,   0)
        );

        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE4,
            PWR_MAS(ratesArray[rate11s]-cck_ofdm_delta, 24)
              | PWR_MAS(ratesArray[rate11l]-cck_ofdm_delta, 16)
              | PWR_MAS(ratesArray[rate5_5s]-cck_ofdm_delta,  8)
              | PWR_MAS(ratesArray[rate5_5l]-cck_ofdm_delta,  0)
        );
    }

    /* Write the HT20 power per rate set */
    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE5,
        PWR_MAS(ratesArray[rateHt20_3], 24)
          | PWR_MAS(ratesArray[rateHt20_2],  16)
          | PWR_MAS(ratesArray[rateHt20_1],  8)
          | PWR_MAS(ratesArray[rateHt20_0],   0)
    );

    OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE6,
        PWR_MAS(ratesArray[rateHt20_7], 24)
          | PWR_MAS(ratesArray[rateHt20_6],  16)
          | PWR_MAS(ratesArray[rateHt20_5],  8)
          | PWR_MAS(ratesArray[rateHt20_4],   0)
    );

    if (IS_CHAN_HT(c)) {
        /* Write the HT40 power per rate set */
        /* Correct the difference between HT40 and HT20/Legacy */
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE7,
            PWR_MAS(ratesArray[rateHt40_3] + ht40PowerIncForPdadc, 24)
              | PWR_MAS(ratesArray[rateHt40_2] + ht40PowerIncForPdadc,  16)
              | PWR_MAS(ratesArray[rateHt40_1] + ht40PowerIncForPdadc,  8)
              | PWR_MAS(ratesArray[rateHt40_0] + ht40PowerIncForPdadc,   0)
        );

        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE8,
            PWR_MAS(ratesArray[rateHt40_7] + ht40PowerIncForPdadc, 24)
              | PWR_MAS(ratesArray[rateHt40_6] + ht40PowerIncForPdadc,  16)
              | PWR_MAS(ratesArray[rateHt40_5] + ht40PowerIncForPdadc,  8)
              | PWR_MAS(ratesArray[rateHt40_4] + ht40PowerIncForPdadc,   0)
        );

        /* Write the Dup/Ext 40 power per rate set */
        OS_REG_WRITE(ah, AR_PHY_POWER_TX_RATE9,
            PWR_MAS(ratesArray[rateExtOfdm], 24)
              | PWR_MAS(ratesArray[rateExtCck]-cck_ofdm_delta,  16)
              | PWR_MAS(ratesArray[rateDupOfdm],  8)
              | PWR_MAS(ratesArray[rateDupCck]-cck_ofdm_delta,   0)
        );
    }
#undef PWR_MAS
}

void ar5416_tx99_start(struct ath_hal *ah, u_int8_t *data)
{
    u_int32_t val;
    u_int32_t qnum = (uintptr_t)data;

    /* Disable AGC to A2 */
    /* HT40 mode would not work well with this */
    //OS_REG_WRITE(ah, AR_PHY_TEST,
    //    (OS_REG_READ(ah, AR_PHY_TEST) | PHY_AGC_CLR));

    OS_REG_WRITE(ah, 0x9864, OS_REG_READ(ah, 0x9864) | 0x7f000);
    OS_REG_WRITE(ah, 0x9924, OS_REG_READ(ah, 0x9924) | 0x7f00fe);

    /* Follow ar9300_tx99_start() to solve problems in non-CCK rates */
    OS_REG_WRITE(ah, AR_DIAG_SW, OS_REG_READ(ah, AR_DIAG_SW) &~ AR_DIAG_RX_DIS);

    OS_REG_WRITE(ah, AR_CR, AR_CR_RXD);     /* set receive disable */
    /* set CW_MIN and CW_MAX both to 0, AIFS=2 */
    OS_REG_WRITE(ah, AR_DLCL_IFS(qnum), 0);
    //OS_REG_WRITE(sc->sc_ah, AR_DLCL_IFS(txep->axq_qnum), 0x200000);
    OS_REG_WRITE(ah, AR_D_GBL_IFS_SIFS, 20); /* 50 OK */
    OS_REG_WRITE(ah, AR_D_GBL_IFS_EIFS, 20);
    /* enable prefetch on qnum */
    OS_REG_WRITE(ah, AR_D_FPCTL, 0x10|qnum);
    /* 200 ok for HT20, 400 ok for HT40 */
    OS_REG_WRITE(ah, AR_TIME_OUT, 0x00000400);
    OS_REG_WRITE(ah, AR_DRETRY_LIMIT(qnum), 0xffffffff);
    
    /* set QCU modes to early termination */
    val = OS_REG_READ(ah, AR_QMISC(qnum));
    OS_REG_WRITE(ah, AR_QMISC(qnum), val | AR_Q_MISC_DCU_EARLY_TERM_REQ);
}

void ar5416_tx99_stop(struct ath_hal *ah)
{   
    /* this should follow the setting of start */
    OS_REG_WRITE(ah, AR_DIAG_SW, OS_REG_READ(ah, AR_DIAG_SW) | AR_DIAG_RX_DIS);
}
#endif /* ATH_TX99_DIAG */

HAL_BOOL ar5416Get3StreamSignature(struct ath_hal *ah)
{
    u_int32_t reg = 0;
    u_int32_t sig1 = 0;
    u_int32_t sig2 = 0;
    u_int32_t sig3 = 0;

    reg = OS_REG_READ(ah, AR_PHY_TEST);
    OS_REG_WRITE(ah, AR_PHY_TEST, reg & ~(0x1 << RX_OBS_SEL_5TH_BIT_S));
    OS_REG_WRITE(ah, AR_PHY_TESTCTRL, AR_PHY_TESTCTRL_RX_OBS_SEL_3RD_BIT);
    reg = OS_REG_READ(ah, AR_PHY_TSTADC_1);
    if (reg & AR_PHY_TSTADC_1_RX_CLEAR) {
        ath_hal_printf(ah,"sig1 rx_clear is set 0x9c24 %x\n",reg);
        sig1 = 1;
    }

    reg = OS_REG_READ(ah, AR_PHY_TEST);
    OS_REG_WRITE(ah, AR_PHY_TEST, reg & ~(0x1 << RX_OBS_SEL_5TH_BIT_S));
    OS_REG_WRITE(ah, AR_PHY_TESTCTRL, (AR_PHY_TESTCTRL_RX_OBS_SEL_1ST_BIT | AR_PHY_TESTCTRL_RX_OBS_SEL_3RD_BIT));
    reg = OS_REG_READ(ah, AR_PHY_TSTADC_1);
    if (reg & AR_PHY_TSTADC_1_HT_RATE_UNSUPPORTED) {
        ath_hal_printf(ah,"sig2 HT_unsupported rate is set 0x9c24 %x\n",reg);
        sig2 = 1;
    }

    reg = OS_REG_READ(ah, AR_PHY_TEST);
    OS_REG_WRITE(ah, AR_PHY_TEST, reg & ~(0x1 << AGC_OBS_SEL_4_S));
    reg = OS_REG_READ(ah, AR_PHY_TEST);
    OS_REG_WRITE(ah, AR_PHY_TEST, reg | (0x1 << AGC_OBS_SEL_3_S));
    OS_REG_WRITE(ah,AR_PHY_TESTCTRL, 0x6005d);
    reg = OS_REG_READ(ah, AR_PHY_TSTADC_2);
    if (reg & (0x1 << AR_PHY_TSTADC_2_RSSI_GT_RADAR_THRESH_S)) {
        ath_hal_printf(ah,"sig3 rssi_gt_radar_thresh is set 0x9c28 %x\n",reg);
        sig3 = 1;
    }
    if (sig1 && sig2 && sig3) {
        sig1 = 0;
        sig2 = 0;
        sig3 = 0;
        return AH_TRUE;
    }
    else
        return AH_FALSE;
}

                                                         

/*
 * ar5416ForceVCS
 *
 * This function forces the Virtual Carrier Sense to be cleared.
 * Used to reset the chip when it erroneously detects a long frame,
 * a situation which happens on older chips with DFS enabled under certain conditions
 * (Merlin when it receives 3 stream rates)
 */
HAL_BOOL
ar5416ForceVCS(struct ath_hal *ah)
{

    u_int32_t diag = 0;
    //Only for Merlin
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        diag = OS_REG_READ(ah, AR_DIAG_SW);
        OS_REG_WRITE(ah, AR_DIAG_SW, diag | AR_DIAG_FORCE_CH_IDLE_HIGH |  AR_DIAG_IGNORE_VIRT_CS );
        OS_DELAY(10000);
        OS_REG_WRITE(ah, AR_DIAG_SW, diag & ~AR_DIAG_FORCE_CH_IDLE_HIGH & ~AR_DIAG_IGNORE_VIRT_CS);
     }
     return AH_TRUE;
}

/*
 * ar5416SetDfs3StreamFix
 *
 * This function configures register settings to enable the DFS 3 stream fix
 * PHY errors on Unsupported (Illegal rates) is turned off
 * Power drop is enabled
 */
HAL_BOOL
ar5416SetDfs3StreamFix(struct ath_hal *ah, u_int32_t val)
{

    u_int32_t diag = 0;
    //Only for Merlin
    if (AR_SREV_MERLIN_10_OR_LATER(ah)) {
        if (val) {
           ath_hal_printf(ah, "Engaging DFS 3 stream fix\n");
           diag = OS_REG_READ(ah, AR_PHY_FRAME_CTL);
           OS_REG_WRITE(ah, AR_PHY_FRAME_CTL, diag & ~AR_PHY_FRAME_CTL_ILLEGAL_RATE);
           diag = OS_REG_READ(ah, AR_PHY_RESTART);
           OS_REG_WRITE(ah, AR_PHY_RESTART, diag | AR_PHY_RESTART_POWER_DROP);
        } else {
           ath_hal_printf(ah, "Disengaging DFS 3 stream fix\n");
           diag = OS_REG_READ(ah, AR_PHY_FRAME_CTL);
           OS_REG_WRITE(ah, AR_PHY_FRAME_CTL, diag | AR_PHY_FRAME_CTL_ILLEGAL_RATE);
           diag = OS_REG_READ(ah, AR_PHY_RESTART);
           OS_REG_WRITE(ah, AR_PHY_RESTART, diag & ~AR_PHY_RESTART_POWER_DROP);
        }
    }
     return AH_TRUE;
}

/* Fix for EV 108445 - Continuous beacon stuck */
void ar5416_reset_nav(struct ath_hal *ah)
{
	u_int32_t regval;

	/*check NAV register */
	regval = OS_REG_READ(ah, AR_NAV);
	if ((regval != 0xdeadbeef) && (regval > 0x7fff)) {
		printk("%s: abnormal nav value found 0x%x\n", __func__, regval);
		OS_REG_WRITE(ah, AR_NAV, 0);
	}

}
void ar5416_factory_defaults(struct ath_hal *ah)
{
        struct hal_reg_parm hal_conf_parm;
            ath_hal_factory_defaults(AH_PRIVATE(ah), &hal_conf_parm);
}
#endif /* AH_SUPPORT_AR5416 */
