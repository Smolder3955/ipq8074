/*
 * Copyright (c) 2010, Atheros Communications Inc. 
 * All Rights Reserved.
 * 
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 * 
 */

#include "ath_internal.h"
#include <osdep.h>
#if ATH_SUPPORT_RAW_ADC_CAPTURE
#include "ah.h"
#include "spectral_raw_adc_ioctl.h"

#define MAX_CAPTURE_COUNT   (SPECTRAL_RAW_ADC_SAMPLES_PER_CALL*SPECTRAL_RAW_ADC_MAX_CHAINS)
#define DC_FILT_WINDOW      32

typedef struct {
    SPECTRAL_ADC_SAMPLE p[SPECTRAL_RAW_ADC_MAX_CHAINS];
} ADC_SAMPLE_POINT;

typedef struct {
    ADC_SAMPLE_POINT lb[SPECTRAL_RAW_ADC_SAMPLES_PER_CALL];
    ADC_SAMPLE_POINT lb_seed;
    int is_seeded;
} SPECTRAL_ADC_DC_FILTER;

int spectral_enter_raw_capture_mode(ath_dev_t dev, void *indata)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    SPECTRAL_ADC_CAPTURE_PARAMS *params = (SPECTRAL_ADC_CAPTURE_PARAMS*)indata;
    HAL_CHANNEL hchan;
    int error = 0;

    if (ath_hal_getcapability(sc->sc_ah, HAL_CAP_RAW_ADC_CAPTURE, 0, NULL) != HAL_OK) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: called but raw capture mode not supported!\n", __func__, __LINE__);
        return -ENOTSUP;
    }

    /* sanity check input version */
    if (params->version != SPECTRAL_ADC_API_VERSION) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: version mismatch: expect %d got %d\n", 
                __func__, __LINE__, params->version, SPECTRAL_ADC_API_VERSION);
        return -EINVAL; 
    }

    if (sc->sc_invalid) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: called but device is invalid or removed!\n", __func__, __LINE__);
        return -ENXIO;
    }

    if (sc->sc_raw_adc_capture_enabled) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: already raw capture mode - calling spectral_exit_raw_capture_mode() first\n", __func__, __LINE__);
        spectral_exit_raw_capture_mode(dev);
    }

    /* save current channel and chain mask so we can restore later */
    sc->sc_save_rx_chainmask = sc->sc_rx_chainmask;
    sc->sc_save_current_chan = sc->sc_curchan;
    sc->sc_save_current_opmode = sc->sc_opmode;
    /* ensure chain mask is valid */
    params->chain_info.chain_mask &= sc->sc_rx_chainmask;
    /* store requested capture params */
    sc->sc_adc_chain_mask = params->chain_info.chain_mask;
    sc->sc_adc_num_chains = sc->sc_mask2chains[params->chain_info.chain_mask];
    sc->sc_adc_capture_flags = params->capture_flags;
    /* set new chain mask */
    ath_set_rx_chainmask(dev, params->chain_info.chain_mask);
    /* change channel to that requested */
    OS_MEMZERO(&hchan, sizeof(hchan));
    hchan.channel = params->freq;
    if (hchan.channel > 5000)
        hchan.channel_flags = CHANNEL_A_HT20;
    else
        hchan.channel_flags = CHANNEL_G_HT20;
    sc->sc_full_reset = 1; /* ensure we do a full reset */
    ath_set_channel(dev, &hchan, 0, 0, 0, 0); 
    /* save channel info to return with capture data later */
    sc->sc_adc_chan_flags = sc->sc_curchan.channel_flags;
    sc->sc_adc_freq = sc->sc_curchan.channel;
    sc->sc_adc_ieee = ath_hal_mhz2ieee(sc->sc_ah, sc->sc_adc_freq, sc->sc_adc_chan_flags);
    /* did the channel change succeed ?*/
    if (sc->sc_curchan.channel != hchan.channel ||
        sc->sc_curchan.channel_flags != hchan.channel_flags) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: set chan to %dMhz[%x] FAILED! current chan=%dMhz[0x%x]\n", 
                __func__, __LINE__, hchan.channel, hchan.channel_flags, 
                sc->sc_curchan.channel, sc->sc_curchan.channel_flags);
        error = -EINVAL; 
    } else {
        /* ok, all set up - enter raw capture mode */
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: chan is now %dMhz flags=0x%x mask=0x%x\n", __func__, __LINE__, 
                sc->sc_curchan.channel, sc->sc_curchan.channel_flags, params->chain_info.chain_mask);
        sc->sc_raw_adc_capture_enabled = 1;
        sc->sc_opmode = HAL_M_RAW_ADC_CAPTURE;
        ath_hal_enable_test_addac_mode(sc->sc_ah);
    }

    return error;
}

