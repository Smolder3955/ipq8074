/*
 * =====================================================================================
 *
 *       Filename:  ath_classifier.c
 *
 *    Description:  Classifier
 *
 *        Version:  1.0
 *        Created:  12/26/2011 11:16:31 AM
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Anil Hebbar (Algorithm)
 *         Author:  S.Karthikeyan
 *        Company:  Qualcomm Atheros
 *
 *        Copyright (c) 2012-2018 Qualcomm Technologies, Inc.
 *
 *        All Rights Reserved.
 *        Confidential and Proprietary - Qualcomm Technologies, Inc.
 *
 *        2012-2016 Qualcomm Atheros, Inc.
 *
 *        All Rights Reserved.
 *        Qualcomm Atheros Confidential and Proprietary.
 *
 * =====================================================================================
 */

#include "stdio.h"
#include "stdlib.h"
#include <linux/types.h>
#include <string.h>
#include <math.h>
#include "ath_classifier.h"

CLASSIFER_DATA_STRUCT class_data[CLASSIFIER_HASHSIZE];
float powf_precompute_table[256];

/* Buffers to log the FFT bin values */
static u_int8_t bin_log_buf[(MAX_FFT_BINS + 3) * SPECTRAL_DBG_LOG_SAMP];
static int32_t mis_log_buf[3 * SPECTRAL_DBG_LOG_SAMP];

/* Added to suport secondary 80Mhz segment */
static u_int8_t bin_log_buf_sec80[(MAX_FFT_BINS + 3) * SPECTRAL_DBG_LOG_SAMP];

/* Timestamp is common to both segments and so miscellaneous secondary 80Mhz segment
   buffer only holds RSSI and Noise Floor
 */
static int32_t mis_log_buf_sec80[2 * SPECTRAL_DBG_LOG_SAMP];

static void init_powf_table(void);



/*
 * Function     : init_classifier_lookup_tables
 * Description  : Initialize all look up tables, should be called first
 * Input params : Void
 *
 */
void init_classifier_lookup_tables(void)
{
    init_powf_table();
}

/*
 * Function     : init_classifier_data
 * Description  : Initializes the classifier data structure for given MAC address
 * Input params : MAC address, pointer to Spectral capabilities information,
 *                length of Spectral capabilities information
 */
void init_classifier_data(const u_int8_t* macaddr, void *spectral_caps,
        size_t spectral_caps_len)
{
    int index = 0;
    CLASSIFER_DATA_STRUCT *pclas = NULL;

    SPECTRAL_CLASSIFIER_ASSERT(macaddr != NULL);
    SPECTRAL_CLASSIFIER_ASSERT(spectral_caps != NULL);
    SPECTRAL_CLASSIFIER_ASSERT(spectral_caps_len == sizeof(pclas->caps));

    index = CLASSIFIER_HASH(macaddr);
    pclas = &class_data[index];

    memcpy(&(pclas->caps), spectral_caps, sizeof(pclas->caps));

    if (pclas->caps.advncd_spectral_cap == true) {
        pclas->thresholds.cw_int_det_thresh = ADVNCD_CW_INT_DET_THRESH;
        pclas->thresholds.wifi_det_min_diff = (int)ADVNCD_WIFI_DET_MIN_DIFF;
        pclas->thresholds.fhss_sum_scale_down_factor = ADVNCD_FHSS_SUM_SCALE_DOWN_FACTOR;
        pclas->thresholds.mwo_pwr_variation_threshold = ADVNCD_MWO_POW_VARTIATION_THRESH;
    } else {
        pclas->thresholds.cw_int_det_thresh = CW_INT_DET_THRESH;
        pclas->thresholds.wifi_det_min_diff = (int)WIFI_DET_MIN_DIFF;
        pclas->thresholds.fhss_sum_scale_down_factor = FHSS_SUM_SCALE_DOWN_FACTOR;
        pclas->thresholds.mwo_pwr_variation_threshold = MWO_POW_VARIATION_THRESH;
    }

}

/*
 * Function     : get_classifier_data
 * Description  : Returns the classifier data structure for given MAC address
 * Input params : MAC address
 * Return       : Pointer to Classifier data structure
 *
 */
CLASSIFER_DATA_STRUCT* get_classifier_data(const u_int8_t* macaddr)
{
    int index = CLASSIFIER_HASH(macaddr);
    return &class_data[index];
}

/*
 * Function     : spectral_scan_log_data
 * Description  : Log the spectral data
 * Input params : Pointer to classifier data structure, mode, commit
 * Return       : Void
 */
static void
spectral_scan_log_data(SPECTRAL_SAMP_MSG* msg, CLASSIFER_DATA_STRUCT *pclas, DETECT_MODE mode, u_int32_t commit)
{
    u_int8_t *buf_ptr;
    u_int8_t *buf_ptr_sec80;

    /* Check if the mode matches */
    if (pclas->log_mode != mode) {
        return;
    }

    /* Check if enough data has already been logged */
    if (pclas->commit_done) {
        return;
    }

    if (msg->signature != SPECTRAL_SIGNATURE) {
        return;
    }

    /* Check if the initlization needs to be done */
    if (!pclas->spectral_log_first_time) {

        pclas->spectral_log_first_time          = TRUE;
        pclas->spectral_log_num_bin             = msg->samp_data.bin_pwr_count;
        pclas->spectral_bin_bufSave             = bin_log_buf;
        pclas->spectral_bin_bufSave_sec80       = bin_log_buf_sec80;
        pclas->spectral_data_misc               = mis_log_buf;
        pclas->spectral_data_misc_sec80         = mis_log_buf_sec80;
    }

    /* Not not log the burst if the commit flag is set, just save the data logged so far */
    if (!commit) {

        /* Get the offset to the place where the data needs to be saved */
        buf_ptr = pclas->spectral_bin_bufSave + (pclas->spectral_log_num_bin + 2) * pclas->last_samp;
        buf_ptr_sec80 = pclas->spectral_bin_bufSave_sec80 +
                        (pclas->spectral_log_num_bin + 2) * pclas->last_samp;
        buf_ptr[0] = (u_int8_t)(pclas->spectral_num_samp_log & 0xff);

        /*XXXX : The value overflows for bin values greater than 255
                 For printing to the file, the values are taken from spectral_log_num_bin variable
         */
        buf_ptr[1] = (u_int8_t)pclas->spectral_log_num_bin;

        /* Copy the bins */
        memcpy(buf_ptr + 3, msg->samp_data.bin_pwr, pclas->spectral_log_num_bin);

        /* Copy the time stamp, rssi and noise floor data */
        pclas->spectral_data_misc[pclas->last_samp * 3]     = msg->samp_data.spectral_tstamp;
        pclas->spectral_data_misc[pclas->last_samp * 3 + 1] = msg->samp_data.spectral_rssi;
        pclas->spectral_data_misc[pclas->last_samp * 3 + 2] = msg->samp_data.noise_floor;

        /* Logging secondary 80Mhz data if channel width is 160Mhz */
        if (msg->samp_data.ch_width == IEEE80211_CWM_WIDTH160) {

            /* Sample number and No. of FFT bins are loaded from primary 80Mhz segment buffer */
            buf_ptr_sec80[0] = buf_ptr[0];
            buf_ptr_sec80[1] = buf_ptr[1];

            memcpy(buf_ptr_sec80 + 3, msg->samp_data.bin_pwr_sec80, pclas->spectral_log_num_bin);

            /* Timestamp is common to both segments and so miscellaneous secondary 80Mhz segment
               buffer only holds RSSI and Noise Floor
             */
            pclas->spectral_data_misc_sec80[pclas->last_samp * 2]     =
            msg->samp_data.spectral_rssi_sec80;
            pclas->spectral_data_misc_sec80[pclas->last_samp * 2 + 1] =
            msg->samp_data.noise_floor_sec80;
        }

        pclas->spectral_num_samp_log++;

        if (pclas->spectral_num_samp_log >= SPECTRAL_DBG_LOG_SAMP ) {
            pclas->spectral_num_samp_log = SPECTRAL_DBG_LOG_SAMP;
            /* In case of log all samples, there is no trigger, commit */
            if (pclas->log_mode == SPECT_CLASS_DETECT_ALL) {
                commit = 1;
            }
        }

        pclas->last_samp++;

        if (pclas->last_samp == SPECTRAL_DBG_LOG_SAMP) {
            pclas->last_samp = 0;
        }
    }

    /* Check if enough samples have been captured. If so, log the data */
    if (commit) {

        /* Commit data to file */
        FILE* spectral_log_fp = fopen("classifier.log", "wt");

        if (!spectral_log_fp) {
            printf("Spectral Classifier: Could not open file %s to write\n", "classifier.log");
            return;
        }

        printf("Spectral Classifier: Number of samples captured %d\n", pclas->spectral_num_samp_log);

        printf("Spectral Classifier: Writing samples to file. Please wait for a \n"
               "                     few minutes. Classification functionality \n"
               "                     might be limited in the meantime...\n");
        {
            /* Print the data into a ascii file */
            u_int16_t cnt       = 0;
            u_int16_t valCnt    = 0;
            u_int16_t sampIdx   = pclas->last_samp;

            if (pclas->spectral_num_samp_log < SPECTRAL_DBG_LOG_SAMP) {
                /* In this case, no wrap around, so the first sample is the start of the
                 * buffer
                 */
                sampIdx = 0;
            }


            /****************************************************

                SAMP FILE FORMAT
                ----------------

                Line 1 : Version number | MAC Address | Channel Width | Operating Frequency
                Line 2 : Sample 1 related info for primary 80Mhz segment
                Line 3 : Sample 1 related info for secondary 80Mhz segment
                Line 4 : Sample 2 related info for primary 80Mhz segment
                Line 5 : Sample 2 related info for secondary 80Mhz segment
                :
                Line n : Sample n related info

                XXX : KEEP THIS IN SYNC WITH REPLAY FUNCTION
                      ELSE THE REPLAY FUNCTION WILL FAIL
             *******************************************************/
            fprintf(spectral_log_fp, "%d %s %d %d\n", SPECTRAL_LOG_VERSION_ID2, ether_sprintf(pclas->macaddr), msg->samp_data.ch_width, msg->freq);

            for (cnt = 0; cnt < pclas->spectral_num_samp_log; cnt++) {

                buf_ptr = pclas->spectral_bin_bufSave + (pclas->spectral_log_num_bin + 2) * sampIdx;
                buf_ptr_sec80 = pclas->spectral_bin_bufSave_sec80 + (pclas->spectral_log_num_bin + 2)
                  * sampIdx;


                /*
                 * XXX: buf_ptr[0] : Holds index
                 *      buf_ptr[1] : Holds number for bins
                 *      But these are of type unsigned char and hold value upto 256
                 *      Hence, cnt, pclas->spectral_log_num_bin are used to print correct values
                 *      to the file
                 */

                fprintf( spectral_log_fp, "%u %u ", cnt, pclas->spectral_log_num_bin);

                for (valCnt = 0; valCnt < pclas->spectral_log_num_bin; valCnt++) {
                    fprintf( spectral_log_fp, "%u ", (u_int8_t)(buf_ptr[2 + valCnt]));
                }

                fprintf(spectral_log_fp, "%u ", (unsigned)pclas->spectral_data_misc[sampIdx * 3]);
                fprintf(spectral_log_fp, "%d ", pclas->spectral_data_misc[sampIdx * 3 + 1]);
                fprintf(spectral_log_fp, "%d", pclas->spectral_data_misc[sampIdx * 3 + 2]);
                fprintf(spectral_log_fp,"\n");
                buf_ptr += (pclas->spectral_log_num_bin + 2);

                /* Writing secondary 80Mhz segment related info if channel width is 160Mhz */
                if (msg->samp_data.ch_width == IEEE80211_CWM_WIDTH160) {
                  fprintf( spectral_log_fp, "%u %u ", cnt, pclas->spectral_log_num_bin);

                  for (valCnt = 0; valCnt < pclas->spectral_log_num_bin; valCnt++) {
                    fprintf( spectral_log_fp, "%u ", (u_int8_t)(buf_ptr_sec80[2 + valCnt]));
                  }

                  fprintf(spectral_log_fp, "%u ", (unsigned)pclas->spectral_data_misc[sampIdx * 3]);
                  fprintf(spectral_log_fp, "%d ", pclas->spectral_data_misc_sec80[sampIdx * 2]);
                  fprintf(spectral_log_fp, "%d", pclas->spectral_data_misc_sec80[sampIdx * 2 + 1]);
                  fprintf(spectral_log_fp,"\n");
                }
                sampIdx++;
                buf_ptr_sec80 += (pclas->spectral_log_num_bin + 2);
                if (sampIdx == SPECTRAL_DBG_LOG_SAMP) sampIdx = 0;
            }
        }

        /* Cleanup */
        fclose(spectral_log_fp);
        system("sync");
        printf("Spectral Classifier: Completed writing samples to file.\n");
        pclas->commit_done = TRUE;
    }
}

