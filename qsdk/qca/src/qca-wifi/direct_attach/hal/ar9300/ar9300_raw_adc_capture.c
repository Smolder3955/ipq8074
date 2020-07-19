/*
 * Copyright (c) 2002-2009 Atheros Communications, Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */


#include "opt_ah.h"

#ifdef AH_SUPPORT_AR9300

#include "ah.h"
#include "ah_desc.h"
#include "ah_internal.h"

#include "ar9300/ar9300phy.h"
#include "ar9300/ar9300.h"
#include "ar9300/ar9300reg.h"
#include "ar9300/ar9300desc.h"

#if ATH_SUPPORT_RAW_ADC_CAPTURE

#define MAX_ADC_SAMPLES                 1024
#define AR9300_MAX_CHAINS               3
#define AR9300_RAW_ADC_SAMPLE_MIN        -512
#define AR9300_RAW_ADC_SAMPLE_MAX        511
#define RXTX_DESC_BUFFER_ADDRESS        0xE000
/* AR9300_MAX_ADC_POWER:
 * 100 times (10 * log10(2 * AR9300_RAW_ADC_SAMPLE_MAX ^ 2) = 57.1787)
 */
#define AR9300_MAX_ADC_POWER            5718

typedef struct adc_sample_t {
        int16_t i;
        int16_t q;
} ADC_SAMPLE;

/*
 * Recipe for 3 chains ADC capture:
 * 1. tstdac_out_sel=2'b01
 * 2. cf_tx_obs_sel=3'b111
 * 3. cf_tx_obs_mux_sel=2'b11
 * 4. cf_rx_obs_sel=5'b00000
 * 5. cf_bbb_obs_sel=4'b0001
 */

static void ar9300_setup_test_addac_mode(struct ath_hal *ah);
static void ar9300_force_agc_gain(struct ath_hal *ah);

/* Chainmask to numchains mapping */
static u_int8_t mask2chains[8] = {
    0 /* 000 */,
    1 /* 001 */,
    1 /* 010 */,
    2 /* 011 */,
    1 /* 100 */,
    2 /* 101 */,
    2 /* 110 */,
    3 /* 111 */};

