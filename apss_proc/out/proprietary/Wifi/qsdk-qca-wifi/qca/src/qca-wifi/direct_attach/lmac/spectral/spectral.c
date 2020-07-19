/*
 * Copyright (c) 2011,2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2002-2009 Atheros Communications, Inc.
 * All Rights Reserved.
 */
#include <osdep.h>
#include "ah.h"
#include "spectral.h"
#include "ath_internal.h"
#include "ieee80211_var.h"
#include <wlan_objmgr_cmn.h>
#include <wlan_spectral_public_structs.h>
#include <wlan_osif_priv.h>
#include "if_athvar.h"

#if WLAN_SPECTRAL_ENABLE

// Globals
int BTH_MIN_NUMBER_OF_FRAMES    =   8;
extern int spectral_debug_level;
struct ath_spectral_ops ops_spectral;

// Function declarations
static u_int64_t null_get_tsf64(void* arg);
static u_int32_t null_get_capability(void* arg, enum spectral_capability_type type);
static u_int32_t null_set_rxfilter(void* arg, int rxfilter);
static u_int32_t null_get_rxfilter(void* arg);
static u_int32_t null_is_spectral_active(void* arg);
static u_int32_t null_is_spectral_enabled(void* arg);
static u_int32_t null_start_spectral_scan(void* arg);
static u_int32_t null_stop_spectral_scan(void* arg);
static u_int32_t null_get_extension_channel(void* arg);
static int8_t null_get_ctl_noisefloor(void* arg);
static int8_t null_get_ext_noisefloor(void* arg);
static u_int32_t null_configure_spectral(void* arg, struct spectral_config* params);
static u_int32_t null_get_spectral_config(void* arg, struct spectral_config* params);
static u_int32_t null_get_ent_spectral_mask(void* arg);
static u_int32_t null_get_mac_address(void* arg, char* addr);
static u_int32_t null_get_current_channel(void* arg);
static u_int32_t null_reset_hw(void* arg);
static u_int32_t null_get_chain_noise_floor(void* arg, int16_t* nfBuf);
static int null_spectral_process_phyerr(struct ath_spectral* spectral, u_int8_t* data, u_int32_t datalen,
                SPECTRAL_RFQUAL_INFO* p_rfqual,
                SPECTRAL_CHAN_INFO* p_chaninfo,
                u_int64_t tsf64,
                spectral_acs_stats_t *acs_stats);
#if WLAN_SPECTRAL_ENABLE_DBG_FUNCS
static void test_spectral_ops(struct ath_spectral* spectral);
#endif /* WLAN_SPECTRAL_ENABLE_DBG_FUNCS */
static void spectral_init_param_defaults(struct ath_spectral* spectral);

/*
 * Function     : spectral_init_dummy_function_table
 * Description  : Initializes dummy Spectral functions
 * Input        : Pointer to Spectral Struct
 * Output       : Void
 *
 */
void spectral_init_dummy_function_table(struct ath_spectral* ps)
{
    SPECTRAL_OPS* p_sops = GET_SPECTRAL_OPS(ps);

    p_sops->get_tsf64               = null_get_tsf64;
    p_sops->get_capability          = null_get_capability;
    p_sops->set_rxfilter            = null_set_rxfilter;
    p_sops->get_rxfilter            = null_get_rxfilter;
    p_sops->is_spectral_enabled     = null_is_spectral_enabled;
    p_sops->is_spectral_active      = null_is_spectral_active;
    p_sops->start_spectral_scan     = null_start_spectral_scan;
    p_sops->stop_spectral_scan      = null_stop_spectral_scan;
    p_sops->get_extension_channel   = null_get_extension_channel;
    p_sops->get_ctl_noisefloor      = null_get_ctl_noisefloor;
    p_sops->get_ext_noisefloor      = null_get_ext_noisefloor;
    p_sops->configure_spectral      = null_configure_spectral;
    p_sops->get_spectral_config     = null_get_spectral_config;
    p_sops->get_ent_spectral_mask   = null_get_ent_spectral_mask;
    p_sops->get_mac_address         = null_get_mac_address;
    p_sops->get_current_channel     = null_get_current_channel;
    p_sops->reset_hw                = null_reset_hw;
    p_sops->get_chain_noise_floor   = null_get_chain_noise_floor;
    p_sops->spectral_process_phyerr = null_spectral_process_phyerr;
}

/*
 * Function     : spectral_init_param_defaults
 * Description  : Initializes Spectral parameter defaults.
 *                It is the caller's responsibility to ensure
 *                that the Spectral parameters structure passed
 *                is valid.
 * Input        : Pointer to Spectral structure
 * Output       : Void
 *
 */
static void spectral_init_param_defaults(struct ath_spectral* spectral)
{
    struct ieee80211com* ic = spectral->ic;
    struct spectral_config *params = &spectral->params;

    params->ss_count = SPECTRAL_SCAN_COUNT_DEFAULT;
    params->ss_period = SPECTRAL_SCAN_PERIOD_GEN_I_DEFAULT;
    params->ss_spectral_pri = SPECTRAL_SCAN_PRIORITY_DEFAULT;
    params->ss_fft_size = SPECTRAL_SCAN_FFT_SIZE_DEFAULT;
    params->ss_gc_ena = SPECTRAL_SCAN_GC_ENA_DEFAULT;
    params->ss_restart_ena = SPECTRAL_SCAN_RESTART_ENA_DEFAULT;
    params->ss_noise_floor_ref = SPECTRAL_SCAN_NOISE_FLOOR_REF_DEFAULT;
    params->ss_init_delay = SPECTRAL_SCAN_INIT_DELAY_DEFAULT;
    params->ss_nb_tone_thr = SPECTRAL_SCAN_NB_TONE_THR_DEFAULT;
    params->ss_str_bin_thr = SPECTRAL_SCAN_STR_BIN_THR_DEFAULT;
    params->ss_wb_rpt_mode = SPECTRAL_SCAN_WB_RPT_MODE_DEFAULT;
    params->ss_rssi_rpt_mode = SPECTRAL_SCAN_RSSI_RPT_MODE_DEFAULT;
    params->ss_rssi_thr = SPECTRAL_SCAN_RSSI_THR_DEFAULT;
    params->ss_pwr_format = SPECTRAL_SCAN_PWR_FORMAT_DEFAULT;
    params->ss_rpt_mode = SPECTRAL_SCAN_RPT_MODE_DEFAULT;
    params->ss_bin_scale = SPECTRAL_SCAN_BIN_SCALE_DEFAULT;
    params->ss_dbm_adj = SPECTRAL_SCAN_DBM_ADJ_DEFAULT;
    params->ss_chn_mask = ieee80211com_get_rx_chainmask(ic);

    params->ss_short_report = SPECTRAL_SCAN_SHORT_REPORT_DEFAULT;
    params->ss_fft_period = SPECTRAL_SCAN_FFT_PERIOD_DEFAULT;
}

/*
 * Function     : spectral_check_hw_capability
 * Description  : Check Hardware capabaility
 * Input        : Pointer to IC
 * Output       : Void
 *
 */
int spectral_check_hw_capability(struct ath_spectral *spectral)
{
    struct spectral_ops* p_sops     = NULL;
    struct spectral_caps* pcap      = NULL;

    int is_spectral_supported = AH_TRUE;

    p_sops      = GET_SPECTRAL_OPS(spectral);
    pcap        = &spectral->capability;

    if (p_sops->get_capability(spectral, HAL_CAP_PHYDIAG) == AH_FALSE) {
        is_spectral_supported = AH_FALSE;
        printk("SPECTRAL : No PHYDIAG support\n");
        return is_spectral_supported;
    } else {
        pcap->phydiag_cap = 1;
    }

    if (p_sops->get_capability(spectral, HAL_CAP_RADAR) == AH_FALSE) {
        is_spectral_supported = AH_FALSE;
        printk("SPECTRAL : No RADAR support\n");
        return is_spectral_supported;
    } else {
        pcap->radar_cap = 1;
    }

    if (p_sops->get_capability(spectral, HAL_CAP_SPECTRAL_SCAN) == AH_FALSE) {
        is_spectral_supported = AH_FALSE;
        printk("SPECTRAL : No SPECTRAL SUPPORT\n");
        return is_spectral_supported;
    } else {
        pcap->spectral_cap = 1;
    }

    if (p_sops->get_capability(spectral, HAL_CAP_ADVNCD_SPECTRAL_SCAN)
            == AH_FALSE) {
        printk("SPECTRAL : No ADVANCED SPECTRAL SUPPORT\n");
    } else {
        pcap->advncd_spectral_cap = 1;
    }

    return is_spectral_supported;
}OS_EXPORT_SYMBOL(spectral_check_hw_capability);


