#ifndef WIN32
#include <string.h>
#include <linux/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#endif

#include "classifier.h"
#include "spectral_data.h"
#include "spec_msg_proto.h"
#include "classifier.h" 
#include "spectral_types.h" 

#define MAX_NUM_BINS 128

void ms_classifier_initialize(struct ss *spectral_lower, struct ss *spectral_upper);

void ms_init_classifier(struct ss *lwrband, struct ss *uprband, SPECTRAL_CLASSIFIER_PARAMS *cp)
{
  memset(lwrband, 0, sizeof(struct ss));
  memset(uprband, 0, sizeof(struct ss));

 if (cp->spectral_20_40_mode) {

    /* AP is in 20-40 mode so we will need to process both extension and primary channel spectral data */
        lwrband->b_chan_in_mhz = cp->lower_chan_in_mhz;
        uprband->b_chan_in_mhz = cp->upper_chan_in_mhz;

    uprband->dynamic_ht2040_en = cp->spectral_20_40_mode;
    uprband->dc_index = cp->spectral_dc_index;
    uprband->dc_in_mhz = (uprband->b_chan_in_mhz - 10);
    uprband->lwr_band_data = 0;

 } else {
        /* AP is in 20MHz mode so only primary channel spectral data counts*/
        lwrband->b_chan_in_mhz = cp->lower_chan_in_mhz;
    }

  lwrband->dynamic_ht2040_en = cp->spectral_20_40_mode;
  lwrband->dc_index = cp->spectral_dc_index;
  lwrband->dc_in_mhz = (lwrband->b_chan_in_mhz + 10);
  lwrband->lwr_band_data = 1;
  ms_classifier_initialize(lwrband, uprband);
}


void ms_classifier_initialize(struct ss *spectral_lower, struct ss *spectral_upper)
{
  struct ss *lwrband, *uprband;
  lwrband = spectral_lower;
  uprband = spectral_upper;

 lwrband->print_to_screen=1;
 uprband->print_to_screen=1;

  lwrband->max_bin = INT_MIN;
  lwrband->min_bin = INT_MAX;
  uprband->max_bin = INT_MIN;
  uprband->min_bin = INT_MAX;

  lwrband->count_vbr = 0;
  uprband->count_vbr = 0;
  lwrband->count_bth = 0;
  uprband->count_bth = 0;
  lwrband->count_bts = 0;
  uprband->count_bts = 0;
  lwrband->count_cwa = 0;
  uprband->count_cwa = 0;
  lwrband->count_wln = 0;
  uprband->count_wln = 0;
  lwrband->count_mwo = 0;
  uprband->count_mwo = 0;
  lwrband->count_cph = 0;
  uprband->count_cph = 0; 
  lwrband->num_identified_short_bursts = 0;
  uprband->num_identified_short_bursts = 0;
  lwrband->num_identified_bursts = 0;
  uprband->num_identified_bursts = 0;
  lwrband->previous_nb = 0;
  uprband->previous_nb = 0;
  lwrband->previous_wb = 0;
  uprband->previous_wb = 0;
  lwrband->prev_burst_stop = 0;
  uprband->prev_burst_stop = 0;
  lwrband->average_rssi_total = 0;
  uprband->average_rssi_total = 0;
  lwrband->average_rssi_numbr = 1;
  uprband->average_rssi_numbr = 1;

  reset_vbr(lwrband);
  reset_vbr(uprband);
  reset_bth(lwrband);
  reset_bth(uprband);
  reset_bts(lwrband);
  reset_bts(uprband);
  reset_cph(lwrband);
  reset_cph(uprband);
  reset_cwa(lwrband);
  reset_cwa(uprband);
  reset_mwo(lwrband);
  reset_mwo(uprband);
  reset_wln(lwrband);
  reset_wln(uprband);

  return;
}

