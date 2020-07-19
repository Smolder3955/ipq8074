/*
* Copyright (c) 2011, 2018 Qualcomm Innovation Center, Inc.
* All Rights Reserved.
* Confidential and Proprietary - Qualcomm Innovation Center, Inc.
*
*/

/*
 * Copyright (c) 2002-2009 Atheros Communications, Inc. 
 * All Rights Reserved.
 * 
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */


#include "opt_ah.h"

#ifdef AH_SUPPORT_AR5416

#include "ah.h"

#if ATH_SUPPORT_RAW_ADC_CAPTURE

#include "ah_desc.h"
#include "ah_internal.h"

#include "ar5416/ar5416phy.h"
#include "ar5416/ar5416.h"
#include "ar5416/ar5416reg.h"
#include "ar5416/ar5416desc.h"


#define MAX_ADC_SAMPLES                 1024
#define AR5416_MAX_CHAINS               3
#define AR5416_RAW_ADC_SAMPLE_MIN		-256
#define AR5416_RAW_ADC_SAMPLE_MAX		255
#define RXTX_DESC_BUFFER_ADDRESS        0xE000
#define AR5416_MAX_ADC_POWER            5114    // 100 times (10 * log10(2 * AR5416_RAW_ADC_SAMPLE_MAX ^ 2) = 51.1411)

typedef struct adc_sample_t {
    int16_t i;
    int16_t q;
} ADC_SAMPLE;

void ar5416SetupTestAddacMode(struct ath_hal *ah);
void ar5416ShutdownRx(struct ath_hal *ah);

/* Chainmask to numchains mapping */
static u_int8_t mask2chains[8] = { 0 /* 000 */, 1 /* 001 */, 1 /* 010 */, 2 /* 011 */, 1 /* 100 */,  2 /* 101 */, 2 /* 110 */, 3 /* 111 */ };

void
ar5416ShutdownRx(struct ath_hal *ah)
{
    ar5416SetRxAbort(ah, 1);
    if(ar5416StopDmaReceive(ah, 0) == AH_FALSE) {
        ath_hal_printf(ah, "%s: ar5416StopDmaReceive() failed\n", __func__);
    }
    ar5416SetRxFilter(ah, 0);
}

void
ar5416EnableTestAddacMode(struct ath_hal *ah)
{
    ar5416ShutdownRx(ah);
}

void
ar5416DisableTestAddacMode(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, 0x9848, OS_REG_READ(ah, 0x9848) & ~(0x1 << 21)); // disable bb_ch0_gain_force
    OS_REG_WRITE(ah, 0xa848, OS_REG_READ(ah, 0xa848) & ~(0x1 << 21)); // disable bb_ch1_gain_force

    /* Clear the TST_ADDAC register to disable */
    OS_REG_WRITE(ah, AR_TST_ADDAC, 0);
    OS_DELAY(100);
}


void
ar5416SetupTestAddacMode(struct ath_hal *ah)
{
    u_int32_t val;

    // see: //depot/bringup/ar5k/test_merlin/merlin2/adc_capture.scr

    // reg 2: Test control/status (0x9808)
    // bit  8: (1) driver to tstadc bus enable
    // bit 10: (0) chain 0
    // bits [13:10] select point in rx path to observe, along with rx_obs_sel_5th_bit in reg#0
    //      5'd0: chn_tstadc_i_0; (chain 0, select w/ bbb_obs_sel) chn_tstadc_q_0; (ms=0)
    // bit 0 : (1) driver to tstdac bus enable
    // bit (2-4) 111 select which points in tx path to observe
    // bit (5-6) 11 select which sub-point in tx path to observe (3=> ADC output).
    // bits [18:16] 000 - agc_obs_sel - select which test points in agc to observe, together with agc_obs_sel_3(bit 18 reg#0) and agc_obs_sel_4(bit 24 reg#0)
    val = (0x1 << 8) | (0 << 10) | (0x1 << 0) | (0x7 << 2) | (0x3 << 5);
    OS_REG_WRITE(ah, AR_PHY_TESTCTRL, val);

    // reg 0: Test control 2 (0x9800)
    // bit 19-22: bbb_obs_sel - select test points in bbb or chn rx: (1) observe fir_out_i
    // bit 31-30 select which chain to drive tstdac_out (0x1 => chain 1) (0: chain0, 1: chain1, 2: chain2)
    // bit 18: should be zero: agc_obs_sel_3 - select which test points in agc to observe, together with agc_obs_sel_4(bit 24) and agc_obs_sel(in reg#2)
    // bit 24: should be zero: agc_obs_sel_4 - select which test points in agc to observe, together with agc_obs_sel_3(bit 18) and agc_obs_sel(in reg#2)
    // bit 23: should be zero: the 5th bit of 'rx_obs_sel' (ms=0)
    val = OS_REG_READ(ah, AR_PHY_TEST);
    val = (val & 0x3E03FFFF) | (0x1 << 19) | (0x1 << 30);
    OS_REG_WRITE(ah, AR_PHY_TEST, val); //This selects chain 1 signals
    val &= 0x3FFFFFFF; // bit 31-30 select which chain to drive tstdac_out
    OS_REG_WRITE(ah, AR_PHY_TEST, val); //This selects chain 0 signals
    OS_DELAY(10);

    // clear TST_ADDAC register
    OS_REG_WRITE(ah, AR_TST_ADDAC, 0);
    OS_DELAY(5);

    val = OS_REG_READ(ah, AR_TST_ADDAC);
    val |= (AR_TST_ADDAC_TST_LOOP_ENA);
    OS_REG_WRITE(ah, AR_TST_ADDAC, val);
    OS_DELAY(10);
}

