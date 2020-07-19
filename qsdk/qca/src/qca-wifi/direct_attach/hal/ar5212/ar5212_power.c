/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
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

#ifdef AH_SUPPORT_AR5212

#include "ah.h"
#include "ah_internal.h"

#include "ar5212/ar5212.h"
#include "ar5212/ar5212reg.h"
#include "ar5212/ar5212desc.h"

/*
 * Notify Power Mgt is enabled in self-generated frames.
 * If requested, force chip awake.
 *
 * Returns A_OK if chip is awake or successfully forced awake.
 *
 * WARNING WARNING WARNING
 * There is a problem with the chip where sometimes it will not wake up.
 */
static HAL_BOOL
ar5212SetPowerModeAwake(struct ath_hal *ah, int set_chip)
{
#define	POWER_UP_TIME	2000
	u_int32_t val;
	int i;

	if (set_chip) {
		OS_REG_RMW_FIELD(ah, AR_SCR, AR_SCR_SLE, AR_SCR_SLE_WAKE);
		OS_DELAY(10);	/* Give chip the chance to awake */

		for (i = POWER_UP_TIME / 50; i != 0; i--) {
			val = OS_REG_READ(ah, AR_PCICFG);
			if ((val & AR_PCICFG_SPWR_DN) == 0)
				break;
			OS_DELAY(50);
			OS_REG_RMW_FIELD(ah, AR_SCR, AR_SCR_SLE,
				AR_SCR_SLE_WAKE);
		}
		if (i == 0) {
#ifdef AH_DEBUG
			HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: Failed to wakeup in %ums\n",
				__func__, POWER_UP_TIME/20);
#endif
			return AH_FALSE;
		}
	} 

	OS_REG_CLR_BIT(ah, AR_STA_ID1, AR_STA_ID1_PWR_SAV);
	return AH_TRUE;
#undef POWER_UP_TIME
}

/*
 * Notify Power Mgt is disabled in self-generated frames.
 * If requested, force chip to sleep.
 */
static void
ar5212SetPowerModeSleep(struct ath_hal *ah, int set_chip)
{
	OS_REG_SET_BIT(ah, AR_STA_ID1, AR_STA_ID1_PWR_SAV);
	if (set_chip)
		OS_REG_RMW_FIELD(ah, AR_SCR, AR_SCR_SLE, AR_SCR_SLE_SLP);
}

/*
 * Notify Power Management is enabled in self-generating
 * fames.  If request, set power mode of chip to
 * auto/normal.  Duration in units of 128us (1/8 TU).
 */
static void
ar5212SetPowerModeNetworkSleep(struct ath_hal *ah, int set_chip)
{
	OS_REG_SET_BIT(ah, AR_STA_ID1, AR_STA_ID1_PWR_SAV);
	if (set_chip)
		OS_REG_RMW_FIELD(ah, AR_SCR, AR_SCR_SLE, AR_SCR_SLE_NORM);
}

/*
 * Set power mgt to the requested mode, and conditionally set
 * the chip as well
 */
HAL_BOOL
ar5212SetPowerMode(struct ath_hal *ah, HAL_POWER_MODE mode, int set_chip)
{
	struct ath_hal_5212 *ahp = AH5212(ah);
#ifdef AH_DEBUG
	static const char* modes[] = {
		"UNDEFINED",
		"AWAKE",
		"FULL-SLEEP",
		"NETWORK SLEEP"
	};
#endif
	int status = AH_TRUE;

#ifdef AH_DEBUG
	HDPRINTF(ah, HAL_DBG_POWER_MGMT, "%s: %s -> %s (%s)\n", __func__,
		modes[ahp->ah_powerMode], modes[mode],
		set_chip ? "set chip " : "");
#endif
	switch (mode) {
	case HAL_PM_AWAKE:
		status = ar5212SetPowerModeAwake(ah, set_chip);
		break;
	case HAL_PM_FULL_SLEEP:
		ar5212SetPowerModeSleep(ah, set_chip);
		ahp->ah_chipFullSleep = AH_TRUE;
		break;
	case HAL_PM_NETWORK_SLEEP:
		ar5212SetPowerModeNetworkSleep(ah, set_chip);
		break;
	default:
		HDPRINTF(ah, HAL_DBG_POWER_MGMT, "%s: unknown power mode %u\n", __func__, mode);
		return AH_FALSE;
	}
	ahp->ah_powerMode = mode;
	return status;
}

