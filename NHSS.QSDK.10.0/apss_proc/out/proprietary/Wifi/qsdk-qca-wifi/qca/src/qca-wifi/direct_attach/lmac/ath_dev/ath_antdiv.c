/*
 * Copyright (c) 2009, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * Copyright (c) 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
 *
 */

#include "ath_internal.h"
#include "ath_antdiv.h"

#ifndef REMOVE_PKT_LOG
#include "pktlog.h"
extern struct ath_pktlog_funcs *g_pktlog_funcs;
#define MAX_LOG 128
#endif

/*
 * Conditional compilation to avoid errors if ATH_SLOW_ANT_DIV is not defined
 */
#if ATH_SLOW_ANT_DIV

void
ath_slow_ant_div_init(struct ath_antdiv *antdiv, struct ath_softc *sc, int32_t rssitrig,
                      u_int32_t min_dwell_time, u_int32_t settle_time)
{
    int    default_antenna;

    spin_lock_init(&(antdiv->antdiv_lock));

    antdiv->antdiv_sc             = sc;
    antdiv->antdiv_rssitrig       = rssitrig;
    antdiv->antdiv_min_dwell_time = min_dwell_time * 1000;
    antdiv->antdiv_settle_time    = settle_time * 1000;

    if (! ath_hal_get_config_param(sc->sc_ah, HAL_CONFIG_DEFAULTANTENNASET, &default_antenna))
    {
        default_antenna = 0;
    }
    antdiv->antdiv_def_antcfg = default_antenna;
	DPRINTF(sc, sc->sc_reg_parm.showAntDivDbgInfo, "%s:slowAntDivEnabled, DefaultAntenna=%d, AntDivMinDwellTime=%d ms, AntDivSettleTime=%d ms\n",
		__func__, antdiv->antdiv_def_antcfg, antdiv->antdiv_min_dwell_time, antdiv->antdiv_settle_time);
}

void
ath_slow_ant_div_start(struct ath_antdiv *antdiv, u_int8_t num_antcfg, const u_int8_t *bssid)
{
    struct ath_softc *sc = antdiv->antdiv_sc;
    spin_lock(&antdiv->antdiv_lock);

    antdiv->antdiv_num_antcfg   = num_antcfg < ATH_ANT_DIV_MAX_CFG ? num_antcfg : ATH_ANT_DIV_MAX_CFG;
    antdiv->antdiv_state        = ATH_ANT_DIV_IDLE;
    antdiv->antdiv_curcfg       = 0;
    antdiv->antdiv_bestcfg      = 0;
    antdiv->antdiv_laststatetsf = 0;
    antdiv->antdiv_suspend      = 0;

    OS_MEMCPY(antdiv->antdiv_bssid, bssid, sizeof(antdiv->antdiv_bssid));

    antdiv->antdiv_start = 1;
    DPRINTF(sc, sc->sc_reg_parm.showAntDivDbgInfo, "%s: antdiv_num_antcfg=%d,  antdiv_state=idle\n", __func__, antdiv->antdiv_num_antcfg);

    spin_unlock(&antdiv->antdiv_lock);
}

void
ath_slow_ant_div_stop(struct ath_antdiv *antdiv)
{
       struct ath_softc *sc = antdiv->antdiv_sc;
	spin_lock(&antdiv->antdiv_lock);

    /*
     * Lose suspend/resume state if Antenna Diversity is disabled.
     */
    antdiv->antdiv_suspend = 0;
    antdiv->antdiv_start   = 0;
    DPRINTF(sc, sc->sc_reg_parm.showAntDivDbgInfo, "%s\n", __func__);

    spin_unlock(&antdiv->antdiv_lock);
}

void
ath_slow_ant_div_suspend(ath_dev_t dev)
{
    struct ath_softc    *sc     = ATH_DEV_TO_SC(dev);
    struct ath_hal      *ah     = sc->sc_ah;
    struct ath_antdiv   *antdiv = &sc->sc_antdiv;

    spin_lock(&antdiv->antdiv_lock);

    /*
     * Suspend Antenna Diversity and select default antenna.
     */
    if (antdiv->antdiv_start) {
        antdiv->antdiv_suspend = 1;
        if (HAL_OK == ath_hal_selectAntConfig(ah, antdiv->antdiv_def_antcfg)) {
            antdiv->antdiv_curcfg = antdiv->antdiv_def_antcfg;
        }
    }

    spin_unlock(&antdiv->antdiv_lock);
}

void
ath_slow_ant_div_resume(ath_dev_t dev)
{
    struct ath_softc    *sc     = ATH_DEV_TO_SC(dev);
    struct ath_antdiv   *antdiv = &sc->sc_antdiv;

    spin_lock(&antdiv->antdiv_lock);

    /*
     * Resume Antenna Diversity.
     * No need to restore previous setting. Algorithm will automatically
     * choose a different antenna if the default is not appropriate.
     */
    antdiv->antdiv_suspend = 0;

    spin_unlock(&antdiv->antdiv_lock);
}

static void find_max_sdev(int8_t *val_in, u_int8_t num_val, u_int8_t *max_index, u_int16_t *sdev)
{
    int8_t   *max_val, *val = val_in, *val_last = val_in + num_val;

    if (val_in && num_val) {
        max_val = val;
        while (++val != val_last) {
            if (*val > *max_val)
                max_val = val;
        };

        if (max_index)
            *max_index = (u_int8_t)(max_val - val_in);

        if (sdev) {
            val = val_in;
            *sdev = 0;
            do {
                *sdev += (*max_val - *val);
            } while (++val != val_last);
        }
    }
}

void
ath_slow_ant_div(struct ath_antdiv *antdiv, struct ieee80211_frame *wh, struct ath_rx_status *rx_stats)
{
    struct ath_softc *sc = antdiv->antdiv_sc;
    struct ath_hal   *ah = sc->sc_ah;
    u_int64_t curtsf = 0;
    u_int8_t  bestcfg, bestcfg_chain[ATH_ANT_DIV_MAX_CHAIN], curcfg = antdiv->antdiv_curcfg;
    u_int16_t sdev_chain[ATH_ANT_DIV_MAX_CHAIN];
#ifndef REMOVE_PKT_LOG
    char logtext[MAX_LOG];
#endif

    spin_lock(&antdiv->antdiv_lock);

    if (antdiv->antdiv_start &&
        (! antdiv->antdiv_suspend) &&
        IEEE80211_IS_BEACON(wh) &&
        ATH_ADDR_EQ(wh->i_addr3, antdiv->antdiv_bssid)){
        antdiv->antdiv_lastbrssictl[0][curcfg] = rx_stats->rs_rssi_ctl0;
        antdiv->antdiv_lastbrssictl[1][curcfg] = rx_stats->rs_rssi_ctl1;
        antdiv->antdiv_lastbrssictl[2][curcfg] = rx_stats->rs_rssi_ctl2;

	DPRINTF(sc, sc->sc_reg_parm.showAntDivDbgInfo, "CurrentRSSI: %d%d%d\n",
			antdiv->antdiv_lastbrssictl[0][curcfg],
			antdiv->antdiv_lastbrssictl[1][curcfg],
			antdiv->antdiv_lastbrssictl[2][curcfg]);

/* not yet
        antdiv->antdiv_lastbrssi[curcfg] = rx_stats->rs_rssi;
        antdiv->antdiv_lastbrssiext[0][curcfg] = rx_stats->rs_rssi_ext0;
        antdiv->antdiv_lastbrssiext[1][curcfg] = rx_stats->rs_rssi_ext1;
        antdiv->antdiv_lastbrssiext[2][curcfg] = rx_stats->rs_rssi_ext2;
*/
        antdiv->antdiv_lastbtsf[curcfg] = ath_hal_gettsf64(sc->sc_ah);
        curtsf = antdiv->antdiv_lastbtsf[curcfg];
    } else {
        spin_unlock(&antdiv->antdiv_lock);
        return;
    }

    switch (antdiv->antdiv_state) {
    case ATH_ANT_DIV_IDLE:
	DPRINTF(sc, sc->sc_reg_parm.showAntDivDbgInfo, "%s: DwellTimeInIdle = %d ms, MinDwellTime=%d ms\n",
		__func__, curtsf - antdiv->antdiv_laststatetsf, antdiv->antdiv_min_dwell_time);

        if ((curtsf - antdiv->antdiv_laststatetsf) > antdiv->antdiv_min_dwell_time) {
            int8_t min_rssi = antdiv->antdiv_lastbrssictl[0][curcfg];
            int      i;
            for (i = 1; i < ATH_ANT_DIV_MAX_CHAIN; i++) {
                if (antdiv->antdiv_lastbrssictl[i][curcfg] != (-128)) {
                    if (antdiv->antdiv_lastbrssictl[i][curcfg] < min_rssi) {
                        min_rssi = antdiv->antdiv_lastbrssictl[i][curcfg];
                    }
                }
            }
		DPRINTF(sc, sc->sc_reg_parm.showAntDivDbgInfo, "%s: MinRSSI=%d, RSSItrig=%d\n", min_rssi, antdiv->antdiv_rssitrig);

            if (min_rssi < antdiv->antdiv_rssitrig) {
                curcfg ++;
                if (curcfg == antdiv->antdiv_num_antcfg) {
                    curcfg = 0;
                }

                if (HAL_OK == ath_hal_selectAntConfig(ah, curcfg)) {
                    antdiv->antdiv_bestcfg = antdiv->antdiv_curcfg;
                    antdiv->antdiv_curcfg = curcfg;
                    antdiv->antdiv_laststatetsf = curtsf;
                    antdiv->antdiv_state = ATH_ANT_DIV_SCAN;
		DPRINTF(sc, sc->sc_reg_parm.showAntDivDbgInfo, "%s: select_antcfg = %d, antdiv_state=idle\n", __func__, antdiv->antdiv_curcfg);

#ifndef REMOVE_PKT_LOG
                    /* do pktlog */
                    {
                        snprintf(logtext, MAX_LOG, "SlowAntDiv: select_antcfg = %d\n", antdiv->antdiv_curcfg);
                        ath_log_text(sc, logtext, 0);
                    }
#endif
                }
            }
        }
        break;

    case ATH_ANT_DIV_SCAN:
	DPRINTF(sc, sc->sc_reg_parm.showAntDivDbgInfo, "%s: DwellTimeInScan=%d ms, AntdivSettleTime=%d ms\n",
			__func__, curtsf - antdiv->antdiv_laststatetsf, antdiv->antdiv_settle_time);
        if ((curtsf - antdiv->antdiv_laststatetsf) < antdiv->antdiv_settle_time)
            break;

        curcfg ++;
        if (curcfg == antdiv->antdiv_num_antcfg) {
            curcfg = 0;
        }

        if (curcfg == antdiv->antdiv_bestcfg) {
            u_int16_t *max_sdev;
            int       i;
            for (i = 0; i < ATH_ANT_DIV_MAX_CHAIN; i++) {
                find_max_sdev(&antdiv->antdiv_lastbrssictl[i][0], antdiv->antdiv_num_antcfg, &bestcfg_chain[i], &sdev_chain[i]);
		DPRINTF(sc, sc->sc_reg_parm.showAntDivDbgInfo, "%s: best cfg (chain%d) = %d (cfg0:%d, cfg1:%d, sdev:%d)\n", __func__, i,
                            bestcfg_chain[i], antdiv->antdiv_lastbrssictl[i][0],
                            antdiv->antdiv_lastbrssictl[i][1], sdev_chain[i]);

#ifndef REMOVE_PKT_LOG
                /* do pktlog */
                {
                    snprintf(logtext, MAX_LOG, "SlowAntDiv: best cfg (chain%d) = %d (cfg0:%d, cfg1:%d, sdev:%d)\n", i,
                            bestcfg_chain[i], antdiv->antdiv_lastbrssictl[i][0],
                            antdiv->antdiv_lastbrssictl[i][1], sdev_chain[i]);
                    ath_log_text(sc, logtext, 0);
                }
#endif
            }

            max_sdev = sdev_chain;
            for (i = 1; i < ATH_ANT_DIV_MAX_CHAIN; i++) {
                if (*(sdev_chain + i) > *max_sdev) {
                    max_sdev = sdev_chain + i;
                }
            }
            bestcfg = bestcfg_chain[(max_sdev - sdev_chain)];

            if (bestcfg != antdiv->antdiv_curcfg) {
                if (HAL_OK == ath_hal_selectAntConfig(ah, bestcfg)) {
                    antdiv->antdiv_bestcfg = bestcfg;
                    antdiv->antdiv_curcfg = bestcfg;
		DPRINTF(sc, sc->sc_reg_parm.showAntDivDbgInfo, "%s: select best cfg = %d\n", __func__, antdiv->antdiv_curcfg);
#ifndef REMOVE_PKT_LOG
                    /* do pktlog */
                    {
                        snprintf(logtext, MAX_LOG, "SlowAntDiv: select best cfg = %d\n", antdiv->antdiv_curcfg);
                        ath_log_text(sc, logtext, 0);
                    }
#endif
                }
            }
            antdiv->antdiv_laststatetsf = curtsf;
            antdiv->antdiv_state = ATH_ANT_DIV_IDLE;
	    DPRINTF(sc, sc->sc_reg_parm.showAntDivDbgInfo, "%s: antdiv_state=idle\n", __func__);

        } else {
            if (HAL_OK == ath_hal_selectAntConfig(ah, curcfg)) {
                antdiv->antdiv_curcfg = curcfg;
                antdiv->antdiv_laststatetsf = curtsf;
                antdiv->antdiv_state = ATH_ANT_DIV_SCAN;
		DPRINTF(sc, sc->sc_reg_parm.showAntDivDbgInfo, "%s: select ant cfg = %d, antdiv_state=scan\n", __func__, antdiv->antdiv_curcfg);

#ifndef REMOVE_PKT_LOG
                /* do pktlog */
                {
                    snprintf(logtext, MAX_LOG, "SlowAntDiv: select ant cfg = %d\n", antdiv->antdiv_curcfg);
                    ath_log_text(sc, logtext, 0);
                }
#endif
            }
        }

        break;
    }

    spin_unlock(&antdiv->antdiv_lock);
}

