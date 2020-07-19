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
 */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR9300

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"

#include "ar9300.h"
#include "ar9300reg.h"
#include "ar9300phy.h"
#include "ar9300desc.h"

#if ATH_SUPPORT_RADIO_RETENTION

#define AH_RTT_AGCCTRL_TIMEOUT          100000 /* default timeout (usec) */
#define AH_RTT_RESTORE_TIMEOUT          1000 /* default timeout (usec) */
#define AH_RTT_ACCESS_TIMEOUT           100 /* default timeout (usec) */
#define AH_RTT_BAD_VALUE                0x0bad0bad

/* RTT (Radio Retention Table) hardware implementation information
 *
 * There is an internal table (i.e. the rtt) for each chain (or bank).
 * Each table contains 6 entries and each entry is corresponding to
 * a specific calibration parameter as depicted below.
 *  0~2 - DC offset DAC calibration: loop, low, high (offsetI/Q_...)
 *  3   - Filter cal (filterfc)
 *  4   - RX gain settings
 *  5   - Peak detector offset calibration (agc_caldac)
 *
 */

void ar9300_rtt_enable(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_PHY_RTT_CTRL, 1);
}

void ar9300_rtt_disable(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_PHY_RTT_CTRL, 0);
}

void ar9300_rtt_set_mask(struct ath_hal *ah, u_int32_t rtt_mask)
{
    OS_REG_RMW_FIELD(ah, AR_PHY_RTT_CTRL, AR_PHY_RTT_CTRL_RESTORE_MASK, rtt_mask);
}

bool ar9300_rtt_force_restore(struct ath_hal *ah)
{
	if (!ath_hal_wait(ah, AR_PHY_RTT_CTRL,
            AR_PHY_RTT_CTRL_FORCE_RADIO_RESTORE,
            0,
            AH_RTT_RESTORE_TIMEOUT))
    {
	    HDPRINTF(ah, HAL_DBG_FCS_RTT,
	        "(RTT) %s: last restore not finished\n", __func__);
	    return false;
	}

	OS_REG_RMW_FIELD(ah, AR_PHY_RTT_CTRL, AR_PHY_RTT_CTRL_FORCE_RADIO_RESTORE, 1);

	if (!ath_hal_wait(ah, AR_PHY_RTT_CTRL,
            AR_PHY_RTT_CTRL_FORCE_RADIO_RESTORE,
            0,
            AH_RTT_RESTORE_TIMEOUT))
    {
	    HDPRINTF(ah, HAL_DBG_FCS_RTT,
	        "(RTT) %s: restore not finished\n", __func__);
	    return false;
	}
    HDPRINTF(ah, HAL_DBG_FCS_RTT, "(RTT) %s: restore finished\n", __func__);

    return true;
}

static int ar9300_rtt_write_table_entry_0(struct ath_hal *ah, u_int32_t index,
                                          u_int32_t data28)
{
    u_int32_t val, val_1, val_1_read;
    
    val = SM(0, AR_PHY_RTT_SW_RTT_TABLE_ACCESS_0) |
          SM(1, AR_PHY_RTT_SW_RTT_TABLE_WRITE_0) |
          SM(index, AR_PHY_RTT_SW_RTT_TABLE_ADDR_0);
    val_1 = SM(data28, AR_PHY_RTT_SW_RTT_TABLE_DATA_0);

    /* write data and command */
    OS_REG_WRITE(ah, AR_PHY_RTT_TABLE_SW_INTF_1_B0, val_1);
    OS_REG_WRITE(ah, AR_PHY_RTT_TABLE_SW_INTF_B0, val);
    OS_DELAY(1);

    /* trigger command */
    val |= SM(1, AR_PHY_RTT_SW_RTT_TABLE_ACCESS_0);
    OS_REG_WRITE(ah, AR_PHY_RTT_TABLE_SW_INTF_B0, val);
    OS_DELAY(1);
	if (!ath_hal_wait(ah, AR_PHY_RTT_TABLE_SW_INTF_B0,
        AR_PHY_RTT_SW_RTT_TABLE_ACCESS_0,
        0,
        AH_RTT_ACCESS_TIMEOUT))
    {
	    HDPRINTF(ah, HAL_DBG_FCS_RTT,
	        "(RTT) %s: rtt access not finished\n", __func__);
	    return AH_RTT_BAD_VALUE;
	}

    /* trigger read command */
    val &= ~SM(1, AR_PHY_RTT_SW_RTT_TABLE_WRITE_0);
    OS_REG_WRITE(ah, AR_PHY_RTT_TABLE_SW_INTF_B0, val);
    OS_DELAY(1);
	if (!ath_hal_wait(ah, AR_PHY_RTT_TABLE_SW_INTF_B0,
        AR_PHY_RTT_SW_RTT_TABLE_ACCESS_0,
        0,
        AH_RTT_ACCESS_TIMEOUT))
    {
	    HDPRINTF(ah, HAL_DBG_FCS_RTT,
	        "(RTT) %s: rtt access not finished\n", __func__);
	    return AH_RTT_BAD_VALUE;
	}

    /* read back */
    val_1_read = OS_REG_READ(ah, AR_PHY_RTT_TABLE_SW_INTF_1_B0);

    return val_1_read;
}

