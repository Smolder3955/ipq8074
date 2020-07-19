/*
 * Copyright (c) 2011,2016-2018 Qualcomm Innovation Center, Inc.
 * All Rights Reserved
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011, 2016 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 * Copyright (c) 2008 Atheros Communications Inc.
 * All Rights Reserved.
 */

#include <osdep.h>

#include <ieee80211_var.h>
#include <ieee80211_channel.h>
#include <ieee80211_acs.h>
#include <ieee80211_acs_internal.h>
#include <ol_if_athvar.h>
#include <ieee80211.h>
#include <wlan_son_pub.h>
#include <wlan_lmac_if_api.h>
#include "wlan_utility.h"
#include <ieee80211_cbs.h>

#if ATH_ACS_DEBUG_SUPPORT
#include "acs_debug.h"
#endif

#if WIFI_MEM_MANAGER_SUPPORT
#include "mem_manager.h"
#endif

unsigned int  eacs_dbg_mask = 0;



static void ieee80211_free_ht40intol_scan_resource(ieee80211_acs_t acs);

static void ieee80211_acs_free_scan_resource(ieee80211_acs_t acs);

static void ieee80211_acs_scan_report_internal(struct ieee80211com *ic);
static QDF_STATUS ieee80211_get_chan_neighbor_list(void *arg, wlan_scan_entry_t se);

static int ieee80211_acs_derive_sec_chans_with_mode(u_int8_t phymode,
            u_int8_t vht_center_freq, u_int8_t channel_ieee_se,
            u_int8_t *sec_chan_20, u_int8_t *sec_chan_40_1, u_int8_t *sec_chan_40_2);

static OS_TIMER_FUNC(ieee80211_ch_long_timer);
static OS_TIMER_FUNC(ieee80211_ch_nohop_timer);
static OS_TIMER_FUNC(ieee80211_ch_cntwin_timer);

int ieee80211_check_and_execute_pending_acsreport(wlan_if_t vap);
#if ATH_ACS_SUPPORT_SPECTRAL && WLAN_SPECTRAL_ENABLE
int get_eacs_control_duty_cycle(ieee80211_acs_t acs);
int get_eacs_extension_duty_cycle(ieee80211_acs_t acs);
#endif
static INLINE u_int8_t ieee80211_acs_in_progress(ieee80211_acs_t acs)
{
    return atomic_read(&(acs->acs_in_progress));
}

/*
 * Check for channel interference.
 */
static int
ieee80211_acs_channel_is_set(struct ieee80211vap *vap)
{
    struct ieee80211_ath_channel    *chan = NULL;

    chan =  vap->iv_des_chan[vap->iv_des_mode];

    if ((chan == NULL) || (chan == IEEE80211_CHAN_ANYC)) {
        return (0);
    } else {
        return (1);
    }
}

/*
 * Check for channel interference.
 */
static int
ieee80211_acs_check_interference(struct ieee80211_ath_channel *chan, struct ieee80211vap *vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    uint32_t dfs_reg = 0;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;

    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        return -1;
    }

    psoc = wlan_pdev_get_psoc(pdev);
    if (psoc == NULL) {
        return -1;
    }

    reg_rx_ops = wlan_lmac_if_get_reg_rx_ops(psoc);
    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_REGULATORY_SB_ID) !=
            QDF_STATUS_SUCCESS) {
        return -1;
    }
    reg_rx_ops->get_dfs_region(pdev, &dfs_reg);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_SB_ID);

    /*
     * (1) skip static turbo channel as it will require STA to be in
     * static turbo to work.
     * (2) skip channel which's marked with radar detection
     * (3) WAR: we allow user to config not to use any DFS channel
     * (4) skip excluded 11D channels. See bug 31246
     */
    if ( IEEE80211_IS_CHAN_STURBO(chan) ||
            IEEE80211_IS_CHAN_RADAR(chan) ||
            (IEEE80211_IS_CHAN_DFSFLAG(chan) && ieee80211_ic_block_dfschan_is_set(ic)) ||
            IEEE80211_IS_CHAN_11D_EXCLUDED(chan) ||
            ( ic->ic_no_weather_radar_chan &&
                (ieee80211_check_weather_radar_channel(chan)) &&
                (DFS_ETSI_DOMAIN == dfs_reg)
            )) {
        return (1);
    } else {
        return (0);
    }
}

static void ieee80211_acs_get_adj_ch_stats(ieee80211_acs_t acs, struct ieee80211_ath_channel *channel,
        struct ieee80211_acs_adj_chan_stats *adj_chan_stats )
{
#define ADJ_CHANS 8
    u_int8_t ieee_chan = ieee80211_chan2ieee(acs->acs_ic, channel);
    int k;
    u_int8_t sec_chan, first_adj_chan, last_adj_chan;
    u_int8_t sec_chan_40_1 = 0;
    u_int8_t sec_chan_40_2 = 0;
    u_int8_t sec_chan_80_1 = 0;
    u_int8_t sec_chan_80_2 = 0;
    u_int8_t sec_chan_80_3 = 0;
    u_int8_t sec_chan_80_4 = 0;
    int8_t pri_center_ch_diff, pri_center_ch;
    int8_t sec_level;

    int64_t mode_mask = (IEEE80211_CHAN_11NA_HT20 |
            IEEE80211_CHAN_11NA_HT40PLUS |
            IEEE80211_CHAN_11NA_HT40MINUS |
            IEEE80211_CHAN_11AC_VHT20 |
            IEEE80211_CHAN_11AC_VHT40PLUS |
            IEEE80211_CHAN_11AC_VHT40MINUS |
            IEEE80211_CHAN_11AC_VHT80 |
            IEEE80211_CHAN_11AC_VHT160 |
            IEEE80211_CHAN_11AC_VHT80_80 |
            IEEE80211_CHAN_11AXA_HE20 |
            IEEE80211_CHAN_11AXA_HE40PLUS |
            IEEE80211_CHAN_11AXA_HE40MINUS |
            IEEE80211_CHAN_11AXA_HE80 |
            IEEE80211_CHAN_11AXA_HE160 |
            IEEE80211_CHAN_11AXA_HE80_80);


    adj_chan_stats->if_valid_stats = 1;
    adj_chan_stats->adj_chan_load = 0;
    adj_chan_stats->adj_chan_rssi = 0;
    adj_chan_stats->adj_chan_idx = 0;

    switch (channel->ic_flags & mode_mask)
    {
        case IEEE80211_CHAN_11NA_HT40PLUS:
        case IEEE80211_CHAN_11AC_VHT40PLUS:
        case IEEE80211_CHAN_11AXA_HE40PLUS:
            sec_chan = ieee_chan+4;
            first_adj_chan = ieee_chan - ADJ_CHANS;
            last_adj_chan = sec_chan + ADJ_CHANS;
            break;
        case IEEE80211_CHAN_11NA_HT40MINUS:
        case IEEE80211_CHAN_11AC_VHT40MINUS:
        case IEEE80211_CHAN_11AXA_HE40MINUS:
            sec_chan = ieee_chan-4;
            first_adj_chan = sec_chan - ADJ_CHANS;
            last_adj_chan = ieee_chan + ADJ_CHANS;
            break;
        case IEEE80211_CHAN_11AC_VHT80:
        case IEEE80211_CHAN_11AXA_HE80:
        case IEEE80211_CHAN_11AC_VHT80_80:
        case IEEE80211_CHAN_11AXA_HE80_80:

            ieee80211_acs_derive_sec_chans_with_mode(ieee80211_chan2mode(channel),
                    channel->ic_vhtop_ch_freq_seg1, ieee_chan, &sec_chan,
                                               &sec_chan_40_1, &sec_chan_40_2);

           /* Adjacent channels are 4 channels before the band and 4 channels are
               after the band */
            first_adj_chan = (channel->ic_vhtop_ch_freq_seg1 - 6) - 2*ADJ_CHANS;
            last_adj_chan =  (channel->ic_vhtop_ch_freq_seg1 + 6) + 2*ADJ_CHANS;
            break;
        case IEEE80211_CHAN_11AC_VHT160:
        case IEEE80211_MODE_11AXA_HE160:
            pri_center_ch_diff = ieee_chan - channel->ic_vhtop_ch_freq_seg2;

            /* Secondary 20 channel would be less(2 or 6) or more (2 or 6)
             * than center frequency based on primary channel
             */
            if(pri_center_ch_diff > 0) {
                sec_level = LOWER_FREQ_SLOT;
                sec_chan_80_1 = channel->ic_vhtop_ch_freq_seg2 - SEC_80_4;
                sec_chan_80_2 = channel->ic_vhtop_ch_freq_seg2 - SEC_80_3;
                sec_chan_80_3 = channel->ic_vhtop_ch_freq_seg2 - SEC_80_2;
                sec_chan_80_4 = channel->ic_vhtop_ch_freq_seg2 - SEC_80_1;
            }
            else {
                sec_level = UPPER_FREQ_SLOT;
                sec_chan_80_1 = channel->ic_vhtop_ch_freq_seg2 + SEC_80_1;
                sec_chan_80_2 = channel->ic_vhtop_ch_freq_seg2 + SEC_80_2;
                sec_chan_80_3 = channel->ic_vhtop_ch_freq_seg2 + SEC_80_3;
                sec_chan_80_4 = channel->ic_vhtop_ch_freq_seg2 + SEC_80_4;
            }
            /* Derive Secondary 20, 40 from Primary 80 */
            pri_center_ch = channel->ic_vhtop_ch_freq_seg2 - PRI_80_CENTER*sec_level;

            ieee80211_acs_derive_sec_chans_with_mode(ieee80211_chan2mode(channel),
                                              pri_center_ch, ieee_chan, &sec_chan,
                                               &sec_chan_40_1, &sec_chan_40_2);
            /* Adjacent channels are 4 channels before the band and 4 channels are
               after the band */
            first_adj_chan = (channel->ic_vhtop_ch_freq_seg2 - 14) - 2*ADJ_CHANS;
            last_adj_chan =  (channel->ic_vhtop_ch_freq_seg2 + 14) + 2*ADJ_CHANS;

            break;
        case IEEE80211_CHAN_11NA_HT20:
        case IEEE80211_CHAN_11AC_VHT20:
        case IEEE80211_MODE_11AXA_HE20:
        default: /* neither HT40+ nor HT40-, finish this call */
            sec_chan = ieee_chan;
            first_adj_chan = sec_chan - ADJ_CHANS;
            last_adj_chan = ieee_chan + ADJ_CHANS;
            break;
    }
    if((sec_chan != ieee_chan) && (sec_chan >= 0)) {

        if( (acs->acs_noisefloor[sec_chan] != NF_INVALID)
                && ( acs->acs_noisefloor[sec_chan] >= acs->acs_noisefloor_threshold) ) {
            adj_chan_stats->adj_chan_flag |= ADJ_CHAN_SEC_NF_FLAG;
        }
    }

    if(sec_chan_40_1 & sec_chan_40_2) {

       if (sec_chan_40_1 >= 0) {
           if( (acs->acs_noisefloor[sec_chan_40_1] != NF_INVALID)
                    && (acs->acs_noisefloor[sec_chan_40_1] >= acs->acs_noisefloor_threshold )) {
               adj_chan_stats->adj_chan_flag |= ADJ_CHAN_SEC1_NF_FLAG;
           }
       }

       if (sec_chan_40_2 >= 0) {
           if( (acs->acs_noisefloor[sec_chan_40_2] !=NF_INVALID)
                   && (acs->acs_noisefloor[sec_chan_40_2] >= acs->acs_noisefloor_threshold )) {
               adj_chan_stats->adj_chan_flag |= ADJ_CHAN_SEC2_NF_FLAG;
           }
       }
    }

    if(sec_chan_80_1 && sec_chan_80_2 && sec_chan_80_3 && sec_chan_80_4) {
        if (sec_chan_80_1 >= 0) {
            if( (acs->acs_noisefloor[sec_chan_80_1] != NF_INVALID)
                     && (acs->acs_noisefloor[sec_chan_80_1] >= acs->acs_noisefloor_threshold )) {
                adj_chan_stats->adj_chan_flag |= ADJ_CHAN_SEC3_NF_FLAG;
            }
        }

        if (sec_chan_80_2 >= 0) {
            if( (acs->acs_noisefloor[sec_chan_80_2] !=NF_INVALID)
                    && (acs->acs_noisefloor[sec_chan_80_2] >= acs->acs_noisefloor_threshold )) {
                adj_chan_stats->adj_chan_flag |= ADJ_CHAN_SEC4_NF_FLAG;
            }
        }

        if (sec_chan_80_3 >= 0) {
            if( (acs->acs_noisefloor[sec_chan_80_3] !=NF_INVALID)
                    && (acs->acs_noisefloor[sec_chan_80_3] >= acs->acs_noisefloor_threshold )) {
                adj_chan_stats->adj_chan_flag |= ADJ_CHAN_SEC5_NF_FLAG;
            }
        }

        if (sec_chan_80_4 >= 0) {
            if( (acs->acs_noisefloor[sec_chan_80_4] !=NF_INVALID)
                    && (acs->acs_noisefloor[sec_chan_80_4] >= acs->acs_noisefloor_threshold )) {
                adj_chan_stats->adj_chan_flag |= ADJ_CHAN_SEC6_NF_FLAG;
            }
        }


    }

    eacs_trace(EACS_DBG_ADJCH,("ieee_chan %d sec_chan %d first_adj_chan %d last_adj_chan %d",
                ieee_chan, sec_chan, first_adj_chan, last_adj_chan));
    eacs_trace(EACS_DBG_ADJCH,("sec_chan_40_1  %d sec_chan_40_2 %d  FLAG %x",
                sec_chan_40_1, sec_chan_40_2, adj_chan_stats->adj_chan_flag));




    /*Update EACS plus parameter */
    adj_chan_stats->adj_chan_loadsum = 0;
    adj_chan_stats->adj_chan_rssisum = 0;
    adj_chan_stats->adj_chan_srsum = 0;

    for (k = first_adj_chan ; (k <= last_adj_chan) && (k > 0); k += 2) {
        int effchfactor;

        if (k == ieee_chan)  continue;

        effchfactor =  k - ieee_chan;

        if(effchfactor < 0)
            effchfactor = 0 - effchfactor;

        effchfactor = effchfactor >> 1;
        if(effchfactor == 0)  effchfactor =1;

        if((acs->acs_noisefloor[k] != NF_INVALID) && (acs->acs_noisefloor[k] >= acs->acs_noisefloor_threshold )){

            eacs_trace(EACS_DBG_ADJCH,("ADJ NF exceeded add 100 each to rssi and load %d ",acs->acs_noisefloor[k]));
            adj_chan_stats->adj_chan_loadsum += 100 / effchfactor;
            adj_chan_stats->adj_chan_rssisum += 100 / effchfactor ;
        }
        else{
            adj_chan_stats->adj_chan_loadsum += (acs->acs_chan_load[k] / effchfactor) ;
            adj_chan_stats->adj_chan_rssisum += (acs->acs_chan_rssi[k] / effchfactor) ;
        }

        eacs_trace(EACS_DBG_ADJCH,("ADJ: chan %d sec %d Adj ch: %d ch load: %d rssi: %d effchfactor  %d loadsum %d rssisum %d ",
                    ieee_chan,sec_chan, k, acs->acs_chan_load[k], acs->acs_chan_rssi[k], effchfactor,
                    adj_chan_stats->adj_chan_loadsum,adj_chan_stats->adj_chan_rssisum ));

        adj_chan_stats->adj_chan_srsum += ((ACS_DEFAULT_SR_LOAD / effchfactor) * acs->acs_srp_supported[k]);

        eacs_trace(EACS_DBG_ADJCH,("ADJ: nsr %d ,sr primary load: %d, sr adj load %d", acs->acs_srp_supported[k], acs->acs_srp_load[k], adj_chan_stats->adj_chan_srsum));
    }

    eacs_trace(EACS_DBG_ADJCH,("Chan %d : ADJ rssi %d ADJ load %d ",
                ieee_chan,adj_chan_stats->adj_chan_rssisum,adj_chan_stats->adj_chan_loadsum));


    eacs_trace(EACS_DBG_ADJCH, ("Adj ch stats valid: %d ind: %d rssi: %d load: %d",
                adj_chan_stats->if_valid_stats, adj_chan_stats->adj_chan_idx,
                adj_chan_stats->adj_chan_rssi, adj_chan_stats->adj_chan_load ));

#undef ADJ_CHANS
}

int
eacs_plus_filter(
        ieee80211_acs_t acs,
        const char *pristr,
        int *primparam,
        const char *secstr,
        int *secparam,
        int primin,
        int privar,
        unsigned int rejflag,
        int *minval
        )
{
    int minix, i, cur_chan, priparamval, secparamval;
    struct ieee80211_ath_channel *channel;
    int findmin = 1, rejhigher = 1;

    eacs_trace(EACS_DBG_LVL1,(" %s, %s min %d, var %d regflag %x",pristr, secstr, primin, privar, rejflag));
    minix = -1;
    if(*minval == 0)
        findmin = 0;
    if(*minval == 1){
        rejhigher =0;
        *minval = 0xFFF;
    }



    for(i = 0; i < acs->acs_nchans ; i++)
    {
        channel =  acs->acs_chans[i];
        cur_chan =  ieee80211_chan2ieee(acs->acs_ic, channel);

         /* WAR to handle channel 5,6,7 for HT40 mode (2 entries of channel are present) */
        if(((cur_chan >= 5) &&  (cur_chan <= 7))&& IEEE80211_IS_CHAN_11NG_HT40MINUS(channel)) {
            cur_chan = 15 + (cur_chan - 5);
        }
        if(acs->acs_channelrejflag[cur_chan]){
            eacs_trace(EACS_DBG_RSSI,("chan %d: Rej flags %x\n ", cur_chan, acs->acs_channelrejflag[cur_chan]));
            continue;
        }
        priparamval = primparam[cur_chan];
        secparamval = secparam [cur_chan];


        if(rejhigher){

            if(  ( priparamval - primin)  > privar ){
                acs->acs_channelrejflag[cur_chan] |= rejflag;
                eacs_trace(EACS_DBG_RSSI,("Filter Rej %s Param Over %s ---Chan %d   Rej cur %d min %d delta %d  allowed %d  "
                            ,secstr, pristr, cur_chan, priparamval, primin, (priparamval- primin), privar ));
            }
        }else{
            if(  ( priparamval - primin) <  privar ){
                acs->acs_channelrejflag[cur_chan] |= rejflag;
                eacs_trace(EACS_DBG_RSSI,("Filter Rej %s Param Over %s ---Chan %d   Rej cur %d min %d delta %d  allowed %d  "
                            ,secstr, pristr, cur_chan, priparamval, primin, (priparamval- primin)*100/(primin+1), privar ));
            }
        }
        if(!(acs->acs_channelrejflag[cur_chan] & rejflag))
            eacs_trace(EACS_DBG_RSSI,("Filter Accept %s Param Over %s ---Chan %d    cur %d min %d delta %d  allowed %d  "
                        ,secstr, pristr, cur_chan, priparamval, primin, (priparamval- primin), privar ));

        if(!acs->acs_channelrejflag[cur_chan] ){
            if(findmin ){
                if(  secparamval  <  *minval ){
                    minix = i;
                    *minval = secparamval;
                }
            }
            else{
                if(  secparamval  >  *minval ){
                    minix = i;
                    *minval = secparamval;
                }
            }
        }
    }
    return minix;
}

static int acs_find_secondary_80mhz_chan(ieee80211_acs_t acs, u_int8_t prichan)
{
    u_int8_t flag = 0;
    u_int16_t i;
    u_int8_t chan;
    struct ieee80211_ath_channel *channel;

    do {
       for (i = 0; i < acs->acs_nchans; i++) {
           channel = acs->acs_chans[i];
           chan = ieee80211_chan2ieee(acs->acs_ic, channel);
           /* skip Primary 80 mhz channel */
           if((acs->acs_channelrejflag[chan] & ACS_REJFLAG_PRIMARY_80_80) ||
              (acs->acs_channelrejflag[chan] & ACS_REJFLAG_NO_SEC_80_80)) {
               continue;
           }
            /* Skip channels, if high NF is seen in its primary and secondary channels */
           if(!flag && (acs->acs_channelrejflag[chan] & ACS_REJFLAG_HIGHNOISE)) {
               continue;
           }
           if(!flag && acs->acs_adjchan_flag[chan]) {
               continue;
           }
           return i;
       }
       flag++;
    } while(flag < 2); /* loop for two iterations only */
    /* It should not reach here */
    return -1;
}

/**
 * @brief: EMIWAR to Skip the Secondary channel based on the EMIWAR Value
 *
 * @param acs
 * @param primary_channel
 *
 */
static
void acs_emiwar80p80_skip_secondary_80mhz_chan(ieee80211_acs_t acs,struct ieee80211_ath_channel *primary_channel) {

    struct ieee80211com *ic = NULL;

    if((NULL == acs) || (NULL == primary_channel) || (NULL == acs->acs_ic))
        return;

    ic = acs->acs_ic;

    if(ic->ic_emiwar_80p80 == EMIWAR_80P80_FC1GTFC2) {
        /* WAR to skip all secondary 80 whose channel center freq ie less than primary channel center freq*/
        u_int8_t sec_channelrej_list[] = {36,40,44,48 /*1st 80MHz Band*/
                                          ,52,56,60,64 /*2nd 80MHz Band*/
                                          ,100,104,108,112/*3rd 80MHz Band*/
                                          ,116,120,124,128/*4th 80MHz Band*/
                                          ,132,136,140,144/*5th 80MHz Band*/};
        u_int8_t channelrej_num = 0;
        u_int8_t chan_i;

        switch(primary_channel->ic_vhtop_ch_freq_seg1) {
            case 58:
                channelrej_num = 4; /*Ignore the 1st 80MHz Band*/
                break;
            case 106:
                channelrej_num = 8; /*Ignore the 1st,2nd 80MHz Band*/
                break;
            case 122:
                channelrej_num = 12; /*Ignore the 1st,2nd and 3rd 80MHz Band*/
                break;
            case 138:
                channelrej_num = 16; /*Ignore the 1st,2nd,3rd and 4th 80MHz Band*/
                break;
            case 155:
                channelrej_num = 20; /*Ignore the 1st,2nd,3rd,4th and 5th 80MHz Band*/
                break;
            default:
                channelrej_num = 0;
                break;
        }

        for(chan_i=0;chan_i < channelrej_num;chan_i++)
            acs->acs_channelrejflag[sec_channelrej_list[chan_i]] |= ACS_REJFLAG_NO_SEC_80_80;

    }else if((ic->ic_emiwar_80p80 == EMIWAR_80P80_BANDEDGE) && (primary_channel->ic_vhtop_ch_freq_seg1 == 155)) {
        /* WAR to skip 42 as secondary 80, if 155 as primary 80 center freq */
        acs->acs_channelrejflag[36] |= ACS_REJFLAG_NO_SEC_80_80;
        acs->acs_channelrejflag[40] |= ACS_REJFLAG_NO_SEC_80_80;
        acs->acs_channelrejflag[44] |= ACS_REJFLAG_NO_SEC_80_80;
        acs->acs_channelrejflag[48] |= ACS_REJFLAG_NO_SEC_80_80;
    }
}

/**
 * @brief: Select the channel with least nbss
 *
 * @param ic
 *
 */
static INLINE struct ieee80211_ath_channel *
ieee80211_acs_select_min_nbss(struct ieee80211com *ic)
{
#define ADJ_CHANS 8
#define NUM_CHANNELS_2G     11
#define CHANNEL_2G_FIRST    1
#define CHANNEL_2G_LAST     14
#define CHANNEL_5G_FIRST  36

    ieee80211_acs_t acs = ic->ic_acs;
    struct ieee80211_ath_channel *channel = NULL, *best_channel = NULL, *channel_sec80 = NULL;
    u_int8_t i, ieee_chan, min_bss_count = 0xFF;
    u_int8_t channel_num = 0;
    int bestix = -1, bestix_sec80 = -1;
    u_int8_t pri20 = 0;
    u_int8_t sec_20 = 0;
    u_int8_t sec40_1 = 0;
    u_int8_t sec40_2 = 0;
    u_int8_t chan_i, first_adj_chan, last_adj_chan;

    channel = ieee80211_find_dot11_channel(ic, 0, 0, acs->acs_vap->iv_des_mode);
    channel_num = ieee80211_chan2ieee(acs->acs_ic, channel);

    if( (channel_num >= CHANNEL_2G_FIRST) && (channel_num <= CHANNEL_2G_LAST) ) {
        /* mode is 2.4 G */
        for(i = 0; i < acs->acs_nchans; i++)
        {
            channel = acs->acs_chans[i];
            ieee_chan = ieee80211_chan2ieee(acs->acs_ic, channel);
            if(acs->acs_2g_allchan == 0) {
                if((ieee_chan != 1) && (ieee_chan != 6) && (ieee_chan != 11)) {
                  continue;
                }
            }
#if WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
            if(ic->ic_primary_allowed_enable &&
                        !ieee80211_check_allowed_prim_chanlist(ic, ieee_chan))
            {
                    eacs_trace(EACS_DBG_DEFAULT,("Channel %d is not a primary allowed channel \n",
                    ieee_chan));
                    continue;
            }
#endif
            if (min_bss_count > acs->acs_chan_nbss[ieee_chan])
            {
                min_bss_count = acs->acs_chan_nbss[ieee_chan];
                best_channel = channel;
            }
            eacs_trace(EACS_DBG_DEFAULT,("select  channel %d and Bss counter is %d",
                        ieee80211_chan2ieee(acs->acs_ic, best_channel),min_bss_count));
        }
    } else if(channel_num >= CHANNEL_5G_FIRST) {
        /* mode is 5 G */
        for (i = 0; i < acs->acs_nchans; i++) {
            channel = acs->acs_chans[i];
            if (ieee80211_acs_check_interference(channel, acs->acs_vap) &&
                (ieee80211_find_dot11_channel(acs->acs_ic,
                          ieee80211_chan2ieee(acs->acs_ic, channel), channel->ic_vhtop_ch_freq_seg2,
                             acs->acs_vap->iv_des_mode) != NULL)) {
                    continue;
            }
            ieee_chan = ieee80211_chan2ieee(acs->acs_ic, channel);
            if(acs->acs_channelrejflag[ieee_chan] & ACS_REJFLAG_NO_PRIMARY_80_80)
                    continue;

            if (min_bss_count > acs->acs_chan_nbss[ieee_chan])
            {
                min_bss_count = acs->acs_chan_nbss[ieee_chan];
                best_channel = acs->acs_chans[i];
                bestix = i;
            }
        }
        if((best_channel) && (acs->acs_vap->iv_des_mode == IEEE80211_MODE_11AC_VHT80_80)) {
           channel = acs->acs_chans[bestix];
           pri20 = ieee80211_chan2ieee(acs->acs_ic, channel);
           ieee80211_acs_derive_sec_chans_with_mode(acs->acs_vap->iv_des_mode,
                                channel->ic_vhtop_ch_freq_seg1,
                                 pri20, &sec_20, &sec40_1, &sec40_2);

           acs->acs_channelrejflag[pri20] |= ACS_REJFLAG_PRIMARY_80_80;
           acs->acs_channelrejflag[sec_20] |= ACS_REJFLAG_PRIMARY_80_80;
           acs->acs_channelrejflag[sec40_1] |= ACS_REJFLAG_PRIMARY_80_80;
           acs->acs_channelrejflag[sec40_2] |= ACS_REJFLAG_PRIMARY_80_80;

           /* EMIWAR 80P80 to skip secondary 80 */
           if(ic->ic_emiwar_80p80) {
               acs_emiwar80p80_skip_secondary_80mhz_chan(acs,channel);
               /*
               * WAR clear the Primary Reject channle flag
               * So channel can be chose best secondary 80 mhz channel
               */

               for (i = 0; i < acs->acs_nchans; i++) {
                   ieee_chan = ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[i]);
                   acs->acs_channelrejflag[ieee_chan] &= ~ACS_REJFLAG_NO_PRIMARY_80_80;
               }
           }
           /* mark channels as unusable for secondary 80 */
           first_adj_chan = (channel->ic_vhtop_ch_freq_seg1 - 6) - 2*ADJ_CHANS;
           last_adj_chan =  (channel->ic_vhtop_ch_freq_seg1 + 6) + 2*ADJ_CHANS;
           for(chan_i=first_adj_chan;chan_i <= last_adj_chan; chan_i += 4) {
              if ((chan_i >= (channel->ic_vhtop_ch_freq_seg1 -6)) && (chan_i <= (channel->ic_vhtop_ch_freq_seg1 +6))) {
                 continue;
              }
              acs->acs_channelrejflag[chan_i] |= ACS_REJFLAG_NO_SEC_80_80;
           }
           bestix_sec80 = acs_find_secondary_80mhz_chan(acs, bestix);
           if(bestix_sec80 == -1) {
              printk("Issue in Random secondary 80mhz channel selection \n");
           }
           else {
              channel_sec80 = acs->acs_chans[bestix_sec80];
              best_channel->ic_vhtop_ch_freq_seg2 = channel_sec80->ic_vhtop_ch_freq_seg1;
           }
        }
        eacs_trace(EACS_DBG_DEFAULT,("select  channel %d and Bss counter is %d\r",
                     ieee80211_chan2ieee(acs->acs_ic, best_channel),min_bss_count));
    }
#undef NUM_CHANNELS_2G
#undef CHANNEL_2G_FIRST
#undef CHANNEL_2G_LAST
#undef CHANNEL_5G_FIRST
#undef ADJ_CHANS
    return best_channel;
}

/*  This function finds the best channe using ACS parameters */
static int acs_find_best_channel_ix(ieee80211_acs_t acs, int rssivarcor,
                                       int rssimin)
{
    int rssimix,minload,minloadix,maxregpower,maxregpowerindex;
    int minsrix, minsrload;
    /*Find Best Channel load channel out of Best RSSI Channel with variation of 20%*/
    eacs_trace(EACS_DBG_DEFAULT,("RSSI Filter \n"));
    minsrload = 0xFFFF;
    minsrix = eacs_plus_filter(acs,"RSSI",acs->acs_chan_rssitotal, "SR",
                             acs->acs_srp_load, rssimin,
                             acs->acs_rssivar + rssivarcor, ACS_REJFLAG_RSSI,
                             &minsrload);

    if ((minsrix > 0) && (minsrix < IEEE80211_ACS_CHAN_MAX))
        eacs_trace(EACS_DBG_CHLOAD,("Minimum SR Channel %d \n",
                 ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[minsrix])));

    eacs_trace(EACS_DBG_DEFAULT,("SR Filter \n"));
    minload = 0xFFFF;
    minloadix  = eacs_plus_filter(acs,"SR",acs->acs_srp_load, "CHLOAD",
                             acs->acs_chan_loadsum, minsrload,
                             acs->acs_srvar, ACS_REJFLAG_SPATIAL_REUSE,
                             &minload );

    if ((minloadix > 0) && (minloadix < IEEE80211_ACS_CHAN_MAX))
        eacs_trace(EACS_DBG_CHLOAD,("Minimum Ch load Channel %d ",
                 ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[minloadix])));

    eacs_trace(EACS_DBG_DEFAULT,("Regulatory Filter "));
    maxregpower =0;
    maxregpowerindex  = eacs_plus_filter(acs,"CH LOAD",acs->acs_chan_loadsum,
                                          "REGPOWER", acs->acs_chan_regpower,
                                          minload, acs->acs_chloadvar,
                                          ACS_REJFLAG_CHANLOAD, &maxregpower );

    if ((maxregpowerindex > 0) && (maxregpowerindex < IEEE80211_ACS_CHAN_MAX))
        eacs_trace(EACS_DBG_REGPOWER,("Max Power of Channel %d ",
          ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[maxregpowerindex])));


    rssimin = 1;
    rssimix  = eacs_plus_filter(acs,"REG POWER",acs->acs_chan_regpower,
                                  "RSSI TOTOAL", acs->acs_chan_rssitotal,
                        maxregpower, 0 ,  ACS_REJFLAG_REGPOWER, &rssimin );
    return rssimix;
}