#define FORCED_GAIN_2G 168 // was 198 (198-30 = -15db)
#define FORCED_GAIN_5G 174 // was 202 (204-30 = -15db)
static void
ar5416ForceAGCGain(struct ath_hal *ah)
{
    u_int32_t forced_gain;

    // Use the following code to force Rx gain(through BB interface) in Merlin. 
    // For 2G, set $FORCED_GAIN to 198. For 5G, set $FORCED_GAIN to 202.
    if (AH_PRIVATE(ah)->ah_curchan != NULL &&
        IS_CHAN_2GHZ(AH_PRIVATE(ah)->ah_curchan)) {
        forced_gain = FORCED_GAIN_2G;
    } else {
        forced_gain = FORCED_GAIN_5G;
    }

    OS_REG_WRITE(ah, 0x9848, OS_REG_READ(ah, 0x9848) | (0x1 << 21)); // bb_ch0_gain_force
    OS_REG_WRITE(ah, 0xa848, OS_REG_READ(ah, 0xa848) | (0x1 << 21)); // bb_ch1_gain_force
    OS_REG_WRITE(ah, 0x984c, (OS_REG_READ(ah, 0x984c) & 0x0001ffff) | (forced_gain << 17));
    OS_REG_WRITE(ah, 0xa84c, (OS_REG_READ(ah, 0xa84c) & 0x0001ffff) | (forced_gain << 17));
}

void
ar5416BeginAdcCapture(struct ath_hal *ah, int auto_agc_gain)
{
    ar5416SetupTestAddacMode(ah);

	if (!auto_agc_gain) {
		/* Force max AGC gain */
		ar5416ForceAGCGain(ah);
	}

    /* Begin capture and wait 1ms before reading the capture data*/
    OS_REG_SET_BIT(ah, AR_TST_ADDAC, AR_TST_ADDAC_BEGIN_CAPTURE);
    OS_DELAY(1000);
}

static inline int16_t convert_to_signed(int sample)
{
    if (sample > AR5416_RAW_ADC_SAMPLE_MAX)
        sample -= (AR5416_RAW_ADC_SAMPLE_MAX+1) << 1;
    return sample;
}

HAL_STATUS
ar5416RetrieveCaptureData(struct ath_hal *ah, u_int16_t chain_mask, int disable_dc_filter, void *sample_buf, u_int32_t* num_samples)
{
    u_int32_t val, i, ch;
    int descr_address, num_chains;
    ADC_SAMPLE* sample;
    int32_t i_sum[AR5416_MAX_CHAINS] = {0};
    int32_t q_sum[AR5416_MAX_CHAINS] = {0};

    num_chains = mask2chains[chain_mask];
    if (*num_samples < (MAX_ADC_SAMPLES * num_chains)) {
        /* supplied buffer is too small - update to inform caller of required size */
        *num_samples = MAX_ADC_SAMPLES * num_chains;
        return HAL_ENOMEM;
    }

    /* Make sure we are reading TXBUF */
    OS_REG_CLR_BIT(ah, AR_RXFIFO_CFG, AR_RXFIFO_CFG_REG_RD_ENA);

    sample = (ADC_SAMPLE*)sample_buf;
    descr_address = RXTX_DESC_BUFFER_ADDRESS;
    for (i=0; i < MAX_ADC_SAMPLES; i++, descr_address+=4, sample+=num_chains) {
        val = OS_REG_READ(ah, descr_address);
        /* Get bits [27:18] from TXBUF - chain#0 I */
        i_sum[0] += sample[0].i = convert_to_signed((val >> 18) & 0x1ff);
        /* Get bits [17:9] from TXBUF - chain#0 Q */
        q_sum[0] += sample[0].q = convert_to_signed((val >> 9) & 0x1ff);
        if (num_chains >= 2) {
            /* Get bits [8:0] from TXBUF - chain#1 I */
            i_sum[1] += sample[1].i = convert_to_signed(val & 0x1ff);
        }
    }

    if (num_chains >= 2) {
        /* Make sure we are reading RXBUF */
        OS_REG_SET_BIT(ah, AR_RXFIFO_CFG, AR_RXFIFO_CFG_REG_RD_ENA);
    
        sample = (ADC_SAMPLE*)sample_buf;
        descr_address = RXTX_DESC_BUFFER_ADDRESS;
        for (i=0; i < MAX_ADC_SAMPLES; i++, descr_address+=4, sample+=num_chains) {
            val = OS_REG_READ(ah, descr_address);
            /* Get bits [27:18] from RXBUF - chain #1 Q */
            q_sum[1] += sample[1].q = convert_to_signed((val >> 18) & 0x1ff);
            if (num_chains >= 3) {
                /* Get bits [17:9] from RXBUF - chain #2 I */
                i_sum[2] += sample[2].i = convert_to_signed((val >> 9) & 0x1ff);
                /* Get bits [8:0] from RXBUF - chain#2 Q */
                q_sum[2] += sample[2].q = convert_to_signed(val & 0x1ff);
            }
        }
    }

    if (!disable_dc_filter) {
        //ath_hal_printf(ah, "%s: running DC filter over %d chains now...\n", __func__, num_chains);
        for (ch=0; ch < num_chains; ch++) {
            i_sum[ch] /= MAX_ADC_SAMPLES;
            q_sum[ch] /= MAX_ADC_SAMPLES;
            sample = (ADC_SAMPLE*)sample_buf;
            for (i=0; i < MAX_ADC_SAMPLES; i++, sample++) {
                sample[ch].i -= i_sum[ch];
                sample[ch].q -= q_sum[ch];
            }
        }
    }

    /* inform caller of returned sample count */
    *num_samples = MAX_ADC_SAMPLES * num_chains;
    return HAL_OK;
}