/*
 * Function     : spectral_clear_stats
 * Description  : Clears Spectral related stats structure
 * Input        : Pointer to Spectral Struct
 * Output       : Void
 *
 */
void spectral_clear_stats(struct ath_spectral* spectral)
{
    SPECTRAL_OPS *p_sops = GET_SPECTRAL_OPS(spectral);
    OS_MEMZERO(&spectral->ath_spectral_stats, sizeof (struct spectral_stats));
    spectral->ath_spectral_stats.last_reset_tstamp = p_sops->get_tsf64(spectral);
}

/*
 * Function     : pdev_spectral_init_da
 * Description  : Initializes Spectral DA LMAC module and it's data structures
 * Input        : Pointer to pdev
 * Output       : Pointer to Spectral LMAC private structure
 *
 */
void* pdev_spectral_init_da(struct wlan_objmgr_pdev *pdev)
{
    struct ath_softc_net80211 *scn = NULL;
    struct pdev_osif_priv *osif_priv = NULL;
    struct ath_spectral *spectral = NULL;
    SPECTRAL_OPS *p_sops = NULL;

    osif_priv = wlan_pdev_get_ospriv(pdev);
    scn = (struct ath_softc_net80211 *)osif_priv->legacy_osif_priv;

    spectral = (struct ath_spectral *)qdf_mem_malloc(sizeof(struct ath_spectral));
    if (spectral == NULL) {
        printk("SPECTRAL : Memory allocation failed\n");
        return spectral;
    }

    qdf_mem_zero(spectral, sizeof (struct ath_spectral));

    spectral->ic = &scn->sc_ic;
    /* store pdev in spectral */
    spectral->pdev_obj = pdev;

    /* init the function ptr table */
    spectral_init_dummy_function_table(spectral);

    /* get spectral function table */
    p_sops = GET_SPECTRAL_OPS(spectral);


    /* TODO : Should this be called here of after ath_attach ? */
    if (p_sops->get_capability(spectral, HAL_CAP_PHYDIAG)) {
        printk(KERN_INFO "HAL_CAP_PHYDIAG : Capable\n");
    }

    SPECTRAL_TODO("Need to fix the capablity check for RADAR");
    if (p_sops->get_capability(spectral, HAL_CAP_RADAR)) {
        printk(KERN_INFO "HAL_CAP_RADAR   : Capable\n");
    }

    SPECTRAL_TODO("Need to fix the capablity check for SPECTRAL\n");
    /* TODO : Should this be called here of after ath_attach ? */
    if (p_sops->get_capability(spectral, HAL_CAP_SPECTRAL_SCAN)) {
        printk(KERN_INFO "HAL_CAP_SPECTRAL_SCAN : Capable\n");
    }

    qdf_spinlock_create(&spectral->ath_spectral_lock);
    qdf_spinlock_create(&spectral->noise_pwr_reports_lock);
    spectral_clear_stats(spectral);

    qdf_spinlock_create(&spectral->spectral_skbqlock);
    STAILQ_INIT(&spectral->spectral_skbq);

#ifdef SPECTRAL_USE_NETLINK_SOCKETS
    spectral_init_netlink(spectral);
#endif

    /* Set the default values for spectral parameters */
    spectral_init_param_defaults(spectral);

    return spectral;
}

/*
 * Function     : pdev_spectral_postinit
 * Description  : Initializes Spectral DA LMAC module and it's data structures
 * Input        : Pointer to ath_softc
 * Output       : 0 for success, else failure
 *
 */
int pdev_spectral_postinit(struct ath_softc* sc) {

    int err = 0;
    struct ath_softc_net80211 *scn = NULL;
    struct wlan_objmgr_pdev *pdev = NULL;
    struct ath_spectral *spectral = NULL;
    struct pdev_osif_priv *osif_priv = NULL;
    struct ieee80211com *ic = NULL;

    ic = (struct ieee80211com *)sc->sc_ieee;
    pdev = ic->ic_pdev_obj;
    osif_priv = wlan_pdev_get_ospriv(pdev);
    scn = (struct ath_softc_net80211 *)osif_priv->legacy_osif_priv;
    spectral = get_spectral_handle_from_pdev(pdev);

    /*
     * First initlaize the Spectral table in ATH layer and pass the
     * table to register the same to Spectral table in Spectral
     * module
     */
    sc->sc_spectral = spectral;
    spectral->ath_softc_handle = (void *)sc;
    /* Set non-edma bit in spectral if device does not support edma */
    spectral->sc_spectral_non_edma = (sc->sc_enhanceddmasupport)?0:1;

    scn->sc_ops->ath_init_spectral_ops(&sc->p_sops);
    spectral_register_funcs(spectral, (SPECTRAL_OPS*)&sc->p_sops);
    if (spectral_check_hw_capability(spectral) == AH_FALSE) {
	/*
	 TBD:
	 When the chip does not support spectral (XB112/Jet, ..)
	 only the LMAC spectral object is freed, PDEV spectral object
	 still has a dangling reference to LMAC spectral object.
	 */
        /* pdev_spectral_deinit_da(pdev); */
        err = EIO;
    }
    return err;
}
EXPORT_SYMBOL(pdev_spectral_postinit);

/*
 * Function     : pdev_spectral_deinit_da
 * Description  : De-initialize Spectral DA LMAC module and it's data structures
 * Input        : Pointer to pdev
 * Output       : None
 *
 */
void pdev_spectral_deinit_da(struct wlan_objmgr_pdev *pdev)
{
    struct ath_spectral *spectral = NULL;
    struct ath_softc* sc = NULL;

    spectral = get_spectral_handle_from_pdev(pdev);
    if (spectral == NULL) {
         printk("SPECTRAL : Module dosen't exist\n");
         return;
    }
    sc = (struct ath_softc *)spectral->ath_softc_handle;

#ifdef SPECTRAL_USE_NETLINK_SOCKETS
    spectral_destroy_netlink(spectral);
#endif
    qdf_spinlock_destroy(&spectral->ath_spectral_lock);
    qdf_spinlock_destroy(&spectral->noise_pwr_reports_lock);

    if (spectral) {
         qdf_mem_free(spectral);
         spectral = NULL;
         sc->sc_spectral = NULL;
    }
}


static void init_upper_lower_flags(struct ath_spectral *spectral)
{

    int current_channel = 0;
    int ext_channel = 0;
    SPECTRAL_OPS* p_sops = GET_SPECTRAL_OPS(spectral);

    current_channel = p_sops->get_current_channel(spectral);
    ext_channel     = p_sops->get_extension_channel(spectral);

    if ((current_channel == 0) || (ext_channel == 0)) {
        return;
    }

    if (spectral->sc_spectral_20_40_mode) {
        // HT40 mode
        if (ext_channel < current_channel) {
            spectral->lower_is_extension = 1;
            spectral->upper_is_control   = 1;
            spectral->lower_is_control   = 0;
            spectral->upper_is_extension = 0;
        } else {
            spectral->lower_is_extension = 0;
            spectral->upper_is_control   = 0;
            spectral->lower_is_control   = 1;
            spectral->upper_is_extension = 1;
        }
    } else {
      // HT20 mode, lower is always control
      spectral->lower_is_extension = 0;
      spectral->upper_is_control   = 0;
      spectral->lower_is_control   = 1;
      spectral->upper_is_extension = 0;
    }

}


int get_fft_bin_count(int fft_len)
{
    int bin_count = 0;
    switch(fft_len) {
        case 5:
          bin_count = 16;
          break;
        case 6:
          bin_count = 32;
          break;
        case 7:
          bin_count = 64;
          break;
        case 8:
          bin_count = 128;
          break;
        case 9:
          bin_count = 256;
          break;
        default:
          break;
    }

    return bin_count;
}

/*
 * Function     : spectral_scan_enable_params
 * Description  : Program Spectral Scan Params
 * Input        : Pointer to Spectral Struct and struct spectral_config struct
 * Output       : Success/Failure
 *
 */