int spectral_exit_raw_capture_mode(ath_dev_t dev)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    int error = 0;

    if (sc->sc_raw_adc_capture_enabled) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: restore chan=%d flags=0x%x imask=0x%x\n", __func__, __LINE__, 
                sc->sc_save_current_chan.channel, sc->sc_save_current_chan.channel_flags, sc->sc_imask);
        sc->sc_opmode = sc->sc_save_current_opmode;
        if (!sc->sc_invalid) {
            ath_hal_disable_test_addac_mode(sc->sc_ah);
            ath_set_rx_chainmask(dev,sc->sc_save_rx_chainmask);
            sc->sc_full_reset = 1; /* ensure we do a full reset */
            ath_set_channel(dev, &sc->sc_save_current_chan, 0, 0, 0, 0);
        }
        sc->sc_raw_adc_capture_enabled = 0;
    }    
    return error;
}


static void spectral_run_dc_filter(ath_dev_t dev, SPECTRAL_ADC_DATA *spc, SPECTRAL_ADC_DC_FILTER* dcf)
{
    u_int32_t i, ch, loops;
    SPECTRAL_ADC_SAMPLE *sample;
    int num_chains = spc->cap.chain_info.num_chains;

    loops = dcf->is_seeded ? 1 : 2;
    for (; loops > 0; loops--) {
        sample = spc->data;
        for (i=0; i < SPECTRAL_RAW_ADC_SAMPLES_PER_CALL; i++, sample+=num_chains) {
            if (i == 0) {
                dcf->lb[i] = dcf->lb_seed;
            } else {
                //lb_dc(n)=data_in(n) + lb_dc(n-1) - floor( lb_dc(n-1)/alpha );
                for (ch=0; ch < num_chains; ch++) {
                    dcf->lb[i].p[ch].sampleI = sample[ch].sampleI 
                                               + dcf->lb[i-1].p[ch].sampleI
                                               - (dcf->lb[i-1].p[ch].sampleI/DC_FILT_WINDOW);
                    dcf->lb[i].p[ch].sampleQ = sample[ch].sampleQ 
                                               + dcf->lb[i-1].p[ch].sampleQ 
                                               - (dcf->lb[i-1].p[ch].sampleQ/DC_FILT_WINDOW);
                }
            }
        }
        // use the last value as seed!
        dcf->lb_seed = dcf->lb[SPECTRAL_RAW_ADC_SAMPLES_PER_CALL-1];
        dcf->is_seeded = 1;
    }

    // data_out = data_in - lb_dc/alpha;
    sample = spc->data;
    for (i=0; i < SPECTRAL_RAW_ADC_SAMPLES_PER_CALL; i++, sample+=num_chains) {
        for (ch=0; ch < num_chains; ch++) {
            sample[ch].sampleI -= dcf->lb[i].p[ch].sampleI/DC_FILT_WINDOW;
            sample[ch].sampleQ -= dcf->lb[i].p[ch].sampleQ/DC_FILT_WINDOW;
        }
    }
}