#endif    /* ATH_SLOW_ANT_DIV */

#if ATH_ANT_DIV_COMB
static int
ath_ant_div_comb_alt_check(u_int config_group, int alt_ratio, int alt_ratio_th, int curr_main_set, int curr_alt_set, int alt_rssi_avg, int main_rssi_avg, int low_rssi_th)
{
    int result = 0;

    switch(config_group) {
        case HAL_ANTDIV_CONFIG_GROUP_1:
        case HAL_ANTDIV_CONFIG_GROUP_2:
            if((((curr_main_set == HAL_ANT_DIV_COMB_LNA2) && (curr_alt_set == HAL_ANT_DIV_COMB_LNA1) && (alt_rssi_avg >= main_rssi_avg - 5)) ||
                ((curr_main_set == HAL_ANT_DIV_COMB_LNA1) && (curr_alt_set == HAL_ANT_DIV_COMB_LNA2) && (alt_rssi_avg >= main_rssi_avg - 2)) ||
                (alt_ratio > alt_ratio_th)) && (alt_rssi_avg >= low_rssi_th) &&
                (alt_rssi_avg >= 4))
            {
                result = 1;
            } else {
                result = 0;
            }
            break;
        case DEFAULT_ANTDIV_CONFIG_GROUP:
        default:
            (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO) ? (result = 1) : (result = 0);
            break;
    }

    return result;
}

static void
ath_ant_adjust_fast_div_bias_tp(struct ath_antcomb *antcomb, int alt_ratio, int alt_ant_ratio_th, u_int config_group, HAL_ANT_COMB_CONFIG *pdiv_ant_conf)
{

    if(config_group == HAL_ANTDIV_CONFIG_GROUP_1)
    {

        switch ((pdiv_ant_conf->main_lna_conf << 4) | pdiv_ant_conf->alt_lna_conf) {
            case (0x01): //A-B LNA2
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x02): //A-B LNA1
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x03): //A-B A+B
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x10): //LNA2 A-B
                if ((antcomb->scan == 0) && (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) {
                  pdiv_ant_conf->fast_div_bias = 0x3f;
                }else{
                  pdiv_ant_conf->fast_div_bias = 0x1;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x12): //LNA2 LNA1
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x13): //LNA2 A+B
                if ((antcomb->scan == 0) && (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) {
                  pdiv_ant_conf->fast_div_bias = 0x3f;
                }else{
                  pdiv_ant_conf->fast_div_bias = 0x1;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x20): //LNA1 A-B
                if ((antcomb->scan == 0) && (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) {
                    pdiv_ant_conf->fast_div_bias = 0x3f;
                }else{
                    pdiv_ant_conf->fast_div_bias = 0x1;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x21): //LNA1 LNA2
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x23): //LNA1 A+B
                if ((antcomb->scan == 0) && (alt_ratio > ANT_DIV_COMB_ALT_ANT_RATIO)) {
                  pdiv_ant_conf->fast_div_bias = 0x3f;
                }else{
                  pdiv_ant_conf->fast_div_bias = 0x1;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x30): //A+B A-B
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x31): //A+B LNA2
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x32): //A+B LNA1
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            default:
                break;
          }
    } else if (config_group == HAL_ANTDIV_CONFIG_GROUP_2) {
        switch ((pdiv_ant_conf->main_lna_conf << 4) | pdiv_ant_conf->alt_lna_conf) {
            case (0x01): //A-B LNA2
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x02): //A-B LNA1
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x03): //A-B A+B
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x10): //LNA2 A-B
                if ((antcomb->scan == 0) && (alt_ratio > alt_ant_ratio_th)) {
                    pdiv_ant_conf->fast_div_bias = 0x1;
                } else {
                    pdiv_ant_conf->fast_div_bias = 0x2;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x12): //LNA2 LNA1
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x13): //LNA2 A+B
                if ((antcomb->scan == 0) && (alt_ratio > alt_ant_ratio_th)) {
                    pdiv_ant_conf->fast_div_bias = 0x1;
                } else {
                    pdiv_ant_conf->fast_div_bias = 0x2;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x20): //LNA1 A-B
                if ((antcomb->scan == 0) && (alt_ratio > alt_ant_ratio_th)) {
                    pdiv_ant_conf->fast_div_bias = 0x1;
                } else {
                    pdiv_ant_conf->fast_div_bias = 0x2;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x21): //LNA1 LNA2
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x23): //LNA1 A+B
                if ((antcomb->scan == 0) && (alt_ratio > alt_ant_ratio_th)) {
                    pdiv_ant_conf->fast_div_bias = 0x1;
                } else {
                    pdiv_ant_conf->fast_div_bias = 0x2;
                }
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x30): //A+B A-B
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x31): //A+B LNA2
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            case (0x32): //A+B LNA1
                pdiv_ant_conf->fast_div_bias = 0x1;
                pdiv_ant_conf->main_gaintb   = 0;
                pdiv_ant_conf->alt_gaintb    = 0;
                break;
            default:
                break;
        }
        if (antcomb->fast_div_bias)
            pdiv_ant_conf->fast_div_bias = antcomb->fast_div_bias;
    } else { /* DEFAULT_ANTDIV_CONFIG_GROUP */
        switch ((pdiv_ant_conf->main_lna_conf << 4) | pdiv_ant_conf->alt_lna_conf) {
            case (0x01): //A-B LNA2
                 pdiv_ant_conf->fast_div_bias = 0x3b;
                 break;
             case (0x02): //A-B LNA1
                 pdiv_ant_conf->fast_div_bias = 0x3d;
                 break;
             case (0x03): //A-B A+B
                 pdiv_ant_conf->fast_div_bias = 0x1;
                 break;
             case (0x10): //LNA2 A-B
                 pdiv_ant_conf->fast_div_bias = 0x7;
                 break;
             case (0x12): //LNA2 LNA1
                 pdiv_ant_conf->fast_div_bias = 0x2;
                 break;
             case (0x13): //LNA2 A+B
                 pdiv_ant_conf->fast_div_bias = 0x7;
                 break;
             case (0x20): //LNA1 A-B
                 pdiv_ant_conf->fast_div_bias = 0x6;
                 break;
             case (0x21): //LNA1 LNA2
                 pdiv_ant_conf->fast_div_bias = 0x0;
                 break;
             case (0x23): //LNA1 A+B
                 pdiv_ant_conf->fast_div_bias = 0x6;
                 break;
             case (0x30): //A+B A-B
                 pdiv_ant_conf->fast_div_bias = 0x1;
                 break;
             case (0x31): //A+B LNA2
                 pdiv_ant_conf->fast_div_bias = 0x3b;
                 break;
             case (0x32): //A+B LNA1
                 pdiv_ant_conf->fast_div_bias = 0x3d;
                 break;
             default:
                 break;
           }
    }
}