/*
 * Function     : init_powf_table
 * Description  : Initialize precomputed table for powf function
 * Input params : Void
 * Return       : Void
 */
static void init_powf_table(void)
{
    int16_t i;

    for (i = -128; i <= 127; i++) {
        powf_precompute_table[((u_int8_t)i)] = powf((float)10.0, (float)(i / 20.0));
    }

    return;
}

/*
 * Function     : spectral_scan_classifer_sm_init
 * Description  : Initialize the Spectral Classifier State Machine. Init is done on type/mode
 * Input params : Pointer to classifier data, mode to initialize
 * Return       : Void
 */
void spectral_scan_classifer_sm_init(CLASSIFER_DATA_STRUCT *pclas, DETECT_MODE mode, uint8_t seg_index)
{
    /* Initialize the microwave oven interference */
    if (mode & SPECT_CLASS_DETECT_MWO) {

        pclas->mwo_burst_idx        = 0;
        pclas->mwo_burst_found      = 0;
        pclas->mwo_in_burst_time    = 0;
        pclas->mwo_thresh           = MWO_POW_VARIATION_THRESH;
        pclas->mwo_rssi             = 0;
        pclas->mwo_cur_bin          = 0;
        memset(&pclas->mwo_param, 0, sizeof(spectral_mwo_param)*NUM_MWO_BINS);

    }

    /* Initialize the CW interference */
    if (mode & SPECT_CLASS_DETECT_CW) {

      if(!seg_index) {
        pclas->cw[0].burst_found        = 0;
        pclas->cw[0].start_time         = 0;
        pclas->cw[0].last_found_time    = 0;
        pclas->cw[0].rssi               = 0;
        pclas->cw[0].num_detected       = 0;
      }
      else {
        pclas->cw[1].burst_found        = 0;
        pclas->cw[1].start_time         = 0;
        pclas->cw[1].last_found_time    = 0;
        pclas->cw[1].rssi               = 0;
        pclas->cw[1].num_detected       = 0;
      }
    }

    /* Initialize the WiFi interference */
    if (mode & SPECT_CLASS_DETECT_WiFi) {
        pclas->spectral_num_wifi_detected = 0;
        pclas->wifi_rssi = 0;
    }

    /* Initialize the FHSS interference */
    if (mode & SPECT_CLASS_DETECT_FHSS) {
      if(!seg_index) {
        pclas->fhss[0].cur_bin = 0;
        memset(&pclas->fhss[0].fhss_param, 0, sizeof(spectral_fhss_param) * NUM_FHSS_BINS);
      }
      else {
        pclas->fhss[1].cur_bin = 0;
        memset(&pclas->fhss[1].fhss_param, 0, sizeof(spectral_fhss_param) * NUM_FHSS_BINS);
      }
    }

    /* Initialize generic data */
    if (mode == SPECT_CLASS_DETECT_ALL) {
        /* Some generic init */
        pclas->spectral_detect_mode         = SPECT_CLASS_DETECT_ALL;
        pclas->spectral_num_wifi_detected   = 0;
        pclas->current_interference         = 0;
        pclas->mwo_detect_ts                = 0;
        pclas->mwo_num_detect               = 0;
        pclas->cw[0].detect_ts              = 0;
        pclas->cw[0].num_detect             = 0;
        pclas->cw[1].detect_ts              = 0;
        pclas->cw[1].num_detect             = 0;
        pclas->wifi_detect_ts               = 0;
        pclas->wifi_num_detect              = 0;
        pclas->dsss_detect_ts               = 0;
        pclas->dsss_num_detect              = 0;
        pclas->cur_freq                     = 0;
        pclas->cw_cnt                       = 0;
        pclas->wifi_cnt                     = 0;
        pclas->mwo_cnt                      = 0;
        pclas->fhss_cnt                     = 0;
    }

    pclas->sm_init_done = TRUE;
}

/*
 * Function     : detect_mwo
 * Description  : Detect Microwave oven
 * Input params : Pointers to SAMP msg and Classifier data
 * Return       : Found/Not Found
 *
 */