int spectral_scan_enable_params(struct ath_spectral *spectral, struct spectral_config* spectral_params)
{
    u_int32_t rfilt         = 0;
    int extension_channel   = 0;
    int current_channel     = 0;
    SPECTRAL_OPS* p_sops    = NULL;
    struct ieee80211com* ic = NULL;
    struct ath_softc* sc    = NULL;

    if (spectral == NULL) {
        printk("SPECTRAL : Spectral is NULL\n");
        return 1;
    }

    sc = GET_SPECTRAL_ATHSOFTC(spectral);
    ic = spectral->ic;
    p_sops = GET_SPECTRAL_OPS(spectral);

    if (p_sops == NULL) {
        printk("SPECTRAL : p_sops is NULL\n");
        return 1;
    }

    /* get the receive filters */
    rfilt = p_sops->get_rxfilter(spectral);
    rfilt &= ~HAL_RX_FILTER_PHYRADAR;

    p_sops->set_rxfilter(spectral, rfilt);
    rfilt |= HAL_RX_FILTER_PHYRADAR;
    p_sops->set_rxfilter(spectral, rfilt);

    spectral->sc_spectral_noise_pwr_cal = spectral_params->ss_spectral_pri ? 1 : 0;


    /* check if extension channel is present */
    extension_channel   = p_sops->get_extension_channel(spectral);
    current_channel     = p_sops->get_current_channel(spectral);
    spectral->ch_width  = ic->ic_cwm_get_width(ic);

    if (spectral->capability.advncd_spectral_cap) {
        spectral->lb_edge_extrabins = 0;
        spectral->rb_edge_extrabins = 0;

        if (spectral->is_lb_edge_extrabins_format &&
            spectral->params.ss_rpt_mode == 2) {
           spectral->lb_edge_extrabins = 4;
        }

        if (spectral->is_rb_edge_extrabins_format &&
            spectral->params.ss_rpt_mode == 2) {
           spectral->rb_edge_extrabins = 4;
        }

        if (spectral->ch_width == IEEE80211_CWM_WIDTH20) {
            //printk("SPECTRAL : (11AC) 20MHz Channel Width (Channel = %d)\n", current_channel);
            spectral->sc_spectral_20_40_mode    = 0;

            spectral->spectral_numbins  = get_fft_bin_count(spectral->params.ss_fft_size);
            spectral->spectral_fft_len  = get_fft_bin_count(spectral->params.ss_fft_size);
            spectral->spectral_data_len = get_fft_bin_count(spectral->params.ss_fft_size);

            /* Initialize classifier params to be sent to user space classifier */
            spectral->classifier_params.lower_chan_in_mhz = current_channel;
            spectral->classifier_params.upper_chan_in_mhz = 0;

        } else if (spectral->ch_width == IEEE80211_CWM_WIDTH40) {
            //printk("SPECTRAL : (11AC) 40MHz Channel Width (Channel = %d)\n", current_channel);
            spectral->sc_spectral_20_40_mode    = 1;    // TODO : Remove this variable

            spectral->spectral_numbins  = get_fft_bin_count(spectral->params.ss_fft_size);
            spectral->spectral_fft_len  = get_fft_bin_count(spectral->params.ss_fft_size);
            spectral->spectral_data_len = get_fft_bin_count(spectral->params.ss_fft_size);

            /* Initialize classifier params to be sent to user space classifier */
            if (extension_channel < current_channel) {
                 spectral->classifier_params.lower_chan_in_mhz = extension_channel;
                 spectral->classifier_params.upper_chan_in_mhz = current_channel;
            } else {
                 spectral->classifier_params.lower_chan_in_mhz = current_channel;
                 spectral->classifier_params.upper_chan_in_mhz = extension_channel;
            }

        } else if (spectral->ch_width == IEEE80211_CWM_WIDTH80) {
            //printk("SPECTRAL : (11AC) 80MHz Channel Width (Channel = %d)\n", current_channel);
            /* Set the FFT Size */
            spectral->sc_spectral_20_40_mode    = 0;    // TODO : Remove this variable
            spectral->spectral_numbins  = get_fft_bin_count(spectral->params.ss_fft_size);
            spectral->spectral_fft_len  = get_fft_bin_count(spectral->params.ss_fft_size);
            spectral->spectral_data_len = get_fft_bin_count(spectral->params.ss_fft_size);

            /* Initialize classifier params to be sent to user space classifier */
            spectral->classifier_params.lower_chan_in_mhz = current_channel;
            spectral->classifier_params.upper_chan_in_mhz = 0;

            /* Initialize classifier params to be sent to user space classifier */
            if (extension_channel < current_channel) {
                 spectral->classifier_params.lower_chan_in_mhz = extension_channel;
                 spectral->classifier_params.upper_chan_in_mhz = current_channel;
            } else {
                 spectral->classifier_params.lower_chan_in_mhz = current_channel;
                 spectral->classifier_params.upper_chan_in_mhz = extension_channel;
            }

        } else if (spectral->ch_width == IEEE80211_CWM_WIDTH160) {
            //printk("SPECTRAL : (11AC) 160MHz Channel Width (Channel = %d)\n", current_channel);
            /* Set the FFT Size */

            /* The below applies to both 160 and 80+80 cases */

            spectral->sc_spectral_20_40_mode    = 0;    // TODO : Remove this variable
            spectral->spectral_numbins  = get_fft_bin_count(spectral->params.ss_fft_size);
            spectral->spectral_fft_len  = get_fft_bin_count(spectral->params.ss_fft_size);
            spectral->spectral_data_len = get_fft_bin_count(spectral->params.ss_fft_size);

            /* Initialize classifier params to be sent to user space classifier */
            spectral->classifier_params.lower_chan_in_mhz = current_channel;
            spectral->classifier_params.upper_chan_in_mhz = 0;

            /* Initialize classifier params to be sent to user space classifier */
            if (extension_channel < current_channel) {
                 spectral->classifier_params.lower_chan_in_mhz = extension_channel;
                 spectral->classifier_params.upper_chan_in_mhz = current_channel;
            } else {
                 spectral->classifier_params.lower_chan_in_mhz = current_channel;
                 spectral->classifier_params.upper_chan_in_mhz = extension_channel;
            }
        }

        if (spectral->spectral_numbins) {
            spectral->spectral_numbins +=  spectral->lb_edge_extrabins;
            spectral->spectral_numbins +=  spectral->rb_edge_extrabins;
        }

        if (spectral->spectral_fft_len) {
            spectral->spectral_fft_len +=  spectral->lb_edge_extrabins;
            spectral->spectral_fft_len +=  spectral->rb_edge_extrabins;
        }

        if (spectral->spectral_data_len) {
            spectral->spectral_data_len +=  spectral->lb_edge_extrabins;
            spectral->spectral_data_len +=  spectral->rb_edge_extrabins;
        }
    } else {
        //printk("SPECTRAL : Legacy (Non-11AC)\n");
        /*
         * The decision to find 20/40 mode is found based on the presence of extension channel
         * instead of channel width, as the channel width can dynamically change
         */

        if (extension_channel == 0) {
            //printk("SPECTRAL : (Legacy) 20MHz Channel Width (Channel = %d)\n", current_channel);
            spectral->spectral_numbins                  = SPECTRAL_HT20_NUM_BINS;
            spectral->spectral_dc_index                 = SPECTRAL_HT20_DC_INDEX;
            spectral->spectral_fft_len                  = SPECTRAL_HT20_FFT_LEN;
            spectral->spectral_data_len                 = SPECTRAL_HT20_TOTAL_DATA_LEN;
            spectral->spectral_lower_max_index_offset   = -1; // only valid in 20-40 mode
            spectral->spectral_upper_max_index_offset   = -1; // only valid in 20-40 mode
            spectral->spectral_max_index_offset         = spectral->spectral_fft_len + 2;
            spectral->sc_spectral_20_40_mode            = 0;

            /* Initialize classifier params to be sent to user space classifier */
            spectral->classifier_params.lower_chan_in_mhz = current_channel;
            spectral->classifier_params.upper_chan_in_mhz = 0;

        } else {
            //printk("SPECTRAL : (Legacy) 40MHz Channel Width (Channel = %d)\n", current_channel);
            spectral->spectral_numbins                  = SPECTRAL_HT40_TOTAL_NUM_BINS;
            spectral->spectral_fft_len                  = SPECTRAL_HT40_FFT_LEN;
            spectral->spectral_data_len                 = SPECTRAL_HT40_TOTAL_DATA_LEN;
            spectral->spectral_dc_index                 = SPECTRAL_HT40_DC_INDEX;
            spectral->spectral_max_index_offset         = -1; //only valid in 20 mode
            spectral->spectral_lower_max_index_offset   = spectral->spectral_fft_len + 2;
            spectral->spectral_upper_max_index_offset   = spectral->spectral_fft_len + 5;
            spectral->sc_spectral_20_40_mode            = 1;

            /* Initialize classifier params to be sent to user space classifier */
            if (extension_channel < current_channel) {
                 spectral->classifier_params.lower_chan_in_mhz = extension_channel;
                 spectral->classifier_params.upper_chan_in_mhz = current_channel;
            } else {
                 spectral->classifier_params.lower_chan_in_mhz = current_channel;
                 spectral->classifier_params.upper_chan_in_mhz = extension_channel;
            }
        }
    }

    spectral->send_single_packet                    = 0;
    spectral->classifier_params.spectral_20_40_mode = spectral->sc_spectral_20_40_mode;
    spectral->classifier_params.spectral_dc_index   = spectral->spectral_dc_index;
    spectral->spectral_sent_msg                     = 0;
    spectral->classify_scan                         = 0;
    spectral->num_spectral_data                     = 0;

     if (!p_sops->is_spectral_active(spectral)) {
        p_sops->configure_spectral(spectral, spectral_params);
        p_sops->start_spectral_scan(spectral);
        //printk("Enabled spectral scan on channel %d\n", p_sops->get_current_channel(spectral));
    } else {
        //printk("Spectral scan is already ACTIVE on channel %d\n", p_sops->get_current_channel(spectral));
    }

    /* get current spectral configuration */
    p_sops->get_spectral_config(spectral, &spectral->params);

    init_upper_lower_flags(spectral);

    /* Checking SC ptr, For offload path SC ptr is null */
    if((sc != NULL) && (sc->sc_ieee_ops != NULL) &&
       (sc->sc_ieee_ops->spectral_init_chan_loading)) {
       sc->sc_ieee_ops->spectral_init_chan_loading(sc->sc_ieee,
                                                   current_channel, extension_channel);
    }

#ifdef SPECTRAL_CLASSIFIER_IN_KERNEL
    init_classifier(sc);
#endif
    return 0;
}