static void
ath_ant_tune_comb_LNA1_value(HAL_ANT_COMB_CONFIG         *pdiv_ant_conf)
{
    uint8_t config_group = pdiv_ant_conf->antdiv_configgroup;
    if ((config_group == HAL_ANTDIV_CONFIG_GROUP_1) ||
        (config_group == HAL_ANTDIV_CONFIG_GROUP_2)) {
        pdiv_ant_conf->lna1_lna2_delta = -9;
    } else { /* DEFAULT_ANTDIV_CONFIG_GROUP */
        pdiv_ant_conf->lna1_lna2_delta = -3;
    }

}
void
ath_ant_div_comb_init(struct ath_antcomb *antcomb, struct ath_softc *sc)
{
    struct ath_hal *ah = sc->sc_ah;
    HAL_ANT_COMB_CONFIG div_ant_conf;
    OS_MEMZERO(antcomb, sizeof(struct ath_antcomb));
    antcomb->antcomb_sc = sc;
    antcomb->count = ANT_DIV_COMB_INIT_COUNT;
    antcomb->low_rssi_th = sc->sc_reg_parm.ant_div_low_rssi_cfg;
    antcomb->fast_div_bias = sc->sc_reg_parm.ant_div_fast_div_bias;
    ath_hal_getAntDivCombConf(ah, &div_ant_conf);
    ath_ant_tune_comb_LNA1_value(&div_ant_conf);
}


void
ath_ant_div_comb_scan(struct ath_antcomb *antcomb, struct ath_rx_status *rx_stats)
{
    struct ath_softc *sc = antcomb->antcomb_sc;
    struct ath_hal *ah = sc->sc_ah;
    int alt_ratio, alt_rssi_avg, main_rssi_avg, curr_alt_set;
    int curr_main_set, curr_bias;
    int8_t main_rssi = rx_stats->rs_rssi_ctl0;
    int8_t alt_rssi = rx_stats->rs_rssi_ctl1;
    int8_t end_st = 0;
    int8_t rx_ant_conf,  main_ant_conf;
    u_int8_t rx_pkt_moreaggr = rx_stats->rs_moreaggr;
    u_int8_t short_scan = 0;
    u_int32_t comb_delta_time = 0;
    int tmp_ant_ratio;
    int tmp_ant_ratio2;
    HAL_ANT_COMB_CONFIG div_ant_conf;

    /* Data Collection */
    rx_ant_conf = (rx_stats->rs_rssi_ctl2 >> 4) & 0x3;
    main_ant_conf= (rx_stats->rs_rssi_ctl2 >> 2) & 0x3;

    if (alt_rssi >= antcomb->low_rssi_th) {
        tmp_ant_ratio = ANT_DIV_COMB_ALT_ANT_RATIO;
        tmp_ant_ratio2 = ANT_DIV_COMB_ALT_ANT_RATIO2;
    } else {
        tmp_ant_ratio = ANT_DIV_COMB_ALT_ANT_RATIO_LOW_RSSI;
        tmp_ant_ratio2 = ANT_DIV_COMB_ALT_ANT_RATIO2_LOW_RSSI;
    }

    /* Only Record packet with main_rssi/alt_rssi is positive */
    if ((main_rssi > 0) && (alt_rssi > 0)) {
        antcomb->total_pkt_count++;
        antcomb->main_total_rssi += main_rssi;
        antcomb->alt_total_rssi  += alt_rssi;
        if (main_ant_conf == rx_ant_conf) {
           antcomb->main_recv_cnt++;
        }
        else {
           antcomb->alt_recv_cnt++;
        }
    }

    /* Short scan check */
    /* If alt is good when count=100, and alt is bad when scan=1,
     * reduce pkt number to 128 to reduce dip
     * Don't care aggregation
     */
    short_scan = 0;
    if (antcomb->scan && antcomb->alt_good) {
        comb_delta_time = (CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP()) -
                           antcomb->scan_start_time);
        if (comb_delta_time > ANT_DIV_COMB_SHORT_SCAN_INTR)
            short_scan = 1;
        else{
            if (antcomb->total_pkt_count == ANT_DIV_COMB_SHORT_SCAN_PKTCOUNT)
            {
               if (antcomb->total_pkt_count == 0) {
                  alt_ratio = 0;
               } else {
                  alt_ratio = ((antcomb->alt_recv_cnt * 100) /
                                antcomb->total_pkt_count);
               }
               if (alt_ratio < tmp_ant_ratio)
                   short_scan = 1;
            }
        }
    }

    /* Diversity and Combining algorithm */
    if (((antcomb->total_pkt_count < ANT_DIV_COMB_MAX_PKTCOUNT) ||
         (rx_pkt_moreaggr != 0)) &&
         (short_scan == 0)) {
        return;
    }

    /* When collect more than 512 packets, and finish aggregation packet
     * Or short scan=1.
     * Start to do decision
     */
    if (antcomb->total_pkt_count == 0) {
        alt_ratio = 0;
        main_rssi_avg = 0;
        alt_rssi_avg = 0;
    } else {
        alt_ratio = ((antcomb->alt_recv_cnt * 100) / antcomb->total_pkt_count);
        main_rssi_avg = (antcomb->main_total_rssi / antcomb->total_pkt_count);
        alt_rssi_avg  = (antcomb->alt_total_rssi / antcomb->total_pkt_count);
    }


    ath_hal_getAntDivCombConf(ah, &div_ant_conf);
    curr_alt_set = div_ant_conf.alt_lna_conf;
    curr_main_set = div_ant_conf.main_lna_conf;
    curr_bias     = div_ant_conf.fast_div_bias;

    antcomb->count++;

    if (antcomb->count == ANT_DIV_COMB_MAX_COUNT) {
        if (alt_ratio > tmp_ant_ratio) {
            antcomb->alt_good = 1;
            antcomb->quick_scan_cnt = 0;

            if (curr_main_set == HAL_ANT_DIV_COMB_LNA2) {
                   antcomb->rssi_lna2 = main_rssi_avg;
            } else if (curr_main_set == HAL_ANT_DIV_COMB_LNA1) {
                   antcomb->rssi_lna1 = main_rssi_avg;
            }
            switch ((curr_main_set << 4) | curr_alt_set) {
                case (0x10): //LNA2 A-B
                    antcomb->main_conf = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                    antcomb->first_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                    antcomb->second_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1;
                    break;
                case (0x20):
                    antcomb->main_conf = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                    antcomb->first_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                    antcomb->second_quick_scan_conf = HAL_ANT_DIV_COMB_LNA2;
                    break;
                case (0x21): //LNA1 LNA2
                    antcomb->main_conf = HAL_ANT_DIV_COMB_LNA2;
                    antcomb->first_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                    antcomb->second_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                    break;
                case (0x12): //LNA2 LNA1
                    antcomb->main_conf = HAL_ANT_DIV_COMB_LNA1;
                    antcomb->first_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                    antcomb->second_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                    break;
                case (0x13): //LNA2 A+B
                    antcomb->main_conf = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                    antcomb->first_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                    antcomb->second_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1;
                    break;
                case (0x23): //LNA1 A+B
                    antcomb->main_conf = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                    antcomb->first_quick_scan_conf = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                    antcomb->second_quick_scan_conf = HAL_ANT_DIV_COMB_LNA2;
                    break;
                default:
                    break;
            }
        }
        else {
            antcomb->alt_good=0;
        }
        antcomb->count = 0;
        antcomb->scan = 1;
        antcomb->scan_not_start = 1;
    }

    if (ath_ant_div_comb_alt_check(div_ant_conf.antdiv_configgroup, alt_ratio, tmp_ant_ratio, curr_main_set, curr_alt_set, alt_rssi_avg, main_rssi_avg, antcomb->low_rssi_th) &&
        (antcomb->scan == 0)) {
        if (curr_alt_set==HAL_ANT_DIV_COMB_LNA2) {
            // Switch main and alt LNA
            div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
            div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1;
            end_st = 24;
        }
        else if (curr_alt_set==HAL_ANT_DIV_COMB_LNA1) {
            div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
            div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA2;
            end_st = 25;
        }
        else {
            end_st = 1;
        }
        goto div_comb_done;
    }

    if ((curr_alt_set != HAL_ANT_DIV_COMB_LNA1) &&
        (curr_alt_set != HAL_ANT_DIV_COMB_LNA2) &&
        (antcomb->scan != 1)) {
        /* Set alt to another LNA*/
        if (curr_main_set==HAL_ANT_DIV_COMB_LNA2) { //main LNA2, alt LNA1
            div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
            div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1;
            end_st = 22;
        }else if(curr_main_set==HAL_ANT_DIV_COMB_LNA1) { //main LNA1, alt LNA2
            div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
            div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA2;
            end_st = 23;
        }
        else {
            end_st = 2;
        }
        goto div_comb_done;
    }

    if ((alt_rssi_avg < (main_rssi_avg + div_ant_conf.lna1_lna2_delta)) &&
        (antcomb->scan != 1)) {
        end_st = 3;
        goto div_comb_done;
    }
    if (antcomb->scan_not_start == 0) {
        switch (curr_alt_set) {
            case HAL_ANT_DIV_COMB_LNA2:
                antcomb->rssi_lna2 = alt_rssi_avg;
                antcomb->rssi_lna1 = main_rssi_avg;
                antcomb->scan = 1;
                /* set to A+B */
                div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                end_st = 4;
                break;
            case HAL_ANT_DIV_COMB_LNA1:
                antcomb->rssi_lna1 = alt_rssi_avg;
                antcomb->rssi_lna2 = main_rssi_avg;
                antcomb->scan = 1;
                /* set to A+B */
                div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                end_st = 4;
                break;
            case HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2:
                antcomb->rssi_add = alt_rssi_avg;
                antcomb->scan = 1;
                /* set to A-B */
                div_ant_conf.main_lna_conf = curr_main_set;
                div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                end_st = 5;
                break;
            case HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2:
                antcomb->rssi_sub = alt_rssi_avg;
                antcomb->scan = 0;
                if (antcomb->rssi_lna2 >
                    (antcomb->rssi_lna1 + ANT_DIV_COMB_LNA1_LNA2_SWITCH_DELTA)) {
                    //use LNA2 as main LNA
                    if ((antcomb->rssi_add > antcomb->rssi_lna1) &&
                        (antcomb->rssi_add > antcomb->rssi_sub)) {
                        /* set to A+B */
                        div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                        div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                        end_st = 7;
                    }
                    else if (antcomb->rssi_sub > antcomb->rssi_lna1) {
                        /* set to A-B */
                        div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                        div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                        end_st = 6;
                    }
                    else {
                        /* set to LNA1 */
                        div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                        div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1;
                        end_st = 8;
                    }
                }
                else { //use LNA1 as main LNA
                    if ((antcomb->rssi_add > antcomb->rssi_lna2) &&
                        (antcomb->rssi_add > antcomb->rssi_sub)) {
                        /* set to A+B */
                        div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                        div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2;
                        end_st = 27;
                    }
                    else if (antcomb->rssi_sub > antcomb->rssi_lna1) {
                        /* set to A-B */
                        div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                        div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2;
                        end_st = 26;
                    }
                    else {
                        /* set to LNA2 */
                        div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                        div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA2;
                        end_st = 28;
                    }
                }
                break;
            default:
                break;
        } /* End Switch*/
    } /* End scan_not_start = 0 */
    else {
        if (antcomb->alt_good != 1) {
            antcomb->scan_not_start= 0;
            /* Set alt to another LNA */
            if (curr_main_set==HAL_ANT_DIV_COMB_LNA2) {
                div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA1;
            }else if (curr_main_set==HAL_ANT_DIV_COMB_LNA1) {
                div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                div_ant_conf.alt_lna_conf  = HAL_ANT_DIV_COMB_LNA2;
            }
            end_st = 9;
            goto div_comb_done;
        }
        /* alt_good == 1*/
        switch (antcomb->quick_scan_cnt) {
        case 0:
            /* set alt to main, and alt to first conf */
            end_st = 10;
            /* Set configuration */
            div_ant_conf.main_lna_conf = antcomb->main_conf;
            div_ant_conf.alt_lna_conf = antcomb->first_quick_scan_conf;
            break;
        case 1:
            /* set alt to main, and alt to first conf */
            end_st = 11;
            /* Set configuration */
            div_ant_conf.main_lna_conf = antcomb->main_conf;
            div_ant_conf.alt_lna_conf = antcomb->second_quick_scan_conf;
            antcomb->rssi_first = main_rssi_avg;
            antcomb->rssi_second = alt_rssi_avg;
            /* if alt have some good and also RSSI is larger than
             * (main+ANT_DIV_COMB_LNA1_DELTA_HI), choose alt.
             * But be careful when RSSI is high (diversity is off),
             * we won't have any packets.
             * So add a threshold of 50 packets
             * Or alt already have larger RSSI
             */
            if (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA1) { //main is LNA1
                if ((((alt_ratio >= tmp_ant_ratio2) &&
                    (alt_rssi_avg >= antcomb->low_rssi_th) &&
                    (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_HI)) ||
                    (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_LOW)) &&
                    (antcomb->total_pkt_count > 50)) {
                    antcomb->first_ratio = 1;
                }
                else {
                    antcomb->first_ratio = 0;
                }
            }
            else if (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA2) { // main is LNA2
                if ((((alt_ratio >= tmp_ant_ratio2) &&
                    (alt_rssi_avg >= antcomb->low_rssi_th) &&
                    (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_MID)) ||
                    (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_LOW)) &&
                     (antcomb->total_pkt_count > 50)) {
                     antcomb->first_ratio = 1;
                }
                else {
                     antcomb->first_ratio = 0;
                }
            }
            else {
                if ((((alt_ratio >= tmp_ant_ratio2) &&
                    (alt_rssi_avg >= antcomb->low_rssi_th) &&
                    (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_HI)) ||
                    (alt_rssi_avg > main_rssi_avg)) &&
                    (antcomb->total_pkt_count > 50)) {
                    antcomb->first_ratio = 1;
                }
                else {
                    antcomb->first_ratio = 0;
                }
            }
            break;
        case 2:
            antcomb->alt_good=0;
            antcomb->scan_not_start=0;
            antcomb->scan = 0;
            antcomb->rssi_first = main_rssi_avg;
            antcomb->rssi_third = alt_rssi_avg;
            /* if alt have some good and also RSSI is larger than
             * (main + ANT_DIV_COMB_LNA1_DELTA_HI), choose alt.
             * But be careful when RSSI is high (diversity is off),
             * we won't have any packets.
             * So add a threshould of 50 packets
             * Or alt already have larger RSSI
             */

            if (antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2)
            {
               if (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA2)
               {
                  antcomb->rssi_lna2 = main_rssi_avg;
               }
               else if (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA1)
               {
                  antcomb->rssi_lna1 = main_rssi_avg;
               }
            } else if (antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA2) {
                  antcomb->rssi_lna2 = alt_rssi_avg;
            } else if (antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA1) {
                  antcomb->rssi_lna1 = alt_rssi_avg;
            }

            //select which is main LNA
            if (antcomb->rssi_lna2 > antcomb->rssi_lna1 + ANT_DIV_COMB_LNA1_LNA2_SWITCH_DELTA ) {
                div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA2;
            }else {
                div_ant_conf.main_lna_conf = HAL_ANT_DIV_COMB_LNA1;
            }

            if (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA1) { // main is LNA1
                if ((((alt_ratio >= tmp_ant_ratio2) &&
                     (alt_rssi_avg >= antcomb->low_rssi_th) &&
                     (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_HI)) ||
                     (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_LOW)) &&
                    (antcomb->total_pkt_count > 50)) {
                    antcomb->second_ratio=1;
                }
                else {
                    antcomb->second_ratio=0;
                }
            }
            else if (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA2) { // main is LNA2
                if ((((alt_ratio >= tmp_ant_ratio2) &&
                     (alt_rssi_avg >= antcomb->low_rssi_th) &&
                     (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_MID)) ||
                     (alt_rssi_avg>main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_LOW)) &&
                    (antcomb->total_pkt_count > 50)) {
                    antcomb->second_ratio=1;
                }
                else {
                    antcomb->second_ratio=0;
                }
            }
            else {
                if ((((alt_ratio >= tmp_ant_ratio2) &&
                    (alt_rssi_avg >= antcomb->low_rssi_th) &&
                    (alt_rssi_avg > main_rssi_avg + ANT_DIV_COMB_LNA1_DELTA_HI)) ||
                    (alt_rssi_avg>main_rssi_avg)) &&
                    (antcomb->total_pkt_count > 50)) {
                    antcomb->second_ratio = 1;
                }
                else {
                    antcomb->second_ratio = 0;
                }
            }

            /* judgement, set alt to the conf with maximun ratio */
            if (antcomb->first_ratio) {
                if (antcomb->second_ratio) {
                    /* compare RSSI of alt*/
                    if (antcomb->rssi_second > antcomb->rssi_third){
                        /*first alt*/
                        if ((antcomb->first_quick_scan_conf == HAL_ANT_DIV_COMB_LNA1) ||
                            (antcomb->first_quick_scan_conf == HAL_ANT_DIV_COMB_LNA2)) {
                            /* Set alt LNA1 or LNA2*/
                            if (div_ant_conf.main_lna_conf == HAL_ANT_DIV_COMB_LNA2) {
                                div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                            } else {
                                div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                            }
                            end_st = 12;
                        }
                        else {
                            /* Set alt to A+B or A-B */
                            div_ant_conf.alt_lna_conf = antcomb->first_quick_scan_conf;

                            end_st = 13;
                        }
                    }
                    else {
                        /*second alt*/
                        if ((antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA1) ||
                            (antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA2)) {
                            /* Set alt LNA1 or LNA2*/
                            if (div_ant_conf.main_lna_conf == HAL_ANT_DIV_COMB_LNA2) {
                                div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                            } else {
                                div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                            }
                            end_st = 14;
                        }
                        else {
                            /* Set alt to A+B or A-B */
                            div_ant_conf.alt_lna_conf = antcomb->second_quick_scan_conf;
                            end_st = 15;
                        }
                    }
                }
                else {
                    /* first alt*/
                    if ((antcomb->first_quick_scan_conf == HAL_ANT_DIV_COMB_LNA1) ||
                        (antcomb->first_quick_scan_conf == HAL_ANT_DIV_COMB_LNA2)) {
                        /* Set alt LNA1 or LNA2*/
                            if (div_ant_conf.main_lna_conf == HAL_ANT_DIV_COMB_LNA2) {
                                div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                            } else {
                                div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                            }
                        end_st = 16;
                    }
                    else {
                        /* Set alt to A+B or A-B */
                        div_ant_conf.alt_lna_conf = antcomb->first_quick_scan_conf;
                        end_st = 17;
                    }
                }
            }
            else {
                if (antcomb->second_ratio) {
                    /* second alt*/
                    if ((antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA1) ||
                        (antcomb->second_quick_scan_conf == HAL_ANT_DIV_COMB_LNA2)) {
                        /* Set alt LNA1 or LNA2*/
                        if (div_ant_conf.main_lna_conf == HAL_ANT_DIV_COMB_LNA2) {
                            div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                        } else {
                            div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                        }
                        end_st = 18;
                    }
                    else {
                        /* Set alt to A+B or A-B */
                        div_ant_conf.alt_lna_conf = antcomb->second_quick_scan_conf;
                        end_st = 19;
                    }
                }
                else {
                    /* main is largest*/
                    if ((antcomb->main_conf == HAL_ANT_DIV_COMB_LNA1) ||
                        (antcomb->main_conf == HAL_ANT_DIV_COMB_LNA2)) {
                        /* Set alt LNA1 or LNA2*/
                        if (div_ant_conf.main_lna_conf == HAL_ANT_DIV_COMB_LNA2) {
                            div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA1;
                        } else {
                            div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA2;
                        }
                        end_st = 20;

                    }
                    else {
                        /* Set alt to A+B or A-B */
                        div_ant_conf.alt_lna_conf = antcomb->main_conf;
                        end_st = 21;
                    }
                }
            }
            break;
        default:
            break;
        } /* end of quick_scan_cnt switch */
        antcomb->quick_scan_cnt++;
    } /* End scan_not_start != 0*/