int detect_mwo(SPECTRAL_SAMP_MSG* msg, CLASSIFER_DATA_STRUCT *pclas)
{
    int mwo_burst_found     = 0;
    int mwo_device_found    = 0;

    /* Check if this is a valid Microwave Oven frequency */
    if (msg->freq < MWO_MIN_FREQ || msg->freq > MWO_MAX_FREQ) {
        return 0;
    }

    if (msg->samp_data.spectral_rssi > GET_MWO_MIN_RSSI_THRESHOLD(pclas)) {

        /* There is something in the air */
        float rssiLin                   = powf_precompute_table[(u_int8_t)msg->samp_data.spectral_rssi];
        u_int32_t bin_cnt               = 0;
        u_int32_t bin_pwr[MAX_FFT_BINS] = {0};
        u_int32_t low_bnd_pwr           = 0;
        u_int32_t up_bnd_pwr            = 0;
        u_int32_t bin_group_diff_abs    = 0;
        /* Time delta since last recorded in-burst instance */
        u_int32_t time_since_in_burst   = 0;

        /* Calculate power levels for each bins */
        for (bin_cnt = 0; bin_cnt < SAMP_NUM_OF_BINS(msg); bin_cnt++ ) {
            bin_pwr[bin_cnt] = (u_int32_t)(((float)msg->samp_data.bin_pwr[bin_cnt] * rssiLin) + 0.5);
        }

        for (bin_cnt = 0; bin_cnt < MWO_BIN_CLUSTER_COUNT; bin_cnt++) {
            low_bnd_pwr += bin_pwr[bin_cnt];
            up_bnd_pwr += bin_pwr[msg->samp_data.bin_pwr_count - bin_cnt - 1];
        }

        bin_group_diff_abs = abs((int)(up_bnd_pwr - low_bnd_pwr));

        if (bin_group_diff_abs > GET_MWO_POW_VARIATION_THRESHOLD(pclas)) {
            pclas->mwo_thresh = (GET_PCLAS_MWO_TRHESHOLD(pclas) - (GET_PCLAS_MWO_TRHESHOLD(pclas) >> 2)) +
                (bin_group_diff_abs >> 2);
        }

        //printf("%d,  %d,  %d, %d\n", low_bnd_pwr, up_bnd_pwr, bin_group_diff_abs, pclas->mwo_thresh);

        if (bin_group_diff_abs > (int)((GET_PCLAS_MWO_TRHESHOLD(pclas) * 7 ) >> 3) ) {
            /* Tune the threshold so that the threshold is not too low or too high */

            if (!IS_MWO_BURST_FOUND(pclas)) {

                /* First burst */
                pclas->mwo_burst_start_time     = SAMP_GET_SPECTRAL_TIMESTAMP(msg);
                pclas->mwo_in_burst_time        = SAMP_GET_SPECTRAL_TIMESTAMP(msg);
                pclas->mwo_burst_found          = 1;

                spectral_scan_log_data(msg, pclas, SPECT_CLASS_DETECT_MWO, 0);

            } else {
                time_since_in_burst = SAMP_GET_SPECTRAL_TIMESTAMP(msg) -
                    GET_PCLAS_MWO_IN_BURST_TIME(pclas);

                if (time_since_in_burst < MWO_INTER_BURST_DURATION) {
                    pclas->mwo_in_burst_time = SAMP_GET_SPECTRAL_TIMESTAMP(msg);

                    if ((SAMP_GET_SPECTRAL_TIMESTAMP(msg) - GET_PCLAS_MWO_BURST_START_TIME(pclas)) > MWO_MAX_BURST_TIME) {
                        /* This is not an MWO burst because it is on for too long */
                        spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_MWO, 0);
                    } else {
                        spectral_scan_log_data(msg, pclas, SPECT_CLASS_DETECT_MWO, 0);
                    }

                } else {
                    if (GET_PCLAS_MWO_IN_BURST_TIME(pclas) ==
                                GET_PCLAS_MWO_BURST_START_TIME(pclas)) {
                        /* The burst is too short. Might be FHSS or some
                         * spurious signal.
                         */
#if CLASSIFIER_DEBUG
                        cinfo("Burst is too short. Might be spurious.\n");
#endif /* CLASSIFIER_DEBUG */
                        spectral_scan_classifer_sm_init(pclas,
                                SPECT_CLASS_DETECT_MWO, 0);
                    } else if (time_since_in_burst >
                            MWO_MAX_INTER_BURST_DURATION) {
                        /* The next burst occurred after too long a gap. Might be
                         * a spurious signal.
                         */
 #if CLASSIFIER_DEBUG
                        cinfo("Inter-burst duration is too high (%u us). Might "
                              "be spurious.\n",
                              time_since_in_burst);
#endif /* CLASSIFIER_DEBUG */
                        spectral_scan_classifer_sm_init(pclas,
                                SPECT_CLASS_DETECT_MWO, 0);
                    } else {
                        /* Previous burst is over, this could be a new burst */
                        pclas->mwo_burst_idx++;
                        pclas->mwo_rssi += SAMP_GET_SPECTRAL_RSSI(msg);

                        spectral_scan_log_data(msg, pclas, SPECT_CLASS_DETECT_MWO, 0);

                        if (pclas->mwo_burst_idx == MWO_NUM_BURST) {

                            /* Got enough bursts to be sure */
                            pclas->mwo_rssi /= MWO_NUM_BURST;
                            mwo_burst_found = 1;
                            spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_MWO, 0);

                        } else {

                            /* Start recording the new burst */
                            pclas->mwo_burst_start_time = SAMP_GET_SPECTRAL_TIMESTAMP(msg);
                            pclas->mwo_in_burst_time    = SAMP_GET_SPECTRAL_TIMESTAMP(msg);
                        }
                    }
                }
            }

        } else {

            if ((pclas->mwo_burst_found) && (bin_group_diff_abs < (int)(((GET_PCLAS_MWO_TRHESHOLD(pclas) * 7) >> 3) >> 2))) {
                /* We had found a burst. Check if the time is too long ago and clear it */
                if ((SAMP_GET_SPECTRAL_TIMESTAMP(msg) - GET_PCLAS_MWO_BURST_START_TIME(pclas)) > MWO_BURST_INACTIVITY_TIMEOUT) {
                    /* too long without a burst, clear microwave detction */
                    spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_MWO, 0);
                }
            }
        }
    } else {
        /* No interference detected, check if we were looking for something */
        if (IS_MWO_BURST_FOUND(pclas)) {
            /* We had found a burst. Check if the time is too far back in time, and clear it */
            if ((SAMP_GET_SPECTRAL_TIMESTAMP(msg) - GET_PCLAS_MWO_BURST_START_TIME(pclas)) > MWO_BURST_INACTIVITY_TIMEOUT) {
                /* Too long without a burst, clear microwave detction */
                spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_MWO, 0);
            }
        }
    }

    if (mwo_burst_found) {

        /* Something has been detected. Introduce hysteresis for stability of detection */
        if (!(pclas->current_interference & SPECT_CLASS_DETECT_MWO)) {
            if (!pclas->mwo_num_detect) {
                pclas->mwo_num_detect = 1;
                pclas->mwo_detect_ts = msg->samp_data.spectral_tstamp;
            } else {
                pclas->mwo_num_detect++;
                if ((int)(SAMP_GET_SPECTRAL_TIMESTAMP(msg) - GET_PCLAS_MWO_DETECT_TIMESTAMP(pclas)) < (MWO_STABLE_DETECT_THRESH)) {
                    pclas->current_interference |= SPECT_CLASS_DETECT_MWO;
                    pclas->mwo_detect_ts = SAMP_GET_SPECTRAL_TIMESTAMP(msg);
                    printf("Spectral Classifier: Found MWO Interference in frequency %d\n", msg->freq);
                    mwo_device_found = 0;
                    pclas->mwo_cnt++;
                    spectral_scan_log_data(msg, pclas, SPECT_CLASS_DETECT_MWO, 1);
                } else {
                    /* Took too much time, reset the counter -- Should not be here */
                    pclas->mwo_num_detect = 1;
                    pclas->mwo_detect_ts = SAMP_GET_SPECTRAL_TIMESTAMP(msg);
                }
            }
        } else {
            /* Update the time */
            pclas->mwo_num_detect = 1;
            pclas->mwo_detect_ts = SAMP_GET_SPECTRAL_TIMESTAMP(msg);
        }
    } else if (pclas->current_interference & SPECT_CLASS_DETECT_MWO) {
        /* Check if it has been found before and not found for some time */
        if ((int)(msg->samp_data.spectral_tstamp - pclas->mwo_detect_ts) > (MWO_DETECT_INACTIVITY_TIMEOUT)) {
            pclas->current_interference &=  ~(SPECT_CLASS_DETECT_MWO);
            pclas->mwo_num_detect = 0;
            printf("Spectral Classifier: No MWO Interference\n");
        }
    }

    return mwo_device_found;
}

/*
 * Function     : detect_cw
 * Description  : Detect the CW interfernce.
 * Input params : Pointers to SAMP msg and classifier data
 * Return       : Found/Not Found
 *
 */