static int ar9300_rtt_write_table_entry_1(struct ath_hal *ah, u_int32_t index,
                                          u_int32_t data28)
{
    u_int32_t val, val_1, val_1_read;
    
    val = SM(0, AR_PHY_RTT_SW_RTT_TABLE_ACCESS_1) |
          SM(1, AR_PHY_RTT_SW_RTT_TABLE_WRITE_1) |
          SM(index, AR_PHY_RTT_SW_RTT_TABLE_ADDR_1);
    val_1 = SM(data28, AR_PHY_RTT_SW_RTT_TABLE_DATA_1);

    /* write data and command */
    OS_REG_WRITE(ah, AR_PHY_RTT_TABLE_SW_INTF_1_B1, val_1);
    OS_REG_WRITE(ah, AR_PHY_RTT_TABLE_SW_INTF_B1, val);
    OS_DELAY(1);

    /* trigger command */
    val |= SM(1, AR_PHY_RTT_SW_RTT_TABLE_ACCESS_1);
    OS_REG_WRITE(ah, AR_PHY_RTT_TABLE_SW_INTF_B1, val);
    OS_DELAY(1);
	if (!ath_hal_wait(ah, AR_PHY_RTT_TABLE_SW_INTF_B1,
        AR_PHY_RTT_SW_RTT_TABLE_ACCESS_1,
        0,
        AH_RTT_ACCESS_TIMEOUT))
    {
	    HDPRINTF(ah, HAL_DBG_FCS_RTT,
	        "(RTT) %s: rtt access not finished\n", __func__);
	    return AH_RTT_BAD_VALUE;
	}

    /* trigger read command */
    val &= ~SM(1, AR_PHY_RTT_SW_RTT_TABLE_WRITE_1);
    OS_REG_WRITE(ah, AR_PHY_RTT_TABLE_SW_INTF_B1, val);
    OS_DELAY(1);
	if (!ath_hal_wait(ah, AR_PHY_RTT_TABLE_SW_INTF_B1,
        AR_PHY_RTT_SW_RTT_TABLE_ACCESS_1,
        0,
        AH_RTT_ACCESS_TIMEOUT))
    {
	    HDPRINTF(ah, HAL_DBG_FCS_RTT,
	        "(RTT) %s: rtt access not finished\n", __func__);
	    return AH_RTT_BAD_VALUE;
	}

    /* read back */
    val_1_read = OS_REG_READ(ah, AR_PHY_RTT_TABLE_SW_INTF_1_B1);

    return val_1_read;
}

int ar9300_rtt_write_table_entry(struct ath_hal *ah, u_int32_t chain,
                                 u_int32_t index, u_int32_t data)
{
    if ((index >= AH_RTT_MAX_NUM_TABLE_ENTRY) ||
        (chain >= AH9300(ah)->radio_retention_chains))
    {
        return AH_RTT_BAD_VALUE;
    }
    if (chain == 0) {
        return ar9300_rtt_write_table_entry_0(ah, index, data>>4);
    }
    if (chain == 1) {
        return ar9300_rtt_write_table_entry_1(ah, index, data>>4);
    }
    return AH_RTT_BAD_VALUE;
}