/*
 * In 5 GHz, if the channel is unoccupied the max rssi
 * should be zero; just take it.Otherwise
 * track the channel with the lowest rssi and
 * use that when all channels appear occupied.
 */
static INLINE struct ieee80211_ath_channel *
ieee80211_acs_find_best_11na_centerchan(ieee80211_acs_t acs)
{
#define ADJ_CHANS 8
    struct ieee80211_ath_channel *channel, *channel_sec80;
    struct ieee80211_ath_channel *tmpchannel;
    int i;
    int cur_chan;
    struct ieee80211_acs_adj_chan_stats *adj_chan_stats;
    int minnf,nfmix,rssimin,rssimix,bestix, bestix_sec80;
    int tmprssimix = 0;
    int rssivarcor=0,prinfclean = 0, secnfclean = 0;
    int reg_tx_power;
    u_int8_t pri20, sec_20, sec40_1, sec40_2;
    u_int8_t vht_center_freq;
    u_int8_t chan_i, first_adj_chan, last_adj_chan;
    struct ieee80211com *ic = acs->acs_ic;
    uint32_t dfs_reg = 0;
    struct wlan_objmgr_pdev *pdev;
    struct wlan_objmgr_psoc *psoc;
    struct wlan_lmac_if_reg_rx_ops *reg_rx_ops;
#if ATH_SUPPORT_VOW_DCS
    u_int32_t nowms =  (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    u_int8_t ieee_chan, intr_chan_cnt = 0;

#define DCS_AGING_TIME 300000 /* 5 mins */
#define DCS_PENALTY    30     /* channel utilization in % */
#define DCS_DISABLE_THRESH 3  /* disable dcs, after these many channel change triggers */
    for (i = 0; i < acs->acs_nchans; i++) {
        channel = acs->acs_chans[i];
        if (ieee80211_acs_check_interference(channel, acs->acs_vap)) {
            continue;
        }
        ieee_chan = ieee80211_chan2ieee(acs->acs_ic, channel);
        if ( acs->acs_intr_status[ieee_chan] ){
            if ((nowms >= acs->acs_intr_ts[ieee_chan]) &&
                    ((nowms - acs->acs_intr_ts[ieee_chan]) <= DCS_AGING_TIME)){
                acs->acs_chan_load[ieee_chan] += DCS_PENALTY;
                intr_chan_cnt = acs->acs_intr_status[ieee_chan];
            }
            else{
                acs->acs_intr_status[ieee_chan] = 0;
            }
        }
    }
    if ( intr_chan_cnt >= DCS_DISABLE_THRESH ){
        /* To avoid frequent channel change,if channel change is
         * triggered three times in last 5 mins, disable dcs.
         */
        ic->ic_disable_dcsim(ic);
        /* Setting DCS Enable timer to enable DCS after a defined time interval
         * Flag used to check if timer is already set as DCS is still running for
         * CW interference mitigation. Avoids repeated setting of timer.
         */
        if(!acs->is_dcs_enable_timer_set) {
            OS_SET_TIMER(&acs->dcs_enable_timer, acs->dcs_enable_time*1000);
            acs->is_dcs_enable_timer_set = TRUE;
        }
    }
#undef DCS_AGING_TIME
#undef DCS_PENALTY
#undef DCS_DISABLE_THRESH
#endif



    eacs_trace(EACS_DBG_LVL1,("entry"));
    adj_chan_stats = (struct ieee80211_acs_adj_chan_stats *) OS_MALLOC(acs->acs_osdev,
            IEEE80211_CHAN_MAX * sizeof(struct ieee80211_acs_adj_chan_stats), 0);

    if (adj_chan_stats) {
        OS_MEMZERO(adj_chan_stats, sizeof(struct ieee80211_acs_adj_chan_stats)*IEEE80211_CHAN_MAX);
    }
    else {
        qdf_print("%s: malloc failed",__func__);
        return NULL;//ENOMEM;
    }


    acs->acs_minrssi_11na = 0xffffffff; /* Some large value */

    /* Scan through the channel list to find the best channel */
    minnf = 0xFFFF; /*Find max NF*/
    nfmix = -1;

    rssimin = 0xFFFF;
    rssimix= -1;

    if((acs->acs_vap->iv_des_mode == IEEE80211_MODE_11AC_VHT80_80) && \
          (acs->acs_ic->ic_emiwar_80p80 == EMIWAR_80P80_FC1GTFC2)) {
          /*
          * When EMIWAR is EMIWAR_80P80_FC1GTFC2
          * there will be no 80_80 channel combination for 149,153,157,161,
          * as we forcefully added the 80MHz channel to acs_chan list
          * mark those channle as NO Primary Select so acs will consider as primary channel
          * but can consider these channel as secondary channel.
          */
          for (i = 0; i < acs->acs_nchans; i++) {
              if(IEEE80211_MODE_11AC_VHT80_80 != ieee80211_chan2mode(acs->acs_chans[i])) {
                  cur_chan = ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[i]);
                  acs->acs_channelrejflag[cur_chan] |= ACS_REJFLAG_NO_PRIMARY_80_80;
              }
          }
    }

    pdev = ic->ic_pdev_obj;
    if(pdev == NULL) {
        qdf_print("%s : pdev is null", __func__);
        return NULL;
    }

    psoc = wlan_pdev_get_psoc(pdev);
    if (psoc == NULL) {
        qdf_print("%s : psoc is null", __func__);
        return NULL;
    }

    reg_rx_ops = wlan_lmac_if_get_reg_rx_ops(psoc);
    if (wlan_objmgr_pdev_try_get_ref(pdev, WLAN_REGULATORY_SB_ID) !=
            QDF_STATUS_SUCCESS) {
        return NULL;
    }
    reg_rx_ops->get_dfs_region(pdev, &dfs_reg);
    wlan_objmgr_pdev_release_ref(pdev, WLAN_REGULATORY_SB_ID);

    for (i = 0; i < acs->acs_nchans; i++) {
        channel = acs->acs_chans[i];
        cur_chan = ieee80211_chan2ieee(acs->acs_ic, channel);

         /*
          * WAR to handle channel 5,6,7 for HT40 mode.
          * (2 entries of these channels are present , one set for HT40+ and other for HT40-)
          * So handling these second set of channels 5,6 and 7 as 15,16 and 17 respectivel for HT40-
          */
        if (((cur_chan >= 5) && (cur_chan <= 7)) && IEEE80211_IS_CHAN_11NG_HT40MINUS(channel))
        {
            cur_chan = 15 + (cur_chan - 5);
        }

        /* The BLACKLIST flag need to be preserved for each channel till all
         * the channels are looped, since it is used rejecting the already
         * Ranked channels in the ACS ranking feature */
        if ((acs->acs_ranking) && (acs->acs_channelrejflag[cur_chan] & ACS_REJFLAG_BLACKLIST))
        {
            acs->acs_channelrejflag[cur_chan] &= ACS_REJFLAG_NO_PRIMARY_80_80;
            acs->acs_channelrejflag[cur_chan] |= ACS_REJFLAG_BLACKLIST;
        }
        else {
        /*Clear all the reject flag except ACS_REJFLAG_NO_PRIMARY_80_80*/
            acs->acs_channelrejflag[cur_chan] &= ACS_REJFLAG_NO_PRIMARY_80_80;
        }

        /* Check if it is 5GHz channel  */
        if (!IEEE80211_IS_CHAN_5GHZ(channel)){
            acs->acs_channelrejflag[cur_chan] |= ACS_FLAG_NON5G;
            continue;
        }
        /* Best Channel for VHT BSS shouldn't be the secondary channel of other BSS
         * Do not consider this channel for best channel selection
         */
        if((acs->acs_vap->iv_des_mode == IEEE80211_MODE_AUTO) ||
                (acs->acs_vap->iv_des_mode >= IEEE80211_MODE_11AC_VHT20)) {
            if(acs->acs_sec_chan[cur_chan] == true) {
                eacs_trace(EACS_DBG_DEFAULT, ("Skipping the chan %d for best channel selection If desired mode is AUTO/VHT",cur_chan));
                acs->acs_channelrejflag[cur_chan] |= ACS_REJFLAG_SECCHAN ;
            }
        }
        if(acs->acs_ic->ic_no_weather_radar_chan) {
            if(ieee80211_check_weather_radar_channel(channel)
                    && (dfs_reg == DFS_ETSI_DOMAIN)) {
                acs->acs_channelrejflag[cur_chan] |= ACS_REJFLAG_WEATHER_RADAR ;
                eacs_trace(EACS_DBG_DEFAULT,("Channel rej %d  Weather Radar Flag %x ",cur_chan,acs->acs_channelrejflag[cur_chan]));
                continue;
            }
        }
        /* Check of DFS and other modes where we do not want to use the
         * channel
         */
        if (ieee80211_acs_check_interference(channel, acs->acs_vap)) {
            acs->acs_channelrejflag[cur_chan] |= ACS_REJFLAG_DFS ;
            eacs_trace(EACS_DBG_DEFAULT,("Channel rej %d  DFS  %x ",cur_chan,acs->acs_channelrejflag[cur_chan]));

            continue;
        }

        /* Check if the noise floor value is very high. If so, it indicates
         * presence of CW interference (Video Bridge etc). This channel
         * should not be used
         */

        if ((minnf > acs->acs_noisefloor[cur_chan]) &&
             ((acs->acs_channelrejflag[cur_chan] & ACS_REJFLAG_SECCHAN) == 0) &&
             ((acs->acs_channelrejflag[cur_chan] & ACS_REJFLAG_NO_PRIMARY_80_80) == 0))
        {
            minnf = acs->acs_noisefloor[cur_chan];
            nfmix = i;
        }


        if ((acs->acs_noisefloor[cur_chan] != NF_INVALID) && (acs->acs_noisefloor[cur_chan] >= ic->ic_acs->acs_noisefloor_threshold) ) {
            eacs_trace(EACS_DBG_NF, ( "NF Rej Chan chan: %d maxrssi: %d, nf: %d\n",
                        cur_chan, acs->acs_chan_rssi[cur_chan], acs->acs_noisefloor[cur_chan] ));

            acs->acs_channelrejflag[cur_chan] |= ACS_REJFLAG_HIGHNOISE;
            acs->acs_chan_loadsum[cur_chan] = 100;
            if(acs->acs_chan_rssi[cur_chan] < 100) {
               acs->acs_chan_rssi[cur_chan] = 100;
            }

        }else{
            prinfclean++;
            acs->acs_chan_loadsum[cur_chan] = acs->acs_chan_load[cur_chan];
        }

        acs->acs_chan_rssitotal[cur_chan] = acs->acs_chan_rssi[cur_chan];
        acs->acs_srp_load[cur_chan] = acs->acs_srp_supported[cur_chan] * ACS_DEFAULT_SR_LOAD;

        if( !adj_chan_stats[i].if_valid_stats){
            ieee80211_acs_get_adj_ch_stats(acs, channel, &adj_chan_stats[i]);
            eacs_trace(EACS_DBG_RSSI,("Chan:%d  Channel RSSI %d adj RSSI %d ",
                    cur_chan,acs->acs_chan_rssitotal[cur_chan] ,adj_chan_stats[i].adj_chan_rssisum));
            eacs_trace(EACS_DBG_CHLOAD,("Chan:%d  Channel Load %d  adj load %d ",
                    cur_chan,acs->acs_chan_loadsum[cur_chan] ,adj_chan_stats[i].adj_chan_loadsum ));



            acs->acs_adjchan_load[cur_chan]    =  adj_chan_stats[i].adj_chan_load;
            acs->acs_chan_loadsum[cur_chan]   +=  adj_chan_stats[i].adj_chan_loadsum;
            acs->acs_chan_rssitotal[cur_chan] +=  adj_chan_stats[i].adj_chan_rssisum;
            acs->acs_adjchan_flag[cur_chan]       =  adj_chan_stats[i].adj_chan_flag;
            acs->acs_srp_load[cur_chan] +=  adj_chan_stats[i].adj_chan_srsum;

            if(! acs->acs_adjchan_flag[cur_chan])
                secnfclean++;


        }


        if(!acs->acs_channelrejflag[cur_chan]){
            if(rssimin > acs->acs_chan_rssitotal[cur_chan]){
                rssimin = acs->acs_chan_rssitotal[cur_chan];
                rssimix = i;
            }
        }

        /*
         * ETSI UNII-II Ext band has different limits for STA and AP.
         * The software reg domain table contains the STA limits(23dBm).
         * For AP we adjust the max power(30dBm) dynamically
         */
        if (UNII_II_EXT_BAND(ieee80211_chan2freq(acs->acs_ic, channel))
                && (dfs_reg == DFS_ETSI_DOMAIN)){
            reg_tx_power = MIN( 30, channel->ic_maxregpower+7 );
        }
        else {
            reg_tx_power  = channel->ic_maxregpower;
        }
        eacs_trace(EACS_DBG_REGPOWER,("chan:%d received Tx power is %d: reg tx power is %d ",
                                       cur_chan, acs->acs_chan_regpower[cur_chan], reg_tx_power));
        if(acs->acs_chan_regpower[cur_chan]) {
            acs->acs_chan_regpower[cur_chan] = MIN( acs->acs_chan_regpower[cur_chan], reg_tx_power);
        }
        else {
            acs->acs_chan_regpower[cur_chan] = reg_tx_power;
        }
        eacs_trace(EACS_DBG_REGPOWER,("chan:%d received Tx power is %d: reg tx power is %d \n",
                                       cur_chan, acs->acs_chan_regpower[cur_chan], reg_tx_power));

        /* check neighboring channel load
         * pending - first check the operating mode from beacon( 20MHz/40 MHz) and
         * based on that find the interfering channel
         */
        eacs_trace(EACS_DBG_RSSI,("Channel %d Rssi %d RssiTotal %d nf %d RegPower %d Load %d adjload %d weighted load %d   ",
                    cur_chan,acs->acs_chan_rssi[cur_chan],acs->acs_chan_rssitotal[cur_chan],acs->acs_noisefloor[cur_chan],
                    acs->acs_chan_regpower[cur_chan], acs->acs_chan_load[cur_chan],
                    acs->acs_adjchan_load[cur_chan], acs->acs_chan_loadsum[cur_chan]));

    }

    if(prinfclean == 0){
        eacs_trace(EACS_DBG_DEFAULT, ( "Extreme Exceptional case all Channels flooded with NF"));
        eacs_trace(EACS_DBG_DEFAULT, ( "selecting minnfchang "));
        bestix = nfmix;
        goto selectchan;
    }

    if(secnfclean > 0){
        rssimix =-1;
        rssimin = 0xFFFF;

        /*apply adj channel nf free filter to main rej flag i recalculate rssimin*/
        for (i = 0; i < acs->acs_nchans; i++) {

            channel = acs->acs_chans[i];
            cur_chan = ieee80211_chan2ieee(acs->acs_ic, channel);
            acs->acs_channelrejflag[cur_chan] |= acs->acs_adjchan_flag[cur_chan];
            if(!acs->acs_channelrejflag[cur_chan]){
                if(rssimin > acs->acs_chan_rssitotal[cur_chan]){
                    rssimin = acs->acs_chan_rssitotal[cur_chan];
                    tmprssimix = i;
                }
            }
        }
        /*
        * Don't update the rssimix if all the secondary channels are rejected
        * and Clear the rejection flag
        */
        if(tmprssimix == -1) {
            for (i = 0; i < acs->acs_nchans; i++) {
               channel = acs->acs_chans[i];
               cur_chan = ieee80211_chan2ieee(acs->acs_ic, channel);
               acs->acs_channelrejflag[cur_chan] &= ~acs->acs_adjchan_flag[cur_chan];
            }
        } else {
            rssimix = tmprssimix;
        }

    }else{
        /*Non of the secondary are clean */
        /*Ignor the RSSI use only channle load */
        printk("Rare Scenario All the channel has NF at the secondary channel \n");
        eacs_trace(EACS_DBG_DEFAULT, ( "ignore RSSI metric ,  increase variance to 1000  "));
        rssivarcor=1000;

    }



    if (rssimix > 0) {
        eacs_trace(EACS_DBG_RSSI,("Minimum Rssi Channel %d ",ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[rssimix])));
    }

    bestix = rssimix;

    bestix = acs_find_best_channel_ix(acs, rssivarcor, rssimin);

selectchan :

    acs->acs_11nabestchan = bestix;
    if ((bestix >= 0) && (bestix < IEEE80211_ACS_CHAN_MAX)) {
        channel = acs->acs_chans[bestix];
        acs->acs_status = ACS_SUCCESS;
#if ATH_SUPPORT_VOW_DCS
        ic->ic_eacs_done = 1;
#endif

        eacs_trace(EACS_DBG_DEFAULT,("80mhz index is %d ieee is %d ",bestix, channel->ic_ieee));
        if(acs->acs_vap->iv_des_mode == IEEE80211_MODE_11AC_VHT80_80) {
            /* dervice primary and secondary channels from the channel */
           pri20 = channel->ic_ieee;
            /* Derive secondary channels */
           vht_center_freq = channel->ic_vhtop_ch_freq_seg1;
           ieee80211_acs_derive_sec_chans_with_mode(acs->acs_vap->iv_des_mode,
                                vht_center_freq, pri20, &sec_20, &sec40_1,
                                                         &sec40_2);
            /* Mark Primary 80Mhz channels for not selecting as secondary 80Mhz */
           acs->acs_channelrejflag[pri20] |= ACS_REJFLAG_PRIMARY_80_80;
           acs->acs_channelrejflag[sec_20] |= ACS_REJFLAG_PRIMARY_80_80;
           acs->acs_channelrejflag[sec40_1] |= ACS_REJFLAG_PRIMARY_80_80;
           acs->acs_channelrejflag[sec40_2] |= ACS_REJFLAG_PRIMARY_80_80;

           /* EMIWAR 80P80 to skip secondary 80 */
           if(ic->ic_emiwar_80p80)
               acs_emiwar80p80_skip_secondary_80mhz_chan(acs,channel);
           /* mark channels as unusable for secondary 80 */
           first_adj_chan = (vht_center_freq - 6) - 2*ADJ_CHANS;
           last_adj_chan =  (vht_center_freq + 6) + 2*ADJ_CHANS;
           for(chan_i=first_adj_chan;chan_i <= last_adj_chan; chan_i += 4) {
              if ((chan_i >= (vht_center_freq -6)) && (chan_i <= (vht_center_freq +6))) {
                 continue;
              }
              acs->acs_channelrejflag[chan_i] |= ACS_REJFLAG_NO_SEC_80_80;
           }

           tmprssimix = -1;
           for (i = 0; i < acs->acs_nchans; i++) {
               tmpchannel = acs->acs_chans[i];
               cur_chan = ieee80211_chan2ieee(acs->acs_ic, tmpchannel);
               rssimin = 0xFFFF;
               /*
               *  reset RSSI, CHANLOAD, REGPOWER, NO_PRIMARY_80_80 flags,
               *  to allow it chose best secondary 80 mhz channel
               */
               acs->acs_channelrejflag[cur_chan] = acs->acs_channelrejflag[cur_chan] &
                                                   (~(ACS_REJFLAG_RSSI | ACS_REJFLAG_CHANLOAD
                                                        | ACS_REJFLAG_REGPOWER | ACS_REJFLAG_SECCHAN
                                                        | ACS_REJFLAG_NO_PRIMARY_80_80));
               if(!acs->acs_channelrejflag[cur_chan]){
                   if(rssimin > acs->acs_chan_rssitotal[cur_chan]){
                       rssimin = acs->acs_chan_rssitotal[cur_chan];
                       tmprssimix = i;
                   }
               }
           }
           if(tmprssimix != -1) {
              bestix_sec80 = tmprssimix;
           }
            /* how to handle DFS channels XXX*/
           bestix_sec80 = acs_find_best_channel_ix(acs, rssivarcor, rssimin);
           eacs_trace(EACS_DBG_DEFAULT,("bestix_sec80 index is %d ",bestix_sec80));
            /* Could not find the secondary 80mhz channel, pick random channel as
               secondary 80mhz channel */
           if (!((bestix_sec80 >= 0) && (bestix_sec80 < IEEE80211_ACS_CHAN_MAX))) {
              bestix_sec80 = acs_find_secondary_80mhz_chan(acs, bestix);
              printk("Random channel is selected for secondary 80Mhz, index is %d \n",bestix_sec80);
           }
           if(bestix_sec80 == -1) {
              printk("Issue in Random secondary 80mhz channel selection \n");
           }
           else {
              channel_sec80 = acs->acs_chans[bestix_sec80];
              eacs_trace(EACS_DBG_DEFAULT,("secondary 80mhz index is %d ieee is %d ",bestix_sec80, channel_sec80->ic_ieee));
              channel = ieee80211_find_dot11_channel(acs->acs_ic, channel->ic_ieee, channel_sec80->ic_vhtop_ch_freq_seg1, IEEE80211_MODE_11AC_VHT80_80);
           }
        }
    } else {
        /* If no channel is derived, then pick the random channel(least BSS) for operation */
        printk("ACS failed to derive the channel. So,selecting channel with least BSS \n");
        channel = ieee80211_acs_select_min_nbss(acs->acs_ic);
        acs->acs_status = ACS_FAILED_NBSS;
        if (channel == NULL) {
            if(acs->acs_vap->iv_des_mode == IEEE80211_MODE_11AC_VHT80_80) {
                acs->acs_ic->ic_curchan->ic_vhtop_ch_freq_seg2 = ieee80211_mhz2ieee(acs->acs_ic, 5775, acs->acs_ic->ic_curchan->ic_flags);
            }
            channel = ieee80211_find_dot11_channel(acs->acs_ic, 0, 0, acs->acs_vap->iv_des_mode);
            acs->acs_status = ACS_FAILED_RNDM;
            eacs_trace(EACS_DBG_DEFAULT,("Selected dot11 channel %d  ", ieee80211_chan2ieee(acs->acs_ic, channel)));
        }
        if(channel) {
            printk("random channel is %d \n",channel->ic_ieee);
        }
        /* In case of ACS ranking this is called multiple times and creates
		 * lot of logs on console */
        if (!acs->acs_ranking) {
            ieee80211_acs_scan_report_internal(acs->acs_ic);
        }
    }
    OS_FREE(adj_chan_stats);
    return channel;
#undef ADJ_CHANS
}

struct centerchan {
    u_int8_t count;                                      /* number of chans to average the rssi */
    u_int8_t chanlist[IEEE80211_OVERLAPPING_INDEX_MAX];  /* the possible beacon channels around center chan */
};

static int32_t ieee80211_acs_find_channel_totalrssi(ieee80211_acs_t acs, const u_int8_t *chanlist, u_int8_t chancount, u_int8_t centChan)
{
    u_int8_t chan;
    int i;
    u_int32_t totalrssi = 0; /* total rssi for all channels so far */
    uint32_t total_srload = 0;
    int effchfactor;

    if (chancount <= 0) {
        /* return a large enough number for not to choose this channel */
        return 0xffffffff;
    }

    for (i = 0; i < chancount; i++) {
        chan = chanlist[i];

        effchfactor = chan - centChan;

        if(effchfactor < 0)
            effchfactor = 0 - effchfactor;

        effchfactor += 1;

        totalrssi += acs->acs_chan_rssi[chan]/effchfactor;
        total_srload += acs->acs_srp_supported[chan] * (ACS_DEFAULT_SR_LOAD/effchfactor);
        eacs_trace(EACS_DBG_RSSI,( "chan: %d  effrssi: %d effsrload %d factor %d centr %d nsrp %d",  chan, acs->acs_chan_rssi[chan],(acs->acs_srp_supported[chan] * (ACS_DEFAULT_SR_LOAD/effchfactor)),effchfactor,centChan, acs->acs_srp_supported[chan]));
    }
    acs->acs_chan_rssitotal[centChan] = totalrssi;
    acs->acs_srp_load[centChan] = total_srload;
    eacs_trace(EACS_DBG_RSSI,( "chan: %d final rssi: %d ,total srload %d",  centChan, acs->acs_chan_rssitotal[centChan], acs->acs_srp_load[centChan]));
    return totalrssi;
}

static void ieee80211_get_11ng_overlapping_chans(struct centerchan *centerchans, ieee80211_acs_t acs)
{
    u_int8_t i, j, max_chans;
    u_int32_t first_chfreq, acs_nchfreq, min_freq, low_freq, high_freq;

    if(!IEEE80211_IS_CHAN_2GHZ(acs->acs_chans[0]))
    {
        /* We should never end up here */
        eacs_trace(EACS_DBG_DEFAULT,("%s : Not a 2GHZ channel!! freq : %d  \n",
                   __func__, ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[0])));
        return;
    }
    first_chfreq = 2412;
    acs_nchfreq = acs->acs_chans[acs->acs_nchans - 1]->ic_freq;
    min_freq = acs->acs_chans[0]->ic_freq;

    max_chans = ieee80211_mhz2ieee(acs->acs_ic, acs_nchfreq, 0);
    /* When the scan is happening on dual-band, the first channels added to acs_chans will be of 2GH */
    for(i = (ieee80211_mhz2ieee(acs->acs_ic, min_freq, 0) - 1);min_freq <= acs_nchfreq && i < max_chans && i < IEEE80211_MAX_2G_SUPPORTED_CHAN; min_freq += 5, i++)
    {
       low_freq = (min_freq - 10) < first_chfreq ? first_chfreq : (min_freq - 10);
       high_freq = (min_freq + 10) > acs_nchfreq ? acs_nchfreq : (min_freq + 10);
       for(j = 0; low_freq <= high_freq && j < IEEE80211_OVERLAPPING_INDEX_MAX; j++)
       {
          centerchans[i].chanlist[j] = ieee80211_mhz2ieee(acs->acs_ic, low_freq, 0);
          centerchans[i].count++;
          low_freq += 5;
       }
    }
}

