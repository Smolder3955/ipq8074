/*****************************************************************************/
/* \file ath_config.c
** \brief Configuration code for controlling the ATH layer.
**
**  This file contains the routines used to configure and control the ATH
**  object once instantiated.
**
**
 * Copyright (c) 2009, Atheros Communications Inc.
 * All Rights Reserved.
 *
 * Copyright (c) 2011, 2017 Qualcomm Innovation Center, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Innovation Center, Inc.
 *
 * 2011 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 *
*/

#include "ath_internal.h"
#include <ieee80211_ioctl_acfg.h>
#if UNIFIED_SMARTANTENNA
#include <target_if_sa_api.h>
#endif /* UNIFIED_SMARTANTENNA */
#if QCA_AIRTIME_FAIRNESS
#include <target_if_atf.h>
#endif /* QCA_AIRTIME_FAIRNESS */
/******************************************************************************/
/*!
**  \brief Set ATH configuration parameter
**
**  This routine will set ATH parameters to the provided values.  The incoming
**  values are always encoded as integers -- conversions are done here.
**
**  \param dev Handle for the ATH device
**  \param ID Parameter ID value, as defined in the ath_param_ID_t type
**  \param buff Buffer containing the value, currently always an integer
**  \return 0 on success
**  \return -1 for unsupported values
*/

int
ath_set_config(ath_dev_t dev, ath_param_ID_t ID, void *buff)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
    struct ieee80211com *ic = (struct ieee80211com*)(sc->sc_ieee);
#if QCA_AIRTIME_FAIRNESS
    struct wlan_objmgr_pdev *pdev = ic->ic_pdev_obj;
    struct wlan_objmgr_psoc *psoc = NULL;
    int atf_ssidgroup = 0;
#endif /* QCA_AIRTIME_FAIRNESS */
    int retval = 0;
    u_int32_t hal_chainmask;
    int val, ldpcsupport;
    int value;
#if DBDC_REPEATER_SUPPORT
    struct ieee80211com *tmp_ic = NULL;
    int i = 0;
    struct ieee80211com *fast_lane_ic = NULL;
#endif
#if ATH_BT_COEX_3WIRE_MODE
    struct ath_hal *ah = sc->sc_ah;
    u_long bt_weight;