/*
 * Function     : spectral_scan_enable
 * Description  : Enable Spectral Scan
 * Input        : Pointer to Spectral Struct
 * Output       : Success/Failure
 *
 */
int spectral_scan_enable(struct ath_spectral* spectral, u_int8_t priority)
{
    spectral_init_param_defaults(spectral);

    if(priority == 0) {
        spectral->params.ss_spectral_pri = 0;
    }
    else {
        spectral->params.ss_spectral_pri = 1;
    }
    return spectral_scan_enable_params(spectral, &spectral->params);
}

/* Get channel information from channel scan */
void spectral_record_chan_info(struct ath_spectral *spectral,
                               u_int16_t chan_num,
                               bool are_chancnts_valid,
                               u_int32_t scanend_clr_cnt,
                               u_int32_t scanstart_clr_cnt,
                               u_int32_t scanend_cycle_cnt,
                               u_int32_t scanstart_cycle_cnt,
                               bool is_nf_valid,
                               int16_t nf,
                               bool is_per_valid,
                               u_int32_t per)
{
    SPECTRAL_OPS* p_sops = NULL;
    u_int32_t clear_cnt, cycle_cnt, shift_fact;
    u_int32_t chan_load = ((1<<16)-1);

    if ((chan_num >= MAX_NUM_CHANNELS) || (spectral == NULL)) {
        return;
    }

    p_sops = GET_SPECTRAL_OPS(spectral);

    if (p_sops == NULL) {
        return;
    }

    if (are_chancnts_valid == true) {
        /* Calculate channel load */

        clear_cnt = scanend_clr_cnt - scanstart_clr_cnt;
        cycle_cnt = scanend_cycle_cnt - scanstart_cycle_cnt;

        /* The count could have overflowed, so it is important
           to check for this condition and correct for the same. */
        if (scanend_clr_cnt < scanstart_clr_cnt) {
            clear_cnt += 1 << 31;
        }
        if (scanend_cycle_cnt < scanstart_cycle_cnt) {
            cycle_cnt += 1 << 31;
        }

        /* Take only the 16 significant bits */
        shift_fact = 16;
        if (cycle_cnt) {
            while ((cycle_cnt >= (1 << shift_fact)) && (shift_fact < 32))
            {
                shift_fact++;
            }
        }
        shift_fact -= 16;

        clear_cnt >>= shift_fact;
        cycle_cnt >>= shift_fact;

        if (cycle_cnt) {
            chan_load -= (clear_cnt *((1 << 16) - 1)) / cycle_cnt;
        } else {
            /* Cycle count cannot be zero. But if so, set the chan_load to
               zero */
            chan_load = 0;
        }
    } else {
        /* We haven't been passed Rx clear counts */
        chan_load = 0;
    }

    spectral->chaninfo[chan_num].channel_load = chan_load;
    if (is_nf_valid == true) {
        spectral->chaninfo[chan_num].noisefloor = nf;
    } else {
        spectral->chaninfo[chan_num].noisefloor = p_sops->get_ctl_noisefloor(spectral);
    }

    if (is_per_valid == true) {
        spectral->chaninfo[chan_num].per = per;
    }

    spectral->chaninfo[chan_num].cycle_count = 0;
}

/*
 * Function     : spectral_set_thresholds
 * Description  : Set SPECTRAL thresholds
 * Input        : Pointer to pdev, threshold type, value
 * Output       : Success/Failure
 *
 */
int spectral_set_thresholds(struct wlan_objmgr_pdev *pdev,
                            const u_int32_t threshtype,
                            const u_int32_t value)
{
    struct spectral_config params;
    SPECTRAL_OPS* p_sops = NULL;
    struct ath_spectral *spectral = NULL;

    spectral = get_spectral_handle_from_pdev(pdev);
    p_sops = GET_SPECTRAL_OPS(spectral);
    if (spectral == NULL) {
        printk(  "%s: sc_spectral is NULL\n", __func__);
        return 0;
    }

    switch (threshtype) {
        case SPECTRAL_PARAM_FFT_PERIOD:
            spectral->params.ss_fft_period = value;
            break;
        case SPECTRAL_PARAM_SCAN_PERIOD:
            spectral->params.ss_period = value;
            break;
        case SPECTRAL_PARAM_SCAN_COUNT:
            spectral->params.ss_count = value;
            break;
        case SPECTRAL_PARAM_SHORT_REPORT:
            spectral->params.ss_short_report = (!!value) ? true:false;
            break;
        case SPECTRAL_PARAM_SPECT_PRI:
            spectral->params.ss_spectral_pri = (!!value)? true:false;
            break;
        case SPECTRAL_PARAM_FFT_SIZE:
            spectral->params.ss_fft_size = value;
            break;
        case SPECTRAL_PARAM_GC_ENA:
            spectral->params.ss_gc_ena = !!value;
            break;
        case SPECTRAL_PARAM_RESTART_ENA:
            spectral->params.ss_restart_ena = !!value;
            break;
        case SPECTRAL_PARAM_NOISE_FLOOR_REF:
            spectral->params.ss_noise_floor_ref = value;
            break;
        case SPECTRAL_PARAM_INIT_DELAY:
            spectral->params.ss_init_delay = value;
            break;
        case SPECTRAL_PARAM_NB_TONE_THR:
            spectral->params.ss_nb_tone_thr = value;
            break;
        case SPECTRAL_PARAM_STR_BIN_THR:
            spectral->params.ss_str_bin_thr = value;
            break;
        case SPECTRAL_PARAM_WB_RPT_MODE:
            spectral->params.ss_wb_rpt_mode = !!value;
            break;
        case SPECTRAL_PARAM_RSSI_RPT_MODE:
            spectral->params.ss_rssi_rpt_mode = !!value;
            break;
        case SPECTRAL_PARAM_RSSI_THR:
            spectral->params.ss_rssi_thr = value;
            break;
        case SPECTRAL_PARAM_PWR_FORMAT:
            spectral->params.ss_pwr_format = !!value;
            break;
        case SPECTRAL_PARAM_RPT_MODE:
            spectral->params.ss_rpt_mode = value;
            break;
        case SPECTRAL_PARAM_BIN_SCALE:
            spectral->params.ss_bin_scale = value;
            break;
        case SPECTRAL_PARAM_DBM_ADJ:
            spectral->params.ss_dbm_adj = !!value;
            break;
        case SPECTRAL_PARAM_CHN_MASK:
            spectral->params.ss_chn_mask = value;
            break;
    }

    p_sops->configure_spectral(spectral, &spectral->params);
    p_sops->get_spectral_config(spectral, &params); /* only to validate the writes */
    //print_spectral_params(&spectral->params);
    return 1;
}

