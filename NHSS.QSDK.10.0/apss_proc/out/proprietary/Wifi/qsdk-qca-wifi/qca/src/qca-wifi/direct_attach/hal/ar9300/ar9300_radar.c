/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 * Copyright (c) 2008-2010, Atheros Communications Inc.
 * All Rights Reserved.
 */


#include "opt_ah.h"

#ifdef AH_SUPPORT_AR9300

#include "ah.h"
#include "ah_desc.h"
#include "ah_internal.h"

#include "ar9300/ar9300.h"
#include "ar9300/ar9300phy.h"
#include "ar9300/ar9300reg.h"
#include "ah_regdomain.h"
/*
 * Default 5413/9300 radar phy parameters
 * Values adjusted to fix EV76432/EV76320
 */
#define AR9300_DFS_FIRPWR   -28
#define AR9300_DFS_RRSSI    0
#define AR9300_DFS_HEIGHT   10
#define AR9300_DFS_PRSSI    6 
#define AR9300_DFS_INBAND   8
#define AR9300_DFS_RELPWR   8
#define AR9300_DFS_RELSTEP  12
#define AR9300_DFS_MAXLEN   255
#define AR9300_DFS_PRSSI_CAC 10 

#ifdef ATH_SUPPORT_DFS

/*
 * Enable radar detection and set the radar parameters per the
 * values in pe
 */
void
ar9300_enable_dfs(struct ath_hal *ah, HAL_PHYERR_PARAM *pe)
{
    u_int32_t val;
    struct ath_hal_private  *ahp = AH_PRIVATE(ah);
    HAL_CHANNEL_INTERNAL *ichan = ahp->ah_curchan;
    struct ath_hal_9300 *ah9300 = AH9300(ah);
    bool asleep = ah9300->ah_chip_full_sleep;
    int reg_writes = 0;

    if ((AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah) || AR_SREV_HONEYBEE(ah)) && asleep) {
        ar9300_set_power_mode(ah, HAL_PM_AWAKE, true);
    }

    val = OS_REG_READ(ah, AR_PHY_RADAR_0);
    val |= AR_PHY_RADAR_0_FFT_ENA | AR_PHY_RADAR_0_ENA;
    if (pe->pe_firpwr != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_FIRPWR;
        val |= SM(pe->pe_firpwr, AR_PHY_RADAR_0_FIRPWR);
    }
    if (pe->pe_rrssi != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_RRSSI;
        val |= SM(pe->pe_rrssi, AR_PHY_RADAR_0_RRSSI);
    }
    if (pe->pe_height != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_HEIGHT;
        val |= SM(pe->pe_height, AR_PHY_RADAR_0_HEIGHT);
    }
    if (pe->pe_prssi != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_PRSSI;
        if (AR_SREV_AR9580(ah) || AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_OSPREY(ah)) {
            if (ah->ah_use_cac_prssi) {
                val |= SM(AR9300_DFS_PRSSI_CAC, AR_PHY_RADAR_0_PRSSI);
            } else {
                val |= SM(pe->pe_prssi, AR_PHY_RADAR_0_PRSSI);
            }
        } else {
            val |= SM(pe->pe_prssi, AR_PHY_RADAR_0_PRSSI);
        }
    }
    if (pe->pe_inband != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_0_INBAND;
        val |= SM(pe->pe_inband, AR_PHY_RADAR_0_INBAND);
    }
    OS_REG_WRITE(ah, AR_PHY_RADAR_0, val);

    val = OS_REG_READ(ah, AR_PHY_RADAR_1);
    val |= AR_PHY_RADAR_1_MAX_RRSSI | AR_PHY_RADAR_1_BLOCK_CHECK;
    if (pe->pe_maxlen != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_1_MAXLEN;
        val |= SM(pe->pe_maxlen, AR_PHY_RADAR_1_MAXLEN);
    }
    if (pe->pe_relstep != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_1_RELSTEP_THRESH;
        val |= SM(pe->pe_relstep, AR_PHY_RADAR_1_RELSTEP_THRESH);
    }
    if (pe->pe_relpwr != HAL_PHYERR_PARAM_NOVAL) {
        val &= ~AR_PHY_RADAR_1_RELPWR_THRESH;
        val |= SM(pe->pe_relpwr, AR_PHY_RADAR_1_RELPWR_THRESH);
    }
    OS_REG_WRITE(ah, AR_PHY_RADAR_1, val);

    if (ath_hal_getcapability(ah, HAL_CAP_EXT_CHAN_DFS, 0, 0) == HAL_OK) {
        val = OS_REG_READ(ah, AR_PHY_RADAR_EXT);
        if (IS_CHAN_HT40(ichan)) {
            /* Enable extension channel radar detection */
            OS_REG_WRITE(ah, AR_PHY_RADAR_EXT, val | AR_PHY_RADAR_EXT_ENA);
        } else {
            /* HT20 mode, disable extension channel radar detect */
            OS_REG_WRITE(ah, AR_PHY_RADAR_EXT, val & ~AR_PHY_RADAR_EXT_ENA);
        }
    }
    /*
        apply DFS postamble array from INI
        column 0 is register ID, column 1 is  HT20 value, colum2 is HT40 value
    */

    if (AR_SREV_AR9580(ah) || AR_SREV_WASP(ah) || AR_SREV_OSPREY_22(ah) || AR_SREV_SCORPION(ah)) {
        REG_WRITE_ARRAY(&ah9300->ah_ini_dfs,IS_CHAN_HT40(ichan)? 2:1, reg_writes);
    }