div_comb_done:
    /* Adjust the fast_div_bias based on main and alt lna conf */
    ath_ant_adjust_fast_div_bias_tp(antcomb, alt_ratio, tmp_ant_ratio, div_ant_conf.antdiv_configgroup, &div_ant_conf);


    div_ant_conf.switch_com_r     = HAL_SWITCH_COM_MASK;  /* don't care */
    div_ant_conf.switch_com_t     = HAL_SWITCH_COM_MASK;  /* don't care */
    div_ant_conf.fast_div_disable = AH_FALSE;

    /* Write new setting to register*/
    ath_hal_setAntDivCombConf(ah, &div_ant_conf);

    /* get system time*/
    antcomb->scan_start_time = CONVERT_SYSTEM_TIME_TO_MS(OS_GET_TIMESTAMP());

#if 0
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB current time is 0x%x \n", antcomb->scan_start_time);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB total pkt num   %3d \n", antcomb->total_pkt_count);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB count num       %3d \n", antcomb->count);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB total main num  %3d \n", antcomb->main_recv_cnt);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB total alt  num  %3d \n", antcomb->alt_recv_cnt);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB Ratio           %3d \n", alt_ratio);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB RSSI main avg   %3d \n", main_rssi_avg);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB RSSI alt  avg   %3d \n", alt_rssi_avg);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB current alt setting is %d, END %d div_scan=%d scan_not_start=%d\n",
        curr_alt_set, end_st, antcomb->scan,antcomb->scan_not_start);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB Current setting main=%d alt=%d bias =0x%x \n",curr_main_set,curr_alt_set,curr_bias);
    DPRINTF(sc, ATH_DEBUG_ANY, "ANT_DIV_COMB next_main=%d next_alt=%d next_bias =0x%x \n",
        div_ant_conf.main_lna_conf,div_ant_conf.alt_lna_conf,div_ant_conf.fast_div_bias);