#if 0
/*
 * Return the current sleep state of the chip
 * TRUE = sleeping
 */
HAL_BOOL
ar5212GetPowerStatus(struct ath_hal *ah)
{
	return (OS_REG_READ(ah, AR_PCICFG) & AR_PCICFG_SPWR_DN) != 0;
}
#endif

#if ATH_WOW

static HAL_BOOL
ar5212WowCreateKeepAlivePattern(struct ath_hal *ah)
{
    struct ath_hal_5212 *ahp = AH5212(ah);

    u_int32_t  frame_len = 28;
    u_int32_t  tpc = 0x3f;
    u_int32_t  antenna_mode = 1;
    u_int32_t  transmit_rate = 0xd; // 36 mbps
    u_int32_t  frame_type = 0x12; // frame type -> Data; Subtype -> Null Data
    u_int32_t  to_ds = 1;
    u_int32_t  duration_id = 0x3c;
    u_int8_t   *staMacAddr;
    u_int8_t   *apMacAddr;
    u_int8_t   *addr1, *addr2, *addr3;
    u_int32_t  data_word1 = 0, 
               data_word2 = 0, 
               data_word3 = 0, 
               data_word4 = 0, 
               data_word5 = 0;
    
    staMacAddr = (u_int8_t*)ahp->ah_macaddr;
    apMacAddr = (u_int8_t*)ahp->ah_bssid;
    
    addr2 = staMacAddr;
    addr1 = addr3 = apMacAddr;

    /* Set the Transmit Buffer */

    OS_REG_WRITE(ah, AR_WOW_KA_DESC_WORD2, (frame_len | (tpc << 16) | (antenna_mode << 25)));
    OS_REG_WRITE(ah, AR_WOW_KA_DESC_WORD3, 0);
    OS_REG_WRITE(ah, AR_WOW_KA_DESC_WORD4, 0x7ffff);
    OS_REG_WRITE(ah, AR_WOW_KA_DESC_WORD5, transmit_rate);

    data_word1 = (((u_int32_t)addr1[3] << 24) | ((u_int32_t)addr1[2] << 16) |
                  ((u_int32_t)addr1[1]) << 8 | ((u_int32_t)addr1[0]));
    data_word2 = (((u_int32_t)addr2[1] << 24) | ((u_int32_t)addr2[0] << 16) |
                  ((u_int32_t)addr1[5]) << 8 | ((u_int32_t)addr1[4]));
    data_word3 = (((u_int32_t)addr2[5] << 24) | ((u_int32_t)addr2[4] << 16) |
                  ((u_int32_t)addr2[3]) << 8 | ((u_int32_t)addr2[2]));
    data_word4 = (((u_int32_t)addr3[3] << 24) | ((u_int32_t)addr3[2] << 16) |
                  ((u_int32_t)addr3[1]) << 8 | (u_int32_t)addr3[0]);
    data_word5 = (((u_int32_t)addr3[5]) << 8 | ((u_int32_t)addr3[4]));

    OS_REG_WRITE(ah, AR_WOW_KA_DATA_WORD0, ((frame_type << 2) | (to_ds << 8) | (duration_id <<16)));
    OS_REG_WRITE(ah, AR_WOW_KA_DATA_WORD1, data_word1);
    OS_REG_WRITE(ah, AR_WOW_KA_DATA_WORD2, data_word2);
    OS_REG_WRITE(ah, AR_WOW_KA_DATA_WORD3, data_word3);
    OS_REG_WRITE(ah, AR_WOW_KA_DATA_WORD4, data_word4);
    OS_REG_WRITE(ah, AR_WOW_KA_DATA_WORD5, data_word5);

    return AH_TRUE;
}