#ifdef ATH_HAL_DFS_CHIRPING_FIX_APH128
    HDPRINTF(ah, HAL_DBG_DFS,"DFS change the timing value\n");
    if (AR_SREV_AR9580(ah) && IS_CHAN_HT40(ichan)) {
        OS_REG_WRITE(ah, AR_PHY_TIMING6, 0x3140c00a);	
    }
#endif
    if ((AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah) || AR_SREV_HONEYBEE(ah)) && asleep) {
        ar9300_set_power_mode(ah, HAL_PM_FULL_SLEEP, true);
    }
}

bool
ar9300_radar_wait(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);

    if (!ahp->ah_curchan) {
        return true;
    }

    /*
     * Rely on the upper layers to determine that we have spent
     * enough time waiting.
     */
    chan->channel = ahp->ah_curchan->channel;
    chan->channel_flags = ahp->ah_curchan->channel_flags;
    chan->max_reg_tx_power = ahp->ah_curchan->max_reg_tx_power;

    ahp->ah_curchan->priv_flags |= CHANNEL_DFS_CLEAR;
    chan->priv_flags  = ahp->ah_curchan->priv_flags;
    return false;

}

/*
 * Get the radar parameter values and return them in the pe
 * structure
 */
void
ar9300_get_dfs_thresh(struct ath_hal *ah, HAL_PHYERR_PARAM *pe)
{
    u_int32_t val, temp;
    struct ath_hal_9300 *ahp = AH9300(ah);
    bool asleep = ahp->ah_chip_full_sleep;
    
    if ((AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah) || AR_SREV_HONEYBEE(ah)) && asleep) {
        ar9300_set_power_mode(ah, HAL_PM_AWAKE, true);
    }

    val = OS_REG_READ(ah, AR_PHY_RADAR_0);
    temp = MS(val, AR_PHY_RADAR_0_FIRPWR);
    temp |= ~(AR_PHY_RADAR_0_FIRPWR >> AR_PHY_RADAR_0_FIRPWR_S);
    pe->pe_firpwr = temp;
    pe->pe_rrssi = MS(val, AR_PHY_RADAR_0_RRSSI);
    pe->pe_height = MS(val, AR_PHY_RADAR_0_HEIGHT);
    pe->pe_prssi = MS(val, AR_PHY_RADAR_0_PRSSI);
    pe->pe_inband = MS(val, AR_PHY_RADAR_0_INBAND);

    val = OS_REG_READ(ah, AR_PHY_RADAR_1);

    pe->pe_relpwr = MS(val, AR_PHY_RADAR_1_RELPWR_THRESH);
    if (val & AR_PHY_RADAR_1_RELPWR_ENA) {
        pe->pe_relpwr |= HAL_PHYERR_PARAM_ENABLE;
    }
    pe->pe_relstep = MS(val, AR_PHY_RADAR_1_RELSTEP_THRESH);
    if (val & AR_PHY_RADAR_1_RELSTEP_CHECK) {
        pe->pe_relstep |= HAL_PHYERR_PARAM_ENABLE;
    }
    pe->pe_maxlen = MS(val, AR_PHY_RADAR_1_MAXLEN);
    
    if ((AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah) || AR_SREV_HONEYBEE(ah)) && asleep) {
        ar9300_set_power_mode(ah, HAL_PM_FULL_SLEEP, true);
    }
}