static INLINE struct ieee80211_ath_channel *
ieee80211_acs_find_best_11ng_centerchan(ieee80211_acs_t acs)
{
    struct ieee80211_ath_channel *channel;
    int i;
    u_int8_t chan;
#if 0
    u_int8_t band;
#endif
    unsigned int   totalrssi = 0;
    int            bestix,nfmin , nfmix, loadmin,rssimin,rssimix;
    int            minloadix, maxregpower, maxregpowerindex = 0;
    int minsr_ix;
    int minsrload;
    int            extchan,ht40minus_chan ;
    int rssivarcor=0;
    int64_t mode_mask = (IEEE80211_CHAN_11NG_HT40PLUS |
            IEEE80211_CHAN_11NG_HT40MINUS |IEEE80211_CHAN_11AXG_HE40PLUS | IEEE80211_CHAN_11AXG_HE40MINUS);

#if ATH_SUPPORT_VOW_DCS || WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
    struct ieee80211com *ic = acs->acs_ic;
#endif

    struct centerchan centerchans[IEEE80211_MAX_2G_SUPPORTED_CHAN];

    /*
     * The following center chan data structures are used to calculare the
     * the Total RSSI of beacon seen in the center chan and overlapping channel
     * The channle with minimum Rssi gets piccked up as best channel
     * The channel with 20% Rssi variance with minimum will go for next phase of
     * filteration for channel load.
     */
    OS_MEMSET(centerchans, 0x0, sizeof(centerchans));
    ieee80211_get_11ng_overlapping_chans(centerchans, acs);

    acs->acs_minrssisum_11ng = 0xffffffff;

    bestix  = -1;
    nfmin   = 0xFFFF;
    nfmix   = -1;
    rssimin = 0xFFFF;
    rssimix = -1;

    for (i = 0; i < acs->acs_nchans; i++) {
        channel = acs->acs_chans[i];
        chan = ieee80211_chan2ieee(acs->acs_ic, channel);

        /* The BLACKLIST flag need to be preserved for each channel till all
         * the channels are looped, since it is used rejecting the already
         * Ranked channels in the ACS ranking feature */
        if ((acs->acs_ranking) && (acs->acs_channelrejflag[chan] & ACS_REJFLAG_BLACKLIST)) {
            acs->acs_channelrejflag[chan] = 0;
            acs->acs_channelrejflag[chan] |= ACS_REJFLAG_BLACKLIST;
        }
        else {
            acs->acs_channelrejflag[chan]=0;
        }

        if(chan > 11)
        {
            acs->acs_channelrejflag[chan] |= ACS_REJFLAG_NON2G;
        }
        if(acs->acs_2g_allchan == 0) {
            if ((chan != 1) && (chan != 6) && (chan != 11)) {
                /* Don't bother with other center channels except for 1, 6 & 11 */
                acs->acs_channelrejflag[chan] |= ACS_REJFLAG_NON2G;
            }
        }
        eacs_trace(EACS_DBG_REGPOWER,("chan:%d received Tx power is %d: reg tx power is %d ", chan,
                                      acs->acs_chan_regpower[chan], channel->ic_maxregpower));
        if(acs->acs_chan_regpower[chan]) {
            acs->acs_chan_regpower[chan] = MIN( acs->acs_chan_regpower[chan], channel->ic_maxregpower);
        }
        else {
            acs->acs_chan_regpower[chan] = channel->ic_maxregpower;
        }
        eacs_trace(EACS_DBG_REGPOWER,("chan:%d received Tx power is %d: reg tx power is %d \n", chan,
                                      acs->acs_chan_regpower[chan], channel->ic_maxregpower));
        if (acs->acs_ch_hopping.ch_max_hop_cnt < ACS_CH_HOPPING_MAX_HOP_COUNT &&
                ieee80211com_has_cap_ext(acs->acs_ic,IEEE80211_ACS_CHANNEL_HOPPING)) {
            if(IEEE80211_CH_HOPPING_IS_CHAN_BLOCKED(channel)) {
                acs->acs_channelrejflag[chan] |= ACS_REJFLAG_NON2G;
                continue;
            }
        }
        /* find the Total rssi for this 40Mhz band */
        totalrssi = ieee80211_acs_find_channel_totalrssi(acs, centerchans[chan-1].chanlist, centerchans[chan-1].count, chan);

        /*OBSS check */

        switch (channel->ic_flags & mode_mask)
        {
            case IEEE80211_CHAN_11NG_HT40PLUS:
            case IEEE80211_CHAN_11AXG_HE40PLUS:
                extchan = chan + 4;
                eacs_trace(EACS_DBG_CHLOAD,("IEEE80211_CHAN_11NG_HT40PLUS   ext %d ",extchan));
                break;
            case IEEE80211_CHAN_11NG_HT40MINUS:
            case IEEE80211_CHAN_11AXG_HE40MINUS:
                extchan = chan - 4;
                eacs_trace(EACS_DBG_CHLOAD,(" IEEE80211_CHAN_11NG_HT40MINUS  ext %d ",extchan));
                break;
            default: /* neither HT40+ nor HT40-, finish this call */
                extchan =0;
                eacs_trace(EACS_DBG_CHLOAD,("Error ch flags %llx", (channel->ic_flags & mode_mask)));
                break;
        }
         /* WAR to handle channel 6 for HT40 mode (2 entries of channel 6 are present) */
        if(((chan >= 5)&& (chan <= 7)) && IEEE80211_IS_CHAN_11NG_HT40MINUS(channel)) {

            ht40minus_chan = 15 + (chan-5);
            acs->acs_chan_loadsum[ht40minus_chan] = acs->acs_chan_load[chan];
            acs->acs_chan_regpower[ht40minus_chan] = acs->acs_chan_regpower[chan];
            acs->acs_chan_rssitotal[ht40minus_chan] = acs->acs_chan_rssitotal[chan];
            acs->acs_channelrejflag[ht40minus_chan] = acs->acs_channelrejflag[chan];
            acs->acs_noisefloor[ht40minus_chan] = acs->acs_noisefloor[chan];
            acs->acs_srp_load[ht40minus_chan] = acs->acs_srp_load[chan];
            chan = ht40minus_chan;
        }
        else {
            acs->acs_chan_loadsum[chan] = acs->acs_chan_load[chan];
        }
        if(acs->acs_2g_allchan == 0) {
           if ((chan != 1) && (chan != 6) && (chan != 11) && (chan != 16)) {
                /* Don't bother with other center channels except for 1, 6 & 11 */
                /* channel 16 means channel 6 HT40- */
                acs->acs_channelrejflag[chan] |= ACS_REJFLAG_NON2G;
                continue;
            }
        }
        if((extchan > 0) && (extchan < IEEE80211_ACS_CHAN_MAX)) {
            acs->acs_srp_load[chan] += acs->acs_srp_load[extchan]/2;
            if ((acs->acs_noisefloor[extchan] !=NF_INVALID) && (acs->acs_noisefloor[extchan] < ACS_11NG_NOISE_FLOOR_REJ))
                acs->acs_chan_loadsum[chan] += acs->acs_chan_load[extchan]/2;
            else
                acs->acs_chan_loadsum[chan] += 50;
        }

        if((extchan > 0) && (extchan < IEEE80211_ACS_CHAN_MAX)) {
            eacs_trace(EACS_DBG_CHLOAD,( "Chan Load%d =%d ext_chan%d = %d SR load %d, SR extn load %d",
                         chan, acs->acs_chan_load[chan], extchan, acs->acs_chan_load[extchan], acs->acs_srp_load[chan], acs->acs_srp_load[extchan]));
        }
        eacs_trace(EACS_DBG_RSSI,( " chan: %d beacon RSSI %di Noise FLoor %d ",  chan,
                                      totalrssi,acs->acs_noisefloor[chan]));


        if( nfmin > acs->acs_noisefloor[chan]){
            nfmin = acs->acs_noisefloor[chan];
            nfmix = i;
        }


        if ( (acs->acs_noisefloor[chan] !=NF_INVALID) && (acs->acs_noisefloor[chan] > ACS_11NG_NOISE_FLOOR_REJ)){
            eacs_trace(EACS_DBG_NF,("NF Skipping Channel %d : NF %d  ",chan,acs->acs_noisefloor[chan]));
            acs->acs_channelrejflag[chan]|= ACS_REJFLAG_HIGHNOISE;
            acs->acs_chan_loadsum[chan] += 100;
            continue;
        }
        if( rssimin > totalrssi){
            rssimin = totalrssi;
            rssimix = i;

        }

    }

    if(rssimin == -1){
        eacs_trace(EACS_DBG_DEFAULT, ("Unlikely scenario where all channels are heaving high noise"));
        bestix =nfmix;
        goto selectchan;

    }

    bestix = rssimix;


    /*Find Best Channel load channel out of Best RSSI Channel with variation of 20%*/
    eacs_trace(EACS_DBG_DEFAULT,("RSSI Filter \n"));
    minsrload = 0xFFFF;
    minsr_ix = eacs_plus_filter(acs,"RSSI",acs->acs_chan_rssitotal, "SR", acs->acs_srp_load,
            rssimin, acs->acs_rssivar + rssivarcor, ACS_REJFLAG_RSSI, &minsrload);

    if ((minsr_ix > 0) && (minsr_ix < IEEE80211_ACS_CHAN_MAX))
        eacs_trace(EACS_DBG_CHLOAD,("Minimum SR Channel %d \n",
                 ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[minsr_ix])));

    eacs_trace(EACS_DBG_DEFAULT,("SR Filter \n"));
    loadmin = 0xFFFF;
    minloadix = eacs_plus_filter(acs,"SR",acs->acs_srp_load, "CHLOAD",
                             acs->acs_chan_loadsum, minsrload,
                             acs->acs_srvar, ACS_REJFLAG_SPATIAL_REUSE,
                             &loadmin);

    if ((minloadix > 0) && (minloadix < IEEE80211_ACS_CHAN_MAX))
        eacs_trace(EACS_DBG_CHLOAD,("Minimum Ch load Channel %d ",ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[minloadix])));

    eacs_trace(EACS_DBG_DEFAULT,("Regulatory Filter "));
    maxregpower =0;
    maxregpowerindex  = eacs_plus_filter(acs,"CH LOAD",acs->acs_chan_loadsum, "REGPOWER", acs->acs_chan_regpower,
            loadmin, acs->acs_chloadvar,  ACS_REJFLAG_CHANLOAD, &maxregpower );


    if ((maxregpowerindex > 0) && (maxregpowerindex < IEEE80211_ACS_CHAN_MAX))
        eacs_trace(EACS_DBG_REGPOWER,("Max Power of Channel %d ",ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[maxregpowerindex])));


    rssimin = 1;
    rssimix  = eacs_plus_filter(acs,"REG POWER",acs->acs_chan_regpower, "RSSI TOTOAL", acs->acs_chan_rssitotal,
            maxregpower, 0 ,  ACS_REJFLAG_REGPOWER, &rssimin );


    bestix = rssimix;

selectchan:


    acs->acs_11ngbestchan = bestix;
    if ((bestix >= 0) && (bestix < IEEE80211_ACS_CHAN_MAX)) {
        channel = acs->acs_chans[bestix];

        acs->acs_status = ACS_SUCCESS;
#if ATH_SUPPORT_VOW_DCS
        ic->ic_eacs_done = 1;
#endif
        eacs_trace(EACS_DBG_CHLOAD,("Best Channel Selected %d rssi %d nf %d ",
                ieee80211_chan2ieee(acs->acs_ic, channel),rssimin,
                acs->acs_noisefloor[ieee80211_chan2ieee(acs->acs_ic, channel)] ));
    } else {
         /* If no channel is derived, then pick the random channel(least BSS) for operation */
        channel = ieee80211_acs_select_min_nbss(acs->acs_ic);
        acs->acs_status = ACS_FAILED_NBSS;
        if (channel == NULL) {
#if WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
                if(ic->ic_primary_allowed_enable) {
                        channel = ieee80211_find_dot11_channel(acs->acs_ic,
                                acs->acs_ic->ic_primary_chanlist->chan[0],
                                0,
                                acs->acs_vap->iv_des_mode);
                } else
#endif
            channel = ieee80211_find_dot11_channel(acs->acs_ic, 0, 0, acs->acs_vap->iv_des_mode);
            acs->acs_status = ACS_FAILED_RNDM;
            eacs_trace(EACS_DBG_DEFAULT,("Selected dot11 channel %d  ", ieee80211_chan2ieee(acs->acs_ic, channel)));
        }
        printk("ACS failed to derive the channel. So,selecting random channel \n");

        /* In case of ACS ranking this is called multiple times and creates lot of logs on console */
        if (!acs->acs_ranking) {
            ieee80211_acs_scan_report_internal(acs->acs_ic);
        }
    }

    return channel ;


}

static INLINE struct ieee80211_ath_channel *
ieee80211_acs_find_best_auto_centerchan(ieee80211_acs_t acs)
{
    struct ieee80211_ath_channel *channel_11na, *channel_11ng;

    u_int32_t ieee_chan_11na, ieee_chan_11ng;
    uint32_t ch_load_11na;
    uint32_t ch_load_11ng;

    eacs_trace(EACS_DBG_LVL1,("entry"));
    channel_11na = ieee80211_acs_find_best_11na_centerchan(acs);
    channel_11ng = ieee80211_acs_find_best_11ng_centerchan(acs);

    ieee_chan_11ng = ieee80211_chan2ieee(acs->acs_ic, channel_11ng);
    ieee_chan_11na = ieee80211_chan2ieee(acs->acs_ic, channel_11na);
    eacs_trace(EACS_DBG_LVL0, ( "11na chan: %d rssi: %d, 11ng chan: %d rssi: %d",
                ieee80211_chan2ieee(acs->acs_ic, channel_11na), acs->acs_minrssi_11na,
                ieee80211_chan2ieee(acs->acs_ic, channel_11ng), acs->acs_minrssisum_11ng));

    ch_load_11na = acs->acs_chan_load[ieee_chan_11na];
    ch_load_11ng = acs->acs_chan_load[ieee_chan_11ng];

    /* Do channel load comparison only if radio supports both 11ng and 11na */
    if ((ieee_chan_11ng != 0) && (ieee_chan_11na != 0)) {
    /* Check which of them have the minimum channel load. If both have the same,
     * choose the 5GHz channel
     */
        if (ch_load_11ng <= ch_load_11na) {
            return channel_11ng;
        } else {
            if ((ch_load_11na - ch_load_11ng) <= acs->acs_chloadvar) {
                /* prefer 2G channel over 5G when channel loads are similar */
                return channel_11ng;
            } else {
                return channel_11na;
            }
        }
    } else if (ieee_chan_11na != 0) {
            return channel_11na;
    } else if (ieee_chan_11ng != 0) {
            return channel_11ng;
    } else {
            return IEEE80211_CHAN_ANYC;
    }
}

static INLINE struct ieee80211_ath_channel *
ieee80211_acs_find_best_centerchan(ieee80211_acs_t acs)
{
    struct ieee80211_ath_channel *channel;

    eacs_trace(EACS_DBG_LVL1,("entry"));
    switch (acs->acs_vap->iv_des_mode)
    {
        case IEEE80211_MODE_11A:
        case IEEE80211_MODE_TURBO_A:
        case IEEE80211_MODE_11NA_HT20:
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NA_HT40MINUS:
        case IEEE80211_MODE_11NA_HT40:
        case IEEE80211_MODE_11AC_VHT20:
        case IEEE80211_MODE_11AC_VHT40PLUS:
        case IEEE80211_MODE_11AC_VHT40MINUS:
        case IEEE80211_MODE_11AC_VHT40:
        case IEEE80211_MODE_11AC_VHT80:
        case IEEE80211_MODE_11AC_VHT160:
        case IEEE80211_MODE_11AC_VHT80_80:
        case IEEE80211_MODE_11AXA_HE20:
        case IEEE80211_MODE_11AXA_HE40PLUS:
        case IEEE80211_MODE_11AXA_HE40MINUS:
        case IEEE80211_MODE_11AXA_HE40:
        case IEEE80211_MODE_11AXA_HE80:
        case IEEE80211_MODE_11AXA_HE160:
        case IEEE80211_MODE_11AXA_HE80_80:

            channel = ieee80211_acs_find_best_11na_centerchan(acs);
            break;

        case IEEE80211_MODE_11B:
        case IEEE80211_MODE_11G:
        case IEEE80211_MODE_TURBO_G:
        case IEEE80211_MODE_11NG_HT20:
        case IEEE80211_MODE_11NG_HT40PLUS:
        case IEEE80211_MODE_11NG_HT40MINUS:
        case IEEE80211_MODE_11NG_HT40:
        case IEEE80211_MODE_11AXG_HE20:
        case IEEE80211_MODE_11AXG_HE40PLUS:
        case IEEE80211_MODE_11AXG_HE40MINUS:
        case IEEE80211_MODE_11AXG_HE40:

            channel = ieee80211_acs_find_best_11ng_centerchan(acs);
            break;

        default:
            if (acs->acs_scan_2ghz_only) {
                channel = ieee80211_acs_find_best_11ng_centerchan(acs);
            }
            else if (acs->acs_scan_5ghz_only) {
                channel = ieee80211_acs_find_best_11na_centerchan(acs);
            }
            else {
                channel = ieee80211_acs_find_best_auto_centerchan(acs);
            }
            break;
    }
#if ATH_ACS_SUPPORT_SPECTRAL && WLAN_SPECTRAL_ENABLE
    acs->acs_ic->ic_stop_spectral_scan(acs->acs_ic);
#endif

    return channel;
}

/*
 * Find all channels with active AP's.
 */
static QDF_STATUS
ieee80211_mark_40intol(void *arg, wlan_scan_entry_t se)
{
    struct ieee80211_ath_channel *scan_chan = wlan_util_scan_entry_channel(se);

    struct acs_obsscheck *ochk = (struct acs_obsscheck *) arg ;

    int sechan;
    ieee80211_acs_t acs = NULL;
    struct ieee80211com *ic = NULL;
    struct ieee80211_ath_channel *channel = NULL;
    u_int8_t obss_rssi = 0;
    uint8_t ssid [IEEE80211_MAX_SSID + 1] = {0, };

    if ((ochk == NULL) || (ochk->acs == NULL) || (ochk->acs->acs_ic == NULL)) {
        eacs_trace(EACS_DBG_OBSS,("vap/acs/ic is null"));
        return EINVAL;
    }

    acs = ochk->acs;
    ic = acs->acs_ic;
    channel = ochk->channel;

    sechan = ieee80211_chan2ieee(acs->acs_ic, scan_chan);
    qdf_mem_copy(ssid, util_scan_entry_ssid(se)->ssid, util_scan_entry_ssid(se)->length);
    ssid[util_scan_entry_ssid(se)->length] = '\0';

    eacs_trace(EACS_DBG_OBSS, ("Scan entry ssid %s rssi %d channel %d ", ssid,util_scan_entry_rssi(se), sechan));


    /* If we see any beacon with some RSSI mark that channel as intolaraent */
    obss_rssi = util_scan_entry_rssi(se);

    if(obss_rssi <= ic->obss_rssi_threshold ) {
        eacs_trace(EACS_DBG_OBSS, ("rejecting scan entry for %d rssi < less than threshold value: %d > ", obss_rssi, ic->obss_rssi_threshold));
        return EOK;
    }

    if (ochk->onlyextcheck){
        if ( ochk->extchan == sechan){
            eacs_trace(EACS_DBG_OBSS,("Found Scan entry on Ext chan %d   marking overlap detected for Chan %d  ",
                        ochk->extchan, ieee80211_chan2ieee(acs->acs_ic, channel)));

            /* 11AX TODO: Recheck future 802.11ax drafts (>D1.0) on coex rules
             */
            if (IEEE80211_IS_CHAN_11AX(channel)) {
                channel->ic_flags |= IEEE80211_CHAN_HE40INTOL;
            } else {
                channel->ic_flags |= IEEE80211_CHAN_HT40INTOL;
            }
        }
    }else{
        /* In case of primary channel we should not consider for OBSS */
        if (sechan == ieee80211_chan2ieee(acs->acs_ic, channel)) {
            eacs_trace(EACS_DBG_OBSS,("Found Scan entry on Primary chan %d reject scan entry ",
                        ieee80211_chan2ieee(acs->acs_ic, channel)));

            return EOK;
        }

        if ((sechan >= ochk->olminlimit) && (sechan <= ochk->olmaxlimit)){
            eacs_trace(EACS_DBG_OBSS,("Scan entry Chan %d within overlap range ( %d - %d ) -Marking chan %d ",
                        sechan, ochk->olminlimit, ochk->olmaxlimit,
                        ieee80211_chan2ieee(acs->acs_ic, channel)));

            /* 11AX TODO: Recheck future 802.11ax drafts (>D1.0) on coex rules
             */
            if (IEEE80211_IS_CHAN_11AX(channel)) {
                channel->ic_flags |= IEEE80211_CHAN_HE40INTOL;
            } else {
                channel->ic_flags |= IEEE80211_CHAN_HT40INTOL;
            }
        }
    }
    return EOK;
}

static void
ieee80211_find_40intol_overlap(ieee80211_acs_t acs,
        struct ieee80211_ath_channel *channel)
{
#define CEILING 12
#define FLOOR    1
#define HT40_NUM_CHANS 5
    u_int8_t ieee_chan = ieee80211_chan2ieee(acs->acs_ic, channel);


    int mean_chan,extchan;

    int64_t mode_mask = (IEEE80211_CHAN_11NG_HT40PLUS | IEEE80211_CHAN_11NG_HT40MINUS |
                        IEEE80211_CHAN_11AXG_HE40PLUS | IEEE80211_CHAN_11AXG_HE40MINUS);
    struct acs_obsscheck obsschk;

    eacs_trace(EACS_DBG_OBSS,("entry"));

    if (!channel || (ieee_chan == (u_int8_t)IEEE80211_CHAN_ANY))
        return;

    switch (channel->ic_flags & mode_mask)
    {
        case IEEE80211_CHAN_11NG_HT40PLUS:
        case IEEE80211_CHAN_11AXG_HE40PLUS:
            mean_chan = ieee_chan + 2;
            extchan = mean_chan + 2;
            eacs_trace(EACS_DBG_OBSS,("IEEE80211_CHAN_11NG_HT40PLUS  mean %d ext %d ",mean_chan,extchan));
            break;
        case IEEE80211_CHAN_11NG_HT40MINUS:
        case IEEE80211_CHAN_11AXG_HE40MINUS:
            mean_chan = ieee_chan - 2;
            extchan = mean_chan - 2;
            eacs_trace(EACS_DBG_OBSS,(" IEEE80211_CHAN_11NG_HT40MINUS  mean %d ext %d ",mean_chan,extchan));
            break;
        default: /* neither HT40+ nor HT40-, finish this call */
            eacs_trace(EACS_DBG_OBSS,("Error "));
            return;
    }


    /* We should mark the intended channel as 40 MHz intolerant
       if the intended frequency overlaps the iterated channel partially */

    /* According to 802.11n 2009, affected channel = [mean_chan-5, mean_chan+5] */
    obsschk.acs = acs;
    obsschk.channel = channel;
    obsschk.onlyextcheck = acs->acs_limitedbsschk;
    obsschk.extchan = extchan;
    obsschk.olminlimit = MAX(mean_chan-HT40_NUM_CHANS, FLOOR);
    obsschk.olmaxlimit  = MIN(CEILING,mean_chan+HT40_NUM_CHANS);

    ucfg_scan_db_iterate(wlan_vap_get_pdev(acs->acs_vap),
            ieee80211_mark_40intol, (void *)&obsschk);

    if ((extchan > 0) &&(( acs->acs_noisefloor[extchan] != NF_INVALID) && (acs->acs_noisefloor[extchan] > ACS_11NG_NOISE_FLOOR_REJ) )){
        eacs_trace(EACS_DBG_OBSS, (" Reject extension channel: %d  noise is: %d ",extchan,acs->acs_noisefloor[extchan]));

        /* 11AX TODO: Recheck future 802.11ax drafts (>D1.0) on coex rules */
        if (IEEE80211_IS_CHAN_11AX(channel)) {
            channel->ic_flags |= IEEE80211_CHAN_HE40INTOL;
        } else {
            channel->ic_flags |= IEEE80211_CHAN_HT40INTOL;
        }
    }

    eacs_trace(EACS_DBG_OBSS, (" selected chan %d  overlap %llx ",ieee_chan,
                (channel->ic_flags & (IEEE80211_CHAN_HT40INTOL |
                                      IEEE80211_CHAN_HE40INTOL))));

#undef CEILING
#undef FLOOR
#undef HT40_NUM_CHANS
}

static QDF_STATUS
ieee80211_get_chan_neighbor_list(void *arg, wlan_scan_entry_t se)
{
    ieee80211_chan_neighbor_list *chan_neighbor_list = (ieee80211_chan_neighbor_list *) arg ;
    struct ieee80211_ath_channel *channel_se = NULL;
    u_int8_t channel_ieee_se;
    int nbss;
    u_int8_t ssid_len;
    u_int8_t *ssid = NULL;
    u_int8_t *bssid = NULL;
    u_int8_t  rssi = 0;
    u_int32_t phymode = 0;
    ieee80211_acs_t acs = NULL;
    struct ieee80211com *ic = NULL;

    /* sanity check */
    if ((chan_neighbor_list == NULL) || (chan_neighbor_list->acs == NULL) || (chan_neighbor_list->acs->acs_ic == NULL) ) {
        eacs_trace(EACS_DBG_OBSS,("vap/acs/ic/ssid is null"));
        return EINVAL;
    }

    ssid = (char *)util_scan_entry_ssid(se)->ssid;
    ssid_len = util_scan_entry_ssid(se)->length;
    bssid = (char *)util_scan_entry_bssid(se);
    rssi = util_scan_entry_rssi(se);
    phymode = util_scan_entry_phymode(se);

    /*if both ssid and bssid is null it is invalid entry*/
    if ((ssid == NULL) && (bssid == NULL)) {
        eacs_trace(EACS_DBG_OBSS,("ssid/bssid is null"));
        return EINVAL;
    }

    acs = chan_neighbor_list->acs;
    ic = acs->acs_ic;
    nbss = chan_neighbor_list->nbss;
    channel_se = wlan_util_scan_entry_channel(se);
    channel_ieee_se = ieee80211_chan2ieee(acs->acs_ic, channel_se);

    /* Only copy the SSID if it in current channel scan entry */
    if ((channel_ieee_se == chan_neighbor_list->chan)) {

       /* Hidden AP the SSID might be null */
        if (ssid != NULL) {
            memcpy(chan_neighbor_list->neighbor_list[nbss].ssid, ssid, ssid_len);
            if (ssid_len < IEEE80211_NWID_LEN) {
                chan_neighbor_list->neighbor_list[nbss].ssid[ssid_len] = '\0';
            }
            else {
                chan_neighbor_list->neighbor_list[nbss].ssid[IEEE80211_NWID_LEN] = '\0';
            }
        }
        memcpy(chan_neighbor_list->neighbor_list[nbss].bssid, bssid, IEEE80211_ADDR_LEN);
        chan_neighbor_list->neighbor_list[nbss].rssi = rssi;
        chan_neighbor_list->neighbor_list[nbss].phymode = phymode;
        chan_neighbor_list->nbss++;
    }

    return EOK;

}

static int ieee80211_acs_derive_sec_chans_with_mode(u_int8_t phymode,
        u_int8_t vht_center_freq, u_int8_t channel_ieee_se,
        u_int8_t *sec_chan_20, u_int8_t *sec_chan_40_1, u_int8_t *sec_chan_40_2)
{
    int8_t pri_center_ch_diff;
    int8_t sec_level;


    switch (phymode)
    {
        /*secondary channel for VHT40MINUS and NAHT40MINUS is same */
        case IEEE80211_MODE_11NA_HT40MINUS:
        case IEEE80211_MODE_11AC_VHT40MINUS:
        case IEEE80211_MODE_11AXA_HE40MINUS:
            *sec_chan_20 = channel_ieee_se - 4;
            break;
            /*secondary channel for VHT40PLUS and NAHT40PLUS is same */
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11AC_VHT40PLUS:
        case IEEE80211_MODE_11AXA_HE40PLUS:
            *sec_chan_20 = channel_ieee_se + 4;
            break;
        case IEEE80211_MODE_11AC_VHT80:
        case IEEE80211_MODE_11AXA_HE80:
             /* For VHT mode, The center frequency is given in VHT OP IE
             * For example: 42 is center freq and 36 is primary channel
             * then secondary 20 channel would be 40
             */
            pri_center_ch_diff = channel_ieee_se - vht_center_freq;
            /* Secondary 20 channel would be less(2 or 6) or more (2 or 6)
             * than center frequency based on primary channel
             */
            if(pri_center_ch_diff > 0) {
                sec_level = LOWER_FREQ_SLOT;
            }
            else {
                sec_level = UPPER_FREQ_SLOT;
            }
            if((sec_level*pri_center_ch_diff) < -2)
                *sec_chan_20 = vht_center_freq - (sec_level* SEC20_OFF_2);
            else
                *sec_chan_20 = vht_center_freq - (sec_level* SEC20_OFF_6);
            break;
        case IEEE80211_MODE_11AC_VHT80_80:
        case IEEE80211_MODE_11AXA_HE80_80:

            /* For VHT mode, The center frequency is given in VHT OP IE
             * For example: 42 is center freq and 36 is primary channel
             * then secondary 20 channel would be 40 and secondary 40 channel
             * would be 44
             */
            pri_center_ch_diff = channel_ieee_se - vht_center_freq;

            /* Secondary 20 channel would be less(2 or 6) or more (2 or 6)
             * than center frequency based on primary channel
             */
            if(pri_center_ch_diff > 0) {
                sec_level = LOWER_FREQ_SLOT;
                *sec_chan_40_1 = vht_center_freq + SEC_40_LOWER;
            }
            else {
                sec_level = UPPER_FREQ_SLOT;
                *sec_chan_40_1 = vht_center_freq - SEC_40_UPPER;
            }
            *sec_chan_40_2 = *sec_chan_40_1 + 4;
            if((sec_level*pri_center_ch_diff) < -2)
                *sec_chan_20 = vht_center_freq - (sec_level* SEC20_OFF_2);
            else
                *sec_chan_20 = vht_center_freq - (sec_level* SEC20_OFF_6);

            break;

        case IEEE80211_MODE_11AC_VHT160:
        case IEEE80211_MODE_11AXA_HE160:

            /* For VHT mode, The center frequency is given in VHT OP IE
             * For example: 42 is center freq and 36 is primary channel
             * then secondary 20 channel would be 40 and secondary 40 channel
             * would be 44
             */
            pri_center_ch_diff = channel_ieee_se - vht_center_freq;

            /* Secondary 20 channel would be less(2 or 6) or more (2 or 6)
             * than center frequency based on primary channel
             */
            if(pri_center_ch_diff > 0) {
                sec_level = LOWER_FREQ_SLOT;
                *sec_chan_40_1 = vht_center_freq + SEC_40_LOWER;
            }
            else {
                sec_level = UPPER_FREQ_SLOT;
                *sec_chan_40_1 = vht_center_freq - SEC_40_UPPER;
            }
            *sec_chan_40_2 = *sec_chan_40_1 + 4;
            if((sec_level*pri_center_ch_diff) < -2)
                *sec_chan_20 = vht_center_freq - (sec_level* SEC20_OFF_2);
            else
                *sec_chan_20 = vht_center_freq - (sec_level* SEC20_OFF_6);

            break;
    }

    return EOK;
}


/*
 * Derive secondary channels based on the phy mode of the scan entry
 */
static int ieee80211_acs_derive_sec_chan(wlan_scan_entry_t se,
        u_int8_t channel_ieee_se, u_int8_t *sec_chan_20,
        u_int8_t *sec_chan_40_1, u_int8_t *sec_chan_40_2, struct ieee80211com *ic)
{
    /*
     * vht_center_freq refers to the center frequency of the primary 80MHz
     * in the case of VHT80, VHT80+80 and VHT160MHz
     */
    u_int8_t vht_center_freq = 0;
    u_int8_t phymode_se;
    struct ieee80211_ie_vhtop *vhtop = NULL;
    struct ieee80211_ie_vhtcap *vhtcap = NULL;
    phymode_se = util_scan_entry_phymode(se);
    vhtop = (struct ieee80211_ie_vhtop *)util_scan_entry_vhtop(se);

    switch (phymode_se)
    {
        case IEEE80211_MODE_11AC_VHT80:
        case IEEE80211_MODE_11AXA_HE80:
            vht_center_freq = vhtop->vht_op_ch_freq_seg1;
            break;
        case IEEE80211_MODE_11AC_VHT80_80:
        case IEEE80211_MODE_11AC_VHT160:
        case IEEE80211_MODE_11AXA_HE80_80:
        case IEEE80211_MODE_11AXA_HE160:
            vhtcap = (struct ieee80211_ie_vhtcap *)util_scan_entry_vhtcap(se);

            if(!vhtop->vht_op_ch_freq_seg2) {
                if (peer_ext_nss_capable(vhtcap) && ic->ic_ext_nss_capable) {
                    /* seg1 contains primary 80 center frequency in the case of
                     * ext_nss enabled revised signalling. This applies to both 160 MHz
                     * and 80+80 MHz
                     */
                    vht_center_freq = vhtop->vht_op_ch_freq_seg1;
                } else {
                    /* seg1 contains the 160 center frequency in the case of
                     * legacy signalling before revised signalling was introduced.
                     * This applies only to 160 MHz and not to 80+80 MHz.
                     */
                    if ((vhtop->vht_op_ch_freq_seg1 - channel_ieee_se) < 0)
                        vht_center_freq = vhtop->vht_op_ch_freq_seg1 + 8;
                    else
                        vht_center_freq = vhtop->vht_op_ch_freq_seg1 - 8;
                }
	    } else {
                /* For 160 MHz: seg1 contains primary 80 center frequency in
                 * the case of revised signalling with ext_nss disabled, or in the case
                 * of revised signaling with ext_nss enabled in which CCFS1 is used.
                 * For 80+80 MHz: seg1 contains primary 80 center frequency in
                 * the case of legacy signaling before revised signalling was
                 * introduced, or revised signalling with ext_nss disabled, or revised
                 * signaling with ext_nss enabled in which CCFS1 is used.
                 */
                vht_center_freq = vhtop->vht_op_ch_freq_seg1;
            }

            break;
    }
    ieee80211_acs_derive_sec_chans_with_mode(phymode_se,vht_center_freq,
                                     channel_ieee_se,sec_chan_20,sec_chan_40_1, sec_chan_40_2);
    eacs_trace(EACS_DBG_LVL0, ("phy mode %d secondary 20 channel is %d and secondary 40 channel is %d secondary 40_2 is %d",(int)phymode_se, *sec_chan_20, *sec_chan_40_1, *sec_chan_40_2));
    return EOK;
}

/*
 * Trace all entries to record the max rssi found for every channel.
 */
