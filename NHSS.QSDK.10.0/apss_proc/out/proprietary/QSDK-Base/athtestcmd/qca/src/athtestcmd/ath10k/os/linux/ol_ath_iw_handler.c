/*! \file
**  \brief
**
** Copyright (c) 2004-2010, Atheros Communications Inc.
**
** Permission to use, copy, modify, and/or distribute this software for any
** purpose with or without fee is hereby granted, provided that the above
** copyright notice and this permission notice appear in all copies.
**
** THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
** WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
** MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
** ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
** WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
** ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
** OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
**
** This module is the Atheros specific ioctl/iwconfig/iwpriv interface
** to the ATH object, normally instantiated as wifiX, where X is the
** instance number (e.g. wifi0, wifi1).
**
** This provides a mechanism to configure the ATH object within the
** Linux OS enviornment.  This file is OS specific.
**
*/


/*
 * Wireless extensions support for 802.11 common code.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,38)
#ifndef AUTOCONF_INCLUDED
#include <linux/config.h>
#endif
#endif
#include <linux/version.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/utsname.h>
#include <linux/if_arp.h>       /* XXX for ARPHRD_ETHER */
#include <net/iw_handler.h>

#include "ol_if_athvar.h"
#include <osdep.h>
#include "if_athioctl.h"
#include "ol_params.h"
#include "asf_amem.h"
#include "dbglog_host.h"
#include "ol_regdomain.h"
#include <if_smart_ant.h>
#include <osif_private.h>

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
#include <osif_nss_wifiol_if.h>
#include <osif_nss_wifiol_vdev_if.h>
#endif

static char *find_ol_ath_ioctl_name(int param, int set_flag);
extern unsigned long ath_ioctl_debug;      /* defined in ath_osdep.c  */
#if ATH_SUPPORT_DSCP_OVERRIDE
int ol_ath_set_vap_dscp_tid_map(struct ieee80211vap *vap);
#endif

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
extern int ieee80211_ioctl_ald_getStatistics(struct net_device *dev, struct iw_request_info *info, void *w, char *extra);
#endif

#define TABLE_SIZE(a)    (sizeof (a) / sizeof (a[0]))

/*
** Defines
*/

/*
 * GPIO related IOCTLs
 */
#define ATH_IOCTL_GPIO_CONFIG       (SIOCIWFIRSTPRIV+6)
#define ATH_IOCTL_GPIO_OUTPUT       (SIOCIWFIRSTPRIV+7)

#define ATH_IOCTL_GET_AGGR_BURST SIOCIWFIRSTPRIV+13
#define ATH_IOCTL_SET_AGGR_BURST SIOCIWFIRSTPRIV+14

#if QCA_AIRTIME_FAIRNESS
#define ATH_IOCTL_SET_ATF_SCHED_DUR            SIOCIWFIRSTPRIV+15
#endif

#if PEER_FLOW_CONTROL
#define OL_ATH_IOCTL_GET_FC_PEER_STATS            SIOCIWFIRSTPRIV+16
#endif

/*
** "split" of config param values, since they are all combined
** into the same table.  This value is a "shift" value for ATH parameters
*/
#define OL_ATH_PARAM_SHIFT     0x1000
#define OL_SPECIAL_PARAM_SHIFT 0x2000

#if QCA_AIRTIME_FAIRNESS
 static int
 ol_ath_iw_set_atf_sched_dur(struct net_device *dev,
                         struct iw_request_info *info,
                         void *w, char *extra)
 {

     extern  int wmi_unified_pdev_set_param(wmi_unified_t wmi_handle,
             WMI_PDEV_PARAM param_id, u_int32_t param_value);

     struct ol_ath_softc_net80211 *scn =  ath_netdev_priv(dev);
     int ac = ((unsigned int *)extra)[0];
     int duration = ((unsigned int *)extra)[1];
     int retval=0;

     if (ac < WME_AC_BE || ac > WME_AC_VO)
     {
         printk("\n Input AC value range out between 0 and 3!! \n");
         return -1;
     }

     if((duration < 0) || (duration > (1 << (30 - 1))) )
     {
         printk("\n Input sched duration value range out of between 0 and  2^30-1!! \n");
         return -1;
     }

     retval = wmi_unified_pdev_set_param(scn->wmi_handle,
                       WMI_PDEV_PARAM_ATF_SCHED_DURATION,
                       (ac&0x03) << 30 | (0x3fffffff & duration));

     return retval;
}

#endif

 static int
 ol_ath_iw_get_aggr_burst(struct net_device *dev,
                                  struct iw_request_info *info,
                                  void *w,
                                  char *extra)
 {
     struct ol_ath_softc_net80211 *scn =  ath_netdev_priv(dev);
     struct iw_point *wri = (struct iw_point *)w;
     u_int32_t *dur = (u_int32_t*)extra;
     printk("%d\n", scn->aggr_burst_dur[0]);
     printk("%d\n", scn->aggr_burst_dur[1]);
     printk("%d\n", scn->aggr_burst_dur[2]);
     printk("%d\n", scn->aggr_burst_dur[3]);

     dur[0] = scn->aggr_burst_dur[0];
     dur[1] = scn->aggr_burst_dur[1];
     dur[2] = scn->aggr_burst_dur[2];
     dur[3] = scn->aggr_burst_dur[3];
     wri->length = 4;

     return 0;
 }

 static int
 ol_ath_iw_set_aggr_burst(struct net_device *dev,
                         struct iw_request_info *info,
                         void *w, char *extra)
 {

     extern  int wmi_unified_pdev_set_param(wmi_unified_t wmi_handle,
             WMI_PDEV_PARAM param_id, u_int32_t param_value);

     struct ol_ath_softc_net80211 *scn =  ath_netdev_priv(dev);
     struct ieee80211com *ic =  &scn->sc_ic;
     int ac = ((unsigned int *)extra)[0];
     int duration = ((unsigned int *)extra)[1];
     int retval=0;

     if (!ic->ic_aggr_burst_support) {
         return -1;
     }

     if (ac < WME_AC_BE || ac > WME_AC_VO)
         return -1;

     retval = wmi_unified_pdev_set_param(scn->wmi_handle,
                       WMI_PDEV_PARAM_AGGR_BURST,
                       (ac&0x0f) << 24 | (0x00ffffff & duration));
     if( EOK == retval) {
         scn->aggr_burst_dur[ac] = duration;
     }
     return EOK;
 }
enum {
    OL_SPECIAL_PARAM_COUNTRY_ID,
    OL_SPECIAL_PARAM_ASF_AMEM_PRINT,
    OL_SPECIAL_DBGLOG_REPORT_SIZE,
    OL_SPECIAL_DBGLOG_TSTAMP_RESOLUTION,
    OL_SPECIAL_DBGLOG_REPORTING_ENABLED,
    OL_SPECIAL_DBGLOG_LOG_LEVEL,
    OL_SPECIAL_DBGLOG_VAP_ENABLE,
    OL_SPECIAL_DBGLOG_VAP_DISABLE,
    OL_SPECIAL_DBGLOG_MODULE_ENABLE,
    OL_SPECIAL_DBGLOG_MODULE_DISABLE,
    OL_SPECIAL_PARAM_DISP_TPC,
    OL_SPECIAL_PARAM_ENABLE_CH_144,
    OL_SPECIAL_PARAM_REGDOMAIN,
    OL_SPECIAL_PARAM_ENABLE_OL_STATS,
    OL_SPECIAL_PARAM_ENABLE_MAC_REQ,
    OL_SPECIAL_PARAM_WLAN_PROFILE_ID_ENABLE,
    OL_SPECIAL_PARAM_WLAN_PROFILE_TRIGGER,
    OL_SPECIAL_PARAM_ENABLE_CH144_EPPR_OVRD,
    OL_SPECIAL_PARAM_ENABLE_PERPKT_TXSTATS,
};