static void
ar9300_shutdown_rx(struct ath_hal *ah)
{
    int wait;

#define AH_RX_STOP_TIMEOUT 100000   /* usec */
#define AH_TIME_QUANTUM       100   /* usec */

    /*ath_hal_printf(ah, "%s: called\n", __func__);*/

    /* (1) Set (RX_ABORT | RX_DIS) bits to reg MAC_DIAG_SW. */
    OS_REG_SET_BIT(ah, AR_DIAG_SW, AR_DIAG_RX_ABORT | AR_DIAG_RX_DIS);

    /*
     * (2) Poll (reg MAC_OBS_BUS_1[24:20] == 0) for 100ms
     * and if it doesn't become 0x0, print reg MAC_OBS_BUS_1.
     * Wait for Rx PCU state machine to become idle.
     */
    for (wait = AH_RX_STOP_TIMEOUT / AH_TIME_QUANTUM; wait != 0; wait--) {
        u_int32_t obs1 = OS_REG_READ(ah, AR_OBS_BUS_1);
        /* (MAC_PCU_OBS_BUS_1[24:20] == 0x0) - Check pcu_rxsm == IDLE */
        if ((obs1 & 0x01F00000) == 0) {
            break;
        }
        OS_DELAY(AH_TIME_QUANTUM);
    }

    /*
     * If bit 24:20 doesn't go to 0 within 100ms, print the value of
     * MAC_OBS_BUS_1 register on debug log.
     */
    if (wait == 0) {
        ath_hal_printf(ah,
            "%s: rx failed to go idle in %d us\n AR_OBS_BUS_1=0x%08x\n",
            __func__,
            AH_RX_STOP_TIMEOUT,
            OS_REG_READ(ah, AR_OBS_BUS_1));
    }

    /* (3) Set MACMISC reg = 0x8100 to configure debug bus */
    OS_REG_WRITE(ah, AR_MACMISC, 0x8100);

    /*
     * (4) Poll (AR_DMADBG_7 reg bits [11:8] == 0x0) for 100ms
     * wait for Rx DMA state machine to become idle
     */
    for (wait = AH_RX_STOP_TIMEOUT / AH_TIME_QUANTUM; wait != 0; wait--) {
        if ((OS_REG_READ(ah, AR_DMADBG_7) & AR_DMADBG_RX_STATE) == 0) {
            break;
        }
        OS_DELAY(AH_TIME_QUANTUM);
    }

    if (wait == 0) {
        ath_hal_printf(ah,
            "AR_DMADBG_7 reg [11:8] is not 0, instead AR_DMADBG_7 reg=0x%08x\n",
            OS_REG_READ(ah, AR_DMADBG_7));
        /* MAC_RXDP_SIZE register (0x70) */
        ath_hal_printf(ah, "AR_RXDP_SIZE=0x%08x\n",
            OS_REG_READ(ah, AR_RXDP_SIZE));
    }

    /* (5) Set RXD bit to reg MAC_CR */
    OS_REG_WRITE(ah, AR_CR, AR_CR_RXD);

    /* (6) Poll MAC_CR.RXE = 0x0 for 100ms or until RXE goes low */
    for (wait = AH_RX_STOP_TIMEOUT / AH_TIME_QUANTUM; wait != 0; wait--) {
        if ((OS_REG_READ(ah, AR_CR) & AR_CR_RXE) == 0) {
            break;
        }
        OS_DELAY(AH_TIME_QUANTUM);
    }

    /* (7) If (RXE_LP|RXE_HP) doesn't go low within 100ms */
    if (wait == 0) {
        ath_hal_printf(ah,
            "%s: RXE_LP of MAC_CR reg failed to go low in %d us\n",
            __func__, AH_RX_STOP_TIMEOUT);
    }

    /* (8) Clear reg MAC_PCU_RX_FILTER */
    ar9300_set_rx_filter(ah, 0);

#undef AH_RX_STOP_TIMEOUT
#undef AH_TIME_QUANTUM
}

void
ar9300_enable_test_addac_mode(struct ath_hal *ah)
{
    ar9300_shutdown_rx(ah);
}

void
ar9300_disable_test_addac_mode(struct ath_hal *ah)
{
    /* Clear the TST_ADDAC register to disable */
    OS_REG_WRITE(ah, AR_TST_ADDAC, 0);
    OS_DELAY(100);
}

static void
ar9300_setup_test_addac_mode(struct ath_hal *ah)
{
	uint32_t Reg_PhyTest;
	uint32_t Reg_PhyTestCtl;
	
#if AH_BYTE_ORDER == AH_BIG_ENDIAN
    /* byteswap Rx/Tx buffer to ensure correct layout */
    HDPRINTF(ah, HAL_DBG_RF_PARAM,
        "%s: big endian - set AR_CFG_SWTB/AR_CFG_SWRB\n", 
             __func__);
    OS_REG_RMW(ah, AR_CFG, AR_CFG_SWTB | AR_CFG_SWRB, 0);
#else
    /* Rx/Tx buffer should not be byteswaped */
    if (OS_REG_READ(ah, AR_CFG) & (AR_CFG_SWTB | AR_CFG_SWRB)) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, 
            "%s: **WARNING: little endian but AR_CFG_SWTB/AR_CFG_SWRB set!\n", 
            __func__);
    }