int detect_cw(SPECTRAL_SAMP_MSG* msg, CLASSIFER_DATA_STRUCT *pclas, uint8_t use_sec80)
{
    int ret_val = 0;
    int index = (use_sec80)?1:0;    /* Index to determine if classifier operates on segment 0 or segment 1 */
    CLASSIFIER_CW_PARAMS *pseg = &pclas->cw[index];
    int16_t   spectral_rssi = 0;
    u_int16_t   bin_pwr_count = 0;
    u_int8_t *bin_pwr = NULL;

    if (!(pclas->spectral_detect_mode & SPECT_CLASS_DETECT_CW))  {
        return ret_val;
    }

    if (!use_sec80) {
        spectral_rssi = msg->samp_data.spectral_rssi;
        bin_pwr_count = msg->samp_data.bin_pwr_count;
        bin_pwr = (u_int8_t *)msg->samp_data.bin_pwr;
    } else {
        spectral_rssi = msg->samp_data.spectral_rssi_sec80;
        bin_pwr_count = msg->samp_data.bin_pwr_count_sec80;
        bin_pwr = (u_int8_t *)msg->samp_data.bin_pwr_sec80;
    }

    /* Check if there is high noise floor */
    if (spectral_rssi > GET_CW_RSSI_THRESH(pclas)) {

        u_int16_t bin_cnt   = 0;
        u_int16_t peak_bin  = 0;
        u_int16_t peak_val  = 0;
        u_int16_t chk_upr   = 0;
        u_int16_t chk_lwr   = 0;
        u_int16_t upr_bin   = 0;
        u_int16_t lwr_bin   = 0;
        u_int32_t upr_sum   = 0;
        u_int32_t lwr_sum   = 0;
        u_int32_t center_sum = 0;

        for (bin_cnt = 0; bin_cnt < bin_pwr_count; bin_cnt++ ) {
            if (peak_val < bin_pwr[bin_cnt]) {
                peak_bin = bin_cnt;
                peak_val = bin_pwr[bin_cnt];
            }
        }

        /* Check how many bins we can compare with */
        if (peak_bin + (GET_CW_INT_BIN_SUM_SIZE(pclas) + (GET_CW_INT_BIN_SUM_SIZE(pclas) >> 1)) <= msg->samp_data.bin_pwr_count) {
            chk_upr = 1;
        }

        if ((int)peak_bin - (GET_CW_INT_BIN_SUM_SIZE(pclas) + (GET_CW_INT_BIN_SUM_SIZE(pclas) >> 1)) <= 0) {
            chk_lwr = 1;
        }

        /* set the upper and lower bin markers */
        /* XXX : Note, this logic works only for GET_CW_INT_BIN_SUM_SIZE(pclas) = 3 */
        if (peak_bin == bin_pwr_count) {

            upr_bin = peak_bin;
            lwr_bin = peak_bin - 2;

        } else if (peak_bin == 0) {

            upr_bin = peak_bin + 2;
            lwr_bin = peak_bin;

        } else {

            upr_bin = peak_bin + 1;
            lwr_bin = peak_bin - 1;

        }

        center_sum = bin_pwr[lwr_bin] +
                     bin_pwr[lwr_bin + 1] +
                     bin_pwr[lwr_bin + 2];

        if (chk_upr) {
            upr_sum = bin_pwr[upr_bin + 1] +
                      bin_pwr[upr_bin + 2] +
                      bin_pwr[upr_bin + 3];
            }

        if (chk_lwr) {
            lwr_sum = bin_pwr[lwr_bin - 1] +
                      bin_pwr[lwr_bin - 2] +
                      bin_pwr[lwr_bin - 3];
        }

        /*
         * Check if this is greater than threhold
         * XXX : Note, this logic works only for GET_CW_INT_BIN_SUM_SIZE(pclas) = 3
         */

        if ( (lwr_sum < (center_sum >> GET_CW_SUM_SCALE_DOWN_FACTOR(pclas))) &&
             (upr_sum < (center_sum >> GET_CW_SUM_SCALE_DOWN_FACTOR(pclas))) &&
             (center_sum > GET_CW_INT_DET_THRESH(pclas))) {

                  /* Found a likely CW interference case */
                  pseg->num_detected++;
                  pseg->rssi += spectral_rssi;

                  /* Log the burst for the records */
                  spectral_scan_log_data(msg, pclas, SPECT_CLASS_DETECT_CW,0);

                  if (!pseg->burst_found) {

                      pseg->burst_found       = 1;
                      pseg->start_time        = msg->samp_data.spectral_tstamp;
                      pseg->last_found_time   = msg->samp_data.spectral_tstamp;

                  } else {

                      pseg->last_found_time = msg->samp_data.spectral_tstamp;

                      if (((int)(pseg->last_found_time - pseg->start_time) > GET_CW_INT_FOUND_TIME_THRESH(pclas)) &&
                             (pseg->num_detected > GET_CW_INT_FOUND_MIN_CNT(pclas))) {

                          pseg->rssi /= pseg->num_detected;
                          spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_CW, index);
                          ret_val = 1;

                      }
                  }

             } else {

                 if (pseg->burst_found) {

                     if ((int)(msg->samp_data.spectral_tstamp - pseg->last_found_time) > GET_CW_INT_MISSING_THRESH(pclas) ) {
                         /* Burst missing for too long */
                         spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_CW, index);
                     }
                 }
             }

    } else {

        if (pseg->burst_found) {

            /* Check how long the burst has been missing */
            if ((int)(msg->samp_data.spectral_tstamp - pseg->last_found_time) > GET_CW_INT_MISSING_THRESH(pclas) ) {
                /* Burst missing for too long */
                spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_CW, index);
            }
        }
    }

    if (ret_val) {

        /* add hystrysis to the detection in order to provide stablity */
        ret_val = 0;

        if ((!(pclas->current_interference & SPECT_CLASS_DETECT_CW)) || (!pseg->found_cw)) {

            if (!pseg->num_detect) {

                pseg->num_detect = 1;
                pseg->detect_ts = msg->samp_data.spectral_tstamp;

            } else {

                pseg->num_detect++;

                if (pseg->num_detect >=3) {
                    if ((int)(msg->samp_data.spectral_tstamp - pseg->detect_ts) < GET_CW_INT_CONFIRM_WIN(pclas)) {

                        /* found 2 detect within given window */
                        if(!pclas->cw[!index].found_cw) {
                            pclas->current_interference |= SPECT_CLASS_DETECT_CW;
                        }
                        pseg->detect_ts = msg->samp_data.spectral_tstamp;
                        pclas->cw_cnt++;

                        /* Commit data into file */
                        spectral_scan_log_data(msg, pclas, SPECT_CLASS_DETECT_CW,1);
                        printf("Spectral Classifier: Found CW interference on segment %d \n",
                            index);
                        ret_val = 1;
                        pseg->found_cw = 1;
                    } else {
                        pseg->num_detect = 0;
                        pseg->detect_ts = msg->samp_data.spectral_tstamp;
                    }
                }
            }
        } else {
            /* Found after a long time */
            pseg->num_detect = 1;
            pseg->detect_ts = msg->samp_data.spectral_tstamp;
        }

    } else if ((pclas->current_interference & SPECT_CLASS_DETECT_CW) && (pseg->found_cw)) {

        /* Check if it has been found before and not found of some time */
        if ((int)(msg->samp_data.spectral_tstamp - pseg->detect_ts) > GET_CW_INT_CONFIRM_MISSING_WIN(pclas)) {
            if(!pclas->cw[!index].found_cw) {
                pclas->current_interference &=  ~(SPECT_CLASS_DETECT_CW);
            }
            pseg->num_detect = 0;
            printf("Spectral Classifier: No CW interference on segment %d \n", index);
            pseg->found_cw = 0;
        }
    }

    return ret_val;
}

/*
 * Function     : get_bin_offset
 * Description  : Get the bin offset for the given number of bins.
 * Input params : Loop count, number of bins
 * Return       : offset index
 *
 */

static int get_bin_offset(u_int32_t loop_cnt, u_int32_t num_bins)
{
    int index = 0;

    index = (num_bins * loop_cnt) - 1;

    if ((index < 0) || (index > NUM_FFT_BINS_80MHZ)) {
        index = 0;
    }

    return index;
}
/*
 * Function     : detect_wifi
 * Description  : Detect WiFi interference
 * Input params : Pointers to SAMP msg and classifier data
 * Return       : Found/Not Found
 *
 * INFO         : WiFi interference is Wide Band signal. Low on the edge and should
 *                high enough RSSI
 *
 *
 * FFT Size initialization done before starting spectral scan
 * ----------------------------------------------------------
 * 20MHz    - FFT Size is set to 7, FFT bin count is 64 for 11ac, 56 for legacy
 * 40MHz    - FFT Size is set ti 8, FFT bin count is 128
 * 80MHz    - FFT Size is set ti 9, FFT bin count is 256
 */

