#ifndef WIN32
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <linux/types.h>
#else
#include "spectral_types.h"
#endif

#include "classifier.h"
#include <spectral_data.h>
#include <spec_msg_proto.h>

/* HACK */
int BTH_MIN_NUMBER_OF_FRAMES=8;

int detects_extension_channel;
int detects_control_channel;
int detects_below_dc;
int detects_above_dc;

void classifier(struct ss *bd, int timestamp, int last_capture_time, int rssi, int narrowband, int peak_index);
void nb_classifier(int timestamp, struct ss *bd);
void wb_classifier(int timestamp, struct ss *bd);
void reset_vbr(struct ss *bd);
void reset_bth(struct ss *bd);
void reset_bts(struct ss *bd);
void reset_cph(struct ss *bd);
void reset_cwa(struct ss *bd);
void reset_mwo(struct ss *bd);
void reset_mwo_long(struct ss *bd);
void reset_nb_counters(struct ss *bd);
int mean_value(int* input_array, int array_length);



void init_bandinfo(struct ss *plwrband, struct ss *puprband, int print_enable)
{
    /*  initialize  */

    plwrband->max_bin = INT_MIN;
    plwrband->min_bin = INT_MAX;
    puprband->max_bin = INT_MIN;
    puprband->min_bin = INT_MAX;
    plwrband->count_vbr = 0;
    puprband->count_vbr = 0;
    plwrband->count_bth = 0;
    puprband->count_bth = 0;
    plwrband->count_bts = 0;
    puprband->count_bts = 0;
    plwrband->count_cwa = 0;
    puprband->count_cwa = 0;
    plwrband->count_wln = 0;
    puprband->count_wln = 0;
    plwrband->count_mwo = 0;
    puprband->count_mwo = 0;
    plwrband->count_cph = 0;
    puprband->count_cph = 0; 
    plwrband->num_identified_short_bursts = 0;
    puprband->num_identified_short_bursts = 0;
    plwrband->num_identified_bursts = 0;
    puprband->num_identified_bursts = 0;
    plwrband->previous_nb = 0;
    puprband->previous_nb = 0;
    plwrband->previous_wb = 0;
    puprband->previous_wb = 0;
    plwrband->prev_burst_stop = 0;
    puprband->prev_burst_stop = 0;
    plwrband->average_rssi_total = 0;
    puprband->average_rssi_total = 0;
    plwrband->average_rssi_numbr = 1;
    puprband->average_rssi_numbr = 1;
    plwrband->print_to_screen = print_enable;
    puprband->print_to_screen = print_enable;
}

void classifier(struct ss *bd, int timestamp, int last_capture_time, int rssi, int narrowband, int peak_index) 
{
    static int print_once=0;

  int high_rssi, delay, burst_not_at_edge_ht20, burst_not_at_edge_ht40; 
  
  delay = timestamp - last_capture_time;
  high_rssi = rssi > MINIMUM_RSSI;

  if(!print_once) {
    //printf("%s timestamp=%d last_capture=%d rssi=%d nb=%d peak_index=%d\n", __func__,timestamp, last_capture_time, rssi, narrowband, peak_index);
    print_once=1;
   }

  if ( (delay > 0) && (delay < MAXIMUM_SAMPLING_INTERVAL) ) {
    bd->narrowband = narrowband;
    bd->wideband = !narrowband;
    bd->peak_index = peak_index;
  
    if ( (bd->narrowband == 1) && high_rssi ) {
      bd->previous_nb = bd->previous_nb + 1;
      bd->current_burst_rssi = bd->current_burst_rssi + rssi;
      bd->current_burst_stop = timestamp;
      if (bd->previous_nb == 1) {
        bd->current_burst_start_time = timestamp;
      } else {
        bd->current_burst_width = timestamp - bd->current_burst_start_time;
      }

      bd->max_bin = 
        (bd->peak_index > bd->max_bin) ?  bd->peak_index : bd->max_bin;
      bd->min_bin = 
        (bd->peak_index < bd->min_bin) ?  bd->peak_index : bd->min_bin;

    } else { // wideband or no-signal
        if (bd->previous_nb > MIN_PREVIOUS_NB) {
          bd->do_burst_check = 1;
        } else {
          reset_nb_counters(bd);
        }
    }

    // bd->wideband
    if ( (bd->wideband == 1) && high_rssi ) {
      bd->previous_wb = bd->previous_wb + 1;
      bd->current_burst_stop_wb = timestamp;
      if (bd->previous_wb == 1) {
        bd->current_burst_start_time_wb = timestamp;
      } else {
        bd->current_burst_width_wb = timestamp - bd->current_burst_start_time_wb;
      }
      bd->current_burst_rssi_wb = bd->current_burst_rssi_wb + rssi;
      //bd->current_burst_wb_sum1 += sum1;
      //bd->current_burst_wb_sum2 += sum2;
      //bd->current_burst_wb_sum3 += sum3;
      //bd->current_burst_wb_sum4 += sum4;
    } else { // narrowband or no-signal
      if (bd->previous_wb > MIN_PREVIOUS_WB) {
        bd->do_burst_check_wb = 1;
      } else {
        //printf("Before (previous_wb) %d, now 0\n", bd->previous_wb);
        bd->previous_wb = 0;
        bd->current_burst_rssi_wb = 0;
        bd->current_burst_wb_sum1 = 0;
        bd->current_burst_wb_sum2 = 0;
        bd->current_burst_wb_sum3 = 0;
        bd->current_burst_wb_sum4 = 0;
      }
    }

  } else if (delay > 0) { // Big jump detected
    // last_capture_time = timestamp;
    if (bd->previous_nb > MIN_PREVIOUS_NB) {
      bd->do_burst_check = 1;
    } else {
      reset_nb_counters(bd);
    }

    if (bd->previous_wb > MIN_PREVIOUS_WB) {
      bd->do_burst_check_wb = 1;
    } else {
      bd->previous_wb = 0;
      bd->current_burst_rssi_wb = 0;
      bd->current_burst_wb_sum1 = 0;
      bd->current_burst_wb_sum2 = 0;
      bd->current_burst_wb_sum3 = 0;
      bd->current_burst_wb_sum4 = 0;
    }
  }

  // For CW, don't wait until the burst ends.
  if (bd->current_burst_width > CWA_WIDTH_THRESHOLD) {
    bd->do_burst_check = 1;
  }

  // check to see whether the detected burst is at the edge of the spectrum. 
  // Somehow these are false alarms. TBD.
  if (bd->do_burst_check) {
    burst_not_at_edge_ht20 = ( (bd->max_bin > 5) && (bd->min_bin < 52 )  ) && (bd->dynamic_ht2040_en == 0);
    burst_not_at_edge_ht40 =  (bd->max_bin > 5)  && (bd->dynamic_ht2040_en == 1);
    if ( burst_not_at_edge_ht20 || burst_not_at_edge_ht40 ) {
      bd->do_burst_check = 1;
    } else {
       //printf("Burst at edge... ignoring..\n");
      bd->do_burst_check = 0;
      reset_nb_counters(bd);
    }

    // Remove DC Bursts fo HT20
    if ( (!bd->dynamic_ht2040_en) && (bd->max_bin < (bd->dc_index+2)) && (bd->min_bin > (bd->dc_index-2)) ) {
      bd->do_burst_check = 0;
      reset_nb_counters(bd);
    }
    //printf("Burst not at edge %d ", burst_not_at_edge_ht20);
  }

  // FINAL BURST CHECK ###
  if ( bd->do_burst_check == 1 ) {
    nb_classifier(timestamp, bd);
    reset_nb_counters(bd);
  } // END OF DO_BURST_CHECK


  bd->do_burst_check = 0;


  if ( bd->do_burst_check_wb == 1 ) {
     //printf("Wideband Burst Width = %d\n", bd->current_burst_width_wb);
    //Wed Oct 22 11:14:37 PDT 2008 - no wideband detection
    //wb_classifier(timestamp, bd);

    bd->previous_wb = 0;
    bd->current_burst_rssi_wb = 0;
    bd->current_burst_wb_sum1 = 0;
    bd->current_burst_wb_sum2 = 0;
    bd->current_burst_wb_sum3 = 0;
    bd->current_burst_wb_sum4 = 0;
  }


  bd->do_burst_check_wb = 0;
}