/* TBD: Should querying hal for hardware capability */
#define MAX_PATTERN_SIZE                  256
#define MAX_PATTERN_MASK_SIZE             32
#define MAX_NUM_USER_PATTERN              6  /* Deducting the disassociate/deauthenticate packets */

void
ar5212WowApplyPattern(struct ath_hal *ah, u_int8_t *p_ath_pattern, u_int8_t *p_ath_mask, int32_t pattern_count, u_int32_t athPatternLen)
{
    int i;
    u_int32_t    reg_pat[] = {
                 (AR_WOW_TB_PATTERN0 + 4),  //FIXME, once the fix is ready in hw,
                 (AR_WOW_TB_PATTERN1 + 4),  //remove this +4 for all.
                 (AR_WOW_TB_PATTERN2 + 4),
                 (AR_WOW_TB_PATTERN3 + 4),
                 (AR_WOW_TB_PATTERN4 + 4),
                 (AR_WOW_TB_PATTERN5 + 4),
                 (AR_WOW_TB_PATTERN6 + 4),
                 (AR_WOW_TB_PATTERN7 + 4)
              };
    u_int32_t    reg_mask[] = {
                  AR_WOW_TB_MASK0,
                  AR_WOW_TB_MASK1,
                  AR_WOW_TB_MASK2,
                  AR_WOW_TB_MASK3,
                  AR_WOW_TB_MASK4,
                  AR_WOW_TB_MASK5,
                  AR_WOW_TB_MASK6,
                  AR_WOW_TB_MASK7
               };
    u_int32_t   pattern_val;
    u_int32_t   mask_val;
    u_int8_t    mask_bit = 0x1;
    u_int8_t    pattern;

/* TBD: should check count by querying the hardware capability */
    if (pattern_count >= MAX_NUM_USER_PATTERN) {
        return;
    }

    pattern = (u_int8_t)OS_REG_READ(ah, AR_WOW_PATTERN_ENABLE);
    pattern = pattern | (mask_bit << pattern_count);
    OS_REG_WRITE(ah, AR_WOW_PATTERN_ENABLE, pattern);

    /* Set the registers for pattern */
    for (i = 0; i < MAX_PATTERN_SIZE; i+=4) {
        pattern_val = (((u_int32_t)p_ath_pattern[i]) |
                      ((u_int32_t)p_ath_pattern[i+1] << 8) |
                      ((u_int32_t)p_ath_pattern[i+2] << 16) |
                      ((u_int32_t)p_ath_pattern[i+3] << 24));
        OS_REG_WRITE(ah, (reg_pat[pattern_count] + i), pattern_val);
    }   

    /* Set the registers for mask */
    for (i = 0; i < MAX_PATTERN_MASK_SIZE; i+=4) {
        mask_val = (((u_int32_t)p_ath_mask[i]) |
                   ((u_int32_t)p_ath_mask[i+1] << 8) |
                   ((u_int32_t)p_ath_mask[i+2] << 16) |
                   ((u_int32_t)p_ath_mask[i+3] << 24));
        OS_REG_WRITE(ah, (reg_mask[pattern_count] + i), mask_val);
    }

    return;
}