int detect_wifi(SPECTRAL_SAMP_MSG* msg, CLASSIFER_DATA_STRUCT* pclas)
{

    int found           = 0;
    int ch_width        = 0;
    u_int8_t *pfft_bins = NULL;
    u_int32_t num_bins  = 0;
    int index = 0;
    u_int32_t loop_cnt = 0;


    if (!(pclas->spectral_detect_mode & SPECT_CLASS_DETECT_WiFi)) {
        return 0;
    }

    /* get operating channel width */
    ch_width = msg->samp_data.ch_width;

    /* Check if there is high signal */
    if (msg->samp_data.spectral_rssi > GET_WIFI_DET_MIN_RSSI(pclas)) {

        if (ch_width == IEEE80211_CWM_WIDTH20) {

            /* check the first segment */
            pfft_bins   = (u_int8_t*)&msg->samp_data.bin_pwr[0];
            num_bins    = msg->samp_data.bin_pwr_count;   // can be 56 (legacy) or 64 (11ac)
            found       = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);

#if CLASSIFIER_DEBUG
            if (found) {
                cinfo("Detected WiFi (Channel Width 20MHz)");
            }
#endif  // CLASSIFIER_DEBUG

        } else if (ch_width == IEEE80211_CWM_WIDTH40) {

            /* check the first 20MHz segment */
            pfft_bins   = (u_int8_t*)&msg->samp_data.bin_pwr[0];
            num_bins    = NUM_FFT_BINS_20MHZ;
            found       = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);

#if CLASSIFIER_DEBUG
            if (found) {
                cinfo("Detected WiFi (Channel Width 40MHz : First 20MHz Segment)");
            }
#endif  // CLASSIFIER_DEBUG

            /* check the second 20MHz segment */
            if (!found) {
                pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr[63];
                found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
#if CLASSIFIER_DEBUG
                if (found) {
                  cinfo("Detected WiFi (Channel Width 40MHz : Second 20MHz Segment)");
                }
#endif  // CLASSIFIER_DEBUG
            }

            if (!found) {
                pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr[0];
                num_bins  = msg->samp_data.bin_pwr_count;
                found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
#if CLASSIFIER_DEBUG
                if (found) {
                    cinfo("Detected WiFi (Channel Width 40MHz : 40MHz Segment)");
                }
#endif  // CLASSIFIER_DEBUG
            }


        } else if (ch_width == IEEE80211_CWM_WIDTH80) {
            /* check the first segment */
            pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr[0];
            num_bins  = NUM_FFT_BINS_20MHZ;
            found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);

#if CLASSIFIER_DEBUG
            if (found) {
                cinfo("Detected WiFi (Channel Width 80MHz : First 20MHz Segment)");
            }
#endif  // CLASSIFIER_DEBUG

            /* check the second segment */
            if (!found) {
                pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr[63];
                num_bins = NUM_FFT_BINS_20MHZ;
                found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
#if CLASSIFIER_DEBUG
                if (found) {
                    cinfo("Detected WiFi (Channel Width 80MHz : Second 20MHz Segment)");
                }
#endif  // CLASSIFIER_DEBUG
            }

            /* check the third segment */
            if (!found) {
                pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr[127];
                num_bins = NUM_FFT_BINS_20MHZ;
                found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
#if CLASSIFIER_DEBUG
                if (found) {
                    cinfo("Detected WiFi (Channel Width 80MHz : Third 20MHz Segment)");
                }
#endif  // CLASSIFIER_DEBUG
            }


            /* check the fourth segment */
            if (!found) {
                pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr[191];
                num_bins = NUM_FFT_BINS_20MHZ;
                found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
#if CLASSIFIER_DEBUG
                if (found) {
                    cinfo("Detected WiFi (Channel Width 80MHz : Fourth 20MHz Segment)");
                }
#endif  // CLASSIFIER_DEBUG
            }

            if (!found) {
                pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr[0];
                num_bins  = NUM_FFT_BINS_40MHZ;
                found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
#if CLASSIFIER_DEBUG
                if (found) {
                    cinfo("Detected WiFi (Channel Width 80MHz : First 40MHz Segment)");
                }
#endif  // CLASSIFIER_DEBUG
            }

            if (!found) {
                pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr[0 + NUM_FFT_BINS_40MHZ - 1];
                num_bins  = NUM_FFT_BINS_40MHZ;
                found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
#if CLASSIFIER_DEBUG
                if (found) {
                    cinfo("Detected WiFi (Channel Width 80MHz : Second 40MHz Segment)");
                }
#endif  // CLASSIFIER_DEBUG
            }

            if (!found) {
                pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr[0];
                num_bins  = NUM_FFT_BINS_80MHZ;
                found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
#if CLASSIFIER_DEBUG
                if (found) {
                    cinfo("Detected WiFi (Channel Width 80MHz : 80MHz Segment)");
                }
#endif  // CLASSIFIER_DEBUG
            }


        } else if (ch_width == IEEE80211_CWM_WIDTH160) {
           for (loop_cnt = 0; loop_cnt < NUM_20MHZ_SEGMENT_IN_160MHZ; loop_cnt++) {
                if (loop_cnt < 4) {
                    /* check the first segment */
                    index = get_bin_offset(loop_cnt, NUM_FFT_BINS_20MHZ);
                    pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr[index];
                    num_bins  = NUM_FFT_BINS_20MHZ;
                    found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
                } else {
                    /* check the first segment */
                    index = get_bin_offset(loop_cnt - 4, NUM_FFT_BINS_20MHZ);
                    pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr_sec80[index];
                    num_bins  = NUM_FFT_BINS_20MHZ;
                    found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
                }

                if (found) {
#if CLASSIFIER_DEBUG
                    cinfo("Detected WiFi (Channel Width 160MHz : 20 MHz transmission)");
#endif  // CLASSIFIER_DEBUG
                    break;
                }

            }

            if (!found) {
                for (loop_cnt= 0; loop_cnt < NUM_40MHZ_SEGMENT_IN_160MHZ; loop_cnt++) {
                    if (loop_cnt < 2) {
                        index = get_bin_offset(loop_cnt, NUM_FFT_BINS_40MHZ);
                        pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr[index];
                        num_bins  = NUM_FFT_BINS_40MHZ;
                        found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
                    } else {
                        index = get_bin_offset(loop_cnt - 2, NUM_FFT_BINS_40MHZ);
                        pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr_sec80[index];
                        num_bins  = NUM_FFT_BINS_40MHZ;
                        found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
                    }

                    if (found) {
#if CLASSIFIER_DEBUG
                        cinfo("Detected WiFi (Channel Width 160MHz : 40 MHz transmission)");
#endif  // CLASSIFIER_DEBUG
                        break;
                    }
                }
            }

            if (!found) {
                pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr[0];
                num_bins  = NUM_FFT_BINS_80MHZ;
                found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
#if CLASSIFIER_DEBUG
                if (found) {
                    cinfo("Detected WiFi (Channel Width 160MHz : 80 MHz transmission in primary 80MHz segment)");
                }
#endif  // CLASSIFIER_DEBUG
            }

            if (!found) {
                pfft_bins = (u_int8_t*)&msg->samp_data.bin_pwr_sec80[0 + NUM_FFT_BINS_80MHZ - 1];
                num_bins  = NUM_FFT_BINS_80MHZ;
                found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
#if CLASSIFIER_DEBUG
                if (found) {
                    cinfo("Detected WiFi (Channel Width 160MHz : 80 MHz transmission in secondary 80MHz segment)");
                }
#endif  // CLASSIFIER_DEBUG
            }
            if (!found) {
                u_int8_t buf [2 * NUM_FFT_BINS_80MHZ];

                /* Discarding 4 bins on the edge */
                memcpy(buf,(u_int8_t*)&msg->samp_data.bin_pwr[3], NUM_FFT_BINS_80MHZ);

                memcpy(buf + NUM_FFT_BINS_80MHZ, (u_int8_t*)&msg->samp_data.bin_pwr_sec80, NUM_FFT_BINS_80MHZ);
                pfft_bins = buf;
                num_bins  = 2 * NUM_FFT_BINS_80MHZ;
                found = check_wifi_signal(pclas, num_bins, pfft_bins, ch_width);
#if CLASSIFIER_DEBUG
                if (found) {
                    cinfo("Detected WiFi (Channel Width 160MHz : 160MHz Segment)");
                }
#endif  // CLASSIFIER_DEBUG
            }
        } else if (ch_width == IEEE80211_CWM_WIDTHINVALID) {
            found = FALSE;
        }

        /* If WiFi detected log the data */
        if (found) {
            spectral_scan_log_data(msg, pclas, SPECT_CLASS_DETECT_WiFi, 0);
            pclas->spectral_num_wifi_detected++;
            pclas->wifi_rssi += msg->samp_data.spectral_rssi;

        }
    }

    if (found == TRUE) {
        found = FALSE;

        /* Check if there are at least 5 detects in a 500ms interval */
        if (!(pclas->current_interference & SPECT_CLASS_DETECT_WiFi)) {

            if (!pclas->wifi_num_detect) {

                /* First detect */
                pclas->wifi_num_detect = 1;
                pclas->wifi_detect_ts = msg->samp_data.spectral_tstamp;

            } else {

                pclas->wifi_num_detect++;

                /* 500ms has not elapsed */
                if ((int)(msg->samp_data.spectral_tstamp - pclas->wifi_detect_ts) < GET_WIFI_DET_CONFIRM_WIN(pclas)) {

                    if (pclas->wifi_num_detect >= WIFI_MIN_NUM_DETECTS) {

                        /* And there are 2 detects, set the WiFi interference flag to 1 */
                        pclas->current_interference |= SPECT_CLASS_DETECT_WiFi;
                        pclas->wifi_detect_ts       = msg->samp_data.spectral_tstamp;
                        pclas->wifi_rssi /= pclas->spectral_num_wifi_detected;

                        printf("Spectral Classifier: Found WiFi interference in freq %d with RSSI %d\n", msg->freq, pclas->wifi_rssi);

                        found = 1;
                        pclas->wifi_cnt++;

                        /* Commit the logged data */
                        spectral_scan_log_data(msg, pclas, SPECT_CLASS_DETECT_WiFi, 1);
                    }
                } else {
                    /* Took too much time, reset the counter */
                    pclas->wifi_num_detect = 1;
                    pclas->wifi_detect_ts = msg->samp_data.spectral_tstamp;
                }
            }
        } else {
            /* Check if a positive detect happend after a long time */
            if ((int)(msg->samp_data.spectral_tstamp - pclas->wifi_detect_ts) > GET_WIFI_DET_RESET_TIME(pclas)) {

                /* Too much time, reset and check again */
                pclas->current_interference &=  ~(SPECT_CLASS_DETECT_WiFi);
                pclas->wifi_num_detect = 0;
                printf("Spectral Classifier: No WiFi interference\n");
            } else {
                /* One more detected, reset the time */
                pclas->wifi_num_detect = 1;
                pclas->wifi_detect_ts = msg->samp_data.spectral_tstamp;
            }
        }
    } else if (pclas->current_interference & SPECT_CLASS_DETECT_WiFi) {
        /* Check if it has been found before and not found of some time */
        if ((int)(msg->samp_data.spectral_tstamp - pclas->wifi_detect_ts) > GET_WIFI_DET_RESET_TIME(pclas)) {
            pclas->current_interference &=  ~(SPECT_CLASS_DETECT_WiFi);
            pclas->wifi_num_detect = 0;
            printf("Spectral Classifier: No WiFi interference\n");
        }
    }
    return found;
}