#endif
	if (AR_SREV_DRAGONFLY(ah)) {
		Reg_PhyTest =  AR_PHY_TEST_DRAGONFLY;
		Reg_PhyTestCtl = AR_PHY_TEST_CTL_STATUS_DRAGONFLY;
	} else {
		Reg_PhyTest =  AR_PHY_TEST;
		Reg_PhyTestCtl = AR_PHY_TEST_CTL_STATUS;
	}

    OS_REG_WRITE(ah, Reg_PhyTest, 0);
    OS_REG_WRITE(ah, Reg_PhyTestCtl, 0);

    /* cf_bbb_obs_sel=4'b0001 */
    OS_REG_RMW_FIELD(ah, Reg_PhyTest, AR_PHY_TEST_BBB_OBS_SEL, 0x1);

    /* cf_rx_obs_sel=5'b00000, this is the 5th bit */
    OS_REG_CLR_BIT(ah, Reg_PhyTest, AR_PHY_TEST_RX_OBS_SEL_BIT5);

    /* cf_tx_obs_sel=3'b111 */
    OS_REG_RMW_FIELD(
        ah, Reg_PhyTestCtl, AR_PHY_TEST_CTL_TX_OBS_SEL, 0x7);

    /* cf_tx_obs_mux_sel=2'b11 */
    OS_REG_RMW_FIELD(
        ah, Reg_PhyTestCtl, AR_PHY_TEST_CTL_TX_OBS_MUX_SEL, 0x3);

    /* enable TSTADC */
    OS_REG_SET_BIT(ah, Reg_PhyTestCtl, AR_PHY_TEST_CTL_TSTDAC_EN);

    /* cf_rx_obs_sel=5'b00000, these are the first 4 bits */
    OS_REG_RMW_FIELD(
        ah, Reg_PhyTestCtl, AR_PHY_TEST_CTL_RX_OBS_SEL, 0x0);

    /* tstdac_out_sel=2'b01 */
    OS_REG_RMW_FIELD(ah, Reg_PhyTest, AR_PHY_TEST_CHAIN_SEL, 0x1);
}

static void
ar9300_force_agc_gain(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    int i;
    static const struct {
        int rxtx1, rxtx2;
    } chn_reg[AR9300_MAX_CHAINS] = {
        {AR_PHY_65NM_CH0_RXTX1, AR_PHY_65NM_CH0_RXTX2},
        {AR_PHY_65NM_CH1_RXTX1, AR_PHY_65NM_CH1_RXTX2},
        {AR_PHY_65NM_CH2_RXTX1, AR_PHY_65NM_CH2_RXTX2}
    };
    /* 
     * for Osprey 1.0, force Rx gain through long shift (analog) interface
     * this works for Osprey 2.0 too
     * TODO: for Osprey 2.0, we can set gain via baseband registers 
     */
    for (i = 0; i < AR9300_MAX_CHAINS; i++) {
        if (ahp->ah_rx_chainmask & (1 << i)) {
            if (AH_PRIVATE(ah)->ah_curchan != NULL &&
                IS_CHAN_2GHZ(AH_PRIVATE(ah)->ah_curchan)) 
            {
                // For 2G band:
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx2, 
                    AR_PHY_RX1DB_BIQUAD_LONG_SHIFT, 0x1);  // 10db=3
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx2, 
                    AR_PHY_RX6DB_BIQUAD_LONG_SHIFT, 0x2);  // 10db=2
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx2, 
                    AR_PHY_LNAGAIN_LONG_SHIFT,      0x7);  // 10db=6
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx2, 
                    AR_PHY_MXRGAIN_LONG_SHIFT,      0x3);  // 10db=3
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx2, 
                    AR_PHY_VGAGAIN_LONG_SHIFT,      0x0);  // 10db=0
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx1, 
                    AR_PHY_SCFIR_GAIN_LONG_SHIFT,   0x1);  // 10db=1
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx1, 
                    AR_PHY_MANRXGAIN_LONG_SHIFT,    0x1);  // 10db=1
                /* force external LNA on to disable strong signal mechanism */
                OS_REG_RMW_FIELD(ah, AR_PHY_SWITCH_CHAIN(i), 
                    AR_PHY_SWITCH_TABLE_R0,  0x1); 
                OS_REG_RMW_FIELD(ah, AR_PHY_SWITCH_CHAIN(i), 
                    AR_PHY_SWITCH_TABLE_R1,  0x1); 
                OS_REG_RMW_FIELD(ah, AR_PHY_SWITCH_CHAIN(i), 
                    AR_PHY_SWITCH_TABLE_R12, 0x1); 
            } else {
                // For 5G band:
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx2, 
                    AR_PHY_RX1DB_BIQUAD_LONG_SHIFT, 0x2); // was 3=10/15db,
                                                          // 2=+1db
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx2, 
                    AR_PHY_RX6DB_BIQUAD_LONG_SHIFT, 0x2); // was 1
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx2, 
                    AR_PHY_LNAGAIN_LONG_SHIFT,      0x7);
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx2, 
                    AR_PHY_MXRGAIN_LONG_SHIFT,      0x3); // was 2=15db, 3=10db
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx2, 
                    AR_PHY_VGAGAIN_LONG_SHIFT,      0x6);
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx1, 
                    AR_PHY_SCFIR_GAIN_LONG_SHIFT,   0x1);
                OS_REG_RMW_FIELD(ah, chn_reg[i].rxtx1, 
                    AR_PHY_MANRXGAIN_LONG_SHIFT,    0x1);
                /* force external LNA on to disable strong signal mechanism */
                OS_REG_RMW_FIELD(ah, AR_PHY_SWITCH_CHAIN(i), 
                    AR_PHY_SWITCH_TABLE_R0,  0x0); 
                OS_REG_RMW_FIELD(ah, AR_PHY_SWITCH_CHAIN(i), 
                    AR_PHY_SWITCH_TABLE_R1,  0x0); 
                OS_REG_RMW_FIELD(ah, AR_PHY_SWITCH_CHAIN(i), 
                    AR_PHY_SWITCH_TABLE_R12, 0x0); 
            }
        }
    }
}