void ar9300_adjust_difs(struct ath_hal *ah, u_int32_t val)
{
    if (val == 0) {
        /*
         * EV 116936:
         * Restore the register values with that of the HAL structure.
         * Do not assume and overwrite these values to whatever 
         * is in ar9300_osprey22.ini.
         */
        struct ath_hal_9300 *ahp = AH9300(ah);
        HAL_TX_QUEUE_INFO *qi;
        int q;

        ah->ah_fccaifs = 0;
        HDPRINTF(ah, HAL_DBG_DFS, "%s: restore DIFS \n", __func__);
        for (q = 0; q < 4; q++) {
            qi = &ahp->ah_txq[q];
            OS_REG_WRITE(ah, AR_DLCL_IFS(q),
                    SM(qi->tqi_cwmin, AR_D_LCL_IFS_CWMIN)
                    | SM(qi->tqi_cwmax, AR_D_LCL_IFS_CWMAX)
                    | SM(qi->tqi_aifs, AR_D_LCL_IFS_AIFS));
        }
    } else {
        /*
         * These are values from George Lai and are specific to
         * FCC domain. They are yet to be determined for other domains. 
         */

        ah->ah_fccaifs = 1;
        HDPRINTF(ah, HAL_DBG_DFS, "%s: set DIFS to default\n", __func__);
        /*printk("%s:  modify DIFS\n", __func__);*/
        
        OS_REG_WRITE(ah, AR_DLCL_IFS(0), 0x05fffc0f);
        OS_REG_WRITE(ah, AR_DLCL_IFS(1), 0x05f0fc0f);
        OS_REG_WRITE(ah, AR_DLCL_IFS(2), 0x05f03c07);
        OS_REG_WRITE(ah, AR_DLCL_IFS(3), 0x05f01c03);
    }
}

u_int32_t ar9300_dfs_config_fft(struct ath_hal *ah, bool is_enable)
{
    u_int32_t val;
    struct ath_hal_9300 *ahp = AH9300(ah);
    bool asleep = ahp->ah_chip_full_sleep;

    if ((AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah) || AR_SREV_HONEYBEE(ah)) && asleep) {
        ar9300_set_power_mode(ah, HAL_PM_AWAKE, true);
    }

    val = OS_REG_READ(ah, AR_PHY_RADAR_0);

    if (is_enable) {
        val |= AR_PHY_RADAR_0_FFT_ENA;
    } else {
        val &= ~AR_PHY_RADAR_0_FFT_ENA;
    }

    OS_REG_WRITE(ah, AR_PHY_RADAR_0, val);
    val = OS_REG_READ(ah, AR_PHY_RADAR_0);
    
    if ((AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah) || AR_SREV_HONEYBEE(ah)) && asleep) {
        ar9300_set_power_mode(ah, HAL_PM_FULL_SLEEP, true);
    }
    return val;
}
/*
    function to adjust PRSSI value for CAC problem

*/
void
ar9300_dfs_cac_war(struct ath_hal *ah, u_int32_t start)
{
    u_int32_t val;
    struct ath_hal_9300 *ahp = AH9300(ah);
    bool asleep = ahp->ah_chip_full_sleep;

    if ((AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah) || AR_SREV_HONEYBEE(ah)) && asleep) {
        ar9300_set_power_mode(ah, HAL_PM_AWAKE, true);
    }

    if (AR_SREV_AR9580(ah) || AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_OSPREY(ah)) {
        val = OS_REG_READ(ah, AR_PHY_RADAR_0);
        if (start) {
            val &= ~AR_PHY_RADAR_0_PRSSI;
            val |= SM(AR9300_DFS_PRSSI_CAC, AR_PHY_RADAR_0_PRSSI); 
        } else {
            val &= ~AR_PHY_RADAR_0_PRSSI;
            val |= SM(AR9300_DFS_PRSSI, AR_PHY_RADAR_0_PRSSI);
        }
        OS_REG_WRITE(ah, AR_PHY_RADAR_0, val | AR_PHY_RADAR_0_ENA);
        ah->ah_use_cac_prssi = start;
    }
    
    if ((AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah) || AR_SREV_HONEYBEE(ah)) && asleep) {
        ar9300_set_power_mode(ah, HAL_PM_FULL_SLEEP, true);
    }
}
#endif /* ATH_SUPPORT_DFS */