#endif /* 0 */
    /* Clear variable */
    antcomb->total_pkt_count = 0;
    antcomb->main_total_rssi = 0;
    antcomb->alt_total_rssi = 0;
    antcomb->main_recv_cnt = 0;
    antcomb->alt_recv_cnt = 0;
}

/*
 * Smart antenna
 *
 *
 */
static ATH_SA_PATTERN_INFO ath_antenna_info_3x4[] = {
    /*         lna_str           ant_str  main_lna                          switch_com_ra1l1        switch_com_t1 */
    /* [0] */{"LNA1",            "ANT1",  HAL_ANT_DIV_COMB_LNA1,            HAL_SWITCH_COM_ANT1,    HAL_SWITCH_COM_ANT1},
    /* [1] */{"LNA1",            "ANT2",  HAL_ANT_DIV_COMB_LNA1,            HAL_SWITCH_COM_ANT2,    HAL_SWITCH_COM_ANT2},
    /* [2] */{"LNA1",            "ANT3",  HAL_ANT_DIV_COMB_LNA1,            HAL_SWITCH_COM_ANT3,    HAL_SWITCH_COM_ANT3},
    /* [3] */{"LNA1_MINUS_LNA2", "ANTx",  HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2, HAL_SWITCH_COM_ANTX,    HAL_SWITCH_COM_ANTX},
    /* [4] */{"LNA2",            "ANTx",  HAL_ANT_DIV_COMB_LNA2,            HAL_SWITCH_COM_MASK,    HAL_SWITCH_COM_ANTX},
    /* [5] */{"LNA1_PLUS_LNA2",  "ANTx",  HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2,  HAL_SWITCH_COM_ANTX,    HAL_SWITCH_COM_ANTX},
};

/*
 * table rule:  1. switch_com can't be "HAL_SWITCH_COM_ANTX" if pattern < antennaAntx,
 *              2. antennaAntx != 0
 * After scan finish antennaAntx patterns, then chose the best for Antx
 */
static ATH_SA_PATTERN_TABLE ath_antenna_table_3x4 = {
    sizeof(ath_antenna_info_3x4)/sizeof(ATH_SA_PATTERN_INFO),
    ath_antenna_info_3x4,
    3,                             // antennaAntx
};

static void ath_smartant_scan_end(struct ath_sascan *sascan);
static void ath_smartant_idle_start(struct ath_sascan *sascan);
static OS_TIMER_FUNC(ath_smartant_scan_handler);
static OS_TIMER_FUNC(ath_smartant_idle_handler);
static OS_TIMER_FUNC(ath_smartant_check_handler);

u_int8_t
ath_smartant_attach(struct ath_softc *sc)
{
    struct ath_sascan   *sascan;
    struct ath_hal      *ah = sc->sc_ah;

    sc->sc_sascan = (struct ath_sascan *)OS_MALLOC(sc->sc_osdev, sizeof(struct ath_sascan), GFP_KERNEL);
    if (!sc->sc_sascan) {
        DPRINTF(sc, ATH_DEBUG_ANY, "Smart antenna attach failed, not enough memory!\n", __func__);
        sc->sc_smart_antenna = 0;
        return AH_FALSE;
    }
    sascan = sc->sc_sascan;
    OS_MEMZERO(sascan, sizeof(struct ath_sascan));

    if (sc->sc_reg_parm.forceSmartAntenna)
        sc->sc_smart_antenna = sc->sc_reg_parm.forceSmartAntenna;

    sascan->sa_mode             = sc->sc_smart_antenna;
    sascan->sa_sc               = sc;
    sascan->sa_state            = SMART_ANT_INIT;
    sascan->antenna_table       = &ath_antenna_table_3x4;
    sascan->patternMaxCount     = sascan->antenna_table->patternCount;
    sascan->antennaAntxCount    = sascan->antenna_table->antennaAntx;
    sascan->best_rx_conf        = 0;
    sascan->antenna_pattern     = 0;
    sascan->antx_conf           = 0;
    sascan->keep_same_cnt       = 0;
    sascan->keep_same_granted   = 1;
    sascan->best_tx_conf        = 0;
    sascan->period_last_time    = OS_GET_TIMESTAMP();
    sascan->sa_ap_count         = 0;
    TAILQ_INIT(&sascan->sa_aplist);

    if (sascan->patternMaxCount > SMART_ANT_TABLE_MAX) {
        DPRINTF(sc, ATH_DEBUG_ANY, "Smart antenna attach failed, can't attach patternMaxCount[%d] > max[%d]\n",
                                                 __func__, sascan->patternMaxCount, SMART_ANT_TABLE_MAX);
        OS_FREE(sc->sc_sascan);
        sc->sc_sascan = NULL;
        sc->sc_smart_antenna = 0;
        return AH_FALSE;
    }

    OS_INIT_TIMER(sc->sc_osdev, &sascan->sa_scan_timer, ath_smartant_scan_handler,
            sc, QDF_TIMER_TYPE_WAKE_APPS);
    OS_INIT_TIMER(sc->sc_osdev, &sascan->sa_idle_timer, ath_smartant_idle_handler,
            sc, QDF_TIMER_TYPE_WAKE_APPS);
    OS_INIT_TIMER(sc->sc_osdev, &sascan->sa_check_timer, ath_smartant_check_handler,
            sc, QDF_TIMER_TYPE_WAKE_APPS);
    return AH_TRUE;
}

void
ath_smartant_detach(struct ath_softc *sc)
{
    struct ath_sascan   *sascan = sc->sc_sascan;
    struct ath_sa_aplist *ap;

    if (!sascan) {
        return;
    }

    while (!TAILQ_EMPTY(&sascan->sa_aplist)) {
        ap = TAILQ_FIRST(&sascan->sa_aplist);
        TAILQ_REMOVE(&sascan->sa_aplist, ap, ap_next);
        OS_FREE(ap);
    }

    OS_FREE_TIMER(&sascan->sa_scan_timer);
    OS_FREE_TIMER(&sascan->sa_idle_timer);
    OS_FREE_TIMER(&sascan->sa_check_timer);

    OS_FREE(sc->sc_sascan);
    sc->sc_sascan = NULL;
}

/*
 * calculate delta time
 */
static void ath_smartant_get_time(struct ath_sascan *sascan, struct ath_sa_info *cell)
{
    systime_t cur_time;

    cur_time = OS_GET_TIMESTAMP();

    if (sascan->sa_mode != SMART_ANT_FORCED_FIXED) {
        if (!cell->delta_time)
            cell->delta_time = CONVERT_SYSTEM_TIME_TO_MS(cur_time - sascan->period_last_time);
        sascan->period_last_time = cur_time;
    }
}

static u_int8_t
ath_smartant_conf_antx_r(struct ath_sascan *sascan, u_int8_t conf, u_int8_t antx_conf)
{
    u_int8_t switch_com_r;

    if (sascan->antenna_table->info[conf].switch_com_ra1l1 == HAL_SWITCH_COM_ANTX) {
    	switch_com_r = sascan->antenna_table->info[antx_conf].switch_com_ra1l1;
    } else {
        switch_com_r = sascan->antenna_table->info[conf].switch_com_ra1l1;
    }
    return switch_com_r;
}

static u_int8_t
ath_smartant_conf_antx_t(struct ath_sascan *sascan, u_int8_t conf)
{
    u_int8_t switch_com_t;

    if (sascan->antenna_table->info[conf].switch_com_t1 == HAL_SWITCH_COM_ANTX) {
    	switch_com_t = sascan->antenna_table->info[sascan->best_tx_conf].switch_com_t1;
    } else {
        switch_com_t = sascan->antenna_table->info[conf].switch_com_t1;
    }
    return switch_com_t;
}

/*
 * Setup HAL antenna configuration.
 */
static void
ath_smartant_conf_update(struct ath_sascan *sascan,
                             u_int8_t rx_conf,
                             u_int8_t tx_conf)
{
    struct ath_softc *sc = sascan->sa_sc;
    struct ath_hal *ah = sc->sc_ah;
    HAL_ANT_COMB_CONFIG div_ant_conf;

    if (sascan->sa_mode == SMART_ANT_FORCED_FIXED) {
        return;
    }

    if (tx_conf >= sascan->antennaAntxCount)
        tx_conf = sascan->best_tx_conf;

    OS_MEMZERO(&div_ant_conf, sizeof(HAL_ANT_COMB_CONFIG));

    if (sc->sc_reg_parm.saFastDivDisable ||
        (sascan->sa_state != SMART_ANT_IDLE)) {

        /* Disable fast diversity */
        sc->sc_diversity = 0;
        ath_hal_setdiversity(ah, AH_FALSE);

        /* Disable LNA diversity */
        div_ant_conf.fast_div_disable = AH_TRUE;
    }

    div_ant_conf.main_lna_conf    = sascan->antenna_table->info[rx_conf].main_lna;
    div_ant_conf.alt_lna_conf     = HAL_ANT_DIV_COMB_LNA2;
    div_ant_conf.switch_com_r     = ath_smartant_conf_antx_r(sascan, rx_conf, sascan->antx_conf);
    div_ant_conf.switch_com_t     = ath_smartant_conf_antx_t(sascan, tx_conf);

    if (div_ant_conf.main_lna_conf == HAL_ANT_DIV_COMB_LNA2)
        div_ant_conf.alt_lna_conf = HAL_ANT_DIV_COMB_LNA1;

    ath_hal_setAntDivCombConf(ah, &div_ant_conf);
    sascan->cur_rx_conf = rx_conf;
    sascan->cur_tx_conf = tx_conf;
}

/*
 * Setup HAL antenna configuration for Tx.
 */
static void
ath_smartant_conf_update_tx(struct ath_sascan *sascan, u_int8_t conf)
{
    struct ath_softc *sc = sascan->sa_sc;
    struct ath_hal *ah = sc->sc_ah;
    HAL_ANT_COMB_CONFIG div_ant_conf;

    if (conf >= sascan->antennaAntxCount) return;

    OS_MEMZERO(&div_ant_conf, sizeof(HAL_ANT_COMB_CONFIG));

    div_ant_conf.main_lna_conf    = HAL_ANT_DIV_COMB_LNA_MASK;
    div_ant_conf.switch_com_t     = sascan->antenna_table->info[conf].switch_com_t1;

    ath_hal_setAntDivCombConf(ah, &div_ant_conf);
    sascan->cur_tx_conf = conf;
}

/*
 * Force fixed antenna diversity setting to HAL.
 */