void
ar9300_begin_adc_capture(struct ath_hal *ah, int auto_agc_gain)
{
    ar9300_setup_test_addac_mode(ah);

    /* First clear TST_ADDAC register */
    ar9300_disable_test_addac_mode(ah);

    if (!auto_agc_gain) {
        /* Force max AGC gain */
        ar9300_force_agc_gain(ah);
    }

    /* Now enable TST mode */
    OS_REG_SET_BIT(ah, AR_TST_ADDAC, AR_TST_ADDAC_TST_MODE);
    OS_REG_SET_BIT(ah, AR_TST_ADDAC, AR_TST_ADDAC_TST_LOOP_ENA);
    OS_DELAY(10);

    /* Begin capture and wait 1ms before reading the capture data*/
    OS_REG_SET_BIT(ah, AR_TST_ADDAC, AR_TST_ADDAC_BEGIN_CAPTURE);
    OS_DELAY(1000);
}

static inline int16_t convert_to_signed(int sample)
{
    if (sample > AR9300_RAW_ADC_SAMPLE_MAX) {
        sample -= (AR9300_RAW_ADC_SAMPLE_MAX + 1) << 1;
    }
    return sample;
}

HAL_STATUS
ar9300_retrieve_capture_data(
    struct ath_hal *ah,
    u_int16_t chain_mask,
    int disable_dc_filter,
    void *sample_buf,
    u_int32_t* num_samples)
{
    u_int32_t val, i;
    int descr_address, num_chains;
    ADC_SAMPLE *sample;

    num_chains = mask2chains[chain_mask];
    if (*num_samples < (MAX_ADC_SAMPLES * num_chains)) {
        /*
         * supplied buffer is too small -
         * update to inform caller of required size
         */
        *num_samples = MAX_ADC_SAMPLES * num_chains;
        return HAL_ENOMEM;
    }

    /* Make sure we are reading TXBUF */
    OS_REG_CLR_BIT(ah, AR_RXFIFO_CFG, AR_RXFIFO_CFG_REG_RD_ENA);

    sample = (ADC_SAMPLE*)sample_buf;
    descr_address = RXTX_DESC_BUFFER_ADDRESS;
    for (i = 0;
         i < MAX_ADC_SAMPLES;
         i++, descr_address += 4, sample += num_chains)
    {
        val = OS_REG_READ(ah, descr_address);
        /* Get bits [29:20]from TXBUF - chain#0 I */
        sample[0].i = convert_to_signed((val >> 20) & 0x3ff);
        /* Get bits [19:10] from TXBUF - chain#0 Q */
        sample[0].q = convert_to_signed((val >> 10) & 0x3ff);
        if (num_chains >= 2) {
            /* Get bits [9:0] from TXBUF - chain#1 I */
            sample[1].i = convert_to_signed(val & 0x3ff);
        }
    }

    if (num_chains >= 2) {
        /* Make sure we are reading RXBUF */
        OS_REG_SET_BIT(ah, AR_RXFIFO_CFG, AR_RXFIFO_CFG_REG_RD_ENA);

        sample = (ADC_SAMPLE*)sample_buf;
        descr_address = RXTX_DESC_BUFFER_ADDRESS;
        for (i = 0;
             i < MAX_ADC_SAMPLES;
             i++, descr_address += 4, sample += num_chains)
        {
            val = OS_REG_READ(ah, descr_address);
            /* Get bits [27:18] from RXBUF - chain #1 Q */
            sample[1].q = convert_to_signed((val >> 20) & 0x3ff);
            if (num_chains >= 3) {
                /* Get bits [19:10] from RXBUF - chain #2 I */
                sample[2].i = convert_to_signed((val >> 10) & 0x3ff);
                /* Get bits [9:0] from RXBUF - chain#2 Q */
                sample[2].q = convert_to_signed(val & 0x3ff);
            }
        }
    }

    /* inform caller of returned sample count */
    *num_samples = MAX_ADC_SAMPLES * num_chains;

#ifdef TEST_DATA
    sample = (ADC_SAMPLE*)sample_buf;
    for (i = 0; i < *num_samples; i++) {
        sample[i].i = i & 0x3ff;
        sample[i].q = (~i) & 0x3ff;
    }
    ath_hal_printf(AH_NULL, "%s: faked %d samples with %d chains\n", 
                   __func__, *num_samples, num_chains);
#endif

    return HAL_OK;
}