/*
 * Function     : spectral_get_thresholds
 * Description  : Get SPECTRAL thresholds
 * Input        : Pointer to pdev, threshold type, value
 * Output       : Success/Failure
 *
 */
void spectral_get_thresholds(struct wlan_objmgr_pdev *pdev, struct spectral_config *param)
{
    SPECTRAL_OPS* p_sops = NULL;
    struct ath_spectral *spectral = NULL;

    spectral = get_spectral_handle_from_pdev(pdev);
    p_sops = GET_SPECTRAL_OPS(spectral);

    OS_MEMZERO(param, sizeof (struct spectral_config));
    p_sops->get_spectral_config(spectral, param);
}

static pwr_dBm spectral_get_median_pwr(pwr_dBm arr[], int length)
{
    int i, j;
    pwr_dBm tmp;
    for (i = 1; i < length; i++) {
        j = i;
        while (j > 0 && arr[j - 1] > arr[j]) {
            tmp = arr[j];
            arr[j] = arr[j - 1];
            arr[j - 1] = tmp;
            j--;
        }
    }
    return((length & 1) ? arr[length >> 1] : ((arr[(length >> 1) - 1] + arr[length >> 1]) >> 1));
}

/*
 * Function     : spectral_get_noise_power
 * Description  : Get SPECTRAL noise power
 * Input        : Pointer to Spectral Struct, threshold type, value
 * Output       : Success/Failure
 *
 */
#define MAX_NOISE_PWR_WAIT 20000
int spectral_get_noise_power(struct ath_spectral *spectral,
                             int rptcount,                  /* number of noise pwr reports required */
                             NOISE_PWR_CAL* cal_override,   /* cal debug override - may be NULL */
                             CHAIN_NOISE_PWR_INFO* ctl_c0,  /* reports for chain 0 control */
                             CHAIN_NOISE_PWR_INFO* ctl_c1,  /* reports for chain 1 control */
                             CHAIN_NOISE_PWR_INFO* ctl_c2,  /* reports for chain 2 control */
                             CHAIN_NOISE_PWR_INFO* ext_c0,  /* reports for chain 0 ext */
                             CHAIN_NOISE_PWR_INFO* ext_c1,  /* reports for chain 1 ext */
                             CHAIN_NOISE_PWR_INFO* ext_c2)  /* reports for chain 2 ext */
{

    int ch, i, j, error = 0, idx, val;
    struct spectral_config params;
    CHAIN_NOISE_PWR_INFO* ns;
    int rssi_total[ATH_HOST_MAX_ANTENNA*2];
    //pwr_dBm un_cal;
    struct ieee80211com* ic = spectral->ic;
    pwr_dBm *tmp_arr = NULL;
    int16_t nfBuf[ATH_HOST_MAX_ANTENNA*2];
    SPECTRAL_OPS* p_sops = GET_SPECTRAL_OPS(spectral);

    if (rptcount < 1 || rptcount > MAX_NOISE_PWR_REPORTS) {
        error = EINVAL;
        printk(
                         "%s[%d]: error=%d\n", __func__, __LINE__, error);
        return error;
    }

    if (spectral->noise_pwr_reports_reqd || spectral->noise_pwr_reports_recv) {
        error = EINPROGRESS;
        printk(
                         "%s[%d]: error=%d\n", __func__, __LINE__, error);
        return error;
    }

    tmp_arr = (pwr_dBm *)OS_MALLOC(ic->ic_osdev, sizeof(char) * MAX_NOISE_PWR_REPORTS, GFP_KERNEL);

    if (tmp_arr == NULL) {
        printk("SPECTRAL : Memory allocation failed\n");
        return error;
    }

    OS_MEMZERO(tmp_arr, sizeof (char) * MAX_NOISE_PWR_REPORTS);

    if(p_sops->is_spectral_active(spectral)) {
        printk("%s[%d]: spectral already active - cancel\n", __func__, __LINE__);
        SPECTRAL_LOCK(spectral);
        p_sops->stop_spectral_scan(spectral);
        spectral->sc_spectral_scan = 0;
        SPECTRAL_UNLOCK(spectral);
    }

    qdf_spin_lock(&spectral->noise_pwr_reports_lock);
    spectral->noise_pwr_reports_reqd = rptcount;
    spectral->noise_pwr_reports_recv = 0;
    spectral->noise_pwr_chain_ctl[0] = ctl_c0;
    spectral->noise_pwr_chain_ctl[1] = ctl_c1;
    spectral->noise_pwr_chain_ctl[2] = ctl_c2;
    spectral->noise_pwr_chain_ext[0] = ext_c0;
    spectral->noise_pwr_chain_ext[1] = ext_c1;
    spectral->noise_pwr_chain_ext[2] = ext_c2;
    /* init report counts */
    for(i = 0; i < ATH_HOST_MAX_ANTENNA; i++) {
        if (spectral->noise_pwr_chain_ctl[i]) {
            spectral->noise_pwr_chain_ctl[i]->rptcount = 0;
        }
        if (spectral->noise_pwr_chain_ext[i]) {
            spectral->noise_pwr_chain_ext[i]->rptcount = 0;
        }

#ifndef ATH_PERF_PWR_OFFLOAD
        if ((spectral->ath_softc_handle->sc_rx_chainmask & (1 << i)) == 0) {
            spectral->noise_pwr_chain_ctl[i] = spectral->noise_pwr_chain_ext[i] = NULL;
        }
        if (!(spectral->ath_softc_handle->sc_curchan.channel_flags & (CHANNEL_HT40PLUS | CHANNEL_HT40MINUS))) {
            spectral->noise_pwr_chain_ext[i] = NULL;
        }
#else   // ATH_PERF_PWR_OFFLOAD
        not_yet_implemented();
#endif  // ATH_PERF_PWR_OFFLOAD
    }
    qdf_spin_unlock(&spectral->noise_pwr_reports_lock);

    /* get the lastest noisefloor reading */
    p_sops->get_chain_noise_floor(spectral, nfBuf);

    /* Get current spectral parameters */
    spectral_get_thresholds(spectral->pdev_obj, &params);
    for(i = 0; i < 2; i++) {
        for(ch = 0; ch < ATH_HOST_MAX_ANTENNA; ch++) {
            idx = ch + (i*ATH_HOST_MAX_ANTENNA);
            if (cal_override && (cal_override->valid_chain_mask & (1 << ch)) != 0) {
                params.ss_nf_cal[idx] = cal_override->chain[ch].cal;
                params.ss_nf_pwr[idx] = cal_override->chain[ch].pwr;
            }
            if (params.ss_nf_cal[idx] & 3) {
                /* adjust dBm by any fractional dBr amount */
                params.ss_nf_pwr[idx] += (params.ss_nf_cal[idx] & 3);
                params.ss_nf_cal[idx] &= ~3;
            }
#ifndef ATH_PERF_PWR_OFFLOAD
            if ((sc->sc_rx_chainmask & (1 << ch)) &&
                idx < sizeof(params.ss_nf_cal)/sizeof(params.ss_nf_cal[0]))
            {
                un_cal = nfBuf[idx] + params.ss_nf_pwr[idx] - params.ss_nf_cal[idx];
                if (!i || (sc->sc_curchan.channel_flags & (CHANNEL_HT40PLUS | CHANNEL_HT40MINUS))) {
                    printk(
                             "eeprom nf chan=%d chain%d%s: un-cal=%4d.%02d dBm   cal=%4d.%02d dBr   pwr=%4d.%02d dBm\n",
                             sc->sc_curchan.channel, ch, i ? "ext" : "ctl",
                             NOISE_PWR_DBM_2_INT(un_cal),                  NOISE_PWR_DBM_2_DEC(un_cal),
                             NOISE_PWR_DBM_2_INT(params.ss_nf_cal[idx]),   NOISE_PWR_DBM_2_DEC(params.ss_nf_cal[idx]),
                             NOISE_PWR_DBM_2_INT(params.ss_nf_pwr[idx]),   NOISE_PWR_DBM_2_DEC(params.ss_nf_pwr[idx]));
                }
            }
#else   // ATH_PERF_PWR_OFFLOAD
            not_yet_implemented();
#endif  // ATH_PERF_PWR_OFFLOAD
        }
    }

    /* override to configure hardware */
    spectral_init_param_defaults(spectral);

    params.ss_spectral_pri = 1;

#ifndef ATH_PERF_PWR_OFFLOAD
    spectral->ath_softc_handle->sc_scanning = 1; // dummy a scan to prevent ath_calibrate
#endif // ATH_PERF_PWR_OFFLOAD

    SPECTRAL_LOCK(spectral);
    spectral->scan_start_tstamp = p_sops->get_tsf64(spectral);
    error = spectral_scan_enable_params(spectral, &params);
    spectral->sc_spectral_scan = 1;
    SPECTRAL_UNLOCK(spectral);

    if (error) {
        SPECTRAL_LOCK(spectral);
        spectral->sc_spectral_scan = 0;
        SPECTRAL_UNLOCK(spectral);
        printk("%s[%d]: error=%d\n", __func__, __LINE__, error);
    }
    else {
        /* wait for ss to complete */
        for (i = 0; i < MAX_NOISE_PWR_WAIT; i++) {
            if (!p_sops->is_spectral_active(spectral)) {
                printk("%s[%d]: noise pwr done - reports_recv=%d\n", __func__, __LINE__, spectral->noise_pwr_reports_recv);
                break;
            }
            OS_DELAY(100);
        }
        /* did we timeout ? */
        if (i == MAX_NOISE_PWR_WAIT) {
            SPECTRAL_LOCK(spectral);
            stop_current_scan_da(spectral->pdev_obj);
            spectral->sc_spectral_scan = 0;
            SPECTRAL_UNLOCK(spectral);
            error = EBUSY;
        }
    }

    qdf_spin_lock(&spectral->noise_pwr_reports_lock);

    if (!error && spectral->noise_pwr_reports_recv < spectral->noise_pwr_reports_reqd) {
        error = EAGAIN;
        printk("%s[%d]: noise pwr done but only %d/%d reports_recv\n", __func__, __LINE__, spectral->noise_pwr_reports_recv,
                         spectral->noise_pwr_reports_reqd);
    }

    /*
     * for both ctl and ext:
     * - fill in factory nf pwr from eeprom and un-cal nf
     * - add nf to rssi reports to get pwr
     * - calc mean pwr
    */
    if (!error) {
        OS_MEMZERO(rssi_total, sizeof(rssi_total));
        for(i = 0; i < 2; i++) {
            for(ch = 0; ch < ATH_HOST_MAX_ANTENNA; ch++) {
                ns = (i == 0) ? spectral->noise_pwr_chain_ctl[ch] : spectral->noise_pwr_chain_ext[ch];
                idx = ch + (i*ATH_HOST_MAX_ANTENNA);
                if (ns && ns->rptcount) {
                    ns->un_cal_nf = nfBuf[idx] + params.ss_nf_pwr[idx] - params.ss_nf_cal[idx];
                    ns->factory_cal_nf = params.ss_nf_pwr[idx];
                    rssi_total[idx] = 0;
                    for(j = 0; j < ns->rptcount; j++) {
                        rssi_total[idx] += ns->pwr[j];
                        val = INT_2_NOISE_PWR_DBM(ns->pwr[j] + (ns->factory_cal_nf >> 2) + NOISE_PWR_DATA_OFFSET)
                              + (ns->factory_cal_nf & 3);
                        // clamp under/overflows to range
                        val = (val < -128) ? -128 : ((val > 127) ? 127 : val);
                        ns->pwr[j] = val;
                    }
                    OS_MEMCPY(tmp_arr, ns->pwr, ns->rptcount);
                    ns->median_pwr = spectral_get_median_pwr(tmp_arr, ns->rptcount);
                    rssi_total[idx] = (rssi_total[idx] << 2) / ns->rptcount;
                }
            }
        }
    }

    /* reset/cleanup */
    OS_MEMZERO(spectral->noise_pwr_chain_ctl, sizeof(spectral->noise_pwr_chain_ctl));
    OS_MEMZERO(spectral->noise_pwr_chain_ext, sizeof(spectral->noise_pwr_chain_ext));
    spectral->noise_pwr_reports_reqd = spectral->noise_pwr_reports_recv = 0;

    qdf_spin_unlock(&spectral->noise_pwr_reports_lock);

    if (!error) {
        printk("%s", "rssi ave: ");
        for(i = 0; i < ATH_HOST_MAX_ANTENNA; i++) {
            printk(  "%3d.%02d ",
                (rssi_total[i] < 0) ? ((rssi_total[i]+3)>>2) : rssi_total[i]>>2,
                (rssi_total[i]&3)*25);
        }
        printk(  "%s", " |");
        for(i = ATH_HOST_MAX_ANTENNA; i < ATH_HOST_MAX_ANTENNA*2; i++) {
            printk(  "%3d.%02d ",
                (rssi_total[i] < 0) ? ((rssi_total[i]+3)>>2) : rssi_total[i]>>2,
                (rssi_total[i]&3)*25);
        }
        printk(  "%s", "\n");
        printk(  "Temp data: %d [0x%x]\n",
                         params.ss_nf_temp_data, params.ss_nf_temp_data);
    }

#ifndef ATH_PERF_PWR_OFFLOAD
    spectral->ath_softc_handle->sc_scanning = 0;
#endif // ATH_PERF_PWR_OFFLOAD

    if (error == EBUSY) {
        /* force a full reset to restore normal operation */
        printk(  "%s[%d]: TIME_OUT: force reset on exit\n", __func__, __LINE__);
        p_sops->reset_hw(spectral);
    }

    if (error) {
      printk(  "%s[%d]: error=%d\n", __func__, __LINE__, error);
    }

    if (tmp_arr) {
        OS_FREE(tmp_arr);
    }

    return error;
}