HAL_BOOL
ar5212WowEnable(struct ath_hal *ah, u_int32_t pattern_enable, u_int32_t timeoutInSeconds, int clearbssid,
                                                                                       bool offloadEnable)
{
    u_int32_t regVal32 = 0;

    /* WAR for Bug in Swan, 
    Disable the Beacon_failure_enable register
    Enable the Keep_alive_fail_disable registers*/
    OS_REG_WRITE(ah, AR_WOW_BEACON_FAIL_ENABLE, 0x0);
    OS_REG_WRITE(ah, AR_WOW_KEEP_ALIVE_FAIL_DISABLE, 0x0); 
    OS_REG_WRITE(ah, AR_WOW_KEEP_ALIVE_FRAME_DISABLE, 0x0);

    OS_REG_WRITE(ah, AR_WOW_KEEP_ALIVE_CNT, 0xFF);
    
    /* Create KeepAlive Pattern */
    ar5212WowCreateKeepAlivePattern(ah);

    /* Configure Mac Wow Registers */

    /* Enable the magic packet registers */
    if (pattern_enable & AR_WOW_MAGIC_PATTERN_EN) {
        OS_REG_WRITE(ah, AR_WOW_MAGIC_ENABLE, (AR_WOW_MAGIC_PATTERN_EN >> AR_WOW_MAGIC_PATTERN_EN_S));
    }

    
    // Enabling both Vcc and Vaux
    OS_REG_WRITE(ah, AR_GPIODO, 0x0);
    // Driving GPIOs for Vcc and Vaux
    OS_REG_WRITE(ah, AR_GPIOCR, 0xf);
    // Now enabling only Vaux
    OS_REG_WRITE(ah, AR_GPIODO, 0x1);

   /* WAR for SWAN
    * AR_WOW_HOST_PME_ENABLE is the main register to enable WoW from Swan 1.4 metal fix
    * Bits 8 and 9 are used to Work Around Standby issue in Swan and hence they overide
    * their actual purpose.
    * Bits 7 and 10 are to be set to enable WoW mechanism.
    */

    regVal32 = (AR_WOW_PME_CLEAR | AR_WOW_PWR_STATE_MASK_D3 | OS_REG_READ(ah, AR_WOW_HOST_PCIE_PMC));
    OS_REG_WRITE(ah, AR_WOW_HOST_PCIE_PMC, regVal32);
    
    regVal32 = (~AR_WOW_PME_CLEAR & OS_REG_READ(ah, AR_WOW_HOST_PCIE_PMC));
    OS_REG_WRITE(ah, AR_WOW_HOST_PCIE_PMC, regVal32);

    regVal32 = (AR_WOW_HOST_PME_ENABLE | AR_WOW_PWR_STATE_MASK_D0 |AR_WOW_PWR_STATE_MASK_D3 | OS_REG_READ(ah, AR_WOW_HOST_PCIE_PMC));
    OS_REG_WRITE(ah, AR_WOW_HOST_PCIE_PMC, regVal32);

    //Chip should operate on Full power in WoW mode    
    //ar5212SetPowerMode(ah, HAL_PM_NETWORK_SLEEP, TRUE);

    return AH_TRUE;
}



u_int32_t
//ar5212WowWakeUp(struct ath_hal *ah, u_int8_t  *chipPatternBytes)
ar5212WowWakeUp(struct ath_hal *ah, bool offloadEnabled)
{
    u_int32_t  wowStatus = 0;
    u_int32_t regVal32 = 0;
    
    // Enabling both Vcc and Vaux
    OS_REG_WRITE(ah, AR_GPIODO, 0x0);
    // Driving GPIOs for Vcc and Vaux
    OS_REG_WRITE(ah, AR_GPIOCR, 0xf);
    // Now enabling only Vcc
    OS_REG_WRITE(ah, AR_GPIODO, 0x2);
    
    /* Read the WOW STATUS register to know the wakeup reason */
    wowStatus = OS_REG_READ(ah, AR_WOW_STATUS);

    /* Set and then clear the WOW_PME_CLEAR registers for swan to generate
     * next wow signals.
     */
    regVal32 = OS_REG_READ(ah, AR_WOW_HOST_PCIE_PMC);
    regVal32 |= AR_WOW_PME_CLEAR;
    OS_REG_WRITE(ah, AR_WOW_HOST_PCIE_PMC, regVal32);
    regVal32 &= ~AR_WOW_PME_CLEAR;
    OS_REG_WRITE(ah, AR_WOW_PME_CLEAR, regVal32);

    return wowStatus;
}
#endif /*ATH_WOW*/

#endif /* AH_SUPPORT_AR5212 */