typedef struct gain_cal_entry_t {
    int freq;                          /* Mhz */
    int16_t offset[AR9300_MAX_CHAINS]; /* 100 times units (0.01 db) */
} GAIN_CAL_TABLE;

static const GAIN_CAL_TABLE gain_cal_data_osprey[] = {
          /* freq,  chain0_offset,
					chain1_offset,
                    chain2_offset */
/*ch  1*/  { 2412, {  638,   // 500+138:  apladj 10/8: -250, was: 750=13.3-5.8
                      1058,  // 880+178:  apladj 10/8: -130, was: 1210=16.3-4.2
                      938}}, // 730+208:  apladj 10/8: -100, was: 1130=16.3-5.0
/*ch  6*/  { 2437, {  662,   // 420+242:  apladj 10/8: -250, was: 670=13.1-6.4
                      1134,  // 860+274:  apladj 10/8: -230, was: 1140=16.1-4.7
                      906}}, // 700+206:  apladj 10/8: -200, was: 1050=14.1-3.6
/*ch 11*/  { 2462, {  690,   // 440+250:  apladj 10/8: -250, was: 720 =12.3-5.1
                     1134,   // 860+274:  apladj 10/8: -150, was: 1160=15.3-3.7
                      934}}, // 710+224:  apladj 10/8: -100, was: 1110=17.3-6.2
/*ch 36*/  { 5180, { 1424,   // 1260+164: apladj 10/8: -350, was: 2480=25.3-0.5
                     1090,   // 680+410:  apladj 10/8: -500, was: 1680=19.3-2.5
                     1274}}, // 1070+204: apladj 10/8: -450, was: 2170=25.3-3.6
/*ch 44*/  { 5220, { 1376,   // 1250+126: apladj 10/8: -400, was: 2450=25.0-0.5
                     1026,   // 580+446:  apladj 10/8: -700, was: 1580=19.0-3.2
                     1296}}, // 1000+296: apladj 10/8: -550, was: 2050=23.0-2.5
/*ch 64*/  { 5320, { 1258,   // 1260-002  apladj 10/8: -200, was: 2510=23.9+1.2
                      910,   // 510+400   apladj 10/8: -700, was: 1510=16.9-1.8
                     1096}}, // 850+246   apladj 10/8: -600, was: 1850=19.9-1.4
           { 5400, { 2220,   // 22.2               
                     1520,   // 15.2               
                     1620}}, // 16.2               
/*ch100*/  { 5500, { 1220,   // 1230-010  apladj 10/8: -350, was: 2280=18.9+3.9
                      944,   // 550+394   apladj 10/8: -750, was: 1450=12.9+1.6
                      838}}, // 410+428   apladj 10/8: -700, was: 1510=13.9+1.2
/*ch120*/  { 5600, { 1300,   // 1160+140  apladj 10/8: -400, was: 2060=15.8+4.8
                      974,   // 720+254   apladj 10/8: -700, was: 1420=10.8+3.4
                      882}}, // 580+302   apladj 10/8: -600, was: 1580=14.8+1.0
/*ch140*/  { 5700, { 1514,   // 1360+154  apladj 10/8: -350, was: 1910=13.6+5.5
                     1064,   // 910+154   apladj 10/8: -500, was: 1360=8.6+5.0
                      876}}, // 870+006   apladj 10/8: -450, was: 1520=9.6+5.6
/*ch149*/  { 5745, { 1564,   // 1410+154  apladj 10/8: -300, was: 1910=13.6+5.5
                     1164,   // 1010+154  apladj 10/8: -400, was: 1360=8.6+5.0
                      926}}, // 920+006   apladj 10/8: -400, was: 1520=9.6+5.6
/*ch157*/  { 5785, { 1648,   // 1520+128  apladj 10/8: -400, was: 1920=12.7+6.5
                     1242,   // 1110+132  apladj 10/8: -400, was: 1460=8.7+5.9
                      988}}, // 1030-042  apladj 10/8: -400, was: 1530=11.7+3.6
/*ch165*/  { 5825, { 1698,   // 1570+128  apladj 10/8: -350, was: 1920=12.7+6.5
                     1292,   // 1160+132  apladj 10/8: -350, was: 1460=8.7+5.9,
                     1038}}, // 1080-042  apladj 10/8: -350, was: 1530=11.7+3.6
           {    0, {}     }	
};