static QDF_STATUS
ieee80211_acs_get_channel_maxrssi_n_secondary_ch(void *arg, wlan_scan_entry_t se)
{
    ieee80211_acs_t acs = (ieee80211_acs_t) arg;
    struct ieee80211_ath_channel *channel_se = wlan_util_scan_entry_channel(se);
    u_int8_t rssi_se = util_scan_entry_rssi(se);
    u_int8_t channel_ieee_se = ieee80211_chan2ieee(acs->acs_ic, channel_se);
    u_int8_t sec_chan_20 = 0;
    u_int8_t sec_chan_40_1 = 0;
    u_int8_t sec_chan_40_2 = 0;
    u_int8_t len;
    uint8_t ssid [IEEE80211_MAX_SSID + 1] = {0, };
    struct ieee80211_ie_srp_extie *srp_ie;
    struct ieee80211_ie_hecap *hecap_ie;
    uint32_t srps = 0;
    uint32_t srp_nobsspd = 1;
    uint8_t *hecap_phy_ie;

    len = util_scan_entry_ssid(se)->length;
    qdf_mem_copy(ssid, util_scan_entry_ssid(se)->ssid, len);
    ssid[len] = '\0';

    eacs_trace(EACS_DBG_RSSI, ( "ssid %s chan %d", ssid,(int)channel_ieee_se));

    if (rssi_se > acs->acs_chan_maxrssi[channel_ieee_se]) {
        acs->acs_chan_maxrssi[channel_ieee_se] = rssi_se;
    }
    acs->acs_chan_rssi[channel_ieee_se] +=rssi_se;
    /* This support is for stats */
    if ((acs->acs_chan_minrssi[channel_ieee_se] == 0) ||
            (rssi_se < acs->acs_chan_maxrssi[channel_ieee_se])) {
        acs->acs_chan_minrssi[channel_ieee_se] = rssi_se;
    }
    acs->acs_chan_nbss[channel_ieee_se] += 1;

    eacs_trace(EACS_DBG_RSSI, ("nbss %d, rssi %d, Total rsssi %d, noise: %d ",acs->acs_chan_nbss[channel_ieee_se], rssi_se, acs->acs_chan_rssi[channel_ieee_se], acs->acs_noisefloor[channel_ieee_se]));

    if (!ieee80211_acs_derive_sec_chan(se, channel_ieee_se, &sec_chan_20, &sec_chan_40_1, &sec_chan_40_2, acs->acs_ic)) {
        acs->acs_sec_chan[sec_chan_20] = true;
        /* Secondary 40 would be enabled for 80+80 Mhz channel or
         * 160 Mhz channel
         */
        if ((sec_chan_40_1 != 0) && (sec_chan_40_2 != 0)) {
            acs->acs_sec_chan[sec_chan_40_1] = true;
            acs->acs_sec_chan[sec_chan_40_2] = true;
        }
    }

    hecap_ie = (struct ieee80211_ie_hecap *)util_scan_entry_hecap(se);
    srp_ie = (struct ieee80211_ie_srp_extie *)util_scan_entry_spatial_reuse_parameter(se);

    if (hecap_ie) {
        hecap_phy_ie = &hecap_ie->hecap_phyinfo[0];
        srps = HECAP_PHY_SRPSPRESENT_GET_FROM_IE(&hecap_phy_ie);
    }

#ifdef OBSS_PD
    if (srp_ie) {
        srp_nobsspd = srp_ie->sr_control & IEEE80211_SRP_SRCTRL_OBSS_PD_DISALLOWED_MASK;
    }
#endif

    if (srps) {
        acs->acs_srp_info[channel_ieee_se].srp_allowed += 1;
    }

    if (!srp_nobsspd) {
            acs->acs_srp_info[channel_ieee_se].srp_obsspd_allowed += 1;
    }

    if (!srp_nobsspd || srps) {
           acs->acs_srp_supported[channel_ieee_se]++;
           eacs_trace(EACS_DBG_RSSI, ("srps %d srp_nobsspd %d total_srp bss %d", srps, srp_nobsspd, acs->acs_srp_supported[channel_ieee_se]));
    }

    return EOK;
}

static INLINE void ieee80211_acs_check_band_to_scan(ieee80211_acs_t acs, u_int16_t nchans_2ghz)
{
    if ((nchans_2ghz) && (acs->acs_nchans == nchans_2ghz)) {
        eacs_trace(EACS_DBG_LVL0,( "No 5 GHz channels available, skip 5 GHz scan" ));
        acs->acs_scan_5ghz_only = 0;
        acs->acs_scan_2ghz_only = 1;
    }
    else if (!(nchans_2ghz) && (acs->acs_nchans)) {
        eacs_trace(EACS_DBG_LVL0,( "No 2 GHz channels available, skip 2 GHz scan" ));
        acs->acs_scan_2ghz_only = 0;
        acs->acs_scan_5ghz_only = 1;
    }
}


/**
 * @brief whether a channel is blocked/unblocked
 *
 * @param acs
 * @param channel
 * @return true if channel is unblocked otheriwse false
 */
static inline int acs_is_channel_blocked(ieee80211_acs_t acs,u_int8_t chnum)
{
    acs_bchan_list_t * bchan = NULL ;
    u_int16_t i = 0;

    if(acs == NULL)
        return EPERM;

    bchan = &(acs->acs_bchan_list);
    for(i = 0;i < bchan->uchan_cnt;i++)
    {
        if(bchan->uchan[i] == chnum) {
            /* Non zero values means ignore this channel */
            return true;
        }
    }
    return false;
}

static INLINE void ieee80211_acs_get_phymode_channels(ieee80211_acs_t acs, enum ieee80211_phymode mode)
{
    struct ieee80211_ath_channel *channel;
    int i, extchan, skipchanextinvalid;
    struct ieee80211_ath_channel_list chan_info;

    eacs_trace(EACS_DBG_LVL0,( " get channels with phy mode=%d",  mode));
    ieee80211_enumerate_channels(channel, acs->acs_ic, i) {
        eacs_trace(EACS_DBG_LVL1,(" ic_freq: %d ic_ieee: %d ic_flags: %llx ", channel->ic_freq, channel->ic_ieee, channel->ic_flags));
        /*reset overlap bits */

        if(channel->ic_flags & (IEEE80211_CHAN_HT40INTOLMARK | IEEE80211_CHAN_HT40INTOL | IEEE80211_CHAN_HE40INTOL | IEEE80211_CHAN_HE40INTOLMARK)) {
            channel->ic_flags &= ~(IEEE80211_CHAN_HT40INTOLMARK | IEEE80211_CHAN_HT40INTOL | IEEE80211_CHAN_HE40INTOL | IEEE80211_CHAN_HE40INTOLMARK);
        }

        if (mode == ieee80211_chan2mode(channel)) {
            u_int8_t ieee_chan_num = ieee80211_chan2ieee(acs->acs_ic, channel);
#if ATH_CHANNEL_BLOCKING
            u_int8_t ieee_ext_chan_num;
#endif
            /*
            * When EMIWAR is EMIWAR_80P80_FC1GTFC2 for some channel 80_80MHZ channel combination is not allowed
            * this code will allowed the 80MHz combination to add in to acs_channel list
            * so that these can be used as secondary channel
            */
            if((acs->acs_ic->ic_emiwar_80p80 == EMIWAR_80P80_FC1GTFC2) &&
                ((mode == IEEE80211_MODE_11AC_VHT80) || (mode == IEEE80211_MODE_11AXA_HE80)) &&
                ((acs->acs_vap->iv_des_mode == IEEE80211_MODE_11AC_VHT80_80) || (acs->acs_vap->iv_des_mode == IEEE80211_MODE_11AXA_HE80_80))&&
                (acs->acs_chan_maps[ieee_chan_num] != 0xff)) {
                eacs_trace(EACS_DBG_LVL1,( " Duplicate channel freq %d",  channel->ic_freq));
                continue;
            }
             /* VHT80_80, channel list has duplicate entries, since ACS picks both primary and secondary 80 mhz channels
                single entry in ACS would be sufficient */
            if(acs->acs_nchans &&
                (ieee80211_chan2ieee(acs->acs_ic, acs->acs_chans[acs->acs_nchans-1]) == ieee_chan_num))
            {
                eacs_trace(EACS_DBG_LVL1,( " Duplicate channel freq %d",  channel->ic_freq));
                continue;
            }
#if ATH_SUPPORT_IBSS_ACS
            /*
             * ACS : filter out DFS channels for IBSS mode
             */
            if((wlan_vap_get_opmode(acs->acs_vap) == IEEE80211_M_IBSS) && IEEE80211_IS_CHAN_DISALLOW_ADHOC(channel)) {
                eacs_trace(EACS_DBG_LVL1,( "skip DFS-check channel %d",  channel->ic_freq));
                continue;
            }
#endif
            if ((wlan_vap_get_opmode(acs->acs_vap) == IEEE80211_M_HOSTAP) && IEEE80211_IS_CHAN_DISALLOW_HOSTAP(channel)) {
                eacs_trace(EACS_DBG_LVL0,( "skip Station only channel %d", channel->ic_freq));
                continue;
            }

#if WLAN_SUPPORT_PRIMARY_ALLOWED_CHAN
            if(acs->acs_ic->ic_primary_allowed_enable &&
                            !(ieee80211_check_allowed_prim_chanlist(
                                            acs->acs_ic, ieee_chan_num))) {
                    eacs_trace(EACS_DBG_BLOCK, ("Channel %d is not a primary allowed channel \n",ieee_chan_num));
                    continue;
            }
#endif
            /*channel is blocked as per user setting */
            if(acs_is_channel_blocked(acs,ieee_chan_num)) {
                eacs_trace(EACS_DBG_BLOCK, ("Channel %d is blocked, in mode %d", ieee_chan_num, mode));
                continue;
            }

            /* channel is present in nol list */
            if (IEEE80211_IS_CHAN_RADAR(channel)){
                eacs_trace(EACS_DBG_BLOCK, ("Channel %d is in nol, in mode %d\n", ieee_chan_num, mode));
                continue;
            }

            ieee80211_get_extchaninfo(acs->acs_ic, channel, &chan_info);

            skipchanextinvalid = 0;

            for (extchan = 0; extchan < chan_info.cl_nchans ; extchan++) {
                if (chan_info.cl_channels[extchan] == NULL) {
                    if(ieee80211_is_extchan_144(acs->acs_ic, channel, extchan)) {
                         continue;
                    }
                    eacs_trace(EACS_DBG_LVL0, ("Rejecting Channel for ext channel found null "
                                "channel number %d EXT channel %d found %pK", ieee_chan_num, extchan,
                                (void *)chan_info.cl_channels[extchan]));
                    skipchanextinvalid = 1;
                    break;
                } else if (IEEE80211_IS_CHAN_RADAR(chan_info.cl_channels[extchan])) {
                    eacs_trace(EACS_DBG_LVL0, ("Rejecting Channel for ext channel as its in nol "
                                "channel number %d EXT channel %d found %pK\n", ieee_chan_num, extchan,
                                (void*)chan_info.cl_channels[extchan]));
                    skipchanextinvalid = 1;
                    break;
                }
            }

#if ATH_CHANNEL_BLOCKING
            if (acs->acs_block_mode & ACS_BLOCK_EXTENSION) {
                /* extension channel (in NAHT40/ACVHT40/ACVHT80 mode) is blocked as per user setting */
                for (extchan = 0; (skipchanextinvalid == 0) && (extchan < chan_info.cl_nchans); extchan++) {
                    ieee_ext_chan_num = ieee80211_chan2ieee(acs->acs_ic, chan_info.cl_channels[extchan]);
                    if (acs_is_channel_blocked(acs, ieee_ext_chan_num)) {
                        eacs_trace(EACS_DBG_BLOCK, ("Channel %d can't be used, as ext channel %d "
                                    "is blocked, in mode %d", ieee_chan_num, ieee_ext_chan_num, mode));
                        skipchanextinvalid = 1; /* break */
                    }
                }

                /* in 2.4GHz band checking channels overlapping with primary,
                 * or if required with secondary too (NGHT40 mode) */
                if (!skipchanextinvalid && IEEE80211_IS_CHAN_2GHZ(channel)) {
                    struct ieee80211_ath_channel *ext_chan;
                    int start = channel->ic_freq - 15, end = channel->ic_freq + 15, f;
                    if (IEEE80211_IS_CHAN_11NG_HT40PLUS(channel))
                        end = channel->ic_freq + 35;
                    if (IEEE80211_IS_CHAN_11NG_HT40MINUS(channel))
                        start = channel->ic_freq - 35;
                    for (f = start; f <= end; f += 5) {
                        ext_chan = acs->acs_ic->ic_find_channel(acs->acs_ic, f, 0, IEEE80211_CHAN_B);
                        if (ext_chan) {
                            ieee_ext_chan_num = ieee80211_chan2ieee(acs->acs_ic, ext_chan);
                            if (acs_is_channel_blocked(acs, ieee_ext_chan_num)) {
                                eacs_trace(EACS_DBG_BLOCK, ("Channel %d can't be used, as ext "
                                            "or overlapping channel %d is blocked, in mode %d",
                                            ieee_chan_num, ieee_ext_chan_num, mode));
                                skipchanextinvalid = 1;
                                break;
                            }
                        }
                    }
                }
            }
#endif

            if ( skipchanextinvalid ) {
                eacs_trace(EACS_DBG_LVL0, ("skip channel %d -> %d due to ext channel "
                            "invalid or blocked", channel->ic_ieee, channel->ic_freq));
                continue;
            }

            eacs_trace(EACS_DBG_LVL0,("adding channel %d %016llx %d to list",
                        channel->ic_freq, channel->ic_flags, channel->ic_ieee));

            if (ieee_chan_num != (u_int8_t) IEEE80211_CHAN_ANY) {
                acs->acs_chan_maps[ieee_chan_num] = acs->acs_nchans;
            }
            acs->acs_chans[acs->acs_nchans++] = channel;
            if (acs->acs_nchans >= IEEE80211_ACS_CHAN_MAX) {
                eacs_trace(EACS_DBG_LVL0,("acs number of channels are more than"
                            "MAXIMUM channels "));
                break;
            }
        }
    }
}

/*
 * construct the available channel list
 */
static void ieee80211_acs_construct_chan_list(ieee80211_acs_t acs, int mode)
{
    u_int16_t nchans_2ghz = 0;

    eacs_trace(EACS_DBG_LVL0,("phymode %d",mode));
    /* reset channel mapping array */
    OS_MEMSET(&acs->acs_chan_maps, 0xff, sizeof(acs->acs_chan_maps));
    acs->acs_nchans = 0;

    switch (mode) {
        case IEEE80211_MODE_AUTO:
            /*
             * Try to add channels corresponding to different 2.4GHz modes
             * in decreasing order, starting from highgest possible mode.
             * The higher mode channels, that the device doesn't support, won't be added.
             * So, this will select highest mode, the device supports.
             */

            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXG_HE40PLUS);
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXG_HE40MINUS);

            /*
             * acs->acs_nchans will be incremented by ieee80211_acs_get_phymode_channels.
             * check if the previous call could add the channels.
             * If it has already added, no need to add the lower mode channels.
             * otherwise we will try to add lower mode channels.
             */
            if (!acs->acs_nchans) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NG_HT40PLUS);
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NG_HT40MINUS);
            }

            if (!acs->acs_nchans) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NG_HT20);
            }

            if (!acs->acs_nchans) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11G);
            }

            if (!acs->acs_nchans) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11B);
            }

            nchans_2ghz = acs->acs_nchans;
            /*
             * Try to add channels corresponding to different 5GHz modes
             * in decreasing order, starting from highgest possible mode.
             * The higher mode channels, that the device doesn't support, won't be added.
             * So, this will select highest mode, the device supports.
             */

            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXA_HE160);

           /*
             * acs->acs_nchans will be incremented by ieee80211_acs_get_phymode_channels.
             * check if the previous call could add the channels.
             * If it has already added, no need to add the lower mode channels.
             * otherwise we will try to add lower mode channels.
             * We will try to add the 5GHz channels, even though we have already added 2.4GHz channels,
             * since this could be a dual-band radio.
             */

            if (!(acs->acs_nchans > nchans_2ghz)) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXA_HE80);
            }

            if (!(acs->acs_nchans > nchans_2ghz)) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXA_HE40PLUS);
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXA_HE40MINUS);
            }

            if (!(acs->acs_nchans > nchans_2ghz)) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AC_VHT160);
            }

            if (!(acs->acs_nchans > nchans_2ghz)) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AC_VHT80);
            }

            if (!(acs->acs_nchans > nchans_2ghz)) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AC_VHT40PLUS);
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AC_VHT40MINUS);
            }

            if (!(acs->acs_nchans > nchans_2ghz)) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AC_VHT20);
            }

            if (!(acs->acs_nchans > nchans_2ghz)) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NA_HT40PLUS);
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NA_HT40MINUS);
            }

            if (!(acs->acs_nchans > nchans_2ghz)) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NA_HT20);
            }

            if (!(acs->acs_nchans > nchans_2ghz)) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11A);
            }

            ieee80211_acs_check_band_to_scan(acs, nchans_2ghz);
            break;

        case IEEE80211_MODE_NONE:
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NG_HT20);
            nchans_2ghz = acs->acs_nchans;
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NA_HT20);

            /* If no HT channel available */
            if (acs->acs_nchans == 0) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11G);
                nchans_2ghz = acs->acs_nchans;
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11A);
            }

            /* If still no channel available */
            if (acs->acs_nchans == 0) {
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11B);
                nchans_2ghz = acs->acs_nchans;
            }
            ieee80211_acs_check_band_to_scan(acs, nchans_2ghz);
            break;

        case IEEE80211_MODE_11AXG_HE40:
            /* if PHY mode is not AUTO, get channel list by PHY mode directly */
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXG_HE40PLUS);
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXG_HE40MINUS);
            acs->acs_scan_2ghz_only = 1;
            eacs_trace(EACS_DBG_LVL0,( " get channels with phy mode AXZG HE40PLUS AND HE40MINUS"));
            break;

        case IEEE80211_MODE_11NG_HT40:
            /* if PHY mode is not AUTO, get channel list by PHY mode directly */
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NG_HT40PLUS);
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NG_HT40MINUS);
            acs->acs_scan_2ghz_only = 1;
            eacs_trace(EACS_DBG_LVL0,( " get channels with phy mode 11ng HT40PLUS AND HT40MINUS"));
            break;

        case IEEE80211_MODE_11NA_HT40:
            eacs_trace(EACS_DBG_LVL0,( " get channels with phy mode HT40PLUS AND HT40MINUTE"));
            /* if PHY mode is not AUTO, get channel list by PHY mode directly */
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NA_HT40PLUS);
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NA_HT40MINUS);
            break;
        case IEEE80211_MODE_11AXA_HE40:
            eacs_trace(EACS_DBG_LVL0,( " get channels with phy mode 11AXA HE40PLUS and HE40MINUS"));
            /* if PHY mode is not AUTO, get channel list by PHY mode directly */
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXA_HE40PLUS);
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXA_HE40MINUS);
            break;

        case IEEE80211_MODE_11AC_VHT40:

            eacs_trace(EACS_DBG_LVL0,( " get channels with phy mode VHT40 plus and minuse"));
            /* if PHY mode is not AUTO, get channel list by PHY mode directly */
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AC_VHT40PLUS);
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AC_VHT40MINUS);
            break;
        case IEEE80211_MODE_11AXA_HE80:
            eacs_trace(EACS_DBG_LVL0,( " get channels with phy mode 11AXA HE80"));
            /* if PHY mode is not AUTO, get channel list by PHY mode directly */
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXA_HE80);
            break;
        case IEEE80211_MODE_11AC_VHT80:

            eacs_trace(EACS_DBG_LVL0,( " get channels with phy mode VHT80 "));
            /* if PHY mode is not AUTO, get channel list by PHY mode directly */
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AC_VHT80);
            break;
        case IEEE80211_MODE_11AXA_HE80_80:
            eacs_trace(EACS_DBG_LVL0,( " get channels with phy mode 11AXA HE80_80"));
            /* if PHY mode is not AUTO, get channel list by PHY mode directly */
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXA_HE80_80);
            if(acs->acs_ic->ic_emiwar_80p80 == EMIWAR_80P80_FC1GTFC2) {
                /*
                 * When EMIWAR is EMIWAR_80P80_FC1GTFC2
                 * there will be no 80_80 channel combination for 149,153,157,161,
                 * because of which these channel will not be added to acs_chan list
                 * in which case acs will not even consider as these channel as secondary channel.
                 * So in this case add 149,153,157,161 80MHz channels to acs channel list
                 * and force acs not to select these channel as primary
                 * but can consider these channel as secondary channel.
                 */
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXA_HE80);
            }
            break;
        case IEEE80211_MODE_11AC_VHT80_80:

            eacs_trace(EACS_DBG_LVL0,( " get channels with phy mode VHT80_80 "));
            /* if PHY mode is not AUTO, get channel list by PHY mode directly */
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AC_VHT80_80);
            if(acs->acs_ic->ic_emiwar_80p80 == EMIWAR_80P80_FC1GTFC2) {
                /*
                 * When EMIWAR is EMIWAR_80P80_FC1GTFC2
                 * there will be no 80_80 channel combination for 149,153,157,161,
                 * because of which these channel will not be added to acs_chan list
                 * in which case acs will not even consider as these channel as secondary channel.
                 * So in this case add 149,153,157,161 80MHz channels to acs channel list
                 * and force acs not to select these channel as primary
                 * but can consider these channel as secondary channel.
                 */
                ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AC_VHT80);
            }
            break;
        case IEEE80211_MODE_11AXA_HE160:
            eacs_trace(EACS_DBG_LVL0,( " get channels with phy mode 11AXA HE160"));
            /* if PHY mode is not AUTO, get channel list by PHY mode directly */
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AXA_HE160);
            break;
        case IEEE80211_MODE_11AC_VHT160:

            eacs_trace(EACS_DBG_LVL0,( " get channels with phy mode VHT160"));
            /* if PHY mode is not AUTO, get channel list by PHY mode directly */
            ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11AC_VHT160);
            break;
        default:
            /* if PHY mode is not AUTO, get channel list by PHY mode directly */
            ieee80211_acs_get_phymode_channels(acs, mode);
            break;
    }
}

static OS_TIMER_FUNC(acs_bk_scan_timer)
{

    ieee80211_acs_t acs ;

    OS_GET_TIMER_ARG(acs, ieee80211_acs_t );
    eacs_trace(EACS_DBG_LVL1,("Scan timer  handler "));

    if(acs->acs_scantimer_handler)
        acs->acs_scantimer_handler(acs->acs_scantimer_arg);

    OS_SET_TIMER(&acs->acs_bk_scantimer, acs->acs_bk_scantime*1000);

}

#if ATH_SUPPORT_VOW_DCS
static OS_TIMER_FUNC(dcs_enable)
{
    struct ieee80211com *ic = NULL;
    ieee80211_acs_t acs;
    OS_GET_TIMER_ARG(acs, ieee80211_acs_t);
    ic = acs->acs_ic;
    /* Enable DCS WLAN_IM functionality */
    ic->ic_enable_dcsim(ic);
    /* Deleting DCS enable timer and resetting timer flag to 0 once DCS WLAN_IM is enabled */
    OS_CANCEL_TIMER(&acs->dcs_enable_timer);
    acs->is_dcs_enable_timer_set = 0;
}
#endif

int ieee80211_acs_init(ieee80211_acs_t *acs, wlan_dev_t devhandle, osdev_t osdev)
{
    OS_MEMZERO(*acs, sizeof(struct ieee80211_acs));

    /* Save handles to required objects.*/
    (*acs)->acs_ic     = devhandle;
    (*acs)->acs_osdev  = osdev;

    (*acs)->acs_noisefloor_threshold = ACS_NOISE_FLOOR_THRESH_MIN;

    spin_lock_init(&((*acs)->acs_lock));
    qdf_spinlock_create(&((*acs)->acs_ev_lock));
    (*acs)->acs_rssivar   = ACS_ALLOWED_RSSIVARAINCE ;
    (*acs)->acs_chloadvar = ACS_ALLOWED_CHANLOADVARAINCE;
    (*acs)->acs_srvar = ACS_ALLOWED_SRVARAINCE;
    (*acs)->acs_bk_scantime  = ATH_ACS_DEFAULT_SCANTIME;
    (*acs)->acs_bkscantimer_en = 0;
    (*acs)->acs_ic->ic_acs_ctrlflags = 0;
    (*acs)->acs_limitedbsschk = 0;
    /* By default report to user space is disabled  */
    (*acs)->acs_scan_req_param.acs_scan_report_active = false;
    (*acs)->acs_scan_req_param.acs_scan_report_pending = false;
    eacs_dbg_mask = 0;
#ifdef ATH_SUPPORT_VOW_DCS
    (*acs)->dcs_enable_time = DCS_ENABLE_TIME;
    (*acs)->is_dcs_enable_timer_set = 0;
#endif
    (*acs)->acs_ch_hopping.param.nohop_dur = CHANNEL_HOPPING_NOHOP_TIMER ; /* one minute*/
    /* 15 minutes */
    (*acs)->acs_ch_hopping.param.long_dur =  CHANNEL_HOPPING_LONG_DURATION_TIMER;
    (*acs)->acs_ch_hopping.param.cnt_dur = CHANNEL_HOPPING_CNTWIN_TIMER ; /* 5 sec */
    /* switch channel if out of total time 75 % idetects noise */
    (*acs)->acs_ch_hopping.param.cnt_thresh = 75;
    /* INIT value will increment at each channel selection*/
    (*acs)->acs_ch_hopping.ch_max_hop_cnt = 0;
    /*video bridge Detection threshold */
    (*acs)->acs_ch_hopping.param.noise_thresh = CHANNEL_HOPPING_VIDEO_BRIDGE_THRESHOLD;
    (*acs)->acs_ch_hopping.ch_nohop_timer_active = false ; /*HOPPING TIMER ACTIVE  */
    /*Default channel change will happen through usual ways not by channel hopping */
    (*acs)->acs_ch_hopping.ch_hop_triggered = false;
    /* Init these values will help us in reporting user
       the values being used in case he wants to enquire
       even before setting these values */
    (*acs)->acs_scan_req_param.mindwell = MIN_DWELL_TIME         ;
    (*acs)->acs_scan_req_param.maxdwell = MAX_DWELL_TIME ;
    atomic_set(&(*acs)->acs_in_progress,false);
    (*acs)->acs_run_incomplete = false;
    (*acs)->acs_tx_power_type = ACS_TX_POWER_OPTION_TPUT;
    (*acs)->acs_2g_allchan = 0;
    (*acs)->acs_status = ACS_DEFAULT;

    OS_INIT_TIMER(osdev, & (*acs)->acs_bk_scantimer, acs_bk_scan_timer, (void * )(*acs), QDF_TIMER_TYPE_WAKE_APPS);

    OS_INIT_TIMER((*acs)->acs_ic->ic_osdev,
            &((*acs)->acs_ch_hopping.ch_long_timer), ieee80211_ch_long_timer, (void *) ((*acs)->acs_ic), QDF_TIMER_TYPE_WAKE_APPS);

    OS_INIT_TIMER((*acs)->acs_ic->ic_osdev,
            &((*acs)->acs_ch_hopping.ch_nohop_timer), ieee80211_ch_nohop_timer,
            (void *) (*acs)->acs_ic, QDF_TIMER_TYPE_WAKE_APPS);

    OS_INIT_TIMER((*acs)->acs_ic->ic_osdev,
            &((*acs)->acs_ch_hopping.ch_cntwin_timer), ieee80211_ch_cntwin_timer,
            (void *) (*acs)->acs_ic, QDF_TIMER_TYPE_WAKE_APPS);
#ifdef ATH_SUPPORT_VOW_DCS
    /* Initializing DCS Enable timer */
    OS_INIT_TIMER((*acs)->acs_ic->ic_osdev, & (*acs)->dcs_enable_timer, dcs_enable, (void *)(*acs), QDF_TIMER_TYPE_WAKE_APPS);
#endif

#if ATH_ACS_DEBUG_SUPPORT
    (*acs)->acs_debug_bcn_events = NULL;
    (*acs)->acs_debug_chan_events = NULL;
#endif

    return 0;
}

int ieee80211_acs_attach(ieee80211_acs_t *acs,
        wlan_dev_t          devhandle,
        osdev_t             osdev)
{
#if WIFI_MEM_MANAGER_SUPPORT
    struct ol_ath_softc_net80211 *scn;
    int soc_idx;
    int pdev_idx;
#endif

    if (*acs)
        return -EINPROGRESS; /* already attached ? */

#if WIFI_MEM_MANAGER_SUPPORT
    scn = OL_ATH_SOFTC_NET80211(devhandle);
    soc_idx = scn->soc->soc_idx;
    pdev_idx = devhandle->ic_get_pdev_idx(devhandle);
    *acs = (ieee80211_acs_t ) (wifi_kmem_allocation(soc_idx, pdev_idx, KM_ACS, sizeof(struct ieee80211_acs), 0));
#else
    *acs = (ieee80211_acs_t) OS_MALLOC(osdev, sizeof(struct ieee80211_acs), 0);
#endif


    if (*acs) {
        ieee80211_acs_init(&(*acs), devhandle, osdev);
        return EOK;
    }

    return -ENOMEM;
}

int ieee80211_acs_set_param(ieee80211_acs_t acs, int param , int val)
{

    switch(param){
        case  IEEE80211_ACS_ENABLE_BK_SCANTIMER:
            acs->acs_bkscantimer_en = val ;
            if(val == 1){
                OS_SET_TIMER(&acs->acs_bk_scantimer, acs->acs_bk_scantime *1000);
            }else{
                OS_CANCEL_TIMER(&acs->acs_bk_scantimer);
            }
            break;
        case  IEEE80211_ACS_SCANTIME:
            acs->acs_bk_scantime = val;
            break;
        case  IEEE80211_ACS_RSSIVAR:
            acs->acs_rssivar = val ;
            break;
        case  IEEE80211_ACS_CHLOADVAR:
            acs->acs_chloadvar = val;
            break;
        case  IEEE80211_ACS_SRLOADVAR:
            acs->acs_srvar = val;
            break;
        case  IEEE80211_ACS_LIMITEDOBSS:
            acs->acs_limitedbsschk = val;
            break;
        case IEEE80211_ACS_CTRLFLAG:
            acs->acs_ic->ic_acs_ctrlflags =val;
            break;
        case IEEE80211_ACS_DEBUGTRACE:
            eacs_dbg_mask = val;
            break;
#if ATH_CHANNEL_BLOCKING
        case IEEE80211_ACS_BLOCK_MODE:
            acs->acs_block_mode = val & 0x3;
            break;
#endif

        case IEEE80211_ACS_TX_POWER_OPTION:
            if((val == ACS_TX_POWER_OPTION_TPUT) || (val == ACS_TX_POWER_OPTION_RANGE)) {
               acs->acs_tx_power_type = val;
            }
            else {
               printk("Invalid tx power type %d (Throughput 1, Range 2)\n",val);
            }
            break;

        case IEEE80211_ACS_2G_ALL_CHAN:
           acs->acs_2g_allchan = val;
           break;

        case IEEE80211_ACS_RANK:
            acs->acs_ranking = !!val;
            break;

        case IEEE80211_ACS_NF_THRESH:
            if ((val <= ACS_NOISE_FLOOR_THRESH_MAX)
                && (val >= ACS_NOISE_FLOOR_THRESH_MIN)) {
                acs->acs_noisefloor_threshold = val;
            }
	    else {
                qdf_print("Invalid values: Valid range is -85 to -65");
                return -EINVAL;
            }
            break;

        default :
            printk("Invalid param req %d \n",param);
            return -1;

    }

    return 0;

}

int ieee80211_acs_get_param(ieee80211_acs_t acs, int param )
{
    int val =0;
    switch(param){
        case IEEE80211_ACS_ENABLE_BK_SCANTIMER:
            val = acs->acs_bkscantimer_en ;
            break;
        case IEEE80211_ACS_SCANTIME:
            val = acs->acs_bk_scantime ;
            break;
        case IEEE80211_ACS_RSSIVAR:
            val = acs->acs_rssivar ;
            break;
        case  IEEE80211_ACS_CHLOADVAR:
            val = acs->acs_chloadvar ;
            break;
        case  IEEE80211_ACS_SRLOADVAR:
            val = acs->acs_srvar ;
            break;
        case IEEE80211_ACS_LIMITEDOBSS:
            val = acs->acs_limitedbsschk ;
            break;
        case IEEE80211_ACS_CTRLFLAG:
            val = acs->acs_ic->ic_acs_ctrlflags ;
            break;
        case IEEE80211_ACS_DEBUGTRACE:
            val = eacs_dbg_mask ;
            break;
#if ATH_CHANNEL_BLOCKING
        case IEEE80211_ACS_BLOCK_MODE:
            val = acs->acs_block_mode;
            break;
#endif
        case IEEE80211_ACS_TX_POWER_OPTION:
            val = acs->acs_tx_power_type ;
            break;
        case IEEE80211_ACS_2G_ALL_CHAN:
           val = acs->acs_2g_allchan;
           break;

        case IEEE80211_ACS_RANK:
           val = acs->acs_ranking;
           break;

        case IEEE80211_ACS_NF_THRESH:
            val = acs->acs_noisefloor_threshold;
            break;

        default :
            val = -1;
            printk("Invalid param req %d \n",param);
            return -1;

    }
    return val ;
}



