
/*
 * Copyright (c) 2008-2010, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 */

#ifdef ART_BUILD
#include <math.h> /* pow() */
#endif /* ART_BUILD */

#include "opt_ah.h"

#ifdef AH_SUPPORT_AR9300

#include "ah.h"
#include "ah_internal.h"
#include "ah_devid.h"
#include "ah_desc.h"

#include "ar9300.h"
#include "ar9300reg.h"
#include "ar9300phy.h"
#include "ar9300desc.h"

#define FIX_NOISE_FLOOR     1

#if ATH_SUPPORT_WIFIPOS
#define NEXT_TBTT_NOW       10
#endif

/* Additional Time delay to wait after activiting the Base band */
#define BASE_ACTIVATE_DELAY         100     /* usec */
#define RTC_PLL_SETTLE_DELAY        100     /* usec */
#define COEF_SCALE_S                24
#define HT40_CHANNEL_CENTER_SHIFT   10      /* MHz      */

#define DELPT 32

/* EV 121277
 * We are reading the NF values before we start the NF operation, because
 * of that we are getting very high values like -45.
 * This triggers the CW_INT detected and EACS module triggers the channel change
 * chip_reset_done value is used to fix this issue.
 * chip_reset_flag is set during the RTC reset.
 * chip_reset_flag is cleared during the starting NF operation.
 * if flag is set we will clear the flag and will not read the NF values.
 */

extern  bool ar9300_reset_tx_queue(struct ath_hal *ah, u_int q);
extern  u_int32_t ar9300_num_tx_pending(struct ath_hal *ah, u_int q);


#define MAX_MEASUREMENT 8
#define MAXIQCAL 3
struct coeff_t {
    int32_t mag_coeff[AR9300_MAX_CHAINS][MAX_MEASUREMENT][MAXIQCAL];
    int32_t phs_coeff[AR9300_MAX_CHAINS][MAX_MEASUREMENT][MAXIQCAL];
    int32_t iqc_coeff[2];
    int last_nmeasurement;
    bool last_cal;
};

static bool ar9300_tx_iq_cal_hw_run(struct ath_hal *ah);
static void ar9300_tx_iq_cal_post_proc(struct ath_hal *ah,HAL_CHANNEL_INTERNAL *ichan,
       int iqcal_idx, int max_iqcal, bool is_cal_reusable, bool apply_last_corr);
static void ar9300_tx_iq_cal_outlier_detection(struct ath_hal *ah,HAL_CHANNEL_INTERNAL *ichan,
       u_int32_t num_chains, struct coeff_t *coeff, bool is_cal_reusable);
#if ATH_SUPPORT_CAL_REUSE
static void ar9300_tx_iq_cal_apply(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichan);
#endif

#ifdef ART_BUILD
extern int MyRegisterRead(unsigned int address, unsigned int *value);
extern int MyRegisterWrite(unsigned int address, unsigned int value);
void ar9300_init_otp_Jupiter(struct ath_hal *ah);
#ifdef AH_SUPPORT_HORNET
void ar9300_init_otp_hornet(struct ath_hal *ah);
#endif
#endif

static inline void ar9300_prog_ini(struct ath_hal *ah, struct ar9300_ini_array *ini_arr, int column);
static inline void ar9300_set_rf_mode(struct ath_hal *ah, HAL_CHANNEL *chan);
static inline bool ar9300_init_cal(struct ath_hal *ah, HAL_CHANNEL *chan, bool skip_if_none, bool apply_last_corr);
static inline void ar9300_init_user_settings(struct ath_hal *ah);

#ifdef HOST_OFFLOAD
/*
 * For usb offload solution, some USB registers must be tuned
 * to gain better stability/performance but these registers
 * might be changed while doing wlan reset so do this here
 */
#define WAR_USB_DISABLE_PLL_LOCK_DETECT(__ah) \
do { \
    if (AR_SREV_HORNET(__ah) || AR_SREV_WASP(__ah)) { \
        volatile u_int32_t *usb_ctrl_r1 = (u_int32_t *) 0xb8116c84; \
        volatile u_int32_t *usb_ctrl_r2 = (u_int32_t *) 0xb8116c88; \
        *usb_ctrl_r1 = (*usb_ctrl_r1 & 0xffefffff); \
        *usb_ctrl_r2 = (*usb_ctrl_r2 & 0xfc1fffff) | (1 << 21) | (3 << 22); \
    } \
} while (0)
#else
#define WAR_USB_DISABLE_PLL_LOCK_DETECT(__ah)
#endif

static inline void
ar9300_attach_hw_platform(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    ahp->ah_hwp = HAL_TRUE_CHIP;
    return;
}

/* Adjust various register settings based on half/quarter rate clock setting.
 * This includes: +USEC, TX/RX latency,
 *                + IFS params: slot, eifs, misc etc.
 * SIFS stays the same.
 */
static void
ar9300_set_ifs_timing(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t tx_lat, rx_lat, usec, slot, regval, eifs;

    regval = OS_REG_READ(ah, AR_USEC);
    regval &= ~(AR_USEC_RX_LATENCY | AR_USEC_TX_LATENCY | AR_USEC_USEC);
    if (IS_CHAN_HALF_RATE(chan)) { /* half rates */
        slot = ar9300_mac_to_clks(ah, AR_SLOT_HALF);
        eifs = ar9300_mac_to_clks(ah, AR_EIFS_HALF);
        if (IS_5GHZ_FAST_CLOCK_EN(ah, chan)) { /* fast clock */
            rx_lat = SM(AR_RX_LATENCY_HALF_FAST_CLOCK, AR_USEC_RX_LATENCY);
            tx_lat = SM(AR_TX_LATENCY_HALF_FAST_CLOCK, AR_USEC_TX_LATENCY);
            usec = SM(AR_USEC_HALF_FAST_CLOCK, AR_USEC_USEC);
        } else {
            rx_lat = SM(AR_RX_LATENCY_HALF, AR_USEC_RX_LATENCY);
            tx_lat = SM(AR_TX_LATENCY_HALF, AR_USEC_TX_LATENCY);
            usec = SM(AR_USEC_HALF, AR_USEC_USEC);
        }
    } else { /* quarter rate */
        slot = ar9300_mac_to_clks(ah, AR_SLOT_QUARTER);
        eifs = ar9300_mac_to_clks(ah, AR_EIFS_QUARTER);
        if (IS_5GHZ_FAST_CLOCK_EN(ah, chan)) { /* fast clock */
            rx_lat = SM(AR_RX_LATENCY_QUARTER_FAST_CLOCK, AR_USEC_RX_LATENCY);
            tx_lat = SM(AR_TX_LATENCY_QUARTER_FAST_CLOCK, AR_USEC_TX_LATENCY);
            usec = SM(AR_USEC_QUARTER_FAST_CLOCK, AR_USEC_USEC);
        } else {
            rx_lat = SM(AR_RX_LATENCY_QUARTER, AR_USEC_RX_LATENCY);
            tx_lat = SM(AR_TX_LATENCY_QUARTER, AR_USEC_TX_LATENCY);
            usec = SM(AR_USEC_QUARTER, AR_USEC_USEC);
        }
    }

    OS_REG_WRITE(ah, AR_USEC, (usec | regval | tx_lat | rx_lat));
    OS_REG_WRITE(ah, AR_D_GBL_IFS_SLOT, slot);
    OS_REG_WRITE(ah, AR_D_GBL_IFS_EIFS, eifs);
}


/*
 * This inline function configures the chip either
 * to encrypt/decrypt management frames or pass thru
 */
static inline void
ar9300_init_mfp(struct ath_hal * ah)
{
    u_int32_t   mfpcap, mfp_qos;

    ath_hal_getcapability(ah, HAL_CAP_MFP, 0, &mfpcap);

    if (mfpcap == HAL_MFP_QOSDATA) {
        /* Treat like legacy hardware. Do not touch the MFP registers. */
        HDPRINTF(ah, HAL_DBG_RESET, "%s forced to use QOSDATA\n", __func__);
        return;
    }

    /* MFP support (Sowl 1.0 or greater) */
    if (mfpcap == HAL_MFP_HW_CRYPTO) {
        /* configure hardware MFP support */
        HDPRINTF(ah, HAL_DBG_RESET, "%s using HW crypto\n", __func__);
        OS_REG_RMW_FIELD(ah,
            AR_AES_MUTE_MASK1, AR_AES_MUTE_MASK1_FC_MGMT, AR_AES_MUTE_MASK1_FC_MGMT_MFP);
        OS_REG_RMW(ah,
            AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_MGMT_CRYPTO_ENABLE,
            AR_PCU_MISC_MODE2_NO_CRYPTO_FOR_NON_DATA_PKT);
        /*
        * Mask used to construct AAD for CCMP-AES
        * Cisco spec defined bits 0-3 as mask
        * IEEE802.11w defined as bit 4.
        */
        if (ath_hal_get_mfp_qos(ah)) {
            mfp_qos = AR_MFP_QOS_MASK_IEEE;
        } else {
            mfp_qos = AR_MFP_QOS_MASK_CISCO;
        }
        OS_REG_RMW_FIELD(ah,
            AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_MGMT_QOS, mfp_qos);
    } else if (mfpcap == HAL_MFP_PASSTHRU) {
        /* Disable en/decrypt by hardware */
        HDPRINTF(ah, HAL_DBG_RESET, "%s using passthru\n", __func__);
        OS_REG_RMW(ah,
            AR_PCU_MISC_MODE2,
            AR_PCU_MISC_MODE2_NO_CRYPTO_FOR_NON_DATA_PKT,
            AR_PCU_MISC_MODE2_MGMT_CRYPTO_ENABLE);
    }
}

void
ar9300_get_channel_centers(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan,
    CHAN_CENTERS *centers)
{
    int8_t      extoff;
    struct ath_hal_9300 *ahp = AH9300(ah);

    if (!IS_CHAN_HT40(chan)) {
        centers->ctl_center = centers->ext_center =
        centers->synth_center = chan->channel;
        return;
    }

    HALASSERT(IS_CHAN_HT40(chan));

    /*
     * In 20/40 phy mode, the center frequency is
     * "between" the primary and extension channels.
     */
    if (chan->channel_flags & CHANNEL_HT40PLUS) {
        centers->synth_center = chan->channel + HT40_CHANNEL_CENTER_SHIFT;
        extoff = 1;
    } else {
        centers->synth_center = chan->channel - HT40_CHANNEL_CENTER_SHIFT;
        extoff = -1;
    }

    centers->ctl_center =
        centers->synth_center - (extoff * HT40_CHANNEL_CENTER_SHIFT);
    centers->ext_center =
        centers->synth_center +
        (extoff * ((ahp->ah_ext_prot_spacing == HAL_HT_EXTPROTSPACING_20) ?
            HT40_CHANNEL_CENTER_SHIFT : 15));
}

/*
 * Read the noise-floor values from the HW.
 * Specifically, read the minimum clear-channel assessment value for
 * each chain, for both the control and extension channels.
 * (The received power level during clear-channel periods is the
 * noise floor.)
 * These noise floor values computed by the HW will be stored in the
 * NF history buffer.
 * The HW sometimes produces bogus NF values.  To avoid using these
 * bogus values, the NF data is (a) range-limited, and (b) filtered.
 * However, this data-processing is done when reading the NF values
 * out of the history buffer.  The history buffer stores the raw values.
 * This allows the NF history buffer to be used to check for interference.
 * A single high NF reading might be a bogus HW value, but if the NF
 * readings are consistently high, it must be due to interference.
 * This is the purpose of storing raw NF values in the history buffer,
 * rather than processed values.  By looking at a history of NF values
 * that have not been range-limited, we can check if they are consistently
 * high (due to interference).
 */
#define AH_NF_SIGN_EXTEND(nf)      \
    ((nf) & 0x100) ?               \
        0 - (((nf) ^ 0x1ff) + 1) : \
        (nf)
void
ar9300_upload_noise_floor(struct ath_hal *ah, int is_2g,
    int16_t nfarray[NUM_NF_READINGS])
{   
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    int16_t nf;
    int chan, chain;
    u_int32_t regs[NUM_NF_READINGS] = {
        /* control channel */
        AR_PHY_CCA_0,     /* chain 0 */
        AR_PHY_CCA_1,     /* chain 1 */
        AR_PHY_CCA_2,     /* chain 2 */
        /* extension channel */
        AR_PHY_EXT_CCA,   /* chain 0 */
        AR_PHY_EXT_CCA_1, /* chain 1 */
        AR_PHY_EXT_CCA_2, /* chain 2 */
    };
    u_int8_t chainmask;

    /*
     * Within a given channel (ctl vs. ext), the CH0, CH1, and CH2
     * masks and shifts are the same, though they differ for the
     * control vs. extension channels.
     */
    u_int32_t masks[2] = {
        AR_PHY_MINCCA_PWR,     /* control channel */
        AR_PHY_EXT_MINCCA_PWR, /* extention channel */
    };
    u_int8_t shifts[2] = {
        AR_PHY_MINCCA_PWR_S,     /* control channel */
        AR_PHY_EXT_MINCCA_PWR_S, /* extention channel */
    };

    /*
     * Force NF calibration for all chains.
     */
    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) || AR_SREV_APHRODITE(ah)) {
        chainmask = 0x01;
    } else if (AR_SREV_WASP(ah) || AR_SREV_JUPITER(ah) || AR_SREV_HONEYBEE(ah) ) {
        chainmask = 0x03;
    } else {
        chainmask = 0x07;
    }
     chainmask = chainmask & ahpriv->ah_caps.hal_rx_chain_mask;
    for (chan = 0; chan < 2 /*ctl,ext*/; chan++) {
        for (chain = 0; chain < AR9300_MAX_CHAINS; chain++) {
            int i;

            if (!((chainmask >> chain) & 0x1)) {
                continue;
            }
            i = chan * AR9300_MAX_CHAINS + chain;
            nf = (OS_REG_READ(ah, regs[i]) & masks[chan]) >> shifts[chan];
            nfarray[i] = AH_NF_SIGN_EXTEND(nf);
        }
    }
}

/* ar9300_get_min_cca_pwr -
 * Used by the scan function for a quick read of the noise floor.
 * This is used to detect presence of CW interference such as video bridge.
 * The noise floor is assumed to have been already started during reset
 * called during channel change. The function checks if the noise floor
 * reading is done. In case it has been done, it reads the noise floor value.
 * If the noise floor calibration has not been finished, it assumes this is
 * due to presence of CW interference an returns a high value for noise floor,
 * derived from the CW interference threshold + margin fudge factor.
 */
#define BAD_SCAN_NF_MARGIN (30)
#define PHY_CCA_MAX_GOOD_VAL -50

int16_t ar9300_get_min_cca_pwr(struct ath_hal *ah)
{
    int16_t nf;

    if ((OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0) {
        nf = MS(OS_REG_READ(ah, AR_PHY_CCA_0), AR9280_PHY_MINCCA_PWR);
        if (nf & 0x100) {
            nf = 0 - ((nf ^ 0x1ff) + 1);
        }

        if (nf == PHY_CCA_MAX_GOOD_VAL)
        {   
            /* Making NF value as 0 when HW clears the NF cal complete bit, but NF cal could not converge i.e. 
               HW indication in AR_PHY_AGC_CONTROL register about NF convergence being complete is to be ignored, 
               no side effects*/
            nf = 0;
        }        
        
    } else {
        /* NF calibration is not done, assume CW interference */
        /* Making NF value as 0 if NF cal does not converge, the host will handle further, no side effects*/  
        nf = 0; 
    }
    return nf;
}


/*
 * Noise Floor values for all chains.
 * Most recently updated values from the NF history buffer are used.
 */
void ar9300_chain_noise_floor(struct ath_hal *ah, int16_t *nf_buf,
    HAL_CHANNEL *chan, int is_scan)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    int i, nf_hist_len, recent_nf_index = 0;
    HAL_NFCAL_HIST_FULL *h;
    u_int8_t rx_chainmask = ahp->ah_rx_chainmask | (ahp->ah_rx_chainmask << 3);
    HAL_CHANNEL_INTERNAL *ichan = ath_hal_checkchannel(ah, chan);
    HALASSERT(ichan);

#ifdef ATH_NF_PER_CHAN
    /* Fill 0 if valid internal channel is not found */
    if (ichan == AH_NULL) {
        OS_MEMZERO(nf_buf, sizeof(nf_buf[0])*NUM_NF_READINGS);
        return;
    }
    h = &ichan->nf_cal_hist;
    nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
#else
    /*
     * If a scan is not in progress, then the most recent value goes
     * into ahpriv->nf_cal_hist.  If a scan is in progress, then
     * the most recent value goes into ichan->nf_cal_hist.
     * Thus, return the value from ahpriv->nf_cal_hist if there's
     * no scan, and if the specified channel is the current channel.
     * Otherwise, return the noise floor from ichan->nf_cal_hist.
     */
    if ((!is_scan) && chan->channel == AH_PRIVATE(ah)->ah_curchan->channel) {
        h = &AH_PRIVATE(ah)->nf_cal_hist;
        nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
    } else {
        /* Fill 0 if valid internal channel is not found */
        if (ichan == AH_NULL) {
            OS_MEMZERO(nf_buf, sizeof(nf_buf[0])*NUM_NF_READINGS);
            return;
        }
       /*
        * It is okay to treat a HAL_NFCAL_HIST_SMALL struct as if it were a
        * HAL_NFCAL_HIST_FULL struct, as long as only the index 0 of the
        * nf_cal_buffer is used (nf_cal_buffer[0][0:NUM_NF_READINGS-1])
        */
        h = (HAL_NFCAL_HIST_FULL *) &ichan->nf_cal_hist;
        nf_hist_len = HAL_NF_CAL_HIST_LEN_SMALL;
    }
#endif
    /* Get most recently updated values from nf cal history buffer */
    recent_nf_index =
        (h->base.curr_index) ? h->base.curr_index - 1 : nf_hist_len - 1;

    for (i = 0; i < NUM_NF_READINGS; i++) {
        /* Fill 0 for unsupported chains */
        if (!(rx_chainmask & (1 << i))) {
            nf_buf[i] = 0;
            continue;
        }
        nf_buf[i] = h->nf_cal_buffer[recent_nf_index][i];
    }
}

/*
 * Return the current NF value in register.
 * If the current NF cal is not completed, return 0.
 */
int16_t ar9300_get_nf_from_reg(struct ath_hal *ah, HAL_CHANNEL *chan, int wait_time)
{
    int16_t nfarray[NUM_NF_READINGS] = {0};
    int is_2g = 0;

    if (wait_time <= 0) {
        return 0;
    }

    if (!ath_hal_wait(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF, 0, wait_time)) {
        printk("%s: NF cal is not complete in %dus", __func__, wait_time);
        return 0;
    }
#define IS(_c, _f)       (((_c)->channel_flags & _f) || 0)
    is_2g = IS(chan, CHANNEL_2GHZ);
#undef IS
    ar9300_upload_noise_floor(ah, is_2g, nfarray);

    return nfarray[0];
}

/*
 * Pick up the medium one in the noise floor buffer and update the
 * corresponding range for valid noise floor values
 */
static int16_t
ar9300_get_nf_hist_mid(struct ath_hal *ah, HAL_NFCAL_HIST_FULL *h, int reading,
    int hist_len)
{
    int16_t nfval;
    int16_t sort[HAL_NF_CAL_HIST_LEN_FULL]; /* upper bound for hist_len */
    int i, j;


    for (i = 0; i < hist_len; i++) {
        sort[i] = h->nf_cal_buffer[i][reading];
        HDPRINTF(ah, HAL_DBG_NF_CAL,
            "nf_cal_buffer[%d][%d] = %d\n", i, reading, (int)sort[i]);
    }
    for (i = 0; i < hist_len - 1; i++) {
        for (j = 1; j < hist_len - i; j++) {
            if (sort[j] > sort[j - 1]) {
                nfval = sort[j];
                sort[j] = sort[j - 1];
                sort[j - 1] = nfval;
            }
        }
    }
    nfval = sort[(hist_len - 1) >> 1];

    return nfval;
}

static int16_t ar9300_limit_nf_range(struct ath_hal *ah, int16_t nf)
{
    return nf;
}

#ifndef ATH_NF_PER_CHAN
inline static void
ar9300_reset_nf_hist_buff(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichan)
{
    HAL_CHAN_NFCAL_HIST *h = &ichan->nf_cal_hist;
    HAL_NFCAL_HIST_FULL *home = &AH_PRIVATE(ah)->nf_cal_hist;
    int i;

    /*
     * Copy the value for the channel in question into the home-channel
     * NF history buffer.  The channel NF is probably a value filled in by
     * a prior background channel scan, but if no scan has been done then
     * it is the nominal noise floor filled in by ath_hal_init_NF_buffer
     * for this chip and the channel's band.
     * Replicate this channel NF into all entries of the home-channel NF
     * history buffer.
     * If the channel NF was filled in by a channel scan, it has not had
     * bounds limits applied to it yet - do so now.  It is important to
     * apply bounds limits to the priv_nf value that gets loaded into the
     * WLAN chip's min_cca_pwr register field.  It is also necessary to
     * apply bounds limits to the nf_cal_buffer[] elements.  Since we are
     * replicating a single NF reading into all nf_cal_buffer elements,
     * if the single reading were above the CW_INT threshold, the CW_INT
     * check in ar9300_get_nf would immediately conclude that CW interference
     * is present, even though we're not supposed to set CW_INT unless
     * NF values are _consistently_ above the CW_INT threshold.
     * Applying the bounds limits to the nf_cal_buffer contents fixes this
     * problem.
     */
    for (i = 0; i < NUM_NF_READINGS; i ++) {
        int j;
        int16_t nf;
        /*
         * No need to set curr_index, since it already has a value in
         * the range [0..HAL_NF_CAL_HIST_LEN_FULL), and all nf_cal_buffer
         * values will be the same.
         */
        nf = ar9300_limit_nf_range(ah, h->nf_cal_buffer[0][i]);
        for (j = 0; j < HAL_NF_CAL_HIST_LEN_FULL; j++) {
            home->nf_cal_buffer[j][i] = nf;
        }
        AH_PRIVATE(ah)->nf_cal_hist.base.priv_nf[i] = nf;
    }
}
#endif

/*
 *  Update the noise floor buffer as a ring buffer
 */
static int16_t
ar9300_update_nf_hist_buff(struct ath_hal *ah, HAL_NFCAL_HIST_FULL *h,
   int16_t *nfarray, int hist_len)
{
    int i, nr;
    int16_t nf_no_lim_chain0;

    nf_no_lim_chain0 = ar9300_get_nf_hist_mid(ah, h, 0, hist_len);

    HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] BEFORE\n", __func__, __LINE__);
    for (nr = 0; nr < HAL_NF_CAL_HIST_LEN_FULL; nr++) {
        for (i = 0; i < NUM_NF_READINGS; i++) {
            HDPRINTF(ah, HAL_DBG_NF_CAL,
                "nf_cal_buffer[%d][%d] = %d\n",
                nr, i, (int)h->nf_cal_buffer[nr][i]);
        }
    }
    for (i = 0; i < NUM_NF_READINGS; i++) {
        h->nf_cal_buffer[h->base.curr_index][i] = nfarray[i];
        h->base.priv_nf[i] = ar9300_limit_nf_range(
            ah, ar9300_get_nf_hist_mid(ah, h, i, hist_len));
    }
    HDPRINTF(ah, HAL_DBG_NF_CAL, "%s[%d] AFTER\n", __func__, __LINE__);
    for (nr = 0; nr < HAL_NF_CAL_HIST_LEN_FULL; nr++) {
        for (i = 0; i < NUM_NF_READINGS; i++) {
            HDPRINTF(ah, HAL_DBG_NF_CAL,
                "nf_cal_buffer[%d][%d] = %d\n",
                nr, i, (int)h->nf_cal_buffer[nr][i]);
        }
    }

    if (++h->base.curr_index >= hist_len) {
        h->base.curr_index = 0;
    }

    return nf_no_lim_chain0;
}



/*
 * Adaptive CCA threshold - Calculate and update CCA threshold periodically
 * after NF calibration and at the end of initialization sequence during every
 * chip reset.
 *
 * Step 1: Compute NF_max_primary and NF_max_extension
 * If noise floor completes,
 *   NF_max_primary = max of noise floor read across all chains in primary channel
 *   NF_max_extension = max of noise floor read across all chains in extension channel
 * else
 *   NF_max_primary = NF_max_extension = the value that is forced into HW as noise floor
 *
 * Step 2: Compute CCA_threshold_primary and CCA_threshold_extension
 *   CCA_threshold_primary = CCA_detection_level – CCA_detection_margin – NF_max_primary
 *   CCA_threshold_extension = CCA_detection_level – CCA_detection_margin – NF_max_extension
 *
 * Step 3: Program CCA thresholds
 *
 */
#define BEST_CASE_NOISE_FLOOR   -130
#define MAX_CCA_THRESHOLD       90
#define MIN_CCA_THRESHOLD       0
#define NF_NOM_20MHZ            -101
#define NF_NOM_40MHZ            -98
static void
ar9300_update_cca_threshold(struct ath_hal *ah, int16_t nfarray[NUM_NF_READINGS], u_int8_t rxchainmask)
{
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    int chan, chain;
    u_int16_t   cca_detection_margin_pri, cca_detection_margin_ext;
    int16_t nf, nf_max_primary, nf_max_extension, nf_nominal;
    int16_t derived_max_cca, max_cca_cap, cca_threshold_primary, cca_threshold_extension;
    u_int8_t chainmask;

    if (IS_CHAN_2GHZ(ahpriv->ah_curchan)) {
        nf = ahpriv->nf_2GHz.max;
    }
    else
    {
        nf = ahpriv->nf_5GHz.max;
    }

    nf_max_primary = nf_max_extension = nf;

    if(AH_PRIVATE(ah)->green_ap_ps_on) {
        chainmask = 1;
    } else {
        chainmask = rxchainmask & ahpriv->ah_caps.hal_rx_chain_mask;
    }

    /* Compute max of noise floor read across all chains in primary channel */
    for (chan = 0; chan < 2 /*ctl,ext*/; chan++) {
        int init_nf = 0;
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                    "\n chan : %s", (chan == 0) ? "ctrl" : "extn");
        for (chain = 0; chain < AR9300_MAX_CHAINS; chain++) {
            int i;

            if (!((chainmask >> chain) & 0x1)) {
                continue;
            }
            i = chan * AR9300_MAX_CHAINS + chain;
            if (init_nf == 0) {
                nf = nfarray[i];
                init_nf = 1;
            }
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                        "\t nfarray[%d]= %d", i, nfarray[i]);
            nf = (nf > nfarray[i]) ? nf : nfarray[i];
        }
        if (chan == 0) {
            nf_max_primary = nf;
        } else {
            nf_max_extension = nf;
        }
    }
    if (IS_CHAN_HT40(ahpriv->ah_curchan)) {
        nf_nominal = NF_NOM_40MHZ;
    } else {
        nf_nominal = NF_NOM_20MHZ;
    }

    if (nf_max_primary < nf_nominal) {
        cca_detection_margin_pri = ahpriv->ah_config.ath_hal_cca_detection_margin + (nf_nominal - nf_max_primary);
    } else {
        cca_detection_margin_pri = ahpriv->ah_config.ath_hal_cca_detection_margin;
    }
    if (nf_max_extension < nf_nominal) {
        cca_detection_margin_ext = ahpriv->ah_config.ath_hal_cca_detection_margin + (nf_nominal - nf_max_extension);
    } else {
        cca_detection_margin_ext = ahpriv->ah_config.ath_hal_cca_detection_margin;
    }

    derived_max_cca = (ahpriv->ah_config.ath_hal_cca_detection_level - ahpriv->ah_config.ath_hal_cca_detection_margin - BEST_CASE_NOISE_FLOOR);
    max_cca_cap = derived_max_cca < MAX_CCA_THRESHOLD ? derived_max_cca : MAX_CCA_THRESHOLD;
      HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s: derived_max_cca : %d max_cca_cap : %d \n", __func__, derived_max_cca, max_cca_cap);
    cca_threshold_primary = (ahpriv->ah_config.ath_hal_cca_detection_level - cca_detection_margin_pri - nf_max_primary);
    cca_threshold_primary = cca_threshold_primary < max_cca_cap ? (cca_threshold_primary > MIN_CCA_THRESHOLD ? cca_threshold_primary : MIN_CCA_THRESHOLD) : max_cca_cap;
    cca_threshold_extension = (ahpriv->ah_config.ath_hal_cca_detection_level - cca_detection_margin_ext - nf_max_extension);
    cca_threshold_extension = cca_threshold_extension < max_cca_cap ? (cca_threshold_extension > MIN_CCA_THRESHOLD ? cca_threshold_extension : MIN_CCA_THRESHOLD) : max_cca_cap;

    HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s: nf_max_primary : %d nf_max_extension : %d cca_pri : %d cca_ext : %d\n",
                        __func__, nf_max_primary, nf_max_extension, cca_threshold_primary, cca_threshold_extension);
    OS_REG_RMW_FIELD(ah, AR_PHY_CCA_0, AR_PHY_CCA_THRESH62, cca_threshold_primary);
    OS_REG_RMW_FIELD(ah, AR_PHY_EXTCHN_PWRTHR1, AR_PHY_EXT_CCA0_THRESH62, cca_threshold_extension);
    OS_REG_RMW_FIELD(ah, AR_PHY_EXT_CCA0, AR_PHY_EXT_CCA0_THRESH62_MODE, 0x0);
}

/*
 * Read the NF and check it against the noise floor threshhold
 */
#define IS(_c, _f)       (((_c)->channel_flags & _f) || 0)
static int
ar9300_store_new_nf(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan, int is_scan)
{
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    int nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
    int16_t nf_no_lim;
    int16_t nfarray[NUM_NF_READINGS] = {0};
    HAL_NFCAL_HIST_FULL *h;
    int is_2g = 0;

    if (OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) {
        u_int32_t tsf32, nf_cal_dur_tsf;
        /*
         * The reason the NF calibration did not complete may just be that
         * not enough time has passed since the NF calibration was started,
         * because under certain conditions (when first moving to a new
         * channel) the NF calibration may be checked very repeatedly.
         * Or, there may be CW interference keeping the NF calibration
         * from completing.  Check the delta time between when the NF
         * calibration was started and now to see whether the NF calibration
         * should have already completed (but hasn't, probably due to CW
         * interference), or hasn't had enough time to finish yet.
         */
        /*
         * AH_NF_CAL_DUR_MAX_TSF - A conservative maximum time that the
         *     HW should need to finish a NF calibration.  If the HW
         *     does not complete a NF calibration within this time period,
         *     there must be a problem - probably CW interference.
         * AH_NF_CAL_PERIOD_MAX_TSF - A conservative maximum time between
         *     check of the HW's NF calibration being finished.
         *     If the difference between the current TSF and the TSF
         *     recorded when the NF calibration started is larger than this
         *     value, the TSF must have been reset.
         *     In general, we expect the TSF to only be reset during
         *     regular operation for STAs, not for APs.  However, an
         *     AP's TSF could be reset when joining an IBSS.
         *     There's an outside chance that this could result in the
         *     CW_INT flag being erroneously set, if the TSF adjustment
         *     is smaller than AH_NF_CAL_PERIOD_MAX_TSF but larger than
         *     AH_NF_CAL_DUR_TSF.  However, even if this does happen,
         *     it shouldn't matter, as the IBSS case shouldn't be
         *     concerned about CW_INT.
         */
        /* AH_NF_CAL_DUR_TSF - 90 sec in usec units */
        #define AH_NF_CAL_DUR_TSF (90 * 1000 * 1000)
        /* AH_NF_CAL_PERIOD_MAX_TSF - 180 sec in usec units */
        #define AH_NF_CAL_PERIOD_MAX_TSF (180 * 1000 * 1000)
        /* wraparound handled by using unsigned values */
        tsf32 = ar9300_get_tsf32(ah);
        nf_cal_dur_tsf = tsf32 - AH9300(ah)->nf_tsf32;
        if (nf_cal_dur_tsf > AH_NF_CAL_PERIOD_MAX_TSF) {
            /*
             * The TSF must have gotten reset during the NF cal -
             * just reset the NF TSF timestamp, so the next time
             * this function is called, the timestamp comparison
             * will be valid.
             */
            AH9300(ah)->nf_tsf32 = tsf32;
        } else if (nf_cal_dur_tsf > AH_NF_CAL_DUR_TSF) {
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s: NF did not complete in calibration window\n", __func__);
            /* the NF incompletion is probably due to CW interference */
            chan->channel_flags |= CHANNEL_CW_INT;
        }
        return 0; /* HW's NF measurement not finished */
    }
    HDPRINTF(ah, HAL_DBG_NF_CAL,
        "%s[%d] chan %d\n", __func__, __LINE__, chan->channel);
    is_2g = IS(chan, CHANNEL_2GHZ);
    ar9300_upload_noise_floor(ah, is_2g, nfarray);

    /* Update the NF buffer for each chain masked by chainmask */
#ifdef ATH_NF_PER_CHAN
    h = &chan->nf_cal_hist;
    nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
#else
    if (is_scan) {
        /*
         * This channel's NF cal info is just a HAL_NFCAL_HIST_SMALL struct
         * rather than a HAL_NFCAL_HIST_FULL struct.
         * As long as we only use the first history element of nf_cal_buffer
         * (nf_cal_buffer[0][0:NUM_NF_READINGS-1]), we can use
         * HAL_NFCAL_HIST_SMALL and HAL_NFCAL_HIST_FULL interchangeably.
         */
        h = (HAL_NFCAL_HIST_FULL *) &chan->nf_cal_hist;
        nf_hist_len = HAL_NF_CAL_HIST_LEN_SMALL;
    } else {
        h = &AH_PRIVATE(ah)->nf_cal_hist;
        nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
    }
#endif

    /*
     * nf_no_lim = median value from NF history buffer without bounds limits,
     * priv_nf = median value from NF history buffer with bounds limits.
     */
    nf_no_lim = ar9300_update_nf_hist_buff(ah, h, nfarray, nf_hist_len);

    chan->raw_noise_floor = h->base.priv_nf[0];

    /* check if there is interference */
    chan->channel_flags &= (~CHANNEL_CW_INT);
    /*
     * Use AR9300_EMULATION to check for emulation purpose as PCIE Device ID
     * 0xABCD is recognized as valid Osprey as WAR in some EVs.
     */
    if (nf_no_lim > ahpriv->nfp->nominal + ahpriv->nf_cw_int_delta) {
        /*
         * Since this CW interference check is being applied to the
         * median element of the NF history buffer, this indicates that
         * the CW interference is persistent.  A single high NF reading
         * will not show up in the median, and thus will not cause the
         * CW_INT flag to be set.
         */
        HDPRINTF(ah, HAL_DBG_NF_CAL,
            "%s: NF Cal: CW interferer detected through NF: %d\n",
            __func__, nf_no_lim);
        chan->channel_flags |= CHANNEL_CW_INT;

    }
    return 1; /* HW's NF measurement finished */
}
#undef IS

static inline void
ar9300_get_delta_slope_values(struct ath_hal *ah, u_int32_t coef_scaled,
    u_int32_t *coef_mantissa, u_int32_t *coef_exponent)
{
    u_int32_t coef_exp, coef_man;

    /*
     * ALGO -> coef_exp = 14-floor(log2(coef));
     * floor(log2(x)) is the highest set bit position
     */
    for (coef_exp = 31; coef_exp > 0; coef_exp--) {
        if ((coef_scaled >> coef_exp) & 0x1) {
            break;
        }
    }
    /* A coef_exp of 0 is a legal bit position but an unexpected coef_exp */
    HALASSERT(coef_exp);
    coef_exp = 14 - (coef_exp - COEF_SCALE_S);


    /*
     * ALGO -> coef_man = floor(coef* 2^coef_exp+0.5);
     * The coefficient is already shifted up for scaling
     */
    coef_man = coef_scaled + (1 << (COEF_SCALE_S - coef_exp - 1));

    *coef_mantissa = coef_man >> (COEF_SCALE_S - coef_exp);
    *coef_exponent = coef_exp - 16;
}

#define MAX_ANALOG_START        319             /* XXX */

/*
 * Delta slope coefficient computation.
 * Required for OFDM operation.
 */
static void
ar9300_set_delta_slope(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan)
{
    u_int32_t coef_scaled, ds_coef_exp, ds_coef_man;
    u_int32_t fclk = COEFF; /* clock * 2.5 */

#ifdef AR9340_EMULATION
#ifdef AR9550_EMULATION
    u_int32_t clock_mhz_scaled = 0x400000 * fclk;
#else
    u_int32_t clock_mhz_scaled = 0x800000 * fclk;
#endif
#else
    u_int32_t clock_mhz_scaled = 0x1000000 * fclk;
#endif
    CHAN_CENTERS centers;

    /*
     * half and quarter rate can divide the scaled clock by 2 or 4
     * scale for selected channel bandwidth
     */
    if (IS_CHAN_HALF_RATE(chan)) {
        clock_mhz_scaled = clock_mhz_scaled >> 1;
    } else if (IS_CHAN_QUARTER_RATE(chan)) {
        clock_mhz_scaled = clock_mhz_scaled >> 2;
    }

    /*
     * ALGO -> coef = 1e8/fcarrier*fclock/40;
     * scaled coef to provide precision for this floating calculation
     */
    ar9300_get_channel_centers(ah, chan, &centers);
    coef_scaled = clock_mhz_scaled / centers.synth_center;

    ar9300_get_delta_slope_values(ah, coef_scaled, &ds_coef_man, &ds_coef_exp);

    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING3, AR_PHY_TIMING3_DSC_MAN, ds_coef_man);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING3, AR_PHY_TIMING3_DSC_EXP, ds_coef_exp);

    /*
     * For Short GI,
     * scaled coeff is 9/10 that of normal coeff
     */
    coef_scaled = (9 * coef_scaled) / 10;

    ar9300_get_delta_slope_values(ah, coef_scaled, &ds_coef_man, &ds_coef_exp);

    /* for short gi */
    OS_REG_RMW_FIELD(ah, AR_PHY_SGI_DELTA, AR_PHY_SGI_DSC_MAN, ds_coef_man);
    OS_REG_RMW_FIELD(ah, AR_PHY_SGI_DELTA, AR_PHY_SGI_DSC_EXP, ds_coef_exp);
}

#define IS(_c, _f)       (((_c)->channel_flags & _f) || 0)

static inline HAL_CHANNEL_INTERNAL*
ar9300_check_chan(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    if ((IS(chan, CHANNEL_2GHZ) ^ IS(chan, CHANNEL_5GHZ)) == 0) {
        HDPRINTF(ah, HAL_DBG_CHANNEL,
            "%s: invalid channel %u/0x%x; not marked as 2GHz or 5GHz\n",
            __func__, chan->channel, chan->channel_flags);
        return AH_NULL;
    }

    if ((IS(chan, CHANNEL_OFDM) ^ IS(chan, CHANNEL_CCK) ^
         IS(chan, CHANNEL_HT20) ^ IS(chan, CHANNEL_HT40PLUS) ^
         IS(chan, CHANNEL_HT40MINUS)) == 0)
    {
        HDPRINTF(ah, HAL_DBG_CHANNEL,
            "%s: invalid channel %u/0x%x; not marked as "
            "OFDM or CCK or HT20 or HT40PLUS or HT40MINUS\n",
            __func__, chan->channel, chan->channel_flags);
        return AH_NULL;
    }

    return (ath_hal_checkchannel(ah, chan));
}
#undef IS

static void
ar9300_set_11n_regs(struct ath_hal *ah, HAL_CHANNEL *chan,
    HAL_HT_MACMODE macmode)
{
    u_int32_t phymode;
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int32_t enable_dac_fifo;

    /* XXX */
    enable_dac_fifo =
        OS_REG_READ(ah, AR_PHY_GEN_CTRL) & AR_PHY_GC_ENABLE_DAC_FIFO;

	if (AR_SREV_DRAGONFLY(ah)) {
		/*
		for 3 stream TX EVM improvement
				  0xa204[9]:BB_gen_controls.cf_3_chains_use_walsh=0
		*/
		phymode =
			AR_PHY_GC_HT_EN | AR_PHY_GC_SHORT_GI_40;
	}else {
		phymode =
			AR_PHY_GC_HT_EN | AR_PHY_GC_SINGLE_HT_LTF1 | AR_PHY_GC_SHORT_GI_40;
	}
    /* Enable 11n HT, 20 MHz */
    phymode |= enable_dac_fifo;
    /* Configure baseband for dynamic 20/40 operation */
    if (IS_CHAN_HT40(chan)) {
        phymode |= AR_PHY_GC_DYN2040_EN;
        /* Configure control (primary) channel at +-10MHz */
        if (chan->channel_flags & CHANNEL_HT40PLUS) {
            phymode |= AR_PHY_GC_DYN2040_PRI_CH;
        }

        /* Configure 20/25 spacing */
        if (ahp->ah_ext_prot_spacing == HAL_HT_EXTPROTSPACING_25) {
            phymode |= AR_PHY_GC_DYN2040_EXT_CH;
        }
    }

    /* make sure we preserve INI settings */
    phymode |= OS_REG_READ(ah, AR_PHY_GEN_CTRL);

    /* EV 62881/64991 - turn off Green Field detection for Maverick STA beta */
    phymode &= ~AR_PHY_GC_GF_DETECT_EN;

    OS_REG_WRITE(ah, AR_PHY_GEN_CTRL, phymode);

    /* Set IFS timing for half/quarter rates */
    if (IS_CHAN_HALF_RATE(chan) || IS_CHAN_QUARTER_RATE(chan)) {
        u_int32_t modeselect = OS_REG_READ(ah, AR_PHY_MODE);

        if (IS_CHAN_HALF_RATE(chan)) {
            modeselect |= AR_PHY_MS_HALF_RATE;
        } else if (IS_CHAN_QUARTER_RATE(chan)) {
            modeselect |= AR_PHY_MS_QUARTER_RATE;
        }
        OS_REG_WRITE(ah, AR_PHY_MODE, modeselect);

        ar9300_set_ifs_timing(ah, chan);
        OS_REG_RMW_FIELD(
            ah, AR_PHY_FRAME_CTL, AR_PHY_FRAME_CTL_CF_OVERLAP_WINDOW, 0x3);
    }

    /* Configure MAC for 20/40 operation */
    ar9300_set_11n_mac2040(ah, macmode);

    /* global transmit timeout (25 TUs default)*/
    /* XXX - put this elsewhere??? */
    OS_REG_WRITE(ah, AR_GTXTO, 25 << AR_GTXTO_TIMEOUT_LIMIT_S);

    /* carrier sense timeout */
    OS_REG_WRITE(ah, AR_CST, 0xF << AR_CST_TIMEOUT_LIMIT_S);
}

/*
 * Spur mitigation for MRC CCK
 */
static void
ar9300_spur_mitigate_mrc_cck(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    int i;
    /* spur_freq_for_osprey - hardcoded by Systems team for now. */
    u_int32_t spur_freq_for_osprey[4] = { 2420, 2440, 2464, 2480 };
    u_int32_t spur_freq_for_jupiter[2] = { 2440, 2464};
    int cur_bb_spur, negative = 0, cck_spur_freq;
    u_int8_t* spur_fbin_ptr = NULL;
    int synth_freq;
    int range = 10;
    int max_spurcounts = OSPREY_EEPROM_MODAL_SPURS;

    /*
     * Need to verify range +/- 10 MHz in control channel, otherwise spur
     * is out-of-band and can be ignored.
     */
    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) ||
        AR_SREV_WASP(ah)  || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah)) {
        spur_fbin_ptr = ar9300_eeprom_get_spur_chans_ptr(ah, 1);
        if (spur_fbin_ptr[0] == 0) {
            return;      /* No spur in the mode */
        }
        if (IS_CHAN_HT40(chan)) {
            range = 19;
            if (OS_REG_READ_FIELD(ah, AR_PHY_GEN_CTRL, AR_PHY_GC_DYN2040_PRI_CH)
                == 0x0)
            {
                synth_freq = chan->channel + 10;
            } else {
                synth_freq = chan->channel - 10;
            }
        } else {
            range = 10;
            synth_freq = chan->channel;
        }
    } else if(AR_SREV_JUPITER(ah)) {
        range = 5;
        max_spurcounts = 2; /* Hardcoded by Jupiter Systems team for now. */
        synth_freq = chan->channel;
    } else {
        range = 10;
        max_spurcounts = 4; /* Hardcoded by Osprey Systems team for now. */
        synth_freq = chan->channel;
    }

    for (i = 0; i < max_spurcounts; i++) {
        negative = 0;

        if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) ||
            AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah)) {
            cur_bb_spur =
                FBIN2FREQ(spur_fbin_ptr[i], HAL_FREQ_BAND_2GHZ) - synth_freq;
        } else if(AR_SREV_JUPITER(ah)) {
            cur_bb_spur = spur_freq_for_jupiter[i] - synth_freq;
        } else {
            cur_bb_spur = spur_freq_for_osprey[i] - synth_freq;
        }

        if (cur_bb_spur < 0) {
            negative = 1;
            cur_bb_spur = -cur_bb_spur;
        }
        if (cur_bb_spur < range) {
            cck_spur_freq = (int)((cur_bb_spur << 19) / 11);
            if (negative == 1) {
                cck_spur_freq = -cck_spur_freq;
            }
            cck_spur_freq = cck_spur_freq & 0xfffff;
            /*OS_REG_WRITE_field(ah, BB_agc_control.ycok_max, 0x7);*/
            OS_REG_RMW_FIELD(ah,
                AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_YCOK_MAX, 0x7);
            /*OS_REG_WRITE_field(ah, BB_cck_spur_mit.spur_rssi_thr, 0x7f);*/
            OS_REG_RMW_FIELD(ah,
                AR_PHY_CCK_SPUR_MIT, AR_PHY_CCK_SPUR_MIT_SPUR_RSSI_THR, 0x7f);
            /*OS_REG_WRITE(ah, BB_cck_spur_mit.spur_filter_type, 0x2);*/
            OS_REG_RMW_FIELD(ah,
                AR_PHY_CCK_SPUR_MIT, AR_PHY_CCK_SPUR_MIT_SPUR_FILTER_TYPE, 0x2);
            /*OS_REG_WRITE(ah, BB_cck_spur_mit.use_cck_spur_mit, 0x1);*/
            OS_REG_RMW_FIELD(ah,
                AR_PHY_CCK_SPUR_MIT, AR_PHY_CCK_SPUR_MIT_USE_CCK_SPUR_MIT, 0x1);
            /*OS_REG_WRITE(ah, BB_cck_spur_mit.cck_spur_freq, cck_spur_freq);*/
            OS_REG_RMW_FIELD(ah,
                AR_PHY_CCK_SPUR_MIT, AR_PHY_CCK_SPUR_MIT_CCK_SPUR_FREQ,
                cck_spur_freq);
            return;
        }
    }

    /*OS_REG_WRITE(ah, BB_agc_control.ycok_max, 0x5);*/
    OS_REG_RMW_FIELD(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_YCOK_MAX, 0x5);
    /*OS_REG_WRITE(ah, BB_cck_spur_mit.use_cck_spur_mit, 0x0);*/
    OS_REG_RMW_FIELD(ah,
        AR_PHY_CCK_SPUR_MIT, AR_PHY_CCK_SPUR_MIT_USE_CCK_SPUR_MIT, 0x0);
    /*OS_REG_WRITE(ah, BB_cck_spur_mit.cck_spur_freq, 0x0);*/
    OS_REG_RMW_FIELD(ah,
        AR_PHY_CCK_SPUR_MIT, AR_PHY_CCK_SPUR_MIT_CCK_SPUR_FREQ, 0x0);
}

/* Spur mitigation for OFDM */
static void
ar9300_spur_mitigate_ofdm(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    int synth_freq;
    int range = 10;
    int freq_offset = 0;
    int spur_freq_sd = 0;
    int spur_subchannel_sd = 0;
    int spur_delta_phase = 0;
    int mask_index = 0;
    int i;
    int mode;
    u_int8_t* spur_chans_ptr;

    if (IS_CHAN_5GHZ(chan)) {
        spur_chans_ptr = ar9300_eeprom_get_spur_chans_ptr(ah, 0);
        mode = 0;
    } else {
        spur_chans_ptr = ar9300_eeprom_get_spur_chans_ptr(ah, 1);
        mode = 1;
    }

    if (IS_CHAN_HT40(chan)) {
        range = 19;
        if (OS_REG_READ_FIELD(ah, AR_PHY_GEN_CTRL, AR_PHY_GC_DYN2040_PRI_CH)
            == 0x0)
        {
            synth_freq = chan->channel - 10;
        } else {
            synth_freq = chan->channel + 10;
        }
    } else {
        range = 10;
        synth_freq = chan->channel;
    }

    /* Clean all spur register fields */
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_FILTER, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING11, AR_PHY_TIMING11_SPUR_FREQ_SD, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING11, AR_PHY_TIMING11_SPUR_DELTA_PHASE, 0);
    OS_REG_RMW_FIELD(ah,
        AR_PHY_SFCORR_EXT, AR_PHY_SFCORR_EXT_SPUR_SUBCHANNEL_SD, 0);
    OS_REG_RMW_FIELD(ah,
        AR_PHY_TIMING11, AR_PHY_TIMING11_USE_SPUR_FILTER_IN_AGC, 0);
    OS_REG_RMW_FIELD(ah,
        AR_PHY_TIMING11, AR_PHY_TIMING11_USE_SPUR_FILTER_IN_SELFCOR, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_RSSI, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_EN_VIT_SPUR_RSSI, 0);
    OS_REG_RMW_FIELD(ah,
        AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_ENABLE_NF_RSSI_SPUR_MIT, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_ENABLE_MASK_PPM, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_PILOT_MASK, 0);
    OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_CHAN_MASK, 0);
    OS_REG_RMW_FIELD(ah,
        AR_PHY_PILOT_SPUR_MASK, AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_IDX_A, 0);
	if (AR_SREV_DRAGONFLY(ah)) {
	    OS_REG_RMW_FIELD(ah,
	        AR_PHY_SPUR_MASK_A_DRAGONFLY, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_IDX_A, 0);
	    OS_REG_RMW_FIELD(ah,
	        AR_PHY_CHAN_SPUR_MASK, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_IDX_A, 0);
	    OS_REG_RMW_FIELD(ah,
	        AR_PHY_PILOT_SPUR_MASK, AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_A, 0);
	    OS_REG_RMW_FIELD(ah,
	        AR_PHY_CHAN_SPUR_MASK, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_A, 0);
	    OS_REG_RMW_FIELD(ah,
	        AR_PHY_SPUR_MASK_A_DRAGONFLY, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_A, 0);
	    OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_MASK_RATE_CNTL, 0);
	} else {
        OS_REG_RMW_FIELD(ah,
            AR_PHY_SPUR_MASK_A, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_IDX_A, 0);
        OS_REG_RMW_FIELD(ah,
            AR_PHY_CHAN_SPUR_MASK, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_IDX_A, 0);
        OS_REG_RMW_FIELD(ah,
            AR_PHY_PILOT_SPUR_MASK, AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_A, 0);
        OS_REG_RMW_FIELD(ah,
            AR_PHY_CHAN_SPUR_MASK, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_A, 0);
        OS_REG_RMW_FIELD(ah,
            AR_PHY_SPUR_MASK_A, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_A, 0);
        OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_MASK_RATE_CNTL, 0);
	}
    i = 0;
    while (spur_chans_ptr[i] && i < 5) {
        freq_offset = FBIN2FREQ(spur_chans_ptr[i], mode) - synth_freq;
        if (abs(freq_offset) < range) {
            /*
            printf(
                "Spur Mitigation for OFDM: Synth Frequency = %d, "
                "Spur Frequency = %d\n",
                synth_freq, FBIN2FREQ(spur_chans_ptr[i], mode));
             */
            if (IS_CHAN_HT40(chan)) {
                if (freq_offset < 0) {
                    if (OS_REG_READ_FIELD(
                        ah, AR_PHY_GEN_CTRL, AR_PHY_GC_DYN2040_PRI_CH) == 0x0)
                    {
                        spur_subchannel_sd = 1;
                    } else {
                        spur_subchannel_sd = 0;
                    }
                    spur_freq_sd = ((freq_offset + 10) << 9) / 11;
                } else {
                    if (OS_REG_READ_FIELD(ah,
                        AR_PHY_GEN_CTRL, AR_PHY_GC_DYN2040_PRI_CH) == 0x0)
                    {
                        spur_subchannel_sd = 0;
                    } else {
                        spur_subchannel_sd = 1;
                    }
                    spur_freq_sd = ((freq_offset - 10) << 9) / 11;
                }
                spur_delta_phase = (freq_offset << 17) / 5;
            } else {
                spur_subchannel_sd = 0;
                spur_freq_sd = (freq_offset << 9) / 11;
                spur_delta_phase = (freq_offset << 18) / 5;
            }
            spur_freq_sd = spur_freq_sd & 0x3ff;
            spur_delta_phase = spur_delta_phase & 0xfffff;
            /*
            printf(
                "spur_subchannel_sd = %d, spur_freq_sd = 0x%x, "
                "spur_delta_phase = 0x%x\n", spur_subchannel_sd,
                spur_freq_sd, spur_delta_phase);
             */

            /* OFDM Spur mitigation */
            OS_REG_RMW_FIELD(ah,
                AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_FILTER, 0x1);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_TIMING11, AR_PHY_TIMING11_SPUR_FREQ_SD, spur_freq_sd);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_TIMING11, AR_PHY_TIMING11_SPUR_DELTA_PHASE,
                spur_delta_phase);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_SFCORR_EXT, AR_PHY_SFCORR_EXT_SPUR_SUBCHANNEL_SD,
                spur_subchannel_sd);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_TIMING11, AR_PHY_TIMING11_USE_SPUR_FILTER_IN_AGC, 0x1);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_TIMING11, AR_PHY_TIMING11_USE_SPUR_FILTER_IN_SELFCOR,
                0x1);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_RSSI, 0x1);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_SPUR_RSSI_THRESH, 34);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_EN_VIT_SPUR_RSSI, 1);

            /*
             * Do not subtract spur power from noise floor for wasp.
             * This causes the maximum client test (on Veriwave) to fail
             * when run on spur channel (2464 MHz).
             * Refer to ev#82746 and ev#82744.
             */
            if (!AR_SREV_WASP(ah) && (OS_REG_READ_FIELD(ah, AR_PHY_MODE,
                                           AR_PHY_MODE_DYNAMIC) == 0x1)) {
                OS_REG_RMW_FIELD(ah, AR_PHY_SPUR_REG,
                    AR_PHY_SPUR_REG_ENABLE_NF_RSSI_SPUR_MIT, 1);
            }

            mask_index = (freq_offset << 4) / 5;
            if (mask_index < 0) {
                mask_index = mask_index - 1;
            }
            mask_index = mask_index & 0x7f;
            /*printf("Bin 0x%x\n", mask_index);*/

            OS_REG_RMW_FIELD(ah,
                AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_ENABLE_MASK_PPM, 0x1);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_PILOT_MASK, 0x1);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_CHAN_MASK, 0x1);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_PILOT_SPUR_MASK,
                AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_IDX_A, mask_index);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_SPUR_MASK_A, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_IDX_A,
                mask_index);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_CHAN_SPUR_MASK,
                AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_IDX_A, mask_index);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_PILOT_SPUR_MASK, AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_A,
                0xc);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_CHAN_SPUR_MASK, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_A,
                0xc);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_SPUR_MASK_A, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_A, 0xa0);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_SPUR_REG, AR_PHY_SPUR_REG_MASK_RATE_CNTL, 0xff);
            /*
            printf("BB_timing_control_4 = 0x%x\n",
                OS_REG_READ(ah, AR_PHY_TIMING4));
            printf("BB_timing_control_11 = 0x%x\n",
                OS_REG_READ(ah, AR_PHY_TIMING11));
            printf("BB_ext_chan_scorr_thr = 0x%x\n",
                OS_REG_READ(ah, AR_PHY_SFCORR_EXT));
            printf("BB_spur_mask_controls = 0x%x\n",
                OS_REG_READ(ah, AR_PHY_SPUR_REG));
            printf("BB_pilot_spur_mask = 0x%x\n",
                OS_REG_READ(ah, AR_PHY_PILOT_SPUR_MASK));
            printf("BB_chan_spur_mask = 0x%x\n",
                OS_REG_READ(ah, AR_PHY_CHAN_SPUR_MASK));
            printf("BB_vit_spur_mask_A = 0x%x\n",
                OS_REG_READ(ah, AR_PHY_SPUR_MASK_A));
             */
            break;
        }
        i++;
    }
}


/*
 * Convert to baseband spur frequency given input channel frequency
 * and compute register settings below.
 */
static void
ar9300_spur_mitigate(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    ar9300_spur_mitigate_ofdm(ah, chan);
    ar9300_spur_mitigate_mrc_cck(ah, chan);
}
#if ATH_SUPPORT_WIFIPOS
/*************************************************************
 * ar9300_lean_channel_change
 * \param hal, HAL_CHANNEL, HAL_CHANNEL_INTERNAL, HAL_HT_MACMODE
 * Functionality:
 *      1) Aborts tx queues
 *      2) Kill  last Baseband Rx Frame
 *      3) Setup 11n MAC/Phy mode registers
 *      4) Change the synth
 *      5) Setup the transmit power values
 *      6) Write spur immunity and delta slope for OFDM enabled
 *          modes (A, G, Turbo)
 *      7) Set all the register for fast channel change and then
 *         wait for HW to remove all the frames from the HW queue
 *         using the Quite collision method. This mechanism will drop
 *         tag all the packets as Xretries and sends them back to the
 *         HW.
 *      8) Wait for SYNTH6 to settle down
 */
#define AR9300_ABORT_LOOPS     1000
#define AR9300_ABORT_WAIT      5
bool
ar9300_lean_channel_change(struct ath_hal *ah,
    HAL_OPMODE opmode, HAL_CHANNEL *chan,
    HAL_HT_MACMODE macmode, u_int8_t txchainmask, u_int8_t rxchainmask)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ath_hal_private  *ap  = AH_PRIVATE(ah);
    HAL_CHANNEL_INTERNAL    *ichan;
    bool stopped;

    u_int32_t nexttbtt, nextdba, tsf_tbtt, tbtt, dba;
    int i , q;
    u_int32_t reg32;

    ahp->ah_tx_chainmask = txchainmask & ap->ah_caps.hal_tx_chain_mask;
    ahp->ah_rx_chainmask = rxchainmask & ap->ah_caps.hal_rx_chain_mask;
    ichan = ar9300_check_chan(ah, chan);
    if(ichan != NULL) {
        ichan->paprd_table_write_done = 0;  /* Clear PAPRD table write flag */
    }
    chan->paprd_table_write_done = 0;  /* Clear PAPRD table write flag */

    if (ichan == AH_NULL) {
        HDPRINTF(ah, HAL_DBG_CHANNEL,
            "%s: invalid channel %u/0x%x; no mapping\n",
            __func__, chan->channel, chan->channel_flags);
        printk("%s: invalid channel %u/0x%x; no mapping\n",
            __func__, chan->channel, chan->channel_flags);
        return false;
    }

    /* Diabling queues and waiting for num_txpending
     * Wait time is used for setting up the other HW
     * registers.
     */
        if (ar9300_get_power_mode(ah) != HAL_PM_FULL_SLEEP) {
        /* Need to stop RX DMA before reset otherwise chip might hang */
        stopped = ar9300_set_rx_abort(ah, true); /* abort and disable PCU */
        ar9300_set_rx_filter(ah, 0);
        stopped &= ar9300_stop_dma_receive(ah, 0); /* stop and disable RX DMA */
        if (!stopped) {
            /*
             * During the transition from full sleep to reset,
             * recv DMA regs are not available to be read
             */
            HDPRINTF(ah, HAL_DBG_UNMASKABLE,
                "%s[%d]: ar9300_stop_dma_receive failed\n", __func__, __LINE__);
            printk("%s: STOP RX fail %d\n", __func__, stopped);
            return false;
        }
    } else {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE,
            "%s[%d]: Chip is already in full sleep\n", __func__, __LINE__);
    }

    ap->nfp = IS_CHAN_2GHZ(chan) ? &ap->nf_2GHz : &ap->nf_5GHz;

    /* beacon Q flush */
    nexttbtt = OS_REG_READ(ah, AR_NEXT_TBTT_TIMER);
    nextdba = OS_REG_READ(ah, AR_NEXT_DMA_BEACON_ALERT);
    tsf_tbtt =  OS_REG_READ(ah, AR_TSF_L32);
    tbtt = tsf_tbtt + NEXT_TBTT_NOW;
    dba = tsf_tbtt;
    OS_REG_WRITE(ah, AR_NEXT_DMA_BEACON_ALERT, dba);
    OS_REG_WRITE(ah, AR_NEXT_TBTT_TIMER, tbtt);

#if ATH_SUPPORT_MCI
    if ((AH_PRIVATE(ah)->ah_caps.hal_mci_support) &&
        (ahp->ah_mci_bt_state == MCI_BT_CAL_START))
    {
        HDPRINTF(ah, HAL_DBG_BT_COEX,
            "(MCI) %s: Stop rx for BT cal.\n", __func__);
        ahp->ah_mci_bt_state = MCI_BT_CAL;
        return true;
    }
#endif

    /*
     * wait on all tx queues
     * This need to be checked in the last to gain extra 50 usec. on avg.
     * Currently checked first since we dont have a
     * previous channel information currently.
     * Which is needed to revert the rf changes.
     */
    for (q = AR_NUM_QCU - 1; q <= 0; q--) {
        for (i = 0; i < AR9300_ABORT_LOOPS; i++) {
            if (!ar9300_num_tx_pending(ah, q)) {
                break;
            }
            OS_DELAY(AR9300_ABORT_WAIT);
        }
        if (i == AR9300_ABORT_LOOPS) {
            printk("%s: ABORT LOOP finsihsed for Q: %d, num_pending: %d \n",
                    __func__, q, ar9300_num_tx_pending(ah, q));
            ath_hal_printf(ah,
                    "ABORT LOOP finsihsed for Q: %d, num_pending: %d \n",
                    q, ar9300_num_tx_pending(ah, q));
            return false;
        }
    }



    /*
     * Kill last Baseband Rx Frame - Request analog bus grant
     */
    OS_REG_WRITE(ah, AR_PHY_RFBUS_REQ, AR_PHY_RFBUS_REQ_EN);

    if (!ath_hal_wait(ah, AR_PHY_RFBUS_GRANT, AR_PHY_RFBUS_GRANT_EN,
            AR_PHY_RFBUS_GRANT_EN, AH_WAIT_TIMEOUT))
    {
       // Wating for lonoger time for RF bus grant to dodge RX baseband stuck
       if (!ath_hal_wait(ah, AR_PHY_RFBUS_GRANT, AR_PHY_RFBUS_GRANT_EN,
                AR_PHY_RFBUS_GRANT_EN, AH_WAIT_TIMEOUT)) {
            printk(KERN_DEBUG"%s:OBS_BUS: %x, WATCH_DOG: %x ", __func__,
                             OS_REG_READ(ah, 0x806c), OS_REG_READ(ah, 0xa7c0));
            HDPRINTF(ah, HAL_DBG_PHY_IO,
                    "%s: Could not kill baseband RX\n", __func__);
            printk("%s: Could not kill baseband RX: %x\n", __func__,
                            OS_REG_READ(ah, AR_PHY_AGC_CONTROL));
            OS_REG_WRITE(ah, AR_PHY_RFBUS_REQ, 0);
            return false;
       }

    }

    /* Updating the beacon alert register with correct value */
    OS_REG_WRITE(ah, AR_NEXT_TBTT_TIMER, nexttbtt);
    OS_REG_WRITE(ah, AR_NEXT_DMA_BEACON_ALERT, nextdba);
    /* Setup 11n MAC/Phy mode registers */
    ar9300_set_11n_regs(ah, chan, macmode);
    /* Change the synth */
    if (!ahp->ah_rf_hal.set_channel(ah, ichan)) {
        HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: failed to set channel\n", __func__);
        printk("%s: failed to set channel\n", __func__);
        return false;
    }
    /*
     * Setup the transmit power values.
     * After the public to private hal channel mapping, ichan contains the
     * valid regulatory power value.
     * ath_hal_getctl and ath_hal_getantennaallowed look up ichan from chan.
     */

    if (ar9300_eeprom_set_transmit_power(
            ah, &ahp->ah_eeprom, ichan, ath_hal_getctl(ah, chan),
            ath_hal_getantennaallowed(ah, chan), ichan->max_reg_tx_power * 2,
            AH_MIN(MAX_RATE_POWER, AH_PRIVATE(ah)->ah_power_limit)) != HAL_OK)
    {
        HDPRINTF(ah, HAL_DBG_EEPROM,
            "%s: error init'ing transmit power\n", __func__);
    /* Updating the beacon alert register with correct value */
        return false;
    }
    /*
     * Write spur immunity and delta slope for OFDM enabled modes (A, G, Turbo)
     */
    if (IS_CHAN_OFDM(chan) || IS_CHAN_HT(chan)) {
        ar9300_set_delta_slope(ah, ichan);
    }

    ar9300_spur_mitigate(ah, chan);
    if (!ichan->one_time_cals_done) {
        /*
         * wait for end of offset and carrier leak cals
         */
        ichan->one_time_cals_done = true;
    }
    /*
     * wait on all tx queues
     */
    for (q = 0; q < AR_NUM_QCU; q++) {
        for (i = 0; i < AR9300_ABORT_LOOPS; i++) {
            if (!ar9300_num_tx_pending(ah, q)) {
                break;
            }
            OS_DELAY(AR9300_ABORT_WAIT);
        }
        if (i == AR9300_ABORT_LOOPS) {
            ath_hal_printf(ah,
                    "ABORT LOOP finsihsed for Q: %d, num_pending: %d \n",
                    q, ar9300_num_tx_pending(ah, q));
            return false;
        }
    }
    /*
     * SYNTH6 register read. No macro definded for the register address
     * hence using the direct address
     */
    for (i = 0; i < 30; i++) {
        reg32 = OS_REG_READ(ah, 0x16094);
        if (SYNTH6__SYNTH_LOCK_VC_OK__READ(reg32)) {
            break;
        }
        OS_DELAY(10);
    }
    if (i == 20) {
        ath_hal_printf(ah, "No SYNTH LOCK \n");
        printk("%s: No SYNTH LOCK \n", __func__);
        return false;
    }
    OS_REG_WRITE(ah, AR_PHY_RFBUS_REQ, 0);

    chan->channel_flags = ichan->channel_flags;
    chan->priv_flags = ichan->priv_flags;
    AH_PRIVATE(ah)->ah_curchan->ah_channel_time = 0;
    AH_PRIVATE(ah)->ah_curchan->ah_tsf_last = ar9300_get_tsf64(ah);
    if (!ichan->one_time_cals_done) {
        /*
         * wait for end of offset and carrier leak cals
         */
        ichan->one_time_cals_done = true;
     }

    if (AH9300(ah)->ah_dma_stuck != true) {
        WAR_USB_DISABLE_PLL_LOCK_DETECT(ah);
        return true;
    } else {
        ath_hal_printf(ah, "ah_dma_stuck failed \n");
        printk("%s: ah_dma_stuck failed \n", __func__);
        return false;
    }
    return true;


}
#endif


/**************************************************************
 * ar9300_channel_change
 * Assumes caller wants to change channel, and not reset.
 */
static inline bool
ar9300_channel_change(struct ath_hal *ah, HAL_CHANNEL *chan,
    HAL_CHANNEL_INTERNAL *ichan, HAL_HT_MACMODE macmode)
{
#if ATH_SUPPORT_FAST_CC
#define   CHANNEL_MODE   (CHANNEL_OFDM|CHANNEL_CCK| CHANNEL_TURBO | CHANNEL_HT20 | CHANNEL_HT40PLUS | CHANNEL_HT40MINUS | CHANNEL_HALF|CHANNEL_QUARTER)
#endif

    u_int32_t synth_delay, qnum;
    struct ath_hal_9300 *ahp = AH9300(ah);
#if ATH_SUPPORT_FAST_CC
    bool   bandswitch, modediff;
    bool   ini_reloaded = false;
#endif

    /* TX must be stopped by now */
    for (qnum = 0; qnum < AR_NUM_QCU; qnum++) {
        if (ar9300_num_tx_pending(ah, qnum)) {
            HDPRINTF(ah, HAL_DBG_QUEUE,
                "%s: Transmit frames pending on queue %d\n", __func__, qnum);
            HALASSERT(0);
            return false;
        }
    }

#if ATH_SUPPORT_FAST_CC
    bandswitch = ((ichan->channel_flags & (CHANNEL_2GHZ|CHANNEL_5GHZ)) !=
                     (AH_PRIVATE(ah)->ah_curchan->channel_flags & (CHANNEL_2GHZ|CHANNEL_5GHZ)));

    modediff = ((ichan->channel_flags & CHANNEL_MODE) !=
                     (AH_PRIVATE(ah)->ah_curchan->channel_flags & CHANNEL_MODE));
#endif

    /*
     * Kill last Baseband Rx Frame - Request analog bus grant
     */
    OS_REG_WRITE(ah, AR_PHY_RFBUS_REQ, AR_PHY_RFBUS_REQ_EN);
    if (!ath_hal_wait(ah, AR_PHY_RFBUS_GRANT, AR_PHY_RFBUS_GRANT_EN,
            AR_PHY_RFBUS_GRANT_EN, AH_WAIT_TIMEOUT))
    {
        HDPRINTF(ah, HAL_DBG_PHY_IO,
            "%s: Could not kill baseband RX\n", __func__);
        return false;
    }

#if ATH_SUPPORT_FAST_CC
    if (bandswitch || modediff) {
        int     reg_writes;
        u_int   modes_index, modes_txgaintable_index = 0;

        /* Disable BB_active */
        OS_REG_WRITE(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_DIS);
        OS_DELAY(5);

        if (bandswitch) {
            ar9300_init_pll(ah, chan);
        }

        switch (ichan->channel_flags & CHANNEL_ALL) {
            case CHANNEL_A:
            case CHANNEL_A_HT20:
			    if (AR_SREV_SCORPION(ah)){
			        if (chan->channel <= 5350){
			            modes_txgaintable_index = 1;
			        }else if ((chan->channel > 5350) && (chan->channel <= 5600)){
			            modes_txgaintable_index = 3;
			        }else if (chan->channel > 5600){
			            modes_txgaintable_index = 5;
			        }
			    }
                modes_index = 1;
                break;
            case CHANNEL_A_HT40PLUS:
            case CHANNEL_A_HT40MINUS:
		        if (AR_SREV_SCORPION(ah)){
		            if (chan->channel <= 5350){
		                modes_txgaintable_index = 2;
		            }else if ((chan->channel > 5350) && (chan->channel <= 5600)){
		                modes_txgaintable_index = 4;
		            }else if (chan->channel > 5600){
		                modes_txgaintable_index = 6;
		            }
		        }
                modes_index = 2;
                break;
            case CHANNEL_PUREG:
            case CHANNEL_G_HT20:
            case CHANNEL_B:
		        if (AR_SREV_SCORPION(ah)){
		            modes_txgaintable_index = 8;
		        }else if (AR_SREV_HONEYBEE(ah)){
			        modes_txgaintable_index = 1;
		        }else if (AR_SREV_DRAGONFLY(ah)){
			        modes_txgaintable_index = 2;
		        }
                modes_index = 4;
                break;
            case CHANNEL_G_HT40PLUS:
            case CHANNEL_G_HT40MINUS:
		        if (AR_SREV_SCORPION(ah)){
		            modes_txgaintable_index = 7;
				}else if (AR_SREV_HONEYBEE(ah)){
					modes_txgaintable_index = 1;
				}else if (AR_SREV_DRAGONFLY(ah)){
					modes_txgaintable_index = 1;
				}
                modes_index = 3;
                break;
            case CHANNEL_108G:
                modes_index = 5;
                break;
            default:
                HALASSERT(0);
                return HAL_EINVAL;
        }

        if(modes_index != ahp->ah_mode_index) {
            /* Osprey 2.0+ - new INI format.  Each subsystem has a pre, core, and post array. */
            ar9300_prog_ini(ah, &ahp->ah_ini_soc[ATH_INI_POST], modes_index);
            ar9300_prog_ini(ah, &ahp->ah_ini_mac[ATH_INI_POST], modes_index);
            ar9300_prog_ini(ah, &ahp->ah_ini_bb[ATH_INI_POST], modes_index);
            ar9300_prog_ini(ah, &ahp->ah_ini_radio[ATH_INI_POST], modes_index);
            if (AR_SREV_JUPITER_20(ah) || AR_SREV_APHRODITE(ah)) {
                ar9300_prog_ini(ah, &ahp->ah_ini_radio_post_sys2ant, modes_index);
            }


            /* Write txgain Array Parameters */
            reg_writes = 0;
			if (AR_SREV_SCORPION(ah) || AR_SREV_HONEYBEE(ah)|| AR_SREV_DRAGONFLY(ah)) {
				REG_WRITE_ARRAY(&ahp->ah_ini_modes_txgain, modes_txgaintable_index,
					reg_writes);
			}else{
				REG_WRITE_ARRAY(&ahp->ah_ini_modes_txgain, modes_index, reg_writes);
			}

            /* For 5GHz channels requiring Fast Clock, apply different modal values */
            if (IS_5GHZ_FAST_CLOCK_EN(ah, chan)) {
                REG_WRITE_ARRAY(&ahp->ah_ini_modes_additional, modes_index, reg_writes);
            }

            if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) || AR_SREV_APHRODITE(ah)) {
                REG_WRITE_ARRAY(
                    &ahp->ah_ini_modes_additional, 1/*modes_index*/, reg_writes);
            }

            if (AR_SREV_WASP(ah) && (AH9300(ah)->clk_25mhz == 0)) {
                REG_WRITE_ARRAY(
                    &ahp->ah_ini_modes_additional_40mhz, 1/*modesIndex*/, reg_writes);
            }

            if (2484 == ichan->channel) {
                ar9300_prog_ini(ah, &ahp->ah_ini_japan2484, 1);
            }

			if (AR_SREV_DRAGONFLY(ah) && (ar9300_rx_gain_index_get(ah) == 0)) {
				/* Write xlna Array */
				REG_WRITE_ARRAY(&ahp->ah_ini_xlna, modes_index, reg_writes);
			}

            ahp->ah_mode_index = modes_index;
            ini_reloaded = true;
        }

        ar9300_set_rf_mode(ah, chan);
    }
#endif

    /* Setup 11n MAC/Phy mode registers */
    ar9300_set_11n_regs(ah, chan, macmode);

    /*
     * Change the synth
     */
    if (!ahp->ah_rf_hal.set_channel(ah, ichan)) {
        HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: failed to set channel\n", __func__);
        return false;
    }

    /*
     * Some registers get reinitialized during ATH_INI_POST INI programming.
     */
    ar9300_init_user_settings(ah);

    /*
     * Setup the transmit power values.
     *
     * After the public to private hal channel mapping, ichan contains the
     * valid regulatory power value.
     * ath_hal_getctl and ath_hal_getantennaallowed look up ichan from chan.
     */
    if (ar9300_eeprom_set_transmit_power(
         ah, &ahp->ah_eeprom, ichan, ath_hal_getctl(ah, chan),
         ath_hal_getantennaallowed(ah, chan),
         ath_hal_get_twice_max_regpower(AH_PRIVATE(ah), ichan, chan),
         AH_MIN(MAX_RATE_POWER, AH_PRIVATE(ah)->ah_power_limit)) != HAL_OK)
    {
        HDPRINTF(ah, HAL_DBG_EEPROM,
            "%s: error init'ing transmit power\n", __func__);
        return false;
    }

    /*
     * Release the RFBus Grant.
     */
    OS_REG_WRITE(ah, AR_PHY_RFBUS_REQ, 0);

    /*
     * Write spur immunity and delta slope for OFDM enabled modes (A, G, Turbo)
     */
    if (IS_CHAN_OFDM(chan) || IS_CHAN_HT(chan)) {
        ar9300_set_delta_slope(ah, ichan);
    } else {
        /* Set to Ini default */
        OS_REG_WRITE(ah, AR_PHY_TIMING3, 0x9c0a9f6b);
        OS_REG_WRITE(ah, AR_PHY_SGI_DELTA, 0x00046384);
    }

    ar9300_spur_mitigate(ah, chan);

#if ATH_SUPPORT_FAST_CC
    if (bandswitch || ini_reloaded) {
        ar9300_eeprom_set_board_values(ah, ichan);
    }

    if (bandswitch || modediff) {
        OS_REG_WRITE(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);
    }
#endif /* ATH_SUPPORT_FAST_CC */

    /*
     * Wait for the frequency synth to settle (synth goes on via PHY_ACTIVE_EN).
     * Read the phy active delay register. Value is in 100ns increments.
     */
    synth_delay = OS_REG_READ(ah, AR_PHY_RX_DELAY) & AR_PHY_RX_DELAY_DELAY;
    if (IS_CHAN_CCK(chan)) {
        synth_delay = (4 * synth_delay) / 22;
    } else {
        synth_delay /= 10;
    }

    OS_DELAY(synth_delay + BASE_ACTIVATE_DELAY);

    /*
     * Do calibration.
     */
#if ATH_SUPPORT_FAST_CC
    if (
#if ATH_SUPPORT_RADIO_RETENTION
        //ahp->radio_retention_enable ||
#endif
        bandswitch || ini_reloaded)
    {
        ar9300_init_cal(ah, chan, true, false);
    }
#endif /* ATH_SUPPORT_FAST_CC */

#if ATH_SUPPORT_FAST_CC
#undef CHANNEL_MODE
#endif
    return true;
}

void
ar9300_set_operating_mode(struct ath_hal *ah, int opmode)
{
    u_int32_t val;

    val = OS_REG_READ(ah, AR_STA_ID1);
    val &= ~(AR_STA_ID1_STA_AP | AR_STA_ID1_ADHOC);
    switch (opmode) {
    case HAL_M_HOSTAP:
        OS_REG_WRITE(ah, AR_STA_ID1,
            val | AR_STA_ID1_STA_AP | AR_STA_ID1_KSRCH_MODE);
        OS_REG_CLR_BIT(ah, AR_CFG, AR_CFG_AP_ADHOC_INDICATION);
        break;
    case HAL_M_IBSS:
        OS_REG_WRITE(ah, AR_STA_ID1,
            val | AR_STA_ID1_ADHOC | AR_STA_ID1_KSRCH_MODE);
        OS_REG_SET_BIT(ah, AR_CFG, AR_CFG_AP_ADHOC_INDICATION);
        break;
    case HAL_M_STA:
    case HAL_M_MONITOR:
        OS_REG_WRITE(ah, AR_STA_ID1, val | AR_STA_ID1_KSRCH_MODE);
        break;
    }
}

/* XXX need the logic for Osprey */
inline void
ar9300_init_pll(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t pll;
    u_int8_t clk_25mhz = AH9300(ah)->clk_25mhz;

    if (AR_SREV_HORNET(ah)) {
        if (clk_25mhz) {
            /* Hornet uses PLL_CONTROL_2. Xtal is 25MHz for Hornet.
             * REFDIV set to 0x1.
             * $xtal_freq = 25;
             * $PLL2_div = (704/$xtal_freq); # 176 * 4 = 704.
             * MAC and BB run at 176 MHz.
             * $PLL2_divint = int($PLL2_div);
             * $PLL2_divfrac = $PLL2_div - $PLL2_divint;
             * $PLL2_divfrac = int($PLL2_divfrac * 0x4000); # 2^14
             * $PLL2_Val = ($PLL2_divint & 0x3f) << 19 | (0x1) << 14 |
             *     $PLL2_divfrac & 0x3fff;
             * Therefore, $PLL2_Val = 0xe04a3d
             */
#define DPLL2_KD_VAL            0x1D
#define DPLL2_KI_VAL            0x06
#define DPLL3_PHASE_SHIFT_VAL   0x1

            /* Rewrite DDR PLL2 and PLL3 */
            /* program DDR PLL ki and kd value, ki=0x6, kd=0x1d */
            OS_REG_WRITE(ah, AR_HORNET_CH0_DDR_DPLL2, 0x18e82f01);

            /* program DDR PLL phase_shift to 0x1 */
            OS_REG_RMW_FIELD(ah, AR_HORNET_CH0_DDR_DPLL3,
                AR_PHY_BB_DPLL3_PHASE_SHIFT, DPLL3_PHASE_SHIFT_VAL);

            OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x1142c);
            OS_DELAY(1000);

            /* program refdiv, nint, frac to RTC register */
            OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL2, 0xe04a3d);

            /* program BB PLL ki and kd value, ki=0x6, kd=0x1d */
            OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
                AR_PHY_BB_DPLL2_KD, DPLL2_KD_VAL);
            OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
                AR_PHY_BB_DPLL2_KI, DPLL2_KI_VAL);

            /* program BB PLL phase_shift to 0x1 */
            OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL3,
                AR_PHY_BB_DPLL3_PHASE_SHIFT, DPLL3_PHASE_SHIFT_VAL);
        } else { /* 40MHz */
#undef  DPLL2_KD_VAL
#undef  DPLL2_KI_VAL
#define DPLL2_KD_VAL            0x3D
#define DPLL2_KI_VAL            0x06
            /* Rewrite DDR PLL2 and PLL3 */
            /* program DDR PLL ki and kd value, ki=0x6, kd=0x3d */
            OS_REG_WRITE(ah, AR_HORNET_CH0_DDR_DPLL2, 0x19e82f01);

            /* program DDR PLL phase_shift to 0x1 */
            OS_REG_RMW_FIELD(ah, AR_HORNET_CH0_DDR_DPLL3,
                AR_PHY_BB_DPLL3_PHASE_SHIFT, DPLL3_PHASE_SHIFT_VAL);

            OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x1142c);
            OS_DELAY(1000);

            /* program refdiv, nint, frac to RTC register */
            OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL2, 0x886666);

            /* program BB PLL ki and kd value, ki=0x6, kd=0x3d */
            OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
                AR_PHY_BB_DPLL2_KD, DPLL2_KD_VAL);
            OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
                AR_PHY_BB_DPLL2_KI, DPLL2_KI_VAL);

            /* program BB PLL phase_shift to 0x1 */
            OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL3,
                AR_PHY_BB_DPLL3_PHASE_SHIFT, DPLL3_PHASE_SHIFT_VAL);
        }
        OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x142c);
        OS_DELAY(1000);
    } else if (AR_SREV_POSEIDON(ah) || AR_SREV_APHRODITE(ah)) {
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2, AR_PHY_BB_DPLL2_PLL_PWD, 0x1);

        /* program BB PLL ki and kd value, ki=0x4, kd=0x40 */
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
            AR_PHY_BB_DPLL2_KD, 0x40);
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
            AR_PHY_BB_DPLL2_KI, 0x4);

        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL1,
            AR_PHY_BB_DPLL1_REFDIV, 0x5);
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL1,
            AR_PHY_BB_DPLL1_NINI, 0x58);
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL1,
            AR_PHY_BB_DPLL1_NFRAC, 0x0);

        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
            AR_PHY_BB_DPLL2_OUTDIV, 0x1);
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
            AR_PHY_BB_DPLL2_LOCAL_PLL, 0x1);
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
            AR_PHY_BB_DPLL2_EN_NEGTRIG, 0x1);

        /* program BB PLL phase_shift to 0x6 */
        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL3,
            AR_PHY_BB_DPLL3_PHASE_SHIFT, 0x6);

        OS_REG_RMW_FIELD(ah, AR_PHY_BB_DPLL2,
            AR_PHY_BB_DPLL2_PLL_PWD, 0x0);
        OS_DELAY(1000);

        OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x142c);
        OS_DELAY(1000);
    } else if (AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_HONEYBEE(ah) || AR_SREV_DRAGONFLY(ah)) {
#define SRIF_PLL 1
        u_int32_t regdata, pll2_divint, pll2_divfrac;

#ifndef SRIF_PLL
	u_int32_t pll2_clkmode;
#endif

#ifdef SRIF_PLL
        u_int32_t refdiv;
#endif
        if (clk_25mhz) {
#ifndef SRIF_PLL
            pll2_divint = 0x1c;
            pll2_divfrac = 0xa3d7;
#else
            if (AR_SREV_HONEYBEE(ah) || AR_SREV_DRAGONFLY(ah)) {
                pll2_divint = 0x1c;
                pll2_divfrac = 0xa3d2;
                refdiv = 1;
            } else {
                pll2_divint = 0x54;
                pll2_divfrac = 0x1eb85;
                refdiv = 3;
            }
#endif
        } else {
#ifndef SRIF_PLL
            pll2_divint = 0x11;
            pll2_divfrac = 0x26666;
#else
            if (AR_SREV_WASP(ah)) {
                pll2_divint = 88;
                pll2_divfrac = 0;
                refdiv = 5;
			} else if (AR_SREV_HONEYBEE(ah) || AR_SREV_DRAGONFLY(ah)) {
                pll2_divint = 0x11;
                pll2_divfrac = 0x26665;
                refdiv = 1;
            } else {
                pll2_divint = 0x11;
                pll2_divfrac = 0x26666;
                refdiv = 1;
            }
#endif
        }
#ifndef SRIF_PLL
        pll2_clkmode = 0x3d;
#endif
        /* PLL programming through SRIF Local Mode */
		if (AR_SREV_DRAGONFLY(ah)) {
			OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x850002c); /* Bypass mode */
		} else {
            OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x1142c); /* Bypass mode */
		}
        OS_DELAY(1000);
        do {
            regdata = OS_REG_READ(ah, AR_PHY_PLL_MODE);
            if (AR_SREV_HONEYBEE(ah)||AR_SREV_DRAGONFLY(ah)) {
                regdata = regdata | (0x1 << 22);
            } else {
                regdata = regdata | (0x1 << 16);
            }
            OS_REG_WRITE(ah, AR_PHY_PLL_MODE, regdata); /* PWD_PLL set to 1 */
            OS_DELAY(100);
            /* override int, frac, refdiv */
#ifndef SRIF_PLL
            OS_REG_WRITE(ah, AR_PHY_PLL_CONTROL,
                ((1 << 27) | (pll2_divint << 18) | pll2_divfrac));
#else
            OS_REG_WRITE(ah, AR_PHY_PLL_CONTROL,
                ((refdiv << 27) | (pll2_divint << 18) | pll2_divfrac));
#endif
            OS_DELAY(100);
            regdata = OS_REG_READ(ah, AR_PHY_PLL_MODE);
#ifndef SRIF_PLL
            regdata = (regdata & 0x80071fff) |
                (0x1 << 30) | (0x1 << 13) | (0x6 << 26) | (pll2_clkmode << 19);
#else
            if (AR_SREV_WASP(ah)) {
                regdata = (regdata & 0x80071fff) |
                    (0x1 << 30) | (0x1 << 13) | (0x4 << 26) | (0x18 << 19);
            } else if (AR_SREV_HONEYBEE(ah)) {
                /*
                 * Kd=10, Ki=2, Outdiv=1, Local PLL=0, Phase Shift=4
                 */
                regdata = (regdata & 0x01c00fff) |
                    (0x1 << 31) | (0x2 << 29) | (0xa << 25) | (0x1 << 19) | (0x6 << 12);
			} else if (AR_SREV_DRAGONFLY(ah)) {
				/*
				 * Kd=10, Ki=2, Outdiv=1, Local PLL=0, Phase Shift=0
				 */
				regdata = (regdata & 0x01c00fff) |
					(0x1 << 31) | (0x2 << 29) | (0xa << 25) | (0x1 << 19);
            } else {
                regdata = (regdata & 0x80071fff) |
                    (0x3 << 30) | (0x1 << 13) | (0x4 << 26) | (0x60 << 19);
            }
#endif
            /* Ki, Kd, Local PLL, Outdiv */
            OS_REG_WRITE(ah, AR_PHY_PLL_MODE, regdata);
            regdata = OS_REG_READ(ah, AR_PHY_PLL_MODE);
            if (AR_SREV_HONEYBEE(ah)||AR_SREV_DRAGONFLY(ah)) {
                regdata = (regdata & 0xffbfffff);
            } else {
                regdata = (regdata & 0xfffeffff);
            }
            OS_REG_WRITE(ah, AR_PHY_PLL_MODE, regdata); /* PWD_PLL set to 0 */
            OS_DELAY(1000);
            if (AR_SREV_WASP(ah)) {
                /* clear do measure */
                regdata = OS_REG_READ(ah, AR_PHY_PLL_BB_DPLL3);
                regdata &= ~(1 << 30);
                OS_REG_WRITE(ah, AR_PHY_PLL_BB_DPLL3, regdata);
                OS_DELAY(100);

                /* set do measure */
                regdata = OS_REG_READ(ah, AR_PHY_PLL_BB_DPLL3);
                regdata |= (1 << 30);
                OS_REG_WRITE(ah, AR_PHY_PLL_BB_DPLL3, regdata);

                /* wait for measure done */
                do {
                    regdata = OS_REG_READ(ah, AR_PHY_PLL_BB_DPLL4);
                } while ((regdata & (1 << 3)) == 0);

                /* clear do measure */
                regdata = OS_REG_READ(ah, AR_PHY_PLL_BB_DPLL3);
                regdata &= ~(1 << 30);
                OS_REG_WRITE(ah, AR_PHY_PLL_BB_DPLL3, regdata);

                /* get measure sqsum dvc */
                regdata = (OS_REG_READ(ah, AR_PHY_PLL_BB_DPLL3) & 0x007FFFF8) >> 3;
            } else {
                break;
            }
        } while (regdata >= 0x40000);

		if (AR_SREV_DRAGONFLY(ah)) {
			/* Remove from Bypass mode */
			OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x50002c);
		} else {
            /* Remove from Bypass mode */
            OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, 0x142c);
		}
        OS_DELAY(1000);
    } else {
        pll = SM(0x5, AR_RTC_PLL_REFDIV);

        /* Supposedly not needed on Osprey */
#if 0
        if (chan && IS_CHAN_HALF_RATE(chan)) {
            pll |= SM(0x1, AR_RTC_PLL_CLKSEL);
        } else if (chan && IS_CHAN_QUARTER_RATE(chan)) {
            pll |= SM(0x2, AR_RTC_PLL_CLKSEL);
        }
#endif
        if (chan && IS_CHAN_5GHZ(chan)) {
            pll |= SM(0x28, AR_RTC_PLL_DIV);
            /*
             * When doing fast clock, set PLL to 0x142c
             */
            if (IS_5GHZ_FAST_CLOCK_EN(ah, chan)) {
                pll = 0x142c;
            }
        } else {
            pll |= SM(0x2c, AR_RTC_PLL_DIV);
        }

        OS_REG_WRITE(ah, AR_RTC_PLL_CONTROL, pll);
    }

    /* TODO:
     * For multi-band owl, switch between bands by reiniting the PLL.
     */
    OS_DELAY(RTC_PLL_SETTLE_DELAY);

    OS_REG_WRITE(ah, AR_RTC_SLEEP_CLK,
        AR_RTC_FORCE_DERIVED_CLK | AR_RTC_PCIE_RST_PWDN_EN);

}

static inline bool
ar9300_set_reset(struct ath_hal *ah, int type)
{
    u_int32_t rst_flags;
    u_int32_t tmp_reg;
    struct ath_hal_9300 *ahp = AH9300(ah);
#ifdef AH_SUPPORT_SCORPION
    u_int64_t tsf = 0;
    int i = 0;
    u_int8_t reset_delay = 100;
#endif

    HALASSERT(type == HAL_RESET_WARM || type == HAL_RESET_COLD);

    /*
     * RTC Force wake should be done before resetting the MAC.
     * MDK/ART does it that way.
     */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_WA), AH9300(ah)->ah_wa_reg_val);
    OS_DELAY(10); /* delay to allow AR_WA reg write to kick in */
    OS_REG_WRITE(ah,
        AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN | AR_RTC_FORCE_WAKE_ON_INT);

    /* Reset AHB */
    /* Bug26871 */
    tmp_reg = OS_REG_READ(ah, AR_HOSTIF_REG(ah, AR_INTR_SYNC_CAUSE));
    if (AR_SREV_WASP(ah)) {
        if (tmp_reg & (AR9340_INTR_SYNC_LOCAL_TIMEOUT)) {
            OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_SYNC_ENABLE), 0);
            OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_RC), AR_RC_HOSTIF);
        }
    } else {
        if (tmp_reg & (AR9300_INTR_SYNC_LOCAL_TIMEOUT | AR9300_INTR_SYNC_RADM_CPL_TIMEOUT)) {
            OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_SYNC_ENABLE), 0);
            OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_RC), AR_RC_HOSTIF);
        }
        else {
            /* NO AR_RC_AHB in Osprey */
            /*OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_RC), AR_RC_AHB);*/
        }
    }

    rst_flags = AR_RTC_RC_MAC_WARM;
#if !defined(AR9550_EMULATION) && !defined(AR9530_EMULATION)
    if (type == HAL_RESET_COLD) {
        rst_flags |= AR_RTC_RC_MAC_COLD;
#ifdef AH_SUPPORT_SCORPION
        if (AR_SREV_SCORPION(ah)) {
            reset_delay = 200;
        }
#endif
    }
#endif

#ifdef AH_SUPPORT_HORNET
    /* Hornet WAR: trigger SoC to reset WMAC if ...
     * (1) doing cold reset. Ref: EV 69254
     * (2) beacon pending. Ref: EV 70983
     */
    if (AR_SREV_HORNET(ah) &&
        (ar9300_num_tx_pending(
            ah, AH_PRIVATE(ah)->ah_caps.hal_total_queues - 1) != 0 ||
         type == HAL_RESET_COLD))
    {
        u_int32_t time_out;
#define AR_SOC_RST_RESET         0xB806001C
#define AR_SOC_BOOT_STRAP        0xB80600AC
#define AR_SOC_WLAN_RST          0x00000800 /* WLAN reset */
#define REG_WRITE(_reg, _val)    *((volatile u_int32_t *)(_reg)) = (_val);
#define REG_READ(_reg)           *((volatile u_int32_t *)(_reg))
        HDPRINTF(ah, HAL_DBG_RESET, "%s: Hornet SoC reset WMAC.\n", __func__);

#ifdef ART_BUILD
        MyRegisterRead(AR_SOC_RST_RESET, &tmp_reg);
        MyRegisterWrite(AR_SOC_RST_RESET, tmp_reg | AR_SOC_WLAN_RST);
        MyRegisterRead(AR_SOC_RST_RESET, &tmp_reg);
        MyRegisterWrite(AR_SOC_RST_RESET, tmp_reg & (~AR_SOC_WLAN_RST));
        time_out = 0;
        while (1) {
            MyRegisterRead(AR_SOC_BOOT_STRAP, &tmp_reg);
            if ((tmp_reg & 0x10) != 0) {
                break;
            }
            if (tmp_reg > 0x100) {
                break;
            }
            time_out++;
        }
#else
        REG_WRITE(AR_SOC_RST_RESET,
            REG_READ(AR_SOC_RST_RESET) | AR_SOC_WLAN_RST);
        REG_WRITE(AR_SOC_RST_RESET,
            REG_READ(AR_SOC_RST_RESET) & (~AR_SOC_WLAN_RST));

        time_out = 0;

        while (1) {
            tmp_reg = REG_READ(AR_SOC_BOOT_STRAP);
            if ((tmp_reg & 0x10) == 0) {
                break;
            }
            if (time_out > 20) {
                break;
            }
            OS_DELAY(10000);
            time_out++;
        }

#endif /* ART_BUILD */
        OS_REG_WRITE(ah, AR_RTC_RESET, 1);
#undef REG_READ
#undef REG_WRITE
#undef AR_SOC_WLAN_RST
#undef AR_SOC_RST_RESET
#undef AR_SOC_BOOT_STRAP
    }
#endif /* AH_SUPPORT_HORNET */

#ifdef AH_SUPPORT_SCORPION
    if (AR_SREV_SCORPION(ah)) {
#define DDR_CTL_CONFIG_ADDRESS                                       0xb8000000
#define DDR_CTL_CONFIG_OFFSET                                        0x0108
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_RESERVED_MSB                  29
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_RESERVED_LSB                  29
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_RESERVED_MASK                 0x20000000
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_CPU_MSB                       28
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_CPU_LSB                       28
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_CPU_MASK                      0x10000000
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_GMAC0_MSB                     27
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_GMAC0_LSB                     27
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_GMAC0_MASK                    0x08000000
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_GMAC1_MSB                     26
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_GMAC1_LSB                     26
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_GMAC1_MASK                    0x04000000
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_USB1_I2S_NAND_MSB             25
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_USB1_I2S_NAND_LSB             25
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_USB1_I2S_NAND_MASK            0x02000000
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_PCIE1_MSB                     24
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_PCIE1_LSB                     24
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_PCIE1_MASK                    0x01000000
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_WMAC_MSB                      23
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_WMAC_LSB                      23
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_WMAC_MASK                     0x00800000
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_PCIE2_MSB                     22
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_PCIE2_LSB                     22
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_PCIE2_MASK                    0x00400000
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_USB2_CSUM_MSB                 21
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_USB2_CSUM_LSB                 21
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_USB2_CSUM_MASK                0x00200000
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_MSB                           29
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_LSB                           21
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_MASK                          0x3fe00000
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_GET(x)                        (((x) & DDR_CTL_CONFIG_CLIENT_ACTIVITY_MASK) >> DDR_CTL_CONFIG_CLIENT_ACTIVITY_LSB)
#define DDR_CTL_CONFIG_CLIENT_ACTIVITY_SET(x)                        (((x) << DDR_CTL_CONFIG_CLIENT_ACTIVITY_LSB) & DDR_CTL_CONFIG_CLIENT_ACTIVITY_MASK)
#define MAC_DMA_CFG_ADDRESS                                          0xb8100000
#define MAC_DMA_CFG_OFFSET                                           0x0014

#define MAC_DMA_CFG_HALT_REQ_MSB                                     11
#define MAC_DMA_CFG_HALT_REQ_LSB                                     11
#define MAC_DMA_CFG_HALT_REQ_MASK                                    0x00000800
#define MAC_DMA_CFG_HALT_REQ_GET(x)                                  (((x) & MAC_DMA_CFG_HALT_REQ_MASK) >> MAC_DMA_CFG_HALT_REQ_LSB)
#define MAC_DMA_CFG_HALT_REQ_SET(x)                                  (((x) << MAC_DMA_CFG_HALT_REQ_LSB) & MAC_DMA_CFG_HALT_REQ_MASK)
#define MAC_DMA_CFG_HALT_ACK_MSB                                     12
#define MAC_DMA_CFG_HALT_ACK_LSB                                     12
#define MAC_DMA_CFG_HALT_ACK_MASK                                    0x00001000
#define MAC_DMA_CFG_HALT_ACK_GET(x)                                  (((x) & MAC_DMA_CFG_HALT_ACK_MASK) >> MAC_DMA_CFG_HALT_ACK_LSB)
#define MAC_DMA_CFG_HALT_ACK_SET(x)                                  (((x) << MAC_DMA_CFG_HALT_ACK_LSB) & MAC_DMA_CFG_HALT_ACK_MASK)

#define RST_RESET                                                    0xB806001c
#define RTC_RESET                                                    (1<<27)

#define REG_READ(_reg)          *((volatile u_int32_t *)(_reg))
#define REG_WRITE(_reg, _val)   *((volatile u_int32_t *)(_reg)) = (_val);

#define DDR_REG_READ(_ah, _reg) \
	    *((volatile u_int32_t *)( DDR_CTL_CONFIG_ADDRESS + (_reg)))
#define DDR_REG_WRITE(_ah, _reg, _val) \
	    *((volatile u_int32_t *)(DDR_CTL_CONFIG_ADDRESS + (_reg))) = (_val)

	    OS_REG_WRITE(ah,MAC_DMA_CFG_OFFSET, (OS_REG_READ(ah,MAC_DMA_CFG_OFFSET) & ~MAC_DMA_CFG_HALT_REQ_MASK) |
			    MAC_DMA_CFG_HALT_REQ_SET(1));

	    {
		    int count;
            u_int32_t data, usb_mask;
		    count = 0;
		    while (!MAC_DMA_CFG_HALT_ACK_GET(OS_REG_READ(ah, MAC_DMA_CFG_OFFSET) ))
		    {
			    count++;
			    if (count > 10) {
				    ath_hal_printf(ah, "Halt ACK timeout\n");
				    break;
			    }
			    OS_DELAY(10);
		    }
#ifdef MDK_AP
			    data = FullAddrRead(DDR_CTL_CONFIG_ADDRESS + DDR_CTL_CONFIG_OFFSET);
#else
			    data = DDR_REG_READ(ah,DDR_CTL_CONFIG_OFFSET);
#endif

		    HDPRINTF(ah,HAL_DBG_RESET, "check DDR Activity - HIGH\n");

		    count = 0;
                    usb_mask = DDR_CTL_CONFIG_CLIENT_ACTIVITY_USB1_I2S_NAND_MASK |
                                DDR_CTL_CONFIG_CLIENT_ACTIVITY_USB2_CSUM_MASK;
                    data &= ~usb_mask;
		    while (DDR_CTL_CONFIG_CLIENT_ACTIVITY_GET(data)) {
			    //      AVE_DEBUG(0,"DDR Activity - HIGH\n");
			    ath_hal_printf(ah, "DDR Activity - HIGH\n");
			    count++;
			    OS_DELAY(10);
#ifdef MDK_AP
			    data = FullAddrRead(DDR_CTL_CONFIG_ADDRESS + DDR_CTL_CONFIG_OFFSET);
#else
			    data = DDR_REG_READ(ah,DDR_CTL_CONFIG_OFFSET);
#endif
                            data &= ~usb_mask;
			    if (count > 10) {
				    ath_hal_printf(ah, "DDR Activity timeout\n");
				    break;
			    }
		    }
	    }


	    {
		    //Force RTC reset
                  /* For chips on which RTC reset is done, save TSF before it gets cleared */
		    tsf = ar9300_get_tsf64(ah);
#ifdef MDK_AP
		    FullAddrWrite(RST_RESET, (FullAddrRead(RST_RESET) | RTC_RESET));
		    OS_DELAY(10);
		    FullAddrWrite(RST_RESET, (FullAddrRead(RST_RESET) & ~RTC_RESET));
		    OS_DELAY(10);
#else
		    REG_WRITE(RST_RESET, (REG_READ(RST_RESET) | RTC_RESET));
		    OS_DELAY(10);
		    REG_WRITE(RST_RESET, (REG_READ(RST_RESET) & ~RTC_RESET));
		    OS_DELAY(10);
#endif
                    //only for scorpion as RTC reset need to wait for bit AR_RTC_STATUS_ON set
                    // on register AR_RTC_STATUS
		    OS_REG_WRITE(ah, AR_RTC_RESET, 0);
		    OS_DELAY(10);
		    OS_REG_WRITE(ah, AR_RTC_RESET, 1);
                    do{
                       OS_DELAY(100);
                       tsf += 100;
                       i++;
                    } while ( ( (OS_REG_READ(ah, AR_RTC_STATUS) & AR_RTC_STATUS_ON)!=AR_RTC_STATUS_ON) && i < 30);

                    if (i > 30)
                      printk("AR_RTC_STATUS is not equal to AR_RTC_STATUS_ON\r\n");

		    HDPRINTF(ah,HAL_DBG_RESET,"Scorpion SoC RTC reset done.\n");
	    }
#undef REG_READ
#undef REG_WRITE
    }
#endif  /* AH_SUPPORT_SCORPION */

       /* DMA HALT Added to resolve Peacock and Osprey BUS error during RTC_RC reg read */
    if (AR_SREV_OSPREY(ah)) {
         OS_REG_SET_BIT(ah,AR_CFG,AR_CFG_HALT_REQ);
        {
            int count;
            count = 0;
            while (!OS_REG_IS_BIT_SET(ah,AR_CFG,AR_CFG_HALT_ACK)) {
                count++;
                if (count > 20) {
                    printk("\n %s Halt ACK timeout = AR_CFG = 0x%x",__func__,OS_REG_READ(ah, AR_CFG));
                    break;
                }
                OS_DELAY(10); //only for osprey
#ifdef AH_SUPPORT_SCORPION
                //for scorpion to resotre the tsf value
                if (AR_SREV_SCORPION(ah)) {
                    tsf+=10;
                }
#endif
            }
        }
        /*Clear the HALT_REQ bit */
        OS_REG_CLR_BIT(ah,AR_CFG,AR_CFG_HALT_REQ);
    } /* DMA HALT end */
    /*
     * Set Mac(BB,Phy) Warm Reset
     */
    OS_REG_WRITE(ah, AR_RTC_RC, rst_flags);

    OS_DELAY(50); /* XXX 50 usec */
#ifdef AH_SUPPORT_SCORPION
    //for scorpion to resotre the tsf value
    if (AR_SREV_SCORPION(ah)) {
        tsf+=50;
    }
#endif
    /*
     * Clear resets and force wakeup
     */
    OS_REG_WRITE(ah, AR_RTC_RC, 0);
#ifdef AH_SUPPORT_SCORPION
    //only for scorpion to do this kind of check as it needs to restore the tsf value
    if (AR_SREV_SCORPION(ah)) {
        i=0;
        do{
            OS_DELAY(reset_delay);
            i++;
            tsf+=reset_delay;
        }while (((OS_REG_READ(ah, AR_RTC_RC) & AR_RTC_RC_M) !=0) && i < 30);

        if (i>=30)
            printk("%s: AR_RTC_RC = 0x%x\n", __func__, OS_REG_READ(ah, AR_RTC_RC));

    } else if (!ath_hal_wait(ah, AR_RTC_RC, AR_RTC_RC_M, 0, AH_WAIT_TIMEOUT)) {
#else
    if (!ath_hal_wait(ah, AR_RTC_RC, AR_RTC_RC_M, 0, AH_WAIT_TIMEOUT)) {
#endif
        HDPRINTF(ah, HAL_DBG_UNMASKABLE,
            "%s: RTC stuck in MAC reset\n", __FUNCTION__);
        HDPRINTF(ah, HAL_DBG_UNMASKABLE,
            "%s: AR_RTC_RC = 0x%x\n", __func__, OS_REG_READ(ah, AR_RTC_RC));
        return false;
    }

    /* Clear AHB reset */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_RC), 0);

    ar9300_attach_hw_platform(ah);

#ifdef AH_SUPPORT_SCORPION
    if (AR_SREV_SCORPION(ah)) {
        ar9300_set_tsf64 (ah, tsf);
    }
#endif
    ahp->ah_chip_reset_done = 1;
    return true;
}

static inline bool
ar9300_set_reset_power_on(struct ath_hal *ah)
{
    /* Force wake */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_WA), AH9300(ah)->ah_wa_reg_val);
    OS_DELAY(10); /* delay to allow AR_WA reg write to kick in */
    OS_REG_WRITE(ah, AR_RTC_FORCE_WAKE,
        AR_RTC_FORCE_WAKE_EN | AR_RTC_FORCE_WAKE_ON_INT);
    /*
     * RTC reset and clear. Some delay in between is needed
     * to give the chip time to settle.
     */
    OS_REG_WRITE(ah, AR_RTC_RESET, 0);
    OS_DELAY(2);
    OS_REG_WRITE(ah, AR_RTC_RESET, 1);

    /*
     * Poll till RTC is ON
     */
#define AH_RTC_POLL_TIMEOUT AH_WAIT_TIMEOUT
    if (!ath_hal_wait(ah,
             AR_RTC_STATUS, AR_RTC_STATUS_M,
             AR_RTC_STATUS_ON, AH_RTC_POLL_TIMEOUT))
    {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE,
            "%s: RTC not waking up for %d\n", __FUNCTION__, AH_WAIT_TIMEOUT);
        return false;
    }

    /*
     * Read Revisions from Chip right after RTC is on for the first time.
     * This helps us detect the chip type early and initialize it accordingly.
     */
    ar9300_read_revisions(ah);

    /*
     * Warm reset if we aren't really powering on,
     * just restarting the driver.
     */
    return ar9300_set_reset(ah, HAL_RESET_WARM);
}

/*
 * Write the given reset bit mask into the reset register
 */
bool
ar9300_set_reset_reg(struct ath_hal *ah, u_int32_t type)
{
    bool ret = false;

    /*
     * Set force wake
     */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_WA), AH9300(ah)->ah_wa_reg_val);
    OS_DELAY(10); /* delay to allow AR_WA reg write to kick in */
    OS_REG_WRITE(ah, AR_RTC_FORCE_WAKE,
        AR_RTC_FORCE_WAKE_EN | AR_RTC_FORCE_WAKE_ON_INT);

    switch (type) {
    case HAL_RESET_POWER_ON:
        ret = ar9300_set_reset_power_on(ah);
        break;
    case HAL_RESET_WARM:
    case HAL_RESET_COLD:
        ret = ar9300_set_reset(ah, type);
        break;
    default:
        break;
    }

#if ATH_SUPPORT_MCI
    if (AH_PRIVATE(ah)->ah_caps.hal_mci_support) {
        OS_REG_WRITE(ah, AR_RTC_KEEP_AWAKE, 0x2);
    }
#endif

    return ret;
}

/*
 * Places the PHY and Radio chips into reset.  A full reset
 * must be called to leave this state.  The PCI/MAC/PCU are
 * not placed into reset as we must receive interrupt to
 * re-enable the hardware.
 */
bool
ar9300_phy_disable(struct ath_hal *ah)
{
    if (!ar9300_set_reset_reg(ah, HAL_RESET_WARM)) {
        return false;
    }

#ifdef ATH_SUPPORT_LED
#define REG_READ(_reg)          *((volatile u_int32_t *)(_reg))
#define REG_WRITE(_reg, _val)   *((volatile u_int32_t *)(_reg)) = (_val);
#define ATH_GPIO_OE             0xB8040000
#define ATH_GPIO_OUT            0xB8040008 /* GPIO Ouput Value reg.*/
    if (AR_SREV_WASP(ah)) {
        if (IS_CHAN_2GHZ((AH_PRIVATE(ah)->ah_curchan))) {
            REG_WRITE(ATH_GPIO_OE, (REG_READ(ATH_GPIO_OE) | (0x1 << 13)));
        }
        else {
            REG_WRITE(ATH_GPIO_OE, (REG_READ(ATH_GPIO_OE) | (0x1 << 12)));
        }
    }
    else if (AR_SREV_SCORPION(ah)) {
        if (IS_CHAN_2GHZ((AH_PRIVATE(ah)->ah_curchan))) {
            REG_WRITE(ATH_GPIO_OE, (REG_READ(ATH_GPIO_OE) | (0x1 << 13)));
        }
        else {
            REG_WRITE(ATH_GPIO_OE, (REG_READ(ATH_GPIO_OE) | (0x1 << 12)));
        }
        /* Turn off JMPST led */
        REG_WRITE(ATH_GPIO_OUT, (REG_READ(ATH_GPIO_OUT) | (0x1 << 15)));
    }
    else if (AR_SREV_HONEYBEE(ah)) {
        REG_WRITE(ATH_GPIO_OE, (REG_READ(ATH_GPIO_OE) | (0x1 << 12)));
    }
    else if (AR_SREV_DRAGONFLY(ah)) {
#ifdef ATH_24G_LED_GPIO
#define _MAX_24G_LED_GPIO 22
        if ((ATH_24G_LED_GPIO <= _MAX_24G_LED_GPIO) && IS_CHAN_2GHZ((AH_PRIVATE(ah)->ah_curchan))) {
            REG_WRITE(ATH_GPIO_OE, (REG_READ(ATH_GPIO_OE) | (0x1 << ATH_24G_LED_GPIO)));
        }
#undef _MAX_24G_LED_GPIO
#endif
        /* Turn off JMPST led */
        REG_WRITE(ATH_GPIO_OUT, (REG_READ(ATH_GPIO_OUT) | (0x1 << 15)));
    }

#undef REG_READ
#undef REG_WRITE
#endif

    if ( AR_SREV_OSPREY(ah) ) {
        OS_REG_RMW(ah, AR_HOSTIF_REG(ah, AR_GPIO_OUTPUT_MUX1), 0x0, 0x1f);
    }


    ar9300_init_pll(ah, AH_NULL);

    return true;
}

/*
 * Places all of hardware into reset
 */
bool
ar9300_disable(struct ath_hal *ah)
{
    if (!ar9300_set_power_mode(ah, HAL_PM_AWAKE, true)) {
        return false;
    }
    if (!ar9300_set_reset_reg(ah, HAL_RESET_COLD)) {
        return false;
    }

    ar9300_init_pll(ah, AH_NULL);

    return true;
}

/*
 * TODO: Only write the PLL if we're changing to or from CCK mode
 *
 * WARNING: The order of the PLL and mode registers must be correct.
 */
static inline void
ar9300_set_rf_mode(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t rf_mode = 0;

    if (chan == AH_NULL) {
        return;
    }
    switch (AH9300(ah)->ah_hwp) {
    case HAL_TRUE_CHIP:
        rf_mode |= (IS_CHAN_B(chan) || IS_CHAN_G(chan)) ?
            AR_PHY_MODE_DYNAMIC : AR_PHY_MODE_OFDM;
        break;
    default:
        HALASSERT(0);
        break;
    }
    /*  Phy mode bits for 5GHz channels requiring Fast Clock */
    if ( IS_5GHZ_FAST_CLOCK_EN(ah, chan)) {
        rf_mode |= (AR_PHY_MODE_DYNAMIC | AR_PHY_MODE_DYN_CCK_DISABLE);
    }
    OS_REG_WRITE(ah, AR_PHY_MODE, rf_mode);
}

/*
 * Places the hardware into reset and then pulls it out of reset
 */
bool
ar9300_chip_reset(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    struct ath_hal_9300     *ahp = AH9300(ah);
    OS_MARK(ah, AH_MARK_CHIPRESET, chan ? chan->channel : 0);

    /*
     * Warm reset is optimistic.
     */
    if (!ar9300_set_reset_reg(ah, HAL_RESET_WARM)) {
        return false;
    }

    /* Bring out of sleep mode (AGAIN) */
    if (!ar9300_set_power_mode(ah, HAL_PM_AWAKE, true)) {
        return false;
    }

    ahp->ah_chip_full_sleep = false;

    if (AR_SREV_HORNET(ah)) {
        ar9300_internal_regulator_apply(ah);
    }

    ar9300_init_pll(ah, chan);

    /*
     * Perform warm reset before the mode/PLL/turbo registers
     * are changed in order to deactivate the radio.  Mode changes
     * with an active radio can result in corrupted shifts to the
     * radio device.
     */
    ar9300_set_rf_mode(ah, chan);

    return true;
}

/* ar9300_setup_calibration
 * Setup HW to collect samples used for current cal
 */
inline static void
ar9300_setup_calibration(struct ath_hal *ah, HAL_CAL_LIST *curr_cal)
{
    /* Select calibration to run */
    switch (curr_cal->cal_data->cal_type) {
    case IQ_MISMATCH_CAL:
        /* Start calibration w/ 2^(INIT_IQCAL_LOG_COUNT_MAX+1) samples */
        OS_REG_RMW_FIELD(ah, AR_PHY_TIMING4,
            AR_PHY_TIMING4_IQCAL_LOG_COUNT_MAX,
            curr_cal->cal_data->cal_count_max);
        OS_REG_WRITE(ah, AR_PHY_CALMODE, AR_PHY_CALMODE_IQ);

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: starting IQ Mismatch Calibration\n", __func__);

        /* Kick-off cal */
        OS_REG_SET_BIT(ah, AR_PHY_TIMING4, AR_PHY_TIMING4_DO_CAL);

        break;
    case TEMP_COMP_CAL:
        if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) ||
            AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah)) {
            OS_REG_RMW_FIELD(ah,
                AR_HORNET_CH0_THERM, AR_PHY_65NM_CH0_THERM_LOCAL, 1);
            OS_REG_RMW_FIELD(ah,
                AR_HORNET_CH0_THERM, AR_PHY_65NM_CH0_THERM_START, 1);
        } else if (AR_SREV_JUPITER(ah) || AR_SREV_APHRODITE(ah)) {
            OS_REG_RMW_FIELD(ah,
                AR_PHY_65NM_CH0_THERM_JUPITER, AR_PHY_65NM_CH0_THERM_LOCAL, 1);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_65NM_CH0_THERM_JUPITER, AR_PHY_65NM_CH0_THERM_START, 1);
        } else {
            OS_REG_RMW_FIELD(ah,
                AR_PHY_65NM_CH0_THERM, AR_PHY_65NM_CH0_THERM_LOCAL, 1);
            OS_REG_RMW_FIELD(ah,
                AR_PHY_65NM_CH0_THERM, AR_PHY_65NM_CH0_THERM_START, 1);
        }

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: starting Temperature Compensation Calibration\n", __func__);
        break;
    default:
        HDPRINTF(ah, HAL_DBG_UNMASKABLE,
            "%s called with incorrect calibration type.\n", __func__);
    }
}

/* ar9300_reset_calibration
 * Initialize shared data structures and prepare a cal to be run.
 */
inline static void
ar9300_reset_calibration(struct ath_hal *ah, HAL_CAL_LIST *curr_cal)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    int i;

    /* Setup HW for new calibration */
    ar9300_setup_calibration(ah, curr_cal);

    /* Change SW state to RUNNING for this calibration */
    curr_cal->cal_state = CAL_RUNNING;

    /* Reset data structures shared between different calibrations */
    for (i = 0; i < AR9300_MAX_CHAINS; i++) {
        ahp->ah_meas0.sign[i] = 0;
        ahp->ah_meas1.sign[i] = 0;
        ahp->ah_meas2.sign[i] = 0;
        ahp->ah_meas3.sign[i] = 0;
    }

    ahp->ah_cal_samples = 0;
}

#ifdef XXX_UNUSED_FUNCTION
/*
 * Find out which of the RX chains are enabled
 */
static u_int32_t
ar9300_get_rx_chain_mask(struct ath_hal *ah)
{
    u_int32_t ret_val = OS_REG_READ(ah, AR_PHY_RX_CHAINMASK);
    /* The bits [2:0] indicate the rx chain mask and are to be
     * interpreted as follows:
     * 00x => Only chain 0 is enabled
     * 01x => Chain 1 and 0 enabled
     * 1xx => Chain 2,1 and 0 enabled
     */
    return (ret_val & 0x7);
}
#endif

static void
ar9300_get_nf_hist_base(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *chan,
    int is_scan, int16_t nf[])
{
    HAL_NFCAL_BASE *h_base;

#ifdef ATH_NF_PER_CHAN
    h_base = &chan->nf_cal_hist.base;
#else
    if (is_scan) {
        /*
         * The channel we are currently on is not the home channel,
         * so we shouldn't use the home channel NF buffer's values on
         * this channel.  Instead, use the NF single value already
         * read for this channel.  (Or, if we haven't read the NF for
         * this channel yet, the SW default for this chip/band will
         * be used.)
         */
        h_base = &chan->nf_cal_hist.base;
    } else {
        /* use the home channel NF info */
        h_base = &AH_PRIVATE(ah)->nf_cal_hist.base;
    }
#endif
    OS_MEMCPY(nf, h_base->priv_nf, sizeof(h_base->priv_nf));
}

bool
ar9300_load_nf(struct ath_hal *ah, int16_t nf[])
{
    int i, j;
    int32_t val;
    /* XXX where are EXT regs defined */
    const u_int32_t ar9300_cca_regs[] = {
        AR_PHY_CCA_0,
        AR_PHY_CCA_1,
        AR_PHY_CCA_2,
        AR_PHY_EXT_CCA,
        AR_PHY_EXT_CCA_1,
        AR_PHY_EXT_CCA_2,
    };
    u_int8_t chainmask;

    /*
     * Force NF calibration for all chains, otherwise Vista station
     * would conduct a bad performance
     */
    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) || AR_SREV_APHRODITE(ah)) {
        chainmask = 0x9;
    } else if (AR_SREV_WASP(ah) || AR_SREV_JUPITER(ah) || AR_SREV_HONEYBEE(ah)) {
        chainmask = 0x1b;
    } else {
        chainmask = 0x3F;
    }

    /*
     * Write filtered NF values into max_cca_pwr register parameter
     * so we can load below.
     */
    for (i = 0; i < NUM_NF_READINGS; i++) {
        if (chainmask & (1 << i)) {
            val = OS_REG_READ(ah, ar9300_cca_regs[i]);
            val &= 0xFFFFFE00;
            val |= (((u_int32_t)(nf[i]) << 1) & 0x1ff);
            OS_REG_WRITE(ah, ar9300_cca_regs[i], val);
        }
    }

    /*
     * Load software filtered NF value into baseband internal min_cca_pwr
     * variable.
     */
    OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
    OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);

    /* Wait for load to complete, should be fast, a few 10s of us. */
    /* Changed the max delay 250us back to 10000us, since 250us often
     * results in NF load timeout and causes deaf condition
     * during stress testing 12/12/2009
     */
    for (j = 0; j < 10000; j++) {
        if ((OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0){
            break;
        }
        OS_DELAY(10);
    }
    if ((OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) != 0) {
        /*
         * We timed out waiting for the noisefloor to load, probably
         * due to an in-progress rx.  Simply return here and allow
         * the load plenty of time to complete before the next
         * calibration interval.  We need to avoid trying to load -50
         * (which happens below) while the previous load is still in
         * progress as this can cause rx deafness (see EV 66368,62830).
         * Instead by returning here, the baseband nf cal will
         * just be capped by our present noisefloor until the next
         * calibration timer.
         */
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE,
            "%s: *** TIMEOUT while waiting for nf to load: "
            "AR_PHY_AGC_CONTROL=0x%x ***\n",
            __func__, OS_REG_READ(ah, AR_PHY_AGC_CONTROL));
        return false;
    }

    /*
     * Restore max_cca_power register parameter again so that we're not capped
     * by the median we just loaded.  This will be initial (and max) value
     * of next noise floor calibration the baseband does.
     */
    for (i = 0; i < NUM_NF_READINGS; i++) {
        if (chainmask & (1 << i)) {
            val = OS_REG_READ(ah, ar9300_cca_regs[i]);
            val &= 0xFFFFFE00;
            val |= (((u_int32_t)(-50) << 1) & 0x1ff);
            OS_REG_WRITE(ah, ar9300_cca_regs[i], val);
        }
    }
    return true;
}

/* ar9300_per_calibration
 * Generic calibration routine.
 * Recalibrate the lower PHY chips to account for temperature/environment
 * changes.
 */
inline static void
ar9300_per_calibration(struct ath_hal *ah,  HAL_CHANNEL_INTERNAL *ichan,
    u_int8_t rxchainmask, HAL_CAL_LIST *curr_cal, bool *is_cal_done)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    /* Cal is assumed not done until explicitly set below */
    *is_cal_done = false;

    /* Calibration in progress. */
    if (curr_cal->cal_state == CAL_RUNNING) {
        /* Check to see if it has finished. */
        if (!(OS_REG_READ(ah, AR_PHY_TIMING4) & AR_PHY_TIMING4_DO_CAL)) {
            int i, num_chains = 0;
            for (i = 0; i < AR9300_MAX_CHAINS; i++) {
                if (rxchainmask & (1 << i)) {
                    num_chains++;
                }
            }

            /*
             * Accumulate cal measures for active chains
             */
            curr_cal->cal_data->cal_collect(ah, num_chains);

            ahp->ah_cal_samples++;

            if (ahp->ah_cal_samples >= curr_cal->cal_data->cal_num_samples) {
                /*
                 * Process accumulated data
                 */
                curr_cal->cal_data->cal_post_proc(ah, num_chains);

                /* Calibration has finished. */
                ichan->cal_valid |= curr_cal->cal_data->cal_type;
                curr_cal->cal_state = CAL_DONE;
                *is_cal_done = true;
            } else {
                /* Set-up collection of another sub-sample until we
                 * get desired number
                 */
                ar9300_setup_calibration(ah, curr_cal);
            }
        }
    } else if (!(ichan->cal_valid & curr_cal->cal_data->cal_type)) {
        /* If current cal is marked invalid in channel, kick it off */
        ar9300_reset_calibration(ah, curr_cal);
    }
}

static void
ar9300_start_nf_cal(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
    OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);
    AH9300(ah)->nf_tsf32 = ar9300_get_tsf32(ah);

/*  EV 121277
 *  We are reading the NF values before we start the NF operation, because
 *  of that we are getting very high values like -45.
 *  This triggers the CW_INT detected and EACS module triggers the channel change
 *  chip_reset_done value is used to fix this issue.
 *  chip_reset_flag is set during the RTC reset.
 *  chip_reset_flag is cleared during the starting NF operation.
 *  if flag is set we will clear the flag and will not read the NF values.
 */
    ahp->ah_chip_reset_done = 0;
}

/* ar9300_calibration
 * Wrapper for a more generic Calibration routine. Primarily to abstract to
 * upper layers whether there is 1 or more calibrations to be run.
 */
bool
ar9300_calibration(struct ath_hal *ah,  HAL_CHANNEL *chan, u_int8_t rxchainmask,
    bool do_nf_cal, bool *is_cal_done, int is_scan,
    u_int32_t *sched_cals)
{
#ifndef AR9340_EMULATION
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_CAL_LIST *curr_cal = ahp->ah_cal_list_curr;
    HAL_CHANNEL_INTERNAL *ichan = ath_hal_checkchannel(ah, chan);
    int16_t nf_buf[NUM_NF_READINGS];

    *is_cal_done = true;


    /* XXX: For initial wasp bringup - disable periodic calibration */
    /* Invalid channel check */
    if (ichan == AH_NULL) {
        HDPRINTF(ah, HAL_DBG_CHANNEL,
            "%s: invalid channel %u/0x%x; no mapping\n",
            __func__, chan->channel, chan->channel_flags);
        return false;
    }

    HDPRINTF(ah, HAL_DBG_CALIBRATE,
        "%s: Entering, Doing NF Cal = %d\n", __func__, do_nf_cal);
    HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Chain 0 Rx IQ Cal Correction 0x%08x\n",
        __func__, OS_REG_READ(ah, AR_PHY_RX_IQCAL_CORR_B0));
    if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah) && !AR_SREV_APHRODITE(ah)) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: Chain 1 Rx IQ Cal Correction 0x%08x\n",
            __func__, OS_REG_READ(ah, AR_PHY_RX_IQCAL_CORR_B1));
        if (!AR_SREV_WASP(ah) && !AR_SREV_JUPITER(ah) && !AR_SREV_HONEYBEE(ah)) {
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s: Chain 2 Rx IQ Cal Correction 0x%08x\n",
                __func__, OS_REG_READ(ah, AR_PHY_RX_IQCAL_CORR_B2));
        }
    }

    OS_MARK(ah, AH_MARK_PERCAL, chan->channel);

    /* For given calibration:
     * 1. Call generic cal routine
     * 2. When this cal is done (is_cal_done) if we have more cals waiting
     *    (eg after reset), mask this to upper layers by not propagating
     *    is_cal_done if it is set to TRUE.
     *    Instead, change is_cal_done to FALSE and setup the waiting cal(s)
     *    to be run.
     */
    if (curr_cal && (curr_cal->cal_data->cal_type & *sched_cals) &&
        (curr_cal->cal_state == CAL_RUNNING ||
         curr_cal->cal_state == CAL_WAITING))
    {
        ar9300_per_calibration(ah, ichan, rxchainmask, curr_cal, is_cal_done);

        if (*is_cal_done == true) {
            ahp->ah_cal_list_curr = curr_cal = curr_cal->cal_next;

            if (curr_cal && curr_cal->cal_state == CAL_WAITING) {
                *is_cal_done = false;
                ar9300_reset_calibration(ah, curr_cal);
            } else {
                *sched_cals &= ~IQ_MISMATCH_CAL;
            }
        }
    }

    /* Do NF cal only at longer intervals */
    if (do_nf_cal) {
        int nf_done;

        /* Get the value from the previous NF cal and update history buffer */
        nf_done = ar9300_store_new_nf(ah, ichan, is_scan);
        if (ichan->channel_flags & CHANNEL_CW_INT) {
            chan->channel_flags |= CHANNEL_CW_INT;
        }
        ichan->channel_flags &= (~CHANNEL_CW_INT);

        if (nf_done) {
            /*
             * Load the NF from history buffer of the current channel.
             * NF is slow time-variant, so it is OK to use a historical value.
             */
            ar9300_get_nf_hist_base(ah,
                AH_PRIVATE(ah)->ah_curchan, is_scan, nf_buf);
            if (!ar9300_load_nf(ah, nf_buf)) {
                HDPRINTF(ah, HAL_DBG_RESET, "%s: ar9300_load_nf Failed\n", __func__);
                return false;
            }
            if (AH_PRIVATE(ah)->ah_config.ath_hal_enable_adaptiveCCAThres) {
                ar9300_update_cca_threshold(ah, nf_buf, rxchainmask);
            }
            /* start NF calibration, without updating BB NF register*/
            ar9300_start_nf_cal(ah);
        }
    }
#endif
    return true;
}

/* ar9300_iq_cal_collect
 * Collect data from HW to later perform IQ Mismatch Calibration
 */
void
ar9300_iq_cal_collect(struct ath_hal *ah, u_int8_t num_chains)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    int i;

    /*
     * Accumulate IQ cal measures for active chains
     */
    for (i = 0; i < num_chains; i++) {
        ahp->ah_total_power_meas_i[i] = OS_REG_READ(ah, AR_PHY_CAL_MEAS_0(i));
        ahp->ah_total_power_meas_q[i] = OS_REG_READ(ah, AR_PHY_CAL_MEAS_1(i));
        ahp->ah_total_iq_corr_meas[i] =
            (int32_t) OS_REG_READ(ah, AR_PHY_CAL_MEAS_2(i));
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%d: Chn %d "
            "Reg Offset(0x%04x)pmi=0x%08x; "
            "Reg Offset(0x%04x)pmq=0x%08x; "
            "Reg Offset (0x%04x)iqcm=0x%08x;\n",
            ahp->ah_cal_samples,
            i,
            (unsigned) AR_PHY_CAL_MEAS_0(i),
            ahp->ah_total_power_meas_i[i],
            (unsigned) AR_PHY_CAL_MEAS_1(i),
            ahp->ah_total_power_meas_q[i],
            (unsigned) AR_PHY_CAL_MEAS_2(i),
            ahp->ah_total_iq_corr_meas[i]);
    }
}

/* ar9300_iq_calibration
 * Use HW data to perform IQ Mismatch Calibration
 */
void
ar9300_iq_calibration(struct ath_hal *ah, u_int8_t num_chains)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int32_t power_meas_q, power_meas_i, iq_corr_meas;
    u_int32_t q_coff_denom, i_coff_denom;
    int32_t q_coff, i_coff;
    int iq_corr_neg, i;
    static const u_int32_t offset_array[3] = {
        AR_PHY_RX_IQCAL_CORR_B0,
        AR_PHY_RX_IQCAL_CORR_B1,
        AR_PHY_RX_IQCAL_CORR_B2,
    };

    for (i = 0; i < num_chains; i++) {
        power_meas_i = ahp->ah_total_power_meas_i[i];
        power_meas_q = ahp->ah_total_power_meas_q[i];
        iq_corr_meas = ahp->ah_total_iq_corr_meas[i];

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "Starting IQ Cal and Correction for Chain %d\n", i);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "Orignal: Chn %diq_corr_meas = 0x%08x\n",
            i, ahp->ah_total_iq_corr_meas[i]);

        iq_corr_neg = 0;

        /* iq_corr_meas is always negative. */
        if (iq_corr_meas > 0x80000000)  {
            iq_corr_meas = (0xffffffff - iq_corr_meas) + 1;
            iq_corr_neg = 1;
        }

        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "Chn %d pwr_meas_i = 0x%08x\n", i, power_meas_i);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "Chn %d pwr_meas_q = 0x%08x\n", i, power_meas_q);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "iq_corr_neg is 0x%08x\n", iq_corr_neg);

        i_coff_denom = (power_meas_i / 2 + power_meas_q / 2) / 256;
        q_coff_denom = power_meas_q / 64;

        /* Protect against divide-by-0 */
        if ((i_coff_denom != 0) && (q_coff_denom != 0)) {
            /* IQ corr_meas is already negated if iqcorr_neg == 1 */
            i_coff = iq_corr_meas / i_coff_denom;
            q_coff = power_meas_i / q_coff_denom - 64;
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "Chn %d i_coff = 0x%08x\n", i, i_coff);
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "Chn %d q_coff = 0x%08x\n", i, q_coff);

            /* Force bounds on i_coff */
            if (i_coff >= 63) {
                i_coff = 63;
            } else if (i_coff <= -63) {
                i_coff = -63;
            }

            /* Negate i_coff if iq_corr_neg == 0 */
            if (iq_corr_neg == 0x0) {
                i_coff = -i_coff;
            }

            /* Force bounds on q_coff */
            if (q_coff >= 63) {
                q_coff = 63;
            } else if (q_coff <= -63) {
                q_coff = -63;
            }

            i_coff = i_coff & 0x7f;
            q_coff = q_coff & 0x7f;

            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "Chn %d : i_coff = 0x%x  q_coff = 0x%x\n", i, i_coff, q_coff);
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "Register offset (0x%04x) before update = 0x%x\n",
                offset_array[i], OS_REG_READ(ah, offset_array[i]));

            OS_REG_RMW_FIELD(ah, offset_array[i],
                AR_PHY_RX_IQCAL_CORR_IQCORR_Q_I_COFF, i_coff);
            OS_REG_RMW_FIELD(ah, offset_array[i],
                AR_PHY_RX_IQCAL_CORR_IQCORR_Q_Q_COFF, q_coff);

            /* store the RX cal results */
            ahp->ah_rx_cal_corr[i] = OS_REG_READ(ah, offset_array[i]) & 0x7fff;
            ahp->ah_rx_cal_complete = true;
            ahp->ah_rx_cal_chan = AH_PRIVATE(ah)->ah_curchan->channel;
            ahp->ah_rx_cal_chan_flag = AH_PRIVATE(ah)->ah_curchan->channel_flags
                                        &~ CHANNEL_PASSIVE;

            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "Register offset (0x%04x) QI COFF (bitfields 0x%08x) "
                "after update = 0x%x\n",
                offset_array[i], AR_PHY_RX_IQCAL_CORR_IQCORR_Q_I_COFF,
                OS_REG_READ(ah, offset_array[i]));
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "Register offset (0x%04x) QQ COFF (bitfields 0x%08x) "
                "after update = 0x%x\n",
                offset_array[i], AR_PHY_RX_IQCAL_CORR_IQCORR_Q_Q_COFF,
                OS_REG_READ(ah, offset_array[i]));
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "IQ Cal and Correction done for Chain %d\n", i);
        }
    }

    OS_REG_SET_BIT(ah,
        AR_PHY_RX_IQCAL_CORR_B0, AR_PHY_RX_IQCAL_CORR_IQCORR_ENABLE);
    HDPRINTF(ah, HAL_DBG_CALIBRATE,
        "IQ Cal and Correction (offset 0x%04x) enabled "
        "(bit position 0x%08x). New Value 0x%08x\n",
        (unsigned) (AR_PHY_RX_IQCAL_CORR_B0),
        AR_PHY_RX_IQCAL_CORR_IQCORR_ENABLE,
        OS_REG_READ(ah, AR_PHY_RX_IQCAL_CORR_B0));
}

/*
 * When coming back from offchan, we do not perform RX IQ Cal.
 * But the chip reset will clear all previous results
 * We store the previous results and restore here.
 */
void
ar9300_rx_iq_cal_restore(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int32_t   i_coff, q_coff;
    bool is_restore = false;
    int i;
    static const u_int32_t offset_array[3] = {
        AR_PHY_RX_IQCAL_CORR_B0,
        AR_PHY_RX_IQCAL_CORR_B1,
        AR_PHY_RX_IQCAL_CORR_B2,
    };

    for (i=0; i<AR9300_MAX_CHAINS; i++) {
        if (ahp->ah_rx_cal_corr[i]) {
            i_coff = (ahp->ah_rx_cal_corr[i] &
                        AR_PHY_RX_IQCAL_CORR_IQCORR_Q_I_COFF) >>
                        AR_PHY_RX_IQCAL_CORR_IQCORR_Q_I_COFF_S;
            q_coff = (ahp->ah_rx_cal_corr[i] &
                        AR_PHY_RX_IQCAL_CORR_IQCORR_Q_Q_COFF) >>
                        AR_PHY_RX_IQCAL_CORR_IQCORR_Q_Q_COFF_S;

            OS_REG_RMW_FIELD(ah, offset_array[i],
                AR_PHY_RX_IQCAL_CORR_IQCORR_Q_I_COFF, i_coff);
            OS_REG_RMW_FIELD(ah, offset_array[i],
                AR_PHY_RX_IQCAL_CORR_IQCORR_Q_Q_COFF, q_coff);

            is_restore = true;
        }
    }

    if (is_restore)
        OS_REG_SET_BIT(ah,
            AR_PHY_RX_IQCAL_CORR_B0, AR_PHY_RX_IQCAL_CORR_IQCORR_ENABLE);

    HDPRINTF(ah, HAL_DBG_CALIBRATE,
        "%s: IQ Cal and Correction (offset 0x%04x) enabled "
        "(bit position 0x%08x). New Value 0x%08x\n",
        __func__,
        (unsigned) (AR_PHY_RX_IQCAL_CORR_B0),
        AR_PHY_RX_IQCAL_CORR_IQCORR_ENABLE,
        OS_REG_READ(ah, AR_PHY_RX_IQCAL_CORR_B0));
}

/*
 * Set a limit on the overall output power.  Used for dynamic
 * transmit power control and the like.
 *
 * NB: limit is in units of 0.5 dbM.
 */
bool
ar9300_set_tx_power_limit(struct ath_hal *ah, u_int32_t limit,
    u_int16_t extra_txpow, u_int16_t tpc_in_db)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CHANNEL_INTERNAL *ichan = ahpriv->ah_curchan;
    HAL_CHANNEL *chan = (HAL_CHANNEL *)ichan;

    if (NULL == chan) {
        return false;
      }
    ahpriv->ah_power_limit = AH_MIN(limit, MAX_RATE_POWER);
    ahpriv->ah_extra_txpow = extra_txpow;

    if(chan == NULL) {
        return false;
    }
    if (ar9300_eeprom_set_transmit_power(ah, &ahp->ah_eeprom, ichan,
        ath_hal_getctl(ah, chan), ath_hal_getantennaallowed(ah, chan),
        ath_hal_get_twice_max_regpower(ahpriv, ichan, chan),
        AH_MIN(MAX_RATE_POWER, ahpriv->ah_power_limit)) != HAL_OK)
    {
        return false;
    }
    return true;
}

/*
 * Exported call to check for a recent gain reading and return
 * the current state of the thermal calibration gain engine.
 */
HAL_RFGAIN
ar9300_get_rfgain(struct ath_hal *ah)
{
    return HAL_RFGAIN_INACTIVE;
}

#define HAL_GREEN_AP_RX_MASK 0x1

static inline void
ar9300_init_chain_masks(struct ath_hal *ah, int rx_chainmask, int tx_chainmask)
{
    if (AH_PRIVATE(ah)->green_ap_ps_on ) {
        rx_chainmask = HAL_GREEN_AP_RX_MASK;
    }
    if (rx_chainmask == 0x5) {
        OS_REG_SET_BIT(ah, AR_PHY_ANALOG_SWAP, AR_PHY_SWAP_ALT_CHAIN);
    }
    OS_REG_WRITE(ah, AR_PHY_RX_CHAINMASK, rx_chainmask);
    OS_REG_WRITE(ah, AR_PHY_CAL_CHAINMASK, rx_chainmask);

    /*
     * Adaptive Power Management:
     * Some 3 stream chips exceed the PCIe power requirements.
     * This workaround will reduce power consumption by using 2 tx chains
     * for 1 and 2 stream rates (5 GHz only).
     *
     * Set the self gen mask to 2 tx chains when APM is enabled.
     *
     */
    if (AH_PRIVATE(ah)->ah_caps.hal_enable_apm && (tx_chainmask == 0x7)) {
        OS_REG_WRITE(ah, AR_SELFGEN_MASK, 0x3);
    }
    else {
        OS_REG_WRITE(ah, AR_SELFGEN_MASK, tx_chainmask);
    }

    if (tx_chainmask == 0x5) {
        OS_REG_SET_BIT(ah, AR_PHY_ANALOG_SWAP, AR_PHY_SWAP_ALT_CHAIN);
    }
}

/*
 * Override INI values with chip specific configuration.
 */
static inline void
ar9300_override_ini(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t val;
    HAL_CAPABILITIES *p_cap = &AH_PRIVATE(ah)->ah_caps;

    /*
     * Set the RX_ABORT and RX_DIS and clear it only after
     * RXE is set for MAC. This prevents frames with
     * corrupted descriptor status.
     */
    OS_REG_SET_BIT(ah, AR_DIAG_SW, (AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT));
    /*
     * For Merlin and above, there is a new feature that allows Multicast
     * search based on both MAC Address and Key ID.
     * By default, this feature is enabled.
     * But since the driver is not using this feature, we switch it off;
     * otherwise multicast search based on MAC addr only will fail.
     */
    val = OS_REG_READ(ah, AR_PCU_MISC_MODE2) & (~AR_ADHOC_MCAST_KEYID_ENABLE);
    OS_REG_WRITE(ah, AR_PCU_MISC_MODE2,
        val | AR_BUG_58603_FIX_ENABLE | AR_AGG_WEP_ENABLE);


    /* Osprey revision specific configuration */

    /* Osprey 2.0+ - if SW RAC support is disabled, must also disable
     * the Osprey 2.0 hardware RAC fix.
     */
    if (p_cap->hal_isr_rac_support == false) {
        OS_REG_CLR_BIT(ah, AR_CFG, AR_CFG_MISSING_TX_INTR_FIX_ENABLE);
    }
#ifdef AR9340_EMULATION
    OS_REG_WRITE(ah, 0xa238, 0xcfbc1018); /* BB_frame_control */
    OS_REG_WRITE(ah, 0xa2d8, 0x7999a83a); /* BB_cl_cal_ctrl */
    OS_REG_WRITE(ah, 0xae04, 0x00001000); /* BB_gain_force_max_gains_b1 */
    OS_REG_WRITE(ah, 0xa3f8, 0x0cdbd381); /* BB_tpc_1 */
    OS_REG_WRITE(ah, 0x9e08, 0x0040233c); /* BB_gains_min_offsets */
#if defined (AR9550_EMULATION) || defined (AR9530_EMULATION)
    OS_REG_WRITE(ah, 0xa204, 0x013037c0); /* Static HT20 */
#else
    OS_REG_WRITE(ah, 0xa204, 0x37c0);     /* Static HT20 */
#endif
#endif

#ifndef ART_BUILD
    /* try to enable old pal if it is needed for h/w green tx */
    ar9300_hwgreentx_set_pal_spare(ah, 1);
#endif
}

static inline void
ar9300_prog_ini(struct ath_hal *ah, struct ar9300_ini_array *ini_arr,
    int column)
{
    int i, reg_writes = 0;

    /* New INI format: Array may be undefined (pre, core, post arrays) */
    if (ini_arr->ia_array == NULL) {
        return;
    }

    /*
     * New INI format: Pre, core, and post arrays for a given subsystem may be
     * modal (> 2 columns) or non-modal (2 columns).
     * Determine if the array is non-modal and force the column to 1.
     */
    if (column >= ini_arr->ia_columns) {
        column = 1;
    }

    for (i = 0; i < ini_arr->ia_rows; i++) {
        u_int32_t reg = INI_RA(ini_arr, i, 0);
        u_int32_t val = INI_RA(ini_arr, i, column);

        /*
        ** Determine if this is a shift register value
        ** (reg >= 0x16000 && reg < 0x17000 for Osprey) ,
        ** and insert the configured delay if so.
        ** -this delay is not required for Osprey (EV#71410)
        */
        OS_REG_WRITE(ah, reg, val);
        WAR_6773(reg_writes);

    }
}

static inline HAL_STATUS
ar9300_process_ini(struct ath_hal *ah, HAL_CHANNEL *chan,
    HAL_CHANNEL_INTERNAL *ichan, HAL_HT_MACMODE macmode)
{
    int reg_writes = 0;
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int modes_index, modes_txgaintable_index = 0;
    int i;
    HAL_STATUS status;
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    /* Setup the indices for the next set of register array writes */
    /* TODO:
     * If the channel marker is indicative of the current mode rather
     * than capability, we do not need to check the phy mode below.
     */
    switch (chan->channel_flags & CHANNEL_ALL) {
    case CHANNEL_A:
    case CHANNEL_A_HT20:
        if (AR_SREV_SCORPION(ah)){
            if (chan->channel <= 5350){
                modes_txgaintable_index = 1;
            }else if ((chan->channel > 5350) && (chan->channel <= 5600)){
                modes_txgaintable_index = 3;
            }else if (chan->channel > 5600){
                modes_txgaintable_index = 5;
            }
        }
        modes_index = 1;
        break;

    case CHANNEL_A_HT40PLUS:
    case CHANNEL_A_HT40MINUS:
        if (AR_SREV_SCORPION(ah)){
            if (chan->channel <= 5350){
                modes_txgaintable_index = 2;
            }else if ((chan->channel > 5350) && (chan->channel <= 5600)){
                modes_txgaintable_index = 4;
            }else if (chan->channel > 5600){
                modes_txgaintable_index = 6;
            }
        }
        modes_index = 2;
        break;

    case CHANNEL_PUREG:
    case CHANNEL_G_HT20:
    case CHANNEL_B:
        if (AR_SREV_SCORPION(ah)){
            modes_txgaintable_index = 8;
        }else if (AR_SREV_HONEYBEE(ah)){
	        modes_txgaintable_index = 1;
        }else if (AR_SREV_DRAGONFLY(ah)){
	        modes_txgaintable_index = 2;
        }
        modes_index = 4;
        break;

    case CHANNEL_G_HT40PLUS:
    case CHANNEL_G_HT40MINUS:
        if (AR_SREV_SCORPION(ah)){
            modes_txgaintable_index = 7;
		}else if (AR_SREV_HONEYBEE(ah)){
			modes_txgaintable_index = 1;
		}else if (AR_SREV_DRAGONFLY(ah)){
			modes_txgaintable_index = 1;
		}
        modes_index = 3;
        break;

    case CHANNEL_108G:
        modes_index = 5;
        break;

    default:
        HALASSERT(0);
        return HAL_EINVAL;
    }

#if 0
    /* Set correct Baseband to analog shift setting to access analog chips. */
    OS_REG_WRITE(ah, AR_PHY(0), 0x00000007);
#endif

    HDPRINTF(ah, HAL_DBG_RESET,
        "ar9300_process_ini: "
        "Skipping OS-REG-WRITE(ah, AR-PHY(0), 0x00000007)\n");
    HDPRINTF(ah, HAL_DBG_RESET,
        "ar9300_process_ini: no ADDac programming\n");


    /*
     * Osprey 2.0+ - new INI format.
     * Each subsystem has a pre, core, and post array.
     */
    for (i = 0; i < ATH_INI_NUM_SPLIT; i++) {
        ar9300_prog_ini(ah, &ahp->ah_ini_soc[i], modes_index);
        ar9300_prog_ini(ah, &ahp->ah_ini_mac[i], modes_index);
        ar9300_prog_ini(ah, &ahp->ah_ini_bb[i], modes_index);
        ar9300_prog_ini(ah, &ahp->ah_ini_radio[i], modes_index);
        if ((i == ATH_INI_POST) && (AR_SREV_JUPITER_20(ah) || AR_SREV_APHRODITE(ah))) {
            ar9300_prog_ini(ah, &ahp->ah_ini_radio_post_sys2ant, modes_index);
        }

    }

	if (!(AR_SREV_SOC(ah))) {
			/* Doubler issue : Some board doesn't work well with MCS15. Turn off doubler after freq locking is complete*/
			//ath_hal_printf(ah, "%s[%d] ==== before reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));
			OS_REG_RMW(ah, AR_PHY_65NM_CH0_RXTX2, 1 << AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK_S |
			               1 << AR_PHY_65NM_CH0_RXTX2_SYNTHOVR_MASK_S, 0); /*Set synthon, synthover */
			//ath_hal_printf(ah, "%s[%d] ==== after reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));

			OS_REG_RMW(ah, AR_PHY_65NM_CH1_RXTX2, 1 << AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK_S |
			               1 << AR_PHY_65NM_CH0_RXTX2_SYNTHOVR_MASK_S, 0); /*Set synthon, synthover */
			OS_REG_RMW(ah, AR_PHY_65NM_CH2_RXTX2, 1 << AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK_S |
			               1 << AR_PHY_65NM_CH0_RXTX2_SYNTHOVR_MASK_S, 0); /*Set synthon, synthover */
			OS_DELAY(200);

			//ath_hal_printf(ah, "%s[%d] ==== before reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));
			OS_REG_CLR_BIT(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK); /* clr synthon */
			OS_REG_CLR_BIT(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK); /* clr synthon */
			OS_REG_CLR_BIT(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK); /* clr synthon */
			//ath_hal_printf(ah, "%s[%d] ==== after reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));

			OS_DELAY(1);

			//ath_hal_printf(ah, "%s[%d] ==== before reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));
			OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK, 1); /* set synthon */
			OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK, 1); /* set synthon */
			OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK, 1); /* set synthon */
			//ath_hal_printf(ah, "%s[%d] ==== after reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));

			OS_DELAY(200);

			//ath_hal_printf(ah, "%s[%d] ==== before reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_SYNTH12, OS_REG_READ(ah, AR_PHY_65NM_CH0_SYNTH12));
			OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_SYNTH12, AR_PHY_65NM_CH0_SYNTH12_VREFMUL3, 0xf);
			//OS_REG_CLR_BIT(ah, AR_PHY_65NM_CH0_SYNTH12, 1<< 16); /* clr charge pump */
			//ath_hal_printf(ah, "%s[%d] ==== After  reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_SYNTH12, OS_REG_READ(ah, AR_PHY_65NM_CH0_SYNTH12));

			OS_REG_RMW(ah, AR_PHY_65NM_CH0_RXTX2, 0, 1 << AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK_S |
			               1 << AR_PHY_65NM_CH0_RXTX2_SYNTHOVR_MASK_S); /*Clr synthon, synthover */
			OS_REG_RMW(ah, AR_PHY_65NM_CH1_RXTX2, 0, 1 << AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK_S |
			               1 << AR_PHY_65NM_CH0_RXTX2_SYNTHOVR_MASK_S); /*Clr synthon, synthover */
			OS_REG_RMW(ah, AR_PHY_65NM_CH2_RXTX2, 0, 1 << AR_PHY_65NM_CH0_RXTX2_SYNTHON_MASK_S |
			               1 << AR_PHY_65NM_CH0_RXTX2_SYNTHOVR_MASK_S); /*Clr synthon, synthover */
			//ath_hal_printf(ah, "%s[%d] ==== after reg[0x%08x] = 0x%08x\n", __func__, __LINE__, AR_PHY_65NM_CH0_RXTX2, OS_REG_READ(ah, AR_PHY_65NM_CH0_RXTX2));
		}

    /* Write rxgain Array Parameters */
    REG_WRITE_ARRAY(&ahp->ah_ini_modes_rxgain, 1, reg_writes);
    HDPRINTF(ah, HAL_DBG_RESET, "ar9300_process_ini: Rx Gain programming\n");

    if (AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah)) {
        /* Write rxgain bounds Array */
        REG_WRITE_ARRAY(&ahp->ah_ini_modes_rxgain_bounds, modes_index, reg_writes);
        HDPRINTF(ah, HAL_DBG_RESET, "ar9300_process_ini: Rx Gain table bounds programming\n");
    }
    if (AR_SREV_DRAGONFLY(ah) && (ar9300_rx_gain_index_get(ah) == 0)) {
        /* Write xlna Array */
        REG_WRITE_ARRAY(&ahp->ah_ini_xlna, modes_index, reg_writes);
        HDPRINTF(ah, HAL_DBG_RESET, "ar9300_process_ini: Rx xlna programming\n");
    }

    /* UB124 xLNA settings */
    if (AR_SREV_WASP(ah) && ar9300_rx_gain_index_get(ah) == 2) {
#define REG_WRITE(_reg,_val)    *((volatile u_int32_t *)(_reg)) = (_val);
#define REG_READ(_reg)          *((volatile u_int32_t *)(_reg))
        u_int32_t val;
        /* B8040000:  bit[0]=0, bit[3]=0; */
        val = REG_READ(0xB8040000);
        val &= 0xfffffff6;
        REG_WRITE(0xB8040000, val);
        /* B804002c:  bit[31:24]=0x2e; bit[7:0]=0x2f; */
        val = REG_READ(0xB804002c);
        val &= 0x00ffff00;
        val |= 0x2e00002f;
        REG_WRITE(0xB804002c, val);
        /* B804006c:  bit[1]=1; */
        val = REG_READ(0xB804006c);
        val |= 0x2;
        REG_WRITE(0xB804006c, val);
#undef REG_READ
#undef REG_WRITE
    }


    /* Write txgain Array Parameters */
    if (AR_SREV_SCORPION(ah) || AR_SREV_HONEYBEE(ah)|| AR_SREV_DRAGONFLY(ah)) {
        REG_WRITE_ARRAY(&ahp->ah_ini_modes_txgain, modes_txgaintable_index,
            reg_writes);
    }else{
        REG_WRITE_ARRAY(&ahp->ah_ini_modes_txgain, modes_index, reg_writes);
    }
    HDPRINTF(ah, HAL_DBG_RESET, "ar9300_process_ini: Tx Gain programming\n");


    /* For 5GHz channels requiring Fast Clock, apply different modal values */
    if (IS_5GHZ_FAST_CLOCK_EN(ah, chan)) {
        HDPRINTF(ah, HAL_DBG_RESET,
            "%s: Fast clock enabled, use special ini values\n", __func__);
        REG_WRITE_ARRAY(&ahp->ah_ini_modes_additional, modes_index, reg_writes);
    }

    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah)) {
        HDPRINTF(ah, HAL_DBG_RESET,
            "%s: use xtal ini for AH9300(ah)->clk_25mhz: %d\n",
            __func__, AH9300(ah)->clk_25mhz);
        REG_WRITE_ARRAY(
            &ahp->ah_ini_modes_additional, 1/*modes_index*/, reg_writes);
    }

    if (AR_SREV_WASP(ah) && (AH9300(ah)->clk_25mhz == 0)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: Apply 40MHz ini settings\n", __func__);
        REG_WRITE_ARRAY(
            &ahp->ah_ini_modes_additional_40mhz, 1/*modesIndex*/, reg_writes);
    }

    if (2484 == chan->channel) {
        ar9300_prog_ini(ah, &ahp->ah_ini_japan2484, 1);
    }

#if 0
    if (AR_SREV_JUPITER_20(ah) || AR_SREV_APHRODITE(ah)) {
        ar9300_prog_ini(ah, &ahp->ah_ini_BTCOEX_MAX_TXPWR, 1);
    }
#endif

    /* Override INI with chip specific configuration */
    ar9300_override_ini(ah, chan);

    /* Setup 11n MAC/Phy mode registers */
    ar9300_set_11n_regs(ah, chan, macmode);

    /*
     * Moved ar9300_init_chain_masks() here to ensure the swap bit is set before
     * the pdadc table is written.  Swap must occur before any radio dependent
     * replicated register access.  The pdadc curve addressing in particular
     * depends on the consistent setting of the swap bit.
     */
    ar9300_init_chain_masks(ah, ahp->ah_rx_chainmask, ahp->ah_tx_chainmask);

    /*
     * Setup the transmit power values.
     *
     * After the public to private hal channel mapping, ichan contains the
     * valid regulatory power value.
     * ath_hal_getctl and ath_hal_getantennaallowed look up ichan from chan.
     */
    status = ar9300_eeprom_set_transmit_power(ah, &ahp->ah_eeprom, ichan,
             ath_hal_getctl(ah, chan), ath_hal_getantennaallowed(ah, chan),
             ath_hal_get_twice_max_regpower(ahpriv, ichan, chan),
             AH_MIN(MAX_RATE_POWER, ahpriv->ah_power_limit));
    if (status != HAL_OK) {
        HDPRINTF(ah, HAL_DBG_POWER_MGMT,
            "%s: error init'ing transmit power\n", __func__);
        return HAL_EIO;
    }

#if ATH_SUPPORT_FAST_CC
    /* Saved the last mode index */
    ahp->ah_mode_index = modes_index;
#endif

    return HAL_OK;
#undef N
}

/* ar9300_is_cal_supp
 * Determine if calibration is supported by device and channel flags
 */
inline static bool
ar9300_is_cal_supp(struct ath_hal *ah, HAL_CHANNEL *chan,
    HAL_CAL_TYPES cal_type)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    bool retval = false;

    switch (cal_type & ahp->ah_supp_cals) {
    case IQ_MISMATCH_CAL:
        /* Run IQ Mismatch for non-CCK only */
        if (!IS_CHAN_B(chan)) {
            retval = true;
        }
        break;
    case TEMP_COMP_CAL:
        retval = true;
        break;
    }

    return retval;
}


#if 0
/* ar9285_pa_cal
 * PA Calibration for Kite 1.1 and later versions of Kite.
 * - from system's team.
 */
static inline void
ar9285_pa_cal(struct ath_hal *ah)
{
    u_int32_t reg_val;
    int i, lo_gn, offs_6_1, offs_0;
    u_int8_t reflo;
    u_int32_t phy_test2_reg_val, phy_adc_ctl_reg_val;
    u_int32_t an_top2_reg_val, phy_tst_dac_reg_val;


    /* Kite 1.1 WAR for Bug 35666
     * Increase the LDO value to 1.28V before accessing analog Reg */
    if (AR_SREV_KITE_11(ah)) {
        OS_REG_WRITE(ah, AR9285_AN_TOP4, (AR9285_AN_TOP4_DEFAULT | 0x14) );
    }
    an_top2_reg_val = OS_REG_READ(ah, AR9285_AN_TOP2);

    /* set pdv2i pdrxtxbb */
    reg_val = OS_REG_READ(ah, AR9285_AN_RXTXBB1);
    reg_val |= ((0x1 << 5) | (0x1 << 7));
    OS_REG_WRITE(ah, AR9285_AN_RXTXBB1, reg_val);

    /* clear pwddb */
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G7);
    reg_val &= 0xfffffffd;
    OS_REG_WRITE(ah, AR9285_AN_RF2G7, reg_val);

    /* clear enpacal */
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G1);
    reg_val &= 0xfffff7ff;
    OS_REG_WRITE(ah, AR9285_AN_RF2G1, reg_val);

    /* set offcal */
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G2);
    reg_val |= (0x1 << 12);
    OS_REG_WRITE(ah, AR9285_AN_RF2G2, reg_val);

    /* set pdpadrv1=pdpadrv2=pdpaout=1 */
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G1);
    reg_val |= (0x7 << 23);
    OS_REG_WRITE(ah, AR9285_AN_RF2G1, reg_val);

    /* Read back reflo, increase it by 1 and write it. */
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G3);
    reflo = ((reg_val >> 26) & 0x7);

    if (reflo < 0x7) {
        reflo++;
    }
    reg_val = ((reg_val & 0xe3ffffff) | (reflo << 26));
    OS_REG_WRITE(ah, AR9285_AN_RF2G3, reg_val);

    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G3);
    reflo = ((reg_val >> 26) & 0x7);

    /* use TX single carrier to transmit
     * dac const
     * reg. 15
     */
    phy_tst_dac_reg_val = OS_REG_READ(ah, AR_PHY_TSTDAC_CONST);
    OS_REG_WRITE(ah, AR_PHY_TSTDAC_CONST, ((0x7ff << 11) | 0x7ff));
    reg_val = OS_REG_READ(ah, AR_PHY_TSTDAC_CONST);

    /* source is dac const
     * reg. 2
     */
    phy_test2_reg_val = OS_REG_READ(ah, AR_PHY_TEST2);
    OS_REG_WRITE(ah, AR_PHY_TEST2, ((0x1 << 7) | (0x1 << 1)));
    reg_val = OS_REG_READ(ah, AR_PHY_TEST2);

    /* set dac on
     * reg. 11
     */
    phy_adc_ctl_reg_val = OS_REG_READ(ah, AR_PHY_ADC_CTL);
    OS_REG_WRITE(ah, AR_PHY_ADC_CTL, 0x80008000);
    reg_val = OS_REG_READ(ah, AR_PHY_ADC_CTL);

    OS_REG_WRITE(ah, AR9285_AN_TOP2, (0x1 << 27) | (0x1 << 17) | (0x1 << 16) |
              (0x1 << 14) | (0x1 << 12) | (0x1 << 11) |
              (0x1 << 7) | (0x1 << 5));

    OS_DELAY(10); /* 10 usec */

    /* clear off[6:0] */
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G6);
    reg_val &= 0xfc0fffff;
    OS_REG_WRITE(ah, AR9285_AN_RF2G6, reg_val);
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G3);
    reg_val &= 0xfdffffff;
    OS_REG_WRITE(ah, AR9285_AN_RF2G3, reg_val);

    offs_6_1 = 0;
    for (i = 6; i > 0; i--) {
        /* sef off[$k]==1 */
        reg_val = OS_REG_READ(ah, AR9285_AN_RF2G6);
        reg_val &= 0xfc0fffff;
        reg_val = reg_val | (0x1 << (19 + i)) | ((offs_6_1) << 20);
        OS_REG_WRITE(ah, AR9285_AN_RF2G6, reg_val);
        lo_gn = (OS_REG_READ(ah, AR9285_AN_RF2G9)) & 0x1;
        offs_6_1 = offs_6_1 | (lo_gn << (i - 1));
    }

    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G6);
    reg_val &= 0xfc0fffff;
    reg_val = reg_val | ((offs_6_1 - 1) << 20);
    OS_REG_WRITE(ah, AR9285_AN_RF2G6, reg_val);

    /* set off_0=1; */
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G3);
    reg_val &= 0xfdffffff;
    reg_val = reg_val | (0x1 << 25);
    OS_REG_WRITE(ah, AR9285_AN_RF2G3, reg_val);

    lo_gn = OS_REG_READ(ah, AR9285_AN_RF2G9) & 0x1;
    offs_0 = lo_gn;

    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G3);
    reg_val &= 0xfdffffff;
    reg_val = reg_val | (offs_0 << 25);
    OS_REG_WRITE(ah, AR9285_AN_RF2G3, reg_val);

    /* clear pdv2i */
    reg_val = OS_REG_READ(ah, AR9285_AN_RXTXBB1);
    reg_val &= 0xffffff5f;
    OS_REG_WRITE(ah, AR9285_AN_RXTXBB1, reg_val);

    /* set enpacal */
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G1);
    reg_val |= (0x1 << 11);
    OS_REG_WRITE(ah, AR9285_AN_RF2G1, reg_val);

    /* clear offcal */
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G2);
    reg_val &= 0xffffefff;
    OS_REG_WRITE(ah, AR9285_AN_RF2G2, reg_val);

    /* set pdpadrv1=pdpadrv2=pdpaout=0 */
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G1);
    reg_val &= 0xfc7fffff;
    OS_REG_WRITE(ah, AR9285_AN_RF2G1, reg_val);

    /* Read back reflo, decrease it by 1 and write it. */
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G3);
    reflo = (reg_val >> 26) & 0x7;
    if (reflo) {
        reflo--;
    }
    reg_val = ((reg_val & 0xe3ffffff) | (reflo << 26));
    OS_REG_WRITE(ah, AR9285_AN_RF2G3, reg_val);
    reg_val = OS_REG_READ(ah, AR9285_AN_RF2G3);
    reflo = (reg_val >> 26) & 0x7;

    /* write back registers */
    OS_REG_WRITE(ah, AR_PHY_TSTDAC_CONST, phy_tst_dac_reg_val);
    OS_REG_WRITE(ah, AR_PHY_TEST2, phy_test2_reg_val);
    OS_REG_WRITE(ah, AR_PHY_ADC_CTL, phy_adc_ctl_reg_val);
    OS_REG_WRITE(ah, AR9285_AN_TOP2, an_top2_reg_val);

    /* Kite 1.1 WAR for Bug 35666
     * Decrease the LDO value back to 1.20V */
    if (AR_SREV_KITE_11(ah)) {
        OS_REG_WRITE(ah, AR9285_AN_TOP4, AR9285_AN_TOP4_DEFAULT);
    }
}
#endif

/* ar9300_run_init_cals
 * Runs non-periodic calibrations
 */
inline static bool
ar9300_run_init_cals(struct ath_hal *ah, int init_cal_count)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_CHANNEL_INTERNAL ichan; /* bogus */
    bool is_cal_done;
    HAL_CAL_LIST *curr_cal;
    int i;

    curr_cal = ahp->ah_cal_list_curr;
    if (curr_cal == AH_NULL) {
        return false;
    }
    ichan.cal_valid = 0;

    for (i = 0; i < init_cal_count; i++) {
        /* Reset this Cal */
        ar9300_reset_calibration(ah, curr_cal);
        /* Poll for offset calibration complete */
        if (!ath_hal_wait(
                ah, AR_PHY_TIMING4, AR_PHY_TIMING4_DO_CAL, 0, AH_WAIT_TIMEOUT))
        {
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s: Cal %d failed to complete in 100ms.\n",
                __func__, curr_cal->cal_data->cal_type);
            /* Re-initialize list pointers for periodic cals */
            ahp->ah_cal_list = ahp->ah_cal_list_last = ahp->ah_cal_list_curr
                = AH_NULL;
            return false;
        }
        /* Run this cal */
        ar9300_per_calibration(
            ah, &ichan, ahp->ah_rx_chainmask, curr_cal, &is_cal_done);
        if (is_cal_done == false) {
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s: Not able to run Init Cal %d.\n", __func__,
                curr_cal->cal_data->cal_type);
        }
        if (curr_cal->cal_next) {
            curr_cal = curr_cal->cal_next;
        }
    }

    /* Re-initialize list pointers for periodic cals */
    ahp->ah_cal_list = ahp->ah_cal_list_last = ahp->ah_cal_list_curr = AH_NULL;
    return true;
}

#if 0
static void
ar9300_tx_carrier_leak_war(struct ath_hal *ah)
{
    unsigned long tx_gain_table_max;
    unsigned long reg_bb_cl_map_0_b0 = 0xffffffff;
    unsigned long reg_bb_cl_map_1_b0 = 0xffffffff;
    unsigned long reg_bb_cl_map_2_b0 = 0xffffffff;
    unsigned long reg_bb_cl_map_3_b0 = 0xffffffff;
    unsigned long tx_gain, cal_run = 0;
    unsigned long cal_gain[AR_PHY_TPC_7_TX_GAIN_TABLE_MAX + 1];
    unsigned long cal_gain_index[AR_PHY_TPC_7_TX_GAIN_TABLE_MAX + 1];
    unsigned long new_gain[AR_PHY_TPC_7_TX_GAIN_TABLE_MAX + 1];
    int i, j;

    OS_MEMSET(new_gain, 0, sizeof(new_gain));
    /*printf("     Running TxCarrierLeakWAR\n");*/

    /* process tx gain table, we use cl_map_hw_gen=0. */
    OS_REG_RMW_FIELD(ah, AR_PHY_CL_CAL_CTL, AR_PHY_CL_MAP_HW_GEN, 0);

	//the table we used is txbb_gc[2:0], 1dB[2:1].
    tx_gain_table_max = OS_REG_READ_FIELD(ah,
        AR_PHY_TPC_7, AR_PHY_TPC_7_TX_GAIN_TABLE_MAX);

    for (i = 0; i <= tx_gain_table_max; i++) {
        tx_gain = OS_REG_READ(ah, AR_PHY_TXGAIN_TAB(1) + i * 4);
        cal_gain[i] = (((tx_gain >> 5)& 0x7) << 2) |
            (((tx_gain >> 1) & 0x3) << 0);
        if (i == 0) {
            cal_gain_index[i] = cal_run;
            new_gain[i] = 1;
            cal_run++;
        } else {
            new_gain[i] = 1;
            for (j = 0; j < i; j++) {
                /*
                printf("i=%d, j=%d cal_gain[$i]=0x%04x\n", i, j, cal_gain[i]);
                 */
                if (new_gain[i]) {
                    if ((cal_gain[i] != cal_gain[j])) {
                        new_gain[i] = 1;
                    } else {
                        /* if old gain found, use old cal_run value. */
                        new_gain[i] = 0;
                        cal_gain_index[i] = cal_gain_index[j];
                    }
                }
            }
            /* if new gain found, increase cal_run */
            if (new_gain[i] == 1) {
                cal_gain_index[i] = cal_run;
                cal_run++;
            }
        }

        reg_bb_cl_map_0_b0 = (reg_bb_cl_map_0_b0 & ~(0x1 << i)) |
            ((cal_gain_index[i] >> 0 & 0x1) << i);
        reg_bb_cl_map_1_b0 = (reg_bb_cl_map_1_b0 & ~(0x1 << i)) |
            ((cal_gain_index[i] >> 1 & 0x1) << i);
        reg_bb_cl_map_2_b0 = (reg_bb_cl_map_2_b0 & ~(0x1 << i)) |
            ((cal_gain_index[i] >> 2 & 0x1) << i);
        reg_bb_cl_map_3_b0 = (reg_bb_cl_map_3_b0 & ~(0x1 << i)) |
            ((cal_gain_index[i] >> 3 & 0x1) << i);

        /*
        printf("i=%2d, cal_gain[$i]= 0x%04x, cal_run= %d, "
            "cal_gain_index[i]=%d, new_gain[i] = %d\n",
            i, cal_gain[i], cal_run, cal_gain_index[i], new_gain[i]);
         */
    }
    OS_REG_WRITE(ah, AR_PHY_CL_MAP_0_B0, reg_bb_cl_map_0_b0);
    OS_REG_WRITE(ah, AR_PHY_CL_MAP_1_B0, reg_bb_cl_map_1_b0);
    OS_REG_WRITE(ah, AR_PHY_CL_MAP_2_B0, reg_bb_cl_map_2_b0);
    OS_REG_WRITE(ah, AR_PHY_CL_MAP_3_B0, reg_bb_cl_map_3_b0);
    if (AR_SREV_WASP(ah)) {
        OS_REG_WRITE(ah, AR_PHY_CL_MAP_0_B1, reg_bb_cl_map_0_b0);
        OS_REG_WRITE(ah, AR_PHY_CL_MAP_1_B1, reg_bb_cl_map_1_b0);
        OS_REG_WRITE(ah, AR_PHY_CL_MAP_2_B1, reg_bb_cl_map_2_b0);
        OS_REG_WRITE(ah, AR_PHY_CL_MAP_3_B1, reg_bb_cl_map_3_b0);
    }
}
#endif


static inline void
ar9300_invalidate_saved_cals(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichan)
{
#if ATH_SUPPORT_CAL_REUSE
    if (AH_PRIVATE(ah)->ah_config.ath_hal_cal_reuse &
        ATH_CAL_REUSE_REDO_IN_FULL_RESET)
    {
        ichan->one_time_txiqcal_done = false;
        ichan->one_time_txclcal_done = false;
#if ATH_SUPPORT_RADIO_RETENTION
        ichan->rtt.saved = 0;
#endif
    }
#endif
}

static inline bool
ar9300_restore_rtt_cals(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichan)
{
    bool restore_status = false;
#if ATH_SUPPORT_RADIO_RETENTION
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int32_t *table;
    int i;

    if (ahp->radio_retention_enable && ichan->rtt.saved)
    {
        HDPRINTF(ah, HAL_DBG_FCS_RTT,
            "(RTT) %s: enable RTT - chan = %d\n", __func__, ichan->channel);
        ar9300_rtt_enable(ah);
        ar9300_rtt_set_mask(ah, 0x00); // 000000

        for(i=0; i<AH9300(ah)->radio_retention_chains; i++) {
            table = &ichan->rtt.table[ichan->rtt.last][i][0];
            ar9300_rtt_write_table(ah, i, table, AH_RTT_MAX_NUM_TABLE_ENTRY);
        }

        restore_status = ar9300_rtt_force_restore(ah);

        ar9300_rtt_disable(ah);
        HDPRINTF(ah, HAL_DBG_FCS_RTT, "(RTT) %s: disable RTT\n", __func__);
    } else {
        HDPRINTF(ah, HAL_DBG_FCS_RTT, "(RTT) no saved cal results\n");
    }
#endif /* ATH_SUPPORT_RADIO_RETENTION */

    return restore_status;
}

static void ar9300_manual_pk_cal(struct ath_hal *ah, int is_2g)
{
	int bit=5;
	int agc_comparator_out0=0;
	int caldac_total0=0;
	int caldac_offset0[10];
	int cal_dac_test0=0;
	int agc_comparator_out1=0;
	int caldac_total1=0;
	int caldac_offset1[10];
	int cal_dac_test1=0;
	int agc_comparator_out2=0;
	int caldac_total2=0;
	int caldac_offset2[10];
	int cal_dac_test2=0;
	int i=0;
	int peak_detect_threshold=11;

	for(i=0; i<10; i++) {
		caldac_offset0[i] = 0;
		caldac_offset1[i] = 0;
		caldac_offset2[i] = 0;
	}
	if(AR_SREV_SCORPION(ah) || AR_SREV_HONEYBEE(ah)) {
		peak_detect_threshold=8;
	} else if(AR_SREV_WASP(ah) || AR_SREV_OSPREY(ah) || AR_SREV_AR9580(ah)) {
        peak_detect_threshold=0;
    }
	// Turn off LNA/SW
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRF_GAINSTAGES, AR_PHY_65NM_RXRF_GAINSTAGES_RX_OVERRIDE,  1);
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRF_GAINSTAGES, AR_PHY_65NM_RXRF_GAINSTAGES_LNAON_CALDC,  0);
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXRF_GAINSTAGES, AR_PHY_65NM_RXRF_GAINSTAGES_RX_OVERRIDE,  1);
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXRF_GAINSTAGES, AR_PHY_65NM_RXRF_GAINSTAGES_LNAON_CALDC,  0);
	// Turn off rxon, YKChen
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_65NM_CH0_RXTX2_RXON_OVR,  1);
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_65NM_CH0_RXTX2_RXON,  0);
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_65NM_CH0_RXTX2_RXON_OVR,  1);
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_65NM_CH0_RXTX2_RXON,  0);
	// Turn on AGC for cal
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_OVERRIDE,  1);     //long shift enable
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_ON_OVR,  1);     //long shift enable
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_CAL_OVR,  1);     //long shift enable
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_DBDAC_OVR,  peak_detect_threshold);     //long shift enable
    if(is_2g){
        OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRFSA_AGC,
                AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_DBDAC_OVR, peak_detect_threshold);
    }
    else{
        OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRFSA_AGC,
                AR_PHY_65NM_CH0_RXRF_AGC_AGC5G_DBDAC_OVR, peak_detect_threshold);
    }

	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_OVERRIDE,  1);     //long shift enable
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_ON_OVR,  1);     //long shift enable
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_CAL_OVR,  1);     //long shift enable
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_DBDAC_OVR,  peak_detect_threshold);     //long shift enable
    if(is_2g){
        OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRFSA_AGC,
                AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_DBDAC_OVR, peak_detect_threshold);
    }
    else{
        OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRFSA_AGC,
                AR_PHY_65NM_CH0_RXRF_AGC_AGC5G_DBDAC_OVR, peak_detect_threshold);
    }
    if (!(AR_SREV_HONEYBEE(ah) || AR_SREV_WASP(ah))) {
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXRF_GAINSTAGES, AR_PHY_65NM_RXRF_GAINSTAGES_RX_OVERRIDE,  1);
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXRF_GAINSTAGES, AR_PHY_65NM_RXRF_GAINSTAGES_LNAON_CALDC,  0);
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_65NM_CH0_RXTX2_RXON_OVR,  1);
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_65NM_CH0_RXTX2_RXON,  0);
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_OVERRIDE,  1);     //long shift enable
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_ON_OVR,  1);     //long shift enable
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_CAL_OVR,  1);     //long shift enable
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_DBDAC_OVR,  peak_detect_threshold);     //long shift enable
        if(is_2g){
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRFSA_AGC,
                    AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_DBDAC_OVR, peak_detect_threshold);
        }
        else{
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRFSA_AGC,
                    AR_PHY_65NM_CH0_RXRF_AGC_AGC5G_DBDAC_OVR, peak_detect_threshold);
        }
	}

	// Binary search check caldac_offset
	for (bit=6;bit>0;bit--) {
		caldac_offset0[bit]=1<<(bit-1);
		cal_dac_test0=caldac_total0+caldac_offset0[bit];
		caldac_offset1[bit]=1<<(bit-1);
		cal_dac_test1=caldac_total1+caldac_offset1[bit];
		caldac_offset2[bit]=1<<(bit-1);
		cal_dac_test2=caldac_total2+caldac_offset2[bit];
        if(is_2g) {
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_CALDAC_OVR, cal_dac_test0);
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_CALDAC_OVR, cal_dac_test1);
            if (!(AR_SREV_HONEYBEE(ah) || AR_SREV_WASP(ah))) {
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_CALDAC_OVR, cal_dac_test2);
            }
        } else {
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC5G_CALDAC_OVR, cal_dac_test0);
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC5G_CALDAC_OVR, cal_dac_test1);
            if (!(AR_SREV_WASP(ah))) {
                OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC5G_CALDAC_OVR, cal_dac_test2);
            }
        }
		OS_DELAY(100);

		agc_comparator_out0 = OS_REG_READ_FIELD(ah, AR_PHY_65NM_CH0_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_OUT);
		if (agc_comparator_out0 == 0){
			caldac_offset0[bit] = 1;
		} else{
			caldac_offset0[bit] = 0;
		}
		caldac_total0 += caldac_offset0[bit] << (bit-1) ;
		/*printf("%s(): agc_comparator_out0=%d, bit=%d, caldac_offset0=%d, cal_dac_test0=0x%X.\n",
			__FUNCTION__, agc_comparator_out0, bit,caldac_offset0[bit],cal_dac_test0);*/

		agc_comparator_out1 = OS_REG_READ_FIELD(ah, AR_PHY_65NM_CH1_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_OUT);
		if (agc_comparator_out1 == 0){
			caldac_offset1[bit] = 1;
		} else{
			caldac_offset1[bit] = 0;
		}
		caldac_total1 += caldac_offset1[bit] << (bit-1) ;
		/*printf("%s(): agc_comparator_out1=%d, bit=%d, caldac_offset1=%d, cal_dac_test1=0x%X.\n",
			__FUNCTION__, agc_comparator_out1, bit,caldac_offset1[bit],cal_dac_test1);*/

	    if (!(AR_SREV_HONEYBEE(ah) || AR_SREV_WASP(ah))) {	
			agc_comparator_out2 = OS_REG_READ_FIELD(ah, AR_PHY_65NM_CH2_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_OUT);
			if (agc_comparator_out2 == 0){
				caldac_offset2[bit] = 1;
			} else{
				caldac_offset2[bit] = 0;
			}
			caldac_total2 += caldac_offset2[bit] << (bit-1) ;
			/*printf("%s(): agc_comparator_out2=%d, bit=%d, caldac_offset2=%d, cal_dac_test2=0x%X.\n",
				__FUNCTION__, agc_comparator_out2, bit,caldac_offset2[bit],cal_dac_test2);*/
		}
	}

	/*printf("%s(): caldac_total0=%d, caldac_total1=%d, caldac_total2=%d.\n",
		__FUNCTION__, caldac_total0, caldac_total1, caldac_total2);*/

    if(is_2g) {
        OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_CALDAC_OVR, caldac_total0);
        OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_CALDAC_OVR, caldac_total1);
    } else {
        OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC5G_CALDAC_OVR, caldac_total0);
        OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC5G_CALDAC_OVR, caldac_total1);
    }
	agc_comparator_out0 = OS_REG_READ_FIELD(ah, AR_PHY_65NM_CH0_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_OUT);
	agc_comparator_out1 = OS_REG_READ_FIELD(ah, AR_PHY_65NM_CH1_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_OUT);
	/*
	printf("%s(): ch0.AGC2G_CALDAC_OVR=%d, agc_comparator_out0=%d.\n",
		__FUNCTION__, OS_REG_READ_FIELD(ah, AR_PHY_65NM_CH0_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_CALDAC_OVR), agc_comparator_out0);
	printf("%s(): ch1.AGC2G_CALDAC_OVR=%d, agc_comparator_out1=%d.\n",
		__FUNCTION__, OS_REG_READ_FIELD(ah, AR_PHY_65NM_CH1_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_CALDAC_OVR), agc_comparator_out1);
       */
	if (!(AR_SREV_HONEYBEE(ah) || AR_SREV_WASP(ah))) {
        if(is_2g) {
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_CALDAC_OVR, caldac_total2);
        } else {
            OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC5G_CALDAC_OVR, caldac_total2);
        }
		/*
		agc_comparator_out2 = OS_REG_READ_FIELD(ah, AR_PHY_65NM_CH2_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_OUT);
		printf("%s(): ch2.AGC2G_CALDAC_OVR=%d, agc_comparator_out2=%d.\n",
			__FUNCTION__, OS_REG_READ_FIELD(ah, AR_PHY_65NM_CH2_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC2G_CALDAC_OVR), agc_comparator_out2);
		*/
	}

	// Turn on LNA, YKChen
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRF_GAINSTAGES, AR_PHY_65NM_RXRF_GAINSTAGES_RX_OVERRIDE, 0);
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXRF_GAINSTAGES, AR_PHY_65NM_RXRF_GAINSTAGES_RX_OVERRIDE, 0);
	// Turn off rxon_ovr
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXTX2, AR_PHY_65NM_CH0_RXTX2_RXON_OVR,  0);
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXTX2, AR_PHY_65NM_CH0_RXTX2_RXON_OVR,  0);
	// Turn off peak detector calibration, YKChen
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH0_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_CAL_OVR,  0);      //disalbe calibrate agc, YKChen
	OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH1_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_CAL_OVR,  0);      //disalbe calibrate agc, YKChen

	if (!(AR_SREV_HONEYBEE(ah) || AR_SREV_WASP(ah))) {
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXRF_GAINSTAGES, AR_PHY_65NM_RXRF_GAINSTAGES_RX_OVERRIDE, 0);
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXTX2, AR_PHY_65NM_CH0_RXTX2_RXON_OVR,  0);
		OS_REG_RMW_FIELD(ah, AR_PHY_65NM_CH2_RXRF_AGC, AR_PHY_65NM_CH0_RXRF_AGC_AGC_CAL_OVR,  0);      //disalbe calibrate agc, YKChen
	}
}

/* ar9300_init_cal
 * Initialize Calibration infrastructure
 */
static inline bool
ar9300_init_cal_internal(struct ath_hal *ah, HAL_CHANNEL *chan,
                         HAL_CHANNEL_INTERNAL *ichan, bool enable_rtt,
                         bool do_rtt_cal, bool skip_if_none, bool apply_last_iqcorr)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    bool txiqcal_success_flag = false;
    bool cal_done = false;
    int iqcal_idx = 0;
    bool do_sep_iq_cal = false;
    bool is_cal_reusable = true;
    bool do_agc_cal = do_rtt_cal;
    u_int32_t temp;
    int ch0_done,osdac_ch0,dc_off_ch0_i1,dc_off_ch0_q1,dc_off_ch0_i2,
	dc_off_ch0_q2,dc_off_ch0_i3,dc_off_ch0_q3;
    int ch1_done,osdac_ch1,dc_off_ch1_i1,dc_off_ch1_q1,dc_off_ch1_i2,
	dc_off_ch1_q2,dc_off_ch1_i3,dc_off_ch1_q3;
    int ch2_done,osdac_ch2,dc_off_ch2_i1,dc_off_ch2_q1,dc_off_ch2_i2,
	dc_off_ch2_q2,dc_off_ch2_i3,dc_off_ch2_q3;

#define OFF_UPPER_LT 24
#define OFF_LOWER_LT 7
#if ATH_SUPPORT_RADIO_RETENTION
    u_int32_t agc_control, cal_enable_flags = 0, cal_enable_masks;
#endif
#if ATH_SUPPORT_CAL_REUSE
    bool      cal_reuse_enable = AH_PRIVATE(ah)->ah_config.ath_hal_cal_reuse &
                                 ATH_CAL_REUSE_ENABLE;
    bool      clc_success = false;
    int32_t   ch_idx, j, cl_tab_reg;
    u_int32_t BB_cl_tab_entry = MAX_BB_CL_TABLE_ENTRY;
    u_int32_t BB_cl_tab_b[AR9300_MAX_CHAINS] = {
                    AR_PHY_CL_TAB_0,
                    AR_PHY_CL_TAB_1,
                    AR_PHY_CL_TAB_2
                };
#endif

    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah) || AR_SREV_APHRODITE(ah)) {
        /* Hornet: 1 x 1 */
        ahp->ah_rx_cal_chainmask = 0x1;
        ahp->ah_tx_cal_chainmask = 0x1;
    } else if (AR_SREV_WASP(ah) || AR_SREV_JUPITER(ah) || AR_SREV_HONEYBEE(ah)) {
        /* Wasp/Jupiter: 2 x 2 */
#ifdef JUPITER_EMULATION_1_CHAIN
        ahp->ah_rx_cal_chainmask = 0x1;
        ahp->ah_tx_cal_chainmask = 0x1;
#else
        ahp->ah_rx_cal_chainmask = 0x3;
        ahp->ah_tx_cal_chainmask = 0x3;
#endif
    } else {
        /*
         * Osprey needs to be configured for the correct chain mode
         * before running AGC/TxIQ cals.
         */
        if (ahp->ah_enterprise_mode & AR_ENT_OTP_CHAIN2_DISABLE) {
            /* chain 2 disabled - 2 chain mode */
            ahp->ah_rx_cal_chainmask = 0x3;
            ahp->ah_tx_cal_chainmask = 0x3;
        } else {
            ahp->ah_rx_cal_chainmask = 0x7;
            ahp->ah_tx_cal_chainmask = 0x7;
        }
    }
        ar9300_init_chain_masks(ah, ahp->ah_rx_cal_chainmask, ahp->ah_tx_cal_chainmask);

#if ATH_SUPPORT_RADIO_RETENTION
    if (ahp->radio_retention_enable && !do_rtt_cal) {
        /* save current CAL control settings in BB_agc_control
         *   bit 11:CAL_enable (Offset Calibration)
         *   bit 16:enable_fltr_cal
         *   bit 20:enable_pkdet_cal
         */
        agc_control = OS_REG_READ(ah, AR_PHY_AGC_CONTROL);

        /* mask all bits */
        cal_enable_masks = AR_PHY_AGC_CONTROL_OFFSET_CAL |
                           AR_PHY_AGC_CONTROL_FLTR_CAL |
                           AR_PHY_AGC_CONTROL_PKDET_CAL;
        cal_enable_flags = agc_control & cal_enable_masks;
        agc_control &= ~cal_enable_masks;

        /* disable individual cal bits */
        OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL, agc_control);
    }
#endif /* ATH_SUPPORT_RADIO_RETENTION */

    if (ahp->tx_cl_cal_enable) {
#if ATH_SUPPORT_CAL_REUSE
        /* disable Carrie Leak or set do_agc_cal accordingly */
        if (cal_reuse_enable && ichan->one_time_txclcal_done)
        {
            OS_REG_CLR_BIT(ah, AR_PHY_CL_CAL_CTL, AR_PHY_CL_CAL_ENABLE);
        } else
#endif /* ATH_SUPPORT_CAL_REUSE */
        {
            OS_REG_SET_BIT(ah, AR_PHY_CL_CAL_CTL, AR_PHY_CL_CAL_ENABLE);
            do_agc_cal = true;
        }
    }

    /* Do Tx IQ Calibration here for osprey hornet and wasp */
    /* XXX: For initial wasp bringup - check and enable this */
    /* EV 74233: Tx IQ fails to complete for half/quarter rates */
    if (!(IS_CHAN_HALF_RATE(ichan) || IS_CHAN_QUARTER_RATE(ichan))) {
        if (ahp->tx_iq_cal_enable) {
            /* this should be eventually moved to INI file */
            OS_REG_RMW_FIELD(ah, AR_PHY_TX_IQCAL_CONTROL_1(ah),
                AR_PHY_TX_IQCAL_CONTROL_1_IQCORR_I_Q_COFF_DELPT, DELPT);

            /*
             * For poseidon and later chips,
             * Tx IQ cal HW run will be a part of AGC calibration
             */
            if (ahp->tx_iq_cal_during_agc_cal) {
                /*
                 * txiqcal_success_flag always set to 1 to run
                 *     ar9300_tx_iq_cal_post_proc
                 * if following AGC cal passes
                */
#if ATH_SUPPORT_CAL_REUSE
                if (!cal_reuse_enable || !ichan->one_time_txiqcal_done)
                {
                    txiqcal_success_flag = true;
                    OS_REG_WRITE(ah, AR_PHY_TX_IQCAL_CONTROL_0(ah),
                        OS_REG_READ(ah, AR_PHY_TX_IQCAL_CONTROL_0(ah)) |
                        AR_PHY_TX_IQCAL_CONTROL_0_ENABLE_TXIQ_CAL);
                } else {
                    OS_REG_WRITE(ah, AR_PHY_TX_IQCAL_CONTROL_0(ah),
                        OS_REG_READ(ah, AR_PHY_TX_IQCAL_CONTROL_0(ah)) &
                        (~AR_PHY_TX_IQCAL_CONTROL_0_ENABLE_TXIQ_CAL));
                }
#else
		if (OS_REG_READ_FIELD(ah,
					AR_PHY_TX_IQCAL_CONTROL_0(ah),
					AR_PHY_TX_IQCAL_CONTROL_0_ENABLE_TXIQ_CAL)){
			if (apply_last_iqcorr == true) {
				OS_REG_CLR_BIT(ah, AR_PHY_TX_IQCAL_CONTROL_0(ah),
						AR_PHY_TX_IQCAL_CONTROL_0_ENABLE_TXIQ_CAL);
				txiqcal_success_flag = false;
			} else {
				txiqcal_success_flag = true;
			}
		}else{
			txiqcal_success_flag = false;
		}
#endif
                if (txiqcal_success_flag) {
                    do_agc_cal = true;
                }
            } else
#if ATH_SUPPORT_CAL_REUSE
            if (!cal_reuse_enable || !ichan->one_time_txiqcal_done)
#endif
            {
                do_sep_iq_cal = true;
                do_agc_cal = true;
            }
        }
    }

#if ATH_SUPPORT_MCI
    if (AH_PRIVATE(ah)->ah_caps.hal_mci_support &&
        IS_CHAN_2GHZ(ichan) &&
        (ahp->ah_mci_bt_state == MCI_BT_AWAKE) &&
        do_agc_cal &&
        !(AH_PRIVATE(ah)->ah_config.ath_hal_mci_config &
        ATH_MCI_CONFIG_DISABLE_MCI_CAL))
    {
        u_int32_t payload[4] = {0, 0, 0, 0};

        /* Send CAL_REQ only when BT is AWAKE. */
        HDPRINTF(ah, HAL_DBG_BT_COEX, "(MCI) %s: Send WLAN_CAL_REQ 0x%X\n",
            __func__, ahp->ah_mci_wlan_cal_seq);
        MCI_GPM_SET_CAL_TYPE(payload, MCI_GPM_WLAN_CAL_REQ);
        payload[MCI_GPM_WLAN_CAL_W_SEQUENCE] = ahp->ah_mci_wlan_cal_seq++;
        ar9300_mci_send_message(ah, MCI_GPM, 0, payload, 16, true, false);

        /* Wait BT_CAL_GRANT for 50ms */
        HDPRINTF(ah, HAL_DBG_BT_COEX,
            "(MCI) %s: Wait for BT_CAL_GRANT\n", __func__);
        if (ar9300_mci_wait_for_gpm(ah, MCI_GPM_BT_CAL_GRANT, 0, 50000))
        {
            HDPRINTF(ah, HAL_DBG_BT_COEX,
                "(MCI) %s: Got BT_CAL_GRANT.\n", __func__);
        }
        else {
            is_cal_reusable = false;
            HDPRINTF(ah, HAL_DBG_BT_COEX,
                "(MCI) %s: BT is not responding.\n", __func__);
        }
    }
#endif /* ATH_SUPPORT_MCI */

    if (do_sep_iq_cal)
    {
        /* enable Tx IQ Calibration HW for osprey/hornet/wasp */
        txiqcal_success_flag = ar9300_tx_iq_cal_hw_run(ah);
        OS_REG_WRITE(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_DIS);
        OS_DELAY(5);
        OS_REG_WRITE(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);
    }
#if 0
    if (AR_SREV_HORNET(ah) || AR_SREV_POSEIDON(ah)) {
        ar9300_tx_carrier_leak_war(ah);
    }
#endif
	//Hornet 1.1 workaround peak detect cal RMA#94260
	//call peak detect workaround
	if (AR_SREV_DRAGONFLY(ah) || AR_SREV_SCORPION(ah) || AR_SREV_HONEYBEE(ah) || AR_SREV_OSPREY(ah) || AR_SREV_WASP(ah) || AR_SREV_AR9580(ah)) {
        ar9300_manual_pk_cal(ah, IS_CHAN_2GHZ(ichan));
    }

/*	The below is commented out assuming that the rx_chain_mask will be always 1,3,7 but ART
	uses individual rx chain mask hence the below is taken care in the HAL reset initiated by ART

	if((IS_CHAN_2GHZ(chan)&& AR_SREV_SCORPION(ah))&&
       ((ahp->ah_rx_chainmask==0x2) || (ahp->ah_rx_chainmask==0x4))) {
       printf("digital dc offset WAR in place\n");
	      OS_REG_WRITE(ah, AR_PHY_AGC_DIG_DC_CTRL,
		      OS_REG_READ(ah, AR_PHY_AGC_DIG_DC_CTRL) & 0xfffffffe);
    }

*/

// Dynamic OSDAC selection only for Scorpion 2GHz . To fix EV#130174
	if(IS_CHAN_2GHZ(chan) && AR_SREV_SCORPION(ah)) {
		OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_OFFSET_CAL);
		OS_REG_CLR_BIT(ah, AR_PHY_TX_IQCAL_CONTROL_0(ah),
			AR_PHY_TX_IQCAL_CONTROL_0_ENABLE_TXIQ_CAL);
		OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL,
			OS_REG_READ(ah, AR_PHY_AGC_CONTROL) | AR_PHY_AGC_CONTROL_CAL);
		if (!ath_hal_wait( ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_CAL,
			0, AH_WAIT_TIMEOUT)) {
				HDPRINTF(ah, HAL_DBG_CALIBRATE,
					"%s: AGC cal without offset cal failed to complete in 1ms; "
					"noisy environment?\n", __func__);
				return false;
		}

		OS_REG_SET_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_OFFSET_CAL);

		OS_REG_CLR_BIT(ah, AR_PHY_CL_CAL_CTL, AR_PHY_CL_CAL_ENABLE);
		OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_FLTR_CAL);
		OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_PKDET_CAL);

		ch0_done = 0;
		ch1_done = 0;
		ch2_done = 0;

		while((ch0_done==0) || (ch1_done==0) || (ch2_done==0))	{
			osdac_ch0 =  (OS_REG_READ(ah, AR_PHY_65NM_CH0_BB1) >> 30) & 0x3;
			osdac_ch1 =  (OS_REG_READ(ah, AR_PHY_65NM_CH1_BB1) >> 30) & 0x3;
			osdac_ch2 =  (OS_REG_READ(ah, AR_PHY_65NM_CH2_BB1) >> 30) & 0x3;


			OS_REG_SET_BIT(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);
			OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL,
				OS_REG_READ(ah, AR_PHY_AGC_CONTROL) | AR_PHY_AGC_CONTROL_CAL);
			if (!ath_hal_wait( ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_CAL,
				0, AH_WAIT_TIMEOUT)) {
					HDPRINTF(ah, HAL_DBG_CALIBRATE,
						"%s: DC offset cal failed to complete in 1ms; "
						"noisy environment?\n", __func__);
					return false;
			}
			OS_REG_CLR_BIT(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);

			OS_REG_WRITE(ah,AR_PHY_65NM_CH0_BB3,
				((OS_REG_READ(ah,AR_PHY_65NM_CH0_BB3) & 0xfffffcff)| (1 << 8)));
			OS_REG_WRITE(ah,AR_PHY_65NM_CH1_BB3,
				((OS_REG_READ(ah,AR_PHY_65NM_CH1_BB3) & 0xfffffcff)| (1 << 8)));
			OS_REG_WRITE(ah,AR_PHY_65NM_CH2_BB3,
				((OS_REG_READ(ah,AR_PHY_65NM_CH2_BB3) & 0xfffffcff)| (1 << 8)));

			temp = OS_REG_READ(ah,AR_PHY_65NM_CH0_BB3);
			dc_off_ch0_i1 = (temp >> 26) & 0x1f;
			dc_off_ch0_q1 = (temp >> 21) & 0x1f;
			temp = OS_REG_READ(ah,AR_PHY_65NM_CH1_BB3);
			dc_off_ch1_i1 = (temp >> 26) & 0x1f;
			dc_off_ch1_q1 = (temp >> 21) & 0x1f;
			temp = OS_REG_READ(ah,AR_PHY_65NM_CH2_BB3);
			dc_off_ch2_i1 = (temp >> 26) & 0x1f;
			dc_off_ch2_q1 = (temp >> 21) & 0x1f;

			OS_REG_WRITE(ah,AR_PHY_65NM_CH0_BB3,
				((OS_REG_READ(ah,AR_PHY_65NM_CH0_BB3) & 0xfffffcff)| (2 << 8)));
			OS_REG_WRITE(ah,AR_PHY_65NM_CH1_BB3,
				((OS_REG_READ(ah,AR_PHY_65NM_CH1_BB3) & 0xfffffcff)| (2 << 8)));
			OS_REG_WRITE(ah,AR_PHY_65NM_CH2_BB3,
				((OS_REG_READ(ah,AR_PHY_65NM_CH2_BB3) & 0xfffffcff)| (2 << 8)));

			temp = OS_REG_READ(ah,AR_PHY_65NM_CH0_BB3);
			dc_off_ch0_i2 = (temp >> 26) & 0x1f;
			dc_off_ch0_q2 = (temp >> 21) & 0x1f;
			temp = OS_REG_READ(ah,AR_PHY_65NM_CH1_BB3);
			dc_off_ch1_i2 = (temp >> 26) & 0x1f;
			dc_off_ch1_q2 = (temp >> 21) & 0x1f;
			temp = OS_REG_READ(ah,AR_PHY_65NM_CH2_BB3);
			dc_off_ch2_i2 = (temp >> 26) & 0x1f;
			dc_off_ch2_q2 = (temp >> 21) & 0x1f;

			OS_REG_WRITE(ah,AR_PHY_65NM_CH0_BB3,
				((OS_REG_READ(ah,AR_PHY_65NM_CH0_BB3) & 0xfffffcff)| (3 << 8)));
			OS_REG_WRITE(ah,AR_PHY_65NM_CH1_BB3,
				((OS_REG_READ(ah,AR_PHY_65NM_CH1_BB3) & 0xfffffcff)| (3 << 8)));
			OS_REG_WRITE(ah,AR_PHY_65NM_CH2_BB3,
				((OS_REG_READ(ah,AR_PHY_65NM_CH2_BB3) & 0xfffffcff)| (3 << 8)));

			temp = OS_REG_READ(ah,AR_PHY_65NM_CH0_BB3);
			dc_off_ch0_i3 = (temp >> 26) & 0x1f;
			dc_off_ch0_q3 = (temp >> 21) & 0x1f;
			temp = OS_REG_READ(ah,AR_PHY_65NM_CH1_BB3);
			dc_off_ch1_i3 = (temp >> 26) & 0x1f;
			dc_off_ch1_q3 = (temp >> 21) & 0x1f;
			temp = OS_REG_READ(ah,AR_PHY_65NM_CH2_BB3);
			dc_off_ch2_i3 = (temp >> 26) & 0x1f;
			dc_off_ch2_q3 = (temp >> 21) & 0x1f;

			//check for ch0
			if((dc_off_ch0_i1 > OFF_UPPER_LT) || (dc_off_ch0_i1 < OFF_LOWER_LT) ||
				(dc_off_ch0_i2 > OFF_UPPER_LT) || (dc_off_ch0_i2 < OFF_LOWER_LT) ||
				(dc_off_ch0_i3 > OFF_UPPER_LT) || (dc_off_ch0_i3 < OFF_LOWER_LT) ||
				(dc_off_ch0_q1 > OFF_UPPER_LT) || (dc_off_ch0_q1 < OFF_LOWER_LT) ||
				(dc_off_ch0_q2 > OFF_UPPER_LT) || (dc_off_ch0_q2 < OFF_LOWER_LT) ||
				(dc_off_ch0_q3 > OFF_UPPER_LT) || (dc_off_ch0_q3 < OFF_LOWER_LT)) {
					if(osdac_ch0 == 3) {
						ch0_done = 1;
					}
					else {
						osdac_ch0++;
						OS_REG_WRITE(ah,AR_PHY_65NM_CH0_BB1,
							((OS_REG_READ(ah,AR_PHY_65NM_CH0_BB1) & 0x3fffffff) | (osdac_ch0 << 30)));
						ch0_done = 0;
					}
			}else {
				ch0_done = 1;
			}

			//check for ch1
			if((dc_off_ch1_i1 > OFF_UPPER_LT) || (dc_off_ch1_i1 < OFF_LOWER_LT) ||
				(dc_off_ch1_i2 > OFF_UPPER_LT) || (dc_off_ch1_i2 < OFF_LOWER_LT) ||
				(dc_off_ch1_i3 > OFF_UPPER_LT) || (dc_off_ch1_i3 < OFF_LOWER_LT) ||
				(dc_off_ch1_q1 > OFF_UPPER_LT) || (dc_off_ch1_q1 < OFF_LOWER_LT) ||
				(dc_off_ch1_q2 > OFF_UPPER_LT) || (dc_off_ch1_q2 < OFF_LOWER_LT) ||
				(dc_off_ch1_q3 > OFF_UPPER_LT) || (dc_off_ch1_q3 < OFF_LOWER_LT)) {
					if(osdac_ch1 == 3) {
						ch1_done = 1;
					}
					else {
						osdac_ch1++;
						OS_REG_WRITE(ah,AR_PHY_65NM_CH1_BB1,
							((OS_REG_READ(ah,AR_PHY_65NM_CH1_BB1) & 0x3fffffff) | (osdac_ch1 << 30)));
						ch1_done = 0;
					}
			}else {
				ch1_done = 1;
			}

			//check for ch2
			if((dc_off_ch2_i1 > OFF_UPPER_LT) || (dc_off_ch2_i1 < OFF_LOWER_LT) ||
				(dc_off_ch2_i2 > OFF_UPPER_LT) || (dc_off_ch2_i2 < OFF_LOWER_LT) ||
				(dc_off_ch2_i3 > OFF_UPPER_LT) || (dc_off_ch2_i3 < OFF_LOWER_LT) ||
				(dc_off_ch2_q1 > OFF_UPPER_LT) || (dc_off_ch2_q1 < OFF_LOWER_LT) ||
				(dc_off_ch2_q2 > OFF_UPPER_LT) || (dc_off_ch2_q2 < OFF_LOWER_LT) ||
				(dc_off_ch2_q3 > OFF_UPPER_LT) || (dc_off_ch2_q3 < OFF_LOWER_LT)) {
					if(osdac_ch2 == 3) {
						ch2_done = 1;
					}
					else {
						osdac_ch2++;
						OS_REG_WRITE(ah,AR_PHY_65NM_CH2_BB1,
							((OS_REG_READ(ah,AR_PHY_65NM_CH2_BB1) & 0x3fffffff) | (osdac_ch2 << 30)));
						ch2_done = 0;
					}
			}else {
				ch2_done = 1;
			}


		}

		OS_REG_CLR_BIT(ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_OFFSET_CAL);//disbaling dc offset cal
		OS_REG_SET_BIT(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);

		if (txiqcal_success_flag) {
			OS_REG_SET_BIT(ah, AR_PHY_TX_IQCAL_CONTROL_0(ah),
				AR_PHY_TX_IQCAL_CONTROL_0_ENABLE_TXIQ_CAL);
		}
	}

    // End of Dynamic OSDAC selection
    /*
     * Calibrate the AGC
     *
     * Tx IQ cal is a part of AGC cal for Jupiter/Poseidon, etc.
     * please enable the bit of txiqcal_control_0[31] in INI file
     * for Jupiter/Poseidon/etc.
     */
    if(!AR_SREV_SCORPION(ah)) {
        if (do_agc_cal || !skip_if_none) {
            OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL,
                OS_REG_READ(ah, AR_PHY_AGC_CONTROL) | AR_PHY_AGC_CONTROL_CAL);

            /* Poll for offset calibration complete */
            cal_done = ath_hal_wait(ah,
                    AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_CAL, 0, AH_WAIT_TIMEOUT);
            if (!cal_done) {
                HDPRINTF(ah, HAL_DBG_FCS_RTT,
                    "(FCS) CAL NOT DONE!!! - %d\n", ichan->channel);
            }
        } else {
            cal_done = true;
        }
            /*
             * Tx IQ cal post-processing in SW
             * This part of code should be common to all chips,
             * no chip specific code for Jupiter/Posdeion except for register names.
             */
            if (txiqcal_success_flag) {
                ar9300_tx_iq_cal_post_proc(ah,ichan, 1, 1,is_cal_reusable,false);
            }else if (apply_last_iqcorr == true) {
                /* the apply_last_iqcorr is set true for Honeybee only */
                    ar9300_tx_iq_cal_post_proc(ah, ichan, 0, 0, is_cal_reusable, true);
            }
            } else {
        if (!txiqcal_success_flag) {
            OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL,
                OS_REG_READ(ah, AR_PHY_AGC_CONTROL) | AR_PHY_AGC_CONTROL_CAL);
            if (!ath_hal_wait( ah, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_CAL,
                    0, AH_WAIT_TIMEOUT)) {
                HDPRINTF(ah, HAL_DBG_CALIBRATE,
                    "%s: offset calibration failed to complete in 1ms; "
                    "noisy environment?\n", __func__);
                return false;
            }
            if (apply_last_iqcorr == true) {
                ar9300_tx_iq_cal_post_proc(ah, ichan, 0, 0, is_cal_reusable, true);
            }
        } else {
            for (iqcal_idx=0;iqcal_idx<MAXIQCAL;iqcal_idx++) {
                OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL,
                    OS_REG_READ(ah, AR_PHY_AGC_CONTROL) | AR_PHY_AGC_CONTROL_CAL);

                /* Poll for offset calibration complete */
                if (!ath_hal_wait(ah, AR_PHY_AGC_CONTROL,
                        AR_PHY_AGC_CONTROL_CAL, 0, AH_WAIT_TIMEOUT)) {
                    HDPRINTF(ah, HAL_DBG_CALIBRATE,
                        "%s: offset calibration failed to complete in 1ms; "
                        "noisy environment?\n", __func__);
                    return false;
                }
                /*
                 * Tx IQ cal post-processing in SW
                 * This part of code should be common to all chips,
                 * no chip specific code for Jupiter/Posdeion except for register names.
                 */
                ar9300_tx_iq_cal_post_proc(ah, ichan, iqcal_idx+1, MAXIQCAL, is_cal_reusable, false);
            }
       }
    }


#if ATH_SUPPORT_MCI
    if (AH_PRIVATE(ah)->ah_caps.hal_mci_support &&
        IS_CHAN_2GHZ(ichan) &&
        (ahp->ah_mci_bt_state == MCI_BT_AWAKE) &&
        do_agc_cal &&
        !(AH_PRIVATE(ah)->ah_config.ath_hal_mci_config &
        ATH_MCI_CONFIG_DISABLE_MCI_CAL))
    {
        u_int32_t payload[4] = {0, 0, 0, 0};

        HDPRINTF(ah, HAL_DBG_BT_COEX, "(MCI) %s: Send WLAN_CAL_DONE 0x%X\n",
            __func__, ahp->ah_mci_wlan_cal_done);
        MCI_GPM_SET_CAL_TYPE(payload, MCI_GPM_WLAN_CAL_DONE);
        payload[MCI_GPM_WLAN_CAL_W_SEQUENCE] = ahp->ah_mci_wlan_cal_done++;
        ar9300_mci_send_message(ah, MCI_GPM, 0, payload, 16, true, false);
    }
#endif /* ATH_SUPPORT_MCI */

#if ATH_SUPPORT_RADIO_RETENTION
    if (ahp->radio_retention_enable && !do_rtt_cal) {
        agc_control |= cal_enable_flags;
        OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL, agc_control);
    }
#endif /* ATH_SUPPORT_RADIO_RETENTION */

    if (!cal_done && !AR_SREV_SCORPION(ah) )
    {
#if ATH_SUPPORT_RADIO_RETENTION
        if (ahp->radio_retention_enable && enable_rtt ) {
            ar9300_rtt_disable(ah);
        }
#endif /* ATH_SUPPORT_RADIO_RETENTION */
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: offset calibration failed to complete in 1ms; "
            "noisy environment?\n", __func__);
        return false;
    }

#if 0
    /* Beacon stuck fix, refer to EV 120056 */
    if(IS_CHAN_2GHZ(chan) && AR_SREV_SCORPION(ah))
        OS_REG_WRITE(ah, AR_PHY_TIMING5, OS_REG_READ(ah,AR_PHY_TIMING5) & ~AR_PHY_TIMING5_CYCPWR_THR1_ENABLE);
#endif

#if 0
    /* Do PA Calibration */
    if (AR_SREV_KITE(ah) && AR_SREV_KITE_11_OR_LATER(ah)) {
        ar9285_pa_cal(ah);
    }
#endif

#if ATH_SUPPORT_CAL_REUSE
     if (ichan->one_time_txiqcal_done) {
        ar9300_tx_iq_cal_apply(ah, ichan);
        HDPRINTF(ah, HAL_DBG_FCS_RTT,
            "(FCS) TXIQCAL applied - %d\n", ichan->channel);
    }
#endif /* ATH_SUPPORT_CAL_REUSE */

#if ATH_SUPPORT_CAL_REUSE
    if (cal_reuse_enable && ahp->tx_cl_cal_enable)
    {
        clc_success = (OS_REG_READ(ah, AR_PHY_AGC_CONTROL) &
                  AR_PHY_AGC_CONTROL_CLC_SUCCESS) ? 1 : 0;

        if (ichan->one_time_txclcal_done)
        {
            /* reapply CL cal results */
            for (ch_idx = 0; ch_idx < AR9300_MAX_CHAINS; ch_idx++) {
                if ((ahp->ah_tx_cal_chainmask & (1 << ch_idx)) == 0) {
                    continue;
                }
                cl_tab_reg = BB_cl_tab_b[ch_idx];
                for (j = 0; j < BB_cl_tab_entry; j++) {
                    OS_REG_WRITE(ah, cl_tab_reg, ichan->tx_clcal[ch_idx][j]);
                    cl_tab_reg += 4;;
                }
            }
            HDPRINTF(ah, HAL_DBG_FCS_RTT,
                "(FCS) TX CL CAL applied - %d\n", ichan->channel);
        }
        else if (is_cal_reusable && clc_success) {
            /* save CL cal results */
            for (ch_idx = 0; ch_idx < AR9300_MAX_CHAINS; ch_idx++) {
                if ((ahp->ah_tx_cal_chainmask & (1 << ch_idx)) == 0) {
                    continue;
                }
                cl_tab_reg = BB_cl_tab_b[ch_idx];
                for (j = 0; j < BB_cl_tab_entry; j++) {
                    ichan->tx_clcal[ch_idx][j] = OS_REG_READ(ah, cl_tab_reg);
                    cl_tab_reg += 4;
                }
            }
            ichan->one_time_txclcal_done = true;
            HDPRINTF(ah, HAL_DBG_FCS_RTT,
                "(FCS) TX CL CAL saved - %d\n", ichan->channel);
        }
    }
#endif /* ATH_SUPPORT_CAL_REUSE */

#if ATH_SUPPORT_RADIO_RETENTION
    if (ahp->radio_retention_enable && enable_rtt )
    {
        if (is_cal_reusable) {
            HAL_RADIO_RETENTION *rtt;
            u_int32_t *table;
            int i;

            HDPRINTF(ah, HAL_DBG_FCS_RTT,
                "(RTT) %s: store CAL - chan = %d\n", __func__, ichan->channel);
            rtt = &ichan->rtt;
            if ((rtt->saved < AH_RTT_MAX_NUM_HIST) ||
                HAL_RTT_CTRL(ah, AH_RTT_OVERRIDE_OLD_HIST))
            {
                if (rtt->saved == 0) {
                    rtt->last = 0;
                } else {
                    rtt->last++;
                    if (rtt->last >= AH_RTT_MAX_NUM_HIST) {
                        rtt->last = 0;
                    }
                }
                if (rtt->saved < AH_RTT_MAX_NUM_HIST) {
                    rtt->saved++;
                }
                for(i=0; i<AH9300(ah)->radio_retention_chains; i++) {
                    table = &rtt->table[rtt->last][i][0];
                    ar9300_rtt_read_table(ah, i, table, AH_RTT_MAX_NUM_TABLE_ENTRY);
                }
            }
        }

        ar9300_rtt_disable(ah);

        HDPRINTF(ah, HAL_DBG_FCS_RTT, "(RTT) %s: disable RTT\n", __func__);
    }
#endif /* ATH_SUPPORT_RADIO_RETENTION */

    /* Revert chainmasks to their original values before NF cal */
    ar9300_init_chain_masks(ah, ahp->ah_rx_chainmask, ahp->ah_tx_chainmask);

#if !FIX_NOISE_FLOOR
    /*
     * Do NF calibration after DC offset and other CALs.
     * Per system engineers, noise floor value can sometimes be 20 dB
     * higher than normal value if DC offset and noise floor cal are
     * triggered at the same time.
     */
    OS_REG_WRITE(ah, AR_PHY_AGC_CONTROL,
        OS_REG_READ(ah, AR_PHY_AGC_CONTROL) | AR_PHY_AGC_CONTROL_NF);
#endif

    /* Initialize list pointers */
    ahp->ah_cal_list = ahp->ah_cal_list_last = ahp->ah_cal_list_curr = AH_NULL;

    /*
     * Enable IQ, ADC Gain, ADC DC Offset Cals
     */
    /* Setup all non-periodic, init time only calibrations */
    /* XXX: Init DC Offset not working yet */
#ifdef not_yet
    if (true == ar9300_is_cal_supp(ah, chan, ADC_DC_INIT_CAL)) {
        INIT_CAL(&ahp->ah_adc_dc_cal_init_data);
        INSERT_CAL(ahp, &ahp->ah_adc_dc_cal_init_data);
    }

    /* Initialize current pointer to first element in list */
    ahp->ah_cal_list_curr = ahp->ah_cal_list;

    if (ahp->ah_cal_list_curr) {
        if (ar9300_run_init_cals(ah, 0) == false) {
            return false;
        }
    }
#endif
    /* end - Init time calibrations */

    /* Do not do RX cal in case of offchan, or cal data already exists on same channel*/
    if (ahp->ah_skip_rx_iq_cal) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "Skip RX IQ Cal\n");
        return true;
    }

    /* If Cals are supported, add them to list via INIT/INSERT_CAL */
    if (true == ar9300_is_cal_supp(ah, chan, IQ_MISMATCH_CAL)) {
        INIT_CAL(&ahp->ah_iq_cal_data);
        INSERT_CAL(ahp, &ahp->ah_iq_cal_data);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: enabling IQ Calibration.\n", __func__);
    }
    if (true == ar9300_is_cal_supp(ah, chan, TEMP_COMP_CAL)) {
        INIT_CAL(&ahp->ah_temp_comp_cal_data);
        INSERT_CAL(ahp, &ahp->ah_temp_comp_cal_data);
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: enabling Temperature Compensation Calibration.\n", __func__);
    }

    /* Initialize current pointer to first element in list */
    ahp->ah_cal_list_curr = ahp->ah_cal_list;

    /* Reset state within current cal */
    if (ahp->ah_cal_list_curr) {
        ar9300_reset_calibration(ah, ahp->ah_cal_list_curr);
    }

    /* Mark all calibrations on this channel as being invalid */
    ichan->cal_valid = 0;

    return true;
}

static inline bool
ar9300_init_cal(struct ath_hal *ah, HAL_CHANNEL *chan, bool skip_if_none, bool apply_last_iqcorr)
{
#if ATH_SUPPORT_RADIO_RETENTION
    struct ath_hal_9300 *ahp = AH9300(ah);
#endif
    HAL_CHANNEL_INTERNAL *ichan = ath_hal_checkchannel(ah, chan);
    bool do_rtt_cal = true;
    bool enable_rtt = false;

    if (AH_NULL == ichan) {
        HDPRINTF(ah, HAL_DBG_CHANNEL,
                "%s: invalid channel %u/0x%x; no mapping\n",
                __func__, chan->channel, chan->channel_flags);
        return false;
    }

#if ATH_SUPPORT_FAST_CC
    HDPRINTF(ah, HAL_DBG_FCS_RTT,
        "(FCS) %s CAL\n", skip_if_none?"FCS":"FULL RESET");
#endif

#if ATH_SUPPORT_RADIO_RETENTION
    if (ahp->radio_retention_enable) {
        bool restore_status = false;

        restore_status = ar9300_restore_rtt_cals(ah, ichan);
        if (restore_status == true)
        {
            HDPRINTF(ah, HAL_DBG_FCS_RTT, "(RTT) restore success\n");
            enable_rtt = false;
            do_rtt_cal = false;
        }
        else
        {
            HDPRINTF(ah, HAL_DBG_FCS_RTT, "(RTT) restore failed\n");
            enable_rtt = true;
            do_rtt_cal = true;
        }
    }

    if (enable_rtt) {
        HDPRINTF(ah, HAL_DBG_FCS_RTT,
            "(RTT) %s: enable RTT - chan = %d\n", __func__, ichan->channel);
        ar9300_rtt_enable(ah);
        ar9300_rtt_set_mask(ah, 0x00); // 000000

        ar9300_rtt_clear_table(ah, 0);
        ar9300_rtt_clear_table(ah, 1);
    }
#endif

    return ar9300_init_cal_internal(ah, chan, ichan, enable_rtt, do_rtt_cal, skip_if_none, apply_last_iqcorr);
}

/* ar9300_reset_cal_valid
 * Entry point for upper layers to restart current cal.
 * Reset the calibration valid bit in channel.
 */
void
ar9300_reset_cal_valid(struct ath_hal *ah, HAL_CHANNEL *chan,
    bool *is_cal_done, u_int32_t cal_type)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_CHANNEL_INTERNAL *ichan = ath_hal_checkchannel(ah, chan);
    HAL_CAL_LIST *curr_cal = ahp->ah_cal_list_curr;

    *is_cal_done = true;

    if (curr_cal == AH_NULL) {
        return;
    }
    if (ichan == AH_NULL) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: invalid channel %u/0x%x; no mapping\n",
            __func__, chan->channel, chan->channel_flags);
        return;
    }

    if (!(cal_type & IQ_MISMATCH_CAL)) {
        *is_cal_done = false;
        return;
    }

    /* Expected that this calibration has run before, post-reset.
     * Current state should be done
     */
    if (curr_cal->cal_state != CAL_DONE) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: Calibration state incorrect, %d\n",
            __func__, curr_cal->cal_state);
        return;
    }

    /* Verify Cal is supported on this channel */
    if (ar9300_is_cal_supp(ah, chan, curr_cal->cal_data->cal_type) == false) {
        return;
    }

    HDPRINTF(ah, HAL_DBG_CALIBRATE,
        "%s: Resetting Cal %d state for channel %u/0x%x\n", __func__,
        curr_cal->cal_data->cal_type, chan->channel, chan->channel_flags);

    /* Disable cal validity in channel */
    ichan->cal_valid &= ~curr_cal->cal_data->cal_type;
    curr_cal->cal_state = CAL_WAITING;
    /* Indicate to upper layers that we need polling */
    *is_cal_done = false;
}

static inline void
ar9300_set_dma(struct ath_hal *ah)
{
    u_int32_t   regval;
    struct ath_hal_9300 *ahp = AH9300(ah);
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CAPABILITIES *pCap = &ahpriv->ah_caps;

#if 0
    /*
     * set AHB_MODE not to do cacheline prefetches
     */
    regval = OS_REG_READ(ah, AR_AHB_MODE);
    OS_REG_WRITE(ah, AR_AHB_MODE, regval | AR_AHB_PREFETCH_RD_EN);
#endif

    /*
     * let mac dma reads be in 128 byte chunks
     */
    regval = OS_REG_READ(ah, AR_TXCFG) & ~AR_TXCFG_DMASZ_MASK;
    OS_REG_WRITE(ah, AR_TXCFG, regval | AR_TXCFG_DMASZ_128B);

    /*
     * Restore TX Trigger Level to its pre-reset value.
     * The initial value depends on whether aggregation is enabled, and is
     * adjusted whenever underruns are detected.
     */
    /*
    OS_REG_RMW_FIELD(ah, AR_TXCFG, AR_FTRIG, AH_PRIVATE(ah)->ah_tx_trig_level);
     */
    /*
     * Osprey 1.0 bug (EV 61936). Don't change trigger level from .ini default.
     * Osprey 2.0 - hardware recommends using the default INI settings.
     */
#if 0
    OS_REG_RMW_FIELD(ah, AR_TXCFG, AR_FTRIG, 0x3f);
#endif
    /*
     * let mac dma writes be in 128 byte chunks
     */
    regval = OS_REG_READ(ah, AR_RXCFG) & ~AR_RXCFG_DMASZ_MASK;
    OS_REG_WRITE(ah, AR_RXCFG, regval | AR_RXCFG_DMASZ_128B);

    /*
     * Setup receive FIFO threshold to hold off TX activities
     */
    OS_REG_WRITE(ah, AR_RXFIFO_CFG, 0x200);

    /*
     * reduce the number of usable entries in PCU TXBUF to avoid
     * wrap around bugs. (bug 20428)
     */

    if (AR_SREV_WASP(ah) &&
        (AH_PRIVATE((ah))->ah_mac_rev > AR_SREV_REVISION_WASP_12)) {
        /* Wasp 1.3 fix for EV#85395 requires usable entries
         * to be set to 0x500
         */
        OS_REG_WRITE(ah, AR_PCU_TXBUF_CTRL, 0x500);
    } else {
        OS_REG_WRITE(ah, AR_PCU_TXBUF_CTRL, AR_PCU_TXBUF_CTRL_USABLE_SIZE);
    }

    /*
     * Enable HPQ for UAPSD
     */
    if (pCap->hal_hw_uapsd_trig == AH_TRUE) {
    /* Only enable this if HAL capabilities says it is OK */
        if (AH_PRIVATE(ah)->ah_opmode == HAL_M_HOSTAP) {
            OS_REG_WRITE(ah, AR_HP_Q_CONTROL,
                    AR_HPQ_ENABLE | AR_HPQ_UAPSD | AR_HPQ_UAPSD_TRIGGER_EN);
        }
    } else {
        /* use default value from ini file - which disable HPQ queue usage */
    }

    /*
     * set the transmit status ring
     */
    ar9300_reset_tx_status_ring(ah);

    /*
     * set rxbp threshold.  Must be non-zero for RX_EOL to occur.
     * For Osprey 2.0+, keep the original thresholds
     * otherwise performance is lost due to excessive RX EOL interrupts.
     */
    OS_REG_RMW_FIELD(ah, AR_RXBP_THRESH, AR_RXBP_THRESH_HP, 0x1);
    OS_REG_RMW_FIELD(ah, AR_RXBP_THRESH, AR_RXBP_THRESH_LP, 0x1);

    /*
     * set receive buffer size.
     */
    if (ahp->rx_buf_size) {
        OS_REG_WRITE(ah, AR_DATABUF, ahp->rx_buf_size);
    }
}

static inline void
ar9300_init_bb(struct ath_hal *ah, HAL_CHANNEL *chan)
{
    u_int32_t synth_delay;

    /*
     * Wait for the frequency synth to settle (synth goes on
     * via AR_PHY_ACTIVE_EN).  Read the phy active delay register.
     * Value is in 100ns increments.
     */
    synth_delay = OS_REG_READ(ah, AR_PHY_RX_DELAY) & AR_PHY_RX_DELAY_DELAY;
    if (IS_CHAN_CCK(chan)) {
        synth_delay = (4 * synth_delay) / 22;
    } else {
        synth_delay /= 10;
    }

    /* Activate the PHY (includes baseband activate + synthesizer on) */
    OS_REG_WRITE(ah, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);

    /*
     * There is an issue if the AP starts the calibration before
     * the base band timeout completes.  This could result in the
     * rx_clear false triggering.  As a workaround we add delay an
     * extra BASE_ACTIVATE_DELAY usecs to ensure this condition
     * does not happen.
     */
    OS_DELAY(synth_delay + BASE_ACTIVATE_DELAY);
}

static inline void
ar9300_init_interrupt_masks(struct ath_hal *ah, HAL_OPMODE opmode)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    u_int32_t msi_cfg = 0;
    u_int32_t sync_en_def = AR9300_INTR_SYNC_DEFAULT;

    /*
     * Setup interrupt handling.  Note that ar9300_reset_tx_queue
     * manipulates the secondary IMR's as queues are enabled
     * and disabled.  This is done with RMW ops to insure the
     * settings we make here are preserved.
     */
    ahp->ah_mask_reg =
        AR_IMR_TXERR | AR_IMR_TXURN |
        AR_IMR_RXERR | AR_IMR_RXORN |
        AR_IMR_BCNMISC;

    if (ahp->ah_intr_mitigation_rx) {
        /* enable interrupt mitigation for rx */
        ahp->ah_mask_reg |= AR_IMR_RXINTM | AR_IMR_RXMINTR | AR_IMR_RXOK_HP;
        msi_cfg |= AR_INTCFG_MSI_RXINTM | AR_INTCFG_MSI_RXMINTR;
    } else {
        ahp->ah_mask_reg |= AR_IMR_RXOK_LP | AR_IMR_RXOK_HP;
        msi_cfg |= AR_INTCFG_MSI_RXOK;
    }
    if (ahp->ah_intr_mitigation_tx) {
        /* enable interrupt mitigation for tx */
        ahp->ah_mask_reg |= AR_IMR_TXINTM | AR_IMR_TXMINTR;
        msi_cfg |= AR_INTCFG_MSI_TXINTM | AR_INTCFG_MSI_TXMINTR;
    } else {
        ahp->ah_mask_reg |= AR_IMR_TXOK;
        msi_cfg |= AR_INTCFG_MSI_TXOK;
    }
    if (opmode == HAL_M_HOSTAP) {
        ahp->ah_mask_reg |= AR_IMR_MIB;
    }

    OS_REG_WRITE(ah, AR_IMR, ahp->ah_mask_reg);
    OS_REG_WRITE(ah, AR_IMR_S2, OS_REG_READ(ah, AR_IMR_S2) | AR_IMR_S2_GTT);
    ahp->ah_mask2Reg = OS_REG_READ(ah, AR_IMR_S2);

    if (AH_PRIVATE(ah)->ah_config.ath_hal_enable_msi) {
        /* Cache MSI register value */
        ahp->ah_msi_reg = OS_REG_READ(ah, AR_HOSTIF_REG(ah, AR_PCIE_MSI));
        ahp->ah_msi_reg |= AR_PCIE_MSI_HW_DBI_WR_EN;
        if (AR_SREV_POSEIDON(ah)) {
            ahp->ah_msi_reg &= AR_PCIE_MSI_HW_INT_PENDING_ADDR_MSI_64;
        } else {
            ahp->ah_msi_reg &= AR_PCIE_MSI_HW_INT_PENDING_ADDR;
        }
        /* Program MSI configuration */
        OS_REG_WRITE(ah, AR_INTCFG, msi_cfg);
    }

    /*
     * debug - enable to see all synchronous interrupts status
     */
    /* Clear any pending sync cause interrupts */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_SYNC_CAUSE), 0xFFFFFFFF);

    /* Allow host interface sync interrupt sources to set cause bit */
    if (AR_SREV_POSEIDON(ah)) {
        sync_en_def = AR9300_INTR_SYNC_DEF_NO_HOST1_PERR;
    }
    else if (AR_SREV_WASP(ah)) {
        sync_en_def = AR9340_INTR_SYNC_DEFAULT;
    }
    OS_REG_WRITE(ah,
        AR_HOSTIF_REG(ah, AR_INTR_SYNC_ENABLE), sync_en_def);

    /* _Disable_ host interface sync interrupt when cause bits set */
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_SYNC_MASK), 0);

    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_PRIO_ASYNC_ENABLE), 0);
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_PRIO_ASYNC_MASK), 0);
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_PRIO_SYNC_ENABLE), 0);
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_INTR_PRIO_SYNC_MASK), 0);
}

static inline void
ar9300_init_qos(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_MIC_QOS_CONTROL, 0x100aa);  /* XXX magic */
    OS_REG_WRITE(ah, AR_MIC_QOS_SELECT, 0x3210);    /* XXX magic */

    /* Turn on NOACK Support for QoS packets */
    OS_REG_WRITE(ah, AR_QOS_NO_ACK,
        SM(2, AR_QOS_NO_ACK_TWO_BIT) |
        SM(5, AR_QOS_NO_ACK_BIT_OFF) |
        SM(0, AR_QOS_NO_ACK_BYTE_OFF));

    /*
     * initialize TXOP for all TIDs
     */
    OS_REG_WRITE(ah, AR_TXOP_X, AR_TXOP_X_VAL);
    OS_REG_WRITE(ah, AR_TXOP_0_3, 0xFFFFFFFF);
    OS_REG_WRITE(ah, AR_TXOP_4_7, 0xFFFFFFFF);
    OS_REG_WRITE(ah, AR_TXOP_8_11, 0xFFFFFFFF);
    OS_REG_WRITE(ah, AR_TXOP_12_15, 0xFFFFFFFF);
}

static inline void
ar9300_init_user_settings(struct ath_hal *ah)
{
    struct ath_hal_9300 *ahp = AH9300(ah);

    /* Restore user-specified settings */
    HDPRINTF(ah, HAL_DBG_RESET,
        "--AP %s ahp->ah_misc_mode 0x%x\n", __func__, ahp->ah_misc_mode);
    if (ahp->ah_misc_mode != 0) {
        OS_REG_WRITE(ah,
            AR_PCU_MISC, OS_REG_READ(ah, AR_PCU_MISC) | ahp->ah_misc_mode);
    }
    if (ahp->ah_get_plcp_hdr) {
        OS_REG_CLR_BIT(ah, AR_PCU_MISC, AR_PCU_SEL_EVM);
    }
    if (ahp->ah_slot_time != (u_int) -1) {
        ar9300_set_slot_time(ah, ahp->ah_slot_time);
    }
    if (ahp->ah_ack_timeout != (u_int) -1) {
        ar9300_set_ack_timeout(ah, ahp->ah_ack_timeout);
    }
    if (AH_PRIVATE(ah)->ah_diagreg != 0) {
        OS_REG_SET_BIT(ah, AR_DIAG_SW, AH_PRIVATE(ah)->ah_diagreg);
    }
    if (ahp->ah_beacon_rssi_threshold != 0) {
        ar9300_set_hw_beacon_rssi_threshold(ah, ahp->ah_beacon_rssi_threshold);
    }
#ifdef ATH_SUPPORT_DFS
    if (ahp->ah_cac_quiet_enabled) {
        ar9300_cac_tx_quiet(ah, 1);
    }
#endif /* ATH_SUPPORT_DFS */
}

int
ar9300_get_spur_info(struct ath_hal * ah, int *enable, int len, u_int16_t *freq)
{
    struct ath_hal_private *ap = AH_PRIVATE(ah);
    int i, j;

    for (i = 0; i < len; i++) {
        freq[i] =  0;
    }

    *enable = ap->ah_config.ath_hal_spur_mode;
    for (i = 0, j = 0; i < AR_EEPROM_MODAL_SPURS; i++) {
        if (ap->ah_config.ath_hal_spur_chans[i][0] != AR_NO_SPUR) {
            freq[j++] = ap->ah_config.ath_hal_spur_chans[i][0];
            HDPRINTF(ah, HAL_DBG_ANI,
                "1. get spur %d\n", ap->ah_config.ath_hal_spur_chans[i][0]);
        }
        if (ap->ah_config.ath_hal_spur_chans[i][1] != AR_NO_SPUR) {
            freq[j++] = ap->ah_config.ath_hal_spur_chans[i][1];
            HDPRINTF(ah, HAL_DBG_ANI,
                "2. get spur %d\n", ap->ah_config.ath_hal_spur_chans[i][1]);
        }
    }

    return 0;
}

#define ATH_HAL_2GHZ_FREQ_MIN   20000
#define ATH_HAL_2GHZ_FREQ_MAX   29999
#define ATH_HAL_5GHZ_FREQ_MIN   50000
#define ATH_HAL_5GHZ_FREQ_MAX   59999

int
ar9300_set_spur_info(struct ath_hal * ah, int enable, int len, u_int16_t *freq)
{
    struct ath_hal_private *ap = AH_PRIVATE(ah);
    int i, j, k;

    ap->ah_config.ath_hal_spur_mode = enable;

    if (ap->ah_config.ath_hal_spur_mode == SPUR_ENABLE_IOCTL) {
        for (i = 0; i < AR_EEPROM_MODAL_SPURS; i++) {
            ap->ah_config.ath_hal_spur_chans[i][0] = AR_NO_SPUR;
            ap->ah_config.ath_hal_spur_chans[i][1] = AR_NO_SPUR;
        }
        for (i = 0, j = 0, k = 0; i < len; i++) {
            if (freq[i] > ATH_HAL_2GHZ_FREQ_MIN &&
                freq[i] < ATH_HAL_2GHZ_FREQ_MAX)
            {
                /* 2GHz Spur */
                if (j < AR_EEPROM_MODAL_SPURS) {
                    ap->ah_config.ath_hal_spur_chans[j++][1] =  freq[i];
                    HDPRINTF(ah, HAL_DBG_ANI, "1 set spur %d\n", freq[i]);
                }
            } else if (freq[i] > ATH_HAL_5GHZ_FREQ_MIN &&
                       freq[i] < ATH_HAL_5GHZ_FREQ_MAX)
            {
                /* 5Ghz Spur */
                if (k < AR_EEPROM_MODAL_SPURS) {
                    ap->ah_config.ath_hal_spur_chans[k++][0] =  freq[i];
                    HDPRINTF(ah, HAL_DBG_ANI, "2 set spur %d\n", freq[i]);
                }
            }
        }
    }

    return 0;
}

#define ar9300_check_op_mode(_opmode) \
    ((_opmode == HAL_M_STA) || (_opmode == HAL_M_IBSS) ||\
     (_opmode == HAL_M_HOSTAP) || (_opmode == HAL_M_MONITOR))




#ifndef ATH_NF_PER_CHAN
/*
* To fixed first reset noise floor value not correct issue
* For ART need it to fixed low rate sens too low issue
*/
static int
First_NFCal(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichan,
	int is_scan, HAL_CHANNEL *chan, u_int8_t rxchainmask)
{
    HAL_NFCAL_HIST_FULL *nfh;
    int i, j, k;
    int16_t nfarray[NUM_NF_READINGS] = {0};
    int is_2g = 0;
    int nf_hist_len;
    int stats = 0;

    int16_t nf_buf[NUM_NF_READINGS];
    int16_t nf_max_good_val;
    struct ath_hal_9300 *ahp = AH9300(ah);
#define IS(_c, _f)       (((_c)->channel_flags & _f) || 0)


    if ((!is_scan) &&
        chan->channel == AH_PRIVATE(ah)->ah_curchan->channel)
    {
        nfh = &AH_PRIVATE(ah)->nf_cal_hist;
    } else {
        nfh = (HAL_NFCAL_HIST_FULL *) &ichan->nf_cal_hist;
    }

    ar9300_start_nf_cal(ah);
    for (j = 0; j < 10000; j++) {
        if ((OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0){
            break;
		}
        OS_DELAY(10);
    }
	if ((OS_REG_READ(ah, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) == 0) {
        is_2g = IS(ichan, CHANNEL_2GHZ);
        ar9300_upload_noise_floor(ah, is_2g, nfarray);

	    if (is_scan) {
			/*
			 * This channel's NF cal info is just a HAL_NFCAL_HIST_SMALL struct
			 * rather than a HAL_NFCAL_HIST_FULL struct.
			 * As long as we only use the first history element of nf_cal_buffer
			 * (nf_cal_buffer[0][0:NUM_NF_READINGS-1]), we can use
			 * HAL_NFCAL_HIST_SMALL and HAL_NFCAL_HIST_FULL interchangeably.
			 */
            nfh = (HAL_NFCAL_HIST_FULL *) &ichan->nf_cal_hist;
            nf_hist_len = HAL_NF_CAL_HIST_LEN_SMALL;
		} else {
            nfh = &AH_PRIVATE(ah)->nf_cal_hist;
            nf_hist_len = HAL_NF_CAL_HIST_LEN_FULL;
		}

  	    for (i = 0; i < NUM_NF_READINGS; i ++) {
    		for (k = 0; k < HAL_NF_CAL_HIST_LEN_FULL; k++) {
                nfh->nf_cal_buffer[k][i] = nfarray[i];
            }
            nfh->base.priv_nf[i] = ar9300_limit_nf_range(ah,
							ar9300_get_nf_hist_mid(ah, nfh, i, nf_hist_len));
  		}


		//ar9300StoreNewNf(ah, ichan, is_scan);

		/*
		 * See if the NF value from the old channel should be
		 * retained when switching to a new channel.
		 * TBD: this may need to be changed, as it wipes out the
		 * purpose of saving NF values for each channel.
		 */
		for (i = 0; i < NUM_NF_READINGS; i++)
		{
    		if (IS_CHAN_2GHZ(chan))
    		{
    			if (is_reg_dmn_fcc(ahp->reg_dmn)) {
                            nf_max_good_val = AR_PHY_CCA_MAX_GOOD_VAL_OSPREY_FCC_2GHZ;
                        } else {
                            nf_max_good_val = AR_PHY_CCA_MAX_GOOD_VAL_OSPREY_2GHZ;
                        }
    			if (nfh->nf_cal_buffer[0][i] <
					nf_max_good_val)
                {
                    ichan->nf_cal_hist.nf_cal_buffer[0][i] =
							AH_PRIVATE(ah)->nf_cal_hist.nf_cal_buffer[0][i];
				}
    		} else {
                if (AR_SREV_AR9580(ah)) {
                    if (nfh->nf_cal_buffer[0][i] <
                        AR_PHY_CCA_NOM_VAL_PEACOCK_5GHZ)
                    {
                       ichan->nf_cal_hist.nf_cal_buffer[0][i] =
                       AH_PRIVATE(ah)->nf_cal_hist.nf_cal_buffer[0][i];
                    }
                } else {
                   if (nfh->nf_cal_buffer[0][i] <
                       AR_PHY_CCA_NOM_VAL_OSPREY_5GHZ)
                    {
                        ichan->nf_cal_hist.nf_cal_buffer[0][i] =
                            AH_PRIVATE(ah)->nf_cal_hist.nf_cal_buffer[0][i];
                     }
                }
            }
        }
		/*
		 * Copy the channel's NF buffer, which may have been modified
		 * just above here, to the full NF history buffer.
		 */
        ar9300_reset_nf_hist_buff(ah, ichan);
        ar9300_get_nf_hist_base(ah,
                    AH_PRIVATE(ah)->ah_curchan, is_scan, nf_buf);
        if (!ar9300_load_nf(ah, nf_buf)) {
            HDPRINTF(ah, HAL_DBG_RESET, "%s: ar9300_load_nf Failed\n", __func__);
            return 1;
        }
         if (AH_PRIVATE(ah)->ah_config.ath_hal_enable_adaptiveCCAThres) {
            ar9300_update_cca_threshold(ah, nf_buf, rxchainmask);
        }
        stats = 0;
	} else {
        stats = 1;
	}
#undef IS
    return stats;
}
#endif

#ifdef JUPITER_EMULATION_WOW_OFFLOAD
void EnableOTPWrite(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_GLB_OTP_LDO_CONTROL, 1);
    if (!ath_hal_wait(ah, AR_GLB_OTP_LDO_STATUS, 1, 1,
                        10*AH_WAIT_TIMEOUT)) {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: Failed! status = 0x%x",
                __func__,OS_REG_READ(ah, AR_GLB_OTP_LDO_STATUS));
    }
    /* New to Jupiter for compensate double buffer wrt VDDQ */
    OS_REG_WRITE(ah, AR_OTP_EFUSE_PGENB_SETUP_HOLD_TIME, 7);
    OS_REG_WRITE(ah, AR_OTP_EFUSE_INTF0, 0x810ad079); //LoadOTP
}

void DisableOTPWrite(struct ath_hal *ah)
{
    OS_REG_WRITE(ah, AR_GLB_OTP_LDO_CONTROL,0);
    if (!ath_hal_wait(ah, AR_GLB_OTP_LDO_STATUS, 1, 0,
                10*AH_WAIT_TIMEOUT)){
        HDPRINTF(ah, HAL_DBG_UNMASKABLE, "%s: Failed! status = 0x%x",
                __func__,OS_REG_READ(ah, AR_GLB_OTP_LDO_STATUS));
    }
}

/*
 * Change PCIe config space to enable WoW for emulation. The first time device
 * comes up, this piece of code writes to the OTP. After resetting the FPGA
 * and plugging in the device the second time, the write to the OTP takes
 * effect and is reflected in the PCIe config space.
 *
 * This piece of code was taken directly from system DV.
 */
void ar9300_enable_wow_pci_config_space(struct ath_hal *ah)
{
    int i;
    u_int32_t programming_word_value[] = {
        /* address in otp, init data,    remark */
        0x00000000,     0x00000004, /* tell the hardware to look for init section */
        0x00000004,     0x00000000,
        0x00000008,     0x00000000,
        0x0000000c,     0x00000000,
        0x00000010,     0x00000000,
        0x00000014,     0x00000000,
        0x00000018,     0x00000000,
        0x0000001c,     0x00000000, /* end of static section */
        0x00000020,     0x00004014, /* chip register address */
        0x00000024,     0x3a000400, /* data */
        0x00000028,     0x00005040, /* PCIe config space */
        0x0000002c,     0xffc25001, /* data */
    };
    /* Above control program has to be burned into OTP using following method. */
    EnableOTPWrite(ah);
    OS_REG_WRITE(ah, AR_OTP_EFUSE_INTF5, 1);
    OS_REG_WRITE(ah, AR_OTP_EFUSE_INTF0, 0x10ad079); //LoadOTP
    for (i = 0; i <= (0x2c >> 1); i = i + 2) {
        OS_REG_WRITE(ah, AR_OTP_EFUSE_MEM + programming_word_value[i],
                programming_word_value[i+1]);
    }

    DisableOTPWrite(ah);
}
#endif /* JUPITER_EMULATION_WOW_OFFLOAD */

/*
 * Places the device in and out of reset and then places sane
 * values in the registers based on EEPROM config, initialization
 * vectors (as determined by the mode), and station configuration
 *
 * b_channel_change is used to preserve DMA/PCU registers across
 * a HW Reset during channel change.
 */
bool
ar9300_reset(struct ath_hal *ah, HAL_OPMODE opmode, HAL_CHANNEL *chan,
    HAL_HT_MACMODE macmode, u_int8_t txchainmask, u_int8_t rxchainmask,
    HAL_HT_EXTPROTSPACING extprotspacing, bool b_channel_change,
    HAL_STATUS *status, int is_scan)
{
#define FAIL(_code)     do { ecode = _code; goto bad; } while (0)
    u_int32_t               save_led_state;
    struct ath_hal_9300     *ahp = AH9300(ah);
    struct ath_hal_private  *ap  = AH_PRIVATE(ah);
    HAL_CHANNEL_INTERNAL    *ichan;
    HAL_CHANNEL_INTERNAL    *curchan = ap->ah_curchan;
#if ATH_SUPPORT_MCI
    bool                    save_full_sleep = ahp->ah_chip_full_sleep;
#endif
    u_int32_t               save_def_antenna;
    u_int32_t               mac_sta_id1;
    HAL_STATUS              ecode;
    int                     i, rx_chainmask;
    int                     nf_hist_buff_reset = 0;
    int16_t                 nf_buf[NUM_NF_READINGS];
#ifdef ATH_FORCE_PPM
    u_int32_t               save_force_val, tmp_reg;
#endif
    u_int8_t                clk_25mhz = AH9300(ah)->clk_25mhz;
    bool                    stopped, cal_ret;
#if ATH_SUPPORT_FAST_CC
    bool                    allow_fcs = false;
#endif
    bool                    apply_last_iqcorr = false;
    bool                    tx_iq_cal_disable = false;
    bool                    pk_detect_cal_disable = false;
    bool                    back_to_home_channel = false;

reset_begin:
    if (OS_REG_READ(ah, AR_IER) == AR_IER_ENABLE) {
        HDPRINTF(AH_NULL, HAL_DBG_UNMASKABLE, "** Reset called with WLAN "
                "interrupt enabled %08x **\n", ar9300_get_interrupts(ah));
    }

    /*
     * Set the status to "ok" by default to cover the cases
     * where we return false without going to "bad"
     */
    HALASSERT(status);
    *status = HAL_OK;
    if ((AH_PRIVATE(ah)->ah_config.ath_hal_sta_update_tx_pwr_enable)) {
        AH_PRIVATE(ah)->green_tx_status = HAL_RSSI_TX_POWER_NONE;
    }

#if ATH_SUPPORT_MCI
    if (AH_PRIVATE(ah)->ah_caps.hal_mci_support &&
        (AR_SREV_JUPITER_20(ah) || AR_SREV_APHRODITE(ah)))
    {
        ar9300_mci_2g5g_changed(ah, IS_CHAN_2GHZ(chan));
    }
#endif

    ahp->ah_ext_prot_spacing = extprotspacing;
    ahp->ah_tx_chainmask = txchainmask & ap->ah_caps.hal_tx_chain_mask;
    ahp->ah_rx_chainmask = rxchainmask & ap->ah_caps.hal_rx_chain_mask;
    ahp->ah_tx_cal_chainmask = ap->ah_caps.hal_tx_chain_mask;
    ahp->ah_rx_cal_chainmask = ap->ah_caps.hal_rx_chain_mask;

    /*
     * Keep the previous optinal txchainmask value
     */

#ifdef ATH_SUPPORT_TxBF
    ar9300_fill_txbf_capabilities(ah);
#endif
    HALASSERT(ar9300_check_op_mode(opmode));

    OS_MARK(ah, AH_MARK_RESET, b_channel_change);

    /*
     * Map public channel to private.
     */
    ichan = ar9300_check_chan(ah, chan);
    if (ichan == AH_NULL) {
        HDPRINTF(ah, HAL_DBG_CHANNEL,
            "%s: invalid channel %u/0x%x; no mapping\n",
            __func__, chan->channel, chan->channel_flags);
        FAIL(HAL_EINVAL);
    }

    ichan->paprd_table_write_done = 0;  /* Clear PAPRD table write flag */
    chan->paprd_table_write_done = 0;  /* Clear PAPRD table write flag */

    if (ar9300_get_power_mode(ah) != HAL_PM_FULL_SLEEP) {
        /* Need to stop RX DMA before reset otherwise chip might hang */
        stopped = ar9300_set_rx_abort(ah, true); /* abort and disable PCU */
        ar9300_set_rx_filter(ah, 0);
        stopped &= ar9300_stop_dma_receive(ah, 0); /* stop and disable RX DMA */
        if (!stopped) {
            /*
             * During the transition from full sleep to reset,
             * recv DMA regs are not available to be read
             */
            HDPRINTF(ah, HAL_DBG_UNMASKABLE,
                "%s[%d]: ar9300_stop_dma_receive failed\n", __func__, __LINE__);
            b_channel_change = false;
        }
    } else {
        HDPRINTF(ah, HAL_DBG_UNMASKABLE,
            "%s[%d]: Chip is already in full sleep\n", __func__, __LINE__);
    }

#if ATH_SUPPORT_MCI
    if ((AH_PRIVATE(ah)->ah_caps.hal_mci_support) &&
        (ahp->ah_mci_bt_state == MCI_BT_CAL_START))
    {
        u_int32_t payload[4] = {0, 0, 0, 0};

        HDPRINTF(ah, HAL_DBG_BT_COEX,
            "(MCI) %s: Stop rx for BT cal.\n", __func__);
        ahp->ah_mci_bt_state = MCI_BT_CAL;

        /*
         * MCIFIX: disable mci interrupt here. This is to avoid SW_MSG_DONE or
         * RX_MSG bits to trigger MCI_INT and lead to mci_intr reentry.
         */
        ar9300_mci_disable_interrupt(ah);

        HDPRINTF(ah, HAL_DBG_BT_COEX,
            "(MCI) %s: Send WLAN_CAL_GRANT\n", __func__);
        MCI_GPM_SET_CAL_TYPE(payload, MCI_GPM_WLAN_CAL_GRANT);
        ar9300_mci_send_message(ah, MCI_GPM, 0, payload, 16, true, false);

        /* Wait BT calibration to be completed for 25ms */
        HDPRINTF(ah, HAL_DBG_BT_COEX,
            "(MCI) %s: BT is calibrating.\n", __func__);
        if (ar9300_mci_wait_for_gpm(ah, MCI_GPM_BT_CAL_DONE, 0, 25000)) {
            HDPRINTF(ah, HAL_DBG_BT_COEX,
                "(MCI) %s: Got BT_CAL_DONE.\n", __func__);
        }
        else {
            HDPRINTF(ah, HAL_DBG_BT_COEX,
                "(MCI) %s: ### BT cal takes too long. Force bt_state to be bt_awake.\n",
                __func__);
        }
        ahp->ah_mci_bt_state = MCI_BT_AWAKE;
        /* MCIFIX: enable mci interrupt here */
        ar9300_mci_enable_interrupt(ah);

        return true;
    }
#endif

    /* Bring out of sleep mode */
    if (!ar9300_set_power_mode(ah, HAL_PM_AWAKE, true)) {
        *status = HAL_INV_PMODE;
        return false;
    }

    /* Check the Rx mitigation config again, it might have changed
     * during attach in ath_vap_attach.
     */
    if (AH_PRIVATE(ah)->ah_config.ath_hal_intr_mitigation_rx != 0) {
        ahp->ah_intr_mitigation_rx = true;
    } else {
        ahp->ah_intr_mitigation_rx = false;
    }

    /* Get the value from the previous NF cal and update history buffer */
    if (curchan && (ahp->ah_chip_full_sleep != true)) {

        if(ahp->ah_chip_reset_done){
            ahp->ah_chip_reset_done = 0;
        } else {
        	/*
         	 * is_scan controls updating NF for home channel or off channel.
         	 * Home -> Off, update home channel
         	 * Off -> Home, update off channel
         	 * Home -> Home, uppdate home channel
         	 */
                if ((ahp->ah_scanning == 0) && (is_scan)) {
                    ahp->ah_scanning = 1;
                    ahp->ah_home_channel = ap->ah_curchan->channel;
                    ahp->ah_home_channel_flags = ap->ah_curchan->channel_flags;
                    ar9300_store_new_nf(ah, curchan, 0);
                } else if ((ahp->ah_scanning == 1) && (!is_scan)) {
                    ahp->ah_scanning = 0;
                    if ((ahp->ah_home_channel == chan->channel) &&
                        (ahp->ah_home_channel_flags == chan->channel_flags)) {
                        back_to_home_channel = true;
                    }
                    ar9300_store_new_nf(ah, curchan, 1);
                } else {
                    ar9300_store_new_nf(ah, curchan, is_scan);
                }
        }
    }

    /*
     * Account for the effect of being in either the 2 GHz or 5 GHz band
     * on the nominal, max allowable, and min allowable noise floor values.
     */
    ap->nfp = IS_CHAN_2GHZ(chan) ? &ap->nf_2GHz : &ap->nf_5GHz;

    if ((AR_SREV_SCORPION(ah) || AR_SREV_HONEYBEE(ah)) && curchan && (chan->channel == curchan->channel) &&
        ((chan->channel_flags & (CHANNEL_ALL|CHANNEL_HALF|CHANNEL_QUARTER)) ==
         (curchan->channel_flags &
          (CHANNEL_ALL | CHANNEL_HALF | CHANNEL_QUARTER)))) {
            apply_last_iqcorr = true;
    }

#ifndef ATH_NF_PER_CHAN
    /*
     * If there's only one full-size home-channel NF history buffer
     * rather than a full-size NF history buffer per channel, decide
     * whether to (re)initialize the home-channel NF buffer.
     * If this is just a channel change for a scan, or if the channel
     * is not being changed, don't mess up the home channel NF history
     * buffer with NF values from this scanned channel.  If we're
     * changing the home channel to a new channel, reset the home-channel
     * NF history buffer with the most accurate NF known for the new channel.
     */
    if (!is_scan &&
        (back_to_home_channel == false) &&
        (!ap->ah_curchan ||
        ap->ah_curchan->channel != chan->channel ||
        ap->ah_curchan->channel_flags != chan->channel_flags))
    {
        nf_hist_buff_reset = 1;
        ar9300_reset_nf_hist_buff(ah, ichan);
    }
#endif
    /*
     * In case of
     * - offchan scan, or
     * - same channel and RX IQ Cal already available
     * disable RX IQ Cal.
     */
    if (is_scan) {
        ahp->ah_skip_rx_iq_cal = true;
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "Skip RX IQ Cal due to scanning\n");
    } else {
        if (ahp->ah_rx_cal_complete &&
            ahp->ah_rx_cal_chan == chan->channel &&
            ahp->ah_rx_cal_chan_flag == chan->channel_flags) {
            ahp->ah_skip_rx_iq_cal = true;
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                    "Skip RX IQ Cal due to same channel with completed RX IQ Cal\n");
        } else
            ahp->ah_skip_rx_iq_cal = false;
    }

#if ATH_SUPPORT_FAST_CC
    /*
     * In order to make cross band fast channel switch meaningful, we require
     * some or all calibrations can be reused in order to allow FCS. Same-band
     * same-mode channel change will be allowed same as previously.
     *
     * Test shows that if no calibration enabled, triggering BB_agc_control.
     * do_calibrate in FCS sometimes cause calibration timeout. The fix is to
     * apply all calibrations but skip triggerring do_calibrate.
     *
     * 'FCS_flag' 'IQ/CL_reuseable' 'RTT_saved' allow_fcs
     * --------------------------------------------------
     *      0             x              x        No
     *      1             0              x        No
     *      1             1              0        No  (Non-Jupiter)*
     *      1             1              0        No  (Jupiter)
     *      1             1              1        Yes (Jupiter)
     */
    if (ap->ah_config.ath_hal_fast_channel_change) {
#if ATH_SUPPORT_CAL_REUSE
        if ((AH_PRIVATE(ah)->ah_config.ath_hal_cal_reuse & ATH_CAL_REUSE_ENABLE) &&
            ichan->one_time_txiqcal_done && ichan->one_time_txclcal_done)
        {
#if ATH_SUPPORT_RADIO_RETENTION
            if (ahp->radio_retention_enable) {
                if (ichan->rtt.saved > 0) {
                    allow_fcs = true;
                }
            }
#endif /* ATH_SUPPORT_RADIO_RETENTION */
        }
#endif /* ATH_SUPPORT_CAL_REUSE */
    }
#endif /* ATH_SUPPORT_FAST_CC */

       /* reset the counters */
       AH9300(ah)->ah_cycle_count = 0;
       AH9300(ah)->ah_ctl_busy = 0;
       AH9300(ah)->ah_ext_busy = 0;
       AH9300(ah)->ah_rf = 0;
       AH9300(ah)->ah_tf = 0;
       OS_REG_WRITE(ah, AR_RCCNT, 0);
       OS_REG_WRITE(ah, AR_EXTRCCNT, 0);
       OS_REG_WRITE(ah, AR_CCCNT, 0);
       OS_REG_WRITE(ah, AR_RFCNT, 0);
       OS_REG_WRITE(ah, AR_TFCNT, 0);


    /*
     * Fast channel change (Change synthesizer based on channel freq
     * without resetting chip)
     * Don't do it when
     *   - Flag is not set
     *   - Chip is just coming out of full sleep
     *   - Channel to be set is same as current channel
     *   - Channel flags are different, like when moving from 2GHz to 5GHz
     *     channels
     *   - Merlin: Switching in/out of fast clock enabled channels
     *             (not currently coded, since fast clock is enabled
     *             across the 5GHz band
     *             and we already do a full reset when switching in/out
     *             of 5GHz channels)
     */
    if (b_channel_change &&
        (ahp->ah_chip_full_sleep != true) &&
        (AH_PRIVATE(ah)->ah_curchan != AH_NULL) &&
#if ATH_SUPPORT_FAST_CC
        (allow_fcs ||
#endif
        ((chan->channel != AH_PRIVATE(ah)->ah_curchan->channel) &&
        (((CHANNEL_ALL|CHANNEL_HALF|CHANNEL_QUARTER) & chan->channel_flags) ==
        ((CHANNEL_ALL|CHANNEL_HALF|CHANNEL_QUARTER) & AH_PRIVATE(ah)->ah_curchan->channel_flags))))
#if ATH_SUPPORT_FAST_CC
        )
#endif
    {
        if (ar9300_channel_change(ah, chan, ichan, macmode)) {
            chan->channel_flags = ichan->channel_flags;
            chan->priv_flags = ichan->priv_flags;
            AH_PRIVATE(ah)->ah_curchan->ah_channel_time = 0;
            AH_PRIVATE(ah)->ah_curchan->ah_tsf_last = ar9300_get_tsf64(ah);

            /*
             * Load the NF from history buffer of the current channel.
             * NF is slow time-variant, so it is OK to use a historical value.
             */
            ar9300_get_nf_hist_base(ah,
                AH_PRIVATE(ah)->ah_curchan, is_scan, nf_buf);
            if (!ar9300_load_nf(ah, nf_buf)) {
                HDPRINTF(ah, HAL_DBG_RESET, "%s Line %d: ar9300_load_nf Failed\n", __func__, __LINE__);
                FAIL(HAL_CAL_TEST);
            }
             if (AH_PRIVATE(ah)->ah_config.ath_hal_enable_adaptiveCCAThres) {
                ar9300_update_cca_threshold(ah, nf_buf, rxchainmask);
            }
            /* start NF calibration, without updating BB NF register*/
            ar9300_start_nf_cal(ah);

            /*
             * If channel_change completed and DMA was stopped
             * successfully - skip the rest of reset
             */
            if (AH9300(ah)->ah_dma_stuck != true) {
                WAR_USB_DISABLE_PLL_LOCK_DETECT(ah);
#if ATH_SUPPORT_MCI
                if (AH_PRIVATE(ah)->ah_caps.hal_mci_support && ahp->ah_mci_ready)
                {
                    ar9300_mci_2g5g_switch(ah, true);
                }
#endif
                return true;
            }
         }
    }

#if ATH_SUPPORT_MCI
    if (AH_PRIVATE(ah)->ah_caps.hal_mci_support) {
        ar9300_mci_disable_interrupt(ah);
        if (ahp->ah_mci_ready && !save_full_sleep) {
            ar9300_mci_mute_bt(ah);
            OS_DELAY(20);
            OS_REG_WRITE(ah, AR_BTCOEX_CTRL, 0);
        }

        ahp->ah_mci_bt_state = MCI_BT_SLEEP;
        ahp->ah_mci_ready = false;
    }
#endif

    AH9300(ah)->ah_dma_stuck = false;
#ifdef ATH_FORCE_PPM
    /* Preserve force ppm state */
    save_force_val =
        OS_REG_READ(ah, AR_PHY_TIMING2) &
        (AR_PHY_TIMING2_USE_FORCE | AR_PHY_TIMING2_FORCE_VAL);
#endif
    /*
     * Preserve the antenna on a channel change
     */
    save_def_antenna = OS_REG_READ(ah, AR_DEF_ANTENNA);
    if (0 == ahp->ah_smartantenna_enable )
    {
        if (save_def_antenna == 0) {
            save_def_antenna = 1;
        }
    }

    /* Save hardware flag before chip reset clears the register */
    mac_sta_id1 = OS_REG_READ(ah, AR_STA_ID1) & AR_STA_ID1_BASE_RATE_11B;

    /* Save led state from pci config register */
    save_led_state = OS_REG_READ(ah, AR_CFG_LED) &
        (AR_CFG_LED_ASSOC_CTL | AR_CFG_LED_MODE_SEL |
        AR_CFG_LED_BLINK_THRESH_SEL | AR_CFG_LED_BLINK_SLOW);

    /* Mark PHY inactive prior to reset, to be undone in ar9300_init_bb () */
    ar9300_mark_phy_inactive(ah);

    if (!ar9300_chip_reset(ah, chan)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: chip reset failed\n", __func__);
        FAIL(HAL_EIO);
    }

    OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);


    /* Disable JTAG */
    OS_REG_SET_BIT(ah,
        AR_HOSTIF_REG(ah, AR_GPIO_INPUT_EN_VAL), AR_GPIO_JTAG_DISABLE);

    /*
     * Note that ar9300_init_chain_masks() is called from within
     * ar9300_process_ini() to ensure the swap bit is set before
     * the pdadc table is written.
     */
    ecode = ar9300_process_ini(ah, chan, ichan, macmode);
    if (ecode != HAL_OK) {
        goto bad;
    }

    /*
     * Configuring WMAC PLL values for 25/40 MHz
     */
    if(AR_SREV_WASP(ah) || AR_SREV_HONEYBEE(ah) || AR_SREV_SCORPION(ah) || AR_SREV_DRAGONFLY(ah)) {
        if(clk_25mhz) {
            OS_REG_WRITE(ah, AR_RTC_DERIVED_RTC_CLK, (0x17c << 1)); // 32KHz sleep clk
        } else {
            OS_REG_WRITE(ah, AR_RTC_DERIVED_RTC_CLK, (0x261 << 1)); // 32KHz sleep clk
        }
        OS_DELAY(100);
    }

    ahp->ah_immunity_on = false;
	/* WAR:
		When noise floor calibration is stuck,
		try to disable txiq calibration and redo the whole reset function again.
	*/
    if ((tx_iq_cal_disable) && (AR_SREV_DRAGONFLY(ah))) {
        OS_REG_CLR_BIT(ah, AR_PHY_TX_IQCAL_CONTROL_0(ah),
                AR_PHY_TX_IQCAL_CONTROL_0_ENABLE_TXIQ_CAL);
    } else if ((pk_detect_cal_disable) && (AR_SREV_SCORPION(ah) || AR_SREV_HONEYBEE(ah) || AR_SREV_WASP(ah))) {
        OS_REG_CLR_BIT(ah,AR_PHY_PEAK_DET_CTRL_1,AR_PHY_PEAK_DET_ENABLE);
        OS_REG_CLR_BIT(ah,AR_PHY_AGC_CONTROL,AR_PHY_AGC_CONTROL_PKDET_CAL);
    }
    if (AR_SREV_JUPITER(ah) || AR_SREV_APHRODITE(ah)) {
        ahp->tx_iq_cal_enable = OS_REG_READ_FIELD(ah,
                                AR_PHY_TX_IQCAL_CONTROL_0(ah),
                                AR_PHY_TX_IQCAL_CONTROL_0_ENABLE_TXIQ_CAL) ?
                                1 : 0;
    }
    ahp->tx_cl_cal_enable = (OS_REG_READ(ah, AR_PHY_CL_CAL_CTL) &
                                AR_PHY_CL_CAL_ENABLE) ? 1 : 0;

    /* For devices with full HW RIFS Rx support (Sowl/Howl/Merlin, etc),
     * restore register settings from prior to reset.
     */
    if ((AH_PRIVATE(ah)->ah_curchan != AH_NULL) &&
        (ar9300_get_capability(ah, HAL_CAP_LDPCWAR, 0, AH_NULL) == HAL_OK))
    {
        /* Re-program RIFS Rx policy after reset */
        ar9300_set_rifs_delay(ah, ahp->ah_rifs_enabled);
    }

#if ATH_SUPPORT_MCI
    if (AH_PRIVATE(ah)->ah_caps.hal_mci_support) {
        ar9300_mci_reset(ah, false, IS_CHAN_2GHZ(chan), save_full_sleep);
    }
#endif

    /* Initialize Management Frame Protection */
    ar9300_init_mfp(ah);

    ahp->ah_immunity_vals[0] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR_LOW,
        AR_PHY_SFCORR_LOW_M1_THRESH_LOW);
    ahp->ah_immunity_vals[1] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR_LOW,
        AR_PHY_SFCORR_LOW_M2_THRESH_LOW);
    ahp->ah_immunity_vals[2] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR,
        AR_PHY_SFCORR_M1_THRESH);
    ahp->ah_immunity_vals[3] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR,
        AR_PHY_SFCORR_M2_THRESH);
    ahp->ah_immunity_vals[4] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR,
        AR_PHY_SFCORR_M2COUNT_THR);
    ahp->ah_immunity_vals[5] = OS_REG_READ_FIELD(ah, AR_PHY_SFCORR_LOW,
        AR_PHY_SFCORR_LOW_M2COUNT_THR_LOW);

    /* Write delta slope for OFDM enabled modes (A, G, Turbo) */
    if (IS_CHAN_OFDM(chan) || IS_CHAN_HT(chan)) {
        ar9300_set_delta_slope(ah, ichan);
    }

#ifndef AR9530_EMULATION
    ar9300_spur_mitigate(ah, chan);
    if (!ar9300_eeprom_set_board_values(ah, ichan)) {
        HDPRINTF(ah, HAL_DBG_EEPROM,
            "%s: error setting board options\n", __func__);
        FAIL(HAL_EIO);
    }
#endif

#ifdef ATH_HAL_WAR_REG16284_APH128
    /* temp work around, will be removed. */
    if (AR_SREV_WASP(ah)) {
        OS_REG_WRITE(ah, 0x16284, 0x1553e000);
    }
#endif

    OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);

    OS_REG_WRITE(ah, AR_STA_ID0, LE_READ_4(ahp->ah_macaddr));
    OS_REG_WRITE(ah, AR_STA_ID1, LE_READ_2(ahp->ah_macaddr + 4)
            | mac_sta_id1
            | AR_STA_ID1_RTS_USE_DEF
            | (ap->ah_config.ath_hal_6mb_ack ? AR_STA_ID1_ACKCTS_6MB : 0)
            | ahp->ah_sta_id1_defaults
    );
    ar9300_set_operating_mode(ah, opmode);

    /* Set Venice BSSID mask according to current state */
    OS_REG_WRITE(ah, AR_BSSMSKL, LE_READ_4(ahp->ah_bssid_mask));
    OS_REG_WRITE(ah, AR_BSSMSKU, LE_READ_2(ahp->ah_bssid_mask + 4));

    /* Restore previous antenna */
    OS_REG_WRITE(ah, AR_DEF_ANTENNA, save_def_antenna);
#ifdef ATH_FORCE_PPM
    /* Restore force ppm state */
    tmp_reg = OS_REG_READ(ah, AR_PHY_TIMING2) &
        ~(AR_PHY_TIMING2_USE_FORCE | AR_PHY_TIMING2_FORCE_VAL);
    OS_REG_WRITE(ah, AR_PHY_TIMING2, tmp_reg | save_force_val);
#endif

    /* then our BSSID and assocID */
    OS_REG_WRITE(ah, AR_BSS_ID0, LE_READ_4(ahp->ah_bssid));
    OS_REG_WRITE(ah, AR_BSS_ID1,
        LE_READ_2(ahp->ah_bssid + 4) |
        ((ahp->ah_assoc_id & 0x3fff) << AR_BSS_ID1_AID_S));

    OS_REG_WRITE(ah, AR_ISR, ~0); /* cleared on write */

    OS_REG_RMW_FIELD(ah, AR_RSSI_THR, AR_RSSI_THR_BM_THR, INIT_RSSI_THR);

    /* HW beacon processing */
    OS_REG_RMW_FIELD(ah, AR_RSSI_THR, AR_RSSI_BCN_WEIGHT,
            INIT_RSSI_BEACON_WEIGHT);
    OS_REG_SET_BIT(ah, AR_HWBCNPROC1, AR_HWBCNPROC1_CRC_ENABLE |
            AR_HWBCNPROC1_EXCLUDE_TIM_ELM);
    if (AH_PRIVATE(ah)->ah_config.ath_hal_beacon_filter_interval) {
        OS_REG_RMW_FIELD(ah, AR_HWBCNPROC2, AR_HWBCNPROC2_FILTER_INTERVAL,
                AH_PRIVATE(ah)->ah_config.ath_hal_beacon_filter_interval);
        OS_REG_SET_BIT(ah, AR_HWBCNPROC2,
                AR_HWBCNPROC2_FILTER_INTERVAL_ENABLE);
    }


    /*
     * Set Channel now modifies bank 6 parameters for FOWL workaround
     * to force rf_pwd_icsyndiv bias current as function of synth
     * frequency.Thus must be called after ar9300_process_ini() to ensure
     * analog register cache is valid.
     */
    if (!ahp->ah_rf_hal.set_channel(ah, ichan)) {
        FAIL(HAL_EIO);
    }


    OS_MARK(ah, AH_MARK_RESET_LINE, __LINE__);

    /* Set 1:1 QCU to DCU mapping for all queues */
    for (i = 0; i < AR_NUM_DCU; i++) {
        OS_REG_WRITE(ah, AR_DQCUMASK(i), 1 << i);
    }

    ahp->ah_intr_txqs = 0;
    for (i = 0; i < AH_PRIVATE(ah)->ah_caps.hal_total_queues; i++) {
        ar9300_reset_tx_queue(ah, i);
    }

    ar9300_init_interrupt_masks(ah, opmode);

    /* Reset ier reference count to disabled */
#ifdef MDK_AP
    ahp->ah_ier_ref_count = 1;
#else
    OS_ATOMIC_SET(&ahp->ah_ier_ref_count, 1);
#endif
    if (ath_hal_isrfkillenabled(ah)) {
        ar9300_enable_rf_kill(ah);
    }

    /* must be called AFTER ini is processed */
    ar9300_ani_init_defaults(ah, macmode);

    ar9300_init_qos(ah);

    ar9300_init_user_settings(ah);

#if ATH_SUPPORT_WAPI
    /*
     * Enable WAPI deaggregation and AR_PCU_MISC_MODE2_BC_MC_WAPI_MODE
     */
    OS_REG_SET_BIT(ah,
        AR_MAC_PCU_LOGIC_ANALYZER, AR_MAC_PCU_LOGIC_WAPI_DEAGGR_ENABLE);
    if (AH_PRIVATE(ah)->ah_hal_keytype == HAL_CIPHER_WAPI) {
        OS_REG_SET_BIT(ah,
            AR_PCU_MISC_MODE2, AR_PCU_MISC_MODE2_BC_MC_WAPI_MODE);
    }
#endif

    AH_PRIVATE(ah)->ah_opmode = opmode; /* record operating mode */

    OS_MARK(ah, AH_MARK_RESET_DONE, 0);

    /*
     * disable seq number generation in hw
     */
    OS_REG_WRITE(ah, AR_STA_ID1,
        OS_REG_READ(ah, AR_STA_ID1) | AR_STA_ID1_PRESERVE_SEQNUM);

    ar9300_set_dma(ah);

    /*
     * program OBS bus to see MAC interrupts
     */
#if ATH_SUPPORT_MCI
    if (!AH_PRIVATE(ah)->ah_caps.hal_mci_support) {
        OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_OBS), 8);
    }
#else
    OS_REG_WRITE(ah, AR_HOSTIF_REG(ah, AR_OBS), 8);
#endif


    /* enabling AR_GTTM_IGNORE_IDLE in GTTM register so that
       GTT timer will not increment if the channel idle indicates
       the air is busy or NAV is still counting down */
    OS_REG_WRITE(ah, AR_GTTM, AR_GTTM_IGNORE_IDLE);

    /*
     * GTT debug mode setting
     */
    /*
    OS_REG_WRITE(ah, 0x64, 0x00320000);
    OS_REG_WRITE(ah, 0x68, 7);
    OS_REG_WRITE(ah, 0x4080, 0xC);
     */
    /*
     * Disable general interrupt mitigation by setting MIRT = 0x0
     * Rx and tx interrupt mitigation are conditionally enabled below.
     */
    OS_REG_WRITE(ah, AR_MIRT, 0);
    if (ahp->ah_intr_mitigation_rx) {
        /*
         * Enable Interrupt Mitigation for Rx.
         * If no build-specific limits for the rx interrupt mitigation
         * timer have been specified, use conservative defaults.
         */
        #ifndef AH_RIMT_VAL_LAST
            #define AH_RIMT_LAST_MICROSEC 500
        #endif
        #ifndef AH_RIMT_VAL_FIRST
            #define AH_RIMT_FIRST_MICROSEC 2000
        #endif
#ifndef HOST_OFFLOAD
        OS_REG_RMW_FIELD(ah, AR_RIMT, AR_RIMT_LAST, AH_RIMT_LAST_MICROSEC);
        OS_REG_RMW_FIELD(ah, AR_RIMT, AR_RIMT_FIRST, AH_RIMT_FIRST_MICROSEC);
#else
        /*
         * EV [124660] Chip::Scorpion
         * [PVT-CHN][WDS/NAWDS Perf]Throughput numbers obtained in FO mode is less when compared to DA
         */
        if(ap->ah_config.ath_hal_lower_rx_mitigation) {

            /* lower mitigation level to reduce latency for offload arch. */
            OS_REG_RMW_FIELD(ah, AR_RIMT, AR_RIMT_LAST,
                    (AH_RIMT_LAST_MICROSEC >> 2));
            OS_REG_RMW_FIELD(ah, AR_RIMT, AR_RIMT_FIRST,
                    (AH_RIMT_FIRST_MICROSEC >> 2));
        }
        else {
            OS_REG_RMW_FIELD(ah, AR_RIMT, AR_RIMT_LAST, AH_RIMT_LAST_MICROSEC);
            OS_REG_RMW_FIELD(ah, AR_RIMT, AR_RIMT_FIRST, AH_RIMT_FIRST_MICROSEC);
        }
#endif
    }

    if (ahp->ah_intr_mitigation_tx) {
        /*
         * Enable Interrupt Mitigation for Tx.
         * If no build-specific limits for the tx interrupt mitigation
         * timer have been specified, use the values preferred for
         * the carrier group's products.
         */
        #ifndef AH_TIMT_LAST
            #define AH_TIMT_LAST_MICROSEC 300
        #endif
        #ifndef AH_TIMT_FIRST
            #define AH_TIMT_FIRST_MICROSEC 750
        #endif
        OS_REG_RMW_FIELD(ah, AR_TIMT, AR_TIMT_LAST, AH_TIMT_LAST_MICROSEC);
        OS_REG_RMW_FIELD(ah, AR_TIMT, AR_TIMT_FIRST, AH_TIMT_FIRST_MICROSEC);
    }

    rx_chainmask = ahp->ah_rx_chainmask;

    OS_REG_WRITE(ah, AR_PHY_RX_CHAINMASK, rx_chainmask);
    OS_REG_WRITE(ah, AR_PHY_CAL_CHAINMASK, rx_chainmask);

    ar9300_init_bb(ah, chan);

    /* BB Step 7: Calibration */
    /*
     * Only kick off calibration not on offchan.
     * If coming back from offchan, restore prevous Cal results
     * since chip reset will clear existings.
     */
    if (!ahp->ah_skip_rx_iq_cal) {
        int i;
        /* clear existing RX cal data */
        for (i=0; i<AR9300_MAX_CHAINS; i++)
            ahp->ah_rx_cal_corr[i] = 0;

        ahp->ah_rx_cal_complete = false;
        ahp->ah_rx_cal_chan = chan->channel;
        ahp->ah_rx_cal_chan_flag = chan->channel_flags;
    }
    ar9300_invalidate_saved_cals(ah, ichan);
    cal_ret = ar9300_init_cal(ah, chan, false, apply_last_iqcorr);

#if ATH_SUPPORT_MCI
    if (AH_PRIVATE(ah)->ah_caps.hal_mci_support && ahp->ah_mci_ready) {
        if (IS_CHAN_2GHZ(chan) &&
            (ahp->ah_mci_bt_state == MCI_BT_SLEEP))
        {
            if (ar9300_mci_check_int(ah, AR_MCI_INTERRUPT_RX_MSG_REMOTE_RESET) ||
                ar9300_mci_check_int(ah, AR_MCI_INTERRUPT_RX_MSG_REQ_WAKE))
            {
                /*
                 * BT is sleeping. Check if BT wakes up duing WLAN
                 * calibration. If BT wakes up during WLAN calibration, need
                 * to go through all message exchanges again and recal.
                 */
                HDPRINTF(ah, HAL_DBG_BT_COEX,
                    "(MCI) ### %s: BT wakes up during WLAN calibration.\n",
                    __func__);
                OS_REG_WRITE(ah, AR_MCI_INTERRUPT_RX_MSG_RAW,
                        AR_MCI_INTERRUPT_RX_MSG_REMOTE_RESET |
                        AR_MCI_INTERRUPT_RX_MSG_REQ_WAKE);
                HDPRINTF(ah, HAL_DBG_BT_COEX, "(MCI) send REMOTE_RESET\n");
                ar9300_mci_remote_reset(ah, true);
                ar9300_mci_send_sys_waking(ah, true);
                OS_DELAY(1);
                if (IS_CHAN_2GHZ(chan)) {
                    ar9300_mci_send_lna_transfer(ah, true);
                }
                ahp->ah_mci_bt_state = MCI_BT_AWAKE;

                /* Redo calibration */
                HDPRINTF(ah, HAL_DBG_BT_COEX, "(MCI) %s: Re-calibrate.\n",
                    __func__);
                ar9300_invalidate_saved_cals(ah, ichan);
                cal_ret = ar9300_init_cal(ah, chan, false, ar9300_init_cal);
            }
        }
        ar9300_mci_enable_interrupt(ah);
    }
#endif

    if (!cal_ret) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s: Init Cal Failed\n", __func__);
        FAIL(HAL_ESELFTEST);
    }

    ar9300_init_txbf(ah);
#if 0
    /*
     * WAR for owl 1.0 - restore chain mask for 2-chain cfgs after cal
     */
    rx_chainmask = ahp->ah_rx_chainmask;
    if ((rx_chainmask == 0x5) || (rx_chainmask == 0x3)) {
        OS_REG_WRITE(ah, AR_PHY_RX_CHAINMASK, rx_chainmask);
        OS_REG_WRITE(ah, AR_PHY_CAL_CHAINMASK, rx_chainmask);
    }
#endif

    /* Restore previous led state */
    OS_REG_WRITE(ah, AR_CFG_LED, save_led_state | AR_CFG_SCLK_32KHZ);

#if ATH_BT_COEX_3WIRE_MODE
    if (ah->ah_3wire_bt_coex_enable) {
        ar9300_enable_basic_3wire_btcoex(ah);
    }
#endif
#ifdef ATH_BT_COEX
    if (ahp->ah_bt_coex_config_type != HAL_BT_COEX_CFG_NONE) {
        ar9300_init_bt_coex(ah);

#if ATH_SUPPORT_MCI
        if (AH_PRIVATE(ah)->ah_caps.hal_mci_support && ahp->ah_mci_ready) {
            /* Check BT state again to make sure it's not changed. */
            ar9300_mci_sync_bt_state(ah);
            ar9300_mci_2g5g_switch(ah, true);

            if ((ahp->ah_mci_bt_state == MCI_BT_AWAKE) &&
                (ahp->ah_mci_query_bt == true))
            {
                ahp->ah_mci_need_flush_btinfo = true;
            }
        }
#endif
    }
#endif

    /* Start TSF2 for generic timer 8-15. */
    ar9300_start_tsf2(ah);

    /* MIMO Power save setting */
    if (ar9300_get_capability(ah, HAL_CAP_DYNAMIC_SMPS, 0, AH_NULL) == HAL_OK) {
        ar9300_set_sm_power_mode(ah, ahp->ah_sm_power_mode);
    }

    /*
     * For big endian systems turn on swapping for descriptors
     */
#if AH_BYTE_ORDER == AH_BIG_ENDIAN
    if (AR_SREV_HORNET(ah) || AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_HONEYBEE(ah)|| AR_SREV_DRAGONFLY(ah)) {
        OS_REG_RMW(ah, AR_CFG, AR_CFG_SWTB | AR_CFG_SWRB, 0);
    } else {
        ar9300_init_cfg_reg(ah);
    }
#endif

   if ( AR_SREV_OSPREY(ah) || AR_SREV_WASP(ah) || AR_SREV_SCORPION(ah) || AR_SREV_HONEYBEE(ah)|| AR_SREV_DRAGONFLY(ah)) {
        OS_REG_RMW(ah, AR_CFG_LED, AR_CFG_LED_ASSOC_CTL, AR_CFG_LED_ASSOC_CTL);
    }

#if !(defined(ART_BUILD)) && defined(ATH_SUPPORT_LED)
#define REG_WRITE(_reg, _val)   *((volatile u_int32_t *)(_reg)) = (_val);
#define REG_READ(_reg)          *((volatile u_int32_t *)(_reg))
#define ATH_GPIO_OUT_FUNCTION0  0xB804002C
#define ATH_GPIO_OUT_FUNCTION1  0xB8040030
#define ATH_GPIO_OUT_FUNCTION2  0xB8040034
#define ATH_GPIO_OUT_FUNCTION3  0xB8040038
#define ATH_GPIO_OUT_FUNCTION4  0xB804003C
#define ATH_GPIO_OUT_FUNCTION5  0xB8040040
#define ATH_GPIO_OE             0xB8040000

    if ( AR_SREV_WASP(ah)) {
        if (IS_CHAN_2GHZ((AH_PRIVATE(ah)->ah_curchan))) {
            REG_WRITE(ATH_GPIO_OUT_FUNCTION3, ( REG_READ(ATH_GPIO_OUT_FUNCTION3) & (~(0xff << 8))) | (0x33 << 8) );
            REG_WRITE(ATH_GPIO_OE, ( REG_READ(ATH_GPIO_OE) & (~(0x1 << 13) )));
        }
        else {

            /* Disable 2G WLAN LED. During ath_open, reset function is called even before channel is set.
            So 2GHz is taken as default and it also blinks. Hence
            to avoid both from blinking, disable 2G led while in 5G mode */

            REG_WRITE(ATH_GPIO_OE, ( REG_READ(ATH_GPIO_OE) | (1 << 13) ));
            REG_WRITE(ATH_GPIO_OUT_FUNCTION3, ( REG_READ(ATH_GPIO_OUT_FUNCTION3) & (~(0xff))) | (0x33) );
            REG_WRITE(ATH_GPIO_OE, ( REG_READ(ATH_GPIO_OE) & (~(0x1 << 12) )));
        }

    }
    else if (AR_SREV_SCORPION(ah)) {
        if (IS_CHAN_2GHZ((AH_PRIVATE(ah)->ah_curchan))) {
            REG_WRITE(ATH_GPIO_OUT_FUNCTION3, ( REG_READ(ATH_GPIO_OUT_FUNCTION3) & (~(0xff << 8))) | (0x2F << 8) );
    	    REG_WRITE(ATH_GPIO_OE, (( REG_READ(ATH_GPIO_OE) & (~(0x1 << 13) )) | (0x1 << 12)));
        } else if (IS_CHAN_5GHZ((AH_PRIVATE(ah)->ah_curchan))) {
            REG_WRITE(ATH_GPIO_OUT_FUNCTION3, ( REG_READ(ATH_GPIO_OUT_FUNCTION3) & (~(0xff))) | (0x2F) );
    	    REG_WRITE(ATH_GPIO_OE, (( REG_READ(ATH_GPIO_OE) & (~(0x1 << 12) )) | (0x1 << 13)));
        }
    }
    else if (AR_SREV_HONEYBEE(ah)) {
            REG_WRITE(ATH_GPIO_OUT_FUNCTION3, ( REG_READ(ATH_GPIO_OUT_FUNCTION3) & (~(0xff))) | (0x32) );
            REG_WRITE(ATH_GPIO_OE, (( REG_READ(ATH_GPIO_OE) & (~(0x1 << 12) ))));
    }
    else if (AR_SREV_DRAGONFLY(ah)) {
#ifdef ATH_24G_LED_GPIO
#define _MAX_24G_LED_GPIO 22
            if ((ATH_24G_LED_GPIO <= _MAX_24G_LED_GPIO) && IS_CHAN_2GHZ((AH_PRIVATE(ah)->ah_curchan))) {
                u_int32_t vOB = 0;

                vOB = ATH_24G_LED_GPIO % 4;
                switch(ATH_24G_LED_GPIO / 4)
                {
                    case 0:
                        REG_WRITE(ATH_GPIO_OUT_FUNCTION0, ( REG_READ(ATH_GPIO_OUT_FUNCTION0) & (~(0xff << (8 * vOB)))) | (0x2F << (8 * vOB)));
                        break;
                    case 1:
                        REG_WRITE(ATH_GPIO_OUT_FUNCTION1, ( REG_READ(ATH_GPIO_OUT_FUNCTION1) & (~(0xff << (8 * vOB)))) | (0x2F << (8 * vOB)));
                        break;
                    case 2:
                        REG_WRITE(ATH_GPIO_OUT_FUNCTION2, ( REG_READ(ATH_GPIO_OUT_FUNCTION2) & (~(0xff << (8 * vOB)))) | (0x2F << (8 * vOB)));
                        break;
                    case 3:
                        REG_WRITE(ATH_GPIO_OUT_FUNCTION3, ( REG_READ(ATH_GPIO_OUT_FUNCTION3) & (~(0xff << (8 * vOB)))) | (0x2F << (8 * vOB)));
                        break;
                    case 4:
                        REG_WRITE(ATH_GPIO_OUT_FUNCTION4, ( REG_READ(ATH_GPIO_OUT_FUNCTION4) & (~(0xff << (8 * vOB)))) | (0x2F << (8 * vOB)));
                        break;
                    case 5:
                        REG_WRITE(ATH_GPIO_OUT_FUNCTION5, ( REG_READ(ATH_GPIO_OUT_FUNCTION5) & (~(0xff << (8 * vOB)))) | (0x2F << (8 * vOB)));
                        break;
                }
                REG_WRITE(ATH_GPIO_OE, (( REG_READ(ATH_GPIO_OE) & (~(0x1 << ATH_24G_LED_GPIO) ))));
            }
#undef _MAX_24G_LED_GPIO
#endif
    }

#undef REG_READ
#undef REG_WRITE
#endif

    chan->channel_flags = ichan->channel_flags;
    chan->priv_flags = ichan->priv_flags;

#if FIX_NOISE_FLOOR
    ar9300_get_nf_hist_base(ah, AH_PRIVATE(ah)->ah_curchan, is_scan, nf_buf);
    if (!ar9300_load_nf(ah, nf_buf)) {
        HDPRINTF(ah, HAL_DBG_RESET, "%s Line %d: ar9300_load_nf Failed\n", __func__, __LINE__);
        FAIL(HAL_CAL_TEST);
    }
    if (AH_PRIVATE(ah)->ah_config.ath_hal_enable_adaptiveCCAThres) {
        ar9300_update_cca_threshold(ah, nf_buf, rxchainmask);
    }
    if (nf_hist_buff_reset == 1)
    {
        nf_hist_buff_reset = 0;
    #ifndef ATH_NF_PER_CHAN
	    if (First_NFCal(ah, ichan, is_scan, chan, rxchainmask)){
            if (ahp->ah_skip_rx_iq_cal && !is_scan) {
                /* restore RX Cal result if existing */
                ar9300_rx_iq_cal_restore(ah);
                ahp->ah_skip_rx_iq_cal = false;
            }
		    #ifdef ART_BUILD
            printf("%s Line %d: First_NFCal Failed\n", __func__, __LINE__); //getchar();
            FAIL(HAL_CAL_TEST);
			#endif
        }
    #endif /* ATH_NF_PER_CHAN */
    }
    else{
        ar9300_start_nf_cal(ah);
    }
#endif

#ifdef AH_SUPPORT_AR9300
    /* BB Panic Watchdog */
    if (ar9300_get_capability(ah, HAL_CAP_BB_PANIC_WATCHDOG, 0, AH_NULL) ==
        HAL_OK)
    {
        ar9300_config_bb_panic_watchdog(ah);
    }
#endif
#ifdef AR9340_EMULATION
    /* XXX: Check if this is required for chip */
    OS_REG_WRITE(ah, 0x409c, 0x1);
#endif

    /* While receiving unsupported rate frame receive state machine
     * gets into a state 0xb and if phy_restart happens when rx
     * state machine is in 0xb state, BB would go hang, if we
     * see 0xb state after first bb panic, make sure that we
     * disable the phy_restart.
     *
     * There may be multiple panics, make sure that we always do
     * this if we see this panic at least once. This is required
     * because reset seems to be writing from INI file.
     */
    if ((ar9300_get_capability(ah, HAL_CAP_PHYRESTART_CLR_WAR, 0, AH_NULL)
         == HAL_OK) && (((MS((AH_PRIVATE(ah)->ah_bb_panic_last_status),
                AR_PHY_BB_WD_RX_OFDM_SM)) == 0xb) ||
            AH_PRIVATE(ah)->ah_phyrestart_disabled) )
    {
        ar9300_disable_phy_restart(ah, 1);
    }


#ifdef ART_BUILD
    /* init OTP control setting */
    if (AR_SREV_JUPITER_10(ah)) {
        ar9300_init_otp_Jupiter(ah);
    }
#ifdef AH_SUPPORT_HORNET
    if (AR_SREV_HORNET(ah)) {
        ar9300_init_otp_hornet(ah);
    }
#endif
    if (AR_SREV_WASP(ah)) {
#if AH_SUPPORT_WASP
        /* follow setting should suitable 200MHz+ AHB clock. */
        /* 5000ns @55nm, and assume AHB=250 MHz, */
        OS_REG_WRITE(ah, OTP_PG_STROBE_PW_REG_V_WASP, 1250);
        /* 35ns @55nm */
        OS_REG_WRITE(ah, OTP_RD_STROBE_PW_REG_V_WASP, 9);
        /* 15ns @55nm */
        OS_REG_WRITE(ah, OTP_VDDQ_HOLD_TIME_DELAY_WASP, 4);
        /* 21.2ns @55nm */
        OS_REG_WRITE(ah, OTP_PGENB_SETUP_HOLD_TIME_DELAY_WASP, 6);
        OS_REG_WRITE(ah, OTP_STROBE_PULSE_INTERVAL_DELAY_WASP, 0x0); /* 0 */
        /* 6.8ns@TSMC55nm */
        OS_REG_WRITE(ah, OTP_CSB_ADDR_LOAD_SETUP_HOLD_DELAY_WASP, 0x2);
#endif
    }
#endif

    ahp->ah_radar1 = MS(OS_REG_READ(ah, AR_PHY_RADAR_1),
                        AR_PHY_RADAR_1_CF_BIN_THRESH);
    ahp->ah_dc_offset = MS(OS_REG_READ(ah, AR_PHY_TIMING2),
                        AR_PHY_TIMING2_DC_OFFSET);
    ahp->ah_disable_cck = MS(OS_REG_READ(ah, AR_PHY_MODE),
                        AR_PHY_MODE_DISABLE_CCK);

    if (ap->ah_enable_keysearch_always) {
        ar9300_enable_keysearch_always(ah, 1);
    }

#if ATH_LOW_POWER_ENABLE
#define REG_WRITE(_reg, _val)   *((volatile u_int32_t *)(_reg)) = (_val)
#define REG_READ(_reg)      *((volatile u_int32_t *)(_reg))
    if (AR_SREV_OSPREY(ah)) {
        REG_WRITE(0xb4000080, REG_READ(0xb4000080) | 3);
        OS_REG_WRITE(ah, AR_RTC_RESET, 1);
        OS_REG_SET_BIT(ah, AR_HOSTIF_REG(ah, AR_PCIE_PM_CTRL),
                        AR_PCIE_PM_CTRL_ENA);
        OS_REG_SET_BIT(ah, AR_HOSTIF_REG(ah, AR_SPARE), 0xffffffff);
    }
#undef REG_READ
#undef REG_WRITE
#endif  /* ATH_LOW_POWER_ENABLE */

    WAR_USB_DISABLE_PLL_LOCK_DETECT(ah);

    /* H/W Green TX */
    ar9300_control_signals_for_green_tx_mode(ah);

	/* Fix for EVID 108445 - Continuous beacon stuck */
	/* set AR_PCU_MISC_MODE2 bit 7 CFP_IGNORE to 1 */
	OS_REG_WRITE(ah, AR_PCU_MISC_MODE2,
				 (OS_REG_READ(ah, AR_PCU_MISC_MODE2)|0x80));

    ar9300_set_smart_antenna(ah, ahp->ah_smartantenna_enable, ahp->ah_smartantenna_mode);

    if (ahp->ah_skip_rx_iq_cal && !is_scan) {
        /* restore RX Cal result if existing */
        ar9300_rx_iq_cal_restore(ah);
        ahp->ah_skip_rx_iq_cal = false;
    }

#ifdef JUPITER_EMULATION_WOW_OFFLOAD
    ar9300_enable_wow_pci_config_space(ah);
#endif
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
    ar9300_txbf_loforceon_update(ah,ahp->ah_loforce_enabled);
#endif
    return true;
bad:
    if ((HAL_CAL_TEST == ecode) && (tx_iq_cal_disable == false) &&
            (AR_SREV_DRAGONFLY(ah))) {
        tx_iq_cal_disable = true;
        goto reset_begin;
    } else if ((HAL_CAL_TEST == ecode) && (pk_detect_cal_disable == false) && (AR_SREV_HONEYBEE(ah) ||
                AR_SREV_SCORPION(ah) || AR_SREV_WASP(ah))) {
        pk_detect_cal_disable = true;
        goto reset_begin;
    }
    OS_MARK(ah, AH_MARK_RESET_DONE, ecode);
    *status = ecode;

    if (ahp->ah_skip_rx_iq_cal && !is_scan) {
        /* restore RX Cal result if existing */
        ar9300_rx_iq_cal_restore(ah);
        ahp->ah_skip_rx_iq_cal = false;
    }

    return false;
#undef FAIL
}

void
ar9300_green_ap_ps_on_off( struct ath_hal *ah, u_int16_t on_off)
{
    /* Set/reset the ps flag */
    AH_PRIVATE(ah)->green_ap_ps_on = !!on_off;
}

/*
 * This function returns 1, where it is possible to do
 * single-chain power save.
 */
u_int16_t
ar9300_is_single_ant_power_save_possible(struct ath_hal *ah)
{
    return true;
}

/* To avoid compilation warnings. Functions not used when EMULATION. */
#ifndef AR9340_EMULATION
/*
 * ar9300_find_mag_approx()
 */
static int32_t
ar9300_find_mag_approx(struct ath_hal *ah, int32_t in_re, int32_t in_im)
{
    int32_t abs_i = abs(in_re);
    int32_t abs_q = abs(in_im);
    int32_t max_abs, min_abs;

    if (abs_i > abs_q) {
        max_abs = abs_i;
        min_abs = abs_q;
    } else {
        max_abs = abs_q;
        min_abs = abs_i;
    }

    return (max_abs - (max_abs / 32) + (min_abs / 8) + (min_abs / 4));
}

/*
 * ar9300_solve_iq_cal()
 * solve 4x4 linear equation used in loopback iq cal.
 */
static bool
ar9300_solve_iq_cal(
    struct ath_hal *ah,
    int32_t sin_2phi_1,
    int32_t cos_2phi_1,
    int32_t sin_2phi_2,
    int32_t cos_2phi_2,
    int32_t mag_a0_d0,
    int32_t phs_a0_d0,
    int32_t mag_a1_d0,
    int32_t phs_a1_d0,
    int32_t solved_eq[])
{
    int32_t f1 = cos_2phi_1 - cos_2phi_2;
    int32_t f3 = sin_2phi_1 - sin_2phi_2;
    int32_t f2;
    int32_t mag_tx, phs_tx, mag_rx, phs_rx;
    const int32_t result_shift = 1 << 15;

    f2 = (((int64_t)f1 * (int64_t)f1) / result_shift) + (((int64_t)f3 * (int64_t)f3) / result_shift);

    if (0 == f2) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: Divide by 0(%d).\n",
            __func__, __LINE__);
        return false;
    }

    /* magnitude mismatch, tx */
    mag_tx = f1 * (mag_a0_d0  - mag_a1_d0) + f3 * (phs_a0_d0 - phs_a1_d0);
    /* phase mismatch, tx */
    phs_tx = f3 * (-mag_a0_d0 + mag_a1_d0) + f1 * (phs_a0_d0 - phs_a1_d0);

    mag_tx = (mag_tx / f2);
    phs_tx = (phs_tx / f2);

    /* magnitude mismatch, rx */
    mag_rx =
        mag_a0_d0 - (cos_2phi_1 * mag_tx + sin_2phi_1 * phs_tx) / result_shift;
    /* phase mismatch, rx */
    phs_rx =
        phs_a0_d0 + (sin_2phi_1 * mag_tx - cos_2phi_1 * phs_tx) / result_shift;

    solved_eq[0] = mag_tx;
    solved_eq[1] = phs_tx;
    solved_eq[2] = mag_rx;
    solved_eq[3] = phs_rx;

    return true;
}

/*
 * ar9300_calc_iq_corr()
 */
static bool
ar9300_calc_iq_corr(struct ath_hal *ah, int32_t chain_idx,
    const int32_t iq_res[], int32_t iqc_coeff[])
{
    int32_t i2_m_q2_a0_d0, i2_p_q2_a0_d0, iq_corr_a0_d0;
    int32_t i2_m_q2_a0_d1, i2_p_q2_a0_d1, iq_corr_a0_d1;
    int32_t i2_m_q2_a1_d0, i2_p_q2_a1_d0, iq_corr_a1_d0;
    int32_t i2_m_q2_a1_d1, i2_p_q2_a1_d1, iq_corr_a1_d1;
    int32_t mag_a0_d0, mag_a1_d0, mag_a0_d1, mag_a1_d1;
    int32_t phs_a0_d0, phs_a1_d0, phs_a0_d1, phs_a1_d1;
    int32_t sin_2phi_1, cos_2phi_1, sin_2phi_2, cos_2phi_2;
    int32_t mag_tx, phs_tx, mag_rx, phs_rx;
    int32_t solved_eq[4], mag_corr_tx, phs_corr_tx, mag_corr_rx, phs_corr_rx;
    int32_t q_q_coff, q_i_coff;
    const int32_t res_scale = 1 << 15;
    const int32_t delpt_shift = 1 << 8;
    int32_t mag1, mag2;

    i2_m_q2_a0_d0 = iq_res[0] & 0xfff;
    i2_p_q2_a0_d0 = (iq_res[0] >> 12) & 0xfff;
    iq_corr_a0_d0 = ((iq_res[0] >> 24) & 0xff) + ((iq_res[1] & 0xf) << 8);

    if (i2_m_q2_a0_d0 > 0x800)  {
        i2_m_q2_a0_d0 = -((0xfff - i2_m_q2_a0_d0) + 1);
    }
    if (i2_p_q2_a0_d0 > 0x800)  {
	    i2_p_q2_a0_d0 = -((0xfff - i2_p_q2_a0_d0) + 1);
    }
    if (iq_corr_a0_d0 > 0x800)  {
        iq_corr_a0_d0 = -((0xfff - iq_corr_a0_d0) + 1);
    }

    i2_m_q2_a0_d1 = (iq_res[1] >> 4) & 0xfff;
    i2_p_q2_a0_d1 = (iq_res[2] & 0xfff);
    iq_corr_a0_d1 = (iq_res[2] >> 12) & 0xfff;

    if (i2_m_q2_a0_d1 > 0x800)  {
        i2_m_q2_a0_d1 = -((0xfff - i2_m_q2_a0_d1) + 1);
    }
    if (i2_p_q2_a0_d1 > 0x1000)  {
	    i2_p_q2_a0_d1 = -((0x1fff - i2_p_q2_a0_d1) + 1);
    }
    if (iq_corr_a0_d1 > 0x800)  {
        iq_corr_a0_d1 = -((0xfff - iq_corr_a0_d1) + 1);
    }

    i2_m_q2_a1_d0 = ((iq_res[2] >> 24) & 0xff) + ((iq_res[3] & 0xf) << 8);
    i2_p_q2_a1_d0 = (iq_res[3] >> 4) & 0xfff;
    iq_corr_a1_d0 = iq_res[4] & 0xfff;

    if (i2_m_q2_a1_d0 > 0x800)  {
        i2_m_q2_a1_d0 = -((0xfff - i2_m_q2_a1_d0) + 1);
    }
    if (i2_p_q2_a1_d0 > 0x800)  {
	    i2_p_q2_a1_d0 = -((0xfff - i2_p_q2_a1_d0) + 1);
    }
    if (iq_corr_a1_d0 > 0x800)  {
        iq_corr_a1_d0 = -((0xfff - iq_corr_a1_d0) + 1);
    }

    i2_m_q2_a1_d1 = (iq_res[4] >> 12) & 0xfff;
    i2_p_q2_a1_d1 = ((iq_res[4] >> 24) & 0xff) + ((iq_res[5] & 0xf) << 8);
    iq_corr_a1_d1 = (iq_res[5] >> 4) & 0xfff;

    if (i2_m_q2_a1_d1 > 0x800)  {
        i2_m_q2_a1_d1 = -((0xfff - i2_m_q2_a1_d1) + 1);
    }
    if (i2_p_q2_a1_d1 > 0x800)  {
	    i2_p_q2_a1_d1 = -((0xfff - i2_p_q2_a1_d1) + 1);
    }
    if (iq_corr_a1_d1 > 0x800)  {
        iq_corr_a1_d1 = -((0xfff - iq_corr_a1_d1) + 1);
    }

    if ((i2_p_q2_a0_d0 == 0) ||
        (i2_p_q2_a0_d1 == 0) ||
        (i2_p_q2_a1_d0 == 0) ||
        (i2_p_q2_a1_d1 == 0)) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: Divide by 0(%d):\na0_d0=%d\na0_d1=%d\na2_d0=%d\na1_d1=%d\n",
            __func__, __LINE__,
            i2_p_q2_a0_d0, i2_p_q2_a0_d1, i2_p_q2_a1_d0, i2_p_q2_a1_d1);
        return false;
    }

    if ((i2_p_q2_a0_d0 < 1024) || (i2_p_q2_a0_d0 > 2047) ||
            (i2_p_q2_a1_d0 < 0) || (i2_p_q2_a1_d1 < 0) ||
            (i2_p_q2_a0_d0 <= i2_m_q2_a0_d0) ||
            (i2_p_q2_a0_d0 <= iq_corr_a0_d0) ||
            (i2_p_q2_a0_d1 <= i2_m_q2_a0_d1) ||
            (i2_p_q2_a0_d1 <= iq_corr_a0_d1) ||
            (i2_p_q2_a1_d0 <= i2_m_q2_a1_d0) ||
            (i2_p_q2_a1_d0 <= iq_corr_a1_d0) ||
            (i2_p_q2_a1_d1 <= i2_m_q2_a1_d1) ||
            (i2_p_q2_a1_d1 <= iq_corr_a1_d1)) {
		HDPRINTF(ah, HAL_DBG_CALIBRATE,"%s: %d, bounds check failed\n",
					__func__,__LINE__);
        return false;
    }

    mag_a0_d0 = (i2_m_q2_a0_d0 * res_scale) / i2_p_q2_a0_d0;
    phs_a0_d0 = (iq_corr_a0_d0 * res_scale) / i2_p_q2_a0_d0;

    mag_a0_d1 = (i2_m_q2_a0_d1 * res_scale) / i2_p_q2_a0_d1;
    phs_a0_d1 = (iq_corr_a0_d1 * res_scale) / i2_p_q2_a0_d1;

    mag_a1_d0 = (i2_m_q2_a1_d0 * res_scale) / i2_p_q2_a1_d0;
    phs_a1_d0 = (iq_corr_a1_d0 * res_scale) / i2_p_q2_a1_d0;

    mag_a1_d1 = (i2_m_q2_a1_d1 * res_scale) / i2_p_q2_a1_d1;
    phs_a1_d1 = (iq_corr_a1_d1 * res_scale) / i2_p_q2_a1_d1;

    /* without analog phase shift */
    sin_2phi_1 = (((mag_a0_d0 - mag_a0_d1) * delpt_shift) / DELPT);
    /* without analog phase shift */
    cos_2phi_1 = (((phs_a0_d1 - phs_a0_d0) * delpt_shift) / DELPT);
    /* with  analog phase shift */
    sin_2phi_2 = (((mag_a1_d0 - mag_a1_d1) * delpt_shift) / DELPT);
    /* with analog phase shift */
    cos_2phi_2 = (((phs_a1_d1 - phs_a1_d0) * delpt_shift) / DELPT);

    /* force sin^2 + cos^2 = 1; */
    /* find magnitude by approximation */
    mag1 = ar9300_find_mag_approx(ah, cos_2phi_1, sin_2phi_1);
    mag2 = ar9300_find_mag_approx(ah, cos_2phi_2, sin_2phi_2);

    if ((mag1 == 0) || (mag2 == 0)) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: Divide by 0(%d): mag1=%d, mag2=%d\n",
            __func__, __LINE__, mag1, mag2);
        return false;
    }

    /* normalization sin and cos by mag */
    sin_2phi_1 = (sin_2phi_1 * res_scale / mag1);
    cos_2phi_1 = (cos_2phi_1 * res_scale / mag1);
    sin_2phi_2 = (sin_2phi_2 * res_scale / mag2);
    cos_2phi_2 = (cos_2phi_2 * res_scale / mag2);

    /* calculate IQ mismatch */
    if (false == ar9300_solve_iq_cal(ah,
            sin_2phi_1, cos_2phi_1, sin_2phi_2, cos_2phi_2, mag_a0_d0,
            phs_a0_d0, mag_a1_d0, phs_a1_d0, solved_eq))
    {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: Call to ar9300_solve_iq_cal failed.\n", __func__);
        return false;
    }

    mag_tx = solved_eq[0];
    phs_tx = solved_eq[1];
    mag_rx = solved_eq[2];
    phs_rx = solved_eq[3];

    HDPRINTF(ah, HAL_DBG_CALIBRATE,
        "%s: chain %d: mag mismatch=%d phase mismatch=%d\n",
        __func__, chain_idx, mag_tx / res_scale, phs_tx / res_scale);

    if (res_scale == mag_tx) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: Divide by 0(%d): mag_tx=%d, res_scale=%d\n",
            __func__, __LINE__, mag_tx, res_scale);
        return false;
    }

    /* calculate and quantize Tx IQ correction factor */
    mag_corr_tx = (mag_tx * res_scale) / (res_scale - mag_tx);
    phs_corr_tx = -phs_tx;

    q_q_coff = (mag_corr_tx * 128 / res_scale);
    q_i_coff = (phs_corr_tx * 256 / res_scale);

    HDPRINTF(ah, HAL_DBG_CALIBRATE,
        "%s: tx chain %d: mag corr=%d  phase corr=%d\n",
        __func__, chain_idx, q_q_coff, q_i_coff);

    if (q_i_coff < -63) {
        q_i_coff = -63;
    }
    if (q_i_coff > 63) {
        q_i_coff = 63;
    }
    if (q_q_coff < -63) {
        q_q_coff = -63;
    }
    if (q_q_coff > 63) {
        q_q_coff = 63;
    }

    iqc_coeff[0] = (q_q_coff * 128) + (0x7f & q_i_coff);

    HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: tx chain %d: iq corr coeff=%x\n",
        __func__, chain_idx, iqc_coeff[0]);

    if (-mag_rx == res_scale) {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: Divide by 0(%d): mag_rx=%d, res_scale=%d\n",
            __func__, __LINE__, mag_rx, res_scale);
        return false;
    }

    /* calculate and quantize Rx IQ correction factors */
    mag_corr_rx = (-mag_rx * res_scale) / (res_scale + mag_rx);
    phs_corr_rx = -phs_rx;

    q_q_coff = (mag_corr_rx * 128 / res_scale);
    q_i_coff = (phs_corr_rx * 256 / res_scale);

    HDPRINTF(ah, HAL_DBG_CALIBRATE,
        "%s: rx chain %d: mag corr=%d  phase corr=%d\n",
        __func__, chain_idx, q_q_coff, q_i_coff);

    if (q_i_coff < -63) {
        q_i_coff = -63;
    }
    if (q_i_coff > 63) {
        q_i_coff = 63;
    }
    if (q_q_coff < -63) {
        q_q_coff = -63;
    }
    if (q_q_coff > 63) {
        q_q_coff = 63;
    }

    iqc_coeff[1] = (q_q_coff * 128) + (0x7f & q_i_coff);

    HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s: rx chain %d: iq corr coeff=%x\n",
        __func__, chain_idx, iqc_coeff[1]);

    return true;
}

#define MAX_MAG_DELTA 11 //maximum magnitude mismatch delta across gains
#define MAX_PHS_DELTA 10 //maximum phase mismatch delta across gains
#define ABS(x) ((x) >= 0 ? (x) : (-(x)))

    u_int32_t tx_corr_coeff[MAX_MEASUREMENT][AR9300_MAX_CHAINS] = {
    {   AR_PHY_TX_IQCAL_CORR_COEFF_01_B0,
        AR_PHY_TX_IQCAL_CORR_COEFF_01_B1,
        AR_PHY_TX_IQCAL_CORR_COEFF_01_B2},
    {   AR_PHY_TX_IQCAL_CORR_COEFF_01_B0,
        AR_PHY_TX_IQCAL_CORR_COEFF_01_B1,
        AR_PHY_TX_IQCAL_CORR_COEFF_01_B2},
    {   AR_PHY_TX_IQCAL_CORR_COEFF_23_B0,
        AR_PHY_TX_IQCAL_CORR_COEFF_23_B1,
        AR_PHY_TX_IQCAL_CORR_COEFF_23_B2},
    {   AR_PHY_TX_IQCAL_CORR_COEFF_23_B0,
        AR_PHY_TX_IQCAL_CORR_COEFF_23_B1,
        AR_PHY_TX_IQCAL_CORR_COEFF_23_B2},
    {   AR_PHY_TX_IQCAL_CORR_COEFF_45_B0,
        AR_PHY_TX_IQCAL_CORR_COEFF_45_B1,
        AR_PHY_TX_IQCAL_CORR_COEFF_45_B2},
    {   AR_PHY_TX_IQCAL_CORR_COEFF_45_B0,
        AR_PHY_TX_IQCAL_CORR_COEFF_45_B1,
        AR_PHY_TX_IQCAL_CORR_COEFF_45_B2},
    {   AR_PHY_TX_IQCAL_CORR_COEFF_67_B0,
        AR_PHY_TX_IQCAL_CORR_COEFF_67_B1,
        AR_PHY_TX_IQCAL_CORR_COEFF_67_B2},
    {   AR_PHY_TX_IQCAL_CORR_COEFF_67_B0,
        AR_PHY_TX_IQCAL_CORR_COEFF_67_B1,
        AR_PHY_TX_IQCAL_CORR_COEFF_67_B2},
    };

static void
ar9300_tx_iq_cal_outlier_detection(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichan, u_int32_t num_chains,
    struct coeff_t *coeff,bool is_cal_reusable)
{
    struct ath_hal_9300     *ahp = AH9300(ah);
    int nmeasurement=0, ch_idx, im;
    int n_valid_mag_meas, n_valid_phs_meas;
    int32_t magnitude=0, phase=0;
    int32_t magnitude_max, phase_max;
    int32_t magnitude_min, phase_min;

    int32_t magnitude_max_idx, phase_max_idx;
    int32_t magnitude_min_idx, phase_min_idx;

    int32_t magnitude_avg, phase_avg;
    int32_t outlier_mag_idx = 0;
    int32_t outlier_phs_idx = 0;

    if (AR_SREV_POSEIDON(ah)) {
        HALASSERT(num_chains == 0x1);

        tx_corr_coeff[0][0] = AR_PHY_TX_IQCAL_CORR_COEFF_01_B0_POSEIDON;
        tx_corr_coeff[1][0] = AR_PHY_TX_IQCAL_CORR_COEFF_01_B0_POSEIDON;
        tx_corr_coeff[2][0] = AR_PHY_TX_IQCAL_CORR_COEFF_23_B0_POSEIDON;
        tx_corr_coeff[3][0] = AR_PHY_TX_IQCAL_CORR_COEFF_23_B0_POSEIDON;
        tx_corr_coeff[4][0] = AR_PHY_TX_IQCAL_CORR_COEFF_45_B0_POSEIDON;
        tx_corr_coeff[5][0] = AR_PHY_TX_IQCAL_CORR_COEFF_45_B0_POSEIDON;
        tx_corr_coeff[6][0] = AR_PHY_TX_IQCAL_CORR_COEFF_67_B0_POSEIDON;
        tx_corr_coeff[7][0] = AR_PHY_TX_IQCAL_CORR_COEFF_67_B0_POSEIDON;
    }

    for (ch_idx = 0; ch_idx < AR9300_MAX_CHAINS; ch_idx++) {
	if (ahp->ah_tx_chainmask & (1 << ch_idx)) {
			HDPRINTF(ah, HAL_DBG_CALIBRATE, "%s:applying correction factors for chain %d\n", __func__,ch_idx);
        nmeasurement = OS_REG_READ_FIELD(ah,
            AR_PHY_TX_IQCAL_STATUS_B0(ah), AR_PHY_CALIBRATED_GAINS_0);
        if (nmeasurement > MAX_MEASUREMENT) {
            nmeasurement = MAX_MEASUREMENT;
        }

        if (!AR_SREV_SCORPION(ah)) {
            /*
             * reset max/min variable to min/max values so that
             * we always start with 1st calibrated gain value
             */
            magnitude_max = -64;
            phase_max     = -64;
            magnitude_min = 63;
            phase_min     = 63;
            magnitude_avg = 0;
            phase_avg     = 0;
			magnitude	  = 0;
			phase		  = 0;
            magnitude_max_idx = 0;
            magnitude_min_idx = 0;
            phase_max_idx = 0;
            phase_min_idx = 0;
            n_valid_mag_meas = 0;
            n_valid_phs_meas = 0;

            /* detect outlier only if nmeasurement > 1 */
            if (nmeasurement > 1) {
                /* printf("----------- start outlier detection -----------\n"); */
                /*
                 * find max/min and phase/mag mismatch across all calibrated gains
                 */
                for (im = 0; im < nmeasurement; im++) {
                    magnitude = coeff->mag_coeff[ch_idx][im][0];
                    phase = coeff->phs_coeff[ch_idx][im][0];

                    if (magnitude > magnitude_max) {
                        magnitude_max = magnitude;
                        magnitude_max_idx = im;
                    }
                    if (magnitude < magnitude_min) {
                        magnitude_min = magnitude;
                        magnitude_min_idx = im;
                    }
                    if (phase > phase_max) {
                        phase_max = phase;
                        phase_max_idx = im;
                    }
                    if (phase < phase_min) {
                        phase_min = phase;
                        phase_min_idx = im;
                    }
                }
                /* find average (exclude max abs value) */
                for (im = 0; im < nmeasurement; im++) {
                    magnitude = coeff->mag_coeff[ch_idx][im][0];
                    phase = coeff->phs_coeff[ch_idx][im][0];
                    if ((ABS(magnitude) < ABS(magnitude_max)) ||
                        (ABS(magnitude) < ABS(magnitude_min)))
                    {
                        magnitude_avg = magnitude_avg + magnitude;
                        n_valid_mag_meas = n_valid_mag_meas + 1;
                    }
                    if ((ABS(phase) < ABS(phase_max)) ||
                        (ABS(phase) < ABS(phase_min)))
                    {
                        phase_avg = phase_avg + phase;
                        n_valid_phs_meas = n_valid_phs_meas + 1;
                    }
                }

                if (n_valid_mag_meas > 0) {
                    magnitude_avg = magnitude_avg / n_valid_mag_meas;
                } else {
                    magnitude_avg = magnitude;
                }

                if (n_valid_phs_meas > 0) {
                    phase_avg = phase_avg / n_valid_phs_meas;
                } else {
                    phase_avg = phase;
                }

                /* detect magnitude outlier */
                if (ABS(magnitude_max - magnitude_min) > MAX_MAG_DELTA) {
                    if (ABS(magnitude_max - magnitude_avg) >
                        ABS(magnitude_min - magnitude_avg))
                    {
                        /* max is outlier, force to avg */
                        outlier_mag_idx = magnitude_max_idx;
                    } else {
                        /* min is outlier, force to avg */
                        outlier_mag_idx = magnitude_min_idx;
                    }
                    coeff->mag_coeff[ch_idx][outlier_mag_idx][0] = magnitude_avg;
                    coeff->phs_coeff[ch_idx][outlier_mag_idx][0] = phase_avg;
                    HDPRINTF(ah, HAL_DBG_CALIBRATE,
                        "[ch%d][outlier mag gain%d]:: "
                        "mag_avg = %d (/128), phase_avg = %d (/256)\n",
                        ch_idx, outlier_mag_idx, magnitude_avg, phase_avg);
                }
                /* detect phase outlier */
                if (ABS(phase_max - phase_min) > MAX_PHS_DELTA) {
                    if (ABS(phase_max-phase_avg) > ABS(phase_min - phase_avg)) {
                        /* max is outlier, force to avg */
                        outlier_phs_idx = phase_max_idx;
                    } else{
                        /* min is outlier, force to avg */
                        outlier_phs_idx = phase_min_idx;
                    }
                    coeff->mag_coeff[ch_idx][outlier_phs_idx][0] = magnitude_avg;
                    coeff->phs_coeff[ch_idx][outlier_phs_idx][0] = phase_avg;
                    HDPRINTF(ah, HAL_DBG_CALIBRATE,
                        "[ch%d][outlier phs gain%d]:: "
                        "mag_avg = %d (/128), phase_avg = %d (/256)\n",
                        ch_idx, outlier_phs_idx, magnitude_avg, phase_avg);
                }
            }
        }

        /*printf("------------ after outlier detection -------------\n");*/
        for (im = 0; im < nmeasurement; im++) {
            magnitude = coeff->mag_coeff[ch_idx][im][0];
            phase = coeff->phs_coeff[ch_idx][im][0];

            #if 0
            printf("[ch%d][gain%d]:: mag = %d (/128), phase = %d (/256)\n",
                ch_idx, im, magnitude, phase);
            #endif

            coeff->iqc_coeff[0] = (phase & 0x7f) | ((magnitude & 0x7f) << 7);

            if ((im % 2) == 0) {
                OS_REG_RMW_FIELD(ah,
                    tx_corr_coeff[im][ch_idx],
                    AR_PHY_TX_IQCAL_CORR_COEFF_00_COEFF_TABLE,
                    coeff->iqc_coeff[0]);
            } else {
                OS_REG_RMW_FIELD(ah,
                    tx_corr_coeff[im][ch_idx],
                    AR_PHY_TX_IQCAL_CORR_COEFF_01_COEFF_TABLE,
                    coeff->iqc_coeff[0]);
            }
#if ATH_SUPPORT_CAL_REUSE
            ichan->tx_corr_coeff[im][ch_idx] = coeff->iqc_coeff[0];
#endif
	}
        }
#if ATH_SUPPORT_CAL_REUSE
        ichan->num_measures[ch_idx] = nmeasurement;
#endif
    }

    OS_REG_RMW_FIELD(ah, AR_PHY_TX_IQCAL_CONTROL_3,
                     AR_PHY_TX_IQCAL_CONTROL_3_IQCORR_EN, 0x1);
    OS_REG_RMW_FIELD(ah, AR_PHY_RX_IQCAL_CORR_B0,
                     AR_PHY_RX_IQCAL_CORR_B0_LOOPBACK_IQCORR_EN, 0x1);

#if ATH_SUPPORT_CAL_REUSE
    if (is_cal_reusable) {
        ichan->one_time_txiqcal_done = true;
        HDPRINTF(ah, HAL_DBG_FCS_RTT,
            "(FCS) TXIQCAL saved - %d\n", ichan->channel);
    }
#endif
}

#if ATH_SUPPORT_CAL_REUSE
static void
ar9300_tx_iq_cal_apply(struct ath_hal *ah, HAL_CHANNEL_INTERNAL *ichan)
{
    struct ath_hal_9300 *ahp = AH9300(ah);
    int nmeasurement, ch_idx, im;

    u_int32_t tx_corr_coeff[MAX_MEASUREMENT][AR9300_MAX_CHAINS] = {
        {   AR_PHY_TX_IQCAL_CORR_COEFF_01_B0,
            AR_PHY_TX_IQCAL_CORR_COEFF_01_B1,
            AR_PHY_TX_IQCAL_CORR_COEFF_01_B2},
        {   AR_PHY_TX_IQCAL_CORR_COEFF_01_B0,
            AR_PHY_TX_IQCAL_CORR_COEFF_01_B1,
            AR_PHY_TX_IQCAL_CORR_COEFF_01_B2},
        {   AR_PHY_TX_IQCAL_CORR_COEFF_23_B0,
            AR_PHY_TX_IQCAL_CORR_COEFF_23_B1,
            AR_PHY_TX_IQCAL_CORR_COEFF_23_B2},
        {   AR_PHY_TX_IQCAL_CORR_COEFF_23_B0,
            AR_PHY_TX_IQCAL_CORR_COEFF_23_B1,
            AR_PHY_TX_IQCAL_CORR_COEFF_23_B2},
        {   AR_PHY_TX_IQCAL_CORR_COEFF_45_B0,
            AR_PHY_TX_IQCAL_CORR_COEFF_45_B1,
            AR_PHY_TX_IQCAL_CORR_COEFF_45_B2},
        {   AR_PHY_TX_IQCAL_CORR_COEFF_45_B0,
            AR_PHY_TX_IQCAL_CORR_COEFF_45_B1,
            AR_PHY_TX_IQCAL_CORR_COEFF_45_B2},
        {   AR_PHY_TX_IQCAL_CORR_COEFF_67_B0,
            AR_PHY_TX_IQCAL_CORR_COEFF_67_B1,
            AR_PHY_TX_IQCAL_CORR_COEFF_67_B2},
        {   AR_PHY_TX_IQCAL_CORR_COEFF_67_B0,
            AR_PHY_TX_IQCAL_CORR_COEFF_67_B1,
            AR_PHY_TX_IQCAL_CORR_COEFF_67_B2},
    };

    if (AR_SREV_POSEIDON(ah)) {
        HALASSERT(ahp->ah_tx_cal_chainmask == 0x1);

        tx_corr_coeff[0][0] = AR_PHY_TX_IQCAL_CORR_COEFF_01_B0_POSEIDON;
        tx_corr_coeff[1][0] = AR_PHY_TX_IQCAL_CORR_COEFF_01_B0_POSEIDON;
        tx_corr_coeff[2][0] = AR_PHY_TX_IQCAL_CORR_COEFF_23_B0_POSEIDON;
        tx_corr_coeff[3][0] = AR_PHY_TX_IQCAL_CORR_COEFF_23_B0_POSEIDON;
        tx_corr_coeff[4][0] = AR_PHY_TX_IQCAL_CORR_COEFF_45_B0_POSEIDON;
        tx_corr_coeff[5][0] = AR_PHY_TX_IQCAL_CORR_COEFF_45_B0_POSEIDON;
        tx_corr_coeff[6][0] = AR_PHY_TX_IQCAL_CORR_COEFF_67_B0_POSEIDON;
        tx_corr_coeff[7][0] = AR_PHY_TX_IQCAL_CORR_COEFF_67_B0_POSEIDON;
    }

    for (ch_idx = 0; ch_idx < AR9300_MAX_CHAINS; ch_idx++) {
        if ((ahp->ah_tx_cal_chainmask & (1 << ch_idx)) == 0) {
            continue;
        }
        nmeasurement = ichan->num_measures[ch_idx];

        for (im = 0; im < nmeasurement; im++) {
            if ((im % 2) == 0) {
                OS_REG_RMW_FIELD(ah,
                    tx_corr_coeff[im][ch_idx],
                    AR_PHY_TX_IQCAL_CORR_COEFF_00_COEFF_TABLE,
                    ichan->tx_corr_coeff[im][ch_idx]);
            } else {
                OS_REG_RMW_FIELD(ah,
                    tx_corr_coeff[im][ch_idx],
                    AR_PHY_TX_IQCAL_CORR_COEFF_01_COEFF_TABLE,
                    ichan->tx_corr_coeff[im][ch_idx]);
            }
        }
    }

    OS_REG_RMW_FIELD(ah, AR_PHY_TX_IQCAL_CONTROL_3,
                     AR_PHY_TX_IQCAL_CONTROL_3_IQCORR_EN, 0x1);
    OS_REG_RMW_FIELD(ah, AR_PHY_RX_IQCAL_CORR_B0,
                     AR_PHY_RX_IQCAL_CORR_B0_LOOPBACK_IQCORR_EN, 0x1);
}
#endif

/*
 * ar9300_tx_iq_cal_hw_run is only needed for osprey/wasp/hornet
 * It is not needed for jupiter/poseidon.
 */
bool
ar9300_tx_iq_cal_hw_run(struct ath_hal *ah)
{
    int is_tx_gain_forced;

    is_tx_gain_forced = OS_REG_READ_FIELD(ah,
        AR_PHY_TX_FORCED_GAIN, AR_PHY_TXGAIN_FORCE);
    if (is_tx_gain_forced) {
        /*printf("Tx gain can not be forced during tx I/Q cal!\n");*/
        OS_REG_RMW_FIELD(ah, AR_PHY_TX_FORCED_GAIN, AR_PHY_TXGAIN_FORCE, 0);
    }

    /* enable tx IQ cal */
    OS_REG_RMW_FIELD(ah, AR_PHY_TX_IQCAL_START(ah),
        AR_PHY_TX_IQCAL_START_DO_CAL, AR_PHY_TX_IQCAL_START_DO_CAL);

    if (!ath_hal_wait(ah,
            AR_PHY_TX_IQCAL_START(ah), AR_PHY_TX_IQCAL_START_DO_CAL, 0,
            AH_WAIT_TIMEOUT))
    {
        HDPRINTF(ah, HAL_DBG_CALIBRATE,
            "%s: Tx IQ Cal is never completed.\n", __func__);
        return false;
    }
    return true;
}

static void
ar9300_tx_iq_cal_post_proc(struct ath_hal *ah,HAL_CHANNEL_INTERNAL *ichan,
                           int iqcal_idx, int max_iqcal,bool is_cal_reusable, bool apply_last_corr)
{
    int nmeasurement=0, im, ix, iy, temp;
    struct ath_hal_9300     *ahp = AH9300(ah);
    u_int32_t txiqcal_status[AR9300_MAX_CHAINS] = {
        AR_PHY_TX_IQCAL_STATUS_B0(ah),
        AR_PHY_TX_IQCAL_STATUS_B1,
        AR_PHY_TX_IQCAL_STATUS_B2,
    };
    const u_int32_t chan_info_tab[] = {
        AR_PHY_CHAN_INFO_TAB_0,
        AR_PHY_CHAN_INFO_TAB_1,
        AR_PHY_CHAN_INFO_TAB_2,
    };
    int32_t iq_res[6];
    int32_t ch_idx, j;
    u_int32_t num_chains = 0;

    static struct coeff_t coeff;
    u_int32_t reg_PhyChInfoMem;

    txiqcal_status[0] = AR_PHY_TX_IQCAL_STATUS_B0(ah);
	reg_PhyChInfoMem = (AR_SREV_DRAGONFLY(ah)) ?
		AR_PHY_CHAN_INFO_MEMORY_DRAGONFLY:AR_PHY_CHAN_INFO_MEMORY;

    for (ch_idx = 0; ch_idx < AR9300_MAX_CHAINS; ch_idx++) {
        if (ahp->ah_tx_chainmask & (1 << ch_idx)) {
            num_chains++;
        }
    }

    if (AR_SREV_DRAGONFLY(ah)) {
	apply_last_corr = false;
    }

    if (apply_last_corr) {
	    if (coeff.last_cal == true) {
		    int32_t magnitude, phase;
		    int ch_idx, im;
		    u_int32_t tx_corr_coeff[MAX_MEASUREMENT][AR9300_MAX_CHAINS] = {
			    {   AR_PHY_TX_IQCAL_CORR_COEFF_01_B0,
				    AR_PHY_TX_IQCAL_CORR_COEFF_01_B1,
				    AR_PHY_TX_IQCAL_CORR_COEFF_01_B2},
			    {   AR_PHY_TX_IQCAL_CORR_COEFF_01_B0,
				    AR_PHY_TX_IQCAL_CORR_COEFF_01_B1,
				    AR_PHY_TX_IQCAL_CORR_COEFF_01_B2},
			    {   AR_PHY_TX_IQCAL_CORR_COEFF_23_B0,
				    AR_PHY_TX_IQCAL_CORR_COEFF_23_B1,
				    AR_PHY_TX_IQCAL_CORR_COEFF_23_B2},
			    {   AR_PHY_TX_IQCAL_CORR_COEFF_23_B0,
				    AR_PHY_TX_IQCAL_CORR_COEFF_23_B1,
				    AR_PHY_TX_IQCAL_CORR_COEFF_23_B2},
			    {   AR_PHY_TX_IQCAL_CORR_COEFF_45_B0,
				    AR_PHY_TX_IQCAL_CORR_COEFF_45_B1,
				    AR_PHY_TX_IQCAL_CORR_COEFF_45_B2},
			    {   AR_PHY_TX_IQCAL_CORR_COEFF_45_B0,
				    AR_PHY_TX_IQCAL_CORR_COEFF_45_B1,
				    AR_PHY_TX_IQCAL_CORR_COEFF_45_B2},
			    {   AR_PHY_TX_IQCAL_CORR_COEFF_67_B0,
				    AR_PHY_TX_IQCAL_CORR_COEFF_67_B1,
				    AR_PHY_TX_IQCAL_CORR_COEFF_67_B2},
			    {   AR_PHY_TX_IQCAL_CORR_COEFF_67_B0,
				    AR_PHY_TX_IQCAL_CORR_COEFF_67_B1,
				    AR_PHY_TX_IQCAL_CORR_COEFF_67_B2},
		    };
		    for (ch_idx = 0; ch_idx < AR9300_MAX_CHAINS; ch_idx++) {
			    if (ahp->ah_tx_chainmask & (1 << ch_idx)) {
				    for (im = 0; im < coeff.last_nmeasurement; im++) {
					    magnitude = coeff.mag_coeff[ch_idx][im][0];
					    phase = coeff.phs_coeff[ch_idx][im][0];

#if 0
					    printf("[ch%d][gain%d]:: mag = %d (/128), phase = %d (/256)\n",
						    ch_idx, im, magnitude, phase);
#endif

					    coeff.iqc_coeff[0] = (phase & 0x7f) | ((magnitude & 0x7f) << 7);
					    if ((im % 2) == 0) {
						    OS_REG_RMW_FIELD(ah,
							    tx_corr_coeff[im][ch_idx],
							    AR_PHY_TX_IQCAL_CORR_COEFF_00_COEFF_TABLE,
							    coeff.iqc_coeff[0]);
					    } else {
						    OS_REG_RMW_FIELD(ah,
							    tx_corr_coeff[im][ch_idx],
							    AR_PHY_TX_IQCAL_CORR_COEFF_01_COEFF_TABLE,
							    coeff.iqc_coeff[0]);
					    }
				    }
			    }
		    }
		    OS_REG_RMW_FIELD(ah, AR_PHY_TX_IQCAL_CONTROL_3,
				    AR_PHY_TX_IQCAL_CONTROL_3_IQCORR_EN, 0x1);
	    }
	    return;
    }


    for (ch_idx = 0; ch_idx < AR9300_MAX_CHAINS; ch_idx++) {
	if (ahp->ah_tx_chainmask & (1 << ch_idx)) {
	HDPRINTF(ah, HAL_DBG_CALIBRATE,"%s:post proc for chain %d\n", __func__, ch_idx);
        nmeasurement = OS_REG_READ_FIELD(ah,
            AR_PHY_TX_IQCAL_STATUS_B0(ah), AR_PHY_CALIBRATED_GAINS_0);
        if (nmeasurement > MAX_MEASUREMENT) {
            nmeasurement = MAX_MEASUREMENT;
        }

        for (im = 0; im < nmeasurement; im++) {
            HDPRINTF(ah, HAL_DBG_CALIBRATE,
                "%s: Doing Tx IQ Cal for chain %d.\n", __func__, ch_idx);
            if (OS_REG_READ(ah, txiqcal_status[ch_idx]) &
                AR_PHY_TX_IQCAL_STATUS_FAILED)
            {
                HDPRINTF(ah, HAL_DBG_CALIBRATE,
                    "%s: Tx IQ Cal failed for chain %d.\n", __func__, ch_idx);
		        HDPRINTF(ah, HAL_DBG_CALIBRATE,"%s:Hardware failure: \t", __func__);
                goto TX_IQ_CAL_FAILED_;
            }

            for (j = 0; j < 3; j++) {
                u_int32_t idx = 2 * j;
                /* 3 registers for each calibration result */
                u_int32_t offset = 4 * (3 * im + j);

                OS_REG_RMW_FIELD(ah, reg_PhyChInfoMem,
                    AR_PHY_CHAN_INFO_TAB_S2_READ, 0);
                /* 32 bits */
                iq_res[idx] = OS_REG_READ(ah, chan_info_tab[ch_idx] + offset);
                OS_REG_RMW_FIELD(ah, reg_PhyChInfoMem,
                    AR_PHY_CHAN_INFO_TAB_S2_READ, 1);
                /* 16 bits */
                iq_res[idx + 1] = 0xffff &
                    OS_REG_READ(ah, chan_info_tab[ch_idx] + offset);

                HDPRINTF(ah, HAL_DBG_CALIBRATE,
                    "%s: IQ RES[%d]=0x%x IQ_RES[%d]=0x%x\n",
                    __func__, idx, iq_res[idx], idx + 1, iq_res[idx + 1]);
            }

            if (false == ar9300_calc_iq_corr(
                             ah, ch_idx, iq_res, coeff.iqc_coeff))
            {
                HDPRINTF(ah, HAL_DBG_CALIBRATE,
                    "%s: Failed in calculation of IQ correction.\n",
                     __func__);
		        HDPRINTF(ah, HAL_DBG_CALIBRATE,"%s:Software failure: \t", __func__);
                goto TX_IQ_CAL_FAILED_;
            }

            coeff.phs_coeff[ch_idx][im][iqcal_idx-1] = coeff.iqc_coeff[0] & 0x7f;
            coeff.mag_coeff[ch_idx][im][iqcal_idx-1] = (coeff.iqc_coeff[0] >> 7) & 0x7f;
            if (coeff.mag_coeff[ch_idx][im][iqcal_idx-1] > 63) {
                coeff.mag_coeff[ch_idx][im][iqcal_idx-1] -= 128;
            }
            if (coeff.phs_coeff[ch_idx][im][iqcal_idx-1] > 63) {
                coeff.phs_coeff[ch_idx][im][iqcal_idx-1] -= 128;
            }
#if 0
            ath_hal_printf(ah, "IQCAL::[ch%d][gain%d]:: mag = %d phase = %d \n",
                ch_idx, im, coeff.mag_coeff[ch_idx][im][iqcal_idx-1],
                coeff.phs_coeff[ch_idx][im][iqcal_idx-1]);
#endif
	}
        }
    }
    //last iteration; calculate mag and phs
    if (iqcal_idx == max_iqcal) {
        if (max_iqcal>1) {
            for (ch_idx = 0; ch_idx < AR9300_MAX_CHAINS; ch_idx++) {
                for (im = 0; im < nmeasurement; im++) {
                    //sort mag and phs
                    for( ix=0;ix<max_iqcal-1;ix++){
                        for( iy=ix+1;iy<=max_iqcal-1;iy++){
                            if(coeff.mag_coeff[ch_idx][im][iy] <
                                coeff.mag_coeff[ch_idx][im][ix]) {
                                //swap
                                temp=coeff.mag_coeff[ch_idx][im][ix];
                                coeff.mag_coeff[ch_idx][im][ix] = coeff.mag_coeff[ch_idx][im][iy];
                                coeff.mag_coeff[ch_idx][im][iy] = temp;
                            }
                            if(coeff.phs_coeff[ch_idx][im][iy] <
                                coeff.phs_coeff[ch_idx][im][ix]){
                                //swap
                                temp=coeff.phs_coeff[ch_idx][im][ix];
                                coeff.phs_coeff[ch_idx][im][ix]=coeff.phs_coeff[ch_idx][im][iy];
                                coeff.phs_coeff[ch_idx][im][iy]=temp;
                            }
                        }
                    }
                    //select median; 3rd entry in the sorted array
                    coeff.mag_coeff[ch_idx][im][0] =
                        coeff.mag_coeff[ch_idx][im][max_iqcal/2];
                    coeff.phs_coeff[ch_idx][im][0] =
                        coeff.phs_coeff[ch_idx][im][max_iqcal/2];
                    HDPRINTF(ah, HAL_DBG_CALIBRATE,
                        "IQCAL: Median [ch%d][gain%d]:: mag = %d phase = %d \n",
                        ch_idx, im,coeff.mag_coeff[ch_idx][im][0],
                        coeff.phs_coeff[ch_idx][im][0]);
                }
            }
        }
        ar9300_tx_iq_cal_outlier_detection(ah,ichan, num_chains, &coeff,is_cal_reusable);
    }


    coeff.last_nmeasurement = nmeasurement;
    coeff.last_cal = true;

    return;

TX_IQ_CAL_FAILED_:
    /* no need to print this, it is AGC failure not chip stuck */
    /*ath_hal_printf(ah, "Tx IQ Cal failed(%d)\n", line);*/
    HDPRINTF(ah, HAL_DBG_CALIBRATE, "Tx IQ cal failed \n");
    coeff.last_cal = false;
    return;
}

#endif

/*
 * ar9300_disable_phy_restart
 *
 * In some BBpanics, we can disable the phyrestart
 * disable_phy_restart
 *      != 0, disable the phy restart in h/w
 *      == 0, enable the phy restart in h/w
 */
void ar9300_disable_phy_restart(struct ath_hal *ah, int disable_phy_restart)
{
    u_int32_t val;

    val = OS_REG_READ(ah, AR_PHY_RESTART);
    if (disable_phy_restart) {
        val &= ~AR_PHY_RESTART_ENA;
        AH_PRIVATE(ah)->ah_phyrestart_disabled = 1;
    } else {
        val |= AR_PHY_RESTART_ENA;
        AH_PRIVATE(ah)->ah_phyrestart_disabled = 0;
    }
    OS_REG_WRITE(ah, AR_PHY_RESTART, val);

    val = OS_REG_READ(ah, AR_PHY_RESTART);
}

bool
ar9300_interference_is_present(struct ath_hal *ah)
{
    int i;
    struct ath_hal_private  *ahpriv = AH_PRIVATE(ah);

    /* This function is called after a stuck beacon, if EACS is enabled.
     * If CW interference is severe, then HW goes into a loop of continuous
     * stuck beacons and resets. On reset the NF cal history is cleared.
     * So the median value of the history cannot be used -
     * hence check if any value (Chain 0/Primary Channel)
     * is outside the bounds.
     */
    HAL_NFCAL_HIST_FULL *h = AH_HOME_CHAN_NFCAL_HIST(ah);
    for (i = 0; i < HAL_NF_CAL_HIST_LEN_FULL; i++) {
        if (h->nf_cal_buffer[i][0] >
            ahpriv->nfp->nominal + ahpriv->nf_cw_int_delta)
        {
            return true;
        }

    }
    return false;
}
#if ATH_TxBF_DYNAMIC_LOF_ON_N_CHAIN_MASK
HAL_BOOL ar9300_txbf_loforceon_update(struct ath_hal *ah,bool loforcestate)
{

    struct ath_hal_9300 *ahp = AH9300(ah);

    if (loforcestate){
        OS_REG_RMW_FIELD(ah,
                AR_PHY_65NM_CH0_RXTX3, AR_PHY_65NM_CHAIN_RXTX3_LOFORCE_MASK, 1);
        if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah) && !AR_SREV_APHRODITE(ah)) {
            OS_REG_RMW_FIELD(ah,
                    AR_PHY_65NM_CH1_RXTX3, AR_PHY_65NM_CHAIN_RXTX3_LOFORCE_MASK, 1);
            //TODO :If any new chip 2x2 introduced,please add condition here as follows.
            if (!AR_SREV_WASP(ah) && !AR_SREV_JUPITER(ah) && !AR_SREV_HONEYBEE(ah)) {
                OS_REG_RMW_FIELD(ah,
                        AR_PHY_65NM_CH2_RXTX3, AR_PHY_65NM_CHAIN_RXTX3_LOFORCE_MASK, 1);
            }
        }
        OS_REG_WRITE(ah, AR_SELFGEN_MASK,1);
        ahp->ah_loforce_enabled = 1;
    }else {
        OS_REG_RMW_FIELD(ah,
                AR_PHY_65NM_CH0_RXTX3, AR_PHY_65NM_CHAIN_RXTX3_LOFORCE_MASK, 0);
        if (!AR_SREV_HORNET(ah) && !AR_SREV_POSEIDON(ah) && !AR_SREV_APHRODITE(ah)) {
            OS_REG_RMW_FIELD(ah,
                    AR_PHY_65NM_CH1_RXTX3, AR_PHY_65NM_CHAIN_RXTX3_LOFORCE_MASK, 0);
            //TODO :If any new chip 2x2 introduced,please add condition here as follows.
            if (!AR_SREV_WASP(ah) && !AR_SREV_JUPITER(ah) && !AR_SREV_HONEYBEE(ah)) {
                OS_REG_RMW_FIELD(ah,
                        AR_PHY_65NM_CH2_RXTX3, AR_PHY_65NM_CHAIN_RXTX3_LOFORCE_MASK, 0);
            }
        }
        if (AH_PRIVATE(ah)->ah_caps.hal_enable_apm && (ahp->ah_tx_chainmask == 0x7)){
            OS_REG_WRITE(ah, AR_SELFGEN_MASK,0x03);
        }
        else{
            OS_REG_WRITE(ah, AR_SELFGEN_MASK,ahp->ah_tx_chainmask);
        }
        ahp->ah_loforce_enabled = 0;
    }
    return AH_TRUE;
}
#endif

#ifdef ART_BUILD

void
ar9300_get_pll_info(unsigned int *cpu_freq, unsigned int *ddr_freq,
    unsigned int *ahb_freq)
{
    #define AR_SOC_SEL_25M_40M      0xb80600ac
    #define AR_SOC_CPU_PLL_CONFIG   0xb8050000
    #define AR_SOC_CLOCK_CONTROL    0xb8050008
    #define AR_SOC_PLL_DITHER_FRAC  0xb8050010
    #define AR_SOC_PLL_DITHER       0xb8050014

    unsigned int    reg_bootstrap;
    unsigned int    reg_cpu_pll_config;
    unsigned int    reg_clock_control;
    unsigned int    reg_pll_dither_frac;
    unsigned int    reg_pll_dither;
    unsigned int    sel_25m_40m;
    unsigned int    outdiv, refdiv, nint, nfrac;
    unsigned int    nfrac_min, nfrac_max;
    unsigned int    dither_en;
    unsigned int    cpu_post_div, ddr_post_div, ahb_post_div, bypass;
    /*====================================================================*/
    unsigned int    xtal, vco, pll_freq;
    unsigned int    vco_int, vco_frac, outdiv_decimal;
    /*====================================================================*/

    MyRegisterRead(AR_SOC_SEL_25M_40M, &reg_bootstrap);
    sel_25m_40m = (reg_bootstrap >> 0) & 0x1;
    if (sel_25m_40m) {
        xtal = 40 * 1000 * 1000;
    } else {
        xtal = 25 * 1000 * 1000;
    }

    MyRegisterRead(AR_SOC_CPU_PLL_CONFIG, &reg_cpu_pll_config);
    MyRegisterRead(AR_SOC_CLOCK_CONTROL, &reg_clock_control);
    MyRegisterRead(AR_SOC_PLL_DITHER_FRAC, &reg_pll_dither_frac);
    MyRegisterRead(AR_SOC_PLL_DITHER, &reg_pll_dither);

    outdiv = (reg_cpu_pll_config >> 23) & 0x7;
    refdiv = (reg_cpu_pll_config >> 16) & 0x1f;
    nint   = (reg_cpu_pll_config >> 10) & 0x3f;

    nfrac_max = (reg_pll_dither_frac >>  0) & 0x3ff;
    nfrac_max <<= 8;
    nfrac_min = (reg_pll_dither_frac >> 10) & 0x3ff;
    nfrac_min <<= 8;

    dither_en = (reg_pll_dither >> 31) & 0x1;

    if (dither_en) {
        nfrac = (nfrac_max + nfrac_min) / 2;
    } else {
        nfrac = nfrac_min;
    }

    cpu_post_div = (reg_clock_control >>  5) & 0x3;
    ddr_post_div = (reg_clock_control >> 10) & 0x3;
    ahb_post_div = (reg_clock_control >> 15) & 0x3;

    bypass       = (reg_clock_control >> 2) & 0x1;

    cpu_post_div++;
    ddr_post_div++;
    ahb_post_div++;

    if (bypass) {
        *cpu_freq = xtal / cpu_post_div;
        *ddr_freq = xtal / ddr_post_div;
        *ahb_freq = xtal / ahb_post_div;
        return;
    }

    vco_int  = (xtal / refdiv) * nint;
    vco_frac = (xtal / (refdiv * 0x3ffff)) * nfrac;

    vco = vco_int + vco_frac;

    if (outdiv == 1) {
        outdiv_decimal = 2;
    } else if (outdiv == 2) {
        outdiv_decimal = 4;
    } else if (outdiv == 3) {
        outdiv_decimal = 8;
    } else if (outdiv == 4) {
        outdiv_decimal = 16;
    } else if (outdiv == 5) {
        outdiv_decimal = 32;
    } else if (outdiv == 6) {
        outdiv_decimal = 64;
    } else if (outdiv == 7) {
        outdiv_decimal = 128;
    } else {
        outdiv_decimal = 1;
    }

    pll_freq = vco / outdiv_decimal;

    *cpu_freq = pll_freq / cpu_post_div;
    *ddr_freq = pll_freq / ddr_post_div;
    *ahb_freq = pll_freq / ahb_post_div;
}

#ifdef AH_SUPPORT_HORNET
void ar9300_init_otp_hornet(struct ath_hal *ah)
{
    #define OTP_PG_STROBE_PW_REG_V              0x15f08
    #define OTP_RD_STROBE_PW_REG_V              0x15f0c
    #define OTP_VDDQ_HOLD_TIME_DELAY            0x15f30
    #define OTP_PGENB_SETUP_HOLD_TIME_DELAY     0x15f34
    #define OTP_STROBE_PULSE_INTERVAL_DELAY     0x15f38
    #define OTP_CSB_ADDR_LOAD_SETUP_HOLD_DELAY  0x15f3c

    unsigned int cpu_freq = 0, ddr_freq = 0, ahb_freq = 0;
    unsigned long temp_reg = 0;

    ar9300_get_pll_info(&cpu_freq, &ddr_freq, &ahb_freq);

    /*
    printf("CPU %d Hz, DDR %d Hz, AHB %d Hz\n", cpu_freq, ddr_freq, ahb_freq);
     */

    temp_reg = ahb_freq * pow(10, -9) * 5000 * 10;
    if (fmod(temp_reg, 10) != 0) {
        temp_reg = (temp_reg / 10) + 1;
    } else {
        temp_reg = temp_reg / 10;
    }
    /*printf("reg %x = %x\n", OTP_PG_STROBE_PW_REG_V, temp_reg);*/
    /* it need 5000ns for TSMC55nm */
    OS_REG_WRITE(ah, OTP_PG_STROBE_PW_REG_V, (unsigned int)temp_reg);
    temp_reg = ahb_freq * pow(10, -9) * 35 * 10;
    if (fmod(temp_reg, 10) != 0) {
        temp_reg = (temp_reg / 10) + 1;
    } else {
        temp_reg = temp_reg / 10;
    }
    /*printf("reg %x = %x\n", OTP_RD_STROBE_PW_REG_V, temp_reg);*/
    /* it need 35ns for TSMC55nm */
    OS_REG_WRITE(ah, OTP_RD_STROBE_PW_REG_V, (unsigned int)temp_reg - 1);
    temp_reg = ahb_freq * pow(10, -9) * 15 * 10;
    if (fmod(temp_reg, 10) != 0) {
        temp_reg = (temp_reg / 10) + 1;
    } else {
        temp_reg = temp_reg / 10;
    }
    /*printf("reg %x = %x\n", OTP_VDDQ_HOLD_TIME_DELAY, temp_reg);*/
    /* it need 15ns for TSMC55nm */
    OS_REG_WRITE(ah, OTP_VDDQ_HOLD_TIME_DELAY, (unsigned int)temp_reg - 1);
    temp_reg = ahb_freq * pow(10, -9) * 21.2 * 10;
    if (fmod(temp_reg, 10) != 0) {
        temp_reg = (temp_reg / 10) + 1;
    } else {
        temp_reg = temp_reg / 10;
    }
    /*printf("reg %x = %x\n", OTP_PGENB_SETUP_HOLD_TIME_DELAY, temp_reg);*/
    /* it need 21.2ns for TSMC55nm */
    OS_REG_WRITE(ah,
        OTP_PGENB_SETUP_HOLD_TIME_DELAY, (unsigned int)temp_reg - 1);
    /* it need 0 for TSMC55nm */
    OS_REG_WRITE(ah, OTP_STROBE_PULSE_INTERVAL_DELAY, 0x0);
    temp_reg = ahb_freq * pow(10, -9) * 6.8 * 10;
    if (fmod(temp_reg, 10) != 0) {
        temp_reg = (temp_reg / 10) + 1;
    } else {
        temp_reg = temp_reg / 10;
    }
    /*printf("reg %x = %x\n", OTP_CSB_ADDR_LOAD_SETUP_HOLD_DELAY, temp_reg);*/
    /* it need 6.8ns for TSMC55nm */
    OS_REG_WRITE(ah,
        OTP_CSB_ADDR_LOAD_SETUP_HOLD_DELAY, (unsigned int)temp_reg - 1);
}
#endif /* hornet */
void ar9300_init_otp_Jupiter(struct ath_hal *ah)
{
    u_int32_t tmp_data;
    OS_REG_WRITE(ah,
        OTP_PGENB_SETUP_HOLD_TIME_DELAY, (unsigned int)0x6);
    OS_REG_WRITE(ah,
        BTOTP_INTF2, (unsigned int)0x5);

	/* check and make sure BT_CLOCK_CONTROL bit 5 is set */
    tmp_data = OS_REG_READ(ah, BT_CLOCK_CONTROL);
    if (!(tmp_data & 0x20)) {
        tmp_data |= 0x00000020;
        OS_REG_WRITE(ah, BT_CLOCK_CONTROL, tmp_data);
	}
	/* set/reset BT_RESET_CTL */
    tmp_data = OS_REG_READ(ah, BT_RESET_CTL);
    OS_REG_WRITE(ah, BT_RESET_CTL, (tmp_data | 0x00008000));
    OS_DELAY(10);
    OS_REG_WRITE(ah, BT_RESET_CTL, (tmp_data & (~0x00008000)));
}

//Function - Ar9300_get_corr_coeff
//Purpose  - Retrive runtime memory tx coefficient and number of entries
//Parameter- coeff_type : COEFF_TX_TYPE, COEFF_RX_TYPE
//           coeff_array : retunred array adress of tx/rx coeff
//           row  : return number of entries
//           col  : return size of each of entries
//Return   - None
int Ar9300_get_corr_coeff(int coeff_type, u_int32_t **coeff_array, unsigned int *max_row, unsigned int *max_col)
{
	switch (coeff_type) {
		case AR9300_COEFF_TX_TYPE:
				*max_row = MAX_MEASUREMENT;
				*max_col = AR9300_MAX_CHAINS;
				*coeff_array =(u_int32_t *)tx_corr_coeff;
				break;
		case AR9300_COEFF_RX_TYPE:
				//*max_row = MAX_MEASUREMENT;
				//*max_col = AR9300_MAX_CHAINS;
				//*coeff_array =(u_int32_t *)rx_corr_coeff;
				break;
		default :
				return (-1);
	}
	return (0);
}

#endif /* ART_BUILD */
#if ATH_ANT_DIV_COMB
HAL_BOOL
ar9300_ant_ctrl_set_lna_div_use_bt_ant(struct ath_hal *ah, HAL_BOOL enable, HAL_CHANNEL *chan)
{
    u_int32_t value;
    u_int32_t regval;
    struct ath_hal_9300 *ahp = AH9300(ah);
    HAL_CHANNEL_INTERNAL *ichan;
    struct ath_hal_private *ahpriv = AH_PRIVATE(ah);
    HAL_CAPABILITIES *pcap = &ahpriv->ah_caps;

    if (AR_SREV_POSEIDON(ah)) {
        // Make sure this scheme is only used for WB225(Astra)
        ahp->ah_lna_div_use_bt_ant_enable = enable;

        ichan = ar9300_check_chan(ah, chan);
        if ( ichan == AH_NULL ) {
            HDPRINTF(ah, HAL_DBG_CHANNEL, "%s: invalid channel %u/0x%x; no mapping\n",
                     __func__, chan->channel, chan->channel_flags);
            return AH_FALSE;
        }

        if ( enable == TRUE ) {
            pcap->hal_ant_div_comb_support = TRUE;
        } else {
            pcap->hal_ant_div_comb_support = pcap->hal_ant_div_comb_support_org;
        }

#define AR_SWITCH_TABLE_COM2_ALL (0xffffff)
#define AR_SWITCH_TABLE_COM2_ALL_S (0)
        value = ar9300_ant_ctrl_common2_get(ah, IS_CHAN_2GHZ(ichan));
        if ( enable == TRUE ) {
            value &= ~AR_SWITCH_TABLE_COM2_ALL;
            value |= ahpriv->ah_config.ath_hal_ant_ctrl_comm2g_switch_enable;
        }
        OS_REG_RMW_FIELD(ah, AR_PHY_SWITCH_COM_2, AR_SWITCH_TABLE_COM2_ALL, value);

        value = ar9300_eeprom_get(ahp, EEP_ANTDIV_control);
        /* main_lnaconf, alt_lnaconf, main_tb, alt_tb */
        regval = OS_REG_READ(ah, AR_PHY_MC_GAIN_CTRL);
        regval &= (~ANT_DIV_CONTROL_ALL); /* clear bit 25~30 */
        regval |= (value & 0x3f) << ANT_DIV_CONTROL_ALL_S;
        /* enable_lnadiv */
        regval &= (~MULTICHAIN_GAIN_CTRL__ENABLE_ANT_DIV_LNADIV__MASK);
        regval |= ((value >> 6) & 0x1) <<
                  MULTICHAIN_GAIN_CTRL__ENABLE_ANT_DIV_LNADIV__SHIFT;
        if ( enable == TRUE ) {
            regval |= ANT_DIV_ENABLE;
        }
        OS_REG_WRITE(ah, AR_PHY_MC_GAIN_CTRL, regval);

        /* enable fast_div */
        regval = OS_REG_READ(ah, AR_PHY_CCK_DETECT);
        regval &= (~BBB_SIG_DETECT__ENABLE_ANT_FAST_DIV__MASK);
        regval |= ((value >> 7) & 0x1) <<
                  BBB_SIG_DETECT__ENABLE_ANT_FAST_DIV__SHIFT;
        if ( enable == TRUE ) {
            regval |= FAST_DIV_ENABLE;
        }
        OS_REG_WRITE(ah, AR_PHY_CCK_DETECT, regval);

        if ( AR_SREV_POSEIDON_11_OR_LATER(ah) ) {
            if (pcap->hal_ant_div_comb_support) {
                /* If support DivComb, set MAIN to LNA1 and ALT to LNA2 at the first beginning */
                regval = OS_REG_READ(ah, AR_PHY_MC_GAIN_CTRL);
                /* clear bit 25~30 main_lnaconf, alt_lnaconf, main_tb, alt_tb */
                regval &= (~(MULTICHAIN_GAIN_CTRL__ANT_DIV_MAIN_LNACONF__MASK |
                             MULTICHAIN_GAIN_CTRL__ANT_DIV_ALT_LNACONF__MASK |
                             MULTICHAIN_GAIN_CTRL__ANT_DIV_ALT_GAINTB__MASK |
                             MULTICHAIN_GAIN_CTRL__ANT_DIV_MAIN_GAINTB__MASK));
                regval |= (HAL_ANT_DIV_COMB_LNA1 <<
                           MULTICHAIN_GAIN_CTRL__ANT_DIV_MAIN_LNACONF__SHIFT);
                regval |= (HAL_ANT_DIV_COMB_LNA2 <<
                           MULTICHAIN_GAIN_CTRL__ANT_DIV_ALT_LNACONF__SHIFT);
                OS_REG_WRITE(ah, AR_PHY_MC_GAIN_CTRL, regval);
            }
        }

        return AH_TRUE;
    } else {
        return AH_TRUE;
    }
}
#endif /* ATH_ANT_DIV_COMB */

#endif /* AH_SUPPORT_AR9300 */