static const GAIN_CAL_TABLE gain_cal_data_wasp[] = {
          /* freq,  chain0_offset,
					chain1_offset,
                    chain2_offset */
/*ch  1*/  { 2412, {  638,   
                      1058,  
                      938}}, 
/*ch  6*/  { 2437, {  662,   
                      1134,  
                      906}}, 
/*ch 11*/  { 2462, {  690,   
                     1134,   
                      934}}, 
/*ch 36*/  { 5180, { 1424,   
                     1090,   
                     1274}}, 
/*ch 44*/  { 5220, { 1376,   
                     1026,   
                     1296}}, 
/*ch 64*/  { 5320, { 1258,   
                      910,   
                     1096}}, 
           { 5400, { 2220,   
                     1520,   
                     1620}}, 
/*ch100*/  { 5500, { 1220,   
                      944,   
                      838}}, 
/*ch120*/  { 5600, { 1300,   
                      974,   
                      882}}, 
/*ch140*/  { 5700, { 1514,   
                     1064,   
                      876}}, 
/*ch149*/  { 5745, { 1564,   
                     1164,   
                      926}}, 
/*ch157*/  { 5785, { 1648,   
                     1242,   
                      988}}, 
/*ch165*/  { 5825, { 1698,   
                     1292,   
                     1038}}, 
           {    0, {}     }	
};