static const struct iw_priv_args ol_ath_iw_priv_args[] = {
    /*
    ** Target HAL interface routines and parameters
    */
    { OL_ATH_PARAM_GET_IF_ID | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "get_if_path" },
    { ATH_HAL_IOCTL_SETPARAM,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2,
       0, "setHALparam" },
    { ATH_HAL_IOCTL_GETPARAM,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getHALparam" },

    /* sub-ioctl handlers */
    { ATH_HAL_IOCTL_SETPARAM,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "" },
    { ATH_HAL_IOCTL_GETPARAM,0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "" },

    { OL_HAL_CONFIG_DMA_BEACON_RESPONSE_TIME,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "DMABcnRespT" },

    /*
    ** Target device parameters
    ** This is for integer parameters
    */
    { OL_ATH_PARAM_TXCHAINMASK | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "txchainmask" },
    { OL_ATH_PARAM_TXCHAINMASK | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_txchainmask" },

    { OL_ATH_PARAM_RXCHAINMASK | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "rxchainmask" },
    { OL_ATH_PARAM_RXCHAINMASK | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_rxchainmask" },

    { OL_ATH_PARAM_AMPDU | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "AMPDU" },
    { OL_ATH_PARAM_AMPDU | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getAMPDU" },

    { OL_ATH_PARAM_AMPDU_LIMIT | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "AMPDULim" },
    { OL_ATH_PARAM_AMPDU_LIMIT | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getAMPDULim" },

    { OL_ATH_PARAM_AMPDU_SUBFRAMES | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "AMPDUFrames" },
    { OL_ATH_PARAM_AMPDU_SUBFRAMES | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getAMPDUFrames" },

    { OL_ATH_PARAM_LDPC | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "LDPC" },
    { OL_ATH_PARAM_LDPC | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getLDPC" },
    { OL_ATH_PARAM_PS_STATE_CHANGE | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "ps_report" },
    { OL_ATH_PARAM_PS_STATE_CHANGE | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "get_ps_report" },

    { OL_ATH_PARAM_NON_AGG_SW_RETRY_TH | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "nonaggr_swretry" },

    { OL_ATH_PARAM_AGG_SW_RETRY_TH | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "aggr_swretry" },

    { OL_ATH_PARAM_STA_KICKOUT_TH | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "sta_kickout" },

    { OL_ATH_PARAM_LTR_ENABLE | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "LTR" },
    { OL_ATH_PARAM_LTR_ENABLE | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getLTR" },

    { OL_ATH_PARAM_LTR_AC_LATENCY_BE | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "LTRBE" },
    { OL_ATH_PARAM_LTR_AC_LATENCY_BE | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getLTRBE" },

    { OL_ATH_PARAM_LTR_AC_LATENCY_BK | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "LTRBK" },
    { OL_ATH_PARAM_LTR_AC_LATENCY_BK | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getLTRBK" },

    { OL_ATH_PARAM_LTR_AC_LATENCY_VO | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "LTRVO" },
    { OL_ATH_PARAM_LTR_AC_LATENCY_VO | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getLTRVO" },

    { OL_ATH_PARAM_LTR_AC_LATENCY_VI | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "LTRVI" },
    { OL_ATH_PARAM_LTR_AC_LATENCY_VI | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getLTRVI" },

    { OL_ATH_PARAM_LTR_AC_LATENCY_TIMEOUT | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "LTRACTimeout" },
    { OL_ATH_PARAM_LTR_AC_LATENCY_TIMEOUT | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getLTRACTimeout" },

    { OL_ATH_PARAM_LTR_TX_ACTIVITY_TIMEOUT | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "LTRTxTimeout" },
    { OL_ATH_PARAM_LTR_TX_ACTIVITY_TIMEOUT | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getLTRTxTimeout" },

    { OL_ATH_PARAM_LTR_SLEEP_OVERRIDE | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "LTRSLEEP" },
    { OL_ATH_PARAM_LTR_SLEEP_OVERRIDE | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getLTRSLEEP" },

    { OL_ATH_PARAM_LTR_RX_OVERRIDE | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "LTRRX" },
    { OL_ATH_PARAM_LTR_RX_OVERRIDE | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getLTRRX" },

    { OL_ATH_PARAM_L1SS_ENABLE | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "L1SS" },
    { OL_ATH_PARAM_L1SS_ENABLE | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getL1SS" },

    { OL_ATH_PARAM_DSLEEP_ENABLE | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "DSLEEP" },
    { OL_ATH_PARAM_DSLEEP_ENABLE | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "getDSLEEP" },

    { OL_ATH_PARAM_BCN_BURST | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "set_bcnburst" },
    { OL_ATH_PARAM_BCN_BURST | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "get_bcnburst" },
    { OL_ATH_PARAM_ARPDHCP_AC_OVERRIDE | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "arpdhcp_ac" },
    { OL_ATH_PARAM_ARPDHCP_AC_OVERRIDE | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "g_arpdhcp_ac" },

    { OL_ATH_PARAM_IGMPMLD_OVERRIDE | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "sIgmpDscpOvrid" },
    { OL_ATH_PARAM_IGMPMLD_OVERRIDE | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "gIgmpDscpOvrid" },

    { OL_ATH_PARAM_IGMPMLD_TID | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "sIgmpDscpTidMap" },
    { OL_ATH_PARAM_IGMPMLD_TID | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "gIgmpDscpTidMap" },

    { OL_ATH_PARAM_TXPOWER_LIMIT2G | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "TXPowLim2G" },
    { OL_ATH_PARAM_TXPOWER_LIMIT2G | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getTxPowLim2G" },
    { OL_ATH_PARAM_RTS_CTS_RATE | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "setctsrate" },
    { OL_ATH_PARAM_RTS_CTS_RATE| ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_ctsrate" },
    { OL_ATH_PARAM_DEAUTH_COUNT | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "setdeauthcnt" },
    { OL_ATH_PARAM_DEAUTH_COUNT| ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getdeauthcnt" },

    { OL_ATH_PARAM_TXPOWER_LIMIT5G | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "TXPowLim5G" },
    { OL_ATH_PARAM_TXPOWER_LIMIT5G | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getTxPowLim5G" },
    { OL_ATH_PARAM_DYN_GROUPING | OL_ATH_PARAM_SHIFT,
	    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,   "dyn_grouping" },
    { OL_ATH_PARAM_DYN_GROUPING | OL_ATH_PARAM_SHIFT,
	0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "g_dyngroup" },
    { OL_ATH_PARAM_DPD_ENABLE | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,   "dpd_enable" },
    { OL_ATH_PARAM_DPD_ENABLE | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "g_dpdenable" },
    { OL_ATH_PARAM_DBGLOG_RATELIM | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,   "dbglog_ratelim" },
    { OL_ATH_PARAM_TXPOWER_SCALE | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "tpscale" },

    { OL_ATH_PARAM_TXPOWER_SCALE | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_tpscale" },

    /* tpscale reduces power from regulatory limit whereas
       ctlpwscale reduces from the final power */
    { OL_ATH_PARAM_CTL_POWER_SCALE | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "ctlpwscale" },
    { OL_ATH_PARAM_CTL_POWER_SCALE | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_ctlpwscale" },

    /* enable and disable dcs funcationality */
    { OL_ATH_PARAM_DCS | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "dcs_enable" },
    { OL_ATH_PARAM_DCS | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_dcs_enable" },
    { OL_ATH_PARAM_DCS_COCH_THR | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dcs_coch_th"},
    { OL_ATH_PARAM_DCS_COCH_THR | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_dcs_coch_th"},
    { OL_ATH_PARAM_DCS_TXERR_THR | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dcs_errth"},
    { OL_ATH_PARAM_DCS_TXERR_THR | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_dcs_errth"},
    { OL_ATH_PARAM_DCS_PHYERR_THR | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "g_dcs_phyerrth"},
    { OL_ATH_PARAM_DCS_PHYERR_THR | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "s_dcs_phyerrth"},
    { OL_ATH_PARAM_DCS_USERMAX_CU_THR | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dcs_maxcu"},
    { OL_ATH_PARAM_DCS_USERMAX_CU_THR | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_dcs_maxcu"},
    { OL_ATH_PARAM_DCS_DEBUG | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dcs_debug"},
    { OL_ATH_PARAM_DCS_DEBUG | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT |  IW_PRIV_SIZE_FIXED | 1, 0, "set_dcs_debug"},
    { OL_ATH_PARAM_BURST_DUR | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "burst_dur" },
    { OL_ATH_PARAM_BURST_DUR | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_burst_dur" },
    { OL_ATH_PARAM_BURST_ENABLE | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "burst" },
     { OL_ATH_PARAM_BURST_ENABLE | ATH_PARAM_SHIFT,
         0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_burst"},

    /* ANI configurable parameters */
    { OL_ATH_PARAM_ANI_ENABLE | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "ani_enable" },
    { OL_ATH_PARAM_ANI_ENABLE | OL_ATH_PARAM_SHIFT,
         0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "get_ani_enable" },

    { OL_ATH_PARAM_ANI_POLL_PERIOD | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "ani_poll_len" },

    { OL_ATH_PARAM_ANI_LISTEN_PERIOD | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "ani_listen_len" },

    { OL_ATH_PARAM_ANI_OFDM_LEVEL | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "ani_ofdm_level" },

    { OL_ATH_PARAM_ANI_CCK_LEVEL | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "ani_cck_level" },

    /* Enable and disable Dynamic TX Chainmask funcationality */
    { OL_ATH_PARAM_DYN_TX_CHAINMASK | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dyntxchain" },
    { OL_ATH_PARAM_DYN_TX_CHAINMASK| ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_dyntxchain" },
    { OL_ATH_PARAM_VOW_EXT_STATS | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "vow_extstats" },
    { OL_ATH_PARAM_VOW_EXT_STATS | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "g_vow_extstats" },

    { OL_ATH_PARAM_DBG_ARP_SRC_ADDR | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "arp_dbg_srcaddr" },
    { OL_ATH_PARAM_DBG_ARP_SRC_ADDR | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "g_arp_dbgstats" },

    { OL_ATH_PARAM_DBG_ARP_DST_ADDR | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "arp_dbg_dstaddr" },

    { OL_ATH_PARAM_ARP_DBG_CONF | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "arp_dbg_conf" },
    { OL_ATH_PARAM_ARP_DBG_CONF | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,         "g_arp_dbg_conf" },

   /*
   ** "special" parameters
   ** processed in this funcion
   */

    { OL_SPECIAL_PARAM_COUNTRY_ID | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "setCountryID" },
    { OL_SPECIAL_PARAM_COUNTRY_ID | OL_SPECIAL_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getCountryID" },
    { OL_SPECIAL_PARAM_REGDOMAIN | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "setRegdomain" },
    { OL_SPECIAL_PARAM_REGDOMAIN | OL_SPECIAL_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getRegdomain" },

    { OL_SPECIAL_PARAM_ENABLE_CH_144 | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "setCH144" },
    { OL_SPECIAL_PARAM_ENABLE_CH_144 | OL_SPECIAL_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getCH144" },

    { OL_SPECIAL_PARAM_ENABLE_CH144_EPPR_OVRD | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "setCH144EppOvrd" },
    { OL_SPECIAL_PARAM_ENABLE_CH144_EPPR_OVRD | OL_SPECIAL_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getCH144EppOvrd" },


    { OL_SPECIAL_PARAM_ASF_AMEM_PRINT | OL_SPECIAL_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "amemPrint" },
    { OL_SPECIAL_PARAM_ENABLE_OL_STATS | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "enable_ol_stats" },
   { OL_SPECIAL_PARAM_ENABLE_OL_STATS | OL_SPECIAL_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "ol_stats_status" },
    { OL_SPECIAL_PARAM_ENABLE_MAC_REQ | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "enable_macreq" },

    /* DBGLOG specific commands */
    { OL_SPECIAL_DBGLOG_REPORT_SIZE | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_reportsize" },
    { OL_SPECIAL_DBGLOG_TSTAMP_RESOLUTION | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_tstamprez" },
    { OL_SPECIAL_DBGLOG_REPORTING_ENABLED | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_reporten" },
    { OL_SPECIAL_DBGLOG_LOG_LEVEL | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_loglevel" },

    { OL_SPECIAL_DBGLOG_VAP_ENABLE | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_vapon" },
    { OL_SPECIAL_DBGLOG_VAP_DISABLE | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_vapoff" },
    { OL_SPECIAL_DBGLOG_MODULE_ENABLE | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_modon" },
    { OL_SPECIAL_DBGLOG_MODULE_DISABLE| OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "dl_modoff" },
    { OL_SPECIAL_PARAM_DISP_TPC | OL_SPECIAL_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "disp_tpc" },
    { OL_ATH_PARAM_DSCP_TID_MAP | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "dscp_tid_map" },
    { OL_SPECIAL_PARAM_WLAN_PROFILE_ID_ENABLE | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "prof_id_enable" },
    { OL_SPECIAL_PARAM_WLAN_PROFILE_TRIGGER | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "prof_trigger" },

#if ATH_SUPPORT_DSCP_OVERRIDE
	{ OL_ATH_PARAM_HMMC_DSCP_OVERRIDE | OL_ATH_PARAM_SHIFT,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "sHmmcDscpOvrid" },
	{ OL_ATH_PARAM_HMMC_DSCP_OVERRIDE | OL_ATH_PARAM_SHIFT,
		0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "gHmmcDscpOvrid" },
	{ OL_ATH_PARAM_HMMC_DSCP_TID_MAP | OL_ATH_PARAM_SHIFT,
		IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,      "sHmmcDscpTidMap" },
	{ OL_ATH_PARAM_HMMC_DSCP_TID_MAP | OL_ATH_PARAM_SHIFT,
		0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "gHmmcDscpTidMap" },
#endif

#if UMAC_SUPPORT_PERIODIC_PERFSTATS
    { OL_ATH_PARAM_PRDPERFSTAT_THRPUT_ENAB | OL_ATH_PARAM_SHIFT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "thrput_enab" },
    { OL_ATH_PARAM_PRDPERFSTAT_THRPUT_ENAB | OL_ATH_PARAM_SHIFT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_thrput_enab" },
    { OL_ATH_PARAM_PRDPERFSTAT_THRPUT_WIN | OL_ATH_PARAM_SHIFT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "thrput_win" },
    { OL_ATH_PARAM_PRDPERFSTAT_THRPUT_WIN | OL_ATH_PARAM_SHIFT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_thrput_win" },
    { OL_ATH_PARAM_PRDPERFSTAT_THRPUT | OL_ATH_PARAM_SHIFT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_thrput" },
    { OL_ATH_PARAM_PRDPERFSTAT_PER_ENAB | OL_ATH_PARAM_SHIFT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "PER_enab" },
    { OL_ATH_PARAM_PRDPERFSTAT_PER_ENAB | OL_ATH_PARAM_SHIFT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_PER_enab" },
    { OL_ATH_PARAM_PRDPERFSTAT_PER_WIN | OL_ATH_PARAM_SHIFT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "PER_win" },
    { OL_ATH_PARAM_PRDPERFSTAT_PER_WIN | OL_ATH_PARAM_SHIFT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_PER_win" },
    { OL_ATH_PARAM_PRDPERFSTAT_PER | OL_ATH_PARAM_SHIFT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_prdic_PER" },
#endif /* UMAC_SUPPORT_PERIODIC_PERFSTATS */

    { OL_ATH_PARAM_TOTAL_PER | OL_ATH_PARAM_SHIFT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_total_PER" },

#if ATH_RX_LOOPLIMIT_TIMER
    { OL_ATH_PARAM_LOOPLIMIT_NUM | OL_ATH_PARAM_SHIFT,
    IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "rxintr_delay" },
    { OL_ATH_PARAM_LOOPLIMIT_NUM | ATH_PARAM_SHIFT,
    0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_rxintr_delay"},
#endif

    { OL_ATH_PARAM_ANTENNA_GAIN_2G | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "antgain_2g" },
    { OL_ATH_PARAM_ANTENNA_GAIN_5G | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "antgain_5g" },

    { OL_ATH_PARAM_RX_FILTER | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_rxfilter" },

    { OL_ATH_PARAM_ENABLE_AMSDU | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "enable_amsdu" },

    { OL_ATH_PARAM_ENABLE_AMSDU | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_amsdu" },

    { OL_ATH_PARAM_ENABLE_AMPDU | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "enable_ampdu" },

    { OL_ATH_PARAM_ENABLE_AMPDU | ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_ampdu" },

    /*
    ** The Get Country/Set Country interface
    */
    { ATH_IOCTL_SETCOUNTRY, IW_PRIV_TYPE_CHAR | 3, 0,       "setCountry" },
    { ATH_IOCTL_GETCOUNTRY, 0, IW_PRIV_TYPE_CHAR | 3,       "getCountry" },

    /*
    ** The Get MAC address/Set MAC address interface
    */

    { ATH_IOCTL_SETHWADDR,
        IW_PRIV_TYPE_CHAR | ((IEEE80211_ADDR_LEN * 3) - 1), 0,  "setHwaddr" },
    { ATH_IOCTL_GETHWADDR,
        0, IW_PRIV_TYPE_CHAR | ((IEEE80211_ADDR_LEN * 3) - 1),  "getHwaddr" },

#if UNIFIED_SMARTANTENNA
    { ATH_IOCTL_SET_SMART_ANTENNA, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4,
        0, "set_sa_param" },
    { ATH_IOCTL_GET_SMART_ANTENNA,IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "get_sa_param" },
#endif

    /*
    ** GPIO Configuration interface
    */
    { ATH_IOCTL_GPIO_CONFIG,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4, 0, "gpio_config" },
    { ATH_IOCTL_GPIO_OUTPUT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "gpio_output" },

     { ATH_IOCTL_SET_AGGR_BURST,
         IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "aggr_burst" },
     { ATH_IOCTL_GET_AGGR_BURST,
         0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 4, "get_aggr_burst" },
     /* SIFS BURST */
#if QCA_AIRTIME_FAIRNESS
     { ATH_IOCTL_SET_ATF_SCHED_DUR,
         IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "atf_sched_dur" },
#endif

#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    { OL_ATH_PARAM_BUFF_THRESH | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_buff_thresh"},
    { OL_ATH_PARAM_BUFF_THRESH | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_buff_thresh"},
#endif
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
    { OL_ATH_PARAM_BLK_REPORT_FLOOD | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getBlkReportFld"},
    { OL_ATH_PARAM_BLK_REPORT_FLOOD | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setBlkReportFld"},
    { OL_ATH_PARAM_DROP_STA_QUERY | ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getDropSTAQuery"},
    { OL_ATH_PARAM_DROP_STA_QUERY | ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setDropSTAQuery"},
#endif
    { OL_ATH_PARAM_ACS_ENABLE_BK_SCANTIMEREN |OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "acs_bkscanen" },
    { OL_ATH_PARAM_ACS_ENABLE_BK_SCANTIMEREN |OL_ATH_PARAM_SHIFT, 0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "g_acs_bkscanen" },
    { OL_ATH_PARAM_ACS_SCANTIME |OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,      "acs_scanintvl" },
    { OL_ATH_PARAM_ACS_SCANTIME |OL_ATH_PARAM_SHIFT, 0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "g_acsscanintvl" },
    { OL_ATH_PARAM_ACS_RSSIVAR |OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,        "acs_rssivar" },
    { OL_ATH_PARAM_ACS_RSSIVAR |OL_ATH_PARAM_SHIFT,  0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_acs_rssivar" },
    { OL_ATH_PARAM_ACS_CHLOADVAR|OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,      "acs_chloadvar" },
    { OL_ATH_PARAM_ACS_CHLOADVAR|OL_ATH_PARAM_SHIFT, 0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "g_acschloadvar" },
    { OL_ATH_PARAM_ACS_LIMITEDOBSS|OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,        "acs_lmtobss" },
    { OL_ATH_PARAM_ACS_LIMITEDOBSS|OL_ATH_PARAM_SHIFT, 0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,        "get_acslmtobss" },
    { OL_ATH_PARAM_ACS_CTRLFLAG|OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,      "acs_ctrlflags" },
    { OL_ATH_PARAM_ACS_CTRLFLAG|OL_ATH_PARAM_SHIFT,    0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,     "getacsctrlflags" },
    { OL_ATH_PARAM_ACS_DEBUGTRACE|OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "acs_dbgtrace" },
    { OL_ATH_PARAM_ACS_DEBUGTRACE|OL_ATH_PARAM_SHIFT,  0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "g_acs_dbgtrace" },
#if ATH_CHANNEL_BLOCKING
    { OL_ATH_PARAM_ACS_BLOCK_MODE | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "acs_bmode" },
    { OL_ATH_PARAM_ACS_BLOCK_MODE | OL_ATH_PARAM_SHIFT, 0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "g_acs_bmode" },
#endif
    { OL_ATH_PARAM_RESET_OL_STATS|OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,     "reset_ol_stats" },
    { OL_ATH_PARAM_SET_FW_HANG_ID | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_fw_hang" },
    { OL_ATH_PARAM_FW_RECOVERY_ID | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "set_fw_recovery" },
    { OL_ATH_PARAM_FW_RECOVERY_ID | OL_ATH_PARAM_SHIFT,  0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_fw_recovery" },
    { OL_ATH_PARAM_RADIO_TYPE | OL_ATH_PARAM_SHIFT,  0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_radio_type" },
    { OL_ATH_PARAM_DISABLE_DFS | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "blockdfslist" },
    { OL_ATH_PARAM_DISABLE_DFS | OL_ATH_PARAM_SHIFT,0,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_blockdfslist" },
    /* Qboost enable and disable per peer */
    { OL_ATH_PARAM_QBOOST | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "qboost_enable" },
    { OL_ATH_PARAM_SIFS_FRMTYPE | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "sifs_frmtype" },
    { OL_ATH_PARAM_SIFS_UAPSD | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "sifs_uapsd" },

    { OL_ATH_PARAM_BLOCK_INTERBSS | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "block_interbss" },
    { OL_ATH_PARAM_BLOCK_INTERBSS | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_blk_interbss" },
    { OL_ATH_PARAM_FW_DISABLE_RESET | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "disable_reset" },
    { OL_ATH_PARAM_MSDU_TTL | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "msdu_ttl" },
    { OL_ATH_PARAM_PPDU_DURATION | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,         "ppdu_duration" },
    { OL_ATH_PARAM_SET_TXBF_SND_PERIOD | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "txbf_snd_int" },
    { OL_ATH_PARAM_SET_TXBF_SND_PERIOD | OL_ATH_PARAM_SHIFT,  0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,   "get_txbf_snd" },
    { OL_ATH_PARAM_ALLOW_PROMISC | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "promisc" },
    { OL_ATH_PARAM_ALLOW_PROMISC | OL_ATH_PARAM_SHIFT,
      0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_promisc" },
    { OL_ATH_PARAM_BURST_MODE | OL_ATH_PARAM_SHIFT,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,"burst_mode" },
#if ATH_SUPPORT_WRAP
    { OL_ATH_PARAM_MCAST_BCAST_ECHO |OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "mcast_echo" },
    { OL_ATH_PARAM_MCAST_BCAST_ECHO |OL_ATH_PARAM_SHIFT, 0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "g_mcast_echo" },
#endif
#if QCA_AIRTIME_FAIRNESS
     { OL_ATH_PARAM_ATF_STRICT_SCHED | OL_ATH_PARAM_SHIFT,
         IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "atfstrictsched" },
     { OL_ATH_PARAM_ATF_STRICT_SCHED | OL_ATH_PARAM_SHIFT,0,
         IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "gatfstrictsched" },
     { OL_ATH_PARAM_ATF_OBSS_SCHED | OL_ATH_PARAM_SHIFT,
         IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "atfobsssched" },
     { OL_ATH_PARAM_ATF_OBSS_SCHED | OL_ATH_PARAM_SHIFT,0,
         IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "g_atfobsssched" },
     { OL_ATH_PARAM_ATF_OBSS_SCALE | OL_ATH_PARAM_SHIFT,
         IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "atfobssscale" },
#endif
     { OL_ATH_PARAM_OBSS_RSSI_THRESHOLD |OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "obss_rssi_th" },
     { OL_ATH_PARAM_OBSS_RSSI_THRESHOLD |OL_ATH_PARAM_SHIFT, 0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "gobss_rssi_th" },
     { OL_ATH_PARAM_OBSS_RX_RSSI_THRESHOLD |OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "obss_rx_rssi_th" },
     { OL_ATH_PARAM_OBSS_RX_RSSI_THRESHOLD |OL_ATH_PARAM_SHIFT, 0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "gobss_rx_rssi_th" },
     { OL_ATH_PARAM_ACS_TX_POWER_OPTION |OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "acs_txpwr_opt" },
     { OL_ATH_PARAM_ACS_TX_POWER_OPTION | OL_ATH_PARAM_SHIFT, 0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "g_acs_txpwr_opt" },
     { OL_ATH_PARAM_ANT_POLARIZATION | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "antenna_plzn" },
     { OL_ATH_PARAM_PRINT_RATE_LIMIT | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "print_ratelimit" },
     { OL_ATH_PARAM_PRINT_RATE_LIMIT | OL_ATH_PARAM_SHIFT, 0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,      "gprint_ratelimit" },
     { OL_ATH_PARAM_PDEV_RESET | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,    "pdev_reset" },
     { OL_ATH_PARAM_FW_DUMP_NO_HOST_CRASH | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,    "set_nohostcrash" },
     { OL_ATH_PARAM_FW_DUMP_NO_HOST_CRASH | OL_ATH_PARAM_SHIFT,  0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_nohostcrash" },
     { OL_ATH_PARAM_CONSIDER_OBSS_NON_ERP_LONG_SLOT | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,    "obss_long_slot" },
     { OL_ATH_PARAM_CONSIDER_OBSS_NON_ERP_LONG_SLOT | OL_ATH_PARAM_SHIFT,  0,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "g_obss_long_slot" },
#if PEER_FLOW_CONTROL
     /*Peer Stats*/
    { OL_ATH_PARAM_STATS_FC | OL_ATH_PARAM_SHIFT,
       0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "fc_stats_global" },
    { OL_ATH_IOCTL_GET_FC_PEER_STATS,
        IW_PRIV_TYPE_CHAR | ((IEEE80211_ADDR_LEN * 3) - 1), 0,  "fc_peer_stats" },

     /* Peer Flow control and Queue Map parameters */
    { OL_ATH_PARAM_QFLUSHINTERVAL | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "fc_flush_intv" },
    { OL_ATH_PARAM_QFLUSHINTERVAL | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "g_fc_flush_intv" },
    { OL_ATH_PARAM_TOTAL_Q_SIZE | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "fc_buf_max" },
    { OL_ATH_PARAM_TOTAL_Q_SIZE | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "g_fc_buf_max" },
    { OL_ATH_PARAM_MAX_Q_LIMIT | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "fc_q_max" },
    { OL_ATH_PARAM_MAX_Q_LIMIT | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "g_fc_q_max" },
    { OL_ATH_PARAM_MIN_THRESHOLD | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "fc_buf_min" },
    { OL_ATH_PARAM_MIN_THRESHOLD | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "g_fc_buf_min" },
    { OL_ATH_PARAM_MIN_Q_LIMIT | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "fc_q_min" },
    { OL_ATH_PARAM_MIN_Q_LIMIT | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "g_fc_q_min" },
    { OL_ATH_PARAM_CONG_CTRL_TIMER_INTV | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "fc_cong_timer" },
    { OL_ATH_PARAM_CONG_CTRL_TIMER_INTV | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "g_fc_cong_timer" },
    { OL_ATH_PARAM_STATS_TIMER_INTV | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "fc_stats_timer" },
    { OL_ATH_PARAM_STATS_TIMER_INTV | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "g_fc_stats_timer" },
    { OL_ATH_PARAM_ROTTING_TIMER_INTV | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "fc_rot_timer" },
    { OL_ATH_PARAM_ROTTING_TIMER_INTV | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "g_fc_rot_timer" },
    { OL_ATH_PARAM_LATENCY_PROFILE | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "fc_latency_on" },
    { OL_ATH_PARAM_HOSTQ_DUMP | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "fc_hostq_dump" },
    { OL_ATH_PARAM_TIDQ_MAP | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "fc_tidq_map" },
#endif
    { OL_ATH_PARAM_DISABLE_STA_VAP_AMSDU | OL_ATH_PARAM_SHIFT,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,    "st_ht_noamsdu" },
    { OL_ATH_PARAM_DISABLE_STA_VAP_AMSDU | OL_ATH_PARAM_SHIFT,  0,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "g_st_ht_noamsdu" },
#if ATH_SUPPORT_DFS && ATH_SUPPORT_STA_DFS
    { OL_ATH_PARAM_STADFS_ENABLE | ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "staDFSEn" },
    { OL_ATH_PARAM_STADFS_ENABLE | OL_ATH_PARAM_SHIFT,0,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "get_staDFSEn" },
#endif
#if DBDC_REPEATER_SUPPORT
    { OL_ATH_PARAM_PRIMARY_RADIO | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "primaryradio" },
    { OL_ATH_PARAM_PRIMARY_RADIO | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getprimaryradio" },
    { OL_ATH_PARAM_DBDC_ENABLE | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "dbdc_enable" },
    { OL_ATH_PARAM_DBDC_ENABLE | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_dbdc_enable" },
    { OL_ATH_PARAM_CLIENT_MCAST | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "client_mcast" },
    { OL_ATH_PARAM_CLIENT_MCAST | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "getclient_mcast" },

#endif
#if ATH_DATA_TX_INFO_EN
    { OL_SPECIAL_PARAM_ENABLE_PERPKT_TXSTATS | OL_SPECIAL_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "en_data_txstats" },
    { OL_SPECIAL_PARAM_ENABLE_PERPKT_TXSTATS | OL_SPECIAL_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_data_txstats" },
#endif
    { OL_ATH_PARAM_TXPOWER_DBSCALE | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "tpdbscale" },
    { OL_ATH_PARAM_PHY_OFDM_ERR | ATH_PARAM_SHIFT,    0,
     IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,"g_phyofdmerr" },
    { OL_ATH_PARAM_PHY_CCK_ERR | ATH_PARAM_SHIFT,    0,
     IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,"g_phycckerr" },
    { OL_ATH_PARAM_FCS_ERR | ATH_PARAM_SHIFT,    0,
     IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,"g_fcserr" },
    { OL_ATH_PARAM_CHAN_UTIL | ATH_PARAM_SHIFT,    0,
     IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,"g_chanutil" },
#if QCA_EMIWAR_80P80_CONFIG_SUPPORT
    { OL_ATH_PARAM_EMIWAR_80P80 | OL_ATH_PARAM_SHIFT,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,       "emiwar80p80" },
#endif /*QCA_EMIWAR_80P80_CONFIG_SUPPORT*/
    { OL_ATH_PARAM_EMIWAR_80P80 | OL_ATH_PARAM_SHIFT,
        0, IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,       "get_emiwar80p80" },

};



/******************************************************************************/
/*!
**  \brief Set ATH/HAL parameter value
**
**  Interface routine called by the iwpriv handler to directly set specific
**  HAL parameters.  Parameter ID is stored in the above iw_priv_args structure
**
**  \param dev Net Device pointer, Linux driver handle
**  \param info Request Info Stucture containing type info, etc.
**  \param w Pointer to a word value, not used here
**  \param extra Pointer to the actual data buffer.
**  \return 0 for success
**  \return Non Zero on error
*/
/**
 * @brief
 *    - iwpriv description:\n
 *         - iwpriv cmd: iwpriv wifiN 6MBAck 1|0\n
 *           This command will enable (1) or disable (0) the use of the
 *           6 MB/s (OFDM) data rate for ACK frames.  If disabled, ACK
 *           frames will be sent at the CCK rate.  The default value is
 *           0.  The command has a corresponding get command to return
 *           the current value.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: AddSWBbo\n
 *           Adjust the calculation of the ready time for the QoS queues
 *           to adjust the QoS queue performance for optimal timing. These
 *           In the AP application they are not relevant, so they should
 *           not be modified. Their default value is zero.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN SWBcnRespT Response Time\n
 *           These settings are used to adjust the calculation of the ready
 *           time for the QoS queues.  This is used to adjust the performance
 *           of the QoS queues for optimal timing.  The parameter Software
 *           Beacon Response Time represents the time, in microseconds,
 *           required to process beacons in software.  The DMA Beacon Response
 *           Time is the time required to transfer the beacon message from
 *           Memory into the MAC queue.  Finally, the Additional Software
 *           Beacon Backoff is a fudge factor used for final adjustment
 *           of the ready time offset.
 *           These parameters are used for experimental adjustment of queue
 *           performance.  In the AP application they are not relevant,
 *           so they should not be modified.  Their default value is zero.
 *            Each parameter has a corresponding get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN DMABcnRespT Response Time\n
 *           These settings are used to adjust the calculation of the ready
 *           time for the QoS queues.  This is used to adjust the performance
 *           of the QoS queues for optimal timing.  The parameter Software
 *           Beacon Response Time represents the time, in microseconds,
 *           required to process beacons in software.  The DMA Beacon Response
 *           Time is the time required to transfer the beacon message from
 *           Memory into the MAC queue.  Finally, the Additional Software
 *           Beacon Backoff is a fudge factor used for final adjustment
 *           of the ready time offset.
 *           These parameters are used for experimental adjustment of queue
 *           performance.  In the AP application they are not relevant,
 *           so they should not be modified.  Their default value is zero.
 *            Each parameter has a corresponding get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN 6MBAck 1|0\n
 *           This command will enable (1) or disable (0) the use of the
 *           6 MB/s (OFDM) data rate for ACK frames.  If disabled, ACK
 *           frames will be sent at the CCK rate.  The default value is
 *           0.  The command has a corresponding get command to return
 *           the current value.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN AggrProtDur duration \n
 *           These are used to enable RTS/CTS protection on aggregate frames,
 *           and control the size of the frames receiving RTS/CTS protection.
 *            These are typically used as a test commands to set a specific
 *           condition in the driver.  The AggrProt command is used to
 *           enable (1) or disable (0) this function.  Default is disabled.
 *            The AggrProtDur setting indicates the amount of time to add
 *           to the duration of the CTS period to allow for additional
 *           packet bursts before a new RTS/CTS is required.  Default is
 *           8192 microseconds.  The AggrProtMax command is used to indicate
 *           the largest aggregate size to receive RTS/CTS protection.
 *            The default is 8192 bytes.  These commands have associated
 *           get commands.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 8192 microseconds
 *             .
 *         - iwpriv cmd: iwpriv wifiN AggrProtMax size\n
 *           These are used to enable RTS/CTS protection on aggregate frames,
 *           and control the size of the frames receiving RTS/CTS protection.
 *            These are typically used as a test commands to set a specific
 *           condition in the driver.  The AggrProt command is used to
 *           enable (1) or disable (0) this function.  Default is disabled.
 *            The AggrProtDur setting indicates the amount of time to add
 *           to the duration of the CTS period to allow for additional
 *           packet bursts before a new RTS/CTS is required.  Default is
 *           8192 microseconds.  The AggrProtMax command is used to indicate
 *           the largest aggregate size to receive RTS/CTS protection.
 *            The default is 8192 bytes.  These commands have associated
 *           get commands.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 8192 bytes
 *             .
 *         - iwpriv cmd: iwpriv wifiN AMPDU 1|0 \n
 *           This is used to enable/disable transmit AMPDU aggregation
 *           for the entire interface.  Receiving of aggregate frames will
 *           still be performed, but no aggregate frames will be transmitted
 *           if this is disabled.  This has a corresponding get command,
 *           and the default value is 1 (enabled).
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: enabled
 *             .
 *         - iwpriv cmd: iwpriv wifiN AMPDUFrames numFrames\n
 *           This command will set the maximum number of subframes to place
 *           into an AMPDU aggregate frame.  Frames are added to an aggregate
 *           until either a) the transmit duration is exceeded, b) the
 *           number of subframes is exceeded, c) the maximum number of
 *           bytes is exceeded, or d) the corresponding queue is empty.
 *            The subframe that causes the excess conditions will not be
 *           included in the aggregate frame, but will be queued up to
 *           be transmitted with the next aggregate frame.
 *           The default value is 32.  This command has a corresponding get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 32
 *             .
 *         - iwpriv cmd: iwpriv wifiN AMPDULim Byte Limit\n
 *           This parameter will limit the number of bytes included in
 *           an AMPDU aggregate frame.  Frames are added to an aggregate
 *           until either a) the transmit duration is exceeded, b) the
 *           number of subframes is exceeded, c) the maximum number of
 *           bytes is exceeded, or d) the corresponding queue is empty.
 *            The subframe that causes the excess conditions will not be
 *           included in the aggregate frame, but will be queued up to
 *           be transmitted with the next aggregate frame.
 *           The default value of this parameter is 50000.  This command
 *           has a corresponding get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 50000
 *             .
 *         - iwpriv cmd: iwpriv wifiN ANIEna 0|1\n
 *           This parameter enables the Automatic Noise Immunity (ANI)
 *           processing in both the driver and the baseband unit.  ANI
 *           is used to mitigate unpredictable noise spurs in receive channels
 *           that are due to the host system that the device is installed
 *           in.  This feature was added for CardBus and PCIE devices sold
 *           in the retail market that were not pre-installed in host systems.
 *            Most AP implementations will not enable ANI, preferring to
 *           limit noise spurs by design.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: Not sure
 *             .
 *         - iwpriv cmd: iwpriv wifiN AntSwap 1|0 \n
 *           These commands are used to control antenna switching behavior.
 *            For 11n devices, these control which chains are used for
 *           transmit.  For Legacy devices, these are used to determine
 *           if diversity switching is enabled or disabled.  The AntSwap
 *           command is used to indicate when antenna A and B are swapped
 *           from the usual configuration.  This will cause Antenna A
 *           to be used by chain 1 or 2, and Antenna B will be used
 *           by chain 0.  The default is 0, which indicates antennas are
 *           not swapped (Antenna A to chain 0, Antenna B to chain
 *           1,2).  The Diversity Control command will enable/disable antenna
 *           switching altogether.  If Diversity control is set to Antenna
 *           A (1) or Antenna B (2), the transmitting antenna will
 *           not change based on receive signal strength.  If Diversity
 *           Control is set to Variable (0), then the transmitting
 *           antenna will be selected based on received signal strength.
 *            Both commands have associated get commands.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN DivtyCtl AntSel\n
 *           These commands are used to control antenna switching behavior.
 *            For 11n devices, these control which chains are used for
 *           transmit.  For Legacy devices, these are used to determine
 *           if diversity switching is enabled or disabled.  The AntSwap
 *           command is used to indicate when antenna A and B are swapped
 *           from the usual configuration.  This will cause Antenna A
 *           to be used by chain 1 or 2, and Antenna B will be used
 *           by chain 0.  The default is 0, which indicates antennas are
 *           not swapped (Antenna A to chain 0, Antenna B to chain
 *           1,2).  The Diversity Control command will enable/disable antenna
 *           switching altogether.  If Diversity control is set to Antenna
 *           A (1) or Antenna B (2), the transmitting antenna will
 *           not change based on receive signal strength.  If Diversity
 *           Control is set to Variable (0), then the transmitting
 *           antenna will be selected based on received signal strength.
 *            Both commands have associated get commands.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN BcnNoReset 1|0 \n
 *           This controls a debug flag that will either reset the chip
 *           or not when a stuck beacon is detected.  If enabled (1), the
 *           system will NOT reset the chip upon detecting a stuck beacon,
 *           but will dump several registers to the console.  Additional
 *           debug messages will be output if enabled, also.  The default
 *           value is 0.  This command has a corresponding get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN CABlevel % Multicast \n
 *           This will set the amount of space that can be used by Multicast
 *           traffic in the Content After Beacon (CAB) queue.  CAB frames
 *           are also called Beacon Gated Traffic frames, and are sent
 *           attached to every beacon. In certain situations, there may
 *           be such a large amount of multicast traffic being transmitted
 *           that there is no time left to send management or Best Effort
 *           (BE) traffic.  TCP traffic gets starved out in these situations.
 *            This parameter controls how much of the CAB queue can be
 *           used by Multicast traffic, freeing the remainder for BE traffic.
 *            The default value of this parameter is 80 (80% Multicast),
 *           and the command has a corresponding get command
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 80
 *             .
 *         - iwpriv cmd: iwpriv wifiN CCKWeakThr 1|0\n
 *           This command will select either normal (0) or weak (1) CCK
 *           signal detection thresholds in the baseband.  This is used
 *           to toggle between a more sensitive threshold and a less sensitive
 *           one, as part of the Adaptive Noise Immunity (ANI) algorithm.
 *            The actual settings are set at the factory, and are stored
 *           in EEPROM.  If ANI is enabled, this parameter may be changed
 *           independent of operator setting, so this command may be overridden
 *           during operation.
 *           The default value for this parameter is 0.  This command has
 *           a corresponding Get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN chainmasksel 1|0\n
 *           This command enables (1) automatic chainmask selection.  This
 *           feature allows the system to select between 2 and 3 transmit
 *           chains, depending on the signal quality on the channel.  For
 *           stations that are distant, using 3 chain transmit allows for
 *           better range performance.  The default value of this parameter
 *           is 0 (disabled).  This command has a corresponding get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: CKTrgLow\n
 *           Check CTKrgHi
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: Not sure
 *             .
 *         - iwpriv cmd: iwpriv wifiN CCKTrgHi High Threshold\n
 *           These commands control the CCK PHY Error/sec threshold settings
 *           for the ANI immunity levels.  A PHY error rate below the low
 *           trigger will cause the ANI algorithm to lower immunity thresholds,
 *           and a PHY error rate exceeding the high threshold will cause
 *           immunity thresholds to be increased.  When a limit is exceed,
 *           the ANI algorithm will modify one of several baseband settings
 *           to either increase or decrease sensitivity.  Thresholds are
 *           increased/decreased in the following order:
 *           Increase:
 *           raise Noise Immunity level to MAX from 0, if Spur Immunity level is at MAX;
 *           raise Noise Immunity level to next level from non-zero value;
 *           raise Spur Immunity Level;
 *           (if using CCK rates)  raise CCK weak signal threshold + raise
 *           FIR Step level;
 *           Disable ANI PHY Err processing to reduce CPU load
 *           Decrease:
 *           lower Noise Immunity level;
 *           lower FIR step level;
 *           lower CCK weak signal threshold;
 *           lower Spur Immunity level;
 *           The default values for these settings are 200 errors/sec for
 *           the high threshold, and 100 errors/sec for the low threshold.
 *           Each of these commands has a corresponding get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 200 errors/sec
 *             .
 *         - iwpriv cmd: iwpriv wifiN CWMIgnExCCA 1|0\n
 *           This command allows the system to ignore the clear channel
 *           assessment (CCA) on the extension channel for 11n devices
 *           operating in HT 40 mode.  Normally, to transmit, the device
 *           will require no energy detected on both the control and extension
 *           channels for a minimum of a PIFS duration.  This control will
 *           allow for ignoring energy on the extension channel.  This
 *           is not in conformance with the latest draft of the 802.11n
 *           specifications, and should only be used in a test mode.  The
 *           default value for this parameter is 0 (do NOT ignore extension
 *           channel CCA).  This command has a corresponding get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN FIRStepLvl level\n
 *           This command will adjust the FIR filter parameter that determines
 *           when a signal is in band for weak signal detection.  Raising
 *           this level reduces the likelihood of adjacent channel interferers
 *           causing a large number of (low RSSI) PHY errors, lowering
 *           the level allows easier weak signal detection for extended
 *           range.  This parameter is also modified by the ANI algorithm,
 *           so this may be change dynamically during operation, usually
 *           in steps of single units.  The default value for this parameter
 *           is 0.  The command has a corresponding Get command, but this
 *           returns the initialization (starting) value, and not the value
 *           that is currently in the operating registers.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN ForceBias Bias\n
 *           This command activates the force bias feature.  This is
 *           used as a workaround to a directional sensitivity issue in
 *           select the bias level depending on the selected frequency.
 *            ForceBias will set the bias to a value between 0 and 7.
 *           These commands are only available when the driver is compiled
 *           with the #define ATH_FORCE_BIAS parameter defined.  Even when
 *           this switch is enabled, the default values for both parameters
 *           are 0 (disabled).  This should only be enabled if the sensitivity
 *           issue is actually present.
 *           The command has corresponding Get commands
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN ForBiasAuto 1|0\n
 *           This command activates the force bias feature.  This is
 *           used as a workaround to a directional sensitivity issue in
 *           select the bias level depending on the selected frequency.
 *            ForceBias will set the bias to a value between 0 and 7.
 *           These commands are only available when the driver is compiled
 *           with the #define ATH_FORCE_BIAS parameter defined.  Even when
 *           this switch is enabled, the default values for both parameters
 *           are 0 (disabled).  This should only be enabled if the sensitivity
 *           issue is actually present.
 *           The command has corresponding Get commands
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN HALDbg debug level\n
 *           Used to set the debug level in the HAL code.  This can be
 *           modified on the fly as required.  The HAL must be built
 *           with the AH_DEBUG parameter defined for this command to be
 *           available; otherwise it is conditionally compiled out.  The
 *           value provided is a bitmask selecting specific categories
 *           of debug information to select from.  Note that some categories
 *           will produce copious amounts of output, and should be used
 *           sparingly for a few seconds.
 *           Table 8 HAL Debug Flags
 *           Symbolic Name	Enable Bit	Description
 *           and initialization
 *           HAL_DBG_PHY_IO	0x00000002	PHY read/write states
 *           HAL_DBG_REG_IO	0x00000004	Register I/O, including all register
 *           values.  Use with caution
 *           HAL_DBG_RF_PARAM	0x00000008	RF Parameter information, and table settings.
 *           HAL_DBG_QUEUE	0x00000010	Queue management for WMM support
 *           HAL_DBG_EEPROM_DUMP	0x00000020	Large dump of EEPROM information.
 *            System must be compiled with the EEPROM_DUMP conditional
 *           variable defined
 *           HAL_DBG_EEPROM	0x00000040	EEPROM read/write and status information
 *           HAL_DBG_NF_CAL	0x00000080	Noise Floor calibration debug information
 *           HAL_DBG_CALIBRATE	0x00000100	All other calibration debug information
 *           HAL_DBG_CHANNEL	0x00000200	Channel selection and channel settings
 *           HAL_DBG_INTERRUPT	0x00000400	Interrupt processing.  WARNING:
 *           this produces a LOT of output, use in short bursts.
 *           HAL_DBG_DFS	0x00000800	DFS settings
 *           HAL_DBG_DMA	0x00001000	DMA debug information
 *           HAL_DBG_REGULATORY	0x00002000	Regulatory table settings and selection
 *           HAL_DBG_TX	0x00004000	Transmit path information
 *           HAL_DBG_TXDESC	0x00008000	Transmit descriptor processing
 *           HAL_DBG_RX	0x00010000	Receive path information
 *           HAL_DBG_RXDESC	0x00020000	Receive descriptor processing
 *           HAL_DBG_ANI	0x00040000	Debug information for Automatic Noise Immunity (ANI)
 *           HAL_DBG_BEACON	0x00080000	Beacon processing and setup information
 *           HAL_DBG_KEYCACHE	0x00100000	Encryption Key management
 *           HAL_DBG_POWER_MGMT	0x00200000	Power and Tx Power level management
 *           HAL_DBG_MALLOC	0x00400000	Memory allocation
 *           HAL_DBG_FORCE_BIAS	0x00800000	Force Bias related processing
 *           HAL_DBG_POWER_OVERRIDE	0x01000000	TX Power Override processing
 *           HAL_DBG_UNMASKABLE	0xFFFFFFFF	Will be printed in all cases
 *           if AH_DEBUG is defined.
 *
 *           This command has a corresponding get command.  Its default
 *           value is 0 (no debugging on), but this does not disable the
 *           Unmaskable prints.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: hbrPER_high\n
 *           Cannot find in AP Manual
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: Not sure
 *             .
 *         - iwpriv cmd: hbrPER_low\n
 *           Cannot find in AP Manual
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: Not sure
 *             .
 *         - iwpriv cmd: iwpriv wifiN HTEna 1|0\n
 *           This command is used to enable (1) or disable (0) 11N (HT)
 *           data rates.  This is normally only used as a test command.
 *            The parameter is set to 1 (enabled) by default.  The command
 *           has a corresponding Get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 1
 *             .
 *         - iwpriv cmd: iwpriv wifiN NoiseImmLvl level\n
 *           This will select a specific noise immunity level parameter
 *           during initialization.  This command only has effect prior
 *           to creating a specific HAL instance, and should be done only
 *           to a specific set of baseband parameters used to adjust the
 *           sensitivity of the baseband receiver.  The values are set
 *           at the factory, and are selected as a set by this parameter.
 *           This level is also controlled by the ANI algorithm, so that
 *           the initial immunity level will be modified during operation
 *           to select the optimal level for current conditions.  The default
 *           value for this parameter is 4, and should not be changed unless
 *           there is a specific reason.  The command has a corresponding
 *           get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 4
 *             .
 *         - iwpriv cmd: iwpriv wifiN OFDMTrgLow Low Threshold\n
 *           These commands control the OFDM PHY Error/sec threshold settings
 *           for the ANI immunity levels.  A PHY error rate below the low
 *           trigger will cause the ANI algorithm to lower immunity thresholds,
 *           and a PHY error rate exceeding the high threshold will cause
 *           immunity thresholds to be increased.  When a limit is exceed,
 *           the ANI algorithm will modify one of several baseband settings
 *           to either increase or decrease sensitivity.  Thresholds are
 *           increased/decreased in the following order:
 *           Increase:
 *           raise Noise Immunity level to MAX from 0, if Spur Immunity level is at MAX;
 *           raise Noise Immunity level to next level from non-zero value;
 *           raise Spur Immunity Level;
 *           (if using CCK rates)  raise CCK weak signal threshold + raise
 *           FIR Step level;
 *           Disable ANI PHY Err processing to reduce CPU load
 *           Decrease:
 *           OFDM weak signal detection on + increased Spur Immunity to 1;
 *           lower Noise Immunity level;
 *           lower FIR step level;
 *           lower CCK weak signal threshold;
 *           lower Spur Immunity level;
 *           OFDM weak signal detection on, with the existing Spur Immunity level 0
 *           The default values for these settings are 500 errors/sec for
 *           the high threshold, and 200 errors/sec for the low threshold.
 *           Each of these commands has a corresponding get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 200 errors/sec
 *             .
 *         - iwpriv cmd: iwpriv wifiN OFDMTrgHi High Threshold\n
 *           These commands control the OFDM PHY Error/sec threshold settings
 *           for the ANI immunity levels.  A PHY error rate below the low
 *           trigger will cause the ANI algorithm to lower immunity thresholds,
 *           and a PHY error rate exceeding the high threshold will cause
 *           immunity thresholds to be increased.  When a limit is exceed,
 *           the ANI algorithm will modify one of several baseband settings
 *           to either increase or decrease sensitivity.  Thresholds are
 *           increased/decreased in the following order:
 *           Increase:
 *           raise Noise Immunity level to MAX from 0, if Spur Immunity level is at MAX;
 *           raise Noise Immunity level to next level from non-zero value;
 *           raise Spur Immunity Level;
 *           (if using CCK rates)  raise CCK weak signal threshold + raise
 *           FIR Step level;
 *           Disable ANI PHY Err processing to reduce CPU load
 *           Decrease:
 *           OFDM weak signal detection on + increased Spur Immunity to 1;
 *           lower Noise Immunity level;
 *           lower FIR step level;
 *           lower CCK weak signal threshold;
 *           lower Spur Immunity level;
 *           OFDM weak signal detection on, with the existing Spur Immunity level 0
 *           The default values for these settings are 500 errors/sec for
 *           the high threshold, and 200 errors/sec for the low threshold.
 *           Each of these commands has a corresponding get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 500 errors/sec
 *             .
 *         - iwpriv cmd: iwpriv wifiN OFDMWeakDet 1|0\n
 *           This command will select normal (0) or weak (1) OFDM signal
 *           detection thresholds in the baseband register.  The actual
 *           thresholds are factory set, and are loaded in the EEPROM.
 *            This parameter corresponds to the initialization value for
 *           the ANI algorithm, and is only valid prior to system startup.
 *            The default value for this parameter is 1 (detect weak signals).
 *            The corresponding Get command will return the initialization
 *           value only.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 1
 *             .
 *         - iwpriv cmd: retrydur\n
 *           Cannot find in AP Manual
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: Not sure
 *             .
 *         - iwpriv cmd: iwpriv wifiN RSSIThrLow far threshold\n
 *           These settings are used to determine the relative distance
 *           of the AP from the station.  This is used to determine how
 *           the ANI immunity levels are selected.  If the average beacon
 *           RSSI of beacons from the AP is greater than RSSIThrHi, then
 *           the STA is determined to be at close-range. If the average
 *           beacon RSSI of beacons from the AP is less than RSSIThrHi,
 *           but greater than RSSIThrLow, then the STA is determined to
 *           be at mid-range. If the average beacon RSSI of beacons from
 *           the AP is less than RSSIThrLow, then the STA is determined
 *           to be at long-range.  The default values 40 for the high (near)
 *           threshold and 7 for the low (far) threshold.  This command
 *           has corresponding Get commands
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 7
 *             .
 *         - iwpriv cmd: iwpriv wifiN RSSIThrHi near threshold\n
 *           These settings are used to determine the relative distance
 *           of the AP from the station.  This is used to determine how
 *           the ANI immunity levels are selected.  If the average beacon
 *           RSSI of beacons from the AP is greater than RSSIThrHi, then
 *           the STA is determined to be at close-range. If the average
 *           beacon RSSI of beacons from the AP is less than RSSIThrHi,
 *           but greater than RSSIThrLow, then the STA is determined to
 *           be at mid-range. If the average beacon RSSI of beacons from
 *           the AP is less than RSSIThrLow, then the STA is determined
 *           to be at long-range.  The default values 40 for the high (near)
 *           threshold and 7 for the low (far) threshold.  This command
 *           has corresponding Get commands
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 40
 *             .
 *         - iwpriv cmd: iwpriv wifiN setCountryID Country ID Num\n
 *           These are used to set the AP to the regulatory requirements
 *           of the indicated country.  The SetCountryID command takes
 *           an integer value that represents the country, such as 840
 *           for US.  The setCountry command takes a string argument that
 *           includes the two character country string, plus I for
 *           indoor or O for outdoor.  The default value for country
 *           ID is 0 for debug, so that during initialization the country
 *           ID must be defined.  This is a requirement for the final system
 *           configuration. See Table 14 Country Code Definition
 *            for a full list of country IDs and strings.
 *           Each command has a corresponding get command.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: setVowExt\n
 *           Cannot find in AP Manual
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: Not sure
 *             .
 *         - iwpriv cmd: iwpriv wifiN SpurImmLvl level\n
 *           This command will set the Spur Immunity level, corresponding
 *           to the baseband parameter, cyc_pwr_thr1 that determines the
 *           minimum cyclic RSSI that can cause OFDM weak signal detection.
 *            Raising this level reduces the number of OFDM PHY Errs/s
 *           (caused due to board spurs, or interferers with OFDM symbol
 *           periodicity), lowering it allows detection of weaker OFDM
 *           signals (extending range).  As with most ANI related commands,
 *           this value is the initialization value, not the operating
 *           value.  The default value for this command is 2.  This command
 *           has corresponding get commands.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 2
 *             .
 *         - iwpriv cmd: iwpriv wifiN txchainmask mask\n
 *           These parameters set the transmit and receive chainmask values.
 *            For MIMO devices, the chainmask indicates how many streams
 *           are transmitted/received, and which chains are used.  For
 *           some Atheros devices up to 3 chains can be used, while others
 *           are restricted to 2 or 1 chain.  Its important to note that
 *           the maximum number of chains available for the device being
 *           used.  For dual chain devices, chain 2 is not available.
 *           Similarly, single chain devices will only support chain 0.
 *            The chains are represented in the bit mask as follows:
 *           Chain 0	0x01
 *           Chain 1	0x02
 *           Chain 2	0x04
 *           Selection of chainmask can affect several performance factors.
 *            For a 3 chain device, most of the time an RX chainmask of
 *           0x05 (or 0x03) is used for 2x2 stream reception.  For near
 *           range operations, a TX chainmask of 0x05 (or 0x03) is used
 *           to minimize near range effects.  For far range, a mask of
 *           0x07 is used for transmit.
 *           The default chainmask values are stored in EEPROM.  This iwpriv
 *           command will override the current chainmask settings.  These
 *           commands have corresponding get commands
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: stored in EEPROM
 *             .
 *         - iwpriv cmd: iwpriv wifiN rxchainmask mask\n
 *           These parameters set the transmit and receive chainmask values.
 *            For MIMO devices, the chainmask indicates how many streams
 *           are transmitted/received, and which chains are used.  For
 *           some Atheros devices up to 3 chains can be used, while others
 *           are restricted to 2 or 1 chain.  Its important to note that
 *           the maximum number of chains available for the device being
 *           used.  For dual chain devices, chain 2 is not available.
 *           Similarly, single chain devices will only support chain 0.
 *            The chains are represented in the bit mask as follows:
 *           Chain 0	0x01
 *           Chain 1	0x02
 *           Chain 2	0x04
 *           Selection of chainmask can affect several performance factors.
 *            For a 3 chain device, most of the time an RX chainmask of
 *           0x05 (or 0x03) is used for 2x2 stream reception.  For near
 *           range operations, a TX chainmask of 0x05 (or 0x03) is used
 *           to minimize near range effects.  For far range, a mask of
 *           0x07 is used for transmit.
 *           The default chainmask values are stored in EEPROM.  This iwpriv
 *           command will override the current chainmask settings.  These
 *           commands have corresponding get commands
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: stored in EEPROM
 *             .
 *         - iwpriv cmd: iwpriv athN TXPowLim limit\n
 *           This command will set the current Tx power limit.  This has
 *           the same effect as the iwconfig ath0 txpower command.  The
 *           Tx power will be set to the minimum of the value provided
 *           and the regulatory limit.  Note that this value may be updated
 *           by other portions of the code, so the effect of the value
 *           may be temporary.  The value is in units of 0.5 db increments.
 *            This command has a corresponding get command, and its default
 *           value is 0.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv athN TXPwrOvr overrideLimit\n
 *           This command is used to override the transmit power limits
 *           set by iwconfig or the TXPowLim command.  This is used for
 *           testing only, and is still limited by the regulatory power.
 *           get command, and its default value is 0.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? Not sure
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN dyntxchain 1|0\n
 *           This command is used to override the dynamic txChainmask
 *           fucntion on/off. Even this has been turned on, Tx beamforming
 *           and STBC will take proirity.
 *           get command, and its default value is 0.
 *             - iwpriv arguments:
 *             - iwpriv restart needed? No
 *             - iwpriv default value: 0
 *             .
 *         - iwpriv cmd: iwpriv wifiN interbss_allow 1|0\n
 *           This command is used to allow or disallow the traffic between two
 *           stations that are associated between two different VAPs.
 *           In case of multiple SSID mode, operator might chose not to allow
 *           the traffic between stations of different vaps to talk to each other.
 *           - iwpriv arguments
 *           - iwpriv restart needed ? No
 *           - iwpriv default value : 1
 *
 */
static int ol_ath_iw_setparam(struct net_device *dev,
                           struct iw_request_info *info,
                           void *w,
                           char *extra)
{

    struct ol_ath_softc_net80211 *scn =  ath_netdev_priv(dev);
    int *i = (int *) extra;
    int param = i[0];       /* parameter id is 1st */
    int value = i[1];       /* NB: most values are TYPE_INT */
    int retval = 0;
    bool restart_vaps = FALSE;

    /*
    ** Code Begins
    ** Since the parameter passed is the value of the parameter ID, we can call directly
    */
    if (ath_ioctl_debug)
        printk("***%s: dev=%s ioctl=0x%04x  name=%s\n", __func__, dev->name, param, find_ol_ath_ioctl_name(param, 1));

    if ( param & OL_ATH_PARAM_SHIFT )
    {
        /*
        ** It's an ATH value.  Call the  ATH configuration interface
        */

        param -= OL_ATH_PARAM_SHIFT;
        retval = ol_ath_set_config_param(scn, (ol_ath_param_t)param,
                                                   &value, &restart_vaps);
    }
    else if ( param & OL_SPECIAL_PARAM_SHIFT )
    {
        param -= OL_SPECIAL_PARAM_SHIFT;

        switch (param) {
        case OL_SPECIAL_PARAM_COUNTRY_ID:
            retval = wlan_set_countrycode(&scn->sc_ic, NULL, value, CLIST_NEW_COUNTRY);
            break;
        case OL_SPECIAL_PARAM_ASF_AMEM_PRINT:
            asf_amem_status_print();
            if ( value ) {
                asf_amem_allocs_print(asf_amem_alloc_all, value == 1);
            }
            break;
        case OL_SPECIAL_DBGLOG_REPORT_SIZE:
            dbglog_set_report_size(scn->wmi_handle, value);
            break;
        case OL_SPECIAL_DBGLOG_TSTAMP_RESOLUTION:
            dbglog_set_timestamp_resolution(scn->wmi_handle, value);
            break;
        case OL_SPECIAL_DBGLOG_REPORTING_ENABLED:
            dbglog_reporting_enable(scn->wmi_handle, value);
            break;
        case OL_SPECIAL_DBGLOG_LOG_LEVEL:
            dbglog_set_log_lvl(scn->wmi_handle, value);
            break;
        case OL_SPECIAL_DBGLOG_VAP_ENABLE:
            dbglog_vap_log_enable(scn->wmi_handle, value, TRUE);
            break;
        case OL_SPECIAL_DBGLOG_VAP_DISABLE:
            dbglog_vap_log_enable(scn->wmi_handle, value, FALSE);
            break;
        case OL_SPECIAL_DBGLOG_MODULE_ENABLE:
            dbglog_module_log_enable(scn->wmi_handle, value, TRUE);
			break;
        case OL_SPECIAL_DBGLOG_MODULE_DISABLE:
            dbglog_module_log_enable(scn->wmi_handle, value, FALSE);
            break;
        case OL_SPECIAL_PARAM_DISP_TPC:
            wmi_unified_pdev_get_tpc_config(scn->wmi_handle, value);
            break;
        case OL_SPECIAL_PARAM_ENABLE_CH_144:
            ol_regdmn_set_ch144(scn->ol_regdmn_handle, value);
            retval = wlan_set_countrycode(&scn->sc_ic, NULL, scn->ol_regdmn_handle->ol_regdmn_countryCode, CLIST_NEW_COUNTRY);
            break;
        case OL_SPECIAL_PARAM_ENABLE_CH144_EPPR_OVRD:
            ol_regdmn_set_ch144_eppovrd(scn->ol_regdmn_handle, value);
            retval = wlan_set_countrycode(&scn->sc_ic, NULL, scn->ol_regdmn_handle->ol_regdmn_countryCode, CLIST_NEW_COUNTRY);
            break;
        case OL_SPECIAL_PARAM_REGDOMAIN:
{
            struct ieee80211_channel *chan = ieee80211_get_current_channel(&scn->sc_ic);
            u_int16_t freq = ieee80211_chan2freq(&scn->sc_ic, chan);
            u_int32_t flags = chan->ic_flags;
            u_int8_t cfreq2 = chan->ic_vhtop_ch_freq_seg2;
            u_int32_t mode = ieee80211_chan2mode(chan);

            /*
             * After this command, the channel list would change.
             * must set ic_curchan properly.
             */
            retval = wlan_set_regdomain(&scn->sc_ic, value);

            chan = ieee80211_find_channel(&scn->sc_ic, freq, cfreq2, flags); /* cfreq2 arguement will be ignored for non VHT80+80 mode */
            if (chan == NULL) {
                printk("Current channel not supported in new RD. Configuring to a random channel\n");
                chan = ieee80211_find_dot11_channel(&scn->sc_ic, 0, 0, mode);
                if (chan == NULL) {
                    chan = ieee80211_find_dot11_channel(&scn->sc_ic, 0, 0, 0);
                    if(chan == NULL)
                        return -1;
                }
            }
            scn->sc_ic.ic_curchan = chan;
            scn->sc_ic.ic_set_channel(&scn->sc_ic);
}
            break;
        case OL_SPECIAL_PARAM_ENABLE_OL_STATS:
            if (scn->sc_ic.ic_ath_enable_ap_stats) {
                scn->sc_ic.ic_ath_enable_ap_stats(&scn->sc_ic, value);
                retval = 0;
            }
            break;
        case OL_SPECIAL_PARAM_ENABLE_MAC_REQ:
            printk("%s: mac req feature %d \n", __func__, value);
            scn->macreq_enabled = value;
            break;
        case OL_SPECIAL_PARAM_WLAN_PROFILE_ID_ENABLE:
            return wmi_unified_wlan_profile_cmd(scn->wmi_handle,
                                 WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID, value, 1);
        case OL_SPECIAL_PARAM_WLAN_PROFILE_TRIGGER:
            return wmi_unified_wlan_profile_cmd(scn->wmi_handle,
                                 WMI_WLAN_PROFILE_TRIGGER_CMDID, value, 0);
#if ATH_DATA_TX_INFO_EN
        case OL_SPECIAL_PARAM_ENABLE_PERPKT_TXSTATS:
            if(value == 1){
                scn->enable_perpkt_txstats = 1;
            }else{
                scn->enable_perpkt_txstats = 0;
            }

#ifdef QCA_NSS_WIFI_OFFLOAD_SUPPORT
            osif_nss_ol_set_perpkt_txstats(scn);
#endif
            break;
#endif
        default:
            retval = -EOPNOTSUPP;
            break;
        }
    }
    else
    {
        retval = (int) ol_hal_set_config_param(scn, (ol_hal_param_t)param, &value);
    }

    if (restart_vaps == TRUE) {
        retval = osif_restart_vaps(&scn->sc_ic);
    }

    return (retval);
}

/******************************************************************************/
/*!
**  \brief Returns value of HAL parameter
**
**  This returns the current value of the indicated HAL parameter, used by
**  iwpriv interface.
**
**  \param dev Net Device pointer, Linux driver handle
**  \param info Request Info Stucture containing type info, etc.
**  \param w Pointer to a word value, not used here
**  \param extra Pointer to the actual data buffer.
**  \return 0 for success
**  \return -EOPNOTSUPP for invalue request
*/

static int ol_ath_iw_getparam(struct net_device *dev, struct iw_request_info *info, void *w, char *extra)
{
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    struct ieee80211com *ic;
    int param   = *(int *)extra;
    int *val   = (int *)extra;
    int retval   = 0;

    /*
    ** Code Begins
    ** Since the parameter passed is the value of the parameter ID, we can call directly
    */
    ic = &scn->sc_ic;

    if ( param & OL_ATH_PARAM_SHIFT )
    {
        /*
        ** It's an ATH value.  Call the  ATH configuration interface
        */

        param -= OL_ATH_PARAM_SHIFT;
        if (ol_ath_get_config_param(scn, (ol_ath_param_t)param, (void *)extra))
        {
            retval = -EOPNOTSUPP;
        }
    }
    else if ( param & OL_SPECIAL_PARAM_SHIFT )
    {
        if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_COUNTRY_ID) ) {
            IEEE80211_COUNTRY_ENTRY    cval;

            ic->ic_get_currentCountry(ic, &cval);
            val[0] = cval.countryCode;
        } else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_ENABLE_CH_144) ) {
            ol_regdmn_get_ch144(scn->ol_regdmn_handle, &val[0]);
        } else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_REGDOMAIN) ) {
            val[0] = 0;
            ol_regdmn_get_regdomain(scn->ol_regdmn_handle, (REGDMN_REG_DOMAIN *)&val[0]);
		} else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_ENABLE_OL_STATS) ) {
			*val = scn->scn_stats.ap_stats_tx_cal_enable;
		}
#if ATH_DATA_TX_INFO_EN
        else if ( param == (OL_SPECIAL_PARAM_SHIFT | OL_SPECIAL_PARAM_ENABLE_PERPKT_TXSTATS) ) {
			*val = scn->enable_perpkt_txstats;
        }
#endif
        else {
            retval = -EOPNOTSUPP;
        }
    }
    else
    {
        if ( ol_hal_get_config_param(scn, (ol_hal_param_t)param, (void *)extra))
        {
            retval = -EOPNOTSUPP;
        }
    }

    return (retval);
}

/******************************************************************************/
/*!
**  \brief Set country
**
**  Interface routine called by the iwpriv handler to directly set specific
**  HAL parameters.  Parameter ID is stored in the above iw_priv_args structure
**
**  \param dev Net Device pointer, Linux driver handle
**  \param info Request Info Stucture containing type info, etc.
**  \param w Pointer to a word value, not used here
**  \param extra Pointer to the actual data buffer.
**  \return 0 for success
**  \return Non Zero on error
*/

static int ol_ath_iw_setcountry(struct net_device *dev,
                           struct iw_request_info *info,
                           void *w,
                           char *extra)
{

    struct ol_ath_softc_net80211 *scn =  ath_netdev_priv(dev);

    char *p = (char *) extra;
    int retval;

    printk("[%s][%d] *p=%x, *(p+1)=%x\n", __func__, __LINE__, *p, *(p+1));

    if (&scn->sc_ic) {
        retval=  wlan_set_countrycode(&scn->sc_ic, p, 0, CLIST_NEW_COUNTRY);
    } else {
        retval = -EOPNOTSUPP;
    }

    return retval;
}

/******************************************************************************/
/*!
**  \brief Returns country string
**
**  This returns the current value of the indicated HAL parameter, used by
**  iwpriv interface.
**
**  \param dev Net Device pointer, Linux driver handle
**  \param info Request Info Stucture containing type info, etc.
**  \param w Pointer to a word value, not used here
**  \param extra Pointer to the actual data buffer.
**  \return 0 for success
*/

static int ol_ath_iw_getcountry(struct net_device *dev, struct iw_request_info *info, void *w, char *extra)
{
    struct ol_ath_softc_net80211 *scn =  ath_netdev_priv(dev);
    struct iw_point           *wri  = (struct iw_point *)w;
    char                      *str  = (char *)extra;
    int retval = 0;
    struct ieee80211com *ic = &scn->sc_ic;
    IEEE80211_COUNTRY_ENTRY    cval;

    ic->ic_get_currentCountry(ic, &cval);

    /* In case of setting regdomain only, country is CTRY_DEFAULT */
    if (cval.countryCode == 0) {
        str[0] = cval.iso[0];
        str[1] = cval.iso[1];
        str[2] = cval.iso[2];
        str[3] = 0;
    } else
        retval= wlan_get_countrycode(&scn->sc_ic, str);

    wri->length = 3;

    return retval;
}


static int char_to_num(char c)
{
    if ( c >= 'A' && c <= 'F' )
        return c - 'A' + 10;
    if ( c >= 'a' && c <= 'f' )
        return c - 'a' + 10;
    if ( c >= '0' && c <= '9' )
        return c - '0';
    return -EINVAL;
}

static char num_to_char(int n)
{
    if ( n >= 10 && n <= 15 )
        return n  - 10 + 'A';
    if ( n >= 0 && n <= 9 )
        return n  + '0';
    return ' '; //Blank space
}

/******************************************************************************/
/*!
**  \brief Set MAC address
**
**  Interface routine called by the iwpriv handler to directly set MAC
**  address.
**
**  \param dev Net Device pointer, Linux driver handle
**  \param info Request Info Stucture containing type info, etc.
**  \param w Pointer to a word value, not used here
**  \param extra Pointer to the actual data buffer.
**  \return 0 for success
**  \return Non Zero on error
*/

static int ol_ath_iw_sethwaddr(struct net_device *dev, struct iw_request_info *info, void *w, char *extra)
{
    int retval;
    unsigned char addr[IEEE80211_ADDR_LEN];
    struct sockaddr sa;
    int i = 0;
    char *buf = extra;
    struct ol_ath_softc_net80211 *scn = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;

    do {
        retval = char_to_num(*buf++);
        if ( retval < 0 )
            break;

        addr[i] = retval << 4;

        retval = char_to_num(*buf++);
        if ( retval < 0 )
            break;

        addr[i++] += retval;

        if ( i < IEEE80211_ADDR_LEN && *buf++ != ':' ) {
            retval = -EINVAL;
            break;
        }
        retval = 0;
    } while ( i < IEEE80211_ADDR_LEN );

    if ( !retval ) {
        if ( !TAILQ_EMPTY(&ic->ic_vaps) ) {
            retval = -EBUSY; //We do not set the MAC address if there are VAPs present
        } else {
            IEEE80211_ADDR_COPY(&sa.sa_data, addr);
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,30)
            retval = dev->netdev_ops->ndo_set_mac_address(dev, &sa);
#else
            retval = dev->set_mac_address(dev, &sa);
#endif
        }
    }

    return retval;
}

/******************************************************************************/
/*!
**  \brief Returns MAC address
**
**  This returns the current value of the MAC address, used by
**  iwpriv interface.
**
**  \param dev Net Device pointer, Linux driver handle
**  \param info Request Info Stucture containing type info, etc.
**  \param w Pointer to a word value, not used here
**  \param extra Pointer to the actual data buffer.
**  \return 0 for success
*/

static int ol_ath_iw_gethwaddr(struct net_device *dev, struct iw_request_info *info, void *w, char *extra)
{
#if 0 /* TBD */
    struct iw_point *wri = (struct iw_point *)w;
    char *buf = extra;
    int i, j = 0;

    for ( i = 0; i < IEEE80211_ADDR_LEN; i++ ) {
        buf[j++] = num_to_char(dev->dev_addr[i] >> 4);
        buf[j++] = num_to_char(dev->dev_addr[i] & 0xF);

        if ( i < IEEE80211_ADDR_LEN - 1 )
            buf[j++] = ':';
        else
            buf[j++] = '\0';
    }

    wri->length = (IEEE80211_ADDR_LEN * 3) - 1;

    return 0;
#else
    return -EOPNOTSUPP;
#endif
}


static int ol_ath_iw_gpioconfig(struct net_device *dev,
                                 struct iw_request_info *info,
                                 void *w,
                                 char *extra)
{
    struct ol_ath_softc_net80211 *scn =  ath_netdev_priv(dev);
    int *param = (int *) extra;

    wmi_unified_gpio_config(scn->wmi_handle, param[0], param[1], param[2], param[3]);
    return 0;
}

static int ol_ath_iw_gpiooutput(struct net_device *dev,
                                 struct iw_request_info *info,
                                 void *w,
                                 char *extra)
{
    struct ol_ath_softc_net80211 *scn =  ath_netdev_priv(dev);
    int *param = (int *) extra;

    wmi_unified_gpio_output(scn->wmi_handle, param[0], param[1]);
    return 0;
}

#if UNIFIED_SMARTANTENNA
static int ol_ath_iw_set_saparam(struct net_device *dev,
        struct iw_request_info *info,
        void *w,
        char *extra)
{
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;
    int status;
    status = ieee80211_smart_ant_set_param(ic, extra);
    return status;
}

static int ol_ath_iw_get_saparam(struct net_device *dev, struct iw_request_info *info, void *w, char *extra)
{
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);
    struct ieee80211com *ic = &scn->sc_ic;

    uint32_t value = 0;
    value = ieee80211_smart_ant_get_param(ic, extra);
    return value;
}
#endif

static int
null_ioctl_func(struct net_device *dev,
                         struct iw_request_info *info,
                         void *w, char *extra)
{
    printk(" This ioctl is not implemented\n");

    return -1;
}
#if PEER_FLOW_CONTROL
extern void ol_txrx_per_peer_stats(struct ol_txrx_pdev_t *pdev, char *addr);
static int ol_ath_iw_get_peer_info(struct net_device *dev,
                           struct iw_request_info *info,
                           void *w,
                           char *extra)
{
    int retval;
    unsigned char addr[IEEE80211_ADDR_LEN];
    int i = 0;
    char *buf = extra;
    struct ol_ath_softc_net80211 *scn  = ath_netdev_priv(dev);

    do {
        retval = char_to_num(*buf++);
        if ( retval < 0 )
            break;

        addr[i] = retval << 4;
        retval = char_to_num(*buf++);
        if ( retval < 0 )
            break;

        addr[i++] += retval;

        if ( i < IEEE80211_ADDR_LEN && *buf++ != ':' ) {
            retval = -EINVAL;
            break;
        }
        retval = 0;
    } while ( i < IEEE80211_ADDR_LEN );

    if ( !retval ) {

        ol_txrx_per_peer_stats(scn->pdev_txrx_handle, addr);

    }

    return retval;
}
#endif
/*
** iwpriv Handlers
** This table contains the references to the routines that actually get/set
** the parameters in the args table.
*/

static const iw_handler ol_ath_iw_priv_handlers[] = {
    (iw_handler) ol_ath_iw_setparam,          /* SIOCWFIRSTPRIV+0 */
    (iw_handler) ol_ath_iw_getparam,          /* SIOCWFIRSTPRIV+1 */
    (iw_handler) ol_ath_iw_setcountry,        /* SIOCWFIRSTPRIV+2 */
    (iw_handler) ol_ath_iw_getcountry,        /* SIOCWFIRSTPRIV+3 */
    (iw_handler) ol_ath_iw_sethwaddr,         /* SIOCWFIRSTPRIV+4 */
    (iw_handler) ol_ath_iw_gethwaddr,         /* SIOCWFIRSTPRIV+5 */
    (iw_handler) ol_ath_iw_gpioconfig,        /* SIOCWFIRSTPRIV+6 */
    (iw_handler) ol_ath_iw_gpiooutput,        /* SIOCWFIRSTPRIV+7 */
/* Adding null ioctl replacing dscp tid map get and set ioctl function. 
 * Can be reused in future.
 */
    (iw_handler) null_ioctl_func,
    (iw_handler) null_ioctl_func,
#if ATH_SUPPORT_HYFI_ENHANCEMENTS
	(iw_handler) ieee80211_ioctl_ald_getStatistics,   /* SIOCWFIRSTPRIV+10 */
#else
	(iw_handler) null_ioctl_func,
#endif
#if UNIFIED_SMARTANTENNA
    (iw_handler) ol_ath_iw_set_saparam,        /* SIOCWFIRSTPRIV+11 */
    (iw_handler) ol_ath_iw_get_saparam,        /* SIOCWFIRSTPRIV+12 */
#else
	(iw_handler) null_ioctl_func,
	(iw_handler) null_ioctl_func,
#endif
     (iw_handler) ol_ath_iw_get_aggr_burst,    /* SIOCWFIRSTPRIV+13 */
     (iw_handler) ol_ath_iw_set_aggr_burst,   /* SIOCWFIRSTPRIV+14 */
#if QCA_AIRTIME_FAIRNESS
     (iw_handler) ol_ath_iw_set_atf_sched_dur,         /* SIOCWFIRSTPRIV+15 */
#endif

#if PEER_FLOW_CONTROL
     (iw_handler) ol_ath_iw_get_peer_info,         /* SIOCWFIRSTPRIV+16 */
#endif
};

/*
** Wireless Handler Structure
** This table provides the Wireless tools the interface required to access
** parameters in the HAL.  Each sub table contains the definition of various
** control tables that reference functions this module
*/
static struct iw_handler_def ol_ath_iw_handler_def = {
    .standard           = (iw_handler *) NULL,
    .num_standard       = 0,
    .private            = (iw_handler *) ol_ath_iw_priv_handlers,
    .num_private        = TABLE_SIZE(ol_ath_iw_priv_handlers),
    .private_args       = (struct iw_priv_args *) ol_ath_iw_priv_args,
    .num_private_args   = TABLE_SIZE(ol_ath_iw_priv_args),
    .get_wireless_stats	= NULL,
};

void ol_ath_iw_attach(struct net_device *dev)
{
    dev->wireless_handlers = &ol_ath_iw_handler_def;
}
EXPORT_SYMBOL(ol_ath_iw_attach);

/*
 *
 * Search the ath_iw_priv_arg table for ATH_HAL_IOCTL_SETPARAM defines to
 * display the param name
 */
static char *find_ol_ath_ioctl_name(int param, int set_flag)
{
    struct iw_priv_args *pa = (struct iw_priv_args *) ol_ath_iw_priv_args;
    int num_args  = ol_ath_iw_handler_def.num_private_args;
    int i;

    /* now look for the sub-ioctl number */

    for (i = 0; i < num_args; i++, pa++) {
        if (pa->cmd == param) {
            if (set_flag) {
                if (pa->set_args)
                    return(pa->name ? pa->name : "UNNAMED");
            } else if (pa->get_args)
                    return(pa->name ? pa->name : "UNNAMED");
        }
    }
    return("Unknown IOCTL");
}