/*
 * Function     : spectral_send_intf_found_msg
 * Description  : Send EACS Message
 * Input        : Pointer to Spectral Struct
 * Output       : Success/Failure
 *
 */
#ifdef HOST_OFFLOAD
extern void
atd_spectral_msg_send(struct net_device *dev, struct spectral_samp_msg *msg, uint16_t msg_len);
#endif

void spectral_send_intf_found_msg(struct ath_spectral* spectral, u_int16_t cw_int, u_int32_t dcs_enabled)
{
#ifdef SPECTRAL_USE_NETLINK_SOCKETS
    struct spectral_samp_msg *msg = NULL;
    SPECTRAL_OPS* p_sops = GET_SPECTRAL_OPS(spectral);
    spectral_prep_skb(spectral);
    if (spectral->spectral_skb != NULL) {
         spectral->spectral_nlh = (struct nlmsghdr*)spectral->spectral_skb->data;
         msg = (struct spectral_samp_msg*) NLMSG_DATA(spectral->spectral_nlh);
         msg->int_type = cw_int ? SPECTRAL_DCS_INT_CW : SPECTRAL_DCS_INT_WIFI;
         msg->dcs_enabled = dcs_enabled;
         msg->signature = SPECTRAL_SIGNATURE;
         p_sops->get_mac_address(spectral, msg->macaddr);
         spectral_bcast_msg(spectral);
    }
#endif
#ifdef HOST_OFFLOAD
    {
        struct spectral_samp_msg *buf = NULL;

        buf = (struct spectral_samp_msg *)OS_MALLOC(spectral->ic->ic_osdev,
                sizeof(struct ath_spectral),
                GFP_KERNEL);
        buf->int_type = cw_int ? SPECTRAL_DCS_INT_CW : SPECTRAL_DCS_INT_WIFI;
        buf->dcs_enabled = dcs_enabled;
        buf->signature = SPECTRAL_SIGNATURE;
        p_sops->get_mac_address(spectral, buf->macaddr);
        atd_spectral_msg_send(spectral->ic->ic_osdev->netdev,
                buf,
                sizeof(struct spectral_samp_msg));
        OS_FREE(buf);
    }
#endif
}

#define MIN_SPECTRAL_DATA_SIZE (10)

/*
 * Function     : is_spectral_phyerr
 * Description  : Check for PHY ERR
 * Input        : Pointer to Spectral Struct
 * Output       : Success/Failure
 *
 */
int is_spectral_phyerr(struct ath_spectral* spectral,
                      struct ath_buf *bf,
                      struct ath_rx_status *rxs)
{

    u_int8_t pulse_bw_info = 0;
    u_int8_t *byte_ptr;
    u_int8_t last_byte_0;
    u_int8_t last_byte_1;
    u_int8_t last_byte_2;
    u_int8_t last_byte_3;
    u_int8_t secondlast_byte_0;
    u_int8_t secondlast_byte_1;
    u_int8_t secondlast_byte_2;
    u_int8_t secondlast_byte_3;
    u_int8_t *dataPtr;
    u_int16_t datalen;

    u_int32_t *last_word_ptr;
    u_int32_t *secondlast_word_ptr;