void ieee80211_acs_deinit(ieee80211_acs_t *acs)
{
    struct acs_ch_hopping_t *ch = NULL;

    ch = &((*acs)->acs_ch_hopping);
    /*
     * Free synchronization objects
     */

    OS_FREE_TIMER(&(*acs)->acs_bk_scantimer);
    OS_FREE_TIMER(&ch->ch_cntwin_timer);
    OS_FREE_TIMER(&ch->ch_nohop_timer);
    OS_FREE_TIMER(&ch->ch_long_timer);
#if ATH_SUPPORT_VOW_DCS
    OS_FREE_TIMER(&(*acs)->dcs_enable_timer);
#endif
    spin_lock_destroy(&((*acs)->acs_lock));
    qdf_spinlock_destroy(&((*acs)->acs_ev_lock));
}

int ieee80211_acs_detach(ieee80211_acs_t *acs)
{
#if WIFI_MEM_MANAGER_SUPPORT
    wlan_dev_t devhandle;
    struct ol_ath_softc_net80211 *scn;
#endif

    if (*acs == NULL)
        return EINPROGRESS; /* already detached ? */

#if WIFI_MEM_MANAGER_SUPPORT
    devhandle = (*acs)->acs_ic;
    scn = OL_ATH_SOFTC_NET80211(devhandle);
#endif

    ieee80211_acs_deinit(&(*acs));

#if WIFI_MEM_MANAGER_SUPPORT
    wifi_kmem_free(scn->soc->soc_idx, devhandle->ic_get_pdev_idx(devhandle), KM_ACS,(void *)(*acs));
#else
    OS_FREE(*acs);
#endif

    *acs = NULL;

    return EOK;
}

static void
ieee80211_acs_post_event(ieee80211_acs_t                 acs,
        struct ieee80211_ath_channel       *channel)
{
    int                                 i,num_handlers;
    ieee80211_acs_event_handler         acs_event_handlers[IEEE80211_MAX_ACS_EVENT_HANDLERS];
    void                                *acs_event_handler_arg[IEEE80211_MAX_ACS_EVENT_HANDLERS];

    eacs_trace(EACS_DBG_LVL1,("entry"));
    /*
     * make a local copy of event handlers list to avoid
     * the call back modifying the list while we are traversing it.
     */
    spin_lock(&acs->acs_lock);
    num_handlers=acs->acs_num_handlers;
    for (i=0; i < num_handlers; ++i) {
        acs_event_handlers[i] = acs->acs_event_handlers[i];
        acs_event_handler_arg[i] = acs->acs_event_handler_arg[i];
    }
    spin_unlock(&acs->acs_lock);
    for (i = 0; i < num_handlers; ++i) {
        (acs_event_handlers[i]) (acs_event_handler_arg[i], channel);
    }
}

/*
 * scan handler used for scan complete
 */
static void ieee80211_ht40intol_evhandler(struct wlan_objmgr_vdev *vdev,
        struct scan_event *event, void *arg)
{
    struct ieee80211vap *originator = wlan_vdev_get_mlme_ext_obj(vdev);
    ieee80211_acs_t acs = NULL;
    struct ieee80211vap *vap = NULL;
    int i = 0;
    uint64_t lock_held_duration = 0;

    if (originator == NULL) {
        eacs_trace(EACS_DBG_LVL0, ("vdev pointer is invalid\n"));
        return;
    }

    if (arg == NULL) {
        eacs_trace(EACS_DBG_LVL0,("acs is null"));
        return;
    }
    acs = (ieee80211_acs_t) arg;
    vap = acs->acs_vap;

    /*
     * we don't need lock in evhandler since
     * 1. scan module would guarantee that event handlers won't get called simultaneously
     * 2. acs_in_progress prevent furher access to ACS module
     */
#if 0
#if DEBUG_EACS
    printk( "%s scan_id %08X event %d reason %d \n", __func__, event->scan_id, event->type, event->reason);
#endif
#endif

#if ATH_SUPPORT_MULTIPLE_SCANS
    /*
     * Ignore notifications received due to scans requested by other modules
     * and handle new event SCAN_EVENT_TYPE_DEQUEUED.
     */
    ASSERT(0);
    /* Ignore events reported by scans requested by other modules */
    if (acs->acs_scan_id != event->scan_id) {
        return;
    }

#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */
    if( event->type == SCAN_EVENT_TYPE_FOREIGN_CHANNEL_GET_NF ) {
        struct ieee80211com *ic = originator->iv_ic;
        /* Get the noise floor value */
        acs->acs_noisefloor[ieee80211_mhz2ieee(originator->iv_ic,event->chan_freq, 0)] =
            ic->ic_get_cur_chan_nf(ic);
        eacs_trace(EACS_DBG_LVL0,("Updating Noise floor chan %d value %d",
                    ieee80211_mhz2ieee(originator->iv_ic,event->chan_freq, 0),
                    acs->acs_noisefloor[ieee80211_mhz2ieee(originator->iv_ic,event->chan_freq, 0)]));
    }

    if ((event->type != SCAN_EVENT_TYPE_COMPLETED) &&
            (event->type != SCAN_EVENT_TYPE_DEQUEUED)) {
        return;
    }
    vap->iv_ic->ic_ht40intol_scan_running = 0;
    qdf_spin_lock(&acs->acs_ev_lock);
    /* If ACS is not in progress, then return, since ACS cancel was called before this*/
    if (!ieee80211_acs_in_progress(acs)) {
        qdf_spin_unlock(&acs->acs_ev_lock);
        return;
    }
    lock_held_duration = qdf_ktime_to_ms(qdf_ktime_get());

    if (event->reason != SCAN_REASON_COMPLETED) {
        eacs_trace(EACS_DBG_LVL0,( "Scan not totally complete. Should not occur normally! Investigate" ));
        goto scan_done;
    }
    ieee80211_find_40intol_overlap(acs, vap->iv_des_chan[vap->iv_des_mode]);

    for(i=0; i<IEEE80211_CHAN_MAX ;i++) {
        acs->acs_chan_rssitotal[i]= 0;
        acs->acs_chan_rssi[i] = 0;
    }

    ucfg_scan_db_iterate(wlan_vap_get_pdev(acs->acs_vap),
            ieee80211_acs_get_channel_maxrssi_n_secondary_ch, acs);

scan_done:
    ieee80211_free_ht40intol_scan_resource(acs);
    ieee80211_acs_post_event(acs, vap->iv_des_chan[vap->iv_des_mode]);

    /*Comparing to true and setting it false return value is
        true in case comparison is successfull */

    if(!OS_ATOMIC_CMPXCHG(&(acs->acs_in_progress),true,false))
     {
         eacs_trace(EACS_DBG_LVL0,("Wrong locking in ACS --investigate -- "));
         atomic_set(&(acs->acs_in_progress),false);
     }

    ieee80211_check_and_execute_pending_acsreport(vap);

    lock_held_duration = qdf_ktime_to_ms(qdf_ktime_get()) - lock_held_duration;
    qdf_spin_unlock(&acs->acs_ev_lock);
    qdf_info("lock held duration: %llu(ms)", lock_held_duration);
    return;
}

struct ieee80211_ath_channel *
ieee80211_acs_check_altext_channel(
        ieee80211_acs_t acs,
        struct ieee80211_ath_channel *channel )
{
    int i;
    struct ieee80211_ath_channel *altchannel =NULL;


    if(channel->ic_flags & IEEE80211_CHAN_11NG_HT40PLUS ){
        /*find the HT40minu channle */
        for (i = 0; i < acs->acs_nchans; i++) {
            if(channel->ic_ieee == acs->acs_chans[i]->ic_ieee ){
                if(channel == acs->acs_chans[i])
                    continue;
                if(channel->ic_flags & IEEE80211_CHAN_11NG_HT40MINUS ){
                    altchannel = acs->acs_chans[i];

                }

            }
        }

    }else{
        /*find the HT40minu channle */
        for (i = 0; i < acs->acs_nchans; i++) {

            if(channel->ic_ieee == acs->acs_chans[i]->ic_ieee){
                if(channel == acs->acs_chans[i])
                    continue;

                if(channel->ic_flags & IEEE80211_CHAN_11NG_HT40PLUS){
                    altchannel = acs->acs_chans[i];
                }

            }
        }

    }

    if(altchannel !=NULL){
        ieee80211_find_40intol_overlap(acs, altchannel);
        /* 11AX TODO: Recheck future 802.11ax drafts (>D1.0) on coex rules */
        if(!(altchannel->ic_flags & (IEEE80211_CHAN_HT40INTOLMARK |
                                     IEEE80211_CHAN_HE40INTOLMARK))){
            channel = altchannel;
        }
    }
    return channel;
}


/**
 * @brief Update Secondary Channel RSSIs
 *
 * @return
 */
static void ieee80211_acs_update_sec_chan_rssi(ieee80211_acs_t acs)
{
    int i;

    for(i=0; i<IEEE80211_CHAN_MAX ;i++) {
        acs->acs_chan_rssitotal[i]= 0;
        acs->acs_chan_rssi[i] = 0;
    }

    ucfg_scan_db_iterate(wlan_vap_get_pdev(acs->acs_vap),
            ieee80211_acs_get_channel_maxrssi_n_secondary_ch, acs);
    return;
}

/**
 * @brief Rank the channels after ACS scan
 *
 * @param chans_to_rank - number of channels to rank from the top
 *
 * @return
 */
static struct ieee80211_ath_channel *ieee80211_acs_rank_channels(ieee80211_acs_t acs, uint8_t chans_to_rank)
{
    int i;
    struct ieee80211_ath_channel *channel = NULL;
    struct ieee80211_ath_channel *top_channel = NULL;
    u_int8_t ieee_chan_num = 0;
    u_int32_t random_chan = 0;
    u_int32_t ht40minus_chan = 0;
    struct ieee80211com *ic = acs->acs_ic;

    /* For ACS channel ranking, get best channel repeatedly after blacklisting
     * the gotten channel and rank them in order */

        OS_MEMSET(acs->acs_rank, 0x0, sizeof(acs_rank_t)*IEEE80211_ACS_CHAN_MAX);
        /* Allot Rank for the returned best channel and mark it as blacklisted
         * channel so that it wont be considered for selection of best channel
         * in the next iteration.
         * A random channel is returned when all the channels are unusable
         * in which case its noted*/

        for (i = 0; i < chans_to_rank; i++) {
            channel = ieee80211_acs_find_best_centerchan(acs);
            switch (acs->acs_vap->iv_des_mode) {
                case IEEE80211_MODE_11NG_HT40PLUS:
			    case IEEE80211_MODE_11NG_HT40MINUS:
			    case IEEE80211_MODE_11NG_HT40:
			    case IEEE80211_MODE_11AXG_HE40PLUS:
			    case IEEE80211_MODE_11AXG_HE40MINUS:
			    case IEEE80211_MODE_11AXG_HE40:
			        ieee80211_find_40intol_overlap(acs, channel);
                default:
                    break;
            }

            ieee_chan_num = ieee80211_chan2ieee(ic, channel);
            if ((acs->acs_channelrejflag[ieee_chan_num]) & (ACS_REJFLAG_BLACKLIST)){
                random_chan = ieee_chan_num;
            }
            else {
                if (ieee_chan_num != 0) {
                    acs->acs_channelrejflag[ieee_chan_num] |= ACS_REJFLAG_BLACKLIST;
                    acs->acs_rank[ieee_chan_num].rank = (i+1);
                        if (!i)
                           top_channel = channel;

                }
            }
        }

        /* Mark a Reason code in the channel rank description for the all other
         * channels which have been Rejected for use */
        for (i = 0; i < acs->acs_nchans; i++) {
            channel = acs->acs_chans[i];
            ieee_chan_num = ieee80211_chan2ieee(ic, channel);

            /* WAR to handle channel 5,6,7 for HT40 mode
             * (2 entries of channel are present) one for HT40PLUS and
             * another for HT40MINUS, the HT40MINUS channel flags are
             * stored in ht40minus_chan index of the acs flags array */
            if (((ieee_chan_num >= 5)&& (ieee_chan_num <= 7)) &&
                            IEEE80211_IS_CHAN_11NG_HT40MINUS(channel)) {

                ht40minus_chan = 15 + (ieee_chan_num - 5);

                snprintf(acs->acs_rank[ieee_chan_num].desc, ACS_RANK_DESC_LEN,
                        "(%s%s%s%s%s%s%s%s%s%s%s%s%s%s)",
                        (ieee_chan_num == random_chan)? "Random ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_SECCHAN))? "SC ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_WEATHER_RADAR))? "WR ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_DFS))? "DF ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_HIGHNOISE))? "HN ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_RSSI))? "RS ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_CHANLOAD))? "CL ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_REGPOWER))? "RP ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_NON2G))? "N2G ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_PRIMARY_80_80))? "P80X ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_NO_SEC_80_80))? "NS80X ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_NO_PRIMARY_80_80))? "NP80X ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_FLAG_NON5G))? "N5G ":"",
                        ((acs->acs_channelrejflag[ht40minus_chan] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_SPATIAL_REUSE))? "SPRE ":"");

                        acs->acs_channelrejflag[ht40minus_chan] &= ~ACS_REJFLAG_BLACKLIST;
            }
            else
            {
                snprintf(acs->acs_rank[ieee_chan_num].desc, ACS_RANK_DESC_LEN,
                        "(%s%s%s%s%s%s%s%s%s%s%s%s%s%s)",
                        (ieee_chan_num == random_chan)? "Random ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_SECCHAN))? "SC ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_WEATHER_RADAR))? "WR ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_DFS))? "DF ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_HIGHNOISE))? "HN ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_RSSI))? "RS ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_CHANLOAD))? "CL ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_REGPOWER))? "RP ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_NON2G))? "N2G ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_PRIMARY_80_80))? "P80X ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_NO_SEC_80_80))? "NS80X ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_NO_PRIMARY_80_80))? "NP80X ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_FLAG_NON5G))? "N5G ":"",
                        ((acs->acs_channelrejflag[ieee_chan_num] &
                                (~ACS_REJFLAG_BLACKLIST)) &
                                (ACS_REJFLAG_SPATIAL_REUSE))? "SPRE ":"");

                        acs->acs_channelrejflag[ieee_chan_num] &= ~ACS_REJFLAG_BLACKLIST;
            }
        }
    return top_channel;
}

/**
 * @brief to used internally to acs to set/get value to ath layer
 */
static int acs_noise_detection_param(struct ieee80211com *ic ,int cmd ,int param,int *val)
{
    int err = EOK;

    if (!ieee80211com_has_cap_ext(ic,IEEE80211_ACS_CHANNEL_HOPPING))
        return EINVAL;

    if (cmd) {
        if (ic->ic_set_noise_detection_param) {
            if (param == IEEE80211_ENABLE_NOISE_DETECTION ||
                    param == IEEE80211_NOISE_THRESHOLD) /*Rest param are not supported */
                ic->ic_set_noise_detection_param(ic,param,*val);
            else {
                err = EINVAL;
            }
        }
        else { /* SET Noise detection in ath layer not enabled */
            err = EINVAL;
        }
    } else { /* GET part */
        if (ic->ic_get_noise_detection_param) {
            if (IEEE80211_GET_COUNTER_VALUE == param)
                ic->ic_get_noise_detection_param(ic,param,val);
            else  { /* in get path we dont need to get it from ath layer*/
                err = EINVAL;
            }
        } else
                err = EINVAL;
    }
    return err;
}

void ieee80211_acs_retrieve_chan_info(struct ieee80211com *ic, enum scan_event_type type, uint32_t freq)
{
    u_int32_t now = 0;
    u_int8_t flags;
    ieee80211_acs_t acs = ic->ic_acs;

    if (type == SCAN_EVENT_TYPE_FOREIGN_CHANNEL_GET_NF) {
        now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
        flags = ACS_CHAN_STATS_NF;
        /* Get the noise floor value */
        acs->acs_noisefloor[ieee80211_mhz2ieee(ic, freq, 0)] = ic->ic_get_cur_chan_nf(ic);

        eacs_trace(EACS_DBG_SCAN,("Requesting for CHAN STATS and NF from Target \n"));
        eacs_trace(EACS_DBG_SCAN,("%d.%03d | %s:request stats and nf \n",  now / 1000, now % 1000, __func__));
        if (ic->ic_hal_get_chan_info)
            ic->ic_hal_get_chan_info(ic, flags);
    }
    if ( type == SCAN_EVENT_TYPE_FOREIGN_CHANNEL ) {
        /* get initial chan stats for the current channel */
        flags = ACS_CHAN_STATS;
        if (ic->ic_hal_get_chan_info)
            ic->ic_hal_get_chan_info(ic, flags);
    }
    return;
}

/*
 * scan handler used for scan complete
 */
static void ieee80211_acs_scan_evhandler(struct wlan_objmgr_vdev *vdev,
        struct scan_event *event, void *arg)
{
    struct ieee80211vap *originator = wlan_vdev_get_mlme_ext_obj(vdev);
    struct ieee80211_ath_channel *channel = IEEE80211_CHAN_ANYC;
    struct ieee80211com *ic;
    ieee80211_acs_t acs;
    struct acs_ch_hopping_t *ch = NULL;
    int val = 0,retval = 0;
    struct ieee80211vap *vap, *tvap;
    uint64_t lock_held_duration = 0;

    if (originator == NULL) {
        eacs_trace(EACS_DBG_LVL0, ("vdev pointer is invalid\n"));
        return;
    }

    if (arg == NULL) {
        eacs_trace(EACS_DBG_LVL0,("acs is null"));
        return;
    }

    ic = originator->iv_ic;
    acs = (ieee80211_acs_t) arg;
    ch = &(acs->acs_ch_hopping);
    /*
     * we don't need lock in evhandler since
     * 1. scan module would guarantee that event handlers won't get called simultaneously
     * 2. acs_in_progress prevent furher access to ACS module
     */
    eacs_trace(EACS_DBG_SCAN,( "scan_id %08X event %d reason %d ",
                event->scan_id, event->type, event->reason));

#if ATH_SUPPORT_MULTIPLE_SCANS
    /*
     * Ignore notifications received due to scans requested by other modules
     * and handle new event IEEE80211_SCAN_DEQUEUED.
     */
    ASSERT(0);

    /* Ignore events reported by scans requested by other modules */
    if (acs->acs_scan_id != event->scan_id) {
        return;
    }
#endif    /* ATH_SUPPORT_MULTIPLE_SCANS */

    /*
     * Retrieve the Noise floor information and channel load
     * in case of channel change and restart the noise floor
     * computation for the next channel
     */
    ieee80211_acs_retrieve_chan_info(ic, event->type, event->chan_freq);

    if (event->type != SCAN_EVENT_TYPE_COMPLETED) {
        return;
    }

    qdf_spin_lock(&acs->acs_ev_lock);
    /* If ACS is not in progress, then return, since ACS cancel was callled before this*/
    if (!ieee80211_acs_in_progress(acs)) {
        qdf_spin_unlock(&acs->acs_ev_lock);
        return;
    }

    lock_held_duration = qdf_ktime_to_ms(qdf_ktime_get());

    if (event->reason != SCAN_REASON_COMPLETED) {
        eacs_trace(EACS_DBG_SCAN,( " Scan not totally complete. Should not occur normally! Investigate." ));
        /* If scan is cancelled, ACS should invoke the scan again*/
        channel = IEEE80211_CHAN_ANYC;
        goto scan_done;
    }

    ieee80211_acs_update_sec_chan_rssi(acs);
    if (acs->acs_ranking) {
        channel = ieee80211_acs_rank_channels(acs, acs->acs_nchans);
    } else {
        /* To prevent channel selection when acs report is active */
        if (!acs->acs_scan_req_param.acs_scan_report_active) {
            channel = ieee80211_acs_find_best_centerchan(acs);
            switch (acs->acs_vap->iv_des_mode) {
            case IEEE80211_MODE_11NG_HT40PLUS:
            case IEEE80211_MODE_11NG_HT40MINUS:
            case IEEE80211_MODE_11NG_HT40:
            case IEEE80211_MODE_11AXG_HE40PLUS:
            case IEEE80211_MODE_11AXG_HE40MINUS:
            case IEEE80211_MODE_11AXG_HE40:
                ieee80211_find_40intol_overlap(acs, channel);
                break;
            default:
                break;
            }
        }
    }

scan_done:
    ieee80211_acs_free_scan_resource(acs);
    /*ACS scan report is going on no need to take any decision for
      channel hopping timers */
    if (!acs->acs_scan_req_param.acs_scan_report_active) {
        if (ieee80211com_has_cap_ext(ic,IEEE80211_ACS_CHANNEL_HOPPING)) {

            if (!acs->acs_ch_hopping.ch_max_hop_cnt) {
                retval = acs_noise_detection_param(ic,true,IEEE80211_NOISE_THRESHOLD,
                        &ch->param.noise_thresh);
                if (retval == EOK ) {
                    OS_SET_TIMER(&ch->ch_long_timer, SEC_TO_MSEC(ch->param.long_dur));
                } else {
                    goto out;
                }
            }
            if (acs->acs_ch_hopping.ch_max_hop_cnt < ACS_CH_HOPPING_MAX_HOP_COUNT) {
                /* API to intimate sc that no hop is going on so should
                 * not calculate the noise floor */
                retval = acs_noise_detection_param(ic,true,IEEE80211_ENABLE_NOISE_DETECTION,&val);
                if (retval == EOK) {
                    OS_SET_TIMER(&ch->ch_nohop_timer, SEC_TO_MSEC(ch->param.nohop_dur)); /*In sec*/
                    acs->acs_ch_hopping.ch_nohop_timer_active = true;
                }
                else {
                    goto out;
                }
            }
        }
    }
out:
    /* To prevent channel selection when channel load report is active */
    if(!acs->acs_scan_req_param.acs_scan_report_active) {
        ieee80211_acs_post_event(acs, channel);
    }
#if QCA_SUPPORT_SON
    else {
        u_int32_t i = 0;
        for (i = 0; i < acs->acs_uchan_list.uchan_cnt; i++) {
            u_int ieee_chan_num = acs->acs_uchan_list.uchan[i];

            /* Inform the band steering module of the channel utilization. It will
             * ignore it if it is not for the right channel or it was not
             * currently requesting it.
             */
            son_record_utilization(acs->acs_vap->vdev_obj, ieee_chan_num,
                                   acs->acs_chan_load[ieee_chan_num]);
        }
    }
#endif

    if (!OS_ATOMIC_CMPXCHG(&(acs->acs_in_progress),true,false))
    {
        eacs_trace(EACS_DBG_LVL0,("Wrong locking in ACS --investigate -- "));
        atomic_set(&(acs->acs_in_progress),false);
    }

    acs->acs_scan_req_param.acs_scan_report_active = false;
    ieee80211_check_and_execute_pending_acsreport(acs->acs_vap);

    /*as per requirement we need to send event both for DCS as well as channel hop*/
    if (ch->ch_hop_triggered || ic->cw_inter_found) {
        if(channel != IEEE80211_CHAN_ANYC) {
            eacs_trace(EACS_DBG_LVL0,("Changed Channel Due to Channel Hopping %d ",
                        ieee80211_chan2ieee(acs->acs_ic, channel)));
            IEEE80211_DELIVER_EVENT_CH_HOP_CHANNEL_CHANGE(acs->acs_vap,ieee80211_chan2ieee(acs->acs_ic, channel));
        }
    }
    ch->ch_hop_triggered = false;

    TAILQ_FOREACH_SAFE(vap, &ic->ic_vaps, iv_next, tvap) {
        if (vap->ap_chan_rpt_enable && ieee80211_bg_scan_enabled(vap))
            ieee80211_update_ap_chan_rpt(vap);

        if (vap->rnr_enable && ieee80211_bg_scan_enabled(vap))
            ieee80211_update_rnr(vap);
    }

    lock_held_duration = qdf_ktime_to_ms(qdf_ktime_get()) - lock_held_duration;
    qdf_spin_unlock(&acs->acs_ev_lock);
    qdf_info("lock held duration: %llu(ms)", lock_held_duration);
    return;
}

static void ieee80211_acs_free_scan_resource(ieee80211_acs_t acs)
{
    struct wlan_objmgr_psoc *psoc = NULL;

    psoc = wlan_vap_get_psoc(acs->acs_vap);
    /* Free requester ID and callback () */
    ucfg_scan_unregister_requester(psoc, acs->acs_scan_requestor);
}

static void ieee80211_free_ht40intol_scan_resource(ieee80211_acs_t acs)
{
    struct wlan_objmgr_psoc *psoc = NULL;

    psoc = wlan_vap_get_psoc(acs->acs_vap);
    /* Free requester ID and callback () */
    ucfg_scan_unregister_requester(psoc, acs->acs_scan_requestor);
}

static INLINE void
ieee80211_acs_iter_vap_channel(void *arg, struct ieee80211vap *vap, bool is_last_vap)
{
    struct ieee80211vap *current_vap = (struct ieee80211vap *) arg;
    struct ieee80211_ath_channel *channel;

    if (wlan_vap_get_opmode(vap) != IEEE80211_M_HOSTAP) {
        return;
    }
    if (ieee80211_acs_channel_is_set(current_vap)) {
        return;
    }
    if (vap == current_vap) {
        return;
    }

    if (ieee80211_acs_channel_is_set(vap)) {
        channel =  vap->iv_des_chan[vap->iv_des_mode];
        current_vap->iv_ic->ic_acs->acs_channel = channel;
    }

}
static void ieee80211_acs_flush_olddata(ieee80211_acs_t acs)
{
    int i;

    eacs_trace(EACS_DBG_LVL0,( " Flush old data " ));
    acs->acs_minrssi_11na = 0xffffffff ;
    acs->acs_minrssisum_11ng = 0xffffffff;
    OS_MEMSET(acs->acs_chan_nbss, 0x0, sizeof(acs->acs_chan_nbss));
    OS_MEMSET(acs->acs_chan_rssitotal, 0x0, sizeof(acs->acs_chan_rssitotal));
    OS_MEMSET(acs->acs_chan_rssi,      0x0, sizeof(acs->acs_chan_rssi));
    OS_MEMSET(acs->acs_chan_maxrssi,   0x0, sizeof(acs->acs_chan_maxrssi));
    OS_MEMSET(acs->acs_chan_minrssi,   0x0, sizeof(acs->acs_chan_minrssi));
    OS_MEMSET(acs->acs_chan_load,      0x0, sizeof(acs->acs_chan_load));
    OS_MEMSET(acs->acs_cycle_count,    0x0, sizeof(acs->acs_cycle_count));
    OS_MEMSET(acs->acs_adjchan_load,   0x0, sizeof(acs->acs_adjchan_load));
    OS_MEMSET(acs->acs_chan_loadsum,   0x0, sizeof(acs->acs_chan_loadsum));
    OS_MEMSET(acs->acs_chan_regpower,  0x0, sizeof(acs->acs_chan_regpower));
    OS_MEMSET(acs->acs_channelrejflag, 0x0, sizeof(acs->acs_channelrejflag));
    OS_MEMSET(acs->acs_adjchan_flag,   0x0, sizeof(acs->acs_adjchan_flag));
    OS_MEMSET(acs->acs_adjchan_load,   0x0, sizeof(acs->acs_adjchan_load));
    OS_MEMSET(acs->acs_srp_supported,  0x0, sizeof(acs->acs_srp_supported));
    OS_MEMSET(acs->acs_srp_load,       0x0, sizeof(acs->acs_srp_load));
    for(i = 0 ; i < IEEE80211_CHAN_MAX ; i++) {
         acs->acs_noisefloor[i] = NF_INVALID;
         acs->acs_sec_chan[i] = false;
    }




}


static int ieee80211_find_ht40intol_bss(struct ieee80211vap *vap)
{
    struct ieee80211com *ic = NULL;
    ieee80211_acs_t acs = NULL;
    struct ieee80211_ath_channel *chan;
    struct scan_start_request *scan_params = NULL;
    uint32_t *chan_list = NULL;
    uint32_t chan_count = 0;
    int ret = EOK;
    u_int8_t i;
    struct wlan_objmgr_vdev *vdev = NULL;
    struct wlan_objmgr_pdev *pdev = NULL;
    struct wlan_objmgr_psoc *psoc = NULL;
    QDF_STATUS status = QDF_STATUS_SUCCESS;

    if ((vap == NULL) || (vap->iv_ic == NULL) || (vap->iv_ic->ic_acs == NULL)) {
        eacs_trace(EACS_DBG_LVL0,("vap/ic/acs is null" ));
        return EINVAL;
    }

    vdev = vap->vdev_obj;
    status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_OSIF_SCAN_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        scan_info("unable to get reference");
        return EBUSY;
    }

    ic = vap->iv_ic;
    acs = ic->ic_acs;

    eacs_trace(EACS_DBG_LVL0,("entry"));
    spin_lock(&acs->acs_lock);

    if(OS_ATOMIC_CMPXCHG(&(acs->acs_in_progress),false,true)) {
        /* Just wait for acs done */
        spin_unlock(&acs->acs_lock);
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        return EINPROGRESS;
    }

    acs->acs_run_incomplete = false;
    spin_unlock(&acs->acs_lock);

   /* acs_in_progress prevents others from reaching here so unlocking is OK */

    acs->acs_vap = vap;

    /* reset channel mapping array */
    OS_MEMSET(&acs->acs_chan_maps, 0xff, sizeof(acs->acs_chan_maps));
    acs->acs_nchans = 0;
    /* Get 11NG HT20 channels */
    ieee80211_acs_get_phymode_channels(acs, IEEE80211_MODE_11NG_HT20);

    if (acs->acs_nchans == 0) {
        eacs_trace(EACS_DBG_LVL0,("Cannot construct the available channel list." ));
        goto err;
    }

    /* register scan event handler */
    psoc = wlan_vap_get_psoc(vap);
    acs->acs_scan_requestor = ucfg_scan_register_requester(psoc, (uint8_t*)"acs",
            ieee80211_ht40intol_evhandler, (void *)acs);
    if (!acs->acs_scan_requestor) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                "%s: wlan_scan_register_event_handler() failed handler=%08p,%08p status=%08X\n",
                __func__, ieee80211_ht40intol_evhandler, acs, status);
        scan_err("unable to allocate requester: status: %d", status);
        goto err;
    }

    chan_list = (u_int32_t *) qdf_mem_malloc(sizeof(u_int32_t)*acs->acs_nchans);
    if (!chan_list) {
        scan_err("unable to allocate chan list");
        goto err;
    }

    scan_params = (struct scan_start_request*) qdf_mem_malloc(sizeof(*scan_params));
    if (!scan_params) {
        scan_err("unable to allocate scan request");
        goto err;
    }
    status = wlan_update_scan_params(vap,scan_params,IEEE80211_M_HOSTAP,
            false,true,false,true,0,NULL,0);
    if (status) {
        qdf_mem_free(scan_params);
        scan_err("scan param init failed with status: %d", status);
        goto err;
    }

    scan_params->legacy_params.scan_type = SCAN_TYPE_FOREGROUND;
    scan_params->scan_req.scan_flags = 0;
    scan_params->scan_req.scan_f_passive = true;
    scan_params->legacy_params.min_dwell_passive = MIN_DWELL_TIME;
    scan_params->scan_req.dwell_time_passive = MAX_DWELL_TIME;
    scan_params->legacy_params.min_dwell_active = MIN_DWELL_TIME;
    scan_params->scan_req.dwell_time_active = MAX_DWELL_TIME;
    scan_params->scan_req.scan_f_2ghz = true;

    /* scan needs to be done on 2GHz  channels */
    for (i = 0; i < acs->acs_nchans; i++) {
        chan = acs->acs_chans[i];
        if (IEEE80211_IS_CHAN_2GHZ(chan)) {
            chan_list[chan_count++] = chan->ic_freq;
        }
    }
    status = ucfg_scan_init_chanlist_params(scan_params,
            chan_count, chan_list, NULL);
    if (status) {
        qdf_mem_free(scan_params);
        goto err;
    }

    /* If scan is invoked from ACS, Channel event notification should be
     * enabled  This is must for offload architecture
     */
    scan_params->scan_req.scan_f_chan_stat_evnt = true;

    pdev = wlan_vdev_get_pdev(vdev);
    /*Flush scan table before starting scan */
    ucfg_scan_flush_results(pdev, NULL);

    ieee80211_acs_flush_olddata(acs);

    /* Try to issue a scan */
    if ((status = wlan_ucfg_scan_start(vap,
                scan_params,
                acs->acs_scan_requestor,
                SCAN_PRIORITY_HIGH,
                &(acs->acs_scan_id), 0, NULL)) != QDF_STATUS_SUCCESS) {
        eacs_trace(EACS_DBG_LVL0,( " Issue a scan fail."  ));
        scan_err("scan_start failed with status: %d", status);
        goto err;
    }


    vap->iv_ic->ic_ht40intol_scan_running = 1;
    goto end;