HAL_CHANNEL *
ar9300_get_extension_channel(struct ath_hal *ah)
{
    struct ath_hal_private  *ahp = AH_PRIVATE(ah);
    struct ath_hal_private_tables  *aht = AH_TABLES(ah);
    int i = 0;

    HAL_CHANNEL_INTERNAL *ichan = AH_NULL;
    CHAN_CENTERS centers;

    ichan = ahp->ah_curchan;
    ar9300_get_channel_centers(ah, ichan, &centers);
    if (centers.ctl_center == centers.ext_center) {
        return AH_NULL;
    }
    for (i = 0; i < ahp->ah_nchan; i++) {
        ichan = &aht->ah_channels[i];
        if (ichan->channel == centers.ext_center) {
            return (HAL_CHANNEL*)ichan;
        }
    }
    return AH_NULL;
}


bool
ar9300_is_fast_clock_enabled(struct ath_hal *ah)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);

    if (IS_5GHZ_FAST_CLOCK_EN(ah, ahp->ah_curchan)) {
        return true;
    }
    return false;
}

u_int16_t
ar9300_get_ah_devid(struct ath_hal *ah)
{
    struct ath_hal_private *ahp = AH_PRIVATE(ah);

    return ahp->ah_devid;
}

bool
ar9300_handle_radar_bb_panic(struct ath_hal *ah)
{
    u_int32_t status;
    u_int32_t val;   
    struct ath_hal_9300 *ahp = AH9300(ah);
    bool asleep = ahp->ah_chip_full_sleep;
       
    status = AH_PRIVATE(ah)->ah_bb_panic_last_status;
   
    if ( status == 0x04000539 ) { 
        if ((AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah) || AR_SREV_HONEYBEE(ah)) && asleep) {
            ar9300_set_power_mode(ah, HAL_PM_AWAKE, true);
        } 
        /* recover from this BB panic without reset*/
        /* set AR9300_DFS_FIRPWR to -1 */
        val = OS_REG_READ(ah, AR_PHY_RADAR_0);
        val &= (~AR_PHY_RADAR_0_FIRPWR);
        val |= SM( 0x7f, AR_PHY_RADAR_0_FIRPWR);
        OS_REG_WRITE(ah, AR_PHY_RADAR_0, val);
        OS_DELAY(1);
        /* set AR9300_DFS_FIRPWR to its default value */
        val = OS_REG_READ(ah, AR_PHY_RADAR_0);
        val &= ~AR_PHY_RADAR_0_FIRPWR;
        val |= SM( AR9300_DFS_FIRPWR, AR_PHY_RADAR_0_FIRPWR);
        OS_REG_WRITE(ah, AR_PHY_RADAR_0, val);
        if ((AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah) || AR_SREV_HONEYBEE(ah)) && asleep) {
            ar9300_set_power_mode(ah, HAL_PM_FULL_SLEEP, true);
        }   
        return true;
    } else if (status == 0x0400000a) {
        /* EV 92527 : reset required if we see this signature */
        HDPRINTF(ah, HAL_DBG_DFS, "%s: BB Panic -- 0x0400000a\n", __func__);
        return false;
    } else if (status == 0x1300000a) {
        /* EV92527: we do not need a reset if we see this signature */
        HDPRINTF(ah, HAL_DBG_DFS, "%s: BB Panic -- 0x1300000a\n", __func__);
        return true;
    } else if ((AR_SREV_WASP(ah) || AR_SREV_HONEYBEE(ah) || AR_SREV_DRAGONFLY(ah)) && (status == 0x04000409)) {
        return true;
    } else {
        if (ar9300_get_capability(ah, HAL_CAP_LDPCWAR, 0, AH_NULL) == HAL_OK &&
            (status & 0xff00000f) == 0x04000009 &&
            status != 0x04000409 &&
            status != 0x04000b09 &&
            status != 0x04000e09 &&
            (status & 0x0000ff00))
        {
            /* disable RIFS Rx */
#ifdef AH_DEBUG
            HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: BB status=0x%08x rifs=%d - disable\n",
                     __func__, status, ahp->ah_rifs_enabled);
#endif
            ar9300_set_rifs_delay(ah, false);
        }
        return false;
    }
}

#endif