void nb_classifier(int timestamp, struct ss *bd)
{
  
  int i;
  int found_bts, found_bth, found_cph, found_mwo, found_mwo_long_burst, found_cwa, found_cwa_long_burst, found_vbr;
  int this_burst_is_mwo_narrowband, this_burst_is_narrowband, detected_frequency_khz;
  int burst01_gap, burst01_small_gap, bursts_are_atmost_4mhz_away, bursts_are_in_freq_range;
  int something_detected, ms10, ms20, ms30, conseq_burst_diff1, test_large_burstwidth800; 
  int test_large_burstwidth950, test_large_burstwidth600, test_large_burstwidth500; 
  //Thu Sep 25 14:34:32 PDT 2008 - remove unused variables
  //char lower_or_upper;
  //int max_gap_between_detects, test_mwo_proper_separation01, test_mwo_proper_separation12, test_mwo_proper_separation23;
  //int test_freq12_changes, test_freq23_changes, test_freq13_changes, test_freq03_changes;
  int test_freq12_changes, test_freq13_changes, test_freq03_changes;
  int test_mwo_proper_separation01, test_mwo_proper_separation12, test_mwo_proper_separation23;
  int one_handset, two_handset, mwo_bursts23_in_range, mwo_bursts12_in_range, mwo_bursts01_in_range;
  int test_cph_time_diff_ok, test_cph_width, test_freq01_changes, test_freq02_changes;
  int test_bts_time_diff_ok, conseq_burst_diff01, conseq_burst_diff23, conseq_burst_diff12;
  int test_bth_diff12_large, test_bth_time_diff_ok, test_cph_closewidth_bursts, test_bts_burst_sep; 
  int mwo_freq_changes01, mwo_freq_changes12, mwo_frequency_bins;
  burst01_gap = burst01_small_gap = 0;
  conseq_burst_diff01 = conseq_burst_diff12 = conseq_burst_diff23 = 0;
  mwo_freq_changes01 = mwo_freq_changes12 = mwo_frequency_bins = 0;
  something_detected = 0;
  
  

  found_cwa_long_burst = found_mwo_long_burst = found_vbr = 0;
  found_bts = found_bth = found_cph = found_mwo = found_cwa = 0;

  /*
  if (bd->lwr_band_data == 1) {
    lower_or_upper = "L"; // lower or upper
  } else {
    lower_or_upper = "U";
  }
  */

  this_burst_is_mwo_narrowband = (bd->max_bin - bd->min_bin) < MWO_MAX_BINSPAN;
  this_burst_is_narrowband = (bd->max_bin - bd->min_bin) < SINGLE_FREQUENCY_MAX_BINSPAN;
   //printf("%d %d %d - ", bd->current_burst_width, bd->max_bin, bd->min_bin );

  // Find the frequency of each burst
  if (bd->dynamic_ht2040_en){
    if (bd->lwr_band_data) {
      detected_frequency_khz = bd->b_chan_in_mhz*1000 + ( (bd->max_bin+bd->min_bin)/2 - bd->dc_index)*(SAMPLING_FREQUENCY/128)/1000;
    } else {
      detected_frequency_khz = bd->b_chan_in_mhz*1000 + ( (bd->max_bin+bd->min_bin)/2 )*(SAMPLING_FREQUENCY/128)/1000;
    }
  } else {
    detected_frequency_khz = bd->b_chan_in_mhz*1000 + ( (bd->max_bin+bd->min_bin)/2 - bd->dc_index)*(SAMPLING_FREQUENCY/128)/1000;
  }
   //printf("detected freq = %d\n", detected_frequency_khz);
#if 0
  //##############################################################################
  //#####                  DETECTION OF VIDEO BRIDGE                         ######
  //###############################################################################
  burst01_gap = ( bd->current_burst_stop - bd->prev_burst_stop - bd->current_burst_width ) ;
  burst01_small_gap = burst01_gap < VBR_MAX_GAP;
  bd->prev_burst_stop = bd->current_burst_stop;

  bd->burst2_freq_bin = bd->burst1_freq_bin;
  bd->burst1_freq_bin = bd->burst0_freq_bin;
  bd->burst0_freq_bin = ( bd->max_bin + bd->min_bin )/2;
  bd->burst2_in_freq_range = bd->burst1_in_freq_range;
  bd->burst1_in_freq_range = bd->burst0_in_freq_range;
  bd->burst0_in_freq_range = 0;
  if ( (abs(detected_frequency_khz - VBR_FREQ1_KHZ) < VBR_FREQ_RANGE) || 
       (abs(detected_frequency_khz - VBR_FREQ2_KHZ) < VBR_FREQ_RANGE) || 
       (abs(detected_frequency_khz - VBR_FREQ3_KHZ) < VBR_FREQ_RANGE) || 
       (abs(detected_frequency_khz - VBR_FREQ4_KHZ) < VBR_FREQ_RANGE) ){
    bd->burst0_in_freq_range = 1;
  } else {
    bd->burst0_in_freq_range = 0;  
  }

  // 12 comes from 4MHz approximation (4MHz/300Khz);
  bursts_are_atmost_4mhz_away = ( abs(bd->burst0_freq_bin - bd->burst1_freq_bin) < 12 ) && 
                  ( abs(bd->burst0_freq_bin - bd->burst2_freq_bin) < 12 ) ; 
  bursts_are_in_freq_range = bd->burst0_in_freq_range && bd->burst1_in_freq_range && bd->burst2_in_freq_range;
  if ( (bd->current_burst_width > 300) && bursts_are_atmost_4mhz_away && bursts_are_in_freq_range  && burst01_small_gap ) {
    found_vbr = 1;

  }

#endif
  bursts_are_atmost_4mhz_away = 0; 
  bursts_are_in_freq_range = 0; 

  /* mute compiler warning */
  bursts_are_in_freq_range = bursts_are_in_freq_range;
  bursts_are_atmost_4mhz_away = bursts_are_atmost_4mhz_away;
  burst01_gap = burst01_gap;

  //###############################################################################
  //##### DETECTION OF MW, FHSS CORDLESS PHONES AND BLUETOOTH A2DP PROTOCOLS ######
  //###############################################################################
  if ( this_burst_is_narrowband && (bd->current_burst_width > CWA_WIDTH_THRESHOLD) ) {
    found_cwa_long_burst = 1;
  } else if ( this_burst_is_mwo_narrowband &&
          (bd->current_burst_width > MWO_WIDTH_THRESHOLD) &&
          (bd->max_bin > (bd->min_bin+2)) && 
          (bd->current_burst_width < MWO_MAX_LENGTH) ) {

    mwo_frequency_bins = ( bd->max_bin+bd->min_bin )/2;
    mwo_freq_changes01 = SPECTRAL_ABS_DIFF(bd->mwo_long_burst_frequency_bins, mwo_frequency_bins) > 2;
    mwo_freq_changes12 = SPECTRAL_ABS_DIFF(bd->mwo_long_burst_frequency_bins1, bd->mwo_long_burst_frequency_bins) > 2;
    mwo_bursts01_in_range =  
      ( (timestamp-bd->mwo_long_burst_timestamp) > MWO_MINIMUM_BURST_DIFF ) &&
      ( (timestamp-bd->mwo_long_burst_timestamp ) < 4*MWO_MINIMUM_BURST_DIFF ) ;
    mwo_bursts12_in_range =
      ( (bd->mwo_long_burst_timestamp-bd->mwo_long_burst_timestamp1) > MWO_MINIMUM_BURST_DIFF ) &&
      ( (bd->mwo_long_burst_timestamp-bd->mwo_long_burst_timestamp1 ) < 4*MWO_MINIMUM_BURST_DIFF ) ;
    mwo_bursts23_in_range =
      ( (bd->mwo_long_burst_timestamp1-bd->mwo_long_burst_timestamp2) > MWO_MINIMUM_BURST_DIFF ) &&
      ( (bd->mwo_long_burst_timestamp1-bd->mwo_long_burst_timestamp2 ) < 4*MWO_MINIMUM_BURST_DIFF ) ;

    /* printf("values %d %d %d %d\n", timestamp, bd->mwo_long_burst_timestamp, bd->mwo_long_burst_timestamp1, bd->mwo_long_burst_timestamp2);
    printf("found mwo_long %d %d %d %d %d %d\n", mwo_bursts01_in_range, mwo_bursts12_in_range, mwo_bursts23_in_range, !mwo_freq_changes01, !mwo_freq_changes12, mwo_freq_changes12);
    */
    /* mute compiler warning */
    mwo_bursts12_in_range = mwo_bursts12_in_range;
    mwo_bursts23_in_range = mwo_bursts23_in_range;

    if ( mwo_bursts01_in_range && !mwo_freq_changes01 && !mwo_freq_changes12 && !bd->burst0_in_freq_range) {
      found_mwo_long_burst = 1;
    } else {
      found_mwo_long_burst = 0;
    }

    bd->mwo_long_burst_timestamp2 = bd->mwo_long_burst_timestamp1;
    bd->mwo_long_burst_timestamp1 = bd->mwo_long_burst_timestamp;
    bd->mwo_long_burst_timestamp = timestamp;
    bd->mwo_long_burst_frequency_bins1 = bd->mwo_long_burst_frequency_bins;
    bd->mwo_long_burst_frequency_bins = mwo_frequency_bins;
    //printf("sec %d, width %d, rssi %d, diff %d\n", sec, bd->current_burst_width, (bd->current_burst_rssi/bd->previous_nb), bd->current_burst_start_time-bd->burstprev1_start);
  } else if ( (bd->current_burst_width > LBURST_MIN_BURSTWIDTH) && this_burst_is_narrowband ) {


    // this is to prevent the mwo false alarm due to video bridge. but
    // it should help to prevent other possible false alarms as well.
    //bd->mwo_long_burst_timestamp = INT_MIN/2;

    bd->num_identified_bursts = bd->num_identified_bursts + 1;

    //printf("sec %d, width %d, rssi %d, diff %d\n", sec, bd->current_burst_width, bd->current_burst_rssi/bd->previous_nb, bd->current_burst_start_time-bd->burstprev1_start);
    // Burst 3
    bd->burstprev3_frequency = bd->burstprev2_frequency;
    bd->burstprev3_start = bd->burstprev2_start;
    bd->burstprev3_max_min = bd->burstprev2_max_min;
    // Burst 2
    bd->burstprev2_frequency = bd->burstprev1_frequency;
    bd->burstprev2_start = bd->burstprev1_start;
    bd->burstprev2_stop = bd->burstprev1_stop;
    bd->burstprev2_rssi = bd->burstprev1_rssi;
    bd->burstprev2_previous_nb = bd->burstprev1_previous_nb;
    bd->burstprev2_width = bd->burstprev1_width;
    bd->burstprev2_max_min = bd->burstprev1_max_min;
    // Burst 1
    bd->burstprev1_frequency = bd->burst_frequency;
    bd->burstprev1_start = bd->burst_start;
    bd->burstprev1_stop = bd->burst_stop;
    bd->burstprev1_rssi = bd->burst_rssi;
    bd->burstprev1_previous_nb = bd->burst_previous_nb;
    bd->burstprev1_width = bd->burst_width;
    bd->burstprev1_max_min = bd->burst_max_min;
    // Burst 0 - Current Burst
    bd->burst_frequency = (bd->max_bin + bd->min_bin )*(SUBCARRIER_SPACING_NO_DECIMAL);
    bd->burst_frequency = (bd->burst_frequency/20);
    
    bd->burst_start = bd->current_burst_start_time;
    bd->burst_stop = bd->current_burst_stop;
    bd->burst_width = bd->current_burst_width;
    bd->burst_rssi = bd->current_burst_rssi;
    bd->burst_previous_nb = bd->previous_nb;
    bd->burst_max_min = bd->max_bin - bd->min_bin;

    if  (bd->num_identified_bursts > 3) {

        test_freq01_changes = ( SPECTRAL_ABS_DIFF(bd->burst_frequency, bd->burstprev1_frequency) > (int)(TRIPLE_FREQUENCY_HOPPING_MIN_FREQ_CHANGE) );
        test_freq12_changes = ( SPECTRAL_ABS_DIFF(bd->burstprev1_frequency, bd->burstprev2_frequency) > (int)(TRIPLE_FREQUENCY_HOPPING_MIN_FREQ_CHANGE) );
        test_freq02_changes = ( SPECTRAL_ABS_DIFF(bd->burst_frequency, bd->burstprev2_frequency) > (int)(TRIPLE_FREQUENCY_HOPPING_MIN_FREQ_CHANGE) );
        test_freq03_changes = ( SPECTRAL_ABS_DIFF(bd->burst_frequency, bd->burstprev3_frequency) > (int)(TRIPLE_FREQUENCY_HOPPING_MIN_FREQ_CHANGE) );
        test_freq13_changes = ( SPECTRAL_ABS_DIFF(bd->burstprev1_frequency, bd->burstprev3_frequency) > (int)(TRIPLE_FREQUENCY_HOPPING_MIN_FREQ_CHANGE) );

        conseq_burst_diff01 = bd->burst_start - bd->burstprev1_start;
        conseq_burst_diff12 = bd->burstprev1_start - bd->burstprev2_start;
        conseq_burst_diff23 = bd->burstprev2_start - bd->burstprev3_start;
  
        test_large_burstwidth950 = bd->burst_width > 950;
        test_large_burstwidth800 = bd->burst_width > 800;
        test_large_burstwidth600 = bd->burst_width > 600;
        test_large_burstwidth500 = bd->burst_width > 500;
  
        // Cordless phone detection
        test_cph_width = ( (bd->burst_width < CPH_MAX_WIDTH) && (bd->burstprev1_width <CPH_MAX_WIDTH)  );
        test_cph_closewidth_bursts = ( SPECTRAL_ABS_DIFF(bd->burstprev1_width, bd->burst_width) < CPH_BURST_WIDTH_DELTA );
        one_handset = (conseq_burst_diff01 < CPH_ONE_HANDSET_PERIOD_MAX) && (conseq_burst_diff01 > CPH_ONE_HANDSET_PERIOD_MIN);
        two_handset = (conseq_burst_diff01 < CPH_TWO_HANDSET_PERIOD_MAX) && (conseq_burst_diff01 > CPH_TWO_HANDSET_PERIOD_MIN);
        ms10 = (conseq_burst_diff01 < 11000) && (conseq_burst_diff01 > 9000);
        ms20 = (conseq_burst_diff01 < 21000) && (conseq_burst_diff01 > 19000);
        ms30 = (conseq_burst_diff01 < 31000) && (conseq_burst_diff01 > 29000);
        test_cph_time_diff_ok = (one_handset || two_handset || ms10 || ms20 || ms30); 
        test_cph_time_diff_ok = one_handset || two_handset;
  
        // MW2. check to see that bursts are separated enough.
        test_mwo_proper_separation01 = conseq_burst_diff01 > 8000;
        test_mwo_proper_separation12 = conseq_burst_diff12 > 8000;
        test_mwo_proper_separation23 = 1;
  
  
        test_bts_burst_sep = (bd->burst_start - bd->burstprev1_stop) < BTS_MAX_BURST_SEPARATION;

        /* mute compiler warning */
        test_bts_burst_sep = test_bts_burst_sep;
        test_large_burstwidth950 = test_large_burstwidth950;

        // test_bts_smallburstfirst = bd->burst_width > bd->burstprev1_width;
  
        // found_bts = test_freq02_changes && test_large_burstwidth600 && test_bts_burst_sep && test_cph_time_diff_notok;
  
        test_bts_time_diff_ok = (bd->burst_start - bd->burstprev1_start) < BTH_MAX_BURST_DIFF;
        found_bts = test_bts_time_diff_ok && 
                     test_large_burstwidth600 && 
                     !test_freq01_changes && 
                     test_freq02_changes && 
                     test_freq13_changes;
  
        found_cph = test_cph_width  &&  test_cph_time_diff_ok &&
            (!test_freq01_changes) && test_freq03_changes && test_freq12_changes && test_cph_closewidth_bursts;
  
        found_mwo = (!test_freq01_changes) && (!test_freq02_changes) && (!test_freq03_changes) && 
          test_large_burstwidth500 && (bd->max_bin > bd->min_bin) &&
          test_mwo_proper_separation01 && test_mwo_proper_separation12 && test_mwo_proper_separation23  && !bd->burst0_in_freq_range;
  
        found_cwa = (bd->burst_max_min == 0) && (bd->burstprev1_max_min == 0) && 
          (bd->burstprev2_max_min == 0) && (bd->burstprev3_max_min == 0) && 
          (bd->burst_frequency == bd->burstprev1_frequency) && (bd->burst_frequency == bd->burstprev2_frequency) &&
          (bd->burst_frequency == bd->burstprev3_frequency) && 
          (test_large_burstwidth800) && (conseq_burst_diff01 < 10000);
    } 
  }
  
  //###############################################################################
  //#####           DETECTION OF BLUETOOTH HANDSFREE PROTOCOL                ######
  //###############################################################################
  if ( (bd->current_burst_width < SBURST_MAX_BURSTWIDTH ) && (bd->current_burst_width > SBURST_MIN_BURSTWIDTH) && this_burst_is_narrowband ) {
  
    bd->num_identified_short_bursts = bd->num_identified_short_bursts + 1;
  
    //# Burst 3
    bd->sburstprev3_frequency = bd->sburstprev2_frequency;
  
    //# Burst 2
    bd->sburstprev2_frequency = bd->sburstprev1_frequency;
    bd->sburstprev2_start = bd->sburstprev1_start;
    bd->sburstprev2_rssi = bd->sburstprev1_rssi;
    bd->sburstprev2_previous_nb = bd->sburstprev1_previous_nb;
    bd->sburstprev2_width = bd->sburstprev1_width;
    bd->sburstprev2_max_min = bd->sburstprev1_max_min;
    //# Burst 1
    bd->sburstprev1_frequency = bd->sburst_frequency;
    bd->sburstprev1_start = bd->sburst_start;
    bd->sburstprev1_stop = bd->sburst_stop;
    bd->sburstprev1_rssi = bd->sburst_rssi;
    bd->sburstprev1_previous_nb = bd->sburst_previous_nb;
    bd->sburstprev1_width = bd->sburst_width;
    bd->sburstprev1_max_min = bd->sburst_max_min;
    //# Burst 0 - Current Burst
    //bd->sburst_frequency = ( bd->max_bin + bd->min_bin ) * (SUBCARRIER_SPACING/2);
    bd->sburst_frequency = (bd->max_bin + bd->min_bin )*(SUBCARRIER_SPACING_NO_DECIMAL);
    //SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL1,"max_bin=%d min_bin=%d peak_index=%d sburst_freq=%d \n",bd->max_bin, bd->min_bin, bd->peak_index, bd->sburst_frequency); 
    bd->sburst_frequency = (bd->sburst_frequency/20);
    //SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL1,"sburst_freq=%d \n", bd->sburst_frequency); 

    bd->sburst_start = bd->current_burst_start_time;
    bd->sburst_stop = bd->current_burst_stop;
    bd->sburst_width = bd->current_burst_width;
    bd->sburst_rssi = bd->current_burst_rssi;
    bd->sburst_previous_nb = bd->previous_nb;
    bd->sburst_max_min = bd->max_bin - bd->min_bin;
    //SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL1,"sburst_max_min=%d \n", bd->sburst_max_min); 
      
    if  (bd->num_identified_short_bursts > 2) {
   //printf("%s %d\n", __func__, __LINE__);
      //SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL1,"sburst_freq=%d sburstprev1_freq=%d sburstprev2_freq=%d\n", 
                    //bd->sburst_frequency, bd->sburstprev1_frequency, bd->sburstprev2_frequency );
      test_freq01_changes = ( SPECTRAL_ABS_DIFF(bd->sburst_frequency, bd->sburstprev1_frequency) > (int)(DOUBLE_FREQUENCY_HOPPING_MIN_FREQ_CHANGE) );
      test_freq02_changes = ( SPECTRAL_ABS_DIFF(bd->sburst_frequency, bd->sburstprev2_frequency) > (int)(DOUBLE_FREQUENCY_HOPPING_MIN_FREQ_CHANGE) );
      test_freq12_changes = ( SPECTRAL_ABS_DIFF(bd->sburstprev1_frequency, bd->sburstprev2_frequency) > (int)(DOUBLE_FREQUENCY_HOPPING_MIN_FREQ_CHANGE) );
      //# test_freq23_changes = ( abs(bd->sburstprev2_frequency - bd->sburstprev3_frequency) > 2*FREQUENCY_HOPPING_MIN_FREQ_CHANGE );
  
      //# conseq_burst_diff0 = bd->sburst_start - bd->sburstprev1_start;
      //# test_bth_time_diff_ok = conseq_burst_diff0 < BTH_MAX_BURST_DIFF;
      test_bth_time_diff_ok = (bd->sburst_start - bd->sburstprev1_start) < BTH_MAX_BURST_DIFF;
  
      //# make sure the burst are separated enough - ||             ||            ||
      conseq_burst_diff1 = bd->sburstprev1_start - bd->sburstprev2_start;
      test_bth_diff12_large = conseq_burst_diff1 > BTH_MIN_PREVBURST_DIFF;
  
      found_bth = test_bth_time_diff_ok && test_bth_diff12_large && !test_freq01_changes && test_freq02_changes && test_freq12_changes;
      //SPECTRAL_DPRINTK(sc, ATH_DEBUG_SPECTRAL1, "found_bth=%d test_bth_time_diff_ok=%d test_bth_diff12_large=%d test_freq01_changes=%d test_freq02_changes=%d test_freq12_changes=%d\n", found_bth, test_bth_time_diff_ok, test_bth_diff12_large, test_freq01_changes, test_freq02_changes, test_freq12_changes);
      /*
      if (bd->lwr_band_data == 0)
        //printf("BT HF: %d - %d %d %d %d (%d) %d %d\n", bd->lwr_band_data, test_bth_time_diff_ok, test_bth_diff12_large, (!test_freq01_changes), test_freq02_changes, found_bth, bd->sburst_start-bd->sburstprev1_stop, BTH_MAX_BURST_DIFF);
      */
  
    } 
  } //# END of DETECTION 
  
  
  //# Record RSSI and time-stamps of detected signals.
  if (found_vbr) {  
    for (i = 1; i < VBR_MIN_NUMBER_OF_FRAMES; ++i){
      bd->vbr_timestamps[i-1] = bd->vbr_timestamps[i];
      bd->vbr_rssi[i-1] = bd->vbr_rssi[i];
    }
    bd->vbr_timestamps[VBR_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_stop;
    bd->vbr_rssi[VBR_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_rssi/bd->previous_nb;
  }
  if (found_bth) {  
    for (i = 1; i < BTH_MIN_NUMBER_OF_FRAMES; ++i){
      bd->bth_timestamps[i-1] = bd->bth_timestamps[i];
      bd->bth_rssi[i-1] = bd->bth_rssi[i];
    }
    bd->bth_timestamps[BTH_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_stop;
    bd->bth_rssi[BTH_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_rssi/bd->previous_nb;
  }
  if (found_bts) {  
    for (i = 1; i < BTS_MIN_NUMBER_OF_FRAMES; ++i){
      bd->bts_timestamps[i-1] = bd->bts_timestamps[i];
      bd->bts_rssi[i-1] = bd->bts_rssi[i];
    }
    bd->bts_timestamps[BTS_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_stop;
    bd->bts_rssi[BTS_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_rssi/bd->previous_nb;
  }
  if (found_cph) {  
    for (i = 1; i < CPH_MIN_NUMBER_OF_FRAMES; ++i){
      bd->cph_timestamps[i-1] = bd->cph_timestamps[i];
      bd->cph_rssi[i-1] = bd->cph_rssi[i];
    }
    bd->cph_timestamps[CPH_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_stop;
    bd->cph_rssi[CPH_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_rssi/bd->previous_nb;
  }
  if (found_mwo) {  
    for (i = 1; i < MWO_MIN_NUMBER_OF_FRAMES; ++i){
      bd->mwo_timestamps[i-1] = bd->mwo_timestamps[i];
      bd->mwo_rssi[i-1] = bd->mwo_rssi[i];
    }
    bd->mwo_timestamps[MWO_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_stop;
    bd->mwo_rssi[MWO_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_rssi;
  }
  if (found_cwa) {  
    for (i = 1; i < CWA_MIN_NUMBER_OF_FRAMES; ++i){
      bd->cwa_timestamps[i-1] = bd->cwa_timestamps[i];
      bd->cwa_rssi[i-1] = bd->cwa_rssi[i];
    }
    bd->cwa_timestamps[CWA_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_stop;
    bd->cwa_rssi[CWA_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_rssi/bd->previous_nb;
  }
  
  /*if (2 == 0){
    //# //printf("Frist report %d\n", first_report);
    first_hour = hour;
    first_min = min;
    first_sec = sec;
  }*/
  
  //passed_time_in_seconds = (hour-first_hour)*3600 + (min-first_min)*60 + (sec-first_sec);
  
  //# No empty lines should happen in the file.
  //# passed_time_in_seconds = (min-first_min)*60 + (sec-first_sec);
  //# if (passed_time_in_seconds < 0) {
  //#   //printf("min %d, minprev %d\n", min, first_min);
  //# }
  
  // Video Bridge
  if ( ( (bd->current_burst_stop - bd->vbr_timestamps[0] ) < OBSERVATION_WINDOW ) && found_vbr) {
    bd->count_vbr += 1;
    something_detected = 1;
    //mean_rssi = &mean_value(@{bd->vbr_rssi});

    bd->vbr_mean_rssi = mean_value(bd->vbr_rssi, VBR_MIN_NUMBER_OF_FRAMES);
    //if (PRINT_DETECTION_TO_SCREEN) {
    //  //printf("%s: Video Bridge       : Time %d:%d:%d, RSSI %d! [%d][%ds]\n", 
    //    LOU, hour, min, sec, mean_rssi, bd->count_vbr}, passed_time_in_seconds);
    ////printf("%s: Video Bridge       : [%d]\n", lower_or_upper, bd->count_vbr);
    //if (bd->print_to_screen)
      printf(" Video Bridge       : [%d] RSSI %d\n", bd->count_vbr, bd->vbr_mean_rssi);

    reset_cph(bd);
    reset_bth(bd);
    reset_bts(bd);
    reset_cwa(bd);
    reset_mwo(bd);
    reset_mwo_long(bd);
  }


  //## Cordless Phone
  if ( ( (bd->current_burst_stop - bd->cph_timestamps[0] ) < OBSERVATION_WINDOW ) && found_cph ) {
    bd->count_cph += 1;
    something_detected = 1;
  
  //#  last_report_time = bd->current_burst_stop;
  
  
    bd->cph_mean_rssi = mean_value(bd->cph_rssi, CPH_MIN_NUMBER_OF_FRAMES);
    //mean_rssi2 = &mean_value(@{bd->cph_rssi2;
    //if (PRINT_DETECTION_TO_SCREEN) {
    //printf("Cordless phone     : RSSI %d [%d]\n",  bd->cph_rssi, bd->count_cph); 
    printf("Cordless phone [%d]\n", bd->count_cph); 
    if (bd->dynamic_ht2040_en){
        if (bd->lwr_band_data) {
            bd->detects_extension_channel ++;
        } else {
            bd->detects_control_channel ++;
        }
    }
    if(detected_frequency_khz <= (bd->dc_in_mhz * 1000)) 
        bd->detects_below_dc++;
    else
        bd->detects_above_dc++;

    if(detected_frequency_khz < (bd->cph_min_freq)) 
        bd->cph_min_freq = (detected_frequency_khz);
    if(detected_frequency_khz > (bd->cph_max_freq))
        bd->cph_max_freq = (detected_frequency_khz);

    //printf("Cordless phone     : [%d] freq %d chan %d dc at %d\n",  bd->count_cph, detected_frequency_khz, bd->b_chan_in_mhz, bd->dc_in_mhz); 
    //  is_dect_present = 1;
    //}
    //if (bd->print_to_screen)
      //printf(" CPH       : [%d], rssi %d\n", bd->count_cph, bd->cph_mean_rssi);
    //# //printf("Freq %d, CW freq %d\n", bd->min_bin, cwa_bin);
  
  
    reset_bts(bd);
    reset_bth(bd);
    reset_vbr(bd);
    reset_mwo(bd);
    reset_mwo_long(bd);
    reset_cwa(bd);
  } 
  
  //# Bluetooth Headset
  if ( ( (bd->current_burst_stop - bd->bth_timestamps[0] ) < OBSERVATION_WINDOW ) && found_bth ) {
    bd->count_bth += 1;
    something_detected = 1;
    //  last_report_time = bd->current_burst_stop;
  
    //mean_rssi1 = &mean_value(@{bd->bth_rssi1##);
    //mean_rssi2 = &mean_value(@{bd->bth_rssi2##);
    bd->bth_mean_rssi = mean_value(bd->bth_rssi, BTH_MIN_NUMBER_OF_FRAMES);
    /*if (PRINT_DETECTION_TO_SCREEN) {
      //printf("%s: Bluetooth Handsfree: Time %d:%d:%d, RSSI %d-%d! [%d][%ds]\n", 
        LOU, hour, min, sec, mean_rssi1, mean_rssi2, bd->count_bth}, passed_time_in_seconds);
    }*/
    if(detected_frequency_khz < (bd->bth_min_freq)) 
        bd->bth_min_freq = (detected_frequency_khz);
    if(detected_frequency_khz > (bd->bth_max_freq))
        bd->bth_max_freq = (detected_frequency_khz);

    if (bd->print_to_screen)
      printf(" BTH       : [%d], rssi %d\n", bd->count_bth, bd->bth_mean_rssi);
    //printf("Burst Start %d\n", bd->sburst_start);
  
    reset_bts(bd);
    reset_vbr(bd);
    reset_cwa(bd);
    reset_mwo(bd);
    reset_mwo_long(bd);
    reset_cph(bd);
  }
  
  // Bluetooth Stereo
  if ( ( (bd->current_burst_stop - bd->bts_timestamps[0] ) < OBSERVATION_WINDOW ) && found_bts) {
    bd->count_bts += 1;
    something_detected = 1;
  
    //mean_rssi = &mean_value(@{bd->bts_rssi##);
    bd->bts_mean_rssi = mean_value(bd->bts_rssi, BTS_MIN_NUMBER_OF_FRAMES);
  
    /*if (PRINT_DETECTION_TO_SCREEN) {
      //printf("%s: Bluetooth          : Time %d:%d:%d, RSSI %d! [%d][%ds]\n", 
        LOU, hour, min, sec, mean_rssi, bd->count_bts}, passed_time_in_seconds);
    }*/
    if (bd->print_to_screen)
      printf(" BTS       : [%d]\n", bd->count_bts);

#if 0
    if(detected_frequency_khz < (bd->bts_min_freq * 1000))
        bd->bts_min_freq = (detected_frequency_khz/1000);
    if(detected_frequency_khz > (bd->bts_max_freq * 1000))
        bd->bts_max_freq = (detected_frequency_khz/1000);
#else
    if(detected_frequency_khz < (bd->bts_min_freq ))
        bd->bts_min_freq = (detected_frequency_khz);
    if(detected_frequency_khz > (bd->bts_max_freq))
        bd->bts_max_freq = (detected_frequency_khz);

#endif 
  
    reset_vbr(bd);
    reset_bth(bd);
    reset_mwo(bd);
    reset_cph(bd);
    reset_mwo_long(bd);
    reset_cwa(bd);
  }
  
  if ( ( ( (bd->current_burst_stop - bd->mwo_timestamps[0] ) < OBSERVATION_WINDOW ) && found_mwo) || found_mwo_long_burst ) {
    bd->count_mwo += 1;
    something_detected = 1;

    /*
    if (PRINT_DETECTION_TO_SCREEN) {
      if (found_mwo_long_burst) {
        //mean_rssi = bd->current_burst_rssi}/bd->previous_nb;
        //printf("%s: Microwave 1      : Time %d:%d:%d, RSSI %d, Frequency %d MHz! [%d][%ds]\n", 
          LOU, hour, min, sec, mean_rssi, detected_frequency_khz, bd->count_mwo}, passed_time_in_seconds);
      } else {
        //mean_rssi = &mean_value(@{bd->mwo_rssi##);
    bd->bts_mean_rssi = mean_value(bd->bts_rssi, BTS_MIN_NUMBER_OF_FRAMES);
        //printf("%s: Microwave 2      : Time %d:%d:%d, RSSI %d, Frequency %d MHz! [%d][%ds]\n", 
          LOU, hour, min, sec, mean_rssi, detected_frequency_khz, bd->count_mwo}, passed_time_in_seconds);
      }
    }
    */

    if (found_mwo_long_burst) {
      bd->mwo_mean_rssi = bd->current_burst_rssi/bd->previous_nb;
    } else {
      bd->mwo_mean_rssi = mean_value(bd->mwo_rssi, MWO_MIN_NUMBER_OF_FRAMES);
    }

    if(detected_frequency_khz < (bd->mwo_min_freq)) 
        bd->mwo_min_freq = (detected_frequency_khz);
    if(detected_frequency_khz > (bd->mwo_max_freq))
        bd->mwo_max_freq = (detected_frequency_khz);

    if (bd->print_to_screen)
      printf(" MWO       : [%d] %d, rssi %d\n", 
        bd->count_mwo, found_mwo_long_burst, bd->mwo_mean_rssi);
  
    reset_vbr(bd);
    reset_bts(bd);
    reset_bth(bd);
    reset_cph(bd);
    reset_cwa(bd);
  }
  if ( ( ( (bd->current_burst_stop - bd->cwa_timestamps[0] ) < OBSERVATION_WINDOW ) && found_cwa) 
          || found_cwa_long_burst ) {
    bd->count_cwa += 1;
    something_detected = 1;

    if (found_cwa_long_burst) {
      bd->cwa_mean_rssi = bd->current_burst_rssi/bd->previous_nb;
    } else {
      bd->cwa_mean_rssi = mean_value(bd->cwa_rssi, CWA_MIN_NUMBER_OF_FRAMES);
    }
#if 0
    if(detected_frequency_khz < (bd->cwa_min_freq * 1000)) 
        bd->cwa_min_freq = (detected_frequency_khz/1000);
    if(detected_frequency_khz > (bd->cwa_max_freq * 1000))
        bd->cwa_max_freq = (detected_frequency_khz/1000);
#else
    if(detected_frequency_khz < (bd->cwa_min_freq)) 
        bd->cwa_min_freq = (detected_frequency_khz);
    if(detected_frequency_khz > (bd->cwa_max_freq))
        bd->cwa_max_freq = (detected_frequency_khz);
#endif  
    /*cwa_timestamp = last_capture_time;
    cwa_present = 1;
    cwa_bin = bd->min_bin;
    */
    //if (bd->print_to_screen)
      printf(" CWA       : [%d] freq %d\n", bd->count_cwa, detected_frequency_khz);
  
    reset_mwo(bd);
    reset_mwo_long(bd);
    reset_bts(bd);
    reset_bth(bd);
    reset_mwo(bd);
    reset_vbr(bd);
  }
  

  
  if (something_detected == 1) {
 
    bd->average_rssi_numbr += bd->previous_nb;
    bd->average_rssi_total += bd->current_burst_rssi;
  
    /*if ((bd->current_burst_stop - bd->last_report_time) > max_gap_between_detects) { 
      max_gap_between_detects = bd->current_burst_stop - bd->last_report_time ;
    }*/
    bd->last_report_time = bd->current_burst_stop;
  
    something_detected = 0;
  }
  
}


//########################################################
//########           WIDEBAND Classifier           #######
//########################################################
void wb_classifier(int timestamp, struct ss *bd){

  //This script classifies the burst detected by spectral scan into different device types.
  int found_wln, something_detected;
  int i;
  
  found_wln = 0;
  something_detected = 0;

  /*if ($lwr_or_upr == 1) {
    $LOU = "L"; # lower or upper
  } else {
    $LOU = "U";
  }*/


  /*# Average powers per burst.
  $avrg1 = bd->current_burst_wb_sum1}/bd->previous_wb};
  $avrg2 = bd->current_burst_wb_sum2}/bd->previous_wb};
  $avrg3 = bd->current_burst_wb_sum3}/bd->previous_wb};
  $avrg4 = bd->current_burst_wb_sum4}/bd->previous_wb};
  */
  
  found_wln = (bd->current_burst_width_wb > WLN_MIN_WIDTH) && (bd->current_burst_width_wb < WLN_MAX_WIDTH);

  if (found_wln) {  
    for (i = 1; i < WLN_MIN_NUMBER_OF_FRAMES; ++i){
      bd->wln_timestamps[i-1] = bd->wln_timestamps[i];
      bd->wln_rssi[i-1] = bd->wln_rssi[i];
    }
    bd->wln_timestamps[WLN_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_stop_wb;
    bd->wln_rssi[WLN_MIN_NUMBER_OF_FRAMES-1] = bd->current_burst_rssi_wb/bd->previous_wb;
  }

  /*if ($found_wln) {
    # //printf("b");
    push(@{bd->wln_timestamps##, bd->current_burst_stop_wb});
    $dummy = shift(@{bd->wln_timestamps##);
    push(@{bd->wln_rssi##, (bd->current_burst_rssi_wb}/bd->previous_wb}) );
    $dummy = shift(@{bd->wln_rssi##);
    push(@{bd->wln_pwr1##, $avrg1 ); $dummy = shift(@{bd->wln_pwr1##);
    push(@{bd->wln_pwr2##, $avrg2 ); $dummy = shift(@{bd->wln_pwr2##);
    push(@{bd->wln_pwr3##, $avrg3 ); $dummy = shift(@{bd->wln_pwr3##);
    push(@{bd->wln_pwr4##, $avrg4 ); $dummy = shift(@{bd->wln_pwr4##);
  }*/

  if ( ( (bd->current_burst_stop_wb - bd->wln_timestamps[0] ) < OBSERVATION_WINDOW ) && found_wln ) {
    bd->count_wln += 1;
    something_detected = 1;


    bd->wln_mean_rssi = mean_value(bd->wln_rssi, WLN_MIN_NUMBER_OF_FRAMES);

    /*
    $avrg1= &mean_value(@{bd->wln_pwr1##);
    $avrg2= &mean_value(@{bd->wln_pwr2##);
    $avrg3= &mean_value(@{bd->wln_pwr3##);
    $avrg4= &mean_value(@{bd->wln_pwr4##);


    # Find the group with maximum power
    $max_avrg = $avrg1;
    $max_avrg = ($avrg2 > $max_avrg) ? $avrg2 : $max_avrg;
    $max_avrg = ($avrg3 > $max_avrg) ? $avrg3 : $max_avrg;
    $max_avrg = ($avrg4 > $max_avrg) ? $avrg4 : $max_avrg;

    $pass_ratio_test1 = ($avrg1 > ($max_avrg/$wln_power_ratio));
    $pass_ratio_test2 = ($avrg2 > ($max_avrg/$wln_power_ratio));
    $pass_ratio_test3 = ($avrg3 > ($max_avrg/$wln_power_ratio));
    $pass_ratio_test4 = ($avrg4 > ($max_avrg/$wln_power_ratio));

    # Find the center frequency
    if ($pass_ratio_test1) {
      if ($pass_ratio_test2) {
        if ($pass_ratio_test3) {
          if ($pass_ratio_test4) {
            $wln_frequency = $CHAN_IN_MHZ;
          } else {
            $wln_frequency = $CHAN_IN_MHZ - 5;
          }
        } else {
          $wln_frequency = $CHAN_IN_MHZ - 10;
        }
      } else {
        $wln_frequency = $CHAN_IN_MHZ - 15;
      }
    } else {
      if ($pass_ratio_test2) {
        $wln_frequency = $CHAN_IN_MHZ + 5;
      } else {
        if ($pass_ratio_test3) {
          $wln_frequency = $CHAN_IN_MHZ + 10;
        } else {
          $wln_frequency = $CHAN_IN_MHZ + 15;
        }
      }

    }
    */

    // printf("Sums  : %d %d %d %d (max = %d), width %d\n", $avrg1, $avrg2, $avrg3, $avrg4, $max_avrg, bd->current_burst_width_wb} );
    // printf("Ratios: %d %d %d %d\n", $pass_ratio_test1, $pass_ratio_test2, $pass_ratio_test3, $pass_ratio_test4 );
    //if (bd->print_to_screen)
      printf(" WLAN Signal       : [%d] RSSI %d\n", bd->count_wln, bd->wln_mean_rssi);

    //printf("%s: WLAN Signal        : Time %d:%d:%d, RSSI %d!, Center %d [%d][%ds]\n", 
    //  $LOU, $hour, $min, $sec, $mean_rssi, $wln_frequency, bd->count_wln}, $passed_time_in_seconds);
    
  }


  if (something_detected == 1) {
  
    // No need to add wb power right now. 
    //bd->average_rssi_numbr += bd->previous_wb;
    //bd->average_rssi_total += bd->current_burst_rssi_wb;
  
    /*if ((bd->current_burst_stop - bd->last_report_time) > max_gap_between_detects) { 
      max_gap_between_detects = bd->current_burst_stop - bd->last_report_time ;
    }*/
    bd->last_report_time = bd->current_burst_stop_wb;
  
    something_detected = 0;
  }

}

  
  
  
/* RESET TIME-COUNTERS */
void reset_vbr(struct ss *bd) {
  int i;
  for (i =0; i < VBR_MIN_NUMBER_OF_FRAMES; ++i) {
    bd->vbr_timestamps[i] = INT_MIN/2;
  }
}
void reset_bth(struct ss *bd) {
  int i;
  for (i =0; i < BTH_MIN_NUMBER_OF_FRAMES; ++i) {
    bd->bth_timestamps[i] = INT_MIN/2;
  }
  bd->bth_max_freq = INT_MIN;
  bd->bth_min_freq = INT_MAX;
}
void reset_bts(struct ss *bd) {
  int i;
  for (i =0; i < BTS_MIN_NUMBER_OF_FRAMES; ++i) {
    bd->bts_timestamps[i] = INT_MIN/2;
  }
  bd->bts_max_freq = INT_MIN;
  bd->bts_min_freq = INT_MAX;
}
void reset_cph(struct ss *bd) {
  int i;
  for (i =0; i < CPH_MIN_NUMBER_OF_FRAMES; ++i) {
    bd->cph_timestamps[i] = INT_MIN/2;
  }
  bd->cph_max_freq = INT_MIN;
  bd->cph_min_freq = INT_MAX;
}
void reset_cwa(struct ss *bd) {
  int i;
  for (i =0; i < CWA_MIN_NUMBER_OF_FRAMES; ++i) {
    bd->cwa_timestamps[i] = INT_MIN/2;
  }
  bd->cwa_max_freq = INT_MIN;
  bd->cwa_min_freq = INT_MAX;
}
void reset_mwo(struct ss *bd) {
  int i;
  for (i =0; i < MWO_MIN_NUMBER_OF_FRAMES; ++i) {
    bd->mwo_timestamps[i] = INT_MIN/2;
  }
  bd->mwo_max_freq = INT_MIN;
  bd->mwo_min_freq = INT_MAX;
}
void reset_mwo_long(struct ss *bd) {
  bd->mwo_long_burst_timestamp = INT_MIN/2;
  bd->mwo_long_burst_timestamp1 = INT_MIN/2;
}
void reset_wln(struct ss *bd) {
  int i;
  for (i =0; i < WLN_MIN_NUMBER_OF_FRAMES; ++i) {
    bd->wln_timestamps[i] = INT_MIN/2;
  }
}

/* RESET NB counters */
void reset_nb_counters(struct ss *bd) {
  bd->previous_nb = 0;
  bd->max_bin = INT_MIN;
  bd->min_bin = INT_MAX;
  bd->current_burst_rssi = 0;
  bd->current_burst_width = 0;
}

int mean_value(int* input_array, int array_length){
  int i, sum, mean;
  sum = 0;

  for (i = 0; i < array_length; ++i) {
    sum += input_array[i];
  }
  mean = sum / array_length;

  return mean;
}