int ar9300_rtt_write_table(struct ath_hal *ah, u_int32_t chain,
                           u_int32_t *table, u_int32_t n_item)
{
    int i;

    if (n_item > AH_RTT_MAX_NUM_TABLE_ENTRY) {
        n_item = AH_RTT_MAX_NUM_TABLE_ENTRY;
    }
    if (chain < AH9300(ah)->radio_retention_chains) {
        for (i=0; i<n_item; i++) {
            ar9300_rtt_write_table_entry(ah, chain, i, table[i]);
        }
    }
    
    return 0;
}

static int ar9300_rtt_read_table_entry_0(struct ath_hal *ah, u_int32_t index)
{
    u_int32_t val, val_1_read;
    
    val = SM(0, AR_PHY_RTT_SW_RTT_TABLE_ACCESS_0) |
          SM(0, AR_PHY_RTT_SW_RTT_TABLE_WRITE_0) |
          SM(index, AR_PHY_RTT_SW_RTT_TABLE_ADDR_0);

    /* write command */
    OS_REG_WRITE(ah, AR_PHY_RTT_TABLE_SW_INTF_B0, val);
    OS_DELAY(1);

    /* trigger command */
    val |= SM(1, AR_PHY_RTT_SW_RTT_TABLE_ACCESS_0);
    OS_REG_WRITE(ah, AR_PHY_RTT_TABLE_SW_INTF_B0, val);
    OS_DELAY(1);
	if (!ath_hal_wait(ah, AR_PHY_RTT_TABLE_SW_INTF_B0,
        AR_PHY_RTT_SW_RTT_TABLE_ACCESS_0,
        0,
        AH_RTT_ACCESS_TIMEOUT))
    {
	    HDPRINTF(ah, HAL_DBG_FCS_RTT,
	        "(RTT) %s: rtt access not finished\n", __func__);
	    return AH_RTT_BAD_VALUE;
	}

    /* read back */
    val = OS_REG_READ(ah, AR_PHY_RTT_TABLE_SW_INTF_B0);
    val_1_read = OS_REG_READ(ah, AR_PHY_RTT_TABLE_SW_INTF_1_B0);

    return val_1_read;
}

static int ar9300_rtt_read_table_entry_1(struct ath_hal *ah, u_int32_t index)
{
    u_int32_t val, val_1_read;
    
    val = SM(0, AR_PHY_RTT_SW_RTT_TABLE_ACCESS_1) |
          SM(0, AR_PHY_RTT_SW_RTT_TABLE_WRITE_1) |
          SM(index, AR_PHY_RTT_SW_RTT_TABLE_ADDR_1);

    /* write command */
    OS_REG_WRITE(ah, AR_PHY_RTT_TABLE_SW_INTF_B1, val);
    OS_DELAY(1);

    /* trigger command */
    val |= SM(1, AR_PHY_RTT_SW_RTT_TABLE_ACCESS_1);
    OS_REG_WRITE(ah, AR_PHY_RTT_TABLE_SW_INTF_B1, val);
    OS_DELAY(1);
	if (!ath_hal_wait(ah, AR_PHY_RTT_TABLE_SW_INTF_B1,
        AR_PHY_RTT_SW_RTT_TABLE_ACCESS_1,
        0,
        AH_RTT_ACCESS_TIMEOUT))
    {
	    HDPRINTF(ah, HAL_DBG_FCS_RTT,
	        "(RTT) %s: rtt access not finished\n", __func__);
	    return AH_RTT_BAD_VALUE;
	}

    /* read back */
    val = OS_REG_READ(ah, AR_PHY_RTT_TABLE_SW_INTF_B1);
    val_1_read = OS_REG_READ(ah, AR_PHY_RTT_TABLE_SW_INTF_1_B1);

    return val_1_read;
}

int ar9300_rtt_read_table_entry(struct ath_hal *ah, u_int32_t chain,
                                u_int32_t index)
{
    if ((index >= AH_RTT_MAX_NUM_TABLE_ENTRY) ||
        (chain >= AH9300(ah)->radio_retention_chains))
    {
        return AH_RTT_BAD_VALUE;
    }
    if (chain == 0) {
        return ar9300_rtt_read_table_entry_0(ah, index);
    }
    if (chain == 1) {
        return ar9300_rtt_read_table_entry_1(ah, index);
    }
    return AH_RTT_BAD_VALUE;
}