err:
    ieee80211_free_ht40intol_scan_resource(acs);

    if(!OS_ATOMIC_CMPXCHG(&(acs->acs_in_progress),true,false))
     {
         eacs_trace(EACS_DBG_LVL0,("Wrong locking in ACS --investigate -- "));
         eacs_trace(EACS_DBG_LVL0,("entry"));
         atomic_set(&(acs->acs_in_progress),false);
      }

    ieee80211_acs_post_event(acs, vap->iv_des_chan[vap->iv_des_mode]);
end:
    if (chan_list) {
        qdf_mem_free(chan_list);
    }
    wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
    return ret;
}

static int ieee80211_autoselect_infra_bss_channel(struct ieee80211vap *vap, bool is_scan_report,
                                                  cfg80211_hostapd_acs_params *cfg_acs_params)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_acs_t acs = ic->ic_acs;
    struct ieee80211_ath_channel *channel;
    struct scan_start_request *scan_params = NULL;
    u_int32_t num_vaps;
    int ret = EOK;
    u_int8_t chan_list_allocated = false;
    struct acs_ch_hopping_t *ch = NULL;
    struct wlan_objmgr_psoc *psoc = NULL;
    uint32_t num_channels = 0;
    uint32_t *chan_list = NULL;
    QDF_STATUS status = QDF_STATUS_SUCCESS;
    struct wlan_objmgr_vdev *vdev = NULL;
    struct wlan_objmgr_pdev *pdev = NULL;
    u_int32_t *scan_channel_list = NULL;
    u_int16_t ix;
    u_int8_t skip_scan = 0;

    eacs_trace(EACS_DBG_LVL0,("Invoking ACS module for Best channel selection "));
    scan_info("ACS started: vap:0x%pK", vap);

    vdev = vap->vdev_obj;
    status = wlan_objmgr_vdev_try_get_ref(vdev, WLAN_OSIF_SCAN_ID);
    if (QDF_IS_STATUS_ERROR(status)) {
        scan_info("unable to get reference");
        return EBUSY;
    }

    spin_lock(&acs->acs_lock);

    if(OS_ATOMIC_CMPXCHG(&(acs->acs_in_progress),false,true)) {
        /* Just wait for acs done */
        spin_unlock(&acs->acs_lock);
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        return EINPROGRESS;
    }
    acs->acs_run_incomplete = false;
    /* check if any VAP already set channel */
    acs->acs_channel = NULL;
    ch = &(acs->acs_ch_hopping);
    ieee80211_iterate_vap_list_internal(ic, ieee80211_acs_iter_vap_channel,vap,num_vaps);

    spin_unlock(&acs->acs_lock);

    acs->acs_scan_req_param.acs_scan_report_active = is_scan_report;

    /* acs_in_progress prevents others from reaching here so unlocking is OK */
    /* Report active means we dont want to select channel so its okay to go beyond */
    /* if channel change triggered by channel hopping then go ahead */
    if (acs->acs_channel && (!ic->cw_inter_found)
            &&  (!acs->acs_scan_req_param.acs_scan_report_active)
            &&  (!ch->ch_hop_triggered)) {
        /* wlan scanner not yet started so acs_in_progress = true is OK */
        ieee80211_acs_post_event(acs, acs->acs_channel);
        atomic_set(&acs->acs_in_progress,false);
        wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
        return EOK;
    }

    acs->acs_vap = vap;

    if(cfg_acs_params != NULL){
        vap->iv_des_mode = cfg_acs_params->hw_mode;
    }

    /*  when report is active we dont want to depend on des mode as des mode will
        give channel list only for paricular mode in acsreport so choosing IEEE80211_MODE_NONE instead.
        IEEE80211_MODE_NONE mode makes sure maximum channels are added for the scanning.
    */

    if(acs->acs_scan_req_param.acs_scan_report_active) {
        ieee80211_acs_construct_chan_list(acs, IEEE80211_MODE_NONE);
    } else if((cfg_acs_params != NULL) && (cfg_acs_params->ch_list != NULL)) {
        u_int8_t i;
        for(i = 0; i < cfg_acs_params->ch_list_len; i++){
            if (!acs_is_channel_blocked(acs, cfg_acs_params->ch_list[i])) {
                channel = ieee80211_find_dot11_channel(ic, cfg_acs_params->ch_list[i],
                                     0, cfg_acs_params->hw_mode);
                if((channel != NULL) &&  (channel != IEEE80211_CHAN_ANYC))
                    acs->acs_chans[acs->acs_nchans++] = channel;
            }
        }
    } else {
        ieee80211_acs_construct_chan_list(acs,acs->acs_vap->iv_des_mode);
    }

    if (acs->acs_nchans == 0) {
        eacs_trace(EACS_DBG_DEFAULT,( " Cannot construct the available channel list." ));
        scan_err("Can't construct chan list");
        ret = -EINVAL;
        goto err;
    }
#if ATH_SUPPORT_VOW_DCS
    /* update dcs information */
    if(ic->cw_inter_found && ic->ic_curchan){
        acs->acs_intr_status[ieee80211_chan2ieee(ic, ic->ic_curchan)] += 1;
        acs->acs_intr_ts[ieee80211_chan2ieee(ic, ic->ic_curchan)] =
            (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    }
#endif

    /* register scan event handler */
    psoc = wlan_vap_get_psoc(vap);
    acs->acs_scan_requestor = ucfg_scan_register_requester(psoc, (uint8_t*)"acs",
        ieee80211_acs_scan_evhandler, (void *)acs);

    if (!acs->acs_scan_requestor) {
        IEEE80211_DPRINTF(vap, IEEE80211_MSG_ANY,
                "%s: wlan_scan_register_event_handler() failed handler=%08p,%08p rc=%08X",
                __func__, ieee80211_acs_scan_evhandler, acs, status);
        scan_err("unable to allocate requester: status: %d", status);
        ret = -ENOMEM;
        goto err;
    }

    scan_params = (struct scan_start_request *) qdf_mem_malloc(sizeof(*scan_params));
    if (!scan_params) {
        ret = -ENOMEM;
        scan_err("unable to allocate scan request");
        goto err;
    }

    /* Fill scan parameter */
    status = wlan_update_scan_params(vap,scan_params,IEEE80211_M_HOSTAP,
            false,true,false,true,0,NULL,0);
    if (status) {
        qdf_mem_free(scan_params);
        ret = -EINVAL;
        scan_err("init ssid failed with status: %d", status);
        goto err;
    }

    scan_params->legacy_params.scan_type = SCAN_TYPE_FOREGROUND;
    scan_params->scan_req.scan_flags = 0;
    scan_params->scan_req.scan_f_passive = true;
    scan_params->legacy_params.min_dwell_passive = MIN_DWELL_TIME;
    scan_params->scan_req.dwell_time_passive = MAX_DWELL_TIME;
    scan_params->legacy_params.min_dwell_active = MIN_DWELL_TIME;
    scan_params->scan_req.dwell_time_active = MAX_DWELL_TIME;

    /* giving priority to user configured param when report is active  */
    if(acs->acs_scan_req_param.acs_scan_report_active) {
        if(acs->acs_scan_req_param.mindwell) {
            scan_params->legacy_params.min_dwell_active = acs->acs_scan_req_param.mindwell;
            scan_params->legacy_params.min_dwell_passive = acs->acs_scan_req_param.mindwell;
        }

        if(acs->acs_scan_req_param.maxdwell) {
            scan_params->scan_req.dwell_time_active = acs->acs_scan_req_param.maxdwell;
            scan_params->scan_req.dwell_time_passive = acs->acs_scan_req_param.maxdwell;
        }
        if(acs->acs_scan_req_param.rest_time) {
            /* rest time only valid for background scan */
            scan_params->scan_req.min_rest_time = acs->acs_scan_req_param.rest_time;
            scan_params->scan_req.max_rest_time = acs->acs_scan_req_param.rest_time;
            scan_params->legacy_params.scan_type = SCAN_TYPE_BACKGROUND;
        }
        if(acs->acs_scan_req_param.scan_mode) {
            /* Enabeling promise mode to get 11b stats */
            scan_params->scan_req.scan_f_promisc_mode = true;
        }
        if(acs->acs_scan_req_param.idle_time) {
            scan_params->scan_req.idle_time = acs->acs_scan_req_param.idle_time;
        }
        if(acs->acs_scan_req_param.max_scan_time) {
            scan_params->scan_req.max_scan_time  = acs->acs_scan_req_param.max_scan_time;
        }
    }

    /* If scan is invoked from ACS, Channel event notification should be
     * enabled  This is must for offload architecture
     */
    scan_params->scan_req.scan_f_chan_stat_evnt = true;

    switch (vap->iv_des_mode)
    {
        case IEEE80211_MODE_11A:
        case IEEE80211_MODE_TURBO_A:
        case IEEE80211_MODE_11NA_HT20:
        case IEEE80211_MODE_11NA_HT40PLUS:
        case IEEE80211_MODE_11NA_HT40MINUS:
        case IEEE80211_MODE_11NA_HT40:
        case IEEE80211_MODE_11AC_VHT20:
        case IEEE80211_MODE_11AC_VHT40PLUS:
        case IEEE80211_MODE_11AC_VHT40MINUS:
        case IEEE80211_MODE_11AC_VHT40:
        case IEEE80211_MODE_11AC_VHT80:
        case IEEE80211_MODE_11AC_VHT160:
        case IEEE80211_MODE_11AC_VHT80_80:
        case IEEE80211_MODE_11AXA_HE20:
        case IEEE80211_MODE_11AXA_HE40PLUS:
        case IEEE80211_MODE_11AXA_HE40MINUS:
        case IEEE80211_MODE_11AXA_HE40:
        case IEEE80211_MODE_11AXA_HE80:
        case IEEE80211_MODE_11AXA_HE160:
        case IEEE80211_MODE_11AXA_HE80_80:
            scan_params->scan_req.scan_f_5ghz = true;
            break;

        case IEEE80211_MODE_11B:
        case IEEE80211_MODE_11G:
        case IEEE80211_MODE_TURBO_G:
        case IEEE80211_MODE_11NG_HT20:
        case IEEE80211_MODE_11NG_HT40PLUS:
        case IEEE80211_MODE_11NG_HT40MINUS:
        case IEEE80211_MODE_11NG_HT40:
        case IEEE80211_MODE_11AXG_HE20:
        case IEEE80211_MODE_11AXG_HE40PLUS:
        case IEEE80211_MODE_11AXG_HE40MINUS:
        case IEEE80211_MODE_11AXG_HE40:
            scan_params->scan_req.scan_f_2ghz = true;
            break;

        default:
            if (acs->acs_scan_2ghz_only) {
                scan_params->scan_req.scan_f_2ghz = true;
            } else if (acs->acs_scan_5ghz_only) {
                scan_params->scan_req.scan_f_5ghz = true;
            } else {
                scan_params->scan_req.scan_f_2ghz = true;
                scan_params->scan_req.scan_f_5ghz = true;
            }
            break;
    }

    acs->acs_nchans_scan = 0;

    eacs_trace(EACS_DBG_DEFAULT,( " User channel count : %d \n", acs->acs_uchan_list.uchan_cnt));

    /* giving priority to user configured param when report is active  */
    if(acs->acs_uchan_list.uchan_cnt &&
            (acs->acs_scan_req_param.acs_scan_report_active)) {
        int i;
        u_int32_t alloc_count = (acs->acs_uchan_list.uchan_cnt > IEEE80211_ACS_CHAN_MAX) ?
            IEEE80211_ACS_CHAN_MAX : acs->acs_uchan_list.uchan_cnt;

        scan_channel_list = (u_int32_t *) OS_MALLOC(acs->acs_osdev,
                                 sizeof(u_int32_t) * alloc_count, 0);
        if (scan_channel_list == NULL) {
            qdf_mem_free(scan_params);
            ret = -ENOMEM;
            goto err2;
        }

        chan_list_allocated = true;

        /* Consrtruct the list of channels to be scanned.
           This also makes sure that when the scan report is requested,
           only the requested user channels are displayed.
         */

        for (ix = 0, i = 0; ix < alloc_count; ix++)
        {
            wlan_chan_t chan =
                ieee80211_find_dot11_channel(ic, acs->acs_uchan_list.uchan[ix], 0, IEEE80211_MODE_AUTO);

            if (!chan)
                continue;

            /*Allow the channels only supported by the radio*/
            if ((scan_params->scan_req.scan_f_2ghz && scan_params->scan_req.scan_f_5ghz) ||
                (scan_params->scan_req.scan_f_2ghz ? IEEE80211_IS_CHAN_2GHZ(chan) :
                (scan_params->scan_req.scan_f_5ghz ? IEEE80211_IS_CHAN_5GHZ(chan) : 0)))
            {
                *(scan_channel_list + i) = wlan_chan_to_freq(acs->acs_uchan_list.uchan[ix]);
                acs->acs_ieee_chan[i] = ieee80211_chan2ieee(ic, chan);
                i++;
            }

        }
        status = ucfg_scan_init_chanlist_params(scan_params,
                acs->acs_uchan_list.uchan_cnt, scan_channel_list, NULL);
        OS_FREE(scan_channel_list);

        acs->acs_nchans_scan = scan_params->scan_req.chan_list.num_chan;
        /* No need to issue a scan if there are no channels added to the scan list*/
        skip_scan = !acs->acs_nchans_scan;
    } else {
        /* If scan needs to be done on specific band or specific set of channels */
        if ((scan_params->scan_req.scan_f_5ghz == false) ||
                (scan_params->scan_req.scan_f_2ghz == false)) {
            wlan_chan_t *chans;
            u_int16_t nchans = 0;
            u_int8_t i;


            chans = (wlan_chan_t *)OS_MALLOC(acs->acs_osdev,
                            sizeof(wlan_chan_t) * IEEE80211_CHAN_MAX, 0);
            if (chans == NULL) {
                qdf_mem_free(scan_params);
                ret = -ENOMEM;
                goto err2;
            }
            /* HT20 only if IEEE80211_MODE_AUTO */

            if (scan_params->scan_req.scan_f_2ghz) {
                nchans = wlan_get_channel_list(ic, IEEE80211_MODE_11NG_HT20,
                                                chans, IEEE80211_CHAN_MAX);
                /* If no any HT channel available */
                if (nchans == 0) {
                     nchans = wlan_get_channel_list(ic, IEEE80211_MODE_11G,
                                                chans, IEEE80211_CHAN_MAX);
                }
            } else {
                nchans = wlan_get_channel_list(ic, IEEE80211_MODE_11NA_HT20,
                                                chans, IEEE80211_CHAN_MAX);
                /* If no any HT channel available */
                if (nchans == 0) {
                     nchans = wlan_get_channel_list(ic, IEEE80211_MODE_11A,
                                                chans, IEEE80211_CHAN_MAX);
                }
            }
            /* If still no channel available */
            if (nchans == 0) {
                nchans = wlan_get_channel_list(ic, IEEE80211_MODE_11B,
                                            chans, IEEE80211_CHAN_MAX);
            }


            chan_list = (u_int32_t *) OS_MALLOC(acs->acs_osdev, sizeof(uint32_t)*nchans, 0);

            if (chan_list == NULL) {
                qdf_mem_free(scan_params);
                OS_FREE(chans);
                ret = -EINVAL;
                goto err2;
            }

            chan_list_allocated = true;

            num_channels = 0;
            for (i = 0; i < nchans; i++)
            {
                const wlan_chan_t chan = chans[i];
                if ((scan_params->scan_req.scan_f_2ghz) ?
                    IEEE80211_IS_CHAN_2GHZ(chan) : IEEE80211_IS_CHAN_5GHZ(chan)) {
                    chan_list[num_channels++] = chan->ic_freq;
                    acs->acs_ieee_chan[i] = ieee80211_chan2ieee(ic, chan);
                 }
            }
            acs->acs_nchans_scan = nchans;
            status = ucfg_scan_init_chanlist_params(scan_params, num_channels,
                    chan_list, NULL);
            OS_FREE(chans);
        }
    }
    if (status) {
        qdf_mem_free(scan_params);
        ret = -EINVAL;
        scan_err("init chan list failed with status: %d", status);
        goto err2;
    }

    /* Try to issue a scan */
#if ATH_ACS_SUPPORT_SPECTRAL && WLAN_SPECTRAL_ENABLE
    /* For ACS spectral sample indication priority should be
       low (0) to indicate scan entries to mgmt layer */
    /* Only reporting is going on so no spectral */
    if(!acs->acs_scan_req_param.acs_scan_report_active) {
        ic->ic_start_spectral_scan(ic,0);
    }
#endif

    pdev = wlan_vdev_get_pdev(vdev);
    /*Flush scan table before starting scan */
    ucfg_scan_flush_results(pdev, NULL);

    ieee80211_acs_flush_olddata(acs);
    if (skip_scan || (ret = wlan_ucfg_scan_start(vap,
                    scan_params,
                    acs->acs_scan_requestor,
                    SCAN_PRIORITY_HIGH,
                    &(acs->acs_scan_id), 0, NULL)) != QDF_STATUS_SUCCESS) {

        eacs_trace(EACS_DBG_DEFAULT,( " Issue a scan fail."));
        scan_err("scan_start failed with status: %d", ret);
        ret = -EINVAL;
        goto err2;
    }
    acs->acs_startscantime = OS_GET_TIMESTAMP();
    goto end;

err2:
    scan_err("label err2");
    /* Since the scan didn't happen, clear the acs_scan_report_active flag */
    acs->acs_scan_req_param.acs_scan_report_active = false;
    ieee80211_acs_free_scan_resource(acs);
err:
    scan_err("label err");
    /* select the first available channel to start */
    if(!acs->acs_scan_req_param.acs_scan_report_active) {
        channel = ieee80211_find_dot11_channel(ic, 0, 0, vap->iv_des_mode);
        ieee80211_acs_post_event(acs, channel);
        eacs_trace(EACS_DBG_DEFAULT,( "Use the first available channel=%d to start"
                    , ieee80211_chan2ieee(ic, channel)));
    }

#if ATH_ACS_SUPPORT_SPECTRAL && WLAN_SPECTRAL_ENABLE
    /* Only reporting is going on so no spectral */
    if(!acs->acs_scan_req_param.acs_scan_report_active) {
        ic->ic_stop_spectral_scan(ic);
    }
#endif
    wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
    atomic_set(&acs->acs_in_progress,false);
    ch->ch_hop_triggered = false;
    if(chan_list_allocated == true)
        OS_FREE(chan_list);
    return ret;
end:
    wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_SCAN_ID);
    if(chan_list_allocated == true)
        OS_FREE(chan_list);
    return ret;
}

int ieee80211_acs_startscantime(struct ieee80211com *ic)
{
    ieee80211_acs_t acs = ic->ic_acs;
    return(acs->acs_startscantime);
}

int ieee80211_acs_state(struct ieee80211_acs *acs)
{
    if(acs->acs_startscantime == 0){

        /*
         * Scan never initiated
         */

        return EINVAL;
    }
   if(ieee80211_acs_in_progress(acs)) {

       /*
        * Scan is in progress
        */

       return EINPROGRESS;
   }
   else{

        /*
         * Scan has been completed
         */
        return EOK;

    }
    return -1;
}

static int ieee80211_acs_register_scantimer_handler(void *arg,
        ieee80211_acs_scantimer_handler evhandler,
        void                         *arg2)
{

    struct ieee80211com *ic = (struct ieee80211com *)arg;
    ieee80211_acs_t          acs = ic->ic_acs;

    eacs_trace(EACS_DBG_LVL1,("entry"));
    spin_lock(&acs->acs_lock);
    acs->acs_scantimer_handler= evhandler;
    acs->acs_scantimer_arg = arg2;
    spin_unlock(&acs->acs_lock);

    return EOK;
}
static int ieee80211_acs_register_event_handler(ieee80211_acs_t          acs,
        ieee80211_acs_event_handler evhandler,
        void                         *arg)
{
    int    i;

    for (i = 0; i < IEEE80211_MAX_ACS_EVENT_HANDLERS; ++i) {
        if ((acs->acs_event_handlers[i] == evhandler) &&
                (acs->acs_event_handler_arg[i] == arg)) {
            return EEXIST; /* already exists */
        }
    }

    if (acs->acs_num_handlers >= IEEE80211_MAX_ACS_EVENT_HANDLERS) {
        return ENOSPC;
    }

    spin_lock(&acs->acs_lock);
    acs->acs_event_handlers[acs->acs_num_handlers] = evhandler;
    acs->acs_event_handler_arg[acs->acs_num_handlers++] = arg;
    spin_unlock(&acs->acs_lock);

    return EOK;
}

static int ieee80211_acs_unregister_event_handler(ieee80211_acs_t          acs,
        ieee80211_acs_event_handler evhandler,
        void                         *arg)
{
    int    i;

    spin_lock(&acs->acs_lock);
    for (i = 0; i < IEEE80211_MAX_ACS_EVENT_HANDLERS; ++i) {
        if ((acs->acs_event_handlers[i] == evhandler) &&
                (acs->acs_event_handler_arg[i] == arg)) {
            /* replace event handler being deleted with the last one in the list */
            acs->acs_event_handlers[i]    = acs->acs_event_handlers[acs->acs_num_handlers - 1];
            acs->acs_event_handler_arg[i] = acs->acs_event_handler_arg[acs->acs_num_handlers - 1];

            /* clear last event handler in the list */
            acs->acs_event_handlers[acs->acs_num_handlers - 1]    = NULL;
            acs->acs_event_handler_arg[acs->acs_num_handlers - 1] = NULL;
            acs->acs_num_handlers--;

            spin_unlock(&acs->acs_lock);

            return EOK;
        }
    }
    spin_unlock(&acs->acs_lock);

    return ENXIO;

}

/**
 * @brief  Cancel the scan
 *
 * @param vap
 *
 * @return
 */
static int ieee80211_acs_cancel(struct ieee80211vap *vap)
{
    struct ieee80211com *ic;
    ieee80211_acs_t acs;

    /* If vap is NULL return here */
    if(vap == NULL) {
       return 0;
    }

    vap->iv_ic->ic_ht40intol_scan_running = 0;
    ic = vap->iv_ic;
    acs = ic->ic_acs;

    /* If ACS is not initiated from this vap, so
       don't unregister scan handlers */
    if(vap != acs->acs_vap) {
       return 0;
    }
    qdf_spin_lock(&acs->acs_ev_lock);
    /* If ACS is not in progress, then return, since the ACS handling must have completed before
     acquiring the lock*/
    if (!ieee80211_acs_in_progress(acs)) {
        qdf_spin_unlock(&acs->acs_ev_lock);
        return 0;
    }

       /* Unregister scan event handler */
    ieee80211_acs_free_scan_resource(acs);
    ieee80211_free_ht40intol_scan_resource(acs);
     /* Post ACS event with NULL channel to report cancel scan */
    ieee80211_acs_post_event(acs, IEEE80211_CHAN_ANYC);
     /*Reset ACS in progress flag */
    atomic_set(&acs->acs_in_progress,false);
    acs->acs_ch_hopping.ch_hop_triggered = false;
    qdf_spin_unlock(&acs->acs_ev_lock);
    return 1;
}


int wlan_autoselect_register_scantimer_handler(void * arg ,
        ieee80211_acs_scantimer_handler  evhandler,
        void                          *arg2)
{
    return ieee80211_acs_register_scantimer_handler(arg ,
            evhandler,
            arg2);
}


int wlan_autoselect_register_event_handler(wlan_if_t                    vaphandle,
        ieee80211_acs_event_handler evhandler,
        void                         *arg)
{
    return ieee80211_acs_register_event_handler(vaphandle->iv_ic->ic_acs,
            evhandler,
            arg);
}

int wlan_autoselect_unregister_event_handler(wlan_if_t                    vaphandle,
        ieee80211_acs_event_handler evhandler,
        void                         *arg)
{
    return ieee80211_acs_unregister_event_handler(vaphandle->iv_ic->ic_acs, evhandler, arg);
}

int wlan_autoselect_in_progress(wlan_if_t vaphandle)
{
    if (!vaphandle->iv_ic->ic_acs) return 0;
    return ieee80211_acs_in_progress(vaphandle->iv_ic->ic_acs);
}

int wlan_autoselect_find_infra_bss_channel(wlan_if_t vaphandle,
                                                 cfg80211_hostapd_acs_params *cfg_acs_params)
{
    return ieee80211_autoselect_infra_bss_channel(vaphandle, false /* is_scan_report */, cfg_acs_params);
}

int wlan_attempt_ht40_bss(wlan_if_t             vaphandle)
{
    return ieee80211_find_ht40intol_bss(vaphandle);
}

int wlan_autoselect_cancel_selection(wlan_if_t vaphandle)
{
   return ieee80211_acs_cancel(vaphandle);
}

int wlan_acs_find_best_channel(struct ieee80211vap *vap, int *bestfreq, int num)
{

    struct ieee80211com *ic = vap->iv_ic;
    struct ieee80211_ath_channel *best_11na = NULL;
    struct ieee80211_ath_channel *best_11ng = NULL;
    struct ieee80211_ath_channel *best_overall = NULL;
    int retv = 0,i=0;

    ieee80211_acs_t acs = ic->ic_acs;
    ieee80211_acs_t temp_acs;

    eacs_trace(EACS_DBG_LVL1,("entry"));

    temp_acs = (ieee80211_acs_t) OS_MALLOC(acs->acs_osdev, sizeof(struct ieee80211_acs), 0);

    if (temp_acs) {
        OS_MEMZERO(temp_acs, sizeof(struct ieee80211_acs));
    }
    else {
        qdf_print("%s: malloc failed ",__func__);
        return ENOMEM;
    }

    temp_acs->acs_ic = ic;
    temp_acs->acs_vap = vap;
    temp_acs->acs_osdev = acs->acs_osdev;
    temp_acs->acs_num_handlers = 0;
    atomic_set(&(temp_acs->acs_in_progress),true);
    temp_acs->acs_scan_2ghz_only = 0;
    temp_acs->acs_scan_5ghz_only = 0;
    temp_acs->acs_channel = NULL;
    temp_acs->acs_nchans = 0;



    ieee80211_acs_construct_chan_list(temp_acs,IEEE80211_MODE_AUTO);
    if (temp_acs->acs_nchans == 0) {
        eacs_trace(EACS_DBG_LVL0,( "Cannot construct the available channel list." ));
        retv = -1;
        goto err;
    }

    for(i=0; i<IEEE80211_CHAN_MAX ;i++) {
        acs->acs_chan_rssitotal[i]= 0;
        acs->acs_chan_rssi[i] = 0;
    }

    ucfg_scan_db_iterate(wlan_vap_get_pdev(temp_acs->acs_vap),
            ieee80211_acs_get_channel_maxrssi_n_secondary_ch, temp_acs);

    best_11na = ieee80211_acs_find_best_11na_centerchan(temp_acs);
    best_11ng = ieee80211_acs_find_best_11ng_centerchan(temp_acs);

    if (temp_acs->acs_minrssi_11na > temp_acs->acs_minrssisum_11ng) {
        best_overall = best_11ng;
    } else {
        best_overall = best_11na;
    }

    if( best_11na==NULL || best_11ng==NULL || best_overall==NULL) {
        eacs_trace(EACS_DBG_LVL1, ("  channel "));
        retv = -1;
        goto err;
    }

    bestfreq[0] = (int) best_11na->ic_freq;
    bestfreq[1] = (int) best_11ng->ic_freq;
    bestfreq[2] = (int) best_overall->ic_freq;

err:
    OS_FREE(temp_acs);
    return retv;

}
/*
 * Update NF and Channel Load upon WMI event
 */