void
ath_smartant_fixed(struct ath_sascan *sascan)
{
    struct ath_softc *sc = sascan->sa_sc;
    struct ath_hal *ah = sc->sc_ah;
    HAL_ANT_COMB_CONFIG div_ant_conf;

    if (sascan->sa_mode == SMART_ANT_FORCED_FIXED) {
        /*
         * Configure Rx
         */
        OS_MEMZERO(&div_ant_conf, sizeof(HAL_ANT_COMB_CONFIG));

        div_ant_conf.main_lna_conf    = sc->sc_reg_parm.saFixedMainLna;
        div_ant_conf.alt_lna_conf     = sc->sc_reg_parm.saFixedAltLna;
        div_ant_conf.fast_div_disable = sc->sc_reg_parm.saFastDivDisable;
        div_ant_conf.switch_com_r     = sc->sc_reg_parm.saFixedSwitchComR;
        div_ant_conf.switch_com_t     = sc->sc_reg_parm.saFixedSwitchComT;

        if (div_ant_conf.fast_div_disable) {
            /* Disable fast diversity */
            sc->sc_diversity = 0;
            ath_hal_setdiversity(ah, AH_FALSE);
        }
        ath_hal_setAntDivCombConf(ah, &div_ant_conf);
    }
    else {
        if (sascan->sa_state == SMART_ANT_IDLE)
            ath_smartant_conf_update(sascan, sascan->best_rx_conf, sascan->best_tx_conf);
        else if (sascan->sa_state == SMART_ANT_SCAN)
            ath_smartant_conf_update(sascan, sascan->cur_rx_conf, sascan->cur_tx_conf);
    }
}

void
ath_smartant_normal_scan_handle(ath_dev_t dev, u_int8_t scan_start)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ath_sascan *sascan = sc->sc_sascan;

    if (!sc->sc_smart_antenna || !sascan)
        return;

    if (scan_start) {
        if (sascan->idle_rx_info.data_cnt > SMART_ANT_LOW_TRAFFIC)
            return;

        ath_smartant_stop(sascan);

        do {
            sascan->sa_ap_conf = (sascan->sa_ap_conf + 1) % sascan->patternMaxCount;
        } while ((sascan->antenna_table->info[sascan->sa_ap_conf].main_lna == HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2) ||
             (sascan->antenna_table->info[sascan->sa_ap_conf].main_lna == HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2));

        ath_smartant_conf_update(sascan, sascan->sa_ap_conf, sascan->sa_ap_conf);
    }
    else {
        ath_smartant_start(sascan, sascan->bssid);
    }
}

void
ath_smartant_ap_list(struct ath_sascan *sascan, struct ieee80211_frame *wh, struct ath_rx_status *rx_stats)
{
#define AGING_TIMEOUT_MS    300000
    u_int8_t    main_rssi = rx_stats->rs_rssi_ctl0;
    struct ath_sa_aplist *ap=NULL, *next=NULL;
    systime_t   cur_time = OS_GET_TIMESTAMP();

    if ((sascan->antenna_table->info[sascan->cur_rx_conf].main_lna == HAL_ANT_DIV_COMB_LNA1_MINUS_LNA2) ||
        (sascan->antenna_table->info[sascan->cur_rx_conf].main_lna == HAL_ANT_DIV_COMB_LNA1_PLUS_LNA2)) {
        return;
    }

    TAILQ_FOREACH_SAFE(ap, &sascan->sa_aplist, ap_next, next){
        if (!OS_MEMCMP(ap->addr, wh->i_addr2, IEEE80211_ADDR_LEN)) {
            /* ap has the same bssid as wh->i_addr2 */
            TAILQ_REMOVE(&sascan->sa_aplist, ap, ap_next);
            break;
        }
        else if (CONVERT_SYSTEM_TIME_TO_MS(cur_time - ap->time_stamp) > AGING_TIMEOUT_MS) {
            TAILQ_REMOVE(&sascan->sa_aplist, ap, ap_next);
            OS_FREE(ap);
            sascan->sa_ap_count--;
        }
    }

    if (!ap) {
        ap = (struct ath_sa_aplist *)OS_MALLOC(sascan->sa_sc->sc_osdev, sizeof(struct ath_sa_aplist), GFP_KERNEL);
        OS_MEMZERO(ap, sizeof(struct ath_sa_aplist));
        OS_MEMCPY(ap->addr, wh->i_addr2, IEEE80211_ADDR_LEN);
        sascan->sa_ap_count++;
    }
    ap->time_stamp = cur_time;

    if (ap->rssi[sascan->cur_rx_conf])
        ap->rssi[sascan->cur_rx_conf] = (main_rssi >> 1) + (ap->rssi[sascan->cur_rx_conf] >> 1);
    else
        ap->rssi[sascan->cur_rx_conf] = main_rssi;

    TAILQ_INSERT_TAIL(&sascan->sa_aplist, ap, ap_next);
}

void
ath_smartant_get_apconf(struct ath_sascan *sascan, const u_int8_t *bssid)
{
    u_int16_t j, ap_ant_conf=0, tx_ant_conf=0;
    struct ath_sa_aplist *ap=NULL, *next=NULL;

    TAILQ_FOREACH_SAFE(ap, &sascan->sa_aplist, ap_next, next) {
        if (!OS_MEMCMP(ap->addr, bssid, IEEE80211_ADDR_LEN)) {
            /* bssid is the same as ap */
            for (j=0; j<sascan->patternMaxCount; j++) {
                if (ap->rssi[ap_ant_conf] < ap->rssi[j]) {
                    ap_ant_conf = j;
                    if (ap_ant_conf < sascan->antennaAntxCount)
                        tx_ant_conf = ap_ant_conf;
                }
            }

            ath_smartant_conf_update(sascan, ap_ant_conf, tx_ant_conf);
            break;
        }
    }
}

void
ath_smartant_start(struct ath_sascan *sascan, const u_int8_t *bssid)
{
    if ((sascan->sa_mode == SMART_ANT_FORCED_FIXED) ||
        (sascan->sa_state != SMART_ANT_INIT)) {
        return;
    }
    OS_MEMCPY(sascan->bssid, bssid, IEEE80211_ADDR_LEN);
    ath_smartant_get_apconf(sascan, bssid);
    ath_smartant_idle_start(sascan);
}

void
ath_smartant_stop(struct ath_sascan *sascan)
{
    if ((sascan->sa_mode == SMART_ANT_FORCED_FIXED) ||
        (sascan->sa_state == SMART_ANT_INIT)) {
        return;
    }
    OS_CANCEL_TIMER(&sascan->sa_scan_timer);
    OS_CANCEL_TIMER(&sascan->sa_idle_timer);
    OS_CANCEL_TIMER(&sascan->sa_check_timer);
    sascan->sa_state = SMART_ANT_INIT;
}

static u_int8_t
ath_smartant_find_next(struct ath_sascan *sascan)
{
    u_int8_t _antenna_pattern = 0;

    if (!sascan->scan_rx_info[sascan->antx_conf].visited) {
        _antenna_pattern = sascan->antx_conf;
    }
    else {
        while (_antenna_pattern < sascan->patternMaxCount) {
            if (!sascan->scan_rx_info[_antenna_pattern].visited)
                break;
            else
                _antenna_pattern++;
        }
    }
    sascan->scan_rx_info[_antenna_pattern].visited = 1;
    return _antenna_pattern;
}
static void ath_smartant_history(struct ath_sascan *sascan)
{
	int i;
    systime_t cur_time;
    u_int32_t scan_Start2End_time = 0;

    cur_time = OS_GET_TIMESTAMP();
    scan_Start2End_time = CONVERT_SYSTEM_TIME_TO_MS(cur_time - sascan->scan_time);
    sascan->scan_time = cur_time;

    /* Rx antenna diversity history record */
    sascan->h_r_id[sascan->h_r_idx] = sascan->sa_r_id;
    sascan->sa_r_id++;

    sascan->h_ant_conf[sascan->h_r_idx] = sascan->best_rx_conf;
    sascan->h_antx_conf[sascan->h_r_idx] = sascan->antx_conf;
    sascan->h_scan_Start2End_time[sascan->h_r_idx] = scan_Start2End_time;

    sascan->h_r_idx = (sascan->h_r_idx + 1) & (SMART_ANT_HISTORY_NUM - 1);

    DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
    DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "History of Rx antenna selected:\n");
    for (i = sascan->h_r_idx; i < SMART_ANT_HISTORY_NUM ; i++) {
        DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "id[%03d] = Pattern[%d] S2E[%06d] Lna[%s] Ant[%s]\n",
                                        sascan->h_r_id[i],
                                        sascan->h_ant_conf[i],
                                        sascan->h_scan_Start2End_time[i],
                                        sascan->antenna_table->info[sascan->h_ant_conf[i]].lna_str,
                                        sascan->antenna_table->info[sascan->h_antx_conf[i]].ant_str);
    }
    for (i = 0; i < sascan->h_r_idx ; i++) {
        DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "id[%03d] = Pattern[%d] S2E[%06d] Lna[%s] Ant[%s]\n",
                                        sascan->h_r_id[i],
                                        sascan->h_ant_conf[i],
                                        sascan->h_scan_Start2End_time[i],
                                        sascan->antenna_table->info[sascan->h_ant_conf[i]].lna_str,
                                        sascan->antenna_table->info[sascan->h_antx_conf[i]].ant_str);
    }

    DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");
}

static u_int8_t
ath_smartant_get_antenna_index(struct ath_sascan *sascan, u_int8_t rx_ant_conf, u_int8_t rx_switch_com)
{
    u_int8_t i;
    u_int8_t antenna_index = sascan->patternMaxCount;

    /*
     * Known issue:
     * If rx_switch_com be don't care, then switch_com_t1 got SW latency.
     */
    for (i = 0; i < sascan->patternMaxCount; i++) {
        if ((rx_ant_conf == sascan->antenna_table->info[i].main_lna) &&
            ((rx_switch_com == ath_smartant_conf_antx_r(sascan, i, sascan->antx_conf)) ||
            /*
             * Workaround: when lna not LNA1, the switch com from Rx status always be ANT1
             */
        	 (sascan->antenna_table->info[i].switch_com_ra1l1 == HAL_SWITCH_COM_ANTX) ||
        	 (sascan->antenna_table->info[i].switch_com_ra1l1 == HAL_SWITCH_COM_MASK)) &&
        	((sascan->antenna_table->info[sascan->cur_rx_conf].switch_com_t1 == sascan->antenna_table->info[i].switch_com_t1) ||
        	 (sascan->antenna_table->info[i].switch_com_t1 == HAL_SWITCH_COM_MASK))){
            antenna_index = i;
        }
    }
    return antenna_index;
}

/*
 * Find the best rx antenna configuration
 */