#endif
    switch(ID)
    {
        case ATH_PARAM_TXCHAINMASK:
             (void) ath_hal_gettxchainmask(sc->sc_ah, &hal_chainmask);
             if (!*(u_int32_t *)buff) {
                 sc->sc_config.txchainmask = hal_chainmask;
             } else {
                 sc->sc_config.txchainmask = *(u_int32_t *)buff & hal_chainmask;
             }
        break;

        case ATH_PARAM_RXCHAINMASK:
             (void) ath_hal_getrxchainmask(sc->sc_ah, &hal_chainmask);
             if (!*(u_int32_t *)buff) {
                 sc->sc_config.rxchainmask = hal_chainmask;
             } else {
                 sc->sc_config.rxchainmask = *(u_int32_t *)buff & hal_chainmask;
             }
        break;

        case ATH_PARAM_TXCHAINMASKLEGACY:
            sc->sc_config.txchainmasklegacy = *(int *)buff;
        break;

        case ATH_PARAM_RXCHAINMASKLEGACY:
            sc->sc_config.rxchainmasklegacy = *(int *)buff;
        break;

        case ATH_PARAM_CHAINMASK_SEL:
            sc->sc_config.chainmask_sel = *(int *)buff;
        break;

        case ATH_PARAM_AMPDU:
            sc->sc_txaggr = *(int *)buff;
        break;

        case ATH_PARAM_AMPDU_LIMIT:
            sc->sc_config.ampdu_limit = *(int *)buff;
        break;

        case ATH_PARAM_AMPDU_SUBFRAMES:
            sc->sc_config.ampdu_subframes = *(int *)buff;
        break;

        case ATH_PARAM_AMPDU_RX_BSIZE:
            sc->sc_config.ampdu_rx_bsize = *(int *)buff;
        break;

        case ATH_PARAM_AGGR_PROT:
            sc->sc_config.ath_aggr_prot = *(int *)buff;
        break;

        case ATH_PARAM_AGGR_PROT_DUR:
            sc->sc_config.ath_aggr_prot_duration = *(int *)buff;
        break;

        case ATH_PARAM_AGGR_PROT_MAX:
            sc->sc_config.ath_aggr_prot_max = *(int *)buff;
        break;

        case ATH_PARAM_AGGR_BURST:
            sc->sc_aggr_burst = *(int *)buff;
            if (sc->sc_aggr_burst) {
                ath_txq_burst_update(sc, HAL_WME_AC_BE, ATH_BURST_DURATION);
            } else {
                ath_txq_burst_update(sc, HAL_WME_AC_BE, 0);
            }

            sc->sc_aggr_burst_duration = ATH_BURST_DURATION;
        break;

        case ATH_PARAM_AGGR_BURST_DURATION:
            if (sc->sc_aggr_burst) {
                sc->sc_aggr_burst_duration = *(int *)buff;
                ath_txq_burst_update(sc, HAL_WME_AC_BE,
                                     sc->sc_aggr_burst_duration);
            } else {
                printk("Error: enable bursting before setting duration\n");
            }
        break;

        case ATH_PARAM_TXPOWER_LIMIT2G:
            if (*(int *)buff > ATH_TXPOWER_MAX_2G) {
                retval = -1;
            } else {
                sc->sc_config.txpowlimit2G = *(int *)buff;
            }
        break;

        case ATH_PARAM_TXPOWER_LIMIT5G:
            if (*(int *)buff > ATH_TXPOWER_MAX_5G) {
                retval = -1;
            } else {
                sc->sc_config.txpowlimit5G = *(int *)buff;
            }
        break;

        case ATH_PARAM_TXPOWER_OVERRIDE:
            sc->sc_config.txpowlimit_override = *(int *)buff;
        break;

        case ATH_PARAM_PCIE_DISABLE_ASPM_WK:
            sc->sc_config.pcieDisableAspmOnRfWake = *(int *)buff;
        break;

        case ATH_PARAM_PCID_ASPM:
            sc->sc_config.pcieAspm = *(int *)buff;
        break;

        case ATH_PARAM_BEACON_NORESET:
            sc->sc_noreset = *(int *)buff;
        break;

        case ATH_PARAM_CAB_CONFIG:
            sc->sc_config.cabqReadytime = *(int *)buff;
            ath_cabq_update(sc);
        break;

        case ATH_PARAM_ATH_DEBUG:
        {
            int cat, mask = *(int *)buff, debug_any = 0;
            for (cat = 0; cat < ATH_DEBUG_NUM_CATEGORIES; cat++) {
                int category_mask = (1 << cat) & mask;
                asf_print_mask_set(&sc->sc_print, cat, category_mask);
                debug_any = category_mask ? 1 : debug_any;
            }
            /* Update the ATH_DEBUG_ANY debug mask bit */
            asf_print_mask_set(&sc->sc_print, ATH_DEBUG_ANY, debug_any);
        }
        break;
#if ATH_BT_COEX_3WIRE_MODE
        case ATH_PARAM_ATH_3WIRE_BT_COEX:
        {
            val = *(int *)buff;
            if (val == 1) {
                sc->sc_3wire_bt_coex_enable = val;
                ah->ah_3wire_bt_coex_enable = val;
                if (!ath_hal_enable_basic_3wire_btcoex(sc->sc_ah)) {
                    printk("BT_COEX 3WIRE MODE Enabled \n");
                    printk("WL Weight: 0x%lx BT Weight:0x%lx\n",
                            ah->ah_bt_coex_wl_weight,ah->ah_bt_coex_bt_weight);
                } else {
                    printk(" BT_COEX not supported for this Radio \n");
                }
            } else if (val == 0) {
                sc->sc_3wire_bt_coex_enable = val;
                ah->ah_3wire_bt_coex_enable = val;
                if (!ath_hal_disable_basic_3wire_btcoex(sc->sc_ah)) {
                    printk(" BT_COEX 3WIRE MODE Disasbled \n");
                } else {
                    printk(" BT_COEX not supported for this Radio \n");
                }
            } else {
                printk("Enter: 1 to Enable BT_COEX , 0 to Disable the BT_COEX \n");
            }
        }
        break;

        case ATH_PARAM_ATH_3WIRE_BT_WEIGHT:
            bt_weight = *(unsigned long *)buff;
            ah->ah_bt_coex_bt_weight = bt_weight;
            printk("bt_weight = 0x%lx\n", ah->ah_bt_coex_bt_weight);
        break;

        case ATH_PARAM_ATH_3WIRE_WL_WEIGHT:
            bt_weight = *(unsigned long *)buff;
            ah->ah_bt_coex_wl_weight = bt_weight;
            printk("wl_weight = 0x%lx\n", ah->ah_bt_coex_wl_weight);
            break;
#endif

        case ATH_PARAM_ATH_TPSCALE:
           sc->sc_config.tpscale = *(int *)buff;
           ath_update_tpscale(sc);
        break;

        case ATH_PARAM_ACKTIMEOUT:
           /* input is in usec */
           ath_hal_setacktimeout(sc->sc_ah, (*(int *)buff));
        break;

        case ATH_PARAM_RIMT_FIRST:
           /* input is in usec */
           ath_hal_setintrmitigation_timer(sc->sc_ah, HAL_INT_RX_FIRSTPKT, (*(int *)buff));
        break;

        case ATH_PARAM_RIMT_LAST:
           /* input is in usec */
           ath_hal_setintrmitigation_timer(sc->sc_ah, HAL_INT_RX_LASTPKT, (*(int *)buff));
        break;

        case ATH_PARAM_TIMT_FIRST:
           /* input is in usec */
           ath_hal_setintrmitigation_timer(sc->sc_ah, HAL_INT_TX_FIRSTPKT, (*(int *)buff));
        break;

        case ATH_PARAM_TIMT_LAST:
           /* input is in usec */
           ath_hal_setintrmitigation_timer(sc->sc_ah, HAL_INT_TX_LASTPKT, (*(int *)buff));
        break;

        case ATH_PARAM_AMSDU_ENABLE:
            sc->sc_txamsdu = *(int *)buff;
        break;

        case ATH_PARAM_MAX_CLIENTS_PER_RADIO:
            value = *(int *)buff;
            retval = sc->sc_ieee_ops->set_max_sta(sc->sc_ieee, value);
        break;

#if ATH_SUPPORT_IQUE
		case ATH_PARAM_RETRY_DURATION:
			sc->sc_retry_duration = *(int *)buff;
		break;
		case ATH_PARAM_HBR_HIGHPER:
			sc->sc_hbr_per_high = *(int *)buff;
		break;

		case ATH_PARAM_HBR_LOWPER:
			sc->sc_hbr_per_low = *(int *)buff;
		break;
#endif

        case ATH_PARAM_RX_STBC:
            sc->sc_rxstbcsupport = *(int *)buff;
            sc->sc_ieee_ops->set_stbc_config(sc->sc_ieee, sc->sc_rxstbcsupport, 0);
        break;

        case ATH_PARAM_TX_STBC:
            sc->sc_txstbcsupport = *(int *)buff;
            sc->sc_ieee_ops->set_stbc_config(sc->sc_ieee, sc->sc_txstbcsupport, 1);
        break;

        case ATH_PARAM_LDPC:
            val = *(int *)buff;
            if ((val == 0) || ((ath_hal_ldpcsupport(sc->sc_ah, &ldpcsupport)) && ((ldpcsupport & val) == val))) {
                sc->sc_ldpcsupport = *(int *)buff;
                sc->sc_ieee_ops->set_ldpc_config(sc->sc_ieee, sc->sc_ldpcsupport);
            }
        break;

        case ATH_PARAM_LIMIT_LEGACY_FRM:
            sc->sc_limit_legacy_frames = *(int *)buff;
        break;

        case ATH_PARAM_TOGGLE_IMMUNITY:
            sc->sc_toggle_immunity = *(int *)buff;
        break;

        case ATH_PARAM_GPIO_LED_CUSTOM:
	        sc->sc_reg_parm.gpioLedCustom = *(int *)buff;
	        ath_led_reinit(sc);
	    break;

	    case ATH_PARAM_SWAP_DEFAULT_LED:
	        sc->sc_reg_parm.swapDefaultLED =  *(int *)buff;
	        ath_led_reinit(sc);
	    break;

#if ATH_SUPPORT_WIRESHARK
        case ATH_PARAM_TAPMONITOR:
            sc->sc_tapmonenable = *(int *)buff;
        break;
#endif
#if ATH_TX_DUTY_CYCLE
        case ATH_PARAM_TX_DUTY_CYCLE:
            if(*(int *)buff < ATH_TX_DUTY_CYCLE_MIN) {
                DPRINTF(sc, ATH_DEBUG_UNMASKABLE , "Setting tx_duty_cycle should be (tx_duty_cycle >= %d)\n", ATH_TX_DUTY_CYCLE_MIN);
            	retval = -1;
            }
            else{
                sc->sc_tx_dc_active_pct = *(int *)buff;
                if(sc->sc_tx_dc_active_pct >= ATH_TX_DUTY_CYCLE_MAX){
                    sc->sc_tx_dc_active_pct = 0;
                    ath_set_tx_duty_cycle(sc, sc->sc_tx_dc_active_pct, false);
                }else
                    ath_set_tx_duty_cycle(sc, sc->sc_tx_dc_active_pct, true);
            }
        break;
 #endif
#if ATH_SUPPORT_VOW_DCS
        case ATH_PARAM_DCS_COCH:
            sc->sc_coch_intr_thresh = *(int *)buff;
        break;
        case ATH_PARAM_DCS_TXERR:
            sc->sc_tx_err_thresh = *(int *)buff;
        break;
        case ATH_PARAM_DCS_PHYERR:
            sc->sc_phyerr_penalty = *(int *)buff;
        break;
#endif
#if ATH_SUPPORT_VOWEXT
        case ATH_PARAM_VOWEXT:
            printk("Setting new VOWEXT parameters buffers with %d\n",ATH_TXDESC);
            sc->sc_vowext_flags = (*(u_int32_t*)buff) & ATH_CAP_VOWEXT_MASK;
            printk("VOW enabled features: \n");
            printk("\tfair_queuing: %d\n\t"\
                   "retry_delay_red %d,\n\tbuffer reordering %d\n\t"\
			       "enhanced_rate_control_and_aggr %d\n",
                   (ATH_IS_VOWEXT_FAIRQUEUE_ENABLED(sc) ? 1 : 0),
                   (ATH_IS_VOWEXT_AGGRSIZE_ENABLED(sc) ? 1 : 0),
                   (ATH_IS_VOWEXT_BUFFREORDER_ENABLED(sc) ? 1 : 0),
			       (ATH_IS_VOWEXT_RCA_ENABLED(sc) ? 1 : 0));
        break;

        case ATH_PARAM_VSP_ENABLE:
            /* VSP feature uses ATH_TX_BUF_FLOW_CNTL feature so enable this feature only when ATH_TX_BUF_FLOW_CNTL is defined*/
            #if ATH_TX_BUF_FLOW_CNTL
	        sc->vsp_enable = *(int *)buff;
            #else
            if(*(int *)buff)
            {
                printk ("ATH_TX_BUF_FLOW_CNTL is not compiled in so can't enable VSP\n");
            }
            #endif
        break;

        case ATH_PARAM_VSP_THRESHOLD:
	        sc->vsp_threshold = (*(int *)buff)*1024*100; /* Convert from mbps to goodput (kbps*100) */
        break;

        case ATH_PARAM_VSP_EVALINTERVAL:
	        sc->vsp_evalinterval = *(int *)buff;
        break;

        case ATH_PARAM_VSP_STATSCLR:
            if(*(int *)buff)
            {
                /* clear out VSP Statistics */
                sc->vsp_bependrop    = 0;
                sc->vsp_bkpendrop    = 0;
                sc->vsp_vipendrop    = 0;
                sc->vsp_vicontention = 0;
            }
        break;
#endif
#if ATH_VOW_EXT_STATS
        case ATH_PARAM_VOWEXT_STATS:
            sc->sc_vowext_stats = ((*(u_int32_t*)buff)) ? 1: 0;
        break;
#endif
#ifdef ATH_SUPPORT_TxBF
        case ATH_PARAM_TXBF_SW_TIMER:
            sc->sc_reg_parm.TxBFSwCvTimeout = *(u_int32_t *) buff;
        break;
#endif
#if ATH_SUPPORT_VOWEXT
        case ATH_PARAM_PHYRESTART_WAR:
        {
            int oval=0;
            oval = sc->sc_phyrestart_disable;
            sc->sc_phyrestart_disable = *(int*) buff;
            /* In case if command issued to disable then disable it
             * immediate. Do not try touching the register multiple times.
             */
            if (oval && ((sc->sc_hang_war & HAL_PHYRESTART_CLR_WAR) &&
                (0 == sc->sc_phyrestart_disable)))
            {
                ath_hal_phyrestart_clr_war_enable(sc->sc_ah, 0);
            }
            else if ((sc->sc_hang_war & HAL_PHYRESTART_CLR_WAR) &&
                (sc->sc_phyrestart_disable) )
            {
                ath_hal_phyrestart_clr_war_enable(sc->sc_ah, 1);
        }
        }
        break;
#endif
        case ATH_PARAM_KEYSEARCH_ALWAYS_WAR:
        {
            sc->sc_keysearch_always_enable = *(int*) buff;
            ath_hal_keysearch_always_war_enable(sc->sc_ah,
                    sc->sc_keysearch_always_enable );
        }
        break;
#if ATH_SUPPORT_DYN_TX_CHAINMASK
        case ATH_PARAM_DYN_TX_CHAINMASK:
            sc->sc_dyn_txchainmask = *(int *)buff;
        break;
#endif /* ATH_SUPPORT_DYN_TX_CHAINMASK */
        case ATH_PARAM_BCN_BURST:
            if (ieee80211_vap_is_any_running(ic)) {
                qdf_print("%s: VAP(s) in running state. Cannot change between burst/staggered beacon modes",
                          __func__);
                retval = -EINVAL;
                break;
            }
            if (*(int *)buff)
                sc->sc_reg_parm.burst_beacons = 1;
            else
                sc->sc_reg_parm.burst_beacons = 0;
            break;
#if ATH_ANI_NOISE_SPUR_OPT
        case ATH_PARAM_NOISE_SPUR_OPT:
            sc->sc_config.ath_noise_spur_opt = *(int *)buff;
            break;
#endif /* ATH_ANI_NOISE_SPUR_OPT */
        case ATH_PARAM_DCS_ENABLE:
            sc->sc_dcs_enabled = (*((int *)buff));
            sc->sc_dcs_enabled  &= ATH_CAP_DCS_MASK;
            /* disable dcs-wlan-im for 11G mode */
            if ( (sc->sc_dcs_enabled & ATH_CAP_DCS_WLANIM) && (!((sc->sc_curmode == WIRELESS_MODE_11NA_HT20)
                        ||(sc->sc_curmode == WIRELESS_MODE_11NA_HT40PLUS)
                        ||(sc->sc_curmode == WIRELESS_MODE_11NA_HT40MINUS))) ){
                printk("Disabling DCS for 11G mode");
                sc->sc_dcs_enabled &= (~ATH_CAP_DCS_WLANIM);
            }
             printk("\tDCS for CW interference mitigation:   %d\n\t"\
                   "DCS for WLAN interference mitigation: %d\n",
                   ((sc->sc_dcs_enabled & ATH_CAP_DCS_CWIM) ? 1 : 0),
                   ((sc->sc_dcs_enabled & ATH_CAP_DCS_WLANIM) ? 1 : 0));

            break;
#if ATH_SUPPORT_RX_PROC_QUOTA
        case ATH_PARAM_CNTRX_NUM:
            if (*(int *)buff)
                sc->sc_process_rx_num = (*((int *)buff));
            /* converting to even number as we
               devide this by two for edma case */
            if((sc->sc_process_rx_num % 2)
                    && sc->sc_enhanceddmasupport) {
                sc->sc_process_rx_num++;
                printk("Adjusting to Nearest Even Value %d \n",sc->sc_process_rx_num);
            }
            break;
#endif
#if ATH_RX_LOOPLIMIT_TIMER
        case ATH_PARAM_LOOPLIMIT_NUM:
            if (*(int *)buff)
                sc->sc_rx_looplimit_timeout = (*((int *)buff));
            break;
#endif
        case ATH_PARAM_ANTENNA_GAIN_2G:
            if (*(int *)buff >=0 && *(int *)buff <= 30)
                ath_hal_set_antenna_gain(sc->sc_ah, *((int *)buff), 1);
            /* internal reset for power table re-calculation */
            sc->sc_reset_type = ATH_RESET_NOLOSS;
            ath_internal_reset(sc);
            sc->sc_reset_type = ATH_RESET_DEFAULT;
            break;
        case ATH_PARAM_ANTENNA_GAIN_5G:
            if (*(int *)buff >=0 && *(int *)buff <= 30)
                ath_hal_set_antenna_gain(sc->sc_ah, *((int *)buff), 0);
            /* internal reset for power table re-calculation */
            sc->sc_reset_type = ATH_RESET_NOLOSS;
            ath_internal_reset(sc);
            sc->sc_reset_type = ATH_RESET_DEFAULT;
            break;
        case ATH_PARAM_RTS_CTS_RATE:
            if ((*(int *)buff) < 4)
            {
                sc->sc_protrix=*(int *)buff;
                sc->sc_cus_cts_set=1;
            }
            else
            {
                sc->sc_cus_cts_set=0;
                printk ("ATH_PARAM_RTS_CTS_RATE not support\n");
            }
            break;
        case ATH_PARAM_NODEBUG:
            sc->sc_nodebug = *(int *)buff;
            printk("sc nodebug %d \n",sc->sc_nodebug);
            /* DA change required as part of dp stats convergence */
#if 0
#if ATH_SUPPORT_EXT_STAT
            if (!sc->sc_nodebug)
            {
                /* reset the statistics */
                ieee80211_reset_client_rt_rate(ic);
                /* start the timer to calculate the bytes/packets in last 1 second */
                OS_SET_TIMER (&ic->ic_client_stat_timer, 1000);
            }
            else
            {
                /* stop the timer to calculate the bytes/packets in last 1 second */
                OS_CANCEL_TIMER (&ic->ic_client_stat_timer);
            }
#endif
#endif
            break;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        case ATH_PARAM_BUFF_THRESH:
        {
            int thresh = *(int *)buff;
            if( (thresh >= MIN_BUFF_LEVEL_IN_PERCENT) && (thresh<=100) )
            {
                sc->sc_ald.sc_ald_free_buf_lvl = ATH_TXBUF - ( (ATH_TXBUF * thresh) / 100);
                DPRINTF(sc, ATH_DEBUG_UNMASKABLE , "Buff Warning Level=%d\n", (ATH_TXBUF - sc->sc_ald.sc_ald_free_buf_lvl));
            }
            else
            {
                DPRINTF(sc, ATH_DEBUG_UNMASKABLE , "ERR: Buff Thresh(in %) should be >=%d and <=100\n", MIN_BUFF_LEVEL_IN_PERCENT);
            }

        }
        break;
        case ATH_PARAM_BLK_REPORT_FLOOD:
			sc->sc_ieee_ops->block_flooding_report(sc->sc_ieee, !!((*(u_int32_t*)buff)));
        break;
        case ATH_PARAM_DROP_STA_QUERY:
			sc->sc_ieee_ops->drop_query_from_sta(sc->sc_ieee, !!((*(u_int32_t*)buff)));
        break;
        case ATH_PARAM_ALDSTATS:
            sc->sc_aldstatsenabled = !!(*(int *)buff);
            printk("sc aldstatsenabled %d \n",sc->sc_aldstatsenabled);
			sc->sc_nodebug = sc->sc_aldstatsenabled;
            break;
#endif
#if ATH_SUPPORT_DSCP_OVERRIDE
        case ATH_PARAM_DSCP_OVERRIDE:
            sc->sc_ieee_ops->set_dscp_override(sc->sc_ieee, *(u_int32_t*)buff);
        break;
        case ATH_PARAM_DSCP_TID_MAP_RESET:
            sc->sc_ieee_ops->reset_dscp_tid_map(sc->sc_ieee, (u_int8_t)(*(u_int32_t*)buff));
        break;
        case ATH_PARAM_IGMP_DSCP_OVERRIDE:
            sc->sc_ieee_ops->set_igmp_dscp_override(sc->sc_ieee, *(u_int32_t*)buff);
        break;
        case ATH_PARAM_IGMP_DSCP_TID_MAP:
            sc->sc_ieee_ops->set_igmp_dscp_tid_map(sc->sc_ieee, (u_int8_t)(*(u_int32_t*)buff));
        break;
        case ATH_PARAM_HMMC_DSCP_OVERRIDE:
            sc->sc_ieee_ops->set_hmmc_dscp_override(sc->sc_ieee, *(u_int32_t*)buff);
        break;
        case ATH_PARAM_HMMC_DSCP_TID_MAP:
            sc->sc_ieee_ops->set_hmmc_dscp_tid_map(sc->sc_ieee, (u_int8_t)(*(u_int32_t*)buff));
        break;
#endif
        case ATH_PARAM_ALLOW_PROMISC:
            sc->sc_allow_promisc = *(int*)buff;
            break;
        case ATH_PARAM_ACS_CTRLFLAG:
            if(sc->sc_ieee_ops->set_acs_param)
                sc->sc_ieee_ops->set_acs_param(sc->sc_ieee, IEEE80211_ACS_CTRLFLAG , *(int *)buff);
            break;
        case ATH_PARAM_ACS_ENABLE_BK_SCANTIMEREN:
            if(sc->sc_ieee_ops->set_acs_param)
                sc->sc_ieee_ops->set_acs_param(sc->sc_ieee, IEEE80211_ACS_ENABLE_BK_SCANTIMER , *(int *)buff);
            break;
        case ATH_PARAM_ACS_SCANTIME:
            if(sc->sc_ieee_ops->set_acs_param)
                sc->sc_ieee_ops->set_acs_param(sc->sc_ieee, IEEE80211_ACS_SCANTIME , *(int *)buff);
            break;
        case ATH_PARAM_ACS_RSSIVAR:
            if(sc->sc_ieee_ops->set_acs_param)
                sc->sc_ieee_ops->set_acs_param(sc->sc_ieee, IEEE80211_ACS_RSSIVAR , *(int *)buff);
            break;
        case ATH_PARAM_ACS_CHLOADVAR:
            if(sc->sc_ieee_ops->set_acs_param)
                sc->sc_ieee_ops->set_acs_param(sc->sc_ieee, IEEE80211_ACS_CHLOADVAR , *(int *)buff);
            break;
        case ATH_PARAM_ACS_SRLOADVAR:
            if(sc->sc_ieee_ops->set_acs_param)
                sc->sc_ieee_ops->set_acs_param(sc->sc_ieee, IEEE80211_ACS_SRLOADVAR , *(int *)buff);
            break;
        case ATH_PARAM_ACS_LIMITEDOBSS:
            if(sc->sc_ieee_ops->set_acs_param)
                sc->sc_ieee_ops->set_acs_param(sc->sc_ieee, IEEE80211_ACS_LIMITEDOBSS , *(int *)buff);
            break;
        case ATH_PARAM_ACS_DEBUGTRACE:
            if(sc->sc_ieee_ops->set_acs_param)
                sc->sc_ieee_ops->set_acs_param(sc->sc_ieee, IEEE80211_ACS_DEBUGTRACE , *(int *)buff);
            break;
        case ATH_PARAM_ACS_2G_ALLCHAN:
            if(sc->sc_ieee_ops->set_acs_param)
                sc->sc_ieee_ops->set_acs_param(sc->sc_ieee, IEEE80211_ACS_2G_ALL_CHAN, *(int *)buff);
            break;
#if ATH_CHANNEL_BLOCKING
        case ATH_PARAM_ACS_BLOCK_MODE:
            if (sc->sc_ieee_ops->set_acs_param)
                sc->sc_ieee_ops->set_acs_param(sc->sc_ieee, IEEE80211_ACS_BLOCK_MODE, *(int *)buff);
            break;
#endif
        case ATH_PARAM_RATE_ENABLE_RTS:
                sc->sc_limitrate = *(u_int32_t *) buff;
            break;
        case ATH_PARAM_EN_SELECTIVE_RTS:
            if ((sc->sc_limitrate!=0))
                sc->sc_user_en_rts_cts = *(u_int32_t *) buff;
            else
            {
                printk("Enable Rate limit first \r\n");
                retval = (-1);
            }
            break;
        case ATH_PARAM_DISABLE_DFS:
            if (*(int *)buff)
                sc->sc_is_blockdfs_set = true;
            else
                sc->sc_is_blockdfs_set = false;
            break;
#if QCA_AIRTIME_FAIRNESS
        case ATH_PARAM_ATF_STRICT_SCHED:
            val = sc->sc_ieee_ops->get_atf_scheduling(sc->sc_ieee);
            psoc = wlan_pdev_get_psoc(pdev);
            atf_ssidgroup = target_if_atf_get_ssid_group(psoc, pdev);
            if(((*(u_int32_t*)buff) == 1) && (!(val & ATF_GROUP_SCHED_POLICY)) && atf_ssidgroup)
            {
                printk("\nFair queue across groups is enabled so strict queue within groups is not allowed. Invalid combination \n");
                return -EINVAL;
            }
            if (*(u_int32_t*)buff)
                val |= IEEE80211_ATF_SCHED_STRICT;
            else
                val &= ~IEEE80211_ATF_SCHED_STRICT;

            sc->sc_ieee_ops->atf_scheduling(sc->sc_ieee, val);
            break;
        case ATH_PARAM_ATF_OBSS_SCHED:
            val = sc->sc_ieee_ops->get_atf_scheduling(sc->sc_ieee);

            if (*(u_int32_t*)buff)
                val |= IEEE80211_ATF_SCHED_OBSS;
            else
                val &= ~IEEE80211_ATF_SCHED_OBSS;

            sc->sc_ieee_ops->atf_scheduling(sc->sc_ieee, val);
            break;
        case ATH_PARAM_ATF_ADJUST_POSSIBLE_TPUT:
            sc->sc_adjust_possible_tput = *(int *)buff & ATF_ADJUST_POSSIBLE_TPUT_MASK;
            sc->sc_use_aggr_for_possible_tput = (*(int *)buff & ATF_USE_AGGR_FOR_POSSIBLE_TPUT_MASK) >>
                                                                    ATF_USE_AGGR_FOR_POSSIBLE_TPUT_SHIFT;
            break;
        case ATH_PARAM_ATF_ADJUST_TPUT:
            sc->sc_adjust_tput = *(int *)buff;
            break;
        case ATH_PARAM_ATF_ADJUST_TPUT_MORE:
            sc->sc_adjust_tput_more = (*(int *)buff & ATF_ADJUST_TPUT_MORE_MASK) >>
                                                          ATF_ADJUST_TPUT_MORE_SHIFT;
            sc->sc_adjust_tput_more_thrs = *(int *)buff & ATF_ADJUST_TPUT_MORE_THRS_MASK;
            break;
        case ATH_PARAM_ATF_OBSS_SCALE:
            sc->sc_ieee_ops->atf_obss_scale(sc->sc_ieee, *(u_int32_t*)buff);
            break;
        case ATH_PARAM_ATF_GROUP_SCHED_POLICY:
            val = sc->sc_ieee_ops->get_atf_scheduling(sc->sc_ieee);
            if(((*(u_int32_t*)buff) == 0) && (val & IEEE80211_ATF_SCHED_STRICT))
            {
                printk("\nStrict queue within groups is enabled so fair queue across groups is not allowed. Invalid combination \n");
                return -EINVAL;
            }

            if (*(u_int32_t*)buff)
                val |= IEEE80211_ATF_GROUP_SCHED_POLICY;
            else
                val &= ~IEEE80211_ATF_GROUP_SCHED_POLICY;

            sc->sc_ieee_ops->atf_scheduling(sc->sc_ieee, val);
            break;
#endif
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
        case ATH_PARAM_STADFS_ENABLE:
            if(!(*(int *)buff)) {
                ieee80211com_clear_cap_ext(ic,IEEE80211_CEXT_STADFS);
            } else {
                ieee80211com_set_cap_ext(ic,IEEE80211_CEXT_STADFS);
            }
            break;
#endif
        case ATH_PARAM_CHANSWITCH_OPTIONS:
            ic->ic_chanswitch_flags = (*(int *)buff);
            /* When TxCSA is set to 1, Repeater CAC(IEEE80211_CSH_OPT_CAC_APUP_BYSTA)
             * will be forced to 1. Because, the TXCSA is done by changing channel in
             * the beacon update function(ieee80211_beacon_update) and AP VAPs change
             * the channel, if the new channel is DFS then AP VAPs do CAC and STA VAP
             * has to synchronize with the AP VAPS' CAC.
             */
            if (IEEE80211_IS_CSH_CSA_APUP_BYSTA_ENABLED(ic)) {
                IEEE80211_CSH_CAC_APUP_BYSTA_ENABLE(ic);
                qdf_print("%s: When TXCSA is set to 1, Repeater CAC is forced to 1", __func__);
            }
            break;
        case ATH_MIN_RSSI_ENABLE:
            {
                val = *(int *)buff;
                if (val == 0 || val == 1) {
                    if (val)
                       sc->sc_ieee_ops->set_enable_min_rssi(sc->sc_ieee, true);
                    else
                       sc->sc_ieee_ops->set_enable_min_rssi(sc->sc_ieee, false);
                } else {
                    printk("Enter 0 or 1\n");
                    return (-1);
                }
            }
            break;
        case ATH_MIN_RSSI:
            {
                val = *(int *)buff;
                /* here rssi is actually snr, so only positive valuse accepted */
                if (val <= 0) {
                    printk("snr should be a positive value\n");
                    return (-1);
                } else
                    sc->sc_ieee_ops->set_min_rssi(sc->sc_ieee, val);
            }
            break;
#if DBDC_REPEATER_SUPPORT
        case ATH_PARAM_PRIMARY_RADIO:
            val = *(int *)buff;
            if(!ic->ic_sta_vap) {
                qdf_print("Radio not configured on repeater mode");
                retval = (-1);
               break;
            }
            if(val != 1) {
                qdf_print("Value should be given as 1 to set primary radio");
                retval = (-1);
                break;
            }
            if(ic->ic_primary_radio == val) {
                qdf_print("primary radio is set already for this radio");
                break;
            }
            for (i=0; i < MAX_RADIO_CNT; i++) {
                GLOBAL_IC_LOCK_BH(ic->ic_global_list);
                tmp_ic = ic->ic_global_list->global_ic[i];
                GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
                if (tmp_ic) {
                    spin_lock(&tmp_ic->ic_lock);
                    if (ic == tmp_ic) {
                        /* Setting current radio as primary radio*/
                        qdf_print("Setting primary radio for %s", ether_sprintf(ic->ic_myaddr));
                        tmp_ic->ic_primary_radio = 1;
                    } else {
                        tmp_ic->ic_primary_radio = 0;
                    }
                    spin_unlock(&tmp_ic->ic_lock);
                }
            }
            wlan_update_radio_priorities(ic);
#if ATH_SUPPORT_WRAP
            osif_set_primary_radio_event(ic);
#endif
            break;
        case ATH_PARAM_DBDC_ENABLE:
            GLOBAL_IC_LOCK_BH(ic->ic_global_list);
            ic->ic_global_list->dbdc_process_enable = (*((int *)buff)) ?1:0;
            GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            break;
        case ATH_PARAM_CLIENT_MCAST:
            GLOBAL_IC_LOCK_BH(ic->ic_global_list);
            if (*(int *)buff) {
                ic->ic_global_list->force_client_mcast_traffic = 1;
                printk("Enabling MCAST client traffic to go on corresponding STA VAP\n");
            } else {
                ic->ic_global_list->force_client_mcast_traffic = 0;
                printk("Disabling MCAST client traffic to go on corresponding STA VAP\n");
            }
            GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            break;
#endif
        case ATH_PARAM_ATH_CTL_POWER_SCALE:
            sc->sc_config.pwscale = *(int *)buff;
            ath_control_pwscale(sc);
            break;

#if UMAC_SUPPORT_ACFG
        case ATH_PARAM_DIAG_ENABLE:
            val = *(int *)buff;
            if (val == 0 || val == 1) {
                struct ieee80211com *ic = (struct ieee80211com*)(sc->sc_ieee);
                if (val && !ic->ic_diag_enable) {
                    acfg_diag_pvt_t *diag = (acfg_diag_pvt_t *)ic->ic_diag_handle;
                    if (diag) {
                        ic->ic_diag_enable = val;
                        OS_SET_TIMER(&diag->diag_timer, 0);
                    }
                }else if (!val) {
                    ic->ic_diag_enable = val;
                }
            } else {
                printk("Enter 0 or 1\n");
                return (-1);
            }
            break;
#endif
        case ATH_PARAM_CHAN_STATS_TH:
            ic->ic_chan_stats_th = *(int *)buff;
            break;

        case ATH_PARAM_PASSIVE_SCAN_ENABLE:
            ic->ic_strict_pscan_enable = !!(*(int *)buff);
            break;
#if DBDC_REPEATER_SUPPORT
        case ATH_PARAM_DELAY_STAVAP_UP:
            val = *(int *)buff;
            GLOBAL_IC_LOCK_BH(ic->ic_global_list);
            if (val) {
                ic->ic_global_list->delay_stavap_connection = val;
                qdf_print("Enabling DELAY_STAVAP_UP:%d",val);
            } else {
                ic->ic_global_list->delay_stavap_connection = 0;
                qdf_print("Disabling DELAY_STAVAP_UP");
            }
            GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            break;
#endif
        case ATH_PARAM_TID_OVERRIDE_QUEUE_MAPPING:
            sc->sc_ieee_ops->set_tid_override_queue_mapping(sc->sc_ieee, *(u_int32_t*)buff);
            break;
        case ATH_PARAM_NO_VLAN:
             ic->ic_no_vlan = !!(*(int *)buff);
            break;
        case ATH_PARAM_ATF_LOGGING:
             ic->ic_atf_logging = !!(*(int *)buff);
             break;

        case ATH_PARAM_STRICT_DOTH:
            ic->ic_strict_doth = !!(*(int *)buff);
            break;
#if DBDC_REPEATER_SUPPORT
        case ATH_PARAM_DISCONNECTION_TIMEOUT:
            val = *(int *)buff;
            GLOBAL_IC_LOCK_BH(ic->ic_global_list);
            ic->ic_global_list->disconnect_timeout = val;
            qdf_print(" Disconnect_timeout value entered:%d ",val);
            GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            break;
        case ATH_PARAM_RECONFIGURATION_TIMEOUT:
            val = *(int *)buff;
            GLOBAL_IC_LOCK_BH(ic->ic_global_list);
            ic->ic_global_list->reconfiguration_timeout = val;
            qdf_print(" reconfiguration_timeout value entered:%d ",val);
            GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            break;
#endif
        case ATH_PARAM_CHANNEL_SWITCH_COUNT:
             ic->ic_chan_switch_cnt = (uint8_t)(*(int *)buff);
             break;
        case ATH_PARAM_SECONDARY_OFFSET_IE:
            ic->ic_sec_offsetie =  !!(*(int *)buff);
            break;
        case ATH_PARAM_WIDE_BAND_SUB_ELEMENT:
            ic->ic_wb_subelem =  !!(*(int *)buff);
            break;
#if DBDC_REPEATER_SUPPORT
        case ATH_PARAM_ALWAYS_PRIMARY:
            val = *(int *)buff;
            GLOBAL_IC_LOCK_BH(ic->ic_global_list);
            ic->ic_global_list->always_primary = val ?1:0;
            qdf_print("Setting always primary flag as %d ",val);
            GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            break;
	case ATH_PARAM_FAST_LANE:
	    val = *(int *)buff;
            if (val && (ic->ic_global_list->num_fast_lane_ic > 1)) {
                /* fast lane support allowed only on 2 radios*/
                qdf_print("fast lane support allowed only on 2 radios ");
                return (-1);
            }
            if ((ic->fast_lane == 0) && val) {
                GLOBAL_IC_LOCK_BH(ic->ic_global_list);
                ic->ic_global_list->num_fast_lane_ic++;
                GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            }
            if ((ic->fast_lane == 1) && !val) {
                GLOBAL_IC_LOCK_BH(ic->ic_global_list);
                ic->ic_global_list->num_fast_lane_ic--;
                GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
            }
	    spin_lock(&ic->ic_lock);
	    ic->fast_lane = val ?1:0;
            qdf_print("Setting fast lane flag as %d for radio:%s",val,ether_sprintf(ic->ic_my_hwaddr));
            spin_unlock(&ic->ic_lock);
            if (ic->fast_lane) {
                for (i=0; i < MAX_RADIO_CNT; i++) {
                    GLOBAL_IC_LOCK_BH(ic->ic_global_list);
                    tmp_ic = ic->ic_global_list->global_ic[i];
                    GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
                    if (tmp_ic && (tmp_ic != ic) && tmp_ic->fast_lane) {
                        spin_lock(&tmp_ic->ic_lock);
                        tmp_ic->fast_lane_ic = ic;
                        spin_unlock(&tmp_ic->ic_lock);
                        spin_lock(&ic->ic_lock);
                        ic->fast_lane_ic = tmp_ic;
                        qdf_print("fast lane ic mac:%s",ether_sprintf(ic->fast_lane_ic->ic_my_hwaddr));
                        spin_unlock(&ic->ic_lock);
                    }
		}
	    } else {
                fast_lane_ic = ic->fast_lane_ic;
                if (fast_lane_ic) {
                    spin_lock(&fast_lane_ic->ic_lock);
                    fast_lane_ic->fast_lane_ic = NULL;
                    spin_unlock(&fast_lane_ic->ic_lock);
                }
                spin_lock(&ic->ic_lock);
                ic->fast_lane_ic = NULL;
                spin_unlock(&ic->ic_lock);
            }
            qdf_print("num fast lane ic count %d",ic->ic_global_list->num_fast_lane_ic);
            break;
        case ATH_PARAM_PREFERRED_UPLINK:
            val = *(int *)buff;
            if(!ic->ic_sta_vap) {
                qdf_print("Radio not configured on repeater mode");
                retval = -EINVAL;
                break;
            }
            if(val != 1) {
                qdf_print("Value should be given as 1 to set as preferred uplink");
                retval = -EINVAL;
                break;
            }
            for (i=0; i < MAX_RADIO_CNT; i++) {
                GLOBAL_IC_LOCK_BH(ic->ic_global_list);
                tmp_ic = ic->ic_global_list->global_ic[i];
                GLOBAL_IC_UNLOCK_BH(ic->ic_global_list);
                if (tmp_ic) {
                    spin_lock(&tmp_ic->ic_lock);
                    if (ic == tmp_ic) {
                        /* Setting current radio as preferred uplink*/
                        tmp_ic->ic_preferredUplink = 1;
                    } else {
                        tmp_ic->ic_preferredUplink = 0;
                    }
                    spin_unlock(&tmp_ic->ic_lock);
                }
            }
            break;
#endif
        case ATH_PARAM_DUMP_OBJECTS:
            wlan_objmgr_print_ref_cnts(ic);
            break;

        case ATH_PARAM_ACS_RANK:
            if (ic->ic_acs) {
                ieee80211_acs_set_param(ic->ic_acs, IEEE80211_ACS_RANK , *(int *)buff);
            }

            break;
#if ATH_PARAMETER_API
        case ATH_PARAM_PAPI_ENABLE:
            val = *(int *)buff;
            if (val == 0 || val == 1) {
                ic->ic_papi_enable = val;
            } else {
                QDF_PRINT_INFO(QDF_PRINT_IDX_SHARED, QDF_MODULE_ID_ANY, QDF_TRACE_LEVEL_INFO,
                               "Value should be either 0 or 1\n");
                return (-1);
            }
            break;
#endif
        case ATH_PARAM_NF_THRESH:
            if (!ic->ic_acs) {
                qdf_print("Unable to Set NF Threshold");
                return -1;
            }
            retval = ieee80211_acs_set_param(ic->ic_acs,
                                             IEEE80211_ACS_NF_THRESH,
                                             *(int *)buff);
            break;

        default:
            return (-1);
    }
    return (retval);
}