/*
 * Function     : check_wifi_signal
 * Description  : checks for WiFi signal pattern in the given FFT bins
 * Input params : Pointers to classifier data, pointer to fft bins, number of fft bins
 * Return       : Found/Not Found
 *
 * INFO         : WiFi interference is Wide Band signal. Low on the edge and should
 *                high enough RSSI
 *
 */
int check_wifi_signal(CLASSIFER_DATA_STRUCT* pclas, u_int32_t num_bins, u_int8_t* pfft_bins, u_int32_t ch_width)
{
    u_int16_t peak_val = 0;
    u_int16_t num_bins_above_threshold = 0;
    int i = 0;
    int found = FALSE;
    int peak_val_threshold = 0;

    /* find the peak value in the given bins */
    for (i = 0; i < num_bins; i++) {
        if (peak_val < pfft_bins[i]) {
            peak_val = pfft_bins[i];
        }
    }

    /* check how many bins are above peak bins are above threshold */
    /* Peak val threhold is set to 25% of peak value */
    peak_val_threshold = peak_val >> 2;

    for (i = 0; i < num_bins; i++) {
        if (pfft_bins[i] >= peak_val_threshold) {
            num_bins_above_threshold++;
        }
    }

    /* if at least half of the bins are greater than or equal to the peak value - 6dB */
    if (num_bins_above_threshold >= (num_bins >> 1)) {

        u_int32_t start_sum     = 0;
        u_int32_t mid_sum       = 0;
        u_int32_t end_sum       = 0;
        u_int32_t mid_bin_index = 0;

        mid_bin_index = (num_bins >> 1) - (GET_WIFI_BIN_WIDTH(pclas) >> 1) - 1;

        for (i = 0; i < (GET_WIFI_BIN_WIDTH(pclas)); i++) {
            start_sum   += pfft_bins[i];
            end_sum     += pfft_bins[num_bins - i -1];
            mid_sum     += pfft_bins[mid_bin_index + i - 1];
        }

        if (((int)(mid_sum - end_sum) > GET_WIFI_DET_MIN_DIFF(pclas)) &&
            ((int)(mid_sum - start_sum) > GET_WIFI_DET_MIN_DIFF(pclas))) {
            /* Most likely WiFi Signal */
            found = TRUE;
        }
    }

    return found;
}

/*
 * Function     : detect_fhss
 * Description  : Detect FHSS Interference
 * Input params : Pointer to SAMP msg and classifier data
 * Return       : Found/Not Found
 *
 * INFO         : FHSS in narrow band signal. Frequency hopping signals dwell for fixed amount
 *                of time (10ms) in single channel.
 *
 */