static u_int8_t
ath_smartant_best_rx(struct ath_sascan *sascan,
                      struct ath_sa_info *cell,
                      u_int8_t max_num,
                      u_int8_t current_pattern)
{
    int i;
    u_int8_t  best_idx = current_pattern;
    u_int32_t best_value = cell[current_pattern].avg_phyrate;

    for (i=0; i<max_num; i++) {
        if (best_value < cell[i].avg_phyrate) {
            best_value = cell[i].avg_phyrate;
            best_idx = i;
        }
    }
    return best_idx;
}

/*
 * if the performance of scan pattern is worse than rx_phyrate_thresh1/rx_phyrate_thresh2
 *    cut off and switch to the last best antenna config.
 * For rate control of AP side, so we switch to the last best antenna,
 *    keep the good performance of next scan pattern start.
 */
static void
ath_smartant_bad_rx_check(struct ath_sascan *sascan)
{
	/*
	 * - protect last best antenna config
	 * - avoid false alarm because one packet with low rate in the early time of scan antenna pattern.
	 *   data_cnt >= SMART_ANT_PKT_THRESH
	 * - if Antx enable, protect the antx_conf
	 */
	OS_CANCEL_TIMER(&sascan->sa_check_timer);

    if (sascan->scan_rx_info[sascan->antenna_pattern].avg_phyrate < sascan->rx_phyrate_thresh1) {
            /*
             * bad configuration detect, add skip_cnt for current antenna pattern
             *  #(skip_cnt) run antenna diversity scan competition in the future .
             */
        	if (!sascan->scan_rx_info[sascan->antenna_pattern].f_bad_detect) {
                sascan->scan_rx_info[sascan->antenna_pattern].f_bad_detect = 1;

                if (sascan->scan_rx_info[sascan->antenna_pattern].avg_phyrate < sascan->rx_phyrate_thresh2) {
                    sascan->antenna_skip_cnt[sascan->antenna_pattern] += 3;
                } else {
                    sascan->antenna_skip_cnt[sascan->antenna_pattern] += 2;
                }

                if (sascan->antenna_skip_cnt[sascan->antenna_pattern] > SMART_ANT_SKIP_MAX) {
                    sascan->antenna_skip_cnt[sascan->antenna_pattern] = SMART_ANT_SKIP_MAX;
                }
            }

            /*
             * remaining received in this pattern scan period would use the best_rx_conf,
             * for recover target rate control.
             */
            ath_smartant_conf_update(sascan, sascan->best_rx_conf, sascan->cur_tx_conf);
            sascan->bad_rx_detected = AH_TRUE;
            ath_smartant_get_time(sascan, &sascan->scan_rx_info[sascan->antenna_pattern]);

    } /* bad configuration fast detect  */
}


static void
ath_smartant_bad_tx_check(struct ath_sascan *sascan)
{
    if (sascan->scan_tx_info[sascan->cur_tx_conf].avg_phyrate < sascan->tx_phyrate_thresh) {
        sascan->bad_tx_detected = AH_TRUE;
        ath_smartant_conf_update_tx(sascan, sascan->best_tx_conf);
    } /* bad configuration fast detect  */
}

static void
ath_smartant_antx_scan_end(struct ath_sascan *sascan)
{
    int i;

    /* find the best antenna */
    sascan->antx_conf = ath_smartant_best_rx(sascan,
                                         &sascan->scan_rx_info[0],
                                         sascan->antennaAntxCount,
                                         sascan->antx_conf);
    if (sascan->old_antx_conf != sascan->antx_conf) {
        /*the antx is new guy, reset  skip cnt of LNA scan patterns */
        for (i = sascan->antennaAntxCount; i < sascan->patternMaxCount; i++)
            sascan->antenna_skip_cnt[i]= 0;
    }
    sascan->antenna_skip_cnt[sascan->antx_conf] = 0;


    /* tx antenna scan */
    for (i=0; i < sascan->antennaAntxCount; i++) {
        if ((sascan->scan_tx_info[sascan->best_tx_conf].avg_phyrate < sascan->scan_tx_info[i].avg_phyrate) &&
            ((sascan->scan_tx_info[i].data_cnt > SMART_ANT_PKT_THRESH) ||
             (sascan->scan_tx_info[sascan->best_tx_conf].data_cnt < SMART_ANT_PKT_THRESH))) {
            sascan->best_tx_conf = i;
        }
    }

    DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "[id] [avg_rate,cnt] [time] [skip cnt][skip, bad] \n");

    for (i = 0; i < sascan->antennaAntxCount; i++) {
    	DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "[%02d] [%06d, %03d] [%05d] [%d][%d,  %d] \n",
                                                i,
                                                sascan->scan_rx_info[i].avg_phyrate,
                                                sascan->scan_rx_info[i].data_cnt,
                                                sascan->scan_rx_info[i].delta_time,
                                                sascan->antenna_skip_cnt[i],
                                                sascan->scan_rx_info[i].f_skip,
                                                sascan->scan_rx_info[i].f_bad_detect);
    }

    DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "###############################################################\n");
}

/*
 * scan period setup (start antenna pattern index)
 */
static void
ath_smartant_scan_next(struct ath_sascan *sascan)
{
    u_int8_t    _skip_this_run = 0;
    u_int8_t    _antenna_pattern;

    OS_CANCEL_TIMER(&sascan->sa_scan_timer);
    OS_CANCEL_TIMER(&sascan->sa_check_timer);

    /* find out next antenna pattern */
    while ((_antenna_pattern = ath_smartant_find_next(sascan)) < sascan->patternMaxCount) {
        if (_antenna_pattern == sascan->antennaAntxCount)
            ath_smartant_antx_scan_end(sascan);

        if (sascan->antenna_skip_cnt[_antenna_pattern] == 0) {
            break;
        }
        else {
            /* bad configuration, count down and bypass this pattern */
            sascan->antenna_skip_cnt[_antenna_pattern]--;
            sascan->scan_rx_info[_antenna_pattern].f_skip = 1;
        }
    }

    if (_antenna_pattern != sascan->antx_conf) {
        if (sascan->best_rx_phyrate < sascan->scan_rx_info[sascan->antenna_pattern].avg_phyrate) {
            sascan->best_rx_phyrate = sascan->scan_rx_info[sascan->antenna_pattern].avg_phyrate;

            /* bad configuration fast detect thres1 = best_rx_phyrate * 0.75 */
            sascan->rx_phyrate_thresh1 = (sascan->best_rx_phyrate >> 1) + (sascan->best_rx_phyrate >> 2);

            /* bad configuration fast detect thres2 = best_rx_phyrate * 0.5 */
            sascan->rx_phyrate_thresh2 = (sascan->best_rx_phyrate >> 1);
        }

        if ((sascan->antenna_pattern < sascan->antennaAntxCount) &&
            (sascan->best_tx_phyrate < sascan->scan_tx_info[sascan->antenna_pattern].avg_phyrate)) {
            sascan->best_tx_phyrate = sascan->scan_tx_info[sascan->antenna_pattern].avg_phyrate;
            sascan->tx_phyrate_thresh = (sascan->best_tx_phyrate >> 1) + (sascan->best_tx_phyrate >> 2);
        }

        ath_smartant_get_time(sascan, &sascan->scan_rx_info[sascan->antenna_pattern]);
    }

    sascan->antenna_pattern = _antenna_pattern;
    sascan->bad_rx_detected = AH_FALSE;
    sascan->bad_tx_detected = AH_FALSE;

    if (sascan->antenna_pattern >= sascan->patternMaxCount) {
        ath_smartant_scan_end(sascan);
    } else {
        /* setup with the well configuration */
        ath_smartant_conf_update(sascan, sascan->antenna_pattern, sascan->antenna_pattern);
        OS_SET_TIMER(&sascan->sa_scan_timer, SMART_ANT_SCAN_TIME);
        OS_SET_TIMER(&sascan->sa_check_timer, SMART_ANT_CHECK_BAD_TIME);
    }
}

static void
ath_smartant_scan_start(struct ath_sascan *sascan)
{

    if ((sascan->sa_state == SMART_ANT_INIT) || (sascan->sa_mode == SMART_ANT_FORCED_FIXED))
        return;

    OS_CANCEL_TIMER(&sascan->sa_idle_timer);
    OS_MEMZERO(sascan->scan_rx_info,
               sizeof(struct ath_sa_info) * SMART_ANT_TABLE_MAX);
    OS_MEMZERO(sascan->scan_tx_info,
               sizeof(struct ath_sa_info) * SMART_ANT_TABLE_MAX);

    sascan->scan_time           = OS_GET_TIMESTAMP();
    sascan->period_last_time    = OS_GET_TIMESTAMP();
    sascan->sa_state            = SMART_ANT_SCAN;
    sascan->keep_same_cnt       = 0;
    sascan->bad_rx_detected     = AH_FALSE;
    sascan->bad_tx_detected     = AH_FALSE;
    sascan->best_rx_phyrate     = 0;
    sascan->best_tx_phyrate     = 0;
    sascan->rx_phyrate_thresh1  = 0;
    sascan->rx_phyrate_thresh2  = 0;
    sascan->tx_phyrate_thresh   = 0;

    ath_smartant_scan_next(sascan);
}

/*
 * SCAN end, sort the avg_phyrate,
 * select and setup the best antenna configuration.
 */
static void
ath_smartant_scan_end(struct ath_sascan *sascan)
{
    int i;
    u_int8_t    best_id;

    if (!sascan->scan_rx_info[sascan->patternMaxCount-1].f_skip) {
        ath_smartant_get_time(sascan, &sascan->scan_rx_info[sascan->antenna_pattern]);
    }

    /* find the best antenna */
    best_id = ath_smartant_best_rx(sascan,
                                &sascan->scan_rx_info[0],
                                sascan->patternMaxCount,
                                sascan->best_rx_conf);

    /* Assign keep_same_granted */
    if ((best_id == sascan->best_rx_conf) && (sascan->old_antx_conf == sascan->antx_conf)) {
        if (sascan->keep_same_granted < SMART_ANT_MAX_IDLE)
            sascan->keep_same_granted = sascan->keep_same_granted * 2;
    } else {
        /* new guy, reset grant to 1 */
        sascan->keep_same_granted = 1;
        sascan->best_rx_conf = best_id;
        sascan->old_antx_conf  = sascan->antx_conf;
    }
    sascan->antenna_skip_cnt[sascan->best_rx_conf] = 0;
    ath_smartant_conf_update(sascan, sascan->best_rx_conf, sascan->best_tx_conf);
    ath_smartant_idle_start(sascan);

    DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "[id] [avg_rate,cnt] [time] [skip cnt][skip, bad] \n");
    for (i=0; i<sascan->patternMaxCount; i++) {
    	DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "[%02d] [%06d, %03d] [%05d] [%d][%d,  %d] \n",
                                                i,
                                                sascan->scan_rx_info[i].avg_phyrate,
                                                sascan->scan_rx_info[i].data_cnt,
                                                sascan->scan_rx_info[i].delta_time,
                                                sascan->antenna_skip_cnt[i],
                                                sascan->scan_rx_info[i].f_skip,
                                                sascan->scan_rx_info[i].f_bad_detect);
    }

    DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "%s_id[%03d] Rx [%d][%s (%s)] total selected by rx_scan_2\n",
                 __func__, sascan->sa_r_id,
                           sascan->best_rx_conf,
                           sascan->antenna_table->info[sascan->best_rx_conf].lna_str,
                           sascan->antenna_table->info[sascan->antx_conf].ant_str);
    for (i=0; i < sascan->antennaAntxCount; i++) {
        DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "tx_conf[%d] phyrate: %d, data_cnt: %d\n", i, sascan->scan_tx_info[i].avg_phyrate, sascan->scan_tx_info[i].data_cnt);
    }
    DPRINTF(sascan->sa_sc, ATH_DEBUG_ANY, "best_tx_conf: %d\n", sascan->best_tx_conf);
    ath_smartant_history(sascan);
}