    if ((!(rxs->rs_phyerr == HAL_PHYERR_RADAR)) &&
           (!(rxs->rs_phyerr == HAL_PHYERR_FALSE_RADAR_EXT)) &&
           (!(rxs->rs_phyerr == HAL_PHYERR_SPECTRAL))) {
        return AH_FALSE;
    }

    if (!rxs->rs_datalen) {
        return AH_FALSE;
    }

    if (!spectral) {
        return AH_FALSE;
    }

    // If spectral scan is not started, just drop this packet.
    if (!(spectral->sc_spectral_scan || spectral->sc_spectral_full_scan)) {
        return AH_FALSE;
    }

    //FIXME. When datalen is less than 2 words, following process will underrun.
    if ( rxs->rs_datalen < MIN_SPECTRAL_DATA_SIZE ) {
        return AH_FALSE;
    }

    /* This is the case for Kiwi and later chips, where we do not need
     * the bandwidth info to make sure it is a spectral data */
    if( rxs->rs_phyerr == HAL_PHYERR_SPECTRAL ) return AH_TRUE;

    datalen = rxs->rs_datalen;
    dataPtr = bf->bf_vdata;
    last_word_ptr = (u_int32_t *)(dataPtr + datalen - (datalen % 4));
    secondlast_word_ptr = last_word_ptr - 1;

    byte_ptr = (u_int8_t *)last_word_ptr;
    last_byte_0 = (*(byte_ptr) & 0xff);
    last_byte_1 = (*(byte_ptr + 1) & 0xff);
    last_byte_2 = (*(byte_ptr + 2) & 0xff);
    last_byte_3 = (*(byte_ptr + 3) & 0xff);

    byte_ptr = (u_int8_t*)secondlast_word_ptr;
    secondlast_byte_0 = (*(byte_ptr) & 0xff);
    secondlast_byte_1 = (*(byte_ptr + 1) & 0xff);
    secondlast_byte_2 = (*(byte_ptr + 2) & 0xff);
    secondlast_byte_3 = (*(byte_ptr + 3) & 0xff);

    /* extract the bwinfo */
    switch ((datalen & 0x3))  {
        case 0:
            pulse_bw_info = secondlast_byte_3;
            break;
        case 1:
            pulse_bw_info = last_byte_0;
            break;
        case 2:
            pulse_bw_info = last_byte_1;
            break;
        case 3:
            pulse_bw_info = last_byte_2;
            break;
    }

    /* the 5lsbs contain information regarding the source of data */
    pulse_bw_info &= 0x10; //SPECTRAL_SCAN_BITMASK;
    return (pulse_bw_info ? AH_TRUE:AH_FALSE);
}


/*
 * Function     : get_nfc_ext_rssi
 * Description  : Get Noisefloor compensated control channel RSSI
 * Input        : Pointer to Spectral Struct, rssi, CTL noisefloor
 * Output       : Noisefloor
 *
 */
int8_t get_nfc_ctl_rssi(struct ath_spectral* spectral, int8_t rssi, int8_t *ctl_nf)
{
    int8_t temp;
    int16_t nf = -110;
    SPECTRAL_OPS* p_sops = GET_SPECTRAL_OPS(spectral);
    *ctl_nf = -110;

    nf = p_sops->get_ctl_noisefloor(spectral);
    temp = 110 + (nf) + (rssi);
    *ctl_nf = nf;
    return temp;
}

/*
 * Function     : get_nfc_ext_rssi
 * Description  : Get Noisefloor compensated extension channel RSSI
 * Input        : Pointer to Spectral Struct, rssi, CTL noisefloor
 * Output       : Noisefloor
 *
 */
int8_t get_nfc_ext_rssi(struct ath_spectral* spectral, int8_t rssi, int8_t *ext_nf)
{
    int8_t temp;
    int16_t nf = -110;
    SPECTRAL_OPS* p_sops = GET_SPECTRAL_OPS(spectral);
    *ext_nf = -110;
    nf = p_sops->get_ext_noisefloor(spectral);
    temp = 110 + (nf) + (rssi);
    *ext_nf = nf;
    return temp;
}



/*
 * Dummy Functions :
 * These functions are there, to avoid any crashes, due to invoking
 * of spectral functions before they are registered
 *
 */
static u_int64_t null_get_tsf64(void* arg)
{
    spectral_ops_not_registered("get_tsf64");
    return 0;
}

static u_int32_t null_get_capability(void* arg, enum spectral_capability_type type)
{
    /*
     * TODO : We should have conditional compilation to get the capability
     *      : We have not yet attahced ATH layer here, so there is no
     *      : way to check the HAL capbalities
     */
    spectral_ops_not_registered("get_capability");

    /* TODO : For the time being, we are returning TRUE */
    return AH_TRUE;
}

static u_int32_t null_set_rxfilter(void* arg, int rxfilter)
{
    spectral_ops_not_registered("set_rxfilter");
    return 1;
}

static u_int32_t null_get_rxfilter(void* arg)
{
    spectral_ops_not_registered("get_rxfilter");
    return 0;
}

static u_int32_t null_is_spectral_active(void* arg)
{
    spectral_ops_not_registered("is_spectral_active");
    return 1;
}

static u_int32_t null_is_spectral_enabled(void* arg)
{
    spectral_ops_not_registered("is_spectral_enabled");
    return 1;
}

static u_int32_t null_start_spectral_scan(void* arg)
{
    spectral_ops_not_registered("start_spectral_scan");
    return 1;
}

static u_int32_t null_stop_spectral_scan(void* arg)
{
    spectral_ops_not_registered("stop_spectral_scan");
    return 1;
}

static u_int32_t null_get_extension_channel(void* arg)
{
    spectral_ops_not_registered("get_extension_channel");
    return 1;
}

static int8_t null_get_ctl_noisefloor(void* arg)
{
    spectral_ops_not_registered("get_ctl_noisefloor");
    return 1;
}

static int8_t null_get_ext_noisefloor(void* arg)
{
    spectral_ops_not_registered("get_ext_noisefloor");
    return 0;
}

static u_int32_t null_configure_spectral(void* arg, struct spectral_config* params)
{
    spectral_ops_not_registered("configure_spectral");
    return 0;
}

static u_int32_t null_get_spectral_config(void* arg, struct spectral_config* params)
{
    spectral_ops_not_registered("get_spectral_config");
    return 0;
}

static u_int32_t null_get_ent_spectral_mask(void* arg)
{
    spectral_ops_not_registered("get_ent_spectral_mask");
    return 0;
}

static u_int32_t null_get_mac_address(void* arg, char* addr)
{
    spectral_ops_not_registered("get_mac_address");
    return 0;
}

static u_int32_t null_get_current_channel(void* arg)
{
    spectral_ops_not_registered("get_current_channel");
    return 0;
}

static u_int32_t null_reset_hw(void* arg)
{
    spectral_ops_not_registered("get_current_channel");
    return 0;
}

static u_int32_t null_get_chain_noise_floor(void* arg, int16_t* nfBuf)
{
    spectral_ops_not_registered("get_chain_noise_floor");
    return 0;
}

static int null_spectral_process_phyerr(struct ath_spectral* spectral, u_int8_t* data, u_int32_t datalen,
                SPECTRAL_RFQUAL_INFO* p_rfqual,
                SPECTRAL_CHAN_INFO* p_chaninfo,
                u_int64_t tsf64,
                spectral_acs_stats_t *acs_stats)
{
    spectral_ops_not_registered("spectral_process_phyerr");
    return 0;
}

/*
 * Function     : test_spectral_ops
 * Description  : Checks the registred Spectral functions
 * Input        : Pointer to Spectral Struct
 * Output       : Void
 *
 */
#if WLAN_SPECTRAL_ENABLE_DBG_FUNCS
void test_spectral_ops(struct ath_spectral* spectral)
{
    SPECTRAL_OPS* p_sops = GET_SPECTRAL_OPS(spectral);
    printk("Testing Spectral Ops\n");
    printk("TSF         = %llu\n", p_sops->get_tsf64(spectral));
    printk("RXFILT      = %d\n", p_sops->get_rxfilter(spectral));
    printk("Spectral    = %s\n", p_sops->is_spectral_enabled(spectral)?"Enabled":"Disabled");
    printk("Spectral    = %s\n", p_sops->is_spectral_active(spectral)?"Active":"Inactive");
    printk("CTL NF      = %d\n", p_sops->get_ctl_noisefloor(spectral));
    printk("EXT NF      = %d\n", p_sops->get_ext_noisefloor(spectral));
    printk("CUR CH      = %d\n", p_sops->get_current_channel(spectral));
}
#endif
/*
 * Function     : spectral_register_funcs
 * Description  : Registers Spectral related functions
 * Input        : Pointer to Spectral Struct, Pointer to Spectral function tabel
 * Output       : Void
 *
 */