static int16_t gain_cal_offset(struct ath_hal *ah, int freq_mhz, int chain)
{
    int i;
    const GAIN_CAL_TABLE *table = NULL;

    if (AR_SREV_OSPREY_22(ah)) {
        table = gain_cal_data_osprey;
    } else if (AR_SREV_AR9580(ah)) {
		/* Temporarily using Osprey's gain cal table for Peacock
		     Replace with Peacock's gain cal table when available*/
		table = gain_cal_data_osprey;
    } else if (AR_SREV_WASP(ah)) {
        table = gain_cal_data_wasp;
    }

    if (!table) {
        ath_hal_printf(ah,
               "%s: **Warning: device %d.%d: no cal offset table found\n",
               __func__, (AH_PRIVATE(ah))->ah_mac_version, 
               (AH_PRIVATE(ah))->ah_mac_rev);
        return 0;
    }

    for (i = 0; table[i].freq != 0; i++) {
        if (table[i + 0].freq == freq_mhz ||
            table[i + 1].freq > freq_mhz ||
            table[i + 1].freq == 0) {
            return(table[i].offset[chain]);
        }
    }

    ath_hal_printf(ah,
        "%s: **Warning: device %d.%d: "
        "no cal offset found for freq %d chain %d\n",
        __func__, (AH_PRIVATE(ah))->ah_mac_version, 
        (AH_PRIVATE(ah))->ah_mac_rev, freq_mhz, chain);
    return 0;
}

HAL_STATUS
ar9300_get_min_agc_gain(
    struct ath_hal *ah,
    int freq_mhz,
    int32_t *chain_gain,
    int num_chain_gain)
{
    int ch;
    for (ch = 0; ch < num_chain_gain && ch < AR9300_MAX_CHAINS; ch++) {
        /*
         * cal offset 100 times units (0.01 db)
         * round to nearest and return (gain is -ve)
         */
        chain_gain[ch] = -(gain_cal_offset(ah, freq_mhz, ch) + 50) / 100;
    }
    return HAL_OK;
}


/* values are db*100 */
#define AR9300_GI_2GHZ  7100  /* was 7600: -10db=6600, -5db=7100 */
#define AR9300_GI_5GHZ  8600  /* was 9100: -15db=7600, -10db=8100, -5db=8600 */

HAL_STATUS
ar9300_calc_adc_ref_powers(
    struct ath_hal *ah,
    int freq_mhz,
    int16_t *sample_min,
    int16_t *sample_max,
    int32_t *chain_ref_pwr,
    int num_chain_ref_pwr)
{
    int32_t ref_power;
    int ch;
    const int32_t GI = (freq_mhz < 5000) ? AR9300_GI_2GHZ : AR9300_GI_5GHZ;
    for (ch = 0; ch < num_chain_ref_pwr && ch < AR9300_MAX_CHAINS; ch++) {
        /*
         * calculation is done in 100 times units (0.01 db)
         */
        ref_power =
            -GI + gain_cal_offset(ah, freq_mhz, ch) - AR9300_MAX_ADC_POWER;
        chain_ref_pwr[ch] = (ref_power - 50) / 100;
        /*
        ath_hal_printf(AH_NULL, "Abs power at freq %d for chain %d: %3.2f\n",
            freq_mhz, ch, chain_ref_pwr[ch]);
         */
    }
    *sample_min = AR9300_RAW_ADC_SAMPLE_MIN;
    *sample_max = AR9300_RAW_ADC_SAMPLE_MAX;
    return HAL_OK;
}

#endif

#else
/*
 * Raw capture mode not enabled - insert dummy code to keep the compiler happy
 */
typedef int ar9300_dummy_adc_capture;

#endif /* AH_SUPPORT_AR9300*/