/******************************************************************************/
/*!
**  \brief Get ATH configuration parameter
**
**  This returns the current value of the indicated parameter.  Conversion
**  to integer done here.
**
**  \param dev Handle for the ATH device
**  \param ID Parameter ID value, as defined in the ath_param_ID_t type
**  \param buff Buffer to place the integer value into.
**  \return 0 on success
**  \return -1 for unsupported values
*/

int
ath_get_config(ath_dev_t dev, ath_param_ID_t ID, void *buff)
{
    struct ath_softc *sc = ATH_DEV_TO_SC(dev);
#if ATH_BT_COEX_3WIRE_MODE
    struct ath_hal *ah = sc->sc_ah;
#endif
    struct ieee80211com *ic = (struct ieee80211com*)(sc->sc_ieee);
    switch(ID)
    {
        case ATH_PARAM_TXCHAINMASK:
            *(int *)buff = sc->sc_config.txchainmask;
        break;

        case ATH_PARAM_GET_IF_ID:
            *(int *)buff = IF_ID_DIRECT_ATTACH;
        break;

        case ATH_PARAM_RXCHAINMASK:
            *(int *)buff = sc->sc_config.rxchainmask;
        break;

        case ATH_PARAM_TXCHAINMASKLEGACY:
            *(int *)buff = sc->sc_config.txchainmasklegacy;
        break;

        case ATH_PARAM_RXCHAINMASKLEGACY:
            *(int *)buff = sc->sc_config.rxchainmasklegacy;
        break;
        case ATH_PARAM_CHAINMASK_SEL:
            *(int *)buff = sc->sc_config.chainmask_sel;
        break;

        case ATH_PARAM_AMPDU:
            *(int *)buff = sc->sc_txaggr;
        break;

        case ATH_PARAM_AMPDU_LIMIT:
            *(int *)buff = sc->sc_config.ampdu_limit;
        break;

        case ATH_PARAM_AMPDU_SUBFRAMES:
            *(int *)buff = sc->sc_config.ampdu_subframes;
        break;

        case ATH_PARAM_AMPDU_RX_BSIZE:
            *(int *)buff = sc->sc_config.ampdu_rx_bsize;
        break;

        case ATH_PARAM_AGGR_PROT:
            *(int *)buff = sc->sc_config.ath_aggr_prot;
        break;

        case ATH_PARAM_AGGR_PROT_DUR:
            *(int *)buff = sc->sc_config.ath_aggr_prot_duration;
        break;

        case ATH_PARAM_AGGR_PROT_MAX:
            *(int *)buff = sc->sc_config.ath_aggr_prot_max;
        break;

        case ATH_PARAM_TXPOWER_LIMIT2G:
            *(int *)buff = sc->sc_config.txpowlimit2G;
        break;

        case ATH_PARAM_TXPOWER_LIMIT5G:
            *(int *)buff = sc->sc_config.txpowlimit5G;
        break;

        case ATH_PARAM_TXPOWER_OVERRIDE:
            *(int *)buff = sc->sc_config.txpowlimit_override;
        break;

        case ATH_PARAM_PCIE_DISABLE_ASPM_WK:
            *(int *)buff = sc->sc_config.pcieDisableAspmOnRfWake;
        break;

        case ATH_PARAM_PCID_ASPM:
            *(int *)buff = sc->sc_config.pcieAspm;
        break;

        case ATH_PARAM_BEACON_NORESET:
            *(int *)buff = sc->sc_noreset;
        break;

        case ATH_PARAM_CAB_CONFIG:
            *(int *)buff = sc->sc_config.cabqReadytime;
        break;

        case ATH_PARAM_ATH_DEBUG:
        {
            /*
             * asf_print: the asf print mask is (probably) 64 bits long, but
             * we are expected to return 32 bits. For now, just return the 32
             * LSBs from the print mask. Currently, all used ATH print
             * categories fit within 32 bits anyway.
             */
            u_int8_t *mask_ptr = &sc->sc_print.category_mask[0];
            *(int *)buff =
                mask_ptr[0] |
                (mask_ptr[1] << 8) |
                (mask_ptr[2] << 16) |
                (mask_ptr[3] << 24);
        }
        break;

#if ATH_BT_COEX_3WIRE_MODE
        case ATH_PARAM_ATH_3WIRE_BT_COEX:
           *(int *) buff = sc->sc_3wire_bt_coex_enable;
        break;

        case ATH_PARAM_ATH_3WIRE_BT_WEIGHT:
           *(u_long *) buff = ah->ah_bt_coex_bt_weight;
            printk("bt_weight = 0x%lx\n", ah->ah_bt_coex_bt_weight);
        break;

        case ATH_PARAM_ATH_3WIRE_WL_WEIGHT:
           *(u_long *) buff = ah->ah_bt_coex_wl_weight;
            printk("wl_weight = 0x%lx\n",ah->ah_bt_coex_wl_weight);
        break;
#endif

        case ATH_PARAM_ATH_TPSCALE:
           *(int *) buff = sc->sc_config.tpscale;
        break;
        case ATH_PARAM_ATH_RXQ_INFO:
        {
            struct ath_rx_edma *rxedma_lp, *rxedma_hp;
            int i = 0;
            struct ath_buf *bf, *tbf;
            rxedma_lp = &sc->sc_rxedma[HAL_RX_QUEUE_LP];
            rxedma_hp = &sc->sc_rxedma[HAL_RX_QUEUE_HP];
            printk("LP details ---\n");
            printk("rxfifo =%pK, rxfifodepth = %d, rxfifohwsize =%d, rxfifoheadindex=%d, rxfifotailindex=%d\n",
                    rxedma_lp->rxfifo, rxedma_lp->rxfifodepth, rxedma_lp->rxfifohwsize, rxedma_lp->rxfifoheadindex, rxedma_lp->rxfifotailindex );

            printk("HP details ---\n");
            printk("rxfifo =%pK, rxfifodepth = %d, rxfifohwsize =%d, rxfifoheadindex=%d, rxfifotailindex=%d\n",
                    rxedma_hp->rxfifo, rxedma_hp->rxfifodepth, rxedma_hp->rxfifohwsize, rxedma_hp->rxfifoheadindex, rxedma_hp->rxfifotailindex );

            printk("sc_imask = 0x%x\n", sc->sc_imask);
            TAILQ_FOREACH_SAFE(bf, &sc->sc_rxbuf, bf_list, tbf) {
                if(bf == NULL)
                    break;
                i++;
            }
            bf = TAILQ_FIRST(&sc->sc_rxbuf);
            printk("sc_rxbuf num of elements = %d, bf=%pK, bf_mpdu=%pK\n", i, bf, bf?bf->bf_mpdu:0 );
        }
        break;

        case ATH_PARAM_ACKTIMEOUT:
           *(int *) buff = ath_hal_getacktimeout(sc->sc_ah) /* usec */;
        break;

        case ATH_PARAM_RIMT_FIRST:
           *(int *)buff = ath_hal_getintrmitigation_timer(sc->sc_ah, HAL_INT_RX_FIRSTPKT);
        break;

        case ATH_PARAM_RIMT_LAST:
           *(int *)buff = ath_hal_getintrmitigation_timer(sc->sc_ah, HAL_INT_RX_LASTPKT);
        break;

        case ATH_PARAM_TIMT_FIRST:
           *(int *)buff = ath_hal_getintrmitigation_timer(sc->sc_ah, HAL_INT_TX_FIRSTPKT);
        break;

        case ATH_PARAM_TIMT_LAST:
           *(int *)buff = ath_hal_getintrmitigation_timer(sc->sc_ah, HAL_INT_TX_LASTPKT);
        break;

        case ATH_PARAM_AMSDU_ENABLE:
            *(int *)buff = sc->sc_txamsdu;
        break;

        case ATH_PARAM_MAX_CLIENTS_PER_RADIO:
            *(int *)buff = sc->sc_ieee_ops->get_max_sta(sc->sc_ieee);
        break;
#if ATH_SUPPORT_IQUE
		case ATH_PARAM_RETRY_DURATION:
			*(int *)buff = sc->sc_retry_duration;
		break;
		case ATH_PARAM_HBR_HIGHPER:
			*(int *)buff = sc->sc_hbr_per_high;
		break;

		case ATH_PARAM_HBR_LOWPER:
			*(int *)buff = sc->sc_hbr_per_low;
		break;
#endif
        case ATH_PARAM_RX_STBC:
            *(int *)buff = sc->sc_rxstbcsupport;
        break;

        case ATH_PARAM_TX_STBC:
            *(int *)buff = sc->sc_txstbcsupport;
        break;

        case ATH_PARAM_LDPC:
            *(int *)buff = sc->sc_ldpcsupport;
        break;

        case ATH_PARAM_LIMIT_LEGACY_FRM:
            *(int *)buff = sc->sc_limit_legacy_frames;
        break;

        case ATH_PARAM_TOGGLE_IMMUNITY:
            *(int *)buff = sc->sc_toggle_immunity;
        break;

        case ATH_PARAM_WEP_TKIP_AGGR_TX_DELIM:
            (void)ath_hal_gettxdelimweptkipaggr(sc->sc_ah, (u_int32_t *)buff);
            break;

        case ATH_PARAM_WEP_TKIP_AGGR_RX_DELIM:
            (void)ath_hal_getrxdelimweptkipaggr(sc->sc_ah, (u_int32_t *)buff);
            break;

	case ATH_PARAM_GPIO_LED_CUSTOM:
            *(int *)buff = sc->sc_reg_parm.gpioLedCustom;
            break;

        case ATH_PARAM_SWAP_DEFAULT_LED:
            *(int *)buff = sc->sc_reg_parm.swapDefaultLED;
            break;
#if ATH_TX_DUTY_CYCLE
        case ATH_PARAM_TX_DUTY_CYCLE:
            *(int *)buff = sc->sc_tx_dc_enable? sc->sc_tx_dc_active_pct : ATH_TX_DUTY_CYCLE_MAX ;
        break;
#endif
#if ATH_SUPPORT_VOW_DCS
        case ATH_PARAM_DCS_COCH:
            *(int *)buff = sc->sc_coch_intr_thresh;
        break;
        case ATH_PARAM_DCS_TXERR:
            *(int *)buff = sc->sc_tx_err_thresh;
        break;
        case ATH_PARAM_DCS_PHYERR:
            *(int *)buff = sc->sc_phyerr_penalty;
        break;
#endif
#if ATH_SUPPORT_VOWEXT
        case ATH_PARAM_VOWEXT:
            *(int *)buff = sc->sc_vowext_flags;
        break;

        case ATH_PARAM_VSP_ENABLE:
	         *(int *)buff = sc->vsp_enable;
        break;

        case ATH_PARAM_VSP_THRESHOLD:
	        *(int *)buff = (sc->vsp_threshold)/(100*1024);  /* Convert from goodput fromat (kbps*100) to mbps format */
        break;

        case ATH_PARAM_VSP_EVALINTERVAL:
	        *(int *)buff = sc->vsp_evalinterval;
        break;

        case ATH_PARAM_VSP_STATS:
            printk("VSP Statistics:\n\t"\
                    "vi contention: %d\n\tbe(penalized) drop : %d\n\t"\
                    "bk(penalized) drop : %d\n\tvi(penalized) drop : %d\n",\
                    sc->vsp_vicontention, sc->vsp_bependrop,
                    sc->vsp_bkpendrop, sc->vsp_vipendrop);
        break;
#endif
#if ATH_VOW_EXT_STATS
        case ATH_PARAM_VOWEXT_STATS:
            *(int*)buff = sc->sc_vowext_stats;
            break;
#endif
//        case ATH_PARAM_USE_EAP_LOWEST_RATE:
//            *(int *)buff = sc->sc_eap_lowest_rate;

#if ATH_SUPPORT_WIRESHARK
        case ATH_PARAM_TAPMONITOR:
            *(int *)buff = sc->sc_tapmonenable;
        break;
#endif
#ifdef ATH_SUPPORT_TxBF
        case ATH_PARAM_TXBF_SW_TIMER:
            *(int *)buff = sc->sc_reg_parm.TxBFSwCvTimeout  ;
        break;
#endif
#if ATH_SUPPORT_VOWEXT
        case ATH_PARAM_PHYRESTART_WAR:
            *(int*) buff  = sc->sc_phyrestart_disable;
            break;
#endif
        case ATH_PARAM_KEYSEARCH_ALWAYS_WAR:
            *(int*) buff  = sc->sc_keysearch_always_enable;
            break;
        case ATH_PARAM_CHANNEL_SWITCHING_TIME_USEC:
            (void)ath_hal_getchannelswitchingtime(sc->sc_ah, (u_int32_t *)buff);
            break;
#if ATH_SUPPORT_DYN_TX_CHAINMASK
        case ATH_PARAM_DYN_TX_CHAINMASK:
            *(int *)buff = sc->sc_dyn_txchainmask;
            break;
#endif /* ATH_SUPPORT_DYN_TX_CHAINMASK */
        case ATH_PARAM_AGGR_BURST:
            *(int *)buff = sc->sc_aggr_burst;
            break;
        case ATH_PARAM_AGGR_BURST_DURATION:
            *(int *)buff = sc->sc_aggr_burst_duration;
        break;
        case ATH_PARAM_BCN_BURST:
            *(int *)buff = sc->sc_reg_parm.burst_beacons;
            break;
        case ATH_PARAM_DCS_ENABLE:
            *(int *)buff = sc->sc_dcs_enabled;
        break;

        case ATH_PARAM_TOTAL_PER:
            *(int *)buff =
                sc->sc_ieee_ops->get_total_per(sc->sc_ieee);
            break;
#if ATH_SUPPORT_RX_PROC_QUOTA
        case ATH_PARAM_CNTRX_NUM:
            *(int *)buff = sc->sc_process_rx_num;
            break;
#endif
#if ATH_RX_LOOPLIMIT_TIMER
        case ATH_PARAM_LOOPLIMIT_NUM:
            *(int *)buff = sc->sc_rx_looplimit_timeout;
            break;
#endif
        case ATH_PARAM_ANTENNA_GAIN_2G:
            *(int *)buff = ath_hal_get_antenna_gain(sc->sc_ah, 1);
            break;
        case ATH_PARAM_ANTENNA_GAIN_5G:
            *(int *)buff = ath_hal_get_antenna_gain(sc->sc_ah, 0);
            break;
        case ATH_PARAM_RTS_CTS_RATE:
            *(int *)buff = sc->sc_protrix;
            break;
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
        case ATH_PARAM_BUFF_THRESH:
            *(int *)buff = ATH_TXBUF - sc->sc_ald.sc_ald_free_buf_lvl;
        break;
        case ATH_PARAM_BLK_REPORT_FLOOD:
            *(int *)buff = sc->sc_ieee_ops->get_block_flooding_report(sc->sc_ieee);
        break;
        case ATH_PARAM_DROP_STA_QUERY:
            *(int *)buff = sc->sc_ieee_ops->get_drop_query_from_sta(sc->sc_ieee);
        break;
#endif
#if ATH_SUPPORT_DSCP_OVERRIDE
        case ATH_PARAM_DSCP_OVERRIDE:
            *(int *)buff = sc->sc_ieee_ops->get_dscp_override(sc->sc_ieee);
        case ATH_PARAM_IGMP_DSCP_OVERRIDE:
            *(int *)buff = sc->sc_ieee_ops->get_igmp_dscp_override(sc->sc_ieee);
        break;
        case ATH_PARAM_IGMP_DSCP_TID_MAP:
            *(int *)buff = sc->sc_ieee_ops->get_igmp_dscp_tid_map(sc->sc_ieee);
        break;
        case ATH_PARAM_HMMC_DSCP_OVERRIDE:
            *(int *)buff = sc->sc_ieee_ops->get_hmmc_dscp_override(sc->sc_ieee);
        break;
        case ATH_PARAM_HMMC_DSCP_TID_MAP:
            *(int *)buff = sc->sc_ieee_ops->get_hmmc_dscp_tid_map(sc->sc_ieee);
        break;
#endif
        case ATH_PARAM_ALLOW_PROMISC:
            *(int *)buff = sc->sc_allow_promisc;
        break;
        case ATH_PARAM_ACS_CTRLFLAG:
            if(sc->sc_ieee_ops->get_acs_param)
                *(int *)buff =sc->sc_ieee_ops->get_acs_param(sc->sc_ieee, IEEE80211_ACS_CTRLFLAG);
            break;

        case ATH_PARAM_ACS_ENABLE_BK_SCANTIMEREN:
            if(sc->sc_ieee_ops->get_acs_param)
                *(int *)buff =sc->sc_ieee_ops->get_acs_param(sc->sc_ieee, IEEE80211_ACS_ENABLE_BK_SCANTIMER);
            break;

        case ATH_PARAM_ACS_SCANTIME:
            if(sc->sc_ieee_ops->get_acs_param)
                *(int *)buff =sc->sc_ieee_ops->get_acs_param(sc->sc_ieee, IEEE80211_ACS_SCANTIME);
            break;

        case ATH_PARAM_ACS_RSSIVAR:
            if(sc->sc_ieee_ops->get_acs_param)
                *(int *)buff =sc->sc_ieee_ops->get_acs_param(sc->sc_ieee, IEEE80211_ACS_RSSIVAR);
            break;

        case ATH_PARAM_ACS_CHLOADVAR:
            if(sc->sc_ieee_ops->get_acs_param)
                *(int *)buff =sc->sc_ieee_ops->get_acs_param(sc->sc_ieee, IEEE80211_ACS_CHLOADVAR);
            break;

        case ATH_PARAM_ACS_SRLOADVAR:
            if(sc->sc_ieee_ops->get_acs_param)
                *(int *)buff =sc->sc_ieee_ops->get_acs_param(sc->sc_ieee, IEEE80211_ACS_SRLOADVAR);
            break;

        case ATH_PARAM_ACS_LIMITEDOBSS:
            if(sc->sc_ieee_ops->get_acs_param)
                *(int *)buff =sc->sc_ieee_ops->get_acs_param(sc->sc_ieee, IEEE80211_ACS_LIMITEDOBSS);
            break;

        case ATH_PARAM_ACS_DEBUGTRACE:
            if(sc->sc_ieee_ops->get_acs_param)
                *(int *)buff =sc->sc_ieee_ops->get_acs_param(sc->sc_ieee, IEEE80211_ACS_DEBUGTRACE);
            break;
        case ATH_PARAM_ACS_2G_ALLCHAN:
            if(sc->sc_ieee_ops->get_acs_param)
                *(int *)buff =sc->sc_ieee_ops->get_acs_param(sc->sc_ieee, IEEE80211_ACS_2G_ALL_CHAN);
            break;
#if ATH_CHANNEL_BLOCKING
        case ATH_PARAM_ACS_BLOCK_MODE:
            if (sc->sc_ieee_ops->get_acs_param)
                *(int *)buff =sc->sc_ieee_ops->get_acs_param(sc->sc_ieee, IEEE80211_ACS_BLOCK_MODE);
            break;
#endif
        case ATH_PARAM_RADIO_TYPE:
            *(int *)buff = ic->ic_is_mode_offload(ic);
            break;
        case ATH_PARAM_RATE_ENABLE_RTS:
                *(int *) buff = sc->sc_limitrate;
            break;
        case ATH_PARAM_EN_SELECTIVE_RTS:
                *(int *) buff = sc->sc_user_en_rts_cts;
            break;
        case ATH_PARAM_DISABLE_DFS:
            *(int *)buff = (sc->sc_is_blockdfs_set == true);
            break;
 #if QCA_AIRTIME_FAIRNESS
        case ATH_PARAM_ATF_STRICT_SCHED:
            *(int *)buff = (sc->sc_ieee_ops->get_atf_scheduling(sc->sc_ieee) &
                            IEEE80211_ATF_SCHED_STRICT);
            break;
        case ATH_PARAM_ATF_OBSS_SCHED:
            *(int *)buff = !!(sc->sc_ieee_ops->get_atf_scheduling(sc->sc_ieee) &
                            IEEE80211_ATF_SCHED_OBSS);
            break;
        case ATH_PARAM_ATF_GROUP_SCHED_POLICY:
            *(int *)buff = !!(sc->sc_ieee_ops->get_atf_scheduling(sc->sc_ieee) &
                            IEEE80211_ATF_GROUP_SCHED_POLICY);
            break;
        case ATH_PARAM_ATF_ADJUST_POSSIBLE_TPUT:
            *(int *)buff = sc->sc_adjust_possible_tput |
                           (sc->sc_use_aggr_for_possible_tput << ATF_USE_AGGR_FOR_POSSIBLE_TPUT_SHIFT);
            break;
        case ATH_PARAM_ATF_ADJUST_TPUT:
            *(int *)buff = sc->sc_adjust_tput;
            break;
        case ATH_PARAM_ATF_ADJUST_TPUT_MORE:
            *(int *)buff = sc->sc_adjust_tput_more_thrs |
                           (sc->sc_adjust_tput_more << ATF_ADJUST_TPUT_MORE_SHIFT);
            break;
#endif
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
        case ATH_PARAM_STADFS_ENABLE:
            *(int *)buff = ieee80211com_has_cap_ext(ic,IEEE80211_CEXT_STADFS);
            break;
#endif
        case ATH_PARAM_CHANSWITCH_OPTIONS:
            (*(int *)buff) = ic->ic_chanswitch_flags;
            qdf_print(
                    "IEEE80211_CSH_OPT_NONDFS_RANDOM    0x00000001\n"
                    "IEEE80211_CSH_OPT_IGNORE_CSA_DFS   0x00000002\n"
                    "IEEE80211_CSH_OPT_CAC_APUP_BYSTA   0x00000004\n"
                    "IEEE80211_CSH_OPT_CSA_APUP_BYSTA   0x00000008\n"
                    "IEEE80211_CSH_OPT_RCSA_TO_UPLINK   0x00000010\n"
                    "IEEE80211_CSH_OPT_PROCESS_RCSA     0x00000020\n"
                    "IEEE80211_CSH_OPT_APRIORI_NEXT_CHANNEL 0x00000040\n"
                    "IEEE80211_CSH_OPT_AVOID_DUAL_CAC   0x00000080\n"
                    );
            break;
#if DBDC_REPEATER_SUPPORT
        case ATH_PARAM_PRIMARY_RADIO:
            *(int *)buff = ic->ic_primary_radio;
            break;
        case ATH_PARAM_DBDC_ENABLE:
            *(int *)buff = ic->ic_global_list->dbdc_process_enable;
            break;
        case ATH_PARAM_CLIENT_MCAST:
            *(int *)buff = ic->ic_global_list->force_client_mcast_traffic;
            break;
#endif
        case ATH_PARAM_ATH_CTL_POWER_SCALE:
            *(int *) buff = sc->sc_config.pwscale;
            break;
        case ATH_PARAM_PHY_OFDM_ERR:
            {
#define ANI_OFDM_ERR_OFFSET 10
                void *outdata = NULL;
                u_int32_t outsize;
                ath_hal_getdiagstate(sc->sc_ah, HAL_DIAG_ANI_STATS, NULL, 0, &outdata, &outsize);
                if (outdata) {
                    *(int *)buff = *((u_int32_t *)outdata + ANI_OFDM_ERR_OFFSET);
                }
            }
            break;
        case ATH_PARAM_PHY_CCK_ERR:
            {
#define ANI_CCK_ERR_OFFSET 11
                void *outdata = NULL;
                u_int32_t outsize;
                ath_hal_getdiagstate(sc->sc_ah, HAL_DIAG_ANI_STATS, NULL, 0, &outdata, &outsize);
                if (outdata) {
                    *(int *)buff = *((u_int32_t *)outdata + ANI_CCK_ERR_OFFSET);
                }
            }
            break;
        case ATH_PARAM_FCS_ERR:
            {
                struct ath_mib_mac_stats mibstats;

                ath_update_mib_macstats(dev);
                ath_get_mib_macstats(dev, &mibstats);

                *(int *)buff = mibstats.fcs_bad;
            }
            break;
        case ATH_PARAM_CHAN_UTIL:
            {
                u_int8_t ctlrxc, extrxc, rfcnt, tfcnt, obss;
                ctlrxc = sc->sc_chan_busy_per & 0xff;
                extrxc = (sc->sc_chan_busy_per & 0xff00) >> 8;
                rfcnt = (sc->sc_chan_busy_per & 0xff0000) >> 16;
                tfcnt = (sc->sc_chan_busy_per & 0xff000000) >> 24;

                if (ic->ic_curchan->ic_flags & IEEE80211_CHAN_HT20)
                    obss = ctlrxc - tfcnt;
                else
                    obss = (ctlrxc + extrxc) - tfcnt;

                *(int *)buff = obss;
            }
            break;
#if UMAC_SUPPORT_ACFG
       case ATH_PARAM_DIAG_ENABLE:
           *(int *)buff = ic->ic_diag_enable;
           break;
#endif
        case ATH_PARAM_CHAN_STATS_TH:
            *(int *)buff = ic->ic_chan_stats_th;
            break;

        case ATH_PARAM_PASSIVE_SCAN_ENABLE:
            *(int *)buff = ic->ic_strict_pscan_enable;
            break;

        case ATH_MIN_RSSI_ENABLE:
           {
                *(int *)buff = sc->sc_ieee_ops->get_enable_min_rssi(sc->sc_ieee);
           }
           break;
        case ATH_MIN_RSSI:
           {
                *(int *)buff = sc->sc_ieee_ops->get_min_rssi(sc->sc_ieee);
           }
          break;
#if DBDC_REPEATER_SUPPORT
        case ATH_PARAM_DELAY_STAVAP_UP:
            *(int *)buff = ic->ic_global_list->delay_stavap_connection;
          break;
#endif
        case ATH_PARAM_TID_OVERRIDE_QUEUE_MAPPING:
             *(int *)buff = ic->tid_override_queue_mapping;
            break;
        case ATH_PARAM_NO_VLAN:
             *(int *)buff = ic->ic_no_vlan;
            break;
        case ATH_PARAM_ATF_LOGGING:
             *(int *)buff = ic->ic_atf_logging;
             break;
        case ATH_PARAM_STRICT_DOTH:
             *(int *)buff = ic->ic_strict_doth;
             break;
#if DBDC_REPEATER_SUPPORT
        case ATH_PARAM_DISCONNECTION_TIMEOUT:
            *(int *)buff = ic->ic_global_list->disconnect_timeout;
            break;
        case ATH_PARAM_RECONFIGURATION_TIMEOUT:
            *(int *)buff = ic->ic_global_list->reconfiguration_timeout;
            break;
#endif
        case ATH_PARAM_CHANNEL_SWITCH_COUNT:
             *(int *)buff = ic->ic_chan_switch_cnt;
             break;
        case ATH_PARAM_SECONDARY_OFFSET_IE:
            *(int *)buff = ic->ic_sec_offsetie;
            break;
        case ATH_PARAM_WIDE_BAND_SUB_ELEMENT:
            *(int *)buff = ic->ic_wb_subelem;
            break;
#if DBDC_REPEATER_SUPPORT
        case ATH_PARAM_ALWAYS_PRIMARY:
            *(int *)buff = ic->ic_global_list->always_primary;
            break;
        case ATH_PARAM_FAST_LANE:
            *(int *)buff = ic->fast_lane;
            break;
        case ATH_PARAM_PREFERRED_UPLINK:
            *(int *)buff = ic->ic_preferredUplink;
            break;
        case ATH_PARAM_NF_THRESH:
            if (ic->ic_acs)
                *(int *)buff =  ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_NF_THRESH);
            else {
                qdf_print("Unable to get NF Threshold");
                return -1;
            }
            break;
#endif
        case ATH_PARAM_ACS_RANK:
            if (ic->ic_acs) {
                *(int *)buff = ieee80211_acs_get_param(ic->ic_acs, IEEE80211_ACS_RANK);
            }
            break;
#if ATH_PARAMETER_API
       case ATH_PARAM_PAPI_ENABLE:
           *(int *)buff = ic->ic_papi_enable;
           break;
#endif

       default:
            return (-1);
    }
    return (0);
}