static void
ath_smartant_idle_start(struct ath_sascan *sascan)
{
    sascan->sa_state = SMART_ANT_IDLE;

    if (sascan->idle_rx_info.data_cnt < SMART_ANT_LOW_TRAFFIC)
        ath_smartant_get_apconf(sascan, sascan->bssid);

    OS_MEMZERO(&sascan->idle_rx_info, sizeof(struct ath_sa_info));
    OS_SET_TIMER(&sascan->sa_idle_timer, SMART_ANT_IDLE_TIME);
}

/*
 * Antenna idle end:
 *   If performance of this idle period is not stable, then goto antenna scan start,
 *   else (stable) keep this antenna configuration while(keep_same_cnt <= keep_same_granted).
 */

static void
ath_smartant_idle_end(struct ath_sascan *sascan)
{
    struct ath_softc *sc = sascan->sa_sc;

    ath_smartant_get_time(sascan, &sascan->idle_rx_info);
    sascan->keep_same_cnt++;
    if ((sascan->idle_rx_info.avg_phyrate >= (sascan->best_rx_phyrate * SMART_ANT_LOWER_FACTOR / 100)) &&
    	(sascan->idle_rx_info.avg_phyrate <= (sascan->best_rx_phyrate * SMART_ANT_UPPER_FACTOR / 100)) &&
    	(sascan->keep_same_cnt < sascan->keep_same_granted) ||
    	(sascan->idle_rx_info.data_cnt < SMART_ANT_LOW_TRAFFIC)) {
        /* Keep this antenna setting,  */
        OS_MEMZERO(sascan->antenna_skip_cnt,
               sizeof(u_int8_t) * SMART_ANT_TABLE_MAX);
        ath_smartant_idle_start(sascan);
    } else {
        ath_smartant_scan_start(sascan);
    }
}

static void
ath_smartant_phyrate_update(struct ath_sa_info *cell, int phyrate)
{
    cell->data_cnt++;

    if(!cell->avg_phyrate)
        /* first received */
        cell->avg_phyrate = phyrate;
    else
        /* avg = (avg * 7 + phyrate) / 8 */
        cell->avg_phyrate = ((cell->avg_phyrate - (cell->avg_phyrate >> 3)) + (phyrate >> 3));
}

/*
 * rx diversity scan
 */
void
ath_smartant_rx_scan(struct ath_sascan *sascan, struct ieee80211_frame *wh, struct ath_rx_status *rx_stats, int phyrate)
{
    u_int8_t rx_ant_conf, main_ant_conf, alt_ant_conf, cur_ant_conf;
    u_int8_t rx_switch_com;

    if (IEEE80211_IS_BEACON(wh))
        ath_smartant_ap_list(sascan, wh, rx_stats);

    if ((sascan->sa_state == SMART_ANT_INIT) || (sascan->sa_mode == SMART_ANT_FORCED_FIXED))
        return;

    /* Data Collection */
    rx_ant_conf   = (rx_stats->rs_rssi_ctl2 >> SMART_ANT_CUR_CONF_S)  & SMART_ANT_CONF_MASK;
    main_ant_conf = (rx_stats->rs_rssi_ctl2 >> SMART_ANT_MAIN_CONF_S) & SMART_ANT_CONF_MASK;
    alt_ant_conf  = (rx_stats->rs_rssi_ctl2 >> SMART_ANT_ALT_CONF_S)  & SMART_ANT_CONF_MASK;
    rx_switch_com = rx_stats->rs_rssi_ext2 & SMART_ANT_SWITCH_MASK;
    /*
     * - Check the frame recv from which ant configure and hw ant configure.
     *   ( ps: disable antenna fast diversity, check if rx_ant_conf and main_ant_conf match )
     * - Accept single or last MPDU.
     */
    if ((rx_ant_conf != main_ant_conf) || rx_stats->rs_moreaggr )
	    return;

    /* Update received information */
    if (sascan->sa_state == SMART_ANT_SCAN) {
        cur_ant_conf = ath_smartant_get_antenna_index(sascan, rx_ant_conf, rx_switch_com);
        if ((cur_ant_conf == sascan->antenna_pattern) && !sascan->bad_rx_detected &&
            IEEE80211_IS_DATA(wh)) {
            ath_smartant_phyrate_update(&sascan->scan_rx_info[cur_ant_conf], phyrate);
            if (sascan->scan_rx_info[cur_ant_conf].data_cnt >= SMART_ANT_PKT_THRESH)
                ath_smartant_bad_rx_check(sascan);
        }
    } else {
    	/* idle state */
        if (IEEE80211_IS_DATA(wh)) {
            ath_smartant_phyrate_update(&sascan->idle_rx_info, phyrate);
        }
    }
}

void
ath_smartant_tx_scan(struct ath_sascan *sascan, int phyrate)
{
	/* tx antenna diversity */
    if ((sascan->sa_mode == SMART_ANT_FORCED_FIXED) ||
        (sascan->sa_state != SMART_ANT_SCAN)) {
        return;
    }

    if ((sascan->cur_tx_conf >= sascan->antennaAntxCount) || sascan->bad_tx_detected ||
        (sascan->antenna_pattern >= sascan->antennaAntxCount)) {
        return;
    }

    ath_smartant_phyrate_update(&sascan->scan_tx_info[sascan->cur_tx_conf], phyrate);

    if (sascan->scan_tx_info[sascan->cur_tx_conf].data_cnt >= SMART_ANT_PKT_THRESH)
        ath_smartant_bad_tx_check(sascan);

    return;
}

/*
 * For scan pattern
 */
static
OS_TIMER_FUNC(ath_smartant_scan_handler)
{
    struct ath_softc *sc;
    struct ath_sascan *sascan;
    u_int8_t scan_ant_conf;
    int i;

    OS_GET_TIMER_ARG(sc, struct ath_softc *);
    sascan = sc->sc_sascan;

    if (sc->sc_scanning)
        ath_smartant_idle_start(sascan);

    if (sascan->sa_state != SMART_ANT_IDLE)
        ath_smartant_scan_next(sascan);
}

static
OS_TIMER_FUNC(ath_smartant_idle_handler)
{
    struct ath_softc *sc;
    struct ath_sascan *sascan;

    OS_GET_TIMER_ARG(sc, struct ath_softc *);
    sascan = sc->sc_sascan;
    ath_smartant_idle_end(sascan);
}

static
OS_TIMER_FUNC(ath_smartant_check_handler)
{
    struct ath_softc *sc;
    struct ath_sascan *sascan;

    OS_GET_TIMER_ARG(sc, struct ath_softc *);
    sascan = sc->sc_sascan;

    if (!sascan->bad_rx_detected)   ath_smartant_bad_rx_check(sascan);
    if (!sascan->bad_tx_detected)   ath_smartant_bad_tx_check(sascan);
}

u_int32_t ath_smartant_query(ath_dev_t dev, u_int32_t *buffer)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    struct ath_sascan   *sascan = sc->sc_sascan;
    struct ath_sa_query_info    *smartAntInfo = (struct ath_sa_query_info *)buffer;
    struct ath_sa_aplist *ap=NULL, *next=NULL;
    u_int32_t   itemlen;

    if (!sascan) {
        smartAntInfo->sa_attached = 0;
        return sizeof(struct ath_sa_query_info);
    }
    smartAntInfo->sa_attached = 1;

    if (sascan->sa_mode == SMART_ANT_FORCED_FIXED) {
        smartAntInfo->sa_started = 0;
        return sizeof(struct ath_sa_query_info);
    }
    smartAntInfo->sa_started        = (sascan->sa_state == SMART_ANT_INIT)? 0:1;
    smartAntInfo->sa_ap_count       = sascan->sa_ap_count;
    smartAntInfo->sa_current_ant    = sascan->antx_conf;
    OS_MEMCPY(smartAntInfo->bssid, sascan->bssid, IEEE80211_ADDR_LEN);

    itemlen = 0;
    TAILQ_FOREACH_SAFE(ap, &sascan->sa_aplist, ap_next, next) {
        OS_MEMCPY(smartAntInfo->sa_ap_list + itemlen, ap->addr, IEEE80211_ADDR_LEN);
        itemlen += IEEE80211_ADDR_LEN;
        OS_MEMCPY(smartAntInfo->sa_ap_list + itemlen, ap->rssi, SMART_ANT_TABLE_MAX);
        itemlen += (SMART_ANT_TABLE_MAX);
    }
    return sizeof(struct ath_sa_query_info) + itemlen;
}


void ath_smartant_config(ath_dev_t dev, u_int32_t *buffer)
{
    struct ath_softc    *sc = ATH_DEV_TO_SC(dev);
    struct ath_sascan   *sascan = sc->sc_sascan;
    struct ath_sa_config_info   *smartAntConfigInfo = (struct ath_sa_config_info *)buffer;

    if (!sascan) return;

    if (smartAntConfigInfo->saAlg == SMART_ANT_FORCED_AUTO) {
        /* restart SA scan */
        sascan->sa_mode = SMART_ANT_FORCED_AUTO;
        ath_smartant_start(sascan, sascan->bssid);
    } else if (smartAntConfigInfo->saAlg == SMART_ANT_FORCED_FIXED){
        sascan->sa_mode                     = SMART_ANT_FORCED_FIXED;
        sc->sc_reg_parm.saFixedMainLna      = smartAntConfigInfo->saFixedMainLna;
        sc->sc_reg_parm.saFixedSwitchComR   = smartAntConfigInfo->saFixedSwitchComR;
        sc->sc_reg_parm.saFixedSwitchComT   = smartAntConfigInfo->saFixedSwitchComT;
        ath_smartant_stop(sascan);
        ath_smartant_fixed(sascan);
    }
}


#endif /* ATH_ANT_DIV_COMB */