int detect_fhss(SPECTRAL_SAMP_MSG* msg, CLASSIFER_DATA_STRUCT *pclas, uint8_t use_sec80)
{
    int ret_val = 0;
    int index = (use_sec80)?1:0;    /* Index to determine if classifier operates on segment 0 or segment 1 */
    CLASSIFIER_FHSS_PARAMS *pseg = &pclas->fhss[index];
    spectral_fhss_param *cur_fhss = &pseg->fhss_param[pseg->cur_bin];
    int16_t     spectral_rssi; /* This change to signed int16 is to fix the negative rssi issue */
    u_int16_t   bin_pwr_count;
    u_int8_t *bin_pwr = NULL;

    if (!(pclas->spectral_detect_mode & SPECT_CLASS_DETECT_FHSS)) {
        return ret_val;
    }

    if (!use_sec80) {
        spectral_rssi = msg->samp_data.spectral_rssi;
        bin_pwr_count = msg->samp_data.bin_pwr_count;
        bin_pwr = (u_int8_t *)msg->samp_data.bin_pwr;
    } else {
        spectral_rssi = msg->samp_data.spectral_rssi_sec80;
        bin_pwr_count = msg->samp_data.bin_pwr_count_sec80;
        bin_pwr = (u_int8_t *)msg->samp_data.bin_pwr_sec80;
    }

    if ((spectral_rssi > GET_FHSS_DET_THRESH(pclas)) && (!(pclas->current_interference & SPECT_CLASS_DETECT_MWO))) {

        /* something in the air */
        u_int16_t bin_cnt       = 0;
        u_int16_t peak_bin      = 0;
        u_int16_t peak_val      = 0;
        u_int16_t chk_upr       = 0;
        u_int16_t chk_lwr       = 0;
        u_int16_t upr_bin       = 0;
        u_int16_t lwr_bin       = 0;
        u_int32_t upr_sum       = 0;
        u_int32_t lwr_sum       = 0;
        u_int32_t center_sum    = 0;
        u_int16_t peak_upper    = 0;
        u_int16_t peak_lower    = 0;

        /* Do a peak search and figure out the max data */
        for (bin_cnt = 0; bin_cnt < bin_pwr_count; bin_cnt++ ) {
            if (peak_val < bin_pwr[bin_cnt]) {
                peak_bin = bin_cnt;
                peak_val = bin_pwr[bin_cnt];
            }
        }

        /* Check how many bins we can compare with */
        if (peak_bin + (GET_FHSS_INT_BIN_SUM_SIZE(pclas) + (GET_FHSS_INT_BIN_SUM_SIZE(pclas) >> 1)) <= msg->samp_data.bin_pwr_count) {
            chk_upr = 1;
        }

        if ((int)peak_bin - (GET_FHSS_INT_BIN_SUM_SIZE(pclas) + (GET_FHSS_INT_BIN_SUM_SIZE(pclas) >> 1)) <= 0) {
            chk_lwr = 1;
        }


        /* XXX : Note, this logic works only for GET_FHSS_INT_BIN_SUM_SIZE(pclas) = 3 */
        /* set the upper and lower bin markers */
        if (peak_bin == bin_pwr_count) {

            upr_bin     = peak_bin;
            lwr_bin     = peak_bin - 2;
            peak_upper  = peak_bin;
            peak_lower  = peak_bin - 2;

        } else if (peak_bin == 0) {

            upr_bin     = peak_bin + 2;
            lwr_bin     = peak_bin;
            peak_upper  = peak_bin + 2;
            peak_lower  = peak_bin;

        } else {

            upr_bin     = peak_bin + 1;
            lwr_bin     = peak_bin - 1;
            peak_upper  = peak_bin + 2;
            peak_lower  = peak_bin - 2;

        }

        center_sum = bin_pwr[lwr_bin] +
                bin_pwr[lwr_bin + 1] +
                bin_pwr[lwr_bin + 2];

        if (chk_upr) {
                upr_sum = bin_pwr[upr_bin + 1] +
                        bin_pwr[upr_bin + 2] +
                        bin_pwr[upr_bin + 3];
        }

        if (chk_lwr) {
                lwr_sum = bin_pwr[lwr_bin - 1] +
                        bin_pwr[lwr_bin - 2] +
                        bin_pwr[lwr_bin - 3];
        }

        /* Check if this is greater than threshold
         * XXX : Note, this logic works only for GET_FHSS_INT_BIN_SUM_SIZE(pclas) = 3
         */
        if ( (lwr_sum < (center_sum >> GET_FHSS_SUM_SCALE_DOWN_FACTOR(pclas))) &&
             (upr_sum < (center_sum >> GET_FHSS_SUM_SCALE_DOWN_FACTOR(pclas))) &&
             (center_sum > GET_FHSS_CENTER_THRESH(pclas)) ) {

            cur_fhss = &pseg->fhss_param[pseg->cur_bin];

            /* Log the burst as possible FHSS */
            spectral_scan_log_data(msg, pclas, SPECT_CLASS_DETECT_FHSS, 0);

            /* Check if this condition has lasted long enough */
            if (!cur_fhss->in_use) {

                /* This is being used for the first time */

                cur_fhss->in_use    = 1;
                cur_fhss->start_ts  = msg->samp_data.spectral_tstamp;
                cur_fhss->freq_bin  = peak_bin;
                cur_fhss->last_ts   = msg->samp_data.spectral_tstamp;
                cur_fhss->rssi      += spectral_rssi;
                cur_fhss->num_samp++;

            } else {

                /* This has been in use, check if this could be a new burst */
                if ( (cur_fhss->freq_bin < peak_upper) &&
                     (cur_fhss->freq_bin > peak_lower)) {

                    /* This is a current burst, check if this has been on for too long */
                    if ((int)(msg->samp_data.spectral_tstamp - cur_fhss->start_ts) > GET_FHSS_SINGLE_BURST_TIME(pclas)) {
                        /* This burst has been there for more then 15sec, it cannot be a FHSS burst */

                        spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_FHSS, index);
                    } else {
                        /* Store the last time stamp */

                        cur_fhss->last_ts   =  msg->samp_data.spectral_tstamp;
                        cur_fhss->rssi      += spectral_rssi;
                        cur_fhss->num_samp++;
                    }

                } else {

                    /* Try putting it in the next bin */
                    cur_fhss->delta = (int)(cur_fhss->last_ts - cur_fhss->start_ts);

                    /* If the delta is too short, just reuse the bin, it is a fake signal */
                    if (cur_fhss->delta > GET_FHSS_MIN_DWELL_TIME(pclas)) {

                        pseg->cur_bin++;

                        if (pseg->cur_bin == NUM_FHSS_BINS) {
                            /* All bins are full, search to see if there is a possible FHSS */
                            u_int32_t avg_delta         = 0;
                            u_int32_t fir_delta         = pseg->fhss_param[1].delta;
                            u_int32_t fir_min           = fir_delta - (fir_delta >> 2);
                            u_int32_t fir_max           = fir_delta + (fir_delta >> 2);
                            u_int16_t num_fhss_burst    = 0;
                            u_int32_t tot_burst         = 0;
                            int32_t tot_rssi          = 0;

                            /* Check if the dwell time is about the minimum
                            * TODO: Not needed because already weeded out
                            */
                            if (fir_delta > GET_FHSS_MIN_DWELL_TIME(pclas)) {
                                /* loop through the dwell time and see if the if the dwell time is about the same */
                                for (bin_cnt = 0; bin_cnt < NUM_FHSS_BINS; bin_cnt++) {
                                    if ((pseg->fhss_param[bin_cnt].delta > fir_min) &&
                                        (pseg->fhss_param[bin_cnt].delta < fir_max)) {
                                        /* dwell time is about the same */
                                        avg_delta   += pseg->fhss_param[bin_cnt].delta;
                                        tot_rssi    += pseg->fhss_param[bin_cnt].rssi;
                                        tot_burst   += pseg->fhss_param[bin_cnt].num_samp;
                                        num_fhss_burst++;
                                    }
                                }

                                if (num_fhss_burst > (NUM_FHSS_BINS >> 1)) {
                                    /* At least 1/2 the bins have about the same dwell time and hence declare
                                    * FHSS burst detection
                                    */
                                    avg_delta /= num_fhss_burst;
                                    tot_rssi /= tot_burst;
                                    //printf("Avg RSSI = %d total Burst %d Avg delta = %d\n",
                                    //    tot_rssi, tot_burst, avg_delta);

                                    ret_val = 1;
                                    /* Log the data */
                                }
                            }

                            spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_FHSS, index);
                        }

                        /* Put the data back in the same bin */
                        if (pseg->cur_bin <= NUM_FHSS_BINS) {
                            cur_fhss = &pseg->fhss_param[pseg->cur_bin];
                            cur_fhss->in_use    = 1;
                            cur_fhss->start_ts  = msg->samp_data.spectral_tstamp;
                            cur_fhss->freq_bin  = peak_bin;
                            cur_fhss->last_ts   = msg->samp_data.spectral_tstamp;
                        } else {
                            printf("Spectral Classifier: Array out of bound error \n");
                        }

                    } else {

                        /* add a new bin with different frequency */
                        cur_fhss = &pseg->fhss_param[pseg->cur_bin];
                        cur_fhss->in_use    = 1;
                        cur_fhss->start_ts  = msg->samp_data.spectral_tstamp;
                        cur_fhss->freq_bin  = peak_bin;
                        cur_fhss->last_ts   = msg->samp_data.spectral_tstamp;
                    }
                }
            }

        } else {

            /* Not a narrow band burst, check if it has been absent for too long */
            /* This is a current burst, check if this has been on for too long */
            if ((int)(msg->samp_data.spectral_tstamp - cur_fhss->start_ts) > GET_FHSS_LACK_OF_BURST_TIME(pclas)) {
                /* This burst has been there for more then 15sec, it cannot be a FHSS burst */
                spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_FHSS, index);
            }
        }

    } else {

        /* No burst for a long time */
        /* This is a current burst, check if this has been on for too long */
        if ((int)(msg->samp_data.spectral_tstamp - cur_fhss->start_ts) > GET_FHSS_LACK_OF_BURST_TIME(pclas)) {
            /* This burst has been there for more then 15sec, it cannot be a FHSS burst */
            spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_FHSS, index);
        }
    }

    if (ret_val) {

        ret_val = 0;

        /* Check if at least 2 detects happen in given amount of time */
        if (!(pclas->current_interference & SPECT_CLASS_DETECT_FHSS) || (!pseg->found_fhss)) {

            if (!pseg->num_detect) {
                pseg->num_detect = 1;
                pseg->detect_ts  = msg->samp_data.spectral_tstamp;
            } else {

                pseg->num_detect++;

                if ((int)(msg->samp_data.spectral_tstamp - pseg->detect_ts) < GET_FHSS_DETECTION_CONFIRM_WIN(pclas)) {

                    if(!pclas->fhss[!index].found_fhss) {
                        pclas->current_interference |= SPECT_CLASS_DETECT_FHSS;
                    }
                    pseg->detect_ts = msg->samp_data.spectral_tstamp;

                    printf("Spectral Classifier: Found FHSS interference on segment %d \n", index);
                    pclas->fhss_cnt++;
                    pseg->found_fhss = 1;

                    /* commit the logged data */
                    spectral_scan_log_data(msg, pclas, SPECT_CLASS_DETECT_FHSS, 1);

                } else {

                    pseg->num_detect = 0;
                    pseg->detect_ts = msg->samp_data.spectral_tstamp;

                }
            }
        } else {

            /* Update the time */
            pseg->num_detect = 1;
            pseg->detect_ts = msg->samp_data.spectral_tstamp;

        }

    } else if ((pclas->current_interference & SPECT_CLASS_DETECT_FHSS) && (pseg->found_fhss)) {

        /* Check if it has been found before and not found of some time */
        if ((int)(msg->samp_data.spectral_tstamp - pseg->detect_ts) > GET_FHSS_DETECTION_RESET_WIN(pclas)) {
            if(!pclas->fhss[!index].found_fhss) {
                pclas->current_interference &=  ~(SPECT_CLASS_DETECT_FHSS);
            }
            pseg->num_detect      = 0;
            printf("Spectral Classifier: No FHSS interference on segment %d \n", index);
            pseg->found_fhss = 0;
        }
    }
    return ret_val;
}