void spectral_register_funcs(void* arg, SPECTRAL_OPS* p)
{
    struct ath_spectral *spectral = ((struct ath_spectral*)(arg));
    SPECTRAL_OPS* p_sops = GET_SPECTRAL_OPS(spectral);

    p_sops->get_tsf64               = p->get_tsf64;
    p_sops->get_capability          = p->get_capability;
    p_sops->set_rxfilter            = p->set_rxfilter;
    p_sops->get_rxfilter            = p->get_rxfilter;
    p_sops->is_spectral_enabled     = p->is_spectral_enabled;
    p_sops->is_spectral_active      = p->is_spectral_active;
    p_sops->start_spectral_scan     = p->start_spectral_scan;
    p_sops->stop_spectral_scan      = p->stop_spectral_scan;
    p_sops->get_extension_channel   = p->get_extension_channel;
    p_sops->get_ctl_noisefloor      = p->get_ctl_noisefloor;
    p_sops->get_ext_noisefloor      = p->get_ext_noisefloor;
    p_sops->configure_spectral      = p->configure_spectral;
    p_sops->get_spectral_config     = p->get_spectral_config;
    p_sops->get_ent_spectral_mask   = p->get_ent_spectral_mask;
    p_sops->get_mac_address         = p->get_mac_address;
    p_sops->get_current_channel     = p->get_current_channel;
    p_sops->reset_hw                = p->reset_hw;
    p_sops->get_chain_noise_floor   = p->get_chain_noise_floor;
    p_sops->spectral_process_phyerr = p->spectral_process_phyerr;

    return;

}OS_EXPORT_SYMBOL(spectral_register_funcs);

/*
 * Function     : spectral_set_debug_level_da
 * Description  : Set debug level for Spectral DA LMAC
 * Input        : Pointer to pdev, debug level
 * Return       : 0
 */
static int spectral_set_debug_level_da(struct wlan_objmgr_pdev *pdev, u_int32_t debug_level)
{
    spectral_debug_level = (DEBUG_SPECTRAL << debug_level);
    return 0;
}

/*
 * Function     : spectral_get_debug_level_da
 * Description  : Get debug level for Spectral DA LMAC
 * Input        : Pointer to pdev
 * Return       : Debug level
 */
static u_int32_t spectral_get_debug_level_da(struct wlan_objmgr_pdev *pdev)
{
    qdf_print("spectral_get_debug_level_da is unimplemented");
    return 0;
}

/*
 * Function     : get_spectral_capinfo_da
 * Description  : Get Spectral capability information maintained by Spectral DA
 *                LMAC
 * Input        : Pointer to pdev, pointer to buffer into which data should be
 *                copied
 * Output       : Spectral capability information
 */
static void get_spectral_capinfo_da(struct wlan_objmgr_pdev *pdev, void *outdata)
{
    struct ath_spectral *spectral = NULL;

    spectral = get_spectral_handle_from_pdev(pdev);
    qdf_mem_copy(outdata, &spectral->capability,
                    sizeof(struct spectral_caps));
}

/*
 * Function     : get_spectral_diagstats_da
 * Description  : Get Spectral diagnostic information maintained by Spectral DA
 *                LMAC
 * Input        : Pointer to pdev, pointer to buffer into which data should be
 *                copied
 * Output       : Spectral diagnostic information
 */
static void get_spectral_diagstats_da(struct wlan_objmgr_pdev *pdev, void *outdata)
{
    struct ath_spectral *spectral = NULL;

    spectral = get_spectral_handle_from_pdev(pdev);
    qdf_mem_copy(outdata, &spectral->diag_stats,
                    sizeof(struct spectral_diag_stats));
}

/*
 * Function     : get_nominal_nf_da
 * Description  : Get Nominal Noise Floor, through Spectral DA LMAC
 * Input        : Pointer to pdev
 * Return       : Nominal Noise Floor
 */
int16_t get_nominal_nf_da(struct wlan_objmgr_pdev *pdev)
{
    struct ath_spectral *spectral = NULL;
    SPECTRAL_OPS* p_sops = NULL;
    u_int32_t channel_freq;
    HAL_FREQ_BAND band;

    spectral = get_spectral_handle_from_pdev(pdev);
    p_sops = GET_SPECTRAL_OPS(spectral);
    channel_freq = p_sops->get_current_channel(spectral);
    band = (channel_freq > 4000)?HAL_FREQ_BAND_5GHZ:HAL_FREQ_BAND_2GHZ;
    return p_sops->get_nominal_nf(spectral, band);
}

/*
 * Function     : is_spectral_active_da
 * Description  : Get whether Spectral is active, for Spectral DA LMAC
 * Input        : Pointer to pdev
 * Return       : True if Spectral is active, false if Spectral is inactive
 */
static bool is_spectral_active_da(struct wlan_objmgr_pdev *pdev)
{
    struct ath_spectral *spectral = NULL;
    SPECTRAL_OPS* p_sops = NULL;

    spectral = get_spectral_handle_from_pdev(pdev);
    p_sops = GET_SPECTRAL_OPS(spectral);
    return p_sops->is_spectral_active(spectral);
}

/*
 * Function     : is_spectral_enabled_da
 * Description  : Get whether Spectral is enabled, for Spectral DA LMAC
 * Input        : Pointer to pdev
 * Return       : True if Spectral is enabled, false if Spectral is not enabled
 */
static bool is_spectral_enabled_da(struct wlan_objmgr_pdev *pdev)
{
    struct ath_spectral *spectral = NULL;
    SPECTRAL_OPS* p_sops = NULL;

    spectral = get_spectral_handle_from_pdev(pdev);
    p_sops = GET_SPECTRAL_OPS(spectral);
    return p_sops->is_spectral_enabled(spectral);
}

/**
 * da_ath_sptrl_register_tx_ops() - Register Spectral DA LMAC tx ops
 * @rx_ops: LMAC tx ops
 *
 * Return: None
 */
void da_ath_sptrl_register_tx_ops(struct wlan_lmac_if_tx_ops *tx_ops)
{
    tx_ops->sptrl_tx_ops.sptrlto_pdev_spectral_init     = pdev_spectral_init_da;
    tx_ops->sptrl_tx_ops.sptrlto_pdev_spectral_deinit   = pdev_spectral_deinit_da;
    tx_ops->sptrl_tx_ops.sptrlto_set_spectral_config    = spectral_set_thresholds;
    tx_ops->sptrl_tx_ops.sptrlto_get_spectral_config    = spectral_get_thresholds;
    tx_ops->sptrl_tx_ops.sptrlto_start_spectral_scan    = start_spectral_scan_da;
    tx_ops->sptrl_tx_ops.sptrlto_stop_spectral_scan     = stop_current_scan_da;
    tx_ops->sptrl_tx_ops.sptrlto_is_spectral_active     = is_spectral_active_da;
    tx_ops->sptrl_tx_ops.sptrlto_is_spectral_enabled    = is_spectral_enabled_da;
    tx_ops->sptrl_tx_ops.sptrlto_set_debug_level        = spectral_set_debug_level_da;
    tx_ops->sptrl_tx_ops.sptrlto_get_debug_level        = spectral_get_debug_level_da;
    tx_ops->sptrl_tx_ops.sptrlto_get_spectral_capinfo   = get_spectral_capinfo_da;
    tx_ops->sptrl_tx_ops.sptrlto_get_spectral_diagstats = get_spectral_diagstats_da;
}
EXPORT_SYMBOL(da_ath_sptrl_register_tx_ops);

#ifdef __linux__
/*
 * Linux Module glue.
 */

#ifndef EXPORT_SYMTAB
#define EXPORT_SYMTAB
#endif

EXPORT_SYMBOL(spectral_scan_enable);
EXPORT_SYMBOL(ath_process_spectraldata);
EXPORT_SYMBOL(is_spectral_phyerr);
EXPORT_SYMBOL(spectral_send_intf_found_msg);
EXPORT_SYMBOL(spectral_scan_enable_params);
EXPORT_SYMBOL(spectral_record_chan_info);

#endif /* __linux__ */

#undef printk
#undef SPECTRAL_MIN
#undef SPECTRAL_MAX
#undef SPECTRAL_DIFF

#endif