int ar9300_rtt_read_table(struct ath_hal *ah, u_int32_t chain,
                          u_int32_t *table, u_int32_t n_item)
{
    int i;

    if (n_item > AH_RTT_MAX_NUM_TABLE_ENTRY) {
        n_item = AH_RTT_MAX_NUM_TABLE_ENTRY;
    }
    if (chain < AH9300(ah)->radio_retention_chains) {
        for (i=0; i<n_item; i++) {
            table[i] = ar9300_rtt_read_table_entry(ah, chain, i);
        }
    }
    
    return 0;
}

int ar9300_rtt_clear_table(struct ath_hal *ah, u_int32_t chain)
{
    int i;

    if (chain < AH9300(ah)->radio_retention_chains) {
        for (i=0; i<AH_RTT_MAX_NUM_TABLE_ENTRY; i++) {
            ar9300_rtt_write_table_entry(ah, chain, i, 0);
        }
    }
    
    return 0;
}

void ar9300_rtt_dump_table(struct ath_hal *ah, u_int32_t chain)
{
#if DBG
    u_int32_t table[AH_RTT_MAX_NUM_TABLE_ENTRY];
    int i;

    ar9300_rtt_read_table(ah, chain, table, AH_RTT_MAX_NUM_TABLE_ENTRY);
    for (i=0; i<AH_RTT_MAX_NUM_TABLE_ENTRY; i++)
    {
        HDPRINTF(ah, HAL_DBG_FCS_RTT,
            "(RTT) C%d-%d: 0x%07x_%x\n", chain, i, table[i] >> 4, table[i] & 0xf);
    }
#endif
}

void ar9300_rtt_dump_registers(struct ath_hal *ah)
{
#if DBG
    HDPRINTF(ah, HAL_DBG_FCS_RTT,
        "(RTT) BB_rtt_ctrl               @ 0x%08x\n",
        OS_REG_READ(ah, AR_PHY_RTT_CTRL));
    HDPRINTF(ah, HAL_DBG_FCS_RTT,
        "(RTT)   ena_radio_retention = %d\n",
        OS_REG_READ_FIELD(ah, AR_PHY_RTT_CTRL,
        AR_PHY_RTT_CTRL_ENA_RADIO_RETENTION));
    HDPRINTF(ah, HAL_DBG_FCS_RTT,
        "(RTT)   restore_mask        = 0x%x\n",
        OS_REG_READ_FIELD(ah, AR_PHY_RTT_CTRL, AR_PHY_RTT_CTRL_RESTORE_MASK));
    HDPRINTF(ah, HAL_DBG_FCS_RTT,
        "(RTT)   force_radio_restore = %d\n",
        OS_REG_READ_FIELD(ah, AR_PHY_RTT_CTRL,
        AR_PHY_RTT_CTRL_FORCE_RADIO_RESTORE));
    HDPRINTF(ah, HAL_DBG_FCS_RTT,
        "(RTT) BB_rtt_table_sw_intf_b0   @ 0x%08x\n",
        OS_REG_READ(ah, AR_PHY_RTT_TABLE_SW_INTF_B0));
    HDPRINTF(ah, HAL_DBG_FCS_RTT,
        "(RTT) BB_rtt_table_sw_intf_1_b0 @ 0x%08x\n",
        OS_REG_READ(ah, AR_PHY_RTT_TABLE_SW_INTF_1_B0));
    HDPRINTF(ah, HAL_DBG_FCS_RTT,
        "(RTT) BB_rtt_table_sw_intf_b1   @ 0x%08x\n",
        OS_REG_READ(ah, AR_PHY_RTT_TABLE_SW_INTF_B1));
    HDPRINTF(ah, HAL_DBG_FCS_RTT,
        "(RTT) BB_rtt_table_sw_intf_1_b1 @ 0x%08x\n",
        OS_REG_READ(ah, AR_PHY_RTT_TABLE_SW_INTF_1_B1));
#endif
}

#endif /* ATH_SUPPORT_RADIO_RETENTION */

#endif /* AH_SUPPORT_AR9300 */