/*
 * Function     : ether_sprintf
 * Description  : format MAC address for printing
 * Input params : pointer to mac address
 * Return       : formatted string
 *
 */

const char* ether_sprintf(const u_int8_t *mac)
{
    static char etherbuf[18];
    snprintf(etherbuf, sizeof(etherbuf), "%02x:%02x:%02x:%02x:%02x:%02x",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return etherbuf;
}


/*
 * Function     : print_spect_int_stats
 * Description  : Print spectral interference statas
 * Input params : Void
 * Return       : Void
 *
 */
extern void print_spect_int_stats()
{
    int i = 0;
    CLASSIFER_DATA_STRUCT *pclas;

    for (i = 0; i < CLASSIFIER_HASHSIZE; i++) {
        pclas = &class_data[i];

        if (pclas->is_valid) {
            printf("\nInterface = %s\n", ether_sprintf((const u_int8_t*)pclas->macaddr));
            printf("-----------------------------------------------\n");
            printf(" Number of MWO detection  %d\n"
                   " Number of WiFi detection %d\n"
                   " Number of FHSS detection %d\n"
                   " Number of CW detection   %d\n",
                    pclas->mwo_cnt,
                    pclas->wifi_cnt,
                    pclas->fhss_cnt,
                    pclas->cw_cnt);
        } /* end if */

    } /* end for */
}

/*
 * Function     : set_log_type
 * Description  : Set the type of info to log for debugging purpose
 * Input params : Pointer to classifier data structure, type of logging
 * Return       : Void
 *
 */
void set_log_type(CLASSIFER_DATA_STRUCT *pclas, u_int16_t log_type)
{
    switch (log_type) {
       case LOG_MWO:
           pclas->log_mode = SPECT_CLASS_DETECT_MWO;
           break;
       case LOG_CW:
           pclas->log_mode = SPECT_CLASS_DETECT_CW;
           break;
       case LOG_WIFI:
           pclas->log_mode = SPECT_CLASS_DETECT_WiFi;
           break;
       case LOG_FHSS:
           pclas->log_mode = SPECT_CLASS_DETECT_FHSS;
           break;
       case LOG_ALL:
           pclas->log_mode = SPECT_CLASS_DETECT_ALL;
           break;
       default:
           pclas->log_mode = SPECT_CLASS_DETECT_NONE;
           break;
    }
}

/*
 * Function     : classifier_process_spectral_msg
 * Description  : Process the incoming SAMP message
 * Input params : Pointers to SAMP msg, classifier struct, log type, and whether
 *                to enable linear scaling for Gen3 chipsets
 * Return       : Void
 *
 */
void classifier_process_spectral_msg(SPECTRAL_SAMP_MSG* msg,
        CLASSIFER_DATA_STRUCT *pclas, u_int16_t log_type,
        bool enable_gen3_linear_scaling)
{
    u_int16_t bin_cnt = 0;
    u_int32_t temp_scaled_binmag = 0;

    /* validate */
    if (msg->signature != SPECTRAL_SIGNATURE) {
        return;
    }

    /*
     * For gen3 chipsets, the sample should be discarded if gain change is
     * indicated.
     */
    if ((pclas->caps.hw_gen == SPECTRAL_CAP_HW_GEN_3) &&
            msg->samp_data.spectral_gainchange) {
        return;
    }

    /* Mark as valid */
    pclas->is_valid = TRUE;

    /* Store the interface mac address */
    memcpy(pclas->macaddr, msg->macaddr, MAC_ADDR_LEN);

    if (enable_gen3_linear_scaling &&
            (pclas->caps.hw_gen == SPECTRAL_CAP_HW_GEN_3)) {
        /*
         * Scale the gen3 bins to values approximately similar to those of
         * gen2.
         */

        for (bin_cnt = 0; bin_cnt < msg->samp_data.bin_pwr_count; bin_cnt++)
        {
            /*
             * Note: Currently, we pass the same gen2 and gen3 bin_scale
             * values to the scaling formula, since for our algorithms we need
             * to scale for similar settings between gen2 and gen3. However, the
             * formula currently ignores the bin_scale values if they are the
             * same.
             * So we pass a bin_scale value of 1 for both gen2 and gen3 (which
             * is the recommended value for our algorithms anyway).
             *
             * A change can be added later to dynamically determine bin scale
             * values to be used.
             */

            /*
             * TODO: Get default max gain value, low level offset, RSSI
             * threshold, and high level offset from Spectral capabilities
             * structure once these are added there.
             */
            temp_scaled_binmag =
                spectral_scale_linear_to_gen2(msg->samp_data.bin_pwr[bin_cnt],
                        SPECTRAL_QCA9984_MAX_GAIN,
                        SPECTRAL_IPQ8074_DEFAULT_MAX_GAIN_HARDCODE,
                        SPECTRAL_SCALING_LOW_LEVEL_OFFSET,
                        msg->samp_data.spectral_rssi,
                        SPECTRAL_SCALING_RSSI_THRESH,
                        msg->samp_data.spectral_agc_total_gain,
                        SPECTRAL_SCALING_HIGH_LEVEL_OFFSET,
                        1,
                        1);

            msg->samp_data.bin_pwr[bin_cnt] =
                (temp_scaled_binmag > 255) ? 255: temp_scaled_binmag;
        }

        if (msg->samp_data.ch_width == IEEE80211_CWM_WIDTH160) {
            for (bin_cnt = 0;
                 bin_cnt < msg->samp_data.bin_pwr_count_sec80;
                 bin_cnt++)
            {
                /* See note for pri80 above regarding bin_scale values. */

                /*
                 * TODO: Get default max gain value, low level offset, RSSI
                 * threshold, and high level offset from Spectral capabilities
                 * structure once these are added there.
                 */
                temp_scaled_binmag =
                    spectral_scale_linear_to_gen2(\
                          msg->samp_data.bin_pwr_sec80[bin_cnt],
                          SPECTRAL_QCA9984_MAX_GAIN,
                          SPECTRAL_IPQ8074_DEFAULT_MAX_GAIN_HARDCODE,
                          SPECTRAL_SCALING_LOW_LEVEL_OFFSET,
                          msg->samp_data.spectral_rssi_sec80,
                          SPECTRAL_SCALING_RSSI_THRESH,
                          msg->samp_data.spectral_agc_total_gain_sec80,
                          SPECTRAL_SCALING_HIGH_LEVEL_OFFSET,
                          1,
                          1);

                msg->samp_data.bin_pwr_sec80[bin_cnt] =
                   (temp_scaled_binmag > 255) ? 255: temp_scaled_binmag;
            }
        }
    }

    if (!pclas->sm_init_done) {
        /* Initialize the classifier state machine */
        spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_ALL, 0);
        /* Set the log type */
        set_log_type(pclas, log_type);
    }

    /* Initialize the classifier state machine, if the operating frequency has changed */
    if (pclas->cur_freq != msg->freq) {
        spectral_scan_classifer_sm_init(pclas, SPECT_CLASS_DETECT_ALL, 0);
        pclas->cur_freq = msg->freq;
    }

    /* Log the Spectral data for debugging */
    spectral_scan_log_data(msg, pclas, SPECT_CLASS_DETECT_ALL, 0);

    /* Detect interference sources */
    detect_mwo(msg, pclas);
    if (msg->samp_data.ch_width == IEEE80211_CWM_WIDTH160) {

        detect_cw(msg, pclas, 0);
        detect_cw(msg, pclas, 1);

        detect_fhss(msg, pclas, 0);
        detect_fhss(msg, pclas, 1);

        detect_wifi(msg, pclas);
    } else {
        detect_cw(msg, pclas, 0);
        detect_fhss(msg, pclas, 0);
        detect_wifi(msg, pclas);
    }
}

/*
 * Function     : print_detected_interference
 * Description  : Print the type of interference detected
 * Input params : Pointer to classifier struct
 * Return       : Void
 *
 */
void print_detected_interference(CLASSIFER_DATA_STRUCT* pclas)
{

    if (IS_MWO_DETECTED(pclas)) {
        printf("MWO Detected\n");
    }

    if (IS_CW_DETECTED(pclas)) {
        printf("CW Detected\n");
    }

    if (IS_WiFi_DETECTED(pclas)) {
        printf("WiFi Detected\n");
    }

    if (IS_CORDLESS_24_DETECTED(pclas)) {
        printf("CP (2.4GHZ) Detected\n");
    }

    if (IS_CORDLESS_5_DETECTED(pclas)) {
        printf("CP (5GHz) Detected\n");
    }

    if (IS_BT_DETECTED(pclas)) {
        printf("BT Detected\n");
    }

    if (IS_FHSS_DETECTED(pclas)) {
        printf("FHSS Detected\n");
    }

}