static const struct {
    int freq;                          /* Mhz */
    int16_t offset[AR5416_MAX_CHAINS]; /* 100 times units (0.01 db) */
} gain_cal_data_merlin[] = {
          // freq  chain0_offset chain1_offset chain2_offset
/*ch  1*/  { 2412, {  420,   
                      104,   
                        0}}, 
/*ch  6*/  { 2437, {  399,   
                      163,   
                        0}}, 
/*ch 11*/  { 2462, {  495,   
                      192,   
                        0}}, 
/*ch 36*/  { 5180, { 2306,   
                     2300,   
                        0}}, 
/*ch 44*/  { 5220, { 2208,   
                     2215,   
                        0}}, 
/*ch 64*/  { 5320, { 2029,   
                     1968,   
                        0}}, 
/*ch100*/  { 5500, { 1971,   
                     1593,   
                        0}}, 
/*ch120*/  { 5600, { 1977,   
                     1622,   
                        0}}, 
/*ch140*/  { 5700, { 1811,   
                     1622,   
                        0}}, 
/*ch157*/  { 5785, { 1951,   
                     1961,   
                        0}}, 
/*ch165*/  { 5825, { 1976,   
                     2067,   
                        0}}, 
	       {    0, {}     }	
};

static int16_t gainCalOffset(int freq_mhz, int chain)
{
    int i;
    for(i = 0; gain_cal_data_merlin[i].freq != 0; i++) {
        if (gain_cal_data_merlin[i].freq == freq_mhz ||
            gain_cal_data_merlin[i+1].freq > freq_mhz ||
			gain_cal_data_merlin[i+1].freq == 0) {
            return(gain_cal_data_merlin[i].offset[chain]);
        }
    }
    ath_hal_printf(AH_NULL, "%s: **Warning: no cal offset found for freq %d chain %d\n", __func__, freq_mhz, chain);
    return 0;
}

HAL_STATUS
ar5416GetMinAGCGain(struct ath_hal *ah, int freq_mhz, int32_t *chain_gain, int num_chain_gain)
{
#ifdef not_yet
    int ch;
    for(ch = 0; ch < num_chain_gain && ch < AR5416_MAX_CHAINS; ch++) {
        /*
         * cal offset 100 times units (0.01 db)
         * round to nearest and return (gain is -ve)
         */
        chain_gain[ch] = -(gainCalOffset(freq_mhz, ch)+50)/100;
    }
    return HAL_OK;
#else
    return HAL_ENOTSUPP;
#endif
}


// values are db*100
#define AR5416_GI_2GHZ  7600
#define AR5416_GI_5GHZ  9100

HAL_STATUS
ar5416CalculateADCRefPowers(struct ath_hal *ah, int freq_mhz, int16_t *sample_min, int16_t *sample_max, int32_t *chain_ref_pwr, int num_chain_ref_pwr)
{
    int32_t ref_power;
    int ch;
    const int32_t GI = (freq_mhz < 5000) ? AR5416_GI_2GHZ : AR5416_GI_5GHZ;
    for(ch = 0; ch < num_chain_ref_pwr && ch < AR5416_MAX_CHAINS; ch++) {
        /*
         * calculation is done in 100 times units (0.01 db)
         */
        ref_power = -GI + gainCalOffset(freq_mhz, ch) - AR5416_MAX_ADC_POWER;
        chain_ref_pwr[ch] = (ref_power-50)/100; 
        //ath_hal_printf(AH_NULL, "Abs power at freq %d for chain %d: %3.2f\n", freq_mhz, ch, chain_ref_pwr[ch]);
    }
    *sample_min = AR5416_RAW_ADC_SAMPLE_MIN;
    *sample_max = AR5416_RAW_ADC_SAMPLE_MAX;
    return HAL_OK;
}

#endif

#else
/* Raw capture mode not enabled - insert dummy code to keep the compiler happy */
typedef int ar5416_dummy_adc_capture;

#endif /* AH_SUPPORT_AR5416*/