int spectral_retrieve_raw_capture(ath_dev_t dev, void* outdata, u_int32_t *outsize)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    SPECTRAL_ADC_DATA *rparams = (SPECTRAL_ADC_DATA *)outdata;
    int error = 0;
    u_int32_t max_samples;
    HAL_STATUS status;
    SPECTRAL_ADC_DC_FILTER* dcf = NULL;

    if (!sc->sc_raw_adc_capture_enabled) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: called while not in raw capture mode!\n", __func__, __LINE__);
        return -ENOTCONN; 
    }

    if (sc->sc_opmode != HAL_M_RAW_ADC_CAPTURE) {
        spectral_exit_raw_capture_mode(dev);
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: called while HAL mode not raw capture!\n", __func__, __LINE__);
        return -ESTALE; 
    }

    if (*outsize < sizeof(SPECTRAL_ADC_DATA) || !outdata) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: too small: expect >= %d, got %d\n", 
                __func__, __LINE__, sizeof(SPECTRAL_ADC_DATA), *outsize);
        return -EINVAL; 
    }

    if (sc->sc_invalid) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: called but device is invalid or removed!\n", __func__, __LINE__);
        return -ENXIO;
    }

    if (!(sc->sc_adc_capture_flags & SPECTRAL_ADC_CAPTURE_FLAG_DISABLE_DC_FILTER)) {
        dcf = (SPECTRAL_ADC_DC_FILTER *) OS_MALLOC(dev, sizeof(SPECTRAL_ADC_DC_FILTER), GPF_KERNEL);
        if (dcf) {
            OS_MEMZERO(dcf, sizeof(SPECTRAL_ADC_DC_FILTER)); 
        } else {
            DPRINTF(sc, ATH_DEBUG_ANY, "%s: alloc DC filter failed!\n", __func__);
        }
    }

    DPRINTF(sc, ATH_DEBUG_RECV, "%s[%d]: chain_mask=0x%x capture_flags=0x%x\n", __func__, __LINE__, sc->sc_adc_chain_mask, sc->sc_adc_capture_flags);
    ath_hal_begin_adc_capture(sc->sc_ah, (sc->sc_adc_capture_flags & SPECTRAL_ADC_CAPTURE_FLAG_AGC_AUTO));
    rparams->cap.version = SPECTRAL_ADC_API_VERSION;
    rparams->duration = SPECTRAL_RAW_ADC_SAMPLES_PER_CALL / SPECTRAL_RAW_ADC_SAMPLES_FREQUENCY_MHZ;
    rparams->sample_len = max_samples = (*outsize - sizeof(SPECTRAL_ADC_DATA))/sizeof(SPECTRAL_ADC_SAMPLE);
    rparams->cap.chain_info.num_chains = sc->sc_adc_num_chains;
    rparams->cap.freq = sc->sc_adc_freq;
    rparams->cap.ieee = sc->sc_adc_ieee;
    rparams->cap.chan_flags = sc->sc_adc_chan_flags;
    rparams->cap.chain_info.chain_mask = sc->sc_adc_chain_mask;
    rparams->cap.capture_flags = sc->sc_adc_capture_flags;
    ath_hal_calculate_adc_ref_powers(sc->sc_ah, sc->sc_adc_freq, &rparams->min_sample_val, &rparams->max_sample_val,
                                     rparams->ref_power, sizeof(rparams->ref_power)/sizeof(rparams->ref_power[0]));
    if ((status = ath_hal_retrieve_capture_data(sc->sc_ah, sc->sc_adc_chain_mask,
                                                1, /* disable dc filt here - now we do it after call */
                                                rparams->data, &rparams->sample_len)) != HAL_OK) {
        DPRINTF(sc, ATH_DEBUG_ANY, "%s[%d]: ath_hal_retrieve_capture_data failed[%d]: cnt passed was %d[%d]\n", 
                __func__, __LINE__, status, max_samples, rparams->sample_len);
        error = (status == HAL_ENOMEM) ? -ENOMEM : -EIO; 
    } else {
        if (!(sc->sc_adc_capture_flags & SPECTRAL_ADC_CAPTURE_FLAG_DISABLE_DC_FILTER) &&
            dcf) 
        {
            DPRINTF(sc, ATH_DEBUG_RECV, "%s: running DC filter [wnd=%d] over %d chains now...\n", 
                    __func__, DC_FILT_WINDOW, sc->sc_adc_num_chains);
            spectral_run_dc_filter(dev, rparams, dcf);
        }
        *outsize = sizeof(SPECTRAL_ADC_DATA) + rparams->sample_len*sizeof(SPECTRAL_ADC_SAMPLE);
    }

    if (dcf) {
        OS_FREE(dcf);
    }

    return error;
}

#endif