#define MIN_CLEAR_CNT_DIFF 1000
void ieee80211_acs_stats_update(ieee80211_acs_t acs,
        u_int8_t flags,
        u_int ieee_chan_num,
        int16_t noisefloor,
        struct ieee80211_chan_stats *chan_stats)
{
    u_int32_t temp = 0,cycles_cnt = 0;
    u_int32_t now = (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    struct ieee80211com *ic = NULL;
    struct ol_ath_softc_net80211 *scn = NULL;
    bool is_wrap_around = false;

    eacs_trace(EACS_DBG_LVL1,(": %d.%03d",  now / 1000, now % 1000));
    if (acs == NULL) {
        eacs_trace(EACS_DBG_LVL0,("ERROR: ACS is not initialized"));
        return;
    }

    if (ieee_chan_num == 0) {
        eacs_trace(EACS_DBG_LVL0,("ERROR: Invalid channel 0"));
        return;
    }

    if (!ieee80211_acs_in_progress(acs)) {
        eacs_trace(EACS_DBG_LVL0,("ERROR: ACS not in progress \n"));
        acs->acs_run_incomplete = true;
        return;
    }

    if (flags == ACS_CHAN_STATS_NF) {
        /* Ensure we received ACS_CHAN_STATS event before for same channel */
        if ((acs->acs_last_evt.event != ACS_CHAN_STATS) ||
                (acs->acs_last_evt.ieee_chan != ieee_chan_num)) {
            eacs_trace(EACS_DBG_LVL0,("ERROR: Received STATS_NF without STATS_DIFF\n"));
            eacs_trace(EACS_DBG_LVL0,("Previous chan: %d, event: %d\n", ieee_chan_num, flags));
            return;
        }
    }
    ic = acs->acs_ic;
    scn = OL_ATH_SOFTC_NET80211(ic);

    if (flags == ACS_CHAN_STATS_DIFF) {

        acs->acs_cycle_count[ieee_chan_num] = chan_stats->cycle_cnt;
        acs->acs_chan_load[ieee_chan_num] = chan_stats->chan_clr_cnt;
        acs->acs_80211_b_duration[ieee_chan_num] = chan_stats->duration_11b_data;

        eacs_trace(EACS_DBG_CHLOAD ,("CH:%d Diff Clr cnt %u cycle cnt %u diff %u ",
                    ieee_chan_num,chan_stats->chan_clr_cnt,chan_stats->cycle_cnt ,
                    acs->acs_chan_load[ieee_chan_num]));

    } else if (flags == ACS_CHAN_STATS_NF) {

        acs->acs_last_evt.event = ACS_CHAN_STATS_NF;
        acs->acs_last_evt.ieee_chan = ieee_chan_num;

        /* if initial counters are not recorded, return */
        if (acs->acs_cycle_count[ieee_chan_num] == 0)  {
            eacs_trace(EACS_DBG_LVL0,
                ("NF and Channel Load can't be updated"));
            return;
        }

        /* For Beeliner family of chipsets, hardware counters cycle_cnt and chan_clr_cnt wrap
         * around independently. After reaching max value of 0xffffffff they become 0x7fffffff.
         *
         * For Peregrine and Direct attach chipsets, once cycle_cnt reaches 0xffffffff, both
         * cycle_cnt and chan_clr_cnt are right shifted by one bit. Because of this right
         * shifting we can't calculate correct channel utilization when wrap around happens.
         */

        if (acs->acs_cycle_count[ieee_chan_num] > chan_stats->cycle_cnt) {
            is_wrap_around = true;
            acs->acs_cycle_count[ieee_chan_num] = (MAX_32BIT_UNSIGNED_VALUE - acs->acs_cycle_count[ieee_chan_num])
                + (chan_stats->cycle_cnt - (MAX_32BIT_UNSIGNED_VALUE >> 1));
        } else {
            acs->acs_cycle_count[ieee_chan_num] = chan_stats->cycle_cnt - acs->acs_cycle_count[ieee_chan_num] ;
        }

        if (ic->ic_is_mode_offload(ic) && ic->ic_is_target_ar900b(ic)) {
            /* Beeliner family */
            if (acs->acs_chan_load[ieee_chan_num] > chan_stats->chan_clr_cnt) {
                is_wrap_around = true;
                acs->acs_chan_load[ieee_chan_num] = (MAX_32BIT_UNSIGNED_VALUE - acs->acs_chan_load[ieee_chan_num])
                    + (chan_stats->chan_clr_cnt - (MAX_32BIT_UNSIGNED_VALUE >> 1));
            } else {
                acs->acs_chan_load[ieee_chan_num] = chan_stats->chan_clr_cnt - acs->acs_chan_load[ieee_chan_num] ;
            }
        } else {
            /* Peregrine and Diretc attach family */
            if (acs->acs_cycle_count[ieee_chan_num] > chan_stats->cycle_cnt) {
                is_wrap_around = true;
                acs->acs_chan_load[ieee_chan_num] = chan_stats->chan_clr_cnt - (acs->acs_chan_load[ieee_chan_num] >> 1);
            } else {
                acs->acs_chan_load[ieee_chan_num] = chan_stats->chan_clr_cnt - acs->acs_chan_load[ieee_chan_num] ;
            }
        }

        if (acs->acs_80211_b_duration[ieee_chan_num] > chan_stats->duration_11b_data) {
             acs->acs_80211_b_duration[ieee_chan_num] = (MAX_32BIT_UNSIGNED_VALUE - acs->acs_80211_b_duration[ieee_chan_num])
                                                      + chan_stats->duration_11b_data;
        } else {
             acs->acs_80211_b_duration[ieee_chan_num] = chan_stats->duration_11b_data - acs->acs_80211_b_duration[ieee_chan_num] ;
        }

        eacs_trace(EACS_DBG_CHLOAD ,("CH:%d End Clr cnt %u cycle cnt %u diff %u ",
                    ieee_chan_num,chan_stats->chan_clr_cnt,chan_stats->cycle_cnt ,
                    acs->acs_chan_load[ieee_chan_num]));

    } else if (flags == ACS_CHAN_STATS) {
        eacs_trace(EACS_DBG_CHLOAD ,("CH:%d Initial chan_clr_cnt %u cycle_cnt %u ",
                    ieee_chan_num,      chan_stats->chan_clr_cnt,chan_stats->cycle_cnt));
        acs->acs_last_evt.event = ACS_CHAN_STATS;
        acs->acs_last_evt.ieee_chan = ieee_chan_num;

        acs->acs_chan_load[ieee_chan_num] = chan_stats->chan_clr_cnt;
        acs->acs_cycle_count[ieee_chan_num] = chan_stats->cycle_cnt;
        acs->acs_80211_b_duration[ieee_chan_num] = chan_stats->duration_11b_data;

#if ATH_ACS_SUPPORT_SPECTRAL && WLAN_SPECTRAL_ENABLE
        /*If spectral ACS stats are not reset on channel change, they will be reset here */
        ieee80211_init_spectral_chan_loading(acs->acs_ic,
                ieee80211_ieee2mhz(acs->acs_ic, ieee_chan_num, 0),0);
#endif
    }

    if ((flags == ACS_CHAN_STATS_DIFF) || (flags == ACS_CHAN_STATS_NF)) {
        if (ieee_chan_num != IEEE80211_CHAN_ANY) {
            acs->acs_noisefloor[ieee_chan_num] = noisefloor;
            eacs_trace(EACS_DBG_NF,("Noise floor chan %d  NF %d ",ieee_chan_num,noisefloor));
        }

        if((chan_stats->chan_tx_power_range > 0) || (chan_stats->chan_tx_power_tput > 0)) {
            if(acs->acs_tx_power_type == ACS_TX_POWER_OPTION_TPUT) {
               acs->acs_chan_regpower[ieee_chan_num] = chan_stats->chan_tx_power_tput;
            } else if(acs->acs_tx_power_type == ACS_TX_POWER_OPTION_RANGE)  {
               acs->acs_chan_regpower[ieee_chan_num] = chan_stats->chan_tx_power_range;
            }
        }

        /* make sure when new clr_cnt is more than old clr cnt, ch utilization is non-zero */
        if ((acs->acs_chan_load[ieee_chan_num] > MIN_CLEAR_CNT_DIFF) &&
             (acs->acs_cycle_count[ieee_chan_num] != 0)){
            temp = (u_int32_t)(acs->acs_chan_load[ieee_chan_num]);
            cycles_cnt = (u_int32_t) acs->acs_cycle_count[ieee_chan_num]/100;/*divide it instead multiply temp */
            if (!cycles_cnt) {
                cycles_cnt = 1;
            }
            temp = (u_int32_t)(temp/cycles_cnt);
            /* Some results greater than 100% have been seen. The reason for
             * this is unknown, so for now just floor the results to 100.
             */
            acs->acs_chan_load[ieee_chan_num] = MIN(MAX( 1,temp), 100);
            eacs_trace(EACS_DBG_CHLOAD, ("CH:%u Ch load %u ",ieee_chan_num,acs->acs_chan_load[ieee_chan_num]));
        } else {
            eacs_trace(EACS_DBG_CHLOAD,("CH:%u Ch load  %u(diff is less then min) or cycle count is zero ",ieee_chan_num,acs->acs_chan_load[ieee_chan_num]));
            acs->acs_chan_load[ieee_chan_num] = 0;
        }
        if (is_wrap_around) {
            if ((acs->acs_chan_load[ieee_chan_num] >= 100) &&
                (acs->acs_last_evt.last_util.ieee_chan == ieee_chan_num)) {
                eacs_trace(EACS_DBG_CHLOAD,("wrap_around happened. CH: %d, calculated util: %d, Restoring to %d",
                        ieee_chan_num, acs->acs_chan_load[ieee_chan_num], acs->acs_last_evt.last_util.ch_util));
                acs->acs_chan_load[ieee_chan_num] = acs->acs_last_evt.last_util.ch_util;
            }
        } else {
            acs->acs_last_evt.last_util.ch_util = acs->acs_chan_load[ieee_chan_num];
            acs->acs_last_evt.last_util.ieee_chan = ieee_chan_num;
        }

#if ATH_ACS_SUPPORT_SPECTRAL && WLAN_SPECTRAL_ENABLE
        acs->acs_channel_loading[ieee_chan_num] = get_eacs_control_duty_cycle(acs);
#endif
    }
}

int wlan_acs_get_user_chanlist(wlan_if_t vaphandle, u_int8_t *chanlist)
{
    struct ieee80211vap *vap = vaphandle;
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_acs_t acs = ic->ic_acs;
    u_int32_t i=0;

    for(i=0;i<acs->acs_uchan_list.uchan_cnt;i++)
    {
        chanlist[i] = acs->acs_uchan_list.uchan[i];
    }
    return i;
}

int wlan_acs_set_user_chanlist(struct ieee80211vap *vap, u_int8_t *extra)
{
#define APPEND_LIST 0xff
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_acs_t acs = ic->ic_acs;
    u_int32_t i = 0;
    int *ptr = NULL,append = 0,dup = false;

    if(*extra == APPEND_LIST) {
        /*append list*/
        ptr = (int *)(&(acs->acs_uchan_list.uchan[acs->acs_uchan_list.uchan_cnt]));
        extra++; /*Trim apend byte */
        append = 1;
    }
    else {
        /*Flush list and start copying */
        OS_MEMZERO(acs->acs_uchan_list.uchan,IEEE80211_ACS_CHAN_MAX);
        ptr =(int *)(&(acs->acs_uchan_list.uchan[0]));
        acs->acs_uchan_list.uchan_cnt = 0;
    }

    while(*extra) {
        if(append) /*duplicate detection */
        {
            for(i = 0;i < acs->acs_uchan_list.uchan_cnt; i++) {
                if(*extra == acs->acs_uchan_list.uchan[i]) {
                    dup = true;
                    extra++;
                    break;
                } else {
                    dup = false;
                }
            }
        }
        if(!dup) {
            *ptr++ = *extra++;
            acs->acs_uchan_list.uchan_cnt++;
        }
    }
#undef APPEND_LIST
    return 0;
}


/**
 * @brief channel change for channel hopping
 *
 * @param vap
 */
static void ieee80211_acs_channel_hopping_change_channel(struct ieee80211vap *vap)
{
    struct ieee80211com *ic;
    ieee80211_acs_t acs;
    struct acs_ch_hopping_t *ch;

    ASSERT(vap);
    ic = vap->iv_ic;
    acs = ic->ic_acs;

    if(acs == NULL)
        return;

    ch = &(acs->acs_ch_hopping);

    if (ch->ch_hop_triggered)
        return ;  /* channel change is already active  */

    spin_lock(&ic->ic_lock);
    ch->ch_hop_triggered = true; /* To bail out acs in multivap scenarios */
    spin_unlock(&ic->ic_lock);

    TAILQ_FOREACH(vap, &ic->ic_vaps, iv_next) {
        if (vap->iv_opmode == IEEE80211_M_HOSTAP) {
            if (!wlan_set_channel(vap, IEEE80211_CHAN_ANY, 0)) {
                /* ACS is done on per radio, so calling it once is
                 * good enough
                 */
                goto done;
            }
        }
    }
done:
    return;
}

/**
 * @brief timer to keep track of noise detection
 *
 * @param ieee80211_ch_cntwin_timer
 */
static OS_TIMER_FUNC(ieee80211_ch_cntwin_timer)
{
    struct ieee80211com *ic = NULL;
    ieee80211_acs_t acs = NULL;
    struct acs_ch_hopping_t *ch = NULL;
    struct ieee80211_ath_channel *channel = NULL,*ichan = NULL;
    struct ieee80211vap *vap = NULL;
    int32_t i = 0,flag = 0,retval = 0, val = 0;
    u_int16_t cur_freq = 0;

    OS_GET_TIMER_ARG(ic, struct ieee80211com *);

    ASSERT(ic);

    acs = ic->ic_acs;

    if(acs == NULL)
        return;

    ch = &(acs->acs_ch_hopping);

    vap = acs->acs_vap;

    if(vap == NULL)
        return;

    if(ieee80211_vap_deleted_is_clear(vap)) {
        /*Stopping noise detection To collect stats */
        val = false;
        retval = acs_noise_detection_param(ic,true,IEEE80211_ENABLE_NOISE_DETECTION,&val);

        if(retval == EOK) {
            retval = acs_noise_detection_param(ic,false,IEEE80211_GET_COUNTER_VALUE,&val);
            if(val > acs->acs_ch_hopping.param.cnt_thresh)
            {
                if (ieee80211_acs_channel_is_set(vap)) {
                    channel =  vap->iv_des_chan[vap->iv_des_mode];
                    if(channel) {
                        cur_freq = ieee80211_chan2freq(ic,channel);
                        ieee80211_enumerate_channels(ichan, ic, i)
                        {
                            if (cur_freq == ieee80211_chan2freq(ic, ichan))
                            {
                                eacs_trace(EACS_DBG_LVL0,("%d  ",ieee80211_chan2freq(ic, ichan)));
                                IEEE80211_CH_HOPPING_SET_CHAN_BLOCKED(ichan);
                                flag = true;
                            }
                        }
                    }
                }
            }
        } /*retval == EOK*/
        else
            return;


        if(flag) {
            acs->acs_ch_hopping.ch_max_hop_cnt++;
            ieee80211_acs_channel_hopping_change_channel(vap);
            return; /*Donot fire timer */
        }

        if(acs->acs_ch_hopping.ch_max_hop_cnt < ACS_CH_HOPPING_MAX_HOP_COUNT ) { /*Three hops are enough */
            /*Resting time over should enable noise detection now */
            val = true;
            retval = acs_noise_detection_param(ic,true,IEEE80211_ENABLE_NOISE_DETECTION,&val);
            if(EOK == retval ) {
                /*Restarting noise detection again */
                OS_SET_TIMER(&ch->ch_cntwin_timer, SEC_TO_MSEC(ch->param.cnt_dur)); /*in sec */
            }
        }
    }/*vap deleted is clear */
    else {
        return; /*Do not fire timer return */
    }
}

/**
 * @brief Long duration timer for keeping track of history
 *
 * @param ieee80211_ch_long_timer
 */
static OS_TIMER_FUNC(ieee80211_ch_long_timer)
{
    struct ieee80211com *ic = NULL;
    ieee80211_acs_t acs = NULL;
    struct acs_ch_hopping_t *ch = NULL;
    struct ieee80211_ath_channel *ichan = NULL;
    int i=0,retval = 0;

    OS_GET_TIMER_ARG(ic, struct ieee80211com *);

    ASSERT(ic);

    acs = ic->ic_acs;

    if(acs == NULL) /*vap delete may be in process */
        return;

    ch = &(acs->acs_ch_hopping);
    ieee80211_enumerate_channels(ichan, ic, i) {
        IEEE80211_CH_HOPPING_CLEAR_CHAN_BLOCKED(ichan);
    }

    if(acs->acs_ch_hopping.ch_max_hop_cnt >= ACS_CH_HOPPING_MAX_HOP_COUNT) {

        /*Restarting noise detection again */
        i = true;
        retval = acs_noise_detection_param(ic,true,IEEE80211_ENABLE_NOISE_DETECTION,&i);
        if(retval == EOK)
            OS_SET_TIMER(&ch->ch_cntwin_timer, SEC_TO_MSEC(ch->param.cnt_dur)); /*in sec */
    }

    acs->acs_ch_hopping.ch_max_hop_cnt = 0;
    eacs_trace(EACS_DBG_LVL0,("line %d LONG DURATION  EXPIRES SET itself  ",__LINE__));
    if(retval == EOK )
        OS_SET_TIMER(&ch->ch_long_timer, SEC_TO_MSEC(ch->param.long_dur)); /*in sec */
}

/**
 * @brief NO hopping timer , cannot change channel till it is active
 *
 * @param ieee80211_ch_nohop_timer
 */
static OS_TIMER_FUNC(ieee80211_ch_nohop_timer)
{
    struct ieee80211com *ic = NULL;
    int val = true,retval = 0;
    ieee80211_acs_t acs = NULL;
    struct acs_ch_hopping_t *ch = NULL;

    OS_GET_TIMER_ARG(ic, struct ieee80211com *);
    ASSERT(ic);

    acs = ic->ic_acs;
    ch = &(acs->acs_ch_hopping);
    eacs_trace(EACS_DBG_LVL0,("line %d NOHOP EXPIRES SET CNTWIN ",__LINE__));
    acs->acs_ch_hopping.ch_nohop_timer_active = false;
    retval = acs_noise_detection_param(ic,true,IEEE80211_ENABLE_NOISE_DETECTION,&val);

    if(retval == EOK)
        OS_SET_TIMER(&ch->ch_cntwin_timer, SEC_TO_MSEC(ch->param.cnt_dur)); /*in sec */
    /* Any thing for else ?*/
    return;
}

/**
 * @brief Setting long duration timer from user land
 *
 * @param acs
 * @param val
 *
 * @return Zero in case of succes
 */
int ieee80211_acs_ch_long_dur(ieee80211_acs_t acs,int val)
{
    struct acs_ch_hopping_t *ch = &(acs->acs_ch_hopping);
    /* Long duration  in minutes */
    if(val)
    {
        if(val < ch->param.nohop_dur)
            return EINVAL;

        /* start timer */
        ch->param.long_dur = val;
        OS_SET_TIMER(&ch->ch_long_timer, SEC_TO_MSEC(ch->param.long_dur)); /*in sec */
    } else {
        /* stop timer */
        OS_CANCEL_TIMER(&ch->ch_long_timer);
    }
    return EOK;
}

/**
 * @brief No hopping timer triggered from user land
 *
 * @param acs
 * @param val
 *
 * @return EOK if successfull
 */

int ieee80211_acs_ch_nohop_dur(ieee80211_acs_t acs,int val)
{
    struct acs_ch_hopping_t *ch = &(acs->acs_ch_hopping);

    /*channel hopping in seconds */
    if(val)
    {
        /* Do not restart timer,its for
           next evalutaion of no hopping */
        ch->param.nohop_dur = val;
    } else {
        /* stop timer */
        OS_CANCEL_TIMER(&ch->ch_nohop_timer);
    }
    return EOK;
}

/**
 * @brief api from os specific files
 *
 * @param vap
 * @param param
 * @param cmd
 * @param val
 *
 * @return EOK in case of success
 */
int wlan_acs_param_ch_hopping(wlan_if_t vap, int param, int cmd,int *val)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_acs_t acs = ic->ic_acs;
    int error = EOK,retval = EOK;
#define NOISE_FLOOR_MAX -60
#define NOISE_FLOOR_MIN -128
#define MAX_COUNTER_THRESH  100
    switch (param)
    {
        case true: /*SET */
            switch(cmd)
            {
                case IEEE80211_ACS_ENABLE_CH_HOP:
                    if(*val) {
                        ieee80211com_set_cap_ext(ic,IEEE80211_ACS_CHANNEL_HOPPING);
                        /*See if we want to init timer used in attached function */
                    }else {
                        ieee80211com_clear_cap_ext (ic,IEEE80211_ACS_CHANNEL_HOPPING);
                    }
                    break;
                case IEEE80211_ACS_CH_HOP_LONG_DUR:
                    ieee80211_acs_ch_long_dur(acs,*val);
                    break;
                case IEEE80211_ACS_CH_HOP_NO_HOP_DUR:
                    ieee80211_acs_ch_nohop_dur(acs,*val);
                    break;
                case IEEE80211_ACS_CH_HOP_CNT_WIN_DUR:
                    if(*val) {
                        acs->acs_ch_hopping.param.cnt_dur = *val;
                        if( acs->acs_ch_hopping.ch_nohop_timer_active == false) {
                            /*Timer not working*/
                            OS_CANCEL_TIMER(&acs->acs_ch_hopping.ch_cntwin_timer);
                            OS_SET_TIMER(&(acs->acs_ch_hopping.ch_cntwin_timer),
                                    SEC_TO_MSEC(acs->acs_ch_hopping.param.cnt_dur));
                        } else {
                            error = -EINVAL;
                        }

                    }
                    break;
                case IEEE80211_ACS_CH_HOP_NOISE_TH:
                    if((*val > NOISE_FLOOR_MIN) && (*val < NOISE_FLOOR_MAX)) {
                        acs->acs_ch_hopping.param.noise_thresh = *val;
                        retval = acs_noise_detection_param(ic,true,IEEE80211_NOISE_THRESHOLD,
                                &acs->acs_ch_hopping.param.noise_thresh);
                        if((acs->acs_ch_hopping.ch_nohop_timer_active == false)
                                && EOK == retval) {
                            /*Timer not working*/
                            OS_CANCEL_TIMER(&acs->acs_ch_hopping.ch_cntwin_timer);
                            OS_SET_TIMER(&(acs->acs_ch_hopping.ch_cntwin_timer),
                                    SEC_TO_MSEC(acs->acs_ch_hopping.param.cnt_dur));
                        }
                    } else {
                        error = -EINVAL;
                    }
                    break;
                case IEEE80211_ACS_CH_HOP_CNT_TH:
                    if(*val && *val <= MAX_COUNTER_THRESH) /*value is in percentage */
                        acs->acs_ch_hopping.param.cnt_thresh = *val;
                    else {
                        error = -EINVAL;
                    }
                    break;
                default:
                    eacs_trace(EACS_DBG_LVL0,
                            ("line %d should bot happen INVESTIGATE ",
                             __LINE__));
                    error = -EINVAL;
                    break;
            }
            break;
        case false: /*GET */
            switch(cmd)
            {
                case IEEE80211_ACS_ENABLE_CH_HOP:
                    *val = ieee80211com_has_cap_ext(ic,IEEE80211_ACS_CHANNEL_HOPPING);
                    break;
                case IEEE80211_ACS_CH_HOP_LONG_DUR:
                    *val = acs->acs_ch_hopping.param.long_dur;
                    break;
                case IEEE80211_ACS_CH_HOP_NO_HOP_DUR:
                    *val = acs->acs_ch_hopping.param.nohop_dur;
                    break;
                case IEEE80211_ACS_CH_HOP_CNT_WIN_DUR:
                    *val = acs->acs_ch_hopping.param.cnt_dur;
                    break;
                case IEEE80211_ACS_CH_HOP_NOISE_TH:
                    *val = acs->acs_ch_hopping.param.noise_thresh;
                    break;

                case IEEE80211_ACS_CH_HOP_CNT_TH:
                    *val = acs->acs_ch_hopping.param.cnt_thresh;
                    break;

                default:
                    eacs_trace(EACS_DBG_LVL0,("line %d should bot happen INVESTIGATE ",__LINE__));
                    error = -EINVAL;
                    break;
            }
            break;
        default:
            eacs_trace(EACS_DBG_LVL0,("line %d should bot happen INVESTIGATE ",__LINE__));
                error = -EINVAL;
                break;
    }
#undef NOISE_FLOOR_MAX
#undef NOISE_FLOOR_MIN
#undef MAX_COUNTER_THRESH
    return error;
}

int ieee80211_check_and_execute_pending_acsreport(wlan_if_t vap)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_acs_t acs = ic->ic_acs;
    u_int32_t status = EOK;

    if(true == acs->acs_scan_req_param.acs_scan_report_pending) {
        status = ieee80211_autoselect_infra_bss_channel(vap, true /* is_scan_report */, NULL);
        if(status!= EOK) {
            printk("line %d Acs is active ,Can not execute acs report.\n",
                    __LINE__);
        }
    }
    acs->acs_scan_req_param.acs_scan_report_pending = false;
    return status;
}

/**
 * @brief to start acs scan report
 *
 * @param vap
 * @param set
 * @param cmd
 * @param val
 *
 * @return EOK in case of success
 */
int wlan_acs_start_scan_report(wlan_if_t vap, int set, int cmd, void *val)
{
/* XXX tunables */
/* values used in scan state machines  HW_DEFAULT_REPEAT_PROBE_REQUEST_INTVAL*/
#define IEEE80211_MIN_DWELL 50
#define IEEE80211_MAX_DWELL 10000 /* values used in scan state machines in msec */
#define CHANNEL_LOAD_REQUESTED 2
    struct ieee80211com *ic = vap->iv_ic;
    unsigned int status = EOK;
    ieee80211_acs_t acs = ic->ic_acs;
    eacs_trace(EACS_DBG_LVL0,("Invoking ACS module for ACS report "));
    if(set) {
        switch(cmd)
        {
            case IEEE80211_START_ACS_REPORT:
                if(*((u_int32_t *)val))
                {
                    status = ieee80211_autoselect_infra_bss_channel(vap, true /* is_scan_report */, NULL);

                    if(status!= EOK) {
                        /* Bypassing failure for direct attach hardware as for direct
                           attach we are taking channel load directly from hardware
                           for failure cases */
                        if( *((u_int32_t *)val) != CHANNEL_LOAD_REQUESTED) {
                            acs->acs_scan_req_param.acs_scan_report_pending = true;
                            eacs_trace(EACS_DBG_LVL0,("Acs is active ,acs report will be processed after acs is done \n"));
                        }
                    }
                    return status;
                }
                else
                    return -EINVAL;
                break;
            case IEEE80211_MIN_DWELL_ACS_REPORT:
                if( *((u_int32_t *)val) > IEEE80211_MIN_DWELL)
                {
                    acs->acs_scan_req_param.mindwell =  *((u_int32_t *)val);
                    return EOK;
                } else {
                    eacs_trace(EACS_DBG_LVL0,("Mindwell time must be greater than %dms ",
                                            IEEE80211_MIN_DWELL));
                    return -EINVAL;
                }
                break;
            case IEEE80211_MAX_DWELL_ACS_REPORT:
                if( *((u_int32_t *)val) < IEEE80211_MAX_DWELL)
                {
                    if(acs->acs_scan_req_param.mindwell) {
                        if( *((u_int32_t *)val) < acs->acs_scan_req_param.mindwell) {
                            eacs_trace(EACS_DBG_LVL0,("max dwell time %d less than min dwell %d\n",
						     *((u_int32_t *)val), acs->acs_scan_req_param.mindwell));
                            return -EINVAL;
                        }
                    }
                    acs->acs_scan_req_param.maxdwell =  *((u_int32_t *)val);
                    return EOK;
                } else {
                    eacs_trace(EACS_DBG_LVL0,("max dwell time %d greater than %d\n",  *((u_int32_t *)val),
					    IEEE80211_MAX_DWELL));
                    return -EINVAL;
                }
                break;
             case IEEE80211_MAX_SCAN_TIME_ACS_REPORT:
                if ( *((u_int32_t *)val) >= 0) {
                    acs->acs_scan_req_param.max_scan_time =  *((u_int32_t *)val);
                    return EOK;
                } else {
                    return -EINVAL;
                }
                break;
#if QCA_LTEU_SUPPORT
             case IEEE80211_SCAN_IDLE_TIME:
                if (*((u_int32_t *)val) >= 0) {
                    acs->acs_scan_req_param.idle_time = *((u_int32_t *)val);
                    return EOK;
                } else {
                    return -EINVAL;
                }
#endif
                break;
            case IEEE80211_SCAN_REST_TIME:
                if ( *((u_int32_t *)val) >= 0) {
                    acs->acs_scan_req_param.rest_time =  *((u_int32_t *)val);
                    return EOK;
                } else {
                    return -EINVAL;
                }
                break;
            case IEEE80211_SCAN_MODE:
                if ((*((u_int8_t *)val) == IEEE80211_SCAN_PASSIVE)
                        || (*((u_int8_t *)val) == IEEE80211_SCAN_ACTIVE)) {
                    acs->acs_scan_req_param.scan_mode = *((u_int8_t *)val);
                    return EOK;
                } else {
                    return -EINVAL;
                }
                break;
            default :
                IEEE80211_DPRINTF(acs->acs_vap, IEEE80211_MSG_ACS,"Invalid parameter");
                return -EINVAL;
        }
    } else /*get part */
    {
        switch(cmd) {
            case IEEE80211_START_ACS_REPORT:
                *((u_int32_t *)val) = (u_int32_t) acs->acs_scan_req_param.acs_scan_report_active;;
                break;
            case IEEE80211_MIN_DWELL_ACS_REPORT:
                *((u_int32_t *)val) = (u_int32_t) acs->acs_scan_req_param.mindwell;
                break;
            case IEEE80211_MAX_DWELL_ACS_REPORT:
                *((u_int32_t *)val) = (u_int32_t) acs->acs_scan_req_param.maxdwell;
                break;
            case IEEE80211_MAX_SCAN_TIME_ACS_REPORT:
                *((u_int32_t *)val) = (u_int32_t) acs->acs_scan_req_param.max_scan_time;
                break;
#if QCA_LTEU_SUPPORT
            case IEEE80211_SCAN_IDLE_TIME:
                *((u_int32_t *)val) = (u_int32_t)acs->acs_scan_req_param.idle_time;
                break;
#endif
            default :
                IEEE80211_DPRINTF(acs->acs_vap, IEEE80211_MSG_ACS,"Invalid parameter");
                return -EINVAL;
        }
        return EOK;
    }
#undef IEEE80211_MIN_DWELL
#undef IEEE80211_MAX_DWELL
#undef CHANNEL_LOAD_REQUESTED
}

/**
 * @brief: Generates EACS report on request
 *
 * @param ic,
 *
 * @param acs_r, entry of debug stats
 *
 * @param internal, to know whether report is requested by EACS or other module
 *
 */
int ieee80211_acs_scan_report(struct ieee80211com *ic, struct ieee80211vap *vap, struct ieee80211_acs_dbg *acs_report, u_int8_t internal)
{
    ieee80211_acs_t acs = ic->ic_acs;
    struct ieee80211_ath_channel *channel = NULL;
    u_int8_t i, ieee_chan;
    u_int16_t nchans;
    ieee80211_chan_neighbor_list *acs_neighbor_list = NULL;
    int status = 0;
    struct ieee80211_acs_dbg *acs_r = NULL;
    uint8_t  acs_entry_id = 0;
    ACS_LIST_TYPE acs_type = 0;
    ieee80211_neighbor_info *neighbour_list;
    uint32_t neighbour_size;
    uint8_t nbss_allocated;
    int error;

    error = __xcopy_from_user(&acs_entry_id, &acs_report->entry_id, sizeof(acs_report->entry_id));
    if (error) {
        eacs_trace(EACS_DBG_LVL0, ("copy_from_user failed"));
        return -EFAULT;
    }

    error = __xcopy_from_user(&acs_type, &acs_report->acs_type, sizeof(acs_report->acs_type));
    if (error) {
        eacs_trace(EACS_DBG_LVL0, ("copy_from_user failed"));
        return -EFAULT;
    }

    acs_r = (struct ieee80211_acs_dbg *) qdf_mem_malloc(sizeof(*acs_r));
    if (!acs_r) {
        qdf_info("%s: Failed to allocate memory ",__func__);
        return -ENOMEM;
    }

    if(ieee80211_acs_in_progress(acs) && !internal) {
        qdf_info("%s: ACS scan is in progress, Please request for the report after a while... ",__func__);
        acs_r->nchans = 0;
        goto end;
    }

    if(acs->acs_run_incomplete) {
        eacs_trace(EACS_DBG_LVL0,("ACS run is cancelled, could be due to ICM or someother reason "));
        acs_r->nchans = 0;
        goto end;
    }

    nchans = (acs->acs_nchans_scan)?(acs->acs_nchans_scan):(acs->acs_nchans);

    /* For ACS Ranking, we need only the channels which have been analysed when
     * selecting the best channel */
    if (acs->acs_ranking) {
        nchans = acs->acs_nchans;
    }

    i = acs_entry_id;
    if(i >= nchans) {
        acs_r->nchans = 0;
        status = -EINVAL;
        goto end;
    }

    acs_r->nchans = nchans;
    acs_r->acs_status = acs->acs_status;
    /* If scan channel list is not generated by ACS,
       acs_chans[i] have all channels */
    if(acs->acs_nchans_scan == 0) {
        channel = acs->acs_chans[i];
        ieee_chan = ieee80211_chan2ieee(acs->acs_ic, channel);
        acs_r->chan_freq = ieee80211_chan2freq(acs->acs_ic, channel);
    } else {
        ieee_chan = acs->acs_ieee_chan[i];
        acs_r->chan_freq = ieee80211_ieee2mhz(ic, ieee_chan,0);
    }

    /* For ACS Ranking, we need only the channels which have been analysed when
     * selecting the best channel */
    if (acs->acs_ranking) {
        channel = acs->acs_chans[i];
        ieee_chan = ieee80211_chan2ieee(acs->acs_ic, channel);
        acs_r->chan_freq = ieee80211_chan2freq(acs->acs_ic, channel);
    }

    if (acs_type == ACS_CHAN_STATS) {
        acs_r->ieee_chan = ieee_chan;
        acs_r->chan_nbss = acs->acs_chan_nbss[ieee_chan];
        acs_r->chan_maxrssi = acs->acs_chan_maxrssi[ieee_chan];
        acs_r->chan_minrssi = acs->acs_chan_minrssi[ieee_chan];
        acs_r->noisefloor = acs->acs_noisefloor[ieee_chan];
        acs_r->channel_loading = 0;   /*Spectral dependency from ACS is removed*/
        acs_r->chan_load = acs->acs_chan_load[ieee_chan];
        acs_r->sec_chan = acs->acs_sec_chan[ieee_chan];
        acs_r->chan_80211_b_duration = acs->acs_80211_b_duration[ieee_chan];
        acs_r->chan_nbss_srp = acs->acs_srp_supported[ieee_chan];
        acs_r->chan_srp_load = acs->acs_srp_load[ieee_chan];
        if (acs->acs_chans[i])
            acs_r->chan_radar_noise = IEEE80211_IS_CHAN_RADAR(acs->acs_chans[i]);
        acs_r->chan_in_pool = (acs_r->chan_radar_noise) ? 0 : 1;

        /* For ACS channel Ranking, copy the rank and description */
        if (acs->acs_ranking) {
            acs_r->acs_rank.rank = acs->acs_rank[ieee_chan].rank;
            memcpy(acs_r->acs_rank.desc, acs->acs_rank[ieee_chan].desc, ACS_RANK_DESC_LEN);
        }

        if (copy_to_user(acs_report, acs_r, sizeof(*acs_r))) {
            eacs_trace(EACS_DBG_LVL0,("copy_to_user failed "));
        }
    }

    if (acs_type == ACS_NEIGHBOUR_GET_LIST_COUNT || acs_type == ACS_NEIGHBOUR_GET_LIST) {

        acs_neighbor_list = (ieee80211_chan_neighbor_list *) qdf_mem_malloc(sizeof(ieee80211_chan_neighbor_list));
        if (!acs_neighbor_list) {
            qdf_info("%s: malloc failed \n",__func__);
            status = -ENOMEM;
            goto end;
        }

        OS_MEMZERO(acs_neighbor_list, sizeof(ieee80211_chan_neighbor_list));

        neighbour_list = (ieee80211_neighbor_info *) qdf_mem_malloc(sizeof(ieee80211_neighbor_info) * IEEE80211_MAX_NEIGHBOURS);
        if (!neighbour_list) {
            OS_FREE(acs_neighbor_list);
            qdf_info("malloc failed\n");
            status = -ENOMEM;
            goto end;
        }

        OS_MEMZERO(neighbour_list, sizeof(ieee80211_neighbor_info) * IEEE80211_MAX_NEIGHBOURS);
        neighbour_size = sizeof(ieee80211_neighbor_info) * IEEE80211_MAX_NEIGHBOURS;

        acs_neighbor_list->acs = acs;
        acs_neighbor_list->chan = ieee_chan;
        acs_neighbor_list->neighbor_list = neighbour_list;
        acs_neighbor_list->neighbor_size = neighbour_size;

        ucfg_scan_db_iterate(wlan_vap_get_pdev(vap),
                ieee80211_get_chan_neighbor_list, (void *)acs_neighbor_list);

        if (copy_to_user(&acs_report->chan_nbss, &acs_neighbor_list->nbss, sizeof(acs_neighbor_list->nbss))) {
            qdf_info("Copy to user failed\n");
            status = -EFAULT;
            goto clean_list;
        }

        if (copy_to_user(&acs_report->ieee_chan, &acs_neighbor_list->chan, sizeof(acs_neighbor_list->chan))) {
            qdf_info("Copy to user failed\n");
            status = -EFAULT;
            goto clean_list;
        }

        if (acs_type == ACS_NEIGHBOUR_GET_LIST) {
            error = __xcopy_from_user(&nbss_allocated, &acs_report->chan_nbss, sizeof(acs_report->chan_nbss));
            if (error) {
                qdf_info("copy_from_user failed");
                status = -EFAULT;
                goto clean_list;
            }

            if (copy_to_user(acs_report->neighbor_list, acs_neighbor_list->neighbor_list, sizeof(ieee80211_neighbor_info) * nbss_allocated)) {
                qdf_info("Copy to user failed\n");
                status = -EFAULT;
                goto clean_list;
            }
        }

clean_list:
        OS_FREE(acs_neighbor_list);
        OS_FREE(neighbour_list);
    }

end:
    qdf_mem_free(acs_r);
    return status;
}

/**
 * @brief: Displays ACS statistics
 *
 * @param ic
 *
 */
static void ieee80211_acs_scan_report_internal(struct ieee80211com *ic)
{
    struct ieee80211_acs_dbg *acs_dbg;
    ieee80211_acs_t acs = ic->ic_acs;
    u_int8_t i;

    acs_dbg = (struct ieee80211_acs_dbg *) OS_MALLOC(acs->acs_osdev,
                        sizeof(struct ieee80211_acs_dbg), 0);

    if (acs_dbg) {
        OS_MEMZERO(acs_dbg, sizeof(struct ieee80211_acs_dbg));
    }
    else {
        qdf_print("%s: malloc failed ",__func__);
        return;
    }

    /* output the current configuration */
    i = 0;
    do {
        acs_dbg->entry_id = i;
        acs_dbg->acs_type = ACS_CHAN_STATS;

        ieee80211_acs_scan_report(ic, acs->acs_vap, acs_dbg, 1);
        if((acs_dbg->nchans) && (i == 0)) {
            printk("******** ACS report ******** \n");
            printk(" Channel | BSS  | minrssi | maxrssi | NF | Ch load | spect load | sec_chan | SR bss | SR load \n");
            printk("-----------------------------------------------------------------------------------------------\n");
        } else if(acs_dbg->nchans == 0) {
            printk("Failed to print ACS scan report\n");
            break;
        }
        /*To make sure we are not getting more than 100 %*/
        if(acs_dbg->chan_load  > 100)
            acs_dbg->chan_load = 100;

        printk(" %4d(%3d) %4d     %4d      %4d   %4d    %4d        %4d       %4d     %4d      %4d   \n",
                acs_dbg->chan_freq,
                acs_dbg->ieee_chan,
                acs_dbg->chan_nbss,
                acs_dbg->chan_minrssi,
                acs_dbg->chan_maxrssi,
                acs_dbg->noisefloor,
                acs_dbg->chan_load,
                acs_dbg->channel_loading,
                acs_dbg->sec_chan,
                acs_dbg->chan_nbss_srp,
                acs_dbg->chan_srp_load);

       i++;
    } while(i < acs_dbg->nchans);
    OS_FREE(acs_dbg);

}

/**
 * @brief: Invoke EACS report handler, it acts as inteface for other modules
 *
 * @param vap,
 *
 * @param acs_rp, entry of debug stats
 *
 */
int wlan_acs_scan_report(wlan_if_t vaphandle,void *acs_rp)
{
    struct ieee80211_acs_dbg *acs_r = (struct ieee80211_acs_dbg *)acs_rp;
    return ieee80211_acs_scan_report(vaphandle->iv_ic, vaphandle, acs_r, 0);
}


/**
 * @brief api to set blocked channel list
 *
 * @param vap
 * @param param :- channel to block
 * @param number of channels
 * @return status code
 */
int wlan_acs_block_channel_list(wlan_if_t vap,u_int8_t *chan,u_int8_t nchan)
{
#define FLUSH_LIST 0
    int i = 0;
    acs_bchan_list_t *bchan = NULL;
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_acs_t acs = ic->ic_acs;

    if(NULL == chan)
        return -ENOSPC;

    bchan = &(acs->acs_bchan_list);

    if(FLUSH_LIST == chan[0])
    {
        OS_MEMZERO(&(bchan->uchan[0]),bchan->uchan_cnt);
        bchan->uchan_cnt = 0;

        /* Re-populating the CFG blocked channels (if any) since these channels
         * are not intended to be flushed unless directly removed from CFG  */
        if (ic->ic_pri20_cfg_blockchanlist.n_chan) {
            wlan_acs_block_channel_list(vap,
                                        ic->ic_pri20_cfg_blockchanlist.chan,
                                        ic->ic_pri20_cfg_blockchanlist.n_chan);
        }

        return 0;
    }
    else {
        while(i < nchan) {
            if (acs_is_channel_blocked(acs, chan[i])) {
                /* Channel is already present in the list */
                qdf_debug("chan %u is already in the list", chan[i]);
                i++;
                continue;
            }

            bchan->uchan[bchan->uchan_cnt] = chan[i];
            bchan->uchan_cnt++;
            i++;
        }
    }

    bchan->uchan_cnt %= IEEE80211_CHAN_MAX;
    return 0;
#undef FLUSH_LIST
}

#if ATH_CHANNEL_BLOCKING
struct ieee80211_ath_channel *
wlan_acs_channel_allowed(wlan_if_t vap, struct ieee80211_ath_channel *c, enum ieee80211_phymode mode)
{
    struct ieee80211com *ic = vap->iv_ic;
    ieee80211_acs_t acs = ic->ic_acs;
    struct ieee80211_ath_channel *channel;
    int i, j, n_modes, extchan, blocked;
    struct ieee80211_ath_channel_list chan_info;
    enum ieee80211_phymode modes[IEEE80211_MODE_MAX];

    if (!(acs->acs_block_mode & ACS_BLOCK_MANUAL))
        return c;

    /* real phymode */
    if (mode == IEEE80211_MODE_AUTO) {
        modes[0] = IEEE80211_MODE_11AXA_HE160;
        modes[1] = IEEE80211_MODE_11AXA_HE80;
        modes[2] = IEEE80211_MODE_11AXA_HE40PLUS;
        modes[3] = IEEE80211_MODE_11AXA_HE40MINUS;
        modes[4] = IEEE80211_MODE_11AXA_HE20;
        modes[5] = IEEE80211_MODE_11AC_VHT160;
        modes[6] = IEEE80211_MODE_11AC_VHT80;
        modes[7] = IEEE80211_MODE_11AC_VHT40PLUS;
        modes[8] = IEEE80211_MODE_11AC_VHT40MINUS;
        modes[9] = IEEE80211_MODE_11AC_VHT20;
        modes[10] = IEEE80211_MODE_11AXG_HE40PLUS;
        modes[11] = IEEE80211_MODE_11AXG_HE40MINUS;
        modes[13] = IEEE80211_MODE_11NG_HT40PLUS;
        modes[14] = IEEE80211_MODE_11NG_HT40MINUS;
        modes[15] = IEEE80211_MODE_11NA_HT40PLUS;
        modes[16] = IEEE80211_MODE_11NA_HT40MINUS;
        modes[12] = IEEE80211_MODE_11AXG_HE20;
        modes[17] = IEEE80211_MODE_11NG_HT20;
        modes[18] = IEEE80211_MODE_11NA_HT20;
        modes[19] = IEEE80211_MODE_11G;
        modes[20] = IEEE80211_MODE_11A;
        modes[21] = IEEE80211_MODE_11B;
        n_modes = 22;

    } else if (mode == IEEE80211_MODE_11NG_HT40) {
        modes[0] = IEEE80211_MODE_11NG_HT40PLUS;
        modes[1] = IEEE80211_MODE_11NG_HT40MINUS;
        n_modes = 2;
    } else if (mode == IEEE80211_MODE_11NA_HT40) {
        modes[0] = IEEE80211_MODE_11NA_HT40PLUS;
        modes[1] = IEEE80211_MODE_11NA_HT40MINUS;
        n_modes = 2;
    } else if (mode == IEEE80211_MODE_11AC_VHT40) {
        modes[0] = IEEE80211_MODE_11AC_VHT40PLUS;
        modes[1] = IEEE80211_MODE_11AC_VHT40MINUS;
        n_modes = 2;
    } else {
        modes[0] = mode;
        n_modes = 1;
    }

    for (j = 0; j < n_modes; j++) {
        blocked = 0;
        ieee80211_enumerate_channels(channel, ic, i) {
            u_int8_t ieee_chan_num = ieee80211_chan2ieee(ic, channel);
            u_int8_t ieee_ext_chan_num;

            if (ieee80211_chan2mode(channel) != modes[j]) {
                eacs_trace(EACS_DBG_LVL0, ("channel %d mode %d not equal mode %d",
                            ieee_chan_num, ieee80211_chan2mode(channel), modes[j]));
                continue; /* next channel */
            }

            if (ieee_chan_num != ieee80211_chan2ieee(ic, c)) {
                eacs_trace(EACS_DBG_LVL0, ("channel %d mode %d not equal "
                            "channel %d", ieee_chan_num, modes[j], ieee80211_chan2ieee(ic, c)));
                continue; /* next channel */
            }

            /* channel is blocked as per user setting */
            if (acs_is_channel_blocked(acs, ieee_chan_num)) {
                eacs_trace(EACS_DBG_BLOCK, ("Channel %d is blocked, in mode %d", ieee_chan_num, modes[j]));
                blocked = 1;
                break; /* next phymode */
            }

            ieee80211_get_extchaninfo(ic, channel, &chan_info);

            for (extchan = 0; extchan < chan_info.cl_nchans; extchan++) {
                if (chan_info.cl_channels[extchan] == NULL) {
                    if(ieee80211_is_extchan_144(acs->acs_ic,channel, extchan)) {
                         continue;
                    }
                    eacs_trace(EACS_DBG_LVL0, ("Rejecting Channel for ext channel found null "
                                "channel number %d EXT channel %d found %pK", ieee_chan_num, extchan,
                                chan_info.cl_channels[extchan]));
                    blocked = 1;
                    break;
                }

                /* extension channel (in NAHT40/ACVHT40/ACVHT80 mode) is blocked as per user setting */
                if (acs->acs_block_mode & ACS_BLOCK_EXTENSION) {
                    ieee_ext_chan_num = ieee80211_chan2ieee(ic, chan_info.cl_channels[extchan]);
                    if (acs_is_channel_blocked(acs, ieee_ext_chan_num)) {
                        eacs_trace(EACS_DBG_BLOCK, ("Channel %d can't be used, as ext channel %d "
                                    "is blocked, in mode %d", ieee_chan_num, ieee_ext_chan_num, modes[j]));
                        blocked = 1;
                        break;
                    }
                }
            }

            if (blocked)
                break; /* next phymode */

            /* in 2.4GHz band checking channels overlapping with primary,
             * or if required with secondary too (NGHT40 mode) */
            if ((acs->acs_block_mode & ACS_BLOCK_EXTENSION) && IEEE80211_IS_CHAN_2GHZ(channel)) {
                struct ieee80211_ath_channel *ext_chan;
                int start = channel->ic_freq - 15, end = channel->ic_freq + 15, f;
                if (IEEE80211_IS_CHAN_11NG_HT40PLUS(channel))
                    end = channel->ic_freq + 35;
                if (IEEE80211_IS_CHAN_11NG_HT40MINUS(channel))
                    start = channel->ic_freq - 35;
                for (f = start; f <= end; f += 5) {
                    ext_chan = ic->ic_find_channel(ic, f, 0, IEEE80211_CHAN_B);
                    if (ext_chan) {
                        ieee_ext_chan_num = ieee80211_chan2ieee(ic, ext_chan);
                        if (acs_is_channel_blocked(acs, ieee_ext_chan_num)) {
                            eacs_trace(EACS_DBG_BLOCK, ("Channel %d can't be used, as ext "
                                        "or overlapping channel %d is blocked, in mode %d",
                                        ieee_chan_num, ieee_ext_chan_num, modes[j]));
                            blocked = 1;
                            break;
                        }
                    }
                }
            }

            if (blocked)
                break; /* next phymode */

            /* channel is allowed in this phymode */
            if (c != channel) {
                eacs_trace(EACS_DBG_BLOCK, ("channel %d phymode %d instead of channel %d phymode %d",
                            ieee80211_chan2ieee(ic, channel), ieee80211_chan2mode(channel),
                            ieee80211_chan2ieee(ic, c), ieee80211_chan2mode(c)));
            }
            return channel;
        }
    }

    return NULL;
}
#endif

#if ATH_ACS_SUPPORT_SPECTRAL && WLAN_SPECTRAL_ENABLE

/*
 * Function     : update_eacs_avg_rssi
 * Description  : Update average RSSI
 * Input        : Pointer to acs struct
                   nfc_ctl_rssi  - control chan rssi
                   nfc_ext_rssi  - extension chan rssi
 * Output       : Noisefloor
 *
 */
static void update_eacs_avg_rssi(ieee80211_acs_t acs, int8_t nfc_ctl_rssi, int8_t nfc_ext_rssi)
{
    int temp=0;

    if(acs->acs_ic->ic_cwm_get_width(acs->acs_ic) == IEEE80211_CWM_WIDTH40) {
        // HT40 mode
        temp = (acs->ext_eacs_avg_rssi * (acs->ext_eacs_spectral_reports));
        temp += nfc_ext_rssi;
        acs->ext_eacs_spectral_reports++;
        acs->ext_eacs_avg_rssi = (temp / acs->ext_eacs_spectral_reports);
    }


    temp = (acs->ctl_eacs_avg_rssi * (acs->ctl_eacs_spectral_reports));
    temp += nfc_ctl_rssi;

    acs->ctl_eacs_spectral_reports++;
    acs->ctl_eacs_avg_rssi = (temp / acs->ctl_eacs_spectral_reports);
}


/*
 * Function     : update EACS Thresholds
 * Description  : Updates EACS Thresholds
 * Input        : Pointer to acs struct
                  ctl_nf - control chan nf
                  ext_nf  - extension chan rs
 * Output       : void
 *
 */
void update_eacs_thresholds(ieee80211_acs_t acs,int8_t ctrl_nf,int8_t ext_nf)
{

    acs->ctl_eacs_rssi_thresh = ctrl_nf + 10;

    if (acs->acs_ic->ic_cwm_get_width(acs->acs_ic) == IEEE80211_CWM_WIDTH40) {
        acs->ext_eacs_rssi_thresh = ext_nf + 10;
    }

    acs->ctl_eacs_rssi_thresh = 32;
    acs->ext_eacs_rssi_thresh = 32;
}

/*
 * Function     : ieee80211_update_eacs_counters
 * Description  : Update EACS counters
 * Input        : Pointer to ic struct
                  nfc_ctl_rssi  - control chan rssi
                  nfc_ext_rssi  - extension chan rssi
                  ctl_nf - control chan nf
                  ext_nf  - extension chan rs
 * Output       : Noisefloor
 *
 */
void ieee80211_update_eacs_counters(struct ieee80211com *ic, int8_t nfc_ctl_rssi,
        int8_t nfc_ext_rssi,int8_t ctrl_nf, int8_t ext_nf)
{
    ieee80211_acs_t acs = ic->ic_acs;


    if(!ieee80211_acs_in_progress(acs)) {
        return;
    }
    acs->eacs_this_scan_spectral_data++;
    update_eacs_thresholds(acs,ctrl_nf,ext_nf);
    update_eacs_avg_rssi(acs, nfc_ctl_rssi, nfc_ext_rssi);

    if (ic->ic_cwm_get_width(ic) == IEEE80211_CWM_WIDTH40) {
        // HT40 mode
        if (nfc_ext_rssi > acs->ext_eacs_rssi_thresh){
            acs->ext_eacs_interf_count++;
        }
        acs->ext_eacs_duty_cycle=((acs->ext_eacs_interf_count * 100)/acs->eacs_this_scan_spectral_data);
    }
    if (nfc_ctl_rssi > acs->ctl_eacs_rssi_thresh){
        acs->ctl_eacs_interf_count++;
    }

    acs->ctl_eacs_duty_cycle=((acs->ctl_eacs_interf_count * 100)/acs->eacs_this_scan_spectral_data);
#if DEBUG_EACS
    if(acs->ctl_eacs_interf_count > acs->eacs_this_scan_spectral_data) {
        printk("eacs interf count is greater than spectral samples %d:%d \n",
                acs->ctl_eacs_interf_count,acs->eacs_this_scan_spectral_data);
    }
#endif
}

/*
 * Function     : get_eacs_control_duty_cycle
 * Description  : Get EACS control duty cycle
 * Input        : Pointer to acs Struct
 * Output       : Duty Cycle
 *
 */
int get_eacs_control_duty_cycle(ieee80211_acs_t acs)
{
    return acs->ctl_eacs_duty_cycle;
}

/* Function     : get_eacs_extension_duty_cycle
 * Description  : Get EACS extension duty cycle
 * Input        : Pointer to  acs struct
 * Output       : Duty Cycle
 *
 */
int get_eacs_extension_duty_cycle(ieee80211_acs_t acs)
{
    return acs->ext_eacs_duty_cycle;
}

/*
 * Function     : print_chan_loading_details
 * Description  : Debug function to print channel loading information
 * Input        : Pointer to ic
 * Output       : void
 *
 */
static void print_chan_loading_details(struct ieee80211com *ic)
{
    ieee80211_acs_t acs = ic->ic_acs;
    printk("ctl_chan_freq       = %d\n",acs->ctl_chan_frequency);
    printk("ctl_interf_count    = %d\n",acs->ctl_eacs_interf_count);
    printk("ctl_duty_cycle      = %d\n",acs->ctl_eacs_duty_cycle);
    printk("ctl_chan_loading    = %d\n",acs->ctl_chan_loading);
    printk("ctl_nf              = %d\n",acs->ctl_chan_noise_floor);

    if (ic->ic_cwm_get_width(ic) == IEEE80211_CWM_WIDTH40) {
        // HT40 mode
        printk("ext_chan_freq       = %d\n",acs->ext_chan_frequency);
        printk("ext_interf_count    = %d\n",acs->ext_eacs_interf_count);
        printk("ext_duty_cycle      = %d\n",acs->ext_eacs_duty_cycle);
        printk("ext_chan_loading    = %d\n",acs->ext_chan_loading);
        printk("ext_nf              = %d\n",acs->ext_chan_noise_floor);
    }

    printk("%s this_scan_spectral_data count = %d\n", __func__, acs->eacs_this_scan_spectral_data);
}



/*
 * Function     : ieee80211_init_spectral_chan_loading
 * Description  : Initializes Channel loading information
 * Input        : Pointer to ic
                  current_channel - control channel freq
                  ext_channel - extension channel freq
 * Output       : void
 *
 */
void ieee80211_init_spectral_chan_loading(struct ieee80211com *ic,
        int current_channel, int ext_channel)
{

    ieee80211_acs_t acs = ic->ic_acs;

    if(!ieee80211_acs_in_progress(acs)) {
        return;
    }
    if ((current_channel == 0) && (ext_channel == 0)) {
        return;
    }
    /* Check if channel change has happened in this reset */
    if(acs->ctl_chan_frequency != current_channel) {
        /* If needed, check the channel loading details
        * print_chan_loading_details(spectral);
         */
        acs->eacs_this_scan_spectral_data = 0;

        if (ic->ic_cwm_get_width(ic) == IEEE80211_CWM_WIDTH40) {
            // HT40 mode
            acs->ext_eacs_interf_count = 0;
            acs->ext_eacs_duty_cycle   = 0;
            acs->ext_chan_loading      = 0;
            acs->ext_chan_noise_floor  = 0;
            acs->ext_chan_frequency    = ext_channel;
        }

        acs->ctl_eacs_interf_count = 0;
        acs->ctl_eacs_duty_cycle   = 0;
        acs->ctl_chan_loading      = 0;
        acs->ctl_chan_noise_floor  = 0;
        acs->ctl_chan_frequency    = current_channel;

        acs->ctl_eacs_rssi_thresh = SPECTRAL_EACS_RSSI_THRESH;
        acs->ext_eacs_rssi_thresh = SPECTRAL_EACS_RSSI_THRESH;

        acs->ctl_eacs_avg_rssi = SPECTRAL_EACS_RSSI_THRESH;
        acs->ext_eacs_avg_rssi = SPECTRAL_EACS_RSSI_THRESH;

        acs->ctl_eacs_spectral_reports = 0;
        acs->ext_eacs_spectral_reports = 0;

    }

}

/*
 * Function     : ieee80211_get_spectral_freq_loading
 * Description  : Get EACS control duty cycle
 * Input        : Pointer to ic
 * Output       : Duty Cycle
 *
 */

int ieee80211_get_spectral_freq_loading(struct ieee80211com *ic)
{
    ieee80211_acs_t acs = ic->ic_acs;
    int duty_cycles;
    duty_cycles = ((acs->ctl_eacs_duty_cycle > acs->ext_eacs_duty_cycle) ?
            acs->ctl_eacs_duty_cycle :
            acs->ext_eacs_duty_cycle);
    return duty_cycles;
}
#endif

/* List of function APIs used by other modules(like CBS) to control ACS */
int ieee80211_acs_api_prepare(struct ieee80211vap *vap, ieee80211_acs_t acs, enum ieee80211_phymode mode,
                              uint32_t *chan_list, uint32_t *nchans)
{
    struct ieee80211com *ic = acs->acs_ic;
    wlan_chan_t *chans;
    u_int8_t i;
    u_int32_t num_chans = 0;

    *nchans = 0;

    if (OS_ATOMIC_CMPXCHG(&(acs->acs_in_progress),false,true)) {
        /* Just wait for acs done */
        return -EINPROGRESS;
    }
    acs->acs_vap = vap;
    ieee80211_acs_construct_chan_list(acs, mode);
    if (acs->acs_nchans == 0) {
        eacs_trace(EACS_DBG_DEFAULT,( " Cannot construct the available channel list." ));
        return -EINVAL;
    }

#if ATH_SUPPORT_VOW_DCS
    /* update dcs information */
    if (ic->cw_inter_found && ic->ic_curchan) {
        acs->acs_intr_status[ieee80211_chan2ieee(ic, ic->ic_curchan)] += 1;
        acs->acs_intr_ts[ieee80211_chan2ieee(ic, ic->ic_curchan)] =
            (u_int32_t) CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());
    }
#endif

    if (acs->acs_uchan_list.uchan_cnt) {
        *nchans = acs->acs_uchan_list.uchan_cnt;

        for (i = 0; i < *nchans; i++) {
            chan_list[i] = acs->acs_uchan_list.uchan[i];
        }
    }
    else {
        chans = (wlan_chan_t *)OS_MALLOC(acs->acs_osdev,
                                  sizeof(wlan_chan_t) * IEEE80211_CHAN_MAX, GFP_KERNEL);
        if (chans == NULL)
            return -ENOMEM;
        num_chans = wlan_get_channel_list(ic, IEEE80211_MODE_11NG_HT20,
                                          chans, IEEE80211_CHAN_MAX);
        *nchans += num_chans;
        num_chans = wlan_get_channel_list(ic, IEEE80211_MODE_11NA_HT20,
                                          chans, IEEE80211_CHAN_MAX);
        *nchans += num_chans;

        /* try non-HT channels */
        if (*nchans == 0) {
            num_chans = wlan_get_channel_list(ic, IEEE80211_MODE_11G,
                                              chans, IEEE80211_CHAN_MAX);
            *nchans += num_chans;
            num_chans = wlan_get_channel_list(ic, IEEE80211_MODE_11A,
                                              chans, IEEE80211_CHAN_MAX);
            *nchans += num_chans;
        }
        /* If still no channel available */
        if (*nchans == 0) {
            num_chans = wlan_get_channel_list(ic, IEEE80211_MODE_11B,
                                              chans, IEEE80211_CHAN_MAX);
            *nchans += num_chans;
        }

        if (*nchans == 0) {
            /* no channels to scan */
            return -EINVAL;
        }
        for (i = 0; i < *nchans; i++) {
            chan_list[i] = chans[i]->ic_freq;
            acs->acs_ieee_chan[i] = ieee80211_chan2ieee(ic, chans[i]);
        }

        acs->acs_nchans_scan = *nchans;
        OS_FREE(chans);
    }
    return EOK;
}

int ieee80211_acs_api_rank(ieee80211_acs_t acs, u_int8_t top)
{
    ieee80211_acs_update_sec_chan_rssi(acs);
    ieee80211_acs_rank_channels(acs, top);
    return EOK;
}

int ieee80211_acs_api_complete(ieee80211_acs_t acs)
{
    if (!OS_ATOMIC_CMPXCHG(&(acs->acs_in_progress),true,false))
    {
        eacs_trace(EACS_DBG_LVL0,("Wrong locking in ACS --investigate -- \n"));
        eacs_trace(EACS_DBG_LVL0,(" %s \n",__func__));
        atomic_set(&(acs->acs_in_progress),false);
    }
    return EOK;
}

int ieee80211_acs_api_flush(struct ieee80211vap *vap)
{
    ieee80211_acs_t acs = vap->iv_ic->ic_acs;
    struct wlan_objmgr_vdev *vdev = NULL;
    struct wlan_objmgr_pdev *pdev = NULL;
    vdev = vap->vdev_obj;
    pdev = wlan_vdev_get_pdev(vdev);

    /*Flush scan table before starting scan */
    ucfg_scan_flush_results(pdev, NULL);
    ieee80211_acs_flush_olddata(acs);
    acs->acs_startscantime = OS_GET_TIMESTAMP();

    return EOK;
}

int ieee80211_acs_api_update(struct ieee80211com *ic, enum scan_event_type type, uint32_t freq)
{
    ieee80211_acs_retrieve_chan_info(ic, type, freq);

    return EOK;
}

int ieee80211_acs_api_get_ranked_chan(ieee80211_acs_t acs, int rank)
{
    int i;

    for (i = 0; i < IEEE80211_ACS_CHAN_MAX; i++) {
        if (acs->acs_rank[i].rank == rank)
            break;
    }
    if (i == IEEE80211_ACS_CHAN_MAX) {
        return 0;
    }
    qdf_print("****ranked chan %d %d", rank, i);
    return i;
}

int ieee80211_acs_api_resttime(struct ieee80211_acs *acs)
{
    return acs->acs_scan_req_param.rest_time;
}

int ieee80211_acs_api_dwelltime(struct ieee80211_acs *acs)
{
    return acs->acs_scan_req_param.mindwell;
}


